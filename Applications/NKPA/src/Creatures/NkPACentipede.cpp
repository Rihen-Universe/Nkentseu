/**
 * @File    NkPACentipede.cpp
 * @Brief   Mille-pattes : 16 segments, paires de pattes ondulantes, tête avec antennes et yeux.
 */

#include "Creatures/NkPACentipede.h"
#include <cmath>
#include <cstdlib>

namespace nkentseu {
namespace nkpa {

static float32 Randf(float32 lo, float32 hi) {
    return lo + (float32)rand() / (float32)RAND_MAX * (hi - lo);
}

void Centipede::Init(NkVector2f startPos, int32 worldW, int32 worldH) {
    mCr = Randf(0.4f, 0.7f);
    mCg = Randf(0.15f, 0.35f);
    mCb = Randf(0.02f, 0.12f);
    mBody.Init(startPos, 16, 9.f);
    mAgent.Init(startPos, Randf(60.f, 100.f), Randf(6.f, 12.f), Randf(2.f, 4.f), worldW, worldH);
}

void Centipede::Update(float32 dt) {
    mTime += dt;
    mBody.Update(mAgent.Update(dt));
}

void Centipede::Draw(MeshBuilder& mb) const {
    const int32 N = mBody.Count();
    if (N < 2) return;

    // ── Pattes (toutes paires, dessinées avant le corps) ────────────────────
    for (int32 i = 1; i < N; ++i) {
        NkVector2f pos  = mBody.Seg(i).pos;
        float32    ang  = mBody.Seg(i).angle;
        // Déphasage par segment pour une vague de locomotion
        float32    wave = ::sinf(mTime * 6.f + (float32)i * 0.5f) * 0.5f;
        float32    legL = 12.f + ::sinf((float32)i) * 2.f;
        NkVector2f perp = { -::sinf(ang), ::cosf(ang) };
        NkVector2f base = pos;

        // Patte gauche
        NkVector2f kL = base + perp * 6.f
                      + NkVector2f{ ::cosf(ang + PA_PI * 0.5f + wave) * legL,
                                    ::sinf(ang + PA_PI * 0.5f + wave) * legL };
        mb.AddTriFlat(base, base + perp * 5.f, kL, mCr * 0.75f, mCg * 0.6f, mCb * 0.4f, 0.9f);

        // Patte droite
        NkVector2f kR = base - perp * 6.f
                      + NkVector2f{ ::cosf(ang - PA_PI * 0.5f - wave) * legL,
                                    ::sinf(ang - PA_PI * 0.5f - wave) * legL };
        mb.AddTriFlat(base, base - perp * 5.f, kR, mCr * 0.75f, mCg * 0.6f, mCb * 0.4f, 0.9f);
    }

    // ── Corps ────────────────────────────────────────────────────────────────
    for (int32 i = 0; i < N - 1; ++i) {
        float32 alt = (float32)(i % 2) * 0.1f;
        mb.AddQuadGrad(mBody.Seg(i).pos,   7.f, mCr + alt, mCg, mCb, 1.f,
                       mBody.Seg(i+1).pos, 7.f, mCr + alt, mCg, mCb, 1.f);
        mb.AddCircle(mBody.Seg(i).pos, 7.f, mCr + alt, mCg, mCb, 1.f, 12);
    }
    mb.AddCircle(mBody.Seg(N-1).pos, 7.f, mCr, mCg, mCb, 1.f, 12);

    // ── Tête ─────────────────────────────────────────────────────────────────
    NkVector2f head = mBody.Seg(0).pos;
    float32    ang  = mBody.Seg(0).angle + PA_PI;
    mb.AddCircle(head, 9.f, mCr * 1.2f, mCg * 1.1f, mCb * 0.8f, 1.f, 16);

    // Yeux
    NkVector2f perp = { -::sinf(ang), ::cosf(ang) };
    NkVector2f eL   = head + perp * 4.f + NkVector2f{ ::cosf(ang)*5.f, ::sinf(ang)*5.f };
    NkVector2f eR   = head - perp * 4.f + NkVector2f{ ::cosf(ang)*5.f, ::sinf(ang)*5.f };
    mb.AddCircle(eL, 2.5f, 0.9f, 0.8f, 0.1f, 1.f, 8);
    mb.AddCircle(eR, 2.5f, 0.9f, 0.8f, 0.1f, 1.f, 8);
    mb.AddCircle(eL, 1.2f, 0.f,  0.f,  0.f,  1.f, 6);
    mb.AddCircle(eR, 1.2f, 0.f,  0.f,  0.f,  1.f, 6);

    // Antennes (longues)
    for (int32 s = -1; s <= 1; s += 2) {
        float32    wave = ::sinf(mTime * 5.f + (float32)s * 1.2f) * 0.4f;
        NkVector2f base = head + perp * (4.f * (float32)s)
                              + NkVector2f{ ::cosf(ang) * 8.f, ::sinf(ang) * 8.f };
        NkVector2f mid  = base + NkVector2f{ ::cosf(ang + (float32)s*0.5f + wave)*14.f,
                                              ::sinf(ang + (float32)s*0.5f + wave)*14.f };
        NkVector2f tip  = mid  + NkVector2f{ ::cosf(ang + (float32)s*0.8f + wave)*10.f,
                                              ::sinf(ang + (float32)s*0.8f + wave)*10.f };
        mb.AddTriFlat(base, base + NkVector2f{1.f,0.f}, mid, mCr*0.8f, mCg*0.7f, mCb*0.5f, 0.8f);
        mb.AddTriFlat(mid,  mid  + NkVector2f{1.f,0.f}, tip, mCr*0.6f, mCg*0.5f, mCb*0.4f, 0.7f);
    }
}

} // namespace nkpa
} // namespace nkentseu
