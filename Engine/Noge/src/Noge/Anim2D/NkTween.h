#pragma once
// =============================================================================
// Nkentseu/Anim2D/NkTween.h
// =============================================================================
// Interpolation de propriétés (style GSAP/LeanTween/DOTween) pour animation 2D.
//
// USAGE :
//   // Déplacer un objet vers (100, 200) en 1 seconde avec ease OutCubic
//   auto& tw = NkTweenManager::Get().To(&pos, NkVec3f{100,200,0}, 1.f)
//       .SetEase(NkEase::CubicOut)
//       .SetDelay(0.5f)
//       .OnComplete([]{ printf("done!\n"); });
//
//   // Séquence : A puis B
//   auto& seq = NkTweenManager::Get().Sequence();
//   seq.Append(NkTweenManager::Get().To(&alpha, 0.f, 0.3f));
//   seq.AppendInterval(0.5f);
//   seq.Append(NkTweenManager::Get().To(&scale, NkVec3f{2,2,2}, 0.5f));
//
//   // Mise à jour chaque frame
//   NkTweenManager::Get().Update(dt);
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKMath/NkMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Functional/NkFunction.h"

namespace nkentseu {

    using namespace math;

    // =========================================================================
    // NkEase — fonctions d'accélération/décélération
    // =========================================================================
    enum class NkEase : uint8 {
        Linear,
        // Sine
        SineIn,   SineOut,   SineInOut,
        // Quad
        QuadIn,   QuadOut,   QuadInOut,
        // Cubic
        CubicIn,  CubicOut,  CubicInOut,
        // Quart
        QuartIn,  QuartOut,  QuartInOut,
        // Quint
        QuintIn,  QuintOut,  QuintInOut,
        // Expo
        ExpoIn,   ExpoOut,   ExpoInOut,
        // Circ
        CircIn,   CircOut,   CircInOut,
        // Back (overshoot)
        BackIn,   BackOut,   BackInOut,
        // Elastic
        ElasticIn, ElasticOut, ElasticInOut,
        // Bounce
        BounceIn,  BounceOut,  BounceInOut,
        // Custom (courbe Bézier)
        Custom
    };

    /**
     * @brief Évalue une fonction d'ease à t ∈ [0..1] → valeur ∈ [0..1].
     */
    [[nodiscard]] float32 NkEaseEval(NkEase ease, float32 t) noexcept;

    // =========================================================================
    // NkTween — interpolation d'une propriété
    // =========================================================================
    class NkTween {
        public:
            using Callback = NkFunction<void()>;
            using UpdateCallback = NkFunction<void(float32 t)>;  ///< t ∈ [0..1]

            // ── État ──────────────────────────────────────────────────────
            enum class State : uint8 { Idle, Waiting, Playing, Paused, Completed, Killed };

            NkTween()  noexcept = default;
            ~NkTween() noexcept = default;

            // ── API fluide de configuration ───────────────────────────────
            NkTween& SetEase    (NkEase e)      noexcept { mEase = e;      return *this; }
            NkTween& SetDelay   (float32 d)     noexcept { mDelay = d;     return *this; }
            NkTween& SetLoops   (int32 n)       noexcept { mLoops = n;     return *this; }  ///< -1 = infini
            NkTween& SetPingPong(bool v)        noexcept { mPingPong = v;  return *this; }
            NkTween& SetSpeed   (float32 s)     noexcept { mSpeed = s;     return *this; }
            NkTween& SetRelative(bool v)        noexcept { mRelative = v;  return *this; }

            NkTween& OnStart   (Callback cb)    noexcept { mOnStart    = static_cast<Callback&&>(cb); return *this; }
            NkTween& OnComplete(Callback cb)    noexcept { mOnComplete = static_cast<Callback&&>(cb); return *this; }
            NkTween& OnUpdate  (UpdateCallback cb) noexcept { mOnUpdate = static_cast<UpdateCallback&&>(cb); return *this; }

            // ── Contrôle ──────────────────────────────────────────────────
            void Play()    noexcept;
            void Pause()   noexcept;
            void Resume()  noexcept;
            void Restart() noexcept;
            void Kill()    noexcept { mState = State::Killed; }
            void Complete() noexcept;  ///< Force la completion (skip to end)

            // ── État ──────────────────────────────────────────────────────
            [[nodiscard]] State   GetState()      const noexcept { return mState; }
            [[nodiscard]] float32 GetProgress()   const noexcept { return mProgress; }  ///< [0..1]
            [[nodiscard]] bool    IsPlaying()     const noexcept { return mState == State::Playing; }
            [[nodiscard]] bool    IsComplete()    const noexcept { return mState == State::Completed; }
            [[nodiscard]] bool    IsKilled()      const noexcept { return mState == State::Killed; }

            // ── Mise à jour (appelée par NkTweenManager) ──────────────────
            void Update(float32 dt) noexcept;

        protected:
            virtual void ApplyValue(float32 t) noexcept = 0;   ///< Applique l'interpolation
            virtual void SnapToEnd() noexcept = 0;              ///< Finalise (t=1)

            float32  mDuration   = 1.f;
            float32  mDelay      = 0.f;
            float32  mTime       = 0.f;
            float32  mProgress   = 0.f;
            float32  mSpeed      = 1.f;
            int32    mLoops      = 0;   ///< 0=une fois, -1=infini, n=N fois
            int32    mLoopsDone  = 0;
            bool     mPingPong   = false;
            bool     mRelative   = false;
            bool     mStarted    = false;
            NkEase   mEase       = NkEase::Linear;
            State    mState      = State::Idle;

            Callback       mOnStart;
            Callback       mOnComplete;
            UpdateCallback mOnUpdate;
    };

    // =========================================================================
    // Tweens typés
    // =========================================================================

    class NkTweenFloat : public NkTween {
        public:
            NkTweenFloat(float32* target, float32 from, float32 to, float32 duration) noexcept
                : mTarget(target), mFrom(from), mTo(to) { mDuration = duration; }
        protected:
            void ApplyValue(float32 t) noexcept override {
                if (mTarget) *mTarget = mFrom + (mTo - mFrom) * t;
            }
            void SnapToEnd() noexcept override {
                if (mTarget) *mTarget = mTo;
            }
        private:
            float32* mTarget = nullptr;
            float32  mFrom = 0.f, mTo = 0.f;
    };

    class NkTweenVec3 : public NkTween {
        public:
            NkTweenVec3(NkVec3f* target, NkVec3f from, NkVec3f to, float32 duration) noexcept
                : mTarget(target), mFrom(from), mTo(to) { mDuration = duration; }
        protected:
            void ApplyValue(float32 t) noexcept override {
                if (mTarget) *mTarget = mFrom + (mTo - mFrom) * t;
            }
            void SnapToEnd() noexcept override {
                if (mTarget) *mTarget = mTo;
            }
        private:
            NkVec3f* mTarget = nullptr;
            NkVec3f  mFrom = {}, mTo = {};
    };

    class NkTweenColor : public NkTween {
        public:
            NkTweenColor(NkVec4f* target, NkVec4f from, NkVec4f to, float32 duration) noexcept
                : mTarget(target), mFrom(from), mTo(to) { mDuration = duration; }
        protected:
            void ApplyValue(float32 t) noexcept override {
                if (mTarget) *mTarget = {
                    mFrom.x + (mTo.x - mFrom.x) * t,
                    mFrom.y + (mTo.y - mFrom.y) * t,
                    mFrom.z + (mTo.z - mFrom.z) * t,
                    mFrom.w + (mTo.w - mFrom.w) * t
                };
            }
            void SnapToEnd() noexcept override {
                if (mTarget) *mTarget = mTo;
            }
        private:
            NkVec4f* mTarget = nullptr;
            NkVec4f  mFrom = {}, mTo = {};
    };

    // =========================================================================
    // NkTweenSequence — séquence ordonnée de tweens
    // =========================================================================
    class NkTweenSequence : public NkTween {
        public:
            NkTweenSequence() noexcept { mDuration = 0.f; }

            NkTweenSequence& Append  (NkTween* tw)       noexcept;  ///< Après le précédent
            NkTweenSequence& Join    (NkTween* tw)       noexcept;  ///< En parallèle du précédent
            NkTweenSequence& AppendInterval(float32 t)   noexcept;  ///< Pause

        protected:
            void ApplyValue(float32 t) noexcept override;
            void SnapToEnd()           noexcept override;

        private:
            struct Entry { NkTween* tween; float32 startTime; };
            NkVector<Entry> mEntries;
    };

    // =========================================================================
    // NkTweenManager — gestionnaire global des tweens actifs
    // =========================================================================
    class NkTweenManager {
        public:
            [[nodiscard]] static NkTweenManager& Get() noexcept {
                static NkTweenManager instance;
                return instance;
            }

            // ── Factory ───────────────────────────────────────────────────
            NkTween& To(float32* target, float32 endVal, float32 duration) noexcept;
            NkTween& To(NkVec3f* target, NkVec3f end, float32 duration) noexcept;
            NkTween& To(NkVec4f* target, NkVec4f end, float32 duration) noexcept;  ///< couleur
            NkTween& From(float32* target, float32 startVal, float32 duration) noexcept;
            NkTween& From(NkVec3f* target, NkVec3f start, float32 duration) noexcept;
            NkTweenSequence& Sequence() noexcept;

            // ── Contrôle ──────────────────────────────────────────────────
            void Update(float32 dt) noexcept;  ///< Appeler chaque frame

            void KillAll()  noexcept;
            void KillAllOnTarget(void* target) noexcept;
            void PauseAll() noexcept;
            void ResumeAll() noexcept;

            [[nodiscard]] uint32 ActiveCount() const noexcept {
                return static_cast<uint32>(mTweens.Size());
            }

        private:
            NkTweenManager() noexcept = default;
            NkVector<NkTween*> mTweens;  ///< Tweens actifs (owned)

            void Cleanup() noexcept;  ///< Supprime les tweens killed/complete
    };

} // namespace nkentseu
