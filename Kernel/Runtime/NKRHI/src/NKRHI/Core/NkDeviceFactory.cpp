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

        case NkGraphicsApi::NK_GFX_API_DX11:
#ifdef NK_RHI_DX11_ENABLED
            dev = new NkDirectX11Device();
#elif defined(NKENTSEU_PLATFORM_WINDOWS)
            logger_src.Infof("[NkDeviceFactory] DX11 non active (definir NK_RHI_DX11_ENABLED)\n");
#endif
            break;

        case NkGraphicsApi::NK_GFX_API_DX12:
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
        case NkGraphicsApi::NK_GFX_API_DX11:return true;
#endif
#ifdef NK_RHI_DX12_ENABLED
        case NkGraphicsApi::NK_GFX_API_DX12:return true;
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

// =============================================================================
// Ordre de priorité par plateforme (file-scope, non exporté)
// =============================================================================
static NkVector<NkGraphicsApi> GetPlatformPriorityOrder() {
    NkVector<NkGraphicsApi> order;

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
    order.PushBack(NkGraphicsApi::NK_GFX_API_METAL);
    order.PushBack(NkGraphicsApi::NK_GFX_API_OPENGL);

#elif defined(NKENTSEU_PLATFORM_WINDOWS)
    // Vulkan offre les meilleures perfs et la meilleure portabilité multi-GPU.
    // DX12 ensuite (contrôle bas-niveau natif Windows).
    // DX11 comme fallback legacy.
    // OpenGL pour les machines sans DX ou Vulkan.
    order.PushBack(NkGraphicsApi::NK_GFX_API_VULKAN);
    order.PushBack(NkGraphicsApi::NK_GFX_API_DX12);
    order.PushBack(NkGraphicsApi::NK_GFX_API_DX11);
    order.PushBack(NkGraphicsApi::NK_GFX_API_OPENGL);

#elif defined(NKENTSEU_PLATFORM_ANDROID)
    order.PushBack(NkGraphicsApi::NK_GFX_API_VULKAN);
    order.PushBack(NkGraphicsApi::NK_GFX_API_OPENGL);

#else
    // Linux, Emscripten, inconnu : Vulkan puis OpenGL
    order.PushBack(NkGraphicsApi::NK_GFX_API_VULKAN);
    order.PushBack(NkGraphicsApi::NK_GFX_API_OPENGL);
#endif

    order.PushBack(NkGraphicsApi::NK_GFX_API_SOFTWARE);
    return order;
}

// =============================================================================

NkVector<NkGraphicsApi> NkDeviceFactory::GetSupportedApis() {
    NkVector<NkGraphicsApi> apis;
    apis.PushBack(NkGraphicsApi::NK_GFX_API_OPENGL);
#ifdef NK_RHI_VK_ENABLED
    apis.PushBack(NkGraphicsApi::NK_GFX_API_VULKAN);
#endif
#ifdef NK_RHI_DX11_ENABLED
    apis.PushBack(NkGraphicsApi::NK_GFX_API_DX11);
#endif
#ifdef NK_RHI_DX12_ENABLED
    apis.PushBack(NkGraphicsApi::NK_GFX_API_DX12);
#endif
#ifdef NK_RHI_METAL_ENABLED
    apis.PushBack(NkGraphicsApi::NK_GFX_API_METAL);
#endif
    apis.PushBack(NkGraphicsApi::NK_GFX_API_SOFTWARE);
    return apis;
}

NkIDevice* NkDeviceFactory::CreateAutoDetect(NkDeviceInitInfo& init) {
    NkVector<NkGraphicsApi> order = GetPlatformPriorityOrder();

    for (uint32 i = 0; i < (uint32)order.Size(); ++i) {
        NkGraphicsApi api = order[i];
        if (!IsApiSupported(api)) continue;

        NkDeviceInitInfo effectiveInit = init;
        effectiveInit.api = api;
        if (effectiveInit.context.api == NkGraphicsApi::NK_GFX_API_NONE)
            effectiveInit.context.api = api;

        NkIDevice* dev = CreateForApi(api, effectiveInit);
        if (dev && dev->IsValid()) {
            // Mise à jour de l'init appelant pour refléter l'API choisie
            init.api = api;
            if (init.context.api == NkGraphicsApi::NK_GFX_API_NONE)
                init.context.api = api;
            logger_src.Infof("[NkDeviceFactory] CreateAutoDetect: API selectionnee = %s\n",
                             NkGraphicsApiName(api));
            return dev;
        }
        // Ce device n'a pas fonctionné — on le détruit proprement avant d'essayer le suivant
        if (dev) { dev->Shutdown(); delete dev; dev = nullptr; }
    }

    logger_src.Infof("[NkDeviceFactory] CreateAutoDetect: aucune API fonctionnelle sur ce materiel\n");
    return nullptr;
}

} // namespace nkentseu
