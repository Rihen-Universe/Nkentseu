// =============================================================================
// NkReach2D.cpp — implémentation du monde à actions continues (NKAI, Phase 4).
// =============================================================================
#include "NKRL/NkReach2D.h"

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
			} // namespace

			NkReach2D::NkReach2D(float worldSize, float maxStep, float reachRadius, float stepCost, float distScale,
								 uint32 seed)
				: mWorldSize(worldSize > 0.0f ? worldSize : 1.0f), mMaxStep(maxStep > 0.0f ? maxStep : 0.1f),
				  mReachRadius(reachRadius > 0.0f ? reachRadius : 0.1f), mStepCost(stepCost), mDistScale(distScale),
				  mRng(seed ? seed : 1u), mX(0.0f), mY(0.0f), mTx(0.0f), mTy(0.0f) {
			}

			float NkReach2D::Rand01() {
				mRng = mRng * 1664525u + 1013904223u;
				return (float)((mRng >> 8) & 0xFFFFFFu) / (float)0x1000000u; // [0,1)
			}

			float NkReach2D::Norm(float v) const {
				return 2.0f * v / mWorldSize - 1.0f; // [0,worldSize] -> [-1,1]
			}

			void NkReach2D::FillObs(NkVector<float> &obs) const {
				obs.Clear();
				obs.PushBack(Norm(mX));
				obs.PushBack(Norm(mY));
				obs.PushBack(Norm(mTx));
				obs.PushBack(Norm(mTy));
			}

			void NkReach2D::Reset(NkVector<float> &state) {
				// Position de départ ET cible tirées au hasard (indépendamment) dans le monde :
				// évite qu'une politique mémorise une trajectoire fixe plutôt que d'apprendre à
				// atteindre une cible arbitraire (généralisation minimale mais réelle).
				mX = Rand01() * mWorldSize;
				mY = Rand01() * mWorldSize;
				mTx = Rand01() * mWorldSize;
				mTy = Rand01() * mWorldSize;
				// Redistribue la cible si elle tombe trop près du départ (épisode trivial).
				float dx = mTx - mX;
				float dy = mTy - mY;
				while (std::sqrt((double)(dx * dx + dy * dy)) < (double)(2.0f * mReachRadius)) {
					mTx = Rand01() * mWorldSize;
					mTy = Rand01() * mWorldSize;
					dx = mTx - mX;
					dy = mTy - mY;
				}
				FillObs(state);
			}

			float NkReach2D::DistanceToTarget() const {
				const float dx = mTx - mX;
				const float dy = mTy - mY;
				return (float)std::sqrt((double)(dx * dx + dy * dy));
			}

			void NkReach2D::Step(const NkVector<float> &action, NkVector<float> &nextState, float &reward,
								 bool &done) {
				const float dx = Clampf(action.Size() > 0 ? action[0] : 0.0f, -mMaxStep, mMaxStep);
				const float dy = Clampf(action.Size() > 1 ? action[1] : 0.0f, -mMaxStep, mMaxStep);

				mX = Clampf(mX + dx, 0.0f, mWorldSize);
				mY = Clampf(mY + dy, 0.0f, mWorldSize);

				const float dist = DistanceToTarget();

				FillObs(nextState);

				if (dist <= mReachRadius) {
					reward = 1.0f; // cible atteinte : but final (même convention que les grilles Jalon 1/2)
					done = true;
				} else {
					reward = -mStepCost - mDistScale * dist; // coût de pas + terme dense (rapprochement)
					done = false;
				}
			}

		} // namespace rl
	} // namespace ai
} // namespace nkentseu
