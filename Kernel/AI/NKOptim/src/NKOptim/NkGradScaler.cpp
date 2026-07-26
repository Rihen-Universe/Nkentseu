// =============================================================================
// NkGradScaler.cpp — implémentation du loss scaling dynamique (NKAI, mixed precision).
// =============================================================================
#include "NKOptim/NkGradScaler.h"
#include "NKTensor/NkTensorOps.h"
#include "NKTensor/NkDType.h"

#include <cmath>

namespace nkentseu {
	namespace ai {
		namespace optim {

			NkGradScaler::NkGradScaler(float initScale, float growthFactor, float backoffFactor,
									   int32 growthInterval, float minScale)
				: mScale(initScale), mGrowthFactor(growthFactor), mBackoffFactor(backoffFactor),
				  mGrowthInterval(growthInterval), mMinScale(minScale) {
			}

			NkVar NkGradScaler::Scale(const NkVar &loss) const {
				return autograd::MulScalar(loss, (double)mScale);
			}

			bool NkGradScaler::HasInfNan(const NkTensor &t) {
				if (!t.IsValid() || t.Numel() <= 0)
					return false;
				NkTensor c = (t.Device() == NkDevice::NK_GPU) ? t.ToCPU() : t;
				if (!c.IsValid())
					return false;
				c = c.IsContiguous() ? c : c.Contiguous();
				const int64 n = c.Numel();
				bool bad = false;
				NK_DTYPE_DISPATCH_FLOAT(c.DType(), T, {
					const T *p = c.DataAs<T>();
					for (int64 i = 0; i < n; ++i) {
						const double v = (double)p[i];
						if (std::isinf(v) || std::isnan(v)) {
							bad = true;
							break;
						}
					}
				});
				return bad;
			}

			bool NkGradScaler::Unscale(const NkVector<NkVar> &params) {
				bool overflow = false;
				for (uint32 i = 0; i < params.Size() && !overflow; ++i) {
					NkVarNode *node = params[i].Node();
					if (!node || !node->grad.IsValid())
						continue;
					if (HasInfNan(node->grad))
						overflow = true;
				}

				if (overflow) {
					mScale *= mBackoffFactor;
					if (mScale < mMinScale)
						mScale = mMinScale;
					mGoodSteps = 0;
					++mOverflowCount;
					return false;
				}

				const double inv = (mScale != 0.0f) ? (1.0 / (double)mScale) : 1.0;
				for (uint32 i = 0; i < params.Size(); ++i) {
					NkVarNode *node = params[i].Node();
					if (!node || !node->grad.IsValid())
						continue;
					node->grad = ops::MulScalar(node->grad, inv);
				}

				++mGoodSteps;
				if (mGoodSteps >= mGrowthInterval) {
					mScale *= mGrowthFactor;
					mGoodSteps = 0;
				}
				return true;
			}

		} // namespace optim
	} // namespace ai
} // namespace nkentseu
