#pragma once
// =============================================================================
// NogeIntroScene.h
// -----------------------------------------------------------------------------
// Deuxieme ecran d'intro : presentation du moteur "NOGE" (Nkentseu Open Game
// Engine — interpretation libre puisque le logo n'est pas defini).
//
// Design propre, neon, style "powered by" :
//   - Hexagone neon dessine progressivement (style logo gaming)
//   - Texte "POWERED BY" subtil en haut
//   - "NOGE" en grand juste apres
//   - "Game Engine" en sous-titre cyan
//   - Fade in / hold / fade out (~3 secondes)
//
// A la fin, transition Replace() vers SplashScene.
// =============================================================================

#include "Songoo/UI/Scene.h"
#include "Songoo/Render/Texture2D.h"

namespace nkentseu
{
    namespace songoo
    {

        class NogeIntroScene : public Scene
        {
        public:
            NogeIntroScene()  = default;
            ~NogeIntroScene() override = default;

            const char* Name() const noexcept override { return "NogeIntro"; }

            void OnEnter(AppContext& ctx) override;
            void OnExit(AppContext& ctx) override;
            void OnUpdate(AppContext& ctx, float dt) override;
            void OnRender(AppContext& ctx) override;

        private:
            float     mTime = 0.0f;
            bool      mDone = false;
            // Logo Rihen affiche statique en bas de l'ecran (signature
            // designer). Charge depuis Resources/Pong/Textures/logo.png.
            Texture2D mRihenLogo;
            bool      mRihenLogoLoaded = false;
        };

    } // namespace songoo
} // namespace nkentseu
