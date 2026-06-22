// =============================================================================
// Games/Common/MouFeedback.cpp
// =============================================================================
#include "Games/Common/MouFeedback.h"
#include "Games/Common/MouFrame.h"
#include "UI/MouUIColor.h"
#include <cmath>

namespace mou {

    using namespace nkentseu;
    using C = ui::MouUIColor;

    void MouFeedback::ResetLevel(int32 maxStars) noexcept {
        mMax = maxStars; mLeft = maxStars;
        mLostIdx = -1; mLostT = 0.f;
        mState = State::Playing; mTimer = 0.f; mPulse = 0.f;
        mFx.Clear();
        mCue = Cue::None;
    }

    void MouFeedback::Tap(float32 x, float32 y) noexcept {
        mFx.Sparkle(x, y, 7);
        mCue = Cue::Star;
    }

    void MouFeedback::Good(float32 x, float32 y) noexcept {
        mFx.Confetti(x, y, 16);
        mFx.Sparkle(x, y, 12);
        mMascotT = 0.f;          // déclenche le rebond de la mascotte
        mCue = Cue::Good;
    }

    float32 MouFeedback::MascotScale() const noexcept {
        if (mMascotT < 0.45f) {
            const float32 p = mMascotT / 0.45f;
            return 1.f + 0.22f * (float32)std::sin((double)p * 3.14159265);   // une bosse douce
        }
        return 1.f;
    }

    bool MouFeedback::Bad(float32 x, float32 y) noexcept {
        mFx.Puff(x, y, 14);
        if (mLeft > 0) { mLostIdx = mLeft - 1; mLostT = 0.f; --mLeft; }
        if (mLeft <= 0) { mState = State::Failed; mTimer = 0.f; mCue = Cue::Fail; return false; }
        mCue = Cue::Bad;
        return true;
    }

    void MouFeedback::Win(float32 viewW) noexcept {
        if (mState != State::Playing) return;
        mState = State::Won; mTimer = 0.f;
        mFx.Rain(viewW);
        mMascotT = 0.f;
        mCue = Cue::Win;
    }

    void MouFeedback::Update(float32 dt) noexcept {
        mFx.Update(dt);
        mPulse += dt;
        mMascotT += dt;
        if (mLostIdx >= 0) { mLostT += dt; if (mLostT > 0.5f) mLostIdx = -1; }
        if (mState != State::Playing) mTimer += dt;
    }

    void MouFeedback::RenderFx(const MouFrame& frame) const noexcept {
        mFx.Render(frame);
    }

    void MouFeedback::RenderHud(const MouFrame& frame) const noexcept {
        // Rangée d'étoiles en haut à droite, toujours visible pendant le jeu.
        const float32 s = 52.f, gap = 8.f;
        const float32 totalW = mMax * s + (mMax - 1) * gap;
        const float32 x0 = frame.width - totalW - 20.f;
        const float32 y0 = 16.f;
        for (int32 i = 0; i < mMax; ++i) {
            float32 sz = s, off = 0.f;
            uint8 alpha = 255;
            if (i >= mLeft) {
                if (i == mLostIdx) {
                    // étoile qui vient d'être perdue : rétrécit + tombe + s'efface
                    const float32 t = mLostT / 0.5f;
                    sz = s * (1.f - t * 0.5f);
                    off = t * 18.f;
                    alpha = (uint8)(255.f * (1.f - t));
                } else {
                    alpha = 60;   // étoile déjà perdue : grisée
                }
            }
            const float32 cx = x0 + i * (s + gap) + (s - sz) * 0.5f;
            const float32 cy = y0 + (s - sz) * 0.5f + off;
            if (mStarTex)
                frame.Image(mStarTex, cx, cy, sz, sz, math::NkColor{ 255, 255, 255, alpha });
        }
    }

    void MouFeedback::RenderOverlay(const MouFrame& frame) const noexcept {
        if (mState == State::Playing) return;
        const float32 W = frame.width, H = frame.height;
        frame.Rect(0.f, 0.f, W, H, ui::WithAlpha(C::INK(), 110));

        if (mState == State::Won) {
            // Étoiles gagnées (grandes, qui "pop" via le pulse) + Bravo.
            const float32 ss = 150.f;
            const float32 pop = 1.f + 0.08f * (float32)std::sin((double)mPulse * 6.0);
            for (int32 i = 0; i < mMax; ++i) {
                const float32 z = (i < mLeft) ? ss * pop : ss * 0.8f;
                const float32 sx = W * 0.5f + (i - (mMax - 1) * 0.5f) * (ss + 14.f) - z * 0.5f;
                const math::NkColor tint = (i < mLeft) ? math::NkColor{255,255,255,255}
                                                       : ui::WithAlpha(math::NkColor{255,255,255,255}, 70);
                if (mStarTex) frame.Image(mStarTex, sx, H * 0.28f - (z - ss) * 0.5f, z, z, tint);
            }
            frame.TextCentered(frame.titleFont, 0.f, W, H * 0.28f + ss + 14.f, "Bravo !", C::SUNNY());
        } else {
            // Échec : message doux + invitation à réessayer.
            frame.TextCentered(frame.titleFont, 0.f, W, H * 0.42f, "On recommence !", C::ORANGE());
            frame.TextCentered(frame.font, 0.f, W, H * 0.42f + 70.f,
                               "Tu vas y arriver !", C::TEXT_DARK());
        }
    }

}  // namespace mou
