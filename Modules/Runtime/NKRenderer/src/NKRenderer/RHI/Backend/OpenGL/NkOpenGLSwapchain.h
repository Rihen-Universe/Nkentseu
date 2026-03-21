#pragma once
// =============================================================================
// NkOpenGLSwapchain.h
// Swapchain OpenGL — FBO 0 = back buffer natif.
// AcquireNextImage : no-op (toujours disponible).
// Present         : swap natif (SwapBuffers / glXSwapBuffers / eglSwapBuffers).
// =============================================================================
#include "NKRenderer/RHI/Core/NkISwapchain.h"

namespace nkentseu {

class NkOpenGLDevice;

class NkOpenGLSwapchain final : public NkISwapchain {
public:
    explicit NkOpenGLSwapchain(NkOpenGLDevice* device);
    ~NkOpenGLSwapchain() override { Shutdown(); }

    bool Initialize(NkIGraphicsContext* ctx, const NkSwapchainDesc& desc) override;
    void Shutdown() override;
    bool IsValid() const override { return mIsValid; }

    // Acquire — GL : l'image est toujours prête (pas de file d'images GPU)
    bool AcquireNextImage(NkSemaphoreHandle signalSemaphore,
                          NkFenceHandle fence = NkFenceHandle::Null(),
                          uint64 timeoutNs = UINT64_MAX) override;

    // Present — swap natif de la surface OpenGL.
    bool Present(const NkSemaphoreHandle* waitSemaphores, uint32 waitCount) override;

    // Resize — recrée le render pass virtuel et met à jour les dimensions
    void Resize(uint32 width, uint32 height) override;

    NkFramebufferHandle GetCurrentFramebuffer() const override { return mFramebuffer; }
    NkRenderPassHandle  GetCurrentRenderPass()  const override { return mRenderPass; }
    NkGpuFormat GetColorFormat()       const override { return mColorFormat; }
    NkGpuFormat GetDepthFormat()       const override { return NkGpuFormat::NK_D32_FLOAT; }
    uint32   GetWidth()             const override { return mWidth; }
    uint32   GetHeight()            const override { return mHeight; }
    uint32   GetCurrentImageIndex() const override { return 0; }
    uint32   GetImageCount()        const override { return 1; }

private:
    void CreateObjects();
    void DestroyObjects();

    NkOpenGLDevice*     mDevice    = nullptr;
    NkIGraphicsContext* mCtx       = nullptr;
    NkFramebufferHandle mFramebuffer;
    NkRenderPassHandle  mRenderPass;
    NkGpuFormat            mColorFormat = NkGpuFormat::NK_RGBA8_SRGB;
    uint32              mWidth  = 0;
    uint32              mHeight = 0;
    bool                mIsValid = false;
};

} // namespace nkentseu

