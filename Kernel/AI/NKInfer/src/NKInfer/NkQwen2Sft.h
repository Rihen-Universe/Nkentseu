// =============================================================================
// NkQwen2Sft.h — assemblage d'un MODÈLE Qwen2 multi-couches entraînable côté
// LoRA + boucle SFT (NKInfer, chantier QLoRA, Jalon 3 — cf Documentation/
// notes_etape4_qlora.md §2.5 et §4, et D:\Projets\Camrail\AI\
// 03_LOSS_MASKING_PATCH.md pour le masquage de perte décidé par Rihen).
//
// CE QUE CE MODULE ASSEMBLE (au-dessus des jalons 1 et 2, INTACTS) :
//   - embedding GELÉ : simple lookup de lignes dans une matrice [V,d] — aucun
//     gradient n'y redescend (le socle entier est gelé, seuls les adaptateurs
//     LoRA apprennent) ;
//   - N couches chaînées via NkQwen2LayerForwardTrain / NkQwen2LayerBackward
//     (jalon 2, 35/35 au gradient numérique) : le dX produit par le backward de
//     la couche k+1 nourrit le backward de la couche k — c'est LA nouveauté du
//     jalon 3, la traversée verticale complète ;
//   - RMSNorm final + lm_head GELÉS : le backward du lm_head ne produit que
//     dX = dLogits·W (pas de dW — socle gelé), celui du RMSNorm final réutilise
//     NkRMSNormBackward ;
//   - perte CROSS-ENTROPIE MASQUÉE : sur une paire Question/Réponse, la perte
//     ne porte QUE sur la réponse (et son token de fin <|im_end|>), JAMAIS sur
//     le prompt. POURQUOI le masquage doit vivre dans la perte elle-même (et
//     pas dans un one-hot mis à zéro) : dLogits = softmax − onehot ; avec un
//     one-hot nul le gradient vaudrait softmax ≠ 0 et pousserait les positions
//     du prompt vers l'uniforme — c'est le piège documenté dans le patch
//     03_LOSS_MASKING de Camrail. Ici : les lignes masquées ont un gradient
//     STRUCTURELLEMENT nul et la normalisation se fait par le nombre de lignes
//     ACTIVES, pas par T ;
//   - pas Adam sur les adaptateurs SEULS : Adam (Kingma & Ba, 2014, mêmes
//     formules que optim::NkAdam) ré-implémenté ICI sur les seuls tenseurs
//     A/B. POURQUOI pas optim::NkAdam directement : NKInfer ne dépend PAS de
//     NKOptim, et on ne modifie pas les dépendances d'un module existant pour
//     un jalon additif — même méthode que NkQwen2I64Map vs data::NkI64Map au
//     jalon 1. Le gel du socle est STRUCTUREL (l'optimiseur ne voit que les
//     adaptateurs), pas un masque de gradient ;
//   - formatage ChatML d'une paire Q/R avec les tokens spéciaux du vocabulaire
//     (<|im_start|>/<|im_end|>, résolus par le tokenizer du jalon 1), qui
//     produit AUSSI le masque de perte token par token.
//
// PORTÉE HONNÊTE : B(atch)=1 par séquence (comme tout NKInfer), CPU pur (aucun
// dispatch GPU — les produits passent par NkLinearNoBias/NkCpuMatmul*, jamais
// ops::Matmul et sa bascule GPU automatique, piège n°2 des notes), séquence
// complète depuis la position 0 (pas de KV-cache : mécanisme d'inférence).
// L'accumulation multi-exemples du trainer est séquentielle (un exemple à la
// fois), la moyenne des gradients est faite au Step().
//
// Zéro STL : NkString/NkVector/NKMemory uniquement. Namespace ai::infer.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKTensor/NkTensor.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKInfer/NkQwen2Backward.h"
#include "NKInfer/NkQwen2Tokenizer.h"

namespace nkentseu {
	namespace ai {
		namespace infer {

			// ---- Modèle multi-couches ---------------------------------------------
			// Tout le socle (embedding, couches, norme finale, lm_head) est GELÉ ;
			// seuls les NkLoraSet par couche portent des paramètres entraînables.
			// Convention lm_head : [V, d] (poids [out,in], comme les projections —
			// les logits se calculent par NkLinearNoBias, sans transposée
			// matérialisée, piège n°4 des notes).
			struct NkQwen2SftModel {
					NkQwen2Config cfg;
					NkTensor embedding;					  // [V, d] gelé (lookup de lignes)
					NkVector<NkQwen2LayerWeights> layers; // socle gelé, une entrée par couche
					NkVector<NkQwen2LoraSet> lora;		  // adaptateurs (même taille que layers)
					NkTensor finalNorm;					  // [d] gelé (RMSNorm de sortie)
					NkTensor lmHead;					  // [V, d] gelé

					int64 NumLayers() const {
						return (int64)layers.Size();
					}
					int64 VocabSize() const {
						return embedding.IsValid() ? embedding.Shape()[0] : 0;
					}
					// Cohérence structurelle (formes des couches vérifiées par
					// NkQwen2LayerWeights::IsValid, embedding/lm_head en [V,d]).
					bool IsValid() const;
			};

			// Activations sauvegardées du forward complet — une NkQwen2TrainSaved
			// par couche (le minimum du jalon 2) + l'entrée du RMSNorm final.
			// POURQUOI pas la sortie normée ni les logits : le lm_head est gelé
			// (pas de dW donc pas besoin de son entrée), et les logits sont
			// consommés immédiatement par la perte.
			struct NkQwen2SftSaved {
					NkVector<NkQwen2TrainSaved> layers;
					NkTensor xFinal; // [T,d] sortie de la dernière couche = entrée du RMSNorm final
			};

			// Lookup d'embedding GELÉ : ids -> [T,d] (copie de lignes, jamais une
			// vue — piège n°1 : le résultat doit survivre indépendamment).
			// Tenseur invalide si un id sort de [0,V).
			NkTensor NkQwen2SftEmbedLookup(const NkTensor &embedding, const NkVector<int32> &ids);

			// Forward complet : ids -> logits [T,V].
			// embedding (gelé) -> N × NkQwen2LayerForwardTrain (LoRA inclus) ->
			// RMSNorm final (gelé) -> lm_head (gelé, NkLinearNoBias).
			// `saved` reçoit les activations nécessaires au backward.
			NkTensor NkQwen2SftForward(const NkQwen2SftModel &m, const NkVector<int32> &ids, NkQwen2SftSaved &saved);

			// Perte cross-entropie MASQUÉE (moyenne sur les positions ACTIVES).
			//   logits  : [T,V] f32 ;
			//   targets : T ids cibles (position t prédit targets[t]) ;
			//   mask    : T poids 0/1 — 0 = position de PROMPT (ignorée forward ET
			//             backward), 1 = position de RÉPONSE (compte).
			// Si dLogits est non nul, y écrit [T,V] : (softmax − onehot)/nActives
			// sur les lignes actives, 0 EXACTEMENT sur les lignes masquées (cf
			// en-tête : c'est la seule façon d'ignorer vraiment le prompt).
			// Si outActive est non nul, y écrit le nombre de lignes actives.
			// Renvoie la perte, ou un négatif (-1.0) si entrées incohérentes.
			// Softmax et somme en float64 : la perte sert de référence aux tests
			// par différences finies, son bruit doit rester sous celui du forward.
			double NkQwen2SftMaskedCE(const NkTensor &logits, const NkVector<int32> &targets,
									  const NkVector<float32> &mask, NkTensor *dLogits, int64 *outActive);

			// Backward complet : dLogits [T,V] -> dA/dB de TOUTES les couches.
			// lm_head gelé : dXn = dLogits·W ; RMSNorm final gelé : NkRMSNorm-
			// Backward ; puis les couches en ordre INVERSE, le dX de la couche
			// k+1 nourrissant le backward de la couche k. Le dX sous la couche 0
			// est ABANDONNÉ (l'embedding est gelé, rien à apprendre en dessous).
			// `grads` est redimensionné à NumLayers() si vide ; les gradients
			// s'ACCUMULENT (+=, sémantique NkLoraGrad) — l'accumulation
			// multi-exemples ne demande donc aucun code supplémentaire.
			bool NkQwen2SftBackward(const NkQwen2SftModel &m, const NkQwen2SftSaved &saved, const NkTensor &dLogits,
									NkVector<NkQwen2LoraSetGrads> &grads);

			// ---- Formatage ChatML d'une paire Q/R ---------------------------------
			// Exemple d'entraînement : la séquence de tokens ET son masque de
			// perte PAR TOKEN (1 = ce token appartient à la réponse, y compris le
			// <|im_end|> final ; 0 = prompt/gabarit). Le décalage entrée/cible
			// (inputs = tokens[0..n-2], cibles = tokens[1..n-1], masque de la
			// cible t = lossMask[t+1]) est fait par le trainer, pas ici : le
			// masque décrit les TOKENS, propriété du formatage, pas de la boucle.
			struct NkQwen2SftExample {
					NkVector<int32> tokens;
					NkVector<float32> lossMask;
			};

			// Formate « <|im_start|>user\n{Q}<|im_end|>\n<|im_start|>assistant\n
			// {R}<|im_end|> » avec les ids RÉELS du vocabulaire (EncodeWithSpecials
			// du jalon 1 — les spéciaux sont émis atomiques, jamais morcelés par le
			// BPE). Masque : 0 sur tout le gabarit + question, 1 sur les tokens de
			// la réponse ET le <|im_end|> qui la clôt (le modèle doit apprendre à
			// FINIR — c'est « le token de fin » du patch 03_LOSS_MASKING).
			// false + *err si le tokenizer n'a pas <|im_start|>/<|im_end|> ou si
			// un encodage échoue.
			bool NkQwen2SftFormatChatML(const NkQwen2Tokenizer &tok, const NkString &question, const NkString &answer,
										NkQwen2SftExample &out, NkString *err);

			// ---- Boucle SFT (Adam sur les adaptateurs SEULS) ----------------------
			// Cycle : AccumulateExample() × k -> Step() -> ... Les gradients de
			// chaque exemple s'accumulent ; Step() moyenne par le nombre
			// d'exemples accumulés (la perte rapportée est déjà une moyenne par
			// exemple : gradient et perte restent cohérents), applique un pas
			// Adam aux seuls tenseurs A/B (mise à jour EN PLACE : le prochain
			// forward lit les poids à jour via les vues partagées des paires),
			// puis remet l'accumulation à zéro. Formules Adam (Kingma & Ba,
			// 2014, identiques à optim::NkAdam) :
			//   m ← β1·m + (1−β1)·g ;  v ← β2·v + (1−β2)·g²
			//   p ← p − lr·(m/(1−β1ᵗ)) / (√(v/(1−β2ᵗ)) + ε)
			class NkQwen2SftTrainer {
				public:
					// `model` doit survivre au trainer (les paires LoRA sont
					// modifiées EN PLACE à chaque Step). Enregistre les tenseurs
					// A/B de chaque paire VALIDE. false si le modèle est invalide
					// ou sans adaptateur.
					bool Init(NkQwen2SftModel *model, float32 lr);

					// forward + CE masquée + backward (accumulation). Renvoie la
					// perte de l'exemple (négatif si erreur).
					double AccumulateExample(const NkQwen2SftExample &ex);

					// Perte seule (aucun gradient, aucune accumulation) — pour la
					// validation. Négatif si erreur.
					double EvaluateExample(const NkQwen2SftExample &ex) const;

					// Un pas Adam sur les gradients accumulés (moyennés, clip
					// global optionnel). false si rien n'a été accumulé.
					bool Step();

					int64 AccumulatedCount() const {
						return mAccumCount;
					}

					float32 LearningRate() const {
						return mLr;
					}

					void SetLearningRate(float32 lr) {
						mLr = lr;
					}

					// Clipping par norme L2 GLOBALE (tous adaptateurs confondus,
					// même sémantique que optim::NK_CLIP_BY_GLOBAL_NORM :
					// préserve la DIRECTION du gradient). 0 = désactivé (défaut).
					void SetGradClipGlobalNorm(float32 clip) {
						mClipNorm = clip;
					}

				private:
					// Décompose un exemple en (inputs, cibles, masque de cibles)
					// décalés d'un token. false si l'exemple est trop court (< 2)
					// ou si aucune cible n'est active.
					bool SplitExample(const NkQwen2SftExample &ex, NkVector<int32> &inputs, NkVector<int32> &targets,
									  NkVector<float32> &mask) const;

					NkQwen2SftModel *mModel = nullptr;
					NkVector<NkLoraPair *> mPairPtrs;	  // paires valides (ordre : couche, puis q,k,v,o,gate,up,down)
					NkVector<NkQwen2LoraSetGrads> mGrads; // accumulation (une entrée par couche)
					NkVector<NkTensor> mM;				  // 1er moment Adam (2 tenseurs par paire : A puis B)
					NkVector<NkTensor> mV;				  // 2e moment Adam
					int64 mAccumCount = 0;
					int64 mT = 0; // compteur de pas (correction de biais)
					float32 mLr = 0.001f;
					float32 mB1 = 0.9f;
					float32 mB2 = 0.999f;
					float32 mEps = 1e-8f;
					float32 mClipNorm = 0.0f;
			};

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
