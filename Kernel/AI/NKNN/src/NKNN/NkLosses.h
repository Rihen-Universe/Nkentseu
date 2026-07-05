// =============================================================================
// NkLosses.h — fonctions de perte + utilitaires de cible (NKAI, Phase 2).
// =============================================================================
#pragma once

#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"

namespace nkentseu {
    namespace ai {
        namespace nn {

            // Erreur quadratique moyenne (régression / cibles 0-1).
            inline NkVar MSELoss(const NkVar& pred, const NkVar& target) {
                return autograd::MSE(pred, target);
            }

            // Entropie croisée pour la classification : `logits` [B,C] (scores bruts,
            // AVANT softmax) vs `targetOneHot` [B,C]. Softmax appliqué en interne.
            inline NkVar CrossEntropyLoss(const NkVar& logits, const NkVar& targetOneHot) {
                return autograd::SoftmaxCrossEntropy(logits, targetOneHot);
            }
            // Entropie croisée BINAIRE (reconstruction d'images, cible [0,1]) : `logits`
            // = sortie AVANT sigmoid. Plus nette que MSE. Sigmoid appliqué en interne.
            inline NkVar BCELoss(const NkVar& logits, const NkVar& target) {
                return autograd::SigmoidBCE(logits, target);
            }

            // Construit une matrice one-hot [count, numClasses] (F32) depuis des
            // indices de classe (0..numClasses-1).
            NkTensor OneHot(const int32* labels, uint32 count, uint32 numClasses);

        } // namespace nn
    } // namespace ai
} // namespace nkentseu
