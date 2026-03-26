/**
 * @File    NkPABird.cpp
 * @Brief   Oiseau : corps 6 segments, ailes battantes, bec, oeil.
 */

#include "Creatures/NkPABird.h"
#include <cmath>
#include <cstdlib>

namespace nkentseu {
namespace nkpa {

static float32 Randf(float32 lo, float32 hi) {
    return lo + (float32)rand() / (float32)RAND_MAX * (hi - lo);
}

void Bird::Init(NkVector2f startPos, int32 worldW, int32 worldH) {
    mCr = Randf(0.05f, 0.35f);
    mCg = Randf(0.05f, 0.35f);
    mCb = Randf(0.05f, 0.35f);
    mWr = mCr + Randf(0.4f, 0.7f);
    mWg = mCg + Randf(0.4f, 0.7f);
    mWb = mCb + Randf(0.4f, 0.7f);
    mWr = mWr > 1.f ? 1.f : mWr;
    mWg = mWg > 1.f ? 1.f : mWg;
    mWb = mWb > 1.f ? 1.f : mWb;
    mBody.Init(startPos, 6, 10.f);
    mAgent.Init(startPos, Randf(90.f, 150.f), Randf(2.f, 5.f), Randf(1.5f, 3.f), worldW, worldH);
}

void Bird::Update(float32 dt) {
    mTime += dt;
    mBody.Update(mAgent.Update(dt));
}

void Bird::Draw(MeshBuilder& mb) const {
    const int32 N = mBody.Count();
    if (N < 2) return;

    float32 wingBeat = ::sinf(mTime * 8.f);

    // ── Ailes (dessinées en premier) ─────────────────────────────────────────
    // Point d'attache : segment 1 (épaules)
    NkVector2f shoulder = mBody.Seg(1).pos;
    float32    ang      = mBody.Seg(1).angle;
    float32    wAng     = ang + PA_PI;

    // Aile gauche (3 parties : épaule→coude→bout)
    float32    upLift = wingBeat * 30.f;
    NkVector2f perp   = { -::sinf(wAng), ::cosf(wAng) };
    NkVector2f elbow  = shoulder + perp * 20.f + NkVector2f{ 0.f, -upLift };
    NkVector2f wingtip= elbow   + perp * 22.f + NkVector2f{ 0.f, -upLift * 0.5f };
    mb.AddFin(shoulder, elbow, wingtip, mWr, mWg, mWb, 0.9f, mWr*0.7f, mWg*0.7f, mWb*0.7f, 0.6f);
    mb.AddTriFlat(shoulder - NkVector2f{4.f,0.f}, shoulder + NkVector2f{4.f,0.f}, elbow,
                  mWr, mWg, mWb, 0.85f);

    // Aile droite (symétrique)
    NkVector2f elbow2   = shoulder - perp * 20.f + NkVector2f{ 0.f, -upLift };
    NkVector2f wingtip2 = elbow2   - perp * 22.f + NkVector2f{ 0.f, -upLift * 0.5f };
    mb.AddFin(shoulder, elbow2, wingtip2, mWr, mWg, mWb, 0.9f, mWr*0.7f, mWg*0.7f, mWb*0.7f, 0.6f);
    mb.AddTriFlat(shoulder - NkVector2f{4.f,0.f}, shoulder + NkVector2f{4.f,0.f}, elbow2,
                  mWr, mWg, mWb, 0.85f);

    // ── Queue (segments 3-5) ─────────────────────────────────────────────────
    {
        NkVector2f tailBase = mBody.Seg(N-2).pos;
        NkVector2f tailTip  = mBody.Seg(N-1).pos;
        float32    tAng     = mBody.Seg(N-2).angle;
        NkVector2f tPerp    = { -::sinf(tAng), ::cosf(tAng) };
        mb.AddFin(tailBase + tPerp * 8.f, tailBase - tPerp * 8.f, tailTip,
                  mCr, mCg, mCb, 1.f, mCr*0.7f, mCg*0.7f, mCb*0.7f, 0.6f);
    }

    // ── Corps ────────────────────────────────────────────────────────────────
    for (int32 i = 0; i < N - 1; ++i) {
        float32 rad0 = (i == 0) ? 9.f : 7.f - (float32)i;
        float32 rad1 = (i+1 == 0) ? 9.f : 7.f - (float32)(i+1);
        if (rad0 < 2.f) rad0 = 2.f;
        if (rad1 < 2.f) rad1 = 2.f;
        mb.AddQuadGrad(mBody.Seg(i).pos,   rad0, mCr, mCg, mCb, 1.f,
                       mBody.Seg(i+1).pos, rad1, mCr, mCg, mCb, 1.f);
        mb.AddCircle(mBody.Seg(i).pos, rad0, mCr, mCg, mCb, 1.f, 12);
    }

    // ── Tête ─────────────────────────────────────────────────────────────────
    NkVector2f head = mBody.Seg(0).pos;
    float32    hAng = mBody.Seg(0).angle + PA_PI;
    mb.AddCircle(head, 9.f, mCr * 1.15f, mCg * 1.15f, mCb * 1.1f, 1.f, 16);

    // Bec
    NkVector2f beakBase = head + NkVector2f{ ::cosf(hAng) * 8.f, ::sinf(hAng) * 8.f };
    NkVector2f beakTip  = beakBase + NkVector2f{ ::cosf(hAng) * 9.f, ::sinf(hAng) * 9.f };
    NkVector2f beakPerp = { -::sinf(hAng), ::cosf(hAng) };
    mb.AddTriFlat(beakBase + beakPerp * 3.f, beakBase - beakPerp * 3.f, beakTip,
                  0.9f, 0.7f, 0.1f, 1.f);

    // Oeil
    NkVector2f eyeOff = beakPerp * 4.f + NkVector2f{ ::cosf(hAng) * 5.f, ::sinf(hAng) * 5.f };
    NkVector2f eye    = head + eyeOff;
    mb.AddCircle(eye, 3.f, 0.95f, 0.95f, 0.95f, 1.f, 10);
    mb.AddCircle(eye, 1.6f, 0.f, 0.f, 0.f,      1.f, 8);
}

} // namespace nkpa
} // namespace nkentseu
