#pragma once
// =============================================================================
// PongApp.h
// -----------------------------------------------------------------------------
// Application principale Pong Ultra Arena.
// Architecture : un SceneManager pilote toutes les scenes (intros, splash,
// menu, gameplay). PongApp possede les sous-systemes (GLContext, renderer,
// font atlas, settings) et les expose via AppContext.
// =============================================================================

#include "Pong/Game/GameTypes.h"
#include "Pong/Render/GLContext.h"
#include "Pong/Render/GLRenderer2D.h"
#include "Pong/Render/FontAtlas.h"
#include "Pong/Net/NetworkSession.h"
#include "Pong/Net/NetworkDiscovery.h"
#include "Pong/Audio/AudioManager.h"
#include "Pong/UI/AppContext.h"
#include "Pong/UI/SceneManager.h"
#include "NKCore/NkTypes.h"

namespace nkentseu
{
    class NkWindow;
    class NkEvent;
}

namespace nkentseu
{
    namespace pong
    {

        class PongApp
        {
        public:
            explicit PongApp(NkWindow& window) noexcept : mWindow(window) {}
            ~PongApp() = default;

            // ── Lifecycle ────────────────────────────────────────────────────
            /// Initialise GL, atlas, scenes (push RihenIntroScene).
            bool Init();
            /// Libere toutes les ressources.
            void Shutdown();

            /// Gere le redimensionnement de la fenetre.
            void OnResize(uint32 w, uint32 h);

            /// Avance la logique d'un delta-time.
            void Update(float dt);
            /// Dessine la scene active.
            void Render();
            /// Dispatch d'un event poll-e a la scene active.
            void OnEvent(NkEvent& ev);
            /// Lifecycle : informer la scene active.
            void OnPause();
            void OnResume();
            /// Recree la surface GL (apres APP_CMD_INIT_WINDOW Android).
            bool RecreateSurface();

            // ── Accesseurs ───────────────────────────────────────────────────
            bool WantsQuit() const noexcept { return mQuit; }
            void RequestQuit() noexcept     { mQuit = true; }

            /// Etat helper pour Apps.cpp (auto-pause au focus lost).
            GameState State() const noexcept;
            void      SetState(GameState s) noexcept;

        private:
            NkWindow&       mWindow;
            GLContext       mGL;
            GLRenderer2D    mRenderer;
            FontAtlas       mFont;
            GameSettings    mSettings;
            SceneManager    mScenes;
            NetworkSession  mNetwork;
            // Decouverte LAN (beacon UDP + scan). Ticke chaque frame depuis
            // Update(). Cf [[pong_multijoueur_internet_strategy]] phase C.
            NetworkDiscovery mDiscovery;
            // Gestionnaire audio NKAudio : pre-charge des samples + helpers
            // PlayPaddle/PlayScore/etc. Init dans PongApp::Init.
            AudioManager     mAudio;

            bool         mQuit       = false;
            float        mTime       = 0.0f;
            uint32       mViewportW  = 0;
            uint32       mViewportH  = 0;

            // ── Identite reseau Pays/Ville + Code ──────────────────────────
            // Generee une fois dans Init() via africa::PickRandomCountryCityCode.
            // Visible dans le NetworkLobbyScene et envoyee aux pairs via le
            // handshake (PktHello). Constants pour toute la duree de l'app
            // (l'user peut relancer l'app pour avoir un nouveau pseudo).
            // Format affichable : "Cameroun/Douala-123456789".
            char         mMyCountry[32] = { 0 };
            char         mMyCity[32]    = { 0 };
            char         mMyCode[16]    = { 0 };   ///< 9 chiffres zero-padded

            // Construit un AppContext frais pour le frame courant.
            AppContext BuildContext();
        };

    } // namespace pong
} // namespace nkentseu
