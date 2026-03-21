// =============================================================================
// NkOpenGLContext.cpp — Production Ready
// GLAD2 optionnel : #define NK_NO_GLAD2 pour utiliser un loader externe.
// =============================================================================
#include "NkOpenGLContext.h"
#include "NKWindow/Core/NkWindow.h"  // Adaptez selon votre include NkWindow
#include "NKWindow/Core/NkSurface.h"  // Adaptez selon votre include NkWindow
#include "NKLogger/NkLog.h"

#include <cstring>

// Désactive automatiquement GLAD2 si les headers ne sont pas disponibles.
#if !defined(NK_NO_GLAD2) && defined(__has_include)
    #   if defined(NKENTSEU_PLATFORM_WINDOWS)
    #       if !__has_include(<glad/wgl.h>) || !__has_include(<glad/gl.h>)
    #           define NK_NO_GLAD2
    #       endif
    #   elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
    #       if !__has_include(<glad/glx.h>) || !__has_include(<glad/gl.h>)
    #           define NK_NO_GLAD2
    #       endif
    #   elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
    #       if !__has_include(<glad/egl.h>) || !__has_include(<glad/gles2.h>)
    #           define NK_NO_GLAD2
    #       endif
    #   elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    #       if !__has_include(<glad/gles2.h>)
    #           define NK_NO_GLAD2
    #       endif
    #   else
    #       if !__has_include(<glad/gl.h>)
    #           define NK_NO_GLAD2
    #       endif
    #   endif
#endif

// ── Includes GLAD2 optionnels ─────────────────────────────────────────────────
#ifndef NK_NO_GLAD2
    #   if defined(NKENTSEU_PLATFORM_WINDOWS)
    #       include <glad/wgl.h>
    #       include <glad/gl.h>
    #   elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
    #       include <glad/glx.h>
    #       include <glad/gl.h>
    #   elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
    #       include <glad/egl.h>
    #       include <glad/gles2.h>
    #   elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    #       include <glad/gles2.h>
    #       include <emscripten/html5.h>
    #   else
    #       include <glad/gl.h>
    #   endif
#else
    // Stubs GLAD2 pour compilation sans GLAD2
    #   if defined(NKENTSEU_PLATFORM_WINDOWS)
    #       include <GL/gl.h>
    #   elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
    #       include <GL/gl.h>
    #       include <GL/glx.h>
    #   elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
    #       include <EGL/egl.h>
    #       include <GLES3/gl32.h>
    #   elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    #       include <emscripten/html5.h>
    #       include <GLES3/gl3.h>
    #   endif
    #   define GLAD_WGL_EXT_swap_control           0
    #   define GLAD_WGL_EXT_swap_control_tear      0
    #   define GLAD_WGL_ARB_create_context_no_error 0
    #   define GLAD_GLX_EXT_swap_control           0
    #   define GLAD_GLX_MESA_swap_control          0
    #   define GLAD_GLX_SGI_swap_control           0
    #   define GLAD_GLX_ARB_create_context         1
    #   define GLAD_GLX_ARB_create_context_profile 1
#endif

// Constantes WGL ARB (utilisées si GLAD2 absent, magic numbers nommés)
#ifndef WGL_DRAW_TO_WINDOW_ARB
    #define WGL_DRAW_TO_WINDOW_ARB             0x2001
    #define WGL_SUPPORT_OPENGL_ARB             0x2010
    #define WGL_DOUBLE_BUFFER_ARB              0x2011
    #define WGL_PIXEL_TYPE_ARB                 0x2013
    #define WGL_TYPE_RGBA_ARB                  0x202B
    #define WGL_COLOR_BITS_ARB                 0x2014
    #define WGL_RED_BITS_ARB                   0x2015
    #define WGL_GREEN_BITS_ARB                 0x2017
    #define WGL_BLUE_BITS_ARB                  0x2019
    #define WGL_ALPHA_BITS_ARB                 0x201B
    #define WGL_DEPTH_BITS_ARB                 0x2022
    #define WGL_STENCIL_BITS_ARB               0x2023
    #define WGL_SAMPLE_BUFFERS_ARB             0x2041
    #define WGL_SAMPLES_ARB                    0x2042
    #define WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB   0x20A9
    #define WGL_CONTEXT_MAJOR_VERSION_ARB      0x2091
    #define WGL_CONTEXT_MINOR_VERSION_ARB      0x2092
    #define WGL_CONTEXT_FLAGS_ARB              0x2094
    #define WGL_CONTEXT_PROFILE_MASK_ARB       0x9126
    #define WGL_CONTEXT_CORE_PROFILE_BIT_ARB   0x00000001
    #define WGL_CONTEXT_COMPAT_PROFILE_BIT_ARB 0x00000002
    #define WGL_CONTEXT_DEBUG_BIT_ARB          0x00000001
    #define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x00000002
    #define WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB  0x00000004
#endif

// GLAD expose `WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB` whereas some code
// paths use the shorter `WGL_CONTEXT_COMPAT_PROFILE_BIT_ARB`.
#ifndef WGL_CONTEXT_COMPAT_PROFILE_BIT_ARB
    #ifdef WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB
        #define WGL_CONTEXT_COMPAT_PROFILE_BIT_ARB WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB
    #else
        #define WGL_CONTEXT_COMPAT_PROFILE_BIT_ARB 0x00000002
    #endif
#endif

namespace nkentseu {

    #if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NK_NO_GLAD2)
    static GLADapiproc NkWglGetProcAddressCompat(const char* name) {
        GLADapiproc proc = reinterpret_cast<GLADapiproc>(wglGetProcAddress(name));
        if (proc == nullptr ||
            proc == reinterpret_cast<GLADapiproc>(0x1) ||
            proc == reinterpret_cast<GLADapiproc>(0x2) ||
            proc == reinterpret_cast<GLADapiproc>(0x3) ||
            proc == reinterpret_cast<GLADapiproc>(-1)) {
            static HMODULE sOpenGL32 = LoadLibraryA("opengl32.dll");
            if (sOpenGL32) {
                proc = reinterpret_cast<GLADapiproc>(GetProcAddress(sOpenGL32, name));
            }
        }
        return proc;
    }
    #endif

    // ── Debug callback GL ─────────────────────────────────────────────────────────
    #ifndef NK_NO_GLAD2
        #if defined(NKENTSEU_PLATFORM_WINDOWS) \
        || defined(NKENTSEU_WINDOWING_XLIB) \
        || defined(NKENTSEU_WINDOWING_XCB)
            static void GLAPIENTRY GLDebugCallback(
                GLenum src, GLenum type, GLuint id, GLenum severity,
                GLsizei /*len*/, const GLchar* msg, const void* user) {
                const NkOpenGLDesc* d = static_cast<const NkOpenGLDesc*>(user);
                if (!d) return;
                uint32 sev = (severity==GL_DEBUG_SEVERITY_HIGH)   ? 3 :
                            (severity==GL_DEBUG_SEVERITY_MEDIUM)  ? 2 :
                            (severity==GL_DEBUG_SEVERITY_LOW)     ? 1 : 0;
                if (sev < d->glad2.debugSeverityLevel) return;
                const char* ss = sev==3?"HIGH":sev==2?"MEDIUM":sev==1?"LOW":"NOTIF";
                const char* ts = type==GL_DEBUG_TYPE_ERROR?"ERROR":
                                type==GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR?"UB":
                                type==GL_DEBUG_TYPE_PERFORMANCE?"PERF":"OTHER";
                logger.Error("[NkOpenGL] [{0}][{1}] id={2} : {3}", ss, ts, id, msg);
                (void)src;
            }
        #endif
    #endif // NK_NO_GLAD2

    // =============================================================================
    NkOpenGLContext::~NkOpenGLContext() { if (mIsValid) Shutdown(); }

    bool NkOpenGLContext::Initialize(const NkWindow& window, const NkContextDesc& desc) {
        if (mIsValid) { logger.Error("[NkOpenGL] Already initialized"); return false; }
        mDesc = desc;
        const NkSurfaceDesc surf = window.GetSurfaceDesc();
        if (!surf.IsValid()) { logger.Error("[NkOpenGL] Invalid NkSurfaceDesc"); return false; }

        bool ok = false;
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        ok = InitWGL(surf, desc.opengl);
    #elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
        ok = InitGLX(surf, desc.opengl);
    #elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
        ok = InitEGL(surf, desc.opengl);
    #elif defined(NKENTSEU_PLATFORM_MACOS)
        ok = InitNSGL(surf, desc.opengl);
    #elif defined(NKENTSEU_PLATFORM_IOS)
        ok = InitEAGL(surf, desc.opengl);
    #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        ok = InitWebGL(surf, desc.opengl);
    #else
        logger.Error("[NkOpenGL] No OpenGL backend for this platform");
    #endif
        if (!ok) return false;

    #ifndef NK_NO_GLAD2
        if (desc.opengl.glad2.autoLoad) {
            if (!LoadGLAD2(desc.opengl)) { Shutdown(); return false; }
        } else { FillInfo(); }
    #else
        FillInfo();
    #endif

        mData.width  = surf.width;
        mData.height = surf.height;
        mIsValid     = true;
        mVSync       = (desc.opengl.swapInterval != NkGLSwapInterval::Immediate);
        logger.Info("[NkOpenGL] Ready - {0} | {1} | {2}", mData.renderer, mData.version, mData.vendor);
        return true;
    }

    // =============================================================================
    void NkOpenGLContext::FillInfo() {
        const unsigned char* r = glGetString(GL_RENDERER);
        const unsigned char* v = glGetString(GL_VENDOR);
        const unsigned char* s = glGetString(GL_VERSION);
        mData.renderer = r ? (const char*)r : "Unknown";
        mData.vendor   = v ? (const char*)v : "Unknown";
        mData.version  = s ? (const char*)s : "Unknown";
    }

    bool NkOpenGLContext::LoadGLAD2(const NkOpenGLDesc& gl) {
    #ifndef NK_NO_GLAD2
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        if (!gladLoadWGL(mData.hdc, NkWglGetProcAddressCompat)) {
            logger.Error("[NkOpenGL] gladLoadWGL failed"); return false;
        }
        int ver = gladLoadGL(NkWglGetProcAddressCompat);
        if (!ver) { logger.Error("[NkOpenGL] gladLoadGL failed"); return false; }
        logger.Info("[NkOpenGL] GLAD2 GL {0}.{1}", GLAD_VERSION_MAJOR(ver), GLAD_VERSION_MINOR(ver));

    #elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
        const char* wslInterop = std::getenv("WSL_INTEROP");
        const char* wslDistro  = std::getenv("WSL_DISTRO_NAME");
        const bool runningInWsl = (wslInterop && *wslInterop) || (wslDistro && *wslDistro);
        if (!runningInWsl) {
            int screen = DefaultScreen(mData.display);
            if (!gladLoadGLX(mData.display, screen, (GLADloadfunc)glXGetProcAddressARB)) {
                logger.Warn("[NkOpenGL] gladLoadGLX failed, continuing with GL loader only");
            }
        } else {
            logger.Warn("[NkOpenGL] WSL detected: skip gladLoadGLX pre-load");
        }
        int ver = gladLoadGL((GLADloadfunc)glXGetProcAddressARB);
        if (!ver) { logger.Error("[NkOpenGL] gladLoadGL(GLX) failed"); return false; }

    #elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
        if (!gladLoadEGL(mData.eglDisplay, (GLADloadfunc)eglGetProcAddress)) {
            logger.Error("[NkOpenGL] gladLoadEGL failed"); return false;
        }
        int ver = gladLoadGLES2((GLADloadfunc)eglGetProcAddress);
        if (!ver) { logger.Error("[NkOpenGL] gladLoadGLES2 failed"); return false; }

    #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        int ver = gladLoadGLES2((GLADloadfunc)emscripten_webgl_get_proc_address);
        if (!ver) { logger.Error("[NkOpenGL] gladLoadGLES2(WebGL) failed"); return false; }
    #endif

        if (gl.glad2.validateVersion) {
            int maj=0, min=0;
            glGetIntegerv(GL_MAJOR_VERSION, &maj);
            glGetIntegerv(GL_MINOR_VERSION, &min);
            if (maj < gl.majorVersion ||
                (maj==gl.majorVersion && min < gl.minorVersion)) {
                logger.Error("[NkOpenGL] Requested GL {0}.{1}, got {2}.{3}",
                    gl.majorVersion, gl.minorVersion, maj, min);
                return false;
            }
        }

        if (gl.glad2.installDebugCallback &&
            HasFlag(gl.contextFlags, NkGLContextFlags::Debug)) {
    #if defined(NKENTSEU_PLATFORM_WINDOWS) \
    || defined(NKENTSEU_WINDOWING_XLIB) \
    || defined(NKENTSEU_WINDOWING_XCB)
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(GLDebugCallback, &mDesc.opengl);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
            logger.Info("[NkOpenGL] Debug callback installed");
    #endif
        }
    #endif // NK_NO_GLAD2
        FillInfo();
        return true;
    }

    // =============================================================================
    void NkOpenGLContext::Shutdown() {
        if (!mIsValid) return;
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        ShutdownWGL();
    #elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
        ShutdownGLX();
    #elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
        ShutdownEGL();
    #elif defined(NKENTSEU_PLATFORM_MACOS)
        ShutdownNSGL();
    #elif defined(NKENTSEU_PLATFORM_IOS)
        ShutdownEAGL();
    #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        ShutdownWebGL();
    #endif
        mIsValid = false;
        logger.Info("[NkOpenGL] Shutdown OK");
    }

    bool NkOpenGLContext::BeginFrame() { return mIsValid; }
    void NkOpenGLContext::EndFrame()   { /* optionnel : glFlush() */ }

    void NkOpenGLContext::Present() {
        if (!mIsValid) return;
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        SwapWGL();
    #elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
        SwapGLX();
    #elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
        SwapEGL();
    #elif defined(NKENTSEU_PLATFORM_MACOS)
        SwapNSGL();
    #elif defined(NKENTSEU_PLATFORM_IOS)
        SwapEAGL();
    #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        SwapWebGL();
    #endif
    }

    bool NkOpenGLContext::OnResize(uint32 w, uint32 h) {
        if (!mIsValid) return false;
        if (w == 0 || h == 0) return true; // Fenêtre minimisée — skip
        mData.width  = w;
        mData.height = h;
        // GL suit la surface automatiquement (le viewport est géré par l'utilisateur)
        return true;
    }

    void NkOpenGLContext::SetVSync(bool enabled) {
        mVSync = enabled;
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        SetVSyncWGL(enabled);
    #elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
        SetVSyncGLX(enabled);
    #elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
        SetVSyncEGL(enabled);
    #endif
    }

    bool          NkOpenGLContext::GetVSync()  const { return mVSync; }
    bool          NkOpenGLContext::IsValid()   const { return mIsValid; }
    NkGraphicsApi NkOpenGLContext::GetApi()    const { return mDesc.api; }
    NkContextDesc NkOpenGLContext::GetDesc()   const { return mDesc; }
    void*         NkOpenGLContext::GetNativeContextData() { return &mData; }

    bool NkOpenGLContext::SupportsCompute() const {
        return mDesc.opengl.majorVersion > 4 ||
            (mDesc.opengl.majorVersion == 4 && mDesc.opengl.minorVersion >= 3);
    }

    NkContextInfo NkOpenGLContext::GetInfo() const {
        NkContextInfo i;
        i.api              = mDesc.api;
        i.renderer         = mData.renderer;
        i.vendor           = mData.vendor;
        i.version          = mData.version;
        i.debugMode        = HasFlag(mDesc.opengl.contextFlags, NkGLContextFlags::Debug);
        i.computeSupported = SupportsCompute();
        return i;
    }

    bool NkOpenGLContext::MakeCurrent() {
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        return wglMakeCurrent(mData.hdc, mData.hglrc) == TRUE;
    #elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
        return glXMakeCurrent(mData.display, mData.window, mData.context) == True;
    #elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
        return eglMakeCurrent(mData.eglDisplay, mData.eglSurface,
                            mData.eglSurface, mData.eglContext) == EGL_TRUE;
    #else
        return false;
    #endif
    }

    void NkOpenGLContext::ReleaseCurrent() {
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        wglMakeCurrent(nullptr, nullptr);
    #elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
        glXMakeCurrent(mData.display, None, nullptr);
    #elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
        eglMakeCurrent(mData.eglDisplay, EGL_NO_SURFACE,
                    EGL_NO_SURFACE, EGL_NO_CONTEXT);
    #endif
    }

    NkOpenGLContext* NkOpenGLContext::CreateSharedContext(const NkWindow& window) {
        if (!mIsValid) return nullptr;
        auto* shared = new NkOpenGLContext();
        shared->mSharedParent = this;
        NkContextDesc sharedDesc = mDesc;
        if (!shared->Initialize(window, sharedDesc)) {
            delete shared; return nullptr;
        }
        return shared;
    }

    // =============================================================================
    //  WGL — Windows
    // =============================================================================
    #if defined(NKENTSEU_PLATFORM_WINDOWS)

    static PIXELFORMATDESCRIPTOR BuildPFD(const NkWGLFallbackPixelFormat& f) {
        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize        = sizeof(pfd);
        pfd.nVersion     = f.version;
        pfd.dwFlags      = static_cast<DWORD>(f.flags);
        pfd.iPixelType   = (f.pixelType == NkPFDPixelType::RGBA)
                        ? PFD_TYPE_RGBA : PFD_TYPE_COLORINDEX;
        pfd.cColorBits   = f.colorBits;
        pfd.cAlphaBits   = f.alphaBits;
        pfd.cDepthBits   = f.depthBits;
        pfd.cStencilBits = f.stencilBits;
        pfd.cAccumBits   = f.accumBits;
        pfd.cAuxBuffers  = f.auxBuffers;
        pfd.iLayerType   = PFD_MAIN_PLANE;
        return pfd;
    }

    bool NkOpenGLContext::InitWGL(const NkSurfaceDesc& surf, const NkOpenGLDesc& gl) {
        mData.hwnd = static_cast<HWND>(surf.hwnd);
        mData.hdc  = GetDC(mData.hwnd);
        if (!mData.hdc) { logger.Error("[NkOpenGL] GetDC failed"); return false; }

        // Étape 1 — Bootstrap avec NkWGLFallbackPixelFormat (configurable utilisateur)
        PIXELFORMATDESCRIPTOR pfd = BuildPFD(gl.wglFallback);
        int tmpFmt = ChoosePixelFormat(mData.hdc, &pfd);
        if (!tmpFmt || !SetPixelFormat(mData.hdc, tmpFmt, &pfd)) {
            logger.Error("[NkOpenGL] Bootstrap SetPixelFormat failed"); return false;
        }
        HGLRC tmpCtx = wglCreateContext(mData.hdc);
        if (!tmpCtx) { logger.Error("[NkOpenGL] Bootstrap wglCreateContext failed"); return false; }
        wglMakeCurrent(mData.hdc, tmpCtx);

        // Étape 2 — Charger les extensions WGL ARB
    #ifndef NK_NO_GLAD2
        if (!gladLoadWGL(mData.hdc, NkWglGetProcAddressCompat)) {
            logger.Error("[NkOpenGL] gladLoadWGL failed");
            wglMakeCurrent(nullptr, nullptr); wglDeleteContext(tmpCtx); return false;
        }
    #else
        mData._wglCreateCtxARB    = (PFNWGLCREATECONTEXTATTRIBSARBPROC)
            wglGetProcAddress("wglCreateContextAttribsARB");
        mData._wglChoosePFARB     = (PFNWGLCHOOSEPIXELFORMATARBPROC)
            wglGetProcAddress("wglChoosePixelFormatARB");
        mData._wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)
            wglGetProcAddress("wglSwapIntervalEXT");
        if (!mData._wglCreateCtxARB || !mData._wglChoosePFARB) {
            logger.Error("[NkOpenGL] WGL ARB extensions not found - driver too old?");
            wglMakeCurrent(nullptr, nullptr); wglDeleteContext(tmpCtx); return false;
        }
    #endif
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(tmpCtx);

        // Étape 3 — Pixel format ARB final (attributs issus de NkOpenGLDesc)
        const int pfAttribs[] = {
            WGL_DRAW_TO_WINDOW_ARB,           1,
            WGL_SUPPORT_OPENGL_ARB,           1,
            WGL_DOUBLE_BUFFER_ARB,            gl.doubleBuffer ? 1 : 0,
            WGL_PIXEL_TYPE_ARB,               WGL_TYPE_RGBA_ARB,
            WGL_COLOR_BITS_ARB,               gl.colorBits,
            WGL_RED_BITS_ARB,                 gl.redBits,
            WGL_GREEN_BITS_ARB,               gl.greenBits,
            WGL_BLUE_BITS_ARB,                gl.blueBits,
            WGL_ALPHA_BITS_ARB,               gl.alphaBits,
            WGL_DEPTH_BITS_ARB,               gl.depthBits,
            WGL_STENCIL_BITS_ARB,             gl.stencilBits,
            WGL_SAMPLE_BUFFERS_ARB,           gl.msaaSamples > 1 ? 1 : 0,
            WGL_SAMPLES_ARB,                  gl.msaaSamples,
            WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, gl.srgbFramebuffer ? 1 : 0,
            0
        };
        int pixFmt = 0; UINT numFmt = 0;
    #ifndef NK_NO_GLAD2
        if (!wglChoosePixelFormatARB(mData.hdc, pfAttribs, nullptr, 1, &pixFmt, &numFmt) || numFmt == 0) {
            logger.Error("[NkOpenGL] wglChoosePixelFormatARB failed"); return false;
        }
    #else
        if (!mData._wglChoosePFARB(mData.hdc, pfAttribs, nullptr, 1, &pixFmt, &numFmt) || numFmt == 0) {
            logger.Error("[NkOpenGL] wglChoosePixelFormatARB (manual) failed"); return false;
        }
    #endif

        // SetPixelFormat final (PFD en output — obligatoire même avec ARB)
        PIXELFORMATDESCRIPTOR finalPfd = {};
        DescribePixelFormat(mData.hdc, pixFmt, sizeof(finalPfd), &finalPfd);
        SetPixelFormat(mData.hdc, pixFmt, &finalPfd);

        // Étape 4 — Contexte final avec attributs version/profil
        int ctxFlags = 0;
        if (HasFlag(gl.contextFlags, NkGLContextFlags::Debug))
            ctxFlags |= WGL_CONTEXT_DEBUG_BIT_ARB;
        if (HasFlag(gl.contextFlags, NkGLContextFlags::ForwardCompat))
            ctxFlags |= WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;
        if (HasFlag(gl.contextFlags, NkGLContextFlags::RobustAccess))
            ctxFlags |= WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB;

        int profileMask = (gl.profile == NkGLProfile::Core)
            ? WGL_CONTEXT_CORE_PROFILE_BIT_ARB
            : WGL_CONTEXT_COMPAT_PROFILE_BIT_ARB;

        const int ctxAttribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, gl.majorVersion,
            WGL_CONTEXT_MINOR_VERSION_ARB, gl.minorVersion,
            WGL_CONTEXT_PROFILE_MASK_ARB,  profileMask,
            WGL_CONTEXT_FLAGS_ARB,         ctxFlags,
            0
        };

        // Partage de ressources : contexte parent transmis comme shareContext
        HGLRC shareCtx = mSharedParent ? mSharedParent->mData.hglrc : nullptr;
    #ifndef NK_NO_GLAD2
        mData.hglrc = wglCreateContextAttribsARB(mData.hdc, shareCtx, ctxAttribs);
    #else
        mData.hglrc = mData._wglCreateCtxARB(mData.hdc, shareCtx, ctxAttribs);
    #endif
        if (!mData.hglrc) { logger.Error("[NkOpenGL] wglCreateContextAttribsARB failed"); return false; }
        wglMakeCurrent(mData.hdc, mData.hglrc);

        // Étape 5 — VSync
    #ifndef NK_NO_GLAD2
        if (GLAD_WGL_EXT_swap_control) {
            int interval = (int)gl.swapInterval;
            if (gl.swapInterval == NkGLSwapInterval::AdaptiveVSync
                && !GLAD_WGL_EXT_swap_control_tear)
                interval = 1; // Fallback VSync classique
            wglSwapIntervalEXT(interval);
        }
    #else
        if (mData._wglSwapIntervalEXT)
            mData._wglSwapIntervalEXT(
                gl.swapInterval == NkGLSwapInterval::Immediate ? 0 : 1);
    #endif

        logger.Info("[NkOpenGL] WGL OK (GL {0}.{1} {2})", gl.majorVersion, gl.minorVersion,
            gl.profile == NkGLProfile::Core ? "Core" : "Compat");
        return true;
    }

    void NkOpenGLContext::ShutdownWGL() {
        wglMakeCurrent(nullptr, nullptr);
        if (mData.hglrc) { wglDeleteContext(mData.hglrc); mData.hglrc = nullptr; }
        if (mData.hdc && mData.hwnd) {
            ReleaseDC(mData.hwnd, mData.hdc); mData.hdc = nullptr;
        }
    }
    void NkOpenGLContext::SwapWGL() { SwapBuffers(mData.hdc); }
    void NkOpenGLContext::SetVSyncWGL(bool on) {
    #ifndef NK_NO_GLAD2
        if (GLAD_WGL_EXT_swap_control) wglSwapIntervalEXT(on ? 1 : 0);
    #else
        if (mData._wglSwapIntervalEXT) mData._wglSwapIntervalEXT(on ? 1 : 0);
    #endif
    }
    #endif // WINDOWS

    // =============================================================================
    //  GLX — Linux XLib / XCB
    // =============================================================================
    #if defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)

    bool NkOpenGLContext::InitGLX(const NkSurfaceDesc& surf, const NkOpenGLDesc& gl) {
    #if defined(NKENTSEU_WINDOWING_XCB)
        // XCB utilise le bridge Xlib pour GLX
        mData.display = XOpenDisplay(nullptr);
        if (!mData.display) { logger.Error("[NkOpenGL] XOpenDisplay failed"); return false; }
        mData.window = (::Window)surf.window;
        if (!mData.window) {
            logger.Error("[NkOpenGL] Invalid XCB window handle for GLX");
            return false;
        }
    #else
        mData.display = static_cast<Display*>(surf.display);
        mData.window  = (::Window)surf.window;
        if (!mData.display || !mData.window) {
            logger.Error("[NkOpenGL] Invalid XLib surface handles for GLX");
            return false;
        }
    #endif
        int screen = DefaultScreen(mData.display);

        int glxMajor = 0;
        int glxMinor = 0;
        if (!glXQueryVersion(mData.display, &glxMajor, &glxMinor) ||
            (glxMajor == 1 && glxMinor < 3)) {
            logger.Error("[NkOpenGL] GLX 1.3+ required");
            return false;
        }

        // FBConfig — réutiliser celui injecté par PrepareWindowConfig ou en choisir un
        GLXFBConfig fbConfig = nullptr;
        uintptr fbPtr = surf.appliedHints.Get(NkSurfaceHintKey::NK_GLX_FB_CONFIG_PTR, 0);
        if (fbPtr) {
            fbConfig = reinterpret_cast<GLXFBConfig>(fbPtr);
            logger.Info("[NkOpenGL] GLX: reuse FBConfig from PrepareWindowConfig");
        } else {
            const NkGLXHints& h = gl.glxHints;
            int drawBits = GLX_WINDOW_BIT;
            if (h.allowPixmap)  drawBits |= GLX_PIXMAP_BIT;
            if (h.allowPbuffer) drawBits |= GLX_PBUFFER_BIT;

            VisualID windowVisualId = 0;
            XWindowAttributes winAttrs{};
            if (XGetWindowAttributes(mData.display, mData.window, &winAttrs) != 0 &&
                winAttrs.visual != nullptr) {
                windowVisualId = XVisualIDFromVisual(winAttrs.visual);
            }

            const int fbAttribsNoVisual[] = {
                GLX_RENDER_TYPE,   h.floatingPointFB ? GLX_RGBA_FLOAT_BIT_ARB : GLX_RGBA_BIT,
                GLX_DRAWABLE_TYPE, drawBits,
                GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
                GLX_RED_SIZE,      h.redBits,
                GLX_GREEN_SIZE,    h.greenBits,
                GLX_BLUE_SIZE,     h.blueBits,
                GLX_ALPHA_SIZE,    h.alphaBits,
                GLX_DEPTH_SIZE,    gl.depthBits,
                GLX_STENCIL_SIZE,  gl.stencilBits,
                GLX_DOUBLEBUFFER,  gl.doubleBuffer ? True : False,
                GLX_STEREO,        h.stereoRendering ? True : False,
                GLX_SAMPLE_BUFFERS,gl.msaaSamples > 1 ? 1 : 0,
                GLX_SAMPLES,       gl.msaaSamples,
                None
            };
            const int fbAttribsWithVisual[] = {
                GLX_RENDER_TYPE,   h.floatingPointFB ? GLX_RGBA_FLOAT_BIT_ARB : GLX_RGBA_BIT,
                GLX_DRAWABLE_TYPE, drawBits,
                GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
                GLX_RED_SIZE,      h.redBits,
                GLX_GREEN_SIZE,    h.greenBits,
                GLX_BLUE_SIZE,     h.blueBits,
                GLX_ALPHA_SIZE,    h.alphaBits,
                GLX_DEPTH_SIZE,    gl.depthBits,
                GLX_STENCIL_SIZE,  gl.stencilBits,
                GLX_DOUBLEBUFFER,  gl.doubleBuffer ? True : False,
                GLX_STEREO,        h.stereoRendering ? True : False,
                GLX_SAMPLE_BUFFERS,gl.msaaSamples > 1 ? 1 : 0,
                GLX_SAMPLES,       gl.msaaSamples,
                GLX_VISUAL_ID,     static_cast<int>(windowVisualId),
                None
            };
            const int* fbAttribs = windowVisualId ? fbAttribsWithVisual : fbAttribsNoVisual;
            int count = 0;
            GLXFBConfig* cfgs = glXChooseFBConfig(mData.display, screen, fbAttribs, &count);
            if (!cfgs || count == 0) { logger.Error("[NkOpenGL] glXChooseFBConfig failed"); return false; }
            fbConfig = mData.fbConfig = cfgs[0];
            XFree(cfgs);
        }

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

        GLXContext shareCtx = mSharedParent ? mSharedParent->mData.context : nullptr;

        using FnCreateContextAttribs = GLXContext(*)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
        auto createContextAttribs = reinterpret_cast<FnCreateContextAttribs>(
            glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB"));

        if (createContextAttribs) {
            mData.context = createContextAttribs(mData.display, fbConfig, shareCtx, True, ctxAttribs);
        } else {
            logger.Warn("[NkOpenGL] glXCreateContextAttribsARB unavailable, trying legacy GLX context");
        }

        if (!mData.context) {
            mData.context = glXCreateNewContext(mData.display, fbConfig, GLX_RGBA_TYPE, shareCtx, True);
        }

        if (!mData.context) { logger.Error("[NkOpenGL] Failed to create GLX context"); return false; }
        if (!glXMakeCurrent(mData.display, mData.window, mData.context)) {
            logger.Error("[NkOpenGL] glXMakeCurrent failed");
            return false;
        }
        SetVSyncGLX(gl.swapInterval != NkGLSwapInterval::Immediate);

        logger.Info("[NkOpenGL] GLX OK (GL {0}.{1})", gl.majorVersion, gl.minorVersion);
        return true;
    }

    void NkOpenGLContext::ShutdownGLX() {
        if (mData.display) {
            glXMakeCurrent(mData.display, None, nullptr);
            if (mData.context) {
                glXDestroyContext(mData.display, mData.context);
                mData.context = nullptr;
            }
        }
    #if defined(NKENTSEU_WINDOWING_XCB)
        if (mData.display) { XCloseDisplay(mData.display); mData.display = nullptr; }
    #endif
    }
    void NkOpenGLContext::SwapGLX() {
        if (!mData.display || !mData.window) return;
        glXSwapBuffers(mData.display, mData.window);
    }
    void NkOpenGLContext::SetVSyncGLX(bool on) {
        if (!mData.display || !mData.window) return;

        using FnSwapIntervalEXT = void(*)(Display*, GLXDrawable, int);
        using FnSwapIntervalMESA = int(*)(unsigned int);
        using FnSwapIntervalSGI = int(*)(int);

        auto fnExt = reinterpret_cast<FnSwapIntervalEXT>(
            glXGetProcAddressARB((const GLubyte*)"glXSwapIntervalEXT"));
        if (fnExt) {
            fnExt(mData.display, mData.window, on ? 1 : 0);
            return;
        }

        auto fnMesa = reinterpret_cast<FnSwapIntervalMESA>(
            glXGetProcAddressARB((const GLubyte*)"glXSwapIntervalMESA"));
        if (fnMesa) {
            fnMesa(on ? 1u : 0u);
            return;
        }

        auto fnSgi = reinterpret_cast<FnSwapIntervalSGI>(
            glXGetProcAddressARB((const GLubyte*)"glXSwapIntervalSGI"));
        if (fnSgi) {
            fnSgi(on ? 1 : 0);
        }
    }
    #endif // XLIB/XCB

    // =============================================================================
    //  EGL — Wayland / Android
    // =============================================================================
    #if defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)

    bool NkOpenGLContext::InitEGL(const NkSurfaceDesc& surf, const NkOpenGLDesc& gl) {
    #if defined(NKENTSEU_WINDOWING_WAYLAND)
        if (!surf.display || !surf.surface) {
            logger.Error("[NkOpenGL] Invalid Wayland surface handles for EGL");
            return false;
        }
        mData.eglDisplay = eglGetDisplay((EGLNativeDisplayType)surf.display);
    #else
        if (!surf.nativeWindow) {
            logger.Error("[NkOpenGL] Invalid Android native window for EGL");
            return false;
        }
        mData.eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    #endif
        if (mData.eglDisplay == EGL_NO_DISPLAY) { logger.Error("[NkOpenGL] eglGetDisplay failed"); return false; }

        EGLint maj, min;
        if (!eglInitialize(mData.eglDisplay, &maj, &min)) { logger.Error("[NkOpenGL] eglInitialize failed"); return false; }

    #ifndef NK_NO_GLAD2
        if (!gladLoadEGL(mData.eglDisplay, (GLADloadfunc)eglGetProcAddress)) {
            logger.Error("[NkOpenGL] gladLoadEGL failed"); return false;
        }
    #endif

        const NkEGLHints& h = gl.eglHints;
        int renderType = (gl.profile == NkGLProfile::ES) ? EGL_OPENGL_ES3_BIT : EGL_OPENGL_BIT;
        int surfTypes  = EGL_WINDOW_BIT | (h.pbufferSurface ? EGL_PBUFFER_BIT : 0);

        const EGLint cfgAttribs[] = {
            EGL_RENDERABLE_TYPE, renderType,
            EGL_SURFACE_TYPE,    surfTypes,
            EGL_RED_SIZE,        h.redBits,
            EGL_GREEN_SIZE,      h.greenBits,
            EGL_BLUE_SIZE,       h.blueBits,
            EGL_ALPHA_SIZE,      h.alphaBits,
            EGL_DEPTH_SIZE,      gl.depthBits,
            EGL_STENCIL_SIZE,    gl.stencilBits,
            EGL_SAMPLE_BUFFERS,  gl.msaaSamples > 1 ? 1 : 0,
            EGL_SAMPLES,         gl.msaaSamples,
            EGL_NONE
        };
        EGLint numCfg = 0;
        eglChooseConfig(mData.eglDisplay, cfgAttribs, &mData.eglConfig, 1, &numCfg);
        if (numCfg == 0) { logger.Error("[NkOpenGL] eglChooseConfig failed"); return false; }

    #if defined(NKENTSEU_WINDOWING_WAYLAND)
        EGLNativeWindowType nwin = (EGLNativeWindowType)surf.surface;
    #else
        EGLNativeWindowType nwin = (EGLNativeWindowType)surf.nativeWindow;
    #endif
        if (!nwin) { logger.Error("[NkOpenGL] EGL native window is null"); return false; }
        mData.eglSurface = eglCreateWindowSurface(mData.eglDisplay, mData.eglConfig, nwin, nullptr);
        if (mData.eglSurface == EGL_NO_SURFACE) { logger.Error("[NkOpenGL] eglCreateWindowSurface failed"); return false; }

        if (!eglBindAPI(gl.profile == NkGLProfile::ES ? EGL_OPENGL_ES_API : EGL_OPENGL_API)) {
            logger.Error("[NkOpenGL] eglBindAPI failed");
            return false;
        }
        const EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, gl.majorVersion, EGL_NONE };
        EGLContext shareCtx = mSharedParent ? mSharedParent->mData.eglContext : EGL_NO_CONTEXT;
        mData.eglContext = eglCreateContext(mData.eglDisplay, mData.eglConfig, shareCtx, ctxAttribs);
        if (mData.eglContext == EGL_NO_CONTEXT) { logger.Error("[NkOpenGL] eglCreateContext failed"); return false; }

        if (!eglMakeCurrent(mData.eglDisplay, mData.eglSurface, mData.eglSurface, mData.eglContext)) {
            logger.Error("[NkOpenGL] eglMakeCurrent failed");
            return false;
        }
        eglSwapInterval(mData.eglDisplay, gl.swapInterval == NkGLSwapInterval::Immediate ? 0 : 1);
        logger.Info("[NkOpenGL] EGL OK");
        return true;
    }
    void NkOpenGLContext::ShutdownEGL() {
        eglMakeCurrent(mData.eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (mData.eglContext != EGL_NO_CONTEXT) { eglDestroyContext(mData.eglDisplay, mData.eglContext); mData.eglContext = EGL_NO_CONTEXT; }
        if (mData.eglSurface != EGL_NO_SURFACE) { eglDestroySurface(mData.eglDisplay, mData.eglSurface); mData.eglSurface = EGL_NO_SURFACE; }
        eglTerminate(mData.eglDisplay); mData.eglDisplay = EGL_NO_DISPLAY;
    }
    void NkOpenGLContext::SwapEGL() { eglSwapBuffers(mData.eglDisplay, mData.eglSurface); }
    void NkOpenGLContext::SetVSyncEGL(bool on) { eglSwapInterval(mData.eglDisplay, on?1:0); }
    #endif // EGL

    // =============================================================================
    //  macOS NSGL — NkOpenGLContext_macOS.mm
    // =============================================================================
    #if defined(NKENTSEU_PLATFORM_MACOS)
    bool NkOpenGLContext::InitNSGL(const NkSurfaceDesc&, const NkOpenGLDesc&) {
        logger.Error("[NkOpenGL] macOS: compile NkOpenGLContext_macOS.mm instead of this file");
        return false;
    }
    void NkOpenGLContext::ShutdownNSGL() {}
    void NkOpenGLContext::SwapNSGL()    {}
    #endif

    // =============================================================================
    //  iOS EAGL — NkOpenGLContext_iOS.mm
    // =============================================================================
    #if defined(NKENTSEU_PLATFORM_IOS)
    bool NkOpenGLContext::InitEAGL(const NkSurfaceDesc&, const NkOpenGLDesc&) {
        logger.Error("[NkOpenGL] iOS: compile NkOpenGLContext_iOS.mm instead"); return false;
    }
    void NkOpenGLContext::ShutdownEAGL() {}
    void NkOpenGLContext::SwapEAGL()     {}
    #endif

    // =============================================================================
    //  WebGL — Emscripten
    // =============================================================================
    #if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    bool NkOpenGLContext::InitWebGL(const NkSurfaceDesc& surf, const NkOpenGLDesc& gl) {
        EmscriptenWebGLContextAttributes attrs;
        emscripten_webgl_init_context_attributes(&attrs);
        attrs.majorVersion          = gl.majorVersion >= 2 ? 2 : 1;
        attrs.minorVersion          = 0;
        attrs.alpha                 = true;
        attrs.depth                 = gl.depthBits > 0;
        attrs.stencil               = gl.stencilBits > 0;
        attrs.antialias             = gl.msaaSamples > 1;
        attrs.premultipliedAlpha    = false;
        attrs.preserveDrawingBuffer = false;
        attrs.enableExtensionsByDefault = true;
        mData.canvasId = surf.canvasId;
        mData.context  = emscripten_webgl_create_context(surf.canvasId, &attrs);
        if (mData.context <= 0) { logger.Error("[NkOpenGL] WebGL context failed"); return false; }
        emscripten_webgl_make_context_current(mData.context);
        logger.Info("[NkOpenGL] WebGL OK");
        return true;
    }
    void NkOpenGLContext::ShutdownWebGL() {
        if (mData.context) { emscripten_webgl_destroy_context(mData.context); mData.context = 0; }
    }
    void NkOpenGLContext::SwapWebGL() { /* Emscripten gère le swap */ }
    #endif

} // namespace nkentseu
