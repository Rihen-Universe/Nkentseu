#pragma once
// =============================================================================
// SplashScene.h
// =============================================================================
// Ecran de splash Songoo : affiche le logo, intro rapide au Mancala.
//   - Logo SONGOO avec shadow orange pulsé
//   - Sous-titre "Joyau d'Afrique"
//   - Badge "* MANCALA *" orange
//   - Aperçu animé du plateau de Mancala (pits + grains)
//   - CTA "APPUYER POUR JOUER" cyan blink
//   - Credits mentionnant Songoo + Rihen
//
// Transition Replace() vers MainMenuScene après 4s OU au toucher.
// =============================================================================

#include "Songoo/UI/Scene.h"

namespace nkentseu
{
    namespace songoo
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

    } // namespace songoo
} // namespace nkentseu
