/**
 * @File    NkPAEel.cpp
 * @Brief   Anguille : 20 segments, nageoire dorsale continue, tête ronde avec yeux.
 */

#include "Creatures/NkPAEel.h"
#include <cmath>
#include <cstdlib>

namespace nkentseu {
namespace nkpa {

static float32 Randf(float32 lo, float32 hi) {
    return lo + (float32)rand() / (float32)RAND_MAX * (hi - lo);
}

void Eel::Init(NkVector2f startPos, int32 worldW, int32 worldH) {
    mCr = Randf(0.2f, 0.5f);
    mCg = Randf(0.4f, 0.7f);
    mCb = Randf(0.5f, 0.9f);
    mBody.Init(startPos, 20, 11.f);
    mAgent.Init(startPos, Randf(70.f, 120.f), Randf(12.f, 20.f), Randf(3.f, 5.f), worldW, worldH);
}

void Eel::Update(float32 dt) {
    mTime += dt;
    mBody.Update(mAgent.Update(dt));
}

float32 Eel::BodyRadius(int32 i) const {
    const int32 N = mBody.Count();
    float32 t = (float32)i / (float32)(N - 1);
    // Effilé aux deux extrémités, large au milieu
    float32 env = ::sinf(t * PA_PI);
    return 3.f + env * 9.f;
}

void Eel::Draw(MeshBuilder& mb) const {
    const int32 N = mBody.Count();
    if (N < 2) return;

    // ── Corps ────────────────────────────────────────────────────────────────
    for (int32 i = 0; i < N - 1; ++i) {
        float32 t  = (float32)i / (float32)(N - 1);
        float32 sh = 0.7f + 0.3f * (1.f - t);
        mb.AddQuadGrad(mBody.Seg(i).pos,     BodyRadius(i),
                       mCr*sh, mCg*sh, mCb*sh, 1.f,
                       mBody.Seg(i+1).pos,   BodyRadius(i+1),
                       mCr*sh, mCg*sh, mCb*sh, 1.f);
    }
    for (int32 i = 0; i < N; ++i) {
        float32 t  = (float32)i / (float32)(N - 1);
        float32 sh = 0.7f + 0.3f * (1.f - t);
        mb.AddCircle(mBody.Seg(i).pos, BodyRadius(i), mCr*sh, mCg*sh, mCb*sh, 1.f, 12);
    }

    // ── Nageoire dorsale (toute la longueur) ─────────────────────────────────
    for (int32 i = 1; i < N - 1; ++i) {
        NkVector2f p0   = mBody.Seg(i - 1).pos;
        NkVector2f p1   = mBody.Seg(i).pos;
        NkVector2f p2   = mBody.Seg(i + 1).pos;
        NkVector2f dir  = (p2 - p0).Normalized();
        NkVector2f perp = dir.Normal();
        float32    ht   = BodyRadius(i) * 0.8f + 4.f;
        NkVector2f tip  = p1 + perp * ht;
        mb.AddTriFlat(p0, p2, tip, mCr * 0.7f, mCg * 0.7f, mCb * 0.5f, 0.55f);
    }

    // ── Tête ─────────────────────────────────────────────────────────────────
    NkVector2f head = mBody.Seg(0).pos;
    float32    ang  = mBody.Seg(0).angle + PA_PI;
    mb.AddCircle(head, 11.f, mCr * 1.1f, mCg * 1.0f, mCb * 0.9f, 1.f, 18);

    // Yeux
    NkVector2f perp = { -::sinf(ang), ::cosf(ang) };
    NkVector2f eL   = head + perp * 5.f + NkVector2f{ ::cosf(ang) * 4.f, ::sinf(ang) * 4.f };
    NkVector2f eR   = head - perp * 5.f + NkVector2f{ ::cosf(ang) * 4.f, ::sinf(ang) * 4.f };
    mb.AddCircle(eL, 3.f, 0.9f, 0.9f, 0.2f, 1.f, 10);
    mb.AddCircle(eR, 3.f, 0.9f, 0.9f, 0.2f, 1.f, 10);
    mb.AddCircle(eL, 1.6f, 0.f, 0.f, 0.f,  1.f, 8);
    mb.AddCircle(eR, 1.6f, 0.f, 0.f, 0.f,  1.f, 8);
}

} // namespace nkpa
} // namespace nkentseu
