#pragma once
/**
 * @File    NkPALizard.h
 * @Brief   Lézard procédural — corps 10 segs, queue 8 segs, 4 pattes IK 3 segs.
 */

#include "Animation/NkPAChain.h"
#include "Animation/NkPAAgent.h"
#include "Renderer/NkPAMesh.h"

namespace nkentseu {
namespace nkpa {

class Lizard {
public:
    void Init(NkVector2f startPos, int32 worldW, int32 worldH);
    void Update(float32 dt);
    void Draw(MeshBuilder& mb) const;

private:
    Chain   mBody;          ///< Corps principal (10 segments)
    Chain   mTail;          ///< Queue (8 segments)
    Chain   mLegFL;         ///< Patte avant gauche (3 segments)
    Chain   mLegFR;         ///< Patte avant droite (3 segments)
    Chain   mLegBL;         ///< Patte arrière gauche (3 segments)
    Chain   mLegBR;         ///< Patte arrière droite (3 segments)
    Agent   mAgent;
    float32 mTime = 0.f;

    // Couleur principale [0,1]
    float32 mCr = 0.4f, mCg = 0.5f, mCb = 0.1f;

    float32    BodyRadius(int32 i) const;
    NkVector2f LegAnchor(int32 bodyIdx) const;
    NkVector2f LegTarget(NkVector2f anchor, float32 ang, float32 side, float32 phase) const;
    void       DrawLeg(MeshBuilder& mb, const Chain& leg) const;
};

} // namespace nkpa
} // namespace nkentseu
