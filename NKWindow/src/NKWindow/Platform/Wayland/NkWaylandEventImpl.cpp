// =============================================================================
// NkWaylandEventImpl.cpp — Événements Wayland (clavier, souris, touch)
// =============================================================================

#include "NkWaylandEventImpl.h"
#include "NkWaylandWindowImpl.h"
#include "../../Events/NkWindowEvent.h"
#include "../../Events/NkKeyboardEvent.h"
#include "../../Events/NkMouseEvent.h"

#include <xkbcommon/xkbcommon.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/input-event-codes.h>
#include <cstring>
#include <deque>
#include <memory>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

// ---------------------------------------------------------------------------
// Destructeur
// ---------------------------------------------------------------------------

NkWaylandEventImpl::~NkWaylandEventImpl() {
	if (mXkbState) {
		xkb_state_unref(mXkbState);
		mXkbState = nullptr;
	}
	if (mXkbKeymap) {
		xkb_keymap_unref(mXkbKeymap);
		mXkbKeymap = nullptr;
	}
	if (mXkbContext) {
		xkb_context_unref(mXkbContext);
		mXkbContext = nullptr;
	}
	if (mPointer) {
		wl_pointer_destroy(mPointer);
		mPointer = nullptr;
	}
	if (mKeyboard) {
		wl_keyboard_destroy(mKeyboard);
		mKeyboard = nullptr;
	}
	if (mTouch) {
		wl_touch_destroy(mTouch);
		mTouch = nullptr;
	}
}

// ---------------------------------------------------------------------------
// Initialize / Shutdown
// ---------------------------------------------------------------------------

void NkWaylandEventImpl::Initialize(IWindowImpl *owner, void *nativeHandle) {
	auto *win  = static_cast<NkWaylandWindowImpl *>(owner);
	mWlData    = static_cast<NkWaylandData *>(nativeHandle);

	// Clé universelle void* = adresse de la surface
	void *key = static_cast<void *>(mWlData->surface);

	WindowEntry entry;
	entry.window = win;
	mWindowMap[key] = entry;

	if (!mXkbContext)
		mXkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

	// Attacher le siège (seat) si présent
	if (mWlData->seat)
		AttachSeat(mWlData->seat);
}

void NkWaylandEventImpl::Shutdown(void *nativeHandle) {
	auto *data = static_cast<NkWaylandData *>(nativeHandle);
	if (data)
		mWindowMap.erase(static_cast<void *>(data->surface));
}

void NkWaylandEventImpl::AttachSeat(wl_seat *seat) {
	wl_seat_add_listener(seat, &kSeatListener, this);
	wl_display_roundtrip(mWlData->display); // déclenche OnSeatCapabilities
}

// ---------------------------------------------------------------------------
// File d'événements
// ---------------------------------------------------------------------------

void NkWaylandEventImpl::PollEvents() {
	if (mWlData && mWlData->display)
		wl_display_dispatch_pending(mWlData->display);

	// Vérifier si une fenêtre veut se fermer
	for (auto &[key, entry] : mWindowMap) {
		if (entry.window && entry.window->WantsClose()) {
			entry.window->ClearClose();
			PushEvent(std::make_unique<NkWindowCloseEvent>());
		}
	}
}

NkEvent *NkWaylandEventImpl::Front() const {
	return mQueue.empty() ? nullptr : mQueue.front().get();
}
void NkWaylandEventImpl::Pop() {
	if (!mQueue.empty())
		mQueue.pop_front();
}
bool NkWaylandEventImpl::IsEmpty() const {
	return mQueue.empty();
}
std::size_t NkWaylandEventImpl::Size() const {
	return mQueue.size();
}
void NkWaylandEventImpl::PushEvent(std::unique_ptr<NkEvent> evt) {
	mQueue.push_back(std::move(evt));
}

void NkWaylandEventImpl::SetEventCallback(NkEventCallback cb) {
	mGlobalCallback = std::move(cb);
}

void NkWaylandEventImpl::SetWindowCallback(void *handle, NkEventCallback cb) {
	auto *data = static_cast<NkWaylandData *>(handle);
	if (!data) return;
	void *key = static_cast<void *>(data->surface);
	auto it = mWindowMap.find(key);
	if (it != mWindowMap.end())
		it->second.callback = std::move(cb);
}

void NkWaylandEventImpl::DispatchEvent(NkEvent *event, void *handle) {
	if (!event)
		return;

	if (mGlobalCallback)
		mGlobalCallback(event);

	auto *data = static_cast<NkWaylandData *>(handle);
	if (data) {
		void *key = static_cast<void *>(data->surface);
		auto it = mWindowMap.find(key);
		if (it != mWindowMap.end() && it->second.callback)
			it->second.callback(event);
	}
}

void NkWaylandEventImpl::PushCloseEvent(NkWaylandWindowImpl * /*win*/) {
	PushEvent(std::make_unique<NkWindowCloseEvent>());
}

// ---------------------------------------------------------------------------
// Listener wl_seat — capacités (clavier + pointeur)
// ---------------------------------------------------------------------------

void NkWaylandEventImpl::OnSeatCapabilities(void *data, wl_seat *seat, uint32_t caps) {
	auto *self = static_cast<NkWaylandEventImpl *>(data);

	const bool hasKb  = (caps & WL_SEAT_CAPABILITY_KEYBOARD) != 0;
	const bool hasPtr = (caps & WL_SEAT_CAPABILITY_POINTER)  != 0;

	if (hasKb && !self->mKeyboard) {
		self->mKeyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(self->mKeyboard, &kKeyboardListener, self);
	} else if (!hasKb && self->mKeyboard) {
		wl_keyboard_destroy(self->mKeyboard);
		self->mKeyboard = nullptr;
	}

	if (hasPtr && !self->mPointer) {
		self->mPointer = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(self->mPointer, &kPointerListener, self);
	} else if (!hasPtr && self->mPointer) {
		wl_pointer_destroy(self->mPointer);
		self->mPointer = nullptr;
	}
}

// ---------------------------------------------------------------------------
// Listener clavier
// ---------------------------------------------------------------------------

void NkWaylandEventImpl::OnKeyboardKeymap(void *data, wl_keyboard *, uint32_t fmt, int fd, uint32_t size) {
	auto *self = static_cast<NkWaylandEventImpl *>(data);
	if (fmt != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
		close(fd);
		return;
	}

	char *raw = static_cast<char *>(mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0));
	close(fd);
	if (raw == MAP_FAILED)
		return;

	if (self->mXkbKeymap)
		xkb_keymap_unref(self->mXkbKeymap);
	if (self->mXkbState)
		xkb_state_unref(self->mXkbState);

	self->mXkbKeymap =
		xkb_keymap_new_from_string(self->mXkbContext, raw, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	munmap(raw, size);

	if (self->mXkbKeymap)
		self->mXkbState = xkb_state_new(self->mXkbKeymap);
}

void NkWaylandEventImpl::OnKeyboardEnter(void *, wl_keyboard *, uint32_t, wl_surface *, wl_array *) {}
void NkWaylandEventImpl::OnKeyboardLeave(void *, wl_keyboard *, uint32_t, wl_surface *) {}

void NkWaylandEventImpl::OnKeyboardKey(void *data, wl_keyboard *, uint32_t /*serial*/, uint32_t /*time*/,
                                        uint32_t key, uint32_t state) {
	auto *self = static_cast<NkWaylandEventImpl *>(data);
	if (!self->mXkbState)
		return;

	xkb_keycode_t keycode = key + 8; // Linux evdev → XKB
	xkb_keysym_t  sym     = xkb_state_key_get_one_sym(self->mXkbState, keycode);

	NkKey           nkKey = XkbKeyToNkKey(sym);
	NkScancode      sc    = static_cast<NkScancode>(key); // evdev keycode as scancode approximation
	NkModifierState mods  = BuildMods(self->mXkbState);

	if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
		self->PushEvent(std::make_unique<NkKeyPressEvent>(nkKey, sc, mods, static_cast<NkU32>(sym)));
	else
		self->PushEvent(std::make_unique<NkKeyReleaseEvent>(nkKey, sc, mods, static_cast<NkU32>(sym)));
}

void NkWaylandEventImpl::OnKeyboardMods(void *data, wl_keyboard *, uint32_t /*serial*/,
                                         uint32_t mods_dep, uint32_t mods_lat,
                                         uint32_t mods_lock, uint32_t group) {
	auto *self = static_cast<NkWaylandEventImpl *>(data);
	if (self->mXkbState)
		xkb_state_update_mask(self->mXkbState, mods_dep, mods_lat, mods_lock, 0, 0, group);
}

// ---------------------------------------------------------------------------
// Listener souris
// ---------------------------------------------------------------------------

void NkWaylandEventImpl::OnPointerEnter(void *, wl_pointer *, uint32_t, wl_surface *, wl_fixed_t sx, wl_fixed_t sy) {
	(void)sx; (void)sy;
}

void NkWaylandEventImpl::OnPointerLeave(void *, wl_pointer *, uint32_t, wl_surface *) {}

void NkWaylandEventImpl::OnPointerMotion(void *data, wl_pointer *, uint32_t /*time*/, wl_fixed_t sx, wl_fixed_t sy) {
	auto *self = static_cast<NkWaylandEventImpl *>(data);
	NkI32 nx   = static_cast<NkI32>(wl_fixed_to_double(sx));
	NkI32 ny   = static_cast<NkI32>(wl_fixed_to_double(sy));
	NkI32 dx   = nx - static_cast<NkI32>(self->mPointerX);
	NkI32 dy   = ny - static_cast<NkI32>(self->mPointerY);
	self->mPointerX = static_cast<NkF32>(nx);
	self->mPointerY = static_cast<NkF32>(ny);
	self->PushEvent(std::make_unique<NkMouseMoveEvent>(nx, ny, nx, ny, dx, dy));
}

void NkWaylandEventImpl::OnPointerButton(void *data, wl_pointer *, uint32_t /*serial*/, uint32_t /*time*/,
                                          uint32_t button, uint32_t state) {
	auto *self  = static_cast<NkWaylandEventImpl *>(data);
	NkI32 x     = static_cast<NkI32>(self->mPointerX);
	NkI32 y     = static_cast<NkI32>(self->mPointerY);
	bool  pressed = (state == WL_POINTER_BUTTON_STATE_PRESSED);

	NkMouseButton btn = NkMouseButton::NK_MB_LEFT;
	if (button == BTN_RIGHT)  btn = NkMouseButton::NK_MB_RIGHT;
	if (button == BTN_MIDDLE) btn = NkMouseButton::NK_MB_MIDDLE;

	if (pressed)
		self->PushEvent(std::make_unique<NkMouseButtonPressEvent>(btn, x, y));
	else
		self->PushEvent(std::make_unique<NkMouseButtonReleaseEvent>(btn, x, y));
}

void NkWaylandEventImpl::OnPointerAxis(void *data, wl_pointer *, uint32_t /*time*/, uint32_t axis, wl_fixed_t value) {
	auto *self = static_cast<NkWaylandEventImpl *>(data);
	NkI32  x   = static_cast<NkI32>(self->mPointerX);
	NkI32  y   = static_cast<NkI32>(self->mPointerY);
	double v   = wl_fixed_to_double(value);

	if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
		self->PushEvent(std::make_unique<NkMouseWheelVerticalEvent>(  static_cast<NkF64>(-v / 10.0), x, y));
	else
		self->PushEvent(std::make_unique<NkMouseWheelHorizontalEvent>(static_cast<NkF64>(-v / 10.0), x, y));
}

// ---------------------------------------------------------------------------
// Conversion XKB keysym → NkKey
// ---------------------------------------------------------------------------

NkKey NkWaylandEventImpl::XkbKeyToNkKey(xkb_keysym_t sym) {
	switch (sym) {
		case XKB_KEY_Escape:    return NkKey::NK_ESCAPE;
		case XKB_KEY_Return:    return NkKey::NK_ENTER;
		case XKB_KEY_Tab:       return NkKey::NK_TAB;
		case XKB_KEY_BackSpace: return NkKey::NK_BACK;
		case XKB_KEY_Delete:    return NkKey::NK_DELETE;
		case XKB_KEY_Insert:    return NkKey::NK_INSERT;
		case XKB_KEY_Home:      return NkKey::NK_HOME;
		case XKB_KEY_End:       return NkKey::NK_END;
		case XKB_KEY_Page_Up:   return NkKey::NK_PAGE_UP;
		case XKB_KEY_Page_Down: return NkKey::NK_PAGE_DOWN;
		case XKB_KEY_Left:      return NkKey::NK_LEFT;
		case XKB_KEY_Right:     return NkKey::NK_RIGHT;
		case XKB_KEY_Up:        return NkKey::NK_UP;
		case XKB_KEY_Down:      return NkKey::NK_DOWN;
		case XKB_KEY_F1:        return NkKey::NK_F1;
		case XKB_KEY_F2:        return NkKey::NK_F2;
		case XKB_KEY_F3:        return NkKey::NK_F3;
		case XKB_KEY_F4:        return NkKey::NK_F4;
		case XKB_KEY_F5:        return NkKey::NK_F5;
		case XKB_KEY_F6:        return NkKey::NK_F6;
		case XKB_KEY_F7:        return NkKey::NK_F7;
		case XKB_KEY_F8:        return NkKey::NK_F8;
		case XKB_KEY_F9:        return NkKey::NK_F9;
		case XKB_KEY_F10:       return NkKey::NK_F10;
		case XKB_KEY_F11:       return NkKey::NK_F11;
		case XKB_KEY_F12:       return NkKey::NK_F12;
		case XKB_KEY_space:     return NkKey::NK_SPACE;
		case XKB_KEY_a: case XKB_KEY_A: return NkKey::NK_A;
		case XKB_KEY_b: case XKB_KEY_B: return NkKey::NK_B;
		case XKB_KEY_c: case XKB_KEY_C: return NkKey::NK_C;
		case XKB_KEY_d: case XKB_KEY_D: return NkKey::NK_D;
		case XKB_KEY_e: case XKB_KEY_E: return NkKey::NK_E;
		case XKB_KEY_f: case XKB_KEY_F: return NkKey::NK_F;
		case XKB_KEY_g: case XKB_KEY_G: return NkKey::NK_G;
		case XKB_KEY_h: case XKB_KEY_H: return NkKey::NK_H;
		case XKB_KEY_i: case XKB_KEY_I: return NkKey::NK_I;
		case XKB_KEY_j: case XKB_KEY_J: return NkKey::NK_J;
		case XKB_KEY_k: case XKB_KEY_K: return NkKey::NK_K;
		case XKB_KEY_l: case XKB_KEY_L: return NkKey::NK_L;
		case XKB_KEY_m: case XKB_KEY_M: return NkKey::NK_M;
		case XKB_KEY_n: case XKB_KEY_N: return NkKey::NK_N;
		case XKB_KEY_o: case XKB_KEY_O: return NkKey::NK_O;
		case XKB_KEY_p: case XKB_KEY_P: return NkKey::NK_P;
		case XKB_KEY_q: case XKB_KEY_Q: return NkKey::NK_Q;
		case XKB_KEY_r: case XKB_KEY_R: return NkKey::NK_R;
		case XKB_KEY_s: case XKB_KEY_S: return NkKey::NK_S;
		case XKB_KEY_t: case XKB_KEY_T: return NkKey::NK_T;
		case XKB_KEY_u: case XKB_KEY_U: return NkKey::NK_U;
		case XKB_KEY_v: case XKB_KEY_V: return NkKey::NK_V;
		case XKB_KEY_w: case XKB_KEY_W: return NkKey::NK_W;
		case XKB_KEY_x: case XKB_KEY_X: return NkKey::NK_X;
		case XKB_KEY_y: case XKB_KEY_Y: return NkKey::NK_Y;
		case XKB_KEY_z: case XKB_KEY_Z: return NkKey::NK_Z;
		case XKB_KEY_0: return NkKey::NK_NUM0;
		case XKB_KEY_1: return NkKey::NK_NUM1;
		case XKB_KEY_2: return NkKey::NK_NUM2;
		case XKB_KEY_3: return NkKey::NK_NUM3;
		case XKB_KEY_4: return NkKey::NK_NUM4;
		case XKB_KEY_5: return NkKey::NK_NUM5;
		case XKB_KEY_6: return NkKey::NK_NUM6;
		case XKB_KEY_7: return NkKey::NK_NUM7;
		case XKB_KEY_8: return NkKey::NK_NUM8;
		case XKB_KEY_9: return NkKey::NK_NUM9;
		case XKB_KEY_Shift_L:   return NkKey::NK_LSHIFT;
		case XKB_KEY_Shift_R:   return NkKey::NK_RSHIFT;
		case XKB_KEY_Control_L: return NkKey::NK_LCTRL;
		case XKB_KEY_Control_R: return NkKey::NK_RCTRL;
		case XKB_KEY_Alt_L:     return NkKey::NK_LALT;
		case XKB_KEY_Alt_R:     return NkKey::NK_RALT;
		case XKB_KEY_Super_L:   return NkKey::NK_LSUPER;
		case XKB_KEY_Super_R:   return NkKey::NK_RSUPER;
		default:                return NkKey::NK_UNKNOWN;
	}
}

NkModifierState NkWaylandEventImpl::BuildMods(xkb_state *state) {
	NkModifierState m{};
	m.shift   = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0;
	m.ctrl    = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_CTRL,  XKB_STATE_MODS_EFFECTIVE) > 0;
	m.alt     = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_ALT,   XKB_STATE_MODS_EFFECTIVE) > 0;
	m.super   = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_LOGO,  XKB_STATE_MODS_EFFECTIVE) > 0;
	return m;
}

} // namespace nkentseu
