#pragma once
// =============================================================================
// NkOverlayRenderer.h
// Rendu d'overlay : UI, debug, HUD, gizmos, wireframe, profiling.
//
// L'overlay est rendu après toute la géométrie 3D et le post-process,
// directement sur le framebuffer final (sans depth test par défaut).
//
// Features :
//   - Debug draw (lignes, sphères, AABB, frustums)
//   - Statistiques de rendu on-screen
//   - Gizmos de transformation (translate/rotate/scale)
//   - Grid overlay
//   - Wireframe overlay (re-rend la scène en wireframe par-dessus)
//   - Profiling HUD (GPU timings, draw counts)
//   - Custom overlays (callback)
// =============================================================================
#include "NKRHI/Core/NkTypes.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"

namespace nkentseu {

    // =============================================================================
    // Type de gizmo
    // =============================================================================
    enum class NkGizmoType : uint8 {
        NK_TRANSLATE,
        NK_ROTATE,
        NK_SCALE,
        NK_UNIVERSAL,   // translate + rotate + scale en un
    };

    // =============================================================================
    // Résultat d'interaction gizmo
    // =============================================================================
    struct NkGizmoResult {
        bool           hovered       = false;
        bool           used          = false;    // en cours de drag
        math::NkMat4   deltaTransform;           // delta appliqué ce frame
        int32          activeAxis    = -1;       // 0=X 1=Y 2=Z 3=XY 4=YZ 5=XZ 6=XYZ
    };

    // =============================================================================
    // NkOverlayRenderer
    // =============================================================================
    class NkOverlayRenderer {
        public:
            explicit NkOverlayRenderer(NkIDevice* device,
                                        NkRender2D* render2D,
                                        NkRender3D* render3D);
            ~NkOverlayRenderer();

            bool Initialize(NkRenderPassHandle overlayPass, uint32 width, uint32 height);
            void Shutdown();
            void OnResize(uint32 width, uint32 height);

            // ── Frame ─────────────────────────────────────────────────────────────────
            void Begin(NkICommandBuffer* cmd, const NkCamera3D& camera);
            void End();

            // ── Debug draw 3D ─────────────────────────────────────────────────────────
            void DrawLine3D      (math::NkVec3 from, math::NkVec3 to, NkColor32 color, float32 duration = 0.f); // duration=0: un seul frame
            void DrawBox         (const NkAABB& box, const math::NkMat4& transform, NkColor32 color);
            void DrawSphere      (math::NkVec3 center, float32 radius, NkColor32 color, uint32 segments = 32);
            void DrawFrustum     (const NkCamera3D& cam, NkColor32 color);
            void DrawAxes        (const math::NkMat4& transform, float32 size = 0.5f);
            void DrawGrid        (uint32 lineCount = 20, float32 cellSize = 1.f, NkColor32 color = {60,60,60,200}, NkColor32 centerColor = {100,100,100,255});
            void DrawCross       (math::NkVec3 pos, float32 size, NkColor32 color);
            void DrawDirectional (math::NkVec3 pos, math::NkVec3 dir, float32 length, NkColor32 color);

            // ── Gizmos ────────────────────────────────────────────────────────────────
            // Affiche un gizmo de transformation interactif.
            // mousePos et mouseButton: état de la souris pour l'interaction.
            // Retourne le delta de transformation appliqué ce frame.
            NkGizmoResult DrawGizmo(math::NkMat4& transform, NkGizmoType type, math::NkVec2 mousePos, bool mouseButton, float32 size = 1.f);

            // ── Overlays 2D ───────────────────────────────────────────────────────────
            // Ces fonctions passent par NkRender2D en coordonnées pixel.
            void DrawLabel3D     (math::NkVec3 worldPos, const NkString& text, NkColor32 color = NkColor32::White(), float32 fontSize = 14.f, uint32 fontId = 0);
            void DrawStats       (bool enabled = true, math::NkVec2 position = {10,10}, uint32 fontId = 0);
            void DrawFPS         (math::NkVec2 position = {10,10}, NkColor32 color = NkColor32::White(), uint32 fontId = 0);
            void DrawProfiler    (math::NkVec2 position = {10,60}, uint32 fontId = 0);
            void DrawLightVolumes(bool enabled = true);
            void DrawShadowCascades(bool enabled = true);
            void DrawWireframeOverlay(bool enabled, NkColor32 color = {0,180,255,100});
            void DrawNormalsOverlay(bool enabled, float32 length = 0.1f);

            // ── HUD custom ───────────────────────────────────────────────────────────
            // Accès direct au NkRender2D de l'overlay pour du rendu custom
            NkRender2D* GetHUD() { return mRender2D; }

            // ── Persistent lines (debug de physique, etc.) ────────────────────────────
            // Ces lignes persistent jusqu'à expiration de leur durée de vie.
            void AddPersistentLine(math::NkVec3 from, math::NkVec3 to, NkColor32 color, float32 lifetimeSec);
            void ClearPersistentLines();

            // ── Données de stats à injecter ───────────────────────────────────────────
            struct FrameStats {
                float32 fps          = 0.f;
                float32 frameMs      = 0.f;
                float32 gpuMs        = 0.f;
                float32 cpuMs        = 0.f;
                uint32  drawCalls    = 0;
                uint32  triangles    = 0;
                uint32  lightCount   = 0;
                uint64  gpuMemoryMB  = 0;
            };
            void SetFrameStats(const FrameStats& stats) { mStats = stats; }

            // ── Submit ────────────────────────────────────────────────────────────────
            void Submit(NkICommandBuffer* cmd);

        private:
            NkIDevice*   mDevice;
            NkRender2D*  mRender2D;
            NkRender3D*  mRender3D;
            NkCamera3D   mCamera;
            uint32       mWidth, mHeight;
            FrameStats   mStats;

            // Debug draw accumulé
            struct DebugLine3D {
                math::NkVec3 from, to;
                NkColor32    color;
                float32      remainingLife = 0.f;
                bool         persistent   = false;
            };
            NkVector<DebugLine3D>  mLines;
            NkVector<DebugLine3D>  mPersistentLines;

            bool mShowStats     = false;
            bool mLightVolumes  = false;
            bool mShadowCascadesViz = false;
            bool mWireframeOverlay  = false;
            NkColor32 mWireframeColor = {0,180,255,80};
            bool mNormalsOverlay    = false;
            float32 mNormalsLength  = 0.1f;
            bool mInsideFrame   = false;

            // Gizmo state
            struct GizmoState {
                int32        activeAxis  = -1;
                bool         dragging    = false;
                math::NkVec3 dragStart;
                math::NkMat4 transformStart;
            } mGizmoState;

            NkBufferHandle mLineVBO, mLineIBO;
            NkPipelineHandle mLinePipeline;
            NkShaderHandle   mLineShader;

            void FlushLines3D(NkICommandBuffer* cmd);
            void DrawStatsInternal(uint32 fontId);
            math::NkVec2 WorldToScreen(math::NkVec3 worldPos) const;
            void UpdatePersistentLines(float32 dt);
    };

} // namespace nkentseu