#pragma once
// =============================================================================
// NkVulkanContextData.h (Context = instance + surface + physical device)
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

struct NkVulkanContextData {
    // Instance layer
    VkInstance               instance       = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    // Surface owned by the context
    VkSurfaceKHR             surface        = VK_NULL_HANDLE;

    // Selected physical GPU
    VkPhysicalDevice         physicalDevice = VK_NULL_HANDLE;
    uint32                   graphicsFamily = UINT32_MAX;
    uint32                   presentFamily  = UINT32_MAX;
    uint32                   computeFamily  = UINT32_MAX;

    // Read-only metadata used by RHI
    const char*              rendererName   = "";
    uint32                   vramMB         = 0;

    // Surface state (updated on resize)
    uint32                   surfaceWidth   = 0;
    uint32                   surfaceHeight  = 0;
    bool                     minimized      = false;
};

#else

struct NkVulkanContextData {};

#endif // NKENTSEU_HAS_VULKAN_HEADERS

} // namespace nkentseu
