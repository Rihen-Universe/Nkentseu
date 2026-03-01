#include "NkEventSystem.h"
#include "NkGamepadSystem.h"
#include <algorithm>
#include <iostream>

#if defined(NKENTSEU_PLATFORM_WASM) && defined(__EMSCRIPTEN__)
#   include <emscripten.h>
#endif

namespace nkentseu {

    NkEventSystem& NkEventSystem::Instance() {
        static NkEventSystem sInstance;
        return sInstance;
    }

    bool NkEventSystem::Init(std::unique_ptr<IEventImpl> impl) {
        if (!impl || !impl->IsReady()) return false;
        mImpl = std::move(impl);
        mTotalEventCount = 0;
        mEventQueue.clear();
        mPumping = false;
        return true;
    }

    void NkEventSystem::Shutdown() {
        if (!mImpl) return;
        mImpl.reset();
        mEventQueue.clear();
        mTotalEventCount = 0;
        mPumping = false;
    }

    void NkEventSystem::AttachWindow(IWindowImpl* owner, void* nativeHandle) {
        if (!mImpl || !owner || !nativeHandle) return;
        mImpl->Initialize(owner, nativeHandle);
    }

    void NkEventSystem::DetachWindow(void* nativeHandle) {
        if (!mImpl || !nativeHandle) return;
        mImpl->Shutdown(nativeHandle);
    }

    void NkEventSystem::SetWindowCallback(void* nativeHandle, NkEventCallback cb) {
        if (!mImpl) return;
        mImpl->SetWindowCallback(nativeHandle, std::move(cb));
    }

    void NkEventSystem::SetGlobalCallback(NkGlobalEventCallback cb) {
        if (!mImpl) return;
        mImpl->SetGlobalCallback(std::move(cb));
    }

    void NkEventSystem::ClearAllCallbacks() {
        if (!mImpl) return;
        mImpl->ClearAllCallbacks();
    }

    void NkEventSystem::PumpOS() {
        if (!mImpl || mPumping) return;
        mPumping = true;

        mImpl->PollEvents(); // Remplit la file du backend et appelle les callbacks

        while (!mImpl->IsEmpty()) {
            NkEvent* ev = mImpl->Front();
            if (!ev) { mImpl->Pop(); break; }
            ++mTotalEventCount;

            if (mAutoUpdateInputState) {
                UpdateInputState(ev);
            }

            if (mQueueMode) {
                mEventQueue.push_back(std::unique_ptr<NkEvent>(ev->Clone()));
            }

            mImpl->Pop(); // Pop AFTER using ev — avoids dangling ptr (unique_ptr freed on Pop)
        }

        mPumping = false;
    }

    void NkEventSystem::UpdateInputState(NkEvent* /*ev*/) {
        // Le backend maintient déjà mInputState lors du dispatch
    }

    void NkEventSystem::PollEvents() {
        if (!mImpl) return;

        if (mAutoGamepadPoll) {
            auto& gpSys = NkGamepadSystem::Instance();
            if (gpSys.IsReady()) gpSys.PollGamepads();
        }

        PumpOS();

#if defined(NKENTSEU_PLATFORM_WASM) && defined(__EMSCRIPTEN__)
        emscripten_sleep(0);
#endif
    }

    NkEvent* NkEventSystem::PollEvent() {
        if (!mImpl) return nullptr;

        if (!mEventQueue.empty()) {
            mCurrentEvent = std::move(mEventQueue.front());
            mEventQueue.pop_front();
            return mCurrentEvent.get();
        }

        if (mAutoGamepadPoll) {
            auto& gpSys = NkGamepadSystem::Instance();
            if (gpSys.IsReady()) gpSys.PollGamepads();
        }

        PumpOS();

        if (!mEventQueue.empty()) {
            mCurrentEvent = std::move(mEventQueue.front());
            mEventQueue.pop_front();
            return mCurrentEvent.get();
        }

#if defined(NKENTSEU_PLATFORM_WASM) && defined(__EMSCRIPTEN__)
        emscripten_sleep(0);
#endif

        return nullptr;
    }

    bool NkEventSystem::PollEvent(NkEvent*& event) {
        event = PollEvent();
        return event != nullptr;
    }

    void NkEventSystem::DispatchEvent(NkEvent& event) {
        if (!mImpl) return;
        std::lock_guard<std::mutex> lock(mDispatchMutex);
        mImpl->DispatchEvent(&event, nullptr);
        ++mTotalEventCount;
        if (mAutoUpdateInputState) {
            UpdateInputState(&event);
        }
    }

    const NkEventState& NkEventSystem::GetInputState() const noexcept {
        if (!mImpl) {
            static NkEventState dummy;
            return dummy;
        }
        return mImpl->GetInputState();
    }

    std::size_t NkEventSystem::GetPendingEventCount() const noexcept {
        return mEventQueue.size();
    }

    const char* NkEventSystem::GetPlatformName() const noexcept {
        if (!mImpl) return "Unknown";
        return mImpl->GetPlatformName();
    }

} // namespace nkentseu