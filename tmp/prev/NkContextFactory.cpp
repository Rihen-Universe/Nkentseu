// =============================================================================
// NkContextFactory.cpp
// Implémentation de la factory de contextes graphiques.
// =============================================================================

#include "NkContextFactory.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKLogger/NkLog.h"

// Inclure tous les backends disponibles selon la plateforme
#include "NKContext/Backends/OpenGL/NkOpenGLContext.h"
#include "NKContext/Backends/Vulkan/NkVulkanContext.h"
#include "NKContext/Backends/Software/NkSoftwareContext.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   include "NKContext/Backends/DirectX/NkDXContext.h"    // DX11 + DX12
#endif

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
#   include "NKContext/Backends/Metal/NkMetalContext.h"
#endif

// Pour InjectGLXHints
#if defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#   include <GL/glx.h>
#   include <X11/Xlib.h>
#endif

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   include <windows.h>
#elif defined(NKENTSEU_PLATFORM_LINUX)
#   include <dlfcn.h>
#endif

#include <cstdlib>

namespace nkentseu {

// =============================================================================
std::unique_ptr<NkIGraphicsContext>
NkContextFactory::Create(const NkContextDesc& desc, const NkWindow& window) {

    if (!NkGraphicsApiIsAvailable(desc.api)) {
        logger.Error("[NkContextFactory] API '{0}' not available on this platform",
                     NkGraphicsApiName(desc.api));
        return nullptr;
    }

    std::unique_ptr<NkIGraphicsContext> ctx;

    switch (desc.api) {

        // ------------------------------------------------------------------
        case NkGraphicsApi::NK_API_OPENGL:
        case NkGraphicsApi::NK_API_OPENGLES:
        case NkGraphicsApi::NK_API_WEBGL:
            ctx = std::make_unique<NkOpenGLContext>();
            break;

        // ------------------------------------------------------------------
        case NkGraphicsApi::NK_API_VULKAN:
            ctx = std::make_unique<NkVulkanContext>();
            break;

        // ------------------------------------------------------------------
        case NkGraphicsApi::NK_API_DIRECTX11:
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            ctx = std::make_unique<NkDX11Context>();
#else
            logger.Error("[NkContextFactory] DirectX 11 requires Windows");
            return nullptr;
#endif
            break;

        // ------------------------------------------------------------------
        case NkGraphicsApi::NK_API_DIRECTX12:
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            ctx = std::make_unique<NkDX12Context>();
#else
            logger.Error("[NkContextFactory] DirectX 12 requires Windows");
            return nullptr;
#endif
            break;

        // ------------------------------------------------------------------
        case NkGraphicsApi::NK_API_METAL:
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
            ctx = std::make_unique<NkMetalContext>();
#else
            logger.Error("[NkContextFactory] Metal requires macOS or iOS");
            return nullptr;
#endif
            break;

        // ------------------------------------------------------------------
        case NkGraphicsApi::NK_API_SOFTWARE:
            ctx = std::make_unique<NkSoftwareContext>();
            break;

        // ------------------------------------------------------------------
        default:
            logger.Error("[NkContextFactory] Unknown API: {0}", static_cast<int>(desc.api));
            return nullptr;
    }

    // Initialiser le contexte
    logger.Info("[NkContextFactory] Initializing {0} context...", NkGraphicsApiName(desc.api));
    if (!ctx->Initialize(window, desc)) {
        logger.Error("[NkContextFactory] Failed to initialize {0} context",
                     NkGraphicsApiName(desc.api));
        return nullptr;
    }

    const NkContextInfo info = ctx->GetInfo();
    logger.Info("[NkContextFactory] Success - {0} | {1} | {2}",
                NkGraphicsApiName(desc.api),
                info.renderer, info.version);

    return ctx;
}

// =============================================================================
bool NkContextFactory::PrepareWindowConfig(const NkContextDesc& desc,
                                           NkWindowConfig& outConfig) {
    // Uniquement nécessaire pour OpenGL/GLX sur XLib/XCB
    // Toutes les autres APIs : no-op, retourner true immédiatement.

#if defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
    if (desc.api == NkGraphicsApi::NK_API_OPENGL) {
#if defined(NKENTSEU_PLATFORM_LINUX)
        // Sous WSL/WSLg, certains stacks GLX crashent dans la phase de "pre-probe"
        // (avant la création du vrai contexte). On saute l'injection proactive
        // et on laisse InitGLX choisir la config au moment opportun.
        const char* wslInterop = std::getenv("WSL_INTEROP");
        const char* wslDistro  = std::getenv("WSL_DISTRO_NAME");
        if ((wslInterop && *wslInterop) || (wslDistro && *wslDistro)) {
            logger.Warn("[NkContextFactory] WSL detected: skip GLX hint injection pre-pass");
            return true;
        }
#endif
        return InjectGLXHints(desc.opengl, outConfig);
    }
#endif

    // No-op pour Vulkan, DX, Metal, EGL, Software, WebGL
    (void)desc;
    (void)outConfig;
    return true;
}

// =============================================================================
NkGraphicsApi NkContextFactory::GetDefaultApi() {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    return NkGraphicsApi::NK_API_DIRECTX12;
#elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
    return NkGraphicsApi::NK_API_METAL;
#elif defined(NKENTSEU_PLATFORM_ANDROID)
    return NkGraphicsApi::NK_API_VULKAN;
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    return NkGraphicsApi::NK_API_WEBGL;
#else
    // Linux — Vulkan si disponible, sinon OpenGL
    return NkGraphicsApi::NK_API_VULKAN;
#endif
}

// =============================================================================
bool NkContextFactory::IsApiSupported(NkGraphicsApi api) {
    if (!NkGraphicsApiIsAvailable(api)) return false;

    switch (api) {
        case NkGraphicsApi::NK_API_OPENGL: {
#if defined(NKENTSEU_PLATFORM_LINUX) && \
    (defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB))
            const char* wslInterop = std::getenv("WSL_INTEROP");
            const char* wslDistro  = std::getenv("WSL_DISTRO_NAME");
            if ((wslInterop && *wslInterop) || (wslDistro && *wslDistro)) {
                logger.Warn("[NkContextFactory] WSL + GLX detected: skip OpenGL auto-selection");
                return false;
            }
#endif
            return NkGraphicsApiIsAvailable(api);
        }
        case NkGraphicsApi::NK_API_VULKAN: {
            // Vérifier la présence d'un ICD Vulkan
            // Sans linker contre Vulkan ici, on peut charger libvulkan dynamiquement
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            HMODULE lib = LoadLibraryA("vulkan-1.dll");
            if (!lib) return false;
            FreeLibrary(lib);
#elif defined(NKENTSEU_PLATFORM_LINUX)
            void* lib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
            if (!lib) return false;
            dlclose(lib);
#endif
            return true;
        }
        case NkGraphicsApi::NK_API_DIRECTX12: {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            // Vérifier que D3D12 est disponible (Windows 10+)
            HMODULE lib = LoadLibraryA("d3d12.dll");
            if (!lib) return false;
            FreeLibrary(lib);
            return true;
#else
            return false;
#endif
        }
        default:
            return NkGraphicsApiIsAvailable(api);
    }
}

// =============================================================================
//  Injection des hints GLX (Linux XLib / XCB uniquement)
// =============================================================================
#if defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)

bool NkContextFactory::InjectGLXHints(const NkOpenGLDesc& gl,
                                       NkWindowConfig& config) {
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        logger.Error("[NkContextFactory] InjectGLXHints: XOpenDisplay failed");
        return false;
    }

    int glxMajor, glxMinor;
    if (!glXQueryVersion(display, &glxMajor, &glxMinor)
        || (glxMajor == 1 && glxMinor < 3)) {
        logger.Error("[NkContextFactory] GLX 1.3+ required");
        XCloseDisplay(display);
        return false;
    }

    const int fbAttribs[] = {
        GLX_RENDER_TYPE,    GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE,  GLX_WINDOW_BIT,
        GLX_X_VISUAL_TYPE,  GLX_TRUE_COLOR,
        GLX_RED_SIZE,       8,
        GLX_GREEN_SIZE,     8,
        GLX_BLUE_SIZE,      8,
        GLX_ALPHA_SIZE,     8,
        GLX_DEPTH_SIZE,     gl.depthBits,
        GLX_STENCIL_SIZE,   gl.stencilBits,
        GLX_DOUBLEBUFFER,   gl.doubleBuffer ? True : False,
        GLX_SAMPLE_BUFFERS, gl.msaaSamples > 1 ? 1 : 0,
        GLX_SAMPLES,        gl.msaaSamples,
        None
    };

    int fbCount = 0;
    GLXFBConfig* configs = glXChooseFBConfig(display,
        DefaultScreen(display), fbAttribs, &fbCount);

    if (!configs || fbCount == 0) {
        logger.Error("[NkContextFactory] glXChooseFBConfig failed - no matching config");
        XCloseDisplay(display);
        return false;
    }

    GLXFBConfig best = configs[0];

    // Obtenir le VisualID pour XCreateWindow
    XVisualInfo* vi = glXGetVisualFromFBConfig(display, best);
    if (!vi) {
        XFree(configs);
        XCloseDisplay(display);
        return false;
    }

    // Injecter uniquement le VisualID. Ne PAS propager GLXFBConfig*:
    // ce pointeur est lié au Display courant et devient invalide si
    // le Display temporaire est fermé ici (dangling pointer -> crash possible).
    config.surfaceHints.Set(NkSurfaceHintKey::NK_GLX_VISUAL_ID,
                             (uintptr)vi->visualid);
    config.surfaceHints.Set(NkSurfaceHintKey::NK_GLX_FB_CONFIG_PTR, 0);

    logger.Info("[NkContextFactory] GLX hints injected - VisualID=0x{0}",
                static_cast<unsigned long>(vi->visualid));

    XFree(vi);
    XFree(configs);
    XCloseDisplay(display);
    return true;
}

#endif // GLX

} // namespace nkentseu
