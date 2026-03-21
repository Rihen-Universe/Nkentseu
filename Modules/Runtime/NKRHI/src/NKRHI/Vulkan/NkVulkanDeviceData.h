#pragma once
// =============================================================================
// NkVulkanDeviceData.h — Données internes du device Vulkan
// =============================================================================
#include "NKCore/NkTypes.h"
#include "NKRHI/Core/NkTypes.h"
#include "NKRHI/Core/NkDescs.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#   endif
#   include <windows.h>
#endif

#if !defined(NKENTSEU_HAS_VULKAN_HEADERS)
#   if defined(NKENTSEU_PLATFORM_EMSCRIPTEN) || defined(__EMSCRIPTEN__)
#       define NKENTSEU_HAS_VULKAN_HEADERS 0
#   elif defined(__has_include)
#       if __has_include(<vulkan/vulkan.h>)
#           define NKENTSEU_HAS_VULKAN_HEADERS 1
#       else
#           define NKENTSEU_HAS_VULKAN_HEADERS 0
#       endif
#   else
#       define NKENTSEU_HAS_VULKAN_HEADERS 0
#   endif
#endif

#if NKENTSEU_HAS_VULKAN_HEADERS
#   include <vulkan/vulkan.h>
#endif

namespace nkentseu {

#if NKENTSEU_HAS_VULKAN_HEADERS

    static constexpr uint32 kNkVkMaxFrames   = 3;
    static constexpr uint32 kNkVkMaxImages   = 8;

    // =============================================================================
    // Allocateur mémoire Vulkan simplifié (sans VMA)
    // =============================================================================
    struct NkVkAllocation {
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize   offset = 0;
        VkDeviceSize   size   = 0;
        void*          mapped = nullptr; // != nullptr si persistently mapped
    };

    // =============================================================================
    // Objets internes
    // =============================================================================
    struct NkVkBuffer {
        VkBuffer        buffer = VK_NULL_HANDLE;
        NkVkAllocation  alloc;
        NkBufferDesc    desc;
        NkResourceState state = NkResourceState::NK_COMMON;
    };

    struct NkVkTexture {
        VkImage         image  = VK_NULL_HANDLE;
        VkImageView     view   = VK_NULL_HANDLE;
        NkVkAllocation  alloc;
        NkTextureDesc   desc;
        VkImageLayout   layout = VK_IMAGE_LAYOUT_UNDEFINED;
        bool            isSwapchain = false;
    };

    struct NkVkSampler {
        VkSampler sampler = VK_NULL_HANDLE;
    };

    struct NkVkShader {
        struct Stage {
            VkShaderModule module = VK_NULL_HANDLE;
            VkShaderStageFlagBits stage{};
            NkString entryPoint = "main";
        };
        NkVector<Stage> stages;
    };

    struct NkVkPipeline {
        VkPipeline            pipeline = VK_NULL_HANDLE;
        VkPipelineLayout      layout   = VK_NULL_HANDLE;
        VkDescriptorSetLayout descLayout = VK_NULL_HANDLE;
        bool isCompute = false;
    };

    struct NkVkRenderPass {
        VkRenderPass renderPass = VK_NULL_HANDLE;
        NkRenderPassDesc desc;
    };

    struct NkVkFramebuffer {
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        uint32 w=0, h=0;
    };

    struct NkVkDescSetLayout {
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        NkDescriptorSetLayoutDesc desc;
    };

    struct NkVkDescSet {
        VkDescriptorSet set = VK_NULL_HANDLE;
        uint64 layoutId = 0;
    };

    struct NkVkFenceObj {
        VkFence fence = VK_NULL_HANDLE;
    };

    // =============================================================================
    // Frame data (triple buffering)
    // =============================================================================
    struct NkVkFrameData {
        VkCommandPool   cmdPool        = VK_NULL_HANDLE;
        VkCommandBuffer cmdBuffer      = VK_NULL_HANDLE;
        VkFence         inFlightFence  = VK_NULL_HANDLE;
        VkSemaphore     imageAvailable = VK_NULL_HANDLE;
        VkSemaphore     renderFinished = VK_NULL_HANDLE;
        bool            inUse          = false;
    };

    // =============================================================================
    // Données principales du device
    // =============================================================================
    struct NkVulkanDeviceData {
        // --- Instance & surface ---
        VkInstance               instance        = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugMessenger  = VK_NULL_HANDLE;
        VkSurfaceKHR             surface         = VK_NULL_HANDLE;

        // --- Device ---
        VkPhysicalDevice         physicalDevice  = VK_NULL_HANDLE;
        VkDevice                 device          = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties deviceProps   = {};
        uint32                   graphicsFamily = UINT32_MAX;
        uint32                   presentFamily  = UINT32_MAX;
        uint32                   computeFamily  = UINT32_MAX;
        uint32                   transferFamily = UINT32_MAX;
        VkQueue                  graphicsQueue  = VK_NULL_HANDLE;
        VkQueue                  presentQueue   = VK_NULL_HANDLE;
        VkQueue                  computeQueue   = VK_NULL_HANDLE;
        VkQueue                  transferQueue  = VK_NULL_HANDLE;

        // --- Swapchain ---
        VkSwapchainKHR           swapchain       = VK_NULL_HANDLE;
        VkFormat                 swapFormat      = VK_FORMAT_UNDEFINED;
        VkColorSpaceKHR          swapColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        VkExtent2D               swapExtent      = {0, 0};
        uint32                   swapImageCount  = 0;
        NkVector<VkImage>         swapImages;
        NkVector<VkImageView>     swapViews;

        // --- Depth ---
        VkImage                  depthImage      = VK_NULL_HANDLE;
        VkDeviceMemory           depthMemory     = VK_NULL_HANDLE;
        VkImageView              depthView       = VK_NULL_HANDLE;

        // --- Command pools & buffers ---
        VkCommandPool            commandPool     = VK_NULL_HANDLE;
        VkCommandPool            computePool     = VK_NULL_HANDLE;
        VkCommandPool            oneShotPool     = VK_NULL_HANDLE;
        VkCommandBuffer          commandBuffers[kNkVkMaxFrames] = {};

        // --- Frame data ---
        NkVkFrameData            frames[kNkVkMaxFrames];

        // --- Synchronisation ---
        VkSemaphore              imageAvailSem [kNkVkMaxFrames] = {};
        VkSemaphore              renderDoneSem [kNkVkMaxFrames] = {};
        VkFence                  inFlightFences[kNkVkMaxFrames] = {};
        VkFence                  imageInFlight [kNkVkMaxImages] = {};

        // --- Descriptor pool ---
        VkDescriptorPool         descPool        = VK_NULL_HANDLE;

        // --- État courant ---
        uint32                   currentFrame        = 0;
        uint32                   currentImageIndex   = 0;
        bool                     minimized           = false;
        bool                     isPresent           = true;
        bool                     isAcquire           = false;

        // --- Infos ---
        const char*              rendererName    = "";
        uint32                   vramMB          = 0;
    };

#else

    // Stubs quand Vulkan n'est pas disponible
    static constexpr uint32 kNkVkMaxFrames = 3;
    static constexpr uint32 kNkVkMaxImages = 8;

    struct NkVkAllocation {};
    struct NkVkBuffer {};
    struct NkVkTexture {};
    struct NkVkSampler {};
    struct NkVkShader {};
    struct NkVkPipeline {};
    struct NkVkRenderPass {};
    struct NkVkFramebuffer {};
    struct NkVkDescSetLayout {};
    struct NkVkDescSet {};
    struct NkVkFenceObj {};
    struct NkVkFrameData {};

    struct NkVulkanDeviceData {};

#endif // NKENTSEU_HAS_VULKAN_HEADERS

} // namespace nkentseu