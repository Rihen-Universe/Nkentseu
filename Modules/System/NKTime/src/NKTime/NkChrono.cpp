/**
 * @File    NkChrono.cpp
 * @Brief   Implémentation de NkChrono et NkElapsedTime::ToString.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Notes de performance
 *
 *  Windows — QueryPerformanceCounter (QPC)
 *  ----------------------------------------
 *  Pour éviter l'overflow lors de (counter * 1e9 / freq), on décompose :
 *      ns = (counter / freq) * 1e9 + ((counter % freq) * 1e9) / freq
 *  Avec freq ~10 MHz, counter % freq < 1e7, donc (counter % freq) * 1e9 < 1e16
 *  ce qui tient dans int64 (max ~9.2e18). Aucun __int128 ni float80 nécessaire.
 *
 *  POSIX — clock_gettime(CLOCK_MONOTONIC)
 *  ----------------------------------------
 *  Résolution ~1 ns. Pour le sommeil on préfère clock_nanosleep (Linux,
 *  POSIX 2001) à nanosleep, plus précis car relatif à CLOCK_MONOTONIC.
 *  usleep() n'est pas utilisé : déprécié depuis POSIX.1-2008.
 *
 *  Emscripten — emscripten_sleep()
 *  ----------------------------------------
 *  Sur le Web, un appel bloquant doit céder le contrôle au navigateur.
 *  emscripten_sleep() le permet, mais nécessite de compiler avec
 *  `-s ASYNCIFY=1` pour fonctionner correctement. Si cette option n'est
 *  pas activée, le comportement est indéfini (le programme plantera ou
 *  bouclera à l'infini). Les appels avec durée nulle (0 ms) sont acceptés
 *  et se comportent comme un simple yield.
 *
 *  Sleep Windows sub-milliseconde
 *  ----------------------------------------
 *  ::Sleep() a une granularité ~15 ms sans timeBeginPeriod. Pour les durées
 *  inférieures à 1 ms, on utilise Sleep(1) comme approximation conservative.
 *  Une implémentation busy-wait précise peut être ajoutée si nécessaire.
 */

#include "pch.h"
#include "NKTime/NkChrono.h"
#include "NKPlatform/NkPlatformDetect.h"
#include <cstdio>   // snprintf — seule dépendance C standard autorisée

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#include <emscripten.h>
#endif

// ── Includes système ─────────────────────────────────────────────────────────
#if defined(NKENTSEU_PLATFORM_WINDOWS)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <time.h>   // clock_gettime, nanosleep, clock_nanosleep
#  include <errno.h>  // EINTR
#  include <sched.h>  // sched_yield
#endif

namespace nkentseu {

    // =============================================================================
    //  Helpers internes (linkage interne — non visibles hors de ce .cpp)
    // =============================================================================
    namespace {

    #if defined(NKENTSEU_PLATFORM_WINDOWS)

        /// Fréquence QPC — lue une seule fois (idempotente, pas besoin de mutex).
        NKTIME_INLINE int64 QPCFrequency() noexcept {
            static int64 freq = 0;
            if (NKTIME_UNLIKELY(freq == 0)) {
                LARGE_INTEGER li;
                ::QueryPerformanceFrequency(&li);
                freq = static_cast<int64>(li.QuadPart);
            }
            return freq;
        }

        /**
         * @Brief Convertit un compteur QPC en nanosecondes sans overflow.
         *
         * Formule :
         *   ns = (counter / freq) * 1e9 + (counter % freq * 1e9) / freq
         *
         * Garantit que (counter % freq * 1e9) reste dans int64 :
         *   freq ~10^7 → counter % freq < 10^7 → * 10^9 < 10^16 < INT64_MAX (~9.2e18)
         */
        NKTIME_INLINE int64 QPCToNanoseconds(int64 counter, int64 freq) noexcept {
            const int64 whole = (counter / freq) * 1'000'000'000LL;
            const int64 frac  = (counter % freq) * 1'000'000'000LL / freq;
            return whole + frac;
        }

    #else // POSIX

        /**
         * @Brief nanosleep / clock_nanosleep avec reprise automatique sur EINTR.
         * @Note  clock_nanosleep (Linux) est préféré car relatif à CLOCK_MONOTONIC.
         */
        NKTIME_INLINE void PosixSleepNs(int64 ns) noexcept {
            if (ns <= 0) return;
            struct timespec req;
            req.tv_sec  = static_cast<time_t>(ns / 1'000'000'000LL);
            req.tv_nsec = static_cast<long>  (ns % 1'000'000'000LL);

    #  if defined(__linux__)
            while (::clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &req) == -1
                && errno == EINTR) {}
    #  else
            struct timespec rem;
            while (::nanosleep(&req, &rem) == -1 && errno == EINTR) { req = rem; }
    #  endif
        }

    #endif // POSIX

    } // anonymous namespace

    // =============================================================================
    //  NkChrono
    // =============================================================================

    NkChrono::NkChrono() noexcept
        : mStartTime(Now())
    {}

    // -----------------------------------------------------------------------------
    NkElapsedTime NkChrono::Now() noexcept {
        int64 ns = 0;

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        LARGE_INTEGER counter;
        ::QueryPerformanceCounter(&counter);
        ns = QPCToNanoseconds(static_cast<int64>(counter.QuadPart), QPCFrequency());
    #else
        struct timespec ts;
        ::clock_gettime(CLOCK_MONOTONIC, &ts);
        ns = static_cast<int64>(ts.tv_sec) * 1'000'000'000LL
        + static_cast<int64>(ts.tv_nsec);
    #endif

        return NkElapsedTime::FromNanoseconds(static_cast<float64>(ns));
    }

    // -----------------------------------------------------------------------------
    NkElapsedTime NkChrono::Elapsed() const noexcept {
        return Now() - mStartTime;
    }

    // -----------------------------------------------------------------------------
    NkElapsedTime NkChrono::Reset() noexcept {
        const NkElapsedTime now     = Now();
        const NkElapsedTime elapsed = now - mStartTime;
        mStartTime = now;
        return elapsed;
    }

    // -----------------------------------------------------------------------------
    int64 NkChrono::GetFrequency() noexcept {
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        return QPCFrequency();
    #else
        return 1'000'000'000LL; // CLOCK_MONOTONIC : résolution 1 ns
    #endif
    }

    // =============================================================================
    //  NkChrono — Sleep / YieldThread
    // =============================================================================

    void NkChrono::Sleep(const NkDuration& duration) noexcept {
        const int64 ns = duration.ToNanoseconds();
        if (ns <= 0) return;

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        const DWORD ms = static_cast<DWORD>(ns / 1'000'000LL);
        ::Sleep(ms > 0 ? ms : 1);
    #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        // Convertir en millisecondes, avec un minimum de 1 ms pour emscripten_sleep.
        int ms = static_cast<int>(ns / 1'000'000LL);
        if (ms < 1) ms = 1;
        emscripten_sleep(ms);
    #else
        PosixSleepNs(ns);
    #endif
    }

    void NkChrono::Sleep(int64 milliseconds) noexcept {
        if (milliseconds <= 0) return;
        Sleep(NkDuration::FromMilliseconds(milliseconds));
    }

    void NkChrono::Sleep(float64 milliseconds) noexcept {
        if (milliseconds <= 0.0) return;
        Sleep(NkDuration::FromMilliseconds(milliseconds));
    }

    void NkChrono::SleepMilliseconds(int64 ms) noexcept { Sleep(ms); }

    void NkChrono::SleepMicroseconds(int64 us) noexcept {
        if (us <= 0) return;
        Sleep(NkDuration::FromMicroseconds(us));
    }

    void NkChrono::SleepNanoseconds(int64 ns) noexcept {
        if (ns <= 0) return;
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        ::Sleep(1); // Windows n'a pas de sommeil sub-milliseconde fiable sans busy-wait
    #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        int ms = static_cast<int>(ns / 1'000'000LL);
        if (ms < 1) ms = 1;
        emscripten_sleep(ms);
    #else
        PosixSleepNs(ns);
    #endif
    }

    void NkChrono::YieldThread() noexcept {
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        ::SwitchToThread();
    #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        // Céder le contrôle au navigateur sans bloquer.
        emscripten_sleep(0);
    #else
        ::sched_yield();
    #endif
    }

    // =============================================================================
    //  NkElapsedTime — ToString
    // =============================================================================

    NkString NkElapsedTime::ToString() const {
        char buf[64];
        if      (seconds      >= 1.0) ::snprintf(buf, sizeof(buf), "%.3f s",  seconds);
        else if (milliseconds >= 1.0) ::snprintf(buf, sizeof(buf), "%.3f ms", milliseconds);
        else if (microseconds >= 1.0) ::snprintf(buf, sizeof(buf), "%.3f us", microseconds);
        else                          ::snprintf(buf, sizeof(buf), "%.3f ns", nanoseconds);
        return NkString(buf);
    }

} // namespace nkentseu