#pragma once
// NkGraphicsApi.h
#include "NkTypes.h"

namespace nkentseu {

    enum class NkGraphicsApi : uint32 {
        NK_API_NONE       = 0,
        NK_API_OPENGL,
        NK_API_OPENGLES,
        NK_API_WEBGL,
        NK_API_VULKAN,
        NK_API_DIRECTX11,
        NK_API_DIRECTX12,
        NK_API_METAL,
        NK_API_SOFTWARE,
        NK_API_COUNT
    };

    inline const char* NkGraphicsApiName(NkGraphicsApi api) {
        switch (api) {
            case NkGraphicsApi::NK_API_OPENGL:    return "OpenGL";
            case NkGraphicsApi::NK_API_OPENGLES:  return "OpenGL ES";
            case NkGraphicsApi::NK_API_WEBGL:     return "WebGL";
            case NkGraphicsApi::NK_API_VULKAN:    return "Vulkan";
            case NkGraphicsApi::NK_API_DIRECTX11: return "DirectX 11";
            case NkGraphicsApi::NK_API_DIRECTX12: return "DirectX 12";
            case NkGraphicsApi::NK_API_METAL:     return "Metal";
            case NkGraphicsApi::NK_API_SOFTWARE:  return "Software";
            default:                               return "None";
        }
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
            case NkGraphicsApi::NK_API_OPENGL:    return true;
            case NkGraphicsApi::NK_API_VULKAN:    return true;
            case NkGraphicsApi::NK_API_METAL:     return true;
            case NkGraphicsApi::NK_API_SOFTWARE:  return true;
#elif defined(NKENTSEU_PLATFORM_IOS)
            case NkGraphicsApi::NK_API_OPENGLES:  return true;
            case NkGraphicsApi::NK_API_VULKAN:    return true;
            case NkGraphicsApi::NK_API_METAL:     return true;
            case NkGraphicsApi::NK_API_SOFTWARE:  return true;
#elif defined(NKENTSEU_PLATFORM_ANDROID)
            case NkGraphicsApi::NK_API_OPENGLES:  return true;
            case NkGraphicsApi::NK_API_VULKAN:    return true;
            case NkGraphicsApi::NK_API_SOFTWARE:  return true;
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
            case NkGraphicsApi::NK_API_WEBGL:     return true;
            case NkGraphicsApi::NK_API_SOFTWARE:  return true;
#else
            case NkGraphicsApi::NK_API_OPENGL:    return true;
            case NkGraphicsApi::NK_API_VULKAN:    return true;
            case NkGraphicsApi::NK_API_SOFTWARE:  return true;
#endif
            default: return false;
        }
    }

} // namespace nkentseu