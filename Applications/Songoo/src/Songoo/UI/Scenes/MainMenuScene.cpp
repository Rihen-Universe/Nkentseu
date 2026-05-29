// =============================================================================
// MainMenuScene.cpp
// =============================================================================
// Menu principal Songoo - 4 items simples: Jouer / Règles / Options / Quitter.
// Layout centré vertical avec cards responsive.
// =============================================================================

#include "MainMenuScene.h"
#include "GameplayScene.h"
#include "RulesScene.h"
#include "SettingsScene.h"
#include "NKPlatform/NkPlatformDetect.h"
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

// Déclaration forward pour les autres scenes (si nécessaire)
// RulesScene et SettingsScene sont inclues au début

namespace nkentseu
{
    namespace songoo
    {

        // Descriptions des items du menu
        static const struct {
            const char* label;
            const char* desc;
        } kMenuItems[MainMenuScene::kItemCount] = {
            { "JOUER",           "Lancer une partie" },
            { "COMMENT JOUER",   "Règles du Mancala" },
            { "OPTIONS",         "Paramètres" },
            { "QUITTER",         "Fermer l'application" }
        };

        // ─────────────────────────────────────────────────────────────────────
        void MainMenuScene::OnEnter(AppContext& /*ctx*/)
        {
            mTime = 0.0f;
            mFocusIndex = 0;
            mEnterAnim = 0.0f;
        }

        // ─────────────────────────────────────────────────────────────────────
        void MainMenuScene::OnUpdate(AppContext& ctx, float dt)
        {
            mTime += dt;
            mEnterAnim = mEnterAnim + dt * 2.0f;
            if (mEnterAnim > 1.0f) mEnterAnim = 1.0f;
        }

        // ─────────────────────────────────────────────────────────────────────
        void MainMenuScene::OnEvent(AppContext& ctx, NkEvent& ev)
        {
            // Clavier : navigation + validation
            if (auto* k = ev.As<NkKeyPressEvent>())
            {
                switch (k->GetKey())
                {
                case NkKey::NK_UP:
                    mFocusIndex = (mFocusIndex - 1 + kItemCount) % kItemCount;
                    break;
                case NkKey::NK_DOWN:
                    mFocusIndex = (mFocusIndex + 1) % kItemCount;
                    break;
                case NkKey::NK_ENTER:
                case NkKey::NK_SPACE:
                    ActivateItem(ctx, (ItemId)mFocusIndex);
                    break;
                default:
                    break;
                }
                return;
            }

            // Souris : clique sur un bouton
            if (auto* m = ev.As<NkMouseButtonPressEvent>())
            {
                const int itemIdx = HitTestItem(m->GetX(), m->GetY());
                if (itemIdx >= 0 && itemIdx < kItemCount)
                {
                    mFocusIndex = itemIdx;
                    ActivateItem(ctx, (ItemId)itemIdx);
                }
                return;
            }

            // Souris : survol pour focus
            if (auto* m = ev.As<NkMouseMoveEvent>())
            {
                const int itemIdx = HitTestItem(m->GetX(), m->GetY());
                if (itemIdx >= 0 && itemIdx < kItemCount)
                {
                    mFocusIndex = itemIdx;
                }
                return;
            }

            // Touch : tap sur un bouton
            if (auto* t = ev.As<NkTouchEndEvent>())
            {
                if (t->GetNumTouches() > 0)
                {
                    const NkTouchPoint& tp = t->GetTouch(0);
                    const int itemIdx = HitTestItem(tp.clientX, tp.clientY);
                    if (itemIdx >= 0 && itemIdx < kItemCount)
                    {
                        mFocusIndex = itemIdx;
                        ActivateItem(ctx, (ItemId)itemIdx);
                    }
                }
                return;
            }

            // Touch : move pour focus
            if (auto* t = ev.As<NkTouchMoveEvent>())
            {
                if (t->GetNumTouches() > 0)
                {
                    const NkTouchPoint& tp = t->GetTouch(0);
                    const int itemIdx = HitTestItem(tp.clientX, tp.clientY);
                    if (itemIdx >= 0 && itemIdx < kItemCount)
                    {
                        mFocusIndex = itemIdx;
                    }
                }
                return;
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        void MainMenuScene::OnRender(AppContext& ctx)
        {
            GLRenderer2D& r = *ctx.renderer;
            FontAtlas&    f = *ctx.font;
            const int W = ctx.viewportW;
            const int H = ctx.viewportH;
            const SafeArea& sa = ctx.safe;

            // Centre dans la safe area pour mobile
            const float safeCx = sa.SafeCX();
            const float safeCy = sa.SafeCY();
            const float safeW = sa.SafeW();
            const float safeH = sa.SafeH();

            const float scale = GetUIScale(W, H);

            // Fond avec dégradé subtle (dark brown/black ombré)
            r.Clear(0.08f, 0.06f, 0.04f, 1.0f);
            r.Begin(W, H);

            // Grille subtile (très faintement visible)
            const math::NkColor gridC = theme::GridLine();
            math::NkColor faintGrid = gridC;
            faintGrid.a = 30;
            for (int x = 0; x <= W; x += 60)
                r.DrawQuad((float)x, 0.0f, 1.0f, (float)H, faintGrid);
            for (int y = 0; y <= H; y += 60)
                r.DrawQuad(0.0f, (float)y, (float)W, 1.0f, faintGrid);

            // Titre "SONGOO" avec effet pulsant chaleureux
            const float titleY = safeCy - 200.0f * scale;
            const float pulse = 0.5f + 0.5f * math::NkSin(mTime * 2.0f);

            // Ombre dorée (plutôt que orange)
            math::NkColor titleShadow = { 200, 150, 50, (uint8_t)((0.40f + 0.30f * pulse) * 255) };
            f.DrawStringShadowCentered(r, FontAtlas::HeadlineSlot,
                                     safeCx, titleY,
                                     "SONGOO",
                                     theme::White(), titleShadow, 3);

            // Sous-titre "Joyau d'Afrique"
            f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                     safeCx, titleY + 60.0f * scale,
                                     "Joyau d'Afrique",
                                     { 200, 150, 50, (uint8_t)(200 * mEnterAnim) });

            // ── Items du menu avec style arrondi et chaleureux ────────────────
            // Responsive: buttons scale down to fit safe area width
            float btnW = 280.0f * scale;
            const float maxBtnW = safeW * 0.9f;  // Max 90% of safe area width
            if (btnW > maxBtnW) btnW = maxBtnW;

            const float btnH = 65.0f * scale;
            const float btnGap = 18.0f * scale;
            const float totalH = kItemCount * btnH + (kItemCount - 1) * btnGap;
            const float startY = safeCy + 80.0f * scale - totalH * 0.5f;

            mCardListX = safeCx - btnW * 0.5f;
            mCardListW = btnW;
            mCardItemH = btnH;
            mCardItemGap = btnGap;

            for (int i = 0; i < kItemCount; ++i)
            {
                const float itemY = startY + i * (btnH + btnGap);
                mCardItemYs[i] = itemY;

                // Animation : scale + rotate pour effet "bouncy"
                const float enterDelay = i * 0.10f;
                const float enterProg = mEnterAnim > enterDelay ? (mEnterAnim - enterDelay) : 0.0f;
                const float scaleProg = 0.8f + 0.2f * enterProg;  // 0.8 -> 1.0
                const float alpha = enterProg;

                const bool focused = (i == mFocusIndex);

                // Effet de clignotement quand focused
                float blinkAlpha = 1.0f;
                if (focused)
                {
                    blinkAlpha = 0.6f + 0.4f * (0.5f + 0.5f * math::NkSin(mTime * 3.5f));
                }

                // Couleur de base : warm orange pour boutons normaux
                // Cyan pour focused avec clignotement
                math::NkColor bgC, borderC;
                if (focused)
                {
                    bgC = { 0, 245, 255, (uint8_t)(40 * alpha * blinkAlpha) };      // Cyan translucide + blink
                    borderC = { 0, 245, 255, (uint8_t)(220 * alpha * blinkAlpha) }; // Cyan border + blink
                }
                else
                {
                    bgC = { 255, 107, 0, (uint8_t)(20 * alpha) };      // Orange translucide
                    borderC = { 255, 107, 0, (uint8_t)(150 * alpha) }; // Orange border
                }

                // Dessiner bouton arrondi (multiple quads = coin arrondi approximatif)
                const float radius = 8.0f * scale;
                const float x0 = mCardListX;
                const float y0 = itemY;
                const float x1 = x0 + btnW;
                const float y1 = y0 + btnH;

                // Fond principal
                r.DrawQuad(x0 + radius, y0, btnW - radius * 2.0f, btnH, bgC);
                r.DrawQuad(x0, y0 + radius, radius, btnH - radius * 2.0f, bgC);
                r.DrawQuad(x1 - radius, y0 + radius, radius, btnH - radius * 2.0f, bgC);

                // Coins arrondis (circles simples)
                r.DrawCircle(x0 + radius, y0 + radius, radius, bgC, 12);
                r.DrawCircle(x1 - radius, y0 + radius, radius, bgC, 12);
                r.DrawCircle(x0 + radius, y1 - radius, radius, bgC, 12);
                r.DrawCircle(x1 - radius, y1 - radius, radius, bgC, 12);

                // Bordure arrondie
                r.DrawQuadOutline(x0 + radius, y0, btnW - radius * 2.0f, btnH,
                                 borderC, 2.0f);
                r.DrawQuadOutline(x0, y0 + radius, btnW, btnH - radius * 2.0f,
                                 borderC, 2.0f);

                // Label principal (centré verticalement et horizontalement dans le bouton)
                const float textAlpha = focused ? (255 * alpha * blinkAlpha) : (255 * alpha);
                f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                        safeCx, itemY + btnH * 0.25f,
                                        kMenuItems[i].label,
                                        { 255, 255, 255, (uint8_t)textAlpha });

                // Focus indicator : petit point animé qui pulse
                if (focused)
                {
                    const float blink = 0.6f + 0.4f * (0.5f + 0.5f * math::NkSin(mTime * 3.5f));
                    r.DrawCircle(mCardListX - 20.0f * scale, itemY + btnH * 0.5f,
                               6.0f * scale, { 0, 245, 255, (uint8_t)(200 * alpha * blink) }, 16);
                }
            }

            r.End();
        }

        // ─────────────────────────────────────────────────────────────────────
        void MainMenuScene::ActivateItem(AppContext& ctx, ItemId item)
        {
            switch (item)
            {
                case Item_Play:
                    logger.Info("[MainMenu] JOUER - Opening GameplayScene");
                    ctx.scenes->Push(new GameplayScene());
                    break;

                case Item_Rules:
                    logger.Info("[MainMenu] COMMENT JOUER - Opening RulesScene");
                    ctx.scenes->Push(new RulesScene());
                    break;

                case Item_Options:
                    logger.Info("[MainMenu] OPTIONS - Opening SettingsScene");
                    ctx.scenes->Push(new SettingsScene());
                    break;

                case Item_Quit:
#if !defined(NKENTSEU_PLATFORM_ANDROID) && !defined(NKENTSEU_PLATFORM_IOS) && !defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
                    if (ctx.quitRequested)
                        *ctx.quitRequested = true;
#endif
                    break;

                default:
                    break;
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        int MainMenuScene::HitTestItem(float px, float py) const
        {
            for (int i = 0; i < kItemCount; ++i)
            {
                const float itemY = mCardItemYs[i];
                if (px >= mCardListX && px <= mCardListX + mCardListW &&
                    py >= itemY && py <= itemY + mCardItemH)
                {
                    return i;
                }
            }
            return -1;
        }

    } // namespace songoo
} // namespace nkentseu
