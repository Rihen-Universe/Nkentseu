#pragma once
/**
 * @File    NkPAEel.h
 * @Brief   Anguille procédurale — 20 segments très sinueux, nageoire dorsale continue, tête ronde.
 */

#include "Animation/NkPAChain.h"
#include "Animation/NkPAAgent.h"
#include "Renderer/NkPAMesh.h"

namespace nkentseu {
namespace nkpa {

class Eel {
public:
    void Init(NkVector2f startPos, int32 worldW, int32 worldH);
    void Update(float32 dt);
    void Draw(MeshBuilder& mb) const;

private:
    Chain   mBody;
    Agent   mAgent;
    float32 mTime = 0.f;

    float32 mCr = 0.3f, mCg = 0.5f, mCb = 0.6f;

    float32 BodyRadius(int32 i) const;
};

} // namespace nkpa
} // namespace nkentseu
