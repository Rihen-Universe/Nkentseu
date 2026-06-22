// =============================================================================
// GameplayScene.cpp — CORRIGÉE (bugs 1, 2, 3, 4, 9, 10)
// Toute la logique de jeu et d'animation reste IDENTIQUE à l'original SDL3.
// =============================================================================

#include "GameplayScene.h"
#include "GameOverScene.h"
#include "MainMenuScene.h"
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
#include <cmath>
#include <cstdio>
#include <cstring>

namespace nkentseu { namespace songoo {

    static math::NkColor TC()  { return { 180,  70,  15, 255 }; }
    static math::NkColor OR_() { return { 210, 160,  30, 255 }; }
    static math::NkColor CY()  { return {   0, 200, 255, 255 }; }
    static math::NkColor PAR() { return { 255, 235, 184, 255 }; }

    // ── Retourne le nombre de graines à afficher ──────────────────────────────
    // Pendant l'animation : utilise mVisualPits (snapshot)
    // Après animation   : utilise mBoard (source de vérité)
    int GameplayScene::VisualGrains(int pit) const {
        if (mAnim.active) return mVisualPits[pit];
        return mBoard.GetPitGrains(pit);
    }

    // ── OnEnter ───────────────────────────────────────────────────────────────
    void GameplayScene::OnEnter(AppContext& ctx) {
        mBoard.Init();
        mCurrentPlayer = 0;
        mGameOver      = false;
        mTime = mGlowTime = mEnterAnim = 0.f;
        mHoveredPit    = -1;
        mShowInvalidFb = false;
        mInvalidFbT    = 0.f;
        mAnim          = {};
        mAnim.InitCW();
        std::strncpy(mStatusMsg, "Au tour du Joueur 1", sizeof(mStatusMsg));

        // Synchroniser snapshot visuel avec le board initial
        for (int i = 0; i < 14; i++) mVisualPits[i] = mBoard.GetPitGrains(i);

        // ── CORRECTION BUG 1 : chargement unique dans OnEnter ─────────────────
        char path[128];
        for (int i = 0; i < 16; i++) {
            std::snprintf(path, sizeof(path),
                "Resources/Songo/assets/trou%d.png", i);
            mTrouTex[i].LoadFromFile(path);
            if (!mTrouTex[i].IsValid())
                logger.Warn("[Gameplay] trou{}.png manquant", i);
        }

        mBgTex.LoadFromFile("Resources/Songo/assets/Background.png");
        if (!mBgTex.IsValid())
            logger.Warn("[Gameplay] Background.png manquant");

        mHandTex.LoadFromFile("Resources/Songo/assets/hand.png");
        if (!mHandTex.IsValid())
            logger.Warn("[Gameplay] hand.png manquant");
        // ─────────────────────────────────────────────────────────────────────

        ComputeLayout(ctx);
        if (ctx.audio) ctx.audio->PlayDrum(0.8f);
    }

    void GameplayScene::OnExit(AppContext& /*ctx*/) {
        // CORRECTION BUG 1 : libération propre
        for (int i = 0; i < 16; i++) mTrouTex[i].Shutdown();
        mBgTex.Shutdown();
        mHandTex.Shutdown();
    }

    void GameplayScene::OnPause(AppContext& ctx) {
        if (ctx.audio) ctx.audio->PauseBgMusic();
    }
    void GameplayScene::OnResume(AppContext& ctx) {
        if (ctx.audio) ctx.audio->ResumeBgMusic();
    }

    // ── ComputeLayout ─────────────────────────────────────────────────────────
    // Reproduit EXACTEMENT TrouPosition() de l'original, avec scale responsive.
    // Référence 1600×900 : DX=130, DY=262.5, w=168, h=188, gap=98, steep=w+30
    void GameplayScene::ComputeLayout(AppContext& ctx) {
        mScale = GetUIScale(ctx.viewportW, ctx.viewportH);
        const float DX    = 130.f  * mScale;
        const float DY    = 262.5f * mScale;
        const float w     = 168.f  * mScale;
        const float h     = 188.f  * mScale;
        const float gap   = 98.f   * mScale;
        const float steep = w + 30.f * mScale;

        for (int id = 1; id <= 7; id++) {
            int i = id - 1;
            float x = (id == 1) ? (DX + (id-1)*w - 10.f*mScale)
                                 : (DX + (id-1)*steep);
            mPitGeo[i] = { x + w*0.5f, DY + h*0.5f, w*0.45f };
        }
        for (int id = 8; id <= 14; id++) {
            int i = id - 1;
            float x = (id == 8) ? (DX + (id-8)*w - 10.f*mScale)
                                 : (DX + (id-8)*steep);
            mPitGeo[i] = { x + w*0.5f, DY + h + gap + h*0.5f, w*0.45f };
        }

        float bW = 120.f*mScale, bH = 40.f*mScale;
        mRetourBtnX = ctx.safe.LeftX(10.f*mScale);
        mRetourBtnY = ctx.safe.TopY(10.f*mScale);
        mRetourBtnW = bW; mRetourBtnH = bH;
    }

    int GameplayScene::HitTestPit(float px, float py) const {
        for (int i = 0; i < 14; i++) {
            float dx = px - mPitGeo[i].cx;
            float dy = py - mPitGeo[i].cy;
            if (std::sqrtf(dx*dx + dy*dy) <= mPitGeo[i].radius * 1.15f)
                return i;
        }
        return -1;
    }

    // ── OnEvent ───────────────────────────────────────────────────────────────
    // CORRECTION BUG 3 : Nkentseu fournit les coords déjà en espace logique
    // (pas besoin de SDL_RenderCoordinatesFromWindow).
    void GameplayScene::OnEvent(AppContext& ctx, NkEvent& ev) {
        if (auto* k = ev.As<NkKeyPressEvent>()) {
            if (k->GetKey() == NkKey::NK_ESCAPE) { ctx.scenes->Pop(); return; }
        }

        auto handleTouch = [&](float px, float py) {
            // Bouton retour
            if (px >= mRetourBtnX && px <= mRetourBtnX+mRetourBtnW &&
                py >= mRetourBtnY && py <= mRetourBtnY+mRetourBtnH) {
                ctx.scenes->Pop(); return;
            }
            int pit = HitTestPit(px, py);
            if (pit >= 0) TryMove(ctx, pit);
        };

        if (auto* m = ev.As<NkMouseButtonPressEvent>()) {
            handleTouch(m->GetX(), m->GetY());
        }
        if (auto* m = ev.As<NkMouseMoveEvent>()) {
            mHoveredPit = HitTestPit(m->GetX(), m->GetY());
        }
        if (auto* t = ev.As<NkTouchEndEvent>()) {
            if (t->GetNumTouches() > 0) {
                const NkTouchPoint& tp = t->GetTouch(0);
                handleTouch(tp.clientX, tp.clientY);
            }
        }
    }

    // ── TryMove ───────────────────────────────────────────────────────────────
    // CORRECTION BUG 10 : on ne touche pas mBoard ici.
    // On snapshotte mVisualPits et on lance l'animation.
    // mBoard.ExecuteMove() est appelé dans FinishAnimation().
    void GameplayScene::TryMove(AppContext& ctx, int pitIdx) {
        if (mGameOver || mAnim.active) return;

        // IA bloquée au clic si c'est son tour (mode IA)
        if (ctx.settings &&
            ctx.settings->mode == GameMode::VsAI &&
            mCurrentPlayer == 1) return;

        if (!mBoard.CanPlay(mCurrentPlayer, pitIdx)) {
            bool wrongCamp = (mCurrentPlayer==0 && pitIdx>=7) ||
                             (mCurrentPlayer==1 && pitIdx<7);
            std::strncpy(mStatusMsg,
                wrongCamp ? "Ce n'est pas votre camp !"
                          : "Ce trou est vide !",
                sizeof(mStatusMsg));
            mShowInvalidFb = true;
            mInvalidFbT    = 0.f;
            return;
        }

        // Snapshot visuel AVANT de toucher le board
        for (int i = 0; i < 14; i++) mVisualPits[i] = mBoard.GetPitGrains(i);

        // Lance l'animation — identique à l'original
        mAnim.active      = true;
        mAnim.srcIdx      = pitIdx;
        mAnim.curIdx      = pitIdx;
        mAnim.grainesLeft = mBoard.GetPitGrains(pitIdx);
        mAnim.handX       = mPitGeo[pitIdx].cx;
        mAnim.handY       = mPitGeo[pitIdx].cy;
        mAnim.dropping    = true;
        mAnim.pauseTimer  = 0.3f;

        // Vider visuellement le trou source (pas mBoard)
        mVisualPits[pitIdx] = 0;

        int nxt = mAnim.NextPit(pitIdx);
        if (nxt == pitIdx) nxt = mAnim.NextPit(nxt);
        mAnim.targetX = mPitGeo[nxt].cx;
        mAnim.targetY = mPitGeo[nxt].cy;

        if (ctx.audio) ctx.audio->PlayPickup();
        std::strncpy(mStatusMsg, "Semis en cours...", sizeof(mStatusMsg));
    }

    // ── UpdateAnimation ───────────────────────────────────────────────────────
    // CORRECTION BUG 10 : l'anim met à jour mVisualPits (snapshot), pas mBoard.
    void GameplayScene::UpdateAnimation(float dt, AppContext& ctx) {
        if (!mAnim.active) return;

        if (mAnim.dropping) {
            mAnim.pauseTimer -= dt;
            if (mAnim.pauseTimer > 0.f) return;
            mAnim.dropping = false;

            if (mAnim.grainesLeft == 0) {
                FinishAnimation(ctx);
                return;
            }

            mAnim.curIdx = mAnim.NextPit(mAnim.curIdx);
            if (mAnim.curIdx == mAnim.srcIdx)
                mAnim.curIdx = mAnim.NextPit(mAnim.curIdx);

            mAnim.targetX = mPitGeo[mAnim.curIdx].cx;
            mAnim.targetY = mPitGeo[mAnim.curIdx].cy;
            return;
        }

        float dx   = mAnim.targetX - mAnim.handX;
        float dy   = mAnim.targetY - mAnim.handY;
        float dist = std::sqrtf(dx*dx + dy*dy);
        float step = mAnim.speed * dt;

        if (dist <= step || dist < 1.f) {
            mAnim.handX = mAnim.targetX;
            mAnim.handY = mAnim.targetY;

            // Dépôt visuel (snapshot uniquement)
            mVisualPits[mAnim.curIdx]++;
            mAnim.grainesLeft--;
            mAnim.dropping   = true;
            mAnim.pauseTimer = 0.12f;

            if (ctx.audio) ctx.audio->PlayDeposit();
        } else {
            mAnim.handX += (dx / dist) * step;
            mAnim.handY += (dy / dist) * step;
        }
    }

    // ── FinishAnimation ───────────────────────────────────────────────────────
    // CORRECTION BUG 10 : exécution réelle du coup sur le board ICI SEULEMENT.
    void GameplayScene::FinishAnimation(AppContext& ctx) {
        mAnim.active = false;

        // Exécution réelle : board → calcul des captures, changement de joueur
        auto res = mBoard.ExecuteMove(mCurrentPlayer, mAnim.srcIdx);

        // Resynchroniser le snapshot visuel avec le board (après captures)
        for (int i = 0; i < 14; i++) mVisualPits[i] = mBoard.GetPitGrains(i);

        if (res.gameOver) {
            mGameOver = true;
            int s0 = mBoard.GetScore(0), s1 = mBoard.GetScore(1);
            ctx.scenes->Push(new GameOverScene(res.winner, s0, s1));
            return;
        }

        SwitchPlayer(ctx);
    }

    void GameplayScene::SwitchPlayer(AppContext& ctx) {
        mCurrentPlayer = mBoard.GetCurrentPlayer();
        std::snprintf(mStatusMsg, sizeof(mStatusMsg),
            "Au tour du Joueur %d", mCurrentPlayer + 1);
        if (ctx.audio) ctx.audio->PlayDrum();

        // IA
        if (ctx.settings &&
            ctx.settings->mode == GameMode::VsAI &&
            mCurrentPlayer == 1) {
            TriggerIAMove(ctx);
        }
    }

    void GameplayScene::TriggerIAMove(AppContext& ctx) {
        // IA simpliste : premier trou non vide du camp 1
        for (int i = 7; i <= 13; i++) {
            if (mBoard.GetPitGrains(i) > 0) {
                std::snprintf(mStatusMsg, sizeof(mStatusMsg), "L'IA reflechit...");
                TryMove(ctx, i);
                return;
            }
        }
    }

    // ── OnUpdate ──────────────────────────────────────────────────────────────
    // CORRECTION BUG 4 : glowTime incrémenté UNE SEULE FOIS ici
    void GameplayScene::OnUpdate(AppContext& ctx, float dt) {
        mTime      += dt;
        mGlowTime  += dt;   // une seule incrémentation
        mEnterAnim  = math::NkMin(mEnterAnim + dt * 2.f, 1.f);

        if (mShowInvalidFb) {
            mInvalidFbT += dt;
            if (mInvalidFbT > 0.6f) mShowInvalidFb = false;
        }

        UpdateAnimation(dt, ctx);
        ComputeLayout(ctx);
    }

    // ── OnRender ──────────────────────────────────────────────────────────────
    void GameplayScene::OnRender(AppContext& ctx) {
        GLRenderer2D& r = *ctx.renderer;
        const int W = ctx.viewportW, H = ctx.viewportH;

        r.Clear(0.04f, 0.02f, 0.01f, 1.f);
        r.Begin(W, H);

        DrawBackground(ctx);
        DrawPlayerGlow(ctx);
        DrawBoard(ctx);
        DrawHand(ctx);
        DrawUI(ctx);

        // Feedback visuel erreur (teinte rouge brève)
        if (mShowInvalidFb) {
            float a = 1.f - mInvalidFbT / 0.6f;
            float DX = 130.f*mScale, DY = 262.5f*mScale;
            float w  = 168.f*mScale, h  = 188.f*mScale, gap = 98.f*mScale;
            r.DrawQuad(DX-10.f, DY-10.f,
                       7.f*(w+30.f*mScale)+20.f, 2.f*h+gap+20.f,
                       { 255, 0, 0, (uint8_t)(70 * a) });
        }

        r.End();
    }

    // ── CORRECTION BUG 1 : fond pré-chargé, pas de reload chaque frame ────────
    void GameplayScene::DrawBackground(AppContext& ctx) {
        GLRenderer2D& r = *ctx.renderer;
        float DX = 130.f*mScale, DY = 262.5f*mScale;
        float w  = 168.f*mScale, h  = 188.f*mScale, gap = 98.f*mScale;
        float bW = 7.f*(w+30.f*mScale), bH = 2.f*h + gap;

        if (mBgTex.IsValid()) {
            r.BindTexture(mBgTex.Id());
            r.DrawTexturedQuadRGBA(DX-20.f, DY-20.f, bW+40.f, bH+40.f,
                                   0.f, 0.f, 1.f, 1.f, { 255, 255, 255, 255 });
        } else {
            r.DrawQuad(DX-20.f, DY-20.f, bW+40.f, bH+40.f, { 60, 35, 10, 200 });
        }
    }

    void GameplayScene::DrawPlayerGlow(AppContext& ctx) {
        GLRenderer2D& r = *ctx.renderer;
        // CORRECTION BUG 4 : utilise mGlowTime incrémenté une seule fois
        float pulse = 0.55f + 0.45f * math::NkSin(mGlowTime * 3.5f);
        float DX    = 130.f*mScale - 10.f;
        float w     = 168.f*mScale, h = 188.f*mScale, gap = 98.f*mScale;
        float rowW  = 7.f*(w+30.f*mScale) + 20.f;

        if (mCurrentPlayer == 0) {
            uint8_t a  = (uint8_t)(60.f * pulse);
            uint8_t ab = (uint8_t)(120.f * pulse);
            r.DrawQuad(DX-10.f, 215.f*mScale, rowW+20.f, 270.f*mScale,
                       { 255, 185, 30, a });
            r.DrawQuadOutline(DX-10.f, 215.f*mScale, rowW+20.f, 270.f*mScale,
                              { 255, 200, 50, ab }, 2.f);
        } else {
            float yBot = 215.f*mScale + h + gap;
            uint8_t a  = (uint8_t)(60.f * pulse);
            uint8_t ab = (uint8_t)(120.f * pulse);
            r.DrawQuad(DX-10.f, yBot-10.f, rowW+20.f, 270.f*mScale,
                       { 60, 160, 255, a });
            r.DrawQuadOutline(DX-10.f, yBot-10.f, rowW+20.f, 270.f*mScale,
                              { 100, 180, 255, ab }, 2.f);
        }
    }

    void GameplayScene::DrawBoard(AppContext& ctx) {
        for (int i = 0; i < 14; i++) DrawPit(ctx, i);
    }

    void GameplayScene::DrawPit(AppContext& ctx, int i) {
        GLRenderer2D& r = *ctx.renderer;
        FontAtlas&    f = *ctx.font;
        const PitGeo& g = mPitGeo[i];

        // Nombre de graines à afficher (visuel découplé du board logique)
        int grains = VisualGrains(i);
        int texIdx = (grains > 15) ? 15 : grains;

        if (mTrouTex[texIdx].IsValid()) {
            float hw = g.radius, hh = g.radius;
            r.BindTexture(mTrouTex[texIdx].Id());
            r.DrawTexturedQuadRGBA(g.cx-hw, g.cy-hh, hw*2.f, hh*2.f,
                                   0.f, 0.f, 1.f, 1.f, { 255, 255, 255, 255 });
        } else {
            bool isP1 = (i < 7);
            math::NkColor col = isP1 ? TC() : CY();
            r.DrawCircle(g.cx, g.cy, g.radius, AlphaF(col, 0.35f), 24);
            r.DrawCircleOutline(g.cx, g.cy, g.radius+1.5f,
                AlphaF(col, mHoveredPit==i ? 0.9f : 0.6f), 2.f, 24);
            char buf[8]; std::snprintf(buf, sizeof(buf), "%d", grains);
            f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, mScale,
                g.cx, g.cy-8.f*mScale, buf, PAR());
        }

        if (mHoveredPit == i) {
            float p = 0.5f + 0.5f * math::NkSin(mTime * 3.5f);
            r.DrawCircleOutline(g.cx, g.cy, g.radius+4.f,
                { 200, 150, 50, (uint8_t)(150*p) }, 3.f, 24);
        }
    }

    // ── CORRECTION BUG 2 + BUG 9 ─────────────────────────────────────────────
    // La police TTF n'est plus rechargée chaque frame.
    // On utilise FontAtlas natif Nkentseu (cross-platform, pré-chargé).
    void GameplayScene::DrawHand(AppContext& ctx) {
        if (!mAnim.active) return;
        GLRenderer2D& r = *ctx.renderer;
        FontAtlas&    f = *ctx.font;

        float hw = 84.f*mScale, hh = 94.f*mScale;
        if (mHandTex.IsValid()) {
            r.BindTexture(mHandTex.Id());
            r.DrawTexturedQuadRGBA(mAnim.handX-hw, mAnim.handY-hh,
                                   hw*2.f, hh*2.f,
                                   0.f, 0.f, 1.f, 1.f, { 255, 255, 255, 255 });
        } else {
            r.DrawCircle(mAnim.handX, mAnim.handY,
                         20.f*mScale, { 230, 120, 20, 220 }, 16);
        }

        // CORRECTION BUG 2 : plus de TTF_OpenFont ici — FontAtlas natif
        if (mAnim.grainesLeft > 0) {
            char buf[8]; std::snprintf(buf, sizeof(buf), "%d", mAnim.grainesLeft);
            f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, mScale,
                mAnim.handX, mAnim.handY - 52.f*mScale, buf,
                { 255, 255, 200, 255 });
        }
    }

    void GameplayScene::DrawUI(AppContext& ctx) {
        GLRenderer2D& r = *ctx.renderer;
        FontAtlas&    f = *ctx.font;
        const SafeArea& sa = ctx.safe;
        const float cx = sa.SafeCX();
        bool p1Active = (mCurrentPlayer == 0);

        // ── En-tête ──────────────────────────────────────────────────────────
        float headerH = 52.f*mScale;
        r.DrawQuad(sa.LeftX(), sa.TopY(), sa.SafeW(), headerH,
                   { 20, 12, 4, 210 });

        // Bouton Retour
        r.DrawQuad(mRetourBtnX, mRetourBtnY, mRetourBtnW, mRetourBtnH,
                   { 80, 35, 5, (uint8_t)(200*mEnterAnim) });
        r.DrawQuadOutline(mRetourBtnX, mRetourBtnY, mRetourBtnW, mRetourBtnH,
                          OR_(), 1.5f);
        f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, 0.85f*mScale,
            mRetourBtnX+mRetourBtnW*0.5f, mRetourBtnY+mRetourBtnH*0.18f,
            "RETOUR", { 255, 235, 180, (uint8_t)(200*mEnterAnim) });

        // ── Scores ────────────────────────────────────────────────────────────
        char s1[32], s2[32];
        std::snprintf(s1, sizeof(s1), "J1 : %d", mBoard.GetScore(0));
        std::snprintf(s2, sizeof(s2), "J2 : %d", mBoard.GetScore(1));

        f.DrawStringShadowCenteredScaled(r, FontAtlas::SubtitleSlot, 1.1f*mScale,
            cx-220.f*mScale, sa.TopY(10.f*mScale), s1,
            p1Active ? math::NkColor{250,204,38,255} : math::NkColor{160,120,60,180},
            { 40, 15, 2, 120 }, 2);
        f.DrawStringShadowCenteredScaled(r, FontAtlas::SubtitleSlot, 1.1f*mScale,
            cx+220.f*mScale, sa.TopY(10.f*mScale), s2,
            !p1Active ? math::NkColor{140,214,255,255} : math::NkColor{80,120,160,180},
            { 5, 20, 40, 120 }, 2);

        // ── Statut bas ────────────────────────────────────────────────────────
        float statY = sa.BottomY(48.f*mScale);
        r.DrawQuad(sa.LeftX(), statY, sa.SafeW(), 44.f*mScale, { 10, 6, 2, 180 });
        f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, 0.9f*mScale,
            cx, statY+8.f*mScale, mStatusMsg,
            p1Active ? math::NkColor{250,214,76,255}
                     : math::NkColor{153,219,255,255});

        // Kente sur la barre statut
        float segW = sa.SafeW() / 14.f;
        math::NkColor kente[3] = { TC(), OR_(), { 30,100,40,200 } };
        for (int k = 0; k < 14; k++)
            r.DrawQuad(sa.LeftX()+k*segW, statY, segW+1.f, 5.f*mScale, kente[k%3]);
    }

}} // namespace nkentseu::songoo
