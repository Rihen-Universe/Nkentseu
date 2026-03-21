#pragma once
/**
 * @File    NkClock.h
 * @Brief   Orchestrateur de frame — gère NkTime pour la game loop.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  NkClock::NkTime est distinct de ::nkentseu::NkTime (heure du jour HH:MM:SS).
 *  NkClock::NkTime est un snapshot de frame : delta, fps, frameCount, timeScale.
 *
 *  Deux NkChrono internes :
 *    mFrameChrono — resetté à chaque Tick() → delta de frame
 *    mTotalChrono — jamais resetté          → temps depuis construction/Reset()
 *
 * @Usage
 * @code
 *   NkClock clock;
 *   clock.SetFixedDelta(1.f / 50.f);
 *
 *   while (running) {
 *       const NkClock::NkTime& t = clock.Tick();
 *       physics.Step(t.FixedScaled());
 *       renderer.Draw(t.Scaled());
 *   }
 * @endcode
 */

#include "NKTime/NkTimeExport.h"
#include "NKTime/NkChrono.h"

namespace nkentseu {

    class NKENTSEU_TIME_API NkClock {
    public:

        // ── NkTime (snapshot de frame) ────────────────────────────────────────
        struct NKENTSEU_TIME_API NkTime {
            float32 delta      = 0.f;
            float32 total      = 0.f;
            uint64  frameCount = 0;
            float32 fps        = 0.f;
            float32 fixedDelta = 1.f / 60.f;
            float32 timeScale  = 1.f;

            NKTIME_NODISCARD NKTIME_INLINE constexpr float32 Scaled()      const noexcept { return delta      * timeScale; }
            NKTIME_NODISCARD NKTIME_INLINE constexpr float32 FixedScaled() const noexcept { return fixedDelta * timeScale; }

            NKTIME_NODISCARD static NkTime From(
                const NkElapsedTime& delta,
                const NkElapsedTime& total,
                uint64  frame,
                float32 fixedDt,
                float32 scale
            ) noexcept;
        };

        // ── Constructeur / destructeur ────────────────────────────────────────

        NkClock() noexcept;
        ~NkClock() = default;

        NkClock(const NkClock&)            = delete;
        NkClock& operator=(const NkClock&) = delete;

        // ── Cycle de vie ──────────────────────────────────────────────────────

        /**
         * @Brief Remet à zéro les chronos et le compteur de frames.
         *        Conserve timeScale et fixedDelta configurés avant l'appel.
         */
        void Reset() noexcept;

        // ── Frame ─────────────────────────────────────────────────────────────

        /**
         * @Brief Avance d'une frame et retourne le snapshot mis à jour.
         *        En pause : delta = fps = 0, total continue, frameCount s'incrémente.
         */
        const NkTime& Tick() noexcept;

        NKTIME_NODISCARD const NkTime& GetTime() const noexcept { return mTime; }

        // ── Paramètres ────────────────────────────────────────────────────────

        void SetTimeScale (float32 scale)   noexcept;
        void SetFixedDelta(float32 seconds) noexcept;
        void Pause()  noexcept;
        void Resume() noexcept;

        NKTIME_NODISCARD bool IsPaused() const noexcept { return mPaused; }

        // ── Sleep / Yield (délégation directe) ───────────────────────────────

        static void Sleep            (const NkDuration& d) noexcept { NkChrono::Sleep(d);              }
        static void Sleep            (int64  ms)           noexcept { NkChrono::Sleep(ms);              }
        static void Sleep            (float64 ms)          noexcept { NkChrono::Sleep(ms);              }
        static void SleepMilliseconds(int64  ms)           noexcept { NkChrono::SleepMilliseconds(ms);  }
        static void SleepMicroseconds(int64  us)           noexcept { NkChrono::SleepMicroseconds(us);  }
        static void SleepNanoseconds (int64  ns)           noexcept { NkChrono::SleepNanoseconds(ns);   }
        static void YieldThread      ()                    noexcept { NkChrono::YieldThread();           }

    private:
        NkChrono mFrameChrono;
        NkChrono mTotalChrono;
        NkTime   mTime;
        bool     mPaused = false;
    };

} // namespace nkentseu