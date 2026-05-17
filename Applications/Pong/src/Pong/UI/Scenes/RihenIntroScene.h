#pragma once
// =============================================================================
// RihenIntroScene.h
// -----------------------------------------------------------------------------
// Premier écran du jeu : intro animée du logo Rihen.
// Source : Resources/Pong/Textures/logo.png (cercle bicolore + texte RIHEN +
// tagline "RÊVONS ENSEMBLE, RÉINVENTONS LE FUTUR").
//
// Animation en 4 phases (~4.5 s au total) :
//   Phase 0 (0.0 - 0.7s)  Trace du cercle (arc qui s'etend)
//   Phase 1 (0.7 - 1.8s)  Apparition lettres RIHEN une par une (stroke reveal)
//   Phase 2 (1.8 - 2.3s)  Fade in tagline
//   Phase 3 (2.3 - 3.5s)  Cross-fade vers le vrai logo bitmap (NkImage)
//   Phase 4 (3.5 - 4.5s)  Hold (logo entierement visible) puis fade out
//
// A la fin de la phase 4, transition Replace() vers NogeIntroScene.
// =============================================================================

#include "Pong/UI/Scene.h"
#include "Pong/Render/Texture2D.h"

namespace nkentseu
{
    namespace pong
    {

        class RihenIntroScene : public Scene
        {
        public:
            RihenIntroScene()  = default;
            ~RihenIntroScene() override = default;

            const char* Name() const noexcept override { return "RihenIntro"; }

            void OnEnter(AppContext& ctx) override;
            void OnUpdate(AppContext& ctx, float dt) override;
            void OnRender(AppContext& ctx) override;
            void OnExit(AppContext& ctx) override;

        private:
            Texture2D mLogo;            ///< Logo bitmap final (charge depuis logo.png)
            float     mTime  = 0.0f;    ///< Temps ecoule depuis OnEnter (secondes)
            bool      mDone  = false;   ///< Demande transition au prochain frame
            bool      mLogoLoaded = false; ///< true si la texture a ete chargee

            // Couleurs reprises du logo (mesurees visuellement)
            // Teal foncé (cercle haut + texte) : ~ (21, 84, 96)
            // Orange (cercle bas) : ~ (243, 152, 15)
        };

    } // namespace pong
} // namespace nkentseu
