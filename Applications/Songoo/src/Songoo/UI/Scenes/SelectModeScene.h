#pragma once
// =============================================================================
// SelectModeScene.h
// -----------------------------------------------------------------------------
// Ecran intercale entre MainMenu et GameplayScene quand l'utilisateur clique
// JOUER. Propose les 4 modes du GDD §2 :
//   1. Local 1v1  — meme clavier, 2 joueurs
//   2. Reseau     — 1v1 LAN/Internet (TODO : NkNetwork)
//   3. vs IA      — 1 joueur contre AIController
//   4. IA vs IA   — demo automatique, 2 AIController
//
// Apres choix, on positionne `ctx.settings->mode` et on Push GameplayScene.
// Pour les modes non encore implémentés (Reseau), on log et on ne lance pas.
//
// Navigation : fleches gauche/droite, ENTER active, ESC retour menu, tap/clic
// active la card visee.
// =============================================================================

#include "Songoo/UI/Scene.h"
#include "Songoo/Game/GameTypes.h"

namespace nkentseu
{
    namespace songoo
    {

        class SelectModeScene : public Scene
        {
        public:
            // 4 modes ordonnes (correspondent aux GameMode). PUBLIC car
            // referencer depuis tables namespace dans le .cpp.
            static constexpr int kModeCount = 4;

            SelectModeScene()  = default;
            ~SelectModeScene() override = default;

            const char* Name() const noexcept override { return "SelectMode"; }

            void OnEnter (AppContext& ctx) override;
            void OnUpdate(AppContext& ctx, float dt) override;
            void OnRender(AppContext& ctx) override;
            void OnEvent (AppContext& ctx, NkEvent& ev) override;

        private:

            float mTime       = 0.0f;
            int   mFocusIndex = 0;     ///< 0..3 (Local/Reseau/vsIA/IAvsIA)
            float mEnterAnim  = 0.0f;

            // Geometrie cards (sync chaque frame pour hit-test)
            float mCardYs[kModeCount] = { 0, 0, 0, 0 };
            float mCardXs[kModeCount] = { 0, 0, 0, 0 };
            float mCardW = 0.0f;
            float mCardH = 0.0f;

            // Bouton RETOUR
            float mBackX = 0.0f, mBackY = 0.0f;
            float mBackW = 0.0f, mBackH = 0.0f;

            // Actions
            void ActivateMode(AppContext& ctx, int index);
            int  HitTestCard(float px, float py) const;
            bool HitTestBack(float px, float py) const;
        };

    } // namespace songoo
} // namespace nkentseu
