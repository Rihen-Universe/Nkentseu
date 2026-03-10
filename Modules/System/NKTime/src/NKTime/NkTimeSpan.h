#pragma once
/**
 * @File    NkTimeSpan.h
 * @Brief   Durée signée avec décomposition calendaire — sans dépendance STL.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  NkTimeSpan représente une durée signée de plusieurs jours, heures, minutes,
 *  secondes et nanosecondes. Analogue à .NET System.TimeSpan.
 *  Stockage : un unique int64 en nanosecondes (source de vérité).
 *
 *  Distinction NkTimeSpan / NkDuration :
 *    NkTimeSpan : durée signée avec GetDays/Hours/... et GetDate/GetTime.
 *                 Usage : intervalles calendaires, différence entre deux dates.
 *    NkDuration : durée simple pour API (Sleep, timeout, période de timer).
 *                 Usage : NkChrono::Sleep(NkDuration::FromMilliseconds(16)).
 *
 *  GetDays/GetHours/... retournent les composants DÉCOMPOSÉS (pas les totaux) :
 *    NkTimeSpan::FromHours(25).GetDays()  == 1
 *    NkTimeSpan::FromHours(25).GetHours() == 1  (pas 25)
 */

#include "NKTime/NkTimeExport.h"
#include "NKTime/NkTimeConstants.h"
#include "NKTime/NkTimes.h"
#include "NKTime/NkDate.h"
#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {

    class NKENTSEU_TIME_API NkTimeSpan {
        public:

            // ── Constructeurs ────────────────────────────────────────────────────

            NkTimeSpan() noexcept;

            /**
             * @param days         Peut être négatif.
             * @param hours        [0-23] supplémentaires
             * @param minutes      [0-59] supplémentaires
             * @param seconds      [0-59] supplémentaires
             * @param milliseconds [0-999] supplémentaires
             * @param nanoseconds  [0-999999] supplémentaires
             */
            NkTimeSpan(int64 days, int64 hours, int64 minutes, int64 seconds,
                    int64 milliseconds = 0, int64 nanoseconds = 0) noexcept;

            /// Construit depuis un total de nanosecondes.
            constexpr explicit NkTimeSpan(int64 totalNanoseconds) noexcept
                : mTotalNanoseconds(totalNanoseconds) {}

            NkTimeSpan(const NkTimeSpan&)            noexcept = default;
            NkTimeSpan& operator=(const NkTimeSpan&) noexcept = default;

            // ── Fabriques statiques ──────────────────────────────────────────────

            NKTIME_NODISCARD static NkTimeSpan FromDays        (int64 days)         noexcept;
            NKTIME_NODISCARD static NkTimeSpan FromHours       (int64 hours)        noexcept;
            NKTIME_NODISCARD static NkTimeSpan FromMinutes     (int64 minutes)      noexcept;
            NKTIME_NODISCARD static NkTimeSpan FromSeconds     (int64 seconds)      noexcept;
            NKTIME_NODISCARD static NkTimeSpan FromMilliseconds(int64 milliseconds) noexcept;
            NKTIME_NODISCARD static NkTimeSpan FromNanoseconds (int64 nanoseconds)  noexcept;

            // ── Composants décomposés ────────────────────────────────────────────

            NKTIME_NODISCARD int64 GetDays()         const noexcept;
            NKTIME_NODISCARD int64 GetHours()        const noexcept;
            NKTIME_NODISCARD int64 GetMinutes()      const noexcept;
            NKTIME_NODISCARD int64 GetSeconds()      const noexcept;
            NKTIME_NODISCARD int64 GetMilliseconds() const noexcept;
            NKTIME_NODISCARD int64 GetNanoseconds()  const noexcept;

            // ── Totaux ───────────────────────────────────────────────────────────

            NKTIME_NODISCARD constexpr int64  ToNanoseconds() const noexcept { return mTotalNanoseconds; }
            NKTIME_NODISCARD double           ToSeconds()     const noexcept;

            // ── Opérateurs arithmétiques ─────────────────────────────────────────

            NKTIME_NODISCARD NkTimeSpan  operator+ (const NkTimeSpan& o) const noexcept;
            NKTIME_NODISCARD NkTimeSpan  operator- (const NkTimeSpan& o) const noexcept;
            NKTIME_NODISCARD NkTimeSpan  operator* (double factor)       const noexcept;
            NKTIME_NODISCARD NkTimeSpan  operator/ (double divisor)      const noexcept;
            NkTimeSpan& operator+=(const NkTimeSpan& o)                        noexcept;
            NkTimeSpan& operator-=(const NkTimeSpan& o)                        noexcept;
            NkTimeSpan& operator*=(double factor)                               noexcept;
            NkTimeSpan& operator/=(double divisor)                              noexcept;

            // ── Méthodes nommées (chaînables) ────────────────────────────────────

            NkTimeSpan& Add     (const NkTimeSpan& o) noexcept;
            NkTimeSpan& Subtract(const NkTimeSpan& o) noexcept;
            NkTimeSpan& Multiply(double factor)        noexcept;
            NkTimeSpan& Divide  (double divisor)       noexcept;

            // ── Comparaisons ─────────────────────────────────────────────────────

            NKTIME_NODISCARD bool operator==(const NkTimeSpan& o) const noexcept;
            NKTIME_NODISCARD bool operator!=(const NkTimeSpan& o) const noexcept;
            NKTIME_NODISCARD bool operator< (const NkTimeSpan& o) const noexcept;
            NKTIME_NODISCARD bool operator<=(const NkTimeSpan& o) const noexcept;
            NKTIME_NODISCARD bool operator> (const NkTimeSpan& o) const noexcept;
            NKTIME_NODISCARD bool operator>=(const NkTimeSpan& o) const noexcept;

            // ── Extraction calendaire ────────────────────────────────────────────

            /// Partie horaire de la durée (modulo 24h).
            NKTIME_NODISCARD NkTime GetTime() const noexcept;
            /// Partie calendaire depuis l'époque Unix (algorithme de Howard Hinnant).
            NKTIME_NODISCARD NkDate GetDate() const noexcept;

            // ── Formatage ────────────────────────────────────────────────────────

            NKTIME_NODISCARD NkString ToString() const;
            friend NkString ToString(const NkTimeSpan& ts) { return ts.ToString(); }

        private:
            int64 mTotalNanoseconds = 0;
    };

} // namespace nkentseu