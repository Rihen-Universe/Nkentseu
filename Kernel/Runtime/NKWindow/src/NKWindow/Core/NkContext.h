#pragma once

// =============================================================================
// NkContext.h
// API alternative style GLFW/SDL pour gérer le contexte graphique depuis NKWindow.
//
// Flux recommandé :
//   NkContextInit();
//   NkContextWindowHint(...);
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

    enum class NkContextProfile : uint32 {
        NK_CONTEXT_PROFILE_ANY = 0,
        NK_CONTEXT_PROFILE_CORE,
        NK_CONTEXT_PROFILE_COMPATIBILITY,
        NK_CONTEXT_PROFILE_ES,
    };

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

    enum class NkContextMode : uint32 {
        NK_CONTEXT_MODE_SURFACE_ONLY = 0,      // Vulkan / DX / Metal / Software
        NK_CONTEXT_MODE_GRAPHICS_CONTEXT = 1,  // OpenGL style context
    };

    using NkContextProc = void* (*)(const char*);

    // Win32-only pixel format control (WGL path).
    // Ignored on non-Windows platforms.
    struct NkWin32PixelFormatConfig {
        // If true, descriptor fields below are used directly.
        // If false, descriptor is derived from generic NkContextConfig fields.
        bool   useCustomDescriptor = false;

        bool   drawToWindow       = true;
        bool   supportOpenGL      = true;
        bool   doubleBuffer       = true;
        bool   stereo             = false;
        bool   supportGDI         = false;
        bool   genericFormat      = false;
        bool   genericAccelerated = false;

        // 0 = RGBA, 1 = color-index.
        uint32 pixelType = 0;

        uint32 colorBits      = 32;
        uint32 redBits        = 8;
        uint32 redShift       = 0;
        uint32 greenBits      = 8;
        uint32 greenShift     = 0;
        uint32 blueBits       = 8;
        uint32 blueShift      = 0;
        uint32 alphaBits      = 8;
        uint32 alphaShift     = 0;
        uint32 accumBits      = 0;
        uint32 accumRedBits   = 0;
        uint32 accumGreenBits = 0;
        uint32 accumBlueBits  = 0;
        uint32 accumAlphaBits = 0;
        uint32 depthBits      = 24;
        uint32 stencilBits    = 8;
        uint32 auxBuffers     = 0;

        // 0 = main plane, 1 = overlay, -1 = underlay.
        int32  layerType   = 0;
        uint32 layerMask   = 0;
        uint32 visibleMask = 0;
        uint32 damageMask  = 0;

        // If > 0, bypass ChoosePixelFormat and enforce this index.
        int32  forcedPixelFormatIndex = 0;
    };

    struct NkContextConfig {
        graphics::NkGraphicsApi  api  = graphics::NkGraphicsApi::NK_GFX_API_OPENGL;
        uint32           versionMajor = 3;
        uint32           versionMinor = 3;
        NkContextProfile profile      = NkContextProfile::NK_CONTEXT_PROFILE_CORE;

        bool             debug        = false;
        bool             doubleBuffer = true;
        uint32           msaaSamples  = 1;
        bool             vsync        = true;
        bool             stereo       = false;

        uint32           redBits      = 8;
        uint32           greenBits    = 8;
        uint32           blueBits     = 8;
        uint32           alphaBits    = 8;
        uint32           depthBits    = 24;
        uint32           stencilBits  = 8;
        uint32           accumRedBits   = 0;
        uint32           accumGreenBits = 0;
        uint32           accumBlueBits  = 0;
        uint32           accumAlphaBits = 0;
        uint32           auxBuffers   = 0;

        // Win32-only override for PIXELFORMATDESCRIPTOR.
        NkWin32PixelFormatConfig win32PixelFormat{};
    };

    struct NkContext {
        NkContextConfig config{};
        NkContextMode   mode  = NkContextMode::NK_CONTEXT_MODE_SURFACE_ONLY;
        NkSurfaceDesc   surface{};
        NkError         lastError{};
        bool            valid = false;

        // Opaque natives for advanced usage
        void*           nativeDisplay = nullptr;
        void*           nativeContext = nullptr;
        void*           nativeWindow = nullptr;
        uintptr         nativeDrawable = 0;
        bool            ownsNativeDisplay = false;

        NkContextProc   getProcAddress = nullptr;

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        void*           nativeDeviceContext = nullptr; // HDC
    #endif
    };

    // ---- GLFW/SDL-like lifecycle ------------------------------------------------

    bool NkContextInit();
    void NkContextShutdown();

    // ---- Global hints (window-context style) -----------------------------------

    void            NkContextResetHints();
    void            NkContextSetHints(const NkContextConfig& config);
    void            NkContextSetApi(graphics::NkGraphicsApi api);
    void            NkContextSetWin32PixelFormat(const NkWin32PixelFormatConfig& config);
    void            NkContextWindowHint(NkContextHint hint, int32 value);
    NkContextConfig NkContextGetHints();

    // Apply hints into NkWindowConfig before NkWindow::Create()
    void NkContextApplyWindowHints(NkWindowConfig& config);

    // ---- Per-window context -----------------------------------------------------

    NkContextMode NkContextGetModeForApi(graphics::NkGraphicsApi api);

    bool NkContextCreate(NkWindow& window, NkContext& outContext,
                         const NkContextConfig* overrideConfig = nullptr);
    void NkContextDestroy(NkContext& context);

    bool NkContextMakeCurrent(NkContext& context);
    void NkContextSwapBuffers(NkContext& context);

    // Returns the active context proc loader callback if available.
    NkContextProc NkContextGetProcAddressLoader(const NkContext& context);

    // Convenience helper: directly load one symbol.
    void* NkContextGetProcAddress(NkContext& context, const char* procName);

} // namespace nkentseu
