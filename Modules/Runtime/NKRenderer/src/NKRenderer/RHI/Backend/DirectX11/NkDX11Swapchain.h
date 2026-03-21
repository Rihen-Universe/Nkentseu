#pragma once
// =============================================================================
// NkDX11Swapchain.h
// Swapchain DirectX 11 — wrapping de l'IDXGISwapChain1 du contexte.
//
// D3D11 n'a pas de concept "acquire next image" côté application :
//   le back buffer est toujours disponible après Present().
// AcquireNextImage → no-op.
// Present          → IDXGISwapChain1::Present().
// =============================================================================
#include "NKRenderer/RHI/Core/NkISwapchain.h"

#ifdef NK_RHI_DX11_ENABLED
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11_1.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

namespace nkentseu {

class NkDeviceDX11;

class NkDX11Swapchain final : public NkISwapchain {
public:
    explicit NkDX11Swapchain(NkDeviceDX11* device);
    ~NkDX11Swapchain() override { Shutdown(); }

    bool Initialize(NkIGraphicsContext* ctx, const NkSwapchainDesc& desc) override;
    void Shutdown() override;
    bool IsValid() const override { return mIsValid; }

    // D3D11 : always ready — no image queue
    bool AcquireNextImage(NkSemaphoreHandle signalSemaphore,
                          NkFenceHandle fence = NkFenceHandle::Null(),
                          uint64 timeoutNs = UINT64_MAX) override;

    // Present via IDXGISwapChain1::Present()
    bool Present(const NkSemaphoreHandle* waitSemaphores, uint32 waitCount) override;

    // Resize via IDXGISwapChain::ResizeBuffers()
    void Resize(uint32 width, uint32 height) override;

    NkFramebufferHandle GetCurrentFramebuffer() const override { return mFramebuffer; }
    NkRenderPassHandle  GetCurrentRenderPass()  const override { return mRenderPass; }
    NkGpuFormat GetColorFormat()       const override { return NkGpuFormat::NK_BGRA8_SRGB; }
    NkGpuFormat GetDepthFormat()       const override { return NkGpuFormat::NK_D32_FLOAT; }
    uint32   GetWidth()             const override { return mWidth; }
    uint32   GetHeight()            const override { return mHeight; }
    uint32   GetCurrentImageIndex() const override { return 0; }
    uint32   GetImageCount()        const override { return mImageCount; }

    bool SupportsTearing() const override { return mTearingSupported; }

private:
    void CreateFramebufferObjects();
    void DestroyFramebufferObjects();

    NkDeviceDX11*      mDevice    = nullptr;
    NkIGraphicsContext* mCtx       = nullptr;

    // Swapchain natif — non-owning (appartient au contexte DX11)
    IDXGISwapChain1*    mSwapchain = nullptr;

    NkFramebufferHandle mFramebuffer;
    NkRenderPassHandle  mRenderPass;

    uint32 mWidth           = 0;
    uint32 mHeight          = 0;
    uint32 mImageCount      = 3;
    bool   mVsync           = true;
    bool   mTearingSupported= false;
    bool   mIsValid         = false;
};

} // namespace nkentseu
#endif // NK_RHI_DX11_ENABLED

