// =============================================================================
// SettingsScene.cpp
// =============================================================================
// Écran des paramètres Songoo — vitesse de jeu, son, difficulté IA.
// =============================================================================

#include "SettingsScene.h"
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

        // Descriptions des paramètres
        static const struct {
            const char* label;
            const char* desc;
        } kSettingItems[SettingsScene::kItemCount] = {
            { "VITESSE DE JEU",  "Multiplicateur de vitesse (1.0x - 2.0x)" },
            { "SON",              "Activer/Désactiver le son" },
            { "DIFFICULTE IA",    "Niveau IA (Facile / Moyen / Difficile)" }
        };

        // ─────────────────────────────────────────────────────────────────────
        void SettingsScene::OnEnter(AppContext& /*ctx*/)
        {
            mTime = 0.0f;
            mFocusIndex = 0;
            mEnterAnim = 0.0f;
            // TODO: Charger les valeurs depuis une config persistante
            mGameSpeed = 1.0f;
            mSoundEnabled = true;
            mAIDifficulty = 1;
            logger.Info("[Settings] OnEnter");
        }

        // ─────────────────────────────────────────────────────────────────────
        void SettingsScene::OnUpdate(AppContext& ctx, float dt)
        {
            mTime += dt;
            mEnterAnim = mEnterAnim + dt * 2.0f;
            if (mEnterAnim > 1.0f) mEnterAnim = 1.0f;
            (void)ctx;
        }

        // ─────────────────────────────────────────────────────────────────────
        void SettingsScene::OnEvent(AppContext& ctx, NkEvent& ev)
        {
            // Clavier : Echap = retour
            if (auto* k = ev.As<NkKeyPressEvent>())
            {
                if (k->GetKey() == NkKey::NK_ESCAPE)
                {
                    ctx.scenes->Pop();
                    return;
                }
                return;
            }

            // Souris : clique sur bouton RETOUR
            if (auto* m = ev.As<NkMouseButtonPressEvent>())
            {
                const float px = m->GetX();
                const float py = m->GetY();
                if (px >= mBackX && px <= mBackX + mBackW &&
                    py >= mBackY && py <= mBackY + mBackH)
                {
                    ctx.scenes->Pop();
                }
                return;
            }

            // Touch : tap sur bouton RETOUR
            if (auto* t = ev.As<NkTouchEndEvent>())
            {
                if (t->GetNumTouches() > 0)
                {
                    const NkTouchPoint& tp = t->GetTouch(0);
                    if (tp.clientX >= mBackX && tp.clientX <= mBackX + mBackW &&
                        tp.clientY >= mBackY && tp.clientY <= mBackY + mBackH)
                    {
                        ctx.scenes->Pop();
                    }
                }
                return;
            }

            // TODO: Interaction pour modifier les valeurs (sliders/toggles)
            (void)ctx;
            (void)ev;
        }

        // ─────────────────────────────────────────────────────────────────────
        void SettingsScene::OnRender(AppContext& ctx)
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

            // Titre "PARAMETRES" avec effet pulsant chaleureux
            const float titleY = safeCy - 200.0f * scale;
            const float pulse = 0.5f + 0.5f * math::NkSin(mTime * 2.0f);

            // Ombre dorée
            math::NkColor titleShadow = { 200, 150, 50, (uint8_t)((0.40f + 0.30f * pulse) * 255) };
            f.DrawStringShadowCentered(r, FontAtlas::HeadlineSlot,
                                     safeCx, titleY,
                                     "PARAMETRES",
                                     theme::White(), titleShadow, 3);

            // ── Éléments paramètres avec style arrondi et chaleureux ────────────────
            // Responsive: buttons scale down to fit safe area width
            float btnW = 280.0f * scale;
            const float maxBtnW = safeW * 0.9f;  // Max 90% of safe area width
            if (btnW > maxBtnW) btnW = maxBtnW;

            const float btnH = 65.0f * scale;
            const float btnGap = 18.0f * scale;
            const float totalH = kItemCount * btnH + (kItemCount - 1) * btnGap;
            const float startY = safeCy + 80.0f * scale - totalH * 0.5f;

            mItemX = safeCx - btnW * 0.5f;
            mItemW = btnW;
            mItemH = btnH;
            mItemGap = btnGap;

            for (int i = 0; i < kItemCount; ++i)
            {
                const float itemY = startY + i * (btnH + btnGap);
                mItemYs[i] = itemY;

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
                const float x0 = mItemX;
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
                                        kSettingItems[i].label,
                                        { 255, 255, 255, (uint8_t)textAlpha });

                // Valeur selon le type de paramètre (en dessous du label)
                char valueStr[32];
                switch (i)
                {
                case Setting_GameSpeed:
                    snprintf(valueStr, sizeof(valueStr), "%.1fx", mGameSpeed);
                    break;
                case Setting_Sound:
                    snprintf(valueStr, sizeof(valueStr), "%s", mSoundEnabled ? "ON" : "OFF");
                    break;
                case Setting_AIDifficulty:
                {
                    static const char* kLevels[] = { "FACILE", "MOYEN", "DIFFICILE" };
                    snprintf(valueStr, sizeof(valueStr), "%s", kLevels[mAIDifficulty]);
                    break;
                }
                default:
                    valueStr[0] = '\0';
                }

                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                        safeCx, itemY + btnH * 0.75f,
                                        valueStr,
                                        { 0, 245, 255, (uint8_t)(200 * alpha) });
            }

            // ── Bouton RETOUR (style arrondi Afro-warm) ─────────────────────────────
            const float backBtnW = 120.0f * scale;
            const float backBtnH = 50.0f * scale;
            mBackX = safeCx - backBtnW * 0.5f;
            mBackY = titleY + 60.0f * scale;
            mBackW = backBtnW;
            mBackH = backBtnH;

            const float backRadius = 8.0f * scale;
            const float bx0 = mBackX, by0 = mBackY, bx1 = mBackX + backBtnW, by1 = mBackY + backBtnH;
            math::NkColor backBgC = { 255, 107, 0, (uint8_t)(20 * mEnterAnim) };
            math::NkColor backBorderC = { 255, 107, 0, (uint8_t)(150 * mEnterAnim) };

            // Fond principal
            r.DrawQuad(bx0 + backRadius, by0, backBtnW - backRadius * 2.0f, backBtnH, backBgC);
            r.DrawQuad(bx0, by0 + backRadius, backRadius, backBtnH - backRadius * 2.0f, backBgC);
            r.DrawQuad(bx1 - backRadius, by0 + backRadius, backRadius, backBtnH - backRadius * 2.0f, backBgC);

            // Coins arrondis
            r.DrawCircle(bx0 + backRadius, by0 + backRadius, backRadius, backBgC, 12);
            r.DrawCircle(bx1 - backRadius, by0 + backRadius, backRadius, backBgC, 12);
            r.DrawCircle(bx0 + backRadius, by1 - backRadius, backRadius, backBgC, 12);
            r.DrawCircle(bx1 - backRadius, by1 - backRadius, backRadius, backBgC, 12);

            // Bordure arrondie
            r.DrawQuadOutline(bx0 + backRadius, by0, backBtnW - backRadius * 2.0f, backBtnH,
                             backBorderC, 2.0f);
            r.DrawQuadOutline(bx0, by0 + backRadius, backBtnW, backBtnH - backRadius * 2.0f,
                             backBorderC, 2.0f);

            f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                     safeCx, mBackY + backBtnH * 0.25f,
                                     "RETOUR",
                                     { 255, 255, 255, (uint8_t)(255 * mEnterAnim) });

            r.End();
        }

    } // namespace songoo
} // namespace nkentseu
