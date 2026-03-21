// =============================================================================
// NkContextFactory.cpp
// =============================================================================
#include "NkContextFactory.h"
#include "../OpenGL/NkOpenGLContext.h"
#include "../Vulkan/NkVulkanContext.h"
#include "../Software/NkSoftwareContext.h"
#include "../Compute/NkOpenGLComputeContext.h"
#include "../Compute/NkVulkanComputeContext.h"
#include <cstdio>
#include <initializer_list>

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   include "../DirectX/NkDXContext.h"
#   include "../Compute/NkDX11ComputeContext.h"
#   include "../Compute/NkDX12ComputeContext.h"
#endif

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
#   include "../Metal/NkMetalContext.h"
#   include "../Compute/NkMetalComputeContext.h"
#endif

#define NK_FACTORY_LOG(...) printf("[NkContextFactory] " __VA_ARGS__)
#define NK_FACTORY_ERR(...) printf("[NkContextFactory][ERROR] " __VA_ARGS__)

namespace nkentseu {

// =============================================================================
NkIGraphicsContext* NkContextFactory::Create(const NkWindow& window,
                                              const NkContextDesc& desc) {
    NkIGraphicsContext* ctx = nullptr;

    switch (desc.api) {
        case NkGraphicsApi::NK_API_OPENGL:
        case NkGraphicsApi::NK_API_OPENGLES:
        case NkGraphicsApi::NK_API_WEBGL:
            ctx = new NkOpenGLContext();
            break;

        case NkGraphicsApi::NK_API_VULKAN:
            ctx = new NkVulkanContext();
            break;

#if defined(NKENTSEU_PLATFORM_WINDOWS)
        case NkGraphicsApi::NK_API_DIRECTX11:
            ctx = new NkDX11Context();
            break;
        case NkGraphicsApi::NK_API_DIRECTX12:
            ctx = new NkDX12Context();
            break;
#endif

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
        case NkGraphicsApi::NK_API_METAL:
            ctx = new NkMetalContext();
            break;
#endif

        case NkGraphicsApi::NK_API_SOFTWARE:
            ctx = new NkSoftwareContext();
            break;

        default:
            NK_FACTORY_ERR("Unknown or unsupported API: %s\n",
                           NkGraphicsApiName(desc.api));
            return nullptr;
    }

    if (!ctx->Initialize(window, desc)) {
        NK_FACTORY_ERR("Initialize failed for API: %s\n",
                       NkGraphicsApiName(desc.api));
        delete ctx;
        return nullptr;
    }

    NK_FACTORY_LOG("Context created: %s\n", NkGraphicsApiName(desc.api));
    return ctx;
}

// =============================================================================
bool NkContextFactory::IsApiSupported(NkGraphicsApi api) {
    return NkGraphicsApiIsAvailable(api);
}

// =============================================================================
NkIGraphicsContext* NkContextFactory::CreateWithFallback(
    const NkWindow& window,
    std::initializer_list<NkGraphicsApi> preferenceOrder)
{
    for (NkGraphicsApi api : preferenceOrder) {
        if (!IsApiSupported(api)) {
            NK_FACTORY_LOG("API %s not available on this platform — skip\n",
                           NkGraphicsApiName(api));
            continue;
        }

        NkContextDesc desc;
        switch (api) {
            case NkGraphicsApi::NK_API_OPENGL:    desc = NkContextDesc::MakeOpenGL();    break;
            case NkGraphicsApi::NK_API_OPENGLES:  desc = NkContextDesc::MakeOpenGLES();  break;
            case NkGraphicsApi::NK_API_VULKAN:    desc = NkContextDesc::MakeVulkan();    break;
            case NkGraphicsApi::NK_API_DIRECTX11: desc = NkContextDesc::MakeDirectX11(); break;
            case NkGraphicsApi::NK_API_DIRECTX12: desc = NkContextDesc::MakeDirectX12(); break;
            case NkGraphicsApi::NK_API_METAL:     desc = NkContextDesc::MakeMetal();     break;
            case NkGraphicsApi::NK_API_SOFTWARE:  desc = NkContextDesc::MakeSoftware();  break;
            default: continue;
        }

        NkIGraphicsContext* ctx = Create(window, desc);
        if (ctx) {
            NK_FACTORY_LOG("Fallback selected: %s\n", NkGraphicsApiName(api));
            return ctx;
        }
        NK_FACTORY_LOG("API %s failed, trying next\n", NkGraphicsApiName(api));
    }

    NK_FACTORY_ERR("All APIs in fallback list failed\n");
    return nullptr;
}

// =============================================================================
// Compute standalone
// =============================================================================
NkIComputeContext* NkContextFactory::CreateCompute(NkGraphicsApi api,
                                                    const NkContextDesc& desc) {
    NkIComputeContext* ctx = nullptr;

    switch (api) {
        case NkGraphicsApi::NK_API_OPENGL:
        case NkGraphicsApi::NK_API_OPENGLES:
            ctx = new NkOpenGLComputeContext();
            break;

        case NkGraphicsApi::NK_API_VULKAN:
            ctx = new NkVulkanComputeContext();
            break;

#if defined(NKENTSEU_PLATFORM_WINDOWS)
        case NkGraphicsApi::NK_API_DIRECTX11:
            ctx = new NkDX11ComputeContext();
            break;
        case NkGraphicsApi::NK_API_DIRECTX12:
            ctx = new NkDX12ComputeContext();
            break;
#endif

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
        case NkGraphicsApi::NK_API_METAL:
            ctx = new NkMetalComputeContext();
            break;
#endif

        default:
            NK_FACTORY_ERR("No compute context for API: %s\n",
                           NkGraphicsApiName(api));
            return nullptr;
    }

    NK_FACTORY_LOG("Compute context created: %s\n", NkGraphicsApiName(api));
    return ctx;
}

// =============================================================================
// Compute depuis contexte graphique existant (partage device/queue)
// =============================================================================
NkIComputeContext* NkContextFactory::ComputeFromGraphics(NkIGraphicsContext* gfx) {
    if (!gfx || !gfx->IsValid()) {
        NK_FACTORY_ERR("ComputeFromGraphics: invalid graphics context\n");
        return nullptr;
    }
    if (!gfx->SupportsCompute()) {
        NK_FACTORY_ERR("ComputeFromGraphics: API %s does not support compute\n",
                       NkGraphicsApiName(gfx->GetApi()));
        return nullptr;
    }

    switch (gfx->GetApi()) {
        case NkGraphicsApi::NK_API_OPENGL:
        case NkGraphicsApi::NK_API_OPENGLES: {
            auto* c = new NkOpenGLComputeContext();
            c->InitFromGraphicsContext(gfx);
            return c;
        }
        case NkGraphicsApi::NK_API_VULKAN: {
            auto* c = new NkVulkanComputeContext();
            c->InitFromGraphicsContext(gfx);
            return c;
        }
#if defined(NKENTSEU_PLATFORM_WINDOWS)
        case NkGraphicsApi::NK_API_DIRECTX11: {
            auto* c = new NkDX11ComputeContext();
            c->InitFromGraphicsContext(gfx);
            return c;
        }
        case NkGraphicsApi::NK_API_DIRECTX12: {
            auto* c = new NkDX12ComputeContext();
            c->InitFromGraphicsContext(gfx);
            return c;
        }
#endif
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
        case NkGraphicsApi::NK_API_METAL: {
            auto* c = new NkMetalComputeContext();
            c->InitFromGraphicsContext(gfx);
            return c;
        }
#endif
        default:
            NK_FACTORY_ERR("ComputeFromGraphics: unsupported API\n");
            return nullptr;
    }
}

} // namespace nkentseu
