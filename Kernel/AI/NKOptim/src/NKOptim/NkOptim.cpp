// =============================================================================
// NkOptim.cpp — implémentation des optimiseurs (NKAI, Phase 2).
// =============================================================================
#include "NKOptim/NkOptim.h"
#include "NKTensor/NkTensor.h"
#include "NKTensor/NkTensorOps.h"

#include <cmath>

namespace nkentseu {
    namespace ai {
        namespace optim {

            NkSGD::NkSGD(const NkVector<NkVar>& params, float lr, float momentum)
                : mParams(params), mLr(lr), mMomentum(momentum) {
                if (mMomentum != 0.0f) {
                    // Vitesse initialisée à zéro, même forme que chaque paramètre.
                    for (uint32 i = 0; i < mParams.Size(); ++i)
                        mVelocity.PushBack(NkTensor::Zeros(mParams[i].Value().Shape()));
                }
            }

            void NkSGD::Step() {
                for (uint32 i = 0; i < mParams.Size(); ++i) {
                    NkVar& p = mParams[i];
                    const NkTensor& g = p.Grad();
                    if (!g.IsValid()) continue;   // pas de gradient (paramètre inutilisé)

                    if (mMomentum != 0.0f) {
                        // v = μ·v + g ; p = p − lr·v
                        mVelocity[i] = ops::Add(ops::MulScalar(mVelocity[i], mMomentum), g);
                        p.SetValue(ops::Sub(p.Value(), ops::MulScalar(mVelocity[i], mLr)));
                    } else {
                        // p = p − lr·g
                        p.SetValue(ops::Sub(p.Value(), ops::MulScalar(g, mLr)));
                    }
                }
            }

            void NkSGD::ZeroGrad() {
                for (uint32 i = 0; i < mParams.Size(); ++i)
                    mParams[i].ZeroGrad();
            }

            // ---- Adam --------------------------------------------------------
            NkAdam::NkAdam(const NkVector<NkVar>& params, float lr,
                           float beta1, float beta2, float eps)
                : mParams(params), mLr(lr), mB1(beta1), mB2(beta2), mEps(eps) {
                for (uint32 i = 0; i < mParams.Size(); ++i) {
                    mM.PushBack(NkTensor::Zeros(mParams[i].Value().Shape()));
                    mV.PushBack(NkTensor::Zeros(mParams[i].Value().Shape()));
                }
            }

            void NkAdam::Step() {
                ++mT;
                const double b1t = 1.0 - std::pow((double)mB1, (double)mT);
                const double b2t = 1.0 - std::pow((double)mB2, (double)mT);
                for (uint32 i = 0; i < mParams.Size(); ++i) {
                    NkVar& p = mParams[i];
                    const NkTensor& g = p.Grad();
                    if (!g.IsValid()) continue;

                    // m = β1·m + (1−β1)·g ; v = β2·v + (1−β2)·g²
                    mM[i] = ops::Add(ops::MulScalar(mM[i], mB1), ops::MulScalar(g, 1.0 - mB1));
                    mV[i] = ops::Add(ops::MulScalar(mV[i], mB2), ops::MulScalar(ops::Mul(g, g), 1.0 - mB2));

                    // Correction de biais + pas : p −= lr·m̂ / (√v̂ + ε)
                    NkTensor mhat  = ops::MulScalar(mM[i], (b1t > 0.0) ? 1.0 / b1t : 1.0);
                    NkTensor vhat  = ops::MulScalar(mV[i], (b2t > 0.0) ? 1.0 / b2t : 1.0);
                    NkTensor denom = ops::AddScalar(ops::Sqrt(vhat), mEps);
                    NkTensor upd   = ops::Div(mhat, denom);
                    p.SetValue(ops::Sub(p.Value(), ops::MulScalar(upd, mLr)));
                }
            }

            void NkAdam::ZeroGrad() {
                for (uint32 i = 0; i < mParams.Size(); ++i)
                    mParams[i].ZeroGrad();
            }

        } // namespace optim
    } // namespace ai
} // namespace nkentseu
