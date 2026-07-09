// =============================================================================
// NkVoxelVAE.cpp — VAE 3D convolutionnel (NKAI, Phase 6).
// =============================================================================
#include "NKGen/NkVoxelVAE.h"

namespace nkentseu {
	namespace ai {
		namespace gen {

			NkVoxelVAE::NkVoxelVAE(uint32 gridW, uint32 channels, uint32 latent, uint32 seed)
				: mEncConv(1, channels, 3, /*stride*/ 2, /*pad*/ 1, seed + 1u), // W -> W/2
				  mEncMu(channels * (gridW / 2) * (gridW / 2) * (gridW / 2), latent, seed + 13u),
				  mEncLogvar(channels * (gridW / 2) * (gridW / 2) * (gridW / 2), latent, seed + 29u),
				  mDecFc(latent, channels * (gridW / 2) * (gridW / 2) * (gridW / 2), seed + 41u),
				  mDecDeconv(channels, 1, /*k*/ 2, /*stride*/ 2, /*pad*/ 0, seed + 57u), // W/2 -> W
				  mGridW(gridW), mChannels(channels), mLatent(latent), mPooled(gridW / 2) {
			}

			void NkVoxelVAE::Encode(const NkVar &x, NkVar &mu, NkVar &logvar) const {
				NkVar h = nn::Relu(mEncConv.Forward(x)); // [B, C, W/2, W/2, W/2]
				h = nn::Flatten(h);						 // [B, C·(W/2)³]
				mu = mEncMu.Forward(h);
				logvar = mEncLogvar.Forward(h);
			}

			NkVar NkVoxelVAE::Reparam(const NkVar &mu, const NkVar &logvar, const NkVar &eps) const {
				NkVar sigma = autograd::Exp(autograd::MulScalar(logvar, 0.5));
				return autograd::Add(mu, autograd::Mul(sigma, eps));
			}

			NkVar NkVoxelVAE::Decode(const NkVar &z) const {
				const int64 B = z.Value().Shape()[0];
				NkVar h = nn::Relu(mDecFc.Forward(z)); // [B, C·(W/2)³]
				h = autograd::Reshape(h, NkShape{B, (int64)mChannels, (int64)mPooled, (int64)mPooled, (int64)mPooled});
				h = mDecDeconv.Forward(h); // [B, 1, W, W, W]
				return nn::Sigmoid(h);
			}

			void NkVoxelVAE::Parameters(NkVector<NkVar> &out) const {
				mEncConv.Parameters(out);
				mEncMu.Parameters(out);
				mEncLogvar.Parameters(out);
				mDecFc.Parameters(out);
				mDecDeconv.Parameters(out);
			}

		} // namespace gen
	} // namespace ai
} // namespace nkentseu
