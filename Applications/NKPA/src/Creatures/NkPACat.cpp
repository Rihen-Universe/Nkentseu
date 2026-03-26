/**
 * @File    NkPACat.cpp
 * @Brief   Chat : corps souple, queue courbe, 4 pattes IK, tête avec oreilles pointues, moustaches.
 */

#include "Creatures/NkPACat.h"
#include <cmath>
#include <cstdlib>

namespace nkentseu {
namespace nkpa {

static float32 Randf(float32 lo, float32 hi) {
    return lo + (float32)rand() / (float32)RAND_MAX * (hi - lo);
}

void Cat::Init(NkVector2f startPos, int32 worldW, int32 worldH) {
    mCr = Randf(0.55f, 0.85f);
    mCg = Randf(0.45f, 0.7f);
    mCb = Randf(0.3f, 0.55f);
    mBody.Init(startPos, 8,  11.f);
    mTail.Init(startPos, 10, 8.f);
    mLegFL.Init(startPos, 3, 11.f);
    mLegFR.Init(startPos, 3, 11.f);
    mLegBL.Init(startPos, 3, 11.f);
    mLegBR.Init(startPos, 3, 11.f);
    mAgent.Init(startPos, Randf(80.f, 130.f), Randf(3.f, 6.f), Randf(1.5f, 2.5f), worldW, worldH);
}

void Cat::Update(float32 dt) {
    mTime += dt;
    NkVector2f head = mAgent.Update(dt);
    mBody.Update(head);

    // Queue suit l'arrière-train (segment 6) avec courbe
    NkVector2f tailBase = mBody.Seg(6).pos;
    float32    ang      = mBody.Seg(6).angle;
    float32    curlAng  = ang + PA_PI + ::sinf(mTime * 1.5f) * 0.8f;
    NkVector2f tailTarget = tailBase + NkVector2f{ ::cosf(curlAng) * 35.f,
                                                    ::sinf(curlAng) * 35.f };
    mTail.Update(tailTarget);

    float32    ba  = mAgent.HeadAng();
    NkVector2f aF  = mBody.Seg(2).pos;
    NkVector2f aB  = mBody.Seg(5).pos;
    mLegFL.Update(LegTarget(aF, ba, -18.f, 0.f));
    mLegFR.Update(LegTarget(aF, ba,  18.f, PA_PI));
    mLegBL.Update(LegTarget(aB, ba, -18.f, PA_PI));
    mLegBR.Update(LegTarget(aB, ba,  18.f, 0.f));
}

float32 Cat::BodyRadius(int32 i) const {
    const int32 N = mBody.Count();
    float32 t = (float32)i / (float32)(N - 1);
    if (t < 0.2f) return 7.f + t / 0.2f * 5.f;
    return 12.f * (1.f - (t - 0.2f) * 0.7f);
}

NkVector2f Cat::LegTarget(NkVector2f anchor, float32 ang, float32 side, float32 phase) const {
    float32    step = ::sinf(mTime * 5.f + phase);
    NkVector2f perp = { -::sinf(ang) * side, ::cosf(ang) * side };
    NkVector2f fwd  = {  ::cosf(ang) * 12.f, ::sinf(ang) * 12.f };
    float32    lift = (step > 0.f ? step : 0.f) * 8.f;
    return anchor + perp + fwd + NkVector2f{ 0.f, lift };
}

void Cat::DrawLeg(MeshBuilder& mb, const Chain& leg) const {
    const int32 NL = leg.Count();
    for (int32 i = 0; i < NL - 1; ++i) {
        float32 rad0 = 4.5f - (float32)i * 0.5f;
        float32 rad1 = 4.5f - (float32)(i+1) * 0.5f;
        if (rad0 < 2.f) rad0 = 2.f;
        if (rad1 < 2.f) rad1 = 2.f;
        mb.AddQuadGrad(leg.Seg(i).pos,   rad0, mCr*0.85f, mCg*0.8f, mCb*0.75f, 1.f,
                       leg.Seg(i+1).pos, rad1, mCr*0.7f,  mCg*0.65f,mCb*0.6f,  1.f);
        mb.AddCircle(leg.Seg(i).pos, rad0, mCr*0.85f, mCg*0.8f, mCb*0.75f, 1.f, 10);
    }
    // Patte (coussinet)
    NkVector2f tip = leg.Seg(NL-1).pos;
    mb.AddCircle(tip, 3.5f, mCr*0.65f, mCg*0.55f, mCb*0.5f, 1.f, 8);
}

void Cat::Draw(MeshBuilder& mb) const {
    // ── Queue ────────────────────────────────────────────────────────────────
    {
        const int32 NT = mTail.Count();
        for (int32 i = 0; i < NT - 1; ++i) {
            float32 t    = (float32)i / (float32)(NT - 1);
            float32 rad0 = 4.5f * (1.f - t * 0.6f);
            float32 rad1 = 4.5f * (1.f - (t + 1.f/(float32)(NT-1)) * 0.6f);
            mb.AddQuadGrad(mTail.Seg(i).pos,   rad0, mCr, mCg, mCb, 1.f,
                           mTail.Seg(i+1).pos, rad1, mCr*0.85f, mCg*0.8f, mCb*0.75f, 1.f);
            mb.AddCircle(mTail.Seg(i).pos, rad0, mCr, mCg, mCb, 1.f, 10);
        }
        // Bout de queue légèrement plus clair
        mb.AddCircle(mTail.Seg(mTail.Count()-1).pos, 5.f,
                     mCr*1.15f, mCg*1.1f, mCb*1.05f, 1.f, 10);
    }

    // ── Pattes ───────────────────────────────────────────────────────────────
    DrawLeg(mb, mLegFL);
    DrawLeg(mb, mLegFR);
    DrawLeg(mb, mLegBL);
    DrawLeg(mb, mLegBR);

    // ── Corps ────────────────────────────────────────────────────────────────
    const int32 N = mBody.Count();
    for (int32 i = 0; i < N - 1; ++i) {
        float32 sh = 0.8f + 0.2f * (1.f - (float32)i / (float32)(N-1));
        mb.AddQuadGrad(mBody.Seg(i).pos,   BodyRadius(i),
                       mCr*sh, mCg*sh, mCb*sh, 1.f,
                       mBody.Seg(i+1).pos, BodyRadius(i+1),
                       mCr*sh, mCg*sh, mCb*sh, 1.f);
        mb.AddCircle(mBody.Seg(i).pos, BodyRadius(i), mCr*sh, mCg*sh, mCb*sh, 1.f, 14);
    }

    // ── Tête ─────────────────────────────────────────────────────────────────
    NkVector2f head = mBody.Seg(0).pos;
    float32    ang  = mBody.Seg(0).angle + PA_PI;
    // Museau légèrement aplati
    mb.AddEllipse(head, 13.f, 11.f, ang, mCr * 1.05f, mCg * 1.0f, mCb * 0.95f, 1.f, 18);

    NkVector2f perp = { -::sinf(ang), ::cosf(ang) };

    // Oreilles (triangles pointus)
    for (int32 s = -1; s <= 1; s += 2) {
        NkVector2f earBase0 = head + perp * (6.f * (float32)s)
                                   + NkVector2f{ ::cosf(ang)*6.f, ::sinf(ang)*6.f };
        NkVector2f earBase1 = head + perp * (10.f * (float32)s)
                                   + NkVector2f{ ::cosf(ang)*4.f, ::sinf(ang)*4.f };
        NkVector2f earTip   = head + perp * (8.f * (float32)s)
                                   + NkVector2f{ ::cosf(ang)*16.f, ::sinf(ang)*16.f };
        mb.AddTriFlat(earBase0, earBase1, earTip, mCr, mCg, mCb, 1.f);
        // Intérieur rose
        NkVector2f iBase0 = head + perp * (6.5f * (float32)s)
                                 + NkVector2f{ ::cosf(ang)*6.5f, ::sinf(ang)*6.5f };
        NkVector2f iBase1 = head + perp * (9.5f * (float32)s)
                                 + NkVector2f{ ::cosf(ang)*4.5f, ::sinf(ang)*4.5f };
        NkVector2f iTip   = head + perp * (8.f * (float32)s)
                                 + NkVector2f{ ::cosf(ang)*13.f, ::sinf(ang)*13.f };
        mb.AddTriFlat(iBase0, iBase1, iTip, 0.95f, 0.65f, 0.7f, 0.8f);
    }

    // Yeux (amande)
    NkVector2f eL = head + perp * 5.f + NkVector2f{ ::cosf(ang)*4.f, ::sinf(ang)*4.f };
    NkVector2f eR = head - perp * 5.f + NkVector2f{ ::cosf(ang)*4.f, ::sinf(ang)*4.f };
    mb.AddEllipse(eL, 4.f, 2.5f, ang + PA_PI * 0.1f, 0.3f, 0.8f, 0.25f, 1.f, 10);
    mb.AddEllipse(eR, 4.f, 2.5f, ang - PA_PI * 0.1f, 0.3f, 0.8f, 0.25f, 1.f, 10);
    mb.AddEllipse(eL, 2.f, 3.f, ang, 0.05f, 0.05f, 0.05f, 1.f, 8); // pupille fendue
    mb.AddEllipse(eR, 2.f, 3.f, ang, 0.05f, 0.05f, 0.05f, 1.f, 8);
    // Reflet
    mb.AddCircle(eL + NkVector2f{1.f, -1.f}, 1.f, 1.f, 1.f, 1.f, 0.8f, 6);
    mb.AddCircle(eR + NkVector2f{1.f, -1.f}, 1.f, 1.f, 1.f, 1.f, 0.8f, 6);

    // Nez
    NkVector2f nose = head + NkVector2f{ ::cosf(ang)*8.f, ::sinf(ang)*8.f };
    mb.AddEllipse(nose, 3.f, 2.f, ang, 0.85f, 0.5f, 0.55f, 1.f, 8);

    // Moustaches
    for (int32 s = -1; s <= 1; s += 2) {
        for (int32 r = 0; r < 3; ++r) {
            float32    whiskerAng = ang + (float32)s * (0.1f + (float32)r * 0.15f);
            NkVector2f wBase = nose + perp * (3.f * (float32)s);
            NkVector2f wTip  = wBase + NkVector2f{ ::cosf(whiskerAng + (float32)s * PA_PI * 0.5f) * 18.f,
                                                    ::sinf(whiskerAng + (float32)s * PA_PI * 0.5f) * 18.f };
            mb.AddTriFlat(wBase, wBase + NkVector2f{0.5f, 0.f}, wTip, 0.9f, 0.85f, 0.8f, 0.7f);
        }
    }
}

} // namespace nkpa
} // namespace nkentseu
