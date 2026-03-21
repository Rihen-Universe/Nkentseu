// =============================================================================
// NkDeviceFactory.cpp
// =============================================================================
#include "NkDeviceFactory.h"
#include "../RHI_GL/NkRHI_Device_GL.h"

// Inclure les autres devices quand implémentés :
// #include "../RHI_VK/NkRHI_Device_VK.h"
// #include "../RHI_DX11/NkRHI_Device_DX11.h"
// #include "../RHI_DX12/NkRHI_Device_DX12.h"
// #include "../RHI_Metal/NkRHI_Device_Metal.h"

#include <cstdio>

namespace nkentseu {

NkIDevice* NkDeviceFactory::Create(NkIGraphicsContext* ctx) {
    if (!ctx) return nullptr;
    return CreateForApi(ctx->GetApi(), ctx);
}

NkIDevice* NkDeviceFactory::CreateForApi(NkGraphicsApi api, NkIGraphicsContext* ctx) {
    NkIDevice* dev = nullptr;

    switch (api) {
        case NkGraphicsApi::NK_API_OPENGL:
            dev = new NkDevice_GL();
            break;

        case NkGraphicsApi::NK_API_VULKAN:
#ifdef NK_VULKAN_AVAILABLE
            // dev = new NkDevice_VK();
            printf("[NkDeviceFactory] Vulkan RHI non encore implémenté — fallback GL\n");
            dev = new NkDevice_GL();
#else
            printf("[NkDeviceFactory] Vulkan non disponible\n");
#endif
            break;

        case NkGraphicsApi::NK_API_DIRECTX11:
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            // dev = new NkDeviceDX11();
            printf("[NkDeviceFactory] DX11 RHI non encore implémenté\n");
#endif
            break;

        case NkGraphicsApi::NK_API_DIRECTX12:
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            // dev = new NkDeviceDX12();
            printf("[NkDeviceFactory] DX12 RHI non encore implémenté\n");
#endif
            break;

        case NkGraphicsApi::NK_API_METAL:
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
            // dev = new NkDevice_Metal();
            printf("[NkDeviceFactory] Metal RHI non encore implémenté\n");
#endif
            break;

        default:
            printf("[NkDeviceFactory] API non supportée par le RHI: %s\n",
                   NkGraphicsApiName(api));
            break;
    }

    if (!dev) return nullptr;

    if (!dev->Initialize(ctx)) {
        printf("[NkDeviceFactory] Initialize() failed pour API: %s\n",
               NkGraphicsApiName(api));
        delete dev;
        return nullptr;
    }
    printf("[NkDeviceFactory] Device RHI créé: %s | %s\n",
           NkGraphicsApiName(api), dev->GetCaps().vramBytes
           ? "OK" : "caps non disponibles");
    return dev;
}

NkIDevice* NkDeviceFactory::CreateWithFallback(NkIGraphicsContext* ctx,
    std::initializer_list<NkGraphicsApi> order) {
    for (auto api : order) {
        if (!IsApiSupported(api)) continue;
        NkIDevice* dev = CreateForApi(api, ctx);
        if (dev && dev->IsValid()) return dev;
        if (dev) { dev->Shutdown(); delete dev; }
    }
    printf("[NkDeviceFactory] Aucune API disponible dans la liste de fallback\n");
    return nullptr;
}

bool NkDeviceFactory::IsApiSupported(NkGraphicsApi api) {
    switch (api) {
        case NkGraphicsApi::NK_API_OPENGL:    return true;
#ifdef NK_VULKAN_AVAILABLE
        case NkGraphicsApi::NK_API_VULKAN:    return true;
#endif
#if defined(NKENTSEU_PLATFORM_WINDOWS)
        case NkGraphicsApi::NK_API_DIRECTX11: return true;
        case NkGraphicsApi::NK_API_DIRECTX12: return true;
#endif
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
        case NkGraphicsApi::NK_API_METAL:     return true;
#endif
        default: return false;
    }
}

void NkDeviceFactory::Destroy(NkIDevice*& device) {
    if (!device) return;
    device->Shutdown();
    delete device;
    device = nullptr;
}

} // namespace nkentseu
