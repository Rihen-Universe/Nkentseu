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

            // ── Accesseurs ───────────────────────────────────────────────────
            bool WantsQuit() const noexcept { return mQuit; }
            void RequestQuit() noexcept     { mQuit = true; }

            /// Etat helper pour Apps.cpp (auto-pause au focus lost).
            GameState State() const noexcept;
            void      SetState(GameState s) noexcept;

        private:
            NkWindow&    mWindow;
            GLContext    mGL;
            GLRenderer2D mRenderer;
            FontAtlas    mFont;
            GameSettings mSettings;
            SceneManager mScenes;

            bool         mQuit       = false;
            float        mTime       = 0.0f;
            uint32       mViewportW  = 0;
            uint32       mViewportH  = 0;

            // Construit un AppContext frais pour le frame courant.
            AppContext BuildContext();
        };

    } // namespace pong
} // namespace nkentseu
