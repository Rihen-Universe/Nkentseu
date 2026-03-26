/**
 * @File    NkPAEnvironment.cpp
 * @Brief   Implémentation de l'environnement 2 zones + effets.
 */

#include "Environment/NkPAEnvironment.h"
#include <cmath>
#include <cstdlib>

namespace nkentseu {
namespace nkpa {

static float32 RandfE(float32 lo, float32 hi) {
    return lo + (float32)rand() / (float32)RAND_MAX * (hi - lo);
}

// ─────────────────────────────────────────────────────────────────────────────

void Environment::Init(float32 screenW, float32 screenH) {
    mW      = screenW;
    mH      = screenH;
    mWaterY = screenH * 0.52f;

    // Nuages
    for (int32 i = 0; i < MAX_CLOUDS; ++i) {
        mClouds[i].pos   = { RandfE(0.f, mW), RandfE(20.f, mWaterY * 0.45f) };
        mClouds[i].speed = RandfE(8.f, 22.f);
        mClouds[i].scale = RandfE(0.6f, 1.4f);
    }

    // Particules vent (invalides)
    for (auto& p : mWind)   p.life = 0.f;
    for (auto& p : mBubbles) p.life = 0.f;
}

void Environment::SpawnWind() {
    for (auto& p : mWind) {
        if (p.life <= 0.f) {
            p.pos  = { RandfE(-20.f, 0.f), RandfE(10.f, mWaterY - 20.f) };
            p.vel  = { RandfE(40.f, 90.f), RandfE(-8.f, 8.f) };
            p.life = RandfE(0.5f, 1.5f);
            p.size = RandfE(1.f, 3.f);
            p.r = 0.85f; p.g = 0.88f; p.b = 0.92f; p.a = 0.45f;
            return;
        }
    }
}

void Environment::SpawnBubble() {
    for (auto& p : mBubbles) {
        if (p.life <= 0.f) {
            p.pos  = { RandfE(20.f, mW - 20.f), mH - 5.f };
            p.vel  = { RandfE(-5.f, 5.f), RandfE(-25.f, -10.f) };
            p.life = RandfE(1.f, 3.f);
            p.size = RandfE(1.5f, 4.f);
            p.r = 0.7f; p.g = 0.85f; p.b = 0.95f; p.a = 0.4f;
            return;
        }
    }
}

void Environment::Update(float32 dt) {
    mTime += dt;

    // Nuages (scrollent vers la droite, rebouclent)
    for (auto& c : mClouds) {
        c.pos.x += c.speed * dt;
        if (c.pos.x > mW + 120.f) {
            c.pos.x  = -120.f;
            c.pos.y  = RandfE(20.f, mWaterY * 0.45f);
            c.scale  = RandfE(0.6f, 1.4f);
        }
    }

    // Vent
    mWindTimer += dt;
    if (mWindTimer >= 0.08f) { mWindTimer = 0.f; SpawnWind(); }
    for (auto& p : mWind) {
        if (p.life <= 0.f) continue;
        p.pos  = p.pos + p.vel * dt;
        p.life -= dt;
        if (p.pos.x > mW + 10.f) p.life = 0.f;
    }

    // Bulles
    mBubbleTimer += dt;
    if (mBubbleTimer >= 0.25f) { mBubbleTimer = 0.f; SpawnBubble(); }
    for (auto& p : mBubbles) {
        if (p.life <= 0.f) continue;
        p.pos  = p.pos + p.vel * dt;
        p.life -= dt;
        if (p.pos.y < mWaterY) p.life = 0.f;
    }

    // Calcul du surface de vague (précalc pour Draw)
    for (int32 i = 0; i < WATER_VERTS; ++i) {
        float32 x      = mW * (float32)i / (float32)(WATER_VERTS - 1);
        float32 wave1  = ::sinf(x * 0.018f + mTime * 1.8f) * 6.f;
        float32 wave2  = ::sinf(x * 0.04f  - mTime * 2.5f) * 3.f;
        float32 wave3  = ::sinf(x * 0.009f + mTime * 0.9f) * 4.5f;
        mWaveSurface[i] = mWaterY + wave1 + wave2 + wave3;
    }
}

void Environment::DrawSky(MeshBuilder& mb) const {
    // Gradient ciel : bleu clair en haut → bleu-vert vers l'horizon
    float32 w = mW, h = mWaterY;
    mb.AddTriangle({ 0.f, 0.f }, 0.35f, 0.55f, 0.85f, 1.f,
                   { w,   0.f }, 0.35f, 0.55f, 0.85f, 1.f,
                   { 0.f, h   }, 0.45f, 0.7f,  0.6f,  1.f);
    mb.AddTriangle({ w,   0.f }, 0.35f, 0.55f, 0.85f, 1.f,
                   { w,   h   }, 0.45f, 0.7f,  0.6f,  1.f,
                   { 0.f, h   }, 0.45f, 0.7f,  0.6f,  1.f);
}

void Environment::DrawLand(MeshBuilder& mb) const {
    float32 w = mW, top = mWaterY, bot = mH;

    // Sol de base (gradient sable → terre)
    mb.AddTriangle({ 0.f, top }, 0.55f, 0.50f, 0.30f, 1.f,
                   { w,   top }, 0.55f, 0.50f, 0.30f, 1.f,
                   { 0.f, bot }, 0.35f, 0.28f, 0.18f, 1.f);
    mb.AddTriangle({ w,   top }, 0.55f, 0.50f, 0.30f, 1.f,
                   { w,   bot }, 0.35f, 0.28f, 0.18f, 1.f,
                   { 0.f, bot }, 0.35f, 0.28f, 0.18f, 1.f);

    // Bande d'herbe à la frontière terre/eau
    float32 grassH = 10.f;
    mb.AddTriangle({ 0.f, top          }, 0.25f, 0.55f, 0.2f, 1.f,
                   { w,   top          }, 0.25f, 0.55f, 0.2f, 1.f,
                   { 0.f, top + grassH }, 0.15f, 0.4f,  0.1f, 1.f);
    mb.AddTriangle({ w,   top          }, 0.25f, 0.55f, 0.2f, 1.f,
                   { w,   top + grassH }, 0.15f, 0.4f,  0.1f, 1.f,
                   { 0.f, top + grassH }, 0.15f, 0.4f,  0.1f, 1.f);

    // Quelques rochers (ellipses statiques, positionnées à l'init)
    // On les re-dessine chaque frame (positions fixes)
    const NkVector2f rocks[] = {
        {100.f, top + 18.f}, {340.f, top + 22.f}, {620.f, top + 14.f},
        {900.f, top + 20.f}, {1150.f, top + 16.f}
    };
    const float32 rsx[] = {22.f, 18.f, 25.f, 15.f, 20.f};
    const float32 rsy[] = {12.f, 10.f, 14.f, 9.f,  11.f};
    for (int32 r = 0; r < 5; ++r) {
        mb.AddEllipse(rocks[r], rsx[r], rsy[r], 0.f,
                      0.38f, 0.35f, 0.32f, 1.f, 12);
        mb.AddEllipse(rocks[r] - NkVector2f{3.f, 3.f}, rsx[r]*0.6f, rsy[r]*0.5f, 0.f,
                      0.5f, 0.48f, 0.45f, 0.5f, 10);
    }
}

void Environment::DrawWater(MeshBuilder& mb) const {
    float32 w = mW, bot = mH;

    // Corps de l'eau (sous la surface)
    mb.AddTriangle({ 0.f, mWaterY }, 0.06f, 0.18f, 0.38f, 1.f,
                   { w,   mWaterY }, 0.06f, 0.18f, 0.38f, 1.f,
                   { 0.f, bot     }, 0.03f, 0.10f, 0.22f, 1.f);
    mb.AddTriangle({ w,   mWaterY }, 0.06f, 0.18f, 0.38f, 1.f,
                   { w,   bot     }, 0.03f, 0.10f, 0.22f, 1.f,
                   { 0.f, bot     }, 0.03f, 0.10f, 0.22f, 1.f);

    // Surface ondulée (triangle strip)
    for (int32 i = 0; i < WATER_VERTS - 1; ++i) {
        float32    x0 = mW * (float32)i     / (float32)(WATER_VERTS - 1);
        float32    x1 = mW * (float32)(i+1) / (float32)(WATER_VERTS - 1);
        float32    y0 = mWaveSurface[i];
        float32    y1 = mWaveSurface[i+1];
        float32    yBase = mWaterY + 18.f;
        mb.AddTriangle({ x0, y0 }, 0.25f, 0.55f, 0.78f, 0.75f,
                       { x1, y1 }, 0.25f, 0.55f, 0.78f, 0.75f,
                       { x0, yBase }, 0.06f, 0.18f, 0.38f, 0.8f);
        mb.AddTriangle({ x1, y1 }, 0.25f, 0.55f, 0.78f, 0.75f,
                       { x1, yBase }, 0.06f, 0.18f, 0.38f, 0.8f,
                       { x0, yBase }, 0.06f, 0.18f, 0.38f, 0.8f);
    }

    // Reflets scintillants
    for (int32 i = 0; i < WATER_VERTS; i += 3) {
        float32    x = mW * (float32)i / (float32)(WATER_VERTS - 1);
        float32    y = mWaveSurface[i];
        float32    sc = 0.3f + 0.7f * ::fabsf(::sinf(mTime * 4.f + (float32)i));
        mb.AddCircle({ x, y }, 1.5f, 0.85f, 0.92f, 1.f, sc * 0.6f, 6);
    }
}

void Environment::DrawClouds(MeshBuilder& mb) const {
    for (const auto& c : mClouds) {
        float32 s = c.scale;
        NkVector2f p = c.pos;
        // Nuage = 3 ellipses superposées
        mb.AddEllipse(p,                      35.f*s, 16.f*s, 0.f, 0.96f,0.96f,0.98f, 0.85f, 16);
        mb.AddEllipse(p + NkVector2f{22.f*s, -4.f*s}, 26.f*s, 14.f*s, 0.f, 0.94f,0.94f,0.97f, 0.8f, 14);
        mb.AddEllipse(p - NkVector2f{20.f*s,  2.f*s}, 24.f*s, 13.f*s, 0.f, 0.94f,0.94f,0.97f, 0.8f, 14);
    }
}

void Environment::DrawWindParticles(MeshBuilder& mb) const {
    for (const auto& p : mWind) {
        if (p.life <= 0.f) continue;
        float32 alpha = (p.life > 0.3f ? 1.f : p.life / 0.3f) * p.a;
        // Petite ligne de vent (deux triangles minuscules)
        NkVector2f d = { p.vel.x * 0.03f, p.vel.y * 0.03f };
        mb.AddTriFlat(p.pos, p.pos + NkVector2f{0.f, p.size * 0.3f},
                      p.pos + d, p.r, p.g, p.b, alpha);
    }
}

void Environment::DrawBubbles(MeshBuilder& mb) const {
    for (const auto& p : mBubbles) {
        if (p.life <= 0.f) continue;
        float32 alpha = (p.life > 0.4f ? 1.f : p.life / 0.4f) * p.a;
        mb.AddCircle(p.pos, p.size, p.r, p.g, p.b, alpha, 8);
    }
}

void Environment::Draw(MeshBuilder& mb) const {
    DrawSky(mb);
    DrawClouds(mb);
    DrawLand(mb);
    DrawWater(mb);
    DrawWindParticles(mb);
    DrawBubbles(mb);
}

} // namespace nkpa
} // namespace nkentseu
