#include "NkContext.h"
#include "NkWindow.h"
#include "NKPlatform/NkPlatformDetect.h"

// =============================================================================
// Includes natifs par plateforme
// =============================================================================

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   include <windows.h>
#endif

#if defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#   include <X11/Xlib.h>
#   include <GL/glx.h>
#   if defined(None)
#       undef None
#   endif
#endif

#if defined(NKENTSEU_WINDOWING_WAYLAND)
#   if __has_include(<EGL/egl.h>) && __has_include(<wayland-egl.h>)
#       define NKENTSEU_HAS_WAYLAND_EGL 1
#       include <EGL/egl.h>
#       include <wayland-egl.h>
#   else
#       define NKENTSEU_HAS_WAYLAND_EGL 0
#   endif
#endif

#if defined(NKENTSEU_PLATFORM_ANDROID)
#   include <android/native_window.h>
#   include <EGL/egl.h>
#   define NKENTSEU_HAS_ANDROID_EGL 1
#endif

#if defined(NKENTSEU_PLATFORM_HARMONYOS)
#   if __has_include(<EGL/egl.h>)
#       include <EGL/egl.h>
#       define NKENTSEU_HAS_HARMONY_EGL 1
#   else
#       define NKENTSEU_HAS_HARMONY_EGL 0
#   endif
#   if __has_include(<native_window/external_window.h>)
#       include <native_window/external_window.h>
#   else
        struct OHNativeWindow; // forward declaration minimale
#   endif
#endif

#if defined(NKENTSEU_PLATFORM_MACOS)
#   ifdef __OBJC__
#       import <AppKit/AppKit.h>      // NSOpenGLContext, NSOpenGLPixelFormat
#       import <OpenGL/OpenGL.h>      // CGL
#   else
        // Forward declarations pour compilation C++ pure
        struct NSOpenGLContext;
        struct NSOpenGLPixelFormat;
        struct CGLContextObj;
#   endif
#endif

#if defined(NKENTSEU_PLATFORM_IOS)   || defined(NKENTSEU_PLATFORM_TVOS)  || \
    defined(NKENTSEU_PLATFORM_WATCHOS)|| defined(NKENTSEU_PLATFORM_VISIONOS)
#   ifdef __OBJC__
#       import <OpenGLES/EAGL.h>      // EAGLContext, kEAGLRenderingAPIOpenGLES3
#       import <OpenGLES/ES3/gl.h>
#   else
        struct EAGLContext;
#   endif
#endif

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#   include <emscripten.h>
#   include <emscripten/html5.h>
#   include <GLES3/gl3.h>
#   include <EGL/egl.h>
#endif

#include <cstring>

namespace nkentseu {

    // =========================================================================
    // Helpers internes
    // =========================================================================

    namespace {

        static NkContextConfig gHints{};
        static bool            gInit = false;

        static uint32 NkClampU32Min(uint32 value, uint32 minValue) {
            return value < minValue ? minValue : value;
        }

        static void NkSetContextError(NkContext& ctx, uint32 code, const char* msg) {
            ctx.lastError.code    = code;
            ctx.lastError.message = msg;
        }

        static void NkClearContextError(NkContext& ctx) {
            ctx.lastError = NkError::Ok();
        }

        static bool NkIsSurfaceOnlyApi(graphics::NkGraphicsApi api) {
            return api == graphics::NkGraphicsApi::NK_GFX_API_VULKAN  ||
                   api == graphics::NkGraphicsApi::NK_GFX_API_DX11    ||
                   api == graphics::NkGraphicsApi::NK_GFX_API_DX12    ||
                   api == graphics::NkGraphicsApi::NK_GFX_API_METAL   ||
                   api == graphics::NkGraphicsApi::NK_GFX_API_SOFTWARE;
        }

        static bool NkShouldRequestExplicitGL(const NkContextConfig& config) {
            return config.versionMajor >= 3u ||
                   config.versionMinor >  0u  ||
                   config.profile != NkContextProfile::NK_CONTEXT_PROFILE_ANY ||
                   config.debug;
        }

        // ── Helpers EGL communs (Android, HarmonyOS, Wayland) ─────────────────
        // Factorisés pour éviter la duplication entre les 3 backends EGL.

    #if defined(NKENTSEU_PLATFORM_ANDROID)      || \
        defined(NKENTSEU_PLATFORM_HARMONYOS)    || \
        (defined(NKENTSEU_WINDOWING_WAYLAND) && NKENTSEU_HAS_WAYLAND_EGL) || \
        defined(NKENTSEU_PLATFORM_EMSCRIPTEN)

        static void* NkEglProc(const char* name) {
            if (!name) return nullptr;
            return reinterpret_cast<void*>(eglGetProcAddress(name));
        }

        static bool NkEglChooseConfig(EGLDisplay display,
                                      const NkContextConfig& config,
                                      EGLConfig* outConfig,
                                      bool esProfile) {
            if (!outConfig || display == EGL_NO_DISPLAY) return false;
            EGLint renderableType = EGL_OPENGL_ES2_BIT;
            if (esProfile && config.versionMajor >= 3) {
                #ifdef EGL_OPENGL_ES3_BIT
                renderableType = EGL_OPENGL_ES3_BIT;
                #endif
            } else if (!esProfile) {
                renderableType = EGL_OPENGL_BIT;
            }
            const EGLint attrs[] = {
                EGL_SURFACE_TYPE,   EGL_WINDOW_BIT,
                EGL_RENDERABLE_TYPE,renderableType,
                EGL_RED_SIZE,       static_cast<EGLint>(config.redBits),
                EGL_GREEN_SIZE,     static_cast<EGLint>(config.greenBits),
                EGL_BLUE_SIZE,      static_cast<EGLint>(config.blueBits),
                EGL_ALPHA_SIZE,     static_cast<EGLint>(config.alphaBits),
                EGL_DEPTH_SIZE,     static_cast<EGLint>(config.depthBits),
                EGL_STENCIL_SIZE,   static_cast<EGLint>(config.stencilBits),
                EGL_SAMPLES,        (config.msaaSamples > 1u) ?
                                    static_cast<EGLint>(config.msaaSamples) : 0,
                EGL_NONE
            };
            EGLint count = 0;
            return eglChooseConfig(display, attrs, outConfig, 1, &count) == EGL_TRUE
                && count >= 1;
        }

        static EGLContext NkEglCreateContext(EGLDisplay display,
                                             EGLConfig config,
                                             const NkContextConfig& cfg) {
            const EGLint ctxAttrs[] = {
                EGL_CONTEXT_CLIENT_VERSION,
                static_cast<EGLint>(NkClampU32Min(cfg.versionMajor, 2u)),
                EGL_NONE
            };
            return eglCreateContext(display, config, EGL_NO_CONTEXT, ctxAttrs);
        }
    #endif

        // ── Windows WGL helpers ───────────────────────────────────────────────
    #if defined(NKENTSEU_PLATFORM_WINDOWS)

        static constexpr int kWglContextMajorVersion       = 0x2091;
        static constexpr int kWglContextMinorVersion       = 0x2092;
        static constexpr int kWglContextFlags              = 0x2094;
        static constexpr int kWglContextProfileMask        = 0x9126;
        static constexpr int kWglContextDebugBit           = 0x0001;
        static constexpr int kWglContextCoreProfileBit     = 0x00000001;
        static constexpr int kWglContextCompatibilityBit   = 0x00000002;
        static constexpr int kWglContextEs2ProfileBit      = 0x00000004;

        using NkWglCreateContextAttribsProc = HGLRC(WINAPI*)(HDC, HGLRC, const int*);

        static BYTE NkToByte(uint32 v) {
            return static_cast<BYTE>(v > 255u ? 255u : v);
        }

        static BYTE NkToLayerByte(int32 layerType) {
            if (layerType > 0) return static_cast<BYTE>(PFD_OVERLAY_PLANE);
            if (layerType < 0) return static_cast<BYTE>(PFD_UNDERLAY_PLANE);
            return static_cast<BYTE>(PFD_MAIN_PLANE);
        }

        static void NkBuildWglContextAttribs(const NkContextConfig& config, int (&attrs)[16]) {
            int i = 0;
            attrs[i++] = kWglContextMajorVersion;
            attrs[i++] = static_cast<int>(NkClampU32Min(config.versionMajor, 1u));
            attrs[i++] = kWglContextMinorVersion;
            attrs[i++] = static_cast<int>(config.versionMinor);
            int flags = 0;
            if (config.debug) flags |= kWglContextDebugBit;
            if (flags) { attrs[i++] = kWglContextFlags; attrs[i++] = flags; }
            int profileMask = 0;
            switch (config.profile) {
                case NkContextProfile::NK_CONTEXT_PROFILE_CORE:
                    profileMask = kWglContextCoreProfileBit; break;
                case NkContextProfile::NK_CONTEXT_PROFILE_COMPATIBILITY:
                    profileMask = kWglContextCompatibilityBit; break;
                case NkContextProfile::NK_CONTEXT_PROFILE_ES:
                    profileMask = kWglContextEs2ProfileBit; break;
                default: break;
            }
            if (profileMask) { attrs[i++] = kWglContextProfileMask; attrs[i++] = profileMask; }
            attrs[i++] = 0;
        }

        static PIXELFORMATDESCRIPTOR NkBuildPixelFormatDescriptor(const NkContextConfig& config) {
            PIXELFORMATDESCRIPTOR pfd = {};
            pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
            pfd.nVersion = 1;
            const NkWin32PixelFormatConfig& custom = config.win32PixelFormat;
            if (custom.useCustomDescriptor) {
                DWORD flags = 0;
                if (custom.drawToWindow)       flags |= PFD_DRAW_TO_WINDOW;
                if (custom.supportOpenGL)      flags |= PFD_SUPPORT_OPENGL;
                if (custom.doubleBuffer)       flags |= PFD_DOUBLEBUFFER;
                if (custom.stereo)             flags |= PFD_STEREO;
                if (custom.supportGDI)         flags |= PFD_SUPPORT_GDI;
                if (custom.genericFormat)      flags |= PFD_GENERIC_FORMAT;
                if (custom.genericAccelerated) flags |= PFD_GENERIC_ACCELERATED;
                pfd.dwFlags         = flags;
                pfd.iPixelType      = static_cast<BYTE>(custom.pixelType == 1u ? PFD_TYPE_COLORINDEX : PFD_TYPE_RGBA);
                pfd.cColorBits      = NkToByte(custom.colorBits);
                pfd.cRedBits        = NkToByte(custom.redBits);
                pfd.cRedShift       = NkToByte(custom.redShift);
                pfd.cGreenBits      = NkToByte(custom.greenBits);
                pfd.cGreenShift     = NkToByte(custom.greenShift);
                pfd.cBlueBits       = NkToByte(custom.blueBits);
                pfd.cBlueShift      = NkToByte(custom.blueShift);
                pfd.cAlphaBits      = NkToByte(custom.alphaBits);
                pfd.cAlphaShift     = NkToByte(custom.alphaShift);
                pfd.cAccumBits      = NkToByte(custom.accumBits);
                pfd.cAccumRedBits   = NkToByte(custom.accumRedBits);
                pfd.cAccumGreenBits = NkToByte(custom.accumGreenBits);
                pfd.cAccumBlueBits  = NkToByte(custom.accumBlueBits);
                pfd.cAccumAlphaBits = NkToByte(custom.accumAlphaBits);
                pfd.cDepthBits      = NkToByte(custom.depthBits);
                pfd.cStencilBits    = NkToByte(custom.stencilBits);
                pfd.cAuxBuffers     = NkToByte(custom.auxBuffers);
                pfd.iLayerType      = NkToLayerByte(custom.layerType);
                pfd.dwLayerMask     = custom.layerMask;
                pfd.dwVisibleMask   = custom.visibleMask;
                pfd.dwDamageMask    = custom.damageMask;
                return pfd;
            }
            DWORD flags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
            if (config.doubleBuffer) flags |= PFD_DOUBLEBUFFER;
            if (config.stereo)       flags |= PFD_STEREO;
            pfd.dwFlags     = flags;
            pfd.iPixelType  = PFD_TYPE_RGBA;
            const uint32 totalColor = NkClampU32Min(
                config.redBits + config.greenBits + config.blueBits + config.alphaBits, 24u);
            pfd.cColorBits    = NkToByte(totalColor);
            pfd.cRedBits      = NkToByte(config.redBits);
            pfd.cGreenBits    = NkToByte(config.greenBits);
            pfd.cBlueBits     = NkToByte(config.blueBits);
            pfd.cAlphaBits    = NkToByte(config.alphaBits);
            pfd.cAccumRedBits = NkToByte(config.accumRedBits);
            pfd.cAccumGreenBits = NkToByte(config.accumGreenBits);
            pfd.cAccumBlueBits  = NkToByte(config.accumBlueBits);
            pfd.cAccumAlphaBits = NkToByte(config.accumAlphaBits);
            pfd.cDepthBits      = NkToByte(config.depthBits);
            pfd.cStencilBits    = NkToByte(config.stencilBits);
            pfd.cAuxBuffers     = NkToByte(config.auxBuffers);
            pfd.iLayerType      = PFD_MAIN_PLANE;
            return pfd;
        }

        static void* NkWglProc(const char* name) {
            if (!name) return nullptr;
            void* p = reinterpret_cast<void*>(wglGetProcAddress(name));
            if (p) return p;
            static HMODULE sOgl = GetModuleHandleA("opengl32.dll");
            if (!sOgl) sOgl = LoadLibraryA("opengl32.dll");
            return sOgl ? reinterpret_cast<void*>(GetProcAddress(sOgl, name)) : nullptr;
        }
    #endif // WINDOWS

        // ── GLX helpers (XLib + XCB) ──────────────────────────────────────────
    #if defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)

        static constexpr int kGlxContextMajorVersion       = 0x2091;
        static constexpr int kGlxContextMinorVersion       = 0x2092;
        static constexpr int kGlxContextFlags              = 0x2094;
        static constexpr int kGlxContextProfileMask        = 0x9126;
        static constexpr int kGlxContextDebugBit           = 0x00000001;
        static constexpr int kGlxContextCoreProfileBit     = 0x00000001;
        static constexpr int kGlxContextCompatibilityBit   = 0x00000002;
        static constexpr int kGlxContextEs2ProfileBit      = 0x00000004;

        using NkGlxCreateContextAttribsProc = GLXContext(*)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

        static void* NkGlxProc(const char* name) {
            if (!name) return nullptr;
            return reinterpret_cast<void*>(
                glXGetProcAddressARB(reinterpret_cast<const GLubyte*>(name)));
        }

        static bool HasGLXExtension(const char* list, const char* name) {
            if (!list || !name || !*name) return false;
            const size_t len = ::strlen(name);
            const char* s = list;
            while (true) {
                const char* p = ::strstr(s, name);
                if (!p) return false;
                const char* e = p + len;
                if ((p == list || p[-1] == ' ') && (*e == '\0' || *e == ' ')) return true;
                s = e;
            }
        }

        static void BuildGlxContextAttribs(const NkContextConfig& config, int (&attrs)[16]) {
            int i = 0;
            attrs[i++] = kGlxContextMajorVersion;
            attrs[i++] = static_cast<int>(NkClampU32Min(config.versionMajor, 1u));
            attrs[i++] = kGlxContextMinorVersion;
            attrs[i++] = static_cast<int>(config.versionMinor);
            int flags = 0;
            if (config.debug) flags |= kGlxContextDebugBit;
            if (flags) { attrs[i++] = kGlxContextFlags; attrs[i++] = flags; }
            int profileMask = 0;
            switch (config.profile) {
                case NkContextProfile::NK_CONTEXT_PROFILE_CORE:
                    profileMask = kGlxContextCoreProfileBit; break;
                case NkContextProfile::NK_CONTEXT_PROFILE_COMPATIBILITY:
                    profileMask = kGlxContextCompatibilityBit; break;
                case NkContextProfile::NK_CONTEXT_PROFILE_ES:
                    profileMask = kGlxContextEs2ProfileBit; break;
                default: break;
            }
            if (profileMask) { attrs[i++] = kGlxContextProfileMask; attrs[i++] = profileMask; }
            attrs[i++] = 0;
        }

        static GLXFBConfig ChooseGlxFBConfig(Display* dpy, int screen,
                                              const NkContextConfig& config,
                                              VisualID preferredVisual) {
            const int hasMsaa   = (config.msaaSamples > 1u) ? 1 : 0;
            const int msaaSamps = (config.msaaSamples > 1u) ? static_cast<int>(config.msaaSamples) : 0;
            const int attrs[] = {
                GLX_X_RENDERABLE,   True,
                GLX_DRAWABLE_TYPE,  GLX_WINDOW_BIT,
                GLX_RENDER_TYPE,    GLX_RGBA_BIT,
                GLX_X_VISUAL_TYPE,  GLX_TRUE_COLOR,
                GLX_RED_SIZE,   static_cast<int>(NkClampU32Min(config.redBits, 1u)),
                GLX_GREEN_SIZE, static_cast<int>(NkClampU32Min(config.greenBits, 1u)),
                GLX_BLUE_SIZE,  static_cast<int>(NkClampU32Min(config.blueBits, 1u)),
                GLX_ALPHA_SIZE, static_cast<int>(config.alphaBits),
                GLX_DEPTH_SIZE, static_cast<int>(config.depthBits),
                GLX_STENCIL_SIZE, static_cast<int>(config.stencilBits),
                GLX_DOUBLEBUFFER,   config.doubleBuffer ? True : False,
                GLX_STEREO,         config.stereo       ? True : False,
                GLX_ACCUM_RED_SIZE,   static_cast<int>(config.accumRedBits),
                GLX_ACCUM_GREEN_SIZE, static_cast<int>(config.accumGreenBits),
                GLX_ACCUM_BLUE_SIZE,  static_cast<int>(config.accumBlueBits),
                GLX_ACCUM_ALPHA_SIZE, static_cast<int>(config.accumAlphaBits),
                GLX_SAMPLE_BUFFERS, hasMsaa,
                GLX_SAMPLES,        msaaSamps,
                0
            };
            int n = 0;
            GLXFBConfig* fbs = glXChooseFBConfig(dpy, screen, attrs, &n);
            if (!fbs || n <= 0) return nullptr;
            int sel = 0;
            if (preferredVisual != 0) {
                for (int i = 0; i < n; ++i) {
                    XVisualInfo* vi = glXGetVisualFromFBConfig(dpy, fbs[i]);
                    if (!vi) continue;
                    bool match = (vi->visualid == preferredVisual);
                    XFree(vi);
                    if (match) { sel = i; break; }
                }
            }
            GLXFBConfig chosen = fbs[sel];
            XFree(fbs);
            return chosen;
        }
    #endif // XLIB/XCB

        // ── ApplyGlxVisualHint ────────────────────────────────────────────────
        static void ApplyGlxVisualHint(const NkContextConfig& hints, NkWindowConfig& cfg) {
        #if defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
            Display* dpy = XOpenDisplay(nullptr);
            if (!dpy) return;
            const int screen = DefaultScreen(dpy);
            const int attrs[] = {
                GLX_X_RENDERABLE,   True,
                GLX_DRAWABLE_TYPE,  GLX_WINDOW_BIT,
                GLX_RENDER_TYPE,    GLX_RGBA_BIT,
                GLX_X_VISUAL_TYPE,  GLX_TRUE_COLOR,
                GLX_RED_SIZE,   static_cast<int>(NkClampU32Min(hints.redBits, 1u)),
                GLX_GREEN_SIZE, static_cast<int>(NkClampU32Min(hints.greenBits, 1u)),
                GLX_BLUE_SIZE,  static_cast<int>(NkClampU32Min(hints.blueBits, 1u)),
                GLX_ALPHA_SIZE, static_cast<int>(hints.alphaBits),
                GLX_DEPTH_SIZE, static_cast<int>(hints.depthBits),
                GLX_STENCIL_SIZE, static_cast<int>(hints.stencilBits),
                GLX_DOUBLEBUFFER, hints.doubleBuffer ? True : False,
                GLX_STEREO,       hints.stereo       ? True : False,
                GLX_ACCUM_RED_SIZE,   static_cast<int>(hints.accumRedBits),
                GLX_ACCUM_GREEN_SIZE, static_cast<int>(hints.accumGreenBits),
                GLX_ACCUM_BLUE_SIZE,  static_cast<int>(hints.accumBlueBits),
                GLX_ACCUM_ALPHA_SIZE, static_cast<int>(hints.accumAlphaBits),
                GLX_SAMPLE_BUFFERS, (hints.msaaSamples > 1u) ? 1 : 0,
                GLX_SAMPLES,        (hints.msaaSamples > 1u) ?
                                    static_cast<int>(hints.msaaSamples) : 0,
                0
            };
            int n = 0;
            GLXFBConfig* fbs = glXChooseFBConfig(dpy, screen, attrs, &n);
            if (fbs && n > 0) {
                XVisualInfo* vi = glXGetVisualFromFBConfig(dpy, fbs[0]);
                if (vi) {
                    cfg.surfaceHints.Set(NkSurfaceHintKey::NK_GLX_VISUAL_ID,
                                        static_cast<uintptr>(vi->visualid));
                    XFree(vi);
                }
                cfg.surfaceHints.Set(NkSurfaceHintKey::NK_GLX_FB_CONFIG_PTR, 0);
                XFree(fbs);
            }
            XCloseDisplay(dpy);
        #else
            (void)hints; (void)cfg;
        #endif
        }

        // =========================================================================
        // CreateOpenGLContext — cœur de NkContextCreate
        // =========================================================================
        static bool CreateOpenGLContext(NkContext& context) {
            const NkSurfaceDesc& surface = context.surface;

            // ── Windows WGL ───────────────────────────────────────────────────
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            HDC hdc = GetDC(surface.hwnd);
            if (!hdc) {
                NkSetContextError(context, 1001, "GetDC failed.");
                return false;
            }

            const int pixelFormat = GetPixelFormat(hdc);
            if (pixelFormat == 0) {
                bool appliedShared = false;
                const uintptr shareHwndHint = surface.appliedHints.Get(
                    NkSurfaceHintKey::NK_WGL_SHARE_PIXEL_FORMAT_HWND, 0);
                if (shareHwndHint != 0) {
                    HWND shareHwnd = reinterpret_cast<HWND>(shareHwndHint);
                    if (!IsWindow(shareHwnd)) {
                        ReleaseDC(surface.hwnd, hdc);
                        NkSetContextError(context, 1002, "Invalid share HWND.");
                        return false;
                    }
                    HDC shareDC = GetDC(shareHwnd);
                    if (!shareDC) {
                        ReleaseDC(surface.hwnd, hdc);
                        NkSetContextError(context, 1002, "GetDC(share) failed.");
                        return false;
                    }
                    const int spf = GetPixelFormat(shareDC);
                    PIXELFORMATDESCRIPTOR spfd = {};
                    spfd.nSize = sizeof(spfd); spfd.nVersion = 1;
                    const bool canDesc = (spf != 0) &&
                        (DescribePixelFormat(shareDC, spf, sizeof(spfd), &spfd) > 0);
                    ReleaseDC(shareHwnd, shareDC);
                    if (!canDesc || !SetPixelFormat(hdc, spf, &spfd)) {
                        ReleaseDC(surface.hwnd, hdc);
                        NkSetContextError(context, 1002, "SetPixelFormat(share) failed.");
                        return false;
                    }
                    appliedShared = true;
                }
                if (!appliedShared) {
                    PIXELFORMATDESCRIPTOR requested = NkBuildPixelFormatDescriptor(context.config);
                    PIXELFORMATDESCRIPTOR selected  = requested;
                    int pf = 0;
                    if (context.config.win32PixelFormat.forcedPixelFormatIndex > 0) {
                        pf = context.config.win32PixelFormat.forcedPixelFormatIndex;
                        PIXELFORMATDESCRIPTOR described = {};
                        described.nSize = sizeof(described); described.nVersion = 1;
                        if (DescribePixelFormat(hdc, pf, sizeof(described), &described) > 0)
                            selected = described;
                        else {
                            ReleaseDC(surface.hwnd, hdc);
                            NkSetContextError(context, 1002, "DescribePixelFormat failed.");
                            return false;
                        }
                    } else {
                        pf = ChoosePixelFormat(hdc, &requested);
                        if (pf != 0) {
                            PIXELFORMATDESCRIPTOR described = {};
                            described.nSize = sizeof(described); described.nVersion = 1;
                            if (DescribePixelFormat(hdc, pf, sizeof(described), &described) > 0)
                                selected = described;
                        }
                    }
                    if (pf == 0 || !SetPixelFormat(hdc, pf, &selected)) {
                        ReleaseDC(surface.hwnd, hdc);
                        NkSetContextError(context, 1002, "SetPixelFormat failed.");
                        return false;
                    }
                }
            }

            HGLRC rc = nullptr;
            if (NkShouldRequestExplicitGL(context.config)) {
                HGLRC bootstrap = wglCreateContext(hdc);
                if (!bootstrap) {
                    ReleaseDC(surface.hwnd, hdc);
                    NkSetContextError(context, 1003, "wglCreateContext bootstrap failed.");
                    return false;
                }
                if (!wglMakeCurrent(hdc, bootstrap)) {
                    wglDeleteContext(bootstrap);
                    ReleaseDC(surface.hwnd, hdc);
                    NkSetContextError(context, 1004, "wglMakeCurrent bootstrap failed.");
                    return false;
                }
                auto createAttribs = reinterpret_cast<NkWglCreateContextAttribsProc>(
                    wglGetProcAddress("wglCreateContextAttribsARB"));
                if (!createAttribs) {
                    wglMakeCurrent(nullptr, nullptr);
                    wglDeleteContext(bootstrap);
                    ReleaseDC(surface.hwnd, hdc);
                    NkSetContextError(context, 1005, "WGL_ARB_create_context indisponible.");
                    return false;
                }
                int attribs[16] = {};
                NkBuildWglContextAttribs(context.config, attribs);
                rc = createAttribs(hdc, nullptr, attribs);
                wglMakeCurrent(nullptr, nullptr);
                wglDeleteContext(bootstrap);
                if (!rc) {
                    ReleaseDC(surface.hwnd, hdc);
                    NkSetContextError(context, 1006, "wglCreateContextAttribsARB failed.");
                    return false;
                }
            } else {
                rc = wglCreateContext(hdc);
                if (!rc) {
                    ReleaseDC(surface.hwnd, hdc);
                    NkSetContextError(context, 1003, "wglCreateContext failed.");
                    return false;
                }
            }

            context.nativeDisplay        = reinterpret_cast<void*>(surface.hinstance);
            context.nativeDeviceContext  = hdc;
            context.nativeContext        = rc;
            context.nativeDrawable       = reinterpret_cast<uintptr>(surface.hwnd);
            context.getProcAddress       = &NkWglProc;
            context.ownsNativeDisplay    = false;
            return true;

            // ── Linux GLX ─────────────────────────────────────────────────────
        #elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
            Display* dpy = nullptr;
            bool ownDisplay = false;
        #if defined(NKENTSEU_WINDOWING_XLIB)
            dpy = surface.display;
            ownDisplay = false;
        #else
            dpy = XOpenDisplay(nullptr);
            ownDisplay = true;
        #endif
            if (!dpy) {
                NkSetContextError(context, 1101, "XOpenDisplay failed.");
                return false;
            }
            const int screen = DefaultScreen(dpy);
            const VisualID hinted = static_cast<VisualID>(
                surface.appliedHints.Get(NkSurfaceHintKey::NK_GLX_VISUAL_ID));
            GLXContext glx = nullptr;

            if (NkShouldRequestExplicitGL(context.config)) {
                const char* exts = glXQueryExtensionsString(dpy, screen);
                if (!HasGLXExtension(exts, "GLX_ARB_create_context")) {
                    if (ownDisplay) XCloseDisplay(dpy);
                    NkSetContextError(context, 1104, "GLX_ARB_create_context indisponible.");
                    return false;
                }
                auto createAttribs = reinterpret_cast<NkGlxCreateContextAttribsProc>(
                    glXGetProcAddressARB(
                        reinterpret_cast<const GLubyte*>("glXCreateContextAttribsARB")));
                if (!createAttribs) {
                    if (ownDisplay) XCloseDisplay(dpy);
                    NkSetContextError(context, 1105, "glXCreateContextAttribsARB indisponible.");
                    return false;
                }
                GLXFBConfig fbConfig = ChooseGlxFBConfig(dpy, screen, context.config, hinted);
                if (!fbConfig) {
                    if (ownDisplay) XCloseDisplay(dpy);
                    NkSetContextError(context, 1106, "Aucun GLXFBConfig compatible.");
                    return false;
                }
                int attribs[16] = {};
                BuildGlxContextAttribs(context.config, attribs);
                glx = createAttribs(dpy, fbConfig, nullptr, True, attribs);
                if (!glx) {
                    if (ownDisplay) XCloseDisplay(dpy);
                    NkSetContextError(context, 1107, "glXCreateContextAttribsARB failed.");
                    return false;
                }
            } else {
                XVisualInfo* vi = nullptr;
                if (hinted != 0) {
                    XVisualInfo tmpl = {}; tmpl.visualid = hinted;
                    int n = 0;
                    vi = XGetVisualInfo(dpy, VisualIDMask, &tmpl, &n);
                    if (!(vi && n > 0)) vi = nullptr;
                }
                if (!vi) {
                    const int attrs[] = {
                        GLX_RGBA,
                        GLX_RED_SIZE,   static_cast<int>(NkClampU32Min(context.config.redBits, 1u)),
                        GLX_GREEN_SIZE, static_cast<int>(NkClampU32Min(context.config.greenBits, 1u)),
                        GLX_BLUE_SIZE,  static_cast<int>(NkClampU32Min(context.config.blueBits, 1u)),
                        GLX_ALPHA_SIZE, static_cast<int>(context.config.alphaBits),
                        GLX_DOUBLEBUFFER, context.config.doubleBuffer ? True : False,
                        GLX_DEPTH_SIZE, static_cast<int>(context.config.depthBits),
                        GLX_STENCIL_SIZE, static_cast<int>(context.config.stencilBits),
                        0
                    };
                    vi = glXChooseVisual(dpy, screen, const_cast<int*>(attrs));
                }
                if (!vi) {
                    if (ownDisplay) XCloseDisplay(dpy);
                    NkSetContextError(context, 1102, "Aucun GLX visual disponible.");
                    return false;
                }
                glx = glXCreateContext(dpy, vi, nullptr, True);
                XFree(vi);
                if (!glx) {
                    if (ownDisplay) XCloseDisplay(dpy);
                    NkSetContextError(context, 1103, "glXCreateContext failed.");
                    return false;
                }
            }

            context.nativeDisplay     = dpy;
            context.nativeContext     = reinterpret_cast<void*>(glx);
            context.nativeDrawable    = static_cast<uintptr>(surface.window);
            context.getProcAddress    = &NkGlxProc;
            context.ownsNativeDisplay = ownDisplay;
            return true;

            // ── Wayland EGL ───────────────────────────────────────────────────
        #elif defined(NKENTSEU_WINDOWING_WAYLAND) && NKENTSEU_HAS_WAYLAND_EGL
            ::wl_display* wlDisplay = surface.display;
            ::wl_surface* wlSurface = surface.surface;
            if (!wlDisplay || !wlSurface) {
                NkSetContextError(context, 1151, "Wayland display/surface invalide.");
                return false;
            }
            const uintptr eglDisplayHint = surface.appliedHints.Get(NkSurfaceHintKey::NK_EGL_DISPLAY, 0);
            const bool externalDisplay = (eglDisplayHint != 0);
            EGLDisplay eglDisplay = externalDisplay
                ? reinterpret_cast<EGLDisplay>(eglDisplayHint)
                : eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(wlDisplay));
            if (eglDisplay == EGL_NO_DISPLAY) {
                NkSetContextError(context, 1152, "eglGetDisplay failed (Wayland).");
                return false;
            }
            EGLint major = 0, minor = 0;
            if (eglInitialize(eglDisplay, &major, &minor) != EGL_TRUE) {
                NkSetContextError(context, 1153, "eglInitialize failed (Wayland).");
                return false;
            }
            const bool isES = (context.config.profile == NkContextProfile::NK_CONTEXT_PROFILE_ES);
            if (eglBindAPI(isES ? EGL_OPENGL_ES_API : EGL_OPENGL_API) != EGL_TRUE) {
                if (!externalDisplay) eglTerminate(eglDisplay);
                NkSetContextError(context, 1154, "eglBindAPI failed (Wayland).");
                return false;
            }
            EGLConfig eglConfig = reinterpret_cast<EGLConfig>(
                surface.appliedHints.Get(NkSurfaceHintKey::NK_EGL_CONFIG, 0));
            if (!eglConfig && !NkEglChooseConfig(eglDisplay, context.config, &eglConfig, isES)) {
                if (!externalDisplay) eglTerminate(eglDisplay);
                NkSetContextError(context, 1155, "eglChooseConfig failed (Wayland).");
                return false;
            }
            const int w = static_cast<int>(NkClampU32Min(surface.width, 1u));
            const int h = static_cast<int>(NkClampU32Min(surface.height, 1u));
            ::wl_egl_window* eglWindow = wl_egl_window_create(wlSurface, w, h);
            if (!eglWindow) {
                if (!externalDisplay) eglTerminate(eglDisplay);
                NkSetContextError(context, 1156, "wl_egl_window_create failed.");
                return false;
            }
            EGLSurface eglSurface = eglCreateWindowSurface(
                eglDisplay, eglConfig,
                reinterpret_cast<EGLNativeWindowType>(eglWindow), nullptr);
            if (eglSurface == EGL_NO_SURFACE) {
                wl_egl_window_destroy(eglWindow);
                if (!externalDisplay) eglTerminate(eglDisplay);
                NkSetContextError(context, 1157, "eglCreateWindowSurface failed (Wayland).");
                return false;
            }
            EGLContext eglCtx = NkEglCreateContext(eglDisplay, eglConfig, context.config);
            if (eglCtx == EGL_NO_CONTEXT) {
                eglDestroySurface(eglDisplay, eglSurface);
                wl_egl_window_destroy(eglWindow);
                if (!externalDisplay) eglTerminate(eglDisplay);
                NkSetContextError(context, 1158, "eglCreateContext failed (Wayland).");
                return false;
            }
            context.nativeDisplay     = eglDisplay;
            context.nativeContext     = eglCtx;
            context.nativeWindow      = eglWindow;
            context.nativeDrawable    = reinterpret_cast<uintptr>(eglSurface);
            context.getProcAddress    = &NkEglProc;
            context.ownsNativeDisplay = !externalDisplay;
            return true;

        #elif defined(NKENTSEU_WINDOWING_WAYLAND)
            NkSetContextError(context, 1150, "Wayland EGL headers manquants (EGL/egl.h + wayland-egl.h).");
            return false;

            // ── Android EGL ───────────────────────────────────────────────────
        #elif defined(NKENTSEU_PLATFORM_ANDROID)
            ANativeWindow* androidWin = surface.nativeWindow;
            if (!androidWin) {
                NkSetContextError(context, 1200, "ANativeWindow manquant (Android EGL).");
                return false;
            }
            EGLDisplay eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
            if (eglDisplay == EGL_NO_DISPLAY) {
                NkSetContextError(context, 1201, "eglGetDisplay failed (Android).");
                return false;
            }
            EGLint major = 0, minor = 0;
            if (eglInitialize(eglDisplay, &major, &minor) != EGL_TRUE) {
                NkSetContextError(context, 1202, "eglInitialize failed (Android).");
                return false;
            }
            // Android impose OpenGL ES
            if (eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE) {
                eglTerminate(eglDisplay);
                NkSetContextError(context, 1203, "eglBindAPI(ES) failed (Android).");
                return false;
            }
            EGLConfig eglConfig = nullptr;
            if (!NkEglChooseConfig(eglDisplay, context.config, &eglConfig, true)) {
                eglTerminate(eglDisplay);
                NkSetContextError(context, 1204, "eglChooseConfig failed (Android).");
                return false;
            }
            EGLSurface eglSurface = eglCreateWindowSurface(
                eglDisplay, eglConfig,
                reinterpret_cast<EGLNativeWindowType>(androidWin), nullptr);
            if (eglSurface == EGL_NO_SURFACE) {
                eglTerminate(eglDisplay);
                NkSetContextError(context, 1205, "eglCreateWindowSurface failed (Android).");
                return false;
            }
            EGLContext eglCtx = NkEglCreateContext(eglDisplay, eglConfig, context.config);
            if (eglCtx == EGL_NO_CONTEXT) {
                eglDestroySurface(eglDisplay, eglSurface);
                eglTerminate(eglDisplay);
                NkSetContextError(context, 1206, "eglCreateContext failed (Android).");
                return false;
            }
            context.nativeDisplay     = eglDisplay;
            context.nativeContext     = eglCtx;
            context.nativeWindow      = nullptr;   // ANativeWindow géré par NkAndroidWindow
            context.nativeDrawable    = reinterpret_cast<uintptr>(eglSurface);
            context.getProcAddress    = &NkEglProc;
            context.ownsNativeDisplay = true;
            return true;

            // ── HarmonyOS EGL ─────────────────────────────────────────────────
        #elif defined(NKENTSEU_PLATFORM_HARMONYOS) && NKENTSEU_HAS_HARMONY_EGL
            // HarmonyOS utilise exactement la même API EGL qu'Android.
            // OHNativeWindow* est compatible avec EGLNativeWindowType.
            OHNativeWindow* ohWin = surface.ohNativeWindow;
            if (!ohWin) {
                NkSetContextError(context, 1300, "OHNativeWindow manquant (HarmonyOS EGL).");
                return false;
            }
            EGLDisplay eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
            if (eglDisplay == EGL_NO_DISPLAY) {
                NkSetContextError(context, 1301, "eglGetDisplay failed (HarmonyOS).");
                return false;
            }
            EGLint major = 0, minor = 0;
            if (eglInitialize(eglDisplay, &major, &minor) != EGL_TRUE) {
                NkSetContextError(context, 1302, "eglInitialize failed (HarmonyOS).");
                return false;
            }
            // HarmonyOS impose OpenGL ES (pas de GL desktop)
            if (eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE) {
                eglTerminate(eglDisplay);
                NkSetContextError(context, 1303, "eglBindAPI(ES) failed (HarmonyOS).");
                return false;
            }
            EGLConfig eglConfig = nullptr;
            if (!NkEglChooseConfig(eglDisplay, context.config, &eglConfig, true)) {
                eglTerminate(eglDisplay);
                NkSetContextError(context, 1304, "eglChooseConfig failed (HarmonyOS).");
                return false;
            }
            EGLSurface eglSurface = eglCreateWindowSurface(
                eglDisplay, eglConfig,
                reinterpret_cast<EGLNativeWindowType>(ohWin), nullptr);
            if (eglSurface == EGL_NO_SURFACE) {
                eglTerminate(eglDisplay);
                NkSetContextError(context, 1305, "eglCreateWindowSurface failed (HarmonyOS).");
                return false;
            }
            EGLContext eglCtx = NkEglCreateContext(eglDisplay, eglConfig, context.config);
            if (eglCtx == EGL_NO_CONTEXT) {
                eglDestroySurface(eglDisplay, eglSurface);
                eglTerminate(eglDisplay);
                NkSetContextError(context, 1306, "eglCreateContext failed (HarmonyOS).");
                return false;
            }
            context.nativeDisplay     = eglDisplay;
            context.nativeContext     = eglCtx;
            context.nativeWindow      = nullptr;   // OHNativeWindow géré par NkHarmonyWindow
            context.nativeDrawable    = reinterpret_cast<uintptr>(eglSurface);
            context.getProcAddress    = &NkEglProc;
            context.ownsNativeDisplay = true;
            return true;

        #elif defined(NKENTSEU_PLATFORM_HARMONYOS)
            NkSetContextError(context, 1307, "EGL headers manquants (HarmonyOS).");
            return false;

            // ── macOS NSOpenGLContext ─────────────────────────────────────────
        #elif defined(NKENTSEU_PLATFORM_MACOS)
            // macOS : OpenGL deprecated depuis macOS 10.14.
            // Recommandé : utiliser Metal (surface-only) via NkContextGetModeForApi.
            // Ce chemin supporte les apps qui ont encore besoin d'OpenGL legacy.
        #ifdef __OBJC__
            NSOpenGLPixelFormatAttribute pixelAttrs[] = {
                NSOpenGLPFADoubleBuffer,
                NSOpenGLPFADepthSize,        static_cast<NSOpenGLPixelFormatAttribute>(context.config.depthBits),
                NSOpenGLPFAStencilSize,      static_cast<NSOpenGLPixelFormatAttribute>(context.config.stencilBits),
                NSOpenGLPFAColorSize,        static_cast<NSOpenGLPixelFormatAttribute>(
                    context.config.redBits + context.config.greenBits +
                    context.config.blueBits + context.config.alphaBits),
                NSOpenGLPFAOpenGLProfile,
                context.config.versionMajor >= 4
                    ? NSOpenGLProfileVersion4_1Core
                    : (context.config.versionMajor >= 3
                        ? NSOpenGLProfileVersion3_2Core
                        : NSOpenGLProfileVersionLegacy),
                0
            };
            NSOpenGLPixelFormat* pixFmt = [[NSOpenGLPixelFormat alloc]
                initWithAttributes:pixelAttrs];
            if (!pixFmt) {
                NkSetContextError(context, 1400, "NSOpenGLPixelFormat creation failed (macOS).");
                return false;
            }
            NSOpenGLContext* glCtx = [[NSOpenGLContext alloc]
                initWithFormat:pixFmt shareContext:nil];
            [pixFmt release];
            if (!glCtx) {
                NkSetContextError(context, 1401, "NSOpenGLContext creation failed (macOS).");
                return false;
            }
            // Associer le contexte à la NSView du window
            NSView* nsView = static_cast<NSView*>(surface.view);
            if (nsView) [glCtx setView:nsView];
            context.nativeContext  = (__bridge_retained void*)glCtx;
            context.nativeDisplay  = nullptr;
            context.nativeDrawable = 0;
            context.getProcAddress = [](const char* name) -> void* {
                // OpenGL.framework est le loader sur macOS
                static void* handle = nullptr;
                if (!handle) handle = dlopen(
                    "/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY);
                return handle ? dlsym(handle, name) : nullptr;
            };
            context.ownsNativeDisplay = false;
            return true;
        #else
            NkSetContextError(context, 1402,
                "Compilation Objective-C requise pour NSOpenGLContext (macOS). "
                "Utiliser un fichier .mm ou passer à Metal (NK_GFX_API_METAL).");
            return false;
        #endif // __OBJC__

            // ── iOS / tvOS / watchOS / visionOS — EAGLContext ─────────────────
        #elif defined(NKENTSEU_PLATFORM_IOS)   || defined(NKENTSEU_PLATFORM_TVOS)    || \
              defined(NKENTSEU_PLATFORM_WATCHOS)|| defined(NKENTSEU_PLATFORM_VISIONOS)
        #ifdef __OBJC__
            // OpenGL ES est deprecated depuis iOS 12, mais encore fonctionnel.
            // Recommandé : utiliser Metal (surface-only).
            EAGLRenderingAPI eaglApi = kEAGLRenderingAPIOpenGLES3;
            if (context.config.versionMajor < 3)
                eaglApi = kEAGLRenderingAPIOpenGLES2;

            EAGLContext* eaglCtx = [[EAGLContext alloc] initWithAPI:eaglApi];
            if (!eaglCtx && eaglApi == kEAGLRenderingAPIOpenGLES3) {
                // Fallback ES2 si ES3 non disponible (anciens devices)
                eaglCtx = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
            }
            if (!eaglCtx) {
                NkSetContextError(context, 1500, "EAGLContext creation failed (iOS/tvOS).");
                return false;
            }
            context.nativeContext  = (__bridge_retained void*)eaglCtx;
            context.nativeDisplay  = nullptr;
            context.nativeDrawable = 0;
            context.getProcAddress = [](const char* name) -> void* {
                static void* handle = nullptr;
                if (!handle) handle = dlopen(
                    "/System/Library/Frameworks/OpenGLES.framework/OpenGLES", RTLD_LAZY);
                return handle ? dlsym(handle, name) : nullptr;
            };
            context.ownsNativeDisplay = false;
            return true;
        #else
            NkSetContextError(context, 1501,
                "Compilation Objective-C requise pour EAGLContext (iOS). "
                "Utiliser un fichier .mm ou passer à Metal (NK_GFX_API_METAL).");
            return false;
        #endif // __OBJC__

            // ── WebAssembly / Emscripten — WebGL ─────────────────────────────
        #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
            // Emscripten expose OpenGL ES via WebGL — pas de contexte EGL natif.
            // Le contexte WebGL est créé via l'API HTML5 Emscripten.
            EmscriptenWebGLContextAttributes attrs;
            emscripten_webgl_init_context_attributes(&attrs);
            attrs.alpha              = (context.config.alphaBits > 0);
            attrs.depth              = (context.config.depthBits > 0);
            attrs.stencil            = (context.config.stencilBits > 0);
            attrs.antialias          = (context.config.msaaSamples > 1u);
            attrs.premultipliedAlpha = false;
            attrs.preserveDrawingBuffer = false;
            attrs.enableExtensionsByDefault = true;
            // Choisir WebGL 2 (ES 3) ou WebGL 1 (ES 2) selon versionMajor
            attrs.majorVersion = (context.config.versionMajor >= 3) ? 2 : 1;

            const char* canvasId = surface.canvasId ? surface.canvasId : "#canvas";
            EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webglCtx =
                emscripten_webgl_create_context(canvasId, &attrs);
            if (webglCtx <= 0) {
                // Fallback WebGL 1 si WebGL 2 non supporté
                attrs.majorVersion = 1;
                webglCtx = emscripten_webgl_create_context(canvasId, &attrs);
            }
            if (webglCtx <= 0) {
                NkSetContextError(context, 1600, "WebGL context creation failed (Emscripten).");
                return false;
            }
            context.nativeContext  = reinterpret_cast<void*>(static_cast<intptr_t>(webglCtx));
            context.nativeDisplay  = nullptr;
            context.nativeDrawable = 0;
            // Emscripten ne fournit pas de getProcAddress classique —
            // glad/glbinding doivent utiliser emscripten_webgl_make_context_current
            context.getProcAddress = [](const char* name) -> void* {
                // Les symboles WebGL sont directement linkés dans le binaire WASM
                // via glad-gles avec la config Emscripten.
                return nullptr;
            };
            context.ownsNativeDisplay = false;
            return true;

        #else
            NkSetContextError(context, 1999, "Création de contexte OpenGL non supportée sur cette plateforme.");
            return false;
        #endif
        }

    } // namespace (anonymous)

    // =========================================================================
    // API publique
    // =========================================================================

    bool NkContextInit() {
        if (gInit) return true;
        gInit = true;
        NkContextResetHints();
        return true;
    }

    void NkContextShutdown() {
        gInit = false;
        NkContextResetHints();
    }

    void NkContextResetHints() {
        gHints = {};
        gHints.api          = graphics::NkGraphicsApi::NK_GFX_API_OPENGL;
        gHints.versionMajor = 3;
        gHints.versionMinor = 3;
        gHints.profile      = NkContextProfile::NK_CONTEXT_PROFILE_CORE;
        gHints.debug        = false;
        gHints.doubleBuffer = true;
        gHints.msaaSamples  = 1;
        gHints.vsync        = true;
        gHints.stereo       = false;
        gHints.redBits      = 8;
        gHints.greenBits    = 8;
        gHints.blueBits     = 8;
        gHints.alphaBits    = 8;
        gHints.depthBits    = 24;
        gHints.stencilBits  = 8;
        // Accumulation / aux buffer : 0 par défaut (inutilisés en GL 3+)
        gHints.accumRedBits = gHints.accumGreenBits =
        gHints.accumBlueBits = gHints.accumAlphaBits = 0;
        gHints.auxBuffers = 0;
    }

    void NkContextSetHints(const NkContextConfig& config) { gHints = config; }
    void NkContextSetApi(graphics::NkGraphicsApi api)     { gHints.api = api; }

    void NkContextSetWin32PixelFormat(const NkWin32PixelFormatConfig& config) {
        gHints.win32PixelFormat = config;
        if (config.useCustomDescriptor) {
            gHints.doubleBuffer  = config.doubleBuffer;
            gHints.stereo        = config.stereo;
            gHints.redBits       = config.redBits;
            gHints.greenBits     = config.greenBits;
            gHints.blueBits      = config.blueBits;
            gHints.alphaBits     = config.alphaBits;
            gHints.depthBits     = config.depthBits;
            gHints.stencilBits   = config.stencilBits;
            gHints.accumRedBits  = config.accumRedBits;
            gHints.accumGreenBits= config.accumGreenBits;
            gHints.accumBlueBits = config.accumBlueBits;
            gHints.accumAlphaBits= config.accumAlphaBits;
            gHints.auxBuffers    = config.auxBuffers;
        }
    }

    void NkContextWindowHint(NkContextHint hint, int32 value) {
        switch (hint) {
            case NkContextHint::NK_CONTEXT_HINT_API:
                if (value >= static_cast<int32>(graphics::NkGraphicsApi::NK_GFX_API_NONE) &&
                    value <  static_cast<int32>(graphics::NkGraphicsApi::NK_GFX_API_MAX))
                    gHints.api = static_cast<graphics::NkGraphicsApi>(value);
                break;
            case NkContextHint::NK_CONTEXT_HINT_VERSION_MAJOR:
                if (value > 0) gHints.versionMajor = static_cast<uint32>(value); break;
            case NkContextHint::NK_CONTEXT_HINT_VERSION_MINOR:
                if (value >= 0) gHints.versionMinor = static_cast<uint32>(value); break;
            case NkContextHint::NK_CONTEXT_HINT_PROFILE:
                if (value >= static_cast<int32>(NkContextProfile::NK_CONTEXT_PROFILE_ANY) &&
                    value <= static_cast<int32>(NkContextProfile::NK_CONTEXT_PROFILE_ES))
                    gHints.profile = static_cast<NkContextProfile>(value);
                break;
            case NkContextHint::NK_CONTEXT_HINT_DEBUG:
                gHints.debug = (value != 0); break;
            case NkContextHint::NK_CONTEXT_HINT_DOUBLEBUFFER:
                gHints.doubleBuffer = (value != 0);
                if (gHints.win32PixelFormat.useCustomDescriptor)
                    gHints.win32PixelFormat.doubleBuffer = gHints.doubleBuffer;
                break;
            case NkContextHint::NK_CONTEXT_HINT_MSAA_SAMPLES:
                gHints.msaaSamples = value > 1 ? static_cast<uint32>(value) : 1u; break;
            case NkContextHint::NK_CONTEXT_HINT_VSYNC:
                gHints.vsync = (value != 0); break;
            case NkContextHint::NK_CONTEXT_HINT_RED_BITS:
                gHints.redBits = static_cast<uint32>(value > 0 ? value : 1);
                if (gHints.win32PixelFormat.useCustomDescriptor)
                    gHints.win32PixelFormat.redBits = gHints.redBits; break;
            case NkContextHint::NK_CONTEXT_HINT_GREEN_BITS:
                gHints.greenBits = static_cast<uint32>(value > 0 ? value : 1);
                if (gHints.win32PixelFormat.useCustomDescriptor)
                    gHints.win32PixelFormat.greenBits = gHints.greenBits; break;
            case NkContextHint::NK_CONTEXT_HINT_BLUE_BITS:
                gHints.blueBits = static_cast<uint32>(value > 0 ? value : 1);
                if (gHints.win32PixelFormat.useCustomDescriptor)
                    gHints.win32PixelFormat.blueBits = gHints.blueBits; break;
            case NkContextHint::NK_CONTEXT_HINT_ALPHA_BITS:
                gHints.alphaBits = static_cast<uint32>(value >= 0 ? value : 0);
                if (gHints.win32PixelFormat.useCustomDescriptor)
                    gHints.win32PixelFormat.alphaBits = gHints.alphaBits; break;
            case NkContextHint::NK_CONTEXT_HINT_DEPTH_BITS:
                gHints.depthBits = static_cast<uint32>(value >= 0 ? value : 0);
                if (gHints.win32PixelFormat.useCustomDescriptor)
                    gHints.win32PixelFormat.depthBits = gHints.depthBits; break;
            case NkContextHint::NK_CONTEXT_HINT_STENCIL_BITS:
                gHints.stencilBits = static_cast<uint32>(value >= 0 ? value : 0);
                if (gHints.win32PixelFormat.useCustomDescriptor)
                    gHints.win32PixelFormat.stencilBits = gHints.stencilBits; break;
            case NkContextHint::NK_CONTEXT_HINT_STEREO:
                gHints.stereo = (value != 0);
                if (gHints.win32PixelFormat.useCustomDescriptor)
                    gHints.win32PixelFormat.stereo = gHints.stereo; break;
            default: break;
        }
    }

    NkContextConfig NkContextGetHints() { return gHints; }

    void NkContextApplyWindowHints(NkWindowConfig& config) {
        if (gHints.api != graphics::NkGraphicsApi::NK_GFX_API_OPENGL) return;
        ApplyGlxVisualHint(gHints, config);
    }

    NkContextMode NkContextGetModeForApi(graphics::NkGraphicsApi api) {
        return NkIsSurfaceOnlyApi(api)
            ? NkContextMode::NK_CONTEXT_MODE_SURFACE_ONLY
            : NkContextMode::NK_CONTEXT_MODE_GRAPHICS_CONTEXT;
    }

    bool NkContextCreate(NkWindow& window, NkContext& outContext,
                         const NkContextConfig* overrideConfig) {
        if (!gInit && !NkContextInit()) return false;
        if (!window.IsOpen()) {
            outContext = {};
            NkSetContextError(outContext, 1201, "NkContextCreate requiert une fenêtre ouverte.");
            return false;
        }
        outContext = {};
        outContext.config  = overrideConfig ? *overrideConfig : gHints;
        outContext.surface = window.GetSurfaceDesc();
        outContext.mode    = NkContextGetModeForApi(outContext.config.api);
        outContext.valid   = false;
        NkClearContextError(outContext);

        if (!outContext.surface.IsValid()) {
            NkSetContextError(outContext, 1202, "La surface de la fenêtre est invalide.");
            return false;
        }

        // Backends surface-only (Vulkan, Metal, DX, Software) :
        // le contexte est toujours valide, le RHI gère tout lui-même.
        if (outContext.mode == NkContextMode::NK_CONTEXT_MODE_SURFACE_ONLY) {
            outContext.valid = true;
            return true;
        }

        if (outContext.config.api != graphics::NkGraphicsApi::NK_GFX_API_OPENGL) {
            NkSetContextError(outContext, 1203,
                "Seul OpenGL utilise le mode contexte natif dans NKWindow. "
                "Vulkan/Metal/DX utilisent le mode surface-only.");
            return false;
        }

        if (!CreateOpenGLContext(outContext)) return false;
        outContext.valid = true;
        return true;
    }

    void NkContextDestroy(NkContext& context) {
        if (!context.valid) { context = {}; return; }

        if (context.mode == NkContextMode::NK_CONTEXT_MODE_GRAPHICS_CONTEXT &&
            context.config.api == graphics::NkGraphicsApi::NK_GFX_API_OPENGL) {

        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            HDC  hdc = static_cast<HDC>(context.nativeDeviceContext);
            HGLRC rc = static_cast<HGLRC>(context.nativeContext);
            if (rc) { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(rc); }
            if (hdc && context.surface.hwnd) ReleaseDC(context.surface.hwnd, hdc);

        #elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
            Display*    dpy = static_cast<Display*>(context.nativeDisplay);
            GLXContext  glx = reinterpret_cast<GLXContext>(context.nativeContext);
            if (dpy && glx) { glXMakeCurrent(dpy, 0, nullptr); glXDestroyContext(dpy, glx); }
            if (context.ownsNativeDisplay && dpy) XCloseDisplay(dpy);

        #elif defined(NKENTSEU_WINDOWING_WAYLAND) && NKENTSEU_HAS_WAYLAND_EGL
            EGLDisplay eglDisplay = reinterpret_cast<EGLDisplay>(context.nativeDisplay);
            EGLContext eglCtx     = reinterpret_cast<EGLContext>(context.nativeContext);
            EGLSurface eglSurface = reinterpret_cast<EGLSurface>(context.nativeDrawable);
            ::wl_egl_window* eglWin = static_cast<::wl_egl_window*>(context.nativeWindow);
            if (eglDisplay != EGL_NO_DISPLAY) {
                eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                if (eglCtx     != EGL_NO_CONTEXT) eglDestroyContext(eglDisplay, eglCtx);
                if (eglSurface != EGL_NO_SURFACE) eglDestroySurface(eglDisplay, eglSurface);
                if (context.ownsNativeDisplay)    eglTerminate(eglDisplay);
            }
            if (eglWin) wl_egl_window_destroy(eglWin);

        #elif defined(NKENTSEU_PLATFORM_ANDROID) || \
              (defined(NKENTSEU_PLATFORM_HARMONYOS) && NKENTSEU_HAS_HARMONY_EGL)
            // Android et HarmonyOS : même cleanup EGL
            EGLDisplay eglDisplay = reinterpret_cast<EGLDisplay>(context.nativeDisplay);
            EGLContext eglCtx     = reinterpret_cast<EGLContext>(context.nativeContext);
            EGLSurface eglSurface = reinterpret_cast<EGLSurface>(context.nativeDrawable);
            if (eglDisplay != EGL_NO_DISPLAY) {
                eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                if (eglCtx     != EGL_NO_CONTEXT) eglDestroyContext(eglDisplay, eglCtx);
                if (eglSurface != EGL_NO_SURFACE) eglDestroySurface(eglDisplay, eglSurface);
                if (context.ownsNativeDisplay)    eglTerminate(eglDisplay);
            }

        #elif defined(NKENTSEU_PLATFORM_MACOS) && defined(__OBJC__)
            NSOpenGLContext* glCtx = (__bridge_transfer NSOpenGLContext*)context.nativeContext;
            [glCtx clearDrawable];
            // ARC libère automatiquement glCtx

        #elif (defined(NKENTSEU_PLATFORM_IOS) || defined(NKENTSEU_PLATFORM_TVOS) || \
               defined(NKENTSEU_PLATFORM_WATCHOS) || defined(NKENTSEU_PLATFORM_VISIONOS)) \
              && defined(__OBJC__)
            EAGLContext* eaglCtx = (__bridge_transfer EAGLContext*)context.nativeContext;
            if ([EAGLContext currentContext] == eaglCtx)
                [EAGLContext setCurrentContext:nil];
            // ARC libère automatiquement eaglCtx

        #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
            EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webglCtx =
                static_cast<EMSCRIPTEN_WEBGL_CONTEXT_HANDLE>(
                    reinterpret_cast<intptr_t>(context.nativeContext));
            if (webglCtx > 0) emscripten_webgl_destroy_context(webglCtx);
        #endif
        }
        context = {};
    }

    bool NkContextMakeCurrent(NkContext& context) {
        if (!context.valid) return false;
        if (context.mode == NkContextMode::NK_CONTEXT_MODE_SURFACE_ONLY) return true;
        if (context.config.api != graphics::NkGraphicsApi::NK_GFX_API_OPENGL) {
            NkSetContextError(context, 1301, "MakeCurrent disponible seulement pour OpenGL.");
            return false;
        }

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        HDC  hdc = static_cast<HDC>(context.nativeDeviceContext);
        HGLRC rc = static_cast<HGLRC>(context.nativeContext);
        if (!hdc || !rc || !wglMakeCurrent(hdc, rc)) {
            NkSetContextError(context, 1302, "wglMakeCurrent failed.");
            return false;
        }
        return true;

    #elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
        Display*   dpy  = static_cast<Display*>(context.nativeDisplay);
        GLXContext glx  = reinterpret_cast<GLXContext>(context.nativeContext);
        GLXDrawable draw= static_cast<GLXDrawable>(context.nativeDrawable);
        if (!dpy || !glx || draw == 0 || !glXMakeCurrent(dpy, draw, glx)) {
            NkSetContextError(context, 1303, "glXMakeCurrent failed.");
            return false;
        }
        return true;

    #elif defined(NKENTSEU_WINDOWING_WAYLAND) && NKENTSEU_HAS_WAYLAND_EGL
        EGLDisplay eglDisplay = reinterpret_cast<EGLDisplay>(context.nativeDisplay);
        EGLContext eglCtx     = reinterpret_cast<EGLContext>(context.nativeContext);
        EGLSurface eglSurface = reinterpret_cast<EGLSurface>(context.nativeDrawable);
        if (eglDisplay == EGL_NO_DISPLAY || eglCtx == EGL_NO_CONTEXT || eglSurface == EGL_NO_SURFACE) {
            NkSetContextError(context, 1304, "Wayland EGL context incomplet.");
            return false;
        }
        ::wl_egl_window* eglWin = static_cast<::wl_egl_window*>(context.nativeWindow);
        if (eglWin) {
            wl_egl_window_resize(eglWin,
                static_cast<int>(NkClampU32Min(context.surface.width, 1u)),
                static_cast<int>(NkClampU32Min(context.surface.height, 1u)),
                0, 0);
        }
        if (eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglCtx) != EGL_TRUE) {
            NkSetContextError(context, 1305, "eglMakeCurrent failed (Wayland).");
            return false;
        }
        eglSwapInterval(eglDisplay, context.config.vsync ? 1 : 0);
        return true;

    #elif defined(NKENTSEU_WINDOWING_WAYLAND)
        NkSetContextError(context, 1306, "Wayland EGL headers manquants.");
        return false;

    #elif defined(NKENTSEU_PLATFORM_ANDROID) || \
          (defined(NKENTSEU_PLATFORM_HARMONYOS) && NKENTSEU_HAS_HARMONY_EGL)
        // Android et HarmonyOS : même MakeCurrent EGL
        EGLDisplay eglDisplay = reinterpret_cast<EGLDisplay>(context.nativeDisplay);
        EGLContext eglCtx     = reinterpret_cast<EGLContext>(context.nativeContext);
        EGLSurface eglSurface = reinterpret_cast<EGLSurface>(context.nativeDrawable);
        if (eglDisplay == EGL_NO_DISPLAY || eglCtx == EGL_NO_CONTEXT || eglSurface == EGL_NO_SURFACE) {
            NkSetContextError(context, 1307, "EGL context incomplet.");
            return false;
        }
        if (eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglCtx) != EGL_TRUE) {
            NkSetContextError(context, 1308, "eglMakeCurrent failed.");
            return false;
        }
        eglSwapInterval(eglDisplay, context.config.vsync ? 1 : 0);
        return true;

    #elif defined(NKENTSEU_PLATFORM_MACOS) && defined(__OBJC__)
        NSOpenGLContext* glCtx = (__bridge NSOpenGLContext*)context.nativeContext;
        if (!glCtx) {
            NkSetContextError(context, 1400, "NSOpenGLContext null (macOS).");
            return false;
        }
        [glCtx makeCurrentContext];
        return true;

    #elif (defined(NKENTSEU_PLATFORM_IOS) || defined(NKENTSEU_PLATFORM_TVOS) || \
           defined(NKENTSEU_PLATFORM_WATCHOS) || defined(NKENTSEU_PLATFORM_VISIONOS)) \
          && defined(__OBJC__)
        EAGLContext* eaglCtx = (__bridge EAGLContext*)context.nativeContext;
        if (!eaglCtx || ![EAGLContext setCurrentContext:eaglCtx]) {
            NkSetContextError(context, 1500, "EAGLContext::setCurrentContext failed (iOS).");
            return false;
        }
        return true;

    #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webglCtx =
            static_cast<EMSCRIPTEN_WEBGL_CONTEXT_HANDLE>(
                reinterpret_cast<intptr_t>(context.nativeContext));
        if (webglCtx <= 0 ||
            emscripten_webgl_make_context_current(webglCtx) != EMSCRIPTEN_RESULT_SUCCESS) {
            NkSetContextError(context, 1600, "emscripten_webgl_make_context_current failed.");
            return false;
        }
        return true;

    #else
        NkSetContextError(context, 1399, "MakeCurrent non supporté sur cette plateforme.");
        return false;
    #endif
    }

    void NkContextSwapBuffers(NkContext& context) {
        if (!context.valid) return;
        if (context.mode == NkContextMode::NK_CONTEXT_MODE_SURFACE_ONLY) return;
        if (context.config.api != graphics::NkGraphicsApi::NK_GFX_API_OPENGL) return;

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        HDC hdc = static_cast<HDC>(context.nativeDeviceContext);
        if (hdc) SwapBuffers(hdc);

    #elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
        Display* dpy = static_cast<Display*>(context.nativeDisplay);
        if (dpy && context.nativeDrawable != 0)
            glXSwapBuffers(dpy, static_cast<GLXDrawable>(context.nativeDrawable));

    #elif defined(NKENTSEU_WINDOWING_WAYLAND) && NKENTSEU_HAS_WAYLAND_EGL
        EGLDisplay eglDisplay = reinterpret_cast<EGLDisplay>(context.nativeDisplay);
        EGLSurface eglSurface = reinterpret_cast<EGLSurface>(context.nativeDrawable);
        if (eglDisplay != EGL_NO_DISPLAY && eglSurface != EGL_NO_SURFACE)
            eglSwapBuffers(eglDisplay, eglSurface);

    #elif defined(NKENTSEU_PLATFORM_ANDROID) || \
          (defined(NKENTSEU_PLATFORM_HARMONYOS) && NKENTSEU_HAS_HARMONY_EGL)
        // Android et HarmonyOS : même SwapBuffers EGL
        EGLDisplay eglDisplay = reinterpret_cast<EGLDisplay>(context.nativeDisplay);
        EGLSurface eglSurface = reinterpret_cast<EGLSurface>(context.nativeDrawable);
        if (eglDisplay != EGL_NO_DISPLAY && eglSurface != EGL_NO_SURFACE)
            eglSwapBuffers(eglDisplay, eglSurface);

    #elif defined(NKENTSEU_PLATFORM_MACOS) && defined(__OBJC__)
        NSOpenGLContext* glCtx = (__bridge NSOpenGLContext*)context.nativeContext;
        if (glCtx) [glCtx flushBuffer];

    #elif (defined(NKENTSEU_PLATFORM_IOS) || defined(NKENTSEU_PLATFORM_TVOS) || \
           defined(NKENTSEU_PLATFORM_WATCHOS) || defined(NKENTSEU_PLATFORM_VISIONOS)) \
          && defined(__OBJC__)
        // iOS : le swap est géré par CAEAGLLayer::presentRenderbuffer — pas ici.
        // L'appelant doit appeler [eaglCtx presentRenderbuffer:GL_RENDERBUFFER].

    #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        // WebGL : le swap est automatique via requestAnimationFrame.
        // Pas de SwapBuffers explicite nécessaire sous Emscripten.
    #endif
    }

    NkContextProc NkContextGetProcAddressLoader(const NkContext& context) {
        return context.getProcAddress;
    }

    void* NkContextGetProcAddress(NkContext& context, const char* procName) {
        if (!context.valid || !procName || !context.getProcAddress) return nullptr;
        return context.getProcAddress(procName);
    }

} // namespace nkentseu