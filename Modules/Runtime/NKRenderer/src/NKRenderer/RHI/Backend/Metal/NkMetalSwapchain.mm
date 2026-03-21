// =============================================================================
// NkMetalSwapchain.mm
// =============================================================================
#ifdef NK_RHI_METAL_ENABLED
#import "NKRenderer/RHI/Backend/Metal/NkMetalSwapchain.h"
#import "NKRenderer/RHI/Backend/Metal/NkMetalDevice.h"
#include "NKRenderer/Context/Graphics/Metal/NkMetalContext.h"
#import "NKLogger/NkLog.h"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#define NK_MTLSC_LOG(...) logger_src.Infof("[NkMetalSwapchain] " __VA_ARGS__)
#define NK_MTLSC_ERR(...) logger_src.Infof("[NkMetalSwapchain][ERR] " __VA_ARGS__)

namespace nkentseu {

NkMetalSwapchain::NkMetalSwapchain(NkDevice_Metal* device)
    : mDevice(device) {}

// =============================================================================
bool NkMetalSwapchain::Initialize(NkIGraphicsContext* ctx,
                                   const NkSwapchainDesc& desc)
{
    if (!ctx || !ctx->IsValid()) return false;
    mCtx = ctx;

    auto* mtl = static_cast<NkMetalContextData*>(ctx->GetNativeContextData());
    if (!mtl || !mtl->layer) {
        NK_MTLSC_ERR("Donnees natives Metal indisponibles\n");
        return false;
    }

    CAMetalLayer* layer = (__bridge CAMetalLayer*)mtl->layer;
    if (!layer) {
        NK_MTLSC_ERR("CAMetalLayer non disponible dans le contexte\n");
        return false;
    }

    mWidth    = desc.width  > 0 ? desc.width  : mtl->width;
    mHeight   = desc.height > 0 ? desc.height : mtl->height;
    if (mWidth == 0 || mHeight == 0) {
        auto info = ctx->GetInfo();
        if (mWidth == 0) mWidth = info.windowWidth;
        if (mHeight == 0) mHeight = info.windowHeight;
    }
    if (mWidth == 0) mWidth = 1;
    if (mHeight == 0) mHeight = 1;

    // Configurer la pool de drawables
    layer.drawableSize          = CGSizeMake(mWidth, mHeight);
    layer.maximumDrawableCount  = 3;   // triple buffering
    layer.pixelFormat           = MTLPixelFormatBGRA8Unorm_sRGB;
    layer.framebufferOnly       = YES; // optimal pour présentation uniquement
    layer.displaySyncEnabled    = YES; // vsync par défaut

    CreateObjects();
    mIsValid = true;
    NK_MTLSC_LOG("Initialisé (%u×%u)\n", mWidth, mHeight);
    return true;
}

// =============================================================================
void NkMetalSwapchain::Shutdown()
{
    if (!mIsValid) return;
    DestroyObjects();
    mIsValid = false;
    mCtx     = nullptr;
}

// =============================================================================
bool NkMetalSwapchain::AcquireNextImage(NkSemaphoreHandle /*signalSemaphore*/,
                                         NkFenceHandle     /*fence*/,
                                         uint64            /*timeoutNs*/)
{
    return mIsValid;
}

// =============================================================================
bool NkMetalSwapchain::Present(const NkSemaphoreHandle* /*waitSemaphores*/,
                                uint32                   /*waitCount*/)
{
    // Dans la nouvelle architecture, le present est fait par NkDevice_Metal::SubmitAndPresent().
    // Ce swapchain ne force pas de present direct.
    return mIsValid;
}

// =============================================================================
void NkMetalSwapchain::Resize(uint32 width, uint32 height)
{
    if (width == 0 || height == 0) return;
    if (width == mWidth && height == mHeight) return;

    mWidth  = width;
    mHeight = height;

    // Mettre à jour la taille du drawable de la layer
    auto* mtl = mCtx ? static_cast<NkMetalContextData*>(mCtx->GetNativeContextData()) : nullptr;
    if (mtl && mtl->layer) {
        CAMetalLayer* layer = (__bridge CAMetalLayer*)mtl->layer;
        if (layer) layer.drawableSize = CGSizeMake(mWidth, mHeight);
    }

    DestroyObjects();
    CreateObjects();
}

// =============================================================================
void NkMetalSwapchain::CreateObjects()
{
    // Render pass pour le swapchain Metal
    NkRenderPassDesc rpd;
    rpd.AddColor(NkAttachmentDesc::Color(NkGpuFormat::NK_BGRA8_SRGB))
       .SetDepth(NkAttachmentDesc::Depth(NkGpuFormat::NK_D32_FLOAT));
    mRenderPass = mDevice->CreateRenderPass(rpd);

    // Framebuffer — emprunté au swapchain interne du device
    mFramebuffer = mDevice->GetSwapchainFramebuffer();
}

// =============================================================================
void NkMetalSwapchain::DestroyObjects()
{
    mFramebuffer = NkFramebufferHandle::Null();
    if (mRenderPass.IsValid()) {
        mDevice->DestroyRenderPass(mRenderPass);
    }
}

} // namespace nkentseu
#endif // NK_RHI_METAL_ENABLED



