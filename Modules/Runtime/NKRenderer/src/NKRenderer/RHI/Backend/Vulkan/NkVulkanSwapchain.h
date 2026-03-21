#pragma once
// =============================================================================
// NkVulkanSwapchain.h
// Swapchain Vulkan — wrapping du VkSwapchainKHR du NkVulkanDevice.
//
// Architecture :
//   - NkVulkanContext expose instance/surface/physical device uniquement.
//   - NkVulkanDevice crée et possède VkDevice + VkSwapchainKHR.
//   - NkVulkanSwapchain lit les handles natifs depuis NkVulkanDevice.
//   - AcquireNextImage → vkAcquireNextImageKHR
//   - Present         → vkQueuePresentKHR
//
// Multi-fenêtre : pour plusieurs fenêtres, créer plusieurs contextes,
// chacun avec son propre VkSwapchainKHR et NkVulkanSwapchain.
// =============================================================================
#include "NKRenderer/RHI/Core/NkISwapchain.h"

#ifdef NK_RHI_VK_ENABLED
#include <vulkan/vulkan.h>
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {

class NkVulkanDevice;

class NkVulkanSwapchain final : public NkISwapchain {
public:
    explicit NkVulkanSwapchain(NkVulkanDevice* device);
    ~NkVulkanSwapchain() override { Shutdown(); }

    bool Initialize(NkIGraphicsContext* ctx, const NkSwapchainDesc& desc) override;
    void Shutdown() override;
    bool IsValid() const override { return mIsValid; }

    bool AcquireNextImage(NkSemaphoreHandle signalSemaphore,
                          NkFenceHandle fence = NkFenceHandle::Null(),
                          uint64 timeoutNs = UINT64_MAX) override;

    bool Present(const NkSemaphoreHandle* waitSemaphores, uint32 waitCount) override;

    void Resize(uint32 width, uint32 height) override;

    NkFramebufferHandle GetCurrentFramebuffer() const override;
    NkRenderPassHandle  GetCurrentRenderPass()  const override { return mRenderPass; }
    NkGpuFormat GetColorFormat()       const override;
    NkGpuFormat GetDepthFormat()       const override { return NkGpuFormat::NK_D32_FLOAT; }
    uint32   GetWidth()             const override { return mWidth; }
    uint32   GetHeight()            const override { return mHeight; }
    uint32   GetCurrentImageIndex() const override { return mCurrentImageIndex; }
    uint32   GetImageCount()        const override { return mImageCount; }

    bool SupportsHDR()      const override { return false; }
    bool SupportsTearing()  const override { return mTearingSupported; }

private:
    void CreateRenderPassAndFramebuffers();
    void DestroyFramebuffers();

    NkVulkanDevice*     mDevice             = nullptr;
    NkIGraphicsContext* mCtx                = nullptr;

    // Handles Vulkan natifs (depuis le device — pas proprietaires)
    VkDevice            mVkDevice           = VK_NULL_HANDLE;
    VkSwapchainKHR      mSwapchain          = VK_NULL_HANDLE; // non-owning
    VkQueue             mPresentQueue       = VK_NULL_HANDLE;
    VkFormat            mVkFormat           = VK_FORMAT_B8G8R8A8_SRGB;

    // Render pass + framebuffers créés par NkVulkanDevice (notre référence)
    NkRenderPassHandle              mRenderPass;
    NkVector<NkFramebufferHandle>mFramebuffers;

    uint32  mCurrentImageIndex = 0;
    uint32  mImageCount        = 0;
    uint32  mWidth             = 0;
    uint32  mHeight            = 0;
    bool    mTearingSupported  = false;
    bool    mIsValid           = false;
};

} // namespace nkentseu
#endif // NK_RHI_VK_ENABLED

