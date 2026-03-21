// =============================================================================
// NkOpenGLSwapchain.cpp
// Swapchain OpenGL — FBO 0 (back buffer par défaut du contexte GL).
// Aucune image Vulkan ni DXGI : le driver GL gère la présentation.
// =============================================================================
#include "NKRenderer/RHI/Backend/OpenGL/NkOpenGLSwapchain.h"
#include "NKRenderer/RHI/Backend/OpenGL/NkOpenGLDevice.h"
#include "NKRenderer/Context/Core/NkIGraphicsContext.h"
#include "NKRenderer/Context/Graphics/OpenGL/NkOpenGLContextData.h"

#ifdef min
#  undef min
#endif
#ifdef max
#  undef max
#endif

namespace nkentseu {

static void NkSwapPresentOpenGL(NkIGraphicsContext* ctx) {
    if (!ctx) return;
    auto* data = static_cast<NkOpenGLContextData*>(ctx->GetNativeContextData());
    if (!data) return;
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    if (data->hdc) SwapBuffers(data->hdc);
#elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
    if (data->display && data->window) glXSwapBuffers(data->display, data->window);
#elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
    if (data->eglDisplay != EGL_NO_DISPLAY && data->eglSurface != EGL_NO_SURFACE)
        eglSwapBuffers(data->eglDisplay, data->eglSurface);
#endif
}

NkOpenGLSwapchain::NkOpenGLSwapchain(NkOpenGLDevice* device)
    : mDevice(device) {}

// =============================================================================
bool NkOpenGLSwapchain::Initialize(NkIGraphicsContext* ctx,
                                    const NkSwapchainDesc& desc)
{
    if (!ctx || !ctx->IsValid()) return false;
    mCtx = ctx;

    auto info  = ctx->GetInfo();
    mWidth     = desc.width  > 0 ? desc.width  : info.windowWidth;
    mHeight    = desc.height > 0 ? desc.height : info.windowHeight;
    if (mWidth == 0)  mWidth  = 1280;
    if (mHeight == 0) mHeight = 720;

    // Déterminer le format couleur selon la description
    if (desc.colorFormat != NkGpuFormat::NK_BGRA8_UNORM) {
        mColorFormat = desc.colorFormat;
    } else {
        // GL défaut : RGBA8 sRGB (le driver décide du format réel)
        mColorFormat = NkGpuFormat::NK_RGBA8_SRGB;
    }

    CreateObjects();
    mIsValid = true;
    return true;
}

// =============================================================================
void NkOpenGLSwapchain::Shutdown()
{
    if (!mIsValid) return;
    DestroyObjects();
    mIsValid = false;
    mCtx     = nullptr;
}

// =============================================================================
bool NkOpenGLSwapchain::AcquireNextImage(NkSemaphoreHandle /*signalSemaphore*/,
                                          NkFenceHandle     /*fence*/,
                                          uint64            /*timeoutNs*/)
{
    // GL : pas de file d'images GPU — l'image est toujours disponible.
    // Les semaphores ne s'appliquent pas ici (GL est synchrone par design).
    return mIsValid;
}

// =============================================================================
bool NkOpenGLSwapchain::Present(const NkSemaphoreHandle* /*waitSemaphores*/,
                                 uint32                   /*waitCount*/)
{
    if (!mIsValid || !mCtx) return false;
    NkSwapPresentOpenGL(mCtx);
    return true;
}

// =============================================================================
void NkOpenGLSwapchain::Resize(uint32 width, uint32 height)
{
    if (width == 0 || height == 0) return;
    if (width == mWidth && height == mHeight) return;

    mWidth  = width;
    mHeight = height;

    DestroyObjects();
    CreateObjects();
}

// =============================================================================
void NkOpenGLSwapchain::CreateObjects()
{
    // Render pass virtuel — décrit comment le FBO 0 est utilisé
    NkRenderPassDesc rpd;
    NkAttachmentDesc color = NkAttachmentDesc::Color(mColorFormat);
    color.loadOp  = NkLoadOp::NK_CLEAR;
    color.storeOp = NkStoreOp::NK_STORE;
    rpd.AddColor(color);

    NkAttachmentDesc depth = NkAttachmentDesc::Depth();
    depth.loadOp  = NkLoadOp::NK_CLEAR;
    depth.storeOp = NkStoreOp::NK_DONT_CARE;
    rpd.SetDepth(depth);

    mRenderPass = mDevice->CreateRenderPass(rpd);

    // FBO 0 — le framebuffer par défaut du contexte GL.
    // On insère un handle fictif dans la map du device pour que les commandes
    // sachent qu'il faut binder l'ID 0.
    NkFramebufferDesc fbd;
    fbd.renderPass = mRenderPass;
    fbd.width      = mWidth;
    fbd.height     = mHeight;
    // Pas d'attachements listés → CreateFramebuffer retourne le FBO 0 par convention
    mFramebuffer = mDevice->CreateFramebuffer(fbd);
}

// =============================================================================
void NkOpenGLSwapchain::DestroyObjects()
{
    if (mFramebuffer.IsValid()) {
        mDevice->DestroyFramebuffer(mFramebuffer);
    }
    if (mRenderPass.IsValid()) {
        mDevice->DestroyRenderPass(mRenderPass);
    }
}

} // namespace nkentseu



