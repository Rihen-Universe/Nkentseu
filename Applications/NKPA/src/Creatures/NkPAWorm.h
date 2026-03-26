#pragma once
/**
 * @File    NkPAWorm.h
 * @Brief   Ver de terre procédural — 8 segments épais, ondulation lente, tête ronde.
 */

#include "Animation/NkPAChain.h"
#include "Animation/NkPAAgent.h"
#include "Renderer/NkPAMesh.h"

namespace nkentseu {
namespace nkpa {

class Worm {
public:
    void Init(NkVector2f startPos, int32 worldW, int32 worldH);
    void Update(float32 dt);
    void Draw(MeshBuilder& mb) const;

private:
    Chain   mBody;
    Agent   mAgent;
    float32 mTime = 0.f;

    float32 mCr = 0.6f, mCg = 0.3f, mCb = 0.2f;
};

} // namespace nkpa
} // namespace nkentseu
