#include "NkGraphicsContextFactory.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkSurfaceHint.h"
#include "Backend/OpenGL/NkOpenGLContext.h"
#include "Backend/Software/NkSoftwareContext.h"

#if defined(NKENTSEU_GRAPHICS_VULKAN)
#   include "Backend/Vulkan/NkVulkanContext.h"
#endif
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
#   include "Backend/Metal/NkMetalContext.h"
#endif
#if defined(NKENTSEU_PLATFORM_WINDOWS) || defined(NKENTSEU_PLATFORM_XBOX)
#   include "Backend/DirectX/NkDirectXContext.h"
#endif

// GLX pour PrepareForOpenGL sur Linux/XLib/XCB
#if (defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)) \
 && !defined(NKENTSEU_WINDOWING_WAYLAND)
#   include <GL/glx.h>
#   if defined(NKENTSEU_WINDOWING_XLIB)
#       include "NKWindow/Platform/XLib/NkXLibWindow.h"   // NkXLibGetDisplay()
#   elif defined(NKENTSEU_WINDOWING_XCB)
#       include "NKWindow/Platform/XCB/NkXCBWindow.h"     // NkXCBGetConnection()
#       include <X11/Xlib-xcb.h>                          // XGetXCBConnection()
#   endif
#   define NK_NEEDS_GLX_PREPARE 1
#endif

namespace nkentseu {

// =============================================================================
// PrepareWindowConfig â€” AVANT NkWindow::Create()
// =============================================================================

void NkGraphicsContextFactory::PrepareWindowConfig(
    NkWindowConfig&                wcfg,
    const NkGraphicsContextConfig& gcfg)
{
    NkGraphicsApi api = gcfg.api;
    if (api == NkGraphicsApi::Auto) api = BestAvailableApi();

    if (api == NkGraphicsApi::OpenGL)
        PrepareForOpenGL(wcfg, gcfg);
    // Vulkan, Metal, DirectX, Software, EGL :
    // aucun hint nÃ©cessaire sur la fenÃªtre â€” ne rien faire.
}

void NkGraphicsContextFactory::PrepareForOpenGL(
    NkWindowConfig&                wcfg,
    const NkGraphicsContextConfig& gcfg)
{
#if defined(NK_NEEDS_GLX_PREPARE)
    // Sur XLib/XCB, OpenGL/GLX exige que la fenÃªtre soit crÃ©Ã©e
    // avec la bonne XVisualInfo issue de glXChooseFBConfig.
    // On interroge GLX maintenant et on injecte la VisualID dans les hints.

    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) return;

    int screen = DefaultScreen(dpy);

    // Attributs FBConfig selon la config demandÃ©e
    int attribs[] = {
        GLX_X_RENDERABLE,  True,
        GLX_RENDER_TYPE,   GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
        GLX_RED_SIZE,      8,
        GLX_GREEN_SIZE,    8,
        GLX_BLUE_SIZE,     8,
        GLX_ALPHA_SIZE,    8,
        GLX_DEPTH_SIZE,    (gcfg.depthFormat != NkDepthFormat::None) ? 24 : 0,
        GLX_STENCIL_SIZE,  (gcfg.stencilBits > 0) ? 8 : 0,
        GLX_DOUBLEBUFFER,  True,
        GLX_SAMPLE_BUFFERS,(gcfg.msaaSamples > 1) ? 1 : 0,
        GLX_SAMPLES,       (int)gcfg.msaaSamples,
        None
    };

    int fbCount = 0;
    GLXFBConfig* fbs = glXChooseFBConfig(dpy, screen, attribs, &fbCount);
    if (!fbs || fbCount == 0) {
        XCloseDisplay(dpy);
        return;
    }

    // Prendre le premier FBConfig valide
    XVisualInfo* vi = glXGetVisualFromFBConfig(dpy, fbs[0]);
    if (vi) {
        // Injecter la VisualID et le pointeur FBConfig dans les hints
        wcfg.surfaceHints.Set(NkSurfaceHintKey::GlxVisualId,
                              (NkUPtr)vi->visualid);
        wcfg.surfaceHints.Set(NkSurfaceHintKey::GlxFBConfigPtr,
                              (NkUPtr)fbs[0]); // fbs[0] = premier FBConfig
        XFree(vi);
    }

    // Note : fbs allouÃ© par GLX â€” le contexte OpenGL libÃ©rera
    // sa propre copie ; ici on conserve juste l'index/pointer pour
    // la crÃ©ation de la fenÃªtre.
    XFree(fbs);
    XCloseDisplay(dpy);

#else
    // Win32/WGL, EGL, Metal, WASM, Android :
    // aucun hint GLX nÃ©cessaire
    (void)wcfg; (void)gcfg;
#endif
}

// =============================================================================
// Create â€” APRÃˆS NkWindow::Create()
// =============================================================================

NkGraphicsContextPtr NkGraphicsContextFactory::Create(
    NkWindow&                       window,
    const NkGraphicsContextConfig&  config)
{
    NkGraphicsContextConfig resolved = config;
    if (resolved.api == NkGraphicsApi::Auto)
        resolved.api = BestAvailableApi();

    NkSurfaceDesc surface = window.GetSurfaceDesc();
    NkGraphicsContextPtr ctx;

    switch (resolved.api) {
        case NkGraphicsApi::OpenGL:
            ctx = CreateOpenGL  (surface, resolved); break;
        case NkGraphicsApi::Vulkan:
            ctx = CreateVulkan  (surface, resolved); break;
        case NkGraphicsApi::Metal:
            ctx = CreateMetal   (surface, resolved); break;
        case NkGraphicsApi::DirectX11:
            ctx = CreateDirectX (surface, resolved, false); break;
        case NkGraphicsApi::DirectX12:
            ctx = CreateDirectX (surface, resolved, true);  break;
        case NkGraphicsApi::Software:
            ctx = CreateSoftware(surface, resolved); break;
        default:
            return nullptr;
    }

    if (!ctx) return nullptr;

    auto err = ctx->Init(surface, resolved);
    if (!err.IsOk()) return nullptr;

    return ctx;
}

// =============================================================================
// BestAvailableApi
// =============================================================================

NkGraphicsApi NkGraphicsContextFactory::BestAvailableApi() {
#if   defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
    return NkGraphicsApi::Metal;
#elif defined(NKENTSEU_PLATFORM_WINDOWS) || defined(NKENTSEU_PLATFORM_XBOX)
#   if defined(NKENTSEU_GRAPHICS_VULKAN)
    return NkGraphicsApi::Vulkan;
#   else
    return NkGraphicsApi::DirectX12;
#   endif
#elif defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    return NkGraphicsApi::OpenGL;   // OpenGL ES / WebGL
#else
    // Linux
#   if defined(NKENTSEU_GRAPHICS_VULKAN)
    return NkGraphicsApi::Vulkan;
#   else
    return NkGraphicsApi::OpenGL;
#   endif
#endif
}

bool NkGraphicsContextFactory::IsApiAvailable(NkGraphicsApi api) {
    switch (api) {
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
        case NkGraphicsApi::Vulkan:
#if defined(NKENTSEU_GRAPHICS_VULKAN)
            return true;
#else
            return false;
#endif
        case NkGraphicsApi::OpenGL:
        case NkGraphicsApi::Software:
            return true;
        default:
            return false;
    }
}

// =============================================================================
// Create* helpers
// =============================================================================

NkGraphicsContextPtr NkGraphicsContextFactory::CreateOpenGL(
    const NkSurfaceDesc& s, const NkGraphicsContextConfig& c) {
    (void)s; (void)c;
    return std::make_unique<NkOpenGLContext>();
}
NkGraphicsContextPtr NkGraphicsContextFactory::CreateVulkan(
    const NkSurfaceDesc& s, const NkGraphicsContextConfig& c) {
    (void)s; (void)c;
#if defined(NKENTSEU_GRAPHICS_VULKAN)
    return std::make_unique<NkVulkanContext>();
#else
    return nullptr;
#endif
}
NkGraphicsContextPtr NkGraphicsContextFactory::CreateMetal(
    const NkSurfaceDesc& s, const NkGraphicsContextConfig& c) {
    (void)s; (void)c;
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
    return std::make_unique<NkMetalContext>();
#else
    return nullptr;
#endif
}
NkGraphicsContextPtr NkGraphicsContextFactory::CreateDirectX(
    const NkSurfaceDesc& s, const NkGraphicsContextConfig& c, bool dx12) {
    (void)s; (void)c;
#if defined(NKENTSEU_PLATFORM_WINDOWS) || defined(NKENTSEU_PLATFORM_XBOX)
    if (dx12) return std::make_unique<NkDirectX12Context>();
    else       return std::make_unique<NkDirectX11Context>();
#else
    (void)dx12; return nullptr;
#endif
}
NkGraphicsContextPtr NkGraphicsContextFactory::CreateSoftware(
    const NkSurfaceDesc& s, const NkGraphicsContextConfig& c) {
    (void)s; (void)c;
    return std::make_unique<NkSoftwareContext>();
}

} // namespace nkentseu