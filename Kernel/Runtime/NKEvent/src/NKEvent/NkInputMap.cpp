#include "pch.h"
//
// NkInputMap.cpp
// =============================================================================
// Description :
//   Le calcul des actions : sources, modificateurs, declencheurs, contextes,
//   reservations, rappels, capture. Le texte est dans NkInputMapText.cpp.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "NKEvent/NkInputMap.h"

#include "NKMath/NkFunctions.h"

namespace nkentseu {

	using math::NkVec2f;

	namespace {

		constexpr uint32 kNbKeys = static_cast<uint32>(NkKey::NK_KEY_MAX);
		constexpr uint32 kNbMouse = static_cast<uint32>(NkMouseButton::NK_MOUSE_BUTTON_MAX);
		constexpr uint32 kNbPadButtons = static_cast<uint32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX);

		// Cles internes des reservations : un NkInputCode ne sait pas nommer le
		// deplacement de la souris ni un geste, et NkInputDevice ne doit pas
		// grandir pour un besoin interne a ce fichier.
		constexpr uint8 kResCode = 1;
		constexpr uint8 kResMouseDelta = 2;
		constexpr uint8 kResGesture = 3;
		constexpr uint8 kResTouch = 4;

		uint32 CodeKey(const NkInputCode &c) noexcept {
			return (static_cast<uint32>(c.device) << 24) | (c.code & 0xFFFFFFu);
		}

		float32 Length(const NkVec2f &v) noexcept {
			return math::NkSqrt(v.x * v.x + v.y * v.y);
		}

		/// L'autre axe du meme stick ; `isStick` faux si l'axe n'en est pas un.
		NkGamepadAxis Twin(NkGamepadAxis a, bool &isStick) noexcept {
			isStick = true;
			switch (a) {
				case NkGamepadAxis::NK_GP_AXIS_LX: return NkGamepadAxis::NK_GP_AXIS_LY;
				case NkGamepadAxis::NK_GP_AXIS_LY: return NkGamepadAxis::NK_GP_AXIS_LX;
				case NkGamepadAxis::NK_GP_AXIS_RX: return NkGamepadAxis::NK_GP_AXIS_RY;
				case NkGamepadAxis::NK_GP_AXIS_RY: return NkGamepadAxis::NK_GP_AXIS_RX;
				default:                           break;
			}
			isStick = false;
			return a;
		}

		/// Zone morte d'UNE valeur, course restante ramenee a 0..1.
		float32 DeadZone1(float32 v, float32 lo, float32 hi) noexcept {
			const float32 m = math::NkFabs(v);
			if (m <= lo) {
				return 0.f;
			}
			const float32 span = hi > lo ? hi - lo : 1.f;
			float32 k = (m - lo) / span;
			if (k > 1.f) {
				k = 1.f;
			}
			return v < 0.f ? -k : k;
		}

		/// Les durees se cumulent en flottants (0,05 x 10 ne fait pas toujours
		/// 0,5) : une tolerance d'un dixieme de milliseconde, sans laquelle un
		/// « maintenu 0,5 s » partirait une image trop tard, selon l'arrondi.
		constexpr float32 kEpsTemps = 1.0e-4f;

		bool SameZone(const NkInputSource &a, const NkInputSource &b) noexcept {
			return a.zoneX == b.zoneX && a.zoneY == b.zoneY && a.zoneW == b.zoneW && a.zoneH == b.zoneH;
		}

		bool Inside(const NkInputSource &s, float32 u, float32 v) noexcept {
			return u >= s.zoneX && u <= s.zoneX + s.zoneW && v >= s.zoneY && v <= s.zoneY + s.zoneH;
		}

	} // namespace

	// =========================================================================
	// NkInputBinding : constructeurs et With*
	// =========================================================================

	NkInputBinding NkInputBinding::Code(NkInputActionId action, const NkInputCode &code) noexcept {
		NkInputBinding b;
		b.action = action;
		b.source.kind = NkInputSourceKind::NK_INPUT_SRC_CODE;
		b.source.code = code;
		return b;
	}

	NkInputBinding NkInputBinding::Key(NkInputActionId action, NkKey key) noexcept {
		return Code(action, NkInputCode::Key(key));
	}

	NkInputBinding NkInputBinding::GamepadButton(NkInputActionId action, NkGamepadButton button) noexcept {
		return Code(action, NkInputCode::Gamepad(button));
	}

	NkInputBinding NkInputBinding::GamepadAxis(NkInputActionId action, NkGamepadAxis axis) noexcept {
		return Code(action, NkInputCode::GamepadAxis(axis));
	}

	NkInputBinding NkInputBinding::Composite1D(NkInputActionId action, const NkInputCode &positive,
											   const NkInputCode &negative) noexcept {
		NkInputBinding b;
		b.action = action;
		b.source.kind = NkInputSourceKind::NK_INPUT_SRC_COMPOSITE;
		b.source.parts[0] = positive;
		b.source.parts[1] = negative;
		b.source.partCount = 2;
		return b;
	}

	NkInputBinding NkInputBinding::Composite2D(NkInputActionId action, const NkInputCode &right,
											   const NkInputCode &left, const NkInputCode &up,
											   const NkInputCode &down) noexcept {
		NkInputBinding b;
		b.action = action;
		b.source.kind = NkInputSourceKind::NK_INPUT_SRC_COMPOSITE;
		b.source.parts[0] = right;
		b.source.parts[1] = left;
		b.source.parts[2] = up;
		b.source.parts[3] = down;
		b.source.partCount = 4;
		return b;
	}

	NkInputBinding NkInputBinding::Stick(NkInputActionId action, bool right) noexcept {
		NkInputBinding b;
		b.action = action;
		b.source.kind = NkInputSourceKind::NK_INPUT_SRC_STICK;
		b.source.rightStick = right;
		return b;
	}

	NkInputBinding NkInputBinding::MouseDelta(NkInputActionId action) noexcept {
		NkInputBinding b;
		b.action = action;
		b.source.kind = NkInputSourceKind::NK_INPUT_SRC_MOUSE_DELTA;
		return b;
	}

	NkInputBinding NkInputBinding::Gesture(NkInputActionId action, NkInputGesture gesture) noexcept {
		NkInputBinding b;
		b.action = action;
		b.source.kind = NkInputSourceKind::NK_INPUT_SRC_GESTURE;
		b.source.gesture = gesture;
		return b;
	}

	NkInputBinding NkInputBinding::ScreenButton(NkInputActionId action, float32 x, float32 y, float32 w,
												float32 h) noexcept {
		NkInputBinding b;
		b.action = action;
		b.source.kind = NkInputSourceKind::NK_INPUT_SRC_SCREEN_BUTTON;
		b.source.zoneX = x;
		b.source.zoneY = y;
		b.source.zoneW = w;
		b.source.zoneH = h;
		return b;
	}

	NkInputBinding NkInputBinding::ScreenStick(NkInputActionId action, float32 x, float32 y, float32 w, float32 h,
											   float32 radius) noexcept {
		NkInputBinding b = ScreenButton(action, x, y, w, h);
		b.source.kind = NkInputSourceKind::NK_INPUT_SRC_SCREEN_STICK;
		b.source.radius = radius > 0.f ? radius : 0.08f;
		return b;
	}

	NkInputBinding NkInputBinding::WithDeadZone(NkInputDeadZone kind, float32 minimum, float32 maximum) const noexcept {
		NkInputBinding b = *this;
		b.modifiers.deadZone = kind;
		b.modifiers.deadZoneMin = minimum < 0.f ? 0.f : minimum;
		b.modifiers.deadZoneMax = maximum > b.modifiers.deadZoneMin ? maximum : 1.f;
		return b;
	}

	NkInputBinding NkInputBinding::WithScale(float32 x, float32 y) const noexcept {
		NkInputBinding b = *this;
		b.modifiers.scaleX = x;
		b.modifiers.scaleY = y;
		return b;
	}

	NkInputBinding NkInputBinding::Inverted(bool x, bool y) const noexcept {
		NkInputBinding b = *this;
		b.modifiers.invertX = x;
		b.modifiers.invertY = y;
		return b;
	}

	NkInputBinding NkInputBinding::SwappedXY() const noexcept {
		NkInputBinding b = *this;
		b.modifiers.swapXY = true;
		return b;
	}

	NkInputBinding NkInputBinding::WithCurve(float32 exponent) const noexcept {
		NkInputBinding b = *this;
		b.modifiers.curve = exponent > 0.f ? exponent : 1.f;
		return b;
	}

	NkInputBinding NkInputBinding::WithSmoothing(float32 seconds) const noexcept {
		NkInputBinding b = *this;
		b.modifiers.smoothing = seconds > 0.f ? seconds : 0.f;
		return b;
	}

	NkInputBinding NkInputBinding::WithTrigger(NkInputTriggerKind kind, float32 time) const noexcept {
		NkInputBinding b = *this;
		b.trigger.kind = kind;
		b.trigger.time = time > 0.f ? time : 0.f;
		return b;
	}

	NkInputBinding NkInputBinding::WithChord(const NkInputCode &code) const noexcept {
		NkInputBinding b = *this;
		if (b.chordCount < NK_INPUT_CHORD_MAX) {
			b.chord[b.chordCount] = code;
			++b.chordCount;
		}
		return b;
	}

	// =========================================================================
	// NkInputContext
	// =========================================================================

	int32 NkInputContext::Add(const NkInputBinding &binding) {
		bindings.PushBack(binding);
		return static_cast<int32>(bindings.Size()) - 1;
	}

	bool NkInputContext::Remove(int32 index) noexcept {
		if (index < 0 || index >= Count()) {
			return false;
		}
		bindings.Erase(bindings.begin() + index);
		return true;
	}

	// =========================================================================
	// NkInputMap : actions et contextes
	// =========================================================================

	NkInputMap::NkInputMap() {
	}

	NkInputActionId NkInputMap::DeclareAction(const char *name, NkInputValueType type, float32 threshold) {
		if (name == nullptr || name[0] == '\0') {
			return NK_INPUT_ACTION_INVALID;
		}
		NkActionDef def;
		int32 k = 0;
		while (name[k] != '\0' && k < static_cast<int32>(sizeof(def.name)) - 1) {
			// Un nom est UN mot du texte : une espace le couperait en deux.
			def.name[k] = (name[k] == ' ' || name[k] == '\t') ? '_' : name[k];
			++k;
		}
		// Cherche sous le nom RETENU : « Mini jeu » declare deux fois reste UNE
		// action (« Mini_jeu »).
		const NkInputActionId existing = FindAction(def.name);
		if (existing != NK_INPUT_ACTION_INVALID) {
			return existing;
		}
		def.type = type;
		mDefs.PushBack(def);
		NkInputActionState st;
		st.threshold = threshold > 0.f ? threshold : 0.5f;
		mStates.PushBack(st);
		mSums.PushBack(NkVec2f(0.f, 0.f));
		mBound.PushBack(0);
		mManual.PushBack(0);
		return static_cast<NkInputActionId>(mDefs.Size()) - 1;
	}

	NkInputActionId NkInputMap::FindAction(const char *name) const noexcept {
		if (name == nullptr) {
			return NK_INPUT_ACTION_INVALID;
		}
		for (uint32 i = 0; i < mDefs.Size(); ++i) {
			if (NkInputNameEquals(mDefs[i].name, name)) {
				return static_cast<NkInputActionId>(i);
			}
		}
		return NK_INPUT_ACTION_INVALID;
	}

	int32 NkInputMap::ActionCount() const noexcept {
		return static_cast<int32>(mDefs.Size());
	}

	const char *NkInputMap::ActionName(NkInputActionId id) const noexcept {
		if (id < 0 || id >= ActionCount()) {
			return "";
		}
		return mDefs[static_cast<uint32>(id)].name;
	}

	NkInputValueType NkInputMap::ActionType(NkInputActionId id) const noexcept {
		if (id < 0 || id >= ActionCount()) {
			return NkInputValueType::NK_INPUT_BUTTON;
		}
		return mDefs[static_cast<uint32>(id)].type;
	}

	void NkInputMap::Clear() {
		mDefs.Clear();
		mStates.Clear();
		mSums.Clear();
		mBound.Clear();
		mManual.Clear();
		mContexts.Clear();
		mBindingStates.Clear();
		mCallbacks.Clear();
		mRebind = NkRebindState{};
	}

	int32 NkInputMap::AddContext(const char *name, int32 priority, bool consume) {
		NkInputContext c;
		c.name = NkString(name != nullptr && name[0] != '\0' ? name : "Defaut");
		// Un nom est UN mot du texte : une espace le couperait en deux a la
		// relecture (Save puis Load).
		for (uint32 k = 0; k < c.name.Length(); ++k) {
			if (c.name[k] == ' ' || c.name[k] == '\t') {
				c.name[k] = '_';
			}
		}
		const int32 existing = FindContext(c.name.CStr());
		if (existing >= 0) {
			return existing;
		}
		c.priority = priority;
		c.consume = consume;
		mContexts.PushBack(c);
		mBindingStates.PushBack(NkVector<NkBindingState>());
		return static_cast<int32>(mContexts.Size()) - 1;
	}

	int32 NkInputMap::FindContext(const char *name) const noexcept {
		if (name == nullptr) {
			return -1;
		}
		for (uint32 i = 0; i < mContexts.Size(); ++i) {
			if (NkInputNameEquals(mContexts[i].name.CStr(), name)) {
				return static_cast<int32>(i);
			}
		}
		return -1;
	}

	int32 NkInputMap::ContextCount() const noexcept {
		return static_cast<int32>(mContexts.Size());
	}

	NkInputContext *NkInputMap::Context(int32 index) noexcept {
		if (index < 0 || index >= ContextCount()) {
			return nullptr;
		}
		return &mContexts[static_cast<uint32>(index)];
	}

	const NkInputContext *NkInputMap::Context(int32 index) const noexcept {
		if (index < 0 || index >= ContextCount()) {
			return nullptr;
		}
		return &mContexts[static_cast<uint32>(index)];
	}

	bool NkInputMap::SetContextEnabled(int32 index, bool enabled) noexcept {
		NkInputContext *c = Context(index);
		if (c == nullptr) {
			return false;
		}
		if (c->enabled && !enabled) {
			// Ce que ses liaisons tenaient ne doit pas rester tenu : son etat
			// repart de zero, et l'image suivante recalcule sans elle.
			NkVector<NkBindingState> &etats = mBindingStates[static_cast<uint32>(index)];
			for (uint32 i = 0; i < etats.Size(); ++i) {
				etats[i] = NkBindingState{};
			}
		}
		c->enabled = enabled;
		return true;
	}

	bool NkInputMap::SetContextEnabled(const char *name, bool enabled) noexcept {
		return SetContextEnabled(FindContext(name), enabled);
	}

	void NkInputMap::BindingsChanged() noexcept {
		ResetBindingStates();
	}

	void NkInputMap::ResetBindingStates() noexcept {
		for (uint32 c = 0; c < mContexts.Size(); ++c) {
			NkVector<NkBindingState> &etats = mBindingStates[c];
			etats.Clear();
			for (uint32 i = 0; i < mContexts[c].bindings.Size(); ++i) {
				etats.PushBack(NkBindingState{});
			}
		}
		for (uint32 d = 0; d < NK_MAX_TOUCH_POINTS; ++d) {
			mFingers[d].stickContext = -1;
			mFingers[d].stickBinding = -1;
		}
	}

	// =========================================================================
	// Peripheriques
	// =========================================================================

	void NkInputMap::SetGamepad(int32 index) noexcept {
		if (index == NK_INPUT_ANY_GAMEPAD || index == NK_INPUT_NO_GAMEPAD) {
			mGamepad = index;
			return;
		}
		mGamepad = (index >= 0 && index < static_cast<int32>(NK_MAX_GAMEPADS)) ? index : NK_INPUT_NO_GAMEPAD;
	}

	void NkInputMap::SetAcceptKeyboardMouse(bool accept) noexcept {
		mAcceptKeyboardMouse = accept;
		if (!accept) {
			for (uint32 k = 0; k < kNbKeys; ++k) {
				mKeys[k] = false;
			}
			for (uint32 b = 0; b < kNbMouse; ++b) {
				mMouse[b] = false;
			}
		}
	}

	void NkInputMap::SetAcceptTouch(bool accept) noexcept {
		mAcceptTouch = accept;
		if (!accept) {
			for (uint32 d = 0; d < NK_MAX_TOUCH_POINTS; ++d) {
				mFingers[d].active = false;
			}
		}
	}

	void NkInputMap::SetScreenSurface(float32 x, float32 y, float32 w, float32 h) noexcept {
		mSurfX = x;
		mSurfY = y;
		mSurfW = w;
		mSurfH = h;
	}

	void NkInputMap::Normalize(float32 x, float32 y, float32 &u, float32 &v) const noexcept {
		if (mSurfW <= 0.f || mSurfH <= 0.f) {
			u = -1.f;
			v = -1.f;
			return;
		}
		u = (x - mSurfX) / mSurfW;
		v = (y - mSurfY) / mSurfH;
	}

	bool NkInputMap::IsKeyHeld(NkKey key) const noexcept {
		const uint32 k = static_cast<uint32>(key);
		return k < kNbKeys && mKeys[k];
	}

	uint32 NkInputMap::TouchCount() const noexcept {
		uint32 n = 0;
		for (uint32 d = 0; d < NK_MAX_TOUCH_POINTS; ++d) {
			n += mFingers[d].active ? 1u : 0u;
		}
		return n;
	}

	bool NkInputMap::AnyBindingUses(const NkInputCode &code) const noexcept {
		for (uint32 c = 0; c < mContexts.Size(); ++c) {
			const NkInputContext &ctx = mContexts[c];
			if (!ctx.enabled) {
				continue;
			}
			for (uint32 i = 0; i < ctx.bindings.Size(); ++i) {
				const NkInputSource &s = ctx.bindings[i].source;
				if (s.kind == NkInputSourceKind::NK_INPUT_SRC_CODE && s.code.device == code.device &&
					s.code.code == code.code) {
					return true;
				}
				if (s.kind == NkInputSourceKind::NK_INPUT_SRC_COMPOSITE) {
					for (uint32 p = 0; p < s.partCount; ++p) {
						if (s.parts[p].device == code.device && s.parts[p].code == code.code) {
							return true;
						}
					}
				}
			}
		}
		return false;
	}

	bool NkInputMap::AnyKindBinding(NkInputSourceKind kind) const noexcept {
		for (uint32 c = 0; c < mContexts.Size(); ++c) {
			const NkInputContext &ctx = mContexts[c];
			for (uint32 i = 0; ctx.enabled && i < ctx.bindings.Size(); ++i) {
				if (ctx.bindings[i].source.kind == kind) {
					return true;
				}
			}
		}
		return false;
	}

	bool NkInputMap::AnyGestureBinding(NkInputGesture gesture) const noexcept {
		for (uint32 c = 0; c < mContexts.Size(); ++c) {
			const NkInputContext &ctx = mContexts[c];
			for (uint32 i = 0; ctx.enabled && i < ctx.bindings.Size(); ++i) {
				const NkInputSource &s = ctx.bindings[i].source;
				if (s.kind == NkInputSourceKind::NK_INPUT_SRC_GESTURE && s.gesture == gesture) {
					return true;
				}
			}
		}
		return false;
	}

	bool NkInputMap::FingerStick(const NkFinger &f, const NkInputSource *&source) const noexcept {
		source = nullptr;
		if (f.stickContext < 0 || f.stickContext >= ContextCount()) {
			return false;
		}
		const NkInputContext &ctx = mContexts[static_cast<uint32>(f.stickContext)];
		// ⚠️ Une liaison retiree a la main sans BindingsChanged() laisserait un
		//    indice perime : on le verifie plutot que de lire hors du tableau.
		if (f.stickBinding < 0 || f.stickBinding >= ctx.Count()) {
			return false;
		}
		source = &ctx.bindings[static_cast<uint32>(f.stickBinding)].source;
		return true;
	}

	/// Le joystick sous un doigt qui vient de se poser : un seul doigt par
	/// joystick (le second peut viser un bouton de la meme zone).
	void NkInputMap::CaptureFinger(NkFinger &f) noexcept {
		f.stickContext = -1;
		f.stickBinding = -1;
		for (uint32 c = 0; c < mContexts.Size(); ++c) {
			const NkInputContext &ctx = mContexts[c];
			if (!ctx.enabled) {
				continue;
			}
			for (uint32 i = 0; i < ctx.bindings.Size(); ++i) {
				const NkInputSource &s = ctx.bindings[i].source;
				if (s.kind != NkInputSourceKind::NK_INPUT_SRC_SCREEN_STICK || !Inside(s, f.u, f.v)) {
					continue;
				}
				bool taken = false;
				for (uint32 d = 0; d < NK_MAX_TOUCH_POINTS; ++d) {
					const NkFinger &o = mFingers[d];
					const NkInputSource *os = nullptr;
					if (&o == &f || !o.active || !FingerStick(o, os)) {
						continue;
					}
					if (static_cast<uint32>(o.stickContext) == c && SameZone(*os, s)) {
						taken = true;
						break;
					}
				}
				if (!taken) {
					f.stickContext = static_cast<int32>(c);
					f.stickBinding = static_cast<int32>(i);
					return;
				}
			}
		}
	}

	// =========================================================================
	// Lecture des evenements
	// =========================================================================

	bool NkInputMap::Read(const NkEvent &e) noexcept {
		// ── Clavier ──
		if (const auto *k = e.As<NkKeyPressEvent>()) {
			if (!mAcceptKeyboardMouse) {
				return false;
			}
			if (mRebind.active) {
				if (mRebind.escapeCancels && k->GetKey() == NkKey::NK_ESCAPE) {
					CancelRebind();
				} else {
					FinishRebind(NkInputCode::Key(k->GetKey()));
				}
				return true; // la touche de la capture ne presse rien
			}
			const uint32 i = static_cast<uint32>(k->GetKey());
			if (i < kNbKeys) {
				mKeys[i] = true;
			}
			return AnyBindingUses(NkInputCode::Key(k->GetKey()));
		}
		if (const auto *k = e.As<NkKeyRepeatEvent>()) {
			return mAcceptKeyboardMouse && AnyBindingUses(NkInputCode::Key(k->GetKey()));
		}
		if (const auto *k = e.As<NkKeyReleaseEvent>()) {
			if (!mAcceptKeyboardMouse) {
				return false;
			}
			const uint32 i = static_cast<uint32>(k->GetKey());
			if (i < kNbKeys) {
				mKeys[i] = false;
			}
			return AnyBindingUses(NkInputCode::Key(k->GetKey()));
		}
		// ── Souris ──
		if (const auto *m = e.As<NkMouseButtonPressEvent>()) {
			if (!mAcceptKeyboardMouse) {
				return false;
			}
			if (mRebind.active) {
				FinishRebind(NkInputCode::Mouse(m->GetButton()));
				return true;
			}
			const uint32 i = static_cast<uint32>(m->GetButton());
			if (i < kNbMouse) {
				mMouse[i] = true;
			}
			return AnyBindingUses(NkInputCode::Mouse(m->GetButton()));
		}
		if (const auto *m = e.As<NkMouseButtonReleaseEvent>()) {
			if (!mAcceptKeyboardMouse) {
				return false;
			}
			const uint32 i = static_cast<uint32>(m->GetButton());
			if (i < kNbMouse) {
				mMouse[i] = false;
			}
			return AnyBindingUses(NkInputCode::Mouse(m->GetButton()));
		}
		if (const auto *m = e.As<NkMouseMoveEvent>()) {
			if (!mAcceptKeyboardMouse) {
				return false;
			}
			mMouseDX += static_cast<float32>(m->GetDeltaX());
			mMouseDY += static_cast<float32>(m->GetDeltaY());
			return AnyKindBinding(NkInputSourceKind::NK_INPUT_SRC_MOUSE_DELTA);
		}
		if (const auto *w = e.As<NkMouseWheelVerticalEvent>()) {
			if (!mAcceptKeyboardMouse) {
				return false;
			}
			mWheelV += static_cast<float32>(w->GetDeltaY());
			return AnyBindingUses(NkInputCode::Wheel(false));
		}
		if (const auto *w = e.As<NkMouseWheelHorizontalEvent>()) {
			if (!mAcceptKeyboardMouse) {
				return false;
			}
			mWheelH += static_cast<float32>(w->GetDeltaX());
			return AnyBindingUses(NkInputCode::Wheel(true));
		}

		if (!mAcceptTouch) {
			return false;
		}
		// ── Gestes ──
		if (const auto *g = e.As<NkGestureTapEvent>()) {
			const uint32 i = g->GetTapCount() >= 2u ? static_cast<uint32>(NkInputGesture::NK_INPUT_GESTURE_DOUBLE_TAP)
													: static_cast<uint32>(NkInputGesture::NK_INPUT_GESTURE_TAP);
			mGestureImpulse[i] = 1.f;
			return AnyGestureBinding(static_cast<NkInputGesture>(i));
		}
		if (e.As<NkGestureLongPressEvent>() != nullptr) {
			mGestureImpulse[static_cast<uint32>(NkInputGesture::NK_INPUT_GESTURE_LONG_PRESS)] = 1.f;
			return AnyGestureBinding(NkInputGesture::NK_INPUT_GESTURE_LONG_PRESS);
		}
		if (const auto *g = e.As<NkGestureSwipeEvent>()) {
			NkInputGesture k = NkInputGesture::NK_INPUT_GESTURE_SWIPE_LEFT;
			if (g->IsRight()) {
				k = NkInputGesture::NK_INPUT_GESTURE_SWIPE_RIGHT;
			} else if (g->IsUp()) {
				k = NkInputGesture::NK_INPUT_GESTURE_SWIPE_UP;
			} else if (g->IsDown()) {
				k = NkInputGesture::NK_INPUT_GESTURE_SWIPE_DOWN;
			}
			mGestureImpulse[static_cast<uint32>(k)] = 1.f;
			return AnyGestureBinding(k);
		}
		if (const auto *g = e.As<NkGesturePinchEvent>()) {
			mGestureImpulse[static_cast<uint32>(NkInputGesture::NK_INPUT_GESTURE_PINCH)] += g->GetScaleDelta();
			return AnyGestureBinding(NkInputGesture::NK_INPUT_GESTURE_PINCH);
		}
		if (const auto *g = e.As<NkGestureRotateEvent>()) {
			mGestureImpulse[static_cast<uint32>(NkInputGesture::NK_INPUT_GESTURE_ROTATE)] += g->GetAngleDelta();
			return AnyGestureBinding(NkInputGesture::NK_INPUT_GESTURE_ROTATE);
		}
		if (const auto *g = e.As<NkGesturePanEvent>()) {
			mPanX += g->GetDeltaX();
			mPanY += g->GetDeltaY();
			return AnyGestureBinding(NkInputGesture::NK_INPUT_GESTURE_PAN);
		}

		// ── Doigts ──
		const NkEventType::Value type = e.GetType();
		if (type == NkEventType::NK_TOUCH_CANCEL) {
			for (uint32 d = 0; d < NK_MAX_TOUCH_POINTS; ++d) {
				mFingers[d].active = false;
				mFingers[d].stickContext = -1;
			}
			return true;
		}
		if (type != NkEventType::NK_TOUCH_BEGIN && type != NkEventType::NK_TOUCH_MOVE &&
			type != NkEventType::NK_TOUCH_END) {
			return false;
		}
		const NkTouchEvent &t = static_cast<const NkTouchEvent &>(e);
		bool taken = false;
		for (uint32 k = 0; k < t.GetNumTouches(); ++k) {
			const NkTouchPoint &p = t.GetTouch(k);
			int32 slot = -1;
			int32 libre = -1;
			for (uint32 d = 0; d < NK_MAX_TOUCH_POINTS; ++d) {
				if (mFingers[d].active && mFingers[d].id == p.id) {
					slot = static_cast<int32>(d);
					break;
				}
				if (!mFingers[d].active && libre < 0) {
					libre = static_cast<int32>(d);
				}
			}
			if (type == NkEventType::NK_TOUCH_BEGIN && slot < 0) {
				if (libre < 0) {
					continue;
				}
				slot = libre;
				NkFinger &f = mFingers[static_cast<uint32>(slot)];
				f.id = p.id;
				f.active = true;
				Normalize(p.clientX, p.clientY, f.u, f.v);
				f.u0 = f.u;
				f.v0 = f.v;
				CaptureFinger(f);
			}
			if (slot < 0) {
				continue;
			}
			NkFinger &f = mFingers[static_cast<uint32>(slot)];
			Normalize(p.clientX, p.clientY, f.u, f.v);
			if (f.stickContext >= 0) {
				taken = true;
			}
			for (uint32 c = 0; c < mContexts.Size() && !taken; ++c) {
				const NkInputContext &ctx = mContexts[c];
				for (uint32 i = 0; ctx.enabled && i < ctx.bindings.Size() && !taken; ++i) {
					const NkInputSource &s = ctx.bindings[i].source;
					if (s.kind == NkInputSourceKind::NK_INPUT_SRC_SCREEN_BUTTON && Inside(s, f.u, f.v)) {
						taken = true;
					}
				}
			}
			if (type == NkEventType::NK_TOUCH_END) {
				f.active = false;
				f.stickContext = -1;
				f.stickBinding = -1;
			}
		}
		return taken;
	}

	// =========================================================================
	// Les sources
	// =========================================================================

	bool NkInputMap::PadButton(NkGamepadButton button, const NkGamepadSystem *pads) const noexcept {
		if (pads == nullptr || mGamepad == NK_INPUT_NO_GAMEPAD) {
			return false;
		}
		if (mGamepad == NK_INPUT_ANY_GAMEPAD) {
			for (uint32 p = 0; p < NK_MAX_GAMEPADS; ++p) {
				if (pads->IsButtonDown(p, button)) {
					return true;
				}
			}
			return false;
		}
		return pads->IsButtonDown(static_cast<uint32>(mGamepad), button);
	}

	float32 NkInputMap::PadAxis(NkGamepadAxis axis, const NkGamepadSystem *pads) const noexcept {
		if (pads == nullptr || mGamepad == NK_INPUT_NO_GAMEPAD) {
			return 0.f;
		}
		if (mGamepad == NK_INPUT_ANY_GAMEPAD) {
			// Toutes les manettes : la plus poussee l'emporte (un joueur seul
			// qui change de manette ne doit rien avoir a configurer).
			float32 best = 0.f;
			for (uint32 p = 0; p < NK_MAX_GAMEPADS; ++p) {
				const float32 v = pads->GetAxis(p, axis);
				if (math::NkFabs(v) > math::NkFabs(best)) {
					best = v;
				}
			}
			return best;
		}
		return pads->GetAxis(static_cast<uint32>(mGamepad), axis);
	}

	float32 NkInputMap::CodeValue(const NkInputCode &c, const NkGamepadSystem *pads) const noexcept {
		switch (c.device) {
			case NkInputDevice::NK_KEYBOARD:
				return (c.code < kNbKeys && mKeys[c.code]) ? 1.f : 0.f;
			case NkInputDevice::NK_MOUSE:
				return (c.code < kNbMouse && mMouse[c.code]) ? 1.f : 0.f;
			case NkInputDevice::NK_MOUSEWHEEL:
				return c.code == 1u ? mWheelH : mWheelV;
			case NkInputDevice::NK_GAMEPAD:
				return PadButton(static_cast<NkGamepadButton>(c.code), pads) ? 1.f : 0.f;
			case NkInputDevice::NK_GAMEPAD_AXIS:
				return PadAxis(static_cast<NkGamepadAxis>(c.code), pads);
		}
		return 0.f;
	}

	bool NkInputMap::ChordHeld(const NkInputBinding &b, const NkGamepadSystem *pads) const noexcept {
		for (uint32 i = 0; i < b.chordCount; ++i) {
			if (math::NkFabs(CodeValue(b.chord[i], pads)) < 0.5f) {
				return false;
			}
		}
		return true;
	}

	NkVec2f NkInputMap::EvaluateSource(const NkInputBinding &b, int32 ctx, int32 index,
									   const NkGamepadSystem *pads) const noexcept {
		(void)index;
		const NkInputSource &s = b.source;
		switch (s.kind) {
			case NkInputSourceKind::NK_INPUT_SRC_CODE: {
				const float32 v = CodeValue(s.code, pads);
				// Zone morte RADIALE sur UN axe de stick : il faut son jumeau, la
				// longueur du vecteur est la seule mesure juste.
				if (s.code.device == NkInputDevice::NK_GAMEPAD_AXIS &&
					b.modifiers.deadZone == NkInputDeadZone::NK_INPUT_DEADZONE_RADIAL) {
					bool stick = false;
					const NkGamepadAxis twin = Twin(static_cast<NkGamepadAxis>(s.code.code), stick);
					if (stick) {
						return NkVec2f(v, PadAxis(twin, pads));
					}
				}
				return NkVec2f(v, 0.f);
			}
			case NkInputSourceKind::NK_INPUT_SRC_COMPOSITE: {
				if (s.partCount >= 4) {
					const float32 x = CodeValue(s.parts[0], pads) - CodeValue(s.parts[1], pads);
					const float32 y = CodeValue(s.parts[2], pads) - CodeValue(s.parts[3], pads);
					return NkVec2f(x, y);
				}
				return NkVec2f(CodeValue(s.parts[0], pads) - CodeValue(s.parts[1], pads), 0.f);
			}
			case NkInputSourceKind::NK_INPUT_SRC_STICK: {
				const NkGamepadAxis ax = s.rightStick ? NkGamepadAxis::NK_GP_AXIS_RX : NkGamepadAxis::NK_GP_AXIS_LX;
				const NkGamepadAxis ay = s.rightStick ? NkGamepadAxis::NK_GP_AXIS_RY : NkGamepadAxis::NK_GP_AXIS_LY;
				return NkVec2f(PadAxis(ax, pads), PadAxis(ay, pads));
			}
			case NkInputSourceKind::NK_INPUT_SRC_MOUSE_DELTA:
				return NkVec2f(mMouseDX, mMouseDY);
			case NkInputSourceKind::NK_INPUT_SRC_GESTURE: {
				if (s.gesture == NkInputGesture::NK_INPUT_GESTURE_PAN) {
					return NkVec2f(mPanX, mPanY);
				}
				return NkVec2f(mGestureImpulse[static_cast<uint32>(s.gesture)], 0.f);
			}
			case NkInputSourceKind::NK_INPUT_SRC_SCREEN_BUTTON: {
				for (uint32 d = 0; d < NK_MAX_TOUCH_POINTS; ++d) {
					const NkFinger &f = mFingers[d];
					// Un doigt qui tient un joystick ne presse pas un bouton en y
					// glissant : la regle de tous les jeux mobiles.
					if (f.active && f.stickContext < 0 && Inside(s, f.u, f.v)) {
						return NkVec2f(1.f, 0.f);
					}
				}
				return NkVec2f(0.f, 0.f);
			}
			case NkInputSourceKind::NK_INPUT_SRC_SCREEN_STICK: {
				for (uint32 d = 0; d < NK_MAX_TOUCH_POINTS; ++d) {
					const NkFinger &f = mFingers[d];
					const NkInputSource *capteur = nullptr;
					if (!f.active || f.stickContext != ctx || !FingerStick(f, capteur)) {
						continue;
					}
					if (!SameZone(*capteur, s)) {
						continue;
					}
					// En PIXELS, rapporte au rayon : un joystick est rond a l'ecran,
					// pas etire comme la surface.
					const float32 r = s.radius * mSurfH;
					if (r <= 0.f) {
						return NkVec2f(0.f, 0.f);
					}
					float32 x = (f.u - f.u0) * mSurfW / r;
					float32 y = -(f.v - f.v0) * mSurfH / r; // l'ecran descend, le stick monte
					const float32 m = math::NkSqrt(x * x + y * y);
					if (m > 1.f) {
						x /= m;
						y /= m;
					}
					return NkVec2f(x, y);
				}
				return NkVec2f(0.f, 0.f);
			}
			case NkInputSourceKind::NK_INPUT_SRC_NONE:
				break;
		}
		return NkVec2f(0.f, 0.f);
	}

	NkVec2f NkInputMap::ApplyModifiers(const NkInputModifiers &m, NkVec2f v, bool radialPair, NkBindingState &state,
									   float32 dt) noexcept {
		// ── 1. Zone morte ──
		if (m.deadZone == NkInputDeadZone::NK_INPUT_DEADZONE_AXIAL) {
			v.x = DeadZone1(v.x, m.deadZoneMin, m.deadZoneMax);
			v.y = DeadZone1(v.y, m.deadZoneMin, m.deadZoneMax);
		} else if (m.deadZone == NkInputDeadZone::NK_INPUT_DEADZONE_RADIAL) {
			const float32 len = Length(v);
			const float32 k = DeadZone1(len, m.deadZoneMin, m.deadZoneMax);
			if (len > 0.f) {
				v.x = v.x * k / len;
				v.y = v.y * k / len;
			}
		}
		// Le jumeau n'a servi qu'a mesurer la longueur : l'axe seul continue.
		if (radialPair) {
			v.y = 0.f;
		}
		// ── 2. Courbe (sur la longueur : la direction ne tourne pas) ──
		if (m.curve != 1.f) {
			const float32 len = Length(v);
			if (len > 0.f) {
				const float32 k = math::NkPow(len, m.curve) / len;
				v.x *= k;
				v.y *= k;
			}
		}
		// ── 3. Echelle, 4. inversion, 5. permutation ──
		v.x *= m.scaleX;
		v.y *= m.scaleY;
		if (m.invertX) {
			v.x = -v.x;
		}
		if (m.invertY) {
			v.y = -v.y;
		}
		if (m.swapXY) {
			const float32 t = v.x;
			v.x = v.y;
			v.y = t;
		}
		// ── 6. Lissage exponentiel, independant de la cadence ──
		if (m.smoothing > 0.f) {
			const float32 a = dt > 0.f ? 1.f - math::NkExp(-dt / m.smoothing) : 0.f;
			state.smoothX += (v.x - state.smoothX) * a;
			state.smoothY += (v.y - state.smoothY) * a;
			v.x = state.smoothX;
			v.y = state.smoothY;
		}
		return v;
	}

	// =========================================================================
	// Reservations (contextes qui consomment)
	// =========================================================================

	void NkInputMap::ReserveKey(uint8 kind, uint32 code) noexcept {
		if (KeyReserved(kind, code) || mReservedCount >= 256) {
			return;
		}
		mReserved[mReservedCount].kind = kind;
		mReserved[mReservedCount].code = code;
		++mReservedCount;
	}

	bool NkInputMap::KeyReserved(uint8 kind, uint32 code) const noexcept {
		for (int32 i = 0; i < mReservedCount; ++i) {
			if (mReserved[i].kind == kind && mReserved[i].code == code) {
				return true;
			}
		}
		return false;
	}

	void NkInputMap::Reserve(const NkInputBinding &b) noexcept {
		const NkInputSource &s = b.source;
		switch (s.kind) {
			case NkInputSourceKind::NK_INPUT_SRC_CODE:
				ReserveKey(kResCode, CodeKey(s.code));
				break;
			case NkInputSourceKind::NK_INPUT_SRC_COMPOSITE:
				for (uint32 p = 0; p < s.partCount; ++p) {
					ReserveKey(kResCode, CodeKey(s.parts[p]));
				}
				break;
			case NkInputSourceKind::NK_INPUT_SRC_STICK: {
				const NkGamepadAxis ax = s.rightStick ? NkGamepadAxis::NK_GP_AXIS_RX : NkGamepadAxis::NK_GP_AXIS_LX;
				const NkGamepadAxis ay = s.rightStick ? NkGamepadAxis::NK_GP_AXIS_RY : NkGamepadAxis::NK_GP_AXIS_LY;
				ReserveKey(kResCode, CodeKey(NkInputCode::GamepadAxis(ax)));
				ReserveKey(kResCode, CodeKey(NkInputCode::GamepadAxis(ay)));
				break;
			}
			case NkInputSourceKind::NK_INPUT_SRC_MOUSE_DELTA:
				ReserveKey(kResMouseDelta, 0u);
				break;
			case NkInputSourceKind::NK_INPUT_SRC_GESTURE:
				ReserveKey(kResGesture, static_cast<uint32>(s.gesture));
				break;
			case NkInputSourceKind::NK_INPUT_SRC_SCREEN_BUTTON:
			case NkInputSourceKind::NK_INPUT_SRC_SCREEN_STICK:
				// Le doigt est reserve par ZONE : deux contextes peuvent poser
				// des boutons a des endroits differents sans se gener.
				ReserveKey(kResTouch, static_cast<uint32>(s.zoneX * 1000.f) * 1000u + static_cast<uint32>(s.zoneY * 1000.f));
				break;
			case NkInputSourceKind::NK_INPUT_SRC_NONE:
				break;
		}
	}

	bool NkInputMap::IsReserved(const NkInputBinding &b) const noexcept {
		const NkInputSource &s = b.source;
		switch (s.kind) {
			case NkInputSourceKind::NK_INPUT_SRC_CODE:
				return KeyReserved(kResCode, CodeKey(s.code));
			case NkInputSourceKind::NK_INPUT_SRC_COMPOSITE:
				for (uint32 p = 0; p < s.partCount; ++p) {
					if (KeyReserved(kResCode, CodeKey(s.parts[p]))) {
						return true;
					}
				}
				return false;
			case NkInputSourceKind::NK_INPUT_SRC_STICK: {
				const NkGamepadAxis ax = s.rightStick ? NkGamepadAxis::NK_GP_AXIS_RX : NkGamepadAxis::NK_GP_AXIS_LX;
				return KeyReserved(kResCode, CodeKey(NkInputCode::GamepadAxis(ax)));
			}
			case NkInputSourceKind::NK_INPUT_SRC_MOUSE_DELTA:
				return KeyReserved(kResMouseDelta, 0u);
			case NkInputSourceKind::NK_INPUT_SRC_GESTURE:
				return KeyReserved(kResGesture, static_cast<uint32>(s.gesture));
			case NkInputSourceKind::NK_INPUT_SRC_SCREEN_BUTTON:
			case NkInputSourceKind::NK_INPUT_SRC_SCREEN_STICK:
				return KeyReserved(kResTouch,
								   static_cast<uint32>(s.zoneX * 1000.f) * 1000u + static_cast<uint32>(s.zoneY * 1000.f));
			case NkInputSourceKind::NK_INPUT_SRC_NONE:
				break;
		}
		return false;
	}

	void NkInputMap::SortedContexts(int32 *order, int32 &count) const noexcept {
		count = 0;
		for (uint32 c = 0; c < mContexts.Size() && count < 64; ++c) {
			if (mContexts[c].enabled) {
				order[count] = static_cast<int32>(c);
				++count;
			}
		}
		// Tri par insertion, STABLE : a priorite egale, l'ordre de declaration.
		for (int32 i = 1; i < count; ++i) {
			const int32 v = order[i];
			int32 j = i - 1;
			while (j >= 0 && mContexts[static_cast<uint32>(order[j])].priority < mContexts[static_cast<uint32>(v)].priority) {
				order[j + 1] = order[j];
				--j;
			}
			order[j + 1] = v;
		}
	}

	// =========================================================================
	// L'image
	// =========================================================================

	void NkInputMap::Update(float32 dt, const NkGamepadSystem *pads) noexcept {
		if (!math::NkIsFinite(dt) || dt < 0.f) {
			dt = 0.f;
		}
		RebindFromPads(pads);

		// L'etat des liaisons suit le nombre de liaisons (ajout a la main).
		for (uint32 c = 0; c < mContexts.Size(); ++c) {
			NkVector<NkBindingState> &etats = mBindingStates[c];
			while (etats.Size() < mContexts[c].bindings.Size()) {
				etats.PushBack(NkBindingState{});
			}
		}

		const uint32 nActions = mDefs.Size();
		for (uint32 a = 0; a < nActions; ++a) {
			mSums[a] = NkVec2f(0.f, 0.f);
			mBound[a] = 0;
		}
		mReservedCount = 0;
		mChordBlockedCount = 0;

		int32 order[64];
		int32 nOrder = 0;
		SortedContexts(order, nOrder);

		for (int32 oi = 0; oi < nOrder; ++oi) {
			const int32 ci = order[oi];
			NkInputContext &ctx = mContexts[static_cast<uint32>(ci)];
			NkVector<NkBindingState> &etats = mBindingStates[static_cast<uint32>(ci)];

			// ── Les ACCORDS tenus bloquent leur touche seule (Ctrl+S n'est pas S) ──
			for (uint32 i = 0; i < ctx.bindings.Size(); ++i) {
				const NkInputBinding &b = ctx.bindings[i];
				if (b.chordCount > 0 && b.source.kind == NkInputSourceKind::NK_INPUT_SRC_CODE && ChordHeld(b, pads) &&
					math::NkFabs(CodeValue(b.source.code, pads)) >= 0.5f && mChordBlockedCount < 32) {
					mChordBlocked[mChordBlockedCount] = b.source.code;
					++mChordBlockedCount;
				}
			}

			for (uint32 i = 0; i < ctx.bindings.Size(); ++i) {
				const NkInputBinding &b = ctx.bindings[i];
				if (b.action < 0 || static_cast<uint32>(b.action) >= nActions) {
					continue;
				}
				NkBindingState &st = etats[i];
				mBound[static_cast<uint32>(b.action)] = 1;
				if (IsReserved(b)) {
					st = NkBindingState{};
					continue;
				}
				bool blocked = false;
				if (b.chordCount == 0 && b.source.kind == NkInputSourceKind::NK_INPUT_SRC_CODE) {
					for (int32 k = 0; k < mChordBlockedCount; ++k) {
						if (mChordBlocked[k] == b.source.code) {
							blocked = true;
						}
					}
				}

				const bool radialPair = b.source.kind == NkInputSourceKind::NK_INPUT_SRC_CODE &&
										b.source.code.device == NkInputDevice::NK_GAMEPAD_AXIS &&
										b.modifiers.deadZone == NkInputDeadZone::NK_INPUT_DEADZONE_RADIAL;
				NkVec2f v = EvaluateSource(b, ci, static_cast<int32>(i), pads);
				v = ApplyModifiers(b.modifiers, v, radialPair, st, dt);
				if (blocked || !ChordHeld(b, pads)) {
					v = NkVec2f(0.f, 0.f);
				}

				// ── Le declencheur ──
				const float32 seuil = mStates[static_cast<uint32>(b.action)].threshold;
				const bool active = Length(v) >= seuil;
				const bool edgeDown = active && !st.wasActive;
				const bool edgeUp = !active && st.wasActive;
				st.sincePress += dt;
				NkVec2f out(0.f, 0.f);
				switch (b.trigger.kind) {
					case NkInputTriggerKind::NK_INPUT_TRIGGER_DOWN:
						out = v;
						break;
					case NkInputTriggerKind::NK_INPUT_TRIGGER_PRESSED:
						out = edgeDown ? v : NkVec2f(0.f, 0.f);
						break;
					case NkInputTriggerKind::NK_INPUT_TRIGGER_RELEASED:
						out = edgeUp ? NkVec2f(st.lastX, st.lastY) : NkVec2f(0.f, 0.f);
						break;
					case NkInputTriggerKind::NK_INPUT_TRIGGER_HOLD: {
						const float32 tenu = active ? (st.wasActive ? st.heldTime + dt : 0.f) : 0.f;
						out = (active && tenu + kEpsTemps >= b.trigger.time) ? v : NkVec2f(0.f, 0.f);
						break;
					}
					case NkInputTriggerKind::NK_INPUT_TRIGGER_TAP:
						out = (edgeUp && st.heldTime <= b.trigger.time + kEpsTemps) ? NkVec2f(st.lastX, st.lastY)
																		  : NkVec2f(0.f, 0.f);
						break;
					case NkInputTriggerKind::NK_INPUT_TRIGGER_DOUBLE_TAP:
						if (edgeDown && st.sincePress <= b.trigger.time + kEpsTemps) {
							out = v;
							st.sincePress = 1.0e9f; // un triple appui n'est pas deux doubles
						} else if (edgeDown) {
							st.sincePress = 0.f;
						}
						break;
				}
				if (b.trigger.kind != NkInputTriggerKind::NK_INPUT_TRIGGER_DOUBLE_TAP && edgeDown) {
					st.sincePress = 0.f;
				}
				if (active) {
					st.heldTime = st.wasActive ? st.heldTime + dt : 0.f;
					st.lastX = v.x;
					st.lastY = v.y;
				}
				st.wasActive = active;

				// ── Vers le type de l'action ──
				NkVec2f &sum = mSums[static_cast<uint32>(b.action)];
				switch (mDefs[static_cast<uint32>(b.action)].type) {
					case NkInputValueType::NK_INPUT_BUTTON:
						sum.x += Length(out);
						break;
					case NkInputValueType::NK_INPUT_AXIS1D:
						sum.x += out.x;
						break;
					case NkInputValueType::NK_INPUT_AXIS2D:
						sum.x += out.x;
						sum.y += out.y;
						break;
				}
			}
			if (ctx.consume) {
				for (uint32 i = 0; i < ctx.bindings.Size(); ++i) {
					Reserve(ctx.bindings[i]);
				}
			}
		}

		// ── Les actions : bornes, etats, temps tenu ──
		for (uint32 a = 0; a < nActions; ++a) {
			NkInputActionState &s = mStates[a];
			s.prevX = s.x;
			s.prevY = s.y;
			if (mBound[a] != 0) {
				mManual[a] = 0;
			} else if (mManual[a] == 0) {
				// Sans liaison allumee (son contexte vient de s'eteindre) et sans
				// valeur posee par programme : l'action RETOMBE a zero. Sans cette
				// branche, le bouton d'un menu ferme restait enfonce pour toujours.
				s.x = 0.f;
				s.y = 0.f;
			}
			if (mBound[a] != 0) {
				NkVec2f v = mSums[a];
				switch (mDefs[a].type) {
					case NkInputValueType::NK_INPUT_BUTTON:
						v.x = math::NkClamp(v.x, 0.f, 1.f);
						v.y = 0.f;
						break;
					case NkInputValueType::NK_INPUT_AXIS1D:
						v.x = math::NkClamp(v.x, -1.f, 1.f);
						v.y = 0.f;
						break;
					case NkInputValueType::NK_INPUT_AXIS2D: {
						const float32 len = Length(v);
						if (len > 1.f) {
							v.x /= len;
							v.y /= len;
						}
						break;
					}
				}
				s.x = v.x;
				s.y = v.y;
			}
			const bool wasDown = s.down;
			s.down = math::NkSqrt(s.x * s.x + s.y * s.y) >= s.threshold;
			s.pressed = s.down && !wasDown;
			s.released = !s.down && wasDown;
			s.heldTime = s.down ? (wasDown ? s.heldTime + dt : 0.f) : 0.f;
		}

		// Les deltas de l'image sont consommes par cette image.
		mWheelV = 0.f;
		mWheelH = 0.f;
		mMouseDX = 0.f;
		mMouseDY = 0.f;
		mPanX = 0.f;
		mPanY = 0.f;
		for (uint32 g = 0; g < static_cast<uint32>(NkInputGesture::NK_INPUT_GESTURE_COUNT); ++g) {
			mGestureImpulse[g] = 0.f;
		}

		FireCallbacks();
	}

	void NkInputMap::FireCallbacks() noexcept {
		for (uint32 i = 0; i < mCallbacks.Size(); ++i) {
			NkCallbackEntry &c = mCallbacks[i];
			if (c.action < 0 || c.action >= ActionCount() || !c.callback) {
				continue;
			}
			const NkInputActionState &s = mStates[static_cast<uint32>(c.action)];
			bool fire = false;
			switch (c.phase) {
				case NkInputPhase::NK_INPUT_STARTED:   fire = s.pressed;  break;
				case NkInputPhase::NK_INPUT_ONGOING:   fire = s.down;     break;
				case NkInputPhase::NK_INPUT_COMPLETED: fire = s.released; break;
			}
			if (fire) {
				c.callback(c.action, c.phase, s);
			}
		}
	}

	void NkInputMap::ReleaseAll() noexcept {
		for (uint32 k = 0; k < kNbKeys; ++k) {
			mKeys[k] = false;
		}
		for (uint32 b = 0; b < kNbMouse; ++b) {
			mMouse[b] = false;
		}
		for (uint32 d = 0; d < NK_MAX_TOUCH_POINTS; ++d) {
			mFingers[d] = NkFinger{};
		}
		mWheelV = 0.f;
		mWheelH = 0.f;
		mMouseDX = 0.f;
		mMouseDY = 0.f;
		mPanX = 0.f;
		mPanY = 0.f;
		for (uint32 g = 0; g < static_cast<uint32>(NkInputGesture::NK_INPUT_GESTURE_COUNT); ++g) {
			mGestureImpulse[g] = 0.f;
		}
		ResetBindingStates();
		// ⚠️ LE RELACHEMENT SE VOIT TOUT DE SUITE : un tir charge qui attend
		//    « relache » ne doit pas le rater parce que le jeu a perdu la main.
		for (uint32 a = 0; a < mStates.Size(); ++a) {
			NkInputActionState &s = mStates[a];
			const bool etait = s.down;
			s.prevX = s.x;
			s.prevY = s.y;
			s.x = 0.f;
			s.y = 0.f;
			s.down = false;
			s.pressed = false;
			s.released = etait;
			s.heldTime = 0.f;
		}
		for (uint32 i = 0; i < mCallbacks.Size(); ++i) {
			NkCallbackEntry &c = mCallbacks[i];
			if (c.phase != NkInputPhase::NK_INPUT_COMPLETED || c.action < 0 || c.action >= ActionCount() || !c.callback) {
				continue;
			}
			const NkInputActionState &s = mStates[static_cast<uint32>(c.action)];
			if (s.released) {
				c.callback(c.action, c.phase, s);
			}
		}
	}

	// =========================================================================
	// Interrogation
	// =========================================================================

	const NkInputActionState &NkInputMap::State(NkInputActionId id) const noexcept {
		static const NkInputActionState empty;
		if (id < 0 || id >= ActionCount()) {
			return empty;
		}
		return mStates[static_cast<uint32>(id)];
	}

	float32 NkInputMap::Value(NkInputActionId id) const noexcept {
		return State(id).x;
	}

	NkVec2f NkInputMap::Value2D(NkInputActionId id) const noexcept {
		const NkInputActionState &s = State(id);
		return NkVec2f(s.x, s.y);
	}

	bool NkInputMap::IsDown(NkInputActionId id) const noexcept {
		return State(id).down;
	}

	bool NkInputMap::WasPressed(NkInputActionId id) const noexcept {
		return State(id).pressed;
	}

	bool NkInputMap::WasReleased(NkInputActionId id) const noexcept {
		return State(id).released;
	}

	bool NkInputMap::IsBound(NkInputActionId id) const noexcept {
		if (id < 0 || id >= ActionCount()) {
			return false;
		}
		return mBound[static_cast<uint32>(id)] != 0;
	}

	float32 NkInputMap::HeldTime(NkInputActionId id) const noexcept {
		return State(id).heldTime;
	}

	void NkInputMap::SetValue(NkInputActionId id, float32 x, float32 y) noexcept {
		if (id < 0 || id >= ActionCount()) {
			return;
		}
		NkInputActionState &s = mStates[static_cast<uint32>(id)];
		s.x = x;
		s.y = y;
		mManual[static_cast<uint32>(id)] = 1;
	}

	uint32 NkInputMap::OnAction(NkInputActionId id, NkInputPhase phase, NkInputCallback callback) {
		NkCallbackEntry e;
		e.token = mNextToken;
		++mNextToken;
		e.action = id;
		e.phase = phase;
		e.callback = traits::NkMove(callback);
		mCallbacks.PushBack(traits::NkMove(e));
		return mCallbacks[mCallbacks.Size() - 1].token;
	}

	void NkInputMap::RemoveCallback(uint32 token) noexcept {
		for (uint32 i = 0; i < mCallbacks.Size(); ++i) {
			if (mCallbacks[i].token == token) {
				mCallbacks.Erase(mCallbacks.begin() + i);
				return;
			}
		}
	}

	// =========================================================================
	// Capture
	// =========================================================================

	bool NkInputMap::BeginRebind(int32 context, int32 binding, int32 part, bool escapeCancels) noexcept {
		const NkInputContext *c = Context(context);
		if (c == nullptr || binding < 0 || binding >= c->Count()) {
			return false;
		}
		const NkInputSource &s = c->bindings[static_cast<uint32>(binding)].source;
		const bool code = s.kind == NkInputSourceKind::NK_INPUT_SRC_CODE && part < 0;
		const bool piece = s.kind == NkInputSourceKind::NK_INPUT_SRC_COMPOSITE && part >= 0 &&
						   part < static_cast<int32>(s.partCount);
		if (!code && !piece) {
			return false;
		}
		mRebind = NkRebindState{};
		mRebind.active = true;
		mRebind.escapeCancels = escapeCancels;
		mRebind.context = context;
		mRebind.binding = binding;
		mRebind.part = part;
		return true;
	}

	void NkInputMap::CancelRebind() noexcept {
		mRebind.active = false;
		mRebind.succeeded = false;
	}

	void NkInputMap::FinishRebind(const NkInputCode &code) noexcept {
		NkInputContext *c = Context(mRebind.context);
		mRebind.active = false;
		if (c == nullptr || mRebind.binding >= c->Count()) {
			mRebind.succeeded = false;
			return;
		}
		NkInputSource &s = c->bindings[static_cast<uint32>(mRebind.binding)].source;
		if (mRebind.part >= 0) {
			s.parts[mRebind.part] = code;
		} else {
			s.code = code;
		}
		mRebind.succeeded = true;
		mRebind.result = code;
	}

	void NkInputMap::RebindFromPads(const NkGamepadSystem *pads) noexcept {
		for (uint32 p = 0; p < NK_MAX_GAMEPADS; ++p) {
			const bool mine = mGamepad == NK_INPUT_ANY_GAMEPAD || mGamepad == static_cast<int32>(p);
			const bool connected = pads != nullptr && mine && pads->IsConnected(p);
			for (uint32 b = 0; b < kNbPadButtons; ++b) {
				const bool down = connected && pads->IsButtonDown(p, static_cast<NkGamepadButton>(b));
				// Seul un bouton qui VIENT d'etre enfonce est pris : pas celui
				// qui l'etait deja quand le menu s'est ouvert.
				if (mRebind.active && down && !mPadButtonsBefore[p][b] && b != 0u) {
					FinishRebind(NkInputCode::Gamepad(static_cast<NkGamepadButton>(b)));
				}
				mPadButtonsBefore[p][b] = down;
			}
			if (!mRebind.active || !connected) {
				continue;
			}
			static const NkGamepadAxis kAxes[] = {
				NkGamepadAxis::NK_GP_AXIS_LX, NkGamepadAxis::NK_GP_AXIS_LY, NkGamepadAxis::NK_GP_AXIS_RX,
				NkGamepadAxis::NK_GP_AXIS_RY, NkGamepadAxis::NK_GP_AXIS_LT, NkGamepadAxis::NK_GP_AXIS_RT,
			};
			for (NkGamepadAxis a : kAxes) {
				if (math::NkFabs(pads->GetAxis(p, a)) > 0.5f) {
					FinishRebind(NkInputCode::GamepadAxis(a));
					break;
				}
			}
		}
	}

	// =========================================================================
	// NkInputPlayers
	// =========================================================================

	int32 NkInputPlayers::Add(NkInputMap &map) noexcept {
		if (mCount >= MAX_PLAYERS) {
			return -1;
		}
		mMaps[mCount] = &map;
		++mCount;
		return mCount - 1;
	}

	NkInputMap *NkInputPlayers::Player(int32 index) noexcept {
		if (index < 0 || index >= mCount) {
			return nullptr;
		}
		return mMaps[index];
	}

	int32 NkInputPlayers::PlayerForGamepad(int32 pad) const noexcept {
		for (int32 i = 0; i < mCount; ++i) {
			if (mMaps[i]->GetGamepad() == pad) {
				return i;
			}
		}
		return -1;
	}

	int32 NkInputPlayers::Update(const NkGamepadSystem *pads, NkGamepadButton joinButton) noexcept {
		int32 joined = -1;
		// Sans systeme de manettes, on ne sait RIEN : ce n'est pas « toutes
		// debranchees », et personne ne perd sa manette pour autant.
		if (pads == nullptr) {
			return joined;
		}
		for (uint32 p = 0; p < NK_MAX_GAMEPADS; ++p) {
			const bool connected = pads != nullptr && pads->IsConnected(p);
			for (uint32 b = 1; b < kNbPadButtons; ++b) {
				const bool down = connected && pads->IsButtonDown(p, static_cast<NkGamepadButton>(b));
				const bool wanted = joinButton == NkGamepadButton::NK_GP_UNKNOWN || static_cast<uint32>(joinButton) == b;
				if (joined < 0 && down && !mBefore[p][b] && wanted && PlayerForGamepad(static_cast<int32>(p)) < 0) {
					for (int32 i = 0; i < mCount; ++i) {
						const int32 g = mMaps[i]->GetGamepad();
						if (g == NK_INPUT_NO_GAMEPAD || g == NK_INPUT_ANY_GAMEPAD) {
							mMaps[i]->SetGamepad(static_cast<int32>(p));
							joined = i;
							break;
						}
					}
				}
				mBefore[p][b] = down;
			}
			// Une manette debranchee libere son joueur : il pourra en reprendre une.
			if (!connected) {
				const int32 owner = PlayerForGamepad(static_cast<int32>(p));
				if (owner >= 0) {
					mMaps[owner]->SetGamepad(NK_INPUT_NO_GAMEPAD);
				}
			}
		}
		return joined;
	}

} // namespace nkentseu
