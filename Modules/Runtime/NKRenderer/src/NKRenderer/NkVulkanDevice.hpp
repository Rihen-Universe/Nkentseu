// =============================================================================
// NkVulkanDevice.cpp — Implémentation Vulkan multiplateforme
//
// Plateformes : Windows, Linux (XLib/XCB/Wayland), macOS/iOS (MoltenVK),
//               Android, WASM (partiel via vkGetInstanceProcAddr)
// =============================================================================

#include "NkVulkanDevice.h"
#include "NKWindow/Core/NkSurface.h"
#include "NKLogger/NkLog.h"

// ── Vulkan headers ───────────────────────────────────────────────────────────
#include <vulkan/vulkan.h>

// Extensions de surface par plateforme
#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   include <vulkan/vulkan_win32.h>
#   define VK_SURFACE_EXT       "VK_KHR_win32_surface"
#   define NK_VK_PLATFORM_INST  VK_KHR_WIN32_SURFACE_EXTENSION_NAME

#elif defined(NKENTSEU_WINDOWING_XLIB)
#   include <vulkan/vulkan_xlib.h>
#   define VK_SURFACE_EXT       "VK_KHR_xlib_surface"
#   define NK_VK_PLATFORM_INST  VK_KHR_XLIB_SURFACE_EXTENSION_NAME

#elif defined(NKENTSEU_WINDOWING_XCB)
#   include <vulkan/vulkan_xcb.h>
#   define NK_VK_PLATFORM_INST  VK_KHR_XCB_SURFACE_EXTENSION_NAME

#elif defined(NKENTSEU_WINDOWING_WAYLAND)
#   include <vulkan/vulkan_wayland.h>
#   define NK_VK_PLATFORM_INST  VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME

#elif defined(NKENTSEU_PLATFORM_ANDROID)
#   include <vulkan/vulkan_android.h>
#   define NK_VK_PLATFORM_INST  VK_KHR_ANDROID_SURFACE_EXTENSION_NAME

#elif defined(NKENTSEU_PLATFORM_MACOS)
#   include <vulkan/vulkan_metal.h>
#   define NK_VK_PLATFORM_INST  VK_EXT_METAL_SURFACE_EXTENSION_NAME

#elif defined(NKENTSEU_PLATFORM_IOS)
#   include <vulkan/vulkan_metal.h>
#   define NK_VK_PLATFORM_INST  VK_EXT_METAL_SURFACE_EXTENSION_NAME

#else
#   define NK_VK_PLATFORM_INST  ""
#endif

#include <cstring>
#include <vector>
#include <algorithm>
#include <cassert>

// Macro de vérification Vulkan
#define VK_CHECK(expr)  do { \
    VkResult _r = (expr); \
    if (_r != VK_SUCCESS) { \
        logger.Errorf("[Vulkan] %s failed: %d (%s:%d)", \
                #expr, static_cast<int>(_r), __FILE__, __LINE__); \
        return false; \
    } } while(0)

#define VK_CHECKVOID(expr) do { \
    VkResult _r = (expr); \
    if (_r != VK_SUCCESS) { \
        logger.Errorf("[Vulkan] %s failed: %d", #expr, static_cast<int>(_r)); \
        return; \
    } } while(0)

namespace nkentseu { namespace rhi {

// Cast utilitaires
#define VKI(x)  reinterpret_cast<VkInstance>(x)
#define VKD(x)  reinterpret_cast<VkDevice>(x)
#define VKPD(x) reinterpret_cast<VkPhysicalDevice>(x)
#define VKS(x)  reinterpret_cast<VkSurfaceKHR>(x)
#define VKSC(x) reinterpret_cast<VkSwapchainKHR>(x)
#define VKRP(x) reinterpret_cast<VkRenderPass>(x)
#define VKFB(x) reinterpret_cast<VkFramebuffer>(x)
#define VKCB(x) reinterpret_cast<VkCommandBuffer>(x)
#define VKBUF(x) reinterpret_cast<VkBuffer>(x)
#define VKIMG(x) reinterpret_cast<VkImage>(x)
#define VKIV(x)  reinterpret_cast<VkImageView>(x)
#define VKMEM(x) reinterpret_cast<VkDeviceMemory>(x)
#define VKPL(x)  reinterpret_cast<VkPipeline>(x)
#define VKPLL(x) reinterpret_cast<VkPipelineLayout>(x)
#define VKSP(x)  reinterpret_cast<VkSampler>(x)
#define VKSM(x)  reinterpret_cast<VkShaderModule>(x)
#define VKDSL(x) reinterpret_cast<VkDescriptorSetLayout>(x)
#define VKDS(x)  reinterpret_cast<VkDescriptorSet>(x)
#define VKDP(x)  reinterpret_cast<VkDescriptorPool>(x)
#define VKQ(x)   reinterpret_cast<VkQueue>(x)
#define VKFN(x)  reinterpret_cast<VkFence>(x)
#define VKSEM(x) reinterpret_cast<VkSemaphore>(x)
#define VKCP(x)  reinterpret_cast<VkCommandPool>(x)

// ---------------------------------------------------------------------------
// Helpers de conversion NkRHI → Vulkan
// ---------------------------------------------------------------------------

static VkFormat ToVkFormat(NkTextureFormat f) {
    switch (f) {
        case NkTextureFormat::RGBA8_UNORM:       return VK_FORMAT_R8G8B8A8_UNORM;
        case NkTextureFormat::RGBA8_SRGB:        return VK_FORMAT_R8G8B8A8_SRGB;
        case NkTextureFormat::BGRA8_UNORM:       return VK_FORMAT_B8G8R8A8_UNORM;
        case NkTextureFormat::BGRA8_SRGB:        return VK_FORMAT_B8G8R8A8_SRGB;
        case NkTextureFormat::R8_UNORM:          return VK_FORMAT_R8_UNORM;
        case NkTextureFormat::RG8_UNORM:         return VK_FORMAT_R8G8_UNORM;
        case NkTextureFormat::RGBA16F:           return VK_FORMAT_R16G16B16A16_SFLOAT;
        case NkTextureFormat::RGBA32F:           return VK_FORMAT_R32G32B32A32_SFLOAT;
        case NkTextureFormat::R32F:              return VK_FORMAT_R32_SFLOAT;
        case NkTextureFormat::RG32F:             return VK_FORMAT_R32G32_SFLOAT;
        case NkTextureFormat::Depth16:           return VK_FORMAT_D16_UNORM;
        case NkTextureFormat::Depth24Stencil8:   return VK_FORMAT_D24_UNORM_S8_UINT;
        case NkTextureFormat::Depth32F:          return VK_FORMAT_D32_SFLOAT;
        case NkTextureFormat::BC1_RGBA_UNORM:    return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case NkTextureFormat::BC3_RGBA_UNORM:    return VK_FORMAT_BC3_UNORM_BLOCK;
        case NkTextureFormat::BC7_RGBA_UNORM:    return VK_FORMAT_BC7_UNORM_BLOCK;
        default:                                 return VK_FORMAT_UNDEFINED;
    }
}

static VkShaderStageFlagBits ToVkStage(NkShaderStage s) {
    VkShaderStageFlags flags = 0;
    if (static_cast<NkU32>(s) & static_cast<NkU32>(NkShaderStage::Vertex))
        flags |= VK_SHADER_STAGE_VERTEX_BIT;
    if (static_cast<NkU32>(s) & static_cast<NkU32>(NkShaderStage::Fragment))
        flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (static_cast<NkU32>(s) & static_cast<NkU32>(NkShaderStage::Compute))
        flags |= VK_SHADER_STAGE_COMPUTE_BIT;
    if (static_cast<NkU32>(s) & static_cast<NkU32>(NkShaderStage::Geometry))
        flags |= VK_SHADER_STAGE_GEOMETRY_BIT;
    return static_cast<VkShaderStageFlagBits>(flags);
}

static VkDescriptorType ToVkDescType(NkDescriptorType t) {
    switch (t) {
        case NkDescriptorType::UniformBuffer:        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case NkDescriptorType::StorageBuffer:        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case NkDescriptorType::CombinedImageSampler: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case NkDescriptorType::StorageImage:         return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case NkDescriptorType::InputAttachment:      return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        default:                                     return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
}

static VkFilter ToVkFilter(NkFilter f) {
    return f == NkFilter::Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
}

static VkSamplerAddressMode ToVkWrap(NkWrapMode w) {
    switch (w) {
        case NkWrapMode::Repeat:         return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case NkWrapMode::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case NkWrapMode::ClampToEdge:    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case NkWrapMode::ClampToBorder:  return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default:                         return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

static VkCompareOp ToVkCompare(NkCompareOp op) {
    switch (op) {
        case NkCompareOp::Never:        return VK_COMPARE_OP_NEVER;
        case NkCompareOp::Less:         return VK_COMPARE_OP_LESS;
        case NkCompareOp::Equal:        return VK_COMPARE_OP_EQUAL;
        case NkCompareOp::LessEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        case NkCompareOp::Greater:      return VK_COMPARE_OP_GREATER;
        case NkCompareOp::NotEqual:     return VK_COMPARE_OP_NOT_EQUAL;
        case NkCompareOp::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case NkCompareOp::Always:       return VK_COMPARE_OP_ALWAYS;
        default:                        return VK_COMPARE_OP_LESS;
    }
}

static VkBlendFactor ToVkBlend(NkBlendFactor b) {
    switch (b) {
        case NkBlendFactor::Zero:                return VK_BLEND_FACTOR_ZERO;
        case NkBlendFactor::One:                 return VK_BLEND_FACTOR_ONE;
        case NkBlendFactor::SrcColor:            return VK_BLEND_FACTOR_SRC_COLOR;
        case NkBlendFactor::OneMinusSrcColor:    return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case NkBlendFactor::DstColor:            return VK_BLEND_FACTOR_DST_COLOR;
        case NkBlendFactor::OneMinusDstColor:    return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case NkBlendFactor::SrcAlpha:            return VK_BLEND_FACTOR_SRC_ALPHA;
        case NkBlendFactor::OneMinusSrcAlpha:    return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case NkBlendFactor::DstAlpha:            return VK_BLEND_FACTOR_DST_ALPHA;
        case NkBlendFactor::OneMinusDstAlpha:    return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        default:                                 return VK_BLEND_FACTOR_ONE;
    }
}

static VkBlendOp ToVkBlendOp(NkBlendOp op) {
    switch (op) {
        case NkBlendOp::Add:             return VK_BLEND_OP_ADD;
        case NkBlendOp::Subtract:        return VK_BLEND_OP_SUBTRACT;
        case NkBlendOp::ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case NkBlendOp::Min:             return VK_BLEND_OP_MIN;
        case NkBlendOp::Max:             return VK_BLEND_OP_MAX;
        default:                         return VK_BLEND_OP_ADD;
    }
}

static VkVertexInputRate ToVkInputRate(bool instanced) {
    return instanced ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
}

static VkFormat ToVkVertexFormat(NkVertexFormat f) {
    switch (f) {
        case NkVertexFormat::Float1: return VK_FORMAT_R32_SFLOAT;
        case NkVertexFormat::Float2: return VK_FORMAT_R32G32_SFLOAT;
        case NkVertexFormat::Float3: return VK_FORMAT_R32G32B32_SFLOAT;
        case NkVertexFormat::Float4: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case NkVertexFormat::Int1:   return VK_FORMAT_R32_SINT;
        case NkVertexFormat::Int2:   return VK_FORMAT_R32G32_SINT;
        case NkVertexFormat::Int3:   return VK_FORMAT_R32G32B32_SINT;
        case NkVertexFormat::Int4:   return VK_FORMAT_R32G32B32A32_SINT;
        case NkVertexFormat::UByte4_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
        default:                     return VK_FORMAT_R32G32B32_SFLOAT;
    }
}

// =============================================================================
// NkVulkanDevice — Init
// =============================================================================

NkVulkanDevice::~NkVulkanDevice() { if (mInitialized) Shutdown(); }

bool NkVulkanDevice::Init(const NkSurfaceDesc& surface,
                           const NkGraphicsContextConfig& config)
{
    mValidation = config.vkValidation;

    if (!CreateInstance(config))         return false;
    if (!CreateSurface(surface))         return false;
    if (!PickPhysicalDevice())           return false;
    if (!CreateLogicalDevice())          return false;
    if (!CreateSwapchain(surface.width, surface.height)) return false;
    if (!CreateSwapchainViews())         return false;
    if (!CreateDepthBuffer(surface.width, surface.height)) return false;
    if (!CreateRenderPassInternal())     return false;
    if (!CreateFramebuffersInternal())   return false;
    if (!CreateCommandPool())            return false;
    if (!CreateDescriptorPool())         return false;
    if (!CreateCommandBuffers())         return false;
    if (!CreateSyncObjects())            return false;

    mInitialized = true;
    return true;
}

// =============================================================================
// CreateInstance
// =============================================================================

bool NkVulkanDevice::CreateInstance(const NkGraphicsContextConfig& cfg) {
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "NkApp";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "NkEngine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = cfg.vkApiVersion != 0
                                   ? cfg.vkApiVersion
                                   : VK_API_VERSION_1_2;

    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        NK_VK_PLATFORM_INST,
    };
    if (cfg.vkValidation)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // macOS/iOS : portability extension (MoltenVK)
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
    extensions.push_back("VK_KHR_portability_enumeration");
    extensions.push_back("VK_KHR_get_physical_device_properties2");
#endif

    std::vector<const char*> layers;
    if (cfg.vkValidation)
        layers.push_back("VK_LAYER_KHRONOS_validation");

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &appInfo;
    ci.enabledExtensionCount   = (NkU32)extensions.size();
    ci.ppEnabledExtensionNames = extensions.data();
    ci.enabledLayerCount       = (NkU32)layers.size();
    ci.ppEnabledLayerNames     = layers.empty() ? nullptr : layers.data();

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
    ci.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    VkInstance inst;
    VK_CHECK(vkCreateInstance(&ci, nullptr, &inst));
    mInstance = inst;
    return true;
}

// =============================================================================
// CreateSurface — multiplateforme
// =============================================================================

bool NkVulkanDevice::CreateSurface(const NkSurfaceDesc& sd) {
    VkSurfaceKHR surface = VK_NULL_HANDLE;

#if defined(NKENTSEU_PLATFORM_WINDOWS)
    VkWin32SurfaceCreateInfoKHR ci{};
    ci.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    ci.hwnd      = sd.hwnd;
    ci.hinstance = sd.hinstance;
    VK_CHECK(vkCreateWin32SurfaceKHR(VKI(mInstance), &ci, nullptr, &surface));

#elif defined(NKENTSEU_WINDOWING_XLIB)
    VkXlibSurfaceCreateInfoKHR ci{};
    ci.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    ci.dpy    = (Display*)sd.display;
    ci.window = (Window)sd.window;
    VK_CHECK(vkCreateXlibSurfaceKHR(VKI(mInstance), &ci, nullptr, &surface));

#elif defined(NKENTSEU_WINDOWING_XCB)
    VkXcbSurfaceCreateInfoKHR ci{};
    ci.sType      = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    ci.connection = (xcb_connection_t*)sd.connection;
    ci.window     = (xcb_window_t)sd.window;
    VK_CHECK(vkCreateXcbSurfaceKHR(VKI(mInstance), &ci, nullptr, &surface));

#elif defined(NKENTSEU_WINDOWING_WAYLAND)
    VkWaylandSurfaceCreateInfoKHR ci{};
    ci.sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    ci.display = (wl_display*)sd.display;
    ci.surface = (wl_surface*)sd.surface;
    VK_CHECK(vkCreateWaylandSurfaceKHR(VKI(mInstance), &ci, nullptr, &surface));

#elif defined(NKENTSEU_PLATFORM_ANDROID)
    VkAndroidSurfaceCreateInfoKHR ci{};
    ci.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    ci.window = (ANativeWindow*)sd.nativeWindow;
    VK_CHECK(vkCreateAndroidSurfaceKHR(VKI(mInstance), &ci, nullptr, &surface));

#elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
    VkMetalSurfaceCreateInfoEXT ci{};
    ci.sType   = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    ci.pLayer  = sd.metalLayer; // CAMetalLayer*
    VK_CHECK(vkCreateMetalSurfaceEXT(VKI(mInstance), &ci, nullptr, &surface));

#else
    logger.Error("[Vulkan] CreateSurface: plateforme non supportée");
    return false;
#endif

    mSurface = surface;
    return true;
}

// =============================================================================
// PickPhysicalDevice
// =============================================================================

bool NkVulkanDevice::PickPhysicalDevice() {
    NkU32 count = 0;
    vkEnumeratePhysicalDevices(VKI(mInstance), &count, nullptr);
    if (count == 0) return false;

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(VKI(mInstance), &count, devices.data());

    // Sélectionner le meilleur GPU (discret > intégré > virtuel)
    VkPhysicalDevice best = VK_NULL_HANDLE;
    int bestScore = -1;

    for (auto& dev : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);

        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)   score = 3;
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score = 2;
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) score = 1;

        // Vérifier que les queues nécessaires existent
        NkU32 qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qProps(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, qProps.data());

        bool hasGraphics = false, hasPresent = false;
        for (NkU32 i = 0; i < qCount; ++i) {
            if (qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) hasGraphics = true;
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, VKS(mSurface), &present);
            if (present) hasPresent = true;
        }
        if (!hasGraphics || !hasPresent) continue;

        if (score > bestScore) { best = dev; bestScore = score; }
    }

    if (!best) return false;
    mPhysicalDevice = best;

    // Trouver les familles de queues
    NkU32 qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(VKPD(mPhysicalDevice), &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(VKPD(mPhysicalDevice), &qCount, qProps.data());

    mGraphicsFamily = mPresentFamily = mComputeFamily = UINT32_MAX;
    for (NkU32 i = 0; i < qCount; ++i) {
        if ((qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && mGraphicsFamily == UINT32_MAX)
            mGraphicsFamily = i;
        if ((qProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && mComputeFamily == UINT32_MAX)
            mComputeFamily = i;
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(VKPD(mPhysicalDevice), i, VKS(mSurface), &present);
        if (present && mPresentFamily == UINT32_MAX) mPresentFamily = i;
    }

    return mGraphicsFamily != UINT32_MAX && mPresentFamily != UINT32_MAX;
}

// =============================================================================
// CreateLogicalDevice
// =============================================================================

bool NkVulkanDevice::CreateLogicalDevice() {
    std::vector<NkU32> uniqueFamilies = { mGraphicsFamily };
    if (mPresentFamily  != mGraphicsFamily) uniqueFamilies.push_back(mPresentFamily);
    if (mComputeFamily  != mGraphicsFamily) uniqueFamilies.push_back(mComputeFamily);

    float priority = 1.f;
    std::vector<VkDeviceQueueCreateInfo> queueCIs;
    for (NkU32 fam : uniqueFamilies) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = fam;
        qi.queueCount       = 1;
        qi.pQueuePriorities = &priority;
        queueCIs.push_back(qi);
    }

    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = VK_TRUE;
    features.fillModeNonSolid  = VK_TRUE; // wireframe
    features.multiDrawIndirect = VK_TRUE;

    std::vector<const char*> extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
    extensions.push_back("VK_KHR_portability_subset");
#endif

    VkDeviceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount    = (NkU32)queueCIs.size();
    ci.pQueueCreateInfos       = queueCIs.data();
    ci.pEnabledFeatures        = &features;
    ci.enabledExtensionCount   = (NkU32)extensions.size();
    ci.ppEnabledExtensionNames = extensions.data();

    VkDevice dev;
    VK_CHECK(vkCreateDevice(VKPD(mPhysicalDevice), &ci, nullptr, &dev));
    mDevice = dev;

    VkQueue q;
    vkGetDeviceQueue(dev, mGraphicsFamily, 0, &q); mGraphicsQueue = q;
    vkGetDeviceQueue(dev, mPresentFamily,  0, &q); mPresentQueue  = q;
    vkGetDeviceQueue(dev, mComputeFamily,  0, &q); mComputeQueue  = q;

    return true;
}

// =============================================================================
// CreateSwapchain
// =============================================================================

bool NkVulkanDevice::CreateSwapchain(NkU32 width, NkU32 height) {
    // Capacités de la surface
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VKPD(mPhysicalDevice),
                                               VKS(mSurface), &caps);

    // Format : préférer BGRA8_SRGB
    NkU32 fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(VKPD(mPhysicalDevice),
                                          VKS(mSurface), &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(VKPD(mPhysicalDevice),
                                          VKS(mSurface), &fmtCount, formats.data());

    VkSurfaceFormatKHR chosen = formats[0];
    for (auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        { chosen = f; break; }
    }
    mSwapchainFormat = chosen.format;

    // Present mode : MAILBOX (triple buffering) sinon FIFO
    NkU32 pmCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(VKPD(mPhysicalDevice),
                                               VKS(mSurface), &pmCount, nullptr);
    std::vector<VkPresentModeKHR> pms(pmCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(VKPD(mPhysicalDevice),
                                               VKS(mSurface), &pmCount, pms.data());

    VkPresentModeKHR pm = VK_PRESENT_MODE_FIFO_KHR;
    for (auto& p : pms)
        if (p == VK_PRESENT_MODE_MAILBOX_KHR) { pm = p; break; }

    // Extent
    VkExtent2D ext;
    if (caps.currentExtent.width != UINT32_MAX) {
        ext = caps.currentExtent;
    } else {
        ext.width  = std::clamp(width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
        ext.height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    mWidth  = ext.width;
    mHeight = ext.height;

    NkU32 imgCount = std::min(caps.minImageCount + 1,
                               caps.maxImageCount > 0 ? caps.maxImageCount : 8u);

    VkSwapchainCreateInfoKHR sci{};
    sci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface          = VKS(mSurface);
    sci.minImageCount    = imgCount;
    sci.imageFormat      = chosen.format;
    sci.imageColorSpace  = chosen.colorSpace;
    sci.imageExtent      = ext;
    sci.imageArrayLayers = 1;
    sci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    NkU32 qfams[] = { mGraphicsFamily, mPresentFamily };
    if (mGraphicsFamily != mPresentFamily) {
        sci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        sci.queueFamilyIndexCount = 2;
        sci.pQueueFamilyIndices   = qfams;
    } else {
        sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    sci.preTransform   = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode    = pm;
    sci.clipped        = VK_TRUE;

    VkSwapchainKHR sc;
    VK_CHECK(vkCreateSwapchainKHR(VKD(mDevice), &sci, nullptr, &sc));
    mSwapchain = sc;

    vkGetSwapchainImagesKHR(VKD(mDevice), sc, &mSwapImageCount, nullptr);
    mSwapImageCount = std::min(mSwapImageCount, kMaxSwapImages);
    VkImage imgs[kMaxSwapImages];
    vkGetSwapchainImagesKHR(VKD(mDevice), sc, &mSwapImageCount, imgs);
    for (NkU32 i = 0; i < mSwapImageCount; ++i) mSwapImages[i] = imgs[i];

    return true;
}

bool NkVulkanDevice::CreateSwapchainViews() {
    for (NkU32 i = 0; i < mSwapImageCount; ++i) {
        VkImageViewCreateInfo ci{};
        ci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ci.image    = VKIMG(mSwapImages[i]);
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format   = (VkFormat)mSwapchainFormat;
        ci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        VkImageView iv;
        VK_CHECK(vkCreateImageView(VKD(mDevice), &ci, nullptr, &iv));
        mSwapViews[i] = iv;

        // Enregistrer comme NkTextureHandle
        NkU32 slot = mTextures.Alloc();
        auto* entry = mTextures.Get(slot);
        entry->image     = mSwapImages[i];
        entry->imageView = iv;
        entry->width  = mWidth; entry->height = mHeight;
        entry->format = mSwapchainFormat;
        entry->isSwapchain = true;
        mSwapTextureHandles[i].id = slot;
    }
    return true;
}

bool NkVulkanDevice::CreateDepthBuffer(NkU32 width, NkU32 height) {
    // Trouver un format depth supporté
    VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT
    };
    VkFormat depthFmt = VK_FORMAT_UNDEFINED;
    for (auto f : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(VKPD(mPhysicalDevice), f, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            depthFmt = f; break;
        }
    }
    if (depthFmt == VK_FORMAT_UNDEFINED) return false;
    mDepthFormat = depthFmt;

    // Créer l'image
    VkImageCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType     = VK_IMAGE_TYPE_2D;
    ci.format        = depthFmt;
    ci.extent        = { width, height, 1 };
    ci.mipLevels     = 1; ci.arrayLayers = 1;
    ci.samples       = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ci.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage img;
    VK_CHECK(vkCreateImage(VKD(mDevice), &ci, nullptr, &img));
    mDepthImage = img;

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(VKD(mDevice), img, &req);
    VkDeviceMemory mem;
    if (!AllocateMemory(req.size, req.memoryTypeBits,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, (void**)&mem))
        return false;
    mDepthMemory = mem;
    vkBindImageMemory(VKD(mDevice), img, mem, 0);

    VkImageViewCreateInfo vci{};
    vci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image    = img;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format   = depthFmt;
    bool hasStencil = (depthFmt == VK_FORMAT_D24_UNORM_S8_UINT ||
                       depthFmt == VK_FORMAT_D32_SFLOAT_S8_UINT);
    vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT |
                                       (hasStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u);
    vci.subresourceRange.levelCount = 1;
    vci.subresourceRange.layerCount = 1;
    VkImageView dv;
    VK_CHECK(vkCreateImageView(VKD(mDevice), &vci, nullptr, &dv));
    mDepthView = dv;

    return true;
}

bool NkVulkanDevice::CreateRenderPassInternal() {
    VkAttachmentDescription colorAtt{};
    colorAtt.format         = (VkFormat)mSwapchainFormat;
    colorAtt.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAtt.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAtt.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAtt{};
    depthAtt.format         = (VkFormat)mDepthFormat;
    depthAtt.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAtt.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAtt.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAtt.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference depthRef{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription atts[] = { colorAtt, depthAtt };
    VkRenderPassCreateInfo rci{};
    rci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rci.attachmentCount = 2;
    rci.pAttachments    = atts;
    rci.subpassCount    = 1;
    rci.pSubpasses      = &subpass;
    rci.dependencyCount = 1;
    rci.pDependencies   = &dep;

    VkRenderPass rp;
    VK_CHECK(vkCreateRenderPass(VKD(mDevice), &rci, nullptr, &rp));
    mSwapRenderPass = rp;

    // Exposer via handle
    NkU32 slot = mRenderPasses.Alloc();
    mRenderPasses.Get(slot)->renderPass = rp;
    mSwapRenderPassHandle.id = slot;

    return true;
}

bool NkVulkanDevice::CreateFramebuffersInternal() {
    for (NkU32 i = 0; i < mSwapImageCount; ++i) {
        VkImageView atts[] = { VKIV(mSwapViews[i]), VKIV(mDepthView) };
        VkFramebufferCreateInfo fci{};
        fci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fci.renderPass      = VKRP(mSwapRenderPass);
        fci.attachmentCount = 2;
        fci.pAttachments    = atts;
        fci.width           = mWidth;
        fci.height          = mHeight;
        fci.layers          = 1;

        VkFramebuffer fb;
        VK_CHECK(vkCreateFramebuffer(VKD(mDevice), &fci, nullptr, &fb));
        mSwapFBs[i] = fb;

        NkU32 slot = mFramebuffers.Alloc();
        auto* entry = mFramebuffers.Get(slot);
        entry->framebuffer = fb;
        entry->width = mWidth; entry->height = mHeight;
        mSwapFBHandles[i].id = slot;
    }
    return true;
}

bool NkVulkanDevice::CreateCommandPool() {
    VkCommandPoolCreateInfo ci{};
    ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.queueFamilyIndex = mGraphicsFamily;
    ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool cp;
    VK_CHECK(vkCreateCommandPool(VKD(mDevice), &ci, nullptr, &cp));
    mCommandPool = cp;
    return true;
}

bool NkVulkanDevice::CreateDescriptorPool() {
    VkDescriptorPoolSize sizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,          1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,          1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,            500 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,         200 },
    };
    VkDescriptorPoolCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    ci.maxSets       = 4000;
    ci.poolSizeCount = (NkU32)(sizeof(sizes)/sizeof(sizes[0]));
    ci.pPoolSizes    = sizes;
    VkDescriptorPool dp;
    VK_CHECK(vkCreateDescriptorPool(VKD(mDevice), &ci, nullptr, &dp));
    mDescriptorPool = dp;
    return true;
}

bool NkVulkanDevice::CreateCommandBuffers() {
    VkCommandBufferAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool        = VKCP(mCommandPool);
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    VkCommandBuffer cbs[MAX_FRAMES_IN_FLIGHT];
    VK_CHECK(vkAllocateCommandBuffers(VKD(mDevice), &ai, cbs));
    for (NkU32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        mCmdBuffers[i] = cbs[i];
        mCmdWrappers[i] = new NkVulkanCommandBuffer(cbs[i], mDevice);
        // Connecter les pools
        mCmdWrappers[i]->mBuffers     = &mBuffers;
        mCmdWrappers[i]->mTextures    = &mTextures;
        mCmdWrappers[i]->mPipelines   = &mPipelines;
        mCmdWrappers[i]->mDescSets    = &mDescSets;
        mCmdWrappers[i]->mDescLayouts = &mDescLayouts;
        mCmdWrappers[i]->mRenderPasses= &mRenderPasses;
        mCmdWrappers[i]->mFramebuffers= &mFramebuffers;
    }
    return true;
}

bool NkVulkanDevice::CreateSyncObjects() {
    VkSemaphoreCreateInfo sci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (NkU32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkSemaphore s1, s2; VkFence fn;
        VK_CHECK(vkCreateSemaphore(VKD(mDevice), &sci, nullptr, &s1));
        VK_CHECK(vkCreateSemaphore(VKD(mDevice), &sci, nullptr, &s2));
        VK_CHECK(vkCreateFence    (VKD(mDevice), &fci, nullptr, &fn));
        mImageAvailable[i] = s1;
        mRenderFinished[i] = s2;
        mInFlight[i]       = fn;
    }
    return true;
}

// =============================================================================
// Frame
// =============================================================================

bool NkVulkanDevice::BeginFrame() {
    vkWaitForFences(VKD(mDevice), 1,
                    (VkFence*)&mInFlight[mCurrentFrame], VK_TRUE, UINT64_MAX);

    VkResult res = vkAcquireNextImageKHR(
        VKD(mDevice), VKSC(mSwapchain), UINT64_MAX,
        VKSEM(mImageAvailable[mCurrentFrame]),
        VK_NULL_HANDLE, &mCurrentImage);

    if (res == VK_ERROR_OUT_OF_DATE_KHR) return false;
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) return false;

    vkResetFences(VKD(mDevice), 1, (VkFence*)&mInFlight[mCurrentFrame]);

    vkResetCommandBuffer(VKCB(mCmdBuffers[mCurrentFrame]), 0);
    mCmdWrappers[mCurrentFrame]->Begin();

    return true;
}

void NkVulkanDevice::EndFrame(NkRHICommandBuffer& cmd) {
    cmd.End();

    VkCommandBuffer cb = VKCB(mCmdBuffers[mCurrentFrame]);
    VkSemaphore waitSem   = VKSEM(mImageAvailable[mCurrentFrame]);
    VkSemaphore signalSem = VKSEM(mRenderFinished[mCurrentFrame]);
    VkFence     fence     = VKFN (mInFlight     [mCurrentFrame]);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{};
    si.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount   = 1;
    si.pWaitSemaphores      = &waitSem;
    si.pWaitDstStageMask    = &waitStage;
    si.commandBufferCount   = 1;
    si.pCommandBuffers      = &cb;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores    = &signalSem;

    vkQueueSubmit(VKQQ(mGraphicsQueue), 1, &si, fence);

    VkSwapchainKHR sc = VKSC(mSwapchain);
    VkPresentInfoKHR pi{};
    pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores    = &signalSem;
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &sc;
    pi.pImageIndices      = &mCurrentImage;

    vkQueuePresentKHR(VKQQ(mPresentQueue), &pi);
    mCurrentFrame = (mCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

bool NkVulkanDevice::Recreate(NkU32 width, NkU32 height) {
    WaitIdle();
    DestroySwapchain();
    if (!CreateSwapchain(width, height))       return false;
    if (!CreateSwapchainViews())               return false;
    if (!CreateDepthBuffer(width, height))     return false;
    if (!CreateRenderPassInternal())           return false;
    if (!CreateFramebuffersInternal())         return false;
    return true;
}

void NkVulkanDevice::WaitIdle() {
    if (mDevice) vkDeviceWaitIdle(VKD(mDevice));
}

// =============================================================================
// Ressources — Buffer
// =============================================================================

NkU32 NkVulkanDevice::FindMemoryType(NkU32 typeFilter, NkU32 props) const {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(VKPD(mPhysicalDevice), &mp);
    for (NkU32 i = 0; i < mp.memoryTypeCount; ++i)
        if ((typeFilter & (1 << i)) &&
            (mp.memoryTypes[i].propertyFlags & props) == props)
            return i;
    return UINT32_MAX;
}

bool NkVulkanDevice::AllocateMemory(NkU64 size, NkU32 filter,
                                     NkU32 props, void** outMem) const {
    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = size;
    ai.memoryTypeIndex = FindMemoryType(filter, props);
    if (ai.memoryTypeIndex == UINT32_MAX) return false;
    VkDeviceMemory mem;
    if (vkAllocateMemory(VKD(mDevice), &ai, nullptr, &mem) != VK_SUCCESS)
        return false;
    *outMem = mem;
    return true;
}

NkBufferHandle NkVulkanDevice::CreateBuffer(const NkBufferDesc& desc) {
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (desc.usage & NkBufferUsage::Vertex)   usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (desc.usage & NkBufferUsage::Index)    usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (desc.usage & NkBufferUsage::Uniform)  usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (desc.usage & NkBufferUsage::Storage)  usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (desc.usage & NkBufferUsage::Indirect) usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    if (desc.usage & NkBufferUsage::TransferSrc) usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VkMemoryPropertyFlags memFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if (desc.memory == NkMemoryType::CpuToGpu)
        memFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    else if (desc.memory == NkMemoryType::GpuToCpu)
        memFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

    VkBufferCreateInfo ci{};
    ci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ci.size        = desc.size;
    ci.usage       = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buf;
    if (vkCreateBuffer(VKD(mDevice), &ci, nullptr, &buf) != VK_SUCCESS)
        return NkBufferHandle::Null();

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(VKD(mDevice), buf, &req);

    VkDeviceMemory mem;
    if (!AllocateMemory(req.size, req.memoryTypeBits, memFlags, (void**)&mem)) {
        vkDestroyBuffer(VKD(mDevice), buf, nullptr);
        return NkBufferHandle::Null();
    }
    vkBindBufferMemory(VKD(mDevice), buf, mem, 0);

    // Upload initial data si fourni
    if (desc.initData && desc.memory == NkMemoryType::CpuToGpu) {
        void* mapped;
        vkMapMemory(VKD(mDevice), mem, 0, desc.size, 0, &mapped);
        memcpy(mapped, desc.initData, desc.size);
        vkUnmapMemory(VKD(mDevice), mem);
    } else if (desc.initData) {
        // Staging → DEVICE_LOCAL
        NkBufferDesc stagDesc;
        stagDesc.size   = desc.size;
        stagDesc.usage  = NkBufferUsage::TransferSrc;
        stagDesc.memory = NkMemoryType::CpuToGpu;
        stagDesc.initData = desc.initData;
        NkBufferHandle stag = CreateBuffer(stagDesc);
        if (stag.IsValid()) {
            auto* se = mBuffers.Get(stag.id);
            CopyBufferImmediate(se->buffer, buf, desc.size);
            DestroyBuffer(stag);
        }
    }

    NkU32 slot = mBuffers.Alloc();
    if (!slot) { vkDestroyBuffer(VKD(mDevice), buf, nullptr); vkFreeMemory(VKD(mDevice), mem, nullptr); return NkBufferHandle::Null(); }
    auto* entry = mBuffers.Get(slot);
    entry->buffer     = buf;
    entry->allocation = mem;
    entry->size       = desc.size;
    entry->usage      = desc.usage;
    entry->memory     = desc.memory;
    return { slot };
}

void NkVulkanDevice::DestroyBuffer(NkBufferHandle h) {
    auto* e = mBuffers.Get(h.id);
    if (!e) return;
    if (e->mapped) vkUnmapMemory(VKD(mDevice), VKMEM(e->allocation));
    vkDestroyBuffer  (VKD(mDevice), VKBUF(e->buffer),     nullptr);
    vkFreeMemory     (VKD(mDevice), VKMEM(e->allocation),  nullptr);
    mBuffers.Free(h.id);
}

void* NkVulkanDevice::MapBuffer(NkBufferHandle h) {
    auto* e = mBuffers.Get(h.id);
    if (!e || e->mapped) return e ? e->mapped : nullptr;
    vkMapMemory(VKD(mDevice), VKMEM(e->allocation), 0, e->size, 0, &e->mapped);
    return e->mapped;
}

void NkVulkanDevice::UnmapBuffer(NkBufferHandle h) {
    auto* e = mBuffers.Get(h.id);
    if (!e || !e->mapped) return;
    vkUnmapMemory(VKD(mDevice), VKMEM(e->allocation));
    e->mapped = nullptr;
}

// =============================================================================
// Ressources — Texture
// =============================================================================

NkTextureHandle NkVulkanDevice::CreateTexture(const NkTextureDesc& desc) {
    VkImageUsageFlags usage = 0;
    if (static_cast<NkU32>(desc.usage) & static_cast<NkU32>(NkTextureUsage::Sampled))
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (static_cast<NkU32>(desc.usage) & static_cast<NkU32>(NkTextureUsage::RenderTarget))
        usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (static_cast<NkU32>(desc.usage) & static_cast<NkU32>(NkTextureUsage::DepthStencil))
        usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (static_cast<NkU32>(desc.usage) & static_cast<NkU32>(NkTextureUsage::Storage))
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (static_cast<NkU32>(desc.usage) & static_cast<NkU32>(NkTextureUsage::TransferDst))
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (static_cast<NkU32>(desc.usage) & static_cast<NkU32>(NkTextureUsage::TransferSrc))
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VkFormat fmt = ToVkFormat(desc.format);
    VkImageCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType     = desc.depth > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
    ci.format        = fmt;
    ci.extent        = { desc.width, desc.height, desc.depth };
    ci.mipLevels     = desc.mipLevels;
    ci.arrayLayers   = desc.arrayLayers;
    ci.samples       = (VkSampleCountFlagBits)desc.samples;
    ci.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ci.usage         = usage;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage img;
    if (vkCreateImage(VKD(mDevice), &ci, nullptr, &img) != VK_SUCCESS)
        return NkTextureHandle::Null();

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(VKD(mDevice), img, &req);
    VkDeviceMemory mem;
    if (!AllocateMemory(req.size, req.memoryTypeBits,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, (void**)&mem)) {
        vkDestroyImage(VKD(mDevice), img, nullptr);
        return NkTextureHandle::Null();
    }
    vkBindImageMemory(VKD(mDevice), img, mem, 0);

    bool isDepth = (static_cast<NkU32>(desc.usage) &
                    static_cast<NkU32>(NkTextureUsage::DepthStencil)) != 0;
    VkImageViewCreateInfo vci{};
    vci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image    = img;
    vci.viewType = desc.arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY
                                        : VK_IMAGE_VIEW_TYPE_2D;
    vci.format   = fmt;
    vci.subresourceRange.aspectMask = isDepth
        ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    vci.subresourceRange.levelCount = desc.mipLevels;
    vci.subresourceRange.layerCount = desc.arrayLayers;

    VkImageView iv;
    if (vkCreateImageView(VKD(mDevice), &vci, nullptr, &iv) != VK_SUCCESS) {
        vkDestroyImage(VKD(mDevice), img, nullptr);
        vkFreeMemory(VKD(mDevice), mem, nullptr);
        return NkTextureHandle::Null();
    }

    NkU32 slot = mTextures.Alloc();
    auto* entry = mTextures.Get(slot);
    entry->image      = img;
    entry->imageView  = iv;
    entry->allocation = mem;
    entry->width      = desc.width;
    entry->height     = desc.height;
    entry->mipLevels  = desc.mipLevels;
    entry->format     = fmt;
    entry->layout     = VK_IMAGE_LAYOUT_UNDEFINED;
    return { slot };
}

void NkVulkanDevice::DestroyTexture(NkTextureHandle h) {
    auto* e = mTextures.Get(h.id);
    if (!e || e->isSwapchain) return;
    vkDestroyImageView(VKD(mDevice), VKIV(e->imageView),  nullptr);
    vkDestroyImage    (VKD(mDevice), VKIMG(e->image),      nullptr);
    vkFreeMemory      (VKD(mDevice), VKMEM(e->allocation), nullptr);
    mTextures.Free(h.id);
}

// =============================================================================
// Ressources — Sampler
// =============================================================================

NkSamplerHandle NkVulkanDevice::CreateSampler(const NkSamplerDesc& desc) {
    VkSamplerCreateInfo ci{};
    ci.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    ci.magFilter        = ToVkFilter(desc.magFilter);
    ci.minFilter        = ToVkFilter(desc.minFilter);
    ci.mipmapMode       = desc.mipmapMode == NkMipmapMode::Linear
                            ? VK_SAMPLER_MIPMAP_MODE_LINEAR
                            : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    ci.addressModeU     = ToVkWrap(desc.wrapU);
    ci.addressModeV     = ToVkWrap(desc.wrapV);
    ci.addressModeW     = ToVkWrap(desc.wrapW);
    ci.mipLodBias       = desc.mipLodBias;
    ci.anisotropyEnable = desc.anisotropy ? VK_TRUE : VK_FALSE;
    ci.maxAnisotropy    = desc.maxAnisotropy;
    ci.compareEnable    = desc.compareEnable ? VK_TRUE : VK_FALSE;
    ci.compareOp        = ToVkCompare(desc.compareOp);
    ci.minLod           = desc.minLod;
    ci.maxLod           = desc.maxLod;
    ci.borderColor      = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

    VkSampler smp;
    if (vkCreateSampler(VKD(mDevice), &ci, nullptr, &smp) != VK_SUCCESS)
        return NkSamplerHandle::Null();

    NkU32 slot = mSamplers.Alloc();
    mSamplers.Get(slot)->sampler = smp;
    return { slot };
}

void NkVulkanDevice::DestroySampler(NkSamplerHandle h) {
    auto* e = mSamplers.Get(h.id);
    if (!e) return;
    vkDestroySampler(VKD(mDevice), VKSP(e->sampler), nullptr);
    mSamplers.Free(h.id);
}

// =============================================================================
// Ressources — Shader
// =============================================================================

NkShaderHandle NkVulkanDevice::CreateShader(const NkShaderDesc& desc) {
    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = desc.codeSize;
    ci.pCode    = reinterpret_cast<const NkU32*>(desc.code);
    VkShaderModule mod;
    if (vkCreateShaderModule(VKD(mDevice), &ci, nullptr, &mod) != VK_SUCCESS)
        return NkShaderHandle::Null();
    NkU32 slot = mShaders.Alloc();
    auto* e = mShaders.Get(slot);
    e->module = mod;
    e->stage  = desc.stage;
    e->entry  = desc.entry;
    return { slot };
}

void NkVulkanDevice::DestroyShader(NkShaderHandle h) {
    auto* e = mShaders.Get(h.id);
    if (!e) return;
    vkDestroyShaderModule(VKD(mDevice), VKSM(e->module), nullptr);
    mShaders.Free(h.id);
}

// =============================================================================
// Ressources — DescriptorLayout
// =============================================================================

NkDescriptorLayoutHandle NkVulkanDevice::CreateDescriptorLayout(
    const NkDescriptorLayoutDesc& desc)
{
    std::vector<VkDescriptorSetLayoutBinding> bindings(desc.bindingCount);
    for (NkU32 i = 0; i < desc.bindingCount; ++i) {
        bindings[i].binding         = desc.bindings[i].binding;
        bindings[i].descriptorType  = ToVkDescType(desc.bindings[i].type);
        bindings[i].descriptorCount = desc.bindings[i].count;
        bindings[i].stageFlags      = ToVkStage(desc.bindings[i].stages);
        bindings[i].pImmutableSamplers = nullptr;
    }
    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = (NkU32)bindings.size();
    ci.pBindings    = bindings.data();

    VkDescriptorSetLayout dsl;
    if (vkCreateDescriptorSetLayout(VKD(mDevice), &ci, nullptr, &dsl) != VK_SUCCESS)
        return NkDescriptorLayoutHandle::Null();

    NkU32 slot = mDescLayouts.Alloc();
    mDescLayouts.Get(slot)->layout = dsl;
    return { slot };
}

void NkVulkanDevice::DestroyDescriptorLayout(NkDescriptorLayoutHandle h) {
    auto* e = mDescLayouts.Get(h.id);
    if (!e) return;
    vkDestroyDescriptorSetLayout(VKD(mDevice), VKDSL(e->layout), nullptr);
    mDescLayouts.Free(h.id);
}

// =============================================================================
// Ressources — DescriptorSet
// =============================================================================

NkDescriptorSetHandle NkVulkanDevice::AllocDescriptorSet(
    NkDescriptorLayoutHandle layout)
{
    auto* le = mDescLayouts.Get(layout.id);
    if (!le) return NkDescriptorSetHandle::Null();

    VkDescriptorSetLayout dsl = VKDSL(le->layout);
    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = VKDP(mDescriptorPool);
    ai.descriptorSetCount = 1;
    ai.pSetLayouts        = &dsl;

    VkDescriptorSet ds;
    if (vkAllocateDescriptorSets(VKD(mDevice), &ai, &ds) != VK_SUCCESS)
        return NkDescriptorSetHandle::Null();

    NkU32 slot = mDescSets.Alloc();
    mDescSets.Get(slot)->set = ds;
    return { slot };
}

void NkVulkanDevice::FreeDescriptorSet(NkDescriptorSetHandle h) {
    auto* e = mDescSets.Get(h.id);
    if (!e) return;
    VkDescriptorSet ds = VKDS(e->set);
    vkFreeDescriptorSets(VKD(mDevice), VKDP(mDescriptorPool), 1, &ds);
    mDescSets.Free(h.id);
}

void NkVulkanDevice::UpdateDescriptorSet(NkDescriptorSetHandle dsh,
                                          const NkDescriptorWrite* writes,
                                          NkU32 count)
{
    auto* dse = mDescSets.Get(dsh.id);
    if (!dse) return;

    std::vector<VkWriteDescriptorSet> vkWrites;
    std::vector<VkDescriptorBufferInfo> bufInfos;
    std::vector<VkDescriptorImageInfo>  imgInfos;
    bufInfos.reserve(count);
    imgInfos.reserve(count);

    for (NkU32 i = 0; i < count; ++i) {
        const auto& w = writes[i];
        VkWriteDescriptorSet vkw{};
        vkw.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vkw.dstSet          = VKDS(dse->set);
        vkw.dstBinding      = w.binding;
        vkw.dstArrayElement = w.arrayIndex;
        vkw.descriptorCount = 1;
        vkw.descriptorType  = ToVkDescType(w.type);

        if (w.type == NkDescriptorType::UniformBuffer ||
            w.type == NkDescriptorType::StorageBuffer) {
            auto* be = mBuffers.Get(w.buffer.id);
            if (!be) continue;
            VkDescriptorBufferInfo bi{};
            bi.buffer = VKBUF(be->buffer);
            bi.offset = w.bufferOffset;
            bi.range  = w.bufferRange > 0 ? w.bufferRange : be->size;
            bufInfos.push_back(bi);
            vkw.pBufferInfo = &bufInfos.back();
        } else {
            auto* te = mTextures.Get(w.texture.id);
            auto* se = mSamplers.Get(w.sampler.id);
            VkDescriptorImageInfo ii{};
            if (te) ii.imageView   = VKIV(te->imageView);
            if (se) ii.sampler     = VKSP(se->sampler);
            ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imgInfos.push_back(ii);
            vkw.pImageInfo = &imgInfos.back();
        }
        vkWrites.push_back(vkw);
    }
    vkUpdateDescriptorSets(VKD(mDevice), (NkU32)vkWrites.size(),
                            vkWrites.data(), 0, nullptr);
}

// =============================================================================
// Ressources — Pipeline
// =============================================================================

NkPipelineHandle NkVulkanDevice::CreatePipeline(const NkPipelineDesc& desc) {
    if (desc.type == NkPipelineType::Compute)
        return CreatePipelineCompute(desc); // voir ci-dessous (simplifié)

    // Shader stages
    std::vector<VkPipelineShaderStageCreateInfo> stages;
    for (NkU32 i = 0; i < desc.shaderCount; ++i) {
        auto* se = mShaders.Get(desc.shaders[i].id);
        if (!se) continue;
        VkPipelineShaderStageCreateInfo si{};
        si.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        si.stage  = (VkShaderStageFlagBits)ToVkStage(se->stage);
        si.module = VKSM(se->module);
        si.pName  = se->entry;
        stages.push_back(si);
    }

    // Vertex input
    std::vector<VkVertexInputAttributeDescription> attrs(desc.attributeCount);
    for (NkU32 i = 0; i < desc.attributeCount; ++i) {
        attrs[i].location = desc.attributes[i].location;
        attrs[i].binding  = desc.attributes[i].binding;
        attrs[i].format   = ToVkVertexFormat(desc.attributes[i].format);
        attrs[i].offset   = desc.attributes[i].offset;
    }
    std::vector<VkVertexInputBindingDescription> binds(desc.bindingCount);
    for (NkU32 i = 0; i < desc.bindingCount; ++i) {
        binds[i].binding   = desc.bindings[i].binding;
        binds[i].stride    = desc.bindings[i].stride;
        binds[i].inputRate = ToVkInputRate(desc.bindings[i].instanced);
    }
    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexAttributeDescriptionCount = (NkU32)attrs.size();
    vi.pVertexAttributeDescriptions    = attrs.data();
    vi.vertexBindingDescriptionCount   = (NkU32)binds.size();
    vi.pVertexBindingDescriptions      = binds.data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Viewport (dynamic)
    VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynState{};
    dynState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynState.dynamicStateCount = 2;
    dynState.pDynamicStates    = dynStates;

    VkPipelineViewportStateCreateInfo vps{};
    vps.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vps.viewportCount = 1;
    vps.scissorCount  = 1;

    // Rasterization
    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = desc.raster.polygonMode == NkPolygonMode::Wireframe
                       ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    rs.cullMode    = desc.raster.cullMode == NkCullMode::None  ? VK_CULL_MODE_NONE
                   : desc.raster.cullMode == NkCullMode::Front ? VK_CULL_MODE_FRONT_BIT
                                                                : VK_CULL_MODE_BACK_BIT;
    rs.frontFace   = desc.raster.frontFace == NkFrontFace::CCW
                       ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
    rs.lineWidth   = 1.f;

    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable  = desc.depthStencil.depthTest  ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable = desc.depthStencil.depthWrite ? VK_TRUE : VK_FALSE;
    ds.depthCompareOp   = ToVkCompare(desc.depthStencil.depthOp);

    // Multisample
    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType               = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples= VK_SAMPLE_COUNT_1_BIT;

    // Blend
    std::vector<VkPipelineColorBlendAttachmentState> blends;
    NkU32 colorCount = (desc.colorCount > 0) ? desc.colorCount : 1;
    for (NkU32 i = 0; i < colorCount; ++i) {
        const auto& b = desc.blends[i];
        VkPipelineColorBlendAttachmentState bs{};
        bs.colorWriteMask = b.writeMask;
        bs.blendEnable    = b.enabled ? VK_TRUE : VK_FALSE;
        if (b.enabled) {
            bs.srcColorBlendFactor = ToVkBlend(b.srcColor);
            bs.dstColorBlendFactor = ToVkBlend(b.dstColor);
            bs.colorBlendOp        = ToVkBlendOp(b.colorOp);
            bs.srcAlphaBlendFactor = ToVkBlend(b.srcAlpha);
            bs.dstAlphaBlendFactor = ToVkBlend(b.dstAlpha);
            bs.alphaBlendOp        = ToVkBlendOp(b.alphaOp);
        }
        blends.push_back(bs);
    }
    VkPipelineColorBlendStateCreateInfo cbs{};
    cbs.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbs.attachmentCount = (NkU32)blends.size();
    cbs.pAttachments    = blends.data();

    // Pipeline layout
    std::vector<VkDescriptorSetLayout> dsls;
    for (NkU32 i = 0; i < desc.layoutCount; ++i) {
        auto* le = mDescLayouts.Get(desc.layouts[i].id);
        if (le) dsls.push_back(VKDSL(le->layout));
    }
    VkPushConstantRange pcRange{};
    pcRange.stageFlags = ToVkStage(desc.pushConstantStages);
    pcRange.offset     = 0;
    pcRange.size       = desc.pushConstantSize;

    VkPipelineLayoutCreateInfo pli{};
    pli.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pli.setLayoutCount = (NkU32)dsls.size();
    pli.pSetLayouts    = dsls.empty() ? nullptr : dsls.data();
    if (desc.pushConstantSize > 0) {
        pli.pushConstantRangeCount = 1;
        pli.pPushConstantRanges   = &pcRange;
    }

    VkPipelineLayout pll;
    if (vkCreatePipelineLayout(VKD(mDevice), &pli, nullptr, &pll) != VK_SUCCESS)
        return NkPipelineHandle::Null();

    // Render pass
    auto* rpe = mRenderPasses.Get(desc.renderPass.id);
    VkRenderPass rp = rpe ? VKRP(rpe->renderPass) : VKRP(mSwapRenderPass);

    VkGraphicsPipelineCreateInfo gci{};
    gci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gci.stageCount          = (NkU32)stages.size();
    gci.pStages             = stages.data();
    gci.pVertexInputState   = &vi;
    gci.pInputAssemblyState = &ia;
    gci.pViewportState      = &vps;
    gci.pRasterizationState = &rs;
    gci.pMultisampleState   = &ms;
    gci.pDepthStencilState  = &ds;
    gci.pColorBlendState    = &cbs;
    gci.pDynamicState       = &dynState;
    gci.layout              = pll;
    gci.renderPass          = rp;
    gci.subpass             = desc.subpass;

    VkPipeline pl;
    if (vkCreateGraphicsPipelines(VKD(mDevice), VK_NULL_HANDLE, 1, &gci, nullptr, &pl)
        != VK_SUCCESS) {
        vkDestroyPipelineLayout(VKD(mDevice), pll, nullptr);
        return NkPipelineHandle::Null();
    }

    NkU32 slot = mPipelines.Alloc();
    auto* pe = mPipelines.Get(slot);
    pe->pipeline = pl;
    pe->layout   = pll;
    pe->type     = desc.type;
    return { slot };
}

NkPipelineHandle NkVulkanDevice::CreatePipelineCompute(const NkPipelineDesc& desc) {
    auto* se = mShaders.Get(desc.computeShader.id);
    if (!se) return NkPipelineHandle::Null();

    VkPipelineShaderStageCreateInfo si{};
    si.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    si.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    si.module = VKSM(se->module);
    si.pName  = se->entry;

    std::vector<VkDescriptorSetLayout> dsls;
    for (NkU32 i = 0; i < desc.layoutCount; ++i) {
        auto* le = mDescLayouts.Get(desc.layouts[i].id);
        if (le) dsls.push_back(VKDSL(le->layout));
    }
    VkPipelineLayoutCreateInfo pli{};
    pli.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pli.setLayoutCount = (NkU32)dsls.size();
    pli.pSetLayouts    = dsls.empty() ? nullptr : dsls.data();

    VkPipelineLayout pll;
    if (vkCreatePipelineLayout(VKD(mDevice), &pli, nullptr, &pll) != VK_SUCCESS)
        return NkPipelineHandle::Null();

    VkComputePipelineCreateInfo cci{};
    cci.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cci.stage  = si;
    cci.layout = pll;

    VkPipeline pl;
    if (vkCreateComputePipelines(VKD(mDevice), VK_NULL_HANDLE, 1, &cci, nullptr, &pl)
        != VK_SUCCESS) {
        vkDestroyPipelineLayout(VKD(mDevice), pll, nullptr);
        return NkPipelineHandle::Null();
    }
    NkU32 slot = mPipelines.Alloc();
    auto* pe = mPipelines.Get(slot);
    pe->pipeline = pl; pe->layout = pll; pe->type = desc.type;
    return { slot };
}

void NkVulkanDevice::DestroyPipeline(NkPipelineHandle h) {
    auto* e = mPipelines.Get(h.id);
    if (!e) return;
    vkDestroyPipeline      (VKD(mDevice), VKPL(e->pipeline), nullptr);
    vkDestroyPipelineLayout(VKD(mDevice), VKPLL(e->layout),  nullptr);
    mPipelines.Free(h.id);
}

// =============================================================================
// Shutdown
// =============================================================================

void NkVulkanDevice::DestroySwapchain() {
    for (NkU32 i = 0; i < mSwapImageCount; ++i) {
        if (mSwapFBs[i])   { vkDestroyFramebuffer(VKD(mDevice), VKFB(mSwapFBs[i]), nullptr); mSwapFBs[i] = nullptr; }
        if (mSwapViews[i]) { vkDestroyImageView  (VKD(mDevice), VKIV(mSwapViews[i]), nullptr); mSwapViews[i] = nullptr; }
    }
    if (mDepthView)    { vkDestroyImageView(VKD(mDevice), VKIV(mDepthView), nullptr); mDepthView = nullptr; }
    if (mDepthImage)   { vkDestroyImage    (VKD(mDevice), VKIMG(mDepthImage), nullptr); mDepthImage = nullptr; }
    if (mDepthMemory)  { vkFreeMemory      (VKD(mDevice), VKMEM(mDepthMemory), nullptr); mDepthMemory = nullptr; }
    if (mSwapRenderPass) { vkDestroyRenderPass(VKD(mDevice), VKRP(mSwapRenderPass), nullptr); mSwapRenderPass = nullptr; }
    if (mSwapchain)    { vkDestroySwapchainKHR(VKD(mDevice), VKSC(mSwapchain), nullptr); mSwapchain = nullptr; }
}

void NkVulkanDevice::Shutdown() {
    WaitIdle();
    for (NkU32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        delete mCmdWrappers[i]; mCmdWrappers[i] = nullptr;
        vkDestroySemaphore(VKD(mDevice), VKSEM(mImageAvailable[i]), nullptr);
        vkDestroySemaphore(VKD(mDevice), VKSEM(mRenderFinished[i]), nullptr);
        vkDestroyFence    (VKD(mDevice), VKFN (mInFlight      [i]), nullptr);
    }
    DestroySwapchain();
    if (mCommandPool)    { vkDestroyCommandPool   (VKD(mDevice), VKCP(mCommandPool),   nullptr); mCommandPool   = nullptr; }
    if (mDescriptorPool) { vkDestroyDescriptorPool(VKD(mDevice), VKDP(mDescriptorPool),nullptr); mDescriptorPool= nullptr; }
    if (mDevice)         { vkDestroyDevice(VKD(mDevice), nullptr); mDevice = nullptr; }
    if (mSurface)        { vkDestroySurfaceKHR(VKI(mInstance), VKS(mSurface), nullptr); mSurface = nullptr; }
    if (mInstance)       { vkDestroyInstance(VKI(mInstance), nullptr); mInstance = nullptr; }
    mInitialized = false;
}

// Helpers divers
NkRHICommandBufferPtr NkVulkanDevice::CreateCommandBuffer(bool secondary) {
    VkCommandBufferAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool        = VKCP(mCommandPool);
    ai.level              = secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY
                                      : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cb;
    if (vkAllocateCommandBuffers(VKD(mDevice), &ai, &cb) != VK_SUCCESS)
        return nullptr;
    auto wrapper = std::make_unique<NkVulkanCommandBuffer>(cb, mDevice);
    wrapper->mBuffers = &mBuffers; wrapper->mTextures = &mTextures;
    wrapper->mPipelines = &mPipelines; wrapper->mDescSets = &mDescSets;
    wrapper->mDescLayouts = &mDescLayouts; wrapper->mRenderPasses = &mRenderPasses;
    wrapper->mFramebuffers = &mFramebuffers;
    return wrapper;
}

NkRHICommandBuffer& NkVulkanDevice::GetCurrentCommandBuffer() {
    return *mCmdWrappers[mCurrentFrame];
}

NkTextureHandle    NkVulkanDevice::GetSwapchainTexture(NkU32 i) { return mSwapTextureHandles[i]; }
NkU32              NkVulkanDevice::GetSwapchainImageCount() const { return mSwapImageCount; }
NkU32              NkVulkanDevice::GetCurrentSwapchainIndex() const { return mCurrentImage; }
NkRenderPassHandle NkVulkanDevice::GetSwapchainRenderPass() { return mSwapRenderPassHandle; }
NkFramebufferHandle NkVulkanDevice::GetCurrentFramebuffer() { return mSwapFBHandles[mCurrentImage]; }

// single-time commands
void* NkVulkanDevice::BeginSingleTimeCommands() {
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandPool = VKCP(mCommandPool);
    ai.commandBufferCount = 1;
    VkCommandBuffer cb;
    vkAllocateCommandBuffers(VKD(mDevice), &ai, &cb);
    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);
    return cb;
}

void NkVulkanDevice::EndSingleTimeCommands(void* cmd) {
    VkCommandBuffer cb = VKCB(cmd);
    vkEndCommandBuffer(cb);
    VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.commandBufferCount = 1; si.pCommandBuffers = &cb;
    vkQueueSubmit(VKQQ(mGraphicsQueue), 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(VKQQ(mGraphicsQueue));
    vkFreeCommandBuffers(VKD(mDevice), VKCP(mCommandPool), 1, &cb);
}

void NkVulkanDevice::CopyBufferImmediate(void* src, void* dst, NkU64 size) {
    void* cmd = BeginSingleTimeCommands();
    VkBufferCopy region{ 0, 0, size };
    vkCmdCopyBuffer(VKCB(cmd), VKBUF(src), VKBUF(dst), 1, &region);
    EndSingleTimeCommands(cmd);
}

// =============================================================================
// NkVulkanCommandBuffer
// =============================================================================

NkVulkanCommandBuffer::NkVulkanCommandBuffer(void* cmd, void* dev)
    : mCmdBuf(cmd), mDevice(dev) {}

void NkVulkanCommandBuffer::Begin() {
    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(VKCB(mCmdBuf), &bi);
}

void NkVulkanCommandBuffer::End() {
    vkEndCommandBuffer(VKCB(mCmdBuf));
}

void NkVulkanCommandBuffer::BeginRenderPass(
    NkRenderPassHandle rph, NkFramebufferHandle fbh,
    const NkClearValue* clears, NkU32 clearCount)
{
    auto* rpe = mRenderPasses->Get(rph.id);
    auto* fbe = mFramebuffers->Get(fbh.id);
    if (!rpe || !fbe) return;

    std::vector<VkClearValue> vkClears(clearCount);
    for (NkU32 i = 0; i < clearCount; ++i) {
        vkClears[i].color = {{ clears[i].r, clears[i].g, clears[i].b, clears[i].a }};
        vkClears[i].depthStencil = { clears[i].depth, clears[i].stencil };
    }

    VkRenderPassBeginInfo bi{};
    bi.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    bi.renderPass        = VKRP(rpe->renderPass);
    bi.framebuffer       = VKFB(fbe->framebuffer);
    bi.renderArea.extent = { fbe->width, fbe->height };
    bi.clearValueCount   = clearCount;
    bi.pClearValues      = vkClears.data();
    vkCmdBeginRenderPass(VKCB(mCmdBuf), &bi, VK_SUBPASS_CONTENTS_INLINE);
}

void NkVulkanCommandBuffer::EndRenderPass() { vkCmdEndRenderPass(VKCB(mCmdBuf)); }
void NkVulkanCommandBuffer::NextSubpass()   { vkCmdNextSubpass(VKCB(mCmdBuf), VK_SUBPASS_CONTENTS_INLINE); }

void NkVulkanCommandBuffer::BindPipeline(NkPipelineHandle h) {
    auto* e = mPipelines->Get(h.id);
    if (!e) return;
    VkPipelineBindPoint bp = e->type == NkPipelineType::Compute
        ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
    vkCmdBindPipeline(VKCB(mCmdBuf), bp, VKPL(e->pipeline));
}

void NkVulkanCommandBuffer::SetViewport(const NkViewport& vp) {
    VkViewport v{ vp.x, vp.y, vp.width, vp.height, vp.minDepth, vp.maxDepth };
    vkCmdSetViewport(VKCB(mCmdBuf), 0, 1, &v);
}

void NkVulkanCommandBuffer::SetScissor(const NkScissor& sc) {
    VkRect2D r{ {sc.x, sc.y}, {sc.width, sc.height} };
    vkCmdSetScissor(VKCB(mCmdBuf), 0, 1, &r);
}

void NkVulkanCommandBuffer::BindVertexBuffer(NkU32 binding, NkBufferHandle h, NkU64 offset) {
    auto* e = mBuffers->Get(h.id);
    if (!e) return;
    VkBuffer buf = VKBUF(e->buffer);
    vkCmdBindVertexBuffers(VKCB(mCmdBuf), binding, 1, &buf, &offset);
}

void NkVulkanCommandBuffer::BindIndexBuffer(NkBufferHandle h, NkIndexType type, NkU64 offset) {
    auto* e = mBuffers->Get(h.id);
    if (!e) return;
    vkCmdBindIndexBuffer(VKCB(mCmdBuf), VKBUF(e->buffer), offset,
        type == NkIndexType::Uint16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
}

void NkVulkanCommandBuffer::BindDescriptorSet(NkU32 setIdx, NkDescriptorSetHandle dsh,
                                               NkPipelineHandle plh)
{
    auto* dse = mDescSets->Get(dsh.id);
    auto* ple = mPipelines->Get(plh.id);
    if (!dse || !ple) return;
    VkDescriptorSet ds = VKDS(dse->set);
    VkPipelineBindPoint bp = ple->type == NkPipelineType::Compute
        ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
    vkCmdBindDescriptorSets(VKCB(mCmdBuf), bp, VKPLL(ple->layout),
                             setIdx, 1, &ds, 0, nullptr);
}

void NkVulkanCommandBuffer::PushConstants(NkPipelineHandle plh, NkShaderStage stages,
                                           NkU32 offset, NkU32 size, const void* data)
{
    auto* ple = mPipelines->Get(plh.id);
    if (!ple) return;
    vkCmdPushConstants(VKCB(mCmdBuf), VKPLL(ple->layout),
                        ToVkStage(stages), offset, size, data);
}

void NkVulkanCommandBuffer::Draw(const NkDrawCall& cmd) {
    vkCmdDraw(VKCB(mCmdBuf), cmd.vertexCount, cmd.instanceCount,
               cmd.firstVertex, cmd.firstInstance);
}

void NkVulkanCommandBuffer::DrawIndexed(const NkDrawIndexedCall& cmd) {
    vkCmdDrawIndexed(VKCB(mCmdBuf), cmd.indexCount, cmd.instanceCount,
                      cmd.firstIndex, cmd.vertexOffset, cmd.firstInstance);
}

void NkVulkanCommandBuffer::DrawIndirect(NkBufferHandle h, NkU64 off, NkU32 count) {
    auto* e = mBuffers->Get(h.id);
    if (!e) return;
    vkCmdDrawIndirect(VKCB(mCmdBuf), VKBUF(e->buffer), off, count,
                       sizeof(VkDrawIndirectCommand));
}

void NkVulkanCommandBuffer::Dispatch(NkU32 x, NkU32 y, NkU32 z) {
    vkCmdDispatch(VKCB(mCmdBuf), x, y, z);
}

void NkVulkanCommandBuffer::CopyBuffer(NkBufferHandle s, NkBufferHandle d,
                                        NkU64 so, NkU64 doff, NkU64 sz)
{
    auto* se = mBuffers->Get(s.id);
    auto* de = mBuffers->Get(d.id);
    if (!se || !de) return;
    VkBufferCopy r{ so, doff, sz };
    vkCmdCopyBuffer(VKCB(mCmdBuf), VKBUF(se->buffer), VKBUF(de->buffer), 1, &r);
}

void NkVulkanCommandBuffer::CopyTexture(NkTextureHandle s, NkTextureHandle d) {
    // Implémentation simplifiée — copie mip 0, layer 0
    auto* se = mTextures->Get(s.id);
    auto* de = mTextures->Get(d.id);
    if (!se || !de) return;
    VkImageCopy r{};
    r.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    r.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    r.extent         = { se->width, se->height, 1 };
    vkCmdCopyImage(VKCB(mCmdBuf),
                   VKIMG(se->image), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   VKIMG(de->image), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &r);
}

void NkVulkanCommandBuffer::UploadBuffer(NkBufferHandle, const void*, NkU64, NkU64) {
    // Délégué au device via staging — no-op ici si déjà géré dans CreateBuffer
}

void NkVulkanCommandBuffer::UploadTexture(NkTextureHandle, const void*,
                                           NkU32, NkU32, NkU32, NkU32, NkU32, NkU32) {
    // Idem — utiliser NkRHIDevice::CreateTexture avec staging
}

void NkVulkanCommandBuffer::TextureBarrier(NkTextureHandle h,
                                            bool fromWrite, bool toRead)
{
    auto* e = mTextures->Get(h.id);
    if (!e) return;
    VkImageMemoryBarrier b{};
    b.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.image               = VKIMG(e->image);
    b.oldLayout           = fromWrite ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                      : VK_IMAGE_LAYOUT_UNDEFINED;
    b.newLayout           = toRead    ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                      : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    b.srcAccessMask       = fromWrite ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0;
    b.dstAccessMask       = toRead    ? VK_ACCESS_SHADER_READ_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    b.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(VKCB(mCmdBuf),
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &b);
}

void NkVulkanCommandBuffer::BufferBarrier(NkBufferHandle h,
                                           bool fromWrite, bool toRead)
{
    auto* e = mBuffers->Get(h.id);
    if (!e) return;
    VkBufferMemoryBarrier b{};
    b.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    b.buffer              = VKBUF(e->buffer);
    b.offset              = 0; b.size = VK_WHOLE_SIZE;
    b.srcAccessMask       = fromWrite ? VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_TRANSFER_WRITE_BIT;
    b.dstAccessMask       = toRead    ? VK_ACCESS_SHADER_READ_BIT  : VK_ACCESS_TRANSFER_READ_BIT;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkCmdPipelineBarrier(VKCB(mCmdBuf),
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT  | VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 1, &b, 0, nullptr);
}

// =============================================================================
// Factory
// =============================================================================

NkRHIDevicePtr NkRHIDeviceFactory::Create(const NkSurfaceDesc& surface,
                                            const NkGraphicsContextConfig& config)
{
    NkGraphicsApi api = config.api;
    if (api == NkGraphicsApi::Auto)
        api = NkGraphicsApi::Vulkan; // défaut

    if (api == NkGraphicsApi::Vulkan) {
        auto dev = std::make_unique<NkVulkanDevice>();
        if (!dev->Init(surface, config)) return nullptr;
        return dev;
    }
    return nullptr; // autres backends : ajouter ici
}

}} // namespace nkentseu::rhi
