// =============================================================================
// NkNoopEventSystem.cpp
// Headless/fallback implementation of platform-specific NkEventSystem methods.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY) || \
    (!defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX) && \
    !defined(NKENTSEU_PLATFORM_ANDROID) && !defined(NKENTSEU_PLATFORM_MACOS) && !defined(NKENTSEU_PLATFORM_IOS) && \
    !defined(NKENTSEU_PLATFORM_EMSCRIPTEN) && !defined(__EMSCRIPTEN__) && \
    !(defined(NKENTSEU_PLATFORM_LINUX) && (defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB) || defined(NKENTSEU_WINDOWING_WAYLAND))))

#include "NKWindow/Events/NkEventSystem.h"

namespace nkentseu {
    using namespace math;

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
        mData.mInitialized = true;
        return true;
    }

    void NkEventSystem::PumpOS() {
        if (!mReady || mPumping) {
            return;
        }
        mPumping = true;
        // No operating-system queue in headless/noop mode.
        mPumping = false;
    }

    const char* NkEventSystem::GetPlatformName() const noexcept {
        return "Noop";
    }

    void NkEventSystem::Enqueue_Public(NkEvent& evt, NkWindowId winId) {
        Enqueue(evt, winId);
    }

} // namespace nkentseu

#endif
