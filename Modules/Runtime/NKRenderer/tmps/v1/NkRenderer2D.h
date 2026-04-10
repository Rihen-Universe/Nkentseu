#pragma once
// =============================================================================
// NkRenderer2D.h / NkRenderer3D.h
// Helpers haut niveau construits sur NkIRenderer.
//
// Ces classes sont des facades optionnelles — l'application peut utiliser
// NkIRenderer directement ou passer par ces helpers plus expressifs.
//
// NkRenderer2D :
//   Gère automatiquement une caméra 2D orthographique, un batch de sprites
//   et les primitives 2D. API proche de LibGDX / Monogame SpriteBatch.
//
// NkRenderer3D :
//   Gère un pipeline de rendu 3D forward avec shadow mapping optionnel,
//   une liste de lumières et une caméra 3D. API proche d'Irrlicht / Godot.
// =============================================================================
#include "NKRenderer/Core/NkIRenderer.h"
#include "NKRenderer/Core/NkRendererTypes.h"

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // NkRenderer2D — batch de sprites et primitives 2D
    // =========================================================================
    class NkRenderer2D {
    public:
        explicit NkRenderer2D(NkIRenderer* renderer)
            : mRenderer(renderer) {}

        ~NkRenderer2D() { Destroy(); }

        // Initialisation : crée la caméra orthographique et les matériaux internes
        bool Init(uint32 viewW, uint32 viewH);
        void Destroy();

        // ── Frame ────────────────────────────────────────────────────────────
        void Begin(NkRendererCommand& cmd);
        void End();

        // ── Caméra ───────────────────────────────────────────────────────────
        void SetCamera(NkVec2f position, float32 zoom=1.f, float32 rotation=0.f);
        void SetViewport(uint32 w, uint32 h);
        NkCameraHandle GetCamera() const { return mCamera; }

        // ── Sprites ──────────────────────────────────────────────────────────
        // Sprite centré à la position donnée
        void DrawSprite(NkTextureHandle tex,
                        NkVec2f center, NkVec2f size,
                        NkVec4f color = {1,1,1,1},
                        float32 rotation = 0.f,
                        NkVec4f uvRect  = {0,0,1,1},  // normalised
                        float32 depth   = 0.f,
                        uint32  layer   = 0);

        // Sprite par coin (top-left)
        void DrawSpriteCorner(NkTextureHandle tex,
                              NkVec2f topLeft, NkVec2f size,
                              NkVec4f color = {1,1,1,1},
                              float32 depth = 0.f, uint32 layer = 0);

        // ── Primitives ───────────────────────────────────────────────────────
        void FillRect    (NkVec2f pos, NkVec2f size, NkVec4f color,
                          float32 depth=0.f, uint32 layer=0);
        void DrawRect    (NkVec2f pos, NkVec2f size, NkVec4f color,
                          float32 thickness=1.f, float32 depth=0.f, uint32 layer=0);
        void FillCircle  (NkVec2f center, float32 radius, NkVec4f color,
                          uint32 segments=32, float32 depth=0.f, uint32 layer=0);
        void DrawCircle  (NkVec2f center, float32 radius, NkVec4f color,
                          float32 thickness=1.f, uint32 segs=32,
                          float32 depth=0.f, uint32 layer=0);
        void DrawLine    (NkVec2f start, NkVec2f end, NkVec4f color,
                          float32 thickness=1.f, float32 depth=0.f, uint32 layer=0);
        void DrawArrow   (NkVec2f start, NkVec2f end, NkVec4f color,
                          float32 thickness=1.f, float32 headSize=12.f,
                          float32 depth=0.f, uint32 layer=0);
        void FillTriangle(NkVec2f a, NkVec2f b, NkVec2f c, NkVec4f color,
                          float32 depth=0.f, uint32 layer=0);

        // ── Texte ────────────────────────────────────────────────────────────
        void DrawText(NkFontHandle font, const char* text,
                      NkVec2f position, float32 size,
                      NkVec4f color={1,1,1,1},
                      float32 maxWidth=0.f, uint32 layer=0);

        // Mesure d'un texte (en pixels screen)
        NkVec2f MeasureText(NkFontHandle font, const char* text, float32 size) const;

        // ── Transforms ───────────────────────────────────────────────────────
        void PushTransform(NkMat3f transform);
        void PopTransform();
        NkMat3f GetCurrentTransform() const;

    private:
        NkIRenderer*      mRenderer = nullptr;
        NkRendererCommand* mCmd     = nullptr;
        NkCameraHandle    mCamera;
        NkVec2f           mViewport = {1280.f, 720.f};

        struct TransformStack {
            NkMat3f transforms[32];
            int32   top = 0;
        } mTransformStack;
    };

    // =========================================================================
    // NkRenderer3D — rendu 3D forward avec lumières et shadows
    // =========================================================================
    struct NkRenderer3DConfig {
        bool  shadowMapping  = true;
        uint32 shadowMapSize = 2048;
        uint32 maxPointLights= 8;
        bool  msaa           = false;
        uint32 msaaSamples   = 4;
        bool  postProcess    = false;
    };

    class NkRenderer3D {
    public:
        explicit NkRenderer3D(NkIRenderer* renderer)
            : mRenderer(renderer) {}

        ~NkRenderer3D() { Destroy(); }

        bool Init(const NkRenderer3DConfig& config = {});
        void Destroy();

        // ── Caméra ───────────────────────────────────────────────────────────
        void SetCamera(const NkCameraDesc3D& desc);
        void SetCameraLookAt(NkVec3f eye, NkVec3f target, NkVec3f up={0,1,0});
        void SetCameraFOV(float32 fovDegrees);
        void SetCameraClip(float32 nearPlane, float32 farPlane);
        void SetViewport(uint32 w, uint32 h);
        NkCameraHandle GetCamera() const { return mCamera; }

        NkMat4f GetViewMatrix()       const;
        NkMat4f GetProjectionMatrix() const;
        NkMat4f GetViewProjection()   const;

        // ── Raycasting utilitaire ────────────────────────────────────────────
        // Convertit un point écran (px, py) en rayon monde
        void ScreenToWorldRay(uint32 px, uint32 py,
                              uint32 screenW, uint32 screenH,
                              NkVec3f& outOrigin, NkVec3f& outDir) const;

        // ── Lumières ─────────────────────────────────────────────────────────
        void SetAmbientLight(NkVec3f color, float32 intensity=0.03f);
        void SetSunLight    (NkVec3f direction, NkVec3f color, float32 intensity=1.f,
                             bool castShadow=true);
        void AddPointLight  (NkVec3f position, NkVec3f color,
                             float32 intensity=1.f, float32 radius=10.f,
                             bool castShadow=false);
        void AddSpotLight   (NkVec3f position, NkVec3f direction,
                             NkVec3f color, float32 intensity=1.f,
                             float32 radius=15.f, float32 innerAngle=15.f,
                             float32 outerAngle=30.f, bool castShadow=false);
        void ClearLights();

        // ── Draw calls ────────────────────────────────────────────────────────
        void Begin(NkRendererCommand& cmd);
        void End();

        void DrawMesh  (NkMeshHandle mesh, NkMaterialInstHandle mat,
                        const NkMat4f& transform,
                        bool castShadow=true, bool receiveShadow=true,
                        uint32 layer=0);
        void DrawModel (NkModelHandle model, const NkMat4f& transform,
                        uint32 layer=0);
        void DrawSkybox(NkTextureHandle cubemap);

        // ── Texte world-space ────────────────────────────────────────────────
        void DrawText3D(NkFontHandle font, const char* text,
                        NkVec3f position, float32 size,
                        NkVec4f color={1,1,1,1}, bool billboard=true);

        // ── Debug ─────────────────────────────────────────────────────────────
        void DrawGrid      (uint32 cells=20, float32 cellSize=1.f,
                            NkVec4f color={0.4f,0.4f,0.4f,1.f});
        void DrawAxes      (NkVec3f origin={0,0,0}, float32 size=1.f);
        void DrawFrustum   (const NkCameraDesc3D& cam, NkVec4f color={0.8f,0.8f,0.2f,1.f});
        void DrawBoundingBox(NkVec3f min, NkVec3f max, NkVec4f color={0,1,0,1});
        void DrawSphere    (NkVec3f center, float32 radius, NkVec4f color={0,0,1,1});
        void DrawLine3D    (NkVec3f start, NkVec3f end, NkVec4f color={1,0,0,1},
                            float32 thickness=1.f);

        // ── Render target (offscreen) ─────────────────────────────────────────
        NkRenderTargetHandle GetMainRenderTarget() const { return mMainRT; }
        NkRenderTargetHandle GetShadowMap()        const { return mShadowRT; }

    private:
        NkIRenderer*         mRenderer = nullptr;
        NkRendererCommand*   mCmd      = nullptr;
        NkRenderer3DConfig   mConfig;

        NkCameraHandle       mCamera;
        NkRenderTargetHandle mMainRT;
        NkRenderTargetHandle mShadowRT;

        struct LightData {
            NkVector<NkLightDesc> lights;
            NkVec3f               ambient     = {0.03f, 0.03f, 0.03f};
            float32               ambientIntensity = 1.f;
        } mLightData;
    };

} // namespace renderer
} // namespace nkentseu