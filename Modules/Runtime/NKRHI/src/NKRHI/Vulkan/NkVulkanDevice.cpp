// =============================================================================
// NkRHI_Device_VK.cpp — Backend Vulkan du NkIDevice
// =============================================================================
#ifdef NK_RHI_VK_ENABLED
#include "NkVulkanDevice.h"
#include "NkVulkanCommandBuffer.h"
#include "NKRHI/Core/NkGpuPolicy.h"
#include "NKLogger/NkLog.h"
#include "NKContainers/Associative/NkSet.h"
#include <cstring>
#include <algorithm>
#include <cmath>

#define NK_VK_LOG(...)  logger_src.Infof("[NkRHI_VK] " __VA_ARGS__)
#define NK_VK_ERR(...)  logger_src.Infof("[NkRHI_VK][ERR] " __VA_ARGS__)
#define NK_VK_CHECK(r)  do { VkResult _r=(r); if(_r!=VK_SUCCESS) { NK_VK_ERR("VkResult=%d at %s:%d\n",(int)_r,__FILE__,__LINE__); } } while(0)
#define NK_VK_CHECKRET(r, msg) do { VkResult _r=(r); if(_r!=VK_SUCCESS){ NK_VK_ERR(msg " (err=%d)\n",(int)_r); return false; } } while(0)

namespace nkentseu {
    namespace {

        int ScoreVkDevice(const VkPhysicalDeviceProperties& props, NkGpuPreference pref) {
            int score = 0;
            switch (pref) {
                case NkGpuPreference::NK_HIGH_PERFORMANCE:
                    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 2000;
                    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 500;
                    break;
                case NkGpuPreference::NK_LOW_POWER:
                    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 2000;
                    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 200;
                    break;
                case NkGpuPreference::NK_DEFAULT:
                default:
                    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;
                    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 900;
                    break;
            }
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
                score -= 10000;
            }
            score += static_cast<int>(props.limits.maxImageDimension2D);
            return score;
        }

        bool HasCStr(const NkVector<const char*>& arr, const char* value) {
            for (uint32 i = 0; i < arr.Size(); ++i) {
                if (std::strcmp(arr[i], value) == 0) return true;
            }
            return false;
        }

    } // namespace

    // =============================================================================
    NkVulkanDevice::~NkVulkanDevice() { if (mIsValid) Shutdown(); }

    // =============================================================================
    bool NkVulkanDevice::Initialize(const NkDeviceInitInfo& init) {
        mInit = init;
        NkGpuPolicy::ApplyPreContext(mInit.context);
        const NkVulkanDesc& vkdesc = mInit.context.vulkan;

        mWidth  = NkDeviceInitWidth(init);
        mHeight = NkDeviceInitHeight(init);
        if (mWidth == 0)  mWidth = 1280;
        if (mHeight == 0) mHeight = 720;

        VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        appInfo.pApplicationName = vkdesc.appName ? vkdesc.appName : "Nkentseu";
        appInfo.applicationVersion = vkdesc.appVersion;
        appInfo.pEngineName = vkdesc.engineName ? vkdesc.engineName : "NkEngine";
        appInfo.engineVersion = 1;
        appInfo.apiVersion = vkdesc.apiVersion != 0 ? vkdesc.apiVersion : VK_API_VERSION_1_1;

        NkVector<const char*> instanceExts;
        instanceExts.PushBack(VK_KHR_SURFACE_EXTENSION_NAME);
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        instanceExts.PushBack(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    #elif defined(NKENTSEU_WINDOWING_XLIB)
        instanceExts.PushBack(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
    #elif defined(NKENTSEU_WINDOWING_XCB)
        instanceExts.PushBack(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
    #elif defined(NKENTSEU_WINDOWING_WAYLAND)
        instanceExts.PushBack(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
    #elif defined(NKENTSEU_PLATFORM_ANDROID)
        instanceExts.PushBack(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
    #elif defined(NKENTSEU_PLATFORM_MACOS)
        instanceExts.PushBack(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
        instanceExts.PushBack(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        instanceExts.PushBack(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    #else
        NK_VK_ERR("Unsupported platform for Vulkan surface creation in this build\n");
        return false;
    #endif
        if (vkdesc.debugMessenger && !HasCStr(instanceExts, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
            instanceExts.PushBack(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        for (uint32 i = 0; i < vkdesc.extraInstanceExtCount; ++i) {
            const char* ext = vkdesc.extraInstanceExt[i];
            if (ext && !HasCStr(instanceExts, ext)) {
                instanceExts.PushBack(ext);
            }
        }

        NkVector<const char*> layers;
        if (vkdesc.validationLayers) {
            layers.PushBack("VK_LAYER_KHRONOS_validation");
        }

        VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        ici.pApplicationInfo = &appInfo;
        ici.enabledExtensionCount = (uint32)instanceExts.Size();
        ici.ppEnabledExtensionNames = instanceExts.Data();
        ici.enabledLayerCount = (uint32)layers.Size();
        ici.ppEnabledLayerNames = layers.Empty() ? nullptr : layers.Data();
        NK_VK_CHECKRET(vkCreateInstance(&ici, nullptr, &mInstance), "vkCreateInstance");

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        VkWin32SurfaceCreateInfoKHR sci{};
        sci.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        sci.hinstance = init.surface.hinstance ? init.surface.hinstance : GetModuleHandleW(nullptr);
        sci.hwnd = init.surface.hwnd;
        if (!sci.hwnd) {
            NK_VK_ERR("HWND missing in NkDeviceInitInfo.surface\n");
            return false;
        }
        NK_VK_CHECK(vkCreateWin32SurfaceKHR(mInstance, &sci, nullptr, &mSurface));
    #elif defined(NKENTSEU_WINDOWING_XLIB)
        VkXlibSurfaceCreateInfoKHR sci{};
        sci.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        sci.dpy    = static_cast<Display*>(init.surface.display);
        sci.window = (::Window)init.surface.window;
        NK_VK_CHECK(vkCreateXlibSurfaceKHR(mInstance, &sci, nullptr, &mSurface));
    #elif defined(NKENTSEU_WINDOWING_XCB)
        VkXcbSurfaceCreateInfoKHR sci{};
        sci.sType      = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
        sci.connection = static_cast<xcb_connection_t*>(init.surface.connection);
        sci.window     = (xcb_window_t)init.surface.window;
        NK_VK_CHECK(vkCreateXcbSurfaceKHR(mInstance, &sci, nullptr, &mSurface));
    #elif defined(NKENTSEU_WINDOWING_WAYLAND)
        VkWaylandSurfaceCreateInfoKHR sci{};
        sci.sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        sci.display = static_cast<wl_display*>(init.surface.display);
        sci.surface = static_cast<wl_surface*>(init.surface.surface);
        NK_VK_CHECK(vkCreateWaylandSurfaceKHR(mInstance, &sci, nullptr, &mSurface));
    #elif defined(NKENTSEU_PLATFORM_ANDROID)
        VkAndroidSurfaceCreateInfoKHR sci{};
        sci.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        sci.window = static_cast<ANativeWindow*>(init.surface.nativeWindow);
        NK_VK_CHECK(vkCreateAndroidSurfaceKHR(mInstance, &sci, nullptr, &mSurface));
    #elif defined(NKENTSEU_PLATFORM_MACOS)
        VkMetalSurfaceCreateInfoEXT sci{};
        sci.sType  = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
        sci.pLayer = static_cast<const CAMetalLayer*>(init.surface.metalLayer);
        NK_VK_CHECK(vkCreateMetalSurfaceEXT(mInstance, &sci, nullptr, &mSurface));
    #else
        NK_VK_ERR("No Vulkan surface impl for this platform\n");
        return false;
    #endif

        uint32 gpuCount = 0;
        NK_VK_CHECKRET(vkEnumeratePhysicalDevices(mInstance, &gpuCount, nullptr), "vkEnumeratePhysicalDevices(count)");
        if (gpuCount == 0) {
            NK_VK_ERR("No Vulkan GPU detected\n");
            return false;
        }

        NkVector<VkPhysicalDevice> gpus(gpuCount);
        NK_VK_CHECKRET(vkEnumeratePhysicalDevices(mInstance, &gpuCount, gpus.Data()), "vkEnumeratePhysicalDevices(list)");

        int bestScore = -1000000;
        const uint32 preferredAdapterIndex =
            (vkdesc.preferredAdapterIndex != UINT32_MAX)
                ? vkdesc.preferredAdapterIndex
                : ((mInit.context.gpu.adapterIndex >= 0)
                    ? static_cast<uint32>(mInit.context.gpu.adapterIndex)
                    : UINT32_MAX);
        const NkGpuVendor vendorPreference = mInit.context.gpu.vendorPreference;
        const bool strictVendor = vendorPreference != NkGpuVendor::NK_ANY;

        for (uint32 g = 0; g < gpuCount; ++g) {
            if (preferredAdapterIndex != UINT32_MAX && g != preferredAdapterIndex) {
                continue;
            }
            VkPhysicalDevice gpu = gpus[g];

            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties(gpu, &props);
            if (!mInit.context.gpu.allowSoftwareAdapter &&
                props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
                continue;
            }
            if (strictVendor &&
                !NkGpuPolicy::MatchesVendorPciId(props.vendorID, vendorPreference)) {
                continue;
            }

            uint32 extCount = 0;
            vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extCount, nullptr);
            NkVector<VkExtensionProperties> exts(extCount);
            vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extCount, exts.Data());

            bool hasSwapchain = false;
            for (uint32 e = 0; e < exts.Size(); ++e) {
                if (std::strcmp(exts[e].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                    hasSwapchain = true;
                    break;
                }
            }
            if (!hasSwapchain) continue;

            uint32 qCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &qCount, nullptr);
            NkVector<VkQueueFamilyProperties> qprops(qCount);
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &qCount, qprops.Data());

            uint32 graphics = UINT32_MAX;
            uint32 present  = UINT32_MAX;
            uint32 compute  = UINT32_MAX;
            for (uint32 i = 0; i < qCount; ++i) {
                if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && graphics == UINT32_MAX) graphics = i;
                if ((qprops[i].queueFlags & VK_QUEUE_COMPUTE_BIT)  && compute  == UINT32_MAX) compute  = i;
                VkBool32 canPresent = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, mSurface, &canPresent);
                if (canPresent && present == UINT32_MAX) present = i;
            }

            if (graphics != UINT32_MAX && present != UINT32_MAX) {
                const int score = ScoreVkDevice(props, mInit.context.gpu.preference);
                if (score > bestScore) {
                    bestScore = score;
                    mPhysicalDevice = gpu;
                    mGraphicsFamily = graphics;
                    mPresentFamily  = present;
                    mComputeFamily  = (compute != UINT32_MAX) ? compute : graphics;
                }
            }
        }

        if (!mPhysicalDevice || mGraphicsFamily == UINT32_MAX || mPresentFamily == UINT32_MAX) {
            NK_VK_ERR("No Vulkan GPU supports required queues/swapchain\n");
            return false;
        }
        if (!CreateLogicalDevice()) {
            return false;
        }

        // ── Descriptor pool global ────────────────────────────────────────────────
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

        // ── Command pool one-shot ────────────────────────────────────────────────
        VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cpci.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        cpci.queueFamilyIndex = mGraphicsFamily;
        NK_VK_CHECKRET(vkCreateCommandPool(mDevice, &cpci, nullptr, &mOneShotPool),
                    "vkCreateCommandPool (one-shot)");

        // ── Frame data ────────────────────────────────────────────────────────────
        for (uint32 i=0; i<MAX_FRAMES; i++) {
            VkCommandPoolCreateInfo fcp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            fcp.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            fcp.queueFamilyIndex = mGraphicsFamily;
            NK_VK_CHECK(vkCreateCommandPool(mDevice, &fcp, nullptr, &mFrames[i].cmdPool));

            VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            cbai.commandPool        = mFrames[i].cmdPool;
            cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cbai.commandBufferCount = 1;
            NK_VK_CHECK(vkAllocateCommandBuffers(mDevice, &cbai, &mFrames[i].cmdBuffer));

            VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            NK_VK_CHECK(vkCreateFence(mDevice, &fi, nullptr, &mFrames[i].inFlightFence));

            VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            NK_VK_CHECK(vkCreateSemaphore(mDevice, &si, nullptr, &mFrames[i].imageAvailable));
            NK_VK_CHECK(vkCreateSemaphore(mDevice, &si, nullptr, &mFrames[i].renderFinished));
        }

        // ── Swapchain ─────────────────────────────────────────────────────────────
        CreateSwapchain(mWidth, mHeight);
        QueryCaps();

        mIsValid = true;
        NK_VK_LOG("Initialisé (%u×%u, %u frames in flight)\n", mWidth, mHeight, MAX_FRAMES);
        return true;
    }

    bool NkVulkanDevice::CreateLogicalDevice() {
        const float queuePriority = 1.0f;
        const NkVulkanDesc& vkdesc = mInit.context.vulkan;
        NkSet<uint32> queueFamilies = {mGraphicsFamily, mPresentFamily};
        if (vkdesc.enableComputeQueue) {
            queueFamilies.Insert(mComputeFamily);
        }
        NkVector<VkDeviceQueueCreateInfo> queueInfos;
        queueInfos.Reserve(queueFamilies.Size());
        for (uint32 family : queueFamilies) {
            VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            qci.queueFamilyIndex = family;
            qci.queueCount = 1;
            qci.pQueuePriorities = &queuePriority;
            queueInfos.PushBack(qci);
        }

        NkVector<const char*> deviceExts;
        deviceExts.PushBack(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        for (uint32 i = 0; i < vkdesc.extraDeviceExtCount; ++i) {
            const char* ext = vkdesc.extraDeviceExt[i];
            if (ext && !HasCStr(deviceExts, ext)) {
                deviceExts.PushBack(ext);
            }
        }
        VkPhysicalDeviceFeatures features{};

        VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        dci.queueCreateInfoCount = (uint32)queueInfos.Size();
        dci.pQueueCreateInfos = queueInfos.Data();
        dci.enabledExtensionCount = (uint32)deviceExts.Size();
        dci.ppEnabledExtensionNames = deviceExts.Data();
        dci.pEnabledFeatures = &features;

        NK_VK_CHECKRET(vkCreateDevice(mPhysicalDevice, &dci, nullptr, &mDevice), "vkCreateDevice");

        vkGetDeviceQueue(mDevice, mGraphicsFamily, 0, &mGraphicsQueue);
        vkGetDeviceQueue(mDevice, mPresentFamily, 0, &mPresentQueue);
        if (vkdesc.enableComputeQueue) {
            vkGetDeviceQueue(mDevice, mComputeFamily, 0, &mComputeQueue);
        } else {
            mComputeQueue = mGraphicsQueue;
        }

        if (!mGraphicsQueue || !mPresentQueue || (vkdesc.enableComputeQueue && !mComputeQueue)) {
            NK_VK_ERR("Queues Vulkan invalides apres vkGetDeviceQueue\n");
            vkDestroyDevice(mDevice, nullptr);
            mDevice = VK_NULL_HANDLE;
            return false;
        }
        return true;
    }

    // =============================================================================
    void NkVulkanDevice::Shutdown() {
        WaitIdle();
        DestroySwapchain();

        for (auto& [id,b] : mBuffers)   {
            vkDestroyBuffer(mDevice,b.buffer,nullptr);
            FreeMemory(b.alloc);
        }
        for (auto& [id,t] : mTextures) if (!t.isSwapchain) {
            vkDestroyImageView(mDevice,t.view,nullptr);
            vkDestroyImage(mDevice,t.image,nullptr);
            FreeMemory(t.alloc);
        }
        for (auto& [id,s] : mSamplers)  vkDestroySampler(mDevice,s.sampler,nullptr);
        for (auto& [id,sh]: mShaders)
            for (auto& st:sh.stages) vkDestroyShaderModule(mDevice,st.module,nullptr);
        for (auto& [id,p] : mPipelines) {
            vkDestroyPipeline(mDevice,p.pipeline,nullptr);
            vkDestroyPipelineLayout(mDevice,p.layout,nullptr);
        }
        for (auto& [id,rp]: mRenderPasses) vkDestroyRenderPass(mDevice,rp.renderPass,nullptr);
        for (auto& [id,fb]: mFramebuffers) vkDestroyFramebuffer(mDevice,fb.framebuffer,nullptr);
        for (auto& [id,dl]: mDescLayouts)  vkDestroyDescriptorSetLayout(mDevice,dl.layout,nullptr);
        for (auto& [id,f] : mFences)       vkDestroyFence(mDevice,f.fence,nullptr);

        for (uint32 i=0;i<MAX_FRAMES;i++) {
            vkDestroyFence    (mDevice,mFrames[i].inFlightFence,nullptr);
            vkDestroySemaphore(mDevice,mFrames[i].imageAvailable,nullptr);
            vkDestroySemaphore(mDevice,mFrames[i].renderFinished,nullptr);
            vkDestroyCommandPool(mDevice,mFrames[i].cmdPool,nullptr);
        }
        vkDestroyDescriptorPool(mDevice,mDescPool,nullptr);
        vkDestroyCommandPool(mDevice,mOneShotPool,nullptr);
        vkDestroyDevice(mDevice, nullptr);
        mDevice = VK_NULL_HANDLE;
        mGraphicsQueue = VK_NULL_HANDLE;
        mPresentQueue = VK_NULL_HANDLE;
        mComputeQueue = VK_NULL_HANDLE;

        mIsValid = false;
        NK_VK_LOG("Shutdown\n");
    }

    // =============================================================================
    void NkVulkanDevice::CreateSwapchain(uint32 w, uint32 h) {
        // Capacités swapchain
        VkSurfaceCapabilitiesKHR caps;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &caps);

        uint32 imgCount = std::max(caps.minImageCount+1, MAX_FRAMES);
        if (caps.maxImageCount>0) imgCount=std::min(imgCount,caps.maxImageCount);

        mSwapExtent = {w,h};
        mSwapExtent.width  = std::clamp(w, caps.minImageExtent.width,  caps.maxImageExtent.width);
        mSwapExtent.height = std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);

        // Choisir le format
        uint32 fmtCnt=0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice,mSurface,&fmtCnt,nullptr);
        NkVector<VkSurfaceFormatKHR> fmts(fmtCnt);
        vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice,mSurface,&fmtCnt,fmts.Data());
        mSwapFormat = fmts[0].format;
        VkColorSpaceKHR colorSpace = fmts[0].colorSpace;
        for (auto& f:fmts) {
            if (f.format==VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace==VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                mSwapFormat=f.format; colorSpace=f.colorSpace; break;
            }
        }

        // Present mode
        uint32 pmCnt=0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice,mSurface,&pmCnt,nullptr);
        NkVector<VkPresentModeKHR> pms(pmCnt);
        vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice,mSurface,&pmCnt,pms.Data());
        VkPresentModeKHR pm = VK_PRESENT_MODE_FIFO_KHR; // VSync garanti
        for (auto p:pms) if (p==VK_PRESENT_MODE_MAILBOX_KHR) { pm=p; break; }

        VkSwapchainCreateInfoKHR sci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        sci.surface          = mSurface;
        sci.minImageCount    = imgCount;
        sci.imageFormat      = mSwapFormat;
        sci.imageColorSpace  = colorSpace;
        sci.imageExtent      = mSwapExtent;
        sci.imageArrayLayers = 1;
        sci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        uint32 familyIndices[] = { mGraphicsFamily, mPresentFamily };
        if (mGraphicsFamily != mPresentFamily) {
            sci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            sci.queueFamilyIndexCount = 2;
            sci.pQueueFamilyIndices = familyIndices;
        } else {
            sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        sci.preTransform     = caps.currentTransform;
        sci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        sci.presentMode      = pm;
        sci.clipped          = VK_TRUE;
        NK_VK_CHECK(vkCreateSwapchainKHR(mDevice,&sci,nullptr,&mSwapchain));

        // Images swapchain
        uint32 cnt=0;
        vkGetSwapchainImagesKHR(mDevice,mSwapchain,&cnt,nullptr);
        mSwapImages.Resize(cnt);
        vkGetSwapchainImagesKHR(mDevice,mSwapchain,&cnt,mSwapImages.Data());
        mSwapViews.Resize(cnt);

        for (uint32 i=0;i<cnt;i++) {
            VkImageViewCreateInfo ivci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            ivci.image    = mSwapImages[i];
            ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ivci.format   = mSwapFormat;
            ivci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
            NK_VK_CHECK(vkCreateImageView(mDevice,&ivci,nullptr,&mSwapViews[i]));
        }

        // Depth buffer
        NkTextureDesc dd=NkTextureDesc::DepthStencil(mSwapExtent.width,mSwapExtent.height);
        auto depthHandle=CreateTexture(dd);
        mDepthTexId=depthHandle.id;

        CreateSwapchainRenderPassAndFramebuffers();
    }

    void NkVulkanDevice::CreateSwapchainRenderPassAndFramebuffers() {
        // Render pass swapchain
        NkRenderPassDesc rpd;
        NkAttachmentDesc col; col.format=GetSwapchainFormat(); col.loadOp=NkLoadOp::NK_CLEAR;
        rpd.AddColor(col).SetDepth(NkAttachmentDesc::Depth());
        mCreatingSwapchainRenderPass = true;
        mSwapchainRP = CreateRenderPass(rpd);
        mCreatingSwapchainRenderPass = false;

        // Framebuffers (un par image swapchain)
        uint32 n=(uint32)mSwapImages.Size();
        mSwapchainFBs.Resize(n);
        for (uint32 i=0;i<n;i++) {
            // Enregistrer les image views du swapchain dans mTextures
            uint64 colorId=NextId();
            NkVkTexture swTex{};
            swTex.image=mSwapImages[i]; swTex.view=mSwapViews[i];
            swTex.desc=NkTextureDesc::RenderTarget(mSwapExtent.width,mSwapExtent.height,GetSwapchainFormat());
            swTex.isSwapchain=true;
            swTex.layout=VK_IMAGE_LAYOUT_UNDEFINED;
            mTextures[colorId]=swTex;
            NkTextureHandle colorH; colorH.id=colorId;

            NkTextureHandle depthH; depthH.id=mDepthTexId;

            NkFramebufferDesc fbd;
            fbd.renderPass=mSwapchainRP;
            fbd.colorAttachments.PushBack(colorH);
            fbd.depthAttachment=depthH;
            fbd.width=mSwapExtent.width; fbd.height=mSwapExtent.height;
            mSwapchainFBs[i]=CreateFramebuffer(fbd);
        }
    }

    void NkVulkanDevice::DestroySwapchain() {
        for (auto& fb:mSwapchainFBs) DestroyFramebuffer(fb);
        mSwapchainFBs.Clear();
        if (mSwapchainRP.IsValid()) DestroyRenderPass(mSwapchainRP);

        // Depth
        NkTextureHandle dh; dh.id=mDepthTexId;
        if (dh.IsValid()) DestroyTexture(dh);
        mDepthTexId=0;

        // Swapchain image views (isSwapchain=true → pas de vkDestroyImage)
        for (auto& v:mSwapViews) if(v) vkDestroyImageView(mDevice,v,nullptr);
        mSwapViews.Clear();

        if (mSwapchain) { vkDestroySwapchainKHR(mDevice,mSwapchain,nullptr); mSwapchain=VK_NULL_HANDLE; }
        mSwapImages.Clear();
    }

    void NkVulkanDevice::RecreateSwapchain(uint32 w, uint32 h) {
        vkDeviceWaitIdle(mDevice);
        DestroySwapchain();
        CreateSwapchain(w,h);
    }

    // =============================================================================
    // Memory allocation
    // =============================================================================
    uint32 NkVulkanDevice::FindMemoryType(uint32 filter, VkMemoryPropertyFlags props) const {
        VkPhysicalDeviceMemoryProperties mp;
        vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice,&mp);
        for (uint32 i=0;i<mp.memoryTypeCount;i++)
            if ((filter&(1<<i)) && (mp.memoryTypes[i].propertyFlags&props)==props) return i;
        return UINT32_MAX;
    }

    NkVkAllocation NkVulkanDevice::AllocateMemory(VkMemoryRequirements req, VkMemoryPropertyFlags props) {
        VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mai.allocationSize  = req.size;
        mai.memoryTypeIndex = FindMemoryType(req.memoryTypeBits, props);
        NkVkAllocation a; a.size=req.size;
        NK_VK_CHECK(vkAllocateMemory(mDevice,&mai,nullptr,&a.memory));
        if (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            vkMapMemory(mDevice,a.memory,0,req.size,0,&a.mapped);
        return a;
    }

    void NkVulkanDevice::FreeMemory(NkVkAllocation& a) {
        if (a.mapped) vkUnmapMemory(mDevice,a.memory);
        if (a.memory) vkFreeMemory(mDevice,a.memory,nullptr);
        a={};
    }

    // =============================================================================
    // One-shot command buffer
    // =============================================================================
    VkCommandBuffer NkVulkanDevice::BeginOneShot() {
        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool=mOneShotPool; ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount=1;
        VkCommandBuffer cmd=VK_NULL_HANDLE;
        vkAllocateCommandBuffers(mDevice,&ai,&cmd);
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd,&bi);
        return cmd;
    }
    void NkVulkanDevice::EndOneShot(VkCommandBuffer cmd) {
        vkEndCommandBuffer(cmd);
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount=1; si.pCommandBuffers=&cmd;
        vkQueueSubmit(mGraphicsQueue,1,&si,VK_NULL_HANDLE);
        vkQueueWaitIdle(mGraphicsQueue);
        vkFreeCommandBuffers(mDevice,mOneShotPool,1,&cmd);
    }

    // =============================================================================
    // Image transition
    // =============================================================================
    void NkVulkanDevice::TransitionImage(VkCommandBuffer cmd, VkImage img,
        VkImageLayout from, VkImageLayout to, VkImageAspectFlags aspect,
        uint32 baseMip, uint32 mips, uint32 baseLayer, uint32 layers) {

        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout=from; b.newLayout=to;
        b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        b.image=img;
        b.subresourceRange={aspect,baseMip,mips==UINT32_MAX?VK_REMAINING_MIP_LEVELS:mips,
                            baseLayer,layers==UINT32_MAX?VK_REMAINING_ARRAY_LAYERS:layers};

        VkPipelineStageFlags src=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkPipelineStageFlags dst=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        // Access masks selon les layouts
        switch(from) {
            case VK_IMAGE_LAYOUT_UNDEFINED:                b.srcAccessMask=0; break;
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:     b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; src=VK_PIPELINE_STAGE_TRANSFER_BIT; break;
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: b.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; src=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; break;
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: b.srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; src=VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT; break;
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: b.srcAccessMask=VK_ACCESS_SHADER_READ_BIT; src=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; break;
            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:          b.srcAccessMask=VK_ACCESS_MEMORY_READ_BIT; break;
            default: break;
        }
        switch(to) {
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:     b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; dst=VK_PIPELINE_STAGE_TRANSFER_BIT; break;
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: b.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; dst=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; break;
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: b.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; dst=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; break;
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT; dst=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; break;
            case VK_IMAGE_LAYOUT_GENERAL:                  b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT; dst=VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; break;
            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:          b.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT; break;
            default: break;
        }
        vkCmdPipelineBarrier(cmd,src,dst,0,0,nullptr,0,nullptr,1,&b);
    }

    // =============================================================================
    // Buffers
    // =============================================================================
    NkBufferHandle NkVulkanDevice::CreateBuffer(const NkBufferDesc& desc) {
        bool uploadAfterCreate = false;
        const void* uploadData = desc.initialData;
        NkBufferHandle h;

        {
        threading::NkScopedLock lock(mMutex);
        VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bci.size = desc.sizeBytes;

        // Usage flags
        bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if (NkHasFlag(desc.bindFlags,NkBindFlags::NK_VERTEX_BUFFER))   bci.usage|=VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if (NkHasFlag(desc.bindFlags,NkBindFlags::NK_INDEX_BUFFER))    bci.usage|=VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if (NkHasFlag(desc.bindFlags,NkBindFlags::NK_UNIFORM_BUFFER))  bci.usage|=VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        if (NkHasFlag(desc.bindFlags,NkBindFlags::NK_STORAGE_BUFFER))  bci.usage|=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if (NkHasFlag(desc.bindFlags,NkBindFlags::NK_INDIRECT_ARGS))   bci.usage|=VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        // Default flags selon le type
        switch(desc.type) {
            case NkBufferType::NK_VERTEX:  bci.usage|=VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; break;
            case NkBufferType::NK_INDEX:   bci.usage|=VK_BUFFER_USAGE_INDEX_BUFFER_BIT;  break;
            case NkBufferType::NK_UNIFORM: bci.usage|=VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;break;
            case NkBufferType::NK_STORAGE: bci.usage|=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;break;
            case NkBufferType::NK_INDIRECT:bci.usage|=VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;break;
            default: break;
        }
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer buf=VK_NULL_HANDLE;
        NK_VK_CHECK(vkCreateBuffer(mDevice,&bci,nullptr,&buf));

        VkMemoryRequirements req; vkGetBufferMemoryRequirements(mDevice,buf,&req);
        VkMemoryPropertyFlags memProps;
        switch(desc.usage) {
            case NkResourceUsage::NK_UPLOAD:   memProps=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT; break;
            case NkResourceUsage::NK_READBACK: memProps=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_CACHED_BIT;   break;
            default:                        memProps=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT; break;
        }
        auto alloc=AllocateMemory(req,memProps);
        vkBindBufferMemory(mDevice,buf,alloc.memory,0);

        if (desc.debugName) {
            VkDebugUtilsObjectNameInfoEXT ni{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
            ni.objectType=VK_OBJECT_TYPE_BUFFER; ni.objectHandle=(uint64)buf; ni.pObjectName=desc.debugName;
            // vkSetDebugUtilsObjectNameEXT(mDevice,&ni); // Si extension disponible
        }

        // Upload initial
        if (uploadData) {
            if (alloc.mapped) {
                memcpy(alloc.mapped, uploadData, (size_t)desc.sizeBytes);
            } else {
                // Important: ne pas allouer de staging via CreateBuffer sous ce lock.
                uploadAfterCreate = true;
            }
        }

        uint64 hid=NextId();
        mBuffers[hid]={buf,alloc,desc};
        h.id=hid;
        }

        if (uploadAfterCreate) {
            WriteBuffer(h, uploadData, desc.sizeBytes, 0);
        }
        return h;
    }

    void NkVulkanDevice::DestroyBuffer(NkBufferHandle& h) {
        threading::NkScopedLock lock(mMutex);
        auto it=mBuffers.Find(h.id); if (!it) return;
        vkDestroyBuffer(mDevice,it->buffer,nullptr);
        FreeMemory(it->alloc);
        mBuffers.Erase(h.id); h.id=0;
    }

    bool NkVulkanDevice::WriteBuffer(NkBufferHandle buf,const void* data,uint64 sz,uint64 off) {
        auto it=mBuffers.Find(buf.id); if (!it) return false;
        if (it->alloc.mapped) {
            memcpy((uint8*)it->alloc.mapped+off, data, (size_t)sz);
            return true;
        }
        // Via staging
        NkBufferDesc sd=NkBufferDesc::Staging(sz);
        auto stageH=CreateBuffer(sd);
        auto& stage=mBuffers[stageH.id];
        memcpy(stage.alloc.mapped, data, (size_t)sz);
        auto cmd=BeginOneShot();
        VkBufferCopy cp{0,off,sz};
        vkCmdCopyBuffer(cmd,stage.buffer,it->buffer,1,&cp);
        EndOneShot(cmd);
        DestroyBuffer(stageH);
        return true;
    }

    bool NkVulkanDevice::WriteBufferAsync(NkBufferHandle buf,const void* data,uint64 sz,uint64 off) {
        auto it=mBuffers.Find(buf.id); if (!it) return false;
        if (it->alloc.mapped) { memcpy((uint8*)it->alloc.mapped+off,data,(size_t)sz); return true; }
        return WriteBuffer(buf,data,sz,off);
    }

    bool NkVulkanDevice::ReadBuffer(NkBufferHandle buf,void* out,uint64 sz,uint64 off) {
        auto it=mBuffers.Find(buf.id); if (!it) return false;
        if (it->alloc.mapped) { memcpy(out,(uint8*)it->alloc.mapped+off,(size_t)sz); return true; }
        // Readback via staging
        NkBufferDesc sd=NkBufferDesc::Staging(sz); sd.usage=NkResourceUsage::NK_READBACK;
        auto stageH=CreateBuffer(sd);
        auto& stage=mBuffers[stageH.id];
        auto cmd=BeginOneShot();
        VkBufferCopy cp{off,0,sz};
        vkCmdCopyBuffer(cmd,it->buffer,stage.buffer,1,&cp);
        EndOneShot(cmd);
        memcpy(out,stage.alloc.mapped,(size_t)sz);
        DestroyBuffer(stageH);
        return true;
    }

    NkMappedMemory NkVulkanDevice::MapBuffer(NkBufferHandle buf,uint64 off,uint64 sz) {
        auto it=mBuffers.Find(buf.id); if (!it) return {};
        uint64 mapSz=sz>0?sz:it->desc.sizeBytes-off;
        if (it->alloc.mapped)
            return {(uint8*)it->alloc.mapped+off,mapSz};
        void* ptr=nullptr;
        vkMapMemory(mDevice,it->alloc.memory,off,mapSz,0,&ptr);
        return {ptr,mapSz};
    }
    void NkVulkanDevice::UnmapBuffer(NkBufferHandle buf) {
        auto it=mBuffers.Find(buf.id); if (!it) return;
        if (!it->alloc.mapped) vkUnmapMemory(mDevice,it->alloc.memory);
    }

    // =============================================================================
    // Textures
    // =============================================================================
    NkTextureHandle NkVulkanDevice::CreateTexture(const NkTextureDesc& desc) {
        threading::NkScopedLock lock(mMutex);
        VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ici.imageType   = ToVkImageType(desc.type);
        ici.format      = ToVkFormat(desc.format);
        ici.extent      = {desc.width,desc.height,desc.depth};
        ici.mipLevels   = desc.mipLevels==0 ? (uint32)(floor(log2(std::max(desc.width,desc.height)))+1) : desc.mipLevels;
        ici.arrayLayers = desc.arrayLayers;
        ici.samples     = ToVkSamples(desc.samples);
        ici.tiling      = VK_IMAGE_TILING_OPTIMAL;
        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        ici.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (NkHasFlag(desc.bindFlags,NkBindFlags::NK_SHADER_RESOURCE))  ici.usage|=VK_IMAGE_USAGE_SAMPLED_BIT;
        if (NkHasFlag(desc.bindFlags,NkBindFlags::NK_UNORDERED_ACCESS)) ici.usage|=VK_IMAGE_USAGE_STORAGE_BIT;
        if (NkHasFlag(desc.bindFlags,NkBindFlags::NK_RENDER_TARGET))    ici.usage|=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (NkHasFlag(desc.bindFlags,NkBindFlags::NK_DEPTH_STENCIL))    ici.usage|=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if (desc.type==NkTextureType::NK_CUBE||desc.type==NkTextureType::NK_CUBE_ARRAY)
            ici.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

        VkImage img=VK_NULL_HANDLE;
        NK_VK_CHECK(vkCreateImage(mDevice,&ici,nullptr,&img));

        VkMemoryRequirements req; vkGetImageMemoryRequirements(mDevice,img,&req);
        auto alloc=AllocateMemory(req,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkBindImageMemory(mDevice,img,alloc.memory,0);

        // Image view
        VkImageAspectFlags aspect = NkFormatIsDepth(desc.format)
            ? (NkFormatHasStencil(desc.format)?VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT:VK_IMAGE_ASPECT_DEPTH_BIT)
            : VK_IMAGE_ASPECT_COLOR_BIT;

        VkImageViewCreateInfo ivci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ivci.image=img;
        ivci.viewType=ToVkImageViewType(desc.type,desc.type==NkTextureType::NK_CUBE||desc.type==NkTextureType::NK_CUBE_ARRAY,desc.arrayLayers);
        ivci.format=ici.format;
        ivci.subresourceRange={aspect,0,ici.mipLevels,0,desc.arrayLayers};
        VkImageView view=VK_NULL_HANDLE;
        NK_VK_CHECK(vkCreateImageView(mDevice,&ivci,nullptr,&view));

        // Upload données initiales
        if (desc.initialData) {
            uint32 rowPitch=desc.rowPitch>0?desc.rowPitch:desc.width*NkFormatBytesPerPixel(desc.format);
            uint64 imgSz=rowPitch*desc.height;
            NkBufferDesc sd=NkBufferDesc::Staging(imgSz);
            auto stageH=CreateBuffer(sd);
            auto& stage=mBuffers[stageH.id];
            memcpy(stage.alloc.mapped,desc.initialData,(size_t)imgSz);
            auto cmd=BeginOneShot();
            TransitionImage(cmd,img,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,aspect);
            VkBufferImageCopy cp{};
            cp.imageSubresource={aspect,0,0,1};
            cp.imageExtent={desc.width,desc.height,1};
            vkCmdCopyBufferToImage(cmd,stage.buffer,img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&cp);
            if (ici.mipLevels>1) {
                // Blit mip chain
                for (uint32 m=1;m<ici.mipLevels;m++) {
                    TransitionImage(cmd,img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,aspect,m-1,1);
                    VkImageBlit blit{};
                    blit.srcSubresource={aspect,m-1,0,1};
                    blit.dstSubresource={aspect,m,0,1};
                    blit.srcOffsets[1]={
                        static_cast<int32>(std::max(1u, desc.width  >> (m-1))),
                        static_cast<int32>(std::max(1u, desc.height >> (m-1))),
                        1
                    };
                    blit.dstOffsets[1]={
                        static_cast<int32>(std::max(1u, desc.width  >> m)),
                        static_cast<int32>(std::max(1u, desc.height >> m)),
                        1
                    };
                    vkCmdBlitImage(cmd,img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&blit,VK_FILTER_LINEAR);
                    TransitionImage(cmd,img,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,aspect,m-1,1);
                }
                TransitionImage(cmd,img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,aspect,ici.mipLevels-1,1);
            } else {
                TransitionImage(cmd,img,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,aspect);
            }
            EndOneShot(cmd);
            DestroyBuffer(stageH);
        } else {
            // Transition vers le layout attendu
            VkImageLayout targetLayout = NkFormatIsDepth(desc.format)
                ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                : (NkHasFlag(desc.bindFlags,NkBindFlags::NK_RENDER_TARGET)
                ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            if (targetLayout!=VK_IMAGE_LAYOUT_UNDEFINED) {
                auto cmd=BeginOneShot();
                TransitionImage(cmd,img,VK_IMAGE_LAYOUT_UNDEFINED,targetLayout,aspect);
                EndOneShot(cmd);
            }
        }

        uint64 hid=NextId();
        NkVkTexture t; t.image=img; t.view=view; t.alloc=alloc; t.desc=desc;
        t.layout=desc.initialData?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED;
        mTextures[hid]=t;
        NkTextureHandle h; h.id=hid; return h;
    }

    void NkVulkanDevice::DestroyTexture(NkTextureHandle& h) {
        threading::NkScopedLock lock(mMutex);
        auto it=mTextures.Find(h.id); if (!it) return;
        if (!it->isSwapchain) {
            vkDestroyImageView(mDevice,it->view,nullptr);
            vkDestroyImage(mDevice,it->image,nullptr);
            FreeMemory(it->alloc);
        }
        mTextures.Erase(h.id); h.id=0;
    }

    bool NkVulkanDevice::WriteTexture(NkTextureHandle t,const void* p,uint32 rp) {
        auto it=mTextures.Find(t.id); if (!it) return false;
        auto& desc=it->desc;
        return WriteTextureRegion(t,p,0,0,0,desc.width,desc.height,1,0,0,rp);
    }

    bool NkVulkanDevice::WriteTextureRegion(NkTextureHandle t,const void* pixels,
        uint32 x,uint32 y,uint32 z,uint32 w,uint32 h,uint32 d2,
        uint32 mip,uint32 layer,uint32 rowPitch) {
        auto it=mTextures.Find(t.id); if (!it) return false;
        auto& desc=it->desc;
        uint32 bpp=NkFormatBytesPerPixel(desc.format);
        uint32 rp=rowPitch>0?rowPitch:w*bpp;
        uint64 sz=(uint64)rp*h*d2;
        NkBufferDesc sd=NkBufferDesc::Staging(sz);
        auto stageH=CreateBuffer(sd);
        auto& stage=mBuffers[stageH.id];
        memcpy(stage.alloc.mapped,pixels,(size_t)sz);
        VkImageAspectFlags aspect=NkFormatIsDepth(desc.format)?VK_IMAGE_ASPECT_DEPTH_BIT:VK_IMAGE_ASPECT_COLOR_BIT;
        auto cmd=BeginOneShot();
        TransitionImage(cmd,it->image,it->layout,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,aspect,mip,1,layer,1);
        VkBufferImageCopy cp{}; cp.imageSubresource={aspect,mip,layer,1};
        cp.imageOffset={( int32)x,(int32)y,(int32)z}; cp.imageExtent={w,h,d2};
        vkCmdCopyBufferToImage(cmd,stage.buffer,it->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&cp);
        TransitionImage(cmd,it->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,aspect,mip,1,layer,1);
        EndOneShot(cmd);
        it->layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        DestroyBuffer(stageH);
        return true;
    }

    bool NkVulkanDevice::GenerateMipmaps(NkTextureHandle t, NkFilter filter) {
        auto it=mTextures.Find(t.id); if (!it) return false;
        auto& desc=it->desc;
        auto cmd=BeginOneShot();
        VkImageAspectFlags aspect=NkFormatIsDepth(desc.format)?VK_IMAGE_ASPECT_DEPTH_BIT:VK_IMAGE_ASPECT_COLOR_BIT;
        uint32 mips=desc.mipLevels;
        TransitionImage(cmd,it->image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,aspect,0,1);
        for (uint32 m=1;m<mips;m++) {
            TransitionImage(cmd,it->image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,aspect,m,1);
            VkImageBlit blit{};
            blit.srcSubresource={aspect,m-1,0,desc.arrayLayers};
            blit.dstSubresource={aspect,m,  0,desc.arrayLayers};
            blit.srcOffsets[1]={
                static_cast<int32>(std::max(1u, desc.width  >> (m-1))),
                static_cast<int32>(std::max(1u, desc.height >> (m-1))),
                1
            };
            blit.dstOffsets[1]={
                static_cast<int32>(std::max(1u, desc.width  >> m)),
                static_cast<int32>(std::max(1u, desc.height >> m)),
                1
            };
            vkCmdBlitImage(cmd,it->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            it->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&blit,
                            filter==NkFilter::NK_NEAREST?VK_FILTER_NEAREST:VK_FILTER_LINEAR);
            TransitionImage(cmd,it->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,aspect,m,1);
        }
        TransitionImage(cmd,it->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,aspect,0,mips);
        EndOneShot(cmd);
        return true;
    }

    // =============================================================================
    // Samplers
    // =============================================================================
    NkSamplerHandle NkVulkanDevice::CreateSampler(const NkSamplerDesc& d) {
        threading::NkScopedLock lock(mMutex);
        VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sci.magFilter     = ToVkFilter(d.magFilter);
        sci.minFilter     = ToVkFilter(d.minFilter);
        sci.mipmapMode    = ToVkMipFilter(d.mipFilter);
        sci.addressModeU  = ToVkAddressMode(d.addressU);
        sci.addressModeV  = ToVkAddressMode(d.addressV);
        sci.addressModeW  = ToVkAddressMode(d.addressW);
        sci.mipLodBias    = d.mipLodBias;
        sci.minLod        = d.minLod; sci.maxLod=d.maxLod;
        sci.anisotropyEnable = d.maxAnisotropy>1.f?VK_TRUE:VK_FALSE;
        sci.maxAnisotropy    = d.maxAnisotropy;
        sci.compareEnable    = d.compareEnable?VK_TRUE:VK_FALSE;
        sci.compareOp        = ToVkCompareOp(d.compareOp);
        VkSampler s=VK_NULL_HANDLE;
        NK_VK_CHECK(vkCreateSampler(mDevice,&sci,nullptr,&s));
        uint64 hid=NextId(); mSamplers[hid]={s};
        NkSamplerHandle h; h.id=hid; return h;
    }
    void NkVulkanDevice::DestroySampler(NkSamplerHandle& h) {
        threading::NkScopedLock lock(mMutex);
        auto it=mSamplers.Find(h.id); if (!it) return;
        vkDestroySampler(mDevice,it->sampler,nullptr);
        mSamplers.Erase(h.id); h.id=0;
    }

    // =============================================================================
    // Shaders (SPIR-V uniquement en Vulkan)
    // =============================================================================
    NkShaderHandle NkVulkanDevice::CreateShader(const NkShaderDesc& desc) {
        threading::NkScopedLock lock(mMutex);
        NkVkShader sh;
        for (uint32 i=0;i<desc.stages.Size();i++) {
            auto& s=desc.stages[i];
            if (!s.spirvData || s.spirvSize==0) {
                NK_VK_ERR("Vulkan nécessite SPIR-V (stage %d)\n",i);
                continue;
            }
            VkShaderModuleCreateInfo smci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            smci.codeSize=(size_t)s.spirvSize;
            smci.pCode=(const uint32*)s.spirvData;
            VkShaderModule mod=VK_NULL_HANDLE;
            NK_VK_CHECK(vkCreateShaderModule(mDevice,&smci,nullptr,&mod));
            sh.stages.PushBack({mod,ToVkShaderStage(s.stage),s.entryPoint?s.entryPoint:"main"});
        }
        uint64 hid=NextId(); mShaders[hid]=sh;
        NkShaderHandle h; h.id=hid; return h;
    }
    void NkVulkanDevice::DestroyShader(NkShaderHandle& h) {
        threading::NkScopedLock lock(mMutex);
        auto it=mShaders.Find(h.id); if (!it) return;
        for (auto& s:it->stages) vkDestroyShaderModule(mDevice,s.module,nullptr);
        mShaders.Erase(h.id); h.id=0;
    }

    // =============================================================================
    // Pipelines
    // =============================================================================
    NkPipelineHandle NkVulkanDevice::CreateGraphicsPipeline(const NkGraphicsPipelineDesc& d) {
        threading::NkScopedLock lock(mMutex);
        auto sit=mShaders.Find(d.shader.id); if (!sit) return {};
        auto rpit=mRenderPasses.Find(d.renderPass.id); if (!rpit) return {};
        auto toVkStageFlags = [](NkShaderStage stages) -> VkShaderStageFlags {
            const uint32 bits = (uint32)stages;
            VkShaderStageFlags out = 0;
            if (bits & (uint32)NkShaderStage::NK_VERTEX)    out |= VK_SHADER_STAGE_VERTEX_BIT;
            if (bits & (uint32)NkShaderStage::NK_FRAGMENT)  out |= VK_SHADER_STAGE_FRAGMENT_BIT;
            if (bits & (uint32)NkShaderStage::NK_GEOMETRY)  out |= VK_SHADER_STAGE_GEOMETRY_BIT;
            if (bits & (uint32)NkShaderStage::NK_TESS_CTRL) out |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            if (bits & (uint32)NkShaderStage::NK_TESS_EVAL) out |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            if (bits & (uint32)NkShaderStage::NK_COMPUTE)   out |= VK_SHADER_STAGE_COMPUTE_BIT;
#ifdef VK_SHADER_STAGE_MESH_BIT_EXT
            if (bits & (uint32)NkShaderStage::NK_MESH)      out |= VK_SHADER_STAGE_MESH_BIT_EXT;
#endif
#ifdef VK_SHADER_STAGE_TASK_BIT_EXT
            if (bits & (uint32)NkShaderStage::NK_TASK)      out |= VK_SHADER_STAGE_TASK_BIT_EXT;
#endif
            return out != 0 ? out : VK_SHADER_STAGE_ALL;
        };

        // Shader stages
        NkVector<VkPipelineShaderStageCreateInfo> stages;
        for (auto& s:sit->stages) {
            VkPipelineShaderStageCreateInfo ssci{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            ssci.stage=s.stage; ssci.module=s.module; ssci.pName=s.entryPoint.c_str();
            stages.PushBack(ssci);
        }

        // Vertex input
        NkVector<VkVertexInputBindingDescription>   bindings;
        NkVector<VkVertexInputAttributeDescription> attribs;
        for (uint32 i=0;i<d.vertexLayout.bindings.Size();i++) {
            auto& b=d.vertexLayout.bindings[i];
            bindings.PushBack({b.binding,b.stride,ToVkInputRate(b.perInstance)});
        }
        for (uint32 i=0;i<d.vertexLayout.attributes.Size();i++) {
            auto& a=d.vertexLayout.attributes[i];
            attribs.PushBack({a.location,a.binding,ToVkVertexFormat(a.format),a.offset});
        }
        VkPipelineVertexInputStateCreateInfo visci{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        visci.vertexBindingDescriptionCount=(uint32)bindings.Size(); visci.pVertexBindingDescriptions=bindings.Data();
        visci.vertexAttributeDescriptionCount=(uint32)attribs.Size(); visci.pVertexAttributeDescriptions=attribs.Data();

        VkPipelineInputAssemblyStateCreateInfo iasci{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        iasci.topology=ToVkTopology(d.topology);
        iasci.primitiveRestartEnable=VK_FALSE;

        // Viewport dynamique
        VkPipelineViewportStateCreateInfo vpsci{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        vpsci.viewportCount=1; vpsci.scissorCount=1;

        VkPipelineRasterizationStateCreateInfo rsci{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rsci.polygonMode=ToVkPolygonMode(d.rasterizer.fillMode);
        rsci.cullMode=ToVkCullMode(d.rasterizer.cullMode);
        rsci.frontFace=ToVkFrontFace(d.rasterizer.frontFace);
        rsci.depthClampEnable=d.rasterizer.depthClip?VK_FALSE:VK_TRUE;
        rsci.depthBiasEnable=d.rasterizer.depthBiasConst!=0.f||d.rasterizer.depthBiasSlope!=0.f;
        rsci.depthBiasConstantFactor=d.rasterizer.depthBiasConst;
        rsci.depthBiasSlopeFactor=d.rasterizer.depthBiasSlope;
        rsci.depthBiasClamp=d.rasterizer.depthBiasClamp;
        rsci.lineWidth=1.f;

        VkPipelineMultisampleStateCreateInfo msci{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        msci.rasterizationSamples=ToVkSamples(d.samples);
        msci.alphaToCoverageEnable=d.blend.alphaToCoverage;

        // Depth/stencil
        VkPipelineDepthStencilStateCreateInfo dssci{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        dssci.depthTestEnable=d.depthStencil.depthTestEnable;
        dssci.depthWriteEnable=d.depthStencil.depthWriteEnable;
        dssci.depthCompareOp=ToVkCompareOp(d.depthStencil.depthCompareOp);
        dssci.stencilTestEnable=d.depthStencil.stencilEnable;

        // Blend attachments
        NkVector<VkPipelineColorBlendAttachmentState> blendAttachs(d.blend.attachments.Size());
        for (uint32 i=0;i<d.blend.attachments.Size();i++) {
            auto& a=d.blend.attachments[i];
            blendAttachs[i].blendEnable=a.blendEnable;
            blendAttachs[i].srcColorBlendFactor=ToVkBlendFactor(a.srcColor);
            blendAttachs[i].dstColorBlendFactor=ToVkBlendFactor(a.dstColor);
            blendAttachs[i].colorBlendOp=ToVkBlendOp(a.colorOp);
            blendAttachs[i].srcAlphaBlendFactor=ToVkBlendFactor(a.srcAlpha);
            blendAttachs[i].dstAlphaBlendFactor=ToVkBlendFactor(a.dstAlpha);
            blendAttachs[i].alphaBlendOp=ToVkBlendOp(a.alphaOp);
            blendAttachs[i].colorWriteMask=a.colorWriteMask&0xF;
        }
        VkPipelineColorBlendStateCreateInfo cbsci{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        cbsci.attachmentCount=(uint32)blendAttachs.Size(); cbsci.pAttachments=blendAttachs.Data();

        VkDynamicState dynStates[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dsci{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dsci.dynamicStateCount=2; dsci.pDynamicStates=dynStates;

        // Pipeline layout (descriptor set layouts + push constants)
        NkVector<VkDescriptorSetLayout> setLayouts;
        for (uint32 i = 0; i < d.descriptorSetLayouts.Size(); ++i) {
            auto* lit = mDescLayouts.Find(d.descriptorSetLayouts[i].id);
            if (!lit) {
                NK_VK_ERR("CreateGraphicsPipeline: missing descriptor set layout id=%llu\n",
                          (unsigned long long)d.descriptorSetLayouts[i].id);
                return {};
            }
            setLayouts.PushBack(lit->layout);
        }

        NkVector<VkPushConstantRange> pushRanges;
        if (!d.pushConstants.IsEmpty()) {
            for (uint32 i = 0; i < d.pushConstants.Size(); ++i) {
                VkPushConstantRange pcr{};
                pcr.stageFlags = toVkStageFlags(d.pushConstants[i].stages);
                pcr.offset     = d.pushConstants[i].offset;
                pcr.size       = d.pushConstants[i].size;
                pushRanges.PushBack(pcr);
            }
        } else {
            pushRanges.PushBack({VK_SHADER_STAGE_ALL, 0, 128});
        }

        VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        plci.setLayoutCount = (uint32)setLayouts.Size();
        plci.pSetLayouts = setLayouts.IsEmpty() ? nullptr : setLayouts.Data();
        plci.pushConstantRangeCount=(uint32)pushRanges.Size();
        plci.pPushConstantRanges=pushRanges.Data();
        VkPipelineLayout layout=VK_NULL_HANDLE;
        NK_VK_CHECK(vkCreatePipelineLayout(mDevice,&plci,nullptr,&layout));

        VkGraphicsPipelineCreateInfo gpci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        gpci.stageCount=(uint32)stages.Size(); gpci.pStages=stages.Data();
        gpci.pVertexInputState=&visci; gpci.pInputAssemblyState=&iasci;
        gpci.pViewportState=&vpsci;   gpci.pRasterizationState=&rsci;
        gpci.pMultisampleState=&msci; gpci.pDepthStencilState=&dssci;
        gpci.pColorBlendState=&cbsci; gpci.pDynamicState=&dsci;
        gpci.layout=layout; gpci.renderPass=rpit->renderPass; gpci.subpass=d.subpass;

        VkPipeline pipe=VK_NULL_HANDLE;
        NK_VK_CHECK(vkCreateGraphicsPipelines(mDevice,VK_NULL_HANDLE,1,&gpci,nullptr,&pipe));

        const VkDescriptorSetLayout firstSetLayout =
            setLayouts.IsEmpty() ? VK_NULL_HANDLE : setLayouts[0];
        uint64 hid=NextId(); mPipelines[hid]={pipe,layout,firstSetLayout,false};
        NkPipelineHandle h; h.id=hid; return h;
    }

    NkPipelineHandle NkVulkanDevice::CreateComputePipeline(const NkComputePipelineDesc& d) {
        threading::NkScopedLock lock(mMutex);
        auto sit=mShaders.Find(d.shader.id); if (!sit) return {};
        if (sit->stages.Empty()) return {};

        VkPushConstantRange pcr{VK_SHADER_STAGE_COMPUTE_BIT,0,128};
        VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        plci.pushConstantRangeCount=1; plci.pPushConstantRanges=&pcr;
        VkPipelineLayout layout=VK_NULL_HANDLE;
        NK_VK_CHECK(vkCreatePipelineLayout(mDevice,&plci,nullptr,&layout));

        VkComputePipelineCreateInfo cpci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        cpci.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        cpci.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;
        cpci.stage.module=sit->stages[0].module;
        cpci.stage.pName=sit->stages[0].entryPoint.c_str();
        cpci.layout=layout;

        VkPipeline pipe=VK_NULL_HANDLE;
        NK_VK_CHECK(vkCreateComputePipelines(mDevice,VK_NULL_HANDLE,1,&cpci,nullptr,&pipe));

        uint64 hid=NextId(); mPipelines[hid]={pipe,layout,VK_NULL_HANDLE,true};
        NkPipelineHandle h; h.id=hid; return h;
    }

    void NkVulkanDevice::DestroyPipeline(NkPipelineHandle& h) {
        threading::NkScopedLock lock(mMutex);
        auto it=mPipelines.Find(h.id); if (!it) return;
        vkDestroyPipeline(mDevice,it->pipeline,nullptr);
        vkDestroyPipelineLayout(mDevice,it->layout,nullptr);
        mPipelines.Erase(h.id); h.id=0;
    }

    // =============================================================================
    // Render Passes
    // =============================================================================
    NkRenderPassHandle NkVulkanDevice::CreateRenderPass(const NkRenderPassDesc& d) {
        threading::NkScopedLock lock(mMutex);
        NkVector<VkAttachmentDescription> attachments;
        NkVector<VkAttachmentReference>   colorRefs;
        VkAttachmentReference depthRef{};

        for (uint32 i=0;i<d.colorAttachments.Size();i++) {
            auto& a=d.colorAttachments[i];
            VkAttachmentDescription ad{};
            ad.format=ToVkFormat(a.format); ad.samples=ToVkSamples(a.samples);
            ad.loadOp=ToVkLoadOp(a.loadOp); ad.storeOp=ToVkStoreOp(a.storeOp);
            ad.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE; ad.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
            ad.initialLayout=a.loadOp==NkLoadOp::NK_LOAD?VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED;
            ad.finalLayout=mCreatingSwapchainRenderPass
                ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorRefs.PushBack({(uint32)attachments.Size(),VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
            attachments.PushBack(ad);
        }
        if (d.hasDepth) {
            auto& a=d.depthAttachment;
            VkAttachmentDescription ad{};
            ad.format=ToVkFormat(a.format); ad.samples=ToVkSamples(a.samples);
            ad.loadOp=ToVkLoadOp(a.loadOp); ad.storeOp=ToVkStoreOp(a.storeOp);
            ad.stencilLoadOp=ToVkLoadOp(a.stencilLoad); ad.stencilStoreOp=ToVkStoreOp(a.stencilStore);
            ad.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
            ad.finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthRef={( uint32)attachments.Size(),VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
            attachments.PushBack(ad);
        }

        VkSubpassDescription spd{};
        spd.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
        spd.colorAttachmentCount=(uint32)colorRefs.Size(); spd.pColorAttachments=colorRefs.Data();
        if (d.hasDepth) spd.pDepthStencilAttachment=&depthRef;

        VkSubpassDependency dep{};
        dep.srcSubpass=VK_SUBPASS_EXTERNAL; dep.dstSubpass=0;
        dep.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.dstStageMask=dep.srcStageMask;
        dep.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rpci.attachmentCount=(uint32)attachments.Size(); rpci.pAttachments=attachments.Data();
        rpci.subpassCount=1; rpci.pSubpasses=&spd;
        rpci.dependencyCount=1; rpci.pDependencies=&dep;

        VkRenderPass rp=VK_NULL_HANDLE;
        NK_VK_CHECK(vkCreateRenderPass(mDevice,&rpci,nullptr,&rp));
        uint64 hid=NextId(); mRenderPasses[hid]={rp,d};
        NkRenderPassHandle h; h.id=hid; return h;
    }
    void NkVulkanDevice::DestroyRenderPass(NkRenderPassHandle& h) {
        threading::NkScopedLock lock(mMutex);
        auto it=mRenderPasses.Find(h.id); if (!it) return;
        vkDestroyRenderPass(mDevice,it->renderPass,nullptr);
        mRenderPasses.Erase(h.id); h.id=0;
    }

    // =============================================================================
    // Framebuffers
    // =============================================================================
    NkFramebufferHandle NkVulkanDevice::CreateFramebuffer(const NkFramebufferDesc& d) {
        threading::NkScopedLock lock(mMutex);
        auto rpit=mRenderPasses.Find(d.renderPass.id); if (!rpit) return {};
        NkVector<VkImageView> views;
        for (uint32 i=0;i<d.colorAttachments.Size();i++) {
            auto it=mTextures.Find(d.colorAttachments[i].id);
            if (it) views.PushBack(it->view);
        }
        if (d.depthAttachment.IsValid()) {
            auto it=mTextures.Find(d.depthAttachment.id);
            if (it) views.PushBack(it->view);
        }
        VkFramebufferCreateInfo fci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci.renderPass=(VkRenderPass)rpit->renderPass;
        fci.attachmentCount=(uint32)views.Size(); fci.pAttachments=views.Data();
        fci.width=d.width; fci.height=d.height; fci.layers=d.layers;
        VkFramebuffer fb=VK_NULL_HANDLE;
        NK_VK_CHECK(vkCreateFramebuffer(mDevice,&fci,nullptr,&fb));
        uint64 hid=NextId(); mFramebuffers[hid]={fb,d.width,d.height};
        NkFramebufferHandle h; h.id=hid; return h;
    }
    void NkVulkanDevice::DestroyFramebuffer(NkFramebufferHandle& h) {
        threading::NkScopedLock lock(mMutex);
        auto it=mFramebuffers.Find(h.id); if (!it) return;
        vkDestroyFramebuffer(mDevice,it->framebuffer,nullptr);
        mFramebuffers.Erase(h.id); h.id=0;
    }

    // =============================================================================
    // Descriptor Sets
    // =============================================================================
    NkDescSetHandle NkVulkanDevice::CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc& d) {
        threading::NkScopedLock lock(mMutex);
        auto toVkStageFlags = [](NkShaderStage stages) -> VkShaderStageFlags {
            const uint32 bits = (uint32)stages;
            VkShaderStageFlags out = 0;
            if (bits & (uint32)NkShaderStage::NK_VERTEX)    out |= VK_SHADER_STAGE_VERTEX_BIT;
            if (bits & (uint32)NkShaderStage::NK_FRAGMENT)  out |= VK_SHADER_STAGE_FRAGMENT_BIT;
            if (bits & (uint32)NkShaderStage::NK_GEOMETRY)  out |= VK_SHADER_STAGE_GEOMETRY_BIT;
            if (bits & (uint32)NkShaderStage::NK_TESS_CTRL) out |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            if (bits & (uint32)NkShaderStage::NK_TESS_EVAL) out |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            if (bits & (uint32)NkShaderStage::NK_COMPUTE)   out |= VK_SHADER_STAGE_COMPUTE_BIT;
#ifdef VK_SHADER_STAGE_MESH_BIT_EXT
            if (bits & (uint32)NkShaderStage::NK_MESH)      out |= VK_SHADER_STAGE_MESH_BIT_EXT;
#endif
#ifdef VK_SHADER_STAGE_TASK_BIT_EXT
            if (bits & (uint32)NkShaderStage::NK_TASK)      out |= VK_SHADER_STAGE_TASK_BIT_EXT;
#endif
            return out != 0 ? out : VK_SHADER_STAGE_ALL;
        };
        NkVector<VkDescriptorSetLayoutBinding> bindings(d.bindings.Size());
        for (uint32 i=0;i<d.bindings.Size();i++) {
            bindings[i].binding=d.bindings[i].binding;
            bindings[i].descriptorType=ToVkDescriptorType(d.bindings[i].type);
            bindings[i].descriptorCount=d.bindings[i].count;
            bindings[i].stageFlags=toVkStageFlags(d.bindings[i].stages);
        }
        VkDescriptorSetLayoutCreateInfo dlci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        dlci.bindingCount=(uint32)bindings.Size(); dlci.pBindings=bindings.Data();
        VkDescriptorSetLayout layout=VK_NULL_HANDLE;
        NK_VK_CHECK(vkCreateDescriptorSetLayout(mDevice,&dlci,nullptr,&layout));
        uint64 hid=NextId(); mDescLayouts[hid]={layout,d};
        NkDescSetHandle h; h.id=hid; return h;
    }
    void NkVulkanDevice::DestroyDescriptorSetLayout(NkDescSetHandle& h) {
        threading::NkScopedLock lock(mMutex);
        auto it=mDescLayouts.Find(h.id); if (!it) return;
        vkDestroyDescriptorSetLayout(mDevice,it->layout,nullptr);
        mDescLayouts.Erase(h.id); h.id=0;
    }

    NkDescSetHandle NkVulkanDevice::AllocateDescriptorSet(NkDescSetHandle layoutHandle) {
        threading::NkScopedLock lock(mMutex);
        auto lit=mDescLayouts.Find(layoutHandle.id); if (!lit) return {};
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool=mDescPool; ai.descriptorSetCount=1; ai.pSetLayouts=&lit->layout;
        VkDescriptorSet set=VK_NULL_HANDLE;
        NK_VK_CHECK(vkAllocateDescriptorSets(mDevice,&ai,&set));
        uint64 hid=NextId(); mDescSets[hid]={set,layoutHandle.id};
        NkDescSetHandle h; h.id=hid; return h;
    }
    void NkVulkanDevice::FreeDescriptorSet(NkDescSetHandle& h) {
        threading::NkScopedLock lock(mMutex);
        auto it=mDescSets.Find(h.id); if (!it) return;
        vkFreeDescriptorSets(mDevice,mDescPool,1,&it->set);
        mDescSets.Erase(h.id); h.id=0;
    }

    void NkVulkanDevice::UpdateDescriptorSets(const NkDescriptorWrite* writes, uint32 n) {
        NkVector<VkWriteDescriptorSet> vkWrites;
        NkVector<VkDescriptorBufferInfo> bufInfos; bufInfos.Reserve(n);
        NkVector<VkDescriptorImageInfo>  imgInfos; imgInfos.Reserve(n);

        for (uint32 i=0;i<n;i++) {
            auto& w=writes[i];
            auto sit=mDescSets.Find(w.set.id); if (!sit) continue;
            VkWriteDescriptorSet ws{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            ws.dstSet=sit->set; ws.dstBinding=w.binding; ws.dstArrayElement=w.arrayElem;
            ws.descriptorType=ToVkDescriptorType(w.type); ws.descriptorCount=1;

            if (w.buffer.IsValid()) {
                auto bit=mBuffers.Find(w.buffer.id);
                if (!bit) continue;
                bufInfos.PushBack({bit->buffer,w.bufferOffset,w.bufferRange>0?w.bufferRange:VK_WHOLE_SIZE});
                ws.pBufferInfo=&bufInfos[bufInfos.Size() - 1];
            } else if (w.texture.IsValid()) {
                auto tit=mTextures.Find(w.texture.id);
                VkSampler samp=VK_NULL_HANDLE;
                if (w.sampler.IsValid()) { auto sit2=mSamplers.Find(w.sampler.id); if (sit2) samp=sit2->sampler; }
                if (!tit) continue;
                imgInfos.PushBack({samp,tit->view,ToVkImageLayout(w.textureLayout)});
                ws.pImageInfo=&imgInfos[imgInfos.Size() - 1];
            }
            vkWrites.PushBack(ws);
        }
        if (!vkWrites.Empty())
            vkUpdateDescriptorSets(mDevice,(uint32)vkWrites.Size(),vkWrites.Data(),0,nullptr);
    }

    // =============================================================================
    // Command Buffers
    // =============================================================================
    NkICommandBuffer* NkVulkanDevice::CreateCommandBuffer(NkCommandBufferType t) {
        return new NkVulkanCommandBuffer(this, t);
    }
    void NkVulkanDevice::DestroyCommandBuffer(NkICommandBuffer*& cb) { delete cb; cb=nullptr; }

    // =============================================================================
    // Submit
    // =============================================================================
    void NkVulkanDevice::Submit(NkICommandBuffer* const* cbs, uint32 n, NkFenceHandle fence) {
        NkVector<VkCommandBuffer> vkcbs;
        for (uint32 i=0;i<n;i++) {
            auto* vk=dynamic_cast<NkVulkanCommandBuffer*>(cbs[i]);
            if (vk) vkcbs.PushBack(vk->GetVkCommandBuffer());
        }
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount=(uint32)vkcbs.Size(); si.pCommandBuffers=vkcbs.Data();
        VkFence vkFence=VK_NULL_HANDLE;
        if (fence.IsValid()) { auto it=mFences.Find(fence.id); if (it) vkFence=it->fence; }
        vkQueueSubmit(mGraphicsQueue,1,&si,vkFence);
    }

    void NkVulkanDevice::SubmitAndPresent(NkICommandBuffer* cb) {
        auto& frame=mFrames[mFrameIndex];
        vkWaitForFences(mDevice,1,&frame.inFlightFence,VK_TRUE,UINT64_MAX);
        vkResetFences(mDevice,1,&frame.inFlightFence);

        auto* vkcb=dynamic_cast<NkVulkanCommandBuffer*>(cb);
        VkCommandBuffer cmdBuf=vkcb?vkcb->GetVkCommandBuffer():VK_NULL_HANDLE;

        VkPipelineStageFlags waitStage=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.waitSemaphoreCount=1; si.pWaitSemaphores=&frame.imageAvailable; si.pWaitDstStageMask=&waitStage;
        si.commandBufferCount=1; si.pCommandBuffers=&cmdBuf;
        si.signalSemaphoreCount=1; si.pSignalSemaphores=&frame.renderFinished;
        vkQueueSubmit(mGraphicsQueue,1,&si,frame.inFlightFence);

        VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        pi.waitSemaphoreCount=1; pi.pWaitSemaphores=&frame.renderFinished;
        pi.swapchainCount=1; pi.pSwapchains=&mSwapchain; pi.pImageIndices=&mCurrentImageIdx;
        VkResult r=vkQueuePresentKHR(mPresentQueue,&pi);
        if (r==VK_ERROR_OUT_OF_DATE_KHR||r==VK_SUBOPTIMAL_KHR) RecreateSwapchain(mWidth,mHeight);

        // Serialize for stability with externally-owned command buffers reused every frame.
        vkWaitForFences(mDevice,1,&frame.inFlightFence,VK_TRUE,UINT64_MAX);
    }

    // =============================================================================
    // Fence
    // =============================================================================
    NkFenceHandle NkVulkanDevice::CreateFence(bool signaled) {
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        if (signaled) fi.flags=VK_FENCE_CREATE_SIGNALED_BIT;
        VkFence f=VK_NULL_HANDLE; NK_VK_CHECK(vkCreateFence(mDevice,&fi,nullptr,&f));
        uint64 hid=NextId(); mFences[hid]={f};
        NkFenceHandle h; h.id=hid; return h;
    }
    void NkVulkanDevice::DestroyFence(NkFenceHandle& h) {
        auto it=mFences.Find(h.id); if (!it) return;
        vkDestroyFence(mDevice,it->fence,nullptr);
        mFences.Erase(h.id); h.id=0;
    }
    bool NkVulkanDevice::WaitFence(NkFenceHandle f, uint64 to) {
        auto it=mFences.Find(f.id); if (!it) return false;
        return vkWaitForFences(mDevice,1,&it->fence,VK_TRUE,to)==VK_SUCCESS;
    }
    bool NkVulkanDevice::IsFenceSignaled(NkFenceHandle f) {
        auto it=mFences.Find(f.id); if (!it) return false;
        return vkGetFenceStatus(mDevice,it->fence)==VK_SUCCESS;
    }
    void NkVulkanDevice::ResetFence(NkFenceHandle f) {
        auto it=mFences.Find(f.id); if (!it) return;
        vkResetFences(mDevice,1,&it->fence);
    }
    void NkVulkanDevice::WaitIdle() { vkDeviceWaitIdle(mDevice); }

    // =============================================================================
    // Frame
    // =============================================================================
    void NkVulkanDevice::BeginFrame(NkFrameContext& frame) {
        auto& fd=mFrames[mFrameIndex];
        vkWaitForFences(mDevice,1,&fd.inFlightFence,VK_TRUE,UINT64_MAX);
        VkResult r=vkAcquireNextImageKHR(mDevice,mSwapchain,UINT64_MAX,fd.imageAvailable,VK_NULL_HANDLE,&mCurrentImageIdx);
        if (r==VK_ERROR_OUT_OF_DATE_KHR) { RecreateSwapchain(mWidth,mHeight); }
        frame.frameIndex=mFrameIndex; frame.frameNumber=mFrameNumber;
        frame.frameFence = NkFenceHandle::Null();
    }
    void NkVulkanDevice::EndFrame(NkFrameContext&) {
        mFrameIndex=(mFrameIndex+1)%MAX_FRAMES;
        ++mFrameNumber;
    }
    void NkVulkanDevice::OnResize(uint32 w, uint32 h) {
        if (w==0||h==0) return;
        mWidth=w; mHeight=h;
        RecreateSwapchain(w,h);
    }

    // =============================================================================
    // Caps
    // =============================================================================
    void NkVulkanDevice::QueryCaps() {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(mPhysicalDevice,&props);
        VkPhysicalDeviceFeatures feats;
        vkGetPhysicalDeviceFeatures(mPhysicalDevice,&feats);
        VkPhysicalDeviceMemoryProperties mp;
        vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice,&mp);

        mCaps.maxTextureDim2D        =props.limits.maxImageDimension2D;
        mCaps.maxTextureDim3D        =props.limits.maxImageDimension3D;
        mCaps.maxTextureCubeSize     =props.limits.maxImageDimensionCube;
        mCaps.maxTextureArrayLayers  =props.limits.maxImageArrayLayers;
        mCaps.maxColorAttachments    =props.limits.maxColorAttachments;
        mCaps.maxUniformBufferRange  =props.limits.maxUniformBufferRange;
        mCaps.maxStorageBufferRange  =(uint32)props.limits.maxStorageBufferRange;
        mCaps.maxPushConstantBytes   =props.limits.maxPushConstantsSize;
        mCaps.maxVertexAttributes    =props.limits.maxVertexInputAttributes;
        mCaps.maxVertexBindings      =props.limits.maxVertexInputBindings;
        mCaps.maxComputeGroupSizeX   =props.limits.maxComputeWorkGroupSize[0];
        mCaps.maxComputeGroupSizeY   =props.limits.maxComputeWorkGroupSize[1];
        mCaps.maxComputeGroupSizeZ   =props.limits.maxComputeWorkGroupSize[2];
        mCaps.maxComputeSharedMemory =props.limits.maxComputeSharedMemorySize;
        mCaps.maxSamplerAnisotropy   =(uint32)props.limits.maxSamplerAnisotropy;
        mCaps.minUniformBufferAlign  =(uint32)props.limits.minUniformBufferOffsetAlignment;
        mCaps.minStorageBufferAlign  =(uint32)props.limits.minStorageBufferOffsetAlignment;

        mCaps.tessellationShaders=feats.tessellationShader;
        mCaps.geometryShaders    =feats.geometryShader;
        mCaps.computeShaders     =true;
        mCaps.drawIndirect       =true;
        mCaps.multiViewport      =feats.multiViewport;
        mCaps.independentBlend   =feats.independentBlend;
        mCaps.logicOp            =feats.logicOp;
        mCaps.textureCompressionBC=feats.textureCompressionBC;
        mCaps.textureCompressionETC2=feats.textureCompressionETC2;
        mCaps.textureCompressionASTC=feats.textureCompressionASTC_LDR;
        mCaps.timestampQueries   =props.limits.timestampComputeAndGraphics;
        mCaps.shaderFloat16      =true; // approximation
        mCaps.msaa2x=mCaps.msaa4x=mCaps.msaa8x=true;

        // VRAM
        for (uint32 i=0;i<mp.memoryHeapCount;i++)
            if (mp.memoryHeaps[i].flags&VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                mCaps.vramBytes+=mp.memoryHeaps[i].size;
    }

    // =============================================================================
    // Accesseurs internes
    // =============================================================================
    VkRenderPass     NkVulkanDevice::GetVkRenderPass(uint64 id) const { const auto* it=mRenderPasses.Find(id); return it ? it->renderPass : VK_NULL_HANDLE; }
    uint32           NkVulkanDevice::GetVkRenderPassColorCount(uint64 id) const { const auto* it=mRenderPasses.Find(id); return it ? (uint32)it->desc.colorAttachments.Size() : 0u; }
    bool             NkVulkanDevice::GetVkRenderPassHasDepth(uint64 id) const { const auto* it=mRenderPasses.Find(id); return it ? it->desc.hasDepth : false; }
    VkFramebuffer    NkVulkanDevice::GetVkFB(uint64 id) const { const auto* it=mFramebuffers.Find(id); return it ? it->framebuffer : VK_NULL_HANDLE; }
    VkBuffer         NkVulkanDevice::GetVkBuffer(uint64 id) const { const auto* it=mBuffers.Find(id); return it ? it->buffer : VK_NULL_HANDLE; }
    VkImage          NkVulkanDevice::GetVkImage(uint64 id)  const { const auto* it=mTextures.Find(id); return it ? it->image : VK_NULL_HANDLE; }
    VkImageView      NkVulkanDevice::GetVkImageView(uint64 id) const { const auto* it=mTextures.Find(id); return it ? it->view : VK_NULL_HANDLE; }
    VkSampler        NkVulkanDevice::GetVkSampler(uint64 id) const { const auto* it=mSamplers.Find(id); return it ? it->sampler : VK_NULL_HANDLE; }
    VkPipeline       NkVulkanDevice::GetVkPipeline(uint64 id) const { const auto* it=mPipelines.Find(id); return it ? it->pipeline : VK_NULL_HANDLE; }
    VkPipelineLayout NkVulkanDevice::GetVkPipelineLayout(uint64 id) const { const auto* it=mPipelines.Find(id); return it ? it->layout : VK_NULL_HANDLE; }
    VkDescriptorSet  NkVulkanDevice::GetVkDescSet(uint64 id) const { const auto* it=mDescSets.Find(id); return it ? it->set : VK_NULL_HANDLE; }
    bool             NkVulkanDevice::IsComputePipeline(uint64 id) const { const auto* it=mPipelines.Find(id); return it && it->isCompute; }
    NkTextureDesc    NkVulkanDevice::GetTextureDesc(uint64 id) const { const auto* it=mTextures.Find(id); return it ? it->desc : NkTextureDesc{}; }
    NkBufferDesc     NkVulkanDevice::GetBufferDesc(uint64 id)  const { const auto* it=mBuffers.Find(id); return it ? it->desc : NkBufferDesc{}; }
    NkGPUFormat         NkVulkanDevice::GetSwapchainFormat() const {
        if (mSwapFormat==VK_FORMAT_B8G8R8A8_SRGB)   return NkGPUFormat::NK_BGRA8_SRGB;
        if (mSwapFormat==VK_FORMAT_B8G8R8A8_UNORM)  return NkGPUFormat::NK_BGRA8_UNORM;
        if (mSwapFormat==VK_FORMAT_R8G8B8A8_SRGB)   return NkGPUFormat::NK_RGBA8_SRGB;
        return NkGPUFormat::NK_RGBA8_UNORM;
    }

    // =============================================================================
    // Conversions Vk
    // =============================================================================
    VkFormat NkVulkanDevice::ToVkFormat(NkGPUFormat f) {
        switch(f) {
            case NkGPUFormat::NK_R8_UNORM:      return VK_FORMAT_R8_UNORM;
            case NkGPUFormat::NK_RG8_UNORM:     return VK_FORMAT_R8G8_UNORM;
            case NkGPUFormat::NK_RGBA8_UNORM:   return VK_FORMAT_R8G8B8A8_UNORM;
            case NkGPUFormat::NK_RGBA8_SRGB:    return VK_FORMAT_R8G8B8A8_SRGB;
            case NkGPUFormat::NK_BGRA8_UNORM:   return VK_FORMAT_B8G8R8A8_UNORM;
            case NkGPUFormat::NK_BGRA8_SRGB:    return VK_FORMAT_B8G8R8A8_SRGB;
            case NkGPUFormat::NK_R16_FLOAT:     return VK_FORMAT_R16_SFLOAT;
            case NkGPUFormat::NK_RG16_FLOAT:    return VK_FORMAT_R16G16_SFLOAT;
            case NkGPUFormat::NK_RGBA16_FLOAT:  return VK_FORMAT_R16G16B16A16_SFLOAT;
            case NkGPUFormat::NK_R32_FLOAT:     return VK_FORMAT_R32_SFLOAT;
            case NkGPUFormat::NK_RG32_FLOAT:    return VK_FORMAT_R32G32_SFLOAT;
            case NkGPUFormat::NK_RGB32_FLOAT:   return VK_FORMAT_R32G32B32_SFLOAT;
            case NkGPUFormat::NK_RGBA32_FLOAT:  return VK_FORMAT_R32G32B32A32_SFLOAT;
            case NkGPUFormat::NK_R32_UINT:      return VK_FORMAT_R32_UINT;
            case NkGPUFormat::NK_RG32_UINT:     return VK_FORMAT_R32G32_UINT;
            case NkGPUFormat::NK_D16_UNORM:     return VK_FORMAT_D16_UNORM;
            case NkGPUFormat::NK_D32_FLOAT:     return VK_FORMAT_D32_SFLOAT;
            case NkGPUFormat::NK_D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;
            case NkGPUFormat::NK_D32_FLOAT_S8_UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;
            case NkGPUFormat::NK_BC1_RGB_UNORM: return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
            case NkGPUFormat::NK_BC1_RGB_SRGB:  return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
            case NkGPUFormat::NK_BC3_UNORM:     return VK_FORMAT_BC3_UNORM_BLOCK;
            case NkGPUFormat::NK_BC5_UNORM:     return VK_FORMAT_BC5_UNORM_BLOCK;
            case NkGPUFormat::NK_BC7_UNORM:     return VK_FORMAT_BC7_UNORM_BLOCK;
            case NkGPUFormat::NK_BC7_SRGB:      return VK_FORMAT_BC7_SRGB_BLOCK;
            case NkGPUFormat::NK_ETC2_RGB_UNORM:return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
            case NkGPUFormat::NK_ASTC_4X4_UNORM:return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
            case NkGPUFormat::NK_ASTC_4X4_SRGB: return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
            case NkGPUFormat::NK_R11G11B10_FLOAT:return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
            case NkGPUFormat::NK_A2B10G10R10_UNORM:return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
            default:                       return VK_FORMAT_R8G8B8A8_UNORM;
        }
    }
    VkFilter NkVulkanDevice::ToVkFilter(NkFilter f) { return f==NkFilter::NK_NEAREST?VK_FILTER_NEAREST:VK_FILTER_LINEAR; }
    VkSamplerMipmapMode NkVulkanDevice::ToVkMipFilter(NkMipFilter f) { return f==NkMipFilter::NK_NEAREST?VK_SAMPLER_MIPMAP_MODE_NEAREST:VK_SAMPLER_MIPMAP_MODE_LINEAR; }
    VkSamplerAddressMode NkVulkanDevice::ToVkAddressMode(NkAddressMode a) {
        switch(a) {
            case NkAddressMode::NK_REPEAT:         return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            case NkAddressMode::NK_MIRRORED_REPEAT: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            case NkAddressMode::NK_CLAMP_TO_EDGE:    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case NkAddressMode::NK_CLAMP_TO_BORDER:  return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            default:                            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
    }
    VkCompareOp NkVulkanDevice::ToVkCompareOp(NkCompareOp op) {
        switch(op) {
            case NkCompareOp::NK_NEVER:        return VK_COMPARE_OP_NEVER;
            case NkCompareOp::NK_LESS:         return VK_COMPARE_OP_LESS;
            case NkCompareOp::NK_EQUAL:        return VK_COMPARE_OP_EQUAL;
            case NkCompareOp::NK_LESS_EQUAL:    return VK_COMPARE_OP_LESS_OR_EQUAL;
            case NkCompareOp::NK_GREATER:      return VK_COMPARE_OP_GREATER;
            case NkCompareOp::NK_NOT_EQUAL:     return VK_COMPARE_OP_NOT_EQUAL;
            case NkCompareOp::NK_GREATER_EQUAL: return VK_COMPARE_OP_GREATER_OR_EQUAL;
            default:                        return VK_COMPARE_OP_ALWAYS;
        }
    }
    VkBlendFactor NkVulkanDevice::ToVkBlendFactor(NkBlendFactor f) {
        switch(f) {
            case NkBlendFactor::NK_ZERO:              return VK_BLEND_FACTOR_ZERO;
            case NkBlendFactor::NK_ONE:               return VK_BLEND_FACTOR_ONE;
            case NkBlendFactor::NK_SRC_COLOR:          return VK_BLEND_FACTOR_SRC_COLOR;
            case NkBlendFactor::NK_ONE_MINUS_SRC_COLOR:  return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
            case NkBlendFactor::NK_SRC_ALPHA:          return VK_BLEND_FACTOR_SRC_ALPHA;
            case NkBlendFactor::NK_ONE_MINUS_SRC_ALPHA:  return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            case NkBlendFactor::NK_DST_ALPHA:          return VK_BLEND_FACTOR_DST_ALPHA;
            case NkBlendFactor::NK_ONE_MINUS_DST_ALPHA:  return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
            case NkBlendFactor::NK_SRC_ALPHA_SATURATE:  return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
            default:                               return VK_BLEND_FACTOR_ONE;
        }
    }
    VkBlendOp NkVulkanDevice::ToVkBlendOp(NkBlendOp op) {
        switch(op) {
            case NkBlendOp::NK_ADD:    return VK_BLEND_OP_ADD;
            case NkBlendOp::NK_SUB:    return VK_BLEND_OP_SUBTRACT;
            case NkBlendOp::NK_REV_SUB: return VK_BLEND_OP_REVERSE_SUBTRACT;
            case NkBlendOp::NK_MIN:    return VK_BLEND_OP_MIN;
            case NkBlendOp::NK_MAX:    return VK_BLEND_OP_MAX;
            default:                return VK_BLEND_OP_ADD;
        }
    }
    VkPrimitiveTopology NkVulkanDevice::ToVkTopology(NkPrimitiveTopology t) {
        switch(t) {
            case NkPrimitiveTopology::NK_TRIANGLE_LIST:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            case NkPrimitiveTopology::NK_TRIANGLE_STRIP: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            case NkPrimitiveTopology::NK_LINE_LIST:      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            case NkPrimitiveTopology::NK_LINE_STRIP:     return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
            case NkPrimitiveTopology::NK_POINT_LIST:     return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            case NkPrimitiveTopology::NK_PATCH_LIST:     return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
            default:                                 return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        }
    }
    VkPolygonMode  NkVulkanDevice::ToVkPolygonMode(NkFillMode f) { switch(f){case NkFillMode::NK_WIREFRAME:return VK_POLYGON_MODE_LINE;case NkFillMode::NK_POINT:return VK_POLYGON_MODE_POINT;default:return VK_POLYGON_MODE_FILL;} }
    VkCullModeFlags NkVulkanDevice::ToVkCullMode(NkCullMode c) { switch(c){case NkCullMode::NK_NONE:return VK_CULL_MODE_NONE;case NkCullMode::NK_FRONT:return VK_CULL_MODE_FRONT_BIT;default:return VK_CULL_MODE_BACK_BIT;} }
    VkFrontFace    NkVulkanDevice::ToVkFrontFace(NkFrontFace f){ return f==NkFrontFace::NK_CW?VK_FRONT_FACE_CLOCKWISE:VK_FRONT_FACE_COUNTER_CLOCKWISE; }
    VkSampleCountFlagBits NkVulkanDevice::ToVkSamples(NkSampleCount s) { switch(s){case NkSampleCount::NK_S2:return VK_SAMPLE_COUNT_2_BIT;case NkSampleCount::NK_S4:return VK_SAMPLE_COUNT_4_BIT;case NkSampleCount::NK_S8:return VK_SAMPLE_COUNT_8_BIT;case NkSampleCount::NK_S16:return VK_SAMPLE_COUNT_16_BIT;default:return VK_SAMPLE_COUNT_1_BIT;} }
    VkImageType    NkVulkanDevice::ToVkImageType(NkTextureType t) { switch(t){case NkTextureType::NK_TEX1D:return VK_IMAGE_TYPE_1D;case NkTextureType::NK_TEX3D:return VK_IMAGE_TYPE_3D;default:return VK_IMAGE_TYPE_2D;} }
    VkImageViewType NkVulkanDevice::ToVkImageViewType(NkTextureType t, bool cube, uint32 layers) {
        if (cube && layers>6)  return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        if (cube)              return VK_IMAGE_VIEW_TYPE_CUBE;
        switch(t) {
            case NkTextureType::NK_TEX1D:      return VK_IMAGE_VIEW_TYPE_1D;
            case NkTextureType::NK_TEX3D:      return VK_IMAGE_VIEW_TYPE_3D;
            case NkTextureType::NK_TEX2D_ARRAY: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            default:                        return VK_IMAGE_VIEW_TYPE_2D;
        }
    }
    VkShaderStageFlagBits NkVulkanDevice::ToVkShaderStage(NkShaderStage s) {
        switch(s) {
            case NkShaderStage::NK_VERTEX:   return VK_SHADER_STAGE_VERTEX_BIT;
            case NkShaderStage::NK_FRAGMENT: return VK_SHADER_STAGE_FRAGMENT_BIT;
            case NkShaderStage::NK_GEOMETRY: return VK_SHADER_STAGE_GEOMETRY_BIT;
            case NkShaderStage::NK_TESS_CTRL: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            case NkShaderStage::NK_TESS_EVAL: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            case NkShaderStage::NK_COMPUTE:  return VK_SHADER_STAGE_COMPUTE_BIT;
            default:                      return VK_SHADER_STAGE_ALL;
        }
    }
    VkDescriptorType NkVulkanDevice::ToVkDescriptorType(NkDescriptorType t) {
        switch(t) {
            case NkDescriptorType::NK_UNIFORM_BUFFER:         return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            case NkDescriptorType::NK_UNIFORM_BUFFER_DYNAMIC:  return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            case NkDescriptorType::NK_STORAGE_BUFFER:         return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            case NkDescriptorType::NK_STORAGE_BUFFER_DYNAMIC:  return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            case NkDescriptorType::NK_SAMPLED_TEXTURE:        return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            case NkDescriptorType::NK_STORAGE_TEXTURE:        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            case NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER:  return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            case NkDescriptorType::NK_SAMPLER:               return VK_DESCRIPTOR_TYPE_SAMPLER;
            case NkDescriptorType::NK_INPUT_ATTACHMENT:       return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            default:                                      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
    }
    VkAttachmentLoadOp  NkVulkanDevice::ToVkLoadOp (NkLoadOp  op){switch(op){case NkLoadOp::NK_CLEAR:return VK_ATTACHMENT_LOAD_OP_CLEAR;case NkLoadOp::NK_LOAD:return VK_ATTACHMENT_LOAD_OP_LOAD;default:return VK_ATTACHMENT_LOAD_OP_DONT_CARE;}}
    VkAttachmentStoreOp NkVulkanDevice::ToVkStoreOp(NkStoreOp op){switch(op){case NkStoreOp::NK_STORE:return VK_ATTACHMENT_STORE_OP_STORE;default:return VK_ATTACHMENT_STORE_OP_DONT_CARE;}}
    VkImageLayout NkVulkanDevice::ToVkImageLayout(NkResourceState s) {
        switch(s){
            case NkResourceState::NK_RENDER_TARGET:    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            case NkResourceState::NK_DEPTH_READ:       return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            case NkResourceState::NK_DEPTH_WRITE:      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            case NkResourceState::NK_SHADER_READ:      return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            case NkResourceState::NK_UNORDERED_ACCESS: return VK_IMAGE_LAYOUT_GENERAL;
            case NkResourceState::NK_TRANSFER_SRC:     return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            case NkResourceState::NK_TRANSFER_DST:     return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            case NkResourceState::NK_PRESENT:         return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            default:                               return VK_IMAGE_LAYOUT_GENERAL;
        }
    }
    VkVertexInputRate NkVulkanDevice::ToVkInputRate(bool inst){return inst?VK_VERTEX_INPUT_RATE_INSTANCE:VK_VERTEX_INPUT_RATE_VERTEX;}
    VkFormat NkVulkanDevice::ToVkVertexFormat(NkVertexFormat f){
        switch(f){
            case NkVertexFormat::NK_R8_UNORM:               return VK_FORMAT_R8_UNORM;
            case NkVertexFormat::NK_RG8_UNORM:              return VK_FORMAT_R8G8_UNORM;
            case NkVertexFormat::NK_RGBA8_UNORM:
            case NkVertexFormat::NK_R8G8B8A8_UNORM_PACKED:  return VK_FORMAT_R8G8B8A8_UNORM;
            case NkVertexFormat::NK_RGBA8_SNORM:            return VK_FORMAT_R8G8B8A8_SNORM;
            case NkVertexFormat::NK_R16_FLOAT:              return VK_FORMAT_R16_SFLOAT;
            case NkVertexFormat::NK_RG16_FLOAT:             return VK_FORMAT_R16G16_SFLOAT;
            case NkVertexFormat::NK_RGBA16_FLOAT:           return VK_FORMAT_R16G16B16A16_SFLOAT;
            case NkVertexFormat::NK_R16_UINT:               return VK_FORMAT_R16_UINT;
            case NkVertexFormat::NK_RG16_UINT:              return VK_FORMAT_R16G16_UINT;
            case NkVertexFormat::NK_RGBA16_UINT:            return VK_FORMAT_R16G16B16A16_UINT;
            case NkVertexFormat::NK_R32_FLOAT:              return VK_FORMAT_R32_SFLOAT;
            case NkVertexFormat::NK_RG32_FLOAT:             return VK_FORMAT_R32G32_SFLOAT;
            case NkVertexFormat::NK_RGB32_FLOAT:            return VK_FORMAT_R32G32B32_SFLOAT;
            case NkVertexFormat::NK_RGBA32_FLOAT:           return VK_FORMAT_R32G32B32A32_SFLOAT;
            case NkVertexFormat::NK_R32_UINT:               return VK_FORMAT_R32_UINT;
            case NkVertexFormat::NK_RG32_UINT:              return VK_FORMAT_R32G32_UINT;
            case NkVertexFormat::NK_RGBA32_UINT:            return VK_FORMAT_R32G32B32A32_UINT;
            case NkVertexFormat::NK_R32_SINT:               return VK_FORMAT_R32_SINT;
            case NkVertexFormat::NK_RG32_SINT:              return VK_FORMAT_R32G32_SINT;
            case NkVertexFormat::NK_RGBA32_SINT:            return VK_FORMAT_R32G32B32A32_SINT;
            case NkVertexFormat::NK_A2B10G10R10_UNORM:      return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
            default:                                        return VK_FORMAT_R32G32B32_SFLOAT;
        }
    }

} // namespace nkentseu

#endif // NK_RHI_VK_ENABLED

