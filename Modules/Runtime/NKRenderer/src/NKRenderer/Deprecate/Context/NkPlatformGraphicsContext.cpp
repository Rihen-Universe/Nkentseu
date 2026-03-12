#include "NkPlatformGraphicsContext.h"

#include "NKWindow/Core/NkSurfaceHint.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   include <windows.h>
#endif
#if defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#   include <GL/glx.h>
#endif

namespace nkentseu {

    namespace {

        static uint32 NkMaxU32(uint32 a, uint32 b) {
            return (a > b) ? a : b;
        }

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        static void* NkWglGetProcAddressShim(const char* name) {
            if (name == nullptr) {
                return nullptr;
            }

            void* proc = reinterpret_cast<void*>(wglGetProcAddress(name));
            if (proc != nullptr) {
                return proc;
            }

            static HMODULE openglModule = LoadLibraryA("opengl32.dll");
            if (openglModule == nullptr) {
                return nullptr;
            }
            return reinterpret_cast<void*>(GetProcAddress(openglModule, name));
        }
    #endif

    #if defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
        static void* NkGlxGetProcAddressShim(const char* name) {
            if (name == nullptr) {
                return nullptr;
            }
            return reinterpret_cast<void*>(glXGetProcAddressARB(reinterpret_cast<const GLubyte*>(name)));
        }
    #endif

    #if defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_USE_EGL)
        static void* NkEglGetProcAddressShim(const char* name) {
            if (name == nullptr) {
                return nullptr;
            }
            return reinterpret_cast<void*>(eglGetProcAddress(name));
        }
    #endif

    } // namespace

    NkPlatformGraphicsContext::NkPlatformGraphicsContext(NkGraphicsApi api)
        : mApi(api) {
        ClearError();
    }

    NkPlatformGraphicsContext::~NkPlatformGraphicsContext() {
        Shutdown();
    }

    NkContextError NkPlatformGraphicsContext::Init(const NkSurfaceDesc& surface,
                                                   const NkGraphicsContextConfig& config) {
        Shutdown();

        if (!surface.IsValid()) {
            SetError(1u, "Invalid surface descriptor.");
            return mLastError;
        }

        mSurface = surface;
        mConfig = config;
        mFrameCounter = 0;
        mNeedRecreate = false;

        mSwapchain.width = surface.width;
        mSwapchain.height = surface.height;
        mSwapchain.imageCount = NkMaxU32(1u, config.swapchainImages);
        mSwapchain.maxFrames = NkMaxU32(1u, config.swapchainImages);
        mSwapchain.vsync = config.vsync;
        mSwapchain.dpi = surface.dpi;

        bool ok = false;
        switch (mApi) {
            case NkGraphicsApi::OpenGL:
                ok = InitOpenGL(surface);
                break;
            case NkGraphicsApi::Vulkan:
                ok = InitVulkan(surface);
                break;
            case NkGraphicsApi::Metal:
                ok = InitMetal(surface);
                break;
            case NkGraphicsApi::DirectX11:
                ok = InitDirectX11(surface);
                break;
            case NkGraphicsApi::DirectX12:
                ok = InitDirectX12(surface);
                break;
            case NkGraphicsApi::Software:
                ok = InitSoftware(surface);
                break;
            default:
                SetError(2u, "Unsupported graphics API.");
                return mLastError;
        }

        if (!ok) {
            if (mLastError.IsOk()) {
                SetError(3u, "Graphics context initialization failed.");
            }
            return mLastError;
        }

        mInitialized = true;
        ClearError();
        return mLastError;
    }

    void NkPlatformGraphicsContext::Shutdown() {
        if (!mInitialized && mLastError.IsOk()) {
            return;
        }

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        if (mOwnedDeviceContext != nullptr && mSurface.hwnd != nullptr) {
            ReleaseDC(mSurface.hwnd, static_cast<HDC>(mOwnedDeviceContext));
            mOwnedDeviceContext = nullptr;
        }
    #endif

        mOpenGL = {};
        mVulkan = {};
        mMetal = {};
        mDX11 = {};
        mDX12 = {};
        mSoftware = {};
        mSoftwarePixels.Clear();

        mSwapchain = {};
        mSurface = {};
        mConfig = {};
        mFrameCounter = 0;
        mNeedRecreate = false;
        mInitialized = false;
        ClearError();
    }

    NkContextError NkPlatformGraphicsContext::Recreate(uint32 newWidth, uint32 newHeight) {
        if (!mInitialized) {
            SetError(10u, "Cannot recreate an uninitialized graphics context.");
            return mLastError;
        }
        if (newWidth == 0 || newHeight == 0) {
            SetError(11u, "Cannot recreate graphics context with zero dimensions.");
            return mLastError;
        }

        mSurface.width = newWidth;
        mSurface.height = newHeight;
        mSwapchain.width = newWidth;
        mSwapchain.height = newHeight;

        if (mApi == NkGraphicsApi::Software) {
            if (!ResizeSoftwareBuffer(newWidth, newHeight)) {
                SetError(12u, "Software framebuffer resize failed.");
                return mLastError;
            }
        }

        mNeedRecreate = false;
        ClearError();
        return mLastError;
    }

    bool NkPlatformGraphicsContext::BeginFrame(NkFrameContext& frame) {
        if (!mInitialized) {
            frame.needsRecreate = false;
            return false;
        }

        if (mNeedRecreate) {
            frame.needsRecreate = true;
            return false;
        }

        const uint32 imageCount = NkMaxU32(1u, mSwapchain.imageCount);
        const uint32 maxFrames = NkMaxU32(1u, mSwapchain.maxFrames);

        frame.frameIndex = mFrameCounter % maxFrames;
        frame.imageIndex = mFrameCounter % imageCount;
        frame.needsRecreate = false;
        frame.commandBuffer = nullptr;

        if (mApi == NkGraphicsApi::Vulkan) {
            frame.commandBuffer = mVulkan.commandBuffer;
            mVulkan.currentFrame = frame.frameIndex;
            mVulkan.currentImage = frame.imageIndex;
        } else if (mApi == NkGraphicsApi::DirectX12) {
            frame.commandBuffer = mDX12.commandList;
            mDX12.frameIndex = frame.frameIndex;
        }

        ++mFrameCounter;
        return true;
    }

    void NkPlatformGraphicsContext::EndFrame(NkFrameContext&) {
        // Context initialization layer only: no command submission here.
    }

    void NkPlatformGraphicsContext::Present(NkFrameContext&) {
        // Context initialization layer only: platform renderer presents.
    }

    bool NkPlatformGraphicsContext::IsInitialized() const {
        return mInitialized;
    }

    NkGraphicsApi NkPlatformGraphicsContext::GetApi() const {
        return mApi;
    }

    NkSwapchainDesc NkPlatformGraphicsContext::GetSwapchain() const {
        return mSwapchain;
    }

    NkContextError NkPlatformGraphicsContext::GetLastError() const {
        return mLastError;
    }

    void NkPlatformGraphicsContext::SetVSync(bool enabled) {
        mConfig.vsync = enabled;
        mSwapchain.vsync = enabled;
    }

    bool NkPlatformGraphicsContext::GetVSync() const {
        return mConfig.vsync;
    }

    void* NkPlatformGraphicsContext::GetNativeHandleImpl(uint32 typeId) {
        switch (typeId) {
            case NkOpenGLNativeHandles::kTypeId:
                return (mApi == NkGraphicsApi::OpenGL) ? &mOpenGL : nullptr;
            case NkVulkanNativeHandles::kTypeId:
                return (mApi == NkGraphicsApi::Vulkan) ? &mVulkan : nullptr;
            case NkMetalNativeHandles::kTypeId:
                return (mApi == NkGraphicsApi::Metal) ? &mMetal : nullptr;
            case NkDirectX11NativeHandles::kTypeId:
                return (mApi == NkGraphicsApi::DirectX11) ? &mDX11 : nullptr;
            case NkDirectX12NativeHandles::kTypeId:
                return (mApi == NkGraphicsApi::DirectX12) ? &mDX12 : nullptr;
            case NkSoftwareNativeHandles::kTypeId:
                return (mApi == NkGraphicsApi::Software) ? &mSoftware : nullptr;
            default:
                return nullptr;
        }
    }

    const void* NkPlatformGraphicsContext::GetNativeHandleImpl(uint32 typeId) const {
        switch (typeId) {
            case NkOpenGLNativeHandles::kTypeId:
                return (mApi == NkGraphicsApi::OpenGL) ? &mOpenGL : nullptr;
            case NkVulkanNativeHandles::kTypeId:
                return (mApi == NkGraphicsApi::Vulkan) ? &mVulkan : nullptr;
            case NkMetalNativeHandles::kTypeId:
                return (mApi == NkGraphicsApi::Metal) ? &mMetal : nullptr;
            case NkDirectX11NativeHandles::kTypeId:
                return (mApi == NkGraphicsApi::DirectX11) ? &mDX11 : nullptr;
            case NkDirectX12NativeHandles::kTypeId:
                return (mApi == NkGraphicsApi::DirectX12) ? &mDX12 : nullptr;
            case NkSoftwareNativeHandles::kTypeId:
                return (mApi == NkGraphicsApi::Software) ? &mSoftware : nullptr;
            default:
                return nullptr;
        }
    }

    bool NkPlatformGraphicsContext::InitOpenGL(const NkSurfaceDesc& surface) {
        mOpenGL = {};

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        mOpenGL.hdc = GetDC(surface.hwnd);
        if (mOpenGL.hdc == nullptr) {
            SetError(20u, "OpenGL init failed: GetDC returned null.");
            return false;
        }
        mOwnedDeviceContext = mOpenGL.hdc;
        mOpenGL.getProcAddress = &NkWglGetProcAddressShim;

    #elif defined(NKENTSEU_WINDOWING_XLIB)
        mOpenGL.display = surface.display;
        mOpenGL.drawable = static_cast<uint64>(surface.window);
        mOpenGL.fbConfig = reinterpret_cast<void*>(surface.appliedHints.Get(NkSurfaceHintKey::NK_GLX_FB_CONFIG_PTR));
        mOpenGL.getProcAddress = &NkGlxGetProcAddressShim;

    #elif defined(NKENTSEU_WINDOWING_XCB)
        mOpenGL.display = nullptr;
        mOpenGL.drawable = static_cast<uint64>(surface.window);
        mOpenGL.fbConfig = reinterpret_cast<void*>(surface.appliedHints.Get(NkSurfaceHintKey::NK_GLX_FB_CONFIG_PTR));
        mOpenGL.getProcAddress = &NkGlxGetProcAddressShim;

    #elif defined(NKENTSEU_WINDOWING_WAYLAND)
        mOpenGL.eglDisplay = EGL_NO_DISPLAY;
        mOpenGL.eglSurface = EGL_NO_SURFACE;
        mOpenGL.eglContext = EGL_NO_CONTEXT;
        mOpenGL.getProcAddress = &NkEglGetProcAddressShim;

    #elif defined(NKENTSEU_PLATFORM_ANDROID)
        mOpenGL.eglDisplay = EGL_NO_DISPLAY;
        mOpenGL.eglSurface = EGL_NO_SURFACE;
        mOpenGL.eglContext = EGL_NO_CONTEXT;
        mOpenGL.getProcAddress = &NkEglGetProcAddressShim;

    #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        mOpenGL.webglContext = 1;
    #endif

        return true;
    }

    bool NkPlatformGraphicsContext::InitVulkan(const NkSurfaceDesc& surface) {
        mVulkan = {};
        (void)surface;

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        mVulkan.surface = reinterpret_cast<void*>(surface.hwnd);
    #elif defined(NKENTSEU_WINDOWING_XLIB)
        mVulkan.surface = reinterpret_cast<void*>(static_cast<uintptr>(surface.window));
    #elif defined(NKENTSEU_WINDOWING_XCB)
        mVulkan.surface = reinterpret_cast<void*>(static_cast<uintptr>(surface.window));
    #elif defined(NKENTSEU_WINDOWING_WAYLAND)
        mVulkan.surface = surface.surface;
    #elif defined(NKENTSEU_PLATFORM_ANDROID)
        mVulkan.surface = surface.nativeWindow;
    #elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
        mVulkan.surface = surface.metalLayer;
    #endif

        return true;
    }

    bool NkPlatformGraphicsContext::InitMetal(const NkSurfaceDesc& surface) {
        mMetal = {};
        (void)surface;

    #if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
        mMetal.layer = surface.metalLayer;
        mMetal.currentDrawable = nullptr;
        mMetal.commandBuffer = nullptr;
        mMetal.renderPassDesc = nullptr;
    #endif

        return true;
    }

    bool NkPlatformGraphicsContext::InitDirectX11(const NkSurfaceDesc& surface) {
        mDX11 = {};
        (void)surface;
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        mDX11.device = nullptr;
        mDX11.deviceContext = nullptr;
        mDX11.swapChain = nullptr;
        mDX11.renderTarget = nullptr;
        mDX11.depthStencil = nullptr;
    #endif
        return true;
    }

    bool NkPlatformGraphicsContext::InitDirectX12(const NkSurfaceDesc& surface) {
        mDX12 = {};
        (void)surface;
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        mDX12.device = nullptr;
        mDX12.commandQueue = nullptr;
        mDX12.swapChain = nullptr;
        mDX12.rtvHeap = nullptr;
        mDX12.dsvHeap = nullptr;
        mDX12.commandAllocator = nullptr;
        mDX12.commandList = nullptr;
        mDX12.backBuffer = nullptr;
        mDX12.frameIndex = 0;
    #endif
        return true;
    }

    bool NkPlatformGraphicsContext::InitSoftware(const NkSurfaceDesc& surface) {
        mSoftware = {};
        return ResizeSoftwareBuffer(surface.width, surface.height);
    }

    void NkPlatformGraphicsContext::SetError(uint32 code, const char* message) {
        mLastError.code = code;
        mLastError.message = message;
    }

    void NkPlatformGraphicsContext::ClearError() {
        mLastError = NkContextError::Ok();
    }

    bool NkPlatformGraphicsContext::ResizeSoftwareBuffer(uint32 width, uint32 height) {
        if (width == 0 || height == 0) {
            return false;
        }

        const usize bytes = static_cast<usize>(width) * static_cast<usize>(height) * 4u;
        mSoftwarePixels.Resize(bytes, static_cast<uint8>(0));

        mSoftware.pixels = mSoftwarePixels.Data();
        mSoftware.width = width;
        mSoftware.height = height;
        mSoftware.stride = width * 4u;
        mSoftware.pixelFormat = 0u;

        return true;
    }

} // namespace nkentseu
