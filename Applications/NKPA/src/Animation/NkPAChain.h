#pragma once
/**
 * @File    NkPAChain.h
 * @Brief   Chaîne IK « Follow-the-leader » — base de toute créature procédurale.
 *
 * Algorithme :
 *   Segment 0 = tête (positionné sur la cible)
 *   Pour i = 1 .. N-1 :
 *     dir = normalize(segs[i].pos - segs[i-1].pos)
 *     segs[i].pos = segs[i-1].pos + dir * segLen
 */

#include "NKCore/NkTypes.h"
#include "NKMath/NkVec.h"

namespace nkentseu {
namespace nkpa {

using namespace math;

// ─── Segment individuel ───────────────────────────────────────────────────────

struct Segment {
    NkVector2f pos;
    float32    angle = 0.f; ///< Angle en radians (calculé depuis la direction)
};

// ─── Chaîne de segments ───────────────────────────────────────────────────────

class Chain {
public:
    static constexpr int32 MAX_SEGMENTS = 48;

    void Init(NkVector2f startPos, int32 count, float32 segmentLength);

    /// Déplace la tête vers target, résout la contrainte IK et met à jour les angles.
    void Update(NkVector2f target);

    int32       Count()   const { return mCount; }
    float32     SegLen()  const { return mSegLen; }
    Segment&    Seg(int32 i)       { return mSegs[i]; }
    const Segment& Seg(int32 i) const { return mSegs[i]; }

    NkVector2f HeadPos() const { return mSegs[0].pos; }
    NkVector2f TailPos() const { return mSegs[mCount > 0 ? mCount-1 : 0].pos; }

private:
    Segment mSegs[MAX_SEGMENTS];
    int32   mCount  = 0;
    float32 mSegLen = 20.f;
};

} // namespace nkpa
} // namespace nkentseu
