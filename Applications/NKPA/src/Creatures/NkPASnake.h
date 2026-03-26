#pragma once
/**
 * @File    NkPASnake.h
 * @Brief   Serpent procédural — 24 segments, bandes alternées, langue bifide animée.
 */

#include "Animation/NkPAChain.h"
#include "Animation/NkPAAgent.h"
#include "Renderer/NkPAMesh.h"

namespace nkentseu {
namespace nkpa {

class Snake {
public:
    void Init(NkVector2f startPos, int32 worldW, int32 worldH);
    void Update(float32 dt);
    void Draw(MeshBuilder& mb) const;

private:
    Chain   mBody;
    Agent   mAgent;
    float32 mTime = 0.f;

    // Couleur principale [0,1]
    float32 mCr = 0.2f, mCg = 0.6f, mCb = 0.1f;

    float32 BodyRadius(int32 i) const;
    void    SegColor(int32 i, float32& r, float32& g, float32& b) const;
};

} // namespace nkpa
} // namespace nkentseu
