#pragma once
// =============================================================================
// PongApp.h — Application principale Pong Ultra Arena
// Etape 1 (PoC) : init OpenGL via GLContext + boucle minimale (glClear).
// Etape 2+      : ajouter le rendu reel (GLRenderer, Font atlas, scenes UI).
// =============================================================================

#include "Pong/Game/GameTypes.h"
#include "Pong/Render/GLContext.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
    class NkWindow;
}

namespace nkentseu { namespace pong {

    class PongApp {
        public:
            explicit PongApp(NkWindow& window) noexcept : mWindow(window) {}
            ~PongApp() = default;

            bool Init();
            void Shutdown();

            void OnResize(uint32 w, uint32 h);

            // Boucle de jeu (appelee chaque frame depuis Apps.cpp)
            void Update(float dt);
            void Render();

            bool WantsQuit() const noexcept { return mQuit; }

            GameState State() const noexcept { return mState; }
            void      SetState(GameState s) noexcept { mState = s; }

        private:
            NkWindow&    mWindow;
            GLContext    mGL;
            GameSettings mSettings;
            GameState    mState = GameState::SplashScreen;
            bool         mQuit  = false;

            float        mTime          = 0.0f;
            float        mSplashTimer   = 2.4f;

            uint32       mViewportW = 0;
            uint32       mViewportH = 0;

            // PoC : Render() ne fait qu'un glClear anime selon l'etat
            void RenderClear(float r, float g, float b);
    };

}} // namespace nkentseu::pong
