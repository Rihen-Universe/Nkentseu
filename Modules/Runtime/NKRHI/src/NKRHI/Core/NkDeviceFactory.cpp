// =============================================================================
// NkDeviceFactory.cpp
// =============================================================================
#include "NkDeviceFactory.h"
#include "NKLogger/NkLog.h"
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

namespace nkentseu {

NkIDevice* NkDeviceFactory::Create(const NkDeviceInitInfo& init) {
    const NkGraphicsApi api = NkDeviceInitApi(init);
    if (api == NkGraphicsApi::NK_GFX_API_NONE) {
        logger_src.Infof("[NkDeviceFactory] API non specifiee (init.api/context.api == NK_GFX_API_NONE)\n");
        return nullptr;
    }
    NkDeviceInitInfo effectiveInit = init;
    effectiveInit.api = api;
    if (effectiveInit.context.api == NkGraphicsApi::NK_GFX_API_NONE) {
        effectiveInit.context.api = api;
    }
    return CreateForApi(api, effectiveInit);
}

NkIDevice* NkDeviceFactory::CreateForApi(NkGraphicsApi api, const NkDeviceInitInfo& init) {
    NkIDevice* dev = nullptr;

    switch (api) {
        case NkGraphicsApi::NK_GFX_API_OPENGL:
            dev = new NkOpenGLDevice();
            break;

        case NkGraphicsApi::NK_GFX_API_VULKAN:
#ifdef NK_RHI_VK_ENABLED
            dev = new NkVulkanDevice();
#else
            logger_src.Infof("[NkDeviceFactory] Vulkan non disponible (NK_RHI_VK_ENABLED non defini)\n");
#endif
            break;

        case NkGraphicsApi::NK_GFX_API_D3D11:
#ifdef NK_RHI_DX11_ENABLED
            dev = new NkDirectX11Device();
#elif defined(NKENTSEU_PLATFORM_WINDOWS)
            logger_src.Infof("[NkDeviceFactory] DX11 non active (definir NK_RHI_DX11_ENABLED)\n");
#endif
            break;

        case NkGraphicsApi::NK_GFX_API_D3D12:
#ifdef NK_RHI_DX12_ENABLED
            dev = new NkDirectX12Device();
#elif defined(NKENTSEU_PLATFORM_WINDOWS)
            logger_src.Infof("[NkDeviceFactory] DX12 non active (definir NK_RHI_DX12_ENABLED)\n");
#endif
            break;

        case NkGraphicsApi::NK_GFX_API_METAL:
#ifdef NK_RHI_METAL_ENABLED
            dev = new NkMetalDevice();
#else
            logger_src.Infof("[NkDeviceFactory] Metal non active (definir NK_RHI_METAL_ENABLED)\n");
#endif
            break;

        case NkGraphicsApi::NK_GFX_API_SOFTWARE:
            dev = new NkSoftwareDevice();
            break;

        default:
            logger_src.Infof("[NkDeviceFactory] API non supportee par le RHI: %s\n",
                             NkGraphicsApiName(api));
            break;
    }

    if (!dev) return nullptr;

    if (!dev->Initialize(init)) {
        logger_src.Infof("[NkDeviceFactory] Initialize() failed pour API: %s\n",
                         NkGraphicsApiName(api));
        delete dev;
        return nullptr;
    }

    logger_src.Infof("[NkDeviceFactory] Device RHI cree: %s | %s\n",
                     NkGraphicsApiName(api),
                     dev->GetCaps().vramBytes ? "OK" : "caps non disponibles");
    return dev;
}

NkIDevice* NkDeviceFactory::CreateWithFallback(const NkDeviceInitInfo& init,
                                               std::initializer_list<NkGraphicsApi> order) {
    for (auto api : order) {
        if (!IsApiSupported(api)) continue;
        NkDeviceInitInfo effectiveInit = init;
        effectiveInit.api = api;
        if (effectiveInit.context.api == NkGraphicsApi::NK_GFX_API_NONE) {
            effectiveInit.context.api = api;
        }
        NkIDevice* dev = CreateForApi(api, effectiveInit);
        if (dev && dev->IsValid()) return dev;
        if (dev) { dev->Shutdown(); delete dev; }
    }
    logger_src.Infof("[NkDeviceFactory] Aucune API disponible dans la liste de fallback\n");
    return nullptr;
}

bool NkDeviceFactory::IsApiSupported(NkGraphicsApi api) {
    switch (api) {
        case NkGraphicsApi::NK_GFX_API_OPENGL:   return true;
        case NkGraphicsApi::NK_GFX_API_SOFTWARE: return true;
#ifdef NK_RHI_VK_ENABLED
        case NkGraphicsApi::NK_GFX_API_VULKAN:   return true;
#endif
#ifdef NK_RHI_DX11_ENABLED
        case NkGraphicsApi::NK_GFX_API_D3D11:return true;
#endif
#ifdef NK_RHI_DX12_ENABLED
        case NkGraphicsApi::NK_GFX_API_D3D12:return true;
#endif
#ifdef NK_RHI_METAL_ENABLED
        case NkGraphicsApi::NK_GFX_API_METAL:    return true;
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
