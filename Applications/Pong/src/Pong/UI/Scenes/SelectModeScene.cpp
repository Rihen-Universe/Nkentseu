// =============================================================================
// SelectModeScene.cpp
// =============================================================================

#include "SelectModeScene.h"
#include "SelectMatchConfigScene.h"
#include "NetworkLobbyScene.h"
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
#include "NKMath/NkFunctions.h"

namespace nkentseu
{
    namespace pong
    {

        // ── Description des 4 modes ──────────────────────────────────────────
        struct ModeDesc
        {
            GameMode      mode;
            const char*   title;
            const char*   sub;
            int           iconId;        // pour DrawIcon
            math::NkColor accent;
            bool          enabled;       // false pour Reseau (pas encore impl)
        };

        static const ModeDesc kModes[SelectModeScene::kModeCount] =
        {
            { GameMode::Local,         "JOUER EN LOCAL",     "1V1 - MEME ECRAN, 2 JOUEURS",
              0, { 0, 245, 255, 255 }, true  },
            { GameMode::NetworkLAN,    "MULTIJOUEUR RESEAU", "1V1 EN LAN — HEBERGER OU REJOINDRE",
              1, { 255, 107,   0, 255 }, true  },
            { GameMode::VsAI,          "VS IA",              "JOUER CONTRE L'ORDINATEUR",
              2, { 0, 245, 255, 255 }, true  },
            { GameMode::AIvsAI,        "IA VS IA",           "MODE DEMO - 2 IA S'AFFRONTENT",
              3, { 204, 119, 255, 255 }, true  },
        };

        // ── Icones simples (style identique aux icones MainMenu) ─────────────
        static void DrawModeIcon(GLRenderer2D& r, int iconId,
                                 float boxX, float boxY, float boxSize,
                                 math::NkColor c)
        {
            const float cx = boxX + boxSize * 0.5f;
            const float cy = boxY + boxSize * 0.5f;
            const float hb = boxSize * 0.5f;
            switch (iconId)
            {
            case 0:  // Manette (Local)
            {
                const float w = boxSize * 0.8f;
                const float h = boxSize * 0.5f;
                r.DrawQuadOutline(cx - w * 0.5f, cy - h * 0.5f, w, h, c, 1.5f);
                r.DrawCircle(cx + w * 0.25f, cy,         2.0f, c, 8);
                r.DrawCircle(cx + w * 0.35f, cy - 4.0f,  2.0f, c, 8);
                r.DrawQuad  (cx - w * 0.4f,  cy - 1.0f,  8.0f, 2.0f, c);
                r.DrawQuad  (cx - w * 0.35f, cy - 4.0f,  2.0f, 8.0f, c);
                break;
            }
            case 1:  // Antenne wifi (Reseau)
            {
                r.DrawCircle(cx, cy + 6.0f, 2.0f, c, 12);
                r.DrawCircleOutline(cx, cy + 6.0f, 6.0f, c, 1.5f, 24);
                r.DrawCircleOutline(cx, cy + 6.0f, 10.0f, c, 1.5f, 28);
                break;
            }
            case 2:  // Robot (vs IA)
            {
                const float w = boxSize * 0.55f;
                const float h = boxSize * 0.55f;
                r.DrawQuadOutline(cx - w * 0.5f, cy - h * 0.4f, w, h, c, 1.5f);
                r.DrawQuad(cx - w * 0.25f, cy - h * 0.2f, 3.0f, 3.0f, c);
                r.DrawQuad(cx + w * 0.15f, cy - h * 0.2f, 3.0f, 3.0f, c);
                r.DrawQuad(cx - 0.5f, cy - h * 0.4f - 4.0f, 1.0f, 4.0f, c);
                break;
            }
            case 3:  // 2 robots face-a-face (IA vs IA)
            {
                for (int i = 0; i < 2; ++i)
                {
                    const float fx = (i == 0) ? cx - 8.0f : cx + 8.0f;
                    r.DrawQuadOutline(fx - 4.0f, cy - 4.0f, 8.0f, 8.0f, c, 1.2f);
                    r.DrawQuad(fx - 1.5f, cy - 1.5f, 1.5f, 1.5f, c);
                }
                r.DrawQuad(cx - 0.5f, cy - 3.0f, 1.0f, 6.0f, c);
                break;
            }
            default:
            {
                r.DrawQuadOutline(boxX, boxY, boxSize, boxSize, c, 1.5f);
                (void)hb;
                break;
            }
            }
        }

        static float EaseOutCubic(float t)
        {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            const float u = 1.0f - t;
            return 1.0f - u * u * u;
        }

        // ─────────────────────────────────────────────────────────────────────
        void SelectModeScene::OnEnter(AppContext& /*ctx*/)
        {
            mTime = 0.0f;
            mEnterAnim = 0.0f;
            // Focus par defaut sur l'item "Local" (mode le plus accessible)
            mFocusIndex = 0;
            logger.Info("[SelectMode] OnEnter");
        }

        void SelectModeScene::OnUpdate(AppContext& /*ctx*/, float dt)
        {
            mTime += dt;
            mEnterAnim += dt / 0.4f;
            if (mEnterAnim > 1.0f) mEnterAnim = 1.0f;
        }

        // ─────────────────────────────────────────────────────────────────────
        void SelectModeScene::OnRender(AppContext& ctx)
        {
            GLRenderer2D& r = *ctx.renderer;
            FontAtlas&    f = *ctx.font;
            const int W = ctx.viewportW;
            const int H = ctx.viewportH;
            const float scale = GetUIScale(W, H);

            r.Clear(theme::Dark().r / 255.0f,
                    theme::Dark().g / 255.0f,
                    theme::Dark().b / 255.0f, 1.0f);
            r.Begin(W, H);

            // Background grille (rapide)
            const math::NkColor grid = { 0, 245, 255, 10 };
            const float step = 40.0f;
            for (float x = 0; x < (float)W; x += step) r.DrawQuad(x, 0.0f, 1.0f, (float)H, grid);
            for (float y = 0; y < (float)H; y += step) r.DrawQuad(0.0f, y, (float)W, 1.0f, grid);

            const float safeX = (float)ctx.safe.LeftX();
            const float safeY = (float)ctx.safe.TopY();
            const float safeW = (float)ctx.safe.SafeW();
            const float safeH = (float)ctx.safe.SafeH();
            const float cx = safeX + safeW * 0.5f;

            // Bouton RETOUR (haut-gauche)
            {
                const float bw = 100.0f * scale;
                const float bh = 36.0f * scale;
                mBackW = bw; mBackH = bh;
                mBackX = safeX + 16.0f * scale;
                mBackY = safeY + 16.0f * scale;
                r.DrawQuad       (mBackX, mBackY, bw, bh, { 255, 255, 255, 16 });
                r.DrawQuadOutline(mBackX, mBackY, bw, bh, { 0, 245, 255, 180 }, 1.5f * scale);
                f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                   mBackX + bw * 0.5f, mBackY + bh * 0.30f,
                                   "< RETOUR", theme::Cyan());
            }

            // Titre
            f.DrawStringShadowCenteredScaled(r, FontAtlas::HeadlineSlot, scale,
                                     cx, safeY + 40.0f * scale,
                                     "CHOISIS UN MODE",
                                     theme::White(), theme::Cyan(), 3);
            f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                cx, safeY + 90.0f * scale,
                                "FLECHES GAUCHE/DROITE  -  ENTREE  -  ECHAP",
                                { 255, 255, 255, 110 });

            // ── Layout 4 cards en ligne horizontale ───────────────────────────
            // Taille adaptative : largeur card = (safeW - padding*5) / 4
            const float padding = math::NkMax(16.0f, 24.0f * scale);
            const float gap     = math::NkMax(10.0f, 16.0f * scale);
            const float cardW   = math::NkMax(140.0f,
                                  (safeW - padding * 2.0f - gap * 3.0f) / 4.0f);
            const float cardH   = math::NkMin(safeH * 0.50f,
                                              cardW * 1.20f);
            mCardW = cardW;
            mCardH = cardH;
            const float cardsY = safeY + (safeH - cardH) * 0.5f;
            const float startX = cx - (cardW * 4.0f + gap * 3.0f) * 0.5f;
            const float enterA = EaseOutCubic(mEnterAnim);

            for (int i = 0; i < kModeCount; ++i)
            {
                const ModeDesc& m = kModes[i];
                const float bx = startX + i * (cardW + gap);
                const float by = cardsY;
                mCardXs[i] = bx;
                mCardYs[i] = by;

                // Anim entree decale par card
                float a = mEnterAnim - 0.08f * (float)i;
                if (a < 0.0f) a = 0.0f;
                a = EaseOutCubic(math::NkMin(a, 1.0f)) * enterA;
                const float slideY = (1.0f - a) * 40.0f;

                const bool focused = (i == mFocusIndex);
                const bool disabled = !m.enabled;

                math::NkColor bg   = m.accent; bg.a   = focused ? 50 : 18;
                math::NkColor bord = m.accent; bord.a = focused ? 220 : 80;
                if (disabled) { bg.a = 10; bord.a = 40; }
                if (focused && !disabled)
                {
                    const float pulse = 0.5f + 0.5f * math::NkSin(mTime * 4.0f);
                    bg.a = static_cast<uint8>(bg.a + 30 * pulse);
                }
                bg.a   = static_cast<uint8>(bg.a   * a);
                bord.a = static_cast<uint8>(bord.a * a);

                r.DrawQuad       (bx, by + slideY, cardW, cardH, bg);
                r.DrawQuadOutline(bx, by + slideY, cardW, cardH, bord, focused ? 2.5f * scale : 1.0f);

                // Icone (boite carree au tier superieur)
                const float iconSize = cardW * 0.45f;
                const float iconX = bx + (cardW - iconSize) * 0.5f;
                const float iconY = by + slideY + cardH * 0.12f;
                math::NkColor iconC = m.accent;
                iconC.a = static_cast<uint8>((disabled ? 100 : 220) * a);
                DrawModeIcon(r, m.iconId, iconX, iconY, iconSize, iconC);

                // Titre + sous-titre
                math::NkColor titleC = disabled ? math::NkColor{ 255, 255, 255, 100 }
                                                : theme::White();
                titleC.a = static_cast<uint8>(titleC.a * a);
                math::NkColor subC = { 255, 255, 255, 130 };
                subC.a = static_cast<uint8>(subC.a * a);

                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                   bx + cardW * 0.5f,
                                   by + slideY + cardH * 0.62f,
                                   m.title, titleC);
                f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                   bx + cardW * 0.5f,
                                   by + slideY + cardH * 0.78f,
                                   m.sub, subC);
            }

            // Footer hint
            f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                cx, safeY + safeH - 24.0f * scale,
                                "ENTREE / TAP : LANCER       ECHAP : RETOUR MENU",
                                { 255, 255, 255, 110 });

            r.End();
        }

        // ─────────────────────────────────────────────────────────────────────
        int SelectModeScene::HitTestCard(float px, float py) const
        {
            for (int i = 0; i < kModeCount; ++i)
            {
                if (px >= mCardXs[i] && px <= mCardXs[i] + mCardW
                 && py >= mCardYs[i] && py <= mCardYs[i] + mCardH)
                    return i;
            }
            return -1;
        }
        bool SelectModeScene::HitTestBack(float px, float py) const
        {
            return px >= mBackX && px <= mBackX + mBackW
                && py >= mBackY && py <= mBackY + mBackH;
        }

        void SelectModeScene::ActivateMode(AppContext& ctx, int index)
        {
            if (index < 0 || index >= kModeCount) return;
            const ModeDesc& m = kModes[index];
            if (!m.enabled)
            {
                logger.Info("[SelectMode] Mode '{0}' pas encore disponible", m.title);
                return;
            }
            if (ctx.settings != nullptr) ctx.settings->mode = m.mode;
            logger.Info("[SelectMode] Mode selectionne : {0}", m.title);
            // Mode reseau : on passe par le lobby (Host/Join) avant
            // de configurer le match. Autres modes : direct vers MatchConfig.
            if (m.mode == GameMode::NetworkLAN
             || m.mode == GameMode::NetworkOnline)
            {
                ctx.scenes->Push(new NetworkLobbyScene());
            }
            else
            {
                // PUSH (pas Replace) pour que RETOUR depuis SelectMatchConfig
                // revienne ici (SelectMode) plutot que sauter au MainMenu.
                ctx.scenes->Push(new SelectMatchConfigScene());
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        void SelectModeScene::OnEvent(AppContext& ctx, NkEvent& ev)
        {
            if (auto* kp = ev.As<NkKeyPressEvent>())
            {
                const NkKey k = kp->GetKey();
                if (k == NkKey::NK_RIGHT)
                {
                    mFocusIndex = (mFocusIndex + 1) % kModeCount;
                }
                else if (k == NkKey::NK_LEFT)
                {
                    mFocusIndex = (mFocusIndex - 1 + kModeCount) % kModeCount;
                }
                else if (k == NkKey::NK_ENTER || k == NkKey::NK_NUMPAD_ENTER
                      || k == NkKey::NK_SPACE)
                {
                    ActivateMode(ctx, mFocusIndex);
                }
                else if (k == NkKey::NK_ESCAPE)
                {
                    ctx.scenes->Pop();
                }
                return;
            }
            if (auto* mp = ev.As<NkMouseButtonPressEvent>())
            {
                if (mp->GetButton() == NkMouseButton::NK_MB_LEFT)
                {
                    const float px = (float)mp->GetX();
                    const float py = (float)mp->GetY();
                    if (HitTestBack(px, py))
                    {
                        ctx.scenes->Pop();
                        return;
                    }
                    const int idx = HitTestCard(px, py);
                    if (idx >= 0)
                    {
                        mFocusIndex = idx;
                        ActivateMode(ctx, idx);
                    }
                }
                return;
            }
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
                    const int idx = HitTestCard(tp.clientX, tp.clientY);
                    if (idx >= 0)
                    {
                        mFocusIndex = idx;
                        ActivateMode(ctx, idx);
                    }
                }
                return;
            }
        }

    } // namespace pong
} // namespace nkentseu
