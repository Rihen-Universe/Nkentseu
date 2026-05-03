#pragma once
// =============================================================================
// NkRenderer.h  — NKRenderer v4.0
// Façade publique principale. 
// NKRenderer ne connaît PAS : NkScene, ECS, application.
// Seul lien avec la plateforme : NkSurfaceDesc (depuis NKWindow).
// =============================================================================
#include "Core/NkRendererTypes.h"
#include "Core/NkRendererConfig.h"
#include "NKWindow/Core/NkSurface.h"    // NkSurfaceDesc — seul lien plateforme
#include "NKRHI/Core/NkIDevice.h"

// Forward declarations — évite de tout inclure
namespace nkentseu { namespace renderer {
    class NkRenderGraph;
    class NkTextureLibrary;
    class NkMeshSystem;
    class NkMaterialSystem;
    class NkRender2D;
    class NkRender3D;
    class NkTextRenderer;
    class NkPostProcessStack;
    class NkOverlayRenderer;
    class NkOffscreenTarget;
    class NkShadowSystem;
    class NkVFXSystem;
    class NkAnimationSystem;
    class NkSimulationRenderer;
    // Nouveaux sous-systèmes
    class NkCullingSystem;
    class NkDeferredPass;
    class NkStreamingSystem;
    class NkRaytracingSystem;
    class NkIKSystem;
    class NkDenoiserSystem;
    class NkAIRenderingSystem;
    struct NkOffscreenDesc;
}}

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // NkRenderer — interface pure
    // =========================================================================
    class NkRenderer {
    public:
        virtual ~NkRenderer() = default;

        // ── Fabrique ─────────────────────────────────────────────────────────
        static NkRenderer* Create(NkIDevice*             device,
                                   const NkSurfaceDesc&   surface,
                                   const NkRendererConfig& cfg);
        static void Destroy(NkRenderer*& renderer);

        // ── Cycle de vie ──────────────────────────────────────────────────────
        virtual bool Initialize() = 0;
        virtual void Shutdown()   = 0;
        virtual bool IsValid()    const = 0;

        // ── Frame ─────────────────────────────────────────────────────────────
        virtual bool BeginFrame() = 0;
        virtual void EndFrame()   = 0;
        virtual void Present()    = 0;

        // ── Resize (appeler depuis NkGraphicsContextResizeEvent) ──────────────
        virtual void OnResize(uint32 width, uint32 height) = 0;

        // ── Sous-systèmes ─────────────────────────────────────────────────────
        virtual NkRenderGraph*        GetRenderGraph()  = 0;
        virtual NkTextureLibrary*     GetTextures()     = 0;
        virtual NkMeshSystem*         GetMeshSystem()   = 0;
        virtual NkMaterialSystem*     GetMaterials()    = 0;
        virtual NkRender2D*           GetRender2D()     = 0;
        virtual NkRender3D*           GetRender3D()     = 0;
        virtual NkTextRenderer*       GetTextRenderer() = 0;
        virtual NkPostProcessStack*   GetPostProcess()  = 0;
        virtual NkOverlayRenderer*    GetOverlay()      = 0;
        virtual NkShadowSystem*       GetShadow()       = 0;
        virtual NkVFXSystem*          GetVFX()          = 0;
        virtual NkAnimationSystem*    GetAnimation()    = 0;
        virtual NkSimulationRenderer* GetSimulation()   = 0;

        // ── Nouveaux sous-systèmes (priorité haute→basse) ─────────────────────
        virtual NkCullingSystem*      GetCulling()      = 0;
        virtual NkDeferredPass*       GetDeferred()     = 0;
        virtual NkStreamingSystem*    GetStreaming()     = 0;
        virtual NkRaytracingSystem*   GetRaytracing()   = 0;
        virtual NkIKSystem*           GetIK()           = 0;
        virtual NkDenoiserSystem*     GetDenoiser()     = 0;
        virtual NkAIRenderingSystem*  GetAIRendering()  = 0;

        // ── Targets offscreen ─────────────────────────────────────────────────
        virtual NkOffscreenTarget* CreateOffscreen(const NkOffscreenDesc& desc) = 0;
        virtual void               DestroyOffscreen(NkOffscreenTarget*& t)      = 0;

        // ── Configuration dynamique ───────────────────────────────────────────
        virtual void SetVSync     (bool enabled)          = 0;
        virtual void SetPostConfig(const NkPostConfig& pp)= 0;
        virtual void SetWireframe (bool enabled)          = 0;

        // ── Stats ─────────────────────────────────────────────────────────────
        virtual const NkRendererStats& GetStats()  const = 0;
        virtual void                   ResetStats()      = 0;

        // ── Accès bas niveau ──────────────────────────────────────────────────
        virtual NkIDevice*              GetDevice()     const = 0;
        virtual NkICommandBuffer*       GetCmd()        const = 0;
        virtual uint32                  GetFrameIndex() const = 0;
        virtual uint32                  GetWidth()      const = 0;
        virtual uint32                  GetHeight()     const = 0;
        virtual const NkRendererConfig& GetConfig()     const = 0;
    };

} // namespace renderer
} // namespace nkentseu
