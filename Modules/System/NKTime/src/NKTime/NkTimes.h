#pragma once
/**
 * @File    NkTime.h
 * @Brief   Temps quotidien avec précision nanoseconde — sans dépendance STL.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  Stocke 5 composants : heure [0-23], minute [0-59], seconde [0-59],
 *  milliseconde [0-999], nanoseconde [0-999999].
 *  Les constantes de conversion proviennent de NkTimeConstants.h (source unique).
 *  Les opérateurs arithmétiques passent par int64 (total en ns) pour la cohérence.
 */

#include "NKTime/NkTimeExport.h"
#include "NKTime/NkTimeConstants.h"
#include "NKPlatform/NkPlatformDetect.h"
#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include <ctime>

namespace nkentseu {

    class NKENTSEU_TIME_API NkTime {
        public:

            // ── Constructeurs ────────────────────────────────────────────────────

            /// Initialise tous les composants à zéro (minuit).
            NkTime() noexcept;

            /**
             * @param hour        [0-23]
             * @param minute      [0-59]
             * @param second      [0-59]
             * @param millisecond [0-999]   (défaut : 0)
             * @param nanosecond  [0-999999](défaut : 0)
             */
            NkTime(int32 hour, int32 minute, int32 second,
                int32 millisecond = 0, int32 nanosecond = 0);

            /// Construit depuis un total de nanosecondes depuis minuit.
            explicit NkTime(int64 totalNanoseconds) noexcept;

            NkTime(const NkTime&)            noexcept = default;
            NkTime& operator=(const NkTime&) noexcept = default;

            // ── Accesseurs ───────────────────────────────────────────────────────

            NKTIME_NODISCARD int32 GetHour()        const noexcept { return mHour;        }
            NKTIME_NODISCARD int32 GetMinute()      const noexcept { return mMinute;      }
            NKTIME_NODISCARD int32 GetSecond()      const noexcept { return mSecond;      }
            NKTIME_NODISCARD int32 GetMillisecond() const noexcept { return mMillisecond; }
            NKTIME_NODISCARD int32 GetNanosecond()  const noexcept { return mNanosecond;  }

            // ── Mutateurs ────────────────────────────────────────────────────────

            void SetHour       (int32 hour);
            void SetMinute     (int32 minute);
            void SetSecond     (int32 second);
            void SetMillisecond(int32 millis);
            void SetNanosecond (int32 nano);

            // ── Opérateurs arithmétiques ─────────────────────────────────────────

            NkTime& operator+=(const NkTime& o) noexcept;
            NkTime& operator-=(const NkTime& o) noexcept;
            NKTIME_NODISCARD NkTime operator+(const NkTime& o) const noexcept;
            NKTIME_NODISCARD NkTime operator-(const NkTime& o) const noexcept;

            // ── Comparaisons ─────────────────────────────────────────────────────

            NKTIME_NODISCARD bool operator==(const NkTime& o) const noexcept;
            NKTIME_NODISCARD bool operator!=(const NkTime& o) const noexcept;
            NKTIME_NODISCARD bool operator< (const NkTime& o) const noexcept;
            NKTIME_NODISCARD bool operator<=(const NkTime& o) const noexcept;
            NKTIME_NODISCARD bool operator> (const NkTime& o) const noexcept;
            NKTIME_NODISCARD bool operator>=(const NkTime& o) const noexcept;

            // ── Conversions de type ──────────────────────────────────────────────

            /// Secondes avec précision milliseconde.
            explicit operator float()  const noexcept;
            /// Secondes avec précision nanoseconde.
            explicit operator double() const noexcept;
            /// Nanosecondes totales depuis minuit.
            explicit operator int64()  const noexcept;

            // ── Statiques ────────────────────────────────────────────────────────

            /// Heure système locale avec précision nanoseconde — thread-safe.
            NKTIME_NODISCARD static NkTime GetCurrent();

            // ── Formatage ────────────────────────────────────────────────────────

            /// Format ISO 8601 étendu : "HH:MM:SS.mmm.nnnnnn"
            NKTIME_NODISCARD NkString ToString() const;
            friend NkString ToString(const NkTime& t) { return t.ToString(); }

        private:
            int32 mHour        = 0;
            int32 mMinute      = 0;
            int32 mSecond      = 0;
            int32 mMillisecond = 0;
            int32 mNanosecond  = 0;

            void Normalize() noexcept;
            void Validate()  const;
    };

} // namespace nkentseu