// =============================================================================
// NkLora.h — adaptateurs LoRA (Low-Rank Adaptation) pour les projections
// linéaires du bloc Qwen2 (NKInfer, chantier QLoRA, Jalon 2 — cf
// Documentation/notes_etape4_qlora.md §2.4 et §4).
//
// PRINCIPE (Hu et al., « LoRA: Low-Rank Adaptation of Large Language Models »,
// arXiv:2106.09685) : le socle W₀ [out,in] reste GELÉ ; on n'apprend que deux
// petites matrices A [r,in] et B [out,r] (r << min(out,in)) :
//     y = W₀·x + (alpha/r) · B·(A·x)
// Initialisation standard : A ~ N(0, sigma), B = 0 — à l'initialisation le
// delta est EXACTEMENT nul, donc insérer un adaptateur ne change rien à la
// sortie du socle tant qu'il n'a pas appris (propriété vérifiée bit-à-bit par
// Applications/NKQwen2BackwardTest).
//
// BACKWARD (dérivées exactes, convention x [T,in] : une ligne = une position,
// scale = alpha/r) :
//     u  = x·Aᵀ            [T,r]   (recalculé au backward : il coûte moins cher
//                                   de le refaire que de le stocker — cohérent
//                                   avec le « checkpointing absorbé » du plan)
//     dB = scale · dYᵀ·u   [out,r]
//     du = scale · dY·B    [T,r]
//     dA = duᵀ·x           [r,in]
//     dX += du·A           [T,in]
// AUCUN gradient pour W₀ : le socle est gelé par construction (le backward de
// la projection complète vit dans NkQwen2Backward, qui ne demande que dX).
//
// Zéro STL, CPU pur : les produits matriciels passent par NkLinearNoBias
// (NkQwen2Block.h) et par les helpers CPU stricts ci-dessous — JAMAIS
// ops::Matmul, dont la bascule GPU automatique (M·N·K >= 8e6) est indésirable
// dans un chemin voulu-CPU (piège n°2 des notes QLoRA).
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKTensor/NkTensor.h"

namespace nkentseu {
	namespace ai {
		namespace infer {

			// ---- Produits matriciels CPU stricts (partagés LoRA / backward Qwen2) ----
			// POURQUOI ici et pas dans NkQwen2Block : le forward d'inférence n'en a
			// pas besoin (il ne fait que x·Wᵀ, déjà couvert par NkLinearNoBias) ;
			// le backward, lui, a besoin des deux autres orientations. On ne touche
			// pas au module d'inférence validé (méthode additive, piège n°7).

			// a[M,K] · b[K,N] -> [M,N]. Ordre de boucles i-k-j (cache-friendly),
			// accumulation f32, aucun dispatch GPU.
			NkTensor NkCpuMatmulAB(const NkTensor &a, const NkTensor &b);

			// aᵀ·b : a[K,M], b[K,N] -> [M,N] (SANS matérialiser la transposée de a —
			// la transposée générique élément-par-élément est un poison connu,
			// piège n°4 des notes QLoRA).
			NkTensor NkCpuMatmulATB(const NkTensor &a, const NkTensor &b);

			// ---- RNG reproductible (xorshift64* + Box-Muller) -----------------------
			// POURQUOI un RNG maison : zéro STL (pas de <random>), et les tests de
			// gradient exigent des poids REPRODUCTIBLES par graine fixe.
			struct NkLoraRng {
					uint64 state = 0x9E3779B97F4A7C15ull;

					explicit NkLoraRng(uint64 seed);
					uint64 NextU64();
					float64 NextUniform();	// dans (0,1] — jamais 0 (log() en dépend)
					float32 NextGaussian(); // N(0,1), Box-Muller (le spare est gardé)

				private:
					float64 mSpare = 0.0;
					bool mHasSpare = false;
			};

			// ---- Paire d'adaptateurs LoRA d'UNE projection --------------------------
			struct NkLoraPair {
					NkTensor A;			  // [r, in]  — init N(0, sigma)
					NkTensor B;			  // [out, r] — init 0 (delta initial nul)
					float32 alpha = 0.0f; // échelle numérateur (scale = alpha/r)
					int32 r = 0;		  // rang

					bool IsValid() const {
						return r > 0 && A.IsValid() && B.IsValid();
					}

					float32 Scale() const {
						return r > 0 ? alpha / (float32)r : 0.0f;
					}

					// Fabrique : A ~ N(0, sigma) tirée de `rng`, B = 0.
					static NkLoraPair Create(int32 outFeatures, int32 inFeatures, int32 r, float32 alpha, float32 sigma,
											 NkLoraRng &rng);
			};

			// Gradients d'une paire (mêmes formes que A/B). Les fonctions de backward
			// ACCUMULENT dedans (+=) : dA/dB invalides sont alloués à zéro d'abord —
			// permet l'accumulation de gradient multi-lots sans code supplémentaire.
			struct NkLoraGrad {
					NkTensor dA; // [r, in]
					NkTensor dB; // [out, r]
			};

			// delta = scale · (x·Aᵀ)·Bᵀ. x : [T,in] f32 -> [T,out]. Tenseur invalide
			// si formes/dtype incohérents (message stderr, comme NkQwen2Block).
			NkTensor NkLoraForwardDelta(const NkLoraPair &p, const NkTensor &x);

			// Backward de la contribution LoRA : `x` est l'ENTRÉE du forward (même
			// tenseur), `dY` [T,out] le gradient de la sortie. Accumule dans g.dA et
			// g.dB ; si `dXAccum` est non nul et valide, y accumule (+=) du·A — la
			// contribution de l'adaptateur au gradient qui TRAVERSE la projection
			// (celle du socle gelé, dY·W₀, est ajoutée par l'appelant).
			bool NkLoraBackward(const NkLoraPair &p, const NkTensor &x, const NkTensor &dY, NkLoraGrad &g,
								NkTensor *dXAccum);

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
