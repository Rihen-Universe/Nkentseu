// =============================================================================
// NkDense.cpp — implémentation de la couche dense (NKAI, Phase 2).
// =============================================================================
#include "NKNN/NkDense.h"
#include "NKTensor/NkTensor.h"
#include "NKTensor/NkTensorOps.h"

#include <cmath>

namespace nkentseu {
	namespace ai {
		namespace nn {

			NkDense::NkDense(uint32 inFeatures, uint32 outFeatures, uint32 seed) {
				NkTensor W = NkTensor::Zeros(NkShape{(int64)inFeatures, (int64)outFeatures});
				NkTensor b = NkTensor::Zeros(NkShape{(int64)1, (int64)outFeatures});

				// Init Xavier-uniforme : W ~ U(-r, r), r = sqrt(6/(in+out)). LCG
				// déterministe (graine) pour des runs reproductibles.
				double r = 1.0;
				if (inFeatures + outFeatures > 0)
					r = std::sqrt(6.0 / (double)(inFeatures + outFeatures));
				uint32 s = seed ? seed : 1u;
				float *wp = W.DataAs<float>();
				const int64 n = NkShapeNumel(W.Shape());
				for (int64 i = 0; i < n; ++i) {
					s = s * 1664525u + 1013904223u;
					double u = (double)((s >> 8) & 0xFFFFu) / 65535.0 * 2.0 - 1.0; // [-1,1]
					wp[i] = (float)(u * r);
				}

				mW = NkVar::Leaf(W, true);
				mB = NkVar::Leaf(b, true);
			}

			NkVar NkDense::Forward(const NkVar &x) const {
				return autograd::Add(autograd::Matmul(x, mW), mB);
			}

			void NkDense::Parameters(NkVector<NkVar> &out) const {
				out.PushBack(mW);
				out.PushBack(mB);
			}

		} // namespace nn
	} // namespace ai
} // namespace nkentseu
