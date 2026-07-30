// =============================================================================
// NkKVCache.cpp — voir NkKVCache.h.
// =============================================================================
#include "NKInfer/NkKVCache.h"

#include <cstring> // memcpy

namespace nkentseu {
	namespace ai {
		namespace infer {

			// Concatène a[H,Ta,D] et b[H,Tb,D] sur l'axe TEMPS (1) -> [H,Ta+Tb,D].
			// Motif repris de NKNN/NkTransformer.h (NkMultiHeadAttention::CatTimeAxis),
			// adapté ici à un cache SANS dimension de lot explicite (les têtes KV
			// jouent le rôle de la dimension de tête dans ce fichier-là).
			static NkTensor CatTimeAxis(const NkTensor &a, const NkTensor &b) {
				NkTensor ca = a.IsContiguous() ? a : a.Contiguous();
				NkTensor cb = b.IsContiguous() ? b : b.Contiguous();
				const NkShape &sa = ca.Shape();
				const NkShape &sb = cb.Shape();
				const int64 H = sa[0], Ta = sa[1], D = sa[2], Tb = sb[1];
				NkTensor out = NkTensor::Zeros(NkShape{H, Ta + Tb, D});
				const float *pa = ca.DataAs<float>();
				const float *pb = cb.DataAs<float>();
				float *po = out.DataAs<float>();
				for (int64 h = 0; h < H; ++h) {
					const int64 obase = h * (Ta + Tb) * D;
					const int64 abase = h * Ta * D;
					const int64 bbase = h * Tb * D;
					std::memcpy(po + obase, pa + abase, (usize)(Ta * D) * sizeof(float));
					std::memcpy(po + obase + Ta * D, pb + bbase, (usize)(Tb * D) * sizeof(float));
				}
				return out;
			}

			void NkKVCacheAppend(NkKVCacheLayer &layer, const NkTensor &newK, const NkTensor &newV) {
				if (!layer.k.IsValid()) {
					// Cache vide : le nouveau K/V devient le cache initial. Clone()
					// pour ne PAS partager le stockage d'un tenseur temporaire du
					// forward (qui pourrait être réutilisé/écrasé par l'appelant).
					layer.k = newK.Clone();
					layer.v = newV.Clone();
					return;
				}
				layer.k = CatTimeAxis(layer.k, newK);
				layer.v = CatTimeAxis(layer.v, newV);
			}

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
