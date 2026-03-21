// ============================================================================
// Sandbox/src/main9.cpp
// Example: NkContext API alternative (GLFW/SDL-like) with per-backend setup.
// ============================================================================

#include "NKWindow/Core/NkContext.h"
#include "NKWindow/Core/NkEvents.h"
#include "NKWindow/Core/NkMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKLogger/NkLog.h"

namespace {
    using namespace nkentseu;

    // Change this macro from the build-system if you want another backend.
    #ifndef NK_SANDBOX_CONTEXT_API
    #define NK_SANDBOX_CONTEXT_API nkentseu::NkRendererApi::NK_OPENGL
    #endif

    NkContextConfig BuildContextConfig(NkRendererApi api) {
        NkContextConfig cfg{};
        cfg.api = api;

        // Generic defaults (all backends)
        cfg.vsync = true;
        cfg.debug = false;

        // OpenGL framebuffer requirements
        cfg.versionMajor = 4;
        cfg.versionMinor = 5;
        cfg.profile = NkContextProfile::NK_CONTEXT_PROFILE_CORE;
        cfg.doubleBuffer = true;
        cfg.msaaSamples = 4;
        cfg.stereo = false;
        cfg.redBits = 8;
        cfg.greenBits = 8;
        cfg.blueBits = 8;
        cfg.alphaBits = 8;
        cfg.depthBits = 24;
        cfg.stencilBits = 8;

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        // Optional: full PIXELFORMATDESCRIPTOR control on Win32.
        // Set useCustomDescriptor=false to fallback to generic defaults above.
        if (api == NkRendererApi::NK_OPENGL) {
            cfg.win32PixelFormat.useCustomDescriptor = true;
            cfg.win32PixelFormat.drawToWindow = true;
            cfg.win32PixelFormat.supportOpenGL = true;
            cfg.win32PixelFormat.doubleBuffer = true;
            cfg.win32PixelFormat.stereo = false;
            cfg.win32PixelFormat.pixelType = 0; // RGBA
            cfg.win32PixelFormat.colorBits = 32;
            cfg.win32PixelFormat.redBits = 8;
            cfg.win32PixelFormat.greenBits = 8;
            cfg.win32PixelFormat.blueBits = 8;
            cfg.win32PixelFormat.alphaBits = 8;
            cfg.win32PixelFormat.depthBits = 24;
            cfg.win32PixelFormat.stencilBits = 8;
            cfg.win32PixelFormat.layerType = 0; // main plane

            // Optional: enforce an exact Win32 pixel format index.
            // cfg.win32PixelFormat.forcedPixelFormatIndex = 0;
        }
    #endif

        return cfg;
    }

    void LogBackendBootstrap(NkRendererApi api, const NkSurfaceDesc& surface) {
        switch (api) {
            case NkRendererApi::NK_OPENGL:
                logger.Info("[main16] OpenGL: create native context, then load procs.");
                break;

            case NkRendererApi::NK_VULKAN:
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                logger.Info("[main16] Vulkan surface (Win32): VkWin32SurfaceCreateInfoKHR(hinstance, hwnd).");
            #elif defined(NKENTSEU_WINDOWING_XCB)
                logger.Info("[main16] Vulkan surface (XCB): VkXcbSurfaceCreateInfoKHR(connection, window).");
            #elif defined(NKENTSEU_WINDOWING_XLIB)
                logger.Info("[main16] Vulkan surface (Xlib): VkXlibSurfaceCreateInfoKHR(display, window).");
            #elif defined(NKENTSEU_WINDOWING_WAYLAND)
                logger.Info("[main16] Vulkan surface (Wayland): VkWaylandSurfaceCreateInfoKHR(display, surface).");
            #elif defined(NKENTSEU_PLATFORM_ANDROID)
                logger.Info("[main16] Vulkan surface (Android): VkAndroidSurfaceCreateInfoKHR(nativeWindow).");
            #elif defined(NKENTSEU_PLATFORM_MACOS)
                logger.Info("[main16] Vulkan surface (macOS): CAMetalLayer via VK_EXT_metal_surface.");
            #else
                logger.Info("[main16] Vulkan: use platform-native handles from NkSurfaceDesc.");
            #endif
                break;

            case NkRendererApi::NK_DIRECTX11:
                logger.Info("[main16] DirectX11: create D3D11 device/swapchain from HWND.");
                break;

            case NkRendererApi::NK_DIRECTX12:
                logger.Info("[main16] DirectX12: create DXGI swapchain + command queues from HWND.");
                break;

            case NkRendererApi::NK_METAL:
                logger.Info("[main16] Metal: create device/command queue from view + CAMetalLayer.");
                break;

            case NkRendererApi::NK_SOFTWARE:
                logger.Info("[main16] Software: create CPU framebuffer using surface dimensions.");
                break;

            default:
                logger.Warn("[main16] Unsupported API selection.");
                break;
        }

        logger.Info("[main16] Surface size = {0}x{1}", surface.width, surface.height);
    }
} // namespace

// AppData for this example.
NKENTSEU_DEFINE_APP_DATA(([]() {
    nkentseu::NkAppData d{};
    d.appName = "SandboxContextMain9";
    d.appVersion = "1.0.0";
    d.enableEventLogging = true;
    return d;
})());

int nkmain(const nkentseu::NkEntryState&) {
    using namespace nkentseu;

    const NkRendererApi backendApi = NK_SANDBOX_CONTEXT_API;
    logger.Info("[main16] backend = {0}", NkRendererApiToString(backendApi));

    // 1) Build context settings (fully customizable with defaults).
    NkContextConfig contextConfig = BuildContextConfig(backendApi);

    // 2) Optional global-style registration (GLFW/SDL-like).
    NkContextInit();
    NkContextSetHints(contextConfig);

    // 3) Create window and apply pre-create hints (for GLX/OpenGL paths).
    NkWindowConfig windowConfig{};
    windowConfig.title = "Sandbox main9 - NkContext";
    windowConfig.width = 1280;
    windowConfig.height = 720;
    windowConfig.resizable = true;
    NkContextApplyWindowHints(windowConfig);

    NkWindow window;
    if (!window.Create(windowConfig)) {
        logger.Error("[main16] Window creation failed: {0}", window.GetLastError().ToString());
        return -1;
    }

    // 4) Create NkContext bound to the window.
    NkContext context{};
    if (!NkContextCreate(window, context, &contextConfig)) {
        logger.Error("[main16] Context creation failed: {0}", context.lastError.ToString());
        window.Close();
        NkContextShutdown();
        return -2;
    }

    LogBackendBootstrap(backendApi, context.surface);

    // 5) OpenGL-only: make current + load procedures (GLAD-like flow).
    if (backendApi == NkRendererApi::NK_OPENGL) {
        if (!NkContextMakeCurrent(context)) {
            logger.Error("[main16] MakeCurrent failed: {0}", context.lastError.ToString());
            NkContextDestroy(context);
            window.Close();
            NkContextShutdown();
            return -3;
        }

        NkContextProc loader = NkContextGetProcAddressLoader(context);
        logger.Info("[main16] OpenGL proc loader = {0}", loader ? "available" : "null");
        // Example with GLAD:
        // gladLoadGLLoader((GLADloadproc)NkContextGetProcAddressLoader(context));
    }

    // 6) Minimal loop.
    auto& events = NkEvents();
    bool running = true;
    while (running && window.IsOpen()) {
        NkEvent* ev = nullptr;
        while ((ev = events.PollEvent()) != nullptr) {
            if (ev->Is<NkWindowCloseEvent>()) {
                running = false;
                break;
            }
        }

        if (backendApi == NkRendererApi::NK_OPENGL) {
            NkContextSwapBuffers(context);
        }
    }

    NkContextDestroy(context);
    window.Close();
    NkContextShutdown();
    logger.Info("[main16] shutdown complete");
    return 0;
}


