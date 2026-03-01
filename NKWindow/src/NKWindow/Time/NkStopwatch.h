#pragma once

#include "NkClock.h"

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

/**
 * @brief Simple elapsed-time accumulator.
 *
 * NkStopwatch can be started/stopped multiple times while preserving
 * accumulated elapsed time.
 */
class NkStopwatch {
public:
	NkStopwatch() = default;

	/// @brief Start timing if currently stopped.
	void Start() {
		if (mRunning)
			return;

		mStart = NkClock::Now();
		mRunning = true;
	}

	/// @brief Stop timing and accumulate elapsed time.
	void Stop() {
		if (!mRunning)
			return;

		mAccumulated = mAccumulated + NkClock::ToNkDuration(NkClock::Now() - mStart);
		mRunning = false;
	}

	/// @brief Reset accumulated time and stop.
	void Reset() {
		mAccumulated = NkDuration::Zero();
		mRunning = false;
	}

	/// @brief Reset and start immediately.
	void Restart() {
		mAccumulated = NkDuration::Zero();
		mStart = NkClock::Now();
		mRunning = true;
	}

	/// @brief True when stopwatch is currently running.
	bool IsRunning() const {
		return mRunning;
	}

	/// @brief Total elapsed time including current run segment.
	NkDuration Elapsed() const {
		if (!mRunning)
			return mAccumulated;

		return mAccumulated + NkClock::ToNkDuration(NkClock::Now() - mStart);
	}

private:
	NkClock::TimePoint mStart{};
	NkDuration mAccumulated = NkDuration::Zero();
	bool mRunning = false;
};

} // namespace nkentseu
