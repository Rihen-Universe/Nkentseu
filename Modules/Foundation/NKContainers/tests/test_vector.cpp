#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKContainers/NkContainers.h"
#include "NKPlatform/NkFoundationLog.h"

#include <chrono>
#include <stdio.h>
#include <vector>

using namespace nkentseu;

namespace {

template <typename Fn>
double MeasureNs(Fn&& fn) {
    const auto t0 = std::chrono::high_resolution_clock::now();
    fn();
    const auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

} // namespace

TEST_CASE(NKContainersVector, PushBackAndAccess) {
    NkVector<int> values;
    ASSERT_TRUE(values.Empty());

    values.PushBack(10);
    values.PushBack(20);
    values.PushBack(30);

    ASSERT_EQUAL(3, static_cast<int>(values.Size()));
    ASSERT_EQUAL(10, values.Front());
    ASSERT_EQUAL(30, values.Back());
    ASSERT_EQUAL(20, values[1]);
}

TEST_CASE(NKContainersVector, InsertEraseAndResize) {
    NkVector<int> values;
    values.PushBack(1);
    values.PushBack(3);
    values.Insert(values.begin() + 1, 2);

    ASSERT_EQUAL(3, static_cast<int>(values.Size()));
    ASSERT_EQUAL(2, values[1]);

    values.Erase(values.begin() + 1);
    ASSERT_EQUAL(2, static_cast<int>(values.Size()));
    ASSERT_EQUAL(3, values[1]);

    values.Resize(5, 9);
    ASSERT_EQUAL(5, static_cast<int>(values.Size()));
    ASSERT_EQUAL(9, values[4]);

    values.Resize(1);
    ASSERT_EQUAL(1, static_cast<int>(values.Size()));
    ASSERT_EQUAL(1, values[0]);
}

TEST_CASE(NKContainersBenchmark, VectorVsStdVector) {
    constexpr int kCount = 200000;

    long long nkSum = 0;
    const double nkTimeNs = MeasureNs([&]() {
        NkVector<int> values;
        values.Reserve(kCount);
        for (int i = 0; i < kCount; ++i) {
            values.PushBack(i);
        }
        for (int i = 0; i < kCount; ++i) {
            nkSum += values[static_cast<NkVector<int>::SizeType>(i)];
        }
    });

    long long stlSum = 0;
    const double stlTimeNs = MeasureNs([&]() {
        std::vector<int> values;
        values.reserve(static_cast<std::size_t>(kCount));
        for (int i = 0; i < kCount; ++i) {
            values.push_back(i);
        }
        for (int i = 0; i < kCount; ++i) {
            stlSum += values[static_cast<std::size_t>(i)];
        }
    });

    ASSERT_TRUE(nkSum == stlSum);
    ASSERT_TRUE(nkTimeNs > 0.0);
    ASSERT_TRUE(stlTimeNs > 0.0);

    NK_FOUNDATION_LOG_INFO("[NKContainers Benchmark] NkVector vs std::vector");
    NK_FOUNDATION_LOG_INFO("  NkVector   : %.2f ns total", nkTimeNs);
    NK_FOUNDATION_LOG_INFO("  std::vector: %.2f ns total", stlTimeNs);
}
