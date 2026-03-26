#pragma once
/**
 * @File    NkPAFish.h
 * @Brief   Poisson procédural — 12 segments, nageoires, queue bifide.
 */

#include "Animation/NkPAChain.h"
#include "Animation/NkPAAgent.h"
#include "Renderer/NkPAMesh.h"

namespace nkentseu {
namespace nkpa {

class Fish {
public:
    void Init(NkVector2f startPos, int32 worldW, int32 worldH);
    void Update(float32 dt);
    void Draw(MeshBuilder& mb) const;

private:
    Chain   mBody;
    Agent   mAgent;
    float32 mTime = 0.f;

    // Couleur principale [0,1]
    float32 mCr = 0.2f, mCg = 0.7f, mCb = 0.95f;

    float32 BodyRadius(int32 i) const;
};

} // namespace nkpa
} // namespace nkentseu
