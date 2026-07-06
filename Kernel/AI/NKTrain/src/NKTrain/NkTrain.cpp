// =============================================================================
// NkTrain.cpp — helpers compilés de la boucle d'entraînement (NKAI, Phase 3).
// =============================================================================
#include "NKTrain/NkTrain.h"
#include "NKTensor/NkTensorOps.h"   // ops::Argmax (GPU-natif quand les logits résident)

namespace nkentseu {
    namespace ai {
        namespace train {

            uint32 CountCorrect(const NkTensor& logits, const NkVector<int32>& labels) {
                if (logits.Rank() < 2) return 0;
                const uint32 B = (uint32)logits.Shape()[0];
                // Argmax par ligne via l'API réduction : calculé sur GPU si les logits y
                // résident (seuls les B indices reviennent), sinon sur CPU. Sortie i64 [B].
                NkTensor pred = ops::Argmax(logits, 1);
                if (!pred.IsValid()) return 0;
                uint32 correct = 0;
                for (uint32 i = 0; i < B && i < labels.Size(); ++i)
                    if ((int32)pred.GetItem(NkShape{ (int64)i }) == labels[i]) ++correct;
                return correct;
            }

        } // namespace train
    } // namespace ai
} // namespace nkentseu
