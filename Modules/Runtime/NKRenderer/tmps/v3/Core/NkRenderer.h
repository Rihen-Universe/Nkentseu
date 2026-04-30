#pragma once
// =============================================================================
// NkRenderer.h
// Facade principale du système de rendu NkRenderer.
//
// NkRenderer abstrait le NkIDevice RHI et expose une API haut-niveau orientée
// "rendu" plutôt que "GPU". Il orchestre :
//   - Les passes de rendu (geometry, shadow, post-process, UI overlay)
//   - La gestion des matériaux (PBR, NPR, toon, archiviz...)
//   - Les outils 2D / 3D / offscreen / overlay
//   - Le pipeline de post-traitement (bloom, AO, tonemap, etc.)
//
// Architecture :
//   NkIDevice (RHI)
//     └── NkRenderer        ← vous êtes ici
//           ├── NkRenderGraph     (ordonnancement des passes)
//           ├── NkMaterialSystem  (compilation et cache des matériaux)
//           ├── NkTextureLibrary  (chargement, atlas, streaming)
//           ├── NkMeshCache       (buffers GPU partagés)
//           ├── Render2D          (sprites, texte, shapes 2D)
//           ├── Render3D          (meshes, instancing, skinning)
//           ├── NkOffscreenTarget (FBO arbitraires)
//           └── NkOverlayRenderer (UI, debug, gizmos)
//
// Usage minimal :
//   NkRendererConfig cfg;
//   cfg.api = NkGraphicsApi::NK_GFX_API_VULKAN;
//   NkRenderer* renderer = NkRenderer::Create(device, cfg);
//   renderer->BeginFrame();
//   renderer->GetRender3D()->DrawMesh(mesh, transform, material);
//   renderer->EndFrame();
//   renderer->Present();
// =============================================================================
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

#include "NkRendererConfig.h"
#include "NkRenderGraph.h"
#include "NkTextureLibrary.h"
#include "NkMeshCache.h"

#include "NKRenderer/Materials/NkMaterialSystem.h"

namespace nkentseu {

    class NkRender2D;
    class NkRender3D;
    class NkOffscreenTarget;
    struct NkOffscreenDesc;
    class NkOverlayRenderer;
    class NkPostProcessStack;

    // =============================================================================
    // Stats de rendu par frame
    // =============================================================================
    struct NkRendererStats {
        uint32  drawCalls      = 0;
        uint32  triangles      = 0;
        uint32  materialSwaps  = 0;
        uint32  textureBinds   = 0;
        uint32  shadowCasters  = 0;
        float32 gpuTimeMs      = 0.f;
        float32 cpuBuildTimeMs = 0.f;
        uint64  gpuMemoryBytes = 0;
    };

    // =============================================================================
    // NkRenderer — point d'entrée unique du système de rendu
    // =============================================================================
    class NkRenderer {
        public:
            // ── Cycle de vie ──────────────────────────────────────────────────────────
            static NkRenderer* Create(NkIDevice* device, const NkRendererConfig& cfg);
            static void        Destroy(NkRenderer*& renderer);

            virtual ~NkRenderer() = default;

            virtual bool Initialize() = 0;
            virtual void Shutdown()   = 0;
            virtual bool IsValid() const = 0;

            // ── Frame ─────────────────────────────────────────────────────────────────
            // BeginFrame prépare la frame : acquiert swapchain, reset les pools.
            // EndFrame soumet toutes les passes enregistrées dans le RenderGraph.
            // Present déclenche la présentation (peut être séparé de EndFrame
            // pour permettre du travail CPU pendant la présentation).
            virtual bool    BeginFrame()  = 0;
            virtual void    EndFrame()    = 0;
            virtual void    Present()     = 0;

            // ── Resize ────────────────────────────────────────────────────────────────
            virtual void OnResize(uint32 width, uint32 height) = 0;

            // ── Accès aux sous-systèmes ───────────────────────────────────────────────
            virtual NkRender2D*        GetRender2D()      = 0;
            virtual NkRender3D*        GetRender3D()      = 0;
            virtual NkOverlayRenderer* GetOverlay()       = 0;
            virtual NkPostProcessStack* GetPostProcess()  = 0;
            virtual NkMaterialSystem*  GetMaterials()     = 0;
            virtual NkTextureLibrary*  GetTextures()      = 0;
            virtual NkMeshCache*       GetMeshCache()     = 0;
            virtual NkRenderGraph*     GetRenderGraph()   = 0;

            // ── Cibles offscreen ──────────────────────────────────────────────────────
            // Crée un NkOffscreenTarget indépendant (pour shadow maps, reflection
            // probes, RTT textures, etc.)
            virtual NkOffscreenTarget* CreateOffscreenTarget(const NkOffscreenDesc& desc)  = 0;
            virtual void               DestroyOffscreenTarget(NkOffscreenTarget*& target)  = 0;

            // ── Configuration dynamique ───────────────────────────────────────────────
            virtual void SetVSync(bool enabled)      = 0;
            virtual void SetMSAA(NkSampleCount msaa) = 0;
            virtual void SetHDR(bool enabled)        = 0;
            virtual void SetShadowQuality(uint32 quality) = 0; // 0=off 1=low 2=med 3=high 4=ultra

            // ── Statistiques ──────────────────────────────────────────────────────────
            virtual const NkRendererStats& GetStats() const = 0;
            virtual void                   ResetStats()     = 0;

            // ── Accès bas niveau (interop) ────────────────────────────────────────────
            virtual NkIDevice*         GetDevice()   const = 0;
            virtual NkICommandBuffer*  GetCommandBuffer() const = 0;
            virtual uint32             GetFrameIndex() const = 0;
            virtual uint32             GetWidth()      const = 0;
            virtual uint32             GetHeight()     const = 0;
    };

} // namespace nkentseu