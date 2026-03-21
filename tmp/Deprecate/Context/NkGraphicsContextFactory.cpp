#include "NKContext/Deprecate/NkGraphicsContextFactory.h"

#include "NkPlatformGraphicsContext.h"
#include "NKWindow/Core/NkWindow.h"

#if defined(NKENTSEU_PLATFORM_LINUX) && (defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB))
#   include <X11/Xlib.h>
#   include <GL/glx.h>
#   if defined(None)
#       undef None
#   endif
#endif

namespace nkentseu {

    void NkGraphicsContextFactory::PrepareWindowConfig(NkWindowConfig& wcfg,
                                                       const NkGraphicsContextConfig& gcfg) {
        NkGraphicsApi api = gcfg.api;
        if (api == NkGraphicsApi::Auto) {
            api = BestAvailableApi();
        }

        if (api == NkGraphicsApi::OpenGL) {
            PrepareForOpenGL(wcfg, gcfg);
        }
    }

    void NkGraphicsContextFactory::PrepareForOpenGL(NkWindowConfig& wcfg,
                                                     const NkGraphicsContextConfig& gcfg) {
    #if defined(NKENTSEU_PLATFORM_LINUX) && (defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB))
        Display* display = XOpenDisplay(nullptr);
        if (display == nullptr) {
            return;
        }

        const int screen = DefaultScreen(display);
        const int depthBits = (gcfg.depthFormat == NkDepthFormat::NK_NONE) ? 0 : 24;
        const int stencilBits = (gcfg.stencilBits > 0u) ? static_cast<int>(gcfg.stencilBits) : 0;
        const int msaaSamples = (gcfg.msaaSamples > 1u) ? static_cast<int>(gcfg.msaaSamples) : 0;
        const int hasMsaa = (gcfg.msaaSamples > 1u) ? 1 : 0;

        const int attribs[] = {
            GLX_X_RENDERABLE,  True,
            GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
            GLX_RENDER_TYPE,   GLX_RGBA_BIT,
            GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
            GLX_RED_SIZE,      8,
            GLX_GREEN_SIZE,    8,
            GLX_BLUE_SIZE,     8,
            GLX_ALPHA_SIZE,    8,
            GLX_DEPTH_SIZE,    depthBits,
            GLX_STENCIL_SIZE,  stencilBits,
            GLX_DOUBLEBUFFER,  True,
            GLX_SAMPLE_BUFFERS, hasMsaa,
            GLX_SAMPLES,        msaaSamples,
            0
        };

        int fbCount = 0;
        GLXFBConfig* configs = glXChooseFBConfig(display, screen, attribs, &fbCount);
        if (configs == nullptr || fbCount <= 0) {
            if (configs != nullptr) {
                XFree(configs);
            }
            XCloseDisplay(display);
            return;
        }

        XVisualInfo* vi = glXGetVisualFromFBConfig(display, configs[0]);
        if (vi != nullptr) {
            wcfg.surfaceHints.Set(
                NkSurfaceHintKey::NK_GLX_VISUAL_ID,
                static_cast<uintptr>(vi->visualid));
            XFree(vi);
        }

        // FBConfig* ne reste pas valide après XFree(configs) + XCloseDisplay(display).
        // Le backend window ne nécessite que VisualId pour créer la fenêtre correctement.
        wcfg.surfaceHints.Set(NkSurfaceHintKey::NK_GLX_FB_CONFIG_PTR, 0);

        XFree(configs);
        XCloseDisplay(display);
    #else
        (void)wcfg;
        (void)gcfg;
    #endif
    }

    NkGraphicsContextPtr NkGraphicsContextFactory::Create(NkWindow& window,
                                                          const NkGraphicsContextConfig& config) {
        NkGraphicsContextConfig resolved = config;
        if (resolved.api == NkGraphicsApi::Auto) {
            resolved.api = BestAvailableApi();
        }

        const NkSurfaceDesc surface = window.GetSurfaceDesc();
        if (!surface.IsValid()) {
            return nullptr;
        }

        NkGraphicsContextPtr ctx;
        switch (resolved.api) {
            case NkGraphicsApi::OpenGL:
                ctx = CreateOpenGL(surface, resolved);
                break;
            case NkGraphicsApi::Vulkan:
                ctx = CreateVulkan(surface, resolved);
                break;
            case NkGraphicsApi::Metal:
                ctx = CreateMetal(surface, resolved);
                break;
            case NkGraphicsApi::DirectX11:
                ctx = CreateDirectX(surface, resolved, false);
                break;
            case NkGraphicsApi::DirectX12:
                ctx = CreateDirectX(surface, resolved, true);
                break;
            case NkGraphicsApi::Software:
                ctx = CreateSoftware(surface, resolved);
                break;
            default:
                return nullptr;
        }

        if (!ctx) {
            return nullptr;
        }

        const NkContextError initError = ctx->Init(surface, resolved);
        if (initError.IsFail()) {
            return nullptr;
        }

        return ctx;
    }

    NkGraphicsApi NkGraphicsContextFactory::BestAvailableApi() {
    #if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
        return NkGraphicsApi::Metal;
    #elif defined(NKENTSEU_PLATFORM_WINDOWS) || defined(NKENTSEU_PLATFORM_XBOX)
        return NkGraphicsApi::DirectX12;
    #elif defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        return NkGraphicsApi::OpenGL;
    #else
    #if defined(NKENTSEU_GRAPHICS_VULKAN)
        return NkGraphicsApi::Vulkan;
    #else
        return NkGraphicsApi::OpenGL;
    #endif
    #endif
    }

    bool NkGraphicsContextFactory::IsApiAvailable(NkGraphicsApi api) {
        switch (api) {
            case NkGraphicsApi::NK_NONE:
            case NkGraphicsApi::Auto:
                return false;

            case NkGraphicsApi::Software:
            case NkGraphicsApi::OpenGL:
                return true;

            case NkGraphicsApi::Vulkan:
            #if defined(NKENTSEU_GRAPHICS_VULKAN)
                return true;
            #else
                return false;
            #endif

            case NkGraphicsApi::Metal:
            #if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
                return true;
            #else
                return false;
            #endif

            case NkGraphicsApi::DirectX11:
            case NkGraphicsApi::DirectX12:
            #if defined(NKENTSEU_PLATFORM_WINDOWS) || defined(NKENTSEU_PLATFORM_XBOX)
                return true;
            #else
                return false;
            #endif
        }

        return false;
    }

    NkGraphicsContextPtr NkGraphicsContextFactory::CreateOpenGL(const NkSurfaceDesc&,
                                                                const NkGraphicsContextConfig&) {
        return std::make_unique<NkPlatformGraphicsContext>(NkGraphicsApi::OpenGL);
    }

    NkGraphicsContextPtr NkGraphicsContextFactory::CreateVulkan(const NkSurfaceDesc&,
                                                                const NkGraphicsContextConfig&) {
    #if defined(NKENTSEU_GRAPHICS_VULKAN)
        return std::make_unique<NkPlatformGraphicsContext>(NkGraphicsApi::Vulkan);
    #else
        return nullptr;
    #endif
    }

    NkGraphicsContextPtr NkGraphicsContextFactory::CreateMetal(const NkSurfaceDesc&,
                                                               const NkGraphicsContextConfig&) {
    #if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
        return std::make_unique<NkPlatformGraphicsContext>(NkGraphicsApi::Metal);
    #else
        return nullptr;
    #endif
    }

    NkGraphicsContextPtr NkGraphicsContextFactory::CreateDirectX(const NkSurfaceDesc&,
                                                                 const NkGraphicsContextConfig&,
                                                                 bool dx12) {
    #if defined(NKENTSEU_PLATFORM_WINDOWS) || defined(NKENTSEU_PLATFORM_XBOX)
        return std::make_unique<NkPlatformGraphicsContext>(dx12 ? NkGraphicsApi::DirectX12
                                                                 : NkGraphicsApi::DirectX11);
    #else
        (void)dx12;
        return nullptr;
    #endif
    }

    NkGraphicsContextPtr NkGraphicsContextFactory::CreateSoftware(const NkSurfaceDesc&,
                                                                  const NkGraphicsContextConfig&) {
        return std::make_unique<NkPlatformGraphicsContext>(NkGraphicsApi::Software);
    }

} // namespace nkentseu
