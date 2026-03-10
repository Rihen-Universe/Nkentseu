#pragma once
/**
 * @File    NkDuration.h
 * @Brief   Durée à spécifier — API pour sleep, timeout, délais. int64 interne.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design — Distinction NkDuration / NkElapsedTime
 *  NkDuration    : int64 interne en ns. Utilisée pour SPÉCIFIER une durée
 *                  (NkChrono::Sleep, timeout réseau, période d'un timer).
 *                  Mutable, opérateurs +=/-= disponibles, peut être négative.
 *
 *  NkElapsedTime : float64 interne en ns. RÉSULTAT d'une mesure de chrono.
 *                  4 champs précalculés en lecture directe, non mutable
 *                  depuis l'extérieur.
 *
 *  Conversion possible : NkDuration::FromNanoseconds((int64)elapsed.nanoseconds)
 */

#include "NKTime/NkTimeExport.h"
#include "NKTime/NkTimeConstants.h"
#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {

    class NKENTSEU_TIME_API NkDuration {
    public:

        // ── Constructeurs ────────────────────────────────────────────────────

        constexpr NkDuration() noexcept : mNanoseconds(0) {}
        constexpr explicit NkDuration(int64 nanoseconds) noexcept : mNanoseconds(nanoseconds) {}
        constexpr NkDuration(const NkDuration&)            noexcept = default;
        constexpr NkDuration& operator=(const NkDuration&) noexcept = default;

        // ── Fabriques statiques ──────────────────────────────────────────────

        NKTIME_NODISCARD static constexpr NkDuration FromNanoseconds (int64   ns) noexcept { return NkDuration(ns); }
        NKTIME_NODISCARD static constexpr NkDuration FromMicroseconds(int64   us) noexcept { return NkDuration(us  * time::NS_PER_MICROSECOND); }
        NKTIME_NODISCARD static constexpr NkDuration FromMicroseconds(float64 us) noexcept { return NkDuration(static_cast<int64>(us * static_cast<float64>(time::NS_PER_MICROSECOND))); }
        NKTIME_NODISCARD static constexpr NkDuration FromMilliseconds(int64   ms) noexcept { return NkDuration(ms  * time::NS_PER_MILLISECOND); }
        NKTIME_NODISCARD static constexpr NkDuration FromMilliseconds(float64 ms) noexcept { return NkDuration(static_cast<int64>(ms * static_cast<float64>(time::NS_PER_MILLISECOND))); }
        NKTIME_NODISCARD static constexpr NkDuration FromSeconds     (int64   s)  noexcept { return NkDuration(s   * time::NS_PER_SECOND); }
        NKTIME_NODISCARD static constexpr NkDuration FromSeconds     (float64 s)  noexcept { return NkDuration(static_cast<int64>(s  * static_cast<float64>(time::NS_PER_SECOND))); }
        NKTIME_NODISCARD static constexpr NkDuration FromMinutes     (int64   m)  noexcept { return NkDuration(m   * time::NS_PER_MINUTE); }
        NKTIME_NODISCARD static constexpr NkDuration FromMinutes     (float64 m)  noexcept { return NkDuration(static_cast<int64>(m  * static_cast<float64>(time::NS_PER_MINUTE))); }
        NKTIME_NODISCARD static constexpr NkDuration FromHours       (int64   h)  noexcept { return NkDuration(h   * time::NS_PER_HOUR); }
        NKTIME_NODISCARD static constexpr NkDuration FromHours       (float64 h)  noexcept { return NkDuration(static_cast<int64>(h  * static_cast<float64>(time::NS_PER_HOUR))); }
        NKTIME_NODISCARD static constexpr NkDuration FromDays        (int64   d)  noexcept { return NkDuration(d   * time::NS_PER_DAY); }
        NKTIME_NODISCARD static constexpr NkDuration FromDays        (float64 d)  noexcept { return NkDuration(static_cast<int64>(d  * static_cast<float64>(time::NS_PER_DAY))); }

        // ── Conversions ──────────────────────────────────────────────────────

        NKTIME_NODISCARD constexpr int64   ToNanoseconds()    const noexcept { return mNanoseconds; }
        NKTIME_NODISCARD constexpr int64   ToMicroseconds()   const noexcept { return mNanoseconds / time::NS_PER_MICROSECOND; }
        NKTIME_NODISCARD constexpr float64 ToMicrosecondsF()  const noexcept { return static_cast<float64>(mNanoseconds) / static_cast<float64>(time::NS_PER_MICROSECOND); }
        NKTIME_NODISCARD constexpr int64   ToMilliseconds()   const noexcept { return mNanoseconds / time::NS_PER_MILLISECOND; }
        NKTIME_NODISCARD constexpr float64 ToMillisecondsF()  const noexcept { return static_cast<float64>(mNanoseconds) / static_cast<float64>(time::NS_PER_MILLISECOND); }
        NKTIME_NODISCARD constexpr int64   ToSeconds()        const noexcept { return mNanoseconds / time::NS_PER_SECOND; }
        NKTIME_NODISCARD constexpr float64 ToSecondsF()       const noexcept { return static_cast<float64>(mNanoseconds) / static_cast<float64>(time::NS_PER_SECOND); }
        NKTIME_NODISCARD constexpr int64   ToMinutes()        const noexcept { return mNanoseconds / time::NS_PER_MINUTE; }
        NKTIME_NODISCARD constexpr float64 ToMinutesF()       const noexcept { return static_cast<float64>(mNanoseconds) / static_cast<float64>(time::NS_PER_MINUTE); }
        NKTIME_NODISCARD constexpr int64   ToHours()          const noexcept { return mNanoseconds / time::NS_PER_HOUR; }
        NKTIME_NODISCARD constexpr float64 ToHoursF()         const noexcept { return static_cast<float64>(mNanoseconds) / static_cast<float64>(time::NS_PER_HOUR); }
        NKTIME_NODISCARD constexpr int64   ToDays()           const noexcept { return mNanoseconds / time::NS_PER_DAY; }
        NKTIME_NODISCARD constexpr float64 ToDaysF()          const noexcept { return static_cast<float64>(mNanoseconds) / static_cast<float64>(time::NS_PER_DAY); }

        // ── Opérateurs arithmétiques ─────────────────────────────────────────

        NKTIME_NODISCARD constexpr NkDuration  operator+ (const NkDuration& o) const noexcept { return NkDuration(mNanoseconds + o.mNanoseconds); }
        NKTIME_NODISCARD constexpr NkDuration  operator- (const NkDuration& o) const noexcept { return NkDuration(mNanoseconds - o.mNanoseconds); }
        NKTIME_NODISCARD constexpr NkDuration  operator* (float64 s)           const noexcept { return NkDuration(static_cast<int64>(mNanoseconds * s)); }
        NKTIME_NODISCARD constexpr NkDuration  operator/ (float64 s)           const noexcept { return NkDuration(static_cast<int64>(mNanoseconds / s)); }
        NKTIME_NODISCARD constexpr float64     operator/ (const NkDuration& o) const noexcept { return static_cast<float64>(mNanoseconds) / static_cast<float64>(o.mNanoseconds); }
        NKTIME_NODISCARD constexpr NkDuration  operator- ()                    const noexcept { return NkDuration(-mNanoseconds); }

        constexpr NkDuration& operator+=(const NkDuration& o) noexcept { mNanoseconds += o.mNanoseconds; return *this; }
        constexpr NkDuration& operator-=(const NkDuration& o) noexcept { mNanoseconds -= o.mNanoseconds; return *this; }
        constexpr NkDuration& operator*=(float64 s)           noexcept { mNanoseconds  = static_cast<int64>(mNanoseconds * s); return *this; }
        constexpr NkDuration& operator/=(float64 s)           noexcept { mNanoseconds  = static_cast<int64>(mNanoseconds / s); return *this; }

        // ── Comparaisons ─────────────────────────────────────────────────────

        NKTIME_NODISCARD constexpr bool operator==(const NkDuration& o) const noexcept { return mNanoseconds == o.mNanoseconds; }
        NKTIME_NODISCARD constexpr bool operator!=(const NkDuration& o) const noexcept { return mNanoseconds != o.mNanoseconds; }
        NKTIME_NODISCARD constexpr bool operator< (const NkDuration& o) const noexcept { return mNanoseconds <  o.mNanoseconds; }
        NKTIME_NODISCARD constexpr bool operator<=(const NkDuration& o) const noexcept { return mNanoseconds <= o.mNanoseconds; }
        NKTIME_NODISCARD constexpr bool operator> (const NkDuration& o) const noexcept { return mNanoseconds >  o.mNanoseconds; }
        NKTIME_NODISCARD constexpr bool operator>=(const NkDuration& o) const noexcept { return mNanoseconds >= o.mNanoseconds; }

        // ── Utilitaires ──────────────────────────────────────────────────────

        NKTIME_NODISCARD constexpr NkDuration Abs()        const noexcept { return NkDuration(mNanoseconds < 0 ? -mNanoseconds : mNanoseconds); }
        NKTIME_NODISCARD constexpr bool       IsNegative() const noexcept { return mNanoseconds < 0; }
        NKTIME_NODISCARD constexpr bool       IsZero()     const noexcept { return mNanoseconds == 0; }
        NKTIME_NODISCARD constexpr bool       IsPositive() const noexcept { return mNanoseconds > 0; }

        // ── Formatage ────────────────────────────────────────────────────────

        NKTIME_NODISCARD NkString ToString()        const;
        NKTIME_NODISCARD NkString ToStringPrecise() const;

        // ── Constantes ───────────────────────────────────────────────────────

        NKTIME_NODISCARD static constexpr NkDuration Zero() noexcept { return NkDuration(0); }
        NKTIME_NODISCARD static constexpr NkDuration Max()  noexcept { return NkDuration(NKENTSEU_INT64_MAX); }
        NKTIME_NODISCARD static constexpr NkDuration Min()  noexcept { return NkDuration(NKENTSEU_INT64_MIN); }

    private:
        int64 mNanoseconds;
    };

    // Non-member scalar * duration
    NKTIME_NODISCARD NKTIME_INLINE constexpr NkDuration operator*(float64 scalar, const NkDuration& d) noexcept {
        return d * scalar;
    }

} // namespace nkentseu