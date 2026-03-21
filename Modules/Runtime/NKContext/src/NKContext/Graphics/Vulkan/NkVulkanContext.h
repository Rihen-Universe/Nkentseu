#pragma once
// =============================================================================
// NkVulkanContext.h
// =============================================================================
#include "NKContext/Core/NkIGraphicsContext.h"
#include "NKContext/Core/NkSurfaceDesc.h"
#include "NkVulkanContextData.h"
#include "NKContainers/Sequential/NkVector.h"

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

            // Callbacks de recréation de swapchain
            NkSwapchainCallbackHandle AddCleanUpCallback(NkSwapchainCleanFn fn)     override;
            NkSwapchainCallbackHandle AddRecreateCallback(NkSwapchainRecreateFn fn) override;
            void RemoveCleanUpCallback(NkSwapchainCallbackHandle h)  override;
            void RemoveRecreateCallback(NkSwapchainCallbackHandle h) override;

        #if NKENTSEU_HAS_VULKAN_HEADERS
            // Direct typed access to avoid void* casts.
            VkDevice        GetDevice()              const { return mData.device; }
            VkRenderPass    GetRenderPass()          const { return mData.renderPass; }
            VkCommandBuffer GetCurrentCommandBuffer() const { return mData.commandBuffers[mData.currentFrame]; }
            VkFramebuffer   GetCurrentFramebuffer()  const { return mData.framebuffers[mData.currentImageIndex]; }
            void            WaitIdle()               const { if (mData.device) vkDeviceWaitIdle(mData.device); }
        #endif

        private:
        // Stockage des callbacks clean/recreate (NkVector, sans STL)
        struct NkCbEntry { NkSwapchainCallbackHandle handle; NkSwapchainCleanFn fn; };
        NkVector<NkCbEntry> mCleanCallbacks;
        NkVector<NkCbEntry> mRecreateCallbacks;
        uint32              mNextCbHandle = 1;

        bool CanAquire();

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