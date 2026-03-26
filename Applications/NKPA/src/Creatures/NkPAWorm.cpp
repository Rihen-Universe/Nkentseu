/**
 * @File    NkPAWorm.cpp
 * @Brief   Ver de terre : 8 segments épais, tête ronde avec petits yeux.
 */

#include "Creatures/NkPAWorm.h"
#include <cmath>
#include <cstdlib>

namespace nkentseu {
namespace nkpa {

static float32 Randf(float32 lo, float32 hi) {
    return lo + (float32)rand() / (float32)RAND_MAX * (hi - lo);
}

void Worm::Init(NkVector2f startPos, int32 worldW, int32 worldH) {
    mCr = Randf(0.5f, 0.75f);
    mCg = Randf(0.25f, 0.45f);
    mCb = Randf(0.2f, 0.4f);
    mBody.Init(startPos, 8, 12.f);
    mAgent.Init(startPos, Randf(40.f, 65.f), Randf(8.f, 15.f), Randf(2.f, 3.5f), worldW, worldH);
}

void Worm::Update(float32 dt) {
    mTime += dt;
    mBody.Update(mAgent.Update(dt));
}

void Worm::Draw(MeshBuilder& mb) const {
    const int32 N = mBody.Count();
    if (N < 2) return;

    // ── Corps (segments bien ronds avec anneaux) ─────────────────────────────
    for (int32 i = 0; i < N - 1; ++i) {
        float32 t   = (float32)i / (float32)(N - 1);
        float32 rad = 10.f - t * 3.f; // légèrement effilé vers la queue
        float32 alt = (float32)(i % 2) * 0.08f;
        mb.AddQuadGrad(mBody.Seg(i).pos,   rad,
                       mCr + alt, mCg + alt, mCb + alt, 1.f,
                       mBody.Seg(i+1).pos, rad - 1.f,
                       mCr, mCg, mCb, 1.f);
    }
    // Cercles de jointure (rondeurs prononcées)
    for (int32 i = 0; i < N; ++i) {
        float32 t   = (float32)i / (float32)(N - 1);
        float32 rad = 10.f - t * 3.f;
        float32 alt = (float32)(i % 2) * 0.08f;
        mb.AddCircle(mBody.Seg(i).pos, rad, mCr + alt, mCg + alt, mCb + alt, 1.f, 16);
    }

    // ── Tête ─────────────────────────────────────────────────────────────────
    NkVector2f head = mBody.Seg(0).pos;
    float32    ang  = mBody.Seg(0).angle + PA_PI;
    mb.AddCircle(head, 11.f, mCr * 1.1f, mCg * 0.9f, mCb * 0.85f, 1.f, 18);

    // Petits yeux à peine visibles (le ver est presque aveugle)
    NkVector2f perp = { -::sinf(ang), ::cosf(ang) };
    NkVector2f eL   = head + perp * 3.5f + NkVector2f{ ::cosf(ang)*5.f, ::sinf(ang)*5.f };
    NkVector2f eR   = head - perp * 3.5f + NkVector2f{ ::cosf(ang)*5.f, ::sinf(ang)*5.f };
    mb.AddCircle(eL, 2.f, 0.2f, 0.2f, 0.2f, 0.8f, 8);
    mb.AddCircle(eR, 2.f, 0.2f, 0.2f, 0.2f, 0.8f, 8);
}

} // namespace nkpa
} // namespace nkentseu
