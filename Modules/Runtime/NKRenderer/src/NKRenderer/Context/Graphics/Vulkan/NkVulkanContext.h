#pragma once
// =============================================================================
// NkVulkanContext.h
// Context Vulkan minimal:
//   - VkInstance (+ debug messenger)
//   - VkSurfaceKHR (liee a la fenetre)
//   - VkPhysicalDevice + indices de queues
// Tout le reste (VkDevice, swapchain, render pass, sync...) est cote RHI.
// =============================================================================
#include "NKRenderer/Context/Core/NkIGraphicsContext.h"
#include "NKRenderer/Context/Core/NkSurfaceDesc.h"
#include "NkVulkanContextData.h"

namespace nkentseu {

class NkVulkanContext final : public NkIGraphicsContext {
public:
    NkVulkanContext() = default;
    ~NkVulkanContext() override;

    bool          Initialize(const NkWindow& w, const NkContextDesc& d) override;
    void          Shutdown() override;
    bool          IsValid() const override;

    // Kept for source compatibility with legacy callers.
    bool          BeginFrame();
    void          EndFrame();
    void          Present();

    bool          OnResize(uint32 w, uint32 h) override;
    void          SetVSync(bool enabled) override;
    bool          GetVSync() const override;
    NkGraphicsApi GetApi() const override;
    NkContextInfo GetInfo() const override;
    NkContextDesc GetDesc() const override;
    void*         GetNativeContextData() override;
    bool          SupportsCompute() const override;

private:
#if NKENTSEU_HAS_VULKAN_HEADERS
    bool CreateInstance(const NkVulkanDesc& d);
    bool CreateDebugMessenger();
    bool CreateSurface(const NkSurfaceDesc& surf);
    bool SelectPhysicalDevice(const NkVulkanDesc& d);
    void FindQueueFamilies(VkPhysicalDevice dev);
    bool IsDeviceSuitable(VkPhysicalDevice dev);
    bool CheckDeviceExtensions(VkPhysicalDevice dev) const;

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* data,
        void* user);
#endif

    NkVulkanContextData mData{};
    NkContextDesc       mDesc{};
    bool                mIsValid = false;
    bool                mVSync   = true;

#if NKENTSEU_HAS_VULKAN_HEADERS
    char mRendererName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE] = {};
#endif
};

} // namespace nkentseu
