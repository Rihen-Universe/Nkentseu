/**
 * @File    NkPATurtle.cpp
 * @Brief   Tortue : carapace hexagonale, 4 pattes IK, cou allongé, tête avec yeux.
 */

#include "Creatures/NkPATurtle.h"
#include <cmath>
#include <cstdlib>

namespace nkentseu {
namespace nkpa {

static float32 Randf(float32 lo, float32 hi) {
    return lo + (float32)rand() / (float32)RAND_MAX * (hi - lo);
}

void Turtle::Init(NkVector2f startPos, int32 worldW, int32 worldH) {
    mShellR = Randf(0.25f, 0.45f);
    mShellG = Randf(0.45f, 0.65f);
    mShellB = Randf(0.05f, 0.2f);
    mSkinR  = mShellR * 0.7f;
    mSkinG  = mShellG * 0.7f;
    mSkinB  = mShellB * 0.6f;
    mBody.Init(startPos, 4, 14.f);
    mNeck.Init(startPos, 3, 8.f);
    mLegFL.Init(startPos, 2, 12.f);
    mLegFR.Init(startPos, 2, 12.f);
    mLegBL.Init(startPos, 2, 12.f);
    mLegBR.Init(startPos, 2, 12.f);
    mAgent.Init(startPos, Randf(50.f, 80.f), Randf(2.f, 4.f), Randf(1.f, 2.f), worldW, worldH);
}

void Turtle::Update(float32 dt) {
    mTime += dt;
    NkVector2f center = mAgent.Update(dt);
    mBody.Update(center);
    // Cou sort du segment avant
    mNeck.Update(center + NkVector2f{ ::cosf(mAgent.HeadAng()) * 18.f,
                                      ::sinf(mAgent.HeadAng()) * 18.f });

    float32 ang = mAgent.HeadAng();
    NkVector2f aF = mBody.Seg(1).pos;
    NkVector2f aB = mBody.Seg(3).pos;
    mLegFL.Update(LegTarget(aF, ang, -22.f, 0.f));
    mLegFR.Update(LegTarget(aF, ang,  22.f, PA_PI));
    mLegBL.Update(LegTarget(aB, ang, -22.f, PA_PI));
    mLegBR.Update(LegTarget(aB, ang,  22.f, 0.f));
}

NkVector2f Turtle::LegTarget(NkVector2f anchor, float32 ang, float32 side, float32 phase) const {
    float32    step = ::sinf(mTime * 3.f + phase) * 0.5f;
    NkVector2f perp = { -::sinf(ang) * side, ::cosf(ang) * side };
    NkVector2f fwd  = {  ::cosf(ang) * 8.f,  ::sinf(ang) * 8.f };
    return anchor + perp + fwd + NkVector2f{ 0.f, (step > 0.f ? step : 0.f) * 5.f };
}

void Turtle::DrawLeg(MeshBuilder& mb, const Chain& leg) const {
    const int32 NL = leg.Count();
    for (int32 i = 0; i < NL - 1; ++i) {
        mb.AddQuadGrad(leg.Seg(i).pos,   6.f, mSkinR, mSkinG, mSkinB, 1.f,
                       leg.Seg(i+1).pos, 5.f, mSkinR*0.8f, mSkinG*0.8f, mSkinB*0.7f, 1.f);
        mb.AddCircle(leg.Seg(i).pos, 6.f, mSkinR, mSkinG, mSkinB, 1.f, 10);
    }
    // Palme / griffe simplifiée
    NkVector2f tip = leg.Seg(NL-1).pos;
    mb.AddCircle(tip, 5.f, mSkinR * 0.75f, mSkinG * 0.75f, mSkinB * 0.6f, 1.f, 8);
    for (int32 c = -1; c <= 1; ++c) {
        float32    a  = leg.Seg(NL-2).angle + (float32)c * 0.35f;
        NkVector2f ct = tip + NkVector2f{ ::cosf(a) * 6.f, ::sinf(a) * 6.f };
        mb.AddTriFlat(tip, tip + NkVector2f{2.f,0.f}, ct, mSkinR*0.5f, mSkinG*0.5f, 0.05f, 0.9f);
    }
}

void Turtle::Draw(MeshBuilder& mb) const {
    // ── Pattes ───────────────────────────────────────────────────────────────
    DrawLeg(mb, mLegFL);
    DrawLeg(mb, mLegFR);
    DrawLeg(mb, mLegBL);
    DrawLeg(mb, mLegBR);

    // ── Cou ──────────────────────────────────────────────────────────────────
    {
        const int32 NC = mNeck.Count();
        for (int32 i = 0; i < NC - 1; ++i) {
            mb.AddQuadGrad(mNeck.Seg(i).pos,   5.f, mSkinR, mSkinG, mSkinB, 1.f,
                           mNeck.Seg(i+1).pos, 5.f, mSkinR, mSkinG, mSkinB, 1.f);
            mb.AddCircle(mNeck.Seg(i).pos, 5.f, mSkinR, mSkinG, mSkinB, 1.f, 10);
        }
    }

    // ── Carapace (ellipse bombée avec motif hexagonal) ────────────────────────
    NkVector2f center = mBody.Seg(0).pos;
    float32    cAng   = mAgent.HeadAng();
    // Ombre portée
    mb.AddEllipse(center + NkVector2f{3.f, 4.f}, 30.f, 22.f, cAng,
                  0.05f, 0.05f, 0.02f, 0.3f, 20);
    // Base de la carapace
    mb.AddEllipse(center, 28.f, 20.f, cAng, mShellR, mShellG, mShellB, 1.f, 24);
    // Motif dorsal (plaques)
    for (int32 row = -1; row <= 1; ++row) {
        for (int32 col = -1; col <= 1; ++col) {
            NkVector2f pc = center + NkVector2f{ ::cosf(cAng) * (float32)col * 11.f
                                                 - ::sinf(cAng) * (float32)row * 8.f,
                                                 ::sinf(cAng) * (float32)col * 11.f
                                                 + ::cosf(cAng) * (float32)row * 8.f };
            mb.AddCircle(pc, 4.5f, mShellR * 0.65f, mShellG * 0.65f, mShellB * 0.5f, 0.6f, 6);
        }
    }
    // Reflet
    mb.AddEllipse(center - NkVector2f{5.f, 6.f}, 14.f, 8.f, cAng + 0.3f,
                  mShellR*1.5f, mShellG*1.5f, mShellB*1.2f, 0.25f, 14);

    // ── Tête ─────────────────────────────────────────────────────────────────
    NkVector2f head = mNeck.Seg(0).pos;
    float32    hAng = mNeck.Seg(0).angle + PA_PI;
    mb.AddEllipse(head, 10.f, 7.f, hAng, mSkinR * 1.1f, mSkinG * 1.05f, mSkinB * 0.9f, 1.f, 16);

    // Yeux
    NkVector2f hperp = { -::sinf(hAng), ::cosf(hAng) };
    NkVector2f eL    = head + hperp * 4.f + NkVector2f{ ::cosf(hAng)*5.f, ::sinf(hAng)*5.f };
    NkVector2f eR    = head - hperp * 4.f + NkVector2f{ ::cosf(hAng)*5.f, ::sinf(hAng)*5.f };
    mb.AddCircle(eL, 3.f, 0.8f, 0.75f, 0.1f, 1.f, 10);
    mb.AddCircle(eR, 3.f, 0.8f, 0.75f, 0.1f, 1.f, 10);
    mb.AddCircle(eL, 1.5f, 0.f, 0.f, 0.f,    1.f, 8);
    mb.AddCircle(eR, 1.5f, 0.f, 0.f, 0.f,    1.f, 8);
}

} // namespace nkpa
} // namespace nkentseu
