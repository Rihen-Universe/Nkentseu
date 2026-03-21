#pragma once
// =============================================================================
// NkMetalSwapchain.h
// Swapchain Metal — wrapping de la CAMetalLayer du contexte.
//
// Metal ne gère pas de "swapchain" au sens Vulkan :
//   - La CAMetalLayer vend des drawables depuis une pool interne.
//   - AcquireNextImage → [layer nextDrawable]
//   - Present         → no-op (presentation geree par NkDevice_Metal::SubmitAndPresent)
//
// La profondeur de la pool est contrôlée par CAMetalLayer.maximumDrawableCount.
// =============================================================================
#include "NKRenderer/RHI/Core/NkISwapchain.h"

#ifdef NK_RHI_METAL_ENABLED
#ifdef __OBJC__
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#endif

namespace nkentseu {

class NkDevice_Metal;

class NkMetalSwapchain final : public NkISwapchain {
public:
    explicit NkMetalSwapchain(NkDevice_Metal* device);
    ~NkMetalSwapchain() override { Shutdown(); }

    bool Initialize(NkIGraphicsContext* ctx, const NkSwapchainDesc& desc) override;
    void Shutdown() override;
    bool IsValid() const override { return mIsValid; }

    // AcquireNextImage → [layer nextDrawable] (bloque si pool épuisée)
    bool AcquireNextImage(NkSemaphoreHandle signalSemaphore,
                          NkFenceHandle fence = NkFenceHandle::Null(),
                          uint64 timeoutNs = UINT64_MAX) override;

    // Present: no-op, le device fait le present lors du submit.
    bool Present(const NkSemaphoreHandle* waitSemaphores, uint32 waitCount) override;

    // Resize → mise à jour des dimensions de la layer
    void Resize(uint32 width, uint32 height) override;

    NkFramebufferHandle GetCurrentFramebuffer() const override { return mFramebuffer; }
    NkRenderPassHandle  GetCurrentRenderPass()  const override { return mRenderPass; }
    NkGpuFormat GetColorFormat()       const override { return NkGpuFormat::NK_BGRA8_SRGB; }
    NkGpuFormat GetDepthFormat()       const override { return NkGpuFormat::NK_D32_FLOAT; }
    uint32   GetWidth()             const override { return mWidth; }
    uint32   GetHeight()            const override { return mHeight; }
    uint32   GetCurrentImageIndex() const override { return 0; }
    uint32   GetImageCount()        const override { return 3; }  // maximumDrawableCount

private:
    void CreateObjects();
    void DestroyObjects();

    NkDevice_Metal*     mDevice    = nullptr;
    NkIGraphicsContext* mCtx       = nullptr;

    NkFramebufferHandle mFramebuffer;
    NkRenderPassHandle  mRenderPass;

    uint32 mWidth   = 0;
    uint32 mHeight  = 0;
    bool   mIsValid = false;
};

} // namespace nkentseu
#endif // NK_RHI_METAL_ENABLED

