#include <Unitest/TestMacro.h>
#include <Unitest/Unitest.h>

#include "NKThreading/NkMutex.h"
#include "NKThreading/NkThread.h"
#include "NKThreading/Synchronization/NkBarrier.h"
#include "NKThreading/Synchronization/NkEvent.h"
#include "NKThreading/Synchronization/NkLatch.h"
#include "NKThreading/Synchronization/NkReaderWriterLock.h"

using namespace nkentseu::threading;

TEST_CASE(NKThreadingSync, BarrierPhasesAndLeader) {
    constexpr nkentseu::nk_uint32 kThreads = 4u;
    constexpr nkentseu::nk_uint32 kPhases = 6u;

    NkBarrier barrier(kThreads);
    NkMutex statsMutex;

    nkentseu::nk_uint32 beforeCount = 0u;
    nkentseu::nk_uint32 afterCount = 0u;
    nkentseu::nk_uint32 leaderCount = 0u;

    auto Worker = [&]() {
        for (nkentseu::nk_uint32 phase = 0u; phase < kPhases; ++phase) {
            {
                NkScopedLock lock(statsMutex);
                ++beforeCount;
            }

            if (barrier.Wait()) {
                NkScopedLock lock(statsMutex);
                ++leaderCount;
            }

            {
                NkScopedLock lock(statsMutex);
                ++afterCount;
            }
        }
    };

    NkThread threads[kThreads];
    for (nkentseu::nk_uint32 i = 0u; i < kThreads; ++i) {
        threads[i] = NkThread(Worker);
    }

    for (nkentseu::nk_uint32 i = 0u; i < kThreads; ++i) {
        threads[i].Join();
    }

    ASSERT_EQUAL(kThreads * kPhases, beforeCount);
    ASSERT_EQUAL(kThreads * kPhases, afterCount);
    ASSERT_EQUAL(kPhases, leaderCount);
}

TEST_CASE(NKThreadingSync, LatchCountDownAndTimeout) {
    NkLatch latch(2u);

    NkThread t1([&]() { latch.CountDown(); });
    NkThread t2([&]() { latch.CountDown(); });

    ASSERT_TRUE(latch.Wait(300));
    ASSERT_TRUE(latch.IsReady());
    ASSERT_EQUAL(0u, latch.GetCount());

    t1.Join();
    t2.Join();

    NkLatch timeoutLatch(1u);
    ASSERT_FALSE(timeoutLatch.Wait(5));
    timeoutLatch.CountDown();
    ASSERT_TRUE(timeoutLatch.Wait(5));
}

TEST_CASE(NKThreadingSync, EventManualResetWakesAll) {
    NkEvent event(true, false);
    NkLatch ready(2u);
    NkMutex countMutex;

    nkentseu::nk_uint32 success = 0u;
    nkentseu::nk_uint32 timeout = 0u;

    auto Waiter = [&]() {
        ready.CountDown();
        const nkentseu::nk_bool ok = event.Wait(300);
        NkScopedLock lock(countMutex);
        if (ok) {
            ++success;
        } else {
            ++timeout;
        }
    };

    NkThread t1(Waiter);
    NkThread t2(Waiter);

    ASSERT_TRUE(ready.Wait(200));
    event.Set();

    t1.Join();
    t2.Join();

    ASSERT_EQUAL(2u, success);
    ASSERT_EQUAL(0u, timeout);
    ASSERT_TRUE(event.IsSignaled());

    event.Reset();
    ASSERT_FALSE(event.IsSignaled());
}

TEST_CASE(NKThreadingSync, EventAutoResetWakesOne) {
    NkEvent event(false, false);
    NkLatch ready(2u);
    NkMutex countMutex;

    nkentseu::nk_uint32 success = 0u;
    nkentseu::nk_uint32 timeout = 0u;

    auto Waiter = [&]() {
        ready.CountDown();
        const nkentseu::nk_bool ok = event.Wait(200);
        NkScopedLock lock(countMutex);
        if (ok) {
            ++success;
        } else {
            ++timeout;
        }
    };

    NkThread t1(Waiter);
    NkThread t2(Waiter);

    ASSERT_TRUE(ready.Wait(200));
    event.Set();

    t1.Join();
    t2.Join();

    ASSERT_EQUAL(1u, success);
    ASSERT_EQUAL(1u, timeout);
}

TEST_CASE(NKThreadingSync, EventPulseWithoutWaitersIsTransient) {
    NkEvent manualPulseEvent(true, false);
    manualPulseEvent.Pulse();
    ASSERT_FALSE(manualPulseEvent.IsSignaled());

    NkEvent autoPulseEvent(false, false);
    autoPulseEvent.Pulse();
    ASSERT_FALSE(autoPulseEvent.IsSignaled());
}

TEST_CASE(NKThreadingSync, ReaderWriterLockBasicAndWriters) {
    NkReaderWriterLock rwLock;

    rwLock.LockRead();
    ASSERT_FALSE(rwLock.TryLockWrite());
    rwLock.UnlockRead();

    ASSERT_TRUE(rwLock.TryLockRead());
    ASSERT_TRUE(rwLock.TryLockRead());
    rwLock.UnlockRead();
    rwLock.UnlockRead();

    rwLock.LockWrite();
    ASSERT_FALSE(rwLock.TryLockRead());
    rwLock.UnlockWrite();

    constexpr nkentseu::nk_uint32 kWorkers = 4u;
    constexpr nkentseu::nk_uint32 kIncrementsPerWorker = 500u;

    NkBarrier startBarrier(kWorkers + 1u);
    nkentseu::nk_uint32 counter = 0u;

    auto Writer = [&]() {
        startBarrier.Wait();
        for (nkentseu::nk_uint32 i = 0u; i < kIncrementsPerWorker; ++i) {
            NkWriteLock lock(rwLock);
            ++counter;
        }
    };

    NkThread writers[kWorkers];
    for (nkentseu::nk_uint32 i = 0u; i < kWorkers; ++i) {
        writers[i] = NkThread(Writer);
    }

    startBarrier.Wait();
    for (nkentseu::nk_uint32 i = 0u; i < kWorkers; ++i) {
        writers[i].Join();
    }

    ASSERT_EQUAL(kWorkers * kIncrementsPerWorker, counter);
}
