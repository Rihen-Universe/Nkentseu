// =============================================================================
// SelectMatchConfigScene.cpp
// -----------------------------------------------------------------------------
// Fusion difficulte + obstacles avec scroll vertical.
//
// Architecture :
//   - Le rendu se fait en deux passes :
//     1. Zone fixe (titre haut, bouton LANCER bas) — coords ECRAN
//     2. Zone scrollable (cards Difficulte + Obstacles) — coords MONDE
//        translatees de -mScrollY avant rendu.
//   - Pour le hit-test, on convertit le point ECRAN en MONDE en ajoutant
//     mScrollY a la coordonnee Y. Les boutons fixes (LANCER) restent en
//     coords ECRAN.
//   - Le scroll est piloté par drag tactile (avec threshold pour distinguer
//     tap vs scroll), molette souris, fleches PageUp/PageDown.
// =============================================================================

#include "SelectMatchConfigScene.h"
#include "GameplayScene.h"
#include "NKCanvas/Renderer/Core/NkRenderer2D.h"
#include "Pong/Render/FontAtlas.h"
#include "Pong/UI/Theme.h"
#include "Pong/UI/SceneManager.h"
#include "Pong/UI/UIScale.h"
#include "Pong/UI/ResponsiveLayout.h"
#include "Pong/Game/GameTypes.h"
#include "Pong/Net/NetworkSession.h"
#include "Pong/Net/NetProtocol.h"
#include "NKLogger/NkLog.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include <cstring>
#include <ctime>
#include "NKMath/NkFunctions.h"
#include <cstdio>

namespace nkentseu
{
    namespace pong
    {

        // ── Description difficulte ──────────────────────────────────────────
        struct DiffDescL
        {
            AIDifficulty  level;
            const char*   rank;
            const char*   name;
            const char*   desc;
            int           dots;
            math::NkColor color;
        };
        static const DiffDescL kDiffsL[SelectMatchConfigScene::kDiffCount] =
        {
            { AIDifficulty::Beginner,   "01", "DEBUTANT",   "LENT, ERREURS FREQUENTES.",       1, { 100, 200, 220, 255 } },
            { AIDifficulty::Apprentice, "02", "APPRENTI",   "TRAJECTOIRES SIMPLES.",           2, {   0, 200, 245, 255 } },
            { AIDifficulty::Competitor, "03", "COMPETITEUR","BONNE ANTICIPATION.",             3, { 255, 107,   0, 255 } },
            { AIDifficulty::Expert,     "04", "EXPERT",     "ANGLES COMPLEXES.",               4, { 255,  64,  64, 255 } },
            { AIDifficulty::Legend,     "05", "LEGENDE",    "QUASI-PARFAITE.",                 5, { 255, 215,   0, 255 } },
            { AIDifficulty::Chaos,      "06", "CHAOS",      "ALEATOIRE.",                      5, { 204, 119, 255, 255 } },
        };

        // ── Description obstacles ───────────────────────────────────────────
        struct ObsDescL
        {
            const char*   name;
            const char*   desc;
            int           iconId;
            math::NkColor color;
        };
        static const ObsDescL kObsL[SelectMatchConfigScene::kObsCount] =
        {
            { "MUR SOLIDE",          "REBOND 90 DEG.",        0, { 100, 150, 200, 255 } },
            { "PORTAIL",             "TELEPORT x1.5.",        1, { 255, 215,   0, 255 } },
            { "GRAVITE",             "ATTIRE LA BALLE.",      2, { 204, 119, 255, 255 } },
            { "COURANT D'AIR",       "POUSSE FIXE.",          3, {   0, 255, 100, 255 } },
            { "MIROIR FANTOME",      "TRAVERSE + CLONE.",     4, { 204, 119, 255, 255 } },
            { "MINE",                "EXPLOSION + BLIND.",    5, { 255,  64,  64, 255 } },
            { "AIMANT",              "PROPULSE x2.",          6, {   0, 245, 255, 255 } },
            { "ETOILE BONUS",        "BONUS ALEATOIRE.",      7, { 255, 215,   0, 255 } },
        };

        static void DrawObsIcon(renderer::NkRenderer2D& r, int iconId,
                                float bx, float by, float bs,
                                math::NkColor c)
        {
            const float cx = bx + bs * 0.5f;
            const float cy = by + bs * 0.5f;
            const float hb = bs * 0.5f;
            switch (iconId)
            {
            case 0:
            {
                const float bw = bs * 0.18f;
                const float bh = bs * 0.14f;
                for (int row = 0; row < 2; ++row)
                for (int col = 0; col < 3; ++col)
                {
                    const float xx = cx - bw * 1.5f + col * bw
                                   + ((row & 1) ? bw * 0.5f : 0.0f);
                    const float yy = cy - bh + row * bh;
                    r.DrawRectOutline({ xx, yy, bw - 1.0f, bh - 1.0f }, c, 1.0f);
                }
                break;
            }
            case 1:
                r.DrawCircleOutline({ cx, cy }, hb * 0.6f, c, 2.0f, 32);
                r.DrawFilledCircle({ cx, cy }, hb * 0.2f, c, 16);
                break;
            case 2:
                for (int i = 0; i < 3; ++i)
                    r.DrawCircleOutline({ cx, cy }, hb * (0.25f + 0.18f * i), c, 1.5f, 32);
                r.DrawFilledCircle({ cx, cy }, 2.5f, c, 10);
                break;
            case 3:
                for (int i = 0; i < 3; ++i)
                {
                    const float yy = cy - 6.0f + i * 6.0f;
                    r.DrawFilledRect({ cx - hb * 0.5f, yy, hb, 1.5f }, c);
                }
                break;
            case 4:
                r.DrawFilledTriangle({ cx, cy - hb * 0.5f }, { cx + hb * 0.45f, cy },
                               { cx, cy + hb * 0.5f }, c);
                r.DrawFilledTriangle({ cx, cy - hb * 0.5f }, { cx - hb * 0.45f, cy },
                               { cx, cy + hb * 0.5f }, c);
                break;
            case 5:
                r.DrawFilledCircle({ cx, cy }, hb * 0.35f, c, 16);
                for (int i = 0; i < 8; ++i)
                {
                    const float a = 6.28318f * i / 8.0f;
                    const float r1 = hb * 0.40f;
                    const float r2 = hb * 0.55f;
                    r.DrawLine({ cx + math::NkCos(a) * r1, cy + math::NkSin(a) * r1 },
                               { cx + math::NkCos(a) * r2, cy + math::NkSin(a) * r2 },
                               c, 1.5f);
                }
                break;
            case 6:
                r.DrawFilledRect({ cx - hb * 0.4f, cy - hb * 0.3f, hb * 0.2f, hb * 0.6f }, c);
                r.DrawFilledRect({ cx + hb * 0.2f, cy - hb * 0.3f, hb * 0.2f, hb * 0.6f }, c);
                r.DrawFilledRect({ cx - hb * 0.4f, cy + hb * 0.20f, hb * 0.8f, hb * 0.1f }, c);
                break;
            case 7:
            default:
            {
                const float rad = hb * 0.55f;
                for (int i = 0; i < 5; ++i)
                {
                    const float a1 = -1.5708f + 6.28318f * i / 5.0f;
                    const float a2 = -1.5708f + 6.28318f * (i + 2) / 5.0f;
                    r.DrawLine({ cx + math::NkCos(a1) * rad, cy + math::NkSin(a1) * rad },
                               { cx + math::NkCos(a2) * rad, cy + math::NkSin(a2) * rad },
                               c, 1.5f);
                }
                break;
            }
            }
        }

        // ── Tables d'options pour les steppers ──────────────────────────────
        // maxScore : 0 (illimite) puis valeurs classiques tennis/pong
        static const int   kScoreOptions[] = { 0, 3, 5, 7, 11, 15, 21 };
        static const int   kScoreOptionsN  = (int)(sizeof(kScoreOptions)
                                                 / sizeof(kScoreOptions[0]));
        // timeLimit (en secondes) : 0 = chrono ascendant sans limite,
        // sinon presets classiques (1min, 1m30, 2min, ..., 5min).
        static const float kTimeOptions[]  = { 0.0f, 60.0f, 90.0f, 120.0f,
                                               154.0f, 180.0f, 240.0f, 300.0f };
        static const int   kTimeOptionsN   = (int)(sizeof(kTimeOptions)
                                                 / sizeof(kTimeOptions[0]));
        // ballSpeedMul : multiplicateur >= 1 applique a la vitesse initiale
        // de la balle. 1.0 = vitesse de base, 10.0 = mode super fast.
        // Pas de 0.1 entre 1.0 et 10.0 (91 valeurs) pour controle fin.
        // Toutes les valeurs sont >= 1 par contrat. Le moteur scale aussi
        // le cap interne ET le plancher minSpeed par ce mul (cf. GameplayScene)
        // pour que la valeur choisie reste effective apres les rebonds.
        static constexpr int   kSpeedOptionsN  = 91;
        static float kSpeedOptions[kSpeedOptionsN];   // initialise au 1er appel
        static bool  kSpeedOptionsInit = false;
        static void InitSpeedOptions()
        {
            if (kSpeedOptionsInit) return;
            for (int i = 0; i < kSpeedOptionsN; ++i)
                kSpeedOptions[i] = 1.0f + 0.1f * (float)i;   // 1.0, 1.1, ..., 10.0
            kSpeedOptionsInit = true;
        }

        static int FindScoreIndex(int score)
        {
            for (int i = 0; i < kScoreOptionsN; ++i)
                if (kScoreOptions[i] == score) return i;
            return 4;  // default 11
        }
        static int FindTimeIndex(float t)
        {
            float best = 1e9f; int bi = 0;
            for (int i = 0; i < kTimeOptionsN; ++i)
            {
                const float d = (kTimeOptions[i] > t)
                              ? (kTimeOptions[i] - t)
                              : (t - kTimeOptions[i]);
                if (d < best) { best = d; bi = i; }
            }
            return bi;
        }
        // Forward decl : ApplyParamButton est defini plus bas dans ce TU
        // mais utilise depuis OnUpdate (hold-to-repeat). Sans cette decl,
        // le compilateur ne trouve pas le symbole au site d'appel.
        static void ApplyParamButton(int btn,
                                     int& scoreIndex, int& timeIndex,
                                     bool& winByTwo,
                                     int& speedIndex);

        static void FormatScore(int score, char* out, int outSize)
        {
            if (score <= 0) std::snprintf(out, outSize, "ILLIMITE");
            else            std::snprintf(out, outSize, "%d PTS", score);
        }
        static void FormatTime(float t, char* out, int outSize)
        {
            if (t <= 0.001f) { std::snprintf(out, outSize, "AUCUN"); return; }
            const int m = (int)t / 60;
            const int s = (int)t % 60;
            std::snprintf(out, outSize, "%02d:%02d", m, s);
        }
        static int FindSpeedIndex(float mul)
        {
            InitSpeedOptions();
            float best = 1e9f; int bi = 0;
            for (int i = 0; i < kSpeedOptionsN; ++i)
            {
                const float d = (kSpeedOptions[i] > mul)
                              ? (kSpeedOptions[i] - mul)
                              : (mul - kSpeedOptions[i]);
                if (d < best) { best = d; bi = i; }
            }
            return bi;
        }
        static void FormatSpeed(float mul, char* out, int outSize)
        {
            // Affichage compact : "x1.0", "x1.5", "x2.0"
            std::snprintf(out, outSize, "x%.1f", mul);
        }

        static float EaseOutCubic(float t)
        {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            const float u = 1.0f - t;
            return 1.0f - u * u * u;
        }

        // ─────────────────────────────────────────────────────────────────────
        void SelectMatchConfigScene::OnEnter(AppContext& ctx)
        {
            mTime = 0.0f;
            mEnterAnim = 0.0f;
            mScrollY = 0.0f;
            mMaxScroll = 0.0f;
            mDragActive = false;
            mDragWasScroll = false;
            mDragTouchId = -1;
            mMouseDown = false;
            // Reset hold-to-repeat state (au cas ou la scene est reentered).
            mArmedStepper  = 0;
            mHoldTime      = 0.0f;
            mRepeatAccum   = 0.0f;
            mPressConsumed = false;

            // Section difficulte visible uniquement si mode IA
            mShowDifficulty = false;
            mDiffIndex = 2;
            if (ctx.settings != nullptr)
            {
                const GameMode m = ctx.settings->mode;
                mShowDifficulty  = (m == GameMode::VsAI
                                 || m == GameMode::AIvsAI);
                mShowDifficultyR = (m == GameMode::AIvsAI);
                for (int i = 0; i < kDiffCount; ++i)
                {
                    if (kDiffsL[i].level == ctx.settings->difficulty)
                    {
                        mDiffIndex = i;
                        break;
                    }
                }
                for (int i = 0; i < kDiffCount; ++i)
                {
                    if (kDiffsL[i].level == ctx.settings->difficultyP2)
                    {
                        mDiffIndexR = i;
                        break;
                    }
                }
                for (int i = 0; i < kObsCount; ++i)
                {
                    mObsActive[i] = ctx.settings->obsActive[i];
                }
                // Charge les parametres de match depuis settings
                mScoreIndex = FindScoreIndex(ctx.settings->maxScore);
                mTimeIndex  = FindTimeIndex(ctx.settings->timeLimit);
                mWinByTwo   = ctx.settings->winByTwo;
                mSpeedIndex = FindSpeedIndex(ctx.settings->ballSpeedMul);
            }
            logger.Info("[MatchConfig] OnEnter showDiff={0}",
                        mShowDifficulty ? 1 : 0);
        }

        void SelectMatchConfigScene::OnUpdate(AppContext& /*ctx*/, float dt)
        {
            mTime += dt;
            mEnterAnim += dt / 0.4f;
            if (mEnterAnim > 1.0f) mEnterAnim = 1.0f;

            // Hold-to-repeat sur les steppers : apres kHoldDelay, on auto-fire
            // toutes les kRepeatInterval secondes. Permet d'aller vite sur
            // ballSpeedMul (91 valeurs entre x1 et x10).
            if (mArmedStepper > 0)
            {
                mHoldTime += dt;
                if (mHoldTime > kHoldDelay)
                {
                    mRepeatAccum += dt;
                    while (mRepeatAccum >= kRepeatInterval)
                    {
                        mRepeatAccum -= kRepeatInterval;
                        ApplyParamButton(mArmedStepper, mScoreIndex,
                                         mTimeIndex, mWinByTwo, mSpeedIndex);
                    }
                }
            }
        }

        float SelectMatchConfigScene::ClampScroll(float v) const
        {
            if (v < 0.0f) return 0.0f;
            if (v > mMaxScroll) return mMaxScroll;
            return v;
        }

        // ─────────────────────────────────────────────────────────────────────
        void SelectMatchConfigScene::OnRender(AppContext& ctx)
        {
            renderer::NkRenderer2D& r = *ctx.renderer;
            FontAtlas&    f = *ctx.font;
            const int W = ctx.viewportW;
            const int H = ctx.viewportH;
            const float scale = GetUIScale(W, H);

            // Background grille
            const math::NkColor grid = { 0, 245, 255, 10 };
            const float step = 40.0f;
            for (float x = 0; x < (float)W; x += step) r.DrawFilledRect({ x, 0.0f, 1.0f, (float)H }, grid);
            for (float y = 0; y < (float)H; y += step) r.DrawFilledRect({ 0.0f, y, (float)W, 1.0f }, grid);

            const float safeX = (float)ctx.safe.LeftX();
            const float safeY = (float)ctx.safe.TopY();
            const float safeW = (float)ctx.safe.SafeW();
            const float safeH = (float)ctx.safe.SafeH();
            const float cx    = safeX + safeW * 0.5f;
            const float enterA = EaseOutCubic(mEnterAnim);

            // ── Zones reservees ──────────────────────────────────────────────
            // Layout responsive (2026-05-19) : top/bottom exprimes en % de H
            // (viewport hauteur) avec clamps doux. Top = bandeau titre +
            // sous-titre + bouton retour. Bottom = bouton LANCER sticky.
            mTopReserve    = Pct::H(H, 0.115f, 70.0f, 130.0f);
            mBottomReserve = Pct::H(H, 0.100f, 64.0f, 110.0f);

            const float scrollTop = safeY + mTopReserve;
            const float scrollBot = safeY + safeH - mBottomReserve;
            const float scrollH   = scrollBot - scrollTop;

            // ── Layout du contenu (en coords MONDE, Y depuis scrollTop=0) ────
            // Section Difficulte (si visible) : grille 3 colonnes x 2 lignes
            // Section Obstacles : grille 4 colonnes x 2 lignes
            const float pad = Pct::W(W, 0.014f, 10.0f, 22.0f);
            const float availW = safeW - pad * 2.0f;
            const float gridLeft = safeX + pad;

            float worldY = Pct::H(H, 0.015f, 8.0f, 22.0f);
            const math::NkColor sectionCol = { 255, 255, 255, 110 };

            // ── Section PARAMETRES DU MATCH (toujours visible, en 1er) ──────
            {
                f.DrawStringScaled(r, FontAtlas::SubtitleSlot, scale,
                             gridLeft, worldY - mScrollY + scrollTop,
                             "PARAMETRES DU MATCH", sectionCol);
                worldY += Pct::H(H, 0.052f, 28.0f, 56.0f);

                // Layout : 4 lignes (Score, Temps, Vitesse, WinByTwo) avec
                // label a gauche et stepper / toggle a droite. Dimensions en
                // % du viewport pour rester lisibles sur PC et mobile.
                const float lineH   = Pct::H(H, 0.080f, 44.0f, 80.0f);
                const float btnW    = Pct::W(W, 0.045f, 30.0f, 56.0f);
                const float btnH    = Pct::H(H, 0.055f, 30.0f, 56.0f);
                const float valW    = Pct::W(W, 0.170f, 100.0f, 200.0f);
                const float toggleW = Pct::W(W, 0.100f, 64.0f, 120.0f);
                const float toggleH = Pct::H(H, 0.045f, 24.0f, 48.0f);
                mStepperW = btnW; mStepperH = btnH;
                mToggleW = toggleW; mToggleH = toggleH;

                // Ligne 1 : POINTS POUR GAGNER
                {
                    const float ly = worldY;
                    const float screenLY = ly - mScrollY + scrollTop;
                    if (screenLY + lineH >= scrollTop - 4.0f
                     && screenLY <= scrollBot + 4.0f)
                    {
                        f.DrawStringScaled(r, FontAtlas::BodySlot, scale,
                                     gridLeft, screenLY + lineH * 0.30f,
                                     "POINTS POUR GAGNER",
                                     { 255, 255, 255, 220 });
                        // Stepper a droite
                        const float rxStart = gridLeft + availW - (btnW * 2 + valW);
                        // Boutons -
                        mScoreMinusX = rxStart;
                        mScoreMinusY = ly + (lineH - btnH) * 0.5f;
                        r.DrawFilledRect ({ mScoreMinusX, screenLY + (lineH - btnH) * 0.5f,
                                          btnW, btnH }, { 0, 245, 255, 40 });
                        r.DrawRectOutline({ mScoreMinusX, screenLY + (lineH - btnH) * 0.5f,
                                          btnW, btnH }, { 0, 245, 255, 200 }, 1.5f);
                        f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                           mScoreMinusX + btnW * 0.5f,
                                           screenLY + (lineH - btnH) * 0.5f
                                                    + btnH * 0.18f,
                                           "-", theme::Cyan());
                        // Valeur centrale
                        char buf[24];
                        FormatScore(kScoreOptions[mScoreIndex], buf, sizeof(buf));
                        f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                           rxStart + btnW + valW * 0.5f,
                                           screenLY + lineH * 0.30f,
                                           buf, theme::White());
                        // Bouton +
                        mScorePlusX = rxStart + btnW + valW;
                        mScorePlusY = mScoreMinusY;
                        r.DrawFilledRect ({ mScorePlusX, screenLY + (lineH - btnH) * 0.5f,
                                          btnW, btnH }, { 0, 245, 255, 40 });
                        r.DrawRectOutline({ mScorePlusX, screenLY + (lineH - btnH) * 0.5f,
                                          btnW, btnH }, { 0, 245, 255, 200 }, 1.5f);
                        f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                           mScorePlusX + btnW * 0.5f,
                                           screenLY + (lineH - btnH) * 0.5f
                                                    + btnH * 0.18f,
                                           "+", theme::Cyan());
                    }
                    worldY += lineH;
                }

                // Ligne 2 : TEMPS LIMITE
                {
                    const float ly = worldY;
                    const float screenLY = ly - mScrollY + scrollTop;
                    if (screenLY + lineH >= scrollTop - 4.0f
                     && screenLY <= scrollBot + 4.0f)
                    {
                        f.DrawStringScaled(r, FontAtlas::BodySlot, scale,
                                     gridLeft, screenLY + lineH * 0.30f,
                                     "TEMPS LIMITE",
                                     { 255, 255, 255, 220 });
                        const float rxStart = gridLeft + availW - (btnW * 2 + valW);
                        mTimeMinusX = rxStart;
                        mTimeMinusY = ly + (lineH - btnH) * 0.5f;
                        r.DrawFilledRect ({ mTimeMinusX, screenLY + (lineH - btnH) * 0.5f,
                                          btnW, btnH }, { 0, 245, 255, 40 });
                        r.DrawRectOutline({ mTimeMinusX, screenLY + (lineH - btnH) * 0.5f,
                                          btnW, btnH }, { 0, 245, 255, 200 }, 1.5f);
                        f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                           mTimeMinusX + btnW * 0.5f,
                                           screenLY + (lineH - btnH) * 0.5f
                                                    + btnH * 0.18f,
                                           "-", theme::Cyan());
                        char buf[24];
                        FormatTime(kTimeOptions[mTimeIndex], buf, sizeof(buf));
                        f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                           rxStart + btnW + valW * 0.5f,
                                           screenLY + lineH * 0.30f,
                                           buf, theme::White());
                        mTimePlusX = rxStart + btnW + valW;
                        mTimePlusY = mTimeMinusY;
                        r.DrawFilledRect ({ mTimePlusX, screenLY + (lineH - btnH) * 0.5f,
                                          btnW, btnH }, { 0, 245, 255, 40 });
                        r.DrawRectOutline({ mTimePlusX, screenLY + (lineH - btnH) * 0.5f,
                                          btnW, btnH }, { 0, 245, 255, 200 }, 1.5f);
                        f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                           mTimePlusX + btnW * 0.5f,
                                           screenLY + (lineH - btnH) * 0.5f
                                                    + btnH * 0.18f,
                                           "+", theme::Cyan());
                    }
                    worldY += lineH;
                }

                // Ligne 3 : VITESSE BALLE (stepper multiplicateur >= 1)
                {
                    const float ly = worldY;
                    const float screenLY = ly - mScrollY + scrollTop;
                    if (screenLY + lineH >= scrollTop - 4.0f
                     && screenLY <= scrollBot + 4.0f)
                    {
                        f.DrawStringScaled(r, FontAtlas::BodySlot, scale,
                                     gridLeft, screenLY + lineH * 0.30f,
                                     "VITESSE BALLE",
                                     { 255, 255, 255, 220 });
                        const float rxStart = gridLeft + availW - (btnW * 2 + valW);
                        mSpeedMinusX = rxStart;
                        mSpeedMinusY = ly + (lineH - btnH) * 0.5f;
                        r.DrawFilledRect ({ mSpeedMinusX, screenLY + (lineH - btnH) * 0.5f,
                                          btnW, btnH }, { 0, 245, 255, 40 });
                        r.DrawRectOutline({ mSpeedMinusX, screenLY + (lineH - btnH) * 0.5f,
                                          btnW, btnH }, { 0, 245, 255, 200 }, 1.5f);
                        f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                           mSpeedMinusX + btnW * 0.5f,
                                           screenLY + (lineH - btnH) * 0.5f
                                                    + btnH * 0.18f,
                                           "-", theme::Cyan());
                        char buf[24];
                        FormatSpeed(kSpeedOptions[mSpeedIndex], buf, sizeof(buf));
                        f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                           rxStart + btnW + valW * 0.5f,
                                           screenLY + lineH * 0.30f,
                                           buf, theme::White());
                        mSpeedPlusX = rxStart + btnW + valW;
                        mSpeedPlusY = mSpeedMinusY;
                        r.DrawFilledRect ({ mSpeedPlusX, screenLY + (lineH - btnH) * 0.5f,
                                          btnW, btnH }, { 0, 245, 255, 40 });
                        r.DrawRectOutline({ mSpeedPlusX, screenLY + (lineH - btnH) * 0.5f,
                                          btnW, btnH }, { 0, 245, 255, 200 }, 1.5f);
                        f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                           mSpeedPlusX + btnW * 0.5f,
                                           screenLY + (lineH - btnH) * 0.5f
                                                    + btnH * 0.18f,
                                           "+", theme::Cyan());
                    }
                    worldY += lineH;
                }

                // Ligne 4 : VICTOIRE AVEC 2 PTS D'ECART (toggle)
                {
                    const float ly = worldY;
                    const float screenLY = ly - mScrollY + scrollTop;
                    if (screenLY + lineH >= scrollTop - 4.0f
                     && screenLY <= scrollBot + 4.0f)
                    {
                        f.DrawStringScaled(r, FontAtlas::BodySlot, scale,
                                     gridLeft, screenLY + lineH * 0.30f,
                                     "VICTOIRE A 2 PTS D'ECART",
                                     { 255, 255, 255, 220 });
                        const float tx = gridLeft + availW - toggleW;
                        const float ty = ly + (lineH - toggleH) * 0.5f;
                        mTogglePtsX = tx; mTogglePtsY = ty;
                        const float screenTY = screenLY + (lineH - toggleH) * 0.5f;
                        math::NkColor bg = mWinByTwo
                            ? math::NkColor{ 0, 245, 255, 80 }
                            : math::NkColor{ 255, 255, 255, 30 };
                        math::NkColor bd = mWinByTwo
                            ? math::NkColor{ 0, 245, 255, 220 }
                            : math::NkColor{ 255, 255, 255, 100 };
                        r.DrawFilledRect ({ tx, screenTY, toggleW, toggleH }, bg);
                        r.DrawRectOutline({ tx, screenTY, toggleW, toggleH }, bd, 1.5f);
                        f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                           tx + toggleW * 0.5f,
                                           screenTY + toggleH * 0.20f,
                                           mWinByTwo ? "ON" : "OFF",
                                           mWinByTwo ? theme::Cyan()
                                                     : math::NkColor{ 255, 255, 255, 160 });
                    }
                    worldY += lineH;
                }
                worldY += Pct::H(H, 0.022f, 12.0f, 28.0f);  // gap apres section
            }

            // Section DIFFICULTE
            // En VsAI : 1 grille "DIFFICULTE IA".
            // En AIvsAI : 2 grilles "DIFFICULTE IA GAUCHE" / "DIFFICULTE IA DROITE"
            //   (la gauche utilise mDiffIndex / mDiffX[]/Y[], la droite utilise
            //   mDiffIndexR / mDiffXR[]/YR[]).
            // Le code de rendu est factorise dans un lambda renderGrid pour
            // eviter la duplication.
            auto renderGrid = [&](const char* title,
                                  int  currentIndex,
                                  float* outX, float* outY)
            {
                f.DrawStringScaled(r, FontAtlas::SubtitleSlot, scale,
                             gridLeft, worldY - mScrollY + scrollTop,
                             title,
                             sectionCol);
                worldY += Pct::H(H, 0.052f, 28.0f, 56.0f);

                const int cols = 3;
                const int rows = 2;
                const float cardW = (availW - pad * (cols - 1)) / cols;
                // Hauteur de carte difficulte : env. 22% de H, clampee pour
                // garantir un rendu propre sur mobile portrait comme sur PC.
                const float cardH = Pct::H(H, 0.220f, 130.0f, 200.0f);
                mDiffW = cardW;
                mDiffH = cardH;

                for (int i = 0; i < kDiffCount; ++i)
                {
                    const DiffDescL& d = kDiffsL[i];
                    const int col = i % cols;
                    const int row = i / cols;
                    const float bx = gridLeft + col * (cardW + pad);
                    const float by = worldY + row * (cardH + pad);
                    outX[i] = bx;
                    outY[i] = by;

                    const float screenBY = by - mScrollY + scrollTop;
                    // Skip si compl. hors zone visible
                    if (screenBY + cardH < scrollTop - 4.0f) continue;
                    if (screenBY > scrollBot + 4.0f) continue;

                    float a = mEnterAnim - 0.04f * (float)i;
                    if (a < 0.0f) a = 0.0f;
                    a = EaseOutCubic(math::NkMin(a, 1.0f)) * enterA;

                    const bool focused = (i == currentIndex);
                    math::NkColor bg   = d.color; bg.a   = focused ? 50 : 14;
                    math::NkColor bord = d.color; bord.a = focused ? 220 : 70;
                    if (focused)
                    {
                        const float pulse = 0.5f + 0.5f * math::NkSin(mTime * 4.0f);
                        bg.a = static_cast<uint8>(bg.a + 30 * pulse);
                    }
                    bg.a   = static_cast<uint8>(bg.a   * a);
                    bord.a = static_cast<uint8>(bord.a * a);
                    r.DrawFilledRect ({ bx, screenBY, cardW, cardH }, bg);
                    r.DrawRectOutline({ bx, screenBY, cardW, cardH }, bord,
                                      focused ? 2.5f * scale : 1.0f);

                    // Offsets internes carte exprimes en fraction de cardW/cardH.
                    const float padX  = cardW * 0.08f;
                    const float rankY = cardH * 0.09f;
                    const float nameY = cardH * 0.42f;
                    const float dotsY0 = cardH * 0.60f;
                    const float descY = cardH * 0.70f;

                    math::NkColor topBar = d.color;
                    topBar.a = static_cast<uint8>(255 * a);
                    r.DrawFilledRect({ bx, screenBY, cardW, Pct::H(H, 0.004f, 2.0f, 5.0f) }, topBar);

                    math::NkColor rankCol = d.color;
                    rankCol.a = static_cast<uint8>(220 * a);
                    f.DrawStringShadowScaled(r, FontAtlas::HeadlineSlot, scale,
                                     bx + padX, screenBY + rankY,
                                     d.rank, rankCol, d.color, 1);

                    math::NkColor nameCol = d.color;
                    nameCol.a = static_cast<uint8>(255 * a);
                    f.DrawStringScaled(r, FontAtlas::BodySlot, scale,
                                 bx + padX, screenBY + nameY,
                                 d.name, nameCol);

                    // Dots : positionnees a 60% de la hauteur de carte,
                    // taille proportionnelle a cardW pour eviter qu'elles
                    // debordent ou disparaissent.
                    const float dotsY = screenBY + dotsY0;
                    const float dotR  = math::NkMax(3.0f, cardW * 0.025f);
                    const float dotG  = math::NkMax(8.0f, cardW * 0.065f);
                    for (int dn = 0; dn < 5; ++dn)
                    {
                        const float ddx = bx + padX + dn * dotG;
                        if (dn < d.dots)
                        {
                            math::NkColor dotC = d.color;
                            dotC.a = static_cast<uint8>(255 * a);
                            r.DrawFilledCircle({ ddx, dotsY }, dotR, dotC, 12);
                        }
                        else
                        {
                            math::NkColor dotC = { 255, 255, 255, 30 };
                            dotC.a = static_cast<uint8>(30 * a);
                            r.DrawCircleOutline({ ddx, dotsY }, dotR, dotC, 1.0f, 12);
                        }
                    }

                    math::NkColor descCol = { 255, 255, 255, 140 };
                    descCol.a = static_cast<uint8>(140 * a);
                    f.DrawStringScaled(r, FontAtlas::SmallSlot, scale,
                                 bx + padX, screenBY + descY,
                                 d.desc, descCol);
                }
                worldY += rows * (cardH + pad);
                worldY += Pct::H(H, 0.026f, 14.0f, 36.0f);  // gap inter-section
            };  // end renderGrid lambda

            if (mShowDifficulty)
            {
                renderGrid(
                    mShowDifficultyR ? "DIFFICULTE  IA  GAUCHE" : "DIFFICULTE  IA",
                    mDiffIndex, mDiffX, mDiffY);
            }
            if (mShowDifficultyR)
            {
                renderGrid("DIFFICULTE  IA  DROITE",
                           mDiffIndexR, mDiffXR, mDiffYR);
            }

            // Section OBSTACLES (toujours visible)
            f.DrawStringScaled(r, FontAtlas::SubtitleSlot, scale,
                         gridLeft, worldY - mScrollY + scrollTop,
                         "OBSTACLES  &  TERRAIN",
                         sectionCol);
            worldY += Pct::H(H, 0.052f, 28.0f, 56.0f);

            {
                const int cols = 4;
                const int rows = 2;
                const float cardW = (availW - pad * (cols - 1)) / cols;
                // Carte obstacle : un peu plus basse que carte difficulte
                // (moins de contenu textuel). 16% de H avec clamps doux.
                const float cardH = Pct::H(H, 0.160f, 95.0f, 150.0f);
                mObsW = cardW;
                mObsH = cardH;
                for (int i = 0; i < kObsCount; ++i)
                {
                    const ObsDescL& d = kObsL[i];
                    const int col = i % cols;
                    const int row = i / cols;
                    const float bx = gridLeft + col * (cardW + pad);
                    const float by = worldY + row * (cardH + pad);
                    mObsX[i] = bx;
                    mObsY[i] = by;

                    const float screenBY = by - mScrollY + scrollTop;
                    if (screenBY + cardH < scrollTop - 4.0f) continue;
                    if (screenBY > scrollBot + 4.0f) continue;

                    float a = mEnterAnim - 0.03f * (float)i;
                    if (a < 0.0f) a = 0.0f;
                    a = EaseOutCubic(math::NkMin(a, 1.0f)) * enterA;

                    const bool active = mObsActive[i];
                    math::NkColor bg, bord;
                    if (active)
                    {
                        bg = d.color; bg.a = 24;
                        bord = d.color; bord.a = 110;
                    }
                    else
                    {
                        bg = { 255, 255, 255,  6 };
                        bord = { 255, 255, 255, 30 };
                    }
                    bg.a   = static_cast<uint8>(bg.a   * a);
                    bord.a = static_cast<uint8>(bord.a * a);
                    r.DrawFilledRect ({ bx, screenBY, cardW, cardH }, bg);
                    r.DrawRectOutline({ bx, screenBY, cardW, cardH }, bord, 1.5f * scale);

                    const float iconSize = cardH * 0.35f;
                    const float iconX = bx + (cardW - iconSize) * 0.5f;
                    const float iconY = screenBY + cardH * 0.07f;
                    math::NkColor iconC = active ? d.color
                                                 : math::NkColor{ 255, 255, 255, 80 };
                    iconC.a = static_cast<uint8>(iconC.a * a);
                    DrawObsIcon(r, d.iconId, iconX, iconY, iconSize, iconC);

                    // Case a cocher cliquable en coin haut-droit (toggle on/off
                    // independant du panneau d'edition). Taille proportionnelle
                    // au plus petit cote de la carte.
                    const float cs = math::NkMin(cardW, cardH) * 0.20f;
                    const float csInset = cardW * 0.05f;
                    const float cx2 = bx + cardW - cs - csInset;
                    const float cy2 = screenBY + csInset;
                    // Stocke en coords MONDE pour le hit-test (le screenBY
                    // inclut deja l'offset scroll, on l'inverse).
                    mObsCheckX[i] = cx2;
                    mObsCheckY[i] = (screenBY - scrollTop + mScrollY) + csInset;
                    mObsCheckW = cs; mObsCheckH = cs;

                    math::NkColor cbg = active ? d.color : math::NkColor{ 255, 255, 255, 20 };
                    cbg.a = static_cast<uint8>(cbg.a * a);
                    math::NkColor cbd = active ? d.color : math::NkColor{ 255, 255, 255, 100 };
                    cbd.a = static_cast<uint8>(cbd.a * a);
                    r.DrawFilledRect ({ cx2, cy2, cs, cs }, cbg);
                    r.DrawRectOutline({ cx2, cy2, cs, cs }, cbd, 1.5f * scale);
                    if (active)
                    {
                        // Coche
                        math::NkColor white = { 0, 0, 0, 230 };
                        white.a = static_cast<uint8>(230 * a);
                        const float pad = cs * 0.22f;
                        r.DrawLine({ cx2 + pad,        cy2 + cs * 0.5f },
                                   { cx2 + cs * 0.45f, cy2 + cs - pad }, white, 2.0f);
                        r.DrawLine({ cx2 + cs * 0.45f, cy2 + cs - pad },
                                   { cx2 + cs - pad,   cy2 + pad },       white, 2.0f);
                    }

                    math::NkColor nameC = active ? math::NkColor{ 255, 255, 255, 245 }
                                                 : math::NkColor{ 255, 255, 255, 100 };
                    nameC.a = static_cast<uint8>(nameC.a * a);
                    f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                       bx + cardW * 0.5f,
                                       screenBY + cardH * 0.58f,
                                       d.name, nameC);
                    math::NkColor descC = { 255, 255, 255, 130 };
                    descC.a = static_cast<uint8>(descC.a * a);
                    f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                       bx + cardW * 0.5f,
                                       screenBY + cardH * 0.78f,
                                       d.desc, descC);
                }
                worldY += rows * (cardH + pad);
                worldY += Pct::H(H, 0.026f, 14.0f, 36.0f);  // marge bas
            }

            // ── Maj mMaxScroll (contentH - scrollH) ─────────────────────────
            const float contentH = worldY;
            mMaxScroll = math::NkMax(0.0f, contentH - scrollH);
            mScrollY = ClampScroll(mScrollY);

            // ── TOP : titre + sous-titre (au-dessus du scroll, mask le contenu)
            r.DrawFilledRect({ 0.0f, safeY, (float)W, mTopReserve }, { 5, 10, 20, 230 });

            // Bouton RETOUR (haut-gauche) — dimensions en % viewport
            {
                const float bw = Pct::W(W, 0.110f, 80.0f, 150.0f);
                const float bh = Pct::H(H, 0.055f, 30.0f, 56.0f);
                mBackW = bw; mBackH = bh;
                mBackX = safeX + Pct::W(W, 0.020f, 10.0f, 24.0f);
                mBackY = safeY + Pct::H(H, 0.018f, 8.0f, 22.0f);
                r.DrawFilledRect ({ mBackX, mBackY, bw, bh }, { 255, 255, 255, 16 });
                r.DrawRectOutline({ mBackX, mBackY, bw, bh }, { 0, 245, 255, 180 }, 1.5f * scale);
                f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                   mBackX + bw * 0.5f, mBackY + bh * 0.30f,
                                   "< RETOUR", theme::Cyan());
            }

            // Titre + sous-titre centres en haut. Y exprime en % de H pour
            // suivre la hauteur de mTopReserve.
            f.DrawStringShadowCenteredScaled(r, FontAtlas::HeadlineSlot, scale,
                                     cx, safeY + Pct::H(H, 0.028f, 14.0f, 32.0f),
                                     "CONFIGURATION DE LA PARTIE",
                                     theme::White(), theme::Cyan(), 3);
            f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                cx, safeY + Pct::H(H, 0.085f, 50.0f, 100.0f),
                                mShowDifficulty
                                  ? "CHOISIS UNE DIFFICULTE ET CONFIGURE LES OBSTACLES"
                                  : "CONFIGURE LES OBSTACLES",
                                { 255, 255, 255, 110 });

            // ── BOTTOM : bouton LANCER sticky + fond opaque ─────────────────
            const float botY = safeY + safeH - mBottomReserve;
            r.DrawFilledRect({ 0.0f, botY, (float)W, mBottomReserve }, { 5, 10, 20, 230 });
            // Bouton LANCER : largeur 40% viewport (max 420 px), hauteur 7% H.
            const float launchW = Pct::W(W, 0.400f, 220.0f, 460.0f);
            const float launchH = Pct::H(H, 0.075f, 40.0f, 80.0f);
            mLaunchW = launchW; mLaunchH = launchH;
            mLaunchX = cx - launchW * 0.5f;
            mLaunchY = botY + (mBottomReserve - launchH) * 0.5f;
            math::NkColor lbg = theme::Cyan(); lbg.a = 50;
            math::NkColor lbd = theme::Cyan(); lbd.a = 230;
            r.DrawFilledRect ({ mLaunchX, mLaunchY, launchW, launchH }, lbg);
            r.DrawRectOutline({ mLaunchX, mLaunchY, launchW, launchH }, lbd, 2.0f * scale);
            f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                               cx, mLaunchY + launchH * 0.30f,
                               "LANCER LA PARTIE  (ENTREE)", theme::Cyan());

            // ── PANNEAU MODAL D'EDITION D'UN OBSTACLE ───────────────────────
            if (mEditingObsIndex >= 0 && mEditingObsIndex < kObsCount)
            {
                const ObsDescL& d = kObsL[mEditingObsIndex];
                // Voile sombre par-dessus tout
                r.DrawFilledRect({ 0.0f, 0.0f, (float)W, (float)H }, { 5, 10, 20, 200 });
                // Panneau central : taille en % viewport, centre vertical
                // et horizontal pour rester ergonomique sur tout ratio.
                const float pW = math::NkMin(safeW * 0.85f,
                                             Pct::W(W, 0.55f, 320.0f, 600.0f));
                const float pH = Pct::H(H, 0.55f, 280.0f, 460.0f);
                mEditPanelW = pW; mEditPanelH = pH;
                mEditPanelX = cx - pW * 0.5f;
                mEditPanelY = safeY + (safeH - pH) * 0.5f;
                r.DrawFilledRect ({ mEditPanelX, mEditPanelY, pW, pH },
                                  { 8, 16, 28, 250 });
                r.DrawRectOutline({ mEditPanelX, mEditPanelY, pW, pH },
                                  d.color, 2.0f * scale);

                // Titre + close (offsets relatifs au panneau)
                const float panelPadX = pW * 0.05f;
                const float panelPadY = pH * 0.045f;
                f.DrawStringShadowScaled(r, FontAtlas::SubtitleSlot, scale,
                                 mEditPanelX + panelPadX,
                                 mEditPanelY + panelPadY,
                                 d.name, d.color, d.color, 1);
                // Bouton close : taille proportionnelle a la plus petite
                // dimension du panneau pour rester carre et cliquable.
                const float closeS = math::NkMin(pW, pH) * 0.09f;
                mEditCloseX = mEditPanelX + pW - closeS - panelPadX * 0.6f;
                mEditCloseY = mEditPanelY + panelPadY * 0.85f;
                mEditBtnH = closeS;
                r.DrawFilledRect ({ mEditCloseX, mEditCloseY, closeS, closeS },
                                  { 255, 64, 64, 60 });
                r.DrawRectOutline({ mEditCloseX, mEditCloseY, closeS, closeS },
                                  { 255, 64, 64, 200 }, 1.5f);
                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                   mEditCloseX + closeS * 0.5f,
                                   mEditCloseY + closeS * 0.18f,
                                   "x", { 255, 64, 64, 240 });

                // 3 lignes : NOMBRE, FORCE, IMPREVISIBLE (responsive)
                const float bw = Pct::W(W, 0.045f, 30.0f, 56.0f);
                const float bh = Pct::H(H, 0.055f, 30.0f, 56.0f);
                const float valW = Pct::W(W, 0.145f, 90.0f, 180.0f);
                mEditBtnW = bw; mEditBtnH = bh;
                const float lx = mEditPanelX + pW * 0.07f;
                const float rx = mEditPanelX + pW - pW * 0.07f - (bw * 2 + valW);
                float ly = mEditPanelY + pH * 0.22f;
                const float lineH = Pct::H(H, 0.080f, 44.0f, 80.0f);

                // NOMBRE
                f.DrawStringScaled(r, FontAtlas::BodySlot, scale,
                             lx, ly + lineH * 0.28f,
                             "NOMBRE D'ELEMENTS", { 255, 255, 255, 220 });
                mEditCountMinusX = rx; mEditCountMinusY = ly + (lineH - bh) * 0.5f;
                r.DrawFilledRect ({ mEditCountMinusX, mEditCountMinusY, bw, bh }, { 0, 245, 255, 40 });
                r.DrawRectOutline({ mEditCountMinusX, mEditCountMinusY, bw, bh }, { 0, 245, 255, 200 }, 1.5f);
                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                   mEditCountMinusX + bw * 0.5f,
                                   mEditCountMinusY + bh * 0.20f, "-", theme::Cyan());
                char nbuf[8]; std::snprintf(nbuf, sizeof(nbuf), "%d", mEditCount);
                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                   rx + bw + valW * 0.5f,
                                   ly + lineH * 0.28f, nbuf, theme::White());
                mEditCountPlusX = rx + bw + valW; mEditCountPlusY = mEditCountMinusY;
                r.DrawFilledRect ({ mEditCountPlusX, mEditCountPlusY, bw, bh }, { 0, 245, 255, 40 });
                r.DrawRectOutline({ mEditCountPlusX, mEditCountPlusY, bw, bh }, { 0, 245, 255, 200 }, 1.5f);
                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                   mEditCountPlusX + bw * 0.5f,
                                   mEditCountPlusY + bh * 0.20f, "+", theme::Cyan());
                ly += lineH;

                // FORCE
                f.DrawStringScaled(r, FontAtlas::BodySlot, scale,
                             lx, ly + lineH * 0.28f,
                             "FORCE", { 255, 255, 255, 220 });
                mEditPowerMinusX = rx; mEditPowerMinusY = ly + (lineH - bh) * 0.5f;
                r.DrawFilledRect ({ mEditPowerMinusX, mEditPowerMinusY, bw, bh }, { 0, 245, 255, 40 });
                r.DrawRectOutline({ mEditPowerMinusX, mEditPowerMinusY, bw, bh }, { 0, 245, 255, 200 }, 1.5f);
                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                   mEditPowerMinusX + bw * 0.5f,
                                   mEditPowerMinusY + bh * 0.20f, "-", theme::Cyan());
                const char* pwlbl = (mEditPowerLevel == 1) ? "FAIBLE"
                                  : (mEditPowerLevel == 3) ? "ELEVEE" : "NORMALE";
                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                   rx + bw + valW * 0.5f,
                                   ly + lineH * 0.28f, pwlbl, theme::White());
                mEditPowerPlusX = rx + bw + valW; mEditPowerPlusY = mEditPowerMinusY;
                r.DrawFilledRect ({ mEditPowerPlusX, mEditPowerPlusY, bw, bh }, { 0, 245, 255, 40 });
                r.DrawRectOutline({ mEditPowerPlusX, mEditPowerPlusY, bw, bh }, { 0, 245, 255, 200 }, 1.5f);
                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                   mEditPowerPlusX + bw * 0.5f,
                                   mEditPowerPlusY + bh * 0.20f, "+", theme::Cyan());
                ly += lineH;

                // IMPREVISIBLE (toggle)
                f.DrawStringScaled(r, FontAtlas::BodySlot, scale,
                             lx, ly + lineH * 0.28f,
                             "IMPREVISIBLE (BOUGE)", { 255, 255, 255, 220 });
                const float tW = Pct::W(W, 0.100f, 64.0f, 120.0f);
                const float tH = Pct::H(H, 0.045f, 24.0f, 48.0f);
                mEditChaoticX = mEditPanelX + pW - pW * 0.07f - tW;
                mEditChaoticY = ly + (lineH - tH) * 0.5f;
                mEditChaoticW = tW; mEditChaoticH = tH;
                math::NkColor tbg = mEditChaotic
                    ? math::NkColor{ 0, 245, 255, 80 }
                    : math::NkColor{ 255, 255, 255, 30 };
                math::NkColor tbd = mEditChaotic
                    ? math::NkColor{ 0, 245, 255, 220 }
                    : math::NkColor{ 255, 255, 255, 100 };
                r.DrawFilledRect ({ mEditChaoticX, mEditChaoticY, tW, tH }, tbg);
                r.DrawRectOutline({ mEditChaoticX, mEditChaoticY, tW, tH }, tbd, 1.5f);
                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                   mEditChaoticX + tW * 0.5f,
                                   mEditChaoticY + tH * 0.20f,
                                   mEditChaotic ? "ON" : "OFF",
                                   mEditChaotic ? theme::Cyan()
                                                : math::NkColor{ 255, 255, 255, 160 });
                ly += lineH;

                // Bouton APPLIQUER (centre bas du panneau)
                const float aW = math::NkMin(pW * 0.6f,
                                             Pct::W(W, 0.25f, 160.0f, 280.0f));
                const float aH = Pct::H(H, 0.065f, 36.0f, 68.0f);
                mEditApplyW = aW; mEditApplyH = aH;
                mEditApplyX = mEditPanelX + (pW - aW) * 0.5f;
                mEditApplyY = mEditPanelY + pH - aH - pH * 0.06f;
                r.DrawFilledRect ({ mEditApplyX, mEditApplyY, aW, aH }, { 0, 245, 255, 60 });
                r.DrawRectOutline({ mEditApplyX, mEditApplyY, aW, aH }, theme::Cyan(), 2.0f * scale);
                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                   mEditApplyX + aW * 0.5f,
                                   mEditApplyY + aH * 0.28f,
                                   "APPLIQUER", theme::Cyan());
            }

            // ── Scrollbar minimaliste a droite si overflow ──────────────────
            // Le track est ALIGNE pixel-perfect avec le haut de la zone
            // scrollable et le bas du rectangle sticky du bouton LANCER (pas
            // de padding 2px qui creait un gap visible).
            if (mMaxScroll > 0.5f)
            {
                // Scrollbar : largeur en % W (fine), thumb min en % H pour
                // rester saisissable au doigt sur mobile.
                const float sbW = Pct::W(W, 0.006f, 3.0f, 8.0f);
                const float sbX = safeX + safeW - sbW - Pct::W(W, 0.004f, 2.0f, 6.0f);
                const float sbY = scrollTop;
                const float sbH = scrollH;
                r.DrawFilledRect({ sbX, sbY, sbW, sbH }, { 255, 255, 255, 16 });
                const float frac = scrollH / contentH;
                const float thumbH = math::NkMax(Pct::H(H, 0.030f, 18.0f, 40.0f),
                                                 sbH * frac);
                const float thumbY = sbY
                                   + (sbH - thumbH) * (mScrollY / mMaxScroll);
                r.DrawFilledRect({ sbX, thumbY, sbW, thumbH },
                           { 0, 245, 255, 180 });
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        // Hit-test helpers
        // ─────────────────────────────────────────────────────────────────────
        int SelectMatchConfigScene::HitTestDiff(float worldX, float worldY) const
        {
            if (!mShowDifficulty) return -1;
            for (int i = 0; i < kDiffCount; ++i)
            {
                if (worldX >= mDiffX[i] && worldX <= mDiffX[i] + mDiffW
                 && worldY >= mDiffY[i] && worldY <= mDiffY[i] + mDiffH)
                    return i;
            }
            return -1;
        }
        int SelectMatchConfigScene::HitTestDiffR(float worldX, float worldY) const
        {
            if (!mShowDifficultyR) return -1;
            for (int i = 0; i < kDiffCount; ++i)
            {
                if (worldX >= mDiffXR[i] && worldX <= mDiffXR[i] + mDiffW
                 && worldY >= mDiffYR[i] && worldY <= mDiffYR[i] + mDiffH)
                    return i;
            }
            return -1;
        }
        int SelectMatchConfigScene::HitTestObs(float worldX, float worldY) const
        {
            for (int i = 0; i < kObsCount; ++i)
            {
                if (worldX >= mObsX[i] && worldX <= mObsX[i] + mObsW
                 && worldY >= mObsY[i] && worldY <= mObsY[i] + mObsH)
                    return i;
            }
            return -1;
        }
        bool SelectMatchConfigScene::HitTestLaunch(float screenX, float screenY) const
        {
            return screenX >= mLaunchX && screenX <= mLaunchX + mLaunchW
                && screenY >= mLaunchY && screenY <= mLaunchY + mLaunchH;
        }
        bool SelectMatchConfigScene::HitTestBack(float screenX, float screenY) const
        {
            return screenX >= mBackX && screenX <= mBackX + mBackW
                && screenY >= mBackY && screenY <= mBackY + mBackH;
        }
        bool SelectMatchConfigScene::HitTestObsCheck(float worldX, float worldY,
                                                     int& outIndex) const
        {
            for (int i = 0; i < kObsCount; ++i)
            {
                if (worldX >= mObsCheckX[i] && worldX <= mObsCheckX[i] + mObsCheckW
                 && worldY >= mObsCheckY[i] && worldY <= mObsCheckY[i] + mObsCheckH)
                {
                    outIndex = i;
                    return true;
                }
            }
            return false;
        }
        int SelectMatchConfigScene::HitTestEdit(float sx, float sy) const
        {
            auto in = [&](float x, float y, float w, float h) {
                return sx >= x && sx <= x + w && sy >= y && sy <= y + h;
            };
            if (in(mEditCountMinusX, mEditCountMinusY, mEditBtnW, mEditBtnH)) return 1;
            if (in(mEditCountPlusX,  mEditCountPlusY,  mEditBtnW, mEditBtnH)) return 2;
            if (in(mEditPowerMinusX, mEditPowerMinusY, mEditBtnW, mEditBtnH)) return 3;
            if (in(mEditPowerPlusX,  mEditPowerPlusY,  mEditBtnW, mEditBtnH)) return 4;
            if (in(mEditChaoticX,    mEditChaoticY,    mEditChaoticW, mEditChaoticH)) return 5;
            if (in(mEditCloseX,      mEditCloseY,      mEditBtnH, mEditBtnH)) return 6;
            if (in(mEditApplyX,      mEditApplyY,      mEditApplyW, mEditApplyH)) return 7;
            return 0;
        }
        void SelectMatchConfigScene::OpenEditPanel(int obsIndex, const GameSettings& s)
        {
            mEditingObsIndex = obsIndex;
            // Pour count = 0 (default random), on initialise a 2 pour
            // permettre l'edition.
            mEditCount      = s.obsCount[obsIndex] > 0 ? s.obsCount[obsIndex] : 2;
            mEditPowerLevel = s.obsPowerLevel[obsIndex];
            mEditChaotic    = s.obsChaotic[obsIndex];
        }
        void SelectMatchConfigScene::ApplyEditPanel(AppContext& ctx)
        {
            if (mEditingObsIndex < 0 || ctx.settings == nullptr)
            {
                CloseEditPanel();
                return;
            }
            ctx.settings->obsCount     [mEditingObsIndex] = mEditCount;
            ctx.settings->obsPowerLevel[mEditingObsIndex] = mEditPowerLevel;
            ctx.settings->obsChaotic   [mEditingObsIndex] = mEditChaotic;
            CloseEditPanel();
        }
        void SelectMatchConfigScene::CloseEditPanel()
        {
            mEditingObsIndex = -1;
        }
        int SelectMatchConfigScene::HitTestParam(float wx, float wy) const
        {
            auto inRect = [&](float bx, float by, float bw, float bh) {
                return wx >= bx && wx <= bx + bw
                    && wy >= by && wy <= by + bh;
            };
            if (inRect(mScoreMinusX, mScoreMinusY, mStepperW, mStepperH)) return 1;
            if (inRect(mScorePlusX,  mScorePlusY,  mStepperW, mStepperH)) return 2;
            if (inRect(mTimeMinusX,  mTimeMinusY,  mStepperW, mStepperH)) return 3;
            if (inRect(mTimePlusX,   mTimePlusY,   mStepperW, mStepperH)) return 4;
            if (inRect(mTogglePtsX,  mTogglePtsY,  mToggleW,  mToggleH))  return 5;
            if (inRect(mSpeedMinusX, mSpeedMinusY, mStepperW, mStepperH)) return 6;
            if (inRect(mSpeedPlusX,  mSpeedPlusY,  mStepperW, mStepperH)) return 7;
            return 0;
        }

        // ─────────────────────────────────────────────────────────────────────
        void SelectMatchConfigScene::Launch(AppContext& ctx)
        {
            if (ctx.settings != nullptr)
            {
                if (mShowDifficulty && mDiffIndex >= 0 && mDiffIndex < kDiffCount)
                {
                    ctx.settings->difficulty = kDiffsL[mDiffIndex].level;
                }
                if (mShowDifficultyR && mDiffIndexR >= 0 && mDiffIndexR < kDiffCount)
                {
                    ctx.settings->difficultyP2 = kDiffsL[mDiffIndexR].level;
                }
                for (int i = 0; i < kObsCount; ++i)
                {
                    ctx.settings->obsActive[i] = mObsActive[i];
                }
                // Parametres de match
                ctx.settings->maxScore     = kScoreOptions[mScoreIndex];
                ctx.settings->timeLimit    = kTimeOptions [mTimeIndex];
                ctx.settings->winByTwo     = mWinByTwo;
                ctx.settings->ballSpeedMul = kSpeedOptions[mSpeedIndex];
            }
            int countOn = 0;
            for (int i = 0; i < kObsCount; ++i) if (mObsActive[i]) countOn++;
            logger.Info("[MatchConfig] Launch (diff={0}, {1}/8 obs, score={2}, time={3})",
                        mShowDifficulty ? mDiffIndex : -1, countOn,
                        kScoreOptions[mScoreIndex],
                        (int)kTimeOptions[mTimeIndex]);

            // ── Mode reseau HOST : envoyer kMsgStartMatch au client ─────────
            // Le client recevra le message dans NetworkLobbyScene::OnUpdate et
            // pushera GameplayScene avec les memes settings critiques. Cote
            // local on continue avec le Push habituel ci-dessous.
            if (ctx.network != nullptr
             && ctx.network->State() == NetworkState::Connected
             && ctx.network->Role() == NetworkRole::Host
             && ctx.settings != nullptr)
            {
                // Genere une seed deterministe pour spawn obstacles. La meme
                // seed est appliquee cote HOST (ici) et CLIENT (a la reception
                // du message). Resultat : memes obstacles aux memes positions.
                // std::time(nullptr) varie chaque seconde -> unique par match.
                ctx.settings->obstacleSeed = (uint32)std::time(nullptr);

                netproto::PktStartMatch pkt;
                pkt.type         = netproto::kMsgStartMatch;
                pkt.maxScore     = (uint16)math::NkMax(0, ctx.settings->maxScore);
                pkt.timeLimitSec = (uint16)math::NkMax(0.0f, ctx.settings->timeLimit);
                pkt.ballSpeedMul = ctx.settings->ballSpeedMul;
                uint8 flags = 0;
                if (ctx.settings->winByTwo)        flags |= netproto::kStartFlagWinByTwo;
                if (ctx.settings->powerUpsEnabled) flags |= netproto::kStartFlagPowerUpsOn;
                pkt.flags        = flags;
                pkt.obstacleSeed = ctx.settings->obstacleSeed;
                for (int i = 0; i < 8; ++i)
                {
                    pkt.obsActive[i] = ctx.settings->obsActive[i] ? 1 : 0;
                }
                ctx.network->Broadcast(reinterpret_cast<const uint8*>(&pkt),
                                       sizeof(pkt), 1 /*reliable*/);
                logger.Info("[MatchConfig] HOST envoie kMsgStartMatch au client (seed={0})",
                            ctx.settings->obstacleSeed);
            }

            // PUSH (pas Replace) : RETOUR depuis Gameplay revient ici sur
            // MatchConfig avec les memes settings, l'user peut ajuster et
            // relancer. Le bouton "RETOUR MENU" (game over / pause) utilise
            // PopToRoot pour sauter directement au menu principal.
            ctx.scenes->Push(new GameplayScene());
        }

        // Helper : applique un clic param (button id 1-7)
        static void ApplyParamButton(int btn,
                                     int& scoreIndex, int& timeIndex,
                                     bool& winByTwo,
                                     int& speedIndex)
        {
            switch (btn)
            {
            case 1:  // score -
                scoreIndex = (scoreIndex - 1 + kScoreOptionsN) % kScoreOptionsN;
                break;
            case 2:  // score +
                scoreIndex = (scoreIndex + 1) % kScoreOptionsN;
                break;
            case 3:  // time -
                timeIndex = (timeIndex - 1 + kTimeOptionsN) % kTimeOptionsN;
                break;
            case 4:  // time +
                timeIndex = (timeIndex + 1) % kTimeOptionsN;
                break;
            case 5:  // toggle winByTwo
                winByTwo = !winByTwo;
                break;
            case 6:  // speed -
                speedIndex = (speedIndex - 1 + kSpeedOptionsN) % kSpeedOptionsN;
                break;
            case 7:  // speed +
                speedIndex = (speedIndex + 1) % kSpeedOptionsN;
                break;
            default: break;
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        void SelectMatchConfigScene::OnEvent(AppContext& ctx, NkEvent& ev)
        {
            const float scrollStep = 60.0f;  // pixels par PgUp/Dn ou molette

            // ── Clavier ─────────────────────────────────────────────────────
            if (auto* kp = ev.As<NkKeyPressEvent>())
            {
                const NkKey k = kp->GetKey();
                if (k == NkKey::NK_ENTER || k == NkKey::NK_NUMPAD_ENTER)
                {
                    Launch(ctx);
                    return;
                }
                if (k == NkKey::NK_ESCAPE)
                {
                    ctx.scenes->Pop();
                    return;
                }
                if (k == NkKey::NK_DOWN)      mScrollY = ClampScroll(mScrollY + scrollStep);
                else if (k == NkKey::NK_UP)   mScrollY = ClampScroll(mScrollY - scrollStep);
                else if (k == NkKey::NK_PAGE_DOWN)
                    mScrollY = ClampScroll(mScrollY + scrollStep * 3.0f);
                else if (k == NkKey::NK_PAGE_UP)
                    mScrollY = ClampScroll(mScrollY - scrollStep * 3.0f);
                return;
            }

            // ── Souris : clic + molette ─────────────────────────────────────
            if (auto* mp = ev.As<NkMouseButtonPressEvent>())
            {
                if (mp->GetButton() == NkMouseButton::NK_MB_LEFT)
                {
                    const float px = (float)mp->GetX();
                    const float py = (float)mp->GetY();
                    // Bouton RETOUR (coords ecran, sticky en haut)
                    if (HitTestBack(px, py)) { ctx.scenes->Pop(); return; }
                    // Bouton LANCER (coords ecran)
                    if (HitTestLaunch(px, py)) { Launch(ctx); return; }
                    // Stepper -/+ : declenche immediatement + arme le hold-to-repeat.
                    // Le toggle (id=5) ne s'auto-repete pas — un click = un toggle.
                    if (mEditingObsIndex < 0)
                    {
                        const float scrollTop = (float)ctx.safe.TopY() + mTopReserve;
                        const float worldX = px;
                        const float worldY = (py - scrollTop) + mScrollY;
                        const int pb = HitTestParam(worldX, worldY);
                        if (pb > 0)
                        {
                            ApplyParamButton(pb, mScoreIndex, mTimeIndex,
                                             mWinByTwo, mSpeedIndex);
                            mPressConsumed = true;
                            if (pb != 5)   // pas de repeat sur toggle
                            {
                                mArmedStepper = pb;
                                mHoldTime     = 0.0f;
                                mRepeatAccum  = 0.0f;
                            }
                            return;
                        }
                    }
                    // Demarre un drag potentiel pour scroll vertical
                    mMouseDown = true;
                    mDragActive = true;
                    mDragWasScroll = false;
                    mDragStartY = py;
                    mDragLastY = py;
                }
                return;
            }
            if (auto* mr = ev.As<NkMouseButtonReleaseEvent>())
            {
                // Cesse le hold-to-repeat + ignore le release si le press
                // a deja consume l'evenement (stepper ou toggle).
                if (mPressConsumed)
                {
                    mArmedStepper  = 0;
                    mPressConsumed = false;
                    return;
                }
                if (mr->GetButton() == NkMouseButton::NK_MB_LEFT && mMouseDown)
                {
                    mMouseDown = false;
                    // Si pas de scroll significatif -> c'est un tap : toggle
                    if (!mDragWasScroll)
                    {
                        const float px = (float)mr->GetX();
                        const float py = (float)mr->GetY();
                        // ── Si panneau d'edition ouvert : prioritaire ──────
                        if (mEditingObsIndex >= 0)
                        {
                            const int eb = HitTestEdit(px, py);
                            switch (eb)
                            {
                            case 1: if (mEditCount > 0)  mEditCount--; return;
                            case 2: if (mEditCount < 12) mEditCount++; return;
                            case 3: if (mEditPowerLevel > 1) mEditPowerLevel--; return;
                            case 4: if (mEditPowerLevel < 3) mEditPowerLevel++; return;
                            case 5: mEditChaotic = !mEditChaotic; return;
                            case 6: CloseEditPanel(); return;
                            case 7: ApplyEditPanel(ctx); return;
                            default: return;  // clic ailleurs = absorbe
                            }
                        }
                        const float scrollTop = (float)ctx.safe.TopY() + mTopReserve;
                        const float worldX = px;
                        const float worldY = (py - scrollTop) + mScrollY;
                        // 1. Steppers/toggle parametres prioritaires
                        const int pb = HitTestParam(worldX, worldY);
                        if (pb > 0)
                        {
                            ApplyParamButton(pb, mScoreIndex, mTimeIndex, mWinByTwo, mSpeedIndex);
                            return;
                        }
                        const int di = HitTestDiff(worldX, worldY);
                        if (di >= 0) { mDiffIndex = di; return; }
                        const int diR = HitTestDiffR(worldX, worldY);
                        if (diR >= 0) { mDiffIndexR = diR; return; }
                        // 2. Case a cocher d'un obstacle = toggle uniquement
                        int oc = -1;
                        if (HitTestObsCheck(worldX, worldY, oc))
                        {
                            mObsActive[oc] = !mObsActive[oc];
                            return;
                        }
                        // 3. Corps card obstacle = ouvre panneau d'edition
                        const int oi = HitTestObs(worldX, worldY);
                        if (oi >= 0 && ctx.settings != nullptr)
                        {
                            OpenEditPanel(oi, *ctx.settings);
                            return;
                        }
                    }
                    mDragActive = false;
                    mDragWasScroll = false;
                }
                return;
            }
            if (auto* mm = ev.As<NkMouseMoveEvent>())
            {
                if (mMouseDown && mDragActive)
                {
                    const float py = (float)mm->GetY();
                    const float dy = py - mDragLastY;
                    if (math::NkFabs(py - mDragStartY) > 6.0f) mDragWasScroll = true;
                    mScrollY = ClampScroll(mScrollY - dy);
                    mDragLastY = py;
                }
                return;
            }

            // ── Touch : drag scroll OU tap toggle ───────────────────────────
            if (auto* tb = ev.As<NkTouchBeginEvent>())
            {
                if (tb->GetNumTouches() > 0)
                {
                    const NkTouchPoint& tp = tb->GetTouch(0);
                    if (HitTestBack(tp.clientX, tp.clientY))
                    {
                        ctx.scenes->Pop();
                        return;
                    }
                    if (HitTestLaunch(tp.clientX, tp.clientY))
                    {
                        Launch(ctx);
                        return;
                    }
                    // Stepper hold-to-repeat (idem mouse press).
                    if (mEditingObsIndex < 0)
                    {
                        const float scrollTop = (float)ctx.safe.TopY() + mTopReserve;
                        const float worldX = tp.clientX;
                        const float worldY = (tp.clientY - scrollTop) + mScrollY;
                        const int pb = HitTestParam(worldX, worldY);
                        if (pb > 0)
                        {
                            ApplyParamButton(pb, mScoreIndex, mTimeIndex,
                                             mWinByTwo, mSpeedIndex);
                            mPressConsumed = true;
                            if (pb != 5)
                            {
                                mArmedStepper = pb;
                                mHoldTime     = 0.0f;
                                mRepeatAccum  = 0.0f;
                            }
                            mDragTouchId = (long long)tp.id;  // tracker pour le release
                            return;
                        }
                    }
                    mDragActive = true;
                    mDragWasScroll = false;
                    mDragStartY = tp.clientY;
                    mDragLastY = tp.clientY;
                    mDragTouchId = (long long)tp.id;
                }
                return;
            }
            if (auto* tm = ev.As<NkTouchMoveEvent>())
            {
                // Si on hold un stepper, on n'autorise pas le scroll qui
                // confondrait l'utilisateur. Le hold continue jusqu'au touch end.
                if (mArmedStepper > 0) return;
                for (uint32 i = 0; i < tm->GetNumTouches(); ++i)
                {
                    const NkTouchPoint& tp = tm->GetTouch(i);
                    if ((long long)tp.id != mDragTouchId) continue;
                    const float dy = tp.clientY - mDragLastY;
                    if (math::NkFabs(tp.clientY - mDragStartY) > 6.0f)
                        mDragWasScroll = true;
                    mScrollY = ClampScroll(mScrollY - dy);
                    mDragLastY = tp.clientY;
                }
                return;
            }
            if (auto* te = ev.As<NkTouchEndEvent>())
            {
                // Stop hold-to-repeat + ignore le release si press deja consume.
                if (mPressConsumed)
                {
                    mArmedStepper  = 0;
                    mPressConsumed = false;
                    mDragTouchId   = -1;
                    return;
                }
                for (uint32 i = 0; i < te->GetNumTouches(); ++i)
                {
                    const NkTouchPoint& tp = te->GetTouch(i);
                    if ((long long)tp.id != mDragTouchId) continue;
                    if (!mDragWasScroll)
                    {
                        // Si panneau ouvert : prioritaire
                        if (mEditingObsIndex >= 0)
                        {
                            const int eb = HitTestEdit(tp.clientX, tp.clientY);
                            switch (eb)
                            {
                            case 1: if (mEditCount > 0)  mEditCount--; break;
                            case 2: if (mEditCount < 12) mEditCount++; break;
                            case 3: if (mEditPowerLevel > 1) mEditPowerLevel--; break;
                            case 4: if (mEditPowerLevel < 3) mEditPowerLevel++; break;
                            case 5: mEditChaotic = !mEditChaotic; break;
                            case 6: CloseEditPanel(); break;
                            case 7: ApplyEditPanel(ctx); break;
                            default: break;  // tap ailleurs = absorbe
                            }
                        }
                        else
                        {
                            const float scrollTop = (float)ctx.safe.TopY() + mTopReserve;
                            const float worldX = tp.clientX;
                            const float worldY = (tp.clientY - scrollTop) + mScrollY;
                            const int pb = HitTestParam(worldX, worldY);
                            if (pb > 0)
                            {
                                ApplyParamButton(pb, mScoreIndex, mTimeIndex, mWinByTwo, mSpeedIndex);
                            }
                            else
                            {
                                const int di = HitTestDiff(worldX, worldY);
                                if (di >= 0) { mDiffIndex = di; }
                                else if (int diR = HitTestDiffR(worldX, worldY); diR >= 0)
                                {
                                    mDiffIndexR = diR;
                                }
                                else
                                {
                                    int oc = -1;
                                    if (HitTestObsCheck(worldX, worldY, oc))
                                    {
                                        mObsActive[oc] = !mObsActive[oc];
                                    }
                                    else
                                    {
                                        const int oi = HitTestObs(worldX, worldY);
                                        if (oi >= 0 && ctx.settings != nullptr)
                                        {
                                            OpenEditPanel(oi, *ctx.settings);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    mDragActive = false;
                    mDragTouchId = -1;
                    mDragWasScroll = false;
                }
                return;
            }
        }

    } // namespace pong
} // namespace nkentseu
