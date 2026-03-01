// =============================================================================
// NkXCBWindowImpl.cpp  —  Fenêtre XCB
//
// Cycle de vie :
//   Create() → gXCBWindowRegistry().Insert() + NkGetEventImpl()->Initialize()
//   Close()  → NkGetEventImpl()->Shutdown()  + gXCBWindowRegistry().Remove()
//
// Clé EventImpl : EncodeWindow(xcb_window_t) = void* (uintptr_t cast)
// =============================================================================

#include "NkXCBWindowImpl.h"
#include "../../Core/NkSystem.h"
#include "../../Events/IEventImpl.h"
#include <xcb/xcb_icccm.h>
#include <cstring>
#include <vector>

namespace nkentseu {

    xcb_connection_t *nk_xcb_global_connection = nullptr;
    xcb_screen_t     *nk_xcb_global_screen     = nullptr;

    static xcb_atom_t InternAtom(xcb_connection_t *c, const char *name,
                                  bool onlyIfExists = false) {
        xcb_intern_atom_cookie_t ck = xcb_intern_atom(
            c, onlyIfExists ? 1 : 0,
            static_cast<uint16_t>(strlen(name)), name);
        xcb_intern_atom_reply_t *r = xcb_intern_atom_reply(c, ck, nullptr);
        if (!r) return XCB_ATOM_NONE;
        xcb_atom_t a = r->atom;
        free(r);
        return a;
    }

    // -------------------------------------------------------------------------

    NkXCBWindowImpl::~NkXCBWindowImpl() {
        if (mData.isOpen)
            Close();
    }

    bool NkXCBWindowImpl::Create(const NkWindowConfig &config) {
        mConfig  = config;
        mBgColor = config.bgColor;

        mData.width      = config.width;
        mData.height     = config.height;
        mData.connection = nk_xcb_global_connection;
        mData.screen     = nk_xcb_global_screen;

        if (!mData.connection || !mData.screen) {
            mLastError = NkError(1, "XCB connection non disponible");
            return false;
        }

        NkU8  r  = (mBgColor >> 24) & 0xFF;
        NkU8  g  = (mBgColor >> 16) & 0xFF;
        NkU8  b  = (mBgColor >>  8) & 0xFF;
        NkU32 bg = (r << 16) | (g << 8) | b;
        NkU32 fg = 0xFFFFFF;

        mData.window = xcb_generate_id(mData.connection);

        NkI32 wx = config.x, wy = config.y;
        if (config.centered) {
            wx = (mData.screen->width_in_pixels  - config.width)  / 2;
            wy = (mData.screen->height_in_pixels - config.height) / 2;
        }

        uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
        uint32_t vals[] = {
            bg,
            XCB_EVENT_MASK_EXPOSURE           |
            XCB_EVENT_MASK_STRUCTURE_NOTIFY   |
            XCB_EVENT_MASK_KEY_PRESS          |
            XCB_EVENT_MASK_KEY_RELEASE        |
            XCB_EVENT_MASK_BUTTON_PRESS       |
            XCB_EVENT_MASK_BUTTON_RELEASE     |
            XCB_EVENT_MASK_POINTER_MOTION     |
            XCB_EVENT_MASK_FOCUS_CHANGE
        };

        xcb_create_window(mData.connection, XCB_COPY_FROM_PARENT,
                          mData.window, mData.screen->root,
                          static_cast<int16_t>(wx), static_cast<int16_t>(wy),
                          static_cast<uint16_t>(config.width),
                          static_cast<uint16_t>(config.height),
                          0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                          mData.screen->root_visual, mask, vals);

        xcb_change_property(mData.connection, XCB_PROP_MODE_REPLACE,
                            mData.window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                            static_cast<uint32_t>(config.title.size()),
                            config.title.c_str());

        mData.wmProtocols = InternAtom(mData.connection, "WM_PROTOCOLS");
        mData.wmDelete    = InternAtom(mData.connection, "WM_DELETE_WINDOW");
        xcb_change_property(mData.connection, XCB_PROP_MODE_REPLACE,
                            mData.window, mData.wmProtocols,
                            XCB_ATOM_ATOM, 32, 1, &mData.wmDelete);

        mData.gc = xcb_generate_id(mData.connection);
        uint32_t gcmask   = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND;
        uint32_t gcvals[] = {fg, bg};
        xcb_create_gc(mData.connection, mData.gc, mData.window, gcmask, gcvals);

        xcb_pixmap_t pix = xcb_generate_id(mData.connection);
        xcb_create_pixmap(mData.connection, 1, pix, mData.window, 1, 1);
        mData.blankCursor = xcb_generate_id(mData.connection);
        xcb_create_cursor(mData.connection, mData.blankCursor, pix, pix,
                          0, 0, 0, 0, 0, 0, 0, 0);
        xcb_free_pixmap(mData.connection, pix);

        if (config.visible)
            xcb_map_window(mData.connection, mData.window);
        xcb_flush(mData.connection);
        mData.isOpen = true;

        // Enregistre dans le registre global XCB
        gCurrentXCBWindow = this;
        gXCBWindowRegistry().Insert(mData.window, this);

        // Enregistre dans l'EventImpl global
        if (auto *ev = NkGetEventImpl())
            ev->Initialize(this, EncodeWindow(mData.window));

        return true;
    }

    void NkXCBWindowImpl::Close() {
        if (!mData.isOpen)
            return;

        // Désenregistre de l'EventImpl d'abord
        if (auto *ev = NkGetEventImpl())
            ev->Shutdown(EncodeWindow(mData.window));

        gXCBWindowRegistry().Remove(mData.window);

        if (mData.window != XCB_WINDOW_NONE) {
            xcb_destroy_window(mData.connection, mData.window);
            mData.window = XCB_WINDOW_NONE;
        }
        if (mData.gc) {
            xcb_free_gc(mData.connection, mData.gc);
            mData.gc = 0;
        }
        xcb_flush(mData.connection);
        mData.isOpen = false;
    }

    bool    NkXCBWindowImpl::IsOpen()             const { return mData.isOpen; }
    NkError NkXCBWindowImpl::GetLastError()        const { return mLastError; }
    std::string NkXCBWindowImpl::GetTitle()        const { return mConfig.title; }
    NkU32   NkXCBWindowImpl::GetBackgroundColor()  const { return mBgColor; }
    void    NkXCBWindowImpl::SetBackgroundColor(NkU32 c) { mBgColor = c; }

    void NkXCBWindowImpl::SetTitle(const std::string &t) {
        mConfig.title = t;
        xcb_change_property(mData.connection, XCB_PROP_MODE_REPLACE,
                            mData.window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                            static_cast<uint32_t>(t.size()), t.c_str());
        xcb_flush(mData.connection);
    }

    NkVec2u NkXCBWindowImpl::GetSize() const {
        return {mData.width, mData.height};
    }

    NkVec2u NkXCBWindowImpl::GetPosition() const {
        auto ck = xcb_get_geometry(mData.connection, mData.window);
        xcb_get_geometry_reply_t *r = xcb_get_geometry_reply(mData.connection, ck, nullptr);
        NkVec2u p{};
        if (r) {
            p = {static_cast<NkU32>(r->x), static_cast<NkU32>(r->y)};
            free(r);
        }
        return p;
    }

    NkVec2u NkXCBWindowImpl::GetDisplaySize() const {
        return {mData.screen->width_in_pixels, mData.screen->height_in_pixels};
    }

    void NkXCBWindowImpl::SetSize(NkU32 w, NkU32 h) {
        mData.width  = w;
        mData.height = h;
        uint32_t v[] = {w, h};
        xcb_configure_window(mData.connection, mData.window,
                             XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, v);
        xcb_flush(mData.connection);
    }

    void NkXCBWindowImpl::SetPosition(NkI32 x, NkI32 y) {
        uint32_t v[] = {static_cast<uint32_t>(x), static_cast<uint32_t>(y)};
        xcb_configure_window(mData.connection, mData.window,
                             XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, v);
        xcb_flush(mData.connection);
    }

    void NkXCBWindowImpl::SetVisible(bool v) {
        v ? xcb_map_window(mData.connection, mData.window)
          : xcb_unmap_window(mData.connection, mData.window);
        xcb_flush(mData.connection);
    }

    void NkXCBWindowImpl::Minimize() {
        xcb_icccm_wm_hints_t h{};
        xcb_icccm_wm_hints_set_iconic(&h);
        xcb_icccm_set_wm_hints(mData.connection, mData.window, &h);
        xcb_flush(mData.connection);
    }

    void NkXCBWindowImpl::Maximize() {
        xcb_atom_t st = InternAtom(mData.connection, "_NET_WM_STATE");
        xcb_atom_t mh = InternAtom(mData.connection, "_NET_WM_STATE_MAXIMIZED_HORZ");
        xcb_atom_t mv = InternAtom(mData.connection, "_NET_WM_STATE_MAXIMIZED_VERT");
        xcb_client_message_event_t ev{};
        ev.response_type  = XCB_CLIENT_MESSAGE;
        ev.type           = st;
        ev.window         = mData.window;
        ev.format         = 32;
        ev.data.data32[0] = 1;
        ev.data.data32[1] = mh;
        ev.data.data32[2] = mv;
        xcb_send_event(mData.connection, 1, mData.screen->root,
                       XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                       XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
                       reinterpret_cast<const char *>(&ev));
        xcb_flush(mData.connection);
    }

    void NkXCBWindowImpl::Restore() {
        xcb_map_window(mData.connection, mData.window);
        xcb_flush(mData.connection);
    }

    void NkXCBWindowImpl::SetFullscreen(bool fs) {
        xcb_atom_t st = InternAtom(mData.connection, "_NET_WM_STATE");
        xcb_atom_t fa = InternAtom(mData.connection, "_NET_WM_STATE_FULLSCREEN");
        xcb_client_message_event_t ev{};
        ev.response_type  = XCB_CLIENT_MESSAGE;
        ev.type           = st;
        ev.window         = mData.window;
        ev.format         = 32;
        ev.data.data32[0] = fs ? 1 : 0;
        ev.data.data32[1] = fa;
        xcb_send_event(mData.connection, 1, mData.screen->root,
                       XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                       XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
                       reinterpret_cast<const char *>(&ev));
        xcb_flush(mData.connection);
        mConfig.fullscreen = fs;
    }

    void NkXCBWindowImpl::SetMousePosition(NkU32 x, NkU32 y) {
        xcb_warp_pointer(mData.connection, XCB_NONE, mData.window,
                         0, 0, 0, 0,
                         static_cast<int16_t>(x), static_cast<int16_t>(y));
        xcb_flush(mData.connection);
    }

    void NkXCBWindowImpl::ShowMouse(bool show) {
        xcb_change_window_attributes(mData.connection, mData.window, XCB_CW_CURSOR,
                                     show ? (const uint32_t[]){XCB_CURSOR_NONE}
                                          : &mData.blankCursor);
        xcb_flush(mData.connection);
    }

    void NkXCBWindowImpl::CaptureMouse(bool cap) {
        if (cap)
            xcb_grab_pointer(mData.connection, 1, mData.window,
                             XCB_EVENT_MASK_POINTER_MOTION |
                             XCB_EVENT_MASK_BUTTON_PRESS   |
                             XCB_EVENT_MASK_BUTTON_RELEASE,
                             XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
                             mData.window, XCB_NONE, XCB_CURRENT_TIME);
        else
            xcb_ungrab_pointer(mData.connection, XCB_CURRENT_TIME);
        xcb_flush(mData.connection);
    }

    NkSurfaceDesc NkXCBWindowImpl::GetSurfaceDesc() const {
        NkSurfaceDesc sd;
        sd.width      = mData.width;
        sd.height     = mData.height;
        sd.connection = mData.connection;
        sd.window     = mData.window;
        return sd;
    }

    void NkXCBWindowImpl::BlitSoftwareFramebuffer(const NkU8 *rgba8, NkU32 w, NkU32 h) {
        if (!mData.connection || mData.window == XCB_WINDOW_NONE || !rgba8)
            return;
        std::vector<NkU8> bgrx(static_cast<size_t>(w) * h * 4);
        for (NkU32 i = 0; i < w * h; ++i) {
            bgrx[i * 4 + 0] = rgba8[i * 4 + 2];
            bgrx[i * 4 + 1] = rgba8[i * 4 + 1];
            bgrx[i * 4 + 2] = rgba8[i * 4 + 0];
            bgrx[i * 4 + 3] = 0xFF;
        }
        xcb_put_image(mData.connection, XCB_IMAGE_FORMAT_Z_PIXMAP,
                      mData.window, mData.gc,
                      static_cast<uint16_t>(w), static_cast<uint16_t>(h),
                      0, 0, 0, mData.screen->root_depth,
                      static_cast<uint32_t>(bgrx.size()), bgrx.data());
        xcb_flush(mData.connection);
    }

} // namespace nkentseu
