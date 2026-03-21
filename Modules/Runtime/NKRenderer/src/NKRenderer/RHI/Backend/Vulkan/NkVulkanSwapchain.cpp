// =============================================================================
// NkVulkanSwapchain.cpp
// =============================================================================
#ifdef NK_RHI_VK_ENABLED
#include "NKRenderer/RHI/Backend/Vulkan/NkVulkanSwapchain.h"
#include "NKRenderer/RHI/Backend/Vulkan/NkVulkanDevice.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {

// =============================================================================
NkVulkanSwapchain::NkVulkanSwapchain(NkVulkanDevice* device)
    : mDevice(device) {}

// =============================================================================
bool NkVulkanSwapchain::Initialize(NkIGraphicsContext* ctx,
                                    const NkSwapchainDesc& desc)
{
    if (!ctx || !ctx->IsValid()) return false;
    mCtx = ctx;

    mVkDevice     = mDevice->GetVkDevice();
    mSwapchain    = mDevice->GetVkSwapchain();
    mPresentQueue = mDevice->GetVkPresentQueue();
    mVkFormat     = mDevice->GetVkSwapchainFormatNative();
    mImageCount   = mDevice->GetVkSwapchainImageCount();
    mWidth        = mDevice->GetSwapchainWidth();
    mHeight       = mDevice->GetSwapchainHeight();

    if (mVkDevice == VK_NULL_HANDLE || mSwapchain == VK_NULL_HANDLE || mPresentQueue == VK_NULL_HANDLE) {
        logger_src.Infof("[NkVulkanSwapchain] Donnees Vulkan device/swapchain indisponibles\n");
        return false;
    }

    if (desc.width  > 0) mWidth  = desc.width;
    if (desc.height > 0) mHeight = desc.height;

    CreateRenderPassAndFramebuffers();
    mIsValid = true;
    logger_src.Infof("[NkVulkanSwapchain] Initialise (%u�%u, %u images)\n",
                     mWidth, mHeight, mImageCount);
    return true;
}

// =============================================================================
void NkVulkanSwapchain::Shutdown()
{
    if (!mIsValid) return;
    DestroyFramebuffers();
    // Ne pas detruire mSwapchain : appartient au contexte
    mSwapchain  = VK_NULL_HANDLE;
    mVkDevice   = VK_NULL_HANDLE;
    mIsValid    = false;
    mCtx        = nullptr;
}

// =============================================================================
bool NkVulkanSwapchain::AcquireNextImage(NkSemaphoreHandle signalSemaphore,
                                          NkFenceHandle     fence,
                                          uint64            timeoutNs)
{
    if (!mIsValid) return false;

    // Resoudre le handle opaque en VkSemaphore via le device
    VkSemaphore vkSem = mDevice->GetVkSemaphore(signalSemaphore);
    VkFence     vkFen = mDevice->GetVkFence(fence);

    VkResult result = vkAcquireNextImageKHR(
        mVkDevice, mSwapchain, timeoutNs,
        vkSem,     // signale quand l'image est prete
        vkFen,     // VK_NULL_HANDLE si pas de fence
        &mCurrentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // Le swapchain est out-of-date � le caller doit appeler Resize()
        return false;
    }
    if (result != VK_SUCCESS) {
        logger_src.Infof("[NkVulkanSwapchain] vkAcquireNextImageKHR : erreur 0x%X\n",
                         (unsigned)result);
        return false;
    }

    return true;
}

// =============================================================================
bool NkVulkanSwapchain::Present(const NkSemaphoreHandle* waitSemaphores,
                                 uint32                   waitCount)
{
    if (!mIsValid) return false;

    // Resoudre les handles en VkSemaphore
    NkVector<VkSemaphore> vkWaits;
    vkWaits.Reserve(waitCount);
    for (uint32 i = 0; i < waitCount; ++i) {
        VkSemaphore s = mDevice->GetVkSemaphore(waitSemaphores[i]);
        if (s != VK_NULL_HANDLE) vkWaits.PushBack(s);
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = (uint32)vkWaits.Size();
    presentInfo.pWaitSemaphores    = vkWaits.IsEmpty() ? nullptr : vkWaits.Data();
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &mSwapchain;
    presentInfo.pImageIndices      = &mCurrentImageIndex;
    presentInfo.pResults           = nullptr;

    VkResult result = vkQueuePresentKHR(mPresentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return false; // Caller doit Resize()
    }
    if (result != VK_SUCCESS) {
        logger_src.Infof("[NkVulkanSwapchain] vkQueuePresentKHR : erreur 0x%X\n",
                         (unsigned)result);
        return false;
    }
    return true;
}

// =============================================================================
void NkVulkanSwapchain::Resize(uint32 width, uint32 height)
{
    if (width == 0 || height == 0) return;

    DestroyFramebuffers();

    // Le RHI Vulkan possede le swapchain: resize delegue au device.
    mDevice->OnResize(width, height);

    mVkDevice     = mDevice->GetVkDevice();
    mSwapchain    = mDevice->GetVkSwapchain();
    mPresentQueue = mDevice->GetVkPresentQueue();
    mVkFormat     = mDevice->GetVkSwapchainFormatNative();
    mImageCount   = mDevice->GetVkSwapchainImageCount();
    mWidth        = mDevice->GetSwapchainWidth();
    mHeight       = mDevice->GetSwapchainHeight();

    CreateRenderPassAndFramebuffers();
}

// =============================================================================
NkFramebufferHandle NkVulkanSwapchain::GetCurrentFramebuffer() const
{
    if (mCurrentImageIndex < (uint32)mFramebuffers.Size())
        return mFramebuffers[mCurrentImageIndex];
    return NkFramebufferHandle::Null();
}

// =============================================================================
NkGpuFormat NkVulkanSwapchain::GetColorFormat() const
{
    // Conversion VkFormat -> NkGpuFormat pour les formats de swapchain courants
    switch (mVkFormat) {
        case VK_FORMAT_B8G8R8A8_SRGB:  return NkGpuFormat::NK_BGRA8_SRGB;
        case VK_FORMAT_B8G8R8A8_UNORM: return NkGpuFormat::NK_BGRA8_UNORM;
        case VK_FORMAT_R8G8B8A8_SRGB:  return NkGpuFormat::NK_RGBA8_SRGB;
        case VK_FORMAT_R8G8B8A8_UNORM: return NkGpuFormat::NK_RGBA8_UNORM;
        default:                        return NkGpuFormat::NK_BGRA8_SRGB;
    }
}

// =============================================================================
void NkVulkanSwapchain::CreateRenderPassAndFramebuffers()
{
    // Render pass compatible avec le swapchain
    NkRenderPassDesc rpd;
    NkAttachmentDesc color = NkAttachmentDesc::Color(GetColorFormat());
    color.loadOp  = NkLoadOp::NK_CLEAR;
    color.storeOp = NkStoreOp::NK_STORE;
    rpd.AddColor(color);

    NkAttachmentDesc depth = NkAttachmentDesc::Depth();
    depth.loadOp  = NkLoadOp::NK_CLEAR;
    depth.storeOp = NkStoreOp::NK_DONT_CARE;
    rpd.SetDepth(depth);

    mRenderPass = mDevice->CreateRenderPass(rpd);

    // Un framebuffer par image du swapchain
    mFramebuffers.Resize(mImageCount);

    for (uint32 i = 0; i < mImageCount; ++i) {
        NkFramebufferDesc fbd;
        fbd.renderPass = mRenderPass;
        fbd.width      = mWidth;
        fbd.height     = mHeight;
        // Les images de swapchain sont wrappees comme textures dans le device
        // via les NkVulkanDevice::mSwapchainFBs existants.
        if (i < (uint32)mDevice->GetSwapchainFramebufferCount()) {
            mFramebuffers[i] = mDevice->GetSwapchainFramebufferAt(i);
        } else {
            mFramebuffers[i] = mDevice->CreateFramebuffer(fbd);
        }
    }
}

// =============================================================================
void NkVulkanSwapchain::DestroyFramebuffers()
{
    // On ne detruit que les framebuffers crees ici (pas ceux empruntes au device)
    if (mRenderPass.IsValid()) {
        mDevice->DestroyRenderPass(mRenderPass);
    }
    mFramebuffers.Clear();
}

} // namespace nkentseu
#endif // NK_RHI_VK_ENABLED


