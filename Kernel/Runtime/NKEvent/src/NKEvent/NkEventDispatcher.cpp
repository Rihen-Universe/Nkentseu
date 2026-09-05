#include "pch.h"
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
#include "NkGamepadSystem.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu {

	// =========================================================================
	// NkInputQuery — helper interne
	// =========================================================================

	const NkEventState &NkInputQuery::State() const noexcept {
		static NkEventState sDummyState;
		return mEventSystem ? mEventSystem->GetInputState() : sDummyState;
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

	bool NkInputQuery::IsCtrlDown() const noexcept {
		return State().keyboard.IsCtrlDown();
	}

	bool NkInputQuery::IsAltDown() const noexcept {
		return State().keyboard.IsAltDown();
	}

	bool NkInputQuery::IsShiftDown() const noexcept {
		return State().keyboard.IsShiftDown();
	}

	bool NkInputQuery::IsSuperDown() const noexcept {
		return State().keyboard.IsSuperDown();
	}

	NkKey NkInputQuery::LastKey() const noexcept {
		return State().keyboard.lastKey;
	}

	NkScancode NkInputQuery::LastScancode() const noexcept {
		return State().keyboard.lastScancode;
	}

	// -------------------------------------------------------------------------
	// Souris — NkMouseInputState
	// -------------------------------------------------------------------------

	int32 NkInputQuery::MouseX() const noexcept {
		return State().mouse.x;
	}

	int32 NkInputQuery::MouseY() const noexcept {
		return State().mouse.y;
	}

	int32 NkInputQuery::MouseDeltaX() const noexcept {
		return State().mouse.deltaX;
	}

	int32 NkInputQuery::MouseDeltaY() const noexcept {
		return State().mouse.deltaY;
	}

	int32 NkInputQuery::MouseRawDeltaX() const noexcept {
		return State().mouse.rawDeltaX;
	}

	int32 NkInputQuery::MouseRawDeltaY() const noexcept {
		return State().mouse.rawDeltaY;
	}

	bool NkInputQuery::IsMouseDown(NkMouseButton btn) const noexcept {
		return State().mouse.IsButtonPressed(btn);
	}

	bool NkInputQuery::IsLeftDown() const noexcept {
		return State().mouse.IsLeftDown();
	}

	bool NkInputQuery::IsRightDown() const noexcept {
		return State().mouse.IsRightDown();
	}

	bool NkInputQuery::IsMiddleDown() const noexcept {
		return State().mouse.IsMiddleDown();
	}

	bool NkInputQuery::IsAnyMouseDown() const noexcept {
		return State().mouse.IsAnyButtonPressed();
	}

	bool NkInputQuery::IsMouseInside() const noexcept {
		return State().mouse.insideWindow;
	}

	// -------------------------------------------------------------------------
	// Manette — NkGamepadSetState (state.gamepads)
	// -------------------------------------------------------------------------

	bool NkInputQuery::IsGamepadDown(uint32 idx, NkGamepadButton btn) const noexcept {
		return State().gamepads.IsButtonDown(idx, btn);
	}

	float32 NkInputQuery::GamepadAxis(uint32 idx, NkGamepadAxis ax) const noexcept {
		return State().gamepads.GetAxis(idx, ax);
	}

	bool NkInputQuery::IsGamepadConnected(uint32 idx) const noexcept {
		const auto *slot = State().gamepads.GetSlot(idx);
		return slot && slot->connected;
	}

	void NkInputQuery::GamepadRumble(uint32 idx, float32 motorLow, float32 motorHigh, float32 triggerLeft,
									 float32 triggerRight, uint32 durationMs) const {
		if (mGamepadSystem)
			mGamepadSystem->Rumble(idx, motorLow, motorHigh, triggerLeft, triggerRight, durationMs);
	}

	// =========================================================================
	// NkActionManager — portage d'ActionManager
	// =========================================================================

	void NkActionManager::Clear() noexcept {
		mActions.Clear();
		mCommands.Clear();
	}

	void NkActionManager::CreateAction(const NkString &name, NkActionSubscriber handler) {
		if (mActions.Contains(name))
			return;
		mActions[name] = traits::NkMove(handler);
	}

	void NkActionManager::AddCommand(const NkActionCommand &cmd) {
		const NkString &name = cmd.GetName();
		if (!mActions.Contains(name))
			return;

		auto &cmds = mCommands[name];
		for (const auto &c : cmds)
			if (c == cmd)
				return;
		cmds.PushBack(cmd);
	}

	void NkActionManager::RemoveAction(const NkString &name) {
		mActions.Erase(name);
		mCommands.Erase(name);
	}

	void NkActionManager::RemoveCommand(const NkActionCommand &cmd) {
		NkVector<NkActionCommand> *v = mCommands.Find(cmd.GetName());
		if (!v)
			return;
		for (nk_size i = 0; i < v->Size(); ++i) {
			if ((*v)[i] == cmd) {
				v->Erase(v->begin() + i);
				break;
			}
		}
	}

	void NkActionManager::TriggerAction(const NkInputCode &code, bool isPressed) {
		mCommands.ForEach([&](const NkString &name, NkVector<NkActionCommand> &cmds) {
			for (auto &cmd : cmds)
				if (cmd.GetCode() == code)
					FireAction(name, code, isPressed, cmd);
		});
	}

	void NkActionManager::FireAction(const NkString &name, const NkInputCode &code, bool isPressed,
									 NkActionCommand &cmd) {
		if (isPressed) {
			if (cmd.IsRepeatable() || !cmd.IsPrivateRepeatable()) {
				NkActionSubscriber *sub = mActions.Find(name);
				if (sub && *sub)
					(*sub)(name, code, true, !cmd.IsPrivateRepeatable());
				cmd.SetPrivateRepeatable(true);
			}
		} else {
			cmd.SetPrivateRepeatable(false);
			NkActionSubscriber *sub = mActions.Find(name);
			if (sub && *sub)
				(*sub)(name, code, false, false);
		}
	}

	uint64 NkActionManager::GetCommandCount() const noexcept {
		uint64 n = 0;
		mCommands.ForEach([&](const NkString &, const NkVector<NkActionCommand> &cmds) { n += cmds.Size(); });
		return n;
	}

	uint64 NkActionManager::GetCommandCount(const NkString &name) const noexcept {
		const NkVector<NkActionCommand> *v = mCommands.Find(name);
		return v ? static_cast<uint64>(v->Size()) : 0u;
	}

	// =========================================================================
	// NkAxisManager — portage d'AxisManager
	// =========================================================================

	void NkAxisManager::Clear() noexcept {
		mAxes.Clear();
		mCommands.Clear();
	}

	void NkAxisManager::CreateAxis(const NkString &name, NkAxisSubscriber handler) {
		if (mAxes.Contains(name))
			return;
		mAxes[name] = traits::NkMove(handler);
	}

	void NkAxisManager::AddCommand(const NkAxisCommand &cmd) {
		if (!mAxes.Contains(cmd.GetName()))
			return;
		auto &cmds = mCommands[cmd.GetName()];
		for (const auto &c : cmds)
			if (c == cmd)
				return;
		cmds.PushBack(cmd);
	}

	void NkAxisManager::RemoveAxis(const NkString &name) {
		mAxes.Erase(name);
		mCommands.Erase(name);
	}

	void NkAxisManager::RemoveCommand(const NkAxisCommand &cmd) {
		NkVector<NkAxisCommand> *v = mCommands.Find(cmd.GetName());
		if (!v)
			return;
		for (nk_size i = 0; i < v->Size(); ++i) {
			if ((*v)[i] == cmd) {
				v->Erase(v->begin() + i);
				break;
			}
		}
	}

	// UN AXE A UNE SEULE VALEUR, ET LE RAPPEL PART UNE SEULE FOIS.
	//
	// La premiere version appelait le rappel POUR CHAQUE COMMANDE de l'axe. Un
	// axe « Horizontal » a deux commandes, la fleche droite a +1 et la gauche a
	// -1 : tenir la droite envoyait +1 puis 0, et le zero de la gauche relachee
	// ecrasait le +1. Il fallait donc ecrire `valeur += v` dans le rappel et
	// remettre a zero avant, ce qui n'a de sens pour personne : un axe, comme
	// une manette, a UNE valeur.
	//
	// Depuis que le rafraichissement est automatique (dans PollEvent), cette
	// remise a zero devait en plus se faire loin de l'appel, a un endroit que
	// rien ne signale. On somme donc ici, et le rappel recoit le total, une
	// fois. Le code de l'application devient `valeur = v`, et il n'y a plus
	// rien a remettre a zero.
	//
	// Le code transmis est celui de la commande qui pese le plus fort en valeur
	// absolue : c'est celle qui explique le mieux d'ou vient le mouvement, et
	// c'est ce qu'un ecran de configuration veut afficher.
	void NkAxisManager::UpdateAxes(const NkAxisResolver &resolver) {
		mCommands.ForEach([&](const NkString &name, NkVector<NkAxisCommand> &cmds) {
			if (cmds.IsEmpty())
				return;

			float total = 0.f;
			float meilleure = -1.f;
			const NkAxisCommand *dominante = &cmds[0];

			for (const auto &cmd : cmds) {
				const float brut = resolver(cmd.GetCode().device, cmd.GetCode().code);
				const float valeur = brut * cmd.GetScale();
				total += valeur;
				const float force = math::NkFabs(valeur);
				if (force > meilleure) {
					meilleure = force;
					dominante = &cmd;
				}
			}

			if (math::NkFabs(total) >= dominante->GetMinInterval())
				FireAxis(name, *dominante, total);
		});
	}

	void NkAxisManager::FireAxis(const NkString &name, const NkAxisCommand &cmd, float value) {
		NkAxisSubscriber *sub = mAxes.Find(name);
		if (sub && *sub)
			(*sub)(name, cmd.GetCode(), value);
	}

	uint64 NkAxisManager::GetCommandCount() const noexcept {
		uint64 n = 0;
		mCommands.ForEach([&](const NkString &, const NkVector<NkAxisCommand> &cmds) { n += cmds.Size(); });
		return n;
	}

	uint64 NkAxisManager::GetCommandCount(const NkString &name) const noexcept {
		const NkVector<NkAxisCommand> *v = mCommands.Find(name);
		return v ? static_cast<uint64>(v->Size()) : 0u;
	}

} // namespace nkentseu
