#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKContainers/NkContainers.h"
#include "NKPlatform/NkFoundationLog.h"

#include <chrono>
#include <map>
#include <stdio.h>

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

TEST_CASE(NKContainersMap, InsertFindContainsAndOrder) {
    NkMap<int, int> map;
    ASSERT_TRUE(map.Empty());

    map.Insert(3, 30);
    map.Insert(1, 10);
    map.Insert(2, 20);
    map.Insert(2, 200); // update existing key

    ASSERT_EQUAL(3, static_cast<int>(map.Size()));
    ASSERT_TRUE(map.Contains(1));
    ASSERT_TRUE(map.Contains(2));
    ASSERT_TRUE(map.Contains(3));
    ASSERT_FALSE(map.Contains(99));

    int* value2 = map.Find(2);
    ASSERT_NOT_NULL(value2);
    ASSERT_EQUAL(200, *value2);

    auto it = map.begin();
    ASSERT_TRUE(it != map.end());
    ASSERT_EQUAL(1, it->First);
    ++it;
    ASSERT_TRUE(it != map.end());
    ASSERT_EQUAL(2, it->First);
    ++it;
    ASSERT_TRUE(it != map.end());
    ASSERT_EQUAL(3, it->First);
}

TEST_CASE(NKContainersMap, BracketOperatorInsertDefault) {
    NkMap<int, int> map;
    ASSERT_EQUAL(0, map[7]);
    map[7] = 77;
    ASSERT_EQUAL(77, map[7]);
    ASSERT_EQUAL(1, static_cast<int>(map.Size()));
}

TEST_CASE(NKContainersBenchmark, MapVsStdMap) {
    constexpr int kCount = 20000;

    long long nkSum = 0;
    const double nkTimeNs = MeasureNs([&]() {
        NkMap<int, int> map;
        for (int i = 0; i < kCount; ++i) {
            map.Insert(i, i * 2);
        }
        for (int i = 0; i < kCount; ++i) {
            int* value = map.Find(i);
            if (value) {
                nkSum += *value;
            }
        }
    });

    long long stlSum = 0;
    const double stlTimeNs = MeasureNs([&]() {
        std::map<int, int> map;
        for (int i = 0; i < kCount; ++i) {
            map[i] = i * 2;
        }
        for (int i = 0; i < kCount; ++i) {
            auto it = map.find(i);
            if (it != map.end()) {
                stlSum += it->second;
            }
        }
    });

    ASSERT_TRUE(nkSum == stlSum);
    ASSERT_TRUE(nkTimeNs > 0.0);
    ASSERT_TRUE(stlTimeNs > 0.0);

    NK_FOUNDATION_LOG_INFO("[NKContainers Benchmark] NkMap vs std::map");
    NK_FOUNDATION_LOG_INFO("  NkMap     : %.2f ns total", nkTimeNs);
    NK_FOUNDATION_LOG_INFO("  std::map  : %.2f ns total", stlTimeNs);
}
