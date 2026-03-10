// =============================================================================
// NkSoftwareRenderer.cpp â€” Renderer logiciel : blit OS par plateforme
// Win32  : StretchDIBits (RGBAâ†’BGRA)
// XCB    : xcb_image_put (RGBAâ†’BGRX)
// XLib   : XPutImage     (RGBAâ†’BGRX)
// Android: ANativeWindow_lock / unlockAndPost
// WASM   : Canvas 2D putImageData
// =============================================================================

#include "NkSoftwareRenderer.h"
#include "NKPlatform/NkPlatformDetect.h"
#include <cstring>
#include <algorithm>
#include <vector>

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

#if defined(NKENTSEU_WINDOWING_XCB)
#  include <xcb/xcb.h>
#  if defined(__has_include)
#    if __has_include(<xcb/xcb_image.h>)
#      include <xcb/xcb_image.h>
#      define NKENTSEU_HAS_XCB_IMAGE 1
#    else
#      define NKENTSEU_HAS_XCB_IMAGE 0
#    endif
#  else
#    define NKENTSEU_HAS_XCB_IMAGE 0
#  endif
#endif

#if defined(NKENTSEU_WINDOWING_XLIB) && !defined(NKENTSEU_WINDOWING_XCB)
#  include <X11/Xlib.h>
#  include <X11/Xutil.h>
#endif

#if defined(NKENTSEU_PLATFORM_ANDROID)
#  include <android/native_window.h>
#endif

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#  include <emscripten.h>
#  include <cstdint>
#endif

#if defined(NKENTSEU_WINDOWING_WAYLAND)
#  include <wayland-client.h>
#endif

namespace nkentseu {

// ---------------------------------------------------------------------------
// Destruction
// ---------------------------------------------------------------------------

NkSoftwareRenderer::~NkSoftwareRenderer() {
    if (mReady) Shutdown();
}

// ---------------------------------------------------------------------------
// Init / Shutdown
// ---------------------------------------------------------------------------

bool NkSoftwareRenderer::Init(const NkRendererConfig& config, const NkSurfaceDesc& surface) {
    mConfig  = config;
    mSurface = surface;
    mBgColor = 0x141414FF;
    AllocBuffer(surface.width, surface.height);
    mReady = true;
    return true;
}

void NkSoftwareRenderer::Shutdown() {
    mBuffer.Clear();
    mFbInfo = {};
    mReady  = false;
}

// ---------------------------------------------------------------------------
// Informations
// ---------------------------------------------------------------------------

bool        NkSoftwareRenderer::IsValid()               const { return mReady; }
const char* NkSoftwareRenderer::GetName()               const { return "Software"; }
bool        NkSoftwareRenderer::IsHardwareAccelerated() const { return false; }
NkError     NkSoftwareRenderer::GetLastError()          const { return mLastError; }

const NkFramebufferInfo& NkSoftwareRenderer::GetFramebufferInfo() const {
    return mFbInfo;
}

// ---------------------------------------------------------------------------
// Couleur de fond
// ---------------------------------------------------------------------------

void  NkSoftwareRenderer::SetBackgroundColor(NkU32 rgba) { mBgColor = rgba; }
NkU32 NkSoftwareRenderer::GetBackgroundColor() const     { return mBgColor; }

// ---------------------------------------------------------------------------
// Trame
// ---------------------------------------------------------------------------

void NkSoftwareRenderer::BeginFrame(NkU32 clearColor) {
    if (!mReady) return;
    NkU8 r = (clearColor >> 24) & 0xFF;
    NkU8 g = (clearColor >> 16) & 0xFF;
    NkU8 b = (clearColor >>  8) & 0xFF;
    NkU8 a =  clearColor        & 0xFF;
    NkU8* p = mBuffer.Data();
    const std::size_t count = static_cast<std::size_t>(mFbInfo.width) * mFbInfo.height;
    for (std::size_t i = 0; i < count; ++i, p += 4) {
        p[0] = r; p[1] = g; p[2] = b; p[3] = a;
    }
}

void NkSoftwareRenderer::EndFrame() {}

void NkSoftwareRenderer::Resize(NkU32 width, NkU32 height) {
    mSurface.width  = width;
    mSurface.height = height;
    AllocBuffer(width, height);
    if (!mBuffer.empty()) BeginFrame(mBgColor);
}

// ---------------------------------------------------------------------------
// Present
// ---------------------------------------------------------------------------

void NkSoftwareRenderer::Present(const NkSurfaceDesc& surface) {
    if (!mReady || mBuffer.empty()) return;
    NkU32 w = mFbInfo.width, h = mFbInfo.height;
    if (!w || !h) return;
    BlitOS(surface, w, h);
}

void NkSoftwareRenderer::BlitOS(const NkSurfaceDesc& surface, NkU32 w, NkU32 h) {
#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP)
    BlitWin32(surface, w, h);
#elif defined(NKENTSEU_WINDOWING_XCB)
    BlitXCB(surface, w, h);
#elif defined(NKENTSEU_WINDOWING_XLIB) && !defined(NKENTSEU_WINDOWING_XCB)
    BlitXLib(surface, w, h);
#elif defined(NKENTSEU_WINDOWING_WAYLAND)
    BlitWayland(surface, w, h);
#elif defined(NKENTSEU_PLATFORM_ANDROID)
    BlitANW(surface, w, h);
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    BlitWASM(surface, w, h);
#else
    (void)surface; (void)w; (void)h;
#endif
}

// ---------------------------------------------------------------------------
// SetPixel
// ---------------------------------------------------------------------------

void NkSoftwareRenderer::SetPixel(NkI32 x, NkI32 y, NkU32 rgba) {
    if (!mReady) return;
    if (x < 0 || y < 0 ||
        static_cast<NkU32>(x) >= mFbInfo.width ||
        static_cast<NkU32>(y) >= mFbInfo.height) return;

    NkU8 sr = (rgba >> 24) & 0xFF;
    NkU8 sg = (rgba >> 16) & 0xFF;
    NkU8 sb = (rgba >>  8) & 0xFF;
    NkU8 sa =  rgba         & 0xFF;

    NkU8* dst = mBuffer.Data() + static_cast<std::size_t>(y) * mFbInfo.pitch + static_cast<std::size_t>(x) * 4;

    if (sa == 255) {
        dst[0] = sr; dst[1] = sg; dst[2] = sb; dst[3] = 255;
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

void NkSoftwareRenderer::AllocBuffer(NkU32 w, NkU32 h) {
    if (!w || !h) return;
    mFbInfo.width  = w;
    mFbInfo.height = h;
    mFbInfo.pitch  = w * 4;
    mBuffer.Clear();
    mBuffer.Resize(static_cast<NkVector<NkU8>::SizeType>(static_cast<std::size_t>(w) * h * 4), static_cast<NkU8>(0));
    mFbInfo.pixels = mBuffer.Data();
}

// ---------------------------------------------------------------------------
// BlitWin32
// ---------------------------------------------------------------------------

void NkSoftwareRenderer::BlitWin32(const NkSurfaceDesc& sd, NkU32 w, NkU32 h) {
#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP)
    if (!sd.hwnd || !IsWindow(sd.hwnd)) return;

    NkVector<NkU8> bgra(mBuffer.Size());
    const NkU8* src = mBuffer.Data();
    NkU8*       dst = bgra.Data();
    const std::size_t n = static_cast<std::size_t>(w) * h;
    for (std::size_t i = 0; i < n; ++i, src += 4, dst += 4) {
        dst[0] = src[2]; dst[1] = src[1]; dst[2] = src[0]; dst[3] = src[3];
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = static_cast<LONG>(w);
    bmi.bmiHeader.biHeight      = -static_cast<LONG>(h);
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC  hdc = GetDC(sd.hwnd);
    RECT rc  = {};
    GetClientRect(sd.hwnd, &rc);
    StretchDIBits(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
                  0, 0, static_cast<int>(w), static_cast<int>(h),
                  bgra.Data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
    ReleaseDC(sd.hwnd, hdc);
#else
    (void)sd; (void)w; (void)h;
#endif
}

// ---------------------------------------------------------------------------
// BlitXCB
// ---------------------------------------------------------------------------

void NkSoftwareRenderer::BlitXCB(const NkSurfaceDesc& sd, NkU32 w, NkU32 h) {
#if defined(NKENTSEU_WINDOWING_XCB)
    if (!sd.connection || sd.window == XCB_WINDOW_NONE) return;
    if (xcb_connection_has_error(sd.connection)) return;

    NkVector<NkU8> bgrx;
    bgrx.Resize(static_cast<NkU32>(w) * h * 4);
    const NkU8* src = mBuffer.Data();
    NkU8*       dst = bgrx.Data();
    const std::size_t n = static_cast<std::size_t>(w) * h;
    for (std::size_t i = 0; i < n; ++i, src += 4, dst += 4) {
        dst[0] = src[2]; dst[1] = src[1]; dst[2] = src[0]; dst[3] = 0;
    }

    xcb_gcontext_t gc  = xcb_generate_id(sd.connection);
    uint32_t mask = XCB_GC_FOREGROUND, val = 0;
    xcb_create_gc(sd.connection, gc, sd.window, mask, &val);

#if NKENTSEU_HAS_XCB_IMAGE
    xcb_image_t* img = xcb_image_create_native(
        sd.connection,
        static_cast<uint16_t>(w), static_cast<uint16_t>(h),
        XCB_IMAGE_FORMAT_Z_PIXMAP, 24,
        nullptr, static_cast<uint32_t>(bgrx.Size()), bgrx.Data());
    if (img) {
        xcb_image_put(sd.connection, sd.window, gc, img, 0, 0, 0);
        xcb_image_destroy(img);
    }
#else
    xcb_put_image(
        sd.connection,
        XCB_IMAGE_FORMAT_Z_PIXMAP,
        sd.window,
        gc,
        static_cast<uint16_t>(w),
        static_cast<uint16_t>(h),
        0,
        0,
        0,
        24,
        static_cast<uint32_t>(bgrx.Size()),
        bgrx.Data());
#endif
    xcb_free_gc(sd.connection, gc);
    xcb_flush(sd.connection);
#else
    (void)sd; (void)w; (void)h;
#endif
}

// ---------------------------------------------------------------------------
// BlitXLib
// ---------------------------------------------------------------------------

void NkSoftwareRenderer::BlitXLib(const NkSurfaceDesc& sd, NkU32 w, NkU32 h) {
#if defined(NKENTSEU_WINDOWING_XLIB) && !defined(NKENTSEU_WINDOWING_XCB)
    if (!sd.display || sd.window == 0) return;

    NkVector<NkU8> bgr;
    bgr.Resize(static_cast<NkU32>(w) * h * 4);
    const NkU8* src = mBuffer.Data();
    NkU8*       dst = bgr.Data();
    const std::size_t n = static_cast<std::size_t>(w) * h;
    for (std::size_t i = 0; i < n; ++i, src += 4, dst += 4) {
        dst[0] = src[2]; dst[1] = src[1]; dst[2] = src[0]; dst[3] = 0;
    }

    XImage* img = XCreateImage(
        sd.display, DefaultVisual(sd.display, DefaultScreen(sd.display)),
        24, ZPixmap, 0,
        reinterpret_cast<char*>(bgr.Data()), w, h, 32, 0);
    if (img) {
        GC gc = DefaultGC(sd.display, DefaultScreen(sd.display));
        XPutImage(sd.display, sd.window, gc, img, 0, 0, 0, 0, w, h);
        img->data = nullptr;
        XDestroyImage(img);
    }
    XFlush(sd.display);
#else
    (void)sd; (void)w; (void)h;
#endif
}

// ---------------------------------------------------------------------------
// BlitANW (Android)
// ---------------------------------------------------------------------------

void NkSoftwareRenderer::BlitANW(const NkSurfaceDesc& sd, NkU32 w, NkU32 h) {
#if defined(NKENTSEU_PLATFORM_ANDROID)
    ANativeWindow* anw = sd.nativeWindow;
    if (!anw) return;

    ANativeWindow_Buffer buf{};
    ARect dirty = {0, 0, static_cast<int32_t>(w), static_cast<int32_t>(h)};
    if (ANativeWindow_lock(anw, &buf, &dirty) != 0) return;

    NkU8*       out    = static_cast<NkU8*>(buf.bits);
    const NkU8* in     = mBuffer.Data();
    NkU32       stride = static_cast<NkU32>(buf.stride) * 4;

    for (NkU32 row = 0; row < h && row < static_cast<NkU32>(buf.height); ++row) {
        std::memcpy(out + row * stride, in + row * w * 4,
                    std::min(w, static_cast<NkU32>(buf.stride)) * 4);
    }
    ANativeWindow_unlockAndPost(anw);
#else
    (void)sd; (void)w; (void)h;
#endif
}

// ---------------------------------------------------------------------------
// BlitWASM
// ---------------------------------------------------------------------------

void NkSoftwareRenderer::BlitWASM(const NkSurfaceDesc& sd, NkU32 w, NkU32 h) {
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    if (mBuffer.empty() || w == 0 || h == 0) return;

    const char* canvasId = (sd.canvasId && sd.canvasId[0] != '\0') ? sd.canvasId : "#canvas";
    const std::uintptr_t ptr = reinterpret_cast<std::uintptr_t>(mBuffer.Data());
    const int iw = static_cast<int>(w);
    const int ih = static_cast<int>(h);

    EM_ASM({
        var selector = UTF8ToString($0);
        var ptr      = $1 >>> 0;
        var w        = $2 | 0;
        var h        = $3 | 0;
        var size     = (w * h * 4) | 0;
        var canvas   = document.querySelector(selector);
        if (!canvas && Module["canvas"]) canvas = Module["canvas"];
        if (!canvas) return;
        if (canvas.width  !== w) canvas.width  = w;
        if (canvas.height !== h) canvas.height = h;
        var ctx = canvas.__nk_ctx2d;
        if (!ctx) { ctx = canvas.getContext("2d"); canvas.__nk_ctx2d = ctx; }
        if (!ctx) return;
        var imageData = canvas.__nk_imgData;
        if (!imageData || imageData.width !== w || imageData.height !== h) {
            imageData = ctx.createImageData(w, h);
            canvas.__nk_imgData = imageData;
        }
        var heap = (typeof HEAPU8 !== "undefined") ? HEAPU8
                 : ((typeof Module !== "undefined" && Module["HEAPU8"]) ? Module["HEAPU8"] : null);
        if (!heap) return;
        imageData.data.set(heap.subarray(ptr, ptr + size));
        for (var i = 3; i < size; i += 4) imageData.data[i] = 255;
        ctx.putImageData(imageData, 0, 0);
    }, canvasId, ptr, iw, ih);
#else
    (void)sd; (void)w; (void)h;
#endif
}

// ---------------------------------------------------------------------------
// BlitCocoa (stub â€” Metal/NSView required for full impl)
// ---------------------------------------------------------------------------

void NkSoftwareRenderer::BlitCocoa(const NkSurfaceDesc& sd, NkU32 w, NkU32 h) {
    (void)sd; (void)w; (void)h;
}

// ---------------------------------------------------------------------------
// BlitWayland â€” copy software framebuffer into Wayland SHM buffer and commit
// ---------------------------------------------------------------------------

void NkSoftwareRenderer::BlitWayland(const NkSurfaceDesc& sd, NkU32 w, NkU32 h) {
#if defined(NKENTSEU_WINDOWING_WAYLAND)
    if (!sd.display || !sd.surface || !sd.shmPixels || !sd.shmBuffer || !sd.shmStride) return;
    if (!sd.waylandConfigured) return;

    // Software renderer stores RGBA bytes: [R, G, B, A].
    // WL_SHM_FORMAT_ARGB8888 on little-endian stores bytes: [B, G, R, A].
    // Swap R and B when copying.
    const NkU8* src  = mBuffer.Data();
    NkU8*       dst  = static_cast<NkU8*>(sd.shmPixels);
    const NkU32 rows = (h < mFbInfo.height) ? h : mFbInfo.height;
    const NkU32 cols = (w < mFbInfo.width)  ? w : mFbInfo.width;

    for (NkU32 row = 0; row < rows; ++row) {
        const NkU8* srcRow = src + static_cast<std::size_t>(row) * mFbInfo.pitch;
        NkU8*       dstRow = dst + static_cast<std::size_t>(row) * sd.shmStride;
        for (NkU32 col = 0; col < cols; ++col) {
            dstRow[col*4 + 0] = srcRow[col*4 + 2]; // B â† src.R
            dstRow[col*4 + 1] = srcRow[col*4 + 1]; // G
            dstRow[col*4 + 2] = srcRow[col*4 + 0]; // R â† src.B
            dstRow[col*4 + 3] = 0xFF;               // A = opaque
        }
    }

    wl_surface_attach(sd.surface, sd.shmBuffer, 0, 0);
    wl_surface_damage(sd.surface, 0, 0,
                      static_cast<int32_t>(w), static_cast<int32_t>(h));
    wl_surface_commit(sd.surface);
    wl_display_flush(sd.display);
#else
    (void)sd; (void)w; (void)h;
#endif
}

} // namespace nkentseu
