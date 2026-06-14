// =============================================================================
// SplashScene.cpp
// -----------------------------------------------------------------------------
// Reproduit fidelement docs/01_splash_et_menus.html#splash.
// Premier ecran apres les intros Rihen + Noge.
// =============================================================================

#include "SplashScene.h"
#include "MainMenuScene.h"
#include "NKCanvas/Renderer/Core/NkRenderer2D.h"
#include "Pong/Render/FontAtlas.h"
#include "Pong/Render/SafeArea.h"
#include "Pong/UI/Theme.h"
#include "Pong/UI/SceneManager.h"
#include "Pong/UI/UIScale.h"
#include <cmath>

namespace nkentseu
{
    namespace pong
    {

        // ─────────────────────────────────────────────────────────────────────
        // Helpers locaux
        // ─────────────────────────────────────────────────────────────────────

        // Grille de fond style HTML : lignes cyan 4% alpha espacees de 40 px.
        // On respecte les insets SafeArea pour ne pas tracer sous la status
        // bar Android (les marges restent noires uniformes).
        static void DrawGridBackground(renderer::NkRenderer2D& r, const SafeArea& sa)
        {
            const math::NkColor gc = theme::GridLine();
            const int spacing = 40;
            const int W = (int)sa.vpW;
            const int H = (int)sa.vpH;
            for (int x = 0; x <= W; x += spacing)
            {
                r.DrawFilledRect({ (float)x, 0.0f, 1.0f, (float)H }, gc);
            }
            for (int y = 0; y <= H; y += spacing)
            {
                r.DrawFilledRect({ 0.0f, (float)y, (float)W, 1.0f }, gc);
            }
        }

        // Mini-field anime : terrain miniature avec balle qui rebondit.
        static void DrawMiniField(renderer::NkRenderer2D& r, float x, float y,
                                  float w, float h, float t)
        {
            // Ligne centrale degradee
            const float midY = y + h * 0.5f;
            const int segs = 36;
            const float segW = w / segs;
            for (int i = 0; i < segs; ++i)
            {
                const float fi = (float)i / (segs - 1);
                float a = 1.0f - std::abs(fi - 0.5f) * 2.0f;
                if (a < 0.0f) a = 0.0f;
                r.DrawFilledRect({ x + i * segW, midY - 1.0f, segW + 1.0f, 2.0f },
                           AlphaF(theme::Cyan(), a * 0.30f));
            }
            // Paddles cyan / orange aux bords
            r.DrawFilledRect({ x + 8.0f,      midY - 25.0f, 6.0f, 50.0f }, theme::Cyan());
            r.DrawFilledRect({ x + w - 14.0f, midY - 25.0f, 6.0f, 50.0f }, theme::Orange());
            // Balle anime : oscillation sinusoidale
            const float bx = x + 14.0f + (w - 28.0f) * (0.5f + 0.5f * math::NkSin(t * 2.0f - 1.5707963f));
            const float by = midY + math::NkSin(t * 4.0f) * (h * 0.30f);
            r.DrawFilledCircle({ bx, by }, 5.0f, theme::White(), 18);
            r.DrawCircleOutline({ bx, by }, 6.0f, AlphaF(theme::White(), 0.6f), 1.0f, 18);
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
                // Auto-advance vers le menu principal.
                ctx.scenes->Replace(new MainMenuScene());
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        void SplashScene::OnRender(AppContext& ctx)
        {
            renderer::NkRenderer2D& r = *ctx.renderer;
            FontAtlas&    f = *ctx.font;
            const int W = ctx.viewportW;
            const int H = ctx.viewportH;
            const SafeArea& sa = ctx.safe;
            const float t  = mTime;
            const float cx = sa.SafeCX();
            const float cy = sa.SafeCY();
            // Scale unifie (boost mobile inclus).
            const float scale = GetUIScale(W, H);

            DrawGridBackground(r, sa);

            // ── Logo PONG ────────────────────────────────────────────────────
            const float pulse = 0.5f + 0.5f * math::NkSin(t * 2.5f);
            math::NkColor shadowC = theme::Cyan();
            shadowC.a = (uint8_t)((0.55f + 0.30f * pulse) * 255);
            const float logoY = cy - 180.0f * scale;
            f.DrawStringShadowCentered(r, FontAtlas::DisplaySlot,
                                     cx, logoY,
                                     "PONG", theme::White(), shadowC, 3);

            // ── Sous-titre cyan ──────────────────────────────────────────────
            const float subY = logoY + (72.0f + 12.0f) * scale;
            f.DrawStringShadowCentered(r, FontAtlas::SubtitleSlot,
                                     cx, subY, "ULTRA  ARENA  EDITION",
                                     theme::Cyan(), theme::Cyan(), 1);

            // ── Badge "* 1 VS 1 *" orange outline ────────────────────────────
            const char* badge = "*  1 VS 1  *";
            const float badgeTW = f.MeasureWidth(FontAtlas::BodySlot, badge);
            const float badgePadX = 18.0f * scale;
            const float badgePadY =  8.0f * scale;
            const float badgeW = badgeTW + badgePadX * 2.0f;
            const float badgeH = 18.0f * scale + badgePadY * 2.0f;
            const float badgeX = cx - badgeW * 0.5f;
            const float badgeY = subY + 50.0f * scale;
            r.DrawFilledRect({ badgeX, badgeY, badgeW, badgeH }, AlphaF(theme::Orange(), 0.12f));
            r.DrawRectOutline({ badgeX, badgeY, badgeW, badgeH }, AlphaF(theme::Orange(), 0.55f), 1.0f);
            f.DrawStringCentered(r, FontAtlas::BodySlot,
                               cx, badgeY + badgePadY,
                               badge, theme::Orange());

            // ── Mini-field anime ────────────────────────────────────────────
            const float mfW = 280.0f * scale;
            const float mfH =  80.0f * scale;
            const float mfX = cx - mfW * 0.5f;
            const float mfY = badgeY + badgeH + 28.0f * scale;
            DrawMiniField(r, mfX, mfY, mfW, mfH, t);

            // ── CTA "APPUYER POUR JOUER" ────────────────────────────────────
            const char* cta = "APPUYER POUR JOUER";
            const float ctaTW = f.MeasureWidth(FontAtlas::BodySlot, cta);
            const float ctaPadX = 28.0f * scale;
            const float ctaPadY = 12.0f * scale;
            const float ctaW = ctaTW + ctaPadX * 2.0f;
            const float ctaH = 18.0f * scale + ctaPadY * 2.0f;
            const float ctaX = cx - ctaW * 0.5f;
            const float ctaY = mfY + mfH + 36.0f * scale;
            const float blink = (math::NkSin(t * 3.4f) > 0.0f) ? 1.0f : 0.45f;
            const math::NkColor borderC = AlphaF(theme::Cyan(), blink);
            r.DrawFilledRect({ ctaX, ctaY, ctaW, ctaH }, AlphaF(theme::Cyan(), blink * 0.08f));
            r.DrawRectOutline({ ctaX, ctaY, ctaW, ctaH }, borderC, 2.0f);
            f.DrawStringCentered(r, FontAtlas::BodySlot,
                               cx, ctaY + ctaPadY,
                               cta, AlphaF(theme::Cyan(), 0.95f));

            // ── Credits (zone safe basse) ────────────────────────────────────
            const char* credits = "(C) 2026  PONG ULTRA ARENA  -  v1.1  -  DESIGNED BY RIHEN";
            f.DrawStringCentered(r, FontAtlas::SmallSlot,
                               cx, sa.BottomY(12.0f) - 16.0f,
                               credits, AlphaF(theme::White(), 0.30f));
        }

    } // namespace pong
} // namespace nkentseu
