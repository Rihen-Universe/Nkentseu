#include "NkSimpleGraphicsContext.h"

#include "NKWindow/Core/NkWindow.h"

namespace nkentseu {

    NkGraphicsContextConfig NkSimpleGraphicsContext::BuildConfig(const NkSimpleGraphicsContextDesc& desc) {
        NkGraphicsContextConfig config{};
        config.api = desc.api;

        config.vsync = desc.vsync;
        config.msaaSamples = desc.msaaSamples;
        config.swapchainImages = desc.swapchainImages;

        config.glMajor = desc.glMajor;
        config.glMinor = desc.glMinor;
        config.glProfile = desc.glProfile;
        config.glDebug = desc.debug;

        config.vkValidation = desc.vkValidation || desc.debug;
        config.dxDebugLayer = desc.dxDebugLayer || desc.debug;

        return config;
    }

    NkGraphicsApiInitInfo NkSimpleGraphicsContext::QueryInitInfo(NkGraphicsApi api) {
        NkGraphicsApiInitInfo info{};
        info.api = api;

        switch (api) {
            case NkGraphicsApi::OpenGL:
                info.model = NkGraphicsInitModel::NK_GRAPHICS_INIT_MODEL_CONTEXT;
                info.requiresCurrentContext = true;
                info.requiresProcAddressLoader = true;
            #if defined(NKENTSEU_PLATFORM_LINUX) && (defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB))
                info.requiresWindowHints = true;
            #else
                info.requiresWindowHints = false;
            #endif
                break;

            case NkGraphicsApi::Vulkan:
                info.model = NkGraphicsInitModel::NK_GRAPHICS_INIT_MODEL_SURFACE;
                info.requiresWindowHints = false;
                info.requiresCurrentContext = false;
                info.requiresProcAddressLoader = false;
                break;

            case NkGraphicsApi::DirectX11:
            case NkGraphicsApi::DirectX12:
            case NkGraphicsApi::Metal:
                info.model = NkGraphicsInitModel::NK_GRAPHICS_INIT_MODEL_CONTEXT;
                info.requiresWindowHints = false;
                info.requiresCurrentContext = false;
                info.requiresProcAddressLoader = false;
                break;

            case NkGraphicsApi::Software:
                info.model = NkGraphicsInitModel::NK_GRAPHICS_INIT_MODEL_SURFACE;
                info.requiresWindowHints = false;
                info.requiresCurrentContext = false;
                info.requiresProcAddressLoader = false;
                break;

            case NkGraphicsApi::Auto:
                return QueryInitInfo(NkGraphicsContextFactory::BestAvailableApi());

            case NkGraphicsApi::NK_NONE:
            default:
                info.model = NkGraphicsInitModel::NK_GRAPHICS_INIT_MODEL_CONTEXT;
                info.requiresWindowHints = false;
                info.requiresCurrentContext = false;
                info.requiresProcAddressLoader = false;
                break;
        }

        return info;
    }

    void NkSimpleGraphicsContext::PrepareWindowConfig(NkWindowConfig& windowConfig,
                                                       const NkSimpleGraphicsContextDesc& desc) {
        const NkGraphicsContextConfig config = BuildConfig(desc);
        NkGraphicsContextFactory::PrepareWindowConfig(windowConfig, config);
    }

    NkGraphicsContextPtr NkSimpleGraphicsContext::Create(NkWindow& window,
                                                          const NkSimpleGraphicsContextDesc& desc) {
        const NkGraphicsContextConfig config = BuildConfig(desc);
        return NkGraphicsContextFactory::Create(window, config);
    }

} // namespace nkentseu
