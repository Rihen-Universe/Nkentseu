#pragma once
// NkIGraphicsContext.h — Interface abstraite contexte graphique + compute
#include "NkContextDesc.h"
#include "NkContextInfo.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKCore/NkTypes.h"
#include "NKContainers/Functional/NkFunction.h"

namespace nkentseu {

    class NkWindow;

    // =========================================================================
    // Callbacks de recréation de swapchain — inspirés de l'ancien VulkanContext
    // (m_CleanList / m_RecreateList). Utilise NkFunction (custom, sans STL).
    //   AddCleanUpCallback  → appelé AVANT la destruction (détruire pipelines, etc.)
    //   AddRecreateCallback → appelé APRÈS la recréation (reconstruire pipelines, etc.)
    // Retourne un handle pour pouvoir supprimer le callback plus tard.
    // =========================================================================
    using NkSwapchainCallbackHandle = uint32;
    static constexpr NkSwapchainCallbackHandle NK_INVALID_CALLBACK_HANDLE = 0;
    using NkSwapchainCleanFn    = NkFunction<void()>;   // appel sans argument — supporte lambdas et captures
    using NkSwapchainRecreateFn = NkFunction<void()>;

    class NkIGraphicsContext {
        public:
            virtual ~NkIGraphicsContext() = default;

            // Cycle de vie
            virtual bool Initialize(const NkWindow& window, const NkContextDesc& desc) = 0;
            virtual void Shutdown()  = 0;
            virtual bool IsValid()   const = 0;

            // Boucle de rendu
            virtual bool BeginFrame() = 0;
            virtual void EndFrame()   = 0;
            virtual void Present()    = 0;

            // Couleur de clear (0..1) utilisee par BeginFrame pour le render pass.
            // No-op par defaut : OpenGL/DX clearent via le renderer (glClearColor /
            // ClearRenderTargetView). Vulkan/Software, eux, clearent dans BeginFrame
            // (render pass loadOp / back-buffer CPU) et ont besoin de cette couleur
            // AVANT BeginFrame -> NkRenderWindow::Clear l'appelle. Sans ca, leur fond
            // reste une couleur en dur (gris) au lieu de celle demandee par la scene.
            virtual void SetClearColor(float /*r*/, float /*g*/, float /*b*/, float /*a*/) {}

            // Rend le contexte courant pour le thread appelant (no-op sur les backends non-GL)
            virtual bool MakeCurrent()    { return true; }
            virtual void ReleaseCurrent() {}

            // Surface
            virtual bool OnResize(uint32 width, uint32 height) = 0;
            virtual void SetVSync(bool enabled) = 0;
            virtual bool GetVSync() const = 0;

            // Callbacks de recréation de swapchain (défaut : no-op pour les backends sans swapchain)
            virtual NkSwapchainCallbackHandle AddCleanUpCallback(NkSwapchainCleanFn fn)    { return NK_INVALID_CALLBACK_HANDLE; }
            virtual NkSwapchainCallbackHandle AddRecreateCallback(NkSwapchainRecreateFn fn){ return NK_INVALID_CALLBACK_HANDLE; }
            virtual void RemoveCleanUpCallback(NkSwapchainCallbackHandle)    {}
            virtual void RemoveRecreateCallback(NkSwapchainCallbackHandle)   {}

            // Infos
            virtual NkGraphicsApi GetApi()  const = 0;
            virtual NkContextInfo GetInfo() const = 0;
            virtual NkContextDesc GetDesc() const = 0;

            // Accès natif opaque — caster via NkNativeContext::XXX()
            virtual void* GetNativeContextData() = 0;

            // Compute
            virtual bool SupportsCompute() const = 0;
    };

    using NkGraphicsContextPtr = memory::NkUniquePtr<NkIGraphicsContext>;

} // namespace nkentseu
