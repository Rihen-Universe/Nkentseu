#pragma once

// =============================================================================
// NkRendererApi.h
// Enumeration commune des backends de rendu.
//
// Ce header est volontairement placé dans NKCore pour éviter que NKWindow
// soit propriétaire des types renderer.
// =============================================================================

#include "NKCore/NkTypes.h"

namespace nkentseu {

    enum class NkRendererApi : uint32 {
        NK_NONE = 0,
        NK_SOFTWARE,
        NK_OPENGL,
        NK_VULKAN,
        NK_DIRECTX11,
        NK_DIRECTX12,
        NK_METAL,
        NK_RENDERER_API_MAX
    };

    inline const char* NkRendererApiToString(NkRendererApi api) {
        switch (api) {
            case NkRendererApi::NK_SOFTWARE:  return "Software";
            case NkRendererApi::NK_OPENGL:    return "OpenGL";
            case NkRendererApi::NK_VULKAN:    return "Vulkan";
            case NkRendererApi::NK_DIRECTX11: return "DirectX 11";
            case NkRendererApi::NK_DIRECTX12: return "DirectX 12";
            case NkRendererApi::NK_METAL:     return "Metal";
            default:                           return "None";
        }
    }

} // namespace nkentseu

