/**
 * @File    NkDate.cpp
 * @Brief   Implémentation de NkDate.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */

#include "pch.h"
#include "NKTime/NkDate.h"
#include "NKCore/Assert/NkAssert.h"
#include <cstdio>
#include <ctime>

namespace nkentseu {

    // =============================================================================
    //  Constructeurs
    // =============================================================================

    NkDate::NkDate() noexcept {
        *this = GetCurrent();
    }

    NkDate::NkDate(int32 y, int32 m, int32 d)
        : year(y), month(m), day(d)
    {
        Validate();
    }

    // =============================================================================
    //  Validation
    // =============================================================================

    void NkDate::Validate() const {
        NKENTSEU_ASSERT_MSG(year >= 1601 && year <= 30827, "Year must be in [1601, 30827]");
        NKENTSEU_ASSERT_MSG(month >= 1 && month <= 12,     "Month must be in [1, 12]");
        const int32 maxDay = DaysInMonth(year, month);
        NKENTSEU_ASSERT_MSG(day >= 1 && day <= maxDay,     "Day invalid for current month/year");
    }

    // =============================================================================
    //  Mutateurs
    // =============================================================================

    void NkDate::SetYear(int32 y) {
        NKENTSEU_ASSERT_MSG(y >= 1601 && y <= 30827, "Year must be in [1601, 30827]");
        year = y;
        Validate();
    }

    void NkDate::SetMonth(int32 m) {
        NKENTSEU_ASSERT_MSG(m >= 1 && m <= 12, "Month must be in [1, 12]");
        month = m;
        Validate();
    }

    void NkDate::SetDay(int32 d) {
        day = d;
        Validate();
    }

    // =============================================================================
    //  Statiques
    // =============================================================================

    NkDate NkDate::GetCurrent() {
        time_t now = ::time(nullptr);
        tm ts = {};

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        ::localtime_s(&ts, &now);
    #else
        ::localtime_r(&now, &ts);
    #endif

        return NkDate(ts.tm_year + 1900, ts.tm_mon + 1, ts.tm_mday);
    }

    int32 NkDate::DaysInMonth(int32 year, int32 month) {
        NKENTSEU_ASSERT_MSG(month >= 1 && month <= 12, "Month must be in [1, 12]");
        static const int32 days[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
        if (month == 2 && IsLeapYear(year)) return 29;
        return days[month - 1];
    }

    bool NkDate::IsLeapYear(int32 year) noexcept {
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    }

    // =============================================================================
    //  Formatage
    // =============================================================================

    NkString NkDate::ToString() const {
        char buf[16];
        ::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
        return NkString(buf);
    }

    NkString NkDate::GetMonthName() const {
        static const char* months[12] = {
            "Janvier","Février","Mars","Avril","Mai","Juin",
            "Juillet","Août","Septembre","Octobre","Novembre","Décembre"
        };
        return NkString(months[month - 1]);
    }

    // =============================================================================
    //  Comparaisons
    // =============================================================================

    bool NkDate::operator==(const NkDate& o) const noexcept {
        return year == o.year && month == o.month && day == o.day;
    }
    bool NkDate::operator!=(const NkDate& o) const noexcept { return !(*this == o); }
    bool NkDate::operator< (const NkDate& o) const noexcept {
        if (year  != o.year)  return year  < o.year;
        if (month != o.month) return month < o.month;
        return day < o.day;
    }
    bool NkDate::operator<=(const NkDate& o) const noexcept { return !(o < *this); }
    bool NkDate::operator> (const NkDate& o) const noexcept { return  (o < *this); }
    bool NkDate::operator>=(const NkDate& o) const noexcept { return !(*this < o); }

} // namespace nkentseu