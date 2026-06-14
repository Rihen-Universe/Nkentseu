#pragma once
// =============================================================================
// GameOverScene.h
// =============================================================================
// Écran de fin de partie Songoo — affichage gagnant, scores finaux,
// boutons REJOUER et RETOUR MENU avec design Afro-warm.
// =============================================================================

#include "Songoo/UI/Scene.h"

namespace nkentseu
{
    namespace songoo
    {

        class GameOverScene : public Scene
        {
            public:
                GameOverScene(int winner, int scoreP1, int scoreP2)
                    : mWinner(winner), mScoreP1(scoreP1), mScoreP2(scoreP2) {}
                ~GameOverScene() override = default;

                const char* Name() const noexcept override { return "GameOver"; }

                void OnEnter (AppContext& ctx) override;
                void OnUpdate(AppContext& ctx, float dt) override;
                void OnRender(AppContext& ctx) override;
                void OnEvent (AppContext& ctx, NkEvent& ev) override;

            private:
                // Résultats
                int mWinner = -1;  // -1=P1, +1=P2, 0=tie
                int mScoreP1 = 0;
                int mScoreP2 = 0;

                // Animation & layout
                float mTime = 0.0f;
                float mEnterAnim = 0.0f;
                float mScale = 1.0f;

                // Boutons
                float mReplayBtnX = 0.0f, mReplayBtnY = 0.0f;
                float mReplayBtnW = 0.0f, mReplayBtnH = 0.0f;
                float mMenuBtnX = 0.0f, mMenuBtnY = 0.0f;
                float mMenuBtnW = 0.0f, mMenuBtnH = 0.0f;

                // Feedback
                int mHoveredBtn = -1;  // -1=none, 0=replay, 1=menu

                // Helpers
                void ComputeLayout(AppContext& ctx);
                int HitTestButton(float px, float py) const;
                void DrawWinnerBanner(AppContext& ctx);
                void DrawScores(AppContext& ctx);
                void DrawButtons(AppContext& ctx);
        };

    } // namespace songoo
} // namespace nkentseu
