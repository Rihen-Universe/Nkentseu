#pragma once
#include "NKEvent/NkGraphicsEvent.h"

namespace nkentseu {

    inline const char* NkGraphicsApiName(NkGraphicsApi api) {
        switch (api) {
            case NkGraphicsApi::NK_GFX_API_OPENGL:    return "OpenGL";
            case NkGraphicsApi::NK_GFX_API_OPENGLES:  return "OpenGL ES";
            case NkGraphicsApi::NK_GFX_API_WEBGL:     return "WebGL";
            case NkGraphicsApi::NK_GFX_API_VULKAN:    return "Vulkan";
            case NkGraphicsApi::NK_GFX_API_DX11: return "DirectX 11";
            case NkGraphicsApi::NK_GFX_API_DX12: return "DirectX 12";
            case NkGraphicsApi::NK_GFX_API_METAL:     return "Metal";
            case NkGraphicsApi::NK_GFX_API_SOFTWARE:  return "Software";
            default:                               return "None";
        }
    }

} // namespace nkentseu