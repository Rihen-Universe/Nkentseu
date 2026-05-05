#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKContainers/Associative/NkPriorityQueue.h"
#include "NKMemory/NkAllocator.h"
#include "NKPlatform/NkFoundationLog.h"

#include <chrono>
#include <queue>
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

TEST_CASE(NKContainersPriorityQueue, PushPopAndTopOrder) {
    NkPriorityQueue<int> queue;
    ASSERT_TRUE(queue.Empty());

    queue.Push(3);
    queue.Push(1);
    queue.Push(9);
    queue.Push(2);

    ASSERT_EQUAL(4, static_cast<int>(queue.Size()));
    ASSERT_EQUAL(9, queue.Top());

    queue.Pop();
    ASSERT_EQUAL(3, queue.Top());

    queue.Pop();
    ASSERT_EQUAL(2, queue.Top());
}

TEST_CASE(NKContainersPriorityQueue, AllocatorInjection) {
    memory::NkMallocAllocator allocator;
    NkPriorityQueue<int, memory::NkAllocator> queue(&allocator);

    ASSERT_TRUE(queue.GetAllocator() == &allocator);
    queue.Push(42);
    queue.Push(7);
    ASSERT_EQUAL(42, queue.Top());
}

TEST_CASE(NKContainersBenchmark, PriorityQueueVsStdPriorityQueue) {
    constexpr int kCount = 100000;

    long long nkSum = 0;
    const double nkTimeNs = MeasureNs([&]() {
        NkPriorityQueue<int> queue;
        for (int i = 0; i < kCount; ++i) {
            queue.Push((i * 7) ^ (kCount - i));
        }
        while (!queue.Empty()) {
            nkSum += queue.Top();
            queue.Pop();
        }
    });

    long long stlSum = 0;
    const double stlTimeNs = MeasureNs([&]() {
        std::priority_queue<int> queue;
        for (int i = 0; i < kCount; ++i) {
            queue.push((i * 7) ^ (kCount - i));
        }
        while (!queue.empty()) {
            stlSum += queue.top();
            queue.pop();
        }
    });

    ASSERT_TRUE(nkSum == stlSum);
    ASSERT_TRUE(nkTimeNs > 0.0);
    ASSERT_TRUE(stlTimeNs > 0.0);

    NK_FOUNDATION_LOG_INFO("[NKContainers Benchmark] NkPriorityQueue vs std::priority_queue");
    NK_FOUNDATION_LOG_INFO("  NkPriorityQueue      : %.2f ns total", nkTimeNs);
    NK_FOUNDATION_LOG_INFO("  std::priority_queue  : %.2f ns total", stlTimeNs);
}
