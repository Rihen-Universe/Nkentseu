#pragma once

#include "NkApplicationConfig.h"
#include "NkLayerStack.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkEntry.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKTime/NkTime.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {

    // =========================================================================
    // Application
    // Classe de base abstraite pour toute application Nkentseu.
    //
    // L'utilisateur :
    //   1. Hérite de cette classe.
    //   2. Implémente les callbacks virtuels.
    //   3. Définit CreateApplication(config) qui retourne son instance.
    //
    // Le framework se charge de la boucle, de la fenêtre, du device RHI
    // et du dispatch des événements.
    //
    // Cycle de vie :
    //   Init → OnInit → OnStart → [boucle] → OnStop → OnShutdown → ~dtor
    // =========================================================================
    class NkApplication {
        public:
            explicit NkApplication(const NkApplicationConfig& config);
            virtual ~NkApplication();

            // ── API publique ──────────────────────────────────────────────────────

            // Initialisation
            bool Init();

            // Démarre et bloque jusqu'à la fermeture.
            void Run();

            // Demande l'arrêt propre de la boucle.
            void Quit();

            // Gestion des couches.
            void PushLayer(NkLayer* layer);
            void PushOverlay(NkOverlay* overlay);

            // Accesseurs.
            NkWindow&         GetWindow()  { return mWindow; }
            NkICommandBuffer*          GetCmd()     { return mCmd; }
            NkIDevice*        GetDevice()  { return mDevice; }
            const NkApplicationConfig& GetConfig() const { return mConfig; }

            // Singleton — accès global à l'application courante.
            static NkApplication& Get() { return *sInstance; }
        protected:
            // ── Callbacks utilisateur ─────────────────────────────────────────────

            // Appelé AVANT la création de la fenêtre et du device.
            // Surcharger pour modifier mConfig si besoin.
            virtual void OnPreInit() {}

            // Appelé APRÈS la création de la fenêtre et du device.
            virtual void OnInit() {}

            // Appelé une seule fois avant la première frame.
            virtual void OnStart() {}

            // Appelé chaque frame — logique applicative.
            virtual void OnUpdate(float dt) { (void)dt; }

            // Appelé à intervalle fixe — physique, réseaux.
            virtual void OnFixedUpdate(float fixedDt) { (void)fixedDt; }

            // Appelé après tous les updates — soumission des commandes GPU.
            virtual void OnRender() {}

            // Appelé après OnRender() — UI applicative (NKUI / overlay debug).
            virtual void OnUIRender() {}

            // Appelé quand la fenêtre demande à être fermée.
            // Par défaut appelle Quit(). Surcharger pour ajouter une confirmation.
            virtual void OnClose() { Quit(); }

            // Appelé à chaque redimensionnement.
            virtual void OnResize(nk_uint32 width, nk_uint32 height) {
                (void)width; (void)height;
            }

            // Appelé avant la destruction — libération des ressources applicatives.
            virtual void OnShutdown() {}

            // ── Données membres protégées ─────────────────────────────────────────
            NkApplicationConfig mConfig;
            NkWindow            mWindow;
            NkIDevice*          mDevice  = nullptr;
            NkICommandBuffer*   mCmd     = nullptr;
            NkLayerStack          mLayerStack;
            bool                mRunning = false;

        private:
            // ── Implémentation interne ────────────────────────────────────────────

            bool InitPlatform();
            bool InitDevice();
            void ShutdownDevice();
            void ShutdownPlatform();

            void DispatchEvent(NkEvent* event);

            // Rendu d'une frame complète : logique layers → render layers → submit.
            void RenderFrame(float dt, float fixedDt);

            // Propage aux layers (droite → gauche, consommation possible).
            void DispatchToLayers(NkEvent* event);

            // ── Handlers d'événements internes ───────────────────────────────────
            bool OnWindowClose(NkWindowCloseEvent* e);
            bool OnWindowResize(NkWindowResizeEvent* e);

            // ── État boucle ───────────────────────────────────────────────────────
            float     mAccumulator  = 0.0f;
            nk_uint32 mWidth        = 0;
            nk_uint32 mHeight       = 0;

            static NkApplication* sInstance;
    };

} // namespace nkentseu
