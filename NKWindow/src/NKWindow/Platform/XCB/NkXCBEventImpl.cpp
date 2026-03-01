// =============================================================================
// NkXCBEventImpl.cpp  —  Système d'événements XCB
// =============================================================================

#include "NkXCBEventImpl.h"
#include "NkXCBWindowImpl.h"
#include "../../Events/NkKeycodeMap.h"
#include "../../Events/NkKeyboardEvent.h"
#include "../../Events/NkMouseEvent.h"
#include "../../Events/NkWindowEvent.h"
#include <X11/keysym.h>
#include <cstdlib>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

NkXCBEventImpl::NkXCBEventImpl() = default;
NkXCBEventImpl::~NkXCBEventImpl() {
	if (mKeySymbols)
		xcb_key_symbols_free(mKeySymbols);
}

// ---------------------------------------------------------------------------
// Initialize / Shutdown
// ---------------------------------------------------------------------------

void NkXCBEventImpl::Initialize(IWindowImpl *owner, void *nativeHandle) {
	auto *w = static_cast<NkXCBWindowImpl *>(owner);
	mWindowMap[nativeHandle] = {w, {}};

	// Initialise la connection XCB si ce n'est pas déjà fait
	if (!mConnection && w) {
		mConnection = nk_xcb_global_connection;
		if (mConnection)
			mKeySymbols = xcb_key_symbols_alloc(mConnection);
	}
}

void NkXCBEventImpl::Shutdown(void *nativeHandle) {
	mWindowMap.erase(nativeHandle);

	if (mWindowMap.empty()) {
		if (mKeySymbols) {
			xcb_key_symbols_free(mKeySymbols);
			mKeySymbols = nullptr;
		}
		mConnection = nullptr;
	}
}

// ---------------------------------------------------------------------------
// Queue
// ---------------------------------------------------------------------------

NkEvent *NkXCBEventImpl::Front() const {
	return mQueue.empty() ? nullptr : mQueue.front().get();
}
void NkXCBEventImpl::Pop() {
	if (!mQueue.empty())
		mQueue.pop();
}
bool NkXCBEventImpl::IsEmpty() const {
	return mQueue.empty();
}
std::size_t NkXCBEventImpl::Size() const {
	return mQueue.size();
}
void NkXCBEventImpl::PushEvent(std::unique_ptr<NkEvent> e) {
	mQueue.push(std::move(e));
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

void NkXCBEventImpl::SetGlobalCallback(NkEventCallback cb) {
	mGlobalCallback = std::move(cb);
}

void NkXCBEventImpl::SetWindowCallback(void *nativeHandle, NkEventCallback cb) {
	auto it = mWindowMap.find(nativeHandle);
	if (it != mWindowMap.end())
		it->second.callback = std::move(cb);
}

void NkXCBEventImpl::DispatchEvent(NkEvent *ev, void *nativeHandle) {
	if (!ev) return;
	if (nativeHandle) {
		auto it = mWindowMap.find(nativeHandle);
		if (it != mWindowMap.end() && it->second.callback)
			it->second.callback(ev);
	}
	if (mGlobalCallback)
		mGlobalCallback(ev);
}

// ---------------------------------------------------------------------------
// PollEvents — typed events, void* key
// ---------------------------------------------------------------------------

void NkXCBEventImpl::PollEvents() {
	if (!mConnection)
		return;

	// Lambda d'enfilement : Clone() préserve le type polymorphe
	auto Enqueue = [&](const NkEvent &evt) {
		mQueue.push(std::unique_ptr<NkEvent>(evt.Clone()));
	};

	xcb_generic_event_t *xev = nullptr;
	while ((xev = xcb_poll_for_event(mConnection)) != nullptr) {
		uint8_t type = xev->response_type & ~0x80;

		switch (type) {
			// -----------------------------------------------------------------
			case XCB_KEY_PRESS:
			case XCB_KEY_RELEASE: {
				auto *ke = reinterpret_cast<xcb_key_press_event_t *>(xev);

				// Résolution par position physique (USB HID)
				NkKey k = NkKeycodeMap::NkKeyFromX11Keycode(ke->detail);

				// Fallback par keysym si la position n'est pas reconnue
				if (k == NkKey::NK_UNKNOWN && mKeySymbols) {
					xcb_keysym_t ks = xcb_key_symbols_get_keysym(mKeySymbols, ke->detail, 0);
					k = XcbKeysymToNkKey(ks);
				}

				if (k != NkKey::NK_UNKNOWN) {
					NkScancode sc        = NkScancodeFromXKeycode(ke->detail);
					NkModifierState mods = XcbStateMods(ke->state);
					if (type == XCB_KEY_PRESS)
						Enqueue(NkKeyPressEvent(k, sc, mods, ke->detail));
					else
						Enqueue(NkKeyReleaseEvent(k, sc, mods, ke->detail));
				}
				break;
			}

			// -----------------------------------------------------------------
			case XCB_BUTTON_PRESS:
			case XCB_BUTTON_RELEASE: {
				auto *be = reinterpret_cast<xcb_button_press_event_t *>(xev);
				NkI32 x      = be->event_x;
				NkI32 y      = be->event_y;
				bool  pressed = (type == XCB_BUTTON_PRESS);

				switch (be->detail) {
					case 1:
						if (pressed) Enqueue(NkMouseButtonPressEvent(NkMouseButton::NK_MB_LEFT,   x, y));
						else         Enqueue(NkMouseButtonReleaseEvent(NkMouseButton::NK_MB_LEFT,  x, y));
						break;
					case 2:
						if (pressed) Enqueue(NkMouseButtonPressEvent(NkMouseButton::NK_MB_MIDDLE,   x, y));
						else         Enqueue(NkMouseButtonReleaseEvent(NkMouseButton::NK_MB_MIDDLE,  x, y));
						break;
					case 3:
						if (pressed) Enqueue(NkMouseButtonPressEvent(NkMouseButton::NK_MB_RIGHT,   x, y));
						else         Enqueue(NkMouseButtonReleaseEvent(NkMouseButton::NK_MB_RIGHT,  x, y));
						break;
					case 4:
						if (pressed) Enqueue(NkMouseWheelVerticalEvent( 1.0, x, y));
						break;
					case 5:
						if (pressed) Enqueue(NkMouseWheelVerticalEvent(-1.0, x, y));
						break;
					default:
						break;
				}
				break;
			}

			// -----------------------------------------------------------------
			case XCB_MOTION_NOTIFY: {
				auto *me = reinterpret_cast<xcb_motion_notify_event_t *>(xev);
				NkI32 dx = me->event_x - mPrevMouseX;
				NkI32 dy = me->event_y - mPrevMouseY;
				Enqueue(NkMouseMoveEvent(me->event_x, me->event_y,
				                        me->root_x, me->root_y,
				                        dx, dy));
				mPrevMouseX = me->event_x;
				mPrevMouseY = me->event_y;
				break;
			}

			// -----------------------------------------------------------------
			case XCB_CONFIGURE_NOTIFY: {
				auto *ce = reinterpret_cast<xcb_configure_notify_event_t *>(xev);
				Enqueue(NkWindowResizeEvent(ce->width, ce->height));
				break;
			}

			// -----------------------------------------------------------------
			case XCB_FOCUS_IN: {
				Enqueue(NkWindowFocusGainedEvent());
				break;
			}

			case XCB_FOCUS_OUT: {
				Enqueue(NkWindowFocusLostEvent());
				break;
			}

			// -----------------------------------------------------------------
			case XCB_CLIENT_MESSAGE: {
				auto *cm  = reinterpret_cast<xcb_client_message_event_t *>(xev);
				void *key = NkXCBWindowImpl::EncodeWindow(cm->window);
				auto  it  = mWindowMap.find(key);
				if (it != mWindowMap.end() && it->second.window) {
					if (cm->type == it->second.window->GetWmProtocolsAtom() &&
					    cm->data.data32[0] == it->second.window->GetWmDeleteAtom()) {
						Enqueue(NkWindowCloseEvent());
					}
				}
				break;
			}

			default:
				break;
		}

		free(xev);
	}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

NkKey NkXCBEventImpl::XcbKeysymToNkKey(xcb_keysym_t ks) {
	return NkKeycodeMap::NkKeyFromX11KeySym(static_cast<NkU32>(ks));
}

NkModifierState NkXCBEventImpl::XcbStateMods(uint16_t state) {
	NkModifierState m;
	m.ctrl    = !!(state & XCB_MOD_MASK_CONTROL);
	m.alt     = !!(state & XCB_MOD_MASK_1);
	m.shift   = !!(state & XCB_MOD_MASK_SHIFT);
	m.super   = !!(state & XCB_MOD_MASK_4);
	m.capLock = !!(state & XCB_MOD_MASK_LOCK);
	m.numLock = !!(state & XCB_MOD_MASK_2);
	return m;
}

const char *NkXCBEventImpl::GetPlatformName() const noexcept { return "XCB"; }
bool        NkXCBEventImpl::IsReady()         const noexcept { return true;  }

} // namespace nkentseu
