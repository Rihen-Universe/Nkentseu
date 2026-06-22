#pragma once
// =============================================================================
// StoryScene.h — Animation histoire Songo'o (6 images + sous-titres + fondu)
// Logique IDENTIQUE à l'original (6 frames, durées irrégulières, alpha fondu)
// =============================================================================

#include "Songoo/UI/Scene.h"
#include "Songoo/Render/Texture2D.h"

namespace nkentseu { namespace songoo {

    class StoryScene : public Scene {
    public:
        static constexpr int kFrameCount = 6;

        const char* Name() const noexcept override { return "Story"; }
        void OnEnter (AppContext& ctx) override;
        void OnUpdate(AppContext& ctx, float dt) override;
        void OnRender(AppContext& ctx) override;
        void OnExit  (AppContext& ctx) override;
        void OnEvent (AppContext& ctx, NkEvent& ev) override;

    private:
        struct StoryFrame {
            const char* path;
            float displayTime;
            float fadeIn;
            float fadeOut;
            const char* subtitle;
        };

        static const StoryFrame kFrames[kFrameCount];

        Texture2D mTextures[kFrameCount];
        int   mCurrentFrame = 0;
        float mFrameTimer   = 0.f;
        bool  mDone         = false;

        float ComputeAlpha(int frameIdx, float elapsed) const;
    };

}} // namespace nkentseu::songoo
