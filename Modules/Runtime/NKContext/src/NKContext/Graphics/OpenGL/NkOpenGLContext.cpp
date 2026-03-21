// =============================================================================
// NkOpenGLContext.cpp â€” Production Ready
// Loader OpenGL externe possible (NK_NO_GLAD2 recommandé pour NKContext).
// =============================================================================
#include "NkOpenGLContext.h"
#include "NKWindow/Core/NkWindow.h"  // Adaptez selon votre include NkWindow

#include "NKPlatform/NkPlatformDetect.h"
#include "NKLogger/NkLog.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

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

// â”€â”€ Includes GLAD2 optionnels â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
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

#if defined(NKENTSEU_WINDOWING_WAYLAND)
#   if __has_include(<wayland-egl.h>)
#       include <wayland-egl.h>
#       define NKENTSEU_OPENGL_HAS_WAYLAND_EGL 1
#   else
#       define NKENTSEU_OPENGL_HAS_WAYLAND_EGL 0
#   endif
#endif

// Constantes WGL ARB (utilisÃ©es si GLAD2 absent, magic numbers nommÃ©s)
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

#ifndef WGL_CONTEXT_COMPAT_PROFILE_BIT_ARB
#define WGL_CONTEXT_COMPAT_PROFILE_BIT_ARB 0x00000002
#endif

#define NK_GL_LOG(...) logger.Infof("[NkOpenGL] " __VA_ARGS__)
#define NK_GL_ERR(...) logger.Errorf("[NkOpenGL] " __VA_ARGS__)

namespace nkentseu {

#if defined(NKENTSEU_PLATFORM_WINDOWS)
static void* NkWglGetProcAddressCompat(const char* name) {
    void* proc = reinterpret_cast<void*>(wglGetProcAddress(name));
    if (!proc || proc == reinterpret_cast<void*>(1) || proc == reinterpret_cast<void*>(2) ||
        proc == reinterpret_cast<void*>(3) || proc == reinterpret_cast<void*>(-1)) {
        static HMODULE kOpenGL32 = LoadLibraryA("opengl32.dll");
        if (kOpenGL32) {
            proc = reinterpret_cast<void*>(GetProcAddress(kOpenGL32, name));
        }
    }
    return proc;
}
#elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
static void* NkGlxGetProcAddressCompat(const char* name) {
    return reinterpret_cast<void*>(glXGetProcAddressARB(reinterpret_cast<const GLubyte*>(name)));
}
#elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
static void* NkEglGetProcAddressCompat(const char* name) {
    return reinterpret_cast<void*>(eglGetProcAddress(name));
}
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
static void* NkWebglGetProcAddressCompat(const char* name) {
    return reinterpret_cast<void*>(emscripten_webgl_get_proc_address(name));
}
#endif

// â”€â”€ Debug callback GL â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
#ifndef NK_NO_GLAD2
#if defined(NKENTSEU_PLATFORM_WINDOWS) \
 || defined(NKENTSEU_WINDOWING_XLIB) \
 || defined(NKENTSEU_WINDOWING_XCB)
static void GLAPIENTRY GLDebugCallback(
    GLenum src, GLenum type, GLuint id, GLenum severity,
    GLsizei /*len*/, const GLchar* msg, const void* user)
{
    const NkOpenGLDesc* d = static_cast<const NkOpenGLDesc*>(user);
    if (!d) return;
    uint32 sev = (severity==GL_DEBUG_SEVERITY_HIGH)   ? 3 :
                 (severity==GL_DEBUG_SEVERITY_MEDIUM)  ? 2 :
                 (severity==GL_DEBUG_SEVERITY_LOW)     ? 1 : 0;
    if (sev < d->runtime.debugSeverityLevel) return;
    const char* ss = sev==3?"HIGH":sev==2?"MEDIUM":sev==1?"LOW":"NOTIF";
    const char* ts = type==GL_DEBUG_TYPE_ERROR?"ERROR":
                     type==GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR?"UB":
                     type==GL_DEBUG_TYPE_PERFORMANCE?"PERF":"OTHER";
    NK_GL_ERR("[%s][%s] id=%u : %s\n", ss, ts, id, msg);
    (void)src;
}
#endif
#endif // NK_NO_GLAD2

// =============================================================================
NkOpenGLContext::~NkOpenGLContext() { if (mIsValid) Shutdown(); }

bool NkOpenGLContext::Initialize(const NkWindow& window, const NkContextDesc& desc) {
    logger.Warn("[NkOpenGL][DBG] Initialize enter");
    if (mIsValid) { NK_GL_ERR("Already initialized\n"); return false; }
    logger.Warn("[NkOpenGL][DBG] before mDesc copy");
    mDesc = desc;
    logger.Warn("[NkOpenGL][DBG] after mDesc copy");
    const NkSurfaceDesc surf = window.GetSurfaceDesc();
    logger.Warnf("[NkOpenGL][DBG] surface valid=%d %ux%u",
                 surf.IsValid() ? 1 : 0,
                 static_cast<unsigned>(surf.width),
                 static_cast<unsigned>(surf.height));
    if (!surf.IsValid()) { NK_GL_ERR("Invalid NkSurfaceDesc\n"); return false; }

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
    NK_GL_ERR("No OpenGL backend for this platform\n");
#endif
    if (!ok) return false;

#if defined(NKENTSEU_PLATFORM_WINDOWS)
    mData.getProcAddress = &NkWglGetProcAddressCompat;
#elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
    mData.getProcAddress = &NkGlxGetProcAddressCompat;
#elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
    mData.getProcAddress = &NkEglGetProcAddressCompat;
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    mData.getProcAddress = &NkWebglGetProcAddressCompat;
#else
    mData.getProcAddress = nullptr;
#endif

#ifndef NK_NO_GLAD2
    if (desc.opengl.runtime.autoLoadEntryPoints) {
        if (!LoadOpenGLEntryPoints(desc.opengl)) { Shutdown(); return false; }
    } else { FillInfo(); }
#else
    FillInfo();
#endif

    mData.width  = surf.width;
    mData.height = surf.height;
    mIsValid     = true;
    mVSync       = (desc.opengl.swapInterval != NkGLSwapInterval::Immediate);
    NK_GL_LOG("Ready - %s | %s | %s\n", mData.renderer, mData.version, mData.vendor);
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

bool NkOpenGLContext::LoadOpenGLEntryPoints(const NkOpenGLDesc& gl) {
#ifndef NK_NO_GLAD2
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    if (!gladLoadWGL(mData.hdc, (GLADloadfunc)wglGetProcAddress)) {
        NK_GL_ERR("gladLoadWGL failed\n"); return false;
    }
    int ver = gladLoadGL((GLADloadfunc)NkWglGetProcAddressCompat);
    if (!ver) { NK_GL_ERR("gladLoadGL failed\n"); return false; }
    NK_GL_LOG("GLAD2 GL %d.%d\n", GLAD_VERSION_MAJOR(ver), GLAD_VERSION_MINOR(ver));

#elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
    int ver = gladLoadGL((GLADloadfunc)glXGetProcAddressARB);
    if (!ver) { NK_GL_ERR("gladLoadGL(GLX) failed\n"); return false; }

#elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
    if (!gladLoadEGL(mData.eglDisplay, (GLADloadfunc)eglGetProcAddress)) {
        NK_GL_ERR("gladLoadEGL failed\n");
        return false;
    }
    int ver = gladLoadGLES2((GLADloadfunc)eglGetProcAddress);
    if (!ver) { NK_GL_ERR("gladLoadGLES2 failed\n"); return false; }

#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    int ver = gladLoadGLES2((GLADloadfunc)emscripten_webgl_get_proc_address);
    if (!ver) { NK_GL_ERR("gladLoadGLES2(WebGL) failed\n"); return false; }
#endif

    if (gl.runtime.validateVersion) {
        int maj=0, min=0;
        glGetIntegerv(GL_MAJOR_VERSION, &maj);
        glGetIntegerv(GL_MINOR_VERSION, &min);
        if (maj < gl.majorVersion ||
            (maj==gl.majorVersion && min < gl.minorVersion)) {
            NK_GL_ERR("Requested GL %d.%d, got %d.%d\n",
                gl.majorVersion, gl.minorVersion, maj, min);
            return false;
        }
    }

    if (gl.runtime.installDebugCallback &&
        HasFlag(gl.contextFlags, NkGLContextFlags::Debug)) {
#if defined(NKENTSEU_PLATFORM_WINDOWS) \
 || defined(NKENTSEU_WINDOWING_XLIB) \
 || defined(NKENTSEU_WINDOWING_XCB)
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(GLDebugCallback, &mDesc.opengl);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
            GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
        NK_GL_LOG("Debug callback installed\n");
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
    mData.getProcAddress = nullptr;
    NK_GL_LOG("Shutdown OK\n");
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
    if (w == 0 || h == 0) return true; // FenÃªtre minimisÃ©e â€” skip
    mData.width  = w;
    mData.height = h;
#if defined(NKENTSEU_WINDOWING_WAYLAND) && NKENTSEU_OPENGL_HAS_WAYLAND_EGL
    if (mData.eglNativeWindow) {
        wl_egl_window_resize(
            static_cast<wl_egl_window*>(mData.eglNativeWindow),
            static_cast<int>(w),
            static_cast<int>(h),
            0,
            0);
    }
#endif
    // GL suit la surface automatiquement (le viewport est gÃ©rÃ© par l'utilisateur)
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
//  WGL â€” Windows
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
    if (!mData.hdc) { NK_GL_ERR("GetDC failed\n"); return false; }

    // Ã‰tape 1 â€” Bootstrap avec NkWGLFallbackPixelFormat (configurable utilisateur)
    PIXELFORMATDESCRIPTOR pfd = BuildPFD(gl.wglFallback);
    int tmpFmt = ChoosePixelFormat(mData.hdc, &pfd);
    if (!tmpFmt || !SetPixelFormat(mData.hdc, tmpFmt, &pfd)) {
        NK_GL_ERR("Bootstrap SetPixelFormat failed\n"); return false;
    }
    HGLRC tmpCtx = wglCreateContext(mData.hdc);
    if (!tmpCtx) { NK_GL_ERR("Bootstrap wglCreateContext failed\n"); return false; }
    wglMakeCurrent(mData.hdc, tmpCtx);

    // Ã‰tape 2 â€” Charger les extensions WGL ARB
#ifndef NK_NO_GLAD2
    if (!gladLoadWGL(mData.hdc, (GLADloadfunc)wglGetProcAddress)) {
        NK_GL_ERR("gladLoadWGL failed\n");
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
        NK_GL_ERR("WGL ARB extensions not found â€” driver too old?\n");
        wglMakeCurrent(nullptr, nullptr); wglDeleteContext(tmpCtx); return false;
    }
#endif
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(tmpCtx);

    // Ã‰tape 3 â€” Pixel format ARB final (attributs issus de NkOpenGLDesc)
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
        NK_GL_ERR("wglChoosePixelFormatARB failed\n"); return false;
    }
#else
    if (!mData._wglChoosePFARB(mData.hdc, pfAttribs, nullptr, 1, &pixFmt, &numFmt) || numFmt == 0) {
        NK_GL_ERR("wglChoosePixelFormatARB (manual) failed\n"); return false;
    }
#endif

    // SetPixelFormat final (PFD en output â€” obligatoire mÃªme avec ARB)
    PIXELFORMATDESCRIPTOR finalPfd = {};
    DescribePixelFormat(mData.hdc, pixFmt, sizeof(finalPfd), &finalPfd);
    SetPixelFormat(mData.hdc, pixFmt, &finalPfd);

    // Ã‰tape 4 â€” Contexte final avec attributs version/profil
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
    if (!mData.hglrc) { NK_GL_ERR("wglCreateContextAttribsARB failed\n"); return false; }
    wglMakeCurrent(mData.hdc, mData.hglrc);

    // Ã‰tape 5 â€” VSync
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

    NK_GL_LOG("WGL OK (GL %d.%d %s)\n", gl.majorVersion, gl.minorVersion,
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
//  GLX â€” Linux XLib / XCB
// =============================================================================
#if defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)

bool NkOpenGLContext::InitGLX(const NkSurfaceDesc& surf, const NkOpenGLDesc& gl) {
#if defined(NKENTSEU_WINDOWING_XCB)
    // XCB utilise le bridge Xlib pour GLX
    mData.display = XOpenDisplay(nullptr);
    if (!mData.display) { NK_GL_ERR("XOpenDisplay failed\n"); return false; }
    mData.window = (::Window)surf.window;
#else
    mData.display = static_cast<Display*>(surf.display);
    mData.window  = (::Window)surf.window;
#endif
    if (!mData.display || !mData.window) {
        NK_GL_ERR("Invalid X11 handles for GLX\n");
        return false;
    }
    int screen = DefaultScreen(mData.display);

    const char* wslInterop = std::getenv("WSL_INTEROP");
    const char* wslDistro  = std::getenv("WSL_DISTRO_NAME");
    const bool runningInWsl = (wslInterop && *wslInterop) || (wslDistro && *wslDistro);

#ifndef NK_NO_GLAD2
    if (!runningInWsl) {
        if (!gladLoaderLoadGLX(mData.display, screen)) {
            NK_GL_LOG("gladLoaderLoadGLX failed, continuing with GL loader only\n");
        }
    } else {
        NK_GL_LOG("WSL detected: skip GLX pre-load\n");
    }
#endif

    int glxMajor = 0;
    int glxMinor = 0;
    if (!glXQueryVersion(mData.display, &glxMajor, &glxMinor) ||
        (glxMajor == 1 && glxMinor < 3)) {
        NK_GL_ERR("GLX 1.3+ required\n");
        return false;
    }

    // FBConfig â€” rÃ©utiliser celui injectÃ© par PrepareWindowConfig ou en choisir un
    GLXFBConfig fbConfig = nullptr;
    uintptr fbPtr = surf.appliedHints.Get(NkSurfaceHintKey::NK_GLX_FB_CONFIG_PTR, 0);
    if (fbPtr) {
        fbConfig = reinterpret_cast<GLXFBConfig>(fbPtr);
        NK_GL_LOG("GLX: reuse FBConfig from PrepareWindowConfig\n");
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
        if (!cfgs || count == 0) { NK_GL_ERR("glXChooseFBConfig failed\n"); return false; }
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

    using FnType = GLXContext(*)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
    auto fn = (FnType)glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");

    if (fn && !runningInWsl) {
        mData.context = fn(mData.display, fbConfig, shareCtx, True, ctxAttribs);
    } else {
        NK_GL_LOG("Using legacy GLX context path%s\n", runningInWsl ? " (WSL)" : "");
        mData.context = glXCreateNewContext(mData.display, fbConfig, GLX_RGBA_TYPE, shareCtx, True);
    }

    if (!mData.context) { NK_GL_ERR("GLX context creation failed\n"); return false; }
    if (!glXMakeCurrent(mData.display, mData.window, mData.context)) {
        NK_GL_ERR("glXMakeCurrent failed\n");
        glXDestroyContext(mData.display, mData.context);
        mData.context = nullptr;
        return false;
    }
    SetVSyncGLX(gl.swapInterval != NkGLSwapInterval::Immediate);

    NK_GL_LOG("GLX OK (GL %d.%d)\n", gl.majorVersion, gl.minorVersion);
    return true;
}

void NkOpenGLContext::ShutdownGLX() {
    glXMakeCurrent(mData.display, None, nullptr);
    if (mData.context) { glXDestroyContext(mData.display, mData.context); mData.context = nullptr; }
#if defined(NKENTSEU_WINDOWING_XCB)
    if (mData.display) { XCloseDisplay(mData.display); mData.display = nullptr; }
#endif
}
void NkOpenGLContext::SwapGLX() { glXSwapBuffers(mData.display, mData.window); }
void NkOpenGLContext::SetVSyncGLX(bool on) {
    using FnEXT = void(*)(Display*, GLXDrawable, int);
    using FnMESA = int(*)(unsigned int);
    using FnSGI = int(*)(int);

    auto fnExt = (FnEXT)glXGetProcAddressARB((const GLubyte*)"glXSwapIntervalEXT");
    if (fnExt) { fnExt(mData.display, mData.window, on ? 1 : 0); return; }

    auto fnMesa = (FnMESA)glXGetProcAddressARB((const GLubyte*)"glXSwapIntervalMESA");
    if (fnMesa) { fnMesa(on ? 1u : 0u); return; }

    auto fnSgi = (FnSGI)glXGetProcAddressARB((const GLubyte*)"glXSwapIntervalSGI");
    if (fnSgi) { fnSgi(on ? 1 : 0); }
}
#endif // XLIB/XCB

// =============================================================================
//  EGL â€” Wayland / Android
// =============================================================================
#if defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)

bool NkOpenGLContext::InitEGL(const NkSurfaceDesc& surf, const NkOpenGLDesc& gl) {
    logger.Warn("[NkOpenGL][DBG] InitEGL begin");
#if defined(NKENTSEU_WINDOWING_WAYLAND)
    logger.Warnf("[NkOpenGL][DBG] InitEGL wayland display=%p surface=%p",
                 surf.display, surf.surface);
    mData.eglDisplay = eglGetDisplay((EGLNativeDisplayType)surf.display);
#else
    logger.Warnf("[NkOpenGL][DBG] InitEGL android nativeWindow=%p", surf.nativeWindow);
    mData.eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
#endif
    logger.Warnf("[NkOpenGL][DBG] InitEGL eglDisplay=%p", mData.eglDisplay);
    if (mData.eglDisplay == EGL_NO_DISPLAY) { NK_GL_ERR("eglGetDisplay failed\n"); return false; }

    EGLint maj, min;
    logger.Warn("[NkOpenGL][DBG] InitEGL before eglInitialize");
    if (!eglInitialize(mData.eglDisplay, &maj, &min)) { NK_GL_ERR("eglInitialize failed\n"); return false; }
    logger.Warnf("[NkOpenGL][DBG] InitEGL eglInitialize OK %d.%d", (int)maj, (int)min);

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
    logger.Warn("[NkOpenGL][DBG] InitEGL before eglChooseConfig");
    eglChooseConfig(mData.eglDisplay, cfgAttribs, &mData.eglConfig, 1, &numCfg);
    logger.Warnf("[NkOpenGL][DBG] InitEGL eglChooseConfig numCfg=%d cfg=%p", (int)numCfg, mData.eglConfig);
    if (numCfg == 0) { NK_GL_ERR("eglChooseConfig failed\n"); return false; }

#if defined(NKENTSEU_WINDOWING_WAYLAND)
#if NKENTSEU_OPENGL_HAS_WAYLAND_EGL
    const int width = static_cast<int>(surf.width > 0 ? surf.width : 1u);
    const int height = static_cast<int>(surf.height > 0 ? surf.height : 1u);
    if (!surf.surface) {
        NK_GL_ERR("Wayland surface is null\n");
        return false;
    }
    if (mData.eglNativeWindow) {
        wl_egl_window_destroy(static_cast<wl_egl_window*>(mData.eglNativeWindow));
        mData.eglNativeWindow = nullptr;
    }
    wl_egl_window* wlEglWindow = wl_egl_window_create(surf.surface, width, height);
    logger.Warnf("[NkOpenGL][DBG] InitEGL wl_egl_window=%p size=%dx%d",
                 static_cast<void*>(wlEglWindow), width, height);
    if (!wlEglWindow) {
        NK_GL_ERR("wl_egl_window_create failed\n");
        return false;
    }
    mData.eglNativeWindow = wlEglWindow;
    EGLNativeWindowType nwin = reinterpret_cast<EGLNativeWindowType>(wlEglWindow);
#else
    NK_GL_ERR("Wayland EGL support requires <wayland-egl.h>\n");
    return false;
#endif
#else
    mData.eglNativeWindow = surf.nativeWindow;
    logger.Warnf("[NkOpenGL][DBG] InitEGL nativeWindow=%p", mData.eglNativeWindow);
    EGLNativeWindowType nwin = reinterpret_cast<EGLNativeWindowType>(surf.nativeWindow);
#endif
    logger.Warn("[NkOpenGL][DBG] InitEGL before eglCreateWindowSurface");
    mData.eglSurface = eglCreateWindowSurface(mData.eglDisplay, mData.eglConfig, nwin, nullptr);
    logger.Warnf("[NkOpenGL][DBG] InitEGL eglSurface=%p", mData.eglSurface);
    if (mData.eglSurface == EGL_NO_SURFACE) {
#if defined(NKENTSEU_WINDOWING_WAYLAND) && NKENTSEU_OPENGL_HAS_WAYLAND_EGL
        if (mData.eglNativeWindow) {
            wl_egl_window_destroy(static_cast<wl_egl_window*>(mData.eglNativeWindow));
            mData.eglNativeWindow = nullptr;
        }
#endif
        NK_GL_ERR("eglCreateWindowSurface failed\n");
        return false;
    }

    logger.Warn("[NkOpenGL][DBG] InitEGL before eglBindAPI");
    eglBindAPI(gl.profile == NkGLProfile::ES ? EGL_OPENGL_ES_API : EGL_OPENGL_API);
    const EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, gl.majorVersion, EGL_NONE };
    EGLContext shareCtx = mSharedParent ? mSharedParent->mData.eglContext : EGL_NO_CONTEXT;
    logger.Warnf("[NkOpenGL][DBG] InitEGL before eglCreateContext share=%p", shareCtx);
    mData.eglContext = eglCreateContext(mData.eglDisplay, mData.eglConfig, shareCtx, ctxAttribs);
    logger.Warnf("[NkOpenGL][DBG] InitEGL eglContext=%p", mData.eglContext);
    if (mData.eglContext == EGL_NO_CONTEXT) {
        if (mData.eglSurface != EGL_NO_SURFACE) {
            eglDestroySurface(mData.eglDisplay, mData.eglSurface);
            mData.eglSurface = EGL_NO_SURFACE;
        }
#if defined(NKENTSEU_WINDOWING_WAYLAND) && NKENTSEU_OPENGL_HAS_WAYLAND_EGL
        if (mData.eglNativeWindow) {
            wl_egl_window_destroy(static_cast<wl_egl_window*>(mData.eglNativeWindow));
            mData.eglNativeWindow = nullptr;
        }
#endif
        NK_GL_ERR("eglCreateContext failed\n");
        return false;
    }

    logger.Warn("[NkOpenGL][DBG] InitEGL before eglMakeCurrent");
    eglMakeCurrent(mData.eglDisplay, mData.eglSurface, mData.eglSurface, mData.eglContext);
    eglSwapInterval(mData.eglDisplay, gl.swapInterval == NkGLSwapInterval::Immediate ? 0 : 1);
    NK_GL_LOG("EGL OK\n");
    return true;
}
void NkOpenGLContext::ShutdownEGL() {
    eglMakeCurrent(mData.eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (mData.eglContext != EGL_NO_CONTEXT) { eglDestroyContext(mData.eglDisplay, mData.eglContext); mData.eglContext = EGL_NO_CONTEXT; }
    if (mData.eglSurface != EGL_NO_SURFACE) { eglDestroySurface(mData.eglDisplay, mData.eglSurface); mData.eglSurface = EGL_NO_SURFACE; }
    eglTerminate(mData.eglDisplay); mData.eglDisplay = EGL_NO_DISPLAY;
#if defined(NKENTSEU_WINDOWING_WAYLAND) && NKENTSEU_OPENGL_HAS_WAYLAND_EGL
    if (mData.eglNativeWindow) {
        wl_egl_window_destroy(static_cast<wl_egl_window*>(mData.eglNativeWindow));
        mData.eglNativeWindow = nullptr;
    }
#else
    mData.eglNativeWindow = nullptr;
#endif
}
void NkOpenGLContext::SwapEGL() { eglSwapBuffers(mData.eglDisplay, mData.eglSurface); }
void NkOpenGLContext::SetVSyncEGL(bool on) { eglSwapInterval(mData.eglDisplay, on?1:0); }
#endif // EGL

// =============================================================================
//  macOS NSGL â€” NkOpenGLContext_macOS.mm
// =============================================================================
#if defined(NKENTSEU_PLATFORM_MACOS)
bool NkOpenGLContext::InitNSGL(const NkSurfaceDesc&, const NkOpenGLDesc&) {
    NK_GL_ERR("macOS: compile NkOpenGLContext_macOS.mm instead of this file\n");
    return false;
}
void NkOpenGLContext::ShutdownNSGL() {}
void NkOpenGLContext::SwapNSGL()    {}
#endif

// =============================================================================
//  iOS EAGL â€” NkOpenGLContext_iOS.mm
// =============================================================================
#if defined(NKENTSEU_PLATFORM_IOS)
bool NkOpenGLContext::InitEAGL(const NkSurfaceDesc&, const NkOpenGLDesc&) {
    NK_GL_ERR("iOS: compile NkOpenGLContext_iOS.mm instead\n"); return false;
}
void NkOpenGLContext::ShutdownEAGL() {}
void NkOpenGLContext::SwapEAGL()     {}
#endif

// =============================================================================
//  WebGL â€” Emscripten
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
    if (mData.context <= 0) { NK_GL_ERR("WebGL context failed\n"); return false; }
    emscripten_webgl_make_context_current(mData.context);
    mData.getProcAddress = &NkWebglGetProcAddressCompat;
    NK_GL_LOG("WebGL OK\n");
    return true;
}
void NkOpenGLContext::ShutdownWebGL() {
    if (mData.context) { emscripten_webgl_destroy_context(mData.context); mData.context = 0; }
}
void NkOpenGLContext::SwapWebGL() { /* Emscripten gÃ¨re le swap */ }
#endif

} // namespace nkentseu

