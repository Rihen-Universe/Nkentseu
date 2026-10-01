// =============================================================================
// NkUnkenySon.cpp
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Unkeny/Son/NkUnkenySon.h"

#include "NKAudio/NkAudio.h"
#include "NKLogger/NkLog.h"
#include "NKMemory/NKMemory.h"
#include "Unkeny/Scene/NkUnkenyScene.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace unkeny {

		using audio::AudioEngine;
		using audio::AudioHandle;
		using audio::AudioSample;

		namespace {
			AudioSample *Echantillon(void *p) {
				return static_cast<AudioSample *>(p);
			}
			void Copier(char *dst, usize taille, const char *src) {
				std::snprintf(dst, taille, "%s", src != nullptr ? src : "");
			}
			float32 Clamp(float32 v, float32 a, float32 b) {
				return v < a ? a : (v > b ? b : v);
			}
		} // namespace

		NkSons2D::~NkSons2D() {
			Arreter();
			for (uint32 i = 0; i < mSons.Size(); ++i) {
				Liberer(mSons[i]);
			}
			mSons.Clear();
		}

		bool NkSons2D::Demarrer(bool sortieNulle) {
			if (mActif) {
				return true;
			}
			AudioEngine &m = AudioEngine::Instance();
			if (m.IsInitialized()) {
				mActif = true; // quelqu'un d'autre l'a demarre : on s'en sert, on ne l'arretera pas
				mProprietaire = false;
				return true;
			}
			audio::AudioEngineConfig cfg;
			if (sortieNulle) {
				cfg.backend = audio::AudioBackendType::NULL_OUTPUT;
			}
			mActif = m.Initialize(cfg);
			mProprietaire = mActif;
			if (!mActif) {
				logger.Warn("[unkeny] pas de son : NKAudio n'a pas demarre — le jeu continue en silence");
			}
			return mActif;
		}

		void NkSons2D::Arreter() {
			if (!mActif) {
				return;
			}
			for (uint32 i = 0; i < mSuivis.Size(); ++i) {
				AudioEngine::Instance().Stop(AudioHandle{mSuivis[i].voix});
			}
			mSuivis.Clear();
			if (mProprietaire) {
				AudioEngine::Instance().Shutdown();
			}
			mActif = false;
			mProprietaire = false;
		}

		void NkSons2D::PoserRacine(const char *racine) {
			Copier(mRacine, sizeof(mRacine), racine);
			mRacinePosee = true;
		}

		uint32 NkSons2D::Charger(const char *chemin) {
			if (chemin == nullptr || chemin[0] == '\0') {
				return 0u;
			}
			if (const uint32 deja = Trouver(chemin)) {
				return deja;
			}
			if (!mRacinePosee) {
#if defined(__ANDROID__) || defined(NKENTSEU_PLATFORM_ANDROID)
				PoserRacine("");
#else
				PoserRacine("assets/");
#endif
			}
			char complet[320];
			const bool absolu = chemin[0] == '/' || (chemin[0] != '\0' && chemin[1] == ':');
			std::snprintf(complet, sizeof(complet), "%s%s", absolu ? "" : mRacine, chemin);
			AudioSample lu = audio::AudioLoader::Load(complet);
			if (!lu.IsValid()) {
				logger.Warn("[unkeny] son introuvable ou illisible : {0}", complet);
				return 0u;
			}
			AudioSample *e = memory::NkGetDefaultAllocator().New<AudioSample>();
			if (e == nullptr) {
				audio::AudioLoader::Free(lu);
				return 0u;
			}
			*e = lu;
			NkSon s;
			Copier(s.nom, sizeof(s.nom), chemin);
			s.echantillon = e;
			s.duChargeur = true;
			mSons.PushBack(s);
			return static_cast<uint32>(mSons.Size());
		}

		uint32 NkSons2D::Creer(const float32 *mono, usize frames, int32 frequence, const char *nom) {
			if (mono == nullptr || frames == 0 || frequence <= 0) {
				return 0u;
			}
			if (nom != nullptr && nom[0] != '\0') {
				if (const uint32 deja = Trouver(nom)) {
					return deja;
				}
			}
			// ⚠️ NkAlloc / NkFree, jamais l'allocateur du chargeur : melanger les
			// deux corrompt le tas (NkGemAudio.cpp, AllocateMono, l'a paye).
			float32 *donnees = static_cast<float32 *>(memory::NkAlloc(frames * sizeof(float32)));
			AudioSample *e = memory::NkGetDefaultAllocator().New<AudioSample>();
			if (donnees == nullptr || e == nullptr) {
				if (donnees != nullptr) {
					memory::NkFree(donnees);
				}
				if (e != nullptr) {
					memory::NkGetDefaultAllocator().Delete(e);
				}
				return 0u;
			}
			std::memcpy(donnees, mono, frames * sizeof(float32));
			e->data = donnees;
			e->frameCount = frames;
			e->sampleRate = frequence;
			e->channels = 1;
			e->mAllocator = &memory::NkGetDefaultAllocator();
			NkSon s;
			Copier(s.nom, sizeof(s.nom), nom);
			s.echantillon = e;
			s.duChargeur = false;
			mSons.PushBack(s);
			return static_cast<uint32>(mSons.Size());
		}

		void NkSons2D::Liberer(NkSon &s) {
			AudioSample *e = Echantillon(s.echantillon);
			if (e == nullptr) {
				return;
			}
			if (s.duChargeur) {
				audio::AudioLoader::Free(*e);
			} else if (e->data != nullptr) {
				memory::NkFree(e->data);
				e->data = nullptr;
			}
			memory::NkGetDefaultAllocator().Delete(e);
			s.echantillon = nullptr;
		}

		uint32 NkSons2D::Trouver(const char *nom) const noexcept {
			if (nom == nullptr || nom[0] == '\0') {
				return 0u;
			}
			for (uint32 i = 0; i < mSons.Size(); ++i) {
				if (std::strcmp(mSons[i].nom, nom) == 0) {
					return i + 1u;
				}
			}
			return 0u;
		}

		const char *NkSons2D::Nom(uint32 son) const noexcept {
			if (son == 0u || son > mSons.Size()) {
				return "";
			}
			return mSons[son - 1u].nom;
		}

		float32 NkSons2D::Duree(uint32 son) const noexcept {
			if (son == 0u || son > mSons.Size()) {
				return 0.f;
			}
			const AudioSample *e = Echantillon(mSons[son - 1u].echantillon);
			return e != nullptr ? e->GetDuration() : 0.f;
		}

		uint32 NkSons2D::Voix(uint32 son, float32 volume, float32 pitch, float32 pan, bool boucle, const char *bus) {
			if (!mActif || son == 0u || son > mSons.Size()) {
				return 0u;
			}
			const AudioSample *e = Echantillon(mSons[son - 1u].echantillon);
			if (e == nullptr || !e->IsValid()) {
				return 0u;
			}
			audio::VoiceParams p;
			p.volume = mMuet ? 0.f : volume * volumeGeneral;
			p.pitch = Clamp(pitch, 0.25f, 4.f);
			p.pan = Clamp(pan, -1.f, 1.f);
			p.looping = boucle;
			p.bus = bus != nullptr ? bus : "SFX";
			return AudioEngine::Instance().Play(*e, p).id;
		}

		uint32 NkSons2D::Jouer(uint32 son, float32 volume, float32 pitch, float32 pan, bool boucle, const char *bus) {
			return Voix(son, volume, pitch, pan, boucle, bus);
		}

		uint32 NkSons2D::JouerA(uint32 son, const NkVec2f &position, const NkVue2D &camera, float32 volume, float32 portee) {
			float32 pan = 0.f, gain = 1.f;
			Spatialiser(position, camera, portee, pan, gain);
			if (gain <= 0.001f) {
				return 0u; // trop loin pour etre entendu : on n'occupe pas une voix
			}
			return Voix(son, volume * gain, 1.f, pan, false, "SFX");
		}

		void NkSons2D::Couper(uint32 voix, float32 fondu) {
			if (mActif && voix != 0u) {
				AudioEngine::Instance().Stop(AudioHandle{voix}, fondu);
			}
		}

		bool NkSons2D::Joue(uint32 voix) const {
			return mActif && voix != 0u && AudioEngine::Instance().IsPlaying(AudioHandle{voix});
		}

		void NkSons2D::PoserMuet(bool muet) noexcept {
			mMuet = muet;
		}

		void NkSons2D::Spatialiser(const NkVec2f &position, const NkVue2D &camera, float32 portee, float32 &pan,
								   float32 &gain) noexcept {
			const NkRect vue = camera.ZoneVisible();
			const float32 demi = vue.w * 0.5f;
			const float32 cx = vue.x + demi;
			pan = demi > 1.0e-5f ? Clamp((position.x - cx) / demi, -1.f, 1.f) * 0.8f : 0.f;
			// Distance a la ZONE visible (0 dedans).
			const float32 dx = position.x < vue.x ? vue.x - position.x : (position.x > vue.x + vue.w ? position.x - (vue.x + vue.w) : 0.f);
			const float32 dy = position.y < vue.y ? vue.y - position.y : (position.y > vue.y + vue.h ? position.y - (vue.y + vue.h) : 0.f);
			const float32 d = math::NkSqrt(dx * dx + dy * dy);
			const float32 t = portee > 1.0e-5f ? Clamp(1.f - d / portee, 0.f, 1.f) : (d > 0.f ? 0.f : 1.f);
			gain = t * t;
		}

		void NkSons2D::Avancer(NkScene &scene) {
			ecs::NkWorld &monde = scene.Monde();
			const NkVue2D &cam = scene.Camera();

			// 1. Les voix dont l'entite a disparu : coupees. Sans cela, une boucle
			//    de moteur continuerait apres la destruction de la voiture.
			for (uint32 i = 0; i < mSuivis.Size();) {
				const ecs::NkEntityId id = ecs::NkEntityId::Unpack(mSuivis[i].entite);
				const NkSource2D *s = monde.IsAlive(id) ? monde.Get<NkSource2D>(id) : nullptr;
				const bool fini = mActif && !AudioEngine::Instance().IsPlaying(AudioHandle{mSuivis[i].voix}) &&
								  !AudioEngine::Instance().IsPaused(AudioHandle{mSuivis[i].voix});
				if (s == nullptr || s->voix != mSuivis[i].voix || fini) {
					if (s == nullptr || s->voix != mSuivis[i].voix) {
						Couper(mSuivis[i].voix, 0.05f);
					}
					mSuivis[i] = mSuivis[mSuivis.Size() - 1u];
					mSuivis.PopBack();
				} else {
					++i;
				}
			}

			// 2. Chaque source : lancer, spatialiser, arreter.
			monde.Query<NkSource2D>().ForEach([&](ecs::NkEntityId id, NkSource2D &s) {
				const NkTransform2D *t = monde.Get<NkTransform2D>(id);
				if (s.spatial && t != nullptr) {
					Spatialiser(t->position, cam, s.portee, s.pan, s.gain);
				} else {
					s.pan = 0.f;
					s.gain = 1.f;
				}
				// Une voix qu'on ne suit pas (photo restauree, fichier relu, son
				// termine) n'existe plus : on l'oublie.
				if (s.voix != 0u) {
					bool suivie = false;
					for (uint32 i = 0; i < mSuivis.Size() && !suivie; ++i) {
						suivie = mSuivis[i].voix == s.voix && mSuivis[i].entite == id.Pack();
					}
					if (!suivie) {
						s.voix = 0u;
					}
				}
				// Une entite ETEINTE (NkUnkenyActif.h) se tait : sa voix est coupee,
				// rien n'est lance. `lance` reste : un son « au demarrage » repart
				// quand elle se rallume (comme un PlayOnAwake).
				if (!NkEntiteActive(monde, id)) {
					if (s.voix != 0u) {
						Couper(s.voix, 0.05f);
						s.voix = 0u;
					}
					s.lance = false;
					s.demande = false;
					return;
				}
				if (s.arret) {
					s.arret = false;
					Couper(s.voix, 0.05f);
					s.voix = 0u;
					s.demande = false;
				}
				// Une BOUCLE a auDemarrage revit aussi apres une restauration : un
				// bruit d'ambiance ne doit pas disparaitre apres Jouer / Arreter.
				const bool lancer = s.demande || (s.auDemarrage && (!s.lance || (s.boucle && s.voix == 0u)));
				s.demande = false;
				if (lancer && s.son != 0u) {
					s.lance = true;
					if (s.voix != 0u) {
						Couper(s.voix, 0.f); // une demande relance depuis le debut
					}
					s.voix = Voix(s.son, s.volume * s.gain, s.pitch, s.pan, s.boucle, "SFX");
					if (s.voix != 0u) {
						mSuivis.PushBack(NkSuivi{id.Pack(), s.voix});
					}
				} else if (s.voix != 0u && mActif) {
					AudioEngine::Instance().SetPan(AudioHandle{s.voix}, s.pan);
					AudioEngine::Instance().SetVolume(AudioHandle{s.voix}, mMuet ? 0.f : s.volume * s.gain * volumeGeneral);
					AudioEngine::Instance().SetPitch(AudioHandle{s.voix}, Clamp(s.pitch, 0.25f, 4.f));
				}
			});
		}

	} // namespace unkeny
} // namespace nkentseu
