// =============================================================================
// GameplayScene.cpp
// -----------------------------------------------------------------------------
// Implementation — reproduit 1:1 le comportement de docs/02_terrain_de_jeu_1v1.html
// (sans obstacles ni power-ups dans cette tranche minimale).
// =============================================================================

#include "GameplayScene.h"
#include "Pong/Render/GLRenderer2D.h"
#include "Pong/Render/FontAtlas.h"
#include "Pong/UI/Theme.h"
#include "Pong/UI/SceneManager.h"
#include "Pong/UI/UIScale.h"
#include "NKLogger/NkLog.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace nkentseu
{
    namespace pong
    {

        // ── Constantes HTML brutes (px/frame a 60 fps, dimensions a 1280x720) ─
        // Multipliees par mScale lors de l'usage pour rester visuellement
        // proportionnelles a n'importe quel viewport (cf. UIScale.h).
        static constexpr float kHUDTopHBase  = 56.0f;
        static constexpr float kHUDBotHBase  = 52.0f;
        static constexpr float kBounceVyBase = 4.5f;
        static constexpr float kBounceMul    = 1.04f;       // sans dimension
        static constexpr float kSpeedCapBase = 9.0f;
        static constexpr float kGoalCooldown = 0.8f;        // secondes (pas de scale)
        static constexpr float kPaddleWBase  = 10.0f;
        static constexpr float kPaddleHBase  = 60.0f;
        static constexpr float kBallRBase    =  7.0f;
        static constexpr float kPaddleSpdBase=  4.0f;
        static constexpr float kBallVxInit   =  3.5f;
        static constexpr float kBallVyInit   =  2.2f;
        static constexpr float kPaddleXMargin= 18.0f;       // x=18 et x=W-28 (-10 paddle)

        // ── Couleurs HTML mesurees ───────────────────────────────────────────
        static math::NkColor ColP1()  { return {   0, 245, 255, 255 }; } // cyan
        static math::NkColor ColP2()  { return { 255, 107,   0, 255 }; } // orange
        static math::NkColor ColField(){return {   0, 245, 255,  30 }; } // border arene
        static math::NkColor ColDash() { return { 255, 255, 255,  20 }; } // ligne pointillee
        static math::NkColor ColRing() { return { 255, 255, 255,  18 }; } // cercle central

        // ── Random helper (range [-1, 1]) ────────────────────────────────────
        static float Rand11()
        {
            return ((float)std::rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        }

        // ─────────────────────────────────────────────────────────────────────
        // Lifecycle
        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::OnEnter(AppContext& ctx)
        {
            mScoreL = 0;
            mScoreR = 0;
            mPaused = false;
            mGoalCooldown   = 0.0f;
            mGoalFlashAlpha = 0.0f;
            mGoalFlashSide  = 0;
            mTimeLeft       = 154.0f;
            mTrailHead = 0;
            mTrailCount = 0;
            mKeyW = mKeyS = mKeyUp = mKeyDown = false;

            // Scale calcule depuis le viewport courant (cf. UIScale.h).
            // Applique a toutes les dimensions et vitesses HTML brutes.
            mScale     = GetUIScale(ctx.viewportW, ctx.viewportH);
            mHUDTopH = math::NkMin(kHUDTopHBase * mScale,
                                   (float)ctx.viewportH * 0.12f);
            mHUDBotH = math::NkMin(kHUDBotHBase * mScale,
                                   (float)ctx.viewportH * 0.12f);
            mPaddleW   = kPaddleWBase  * mScale;
            mPaddleH   = kPaddleHBase  * mScale;
            mBallR     = kBallRBase    * mScale;
            mPaddleSpd = kPaddleSpdBase* mScale;

            const float arenaW = (float)ctx.viewportW;
            const float arenaH = math::NkMax(1.0f, (float)ctx.viewportH - mHUDTopH - mHUDBotH);
            ResetPositions(arenaW, arenaH);

            // Vitesse initiale = HTML * scale (la balle traverse le terrain
            // dans le meme temps qu'a 1280x720, peu importe le viewport).
            mBallVX = kBallVxInit * mScale;
            mBallVY = kBallVyInit * mScale;
            logger.Info("[Gameplay] OnEnter arena {0}x{1} scale={2}",
                        (int)arenaW, (int)arenaH, mScale);
        }

        void GameplayScene::OnPause(AppContext& /*ctx*/)
        {
            // Auto-pause systeme : Hidden / FocusLost / app en background.
            // L'overlay pause s'affichera + le user verra "REPRENDRE" au
            // retour de l'app (NkWindowShownEvent -> OnResume, mais on laisse
            // la pause active pour qu'il valide manuellement la reprise).
            if (!mPaused)
            {
                mPaused = true;
                logger.Info("[Gameplay] Auto-pause (lifecycle)");
            }
            // Libere les inputs tenus pour eviter qu'au resume le paddle
            // continue de bouger tout seul.
            mKeyW = mKeyS = mKeyUp = mKeyDown = false;
            mMouseDownL = mMouseDownR = false;
            mTouchIdL = mTouchIdR = -1;
        }

        void GameplayScene::ResetPositions(float arenaW, float arenaH)
        {
            const float cx = arenaW * 0.5f;
            const float cy = arenaH * 0.5f;
            mBallX = cx;
            mBallY = cy;
            mTrailHead = 0;
            mTrailCount = 0;
            const float margin = kPaddleXMargin * mScale;
            mPaddleLX = margin;
            mPaddleLY = cy - mPaddleH * 0.5f;
            mPaddleRX = arenaW - margin - mPaddleW;
            mPaddleRY = cy - mPaddleH * 0.5f;
        }

        void GameplayScene::OnUpdate(AppContext& ctx, float dt)
        {
            if (mPaused) return;
            // Re-sync scale chaque frame au cas ou le viewport ait change
            // (resize desktop). Sur Android landscape locke c'est constant.
            const float newScale = GetUIScale(ctx.viewportW, ctx.viewportH);
            if (math::NkFabs(newScale - mScale) > 0.001f)
            {
                mScale     = newScale;
                mHUDTopH = math::NkMin(kHUDTopHBase * mScale,
                                       (float)ctx.viewportH * 0.12f);
                mHUDBotH = math::NkMin(kHUDBotHBase * mScale,
                                       (float)ctx.viewportH * 0.12f);
                mPaddleW   = kPaddleWBase  * mScale;
                mPaddleH   = kPaddleHBase  * mScale;
                mBallR     = kBallRBase    * mScale;
                mPaddleSpd = kPaddleSpdBase* mScale;
            }
            const float arenaW = (float)ctx.viewportW;
            const float arenaH = math::NkMax(1.0f, (float)ctx.viewportH - mHUDTopH - mHUDBotH);

            // ── Re-positionne X des paddles chaque frame ────────────────────
            // Le paddle droit doit toujours coller au bord droit du viewport
            // meme si la fenetre est resized en cours de partie. Le paddle
            // gauche reste a sa marge. On clamp aussi Y pour ne pas sortir
            // si arenaH a diminue.
            const float marginX = kPaddleXMargin * mScale;
            mPaddleLX = marginX;
            mPaddleRX = arenaW - marginX - mPaddleW;
            if (mPaddleLY > arenaH - mPaddleH) mPaddleLY = arenaH - mPaddleH;
            if (mPaddleRY > arenaH - mPaddleH) mPaddleRY = arenaH - mPaddleH;
            if (mPaddleLY < 0.0f) mPaddleLY = 0.0f;
            if (mPaddleRY < 0.0f) mPaddleRY = 0.0f;

            // Conversion px/frame@60fps -> distance reelle ce frame.
            const float dt60 = dt * 60.0f;

            // Goal cooldown : on attend avant que la balle reprenne ses
            // collisions normales pour eviter les re-buts en boucle.
            if (mGoalCooldown > 0.0f)
            {
                mGoalCooldown -= dt;
                if (mGoalCooldown < 0.0f) mGoalCooldown = 0.0f;
            }
            // Flash de but : decroit en ~350 ms.
            if (mGoalFlashAlpha > 0.0f)
            {
                mGoalFlashAlpha -= dt / 0.35f;
                if (mGoalFlashAlpha < 0.0f) mGoalFlashAlpha = 0.0f;
            }

            // Timer descendant
            if (mTimeLeft > 0.0f) mTimeLeft -= dt;
            if (mTimeLeft < 0.0f) mTimeLeft = 0.0f;

            StepPaddles(arenaH, dt60);
            StepBall   (arenaW, arenaH, dt60);
        }

        // ─────────────────────────────────────────────────────────────────────
        // StepPaddles — W/S et fleches, vitesse 4 px/frame a 60fps. Clamp aux
        // bords de l'arene (mg=0 dans le HTML).
        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::StepPaddles(float arenaH, float dt60)
        {
            const float spd = mPaddleSpd * dt60;
            if (mKeyW)    mPaddleLY -= spd;
            if (mKeyS)    mPaddleLY += spd;
            if (mKeyUp)   mPaddleRY -= spd;
            if (mKeyDown) mPaddleRY += spd;
            // Clamp
            if (mPaddleLY < 0.0f)            mPaddleLY = 0.0f;
            if (mPaddleLY > arenaH - mPaddleH) mPaddleLY = arenaH - mPaddleH;
            if (mPaddleRY < 0.0f)            mPaddleRY = 0.0f;
            if (mPaddleRY > arenaH - mPaddleH) mPaddleRY = arenaH - mPaddleH;
        }

        // ─────────────────────────────────────────────────────────────────────
        // StepBall — meme logique que moveBall() du HTML, en dt60.
        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::StepBall(float arenaW, float arenaH, float dt60)
        {
            // Trail : push la position actuelle AVANT de bouger (comme le HTML).
            mTrailX[mTrailHead] = mBallX;
            mTrailY[mTrailHead] = mBallY;
            mTrailHead = (mTrailHead + 1) % kTrailMax;
            if (mTrailCount < kTrailMax) mTrailCount++;

            mBallX += mBallVX * dt60;
            mBallY += mBallVY * dt60;

            // Rebond haut / bas
            if (mBallY - mBallR < 0.0f)
            {
                mBallY = mBallR;
                mBallVY = math::NkFabs(mBallVY);
            }
            if (mBallY + mBallR > arenaH)
            {
                mBallY = arenaH - mBallR;
                mBallVY = -math::NkFabs(mBallVY);
            }

            // Sortie laterale = but (avec cooldown)
            if (mGoalCooldown <= 0.0f)
            {
                if (mBallX - mBallR < 0.0f)        { TriggerGoal(+1, arenaW, arenaH); return; }
                if (mBallX + mBallR > arenaW)      { TriggerGoal(-1, arenaW, arenaH); return; }
            }

            // Collision paddles (left puis right)
            for (int i = 0; i < 2; ++i)
            {
                const float px = (i == 0) ? mPaddleLX : mPaddleRX;
                const float py = (i == 0) ? mPaddleLY : mPaddleRY;
                const float pw = mPaddleW;
                const float ph = mPaddleH;
                if (mBallX + mBallR > px && mBallX - mBallR < px + pw
                 && mBallY + mBallR > py && mBallY - mBallR < py + ph)
                {
                    // Direction selon position sur le terrain (HTML : b.x<W/2 ? 1 : -1)
                    const float dir = (mBallX < arenaW * 0.5f) ? 1.0f : -1.0f;
                    mBallVX = math::NkFabs(mBallVX) * dir;
                    const float rel = (mBallY - (py + ph * 0.5f)) / (ph * 0.5f);
                    mBallVY = rel * kBounceVyBase * mScale;
                    // Acceleration speed = min(sqrt(...) * 1.04, 9 * scale)
                    float spd2 = mBallVX * mBallVX + mBallVY * mBallVY;
                    float spd  = math::NkSqrt(spd2) * kBounceMul;
                    const float speedCap = kSpeedCapBase * mScale;
                    if (spd > speedCap) spd = speedCap;
                    const float ang = math::NkAtan2(mBallVY, mBallVX);
                    mBallVX = math::NkCos(ang) * spd;
                    mBallVY = math::NkSin(ang) * spd;
                    break;  // une seule collision paddle par frame
                }
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        // TriggerGoal — side : +1 = c'est P1 (gauche) qui marque (balle sortie
        // a droite), -1 = c'est P2 (droite) qui marque.
        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::TriggerGoal(int side, float arenaW, float arenaH)
        {
            mGoalCooldown   = kGoalCooldown;
            mGoalFlashAlpha = 1.0f;
            mGoalFlashSide  = side;
            if (side > 0) mScoreL++;
            else          mScoreR++;
            logger.Info("[Gameplay] GOAL P{0} ! score {1} - {2}",
                        side > 0 ? 1 : 2, mScoreL, mScoreR);

            // Reset balle au centre. Direction : on relance vers le perdant.
            mBallX = arenaW * 0.5f;
            mBallY = arenaH * 0.5f;
            mTrailHead = 0;
            mTrailCount = 0;
            // HTML : side === 'L' (i.e. P1 a marque, b sortie a gauche) -> vx=-3.5
            //        side === 'R' -> vx=+3.5
            // Ici side=+1 = P1 a marque (balle sortie a droite) -> on l'envoie a droite
            //      side=-1 = P2 a marque (balle sortie a gauche) -> on l'envoie a gauche
            mBallVX = (side > 0 ? 1.0f : -1.0f) * kBallVxInit * mScale;
            mBallVY = Rand11() * 2.0f * mScale;  // (rand-0.5)*4 du HTML = Rand11()*2
        }

        // ─────────────────────────────────────────────────────────────────────
        // Rendu
        // ─────────────────────────────────────────────────────────────────────
        static void DrawArenaField(GLRenderer2D& r,
                                   float ax, float ay, float aw, float ah,
                                   float scale)
        {
            // Grille fine cyan : step et epaisseur scaled
            const float gridStep = 32.0f * scale;
            const float lineThick = math::NkMax(1.0f, scale);
            const math::NkColor gridCol = { 0, 245, 255, 10 };
            for (float x = ax; x < ax + aw; x += gridStep)
            {
                r.DrawQuad(x, ay, lineThick, ah, gridCol);
            }
            for (float y = ay; y < ay + ah; y += gridStep)
            {
                r.DrawQuad(ax, y, aw, lineThick, gridCol);
            }
            // Bordure cyan
            r.DrawQuadOutline(ax, ay, aw, ah, ColField(), lineThick);
            // Ligne centrale pointillee (dash 8 / gap 8 scales)
            const float midX = ax + aw * 0.5f;
            const float dashLen = 8.0f * scale;
            const float dashStep = 16.0f * scale;
            for (float yy = ay + 4.0f * scale; yy < ay + ah - 4.0f * scale; yy += dashStep)
            {
                r.DrawQuad(midX - lineThick * 0.5f, yy, lineThick, dashLen, ColDash());
            }
            // Cercle central r=50 scaled
            r.DrawCircleOutline(midX, ay + ah * 0.5f, 50.0f * scale,
                                ColRing(), lineThick, 48);
            // Goal glows lateraux (4 bandes degrade) scales
            const float bandW = 10.0f * scale;
            for (int i = 0; i < 4; ++i)
            {
                const float a = 1.0f - (float)i / 4.0f;
                math::NkColor cl = ColP1(); cl.a = static_cast<uint8_t>(45 * a);
                math::NkColor cr = ColP2(); cr.a = static_cast<uint8_t>(45 * a);
                r.DrawQuad(ax + i * bandW,           ay, bandW, ah, cl);
                r.DrawQuad(ax + aw - (i + 1) * bandW, ay, bandW, ah, cr);
            }
        }

        // Strategie texte responsive :
        //   - On garde un slot de base raisonnable (Body 18px par defaut)
        //   - On scale la rasterisation via DrawStringScaled(scale = mScale)
        //   - Resultat : texte 18 * 2.25 = 40px sur S22+ landscape, lisible.
        // Note : l'atlas raster est en GL_LINEAR donc l'upscale jusqu'a 3x
        // reste correct visuellement.

        static void DrawHUDTop(GLRenderer2D& r, FontAtlas& f,
                               int W, float hudH, float scale,
                               int scoreL, int scoreR, float timeLeft)
        {
            r.DrawQuad(0, 0, (float)W, hudH, { 5, 10, 20, 235 });
            r.DrawQuad(0, hudH - 1, (float)W, 1.0f, { 0, 245, 255, 26 });

            // Slots de base : on garde des slots raisonnables et on applique
            // mScale via DrawStringScaled. Sur mobile le texte sera donc
            // automatiquement scale (~2.5x sur S22+).
            const FontAtlas::SizeSlot slotLabel = FontAtlas::SmallSlot;   // 14
            const FontAtlas::SizeSlot slotScore = FontAtlas::SubtitleSlot;// 28
            const FontAtlas::SizeSlot slotCenter= FontAtlas::BodySlot;    // 18
            const FontAtlas::SizeSlot slotMatch = FontAtlas::SmallSlot;   // 14

            const float marginX = 20.0f * scale;
            const float avatarR = 14.0f * scale;
            const float avatarR2= 12.0f * scale;
            const float gx = marginX;
            const float cyH = hudH * 0.5f;
            r.DrawCircleOutline(gx + avatarR, cyH, avatarR,
                                ColP1(), 2.0f * scale, 32);
            r.DrawCircle(gx + avatarR, cyH, avatarR2,
                         { 0, 245, 255, 38 }, 32);
            f.DrawStringCenteredScaled(r, slotLabel, scale,
                               gx + avatarR, cyH - 7.0f * scale,
                               "P1", ColP1());
            const float textOff = (avatarR * 2.0f) + 10.0f * scale;
            f.DrawStringScaled(r, slotLabel, scale,
                         gx + textOff, 10.0f * scale, "JOUEUR 1", ColP1());
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d", scoreL);
            f.DrawStringShadowScaled(r, slotScore, scale,
                             gx + textOff, 22.0f * scale, buf,
                             { 255, 255, 255, 255 }, ColP1(), 2);

            const int m = (int)(timeLeft) / 60;
            const int s = (int)(timeLeft) % 60;
            char tbuf[16];
            std::snprintf(tbuf, sizeof(tbuf), "%02d:%02d", m, s);
            f.DrawStringCenteredScaled(r, slotCenter, scale,
                               (float)W * 0.5f, 6.0f * scale, tbuf,
                               { 255, 255, 255, 240 });
            f.DrawStringCenteredScaled(r, slotMatch, scale,
                               (float)W * 0.5f, 30.0f * scale,
                               "1 VS 1 - 11 PTS",
                               { 255, 255, 255, 80 });

            const float dx = (float)W - marginX;
            r.DrawCircleOutline(dx - avatarR, cyH, avatarR,
                                ColP2(), 2.0f * scale, 32);
            r.DrawCircle(dx - avatarR, cyH, avatarR2,
                         { 255, 107, 0, 38 }, 32);
            f.DrawStringCenteredScaled(r, slotLabel, scale,
                               dx - avatarR, cyH - 7.0f * scale,
                               "P2", ColP2());
            std::snprintf(buf, sizeof(buf), "%d", scoreR);
            const float scoreW = f.MeasureWidthScaled(slotScore, scale, buf);
            const float nameW  = f.MeasureWidthScaled(slotLabel, scale, "JOUEUR 2");
            f.DrawStringScaled(r, slotLabel, scale,
                         dx - textOff - nameW, 10.0f * scale, "JOUEUR 2", ColP2());
            f.DrawStringShadowScaled(r, slotScore, scale,
                             dx - textOff - scoreW, 22.0f * scale, buf,
                             { 255, 255, 255, 255 }, ColP2(), 2);
        }

        static void DrawHUDBottom(GLRenderer2D& r, FontAtlas& f,
                                  int W, int H, float hudH, float scale,
                                  bool paused)
        {
            const float by = (float)H - hudH;
            r.DrawQuad(0, by, (float)W, hudH, { 5, 10, 20, 235 });
            r.DrawQuad(0, by, (float)W, 1.0f, { 0, 245, 255, 26 });

            const FontAtlas::SizeSlot slotHint = FontAtlas::SmallSlot;

            f.DrawStringCenteredScaled(r, slotHint, scale,
                               (float)W * 0.5f, by + hudH * 0.20f,
                               "P1 : W / S    OU GLISSER GAUCHE          P2 : FLECHES    OU GLISSER DROITE",
                               { 255, 255, 255, 100 });
            f.DrawStringCenteredScaled(r, slotHint, scale,
                               (float)W * 0.5f, by + hudH * 0.55f,
                               paused ? "ESPACE / TAP : REPRENDRE     ECHAP : MENU"
                                      : "ESPACE / TAP : PAUSE     ECHAP : MENU",
                               { 255, 255, 255, 70 });
        }

        // ─────────────────────────────────────────────────────────────────────
        // Trail + balle avec effet de lumiere (halo blanc + glow cyan).
        // Reproduit l'effet shadowBlur=18 du HTML (couches concentriques).
        // ─────────────────────────────────────────────────────────────────────
        static void DrawBallAndTrail(GLRenderer2D& r,
                                     float ax, float ay,
                                     float bx, float by, float br,
                                     const float* trX, const float* trY,
                                     int trHead, int trCount, int trMax)
        {
            // Trail : iterer du plus ancien au plus recent, taille croissante.
            // Chaque point a un halo cyan + un cercle blanc.
            const int oldest = (trHead - trCount + trMax) % trMax;
            for (int i = 0; i < trCount; ++i)
            {
                const int idx = (oldest + i) % trMax;
                const float t = (float)(i + 1) / (float)trCount;  // 0..1
                const float rad = br * t * 0.85f;
                // Halo cyan large (shadowBlur=8 du HTML)
                math::NkColor glow = { 0, 245, 255, static_cast<uint8_t>(110.0f * t) };
                r.DrawCircle(ax + trX[idx], ay + trY[idx], rad * 1.8f, glow, 16);
                // Cercle blanc translucide
                math::NkColor c = { 255, 255, 255, static_cast<uint8_t>(180.0f * t) };
                r.DrawCircle(ax + trX[idx], ay + trY[idx], rad, c, 16);
            }

            // Balle : halo 5 couches decroissantes (shadowBlur=18 du HTML)
            // De l'exterieur vers l'interieur, alpha croissant.
            for (int i = 5; i >= 1; --i)
            {
                const float radius = br + (float)i * 3.5f;
                // Mix cyan -> blanc en se rapprochant du centre
                const float t = 1.0f - (float)i / 5.0f;  // 0 a l'exterieur, 1 au centre
                math::NkColor halo;
                halo.r = static_cast<uint8_t>(  0 + 255.0f * t);
                halo.g = static_cast<uint8_t>(245 +  10.0f * t);
                halo.b = static_cast<uint8_t>(255);
                halo.a = static_cast<uint8_t>(30 + 40 * t);
                r.DrawCircle(ax + bx, ay + by, radius, halo, 24);
            }
            // Cercle blanc plein au centre
            r.DrawCircle(ax + bx, ay + by, br, { 255, 255, 255, 255 }, 24);
        }

        // ─────────────────────────────────────────────────────────────────────
        // Paddle avec halo (shadowBlur=20 du HTML). Couleur du halo = couleur
        // du paddle. 6 couches concentriques pour simuler le blur.
        // ─────────────────────────────────────────────────────────────────────
        static void DrawPaddle(GLRenderer2D& r,
                               float x, float y, float w, float h,
                               math::NkColor c)
        {
            // 6 quads remplis de plus en plus larges, alpha decroissant.
            // Couches "soft" empilees : effet de gradient autour du paddle.
            for (int i = 6; i >= 1; --i)
            {
                const float pad = (float)i * 2.5f;
                math::NkColor glow = c;
                // alpha decroit avec la distance (30 -> 5)
                glow.a = static_cast<uint8_t>(34 - (i - 1) * 5);
                r.DrawQuad(x - pad, y - pad,
                           w + pad * 2.0f, h + pad * 2.0f, glow);
            }
            // Outline lumineux (rebord net)
            math::NkColor edge = c; edge.a = 220;
            r.DrawQuadOutline(x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f, edge, 1.0f);
            // Paddle plein
            r.DrawQuad(x, y, w, h, c);
            // Highlight central blanc tres leger (effet "verre" du neon HTML)
            r.DrawQuad(x + 1.0f, y + 1.0f, w - 2.0f, h * 0.35f,
                       { 255, 255, 255, 50 });
        }

        void GameplayScene::OnRender(AppContext& ctx)
        {
            GLRenderer2D& r = *ctx.renderer;
            FontAtlas&    f = *ctx.font;
            const int W = ctx.viewportW;
            const int H = ctx.viewportH;

            r.Clear(theme::Dark().r / 255.0f,
                    theme::Dark().g / 255.0f,
                    theme::Dark().b / 255.0f, 1.0f);
            r.Begin(W, H);

            // Arene : entre HUD top et HUD bot (scales)
            const float ax = 0.0f;
            const float ay = mHUDTopH;
            const float aw = (float)W;
            const float ah = (float)H - mHUDTopH - mHUDBotH;
            if (ah > 0.0f)
            {
                DrawArenaField(r, ax, ay, aw, ah, mScale);

                // Paddles
                DrawPaddle(r, ax + mPaddleLX, ay + mPaddleLY,
                           mPaddleW, mPaddleH, ColP1());
                DrawPaddle(r, ax + mPaddleRX, ay + mPaddleRY,
                           mPaddleW, mPaddleH, ColP2());

                // Trail + balle
                DrawBallAndTrail(r, ax, ay, mBallX, mBallY, mBallR,
                                 mTrailX, mTrailY, mTrailHead, mTrailCount, kTrailMax);

                // Goal flash sur l'arene
                if (mGoalFlashAlpha > 0.0f)
                {
                    math::NkColor c = (mGoalFlashSide > 0) ? ColP1() : ColP2();
                    c.a = static_cast<uint8_t>(50 * mGoalFlashAlpha);
                    r.DrawQuad(ax, ay, aw, ah, c);
                }
            }

            // HUDs
            DrawHUDTop(r, f, W, mHUDTopH, mScale, mScoreL, mScoreR, mTimeLeft);
            DrawHUDBottom(r, f, W, H, mHUDBotH, mScale, mPaused);

            // ── Bouton PAUSE flottant (HORS arene, hors zone timer) ─────────
            // Place dans la barre HUD top, juste sous le bord superieur, a
            // droite du timer central. Ne chevauche ni le timer (centre) ni
            // les avatars/scores P1/P2 (extremites).
            {
                const float btnSize = mHUDTopH * 0.65f;
                mPauseBtnW = btnSize;
                mPauseBtnH = btnSize;
                // Position : ~70% de la largeur (entre le centre et l'avatar P2)
                mPauseBtnX = (float)W * 0.70f - btnSize * 0.5f;
                mPauseBtnY = (mHUDTopH - btnSize) * 0.5f;

                // Fond + border cyan
                math::NkColor bg = ColP1(); bg.a = 30;
                math::NkColor bd = ColP1(); bd.a = 180;
                r.DrawQuad       (mPauseBtnX, mPauseBtnY, mPauseBtnW, mPauseBtnH, bg);
                r.DrawQuadOutline(mPauseBtnX, mPauseBtnY, mPauseBtnW, mPauseBtnH, bd, 1.5f * mScale);

                // Icone : 2 barres verticales si non-paused, triangle play si paused
                const float ix = mPauseBtnX + mPauseBtnW * 0.5f;
                const float iy = mPauseBtnY + mPauseBtnH * 0.5f;
                const float ih = mPauseBtnH * 0.4f;
                if (mPaused)
                {
                    // Triangle play (3 lignes vers le sommet droit)
                    const float tw = mPauseBtnW * 0.3f;
                    r.DrawTriangle(ix - tw * 0.5f, iy - ih,
                                   ix - tw * 0.5f, iy + ih,
                                   ix + tw * 0.5f, iy,
                                   ColP1());
                }
                else
                {
                    // 2 barres pause
                    const float bw = mPauseBtnW * 0.10f;
                    const float gap = mPauseBtnW * 0.08f;
                    r.DrawQuad(ix - gap - bw, iy - ih, bw, ih * 2.0f, ColP1());
                    r.DrawQuad(ix + gap,      iy - ih, bw, ih * 2.0f, ColP1());
                }
            }

            // ── Overlay PAUSE avec boutons cliquables ────────────────────────
            if (mPaused)
            {
                r.DrawQuad(0, 0, (float)W, (float)H, { 5, 10, 20, 220 });
                const float cx = (float)W * 0.5f;
                const float cy = (float)H * 0.5f;

                // Titre PAUSE
                f.DrawStringShadowCenteredScaled(r, FontAtlas::HeadlineSlot, mScale,
                                         cx, cy - 90.0f * mScale, "PAUSE",
                                         { 255, 255, 255, 255 }, ColP1(), 3);
                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, mScale,
                                   cx, cy - 30.0f * mScale,
                                   "LE MATCH EST EN ATTENTE",
                                   { 255, 255, 255, 120 });

                // Boutons REPRENDRE et RETOUR MENU centres
                const float btnW = 280.0f * mScale;
                const float btnH = 56.0f  * mScale;
                const float btnGap = 16.0f * mScale;

                // REPRENDRE (cyan)
                mResumeBtnW = btnW; mResumeBtnH = btnH;
                mResumeBtnX = cx - btnW * 0.5f;
                mResumeBtnY = cy + 16.0f * mScale;
                math::NkColor rbg = ColP1(); rbg.a = 32;
                math::NkColor rbd = ColP1(); rbd.a = 200;
                r.DrawQuad       (mResumeBtnX, mResumeBtnY, mResumeBtnW, mResumeBtnH, rbg);
                r.DrawQuadOutline(mResumeBtnX, mResumeBtnY, mResumeBtnW, mResumeBtnH, rbd, 2.0f * mScale);
                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, mScale,
                                   cx, mResumeBtnY + btnH * 0.30f,
                                   "REPRENDRE  (ESPACE / TAP)", ColP1());

                // RETOUR MENU (rouge)
                mMenuBtnW = btnW; mMenuBtnH = btnH;
                mMenuBtnX = cx - btnW * 0.5f;
                mMenuBtnY = mResumeBtnY + btnH + btnGap;
                math::NkColor mbg = { 255, 64, 64, 32 };
                math::NkColor mbd = { 255, 64, 64, 200 };
                r.DrawQuad       (mMenuBtnX, mMenuBtnY, mMenuBtnW, mMenuBtnH, mbg);
                r.DrawQuadOutline(mMenuBtnX, mMenuBtnY, mMenuBtnW, mMenuBtnH, mbd, 2.0f * mScale);
                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, mScale,
                                   cx, mMenuBtnY + btnH * 0.30f,
                                   "RETOUR MENU  (ECHAP)",
                                   { 255, 64, 64, 230 });
            }

            r.End();
        }

        // ─────────────────────────────────────────────────────────────────────
        // OnEvent — clavier + touch (drag vertical) + souris
        // ─────────────────────────────────────────────────────────────────────
        // Touch/mouse model :
        //   - L'arene est divisee en 2 moities (X < W/2 = P1 gauche, sinon P2)
        //   - Sur TouchBegin / MouseDown : on associe le touch ID a la moitie
        //   - Sur TouchMove / MouseMove tant que le bouton est tenu : on
        //     applique deltaY a mPaddleLY/RY
        //   - Sur TouchEnd / MouseUp : on libere
        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::OnEvent(AppContext& ctx, NkEvent& ev)
        {
            // ── Clavier ─────────────────────────────────────────────────────
            if (auto* kp = ev.As<NkKeyPressEvent>())
            {
                const NkKey k = kp->GetKey();
                switch (k)
                {
                case NkKey::NK_W:     mKeyW = true;    break;
                case NkKey::NK_S:     mKeyS = true;    break;
                case NkKey::NK_UP:    mKeyUp = true;   break;
                case NkKey::NK_DOWN:  mKeyDown = true; break;
                case NkKey::NK_SPACE: mPaused = !mPaused; break;
                case NkKey::NK_ESCAPE:
                    if (mPaused) ctx.scenes->Pop();
                    else         mPaused = true;
                    break;
                default: break;
                }
                return;
            }
            if (auto* kr = ev.As<NkKeyReleaseEvent>())
            {
                const NkKey k = kr->GetKey();
                switch (k)
                {
                case NkKey::NK_W:    mKeyW = false;    break;
                case NkKey::NK_S:    mKeyS = false;    break;
                case NkKey::NK_UP:   mKeyUp = false;   break;
                case NkKey::NK_DOWN: mKeyDown = false; break;
                default: break;
                }
                return;
            }

            const float halfW = (float)ctx.viewportW * 0.5f;

            // Helper local : teste si le point (x,y) tombe sur un des boutons
            // UI prioritaires (PAUSE / REPRENDRE / RETOUR MENU). Retourne
            // true si l'event a ete consomme par un bouton (= ne pas le
            // propager au drag paddle).
            auto handleUIButtons = [&](float x, float y) -> bool
            {
                // Bouton PAUSE flottant (toujours visible)
                if (PointInRect(x, y, mPauseBtnX, mPauseBtnY, mPauseBtnW, mPauseBtnH))
                {
                    mPaused = !mPaused;
                    return true;
                }
                // Boutons overlay pause (seulement quand pause active)
                if (mPaused)
                {
                    if (PointInRect(x, y, mResumeBtnX, mResumeBtnY, mResumeBtnW, mResumeBtnH))
                    {
                        mPaused = false;
                        return true;
                    }
                    if (PointInRect(x, y, mMenuBtnX, mMenuBtnY, mMenuBtnW, mMenuBtnH))
                    {
                        ctx.scenes->Pop();
                        return true;
                    }
                    // Pendant la pause, tout autre tap doit etre absorbe pour
                    // ne pas bouger les paddles via le drag.
                    return true;
                }
                return false;
            };

            // ── Souris (desktop) : drag bouton gauche tenu ──────────────────
            if (auto* mp = ev.As<NkMouseButtonPressEvent>())
            {
                if (mp->GetButton() == NkMouseButton::NK_MB_LEFT)
                {
                    const float mx = (float)mp->GetX();
                    const float my = (float)mp->GetY();
                    if (handleUIButtons(mx, my)) return;
                    if (mx < halfW) { mMouseDownL = true;  mLastMouseY = my; }
                    else            { mMouseDownR = true;  mLastMouseY = my; }
                }
                return;
            }
            if (auto* mr = ev.As<NkMouseButtonReleaseEvent>())
            {
                if (mr->GetButton() == NkMouseButton::NK_MB_LEFT)
                {
                    mMouseDownL = false;
                    mMouseDownR = false;
                }
                return;
            }
            if (auto* mm = ev.As<NkMouseMoveEvent>())
            {
                const float my = (float)mm->GetY();
                const float dy = my - mLastMouseY;
                if (mMouseDownL) mPaddleLY += dy;
                if (mMouseDownR) mPaddleRY += dy;
                mLastMouseY = my;
                return;
            }

            // ── Touch (mobile) : drag par moitie ────────────────────────────
            if (auto* tb = ev.As<NkTouchBeginEvent>())
            {
                for (uint32 i = 0; i < tb->GetNumTouches(); ++i)
                {
                    const NkTouchPoint& tp = tb->GetTouch(i);
                    // Test bouton PAUSE / overlay AVANT le drag paddle
                    if (handleUIButtons(tp.clientX, tp.clientY)) continue;
                    if (tp.clientX < halfW)
                    {
                        mTouchIdL    = (long long)tp.id;
                        mLastTouchYL = tp.clientY;
                    }
                    else
                    {
                        mTouchIdR    = (long long)tp.id;
                        mLastTouchYR = tp.clientY;
                    }
                }
                return;
            }
            if (auto* tm = ev.As<NkTouchMoveEvent>())
            {
                for (uint32 i = 0; i < tm->GetNumTouches(); ++i)
                {
                    const NkTouchPoint& tp = tm->GetTouch(i);
                    const long long id = (long long)tp.id;
                    if (id == mTouchIdL)
                    {
                        const float dy = tp.clientY - mLastTouchYL;
                        mPaddleLY += dy;
                        mLastTouchYL = tp.clientY;
                    }
                    else if (id == mTouchIdR)
                    {
                        const float dy = tp.clientY - mLastTouchYR;
                        mPaddleRY += dy;
                        mLastTouchYR = tp.clientY;
                    }
                }
                return;
            }
            if (auto* te = ev.As<NkTouchEndEvent>())
            {
                for (uint32 i = 0; i < te->GetNumTouches(); ++i)
                {
                    const NkTouchPoint& tp = te->GetTouch(i);
                    const long long id = (long long)tp.id;
                    if (id == mTouchIdL) mTouchIdL = -1;
                    if (id == mTouchIdR) mTouchIdR = -1;
                }
                return;
            }
            if (auto* tc = ev.As<NkTouchCancelEvent>())
            {
                (void)tc;
                mTouchIdL = -1;
                mTouchIdR = -1;
                return;
            }
        }

    } // namespace pong
} // namespace nkentseu
