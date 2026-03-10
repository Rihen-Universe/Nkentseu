#include "NKThreading/NkSemaphore.h"

namespace nkentseu {
namespace threading {

NkSemaphore::NkSemaphore(nk_uint32 initialCount, nk_uint32 maxCount) noexcept
    : mCount(initialCount),
      mMaxCount(maxCount >= initialCount ? maxCount : initialCount),
      mMutex(),
      mCondVar() {
}

void NkSemaphore::Acquire() noexcept {
    NkScopedLock lock(mMutex);
    while (mCount == 0u) {
        mCondVar.Wait(lock);
    }
    --mCount;
}

nk_bool NkSemaphore::TryAcquire() noexcept {
    NkScopedLock lock(mMutex);
    if (mCount == 0u) {
        return false;
    }
    --mCount;
    return true;
}

nk_bool NkSemaphore::TryAcquireFor(nk_uint32 milliseconds) noexcept {
    NkScopedLock lock(mMutex);
    if (mCount > 0u) {
        --mCount;
        return true;
    }

    const nk_uint64 deadline =
        NkConditionVariable::GetMonotonicTimeMs() + static_cast<nk_uint64>(milliseconds);

    while (mCount == 0u) {
        if (!mCondVar.WaitUntil(lock, deadline)) {
            return false;
        }
    }

    --mCount;
    return true;
}

nk_bool NkSemaphore::Release(nk_uint32 count) noexcept {
    if (count == 0u) {
        return true;
    }

    NkScopedLock lock(mMutex);
    if (mCount > mMaxCount || count > (mMaxCount - mCount)) {
        return false;
    }

    mCount += count;
    for (nk_uint32 i = 0u; i < count; ++i) {
        mCondVar.NotifyOne();
    }
    return true;
}

nk_uint32 NkSemaphore::GetCount() const noexcept {
    NkScopedLock lock(mMutex);
    return mCount;
}

nk_uint32 NkSemaphore::GetMaxCount() const noexcept {
    return mMaxCount;
}

} // namespace threading
} // namespace nkentseu
