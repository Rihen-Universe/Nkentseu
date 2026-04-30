#pragma once

/**
 * @File    NkUIMultiViewport.h
 * @Brief   Système multi-viewport pour NkUI — fenêtres OS natives ou overlays flottants.
 *
 * COMPORTEMENT :
 *
 *   Mode ACTIVÉ (enableMultiViewport = true) :
 *     Quand une NkUIWindowState dockée sort du viewport principal (pos hors des
 *     bounds [0, viewW] x [0, viewH]), elle devient un "viewport secondaire" :
 *     - Une vraie fenêtre OS (NkWindow) est créée
 *     - Elle a son propre NkUINKRHIBackend (swapchain dédié)
 *     - Elle reçoit ses propres événements et les injecte dans NkUIInputState
 *     - La fenêtre flottante NkUI est rendue dans ce viewport secondaire
 *
 *   Mode DÉSACTIVÉ (enableMultiViewport = false) :
 *     Comportement standard — les fenêtres flottantes restent des overlays
 *     dans le viewport principal, même hors des bounds visuels.
 *
 * USAGE :
 *
 *   // Initialisation
 *   NkUIMultiViewportManager mvp;
 *   mvp.Init(device, swapchainRenderPass, api, &mainWindow, ctx.viewW, ctx.viewH);
 *   mvp.SetEnabled(true); // activer/désactiver à chaud
 *
 *   // Boucle principale (avant BeginFrame UI)
 *   mvp.BeginFrame(ctx, wm, input, dt);
 *
 *   // Rendu (après ctx.EndFrame())
 *   mvp.Submit(ctx, wm); // rend dans chaque fenêtre OS secondaire
 *
 *   // Nettoyage
 *   mvp.Destroy();
 *
 * CONTRAINTES :
 *   - NkWindow::Create() doit être capable de créer plusieurs fenêtres
 *     (NkAppData::enableMultiWindow = true dans NkInitialise)
 *   - Chaque viewport secondaire a son propre contexte GPU (device partagé)
 *   - Max 16 viewports secondaires simultanés
 */

#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKUI/NKUI.h"
#include "NkUINKRHIBackend.h"
#include <memory>

namespace nkentseu {
    namespace nkui {

        // ─────────────────────────────────────────────────────────────────────
        //  NkUIViewportState — un viewport secondaire (fenêtre OS + backend)
        // ─────────────────────────────────────────────────────────────────────

        struct NkUIViewportState {
            // Fenêtre OS native
            NkWindow              osWindow;

            // Backend GPU dédié à cette fenêtre
            NkUINKRHIBackend      backend;

            // ID de la NkUIWindowState associée (dans le NkUIWindowManager)
            NkUIID                windowId    = NKUI_ID_NONE;

            // Commande buffer pour le rendu dans cette fenêtre
            NkICommandBuffer*     cmd         = nullptr;

            // Taille courante (mise à jour sur resize)
            uint32                width       = 0;
            uint32                height      = 0;

            // Vrai quand la fenêtre est valide et ouverte
            bool                  alive       = false;

            // Input isolé pour cette fenêtre (souris relative à la fenêtre OS)
            NkUIInputState        localInput;
        };

        // ─────────────────────────────────────────────────────────────────────
        //  NkUIMultiViewportManager
        // ─────────────────────────────────────────────────────────────────────

        class NkUIMultiViewportManager {
        public:
            static constexpr int32 MAX_VIEWPORTS = 16;

            // ── Initialisation ────────────────────────────────────────────────
            /**
             * @param device            Device GPU partagé (swapchain par fenêtre créé dedans)
             * @param mainRenderPass    RenderPass du swapchain principal (template pour les secondaires)
             * @param api               API graphique courante
             * @param mainWindow        Pointeur vers la fenêtre principale (pour hériter du contexte)
             * @param mainViewW/H       Dimensions du viewport principal
             */
            bool Init(NkIDevice*          device,
                      NkRenderPassHandle  mainRenderPass,
                      NkGraphicsApi       api,
                      NkWindow*           mainWindow,
                      int32               mainViewW,
                      int32               mainViewH) noexcept;

            void Destroy() noexcept;

            // ── Activation ────────────────────────────────────────────────────
            void SetEnabled(bool enabled) noexcept { mEnabled = enabled; }
            bool IsEnabled()  const noexcept       { return mEnabled; }

            // ── Par frame ─────────────────────────────────────────────────────
            /**
             * BeginFrame :
             *   - Détecte les fenêtres NkUI qui sortent du viewport principal
             *   - Crée des fenêtres OS si nécessaire
             *   - Détruit les viewports secondaires dont la fenêtre NkUI est fermée
             *   - Pump les événements OS des fenêtres secondaires → localInput
             *   - Met à jour mainViewW/H si la fenêtre principale a été redimensionnée
             */
            void BeginFrame(NkUIContext&       ctx,
                            NkUIWindowManager& wm,
                            float32            dt,
                            int32              mainViewW,
                            int32              mainViewH) noexcept;

            /**
             * Submit :
             *   - Pour chaque viewport secondaire vivant, rend le contenu
             *     de la NkUIWindowState associée dans sa fenêtre OS
             *   - Appelle BeginFrame GPU → draw → Present
             */
            void Submit(NkUIContext& ctx, NkUIWindowManager& wm) noexcept;

            // ── Requêtes ──────────────────────────────────────────────────────

            /**
             * Retourne true si windowId est géré par un viewport secondaire.
             * Dans ce cas, ne pas l'inclure dans le rendu principal.
             */
            bool IsSecondaryViewport(NkUIID windowId) const noexcept;

            /**
             * Retourne l'input local du viewport secondaire associé à windowId.
             * Utilisé pour injecter les events souris relatifs à la fenêtre OS.
             * Retourne nullptr si windowId n'est pas un viewport secondaire.
             */
            const NkUIInputState* GetLocalInput(NkUIID windowId) const noexcept;

            int32 GetViewportCount() const noexcept { return mCount; }

        private:
            NkIDevice*          mDevice     = nullptr;
            NkGraphicsApi       mApi        = NkGraphicsApi::NK_GFX_API_OPENGL;
            NkWindow*           mMainWindow = nullptr;
            bool                mEnabled    = false;
            int32               mMainViewW  = 0;
            int32               mMainViewH  = 0;

            // Pool de viewports (max MAX_VIEWPORTS)
            NkUIViewportState   mViewports[MAX_VIEWPORTS] = {};
            int32               mCount = 0;

            // ── Helpers ───────────────────────────────────────────────────────

            // Crée un viewport secondaire pour une NkUIWindowState flottante
            bool CreateViewport(NkUIID windowId, NkUIWindowManager& wm) noexcept;

            // Détruit un viewport secondaire (index dans mViewports)
            void DestroyViewport(int32 idx) noexcept;

            // Trouve le viewport par windowId (-1 si non trouvé)
            int32 FindViewport(NkUIID windowId) const noexcept;

            // Teste si une fenêtre NkUI est hors du viewport principal
            bool IsOutsideMainViewport(const NkUIWindowState& ws) const noexcept;

            // Synchronise pos/size NkUIWindowState ↔ fenêtre OS
            void SyncWindowToOS(NkUIViewportState& vp, NkUIWindowState& ws) noexcept;
            void SyncOSToWindow(NkUIViewportState& vp, NkUIWindowState& ws) noexcept;
        };

    } // namespace nkui
} // namespace nkentseu