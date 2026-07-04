#pragma once

#include "NKRHI/Core/NkGraphicsApi.h"
#include "NKRHI/Core/NkContextDesc.h"
#include "NKWindow/Core/NkSurface.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)
struct IDXGIAdapter;
struct IDXGIAdapter1;
#endif

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(VK_USE_PLATFORM_WIN32_KHR)
#   define VK_USE_PLATFORM_WIN32_KHR
#endif

// Surfaces Vulkan Linux : le define VK_USE_PLATFORM_*_KHR DOIT précéder <vulkan.h>
// pour que les types (VkXlibSurfaceCreateInfoKHR…) + fonctions (vkCreateXlibSurfaceKHR)
// soient déclarés. On se base sur les defines command-line du jenga
// (NKENTSEU_FORCE_WINDOWING_*_ONLY, toujours dispo) + leur dérivé NKENTSEU_WINDOWING_*.
#if (defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_FORCE_WINDOWING_XLIB_ONLY)) && !defined(VK_USE_PLATFORM_XLIB_KHR)
#   define VK_USE_PLATFORM_XLIB_KHR
#endif
#if (defined(NKENTSEU_WINDOWING_XCB) || defined(NKENTSEU_FORCE_WINDOWING_XCB_ONLY)) && !defined(VK_USE_PLATFORM_XCB_KHR)
#   define VK_USE_PLATFORM_XCB_KHR
#endif
#if (defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY)) && !defined(VK_USE_PLATFORM_WAYLAND_KHR)
#   define VK_USE_PLATFORM_WAYLAND_KHR
#endif

#ifdef NK_RHI_VK_ENABLED
#include <vulkan/vulkan.h>
#endif

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
#   ifndef NK_NO_GLAD2
#       include <glad/glx.h>
#   else
#       include <GL/glx.h>
#   endif

#elif defined(NKENTSEU_WINDOWING_XCB)
#   include <xcb/xcb.h>
#   include <X11/Xlib.h>
#   include <X11/Xlib-xcb.h>
#   ifndef NK_NO_GLAD2
#       include <glad/glx.h>
#   else
#       include <GL/glx.h>
#   endif

#elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
#   ifndef NK_NO_GLAD2
#       include <glad/egl.h>
#   else
#       include <EGL/egl.h>
#       include <EGL/eglext.h>
#   endif

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
        // En TU non-ObjC, UIView est un alias objc_object partout ailleurs dans le
        // moteur (NkSurface.h, NkUIKitWindow.h) : « struct UIView; » créerait une
        // redéclaration « kind différent » si les deux en-têtes se croisent.
        struct EAGLContext; using UIView = struct objc_object;
#   endif

#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#   include <emscripten/html5.h>
#endif

namespace nkentseu {

    using NkGLGetProcAddressFn = void* (*)(const char*);
    using NkPresentCallback = NkFunction<void()>;
    using NkResizeCallback  = NkFunction<bool(uint32, uint32)>;

    struct NkDeviceInitInfo {
        NkGraphicsApi api = NkGraphicsApi::NK_GFX_API_NONE;
        NkContextDesc context{};
        NkSurfaceDesc surface{};

        uint32 width     = 0;
        uint32 height    = 0;
        bool   minimized = false;

        NkPresentCallback presentCallback{};
        NkResizeCallback  resizeCallback{};
    };

    inline NkGraphicsApi NkDeviceInitApi(const NkDeviceInitInfo& init) {
        if (init.api != NkGraphicsApi::NK_GFX_API_NONE) return init.api;
        return init.context.api;
    }

    inline uint32 NkDeviceInitWidth(const NkDeviceInitInfo& init) {
        return init.width > 0 ? init.width : init.surface.width;
    }

    inline uint32 NkDeviceInitHeight(const NkDeviceInitInfo& init) {
        return init.height > 0 ? init.height : init.surface.height;
    }

    inline bool NkDeviceInitComputeEnabledForApi(const NkDeviceInitInfo& init, NkGraphicsApi api) {
        if (!init.context.compute.enable) {
            // Backward-compatible default: compute remains enabled unless explicitly configured.
            return true;
        }
        return init.context.IsComputeEnabledForApi(api);
    }

} // namespace nkentseu
