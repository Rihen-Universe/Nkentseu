// =============================================================================
// NkPPO.cpp — implémentation de PPO (NKAI, Phase 4, Jalon 3). Cf NkPPO.h pour
// les références (Schulman et al. 2017 pour PPO, Schulman et al. 2016 pour GAE).
// =============================================================================
#include "NKRL/NkPPO.h"
#include "NKTensor/NkTensorOps.h"

#include <cmath>

namespace nkentseu {
	namespace ai {
		namespace rl {

			NkPPO::NkPPO(NkPolicyMode mode, uint32 inputDim, uint32 numActionsOrActionDim, const NkPPOConfig &config)
				: mPolicy(mode, inputDim, numActionsOrActionDim,
						  NkPolicyNetConfig{config.hiddenSize, config.initLogStd, config.seed, config.actionScale}),
				  mValue1(inputDim, config.hiddenSize, (config.seed ? config.seed : 1u) * 4u + 3u),
				  mValue2(config.hiddenSize, 1, (config.seed ? config.seed : 1u) * 4u + 4u), mConfig(config),
				  mInputDim(inputDim), mUpdateCount(0) {
				mPolicy.Parameters(mPolicyParams);
				mValue1.Parameters(mValueParams);
				mValue2.Parameters(mValueParams);
				mPolicyOptim = optim::NkAdam(mPolicyParams, config.policyLr);
				mValueOptim = optim::NkAdam(mValueParams, config.valueLr);
			}

			NkTensor NkPPO::EncodeState(const NkVector<float> &state) const {
				return NkTensor::FromData(NkShape{(int64)1, (int64)mInputDim}, state.Data(), NkDType::NK_F32);
			}

			NkVar NkPPO::ForwardValue(const NkTensor &stateInput) const {
				NkVar x = NkVar::Leaf(stateInput, false);
				NkVar h = nn::Relu(mValue1.Forward(x));
				return mValue2.Forward(h); // [1,1]
			}

			void NkPPO::SelectAction(const NkVector<float> &state, NkVector<float> &outAction, float &outLogProb,
									 float &outValue) {
				NkTensor stateT = EncodeState(state);
				mPolicy.SampleAction(stateT, outAction, outLogProb);
				outValue = (float)ForwardValue(stateT).Value().GetItem(NkShape{(int64)0, (int64)0});
			}

			void NkPPO::GreedyAction(const NkVector<float> &state, NkVector<float> &outAction) const {
				NkTensor stateT = EncodeState(state);
				mPolicy.GreedyAction(stateT, outAction);
			}

			void NkPPO::Remember(const NkVector<float> &state, const NkVector<float> &action, float reward,
								 bool done, float logProbOld, float value) {
				mStates.PushBack(state);
				mActions.PushBack(action);
				mRewards.PushBack(reward);
				mDones.PushBack(done);
				mLogProbsOld.PushBack(logProbOld);
				mValuesOld.PushBack(value);
			}

			void NkPPO::ClearRollout() {
				mStates.Clear();
				mActions.Clear();
				mRewards.Clear();
				mDones.Clear();
				mLogProbsOld.Clear();
				mValuesOld.Clear();
			}

			bool NkPPO::TrainStepIfReady(const NkVector<float> &lastState, bool forceUpdate) {
				if (mStates.Size() == 0)
					return false;
				if (mStates.Size() < mConfig.rolloutSize && !forceUpdate)
					return false;
				Update(lastState);
				ClearRollout();
				++mUpdateCount;
				return true;
			}

			void NkPPO::Update(const NkVector<float> &lastState) {
				const nk_size T = mStates.Size();
				if (T == 0)
					return;

				// ---- Bootstrap V(s_T) : nécessaire au GAE si le rollout est coupé en cours
				// d'épisode (dernière transition non terminale). Nul si l'épisode s'est terminé. ----
				float bootstrapValue = 0.0f;
				if (!mDones[T - 1]) {
					NkTensor lastT = EncodeState(lastState);
					bootstrapValue = (float)ForwardValue(lastT).Value().GetItem(NkShape{(int64)0, (int64)0});
				}

				// ---- GAE (Schulman et al. 2016, arXiv:1506.02438), récursion ARRIÈRE ----
				NkVector<float> advantages;
				advantages.Resize(T);
				NkVector<float> returns;
				returns.Resize(T);
				float nextValue = bootstrapValue;
				float nextAdv = 0.0f;
				for (nk_size i = 0; i < T; ++i) {
					const nk_size ti = T - 1 - i; // parcours T-1 .. 0
					const float notDone = mDones[ti] ? 0.0f : 1.0f;
					const float delta = mRewards[ti] + mConfig.gamma * nextValue * notDone - mValuesOld[ti];
					const float adv = delta + mConfig.gamma * mConfig.gaeLambda * notDone * nextAdv;
					advantages[ti] = adv;
					returns[ti] = adv + mValuesOld[ti];
					nextValue = mValuesOld[ti];
					nextAdv = adv;
				}

				// Normalisation des avantages (moyenne 0, écart-type 1) : stabilise l'échelle du
				// gradient de politique d'un rollout à l'autre — pratique standard PPO.
				double meanAdv = 0.0;
				for (nk_size i = 0; i < T; ++i)
					meanAdv += (double)advantages[i];
				meanAdv /= (double)T;
				double varAdv = 0.0;
				for (nk_size i = 0; i < T; ++i) {
					const double d = (double)advantages[i] - meanAdv;
					varAdv += d * d;
				}
				varAdv /= (double)T;
				const double stdAdv = std::sqrt(varAdv) + 1e-8;
				for (nk_size i = 0; i < T; ++i)
					advantages[i] = (float)(((double)advantages[i] - meanAdv) / stdAdv);

				// ---- `epochs` passes sur le MÊME rollout (avantages/retours FIXES, calculés une
				// seule fois ci-dessus) : ratio PPO, objectif clippé, bonus d'entropie, MSE critique.
				for (uint32 epoch = 0; epoch < mConfig.epochs; ++epoch) {
					NkVar policyLossSum;
					bool havePolicyLoss = false;
					NkVar valueLossSum;
					bool haveValueLoss = false;

					for (nk_size ti = 0; ti < T; ++ti) {
						NkTensor stateT = EncodeState(mStates[ti]);
						NkTensor actionT =
							(mPolicy.Mode() == NkPolicyMode::Discrete)
								? NkTensor::Full(NkShape{(int64)1, (int64)1}, (double)mActions[ti][0])
								: NkTensor::FromData(NkShape{(int64)1, (int64)mActions[ti].Size()},
													  mActions[ti].Data(), NkDType::NK_F32);

						// ---- Ratio r_t = exp(logp_new − logp_old) ----
						NkVar logpNew = mPolicy.LogProb(stateT, actionT); // différentiable (politique COURANTE)
						const float logpNewVal = (float)logpNew.Value().GetItem(NkShape{(int64)0});
						const float ratioVal = (float)std::exp((double)logpNewVal - (double)mLogProbsOld[ti]);
						const float A = advantages[ti];

						const float lo = 1.0f - mConfig.clipEps;
						const float hi = 1.0f + mConfig.clipEps;
						const float clippedRatio = ratioVal < lo ? lo : (ratioVal > hi ? hi : ratioVal);
						const float surr1 = ratioVal * A;
						const float surr2 = clippedRatio * A;

						// L^CLIP = min(surr1,surr2) ; perte = −L^CLIP. Le terme ACTIF (le min) porte
						// le gradient réel si c'est le terme NON clippé (surr1) ; s'il s'agit du terme
						// clippé (surr2), il est traité comme une CONSTANTE (Leaf sans gradient) :
						// comportement standard de l'objectif clippé PPO (cf NkPPO.h).
						NkVar sampleLoss;
						if (surr1 <= surr2) {
							NkVar logpOldLeaf =
								NkVar::Leaf(NkTensor::Full(NkShape{(int64)1}, (double)mLogProbsOld[ti]), false);
							NkVar ratioVar = autograd::Exp(autograd::Sub(logpNew, logpOldLeaf));
							sampleLoss = autograd::MulScalar(ratioVar, -(double)A); // -surr1
						} else {
							sampleLoss = NkVar::Leaf(NkTensor::Full(NkShape{(int64)1}, -(double)surr2), false);
						}

						// Bonus d'entropie (terme S, ajouté INCONDITIONNELLEMENT -- pas soumis au clip).
						NkVar entropy = mPolicy.Entropy(stateT);
						NkVar sampleTotal =
							autograd::Sub(sampleLoss, autograd::MulScalar(entropy, (double)mConfig.entropyCoef));

						policyLossSum = havePolicyLoss ? autograd::Add(policyLossSum, sampleTotal) : sampleTotal;
						havePolicyLoss = true;

						// ---- Perte critique : MSE(V_new(s_t), return_t) ----
						NkVar vNew = ForwardValue(stateT); // différentiable (critique COURANT)
						NkVar returnLeaf =
							NkVar::Leaf(NkTensor::Full(NkShape{(int64)1, (int64)1}, (double)returns[ti]), false);
						NkVar vLoss = autograd::MSE(vNew, returnLeaf);
						valueLossSum = haveValueLoss ? autograd::Add(valueLossSum, vLoss) : vLoss;
						haveValueLoss = true;
					}

					if (havePolicyLoss) {
						NkVar policyLoss = autograd::MulScalar(policyLossSum, 1.0 / (double)T);
						mPolicyOptim.ZeroGrad();
						policyLoss.Backward();
						mPolicyOptim.Step();
						// Garde-fou (continu seulement, no-op en discret) : empêche le bonus
						// d'entropie de faire diverger σ (cf NkPolicyNet::ClampLogStd).
						mPolicy.ClampLogStd(-2.0f, 1.0f);
					}
					if (haveValueLoss) {
						NkVar valueLoss = autograd::MulScalar(valueLossSum, 1.0 / (double)T);
						mValueOptim.ZeroGrad();
						valueLoss.Backward();
						mValueOptim.Step();
					}
				}
			}

		} // namespace rl
	} // namespace ai
} // namespace nkentseu
