#pragma once
/**
 * @File    NkPAEnvironment.h
 * @Brief   Environnement 2 zones : milieu marin (bas) + terrestre (haut).
 *          Inclut eau animée, vent, nuages et particules.
 *
 *  Layout vertical (screenH = hauteur totale) :
 *    [0 .. skyH]       : ciel + nuages + vent
 *    [skyH .. waterY]  : zone terrestre (herbe, rochers)
 *    [waterY .. screenH]: mer/rivière (eau animée, bulles)
 *
 *  waterY ≈ 55 % de screenH
 */

#include "NKCore/NkTypes.h"
#include "NKMath/NkVec.h"
#include "Renderer/NkPAMesh.h"

namespace nkentseu {
namespace nkpa {

using namespace math;

// ─── Particule (vent ou bulle) ─────────────────────────────────────────────

struct EnvParticle {
    NkVector2f pos;
    NkVector2f vel;
    float32    life   = 1.f;  ///< [0,1] — 0 = morte
    float32    size   = 2.f;
    float32    r, g, b, a;
};

// ─── Environnement ─────────────────────────────────────────────────────────

class Environment {
public:
    static constexpr int32 MAX_PARTICLES = 120;
    static constexpr int32 MAX_CLOUDS    = 6;
    static constexpr int32 WATER_VERTS   = 48;  ///< points de la surface d'eau

    void Init(float32 screenW, float32 screenH);
    void Update(float32 dt);
    void Draw(MeshBuilder& mb) const;

    /// Frontière mer / terre (en pixels Y depuis le haut)
    float32 WaterY()  const { return mWaterY; }
    float32 ScreenW() const { return mW; }
    float32 ScreenH() const { return mH; }

private:
    float32 mW = 1280.f, mH = 720.f;
    float32 mWaterY = 0.f;  ///< Y de la surface de l'eau
    float32 mTime   = 0.f;

    // ── Nuages ────────────────────────────────────────────────────────────────
    struct Cloud {
        NkVector2f pos;
        float32    speed;
        float32    scale;
    } mClouds[MAX_CLOUDS];

    // ── Particules vent ───────────────────────────────────────────────────────
    EnvParticle mWind[MAX_PARTICLES / 2];
    float32     mWindTimer = 0.f;

    // ── Particules bulles ─────────────────────────────────────────────────────
    EnvParticle mBubbles[MAX_PARTICLES / 2];
    float32     mBubbleTimer = 0.f;

    // ── Vague de surface ──────────────────────────────────────────────────────
    // On stocke les positions Y de chaque point de la surface
    mutable float32 mWaveSurface[WATER_VERTS];

    void SpawnWind();
    void SpawnBubble();
    void DrawSky(MeshBuilder& mb) const;
    void DrawLand(MeshBuilder& mb) const;
    void DrawWater(MeshBuilder& mb) const;
    void DrawClouds(MeshBuilder& mb) const;
    void DrawWindParticles(MeshBuilder& mb) const;
    void DrawBubbles(MeshBuilder& mb) const;
};

} // namespace nkpa
} // namespace nkentseu
