// =============================================================================
// NkRHI_Device_VK.cpp — Backend Vulkan autonome avec NkSurfaceDesc
// =============================================================================
#ifdef NK_RHI_VK_ENABLED
#include "NkVulkanDevice.h"
#include "NkCommandBuffer_VK.h"
#include <cstring>
#include <algorithm>
#include <set>

#define NK_VK_LOG(...)  logger.Infof("[NkRHI_VK] " __VA_ARGS__)
#define NK_VK_ERR(...)  logger.Errorf("[NkRHI_VK][ERR] " __VA_ARGS__)
#define NK_VK_CHECK(r)  do { VkResult _r=(r); if(_r!=VK_SUCCESS) { NK_VK_ERR("VkResult=%d at %s:%d\n",(int)_r,__FILE__,__LINE__); } } while(0)
#define NK_VK_CHECKRET(r, msg) do { VkResult _r=(r); if(_r!=VK_SUCCESS){ NK_VK_ERR(msg " (err=%d)\n",(int)_r); return false; } } while(0)

namespace nkentseu {

// =============================================================================
NkVulkanDevice::~NkVulkanDevice() { if (mIsValid) Shutdown(); }

// =============================================================================
bool NkVulkanDevice::Initialize(const NkSurfaceDesc& surfaceDesc) {
    if (mIsValid) { NK_VK_ERR("Already initialized\n"); return false; }
    
    if (!surfaceDesc.IsValid()) {
        NK_VK_ERR("Invalid surface description\n");
        return false;
    }
    
    mSurfaceDesc = surfaceDesc;
    mWidth = surfaceDesc.width;
    mHeight = surfaceDesc.height;

    // Création de l'instance Vulkan
    if (!CreateInstance()) return false;
    
    // Création du debug messenger (optionnel)
    CreateDebugMessenger();
    
    // Création de la surface à partir de NkSurfaceDesc
    if (!CreateSurface(surfaceDesc)) return false;
    
    // Sélection du GPU
    if (!SelectPhysicalDevice()) return false;
    
    // Création du device logique
    if (!CreateLogicalDevice()) return false;
    
    // Création de la swapchain
    if (!CreateSwapchain(mWidth, mHeight)) return false;
    
    // Création du render pass
    if (!CreateRenderPass()) return false;
    
    // Création des ressources de profondeur
    if (!CreateDepthResources(mWidth, mHeight)) return false;
    
    // Création des framebuffers
    if (!CreateFramebuffers()) return false;
    
    // Création des command pools
    if (!CreateCommandPool()) return false;
    
    // Création des command buffers
    if (!CreateCommandBuffers()) return false;
    
    // Création des objets de synchronisation
    if (!CreateSyncObjects()) return false;
    
    // Création du descriptor pool global
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,          1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,  256},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,          1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,  256},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,           1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,           256},
        {VK_DESCRIPTOR_TYPE_SAMPLER,                 256},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,        64},
    };
    VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    dpci.maxSets       = 4096;
    dpci.poolSizeCount = (uint32)std::size(poolSizes);
    dpci.pPoolSizes    = poolSizes;
    NK_VK_CHECKRET(vkCreateDescriptorPool(mDevice, &dpci, nullptr, &mDescPool),
                   "vkCreateDescriptorPool");

    // Command pool one-shot
    VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpci.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    cpci.queueFamilyIndex = mGraphicsFamily;
    NK_VK_CHECKRET(vkCreateCommandPool(mDevice, &cpci, nullptr, &mOneShotPool),
                   "vkCreateCommandPool (one-shot)");

    // Initialiser les fences des images
    for (uint32 i = 0; i < kMaxImages; ++i)
        mImagesInFlight[i] = VK_NULL_HANDLE;

    QueryCaps();
    mIsValid = true;
    
    NK_VK_LOG("Device initialized with surface (%ux%u, %u frames in flight)\n", 
              mWidth, mHeight, kMaxFrames);
    return true;
}

// =============================================================================
void NkVulkanDevice::Shutdown() {
    WaitIdle();
    DestroySwapchainDependents();

    // Destruction de toutes les ressources
    for (auto& [id,b] : mBuffers) {
        vkDestroyBuffer(mDevice, b.buffer, nullptr);
        FreeMemory(b.alloc);
    }
    mBuffers.Clear();

    for (auto& [id,t] : mTextures) if (!t.isSwapchain) {
        vkDestroyImageView(mDevice, t.view, nullptr);
        vkDestroyImage(mDevice, t.image, nullptr);
        FreeMemory(t.alloc);
    }
    mTextures.Clear();

    for (auto& [id,s] : mSamplers) {
        vkDestroySampler(mDevice, s.sampler, nullptr);
    }
    mSamplers.Clear();

    for (auto& [id,sh] : mShaders) {
        for (auto& st : sh.stages) {
            vkDestroyShaderModule(mDevice, st.module, nullptr);
        }
    }
    mShaders.Clear();

    for (auto& [id,p] : mPipelines) {
        vkDestroyPipeline(mDevice, p.pipeline, nullptr);
        vkDestroyPipelineLayout(mDevice, p.layout, nullptr);
    }
    mPipelines.Clear();

    for (auto& [id,rp] : mRenderPasses) {
        vkDestroyRenderPass(mDevice, rp.renderPass, nullptr);
    }
    mRenderPasses.Clear();

    for (auto& [id,fb] : mFramebuffers) {
        vkDestroyFramebuffer(mDevice, fb.framebuffer, nullptr);
    }
    mFramebuffers.Clear();

    for (auto& [id,dl] : mDescLayouts) {
        vkDestroyDescriptorSetLayout(mDevice, dl.layout, nullptr);
    }
    mDescLayouts.Clear();

    for (auto& [id,f] : mFences) {
        vkDestroyFence(mDevice, f.fence, nullptr);
    }
    mFences.Clear();

    // Destruction des objets de frame
    for (uint32 i = 0; i < kMaxFrames; i++) {
        if (mFrames[i].inFlightFence) 
            vkDestroyFence(mDevice, mFrames[i].inFlightFence, nullptr);
        if (mFrames[i].imageAvailable)
            vkDestroySemaphore(mDevice, mFrames[i].imageAvailable, nullptr);
        if (mFrames[i].renderFinished)
            vkDestroySemaphore(mDevice, mFrames[i].renderFinished, nullptr);
        if (mFrames[i].cmdPool)
            vkDestroyCommandPool(mDevice, mFrames[i].cmdPool, nullptr);
    }

    if (mDescPool) vkDestroyDescriptorPool(mDevice, mDescPool, nullptr);
    if (mOneShotPool) vkDestroyCommandPool(mDevice, mOneShotPool, nullptr);
    if (mGraphicsCmdPool) vkDestroyCommandPool(mDevice, mGraphicsCmdPool, nullptr);
    if (mComputeCmdPool && mComputeCmdPool != mGraphicsCmdPool)
        vkDestroyCommandPool(mDevice, mComputeCmdPool, nullptr);

    // Destruction des ressources de profondeur
    if (mDepthView) vkDestroyImageView(mDevice, mDepthView, nullptr);
    if (mDepthImage) vkDestroyImage(mDevice, mDepthImage, nullptr);
    if (mDepthMemory) vkFreeMemory(mDevice, mDepthMemory, nullptr);

    // Destruction des ressources swapchain
    for (auto& v : mSwapViews) {
        if (v) vkDestroyImageView(mDevice, v, nullptr);
    }
    mSwapViews.Clear();
    mSwapImages.Clear();
    
    if (mSwapchain) vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
    if (mSurface) vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
    if (mDevice) vkDestroyDevice(mDevice, nullptr);
    if (mDebugMessenger) {
        auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(mInstance, "vkDestroyDebugUtilsMessengerEXT");
        if (fn) fn(mInstance, mDebugMessenger, nullptr);
    }
    if (mInstance) vkDestroyInstance(mInstance, nullptr);

    mIsValid = false;
    NK_VK_LOG("Shutdown complete\n");
}

// =============================================================================
// Méthodes de création Vulkan
// =============================================================================

bool NkVulkanDevice::CreateInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "NkEngine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "NkEngine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    NkVector<const char*> extensions;
    extensions.PushBack(VK_KHR_SURFACE_EXTENSION_NAME);
    
    // Extensions de surface selon la plateforme
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
    
    // Debug extensions
    extensions.PushBack(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    NkVector<const char*> layers;
    layers.PushBack("VK_LAYER_KHRONOS_validation");

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

    NK_VK_CHECKRET(vkCreateInstance(&info, nullptr, &mInstance), "vkCreateInstance");
    NK_VK_LOG("Instance created\n");
    return true;
}

// =============================================================================
VKAPI_ATTR VkBool32 VKAPI_CALL NkVulkanDevice::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* /*user*/)
{
    const char* sev = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)   ? "ERROR"   :
                      (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ? "WARNING" :
                      (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)    ? "INFO"    : "VERBOSE";
    
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        logger.Errorf("[Vulkan][%s] %s", sev, data->pMessage);
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        logger.Warnf("[Vulkan][%s] %s", sev, data->pMessage);
    } else {
        logger.Infof("[Vulkan][%s] %s", sev, data->pMessage);
    }
    return VK_FALSE;
}

bool NkVulkanDevice::CreateDebugMessenger() {
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

    auto fn = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(mInstance, "vkCreateDebugUtilsMessengerEXT");
    if (!fn) { NK_VK_ERR("vkCreateDebugUtilsMessengerEXT not found\n"); return false; }
    
    NK_VK_CHECKRET(fn(mInstance, &info, nullptr, &mDebugMessenger), 
                   "vkCreateDebugUtilsMessengerEXT");
    return true;
}

// =============================================================================
bool NkVulkanDevice::CreateSurface(const NkSurfaceDesc& surfDesc) {
    
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        VkWin32SurfaceCreateInfoKHR ci{};
        ci.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        ci.hwnd      = static_cast<HWND>(surfDesc.hwnd);
        ci.hinstance = static_cast<HINSTANCE>(surfDesc.hinstance);
        NK_VK_CHECKRET(vkCreateWin32SurfaceKHR(mInstance, &ci, nullptr, &mSurface), 
                       "vkCreateWin32SurfaceKHR");
    
    #elif defined(NKENTSEU_WINDOWING_XLIB)
        VkXlibSurfaceCreateInfoKHR ci{};
        ci.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        ci.dpy    = static_cast<Display*>(surfDesc.display);
        ci.window = static_cast<::Window>(surfDesc.window);
        NK_VK_CHECKRET(vkCreateXlibSurfaceKHR(mInstance, &ci, nullptr, &mSurface),
                       "vkCreateXlibSurfaceKHR");
    
    #elif defined(NKENTSEU_WINDOWING_XCB)
        VkXcbSurfaceCreateInfoKHR ci{};
        ci.sType      = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
        ci.connection = static_cast<xcb_connection_t*>(surfDesc.connection);
        ci.window     = static_cast<xcb_window_t>(surfDesc.window);
        NK_VK_CHECKRET(vkCreateXcbSurfaceKHR(mInstance, &ci, nullptr, &mSurface),
                       "vkCreateXcbSurfaceKHR");
    
    #elif defined(NKENTSEU_WINDOWING_WAYLAND)
        VkWaylandSurfaceCreateInfoKHR ci{};
        ci.sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        ci.display = static_cast<wl_display*>(surfDesc.display);
        ci.surface = static_cast<wl_surface*>(surfDesc.surface);
        NK_VK_CHECKRET(vkCreateWaylandSurfaceKHR(mInstance, &ci, nullptr, &mSurface),
                       "vkCreateWaylandSurfaceKHR");
    
    #elif defined(NKENTSEU_PLATFORM_ANDROID)
        VkAndroidSurfaceCreateInfoKHR ci{};
        ci.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        ci.window = static_cast<ANativeWindow*>(surfDesc.nativeWindow);
        NK_VK_CHECKRET(vkCreateAndroidSurfaceKHR(mInstance, &ci, nullptr, &mSurface),
                       "vkCreateAndroidSurfaceKHR");
    
    #elif defined(NKENTSEU_PLATFORM_MACOS)
        VkMetalSurfaceCreateInfoEXT ci{};
        ci.sType  = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
        ci.pLayer = static_cast<const CAMetalLayer*>(surfDesc.metalLayer);
        NK_VK_CHECKRET(vkCreateMetalSurfaceEXT(mInstance, &ci, nullptr, &mSurface),
                       "vkCreateMetalSurfaceEXT");
    
    #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        // Sur Emscripten, la surface est créée via HTML canvas
        // Cette implémentation nécessite une extension spécifique
        NK_VK_ERR("Emscripten surface creation not implemented\n");
        return false;
    
    #else
        NK_VK_ERR("No Vulkan surface implementation for this platform\n");
        return false;
    #endif
    
    NK_VK_LOG("Surface created\n");
    return true;
}

// =============================================================================
void NkVulkanDevice::FindQueueFamilies(VkPhysicalDevice dev) {
    uint32 count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    NkVector<VkQueueFamilyProperties> props(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, props.Data());

    mGraphicsFamily = UINT32_MAX;
    uint32 presentFamily = UINT32_MAX;
    mComputeFamily = UINT32_MAX;
    mTransferFamily = UINT32_MAX;

    for (uint32 i = 0; i < count; ++i) {
        // Graphics
        if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            mGraphicsFamily = i;
        
        // Compute dédié (pas graphics)
        if ((props[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            !(props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            mComputeFamily = i;
        
        // Transfer dédié (pas graphics ni compute)
        if ((props[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
            !(props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            !(props[i].queueFlags & VK_QUEUE_COMPUTE_BIT))
            mTransferFamily = i;
        
        // Present support
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, mSurface, &present);
        if (present) presentFamily = i;
    }
    
    // Fallbacks
    if (mComputeFamily == UINT32_MAX && mGraphicsFamily != UINT32_MAX)
        mComputeFamily = mGraphicsFamily;
    
    if (mTransferFamily == UINT32_MAX && mGraphicsFamily != UINT32_MAX)
        mTransferFamily = mGraphicsFamily;
    
    // Present doit être disponible
    if (presentFamily != UINT32_MAX)
        mGraphicsFamily = presentFamily;
}

bool NkVulkanDevice::CheckDeviceExtensions(VkPhysicalDevice dev) const {
    uint32 count = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, nullptr);
    NkVector<VkExtensionProperties> avail(count);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, avail.Data());
    
    bool hasSwapchain = false;
    for (auto& ext : avail) {
        if (strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
            hasSwapchain = true;
    }
    return hasSwapchain;
}

bool NkVulkanDevice::IsDeviceSuitable(VkPhysicalDevice dev) const {
    FindQueueFamilies(dev);
    if (mGraphicsFamily == UINT32_MAX) return false;
    if (!CheckDeviceExtensions(dev)) return false;
    
    uint32 fmtCount = 0, modeCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, mSurface, &fmtCount, nullptr);
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, mSurface, &modeCount, nullptr);
    
    return fmtCount > 0 && modeCount > 0;
}

bool NkVulkanDevice::SelectPhysicalDevice() {
    uint32 count = 0;
    vkEnumeratePhysicalDevices(mInstance, &count, nullptr);
    if (count == 0) { NK_VK_ERR("No Vulkan physical device\n"); return false; }
    
    NkVector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(mInstance, &count, devices.Data());

    // Sélection du meilleur GPU (préfère discrete)
    int bestScore = -1;
    VkPhysicalDevice selected = VK_NULL_HANDLE;
    
    for (uint32 i = 0; i < devices.Size(); ++i) {
        VkPhysicalDevice dev = devices[i];
        if (!IsDeviceSuitable(dev)) continue;
        
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        
        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 500;
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) score += 200;
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) score += 50;
        
        if (score > bestScore) {
            bestScore = score;
            selected = dev;
        }
    }
    
    if (selected == VK_NULL_HANDLE) {
        NK_VK_ERR("No suitable Vulkan device found\n");
        return false;
    }
    
    mPhysicalDevice = selected;
    vkGetPhysicalDeviceProperties(mPhysicalDevice, &mDeviceProps);
    
    NK_VK_LOG("Selected GPU: %s\n", mDeviceProps.deviceName);
    return true;
}

// =============================================================================
bool NkVulkanDevice::CreateLogicalDevice() {
    FindQueueFamilies(mPhysicalDevice);
    
    // Queues uniques
    uint32 uniqueFamilies[3] = {mGraphicsFamily};
    uint32 uniqueCount = 1;
    
    if (mComputeFamily != mGraphicsFamily && mComputeFamily != UINT32_MAX) {
        uniqueFamilies[uniqueCount++] = mComputeFamily;
    }
    if (mTransferFamily != mGraphicsFamily && mTransferFamily != mComputeFamily && 
        mTransferFamily != UINT32_MAX) {
        uniqueFamilies[uniqueCount++] = mTransferFamily;
    }
    
    float priority = 1.0f;
    NkVector<VkDeviceQueueCreateInfo> queueInfos;
    
    for (uint32 i = 0; i < uniqueCount; ++i) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = uniqueFamilies[i];
        qi.queueCount       = 1;
        qi.pQueuePriorities = &priority;
        queueInfos.PushBack(qi);
    }

    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = VK_TRUE;
    features.fillModeNonSolid = VK_TRUE;
    features.wideLines = VK_TRUE;
    features.independentBlend = VK_TRUE;

    NkVector<const char*> extensions;
    extensions.PushBack(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    #if defined(NKENTSEU_PLATFORM_MACOS)
        extensions.PushBack("VK_KHR_portability_subset");
    #endif

    VkDeviceCreateInfo info{};
    info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount    = (uint32)queueInfos.Size();
    info.pQueueCreateInfos       = queueInfos.Data();
    info.enabledExtensionCount   = (uint32)extensions.Size();
    info.ppEnabledExtensionNames = extensions.Data();
    info.pEnabledFeatures        = &features;

    NK_VK_CHECKRET(vkCreateDevice(mPhysicalDevice, &info, nullptr, &mDevice), 
                   "vkCreateDevice");

    vkGetDeviceQueue(mDevice, mGraphicsFamily, 0, &mGraphicsQueue);
    if (mComputeFamily != UINT32_MAX)
        vkGetDeviceQueue(mDevice, mComputeFamily, 0, &mComputeQueue);
    if (mTransferFamily != UINT32_MAX)
        vkGetDeviceQueue(mDevice, mTransferFamily, 0, &mTransferQueue);
    else
        mTransferQueue = mGraphicsQueue;

    NK_VK_LOG("Logical device created\n");
    return true;
}

// =============================================================================
bool NkVulkanDevice::CreateSwapchain(uint32 w, uint32 h) {
    // Capacités
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &caps);

    // Format
    uint32 fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &fmtCount, nullptr);
    NkVector<VkSurfaceFormatKHR> fmts(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &fmtCount, fmts.Data());

    VkSurfaceFormatKHR fmt = fmts[0];
    for (auto& f : fmts) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            fmt = f;
            break;
        }
    }
    mSwapFormat = fmt.format;
    mSwapColorSpace = fmt.colorSpace;

    // Present mode
    uint32 modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &modeCount, nullptr);
    NkVector<VkPresentModeKHR> modes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &modeCount, modes.Data());

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (!mVSync) {
        for (auto m : modes) {
            if (m == VK_PRESENT_MODE_MAILBOX_KHR) { presentMode = m; break; }
        }
        if (presentMode == VK_PRESENT_MODE_FIFO_KHR) {
            for (auto m : modes) {
                if (m == VK_PRESENT_MODE_IMMEDIATE_KHR) { presentMode = m; break; }
            }
        }
    }

    // Extent
    mSwapExtent = {w, h};
    if (caps.currentExtent.width != UINT32_MAX &&
        caps.currentExtent.width != 0 && caps.currentExtent.height != 0) {
        mSwapExtent = caps.currentExtent;
    } else {
        mSwapExtent.width = std::clamp(w, caps.minImageExtent.width, caps.maxImageExtent.width);
        mSwapExtent.height = std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    if (mSwapExtent.width == 0 || mSwapExtent.height == 0) {
        mMinimized = true;
        return true;
    }

    uint32 imgCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imgCount > caps.maxImageCount)
        imgCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR ci{};
    ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface          = mSurface;
    ci.minImageCount    = imgCount;
    ci.imageFormat      = fmt.format;
    ci.imageColorSpace  = fmt.colorSpace;
    ci.imageExtent      = mSwapExtent;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32 families[] = {mGraphicsFamily};
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.queueFamilyIndexCount = 1;
    ci.pQueueFamilyIndices = families;

    ci.preTransform   = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode    = presentMode;
    ci.clipped        = VK_TRUE;

    NK_VK_CHECKRET(vkCreateSwapchainKHR(mDevice, &ci, nullptr, &mSwapchain),
                   "vkCreateSwapchainKHR");

    // Récupérer les images
    uint32 cnt = 0;
    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &cnt, nullptr);
    mSwapImages.Resize(cnt);
    mSwapViews.Resize(cnt);
    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &cnt, mSwapImages.Data());

    // Créer les image views
    for (uint32 i = 0; i < cnt; ++i) {
        VkImageViewCreateInfo ivci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ivci.image    = mSwapImages[i];
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format   = mSwapFormat;
        ivci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        NK_VK_CHECK(vkCreateImageView(mDevice, &ivci, nullptr, &mSwapViews[i]));
    }

    NK_VK_LOG("Swapchain created (%ux%u, %u images)\n", 
              mSwapExtent.width, mSwapExtent.height, cnt);
    return true;
}

// =============================================================================
bool NkVulkanDevice::CreateRenderPass() {
    VkAttachmentDescription colorAtt{};
    colorAtt.format         = mSwapFormat;
    colorAtt.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAtt.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAtt.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info{};
    info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments    = &colorAtt;
    info.subpassCount    = 1;
    info.pSubpasses      = &subpass;
    info.dependencyCount = 1;
    info.pDependencies   = &dep;

    VkRenderPass rp = VK_NULL_HANDLE;
    NK_VK_CHECKRET(vkCreateRenderPass(mDevice, &info, nullptr, &rp),
                   "vkCreateRenderPass");

    NkRenderPassDesc rpDesc;
    mSwapchainRP.id = NextId();
    mRenderPasses[mSwapchainRP.id] = {rp, rpDesc};
    
    return true;
}

// =============================================================================
VkFormat NkVulkanDevice::FindDepthFormat() const {
    VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT
    };
    
    for (auto f : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, f, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            return f;
    }
    return VK_FORMAT_D32_SFLOAT;
}

bool NkVulkanDevice::FindMemoryType(uint32 filter, VkMemoryPropertyFlags props, uint32& out) const {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memProps);
    
    for (uint32 i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((filter & (1 << i)) && 
            (memProps.memoryTypes[i].propertyFlags & props) == props) {
            out = i;
            return true;
        }
    }
    return false;
}

bool NkVulkanDevice::CreateDepthResources(uint32 w, uint32 h) {
    VkFormat depthFmt = FindDepthFormat();

    VkImageCreateInfo imgInfo{};
    imgInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType     = VK_IMAGE_TYPE_2D;
    imgInfo.format        = depthFmt;
    imgInfo.extent        = {w, h, 1};
    imgInfo.mipLevels     = 1;
    imgInfo.arrayLayers   = 1;
    imgInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    
    NK_VK_CHECKRET(vkCreateImage(mDevice, &imgInfo, nullptr, &mDepthImage),
                   "vkCreateImage (depth)");

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(mDevice, mDepthImage, &req);
    
    uint32 memType = 0;
    if (!FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memType))
        return false;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = req.size;
    allocInfo.memoryTypeIndex = memType;
    
    NK_VK_CHECKRET(vkAllocateMemory(mDevice, &allocInfo, nullptr, &mDepthMemory),
                   "vkAllocateMemory (depth)");
    
    vkBindImageMemory(mDevice, mDepthImage, mDepthMemory, 0);

    VkImageViewCreateInfo ivInfo{};
    ivInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivInfo.image                           = mDepthImage;
    ivInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    ivInfo.format                          = depthFmt;
    ivInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    ivInfo.subresourceRange.baseMipLevel   = 0;
    ivInfo.subresourceRange.levelCount     = 1;
    ivInfo.subresourceRange.baseArrayLayer = 0;
    ivInfo.subresourceRange.layerCount     = 1;
    
    NK_VK_CHECKRET(vkCreateImageView(mDevice, &ivInfo, nullptr, &mDepthView),
                   "vkCreateImageView (depth)");
    
    return true;
}

bool NkVulkanDevice::CreateFramebuffers() {
    auto rpit = mRenderPasses.find(mSwapchainRP.id);
    if (rpit == mRenderPasses.end()) return false;
    
    uint32 n = (uint32)mSwapImages.Size();
    mSwapchainFBs.Resize(n);
    
    for (uint32 i = 0; i < n; ++i) {
        VkImageView attachments[] = {mSwapViews[i], mDepthView};
        
        VkFramebufferCreateInfo info{};
        info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass      = rpit->second.renderPass;
        info.attachmentCount = 2;
        info.pAttachments    = attachments;
        info.width           = mSwapExtent.width;
        info.height          = mSwapExtent.height;
        info.layers          = 1;
        
        VkFramebuffer fb = VK_NULL_HANDLE;
        NK_VK_CHECK(vkCreateFramebuffer(mDevice, &info, nullptr, &fb));
        
        uint64 hid = NextId();
        mFramebuffers[hid] = {fb, mSwapExtent.width, mSwapExtent.height};
        mSwapchainFBs[i].id = hid;
    }
    
    return true;
}

bool NkVulkanDevice::CreateCommandPool() {
    VkCommandPoolCreateInfo info{};
    info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.queueFamilyIndex = mGraphicsFamily;
    info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    
    NK_VK_CHECKRET(vkCreateCommandPool(mDevice, &info, nullptr, &mGraphicsCmdPool),
                   "vkCreateCommandPool (graphics)");

    if (mComputeFamily != UINT32_MAX && mComputeFamily != mGraphicsFamily) {
        info.queueFamilyIndex = mComputeFamily;
        NK_VK_CHECKRET(vkCreateCommandPool(mDevice, &info, nullptr, &mComputeCmdPool),
                       "vkCreateCommandPool (compute)");
    } else {
        mComputeCmdPool = mGraphicsCmdPool;
    }
    
    return true;
}

bool NkVulkanDevice::CreateCommandBuffers() {
    for (uint32 i = 0; i < kMaxFrames; ++i) {
        VkCommandPoolCreateInfo cpci{};
        cpci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cpci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cpci.queueFamilyIndex = mGraphicsFamily;
        NK_VK_CHECK(vkCreateCommandPool(mDevice, &cpci, nullptr, &mFrames[i].cmdPool));

        VkCommandBufferAllocateInfo cbai{};
        cbai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cbai.commandPool        = mFrames[i].cmdPool;
        cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbai.commandBufferCount = 1;
        NK_VK_CHECK(vkAllocateCommandBuffers(mDevice, &cbai, &mFrames[i].cmdBuffer));
    }
    
    return true;
}

bool NkVulkanDevice::CreateSyncObjects() {
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32 i = 0; i < kMaxFrames; ++i) {
        NK_VK_CHECK(vkCreateSemaphore(mDevice, &semInfo, nullptr, &mFrames[i].imageAvailable));
        NK_VK_CHECK(vkCreateSemaphore(mDevice, &semInfo, nullptr, &mFrames[i].renderFinished));
        NK_VK_CHECK(vkCreateFence(mDevice, &fenceInfo, nullptr, &mFrames[i].inFlightFence));
    }
    
    return true;
}

void NkVulkanDevice::DestroySwapchainDependents() {
    // Attendre que tout soit terminé
    vkDeviceWaitIdle(mDevice);
    
    // Framebuffers
    for (auto& fb : mSwapchainFBs) {
        if (fb.IsValid()) DestroyFramebuffer(fb);
    }
    mSwapchainFBs.Clear();
    
    // Render pass swapchain
    if (mSwapchainRP.IsValid()) DestroyRenderPass(mSwapchainRP);
    
    // Depth
    if (mDepthView) vkDestroyImageView(mDevice, mDepthView, nullptr);
    if (mDepthImage) vkDestroyImage(mDevice, mDepthImage, nullptr);
    if (mDepthMemory) vkFreeMemory(mDevice, mDepthMemory, nullptr);
    mDepthView = VK_NULL_HANDLE;
    mDepthImage = VK_NULL_HANDLE;
    mDepthMemory = VK_NULL_HANDLE;
    
    // Image views swapchain
    for (auto& v : mSwapViews) {
        if (v) vkDestroyImageView(mDevice, v, nullptr);
    }
    mSwapViews.Clear();
    
    // Swapchain
    if (mSwapchain) {
        vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
        mSwapchain = VK_NULL_HANDLE;
    }
    mSwapImages.Clear();
}

bool NkVulkanDevice::RecreateSwapchain(uint32 w, uint32 h) {
    if (w == 0 || h == 0) {
        mMinimized = true;
        return true;
    }
    
    vkDeviceWaitIdle(mDevice);
    DestroySwapchainDependents();
    
    if (!CreateSwapchain(w, h)) return false;
    if (!CreateDepthResources(w, h)) return false;
    if (!CreateRenderPass()) return false;
    if (!CreateFramebuffers()) return false;
    
    mWidth = w;
    mHeight = h;
    mMinimized = false;
    
    NK_VK_LOG("Swapchain recreated %ux%u\n", w, h);
    return true;
}

// =============================================================================
void NkVulkanDevice::BeginFrame(NkFrameContext& frame) {
    if (!mIsValid || mMinimized) {
        frame.frameIndex = mFrameIndex;
        frame.frameNumber = mFrameNumber;
        return;
    }

    auto& fd = mFrames[mFrameIndex];
    
    vkWaitForFences(mDevice, 1, &fd.inFlightFence, VK_TRUE, UINT64_MAX);
    
    VkResult r = vkAcquireNextImageKHR(mDevice, mSwapchain, UINT64_MAX,
                                        fd.imageAvailable, VK_NULL_HANDLE,
                                        &mCurrentImageIdx);
    
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapchain(mWidth, mHeight);
        frame.frameIndex = mFrameIndex;
        frame.frameNumber = mFrameNumber;
        return;
    }
    
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
        NK_VK_ERR("vkAcquireNextImageKHR failed: %d\n", (int)r);
        return;
    }
    
    if (mImagesInFlight[mCurrentImageIdx] != VK_NULL_HANDLE) {
        vkWaitForFences(mDevice, 1, &mImagesInFlight[mCurrentImageIdx], VK_TRUE, UINT64_MAX);
    }
    mImagesInFlight[mCurrentImageIdx] = fd.inFlightFence;
    
    vkResetFences(mDevice, 1, &fd.inFlightFence);
    
    vkResetCommandBuffer(fd.cmdBuffer, 0);
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(fd.cmdBuffer, &beginInfo);
    
    frame.frameIndex = mFrameIndex;
    frame.frameNumber = mFrameNumber;
    frame.frameFence = {fd.inFlightFence};
}

void NkVulkanDevice::EndFrame(NkFrameContext&) {
    auto& fd = mFrames[mFrameIndex];
    vkEndCommandBuffer(fd.cmdBuffer);
    
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &fd.imageAvailable;
    si.pWaitDstStageMask = &waitStage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &fd.cmdBuffer;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &fd.renderFinished;
    
    vkQueueSubmit(mGraphicsQueue, 1, &si, fd.inFlightFence);
    
    VkPresentInfoKHR pi{};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &fd.renderFinished;
    pi.swapchainCount = 1;
    pi.pSwapchains = &mSwapchain;
    pi.pImageIndices = &mCurrentImageIdx;
    
    VkResult r = vkQueuePresentKHR(mGraphicsQueue, &pi);
    
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
        RecreateSwapchain(mWidth, mHeight);
    }
    
    mFrameIndex = (mFrameIndex + 1) % kMaxFrames;
    ++mFrameNumber;
}

void NkVulkanDevice::OnResize(uint32 w, uint32 h) {
    if (w == 0 || h == 0) {
        mMinimized = true;
        return;
    }
    RecreateSwapchain(w, h);
}

// =============================================================================
NkGPUFormat NkVulkanDevice::GetSwapchainFormat() const {
    switch (mSwapFormat) {
        case VK_FORMAT_B8G8R8A8_SRGB:   return NkGPUFormat::NK_BGRA8_SRGB;
        case VK_FORMAT_B8G8R8A8_UNORM:  return NkGPUFormat::NK_BGRA8_UNORM;
        case VK_FORMAT_R8G8B8A8_SRGB:   return NkGPUFormat::NK_RGBA8_SRGB;
        case VK_FORMAT_R8G8B8A8_UNORM:  return NkGPUFormat::NK_RGBA8_UNORM;
        default:                         return NkGPUFormat::NK_RGBA8_UNORM;
    }
}

// =============================================================================
// Accesseurs internes
// =============================================================================
VkRenderPass NkVulkanDevice::GetVkRP(uint64 id) const {
    auto it = mRenderPasses.find(id);
    return it != mRenderPasses.end() ? it->second.renderPass : VK_NULL_HANDLE;
}

VkFramebuffer NkVulkanDevice::GetVkFB(uint64 id) const {
    auto it = mFramebuffers.find(id);
    return it != mFramebuffers.end() ? it->second.framebuffer : VK_NULL_HANDLE;
}

VkBuffer NkVulkanDevice::GetVkBuffer(uint64 id) const {
    auto it = mBuffers.find(id);
    return it != mBuffers.end() ? it->second.buffer : VK_NULL_HANDLE;
}

VkImage NkVulkanDevice::GetVkImage(uint64 id) const {
    auto it = mTextures.find(id);
    return it != mTextures.end() ? it->second.image : VK_NULL_HANDLE;
}

VkImageView NkVulkanDevice::GetVkImageView(uint64 id) const {
    auto it = mTextures.find(id);
    return it != mTextures.end() ? it->second.view : VK_NULL_HANDLE;
}

VkSampler NkVulkanDevice::GetVkSampler(uint64 id) const {
    auto it = mSamplers.find(id);
    return it != mSamplers.end() ? it->second.sampler : VK_NULL_HANDLE;
}

VkPipeline NkVulkanDevice::GetVkPipeline(uint64 id) const {
    auto it = mPipelines.find(id);
    return it != mPipelines.end() ? it->second.pipeline : VK_NULL_HANDLE;
}

VkPipelineLayout NkVulkanDevice::GetVkPipelineLayout(uint64 id) const {
    auto it = mPipelines.find(id);
    return it != mPipelines.end() ? it->second.layout : VK_NULL_HANDLE;
}

bool NkVulkanDevice::IsComputePipeline(uint64 id) const {
    auto it = mPipelines.find(id);
    return it != mPipelines.end() && it->second.isCompute;
}

// =============================================================================
// Méthodes de stub pour les autres fonctionnalités RHI
// Ces méthodes seront implémentées dans les prochaines étapes
// =============================================================================

NkBufferHandle NkVulkanDevice::CreateBuffer(const NkBufferDesc& d) { return {}; }
void NkVulkanDevice::DestroyBuffer(NkBufferHandle& h) {}
bool NkVulkanDevice::WriteBuffer(NkBufferHandle, const void*, uint64, uint64) { return false; }
bool NkVulkanDevice::WriteBufferAsync(NkBufferHandle, const void*, uint64, uint64) { return false; }
bool NkVulkanDevice::ReadBuffer(NkBufferHandle, void*, uint64, uint64) { return false; }
NkMappedMemory NkVulkanDevice::MapBuffer(NkBufferHandle, uint64, uint64) { return {}; }
void NkVulkanDevice::UnmapBuffer(NkBufferHandle) {}

NkTextureHandle NkVulkanDevice::CreateTexture(const NkTextureDesc& d) { return {}; }
void NkVulkanDevice::DestroyTexture(NkTextureHandle& h) {}
bool NkVulkanDevice::WriteTexture(NkTextureHandle, const void*, uint32) { return false; }
bool NkVulkanDevice::WriteTextureRegion(NkTextureHandle, const void*, uint32, uint32, uint32, uint32, uint32, uint32, uint32, uint32, uint32) { return false; }
bool NkVulkanDevice::GenerateMipmaps(NkTextureHandle, NkFilter) { return false; }

NkSamplerHandle NkVulkanDevice::CreateSampler(const NkSamplerDesc& d) { return {}; }
void NkVulkanDevice::DestroySampler(NkSamplerHandle& h) {}

NkShaderHandle NkVulkanDevice::CreateShader(const NkShaderDesc& d) { return {}; }
void NkVulkanDevice::DestroyShader(NkShaderHandle& h) {}

NkPipelineHandle NkVulkanDevice::CreateGraphicsPipeline(const NkGraphicsPipelineDesc& d) { return {}; }
NkPipelineHandle NkVulkanDevice::CreateComputePipeline(const NkComputePipelineDesc& d) { return {}; }
void NkVulkanDevice::DestroyPipeline(NkPipelineHandle& h) {}

NkRenderPassHandle NkVulkanDevice::CreateRenderPass(const NkRenderPassDesc& d) { return {}; }
void NkVulkanDevice::DestroyRenderPass(NkRenderPassHandle& h) {}
NkFramebufferHandle NkVulkanDevice::CreateFramebuffer(const NkFramebufferDesc& d) { return {}; }
void NkVulkanDevice::DestroyFramebuffer(NkFramebufferHandle& h) {}

NkDescSetHandle NkVulkanDevice::CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc& d) { return {}; }
void NkVulkanDevice::DestroyDescriptorSetLayout(NkDescSetHandle& h) {}
NkDescSetHandle NkVulkanDevice::AllocateDescriptorSet(NkDescSetHandle) { return {}; }
void NkVulkanDevice::FreeDescriptorSet(NkDescSetHandle& h) {}
void NkVulkanDevice::UpdateDescriptorSets(const NkDescriptorWrite*, uint32) {}

NkICommandBuffer* NkVulkanDevice::CreateCommandBuffer(NkCommandBufferType t) { return nullptr; }
void NkVulkanDevice::DestroyCommandBuffer(NkICommandBuffer*& cb) {}

void NkVulkanDevice::Submit(NkICommandBuffer* const*, uint32, NkFenceHandle) {}
void NkVulkanDevice::SubmitAndPresent(NkICommandBuffer*) {}

NkFenceHandle NkVulkanDevice::CreateFence(bool) { return {}; }
void NkVulkanDevice::DestroyFence(NkFenceHandle& h) {}
bool NkVulkanDevice::WaitFence(NkFenceHandle, uint64) { return false; }
bool NkVulkanDevice::IsFenceSignaled(NkFenceHandle) { return false; }
void NkVulkanDevice::ResetFence(NkFenceHandle) {}
void NkVulkanDevice::WaitIdle() { if (mDevice) vkDeviceWaitIdle(mDevice); }

void NkVulkanDevice::QueryCaps() {
    mCaps.maxTextureDim2D = mDeviceProps.limits.maxImageDimension2D;
    mCaps.maxTextureDim3D = mDeviceProps.limits.maxImageDimension3D;
    mCaps.maxTextureCubeSize = mDeviceProps.limits.maxImageDimensionCube;
    mCaps.maxTextureArrayLayers = mDeviceProps.limits.maxImageArrayLayers;
    mCaps.maxColorAttachments = mDeviceProps.limits.maxColorAttachments;
    mCaps.maxUniformBufferRange = mDeviceProps.limits.maxUniformBufferRange;
    mCaps.maxStorageBufferRange = (uint32)mDeviceProps.limits.maxStorageBufferRange;
    mCaps.maxPushConstantBytes = mDeviceProps.limits.maxPushConstantsSize;
    mCaps.maxVertexAttributes = mDeviceProps.limits.maxVertexInputAttributes;
    mCaps.maxVertexBindings = mDeviceProps.limits.maxVertexInputBindings;
    mCaps.maxComputeGroupSizeX = mDeviceProps.limits.maxComputeWorkGroupSize[0];
    mCaps.maxComputeGroupSizeY = mDeviceProps.limits.maxComputeWorkGroupSize[1];
    mCaps.maxComputeGroupSizeZ = mDeviceProps.limits.maxComputeWorkGroupSize[2];
    mCaps.maxComputeSharedMemory = mDeviceProps.limits.maxComputeSharedMemorySize;
    mCaps.maxSamplerAnisotropy = (uint32)mDeviceProps.limits.maxSamplerAnisotropy;
    mCaps.minUniformBufferAlign = (uint32)mDeviceProps.limits.minUniformBufferOffsetAlignment;
    mCaps.minStorageBufferAlign = (uint32)mDeviceProps.limits.minStorageBufferOffsetAlignment;
    
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memProps);
    for (uint32 i = 0; i < memProps.memoryHeapCount; ++i) {
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            mCaps.vramBytes += memProps.memoryHeaps[i].size;
        }
    }
}

} // namespace nkentseu

#endif // NK_RHI_VK_ENABLED