#pragma once
// =============================================================================
// GameplayScene.h
// =============================================================================
// Écran de jeu Songoo — plateau Mancala interactif, gestion des coups,
// sélection des pits, synchronisation avec SongooBoard, feedback visuel.
// =============================================================================

#include "Songoo/UI/Scene.h"
#include "Songoo/Game/SongooBoard.h"

namespace nkentseu
{
    namespace songoo
    {

        class GameplayScene : public Scene
        {
            public:
                GameplayScene()  = default;
                ~GameplayScene() override = default;

                const char* Name() const noexcept override { return "Gameplay"; }

                void OnEnter (AppContext& ctx) override;
                void OnUpdate(AppContext& ctx, float dt) override;
                void OnRender(AppContext& ctx) override;
                void OnEvent (AppContext& ctx, NkEvent& ev) override;

            private:
                // ── Logique de jeu ──────────────────────────────────────────
                SongooBoard mBoard;
                int mCurrentPlayer = 0;          // 0 ou 1
                bool mGameOver = false;
                int mWinner = -1;                // -1=P1, +1=P2, 0=tie

                // ── Layout et échelle ────────────────────────────────────────
                float mScale = 1.0f;
                float mBoardX = 0.0f, mBoardY = 0.0f;
                float mBoardW = 0.0f, mBoardH = 0.0f;

                // Géométrie des pits et mancalas
                struct PitGeometry
                {
                    float cx, cy;    // Centre pour rendu + hit-test
                    float radius;
                    int pitIndex;    // 0-13 mapping
                };
                PitGeometry mPitGeo[14];         // 12 regular pits + 2 mancalas

                float mMancalaLX = 0.0f, mMancalaLY = 0.0f;
                float mMancalaRX = 0.0f, mMancalaRY = 0.0f;
                float mMancalaR = 20.0f;

                // Bouton RETOUR
                float mRetourBtnX = 0.0f, mRetourBtnY = 0.0f;
                float mRetourBtnW = 0.0f, mRetourBtnH = 0.0f;

                // ── Interaction et feedback ──────────────────────────────────
                int mHoveredPitIdx = -1;         // Pour highlight au survol
                int mSelectedPitIdx = -1;
                bool mShowInvalidFeedback = false;
                float mInvalidFeedbackTimer = 0.0f;

                // Input
                long long mTouchId = -1;
                bool mMouseDown = false;
                float mLastMouseX = 0.0f, mLastMouseY = 0.0f;

                // ── Animation ────────────────────────────────────────────────
                float mTime = 0.0f;              // Accumulé pour pulsing
                float mEnterAnim = 0.0f;         // Fade-in entrée (0→1)
                float mTurnChangeAnim = 0.0f;   // Highlight transition tour
                float mGrainDistAnim = -1.0f;   // Animation distribution grains (0→duration)
                int mLastMovedPitIdx = -1;      // Pour animation tracking
                int mDistributionGrains = 0;    // Nombre de grains distribués
                int mDistributionPitsTrace[14]; // Trace des pits visités
                int mDistributionTraceLen = 0;  // Longueur de la trace

                // ── Helpers ──────────────────────────────────────────────────
                void ComputeLayout(AppContext& ctx);
                void ComputePitGeometries();
                int HitTestPit(float px, float py) const;
                void TryExecuteMove(AppContext& ctx, int pitIdx);
                void DrawBoard(AppContext& ctx);
                void DrawPit(AppContext& ctx, int pitIdx);
                void DrawMancala(AppContext& ctx, int side);
                void DrawHeader(AppContext& ctx);
                void DrawTurnIndicator(AppContext& ctx);
                void DrawPitFeedback(AppContext& ctx);
                void DrawInvalidFeedback(AppContext& ctx);
                void DrawDistributionAnimation(AppContext& ctx);
        };

    } // namespace songoo
} // namespace nkentseu
