#pragma once
// =============================================================================
// NkGraphicsApi.h
// NKRenderer side adapter over the canonical enum defined by NKWindow.
//
// Canonical enum:
//   NKWindow/Events/NkGraphicsEvent.h -> enum class NkGraphicsApi
//
// Compatibility:
//   Keeps helper functions used by NKRenderer and allows legacy NK_API_* names
//   via aliases declared in NkGraphicsEvent.h.
// =============================================================================

#include "NKWindow/Events/NkGraphicsEvent.h"

namespace nkentseu {

    inline const char* NkGraphicsApiName(NkGraphicsApi api) {
        return NkGraphicsApiToString(api);
    }

    inline bool NkGraphicsApiIsAvailable(NkGraphicsApi api) {
        switch (api) {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            case NkGraphicsApi::NK_API_OPENGL:    return true;
            case NkGraphicsApi::NK_API_VULKAN:    return true;
            case NkGraphicsApi::NK_API_DIRECTX11: return true;
            case NkGraphicsApi::NK_API_DIRECTX12: return true;
            case NkGraphicsApi::NK_API_SOFTWARE:  return true;
#elif defined(NKENTSEU_PLATFORM_MACOS)
            case NkGraphicsApi::NK_API_OPENGL:    return true; // deprecated but available
            case NkGraphicsApi::NK_API_VULKAN:    return true; // via MoltenVK
            case NkGraphicsApi::NK_API_METAL:     return true;
            case NkGraphicsApi::NK_API_SOFTWARE:  return true;
#elif defined(NKENTSEU_PLATFORM_IOS)
            case NkGraphicsApi::NK_API_OPENGLES:  return true;
            case NkGraphicsApi::NK_API_VULKAN:    return true; // via MoltenVK
            case NkGraphicsApi::NK_API_METAL:     return true;
            case NkGraphicsApi::NK_API_SOFTWARE:  return true;
#elif defined(NKENTSEU_PLATFORM_ANDROID)
            case NkGraphicsApi::NK_API_OPENGLES:  return true;
            case NkGraphicsApi::NK_API_VULKAN:    return true;
            case NkGraphicsApi::NK_API_SOFTWARE:  return true;
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
            case NkGraphicsApi::NK_API_WEBGL:     return true;
            case NkGraphicsApi::NK_API_OPENGLES:  return true; // via WebGL
            case NkGraphicsApi::NK_API_SOFTWARE:  return true;
#elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB) || defined(NKENTSEU_WINDOWING_WAYLAND)
            case NkGraphicsApi::NK_API_OPENGL:    return true;
            case NkGraphicsApi::NK_API_VULKAN:    return true;
            case NkGraphicsApi::NK_API_SOFTWARE:  return true;
#endif
            default: return false;
        }
    }

} // namespace nkentseu

