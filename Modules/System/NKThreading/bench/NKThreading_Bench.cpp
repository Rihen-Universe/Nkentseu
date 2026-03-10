#include "NKThreading/NkMutex.h"
#include "NKThreading/NkThreadPool.h"
#include "NKCore/NkAtomic.h"

#include <cstdio>
#include "NKPlatform/NkFoundationLog.h"
#include <ctime>

using namespace nkentseu;
using namespace nkentseu::threading;


namespace {

volatile nk_uint64 gNkThreadingBenchSink = 0u;

nk_uint64 NkNowNs() {
    timespec ts {};
    timespec_get(&ts, TIME_UTC);
    return (static_cast<nk_uint64>(ts.tv_sec) * 1000000000ull) + static_cast<nk_uint64>(ts.tv_nsec);
}

template <typename Func>
double NkMeasureMeanNs(Func&& func, nk_size iterations) {
    const nk_uint64 t0 = NkNowNs();
    for (nk_size i = 0u; i < iterations; ++i) {
        func();
    }
    const nk_uint64 t1 = NkNowNs();
    const double totalNs = static_cast<double>(t1 - t0);
    return totalNs / static_cast<double>(iterations);
}

void NkBenchMutex() {
    constexpr nk_size kIterations = 1000000u;

    NkMutex nkMutex;
    const double nkNs = NkMeasureMeanNs([&]() {
        nkMutex.Lock();
        gNkThreadingBenchSink = gNkThreadingBenchSink + 1u;
        nkMutex.Unlock();
    }, kIterations);

    NK_FOUNDATION_LOG_INFO("\n[NKThreading Bench] NkMutex\n");
    NK_FOUNDATION_LOG_INFO("  NkMutex mean ns/op          : %.4f\n", nkNs);
}

void NkBenchThreadPoolDispatch() {
    constexpr nk_uint32 kTasks = 100000u;
    NkThreadPool pool(0u);

    NkAtomicUInt32 done(0u);

    const nk_uint64 t0 = NkNowNs();
    for (nk_uint32 i = 0u; i < kTasks; ++i) {
        pool.Enqueue([&done]() {
            done.FetchAdd(1u, NkMemoryOrder::NK_RELAXED);
        });
    }
    pool.Join();
    const nk_uint64 t1 = NkNowNs();
    pool.Shutdown();

    const nk_uint32 doneValue = done.Load(NkMemoryOrder::NK_RELAXED);
    const double totalNs = static_cast<double>(t1 - t0);
    const double perTaskNs = totalNs / static_cast<double>(kTasks);

    gNkThreadingBenchSink = gNkThreadingBenchSink + static_cast<nk_uint64>(doneValue);

    NK_FOUNDATION_LOG_INFO("\n[NKThreading Bench] NkThreadPool dispatch\n");
    NK_FOUNDATION_LOG_INFO("  tasks                        : %u\n", static_cast<unsigned int>(kTasks));
    NK_FOUNDATION_LOG_INFO("  completed                    : %u\n", static_cast<unsigned int>(doneValue));
    NK_FOUNDATION_LOG_INFO("  total time                   : %.2f ms\n", totalNs / 1000000.0);
    NK_FOUNDATION_LOG_INFO("  dispatch+exec ns/task        : %.4f\n", perTaskNs);
}

} // namespace

int main() {
    NK_FOUNDATION_LOG_INFO("\n[NKThreading Bench] start\n");
    NkBenchMutex();
    NkBenchThreadPoolDispatch();
    NK_FOUNDATION_LOG_INFO("\n[NKThreading Bench] sink=%llu\n",
             static_cast<unsigned long long>(gNkThreadingBenchSink));
    return 0;
}

