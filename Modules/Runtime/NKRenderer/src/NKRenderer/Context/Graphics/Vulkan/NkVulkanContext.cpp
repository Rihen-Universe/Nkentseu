// =============================================================================
// NkVulkanContext.cpp
// Context Vulkan minimal: instance + surface + physical device selection.
// =============================================================================
#include "NkVulkanContext.h"

#include "NKPlatform/NkPlatformDetect.h"
#include "NKLogger/NkLog.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKRenderer/Context/Core/NkGpuPolicy.h"
#include "NKContainers/Sequential/NkVector.h"

#include <cstring>

#if NKENTSEU_HAS_VULKAN_HEADERS

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   include <vulkan/vulkan_win32.h>
#elif defined(NKENTSEU_WINDOWING_XLIB)
#   include <vulkan/vulkan_xlib.h>
#elif defined(NKENTSEU_WINDOWING_XCB)
#   include <vulkan/vulkan_xcb.h>
#elif defined(NKENTSEU_WINDOWING_WAYLAND)
#   include <vulkan/vulkan_wayland.h>
#elif defined(NKENTSEU_PLATFORM_ANDROID)
#   include <vulkan/vulkan_android.h>
#elif defined(NKENTSEU_PLATFORM_MACOS)
#   include <vulkan/vulkan_metal.h>
#endif

#define NK_VK_LOG(...) logger.Infof("[NkVulkan] " __VA_ARGS__)
#define NK_VK_ERR(...) logger.Errorf("[NkVulkan] " __VA_ARGS__)
#define NK_VK_CHECKRET(r, msg) do { VkResult _r=(r); if(_r!=VK_SUCCESS){ NK_VK_ERR(msg " (err=%d)\n",(int)_r); return false; } } while(0)

namespace nkentseu {

NkVulkanContext::~NkVulkanContext() {
    if (mData.instance || mData.surface || mData.debugMessenger) {
        Shutdown();
    }
}

bool NkVulkanContext::Initialize(const NkWindow& window, const NkContextDesc& desc) {
    if (mIsValid) {
        NK_VK_ERR("Already initialized\n");
        return false;
    }

    mDesc = desc;
    mVSync = desc.vulkan.vsync;

    const NkSurfaceDesc surf = window.GetSurfaceDesc();
    if (!surf.IsValid()) {
        NK_VK_ERR("Invalid NkSurfaceDesc\n");
        return false;
    }

    if (!CreateInstance(desc.vulkan)) {
        Shutdown();
        return false;
    }
    if (desc.vulkan.debugMessenger && !CreateDebugMessenger()) {
        Shutdown();
        return false;
    }
    if (!CreateSurface(surf)) {
        Shutdown();
        return false;
    }
    if (!SelectPhysicalDevice(desc.vulkan)) {
        Shutdown();
        return false;
    }

    mData.surfaceWidth  = surf.width;
    mData.surfaceHeight = surf.height;
    mData.minimized     = (surf.width == 0 || surf.height == 0);

    mIsValid = true;
    NK_VK_LOG("Context ready - GPU=%s | VRAM=%u MB\n", mData.rendererName, mData.vramMB);
    return true;
}

void NkVulkanContext::Shutdown() {
    if (mData.surface && mData.instance) {
        vkDestroySurfaceKHR(mData.instance, mData.surface, nullptr);
    }

    if (mData.debugMessenger && mData.instance) {
        const auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(mData.instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (fn) {
            fn(mData.instance, mData.debugMessenger, nullptr);
        }
    }

    if (mData.instance) {
        vkDestroyInstance(mData.instance, nullptr);
    }

    mData = NkVulkanContextData{};
    mRendererName[0] = '\0';
    mIsValid = false;
}

bool NkVulkanContext::BeginFrame() { return false; }
void NkVulkanContext::EndFrame() {}
void NkVulkanContext::Present() {}

bool NkVulkanContext::OnResize(uint32 w, uint32 h) {
    if (!mIsValid) {
        return false;
    }
    mData.surfaceWidth  = w;
    mData.surfaceHeight = h;
    mData.minimized     = (w == 0 || h == 0);
    return true;
}

void NkVulkanContext::SetVSync(bool enabled) { mVSync = enabled; }
bool NkVulkanContext::GetVSync() const { return mVSync; }
bool NkVulkanContext::IsValid() const { return mIsValid; }
NkGraphicsApi NkVulkanContext::GetApi() const { return NkGraphicsApi::NK_API_VULKAN; }
NkContextDesc NkVulkanContext::GetDesc() const { return mDesc; }
void* NkVulkanContext::GetNativeContextData() { return &mData; }
bool NkVulkanContext::SupportsCompute() const { return mData.computeFamily != UINT32_MAX; }

NkContextInfo NkVulkanContext::GetInfo() const {
    NkContextInfo i{};
    i.api              = NkGraphicsApi::NK_API_VULKAN;
    i.renderer         = mData.rendererName;
    i.vendor           = mData.rendererName;
    i.version          = "Vulkan";
    i.vramMB           = mData.vramMB;
    i.computeSupported = SupportsCompute();
    i.windowWidth      = mData.surfaceWidth;
    i.windowHeight     = mData.surfaceHeight;
    return i;
}

bool NkVulkanContext::CreateInstance(const NkVulkanDesc& d) {
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = d.appName;
    appInfo.applicationVersion = d.appVersion;
    appInfo.pEngineName        = d.engineName;
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = d.apiVersion ? d.apiVersion : VK_API_VERSION_1_3;

    NkVector<const char*> extensions;
    extensions.PushBack(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    extensions.PushBack(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(NKENTSEU_WINDOWING_XLIB)
    extensions.PushBack(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#elif defined(NKENTSEU_WINDOWING_XCB)
    extensions.PushBack(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(NKENTSEU_WINDOWING_WAYLAND)
    extensions.PushBack(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#elif defined(NKENTSEU_PLATFORM_ANDROID)
    extensions.PushBack(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(NKENTSEU_PLATFORM_MACOS)
    extensions.PushBack(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
    extensions.PushBack(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    extensions.PushBack(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#endif
    if (d.debugMessenger) {
        extensions.PushBack(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    for (uint32 i = 0; i < d.extraInstanceExtCount; ++i) {
        extensions.PushBack(d.extraInstanceExt[i]);
    }

    NkVector<const char*> layers;
    if (d.validationLayers) {
        layers.PushBack("VK_LAYER_KHRONOS_validation");
    }

    VkInstanceCreateInfo info{};
    info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo        = &appInfo;
    info.enabledExtensionCount   = (uint32)extensions.Size();
    info.ppEnabledExtensionNames = extensions.Data();
    info.enabledLayerCount       = (uint32)layers.Size();
    info.ppEnabledLayerNames     = layers.Data();
#if defined(NKENTSEU_PLATFORM_MACOS)
    info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    NK_VK_CHECKRET(vkCreateInstance(&info, nullptr, &mData.instance), "vkCreateInstance");
    return true;
}

VKAPI_ATTR VkBool32 VKAPI_CALL NkVulkanContext::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*)
{
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        logger.Errorf("[NkVulkan][Validation] %s", data->pMessage);
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        logger.Warnf("[NkVulkan][Validation] %s", data->pMessage);
    } else {
        logger.Infof("[NkVulkan][Validation] %s", data->pMessage);
    }
    return VK_FALSE;
}

bool NkVulkanContext::CreateDebugMessenger() {
    VkDebugUtilsMessengerCreateInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = DebugCallback;

    const auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(mData.instance, "vkCreateDebugUtilsMessengerEXT"));
    if (!fn) {
        NK_VK_ERR("vkCreateDebugUtilsMessengerEXT not available\n");
        return false;
    }
    NK_VK_CHECKRET(fn(mData.instance, &info, nullptr, &mData.debugMessenger),
                   "vkCreateDebugUtilsMessengerEXT");
    return true;
}

bool NkVulkanContext::CreateSurface(const NkSurfaceDesc& surf) {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    VkWin32SurfaceCreateInfoKHR ci{};
    ci.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    ci.hwnd      = static_cast<HWND>(surf.hwnd);
    ci.hinstance = static_cast<HINSTANCE>(surf.hinstance);
    NK_VK_CHECKRET(vkCreateWin32SurfaceKHR(mData.instance, &ci, nullptr, &mData.surface),
                   "vkCreateWin32SurfaceKHR");
#elif defined(NKENTSEU_WINDOWING_XLIB)
    VkXlibSurfaceCreateInfoKHR ci{};
    ci.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    ci.dpy    = static_cast<Display*>(surf.display);
    ci.window = (::Window)surf.window;
    NK_VK_CHECKRET(vkCreateXlibSurfaceKHR(mData.instance, &ci, nullptr, &mData.surface),
                   "vkCreateXlibSurfaceKHR");
#elif defined(NKENTSEU_WINDOWING_XCB)
    VkXcbSurfaceCreateInfoKHR ci{};
    ci.sType      = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    ci.connection = static_cast<xcb_connection_t*>(surf.connection);
    ci.window     = (xcb_window_t)surf.window;
    NK_VK_CHECKRET(vkCreateXcbSurfaceKHR(mData.instance, &ci, nullptr, &mData.surface),
                   "vkCreateXcbSurfaceKHR");
#elif defined(NKENTSEU_WINDOWING_WAYLAND)
    VkWaylandSurfaceCreateInfoKHR ci{};
    ci.sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    ci.display = static_cast<wl_display*>(surf.display);
    ci.surface = static_cast<wl_surface*>(surf.surface);
    NK_VK_CHECKRET(vkCreateWaylandSurfaceKHR(mData.instance, &ci, nullptr, &mData.surface),
                   "vkCreateWaylandSurfaceKHR");
#elif defined(NKENTSEU_PLATFORM_ANDROID)
    VkAndroidSurfaceCreateInfoKHR ci{};
    ci.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    ci.window = static_cast<ANativeWindow*>(surf.nativeWindow);
    NK_VK_CHECKRET(vkCreateAndroidSurfaceKHR(mData.instance, &ci, nullptr, &mData.surface),
                   "vkCreateAndroidSurfaceKHR");
#elif defined(NKENTSEU_PLATFORM_MACOS)
    VkMetalSurfaceCreateInfoEXT ci{};
    ci.sType  = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    ci.pLayer = static_cast<const CAMetalLayer*>(surf.metalLayer);
    NK_VK_CHECKRET(vkCreateMetalSurfaceEXT(mData.instance, &ci, nullptr, &mData.surface),
                   "vkCreateMetalSurfaceEXT");
#else
    NK_VK_ERR("No Vulkan surface implementation for this platform\n");
    return false;
#endif
    return true;
}

void NkVulkanContext::FindQueueFamilies(VkPhysicalDevice dev) {
    uint32 count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    NkVector<VkQueueFamilyProperties> props(count);
    if (count > 0) {
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, props.Data());
    }

    mData.graphicsFamily = UINT32_MAX;
    mData.presentFamily  = UINT32_MAX;
    mData.computeFamily  = UINT32_MAX;

    for (uint32 i = 0; i < count; ++i) {
        if ((props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && mData.graphicsFamily == UINT32_MAX) {
            mData.graphicsFamily = i;
        }
        if ((props[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            !(props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            mData.computeFamily == UINT32_MAX) {
            mData.computeFamily = i;
        }

        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, mData.surface, &present);
        if (present && mData.presentFamily == UINT32_MAX) {
            mData.presentFamily = i;
        }
    }

    if (mData.computeFamily == UINT32_MAX &&
        mData.graphicsFamily != UINT32_MAX &&
        (props[mData.graphicsFamily].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
        mData.computeFamily = mData.graphicsFamily;
    }
}

bool NkVulkanContext::CheckDeviceExtensions(VkPhysicalDevice dev) const {
    uint32 count = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, nullptr);
    NkVector<VkExtensionProperties> avail(count);
    if (count > 0) {
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, avail.Data());
    }
    for (uint32 i = 0; i < avail.Size(); ++i) {
        if (std::strcmp(avail[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            return true;
        }
    }
    return false;
}

bool NkVulkanContext::IsDeviceSuitable(VkPhysicalDevice dev) {
    FindQueueFamilies(dev);
    if (mData.graphicsFamily == UINT32_MAX || mData.presentFamily == UINT32_MAX) {
        return false;
    }
    if (!CheckDeviceExtensions(dev)) {
        return false;
    }
    uint32 fmtCount = 0;
    uint32 modeCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, mData.surface, &fmtCount, nullptr);
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, mData.surface, &modeCount, nullptr);
    return fmtCount > 0 && modeCount > 0;
}

bool NkVulkanContext::SelectPhysicalDevice(const NkVulkanDesc& d) {
    uint32 count = 0;
    vkEnumeratePhysicalDevices(mData.instance, &count, nullptr);
    if (count == 0) {
        NK_VK_ERR("No Vulkan physical device\n");
        return false;
    }

    NkVector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(mData.instance, &count, devices.Data());

    struct Candidate {
        VkPhysicalDevice dev = VK_NULL_HANDLE;
        uint32 index = UINT32_MAX;
        VkPhysicalDeviceProperties props{};
    };

    NkVector<Candidate> candidates;
    candidates.Reserve(devices.Size());
    for (uint32 i = 0; i < devices.Size(); ++i) {
        if (!IsDeviceSuitable(devices[i])) {
            continue;
        }
        Candidate c{};
        c.dev = devices[i];
        c.index = i;
        vkGetPhysicalDeviceProperties(c.dev, &c.props);
        candidates.PushBack(c);
    }

    if (candidates.Empty()) {
        NK_VK_ERR("No suitable Vulkan device\n");
        return false;
    }

    const uint32 requestedIndex =
        (mDesc.gpu.adapterIndex >= 0)
            ? static_cast<uint32>(mDesc.gpu.adapterIndex)
            : d.preferredAdapterIndex;

    const NkGpuVendor vendorPreference = mDesc.gpu.vendorPreference;
    const bool strictVendor = vendorPreference != NkGpuVendor::NK_ANY;

    auto matchesVendor = [&](const Candidate& c) -> bool {
        return NkGpuPolicy::MatchesVendorPciId(c.props.vendorID, vendorPreference);
    };

    auto scoreCandidate = [&](const Candidate& c) -> int {
        switch (mDesc.gpu.preference) {
            case NkGpuPreference::NK_HIGH_PERFORMANCE:
                if (c.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) return 400;
                if (c.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) return 250;
                if (c.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) return 120;
                if (c.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) return 60;
                return 100;
            case NkGpuPreference::NK_LOW_POWER:
                if (c.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) return 400;
                if (c.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) return 250;
                if (c.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) return 120;
                if (c.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) return 60;
                return 100;
            case NkGpuPreference::NK_DEFAULT:
            default:
                if (c.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) return 350;
                if (c.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) return 250;
                if (c.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) return 120;
                if (c.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) return 60;
                return 100;
        }
    };

    const Candidate* selected = nullptr;
    if (requestedIndex != UINT32_MAX) {
        for (uint32 i = 0; i < candidates.Size(); ++i) {
            if (candidates[i].index == requestedIndex) {
                if (!strictVendor || matchesVendor(candidates[i])) {
                    selected = &candidates[i];
                }
                break;
            }
        }
    }

    if (!selected) {
        int bestScore = -1;
        for (uint32 i = 0; i < candidates.Size(); ++i) {
            if (strictVendor && !matchesVendor(candidates[i])) {
                continue;
            }
            const int score = scoreCandidate(candidates[i]);
            if (score > bestScore) {
                bestScore = score;
                selected = &candidates[i];
            }
        }
        if (!selected && strictVendor) {
            int bestScoreNoVendor = -1;
            for (uint32 i = 0; i < candidates.Size(); ++i) {
                const int score = scoreCandidate(candidates[i]);
                if (score > bestScoreNoVendor) {
                    bestScoreNoVendor = score;
                    selected = &candidates[i];
                }
            }
        }
    }

    if (!selected) {
        NK_VK_ERR("Failed to select Vulkan adapter\n");
        return false;
    }

    mData.physicalDevice = selected->dev;
    FindQueueFamilies(selected->dev);

    std::strncpy(mRendererName, selected->props.deviceName, sizeof(mRendererName) - 1);
    mRendererName[sizeof(mRendererName) - 1] = '\0';
    mData.rendererName = mRendererName;

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(selected->dev, &memProps);
    for (uint32 i = 0; i < memProps.memoryHeapCount; ++i) {
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            mData.vramMB = (uint32)(memProps.memoryHeaps[i].size / (1024u * 1024u));
            break;
        }
    }

    NK_VK_LOG("Selected GPU #%u: %s | vendor=0x%04X | VRAM=%u MB\n",
              selected->index,
              mData.rendererName,
              (unsigned)selected->props.vendorID,
              mData.vramMB);
    return true;
}

} // namespace nkentseu

#else

namespace nkentseu {

NkVulkanContext::~NkVulkanContext() = default;

bool NkVulkanContext::Initialize(const NkWindow&, const NkContextDesc& desc) {
    mDesc = desc;
    mIsValid = false;
    logger.Errorf("[NkVulkan] Vulkan headers not found at build time.");
    return false;
}

void NkVulkanContext::Shutdown() { mIsValid = false; }
bool NkVulkanContext::IsValid() const { return mIsValid; }
bool NkVulkanContext::BeginFrame() { return false; }
void NkVulkanContext::EndFrame() {}
void NkVulkanContext::Present() {}
bool NkVulkanContext::OnResize(uint32, uint32) { return false; }
void NkVulkanContext::SetVSync(bool enabled) { mVSync = enabled; }
bool NkVulkanContext::GetVSync() const { return mVSync; }
NkGraphicsApi NkVulkanContext::GetApi() const { return NkGraphicsApi::NK_API_VULKAN; }
NkContextDesc NkVulkanContext::GetDesc() const { return mDesc; }
void* NkVulkanContext::GetNativeContextData() { return nullptr; }
bool NkVulkanContext::SupportsCompute() const { return false; }

NkContextInfo NkVulkanContext::GetInfo() const {
    NkContextInfo info{};
    info.api = NkGraphicsApi::NK_API_VULKAN;
    info.renderer = "Unavailable";
    info.vendor = "Unavailable";
    info.version = "Vulkan headers missing";
    info.computeSupported = false;
    return info;
}

} // namespace nkentseu

#endif // NKENTSEU_HAS_VULKAN_HEADERS

