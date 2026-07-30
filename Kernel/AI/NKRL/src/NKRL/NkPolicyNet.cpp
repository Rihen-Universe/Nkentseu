// =============================================================================
// NkPolicyNet.cpp — implémentation du réseau de politique (NKAI, Phase 4, Jalon 3).
// =============================================================================
#include "NKRL/NkPolicyNet.h"
#include "NKTensor/NkTensorOps.h"

#include <cmath>

namespace nkentseu {
	namespace ai {
		namespace rl {

			namespace {
				const double kPi = 3.14159265358979323846;
			}

			NkPolicyNet::NkPolicyNet(NkPolicyMode mode, uint32 inputDim, uint32 outputDim,
									 const NkPolicyNetConfig &config)
				: mMode(mode), mInputDim(inputDim), mOutputDim(outputDim),
				  mLayer1(inputDim, config.hiddenSize, (config.seed ? config.seed : 1u) * 4u + 1u),
				  mLayer2(config.hiddenSize, outputDim, (config.seed ? config.seed : 1u) * 4u + 2u),
				  mActionScale(config.actionScale > 0.0f ? config.actionScale : 1.0f),
				  mRng((config.seed ? config.seed : 1u) * 4u + 7u) {
				if (mMode == NkPolicyMode::ContinuousGaussian) {
					NkTensor logStdT = NkTensor::Full(NkShape{(int64)1, (int64)outputDim}, (double)config.initLogStd);
					mLogStd = NkVar::Leaf(logStdT, true);
				}
			}

			float NkPolicyNet::Rand01() {
				mRng = mRng * 1664525u + 1013904223u;
				return (float)((mRng >> 8) & 0xFFFFFFu) / (float)0x1000000u; // [0,1)
			}

			float NkPolicyNet::RandNormal() {
				float u1 = Rand01();
				if (u1 < 1e-7f)
					u1 = 1e-7f; // évite log(0)
				const float u2 = Rand01();
				return (float)(std::sqrt(-2.0 * std::log((double)u1)) * std::cos(2.0 * kPi * (double)u2));
			}

			NkVar NkPolicyNet::Forward(const NkTensor &stateInput) const {
				NkVar x = NkVar::Leaf(stateInput, false);
				NkVar h = nn::Relu(mLayer1.Forward(x));
				NkVar raw = mLayer2.Forward(h); // [1, outputDim]
				if (mMode == NkPolicyMode::Discrete)
					return raw; // logits, AVANT softmax -- pas de saturation (softmax s'en charge déjà)
				// Continu : μ = actionScale·tanh(raw) -- borne la moyenne (cf NkPolicyNetConfig::actionScale).
				return autograd::MulScalar(nn::Tanh(raw), (double)mActionScale);
			}

			void NkPolicyNet::ClampLogStd(float lo, float hi) {
				if (mMode != NkPolicyMode::ContinuousGaussian || !mLogStd.IsValid())
					return;
				NkTensor v = mLogStd.Value().Clone();
				float *p = v.DataAs<float>();
				const int64 n = NkShapeNumel(v.Shape());
				for (int64 i = 0; i < n; ++i) {
					if (p[i] < lo)
						p[i] = lo;
					if (p[i] > hi)
						p[i] = hi;
				}
				mLogStd.SetValue(v);
			}

			void NkPolicyNet::Parameters(NkVector<NkVar> &out) const {
				mLayer1.Parameters(out);
				mLayer2.Parameters(out);
				if (mMode == NkPolicyMode::ContinuousGaussian)
					out.PushBack(mLogStd);
			}

			NkVar NkPolicyNet::LogProb(const NkTensor &stateInput, const NkTensor &action) const {
				if (mMode == NkPolicyMode::Discrete) {
					NkVar logits = Forward(stateInput);				// [1,numActions]
					NkVar probs = autograd::Softmax(logits);			// [1,numActions] (différentiable)
					NkVar logp = autograd::Log(probs);					// [1,numActions]
					// One-hot de l'action (indice encodé en float, comme SoftmaxCrossEntropyIndexed) —
					// c'est une donnée OBSERVÉE (pas un paramètre), Leaf sans gradient.
					NkVector<float> oneHotBuf;
					oneHotBuf.Resize(mOutputDim);
					for (uint32 c = 0; c < mOutputDim; ++c)
						oneHotBuf[c] = 0.0f;
					int64 idx = (int64)(action.GetItem(NkShape{(int64)0, (int64)0}) + 0.5);
					if (idx >= 0 && (uint32)idx < mOutputDim)
						oneHotBuf[(uint32)idx] = 1.0f;
					NkTensor oneHotT =
						NkTensor::FromData(NkShape{(int64)1, (int64)mOutputDim}, oneHotBuf.Data(), NkDType::NK_F32);
					NkVar mask = NkVar::Leaf(oneHotT, false);
					NkVar masked = autograd::Mul(logp, mask); // annule tout sauf logp[action]
					return autograd::Sum(masked);			   // -> scalaire = logp[action]
				}

				// ---- Continu : densité gaussienne diagonale, forme fermée ----
				// logp = Σ_d [ -0.5·(a_d-μ_d)²·exp(-2·logσ_d) - logσ_d ] - 0.5·D·log(2π)
				NkVar mean = Forward(stateInput);						  // [1,actionDim]
				NkVar actionVar = NkVar::Leaf(action, false);			  // donnée observée, pas de gradient
				NkVar diff = autograd::Sub(actionVar, mean);			  // (a - μ)
				NkVar diffSq = autograd::Mul(diff, diff);				  // (a - μ)²
				NkVar invVar = autograd::Exp(autograd::MulScalar(mLogStd, -2.0)); // exp(-2·logσ) = 1/σ²
				NkVar weighted = autograd::Mul(diffSq, invVar);		  // (a-μ)²/σ²
				NkVar halfNegWeighted = autograd::MulScalar(weighted, -0.5);
				NkVar negLogStd = autograd::MulScalar(mLogStd, -1.0);
				NkVar perDim = autograd::Add(halfNegWeighted, negLogStd); // [1,actionDim]
				NkVar summed = autograd::Sum(perDim);					   // scalaire
				const double constTerm = -0.5 * std::log(2.0 * kPi) * (double)mOutputDim;
				return autograd::AddScalar(summed, constTerm);
			}

			NkVar NkPolicyNet::Entropy(const NkTensor &stateInput) const {
				if (mMode == NkPolicyMode::Discrete) {
					NkVar logits = Forward(stateInput);
					NkVar probs = autograd::Softmax(logits);
					NkVar logp = autograd::Log(probs);
					NkVar prod = autograd::Mul(probs, logp); // p·log p par action
					NkVar s = autograd::Sum(prod);			  // Σ p·log p
					return autograd::MulScalar(s, -1.0);	  // entropie = -Σ p·log p
				}
				// Entropie d'une gaussienne diagonale (forme fermée, ne dépend que de σ) :
				//   H = Σ_d [ 0.5·(1+log(2π)) + logσ_d ] = D·0.5·(1+log(2π)) + Σ_d logσ_d
				NkVar s = autograd::Sum(mLogStd);
				const double constTerm = (double)mOutputDim * 0.5 * (1.0 + std::log(2.0 * kPi));
				return autograd::AddScalar(s, constTerm);
			}

			void NkPolicyNet::SampleAction(const NkTensor &stateInput, NkVector<float> &outAction, float &outLogProb) {
				outAction.Clear();
				if (mMode == NkPolicyMode::Discrete) {
					NkTensor logits = Forward(stateInput).Value(); // [1,numActions], lecture seule (pas de grad)
					NkTensor probs = autograd::Softmax(NkVar::Leaf(logits, false)).Value();
					const float u = Rand01();
					float cum = 0.0f;
					uint32 chosen = mOutputDim > 0 ? mOutputDim - 1 : 0;
					for (uint32 c = 0; c < mOutputDim; ++c) {
						cum += (float)probs.GetItem(NkShape{(int64)0, (int64)c});
						if (u < cum) {
							chosen = c;
							break;
						}
					}
					outAction.PushBack((float)chosen);
					NkTensor actionT = NkTensor::Full(NkShape{(int64)1, (int64)1}, (double)chosen);
					outLogProb = (float)LogProb(stateInput, actionT).Value().GetItem(NkShape{(int64)0});
					return;
				}

				NkTensor mean = Forward(stateInput).Value();		// [1,actionDim]
				NkTensor std = ops::Exp(mLogStd.Value());			// [1,actionDim] : σ = exp(logσ)
				NkVector<float> actionBuf;
				actionBuf.Resize(mOutputDim);
				for (uint32 d = 0; d < mOutputDim; ++d) {
					const float m = (float)mean.GetItem(NkShape{(int64)0, (int64)d});
					const float s = (float)std.GetItem(NkShape{(int64)0, (int64)d});
					const float z = RandNormal();
					actionBuf[d] = m + s * z; // a = μ + σ·z, z ~ N(0,1)
					outAction.PushBack(actionBuf[d]);
				}
				NkTensor actionT = NkTensor::FromData(NkShape{(int64)1, (int64)mOutputDim}, actionBuf.Data(),
													   NkDType::NK_F32);
				outLogProb = (float)LogProb(stateInput, actionT).Value().GetItem(NkShape{(int64)0});
			}

			void NkPolicyNet::GreedyAction(const NkTensor &stateInput, NkVector<float> &outAction) const {
				outAction.Clear();
				if (mMode == NkPolicyMode::Discrete) {
					NkTensor logits = Forward(stateInput).Value();
					NkTensor amax = ops::Argmax(logits, 1);
					int64 a = (int64)amax.GetItem(NkShape{(int64)0});
					if (a < 0)
						a = 0;
					if ((uint32)a >= mOutputDim)
						a = (int64)mOutputDim - 1;
					outAction.PushBack((float)a);
					return;
				}
				NkTensor mean = Forward(stateInput).Value();
				for (uint32 d = 0; d < mOutputDim; ++d)
					outAction.PushBack((float)mean.GetItem(NkShape{(int64)0, (int64)d}));
			}

		} // namespace rl
	} // namespace ai
} // namespace nkentseu
