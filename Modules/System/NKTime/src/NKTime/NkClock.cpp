#include "pch.h"
#include "NKTime/NkClock.h"

namespace nkentseu {

    // ── NkTime::From ─────────────────────────────────────────────────────────

    NkClock::NkTime NkClock::NkTime::From(
        const NkElapsedTime& delta,
        const NkElapsedTime& total,
        uint64  frame,
        float32 fixedDt,
        float32 scale
    ) noexcept
    {
        NkTime t;
        t.delta      = static_cast<float32>(delta.seconds);
        t.total      = static_cast<float32>(total.seconds);
        t.frameCount = frame;
        // Clamp : évite INF/NaN à la première frame ou après un spike
        t.fps        = (delta.seconds > 1.0e-6) ? static_cast<float32>(1.0 / delta.seconds) : 0.f;
        t.fixedDelta = fixedDt;
        t.timeScale  = scale;
        return t;
    }

    // ── NkClock ──────────────────────────────────────────────────────────────

    NkClock::NkClock() noexcept
        : mFrameChrono()
        , mTotalChrono()
        , mTime()
        , mPaused(false)
    {}

    void NkClock::Reset() noexcept {
        // Sauvegarde des paramètres configurés par l'utilisateur
        const float32 savedFixedDelta = mTime.fixedDelta;
        const float32 savedTimeScale  = mTime.timeScale;

        mFrameChrono = NkChrono{};
        mTotalChrono = NkChrono{};
        mTime        = NkTime{};

        // Restauration — Reset() repart de zéro sans perdre la config
        mTime.fixedDelta = savedFixedDelta;
        mTime.timeScale  = savedTimeScale;
    }

    const NkClock::NkTime& NkClock::Tick() noexcept {
        const NkElapsedTime rawDelta = mFrameChrono.Reset();
        const NkElapsedTime total    = mTotalChrono.Elapsed();

        mTime = NkTime::From(
            mPaused ? NkElapsedTime{} : rawDelta,  // delta nul en pause
            total,
            mTime.frameCount + 1,
            mTime.fixedDelta,                       // conservé entre les frames
            mTime.timeScale                         // conservé entre les frames
        );
        return mTime;
    }

    void NkClock::SetTimeScale (float32 scale)   noexcept { mTime.timeScale  = scale;   }
    void NkClock::SetFixedDelta(float32 seconds) noexcept { mTime.fixedDelta = seconds; }
    void NkClock::Pause()  noexcept { mPaused = true;  }
    void NkClock::Resume() noexcept { mPaused = false; }

} // namespace nkentseu