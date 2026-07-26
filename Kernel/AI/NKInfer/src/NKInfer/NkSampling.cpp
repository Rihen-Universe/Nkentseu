// =============================================================================
// NkSampling.cpp — voir NkSampling.h.
// =============================================================================
#include "NKInfer/NkSampling.h"
#include "NKContainers/Sequential/NkVector.h"

#include <cstdio>
#include <cmath>

namespace nkentseu {
	namespace ai {
		namespace infer {

			namespace {
				// Vue brute [V] sur `logits`, qui peut être [V] ou [1,V]. Renvoie
				// nullptr si la forme ne correspond à aucun des deux cas.
				const float *AsFlatLogits(const NkTensor &logits, int64 &outV) {
					if (!logits.IsValid() || logits.DType() != NkDType::NK_F32)
						return nullptr;
					if (logits.Rank() == 1) {
						outV = logits.Shape()[0];
					} else if (logits.Rank() == 2 && logits.Shape()[0] == 1) {
						outV = logits.Shape()[1];
					} else {
						return nullptr;
					}
					// Assure la contigüité (nécessite un stockage temporaire si
					// besoin -> on matérialise via Contiguous() dans l'appelant si
					// jamais on a un doute ; ici on suppose l'entrée déjà contiguë,
					// ce qui est toujours le cas pour des logits fraîchement calculés.
					if (!logits.IsContiguous())
						return nullptr;
					return logits.DataAs<float>();
				}

				// Une seule tirage uniforme [0,1) depuis l'état LCG (avancé en place).
				// Mêmes constantes que NKNN/NkTransformer.h::RandnTensor (cf NkSampling.h).
				double NextUniform(uint32 &state) {
					state = state * 1664525u + 1013904223u;
					return (double)((state >> 8) & 0xFFFFFFu) / 16777216.0;
				}
			} // namespace

			int32 NkSampleGreedy(const NkTensor &logits) {
				int64 V = 0;
				NkTensor cont = logits.IsValid() && !logits.IsContiguous() ? logits.Contiguous() : logits;
				const float *p = AsFlatLogits(cont, V);
				if (!p || V <= 0) {
					fprintf(stderr, "[NkSampling] NkSampleGreedy : logits invalides\n");
					return -1;
				}
				int64 best = 0;
				float bestVal = p[0];
				for (int64 i = 1; i < V; ++i) {
					if (p[i] > bestVal) {
						bestVal = p[i];
						best = i;
					}
				}
				return (int32)best;
			}

			int32 NkSampleTopK(const NkTensor &logits, float32 temperature, int32 topK, uint32 &rngState) {
				int64 V = 0;
				NkTensor cont = logits.IsValid() && !logits.IsContiguous() ? logits.Contiguous() : logits;
				const float *p = AsFlatLogits(cont, V);
				if (!p || V <= 0) {
					fprintf(stderr, "[NkSampling] NkSampleTopK : logits invalides\n");
					return -1;
				}
				if (temperature <= 0.0f)
					temperature = 1.0f;

				const int64 k = (topK <= 0 || (int64)topK >= V) ? V : (int64)topK;

				// Sélectionne les `k` indices de plus haut logit (sélection simple,
				// O(V·k) — largement suffisant pour k <= quelques centaines).
				NkVector<int64> idx;
				NkVector<uint8> used; // 0/1 (évite NkVector<bool>, jamais utilisé ailleurs dans ce dépôt)
				idx.Reserve((NkVector<int64>::SizeType)k);
				used.Resize((NkVector<uint8>::SizeType)V);
				for (int64 i = 0; i < V; ++i)
					used[i] = 0;
				for (int64 s = 0; s < k; ++s) {
					int64 best = -1;
					float bestVal = 0.0f;
					for (int64 i = 0; i < V; ++i) {
						if (used[i])
							continue;
						if (best < 0 || p[i] > bestVal) {
							best = i;
							bestVal = p[i];
						}
					}
					if (best < 0)
						break;
					used[best] = 1;
					idx.PushBack(best);
				}

				// Softmax (température) sur les k logits sélectionnés.
				NkVector<double> probs;
				probs.Reserve((NkVector<double>::SizeType)idx.Size());
				double mx = -1e300;
				for (uint32 i = 0; i < idx.Size(); ++i) {
					const double v = (double)p[idx[i]] / (double)temperature;
					if (v > mx)
						mx = v;
				}
				double sum = 0.0;
				for (uint32 i = 0; i < idx.Size(); ++i) {
					const double v = (double)p[idx[i]] / (double)temperature;
					const double e = std::exp(v - mx);
					probs.PushBack(e);
					sum += e;
				}
				if (sum <= 0.0)
					return idx.Size() > 0 ? (int32)idx[0] : -1;

				const double u = NextUniform(rngState) * sum;
				double cum = 0.0;
				for (uint32 i = 0; i < idx.Size(); ++i) {
					cum += probs[i];
					if (u <= cum)
						return (int32)idx[i];
				}
				return (int32)idx[idx.Size() - 1]; // filet de sécurité (arrondi flottant)
			}

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
