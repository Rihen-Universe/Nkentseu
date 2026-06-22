#pragma once
#include "Songoo/UI/Scene.h"
namespace nkentseu { namespace songoo {
    class OptionsScene : public Scene {
    public:
        const char* Name() const noexcept override { return "Options"; }
        void OnEnter (AppContext&) override;
        void OnUpdate(AppContext& ctx, float dt) override;
        void OnRender(AppContext& ctx) override;
        void OnEvent (AppContext& ctx, NkEvent& ev) override;
    private:
        float mTime = 0.f;
        // Sliders et toggles
        struct Slider { float x, y, w, h; };
        Slider mMusicSlider{}, mSfxSlider{};
        float  mBtnBackX=0,mBtnBackY=0,mBtnBackW=0,mBtnBackH=0;
        bool   mDraggingMusic=false, mDraggingSfx=false;
        float  ComputeSliderVal(const Slider& s, float px) const;
        bool   HitSlider(const Slider& s, float px, float py) const;
        void   DrawSlider(AppContext& ctx, const Slider& s, float val,
                          const char* label, math::NkColor col);
        void   DrawToggle(AppContext& ctx, float x, float y,
                          float w, float h, bool on,
                          const char* label, math::NkColor col);
        struct ToggleBtn { float x,y,w,h; };
        ToggleBtn mTgMusic{}, mTgSfx{}, mTgDrum{};
    };
}} // namespace
