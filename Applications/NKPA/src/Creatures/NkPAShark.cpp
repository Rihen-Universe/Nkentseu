/**
 * @File    NkPAShark.cpp
 * @Brief   Requin : 14 segments gris, nageoire dorsale haute, pectorales, queue hétérocerque.
 */

#include "Creatures/NkPAShark.h"
#include <cmath>
#include <cstdlib>

namespace nkentseu {
namespace nkpa {

static float32 Randf(float32 lo, float32 hi) {
    return lo + (float32)rand() / (float32)RAND_MAX * (hi - lo);
}

void Shark::Init(NkVector2f startPos, int32 worldW, int32 worldH) {
    mCr = Randf(0.35f, 0.55f);
    mCg = Randf(0.35f, 0.55f);
    mCb = Randf(0.45f, 0.65f);
    mBody.Init(startPos, 14, 16.f);
    mAgent.Init(startPos, Randf(130.f, 180.f), Randf(4.f, 8.f), Randf(1.5f, 2.5f), worldW, worldH);
}

void Shark::Update(float32 dt) {
    mTime += dt;
    mBody.Update(mAgent.Update(dt));
}

float32 Shark::BodyRadius(int32 i) const {
    const int32 N = mBody.Count();
    float32 t = (float32)i / (float32)(N - 1);
    if (t < 0.1f)  return 10.f + t / 0.1f * 8.f;   // museau effilé
    if (t < 0.5f)  return 18.f;                       // corps cylindrique
    return 18.f * (1.f - (t - 0.5f) * 1.6f);          // pédoncule caudal
}

void Shark::Draw(MeshBuilder& mb) const {
    const int32 N = mBody.Count();
    if (N < 2) return;

    float32 belly = 0.9f; // ventre plus clair

    // ── Corps ────────────────────────────────────────────────────────────────
    for (int32 i = 0; i < N - 1; ++i) {
        float32 t  = (float32)i / (float32)(N - 1);
        float32 sh = 0.75f + 0.25f * (1.f - t);
        mb.AddQuadGrad(mBody.Seg(i).pos,   BodyRadius(i),
                       mCr*sh, mCg*sh, mCb*sh, 1.f,
                       mBody.Seg(i+1).pos, BodyRadius(i+1),
                       mCr*sh, mCg*sh, mCb*sh, 1.f);
    }
    for (int32 i = 0; i < N; ++i) {
        float32 t  = (float32)i / (float32)(N - 1);
        float32 sh = 0.75f + 0.25f * (1.f - t);
        mb.AddCircle(mBody.Seg(i).pos, BodyRadius(i), mCr*sh, mCg*sh, mCb*sh, 1.f, 14);
    }

    // ── Nageoire dorsale (haute, segment 3–5) ────────────────────────────────
    {
        NkVector2f p1   = mBody.Seg(3).pos;
        NkVector2f p2   = mBody.Seg(5).pos;
        NkVector2f dir  = (p2 - p1).Normalized();
        NkVector2f perp = dir.Normal();
        NkVector2f tip  = (p1 + p2) * 0.5f + perp * 40.f;
        mb.AddFin(p1, p2, tip,
                  mCr * 0.8f, mCg * 0.8f, mCb * 0.8f, 0.9f,
                  mCr * 0.5f, mCg * 0.5f, mCb * 0.6f, 0.6f);
    }

    // ── Nageoires pectorales (segment 4) ────────────────────────────────────
    {
        NkVector2f pos4 = mBody.Seg(4).pos;
        float32    ang  = mBody.Seg(4).angle;
        float32    r4   = BodyRadius(4);
        NkVector2f bl0  = { pos4.x + ::cosf(ang) * r4, pos4.y + ::sinf(ang) * r4 };
        NkVector2f bl1  = { pos4.x - ::cosf(ang) * r4, pos4.y - ::sinf(ang) * r4 };
        NkVector2f tipL = { pos4.x + ::cosf(ang + PA_PI * 0.45f) * 35.f,
                            pos4.y + ::sinf(ang + PA_PI * 0.45f) * 35.f };
        NkVector2f tipR = { pos4.x + ::cosf(ang - PA_PI * 0.45f) * 35.f,
                            pos4.y + ::sinf(ang - PA_PI * 0.45f) * 35.f };
        mb.AddFin(bl0, bl1, tipL, mCr, mCg, mCb * 0.9f, 0.85f, mCr*0.6f, mCg*0.6f, mCb*0.7f, 0.5f);
        mb.AddFin(bl0, bl1, tipR, mCr, mCg, mCb * 0.9f, 0.85f, mCr*0.6f, mCg*0.6f, mCb*0.7f, 0.5f);
    }

    // ── Queue hétérocerque (lobe supérieur plus grand) ───────────────────────
    {
        NkVector2f tail = mBody.Seg(N-1).pos;
        float32    ang  = mBody.Seg(N-2).angle;
        float32    wave = ::sinf(mTime * 3.f) * 0.3f;
        NkVector2f perp = { -::sinf(ang), ::cosf(ang) };
        NkVector2f bL   = tail + perp * 6.f;
        NkVector2f bR   = tail - perp * 6.f;
        // Lobe supérieur (grand)
        NkVector2f tipU = { tail.x + ::cosf(ang + PA_PI + wave) * 32.f,
                            tail.y + ::sinf(ang + PA_PI + wave) * 32.f };
        // Lobe inférieur (petit)
        NkVector2f tipD = { tail.x + ::cosf(ang + PA_PI - wave) * 18.f,
                            tail.y + ::sinf(ang + PA_PI - wave) * 18.f };
        mb.AddFin(bL, tail, tipU, mCr, mCg, mCb, 0.9f, mCr*0.4f, mCg*0.4f, mCb*0.5f, 0.4f);
        mb.AddFin(tail, bR, tipD, mCr, mCg, mCb, 0.9f, mCr*0.4f, mCg*0.4f, mCb*0.5f, 0.4f);
    }

    // ── Tête (museau pointu) ──────────────────────────────────────────────────
    NkVector2f head  = mBody.Seg(0).pos;
    float32    ang   = mBody.Seg(0).angle + PA_PI;
    NkVector2f snout = head + NkVector2f{ ::cosf(ang) * 22.f, ::sinf(ang) * 22.f };
    NkVector2f perp  = { -::sinf(ang), ::cosf(ang) };
    mb.AddTriFlat(head + perp * 14.f, head - perp * 14.f, snout,
                  mCr * 1.05f, mCg * 1.05f, mCb * 1.0f, 1.f);
    mb.AddCircle(head, 14.f, mCr, mCg, mCb, 1.f, 16);
    // Ventre clair
    mb.AddEllipse(head + NkVector2f{::cosf(ang)*8.f, ::sinf(ang)*8.f},
                  12.f, 6.f, ang, belly, belly, belly, 0.7f, 12);

    // Gueule (ligne sombre sous le museau)
    NkVector2f mouthC = head + NkVector2f{ ::cosf(ang) * 14.f, ::sinf(ang) * 14.f };
    mb.AddEllipse(mouthC, 8.f, 3.f, ang, 0.05f, 0.05f, 0.05f, 0.8f, 10);

    // Yeux (au-dessus)
    NkVector2f eL = head + perp * 7.f + NkVector2f{ ::cosf(ang) * 6.f, ::sinf(ang) * 6.f };
    NkVector2f eR = head - perp * 7.f + NkVector2f{ ::cosf(ang) * 6.f, ::sinf(ang) * 6.f };
    mb.AddCircle(eL, 4.f, 0.05f, 0.05f, 0.05f, 1.f, 10);
    mb.AddCircle(eR, 4.f, 0.05f, 0.05f, 0.05f, 1.f, 10);
    mb.AddCircle(eL, 1.8f, 0.f, 0.f, 0.f, 1.f, 8);
    mb.AddCircle(eR, 1.8f, 0.f, 0.f, 0.f, 1.f, 8);
}

} // namespace nkpa
} // namespace nkentseu
