// =============================================================================
// NkUWPEventSystem.cpp
// UWP/Xbox implementation of platform-specific NkEventSystem methods.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_UWP)

#include "NKWindow/Events/NkEventSystem.h"
#define NKENTSEU_UWP_RUNTIME_ONLY 1
#include "NKWindow/EntryPoints/NkUWP.h"

#include <mutex>

namespace nkentseu {

    bool NkEventSystem::Init() {
        if (mReady) {
            return true;
        }

        mTotalEventCount = 0;
        {
            std::lock_guard<std::mutex> lock(mQueueMutex);
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
