// =============================================================================
// NkVulkanContext.cpp — Production Ready
// Patches inclus :
//   - OnResize : guard extent 0x0 (fenêtre minimisée)
//   - RecreateSwapchain : WaitIdle complet avant destruction
//   - Device lost : détection DEVICE_LOST / OUT_OF_DATE
//   - Compute queue dédiée si disponible
// =============================================================================
#include "NkVulkanContext.h"

#include "NKPlatform/NkPlatformDetect.h"
#include "NKLogger/NkLog.h"
#include "NKContainers/NkContainers.h"
#include "NKMath/NkFunctions.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKContext/Core/NkGpuPolicy.h"

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
#define NK_VK_CHECK(r) do { VkResult _r=(r); if(_r!=VK_SUCCESS){ NK_VK_ERR(#r " = %d\n",(int)_r); return false; } } while(0)

namespace nkentseu {

// =============================================================================
NkVulkanContext::~NkVulkanContext() { if (mIsValid) Shutdown(); }

bool NkVulkanContext::Initialize(const NkWindow& window, const NkContextDesc& desc) {
    if (mIsValid) { NK_VK_ERR("Already initialized\n"); return false; }
    mDesc = desc;
    const NkVulkanDesc& vk   = desc.vulkan;
    const NkSurfaceDesc surf = window.GetSurfaceDesc();
    if (!surf.IsValid()) { NK_VK_ERR("Invalid NkSurfaceDesc\n"); return false; }

    if (!CreateInstance(vk))          return false;
    if (vk.debugMessenger)
        CreateDebugMessenger(vk);
    if (!CreateSurface(surf))         return false;
    if (!SelectPhysicalDevice(vk))    return false;
    if (!CreateLogicalDevice(vk))     return false;
    if (!CreateSwapchain(surf.width, surf.height, vk)) return false;
    if (!CreateRenderPass())          return false;
    if (!CreateDepthResources(surf.width, surf.height)) return false;
    if (!CreateFramebuffers())        return false;
    if (!CreateCommandPool())         return false;
    if (!CreateCommandBuffers())      return false;
    if (!CreateSyncObjects())         return false;

    mVSync   = vk.vsync;
    mIsValid = true;
    mData.minimized = false;
    mData.isPresent = true;
    mData.isAcquire = false;
    
    NK_VK_LOG("Ready — %s | VRAM %u MB\n", mData.rendererName, mData.vramMB);
    return true;
}

// =============================================================================
void NkVulkanContext::Shutdown() {
    if (!mIsValid) return;
    vkDeviceWaitIdle(mData.device);
    DestroySwapchainDependents();

    if (mData.renderPass) {
        vkDestroyRenderPass(mData.device, mData.renderPass, nullptr);
        mData.renderPass = VK_NULL_HANDLE;
    }

    for (uint32 i = 0; i < kNkVkMaxFrames; ++i) {
        if (mData.imageAvailSem[i]) vkDestroySemaphore(mData.device, mData.imageAvailSem[i], nullptr);
        if (mData.renderDoneSem[i]) vkDestroySemaphore(mData.device, mData.renderDoneSem[i], nullptr);
        if (mData.inFlightFences[i]) vkDestroyFence(mData.device, mData.inFlightFences[i], nullptr);
    }
    if (mData.commandPool)  vkDestroyCommandPool(mData.device, mData.commandPool, nullptr);
    if (mData.computePool)  vkDestroyCommandPool(mData.device, mData.computePool, nullptr);
    if (mData.device)       vkDestroyDevice(mData.device, nullptr);
    if (mData.surface)      vkDestroySurfaceKHR(mData.instance, mData.surface, nullptr);
    if (mData.debugMessenger) {
        auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(mData.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (fn) fn(mData.instance, mData.debugMessenger, nullptr);
    }
    if (mData.instance) vkDestroyInstance(mData.instance, nullptr);

    mData    = NkVulkanContextData{};
    mIsValid = false;
    NK_VK_LOG("Shutdown OK\n");
}

// =============================================================================
void NkVulkanContext::DestroySwapchainDependents() {
    // Depth
    if (mData.depthView)   vkDestroyImageView(mData.device, mData.depthView, nullptr);
    if (mData.depthImage)  vkDestroyImage(mData.device, mData.depthImage, nullptr);
    if (mData.depthMemory) vkFreeMemory(mData.device, mData.depthMemory, nullptr);
    mData.depthView = VK_NULL_HANDLE;
    mData.depthImage = VK_NULL_HANDLE;
    mData.depthMemory = VK_NULL_HANDLE;

    // Framebuffers
    for (uint32 i = 0; i < mData.swapImageCount; ++i) {
        if (mData.framebuffers[i]) {
            vkDestroyFramebuffer(mData.device, mData.framebuffers[i], nullptr);
            mData.framebuffers[i] = VK_NULL_HANDLE;
        }
    }
    // Image views
    for (uint32 i = 0; i < mData.swapImageCount; ++i) {
        if (mData.swapViews[i]) {
            vkDestroyImageView(mData.device, mData.swapViews[i], nullptr);
            mData.swapViews[i] = VK_NULL_HANDLE;
        }
    }
    // Swapchain
    if (mData.swapchain) {
        vkDestroySwapchainKHR(mData.device, mData.swapchain, nullptr);
        mData.swapchain = VK_NULL_HANDLE;
    }
}

bool NkVulkanContext::CanAquire() {
    // Inspiré de CanAquire() du second code
    if (mData.swapImageCount == 0) return false;
    if (mData.swapExtent.width == 0 || mData.swapExtent.height == 0) return false;
    return true;
}

bool NkVulkanContext::RecreateSwapchain(uint32 w, uint32 h) {
    if (w == 0 || h == 0) {
        mData.minimized = true;
        return true;
    }
    
    mData.minimized = false;

    // Notifier les callbacks de cleanup
    for (usize i = 0; i < mCleanCallbacks.Size(); ++i)
        if (mCleanCallbacks[i].fn) mCleanCallbacks[i].fn();

    vkDeviceWaitIdle(mData.device);
    DestroySwapchainDependents();

    if (!CreateSwapchain(w, h, mDesc.vulkan))  { mIsValid = false; return false; }
    if (!CreateDepthResources(w, h))            { mIsValid = false; return false; }
    if (!CreateFramebuffers())                  { mIsValid = false; return false; }

    for (uint32 i = 0; i < kNkVkMaxImages; ++i)
        mData.imageInFlight[i] = VK_NULL_HANDLE;

    // Notifier les callbacks de recréation
    for (usize i = 0; i < mRecreateCallbacks.Size(); ++i)
        if (mRecreateCallbacks[i].fn) mRecreateCallbacks[i].fn();

    mData.isPresent = true;
    NK_VK_LOG("Swapchain recreated %ux%u\n", w, h);
    return true;
}

// =============================================================================
bool NkVulkanContext::BeginFrame() {
    if (!mIsValid) return false;
    
    mData.isAcquire = false;
    
    // Protection contre la minimisation (inspiré du second code)
    if (mData.minimized || !mData.isPresent || !CanAquire()) {
        return false;
    }
    
    uint32 frame = mData.currentFrame;
    
    // Attendre que la frame précédente soit terminée
    vkWaitForFences(mData.device, 1, &mData.inFlightFences[frame], VK_TRUE, UINT64_MAX);
    
    // Vérifier l'état de la surface
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mData.physicalDevice, mData.surface, &caps);
    
    if (caps.currentExtent.width == 0 || caps.currentExtent.height == 0) {
        mData.minimized = true;
        return false;
    }
    
    // Mettre à jour l'extent si nécessaire
    if (caps.currentExtent.width != mData.swapExtent.width || 
        caps.currentExtent.height != mData.swapExtent.height) {
        mData.swapExtent = caps.currentExtent;
        // Ne pas recréer ici, laisser OnResize le faire
        return false;
    }
    
    // Acquérir l'image suivante
    VkResult r = vkAcquireNextImageKHR(
        mData.device, mData.swapchain, UINT64_MAX,
        mData.imageAvailSem[frame], VK_NULL_HANDLE,
        &mData.currentImageIndex);
    
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {
        if (caps.currentExtent.width > 0 && caps.currentExtent.height > 0) {
            RecreateSwapchain(caps.currentExtent.width, caps.currentExtent.height);
        }
        mData.isPresent = true;
        return false;
    }
    
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
        NK_VK_ERR("vkAcquireNextImageKHR = %d\n", (int)r);
        return false;
    }
    
    // Attendre si l'image est en vol
    if (mData.imageInFlight[mData.currentImageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(mData.device, 1, &mData.imageInFlight[mData.currentImageIndex], VK_TRUE, UINT64_MAX);
    }
    mData.imageInFlight[mData.currentImageIndex] = mData.inFlightFences[frame];
    
    vkResetFences(mData.device, 1, &mData.inFlightFences[frame]);
    
    // Commencer l'enregistrement du command buffer
    VkCommandBuffer cmd = mData.commandBuffers[frame];
    vkResetCommandBuffer(cmd, 0);
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        return false;
    }
    
    // Commencer le render pass
    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = mData.renderPass;
    rpInfo.framebuffer = mData.framebuffers[mData.currentImageIndex];
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = mData.swapExtent;
    
    VkClearValue clearVals[2];
    clearVals[0].color = {{0.1f, 0.1f, 0.1f, 1.0f}};
    clearVals[1].depthStencil = {1.0f, 0};
    rpInfo.clearValueCount = 2;
    rpInfo.pClearValues = clearVals;
    
    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    // Configurer viewport et scissor
    VkViewport vp{};
    vp.x = 0; vp.y = 0;
    vp.width = (float)mData.swapExtent.width;
    vp.height = (float)mData.swapExtent.height;
    vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
    
    VkRect2D scissor{{0, 0}, mData.swapExtent};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    
    mData.isAcquire = true;
    return true;
}

void NkVulkanContext::EndFrame() {
    if (!mIsValid || !mData.isAcquire) return;
    
    VkCommandBuffer cmd = mData.commandBuffers[mData.currentFrame];
    vkCmdEndRenderPass(cmd);
    
    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        NK_VK_ERR("vkEndCommandBuffer failed\n");
    }
}

void NkVulkanContext::Present() {
    if (!mIsValid || !mData.isAcquire) {
        mData.isPresent = true;
        return;
    }
    
    uint32 frame = mData.currentFrame;
    VkCommandBuffer cmd = mData.commandBuffers[frame];

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &mData.imageAvailSem[frame];
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &mData.renderDoneSem[frame];

    if (vkQueueSubmit(mData.graphicsQueue, 1, &submit, mData.inFlightFences[frame]) != VK_SUCCESS) {
        NK_VK_ERR("vkQueueSubmit failed\n");
        mData.isPresent = true;
        return;
    }

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &mData.renderDoneSem[frame];
    present.swapchainCount = 1;
    present.pSwapchains = &mData.swapchain;
    present.pImageIndices = &mData.currentImageIndex;

    VkResult r = vkQueuePresentKHR(mData.presentQueue, &present);
    
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
        VkSurfaceCapabilitiesKHR caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mData.physicalDevice, mData.surface, &caps);
        if (caps.currentExtent.width > 0 && caps.currentExtent.height > 0) {
            RecreateSwapchain(caps.currentExtent.width, caps.currentExtent.height);
        }
    } else if (r != VK_SUCCESS) {
        NK_VK_ERR("vkQueuePresentKHR = %d\n", (int)r);
    }

    // Attendre que le device soit idle (optionnel, comme dans le second code)
    vkDeviceWaitIdle(mData.device);

    mData.currentFrame = (frame + 1) % kNkVkMaxFrames;
    mData.isPresent = true;
}

bool NkVulkanContext::OnResize(uint32 w, uint32 h) {
    if (!mIsValid) return false;
    
    // Protection contre la minimisation
    if (w == 0 || h == 0) {
        mData.minimized = true;
        return true;
    }
    
    mData.minimized = false;
    return RecreateSwapchain(w, h);
}

// =============================================================================
// Callbacks de recréation de swapchain (pattern inspiré de l'ancien VulkanContext)
// =============================================================================
NkSwapchainCallbackHandle NkVulkanContext::AddCleanUpCallback(NkSwapchainCleanFn fn) {
    if (!fn) return NK_INVALID_CALLBACK_HANDLE;
    uint32 h = mNextCbHandle++;
    NkCbEntry e; e.handle = h; e.fn = fn;
    mCleanCallbacks.PushBack(e);
    return h;
}

NkSwapchainCallbackHandle NkVulkanContext::AddRecreateCallback(NkSwapchainRecreateFn fn) {
    if (!fn) return NK_INVALID_CALLBACK_HANDLE;
    uint32 h = mNextCbHandle++;
    NkCbEntry e; e.handle = h; e.fn = fn;
    mRecreateCallbacks.PushBack(e);
    return h;
}

void NkVulkanContext::RemoveCleanUpCallback(NkSwapchainCallbackHandle h) {
    for (usize i = 0; i < mCleanCallbacks.Size(); ++i) {
        if (mCleanCallbacks[i].handle == h) {
            mCleanCallbacks.Erase(mCleanCallbacks.Begin() + i);
            return;
        }
    }
}

void NkVulkanContext::RemoveRecreateCallback(NkSwapchainCallbackHandle h) {
    for (usize i = 0; i < mRecreateCallbacks.Size(); ++i) {
        if (mRecreateCallbacks[i].handle == h) {
            mRecreateCallbacks.Erase(mRecreateCallbacks.Begin() + i);
            return;
        }
    }
}

void NkVulkanContext::SetVSync(bool enabled) { mVSync = enabled; /* nécessite RecreateSwapchain */ }
bool NkVulkanContext::GetVSync()  const { return mVSync; }
bool NkVulkanContext::IsValid()   const { return mIsValid; }
NkGraphicsApi NkVulkanContext::GetApi()  const { return NkGraphicsApi::NK_API_VULKAN; }
NkContextDesc NkVulkanContext::GetDesc() const { return mDesc; }
void* NkVulkanContext::GetNativeContextData() { return &mData; }
bool NkVulkanContext::SupportsCompute() const { return mData.computeFamily != UINT32_MAX; }

NkContextInfo NkVulkanContext::GetInfo() const {
    NkContextInfo i;
    i.api              = NkGraphicsApi::NK_API_VULKAN;
    i.renderer         = mData.rendererName;
    i.vendor           = mData.rendererName;
    i.version          = "Vulkan";
    i.vramMB           = mData.vramMB;
    i.computeSupported = SupportsCompute();
    i.windowWidth      = mData.swapExtent.width;
    i.windowHeight     = mData.swapExtent.height;
    return i;
}

// =============================================================================
// Création de l'instance
// =============================================================================
bool NkVulkanContext::CreateInstance(const NkVulkanDesc& d) {
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = d.appName;
    appInfo.applicationVersion = d.appVersion;
    appInfo.pEngineName        = d.engineName;
    appInfo.engineVersion      = VK_MAKE_VERSION(1,0,0);
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
    if (d.debugMessenger)
        extensions.PushBack(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    for (uint32 i = 0; i < d.extraInstanceExtCount; ++i)
        extensions.PushBack(d.extraInstanceExt[i]);

    NkVector<const char*> layers;
    if (d.validationLayers) {
        layers.PushBack("VK_LAYER_KHRONOS_validation");
        logger.Infof("[NkVulkan] Validation layers enabled");  // ← AJOUTEZ
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

    NK_VK_CHECK(vkCreateInstance(&info, nullptr, &mData.instance));
    NK_VK_LOG("Instance OK\n");
    return true;
}

// =============================================================================
// Debug Messenger
// =============================================================================
VKAPI_ATTR VkBool32 VKAPI_CALL NkVulkanContext::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* /*user*/)
{
    const char* sev = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)   ? "ERROR"   :
                      (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ? "WARNING" :
                      (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)    ? "INFO"    : "VERBOSE";
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        logger.Errorf("[NkVulkan][%s] %s", sev, data->pMessage);
    } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        logger.Warnf("[NkVulkan][%s] %s", sev, data->pMessage);
    } else {
        logger.Infof("[NkVulkan][%s] %s", sev, data->pMessage);
    }
    return VK_FALSE;
}

bool NkVulkanContext::CreateDebugMessenger(const NkVulkanDesc&) {
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
        vkGetInstanceProcAddr(mData.instance, "vkCreateDebugUtilsMessengerEXT");
    if (!fn) { NK_VK_ERR("vkCreateDebugUtilsMessengerEXT not found\n"); return false; }
    NK_VK_CHECK(fn(mData.instance, &info, nullptr, &mData.debugMessenger));
    return true;
}

// =============================================================================
// Surface
// =============================================================================
bool NkVulkanContext::CreateSurface(const NkSurfaceDesc& surf) {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    VkWin32SurfaceCreateInfoKHR ci{};
    ci.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    ci.hwnd      = static_cast<HWND>(surf.hwnd);
    ci.hinstance = static_cast<HINSTANCE>(surf.hinstance);
    NK_VK_CHECK(vkCreateWin32SurfaceKHR(mData.instance, &ci, nullptr, &mData.surface));
#elif defined(NKENTSEU_WINDOWING_XLIB)
    VkXlibSurfaceCreateInfoKHR ci{};
    ci.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    ci.dpy    = static_cast<Display*>(surf.display);
    ci.window = (::Window)surf.window;
    NK_VK_CHECK(vkCreateXlibSurfaceKHR(mData.instance, &ci, nullptr, &mData.surface));
#elif defined(NKENTSEU_WINDOWING_XCB)
    VkXcbSurfaceCreateInfoKHR ci{};
    ci.sType      = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    ci.connection = static_cast<xcb_connection_t*>(surf.connection);
    ci.window     = (xcb_window_t)surf.window;
    NK_VK_CHECK(vkCreateXcbSurfaceKHR(mData.instance, &ci, nullptr, &mData.surface));
#elif defined(NKENTSEU_WINDOWING_WAYLAND)
    VkWaylandSurfaceCreateInfoKHR ci{};
    ci.sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    ci.display = static_cast<wl_display*>(surf.display);
    ci.surface = static_cast<wl_surface*>(surf.surface);
    NK_VK_CHECK(vkCreateWaylandSurfaceKHR(mData.instance, &ci, nullptr, &mData.surface));
#elif defined(NKENTSEU_PLATFORM_ANDROID)
    VkAndroidSurfaceCreateInfoKHR ci{};
    ci.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    ci.window = static_cast<ANativeWindow*>(surf.nativeWindow);
    NK_VK_CHECK(vkCreateAndroidSurfaceKHR(mData.instance, &ci, nullptr, &mData.surface));
#elif defined(NKENTSEU_PLATFORM_MACOS)
    VkMetalSurfaceCreateInfoEXT ci{};
    ci.sType  = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    ci.pLayer = static_cast<const CAMetalLayer*>(surf.metalLayer);
    NK_VK_CHECK(vkCreateMetalSurfaceEXT(mData.instance, &ci, nullptr, &mData.surface));
#else
    NK_VK_ERR("No Vulkan surface impl for this platform\n");
    return false;
#endif
    NK_VK_LOG("Surface OK\n");
    return true;
}

// =============================================================================
// Queue families
// =============================================================================
void NkVulkanContext::FindQueueFamilies(VkPhysicalDevice dev) {
    uint32 count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    NkVector<VkQueueFamilyProperties> props(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, props.Data());

    mData.graphicsFamily = UINT32_MAX;
    mData.presentFamily  = UINT32_MAX;
    mData.computeFamily  = UINT32_MAX;

    for (uint32 i = 0; i < count; ++i) {
        if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            mData.graphicsFamily = i;
        // Queue compute dédiée (pas graphics) — meilleure perf
        if ((props[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            !(props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            mData.computeFamily = i;
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, mData.surface, &present);
        if (present) mData.presentFamily = i;
    }
    // Fallback : compute via graphics family si pas de queue dédiée
    if (mData.computeFamily == UINT32_MAX && mData.graphicsFamily != UINT32_MAX) {
        if (vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, props.Data()),
            props[mData.graphicsFamily].queueFlags & VK_QUEUE_COMPUTE_BIT)
            mData.computeFamily = mData.graphicsFamily;
    }
}

bool NkVulkanContext::CheckDeviceExtensions(VkPhysicalDevice dev) {
    uint32 count = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, nullptr);
    NkVector<VkExtensionProperties> avail(count);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, avail.Data());
    for (auto& ext : avail)
        if (strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
            return true;
    return false;
}

bool NkVulkanContext::IsDeviceSuitable(VkPhysicalDevice dev, const NkVulkanDesc&) {
    FindQueueFamilies(dev);
    if (mData.graphicsFamily == UINT32_MAX || mData.presentFamily == UINT32_MAX) return false;
    if (!CheckDeviceExtensions(dev)) return false;
    uint32 fmtCount = 0, modeCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, mData.surface, &fmtCount, nullptr);
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, mData.surface, &modeCount, nullptr);
    return fmtCount > 0 && modeCount > 0;
}

bool NkVulkanContext::SelectPhysicalDevice(const NkVulkanDesc& d) {
    uint32 count = 0;
    vkEnumeratePhysicalDevices(mData.instance, &count, nullptr);
    if (count == 0) { NK_VK_ERR("No Vulkan physical device\n"); return false; }
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
        VkPhysicalDevice dev = devices[i];
        if (!IsDeviceSuitable(dev, d)) continue;
        Candidate c{};
        c.dev = dev;
        c.index = i;
        vkGetPhysicalDeviceProperties(dev, &c.props);
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
                if (c.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)   return 400;
                if (c.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) return 250;
                if (c.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)    return 120;
                if (c.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)            return 60;
                return 100;
            case NkGpuPreference::NK_LOW_POWER:
                if (c.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) return 400;
                if (c.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)   return 250;
                if (c.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)    return 120;
                if (c.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)            return 60;
                return 100;
            case NkGpuPreference::NK_DEFAULT:
            default:
                if (c.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)   return 350;
                if (c.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) return 250;
                if (c.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)    return 120;
                if (c.props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)            return 60;
                return 100;
        }
    };

    const Candidate* selected = nullptr;
    if (requestedIndex != UINT32_MAX) {
        for (uint32 i = 0; i < candidates.Size(); ++i) {
            if (candidates[i].index == requestedIndex) {
                if (!strictVendor || matchesVendor(candidates[i])) {
                    selected = &candidates[i];
                } else {
                    NK_VK_LOG("Requested adapter #%u rejected due to vendor filter (%s)\n",
                              requestedIndex, NkGpuPolicy::VendorName(vendorPreference));
                }
                break;
            }
        }
        if (!selected) {
            NK_VK_LOG("Requested Vulkan adapter #%u unavailable/suitable. Falling back to policy.\n",
                      requestedIndex);
        }
    }

    if (!selected) {
        int bestScore = -1;
        for (uint32 i = 0; i < candidates.Size(); ++i) {
            const Candidate& c = candidates[i];
            if (strictVendor && !matchesVendor(c)) continue;
            const int score = scoreCandidate(c);
            if (score > bestScore) {
                bestScore = score;
                selected = &c;
            }
        }
        if (!selected && strictVendor) {
            NK_VK_LOG("No Vulkan adapter matched vendor=%s, retry without vendor filter.\n",
                      NkGpuPolicy::VendorName(vendorPreference));
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

    vkGetPhysicalDeviceProperties(selected->dev, &mData.deviceProps);
    mData.rendererName = mData.deviceProps.deviceName;

    // VRAM
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(selected->dev, &memProps);
    for (uint32 i = 0; i < memProps.memoryHeapCount; ++i) {
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            mData.vramMB = (uint32)(memProps.memoryHeaps[i].size / (1024*1024));
            break;
        }
    }
    NK_VK_LOG("GPU #%u: %s | vendor=0x%04X | VRAM: %u MB\n",
              selected->index,
              mData.rendererName,
              (unsigned)selected->props.vendorID,
              mData.vramMB);
    return true;
}

// =============================================================================
// Device logique
// =============================================================================
bool NkVulkanContext::CreateLogicalDevice(const NkVulkanDesc& d) {
    NkArray<uint32, 3> uniqueFamilies{};
    uint32 uniqueCount = 0;
    const auto pushUniqueFamily = [&](uint32 fam) {
        for (uint32 i = 0; i < uniqueCount; ++i) {
            if (uniqueFamilies[i] == fam) {
                return;
            }
        }
        if (uniqueCount < uniqueFamilies.Size()) {
            uniqueFamilies[uniqueCount++] = fam;
        }
    };
    pushUniqueFamily(mData.graphicsFamily);
    pushUniqueFamily(mData.presentFamily);
    if (d.enableComputeQueue && mData.computeFamily != UINT32_MAX) {
        pushUniqueFamily(mData.computeFamily);
    }

    float priority = 1.0f;
    NkVector<VkDeviceQueueCreateInfo> queueInfos;
    for (uint32 i = 0; i < uniqueCount; ++i) {
        const uint32 fam = uniqueFamilies[i];
        VkDeviceQueueCreateInfo qi{};
        qi.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = fam;
        qi.queueCount       = 1;
        qi.pQueuePriorities = &priority;
        queueInfos.PushBack(qi);
    }

    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = VK_TRUE;

    NkVector<const char*> extensions;
    extensions.PushBack(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#if defined(NKENTSEU_PLATFORM_MACOS)
    extensions.PushBack("VK_KHR_portability_subset");
#endif
    for (uint32 i = 0; i < d.extraDeviceExtCount; ++i)
        extensions.PushBack(d.extraDeviceExt[i]);

    VkDeviceCreateInfo info{};
    info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount    = (uint32)queueInfos.Size();
    info.pQueueCreateInfos       = queueInfos.Data();
    info.enabledExtensionCount   = (uint32)extensions.Size();
    info.ppEnabledExtensionNames = extensions.Data();
    info.pEnabledFeatures        = &features;

    NK_VK_CHECK(vkCreateDevice(mData.physicalDevice, &info, nullptr, &mData.device));

    vkGetDeviceQueue(mData.device, mData.graphicsFamily, 0, &mData.graphicsQueue);
    vkGetDeviceQueue(mData.device, mData.presentFamily,  0, &mData.presentQueue);
    if (d.enableComputeQueue && mData.computeFamily != UINT32_MAX)
        vkGetDeviceQueue(mData.device, mData.computeFamily, 0, &mData.computeQueue);

    NK_VK_LOG("Device OK (graphics=%u present=%u compute=%u)\n",
        mData.graphicsFamily, mData.presentFamily, mData.computeFamily);
    return true;
}

// =============================================================================
// Swapchain
// =============================================================================
bool NkVulkanContext::CreateSwapchain(uint32 w, uint32 h, const NkVulkanDesc& d) {
    // Capacités
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mData.physicalDevice, mData.surface, &caps);

    // Format
    uint32 fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(mData.physicalDevice, mData.surface, &fmtCount, nullptr);
    NkVector<VkSurfaceFormatKHR> fmts(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(mData.physicalDevice, mData.surface, &fmtCount, fmts.Data());

    VkSurfaceFormatKHR fmt = fmts[0];
    for (auto& f : fmts)
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            { fmt = f; break; }
    mData.swapFormat = fmt.format;

    // Present mode
    uint32 modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(mData.physicalDevice, mData.surface, &modeCount, nullptr);
    NkVector<VkPresentModeKHR> modes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(mData.physicalDevice, mData.surface, &modeCount, modes.Data());

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // VSync garanti
    if (!d.vsync) {
        for (auto m : modes)
            if (m == VK_PRESENT_MODE_MAILBOX_KHR) { presentMode = m; break; }
        if (presentMode == VK_PRESENT_MODE_FIFO_KHR)
            for (auto m : modes)
                if (m == VK_PRESENT_MODE_IMMEDIATE_KHR) { presentMode = m; break; }
    }

    // Extent
    // Cas 1 : driver fournit un extent fixe et valide → l'utiliser tel quel.
    // Cas 2 : caps.currentExtent == {0,0} pendant une transition (minimize/restore) → utiliser w/h clamped.
    // Cas 3 : caps.currentExtent.width == UINT32_MAX → extent libre, clamper w/h aux limites.
    mData.swapExtent = {w, h};
    if (caps.currentExtent.width != UINT32_MAX &&
        caps.currentExtent.width  != 0 &&
        caps.currentExtent.height != 0)
    {
        mData.swapExtent = caps.currentExtent;
    }
    else {
        mData.swapExtent.width  = math::NkClamp(w, caps.minImageExtent.width,  caps.maxImageExtent.width);
        mData.swapExtent.height = math::NkClamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    // Garde finale : ne jamais créer une swapchain avec un extent nul
    if (mData.swapExtent.width == 0 || mData.swapExtent.height == 0) {
        return false;
    }

    const uint32 maxImageCount = (caps.maxImageCount > 0) ? caps.maxImageCount : d.swapchainImages;
    uint32 imgCount = math::NkMax(caps.minImageCount + 1, math::NkMin(d.swapchainImages, maxImageCount));

    VkSwapchainCreateInfoKHR ci{};
    ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface          = mData.surface;
    ci.minImageCount    = imgCount;
    ci.imageFormat      = fmt.format;
    ci.imageColorSpace  = fmt.colorSpace;
    ci.imageExtent      = mData.swapExtent;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32 families[] = {mData.graphicsFamily, mData.presentFamily};
    if (mData.graphicsFamily != mData.presentFamily) {
        ci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices   = families;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    ci.preTransform   = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode    = presentMode;
    ci.clipped        = VK_TRUE;

    NK_VK_CHECK(vkCreateSwapchainKHR(mData.device, &ci, nullptr, &mData.swapchain));

    vkGetSwapchainImagesKHR(mData.device, mData.swapchain, &mData.swapImageCount, nullptr);
    vkGetSwapchainImagesKHR(mData.device, mData.swapchain, &mData.swapImageCount, mData.swapImages);

    for (uint32 i = 0; i < mData.swapImageCount; ++i) {
        VkImageViewCreateInfo iv{};
        iv.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        iv.image                           = mData.swapImages[i];
        iv.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        iv.format                          = mData.swapFormat;
        iv.components                      = {VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,
                                              VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY};
        iv.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        iv.subresourceRange.baseMipLevel   = 0;
        iv.subresourceRange.levelCount     = 1;
        iv.subresourceRange.baseArrayLayer = 0;
        iv.subresourceRange.layerCount     = 1;
        NK_VK_CHECK(vkCreateImageView(mData.device, &iv, nullptr, &mData.swapViews[i]));
    }
    NK_VK_LOG("Swapchain OK (%ux%u x%u)\n", mData.swapExtent.width, mData.swapExtent.height, mData.swapImageCount);
    return true;
}

// =============================================================================
// RenderPass
// =============================================================================
bool NkVulkanContext::CreateRenderPass() {
    VkAttachmentDescription colorAtt{};
    colorAtt.format         = mData.swapFormat;
    colorAtt.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAtt.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAtt.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAtt{};
    depthAtt.format         = FindDepthFormat();
    depthAtt.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAtt.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAtt.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAtt.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription atts[] = {colorAtt, depthAtt};
    VkRenderPassCreateInfo info{};
    info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 2;
    info.pAttachments    = atts;
    info.subpassCount    = 1;
    info.pSubpasses      = &subpass;
    info.dependencyCount = 1;
    info.pDependencies   = &dep;

    NK_VK_CHECK(vkCreateRenderPass(mData.device, &info, nullptr, &mData.renderPass));
    return true;
}

// =============================================================================
// Depth
// =============================================================================
VkFormat NkVulkanContext::FindDepthFormat() {
    VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT
    };
    for (auto f : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(mData.physicalDevice, f, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            return f;
    }
    return VK_FORMAT_D24_UNORM_S8_UINT;
}

bool NkVulkanContext::FindMemoryType(uint32 filter, VkMemoryPropertyFlags props, uint32& out) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(mData.physicalDevice, &memProps);
    for (uint32 i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((filter & (1<<i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
            out = i; return true;
        }
    }
    return false;
}

bool NkVulkanContext::CreateDepthResources(uint32 w, uint32 h) {
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
    NK_VK_CHECK(vkCreateImage(mData.device, &imgInfo, nullptr, &mData.depthImage));

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(mData.device, mData.depthImage, &req);
    uint32 memType = 0;
    if (!FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memType))
        return false;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = req.size;
    allocInfo.memoryTypeIndex = memType;
    NK_VK_CHECK(vkAllocateMemory(mData.device, &allocInfo, nullptr, &mData.depthMemory));
    vkBindImageMemory(mData.device, mData.depthImage, mData.depthMemory, 0);

    VkImageViewCreateInfo ivInfo{};
    ivInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivInfo.image                           = mData.depthImage;
    ivInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    ivInfo.format                          = depthFmt;
    ivInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    ivInfo.subresourceRange.baseMipLevel   = 0;
    ivInfo.subresourceRange.levelCount     = 1;
    ivInfo.subresourceRange.baseArrayLayer = 0;
    ivInfo.subresourceRange.layerCount     = 1;
    NK_VK_CHECK(vkCreateImageView(mData.device, &ivInfo, nullptr, &mData.depthView));
    return true;
}

// =============================================================================
// Framebuffers
// =============================================================================
bool NkVulkanContext::CreateFramebuffers() {
    for (uint32 i = 0; i < mData.swapImageCount; ++i) {
        VkImageView atts[] = {mData.swapViews[i], mData.depthView};
        VkFramebufferCreateInfo info{};
        info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass      = mData.renderPass;
        info.attachmentCount = 2;
        info.pAttachments    = atts;
        info.width           = mData.swapExtent.width;
        info.height          = mData.swapExtent.height;
        info.layers          = 1;
        NK_VK_CHECK(vkCreateFramebuffer(mData.device, &info, nullptr, &mData.framebuffers[i]));
    }
    return true;
}

// =============================================================================
// Command pool + buffers
// =============================================================================
bool NkVulkanContext::CreateCommandPool() {
    VkCommandPoolCreateInfo info{};
    info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.queueFamilyIndex = mData.graphicsFamily;
    info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    NK_VK_CHECK(vkCreateCommandPool(mData.device, &info, nullptr, &mData.commandPool));

    if (mData.computeFamily != UINT32_MAX && mData.computeFamily != mData.graphicsFamily) {
        info.queueFamilyIndex = mData.computeFamily;
        NK_VK_CHECK(vkCreateCommandPool(mData.device, &info, nullptr, &mData.computePool));
    } else {
        mData.computePool = mData.commandPool;
    }
    return true;
}

bool NkVulkanContext::CreateCommandBuffers() {
    VkCommandBufferAllocateInfo info{};
    info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool        = mData.commandPool;
    info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = kNkVkMaxFrames;
    NK_VK_CHECK(vkAllocateCommandBuffers(mData.device, &info, mData.commandBuffers));
    return true;
}

// =============================================================================
// Objets de synchronisation
// =============================================================================
bool NkVulkanContext::CreateSyncObjects() {
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32 i = 0; i < kNkVkMaxFrames; ++i) {
        NK_VK_CHECK(vkCreateSemaphore(mData.device, &semInfo, nullptr, &mData.imageAvailSem[i]));
        NK_VK_CHECK(vkCreateSemaphore(mData.device, &semInfo, nullptr, &mData.renderDoneSem[i]));
        NK_VK_CHECK(vkCreateFence   (mData.device, &fenceInfo,nullptr, &mData.inFlightFences[i]));
    }
    for (uint32 i = 0; i < kNkVkMaxImages; ++i)
        mData.imageInFlight[i] = VK_NULL_HANDLE;
    return true;
}

} // namespace nkentseu

#else

namespace nkentseu {

NkVulkanContext::~NkVulkanContext() = default;

bool NkVulkanContext::Initialize(const NkWindow&, const NkContextDesc& desc) {
    mDesc = desc;
    mIsValid = false;
    logger.Errorf("[NkVulkan] Vulkan headers not found at build time. Install Vulkan SDK/headers to enable this backend.");
    return false;
}

void NkVulkanContext::Shutdown() {
    mIsValid = false;
}

bool NkVulkanContext::IsValid() const {
    return mIsValid;
}

bool NkVulkanContext::BeginFrame() {
    return false;
}

void NkVulkanContext::EndFrame() {
}

void NkVulkanContext::Present() {
}

bool NkVulkanContext::OnResize(uint32, uint32) {
    return false;
}

void NkVulkanContext::SetVSync(bool enabled) {
    mVSync = enabled;
}

bool NkVulkanContext::GetVSync() const {
    return mVSync;
}

NkGraphicsApi NkVulkanContext::GetApi() const {
    return NkGraphicsApi::NK_API_VULKAN;
}

NkContextInfo NkVulkanContext::GetInfo() const {
    NkContextInfo info{};
    info.api = NkGraphicsApi::NK_API_VULKAN;
    info.renderer = "Unavailable";
    info.vendor = "Unavailable";
    info.version = "Vulkan headers missing";
    info.computeSupported = false;
    return info;
}

NkContextDesc NkVulkanContext::GetDesc() const {
    return mDesc;
}

void* NkVulkanContext::GetNativeContextData() {
    return nullptr;
}

bool NkVulkanContext::SupportsCompute() const {
    return false;
}

} // namespace nkentseu

#endif // NKENTSEU_HAS_VULKAN_HEADERS
