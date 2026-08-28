// -----------------------------------------------------------------------------
// FICHIER: Gemcrush/Ui/NkGemAudio.cpp
// DESCRIPTION: Synthèse des bruitages et de la boucle d'ambiance.
//
//              TOUT EST CALCULÉ, RIEN N'EST LU. Chaque son est une somme
//              d'oscillateurs sous enveloppe. La recette de chacun est écrite
//              en toutes lettres dans BuildSample() : c'est ce qui permet de
//              le retoucher sans rouvrir un éditeur audio.
//
//              GAMME : tous les sons tonals se placent sur une PENTATONIQUE
//              (do ré mi sol la). C'est une contrainte, pas une décoration —
//              deux sons pris au hasard sur cette gamme ne peuvent pas sonner
//              faux ensemble, et un match-3 en superpose beaucoup.
//
// AUTEUR: Rihen
// DATE: 2026-08-27
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "Gemcrush/Ui/NkGemAudio.h"
#include "NKMemory/NkAllocator.h"
#include "NKMath/NKMath.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace game {
		namespace ui {

			using audio::AudioEngine;
			using audio::AudioLoader;
			using audio::AudioSample;
			using audio::VoiceParams;

			namespace {

				constexpr int32 kSampleRate = 48000;
				constexpr float32 kTau = 6.28318530718f;

				/// Pentatonique majeure de do, sur deux octaves (Hz).
				const float32 kScale[10] = {261.63f, 293.66f, 329.63f, 392.00f, 440.00f,
											523.25f, 587.33f, 659.25f, 783.99f, 880.00f};

				/// Bruit blanc déterministe (générateur congruentiel) : reproductible
				/// d'une exécution à l'autre, donc un son ne change jamais tout seul.
				struct Noise {
						uint32 state = 0x13579BDFu;

						float32 Next() noexcept {
							state = state * 1664525u + 1013904223u;
							return (static_cast<float32>((state >> 8) & 0xFFFFu) / 32768.f) - 1.f;
						}
				};

				/// Enveloppe percussive : attaque très courte, décroissance
				/// exponentielle. C'est la forme de tout ce qui est « frappé ».
				float32 Envelope(float32 t01, float32 attack01, float32 decayShape) noexcept {
					if (t01 < attack01) {
						return (attack01 > 0.f) ? (t01 / attack01) : 1.f;
					}
					const float32 x = (t01 - attack01) / math::NkMax(0.0001f, 1.f - attack01);
					return math::NkExp(-decayShape * x);
				}

				/// Alloue les données PCM d'un échantillon mono.
				/// ⚠️ La libération passe par NkGemAudio::FreeSample, JAMAIS par
				/// AudioLoader::Free : ce n'est pas le même allocateur, et mélanger
				/// les deux corrompt le tas (défaut connu du dépôt).
				bool AllocateMono(AudioSample &sample, usize frameCount) {
					const usize bytes = frameCount * sizeof(float32);
					void *raw = memory::NkAlloc(bytes);
					if (raw == nullptr) {
						return false;
					}
					sample.data = static_cast<float32 *>(raw);
					sample.frameCount = frameCount;
					sample.sampleRate = kSampleRate;
					sample.channels = 1;
					sample.mAllocator = &memory::NkGetDefaultAllocator();
					for (usize i = 0; i < frameCount; ++i) {
						sample.data[i] = 0.f;
					}
					return true;
				}

				/// Limiteur doux : évite l'écrêtage dur quand plusieurs partiels
				/// s'additionnent. Sans lui, un accord de trois harmoniques sature
				/// et le son devient râpeux — l'artefact ressemble à un bogue de
				/// carte son alors qu'il vient de la synthèse.
				float32 SoftClip(float32 x) noexcept {
					if (x > 1.f) {
						return 1.f - (1.f / (1.f + (x - 1.f) * 4.f)) * 0.999f;
					}
					if (x < -1.f) {
						return -SoftClip(-x);
					}
					return x;
				}

			} // namespace

			// =========================================================
			// Cycle de vie
			// =========================================================
			NkGemAudio::~NkGemAudio() {
				Shutdown();
			}

			bool NkGemAudio::Init() {
				if (mReady) {
					return true;
				}
				if (!AudioEngine::Instance().Initialize()) {
					// Le jeu CONTINUE sans son. Refuser de démarrer parce qu'une
					// carte son manque transformerait un confort en dépendance.
					logger.Warn("[gemcrush/audio] moteur audio indisponible — le jeu tourne sans son");
					mReady = false;
					return false;
				}
				for (usize i = 0; i < static_cast<usize>(NkGemSfx::Count); ++i) {
					BuildSample(static_cast<NkGemSfx>(i));
				}
				BuildMusic();
				mReady = true;
				logger.Info("[gemcrush/audio] {0} bruitages synthetises + boucle d'ambiance",
							static_cast<int32>(NkGemSfx::Count));
				return true;
			}

			void NkGemAudio::Shutdown() {
				if (!mReady) {
					return;
				}
				StopMusic();
				for (usize i = 0; i < static_cast<usize>(NkGemSfx::Count); ++i) {
					FreeSample(mSfxSamples[i], false);
				}
				FreeSample(mMusicSample, mMusicFromFile);
				AudioEngine::Instance().Shutdown();
				mReady = false;
			}

			void NkGemAudio::FreeSample(AudioSample &sample, bool fromLoader) {
				if (sample.data == nullptr) {
					return;
				}
				if (fromLoader) {
					AudioLoader::Free(sample);
				} else {
					memory::NkFree(sample.data);
					sample.data = nullptr;
					sample.frameCount = 0;
				}
			}

			// =========================================================
			// BuildSample — la recette de chaque bruitage
			// =========================================================
			void NkGemAudio::BuildSample(NkGemSfx sfx) {
				AudioSample &sample = mSfxSamples[static_cast<usize>(sfx)];
				Noise noise;

				switch (sfx) {
					// ÉCHANGE : souffle court qui MONTE — un mouvement, pas un choc.
					case NkGemSfx::Swap: {
						const usize frames = static_cast<usize>(0.10f * kSampleRate);
						if (!AllocateMono(sample, frames)) {
							return;
						}
						float32 phase = 0.f;
						for (usize i = 0; i < frames; ++i) {
							const float32 t = static_cast<float32>(i) / static_cast<float32>(frames);
							const float32 freq = 420.f + 380.f * t;
							phase += kTau * freq / static_cast<float32>(kSampleRate);
							const float32 env = Envelope(t, 0.10f, 5.f);
							sample.data[i] = SoftClip((math::NkSin(phase) * 0.5f + noise.Next() * 0.10f) * env * 0.6f);
						}
						break;
					}
					// REFUS : deux blips graves. Bas et bref = « non », sans agresser.
					case NkGemSfx::Invalid: {
						const usize frames = static_cast<usize>(0.18f * kSampleRate);
						if (!AllocateMono(sample, frames)) {
							return;
						}
						for (usize i = 0; i < frames; ++i) {
							const float32 t = static_cast<float32>(i) / static_cast<float32>(frames);
							// Deux impulsions séparées : 0.0-0.35 puis 0.45-0.80.
							float32 local = -1.f;
							if (t < 0.35f) {
								local = t / 0.35f;
							} else if (t > 0.45f && t < 0.80f) {
								local = (t - 0.45f) / 0.35f;
							}
							if (local < 0.f) {
								continue;
							}
							const float32 phase = kTau * 196.f * (static_cast<float32>(i) / static_cast<float32>(kSampleRate));
							sample.data[i] = SoftClip(math::NkSin(phase) * Envelope(local, 0.05f, 6.f) * 0.45f);
						}
						break;
					}
					// ALIGNEMENT : cloche à trois harmoniques. La hauteur est posée
					// au moment de jouer (pitch), pas ici : un seul échantillon sert
					// les huit rangs de cascade.
					case NkGemSfx::Match: {
						const usize frames = static_cast<usize>(0.42f * kSampleRate);
						if (!AllocateMono(sample, frames)) {
							return;
						}
						const float32 base = kScale[0];
						for (usize i = 0; i < frames; ++i) {
							const float32 t = static_cast<float32>(i) / static_cast<float32>(frames);
							const float32 s = static_cast<float32>(i) / static_cast<float32>(kSampleRate);
							// Partiels inharmoniques (x2.01, x3.03) : c'est ce léger
							// décalage qui fait « cloche » plutôt que « orgue ».
							const float32 v = math::NkSin(kTau * base * s) * 0.55f +
											  math::NkSin(kTau * base * 2.01f * s) * 0.28f +
											  math::NkSin(kTau * base * 3.03f * s) * 0.14f;
							sample.data[i] = SoftClip(v * Envelope(t, 0.01f, 4.5f) * 0.75f);
						}
						break;
					}
					// SPÉCIALE : montée rapide + paillettes. Doit se distinguer d'un
					// alignement ordinaire à la première écoute.
					case NkGemSfx::Special: {
						const usize frames = static_cast<usize>(0.55f * kSampleRate);
						if (!AllocateMono(sample, frames)) {
							return;
						}
						float32 phase = 0.f;
						for (usize i = 0; i < frames; ++i) {
							const float32 t = static_cast<float32>(i) / static_cast<float32>(frames);
							const float32 freq = 330.f + 900.f * t * t;
							phase += kTau * freq / static_cast<float32>(kSampleRate);
							const float32 sparkle =
								math::NkSin(kTau * 2400.f * (static_cast<float32>(i) / static_cast<float32>(kSampleRate))) *
								0.18f * Envelope(t, 0.02f, 9.f);
							sample.data[i] = SoftClip((math::NkSin(phase) * 0.5f + sparkle) * Envelope(t, 0.02f, 3.2f));
						}
						break;
					}
					// ÉTOILE : arpège ascendant de trois notes de la gamme.
					case NkGemSfx::Star: {
						const usize frames = static_cast<usize>(0.50f * kSampleRate);
						if (!AllocateMono(sample, frames)) {
							return;
						}
						const float32 notes[3] = {kScale[5], kScale[7], kScale[9]};
						for (usize i = 0; i < frames; ++i) {
							const float32 t = static_cast<float32>(i) / static_cast<float32>(frames);
							const int32 step = math::NkMin(2, static_cast<int32>(t * 3.f));
							const float32 local = t * 3.f - static_cast<float32>(step);
							const float32 s = static_cast<float32>(i) / static_cast<float32>(kSampleRate);
							const float32 v = math::NkSin(kTau * notes[step] * s) * 0.5f +
											  math::NkSin(kTau * notes[step] * 2.f * s) * 0.15f;
							sample.data[i] = SoftClip(v * Envelope(local, 0.02f, 5.f) * 0.7f);
						}
						break;
					}
					// BOUTON : clic très court. Au-delà de ~50 ms, un clic d'interface
					// commence à se faire remarquer, et il est joué très souvent.
					case NkGemSfx::Button: {
						const usize frames = static_cast<usize>(0.045f * kSampleRate);
						if (!AllocateMono(sample, frames)) {
							return;
						}
						for (usize i = 0; i < frames; ++i) {
							const float32 t = static_cast<float32>(i) / static_cast<float32>(frames);
							const float32 s = static_cast<float32>(i) / static_cast<float32>(kSampleRate);
							sample.data[i] = SoftClip(
								(math::NkSin(kTau * 880.f * s) * 0.6f + noise.Next() * 0.25f) * Envelope(t, 0.02f, 9.f) * 0.5f);
						}
						break;
					}
					// ÉCHEC : descente. L'inverse exact du son de victoire.
					case NkGemSfx::Fail: {
						const usize frames = static_cast<usize>(0.75f * kSampleRate);
						if (!AllocateMono(sample, frames)) {
							return;
						}
						float32 phase = 0.f;
						for (usize i = 0; i < frames; ++i) {
							const float32 t = static_cast<float32>(i) / static_cast<float32>(frames);
							const float32 freq = 392.f - 200.f * t;
							phase += kTau * freq / static_cast<float32>(kSampleRate);
							sample.data[i] =
								SoftClip((math::NkSin(phase) * 0.5f + math::NkSin(phase * 0.5f) * 0.25f) *
										 Envelope(t, 0.05f, 2.6f) * 0.7f);
						}
						break;
					}
					// VICTOIRE : quatre notes montantes, la dernière tenue.
					case NkGemSfx::Win: {
						const usize frames = static_cast<usize>(1.10f * kSampleRate);
						if (!AllocateMono(sample, frames)) {
							return;
						}
						const float32 notes[4] = {kScale[0], kScale[2], kScale[3], kScale[5]};
						for (usize i = 0; i < frames; ++i) {
							const float32 t = static_cast<float32>(i) / static_cast<float32>(frames);
							const int32 step = math::NkMin(3, static_cast<int32>(t * 4.5f));
							const float32 local = math::NkMin(1.f, t * 4.5f - static_cast<float32>(step));
							const float32 s = static_cast<float32>(i) / static_cast<float32>(kSampleRate);
							const float32 v = math::NkSin(kTau * notes[step] * s) * 0.5f +
											  math::NkSin(kTau * notes[step] * 2.f * s) * 0.2f +
											  math::NkSin(kTau * notes[step] * 3.f * s) * 0.08f;
							// La dernière note décroît lentement : c'est elle qui donne
							// la sensation de conclusion.
							const float32 decay = (step == 3) ? 1.8f : 4.5f;
							sample.data[i] = SoftClip(v * Envelope(local, 0.02f, decay) * 0.72f);
						}
						break;
					}
					// TIC : dernières secondes du chronomètre. Sec, aigu, court.
					case NkGemSfx::Tick: {
						const usize frames = static_cast<usize>(0.06f * kSampleRate);
						if (!AllocateMono(sample, frames)) {
							return;
						}
						for (usize i = 0; i < frames; ++i) {
							const float32 t = static_cast<float32>(i) / static_cast<float32>(frames);
							const float32 s = static_cast<float32>(i) / static_cast<float32>(kSampleRate);
							sample.data[i] = SoftClip(math::NkSin(kTau * 1320.f * s) * Envelope(t, 0.01f, 11.f) * 0.55f);
						}
						break;
					}
					default:
						break;
				}
			}

			// =========================================================
			// BuildMusic — boucle d'ambiance générée
			//
			// 12 secondes, bouclables sans couture : la durée est un multiple
			// exact de la mesure, et la dernière note s'éteint avant la fin.
			// Un fondu au raccord masquerait une boucle mal calée ; ici il n'y
			// en a pas besoin, et c'est vérifiable à l'oreille.
			// =========================================================
			void NkGemAudio::BuildMusic() {
				const float32 duration = 12.f;
				const usize frames = static_cast<usize>(duration * kSampleRate);
				if (!AllocateMono(mMusicSample, frames)) {
					return;
				}
				mMusicFromFile = false;

				// Arpège de 16 pas sur la pentatonique, plus une nappe grave.
				const int32 pattern[16] = {0, 2, 3, 5, 3, 2, 0, 2, 5, 7, 5, 3, 2, 0, 2, 3};
				const float32 stepDuration = duration / 16.f;

				for (usize i = 0; i < frames; ++i) {
					const float32 s = static_cast<float32>(i) / static_cast<float32>(kSampleRate);
					const int32 step = math::NkMin(15, static_cast<int32>(s / stepDuration));
					const float32 local = (s - static_cast<float32>(step) * stepDuration) / stepDuration;

					// Voix mélodique : triangle adouci, enveloppe courte.
					const float32 note = kScale[pattern[step]];
					const float32 melody =
						(math::NkSin(kTau * note * s) * 0.55f + math::NkSin(kTau * note * 2.f * s) * 0.12f) *
						Envelope(local, 0.06f, 3.4f);

					// Nappe : deux basses très lentes, une quinte d'écart. Elles
					// donnent le fond sans jamais attirer l'attention.
					const float32 pad = (math::NkSin(kTau * 65.41f * s) * 0.20f + math::NkSin(kTau * 98.f * s) * 0.13f) *
										(0.55f + 0.45f * math::NkSin(kTau * s / duration));

					mMusicSample.data[i] = SoftClip((melody * 0.55f + pad) * 0.55f);
				}
			}

			// =========================================================
			// Lecture
			// =========================================================
			// =========================================================
			// Suspension : arrière-plan et perte de focus
			//
			// ⚠️ Un jeu qui continue de jouer sa musique une fois en
			// arrière-plan est un défaut que l'utilisateur subit sans pouvoir
			// le corriger — il doit tuer l'application. Signalé sur Android le
			// 2026-08-28 ; le même cycle de vie existe sur iOS et HarmonyOS.
			// =========================================================
			void NkGemAudio::Suspend() {
				if (!mReady || mSuspended) {
					return;
				}
				mSuspended = true;
				AudioEngine::Instance().PauseAll();
			}

			void NkGemAudio::Resume() {
				if (!mReady || !mSuspended) {
					return;
				}
				mSuspended = false;
				AudioEngine::Instance().ResumeAll();
			}

			void NkGemAudio::PlaySfx(NkGemSfx sfx, float32 pitch) {
				if (!mReady || mMuted || mSuspended) {
					return; // en arrière-plan, aucun son ne part
				}
				const usize index = static_cast<usize>(sfx);
				if (index >= static_cast<usize>(NkGemSfx::Count) || !mSfxSamples[index].IsValid()) {
					return;
				}
				VoiceParams params;
				params.volume = mSfxVolume;
				params.pitch = math::NkClamp(pitch, 0.25f, 4.f);
				params.bus = "SFX";
				AudioEngine::Instance().Play(mSfxSamples[index], params);
			}

			void NkGemAudio::PlayMatch(int32 cascadeRank) {
				// Un demi-ton par rang (2^(1/12) ≈ 1.05946), plafonné à 8 rangs.
				// Au-delà, la cloche devient stridente et la récompense se
				// retourne en agression.
				const int32 rank = math::NkClamp(cascadeRank, 1, 8);
				float32 pitch = 1.f;
				for (int32 i = 1; i < rank; ++i) {
					pitch *= 1.05946f;
				}
				PlaySfx(NkGemSfx::Match, pitch);
			}

			void NkGemAudio::StartMusic() {
				if (!mReady || mMusicPlaying || !mMusicSample.IsValid()) {
					return;
				}
				VoiceParams params;
				params.volume = mMuted ? 0.f : mMusicVolume;
				params.looping = true;
				params.fadeInTime = 1.2f; // entrée en douceur : une musique qui démarre net surprend
				params.bus = "Music";
				mMusicHandle = AudioEngine::Instance().Play(mMusicSample, params);
				mMusicPlaying = true;
			}

			void NkGemAudio::StopMusic() {
				if (!mReady || !mMusicPlaying) {
					return;
				}
				AudioEngine::Instance().Stop(mMusicHandle);
				mMusicPlaying = false;
			}

			bool NkGemAudio::LoadMusicFile(const char *path) {
				if (!mReady || path == nullptr) {
					return false;
				}
				AudioSample loaded = AudioLoader::Load(path);
				if (!loaded.IsValid()) {
					// Le repli CHANGE le verdict : on rend false et on garde la
					// boucle générée. Un `true` ici ferait croire qu'un morceau
					// joue alors que c'est toujours la synthèse.
					logger.Warn("[gemcrush/audio] musique introuvable ou illisible : {0}", path);
					return false;
				}
				const bool wasPlaying = mMusicPlaying;
				StopMusic();
				FreeSample(mMusicSample, mMusicFromFile);
				mMusicSample = loaded;
				mMusicFromFile = true;
				if (wasPlaying) {
					StartMusic();
				}
				return true;
			}

			// =========================================================
			// Réglages
			// =========================================================
			void NkGemAudio::SetMusicVolume(float32 volume01) {
				mMusicVolume = math::NkClamp(volume01, 0.f, 1.f);
				if (mReady && mMusicPlaying) {
					AudioEngine::Instance().SetVolume(mMusicHandle, mMuted ? 0.f : mMusicVolume);
				}
			}

			void NkGemAudio::SetSfxVolume(float32 volume01) {
				mSfxVolume = math::NkClamp(volume01, 0.f, 1.f);
			}

			void NkGemAudio::SetMuted(bool muted) {
				mMuted = muted;
				if (mReady && mMusicPlaying) {
					AudioEngine::Instance().SetVolume(mMusicHandle, mMuted ? 0.f : mMusicVolume);
				}
			}

		} // namespace ui
	} // namespace game
} // namespace nkentseu
