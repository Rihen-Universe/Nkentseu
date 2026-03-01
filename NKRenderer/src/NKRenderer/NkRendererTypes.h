#pragma once
// =============================================================================
// NkRendererTypes.h
// Types, enums et structures communs a tous les backends de rendu.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NkPlatformDetect.h"

#if defined(NKENTSEU_FAMILY_WINDOWS)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#elif defined(NKENTSEU_PLATFORM_COCOA)
#  ifdef __OBJC__
@class NSView;
@class CAMetalLayer;
#  else
using NSView       = struct objc_object;
using CAMetalLayer = struct objc_object;
#  endif
#elif defined(NKENTSEU_PLATFORM_UIKIT)
#  ifdef __OBJC__
@class UIView;
@class CAMetalLayer;
#  else
using UIView       = struct objc_object;
using CAMetalLayer = struct objc_object;
#  endif
#elif defined(NKENTSEU_PLATFORM_XCB)
#  include <xcb/xcb.h>
#elif defined(NKENTSEU_PLATFORM_XLIB)
#  include <X11/Xlib.h>
#elif defined(NKENTSEU_PLATFORM_ANDROID)
#  include <android/native_window.h>
#endif

namespace nkentseu {

enum class NkRendererApi : NkU32 {
    NK_NONE      = 0,
    NK_SOFTWARE,
    NK_OPENGL,
    NK_VULKAN,
    NK_DIRECTX11,
    NK_DIRECTX12,
    NK_METAL,
    NK_RENDERER_API_MAX
};

inline const char* NkRendererApiToString(NkRendererApi api) noexcept {
    switch (api) {
        case NkRendererApi::NK_SOFTWARE:  return "Software";
        case NkRendererApi::NK_OPENGL:    return "OpenGL";
        case NkRendererApi::NK_VULKAN:    return "Vulkan";
        case NkRendererApi::NK_DIRECTX11: return "DirectX 11";
        case NkRendererApi::NK_DIRECTX12: return "DirectX 12";
        case NkRendererApi::NK_METAL:     return "Metal";
        default:                          return "None";
    }
}

enum class NkPixelFormat : NkU32 {
    NK_PIXEL_UNKNOWN             = 0,
    NK_PIXEL_R8G8B8A8_UNORM,
    NK_PIXEL_B8G8R8A8_UNORM,
    NK_PIXEL_R8G8B8A8_SRGB,
    NK_PIXEL_B8G8R8A8_SRGB,
    NK_PIXEL_R16G16B16A16_FLOAT,
    NK_PIXEL_D24_UNORM_S8_UINT,
    NK_PIXEL_D32_FLOAT,
    NK_PIXEL_FORMAT_MAX
};

struct NkSurfaceDesc {
    NkU32 width  = 0;
    NkU32 height = 0;
    float dpi    = 1.f;
#if defined(NKENTSEU_FAMILY_WINDOWS)
    HWND      hwnd      = nullptr;
    HINSTANCE hinstance = nullptr;
#elif defined(NKENTSEU_PLATFORM_COCOA)
    NSView*       view       = nullptr;
    CAMetalLayer* metalLayer = nullptr;
#elif defined(NKENTSEU_PLATFORM_UIKIT)
    UIView*       view       = nullptr;
    CAMetalLayer* metalLayer = nullptr;
#elif defined(NKENTSEU_PLATFORM_XCB)
    xcb_connection_t* connection = nullptr;
    xcb_window_t      window     = 0;
#elif defined(NKENTSEU_PLATFORM_XLIB)
    Display* display = nullptr;
    ::Window window  = 0;
#elif defined(NKENTSEU_PLATFORM_ANDROID)
    ANativeWindow* nativeWindow = nullptr;
#elif defined(NKENTSEU_PLATFORM_WASM)
    const char* canvasId = "#canvas";
#else
    void* dummy = nullptr;
#endif
};

struct NkFramebufferInfo {
    NkU32 width  = 0;
    NkU32 height = 0;
    NkU32 pitch  = 0;
    NkU8* pixels = nullptr;
};

struct NkRendererConfig {
    NkRendererApi api                   = NkRendererApi::NK_SOFTWARE;
    NkPixelFormat colorFormat           = NkPixelFormat::NK_PIXEL_R8G8B8A8_UNORM;
    NkPixelFormat depthFormat           = NkPixelFormat::NK_PIXEL_D24_UNORM_S8_UINT;
    NkU32         sampleCount           = 1;
    bool          vsync                 = true;
    bool          debug                 = false;
    bool          autoResizeFramebuffer = true;
};

struct NkSoftwareRendererContext { NkFramebufferInfo framebuffer{}; };
struct NkOpenGLRendererContext   {
    void* nativeDisplay = nullptr;
    void* nativeWindow  = nullptr;
    void* context       = nullptr;
};
struct NkVulkanRendererContext {
    void* instance       = nullptr;
    void* physicalDevice = nullptr;
    void* device         = nullptr;
    void* queue          = nullptr;
    void* surface        = nullptr;
    void* nativeDisplay  = nullptr;
    void* nativeWindow   = nullptr;
};
struct NkMetalRendererContext {
    void* device       = nullptr;
    void* commandQueue = nullptr;
    void* layer        = nullptr;
    void* drawable     = nullptr;
    void* view         = nullptr;
};
struct NkDirectX11RendererContext {
    void* device            = nullptr;
    void* deviceContext     = nullptr;
    void* swapChain         = nullptr;
    void* renderTargetView  = nullptr;
    void* nativeWindow      = nullptr;
};
struct NkDirectX12RendererContext {
    void* device       = nullptr;
    void* commandQueue = nullptr;
    void* swapChain    = nullptr;
    void* nativeWindow = nullptr;
};

struct NkRendererContext {
    NkRendererApi              api      = NkRendererApi::NK_NONE;
    NkSurfaceDesc              surface  {};
    NkSoftwareRendererContext  software {};
    NkOpenGLRendererContext    opengl   {};
    NkVulkanRendererContext    vulkan   {};
    NkMetalRendererContext     metal    {};
    NkDirectX11RendererContext dx11     {};
    NkDirectX12RendererContext dx12     {};
    void*                      userData = nullptr;
};

inline void* NkGetNativeWindowHandle(const NkSurfaceDesc& surface) noexcept {
#if defined(NKENTSEU_FAMILY_WINDOWS)
    return reinterpret_cast<void*>(surface.hwnd);
#elif defined(NKENTSEU_PLATFORM_COCOA) || defined(NKENTSEU_PLATFORM_UIKIT)
    return static_cast<void*>(surface.view);
#elif defined(NKENTSEU_PLATFORM_XCB)
    return reinterpret_cast<void*>(static_cast<uintptr_t>(surface.window));
#elif defined(NKENTSEU_PLATFORM_XLIB)
    return reinterpret_cast<void*>(static_cast<uintptr_t>(surface.window));
#elif defined(NKENTSEU_PLATFORM_ANDROID)
    return static_cast<void*>(surface.nativeWindow);
#elif defined(NKENTSEU_PLATFORM_WASM)
    return const_cast<char*>(surface.canvasId ? surface.canvasId : "#canvas");
#else
    return surface.dummy;
#endif
}

inline void* NkGetNativeDisplayHandle(const NkSurfaceDesc& surface) noexcept {
#if defined(NKENTSEU_FAMILY_WINDOWS)
    return reinterpret_cast<void*>(surface.hinstance);
#elif defined(NKENTSEU_PLATFORM_COCOA) || defined(NKENTSEU_PLATFORM_UIKIT)
    return static_cast<void*>(surface.view);
#elif defined(NKENTSEU_PLATFORM_XCB)
    return static_cast<void*>(surface.connection);
#elif defined(NKENTSEU_PLATFORM_XLIB)
    return static_cast<void*>(surface.display);
#elif defined(NKENTSEU_PLATFORM_ANDROID)
    return static_cast<void*>(surface.nativeWindow);
#elif defined(NKENTSEU_PLATFORM_WASM)
    return const_cast<char*>(surface.canvasId ? surface.canvasId : "#canvas");
#else
    return surface.dummy;
#endif
}

} // namespace nkentseu
