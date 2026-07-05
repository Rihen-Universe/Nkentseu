// =============================================================================
// NkLosses.cpp — utilitaires de cible (NKAI, Phase 2).
// =============================================================================
#include "NKNN/NkLosses.h"
#include "NKTensor/NkTensor.h"

namespace nkentseu {
    namespace ai {
        namespace nn {

            NkTensor OneHot(const int32* labels, uint32 count, uint32 numClasses) {
                NkTensor oh = NkTensor::Zeros(NkShape{ (int64)count, (int64)numClasses });
                float* p = oh.DataAs<float>();
                for (uint32 i = 0; i < count; ++i) {
                    int32 c = labels[i];
                    if (c >= 0 && (uint32)c < numClasses)
                        p[(int64)i * numClasses + c] = 1.0f;
                }
                return oh;
            }

        } // namespace nn
    } // namespace ai
} // namespace nkentseu
