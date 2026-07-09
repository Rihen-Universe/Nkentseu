// =============================================================================
// NkConv.cpp — implémentation de la couche convolutionnelle (NKAI, Phase 2).
// =============================================================================
#include "NKNN/NkConv.h"
#include "NKTensor/NkTensor.h"
#include "NKTensor/NkTensorOps.h"

#include <cmath>

namespace nkentseu {
	namespace ai {
		namespace nn {

			NkConv2D::NkConv2D(uint32 inChannels, uint32 outChannels, uint32 kernel, uint32 stride, uint32 pad,
							   uint32 seed)
				: mStride(stride), mPad(pad) {
				NkTensor W =
					NkTensor::Zeros(NkShape{(int64)outChannels, (int64)inChannels, (int64)kernel, (int64)kernel});
				NkTensor b = NkTensor::Zeros(NkShape{(int64)1, (int64)outChannels, (int64)1, (int64)1});

				// Init He-uniforme : W ~ U(-r, r), r = sqrt(6 / fanIn), fanIn = Cin·k·k.
				const uint32 fanIn = inChannels * kernel * kernel;
				double r = (fanIn > 0) ? std::sqrt(6.0 / (double)fanIn) : 0.1;
				uint32 s = seed ? seed : 1u;
				float *wp = W.DataAs<float>();
				const int64 n = NkShapeNumel(W.Shape());
				for (int64 i = 0; i < n; ++i) {
					s = s * 1664525u + 1013904223u;
					double u = (double)((s >> 8) & 0xFFFFu) / 65535.0 * 2.0 - 1.0; // [-1,1]
					wp[i] = (float)(u * r);
				}

				mW = NkVar::Leaf(W, true);
				mB = NkVar::Leaf(b, true);
			}

			NkVar NkConv2D::Forward(const NkVar &x) const {
				return autograd::Add(autograd::Conv2D(x, mW, (int32)mStride, (int32)mPad), mB);
			}

			void NkConv2D::Parameters(NkVector<NkVar> &out) const {
				out.PushBack(mW);
				out.PushBack(mB);
			}

			NkConvTranspose2D::NkConvTranspose2D(uint32 inChannels, uint32 outChannels, uint32 kernel, uint32 stride,
												 uint32 pad, uint32 seed)
				: mStride(stride), mPad(pad) {
				// Poids [Cin, Cout, k, k] (convention conv transposée).
				NkTensor W =
					NkTensor::Zeros(NkShape{(int64)inChannels, (int64)outChannels, (int64)kernel, (int64)kernel});
				NkTensor b = NkTensor::Zeros(NkShape{(int64)1, (int64)outChannels, (int64)1, (int64)1});
				const uint32 fanIn = outChannels * kernel * kernel;
				double r = (fanIn > 0) ? std::sqrt(6.0 / (double)fanIn) : 0.1;
				uint32 s = seed ? seed : 1u;
				float *wp = W.DataAs<float>();
				const int64 n = NkShapeNumel(W.Shape());
				for (int64 i = 0; i < n; ++i) {
					s = s * 1664525u + 1013904223u;
					double u = (double)((s >> 8) & 0xFFFFu) / 65535.0 * 2.0 - 1.0;
					wp[i] = (float)(u * r);
				}
				mW = NkVar::Leaf(W, true);
				mB = NkVar::Leaf(b, true);
			}

			NkVar NkConvTranspose2D::Forward(const NkVar &x) const {
				return autograd::Add(autograd::ConvTranspose2D(x, mW, (int32)mStride, (int32)mPad), mB);
			}

			void NkConvTranspose2D::Parameters(NkVector<NkVar> &out) const {
				out.PushBack(mW);
				out.PushBack(mB);
			}

			// ---- Variantes 3D (voxels) ---------------------------------------
			static NkVar MakeConvWeight3D(uint32 d0, uint32 d1, uint32 k, uint32 fanIn, uint32 seed) {
				NkTensor W = NkTensor::Zeros(NkShape{(int64)d0, (int64)d1, (int64)k, (int64)k, (int64)k});
				double r = (fanIn > 0) ? std::sqrt(6.0 / (double)fanIn) : 0.1;
				uint32 s = seed ? seed : 1u;
				float *wp = W.DataAs<float>();
				const int64 n = NkShapeNumel(W.Shape());
				for (int64 i = 0; i < n; ++i) {
					s = s * 1664525u + 1013904223u;
					double u = (double)((s >> 8) & 0xFFFFu) / 65535.0 * 2.0 - 1.0;
					wp[i] = (float)(u * r);
				}
				return NkVar::Leaf(W, true);
			}

			static NkVar MakeBias3D(uint32 c) {
				return NkVar::Leaf(NkTensor::Zeros(NkShape{1, (int64)c, 1, 1, 1}), true);
			}

			NkConv3D::NkConv3D(uint32 inC, uint32 outC, uint32 k, uint32 stride, uint32 pad, uint32 seed)
				: mStride(stride), mPad(pad) {
				mW = MakeConvWeight3D(outC, inC, k, inC * k * k * k, seed + 1u); // [Cout,Cin,k,k,k]
				mB = MakeBias3D(outC);
			}

			NkVar NkConv3D::Forward(const NkVar &x) const {
				return autograd::Add(autograd::Conv3D(x, mW, (int32)mStride, (int32)mPad), mB);
			}

			void NkConv3D::Parameters(NkVector<NkVar> &out) const {
				out.PushBack(mW);
				out.PushBack(mB);
			}

			NkConvTranspose3D::NkConvTranspose3D(uint32 inC, uint32 outC, uint32 k, uint32 stride, uint32 pad,
												 uint32 seed)
				: mStride(stride), mPad(pad) {
				mW = MakeConvWeight3D(inC, outC, k, outC * k * k * k, seed + 1u); // [Cin,Cout,k,k,k]
				mB = MakeBias3D(outC);
			}

			NkVar NkConvTranspose3D::Forward(const NkVar &x) const {
				return autograd::Add(autograd::ConvTranspose3D(x, mW, (int32)mStride, (int32)mPad), mB);
			}

			void NkConvTranspose3D::Parameters(NkVector<NkVar> &out) const {
				out.PushBack(mW);
				out.PushBack(mB);
			}

		} // namespace nn
	} // namespace ai
} // namespace nkentseu
