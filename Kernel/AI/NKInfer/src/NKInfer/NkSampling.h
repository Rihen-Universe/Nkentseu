// =============================================================================
// NkSampling.h — stratégies d'échantillonnage pour la génération de texte
// (NKInfer, Jalon 3 : « Stratégies d'échantillonnage (greedy, top-k, température) »).
//
// Deux stratégies sur un vecteur de logits 1D (ou [1,V]) :
//   - NkSampleGreedy   : argmax déterministe (toujours le même résultat).
//   - NkSampleTopK     : température + restriction top-k + tirage stochastique
//                        avec graine déterministe (reproductible : même graine
//                        -> même séquence de tokens générés).
//
// Générateur pseudo-aléatoire : LCG (Linear Congruential Generator) — MÊME
// schéma que celui déjà utilisé dans le dépôt pour l'initialisation déterministe
// des poids (NKNN/src/NKNN/NkTransformer.h, fonction RandnTensor) :
//   state = state * 1664525u + 1013904223u   (constantes Numerical Recipes)
//   u = ((state >> 8) & 0xFFFFFF) / 16777216.0   -> uniforme dans [0,1)
// Réutilisé ici tel quel (pas réinventé) pour rester cohérent avec le reste du
// code base et garantir la reproductibilité attendue par la mission.
//
// Zéro STL : NkTensor uniquement en entrée.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKTensor/NkTensor.h"

namespace nkentseu {
	namespace ai {
		namespace infer {

			// Argmax déterministe (glouton) sur des logits 1D (ou [1,V] — la
			// première ligne est utilisée). Renvoie -1 si `logits` est invalide.
			int32 NkSampleGreedy(const NkTensor &logits);

			// Échantillonnage stochastique reproductible :
			//   1. division par `temperature` (doit être > 0 ; 1.0 = neutre)
			//   2. restriction aux `topK` logits les plus élevés (topK <= 0 ou
			//      >= taille du vocabulaire -> pas de restriction)
			//   3. softmax sur les logits restants
			//   4. tirage suivant cette distribution catégorielle, via le LCG
			//      déterministe décrit ci-dessus, dont l'état est `rngState`
			//      (modifié EN PLACE : des appels successifs consomment le flux
			//      pseudo-aléatoire ; repartir de la MÊME graine initiale reproduit
			//      EXACTEMENT la même séquence de tokens).
			// Renvoie -1 si `logits` est invalide.
			int32 NkSampleTopK(const NkTensor &logits, float32 temperature, int32 topK, uint32 &rngState);

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
