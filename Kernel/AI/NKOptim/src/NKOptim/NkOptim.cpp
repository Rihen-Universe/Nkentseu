// =============================================================================
// NkOptim.cpp — implémentation des optimiseurs (NKAI, Phase 2).
// =============================================================================
#include "NKOptim/NkOptim.h"
#include "NKTensor/NkTensor.h"
#include "NKTensor/NkTensorOps.h"
#include "NKTensor/NkTensorGpu.h" // pas d'Adam fusé quand les params résident sur GPU

#include <cmath>

namespace nkentseu {
	namespace ai {
		namespace optim {

			// --- Helpers résidence GPU (miroir de NkVar.cpp ToCpuT/ToDevOf) : le
			// clipping élément-par-élément (NK_CLIP_BY_VALUE) n'a pas de noyau GPU dédié,
			// il fait donc un aller-retour CPU pour les paramètres résidents GPU. La norme
			// globale (NK_CLIP_BY_GLOBAL_NORM) passe par ops:: (déjà GPU-conscient).
			static NkTensor ClipToCpu(const NkTensor &t) {
				return (t.IsValid() && t.Device() == NkDevice::NK_GPU) ? t.ToCPU() : t;
			}

			static NkTensor ClipToDevOf(const NkTensor &r, const NkTensor &ref) {
				return (ref.Device() == NkDevice::NK_GPU && r.IsValid() && r.Device() != NkDevice::NK_GPU)
						   ? r.ToGPU()
						   : r;
			}

			double ClipGradients(NkVector<NkVar> &params, float clipValue, NkGradClipMode mode) {
				if (mode == NkGradClipMode::NK_CLIP_NONE || clipValue <= 0.0f)
					return 0.0;

				if (mode == NkGradClipMode::NK_CLIP_BY_VALUE) {
					for (uint32 i = 0; i < params.Size(); ++i) {
						NkVar &p = params[i];
						const NkTensor &g = p.Grad();
						if (!g.IsValid())
							continue;
						NkTensor gc = ClipToCpu(g).Contiguous().Clone();
						float *gp = gc.DataAs<float>();
						const int64 n = NkShapeNumel(gc.Shape());
						for (int64 j = 0; j < n; ++j) {
							if (gp[j] > clipValue)
								gp[j] = clipValue;
							else if (gp[j] < -clipValue)
								gp[j] = -clipValue;
						}
						p.SetGrad(ClipToDevOf(gc, g));
					}
					return 0.0;
				}

				// NK_CLIP_BY_GLOBAL_NORM : norme L2 sur TOUS les paramètres concaténés.
				double sumSq = 0.0;
				for (uint32 i = 0; i < params.Size(); ++i) {
					const NkTensor &g = params[i].Grad();
					if (!g.IsValid())
						continue;
					NkTensor s = ops::Sum(ops::Mul(g, g)); // scalaire {1}, GPU-conscient
					NkTensor sc = ClipToCpu(s);
					sumSq += sc.IsValid() ? sc.GetItem(NkShape{(int64)0}) : 0.0;
				}
				const double norm = std::sqrt(sumSq);
				if (norm <= (double)clipValue)
					return norm; // déjà sous le seuil -> rien à faire (identique à un no-op)

				const double eps = 1e-6;
				const double scale = (double)clipValue / (norm + eps);
				for (uint32 i = 0; i < params.Size(); ++i) {
					NkVar &p = params[i];
					const NkTensor &g = p.Grad();
					if (!g.IsValid())
						continue;
					p.SetGrad(ops::MulScalar(g, scale));
				}
				return norm;
			}

			NkSGD::NkSGD(const NkVector<NkVar> &params, float lr, float momentum)
				: mParams(params), mLr(lr), mMomentum(momentum) {
				if (mMomentum != 0.0f) {
					// Vitesse initialisée à zéro, même forme que chaque paramètre.
					for (uint32 i = 0; i < mParams.Size(); ++i)
						mVelocity.PushBack(NkTensor::Zeros(mParams[i].Value().Shape()));
				}
			}

			void NkSGD::Step() {
				if (mClipMode != NkGradClipMode::NK_CLIP_NONE)
					ClipGradients(mParams, mClipValue, mClipMode);
				for (uint32 i = 0; i < mParams.Size(); ++i) {
					NkVar &p = mParams[i];
					const NkTensor &g = p.Grad();
					if (!g.IsValid())
						continue; // pas de gradient (paramètre inutilisé)

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
			NkAdam::NkAdam(const NkVector<NkVar> &params, float lr, float beta1, float beta2, float eps,
						   float weightDecay)
				: mParams(params), mLr(lr), mB1(beta1), mB2(beta2), mEps(eps), mWd(weightDecay) {
				for (uint32 i = 0; i < mParams.Size(); ++i) {
					// État sur le MÊME device que le paramètre -> Adam résident dès le 1er pas.
					NkTensor z0 = NkTensor::Zeros(mParams[i].Value().Shape());
					NkTensor v0 = NkTensor::Zeros(mParams[i].Value().Shape());
					if (mParams[i].Value().Device() == NkDevice::NK_GPU) {
						z0 = z0.ToGPU();
						v0 = v0.ToGPU();
					}
					mM.PushBack(z0);
					mV.PushBack(v0);
				}
			}

			void NkAdam::Step() {
				if (mClipMode != NkGradClipMode::NK_CLIP_NONE)
					ClipGradients(mParams, mClipValue, mClipMode);
				++mT;
				const double b1t = 1.0 - std::pow((double)mB1, (double)mT);
				const double b2t = 1.0 - std::pow((double)mB2, (double)mT);
				for (uint32 i = 0; i < mParams.Size(); ++i) {
					NkVar &p = mParams[i];
					const NkTensor &g = p.Grad();
					if (!g.IsValid())
						continue;

					// Voie RAPIDE : si tout réside sur GPU, un SEUL kernel fusé fait le pas
					// (param/m/v mis à jour EN PLACE) -> ~8 dispatchs synchrones -> 1. Inclut wd (AdamW).
					if (NkGpuAdamStep(p.Value(), g, mM[i], mV[i], mLr, mB1, mB2, mEps, (float)b1t, (float)b2t, mWd))
						continue;

					// m = β1·m + (1−β1)·g ; v = β2·v + (1−β2)·g²
					mM[i] = ops::Add(ops::MulScalar(mM[i], mB1), ops::MulScalar(g, 1.0 - mB1));
					mV[i] = ops::Add(ops::MulScalar(mV[i], mB2), ops::MulScalar(ops::Mul(g, g), 1.0 - mB2));

					// Correction de biais + pas AdamW : p −= lr·(m̂/(√v̂+ε) + wd·p)
					NkTensor mhat = ops::MulScalar(mM[i], (b1t > 0.0) ? 1.0 / b1t : 1.0);
					NkTensor vhat = ops::MulScalar(mV[i], (b2t > 0.0) ? 1.0 / b2t : 1.0);
					NkTensor denom = ops::AddScalar(ops::Sqrt(vhat), mEps);
					NkTensor upd = ops::Div(mhat, denom);
					if (mWd > 0.0f)
						upd = ops::Add(upd, ops::MulScalar(p.Value(), mWd));
					p.SetValue(ops::Sub(p.Value(), ops::MulScalar(upd, mLr)));
				}
			}

			void NkAdam::ZeroGrad() {
				for (uint32 i = 0; i < mParams.Size(); ++i)
					mParams[i].ZeroGrad();
			}

			void NkAdam::SetMoments(const NkVector<NkTensor> &m, const NkVector<NkTensor> &v) {
				const uint32 n = mParams.Size();
				for (uint32 i = 0; i < n && i < m.Size() && i < v.Size(); ++i) {
					NkTensor mm = m[i];
					NkTensor vv = v[i];
					if (mParams[i].Value().Device() == NkDevice::NK_GPU) {
						mm = mm.ToGPU();
						vv = vv.ToGPU();
					}
					mM[i] = mm;
					mV[i] = vv;
				}
			}

		} // namespace optim
	} // namespace ai
} // namespace nkentseu
