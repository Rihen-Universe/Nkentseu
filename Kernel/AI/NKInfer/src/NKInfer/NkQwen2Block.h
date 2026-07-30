// =============================================================================
// NkQwen2Block.h — bloc transformer Qwen2/Qwen2.5 (NKInfer, Jalon 3 : « Exécuter
// une architecture transformer »).
//
// Implémente, sur NkTensor CPU brut (PAS d'autograd : inférence pure, aucun
// graphe/gradient nécessaire) :
//   - RMSNorm (pré-normalisation, sans recentrage)
//   - Attention causale avec GQA (Grouped-Query Attention : nHeads têtes de
//     requête, nKVHeads < nHeads têtes clé/valeur PARTAGÉES par groupes)
//   - RoPE (Rotary Position Embedding), style « NEOX » (moitiés) — PAS le style
//     GPT-J (paires adjacentes)
//   - MLP SwiGLU (down(silu(gate(x)) ⊙ up(x)))
// avec biais sur les projections Q/K/V (PAS sur la sortie d'attention ni le MLP).
//
// ARCHITECTURE EXACTE ET SOURCES (vérifiées le 2026-07-25, comme NkGGUFDequant) :
//   - Qwen2 Technical Report, Yang et al., arXiv:2407.10671 (section architecture) :
//     confirme RMSNorm + pré-normalisation, RoPE, GQA, SwiGLU, et « QKV bias »
//     explicitement ajouté sur les projections d'attention (biais retiré partout
//     ailleurs, y compris la projection de sortie attn_output et tout le MLP).
//   - llama.cpp (ggml-org/llama.cpp), fonction de construction du graphe Qwen2
//     (`src/models/qwen2.cpp` / historiquement `llm_build_qwen2` dans
//     `llama.cpp`), consultée via WebFetch le 2026-07-25 : confirme l'ordre exact
//     par couche (attn_norm -> Q/K/V+bias -> RoPE -> attention GQA (échelle
//     1/sqrt(headDim)) -> wo SANS biais -> résiduel -> ffn_norm -> SwiGLU
//     (ffn_gate/ffn_up/ffn_down, activation SILU, combinaison « parallèle » —
//     gate et up calculés indépendamment puis combinés) -> résiduel).
//   - Style RoPE « NEOX » (rotation par MOITIÉS : paire (i, i+headDim/2), PAS
//     paire adjacente (2i,2i+1) façon GPT-J) : confirmé pour la famille
//     Qwen (2/2.5/3) et pour LLaMA dans llama.cpp (constante
//     `LLAMA_ROPE_TYPE_NEOX`), et par HuggingFace `transformers`
//     (`modeling_qwen2.py`, fonctions `rotate_half`/`apply_rotary_pos_emb` —
//     même formule que Llama). Formule reproduite dans NkApplyRoPE ci-dessous.
//   - RMSNorm : Zhang & Sennrich, « Root Mean Square Layer Normalization »,
//     arXiv:1910.07467 — formule y = x/sqrt(mean(x²)+eps)·weight (sans
//     recentrage ni biais additif, contrairement à LayerNorm classique).
//
// theta (freq_base) et eps (RMS epsilon) ne sont JAMAIS supposés : lus depuis
// les métadonnées RÉELLES du GGUF (`qwen2.rope.freq_base`,
// `qwen2.attention.layer_norm_rms_epsilon`) par l'appelant (cf Applications/
// NKLLMInferTest), qui remplit NkQwen2Config en conséquence.
//
// Portée honnête : B (batch) = 1 implicite partout (un seul « fil » de
// génération à la fois — suffisant pour prouver la correction de l'architecture
// et de la génération incrémentale ; le batching réel resterait à ajouter).
// Zéro STL, zéro dépendance GPU (toutes les opérations matricielles de ce
// fichier sont un produit matriciel CPU écrit à la main — PAS ops::Matmul, dont
// le dispatch GPU automatique pour les grandes matrices serait ici indésirable :
// exigence stricte « CPU-only » de ce jalon, cf NKInfer/ROADMAP.md).
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKTensor/NkTensor.h"
#include "NKInfer/NkKVCache.h"

namespace nkentseu {
	namespace ai {
		namespace infer {

			// Hyperparamètres d'un modèle Qwen2/Qwen2.5, lus depuis les métadonnées
			// GGUF réelles (voir NkGGUFLoader/NkGGUFGetUInt/NkGGUFGetFloat) — AUCUNE
			// valeur ici n'est une hypothèse : l'appelant doit les renseigner depuis
			// le fichier réel avant d'appeler NkQwen2LayerForward.
			struct NkQwen2Config {
					int32 dModel = 0;			   // qwen2.embedding_length
					int32 nHeads = 0;			   // qwen2.attention.head_count
					int32 nKVHeads = 0;			   // qwen2.attention.head_count_kv (GQA : nKVHeads <= nHeads)
					int32 headDim = 0;			   // dModel / nHeads (Qwen2.5 7B : 3584/28 = 128)
					int32 ffnDim = 0;			   // qwen2.feed_forward_length
					float32 ropeFreqBase = 0.0f;   // qwen2.rope.freq_base (PAS de défaut arbitraire : 0 = invalide)
					float32 rmsEps = 0.0f;		   // qwen2.attention.layer_norm_rms_epsilon

					bool IsValid() const {
						return dModel > 0 && nHeads > 0 && nKVHeads > 0 && nKVHeads <= nHeads &&
							   (nHeads % nKVHeads) == 0 && headDim > 0 && (headDim % 2) == 0 && ffnDim > 0 &&
							   ropeFreqBase > 0.0f && rmsEps > 0.0f;
					}
			};

			// Poids d'UNE couche Qwen2, déjà déquantifiés en f32 (via NkGGUFDequant).
			// Convention des matrices : [out_features, in_features] (mémoire ggml
			// inversée par NkGGUFDequantizeTensor -> convention "poids PyTorch/GGUF"
			// standard). Utiliser NkQwen2LayerWeights::IsValid(cfg) pour vérifier la
			// cohérence des formes avant Forward.
			struct NkQwen2LayerWeights {
					NkTensor attnNorm; // [d]
					NkTensor wq, bq;   // [nHeads*headDim, d] ; [nHeads*headDim]
					NkTensor wk, bk;   // [nKVHeads*headDim, d] ; [nKVHeads*headDim]
					NkTensor wv, bv;   // [nKVHeads*headDim, d] ; [nKVHeads*headDim]
					NkTensor wo;	   // [d, nHeads*headDim] — PAS de biais (cf sources en tête de fichier)
					NkTensor ffnNorm;  // [d]
					NkTensor wGate;	   // [ffnDim, d]
					NkTensor wUp;	   // [ffnDim, d]
					NkTensor wDown;	   // [d, ffnDim]

					// Vérifie que tous les tenseurs requis sont valides et ont EXACTEMENT
					// les formes attendues pour `cfg`. Ne vérifie PAS les valeurs.
					bool IsValid(const NkQwen2Config &cfg) const;
			};

			// RMSNorm : y = x / sqrt(mean_dernier_axe(x²) + eps) · weight (par ligne,
			// sur le DERNIER axe). x : [T,d] ou [d] ; weight : [d]. Voir sources en
			// tête de fichier. Renvoie un tenseur invalide si les formes/dtype ne
			// correspondent pas (dtype F32 requis).
			NkTensor NkRMSNorm(const NkTensor &x, const NkTensor &weight, float32 eps);

			// SiLU/Swish élémentaire : silu(x) = x·sigmoid(x). Utilisé par le MLP
			// SwiGLU (cf sources en tête de fichier). F32 requis.
			NkTensor NkSiLU(const NkTensor &x);

			// x[T,K] · W[N,K]^T -> [T,N] (projection linéaire SANS biais, convention
			// poids [out_features,in_features]). Calculé en accédant `x` ET `W` ligne
			// par ligne (produit scalaire direct par ligne de sortie) : évite de
			// matérialiser la transposée de `W` via NkTensor::Contiguous() générique
			// (coût prohibitif élément-par-élément pour de grandes matrices — ex. la
			// projection de sortie lm_head Qwen2.5 7B, 152064 lignes). Utilisée pour
			// TOUTES les grandes projections de ce fichier (Q/K/V/O, gate/up/down) et
			// réutilisable telle quelle par l'appelant pour son propre lm_head.
			NkTensor NkLinearNoBias(const NkTensor &x, const NkTensor &w);

			// Applique le RoPE EN PLACE sur `x` ([nHeadsLocal, T, headDim], f32
			// contigu — nHeadsLocal = nHeads pour Q, nKVHeads pour K). Pour la tête h,
			// la position t (0-indexée dans `x`), la position ABSOLUE est
			// `posOffset + t` ; pour chaque paire d'indices (i, i+headDim/2) avec
			// i in [0, headDim/2) :
			//   theta_i = (posOffset+t) · freqBase^(-2i/headDim)
			//   x[h,t,i]            = x1·cos(theta_i) − x2·sin(theta_i)
			//   x[h,t,i+headDim/2]  = x1·sin(theta_i) + x2·cos(theta_i)
			// (x1,x2 = valeurs AVANT rotation). Voir sources en tête de fichier pour
			// la justification du style « NEOX » (moitiés, pas paires adjacentes).
			void NkApplyRoPE(NkTensor &x, int32 posOffset, float32 freqBase);

			// Forward complet d'UNE couche Qwen2 (pré-norme, résiduels, GQA, RoPE,
			// SwiGLU). `x` : [T,d] (T >= 1, un seul "batch" implicite). `cache` : K/V
			// de CETTE couche uniquement, étendus EN PLACE ; la position absolue du
			// premier token de `x` est `cache.Length()` AVANT l'appel (permet aussi
			// bien le "prefill" T>1 depuis un cache vide que la génération pas-à-pas
			// T=1 depuis un cache déjà rempli — mathématiquement équivalent à un
			// forward complet sur toute la séquence, cf test de cohérence dans
			// Applications/NKLLMInferTest). Renvoie [T,d], ou un tenseur invalide en
			// cas d'erreur de forme (message sur stderr, comme NkTensorOps).
			NkTensor NkQwen2LayerForward(const NkQwen2Config &cfg, const NkQwen2LayerWeights &w, const NkTensor &x,
										 NkKVCacheLayer &cache);

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
