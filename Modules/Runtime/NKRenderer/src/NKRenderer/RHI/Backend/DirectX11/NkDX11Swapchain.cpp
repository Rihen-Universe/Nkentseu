// =============================================================================
// NkDX11Swapchain.cpp
// =============================================================================
#ifdef NK_RHI_DX11_ENABLED
#include "NKRenderer/RHI/Backend/DirectX11/NkDX11Swapchain.h"
#include "NKRenderer/RHI/Backend/DirectX11/NkDX11Device.h"
#include "NKRenderer/Context/Graphics/DirectX/NkDirectXContextData.h"
#include "NKLogger/NkLog.h"

#define NK_DX11SC_LOG(...) logger_src.Infof("[NkDX11Swapchain] " __VA_ARGS__)
#define NK_DX11SC_ERR(...) logger_src.Infof("[NkDX11Swapchain][ERR] " __VA_ARGS__)

namespace nkentseu {

NkDX11Swapchain::NkDX11Swapchain(NkDeviceDX11* device)
    : mDevice(device) {}

// =============================================================================
bool NkDX11Swapchain::Initialize(NkIGraphicsContext* ctx,
                                  const NkSwapchainDesc& desc)
{
    if (!ctx || !ctx->IsValid()) return false;
    mCtx   = ctx;
    mVsync = desc.vsync;

    auto* dx11 = static_cast<NkDX11ContextData*>(ctx->GetNativeContextData());
    if (!dx11 || !dx11->swapchain) {
        NK_DX11SC_ERR("Donnees natives DX11 indisponibles\n");
        return false;
    }

    // Acces non-owning au swapchain du contexte
    mSwapchain = dx11->swapchain.Get();
    if (!mSwapchain) {
        NK_DX11SC_ERR("IDXGISwapChain1 non disponible dans le contexte\n");
        return false;
    }

    mWidth     = desc.width  > 0 ? desc.width  : dx11->width;
    mHeight    = desc.height > 0 ? desc.height : dx11->height;
    if (mWidth == 0 || mHeight == 0) {
        auto info = ctx->GetInfo();
        if (mWidth == 0) mWidth = info.windowWidth;
        if (mHeight == 0) mHeight = info.windowHeight;
    }
    if (mWidth == 0) mWidth = 1;
    if (mHeight == 0) mHeight = 1;
    mImageCount= desc.imageCount > 0 ? desc.imageCount : 3;

    // Vérifier tearing (présentation sans vsync à déchirure tolérée)
    {
        IDXGIFactory5* factory5 = nullptr;
        if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory5), (void**)&factory5))) {
            BOOL tearing = FALSE;
            factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                          &tearing, sizeof(tearing));
            mTearingSupported = (tearing == TRUE);
            factory5->Release();
        }
    }

    CreateFramebufferObjects();
    mIsValid = true;
    NK_DX11SC_LOG("Initialisé (%u×%u, vsync=%d)\n", mWidth, mHeight, mVsync);
    return true;
}

// =============================================================================
void NkDX11Swapchain::Shutdown()
{
    if (!mIsValid) return;
    DestroyFramebufferObjects();
    mSwapchain = nullptr; // non-owning
    mIsValid   = false;
    mCtx       = nullptr;
}

// =============================================================================
bool NkDX11Swapchain::AcquireNextImage(NkSemaphoreHandle /*signalSemaphore*/,
                                        NkFenceHandle     /*fence*/,
                                        uint64            /*timeoutNs*/)
{
    // D3D11 : le back buffer est toujours disponible — pas de queue d'images GPU.
    return mIsValid;
}

// =============================================================================
bool NkDX11Swapchain::Present(const NkSemaphoreHandle* /*waitSemaphores*/,
                               uint32                   /*waitCount*/)
{
    if (!mIsValid || !mSwapchain) return false;

    UINT syncInterval = mVsync ? 1 : 0;
    UINT flags = (!mVsync && mTearingSupported)
                 ? DXGI_PRESENT_ALLOW_TEARING : 0;

    HRESULT hr = mSwapchain->Present(syncInterval, flags);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        NK_DX11SC_ERR("Device lost (hr=0x%X)\n", (unsigned)hr);
        return false;
    }
    return SUCCEEDED(hr);
}

// =============================================================================
void NkDX11Swapchain::Resize(uint32 width, uint32 height)
{
    if (width == 0 || height == 0) return;
    if (width == mWidth && height == mHeight) return;

    DestroyFramebufferObjects();

    // Le ResizeBuffers du contexte est géré côté NkIGraphicsContext::OnResize().
    // On met simplement à jour nos dimensions et on recrée les wrappers RHI.
    mWidth  = width;
    mHeight = height;

    CreateFramebufferObjects();
}

// =============================================================================
void NkDX11Swapchain::CreateFramebufferObjects()
{
    // Render pass pour le back buffer DX11
    NkRenderPassDesc rpd;
    rpd.AddColor(NkAttachmentDesc::Color(NkGpuFormat::NK_BGRA8_SRGB))
       .SetDepth(NkAttachmentDesc::Depth(NkGpuFormat::NK_D32_FLOAT));
    mRenderPass = mDevice->CreateRenderPass(rpd);

    // Le framebuffer DX11 est géré en interne par NkDeviceDX11 via GetSwapchainFramebuffer().
    // On expose simplement le handle existant.
    mFramebuffer = mDevice->GetSwapchainFramebuffer();
}

// =============================================================================
void NkDX11Swapchain::DestroyFramebufferObjects()
{
    // Les ressources appartiennent au device — pas à détruire ici
    mFramebuffer = NkFramebufferHandle::Null();
    if (mRenderPass.IsValid()) {
        mDevice->DestroyRenderPass(mRenderPass);
    }
}

} // namespace nkentseu
#endif // NK_RHI_DX11_ENABLED


