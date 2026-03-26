/**
 * @File    NkPAAgent.cpp
 * @Brief   Implémentation de l'agent autonome.
 */

#include "Animation/NkPAAgent.h"
#include <cmath>
#include <cstdlib>

namespace nkentseu {
namespace nkpa {

static float32 Randf(float32 lo, float32 hi) {
    return lo + (float32)rand() / (float32)RAND_MAX * (hi - lo);
}

void Agent::Init(NkVector2f startPos, float32 speed,
                 float32 undulAmp, float32 undulFreq,
                 int32 worldW, int32 worldH) {
    mPos        = startPos;
    mVel        = { 0.f, 0.f };
    mSpeed      = speed;
    mUndulAmp   = undulAmp;
    mUndulFreq  = undulFreq;
    mWorldW     = worldW;
    mWorldH     = worldH;
    mChangeDelay = Randf(2.f, 5.f);
    PickNewTarget();
}

void Agent::PickNewTarget() {
    float32 margin = 80.f;
    mTarget = { Randf(margin, (float32)mWorldW - margin),
                Randf(margin, (float32)mWorldH - margin) };
}

NkVector2f Agent::Update(float32 dt) {
    // Changement de cible périodique
    mChangeTimer += dt;
    if (mChangeTimer >= mChangeDelay) {
        mChangeTimer = 0.f;
        mChangeDelay = Randf(2.f, 5.f);
        PickNewTarget();
    }

    // Seek : orienter la vélocité vers la cible
    NkVector2f toTarget = mTarget - mPos;
    float32    dist     = toTarget.Len();
    if (dist > 5.f) {
        NkVector2f desired = toTarget.Normalized() * mSpeed;
        mVel = mVel.Lerp(desired, dt * 3.f);
    }

    // Ondulation latérale (perpendiculaire à la direction du mouvement)
    mUndulPhase += dt * mUndulFreq;
    NkVector2f fwd  = mVel.Normalized();
    NkVector2f side = fwd.Normal(); // rotation 90° anti-horaire
    float32    wave = ::sinf(mUndulPhase) * mUndulAmp;

    mPos = mPos + fwd * (mSpeed * dt) + side * (wave * dt * 60.f);

    // Angle de la tête (direction du déplacement)
    if (mVel.LenSq() > 0.001f)
        mHeadAngle = ::atan2f(mVel.y, mVel.x);

    // Rebond sur les bords
    const float32 W = (float32)mWorldW, H = (float32)mWorldH;
    const float32 M = 55.f;
    if (mPos.x < M)     { mPos.x = M;   mVel.x =  ::fabsf(mVel.x); mTarget.x = W - M; }
    if (mPos.x > W - M) { mPos.x = W-M; mVel.x = -::fabsf(mVel.x); mTarget.x = M;     }
    if (mPos.y < M)     { mPos.y = M;   mVel.y =  ::fabsf(mVel.y); mTarget.y = H - M; }
    if (mPos.y > H - M) { mPos.y = H-M; mVel.y = -::fabsf(mVel.y); mTarget.y = M;     }

    return mPos;
}

} // namespace nkpa
} // namespace nkentseu
