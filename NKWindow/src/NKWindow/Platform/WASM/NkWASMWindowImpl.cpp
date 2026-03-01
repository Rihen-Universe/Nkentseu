// =============================================================================
// NkWASMWindowImpl.cpp
// =============================================================================

#include "NkWASMWindowImpl.h"
#include "NkWASMEventImpl.h"
#include "../../Events/IEventImpl.h"
#include "../../Core/NkSystem.h"
#include <algorithm>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
/**
 * @brief Internal anonymous namespace.
 */
namespace {
static NkVec2u QueryViewportSizeFallback() {
	int w = EM_ASM_INT({
		var ww = window.innerWidth || 0;
		if (ww <= 0 && document && document.documentElement)
			ww = document.documentElement.clientWidth || 0;
		return ww > 0 ? (ww | 0) : 1;
	});
	int h = EM_ASM_INT({
		var hh = window.innerHeight || 0;
		if (hh <= 0 && document && document.documentElement)
			hh = document.documentElement.clientHeight || 0;
		return hh > 0 ? (hh | 0) : 1;
	});

	return {static_cast<NkU32>(std::max(1, w)), static_cast<NkU32>(std::max(1, h))};
}

static NkVec2u QueryCanvasSizeSafe() {
	int w = 0;
	int h = 0;
	emscripten_get_canvas_element_size("#canvas", &w, &h);
	if (w > 0 && h > 0) {
		return {static_cast<NkU32>(w), static_cast<NkU32>(h)};
	}

	w = EM_ASM_INT({
		var c = document.querySelector("#canvas");
		if (!c &&typeof Module != = "undefined" && Module["canvas"])
			c = Module["canvas"];
		if (!c)
			return 0;
		var r = c.getBoundingClientRect();
		var ww = (r && r.width) ? r.width : 0;
		if (ww <= 0)
			ww = c.width || 0;
		if (ww <= 0)
			ww = window.innerWidth || 0;
		return ww | 0;
	});
	h = EM_ASM_INT({
		var c = document.querySelector("#canvas");
		if (!c &&typeof Module != = "undefined" && Module["canvas"])
			c = Module["canvas"];
		if (!c)
			return 0;
		var r = c.getBoundingClientRect();
		var hh = (r && r.height) ? r.height : 0;
		if (hh <= 0)
			hh = c.height || 0;
		if (hh <= 0)
			hh = window.innerHeight || 0;
		return hh | 0;
	});

	if (w <= 0 || h <= 0) {
		NkVec2u vp = QueryViewportSizeFallback();
		w = static_cast<int>(vp.x);
		h = static_cast<int>(vp.y);
	}

	if (w > 0 && h > 0)
		emscripten_set_canvas_element_size("#canvas", w, h);

	return {static_cast<NkU32>(std::max(1, w)), static_cast<NkU32>(std::max(1, h))};
}

static void ApplyContextMenuPolicy(bool preventContextMenu) {
	EM_ASM(
		{
			var c = document.querySelector('#canvas');
			if (!c &&typeof Module != = 'undefined' && Module['canvas']) {
				c = Module['canvas'];
			}
			if (!c)
				return;

			if (!c.__nk_contextHandler) {
				c.__nk_contextHandler = function(e) {
					e.preventDefault();
				};
			}

			c.removeEventListener('contextmenu', c.__nk_contextHandler);
			if ($0) {
				c.addEventListener('contextmenu', c.__nk_contextHandler);
			}
		},
		preventContextMenu ? 1 : 0);
}

static void InstallCanvasKeyboardFocus() {
	EM_ASM({
		var c = document.querySelector('#canvas');
		if (!c &&typeof Module != = 'undefined' && Module['canvas']) {
			c = Module['canvas'];
		}
		if (!c)
			return;

		if (!c.hasAttribute('tabindex')) {
			c.setAttribute('tabindex', '0');
		}

		if (!c.__nk_focusHandler) {
			c.__nk_focusHandler = function() {
				try {
					c.focus({preventScroll : true});
				} catch (e) {
					try {
						c.focus();
					} catch (_) {
					}
				}
			};
		}

		c.removeEventListener('pointerdown', c.__nk_focusHandler);
		c.removeEventListener('mousedown', c.__nk_focusHandler);
		c.removeEventListener('touchstart', c.__nk_focusHandler);

		c.addEventListener('pointerdown', c.__nk_focusHandler);
		c.addEventListener('mousedown', c.__nk_focusHandler);
		c.addEventListener('touchstart', c.__nk_focusHandler, {passive : true});

		setTimeout(c.__nk_focusHandler, 0);
	});
}

static void RemoveCanvasKeyboardFocus() {
	EM_ASM({
		var c = document.querySelector('#canvas');
		if (!c &&typeof Module != = 'undefined' && Module['canvas']) {
			c = Module['canvas'];
		}
		if (!c || !c.__nk_focusHandler)
			return;
		c.removeEventListener('pointerdown', c.__nk_focusHandler);
		c.removeEventListener('mousedown', c.__nk_focusHandler);
		c.removeEventListener('touchstart', c.__nk_focusHandler);
	});
}
} // namespace

bool NkWASMWindowImpl::Create(const NkWindowConfig &config) {
	mConfig = config;

	NkU32 requestedW = config.width ? config.width : 1280u;
	NkU32 requestedH = config.height ? config.height : 720u;
	emscripten_set_canvas_element_size("#canvas", static_cast<int>(requestedW), static_cast<int>(requestedH));
	NkVec2u actual = QueryCanvasSizeSafe();
	if (actual.x == 0 || actual.y == 0) {
		mLastError = NkError(1, "Unable to determine a valid Web canvas size.");
		return false;
	}

	SetTitle(config.title);
	InstallCanvasKeyboardFocus();

	if (IEventImpl *ev = NkGetEventImpl())
		ev->Initialize(this, nullptr);

	SetWebInputOptions(config.webInput);
	SetScreenOrientation(config.screenOrientation);
	mIsOpen = true;
	return true;
}

void NkWASMWindowImpl::Close() {
	if (!mIsOpen)
		return;
	if (IEventImpl *ev = NkGetEventImpl())
		ev->Shutdown(nullptr);
	RemoveCanvasKeyboardFocus();
	mIsOpen = false;
}

void NkWASMWindowImpl::SetTitle(const std::string &t) {
	mConfig.title = t;
	EM_ASM({ document.title = UTF8ToString($0); }, t.c_str());
}

NkVec2u NkWASMWindowImpl::GetSize() const {
	return QueryCanvasSizeSafe();
}

float NkWASMWindowImpl::GetDpiScale() const {
	return static_cast<float>(emscripten_get_device_pixel_ratio());
}

NkVec2u NkWASMWindowImpl::GetDisplaySize() const {
	int w = EM_ASM_INT({ return window.screen.width; });
	int h = EM_ASM_INT({ return window.screen.height; });
	return {static_cast<NkU32>(w), static_cast<NkU32>(h)};
}

void NkWASMWindowImpl::SetSize(NkU32 w, NkU32 h) {
	emscripten_set_canvas_element_size("#canvas", static_cast<int>(w), static_cast<int>(h));
	QueryCanvasSizeSafe();
}

void NkWASMWindowImpl::SetVisible(bool v) {
	EM_ASM(
		{
			var c = document.querySelector('#canvas');
			if (c)
				c.style.display = $0 ? "" : "none";
		},
		v ? 1 : 0);
}

void NkWASMWindowImpl::SetFullscreen(bool fs) {
	if (fs) {
		EmscriptenFullscreenStrategy s{};
		s.scaleMode = EMSCRIPTEN_FULLSCREEN_SCALE_STRETCH;
		s.canvasResolutionScaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_NONE;
		s.filteringMode = EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT;
		emscripten_enter_soft_fullscreen("#canvas", &s);
	} else {
		emscripten_exit_soft_fullscreen();
	}
	mConfig.fullscreen = fs;
}

void NkWASMWindowImpl::ShowMouse(bool show) {
	EM_ASM(
		{
			var c = document.querySelector('#canvas');
			if (c)
				c.style.cursor = $0 ? 'auto' : 'none';
		},
		show ? 1 : 0);
}

void NkWASMWindowImpl::CaptureMouse(bool cap) {
	if (cap)
		emscripten_request_pointerlock("#canvas", 1);
	else
		emscripten_exit_pointerlock();
}

void NkWASMWindowImpl::SetWebInputOptions(const NkWebInputOptions &options) {
	mConfig.webInput = options;
	NkWASMEventImpl::SetInputOptions(options);
	ApplyContextMenuPolicy(options.preventContextMenu);
}

void NkWASMWindowImpl::SetScreenOrientation(NkScreenOrientation orientation) {
	mConfig.screenOrientation = orientation;
#ifdef __EMSCRIPTEN__
	EM_ASM(
		{
			const o = $0;
			const s = screen.orientation;
			if (!s || !s.lock)
				return;

			if (o == = 0) {
				if (s.unlock)
					s.unlock();
				return;
			}

			const mode = (o == = 1) ? "portrait" : "landscape";
			s.lock(mode).catch(() = > {});
		},
		static_cast<int>(orientation));
#else
	(void)orientation;
#endif
}

NkSafeAreaInsets NkWASMWindowImpl::GetSafeAreaInsets() const {
#ifdef __EMSCRIPTEN__
	// Web safe-area insets from CSS env() are not directly accessible in C++ here.
	// Keep zero defaults for now; app layout can still rely on window size.
#endif
	return {};
}

NkSurfaceDesc NkWASMWindowImpl::GetSurfaceDesc() const {
	NkSurfaceDesc sd;
	auto sz = GetSize();
	sd.width = sz.x ? sz.x : (mConfig.width ? mConfig.width : 1u);
	sd.height = sz.y ? sz.y : (mConfig.height ? mConfig.height : 1u);
	sd.canvasId = "#canvas";
	return sd;
}

} // namespace nkentseu
