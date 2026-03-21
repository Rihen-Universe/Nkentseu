// =============================================================================
// NkDeviceFactory.cpp
// =============================================================================
#include "NkDeviceFactory.h"
#include "NKLogger/NkLog.h"
#include "NKRenderer/RHI/Backend/OpenGL/NkOpenGLDevice.h"
#include "NKRenderer/RHI/Backend/Software/NkSoftwareDevice.h"

#ifdef NK_RHI_VK_ENABLED
#include "NKRenderer/RHI/Backend/Vulkan/NkVulkanDevice.h"
#endif
#ifdef NK_RHI_DX11_ENABLED
#include "NKRenderer/RHI/Backend/DirectX11/NkDX11Device.h"
#endif
#ifdef NK_RHI_DX12_ENABLED
#include "NKRenderer/RHI/Backend/DirectX12/NkDX12Device.h"
#endif
#ifdef NK_RHI_METAL_ENABLED
#include "NKRenderer/RHI/Backend/Metal/NkMetalDevice.h"
#endif

namespace nkentseu {

NkIDevice* NkDeviceFactory::Create(NkIGraphicsContext* ctx) {
    if (!ctx) return nullptr;
    return CreateForApi(ctx->GetApi(), ctx);
}

NkIDevice* NkDeviceFactory::CreateForApi(NkGraphicsApi api, NkIGraphicsContext* ctx) {
    NkIDevice* dev = nullptr;

    switch (api) {
        case NkGraphicsApi::NK_API_OPENGL:
            dev = new NkOpenGLDevice();
            break;

        case NkGraphicsApi::NK_API_VULKAN:
#ifdef NK_RHI_VK_ENABLED
            dev = new NkVulkanDevice();
#else
            logger_src.Infof("[NkDeviceFactory] Vulkan non disponible (NK_RHI_VK_ENABLED non defini)\n");
#endif
            break;

        case NkGraphicsApi::NK_API_DIRECTX11:
#ifdef NK_RHI_DX11_ENABLED
            dev = new NkDeviceDX11();
#elif defined(NKENTSEU_PLATFORM_WINDOWS)
            logger_src.Infof("[NkDeviceFactory] DX11 non active (definir NK_RHI_DX11_ENABLED)\n");
#endif
            break;

        case NkGraphicsApi::NK_API_DIRECTX12:
#ifdef NK_RHI_DX12_ENABLED
            dev = new NkDeviceDX12();
#elif defined(NKENTSEU_PLATFORM_WINDOWS)
            logger_src.Infof("[NkDeviceFactory] DX12 non active (definir NK_RHI_DX12_ENABLED)\n");
#endif
            break;

        case NkGraphicsApi::NK_API_METAL:
#ifdef NK_RHI_METAL_ENABLED
            dev = new NkDevice_Metal();
#else
            logger_src.Infof("[NkDeviceFactory] Metal non active (definir NK_RHI_METAL_ENABLED)\n");
#endif
            break;

        case NkGraphicsApi::NK_API_SOFTWARE:
            dev = new NkSoftwareDevice();
            break;

        default:
            logger_src.Infof("[NkDeviceFactory] API non supportee par le RHI: %s\n",
                             NkGraphicsApiName(api));
            break;
    }

    if (!dev) return nullptr;

    if (!dev->Initialize(ctx)) {
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

NkIDevice* NkDeviceFactory::CreateWithFallback(NkIGraphicsContext* ctx,
                                               std::initializer_list<NkGraphicsApi> order) {
    for (auto api : order) {
        if (!IsApiSupported(api)) continue;
        NkIDevice* dev = CreateForApi(api, ctx);
        if (dev && dev->IsValid()) return dev;
        if (dev) { dev->Shutdown(); delete dev; }
    }
    logger_src.Infof("[NkDeviceFactory] Aucune API disponible dans la liste de fallback\n");
    return nullptr;
}

bool NkDeviceFactory::IsApiSupported(NkGraphicsApi api) {
    switch (api) {
        case NkGraphicsApi::NK_API_OPENGL:   return true;
        case NkGraphicsApi::NK_API_SOFTWARE: return true;
#ifdef NK_RHI_VK_ENABLED
        case NkGraphicsApi::NK_API_VULKAN:   return true;
#endif
#ifdef NK_RHI_DX11_ENABLED
        case NkGraphicsApi::NK_API_DIRECTX11:return true;
#endif
#ifdef NK_RHI_DX12_ENABLED
        case NkGraphicsApi::NK_API_DIRECTX12:return true;
#endif
#ifdef NK_RHI_METAL_ENABLED
        case NkGraphicsApi::NK_API_METAL:    return true;
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
