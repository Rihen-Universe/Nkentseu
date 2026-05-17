// =============================================================================
// RihenIntroScene.cpp
// -----------------------------------------------------------------------------
// Intro Rihen — reproduction du logo bitmap par dessin procedural puis
// crossfade vers la texture finale. Layout strictement aligne sur le bitmap
// (Resources/Pong/Textures/logo.png) pour qu'il n'y ait AUCUN deplacement
// pendant le crossfade.
//
// Composition du logo (mesuree sur logo.png, ratio ~4:1) :
//   - bloc cercle bicolore a GAUCHE
//   - texte "RiHEN" a DROITE du cercle, meme hauteur que le cercle
//   - slogan "REVONS ENSEMBLE, REINVENTONS LE FUTUR" sous le bloc principal,
//     etendu sur TOUTE la largeur du logo
//
// Pipeline d'animation (4.5 s) :
//   Phase 0 (0.00 - 0.70s)  Trace du cercle (arc qui s'etend de 0 a 360 deg)
//   Phase 1 (0.70 - 1.80s)  Lettres "RiHEN" apparaissent une par une (fade-in)
//   Phase 2 (1.80 - 2.30s)  Slogan en fade-in a sa position finale
//   Phase 3 (2.30 - 3.50s)  Crossfade procedural -> bitmap (alignement parfait)
//   Phase 4 (3.50 - 4.50s)  Hold sur le bitmap final puis fade out
//
// A la fin de la phase 4, Replace() vers MainMenuScene (skip Noge/Splash).
// =============================================================================

#include "RihenIntroScene.h"
#include "NogeIntroScene.h"
#include "Pong/Render/GLRenderer2D.h"
#include "Pong/Render/FontAtlas.h"
#include "Pong/UI/Theme.h"
#include "Pong/UI/SceneManager.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NkFunctions.h"
#include <cstring>

namespace nkentseu
{
    namespace pong
    {

        // ── Timecodes des phases (secondes) ──────────────────────────────────
        static constexpr float kT_Arc       = 0.70f;
        static constexpr float kT_Letters   = 1.80f;
        static constexpr float kT_Slogan    = 2.30f;
        static constexpr float kT_Crossfade = 3.50f;
        static constexpr float kT_Hold      = 4.00f;
        static constexpr float kT_End       = 4.50f;

        // ── Couleurs mesurees sur logo.png ───────────────────────────────────
        static math::NkColor LogoTeal()   { return {  21,  84,  96, 255 }; }
        static math::NkColor LogoOrange() { return { 243, 152,  15, 255 }; }

        // ── Layout relatif du logo (ratios mesures sur le PNG) ───────────────
        // Le bitmap fait ~1924x480 px. Les ratios ci-dessous decrivent les
        // proportions du cercle / texte / slogan dans cet espace.
        static constexpr float kLogoAspect      = 4.0f;     // bmpW / bmpH

        // Le bloc cercle + RiHEN occupe la moitie superieure du logo.
        // Le cercle est positionne tout a gauche, centre verticalement sur la
        // ligne du bloc cercle+RIHEN (~ 0.40 * bmpH).
        static constexpr float kCircleCYRatio   = 0.40f;    // Y du centre du cercle / bmpH
        static constexpr float kCircleRRatio    = 0.38f;    // rayon / bmpH
        static constexpr float kCircleCXRatio   = 0.105f;   // X du centre / bmpW

        // Le texte "RiHEN" commence juste a droite du cercle.
        static constexpr float kRihenStartXRatio= 0.235f;
        // Hauteur du glyphe de reference (H majuscule) en proportion de bmpH.
        static constexpr float kRihenHRatio     = 0.78f;

        // Le slogan est en BAS du logo, sur toute la largeur, baseline a
        // ~0.92 * bmpH.
        static constexpr float kSloganYRatio    = 0.88f;    // top du slogan / bmpH
        static constexpr float kSloganHRatio    = 0.12f;    // hauteur slogan / bmpH

        // ── Easing ───────────────────────────────────────────────────────────
        static float EaseOutCubic(float t)
        {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            const float u = 1.0f - t;
            return 1.0f - u * u * u;
        }

        // ─────────────────────────────────────────────────────────────────────
        void RihenIntroScene::OnEnter(AppContext& /*ctx*/)
        {
            mTime = 0.0f;
            mDone = false;
            // Charge le logo bitmap final. Si echec on reste sur le procedural
            // pur (la scene reste valide visuellement).
            mLogoLoaded = mLogo.LoadFromFile("Resources/Pong/Textures/logo.png");
            logger.Info("[RihenIntro] OnEnter - logo loaded = {0}",
                        mLogoLoaded ? "true" : "false");
        }

        void RihenIntroScene::OnExit(AppContext& /*ctx*/)
        {
            mLogo.Shutdown();
            mLogoLoaded = false;
        }

        void RihenIntroScene::OnUpdate(AppContext& ctx, float dt)
        {
            mTime += dt;
            if (!mDone && mTime >= kT_End)
            {
                mDone = true;
                // Apres l'intro Rihen on enchaine sur l'intro du moteur Noge.
                // Le logo Rihen sera reaffiche statique en bas de la scene
                // Noge (signature designer).
                ctx.scenes->Replace(new NogeIntroScene());
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        // DrawArc — trace un anneau (cercle plein partiel) entre les angles
        // a0 et a1. Utilise pour le "trace progressif" du cercle.
        // Implementation : on remplit le disque par triangles fan limites a
        // l'angle courant, ce qui donne l'effet d'un cercle qui "se dessine".
        // bicolor=true : haut = teal, bas = orange (comme dans le logo).
        // ─────────────────────────────────────────────────────────────────────
        static void DrawDiscBicolorArc(GLRenderer2D& r,
                                       float cx, float cy, float radius,
                                       float a0, float a1, int segments,
                                       float globalAlpha)
        {
            if (segments < 8) segments = 8;
            if (a1 <= a0)     return;
            const float PI = 3.14159265f;
            // Couleurs avec alpha global
            math::NkColor teal   = LogoTeal();
            math::NkColor orange = LogoOrange();
            teal.a   = static_cast<uint8_t>(255.0f * globalAlpha);
            orange.a = static_cast<uint8_t>(255.0f * globalAlpha);

            for (int i = 0; i < segments; ++i)
            {
                const float ta = a0 + (a1 - a0) * static_cast<float>(i)     / segments;
                const float tb = a0 + (a1 - a0) * static_cast<float>(i + 1) / segments;
                const float x0 = cx + math::NkCos(ta) * radius;
                const float y0 = cy + math::NkSin(ta) * radius;
                const float x1 = cx + math::NkCos(tb) * radius;
                const float y1 = cy + math::NkSin(tb) * radius;
                // Couleur du triangle : on prend la couleur du milieu de
                // l'arc (sin > 0 = bas en Y screen => orange, sin < 0 = haut
                // => teal). Repere : Y croit vers le BAS a l'ecran.
                const float midA  = 0.5f * (ta + tb);
                const float sinM  = math::NkSin(midA);
                const math::NkColor col = (sinM > 0.0f) ? orange : teal;
                r.DrawTriangle(cx, cy, x0, y0, x1, y1, col);
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        // OnRender — assemblage des phases, layout aligne sur le bitmap.
        // ─────────────────────────────────────────────────────────────────────
        void RihenIntroScene::OnRender(AppContext& ctx)
        {
            GLRenderer2D& r = *ctx.renderer;
            FontAtlas&    f = *ctx.font;
            const int W = ctx.viewportW;
            const int H = ctx.viewportH;
            const float cx = ctx.safe.SafeCX();
            const float cy = ctx.safe.SafeCY();
            const float t  = mTime;

            // Fond noir profond
            r.Clear(theme::Dark().r / 255.0f,
                    theme::Dark().g / 255.0f,
                    theme::Dark().b / 255.0f, 1.0f);
            r.Begin(W, H);

            // ── Dimensionnement du logo a l'ecran (espace de reference) ─────
            // On choisit une largeur cible qui rentre dans la zone safe sans
            // trop deborder. Le logo etant tres horizontal (4:1), on prend
            // ~70% de la largeur safe et on calcule la hauteur deduite.
            const float safeW = static_cast<float>(ctx.safe.SafeW());
            const float safeH = static_cast<float>(ctx.safe.SafeH());
            float bmpW = safeW * 0.70f;
            float bmpH = bmpW / kLogoAspect;
            // Bornage : si la hauteur est trop grande on contraint par H.
            if (bmpH > safeH * 0.55f)
            {
                bmpH = safeH * 0.55f;
                bmpW = bmpH * kLogoAspect;
            }
            const float bmpX = cx - bmpW * 0.5f;
            const float bmpY = cy - bmpH * 0.5f;

            // Positions calculees dans l'espace du bitmap.
            const float circleCX = bmpX + bmpW * kCircleCXRatio;
            const float circleCY = bmpY + bmpH * kCircleCYRatio;
            const float circleR  = bmpH * kCircleRRatio;

            // ── Phase 0 : Trace du cercle (arc qui s'etend) ──────────────────
            // L'arc part du haut (-PI/2) et progresse dans le sens horaire
            // (clockwise = sin croissant = a -> a + delta).
            float arcProgress = 0.0f;
            if (t > 0.0f)
            {
                float lt = t / kT_Arc;
                if (lt > 1.0f) lt = 1.0f;
                arcProgress = EaseOutCubic(lt);
            }
            // Alpha global du procedural — passe a 0 pendant le crossfade.
            float procAlpha = 1.0f;
            if (mLogoLoaded && t >= kT_Slogan)
            {
                const float fadeT = (t - kT_Slogan) / (kT_Crossfade - kT_Slogan);
                procAlpha = 1.0f - EaseOutCubic(math::NkMin(fadeT, 1.0f));
                if (procAlpha < 0.0f) procAlpha = 0.0f;
            }

            if (arcProgress > 0.001f && procAlpha > 0.001f)
            {
                const float PI = 3.14159265f;
                const float a0 = -PI * 0.5f;                       // top
                const float a1 = a0 + 2.0f * PI * arcProgress;     // clockwise
                DrawDiscBicolorArc(r, circleCX, circleCY, circleR,
                                   a0, a1, 96, procAlpha);
            }

            // ── Phase 1 : Lettres "RiHEN" apparaissent une par une ───────────
            // Le texte du logo est exactement "RiHEN" (i minuscule). On
            // mesure la chaine totale pour son rendu final, mais on l'affiche
            // lettre par lettre avec fade-in individuel.
            const char* letters[5] = { "R", "i", "H", "E", "N" };
            const int   nLetters   = 5;

            // Choix du slot de police selon la hauteur cible : le moteur de
            // texte ne supporte pas de scale arbitraire, on se contente d'un
            // des 5 slots prefabriques (14/18/28/48/72 px).
            const float rihenTargetH = bmpH * kRihenHRatio;
            FontAtlas::SizeSlot rihenSlot = FontAtlas::DisplaySlot;
            float rihenSlotPx = 72.0f;
            if (rihenTargetH < 60.0f) { rihenSlot = FontAtlas::HeadlineSlot; rihenSlotPx = 48.0f; }
            if (rihenTargetH < 36.0f) { rihenSlot = FontAtlas::SubtitleSlot; rihenSlotPx = 28.0f; }

            // Y baseline du texte aligne sur le centre du cercle, comme dans
            // le logo (le bloc cercle+RIHEN partage la meme mediane).
            const float rihenTopY = circleCY - rihenSlotPx * 0.5f;
            const float rihenStartX = bmpX + bmpW * kRihenStartXRatio;

            if (t >= kT_Arc && procAlpha > 0.001f)
            {
                const float letterPeriod = (kT_Letters - kT_Arc) / nLetters;
                const float letterDur    = letterPeriod * 2.5f;  // fort overlap

                float curX = rihenStartX;
                for (int i = 0; i < nLetters; ++i)
                {
                    const float lStart = kT_Arc + i * letterPeriod;
                    float lt = (t - lStart) / letterDur;
                    if (lt <= 0.0f)
                    {
                        // Lettre pas encore visible — on avance quand meme le
                        // curseur pour ne pas decaler les suivantes.
                        curX += f.MeasureWidth(rihenSlot, letters[i]);
                        continue;
                    }
                    if (lt > 1.0f) lt = 1.0f;
                    const float alpha = EaseOutCubic(lt) * procAlpha;
                    f.DrawString(r, rihenSlot,
                                 curX, rihenTopY, letters[i],
                                 AlphaF(LogoTeal(), alpha));
                    curX += f.MeasureWidth(rihenSlot, letters[i]);
                }
            }

            // ── Phase 2 : Slogan en fade-in a sa position dans le logo ───────
            // Le slogan est positionne EXACTEMENT a sa place dans le bitmap :
            // centre horizontalement dans le bmp (du bord gauche du cercle au
            // bord droit du N), top a (bmpY + bmpH * kSloganYRatio).
            const char* slogan = "REVONS ENSEMBLE, REINVENTONS LE FUTUR";
            const float sloganTargetH = bmpH * kSloganHRatio;
            FontAtlas::SizeSlot sloganSlot = FontAtlas::SubtitleSlot;
            if (sloganTargetH < 22.0f) sloganSlot = FontAtlas::BodySlot;
            if (sloganTargetH > 40.0f) sloganSlot = FontAtlas::HeadlineSlot;

            const float sloganTopY = bmpY + bmpH * kSloganYRatio;
            // Largeur estimee du slogan
            const float sloganW = f.MeasureWidth(sloganSlot, slogan);
            // Le slogan est centre sur la largeur du logo (et non du
            // viewport) — c'est ce que fait le bitmap.
            const float sloganX = bmpX + (bmpW - sloganW) * 0.5f;

            if (t >= kT_Letters && procAlpha > 0.001f)
            {
                const float lt = (t - kT_Letters) / (kT_Slogan - kT_Letters);
                const float fadeT = (lt > 1.0f) ? 1.0f : lt;
                const float alpha = EaseOutCubic(fadeT) * procAlpha;
                f.DrawString(r, sloganSlot,
                             sloganX, sloganTopY, slogan,
                             AlphaF(LogoTeal(), alpha));
            }

            // ── Phase 3 : Crossfade procedural -> bitmap ─────────────────────
            // Le bitmap est dessine a la MEME position que le bloc procedural
            // (bmpX, bmpY, bmpW, bmpH). Les elements proceduraux ont leur
            // alpha attenue par procAlpha calcule plus haut.
            if (mLogoLoaded && t >= kT_Slogan)
            {
                const float bmpFadeT = (t - kT_Slogan) / (kT_Crossfade - kT_Slogan);
                float bmpAlpha = EaseOutCubic(math::NkMin(bmpFadeT, 1.0f));
                if (bmpAlpha < 0.0f) bmpAlpha = 0.0f;

                r.BindTexture(mLogo.Id());
                r.DrawTexturedQuadRGBA(bmpX, bmpY, bmpW, bmpH,
                                       0.0f, 0.0f, 1.0f, 1.0f,
                                       AlphaF(theme::White(), bmpAlpha));
            }

            // ── Phase 4 : Fade out vers le noir ──────────────────────────────
            if (t >= kT_Hold)
            {
                float a = (t - kT_Hold) / (kT_End - kT_Hold);
                if (a > 1.0f) a = 1.0f;
                a = EaseOutCubic(a);
                r.DrawQuad(0.0f, 0.0f, (float)W, (float)H,
                           AlphaF(theme::Dark(), a));
            }

            r.End();
        }

    } // namespace pong
} // namespace nkentseu
