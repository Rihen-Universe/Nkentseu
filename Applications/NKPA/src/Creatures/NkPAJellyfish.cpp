/**
 * @File    NkPAJellyfish.cpp
 * @Brief   Méduse : cloche pulsante, 6 tentacules traînants.
 */

#include "Creatures/NkPAJellyfish.h"
#include <cmath>
#include <cstdlib>

namespace nkentseu {
namespace nkpa {

static float32 Randf(float32 lo, float32 hi) {
    return lo + (float32)rand() / (float32)RAND_MAX * (hi - lo);
}

void Jellyfish::Init(NkVector2f startPos, int32 worldW, int32 worldH) {
    mCr = Randf(0.5f, 0.9f);
    mCg = Randf(0.2f, 0.5f);
    mCb = Randf(0.7f, 1.0f);
    mBellRadius = Randf(22.f, 35.f);

    // Agent très lent, ondulation prononcée
    mAgent.Init(startPos, Randf(30.f, 55.f), Randf(8.f, 15.f), Randf(1.f, 2.5f), worldW, worldH);

    // 6 tentacules répartis sous la cloche
    for (int32 i = 0; i < JELLYFISH_TENTACLES; ++i) {
        float32 angle = PA_TAU * (float32)i / (float32)JELLYFISH_TENTACLES;
        NkVector2f tp = { startPos.x + ::cosf(angle) * mBellRadius * 0.7f,
                          startPos.y + ::sinf(angle) * mBellRadius * 0.7f };
        mTentacles[i].Init(tp, 8, 9.f);
    }
}

void Jellyfish::Update(float32 dt) {
    mTime += dt;
    NkVector2f pos = mAgent.Update(dt);

    // Pulse : rayon oscille
    float32 pulse = 1.f + 0.15f * ::sinf(mTime * 3.f);

    // Mise à jour de chaque tentacule depuis le bord de la cloche
    for (int32 i = 0; i < JELLYFISH_TENTACLES; ++i) {
        float32    angle  = PA_TAU * (float32)i / (float32)JELLYFISH_TENTACLES + mTime * 0.5f;
        NkVector2f attach = pos + NkVector2f{ ::cosf(angle) * mBellRadius * pulse * 0.75f,
                                              ::sinf(angle) * mBellRadius * pulse * 0.75f };
        mTentacles[i].Update(attach);
    }
}

void Jellyfish::Draw(MeshBuilder& mb) const {
    NkVector2f pos   = mAgent.Pos();
    float32    pulse = 1.f + 0.15f * ::sinf(mTime * 3.f);
    float32    r     = mBellRadius * pulse;

    // ── Tentacules (d'abord, sous la cloche) ─────────────────────────────────
    for (int32 t = 0; t < JELLYFISH_TENTACLES; ++t) {
        const Chain& ten = mTentacles[t];
        const int32  NT  = ten.Count();
        for (int32 i = 0; i < NT - 1; ++i) {
            float32 fade = 1.f - (float32)i / (float32)(NT - 1);
            mb.AddQuadGrad(ten.Seg(i).pos,     2.f * fade,
                           mCr, mCg, mCb, 0.7f * fade,
                           ten.Seg(i+1).pos,   2.f * fade * 0.7f,
                           mCr, mCg, mCb, 0.4f * fade);
        }
    }

    // ── Cloche ───────────────────────────────────────────────────────────────
    // Bord extérieur translucide
    mb.AddEllipse(pos, r, r * 0.55f, 0.f, mCr, mCg, mCb, 0.45f, 24);
    // Corps central
    mb.AddEllipse(pos, r * 0.8f, r * 0.4f, 0.f, mCr * 1.1f, mCg * 1.1f, mCb * 1.0f, 0.65f, 22);
    // Centre lumineux
    mb.AddEllipse(pos, r * 0.45f, r * 0.22f, 0.f, 1.f, 0.95f, 1.f, 0.5f, 16);

    // Canaux radiaux (lignes de symétrie)
    for (int32 i = 0; i < 4; ++i) {
        float32    a  = PA_PI * 0.5f * (float32)i;
        NkVector2f p1 = pos;
        NkVector2f p2 = pos + NkVector2f{ ::cosf(a) * r * 0.7f, ::sinf(a) * r * 0.3f };
        mb.AddTriFlat(p1, p1 + NkVector2f{1.f, 0.f}, p2, mCr * 0.8f, mCg * 0.8f, mCb, 0.3f);
    }
}

} // namespace nkpa
} // namespace nkentseu
