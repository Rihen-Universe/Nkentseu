#pragma once
/**
 * @File    NkPAShark.h
 * @Brief   Requin procédural — 14 segments, nageoire dorsale triangulaire, tête effilée.
 */

#include "Animation/NkPAChain.h"
#include "Animation/NkPAAgent.h"
#include "Renderer/NkPAMesh.h"

namespace nkentseu {
namespace nkpa {

class Shark {
public:
    void Init(NkVector2f startPos, int32 worldW, int32 worldH);
    void Update(float32 dt);
    void Draw(MeshBuilder& mb) const;

private:
    Chain   mBody;
    Agent   mAgent;
    float32 mTime = 0.f;

    float32 mCr = 0.45f, mCg = 0.45f, mCb = 0.5f;

    float32 BodyRadius(int32 i) const;
};

} // namespace nkpa
} // namespace nkentseu
