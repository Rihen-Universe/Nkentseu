#pragma once
// =============================================================================
// NkVulkanContext.h
// Contexte Vulkan cross-plateforme.
//
// Gère : Instance → Surface → PhysicalDevice → Device → Swapchain → RenderPass
//
// La surface Vulkan est créée à partir de NkSurfaceDesc via les extensions
// de présentation spécifiques à chaque plateforme :
//   Windows   → VK_KHR_win32_surface
//   Linux/X11 → VK_KHR_xlib_surface  ou  VK_KHR_xcb_surface
//   Wayland   → VK_KHR_wayland_surface
//   Android   → VK_KHR_android_surface
//   macOS/iOS → VK_EXT_metal_surface (via MoltenVK)
//   WASM      → Non supporté nativement (WebGPU à la place)
// =============================================================================

#include "NKContext/NkIGraphicsContext.h"
#include "NkVulkanContextData.h"

namespace nkentseu {

    class NkVulkanContext final : public NkIGraphicsContext {
        public:
            NkVulkanContext()  = default;
            ~NkVulkanContext() override;

            bool          Initialize(const NkWindow& w, const NkContextDesc& d) override;
            void          Shutdown()                                             override;
            bool          IsValid()   const                                      override;
            bool          BeginFrame()                                           override;
            void          EndFrame()                                             override;
            void          Present()                                              override;
            bool          OnResize(uint32 w, uint32 h)                           override;
            void          SetVSync(bool enabled)                                 override;
            bool          GetVSync() const                                       override;
            NkGraphicsApi GetApi()   const                                       override;
            NkContextInfo GetInfo()  const                                       override;
            NkContextDesc GetDesc()  const                                       override;
            void*         GetNativeContextData()                                 override;
            bool          SupportsCompute() const                                override;

#if NKENTSEU_HAS_VULKAN_HEADERS
            // Accès direct (évite le cast void*)
            VkDevice        GetDevice()             const { return mData.device; }
            VkRenderPass    GetRenderPass()         const { return mData.renderPass; }
            VkCommandBuffer GetCurrentCommandBuffer()const{ return mData.commandBuffers[mData.currentFrame]; }
            VkFramebuffer   GetCurrentFramebuffer() const { return mData.framebuffers[mData.currentImageIndex]; }
            void            WaitIdle()              const { if(mData.device) vkDeviceWaitIdle(mData.device); }
#endif

        private:
#if NKENTSEU_HAS_VULKAN_HEADERS
            bool CreateInstance(const NkVulkanDesc& d);
            bool CreateDebugMessenger(const NkVulkanDesc& d);
            bool CreateSurface(const NkSurfaceDesc& surf);
            bool SelectPhysicalDevice(const NkVulkanDesc& d);
            bool CreateLogicalDevice(const NkVulkanDesc& d);
            bool CreateSwapchain(uint32 w, uint32 h, const NkVulkanDesc& d);
            bool CreateRenderPass();
            bool CreateDepthResources(uint32 w, uint32 h);
            bool CreateFramebuffers();
            bool CreateCommandPool();
            bool CreateCommandBuffers();
            bool CreateSyncObjects();

            void DestroySwapchainDependents();
            bool RecreateSwapchain(uint32 w, uint32 h);

            VkFormat FindDepthFormat();
            bool     FindMemoryType(uint32 filter, VkMemoryPropertyFlags props, uint32& out);
            bool     IsDeviceSuitable(VkPhysicalDevice dev, const NkVulkanDesc& d);
            void     FindQueueFamilies(VkPhysicalDevice dev);
            bool     CheckDeviceExtensions(VkPhysicalDevice dev);

            static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
                VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                VkDebugUtilsMessageTypeFlagsEXT type,
                const VkDebugUtilsMessengerCallbackDataEXT* data,
                void* user);

            NkVulkanContextData mData;
#endif
            NkContextDesc       mDesc;
            bool                mIsValid = false;
            bool                mVSync   = true;
    };

} // namespace nkentseu
