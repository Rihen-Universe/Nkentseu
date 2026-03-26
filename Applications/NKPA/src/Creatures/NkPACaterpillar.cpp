/**
 * @File    NkPACaterpillar.cpp
 * @Brief   Chenille : 10 segments ronds, pattes ondulantes, tête avec yeux et antennes.
 */

#include "Creatures/NkPACaterpillar.h"
#include <cmath>
#include <cstdlib>

namespace nkentseu {
namespace nkpa {

static float32 Randf(float32 lo, float32 hi) {
    return lo + (float32)rand() / (float32)RAND_MAX * (hi - lo);
}

void Caterpillar::Init(NkVector2f startPos, int32 worldW, int32 worldH) {
    mCr = Randf(0.1f, 0.4f);
    mCg = Randf(0.6f, 0.9f);
    mCb = Randf(0.1f, 0.3f);
    mBody.Init(startPos, 10, 13.f);
    mAgent.Init(startPos, Randf(50.f, 80.f), Randf(4.f, 8.f), Randf(1.5f, 3.f), worldW, worldH);
}

void Caterpillar::Update(float32 dt) {
    mTime += dt;
    mBody.Update(mAgent.Update(dt));
}

void Caterpillar::Draw(MeshBuilder& mb) const {
    const int32 N = mBody.Count();
    if (N < 2) return;

    // ── Corps (segments très ronds) ──────────────────────────────────────────
    for (int32 i = N - 1; i >= 0; --i) {
        float32 t  = (float32)i / (float32)(N - 1);
        // Couleur alternée (anneaux)
        float32 alt = (float32)(i % 2) * 0.15f;
        float32 r = mCr + alt, g = mCg, b = mCb;
        float32 rad = 9.f + (1.f - t) * 3.f;  // légèrement plus gros vers la tête
        mb.AddCircle(mBody.Seg(i).pos, rad, r, g, b, 1.f, 18);
        // Spot dorsal
        mb.AddCircle(mBody.Seg(i).pos, rad * 0.35f, mCr * 0.5f, mCg * 0.5f, mCb * 0.5f, 0.7f, 10);
    }

    // ── Pattes (3 paires, segments 3, 5, 7) ─────────────────────────────────
    for (int32 pair = 0; pair < 3; ++pair) {
        int32      idx  = 3 + pair * 2;
        NkVector2f pos  = mBody.Seg(idx).pos;
        float32    ang  = mBody.Seg(idx).angle;
        float32    wave = ::sinf(mTime * 5.f + (float32)pair * PA_PI * 0.6f) * 0.4f;
        float32    rad  = 9.f + (1.f - (float32)idx / (float32)(N-1)) * 3.f;

        NkVector2f perp = { -::sinf(ang), ::cosf(ang) };
        NkVector2f base0 = pos + perp * rad;
        NkVector2f base1 = pos - perp * rad;

        // Patte gauche
        NkVector2f kL = base0 + NkVector2f{ ::cosf(ang + PA_PI * 0.5f + wave) * 14.f,
                                             ::sinf(ang + PA_PI * 0.5f + wave) * 14.f };
        mb.AddTriFlat(base0, base0 + NkVector2f{2.f, 0.f}, kL, mCr * 0.6f, mCg * 0.6f, mCb * 0.4f, 0.9f);

        // Patte droite
        NkVector2f kR = base1 + NkVector2f{ ::cosf(ang - PA_PI * 0.5f - wave) * 14.f,
                                             ::sinf(ang - PA_PI * 0.5f - wave) * 14.f };
        mb.AddTriFlat(base1, base1 + NkVector2f{2.f, 0.f}, kR, mCr * 0.6f, mCg * 0.6f, mCb * 0.4f, 0.9f);
    }

    // ── Tête ─────────────────────────────────────────────────────────────────
    NkVector2f head = mBody.Seg(0).pos;
    float32    ang  = mBody.Seg(0).angle + PA_PI;
    mb.AddCircle(head, 12.f, mCr * 1.1f, mCg * 1.0f, mCb * 0.9f, 1.f, 18);

    // Yeux
    NkVector2f perp = { -::sinf(ang), ::cosf(ang) };
    NkVector2f eL   = head + perp * 5.f + NkVector2f{ ::cosf(ang) * 5.f, ::sinf(ang) * 5.f };
    NkVector2f eR   = head - perp * 5.f + NkVector2f{ ::cosf(ang) * 5.f, ::sinf(ang) * 5.f };
    mb.AddCircle(eL, 3.5f, 0.9f, 0.9f, 0.2f, 1.f, 10);
    mb.AddCircle(eR, 3.5f, 0.9f, 0.9f, 0.2f, 1.f, 10);
    mb.AddCircle(eL, 1.8f, 0.f,  0.f,  0.f,  1.f, 8);
    mb.AddCircle(eR, 1.8f, 0.f,  0.f,  0.f,  1.f, 8);

    // Antennes
    for (int32 s = -1; s <= 1; s += 2) {
        float32    wave = ::sinf(mTime * 4.f + (float32)s) * 0.3f;
        NkVector2f base = head + perp * (5.f * (float32)s)
                              + NkVector2f{ ::cosf(ang) * 8.f, ::sinf(ang) * 8.f };
        NkVector2f tip  = base + NkVector2f{ ::cosf(ang + (float32)s * 0.6f + wave) * 12.f,
                                             ::sinf(ang + (float32)s * 0.6f + wave) * 12.f };
        mb.AddTriFlat(base, base + NkVector2f{1.f, 0.f}, tip, mCr * 0.8f, mCg * 0.8f, mCb * 0.5f, 0.8f);
        mb.AddCircle(tip, 2.f, mCr * 0.6f, mCg * 0.6f, mCb * 0.4f, 1.f, 8);
    }
}

} // namespace nkpa
} // namespace nkentseu
