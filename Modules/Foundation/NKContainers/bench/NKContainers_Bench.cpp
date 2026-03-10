#include "NKContainers/NkContainers.h"
#include "NKContainers/Associative/NkPriorityQueue.h"

#include <chrono>
#include <cstdio>
#include "NKPlatform/NkFoundationLog.h"
#include <map>
#include <queue>
#include <vector>

using namespace nkentseu;


namespace {

volatile nk_int64 gNkContainersBenchSink = 0;

template <typename Func>
double NkMeasureMeanNs(Func&& func, nk_size iterations) {
    const auto t0 = std::chrono::steady_clock::now();
    for (nk_size i = 0u; i < iterations; ++i) {
        func();
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double totalNs = std::chrono::duration<double, std::nano>(t1 - t0).count();
    return totalNs / static_cast<double>(iterations);
}

void NkBenchVectorVsStdVector() {
    constexpr nk_size kCount = 200000u;

    nk_int64 nkSum = 0;
    const double nkNs = NkMeasureMeanNs([&]() {
        NkVector<int> values;
        values.Reserve(kCount);
        for (nk_size i = 0u; i < kCount; ++i) {
            values.PushBack(static_cast<int>(i));
        }
        for (nk_size i = 0u; i < kCount; ++i) {
            nkSum += values[static_cast<NkVector<int>::SizeType>(i)];
        }
    }, 1u);

    nk_int64 stlSum = 0;
    const double stlNs = NkMeasureMeanNs([&]() {
        std::vector<int> values;
        values.reserve(static_cast<std::size_t>(kCount));
        for (nk_size i = 0u; i < kCount; ++i) {
            values.push_back(static_cast<int>(i));
        }
        for (nk_size i = 0u; i < kCount; ++i) {
            stlSum += values[static_cast<std::size_t>(i)];
        }
    }, 1u);

    gNkContainersBenchSink = gNkContainersBenchSink + nkSum + stlSum;

    NK_FOUNDATION_LOG_INFO("\n[NKContainers Bench] NkVector vs std::vector\n");
    NK_FOUNDATION_LOG_INFO("  NkVector total ns          : %.2f\n", nkNs);
    NK_FOUNDATION_LOG_INFO("  std::vector total ns       : %.2f\n", stlNs);
    NK_FOUNDATION_LOG_INFO("  ratio Nk/std               : %.4f\n",
                (stlNs > 0.0) ? (nkNs / stlNs) : 0.0);
}

void NkBenchMapVsStdMap() {
    constexpr int kCount = 30000;

    nk_int64 nkSum = 0;
    const double nkNs = NkMeasureMeanNs([&]() {
        NkMap<int, int> map;
        for (int i = 0; i < kCount; ++i) {
            map.Insert(i, i * 3);
        }
        for (int i = 0; i < kCount; ++i) {
            int* value = map.Find(i);
            if (value) {
                nkSum += *value;
            }
        }
    }, 1u);

    nk_int64 stlSum = 0;
    const double stlNs = NkMeasureMeanNs([&]() {
        std::map<int, int> map;
        for (int i = 0; i < kCount; ++i) {
            map[i] = i * 3;
        }
        for (int i = 0; i < kCount; ++i) {
            std::map<int, int>::iterator it = map.find(i);
            if (it != map.end()) {
                stlSum += it->second;
            }
        }
    }, 1u);

    gNkContainersBenchSink = gNkContainersBenchSink + nkSum + stlSum;

    NK_FOUNDATION_LOG_INFO("\n[NKContainers Bench] NkMap vs std::map\n");
    NK_FOUNDATION_LOG_INFO("  NkMap total ns             : %.2f\n", nkNs);
    NK_FOUNDATION_LOG_INFO("  std::map total ns          : %.2f\n", stlNs);
    NK_FOUNDATION_LOG_INFO("  ratio Nk/std               : %.4f\n",
                (stlNs > 0.0) ? (nkNs / stlNs) : 0.0);
}

void NkBenchPriorityQueueVsStdPriorityQueue() {
    constexpr int kCount = 120000;

    nk_int64 nkSum = 0;
    const double nkNs = NkMeasureMeanNs([&]() {
        NkPriorityQueue<int> queue;
        for (int i = 0; i < kCount; ++i) {
            queue.Push((i * 13) ^ (kCount - i));
        }
        while (!queue.Empty()) {
            nkSum += queue.Top();
            queue.Pop();
        }
    }, 1u);

    nk_int64 stlSum = 0;
    const double stlNs = NkMeasureMeanNs([&]() {
        std::priority_queue<int> queue;
        for (int i = 0; i < kCount; ++i) {
            queue.push((i * 13) ^ (kCount - i));
        }
        while (!queue.empty()) {
            stlSum += queue.top();
            queue.pop();
        }
    }, 1u);

    gNkContainersBenchSink = gNkContainersBenchSink + nkSum + stlSum;

    NK_FOUNDATION_LOG_INFO("\n[NKContainers Bench] NkPriorityQueue vs std::priority_queue\n");
    NK_FOUNDATION_LOG_INFO("  NkPriorityQueue total ns   : %.2f\n", nkNs);
    NK_FOUNDATION_LOG_INFO("  std::priority_queue total ns: %.2f\n", stlNs);
    NK_FOUNDATION_LOG_INFO("  ratio Nk/std               : %.4f\n",
                (stlNs > 0.0) ? (nkNs / stlNs) : 0.0);
}

} // namespace

int main() {
    NK_FOUNDATION_LOG_INFO("\n[NKContainers Bench] start\n");
    NkBenchVectorVsStdVector();
    NkBenchMapVsStdMap();
    NkBenchPriorityQueueVsStdPriorityQueue();
    NK_FOUNDATION_LOG_INFO("\n[NKContainers Bench] sink=%lld\n",
                static_cast<long long>(gNkContainersBenchSink));
    return 0;
}

