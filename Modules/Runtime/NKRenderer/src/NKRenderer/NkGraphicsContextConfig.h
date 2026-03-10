#pragma once
// =============================================================================
// NkGraphicsContextConfig.h
// Configuration du contexte graphique.
// Aucune dépendance vers NkWindow ou NkSurface.
// =============================================================================

#include "NKWindow/Core/NkTypes.h"

namespace nkentseu {

    enum class NkGraphicsApi : NkU32 {
        None      = 0,
        OpenGL    = 1,
        Vulkan    = 2,
        DirectX11 = 3,
        DirectX12 = 4,
        Metal     = 5,
        Software  = 6,
        Auto      = 255,
    };

    enum class NkOpenGLProfile : NkU32 {
        Core          = 0,
        Compatibility = 1,
        ES            = 2,
    };

    enum class NkDepthFormat : NkU32 {
        None             = 0,
        Depth16          = 1,
        Depth24          = 2,
        Depth32F         = 3,
        Depth24Stencil8  = 4,
        Depth32FStencil8 = 5,
    };

    struct NkGraphicsContextConfig {
        NkGraphicsApi     api             = NkGraphicsApi::Auto;
        NkU32             colorBits       = 32;
        NkDepthFormat     depthFormat     = NkDepthFormat::Depth24Stencil8;
        NkU32             stencilBits     = 8;
        NkU32             msaaSamples     = 1;
        bool              vsync           = true;
        NkU32             swapchainImages = 2;

        // OpenGL
        NkU32             glMajor         = 3;
        NkU32             glMinor         = 3;
        NkOpenGLProfile   glProfile       = NkOpenGLProfile::Core;
        bool              glDebug         = false;

        // Vulkan
        bool              vkValidation    = false;
        NkU32             vkApiVersion    = 0;       // 0 = auto (1.2)

        // DirectX
        bool              dxDebugLayer    = false;

        // Software
        NkU32             softPixelFormat = 0;       // 0 = RGBA8

        // --- Helpers ---
        static NkGraphicsContextConfig ForOpenGL(NkU32 maj=3, NkU32 min=3,
                                                bool debug=false) {
            NkGraphicsContextConfig c;
            c.api=NkGraphicsApi::OpenGL; c.glMajor=maj;
            c.glMinor=min; c.glDebug=debug; return c;
        }
        static NkGraphicsContextConfig ForVulkan(bool validation=false) {
            NkGraphicsContextConfig c;
            c.api=NkGraphicsApi::Vulkan;
            c.vkValidation=validation; c.swapchainImages=3; return c;
        }
        static NkGraphicsContextConfig ForMetal() {
            NkGraphicsContextConfig c; c.api=NkGraphicsApi::Metal; return c;
        }
        static NkGraphicsContextConfig ForDirectX12(bool debug=false) {
            NkGraphicsContextConfig c;
            c.api=NkGraphicsApi::DirectX12; c.dxDebugLayer=debug; return c;
        }
        static NkGraphicsContextConfig ForSoftware() {
            NkGraphicsContextConfig c; c.api=NkGraphicsApi::Software; return c;
        }
    };

} // namespace nkentseu