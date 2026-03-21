// =============================================================================
// NkContextFactory.cpp
// =============================================================================
#include "NkContextFactory.h"
#include "NKContext/Graphics/OpenGL/NkOpenGLContext.h"
#include "NKContext/Graphics/Software/NkSoftwareContext.h"
#include "NKContext/Compute/NkOpenGLComputeContext.h"
#include "NKContext/Compute/NkSoftwareComputeContext.h"
#include "NKContext/Core/NkGpuPolicy.h"
#include <cstdio>

#include "NKPlatform/NkPlatformDetect.h"
#include "NKLogger/NkLog.h"

#if !defined(NKENTSEU_ENABLE_VULKAN_BACKEND)
#   define NKENTSEU_ENABLE_VULKAN_BACKEND 1
#endif

#if NKENTSEU_ENABLE_VULKAN_BACKEND
#   include "NKContext/Graphics/Vulkan/NkVulkanContext.h"
#   include "NKContext/Compute/NkVulkanComputeContext.h"
#endif

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   include "NKContext/Graphics/DirectX/NkDXContext.h"
#   include "NKContext/Compute/NkDX11ComputeContext.h"
#   include "NKContext/Compute/NkDX12ComputeContext.h"
#endif

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
#   include "NKContext/Graphics/Metal/NkMetalContext.h"
#   include "NKContext/Compute/NkMetalComputeContext.h"
#endif

#define NK_FACTORY_LOG(...) logger.Infof("[NkContextFactory] " __VA_ARGS__)
#define NK_FACTORY_ERR(...) logger.Errorf("[NkContextFactory] " __VA_ARGS__)

namespace nkentseu {

// =============================================================================
NkIGraphicsContext* NkContextFactory::Create(const NkWindow& window,
                                              const NkContextDesc& desc) {
    logger.Warnf("[NkContextFactory][DBG] Create begin api=%s", NkGraphicsApiName(desc.api));
    NkGpuPolicy::ApplyPreContext(desc);
    NkIGraphicsContext* ctx = nullptr;

    switch (desc.api) {
        case NkGraphicsApi::NK_API_OPENGL:
        case NkGraphicsApi::NK_API_OPENGLES:
        case NkGraphicsApi::NK_API_WEBGL:
            ctx = new NkOpenGLContext();
            break;

        case NkGraphicsApi::NK_API_VULKAN:
#if NKENTSEU_ENABLE_VULKAN_BACKEND
            ctx = new NkVulkanContext();
#else
            NK_FACTORY_ERR("Vulkan backend disabled at build time\n");
            return nullptr;
#endif
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

    logger.Warnf("[NkContextFactory][DBG] Context allocated ptr=%p", static_cast<void*>(ctx));

    logger.Warn("[NkContextFactory][DBG] Initialize begin");
    if (!ctx->Initialize(window, desc)) {
        NK_FACTORY_ERR("Initialize failed for API: %s\n",
                       NkGraphicsApiName(desc.api));
        delete ctx;
        return nullptr;
    }
    logger.Warn("[NkContextFactory][DBG] Initialize success");

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
    const NkGraphicsApi* preferenceOrder,
    uint32 preferenceCount)
{
    if (!preferenceOrder || preferenceCount == 0) {
        NK_FACTORY_ERR("CreateWithFallback called with empty preference list\n");
        return nullptr;
    }

    for (uint32 i = 0; i < preferenceCount; ++i) {
        NkGraphicsApi api = preferenceOrder[i];
        if (!IsApiSupported(api)) {
            NK_FACTORY_LOG("API %s not available on this platform â€” skip\n",
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
    if (!desc.IsComputeEnabledForApi(api)) {
        NK_FACTORY_ERR("Compute is disabled for API: %s\n", NkGraphicsApiName(api));
        return nullptr;
    }

    switch (api) {
        case NkGraphicsApi::NK_API_OPENGL:
        case NkGraphicsApi::NK_API_OPENGLES: {
            auto* ctx = new NkOpenGLComputeContext();
            if (!ctx->Init(desc)) {
                NK_FACTORY_ERR("OpenGL standalone compute init failed\n");
                delete ctx;
                return nullptr;
            }
            NK_FACTORY_LOG("Compute context created: %s\n", NkGraphicsApiName(api));
            return ctx;
        }

        case NkGraphicsApi::NK_API_VULKAN:
#if NKENTSEU_ENABLE_VULKAN_BACKEND
        {
            auto* ctx = new NkVulkanComputeContext();
            if (!ctx->Init(desc)) {
                NK_FACTORY_ERR("Vulkan standalone compute init failed\n");
                delete ctx;
                return nullptr;
            }
            NK_FACTORY_LOG("Compute context created: %s\n", NkGraphicsApiName(api));
            return ctx;
        }
#else
            NK_FACTORY_ERR("Vulkan compute backend disabled at build time\n");
            return nullptr;
#endif

#if defined(NKENTSEU_PLATFORM_WINDOWS)
        case NkGraphicsApi::NK_API_DIRECTX11:
        {
            auto* ctx = new NkDX11ComputeContext();
            if (!ctx->Init(desc)) {
                NK_FACTORY_ERR("DX11 standalone compute init failed\n");
                delete ctx;
                return nullptr;
            }
            NK_FACTORY_LOG("Compute context created: %s\n", NkGraphicsApiName(api));
            return ctx;
        }
        case NkGraphicsApi::NK_API_DIRECTX12:
        {
            auto* ctx = new NkDX12ComputeContext();
            if (!ctx->Init(desc)) {
                NK_FACTORY_ERR("DX12 standalone compute init failed\n");
                delete ctx;
                return nullptr;
            }
            NK_FACTORY_LOG("Compute context created: %s\n", NkGraphicsApiName(api));
            return ctx;
        }
#endif

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
        case NkGraphicsApi::NK_API_METAL:
        {
            auto* ctx = new NkMetalComputeContext();
            if (!ctx->Init(desc)) {
                NK_FACTORY_ERR("Metal standalone compute init failed\n");
                delete ctx;
                return nullptr;
            }
            NK_FACTORY_LOG("Compute context created: %s\n", NkGraphicsApiName(api));
            return ctx;
        }
#endif
        case NkGraphicsApi::NK_API_SOFTWARE: {
            auto* ctx = new NkSoftwareComputeContext();
            if (!ctx->Init(desc)) {
                NK_FACTORY_ERR("Software standalone compute init failed\n");
                delete ctx;
                return nullptr;
            }
            NK_FACTORY_LOG("Compute context created: %s\n", NkGraphicsApiName(api));
            return ctx;
        }

        default:
            NK_FACTORY_ERR("No compute context for API: %s\n",
                           NkGraphicsApiName(api));
            return nullptr;
    }
}

// =============================================================================
// Compute depuis contexte graphique existant (partage device/queue)
// =============================================================================
NkIComputeContext* NkContextFactory::ComputeFromGraphics(NkIGraphicsContext* gfx) {
    if (!gfx || !gfx->IsValid()) {
        NK_FACTORY_ERR("ComputeFromGraphics: invalid graphics context\n");
        return nullptr;
    }
    const NkContextDesc desc = gfx->GetDesc();
    if (!desc.IsComputeEnabledForApi(gfx->GetApi())) {
        NK_FACTORY_ERR("ComputeFromGraphics: compute disabled in context desc for API %s\n",
                       NkGraphicsApiName(gfx->GetApi()));
        return nullptr;
    }
    if (!gfx->SupportsCompute() && gfx->GetApi() != NkGraphicsApi::NK_API_SOFTWARE) {
        NK_FACTORY_ERR("ComputeFromGraphics: API %s does not support compute\n",
                       NkGraphicsApiName(gfx->GetApi()));
        return nullptr;
    }

    switch (gfx->GetApi()) {
        case NkGraphicsApi::NK_API_OPENGL:
        case NkGraphicsApi::NK_API_OPENGLES: {
            auto* c = new NkOpenGLComputeContext();
            c->InitFromGraphicsContext(gfx);
            if (!c->IsValid()) { delete c; return nullptr; }
            return c;
        }
        case NkGraphicsApi::NK_API_VULKAN: {
#if NKENTSEU_ENABLE_VULKAN_BACKEND
            auto* c = new NkVulkanComputeContext();
            c->InitFromGraphicsContext(gfx);
            if (!c->IsValid()) { delete c; return nullptr; }
            return c;
#else
            NK_FACTORY_ERR("Vulkan compute backend disabled at build time\n");
            return nullptr;
#endif
        }
#if defined(NKENTSEU_PLATFORM_WINDOWS)
        case NkGraphicsApi::NK_API_DIRECTX11: {
            auto* c = new NkDX11ComputeContext();
            c->InitFromGraphicsContext(gfx);
            if (!c->IsValid()) { delete c; return nullptr; }
            return c;
        }
        case NkGraphicsApi::NK_API_DIRECTX12: {
            auto* c = new NkDX12ComputeContext();
            c->InitFromGraphicsContext(gfx);
            if (!c->IsValid()) { delete c; return nullptr; }
            return c;
        }
#endif
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
        case NkGraphicsApi::NK_API_METAL: {
            auto* c = new NkMetalComputeContext();
            c->InitFromGraphicsContext(gfx);
            if (!c->IsValid()) { delete c; return nullptr; }
            return c;
        }
#endif
        case NkGraphicsApi::NK_API_SOFTWARE: {
            auto* c = new NkSoftwareComputeContext();
            c->InitFromGraphicsContext(gfx);
            if (!c->IsValid()) { delete c; return nullptr; }
            return c;
        }
        default:
            NK_FACTORY_ERR("ComputeFromGraphics: unsupported API\n");
            return nullptr;
    }
}

} // namespace nkentseu
