// -----------------------------------------------------------------------------
// FICHIER: Core\Nkentseu\src\Nkentseu\Time\NkDuration.cpp
// DESCRIPTION: Duration implementation
// AUTEUR: Rihen
// DATE: 2026-02-10
// -----------------------------------------------------------------------------

#include "NkDuration.h"
#include <cmath>
#include <cstdio>
#include <limits>

namespace nkentseu {
        
        static const core::int64 NANOSECONDS_PER_MICROSECOND = 1000LL;
        static const core::int64 NANOSECONDS_PER_MILLISECOND = 1000000LL;
        static const core::int64 NANOSECONDS_PER_SECOND = 1000000000LL;
        static const core::int64 NANOSECONDS_PER_MINUTE = 60000000000LL;
        static const core::int64 NANOSECONDS_PER_HOUR = 3600000000000LL;
        static const core::int64 NANOSECONDS_PER_DAY = 86400000000000LL;
        
        NkDuration::NkDuration() : mNanoseconds(0) {
        }
        
        NkDuration::NkDuration(core::int64 nanoseconds) : mNanoseconds(nanoseconds) {
        }
        
        NkDuration::NkDuration(const NkDuration& other) : mNanoseconds(other.mNanoseconds) {
        }
        
        NkDuration& NkDuration::operator=(const NkDuration& other) {
            mNanoseconds = other.mNanoseconds;
            return *this;
        }
        
        // Factory methods
        NkDuration NkDuration::FromNanoseconds(core::int64 nanoseconds) {
            return NkDuration(nanoseconds);
        }
        
        NkDuration NkDuration::FromMicroseconds(core::int64 microseconds) {
            return NkDuration(microseconds * NANOSECONDS_PER_MICROSECOND);
        }
        
        NkDuration NkDuration::FromMicroseconds(core::float64 microseconds) {
            return NkDuration(static_cast<core::int64>(microseconds * NANOSECONDS_PER_MICROSECOND));
        }
        
        NkDuration NkDuration::FromMilliseconds(core::int64 milliseconds) {
            return NkDuration(milliseconds * NANOSECONDS_PER_MILLISECOND);
        }
        
        NkDuration NkDuration::FromMilliseconds(core::float64 milliseconds) {
            return NkDuration(static_cast<core::int64>(milliseconds * NANOSECONDS_PER_MILLISECOND));
        }
        
        NkDuration NkDuration::FromSeconds(core::int64 seconds) {
            return NkDuration(seconds * NANOSECONDS_PER_SECOND);
        }
        
        NkDuration NkDuration::FromSeconds(core::float64 seconds) {
            return NkDuration(static_cast<core::int64>(seconds * NANOSECONDS_PER_SECOND));
        }
        
        NkDuration NkDuration::FromMinutes(core::int64 minutes) {
            return NkDuration(minutes * NANOSECONDS_PER_MINUTE);
        }
        
        NkDuration NkDuration::FromMinutes(core::float64 minutes) {
            return NkDuration(static_cast<core::int64>(minutes * NANOSECONDS_PER_MINUTE));
        }
        
        NkDuration NkDuration::FromHours(core::int64 hours) {
            return NkDuration(hours * NANOSECONDS_PER_HOUR);
        }
        
        NkDuration NkDuration::FromHours(core::float64 hours) {
            return NkDuration(static_cast<core::int64>(hours * NANOSECONDS_PER_HOUR));
        }
        
        NkDuration NkDuration::FromDays(core::int64 days) {
            return NkDuration(days * NANOSECONDS_PER_DAY);
        }
        
        NkDuration NkDuration::FromDays(core::float64 days) {
            return NkDuration(static_cast<core::int64>(days * NANOSECONDS_PER_DAY));
        }
        
        // Conversion methods
        core::int64 NkDuration::ToNanoseconds() const {
            return mNanoseconds;
        }
        
        core::float64 NkDuration::ToMicrosecondsF() const {
            return static_cast<core::float64>(mNanoseconds) / NANOSECONDS_PER_MICROSECOND;
        }
        
        core::int64 NkDuration::ToMicroseconds() const {
            return mNanoseconds / NANOSECONDS_PER_MICROSECOND;
        }
        
        core::float64 NkDuration::ToMillisecondsF() const {
            return static_cast<core::float64>(mNanoseconds) / NANOSECONDS_PER_MILLISECOND;
        }
        
        core::int64 NkDuration::ToMilliseconds() const {
            return mNanoseconds / NANOSECONDS_PER_MILLISECOND;
        }
        
        core::float64 NkDuration::ToSecondsF() const {
            return static_cast<core::float64>(mNanoseconds) / NANOSECONDS_PER_SECOND;
        }
        
        core::int64 NkDuration::ToSeconds() const {
            return mNanoseconds / NANOSECONDS_PER_SECOND;
        }
        
        core::float64 NkDuration::ToMinutesF() const {
            return static_cast<core::float64>(mNanoseconds) / NANOSECONDS_PER_MINUTE;
        }
        
        core::int64 NkDuration::ToMinutes() const {
            return mNanoseconds / NANOSECONDS_PER_MINUTE;
        }
        
        core::float64 NkDuration::ToHoursF() const {
            return static_cast<core::float64>(mNanoseconds) / NANOSECONDS_PER_HOUR;
        }
        
        core::int64 NkDuration::ToHours() const {
            return mNanoseconds / NANOSECONDS_PER_HOUR;
        }
        
        core::float64 NkDuration::ToDaysF() const {
            return static_cast<core::float64>(mNanoseconds) / NANOSECONDS_PER_DAY;
        }
        
        core::int64 NkDuration::ToDays() const {
            return mNanoseconds / NANOSECONDS_PER_DAY;
        }
        
        // Arithmetic operations
        NkDuration NkDuration::operator+(const NkDuration& other) const {
            return NkDuration(mNanoseconds + other.mNanoseconds);
        }
        
        NkDuration NkDuration::operator-(const NkDuration& other) const {
            return NkDuration(mNanoseconds - other.mNanoseconds);
        }
        
        NkDuration NkDuration::operator*(core::float64 scalar) const {
            return NkDuration(static_cast<core::int64>(mNanoseconds * scalar));
        }
        
        NkDuration NkDuration::operator/(core::float64 scalar) const {
            return NkDuration(static_cast<core::int64>(mNanoseconds / scalar));
        }
        
        core::float64 NkDuration::operator/(const NkDuration& other) const {
            return static_cast<core::float64>(mNanoseconds) / other.mNanoseconds;
        }
        
        NkDuration& NkDuration::operator+=(const NkDuration& other) {
            mNanoseconds += other.mNanoseconds;
            return *this;
        }
        
        NkDuration& NkDuration::operator-=(const NkDuration& other) {
            mNanoseconds -= other.mNanoseconds;
            return *this;
        }
        
        NkDuration& NkDuration::operator*=(core::float64 scalar) {
            mNanoseconds = static_cast<core::int64>(mNanoseconds * scalar);
            return *this;
        }
        
        NkDuration& NkDuration::operator/=(core::float64 scalar) {
            mNanoseconds = static_cast<core::int64>(mNanoseconds / scalar);
            return *this;
        }
        
        NkDuration NkDuration::operator-() const {
            return NkDuration(-mNanoseconds);
        }
        
        // Comparison operators
        bool NkDuration::operator==(const NkDuration& other) const {
            return mNanoseconds == other.mNanoseconds;
        }
        
        bool NkDuration::operator!=(const NkDuration& other) const {
            return mNanoseconds != other.mNanoseconds;
        }
        
        bool NkDuration::operator<(const NkDuration& other) const {
            return mNanoseconds < other.mNanoseconds;
        }
        
        bool NkDuration::operator<=(const NkDuration& other) const {
            return mNanoseconds <= other.mNanoseconds;
        }
        
        bool NkDuration::operator>(const NkDuration& other) const {
            return mNanoseconds > other.mNanoseconds;
        }
        
        bool NkDuration::operator>=(const NkDuration& other) const {
            return mNanoseconds >= other.mNanoseconds;
        }
        
        // Utility methods
        NkDuration NkDuration::Abs() const {
            return NkDuration(mNanoseconds < 0 ? -mNanoseconds : mNanoseconds);
        }
        
        bool NkDuration::IsNegative() const {
            return mNanoseconds < 0;
        }
        
        bool NkDuration::IsZero() const {
            return mNanoseconds == 0;
        }
        
        bool NkDuration::IsPositive() const {
            return mNanoseconds > 0;
        }
        
        // String conversion
        std::string NkDuration::ToString() const {
            char buffer[128];
            
            if (mNanoseconds == 0) {
                return std::string("0s");
            }
            
            core::int64 abs_ns = mNanoseconds < 0 ? -mNanoseconds : mNanoseconds;
            
            if (abs_ns >= NANOSECONDS_PER_DAY) {
                core::float64 days = static_cast<core::float64>(mNanoseconds) / NANOSECONDS_PER_DAY;
                snprintf(buffer, sizeof(buffer), "%.2fd", days);
            } else if (abs_ns >= NANOSECONDS_PER_HOUR) {
                core::float64 hours = static_cast<core::float64>(mNanoseconds) / NANOSECONDS_PER_HOUR;
                snprintf(buffer, sizeof(buffer), "%.2fh", hours);
            } else if (abs_ns >= NANOSECONDS_PER_MINUTE) {
                core::float64 minutes = static_cast<core::float64>(mNanoseconds) / NANOSECONDS_PER_MINUTE;
                snprintf(buffer, sizeof(buffer), "%.2fmin", minutes);
            } else if (abs_ns >= NANOSECONDS_PER_SECOND) {
                core::float64 seconds = static_cast<core::float64>(mNanoseconds) / NANOSECONDS_PER_SECOND;
                snprintf(buffer, sizeof(buffer), "%.3fs", seconds);
            } else if (abs_ns >= NANOSECONDS_PER_MILLISECOND) {
                core::float64 ms = static_cast<core::float64>(mNanoseconds) / NANOSECONDS_PER_MILLISECOND;
                snprintf(buffer, sizeof(buffer), "%.3fms", ms);
            } else if (abs_ns >= NANOSECONDS_PER_MICROSECOND) {
                core::float64 us = static_cast<core::float64>(mNanoseconds) / NANOSECONDS_PER_MICROSECOND;
                snprintf(buffer, sizeof(buffer), "%.3fµs", us);
            } else {
                snprintf(buffer, sizeof(buffer), "%lldns", static_cast<long long>(mNanoseconds));
            }
            
            return std::string(buffer);
        }
        
        std::string NkDuration::ToStringPrecise() const {
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "%lld ns", static_cast<long long>(mNanoseconds));
            return std::string(buffer);
        }
        
        // Constants
        NkDuration NkDuration::Zero() {
            return NkDuration(0);
        }
        
        NkDuration NkDuration::Max() {
            return NkDuration(std::numeric_limits<core::int64>::max());
        }
        
        NkDuration NkDuration::Min() {
            return NkDuration(std::numeric_limits<core::int64>::min());
        }
        
        // Non-member operators
        NkDuration operator*(core::float64 scalar, const NkDuration& duration) {
            return duration * scalar;
        }
        
} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================