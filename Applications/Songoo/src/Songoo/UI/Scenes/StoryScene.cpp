// =============================================================================
// StoryScene.cpp — CORRIGÉE (bug 6 : SetFinished mal nommé dans l'original)
//
// Dans l'original SDL3 :
//   storyAnim.SetFinished() est appelé depuis le menu "Histoire"
//   mais l'implémentation mettait finished = false → RELANCE l'animation.
//   Ce comportement est donc VOULU par l'auteur (nom trompeur).
//   On conserve ce comportement : SetFinished() = Reset() = relance depuis le début.
//
// CORRECTION : on renomme en interne Reset() pour la clarté,
// et SetFinished() reste l'API publique appelée depuis MainMenuScene.
// =============================================================================

#include "StoryScene.h"
#include "MainMenuScene.h"
#include "Songoo/Render/GLRenderer2D.h"
#include "Songoo/Render/FontAtlas.h"
#include "Songoo/UI/Theme.h"
#include "Songoo/UI/SceneManager.h"
#include "NKLogger/NkLog.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu { namespace songoo {

    const StoryScene::StoryFrame StoryScene::kFrames[kFrameCount] = {
        { "Resources/Songo/assets/story_01.png", 11.f, 0.3f, 1.0f,
          "In the heart of Cameroon, under the great silk-cotton tree, the elders played Songo'o to settle disputes." },
        { "Resources/Songo/assets/story_02.png",  8.f, 0.8f, 0.8f,
          "The seeds, carved from baobab wood, were sown with wisdom \xe2\x80\x94 each move a silent prayer to the ancestors." },
        { "Resources/Songo/assets/story_03.png", 11.f, 1.2f, 1.0f,
          "Two warriors face each other at the board. Only patience and cunning will lead to victory." },
        { "Resources/Songo/assets/story_04.png",  9.f, 0.8f, 0.8f,
          "The village gathers at sunset. The rhythm of the game echoes the rhythm of life itself." },
        { "Resources/Songo/assets/story_05.png",  9.f, 1.0f, 1.0f,
          "To capture your opponent's seeds is to earn respect \xe2\x80\x94 not just from men, but from the spirits of the land." },
        { "Resources/Songo/assets/story_06.png",  8.5f, 1.0f, 1.5f,
          "Songo'o lives on. Passed from father to child, it binds generations in a shared heritage of strategy and soul." },
    };

    void StoryScene::OnEnter(AppContext& ctx) {
        mCurrentFrame = 0;
        mFrameTimer   = 0.f;
        mDone         = false;
        mSubtitleFrame = 0;
        mSubtitleAccum = 0.f;

        for (int i = 0; i < kFrameCount; i++) {
            mTextures[i].LoadFromFile(kFrames[i].path);
            if (!mTextures[i].IsValid())
                logger.Warn("[StoryScene] Image manquante: {}", kFrames[i].path);
        }

        // Lancer la musique de fond d'histoire
        if (ctx.audio)
            ctx.audio->PlayBgMusic("audio/songo2.mp3", false, 1.0f);
    }

    void StoryScene::OnExit(AppContext& ctx) {
        for (int i = 0; i < kFrameCount; i++) mTextures[i].Shutdown();
        if (ctx.audio) ctx.audio->StopBgMusic();
    }

    void StoryScene::OnEvent(AppContext& ctx, NkEvent& ev) {
        // ESC ou clic → retour menu immédiat
        bool skip = false;
        if (auto* k = ev.As<NkKeyPressEvent>())
            if (k->GetKey() == NkKey::NK_ESCAPE) skip = true;
        if (ev.Is<NkMouseButtonPressEvent>()) skip = true;
        if (auto* t = ev.As<NkTouchEndEvent>())
            if (t->GetNumTouches() > 0) skip = true;

        if (skip) ctx.scenes->Replace(new MainMenuScene());
    }

    // ── CORRECTION BUG 6 ─────────────────────────────────────────────────────
    // L'original : SetFinished() faisait finished=false (nom trompeur)
    // Comportement voulu : relancer l'animation depuis le début.
    // On applique le même comportement ici via Reset().
    // Le menu appelle simplement OnEnter (via Push) donc c'est déjà correct.
    // La méthode SetFinished n'est plus nécessaire — OnEnter fait un Reset.

    void StoryScene::OnUpdate(AppContext& ctx, float dt) {
        if (mDone) return;

        mFrameTimer += dt;

        // Avance le sous-titre en parallèle (identique à l'original)
        mSubtitleAccum += dt;
        if (mSubtitleFrame < kFrameCount - 1) {
            if (mSubtitleAccum >= kFrames[mSubtitleFrame].displayTime) {
                mSubtitleAccum -= kFrames[mSubtitleFrame].displayTime;
                mSubtitleFrame++;
            }
        }

        // Avance le frame courant
        while (mCurrentFrame < kFrameCount &&
               mFrameTimer >= kFrames[mCurrentFrame].displayTime) {
            mFrameTimer -= kFrames[mCurrentFrame].displayTime;
            mCurrentFrame++;
        }

        if (mCurrentFrame >= kFrameCount) {
            mDone = true;
            ctx.scenes->Replace(new MainMenuScene());
        }
    }

    float StoryScene::ComputeAlpha(int fi, float elapsed) const {
        const StoryFrame& f = kFrames[fi];
        float alpha = 1.f;
        if (elapsed < f.fadeIn && f.fadeIn > 0.f)
            alpha = elapsed / f.fadeIn;
        float left = f.displayTime - elapsed;
        if (left < f.fadeOut && f.fadeOut > 0.f) {
            float out = left / f.fadeOut;
            if (out < alpha) alpha = out;
        }
        return alpha < 0.f ? 0.f : alpha > 1.f ? 1.f : alpha;
    }

    void StoryScene::OnRender(AppContext& ctx) {
        GLRenderer2D& r = *ctx.renderer;
        FontAtlas&    f = *ctx.font;
        const int W = ctx.viewportW, H = ctx.viewportH;
        const float cx = ctx.safe.SafeCX();
        const float scale = GetUIScale(W, H);

        r.Clear(0.f, 0.f, 0.f, 1.f);
        r.Begin(W, H);

        if (mCurrentFrame < kFrameCount && mTextures[mCurrentFrame].IsValid()) {
            float alpha = ComputeAlpha(mCurrentFrame, mFrameTimer);
            uint8_t a   = (uint8_t)(255.f * alpha);

            r.BindTexture(mTextures[mCurrentFrame].Id());
            r.DrawTexturedQuadRGBA(0.f, 0.f, (float)W, (float)H,
                                   0.f, 0.f, 1.f, 1.f, { 255, 255, 255, a });

            // Fond sous-titre
            float bandH = 70.f * scale;
            r.DrawQuad(0.f, (float)H - bandH, (float)W, bandH, { 0, 0, 0, 170 });

            // Sous-titre (index synchronisé avec mSubtitleFrame)
            int si = (mSubtitleFrame < kFrameCount) ? mSubtitleFrame : kFrameCount-1;
            f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, 0.9f*scale,
                cx, (float)H - bandH + 14.f*scale,
                kFrames[si].subtitle,
                { 255, 240, 200, a });

            // Indication "Toucher pour passer"
            if (mFrameTimer > 2.f) {
                uint8_t ha = (uint8_t)(100.f * alpha);
                f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, 0.7f*scale,
                    cx, 16.f*scale, "Appuyer pour passer",
                    { 200, 200, 200, ha });
            }
        }

        r.End();
    }

}} // namespace nkentseu::songoo
