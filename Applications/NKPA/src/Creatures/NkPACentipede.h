#pragma once
/**
 * @File    NkPACentipede.h
 * @Brief   Mille-pattes procédural — 16 segments, paires de pattes ondulantes, tête avec antennes.
 */

#include "Animation/NkPAChain.h"
#include "Animation/NkPAAgent.h"
#include "Renderer/NkPAMesh.h"

namespace nkentseu {
namespace nkpa {

class Centipede {
public:
    void Init(NkVector2f startPos, int32 worldW, int32 worldH);
    void Update(float32 dt);
    void Draw(MeshBuilder& mb) const;

private:
    Chain   mBody;
    Agent   mAgent;
    float32 mTime = 0.f;

    float32 mCr = 0.55f, mCg = 0.25f, mCb = 0.05f;
};

} // namespace nkpa
} // namespace nkentseu
