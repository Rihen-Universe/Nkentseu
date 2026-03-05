#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKTime/NkDuration.h"
#include "NKTime/NkClock.h"

using namespace nkentseu;

TEST_CASE(NKTimeSmoke, DurationConversions) {
    NkDuration dur = NkDuration::FromMilliseconds(static_cast<nkentseu::core::int64>(1500));
    ASSERT_EQUAL(1500, static_cast<int>(dur.ToMilliseconds()));
    ASSERT_EQUAL(1, static_cast<int>(dur.ToSeconds()));

    NkDuration sum = dur + NkDuration::FromMilliseconds(static_cast<nkentseu::core::int64>(500));
    ASSERT_EQUAL(2000, static_cast<int>(sum.ToMilliseconds()));
}

TEST_CASE(NKTimeSmoke, ClockNowProgresses) {
    const NkClock::TimePoint t0 = NkClock::Now();
    NkClock::Sleep(static_cast<nkentseu::core::int64>(1));
    const NkClock::TimePoint t1 = NkClock::Now();

    ASSERT_TRUE(t1 >= t0);
    ASSERT_TRUE((t1 - t0).ToNanoseconds() >= 0);
}
