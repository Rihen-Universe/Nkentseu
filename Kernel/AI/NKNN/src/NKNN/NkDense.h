// =============================================================================
// NkDense.h — couche entièrement connectée (NKAI, Phase 2).
// =============================================================================
#pragma once

#include "NKAutograd/NkVar.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace ai {
        namespace nn {

            // Couche dense : y = x·W + b.
            //   W : [inFeatures, outFeatures]   b : [1, outFeatures]
            // x : [batch, inFeatures] -> y : [batch, outFeatures] (biais broadcasté).
            // Poids initialisés Xavier-uniforme ; biais à zéro. Les paramètres sont
            // des feuilles NkVar PERSISTANTES (requiresGrad) mises à jour par NKOptim.
            class NkDense {
            public:
                NkDense() = default;
                NkDense(uint32 inFeatures, uint32 outFeatures, uint32 seed = 1u);

                NkVar Forward(const NkVar& x) const;

                // Ajoute W puis b à `out` (pour alimenter un optimiseur / la sérialisation).
                void  Parameters(NkVector<NkVar>& out) const;

                const NkVar& Weight() const { return mW; }
                const NkVar& Bias()   const { return mB; }

            private:
                NkVar mW;
                NkVar mB;
            };

        } // namespace nn
    } // namespace ai
} // namespace nkentseu
