/**
 * @File    NkTime.cpp
 * @Brief   Implémentation de NkTime.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */

#include "pch.h"
#include "NKTime/NkTimes.h"
#include "NKCore/Assert/NkAssert.h"
#include <cstdio>
#include <ctime>
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

namespace nkentseu {

using namespace time;

    // =============================================================================
    //  Constructeurs
    // =============================================================================

    NkTime::NkTime() noexcept
        : mHour(0), mMinute(0), mSecond(0), mMillisecond(0), mNanosecond(0)
    {}

    NkTime::NkTime(int32 hour, int32 minute, int32 second,
                int32 millisecond, int32 nanosecond)
        : mHour(hour), mMinute(minute), mSecond(second)
        , mMillisecond(millisecond), mNanosecond(nanosecond)
    {
        Normalize();
    }

    NkTime::NkTime(int64 totalNanoseconds) noexcept {
        mNanosecond   = static_cast<int32>(totalNanoseconds % NANOSECONDS_PER_MILLISECOND);
        totalNanoseconds /= NANOSECONDS_PER_MILLISECOND;

        mMillisecond  = static_cast<int32>(totalNanoseconds % MILLISECONDS_PER_SECOND);
        totalNanoseconds /= MILLISECONDS_PER_SECOND;

        mSecond       = static_cast<int32>(totalNanoseconds % SECONDS_PER_MINUTE_I32);
        totalNanoseconds /= SECONDS_PER_MINUTE_I32;

        mMinute       = static_cast<int32>(totalNanoseconds % MINUTES_PER_HOUR);
        totalNanoseconds /= MINUTES_PER_HOUR;

        mHour         = static_cast<int32>(totalNanoseconds % HOURS_PER_DAY);
    }

    // =============================================================================
    //  Mutateurs
    // =============================================================================

    void NkTime::SetHour(int32 hour) {
        NKENTSEU_ASSERT_MSG(hour >= 0 && hour < HOURS_PER_DAY, "Hour must be in [0, 23]");
        mHour = hour;
    }
    void NkTime::SetMinute(int32 minute) {
        NKENTSEU_ASSERT_MSG(minute >= 0 && minute < MINUTES_PER_HOUR, "Minute must be in [0, 59]");
        mMinute = minute;
    }
    void NkTime::SetSecond(int32 second) {
        NKENTSEU_ASSERT_MSG(second >= 0 && second < SECONDS_PER_MINUTE_I32, "Second must be in [0, 59]");
        mSecond = second;
    }
    void NkTime::SetMillisecond(int32 millis) {
        NKENTSEU_ASSERT_MSG(millis >= 0 && millis < MILLISECONDS_PER_SECOND, "Millisecond must be in [0, 999]");
        mMillisecond = millis;
    }
    void NkTime::SetNanosecond(int32 nano) {
        NKENTSEU_ASSERT_MSG(nano >= 0 && nano < NANOSECONDS_PER_MILLISECOND, "Nanosecond must be in [0, 999999]");
        mNanosecond = nano;
    }

    // =============================================================================
    //  Opérateurs arithmétiques
    // =============================================================================

    NkTime& NkTime::operator+=(const NkTime& o) noexcept {
        *this = NkTime(static_cast<int64>(*this) + static_cast<int64>(o));
        return *this;
    }
    NkTime& NkTime::operator-=(const NkTime& o) noexcept {
        *this = NkTime(static_cast<int64>(*this) - static_cast<int64>(o));
        return *this;
    }
    NkTime NkTime::operator+(const NkTime& o) const noexcept {
        return NkTime(static_cast<int64>(*this) + static_cast<int64>(o));
    }
    NkTime NkTime::operator-(const NkTime& o) const noexcept {
        return NkTime(static_cast<int64>(*this) - static_cast<int64>(o));
    }

    // =============================================================================
    //  Comparaisons
    // =============================================================================

    bool NkTime::operator==(const NkTime& o) const noexcept { return static_cast<int64>(*this) == static_cast<int64>(o); }
    bool NkTime::operator!=(const NkTime& o) const noexcept { return !(*this == o); }
    bool NkTime::operator< (const NkTime& o) const noexcept { return static_cast<int64>(*this) <  static_cast<int64>(o); }
    bool NkTime::operator<=(const NkTime& o) const noexcept { return !(o < *this); }
    bool NkTime::operator> (const NkTime& o) const noexcept { return  (o < *this); }
    bool NkTime::operator>=(const NkTime& o) const noexcept { return !(*this < o); }

    // =============================================================================
    //  Conversions de type
    // =============================================================================

    NkTime::operator int64() const noexcept {
        const int64 totalSec =
            static_cast<int64>(mSecond)
            + static_cast<int64>(SECONDS_PER_MINUTE_I32) * (
                static_cast<int64>(mMinute)
                + static_cast<int64>(MINUTES_PER_HOUR) * static_cast<int64>(mHour));

        return static_cast<int64>(mNanosecond)
            + static_cast<int64>(mMillisecond) * static_cast<int64>(NANOSECONDS_PER_MILLISECOND)
            + totalSec * static_cast<int64>(NS_PER_SECOND);
    }

    NkTime::operator double() const noexcept {
        return static_cast<double>(static_cast<int64>(*this)) / 1.0e9;
    }

    NkTime::operator float() const noexcept {
        return static_cast<float>(static_cast<double>(*this));
    }

    // =============================================================================
    //  GetCurrent
    // =============================================================================

    NkTime NkTime::GetCurrent() {
        timespec ts = {};
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        FILETIME fileTime{};
        ::GetSystemTimeAsFileTime(&fileTime);

        ULARGE_INTEGER ticks{};
        ticks.LowPart = fileTime.dwLowDateTime;
        ticks.HighPart = fileTime.dwHighDateTime;

        constexpr uint64 kWinToUnixEpochOffsetNs = 11644473600ULL * 1000000000ULL;
        const uint64 nowNs = static_cast<uint64>(ticks.QuadPart) * 100ULL; // 100ns ticks
        const uint64 unixNs = (nowNs >= kWinToUnixEpochOffsetNs) ? (nowNs - kWinToUnixEpochOffsetNs) : 0ULL;

        ts.tv_sec = static_cast<time_t>(unixNs / NS_PER_SECOND);
        ts.tv_nsec = static_cast<long>(unixNs % NS_PER_SECOND);
    #else
        ::clock_gettime(CLOCK_REALTIME, &ts);
    #endif
        const time_t utcSec = static_cast<time_t>(ts.tv_sec);
        tm local = {};
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        ::localtime_s(&local, &utcSec);
    #else
        ::localtime_r(&utcSec, &local);
    #endif
        return NkTime(
            local.tm_hour,
            local.tm_min,
            local.tm_sec,
            static_cast<int32>(ts.tv_nsec / 1'000'000L),
            static_cast<int32>(ts.tv_nsec % 1'000'000L)
        );
    }

    // =============================================================================
    //  Formatage
    // =============================================================================

    NkString NkTime::ToString() const {
        char buf[32];
        ::snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d.%06d",
                mHour, mMinute, mSecond, mMillisecond,
                mNanosecond % NANOSECONDS_PER_MILLISECOND);
        return NkString(buf);
    }

    // =============================================================================
    //  Interne
    // =============================================================================

    void NkTime::Normalize() noexcept {
        mNanosecond   %= NANOSECONDS_PER_MILLISECOND;
        mMillisecond  %= MILLISECONDS_PER_SECOND;
        mSecond       %= SECONDS_PER_MINUTE_I32;
        mMinute       %= MINUTES_PER_HOUR;
        mHour         %= HOURS_PER_DAY;
    }

    void NkTime::Validate() const {
        NKENTSEU_ASSERT_MSG(mHour >= 0 && mHour < HOURS_PER_DAY,             "Invalid hour");
        NKENTSEU_ASSERT_MSG(mMinute >= 0 && mMinute < MINUTES_PER_HOUR,      "Invalid minute");
        NKENTSEU_ASSERT_MSG(mSecond >= 0 && mSecond < SECONDS_PER_MINUTE_I32,"Invalid second");
        NKENTSEU_ASSERT_MSG(mMillisecond >= 0 && mMillisecond < MILLISECONDS_PER_SECOND,   "Invalid millisecond");
        NKENTSEU_ASSERT_MSG(mNanosecond >= 0 && mNanosecond < NANOSECONDS_PER_MILLISECOND, "Invalid nanosecond");
    }

} // namespace nkentseu
