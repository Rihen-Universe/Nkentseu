/**
 * @File    NkPALizard.cpp
 * @Brief   Lézard procédural : corps, queue, 4 pattes IK, griffes.
 */

#include "Creatures/NkPALizard.h"
#include <cmath>
#include <cstdlib>

namespace nkentseu {
namespace nkpa {

static float32 Randf(float32 lo, float32 hi) {
    return lo + (float32)rand() / (float32)RAND_MAX * (hi - lo);
}

void Lizard::Init(NkVector2f startPos, int32 worldW, int32 worldH) {
    mCr = Randf(0.35f, 0.6f);
    mCg = Randf(0.45f, 0.7f);
    mCb = Randf(0.05f, 0.2f);
    mBody.Init(startPos, 10, 12.f);
    mTail.Init(startPos, 8,  9.f);
    mLegFL.Init(startPos, 3, 14.f);
    mLegFR.Init(startPos, 3, 14.f);
    mLegBL.Init(startPos, 3, 14.f);
    mLegBR.Init(startPos, 3, 14.f);
    mAgent.Init(startPos, Randf(70.f, 110.f), Randf(3.f, 6.f), Randf(1.5f, 2.5f), worldW, worldH);
}

void Lizard::Update(float32 dt) {
    mTime += dt;
    NkVector2f head = mAgent.Update(dt);
    mBody.Update(head);
    mTail.Update(mBody.Seg(mBody.Count() - 1).pos);

    float32    ang = mBody.Seg(0).angle;
    NkVector2f aF  = LegAnchor(2);
    NkVector2f aB  = LegAnchor(6);

    mLegFL.Update(LegTarget(aF, ang, -20.f, 0.f));
    mLegFR.Update(LegTarget(aF, ang,  20.f, PA_PI));
    mLegBL.Update(LegTarget(aB, ang, -20.f, PA_PI));
    mLegBR.Update(LegTarget(aB, ang,  20.f, 0.f));
}

float32 Lizard::BodyRadius(int32 i) const {
    const int32 N = mBody.Count();
    float32 t = (float32)i / (float32)(N - 1);
    if (t < 0.15f) return 9.f + t / 0.15f * 5.f;
    return 14.f * (1.f - (t - 0.15f) * 0.85f);
}

NkVector2f Lizard::LegAnchor(int32 bodyIdx) const {
    int32 idx = bodyIdx < mBody.Count() ? bodyIdx : mBody.Count() - 1;
    return mBody.Seg(idx).pos;
}

NkVector2f Lizard::LegTarget(NkVector2f anchor, float32 ang, float32 side, float32 phase) const {
    float32    stepPhase = ::sinf(mTime * 4.f + phase);
    NkVector2f perp      = { -::sinf(ang) * side, ::cosf(ang) * side };
    NkVector2f fwd       = {  ::cosf(ang) * 10.f, ::sinf(ang) * 10.f };
    float32    lift      = (stepPhase > 0.f ? stepPhase : 0.f) * 6.f;
    return anchor + perp + fwd + NkVector2f{ 0.f, lift };
}

void Lizard::DrawLeg(MeshBuilder& mb, const Chain& leg) const {
    const int32 NL = leg.Count();
    for (int32 i = 0; i < NL - 1; ++i) {
        float32 pct  = (float32)i / (float32)NL;
        float32 rad0 = 5.f - pct * 2.f;
        float32 rad1 = 5.f - (pct + 1.f / (float32)NL) * 2.f;
        mb.AddQuadGrad(leg.Seg(i).pos,     rad0, mCr*0.8f, mCg*0.7f, mCb*0.6f, 1.f,
                       leg.Seg(i+1).pos,   rad1, mCr*0.6f, mCg*0.5f, mCb*0.4f, 1.f);
        mb.AddCircle(leg.Seg(i).pos, rad0, mCr*0.8f, mCg*0.7f, mCb*0.6f, 1.f, 10);
    }

    // Griffe (3 pointes)
    NkVector2f tip = leg.Seg(NL - 1).pos;
    float32    ang = leg.Seg(NL - 2).angle;
    for (int32 c = -1; c <= 1; ++c) {
        float32    a2    = ang + (float32)c * 0.35f;
        NkVector2f cBase = tip + NkVector2f{ ::cosf(ang + PA_PI) * 3.f,
                                             ::sinf(ang + PA_PI) * 3.f };
        NkVector2f cTip  = tip + NkVector2f{ ::cosf(a2) * 7.f, ::sinf(a2) * 7.f };
        NkVector2f cpv   = { ::cosf(ang + PA_PI * 0.5f), ::sinf(ang + PA_PI * 0.5f) };
        mb.AddTriFlat(cBase + cpv, cBase - cpv, cTip, 0.15f, 0.15f, 0.1f, 1.f);
    }
    mb.AddCircle(tip, 3.f, mCr*0.5f, mCg*0.4f, mCb*0.3f, 1.f, 8);
}

void Lizard::Draw(MeshBuilder& mb) const {
    // ── Pattes (sous le corps) ───────────────────────────────────────────────
    DrawLeg(mb, mLegFL);
    DrawLeg(mb, mLegFR);
    DrawLeg(mb, mLegBL);
    DrawLeg(mb, mLegBR);

    // ── Queue ────────────────────────────────────────────────────────────────
    const int32 NT = mTail.Count();
    for (int32 i = 0; i < NT - 1; ++i) {
        float32 t    = (float32)i / (float32)(NT - 1);
        float32 rad0 = 7.f * (1.f - t);
        float32 rad1 = 7.f * (1.f - (t + 1.f / (float32)(NT - 1)));
        mb.AddQuadGrad(mTail.Seg(i).pos,   rad0, mCr*0.9f, mCg*0.8f, mCb*0.5f, 1.f,
                       mTail.Seg(i+1).pos, rad1, mCr*0.7f, mCg*0.6f, mCb*0.3f, 1.f);
        mb.AddCircle(mTail.Seg(i).pos, rad0, mCr*0.9f, mCg*0.8f, mCb*0.5f, 1.f, 12);
    }

    // ── Corps ────────────────────────────────────────────────────────────────
    const int32 N = mBody.Count();
    for (int32 i = 0; i < N - 1; ++i) {
        float32 t  = (float32)i / (float32)(N - 1);
        float32 sh = 0.8f + 0.2f * (1.f - t);
        mb.AddQuadGrad(mBody.Seg(i).pos,   BodyRadius(i),   mCr*sh, mCg*sh, mCb*sh, 1.f,
                       mBody.Seg(i+1).pos, BodyRadius(i+1), mCr*sh, mCg*sh, mCb*sh, 1.f);
        mb.AddCircle(mBody.Seg(i).pos, BodyRadius(i), mCr*sh, mCg*sh, mCb*sh, 1.f, 14);
    }
    mb.AddCircle(mBody.Seg(N-1).pos, BodyRadius(N-1), mCr, mCg, mCb, 1.f, 14);

    // ── Tête ────────────────────────────────────────────────────────────────
    NkVector2f head  = mBody.Seg(0).pos;
    float32    ang   = mBody.Seg(0).angle + PA_PI;
    NkVector2f snout = head + NkVector2f{ ::cosf(ang) * 16.f, ::sinf(ang) * 16.f };
    NkVector2f perp  = { -::sinf(ang), ::cosf(ang) };
    mb.AddTriFlat(head + perp * 13.f, head - perp * 13.f, snout, mCr*1.1f, mCg*1.0f, mCb*0.7f, 1.f);
    mb.AddCircle(head, 13.f, mCr*1.1f, mCg*1.0f, mCb*0.7f, 1.f, 16);

    // ── Yeux ────────────────────────────────────────────────────────────────
    NkVector2f eL = head + perp * 8.f + NkVector2f{ ::cosf(ang) * 5.f, ::sinf(ang) * 5.f };
    NkVector2f eR = head - perp * 8.f + NkVector2f{ ::cosf(ang) * 5.f, ::sinf(ang) * 5.f };
    mb.AddCircle(eL, 4.f, 0.9f, 0.8f, 0.1f, 1.f, 10);
    mb.AddCircle(eR, 4.f, 0.9f, 0.8f, 0.1f, 1.f, 10);
    mb.AddCircle(eL, 2.f, 0.f,  0.f,  0.f,  1.f, 8);
    mb.AddCircle(eR, 2.f, 0.f,  0.f,  0.f,  1.f, 8);
}

} // namespace nkpa
} // namespace nkentseu
