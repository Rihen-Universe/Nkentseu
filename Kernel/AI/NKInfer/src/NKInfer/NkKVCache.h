// =============================================================================
// NkKVCache.h — cache clé/valeur pour la génération token-par-token (NKInfer,
// Jalon 3 : « Génération token par token + KV-cache »).
//
// Sans cache, générer T tokens en autorégressif coûte O(T²) : chaque nouveau
// token recalcule les projections K/V de TOUS les tokens précédents. Le cache
// stocke les K/V (après RoPE pour K) déjà calculés couche par couche et les
// étend en place à chaque pas -> O(T) amorti (un seul appel de projection K/V
// par nouveau token, l'attention reste O(T) par tête à cause de la longueur
// croissante des clés, mais rien n'est RECALCULÉ).
//
// Forme : [nKVHeads, Tpast, headDim] (têtes KV — PAS têtes Q : voir GQA dans
// NkQwen2Block.h). Un layer de cache par couche du modèle.
//
// Zéro STL : NkVector/NkTensor uniquement.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKTensor/NkTensor.h"

namespace nkentseu {
	namespace ai {
		namespace infer {

			// Cache K/V d'UNE seule couche. Vide (k/v invalides) tant qu'aucun
			// token n'a été traité par cette couche.
			struct NkKVCacheLayer {
					NkTensor k; // [nKVHeads, Tpast, headDim], f32 contigu
					NkTensor v; // idem

					// Longueur courante (Tpast) du cache. 0 si vide.
					int32 Length() const {
						return k.IsValid() ? (int32)k.Shape()[1] : 0;
					}
			};

			// Étend `layer` EN PLACE avec de nouvelles clés/valeurs `newK`/`newV`
			// (chacune [nKVHeads, Tnew, headDim], f32 contigu) : concatène sur
			// l'axe TEMPS (1). Si `layer` est vide, `newK`/`newV` deviennent le
			// cache initial (copie profonde : le cache ne doit pas partager le
			// stockage d'un tenseur temporaire du forward).
			void NkKVCacheAppend(NkKVCacheLayer &layer, const NkTensor &newK, const NkTensor &newV);

			// Cache multi-couches : un NkKVCacheLayer par couche du modèle.
			struct NkKVCache {
					NkVector<NkKVCacheLayer> layers;

					// Réinitialise pour `nLayers` couches (tout vide, longueur 0).
					void Reset(uint32 nLayers) {
						layers.Clear();
						layers.Resize((NkVector<NkKVCacheLayer>::SizeType)nLayers);
					}

					// Longueur courante (celle de la couche 0 ; toutes les couches
					// avancent ensemble dans ce design). 0 si non initialisé.
					int32 Length() const {
						return layers.Size() > 0 ? layers[0].Length() : 0;
					}
			};

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
