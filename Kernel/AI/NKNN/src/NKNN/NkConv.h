// =============================================================================
// NkConv.h — couche de convolution 2D + pooling/flatten (NKAI, Phase 2, Jalon 2).
//
// Brique des réseaux convolutionnels : partage de poids spatial (tractable +
// capte la structure locale), indispensable aux modèles image/3D (VAE, diffusion,
// perception CNN des agents). Construit sur autograd::Conv2D. Namespace nkentseu::ai::nn.
// =============================================================================
#pragma once

#include "NKAutograd/NkVar.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace nn {

			// Couche convolutionnelle : y = conv2d(x, W) + b.
			//   W : [Cout, Cin, k, k]   b : [1, Cout, 1, 1]  (biais broadcasté)
			// x : [B, Cin, H, W] -> y : [B, Cout, outH, outW].
			// Poids initialisés He-uniforme (adaptés à ReLU) ; biais à zéro.
			class NkConv2D {
				public:
					NkConv2D() = default;
					NkConv2D(uint32 inChannels, uint32 outChannels, uint32 kernel, uint32 stride = 1, uint32 pad = 0,
							 uint32 seed = 1u);

					NkVar Forward(const NkVar &x) const;
					void Parameters(NkVector<NkVar> &out) const;

					const NkVar &Weight() const {
						return mW;
					}

					const NkVar &Bias() const {
						return mB;
					}

				private:
					NkVar mW;
					NkVar mB;
					uint32 mStride = 1;
					uint32 mPad = 0;
			};

			// Couche de convolution TRANSPOSÉE : y = convT2d(x, W) + b (upsampling appris).
			//   W : [Cin, Cout, k, k]   b : [1, Cout, 1, 1]
			// x : [B, Cin, H, W] -> y : [B, Cout, (H-1)·stride-2·pad+k, …]. Décodeurs génératifs.
			class NkConvTranspose2D {
				public:
					NkConvTranspose2D() = default;
					NkConvTranspose2D(uint32 inChannels, uint32 outChannels, uint32 kernel, uint32 stride = 1,
									  uint32 pad = 0, uint32 seed = 1u);
					NkVar Forward(const NkVar &x) const;
					void Parameters(NkVector<NkVar> &out) const;

					const NkVar &Weight() const {
						return mW;
					}

					const NkVar &Bias() const {
						return mB;
					}

				private:
					NkVar mW;
					NkVar mB;
					uint32 mStride = 1;
					uint32 mPad = 0;
			};

			// Convolution 3D (voxels) : W [Cout,Cin,k,k,k], b [1,Cout,1,1,1].
			class NkConv3D {
				public:
					NkConv3D() = default;
					NkConv3D(uint32 inChannels, uint32 outChannels, uint32 kernel, uint32 stride = 1, uint32 pad = 0,
							 uint32 seed = 1u);
					NkVar Forward(const NkVar &x) const;
					void Parameters(NkVector<NkVar> &out) const;

				private:
					NkVar mW, mB;
					uint32 mStride = 1, mPad = 0;
			};

			// Convolution transposée 3D (upsampling voxels) : W [Cin,Cout,k,k,k].
			class NkConvTranspose3D {
				public:
					NkConvTranspose3D() = default;
					NkConvTranspose3D(uint32 inChannels, uint32 outChannels, uint32 kernel, uint32 stride = 1,
									  uint32 pad = 0, uint32 seed = 1u);
					NkVar Forward(const NkVar &x) const;
					void Parameters(NkVector<NkVar> &out) const;

				private:
					NkVar mW, mB;
					uint32 mStride = 1, mPad = 0;
			};

			// Max-pooling 2D (réduction spatiale) — ré-export lisible.
			inline NkVar MaxPool2D(const NkVar &x, uint32 kernel = 2, uint32 stride = 2) {
				return autograd::MaxPool2D(x, (int32)kernel, (int32)stride);
			}

			// Aplatit [B, C, H, W] -> [B, C*H*W] (transition conv -> dense).
			inline NkVar Flatten(const NkVar &x) {
				const NkShape &s = x.Value().Shape();
				int64 B = s.Size() >= 1 ? s[0] : 1, rest = 1;
				for (uint32 i = 1; i < s.Size(); ++i)
					rest *= s[i];
				return autograd::Reshape(x, NkShape{B, rest});
			}

		} // namespace nn
	} // namespace ai
} // namespace nkentseu
