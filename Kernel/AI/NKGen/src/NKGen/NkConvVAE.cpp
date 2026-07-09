// =============================================================================
// NkConvVAE.cpp — VAE convolutionnel (NKAI, Phase 6).
// =============================================================================
#include "NKGen/NkConvVAE.h"

namespace nkentseu {
	namespace ai {
		namespace gen {

			NkConvVAE::NkConvVAE(uint32 imgW, uint32 channels, uint32 latent, uint32 seed)
				: mEncConv(1, channels, 3, /*stride*/ 1, /*pad*/ 1, seed + 1u),
				  mEncMu(channels * (imgW / 2) * (imgW / 2), latent, seed + 13u),
				  mEncLogvar(channels * (imgW / 2) * (imgW / 2), latent, seed + 29u),
				  mDecFc(latent, channels * (imgW / 2) * (imgW / 2), seed + 41u),
				  mDecConv(channels, 1, /*k*/ 3, /*stride*/ 1, /*pad*/ 1, seed + 57u), mImgW(imgW), mChannels(channels),
				  mLatent(latent), mPooled(imgW / 2) {
			}

			void NkConvVAE::Encode(const NkVar &x, NkVar &mu, NkVar &logvar) const {
				NkVar h = nn::Relu(mEncConv.Forward(x)); // [B, C, W, W]
				h = nn::MaxPool2D(h, 2, 2);				 // [B, C, W/2, W/2]
				h = nn::Flatten(h);						 // [B, C·(W/2)²]
				mu = mEncMu.Forward(h);
				logvar = mEncLogvar.Forward(h);
			}

			NkVar NkConvVAE::Reparam(const NkVar &mu, const NkVar &logvar, const NkVar &eps) const {
				NkVar sigma = autograd::Exp(autograd::MulScalar(logvar, 0.5));
				return autograd::Add(mu, autograd::Mul(sigma, eps));
			}

			NkVar NkConvVAE::DecodeLogits(const NkVar &z) const {
				const int64 B = z.Value().Shape()[0];
				NkVar h = nn::Relu(mDecFc.Forward(z)); // [B, C·(W/2)²]
				h = autograd::Reshape(h, NkShape{B, (int64)mChannels, (int64)mPooled, (int64)mPooled});
				h = autograd::Upsample2x(h); // [B, C, W, W] nearest ×2 (sans artefact damier)
				return mDecConv.Forward(h);	 // [B, 1, W, W] logits (Conv lisse)
			}

			NkVar NkConvVAE::Decode(const NkVar &z) const {
				return nn::Sigmoid(DecodeLogits(z)); // [0,1]
			}

			void NkConvVAE::Parameters(NkVector<NkVar> &out) const {
				mEncConv.Parameters(out);
				mEncMu.Parameters(out);
				mEncLogvar.Parameters(out);
				mDecFc.Parameters(out);
				mDecConv.Parameters(out);
			}

		} // namespace gen
	} // namespace ai
} // namespace nkentseu
