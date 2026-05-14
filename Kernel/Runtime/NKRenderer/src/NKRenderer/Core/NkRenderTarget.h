#pragma once
// =============================================================================
// NkRenderTarget.h — NKRenderer v4.0  (Core/)
//
// Render-to-texture : wrapper haut niveau sur NkOffscreenTarget.
//
// Usage typique — planar reflection :
//
//   // Init (une fois)
//   NkRenderTarget reflRT;
//   reflRT.Init(device, texLib, {.width=512, .height=512, .name="Reflection"});
//
//   // Par frame :
//   NkCamera3D mirrorCam = NkRenderTarget::ReflectCamera(mainCam, {0,1,0,0});
//   NkSceneContext rtCtx = ctx;
//   rtCtx.camera = mirrorCam;
//   r3d->BeginScene(rtCtx);
//   r3d->Submit(dc1); ...
//   reflRT.BeginRender(cmd);
//   reflRT.FlushScene(cmd, r3d);
//   reflRT.EndRender(cmd);
//
//   // Binder le résultat sur le sol :
//   floorMat->SetTexture("reflection", reflRT.GetColorHandle());
// =============================================================================
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKRenderer/Core/NkCamera.h"

namespace nkentseu {
    namespace renderer {

        class NkRender3D;

        struct NkRenderTargetDesc {
            uint32   width  = 512;
            uint32   height = 512;
            bool     hdr    = false;   // true = RGBA16F, false = RGBA8
            bool     depth  = true;
            NkString name   = "RT";
        };

        // =====================================================================
        class NkRenderTarget {
            public:
                bool Init(NkIDevice* dev, NkTextureLibrary* texLib,
                        const NkRenderTargetDesc& desc);
                void Shutdown();
                bool IsValid() const { return mOffscreen.IsValid(); }

                // Ouvre le render pass du target.
                // Doit être appelé AVANT r3d->Flush(cmd, rt.GetRenderPass()).
                // Retourne le RenderPass pour EnsurePBRPipeline (compat Vulkan).
                NkRenderPassHandle BeginRender(NkICommandBuffer* cmd,
                                            NkVec4f clearColor = {0.f, 0.f, 0.f, 1.f},
                                            bool    clearDepth  = true);
                void EndRender(NkICommandBuffer* cmd);

                // Flush NkRender3D dans ce target.
                // Appelle r3d->Flush(cmd, GetRenderPass()).
                // Prérequis : r3d->BeginScene() + Submit() avant, BeginRender() avant.
                void FlushScene(NkICommandBuffer* cmd, NkRender3D* r3d);

                bool Resize(uint32 w, uint32 h);

                NkTexHandle        GetColorHandle() const { return mOffscreen.GetColorResult(); }
                NkTexHandle        GetDepthHandle() const { return mOffscreen.GetDepthResult(); }
                NkRenderPassHandle GetRenderPass()  const { return mOffscreen.GetRP(); }
                uint32 GetWidth()  const { return mOffscreen.GetWidth(); }
                uint32 GetHeight() const { return mOffscreen.GetHeight(); }

                // ── Utilitaires réflexion planaire ────────────────────────────────
                // plane = {nx, ny, nz, d}  —  nx·x + ny·y + nz·z + d = 0  (normalisé).
                // Exemple : sol Y=0  →  {0, 1, 0, 0}.
                //           sol Y=1  →  {0, 1, 0,-1}.
                //
                // Attention : la caméra miroir inverse le winding des triangles.
                // Utiliser NkCullMode::NK_FRONT (ou NoCull) pour les passes dans ce RT.
                static NkCamera3D ReflectCamera(const NkCamera3D& cam, NkVec4f plane);

                // Projection oblique : aligne le near clip sur le plan de réflexion.
                // Évite les artefacts de fragments sous le plan.
                // proj = projection non-modifiée ; view = view de la caméra miroir.
                // worldPlane en espace monde (même convention que ReflectCamera).
                static NkMat4f ObliqueProjection(NkMat4f proj, const NkMat4f& view,
                                                NkVec4f worldPlane);

            private:
                NkOffscreenTarget mOffscreen;
        };

    } // namespace renderer
} // namespace nkentseu
