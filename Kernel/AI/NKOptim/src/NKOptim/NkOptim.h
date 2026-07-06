// =============================================================================
// NkOptim.h — optimiseurs (NKAI, Phase 2).
//
// Un optimiseur détient une liste de paramètres (feuilles NkVar persistantes) et
// applique la règle de mise à jour à partir de leur gradient (rempli par
// NkVar::Backward). Il modifie les paramètres EN PLACE (NkVar::SetValue) pour que
// le prochain forward réutilise les poids mis à jour. Namespace : nkentseu::ai::optim.
// =============================================================================
#pragma once

#include "NKAutograd/NkVar.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace ai {
        namespace optim {

            // -----------------------------------------------------------------
            // SGD — descente de gradient stochastique, avec momentum optionnel.
            //   sans momentum : p ← p − lr·g
            //   avec momentum : v ← μ·v + g ; p ← p − lr·v
            // -----------------------------------------------------------------
            class NkSGD {
            public:
                NkSGD() = default;
                NkSGD(const NkVector<NkVar>& params, float lr, float momentum = 0.0f);

                void Step();       // applique un pas sur tous les paramètres
                void ZeroGrad();   // remet à zéro les gradients des paramètres

                float LearningRate() const { return mLr; }
                void  SetLearningRate(float lr) { mLr = lr; }

            private:
                NkVector<NkVar>  mParams;
                NkVector<NkTensor> mVelocity; // état momentum (par paramètre)
                float mLr       = 0.01f;
                float mMomentum = 0.0f;
            };

            // -----------------------------------------------------------------
            // Adam — moments adaptatifs (Kingma & Ba, 2014).
            //   m ← β1·m + (1−β1)·g ;  v ← β2·v + (1−β2)·g²
            //   m̂ = m/(1−β1ᵗ) ;  v̂ = v/(1−β2ᵗ)
            //   p ← p − lr·m̂ / (√v̂ + ε)
            // -----------------------------------------------------------------
            class NkAdam {
            public:
                NkAdam() = default;
                // `weightDecay` > 0 -> AdamW (weight decay découplé).
                NkAdam(const NkVector<NkVar>& params, float lr = 0.001f,
                       float beta1 = 0.9f, float beta2 = 0.999f, float eps = 1e-8f,
                       float weightDecay = 0.0f);

                void Step();
                void ZeroGrad();

                float LearningRate() const { return mLr; }
                void  SetLearningRate(float lr) { mLr = lr; }

            private:
                NkVector<NkVar>    mParams;
                NkVector<NkTensor> mM;   // 1er moment (par paramètre)
                NkVector<NkTensor> mV;   // 2e moment
                float mLr  = 0.001f;
                float mB1  = 0.9f;
                float mB2  = 0.999f;
                float mEps = 1e-8f;
                float mWd  = 0.0f;       // weight decay (AdamW si > 0)
                int64 mT   = 0;          // compteur de pas (pour la correction de biais)
            };

        } // namespace optim
    } // namespace ai
} // namespace nkentseu
