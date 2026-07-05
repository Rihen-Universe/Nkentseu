// =============================================================================
// NkTrain.cpp — helpers compilés de la boucle d'entraînement (NKAI, Phase 3).
// =============================================================================
#include "NKTrain/NkTrain.h"

namespace nkentseu {
    namespace ai {
        namespace train {

            uint32 CountCorrect(const NkTensor& logits, const NkVector<int32>& labels) {
                NkTensor lc = logits.Contiguous();
                const NkShape& sh = lc.Shape();
                const uint32 B = sh.Size() >= 1 ? (uint32)sh[0] : 0;
                const uint32 C = sh.Size() >= 2 ? (uint32)sh[1] : 0;
                if (B == 0 || C == 0) return 0;
                const float* lp = lc.DataAs<float>();
                uint32 correct = 0;
                for (uint32 i = 0; i < B && i < labels.Size(); ++i) {
                    const float* row = lp + (nk_size)i * C;
                    uint32 best = 0; float bv = row[0];
                    for (uint32 c = 1; c < C; ++c) if (row[c] > bv) { bv = row[c]; best = c; }
                    if ((int32)best == labels[i]) ++correct;
                }
                return correct;
            }

        } // namespace train
    } // namespace ai
} // namespace nkentseu
