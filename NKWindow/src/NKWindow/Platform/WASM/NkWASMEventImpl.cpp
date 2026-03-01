// =============================================================================
// NkWASMEventImpl.cpp
// =============================================================================

#include "NkWASMEventImpl.h"
#include "../../Events/NkKeyboardEvent.h"
#include "../../Events/NkMouseEvent.h"
#include "../../Events/NkWindowEvent.h"
#include "../../Events/NkTouchEvent.h"
#include <cmath>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

NkWASMEventImpl *NkWASMEventImpl::sInstance = nullptr;
static NkWebInputOptions gWebInputOptions{};

NkWASMEventImpl::NkWASMEventImpl() {
	sInstance = this;
	emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, this, 1, OnKeyDown);
	emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, this, 1, OnKeyUp);
	emscripten_set_mousemove_callback("#canvas", this, 1, OnMouseMove);
	emscripten_set_mousedown_callback("#canvas", this, 1, OnMouseDown);
	emscripten_set_mouseup_callback("#canvas", this, 1, OnMouseUp);
	emscripten_set_wheel_callback("#canvas", this, 1, OnWheel);
	emscripten_set_touchstart_callback("#canvas", this, 1, OnTouchStart);
	emscripten_set_touchmove_callback("#canvas", this, 1, OnTouchMove);
	emscripten_set_touchend_callback("#canvas", this, 1, OnTouchEnd);
	emscripten_set_touchcancel_callback("#canvas", this, 1, OnTouchCancel);
}

NkWASMEventImpl::~NkWASMEventImpl() {
	emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, 1, nullptr);
	emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, 1, nullptr);
	emscripten_set_mousemove_callback("#canvas", nullptr, 1, nullptr);
	emscripten_set_mousedown_callback("#canvas", nullptr, 1, nullptr);
	emscripten_set_mouseup_callback("#canvas", nullptr, 1, nullptr);
	emscripten_set_wheel_callback("#canvas", nullptr, 1, nullptr);
	emscripten_set_touchstart_callback("#canvas", nullptr, 1, nullptr);
	emscripten_set_touchmove_callback("#canvas", nullptr, 1, nullptr);
	emscripten_set_touchend_callback("#canvas", nullptr, 1, nullptr);
	emscripten_set_touchcancel_callback("#canvas", nullptr, 1, nullptr);
	sInstance = nullptr;
}

void NkWASMEventImpl::Initialize(IWindowImpl *owner, void *nativeHandle) {
	void *handle = nativeHandle ? nativeHandle : owner;
	WindowEntry entry;
	entry.window = owner;
	mWindowMap[handle] = std::move(entry);
	if (!mPrimaryHandle)
		mPrimaryHandle = handle;
}

void NkWASMEventImpl::Shutdown(void *nativeHandle) {
	if (mWindowMap.empty()) {
		mPrimaryHandle = nullptr;
		return;
	}

	if (nativeHandle) {
		mWindowMap.erase(nativeHandle);
	} else if (mPrimaryHandle) {
		mWindowMap.erase(mPrimaryHandle);
	} else {
		mWindowMap.erase(mWindowMap.begin());
	}

	if (mWindowMap.empty())
		mPrimaryHandle = nullptr;
	else if (!mPrimaryHandle || mWindowMap.find(mPrimaryHandle) == mWindowMap.end())
		mPrimaryHandle = mWindowMap.begin()->first;
}

NkEvent *NkWASMEventImpl::Front() const {
	return mQueue.empty() ? nullptr : mQueue.front().get();
}

void NkWASMEventImpl::Pop() {
	if (!mQueue.empty())
		mQueue.pop();
}

bool NkWASMEventImpl::IsEmpty() const {
	return mQueue.empty();
}

std::size_t NkWASMEventImpl::Size() const {
	return mQueue.size();
}

void NkWASMEventImpl::PushEvent(std::unique_ptr<NkEvent> e) {
	mQueue.push(std::move(e));
}

void NkWASMEventImpl::PollEvents() {
	// Emscripten callbacks push events asynchronously; nothing to pull here.
}

void NkWASMEventImpl::SetEventCallback(NkEventCallback cb) {
	mGlobalCallback = std::move(cb);
}

void NkWASMEventImpl::SetWindowCallback(void *nativeHandle, NkEventCallback cb) {
	void *handle = nativeHandle;
	if (!handle)
		handle = mPrimaryHandle;
	if (!handle && !mWindowMap.empty())
		handle = mWindowMap.begin()->first;
	if (!handle)
		return;

	auto it = mWindowMap.find(handle);
	if (it == mWindowMap.end())
		return;

	it->second.callback = std::move(cb);
}

void NkWASMEventImpl::SetInputOptions(const NkWebInputOptions &options) {
	gWebInputOptions = options;
}

NkWebInputOptions NkWASMEventImpl::GetInputOptions() {
	return gWebInputOptions;
}

const NkEventState& NkWASMEventImpl::GetInputState() const {
	return mInputState;
}

NkEventState& NkWASMEventImpl::GetInputState() {
	return mInputState;
}

const char* NkWASMEventImpl::GetPlatformName() const noexcept {
	return "WASM";
}

bool NkWASMEventImpl::IsReady() const noexcept {
	return true;
}

void NkWASMEventImpl::DispatchEvent(NkEvent *event, void *nativeHandle) {
	if (!event)
		return;

	void *handle = nativeHandle;
	if (!handle)
		handle = mPrimaryHandle;

	auto it = mWindowMap.find(handle);
	if (it != mWindowMap.end() && it->second.callback)
		it->second.callback(event);

	if (mGlobalCallback)
		mGlobalCallback(event);
}

/**
 * @brief Struct NkCanvasCoordMap.
 */
struct NkCanvasCoordMap {
	NkI32 x = 0;
	NkI32 y = 0;
	double sx = 1.0;
	double sy = 1.0;
};

static NkCanvasCoordMap MapCssToCanvasCoords(NkI32 cssX, NkI32 cssY) {
	NkCanvasCoordMap out{};
	out.x = cssX;
	out.y = cssY;

	int canvasW = 0;
	int canvasH = 0;
	double cssW = 0.0;
	double cssH = 0.0;

	if (emscripten_get_canvas_element_size("#canvas", &canvasW, &canvasH) != EMSCRIPTEN_RESULT_SUCCESS)
		return out;
	if (emscripten_get_element_css_size("#canvas", &cssW, &cssH) != EMSCRIPTEN_RESULT_SUCCESS)
		return out;
	if (canvasW <= 0 || canvasH <= 0 || cssW <= 0.0 || cssH <= 0.0)
		return out;

	out.sx = static_cast<double>(canvasW) / cssW;
	out.sy = static_cast<double>(canvasH) / cssH;
	out.x = static_cast<NkI32>(std::lround(static_cast<double>(cssX) * out.sx));
	out.y = static_cast<NkI32>(std::lround(static_cast<double>(cssY) * out.sy));
	return out;
}

void NkWASMEventImpl::PushTouchEvent(const EmscriptenTouchEvent *te, NkTouchPhase phase) {
	if (!te)
		return;

	NkTouchPoint points[NK_MAX_TOUCH_POINTS];
	NkU32 count = 0;

	for (int i = 0; i < te->numTouches && count < NK_MAX_TOUCH_POINTS; ++i) {
		const EmscriptenTouchPoint &tp = te->touches[i];
		if (!tp.isChanged && phase != NkTouchPhase::NK_TOUCH_PHASE_MOVED)
			continue;

		NkTouchPoint p;
		p.id = static_cast<NkU64>(tp.identifier);
		p.phase = phase;
		const NkCanvasCoordMap mapped =
			MapCssToCanvasCoords(static_cast<NkI32>(tp.targetX), static_cast<NkI32>(tp.targetY));
		p.clientX = static_cast<float>(mapped.x);
		p.clientY = static_cast<float>(mapped.y);
		p.screenX = static_cast<float>(tp.screenX);
		p.screenY = static_cast<float>(tp.screenY);
		points[count++] = p;
	}

	if (count == 0)
		return;

	switch (phase) {
		case NkTouchPhase::NK_TOUCH_PHASE_BEGAN:
			PushEvent(std::make_unique<NkTouchBeginEvent>(points, count));
			break;
		case NkTouchPhase::NK_TOUCH_PHASE_MOVED:
			PushEvent(std::make_unique<NkTouchMoveEvent>(points, count));
			break;
		case NkTouchPhase::NK_TOUCH_PHASE_ENDED:
			PushEvent(std::make_unique<NkTouchEndEvent>(points, count));
			break;
		case NkTouchPhase::NK_TOUCH_PHASE_CANCELLED:
			PushEvent(std::make_unique<NkTouchCancelEvent>(points, count));
			break;
		default:
			break;
	}
}

static bool IsBrowserShortcut(const EmscriptenKeyboardEvent *ke) {
	if (!ke)
		return false;

	const unsigned long kc = ke->keyCode;

	// Let browser/devtools shortcuts pass through.
	if (kc == 123) // F12
		return true;
	if (ke->ctrlKey && ke->shiftKey && (kc == 73 || kc == 74)) // Ctrl+Shift+I/J
		return true;
	if (ke->ctrlKey && (kc == 82 || kc == 116)) // Ctrl+R / F5
		return true;
	if (ke->metaKey) // macOS browser shortcuts
		return true;

	return false;
}

EM_BOOL NkWASMEventImpl::OnKeyDown(int, const EmscriptenKeyboardEvent *ke, void *) {
	if (!sInstance || !ke)
		return EM_FALSE;
	if (!gWebInputOptions.captureKeyboard)
		return EM_FALSE;
	if (gWebInputOptions.allowBrowserShortcuts && IsBrowserShortcut(ke))
		return EM_FALSE;

	NkScancode sc = NkScancodeFromDOMCode(ke->code);
	NkKey key = NkScancodeToKey(sc);
	if (key == NkKey::NK_UNKNOWN)
		key = DomVkToNkKey(ke->keyCode);

	if (key == NkKey::NK_UNKNOWN)
		return EM_FALSE;

	NkModifierState mods(ke->ctrlKey, ke->altKey, ke->shiftKey, ke->metaKey);
	if (ke->repeat) {
		sInstance->PushEvent(std::make_unique<NkKeyRepeatEvent>(
			key, 1, sc, mods, static_cast<NkU32>(ke->keyCode)));
	} else {
		sInstance->PushEvent(std::make_unique<NkKeyPressEvent>(
			key, sc, mods, static_cast<NkU32>(ke->keyCode)));
	}
	return EM_TRUE;
}

EM_BOOL NkWASMEventImpl::OnKeyUp(int, const EmscriptenKeyboardEvent *ke, void *) {
	if (!sInstance || !ke)
		return EM_FALSE;
	if (!gWebInputOptions.captureKeyboard)
		return EM_FALSE;
	if (gWebInputOptions.allowBrowserShortcuts && IsBrowserShortcut(ke))
		return EM_FALSE;

	NkScancode sc = NkScancodeFromDOMCode(ke->code);
	NkKey key = NkScancodeToKey(sc);
	if (key == NkKey::NK_UNKNOWN)
		key = DomVkToNkKey(ke->keyCode);

	if (key == NkKey::NK_UNKNOWN)
		return EM_FALSE;

	NkModifierState mods(ke->ctrlKey, ke->altKey, ke->shiftKey, ke->metaKey);
	sInstance->PushEvent(std::make_unique<NkKeyReleaseEvent>(
		key, sc, mods, static_cast<NkU32>(ke->keyCode)));
	return EM_TRUE;
}

EM_BOOL NkWASMEventImpl::OnMouseMove(int, const EmscriptenMouseEvent *me, void *) {
	if (!sInstance || !me)
		return EM_TRUE;
	if (!gWebInputOptions.captureMouseMove)
		return EM_FALSE;

	const NkCanvasCoordMap mapped =
		MapCssToCanvasCoords(static_cast<NkI32>(me->targetX), static_cast<NkI32>(me->targetY));
	const NkI32 dx = static_cast<NkI32>(std::lround(static_cast<double>(me->movementX) * mapped.sx));
	const NkI32 dy = static_cast<NkI32>(std::lround(static_cast<double>(me->movementY) * mapped.sy));
	sInstance->PushEvent(std::make_unique<NkMouseMoveEvent>(
		mapped.x, mapped.y,
		static_cast<NkI32>(me->screenX), static_cast<NkI32>(me->screenY),
		dx, dy));
	return EM_TRUE;
}

EM_BOOL NkWASMEventImpl::OnMouseDown(int, const EmscriptenMouseEvent *me, void *) {
	if (!sInstance || !me)
		return EM_TRUE;
	if ((me->button == 0 && !gWebInputOptions.captureMouseLeft) ||
		(me->button == 1 && !gWebInputOptions.captureMouseMiddle) ||
		(me->button == 2 && !gWebInputOptions.captureMouseRight)) {
		return EM_FALSE;
	}
	NkMouseButton btn = me->button == 0	  ? NkMouseButton::NK_MB_LEFT
						: me->button == 1 ? NkMouseButton::NK_MB_MIDDLE
										  : NkMouseButton::NK_MB_RIGHT;
	const NkCanvasCoordMap mapped =
		MapCssToCanvasCoords(static_cast<NkI32>(me->targetX), static_cast<NkI32>(me->targetY));
	sInstance->PushEvent(std::make_unique<NkMouseButtonPressEvent>(
		btn, mapped.x, mapped.y,
		static_cast<NkI32>(me->screenX), static_cast<NkI32>(me->screenY)));
	return EM_TRUE;
}

EM_BOOL NkWASMEventImpl::OnMouseUp(int, const EmscriptenMouseEvent *me, void *) {
	if (!sInstance || !me)
		return EM_TRUE;
	if ((me->button == 0 && !gWebInputOptions.captureMouseLeft) ||
		(me->button == 1 && !gWebInputOptions.captureMouseMiddle) ||
		(me->button == 2 && !gWebInputOptions.captureMouseRight)) {
		return EM_FALSE;
	}
	NkMouseButton btn = me->button == 0	  ? NkMouseButton::NK_MB_LEFT
						: me->button == 1 ? NkMouseButton::NK_MB_MIDDLE
										  : NkMouseButton::NK_MB_RIGHT;
	const NkCanvasCoordMap mapped =
		MapCssToCanvasCoords(static_cast<NkI32>(me->targetX), static_cast<NkI32>(me->targetY));
	sInstance->PushEvent(std::make_unique<NkMouseButtonReleaseEvent>(
		btn, mapped.x, mapped.y,
		static_cast<NkI32>(me->screenX), static_cast<NkI32>(me->screenY)));
	return EM_TRUE;
}

EM_BOOL NkWASMEventImpl::OnWheel(int, const EmscriptenWheelEvent *we, void *) {
	if (!sInstance || !we)
		return EM_TRUE;
	if (!gWebInputOptions.captureMouseWheel)
		return EM_FALSE;

	const double deltaY = -we->deltaY / 100.0;
	const double deltaX =  we->deltaX / 100.0;
	if (deltaY != 0.0)
		sInstance->PushEvent(std::make_unique<NkMouseWheelVerticalEvent>(deltaY));
	if (deltaX != 0.0)
		sInstance->PushEvent(std::make_unique<NkMouseWheelHorizontalEvent>(deltaX));
	return EM_TRUE;
}

EM_BOOL NkWASMEventImpl::OnTouchStart(int, const EmscriptenTouchEvent *te, void *) {
	if (!sInstance)
		return EM_TRUE;
	if (!gWebInputOptions.captureTouch)
		return EM_FALSE;
	sInstance->PushTouchEvent(te, NkTouchPhase::NK_TOUCH_PHASE_BEGAN);
	return EM_TRUE;
}

EM_BOOL NkWASMEventImpl::OnTouchMove(int, const EmscriptenTouchEvent *te, void *) {
	if (!sInstance)
		return EM_TRUE;
	if (!gWebInputOptions.captureTouch)
		return EM_FALSE;
	sInstance->PushTouchEvent(te, NkTouchPhase::NK_TOUCH_PHASE_MOVED);
	return EM_TRUE;
}

EM_BOOL NkWASMEventImpl::OnTouchEnd(int, const EmscriptenTouchEvent *te, void *) {
	if (!sInstance)
		return EM_TRUE;
	if (!gWebInputOptions.captureTouch)
		return EM_FALSE;
	sInstance->PushTouchEvent(te, NkTouchPhase::NK_TOUCH_PHASE_ENDED);
	return EM_TRUE;
}

EM_BOOL NkWASMEventImpl::OnTouchCancel(int, const EmscriptenTouchEvent *te, void *) {
	if (!sInstance)
		return EM_TRUE;
	if (!gWebInputOptions.captureTouch)
		return EM_FALSE;
	sInstance->PushTouchEvent(te, NkTouchPhase::NK_TOUCH_PHASE_CANCELLED);
	return EM_TRUE;
}

NkKey NkWASMEventImpl::DomVkToNkKey(unsigned long kc) {
	switch (kc) {
		case 27:
			return NkKey::NK_ESCAPE;
		case 112:
			return NkKey::NK_F1;
		case 113:
			return NkKey::NK_F2;
		case 114:
			return NkKey::NK_F3;
		case 115:
			return NkKey::NK_F4;
		case 116:
			return NkKey::NK_F5;
		case 117:
			return NkKey::NK_F6;
		case 118:
			return NkKey::NK_F7;
		case 119:
			return NkKey::NK_F8;
		case 120:
			return NkKey::NK_F9;
		case 121:
			return NkKey::NK_F10;
		case 122:
			return NkKey::NK_F11;
		case 123:
			return NkKey::NK_F12;
		case 48:
			return NkKey::NK_NUM0;
		case 49:
			return NkKey::NK_NUM1;
		case 50:
			return NkKey::NK_NUM2;
		case 51:
			return NkKey::NK_NUM3;
		case 52:
			return NkKey::NK_NUM4;
		case 53:
			return NkKey::NK_NUM5;
		case 54:
			return NkKey::NK_NUM6;
		case 55:
			return NkKey::NK_NUM7;
		case 56:
			return NkKey::NK_NUM8;
		case 57:
			return NkKey::NK_NUM9;
		case 65:
			return NkKey::NK_A;
		case 66:
			return NkKey::NK_B;
		case 67:
			return NkKey::NK_C;
		case 68:
			return NkKey::NK_D;
		case 69:
			return NkKey::NK_E;
		case 70:
			return NkKey::NK_F;
		case 71:
			return NkKey::NK_G;
		case 72:
			return NkKey::NK_H;
		case 73:
			return NkKey::NK_I;
		case 74:
			return NkKey::NK_J;
		case 75:
			return NkKey::NK_K;
		case 76:
			return NkKey::NK_L;
		case 77:
			return NkKey::NK_M;
		case 78:
			return NkKey::NK_N;
		case 79:
			return NkKey::NK_O;
		case 80:
			return NkKey::NK_P;
		case 81:
			return NkKey::NK_Q;
		case 82:
			return NkKey::NK_R;
		case 83:
			return NkKey::NK_S;
		case 84:
			return NkKey::NK_T;
		case 85:
			return NkKey::NK_U;
		case 86:
			return NkKey::NK_V;
		case 87:
			return NkKey::NK_W;
		case 88:
			return NkKey::NK_X;
		case 89:
			return NkKey::NK_Y;
		case 90:
			return NkKey::NK_Z;
		case 32:
			return NkKey::NK_SPACE;
		case 13:
			return NkKey::NK_ENTER;
		case 8:
			return NkKey::NK_BACK;
		case 9:
			return NkKey::NK_TAB;
		case 16:
			return NkKey::NK_LSHIFT;
		case 17:
			return NkKey::NK_LCTRL;
		case 18:
			return NkKey::NK_LALT;
		case 37:
			return NkKey::NK_LEFT;
		case 39:
			return NkKey::NK_RIGHT;
		case 38:
			return NkKey::NK_UP;
		case 40:
			return NkKey::NK_DOWN;
		case 45:
			return NkKey::NK_INSERT;
		case 46:
			return NkKey::NK_DELETE;
		case 36:
			return NkKey::NK_HOME;
		case 35:
			return NkKey::NK_END;
		case 33:
			return NkKey::NK_PAGE_UP;
		case 34:
			return NkKey::NK_PAGE_DOWN;
		default:
			return NkKey::NK_UNKNOWN;
	}
}

} // namespace nkentseu
