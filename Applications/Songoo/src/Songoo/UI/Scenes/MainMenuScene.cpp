// =============================================================================
// MainMenuScene.cpp — Menu principal Songo'o
// Palette : terres cuites, or chaud, vert forêt, brun cacao
// =============================================================================

#include "MainMenuScene.h"
#include "GameplayScene.h"
#include "StoryScene.h"
#include "OptionsScene.h"
#include "CreditsScene.h"
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
#include "NKMath/NkFunctions.h"
#include <cstring>

namespace nkentseu { namespace songoo {

    // ── Palette africaine ─────────────────────────────────────────────────────
    static math::NkColor TC()  { return { 180,  70,  15, 255 }; }  // Terre cuite
    static math::NkColor OR_() { return { 210, 160,  30, 255 }; }  // Or chaud
    static math::NkColor VF()  { return {  30, 100,  40, 255 }; }  // Vert forêt
    static math::NkColor BC()  { return {  90,  45,  10, 255 }; }  // Brun cacao
    static math::NkColor PAR() { return { 255, 235, 184, 255 }; }  // Parchemin

    static const struct { const char* label; const char* desc; }
    kItems[MainMenuScene::kItemCount] = {
        { "NOUVELLE PARTIE",  "Lancer une partie" },
        { "HISTOIRE",         "Découvrir Songo'o" },
        { "OPTIONS",          "Paramètres audio"  },
        { "CRÉDITS",          "L'équipe"          },
        { "QUITTER",          "Fermer"            },
    };

    void MainMenuScene::OnEnter(AppContext& /*ctx*/) {
        mTime = 0.f; mFocusIndex = 0; mEnterAnim = 0.f; mGracePeriod = 0.2f;
    }

    void MainMenuScene::OnResumedFromChild(AppContext& /*ctx*/) {
        mGracePeriod = 0.25f;
    }

    void MainMenuScene::OnUpdate(AppContext& ctx, float dt) {
        mTime      += dt;
        mEnterAnim  = math::NkMin(mEnterAnim + dt * 2.f, 1.f);
        if (mGracePeriod > 0.f) mGracePeriod -= dt;

        // Démarrer la musique de fond
        if (ctx.audio && !ctx.audio->IsBgMusicPlaying())
            ctx.audio->PlayBgMusic("audio/background.mp3", true, 0.10f);
    }

    void MainMenuScene::ComputeLayout(AppContext& ctx, float scale) {
        const float W = (float)ctx.viewportW;
        const float H = (float)ctx.viewportH;
        const SafeArea& sa = ctx.safe;

        mCardItemH   = 52.f * scale;
        mCardItemGap = 12.f * scale;
        mCardListW   = 340.f * scale;
        mCardListX   = sa.SafeCX() - mCardListW * 0.5f;

        float totalH = kItemCount * mCardItemH + (kItemCount - 1) * mCardItemGap;
        float startY = sa.SafeCY() + 40.f * scale;

        for (int i = 0; i < kItemCount; ++i)
            mCardItemYs[i] = startY + i * (mCardItemH + mCardItemGap);

        (void)W; (void)H;
    }

    void MainMenuScene::OnEvent(AppContext& ctx, NkEvent& ev) {
        if (mGracePeriod > 0.f) return;

        if (auto* k = ev.As<NkKeyPressEvent>()) {
            switch (k->GetKey()) {
            case NkKey::NK_UP:
                mFocusIndex = (mFocusIndex - 1 + kItemCount) % kItemCount; break;
            case NkKey::NK_DOWN:
                mFocusIndex = (mFocusIndex + 1) % kItemCount; break;
            case NkKey::NK_ENTER: case NkKey::NK_SPACE:
                ActivateItem(ctx, (ItemId)mFocusIndex); break;
            default: break;
            }
            return;
        }
        if (auto* m = ev.As<NkMouseButtonPressEvent>()) {
            int idx = HitTestItem(m->GetX(), m->GetY());
            if (idx >= 0) { mFocusIndex = idx; ActivateItem(ctx, (ItemId)idx); }
            return;
        }
        if (auto* m = ev.As<NkMouseMoveEvent>()) {
            int idx = HitTestItem(m->GetX(), m->GetY());
            if (idx >= 0) mFocusIndex = idx;
            return;
        }
        if (auto* t = ev.As<NkTouchEndEvent>()) {
            if (t->GetNumTouches() > 0) {
                const NkTouchPoint& tp = t->GetTouch(0);
                int idx = HitTestItem(tp.clientX, tp.clientY);
                if (idx >= 0) { mFocusIndex = idx; ActivateItem(ctx, (ItemId)idx); }
            }
            return;
        }
    }

    int MainMenuScene::HitTestItem(float px, float py) const {
        if (px < mCardListX || px > mCardListX + mCardListW) return -1;
        for (int i = 0; i < kItemCount; ++i) {
            if (py >= mCardItemYs[i] && py <= mCardItemYs[i] + mCardItemH)
                return i;
        }
        return -1;
    }

    void MainMenuScene::ActivateItem(AppContext& ctx, ItemId item) {
        if (ctx.audio) ctx.audio->PlayPickup(0.5f);
        switch (item) {
        case Item_Play:
            ctx.scenes->Push(new GameplayScene());
            break;
        case Item_Story:
            ctx.scenes->Push(new StoryScene());
            break;
        case Item_Options:
            ctx.scenes->Push(new OptionsScene());
            break;
        case Item_Credits:
            ctx.scenes->Push(new CreditsScene());
            break;
        case Item_Quit:
            if (ctx.quitRequested) *ctx.quitRequested = true;
            break;
        }
    }

    // ── OnRender ──────────────────────────────────────────────────────────────
    void MainMenuScene::OnRender(AppContext& ctx) {
        GLRenderer2D& r = *ctx.renderer;
        FontAtlas&    f = *ctx.font;
        const int W = ctx.viewportW, H = ctx.viewportH;
        const SafeArea& sa = ctx.safe;
        const float scale = GetUIScale(W, H);

        ComputeLayout(ctx, scale);

        // ── Fond dégradé brun-noir ─────────────────────────────────────────
        r.Clear(0.04f, 0.02f, 0.01f, 1.f);
        r.Begin(W, H);

        // Bandeau gauche kente (vert / or / terre cuite)
        math::NkColor kente[3] = { VF(), OR_(), TC() };
        float segH = (float)H / 21.f;
        for (int k = 0; k < 21; k++)
            r.DrawQuad(0.f, k * segH, 6.f * scale, segH + 1.f, kente[k % 3]);
        for (int k = 0; k < 21; k++)
            r.DrawQuad((float)W - 6.f * scale, k * segH, 6.f * scale, segH + 1.f, kente[k % 3]);

        // ── Titre ─────────────────────────────────────────────────────────────
        const float pulse  = 0.85f + 0.15f * math::NkSin(mTime * 2.2f);
        const float titleY = sa.TopY(18.f * scale);

        f.DrawStringShadowCenteredScaled(r, FontAtlas::DisplaySlot, 1.5f * scale * pulse,
            sa.SafeCX(), titleY,
            "SONGO'O",
            { (uint8_t)(210 * pulse), (uint8_t)(160 * pulse), 30, 255 },
            { 80, 30, 5, 160 }, 3);

        f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, 0.75f * scale,
            sa.SafeCX(), titleY + 68.f * scale,
            "~ Jeu Traditionnel Camerounais ~",
            { 184, 133, 56, (uint8_t)(200 * mEnterAnim) });

        // Séparateur or
        r.DrawQuad(mCardListX, titleY + 95.f * scale, mCardListW, 2.f,
                   AlphaF(OR_(), 0.5f));

        // ── Cards du menu ─────────────────────────────────────────────────────
        for (int i = 0; i < kItemCount; ++i) {
            bool focused  = (i == mFocusIndex);
            float slideX  = (1.f - mEnterAnim) * -60.f * scale;
            float cardX   = mCardListX + slideX;
            float cardY   = mCardItemYs[i];
            float cardW   = mCardListW;
            float cardH   = mCardItemH;

            // Fond card
            math::NkColor bgC  = focused ? AlphaF(TC(), 0.80f)
                                         : AlphaF(BC(), 0.55f);
            math::NkColor brdC = focused ? AlphaF(OR_(), 0.90f)
                                         : AlphaF(TC(), 0.40f);

            r.DrawQuad(cardX, cardY, cardW, cardH, bgC);
            r.DrawQuadOutline(cardX, cardY, cardW, cardH, brdC, focused ? 2.5f : 1.5f);

            // Texte
            math::NkColor txtC = focused ? PAR() : AlphaF(PAR(), 0.70f);
            float tscale = focused ? 1.05f * scale : 0.95f * scale;
            f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, tscale,
                cardX + cardW * 0.5f, cardY + cardH * 0.22f,
                kItems[i].label, txtC);

            // Description
            if (focused) {
                f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, 0.75f * scale,
                    cardX + cardW * 0.5f, cardY + cardH * 0.62f,
                    kItems[i].desc,
                    { 220, 190, 120, 180 });
            }
        }

        // ── Bandeau bas ───────────────────────────────────────────────────────
        float botY = sa.BottomY(10.f * scale);
        r.DrawQuad(mCardListX, botY, mCardListW, 2.f, AlphaF(OR_(), 0.3f));
        f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, 0.65f * scale,
            sa.SafeCX(), botY + 6.f * scale,
            "© 2026 RIHEN UNIVERSE  —  NGIATE KAMNANG INGRID  —  ENSPY",
            { 140, 100, 50, 130 });

        r.End();
    }

}} // namespace nkentseu::songoo
