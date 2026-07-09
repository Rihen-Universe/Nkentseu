// =============================================================================
// NkAutoencoder.h — auto-encodeur (NKAI, Phase 6, Jalon 1 génératif).
//
// Un auto-encodeur COMPRESSE une entrée dans un petit espace LATENT (encodeur)
// puis la RECONSTRUIT (décodeur). Entraîné à minimiser l'erreur de reconstruction
// (MSE), il apprend une représentation compacte — la brique de base des modèles
// génératifs (on GÉNÈRE en décodant un point latent). Construit au-dessus de NKNN.
// Namespace : nkentseu::ai::gen.
//
//   x [B,D] --Encode--> z [B,L] --Decode--> x̂ [B,D]
//   Encode : Dense(D->H) -> relu -> Dense(H->L)
//   Decode : Dense(L->H) -> relu -> Dense(H->D) -> sigmoid   (sortie dans [0,1])
// =============================================================================
#pragma once

#include "NKNN/NkNN.h"
#include "NKAutograd/NkVar.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace gen {

			class NkAutoencoder {
				public:
					NkAutoencoder() = default;
					NkAutoencoder(uint32 inDim, uint32 hidden, uint32 latent, uint32 seed = 1u);

					NkVar Encode(const NkVar &x) const; // -> latent [B,L]
					NkVar Decode(const NkVar &z) const; // -> reconstruction [B,D]

					NkVar Forward(const NkVar &x) const {
						return Decode(Encode(x));
					}

					void Parameters(NkVector<NkVar> &out) const; // enc + dec

					uint32 LatentDim() const {
						return mLatent;
					}

					uint32 InputDim() const {
						return mInDim;
					}

				private:
					nn::NkDense mEnc1, mEnc2; // encodeur
					nn::NkDense mDec1, mDec2; // décodeur
					uint32 mInDim = 0;
					uint32 mLatent = 0;
			};

		} // namespace gen
	} // namespace ai
} // namespace nkentseu
