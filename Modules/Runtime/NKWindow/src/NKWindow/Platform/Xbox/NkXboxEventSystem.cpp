// =============================================================================
// NkXboxEventSystem.cpp
// Xbox implementation of platform-specific NkEventSystem methods.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_XBOX)

#include "NKWindow/Events/NkEventSystem.h"
#include "NKWindow/Core/NkEntry.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

namespace nkentseu {

    bool NkEventSystem::Init() {
        if (mReady) {
            return true;
        }

        mTotalEventCount = 0;
        {
            NkScopedSpinLock lock(mQueueMutex);
            mEventQueue.Clear();
        }
        mPumping = false;
        mReady = true;
        mData.mInitialized = NkXboxIsNativeWindowReady();
        return true;
    }

    void NkEventSystem::PumpOS() {
        if (mPumping || !mReady) {
            return;
        }
        mPumping = true;
        NkXboxPumpSystemEvents();

        mPumping = false;
    }

    const char* NkEventSystem::GetPlatformName() const noexcept {
        return "Xbox";
    }

    void NkEventSystem::Enqueue_Public(NkEvent& evt, NkWindowId winId) {
        Enqueue(evt, winId);
    }

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_XBOX
