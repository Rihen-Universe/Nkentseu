// =============================================================================
// NkOpenGLContext.cpp  (version GLAD2)
// =============================================================================
//
// GLAD2 remplace les magic numbers et les typedefs manuels.
// Il fournit :
//   - glad/wgl.h   → PFNWGL*, gladLoadWGL(), toutes les constantes WGL_ARB_*
//   - glad/glx.h   → gladLoadGLX(), GLX_ARB_*, GLX_EXT_*
//   - glad/egl.h   → gladLoadEGL()
//   - glad/gl.h    → gladLoadGL(), toutes les fonctions GL
//   - glad/gles2.h → gladLoadGLES2()
//
// Comment générer GLAD2 :
//   https://gen.glad.dav1d.de/
//   API      : gl 4.6 + gles2 3.2
//   Spec     : gl
//   Extensions à cocher :
//     WGL_ARB_create_context
//     WGL_ARB_create_context_profile
//     WGL_ARB_create_context_robustness
//     WGL_ARB_pixel_format
//     WGL_ARB_framebuffer_sRGB
//     WGL_ARB_multisample
//     WGL_EXT_swap_control
//     WGL_EXT_swap_control_tear
//     GLX_ARB_create_context
//     GLX_ARB_create_context_profile
//     GLX_ARB_multisample
//     GLX_EXT_swap_control
//   Generator : C/C++
//   Options   : Loader=On, Header-Only=Off
//
// Structure des fichiers générés :
//   glad/
//     include/
//       glad/gl.h      ← toujours nécessaire
//       glad/wgl.h     ← Windows
//       glad/glx.h     ← Linux XLib/XCB
//       glad/egl.h     ← Wayland/Android
//       glad/gles2.h   ← ES
//     src/
//       gl.c
//       wgl.c
//       glx.c
//       egl.c
//
// Ajouter dans CMakeLists.txt :
//   add_subdirectory(extern/glad2)   # si tu utilises glad_add_library CMake
//   target_link_libraries(NkEngine PRIVATE glad)
//   target_include_directories(NkEngine PRIVATE extern/glad2/include)
// =============================================================================

#include "NkOpenGLContext.h"
#include "NkWindow.h"

// ── GLAD2 selon plateforme ────────────────────────────────────────────────────
#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   include <glad/wgl.h>   // Extensions WGL ARB — inclut glad/gl.h automatiquement
#   include <glad/gl.h>
#elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#   include <glad/glx.h>
#   include <glad/gl.h>
#elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
#   include <glad/egl.h>
#   include <glad/gles2.h>
#elif defined(NKENTSEU_PLATFORM_MACOS)
#   include <glad/gl.h>    // NSGL utilise le loader GL générique
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#   include <glad/gles2.h>
#   include <emscripten/html5.h>
#endif

#include <cstdio>
#include <cstring>

#define NK_GL_LOG(...)  printf("[NkOpenGL] " __VA_ARGS__)
#define NK_GL_ERR(...)  printf("[NkOpenGL][ERROR] " __VA_ARGS__)

namespace nkentseu {

// ── Debug callback GLAD2/GL ──────────────────────────────────────────────────
#if defined(NKENTSEU_PLATFORM_WINDOWS) || defined(NKENTSEU_WINDOWING_XLIB) \
 || defined(NKENTSEU_WINDOWING_XCB)
static void GLAPIENTRY GLDebugCallback(
    GLenum source, GLenum type, GLuint id,
    GLenum severity, GLsizei /*length*/,
    const GLchar* message, const void* userParam)
{
    const NkOpenGLDesc* desc = static_cast<const NkOpenGLDesc*>(userParam);
    if (!desc) return;

    // Filtrer selon le niveau de sévérité configuré par l'utilisateur
    uint32 sev = 0;
    switch (severity) {
        case GL_DEBUG_SEVERITY_LOW:    sev = 1; break;
        case GL_DEBUG_SEVERITY_MEDIUM: sev = 2; break;
        case GL_DEBUG_SEVERITY_HIGH:   sev = 3; break;
        default:                       sev = 0; break; // NOTIFICATION
    }
    if (sev < desc->glad2.debugSeverityLevel) return;

    const char* sevStr  = sev == 3 ? "HIGH"
                        : sev == 2 ? "MEDIUM"
                        : sev == 1 ? "LOW" : "NOTIF";
    const char* typeStr = type == GL_DEBUG_TYPE_ERROR ? "ERROR"
                        : type == GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR ? "DEPRECATED"
                        : type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR  ? "UB"
                        : type == GL_DEBUG_TYPE_PERFORMANCE          ? "PERF"
                        : "OTHER";

    NK_GL_ERR("[%s][%s] id=%u src=%u : %s\n",
              sevStr, typeStr, id, source, message);
}
#endif

// =============================================================================
NkOpenGLContext::~NkOpenGLContext() { if (mIsValid) Shutdown(); }

// =============================================================================
bool NkOpenGLContext::Initialize(const NkWindow& window, const NkContextDesc& desc) {
    if (mIsValid) { NK_GL_ERR("Already initialized\n"); return false; }

    mDesc = desc;
    const NkSurfaceDesc surface = window.GetSurfaceDesc();
    if (!surface.IsValid()) { NK_GL_ERR("Invalid NkSurfaceDesc\n"); return false; }

    const NkOpenGLDesc& gl = desc.opengl;
    bool ok = false;

#if defined(NKENTSEU_PLATFORM_WINDOWS)
    ok = InitWGL(surface, gl);
#elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
    ok = InitGLX(surface, gl);
#elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
    ok = InitEGL(surface, gl);
#elif defined(NKENTSEU_PLATFORM_MACOS)
    ok = InitNSGL(surface, gl);
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    ok = InitWebGL(surface, gl);
#endif

    if (!ok) return false;

    // ── Chargement GLAD2 GL ──────────────────────────────────────────────────
    if (gl.glad2.autoLoad) {
        if (!LoadGLAD2(gl)) return false;
    }

    mData.width  = surface.width;
    mData.height = surface.height;
    mIsValid     = true;

    NK_GL_LOG("Ready — %s %s (%s)\n",
              mData.renderer, mData.version, mData.vendor);
    return true;
}

// =============================================================================
//  GLAD2 — chargement des fonctions GL
// =============================================================================
bool NkOpenGLContext::LoadGLAD2(const NkOpenGLDesc& gl) {

#if defined(NKENTSEU_PLATFORM_WINDOWS)
    // 1. Charger les extensions WGL via GLAD2
    if (!gladLoadWGL(mData.hdc, (GLADloadfunc)wglGetProcAddress)) {
        NK_GL_ERR("gladLoadWGL failed\n"); return false;
    }
    // 2. Charger les fonctions GL
    int version = gladLoadGL((GLADloadfunc)wglGetProcAddress);
    if (!version) { NK_GL_ERR("gladLoadGL failed\n"); return false; }
    NK_GL_LOG("GLAD2 GL %d.%d loaded\n", GLAD_VERSION_MAJOR(version),
                                          GLAD_VERSION_MINOR(version));

#elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
    // 1. Charger les extensions GLX via GLAD2
    int screen = DefaultScreen(mData.display);
    if (!gladLoadGLX(mData.display, screen,
                     (GLADloadfunc)glXGetProcAddressARB)) {
        NK_GL_ERR("gladLoadGLX failed\n"); return false;
    }
    // 2. Charger les fonctions GL
    int version = gladLoadGL((GLADloadfunc)glXGetProcAddressARB);
    if (!version) { NK_GL_ERR("gladLoadGL (GLX) failed\n"); return false; }

#elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
    // 1. Charger EGL via GLAD2
    if (!gladLoadEGL(mData.eglDisplay, (GLADloadfunc)eglGetProcAddress)) {
        NK_GL_ERR("gladLoadEGL failed\n"); return false;
    }
    // 2. Charger GLES2/3 via GLAD2
    int version = gladLoadGLES2((GLADloadfunc)eglGetProcAddress);
    if (!version) { NK_GL_ERR("gladLoadGLES2 failed\n"); return false; }

#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    int version = gladLoadGLES2((GLADloadfunc)emscripten_webgl_get_proc_address);
    if (!version) { NK_GL_ERR("gladLoadGLES2 (WebGL) failed\n"); return false; }
#endif

    // Vérification de version
    if (gl.glad2.validateVersion) {
        int major = 0, minor = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        if (major < gl.majorVersion ||
            (major == gl.majorVersion && minor < gl.minorVersion)) {
            NK_GL_ERR("Requested GL %d.%d but got %d.%d\n",
                      gl.majorVersion, gl.minorVersion, major, minor);
            return false;
        }
    }

    // Installation du debug callback GLAD2
    if (gl.glad2.installDebugCallback &&
        HasFlag(gl.contextFlags, NkGLContextFlags::Debug)) {
#if defined(NKENTSEU_PLATFORM_WINDOWS) || defined(NKENTSEU_WINDOWING_XLIB) \
 || defined(NKENTSEU_WINDOWING_XCB)
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(GLDebugCallback, &mDesc.opengl);
        // Désactiver les messages de notification (trop verbeux)
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                              GL_DEBUG_SEVERITY_NOTIFICATION,
                              0, nullptr, GL_FALSE);
        NK_GL_LOG("GL debug callback installed\n");
#endif
    }

    // Remplir les infos
    mData.renderer = (const char*)glGetString(GL_RENDERER);
    mData.vendor   = (const char*)glGetString(GL_VENDOR);
    mData.version  = (const char*)glGetString(GL_VERSION);
    return true;
}

// =============================================================================
//  WGL (Windows) — GLAD2 remplace tous les magic numbers
// =============================================================================
#if defined(NKENTSEU_PLATFORM_WINDOWS)

// Construit le PIXELFORMATDESCRIPTOR depuis NkWGLFallbackPixelFormat
static PIXELFORMATDESCRIPTOR BuildPFD(const NkWGLFallbackPixelFormat& fmt) {
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize        = sizeof(pfd);
    pfd.nVersion     = fmt.version;
    pfd.dwFlags      = static_cast<DWORD>(fmt.flags);
    pfd.iPixelType   = (fmt.pixelType == NkPFDPixelType::RGBA)
                       ? PFD_TYPE_RGBA : PFD_TYPE_COLORINDEX;
    pfd.cColorBits   = fmt.colorBits;
    pfd.cAlphaBits   = fmt.alphaBits;
    pfd.cDepthBits   = fmt.depthBits;
    pfd.cStencilBits = fmt.stencilBits;
    pfd.cAccumBits   = fmt.accumBits;
    pfd.cAuxBuffers  = fmt.auxBuffers;
    pfd.iLayerType   = PFD_MAIN_PLANE;
    return pfd;
}

bool NkOpenGLContext::InitWGL(const NkSurfaceDesc& surface,
                               const NkOpenGLDesc& gl) {
    mData.hwnd = surface.hwnd;
    mData.hdc  = GetDC(surface.hwnd);
    if (!mData.hdc) { NK_GL_ERR("GetDC failed\n"); return false; }

    // ── Étape 1 : Contexte bootstrap avec NkWGLFallbackPixelFormat ───────────
    // L'utilisateur contrôle ce PIXELFORMATDESCRIPTOR via gl.wglFallback.
    PIXELFORMATDESCRIPTOR pfd = BuildPFD(gl.wglFallback);
    int tempFmt = ChoosePixelFormat(mData.hdc, &pfd);
    if (!tempFmt || !SetPixelFormat(mData.hdc, tempFmt, &pfd)) {
        NK_GL_ERR("Bootstrap SetPixelFormat failed\n"); return false;
    }
    HGLRC tempCtx = wglCreateContext(mData.hdc);
    if (!tempCtx) { NK_GL_ERR("Bootstrap wglCreateContext failed\n"); return false; }
    wglMakeCurrent(mData.hdc, tempCtx);

    // ── Étape 2 : GLAD2 charge les extensions WGL ────────────────────────────
    // gladLoadWGL charge : wglCreateContextAttribsARB, wglChoosePixelFormatARB,
    // wglSwapIntervalEXT, WGL_ARB_framebuffer_sRGB, etc.
    // Plus aucun magic number nécessaire après ça.
    if (!gladLoadWGL(mData.hdc, (GLADloadfunc)wglGetProcAddress)) {
        NK_GL_ERR("gladLoadWGL (bootstrap) failed\n");
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(tempCtx);
        return false;
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(tempCtx);

    // ── Étape 3 : Pixel format ARB final — noms GLAD2 ────────────────────────
    // Toutes les constantes viennent maintenant de glad/wgl.h
    const int pfAttribs[] = {
        WGL_DRAW_TO_WINDOW_ARB,             1,
        WGL_SUPPORT_OPENGL_ARB,             1,
        WGL_DOUBLE_BUFFER_ARB,              gl.doubleBuffer ? 1 : 0,
        WGL_PIXEL_TYPE_ARB,                 WGL_TYPE_RGBA_ARB,
        WGL_COLOR_BITS_ARB,                 gl.colorBits,
        WGL_RED_BITS_ARB,                   gl.redBits,
        WGL_GREEN_BITS_ARB,                 gl.greenBits,
        WGL_BLUE_BITS_ARB,                  gl.blueBits,
        WGL_ALPHA_BITS_ARB,                 gl.alphaBits,
        WGL_DEPTH_BITS_ARB,                 gl.depthBits,
        WGL_STENCIL_BITS_ARB,               gl.stencilBits,
        WGL_SAMPLE_BUFFERS_ARB,             gl.msaaSamples > 1 ? 1 : 0,
        WGL_SAMPLES_ARB,                    gl.msaaSamples,
        WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB,   gl.srgbFramebuffer ? 1 : 0,
        0
    };

    int  pixelFormat = 0;
    UINT numFormats  = 0;
    if (!wglChoosePixelFormatARB(mData.hdc, pfAttribs, nullptr,
                                  1, &pixelFormat, &numFormats)
        || numFormats == 0) {
        NK_GL_ERR("wglChoosePixelFormatARB failed\n"); return false;
    }

    // SetPixelFormat final (PIXELFORMATDESCRIPTOR en sortie — juste pour l'appel API)
    PIXELFORMATDESCRIPTOR finalPfd = {};
    DescribePixelFormat(mData.hdc, pixelFormat, sizeof(finalPfd), &finalPfd);
    SetPixelFormat(mData.hdc, pixelFormat, &finalPfd);

    // ── Étape 4 : Contexte final — noms GLAD2 ────────────────────────────────
    int ctxFlags = 0;
    if (HasFlag(gl.contextFlags, NkGLContextFlags::Debug))
        ctxFlags |= WGL_CONTEXT_DEBUG_BIT_ARB;
    if (HasFlag(gl.contextFlags, NkGLContextFlags::ForwardCompat))
        ctxFlags |= WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;
    if (HasFlag(gl.contextFlags, NkGLContextFlags::RobustAccess))
        ctxFlags |= WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB;
    if (HasFlag(gl.contextFlags, NkGLContextFlags::NoError) &&
        GLAD_WGL_ARB_create_context_no_error)
        ctxFlags |= WGL_CONTEXT_OPENGL_NO_ERROR_ARB;

    int profileMask = (gl.profile == NkGLProfile::Core)
                      ? WGL_CONTEXT_CORE_PROFILE_BIT_ARB
                      : WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;

    const int ctxAttribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, gl.majorVersion,
        WGL_CONTEXT_MINOR_VERSION_ARB, gl.minorVersion,
        WGL_CONTEXT_PROFILE_MASK_ARB,  profileMask,
        WGL_CONTEXT_FLAGS_ARB,         ctxFlags,
        0
    };
    mData.hglrc = wglCreateContextAttribsARB(mData.hdc, nullptr, ctxAttribs);
    if (!mData.hglrc) { NK_GL_ERR("wglCreateContextAttribsARB failed\n"); return false; }

    wglMakeCurrent(mData.hdc, mData.hglrc);

    // ── Étape 5 : VSync — noms GLAD2 ─────────────────────────────────────────
    if (GLAD_WGL_EXT_swap_control) {
        // VSync adaptatif si disponible et demandé
        if (gl.swapInterval == NkGLSwapInterval::AdaptiveVSync
            && GLAD_WGL_EXT_swap_control_tear) {
            wglSwapIntervalEXT(-1);
        } else {
            wglSwapIntervalEXT((int)gl.swapInterval);
        }
    }

    NK_GL_LOG("WGL context OK — GL %d.%d %s\n",
              gl.majorVersion, gl.minorVersion,
              gl.profile == NkGLProfile::Core ? "Core" : "Compatibility");
    return true;
}

void NkOpenGLContext::ShutdownWGL() {
    wglMakeCurrent(nullptr, nullptr);
    if (mData.hglrc) { wglDeleteContext(mData.hglrc); mData.hglrc = nullptr; }
    if (mData.hdc && mData.hwnd) {
        ReleaseDC(mData.hwnd, mData.hdc);
        mData.hdc = nullptr;
    }
}

void NkOpenGLContext::SwapWGL() { SwapBuffers(mData.hdc); }

void NkOpenGLContext::SetVSyncWGL(bool enabled) {
    if (GLAD_WGL_EXT_swap_control)
        wglSwapIntervalEXT(enabled ? 1 : 0);
}
#endif // NKENTSEU_PLATFORM_WINDOWS

// =============================================================================
//  GLX (XLib/XCB) — GLAD2
// =============================================================================
#if defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)

bool NkOpenGLContext::InitGLX(const NkSurfaceDesc& surface,
                               const NkOpenGLDesc& gl) {
#if defined(NKENTSEU_WINDOWING_XCB)
    mData.display = XOpenDisplay(nullptr);
    if (!mData.display) { NK_GL_ERR("XOpenDisplay failed\n"); return false; }
    mData.window  = (::Window)surface.window;
#else
    mData.display = surface.display;
    mData.window  = surface.window;
#endif
    int screen = DefaultScreen(mData.display);

    // ── GLAD2 charge GLX ─────────────────────────────────────────────────────
    if (!gladLoadGLX(mData.display, screen,
                     (GLADloadfunc)glXGetProcAddressARB)) {
        NK_GL_ERR("gladLoadGLX failed\n"); return false;
    }

    // Vérifier version GLX via GLAD2
    if (!GLAD_GLX_ARB_create_context || !GLAD_GLX_ARB_create_context_profile) {
        NK_GL_ERR("GLX_ARB_create_context not available\n"); return false;
    }

    // ── FBConfig — NkOpenGLDesc + NkGLXHints ─────────────────────────────────
    // Récupérer depuis les hints si PrepareWindowConfig a déjà tourné
    GLXFBConfig fbConfig = nullptr;
    uintptr fbPtr = surface.appliedHints.Get(NkSurfaceHintKey::NK_GLX_FB_CONFIG_PTR, 0);
    if (fbPtr) {
        fbConfig = reinterpret_cast<GLXFBConfig>(fbPtr);
        NK_GL_LOG("GLX: reusing FBConfig from surface hints\n");
    } else {
        // Construction des attributs depuis NkOpenGLDesc + NkGLXHints
        const NkGLXHints& hints = gl.glxHints;
        int drawableBits = 0;
        if (hints.allowWindow)  drawableBits |= GLX_WINDOW_BIT;
        if (hints.allowPixmap)  drawableBits |= GLX_PIXMAP_BIT;
        if (hints.allowPbuffer) drawableBits |= GLX_PBUFFER_BIT;
        if (drawableBits == 0)  drawableBits  = GLX_WINDOW_BIT;

        int renderType = hints.floatingPointFB
                         ? GLX_RGBA_FLOAT_BIT_ARB : GLX_RGBA_BIT;

        const int fbAttribs[] = {
            GLX_RENDER_TYPE,    renderType,
            GLX_DRAWABLE_TYPE,  drawableBits,
            GLX_X_VISUAL_TYPE,  GLX_TRUE_COLOR,
            GLX_RED_SIZE,       hints.redBits,
            GLX_GREEN_SIZE,     hints.greenBits,
            GLX_BLUE_SIZE,      hints.blueBits,
            GLX_ALPHA_SIZE,     hints.alphaBits,
            GLX_DEPTH_SIZE,     gl.depthBits,
            GLX_STENCIL_SIZE,   gl.stencilBits,
            GLX_DOUBLEBUFFER,   gl.doubleBuffer ? True : False,
            GLX_STEREO,         hints.stereoRendering ? True : False,
            GLX_SAMPLE_BUFFERS, gl.msaaSamples > 1 ? 1 : 0,
            GLX_SAMPLES,        gl.msaaSamples,
            None
        };
        int count = 0;
        GLXFBConfig* cfgs = glXChooseFBConfig(mData.display, screen,
                                               fbAttribs, &count);
        if (!cfgs || count == 0) {
            NK_GL_ERR("glXChooseFBConfig failed\n"); return false;
        }
        fbConfig      = cfgs[0];
        mData.fbConfig = fbConfig;
        XFree(cfgs);
    }

    // ── Contexte final — noms GLAD2 ───────────────────────────────────────────
    int ctxFlags = 0;
    if (HasFlag(gl.contextFlags, NkGLContextFlags::Debug))
        ctxFlags |= GLX_CONTEXT_DEBUG_BIT_ARB;
    if (HasFlag(gl.contextFlags, NkGLContextFlags::ForwardCompat))
        ctxFlags |= GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;

    int profileMask = (gl.profile == NkGLProfile::Core)
                      ? GLX_CONTEXT_CORE_PROFILE_BIT_ARB
                      : GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;

    const int ctxAttribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, gl.majorVersion,
        GLX_CONTEXT_MINOR_VERSION_ARB, gl.minorVersion,
        GLX_CONTEXT_PROFILE_MASK_ARB,  profileMask,
        GLX_CONTEXT_FLAGS_ARB,         ctxFlags,
        None
    };
    mData.context = glXCreateContextAttribsARB(
        mData.display, fbConfig, nullptr, True, ctxAttribs);
    if (!mData.context) { NK_GL_ERR("glXCreateContextAttribsARB failed\n"); return false; }

    glXMakeCurrent(mData.display, mData.window, mData.context);
    SetVSyncGLX(gl.swapInterval == NkGLSwapInterval::VSync);

    NK_GL_LOG("GLX context OK — GL %d.%d\n", gl.majorVersion, gl.minorVersion);
    return true;
}

void NkOpenGLContext::ShutdownGLX() {
    glXMakeCurrent(mData.display, None, nullptr);
    if (mData.context) glXDestroyContext(mData.display, mData.context);
#if defined(NKENTSEU_WINDOWING_XCB)
    XCloseDisplay(mData.display);
#endif
    mData.context = nullptr;
    mData.display = nullptr;
}

void NkOpenGLContext::SwapGLX() {
    glXSwapBuffers(mData.display, mData.window);
}

void NkOpenGLContext::SetVSyncGLX(bool enabled) {
    // GLAD2 expose glXSwapIntervalEXT directement si l'extension est dispo
    if (GLAD_GLX_EXT_swap_control)
        glXSwapIntervalEXT(mData.display, mData.window, enabled ? 1 : 0);
    else if (GLAD_GLX_MESA_swap_control)
        glXSwapIntervalMESA(enabled ? 1 : 0);
    else if (GLAD_GLX_SGI_swap_control)
        glXSwapIntervalSGI(enabled ? 1 : 0);
}
#endif // NKENTSEU_WINDOWING_XLIB || XCB

// =============================================================================
//  EGL (Wayland / Android) — GLAD2
// =============================================================================
#if defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)

bool NkOpenGLContext::InitEGL(const NkSurfaceDesc& surface,
                               const NkOpenGLDesc& gl) {
    const NkEGLHints& hints = gl.eglHints;

#if defined(NKENTSEU_WINDOWING_WAYLAND)
    mData.eglDisplay = eglGetDisplay((EGLNativeDisplayType)surface.display);
#else
    mData.eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
#endif
    if (mData.eglDisplay == EGL_NO_DISPLAY) return false;

    EGLint major, minor;
    if (!eglInitialize(mData.eglDisplay, &major, &minor)) return false;

    // GLAD2 charge EGL
    if (!gladLoadEGL(mData.eglDisplay, (GLADloadfunc)eglGetProcAddress)) {
        NK_GL_ERR("gladLoadEGL failed\n"); return false;
    }

    int renderType = (gl.profile == NkGLProfile::ES)
                     ? EGL_OPENGL_ES3_BIT : EGL_OPENGL_BIT;
    if (hints.openGLConformant) renderType |= EGL_OPENGL_BIT;

    int surfaceTypes = EGL_WINDOW_BIT;
    if (hints.pbufferSurface) surfaceTypes |= EGL_PBUFFER_BIT;

    const EGLint cfgAttribs[] = {
        EGL_RENDERABLE_TYPE, renderType,
        EGL_SURFACE_TYPE,    surfaceTypes,
        EGL_RED_SIZE,        hints.redBits,
        EGL_GREEN_SIZE,      hints.greenBits,
        EGL_BLUE_SIZE,       hints.blueBits,
        EGL_ALPHA_SIZE,      hints.alphaBits,
        EGL_DEPTH_SIZE,      gl.depthBits,
        EGL_STENCIL_SIZE,    gl.stencilBits,
        EGL_SAMPLE_BUFFERS,  gl.msaaSamples > 1 ? 1 : 0,
        EGL_SAMPLES,         gl.msaaSamples,
        hints.bindToTextureRGBA ? EGL_BIND_TO_TEXTURE_RGBA : EGL_NONE,
        hints.bindToTextureRGBA ? EGL_TRUE : EGL_NONE,
        EGL_NONE
    };

    EGLint numCfg;
    eglChooseConfig(mData.eglDisplay, cfgAttribs, &mData.eglConfig, 1, &numCfg);
    if (numCfg == 0) { NK_GL_ERR("eglChooseConfig failed\n"); return false; }

#if defined(NKENTSEU_WINDOWING_WAYLAND)
    EGLNativeWindowType nativeWin = (EGLNativeWindowType)surface.surface;
#else
    EGLNativeWindowType nativeWin = surface.nativeWindow;
#endif

    mData.eglSurface = eglCreateWindowSurface(mData.eglDisplay,
                                               mData.eglConfig, nativeWin, nullptr);
    if (mData.eglSurface == EGL_NO_SURFACE) {
        NK_GL_ERR("eglCreateWindowSurface failed\n"); return false;
    }

    eglBindAPI(gl.profile == NkGLProfile::ES ? EGL_OPENGL_ES_API : EGL_OPENGL_API);

    const EGLint ctxAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, gl.majorVersion,
        EGL_NONE
    };
    mData.eglContext = eglCreateContext(mData.eglDisplay, mData.eglConfig,
                                         EGL_NO_CONTEXT, ctxAttribs);
    if (mData.eglContext == EGL_NO_CONTEXT) {
        NK_GL_ERR("eglCreateContext failed\n"); return false;
    }
    eglMakeCurrent(mData.eglDisplay, mData.eglSurface,
                   mData.eglSurface, mData.eglContext);
    eglSwapInterval(mData.eglDisplay,
                    gl.swapInterval == NkGLSwapInterval::Immediate ? 0 : 1);
    return true;
}

void NkOpenGLContext::ShutdownEGL() {
    eglMakeCurrent(mData.eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (mData.eglContext != EGL_NO_CONTEXT)
        eglDestroyContext(mData.eglDisplay, mData.eglContext);
    if (mData.eglSurface != EGL_NO_SURFACE)
        eglDestroySurface(mData.eglDisplay, mData.eglSurface);
    eglTerminate(mData.eglDisplay);
}
void NkOpenGLContext::SwapEGL() {
    eglSwapBuffers(mData.eglDisplay, mData.eglSurface);
}
void NkOpenGLContext::SetVSyncEGL(bool enabled) {
    eglSwapInterval(mData.eglDisplay, enabled ? 1 : 0);
}
#endif // EGL

// =============================================================================
//  Reste des méthodes de l'interface (BeginFrame / EndFrame / Present / etc.)
//  Identiques à la version précédente — non répétées ici.
// =============================================================================

} // namespace nkentseu
