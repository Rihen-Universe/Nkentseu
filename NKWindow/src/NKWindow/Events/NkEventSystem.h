#pragma once

#include "NkEvent.h"
#include "NkEventState.h"
#include "IEventImpl.h"

#include "NkApplicationEvent.h"
#include "NkWindowEvent.h"
#include "NkKeyboardEvent.h"
#include "NkMouseEvent.h"
#include "NkTouchEvent.h"
#include "NkGamepadEvent.h"
#include "NkDropEvent.h"
#include "NkSystemEvent.h"
#include "NkCustomEvent.h"
#include "NkTransferEvent.h"
#include "NkGenericHidEvent.h"
#include "NkGraphicsEvent.h"

#include <functional>
#include <unordered_map>
#include <vector>
#include <deque>
#include <memory>
#include <mutex>

namespace nkentseu {

    using NkGlobalEventCallback = std::function<void(NkEvent*)>;
    using NkTypedEventCallback  = std::function<void(NkEvent*)>;

    class NkEventSystem {
        public:
            static NkEventSystem& Instance();

            NkEventSystem(const NkEventSystem&) = delete;
            NkEventSystem& operator=(const NkEventSystem&) = delete;

            bool Init(std::unique_ptr<IEventImpl> impl);
            void Shutdown();
            bool IsReady() const noexcept { return mImpl && mImpl->IsReady(); }
            IEventImpl* GetImpl() const noexcept { return mImpl.get(); }

            void AttachWindow(IWindowImpl* owner, void* nativeHandle);
            void DetachWindow(void* nativeHandle);
            void SetWindowCallback(void* nativeHandle, NkEventCallback cb);

            // Mode file
            NkEvent* PollEvent();
            bool PollEvent(NkEvent*& event);

            // Mode callback (pompe sans file)
            void PollEvents();

            // Enregistrement des callbacks (délégué au backend)
            void SetGlobalCallback(NkGlobalEventCallback cb);
            template<typename T>
            void AddEventCallback(std::function<void(T*)> callback) {
                auto wrapper = [callback](NkEvent* ev) {
                    if (auto* typed = ev->As<T>()) callback(typed);
                };
                if (mImpl) mImpl->AddTypedCallback(T::GetStaticType(), std::move(wrapper));
            }
            template<typename T>
            void ClearEventCallbacks() {
                if (mImpl) mImpl->ClearTypedCallbacks(T::GetStaticType());
            }
            void ClearAllCallbacks();

            // Dispatch manuel (injecte un événement)
            void DispatchEvent(NkEvent& event);
            template<typename T>
            void DispatchEvent(T&& event) {
                static_assert(std::is_base_of<NkEvent, std::decay_t<T>>::value,
                            "DispatchEvent : T doit dériver de NkEvent");
                NkEvent& base = event;
                DispatchEvent(base);
            }

            // Snapshot
            const NkEventState& GetInputState() const noexcept;

            // Options
            void SetAutoUpdateInputState(bool enabled) noexcept { mAutoUpdateInputState = enabled; }
            bool GetAutoUpdateInputState() const noexcept { return mAutoUpdateInputState; }
            void SetAutoGamepadPoll(bool enabled) noexcept { mAutoGamepadPoll = enabled; }
            bool GetAutoGamepadPoll() const noexcept { return mAutoGamepadPoll; }
            void SetQueueMode(bool enabled) noexcept { mQueueMode = enabled; }
            bool GetQueueMode() const noexcept { return mQueueMode; }

            // Diagnostics
            std::size_t GetPendingEventCount() const noexcept;
            NkU64 GetTotalEventCount() const noexcept { return mTotalEventCount; }
            const char* GetPlatformName() const noexcept;

        private:
            NkEventSystem() = default;

            void PumpOS();
            void UpdateInputState(NkEvent* ev);

            std::unique_ptr<IEventImpl> mImpl;

            // Queue applicative (mode file)
            std::deque<std::unique_ptr<NkEvent>> mEventQueue;
            std::unique_ptr<NkEvent> mCurrentEvent;

            // Options
            bool mAutoUpdateInputState = true;
            bool mAutoGamepadPoll      = true;
            bool mQueueMode            = true;

            // Diagnostics
            NkU64 mTotalEventCount = 0;

            // Thread-safety pour DispatchEvent()
            std::mutex mDispatchMutex;

            // Protection contre la réentrance
            bool mPumping = false;
    };

    inline NkEventSystem& NkEvents() {
        return NkEventSystem::Instance();
    }

} // namespace nkentseu