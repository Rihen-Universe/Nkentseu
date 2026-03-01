#pragma once

// =============================================================================
// NkWaylandEventImpl.h — Système d'événements Wayland
//
// Gère le clavier, la souris et le touch via wl_seat / wl_keyboard /
// wl_pointer / wl_touch. Les touches sont converties de XKB keysym → NkKey
// via la bibliothèque libxkbcommon.
// =============================================================================

#include "../../Events/IEventImpl.h"
#include "../../Events/NkKeycodeMap.h"
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include <xkbcommon/xkbcommon.h>
#include <unordered_map>
#include <deque>
#include <memory>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

class NkWaylandWindowImpl;
struct NkWaylandData;

// ---------------------------------------------------------------------------
// NkWaylandEventImpl
// ---------------------------------------------------------------------------
class NkWaylandEventImpl : public IEventImpl {
public:
	NkWaylandEventImpl() = default;
	~NkWaylandEventImpl() override;

	// --- Cycle de vie -------------------------------------------------------
	void Initialize(IWindowImpl *owner, void *nativeHandle) override;
	void Shutdown(void *nativeHandle) override;

	// --- File d'événements --------------------------------------------------
	void PollEvents() override;
	NkEvent *Front() const override;
	void Pop() override;
	bool IsEmpty() const override;
	void PushEvent(std::unique_ptr<NkEvent> event) override;
	std::size_t Size() const override;

	// --- Callbacks ----------------------------------------------------------
	void SetEventCallback(NkEventCallback cb) override;
	void SetWindowCallback(void *handle, NkEventCallback cb) override;
	void DispatchEvent(NkEvent *event, void *handle) override;

	// Appelé par NkWaylandWindowImpl pour pousser un événement de fermeture
	void PushCloseEvent(NkWaylandWindowImpl *win);

	const char* GetPlatformName() const noexcept override { return "Wayland"; }
	bool IsReady() const noexcept override { return true; }

private:
	// -----------------------------------------------------------------------
	// État interne
	// -----------------------------------------------------------------------
	struct WindowEntry {
		NkWaylandWindowImpl *window = nullptr;
		NkEventCallback callback;
	};

	NkWaylandData *mWlData = nullptr;
	wl_keyboard *mKeyboard = nullptr;
	wl_pointer *mPointer = nullptr;
	wl_touch *mTouch = nullptr;

	// XKB pour le clavier
	xkb_context *mXkbContext = nullptr;
	xkb_keymap *mXkbKeymap = nullptr;
	xkb_state *mXkbState = nullptr;

	// Tracking souris
	NkF32 mPointerX = 0.f;
	NkF32 mPointerY = 0.f;

	// File d'événements
	std::deque<std::unique_ptr<NkEvent>> mQueue;
	NkEventCallback mGlobalCallback;

	std::unordered_map<void *, WindowEntry> mWindowMap;

	// -----------------------------------------------------------------------
	// Conversion touches
	// -----------------------------------------------------------------------
	static NkKey XkbKeyToNkKey(xkb_keysym_t sym);
	static NkModifierState BuildMods(xkb_state *state);

	// -----------------------------------------------------------------------
	// Listeners clavier
	// -----------------------------------------------------------------------
	static void OnKeyboardKeymap(void *data, wl_keyboard *, uint32_t fmt, int fd, uint32_t size);
	static void OnKeyboardEnter(void *data, wl_keyboard *, uint32_t serial, wl_surface *, wl_array *);
	static void OnKeyboardLeave(void *data, wl_keyboard *, uint32_t serial, wl_surface *);
	static void OnKeyboardKey(void *data, wl_keyboard *, uint32_t serial, uint32_t time, uint32_t key, uint32_t state);
	static void OnKeyboardMods(void *data, wl_keyboard *, uint32_t serial, uint32_t mods_dep, uint32_t mods_lat,
							   uint32_t mods_lock, uint32_t group);
	static void OnKeyboardRepeatInfo(void *, wl_keyboard *, int32_t, int32_t) {
	}

	static constexpr wl_keyboard_listener kKeyboardListener = {
		.keymap = OnKeyboardKeymap,
		.enter = OnKeyboardEnter,
		.leave = OnKeyboardLeave,
		.key = OnKeyboardKey,
		.modifiers = OnKeyboardMods,
		.repeat_info = OnKeyboardRepeatInfo,
	};

	// -----------------------------------------------------------------------
	// Listeners souris
	// -----------------------------------------------------------------------
	static void OnPointerEnter(void *data, wl_pointer *, uint32_t serial, wl_surface *, wl_fixed_t sx, wl_fixed_t sy);
	static void OnPointerLeave(void *data, wl_pointer *, uint32_t serial, wl_surface *);
	static void OnPointerMotion(void *data, wl_pointer *, uint32_t time, wl_fixed_t sx, wl_fixed_t sy);
	static void OnPointerButton(void *data, wl_pointer *, uint32_t serial, uint32_t time, uint32_t button,
								uint32_t state);
	static void OnPointerAxis(void *data, wl_pointer *, uint32_t time, uint32_t axis, wl_fixed_t value);
	static void OnPointerFrame(void *, wl_pointer *) {
	}
	static void OnPointerAxisSource(void *, wl_pointer *, uint32_t) {
	}
	static void OnPointerAxisStop(void *, wl_pointer *, uint32_t, uint32_t) {
	}
	static void OnPointerAxisDiscrete(void *, wl_pointer *, uint32_t, int32_t) {
	}

	static constexpr wl_pointer_listener kPointerListener = {
		.enter = OnPointerEnter,
		.leave = OnPointerLeave,
		.motion = OnPointerMotion,
		.button = OnPointerButton,
		.axis = OnPointerAxis,
		.frame = OnPointerFrame,
		.axis_source = OnPointerAxisSource,
		.axis_stop = OnPointerAxisStop,
		.axis_discrete = OnPointerAxisDiscrete,
	};

	// -----------------------------------------------------------------------
	// Listener wl_seat (obtenir clavier + pointeur)
	// -----------------------------------------------------------------------
	static void OnSeatCapabilities(void *data, wl_seat *seat, uint32_t caps);
	static void OnSeatName(void *, wl_seat *, const char *) {
	}

	static constexpr wl_seat_listener kSeatListener = {
		.capabilities = OnSeatCapabilities,
		.name = OnSeatName,
	};

	void AttachSeat(wl_seat *seat);
};

} // namespace nkentseu
