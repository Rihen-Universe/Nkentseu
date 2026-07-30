// =============================================================================
// NkDQN.cpp — implémentation du Deep Q-Network (NKAI, Phase 4, Jalon 2).
// =============================================================================
#include "NKRL/NkDQN.h"
#include "NKTensor/NkTensorOps.h"

namespace nkentseu {
	namespace ai {
		namespace rl {

			NkDQN::NkDQN(uint32 numStates, uint32 numActions, const NkDQNConfig &config)
				: mNumStates(numStates), mNumActions(numActions), mConfig(config),
				  mOnline1(numStates, config.hiddenSize, (config.seed ? config.seed : 1u) * 4u + 1u),
				  mOnline2(config.hiddenSize, numActions, (config.seed ? config.seed : 1u) * 4u + 2u),
				  mTarget1(numStates, config.hiddenSize, (config.seed ? config.seed : 1u) * 4u + 3u),
				  mTarget2(config.hiddenSize, numActions, (config.seed ? config.seed : 1u) * 4u + 4u),
				  mReplay(config.replayCapacity ? config.replayCapacity : 1, config.seed ? config.seed : 1u),
				  mEpsilon(1.0f), mRng((config.seed ? config.seed : 1u) * 4u + 9u), mGradientSteps(0) {
				mOnline1.Parameters(mOnlineParams);
				mOnline2.Parameters(mOnlineParams);
				mOptim = optim::NkAdam(mOnlineParams, config.lr);
				SyncTarget(); // synchro dure initiale : cible = copie exacte du réseau en ligne
			}

			float NkDQN::Rand01() {
				mRng = mRng * 1664525u + 1013904223u;
				return (float)((mRng >> 8) & 0xFFFFFFu) / (float)0x1000000u; // [0,1)
			}

			NkTensor NkDQN::EncodeOneHot(uint32 state) const {
				NkVector<float> buf;
				buf.Resize(mNumStates);
				for (uint32 i = 0; i < mNumStates; ++i)
					buf[i] = 0.0f;
				if (state < mNumStates)
					buf[state] = 1.0f;
				return NkTensor::FromData(NkShape{(int64)1, (int64)mNumStates}, buf.Data(), NkDType::NK_F32);
			}

			NkTensor NkDQN::ForwardOnlineRaw(const NkTensor &input) const {
				NkTensor h = ops::Relu(ops::Add(ops::Matmul(input, mOnline1.Weight().Value()), mOnline1.Bias().Value()));
				return ops::Add(ops::Matmul(h, mOnline2.Weight().Value()), mOnline2.Bias().Value());
			}

			NkTensor NkDQN::ForwardTargetRaw(const NkTensor &input) const {
				NkTensor h = ops::Relu(ops::Add(ops::Matmul(input, mTarget1.Weight().Value()), mTarget1.Bias().Value()));
				return ops::Add(ops::Matmul(h, mTarget2.Weight().Value()), mTarget2.Bias().Value());
			}

			void NkDQN::SyncTarget() {
				NkVector<NkVar> onlineP;
				NkVector<NkVar> targetP;
				mOnline1.Parameters(onlineP);
				mOnline2.Parameters(onlineP);
				mTarget1.Parameters(targetP);
				mTarget2.Parameters(targetP);
				const uint32 n = onlineP.Size() < targetP.Size() ? (uint32)onlineP.Size() : (uint32)targetP.Size();
				for (uint32 i = 0; i < n; ++i)
					targetP[i].SetValue(onlineP[i].Value().Clone());
			}

			uint32 NkDQN::SelectGreedy(uint32 state) const {
				NkTensor input = EncodeOneHot(state);
				NkTensor q = ForwardOnlineRaw(input);	 // [1, numActions]
				NkTensor amax = ops::Argmax(q, 1);		 // [1] (I64)
				int64 a = (int64)amax.GetItem(NkShape{(int64)0});
				if (a < 0)
					a = 0;
				if ((uint32)a >= mNumActions)
					a = (int64)mNumActions - 1;
				return (uint32)a;
			}

			uint32 NkDQN::SelectAction(uint32 state) {
				if (Rand01() < mEpsilon) {
					uint32 a = (uint32)(Rand01() * (float)mNumActions);
					if (a >= mNumActions)
						a = mNumActions - 1;
					return a;
				}
				return SelectGreedy(state);
			}

			void NkDQN::Remember(uint32 state, uint32 action, float reward, uint32 nextState, bool done) {
				mReplay.Push(state, action, reward, nextState, done);
			}

			bool NkDQN::TrainStep() {
				if (mReplay.Size() < mConfig.minReplaySize || mReplay.IsEmpty())
					return false;

				const nk_size batchSize = mConfig.batchSize < mReplay.Size() ? mConfig.batchSize : mReplay.Size();
				NkVector<nk_size> idx;
				mReplay.Sample(batchSize, idx);
				const uint32 B = (uint32)idx.Size();
				if (B == 0)
					return false;

				// ---- Construit les entrées one-hot batchées + le masque d'action jouée ----
				NkVector<float> stateBuf;
				stateBuf.Resize((nk_size)B * mNumStates);
				NkVector<float> nextStateBuf;
				nextStateBuf.Resize((nk_size)B * mNumStates);
				for (nk_size i = 0; i < stateBuf.Size(); ++i) {
					stateBuf[i] = 0.0f;
					nextStateBuf[i] = 0.0f;
				}

				NkVector<float> actionMaskBuf;
				actionMaskBuf.Resize((nk_size)B * mNumActions);
				for (nk_size i = 0; i < actionMaskBuf.Size(); ++i)
					actionMaskBuf[i] = 0.0f;

				NkVector<float> rewardBuf;
				rewardBuf.Resize(B);
				NkVector<float> doneBuf;
				doneBuf.Resize(B);

				for (uint32 i = 0; i < B; ++i) {
					const NkReplayTransition &t = mReplay.At(idx[i]);
					if (t.state < mNumStates)
						stateBuf[(nk_size)i * mNumStates + t.state] = 1.0f;
					if (t.nextState < mNumStates)
						nextStateBuf[(nk_size)i * mNumStates + t.nextState] = 1.0f;
					if (t.action < mNumActions)
						actionMaskBuf[(nk_size)i * mNumActions + t.action] = 1.0f;
					rewardBuf[i] = t.reward;
					doneBuf[i] = t.done ? 1.0f : 0.0f;
				}

				NkTensor stateT = NkTensor::FromData(NkShape{(int64)B, (int64)mNumStates}, stateBuf.Data(), NkDType::NK_F32);
				NkTensor nextStateT =
					NkTensor::FromData(NkShape{(int64)B, (int64)mNumStates}, nextStateBuf.Data(), NkDType::NK_F32);
				NkTensor maskT =
					NkTensor::FromData(NkShape{(int64)B, (int64)mNumActions}, actionMaskBuf.Data(), NkDType::NK_F32);

				// ---- Cible de Bellman via le réseau CIBLE (aucun gradient) ----
				// y = r + γ·max_a' Qcible(s',a')·(1-terminé)
				NkTensor qNextTarget = ForwardTargetRaw(nextStateT); // [B, numActions]
				NkTensor maxQNext = ops::Max(qNextTarget, 1);		  // [B]
				NkVector<float> targetBuf;
				targetBuf.Resize(B);
				for (uint32 i = 0; i < B; ++i) {
					double mq = maxQNext.GetItem(NkShape{(int64)i});
					double y = (double)rewardBuf[i];
					if (doneBuf[i] < 0.5f)
						y += (double)mConfig.gamma * mq;
					targetBuf[i] = (float)y;
				}
				NkTensor targetT = NkTensor::FromData(NkShape{(int64)B, (int64)1}, targetBuf.Data(), NkDType::NK_F32);

				// Colonne de 1 : réduit [B,numActions] masqué -> [B,1] par Matmul
				// (différentiable), fait office de "gather" de l'action jouée.
				NkVector<float> onesBuf;
				onesBuf.Resize(mNumActions);
				for (uint32 i = 0; i < mNumActions; ++i)
					onesBuf[i] = 1.0f;
				NkTensor onesColT = NkTensor::FromData(NkShape{(int64)mNumActions, (int64)1}, onesBuf.Data(), NkDType::NK_F32);

				// ---- Forward EN LIGNE (différentiable) : Q(s,·) -> masque -> Q(s,a) prédite ----
				NkVar input = NkVar::Leaf(stateT, false);
				NkVar h = nn::Relu(mOnline1.Forward(input));
				NkVar qAll = mOnline2.Forward(h); // [B, numActions]
				NkVar mask = NkVar::Leaf(maskT, false);
				NkVar masked = autograd::Mul(qAll, mask); // [B, numActions]
				NkVar onesCol = NkVar::Leaf(onesColT, false);
				NkVar qSelected = autograd::Matmul(masked, onesCol); // [B,1] = Q(s,a) prédite

				NkVar target = NkVar::Leaf(targetT, false);
				NkVar loss = autograd::MSE(qSelected, target); // scalaire

				mOptim.ZeroGrad();
				loss.Backward();
				mOptim.Step();

				++mGradientSteps;
				if (mConfig.targetSyncInterval > 0 && (mGradientSteps % mConfig.targetSyncInterval) == 0)
					SyncTarget();

				return true;
			}

		} // namespace rl
	} // namespace ai
} // namespace nkentseu
