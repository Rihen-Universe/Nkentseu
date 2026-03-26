#pragma once
/**
 * @File    NkPABird.h
 * @Brief   Oiseau procédural — corps 6 segments, ailes battantes, bec, oeil.
 */

#include "Animation/NkPAChain.h"
#include "Animation/NkPAAgent.h"
#include "Renderer/NkPAMesh.h"

namespace nkentseu {
namespace nkpa {

class Bird {
public:
    void Init(NkVector2f startPos, int32 worldW, int32 worldH);
    void Update(float32 dt);
    void Draw(MeshBuilder& mb) const;

private:
    Chain   mBody;
    Agent   mAgent;
    float32 mTime = 0.f;

    float32 mCr = 0.15f, mCg = 0.15f, mCb = 0.15f; // corps sombre
    float32 mWr = 0.85f, mWg = 0.85f, mWb = 0.85f; // ailes claires
};

} // namespace nkpa
} // namespace nkentseu
