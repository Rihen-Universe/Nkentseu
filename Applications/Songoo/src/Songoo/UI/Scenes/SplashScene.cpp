// =============================================================================
// SplashScene.cpp
// =============================================================================
// Splash screen Songoo : logo + aperçu board Mancala.
// =============================================================================

#include "SplashScene.h"
#include "MainMenuScene.h"
#include "Songoo/Render/GLRenderer2D.h"
#include "Songoo/Render/FontAtlas.h"
#include "Songoo/Render/SafeArea.h"
#include "Songoo/UI/Theme.h"
#include "Songoo/UI/SceneManager.h"
#include "Songoo/UI/UIScale.h"
#include <cmath>

namespace nkentseu
{
    namespace songoo
    {

        // ─────────────────────────────────────────────────────────────────────
        // Helpers locaux
        // ─────────────────────────────────────────────────────────────────────

        // Grille de fond style HTML : lignes cyan 4% alpha espacees de 40 px.
        static void DrawGridBackground(GLRenderer2D& r, const SafeArea& sa)
        {
            const math::NkColor gc = theme::GridLine();
            const int spacing = 40;
            const int W = (int)sa.vpW;
            const int H = (int)sa.vpH;
            for (int x = 0; x <= W; x += spacing)
            {
                r.DrawQuad((float)x, 0.0f, 1.0f, (float)H, gc);
            }
            for (int y = 0; y <= H; y += spacing)
            {
                r.DrawQuad(0.0f, (float)y, (float)W, 1.0f, gc);
            }
        }

        // Aperçu animé du plateau Mancala horizontal : 2 rangées de 6 pits + 2 mancalas.
        // Layout: [Mancala1] [pit1..6] [Mancala2]
        static void DrawMancalaPreview(GLRenderer2D& r, float x, float y,
                                       float w, float h, float t)
        {
            const float pitR = 11.0f;
            const float mancalaR = 16.0f;
            const float grainR = 2.0f;

            // Spacing horizontal
            const float mancalaW = mancalaR * 2.0f + 8.0f;
            const float pitsW = w - mancalaW * 2.0f;
            const float pitSpacing = pitsW / 6.0f;

            const float centerY = y + h * 0.5f;
            const float topPitY = y + h * 0.25f;
            const float botPitY = y + h * 0.75f;

            // === Rangée du bas (joueur 1 - Orange) ===

            // Mancala gauche (Orange, Joueur 1)
            r.DrawCircle(x + mancalaR + 4.0f, botPitY, mancalaR,
                        AlphaF(theme::Orange(), 0.40f), 24);
            r.DrawCircleOutline(x + mancalaR + 4.0f, botPitY, mancalaR + 1.0f,
                               AlphaF(theme::Orange(), 0.80f), 1.0f, 24);

            // 6 pits bas (rangée du joueur 1)
            const float botPitsStartX = x + mancalaW;
            for (int i = 0; i < 6; ++i)
            {
                const float px = botPitsStartX + pitSpacing * (i + 0.5f);

                r.DrawCircle(px, botPitY, pitR,
                            AlphaF(theme::Orange(), 0.30f), 18);
                r.DrawCircleOutline(px, botPitY, pitR + 0.5f,
                                   AlphaF(theme::Orange(), 0.60f), 1.0f, 18);

                // Grains animés
                const int grainCount = 3 + i % 2;
                for (int g = 0; g < grainCount; ++g)
                {
                    const float angle = (t + i * 0.5f + g * 2.0f) * 1.5f;
                    const float gx = px + math::NkCos(angle) * pitR * 0.5f;
                    const float gy = botPitY + math::NkSin(angle) * pitR * 0.5f;
                    r.DrawCircle(gx, gy, grainR, theme::Orange());
                }
            }

            // === Rangée du haut (joueur 2 - Cyan) ===

            // 6 pits haut (rangée du joueur 2, ordre inverse pour le Mancala)
            const float topPitsStartX = x + mancalaW;
            for (int i = 0; i < 6; ++i)
            {
                const float px = topPitsStartX + pitSpacing * (i + 0.5f);

                r.DrawCircle(px, topPitY, pitR,
                            AlphaF(theme::Cyan(), 0.30f), 18);
                r.DrawCircleOutline(px, topPitY, pitR + 0.5f,
                                   AlphaF(theme::Cyan(), 0.60f), 1.0f, 18);

                // Grains animés
                const int grainCount = 3 + i % 2;
                for (int g = 0; g < grainCount; ++g)
                {
                    const float angle = (t + i * 0.5f + g * 2.0f) * 1.5f;
                    const float gx = px + math::NkCos(angle) * pitR * 0.5f;
                    const float gy = topPitY + math::NkSin(angle) * pitR * 0.5f;
                    r.DrawCircle(gx, gy, grainR, theme::Cyan());
                }
            }

            // Mancala droit (Cyan, Joueur 2)
            r.DrawCircle(x + w - mancalaR - 4.0f, topPitY, mancalaR,
                        AlphaF(theme::Cyan(), 0.40f), 24);
            r.DrawCircleOutline(x + w - mancalaR - 4.0f, topPitY, mancalaR + 1.0f,
                               AlphaF(theme::Cyan(), 0.80f), 1.0f, 24);
        }

        // ─────────────────────────────────────────────────────────────────────
        void SplashScene::OnEnter(AppContext& /*ctx*/)
        {
            mTime  = 0.0f;
            mTimer = 4.0f;
        }

        void SplashScene::OnUpdate(AppContext& ctx, float dt)
        {
            mTime += dt;
            mTimer -= dt;
            if (mTimer <= 0.0f)
            {
                ctx.scenes->Replace(new MainMenuScene());
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        void SplashScene::OnRender(AppContext& ctx)
        {
            GLRenderer2D& r = *ctx.renderer;
            FontAtlas&    f = *ctx.font;
            const int W = ctx.viewportW;
            const int H = ctx.viewportH;
            const SafeArea& sa = ctx.safe;
            const float t  = mTime;
            const float cx = sa.SafeCX();
            const float cy = sa.SafeCY();
            const float scale = GetUIScale(W, H);

            r.Clear(theme::Dark().r / 255.0f,
                    theme::Dark().g / 255.0f,
                    theme::Dark().b / 255.0f, 1.0f);
            r.Begin(W, H);
            DrawGridBackground(r, sa);

            // ── Logo SONGOO ──────────────────────────────────────────────────
            const float pulse = 0.5f + 0.5f * math::NkSin(t * 2.5f);
            math::NkColor shadowC = theme::Orange();
            shadowC.a = (uint8_t)((0.55f + 0.30f * pulse) * 255);
            const float logoY = cy - 180.0f * scale;
            f.DrawStringShadowCentered(r, FontAtlas::DisplaySlot,
                                     cx, logoY,
                                     "SONGOO", theme::White(), shadowC, 3);

            // ── Sous-titre "Joyau d'Afrique" ─────────────────────────────────
            const float subY = logoY + (72.0f + 12.0f) * scale;
            f.DrawStringShadowCentered(r, FontAtlas::SubtitleSlot,
                                     cx, subY, "Joyau  d'Afrique",
                                     theme::Orange(), AlphaF(theme::Orange(), 0.50f), 1);

            // ── Badge "* MANCALA *" orange ───────────────────────────────────
            const char* badge = "*  MANCALA  *";
            const float badgeTW = f.MeasureWidth(FontAtlas::BodySlot, badge);
            const float badgePadX = 18.0f * scale;
            const float badgePadY =  8.0f * scale;
            const float badgeW = badgeTW + badgePadX * 2.0f;
            const float badgeH = 18.0f * scale + badgePadY * 2.0f;
            const float badgeX = cx - badgeW * 0.5f;
            const float badgeY = subY + 50.0f * scale;
            r.DrawQuad(badgeX, badgeY, badgeW, badgeH, AlphaF(theme::Orange(), 0.12f));
            r.DrawQuadOutline(badgeX, badgeY, badgeW, badgeH, AlphaF(theme::Orange(), 0.55f), 1.0f);
            f.DrawStringCentered(r, FontAtlas::BodySlot,
                               cx, badgeY + badgePadY,
                               badge, theme::Orange());

            // ── Aperçu board Mancala ────────────────────────────────────────
            const float boardW = 320.0f * scale;
            const float boardH = 100.0f * scale;
            const float boardX = cx - boardW * 0.5f;
            const float boardY = badgeY + badgeH + 28.0f * scale;
            DrawMancalaPreview(r, boardX, boardY, boardW, boardH, t);

            // ── CTA "APPUYER POUR JOUER" avec style arrondi chaleureux ────────
            const char* cta = "APPUYER POUR JOUER";
            const float ctaTW = f.MeasureWidth(FontAtlas::SubtitleSlot, cta);
            const float ctaPadX = 40.0f * scale;
            const float ctaPadY = 20.0f * scale;
            const float ctaW = ctaTW + ctaPadX * 2.0f;
            const float ctaH = 32.0f * scale + ctaPadY * 2.0f;
            const float ctaX = cx - ctaW * 0.5f;
            const float ctaY = boardY + boardH + 36.0f * scale;
            const float radius = 8.0f * scale;

            // Pulsing animation pour l'effet "appuyez ici"
            const float ctaPulse = 0.5f + 0.5f * math::NkSin(t * 3.4f);
            const float pulseAlpha = 0.6f + 0.4f * ctaPulse;

            // Fond arrondi avec gradient-like effect via couleur chaleureuse
            const float x0 = ctaX, y0 = ctaY, x1 = ctaX + ctaW, y1 = ctaY + ctaH;
            math::NkColor bgC = { 255, 107, 0, (uint8_t)(30 * pulseAlpha) };  // Orange warm
            math::NkColor borderC = { 255, 107, 0, (uint8_t)(200 * pulseAlpha) };

            // Fond principal
            r.DrawQuad(x0 + radius, y0, ctaW - radius * 2.0f, ctaH, bgC);
            r.DrawQuad(x0, y0 + radius, radius, ctaH - radius * 2.0f, bgC);
            r.DrawQuad(x1 - radius, y0 + radius, radius, ctaH - radius * 2.0f, bgC);

            // Coins arrondis
            r.DrawCircle(x0 + radius, y0 + radius, radius, bgC, 12);
            r.DrawCircle(x1 - radius, y0 + radius, radius, bgC, 12);
            r.DrawCircle(x0 + radius, y1 - radius, radius, bgC, 12);
            r.DrawCircle(x1 - radius, y1 - radius, radius, bgC, 12);

            // Bordure arrondie
            r.DrawQuadOutline(x0 + radius, y0, ctaW - radius * 2.0f, ctaH,
                             borderC, 2.0f);
            r.DrawQuadOutline(x0, y0 + radius, ctaW, ctaH - radius * 2.0f,
                             borderC, 2.0f);

            // Texte du bouton (centré verticalement et horizontalement)
            f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                     cx, ctaY + ctaH * 0.25f,
                                     cta, { 255, 255, 255, (uint8_t)(255 * pulseAlpha) });

            // ── Credits ──────────────────────────────────────────────────────
            const char* credits = "(C) 2026  SONGOO - MANCALA GAME  -  DESIGNED BY RIHEN";
            f.DrawStringCentered(r, FontAtlas::SmallSlot,
                               cx, sa.BottomY(12.0f) - 16.0f,
                               credits, AlphaF(theme::White(), 0.30f));

            r.End();
        }

    } // namespace songoo
} // namespace nkentseu
