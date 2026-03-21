#pragma once
// =============================================================================
// NkContextFactory.h
// Crée et gère les contextes graphiques et compute.
//
// Multi-contexte :
//   - Plusieurs contextes par application : entièrement supporté.
//     Chaque appel à Create() retourne un contexte indépendant.
//     Usage : un contexte de rendu principal + un contexte de chargement
//             + un contexte de preview dans un panneau secondaire.
//
//   - Plusieurs contextes par fenêtre :
//     OpenGL : possible via wglCreateContextAttribsARB avec hShareContext
//              → partage textures/buffers entre contextes (NkOpenGLContext::CreateSharedContext)
//     Vulkan : une surface par fenêtre, plusieurs command buffers OK
//     DX11/12: un swap chain par HWND, plusieurs device contexts OK
//     Metal  : un CAMetalLayer par NSView/UIView, plusieurs command queues OK
//     Cas d'usage : rendu principal + compute async + UI overlay (ImGui séparé)
//
//   - Compute standalone : CreateCompute(api) retourne un NkIComputeContext*
//     sans surface/swapchain (utile pour GPGPU pur).
// =============================================================================
#include "NKContext/Core/NkIGraphicsContext.h"
#include "NKContext/Compute/NkIComputeContext.h"

namespace nkentseu {

    class NkWindow;

    class NkContextFactory {
    public:
        // ── Contexte graphique ────────────────────────────────────────────────
        // Crée et retourne un contexte graphique complet (avec surface/swapchain).
        // L'appelant est propriétaire — d\u00E9truire via delete.
        static NkIGraphicsContext* Create(const NkWindow& window,
                                          const NkContextDesc& desc);

        // Vérifie si une API est disponible sur la plateforme courante
        static bool IsApiSupported(NkGraphicsApi api);

        // Chaîne de fallback : essaie les APIs dans l'ordre jusqu'au premier succès.
        // Ordre conseillé : {DX12, DX11, Metal, Vulkan, OpenGL, Software}
        static NkIGraphicsContext* CreateWithFallback(
            const NkWindow& window,
            const NkGraphicsApi* preferenceOrder,
            uint32 preferenceCount);

        // ── Contexte compute standalone ───────────────────────────────────────
        // Crée un contexte compute SANS surface/swapchain.
        // Idéal pour GPGPU pur (ML inference, simulation, etc.).
        // `desc.compute.enable` et le backend ciblé doivent être activés.
        // L'appelant est propriétaire.
        static NkIComputeContext* CreateCompute(NkGraphicsApi api,
                                                 const NkContextDesc& desc = {});

        // Obtient un contexte compute depuis un contexte graphique existant.
        // Partage le device/queue — pas de création de device supplémentaire.
        // Nécessite `gfx->GetDesc().compute.enable == true`.
        static NkIComputeContext* ComputeFromGraphics(NkIGraphicsContext* gfx);
    };

} // namespace nkentseu
