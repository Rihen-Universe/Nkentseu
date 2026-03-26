/**
 * @File    NkPAChain.cpp
 * @Brief   Implémentation de la chaîne IK.
 */

#include "Animation/NkPAChain.h"
#include <cmath>

namespace nkentseu {
namespace nkpa {

void Chain::Init(NkVector2f startPos, int32 count, float32 segmentLength) {
    mCount  = (count < MAX_SEGMENTS) ? count : MAX_SEGMENTS;
    mSegLen = segmentLength;
    for (int32 i = 0; i < mCount; ++i) {
        mSegs[i].pos   = { startPos.x, startPos.y + (float32)i * segmentLength };
        mSegs[i].angle = 0.f;
    }
}

void Chain::Update(NkVector2f target) {
    if (mCount < 1) return;

    // Tête suit la cible
    mSegs[0].pos = target;

    // Contrainte IK sur les segments suivants
    for (int32 i = 1; i < mCount; ++i) {
        NkVector2f dir = (mSegs[i].pos - mSegs[i-1].pos).Normalized();
        mSegs[i].pos   = mSegs[i-1].pos + dir * mSegLen;
    }

    // Calcul des angles (direction de chaque segment)
    for (int32 i = 0; i < mCount - 1; ++i) {
        NkVector2f d   = mSegs[i+1].pos - mSegs[i].pos;
        mSegs[i].angle = ::atan2f(d.y, d.x);
    }
    if (mCount > 1)
        mSegs[mCount-1].angle = mSegs[mCount-2].angle;
    else
        mSegs[0].angle = 0.f;
}

} // namespace nkpa
} // namespace nkentseu
