// =============================================================================
// NkWaylandWindow.cpp - NkWindow implementation for Wayland
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_WAYLAND)

#include "NKWindow/Platform/Wayland/NkWaylandWindow.h"
#include "NKWindow/Platform/Wayland/NkWaylandDropTarget.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Events/NkEventSystem.h"

#include <wayland-client.h>
#if __has_include("xdg-shell-client-protocol.h")
#   include "xdg-shell-client-protocol.h"
#elif __has_include(<xdg-shell-client-protocol.h>)
#   include <xdg-shell-client-protocol.h>
#elif __has_include(<wayland-xdg-shell-client-protocol.h>)
#   include <wayland-xdg-shell-client-protocol.h>
#else
#   error "Missing xdg-shell client protocol header. Generate xdg-shell-client-protocol.h with wayland-scanner."
#endif
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>

namespace nkentseu {

    // Surface registry: wl_surface -> NkWindow*
    static std::unordered_map<struct wl_surface*, NkWindow*> sSurfaceMap;
    static NkWindow* sLastWindow = nullptr;

    NkWindow* NkWaylandFindWindow(struct wl_surface* surface) {
        auto it = sSurfaceMap.find(surface);
        return (it != sSurfaceMap.end()) ? it->second : nullptr;
    }

    void NkWaylandRegisterWindow(struct wl_surface* surface, NkWindow* win) {
        if (!surface || !win) {
            return;
        }
        sSurfaceMap[surface] = win;
        sLastWindow = win;
    }

    void NkWaylandUnregisterWindow(struct wl_surface* surface) {
        auto it = sSurfaceMap.find(surface);
        if (it == sSurfaceMap.end()) {
            return;
        }

        NkWindow* win = it->second;
        sSurfaceMap.erase(it);

        if (sLastWindow == win) {
            sLastWindow = sSurfaceMap.empty() ? nullptr : sSurfaceMap.begin()->second;
        }
    }

    NkWindow* NkWaylandGetLastWindow() {
        return sLastWindow;
    }

    static void OnRegistryGlobal(void* data,
                                 struct wl_registry* registry,
                                 uint32_t name,
                                 const char* iface,
                                 uint32_t /*version*/) {
        NkWindow* window = static_cast<NkWindow*>(data);

        if (std::strcmp(iface, wl_compositor_interface.name) == 0) {
            window->mData.mCompositor = static_cast<struct wl_compositor*>(
                wl_registry_bind(registry, name, &wl_compositor_interface, 4));
        } else if (std::strcmp(iface, xdg_wm_base_interface.name) == 0) {
            window->mData.mWmBase = static_cast<struct xdg_wm_base*>(
                wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
        } else if (std::strcmp(iface, wl_shm_interface.name) == 0) {
            window->mData.mShm = static_cast<struct wl_shm*>(
                wl_registry_bind(registry, name, &wl_shm_interface, 1));
        } else if (std::strcmp(iface, wl_seat_interface.name) == 0) {
            window->mData.mSeat = static_cast<struct wl_seat*>(
                wl_registry_bind(registry, name, &wl_seat_interface, 7));
        } else if (std::strcmp(iface, wl_data_device_manager_interface.name) == 0) {
            window->mData.mDataDeviceManager = static_cast<struct wl_data_device_manager*>(
                wl_registry_bind(registry, name, &wl_data_device_manager_interface, 3));
        }
    }

    static void OnRegistryGlobalRemove(void*, struct wl_registry*, uint32_t) {}

    static const struct wl_registry_listener kRegistryListener = {
        .global = OnRegistryGlobal,
        .global_remove = OnRegistryGlobalRemove,
    };

    static void OnWmBasePing(void*, struct xdg_wm_base* base, uint32_t serial) {
        xdg_wm_base_pong(base, serial);
    }

    static const struct xdg_wm_base_listener kWmBaseListener = {
        .ping = OnWmBasePing,
    };

    static void OnXdgSurfaceConfigure(void* data, struct xdg_surface* surface, uint32_t serial) {
        NkWindow* window = static_cast<NkWindow*>(data);
        xdg_surface_ack_configure(surface, serial);
        window->mData.mConfigured = true;
    }

    static const struct xdg_surface_listener kXdgSurfaceListener = {
        .configure = OnXdgSurfaceConfigure,
    };

    static void OnToplevelConfigure(void* data,
                                    struct xdg_toplevel*,
                                    int32_t width,
                                    int32_t height,
                                    struct wl_array*) {
        NkWindow* window = static_cast<NkWindow*>(data);
        if (width > 0 && height > 0) {
            if (window->mData.mWidth != static_cast<uint32_t>(width) ||
                window->mData.mHeight != static_cast<uint32_t>(height)) {
                window->mData.mPrevWidth = window->mData.mWidth;
                window->mData.mPrevHeight = window->mData.mHeight;
                window->mData.mPendingResize = true;
            }
            window->mData.mWidth = static_cast<uint32_t>(width);
            window->mData.mHeight = static_cast<uint32_t>(height);
        }
    }

    static void OnToplevelClose(void* data, struct xdg_toplevel*) {
        NkWindow* window = static_cast<NkWindow*>(data);
        window->mData.mWantsClose = true;
    }

    static const struct xdg_toplevel_listener kToplevelListener = {
        .configure = OnToplevelConfigure,
        .close = OnToplevelClose,
    };

    static struct wl_buffer* CreateShmBuffer(struct wl_shm* shm,
                                             uint32_t width,
                                             uint32_t height,
                                             uint32_t stride,
                                             void** pixels) {
        const std::size_t size = static_cast<std::size_t>(stride) * height;

        char path[] = "/tmp/nk-wl-shm-XXXXXX";
        const int fd = mkstemp(path);
        if (fd < 0) {
            return nullptr;
        }
        unlink(path);

        if (ftruncate(fd, static_cast<off_t>(size)) < 0) {
            close(fd);
            return nullptr;
        }

        void* data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (data == MAP_FAILED) {
            close(fd);
            return nullptr;
        }

        struct wl_shm_pool* pool = wl_shm_create_pool(shm, fd, static_cast<int32_t>(size));
        struct wl_buffer* buffer = wl_shm_pool_create_buffer(
            pool,
            0,
            static_cast<int32_t>(width),
            static_cast<int32_t>(height),
            static_cast<int32_t>(stride),
            WL_SHM_FORMAT_ARGB8888);

        wl_shm_pool_destroy(pool);
        close(fd);

        *pixels = data;
        return buffer;
    }

    NkWindow::NkWindow() = default;

    NkWindow::NkWindow(const NkWindowConfig& config) {
        Create(config);
    }

    NkWindow::~NkWindow() {
        if (mIsOpen) {
            Close();
        }
    }

    bool NkWindow::Create(const NkWindowConfig& config) {
        mConfig = config;

        mData.mDisplay = wl_display_connect(nullptr);
        if (!mData.mDisplay) {
            mLastError = NkError(1, "Wayland: wl_display_connect failed");
            return false;
        }

        mData.mRegistry = wl_display_get_registry(mData.mDisplay);
        wl_registry_add_listener(mData.mRegistry, &kRegistryListener, this);
        wl_display_roundtrip(mData.mDisplay);

        if (!mData.mCompositor || !mData.mWmBase) {
            if (mData.mDataDeviceManager) {
                wl_data_device_manager_destroy(mData.mDataDeviceManager);
                mData.mDataDeviceManager = nullptr;
            }
            if (mData.mSeat) {
                wl_seat_destroy(mData.mSeat);
                mData.mSeat = nullptr;
            }
            if (mData.mShm) {
                wl_shm_destroy(mData.mShm);
                mData.mShm = nullptr;
            }
            if (mData.mCompositor) {
                wl_compositor_destroy(mData.mCompositor);
                mData.mCompositor = nullptr;
            }
            if (mData.mWmBase) {
                xdg_wm_base_destroy(mData.mWmBase);
                mData.mWmBase = nullptr;
            }
            if (mData.mRegistry) {
                wl_registry_destroy(mData.mRegistry);
                mData.mRegistry = nullptr;
            }
            if (mData.mDisplay) {
                wl_display_disconnect(mData.mDisplay);
                mData.mDisplay = nullptr;
            }
            mLastError = NkError(2, "Wayland: missing wl_compositor or xdg_wm_base");
            return false;
        }

        xdg_wm_base_add_listener(mData.mWmBase, &kWmBaseListener, nullptr);

        mData.mSurface = wl_compositor_create_surface(mData.mCompositor);

        mData.mXdgSurface = xdg_wm_base_get_xdg_surface(mData.mWmBase, mData.mSurface);
        xdg_surface_add_listener(mData.mXdgSurface, &kXdgSurfaceListener, this);

        mData.mXdgToplevel = xdg_surface_get_toplevel(mData.mXdgSurface);
        xdg_toplevel_add_listener(mData.mXdgToplevel, &kToplevelListener, this);
        xdg_toplevel_set_title(mData.mXdgToplevel, config.title.c_str());
        xdg_toplevel_set_app_id(mData.mXdgToplevel, config.title.c_str());

        if (!config.resizable) {
            xdg_toplevel_set_max_size(
                mData.mXdgToplevel,
                static_cast<int32_t>(config.width),
                static_cast<int32_t>(config.height));
        }
        if (config.fullscreen) {
            xdg_toplevel_set_fullscreen(mData.mXdgToplevel, nullptr);
        }

        wl_surface_commit(mData.mSurface);
        wl_display_roundtrip(mData.mDisplay);

        mData.mWidth = config.width;
        mData.mHeight = config.height;

        if (mData.mShm) {
            mData.mStride = config.width * 4;
            mData.mBuffer = CreateShmBuffer(
                mData.mShm,
                config.width,
                config.height,
                mData.mStride,
                &mData.mPixels);

            if (mData.mBuffer) {
                std::memset(mData.mPixels, 0, static_cast<std::size_t>(mData.mStride) * config.height);
                wl_surface_attach(mData.mSurface, mData.mBuffer, 0, 0);
                wl_surface_damage(
                    mData.mSurface,
                    0,
                    0,
                    static_cast<int32_t>(config.width),
                    static_cast<int32_t>(config.height));
                wl_surface_commit(mData.mSurface);
            }
        }

        mData.mPrevWidth = mData.mWidth;
        mData.mPrevHeight = mData.mHeight;
        mData.mPendingResize = false;

        NkWaylandRegisterWindow(mData.mSurface, this);
        mId = NkSystem::Instance().RegisterWindow(this);

        mData.mDropTarget = new NkWaylandDropTarget(mData.mDisplay, mData.mSeat, mData.mSurface);
        if (mData.mDropTarget) {
            mData.mDropTarget->SetDataDeviceManager(mData.mDataDeviceManager);

            mData.mDropTarget->SetDropEnterCallback([this](const NkDropEnterEvent& event) {
                NkDropEnterEvent copy(event);
                NkSystem::Events().Enqueue_Public(copy, mId);
            });
            mData.mDropTarget->SetDropLeaveCallback([this](const NkDropLeaveEvent& event) {
                NkDropLeaveEvent copy(event);
                NkSystem::Events().Enqueue_Public(copy, mId);
            });
            mData.mDropTarget->SetDropFileCallback([this](const NkDropFileEvent& event) {
                NkDropFileEvent copy(event);
                NkSystem::Events().Enqueue_Public(copy, mId);
            });
            mData.mDropTarget->SetDropTextCallback([this](const NkDropTextEvent& event) {
                NkDropTextEvent copy(event);
                NkSystem::Events().Enqueue_Public(copy, mId);
            });
        }

        mIsOpen = true;
        return true;
    }

    void NkWindow::Close() {
        if (!mIsOpen) {
            return;
        }
        mIsOpen = false;

        NkWaylandUnregisterWindow(mData.mSurface);
        NkSystem::Instance().UnregisterWindow(mId);
        mId = NK_INVALID_WINDOW_ID;

        if (mData.mDropTarget) {
            delete mData.mDropTarget;
            mData.mDropTarget = nullptr;
        }

        if (mData.mPixels && mData.mBuffer) {
            const std::size_t size = static_cast<std::size_t>(mData.mStride) * mData.mHeight;
            munmap(mData.mPixels, size);
            mData.mPixels = nullptr;
        }
        if (mData.mBuffer) {
            wl_buffer_destroy(mData.mBuffer);
            mData.mBuffer = nullptr;
        }

        if (mData.mXdgToplevel) {
            xdg_toplevel_destroy(mData.mXdgToplevel);
            mData.mXdgToplevel = nullptr;
        }
        if (mData.mXdgSurface) {
            xdg_surface_destroy(mData.mXdgSurface);
            mData.mXdgSurface = nullptr;
        }
        if (mData.mSurface) {
            wl_surface_destroy(mData.mSurface);
            mData.mSurface = nullptr;
        }
        if (mData.mWmBase) {
            xdg_wm_base_destroy(mData.mWmBase);
            mData.mWmBase = nullptr;
        }
        if (mData.mDataDeviceManager) {
            wl_data_device_manager_destroy(mData.mDataDeviceManager);
            mData.mDataDeviceManager = nullptr;
        }
        if (mData.mSeat) {
            wl_seat_destroy(mData.mSeat);
            mData.mSeat = nullptr;
        }
        if (mData.mShm) {
            wl_shm_destroy(mData.mShm);
            mData.mShm = nullptr;
        }
        if (mData.mCompositor) {
            wl_compositor_destroy(mData.mCompositor);
            mData.mCompositor = nullptr;
        }
        if (mData.mRegistry) {
            wl_registry_destroy(mData.mRegistry);
            mData.mRegistry = nullptr;
        }
        if (mData.mDisplay) {
            wl_display_disconnect(mData.mDisplay);
            mData.mDisplay = nullptr;
        }
    }

    bool NkWindow::IsOpen() const { return mIsOpen; }

    bool NkWindow::IsValid() const { return mIsOpen && mData.mSurface != nullptr; }

    NkError NkWindow::GetLastError() const { return mLastError; }

    NkWindowConfig NkWindow::GetConfig() const { return mConfig; }

    std::string NkWindow::GetTitle() const { return mConfig.title; }

    NkVec2u NkWindow::GetSize() const {
        return {
            mData.mWidth ? mData.mWidth : mConfig.width,
            mData.mHeight ? mData.mHeight : mConfig.height,
        };
    }

    NkVec2u NkWindow::GetPosition() const { return {0, 0}; }

    float NkWindow::GetDpiScale() const { return 1.f; }

    NkVec2u NkWindow::GetDisplaySize() const { return {1920, 1080}; }

    NkVec2u NkWindow::GetDisplayPosition() const { return {0, 0}; }

    void NkWindow::SetTitle(const std::string& title) {
        mConfig.title = title;
        if (mData.mXdgToplevel) {
            xdg_toplevel_set_title(mData.mXdgToplevel, title.c_str());
        }
    }

    void NkWindow::SetSize(NkU32 width, NkU32 height) {
        mConfig.width = width;
        mConfig.height = height;
        // Wayland: size is compositor-driven through configure events.
    }

    void NkWindow::SetPosition(NkI32, NkI32) {}

    void NkWindow::SetVisible(bool) {}

    void NkWindow::Minimize() {
        if (mData.mXdgToplevel) {
            xdg_toplevel_set_minimized(mData.mXdgToplevel);
        }
    }

    void NkWindow::Maximize() {
        if (mData.mXdgToplevel) {
            xdg_toplevel_set_maximized(mData.mXdgToplevel);
        }
    }

    void NkWindow::Restore() {
        if (mData.mXdgToplevel) {
            xdg_toplevel_unset_maximized(mData.mXdgToplevel);
        }
    }

    void NkWindow::SetFullscreen(bool fullscreen) {
        mConfig.fullscreen = fullscreen;
        if (!mData.mXdgToplevel) {
            return;
        }
        if (fullscreen) {
            xdg_toplevel_set_fullscreen(mData.mXdgToplevel, nullptr);
        } else {
            xdg_toplevel_unset_fullscreen(mData.mXdgToplevel);
        }
    }

    bool NkWindow::SupportsOrientationControl() const { return false; }

    void NkWindow::SetScreenOrientation(NkScreenOrientation) {}

    NkScreenOrientation NkWindow::GetScreenOrientation() const {
        return NkScreenOrientation::NK_SCREEN_ORIENTATION_LANDSCAPE;
    }

    void NkWindow::SetAutoRotateEnabled(bool) {}

    bool NkWindow::IsAutoRotateEnabled() const { return false; }

    void NkWindow::SetMousePosition(NkU32, NkU32) {}

    void NkWindow::ShowMouse(bool) {}

    void NkWindow::CaptureMouse(bool) {}

    void NkWindow::SetWebInputOptions(const NkWebInputOptions&) {}

    NkWebInputOptions NkWindow::GetWebInputOptions() const { return {}; }

    void NkWindow::SetProgress(float) {}

    NkSafeAreaInsets NkWindow::GetSafeAreaInsets() const { return {}; }

    NkSurfaceDesc NkWindow::GetSurfaceDesc() const {
        NkSurfaceDesc desc;
        const auto size = GetSize();
        desc.width = size.x;
        desc.height = size.y;
        desc.dpi = GetDpiScale();
        desc.display = mData.mDisplay;
        desc.surface = mData.mSurface;
        return desc;
    }

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_WAYLAND
