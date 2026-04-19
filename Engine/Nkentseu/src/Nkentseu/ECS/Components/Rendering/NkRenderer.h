#pragma once
// -----------------------------------------------------------------------------
// FICHIER: NkECS/Components/Rendering/NkRenderer.h
// DESCRIPTION: Composants de rendu 2D et 3D.
//   NkRenderer2D    — rendu 2D (sprite, quad coloré, texturé)
//   NkRenderer3D    — rendu 3D (mesh, matériau)
//   NkSprite        — sprite 2D avec atlas UV, animation de frames
//   NkSpriteAtlas   — atlas de sprites (partagé entre plusieurs entités)
//   NkLight         — source lumineuse (point, directionnel, spot, area)
//   NkParticleSystem— système de particules
//   NkTrailRenderer — tracer de chemin (trail)
//   NkLineRenderer  — rendu de lignes 3D
// -----------------------------------------------------------------------------

#include "../../NkECSDefines.h"
#include "../../Core/NkTypeRegistry.h"
#include "../Core/NkTransform.h"
#include <cstring>
#include <cmath>

namespace nkentseu { namespace ecs {

// =============================================================================
// Types partagés de rendu
// =============================================================================

struct NkColor4 {
    float32 r = 1.f, g = 1.f, b = 1.f, a = 1.f; // RGBA [0..1]
    NkColor4() = default;
    constexpr NkColor4(float32 r, float32 g, float32 b, float32 a = 1.f) noexcept
        : r(r), g(g), b(b), a(a) {}
    static NkColor4 White()   noexcept { return {1,1,1,1}; }
    static NkColor4 Black()   noexcept { return {0,0,0,1}; }
    static NkColor4 Red()     noexcept { return {1,0,0,1}; }
    static NkColor4 Green()   noexcept { return {0,1,0,1}; }
    static NkColor4 Blue()    noexcept { return {0,0,1,1}; }
    static NkColor4 Clear()   noexcept { return {0,0,0,0}; }
    static NkColor4 Yellow()  noexcept { return {1,1,0,1}; }
    static NkColor4 Cyan()    noexcept { return {0,1,1,1}; }
    static NkColor4 Magenta() noexcept { return {1,0,1,1}; }
    static NkColor4 FromU8(uint8 r, uint8 g, uint8 b, uint8 a = 255) noexcept {
        return { r/255.f, g/255.f, b/255.f, a/255.f };
    }
    static NkColor4 FromHex(uint32 hex) noexcept {
        return { ((hex>>16)&0xFF)/255.f, ((hex>>8)&0xFF)/255.f,
                 (hex&0xFF)/255.f, ((hex>>24)&0xFF)/255.f };
    }
};

struct NkRect2D {
    float32 x = 0.f, y = 0.f, w = 1.f, h = 1.f;
    NkRect2D() = default;
    constexpr NkRect2D(float32 x, float32 y, float32 w, float32 h) noexcept
        : x(x), y(y), w(w), h(h) {}
};

// Modes de blend
enum class NkBlendMode : uint8 {
    Opaque      = 0,
    AlphaBlend  = 1,
    Additive    = 2,
    Multiply    = 3,
    Premult     = 4,
    Screen      = 5,
};

// Flip d'image
enum class NkFlipMode : uint8 {
    None  = 0, Horizontal = 1, Vertical = 2, Both = 3
};

// =============================================================================
// NkRenderer2D — rendu d'une entité 2D (quad, sprite, shapes)
// =============================================================================

struct NkRenderer2D {
    uint32      textureId    = 0;        // 0 = quad coloré sans texture
    NkColor4    color        = NkColor4::White();
    NkRect2D    uvRect       = {0,0,1,1};// portion UV de la texture
    NkBlendMode blendMode    = NkBlendMode::AlphaBlend;
    NkFlipMode  flip         = NkFlipMode::None;
    int32       sortingOrder = 0;        // ordre de rendu dans la couche
    int32       sortingLayer = 0;        // index de la couche (0 = default)
    NkVec2      pivot        = {0.5f, 0.5f}; // point de pivot [0..1]
    NkVec2      size         = {1.f, 1.f};   // taille en unités monde
    bool        visible      = true;
    bool        castShadow   = false;
    bool        receiveShadow = false;
    bool        pixelPerfect = false;    // arrondit la position au pixel

    // Propriétés matériau custom
    uint32      shaderId     = 0;         // 0 = shader par défaut
    float32     shaderParams[8] = {};     // paramètres custom au shader
};
NK_COMPONENT(NkRenderer2D)

// =============================================================================
// NkSprite — sprite 2D avec gestion d'atlas et animation de frames
// =============================================================================

struct NkSpriteFrame {
    NkRect2D uvRect  = {0,0,1,1}; // portion UV dans l'atlas
    NkVec2   pivot   = {0.5f, 0.5f};
    float32  duration = 1.f / 12.f; // durée de la frame en secondes
};

struct NkSprite {
    static constexpr uint32 kMaxFrames = 256u;

    uint32      textureId   = 0;
    NkColor4    color       = NkColor4::White();
    NkBlendMode blendMode   = NkBlendMode::AlphaBlend;
    NkFlipMode  flip        = NkFlipMode::None;
    int32       sortingOrder = 0;
    int32       sortingLayer = 0;
    NkVec2      pivot        = {0.5f, 0.5f};
    NkVec2      size         = {1.f, 1.f};
    bool        visible      = true;
    bool        pixelPerfect = false;

    // ── Animation de frames ───────────────────────────────────────────────────
    NkSpriteFrame frames[kMaxFrames] = {};
    uint32        frameCount   = 1;
    uint32        currentFrame = 0;
    float32       frameTimer   = 0.f;
    bool          playing      = false;
    bool          loop         = true;
    float32       playbackSpeed = 1.f;

    // ── UV actuel (calculé depuis frames[currentFrame]) ────────────────────
    NkRect2D    currentUV    = {0,0,1,1};

    void SetSingleFrame(uint32 texId, NkRect2D uv = {0,0,1,1}) noexcept {
        textureId = texId;
        frameCount = 1;
        frames[0].uvRect = uv;
        frames[0].duration = 1.f;
        currentUV = uv;
    }

    void Play() noexcept  { playing = true; }
    void Pause() noexcept { playing = false; }
    void Stop()  noexcept { playing = false; currentFrame = 0; frameTimer = 0.f; }

    void Update(float32 dt) noexcept {
        if (!playing || frameCount <= 1) return;
        frameTimer += dt * playbackSpeed;
        if (frameTimer >= frames[currentFrame].duration) {
            frameTimer -= frames[currentFrame].duration;
            currentFrame = (currentFrame + 1) % frameCount;
            if (!loop && currentFrame == 0) { playing = false; currentFrame = frameCount - 1; }
            currentUV = frames[currentFrame].uvRect;
        }
    }
};
NK_COMPONENT(NkSprite)

// =============================================================================
// NkRenderer3D — rendu d'une entité 3D (static mesh)
// =============================================================================

enum class NkShadowCastMode : uint8 { Off = 0, On = 1, TwoSided = 2, ShadowsOnly = 3 };
enum class NkLODMode : uint8 { Auto = 0, Fixed = 1, None = 2 };

struct NkRenderer3D {
    uint32   meshId          = 0;       // handle vers le mesh GPU
    uint32   materialIds[8]  = {};      // matériaux par sous-mesh (max 8)
    uint32   materialCount   = 1;

    NkShadowCastMode castShadow  = NkShadowCastMode::On;
    bool     receiveShadow   = true;
    bool     visible         = true;
    uint32   cullingMask     = 0xFFFFFFFFu;

    NkLODMode lodMode        = NkLODMode::Auto;
    float32   lodBias        = 0.f;
    uint32    lodLevel       = 0;       // override si lodMode == Fixed

    // Instancing (pour grandes quantités d'entités identiques)
    bool     useInstancing   = false;
    uint32   instanceGroupId = 0;       // regroupe les entités avec le même group

    NkColor4 colorOverride   = NkColor4::White(); // tint multiplicatif
    float32  shaderParams[8] = {};       // paramètres custom
};
NK_COMPONENT(NkRenderer3D)

// =============================================================================
// NkLight — source lumineuse
// =============================================================================

enum class NkLightType : uint8 {
    Directional = 0, // lumière directionnelle (soleil)
    Point       = 1, // lumière ponctuelle (ampoule)
    Spot        = 2, // lumière spot (lampe de poche)
    Area        = 3, // lumière de zone (panneau lumineux)
    Ambient     = 4, // lumière ambiante globale
};

struct NkLight {
    NkLightType type        = NkLightType::Point;
    NkColor4    color       = NkColor4::White();
    float32     intensity   = 1.f;
    float32     range       = 10.f;       // portée (point / spot)
    float32     spotAngle   = 30.f;       // demi-angle du cone spot (degrés)
    float32     spotBlend   = 0.1f;       // douceur du bord du cone
    bool        castShadow  = true;
    uint32      shadowResolution = 1024u;  // résolution de la shadow map
    float32     shadowBias  = 0.005f;
    float32     shadowNormalBias = 0.4f;
    NkVec2      areaSize    = {1.f, 1.f}; // taille de la source (area light)
    bool        enabled     = true;

    // Lumière volumétrique (God rays)
    bool        volumetric  = false;
    float32     volumetricIntensity = 0.1f;
    float32     volumetricScatter   = 0.3f;

    // Flare
    uint32      flareTextureId = 0;
    float32     flareSize      = 0.f;  // 0 = pas de flare
};
NK_COMPONENT(NkLight)

// =============================================================================
// NkParticleSystem — système de particules
// =============================================================================

enum class NkParticleSimulationSpace : uint8 { World = 0, Local = 1 };
enum class NkParticleEmitterShape : uint8 {
    Sphere = 0, Hemisphere = 1, Cone = 2, Box = 3,
    Circle = 4, Edge = 5, Mesh = 6
};

struct NkParticleSystem {
    // ── Emission ──────────────────────────────────────────────────────────────
    float32 emissionRate       = 10.f;   // particules par seconde
    float32 burstCount         = 0.f;    // burst immédiat
    float32 duration           = 5.f;   // durée du système (secondes)
    bool    looping            = true;
    bool    prewarm            = false;
    bool    playing            = false;
    float32 time               = 0.f;   // temps courant de simulation

    // ── Durée de vie des particules ────────────────────────────────────────────
    float32 startLifetimeMin   = 1.f;
    float32 startLifetimeMax   = 2.f;

    // ── Vitesse ───────────────────────────────────────────────────────────────
    float32 startSpeedMin      = 1.f;
    float32 startSpeedMax      = 3.f;

    // ── Taille ────────────────────────────────────────────────────────────────
    float32 startSizeMin       = 0.1f;
    float32 startSizeMax       = 0.3f;
    bool    sizeOverLifetime   = false;
    float32 endSize            = 0.f;

    // ── Couleur ───────────────────────────────────────────────────────────────
    NkColor4 startColor        = NkColor4::White();
    NkColor4 endColor          = NkColor4{1,1,1,0}; // fade out
    bool     colorOverLifetime = true;

    // ── Rotation ──────────────────────────────────────────────────────────────
    float32 startRotationMin   = 0.f;
    float32 startRotationMax   = 360.f;
    float32 angularVelocityMin = 0.f;
    float32 angularVelocityMax = 0.f;

    // ── Gravité et physique ───────────────────────────────────────────────────
    float32 gravityModifier    = 0.f;   // 1 = gravité pleine
    NkParticleSimulationSpace simSpace = NkParticleSimulationSpace::World;

    // ── Shape d'émission ──────────────────────────────────────────────────────
    NkParticleEmitterShape shape = NkParticleEmitterShape::Sphere;
    float32 shapeRadius          = 1.f;
    float32 shapeAngle           = 25.f;  // cone angle
    NkVec3  shapeBox             = {1,1,1};

    // ── Rendu ─────────────────────────────────────────────────────────────────
    uint32  textureId            = 0;
    NkBlendMode blendMode        = NkBlendMode::Additive;
    uint32  maxParticles         = 1000u;
    bool    stretchBillboard     = false; // stretch selon la vélocité

    // ── Etat runtime (géré par NkParticleSystem) ─────────────────────────────
    uint32  activeParticles      = 0;

    void Play()    noexcept { playing = true;  time = 0.f; }
    void Stop()    noexcept { playing = false; time = 0.f; activeParticles = 0; }
    void Pause()   noexcept { playing = false; }
    void Resume()  noexcept { playing = true;  }
};
NK_COMPONENT(NkParticleSystem)

// =============================================================================
// NkTrailRenderer — tracer de chemin (trail, sillage)
// =============================================================================

struct NkTrailRenderer {
    static constexpr uint32 kMaxPoints = 128u;
    float32  time         = 0.5f;    // durée de vie du trail en secondes
    float32  startWidth   = 0.2f;
    float32  endWidth     = 0.f;
    NkColor4 startColor   = NkColor4::White();
    NkColor4 endColor     = NkColor4{1,1,1,0};
    uint32   textureId    = 0;
    NkBlendMode blendMode = NkBlendMode::AlphaBlend;
    bool     enabled      = true;
    float32  minVertexDistance = 0.1f; // distance min entre deux points
};
NK_COMPONENT(NkTrailRenderer)

// =============================================================================
// NkLineRenderer — rendu de lignes 3D (debug, visualisations)
// =============================================================================

struct NkLineRenderer {
    static constexpr uint32 kMaxPoints = 256u;
    NkVec3   points[kMaxPoints] = {};
    uint32   pointCount  = 0;
    float32  width       = 0.1f;
    NkColor4 color       = NkColor4::White();
    bool     loop        = false;
    bool     worldSpace  = true;
    bool     enabled     = true;
};
NK_COMPONENT(NkLineRenderer)

// =============================================================================
// NkSkybox — skybox de la scène
// =============================================================================

struct NkSkybox {
    uint32  cubemapId   = 0;      // handle vers la cubemap GPU
    float32 exposure    = 1.f;
    float32 rotation    = 0.f;   // rotation en degrés
    bool    enabled     = true;
};
NK_COMPONENT(NkSkybox)

}} // namespace nkentseu::ecs
