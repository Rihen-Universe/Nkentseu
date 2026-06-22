#pragma once
// =============================================================================
// RihenIntroScene.h — Intro Rihen : 156 frames PNG chargées en background
// Architecture IDENTIQUE à celle du repo Nkentseu (worker thread + DrainQueue)
// Chemin frames : Resources/Songoo/assets/animrihen/rihen_00000.png … rihen_00155.png
// =============================================================================

#include "Songoo/UI/Scene.h"
#include "Songoo/Render/Texture2D.h"
#include <atomic>
#include <mutex>
#include <thread>

namespace nkentseu { class NkImage; }

namespace nkentseu { namespace songoo {

    class RihenIntroScene : public Scene {
    public:
        static constexpr int   kFrameCount            = 156;
        static constexpr float kDuration              = 4.0f;
        static constexpr float kFadeOut               = 0.3f;
        static constexpr float kFrameDuration         = kDuration / (float)kFrameCount;
        static constexpr int   kUploadsPerUpdate       = 16;
        static constexpr int   kUploadsPerUpdateLoading= 3;
        static constexpr int   kFramesToStartAnim      = kFrameCount;

        RihenIntroScene()  = default;
        ~RihenIntroScene() override = default;

        const char* Name() const noexcept override { return "RihenIntro"; }

        void OnEnter (AppContext& ctx) override;
        void OnUpdate(AppContext& ctx, float dt) override;
        void OnRender(AppContext& ctx) override;
        void OnExit  (AppContext& ctx) override;

    private:
        enum class State : uint8 { Loading, Playing, Done };
        State     mState       = State::Loading;
        float     mLoadingTime = 0.0f;

        Texture2D mFrames[kFrameCount];
        int       mFramesLoaded = 0;
        float     mAspect       = 4.0f;
        float     mTime         = 0.0f;
        bool      mDone         = false;
        Texture2D mLoadingLogo;

        int       mCurrentFrame = 0;
        float     mFrameAccum   = 0.0f;

        std::thread        mWorker;
        std::atomic<bool>  mWorkerStop{false};
        std::atomic<bool>  mWorkerDone{false};
        std::atomic<int>   mWorkerLastAttempted{-1};
        std::mutex         mQueueMutex;
        struct PendingFrame { int index; NkImage* image; };
        NkImage* mPending[kFrameCount] = { nullptr };
        int      mPendingNext = 0;

        void StartWorker();
        void StopWorker();
        void WorkerProc();
        int  DrainQueue(int maxUploads);
    };

}} // namespace nkentseu::songoo
