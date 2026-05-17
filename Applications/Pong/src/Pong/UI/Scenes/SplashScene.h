#pragma once
// =============================================================================
// SplashScene.h
// -----------------------------------------------------------------------------
// Troisieme et dernier ecran d'intro : splash conforme a docs/01_splash_et_menus.html.
//   - Logo PONG (display 72px) avec text-shadow cyan pulsé
//   - Sous-titre "ULTRA ARENA EDITION"
//   - Badge "* 1 VS 1 *" orange outline
//   - Mini-field anime (paddles cyan/orange + balle qui rebondit)
//   - CTA "APPUYER POUR JOUER" cyan blink
//   - Credits bas mentionnant RIHEN
//
// Transition Replace() vers MainMenuScene quand l'utilisateur appuie sur
// n'importe quelle touche/clic OU apres 4 secondes auto-advance.
// =============================================================================

#include "Pong/UI/Scene.h"

namespace nkentseu
{
    namespace pong
    {

        class SplashScene : public Scene
        {
        public:
            SplashScene()  = default;
            ~SplashScene() override = default;

            const char* Name() const noexcept override { return "Splash"; }

            void OnEnter(AppContext& ctx) override;
            void OnUpdate(AppContext& ctx, float dt) override;
            void OnRender(AppContext& ctx) override;

        private:
            float mTime  = 0.0f;
            float mTimer = 4.0f;   // auto-advance apres 4s
        };

    } // namespace pong
} // namespace nkentseu
