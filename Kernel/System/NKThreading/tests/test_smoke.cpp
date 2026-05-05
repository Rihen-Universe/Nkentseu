#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKThreading/NkMutex.h"
#include "NKThreading/NkConditionVariable.h"
#include "NKThreading/NkThread.h"

using namespace nkentseu::threading;

TEST_CASE(NKThreadingSmoke, MutexTryLock) {
    NkMutex mutex;

    ASSERT_TRUE(mutex.TryLock());
    ASSERT_FALSE(mutex.TryLock());
    mutex.Unlock();
    ASSERT_TRUE(mutex.TryLockFor(1));
    mutex.Unlock();
}

TEST_CASE(NKThreadingSmoke, ConditionVariableSignal) {
    NkMutex mutex;
    NkConditionVariable cv;

    bool ready = false;

    NkThread worker([&]() {
        NkScopedLock lock(mutex);
        ready = true;
        cv.NotifyOne();
    });

    NkScopedLock lock(mutex);
    while (!ready) {
        const auto woke = cv.WaitFor(lock, 50);
        (void)woke;
    }

    worker.Join();
    ASSERT_TRUE(ready);
}
