#pragma once
// =============================================================================
// NkSoftwareSwapchain.h — v4 final (inchangé, fourni tel quel)
// =============================================================================
#include "NKRenderer/RHI/Core/NkISwapchain.h"

namespace nkentseu {

class NkSoftwareDevice;

class NkSoftwareSwapchain final : public NkISwapchain {
public:
    explicit NkSoftwareSwapchain(NkSoftwareDevice* device);
    ~NkSoftwareSwapchain() override;

    bool Initialize(NkIGraphicsContext* ctx, const NkSwapchainDesc& desc) override;
    void Shutdown() override;
    bool IsValid() const override { return mIsValid; }

    bool AcquireNextImage(NkSemaphoreHandle signalSemaphore,
                          NkFenceHandle     fence     = NkFenceHandle::Null(),
                          uint64            timeoutNs = UINT64_MAX) override;
    bool Present(const NkSemaphoreHandle* waitSemaphores, uint32 waitCount) override;
    void Resize(uint32 width, uint32 height) override;

    NkFramebufferHandle GetCurrentFramebuffer() const override { return mFramebuffer; }
    NkRenderPassHandle  GetCurrentRenderPass()  const override { return mRenderPass;  }
    NkGpuFormat GetColorFormat()       const override { return NkGpuFormat::NK_RGBA8_UNORM; }
    NkGpuFormat GetDepthFormat()       const override { return NkGpuFormat::NK_D32_FLOAT;   }
    uint32 GetWidth()             const override { return mWidth;  }
    uint32 GetHeight()            const override { return mHeight; }
    uint32 GetCurrentImageIndex() const override { return 0; }
    uint32 GetImageCount()        const override { return 1; }

private:
    void CreateObjects();
    void DestroyObjects();

    NkSoftwareDevice*   mDevice    = nullptr;
    NkFramebufferHandle mFramebuffer;
    NkRenderPassHandle  mRenderPass;
    NkTextureHandle     mColorTex;
    NkTextureHandle     mDepthTex;
    uint32              mWidth   = 0;
    uint32              mHeight  = 0;
    bool                mIsValid = false;
};

} // namespace nkentseu
