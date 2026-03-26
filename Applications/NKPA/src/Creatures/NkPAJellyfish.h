#pragma once
/**
 * @File    NkPAJellyfish.h
 * @Brief   Méduse procédurale — cloche pulsante, 6 tentacules traînantes.
 */

#include "Animation/NkPAChain.h"
#include "Animation/NkPAAgent.h"
#include "Renderer/NkPAMesh.h"

namespace nkentseu {
namespace nkpa {

static constexpr int32 JELLYFISH_TENTACLES = 6;

class Jellyfish {
public:
    void Init(NkVector2f startPos, int32 worldW, int32 worldH);
    void Update(float32 dt);
    void Draw(MeshBuilder& mb) const;

private:
    Agent   mAgent;
    Chain   mTentacles[JELLYFISH_TENTACLES];
    float32 mTime = 0.f;

    float32 mCr = 0.8f, mCg = 0.3f, mCb = 0.9f;
    float32 mBellRadius = 30.f;
};

} // namespace nkpa
} // namespace nkentseu
