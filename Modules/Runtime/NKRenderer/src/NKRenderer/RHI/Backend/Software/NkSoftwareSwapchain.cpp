// =============================================================================
// NkSoftwareSwapchain.cpp — v4 final (inchangé vs v3 fourni)
// =============================================================================
#include "NkSoftwareSwapchain.h"
#include "NkSoftwareDevice.h"
#include "NKLogger/NkLog.h"

#define NK_SCW_LOG(...) logger_src.Infof("[NkSoftwareSwapchain] " __VA_ARGS__)

namespace nkentseu {

NkSoftwareSwapchain::NkSoftwareSwapchain(NkSoftwareDevice* device)
    : mDevice(device) {}

NkSoftwareSwapchain::~NkSoftwareSwapchain() {
    if (mIsValid) Shutdown();
}

bool NkSoftwareSwapchain::Initialize(NkIGraphicsContext* ctx,
                                      const NkSwapchainDesc& desc) {
    mWidth  = desc.width  > 0 ? desc.width  : (ctx ? ctx->GetInfo().windowWidth  : 512u);
    mHeight = desc.height > 0 ? desc.height : (ctx ? ctx->GetInfo().windowHeight : 512u);
    if (mWidth  == 0) mWidth  = 512u;
    if (mHeight == 0) mHeight = 512u;
    CreateObjects();
    mIsValid = true;
    NK_SCW_LOG("Initialisé (%u×%u)\n", mWidth, mHeight);
    return true;
}

void NkSoftwareSwapchain::Shutdown() {
    DestroyObjects();
    mIsValid = false;
    NK_SCW_LOG("Shutdown\n");
}

bool NkSoftwareSwapchain::AcquireNextImage(NkSemaphoreHandle signalSemaphore,
                                            NkFenceHandle fence, uint64) {
    if (fence.IsValid()) mDevice->ResetFence(fence);
    (void)signalSemaphore;
    return true;
}

bool NkSoftwareSwapchain::Present(const NkSemaphoreHandle*, uint32) {
    mDevice->Present();
    return true;
}

void NkSoftwareSwapchain::Resize(uint32 width, uint32 height) {
    if (width == 0 || height == 0) return;
    if (width == mWidth && height == mHeight) return;
    mWidth = width; mHeight = height;
    DestroyObjects();
    CreateObjects();
    NK_SCW_LOG("Resize → %u×%u\n", mWidth, mHeight);
}

void NkSoftwareSwapchain::CreateObjects() {
    NkTextureDesc cd;
    cd.format    = NkGpuFormat::NK_RGBA8_UNORM;
    cd.width     = mWidth; cd.height = mHeight; cd.mipLevels = 1;
    cd.bindFlags = NkBindFlags::NK_RENDER_TARGET | NkBindFlags::NK_SHADER_RESOURCE;
    cd.debugName = "SWSwapchain_Color";
    mColorTex = mDevice->CreateTexture(cd);

    NkTextureDesc dd;
    dd.format    = NkGpuFormat::NK_D32_FLOAT;
    dd.width     = mWidth; dd.height = mHeight; dd.mipLevels = 1;
    dd.bindFlags = NkBindFlags::NK_DEPTH_STENCIL;
    dd.debugName = "SWSwapchain_Depth";
    mDepthTex = mDevice->CreateTexture(dd);

    NkRenderPassDesc rpd;
    rpd.AddColor(NkAttachmentDesc::Color(NkGpuFormat::NK_RGBA8_UNORM))
       .SetDepth(NkAttachmentDesc::Depth());
    mRenderPass = mDevice->CreateRenderPass(rpd);

    NkFramebufferDesc fbd;
    fbd.renderPass = mRenderPass;
    fbd.colorAttachments.PushBack(mColorTex);
    fbd.depthAttachment = mDepthTex;
    fbd.width = mWidth; fbd.height = mHeight;
    mFramebuffer = mDevice->CreateFramebuffer(fbd);
}

void NkSoftwareSwapchain::DestroyObjects() {
    if (mFramebuffer.IsValid()) mDevice->DestroyFramebuffer(mFramebuffer);
    if (mRenderPass.IsValid())  mDevice->DestroyRenderPass(mRenderPass);
    if (mColorTex.IsValid())    mDevice->DestroyTexture(mColorTex);
    if (mDepthTex.IsValid())    mDevice->DestroyTexture(mDepthTex);
    mFramebuffer = {}; mRenderPass = {}; mColorTex = {}; mDepthTex = {};
}

} // namespace nkentseu
