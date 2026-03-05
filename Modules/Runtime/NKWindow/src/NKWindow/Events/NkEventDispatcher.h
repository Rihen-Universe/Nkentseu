#pragma once

// =============================================================================
// NkEventDispatcher.h
//
// Trois outils :
//
//   1. NkEventDispatcher — router un NkEvent* vers des handlers typés (push)
//      Analogue à EventBroker::Route<T> de l'ancien système.
//      Macro : NK_DISPATCH(dispatcher, EventType, method)
//
//   2. NkInputQuery — polling direct de l'état courant des entrées (pull)
//      Analogue à InputManager::IsKeyDown / MouseAxis de l'ancien système.
//      Délègue exactement aux membres de NkEventState (NkEventState.h).
//
//   3. NkActionManager / NkAxisManager — actions et axes nommés.
//      Inspirés d'ActionManager / AxisManager de l'ancien système,
//      adaptés au nouveau système d'events.
//
// Correspondance avec l'ancien système :
//   EventBroker::Route<T>(fn)    → NkEventDispatcher::Dispatch<T>(fn)
//   REGISTER_CLIENT_EVENT(m)     → NK_DISPATCH(d, T, m)
//   InputManager::IsKeyDown()    → NkInput.IsKeyDown()
//   InputManager::IsMouseDown()  → NkInput.IsMouseDown()
//   InputManager::MousePosition  → NkInput.MouseX() / MouseY()
//   InputManager::MouseDelta     → NkInput.MouseDeltaX() / MouseDeltaY()
//   ActionManager                → NkActionManager
//   AxisManager                  → NkAxisManager
// =============================================================================

#include "NkEvent.h"
#include "NkEventState.h"      // NkKeyboardInputState, NkMouseInputState, NkGamepadSetState
#include "NkKeyboardEvent.h"   // NkKey, NkScancode, NkModifierState
#include "NkMouseEvent.h"      // NkMouseButton, NkMouseButtons
#include "NkGamepadEvent.h"    // NkGamepadButton, NkGamepadAxis
#include "NkGamepadSystem.h"   // NkGamepadSystem::Instance()

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <type_traits>
#include <cmath>

namespace nkentseu {

    // =========================================================================
    // 1. NkEventDispatcher
    //    Wrappeur sur NkEvent* — route vers des handlers typés.
    //    Une instance par event, créée dans OnEvent().
    //
    //    Analogie directe avec EventBroker :
    //      EventBroker::Route<T>(func)  →  NkEventDispatcher::Dispatch<T>(func)
    // =========================================================================

    template<typename T>
    using NkEventHandler = std::function<bool(T&)>;

    class NkEventDispatcher {
    public:
        explicit NkEventDispatcher(NkEvent* ev) noexcept : mEvent(ev)  {}
        explicit NkEventDispatcher(NkEvent& ev) noexcept : mEvent(&ev) {}

        // Dispatch<T> — route vers le handler si le type correspond.
        // Retourne true si l'event a été consommé (handler a retourné true).
        template<typename T>
        bool Dispatch(NkEventHandler<T> handler) {
            static_assert(std::is_base_of<NkEvent, T>::value,
                          "NkEventDispatcher::Dispatch — T must derive from NkEvent");
            if (!mEvent || mEvent->IsHandled()) return false;
            if (mEvent->GetType() != T::GetStaticType()) return false;

            bool consumed = handler(static_cast<T&>(*mEvent));
            if (consumed) mEvent->MarkHandled();
            return consumed;
        }

        // Variante directe : lambda / free fn sans wrapper explicite
        template<typename T, typename Fn>
        bool Dispatch(Fn&& fn) {
            return Dispatch<T>(NkEventHandler<T>(std::forward<Fn>(fn)));
        }

        NkEvent*           GetEvent()     const noexcept { return mEvent; }
        bool               IsHandled()    const noexcept { return mEvent && mEvent->IsHandled(); }
        NkEventType::Value GetEventType() const noexcept {
            return mEvent ? mEvent->GetType() : NkEventType::NK_NONE;
        }

    private:
        NkEvent* mEvent = nullptr;
    };

    // =========================================================================
    // Macros NK_DISPATCH
    //
    //   NK_DISPATCH(d, NkKeyPressEvent, OnKeyPress)
    //   équivalent :
    //   broker.Route<NkKeyPressEvent>(REGISTER_CLIENT_EVENT(OnKeyPress))
    //
    //   method_ doit avoir la signature : bool method_(EventType_& e)
    // =========================================================================

    #define NK_DISPATCH(dispatcher_, EventType_, method_)           \
        (dispatcher_).Dispatch<EventType_>(                          \
            [this](EventType_& e_) -> bool { return this->method_(e_); })

    #define NK_DISPATCH_FREE(dispatcher_, EventType_, fn_)          \
        (dispatcher_).Dispatch<EventType_>(fn_)

    // =========================================================================
    // 2. NkInputQuery — polling direct
    //
    //    Membres utilisés de NkEventState (voir NkEventState.h) :
    //
    //    state.keyboard (NkKeyboardInputState) :
    //      .IsKeyPressed(key)     → touche enfoncée
    //      .IsKeyRepeated(key)    → auto-repeat
    //      .IsCtrlDown()          → modificateur Ctrl
    //      .modifiers             → NkModifierState
    //      .lastKey               → dernière touche
    //      .lastScancode          → dernier scancode
    //
    //    state.mouse (NkMouseInputState) :
    //      .x, .y                 → position zone client
    //      .deltaX, .deltaY       → déplacement
    //      .rawDeltaX, .rawDeltaY → brut FPS
    //      .IsButtonPressed(btn)  → bouton enfoncé
    //      .IsLeftDown()          → bouton gauche
    //      .insideWindow          → curseur dans la fenêtre
    //
    //    state.gamepads (NkGamepadSetState) :
    //      .IsButtonDown(idx, btn)
    //      .GetAxis(idx, ax)
    // =========================================================================

    class NkInputQuery {
        public:
            // ------------------------------------------------------------------
            // Clavier — analogue à InputManager::IsKeyDown / Key
            // ------------------------------------------------------------------

            /// true si la touche est actuellement enfoncée
            /// (state.keyboard.IsKeyPressed)
            bool IsKeyDown(NkKey key)     const noexcept;

            /// true si la touche est en auto-repeat OS
            /// (state.keyboard.IsKeyRepeated)
            bool IsKeyRepeated(NkKey key) const noexcept;

            /// Modificateurs courants (state.keyboard.modifiers)
            bool IsCtrlDown()   const noexcept;
            bool IsAltDown()    const noexcept;
            bool IsShiftDown()  const noexcept;
            bool IsSuperDown()  const noexcept;

            /// Dernière touche / scancode (state.keyboard.lastKey / lastScancode)
            NkKey      LastKey()      const noexcept;
            NkScancode LastScancode() const noexcept;

            // ------------------------------------------------------------------
            // Souris — analogue à InputManager::MousePosition / IsMouseDown
            // ------------------------------------------------------------------

            /// Position zone client (state.mouse.x / y)
            NkI32 MouseX()          const noexcept;
            NkI32 MouseY()          const noexcept;

            /// Déplacement depuis le dernier poll (state.mouse.deltaX / deltaY)
            NkI32 MouseDeltaX()     const noexcept;
            NkI32 MouseDeltaY()     const noexcept;

            /// Mouvement brut sans accélération — FPS (state.mouse.rawDeltaX / Y)
            NkI32 MouseRawDeltaX()  const noexcept;
            NkI32 MouseRawDeltaY()  const noexcept;

            /// Boutons (state.mouse.IsButtonPressed / IsLeftDown…)
            bool IsMouseDown(NkMouseButton btn) const noexcept;
            bool IsLeftDown()                   const noexcept;
            bool IsRightDown()                  const noexcept;
            bool IsMiddleDown()                 const noexcept;
            bool IsAnyMouseDown()               const noexcept;

            /// Curseur à l'intérieur (state.mouse.insideWindow)
            bool IsMouseInside()    const noexcept;

            // ------------------------------------------------------------------
            // Manette — délègue à state.gamepads (NkGamepadSetState)
            // Analogue à InputManager::IsGamepadButtonDown / GamepadAxis
            // ------------------------------------------------------------------

            /// Bouton enfoncé (state.gamepads.IsButtonDown)
            bool  IsGamepadDown(NkU32 idx, NkGamepadButton btn) const noexcept;

            /// Valeur d'axe analogique (state.gamepads.GetAxis)
            NkF32 GamepadAxis(NkU32 idx, NkGamepadAxis ax)      const noexcept;

            /// Manette connectée (state.gamepads.slots[idx].connected)
            bool  IsGamepadConnected(NkU32 idx)                 const noexcept;

            /// Vibration — délègue à NkGamepadSystem (commande de sortie)
            void  GamepadRumble(NkU32 idx,
                                NkF32 motorLow    = 0.f,
                                NkF32 motorHigh   = 0.f,
                                NkF32 triggerLeft  = 0.f,
                                NkF32 triggerRight = 0.f,
                                NkU32 durationMs   = 100) const;

        private:
            // Accès à NkEventState via NkSystem::Events().GetInputState()
            const NkEventState& State() const noexcept;
    };

    /// Instance globale stateless (toutes les méthodes sont const)
    inline NkInputQuery NkInput;

    // =========================================================================
    // 3. NkActionManager / NkAxisManager
    //
    //    Portage fidèle d'ActionManager / AxisManager (NkInputController.h)
    //    vers les nouveaux types.
    //
    //    NkInputCode      ≈ ActionCode / AxisCode
    //    NkActionCommand  ≈ ActionCommand
    //    NkAxisCommand    ≈ AxisCommand
    //    NkActionSubscriber ≈ ActionSubscriber
    //    NkAxisSubscriber   ≈ AxisSubscriber
    // =========================================================================

    // -------------------------------------------------------------------------
    // NkInputCode — code d'entrée générique typé par device
    // -------------------------------------------------------------------------

    enum class NkInputDevice : NkU32 {
        NK_KEYBOARD    = 0,
        NK_MOUSE       = 1,
        NK_MOUSEWHEEL  = 2,
        NK_GAMEPAD     = 3,
        NK_GAMEPAD_AXIS = 4,
    };

    struct NkInputCode {
        NkInputDevice device   = NkInputDevice::NK_KEYBOARD;
        NkU32         code     = 0;
        NkU32         modifier = 0;

        bool operator==(const NkInputCode& o) const noexcept {
            return device == o.device && code == o.code && modifier == o.modifier;
        }
        bool operator!=(const NkInputCode& o) const noexcept { return !(*this == o); }

        static NkInputCode Key(NkKey k, NkU32 mod = 0) noexcept {
            return { NkInputDevice::NK_KEYBOARD, static_cast<NkU32>(k), mod };
        }
        static NkInputCode Mouse(NkMouseButton b, NkU32 mod = 0) noexcept {
            return { NkInputDevice::NK_MOUSE, static_cast<NkU32>(b), mod };
        }
        static NkInputCode Wheel(bool horizontal = false) noexcept {
            return { NkInputDevice::NK_MOUSEWHEEL, horizontal ? 1u : 0u, 0 };
        }
        static NkInputCode Gamepad(NkGamepadButton b, NkU32 mod = 0) noexcept {
            return { NkInputDevice::NK_GAMEPAD, static_cast<NkU32>(b), mod };
        }
        static NkInputCode GamepadAxis(NkGamepadAxis a) noexcept {
            return { NkInputDevice::NK_GAMEPAD_AXIS, static_cast<NkU32>(a), 0 };
        }
    };

    // -------------------------------------------------------------------------
    // NkActionCommand — analogue exact d'ActionCommand
    // -------------------------------------------------------------------------

    struct NkActionCommand {
            NkActionCommand(std::string name, NkInputCode code, bool repeatable = true)
                : mName(std::move(name)), mCode(code),
                mRepeatable(repeatable), mPrivRepeatable(repeatable) {}

            const std::string& GetName()               const noexcept { return mName; }
            const NkInputCode& GetCode()               const noexcept { return mCode; }
            bool               IsRepeatable()          const noexcept { return mRepeatable; }
            bool               IsPrivateRepeatable()   const noexcept { return mPrivRepeatable; }
            void               SetPrivateRepeatable(bool v) noexcept  { mPrivRepeatable = v; }

            bool operator==(const NkActionCommand& o) const noexcept { return mCode == o.mCode; }
            bool operator!=(const NkActionCommand& o) const noexcept { return !(*this == o); }

        private:
            std::string mName;
            NkInputCode mCode;
            bool        mRepeatable     = true;
            bool        mPrivRepeatable = true;
            friend class NkActionManager;
    };

    // Signature : (actionName, code, isPressed, isRepeat)
    using NkActionSubscriber = std::function<void(const std::string&,
                                                   const NkInputCode&,
                                                   bool isPressed,
                                                   bool isRepeat)>;

    // -------------------------------------------------------------------------
    // NkAxisCommand — analogue exact d'AxisCommand
    // -------------------------------------------------------------------------

    struct NkAxisCommand {
            NkAxisCommand(std::string name, NkInputCode code,
                        float scale = 1.f, float minInterval = 0.f)
                : mName(std::move(name)), mCode(code),
                mScale(scale), mMinInterval(minInterval) {}

            const std::string& GetName()        const noexcept { return mName; }
            const NkInputCode& GetCode()        const noexcept { return mCode; }
            float              GetScale()       const noexcept { return mScale; }
            float              GetMinInterval() const noexcept { return mMinInterval; }

            bool operator==(const NkAxisCommand& o) const noexcept { return mCode == o.mCode; }
            bool operator!=(const NkAxisCommand& o) const noexcept { return !(*this == o); }

        private:
            std::string mName;
            NkInputCode mCode;
            float       mScale       = 1.f;
            float       mMinInterval = 0.f;
    };

    // Signature : (axisName, code, value)
    using NkAxisSubscriber = std::function<void(const std::string&,
                                                 const NkInputCode&,
                                                 float value)>;

    // -------------------------------------------------------------------------
    // NkActionManager — analogue exact d'ActionManager
    // -------------------------------------------------------------------------

    class NkActionManager {
        public:
            void CreateAction(const std::string& name, NkActionSubscriber handler);
            void AddCommand(const NkActionCommand& cmd);
            void RemoveAction(const std::string& name);
            void RemoveCommand(const NkActionCommand& cmd);

            // Appelé par le dispatcher quand un event arrive
            void TriggerAction(const NkInputCode& code, bool isPressed);

            NkU64 GetActionCount()                         const noexcept { return mActions.size(); }
            NkU64 GetCommandCount()                        const noexcept;
            NkU64 GetCommandCount(const std::string& name) const noexcept;

        private:
            void FireAction(const std::string& name, const NkInputCode& code,
                            bool isPressed, NkActionCommand& cmd);

            std::unordered_map<std::string, NkActionSubscriber>           mActions;
            std::unordered_map<std::string, std::vector<NkActionCommand>> mCommands;
    };

    // -------------------------------------------------------------------------
    // NkAxisManager — analogue exact d'AxisManager
    // -------------------------------------------------------------------------

    // Callback de résolution de valeur — analogue à AxisUpdateFromCode
    //   (NkInputDevice, NkU32 code) → float
    // Exemple d'implémentation :
    //   [](NkInputDevice dev, NkU32 code) -> float {
    //       if (dev == NkInputDevice::Keyboard)
    //           return NkInput.IsKeyDown(static_cast<NkKey>(code)) ? 1.f : 0.f;
    //       if (dev == NkInputDevice::GamepadAxis)
    //           return NkInput.GamepadAxis(0, static_cast<NkGamepadAxis>(code));
    //       return 0.f;
    //   }
    using NkAxisResolver = std::function<float(NkInputDevice, NkU32)>;

    class NkAxisManager {
        public:
            void CreateAxis(const std::string& name, NkAxisSubscriber handler);
            void AddCommand(const NkAxisCommand& cmd);
            void RemoveAxis(const std::string& name);
            void RemoveCommand(const NkAxisCommand& cmd);

            // Appelé chaque frame — analogue à AxisManager::UpdateAxis
            void UpdateAxes(const NkAxisResolver& resolver);

            NkU64 GetAxisCount()                           const noexcept { return mAxes.size(); }
            NkU64 GetCommandCount()                        const noexcept;
            NkU64 GetCommandCount(const std::string& name) const noexcept;

        private:
            void FireAxis(const std::string& name, const NkAxisCommand& cmd, float value);

            std::unordered_map<std::string, NkAxisSubscriber>           mAxes;
            std::unordered_map<std::string, std::vector<NkAxisCommand>> mCommands;
    };

    // =========================================================================
    // Macros de commodité
    // Analogues à REGISTER_ACTION_SUBSCRIBER / REGISTER_AXIS_SUBSCRIBER
    // =========================================================================

    #define NK_ACTION_SUBSCRIBER(method_) \
        [this](const std::string& n_, const NkInputCode& c_, bool p_, bool r_) \
            { this->method_(n_, c_, p_, r_); }

    #define NK_AXIS_SUBSCRIBER(method_) \
        [this](const std::string& n_, const NkInputCode& c_, float v_) \
            { this->method_(n_, c_, v_); }

} // namespace nkentseu