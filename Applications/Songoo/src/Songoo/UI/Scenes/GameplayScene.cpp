// =============================================================================
// GameplayScene.cpp
// =============================================================================
// Écran de jeu Songoo — plateau Mancala interactif, gestion des coups,
// sélection des pits, synchronisation avec SongooBoard, feedback visuel.
// =============================================================================

#include "GameplayScene.h"
#include "GameOverScene.h"
#include "MainMenuScene.h"
#include "Songoo/Render/GLRenderer2D.h"
#include "Songoo/Render/FontAtlas.h"
#include "Songoo/UI/Theme.h"
#include "Songoo/UI/UIScale.h"
#include "Songoo/UI/SceneManager.h"
#include "NKLogger/NkLog.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include <cmath>

namespace nkentseu
{
    namespace songoo
    {

        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::OnEnter(AppContext& ctx)
        {
            mBoard.Init();
            mCurrentPlayer = 0;
            mGameOver = false;
            mWinner = -1;

            mTime = 0.0f;
            mEnterAnim = 0.0f;
            mTurnChangeAnim = 0.0f;
            mGrainDistAnim = -1.0f;
            mLastMovedPitIdx = -1;

            mHoveredPitIdx = -1;
            mSelectedPitIdx = -1;
            mShowInvalidFeedback = false;
            mInvalidFeedbackTimer = 0.0f;

            mTouchId = -1;
            mMouseDown = false;
            mLastMouseX = 0.0f;
            mLastMouseY = 0.0f;

            ComputeLayout(ctx);
            logger.Info("[GameplayScene] OnEnter - board initialized");
        }

        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::OnUpdate(AppContext& ctx, float dt)
        {
            mTime += dt;
            mEnterAnim = mEnterAnim + dt * 2.0f;
            if (mEnterAnim > 1.0f) mEnterAnim = 1.0f;

            if (mTurnChangeAnim > 0.0f) mTurnChangeAnim -= dt;

            // Animation distribution : 0.15s par graine distribué
            if (mGrainDistAnim >= 0.0f)
            {
                mGrainDistAnim += dt;
                float duration = mDistributionGrains * 0.15f;
                if (mGrainDistAnim > duration)
                    mGrainDistAnim = -1.0f;  // Animation terminée
            }

            mInvalidFeedbackTimer += dt;

            mCurrentPlayer = mBoard.GetCurrentPlayer();

            if (!mGameOver)
            {
                int winner = mBoard.CheckGameOver();
                if (winner >= 0)
                {
                    mGameOver = true;
                    mWinner = (winner == 0) ? -1 : (winner == 1) ? 1 : 0;
                    logger.Info("[GameplayScene] Game Over - winner: %d", mWinner);

                    // Afficher GameOverScene après 2 secondes de delay
                    if (mTurnChangeAnim < 0.0f)
                    {
                        mTurnChangeAnim = 0.0f;
                    }
                    mTurnChangeAnim += dt;
                    if (mTurnChangeAnim >= 2.0f)
                    {
                        int finalScoreP1 = mBoard.GetMancalaGrains(0);
                        int finalScoreP2 = mBoard.GetMancalaGrains(1);
                        ctx.scenes->Push(new GameOverScene(mWinner, finalScoreP1, finalScoreP2));
                    }
                }
            }

            ComputeLayout(ctx);
        }

        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::OnRender(AppContext& ctx)
        {
            GLRenderer2D& r = *ctx.renderer;
            FontAtlas&    f = *ctx.font;
            const int W = ctx.viewportW;
            const int H = ctx.viewportH;
            const SafeArea& sa = ctx.safe;

            // Fond sombre (brun foncé Afro-warm)
            r.Clear(0.08f, 0.06f, 0.04f, 1.0f);
            r.Begin(W, H);

            // Grille subtile
            const math::NkColor gridC = theme::GridLine();
            math::NkColor faintGrid = gridC;
            faintGrid.a = 30;
            for (int x = 0; x <= W; x += 60)
                r.DrawQuad((float)x, 0.0f, 1.0f, (float)H, faintGrid);
            for (int y = 0; y <= H; y += 60)
                r.DrawQuad(0.0f, (float)y, (float)W, 1.0f, faintGrid);

            DrawHeader(ctx);
            DrawTurnIndicator(ctx);
            DrawBoard(ctx);
            DrawPitFeedback(ctx);

            // Animation de distribution
            if (mGrainDistAnim >= 0.0f)
            {
                DrawDistributionAnimation(ctx);
            }

            if (mShowInvalidFeedback && mInvalidFeedbackTimer < 0.6f)
            {
                DrawInvalidFeedback(ctx);
            }

            r.End();
        }

        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::OnEvent(AppContext& ctx, NkEvent& ev)
        {
            // Clavier : Echap = retour au menu
            if (auto* k = ev.As<NkKeyPressEvent>())
            {
                if (k->GetKey() == NkKey::NK_ESCAPE)
                {
                    ctx.scenes->Pop();
                    return;
                }
                // Sélection pit par touche 1-6
                if (k->GetKey() >= NkKey::NK_NUM1 && k->GetKey() <= NkKey::NK_NUM6)
                {
                    int pitOffset = (int)k->GetKey() - (int)NkKey::NK_NUM1;
                    if (mCurrentPlayer == 0)
                        TryExecuteMove(ctx, pitOffset);
                    else
                        TryExecuteMove(ctx, 12 - pitOffset);
                    return;
                }
                return;
            }

            // Souris : clique sur pit
            if (auto* m = ev.As<NkMouseButtonPressEvent>())
            {
                const float px = m->GetX();
                const float py = m->GetY();

                // Clique sur RETOUR
                if (px >= mRetourBtnX && px <= mRetourBtnX + mRetourBtnW &&
                    py >= mRetourBtnY && py <= mRetourBtnY + mRetourBtnH)
                {
                    ctx.scenes->Pop();
                    return;
                }

                // Hit-test sur pit
                int pitIdx = HitTestPit(px, py);
                if (pitIdx >= 0)
                {
                    TryExecuteMove(ctx, pitIdx);
                    mMouseDown = true;
                    mLastMouseX = px;
                    mLastMouseY = py;
                }
                return;
            }

            // Souris : survol pour focus
            if (auto* m = ev.As<NkMouseMoveEvent>())
            {
                const int pitIdx = HitTestPit(m->GetX(), m->GetY());
                mHoveredPitIdx = pitIdx;
                return;
            }

            // Touch : tap sur pit
            if (auto* t = ev.As<NkTouchEndEvent>())
            {
                if (t->GetNumTouches() > 0)
                {
                    const NkTouchPoint& tp = t->GetTouch(0);

                    // Clique sur RETOUR
                    if (tp.clientX >= mRetourBtnX && tp.clientX <= mRetourBtnX + mRetourBtnW &&
                        tp.clientY >= mRetourBtnY && tp.clientY <= mRetourBtnY + mRetourBtnH)
                    {
                        ctx.scenes->Pop();
                        return;
                    }

                    int pitIdx = HitTestPit(tp.clientX, tp.clientY);
                    if (pitIdx >= 0)
                    {
                        TryExecuteMove(ctx, pitIdx);
                    }
                }
                mTouchId = -1;
                return;
            }

            // Touch : move pour hover
            if (auto* t = ev.As<NkTouchMoveEvent>())
            {
                if (t->GetNumTouches() > 0)
                {
                    const NkTouchPoint& tp = t->GetTouch(0);
                    const int pitIdx = HitTestPit(tp.clientX, tp.clientY);
                    mHoveredPitIdx = pitIdx;
                }
                return;
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::ComputeLayout(AppContext& ctx)
        {
            const int W = ctx.viewportW;
            const int H = ctx.viewportH;
            const SafeArea& sa = ctx.safe;

            mScale = GetUIScale(W, H);

            // Calcul du board : responsive avec aspect ratio 8:2 (horizontal)
            const float safeW = sa.SafeW();
            const float safeH = sa.SafeH();
            const float safeCx = sa.SafeCX();
            const float safeCy = sa.SafeCY();

            // Réserve 80px pour header en haut + 40px padding bas
            const float availH = safeH - 80.0f * mScale - 40.0f * mScale;
            const float availW = safeW - 40.0f * mScale;

            // Aspect ratio 4:1 (8 unités de large, 2 de haut)
            mBoardW = availW;
            mBoardH = mBoardW * 0.25f;
            if (mBoardH > availH)
            {
                mBoardH = availH;
                mBoardW = mBoardH * 4.0f;
            }

            mBoardX = safeCx - mBoardW * 0.5f;
            mBoardY = safeCy + 40.0f * mScale - mBoardH * 0.5f;

            ComputePitGeometries();

            // Bouton RETOUR
            const float retourW = 120.0f * mScale;
            const float retourH = 50.0f * mScale;
            mRetourBtnX = safeCx - retourW * 0.5f;
            mRetourBtnY = 12.0f * mScale;
            mRetourBtnW = retourW;
            mRetourBtnH = retourH;
        }

        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::ComputePitGeometries()
        {
            const float pitR = mBoardW / 20.0f;
            const float mancalaR = pitR * 1.5f;
            const float spacingH = mBoardW / 8.0f;
            const float topY = mBoardY + mBoardH * 0.25f;
            const float botY = mBoardY + mBoardH * 0.75f;

            mMancalaR = mancalaR;

            // Mancala left (Player 1)
            mMancalaLX = mBoardX + mancalaR + 4.0f;
            mMancalaLY = botY;
            mPitGeo[12].cx = mMancalaLX;
            mPitGeo[12].cy = mMancalaLY;
            mPitGeo[12].radius = mancalaR;
            mPitGeo[12].pitIndex = 12;

            // Pits bottom (Player 1) : 0-5
            for (int i = 0; i < 6; ++i)
            {
                const float px = mBoardX + mancalaR * 2.0f + 8.0f + (i + 0.5f) * spacingH;
                mPitGeo[i].cx = px;
                mPitGeo[i].cy = botY;
                mPitGeo[i].radius = pitR;
                mPitGeo[i].pitIndex = i;
            }

            // Pits top (Player 2) : 6-11 (but rendered right-to-left)
            for (int i = 0; i < 6; ++i)
            {
                const float px = mBoardX + mancalaR * 2.0f + 8.0f + (i + 0.5f) * spacingH;
                mPitGeo[11 - i].cx = px;
                mPitGeo[11 - i].cy = topY;
                mPitGeo[11 - i].radius = pitR;
                mPitGeo[11 - i].pitIndex = 11 - i;
            }

            // Mancala right (Player 2)
            mMancalaRX = mBoardX + mBoardW - mancalaR - 4.0f;
            mMancalaRY = topY;
            mPitGeo[13].cx = mMancalaRX;
            mPitGeo[13].cy = mMancalaRY;
            mPitGeo[13].radius = mancalaR;
            mPitGeo[13].pitIndex = 13;
        }

        // ─────────────────────────────────────────────────────────────────────
        int GameplayScene::HitTestPit(float px, float py) const
        {
            for (int i = 0; i < 14; ++i)
            {
                const float dx = px - mPitGeo[i].cx;
                const float dy = py - mPitGeo[i].cy;
                const float dist = math::NkSqrt(dx * dx + dy * dy);
                if (dist <= mPitGeo[i].radius)
                {
                    return i;
                }
            }
            return -1;
        }

        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::TryExecuteMove(AppContext& ctx, int pitIdx)
        {
            if (mGameOver || pitIdx < 0 || pitIdx >= 12) return;

            // Validation : le pit doit appartenir au joueur courant
            bool isValidPit = false;
            if (mCurrentPlayer == 0 && pitIdx < 6)
                isValidPit = true;
            else if (mCurrentPlayer == 1 && pitIdx >= 6 && pitIdx < 12)
                isValidPit = true;

            if (!isValidPit)
            {
                mShowInvalidFeedback = true;
                mInvalidFeedbackTimer = 0.0f;
                logger.Warnf("[GameplayScene] Invalid pit %d for player %d", pitIdx, mCurrentPlayer);
                return;
            }

            // Vérifier que le pit n'est pas vide
            int grains = mBoard.GetPitGrains(pitIdx);
            if (grains == 0)
            {
                mShowInvalidFeedback = true;
                mInvalidFeedbackTimer = 0.0f;
                logger.Warnf("[GameplayScene] Pit %d is empty", pitIdx);
                return;
            }

            // Calculer la trace de distribution AVANT d'exécuter le move
            mDistributionGrains = grains;
            mDistributionTraceLen = 0;
            static constexpr int kClockwise[14] = {
                0, 1, 2, 3, 4, 5, 6, 13, 12, 11, 10, 9, 8, 7
            };
            int cwPos[14];
            for (int i = 0; i < 14; ++i)
                cwPos[kClockwise[i]] = i;

            int currentPit = pitIdx;
            for (int i = 0; i < grains; ++i)
            {
                int pos = cwPos[currentPit];
                currentPit = kClockwise[(pos + 1) % 14];
                if (currentPit == pitIdx)
                    currentPit = kClockwise[(cwPos[currentPit] + 1) % 14];

                if (mDistributionTraceLen < 14)
                    mDistributionPitsTrace[mDistributionTraceLen++] = currentPit;
            }

            // Exécuter le coup
            mBoard.ExecuteMove(mCurrentPlayer, pitIdx);
            mCurrentPlayer = mBoard.GetCurrentPlayer();
            mLastMovedPitIdx = pitIdx;
            mGrainDistAnim = 0.0f;
            mTurnChangeAnim = 0.3f;

            logger.Infof("[GameplayScene] Move executed: pit %d, next player: %d", pitIdx, mCurrentPlayer);
        }

        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::DrawHeader(AppContext& ctx)
        {
            GLRenderer2D& r = *ctx.renderer;
            FontAtlas&    f = *ctx.font;
            const SafeArea& sa = ctx.safe;

            const float safeCx = sa.SafeCX();
            const float headerY = 12.0f * mScale;
            const float headerH = 56.0f * mScale;

            // Fond semi-transparent foncé
            r.DrawQuad(sa.LeftX(), sa.TopY(), sa.SafeW(), headerH,
                      { 20, 16, 10, 200 });

            // Bouton RETOUR arrondi (orange)
            const float radius = 8.0f * mScale;
            const float x0 = mRetourBtnX, y0 = mRetourBtnY;
            const float x1 = x0 + mRetourBtnW, y1 = y0 + mRetourBtnH;
            math::NkColor bgC = { 255, 107, 0, (uint8_t)(30 * mEnterAnim) };
            math::NkColor borderC = { 255, 107, 0, (uint8_t)(150 * mEnterAnim) };

            r.DrawQuad(x0 + radius, y0, mRetourBtnW - radius * 2.0f, mRetourBtnH, bgC);
            r.DrawQuad(x0, y0 + radius, radius, mRetourBtnH - radius * 2.0f, bgC);
            r.DrawQuad(x1 - radius, y0 + radius, radius, mRetourBtnH - radius * 2.0f, bgC);

            r.DrawCircle(x0 + radius, y0 + radius, radius, bgC, 12);
            r.DrawCircle(x1 - radius, y0 + radius, radius, bgC, 12);
            r.DrawCircle(x0 + radius, y1 - radius, radius, bgC, 12);
            r.DrawCircle(x1 - radius, y1 - radius, radius, bgC, 12);

            r.DrawQuadOutline(x0 + radius, y0, mRetourBtnW - radius * 2.0f, mRetourBtnH,
                             borderC, 2.0f);
            r.DrawQuadOutline(x0, y0 + radius, mRetourBtnW, mRetourBtnH - radius * 2.0f,
                             borderC, 2.0f);

            f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, mScale,
                                     safeCx, y0 + mRetourBtnH * 0.25f,
                                     "RETOUR",
                                     { 255, 255, 255, (uint8_t)(255 * mEnterAnim) });
        }

        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::DrawTurnIndicator(AppContext& ctx)
        {
            GLRenderer2D& r = *ctx.renderer;
            FontAtlas&    f = *ctx.font;
            const SafeArea& sa = ctx.safe;
            const float safeCx = sa.SafeCX();

            const float turnY = (mRetourBtnY + mRetourBtnH) + 20.0f * mScale;

            // Titre du joueur (couleur selon le joueur)
            math::NkColor playerColor = (mCurrentPlayer == 0) ? theme::Orange() : theme::Cyan();
            char playerStr[32];
            snprintf(playerStr, sizeof(playerStr), "JOUEUR %d", mCurrentPlayer + 1);

            f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, mScale,
                                     safeCx, turnY,
                                     playerStr, playerColor);

            // Statut "A TOI DE JOUER" en doré
            f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, mScale,
                                     safeCx, turnY + 35.0f * mScale,
                                     "A TOI DE JOUER",
                                     { 200, 150, 50, (uint8_t)(200 * mEnterAnim) });
        }

        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::DrawBoard(AppContext& ctx)
        {
            for (int i = 0; i < 14; ++i)
            {
                if (i == 12 || i == 13)
                    DrawMancala(ctx, (i == 12) ? 0 : 1);
                else
                    DrawPit(ctx, i);
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::DrawPit(AppContext& ctx, int pitIdx)
        {
            GLRenderer2D& r = *ctx.renderer;
            FontAtlas&    f = *ctx.font;
            const PitGeometry& geo = mPitGeo[pitIdx];

            int grains = mBoard.GetPitGrains(pitIdx);

            // Couleur selon le joueur
            bool isPlayer1 = (pitIdx < 6);
            math::NkColor pitColor = isPlayer1 ? theme::Orange() : theme::Cyan();

            // Fond pit
            r.DrawCircle(geo.cx, geo.cy, geo.radius,
                        AlphaF(pitColor, 0.30f), 24);

            // Bordure
            math::NkColor borderColor = AlphaF(pitColor, 0.60f);
            if (mHoveredPitIdx == pitIdx)
                borderColor = { 200, 150, 50, 200 };  // Gold highlight

            r.DrawCircleOutline(geo.cx, geo.cy, geo.radius + 1.0f,
                               borderColor, 2.0f, 24);

            // Nombre de grains (texte)
            char grainStr[8];
            snprintf(grainStr, sizeof(grainStr), "%d", grains);
            f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, mScale,
                                     geo.cx, geo.cy,
                                     grainStr, { 255, 255, 255, 255 });
        }

        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::DrawMancala(AppContext& ctx, int side)
        {
            GLRenderer2D& r = *ctx.renderer;
            FontAtlas&    f = *ctx.font;
            const PitGeometry& geo = mPitGeo[(side == 0) ? 12 : 13];

            int grains = mBoard.GetMancalaGrains(side);

            // Couleur selon le joueur
            math::NkColor mancalaColor = (side == 0) ? theme::Orange() : theme::Cyan();

            // Fond mancala
            r.DrawCircle(geo.cx, geo.cy, geo.radius,
                        AlphaF(mancalaColor, 0.40f), 28);

            // Bordure
            r.DrawCircleOutline(geo.cx, geo.cy, geo.radius + 1.5f,
                               AlphaF(mancalaColor, 0.80f), 2.0f, 28);

            // Score doré
            char scoreStr[8];
            snprintf(scoreStr, sizeof(scoreStr), "%d", grains);
            f.DrawStringCenteredScaled(r, FontAtlas::DisplaySlot, mScale,
                                     geo.cx, geo.cy,
                                     scoreStr, { 200, 150, 50, 255 });
        }

        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::DrawPitFeedback(AppContext& ctx)
        {
            GLRenderer2D& r = *ctx.renderer;

            if (mHoveredPitIdx >= 0)
            {
                const PitGeometry& geo = mPitGeo[mHoveredPitIdx];
                const float pulse = 0.5f + 0.5f * math::NkSin(mTime * 3.5f);
                r.DrawCircleOutline(geo.cx, geo.cy, geo.radius + 4.0f,
                                   { 200, 150, 50, (uint8_t)(150 * pulse) }, 3.0f, 24);
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::DrawInvalidFeedback(AppContext& ctx)
        {
            GLRenderer2D& r = *ctx.renderer;
            const float alpha = 1.0f - (mInvalidFeedbackTimer / 0.6f);
            const uint8_t a = (uint8_t)(100 * alpha);

            r.DrawQuad(0.0f, mBoardY - 20.0f * mScale, (float)ctx.viewportW,
                      mBoardH + 40.0f * mScale,
                      { 255, 0, 0, a });
        }

        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::DrawDistributionAnimation(AppContext& ctx)
        {
            GLRenderer2D& r = *ctx.renderer;
            FontAtlas&    f = *ctx.font;

            // Calcul du nombre de pits à illuminer
            float duration = mDistributionGrains * 0.15f;
            float progress = mGrainDistAnim / duration;  // 0.0 → 1.0
            int piitsToShow = (int)(progress * mDistributionGrains + 0.5f);
            if (piitsToShow > mDistributionTraceLen) piitsToShow = mDistributionTraceLen;

            // Illuminer les pits visités avec gradient
            for (int i = 0; i < piitsToShow; ++i)
            {
                int pitIdx = mDistributionPitsTrace[i];
                const PitGeometry& geo = mPitGeo[pitIdx];

                // Glow gradient : plus brillant pour les pits récents
                float intensity = (float)i / (float)mDistributionTraceLen;
                uint8_t glowAlpha = (uint8_t)(180 * intensity);

                r.DrawCircle(geo.cx, geo.cy, geo.radius + 6.0f,
                           { 200, 150, 50, glowAlpha }, 24);
                r.DrawCircleOutline(geo.cx, geo.cy, geo.radius + 8.0f,
                                   { 200, 150, 50, (uint8_t)(200 * intensity) }, 3.0f, 24);
            }

            // Dessiner une "main" au pit courant
            if (piitsToShow > 0 && piitsToShow <= mDistributionTraceLen)
            {
                int handPit = mDistributionPitsTrace[piitsToShow - 1];
                const PitGeometry& geo = mPitGeo[handPit];

                // Cercle pulsant pour la main
                float pulse = 0.5f + 0.5f * math::NkSin(mTime * 6.0f);
                r.DrawCircle(geo.cx, geo.cy, geo.radius + 12.0f,
                           { 255, 200, 0, (uint8_t)(150 * pulse) }, 24);

                // Numéro de grain placé
                char grainNum[8];
                snprintf(grainNum, sizeof(grainNum), "+1");
                f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, mScale * 0.8f,
                                         geo.cx, geo.cy - geo.radius - 20.0f,
                                         grainNum, { 255, 200, 0, 255 });
            }
        }

    } // namespace songoo
} // namespace nkentseu
