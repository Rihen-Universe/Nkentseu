// =============================================================================
// GameOverScene.cpp
// =============================================================================
// Écran de fin de partie Songoo — affichage gagnant, scores finaux,
// boutons REJOUER et RETOUR MENU avec design Afro-warm.
// =============================================================================

#include "GameOverScene.h"
#include "GameplayScene.h"
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
        void GameOverScene::OnEnter(AppContext& ctx)
        {
            mTime = 0.0f;
            mEnterAnim = 0.0f;
            mHoveredBtn = -1;
            ComputeLayout(ctx);
            logger.Infof("[GameOverScene] Winner: %d, P1: %d vs P2: %d", mWinner, mScoreP1, mScoreP2);
        }

        // ─────────────────────────────────────────────────────────────────────
        void GameOverScene::OnUpdate(AppContext& ctx, float dt)
        {
            mTime += dt;
            mEnterAnim = mEnterAnim + dt * 2.0f;
            if (mEnterAnim > 1.0f) mEnterAnim = 1.0f;

            ComputeLayout(ctx);
        }

        // ─────────────────────────────────────────────────────────────────────
        void GameOverScene::OnRender(AppContext& ctx)
        {
            GLRenderer2D& r = *ctx.renderer;
            const int W = ctx.viewportW;
            const int H = ctx.viewportH;

            // Fond brun foncé (Afro-warm)
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

            DrawWinnerBanner(ctx);
            DrawScores(ctx);
            DrawButtons(ctx);

            r.End();
        }

        // ─────────────────────────────────────────────────────────────────────
        void GameOverScene::OnEvent(AppContext& ctx, NkEvent& ev)
        {
            // Clavier
            if (auto* k = ev.As<NkKeyPressEvent>())
            {
                if (k->GetKey() == NkKey::NK_ESCAPE)
                {
                    ctx.scenes->Pop();
                    ctx.scenes->Pop();  // Pop GameplayScene aussi
                    return;
                }
                if (k->GetKey() == NkKey::NK_ENTER || k->GetKey() == NkKey::NK_SPACE)
                {
                    if (mHoveredBtn == 0)
                    {
                        ctx.scenes->Pop();
                        ctx.scenes->Replace(new GameplayScene());
                    }
                    return;
                }
                return;
            }

            // Souris : clique sur bouton
            if (auto* m = ev.As<NkMouseButtonPressEvent>())
            {
                int btnIdx = HitTestButton(m->GetX(), m->GetY());
                if (btnIdx == 0)
                {
                    ctx.scenes->Pop();
                    ctx.scenes->Replace(new GameplayScene());
                }
                else if (btnIdx == 1)
                {
                    ctx.scenes->Pop();
                    ctx.scenes->Pop();  // Pop GameplayScene aussi
                }
                return;
            }

            // Souris : survol pour focus
            if (auto* m = ev.As<NkMouseMoveEvent>())
            {
                mHoveredBtn = HitTestButton(m->GetX(), m->GetY());
                return;
            }

            // Touch : tap sur bouton
            if (auto* t = ev.As<NkTouchEndEvent>())
            {
                if (t->GetNumTouches() > 0)
                {
                    const NkTouchPoint& tp = t->GetTouch(0);
                    int btnIdx = HitTestButton(tp.clientX, tp.clientY);
                    if (btnIdx == 0)
                    {
                        ctx.scenes->Pop();
                        ctx.scenes->Replace(new GameplayScene());
                    }
                    else if (btnIdx == 1)
                    {
                        ctx.scenes->Pop();
                        ctx.scenes->Pop();
                    }
                }
                return;
            }

            // Touch : move pour hover
            if (auto* t = ev.As<NkTouchMoveEvent>())
            {
                if (t->GetNumTouches() > 0)
                {
                    const NkTouchPoint& tp = t->GetTouch(0);
                    mHoveredBtn = HitTestButton(tp.clientX, tp.clientY);
                }
                return;
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        void GameOverScene::ComputeLayout(AppContext& ctx)
        {
            const SafeArea& sa = ctx.safe;
            const int W = ctx.viewportW;
            const int H = ctx.viewportH;

            mScale = GetUIScale(W, H);

            const float safeCx = sa.SafeCX();
            const float safeCy = sa.SafeCY();
            const float safeW = sa.SafeW();

            // Boutons responsive
            const float btnW = 140.0f * mScale;
            const float btnH = 60.0f * mScale;
            const float maxBtnW = safeW * 0.4f;
            float actualBtnW = (btnW > maxBtnW) ? maxBtnW : btnW;

            const float btnGap = 30.0f * mScale;
            const float btnY = safeCy + 120.0f * mScale;

            // Bouton REJOUER (gauche)
            mReplayBtnX = safeCx - actualBtnW - btnGap * 0.5f;
            mReplayBtnY = btnY;
            mReplayBtnW = actualBtnW;
            mReplayBtnH = btnH;

            // Bouton RETOUR MENU (droite)
            mMenuBtnX = safeCx + btnGap * 0.5f;
            mMenuBtnY = btnY;
            mMenuBtnW = actualBtnW;
            mMenuBtnH = btnH;
        }

        // ─────────────────────────────────────────────────────────────────────
        int GameOverScene::HitTestButton(float px, float py) const
        {
            // REJOUER
            if (px >= mReplayBtnX && px <= mReplayBtnX + mReplayBtnW &&
                py >= mReplayBtnY && py <= mReplayBtnY + mReplayBtnH)
                return 0;

            // RETOUR MENU
            if (px >= mMenuBtnX && px <= mMenuBtnX + mMenuBtnW &&
                py >= mMenuBtnY && py <= mMenuBtnY + mMenuBtnH)
                return 1;

            return -1;
        }

        // ─────────────────────────────────────────────────────────────────────
        void GameOverScene::DrawWinnerBanner(AppContext& ctx)
        {
            GLRenderer2D& r = *ctx.renderer;
            FontAtlas&    f = *ctx.font;
            const SafeArea& sa = ctx.safe;

            const float safeCx = sa.SafeCX();
            const float winnerY = sa.SafeCY() - 120.0f * mScale;

            char winnerText[64];
            math::NkColor winnerColor;

            if (mWinner == -1)
            {
                snprintf(winnerText, sizeof(winnerText), "JOUEUR 1 A GAGNE!");
                winnerColor = theme::Orange();
            }
            else if (mWinner == 1)
            {
                snprintf(winnerText, sizeof(winnerText), "JOUEUR 2 A GAGNE!");
                winnerColor = theme::Cyan();
            }
            else
            {
                snprintf(winnerText, sizeof(winnerText), "EGALITE!");
                winnerColor = { 200, 150, 50, 255 };  // Doré
            }

            // Ombre
            math::NkColor shadowC = winnerColor;
            shadowC.a = (uint8_t)(150 * mEnterAnim);

            f.DrawStringShadowCentered(r, FontAtlas::DisplaySlot,
                                     safeCx, winnerY,
                                     winnerText, winnerColor, shadowC, 3);
        }

        // ─────────────────────────────────────────────────────────────────────
        void GameOverScene::DrawScores(AppContext& ctx)
        {
            GLRenderer2D& r = *ctx.renderer;
            FontAtlas&    f = *ctx.font;
            const SafeArea& sa = ctx.safe;

            const float safeCx = sa.SafeCX();
            const float safeW = sa.SafeW();
            const float scoresY = sa.SafeCY() + 20.0f * mScale;

            // Barre de scores
            const float scoreBoxW = safeW * 0.8f;
            const float scoreBoxH = 80.0f * mScale;
            const float scoreBoxX = safeCx - scoreBoxW * 0.5f;
            const float scoreBoxY = scoresY - scoreBoxH * 0.5f;

            r.DrawQuad(scoreBoxX, scoreBoxY, scoreBoxW, scoreBoxH,
                      { 20, 16, 10, 150 });
            r.DrawQuadOutline(scoreBoxX, scoreBoxY, scoreBoxW, scoreBoxH,
                             { 200, 150, 50, 200 }, 2.0f);

            // Score P1 (gauche, orange)
            const float p1ScoreX = safeCx - scoreBoxW * 0.25f;
            f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, mScale,
                                     p1ScoreX, scoresY - 20.0f * mScale,
                                     "JOUEUR 1", theme::Orange());
            char p1Text[16];
            snprintf(p1Text, sizeof(p1Text), "%d", mScoreP1);
            f.DrawStringCenteredScaled(r, FontAtlas::DisplaySlot, mScale,
                                     p1ScoreX, scoresY + 15.0f * mScale,
                                     p1Text, theme::Orange());

            // Score P2 (droite, cyan)
            const float p2ScoreX = safeCx + scoreBoxW * 0.25f;
            f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, mScale,
                                     p2ScoreX, scoresY - 20.0f * mScale,
                                     "JOUEUR 2", theme::Cyan());
            char p2Text[16];
            snprintf(p2Text, sizeof(p2Text), "%d", mScoreP2);
            f.DrawStringCenteredScaled(r, FontAtlas::DisplaySlot, mScale,
                                     p2ScoreX, scoresY + 15.0f * mScale,
                                     p2Text, theme::Cyan());
        }

        // ─────────────────────────────────────────────────────────────────────
        void GameOverScene::DrawButtons(AppContext& ctx)
        {
            GLRenderer2D& r = *ctx.renderer;
            FontAtlas&    f = *ctx.font;
            const SafeArea& sa = ctx.safe;
            const float safeCx = sa.SafeCX();

            const float radius = 8.0f * mScale;

            // Fonction lambda pour dessiner un bouton
            auto DrawButton = [&](float x, float y, float w, float h, bool hovered, const char* label)
            {
                math::NkColor bgC = hovered
                    ? math::NkColor{ 0, 245, 255, (uint8_t)(50 * mEnterAnim) }   // Cyan si survolé
                    : math::NkColor{ 255, 107, 0, (uint8_t)(30 * mEnterAnim) };  // Orange sinon
                math::NkColor borderC = hovered
                    ? math::NkColor{ 0, 245, 255, (uint8_t)(220 * mEnterAnim) }
                    : math::NkColor{ 255, 107, 0, (uint8_t)(150 * mEnterAnim) };

                const float x0 = x, y0 = y, x1 = x + w, y1 = y + h;

                // Fond
                r.DrawQuad(x0 + radius, y0, w - radius * 2.0f, h, bgC);
                r.DrawQuad(x0, y0 + radius, radius, h - radius * 2.0f, bgC);
                r.DrawQuad(x1 - radius, y0 + radius, radius, h - radius * 2.0f, bgC);

                // Coins
                r.DrawCircle(x0 + radius, y0 + radius, radius, bgC, 12);
                r.DrawCircle(x1 - radius, y0 + radius, radius, bgC, 12);
                r.DrawCircle(x0 + radius, y1 - radius, radius, bgC, 12);
                r.DrawCircle(x1 - radius, y1 - radius, radius, bgC, 12);

                // Bordure
                r.DrawQuadOutline(x0 + radius, y0, w - radius * 2.0f, h,
                                 borderC, 2.0f);
                r.DrawQuadOutline(x0, y0 + radius, w, h - radius * 2.0f,
                                 borderC, 2.0f);

                // Texte
                f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, mScale,
                                         x + w * 0.5f, y + h * 0.25f,
                                         label, { 255, 255, 255, (uint8_t)(255 * mEnterAnim) });
            };

            DrawButton(mReplayBtnX, mReplayBtnY, mReplayBtnW, mReplayBtnH,
                      mHoveredBtn == 0, "REJOUER");
            DrawButton(mMenuBtnX, mMenuBtnY, mMenuBtnW, mMenuBtnH,
                      mHoveredBtn == 1, "MENU");
        }

    } // namespace songoo
} // namespace nkentseu
