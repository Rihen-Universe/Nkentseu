// =============================================================================
// NkReach2DMulti.cpp — implémentation du monde multi-agent (NKAI, Phase 4).
// =============================================================================
#include "NKRL/NkReach2DMulti.h"

#include <cmath>

namespace nkentseu {
	namespace ai {
		namespace rl {

			namespace {
				float Clampf(float v, float lo, float hi) {
					if (v < lo)
						return lo;
					if (v > hi)
						return hi;
					return v;
				}

				float Dist(float x0, float y0, float x1, float y1) {
					const float dx = x1 - x0;
					const float dy = y1 - y0;
					return (float)std::sqrt((double)(dx * dx + dy * dy));
				}
			} // namespace

			NkReach2DMulti::NkReach2DMulti(uint32 numAgents, float worldSize, float maxStep, float reachRadius,
										   float stepCost, float distScale, float collisionRadius,
										   float collisionPenalty, uint32 seed)
				: mNumAgents(numAgents ? numAgents : 1), mWorldSize(worldSize > 0.0f ? worldSize : 1.0f),
				  mMaxStep(maxStep > 0.0f ? maxStep : 0.1f), mReachRadius(reachRadius > 0.0f ? reachRadius : 0.1f),
				  mStepCost(stepCost), mDistScale(distScale), mCollisionRadius(collisionRadius),
				  mCollisionPenalty(collisionPenalty), mRng(seed ? seed : 1u) {
				mX.Resize(mNumAgents);
				mY.Resize(mNumAgents);
				mTx.Resize(mNumAgents);
				mTy.Resize(mNumAgents);
				mDone.Resize(mNumAgents);
			}

			float NkReach2DMulti::Rand01() {
				mRng = mRng * 1664525u + 1013904223u;
				return (float)((mRng >> 8) & 0xFFFFFFu) / (float)0x1000000u; // [0,1)
			}

			float NkReach2DMulti::DistanceToTarget(uint32 agent) const {
				if (agent >= mNumAgents)
					return 0.0f;
				return Dist(mX[agent], mY[agent], mTx[agent], mTy[agent]);
			}

			// Observation NORMALISÉE dans [-1,1] (même justification que NkReach2D::Norm) : positions
			// propres par worldSize, positions relatives des autres agents par worldSize également
			// (bornées à [-1,1] par construction, cf en-tête NkReach2DMulti.h).
			void NkReach2DMulti::BuildObservation(uint32 agent, NkVector<float> &out) const {
				out.Clear();
				out.PushBack(2.0f * mX[agent] / mWorldSize - 1.0f);
				out.PushBack(2.0f * mY[agent] / mWorldSize - 1.0f);
				out.PushBack(2.0f * mTx[agent] / mWorldSize - 1.0f);
				out.PushBack(2.0f * mTy[agent] / mWorldSize - 1.0f);
				for (uint32 j = 0; j < mNumAgents; ++j) {
					if (j == agent)
						continue;
					out.PushBack((mX[j] - mX[agent]) / mWorldSize);
					out.PushBack((mY[j] - mY[agent]) / mWorldSize);
				}
			}

			void NkReach2DMulti::Reset(NkVector<NkVector<float>> &states) {
				// Positions de départ distinctes (rejet si trop proches d'un agent déjà placé),
				// cibles tirées au hasard (rejet si trop proches du départ de LEUR propre agent).
				// Tentatives bornées : le monde est assez grand (worldSize) pour que ça converge vite.
				for (uint32 i = 0; i < mNumAgents; ++i) {
					float x = 0.0f, y = 0.0f;
					bool ok = false;
					for (uint32 attempt = 0; attempt < 50 && !ok; ++attempt) {
						x = Rand01() * mWorldSize;
						y = Rand01() * mWorldSize;
						ok = true;
						for (uint32 j = 0; j < i; ++j) {
							if (Dist(x, y, mX[j], mY[j]) < mCollisionRadius) {
								ok = false;
								break;
							}
						}
					}
					mX[i] = x;
					mY[i] = y;
					mDone[i] = false;
				}
				for (uint32 i = 0; i < mNumAgents; ++i) {
					float tx = 0.0f, ty = 0.0f;
					for (uint32 attempt = 0; attempt < 50; ++attempt) {
						tx = Rand01() * mWorldSize;
						ty = Rand01() * mWorldSize;
						if (Dist(tx, ty, mX[i], mY[i]) >= 2.0f * mReachRadius)
							break;
					}
					mTx[i] = tx;
					mTy[i] = ty;
				}

				states.Clear();
				for (uint32 i = 0; i < mNumAgents; ++i) {
					NkVector<float> obs;
					BuildObservation(i, obs);
					states.PushBack(obs);
				}
			}

			void NkReach2DMulti::Step(const NkVector<NkVector<float>> &actions, NkVector<NkVector<float>> &nextStates,
									  NkVector<float> &rewards, NkVector<bool> &dones) {
				// Passe 1 : déplace tous les agents ACTIFS (les `done` restent gelés). Toutes les
				// positions du pas de temps sont connues avant qu'un agent ne calcule sa récompense
				// (simultanéité réelle -- pas de mise à jour séquentielle biaisée par l'ordre).
				for (uint32 i = 0; i < mNumAgents; ++i) {
					if (mDone[i])
						continue;
					const NkVector<float> &a = actions[i];
					const float dx = Clampf(a.Size() > 0 ? a[0] : 0.0f, -mMaxStep, mMaxStep);
					const float dy = Clampf(a.Size() > 1 ? a[1] : 0.0f, -mMaxStep, mMaxStep);
					mX[i] = Clampf(mX[i] + dx, 0.0f, mWorldSize);
					mY[i] = Clampf(mY[i] + dy, 0.0f, mWorldSize);
				}

				// Passe 2 : récompenses (distance au but + pénalité de collision), à partir des
				// positions déjà toutes mises à jour.
				rewards.Clear();
				dones.Clear();
				nextStates.Clear();
				for (uint32 i = 0; i < mNumAgents; ++i) {
					if (mDone[i]) {
						NkVector<float> obs;
						BuildObservation(i, obs);
						nextStates.PushBack(obs);
						rewards.PushBack(0.0f); // agent gelé : plus de signal (cf en-tête)
						dones.PushBack(true);
						continue;
					}

					bool colliding = false;
					for (uint32 j = 0; j < mNumAgents; ++j) {
						if (j == i)
							continue;
						if (Dist(mX[i], mY[i], mX[j], mY[j]) < mCollisionRadius) {
							colliding = true;
							break;
						}
					}

					const float dist = DistanceToTarget(i);
					float r;
					bool d;
					if (dist <= mReachRadius) {
						r = 1.0f; // but atteint (même convention que NkReach2D mono-agent)
						d = true;
						mDone[i] = true;
					} else {
						r = -mStepCost - mDistScale * dist;
						if (colliding)
							r -= mCollisionPenalty;
						d = false;
					}

					NkVector<float> obs;
					BuildObservation(i, obs);
					nextStates.PushBack(obs);
					rewards.PushBack(r);
					dones.PushBack(d);
				}
			}

		} // namespace rl
	} // namespace ai
} // namespace nkentseu
