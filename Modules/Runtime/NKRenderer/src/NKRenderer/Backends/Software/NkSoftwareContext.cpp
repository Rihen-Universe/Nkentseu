// =============================================================================
// NkSoftwareContext.cpp — Production Ready
// Patch OnResize : mCachedSurface stocke la NkSurfaceDesc pour réinitialiser
// le presenter natif après resize.
// =============================================================================
#include "NkSoftwareContext.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <algorithm>

// ── Includes natifs ───────────────────────────────────────────────────────────
#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#   endif
#   include <windows.h>
#elif defined(NKENTSEU_WINDOWING_XLIB)
#   include <X11/Xlib.h>
#   include <X11/Xutil.h>
#   include <X11/extensions/XShm.h>
#   include <sys/ipc.h>
#   include <sys/shm.h>
#elif defined(NKENTSEU_WINDOWING_XCB)
#   include <xcb/xcb.h>
#   include <xcb/xcb_image.h>
#elif defined(NKENTSEU_WINDOWING_WAYLAND)
#   include <wayland-client.h>
#   include <sys/mman.h>
#   include <unistd.h>
#elif defined(NKENTSEU_PLATFORM_ANDROID)
#   include <android/native_window.h>
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#   include <emscripten.h>
#   include <emscripten/html5.h>
#endif

namespace nkentseu {

    // =============================================================================
    NkSoftwareContext::~NkSoftwareContext() { if (mIsValid) Shutdown(); }

    bool NkSoftwareContext::Initialize(const NkWindow& window, const NkContextDesc& desc) {
        if (mIsValid) { logger.Error("[NkSoftware] Already initialized"); return false; }
        mDesc = desc;
        const NkSurfaceDesc surf = window.GetSurfaceDesc();
        if (!surf.IsValid()) { logger.Error("[NkSoftware] Invalid NkSurfaceDesc"); return false; }

        mCachedSurface = surf; // PATCH : stocker pour OnResize

        mBackBuffer.Resize(surf.width, surf.height);
        mFrontBuffer.Resize(surf.width, surf.height);

        if (!InitNativePresenter(surf)) return false;

        mData.width  = surf.width;
        mData.height = surf.height;
        mIsValid     = true;
        logger.Info("[NkSoftware] Ready ({0}x{1})", surf.width, surf.height);
        return true;
    }

    void NkSoftwareContext::Shutdown() {
        if (!mIsValid) return;
        ShutdownNativePresenter();
        mBackBuffer  = NkSoftwareFramebuffer{};
        mFrontBuffer = NkSoftwareFramebuffer{};
        mIsValid = false;
        logger.Info("[NkSoftware] Shutdown OK");
    }

    bool NkSoftwareContext::BeginFrame() {
        if (!mIsValid) return false;
        mBackBuffer.Clear(25, 25, 25);
        return true;
    }

    void NkSoftwareContext::EndFrame() {
        if (!mIsValid) return;
        // Swap back ↔ front
        std::swap(mBackBuffer, mFrontBuffer);
    }

    void NkSoftwareContext::Present() {
        if (!mIsValid) return;
        PresentNative();
    }

    bool NkSoftwareContext::OnResize(uint32 w, uint32 h) {
        if (!mIsValid) return false;
        if (w == 0 || h == 0) return true; // Minimisée — skip

        // PATCH CRITIQUE : ShutdownNativePresenter avant de recréer les buffers
        ShutdownNativePresenter();

        mBackBuffer.Resize(w, h);
        mFrontBuffer.Resize(w, h);
        mData.width  = w;
        mData.height = h;

        // Mettre à jour la surface cachée avec les nouvelles dimensions
        mCachedSurface.width  = w;
        mCachedSurface.height = h;

        // Réinitialiser le presenter natif avec la surface mise à jour
        if (!InitNativePresenter(mCachedSurface)) {
            logger.Error("[NkSoftware] OnResize: InitNativePresenter failed");
            mIsValid = false;
            return false;
        }
        return true;
    }

    void NkSoftwareContext::SetVSync(bool e) { mVSync = e; }
    bool NkSoftwareContext::GetVSync()  const { return mVSync; }
    bool NkSoftwareContext::IsValid()   const { return mIsValid; }
    NkGraphicsApi NkSoftwareContext::GetApi()  const { return NkGraphicsApi::NK_API_SOFTWARE; }
    NkContextDesc NkSoftwareContext::GetDesc() const { return mDesc; }
    void* NkSoftwareContext::GetNativeContextData() { return &mData; }
    bool NkSoftwareContext::SupportsCompute() const { return false; }

    NkContextInfo NkSoftwareContext::GetInfo() const {
        NkContextInfo i;
        i.api              = NkGraphicsApi::NK_API_SOFTWARE;
        i.renderer         = "Software CPU";
        i.vendor           = "NkEngine";
        i.version          = "1.0";
        i.computeSupported = false;
        return i;
    }

    // =============================================================================
    //  InitNativePresenter — par plateforme
    // =============================================================================
    bool NkSoftwareContext::InitNativePresenter(const NkSurfaceDesc& surf) {
        uint32 w = surf.width, h = surf.height;

    // ── Windows — GDI DIBSection ─────────────────────────────────────────────────
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        mData.hwnd = surf.hwnd;
        HWND hwnd  = static_cast<HWND>(surf.hwnd);
        mData.hdc  = GetDC(hwnd);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = (LONG)w;
        bmi.bmiHeader.biHeight      = -(LONG)h; // top-down
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        mData.dibBitmap = CreateDIBSection(
            static_cast<HDC>(mData.hdc), &bmi,
            DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!mData.dibBitmap) { logger.Error("[NkSoftware] CreateDIBSection failed"); return false; }

        mData.dibBits = bits;
        mData.dibDC   = CreateCompatibleDC(static_cast<HDC>(mData.hdc));
        SelectObject(static_cast<HDC>(mData.dibDC),
                    static_cast<HBITMAP>(mData.dibBitmap));
        return true;

    // ── Linux XLib — XShm (shared memory) ────────────────────────────────────────
    #elif defined(NKENTSEU_WINDOWING_XLIB)
        Display*      display = static_cast<Display*>(surf.display);
        ::Window      xwin    = (::Window)surf.window;
        mData.display = display;
        mData.window  = xwin;

        // Créer un GC
        GC gc = XCreateGC(display, xwin, 0, nullptr);
        mData.gc = (void*)gc;
        mData.shmInfo = nullptr;

        // Essayer XShm (shared memory — plus rapide)
        int shmMajor, shmMinor; Bool pixmaps;
        mData.useSHM = (XShmQueryVersion(display, &shmMajor, &shmMinor, &pixmaps) == True);

        if (mData.useSHM) {
            XShmSegmentInfo* shm = new XShmSegmentInfo();
            Visual* vis  = DefaultVisual(display, DefaultScreen(display));
            int     depth= DefaultDepth(display, DefaultScreen(display));
            XImage* img  = XShmCreateImage(display, vis, depth, ZPixmap,
                                            nullptr, shm, w, h);
            if (!img) { mData.useSHM = false; delete shm; goto fallback_xlib; }
            shm->shmid = shmget(IPC_PRIVATE, img->bytes_per_line * img->height,
                                IPC_CREAT | 0777);
            if (shm->shmid < 0) { XDestroyImage(img); mData.useSHM=false; delete shm; goto fallback_xlib; }
            shm->shmaddr = img->data = (char*)shmat(shm->shmid, nullptr, 0);
            if (shm->shmaddr == (char*)-1) {
                img->data = nullptr;
                XDestroyImage(img);
                shmctl(shm->shmid, IPC_RMID, nullptr);
                mData.useSHM = false;
                delete shm;
                goto fallback_xlib;
            }
            shm->readOnly = False;
            if (!XShmAttach(display, shm)) {
                shmdt(shm->shmaddr);
                shmctl(shm->shmid, IPC_RMID, nullptr);
                img->data = nullptr;
                XDestroyImage(img);
                mData.useSHM = false;
                delete shm;
                goto fallback_xlib;
            }
            mData.ximage = img;
            mData.shmInfo = shm;
            mData.shmid  = shm->shmid;
            logger.Info("[NkSoftware] XShm presenter OK ({0}x{1})", w, h);
            return true;
        }

        fallback_xlib: {
            // Fallback : XImage classique
            Visual* vis   = DefaultVisual(display, DefaultScreen(display));
            int     depth = DefaultDepth(display, DefaultScreen(display));
            char*   data  = new char[w * h * 4];
            XImage* img   = XCreateImage(display, vis, depth, ZPixmap, 0,
                                        data, w, h, 32, 0);
            mData.ximage = img;
            logger.Info("[NkSoftware] XImage (no SHM) presenter OK ({0}x{1})", w, h);
            return img != nullptr;
        }

    // ── Linux XCB ────────────────────────────────────────────────────────────────
    #elif defined(NKENTSEU_WINDOWING_XCB)
        xcb_connection_t* conn = static_cast<xcb_connection_t*>(surf.connection);
        mData.connection = conn;
        mData.window     = surf.window;

        xcb_screen_t* screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
        uint32 gcMask   = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND;
        uint32 gcValues[2] = { screen->black_pixel, screen->white_pixel };
        xcb_gcontext_t gc = xcb_generate_id(conn);
        xcb_create_gc(conn, gc, (xcb_window_t)surf.window, gcMask, gcValues);
        mData.gc = gc;
        xcb_flush(conn);
        return true;

    // ── Wayland — wl_shm ─────────────────────────────────────────────────────────
    #elif defined(NKENTSEU_WINDOWING_WAYLAND)
        mData.wlDisplay = surf.display;
        mData.wlSurface = surf.surface;
        mData.wlBuffer  = surf.shmBuffer;
        mData.shmPixels = surf.shmPixels;
        mData.waylandConfigured = surf.waylandConfigured;
        mData.shmStride = surf.shmStride ? surf.shmStride : (w * 4u);
        mData.shmSize   = static_cast<uint64>(mData.shmStride) * static_cast<uint64>(h);
        return (surf.display != nullptr &&
                surf.surface != nullptr &&
                surf.shmBuffer != nullptr &&
                surf.shmPixels != nullptr);

    // ── Android — ANativeWindow ───────────────────────────────────────────────────
    #elif defined(NKENTSEU_PLATFORM_ANDROID)
        mData.nativeWindow = surf.nativeWindow;
        ANativeWindow_setBuffersGeometry(
            static_cast<ANativeWindow*>(surf.nativeWindow),
            (int32_t)w, (int32_t)h,
            WINDOW_FORMAT_RGBA_8888);
        return surf.nativeWindow != nullptr;

    // ── macOS — CGContext ─────────────────────────────────────────────────────────
    #elif defined(NKENTSEU_PLATFORM_MACOS)
        mData.nsView = surf.nsView;
        // CGContext est recréé à chaque Present depuis drawRect: — pas de state ici
        return surf.nsView != nullptr;

    // ── WebAssembly ───────────────────────────────────────────────────────────────
    #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        mData.canvasId = surf.canvasId;
        return surf.canvasId != nullptr;

    #else
        return false;
    #endif
    }

    // =============================================================================
    //  ShutdownNativePresenter
    // =============================================================================
    void NkSoftwareContext::ShutdownNativePresenter() {
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        if (mData.dibDC)     { DeleteDC(static_cast<HDC>(mData.dibDC));         mData.dibDC = nullptr; }
        if (mData.dibBitmap) { DeleteObject(static_cast<HBITMAP>(mData.dibBitmap)); mData.dibBitmap = nullptr; }
        if (mData.hdc && mData.hwnd) {
            ReleaseDC(static_cast<HWND>(mData.hwnd), static_cast<HDC>(mData.hdc));
            mData.hdc = nullptr;
        }

    #elif defined(NKENTSEU_WINDOWING_XLIB)
        if (mData.ximage) {
            Display* disp = static_cast<Display*>(mData.display);
            XImage* img   = static_cast<XImage*>(mData.ximage);
            auto* shm = static_cast<XShmSegmentInfo*>(mData.shmInfo);
            if (mData.useSHM && shm) {
                XShmDetach(disp, shm);
                if (shm->shmaddr && shm->shmaddr != (char*)-1) {
                    shmdt(shm->shmaddr);
                }
                if (shm->shmid >= 0) {
                    shmctl(shm->shmid, IPC_RMID, nullptr);
                }
                delete shm;
                mData.shmInfo = nullptr;
                mData.shmid = -1;
                img->data = nullptr;
            } else {
                delete[] img->data;
                img->data = nullptr;
            }
            XDestroyImage(img);
            mData.ximage = nullptr;
        }
        if (mData.shmInfo) {
            delete static_cast<XShmSegmentInfo*>(mData.shmInfo);
            mData.shmInfo = nullptr;
        }
        mData.useSHM = false;
        if (mData.gc && mData.display) {
            XFreeGC(static_cast<Display*>(mData.display),
                    static_cast<GC>(mData.gc));
            mData.gc = nullptr;
        }

    #elif defined(NKENTSEU_WINDOWING_XCB)
        if (mData.gc && mData.connection) {
            xcb_free_gc(static_cast<xcb_connection_t*>(mData.connection),
                        (xcb_gcontext_t)mData.gc);
            mData.gc = 0;
        }

    #elif defined(NKENTSEU_WINDOWING_WAYLAND)
        // Wayland : mémoire gérée par wl_shm — pas de libération ici
        mData.wlDisplay = nullptr;
        mData.wlSurface = nullptr;
        mData.wlBuffer  = nullptr;
        mData.shmPixels = nullptr;
        mData.waylandConfigured = false;
        mData.shmStride = 0;
        mData.shmSize   = 0;
    #endif
    }

    // =============================================================================
    //  PresentNative — copie front buffer vers l'écran
    // =============================================================================
    void NkSoftwareContext::PresentNative() {
        const NkSoftwareFramebuffer& fb = mFrontBuffer;
        if (!fb.IsValid()) return;

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        // Copier pixels → DIBSection puis BitBlt vers l'écran
        if (mData.dibBits && mData.dibDC && mData.hdc) {
            memcpy(mData.dibBits, fb.pixels.data(), fb.pixels.size());
            BitBlt(static_cast<HDC>(mData.hdc), 0, 0,
                (int)fb.width, (int)fb.height,
                static_cast<HDC>(mData.dibDC), 0, 0, SRCCOPY);
        }

    #elif defined(NKENTSEU_WINDOWING_XLIB)
        Display* disp = static_cast<Display*>(mData.display);
        XImage*  img  = static_cast<XImage*>(mData.ximage);
        GC       gc   = static_cast<GC>(mData.gc);
        if (!img) return;

        // Copier pixels (RGBA → BGRA si nécessaire selon le visual)
        if (img->byte_order == LSBFirst) {
            // Convertir RGBA8 → BGRA8 pour X11
            uint32 count = fb.width * fb.height;
            uint32* src  = (uint32*)fb.pixels.data();
            uint32* dst  = (uint32*)img->data;
            for (uint32 i = 0; i < count; ++i) {
                uint32 p = src[i];
                dst[i] = ((p & 0x000000FF) << 16) |  // R→B
                        ( p & 0x0000FF00)         |  // G
                        ((p & 0x00FF0000) >> 16)   |  // B→R
                        ( p & 0xFF000000);             // A
            }
        } else {
            memcpy(img->data, fb.pixels.data(), fb.pixels.size());
        }

        if (mData.useSHM) {
            XShmPutImage(disp, (::Window)mData.window, gc, img,
                         0, 0, 0, 0, fb.width, fb.height, False);
        } else {
            XPutImage(disp, (::Window)mData.window, gc, img,
                    0, 0, 0, 0, fb.width, fb.height);
        }
        XFlush(disp);

    #elif defined(NKENTSEU_WINDOWING_XCB)
        xcb_connection_t* conn = static_cast<xcb_connection_t*>(mData.connection);
        if (!conn) return;
        xcb_put_image(conn, XCB_IMAGE_FORMAT_Z_PIXMAP,
                    (xcb_window_t)mData.window,
                    (xcb_gcontext_t)mData.gc,
                    (uint16_t)fb.width, (uint16_t)fb.height,
                    0, 0, 0, 24,
                    (uint32_t)fb.pixels.size(),
                    (const uint8_t*)fb.pixels.data());
        xcb_flush(conn);

    #elif defined(NKENTSEU_WINDOWING_WAYLAND)
        auto* wlDisplay = static_cast<wl_display*>(mData.wlDisplay);
        auto* wlSurface = static_cast<wl_surface*>(mData.wlSurface);
        auto* wlBuffer  = static_cast<wl_buffer*>(mData.wlBuffer);
        auto* shmPixels = static_cast<uint8*>(mData.shmPixels);
        if (!mData.waylandConfigured || !wlSurface || !wlBuffer || !shmPixels) return;

        const uint32 stride = mData.shmStride ? mData.shmStride : (fb.width * 4u);
        if (stride < 4u) return;

        const uint32 maxRows = static_cast<uint32>(mData.shmSize / stride);
        const uint32 rows = std::min(fb.height, maxRows);
        const uint32 cols = std::min(fb.width, stride / 4u);

        // Wayland SHM ARGB8888 on little-endian is stored as BGRA bytes.
        for (uint32 y = 0; y < rows; ++y) {
            const uint8* src = fb.RowPtr(y);
            uint8* dst = shmPixels + static_cast<uint64>(y) * static_cast<uint64>(stride);
            for (uint32 x = 0; x < cols; ++x) {
                dst[x * 4u + 0u] = src[x * 4u + 2u]; // B <- R
                dst[x * 4u + 1u] = src[x * 4u + 1u]; // G
                dst[x * 4u + 2u] = src[x * 4u + 0u]; // R <- B
                dst[x * 4u + 3u] = 255u;             // A
            }
        }

        wl_surface_attach(wlSurface, wlBuffer, 0, 0);
        wl_surface_damage(wlSurface, 0, 0, (int32_t)fb.width, (int32_t)fb.height);
        wl_surface_commit(wlSurface);
        if (wlDisplay) {
            wl_display_flush(wlDisplay);
        }

    #elif defined(NKENTSEU_PLATFORM_ANDROID)
        ANativeWindow* win = static_cast<ANativeWindow*>(mData.nativeWindow);
        if (!win) return;
        ANativeWindow_Buffer buf;
        ARect bounds = {0, 0, (int32_t)fb.width, (int32_t)fb.height};
        if (ANativeWindow_lock(win, &buf, &bounds) == 0) {
            uint32 copyW = std::min((uint32)buf.stride, fb.width);
            for (uint32 y = 0; y < fb.height && y < (uint32)buf.height; ++y) {
                memcpy((uint8_t*)buf.bits + y*buf.stride*4,
                    fb.RowPtr(y), copyW*4);
            }
            ANativeWindow_unlockAndPost(win);
        }

    #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        EM_ASM({
            var canvas = document.getElementById(UTF8ToString($0).substring(1));
            if (!canvas) return;
            var ctx = canvas.getContext('2d');
            var imgData = ctx.createImageData($1, $2);
            var heap = (typeof HEAPU8 !== 'undefined') ? HEAPU8 : null;
            if (!heap) {
                console.error("[NkSoftware] HEAPU8 unavailable in EM_ASM");
                return;
            }
            var src = new Uint8Array(heap.buffer, $3, $1 * $2 * 4);
            imgData.data.set(src);
            ctx.putImageData(imgData, 0, 0);
        },
        mData.canvasId,
        (int)fb.width, (int)fb.height,
        (int)(uintptr_t)fb.pixels.data());
    #endif
    }

} // namespace nkentseu
