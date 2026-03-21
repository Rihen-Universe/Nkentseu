#pragma once
// =============================================================================
// NkISwapchain.h
// Interface abstraite du swapchain — gestion de la surface de présentation.
//
// Le swapchain est séparé de NkIDevice car il a son propre cycle de vie :
//   - Dépend de la fenêtre (NkIGraphicsContext)
//   - Doit être recréé sur resize ou changement de format
//   - Ses images sont acquises frame par frame
//
// Pattern d'utilisation typique :
//   NkISwapchain* sc = device->CreateSwapchain(ctx, desc);
//   ...
//   // Par frame :
//   sc->AcquireNextImage(imageAvailableSem, {});
//   // ... enregistrer commandes ...
//   device->SubmitOnQueue(NkQueueType::NK_GRAPHICS, submitInfo);
//   sc->Present(&renderFinishedSem, 1);
// =============================================================================
#include "NKRenderer/RHI/Core/NkRhiDescs.h"
#include "NKRenderer/Context/Core/NkIGraphicsContext.h"

namespace nkentseu {

// =============================================================================
// NkISwapchain
// =============================================================================
class NkISwapchain {
public:
    virtual ~NkISwapchain() = default;

    // ── Cycle de vie ─────────────────────────────────────────────────────────
    virtual bool Initialize(NkIGraphicsContext* ctx,
                             const NkSwapchainDesc& desc)    = 0;
    virtual void Shutdown()                                   = 0;
    virtual bool IsValid()                             const  = 0;

    // ── Frame — acquisition et présentation ──────────────────────────────────
    // Acquiert la prochaine image disponible.
    // signalSemaphore : signalé quand l'image est prête à être dessinée.
    // fence           : optionnel, signalé à la même occasion.
    // Retourne false si le swapchain doit être recréé (window minimized, etc.)
    virtual bool AcquireNextImage(NkSemaphoreHandle signalSemaphore,
                                   NkFenceHandle fence = NkFenceHandle::Null(),
                                   uint64 timeoutNs = UINT64_MAX)           = 0;

    // Présente l'image courante.
    // waitSemaphores : signaux GPU à attendre avant de présenter
    // Retourne false si swapchain out-of-date (déclencher Resize)
    virtual bool Present(const NkSemaphoreHandle* waitSemaphores,
                          uint32 waitCount)                                  = 0;

    // Raccourci sans semaphore (pour prototypage / backend SW)
    bool Present() { return Present(nullptr, 0); }

    // ── Resize — à appeler sur WM_SIZE / SDL_WINDOWEVENT_RESIZED ─────────────
    // Recrée les images et framebuffers internes.
    virtual void Resize(uint32 width, uint32 height)                         = 0;

    // ── Accesseurs ────────────────────────────────────────────────────────────
    virtual NkFramebufferHandle GetCurrentFramebuffer()  const               = 0;
    virtual NkRenderPassHandle  GetCurrentRenderPass()   const               = 0;
    virtual NkGpuFormat            GetColorFormat()         const               = 0;
    virtual NkGpuFormat            GetDepthFormat()         const               = 0;
    virtual uint32              GetWidth()               const               = 0;
    virtual uint32              GetHeight()              const               = 0;
    virtual uint32              GetCurrentImageIndex()   const               = 0;
    virtual uint32              GetImageCount()          const               = 0;

    // ── Infos de capacité ────────────────────────────────────────────────────
    virtual bool SupportsHDR()                           const { return false; }
    virtual bool SupportsTearing()                       const { return false; }
};

} // namespace nkentseu
