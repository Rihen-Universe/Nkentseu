/**
 * @File    NkTimeSpan.cpp
 * @Brief   Implémentation de NkTimeSpan.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */

#include "pch.h"
#include "NKTime/NkTimeSpan.h"
#include <cstdio>

namespace nkentseu {

    using namespace time;

    // =============================================================================
    //  Constructeurs
    // =============================================================================

    NkTimeSpan::NkTimeSpan() noexcept
        : mTotalNanoseconds(0)
    {}

    NkTimeSpan::NkTimeSpan(int64 days, int64 hours, int64 minutes, int64 seconds,
                        int64 milliseconds, int64 nanoseconds) noexcept
    {
        mTotalNanoseconds =
            days         * NS_PER_DAY
            + hours        * NS_PER_HOUR
            + minutes      * NS_PER_MINUTE
            + seconds      * NS_PER_SECOND
            + milliseconds * NS_PER_MILLISECOND
            + nanoseconds;
    }

    // =============================================================================
    //  Fabriques statiques
    // =============================================================================

    NkTimeSpan NkTimeSpan::FromDays        (int64 d) noexcept { return NkTimeSpan(d * NS_PER_DAY);         }
    NkTimeSpan NkTimeSpan::FromHours       (int64 h) noexcept { return NkTimeSpan(h * NS_PER_HOUR);        }
    NkTimeSpan NkTimeSpan::FromMinutes     (int64 m) noexcept { return NkTimeSpan(m * NS_PER_MINUTE);      }
    NkTimeSpan NkTimeSpan::FromSeconds     (int64 s) noexcept { return NkTimeSpan(s * NS_PER_SECOND);      }
    NkTimeSpan NkTimeSpan::FromMilliseconds(int64 ms)noexcept { return NkTimeSpan(ms * NS_PER_MILLISECOND);}
    NkTimeSpan NkTimeSpan::FromNanoseconds (int64 ns)noexcept { return NkTimeSpan(ns);                     }

    // =============================================================================
    //  Composants décomposés
    // =============================================================================

    int64 NkTimeSpan::GetDays()         const noexcept { return mTotalNanoseconds / NS_PER_DAY; }
    int64 NkTimeSpan::GetHours()        const noexcept { return (mTotalNanoseconds % NS_PER_DAY)     / NS_PER_HOUR;        }
    int64 NkTimeSpan::GetMinutes()      const noexcept { return (mTotalNanoseconds % NS_PER_HOUR)    / NS_PER_MINUTE;      }
    int64 NkTimeSpan::GetSeconds()      const noexcept { return (mTotalNanoseconds % NS_PER_MINUTE)  / NS_PER_SECOND;      }
    int64 NkTimeSpan::GetMilliseconds() const noexcept { return (mTotalNanoseconds % NS_PER_SECOND)  / NS_PER_MILLISECOND; }
    int64 NkTimeSpan::GetNanoseconds()  const noexcept { return  mTotalNanoseconds % NS_PER_MILLISECOND; }

    double NkTimeSpan::ToSeconds() const noexcept {
        return static_cast<double>(mTotalNanoseconds) / static_cast<double>(NS_PER_SECOND);
    }

    // =============================================================================
    //  Opérateurs arithmétiques
    // =============================================================================

    NkTimeSpan NkTimeSpan::operator+(const NkTimeSpan& o) const noexcept { return NkTimeSpan(mTotalNanoseconds + o.mTotalNanoseconds); }
    NkTimeSpan NkTimeSpan::operator-(const NkTimeSpan& o) const noexcept { return NkTimeSpan(mTotalNanoseconds - o.mTotalNanoseconds); }
    NkTimeSpan NkTimeSpan::operator*(double f) const noexcept { return NkTimeSpan(static_cast<int64>(static_cast<double>(mTotalNanoseconds) * f)); }
    NkTimeSpan NkTimeSpan::operator/(double d) const noexcept {
        if (d == 0.0) return NkTimeSpan(0LL);
        return NkTimeSpan(static_cast<int64>(static_cast<double>(mTotalNanoseconds) / d));
    }

    NkTimeSpan& NkTimeSpan::operator+=(const NkTimeSpan& o) noexcept { return Add(o);      }
    NkTimeSpan& NkTimeSpan::operator-=(const NkTimeSpan& o) noexcept { return Subtract(o); }
    NkTimeSpan& NkTimeSpan::operator*=(double f)            noexcept { return Multiply(f);  }
    NkTimeSpan& NkTimeSpan::operator/=(double d)            noexcept { return Divide(d);    }

    NkTimeSpan& NkTimeSpan::Add     (const NkTimeSpan& o) noexcept { mTotalNanoseconds += o.mTotalNanoseconds; return *this; }
    NkTimeSpan& NkTimeSpan::Subtract(const NkTimeSpan& o) noexcept { mTotalNanoseconds -= o.mTotalNanoseconds; return *this; }
    NkTimeSpan& NkTimeSpan::Multiply(double f)            noexcept { mTotalNanoseconds = static_cast<int64>(static_cast<double>(mTotalNanoseconds) * f); return *this; }
    NkTimeSpan& NkTimeSpan::Divide  (double d)            noexcept {
        if (d != 0.0) mTotalNanoseconds = static_cast<int64>(static_cast<double>(mTotalNanoseconds) / d);
        return *this;
    }

    // =============================================================================
    //  Comparaisons
    // =============================================================================

    bool NkTimeSpan::operator==(const NkTimeSpan& o) const noexcept { return mTotalNanoseconds == o.mTotalNanoseconds; }
    bool NkTimeSpan::operator!=(const NkTimeSpan& o) const noexcept { return !(*this == o); }
    bool NkTimeSpan::operator< (const NkTimeSpan& o) const noexcept { return mTotalNanoseconds <  o.mTotalNanoseconds; }
    bool NkTimeSpan::operator<=(const NkTimeSpan& o) const noexcept { return !(o < *this); }
    bool NkTimeSpan::operator> (const NkTimeSpan& o) const noexcept { return  (o < *this); }
    bool NkTimeSpan::operator>=(const NkTimeSpan& o) const noexcept { return !(*this < o); }

    // =============================================================================
    //  Extraction calendaire
    // =============================================================================

    NkTime NkTimeSpan::GetTime() const noexcept {
        int64 ns = mTotalNanoseconds % NS_PER_DAY;
        if (ns < 0) ns += NS_PER_DAY;

        const int32 h  = static_cast<int32>(ns / NS_PER_HOUR);   ns %= NS_PER_HOUR;
        const int32 m  = static_cast<int32>(ns / NS_PER_MINUTE); ns %= NS_PER_MINUTE;
        const int32 s  = static_cast<int32>(ns / NS_PER_SECOND); ns %= NS_PER_SECOND;
        const int32 ms = static_cast<int32>(ns / NS_PER_MILLISECOND);
        const int32 nano = static_cast<int32>(ns % NS_PER_MILLISECOND);
        return NkTime(h, m, s, ms, nano);
    }

    NkDate NkTimeSpan::GetDate() const noexcept {
        // Algorithme de Howard Hinnant — domaine public
        int64 days = mTotalNanoseconds / NS_PER_DAY;
        days += 719468LL;

        const int64 era = (days >= 0 ? days : days - 146096) / 146097;
        const int64 doe = days - era * 146097;
        const int64 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        const int64 y   = yoe + era * 400;
        const int64 doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        const int64 mp  = (5 * doy + 2) / 153;

        const int32 day   = static_cast<int32>(doy - (153 * mp + 2) / 5 + 1);
        const int32 month = static_cast<int32>(mp < 10 ? mp + 3 : mp - 9);
        const int32 year  = static_cast<int32>(y + (month <= 2 ? 1 : 0));

        return NkDate(year, month, day);
    }

    // =============================================================================
    //  Formatage
    // =============================================================================

    NkString NkTimeSpan::ToString() const {
        const int64 absNs = mTotalNanoseconds < 0 ? -mTotalNanoseconds : mTotalNanoseconds;
        int64 rem = absNs;

        const int64 days  = rem / NS_PER_DAY;         rem %= NS_PER_DAY;
        const int64 hours = rem / NS_PER_HOUR;         rem %= NS_PER_HOUR;
        const int64 mins  = rem / NS_PER_MINUTE;       rem %= NS_PER_MINUTE;
        const int64 secs  = rem / NS_PER_SECOND;       rem %= NS_PER_SECOND;
        const int64 ms    = rem / NS_PER_MILLISECOND;
        const int64 ns    = rem % NS_PER_MILLISECOND;

        char buf[160];
        int  off = 0;

        if (mTotalNanoseconds < 0) buf[off++] = '-';
        if (days > 0) off += ::snprintf(buf + off, sizeof(buf) - static_cast<size_t>(off),
                                        "%lldj ", static_cast<long long>(days));

        off += ::snprintf(buf + off, sizeof(buf) - static_cast<size_t>(off),
                        "%02lld:%02lld:%02lld",
                        static_cast<long long>(hours),
                        static_cast<long long>(mins),
                        static_cast<long long>(secs));

        if (ms > 0 || ns > 0) {
            off += ::snprintf(buf + off, sizeof(buf) - static_cast<size_t>(off),
                            ".%03lld", static_cast<long long>(ms));
            if (ns > 0)
                ::snprintf(buf + off, sizeof(buf) - static_cast<size_t>(off),
                        ".%06lld", static_cast<long long>(ns));
        }

        return NkString(buf);
    }

} // namespace nkentseu