#pragma once

// =============================================================================
// NkXLibEventImpl.h  —  Système d'événements XLib
//
// Pattern identique à Win32 :
//   - Map void* → NkEventCallback pour les callbacks par fenêtre.
//   - Événements typés via NkKeyPressEvent, NkMouseMoveEvent, etc.
//   - Queue FIFO de std::unique_ptr<NkEvent>.
// =============================================================================

#include "../../Events/IEventImpl.h"
#include "../../Events/NkKeycodeMap.h"
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <unordered_map>

namespace nkentseu {

class NkXLibWindowImpl;

class NkXLibEventImpl : public IEventImpl {
public:
    NkXLibEventImpl()  = default;
    ~NkXLibEventImpl() override = default;

    // Cycle de vie
    void Initialize(IWindowImpl *owner, void *nativeHandle) override;
    void Shutdown(void *nativeHandle) override;

    // Queue
    void            PollEvents() override;
    NkEvent*        Front() const override;
    void            Pop() override;
    bool            IsEmpty() const override;
    void            PushEvent(std::unique_ptr<NkEvent> event) override;
    std::size_t     Size() const override;

    // Callbacks
    void SetGlobalCallback(NkEventCallback cb) override;
    void SetWindowCallback(void *nativeHandle, NkEventCallback cb) override;
    void DispatchEvent(NkEvent* event, void *nativeHandle) override;

    const char *GetPlatformName() const noexcept override;
    bool        IsReady()         const noexcept override;

private:
    static NkKey            XlibKeysymToNkKey(KeySym ks);
    static NkModifierState  XlibMods(unsigned int state);

    Display                               *mDisplay    = nullptr;
    NkEventCallback                        mGlobalCallback;
    std::unordered_map<void *, NkEventCallback> mWindowCallbacks;

    // Dernière position souris (pour calcul delta dans NkMouseMoveEvent)
    NkI32 mPrevMouseX = 0;
    NkI32 mPrevMouseY = 0;
};

} // namespace nkentseu
