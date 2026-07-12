// =============================================================================
// NKSpeech/NkVoiceSynth.cpp — synthèse vocale par formants (source-filtre).
// =============================================================================
#include "NKSpeech/NkVoiceSynth.h"
#include "NKSpeech/NkGriffinLim.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace ai {

		using math::NkCos;
		using math::NkExp;
		using math::NkSqrt;

		NkPhone NkVoiceSynth::Vowel(char v, float32 durationMs) {
			NkPhone p;
			p.voiced = true;
			p.durationMs = durationMs;
			// Formants approximés (voix d'homme), tables de phonétique acoustique du français.
			switch (v) {
				case 'a': // /a/ patte
					p.f1 = 750.0f;
					p.f2 = 1300.0f;
					p.f3 = 2500.0f;
					break;
				case 'e': // /ɛ/ è
					p.f1 = 550.0f;
					p.f2 = 1750.0f;
					p.f3 = 2500.0f;
					break;
				case 'E': // /e/ é (fermé) — utile pour distinguer é/è
					p.f1 = 400.0f;
					p.f2 = 2200.0f;
					p.f3 = 2600.0f;
					break;
				case 'i': // /i/ lit
					p.f1 = 280.0f;
					p.f2 = 2300.0f;
					p.f3 = 3000.0f;
					break;
				case 'o': // /o/ eau
					p.f1 = 400.0f;
					p.f2 = 750.0f;
					p.f3 = 2500.0f;
					break;
				case 'u': // /y/ « u » FRANÇAIS : antérieure ARRONDIE (F2 haut, proche F3) — ≠ « ou »
					p.f1 = 270.0f;
					p.f2 = 1900.0f;
					p.f3 = 2250.0f;
					break;
				case 'w': // /u/ « ou » : postérieure arrondie (F2 bas)
					p.f1 = 300.0f;
					p.f2 = 800.0f;
					p.f3 = 2400.0f;
					break;
				default: // silence
					p.f1 = p.f2 = p.f3 = 0.0f;
					p.gain = 0.0f;
					break;
			}
			return p;
		}

		NkPhone NkVoiceSynth::Phoneme(const char *sym, float32 durationMs) {
			NkPhone p;
			p.durationMs = durationMs;
			if (!sym || !sym[0]) {
				p.gain = 0.0f;
				return p;
			}
			const char c = sym[0];
			const bool nasalV = sym[1] == '~'; // voyelle nasale (a~ o~ e~) — approximée
			// Voyelles (réutilise la table) + nasalisation légère (F1 abaissé).
			if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'w' || c == 'E') {
				p = Vowel(c, durationMs);
				if (nasalV)
					p.f1 *= 0.85f; // nasalisation approximée
				return p;
			}
			p.voiced = false;
			p.gain = 0.75f;
			switch (c) {
				// Fricatives non-voisées (bruit mis en forme par les « formants »).
				case 's': p.f1 = 4000; p.f2 = 6000; p.f3 = 7000; break;           // sifflante
				case 'f': p.f1 = 1200; p.f2 = 2400; p.f3 = 4000; p.gain = 0.5f; break; // diffuse
				case 'S': p.f1 = 2000; p.f2 = 2600; p.f3 = 3500; break;           // « ch »
				// Fricatives voisées (approximées : bruit + un peu de voix).
				case 'v': p.f1 = 1000; p.f2 = 2200; p.f3 = 3500; p.voiced = false; p.gain = 0.5f; break;
				case 'z': p.f1 = 3800; p.f2 = 5500; p.f3 = 6500; p.voiced = false; break;
				case 'Z': p.f1 = 1800; p.f2 = 2400; p.f3 = 3400; p.voiced = false; break; // « j »
				// Nasales (murmure voisé, F1 bas).
				case 'm': p.f1 = 250; p.f2 = 1000; p.f3 = 2200; p.voiced = true; break;
				case 'n': p.f1 = 250; p.f2 = 1700; p.f3 = 2600; p.voiced = true; break;
				// Liquides (voisées).
				case 'l': p.f1 = 350; p.f2 = 1200; p.f3 = 2600; p.voiced = true; break;
				case 'r': p.f1 = 400; p.f2 = 1300; p.f3 = 2500; p.voiced = true; break;
				// Plosives : SALVE brève (l'occlusion = un court silence inséré par Speak()).
				case 'p': p.f1 = 800; p.f2 = 1000; p.f3 = 2200; break;
				case 't': p.f1 = 3000; p.f2 = 4000; p.f3 = 5000; break;
				case 'k': p.f1 = 1500; p.f2 = 2200; p.f3 = 3000; break;
				case 'b': p.f1 = 800; p.f2 = 1000; p.f3 = 2200; p.voiced = true; break;
				case 'd': p.f1 = 2600; p.f2 = 3200; p.f3 = 4000; p.voiced = true; break;
				case 'g': p.f1 = 1500; p.f2 = 2000; p.f3 = 2800; p.voiced = true; break;
				default: p.gain = 0.0f; break; // inconnu / silence
			}
			return p;
		}

		namespace {
			bool IsPlosive(char c) {
				return c == 'p' || c == 't' || c == 'k' || c == 'b' || c == 'd' || c == 'g';
			}
			bool IsVowelSym(char c) {
				return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'w' || c == 'E';
			}
		} // namespace

		NkVector<float32> NkVoiceSynth::Speak(const char *phonemes, const NkVoiceSynthConfig &cfg) {
			NkVector<NkPhone> seq;
			if (!phonemes)
				return NkVector<float32>();
			// Tokenise sur les espaces ; chaque token = un symbole de phonème.
			char tok[8];
			int32 tl = 0;
			auto flush = [&]() {
				if (tl == 0)
					return;
				tok[tl] = '\0';
				// Durées par défaut : voyelle longue, consonne courte, salve plosive très courte.
				float32 d = IsVowelSym(tok[0]) ? 200.0f : 110.0f;
				if (IsPlosive(tok[0])) {
					// Occlusion (silence) + salve courte.
					NkPhone clo;
					clo.gain = 0.0f;
					clo.durationMs = 45.0f;
					seq.PushBack(clo);
					d = 22.0f;
				}
				seq.PushBack(Phoneme(tok, d));
				tl = 0;
			};
			for (const char *s = phonemes; *s; ++s) {
				if (*s == ' ') {
					flush();
				} else if (tl < 6) {
					tok[tl++] = *s;
				}
			}
			flush();
			// Petite marge de silence en fin (relâche propre).
			NkPhone end;
			end.gain = 0.0f;
			end.durationMs = 60.0f;
			seq.PushBack(end);
			return Synthesize(seq, cfg);
		}

		NkVoiceSynthConfig NkVoiceSynthConfig::Homme() {
			NkVoiceSynthConfig c;
			c.f0 = 110.0f;
			c.vocalTractScale = 1.0f;
			c.breathiness = 0.05f;
			return c;
		}
		NkVoiceSynthConfig NkVoiceSynthConfig::Femme() {
			NkVoiceSynthConfig c;
			c.f0 = 210.0f;
			c.vocalTractScale = 1.12f; // conduit un peu plus court → formants plus hauts
			c.breathiness = 0.09f;
			return c;
		}
		NkVoiceSynthConfig NkVoiceSynthConfig::Enfant() {
			NkVoiceSynthConfig c;
			c.f0 = 300.0f;
			c.vocalTractScale = 1.30f; // conduit court → formants nettement plus hauts
			c.breathiness = 0.08f;
			c.rate = 1.1f;
			return c;
		}
		NkVoiceSynthConfig NkVoiceSynthConfig::Geant() {
			NkVoiceSynthConfig c;
			c.f0 = 70.0f;
			c.vocalTractScale = 0.82f; // conduit long → formants plus bas (grave, caverneux)
			c.breathiness = 0.04f;
			c.rate = 0.9f;
			return c;
		}

		namespace {
			// Résonateur numérique de formant (Klatt) : y[n] = a·x[n] + b·y[n-1] + c·y[n-2].
			// Résonance à la fréquence F (Hz), largeur de bande BW (Hz). Gain unité en continu.
			struct Resonator {
					float32 a = 1.0f, b = 0.0f, c = 0.0f;
					float32 y1 = 0.0f, y2 = 0.0f;
					void Set(float32 F, float32 BW, int32 sr) {
						const float32 kPi = 3.14159265358979323846f;
						const float32 r = NkExp(-kPi * BW / (float32)sr);
						c = -r * r;
						b = 2.0f * r * NkCos(2.0f * kPi * F / (float32)sr);
						a = 1.0f - b - c;
					}
					float32 Step(float32 x) {
						const float32 y = a * x + b * y1 + c * y2;
						y2 = y1;
						y1 = y;
						return y;
					}
					void Reset() {
						y1 = 0.0f;
						y2 = 0.0f;
					}
			};

			// LCG déterministe pour le bruit des non-voisés.
			inline float32 Lcg(uint32 &s) {
				s = s * 1664525u + 1013904223u;
				return (float32)((s >> 8) & 0xFFFFu) / 32767.5f - 1.0f;
			}
		} // namespace

		NkVector<float32> NkVoiceSynth::Synthesize(const NkVector<NkPhone> &phones, const NkVoiceSynthConfig &cfg) {
			NkVector<float32> out;
			if (phones.Size() == 0 || cfg.sampleRate <= 0)
				return out;
			const int32 sr = cfg.sampleRate;
			const float32 rate = cfg.rate > 0.05f ? cfg.rate : 1.0f;
			const float32 vts = cfg.vocalTractScale > 0.1f ? cfg.vocalTractScale : 1.0f;

			// Durées par phone (débit) + total.
			NkVector<int32> dur;
			dur.Resize(phones.Size());
			int32 totalSamples = 0;
			for (uint32 i = 0; i < phones.Size(); ++i) {
				dur[i] = (int32)(phones[i].durationMs * 0.001f * (float32)sr / rate);
				totalSamples += dur[i];
			}
			if (totalSamples <= 0)
				return out;
			out.Resize((nk_size)totalSamples);

			// SYNTHÈSE TEMPORELLE source-filtre. SOURCE : impulsion glottique LISSÉE (dérivée d'une
			// bosse en cosinus surélevé → bande limitée, moins « buzz » qu'un Dirac) + SOUFFLE (bruit)
			// + JITTER (micro-variation de période) → plus naturel. FILTRE : 3 résonateurs de formants
			// (F1/F2/F3) mis à l'échelle du conduit vocal (vocalTractScale) et INTERPOLÉS d'un phone au
			// suivant (coarticulation). Contour d'intonation : F0 descend légèrement vers la fin.
			Resonator r1, r2, r3;
			uint32 rng = 0x1234abcdu;
			int32 idx = 0;
			float32 period = (float32)sr / cfg.f0;
			// Compteur d'échantillons depuis la dernière impulsion glottique. IMPORTANT : rester
			// BORNÉ (< 2·période) — le déclencheur fait `tSince -= period` (une seule soustraction).
			// L'initialiser à `period` fait sonner une impulsion dès le 1er échantillon voisé.
			float32 tSince = period;
			float32 prevG = 0.0f;
			float32 cf1 = 0.0f, cf2 = 0.0f, cf3 = 0.0f; // formants courants (pour la coarticulation)
			bool prevSounding = false;					// le phone précédent produisait-il du son ?
			const int32 glide = (int32)(cfg.glideMs * 0.001f * (float32)sr / rate);
			for (uint32 pi = 0; pi < phones.Size(); ++pi) {
				const NkPhone &p = phones[pi];
				const int32 nS = dur[pi];

				// SILENCE (gain nul) : sortie 0 + RESET des résonateurs → gap PROPRE (pas de
				// ring-down parasite « bruit après la lettre »), et redémarrage net après.
				if (p.gain <= 0.0f) {
					r1.Reset();
					r2.Reset();
					r3.Reset();
					prevG = 0.0f;
					tSince = period; // borné → impulsion dès la reprise du voisement
					for (int32 n = 0; n < nS && idx < totalSamples; ++n, ++idx)
						out[(nk_size)idx] = 0.0f;
					prevSounding = false;
					continue;
				}

				const float32 tf1 = p.f1 * vts, tf2 = p.f2 * vts, tf3 = p.f3 * vts;
				// Glissement de formants (coarticulation) UNIQUEMENT entre deux phones sonores
				// ADJACENTS (pas à travers un silence) : sinon chaque voyelle isolée démarre NETTE.
				const bool doGlide = prevSounding && cf1 > 0.0f && p.voiced;
				const float32 sf1 = cf1, sf2 = cf2, sf3 = cf3;
				if (!doGlide) { // démarrage net sur les formants cibles
					cf1 = tf1;
					cf2 = tf2;
					cf3 = tf3;
				}
				r1.Set(cf1, 80.0f, sr);
				r2.Set(cf2, 100.0f, sr);
				r3.Set(cf3, 120.0f, sr);

				for (int32 n = 0; n < nS && idx < totalSamples; ++n, ++idx) {
					// Reconfigure les filtres SEULEMENT pendant le glissement (évite le zipper noise).
					if (doGlide && n <= glide && glide > 0) {
						const float32 a = (float32)n / (float32)glide;
						cf1 = sf1 + (tf1 - sf1) * a;
						cf2 = sf2 + (tf2 - sf2) * a;
						cf3 = sf3 + (tf3 - sf3) * a;
						r1.Set(cf1, 80.0f, sr);
						r2.Set(cf2, 100.0f, sr);
						r3.Set(cf3, 120.0f, sr);
					}

					float32 src = 0.0f;
					if (p.voiced) {
						const float32 prog = (float32)idx / (float32)totalSamples;
						const float32 f0 = cfg.f0 * (1.0f - cfg.f0Drift * prog); // intonation descendante
						tSince += 1.0f;
						if (tSince >= period) {
							tSince -= period;
							const float32 j = 1.0f + cfg.f0Jitter * Lcg(rng);
							period = (float32)sr / (f0 * (j > 0.5f ? j : 0.5f));
						}
						const float32 pw = cfg.openQuotient * period;
						float32 gflow = 0.0f;
						if (tSince < pw)
							gflow = 0.5f * (1.0f - NkCos(2.0f * 3.14159265f * tSince / pw));
						const float32 deriv = gflow - prevG; // excitation bande-limitée (moins buzz)
						prevG = gflow;
						src = deriv + cfg.breathiness * Lcg(rng); // + un peu de souffle
					} else {
						src = 0.5f * Lcg(rng); // fricative : bruit
					}
					src *= p.gain;

					float32 y = r1.Step(src) + 0.7f * r2.Step(src) + 0.3f * r3.Step(src);
					float32 env = 1.0f; // attaque/relâche douce (anti-clic aux bords)
					const int32 fade = nS / 8 > 1 ? nS / 8 : 1;
					if (n < fade)
						env = (float32)n / (float32)fade;
					else if (n > nS - fade)
						env = (float32)(nS - n) / (float32)fade;
					out[(nk_size)idx] = y * env;
				}
				prevSounding = true;
			}

			// Normalisation à ~0,9 crête.
			float32 peak = 1e-6f;
			for (uint32 i = 0; i < out.Size(); ++i) {
				const float32 a = out[i] < 0.0f ? -out[i] : out[i];
				if (a > peak)
					peak = a;
			}
			const float32 g = 0.9f / peak;
			for (uint32 i = 0; i < out.Size(); ++i)
				out[i] *= g;
			return out;
		}

		bool NkVoiceSynth::SelfTest() {
			// Synthétise la voyelle 'a' (F1≈800, F2≈1200) puis vérifie que le spectre du signal
			// produit présente bien de l'énergie autour de F1 et F2.
			NkVector<NkPhone> seq;
			seq.PushBack(Vowel('a', 400.0f));
			NkVoiceSynthConfig cfg;
			NkVector<float32> wav = NkVoiceSynth::Synthesize(seq, cfg);
			if ((int32)wav.Size() < cfg.fftSize)
				return false;

			// (0) Son SOUTENU (pas des clics isolés) : une bonne part des échantillons doit
			//     dépasser 10 % de la crête. Un train de clics échouerait ici.
			float32 peak = 1e-6f;
			for (uint32 i = 0; i < wav.Size(); ++i) {
				const float32 a = wav[i] < 0.0f ? -wav[i] : wav[i];
				if (a > peak)
					peak = a;
			}
			int32 loud = 0;
			for (uint32 i = 0; i < wav.Size(); ++i) {
				const float32 a = wav[i] < 0.0f ? -wav[i] : wav[i];
				if (a > 0.1f * peak)
					++loud;
			}
			const double sustained = (double)loud / (double)wav.Size();
			if (sustained < 0.30)
				return false; // signal soutenu attendu (voyelle tenue), pas des impulsions

			// (0bis) PÉRIODIQUE à ~F0 (voisement réel, pas du bruit). Autocorrélation normalisée
			//        au lag = période attendue (sr/F0). Un signal non voisé (bug des impulsions
			//        glottiques) donnerait une corrélation faible → échec.
			{
				const int32 lag = (int32)((float32)cfg.sampleRate / cfg.f0 + 0.5f);
				const int32 c0 = (int32)wav.Size() / 4;
				const int32 M = 3000;
				if (c0 + M + lag < (int32)wav.Size()) {
					double num = 0.0, d1 = 0.0, d2 = 0.0;
					for (int32 i = 0; i < M; ++i) {
						const double x0 = wav[(nk_size)(c0 + i)], x1 = wav[(nk_size)(c0 + i + lag)];
						num += x0 * x1;
						d1 += x0 * x0;
						d2 += x1 * x1;
					}
					const double corr = num / (NkSqrt((float32)(d1 * d2)) + 1e-9);
					if (corr < 0.20)
						return false; // pas assez périodique -> pas de vraie voix (bug voisement)
				}
			}

			NkGriffinLimConfig gl;
			gl.fftSize = cfg.fftSize;
			gl.hopSize = cfg.hopSize;
			NkMagSpectrogram m = NkGriffinLim::Magnitude(wav.Data(), (int32)wav.Size(), gl);
			if (m.frames <= 2)
				return false;
			// Spectre moyen (trames internes).
			NkVector<double> avg;
			avg.Resize((nk_size)m.bins);
			for (int32 k = 0; k < m.bins; ++k)
				avg[(nk_size)k] = 0.0;
			for (int32 f = 2; f < m.frames - 2; ++f)
				for (int32 k = 0; k < m.bins; ++k)
					avg[(nk_size)k] += m.data[(nk_size)f * (nk_size)m.bins + (nk_size)k];

			const float32 binHz = (float32)cfg.sampleRate / (float32)cfg.fftSize;
			auto energyAround = [&](float32 fc, float32 halfBw) {
				const int32 k0 = (int32)((fc - halfBw) / binHz);
				const int32 k1 = (int32)((fc + halfBw) / binHz);
				double s = 0.0;
				for (int32 k = (k0 > 0 ? k0 : 0); k <= k1 && k < m.bins; ++k)
					s += avg[(nk_size)k];
				return s;
			};
			double total = 0.0;
			for (int32 k = 0; k < m.bins; ++k)
				total += avg[(nk_size)k];
			if (total <= 0.0)
				return false;
			const double eF1 = energyAround(800.0f, 150.0f) / total;
			const double eF2 = energyAround(1200.0f, 150.0f) / total;
			// Les deux régions de formants doivent concentrer une part significative de l'énergie.
			return eF1 > 0.05 && eF2 > 0.02;
		}

	} // namespace ai
} // namespace nkentseu
