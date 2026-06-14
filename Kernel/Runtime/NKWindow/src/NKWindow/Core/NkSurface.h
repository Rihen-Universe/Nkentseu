#pragma once
// =============================================================================
// NkSurface.h
// Descripteur de surface natif retourné par NkWindow::GetSurfaceDesc().
//
// Ce struct est le SEUL point de contact entre NkWindow et les backends
// graphiques. NkWindow ne sait pas qui le consomme.
//
// Suffisance par API :
//   OpenGL/WGL  (Windows)  : hwnd + hinstance          → GetDC(hwnd)
//   OpenGL/GLX  (XLib)     : display + window + screen + appliedHints
//   OpenGL/GLX  (XCB)      : connection + window + screen + appliedHints
//   OpenGL/EGL  (Wayland)  : display(wl) + surface(wl)
//   OpenGL/EGL  (Android)  : nativeWindow (ANativeWindow*)
//   OpenGL/EGL  (HarmonyOS): ohNativeWindow (OHNativeWindow*)
//                            → même API EGL que Android, format RGBA8888
//   OpenGL/WebGL(WASM)     : canvasId
//   Vulkan                 : tous les handles natifs suffisent
//   Metal                  : view + metalLayer
//   DirectX 11/12          : hwnd + hinstance
//   Software               : tous (pixel buffer géré par le contexte)
// =============================================================================

#include "NkTypes.h"
#include "NkSurfaceHint.h"
#include "NKPlatform/NkPlatformDetect.h"

// Inclusions natives conditionnelles
#if defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY)
    // rien

#elif defined(NKENTSEU_PLATFORM_UWP) || defined(NKENTSEU_PLATFORM_XBOX)
    // handle opaque

#elif defined(NKENTSEU_PLATFORM_WINDOWS)
#   ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#   endif
#   include <windows.h>

#elif defined(NKENTSEU_PLATFORM_MACOS)
#   ifdef __OBJC__
@class NSView;
@class CAMetalLayer;
#   else
    using NSView       = struct objc_object;
    using CAMetalLayer = struct objc_object;
#   endif

#elif defined(NKENTSEU_PLATFORM_IOS)
#   ifdef __OBJC__
@class UIView;
@class CAMetalLayer;
#   else
    using UIView       = struct objc_object;
    using CAMetalLayer = struct objc_object;
#   endif

#elif defined(NKENTSEU_WINDOWING_XCB)
#   include <xcb/xcb.h>

#elif defined(NKENTSEU_WINDOWING_XLIB)
#   include <X11/Xlib.h>

#elif defined(NKENTSEU_WINDOWING_WAYLAND)
    #include <wayland-client.h>
    #include <wayland-cursor.h>
    #include <xkbcommon/xkbcommon.h>
    struct wl_display;
    struct wl_surface;
    struct wl_buffer;

#elif defined(NKENTSEU_PLATFORM_ANDROID)
#   include <android/native_window.h>

#elif defined(NKENTSEU_PLATFORM_HARMONYOS)
    // HarmonyOS : OHNativeWindow est le handle de surface native.
    // Il est fourni par OH_NativeXComponent_Callback::OnSurfaceCreated()
    // et passé au backend graphique (Vulkan, OpenGL ES, Software).
    //
    // Note : l'include exact dépend du NDK OHOS installé.
    // Si external_window.h n'est pas disponible, on utilise void*.
#   if __has_include(<native_window/external_window.h>)
#       include <native_window/external_window.h>
#   else
        // Forward declaration minimaliste si l'include n'est pas dispo
        struct OHNativeWindow;
#   endif
#endif

namespace nkentseu {

    // ---------------------------------------------------------------------------
    // NkSurfaceDesc
    // ---------------------------------------------------------------------------
    struct NkSurfaceDesc {

        // --- Dimensions physiques (pixels) ---
        uint32  width  = 0;
        uint32  height = 0;
        float   dpi    = 1.f;

        // --- Handles natifs (conditionnel par plateforme) ---

    #if defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY)
        void* dummy = nullptr;

    #elif defined(NKENTSEU_PLATFORM_UWP) || defined(NKENTSEU_PLATFORM_XBOX)
        void* nativeWindow = nullptr;

    #elif defined(NKENTSEU_PLATFORM_WINDOWS)
        HWND      hwnd      = nullptr;
        HINSTANCE hinstance = nullptr;
        // HDC délibérément absent : il est éphémère.

    #elif defined(NKENTSEU_PLATFORM_MACOS)
        NSView*       view       = nullptr;
        CAMetalLayer* metalLayer = nullptr;

    #elif defined(NKENTSEU_PLATFORM_IOS)
        UIView*       view       = nullptr;
        CAMetalLayer* metalLayer = nullptr;

    #elif defined(NKENTSEU_WINDOWING_XCB)
        xcb_connection_t* connection = nullptr;
        xcb_window_t      window     = 0;
        xcb_screen_t*     screen     = nullptr;

    #elif defined(NKENTSEU_WINDOWING_XLIB)
        Display* display = nullptr;
        ::Window window  = 0;
        int      screen  = 0;

    #elif defined(NKENTSEU_WINDOWING_WAYLAND)
        ::wl_display* display          = nullptr;
        ::wl_surface* surface          = nullptr;
        bool          waylandConfigured= false;
        void*         shmPixels        = nullptr;
        ::wl_buffer*  shmBuffer        = nullptr;
        uint32_t      shmStride        = 0;

    #elif defined(NKENTSEU_PLATFORM_ANDROID)
        ANativeWindow* nativeWindow = nullptr;

    #elif defined(NKENTSEU_PLATFORM_HARMONYOS)
        // HarmonyOS : handle de surface natif fourni par XComponent.
        //
        // Pipeline de création :
        //   1. ArkTS : XComponent(type='surface') → onSurfaceCreated callback
        //   2. NkHarmonyBridge.ts → appelle C++ via N-API
        //   3. NkHarmonyOnSurfaceCreated(component, window) → stocke window
        //   4. NkHarmonyWindow remplit cette struct et appelle OnResize()
        //
        // Backends qui utilisent ohNativeWindow :
        //   Vulkan     → VkSurfaceKHR via VK_OHOS_surface (si dispo)
        //                ou EGL + vkGetPhysicalDeviceSurfaceSupportKHR
        //   OpenGL ES  → EGLSurface via eglCreateWindowSurface()
        //   Software   → OH_NativeWindow_NativeWindowRequestBuffer()
        //                ou callback bridge ArkTS (presentCallback)
        OHNativeWindow* ohNativeWindow  = nullptr;

        // Callback de présentation pour le backend Software (optionnel).
        // Signature : void(*)(const uint8* pixels, uint32 w, uint32 h)
        // Fourni par NkHarmonyBridge si OHNativeWindow n'est pas utilisable
        // directement (ex: émulateur DevEco Studio sans accès aux buffers).
        void*           presentCallback = nullptr;

        // Identifiant du XComponent ArkTS (pour debug et routing multi-surface)
        const char*     xComponentId    = nullptr;

    #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        const char* canvasId = "#canvas";

    #else
        void* dummy = nullptr;
    #endif

        // --- Hints appliqués pendant la création de la fenêtre ---
        NkSurfaceHints appliedHints;

        // --- Validation ---
        bool IsValid() const {
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            return hwnd != nullptr && width > 0 && height > 0;
        #elif defined(NKENTSEU_WINDOWING_XLIB)
            return display != nullptr && window != 0 && width > 0;
        #elif defined(NKENTSEU_WINDOWING_XCB)
            return connection != nullptr && window != 0 && width > 0;
        #elif defined(NKENTSEU_WINDOWING_WAYLAND)
            return display != nullptr && surface != nullptr && width > 0;
        #elif defined(NKENTSEU_PLATFORM_ANDROID)
            return nativeWindow != nullptr && width > 0;
        #elif defined(NKENTSEU_PLATFORM_HARMONYOS)
            // La surface peut être valide même sans ohNativeWindow si
            // le callback bridge est fourni (présentation via ArkTS).
            return (ohNativeWindow != nullptr || presentCallback != nullptr)
                && width > 0 && height > 0;
        #elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
            return view != nullptr && width > 0;
        #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
            return canvasId != nullptr && width > 0;
        #else
            return width > 0 && height > 0;
        #endif
        }
    };

} // namespace nkentseu