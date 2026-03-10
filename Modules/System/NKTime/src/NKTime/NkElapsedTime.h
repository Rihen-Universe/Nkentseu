#pragma once
/**
 * @File    NkElapsedTime.h
 * @Brief   Résultat d'une mesure de temps — durée exprimée en 4 unités précalculées.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  NkElapsedTime représente la SORTIE d'une mesure (NkChrono::Elapsed, Reset, Now).
 *  Les 4 unités (ns, µs, ms, s) sont calculées UNE SEULE FOIS à la construction
 *  depuis la source de vérité `nanoseconds`. Chaque accès ultérieur est une lecture
 *  directe de champ : zéro calcul, zéro branche.
 *
 *  Ce type est DISTINCT de NkDuration :
 *    - NkElapsedTime : float64 interne — résultat de mesure haute précision,
 *                      champs publics en lecture directe, pas de mutation.
 *    - NkDuration    : int64 interne  — durée à spécifier (sleep, timeout, délai),
 *                      API mutable avec +=/-= etc.
 *
 *  @Layout mémoire : 4 × float64 = 32 octets (aligné sur une cache line de 64 octets
 *  avec les 32 octets restants libres pour les membres adjacents).
 */

#include "NKTime/NkTimeExport.h"
#include "NKTime/NkTimeConstants.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {

    struct NKENTSEU_TIME_API NkElapsedTime {

            // Champs publics — lecture directe, zéro calcul à l'accès.
            float64 nanoseconds;   ///< Durée en ns
            float64 microseconds;  ///< Durée en µs  (= ns × 1e-3)
            float64 milliseconds;  ///< Durée en ms  (= ns × 1e-6)
            float64 seconds;       ///< Durée en s   (= ns × 1e-9)

            // ── Constructeurs ────────────────────────────────────────────────────

            /// Durée nulle.
            constexpr NkElapsedTime() noexcept
                : nanoseconds(0.0), microseconds(0.0), milliseconds(0.0), seconds(0.0) {}

        private:
            /// Constructeur canonique : toutes unités calculées depuis ns en une passe.
            constexpr explicit NkElapsedTime(float64 ns) noexcept
                : nanoseconds (ns)
                , microseconds(ns * 1.0e-3)
                , milliseconds(ns * 1.0e-6)
                , seconds     (ns * 1.0e-9)
            {}

        public:
            // ── Fabriques statiques ──────────────────────────────────────────────

            NKTIME_NODISCARD static constexpr NkElapsedTime FromNanoseconds (float64 ns) noexcept { return NkElapsedTime(ns);          }
            NKTIME_NODISCARD static constexpr NkElapsedTime FromMicroseconds(float64 us) noexcept { return NkElapsedTime(us * 1.0e3);  }
            NKTIME_NODISCARD static constexpr NkElapsedTime FromMilliseconds(float64 ms) noexcept { return NkElapsedTime(ms * 1.0e6);  }
            NKTIME_NODISCARD static constexpr NkElapsedTime FromSeconds     (float64 s)  noexcept { return NkElapsedTime(s  * 1.0e9);  }

            // ── Accesseurs nommés (synonymes fluides — zéro calcul) ──────────────

            NKTIME_NODISCARD NKTIME_INLINE constexpr float64 ToNanoseconds()  const noexcept { return nanoseconds;  }
            NKTIME_NODISCARD NKTIME_INLINE constexpr float64 ToMicroseconds() const noexcept { return microseconds; }
            NKTIME_NODISCARD NKTIME_INLINE constexpr float64 ToMilliseconds() const noexcept { return milliseconds; }
            NKTIME_NODISCARD NKTIME_INLINE constexpr float64 ToSeconds()      const noexcept { return seconds;      }

            // ── Opérateurs arithmétiques ─────────────────────────────────────────
            // Passent systématiquement par le constructeur privé pour maintenir la cohérence.

            NKTIME_NODISCARD constexpr NkElapsedTime operator+(const NkElapsedTime& rhs) const noexcept {
                return NkElapsedTime(nanoseconds + rhs.nanoseconds);
            }
            NKTIME_NODISCARD constexpr NkElapsedTime operator-(const NkElapsedTime& rhs) const noexcept {
                return NkElapsedTime(nanoseconds - rhs.nanoseconds);
            }
            constexpr NkElapsedTime& operator+=(const NkElapsedTime& rhs) noexcept {
                *this = NkElapsedTime(nanoseconds + rhs.nanoseconds); return *this;
            }
            constexpr NkElapsedTime& operator-=(const NkElapsedTime& rhs) noexcept {
                *this = NkElapsedTime(nanoseconds - rhs.nanoseconds); return *this;
            }
            NKTIME_NODISCARD constexpr NkElapsedTime operator*(float64 factor)  const noexcept { return NkElapsedTime(nanoseconds * factor);  }
            NKTIME_NODISCARD constexpr NkElapsedTime operator/(float64 divisor) const noexcept { return NkElapsedTime(nanoseconds / divisor); }

            // ── Comparaisons (sur ns — source de vérité) ─────────────────────────

            NKTIME_NODISCARD constexpr bool operator==(const NkElapsedTime& rhs) const noexcept { return nanoseconds == rhs.nanoseconds; }
            NKTIME_NODISCARD constexpr bool operator!=(const NkElapsedTime& rhs) const noexcept { return nanoseconds != rhs.nanoseconds; }
            NKTIME_NODISCARD constexpr bool operator< (const NkElapsedTime& rhs) const noexcept { return nanoseconds <  rhs.nanoseconds; }
            NKTIME_NODISCARD constexpr bool operator<=(const NkElapsedTime& rhs) const noexcept { return nanoseconds <= rhs.nanoseconds; }
            NKTIME_NODISCARD constexpr bool operator> (const NkElapsedTime& rhs) const noexcept { return nanoseconds >  rhs.nanoseconds; }
            NKTIME_NODISCARD constexpr bool operator>=(const NkElapsedTime& rhs) const noexcept { return nanoseconds >= rhs.nanoseconds; }

            // ── Formatage ────────────────────────────────────────────────────────
            /**
             * @Brief Formate dans l'unité la plus lisible.
             * @Examples "1.234 s", "45.678 ms", "789.012 us", "12.345 ns"
             * @Note Utilise 'us' (pas 'µs') pour éviter les problèmes d'encodage UTF-8.
             */
            NKTIME_NODISCARD NkString ToString() const;

            friend NkString ToString(const NkElapsedTime& e) { return e.ToString(); }
    };

} // namespace nkentseu