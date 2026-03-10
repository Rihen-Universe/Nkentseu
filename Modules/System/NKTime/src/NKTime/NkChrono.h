#pragma once
/**
 * @File    NkChrono.h
 * @Brief   Chronomètre haute précision — sans dépendance STL.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  NkChrono mesure des durées relatives. Il ne stocke qu'un timestamp de départ
 *  (NkElapsedTime mStartTime) et calcule la différence à chaque appel à Elapsed().
 *
 *  Précision plateforme :
 *    Windows  : QueryPerformanceCounter  (~100 ns selon le matériel)
 *    Linux    : clock_gettime(CLOCK_MONOTONIC) (~1 ns)
 *    macOS    : clock_gettime(CLOCK_MONOTONIC) (~1 ns)
 *
 *  NkElapsedTime est séparé dans NkElapsedTime.h (résultat de mesure, float64).
 *  NkDuration    est séparé dans NkDuration.h    (durée à spécifier, int64).
 *  NkChrono::Sleep accepte NkDuration pour l'API de sommeil.
 *
 * @Usage
 * @code
 *   NkChrono chrono;
 *   // ... travail ...
 *   NkElapsedTime dt    = chrono.Reset();   // lit ET remet à zéro atomiquement
 *   NkElapsedTime total = chrono.Elapsed(); // lit sans remettre à zéro
 *   NkChrono::Sleep(NkDuration::FromMilliseconds(16));
 * @endcode
 */

#include "NKTime/NkTimeExport.h"
#include "NKTime/NkElapsedTime.h"
#include "NKTime/NkDuration.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {

    class NKENTSEU_TIME_API NkChrono {
        public:

            // ── Constructeurs ────────────────────────────────────────────────────

            /// Démarre le chronomètre immédiatement à la construction.
            NkChrono() noexcept;
            ~NkChrono() = default;

            NkChrono(const NkChrono&)            = default;
            NkChrono& operator=(const NkChrono&) = default;

            // ── Mesure ───────────────────────────────────────────────────────────

            /**
             * @Brief Temps écoulé depuis la dernière construction ou Reset().
             * @Note  Ne remet PAS le chrono à zéro.
             */
            NKTIME_NODISCARD NkElapsedTime Elapsed() const noexcept;

            /**
             * @Brief Lit le temps écoulé PUIS remet le chrono à zéro.
             *        Atomique depuis le point de vue de l'appelant.
             * @Return Durée avant remise à zéro.
             */
            NKTIME_NODISCARD NkElapsedTime Reset() noexcept;

            // ── Horloge de base ──────────────────────────────────────────────────

            /**
             * @Brief Temps absolu monotonique en nanosecondes.
             * @Note  Utilisez Elapsed() pour des mesures relatives.
             *        L'origine est arbitraire (boot, epoch, etc.) mais stable.
             */
            NKTIME_NODISCARD static NkElapsedTime Now() noexcept;

            /**
             * @Brief Fréquence de l'horloge sous-jacente en Hz.
             *        Windows : fréquence QPC.
             *        POSIX   : 1 000 000 000 (résolution 1 ns).
             */
            NKTIME_NODISCARD static int64 GetFrequency() noexcept;

            // ── Sommeil ──────────────────────────────────────────────────────────

            /// Sommeil de la durée spécifiée (NkDuration).
            static void Sleep(const NkDuration& duration) noexcept;

            /// Commodité : durée en millisecondes entières.
            static void Sleep(int64 milliseconds)   noexcept;

            /// Commodité : durée en millisecondes flottantes.
            static void Sleep(float64 milliseconds) noexcept;

            static void SleepMilliseconds(int64 ms) noexcept;
            static void SleepMicroseconds(int64 us) noexcept;
            static void SleepNanoseconds (int64 ns) noexcept;

            // ── Scheduling ───────────────────────────────────────────────────────

            /// Cède le quantum restant du thread au scheduler OS.
            static void YieldThread() noexcept;

        private:
            NkElapsedTime mStartTime; ///< Timestamp absolu du dernier Reset()/construction.
    };

} // namespace nkentseu