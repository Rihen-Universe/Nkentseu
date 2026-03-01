#pragma once

#include "NkDuration.h"
#include "NkPlatformDetect.h"

#include <chrono>
#include <thread>

#if defined(NKENTSEU_PLATFORM_WASM) && defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

/**
 * @brief Cross-platform steady clock utilities.
 *
 * NkClock centralizes time acquisition and sleep/yield primitives.
 * On WebAssembly, Sleep/Yield use emscripten cooperative APIs.
 */
class NkClock {
public:
	using Clock = std::chrono::steady_clock;
	using TimePoint = Clock::time_point;

	/// @brief Current monotonic timestamp.
	static TimePoint Now() {
		return Clock::now();
	}

	/// @brief Elapsed duration since @p start.
	static NkDuration ElapsedSince(const TimePoint &start) {
		return ToNkDuration(Clock::now() - start);
	}

	/// @brief Sleep for a duration (cooperative on WASM).
	static void Sleep(const NkDuration &duration) {
		const NkI64 nanoseconds = duration.ToNanoseconds();
		if (nanoseconds <= 0) {
			YieldThread();
			return;
		}

#if defined(NKENTSEU_PLATFORM_WASM) && defined(__EMSCRIPTEN__)
		const NkI64 millis = (nanoseconds + 999999LL) / 1000000LL;
		emscripten_sleep(static_cast<unsigned int>(millis > 0 ? millis : 1));
#else
		std::this_thread::sleep_for(std::chrono::nanoseconds(nanoseconds));
#endif
	}

	/// @brief Sleep for a number of milliseconds.
	static void SleepMilliseconds(NkU32 milliseconds) {
		if (milliseconds == 0) {
			YieldThread();
			return;
		}

		Sleep(NkDuration::FromMilliseconds(static_cast<NkI64>(milliseconds)));
	}

	/// @brief Yield execution to other tasks/threads.
	static void YieldThread() {
#if defined(NKENTSEU_PLATFORM_WASM) && defined(__EMSCRIPTEN__)
		emscripten_sleep(0);
#else
		std::this_thread::yield();
#endif
	}

	/// @brief Convert std::chrono duration to NkDuration.
	static NkDuration ToNkDuration(const Clock::duration &d) {
		return NkDuration::FromNanoseconds(std::chrono::duration_cast<std::chrono::nanoseconds>(d).count());
	}
};

} // namespace nkentseu
