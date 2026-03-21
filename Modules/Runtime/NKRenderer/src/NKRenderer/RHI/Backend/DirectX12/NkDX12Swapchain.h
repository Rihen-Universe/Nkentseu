#pragma once
// =============================================================================
// NkDX12Swapchain.h
// Swapchain DirectX 12 — wrapping de l'IDXGISwapChain4 du contexte.
//
// D3D12 utilise un modèle à N back buffers (triple buffering par défaut).
// La synchronisation CPU-GPU entre frames utilise ID3D12Fence.
//
// Cycle par frame :
//   1. AcquireNextImage → GetCurrentBackBufferIndex()
//   2. Rendre dans le back buffer courant (transition PRESENT→RENDER_TARGET)
//   3. Present → IDXGISwapChain4::Present1()
//   4. Signal fence, avancer au frame suivant
// =============================================================================
#include "NKRenderer/RHI/Core/NkISwapchain.h"

#ifdef NK_RHI_DX12_ENABLED
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include "NKContainers/Sequential/NkVector.h"
using Microsoft::WRL::ComPtr;

namespace nkentseu {

class NkDeviceDX12;

class NkDX12Swapchain final : public NkISwapchain {
public:
    explicit NkDX12Swapchain(NkDeviceDX12* device);
    ~NkDX12Swapchain() override { Shutdown(); }

    bool Initialize(NkIGraphicsContext* ctx, const NkSwapchainDesc& desc) override;
    void Shutdown() override;
    bool IsValid() const override { return mIsValid; }

    // AcquireNextImage → GetCurrentBackBufferIndex() + attente fence frame précédent
    bool AcquireNextImage(NkSemaphoreHandle signalSemaphore,
                          NkFenceHandle fence = NkFenceHandle::Null(),
                          uint64 timeoutNs = UINT64_MAX) override;

    // Present → IDXGISwapChain4::Present1() + signal fence
    bool Present(const NkSemaphoreHandle* waitSemaphores, uint32 waitCount) override;

    // Resize → ResizeBuffers() + recréation des vues
    void Resize(uint32 width, uint32 height) override;

    NkFramebufferHandle GetCurrentFramebuffer() const override;
    NkRenderPassHandle  GetCurrentRenderPass()  const override { return mRenderPass; }
    NkGpuFormat GetColorFormat()       const override { return NkGpuFormat::NK_BGRA8_SRGB; }
    NkGpuFormat GetDepthFormat()       const override { return NkGpuFormat::NK_D32_FLOAT; }
    uint32   GetWidth()             const override { return mWidth; }
    uint32   GetHeight()            const override { return mHeight; }
    uint32   GetCurrentImageIndex() const override { return mCurrentBackBuffer; }
    uint32   GetImageCount()        const override { return mBackBufferCount; }

    bool SupportsTearing() const override { return mTearingSupported; }

private:
    void CreateRTVsAndFramebuffers();
    void DestroyRTVsAndFramebuffers();
    void WaitForFrameFence(uint32 frameIdx);

    NkDeviceDX12*      mDevice           = nullptr;
    NkIGraphicsContext* mCtx              = nullptr;

    // Swapchain natif — non-owning (appartient au contexte DX12)
    IDXGISwapChain4*    mSwapchain        = nullptr;

    // Fence de frame (un par back buffer pour éviter d'écraser un frame en cours)
    ComPtr<ID3D12Fence> mFrameFences[3];
    HANDLE              mFenceEvent       = nullptr;
    uint64              mFenceValues[3]   = {};
    uint64              mNextFenceValue   = 1;

    NkRenderPassHandle              mRenderPass;
    NkVector<NkFramebufferHandle>mFramebuffers;

    uint32 mCurrentBackBuffer = 0;
    uint32 mBackBufferCount   = 3;
    uint32 mWidth             = 0;
    uint32 mHeight            = 0;
    bool   mVsync             = true;
    bool   mTearingSupported  = false;
    bool   mIsValid           = false;
};

} // namespace nkentseu
#endif // NK_RHI_DX12_ENABLED

