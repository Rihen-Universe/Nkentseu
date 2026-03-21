// =============================================================================
// NkDX12Swapchain.cpp
// =============================================================================
#ifdef NK_RHI_DX12_ENABLED
#include "NKRenderer/RHI/Backend/DirectX12/NkDX12Swapchain.h"
#include "NKRenderer/RHI/Backend/DirectX12/NkDX12Device.h"
#include "NKRenderer/Context/Graphics/DirectX/NkDirectXContextData.h"
#include "NKLogger/NkLog.h"

#define NK_DX12SC_LOG(...) logger_src.Infof("[NkDX12Swapchain] " __VA_ARGS__)
#define NK_DX12SC_ERR(...) logger_src.Infof("[NkDX12Swapchain][ERR] " __VA_ARGS__)

namespace nkentseu {

NkDX12Swapchain::NkDX12Swapchain(NkDeviceDX12* device)
    : mDevice(device) {}

// =============================================================================
bool NkDX12Swapchain::Initialize(NkIGraphicsContext* ctx,
                                  const NkSwapchainDesc& desc)
{
    if (!ctx || !ctx->IsValid()) return false;
    mCtx   = ctx;
    mVsync = desc.vsync;

    auto* dx12 = static_cast<NkDX12ContextData*>(ctx->GetNativeContextData());
    if (!dx12 || !dx12->device || !dx12->swapchain) {
        NK_DX12SC_ERR("Donnees natives DX12 indisponibles\n");
        return false;
    }

    // Acces non-owning au swapchain du contexte
    mSwapchain        = dx12->swapchain.Get();
    mBackBufferCount  = dx12->backBufferCount;
    mTearingSupported = dx12->tearingSupported;

    mWidth    = desc.width  > 0 ? desc.width  : dx12->width;
    mHeight   = desc.height > 0 ? desc.height : dx12->height;
    if (mWidth == 0 || mHeight == 0) {
        auto info = ctx->GetInfo();
        if (mWidth == 0) mWidth = info.windowWidth;
        if (mHeight == 0) mHeight = info.windowHeight;
    }
    if (mBackBufferCount == 0) mBackBufferCount = 3;
    if (mWidth == 0) mWidth = 1;
    if (mHeight == 0) mHeight = 1;

    ID3D12Device* d3dDevice = dx12->device.Get();

    // Créer un fence par back buffer pour la synchronisation inter-frame
    for (uint32 i = 0; i < mBackBufferCount; ++i) {
        mFenceValues[i] = 0;
        HRESULT hr = d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                             IID_PPV_ARGS(&mFrameFences[i]));
        if (FAILED(hr)) {
            NK_DX12SC_ERR("CreateFence[%u] échoué (hr=0x%X)\n", i, (unsigned)hr);
            return false;
        }
    }

    // Event de signalement (réutilisé entre frames)
    mFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!mFenceEvent) {
        NK_DX12SC_ERR("CreateEvent échoué\n");
        return false;
    }

    CreateRTVsAndFramebuffers();
    mIsValid = true;
    NK_DX12SC_LOG("Initialisé (%u×%u, %u back buffers)\n",
                  mWidth, mHeight, mBackBufferCount);
    return true;
}

// =============================================================================
void NkDX12Swapchain::Shutdown()
{
    if (!mIsValid) return;

    // Attendre que le GPU finisse tous les frames en vol
    for (uint32 i = 0; i < mBackBufferCount; ++i) {
        WaitForFrameFence(i);
    }

    DestroyRTVsAndFramebuffers();

    if (mFenceEvent) {
        CloseHandle(mFenceEvent);
        mFenceEvent = nullptr;
    }
    for (uint32 i = 0; i < mBackBufferCount; ++i) {
        mFrameFences[i] = nullptr;
    }

    mSwapchain = nullptr; // non-owning
    mIsValid   = false;
    mCtx       = nullptr;
}

// =============================================================================
bool NkDX12Swapchain::AcquireNextImage(NkSemaphoreHandle /*signalSemaphore*/,
                                        NkFenceHandle     /*fence*/,
                                        uint64            /*timeoutNs*/)
{
    if (!mIsValid || !mSwapchain) return false;

    // Identifier le prochain back buffer
    mCurrentBackBuffer = mSwapchain->GetCurrentBackBufferIndex();

    // Attendre que le frame précédent utilisant ce back buffer soit terminé
    WaitForFrameFence(mCurrentBackBuffer);
    return true;
}

// =============================================================================
bool NkDX12Swapchain::Present(const NkSemaphoreHandle* /*waitSemaphores*/,
                               uint32                   /*waitCount*/)
{
    if (!mIsValid || !mSwapchain) return false;

    UINT syncInterval = mVsync ? 1 : 0;
    UINT flags = (!mVsync && mTearingSupported)
                 ? DXGI_PRESENT_ALLOW_TEARING : 0;

    HRESULT hr = mSwapchain->Present(syncInterval, flags);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        NK_DX12SC_ERR("Device lost (hr=0x%X)\n", (unsigned)hr);
        return false;
    }

    // Signaler la fence du frame courant depuis la queue graphics
    auto* cmdQueue = mDevice ? mDevice->Queue() : nullptr;
    if (cmdQueue) {
        uint64 signalValue = ++mNextFenceValue;
        mFenceValues[mCurrentBackBuffer] = signalValue;
        cmdQueue->Signal(mFrameFences[mCurrentBackBuffer].Get(), signalValue);
    }

    return SUCCEEDED(hr);
}

// =============================================================================
void NkDX12Swapchain::Resize(uint32 width, uint32 height)
{
    if (width == 0 || height == 0) return;

    // Attendre que le GPU finisse tous les frames en vol
    for (uint32 i = 0; i < mBackBufferCount; ++i) {
        WaitForFrameFence(i);
    }

    DestroyRTVsAndFramebuffers();

    // ResizeBuffers est gere par NkIGraphicsContext::OnResize()
    auto* dx12 = mCtx ? static_cast<NkDX12ContextData*>(mCtx->GetNativeContextData()) : nullptr;
    if (dx12) {
        mWidth  = dx12->width  > 0 ? dx12->width  : width;
        mHeight = dx12->height > 0 ? dx12->height : height;
    } else {
        mWidth  = width;
        mHeight = height;
    }

    CreateRTVsAndFramebuffers();
}

// =============================================================================
NkFramebufferHandle NkDX12Swapchain::GetCurrentFramebuffer() const
{
    if (mCurrentBackBuffer < (uint32)mFramebuffers.Size())
        return mFramebuffers[mCurrentBackBuffer];
    return NkFramebufferHandle::Null();
}

// =============================================================================
void NkDX12Swapchain::WaitForFrameFence(uint32 frameIdx)
{
    if (!mFrameFences[frameIdx] || !mFenceEvent) return;
    uint64 completedValue = mFrameFences[frameIdx]->GetCompletedValue();
    if (completedValue < mFenceValues[frameIdx]) {
        mFrameFences[frameIdx]->SetEventOnCompletion(
            mFenceValues[frameIdx], mFenceEvent);
        WaitForSingleObject(mFenceEvent, INFINITE);
    }
}

// =============================================================================
void NkDX12Swapchain::CreateRTVsAndFramebuffers()
{
    // Render pass pour le swapchain DX12
    NkRenderPassDesc rpd;
    rpd.AddColor(NkAttachmentDesc::Color(NkGpuFormat::NK_BGRA8_SRGB))
       .SetDepth(NkAttachmentDesc::Depth(NkGpuFormat::NK_D32_FLOAT));
    mRenderPass = mDevice->CreateRenderPass(rpd);

    // Framebuffers : un par back buffer, emprunté au device
    mFramebuffers.Resize(mBackBufferCount);
    for (uint32 i = 0; i < mBackBufferCount; ++i) {
        if (i < mDevice->GetSwapchainFramebufferCount()) {
            mFramebuffers[i] = mDevice->GetSwapchainFramebufferAt(i);
        } else {
            // Créer un framebuffer vide (le device le peuplera via OnResize)
            NkFramebufferDesc fbd;
            fbd.renderPass = mRenderPass;
            fbd.width      = mWidth;
            fbd.height     = mHeight;
            mFramebuffers[i] = mDevice->CreateFramebuffer(fbd);
        }
    }
}

// =============================================================================
void NkDX12Swapchain::DestroyRTVsAndFramebuffers()
{
    mFramebuffers.Clear();
    if (mRenderPass.IsValid()) {
        mDevice->DestroyRenderPass(mRenderPass);
    }
}

} // namespace nkentseu
#endif // NK_RHI_DX12_ENABLED





