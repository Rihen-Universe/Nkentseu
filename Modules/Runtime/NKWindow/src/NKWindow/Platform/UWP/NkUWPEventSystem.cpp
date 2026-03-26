// =============================================================================
// NkUWPEventSystem.cpp
// UWP/Xbox implementation of platform-specific NkEventSystem methods.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_UWP)

#include "NKEvent/NkEventSystem.h"
#define NKENTSEU_UWP_RUNTIME_ONLY 1
#include "NKWindow/EntryPoints/NkUWP.h"

namespace nkentseu {
    using namespace math;

    bool NkEventSystem::Init() {
        if (mReady) {
            return true;
        }

        mData = new NkEventSystemData;
        if (mData == nullptr) return false;

        mTotalEventCount = 0;
        {
            NkScopedSpinLock lock(mQueueMutex);
            mEventQueue.Clear();
        }

        if (!NkUWPIsCoreWindowReady()) {
            mData.mInitialized = false;
            return false;
        }

        mPumping = false;
        mReady = true;
        mData.mInitialized = true;
        return true;
    }

    void NkEventSystem::Shutdown() {
        delete mData;
        mData = nullptr;
    }

    void NkEventSystem::PumpOS() {
        if (mPumping || !mReady) {
            return;
        }
        mPumping = true;
        NkUWPPumpSystemEvents();
        mPumping = false;
    }

    const char* NkEventSystem::GetPlatformName() const noexcept {
        return "UWP";
    }

    void NkEventSystem::Enqueue_Public(NkEvent& evt, NkWindowId winId) {
        Enqueue(evt, winId);
    }

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_UWP
