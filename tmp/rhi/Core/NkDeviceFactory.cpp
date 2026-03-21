// =============================================================================
// NkDeviceFactory.cpp
// =============================================================================
#include "NkDeviceFactory.h"
#include "NKRHI/Opengl/NkOpenglDevice.h"
#include "NKRHI/Software/NkSoftwareDevice.h"

#ifdef NK_RHI_VK_ENABLED
#include "NKRHI/Vulkan/NkVulkanDevice.h"
#endif
#ifdef NK_RHI_DX11_ENABLED
#include "NKRHI/DirectX11/NkDirectX11Device.h"
#endif
#ifdef NK_RHI_DX12_ENABLED
#include "NKRHI/DirectX12/NkDirectX12Device.h"
#endif
#ifdef NK_RHI_METAL_ENABLED
#include "NKRHI/Metal/NkMetalDevice.h"
#endif

#include <cstdio>

namespace nkentseu {

    NkIDevice* NkDeviceFactory::CreateForApi(NkGraphicsApi api, NkSurfaceDesc* surface) {
        NkIDevice* dev = nullptr;

        switch (api) {
            case NkGraphicsApi::NK_API_OPENGL:
                dev = new NkSoftwareDevice();
                break;

            case NkGraphicsApi::NK_API_VULKAN:
    #ifdef NK_RHI_VK_ENABLED
                dev = new NkVulkanDevice();
    #else
                printf("[NkDeviceFactory] Vulkan non disponible (NK_RHI_VK_ENABLED non défini)\n");
    #endif
                break;

            case NkGraphicsApi::NK_API_DIRECTX11:
    #ifdef NK_RHI_DX11_ENABLED
                dev = new NkDirectX11Device();
    #elif defined(NKENTSEU_PLATFORM_WINDOWS)
                printf("[NkDeviceFactory] DX11 non activé (définir NK_RHI_DX11_ENABLED)\n");
    #endif
                break;

            case NkGraphicsApi::NK_API_DIRECTX12:
    #ifdef NK_RHI_DX12_ENABLED
                dev = new NkDirectX12Device();
    #elif defined(NKENTSEU_PLATFORM_WINDOWS)
                printf("[NkDeviceFactory] DX12 non activé (définir NK_RHI_DX12_ENABLED)\n");
    #endif
                break;

            case NkGraphicsApi::NK_API_METAL:
    #ifdef NK_RHI_METAL_ENABLED
                dev = new NkMetalDevice();
    #else
                printf("[NkDeviceFactory] Metal non activé (définir NK_RHI_METAL_ENABLED)\n");
    #endif
                break;

            case NkGraphicsApi::NK_API_SOFTWARE:
                dev = new NkSoftwareDevice();
                break;

            default:
                printf("[NkDeviceFactory] API non supportée par le RHI: %s\n",
                    NkGraphicsApiName(api));
                break;
        }

        if (!dev) return nullptr;

        if (!dev->Initialize(ctx)) {
            printf("[NkDeviceFactory] Initialize() failed pour API: %s\n", NkGraphicsApiName(api));
            delete dev;
            return nullptr;
        }
        printf("[NkDeviceFactory] Device RHI créé: %s | %s\n", NkGraphicsApiName(api), dev->GetCaps().vramBytes ? "OK" : "caps non disponibles");
        return dev;
    }

    NkIDevice* NkDeviceFactory::CreateWithFallback(NkSurfaceDesc* surface, std::initializer_list<NkGraphicsApi> order) {
        for (auto api : order) {
            if (!IsApiSupported(api)) continue;
            NkIDevice* dev = CreateForApi(api, surface);
            if (dev && dev->IsValid()) return dev;
            if (dev) { dev->Shutdown(); delete dev; }
        }
        printf("[NkDeviceFactory] Aucune API disponible dans la liste de fallback\n");
        return nullptr;
    }

    bool NkDeviceFactory::IsApiSupported(NkGraphicsApi api) {
        switch (api) {
            case NkGraphicsApi::NK_API_OPENGL:    return true;
            case NkGraphicsApi::NK_API_SOFTWARE:  return true;
    #ifdef NK_RHI_VK_ENABLED
            case NkGraphicsApi::NK_API_VULKAN:    return true;
    #endif
    #ifdef NK_RHI_DX11_ENABLED
            case NkGraphicsApi::NK_API_DIRECTX11: return true;
    #endif
    #ifdef NK_RHI_DX12_ENABLED
            case NkGraphicsApi::NK_API_DIRECTX12: return true;
    #endif
    #ifdef NK_RHI_METAL_ENABLED
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
