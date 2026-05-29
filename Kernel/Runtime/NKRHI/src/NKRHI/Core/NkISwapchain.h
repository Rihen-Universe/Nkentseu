#pragma once
// =============================================================================
// NkISwapchain.h
// Interface multi-fenêtre optionnelle pour les backends RHI.
//
// Usage normal (mono-fenêtre) : ne pas utiliser — le device gère son swapchain
// interne via BeginFrame() / SubmitAndPresent() / EndFrame() / OnResize().
//
// Usage multi-fenêtre (éditeur, secondary viewports) :
//   swapchain->Initialize(ctx, desc)
//   while (running) {
//       swapchain->AcquireNextImage(...)
//       ... render ...
//       swapchain->Present(...)
//   }
//   swapchain->Shutdown()
// =============================================================================
#include "NkDescs.h"
#include "NkTypes.h"

namespace nkentseu {
    // Forward declarations — évite les dépendances circulaires avec NKCanvas
    class NkIGraphicsContext;

    struct NkSwapchainDesc {
        uint32      width      = 0;
        uint32      height     = 0;
        bool        vsync      = true;
        uint32      imageCount = 2;
        NkGPUFormat colorFormat= NkGPUFormat::NK_RGBA8_SRGB;
        NkGPUFormat depthFormat= NkGPUFormat::NK_D32_FLOAT;
        bool        hdr        = false;
        const char* debugName  = nullptr;
    };

    class NkISwapchain {
    public:
        virtual ~NkISwapchain() = default;

        virtual bool Initialize(NkIGraphicsContext* ctx, const NkSwapchainDesc& desc) = 0;
        virtual void Shutdown() = 0;
        virtual bool IsValid()  const = 0;

        virtual bool AcquireNextImage(NkSemaphoreHandle signalSemaphore,
                                       NkFenceHandle     fence     = NkFenceHandle::Null(),
                                       uint64            timeoutNs = UINT64_MAX)          = 0;

        virtual bool Present(const NkSemaphoreHandle* waitSemaphores,
                              uint32                   waitCount)                          = 0;
        bool Present() { return Present(nullptr, 0); }

        virtual void Resize(uint32 width, uint32 height) = 0;

        virtual NkFramebufferHandle GetCurrentFramebuffer() const = 0;
        virtual NkRenderPassHandle  GetCurrentRenderPass()  const = 0;
        virtual NkGPUFormat         GetColorFormat()        const = 0;
        virtual NkGPUFormat         GetDepthFormat()        const = 0;
        virtual uint32              GetWidth()               const = 0;
        virtual uint32              GetHeight()              const = 0;
        virtual uint32              GetCurrentImageIndex()   const = 0;
        virtual uint32              GetImageCount()          const = 0;

        virtual bool SupportsHDR()     const { return false; }
        virtual bool SupportsTearing() const { return false; }
    };

} // namespace nkentseu
