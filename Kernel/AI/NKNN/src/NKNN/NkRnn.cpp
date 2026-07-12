// =============================================================================
// NkRnn.cpp — cellules récurrentes GRU / LSTM (NKAI, Phase 2).
// =============================================================================
#include "NKNN/NkRnn.h"
#include "NKTensor/NkTensor.h"

namespace nkentseu {
	namespace ai {
		namespace nn {

			using namespace autograd;

			// =====================================================================
			// GRU
			// =====================================================================
			NkGRUCell::NkGRUCell(uint32 inFeatures, uint32 hidden, uint32 seed)
				: mHidden(hidden),
				  // Graines décalées pour que chaque porte s'initialise différemment.
				  mXr(inFeatures, hidden, seed + 1u), mHr(hidden, hidden, seed + 2u),
				  mXz(inFeatures, hidden, seed + 3u), mHz(hidden, hidden, seed + 4u),
				  mXn(inFeatures, hidden, seed + 5u), mHn(hidden, hidden, seed + 6u) {
			}

			NkVar NkGRUCell::Forward(const NkVar &x, const NkVar &h) const {
				NkVar r = Sigmoid(Add(mXr.Forward(x), mHr.Forward(h)));		 // réinitialisation
				NkVar z = Sigmoid(Add(mXz.Forward(x), mHz.Forward(h)));		 // mise à jour
				NkVar n = Tanh(Add(mXn.Forward(x), Mul(r, mHn.Forward(h)))); // candidat
				// h' = (1−z) ⊙ n + z ⊙ h
				NkVar oneMinusZ = AddScalar(MulScalar(z, -1.0), 1.0);
				return Add(Mul(oneMinusZ, n), Mul(z, h));
			}

			void NkGRUCell::Parameters(NkVector<NkVar> &out) const {
				mXr.Parameters(out);
				mHr.Parameters(out);
				mXz.Parameters(out);
				mHz.Parameters(out);
				mXn.Parameters(out);
				mHn.Parameters(out);
			}

			// =====================================================================
			// LSTM
			// =====================================================================
			NkLSTMCell::NkLSTMCell(uint32 inFeatures, uint32 hidden, uint32 seed)
				: mHidden(hidden), mXi(inFeatures, hidden, seed + 1u), mHi(hidden, hidden, seed + 2u),
				  mXf(inFeatures, hidden, seed + 3u), mHf(hidden, hidden, seed + 4u),
				  mXg(inFeatures, hidden, seed + 5u), mHg(hidden, hidden, seed + 6u),
				  mXo(inFeatures, hidden, seed + 7u), mHo(hidden, hidden, seed + 8u) {
			}

			NkLSTMState NkLSTMCell::Forward(const NkVar &x, const NkLSTMState &s) const {
				NkVar i = Sigmoid(Add(mXi.Forward(x), mHi.Forward(s.h))); // entrée
				NkVar f = Sigmoid(Add(mXf.Forward(x), mHf.Forward(s.h))); // oubli
				NkVar g = Tanh(Add(mXg.Forward(x), mHg.Forward(s.h)));	 // candidat
				NkVar o = Sigmoid(Add(mXo.Forward(x), mHo.Forward(s.h))); // sortie
				NkLSTMState out;
				out.c = Add(Mul(f, s.c), Mul(i, g)); // c' = f⊙c + i⊙g
				out.h = Mul(o, Tanh(out.c));		 // h' = o⊙tanh(c')
				return out;
			}

			void NkLSTMCell::Parameters(NkVector<NkVar> &out) const {
				mXi.Parameters(out);
				mHi.Parameters(out);
				mXf.Parameters(out);
				mHf.Parameters(out);
				mXg.Parameters(out);
				mHg.Parameters(out);
				mXo.Parameters(out);
				mHo.Parameters(out);
			}

			// =====================================================================
			// Utilitaires de séquence
			// =====================================================================
			NkVar ZeroState(uint32 batch, uint32 hidden) {
				return NkVar::Leaf(NkTensor::Zeros(NkShape{(int64)batch, (int64)hidden}), false);
			}

			NkVector<NkVar> GRURunSeq(const NkGRUCell &cell, const NkVector<NkVar> &xs, const NkVar &h0) {
				NkVector<NkVar> outs;
				NkVar h = h0;
				for (uint32 t = 0; t < xs.Size(); ++t) {
					h = cell.Forward(xs[t], h);
					outs.PushBack(h);
				}
				return outs;
			}

			NkVector<NkVar> LSTMRunSeq(const NkLSTMCell &cell, const NkVector<NkVar> &xs, const NkLSTMState &s0) {
				NkVector<NkVar> outs;
				NkLSTMState s = s0;
				for (uint32 t = 0; t < xs.Size(); ++t) {
					s = cell.Forward(xs[t], s);
					outs.PushBack(s.h);
				}
				return outs;
			}

			NkVar StackTime(const NkVector<NkVar> &steps) {
				if (steps.Size() == 0)
					return NkVar();
				// Chaque pas [B, H] -> [1, B, H], puis concat sur l'axe 0.
				const NkShape &sh = steps[0].Value().Shape();
				const int64 B = sh.Size() >= 1 ? sh[0] : 1;
				const int64 H = sh.Size() >= 2 ? sh[1] : 1;
				NkShape s3{(int64)1, B, H};
				NkVar acc = Reshape(steps[0], s3);
				for (uint32 t = 1; t < steps.Size(); ++t)
					acc = Concat0(acc, Reshape(steps[t], s3));
				return acc;
			}

		} // namespace nn
	} // namespace ai
} // namespace nkentseu
