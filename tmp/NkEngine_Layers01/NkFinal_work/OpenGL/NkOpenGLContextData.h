#pragma once
// =============================================================================
// NkOpenGLContextData.h — Données internes contexte OpenGL (par plateforme)
// =============================================================================
#include "../Core/NkTypes.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#   define NOMINMAX
#   endif
#   include <windows.h>
    // Pointeurs de fonctions WGL ARB (utilisés quand NK_NO_GLAD2 est défini)
    using PFNWGLCREATECONTEXTATTRIBSARBPROC = HGLRC(WINAPI*)(HDC,HGLRC,const int*);
    using PFNWGLCHOOSEPIXELFORMATARBPROC    = BOOL (WINAPI*)(HDC,const int*,const FLOAT*,UINT,int*,UINT*);
    using PFNWGLSWAPINTERVALEXTPROC         = BOOL (WINAPI*)(int);

#elif defined(NKENTSEU_WINDOWING_XLIB)
#   include <X11/Xlib.h>
#   include <GL/glx.h>

#elif defined(NKENTSEU_WINDOWING_XCB)
#   include <xcb/xcb.h>
#   include <X11/Xlib.h>
#   include <X11/Xlib-xcb.h>
#   include <GL/glx.h>

#elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
#   include <EGL/egl.h>
#   include <EGL/eglext.h>

#elif defined(NKENTSEU_PLATFORM_MACOS)
#   ifdef __OBJC__
#       import <AppKit/AppKit.h>
#   else
        struct NSOpenGLContext; struct NSOpenGLPixelFormat; struct NSView;
#   endif

#elif defined(NKENTSEU_PLATFORM_IOS)
#   ifdef __OBJC__
#       import <UIKit/UIKit.h>
#       import <OpenGLES/ES3/gl.h>
#   else
        struct EAGLContext; struct UIView;
#   endif

#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#   include <emscripten/html5.h>
#endif

namespace nkentseu {

    struct NkOpenGLContextData {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
        HDC    hdc   = nullptr;
        HGLRC  hglrc = nullptr;
        HWND   hwnd  = nullptr;
        // Stockage des fn ARB pour mode sans GLAD2
        PFNWGLCREATECONTEXTATTRIBSARBPROC _wglCreateCtxARB   = nullptr;
        PFNWGLCHOOSEPIXELFORMATARBPROC    _wglChoosePFARB    = nullptr;
        PFNWGLSWAPINTERVALEXTPROC         _wglSwapIntervalEXT= nullptr;

#elif defined(NKENTSEU_WINDOWING_XLIB)
        Display*    display  = nullptr;
        ::Window    window   = 0;
        GLXContext  context  = nullptr;
        GLXFBConfig fbConfig = nullptr;

#elif defined(NKENTSEU_WINDOWING_XCB)
        Display*    display  = nullptr;   // bridge Xlib-xcb
        ::Window    window   = 0;
        GLXContext  context  = nullptr;
        GLXFBConfig fbConfig = nullptr;

#elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
        EGLDisplay  eglDisplay = EGL_NO_DISPLAY;
        EGLSurface  eglSurface = EGL_NO_SURFACE;
        EGLContext  eglContext = EGL_NO_CONTEXT;
        EGLConfig   eglConfig  = nullptr;

#elif defined(NKENTSEU_PLATFORM_MACOS)
#   ifdef __OBJC__
        NSOpenGLContext*     context     = nil;
        NSOpenGLPixelFormat* pixelFormat = nil;
        NSView*              view        = nil;
#   else
        void* context = nullptr; void* pixelFormat = nullptr; void* view = nullptr;
#   endif

#elif defined(NKENTSEU_PLATFORM_IOS)
#   ifdef __OBJC__
        EAGLContext* context  = nil;
        UIView*      view     = nil;
        GLuint       framebuffer = 0;
        GLuint       colorRenderbuffer = 0;
        GLuint       depthRenderbuffer = 0;
#   else
        void* context = nullptr; void* view = nullptr;
        unsigned int framebuffer = 0;
        unsigned int colorRenderbuffer = 0;
        unsigned int depthRenderbuffer = 0;
#   endif

#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context = 0;
        const char* canvasId = "#canvas";

#else
        void* dummy = nullptr;
#endif
        uint32      width    = 0;
        uint32      height   = 0;
        const char* renderer = "Unknown";
        const char* vendor   = "Unknown";
        const char* version  = "Unknown";
    };

} // namespace nkentseu
