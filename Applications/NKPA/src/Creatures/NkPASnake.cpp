/**
 * @File    NkPASnake.cpp
 * @Brief   Serpent procédural : 24 segments, bandes de couleur, langue bifide animée.
 */

#include "Creatures/NkPASnake.h"
#include <cmath>
#include <cstdlib>

namespace nkentseu {
namespace nkpa {

static float32 Randf(float32 lo, float32 hi) {
    return lo + (float32)rand() / (float32)RAND_MAX * (hi - lo);
}

void Snake::Init(NkVector2f startPos, int32 worldW, int32 worldH) {
    mCr = Randf(0.1f, 0.4f);
    mCg = Randf(0.5f, 0.9f);
    mCb = Randf(0.1f, 0.3f);
    mBody.Init(startPos, 24, 10.f);
    mAgent.Init(startPos, Randf(80.f, 130.f), Randf(10.f, 18.f), Randf(2.5f, 4.f), worldW, worldH);
}

void Snake::Update(float32 dt) {
    mTime += dt;
    NkVector2f head = mAgent.Update(dt);
    mBody.Update(head);
}

float32 Snake::BodyRadius(int32 i) const {
    const int32 N = mBody.Count();
    float32 t = (float32)i / (float32)(N - 1);
    float32 envelope = 1.f - t;
    if (t < 0.08f) envelope = t / 0.08f;  // tête effilée
    return 11.f * envelope;
}

void Snake::SegColor(int32 i, float32& r, float32& g, float32& b) const {
    float32 stripe = (float32)((i / 3) % 2);
    r = mCr + stripe * 0.15f;
    g = mCg + stripe * 0.05f;
    b = mCb + stripe * 0.02f;
}

void Snake::Draw(MeshBuilder& mb) const {
    const int32 N = mBody.Count();
    if (N < 2) return;

    // ── Corps : quads à bandes alternées ────────────────────────────────────
    for (int32 i = 0; i < N - 1; ++i) {
        float32 r0, g0, b0;  SegColor(i,   r0, g0, b0);
        float32 r1, g1, b1;  SegColor(i+1, r1, g1, b1);
        mb.AddQuadGrad(mBody.Seg(i).pos,     BodyRadius(i),     r0, g0, b0, 1.f,
                       mBody.Seg(i+1).pos,   BodyRadius(i+1),   r1, g1, b1, 1.f);
    }
    for (int32 i = 0; i < N; ++i) {
        float32 r0, g0, b0;  SegColor(i, r0, g0, b0);
        mb.AddCircle(mBody.Seg(i).pos, BodyRadius(i), r0, g0, b0, 1.f, 14);
    }

    // ── Tête ────────────────────────────────────────────────────────────────
    mb.AddCircle(mBody.Seg(0).pos, 13.f, mCr * 0.7f, mCg * 0.7f, mCb * 0.5f, 1.f, 18);
    mb.AddCircle(mBody.Seg(0).pos, 11.f, mCr * 1.1f, mCg * 1.1f, mCb * 0.9f, 1.f, 18);

    // ── Yeux ────────────────────────────────────────────────────────────────
    {
        float32    ang = mBody.Seg(0).angle + PA_PI;
        NkVector2f e0  = { mBody.Seg(0).pos.x + ::cosf(ang + 0.5f) * 7.f,
                           mBody.Seg(0).pos.y + ::sinf(ang + 0.5f) * 7.f };
        NkVector2f e1  = { mBody.Seg(0).pos.x + ::cosf(ang - 0.5f) * 7.f,
                           mBody.Seg(0).pos.y + ::sinf(ang - 0.5f) * 7.f };
        mb.AddCircle(e0, 3.5f, 0.9f, 0.8f, 0.1f, 1.f, 10);
        mb.AddCircle(e1, 3.5f, 0.9f, 0.8f, 0.1f, 1.f, 10);
        mb.AddCircle(e0, 1.8f, 0.f,  0.f,  0.f,  1.f, 8);
        mb.AddCircle(e1, 1.8f, 0.f,  0.f,  0.f,  1.f, 8);
    }

    // ── Langue bifide animée ─────────────────────────────────────────────────
    {
        float32    ang   = mBody.Seg(0).angle + PA_PI;
        NkVector2f snout = { mBody.Seg(0).pos.x + ::cosf(ang) * 14.f,
                             mBody.Seg(0).pos.y + ::sinf(ang) * 14.f };
        float32    wave  = ::sinf(mTime * 8.f) * 0.25f;
        NkVector2f mid   = snout + NkVector2f{ ::cosf(ang) * 8.f, ::sinf(ang) * 8.f };
        NkVector2f tipL  = mid   + NkVector2f{ ::cosf(ang + 0.5f + wave) * 6.f,
                                               ::sinf(ang + 0.5f + wave) * 6.f };
        NkVector2f tipR  = mid   + NkVector2f{ ::cosf(ang - 0.5f + wave) * 6.f,
                                               ::sinf(ang - 0.5f + wave) * 6.f };
        NkVector2f perp  = { ::cosf(ang + PA_PI * 0.5f), ::sinf(ang + PA_PI * 0.5f) };
        mb.AddTriFlat(snout + perp, snout - perp, mid,  0.9f, 0.1f, 0.1f, 1.f);
        mb.AddTriFlat(mid, mid + NkVector2f{1.f, 0.f}, tipL, 0.95f, 0.1f, 0.1f, 1.f);
        mb.AddTriFlat(mid, mid + NkVector2f{1.f, 0.f}, tipR, 0.95f, 0.1f, 0.1f, 1.f);
    }
}

} // namespace nkpa
} // namespace nkentseu
