#pragma once
/**
 * @File    NkPACaterpillar.h
 * @Brief   Chenille procédurale — 10 segments ronds, pattes courtes, tête avec yeux et antennes.
 */

#include "Animation/NkPAChain.h"
#include "Animation/NkPAAgent.h"
#include "Renderer/NkPAMesh.h"

namespace nkentseu {
namespace nkpa {

class Caterpillar {
public:
    void Init(NkVector2f startPos, int32 worldW, int32 worldH);
    void Update(float32 dt);
    void Draw(MeshBuilder& mb) const;

private:
    Chain   mBody;
    Agent   mAgent;
    float32 mTime = 0.f;

    float32 mCr = 0.2f, mCg = 0.7f, mCb = 0.2f;
};

} // namespace nkpa
} // namespace nkentseu
