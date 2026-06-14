#pragma once

// =============================================================================
// NkContext.h
// API style GLFW/SDL pour gérer le contexte graphique depuis NKWindow.
//
// Plateformes supportées :
//   Windows          → WGL (OpenGL natif)
//   Linux XLib/XCB   → GLX (OpenGL natif)
//   Wayland          → EGL (OpenGL ES ou OpenGL)
//   Android          → EGL (OpenGL ES uniquement)
//   HarmonyOS        → EGL (OpenGL ES uniquement, via OHNativeWindow)
//   macOS            → NSOpenGLContext / CGL (OpenGL legacy) ou Metal (surface-only)
//   iOS/tvOS/watchOS → EAGLContext (OpenGL ES 2/3) ou Metal (surface-only)
//   WebAssembly      → WebGL via Emscripten (OpenGL ES 2/3 émulé)
//
// APIs "surface-only" (Vulkan, DX11/12, Metal, Software) :
//   → NkContextCreate réussit toujours, mode NK_CONTEXT_MODE_SURFACE_ONLY
//   → Aucun contexte natif n'est créé — le backend RHI gère tout lui-même
//
// Flux recommandé :
//   NkContextInit();
//   NkContextWindowHint(NK_CONTEXT_HINT_API, NK_GFX_API_OPENGL);
//   NkContextApplyWindowHints(windowCfg);   // avant window.Create
//   window.Create(windowCfg);
//   NkContext ctx;
//   NkContextCreate(window, ctx);           // après Create
//   NkContextMakeCurrent(ctx);
//   gladLoadGLLoader(NkContextGetProcAddress);
// =============================================================================

#include "NkTypes.h"
#include "NkWindowConfig.h"
#include "NkSurface.h"

namespace nkentseu {

    class NkWindow;

    // ── Profil de contexte OpenGL ─────────────────────────────────────────────
    enum class NkContextProfile : uint32 {
        NK_CONTEXT_PROFILE_ANY = 0,
        NK_CONTEXT_PROFILE_CORE,
        NK_CONTEXT_PROFILE_COMPATIBILITY,
        NK_CONTEXT_PROFILE_ES,          // OpenGL ES (Android, HarmonyOS, iOS, Web)
    };

    // ── Hints de configuration ────────────────────────────────────────────────
    enum class NkContextHint : uint32 {
        NK_CONTEXT_HINT_API = 0,
        NK_CONTEXT_HINT_VERSION_MAJOR,
        NK_CONTEXT_HINT_VERSION_MINOR,
        NK_CONTEXT_HINT_PROFILE,
        NK_CONTEXT_HINT_DEBUG,
        NK_CONTEXT_HINT_DOUBLEBUFFER,
        NK_CONTEXT_HINT_MSAA_SAMPLES,
        NK_CONTEXT_HINT_VSYNC,
        NK_CONTEXT_HINT_RED_BITS,
        NK_CONTEXT_HINT_GREEN_BITS,
        NK_CONTEXT_HINT_BLUE_BITS,
        NK_CONTEXT_HINT_ALPHA_BITS,
        NK_CONTEXT_HINT_DEPTH_BITS,
        NK_CONTEXT_HINT_STENCIL_BITS,
        NK_CONTEXT_HINT_STEREO,
    };

    // ── Mode de contexte ──────────────────────────────────────────────────────
    enum class NkContextMode : uint32 {
        NK_CONTEXT_MODE_SURFACE_ONLY = 0,      // Vulkan / DX / Metal / Software
        NK_CONTEXT_MODE_GRAPHICS_CONTEXT = 1,  // OpenGL style context
    };

    using NkContextProc = void* (*)(const char*);

    // ── Pixel format Win32 (WGL uniquement) ───────────────────────────────────
    struct NkWin32PixelFormatConfig {
        bool   useCustomDescriptor = false;
        bool   drawToWindow       = true;
        bool   supportOpenGL      = true;
        bool   doubleBuffer       = true;
        bool   stereo             = false;
        bool   supportGDI         = false;
        bool   genericFormat      = false;
        bool   genericAccelerated = false;
        uint32 pixelType          = 0;        // 0 = RGBA, 1 = color-index
        uint32 colorBits          = 32;
        uint32 redBits            = 8;
        uint32 redShift           = 0;
        uint32 greenBits          = 8;
        uint32 greenShift         = 0;
        uint32 blueBits           = 8;
        uint32 blueShift          = 0;
        uint32 alphaBits          = 8;
        uint32 alphaShift         = 0;
        uint32 accumBits          = 0;
        uint32 accumRedBits       = 0;
        uint32 accumGreenBits     = 0;
        uint32 accumBlueBits      = 0;
        uint32 accumAlphaBits     = 0;
        uint32 depthBits          = 24;
        uint32 stencilBits        = 8;
        uint32 auxBuffers         = 0;
        int32  layerType          = 0;        // 0 = main, 1 = overlay, -1 = underlay
        uint32 layerMask          = 0;
        uint32 visibleMask        = 0;
        uint32 damageMask         = 0;
        int32  forcedPixelFormatIndex = 0;    // 0 = auto
    };

    // ── Configuration complète du contexte ───────────────────────────────────
    struct NkContextConfig {
        graphics::NkGraphicsApi  api          = graphics::NkGraphicsApi::NK_GFX_API_OPENGL;
        uint32                   versionMajor = 3;
        uint32                   versionMinor = 3;
        NkContextProfile         profile      = NkContextProfile::NK_CONTEXT_PROFILE_CORE;

        bool   debug        = false;
        bool   doubleBuffer = true;
        uint32 msaaSamples  = 1;
        bool   vsync        = true;
        bool   stereo       = false;

        // Canaux couleur / profondeur / stencil
        uint32 redBits      = 8;
        uint32 greenBits    = 8;
        uint32 blueBits     = 8;
        uint32 alphaBits    = 8;
        uint32 depthBits    = 24;
        uint32 stencilBits  = 8;
        uint32 accumRedBits   = 0;
        uint32 accumGreenBits = 0;
        uint32 accumBlueBits  = 0;
        uint32 accumAlphaBits = 0;
        uint32 auxBuffers     = 0;

        // Win32 uniquement
        NkWin32PixelFormatConfig win32PixelFormat{};
    };

    // ── Contexte graphique ───────────────────────────────────────────────────
    struct NkContext {
        NkContextConfig config{};
        NkContextMode   mode    = NkContextMode::NK_CONTEXT_MODE_SURFACE_ONLY;
        NkSurfaceDesc   surface{};
        NkError         lastError{};
        bool            valid   = false;

        // Handles natifs opaques (usage avancé)
        void*           nativeDisplay         = nullptr;   // HDC (Win), Display* (X11), EGLDisplay (EGL)
        void*           nativeContext         = nullptr;   // HGLRC, GLXContext, EGLContext, NSOpenGLContext*
        void*           nativeWindow          = nullptr;   // wl_egl_window* (Wayland), EAGLContext* (iOS)
        uintptr         nativeDrawable        = 0;         // GLXDrawable, EGLSurface, HWND
        bool            ownsNativeDisplay     = false;

        NkContextProc   getProcAddress        = nullptr;

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        void*           nativeDeviceContext   = nullptr;   // HDC
    #endif
    };

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    bool NkContextInit();
    void NkContextShutdown();

    // ── Hints globaux ─────────────────────────────────────────────────────────
    void            NkContextResetHints();
    void            NkContextSetHints(const NkContextConfig& config);
    void            NkContextSetApi(graphics::NkGraphicsApi api);
    void            NkContextSetWin32PixelFormat(const NkWin32PixelFormatConfig& config);
    void            NkContextWindowHint(NkContextHint hint, int32 value);
    NkContextConfig NkContextGetHints();

    // Applique les hints dans NkWindowConfig avant NkWindow::Create()
    void            NkContextApplyWindowHints(NkWindowConfig& config);

    // ── Contexte par fenêtre ──────────────────────────────────────────────────
    NkContextMode   NkContextGetModeForApi(graphics::NkGraphicsApi api);

    bool NkContextCreate(NkWindow& window, NkContext& outContext,
                         const NkContextConfig* overrideConfig = nullptr);
    void NkContextDestroy(NkContext& context);

    bool NkContextMakeCurrent(NkContext& context);
    void NkContextSwapBuffers(NkContext& context);

    NkContextProc NkContextGetProcAddressLoader(const NkContext& context);
    void*         NkContextGetProcAddress(NkContext& context, const char* procName);

} // namespace nkentseu 