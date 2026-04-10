// =============================================================================
// NkRendererFactory.cpp
// =============================================================================
#include "NkRendererFactory.h"
#include "NKLogger/NkLog.h"

// Inclure les implémentations concrètes selon les backends compilés
#include "NKRenderer/Backend/NkOpenGLRenderer.h"

#ifdef NK_RHI_VK_ENABLED
#include "NKRenderer/Backend/NkVulkanRenderer.h"
#endif
#ifdef NK_RHI_DX11_ENABLED
#include "NKRenderer/Backend/NkDX11Renderer.h"
#endif
#ifdef NK_RHI_DX12_ENABLED
#include "NKRenderer/Backend/NkDX12Renderer.h"
#endif

namespace nkentseu {
namespace renderer {

    NkIRenderer* NkRendererFactory::Create(NkIDevice* device) {
        if (!device || !device->IsValid()) {
            logger_src.Infof("[NKRenderer] NkRendererFactory::Create — device invalide\n");
            return nullptr;
        }
        return CreateForApi(device, device->GetApi());
    }

    NkIRenderer* NkRendererFactory::CreateForApi(NkIDevice* device, NkGraphicsApi api) {
        if (!device) return nullptr;

        NkIRenderer* renderer = nullptr;

        switch (api) {
            case NkGraphicsApi::NK_API_OPENGL:
            case NkGraphicsApi::NK_API_OPENGLES:
            case NkGraphicsApi::NK_API_SOFTWARE:
                renderer = new NkOpenGLRenderer(device);
                break;

#ifdef NK_RHI_VK_ENABLED
            case NkGraphicsApi::NK_API_VULKAN:
                renderer = new NkVulkanRenderer(device);
                break;
#endif

#ifdef NK_RHI_DX11_ENABLED
            case NkGraphicsApi::NK_API_DIRECTX11:
                renderer = new NkDX11Renderer(device);
                break;
#endif

#ifdef NK_RHI_DX12_ENABLED
            case NkGraphicsApi::NK_API_DIRECTX12:
                renderer = new NkDX12Renderer(device);
                break;
#endif

            default:
                logger_src.Infof("[NKRenderer] API non supportée par NKRenderer: %s — fallback OpenGL\n",
                                  NkGraphicsApiName(api));
                renderer = new NkOpenGLRenderer(device);
                break;
        }

        if (!renderer) return nullptr;

        if (!renderer->Initialize()) {
            logger_src.Infof("[NKRenderer] Initialize() a échoué pour l'API: %s\n",
                              NkGraphicsApiName(api));
            delete renderer;
            return nullptr;
        }

        logger_src.Infof("[NKRenderer] Renderer créé pour: %s\n", NkGraphicsApiName(api));
        return renderer;
    }

    void NkRendererFactory::Destroy(NkIRenderer*& renderer) {
        if (!renderer) return;
        renderer->Shutdown();
        delete renderer;
        renderer = nullptr;
    }

} // namespace renderer
} // namespace nkentseu