// =============================================================================
// OptionsScene.cpp — Options audio Songo'o
// Logique IDENTIQUE à l'original (musicVolume, sfxVolume, toggles)
// =============================================================================

#include "OptionsScene.h"
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
    static math::NkColor TC()  { return { 180, 70, 15, 255 };  }
    static math::NkColor VF()  { return {  30, 100, 40, 255 }; }
    static math::NkColor PAR() { return { 255, 235, 184, 255 }; }

    void OptionsScene::OnEnter(AppContext& /*ctx*/) { mTime = 0.f; }

    void OptionsScene::OnUpdate(AppContext& ctx, float dt) {
        mTime += dt;
        const float scale = GetUIScale(ctx.viewportW, ctx.viewportH);
        const float cx = ctx.safe.SafeCX(), cy = ctx.safe.SafeCY();

        float slW = 280.f * scale, slH = 20.f * scale;
        mMusicSlider = { cx - slW*0.5f, cy - 50.f*scale, slW, slH };
        mSfxSlider   = { cx - slW*0.5f, cy + 10.f*scale, slW, slH };

        float tgW = 60.f * scale, tgH = 28.f * scale;
        float tgX = cx + 160.f * scale;
        mTgMusic = { tgX, cy - 55.f*scale, tgW, tgH };
        mTgSfx   = { tgX, cy +  5.f*scale, tgW, tgH };
        mTgDrum  = { tgX, cy + 65.f*scale, tgW, tgH };

        float bW = 180.f*scale, bH = 46.f*scale;
        mBtnBackX = cx - bW*0.5f; mBtnBackY = cy + 130.f*scale;
        mBtnBackW = bW; mBtnBackH = bH;
    }

    bool OptionsScene::HitSlider(const Slider& s, float px, float py) const {
        return px >= s.x && px <= s.x + s.w && py >= s.y - 10.f && py <= s.y + s.h + 10.f;
    }
    float OptionsScene::ComputeSliderVal(const Slider& s, float px) const {
        float v = (px - s.x) / s.w;
        return v < 0.f ? 0.f : v > 1.f ? 1.f : v;
    }

    bool HitRect(float x, float y, float w, float h, float px, float py) {
        return px >= x && px <= x+w && py >= y && py <= y+h;
    }

    void OptionsScene::OnEvent(AppContext& ctx, NkEvent& ev) {
        GameSettings& gs = *ctx.settings;

        auto handlePos = [&](float px, float py, bool release) {
            if (!release) {
                if (HitSlider(mMusicSlider, px, py)) { mDraggingMusic = true; }
                if (HitSlider(mSfxSlider,   px, py)) { mDraggingSfx   = true; }
            }
            if (release) { mDraggingMusic = mDraggingSfx = false; }
            if (mDraggingMusic) {
                gs.musicVolume = ComputeSliderVal(mMusicSlider, px);
                if (ctx.audio) ctx.audio->SetMusicVolume(gs.musicVolume);
            }
            if (mDraggingSfx) {
                gs.sfxVolume = ComputeSliderVal(mSfxSlider, px);
                if (ctx.audio) ctx.audio->SetSfxVolume(gs.sfxVolume);
            }
            if (release) {
                if (HitRect(mTgMusic.x, mTgMusic.y, mTgMusic.w, mTgMusic.h, px, py))
                    gs.musicEnabled = !gs.musicEnabled;
                if (HitRect(mTgSfx.x, mTgSfx.y, mTgSfx.w, mTgSfx.h, px, py))
                    gs.soundEnabled = !gs.soundEnabled;
                if (HitRect(mTgDrum.x, mTgDrum.y, mTgDrum.w, mTgDrum.h, px, py))
                    gs.drumEnabled = !gs.drumEnabled;
                if (HitRect(mBtnBackX, mBtnBackY, mBtnBackW, mBtnBackH, px, py))
                    ctx.scenes->Pop();
            }
        };

        if (auto* m = ev.As<NkMouseButtonPressEvent>())
            handlePos(m->GetX(), m->GetY(), false);
        if (auto* m = ev.As<NkMouseButtonReleaseEvent>())
            handlePos(m->GetX(), m->GetY(), true);
        if (auto* m = ev.As<NkMouseMoveEvent>())
            if (mDraggingMusic || mDraggingSfx)
                handlePos(m->GetX(), m->GetY(), false);
        if (auto* t = ev.As<NkTouchEndEvent>())
            if (t->GetNumTouches() > 0) {
                const NkTouchPoint& tp = t->GetTouch(0);
                handlePos(tp.clientX, tp.clientY, true);
            }
        if (auto* k = ev.As<NkKeyPressEvent>())
            if (k->GetKey() == NkKey::NK_ESCAPE) ctx.scenes->Pop();
    }

    void OptionsScene::DrawSlider(AppContext& ctx, const Slider& s, float val,
                                  const char* label, math::NkColor col) {
        GLRenderer2D& r = *ctx.renderer;
        FontAtlas&    f = *ctx.font;
        const float scale = GetUIScale(ctx.viewportW, ctx.viewportH);

        // Track
        r.DrawQuad(s.x, s.y, s.w, s.h, { 50, 35, 15, 200 });
        // Fill
        r.DrawQuad(s.x, s.y, s.w * val, s.h, col);
        // Knob
        float kx = s.x + s.w * val;
        r.DrawCircle(kx, s.y + s.h * 0.5f, s.h * 0.9f, col, 16);

        // Label + %
        char buf[32];
        snprintf(buf, sizeof(buf), "%s  %d%%", label, (int)(val * 100.f));
        f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, 0.85f * scale,
            s.x + s.w * 0.5f, s.y - 18.f * scale, buf,
            { 230, 200, 140, 220 });
    }

    void OptionsScene::DrawToggle(AppContext& ctx, float x, float y,
                                   float w, float h, bool on,
                                   const char* label, math::NkColor col) {
        GLRenderer2D& r = *ctx.renderer;
        FontAtlas&    f = *ctx.font;
        const float scale = GetUIScale(ctx.viewportW, ctx.viewportH);

        math::NkColor bg  = on ? AlphaF(col, 0.8f) : math::NkColor{50, 35, 15, 180};
        math::NkColor brd = on ? col : math::NkColor{100, 70, 30, 150};
        r.DrawQuad(x, y, w, h, bg);
        r.DrawQuadOutline(x, y, w, h, brd, 1.5f);

        // Cercle curseur
        float kx = on ? (x + w - h*0.5f) : (x + h*0.5f);
        r.DrawCircle(kx, y + h*0.5f, h*0.38f, on ? col : math::NkColor{120, 90, 40, 200}, 12);

        f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, 0.75f * scale,
            x - 70.f * scale, y + h*0.15f, label, { 220, 190, 130, 200 });
    }

    void OptionsScene::OnRender(AppContext& ctx) {
        GLRenderer2D& r = *ctx.renderer;
        FontAtlas&    f = *ctx.font;
        const int W = ctx.viewportW, H = ctx.viewportH;
        const float cx = ctx.safe.SafeCX(), cy = ctx.safe.SafeCY();
        const float scale = GetUIScale(W, H);
        const GameSettings& gs = *ctx.settings;

        r.Clear(0.04f, 0.02f, 0.01f, 1.f);
        r.Begin(W, H);

        // Panneau
        float panW = 480.f*scale, panH = 360.f*scale;
        float panX = cx - panW*0.5f, panY = cy - panH*0.5f;
        r.DrawQuad(panX, panY, panW, panH, { 18, 11, 4, 230 });
        r.DrawQuadOutline(panX, panY, panW, panH, OR_(), 2.5f);

        f.DrawStringShadowCenteredScaled(r, FontAtlas::HeadlineSlot, 1.1f * scale,
            cx, panY + 20.f * scale, "OPTIONS",
            { 242, 184, 46, 255 }, { 80, 30, 5, 120 }, 2);

        r.DrawQuad(panX + 20.f*scale, panY + 78.f*scale,
                   panW - 40.f*scale, 2.f, AlphaF(OR_(), 0.4f));

        // Section Audio
        f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, 0.9f*scale,
            cx, panY + 95.f*scale, "AUDIO",
            { 242, 184, 46, 200 });

        DrawSlider(ctx, mMusicSlider, gs.musicVolume, "Musique",    OR_());
        DrawSlider(ctx, mSfxSlider,   gs.sfxVolume,   "Effets",     TC());

        DrawToggle(ctx, mTgMusic.x, mTgMusic.y, mTgMusic.w, mTgMusic.h,
                   gs.musicEnabled,  "Musique",  VF());
        DrawToggle(ctx, mTgSfx.x, mTgSfx.y, mTgSfx.w, mTgSfx.h,
                   gs.soundEnabled,  "Effets",   TC());
        DrawToggle(ctx, mTgDrum.x, mTgDrum.y, mTgDrum.w, mTgDrum.h,
                   gs.drumEnabled,   "Tambours", OR_());

        // Bouton Retour
        r.DrawQuad(mBtnBackX, mBtnBackY, mBtnBackW, mBtnBackH,
                   { 70, 30, 6, 200 });
        r.DrawQuadOutline(mBtnBackX, mBtnBackY, mBtnBackW, mBtnBackH, OR_(), 2.f);
        f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
            mBtnBackX + mBtnBackW*0.5f, mBtnBackY + mBtnBackH*0.18f,
            "RETOUR", { 255, 235, 180, 255 });

        r.End();
    }

}} // namespace nkentseu::songoo
