#pragma once
// =============================================================================
// NkOverlayRenderer.h
// Rendu d'overlays : debug, gizmos, stats, HUD temps-réel.
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
    namespace renderer {

        class NkResourceManager;

        class NkOverlayRenderer {
           public:
                NkOverlayRenderer()  = default;
                ~NkOverlayRenderer() { Shutdown(); }

                bool Init    (NkIDevice* device, NkResourceManager* resources,
                               uint32 width, uint32 height);
                void Shutdown();
                void Resize  (uint32 w, uint32 h);
                bool IsReady () const { return mReady; }

                // ── Frame ─────────────────────────────────────────────────────────────────
                void Begin(NkICommandBuffer* cmd, const NkCamera3D* cam3D = nullptr);
                void End  ();

                // ── Lignes 3D ─────────────────────────────────────────────────────────────
                void DrawLine3D (const NkVec3f& a, const NkVec3f& b,
                                  const NkColorF& c = NkColorF::White(),
                                  float32 thick = 1.f);
                void DrawAABB   (const NkAABB& aabb, const NkColorF& c = NkColorF::Green());
                void DrawSphere (const NkVec3f& center, float32 r,
                                  const NkColorF& c = NkColorF::Cyan(), uint32 seg = 16);
                void DrawAxis   (const NkMat4f& transform, float32 size = 1.f);
                void DrawFrustum(const NkCamera3D& cam, const NkColorF& c = NkColorF::Yellow());
                void DrawGrid   (float32 size = 10.f, uint32 divisions = 10,
                                  const NkColorF& c = NkColorF::Gray(.3f));
                void DrawCross  (const NkVec3f& pos, float32 size = 0.1f,
                                  const NkColorF& c = NkColorF::White());
                void DrawArrow  (const NkVec3f& origin, const NkVec3f& dir,
                                  float32 length = 1.f, const NkColorF& c = NkColorF::White());
                void DrawBone   (const NkVec3f& a, const NkVec3f& b,
                                  const NkColorF& c = NkColorF::Yellow());

                // ── Stats HUD ─────────────────────────────────────────────────────────────
                void DrawStatsPanel(const NkRendererStats& stats,
                                     float32 x = 10.f, float32 y = 10.f,
                                     const NkColorF& bg = NkColorF(.1f,.1f,.1f,.8f));

                // ── Texte screen-space (sans font externe, embedded) ──────────────────────
                void DrawLabel     (const NkVec3f& worldPos, const char* text,
                                     const NkColorF& c = NkColorF::White());
                void DrawTextHUD   (const NkVec2f& screenPos, const char* text,
                                     const NkColorF& c = NkColorF::White(),
                                     float32 size = 12.f);

                // ── Profiling GPU visuel ──────────────────────────────────────────────────
                void DrawTimingBar(const char* label, float32 ms, float32 x, float32 y,
                                    float32 w = 200.f, float32 h = 16.f);

                // ── Activation ────────────────────────────────────────────────────────────
                void SetEnabled(bool v)        { mEnabled = v; }
                bool IsEnabled()         const { return mEnabled; }
                void SetDepthTest(bool v)      { mDepthTest = v; }

           private:
                struct DebugLine3D {
                    NkVec3f a, b; NkColorF c; float32 thick;
                };

                NkIDevice*         mDevice    = nullptr;
                NkResourceManager* mResources = nullptr;
                NkICommandBuffer*  mCmd       = nullptr;
                const NkCamera3D*  mCam3D     = nullptr;
                bool               mReady     = false;
                bool               mEnabled   = true;
                bool               mDepthTest = false;

                uint32             mWidth = 0, mHeight = 0;

                NkVector<DebugLine3D> mLines3D;

                // GPU resources
                uint64  mVBORHI      = 0;
                uint64  mShaderRHI   = 0;
                uint64  mPipeRHI     = 0;
                uint64  mPipeNoDepth = 0;
                uint64  mUBORHI      = 0;
        };

    } // namespace renderer
} // namespace nkentseu
