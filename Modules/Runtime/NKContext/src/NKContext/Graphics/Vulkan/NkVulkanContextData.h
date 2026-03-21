#pragma once
// =============================================================================
// NkVulkanContextData.h — Données internes contexte Vulkan
// =============================================================================
#include "NKCore/NkTypes.h"

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

    struct NkVulkanContextData {
        // --- Instance & surface ---
        VkInstance               instance        = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugMessenger  = VK_NULL_HANDLE;
        VkSurfaceKHR             surface         = VK_NULL_HANDLE;

        // --- Device ---
        VkPhysicalDevice         physicalDevice  = VK_NULL_HANDLE;
        VkDevice                 device          = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties deviceProps   = {};
        uint32                   graphicsFamily  = UINT32_MAX;
        uint32                   presentFamily   = UINT32_MAX;
        uint32                   computeFamily   = UINT32_MAX;
        VkQueue                  graphicsQueue   = VK_NULL_HANDLE;
        VkQueue                  presentQueue    = VK_NULL_HANDLE;
        VkQueue                  computeQueue    = VK_NULL_HANDLE;

        // --- Swapchain ---
        VkSwapchainKHR           swapchain       = VK_NULL_HANDLE;
        VkFormat                 swapFormat      = VK_FORMAT_UNDEFINED;
        VkExtent2D               swapExtent      = {0, 0};
        uint32                   swapImageCount  = 0;
        VkImage                  swapImages   [kNkVkMaxImages] = {};
        VkImageView              swapViews    [kNkVkMaxImages] = {};

        // --- Depth ---
        VkImage                  depthImage      = VK_NULL_HANDLE;
        VkDeviceMemory           depthMemory     = VK_NULL_HANDLE;
        VkImageView              depthView       = VK_NULL_HANDLE;

        // --- RenderPass & framebuffers ---
        VkRenderPass             renderPass      = VK_NULL_HANDLE;
        VkFramebuffer            framebuffers [kNkVkMaxImages] = {};

        // --- Command pools & buffers ---
        VkCommandPool            commandPool     = VK_NULL_HANDLE;
        VkCommandBuffer          commandBuffers[kNkVkMaxFrames] = {};

        // --- Synchronisation ---
        VkSemaphore              imageAvailSem [kNkVkMaxFrames] = {};
        VkSemaphore              renderDoneSem [kNkVkMaxFrames] = {};
        VkFence                  inFlightFences[kNkVkMaxFrames] = {};
        VkFence                  imageInFlight [kNkVkMaxImages] = {};

        // --- Compute ---
        VkCommandPool            computePool     = VK_NULL_HANDLE;

        // --- État courant ---
        uint32                   currentFrame        = 0;
        uint32                   currentImageIndex   = 0;
        
        // --- NOUVEAU : État de la fenêtre ---
        bool                     minimized           = false;
        bool                     isPresent           = true;
        bool                     isAcquire           = false;

        // --- Infos ---
        const char*              rendererName    = "";
        uint32                   vramMB          = 0;
    };

#else

    struct NkVulkanContextData {};

#endif // NKENTSEU_HAS_VULKAN_HEADERS

} // namespace nkentseu