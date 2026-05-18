#pragma once
// =============================================================================
// GameplayScene.h
// -----------------------------------------------------------------------------
// Boucle de jeu 1v1 local — reproduction fidele du mockup HTML
// (docs/02_terrain_de_jeu_1v1.html).
//
// Specs HTML respectees a l'identique (valeurs en pixels par frame a 60 fps,
// converties en pixels/seconde via dt*60 cote C++) :
//   - Balle : vx=3.5, vy=2.2 (init), rayon 7, trail 14 positions
//   - Paddles : 10 x 60 px, vitesse 4
//   - Bounce paddle : vy = rel * 4.5 ; speed ×1.04 capped a 9
//   - Goal : cooldown 800 ms, reset vx=±3.5, vy=(rnd-0.5)*4
//   - Layout : HUD top 56 px, HUD bottom 52 px, terrain entre les deux
//   - Couleurs : P1 cyan #00F5FF, P2 orange #FF6B00
//
// Controles (clavier) :
//   - W / S         : paddle gauche (P1) haut / bas
//   - Fleche haut/bas : paddle droite (P2)
//   - Espace        : pause / reprise
//   - Echap         : si paused -> retour menu (Pop), sinon -> pause
//
// PAS d'obstacles / power-ups / particules dans cette tranche minimale.
// =============================================================================

#include "Pong/UI/Scene.h"
#include "NKMath/NkColor.h"

namespace nkentseu
{
    namespace pong
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
            void OnPause (AppContext& ctx) override;

        private:
            // ── Scale UI/gameplay ───────────────────────────────────────────
            // Calcule a chaque frame depuis le viewport via UIScale.h. Toutes
            // les dimensions HTML (paddle, balle, vitesses, HUD) sont
            // multipliees par mScale pour rester visuellement coherent sur
            // n'importe quel ecran (desktop ou mobile haute densite).
            float mScale  = 1.0f;
            float mHUDTopH = 56.0f;  // = kHUDTopH * mScale, recalcule
            float mHUDBotH = 52.0f;

            // ── Etat balle ──────────────────────────────────────────────────
            // Vitesses stockees en "px par frame a 60 fps" * mScale (= valeurs
            // HTML mises a l'echelle). On applique pos += vel * (dt * 60) pour
            // conserver le comportement exact du mockup peu importe le
            // framerate.
            float mBallX = 0.0f, mBallY = 0.0f;
            float mBallVX = 0.0f, mBallVY = 0.0f;
            float mBallR  = 7.0f;  // = 7 * mScale
            // Trail circulaire — 14 positions (taille HTML).
            static constexpr int kTrailMax = 14;
            float mTrailX[kTrailMax] = {0};
            float mTrailY[kTrailMax] = {0};
            int   mTrailHead   = 0;   ///< prochain index a ecrire
            int   mTrailCount  = 0;   ///< nombre de positions valides

            // ── Paddles ─────────────────────────────────────────────────────
            // x,y = coin haut-gauche. w,h = taille (deja multipliee par mScale).
            float mPaddleLX = 0.0f, mPaddleLY = 0.0f;
            float mPaddleRX = 0.0f, mPaddleRY = 0.0f;
            float mPaddleW  = 10.0f, mPaddleH = 60.0f;  // * mScale
            float mPaddleSpd = 4.0f;                    // * mScale

            // ── Score / etat match ──────────────────────────────────────────
            int   mScoreL = 0;
            int   mScoreR = 0;
            bool  mPaused = false;
            float mGoalCooldown = 0.0f;  // secondes restantes
            float mGoalFlashAlpha = 0.0f;
            int   mGoalFlashSide  = 0;   // -1=left scored, +1=right scored

            // Timer descendant 02:34 -> 0
            float mTimeLeft = 154.0f;

            // ── Etat input (touches tenues) ─────────────────────────────────
            bool mKeyW = false, mKeyS = false;
            bool mKeyUp = false, mKeyDown = false;

            // ── Touch / souris (drag vertical par moitie) ────────────────────
            long long mTouchIdL    = -1;
            long long mTouchIdR    = -1;
            bool      mMouseDownL  = false;
            bool      mMouseDownR  = false;
            float     mLastTouchYL = 0.0f;
            float     mLastTouchYR = 0.0f;
            float     mLastMouseY  = 0.0f;

            // ── Geometrie des boutons cliquables (sync chaque frame) ─────────
            // Pour le bouton PAUSE dans le HUD + boutons de l'overlay pause.
            // Permet le hit-test dans OnEvent sans recalculer le layout.
            float mPauseBtnX  = 0.0f, mPauseBtnY  = 0.0f;
            float mPauseBtnW  = 0.0f, mPauseBtnH  = 0.0f;
            float mResumeBtnX = 0.0f, mResumeBtnY = 0.0f;
            float mResumeBtnW = 0.0f, mResumeBtnH = 0.0f;
            float mMenuBtnX   = 0.0f, mMenuBtnY   = 0.0f;
            float mMenuBtnW   = 0.0f, mMenuBtnH   = 0.0f;

            // Helper : hit-test rectangle.
            static bool PointInRect(float px, float py,
                                    float rx, float ry, float rw, float rh)
            {
                return px >= rx && px <= rx + rw && py >= ry && py <= ry + rh;
            }

            // ── Helpers ─────────────────────────────────────────────────────
            void ResetPositions(float arenaW, float arenaH);
            void TriggerGoal(int side, float arenaW, float arenaH);
            void StepBall   (float arenaW, float arenaH, float dt60);
            void StepPaddles(float arenaH, float dt60);
        };

    } // namespace pong
} // namespace nkentseu
