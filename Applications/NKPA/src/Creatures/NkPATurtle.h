#pragma once
/**
 * @File    NkPATurtle.h
 * @Brief   Tortue procédurale — corps, carapace, 4 pattes IK, tête allongée, zone mixte.
 */

#include "Animation/NkPAChain.h"
#include "Animation/NkPAAgent.h"
#include "Renderer/NkPAMesh.h"

namespace nkentseu {
namespace nkpa {

class Turtle {
public:
    void Init(NkVector2f startPos, int32 worldW, int32 worldH);
    void Update(float32 dt);
    void Draw(MeshBuilder& mb) const;

private:
    Chain   mBody;
    Chain   mLegFL, mLegFR, mLegBL, mLegBR;
    Chain   mNeck;   ///< Cou (3 segments)
    Agent   mAgent;
    float32 mTime = 0.f;

    float32 mShellR = 0.35f, mShellG = 0.55f, mShellB = 0.15f;
    float32 mSkinR  = 0.25f, mSkinG  = 0.45f, mSkinB  = 0.1f;

    NkVector2f LegTarget(NkVector2f anchor, float32 ang, float32 side, float32 phase) const;
    void       DrawLeg(MeshBuilder& mb, const Chain& leg) const;
};

} // namespace nkpa
} // namespace nkentseu
