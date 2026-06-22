// =============================================================================
// GameOverScene.cpp — Écran de fin de partie Songo'o
// Logique IDENTIQUE à l'original (scores, vainqueur, Rejouer / Menu)
// =============================================================================

#include "GameOverScene.h"
#include "GameplayScene.h"
#include "MainMenuScene.h"
#include "Songoo/Render/GLRenderer2D.h"
#include "Songoo/Render/FontAtlas.h"
#include "Songoo/UI/Theme.h"
#include "Songoo/UI/UIScale.h"
#include "Songoo/UI/SceneManager.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include "NKMath/NkFunctions.h"
#include <cstdio>

namespace nkentseu { namespace songoo {

    static math::NkColor OR_() { return { 210, 160, 30, 255 }; }
    static math::NkColor TC()  { return { 180, 70,  15, 255 }; }
    static math::NkColor PAR() { return { 255, 235, 184, 255 }; }

    void GameOverScene::OnEnter(AppContext& ctx) {
        mTime = 0.f;
        if (ctx.audio) ctx.audio->PlayScore();
    }

    void GameOverScene::OnUpdate(AppContext& ctx, float dt) {
        mTime += dt;
        const float scale = GetUIScale(ctx.viewportW, ctx.viewportH);
        const float cx = ctx.safe.SafeCX(), cy = ctx.safe.SafeCY();
        float bW = 220.f * scale, bH = 50.f * scale;
        mBtnReplayX = cx - bW - 10.f * scale; mBtnReplayY = cy + 80.f * scale;
        mBtnReplayW = bW; mBtnReplayH = bH;
        mBtnMenuX   = cx + 10.f * scale;      mBtnMenuY = cy + 80.f * scale;
        mBtnMenuW   = bW; mBtnMenuH = bH;
    }

    void GameOverScene::OnEvent(AppContext& ctx, NkEvent& ev) {
        auto doHit = [&](float px, float py) {
            if (px>=mBtnReplayX && px<=mBtnReplayX+mBtnReplayW &&
                py>=mBtnReplayY && py<=mBtnReplayY+mBtnReplayH) {
                ctx.scenes->Replace(new GameplayScene()); return;
            }
            if (px>=mBtnMenuX && px<=mBtnMenuX+mBtnMenuW &&
                py>=mBtnMenuY && py<=mBtnMenuY+mBtnMenuH) {
                ctx.scenes->PopToRoot(); return;
            }
        };
        if (auto* m = ev.As<NkMouseButtonPressEvent>()) doHit(m->GetX(), m->GetY());
        if (auto* t = ev.As<NkTouchEndEvent>())
            if (t->GetNumTouches() > 0) {
                const NkTouchPoint& tp = t->GetTouch(0);
                doHit(tp.clientX, tp.clientY);
            }
        if (auto* k = ev.As<NkKeyPressEvent>()) {
            if (k->GetKey() == NkKey::NK_ESCAPE) ctx.scenes->PopToRoot();
        }
    }

    void GameOverScene::OnRender(AppContext& ctx) {
        GLRenderer2D& r = *ctx.renderer;
        FontAtlas&    f = *ctx.font;
        const int W = ctx.viewportW, H = ctx.viewportH;
        const float cx = ctx.safe.SafeCX(), cy = ctx.safe.SafeCY();
        const float scale = GetUIScale(W, H);

        r.Clear(0.04f, 0.02f, 0.01f, 1.f);
        r.Begin(W, H);

        // Fondu d'entrée
        float alpha = math::NkMin(mTime / 0.4f, 1.f);

        // Panneau central
        float panW = 560.f * scale, panH = 380.f * scale;
        float panX = cx - panW*0.5f, panY = cy - panH*0.5f;
        r.DrawQuad(panX, panY, panW, panH, { 20, 12, 5, (uint8_t)(220 * alpha) });
        r.DrawQuadOutline(panX, panY, panW, panH, AlphaF(OR_(), alpha), 2.5f);

        // Bandes kente haut/bas du panneau
        math::NkColor kente[3] = { { 180, 70, 15, (uint8_t)(200*alpha) },
                                   { 210, 160, 30, (uint8_t)(200*alpha) },
                                   {  30, 100, 40, (uint8_t)(200*alpha) } };
        float segW = panW / 21.f;
        for (int k = 0; k < 21; k++) {
            r.DrawQuad(panX + k*segW, panY, segW+1.f, 8.f*scale, kente[k%3]);
            r.DrawQuad(panX + k*segW, panY + panH - 8.f*scale, segW+1.f, 8.f*scale, kente[k%3]);
        }

        // Titre
        float pulse = 0.85f + 0.15f * math::NkSin(mTime * 2.f);
        f.DrawStringShadowCenteredScaled(r, FontAtlas::DisplaySlot, 1.3f * scale * pulse,
            cx, panY + 30.f * scale, "FIN DE PARTIE",
            { (uint8_t)(247*pulse), (uint8_t)(184*pulse), 46, (uint8_t)(255*alpha) },
            { 80, 30, 5, 120 }, 3);

        // Séparateur
        r.DrawQuad(panX + 30.f*scale, panY + 100.f*scale,
                   panW - 60.f*scale, 2.f, AlphaF(OR_(), alpha * 0.6f));

        // Scores
        char b1[32], b2[32];
        snprintf(b1, sizeof(b1), "Joueur 1 : %d graines", mS0);
        snprintf(b2, sizeof(b2), "Joueur 2 : %d graines", mS1);
        f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
            cx, panY + 120.f * scale, b1,
            { 235, 204, 133, (uint8_t)(255*alpha) });
        f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
            cx, panY + 160.f * scale, b2,
            { 179, 214, 247, (uint8_t)(255*alpha) });

        // Résultat
        const char* result;
        math::NkColor rcol;
        if      (mWinner == 0) { result = "VICTOIRE JOUEUR 1 !"; rcol = { 247, 204, 51, (uint8_t)(255*alpha) }; }
        else if (mWinner == 1) { result = "VICTOIRE JOUEUR 2 !"; rcol = { 140, 214, 255, (uint8_t)(255*alpha) }; }
        else                   { result = "EGALITE !";            rcol = { 247, 224, 92, (uint8_t)(255*alpha) }; }

        f.DrawStringShadowCenteredScaled(r, FontAtlas::HeadlineSlot, 1.1f * scale,
            cx, panY + 200.f * scale, result,
            rcol, { 30, 15, 3, 100 }, 2);

        // Bouton Rejouer
        r.DrawQuad(mBtnReplayX, mBtnReplayY, mBtnReplayW, mBtnReplayH,
                   { (uint8_t)(100*alpha), (uint8_t)(40*alpha), (uint8_t)(5*alpha), (uint8_t)(200*alpha) });
        r.DrawQuadOutline(mBtnReplayX, mBtnReplayY, mBtnReplayW, mBtnReplayH,
                          AlphaF(OR_(), alpha), 2.f);
        f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
            mBtnReplayX + mBtnReplayW*0.5f, mBtnReplayY + mBtnReplayH*0.18f,
            "REJOUER", AlphaF(PAR(), alpha));

        // Bouton Menu
        r.DrawQuad(mBtnMenuX, mBtnMenuY, mBtnMenuW, mBtnMenuH,
                   { (uint8_t)(30*alpha), (uint8_t)(20*alpha), (uint8_t)(10*alpha), (uint8_t)(200*alpha) });
        r.DrawQuadOutline(mBtnMenuX, mBtnMenuY, mBtnMenuW, mBtnMenuH,
                          AlphaF(TC(), alpha), 2.f);
        f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
            mBtnMenuX + mBtnMenuW*0.5f, mBtnMenuY + mBtnMenuH*0.18f,
            "MENU PRINCIPAL", AlphaF(PAR(), alpha));

        r.End();
    }

}} // namespace nkentseu::songoo
