#pragma once
/**
 * @File    NkPACat.h
 * @Brief   Chat procédural — corps souple 8 segments, queue longue, 4 pattes IK, tête avec oreilles.
 */

#include "Animation/NkPAChain.h"
#include "Animation/NkPAAgent.h"
#include "Renderer/NkPAMesh.h"

namespace nkentseu {
namespace nkpa {

class Cat {
public:
    void Init(NkVector2f startPos, int32 worldW, int32 worldH);
    void Update(float32 dt);
    void Draw(MeshBuilder& mb) const;

private:
    Chain   mBody;
    Chain   mTail;
    Chain   mLegFL, mLegFR, mLegBL, mLegBR;
    Agent   mAgent;
    float32 mTime = 0.f;

    float32 mCr = 0.75f, mCg = 0.6f, mCb = 0.5f; // couleur fourrure

    float32    BodyRadius(int32 i) const;
    NkVector2f LegTarget(NkVector2f anchor, float32 ang, float32 side, float32 phase) const;
    void       DrawLeg(MeshBuilder& mb, const Chain& leg) const;
};

} // namespace nkpa
} // namespace nkentseu
