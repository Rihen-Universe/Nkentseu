#pragma once
// =============================================================================
// NkWaylandWindow.h - Wayland platform data for NkWindow (data only)
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include <cstdint>

#if defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_WAYLAND)

struct wl_display;
struct wl_registry;
struct wl_compositor;
struct wl_shm;
struct wl_seat;
struct wl_data_device_manager;
struct wl_surface;
struct xdg_wm_base;
struct xdg_surface;
struct xdg_toplevel;
struct wl_buffer;

namespace nkentseu {

    class NkWaylandDropTarget;

    struct NkWindowData {
        // Wayland globals
        struct wl_display*     mDisplay           = nullptr;
        struct wl_registry*    mRegistry          = nullptr;
        struct wl_compositor*  mCompositor        = nullptr;
        struct wl_shm*         mShm               = nullptr;
        struct wl_seat*        mSeat              = nullptr;
        struct wl_data_device_manager* mDataDeviceManager = nullptr;

        // Shell
        struct wl_surface*     mSurface           = nullptr;
        struct xdg_wm_base*    mWmBase            = nullptr;
        struct xdg_surface*    mXdgSurface        = nullptr;
        struct xdg_toplevel*   mXdgToplevel       = nullptr;

        // Software buffer
        struct wl_buffer*      mBuffer            = nullptr;
        int                    mShmFd             = -1;
        void*                  mPixels            = nullptr;
        uint32_t               mStride            = 0;

        // Runtime state
        uint32_t               mWidth             = 0;
        uint32_t               mHeight            = 0;
        bool                   mConfigured        = false;
        bool                   mWantsClose        = false;
        bool                   mFullscreen        = false;
        bool                   mPendingResize     = false;
        uint32_t               mPrevWidth         = 0;
        uint32_t               mPrevHeight        = 0;

        NkWaylandDropTarget*   mDropTarget        = nullptr;
    };

    // Backend registry accessors (map lives as static in NkWaylandWindow.cpp)
    class NkWindow;
    NkWindow* NkWaylandFindWindow(struct wl_surface* surface);
    void      NkWaylandRegisterWindow(struct wl_surface* surface, NkWindow* win);
    void      NkWaylandUnregisterWindow(struct wl_surface* surface);
    NkWindow* NkWaylandGetLastWindow();

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_WAYLAND
