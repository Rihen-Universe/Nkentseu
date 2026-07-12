// =============================================================================
// NKGpt/NkSampling.h — échantillonnage de tokens (température + top-k + top-p).
//
// Fonctions d'échantillonnage RÉUTILISABLES pour tout décodage autoregressif
// (GPT ici, mais aussi TTS/agents plus tard). Sans STL. À partir d'un vecteur de
// logits bruts, applique :
//   1. TEMPÉRATURE  : logit/τ (τ<1 = plus sûr, τ>1 = plus créatif ; τ→0 = argmax) ;
//   2. TOP-K        : ne garde que les k plus fortes probabilités (0 = désactivé) ;
//   3. TOP-P/nucleus: ne garde que le plus petit ensemble dont la masse ≥ p (1 = off) ;
//   4. renormalise puis tire au sort (multinomial) via un LCG déterministe.
// Namespace nkentseu::ai::gpt.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NkFunctions.h" // NkExp

namespace nkentseu {
	namespace ai {
		namespace gpt {

			struct NkSampleParams {
					double temperature = 1.0; // τ ; ≤0 traité comme argmax déterministe
					int topK = 0;			  // >0 : garder les k meilleurs
					double topP = 1.0;		  // <1 : nucleus (masse cumulée)
			};

			// Échantillonne un id dans [0, limit) depuis `logits` (taille ≥ limit). `rng` est
			// un état LCG modifié en place (reproductible). Renvoie l'index choisi.
			inline int NkSampleToken(const float *logits, int limit, const NkSampleParams &p, uint64 &rng) {
				if (limit <= 0)
					return 0;
				// Argmax déterministe si température ~nulle.
				if (p.temperature <= 1e-6) {
					int best = 0;
					for (int v = 1; v < limit; ++v)
						if (logits[v] > logits[best])
							best = v;
					return best;
				}
				const double invT = 1.0 / p.temperature;

				// Softmax (stable) sur les `limit` premiers logits.
				double mx = -1e30;
				for (int v = 0; v < limit; ++v)
					if ((double)logits[v] > mx)
						mx = (double)logits[v];
				NkVector<double> prob;
				prob.Resize((nk_size)limit);
				double sum = 0.0;
				for (int v = 0; v < limit; ++v) {
					const double e = math::NkExp(((double)logits[v] - mx) * invT);
					prob[(nk_size)v] = e;
					sum += e;
				}
				const double invSum = (sum > 0.0) ? 1.0 / sum : 0.0;
				for (int v = 0; v < limit; ++v)
					prob[(nk_size)v] *= invSum;

				// Tri des indices par probabilité DÉCROISSANTE (tri par insertion ; limit petit).
				NkVector<int> order;
				order.Resize((nk_size)limit);
				for (int v = 0; v < limit; ++v)
					order[(nk_size)v] = v;
				for (int i = 1; i < limit; ++i) {
					const int key = order[(nk_size)i];
					const double kp = prob[(nk_size)key];
					int j = i - 1;
					while (j >= 0 && prob[(nk_size)order[(nk_size)j]] < kp) {
						order[(nk_size)(j + 1)] = order[(nk_size)j];
						--j;
					}
					order[(nk_size)(j + 1)] = key;
				}

				// Détermine la taille du noyau conservé : cap top-k ET seuil top-p.
				const int kCap = (p.topK > 0 && p.topK < limit) ? p.topK : limit;
				double cum = 0.0;
				int keep = 0;
				for (int i = 0; i < kCap; ++i) {
					cum += prob[(nk_size)order[(nk_size)i]];
					++keep;
					if (p.topP < 1.0 && cum >= p.topP)
						break;
				}
				if (keep < 1)
					keep = 1;

				// Renormalise sur le noyau puis tire au sort (multinomial).
				double keptSum = 0.0;
				for (int i = 0; i < keep; ++i)
					keptSum += prob[(nk_size)order[(nk_size)i]];
				rng = rng * 6364136223846793005ull + 1442695040888963407ull;
				const double r = ((double)((rng >> 11) & 0xFFFFFFFFFFFFFull) / (double)0x10000000000000ull) * keptSum;
				double acc = 0.0;
				for (int i = 0; i < keep; ++i) {
					acc += prob[(nk_size)order[(nk_size)i]];
					if (acc >= r)
						return order[(nk_size)i];
				}
				return order[0];
			}

		} // namespace gpt
	} // namespace ai
} // namespace nkentseu
