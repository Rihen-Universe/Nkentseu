// =============================================================================
// NkQwen2Backward.h — backward MANUEL d'une couche Qwen2 + adaptateurs LoRA
// (NKInfer, chantier QLoRA, Jalon 2 — cf Documentation/notes_etape4_qlora.md
// §2.3-§2.4 et §4).
//
// POURQUOI un backward manuel hors-graphe plutôt qu'étendre NKAutograd : le
// socle est GELÉ (aucun gradient voulu pour wq/wk/wv/wo/gate/up/down — un
// autograd générique en calculerait ou traînerait des feuilles mortes), le
// checkpointing par couche devient TRIVIAL (on rejoue le forward de la couche
// juste avant son backward — c'est le « checkpointing absorbé » décidé dans
// les notes), et la mémoire est bornée par UNE couche. L'autograd (NkVar) ne
// connaît de toute façon ni RMSNorm, ni RoPE, ni SwiGLU, ni l'attention GQA.
//
// CONTRAT DE COHÉRENCE avec le forward d'inférence (NkQwen2Block, INTACT) :
// NkQwen2LayerForwardTrain rejoue EXACTEMENT les mêmes opérations dans le même
// ordre que NkQwen2LayerForward (mêmes NkRMSNorm/NkSiLU/NkLinearNoBias/
// NkApplyRoPE exportés, même produit matriciel interne recopié à l'identique)
// — sans adaptateur (ou avec B=0), la sortie est identique à 0.0 près (testé
// par Applications/NKQwen2BackwardTest). Portée : B(atch)=1, séquence complète
// depuis la position 0 (pas de KV-cache : l'entraînement voit toute la
// séquence d'un coup, le cache est un mécanisme d'inférence incrémentale).
//
// GRADIENTS PRODUITS (poids du socle GELÉS) :
//   (a) dX : le gradient qui TRAVERSE la couche (permettra de chaîner les
//       couches au Jalon 3) ;
//   (b) dA/dB de chaque adaptateur LoRA inséré (q/k/v/o/gate/up/down).
//
// DÉRIVÉES (écrites à la main, prouvées par différences finies dans le test) :
//   - RMSNorm  y_i = x_i·inv·w_i, inv = (mean(x²)+eps)^-1/2 :
//       dx_j = inv·w_j·dy_j − (inv³/d)·x_j·Σ_i(w_i·x_i·dy_i)
//   - RoPE : rotation ORTHOGONALE par paire (i, i+half) -> le backward est la
//       rotation d'angle opposé appliquée au gradient (Rᵀ = R(−θ)) ;
//   - softmax causal (échelle 1/√headDim incluse) :
//       ds_c = P_c·(dP_c − Σ_{c'}P_{c'}·dP_{c'}) sur les colonnes autorisées ;
//   - GQA : les gradients des têtes Q d'un même groupe s'ACCUMULENT sur leur
//       tête K/V partagée ;
//   - SwiGLU h = silu(g)⊙u : dg = dh⊙u⊙silu'(g), du = dh⊙silu(g),
//       silu'(g) = σ(g)·(1 + g·(1−σ(g))) ;
//   - projections gelées y = x·Wᵀ(+b) : dX = dY·W (pas de dW, pas de db) ;
//   - LoRA : voir NkLora.h.
//
// Les briques sont EXPOSÉES individuellement (pas seulement la couche
// complète) : chacune doit être prouvable par gradient numérique ISOLÉMENT
// (méthode « test isolé pour un calcul suspect » du projet).
//
// Zéro STL, CPU pur (aucun dispatch GPU). Namespace ai::infer.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKTensor/NkTensor.h"
#include "NKInfer/NkQwen2Block.h"
#include "NKInfer/NkLora.h"

namespace nkentseu {
	namespace ai {
		namespace infer {

			// ---- Briques isolées --------------------------------------------------

			// Backward du RMSNorm : renvoie dX [T,d] (ou [d]) depuis dY de même
			// forme. `x` et `weight` sont ceux du FORWARD. Pas de dWeight : le
			// socle (poids de norme inclus) est gelé.
			NkTensor NkRMSNormBackward(const NkTensor &x, const NkTensor &weight, float32 eps, const NkTensor &dY);

			// Backward du RoPE : applique EN PLACE la rotation INVERSE (angle −θ)
			// sur `dx` ([H,T,headDim] f32 contigu) — mêmes conventions (NEOX,
			// posOffset, freqBase) que NkApplyRoPE.
			void NkApplyRoPEBackward(NkTensor &dx, int32 posOffset, float32 freqBase);

			// Backward du SwiGLU élémentaire : `g` = gate PRÉ-SiLU, `u` = up (tous
			// deux [T,ffn], tels que h = silu(g)⊙u), `dH` le gradient de h.
			// Produit dG et dU (mêmes formes). false si formes incompatibles.
			bool NkSwiGLUBackward(const NkTensor &g, const NkTensor &u, const NkTensor &dH, NkTensor &dG, NkTensor &dU);

			// Attention GQA causale depuis la position 0 (séquence complète, sans
			// cache). Q [nH,T,headDim], K/V [nKVHeads,T,headDim] (K/Q déjà tournés
			// par RoPE). Renvoie ctx [nH,T,headDim] ; si `outProbs` est non nul, y
			// écrit les probabilités softmax [nH,T,T] (nécessaires au backward).
			// Numériquement IDENTIQUE à la boucle d'attention de
			// NkQwen2LayerForward (mêmes produits, même softmax, même ordre).
			NkTensor NkGQAAttentionForward(const NkTensor &Q, const NkTensor &K, const NkTensor &V, NkTensor *outProbs);

			// Backward de l'attention GQA : depuis dCtx [nH,T,headDim] et les
			// activations du forward, produit dQ [nH,T,headDim] et dK/dV
			// [nKVHeads,T,headDim] (accumulation des têtes Q d'un groupe sur leur
			// tête K/V partagée). L'échelle 1/√headDim est incluse.
			bool NkGQAAttentionBackward(const NkTensor &Q, const NkTensor &K, const NkTensor &V, const NkTensor &probs,
										const NkTensor &dCtx, NkTensor &dQ, NkTensor &dK, NkTensor &dV);

			// ---- Couche complète --------------------------------------------------

			// Adaptateurs LoRA d'une couche : une paire par projection. Une paire
			// INVALIDE (défaut) = pas d'adaptateur sur cette projection — le chemin
			// du socle est alors strictement celui de l'inférence.
			struct NkQwen2LoraSet {
					NkLoraPair q, k, v, o, gate, up, down;
			};

			// Gradients des adaptateurs (mêmes conventions d'accumulation que
			// NkLoraGrad : alloués à zéro au premier usage, puis +=).
			struct NkQwen2LoraSetGrads {
					NkLoraGrad q, k, v, o, gate, up, down;
			};

			// Activations sauvegardées par NkQwen2LayerForwardTrain — le MINIMUM
			// nécessaire au backward (cf notes §4 Jalon 2) : entrées des deux
			// normes, entrées de chaque projection porteuse de LoRA, q/k post-RoPE,
			// probs softmax, gate/up. Tous des tenseurs FRAIS (aucun aliasing avec
			// des buffers réutilisables — piège n°1 : copie NkTensor = vue).
			struct NkQwen2TrainSaved {
					NkTensor x;		// [T,d]  entrée de la couche (clone défensif)
					NkTensor xn1;	// [T,d]  post attnNorm = entrée de wq/wk/wv
					NkTensor Q;		// [nH,T,headDim]  POST-RoPE
					NkTensor K;		// [nKV,T,headDim] POST-RoPE
					NkTensor V;		// [nKV,T,headDim]
					NkTensor probs; // [nH,T,T] softmax causal
					NkTensor ctxT;	// [T,nH*headDim]  entrée de wo
					NkTensor x1;	// [T,d]  après résiduel d'attention
					NkTensor xn2;	// [T,d]  post ffnNorm = entrée de gate/up
					NkTensor g;		// [T,ffn] gate PRÉ-SiLU
					NkTensor u;		// [T,ffn] up
					NkTensor h;		// [T,ffn] silu(g)⊙u = entrée de wDown
			};

			// Forward d'entraînement d'UNE couche : mêmes maths que
			// NkQwen2LayerForward (séquence complète, position 0) + deltas LoRA sur
			// les projections équipées + sauvegarde des activations pour le
			// backward. x : [T,d] f32. Renvoie [T,d] (invalide si erreur de forme).
			NkTensor NkQwen2LayerForwardTrain(const NkQwen2Config &cfg, const NkQwen2LayerWeights &w,
											  const NkQwen2LoraSet &lora, const NkTensor &x, NkQwen2TrainSaved &saved);

			// Backward de la couche : depuis dOut [T,d] et les activations `saved`
			// du DERNIER NkQwen2LayerForwardTrain, produit dX [T,d] (gradient qui
			// traverse) et accumule les dA/dB des adaptateurs présents dans `grads`.
			// Les poids du socle ne reçoivent AUCUN gradient (gelés).
			bool NkQwen2LayerBackward(const NkQwen2Config &cfg, const NkQwen2LayerWeights &w, const NkQwen2LoraSet &lora,
									  const NkQwen2TrainSaved &saved, const NkTensor &dOut, NkTensor &dX,
									  NkQwen2LoraSetGrads &grads);

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
