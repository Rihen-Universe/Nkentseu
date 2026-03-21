#pragma once

#include "NKContext/Deprecate/NkGraphicsContextFactory.h"

namespace nkentseu {

    class NkWindow;

    enum class NkGraphicsInitModel : uint32 {
        NK_GRAPHICS_INIT_MODEL_CONTEXT = 0,
        NK_GRAPHICS_INIT_MODEL_SURFACE,
    };

    struct NkGraphicsApiInitInfo {
        NkGraphicsApi      api = NkGraphicsApi::NK_NONE;
        NkGraphicsInitModel model = NkGraphicsInitModel::NK_GRAPHICS_INIT_MODEL_CONTEXT;
        bool               requiresWindowHints = false;
        bool               requiresCurrentContext = false;
        bool               requiresProcAddressLoader = false;
    };

    struct NkSimpleGraphicsContextDesc {
        NkGraphicsApi   api = NkGraphicsApi::Auto;

        bool            debug = false;
        bool            vsync = true;
        uint32          msaaSamples = 1;
        uint32          swapchainImages = 2;

        // OpenGL specific
        uint32          glMajor = 3;
        uint32          glMinor = 3;
        NkOpenGLProfile glProfile = NkOpenGLProfile::Core;

        // Vulkan specific
        bool            vkValidation = false;

        // DirectX specific
        bool            dxDebugLayer = false;
    };

    class NkSimpleGraphicsContext {
        public:
            static NkGraphicsContextConfig BuildConfig(const NkSimpleGraphicsContextDesc& desc);
            static NkGraphicsApiInitInfo   QueryInitInfo(NkGraphicsApi api);

            // Call before window.Create(config).
            static void PrepareWindowConfig(NkWindowConfig& windowConfig,
                                            const NkSimpleGraphicsContextDesc& desc);

            // Call after window.Create(config).
            static NkGraphicsContextPtr Create(NkWindow& window,
                                               const NkSimpleGraphicsContextDesc& desc);
    };

} // namespace nkentseu
