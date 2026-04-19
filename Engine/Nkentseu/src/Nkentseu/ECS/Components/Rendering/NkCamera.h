#pragma once
// -----------------------------------------------------------------------------
// FICHIER: NkECS/Components/Rendering/NkCamera.h
// DESCRIPTION: Composants caméra — virtuelle (jeu/éditeur) et physique (webcam).
//   NkCamera          — caméra virtuelle 2D/3D (perspective/orthographique)
//   NkCameraPhysical  — caméra physique réelle (webcam, appareil photo, téléphone)
//   NkCameraTarget    — cible qu'une caméra suit (look-at)
// -----------------------------------------------------------------------------

#include "../../NkECSDefines.h"
#include "../../Core/NkTypeRegistry.h"
#include "../Core/NkTransform.h"
#include <cstring>
#include <cmath>

namespace nkentseu { namespace ecs {

// =============================================================================
// Énumérations communes aux caméras
// =============================================================================

enum class NkProjectionMode : uint8 {
    Perspective   = 0,   // caméra 3D standard (FOV + near/far)
    Orthographic  = 1,   // caméra 2D ou rendu technique (size en unités monde)
    Frustum       = 2,   // projection frustum personnalisée
};

enum class NkClearMode : uint8 {
    SkyboxOrColor = 0,  // efface avec couleur de fond ou skybox
    DepthOnly     = 1,  // n'efface que le depth buffer
    None          = 2,  // ne clear rien (GUI overlay, multi-caméra)
    Color         = 3,  // efface uniquement avec une couleur unie
};

// =============================================================================
// NkCamera — caméra virtuelle (jeu, éditeur, cinématique)
// =============================================================================

struct NkCamera {
    // ── Projection ────────────────────────────────────────────────────────────
    NkProjectionMode projectionMode = NkProjectionMode::Perspective;

    float32 fieldOfViewDeg  = 60.f;   // FOV vertical en degrés (perspective)
    float32 orthographicSize = 5.f;   // demi-hauteur en unités monde (ortho)
    float32 nearClip        = 0.1f;   // plan near
    float32 farClip         = 1000.f; // plan far
    float32 aspectRatio     = 16.f/9.f; // largeur/hauteur (mis à jour au resize)

    // ── Priorité et rendu ─────────────────────────────────────────────────────
    int32   priority        = 0;       // plus grand = dessus (multi-caméra)
    NkVec4  viewport        = {0.f, 0.f, 1.f, 1.f}; // [x,y,w,h] normalisé [0..1]

    NkClearMode clearMode   = NkClearMode::SkyboxOrColor;
    NkVec4  backgroundColor = {0.1f, 0.1f, 0.15f, 1.f}; // RGBA

    // ── Rendu ─────────────────────────────────────────────────────────────────
    uint32  cullingMask     = 0xFFFFFFFFu; // layers rendus (bitfield)
    bool    isMainCamera    = false;
    bool    isOrthographic  = false; // shorthand
    bool    hdr             = false;
    bool    msaa            = true;
    uint32  msaaSamples     = 4;
    bool    renderToTexture = false;
    uint32  targetTextureId = 0;     // ID de la texture cible (0 = écran)

    // ── Post-processing ───────────────────────────────────────────────────────
    bool    postProcessing  = true;
    float32 gamma           = 2.2f;
    float32 exposure        = 1.f;
    bool    bloom           = false;
    bool    ambientOcclusion = false;
    bool    depthOfField    = false;
    float32 dofFocusDistance = 10.f;
    float32 dofAperture      = 2.8f;

    // ── 2D spécifique ─────────────────────────────────────────────────────────
    float32 zoom            = 1.f;  // zoom orthographique
    NkVec2  offset2D        = {};   // décalage XY de la caméra 2D

    // ── Matrices calculées (readonly, mises à jour par NkCameraSystem) ─────────
    NkMat4  viewMatrix       = NkMat4::Identity();
    NkMat4  projMatrix       = NkMat4::Identity();
    NkMat4  viewProjMatrix   = NkMat4::Identity();

    // ── Helpers ───────────────────────────────────────────────────────────────

    // Calcule la projection perspective
    void RecalcProjection(float32 aspect = 0.f) noexcept {
        if (aspect > 0.f) aspectRatio = aspect;
        if (projectionMode == NkProjectionMode::Perspective) {
            const float32 f = 1.f / std::tan(fieldOfViewDeg * 0.5f * (3.14159265f/180.f));
            const float32 nd = nearClip, fd = farClip;
            projMatrix.m[ 0] = f / aspectRatio;
            projMatrix.m[ 5] = f;
            projMatrix.m[10] = (fd + nd) / (nd - fd);
            projMatrix.m[11] = -1.f;
            projMatrix.m[14] = (2.f * fd * nd) / (nd - fd);
            projMatrix.m[15] = 0.f;
            projMatrix.m[1]=projMatrix.m[2]=projMatrix.m[3]=0;
            projMatrix.m[4]=projMatrix.m[6]=projMatrix.m[7]=0;
            projMatrix.m[8]=projMatrix.m[9]=0;
            projMatrix.m[12]=projMatrix.m[13]=0;
        } else {
            // Orthographique
            const float32 hw = orthographicSize * aspectRatio * zoom;
            const float32 hh = orthographicSize * zoom;
            const float32 nd = nearClip, fd = farClip;
            projMatrix = NkMat4::Identity();
            projMatrix.m[0]  =  1.f / hw;
            projMatrix.m[5]  =  1.f / hh;
            projMatrix.m[10] = -2.f / (fd - nd);
            projMatrix.m[14] = -(fd + nd) / (fd - nd);
        }
    }

    // Converts screen coords [0..1] to world ray (perspective)
    struct Ray { NkVec3 origin; NkVec3 direction; };
    Ray ScreenToWorldRay(float32 ndcX, float32 ndcY) const noexcept {
        // inverse-project from NDC to world (simplified)
        const float32 aspect = aspectRatio;
        const float32 tanHalf = std::tan(fieldOfViewDeg * 0.5f * (3.14159265f / 180.f));
        const NkVec3 dir = NkVec3{ndcX * aspect * tanHalf, ndcY * tanHalf, -1.f}.Normalized();
        return { {viewMatrix.m[12], viewMatrix.m[13], viewMatrix.m[14]}, dir };
    }

    void SetOrthographic(float32 size) noexcept {
        projectionMode = NkProjectionMode::Orthographic;
        isOrthographic = true;
        orthographicSize = size;
        RecalcProjection();
    }

    void SetPerspective(float32 fovDeg, float32 near = 0.1f, float32 far = 1000.f) noexcept {
        projectionMode = NkProjectionMode::Perspective;
        isOrthographic = false;
        fieldOfViewDeg = fovDeg;
        nearClip = near;
        farClip  = far;
        RecalcProjection();
    }
};
NK_COMPONENT(NkCamera)

// =============================================================================
// NkCameraPhysical — caméra physique réelle (webcam, appareil photo, téléphone)
// Permet d'intégrer un flux vidéo physique dans la scène (AR, capture, analyse)
// =============================================================================

enum class NkPhysicalCameraType : uint8 {
    Webcam      = 0,   // webcam ordinateur
    Smartphone  = 1,   // caméra téléphone
    DSLR        = 2,   // appareil photo reflex / hybride
    IPCamera    = 3,   // caméra réseau (RTSP)
    Capture     = 4,   // carte de capture (HDMI, etc.)
    Depth       = 5,   // caméra de profondeur (Azure Kinect, RealSense)
    Stereo      = 6,   // caméra stéréoscopique
    Thermal     = 7,   // caméra thermique
    Virtual     = 8,   // flux virtuel (pour test)
};

struct NkCameraPhysical {
    NkPhysicalCameraType type     = NkPhysicalCameraType::Webcam;
    char    deviceId[256]         = {};   // ex: "/dev/video0", "USB\\VID_..."
    char    rtspUrl[512]          = {};   // pour IPCamera
    char    deviceName[128]       = {};   // nom lisible

    // ── Paramètres d'acquisition ──────────────────────────────────────────────
    uint32  captureWidth  = 1920;
    uint32  captureHeight = 1080;
    float32 captureFrameRate = 30.f;
    uint32  captureFormatFourCC = 0;   // ex: 'MJPG', 'YUY2', 'NV12'

    // ── Paramètres d'exposition (photo/vidéo physique) ────────────────────────
    float32 iso           = 100.f;
    float32 shutterSpeed  = 1.f/60.f; // en secondes
    float32 aperture      = 2.8f;     // f-number
    float32 focalLength   = 50.f;     // en mm
    float32 sensorWidth   = 36.f;     // en mm (full frame = 36mm)
    float32 sensorHeight  = 24.f;     // en mm
    bool    autoExposure  = true;
    bool    autoFocus     = true;
    float32 manualFocusDist = 1.f;    // en mètres

    // ── État runtime ──────────────────────────────────────────────────────────
    bool    isOpen        = false;
    bool    isStreaming   = false;
    uint32  frameCount    = 0;
    double  lastFrameTime = 0.0;
    uint32  textureId     = 0;         // ID GPU de la texture vidéo

    // ── Calibration (pour AR) ─────────────────────────────────────────────────
    float32 focalLengthPxX = 0.f;    // focal en pixels (calibration intrinsèque)
    float32 focalLengthPxY = 0.f;
    float32 principalX     = 0.f;    // point principal
    float32 principalY     = 0.f;
    float32 distortion[8]  = {};     // coefficients de distorsion (k1,k2,p1,p2,k3,k4,k5,k6)
    bool    hasCalibration = false;

    // ── Helpers ───────────────────────────────────────────────────────────────

    // FOV calculé depuis les paramètres physiques (équivalent 35mm)
    [[nodiscard]] float32 ComputeFOVDegrees() const noexcept {
        if (sensorHeight > 0.f && focalLength > 0.f)
            return 2.f * std::atan(sensorHeight * 0.5f / focalLength) * (180.f / 3.14159265f);
        return 60.f;
    }

    [[nodiscard]] float32 ComputeAspectRatio() const noexcept {
        if (captureHeight > 0) return static_cast<float32>(captureWidth) / captureHeight;
        return 16.f / 9.f;
    }

    void SetResolution(uint32 w, uint32 h) noexcept {
        captureWidth  = w;
        captureHeight = h;
    }
};
NK_COMPONENT(NkCameraPhysical)

// =============================================================================
// NkCameraTarget — cible de caméra (pour smooth follow / orbit)
// =============================================================================

struct NkCameraTarget {
    NkEntityId  target          = NkEntityId::Invalid();
    NkVec3      offset          = {0.f, 2.f, -5.f}; // décalage depuis la cible
    float32     smoothSpeed     = 5.f;
    float32     lookAheadFactor = 0.f;   // regarde devant la cible
    bool        lockX           = false;
    bool        lockY           = false;
    bool        lockZ           = false;
    bool        orbitMode       = false; // mode orbite autour de la cible
    float32     orbitRadius     = 5.f;
    float32     orbitAngleH     = 0.f;  // angle horizontal (radians)
    float32     orbitAngleV     = 0.5f; // angle vertical (radians)
};
NK_COMPONENT(NkCameraTarget)

}} // namespace nkentseu::ecs
