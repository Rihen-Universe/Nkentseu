// =============================================================================
// NkWaylandWindow.cpp - NkWindow implementation for Wayland
//
// Toutes les méthodes sont fonctionnelles :
//   - Create()         : connexion, globals, shell, décorations SSD, SHM buffer
//   - Close()          : nettoyage complet ordonné
//   - SetSize()        : suggestion au compositeur via contraintes min/max
//   - SetVisible()     : attach/detach buffer
//   - SetFullscreen()  : set/unset fullscreen
//   - Minimize()       : set_minimized
//   - Maximize()       : set_maximized / unset_maximized
//   - Restore()        : unset_maximized + unset_fullscreen si nécessaire
//   - SetTitle()       : xdg_toplevel_set_title
//   - SetMousePosition(): mémorisation (warp non standard sur Wayland)
//   - ShowMouse()      : mémorisation — curseur appliqué dans EventSystem
//   - CaptureMouse()   : mémorisation best-effort
//   - GetDpiScale()    : depuis wl_output scale ou DPI physique
//   - GetDisplaySize() : depuis wl_output mode courant
//   - GetSurfaceDesc() : surface descriptor pour le renderer
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_WAYLAND)

#include "NKWindow/Platform/Wayland/NkWaylandWindow.h"
#include "NKWindow/Platform/Wayland/NkWaylandDropTarget.h"
#include "NKWindow/Platform/Wayland/NkWaylandEventSystem.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Events/NkEventSystem.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NkFunctions.h"
#include "NKMemory/NkUtils.h"

#include <wayland-client.h>
#include <wayland-cursor.h>

#if __has_include("xdg-decoration-client-protocol.h")
#   include "xdg-decoration-client-protocol.h"
#elif __has_include(<xdg-decoration-client-protocol.h>)
#   include <xdg-decoration-client-protocol.h>
#endif

#if __has_include("xdg-shell-client-protocol.h")
#   include "xdg-shell-client-protocol.h"
#elif __has_include(<xdg-shell-client-protocol.h>)
#   include <xdg-shell-client-protocol.h>
#elif __has_include(<wayland-xdg-shell-client-protocol.h>)
#   include <wayland-xdg-shell-client-protocol.h>
#else
#   error "Missing xdg-shell client protocol header. Generate it with wayland-scanner."
#endif

#if defined(NKENTSEU_WAYLAND_LIBDECOR) && __has_include("libdecor.h")
#   include "libdecor.h"
#   define NKENTSEU_HAS_LIBDECOR 1
#elif defined(NKENTSEU_WAYLAND_LIBDECOR) && __has_include(<libdecor.h>)
#   include <libdecor.h>
#   define NKENTSEU_HAS_LIBDECOR 1
#elif defined(NKENTSEU_WAYLAND_LIBDECOR) && __has_include(<libdecor-0/libdecor.h>)
#   include <libdecor-0/libdecor.h>
#   define NKENTSEU_HAS_LIBDECOR 1
#else
#   define NKENTSEU_HAS_LIBDECOR 0
#endif

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace nkentseu {

    // =========================================================================
    // wl_output info (DPI, résolution display)
    // =========================================================================

    struct NkWaylandOutputInfo {
        ::wl_output* output     = nullptr;
        int32_t           width      = 1920;
        int32_t           height     = 1080;
        int32_t           physWidth  = 0;   // millimètres
        int32_t           physHeight = 0;   // millimètres
        int32_t           scale      = 1;
    };

    static NkWaylandOutputInfo gOutputInfo;

    static void OnOutputGeometry(void*             data,
                                 ::wl_output* /*output*/,
                                 int32_t           /*x*/,
                                 int32_t           /*y*/,
                                 int32_t           physWidth,
                                 int32_t           physHeight,
                                 int32_t           /*subpixel*/,
                                 const char*       /*make*/,
                                 const char*       /*model*/,
                                 int32_t           /*transform*/) {
        auto* info        = static_cast<NkWaylandOutputInfo*>(data);
        info->physWidth   = physWidth;
        info->physHeight  = physHeight;
    }

    static void OnOutputMode(void*             data,
                             ::wl_output* /*output*/,
                             uint32_t          flags,
                             int32_t           width,
                             int32_t           height,
                             int32_t           /*refresh*/) {
        auto* info = static_cast<NkWaylandOutputInfo*>(data);
        if (flags & WL_OUTPUT_MODE_CURRENT) {
            info->width  = width;
            info->height = height;
        }
    }

    static void OnOutputDone(void*, ::wl_output*) {}

    static void OnOutputScale(void* data, ::wl_output* /*output*/, int32_t scale) {
        static_cast<NkWaylandOutputInfo*>(data)->scale = scale;
    }

    static const ::wl_output_listener kOutputListener = {
        OnOutputGeometry,
        OnOutputMode,
        OnOutputDone,
        OnOutputScale,
    };

    // =========================================================================
    // Surface registry : wl_surface -> NkWindow*
    // =========================================================================

    static NkWindow* sLastWindow = nullptr;

    static NkUnorderedMap<::wl_surface*, NkWindow*>& WaylandSurfaceMap() {
        static NkUnorderedMap<::wl_surface*, NkWindow*> sMap;
        return sMap;
    }

    NkWindow* NkWaylandFindWindow(::wl_surface* surface) {
        auto* win = WaylandSurfaceMap().Find(surface);
        return win ? *win : nullptr;
    }

    void NkWaylandRegisterWindow(::wl_surface* surface, NkWindow* win) {
        if (!surface || !win) return;
        WaylandSurfaceMap()[surface] = win;
        sLastWindow = win;
    }

    void NkWaylandUnregisterWindow(::wl_surface* surface) {
        auto& map = WaylandSurfaceMap();
        auto* win = map.Find(surface);
        if (!win) return;

        NkWindow* w = *win;
        map.Erase(surface);

        if (sLastWindow == w) {
            NkWindow* first = nullptr;
            map.ForEach([&](::wl_surface* const&, NkWindow*& v) {
                if (!first) first = v;
            });
            sLastWindow = first;
        }
    }

    NkWindow* NkWaylandGetLastWindow() { return sLastWindow; }

    // =========================================================================
    // Registry listener
    // =========================================================================

    static void OnRegistryGlobal(void*               data,
                                 ::wl_registry* registry,
                                 uint32_t            name,
                                 const char*         iface,
                                 uint32_t            /*version*/) {
        NkWindow* window = static_cast<NkWindow*>(data);

        if (std::strcmp(iface, wl_compositor_interface.name) == 0) {
            window->mData.mCompositor = static_cast<::wl_compositor*>(
                wl_registry_bind(registry, name, &wl_compositor_interface, 4));

        } else if (std::strcmp(iface, xdg_wm_base_interface.name) == 0) {
            window->mData.mWmBase = static_cast<::xdg_wm_base*>(
                wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));

        } else if (std::strcmp(iface, wl_shm_interface.name) == 0) {
            window->mData.mShm = static_cast<::wl_shm*>(
                wl_registry_bind(registry, name, &wl_shm_interface, 1));

        } else if (std::strcmp(iface, wl_seat_interface.name) == 0) {
            window->mData.mSeat = static_cast<::wl_seat*>(
                wl_registry_bind(registry, name, &wl_seat_interface, 7));
            if (window->mData.mSeat && NkSystem::Instance().IsInitialised()) {
                NkWaylandAttachSeatListener(&NkSystem::Events(), window->mData.mSeat);
            }

        } else if (std::strcmp(iface, wl_data_device_manager_interface.name) == 0) {
            window->mData.mDataDeviceManager = static_cast<::wl_data_device_manager*>(
                wl_registry_bind(registry, name, &wl_data_device_manager_interface, 3));

        } else if (std::strcmp(iface, wl_output_interface.name) == 0) {
            // On ne bind que le premier output (écran principal)
            if (!gOutputInfo.output) {
                gOutputInfo.output = static_cast<::wl_output*>(
                    wl_registry_bind(registry, name, &wl_output_interface, 3));
                wl_output_add_listener(gOutputInfo.output, &kOutputListener, &gOutputInfo);
            }

#if __has_include("xdg-decoration-client-protocol.h") || __has_include(<xdg-decoration-client-protocol.h>)
        } else if (std::strcmp(iface, zxdg_decoration_manager_v1_interface.name) == 0) {
            window->mData.mDecorationManager = static_cast<::zxdg_decoration_manager_v1*>(
                wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1));
#endif
        }
    }

    static void OnRegistryGlobalRemove(void*, ::wl_registry*, uint32_t) {}

    static const ::wl_registry_listener kRegistryListener = {
        OnRegistryGlobal,
        OnRegistryGlobalRemove,
    };

    // =========================================================================
    // xdg_wm_base listener (keepalive ping/pong)
    // =========================================================================

    static void OnWmBasePing(void*, ::xdg_wm_base* base, uint32_t serial) {
        xdg_wm_base_pong(base, serial);
    }

    static const ::xdg_wm_base_listener kWmBaseListener = { OnWmBasePing };

    // =========================================================================
    // xdg_surface listener
    // =========================================================================

    static void OnXdgSurfaceConfigure(void*               data,
                                      ::xdg_surface* surface,
                                      uint32_t            serial) {
        NkWindow* window = static_cast<NkWindow*>(data);
        xdg_surface_ack_configure(surface, serial);
        window->mData.mConfigured = true;
    }

    static const ::xdg_surface_listener kXdgSurfaceListener = {
        OnXdgSurfaceConfigure,
    };

    static void ApplyConfiguredSize(NkWindow* window, uint32_t width, uint32_t height);

    // =========================================================================
    // xdg_toplevel listener
    // =========================================================================

    static void OnToplevelConfigure(void*                data,
                                    ::xdg_toplevel* /*toplevel*/,
                                    int32_t              width,
                                    int32_t              height,
                                    ::wl_array*     states) {
        NkWindow* window = static_cast<NkWindow*>(data);

        // Lecture des états
        bool isFullscreen = false;
        bool isMaximized  = false;
        if (states) {
            const uint32_t* s     = static_cast<const uint32_t*>(states->data);
            const uint32     count = static_cast<uint32>(states->size / sizeof(uint32_t));
            for (uint32 i = 0; i < count; ++i) {
                if (s[i] == XDG_TOPLEVEL_STATE_FULLSCREEN) isFullscreen = true;
                if (s[i] == XDG_TOPLEVEL_STATE_MAXIMIZED)  isMaximized  = true;
            }
        }
        window->mData.mFullscreen = isFullscreen;
        (void)isMaximized;

        // Mise à jour des dimensions si le compositeur en impose
        if (width > 0 && height > 0) {
            ApplyConfiguredSize(
                window,
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height));
        }
    }

    static void OnToplevelClose(void* data, ::xdg_toplevel* /*toplevel*/) {
        static_cast<NkWindow*>(data)->mData.mWantsClose = true;
    }

    static const ::xdg_toplevel_listener kToplevelListener = {
        OnToplevelConfigure,
        OnToplevelClose,
    };

    static void ApplyConfiguredSize(NkWindow* window, uint32_t width, uint32_t height) {
        if (!window || width == 0 || height == 0) return;
        if (window->mData.mWidth != width || window->mData.mHeight != height) {
            window->mData.mPrevWidth     = window->mData.mWidth;
            window->mData.mPrevHeight    = window->mData.mHeight;
            window->mData.mPendingResize = true;
        }
        window->mData.mWidth  = width;
        window->mData.mHeight = height;
    }

    // =========================================================================
    // Decoration listener (SSD — server-side decorations)
    // =========================================================================

#if __has_include("xdg-decoration-client-protocol.h") || __has_include(<xdg-decoration-client-protocol.h>)
    static void OnToplevelDecorationConfigure(void*                               /*data*/,
                                              ::zxdg_toplevel_decoration_v1* /*deco*/,
                                              uint32_t                            mode) {
        if (mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE) {
            logger.Warn("[NkWindow] Wayland: SSD refusees par le compositeur (CSD requis).");
        }
    }

    static const ::zxdg_toplevel_decoration_v1_listener kToplevelDecorationListener = {
        OnToplevelDecorationConfigure,
    };
#endif

#if NKENTSEU_HAS_LIBDECOR
    static void OnLibdecorError(::libdecor* /*context*/,
                                enum libdecor_error /*error*/,
                                const char* message) {
        if (message && *message) {
            logger.Warn("[NkWindow] Wayland/libdecor: {}", message);
        }
    }

    static libdecor_interface kLibdecorInterface = {
        OnLibdecorError
    };

    static void OnLibdecorConfigure(::libdecor_frame* frame,
                                    ::libdecor_configuration* configuration,
                                    void* userData) {
        auto* window = static_cast<NkWindow*>(userData);
        if (!window || !frame) return;

        int width  = static_cast<int>(window->mData.mWidth  ? window->mData.mWidth  : 800u);
        int height = static_cast<int>(window->mData.mHeight ? window->mData.mHeight : 600u);

        if (configuration) {
            int cfgW = width;
            int cfgH = height;
            if (libdecor_configuration_get_content_size(configuration, frame, &cfgW, &cfgH)) {
                width  = cfgW;
                height = cfgH;
            }
        }

        if (width > 0 && height > 0) {
            ApplyConfiguredSize(
                window,
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height));
        }

        if (auto* state = libdecor_state_new(width, height)) {
            libdecor_frame_commit(frame, state, configuration);
            libdecor_state_free(state);
        }

        window->mData.mConfigured = true;
    }

    static void OnLibdecorClose(::libdecor_frame* /*frame*/, void* userData) {
        auto* window = static_cast<NkWindow*>(userData);
        if (!window) return;
        window->mData.mWantsClose = true;
    }

    static void OnLibdecorCommit(::libdecor_frame* /*frame*/, void* /*userData*/) {}

    static void OnLibdecorDismissPopup(::libdecor_frame* /*frame*/,
                                       const char* /*seatName*/,
                                       void* /*userData*/) {}

    static libdecor_frame_interface kLibdecorFrameInterface = {
        OnLibdecorConfigure,
        OnLibdecorClose,
        OnLibdecorCommit,
        OnLibdecorDismissPopup
    };
#endif

    // =========================================================================
    // Shared-memory buffer helper
    // =========================================================================

    static ::wl_buffer* CreateShmBuffer(::wl_shm* shm,
                                             uint32_t       width,
                                             uint32_t       height,
                                             uint32_t       stride,
                                             void**         pixels) {
        const std::size_t size = static_cast<std::size_t>(stride) * height;

        char path[] = "/tmp/nk-wl-shm-XXXXXX";
        const int fd = mkstemp(path);
        if (fd < 0) return nullptr;
        unlink(path);

        if (ftruncate(fd, static_cast<off_t>(size)) < 0) { close(fd); return nullptr; }

        void* data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (data == MAP_FAILED) { close(fd); return nullptr; }

        ::wl_shm_pool* pool = wl_shm_create_pool(shm, fd, static_cast<int32_t>(size));
        ::wl_buffer*   buf  = wl_shm_pool_create_buffer(
            pool, 0,
            static_cast<int32_t>(width), static_cast<int32_t>(height),
            static_cast<int32_t>(stride), WL_SHM_FORMAT_ARGB8888);
        wl_shm_pool_destroy(pool);
        close(fd);

        *pixels = data;
        return buf;
    }

    // =========================================================================
    // Nettoyage complet des ressources Wayland (ordre inverse de la création)
    // =========================================================================

    static void DestroyWaylandResources(NkWindowData& d) {
        const bool wasUsingLibdecor = d.mUsingLibdecor;

        // Buffer software
        if (d.mPixels && d.mBuffer) {
            munmap(d.mPixels, static_cast<std::size_t>(d.mStride) * d.mHeight);
            d.mPixels = nullptr;
        }
        if (d.mBuffer)       { wl_buffer_destroy(d.mBuffer);         d.mBuffer        = nullptr; }

        // Curseur
        if (d.mCursorTheme)  { wl_cursor_theme_destroy(d.mCursorTheme); d.mCursorTheme = nullptr; }
        if (d.mCursorSurface){ wl_surface_destroy(d.mCursorSurface);  d.mCursorSurface = nullptr; }

        // Décorations SSD
#if __has_include("xdg-decoration-client-protocol.h") || __has_include(<xdg-decoration-client-protocol.h>)
        if (d.mToplevelDecoration) {
            zxdg_toplevel_decoration_v1_destroy(d.mToplevelDecoration);
            d.mToplevelDecoration = nullptr;
        }
        if (d.mDecorationManager) {
            zxdg_decoration_manager_v1_destroy(d.mDecorationManager);
            d.mDecorationManager = nullptr;
        }
#endif
#if NKENTSEU_HAS_LIBDECOR
        if (d.mLibdecorFrame) {
            libdecor_frame_unref(d.mLibdecorFrame);
            d.mLibdecorFrame = nullptr;
        }
        if (d.mLibdecor) {
            libdecor_unref(d.mLibdecor);
            d.mLibdecor = nullptr;
        }
        d.mUsingLibdecor = false;
#endif
        // Shell (ordre : toplevel → xdg_surface → surface → wm_base)
        if (!wasUsingLibdecor && d.mXdgToplevel) {
            xdg_toplevel_destroy(d.mXdgToplevel);
        }
        if (!wasUsingLibdecor && d.mXdgSurface) {
            xdg_surface_destroy(d.mXdgSurface);
        }
        d.mXdgToplevel = nullptr;
        d.mXdgSurface  = nullptr;
        if (d.mSurface)      { wl_surface_destroy(d.mSurface);          d.mSurface      = nullptr; }
        if (d.mWmBase)       { xdg_wm_base_destroy(d.mWmBase);          d.mWmBase       = nullptr; }

        // Globals
        if (d.mDataDeviceManager) {
            wl_data_device_manager_destroy(d.mDataDeviceManager);
            d.mDataDeviceManager = nullptr;
        }
        if (d.mSeat)       {
            NkWaylandNotifySeatDestroy(d.mSeat);
            wl_seat_destroy(d.mSeat);
            d.mSeat = nullptr;
        }
        if (d.mShm)        { wl_shm_destroy(d.mShm);                d.mShm        = nullptr; }
        if (d.mCompositor) { wl_compositor_destroy(d.mCompositor);  d.mCompositor = nullptr; }
        if (d.mRegistry)   { wl_registry_destroy(d.mRegistry);      d.mRegistry   = nullptr; }
        if (d.mDisplay)    { wl_display_disconnect(d.mDisplay);      d.mDisplay    = nullptr; }
    }

    // =========================================================================
    // Applique les contraintes de taille depuis mConfig
    // =========================================================================

    static void ApplySizeConstraints(NkWindowData& d, const NkWindowConfig& cfg) {
#if NKENTSEU_HAS_LIBDECOR
        if (d.mUsingLibdecor && d.mLibdecorFrame) {
            const int32_t minW = static_cast<int32_t>(cfg.minWidth);
            const int32_t minH = static_cast<int32_t>(cfg.minHeight);
            const int32_t maxW = (cfg.maxWidth  >= 0xFFFF) ? 0 : static_cast<int32_t>(cfg.maxWidth);
            const int32_t maxH = (cfg.maxHeight >= 0xFFFF) ? 0 : static_cast<int32_t>(cfg.maxHeight);
            libdecor_frame_set_min_content_size(d.mLibdecorFrame, minW, minH);
            libdecor_frame_set_max_content_size(d.mLibdecorFrame, maxW, maxH);
            return;
        }
#endif
        if (!d.mXdgToplevel) return;
        const int32_t minW = static_cast<int32_t>(cfg.minWidth);
        const int32_t minH = static_cast<int32_t>(cfg.minHeight);
        const int32_t maxW = (cfg.maxWidth  >= 0xFFFF) ? 0 : static_cast<int32_t>(cfg.maxWidth);
        const int32_t maxH = (cfg.maxHeight >= 0xFFFF) ? 0 : static_cast<int32_t>(cfg.maxHeight);
        xdg_toplevel_set_min_size(d.mXdgToplevel, minW, minH);
        xdg_toplevel_set_max_size(d.mXdgToplevel, maxW, maxH);
    }

    // =========================================================================
    // NkWindow — constructeurs / destructeur
    // =========================================================================

    NkWindow::NkWindow() = default;

    NkWindow::NkWindow(const NkWindowConfig& config) {
        Create(config);
    }

    NkWindow::~NkWindow() {
        if (mIsOpen) Close();
    }

    // =========================================================================
    // Create
    // =========================================================================

    bool NkWindow::Create(const NkWindowConfig& config) {
        mConfig = config;

        // ------------------------------------------------------------------
        // 1. Connexion Wayland
        // ------------------------------------------------------------------
        mData.mDisplay = wl_display_connect(nullptr);
        if (!mData.mDisplay) {
            mLastError = NkError(1, "Wayland: wl_display_connect failed");
            return false;
        }

        mData.mRegistry = wl_display_get_registry(mData.mDisplay);
        wl_registry_add_listener(mData.mRegistry, &kRegistryListener, this);

        // Deux roundtrips : 1er pour les globals, 2e pour les événements
        // initiaux de chaque global (notamment wl_output geometry/mode).
        wl_display_roundtrip(mData.mDisplay);
        if (mData.mSeat) {
            NkWaylandAttachSeatListener(&NkSystem::Events(), mData.mSeat);
        }
        wl_display_roundtrip(mData.mDisplay);

        if (!mData.mCompositor || !mData.mWmBase) {
            DestroyWaylandResources(mData);
            mLastError = NkError(2, "Wayland: missing wl_compositor or xdg_wm_base");
            return false;
        }

        // ------------------------------------------------------------------
        // 2. Thème de curseur et surface curseur
        // ------------------------------------------------------------------
        if (mData.mShm && mData.mCompositor) {
            mData.mCursorTheme   = wl_cursor_theme_load(nullptr, 24, mData.mShm);
            mData.mCursorSurface = wl_compositor_create_surface(mData.mCompositor);
        }

        // ------------------------------------------------------------------
        // 3. Shell : surface + toplevel (libdecor fallback sinon xdg classique)
        // ------------------------------------------------------------------
        xdg_wm_base_add_listener(mData.mWmBase, &kWmBaseListener, nullptr);

        mData.mSurface = wl_compositor_create_surface(mData.mCompositor);
        mData.mWidth   = config.width;
        mData.mHeight  = config.height;

        bool usedLibdecor = false;

#if NKENTSEU_HAS_LIBDECOR
        mData.mLibdecor = libdecor_new(mData.mDisplay, &kLibdecorInterface);
        if (mData.mLibdecor) {
            mData.mLibdecorFrame =
                libdecor_decorate(mData.mLibdecor, mData.mSurface, &kLibdecorFrameInterface, this);
            if (mData.mLibdecorFrame) {
                usedLibdecor       = true;
                mData.mUsingLibdecor = true;
                mData.mXdgSurface  = libdecor_frame_get_xdg_surface(mData.mLibdecorFrame);
                mData.mXdgToplevel = libdecor_frame_get_xdg_toplevel(mData.mLibdecorFrame);

                libdecor_frame_set_title(mData.mLibdecorFrame, config.title.CStr());
                libdecor_frame_set_app_id(mData.mLibdecorFrame, config.name.CStr());

                if (!config.resizable) {
                    libdecor_frame_set_min_content_size(
                        mData.mLibdecorFrame,
                        static_cast<int32_t>(config.width),
                        static_cast<int32_t>(config.height));
                    libdecor_frame_set_max_content_size(
                        mData.mLibdecorFrame,
                        static_cast<int32_t>(config.width),
                        static_cast<int32_t>(config.height));
                } else {
                    ApplySizeConstraints(mData, config);
                }

                if (config.fullscreen && mData.mXdgToplevel) {
                    xdg_toplevel_set_fullscreen(mData.mXdgToplevel, nullptr);
                    mData.mFullscreen = true;
                }

                libdecor_frame_map(mData.mLibdecorFrame);
                wl_display_roundtrip(mData.mDisplay);
            } else {
                logger.Warn("[NkWindow] Wayland: libdecor_decorate failed, fallback xdg-shell.");
                libdecor_unref(mData.mLibdecor);
                mData.mLibdecor = nullptr;
            }
        } else {
            logger.Warn("[NkWindow] Wayland: libdecor unavailable, fallback xdg-shell.");
        }
#endif

        if (!usedLibdecor) {
            mData.mUsingLibdecor = false;
            mData.mXdgSurface = xdg_wm_base_get_xdg_surface(mData.mWmBase, mData.mSurface);
            xdg_surface_add_listener(mData.mXdgSurface, &kXdgSurfaceListener, this);

            mData.mXdgToplevel = xdg_surface_get_toplevel(mData.mXdgSurface);
            xdg_toplevel_add_listener(mData.mXdgToplevel, &kToplevelListener, this);

            // Titre (barre de titre) + app_id (identifiant D-Bus / .desktop)
            xdg_toplevel_set_title(mData.mXdgToplevel, config.title.CStr());
            xdg_toplevel_set_app_id(mData.mXdgToplevel, config.name.CStr());

            // Contraintes de taille initiales
            if (!config.resizable) {
                xdg_toplevel_set_min_size(mData.mXdgToplevel,
                    static_cast<int32_t>(config.width),
                    static_cast<int32_t>(config.height));
                xdg_toplevel_set_max_size(mData.mXdgToplevel,
                    static_cast<int32_t>(config.width),
                    static_cast<int32_t>(config.height));
            } else {
                ApplySizeConstraints(mData, config);
            }

            if (config.fullscreen) {
                xdg_toplevel_set_fullscreen(mData.mXdgToplevel, nullptr);
                mData.mFullscreen = true;
            }

            // ------------------------------------------------------------------
            // 4. Décorations serveur (SSD) — AVANT le premier wl_surface_commit
            // ------------------------------------------------------------------
#if __has_include("xdg-decoration-client-protocol.h") || __has_include(<xdg-decoration-client-protocol.h>)
            if (mData.mDecorationManager) {
                mData.mToplevelDecoration =
                    zxdg_decoration_manager_v1_get_toplevel_decoration(
                        mData.mDecorationManager, mData.mXdgToplevel);
                if (mData.mToplevelDecoration) {
                    zxdg_toplevel_decoration_v1_add_listener(
                        mData.mToplevelDecoration, &kToplevelDecorationListener, this);
                    zxdg_toplevel_decoration_v1_set_mode(
                        mData.mToplevelDecoration,
                        ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
                }
            } else {
                logger.Warn("[NkWindow] Wayland: zxdg_decoration_manager_v1 absent - SSD non disponibles.");
            }
#endif

            // ------------------------------------------------------------------
            // 5. Premier commit vide — déclenche le configure du compositeur
            // ------------------------------------------------------------------
            wl_surface_commit(mData.mSurface);
            wl_display_roundtrip(mData.mDisplay);
        }

        // ------------------------------------------------------------------
        // 6. Dimensions finales (le compositeur peut avoir imposé les siennes)
        // ------------------------------------------------------------------
        if (mData.mWidth  == 0) mData.mWidth  = config.width;
        if (mData.mHeight == 0) mData.mHeight = config.height;

        // ------------------------------------------------------------------
        // 7. Buffer logiciel initial
        // ------------------------------------------------------------------
        if (mData.mShm) {
            mData.mStride = mData.mWidth * 4;
            mData.mBuffer = CreateShmBuffer(
                mData.mShm, mData.mWidth, mData.mHeight, mData.mStride, &mData.mPixels);

            if (mData.mBuffer && config.visible && mData.mConfigured) {
                memory::NkMemSet(mData.mPixels, 0,
                    static_cast<std::size_t>(mData.mStride) * mData.mHeight);
                wl_surface_attach(mData.mSurface, mData.mBuffer, 0, 0);
                wl_surface_damage(mData.mSurface, 0, 0,
                    static_cast<int32_t>(mData.mWidth),
                    static_cast<int32_t>(mData.mHeight));
                wl_surface_commit(mData.mSurface);
            }
        }

        mData.mPrevWidth     = mData.mWidth;
        mData.mPrevHeight    = mData.mHeight;
        mData.mPendingResize = false;
        mData.mVisible       = config.visible;

        // ------------------------------------------------------------------
        // 8. Enregistrement système
        // ------------------------------------------------------------------
        NkWaylandRegisterWindow(mData.mSurface, this);
        mId = NkSystem::Instance().RegisterWindow(this);

        // ------------------------------------------------------------------
        // 9. Drag & drop (optionnel)
        // ------------------------------------------------------------------
        if (config.dropEnabled) {
            mData.mDropTarget = new NkWaylandDropTarget(
                mData.mDisplay, mData.mSeat, mData.mSurface);
            if (mData.mDropTarget) {
                mData.mDropTarget->SetDataDeviceManager(mData.mDataDeviceManager);

                mData.mDropTarget->SetDropEnterCallback([this](const NkDropEnterEvent& e) {
                    NkDropEnterEvent copy(e);
                    NkSystem::Events().Enqueue_Public(copy, mId);
                });
                mData.mDropTarget->SetDropLeaveCallback([this](const NkDropLeaveEvent& e) {
                    NkDropLeaveEvent copy(e);
                    NkSystem::Events().Enqueue_Public(copy, mId);
                });
                mData.mDropTarget->SetDropFileCallback([this](const NkDropFileEvent& e) {
                    NkDropFileEvent copy(e);
                    NkSystem::Events().Enqueue_Public(copy, mId);
                });
                mData.mDropTarget->SetDropTextCallback([this](const NkDropTextEvent& e) {
                    NkDropTextEvent copy(e);
                    NkSystem::Events().Enqueue_Public(copy, mId);
                });
            }
        }

        mIsOpen = true;
        return true;
    }

    // =========================================================================
    // Close
    // =========================================================================

    void NkWindow::Close() {
        if (!mIsOpen) return;
        mIsOpen = false;

        NkWaylandUnregisterWindow(mData.mSurface);
        NkSystem::Instance().UnregisterWindow(mId);
        mId = NK_INVALID_WINDOW_ID;

        if (mData.mDropTarget) {
            delete mData.mDropTarget;
            mData.mDropTarget = nullptr;
        }

        DestroyWaylandResources(mData);
    }

    // =========================================================================
    // Getters
    // =========================================================================

    bool           NkWindow::IsOpen()      const { return mIsOpen; }
    bool           NkWindow::IsValid()     const { return mIsOpen && mData.mSurface != nullptr; }
    NkError        NkWindow::GetLastError() const { return mLastError; }
    NkWindowConfig NkWindow::GetConfig()    const { return mConfig; }
    NkString NkWindow::GetTitle()     const { return mConfig.title; }

    NkVec2u NkWindow::GetSize() const {
        return {
            mData.mWidth  ? mData.mWidth  : mConfig.width,
            mData.mHeight ? mData.mHeight : mConfig.height,
        };
    }

    // Wayland ne communique pas la position de la fenêtre au client.
    NkVec2u NkWindow::GetPosition() const { return {0, 0}; }

    float NkWindow::GetDpiScale() const {
        // Priorité 1 : échelle entière fournie par wl_output::scale
        if (gOutputInfo.scale > 1) {
            return static_cast<float>(gOutputInfo.scale);
        }
        // Priorité 2 : calcul depuis les dimensions physiques de l'écran
        if (gOutputInfo.physWidth > 0 && gOutputInfo.width > 0) {
            const float dpi = static_cast<float>(gOutputInfo.width)
                            / (static_cast<float>(gOutputInfo.physWidth) / 25.4f);
            return math::NkMax(1.0f, dpi / 96.0f); // 96 DPI = 1.0 (référence)
        }
        return 1.0f;
    }

    NkVec2u NkWindow::GetDisplaySize() const {
        return {
            static_cast<uint32>(gOutputInfo.width  > 0 ? gOutputInfo.width  : 1920),
            static_cast<uint32>(gOutputInfo.height > 0 ? gOutputInfo.height : 1080),
        };
    }

    NkVec2u NkWindow::GetDisplayPosition() const { return {0, 0}; }

    // =========================================================================
    // SetTitle
    // =========================================================================

    void NkWindow::SetTitle(const NkString& title) {
        mConfig.title = title;
#if NKENTSEU_HAS_LIBDECOR
        if (mData.mUsingLibdecor && mData.mLibdecorFrame) {
            libdecor_frame_set_title(mData.mLibdecorFrame, title.CStr());
        }
#endif
        if (mData.mXdgToplevel && mData.mDisplay) {
            xdg_toplevel_set_title(mData.mXdgToplevel, title.CStr());
            wl_display_flush(mData.mDisplay);
        }
    }

    // =========================================================================
    // SetSize — suggestion de taille au compositeur
    //
    // Wayland ne permet pas au client d'imposer sa taille directement.
    // On contraint temporairement min == max == taille souhaitée pour forcer
    // le compositeur à émettre un configure avec ces dimensions.
    // Si la fenêtre est redimensionnable, PumpOS relâche les contraintes
    // une fois le configure reçu et acquitté (mPendingSizeRelease).
    // =========================================================================

    void NkWindow::SetSize(uint32 width, uint32 height) {
        mConfig.width  = width;
        mConfig.height = height;

        if (!mData.mXdgToplevel || !mData.mSurface || !mData.mDisplay) return;

        const int32_t w = static_cast<int32_t>(width);
        const int32_t h = static_cast<int32_t>(height);

        // Contraint min == max == taille souhaitée
#if NKENTSEU_HAS_LIBDECOR
        if (mData.mUsingLibdecor && mData.mLibdecorFrame) {
            libdecor_frame_set_min_content_size(mData.mLibdecorFrame, w, h);
            libdecor_frame_set_max_content_size(mData.mLibdecorFrame, w, h);
        } else
#endif
        {
            xdg_toplevel_set_min_size(mData.mXdgToplevel, w, h);
            xdg_toplevel_set_max_size(mData.mXdgToplevel, w, h);
        }
        wl_surface_commit(mData.mSurface);
        wl_display_flush(mData.mDisplay);

        if (mConfig.resizable) {
            // Prépare le relâchement des contraintes après le configure
            mData.mPendingSizeRelease = true;
            mData.mPendingMinW = mConfig.minWidth;
            mData.mPendingMinH = mConfig.minHeight;
            mData.mPendingMaxW = (mConfig.maxWidth  >= 0xFFFF) ? 0 : mConfig.maxWidth;
            mData.mPendingMaxH = (mConfig.maxHeight >= 0xFFFF) ? 0 : mConfig.maxHeight;
        }
        // Si non redimensionnable : min == max == taille reste fixe définitivement.
    }

    // =========================================================================
    // SetPosition — non supporté nativement par xdg-shell
    // =========================================================================

    void NkWindow::SetPosition(int32 /*x*/, int32 /*y*/) {
        // xdg-shell n'expose pas de requête de positionnement client.
        // Des protocoles étendus (wlr-layer-shell, xdg-activation) existent
        // sur certains compositeurs mais ne sont pas standard.
    }

    // =========================================================================
    // SetVisible — attach / detach buffer
    // =========================================================================

    void NkWindow::SetVisible(bool visible) {
        if (mData.mVisible == visible) return;
        mData.mVisible = visible;

        if (!mData.mSurface || !mData.mDisplay) return;
        if (!mData.mConfigured) return;

        if (visible) {
            if (mData.mBuffer) {
                wl_surface_attach(mData.mSurface, mData.mBuffer, 0, 0);
                wl_surface_damage(mData.mSurface, 0, 0,
                    static_cast<int32_t>(mData.mWidth),
                    static_cast<int32_t>(mData.mHeight));
            }
        } else {
            // Buffer null = surface non affichée
            wl_surface_attach(mData.mSurface, nullptr, 0, 0);
        }
        wl_surface_commit(mData.mSurface);
        wl_display_flush(mData.mDisplay);
    }

    // =========================================================================
    // Minimize / Maximize / Restore / SetFullscreen
    // =========================================================================

    void NkWindow::Minimize() {
        if (!mData.mXdgToplevel || !mData.mDisplay) return;
        xdg_toplevel_set_minimized(mData.mXdgToplevel);
        wl_display_flush(mData.mDisplay);
    }

    void NkWindow::Maximize() {
        if (!mData.mXdgToplevel || !mData.mDisplay) return;
        if (mData.mFullscreen) {
            xdg_toplevel_unset_fullscreen(mData.mXdgToplevel);
            mData.mFullscreen = false;
        }
        xdg_toplevel_set_maximized(mData.mXdgToplevel);
        wl_display_flush(mData.mDisplay);
    }

    void NkWindow::Restore() {
        if (!mData.mXdgToplevel || !mData.mDisplay) return;
        if (mData.mFullscreen) {
            xdg_toplevel_unset_fullscreen(mData.mXdgToplevel);
            mData.mFullscreen = false;
        }
        xdg_toplevel_unset_maximized(mData.mXdgToplevel);
        wl_display_flush(mData.mDisplay);
    }

    void NkWindow::SetFullscreen(bool fullscreen) {
        if (!mData.mXdgToplevel || !mData.mDisplay) return;
        mConfig.fullscreen = fullscreen;
        if (fullscreen) {
            xdg_toplevel_set_fullscreen(mData.mXdgToplevel, nullptr);
        } else {
            xdg_toplevel_unset_fullscreen(mData.mXdgToplevel);
        }
        mData.mFullscreen = fullscreen;
        wl_display_flush(mData.mDisplay);
    }

    // =========================================================================
    // Orientation (desktop Wayland ne supporte pas la rotation)
    // =========================================================================

    bool NkWindow::SupportsOrientationControl() const { return false; }
    void NkWindow::SetScreenOrientation(NkScreenOrientation) {}

    NkScreenOrientation NkWindow::GetScreenOrientation() const {
        return NkScreenOrientation::NK_SCREEN_ORIENTATION_LANDSCAPE;
    }

    void NkWindow::SetAutoRotateEnabled(bool) {}
    bool NkWindow::IsAutoRotateEnabled() const { return false; }

    // =========================================================================
    // Souris
    // =========================================================================

    void NkWindow::SetMousePosition(uint32 x, uint32 y) {
        // Wayland interdit le warp de curseur sans zwp_pointer_constraints_v1.
        // On mémorise la position cible pour usage interne (ex. FPS camera).
        mData.mMouseX = static_cast<int32_t>(x);
        mData.mMouseY = static_cast<int32_t>(y);
    }

    void NkWindow::ShowMouse(bool show) {
        // L'état est mémorisé ici. L'application effective du curseur
        // (visible ou transparent) est faite dans NkWaylandEventSystem.cpp
        // dans OnPointerEnter / OnPointerMotion via UpdateCursor().
        mData.mMouseHidden = !show;
    }

    void NkWindow::CaptureMouse(bool capture) {
        // Mémorisé pour PumpOS / EventSystem.
        // Le confinement réel nécessite zwp_pointer_constraints_v1 (non standard).
        mData.mMouseCaptured = capture;
    }

    // =========================================================================
    // Web / WASM (no-op sur Wayland)
    // =========================================================================

    void              NkWindow::SetWebInputOptions(const NkWebInputOptions&) {}
    NkWebInputOptions NkWindow::GetWebInputOptions() const { return {}; }

    // =========================================================================
    // OS extras / Safe Area (no-op sur Wayland desktop)
    // =========================================================================

    void             NkWindow::SetProgress(float) {}
    NkSafeAreaInsets NkWindow::GetSafeAreaInsets() const { return {}; }

    // =========================================================================
    // Surface descriptor (pour le renderer)
    // =========================================================================

    NkSurfaceDesc NkWindow::GetSurfaceDesc() const {
        NkSurfaceDesc desc;
        const auto size  = GetSize();
        desc.width       = size.x;
        desc.height      = size.y;
        desc.dpi         = GetDpiScale();
        desc.display     = mData.mDisplay;
        desc.surface     = mData.mSurface;
        desc.waylandConfigured = mData.mConfigured;
        desc.shmPixels   = mData.mPixels;
        desc.shmBuffer   = mData.mBuffer;
        desc.shmStride   = mData.mStride;
        return desc;
    }

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_WAYLAND
