// =============================================================================
// NkSoftwareRendererImpl.cpp â€” Renderer logiciel : blit OS par plateforme
// Win32  : StretchDIBits (RGBAâ†’BGRA)
// XCB    : xcb_image_put (RGBAâ†’BGRX)
// XLib   : XPutImage     (RGBAâ†’BGRX)
// Android: ANativeWindow_lock / unlockAndPost
// iOS/macOS : Metal â€” le SoftwareRenderer n'est pas le chemin privilÃ©giÃ©
// =============================================================================

#include "NkSoftwareRendererImpl.h"
#include "NkPlatformDetect.h"
#include <cstring>
#include <algorithm>
#include <vector>

// Inclusions OS conditionnelles
#if defined(NKENTSEU_FAMILY_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#if defined(NKENTSEU_PLATFORM_XCB)
#include <xcb/xcb.h>
#include <xcb/xcb_image.h>
#endif

#if defined(NKENTSEU_PLATFORM_XLIB)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

#if defined(NKENTSEU_PLATFORM_ANDROID)
#include <android/native_window.h>
#endif

#if defined(NKENTSEU_PLATFORM_WASM)
#include <emscripten.h>
#include <cstdint>
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

// ---------------------------------------------------------------------------
// Destruction
// ---------------------------------------------------------------------------

NkSoftwareRendererImpl::~NkSoftwareRendererImpl() {
	if (mReady)
		Shutdown();
}

// ---------------------------------------------------------------------------
// Init / Shutdown
// ---------------------------------------------------------------------------

bool NkSoftwareRendererImpl::Init(const NkRendererConfig &config, const NkSurfaceDesc &surface) {
	mConfig = config;
	mSurface = surface;
	mBgColor = 0x141414FF;
	AllocBuffer(surface.width, surface.height);
	mReady = true;
	return true;
}

void NkSoftwareRendererImpl::Shutdown() {
	mBuffer.clear();
	mFbInfo = {};
	mReady = false;
}

// ---------------------------------------------------------------------------
// Informations
// ---------------------------------------------------------------------------

bool NkSoftwareRendererImpl::IsValid() const {
	return mReady;
}
NkRendererApi NkSoftwareRendererImpl::GetApi() const {
	return NkRendererApi::NK_SOFTWARE;
}
std::string NkSoftwareRendererImpl::GetApiName() const {
	return "Software";
}
bool NkSoftwareRendererImpl::IsHardwareAccelerated() const {
	return false;
}
NkError NkSoftwareRendererImpl::GetLastError() const {
	return mLastError;
}
const NkFramebufferInfo &NkSoftwareRendererImpl::GetFramebufferInfo() const {
	return mFbInfo;
}

// ---------------------------------------------------------------------------
// Couleur de fond
// ---------------------------------------------------------------------------

void NkSoftwareRendererImpl::SetBackgroundColor(NkU32 rgba) {
	mBgColor = rgba;
}
NkU32 NkSoftwareRendererImpl::GetBackgroundColor() const {
	return mBgColor;
}

// ---------------------------------------------------------------------------
// Trame
// ---------------------------------------------------------------------------

void NkSoftwareRendererImpl::BeginFrame(NkU32 clearColor) {
	if (!mReady)
		return;
	NkU8 r = (clearColor >> 24) & 0xFF;
	NkU8 g = (clearColor >> 16) & 0xFF;
	NkU8 b = (clearColor >> 8) & 0xFF;
	NkU8 a = clearColor & 0xFF;
	NkU8 *p = mBuffer.data();
	const std::size_t count = static_cast<std::size_t>(mFbInfo.width) * mFbInfo.height;
	for (std::size_t i = 0; i < count; ++i, p += 4) {
		p[0] = r;
		p[1] = g;
		p[2] = b;
		p[3] = a;
	}
}

void NkSoftwareRendererImpl::EndFrame() {
}

void NkSoftwareRendererImpl::Resize(NkU32 width, NkU32 height) {
	mSurface.width = width;
	mSurface.height = height;
	AllocBuffer(width, height);
	// Fill with background color immediately so the next Present() call
	// shows the background rather than a one-frame black/zero buffer.
	if (!mBuffer.empty())
		BeginFrame(mBgColor);
}

// ---------------------------------------------------------------------------
// Present â€” dispatch vers le blit OS
// ---------------------------------------------------------------------------

void NkSoftwareRendererImpl::Present(const NkSurfaceDesc &surface) {
	if (!mReady || mBuffer.empty())
		return;
	NkU32 w = mFbInfo.width, h = mFbInfo.height;
	if (!w || !h)
		return;
	BlitOS(surface, w, h);
}

void NkSoftwareRendererImpl::BlitOS(const NkSurfaceDesc &surface, NkU32 w, NkU32 h) {
#if defined(NKENTSEU_FAMILY_WINDOWS)
	BlitWin32(surface, w, h);
#elif defined(NKENTSEU_PLATFORM_XCB)
	BlitXCB(surface, w, h);
#elif defined(NKENTSEU_PLATFORM_XLIB)
	BlitXLib(surface, w, h);
#elif defined(NKENTSEU_PLATFORM_ANDROID)
	BlitANW(surface, w, h);
#elif defined(NKENTSEU_PLATFORM_WASM)
	BlitWASM(surface, w, h);
#else
	(void)surface;
	(void)w;
	(void)h;
#endif
}

// ---------------------------------------------------------------------------
// SetPixel â€” primitive RGBA avec alpha blending
// ---------------------------------------------------------------------------

void NkSoftwareRendererImpl::SetPixel(NkI32 x, NkI32 y, NkU32 rgba) {
	if (!mReady)
		return;
	if (x < 0 || y < 0 || static_cast<NkU32>(x) >= mFbInfo.width || static_cast<NkU32>(y) >= mFbInfo.height)
		return;

	NkU8 sr = (rgba >> 24) & 0xFF;
	NkU8 sg = (rgba >> 16) & 0xFF;
	NkU8 sb = (rgba >> 8) & 0xFF;
	NkU8 sa = rgba & 0xFF;

	NkU8 *dst = mBuffer.data() + static_cast<std::size_t>(y) * mFbInfo.pitch + static_cast<std::size_t>(x) * 4;

	if (sa == 255) {
		dst[0] = sr;
		dst[1] = sg;
		dst[2] = sb;
		dst[3] = 255;
	} else if (sa > 0) {
		NkU32 a = sa, ia = 255u - a;
		dst[0] = static_cast<NkU8>((sr * a + dst[0] * ia) / 255u);
		dst[1] = static_cast<NkU8>((sg * a + dst[1] * ia) / 255u);
		dst[2] = static_cast<NkU8>((sb * a + dst[2] * ia) / 255u);
		dst[3] = 255;
	}
}

// ---------------------------------------------------------------------------
// AllocBuffer
// ---------------------------------------------------------------------------

void NkSoftwareRendererImpl::AllocBuffer(NkU32 w, NkU32 h) {
	if (!w || !h)
		return;
	mFbInfo.width = w;
	mFbInfo.height = h;
	mFbInfo.pitch = w * 4;
	mBuffer.assign(static_cast<std::size_t>(w) * h * 4, 0);
	mFbInfo.pixels = mBuffer.data();
}

// ---------------------------------------------------------------------------
// BlitWin32 : RGBA â†’ BGRA + StretchDIBits
// ---------------------------------------------------------------------------

void NkSoftwareRendererImpl::BlitWin32(const NkSurfaceDesc &sd, NkU32 w, NkU32 h) {
#if defined(NKENTSEU_FAMILY_WINDOWS)
	if (!sd.hwnd || !IsWindow(sd.hwnd))
		return;

	// Conversion in-place (copie temporaire BGRA)
	std::vector<NkU8> bgra(mBuffer.size());
	const NkU8 *src = mBuffer.data();
	NkU8 *dst = bgra.data();
	const std::size_t n = static_cast<std::size_t>(w) * h;
	for (std::size_t i = 0; i < n; ++i, src += 4, dst += 4) {
		dst[0] = src[2]; // B
		dst[1] = src[1]; // G
		dst[2] = src[0]; // R
		dst[3] = src[3]; // A
	}

	BITMAPINFO bmi{};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = static_cast<LONG>(w);
	bmi.bmiHeader.biHeight = -static_cast<LONG>(h); // top-down
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	HDC hdc = GetDC(sd.hwnd);
	RECT rc = {};
	GetClientRect(sd.hwnd, &rc);
	StretchDIBits(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, 0, 0, static_cast<int>(w), static_cast<int>(h),
				  bgra.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
	ReleaseDC(sd.hwnd, hdc);
#else
	(void)sd;
	(void)w;
	(void)h;
#endif
}

// ---------------------------------------------------------------------------
// BlitXCB : RGBA â†’ BGRX + xcb_image_put
// ---------------------------------------------------------------------------

void NkSoftwareRendererImpl::BlitXCB(const NkSurfaceDesc &sd, NkU32 w, NkU32 h) {
#if defined(NKENTSEU_PLATFORM_XCB)
	if (!sd.connection || sd.window == XCB_WINDOW_NONE)
		return;
	if (xcb_connection_has_error(sd.connection))
		return;
	xcb_connection_t *conn = sd.connection;
	xcb_window_t win = sd.window;

	// RGBA â†’ BGRX (XCB attend BGR avec padding)
	std::vector<NkU8> bgrx(static_cast<std::size_t>(w) * h * 4);
	const NkU8 *src = mBuffer.data();
	NkU8 *dst = bgrx.data();
	const std::size_t n = static_cast<std::size_t>(w) * h;
	for (std::size_t i = 0; i < n; ++i, src += 4, dst += 4) {
		dst[0] = src[2];
		dst[1] = src[1];
		dst[2] = src[0];
		dst[3] = 0;
	}

	// Obtenir le GC de la fenÃªtre
	xcb_gcontext_t gc = xcb_generate_id(conn);
	uint32_t mask = XCB_GC_FOREGROUND;
	uint32_t val = 0;
	xcb_create_gc(conn, gc, win, mask, &val);

	xcb_image_t *img =
		xcb_image_create_native(conn, static_cast<uint16_t>(w), static_cast<uint16_t>(h), XCB_IMAGE_FORMAT_Z_PIXMAP, 24,
								nullptr, static_cast<uint32_t>(bgrx.size()), bgrx.data());
	if (img) {
		xcb_image_put(conn, win, gc, img, 0, 0, 0);
		xcb_image_destroy(img);
	}
	xcb_free_gc(conn, gc);
	xcb_flush(conn);
#else
	(void)sd;
	(void)w;
	(void)h;
#endif
}

// ---------------------------------------------------------------------------
// BlitXLib : RGBA â†’ BGRX + XPutImage
// ---------------------------------------------------------------------------

void NkSoftwareRendererImpl::BlitXLib(const NkSurfaceDesc &sd, NkU32 w, NkU32 h) {
#if defined(NKENTSEU_PLATFORM_XLIB)
	if (!sd.display || sd.window == 0)
		return;
	Display *dpy = sd.display;
	::Window win = sd.window;

	std::vector<NkU8> bgr(static_cast<std::size_t>(w) * h * 4);
	const NkU8 *src = mBuffer.data();
	NkU8 *dst = bgr.data();
	const std::size_t n = static_cast<std::size_t>(w) * h;
	for (std::size_t i = 0; i < n; ++i, src += 4, dst += 4) {
		dst[0] = src[2];
		dst[1] = src[1];
		dst[2] = src[0];
		dst[3] = 0;
	}

	XImage *img = XCreateImage(dpy, DefaultVisual(dpy, DefaultScreen(dpy)), 24, ZPixmap, 0,
							   reinterpret_cast<char *>(bgr.data()), w, h, 32, 0);
	if (img) {
		GC gc = DefaultGC(dpy, DefaultScreen(dpy));
		XPutImage(dpy, win, gc, img, 0, 0, 0, 0, w, h);
		img->data = nullptr; // donnÃ©es dans bgr, pas dans l'image
		XDestroyImage(img);
	}
	XFlush(dpy);
#else
	(void)sd;
	(void)w;
	(void)h;
#endif
}

// ---------------------------------------------------------------------------
// BlitANW : RGBA â†’ ANativeWindow (Android)
// ---------------------------------------------------------------------------

void NkSoftwareRendererImpl::BlitANW(const NkSurfaceDesc &sd, NkU32 w, NkU32 h) {
#if defined(NKENTSEU_PLATFORM_ANDROID)
	ANativeWindow *anw = sd.nativeWindow;
	if (!anw)
		return;

	ANativeWindow_Buffer buf{};
	ARect dirty = {0, 0, static_cast<int32_t>(w), static_cast<int32_t>(h)};
	if (ANativeWindow_lock(anw, &buf, &dirty) != 0)
		return;

	// Android attend RGBA_8888 ou RGBX_8888 selon le format
	NkU8 *out = static_cast<NkU8 *>(buf.bits);
	const NkU8 *in = mBuffer.data();
	NkU32 stride = static_cast<NkU32>(buf.stride) * 4;

	for (NkU32 row = 0; row < h && row < static_cast<NkU32>(buf.height); ++row) {
		std::memcpy(out + row * stride, in + row * w * 4, std::min(w, static_cast<NkU32>(buf.stride)) * 4);
	}
	ANativeWindow_unlockAndPost(anw);
#else
	(void)sd;
	(void)w;
	(void)h;
#endif
}

// ---------------------------------------------------------------------------
// BlitWASM : RGBA -> canvas 2D ImageData (WebAssembly)
// ---------------------------------------------------------------------------

void NkSoftwareRendererImpl::BlitWASM(const NkSurfaceDesc &sd, NkU32 w, NkU32 h) {
#if defined(NKENTSEU_PLATFORM_WASM)
	if (mBuffer.empty() || w == 0 || h == 0)
		return;

	const char *canvasId = (sd.canvasId && sd.canvasId[0] != '\0') ? sd.canvasId : "#canvas";
	const std::uintptr_t ptr = reinterpret_cast<std::uintptr_t>(mBuffer.data());
	const int iw = static_cast<int>(w);
	const int ih = static_cast<int>(h);

	EM_ASM(
		{
			var selector = UTF8ToString($0);
			var ptr = $1 >>> 0;
			var w = $2 | 0;
			var h = $3 | 0;
			var size = (w * h * 4) | 0;

			var canvas = document.querySelector(selector);
			if (!canvas && Module["canvas"])
				canvas = Module["canvas"];
			if (!canvas)
				return;

			if (canvas.width != = w)
				canvas.width = w;
			if (canvas.height != = h)
				canvas.height = h;

			var ctx = canvas.__nk_ctx2d;
			if (!ctx) {
				ctx = canvas.getContext("2d");
				canvas.__nk_ctx2d = ctx;
			}
			if (!ctx)
				return;

			var imageData = canvas.__nk_imgData;
			if (!imageData || imageData.width != = w || imageData.height != = h) {
				imageData = ctx.createImageData(w, h);
				canvas.__nk_imgData = imageData;
			}

			var heap = (typeof HEAPU8 != = "undefined")
						   ? HEAPU8
						   : ((typeof Module != = "undefined" && Module["HEAPU8"]) ? Module["HEAPU8"] : null);
			if (!heap)
				return;
			imageData.data.set(heap.subarray(ptr, ptr + size));
			// Force every pixel fully opaque: the software renderer stores RGBA
			// with A=255, but defensive-guard against any stale 0-alpha bytes
			// that would make the canvas compositing show transparent black.
			for (var i = 3; i < size; i += 4)
				imageData.data[i] = 255;
			ctx.putImageData(imageData, 0, 0);
		},
		canvasId, ptr, iw, ih);
#else
	(void)sd;
	(void)w;
	(void)h;
#endif
}

} // namespace nkentseu

