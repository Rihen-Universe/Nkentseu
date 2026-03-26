/**
 * @File    NkPAFish.cpp
 * @Brief   Implémentation du poisson procédural.
 */

#include "Creatures/NkPAFish.h"
#include "Renderer/NkPAMesh.h"
#include <cmath>
#include <cstdlib>

namespace nkentseu {
namespace nkpa {

static float32 Randf(float32 lo, float32 hi) {
    return lo + (float32)rand() / (float32)RAND_MAX * (hi - lo);
}

void Fish::Init(NkVector2f startPos, int32 worldW, int32 worldH) {
    mCr = Randf(0.1f, 0.6f);
    mCg = Randf(0.4f, 0.9f);
    mCb = Randf(0.6f, 1.0f);
    mBody.Init(startPos, 12, 14.f);
    mAgent.Init(startPos, Randf(100.f, 160.f), Randf(6.f, 12.f), Randf(2.f, 3.5f), worldW, worldH);
}

void Fish::Update(float32 dt) {
    mTime += dt;
    NkVector2f head = mAgent.Update(dt);
    mBody.Update(head);
}

float32 Fish::BodyRadius(int32 i) const {
    float32 t = (float32)i / (float32)(mBody.Count() - 1);
    return 18.f * (1.f - t * 0.8f);
}

void Fish::Draw(MeshBuilder& mb) const {
    const int32 N = mBody.Count();
    if (N < 2) return;

    // ── Corps : quads gradients + cercles de jointure ───────────────────────
    for (int32 i = 0; i < N - 1; ++i) {
        float32 t0 = (float32)i     / (float32)(N - 1);
        float32 t1 = (float32)(i+1) / (float32)(N - 1);
        float32 r0 = mCr * (0.7f + 0.3f * (1.f - t0));
        float32 g0 = mCg * (0.8f + 0.2f * (1.f - t0));
        float32 r1 = mCr * (0.7f + 0.3f * (1.f - t1));
        float32 g1 = mCg * (0.8f + 0.2f * (1.f - t1));
        mb.AddQuadGrad(mBody.Seg(i).pos,     BodyRadius(i),
                       r0, g0, mCb, 1.f,
                       mBody.Seg(i+1).pos,   BodyRadius(i+1),
                       r1, g1, mCb, 1.f);
    }
    for (int32 i = 0; i < N; ++i) {
        float32 t  = (float32)i / (float32)(N - 1);
        float32 r0 = mCr * (0.7f + 0.3f * (1.f - t));
        float32 g0 = mCg * (0.8f + 0.2f * (1.f - t));
        mb.AddCircle(mBody.Seg(i).pos, BodyRadius(i), r0, g0, mCb, 1.f, 16);
    }

    // ── Nageoire dorsale (segments 2–4) ─────────────────────────────────────
    {
        NkVector2f p1   = mBody.Seg(2).pos;
        NkVector2f p2   = mBody.Seg(4).pos;
        NkVector2f mid  = (p1 + p2) * 0.5f;
        NkVector2f perp = (p2 - p1).Normalized().Normal();
        NkVector2f tip  = mid + perp * 28.f;
        mb.AddFin(p1, p2, tip,
                  mCr, mCg, mCb * 0.7f, 0.7f,
                  mCr * 0.5f, mCg * 0.5f, mCb * 0.4f, 0.4f);
    }

    // ── Nageoires pectorales (segment 3) ────────────────────────────────────
    {
        NkVector2f pos3 = mBody.Seg(3).pos;
        float32    ang  = mBody.Seg(3).angle;
        float32    wave = ::sinf(mTime * 3.f + 1.f) * 0.3f;
        float32    r3   = BodyRadius(3);

        NkVector2f bl0 = { pos3.x + ::cosf(ang) * r3 * 0.8f,
                           pos3.y + ::sinf(ang) * r3 * 0.8f };
        NkVector2f bl1 = { pos3.x - ::cosf(ang) * r3 * 0.8f,
                           pos3.y - ::sinf(ang) * r3 * 0.8f };

        // Côté gauche
        float32    angL = ang + PA_PI * 0.5f + wave;
        NkVector2f tipL = { pos3.x + ::cosf(angL) * 26.f,
                            pos3.y + ::sinf(angL) * 26.f };
        mb.AddFin(bl0, bl1, tipL,
                  mCr, mCg * 0.8f, mCb * 0.6f, 0.65f,
                  mCr * 0.3f, mCg * 0.3f, mCb * 0.3f, 0.3f);

        // Côté droit
        float32    angR = ang - PA_PI * 0.5f - wave;
        NkVector2f tipR = { pos3.x + ::cosf(angR) * 26.f,
                            pos3.y + ::sinf(angR) * 26.f };
        mb.AddFin(bl0, bl1, tipR,
                  mCr, mCg * 0.8f, mCb * 0.6f, 0.65f,
                  mCr * 0.3f, mCg * 0.3f, mCb * 0.3f, 0.3f);
    }

    // ── Nageoire caudale (double lobe) ───────────────────────────────────────
    {
        NkVector2f tailPos = mBody.Seg(N - 1).pos;
        float32    ang     = mBody.Seg(N - 2).angle;
        float32    wave    = ::sinf(mTime * 4.f) * 0.5f;
        float32    tailW   = 22.f;
        NkVector2f perpV   = { -::sinf(ang), ::cosf(ang) };
        NkVector2f baseL   = tailPos + perpV * 8.f;
        NkVector2f baseR   = tailPos - perpV * 8.f;
        NkVector2f tipU    = { tailPos.x + ::cosf(ang + PA_PI + wave) * tailW,
                               tailPos.y + ::sinf(ang + PA_PI + wave) * tailW };
        NkVector2f tipD    = { tailPos.x + ::cosf(ang + PA_PI - wave) * tailW,
                               tailPos.y + ::sinf(ang + PA_PI - wave) * tailW };
        mb.AddFin(baseL, tailPos, tipU,
                  mCr, mCg * 0.7f, mCb * 0.5f, 0.75f,
                  mCr * 0.2f, mCg * 0.2f, mCb * 0.2f, 0.3f);
        mb.AddFin(tailPos, baseR, tipD,
                  mCr, mCg * 0.7f, mCb * 0.5f, 0.75f,
                  mCr * 0.2f, mCg * 0.2f, mCb * 0.2f, 0.3f);
    }

    // ── Oeil ────────────────────────────────────────────────────────────────
    {
        float32    ang    = mBody.Seg(0).angle + PA_PI;
        NkVector2f eyeOff = { ::cosf(ang + 0.5f) * 8.f,
                              ::sinf(ang + 0.5f) * 8.f };
        NkVector2f eyePos = mBody.Seg(0).pos + eyeOff;
        mb.AddCircle(eyePos, 5.f,  1.f, 1.f, 1.f, 1.f, 12);
        mb.AddCircle(eyePos, 2.8f, 0.f, 0.f, 0.f, 1.f, 10);
    }
}

} // namespace nkpa
} // namespace nkentseu
