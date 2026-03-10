#pragma once
/**
 * @File    NkDate.h
 * @Brief   Date grégorienne validée — sans dépendance STL.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */

#include "NKTime/NkTimeExport.h"
#include "NKPlatform/NkPlatformDetect.h"
#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include <ctime>

namespace nkentseu {

    /**
     * @Class NkDate
     * @Brief Représente une date grégorienne avec validation automatique.
     *
     * Plages valides :
     *   year  ∈ [1601, 30827]
     *   month ∈ [1, 12]
     *   day   ∈ [1, DaysInMonth(year, month)]
     */
    class NKENTSEU_TIME_API NkDate {
        public:

            // ── Constructeurs ────────────────────────────────────────────────────

            /// Initialise avec la date système locale courante.
            NkDate() noexcept;

            /**
             * @Brief Construit une date spécifique avec validation.
             * @param year  [1601, 30827]
             * @param month [1, 12]
             * @param day   [1, DaysInMonth(year, month)]
             */
            NkDate(int32 year, int32 month, int32 day);

            NkDate(const NkDate&)            noexcept = default;
            NkDate& operator=(const NkDate&) noexcept = default;

            // ── Accesseurs ───────────────────────────────────────────────────────

            NKTIME_NODISCARD int32 GetYear()  const noexcept { return year;  }
            NKTIME_NODISCARD int32 GetMonth() const noexcept { return month; }
            NKTIME_NODISCARD int32 GetDay()   const noexcept { return day;   }

            // ── Mutateurs ────────────────────────────────────────────────────────

            void SetYear (int32 year);
            void SetMonth(int32 month);
            void SetDay  (int32 day);

            // ── Méthodes statiques ───────────────────────────────────────────────

            /// Date système locale — thread-safe (utilise localtime_r/localtime_s).
            NKTIME_NODISCARD static NkDate GetCurrent();

            /// Nombre de jours dans un mois (tient compte des années bissextiles).
            NKTIME_NODISCARD static int32  DaysInMonth(int32 year, int32 month);

            /// Règle grégorienne : (y%4==0 && y%100!=0) || y%400==0
            NKTIME_NODISCARD static bool   IsLeapYear(int32 year) noexcept;

            // ── Formatage ────────────────────────────────────────────────────────

            /// Format ISO 8601 : "YYYY-MM-DD"
            NKTIME_NODISCARD NkString ToString()     const;

            /// Nom du mois en français (ex: "Janvier").
            NKTIME_NODISCARD NkString GetMonthName() const;

            // ── Opérateurs de comparaison ────────────────────────────────────────

            NKTIME_NODISCARD bool operator==(const NkDate& o) const noexcept;
            NKTIME_NODISCARD bool operator!=(const NkDate& o) const noexcept;
            NKTIME_NODISCARD bool operator< (const NkDate& o) const noexcept;
            NKTIME_NODISCARD bool operator<=(const NkDate& o) const noexcept;
            NKTIME_NODISCARD bool operator> (const NkDate& o) const noexcept;
            NKTIME_NODISCARD bool operator>=(const NkDate& o) const noexcept;

            friend NkString ToString(const NkDate& d) { return d.ToString(); }

        private:
            int32 year  = 1970;
            int32 month = 1;
            int32 day   = 1;

            void Validate() const;
    };

} // namespace nkentseu