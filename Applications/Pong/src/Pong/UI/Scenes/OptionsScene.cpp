// =============================================================================
// OptionsScene.cpp
// =============================================================================

#include "OptionsScene.h"
#include "RulesScene.h"
#include "Pong/Render/GLRenderer2D.h"
#include "Pong/Render/FontAtlas.h"
#include "Pong/UI/Theme.h"
#include "Pong/UI/SceneManager.h"
#include "Pong/UI/UIScale.h"
#include "Pong/UI/ResponsiveLayout.h"
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

        // ── Description des 5 categories ─────────────────────────────────────
        struct CatDesc
        {
            OptionsScene::Category cat;
            const char*            title;
            const char*            sub;
            math::NkColor          accent;
            bool                   enabled;
        };

        static const CatDesc kCats[OptionsScene::kCatCount] =
        {
            { OptionsScene::Cat_Help,
              "AIDE & TUTORIEL",
              "REGLES, OBSTACLES, BONUS, MALUS, CONTROLES",
              { 0, 245, 255, 255 }, true  },
            { OptionsScene::Cat_Audio,
              "AUDIO",
              "VOLUME MUSIQUE, EFFETS, ANNONCE - A VENIR",
              { 255, 215,  0, 255 }, false },
            { OptionsScene::Cat_Graphics,
              "GRAPHIQUES",
              "QUALITE, GLOW, PARTICULES, DALTONIEN - A VENIR",
              { 204, 119, 255, 255 }, false },
            { OptionsScene::Cat_Controls,
              "CONTROLES",
              "VIBRATION, INVERSION, REMAP - A VENIR",
              { 255, 107,   0, 255 }, false },
            { OptionsScene::Cat_Network,
              "RESEAU",
              "REGION, PING - A VENIR (NKNETWORK)",
              {  80, 255, 100, 255 }, false },
        };

        static float EaseOutCubic(float t)
        {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            const float u = 1.0f - t;
            return 1.0f - u * u * u;
        }

        // ─────────────────────────────────────────────────────────────────────
        void OptionsScene::OnEnter(AppContext& /*ctx*/)
        {
            mTime = 0.0f;
            mEnterAnim = 0.0f;
            mFocusIndex = 0;
            // -1 = aucun touch propre a Options. Tout TouchEnd dont l'id ne
            // matche PAS sera ignore (evite la fuite cross-scene).
            mActiveTouchId = -1;
            // Grace period a l'entree : evite qu'un click qui a push Options
            // depuis MainMenu (release sur la pos de TUTORIEL/SUPPORTER de
            // MainMenu = pos sur Options) retrigger un Activate immediat.
            mInputArmDelay = 0.20f;
            logger.Info("[Options] OnEnter");
        }

        // Re-arme le grace period au retour d'un Pop de scene enfant (ex:
        // RulesScene Tutoriel -> RETOUR -> Options reapparait). Sans ca, le
        // mouse release / touch end qui a triggere RETOUR sur la scene enfant
        // se propage a Options et appelle aussi RETOUR -> Pop vers Main.
        void OptionsScene::OnResumedFromChild(AppContext& /*ctx*/)
        {
            mInputArmDelay = 0.20f;
            mActiveTouchId = -1;
        }

        void OptionsScene::OnUpdate(AppContext& /*ctx*/, float dt)
        {
            mTime += dt;
            mEnterAnim += dt / 0.3f;
            if (mEnterAnim > 1.0f) mEnterAnim = 1.0f;
            if (mInputArmDelay > 0.0f) mInputArmDelay -= dt;
        }

        bool OptionsScene::HitTestBack(float sx, float sy) const
        {
            return sx >= mBackX && sx <= mBackX + mBackW
                && sy >= mBackY && sy <= mBackY + mBackH;
        }
        int OptionsScene::HitTestCard(float sx, float sy) const
        {
            for (int i = 0; i < (int)kCatCount; ++i)
            {
                if (sx >= mCardX[i] && sx <= mCardX[i] + mCardW
                 && sy >= mCardY[i] && sy <= mCardY[i] + mCardH)
                    return i;
            }
            return -1;
        }

        // ─────────────────────────────────────────────────────────────────────
        void OptionsScene::OnRender(AppContext& ctx)
        {
            GLRenderer2D& r = *ctx.renderer;
            FontAtlas&    f = *ctx.font;
            const int W = ctx.viewportW;
            const int H = ctx.viewportH;
            const float scale = GetUIScale(W, H);
            const float safeX = (float)ctx.safe.LeftX();
            const float safeW = (float)ctx.safe.SafeW();
            const float enterA = EaseOutCubic(mEnterAnim);

            // Fond
            r.Clear(theme::Dark().r / 255.0f,
                    theme::Dark().g / 255.0f,
                    theme::Dark().b / 255.0f, 1.0f);
            r.Begin(W, H);

            // ── Header : bouton RETOUR + titre ─────────────────────────────
            // Dimensions responsive en % viewport (clamps doux). On garde
            // `scale` UNIQUEMENT pour le rendu des bitmaps (DrawStringScaled)
            // et les epaisseurs d'outline.
            {
                mBackW = Pct::W(W, 0.10f,  70.0f, 140.0f);
                mBackH = Pct::H(H, 0.055f, 32.0f,  56.0f);
                mBackX = (float)ctx.safe.LeftX() + Pct::W(W, 0.012f, 8.0f, 24.0f);
                mBackY = (float)ctx.safe.TopY()  + Pct::H(H, 0.020f, 8.0f, 28.0f);
                math::NkColor bg = { 0, 245, 255, 30 };
                math::NkColor bd = { 0, 245, 255, 200 };
                r.DrawQuad       (mBackX, mBackY, mBackW, mBackH, bg);
                r.DrawQuadOutline(mBackX, mBackY, mBackW, mBackH, bd, 1.5f);
                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                   mBackX + mBackW * 0.5f,
                                   mBackY + mBackH * 0.18f,
                                   "RETOUR", theme::Cyan());

                f.DrawStringCenteredScaled(r, FontAtlas::HeadlineSlot, scale,
                                   (float)W * 0.5f,
                                   (float)ctx.safe.TopY() + Pct::H(H, 0.024f, 10.0f, 32.0f),
                                   "OPTIONS",
                                   theme::White());
            }

            // ── Liste des categories ───────────────────────────────────────
            // Layout vertical : 1 card pleine largeur par categorie.
            // Dimensions responsive (clamps doux).
            const float sideMargin = Pct::W(W, 0.020f, 12.0f, 36.0f);
            const float gridLeft = safeX + sideMargin;
            const float availW   = safeW - sideMargin * 2.0f;
            const float cardH    = Pct::H(H, 0.105f, 56.0f, 110.0f);
            const float pad      = Pct::H(H, 0.018f,  8.0f,  20.0f);
            const float topReserve = Pct::H(H, 0.095f, 56.0f, 110.0f);
            float y = (float)ctx.safe.TopY() + topReserve;
            mCardW = availW;
            mCardH = cardH;

            // Marges internes de la card (responsive).
            const float cardPadL = Pct::W(W, 0.018f, 12.0f, 28.0f);
            const float titleY   = Pct::H(H, 0.018f, 10.0f, 22.0f);
            const float subY     = Pct::H(H, 0.058f, 32.0f, 64.0f);
            const float sideBarW = Pct::W(W, 0.005f,  3.0f,  6.0f);
            const float warnW    = Pct::W(W, 0.110f, 70.0f, 130.0f);

            for (int i = 0; i < (int)kCatCount; ++i)
            {
                const CatDesc& d = kCats[i];
                const float bx = gridLeft;
                const float by = y;
                mCardX[i] = bx;
                mCardY[i] = by;

                float a = mEnterAnim - 0.04f * (float)i;
                if (a < 0.0f) a = 0.0f;
                a = EaseOutCubic(math::NkMin(a, 1.0f)) * enterA;

                const bool focused = (i == mFocusIndex);
                math::NkColor bg = d.accent;
                bg.a = static_cast<uint8_t>((focused ? 50 : 14) * a);
                math::NkColor bd = d.accent;
                bd.a = static_cast<uint8_t>((focused && d.enabled ? 220 : 70) * a);

                r.DrawQuad       (bx, by, availW, cardH, bg);
                r.DrawQuadOutline(bx, by, availW, cardH, bd,
                                  focused && d.enabled ? 2.5f * scale : 1.0f);

                // Bande verticale colore a gauche
                math::NkColor side = d.accent;
                side.a = static_cast<uint8_t>((d.enabled ? 220 : 100) * a);
                r.DrawQuad(bx, by, sideBarW, cardH, side);

                // Titre
                math::NkColor titleCol = d.accent;
                titleCol.a = static_cast<uint8_t>((d.enabled ? 255 : 130) * a);
                f.DrawStringScaled(r, FontAtlas::SubtitleSlot, scale,
                             bx + cardPadL,
                             by + titleY,
                             d.title, titleCol);

                // Sous-titre
                math::NkColor subCol = { 255, 255, 255, (uint8_t)((d.enabled ? 160 : 80) * a) };
                f.DrawStringScaled(r, FontAtlas::SmallSlot, scale,
                             bx + cardPadL,
                             by + subY,
                             d.sub, subCol);

                // Marqueur "BIENTOT" si desactive
                if (!d.enabled)
                {
                    math::NkColor warn = { 255, 200, 60, (uint8_t)(180 * a) };
                    f.DrawStringScaled(r, FontAtlas::SmallSlot, scale,
                                 bx + availW - warnW,
                                 by + titleY,
                                 "BIENTOT", warn);
                }

                y += cardH + pad;
            }

            r.End();
        }

        // ─────────────────────────────────────────────────────────────────────
        void OptionsScene::Activate(AppContext& ctx, Category cat)
        {
            switch (cat)
            {
            case Cat_Help:
                logger.Info("[Options] Push RulesScene");
                ctx.scenes->Push(new RulesScene());
                break;
            // Categories pas encore implementees — on log juste.
            case Cat_Audio:
            case Cat_Graphics:
            case Cat_Controls:
            case Cat_Network:
            default:
                logger.Info("[Options] Category activated (TODO): {0}",
                            kCats[(int)cat].title);
                break;
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        void OptionsScene::OnEvent(AppContext& ctx, NkEvent& ev)
        {
            // Clavier : Echap = back, fleches haut/bas = focus, Entree = activate
            if (auto* k = ev.As<NkKeyPressEvent>())
            {
                switch (k->GetKey())
                {
                case NkKey::NK_ESCAPE:
                    ctx.scenes->Pop(); return;
                case NkKey::NK_UP:
                    mFocusIndex = (mFocusIndex - 1 + (int)kCatCount) % (int)kCatCount; return;
                case NkKey::NK_DOWN:
                    mFocusIndex = (mFocusIndex + 1) % (int)kCatCount; return;
                case NkKey::NK_ENTER:
                case NkKey::NK_SPACE:
                    if (kCats[mFocusIndex].enabled)
                        Activate(ctx, (Category)mFocusIndex);
                    return;
                default: break;
                }
                return;
            }

            // Souris : click = activate (si enabled)
            if (auto* mr = ev.As<NkMouseButtonReleaseEvent>())
            {
                // Grace period anti auto-trigger : ignore les releases qui
                // fuitent depuis une scene enfant qui vient d'etre Pop.
                if (mInputArmDelay > 0.0f) return;
                if (mr->GetButton() == NkMouseButton::NK_MB_LEFT)
                {
                    const float px = (float)mr->GetX();
                    const float py = (float)mr->GetY();
                    if (HitTestBack(px, py)) { ctx.scenes->Pop(); return; }
                    const int idx = HitTestCard(px, py);
                    if (idx >= 0)
                    {
                        mFocusIndex = idx;
                        if (kCats[idx].enabled) Activate(ctx, (Category)idx);
                    }
                }
                return;
            }

            // Touch begin : on enregistre l'id du tap. C'est uniquement le
            // TouchEnd de CE touch qui declenchera l'action — ca evite qu'un
            // TouchEnd "fuite" depuis la scene precedente (apres Pop) ne
            // se fasse passer pour un click ici.
            if (auto* tb = ev.As<NkTouchBeginEvent>())
            {
                if (tb->GetNumTouches() > 0)
                {
                    mActiveTouchId = (long long)tb->GetTouch(0).id;
                }
                return;
            }

            // Touch : tap = activate
            if (auto* te = ev.As<NkTouchEndEvent>())
            {
                // Grace period anti auto-trigger : meme logique que mouse.
                if (mInputArmDelay > 0.0f) return;
                if (te->GetNumTouches() > 0)
                {
                    const NkTouchPoint& tp = te->GetTouch(0);
                    // Ignore les ends qui ne correspondent pas a notre begin.
                    if ((long long)tp.id != mActiveTouchId) { mActiveTouchId = -1; return; }
                    mActiveTouchId = -1;

                    if (HitTestBack(tp.clientX, tp.clientY))
                    {
                        ctx.scenes->Pop();
                        return;
                    }
                    const int idx = HitTestCard(tp.clientX, tp.clientY);
                    if (idx >= 0)
                    {
                        mFocusIndex = idx;
                        if (kCats[idx].enabled) Activate(ctx, (Category)idx);
                    }
                }
                return;
            }
        }

    } // namespace pong
} // namespace nkentseu
