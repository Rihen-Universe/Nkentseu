#include <Unitest/TestMacro.h>
#include <Unitest/Unitest.h>

#include "NKThreading/NkMutex.h"
#include "NKThreading/NkSemaphore.h"
#include "NKThreading/NkThread.h"
#include "NKThreading/NkThreadPool.h"

using namespace nkentseu::threading;

TEST_CASE(NKThreadingSemaphore, TryAcquireAndRelease) {
    NkSemaphore semaphore(0u, 2u);

    ASSERT_FALSE(semaphore.TryAcquire());
    ASSERT_TRUE(semaphore.Release(1u));
    ASSERT_TRUE(semaphore.TryAcquire());
    ASSERT_FALSE(semaphore.TryAcquire());
    ASSERT_EQUAL(0u, semaphore.GetCount());
}

TEST_CASE(NKThreadingSemaphore, TimedAcquire) {
    NkSemaphore semaphore(0u, 1u);

    ASSERT_FALSE(semaphore.TryAcquireFor(5u));

    NkThread releaser([&semaphore]() {
        (void)semaphore.Release(1u);
    });

    ASSERT_TRUE(semaphore.TryAcquireFor(250u));
    releaser.Join();
}

TEST_CASE(NKThreadingThreadPool, EnqueueJoinAndCounters) {
    NkThreadPool pool(2u);
    NkMutex counterMutex;

    nkentseu::nk_uint32 counter = 0u;
    constexpr nkentseu::nk_uint32 taskCount = 128u;

    for (nkentseu::nk_uint32 i = 0u; i < taskCount; ++i) {
        pool.Enqueue([&counterMutex, &counter]() {
            NkScopedLock lock(counterMutex);
            ++counter;
        });
    }

    pool.EnqueuePriority([&counterMutex, &counter]() {
        NkScopedLock lock(counterMutex);
        ++counter;
    });

    pool.EnqueueAffinity([&counterMutex, &counter]() {
        NkScopedLock lock(counterMutex);
        ++counter;
    }, 0u);

    pool.Join();

    ASSERT_EQUAL(taskCount + 2u, counter);
    ASSERT_EQUAL(static_cast<nkentseu::nk_uint64>(taskCount + 2u), pool.GetTasksCompleted());

    pool.Shutdown();
}
