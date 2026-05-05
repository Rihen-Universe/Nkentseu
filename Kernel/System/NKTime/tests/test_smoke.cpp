#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKTime/NkTime.h"

using namespace nkentseu;

TEST_CASE(NKTimeSmoke, DurationConversions) {
    NkDuration dur = NkDuration::FromMilliseconds(static_cast<nkentseu::int64>(1500));
    ASSERT_EQUAL(1500, static_cast<int>(dur.ToMilliseconds()));
    ASSERT_EQUAL(1, static_cast<int>(dur.ToSeconds()));

    NkDuration sum = dur + NkDuration::FromMilliseconds(static_cast<nkentseu::int64>(500));
    ASSERT_EQUAL(2000, static_cast<int>(sum.ToMilliseconds()));
}

TEST_CASE(NKTimeSmoke, ClockNowProgresses) {
    const NkElapsedTime t0 = NkChrono::Now();
    NkChrono::Sleep(static_cast<nkentseu::int64>(1));
    const NkElapsedTime t1 = NkChrono::Now();

    ASSERT_TRUE(t1 >= t0);
    ASSERT_TRUE((t1 - t0).ToNanoseconds() >= 0.0);
}

TEST_CASE(NKTimeSmoke, ChronoNowAndElapsedComparisons) {
    NkChrono chrono;
    const NkElapsedTime tp0 = NkChrono::Now();

    NkChrono::Sleep(static_cast<nkentseu::int64>(2));
    const NkElapsedTime e0 = chrono.Elapsed();
    const NkElapsedTime tp1 = NkChrono::Now();

    NkChrono::Sleep(static_cast<nkentseu::int64>(2));
    const NkElapsedTime e1 = chrono.Elapsed();

    ASSERT_TRUE(tp1 >= tp0);
    ASSERT_TRUE(e1 >= e0);
    ASSERT_TRUE(e1 > e0 || e1 == e0);
}

TEST_CASE(NKTimeSmoke, TimeZoneUtcAndFixedOffset) {
    const NkDate sampleDate(2026, 3, 6);
    const NkTime utcTime(10, 0, 0);

    const NkTimeZone utcZone = NkTimeZone::GetUtc();
    ASSERT_TRUE(utcZone.ToLocal(sampleDate) == sampleDate);
    ASSERT_TRUE(utcZone.ToUtc(sampleDate) == sampleDate);
    const NkTime utcAsLocal = utcZone.ToLocal(utcTime, sampleDate);
    ASSERT_EQUAL(utcTime.GetHour(), utcAsLocal.GetHour());
    ASSERT_EQUAL(utcTime.GetMinute(), utcAsLocal.GetMinute());
    ASSERT_EQUAL(utcTime.GetSecond(), utcAsLocal.GetSecond());
    const NkTime utcRoundTrip = utcZone.ToUtc(utcTime, sampleDate);
    ASSERT_EQUAL(utcTime.GetHour(), utcRoundTrip.GetHour());
    ASSERT_EQUAL(utcTime.GetMinute(), utcRoundTrip.GetMinute());
    ASSERT_EQUAL(utcTime.GetSecond(), utcRoundTrip.GetSecond());
    ASSERT_EQUAL(0, static_cast<int>(utcZone.GetUtcOffset(sampleDate).ToNanoseconds()));

    const NkTimeZone fixedZone = NkTimeZone::FromName("UTC+02:30");
    const nkentseu::int64 offsetSeconds =
        fixedZone.GetUtcOffset(sampleDate).ToNanoseconds() / 1000000000LL;
    ASSERT_EQUAL(9000, static_cast<int>(offsetSeconds));

    const NkTime localTime = fixedZone.ToLocal(utcTime, sampleDate);
    ASSERT_EQUAL(12, localTime.GetHour());
    ASSERT_EQUAL(30, localTime.GetMinute());

    const NkTime roundTripUtc = fixedZone.ToUtc(localTime, sampleDate);
    ASSERT_EQUAL(utcTime.GetHour(), roundTripUtc.GetHour());
    ASSERT_EQUAL(utcTime.GetMinute(), roundTripUtc.GetMinute());
}
