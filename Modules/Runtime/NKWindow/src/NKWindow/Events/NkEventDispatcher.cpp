// =============================================================================
// NkEventDispatcher.cpp
//
// Implémentation de :
//   NkInputQuery    — polling direct via NkEventState (membres exacts)
//   NkActionManager — actions nommées (portage d'ActionManager)
//   NkAxisManager   — axes nommés (portage d'AxisManager)
//
// Membres NkEventState utilisés :
//   state.keyboard.IsKeyPressed(key)    — NkKeyboardInputState
//   state.keyboard.IsKeyRepeated(key)
//   state.keyboard.IsCtrlDown() etc.
//   state.keyboard.lastKey / lastScancode
//   state.mouse.x / y / deltaX / deltaY / rawDeltaX / rawDeltaY
//   state.mouse.IsButtonPressed(btn)
//   state.mouse.IsLeftDown() / IsRightDown() / IsMiddleDown()
//   state.mouse.IsAnyButtonPressed()
//   state.mouse.insideWindow
//   state.gamepads.IsButtonDown(idx, btn)  — NkGamepadSetState
//   state.gamepads.GetAxis(idx, ax)
//   state.gamepads.slots[idx].connected
// =============================================================================

#include "NkEventDispatcher.h"
#include "NkEventSystem.h"
#include "NKWindow/Core/NkSystem.h"   // NkSystem::Events() → NkEventSystem&

#include <algorithm>

namespace nkentseu {

    // =========================================================================
    // NkInputQuery — helper interne
    // =========================================================================

    const NkEventState& NkInputQuery::State() const noexcept {
        return NkSystem::Events().GetInputState();
    }

    // -------------------------------------------------------------------------
    // Clavier — NkKeyboardInputState
    // -------------------------------------------------------------------------

    bool NkInputQuery::IsKeyDown(NkKey key) const noexcept {
        return State().keyboard.IsKeyPressed(key);
    }

    bool NkInputQuery::IsKeyRepeated(NkKey key) const noexcept {
        return State().keyboard.IsKeyRepeated(key);
    }

    bool NkInputQuery::IsCtrlDown()  const noexcept { return State().keyboard.IsCtrlDown();  }
    bool NkInputQuery::IsAltDown()   const noexcept { return State().keyboard.IsAltDown();   }
    bool NkInputQuery::IsShiftDown() const noexcept { return State().keyboard.IsShiftDown(); }
    bool NkInputQuery::IsSuperDown() const noexcept { return State().keyboard.IsSuperDown(); }

    NkKey      NkInputQuery::LastKey()      const noexcept { return State().keyboard.lastKey;      }
    NkScancode NkInputQuery::LastScancode() const noexcept { return State().keyboard.lastScancode; }

    // -------------------------------------------------------------------------
    // Souris — NkMouseInputState
    // -------------------------------------------------------------------------

    NkI32 NkInputQuery::MouseX()          const noexcept { return State().mouse.x;          }
    NkI32 NkInputQuery::MouseY()          const noexcept { return State().mouse.y;          }
    NkI32 NkInputQuery::MouseDeltaX()     const noexcept { return State().mouse.deltaX;     }
    NkI32 NkInputQuery::MouseDeltaY()     const noexcept { return State().mouse.deltaY;     }
    NkI32 NkInputQuery::MouseRawDeltaX()  const noexcept { return State().mouse.rawDeltaX;  }
    NkI32 NkInputQuery::MouseRawDeltaY()  const noexcept { return State().mouse.rawDeltaY;  }

    bool NkInputQuery::IsMouseDown(NkMouseButton btn) const noexcept {
        return State().mouse.IsButtonPressed(btn);
    }
    bool NkInputQuery::IsLeftDown()    const noexcept { return State().mouse.IsLeftDown();   }
    bool NkInputQuery::IsRightDown()   const noexcept { return State().mouse.IsRightDown();  }
    bool NkInputQuery::IsMiddleDown()  const noexcept { return State().mouse.IsMiddleDown(); }
    bool NkInputQuery::IsAnyMouseDown()const noexcept { return State().mouse.IsAnyButtonPressed(); }
    bool NkInputQuery::IsMouseInside() const noexcept { return State().mouse.insideWindow;  }

    // -------------------------------------------------------------------------
    // Manette — NkGamepadSetState (state.gamepads)
    // -------------------------------------------------------------------------

    bool NkInputQuery::IsGamepadDown(NkU32 idx, NkGamepadButton btn) const noexcept {
        return State().gamepads.IsButtonDown(idx, btn);
    }

    NkF32 NkInputQuery::GamepadAxis(NkU32 idx, NkGamepadAxis ax) const noexcept {
        return State().gamepads.GetAxis(idx, ax);
    }

    bool NkInputQuery::IsGamepadConnected(NkU32 idx) const noexcept {
        const auto* slot = State().gamepads.GetSlot(idx);
        return slot && slot->connected;
    }

    void NkInputQuery::GamepadRumble(NkU32 idx,
                                      NkF32 motorLow, NkF32 motorHigh,
                                      NkF32 triggerLeft, NkF32 triggerRight,
                                      NkU32 durationMs) const {
        NkSystem::Gamepads().Rumble(idx,
            motorLow, motorHigh, triggerLeft, triggerRight, durationMs);
    }

    // =========================================================================
    // NkActionManager — portage d'ActionManager
    // =========================================================================

    void NkActionManager::CreateAction(const std::string& name, NkActionSubscriber handler) {
        if (mActions.count(name)) return;  // déjà existant
        mActions[name] = std::move(handler);
    }

    void NkActionManager::AddCommand(const NkActionCommand& cmd) {
        const std::string& name = cmd.GetName();
        if (!mActions.count(name)) return;  // action inexistante

        auto& cmds = mCommands[name];
        for (const auto& c : cmds)
            if (c == cmd) return;  // doublon
        cmds.push_back(cmd);
    }

    void NkActionManager::RemoveAction(const std::string& name) {
        mActions.erase(name);
        mCommands.erase(name);
    }

    void NkActionManager::RemoveCommand(const NkActionCommand& cmd) {
        auto it = mCommands.find(cmd.GetName());
        if (it == mCommands.end()) return;
        auto& v = it->second;
        v.erase(std::remove(v.begin(), v.end(), cmd), v.end());
    }

    void NkActionManager::TriggerAction(const NkInputCode& code, bool isPressed) {
        for (auto& [name, cmds] : mCommands)
            for (auto& cmd : cmds)
                if (cmd.GetCode() == code)
                    FireAction(name, code, isPressed, cmd);
    }

    void NkActionManager::FireAction(const std::string& name,
                                      const NkInputCode& code,
                                      bool isPressed,
                                      NkActionCommand& cmd)
    {
        // Portage exact de ActionManager::CheckEventStatus
        if (isPressed) {
            if (cmd.IsRepeatable() || !cmd.IsPrivateRepeatable()) {
                auto it = mActions.find(name);
                if (it != mActions.end() && it->second)
                    it->second(name, code, true, !cmd.IsPrivateRepeatable());
                cmd.SetPrivateRepeatable(true);
            }
        } else {
            cmd.SetPrivateRepeatable(false);
            auto it = mActions.find(name);
            if (it != mActions.end() && it->second)
                it->second(name, code, false, false);
        }
    }

    NkU64 NkActionManager::GetCommandCount() const noexcept {
        NkU64 n = 0;
        for (const auto& [name, cmds] : mCommands) n += cmds.size();
        return n;
    }

    NkU64 NkActionManager::GetCommandCount(const std::string& name) const noexcept {
        auto it = mCommands.find(name);
        return it != mCommands.end() ? static_cast<NkU64>(it->second.size()) : 0u;
    }

    // =========================================================================
    // NkAxisManager — portage d'AxisManager
    // =========================================================================

    void NkAxisManager::CreateAxis(const std::string& name, NkAxisSubscriber handler) {
        if (mAxes.count(name)) return;
        mAxes[name] = std::move(handler);
    }

    void NkAxisManager::AddCommand(const NkAxisCommand& cmd) {
        if (!mAxes.count(cmd.GetName())) return;
        auto& cmds = mCommands[cmd.GetName()];
        for (const auto& c : cmds)
            if (c == cmd) return;
        cmds.push_back(cmd);
    }

    void NkAxisManager::RemoveAxis(const std::string& name) {
        mAxes.erase(name);
        mCommands.erase(name);
    }

    void NkAxisManager::RemoveCommand(const NkAxisCommand& cmd) {
        auto it = mCommands.find(cmd.GetName());
        if (it == mCommands.end()) return;
        auto& v = it->second;
        v.erase(std::remove(v.begin(), v.end(), cmd), v.end());
    }

    void NkAxisManager::UpdateAxes(const NkAxisResolver& resolver) {
        // Portage exact d'AxisManager::UpdateAxis
        for (auto& [name, cmds] : mCommands) {
            for (const auto& cmd : cmds) {
                float raw   = resolver(cmd.GetCode().device, cmd.GetCode().code);
                float value = raw * cmd.GetScale();
                if (std::abs(value) >= cmd.GetMinInterval())
                    FireAxis(name, cmd, value);
            }
        }
    }

    void NkAxisManager::FireAxis(const std::string& name,
                                  const NkAxisCommand& cmd, float value)
    {
        // Portage de AxisManager::CheckEventStatus
        auto it = mAxes.find(name);
        if (it != mAxes.end() && it->second)
            it->second(name, cmd.GetCode(), value);
    }

    NkU64 NkAxisManager::GetCommandCount() const noexcept {
        NkU64 n = 0;
        for (const auto& [name, cmds] : mCommands) n += cmds.size();
        return n;
    }

    NkU64 NkAxisManager::GetCommandCount(const std::string& name) const noexcept {
        auto it = mCommands.find(name);
        return it != mCommands.end() ? static_cast<NkU64>(it->second.size()) : 0u;
    }

} // namespace nkentseu