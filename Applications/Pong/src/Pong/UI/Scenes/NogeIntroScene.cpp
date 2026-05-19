// =============================================================================
// NogeIntroScene.cpp
// -----------------------------------------------------------------------------
// Logo NOGE dessine entierement par primitives (pas de texture). Style neon
// cyan/orange coherent avec le theme. Design : hexagone qui se trace +
// "POWERED BY" + "NOGE" + "Game Engine".
// =============================================================================

#include "NogeIntroScene.h"
#include "SplashScene.h"
#include "Pong/Render/GLRenderer2D.h"
#include "Pong/Render/FontAtlas.h"
#include "Pong/UI/Theme.h"
#include "Pong/UI/SceneManager.h"
#include "Pong/UI/UIScale.h"
#include <cmath>

namespace nkentseu
{
    namespace pong
    {

        // ── Timecodes des phases ─────────────────────────────────────────────
        static constexpr float kT_HexDraw  = 0.80f;  // fin du dessin de l'hexagone
        static constexpr float kT_TextFade = 1.50f;  // fin du fade-in des textes
        static constexpr float kT_Hold     = 2.70f;  // fin de la phase de hold
        static constexpr float kT_End      = 3.30f;  // fin du fade out

        // ─────────────────────────────────────────────────────────────────────
        void NogeIntroScene::OnEnter(AppContext& /*ctx*/)
        {
            mTime = 0.0f;
            mDone = false;
            // Le logo Rihen est plaque statique en bas de la scene Noge —
            // signature du designer (sans animation, juste un fade-in global
            // avec le reste des textes). Source SVG vectorielle : NkImage::Load
            // detecte le format et passe par NkSVGCodec pour rasteriser.
            mRihenLogoLoaded = mRihenLogo.LoadFromFile(
                "Resources/Pong/Textures/logo.svg");
        }

        // ─────────────────────────────────────────────────────────────────────
        void NogeIntroScene::OnExit(AppContext& /*ctx*/)
        {
            mRihenLogo.Shutdown();
            mRihenLogoLoaded = false;
        }

        // ─────────────────────────────────────────────────────────────────────
        void NogeIntroScene::OnUpdate(AppContext& ctx, float dt)
        {
            mTime += dt;
            if (!mDone && mTime >= kT_End)
            {
                mDone = true;
                ctx.scenes->Replace(new SplashScene());
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        // Trace un hexagone (6 segments) progressivement selon @p progress
        // dans [0..1]. Style neon : segments cyan avec une legere lueur via
        // dessin a 3 epaisseurs decroissantes.
        // ─────────────────────────────────────────────────────────────────────
        static void DrawNeonHexagon(GLRenderer2D& r, float cx, float cy, float radius,
                                    float progress, math::NkColor primary, math::NkColor glow)
        {
            const float PI = 3.14159265f;
            const float a0 = -PI * 0.5f;   // commence en haut
            const int N = 6;
            if (progress <= 0.0f) return;
            if (progress > 1.0f) progress = 1.0f;
            // Combien de segments complets a tracer.
            const float totalSeg = static_cast<float>(N) * progress;
            for (int i = 0; i < N; ++i)
            {
                const float seg = totalSeg - static_cast<float>(i);
                if (seg <= 0.0f) break;
                const float pa = a0 + 2.0f * PI * static_cast<float>(i)     / N;
                const float pb = a0 + 2.0f * PI * static_cast<float>(i + 1) / N;
                const float x0 = cx + math::NkCos(pa) * radius;
                const float y0 = cy + math::NkSin(pa) * radius;
                const float fullX1 = cx + math::NkCos(pb) * radius;
                const float fullY1 = cy + math::NkSin(pb) * radius;
                // segment partiellement trace (le dernier en cours d'apparition)
                float t = seg;
                if (t > 1.0f) t = 1.0f;
                const float x1 = x0 + (fullX1 - x0) * t;
                const float y1 = y0 + (fullY1 - y0) * t;
                // Glow : 3 traits de plus en plus fins, primary par-dessus
                r.DrawLine(x0, y0, x1, y1, glow, 6.0f);
                r.DrawLine(x0, y0, x1, y1, AlphaF(glow, 0.85f), 4.0f);
                r.DrawLine(x0, y0, x1, y1, primary, 2.0f);
            }
            // Points lumineux aux sommets traces.
            const int reached = static_cast<int>(math::NkFloor(totalSeg));
            for (int i = 0; i <= reached && i <= N; ++i)
            {
                const float pa = a0 + 2.0f * PI * static_cast<float>(i) / N;
                const float px = cx + math::NkCos(pa) * radius;
                const float py = cy + math::NkSin(pa) * radius;
                r.DrawCircle(px, py, 4.0f, glow, 16);
                r.DrawCircle(px, py, 2.0f, primary, 16);
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        void NogeIntroScene::OnRender(AppContext& ctx)
        {
            GLRenderer2D& r = *ctx.renderer;
            FontAtlas&    f = *ctx.font;
            const int W = ctx.viewportW;
            const int H = ctx.viewportH;
            const float cx = W * 0.5f;
            const float cy = H * 0.5f;
            const float t  = mTime;
            const float scale = GetUIScale(W, H);

            // Fond noir profond + grille fine
            r.Clear(theme::Dark().r / 255.0f,
                    theme::Dark().g / 255.0f,
                    theme::Dark().b / 255.0f, 1.0f);
            r.Begin(W, H);

            // ── Hexagone neon (centre haut) ──────────────────────────────────
            const float hexY = cy - 60.0f * scale;
            const float hexR = 80.0f * scale;
            float hexProgress = 1.0f;
            if (t < kT_HexDraw) hexProgress = t / kT_HexDraw;
            // Rotation lente apres traçage complet
            (void)hexProgress;
            DrawNeonHexagon(r, cx, hexY, hexR, hexProgress,
                            theme::Cyan(), AlphaF(theme::Cyan(), 0.40f));

            // Petit logo central : carre cyan tourne 45 deg (effet "diamond")
            if (t > kT_HexDraw)
            {
                const float dprog = math::NkMin((t - kT_HexDraw) / 0.30f, 1.0f);
                const float dSize = 18.0f * scale * dprog;
                // 4 triangles en diamond
                r.DrawTriangle(cx, hexY - dSize,
                               cx + dSize, hexY,
                               cx, hexY + dSize,
                               AlphaF(theme::Cyan(), 0.95f));
                r.DrawTriangle(cx, hexY - dSize,
                               cx, hexY + dSize,
                               cx - dSize, hexY,
                               AlphaF(theme::Cyan(), 0.95f));
            }

            // ── Textes (fade-in apres l'hexagone) ────────────────────────────
            float textAlpha = 1.0f;
            if (t < kT_HexDraw)
            {
                textAlpha = 0.0f;
            }
            else if (t < kT_TextFade)
            {
                textAlpha = (t - kT_HexDraw) / (kT_TextFade - kT_HexDraw);
            }

            // "POWERED BY" (small, faded gray, au-dessus du logo NOGE)
            f.DrawStringCentered(r, FontAtlas::SmallSlot,
                               cx, hexY + hexR + 24.0f * scale,
                               "POWERED  BY",
                               AlphaF(theme::White(), textAlpha * 0.45f));

            // "NOGE" en grand (subtitle slot, blanc + glow cyan)
            const char* noge = "NOGE";
            f.DrawStringShadowCentered(r, FontAtlas::HeadlineSlot,
                                     cx, hexY + hexR + 44.0f * scale,
                                     noge,
                                     AlphaF(theme::White(), textAlpha),
                                     AlphaF(theme::Cyan(), textAlpha),
                                     2);

            // "GAME ENGINE" sous-titre cyan
            f.DrawStringCentered(r, FontAtlas::BodySlot,
                               cx, hexY + hexR + 110.0f * scale,
                               "GAME  ENGINE",
                               AlphaF(theme::Cyan(), textAlpha * 0.85f));

            // ── Signature designer : logo Rihen statique en bas ──────────────
            // Centre horizontalement, dans la zone safe basse. Hauteur cible
            // ~7% du viewport pour rester discret. Le logo est tres horizontal
            // (4:1) donc la largeur derivee depasse sans probleme la largeur
            // d'un texte "DESIGNED BY RIHEN".
            if (mRihenLogoLoaded)
            {
                const float aspect = mRihenLogo.AspectRatio();
                float logoH = (float)H * 0.07f;
                float logoW = logoH * (aspect > 0.001f ? aspect : 4.0f);
                // Bornage largeur : pas plus de 30% du viewport.
                const float maxW = (float)W * 0.30f;
                if (logoW > maxW) { logoW = maxW; logoH = logoW / aspect; }
                const float logoX = cx - logoW * 0.5f;
                // En bas, avec une petite marge respectant la safe area.
                // Logo legerement decale vers le HAUT (sortie de la zone
                // immediate du bord) — donne plus de respiration visuelle.
                const float bottomY = (float)(ctx.safe.BottomY());
                const float logoY = bottomY - logoH - 32.0f;

                r.BindTexture(mRihenLogo.Id());
                r.DrawTexturedQuadRGBA(logoX, logoY, logoW, logoH,
                                       0.0f, 0.0f, 1.0f, 1.0f,
                                       AlphaF(theme::White(), textAlpha * 0.85f));
            }
            else
            {
                // Fallback texte si le bitmap n'est pas dispo.
                f.DrawStringCentered(r, FontAtlas::SmallSlot,
                                   cx, (float)H - 28.0f,
                                   "DESIGNED BY RIHEN  -  ENGINE v0.1",
                                   AlphaF(theme::White(), textAlpha * 0.30f));
            }

            // ── Fade out global vers le noir en phase finale ─────────────────
            if (t >= kT_Hold)
            {
                const float fadeOutDur = kT_End - kT_Hold;
                float a = (t - kT_Hold) / fadeOutDur;
                if (a > 1.0f) a = 1.0f;
                r.DrawQuad(0.0f, 0.0f, (float)W, (float)H,
                           AlphaF(theme::Dark(), a));
            }

            r.End();
        }

    } // namespace pong
} // namespace nkentseu
