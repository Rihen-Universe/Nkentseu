// =============================================================================
// NkAutoencoder.cpp — implémentation de l'auto-encodeur (NKAI, Phase 6).
// =============================================================================
#include "NKGen/NkAutoencoder.h"

namespace nkentseu {
	namespace ai {
		namespace gen {

			NkAutoencoder::NkAutoencoder(uint32 inDim, uint32 hidden, uint32 latent, uint32 seed)
				: mEnc1(inDim, hidden, seed + 1u), mEnc2(hidden, latent, seed + 13u), mDec1(latent, hidden, seed + 29u),
				  mDec2(hidden, inDim, seed + 47u), mInDim(inDim), mLatent(latent) {
			}

			NkVar NkAutoencoder::Encode(const NkVar &x) const {
				return mEnc2.Forward(nn::Relu(mEnc1.Forward(x)));
			}

			NkVar NkAutoencoder::Decode(const NkVar &z) const {
				// sigmoid en sortie : reconstruction bornée dans [0,1] (comme les pixels).
				return nn::Sigmoid(mDec2.Forward(nn::Relu(mDec1.Forward(z))));
			}

			void NkAutoencoder::Parameters(NkVector<NkVar> &out) const {
				mEnc1.Parameters(out);
				mEnc2.Parameters(out);
				mDec1.Parameters(out);
				mDec2.Parameters(out);
			}

		} // namespace gen
	} // namespace ai
} // namespace nkentseu
