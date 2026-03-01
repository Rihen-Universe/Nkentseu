#pragma once

#include "../Core/NkTypes.h"

#include <cmath>
#include <string>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

/**
 * @brief Immutable duration value stored in nanoseconds.
 *
 * NkDuration is the base time unit used by NkClock and NkStopwatch.
 * It supports integer and floating-point factories, conversions, and
 * arithmetic operations.
 */
class NkDuration {
public:
	/// @brief Create a zero duration.
	constexpr NkDuration() = default;

	/// @brief Create a duration from raw nanoseconds.
	explicit constexpr NkDuration(NkI64 nanoseconds) : mNanoseconds(nanoseconds) {
	}

	/// @brief Return a zero duration constant.
	static constexpr NkDuration Zero() {
		return NkDuration(0);
	}
	/// @brief Build from nanoseconds.
	static constexpr NkDuration FromNanoseconds(NkI64 ns) {
		return NkDuration(ns);
	}
	/// @brief Build from microseconds.
	static constexpr NkDuration FromMicroseconds(NkI64 us) {
		return NkDuration(us * 1000);
	}
	/// @brief Build from milliseconds.
	static constexpr NkDuration FromMilliseconds(NkI64 ms) {
		return NkDuration(ms * 1000 * 1000);
	}
	/// @brief Build from seconds.
	static constexpr NkDuration FromSeconds(NkI64 s) {
		return NkDuration(s * 1000 * 1000 * 1000);
	}

	/// @brief Build from floating microseconds (rounded).
	static NkDuration FromMicroseconds(double us) {
		return NkDuration(static_cast<NkI64>(std::llround(us * 1000.0)));
	}

	/// @brief Build from floating milliseconds (rounded).
	static NkDuration FromMilliseconds(double ms) {
		return NkDuration(static_cast<NkI64>(std::llround(ms * 1000.0 * 1000.0)));
	}

	/// @brief Build from floating seconds (rounded).
	static NkDuration FromSeconds(double s) {
		return NkDuration(static_cast<NkI64>(std::llround(s * 1000.0 * 1000.0 * 1000.0)));
	}

	/// @brief Convert to nanoseconds.
	constexpr NkI64 ToNanoseconds() const {
		return mNanoseconds;
	}
	/// @brief Convert to microseconds (integer truncation).
	constexpr NkI64 ToMicroseconds() const {
		return mNanoseconds / 1000;
	}
	/// @brief Convert to milliseconds (integer truncation).
	constexpr NkI64 ToMilliseconds() const {
		return mNanoseconds / (1000 * 1000);
	}

	/// @brief Convert to seconds as double precision float.
	double ToSeconds() const {
		return static_cast<double>(mNanoseconds) / (1000.0 * 1000.0 * 1000.0);
	}

	/// @brief Human-readable representation (ns/us/ms/s).
	std::string ToString() const {
		if (std::llabs(mNanoseconds) >= 1000LL * 1000LL * 1000LL)
			return std::to_string(ToSeconds()) + "s";
		if (std::llabs(mNanoseconds) >= 1000LL * 1000LL)
			return std::to_string(ToMilliseconds()) + "ms";
		if (std::llabs(mNanoseconds) >= 1000LL)
			return std::to_string(ToMicroseconds()) + "us";
		return std::to_string(ToNanoseconds()) + "ns";
	}

	constexpr NkDuration operator+(const NkDuration &other) const {
		return NkDuration(mNanoseconds + other.mNanoseconds);
	}

	constexpr NkDuration operator-(const NkDuration &other) const {
		return NkDuration(mNanoseconds - other.mNanoseconds);
	}

	/// @brief Scale duration by a floating scalar.
	NkDuration operator*(double scalar) const {
		return NkDuration(static_cast<NkI64>(std::llround(static_cast<double>(mNanoseconds) * scalar)));
	}

	/// @brief Divide duration by a floating scalar.
	NkDuration operator/(double scalar) const {
		return NkDuration(static_cast<NkI64>(std::llround(static_cast<double>(mNanoseconds) / scalar)));
	}

	constexpr bool operator==(const NkDuration &other) const {
		return mNanoseconds == other.mNanoseconds;
	}
	constexpr bool operator!=(const NkDuration &other) const {
		return mNanoseconds != other.mNanoseconds;
	}
	constexpr bool operator<(const NkDuration &other) const {
		return mNanoseconds < other.mNanoseconds;
	}
	constexpr bool operator<=(const NkDuration &other) const {
		return mNanoseconds <= other.mNanoseconds;
	}
	constexpr bool operator>(const NkDuration &other) const {
		return mNanoseconds > other.mNanoseconds;
	}
	constexpr bool operator>=(const NkDuration &other) const {
		return mNanoseconds >= other.mNanoseconds;
	}

private:
	NkI64 mNanoseconds = 0;
};

} // namespace nkentseu
