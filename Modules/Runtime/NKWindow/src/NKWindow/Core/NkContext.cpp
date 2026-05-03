#include "NkContext.h"

#include "NkWindow.h"

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

#include <cstring>

#if defined(NKENTSEU_WINDOWING_WAYLAND)
#   if __has_include(<EGL/egl.h>) && __has_include(<wayland-egl.h>)
#       define NKENTSEU_HAS_WAYLAND_EGL 1
#       include <EGL/egl.h>
#       include <wayland-egl.h>
#   else
#       define NKENTSEU_HAS_WAYLAND_EGL 0
#   endif
#endif

namespace nkentseu {

    namespace {

        static NkContextConfig gHints{};
        static bool            gInit = false;

        static uint32 NkClampU32Min(uint32 value, uint32 minValue) {
            return value < minValue ? minValue : value;
        }

        static void NkSetContextError(NkContext& context, uint32 code, const char* msg) {
            context.lastError.code = code;
            context.lastError.message = msg;
        }

        static void NkClearContextError(NkContext& context) {
            context.lastError = NkError::Ok();
        }

        static bool NkIsSurfaceOnlyApi(graphics::NkGraphicsApi api) {
            return api == graphics::NkGraphicsApi::NK_GFX_API_VULKAN ||
                   api == graphics::NkGraphicsApi::NK_GFX_API_D3D11 ||
                   api == graphics::NkGraphicsApi::NK_GFX_API_D3D12 ||
                   api == graphics::NkGraphicsApi::NK_GFX_API_METAL ||
                   api == graphics::NkGraphicsApi::NK_GFX_API_SOFTWARE;
        }

        static bool NkShouldRequestExplicitOpenGLContext(const NkContextConfig& config) {
            return config.versionMajor >= 3u ||
                   config.versionMinor > 0u ||
                   config.profile != NkContextProfile::NK_CONTEXT_PROFILE_ANY ||
                   config.debug;
        }

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        static constexpr int kWglContextMajorVersion = 0x2091;
        static constexpr int kWglContextMinorVersion = 0x2092;
        static constexpr int kWglContextFlags = 0x2094;
        static constexpr int kWglContextProfileMask = 0x9126;
        static constexpr int kWglContextDebugBit = 0x0001;
        static constexpr int kWglContextCoreProfileBit = 0x00000001;
        static constexpr int kWglContextCompatibilityProfileBit = 0x00000002;
        static constexpr int kWglContextEs2ProfileBit = 0x00000004;

        using NkWglCreateContextAttribsProc = HGLRC(WINAPI*)(HDC, HGLRC, const int*);

        static BYTE NkToByte(uint32 value) {
            return static_cast<BYTE>(value > 255u ? 255u : value);
        }

        static void NkBuildWglContextAttribs(const NkContextConfig& config, int (&attrs)[16]) {
            int i = 0;
            attrs[i++] = kWglContextMajorVersion;
            attrs[i++] = static_cast<int>(NkClampU32Min(config.versionMajor, 1u));
            attrs[i++] = kWglContextMinorVersion;
            attrs[i++] = static_cast<int>(config.versionMinor);

            int flags = 0;
            if (config.debug) flags |= kWglContextDebugBit;
            if (flags != 0) {
                attrs[i++] = kWglContextFlags;
                attrs[i++] = flags;
            }

            int profileMask = 0;
            switch (config.profile) {
                case NkContextProfile::NK_CONTEXT_PROFILE_CORE:
                    profileMask = kWglContextCoreProfileBit;
                    break;
                case NkContextProfile::NK_CONTEXT_PROFILE_COMPATIBILITY:
                    profileMask = kWglContextCompatibilityProfileBit;
                    break;
                case NkContextProfile::NK_CONTEXT_PROFILE_ES:
                    profileMask = kWglContextEs2ProfileBit;
                    break;
                default:
                    break;
            }
            if (profileMask != 0) {
                attrs[i++] = kWglContextProfileMask;
                attrs[i++] = profileMask;
            }

            attrs[i++] = 0;
        }

        static BYTE NkToLayerByte(int32 layerType) {
            if (layerType > 0) return static_cast<BYTE>(PFD_OVERLAY_PLANE);
            if (layerType < 0) return static_cast<BYTE>(PFD_UNDERLAY_PLANE);
            return static_cast<BYTE>(PFD_MAIN_PLANE);
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
                pfd.dwFlags = flags;

                pfd.iPixelType = static_cast<BYTE>(custom.pixelType == 1u ? PFD_TYPE_COLORINDEX : PFD_TYPE_RGBA);
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
                pfd.bReserved       = 0;
                pfd.dwLayerMask     = custom.layerMask;
                pfd.dwVisibleMask   = custom.visibleMask;
                pfd.dwDamageMask    = custom.damageMask;
                return pfd;
            }

            DWORD flags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
            if (config.doubleBuffer) flags |= PFD_DOUBLEBUFFER;
            if (config.stereo) flags |= PFD_STEREO;
            pfd.dwFlags = flags;
            pfd.iPixelType = PFD_TYPE_RGBA;

            const uint32 totalColorBits = NkClampU32Min(config.redBits + config.greenBits + config.blueBits + config.alphaBits, 24u);
            pfd.cColorBits      = NkToByte(totalColorBits);
            pfd.cRedBits        = NkToByte(config.redBits);
            pfd.cGreenBits      = NkToByte(config.greenBits);
            pfd.cBlueBits       = NkToByte(config.blueBits);
            pfd.cAlphaBits      = NkToByte(config.alphaBits);
            pfd.cAccumRedBits   = NkToByte(config.accumRedBits);
            pfd.cAccumGreenBits = NkToByte(config.accumGreenBits);
            pfd.cAccumBlueBits  = NkToByte(config.accumBlueBits);
            pfd.cAccumAlphaBits = NkToByte(config.accumAlphaBits);
            pfd.cDepthBits      = NkToByte(config.depthBits);
            pfd.cStencilBits    = NkToByte(config.stencilBits);
            pfd.cAuxBuffers     = NkToByte(config.auxBuffers);
            pfd.iLayerType      = PFD_MAIN_PLANE;
            return pfd;
        }
    #endif

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        static void* NkWglProc(const char* name) {
            if (!name) return nullptr;
            void* p = reinterpret_cast<void*>(wglGetProcAddress(name));
            if (p) return p;

            static HMODULE sOgl = GetModuleHandleA("opengl32.dll");
            if (!sOgl) sOgl = LoadLibraryA("opengl32.dll");
            if (!sOgl) return nullptr;
            return reinterpret_cast<void*>(GetProcAddress(sOgl, name));
        }
    #endif

    #if defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
        static constexpr int kGlxContextMajorVersion = 0x2091;
        static constexpr int kGlxContextMinorVersion = 0x2092;
        static constexpr int kGlxContextFlags = 0x2094;
        static constexpr int kGlxContextProfileMask = 0x9126;
        static constexpr int kGlxContextDebugBit = 0x00000001;
        static constexpr int kGlxContextCoreProfileBit = 0x00000001;
        static constexpr int kGlxContextCompatibilityProfileBit = 0x00000002;
        static constexpr int kGlxContextEs2ProfileBit = 0x00000004;

        using NkGlxCreateContextAttribsProc = GLXContext(*)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

        static void* NkGlxProc(const char* name) {
            if (!name) return nullptr;
            return reinterpret_cast<void*>(
                glXGetProcAddressARB(reinterpret_cast<const GLubyte*>(name)));
        }

        static bool HasOpenGLExtension(const char* extensionList, const char* extensionName) {
            if (!extensionList || !extensionName || *extensionName == '\0') return false;

            const size_t nameLen = ::strlen(extensionName);
            const char* scan = extensionList;
            while (true) {
                const char* where = ::strstr(scan, extensionName);
                if (!where) return false;

                const char* end = where + nameLen;
                const bool startOk = (where == extensionList) || (where[-1] == ' ');
                const bool endOk = (*end == '\0') || (*end == ' ');
                if (startOk && endOk) return true;

                scan = end;
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
            if (flags != 0) {
                attrs[i++] = kGlxContextFlags;
                attrs[i++] = flags;
            }

            int profileMask = 0;
            switch (config.profile) {
                case NkContextProfile::NK_CONTEXT_PROFILE_CORE:
                    profileMask = kGlxContextCoreProfileBit;
                    break;
                case NkContextProfile::NK_CONTEXT_PROFILE_COMPATIBILITY:
                    profileMask = kGlxContextCompatibilityProfileBit;
                    break;
                case NkContextProfile::NK_CONTEXT_PROFILE_ES:
                    profileMask = kGlxContextEs2ProfileBit;
                    break;
                default:
                    break;
            }
            if (profileMask != 0) {
                attrs[i++] = kGlxContextProfileMask;
                attrs[i++] = profileMask;
            }

            attrs[i++] = 0;
        }

        static GLXFBConfig ChooseGlxFBConfig(Display* dpy,
                                             int screen,
                                             const NkContextConfig& config,
                                             VisualID preferredVisual) {
            const int hasMsaa = (config.msaaSamples > 1u) ? 1 : 0;
            const int msaaSamples = (config.msaaSamples > 1u)
                ? static_cast<int>(config.msaaSamples)
                : 0;
            const int attrs[] = {
                GLX_X_RENDERABLE, True,
                GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
                GLX_RENDER_TYPE, GLX_RGBA_BIT,
                GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
                GLX_RED_SIZE, static_cast<int>(NkClampU32Min(config.redBits, 1u)),
                GLX_GREEN_SIZE, static_cast<int>(NkClampU32Min(config.greenBits, 1u)),
                GLX_BLUE_SIZE, static_cast<int>(NkClampU32Min(config.blueBits, 1u)),
                GLX_ALPHA_SIZE, static_cast<int>(config.alphaBits),
                GLX_DEPTH_SIZE, static_cast<int>(config.depthBits),
                GLX_STENCIL_SIZE, static_cast<int>(config.stencilBits),
                GLX_DOUBLEBUFFER, config.doubleBuffer ? True : False,
                GLX_STEREO, config.stereo ? True : False,
                GLX_ACCUM_RED_SIZE, static_cast<int>(config.accumRedBits),
                GLX_ACCUM_GREEN_SIZE, static_cast<int>(config.accumGreenBits),
                GLX_ACCUM_BLUE_SIZE, static_cast<int>(config.accumBlueBits),
                GLX_ACCUM_ALPHA_SIZE, static_cast<int>(config.accumAlphaBits),
                GLX_SAMPLE_BUFFERS, hasMsaa,
                GLX_SAMPLES, msaaSamples,
                0
            };

            int fbCount = 0;
            GLXFBConfig* fbs = glXChooseFBConfig(dpy, screen, attrs, &fbCount);
            if (!fbs || fbCount <= 0) return nullptr;

            int selectedIndex = 0;
            if (preferredVisual != 0) {
                for (int i = 0; i < fbCount; ++i) {
                    XVisualInfo* vi = glXGetVisualFromFBConfig(dpy, fbs[i]);
                    if (!vi) continue;

                    const bool match = (vi->visualid == preferredVisual);
                    XFree(vi);
                    if (match) {
                        selectedIndex = i;
                        break;
                    }
                }
            }

            GLXFBConfig chosen = fbs[selectedIndex];
            XFree(fbs);
            return chosen;
        }
    #endif

    #if defined(NKENTSEU_WINDOWING_WAYLAND) && NKENTSEU_HAS_WAYLAND_EGL
        static void* NkEglProc(const char* name) {
            if (!name) return nullptr;
            return reinterpret_cast<void*>(eglGetProcAddress(name));
        }

        static bool IsEglEsProfile(const NkContextConfig& config) {
            return config.profile == NkContextProfile::NK_CONTEXT_PROFILE_ES;
        }

        static EGLint EglRenderableType(const NkContextConfig& config) {
        #ifdef EGL_OPENGL_ES3_BIT
            if (IsEglEsProfile(config) && config.versionMajor >= 3u) {
                return EGL_OPENGL_ES3_BIT;
            }
        #endif
            if (IsEglEsProfile(config)) {
                return EGL_OPENGL_ES2_BIT;
            }
            return EGL_OPENGL_BIT;
        }

        static bool ChooseWaylandEglConfig(EGLDisplay display,
                                           const NkContextConfig& config,
                                           EGLConfig* outConfig) {
            if (!outConfig || display == EGL_NO_DISPLAY) return false;

            const EGLint attrs[] = {
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                EGL_RENDERABLE_TYPE, EglRenderableType(config),
                EGL_RED_SIZE, static_cast<EGLint>(config.redBits),
                EGL_GREEN_SIZE, static_cast<EGLint>(config.greenBits),
                EGL_BLUE_SIZE, static_cast<EGLint>(config.blueBits),
                EGL_ALPHA_SIZE, static_cast<EGLint>(config.alphaBits),
                EGL_DEPTH_SIZE, static_cast<EGLint>(config.depthBits),
                EGL_STENCIL_SIZE, static_cast<EGLint>(config.stencilBits),
                EGL_SAMPLES, (config.msaaSamples > 1u) ? static_cast<EGLint>(config.msaaSamples) : 0,
                EGL_NONE
            };

            EGLint count = 0;
            if (eglChooseConfig(display, attrs, outConfig, 1, &count) != EGL_TRUE || count < 1) {
                return false;
            }
            return true;
        }
    #endif

        static void ApplyGlxVisualHint(const NkContextConfig& hints, NkWindowConfig& cfg) {
        #if defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
            Display* dpy = XOpenDisplay(nullptr);
            if (!dpy) return;

            const int screen = DefaultScreen(dpy);
            const int redBits = static_cast<int>(NkClampU32Min(hints.redBits, 1u));
            const int greenBits = static_cast<int>(NkClampU32Min(hints.greenBits, 1u));
            const int blueBits = static_cast<int>(NkClampU32Min(hints.blueBits, 1u));
            const int alphaBits = static_cast<int>(hints.alphaBits);
            const int depthBits = static_cast<int>(hints.depthBits);
            const int stencilBits = static_cast<int>(hints.stencilBits);
            const int msaaSamples = (hints.msaaSamples > 1u)
                ? static_cast<int>(hints.msaaSamples)
                : 0;
            const int hasMsaa = (hints.msaaSamples > 1u) ? 1 : 0;

            const int attrs[] = {
                GLX_X_RENDERABLE, True,
                GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
                GLX_RENDER_TYPE, GLX_RGBA_BIT,
                GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
                GLX_RED_SIZE, redBits,
                GLX_GREEN_SIZE, greenBits,
                GLX_BLUE_SIZE, blueBits,
                GLX_ALPHA_SIZE, alphaBits,
                GLX_DEPTH_SIZE, depthBits,
                GLX_STENCIL_SIZE, stencilBits,
                GLX_DOUBLEBUFFER, hints.doubleBuffer ? True : False,
                GLX_STEREO, hints.stereo ? True : False,
                GLX_ACCUM_RED_SIZE, static_cast<int>(hints.accumRedBits),
                GLX_ACCUM_GREEN_SIZE, static_cast<int>(hints.accumGreenBits),
                GLX_ACCUM_BLUE_SIZE, static_cast<int>(hints.accumBlueBits),
                GLX_ACCUM_ALPHA_SIZE, static_cast<int>(hints.accumAlphaBits),
                GLX_SAMPLE_BUFFERS, hasMsaa,
                GLX_SAMPLES, msaaSamples,
                0
            };

            int fbCount = 0;
            GLXFBConfig* fbs = glXChooseFBConfig(dpy, screen, attrs, &fbCount);
            if (fbs && fbCount > 0) {
                XVisualInfo* vi = glXGetVisualFromFBConfig(dpy, fbs[0]);
                if (vi) {
                    cfg.surfaceHints.Set(NkSurfaceHintKey::NK_GLX_VISUAL_ID,
                                         static_cast<uintptr>(vi->visualid));
                    XFree(vi);
                }
                // FBConfig pointer lifetime is tied to this display/query.
                cfg.surfaceHints.Set(NkSurfaceHintKey::NK_GLX_FB_CONFIG_PTR, 0);
                XFree(fbs);
            }

            XCloseDisplay(dpy);
        #else
            (void)hints;
            (void)cfg;
        #endif
        }

        static bool CreateOpenGLContext(NkContext& context) {
            const NkSurfaceDesc& surface = context.surface;

        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            HDC hdc = GetDC(surface.hwnd);
            if (!hdc) {
                NkSetContextError(context, 1001, "GetDC failed.");
                return false;
            }

            const int pixelFormat = GetPixelFormat(hdc);
            if (pixelFormat == 0) {
                bool appliedSharedPixelFormat = false;
                const uintptr shareHwndHint = surface.appliedHints.Get(
                    NkSurfaceHintKey::NK_WGL_SHARE_PIXEL_FORMAT_HWND, 0);
                if (shareHwndHint != 0) {
                    HWND shareHwnd = reinterpret_cast<HWND>(shareHwndHint);
                    if (!IsWindow(shareHwnd)) {
                        ReleaseDC(surface.hwnd, hdc);
                        NkSetContextError(context, 1002, "Invalid share HWND for WGL pixel format.");
                        return false;
                    }

                    HDC shareDC = GetDC(shareHwnd);
                    if (!shareDC) {
                        ReleaseDC(surface.hwnd, hdc);
                        NkSetContextError(context, 1002, "GetDC failed for share HWND.");
                        return false;
                    }

                    const int sharePixelFormat = GetPixelFormat(shareDC);
                    PIXELFORMATDESCRIPTOR sharePfd = {};
                    sharePfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
                    sharePfd.nVersion = 1;
                    const bool canDescribe = (sharePixelFormat != 0) &&
                        (DescribePixelFormat(shareDC, sharePixelFormat,
                                             sizeof(PIXELFORMATDESCRIPTOR), &sharePfd) > 0);
                    ReleaseDC(shareHwnd, shareDC);

                    if (!canDescribe || !SetPixelFormat(hdc, sharePixelFormat, &sharePfd)) {
                        ReleaseDC(surface.hwnd, hdc);
                        NkSetContextError(context, 1002, "SetPixelFormat failed from shared HWND.");
                        return false;
                    }
                    appliedSharedPixelFormat = true;
                }

                if (!appliedSharedPixelFormat) {
                    PIXELFORMATDESCRIPTOR requested = NkBuildPixelFormatDescriptor(context.config);
                    PIXELFORMATDESCRIPTOR selected = requested;

                    int pf = 0;
                    if (context.config.win32PixelFormat.forcedPixelFormatIndex > 0) {
                        pf = context.config.win32PixelFormat.forcedPixelFormatIndex;
                        PIXELFORMATDESCRIPTOR described = {};
                        described.nSize = sizeof(PIXELFORMATDESCRIPTOR);
                        described.nVersion = 1;
                        if (DescribePixelFormat(hdc, pf, sizeof(PIXELFORMATDESCRIPTOR), &described) > 0) {
                            selected = described;
                        } else {
                            ReleaseDC(surface.hwnd, hdc);
                            NkSetContextError(context, 1002, "DescribePixelFormat failed for forced index.");
                            return false;
                        }
                    } else {
                        pf = ChoosePixelFormat(hdc, &requested);
                        if (pf != 0) {
                            PIXELFORMATDESCRIPTOR described = {};
                            described.nSize = sizeof(PIXELFORMATDESCRIPTOR);
                            described.nVersion = 1;
                            if (DescribePixelFormat(hdc, pf, sizeof(PIXELFORMATDESCRIPTOR), &described) > 0) {
                                selected = described;
                            }
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
            if (NkShouldRequestExplicitOpenGLContext(context.config)) {
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
                    NkSetContextError(context, 1005, "WGL_ARB_create_context unavailable for requested OpenGL version/profile.");
                    return false;
                }

                int attribs[16] = {};
                NkBuildWglContextAttribs(context.config, attribs);
                rc = createAttribs(hdc, nullptr, attribs);
                wglMakeCurrent(nullptr, nullptr);
                wglDeleteContext(bootstrap);
                if (!rc) {
                    ReleaseDC(surface.hwnd, hdc);
                    NkSetContextError(context, 1006, "wglCreateContextAttribsARB failed for requested OpenGL version/profile.");
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

            context.nativeDisplay = reinterpret_cast<void*>(surface.hinstance);
            context.nativeDeviceContext = hdc;
            context.nativeContext = rc;
            context.nativeDrawable = reinterpret_cast<uintptr>(surface.hwnd);
            context.getProcAddress = &NkWglProc;
            context.ownsNativeDisplay = false;
            return true;

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
                NkSetContextError(context, 1101, "XOpenDisplay failed for GLX context.");
                return false;
            }

            const int screen = DefaultScreen(dpy);
            const VisualID hinted = static_cast<VisualID>(
                surface.appliedHints.Get(NkSurfaceHintKey::NK_GLX_VISUAL_ID));
            GLXContext glx = nullptr;

            if (NkShouldRequestExplicitOpenGLContext(context.config)) {
                const char* extensions = glXQueryExtensionsString(dpy, screen);
                if (!HasOpenGLExtension(extensions, "GLX_ARB_create_context")) {
                    if (ownDisplay) XCloseDisplay(dpy);
                    NkSetContextError(context, 1104, "GLX_ARB_create_context unavailable for requested OpenGL version/profile.");
                    return false;
                }

                auto createAttribs = reinterpret_cast<NkGlxCreateContextAttribsProc>(
                    glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glXCreateContextAttribsARB")));
                if (!createAttribs) {
                    if (ownDisplay) XCloseDisplay(dpy);
                    NkSetContextError(context, 1105, "glXCreateContextAttribsARB symbol unavailable.");
                    return false;
                }

                GLXFBConfig fbConfig = ChooseGlxFBConfig(dpy, screen, context.config, hinted);
                if (!fbConfig) {
                    if (ownDisplay) XCloseDisplay(dpy);
                    NkSetContextError(context, 1106, "No GLXFBConfig matches requested surface/context attributes.");
                    return false;
                }

                int attribs[16] = {};
                BuildGlxContextAttribs(context.config, attribs);
                glx = createAttribs(dpy, fbConfig, nullptr, True, attribs);
                if (!glx) {
                    if (ownDisplay) XCloseDisplay(dpy);
                    NkSetContextError(context, 1107, "glXCreateContextAttribsARB failed for requested OpenGL version/profile.");
                    return false;
                }
            } else {
                XVisualInfo* vi = nullptr;
                if (hinted != 0) {
                    XVisualInfo tmpl = {};
                    tmpl.visualid = hinted;
                    int n = 0;
                    vi = XGetVisualInfo(dpy, VisualIDMask, &tmpl, &n);
                    if (!(vi && n > 0)) {
                        vi = nullptr;
                    }
                }

                if (!vi) {
                    const int attrs[] = {
                        GLX_RGBA,
                        GLX_RED_SIZE, static_cast<int>(NkClampU32Min(context.config.redBits, 1u)),
                        GLX_GREEN_SIZE, static_cast<int>(NkClampU32Min(context.config.greenBits, 1u)),
                        GLX_BLUE_SIZE, static_cast<int>(NkClampU32Min(context.config.blueBits, 1u)),
                        GLX_ALPHA_SIZE, static_cast<int>(context.config.alphaBits),
                        GLX_DOUBLEBUFFER, context.config.doubleBuffer ? True : False,
                        GLX_DEPTH_SIZE, static_cast<int>(context.config.depthBits),
                        GLX_STENCIL_SIZE, static_cast<int>(context.config.stencilBits),
                        GLX_ACCUM_RED_SIZE, static_cast<int>(context.config.accumRedBits),
                        GLX_ACCUM_GREEN_SIZE, static_cast<int>(context.config.accumGreenBits),
                        GLX_ACCUM_BLUE_SIZE, static_cast<int>(context.config.accumBlueBits),
                        GLX_ACCUM_ALPHA_SIZE, static_cast<int>(context.config.accumAlphaBits),
                        0
                    };
                    vi = glXChooseVisual(dpy, screen, const_cast<int*>(attrs));
                }

                if (!vi) {
                    if (ownDisplay) XCloseDisplay(dpy);
                    NkSetContextError(context, 1102, "No GLX visual available.");
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

            context.nativeDisplay = dpy;
            context.nativeContext = reinterpret_cast<void*>(glx);
            context.nativeDrawable = static_cast<uintptr>(surface.window);
            context.getProcAddress = &NkGlxProc;
            context.ownsNativeDisplay = ownDisplay;
            return true;

        #elif defined(NKENTSEU_WINDOWING_WAYLAND) && NKENTSEU_HAS_WAYLAND_EGL
            ::wl_display* wlDisplay = surface.display;
            ::wl_surface* wlSurface = surface.surface;
            if (!wlDisplay || !wlSurface) {
                NkSetContextError(context, 1151, "Wayland surface/display is invalid.");
                return false;
            }

            const uintptr eglDisplayHint = surface.appliedHints.Get(NkSurfaceHintKey::NK_EGL_DISPLAY, 0);
            const bool externalDisplay = (eglDisplayHint != 0);
            EGLDisplay eglDisplay = externalDisplay
                ? reinterpret_cast<EGLDisplay>(eglDisplayHint)
                : eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(wlDisplay));

            if (eglDisplay == EGL_NO_DISPLAY) {
                NkSetContextError(context, 1152, "eglGetDisplay failed for Wayland.");
                return false;
            }

            EGLint eglMajor = 0;
            EGLint eglMinor = 0;
            if (eglInitialize(eglDisplay, &eglMajor, &eglMinor) != EGL_TRUE) {
                NkSetContextError(context, 1153, "eglInitialize failed.");
                return false;
            }

            const EGLenum bindApi = IsEglEsProfile(context.config) ? EGL_OPENGL_ES_API : EGL_OPENGL_API;
            if (eglBindAPI(bindApi) != EGL_TRUE) {
                if (!externalDisplay) eglTerminate(eglDisplay);
                NkSetContextError(context, 1154, "eglBindAPI failed.");
                return false;
            }

            EGLConfig eglConfig = reinterpret_cast<EGLConfig>(
                surface.appliedHints.Get(NkSurfaceHintKey::NK_EGL_CONFIG, 0));
            if (!eglConfig && !ChooseWaylandEglConfig(eglDisplay, context.config, &eglConfig)) {
                if (!externalDisplay) eglTerminate(eglDisplay);
                NkSetContextError(context, 1155, "eglChooseConfig failed.");
                return false;
            }

            const int width = static_cast<int>(NkClampU32Min(surface.width, 1u));
            const int height = static_cast<int>(NkClampU32Min(surface.height, 1u));
            ::wl_egl_window* eglWindow = wl_egl_window_create(wlSurface, width, height);
            if (!eglWindow) {
                if (!externalDisplay) eglTerminate(eglDisplay);
                NkSetContextError(context, 1156, "wl_egl_window_create failed.");
                return false;
            }

            EGLSurface eglSurface = eglCreateWindowSurface(
                eglDisplay,
                eglConfig,
                reinterpret_cast<EGLNativeWindowType>(eglWindow),
                nullptr);
            if (eglSurface == EGL_NO_SURFACE) {
                wl_egl_window_destroy(eglWindow);
                if (!externalDisplay) eglTerminate(eglDisplay);
                NkSetContextError(context, 1157, "eglCreateWindowSurface failed.");
                return false;
            }

            EGLContext eglContext = EGL_NO_CONTEXT;
            if (IsEglEsProfile(context.config)) {
                const EGLint ctxAttrs[] = {
                    EGL_CONTEXT_CLIENT_VERSION, static_cast<EGLint>(NkClampU32Min(context.config.versionMajor, 2u)),
                    EGL_NONE
                };
                eglContext = eglCreateContext(eglDisplay, eglConfig, EGL_NO_CONTEXT, ctxAttrs);
            } else {
                eglContext = eglCreateContext(eglDisplay, eglConfig, EGL_NO_CONTEXT, nullptr);
            }

            if (eglContext == EGL_NO_CONTEXT) {
                eglDestroySurface(eglDisplay, eglSurface);
                wl_egl_window_destroy(eglWindow);
                if (!externalDisplay) eglTerminate(eglDisplay);
                NkSetContextError(context, 1158, "eglCreateContext failed.");
                return false;
            }

            context.surface.appliedHints.Set(
                NkSurfaceHintKey::NK_EGL_DISPLAY,
                reinterpret_cast<uintptr>(eglDisplay));
            context.surface.appliedHints.Set(
                NkSurfaceHintKey::NK_EGL_CONFIG,
                reinterpret_cast<uintptr>(eglConfig));

            context.nativeDisplay = eglDisplay;
            context.nativeContext = eglContext;
            context.nativeWindow = eglWindow;
            context.nativeDrawable = reinterpret_cast<uintptr>(eglSurface);
            context.getProcAddress = &NkEglProc;
            context.ownsNativeDisplay = !externalDisplay;
            return true;

        #elif defined(NKENTSEU_WINDOWING_WAYLAND)
            NkSetContextError(context, 1150, "Wayland EGL headers are missing (EGL/egl.h + wayland-egl.h).");
            return false;

        #else
            NkSetContextError(context, 1199, "OpenGL context creation unsupported on this platform.");
            return false;
        #endif
        }

    } // namespace

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
        gHints.api = graphics::NkGraphicsApi::NK_GFX_API_OPENGL;
        gHints.versionMajor = 3;
        gHints.versionMinor = 3;
        gHints.profile = NkContextProfile::NK_CONTEXT_PROFILE_CORE;
        gHints.debug = false;
        gHints.doubleBuffer = true;
        gHints.msaaSamples = 1;
        gHints.vsync = true;
        gHints.stereo = false;
        gHints.redBits = 8;
        gHints.greenBits = 8;
        gHints.blueBits = 8;
        gHints.alphaBits = 8;
        gHints.depthBits = 24;
        gHints.stencilBits = 8;
        gHints.accumRedBits = 0;
        gHints.accumGreenBits = 0;
        gHints.accumBlueBits = 0;
        gHints.accumAlphaBits = 0;
        gHints.auxBuffers = 0;
    }

    void NkContextSetHints(const NkContextConfig& config) {
        gHints = config;
    }

    void NkContextSetApi(graphics::NkGraphicsApi api) {
        gHints.api = api;
    }

    void NkContextSetWin32PixelFormat(const NkWin32PixelFormatConfig& config) {
        gHints.win32PixelFormat = config;
        // Keep generic OpenGL values aligned with the Win32 descriptor defaults.
        if (config.useCustomDescriptor) {
            gHints.doubleBuffer = config.doubleBuffer;
            gHints.stereo = config.stereo;
            gHints.redBits = config.redBits;
            gHints.greenBits = config.greenBits;
            gHints.blueBits = config.blueBits;
            gHints.alphaBits = config.alphaBits;
            gHints.depthBits = config.depthBits;
            gHints.stencilBits = config.stencilBits;
            gHints.accumRedBits = config.accumRedBits;
            gHints.accumGreenBits = config.accumGreenBits;
            gHints.accumBlueBits = config.accumBlueBits;
            gHints.accumAlphaBits = config.accumAlphaBits;
            gHints.auxBuffers = config.auxBuffers;
        }
    }

    void NkContextWindowHint(NkContextHint hint, int32 value) {
        switch (hint) {
            case NkContextHint::NK_CONTEXT_HINT_API:
                if (value >= static_cast<int32>(graphics::NkGraphicsApi::NK_GFX_API_NONE) &&
                    value <  static_cast<int32>(graphics::NkGraphicsApi::NK_GFX_API_MAX)) {
                    gHints.api = static_cast<graphics::NkGraphicsApi>(value);
                }
                break;
            case NkContextHint::NK_CONTEXT_HINT_VERSION_MAJOR:
                if (value > 0) gHints.versionMajor = static_cast<uint32>(value);
                break;
            case NkContextHint::NK_CONTEXT_HINT_VERSION_MINOR:
                if (value >= 0) gHints.versionMinor = static_cast<uint32>(value);
                break;
            case NkContextHint::NK_CONTEXT_HINT_PROFILE:
                if (value >= static_cast<int32>(NkContextProfile::NK_CONTEXT_PROFILE_ANY) &&
                    value <= static_cast<int32>(NkContextProfile::NK_CONTEXT_PROFILE_ES)) {
                    gHints.profile = static_cast<NkContextProfile>(value);
                }
                break;
            case NkContextHint::NK_CONTEXT_HINT_DEBUG:
                gHints.debug = (value != 0);
                break;
            case NkContextHint::NK_CONTEXT_HINT_DOUBLEBUFFER:
                gHints.doubleBuffer = (value != 0);
                if (gHints.win32PixelFormat.useCustomDescriptor) {
                    gHints.win32PixelFormat.doubleBuffer = gHints.doubleBuffer;
                }
                break;
            case NkContextHint::NK_CONTEXT_HINT_MSAA_SAMPLES:
                gHints.msaaSamples = value > 1 ? static_cast<uint32>(value) : 1u;
                break;
            case NkContextHint::NK_CONTEXT_HINT_VSYNC:
                gHints.vsync = (value != 0);
                break;
            case NkContextHint::NK_CONTEXT_HINT_RED_BITS:
                gHints.redBits = static_cast<uint32>(value > 0 ? value : 1);
                if (gHints.win32PixelFormat.useCustomDescriptor) {
                    gHints.win32PixelFormat.redBits = gHints.redBits;
                }
                break;
            case NkContextHint::NK_CONTEXT_HINT_GREEN_BITS:
                gHints.greenBits = static_cast<uint32>(value > 0 ? value : 1);
                if (gHints.win32PixelFormat.useCustomDescriptor) {
                    gHints.win32PixelFormat.greenBits = gHints.greenBits;
                }
                break;
            case NkContextHint::NK_CONTEXT_HINT_BLUE_BITS:
                gHints.blueBits = static_cast<uint32>(value > 0 ? value : 1);
                if (gHints.win32PixelFormat.useCustomDescriptor) {
                    gHints.win32PixelFormat.blueBits = gHints.blueBits;
                }
                break;
            case NkContextHint::NK_CONTEXT_HINT_ALPHA_BITS:
                gHints.alphaBits = static_cast<uint32>(value >= 0 ? value : 0);
                if (gHints.win32PixelFormat.useCustomDescriptor) {
                    gHints.win32PixelFormat.alphaBits = gHints.alphaBits;
                }
                break;
            case NkContextHint::NK_CONTEXT_HINT_DEPTH_BITS:
                gHints.depthBits = static_cast<uint32>(value >= 0 ? value : 0);
                if (gHints.win32PixelFormat.useCustomDescriptor) {
                    gHints.win32PixelFormat.depthBits = gHints.depthBits;
                }
                break;
            case NkContextHint::NK_CONTEXT_HINT_STENCIL_BITS:
                gHints.stencilBits = static_cast<uint32>(value >= 0 ? value : 0);
                if (gHints.win32PixelFormat.useCustomDescriptor) {
                    gHints.win32PixelFormat.stencilBits = gHints.stencilBits;
                }
                break;
            case NkContextHint::NK_CONTEXT_HINT_STEREO:
                gHints.stereo = (value != 0);
                if (gHints.win32PixelFormat.useCustomDescriptor) {
                    gHints.win32PixelFormat.stereo = gHints.stereo;
                }
                break;
            default:
                break;
        }
    }

    NkContextConfig NkContextGetHints() {
        return gHints;
    }

    void NkContextApplyWindowHints(NkWindowConfig& config) {
        if (gHints.api != graphics::NkGraphicsApi::NK_GFX_API_OPENGL) return;
        ApplyGlxVisualHint(gHints, config);
    }

    NkContextMode NkContextGetModeForApi(graphics::NkGraphicsApi api) {
        return NkIsSurfaceOnlyApi(api)
            ? NkContextMode::NK_CONTEXT_MODE_SURFACE_ONLY
            : NkContextMode::NK_CONTEXT_MODE_GRAPHICS_CONTEXT;
    }

    bool NkContextCreate(NkWindow& window, NkContext& outContext, const NkContextConfig* overrideConfig) {
        if (!gInit && !NkContextInit()) {
            return false;
        }
        if (!window.IsOpen()) {
            outContext = {};
            NkSetContextError(outContext, 1201, "NkContextCreate requires an open window.");
            return false;
        }

        outContext = {};
        outContext.config = overrideConfig ? *overrideConfig : gHints;
        outContext.surface = window.GetSurfaceDesc();
        outContext.mode = NkContextGetModeForApi(outContext.config.api);
        outContext.valid = false;
        NkClearContextError(outContext);

        if (!outContext.surface.IsValid()) {
            NkSetContextError(outContext, 1202, "Window surface is invalid.");
            return false;
        }

        if (outContext.mode == NkContextMode::NK_CONTEXT_MODE_SURFACE_ONLY) {
            outContext.valid = true;
            return true;
        }

        if (outContext.config.api != graphics::NkGraphicsApi::NK_GFX_API_OPENGL) {
            NkSetContextError(outContext, 1203, "Only OpenGL uses native context mode in NKWindow.");
            return false;
        }

        if (!CreateOpenGLContext(outContext)) {
            return false;
        }

        outContext.valid = true;
        return true;
    }

    void NkContextDestroy(NkContext& context) {
        if (!context.valid) {
            context = {};
            return;
        }

        if (context.mode == NkContextMode::NK_CONTEXT_MODE_GRAPHICS_CONTEXT &&
            context.config.api == graphics::NkGraphicsApi::NK_GFX_API_OPENGL) {
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            HDC hdc = static_cast<HDC>(context.nativeDeviceContext);
            HGLRC rc = static_cast<HGLRC>(context.nativeContext);
            if (rc) {
                wglMakeCurrent(nullptr, nullptr);
                wglDeleteContext(rc);
            }
            if (hdc && context.surface.hwnd) {
                ReleaseDC(context.surface.hwnd, hdc);
            }

        #elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
            Display* dpy = static_cast<Display*>(context.nativeDisplay);
            GLXContext glx = reinterpret_cast<GLXContext>(context.nativeContext);
            if (dpy && glx) {
                glXMakeCurrent(dpy, 0, nullptr);
                glXDestroyContext(dpy, glx);
            }
            if (context.ownsNativeDisplay && dpy) {
                XCloseDisplay(dpy);
            }

        #elif defined(NKENTSEU_WINDOWING_WAYLAND) && NKENTSEU_HAS_WAYLAND_EGL
            EGLDisplay eglDisplay = reinterpret_cast<EGLDisplay>(context.nativeDisplay);
            EGLContext eglContext = reinterpret_cast<EGLContext>(context.nativeContext);
            EGLSurface eglSurface = reinterpret_cast<EGLSurface>(context.nativeDrawable);
            ::wl_egl_window* eglWindow = static_cast<::wl_egl_window*>(context.nativeWindow);

            if (eglDisplay != EGL_NO_DISPLAY) {
                eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                if (eglContext != EGL_NO_CONTEXT) {
                    eglDestroyContext(eglDisplay, eglContext);
                }
                if (eglSurface != EGL_NO_SURFACE) {
                    eglDestroySurface(eglDisplay, eglSurface);
                }
                if (context.ownsNativeDisplay) {
                    eglTerminate(eglDisplay);
                }
            }

            if (eglWindow) {
                wl_egl_window_destroy(eglWindow);
            }
        #endif
        }

        context = {};
    }

    bool NkContextMakeCurrent(NkContext& context) {
        if (!context.valid) return false;
        if (context.mode == NkContextMode::NK_CONTEXT_MODE_SURFACE_ONLY) return true;

        if (context.config.api != graphics::NkGraphicsApi::NK_GFX_API_OPENGL) {
            NkSetContextError(context, 1301, "MakeCurrent is only available for OpenGL contexts.");
            return false;
        }

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        HDC hdc = static_cast<HDC>(context.nativeDeviceContext);
        HGLRC rc = static_cast<HGLRC>(context.nativeContext);
        if (!hdc || !rc || !wglMakeCurrent(hdc, rc)) {
            NkSetContextError(context, 1302, "wglMakeCurrent failed.");
            return false;
        }
        return true;

    #elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
        Display* dpy = static_cast<Display*>(context.nativeDisplay);
        GLXContext glx = reinterpret_cast<GLXContext>(context.nativeContext);
        GLXDrawable draw = static_cast<GLXDrawable>(context.nativeDrawable);
        if (!dpy || !glx || draw == 0 || !glXMakeCurrent(dpy, draw, glx)) {
            NkSetContextError(context, 1303, "glXMakeCurrent failed.");
            return false;
        }
        return true;

    #elif defined(NKENTSEU_WINDOWING_WAYLAND) && NKENTSEU_HAS_WAYLAND_EGL
        EGLDisplay eglDisplay = reinterpret_cast<EGLDisplay>(context.nativeDisplay);
        EGLContext eglContext = reinterpret_cast<EGLContext>(context.nativeContext);
        EGLSurface eglSurface = reinterpret_cast<EGLSurface>(context.nativeDrawable);
        if (eglDisplay == EGL_NO_DISPLAY || eglContext == EGL_NO_CONTEXT || eglSurface == EGL_NO_SURFACE) {
            NkSetContextError(context, 1304, "Wayland EGL context is incomplete.");
            return false;
        }

        ::wl_egl_window* eglWindow = static_cast<::wl_egl_window*>(context.nativeWindow);
        if (eglWindow) {
            wl_egl_window_resize(
                eglWindow,
                static_cast<int>(NkClampU32Min(context.surface.width, 1u)),
                static_cast<int>(NkClampU32Min(context.surface.height, 1u)),
                0,
                0);
        }

        if (eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext) != EGL_TRUE) {
            NkSetContextError(context, 1305, "eglMakeCurrent failed.");
            return false;
        }

        eglSwapInterval(eglDisplay, context.config.vsync ? 1 : 0);
        return true;

    #elif defined(NKENTSEU_WINDOWING_WAYLAND)
        NkSetContextError(context, 1306, "Wayland EGL headers are missing (EGL/egl.h + wayland-egl.h).");
        return false;

    #else
        NkSetContextError(context, 1399, "MakeCurrent unsupported on this platform.");
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
        if (dpy && context.nativeDrawable != 0) {
            glXSwapBuffers(dpy, static_cast<GLXDrawable>(context.nativeDrawable));
        }

    #elif defined(NKENTSEU_WINDOWING_WAYLAND) && NKENTSEU_HAS_WAYLAND_EGL
        EGLDisplay eglDisplay = reinterpret_cast<EGLDisplay>(context.nativeDisplay);
        EGLSurface eglSurface = reinterpret_cast<EGLSurface>(context.nativeDrawable);
        if (eglDisplay != EGL_NO_DISPLAY && eglSurface != EGL_NO_SURFACE) {
            eglSwapBuffers(eglDisplay, eglSurface);
        }
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
