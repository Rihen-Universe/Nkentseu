#pragma once

// =============================================================================
// NkXCBEventImpl.h  —  Système d'événements XCB
//
// Pattern identique à Win32 / XLib :
//   - Map void* → WindowEntry (clé = xcb_window_t encodé en void*).
//   - Événements typés via NkKeyPressEvent, NkMouseMoveEvent, etc.
//   - Enqueue + Clone() préserve le type polymorphe.
// =============================================================================

#include "../../Events/IEventImpl.h"
#include "../../Events/NkKeycodeMap.h"
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <unordered_map>

namespace nkentseu {

    class NkXCBWindowImpl;

    class NkXCBEventImpl : public IEventImpl {
    public:
        NkXCBEventImpl();
        ~NkXCBEventImpl() override;

        // Cycle de vie
        void Initialize(IWindowImpl *owner, void *nativeHandle) override;
        void Shutdown(void *nativeHandle) override;

        // Queue
        void        PollEvents() override;
        NkEvent    *Front()      const override;
        void        Pop()        override;
        bool        IsEmpty()    const override;
        void        PushEvent(std::unique_ptr<NkEvent> event) override;
        std::size_t Size()       const override;

        // Callbacks
        void SetGlobalCallback(NkEventCallback cb) override;
        void SetWindowCallback(void *nativeHandle, NkEventCallback cb) override;
        void DispatchEvent(NkEvent *event, void *nativeHandle) override;

        const char *GetPlatformName() const noexcept override;
        bool        IsReady()         const noexcept override;

    private:
        static NkKey           XcbKeysymToNkKey(xcb_keysym_t ks);
        static NkModifierState XcbStateMods(uint16_t state);

        struct WindowEntry {
            NkXCBWindowImpl *window = nullptr;
            NkEventCallback  callback;
        };

        xcb_connection_t                        *mConnection  = nullptr;
        xcb_key_symbols_t                       *mKeySymbols  = nullptr;
        NkEventCallback                          mGlobalCallback;
        std::unordered_map<void *, WindowEntry>  mWindowMap;

        // Dernier position souris (delta dans NkMouseMoveEvent)
        NkI32 mPrevMouseX = 0;
        NkI32 mPrevMouseY = 0;
    };

} // namespace nkentseu
