// =============================================================================
// NkVAE.cpp — implémentation de l'auto-encodeur variationnel (NKAI, Phase 6).
// =============================================================================
#include "NKGen/NkVAE.h"

namespace nkentseu {
	namespace ai {
		namespace gen {

			NkVAE::NkVAE(uint32 inDim, uint32 hidden, uint32 latent, uint32 seed)
				: mEnc1(inDim, hidden, seed + 1u), mEncMu(hidden, latent, seed + 13u),
				  mEncLogvar(hidden, latent, seed + 29u), mDec1(latent, hidden, seed + 41u),
				  mDec2(hidden, inDim, seed + 57u), mInDim(inDim), mLatent(latent) {
			}

			void NkVAE::Encode(const NkVar &x, NkVar &mu, NkVar &logvar) const {
				NkVar h = nn::Relu(mEnc1.Forward(x));
				mu = mEncMu.Forward(h);
				logvar = mEncLogvar.Forward(h);
			}

			NkVar NkVAE::Reparam(const NkVar &mu, const NkVar &logvar, const NkVar &eps) const {
				// σ = exp(0.5·logσ²) ; z = μ + σ·ε.
				NkVar sigma = autograd::Exp(autograd::MulScalar(logvar, 0.5));
				return autograd::Add(mu, autograd::Mul(sigma, eps));
			}

			NkVar NkVAE::DecodeLogits(const NkVar &z) const {
				return mDec2.Forward(nn::Relu(mDec1.Forward(z))); // avant sigmoid
			}

			NkVar NkVAE::Decode(const NkVar &z) const {
				return nn::Sigmoid(DecodeLogits(z)); // [0,1]
			}

			void NkVAE::Parameters(NkVector<NkVar> &out) const {
				mEnc1.Parameters(out);
				mEncMu.Parameters(out);
				mEncLogvar.Parameters(out);
				mDec1.Parameters(out);
				mDec2.Parameters(out);
			}

			NkVar KLDivergence(const NkVar &mu, const NkVar &logvar) {
				// -0.5 · Σ (1 + logσ² − μ² − exp(logσ²))
				NkVar muSq = autograd::Mul(mu, mu);
				NkVar expLV = autograd::Exp(logvar);
				NkVar inner = autograd::AddScalar(autograd::Sub(autograd::Sub(logvar, muSq), expLV), 1.0);
				return autograd::MulScalar(autograd::Sum(inner), -0.5);
			}

		} // namespace gen
	} // namespace ai
} // namespace nkentseu
