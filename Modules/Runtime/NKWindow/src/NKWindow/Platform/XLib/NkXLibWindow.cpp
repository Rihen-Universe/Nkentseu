// =============================================================================
// NkXLibWindow.cpp - implementation XLib de NkWindow (sans PIMPL)
//
// Pattern identique a Win32/XCB:
// - donnees natives dans NkWindow::mData
// - Display global partage entre fenetres
// - registre backend xid -> NkWindow* expose via helpers
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_XLIB)

#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Events/NkEventSystem.h"
#include "NKWindow/Platform/XLib/NkXLibWindow.h"
#include "NKWindow/Platform/XLib/NkXLibDropTarget.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <mutex>
#include <unordered_map>

namespace nkentseu {

    // =============================================================================
    // Globals partages (backend registry)
    // =============================================================================

    static Display*   sDisplay        = nullptr;
    static int        sWindowCount    = 0;
    static std::mutex sDisplayMutex;
    static NkWindow*  sXLibLastWindow = nullptr;

    // Function-local static avoids static init order fiasco with NkAllocator.
    static NkUnorderedMap<::Window, NkWindow*>& XLibWindowMap() {
        static NkUnorderedMap<::Window, NkWindow*> sMap;
        return sMap;
    }

    Display* NkXLibGetDisplay() {
        return sDisplay;
    }

    NkWindow* NkXLibFindWindow(::Window xid) {
        auto* win = XLibWindowMap().Find(xid);
        return win ? *win : nullptr;
    }

    void NkXLibRegisterWindow(::Window xid, NkWindow* win) {
        XLibWindowMap()[xid] = win;
        sXLibLastWindow = win;
    }

    void NkXLibUnregisterWindow(::Window xid) {
        auto& map = XLibWindowMap();
        auto* win = map.Find(xid);
        if (!win) return;

        NkWindow* w = *win;
        map.Erase(xid);

        if (sXLibLastWindow == w) {
            NkWindow* first = nullptr;
            map.ForEach([&](::Window, NkWindow* v) { if (!first) first = v; });
            sXLibLastWindow = first;
        }
    }

    NkWindow* NkXLibGetLastWindow() {
        return sXLibLastWindow;
    }

    // =============================================================================
    // Constructors / Destructor
    // =============================================================================

    NkWindow::NkWindow() = default;

    NkWindow::NkWindow(const NkWindowConfig& c) {
        Create(c);
    }

    NkWindow::~NkWindow() {
        if (mIsOpen) {
            Close();
        }
    }

    // =============================================================================
    // Create
    // =============================================================================

    bool NkWindow::Create(const NkWindowConfig& config) {
        mConfig = config;

        std::lock_guard<std::mutex> lock(sDisplayMutex);

        if (!sDisplay) {
            sDisplay = XOpenDisplay(nullptr);
            if (!sDisplay) {
                mLastError = NkError(1, "XOpenDisplay failed");
                return false;
            }
        }
        ++sWindowCount;

        mData.mDisplay = sDisplay;
        mData.mScreen  = DefaultScreen(sDisplay);

        int x = config.centered
            ? (DisplayWidth(sDisplay, mData.mScreen) - static_cast<int>(config.width)) / 2
            : config.x;
        int y = config.centered
            ? (DisplayHeight(sDisplay, mData.mScreen) - static_cast<int>(config.height)) / 2
            : config.y;

        // Valeurs par défaut (sans hint)
        Visual*       visual = DefaultVisual(sDisplay, mData.mScreen);
        int           depth  = DefaultDepth (sDisplay, mData.mScreen);
        unsigned long vmask  = CWBackPixel | CWBorderPixel | CWEventMask;

        XSetWindowAttributes attrs = {};
        attrs.background_pixel = BlackPixel(sDisplay, mData.mScreen);
        attrs.border_pixel     = BlackPixel(sDisplay, mData.mScreen);
        attrs.event_mask       =
            ExposureMask      | StructureNotifyMask |
            KeyPressMask      | KeyReleaseMask      |
            ButtonPressMask   | ButtonReleaseMask   |
            PointerMotionMask | FocusChangeMask     |
            EnterWindowMask   | LeaveWindowMask;

        // ── Consommation du hint GLX VisualId ───────────────────────────────
        // NkWindow ne sait pas que c'est pour OpenGL.
        // Il applique mécaniquement : si le hint GlxVisualId est présent,
        // utiliser la Visual correspondante plutôt que la default.
        if (config.surfaceHints.Has(NkSurfaceHintKey::NK_GLX_VISUAL_ID)) {
            VisualID vid = static_cast<VisualID>(
                config.surfaceHints.Get(NkSurfaceHintKey::NK_GLX_VISUAL_ID));

            XVisualInfo tmpl{};
            tmpl.visualid = vid;
            int n = 0;
            XVisualInfo* vi = XGetVisualInfo(sDisplay, VisualIDMask, &tmpl, &n);

            if (vi && n > 0) {
                visual = vi->visual;
                depth  = vi->depth;
                // Une visual non-default nécessite un colormap dédié
                attrs.colormap = XCreateColormap(
                    sDisplay, DefaultRootWindow(sDisplay), visual, AllocNone);
                vmask |= CWColormap;
                // background_pixel doit être 0 avec une visual RGBA
                attrs.background_pixel = 0;
                XFree(vi);
            }
            // Si XGetVisualInfo échoue : on reste sur la visual par défaut.
            // Le contexte OpenGL échouera plus tard avec un message clair.
        }
        // ── Fin consommation hint ────────────────────────────────────────────

        mData.mXid = XCreateWindow(
            sDisplay, DefaultRootWindow(sDisplay),
            x, y, config.width, config.height,
            0,
            depth,       // ← depth éventuellement issu du hint
            InputOutput,
            visual,      // ← visual éventuellement issue du hint
            vmask,
            &attrs);

        // ── Mémoriser les hints appliqués → disponibles via GetSurfaceDesc() ─
        mData.mAppliedHints = config.surfaceHints;

        if (!mData.mXid) {
            mLastError = NkError(2, "XCreateWindow failed");
            --sWindowCount;
            return false;
        }

        // WM_DELETE_WINDOW
        mData.mWmDeleteWindow = XInternAtom(sDisplay, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(sDisplay, mData.mXid, &mData.mWmDeleteWindow, 1);

        // Title
        XStoreName(sDisplay, mData.mXid, config.title.CStr());

        // Size constraints
        if (!config.resizable) {
            XSizeHints hints = {};
            hints.flags      = PMinSize | PMaxSize;
            hints.min_width  = hints.max_width  = static_cast<int>(config.width);
            hints.min_height = hints.max_height = static_cast<int>(config.height);
            XSetWMNormalHints(sDisplay, mData.mXid, &hints);
        }

        if (config.visible) {
            XMapWindow(sDisplay, mData.mXid);
            XFlush(sDisplay);
        }

        NkXLibRegisterWindow(mData.mXid, this);
        mId = NkSystem::Instance().RegisterWindow(this);

        // XDND drop target integration (events are forwarded to NkEventSystem queue).
        mData.mDropTarget = new NkXLibDropTarget(mData.mDisplay, mData.mXid);
        if (mData.mDropTarget) {
            mData.mDropTarget->SetDropEnterCallback([this](const NkDropEnterEvent& ev) {
                NkDropEnterEvent copy(ev);
                NkSystem::Events().Enqueue_Public(copy, mId);
            });
            mData.mDropTarget->SetDropLeaveCallback([this](const NkDropLeaveEvent& ev) {
                NkDropLeaveEvent copy(ev);
                NkSystem::Events().Enqueue_Public(copy, mId);
            });
            mData.mDropTarget->SetDropFileCallback([this](const NkDropFileEvent& ev) {
                NkDropFileEvent copy(ev);
                NkSystem::Events().Enqueue_Public(copy, mId);
            });
            mData.mDropTarget->SetDropTextCallback([this](const NkDropTextEvent& ev) {
                NkDropTextEvent copy(ev);
                NkSystem::Events().Enqueue_Public(copy, mId);
            });
        }

        mIsOpen = true;
        return true;
    }

    // =============================================================================
    // Close
    // =============================================================================

    void NkWindow::Close() {
        if (!mIsOpen) {
            return;
        }
        mIsOpen = false;

        NkXLibUnregisterWindow(mData.mXid);
        NkSystem::Instance().UnregisterWindow(mId);
        mId = NK_INVALID_WINDOW_ID;

        if (mData.mDropTarget) {
            delete mData.mDropTarget;
            mData.mDropTarget = nullptr;
        }

        if (mData.mXid && mData.mDisplay) {
            XDestroyWindow(mData.mDisplay, mData.mXid);
            XFlush(mData.mDisplay);
            mData.mXid = 0;
        }

        {
            std::lock_guard<std::mutex> lock(sDisplayMutex);
            --sWindowCount;
            if (sWindowCount <= 0 && sDisplay) {
                XCloseDisplay(sDisplay);
                sDisplay = nullptr;
                sWindowCount = 0;
            }
        }
        mData.mDisplay = nullptr;
    }

    // =============================================================================
    // Queries
    // =============================================================================

    bool NkWindow::IsOpen() const { return mIsOpen; }
    bool NkWindow::IsValid() const { return mIsOpen && mData.mXid != 0; }

    NkString NkWindow::GetTitle() const { return mConfig.title; }
    NkError NkWindow::GetLastError() const { return mLastError; }
    NkWindowConfig NkWindow::GetConfig() const { return mConfig; }

    NkVec2u NkWindow::GetSize() const {
        if (!mData.mDisplay || !mData.mXid) {
            return { mConfig.width, mConfig.height };
        }
        XWindowAttributes a;
        XGetWindowAttributes(mData.mDisplay, mData.mXid, &a);
        return { static_cast<uint32>(a.width), static_cast<uint32>(a.height) };
    }

    NkVec2u NkWindow::GetPosition() const {
        if (!mData.mDisplay || !mData.mXid) {
            return { static_cast<uint32>(mConfig.x), static_cast<uint32>(mConfig.y) };
        }
        ::Window child;
        int x = 0;
        int y = 0;
        XTranslateCoordinates(
            mData.mDisplay, mData.mXid,
            DefaultRootWindow(mData.mDisplay),
            0, 0, &x, &y, &child);
        return { static_cast<uint32>(x), static_cast<uint32>(y) };
    }

    float NkWindow::GetDpiScale() const { return 1.0f; }

    NkVec2u NkWindow::GetDisplaySize() const {
        if (!mData.mDisplay) {
            return { 1920, 1080 };
        }
        return {
            static_cast<uint32>(DisplayWidth(mData.mDisplay, mData.mScreen)),
            static_cast<uint32>(DisplayHeight(mData.mDisplay, mData.mScreen))
        };
    }

    NkVec2u NkWindow::GetDisplayPosition() const { return { 0, 0 }; }

    // =============================================================================
    // Setters
    // =============================================================================

    void NkWindow::SetTitle(const NkString& title) {
        mConfig.title = title;
        if (mData.mDisplay && mData.mXid) {
            XStoreName(mData.mDisplay, mData.mXid, title.CStr());
        }
    }

    void NkWindow::SetSize(uint32 w, uint32 h) {
        mConfig.width = w;
        mConfig.height = h;
        if (mData.mDisplay && mData.mXid) {
            XResizeWindow(mData.mDisplay, mData.mXid, w, h);
        }
    }

    void NkWindow::SetPosition(int32 x, int32 y) {
        mConfig.x = x;
        mConfig.y = y;
        if (mData.mDisplay && mData.mXid) {
            XMoveWindow(mData.mDisplay, mData.mXid, x, y);
        }
    }

    void NkWindow::SetVisible(bool visible) {
        if (!mData.mDisplay || !mData.mXid) {
            return;
        }
        if (visible) {
            XMapWindow(mData.mDisplay, mData.mXid);
        } else {
            XUnmapWindow(mData.mDisplay, mData.mXid);
        }
        XFlush(mData.mDisplay);
    }

    void NkWindow::Minimize() {
        if (mData.mDisplay && mData.mXid) {
            XIconifyWindow(mData.mDisplay, mData.mXid, mData.mScreen);
        }
    }

    void NkWindow::Maximize() {
        if (!mData.mDisplay || !mData.mXid) {
            return;
        }
        Atom wmState = XInternAtom(mData.mDisplay, "_NET_WM_STATE", False);
        Atom maxH = XInternAtom(mData.mDisplay, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
        Atom maxV = XInternAtom(mData.mDisplay, "_NET_WM_STATE_MAXIMIZED_VERT", False);
        XEvent ev = {};
        ev.type = ClientMessage;
        ev.xclient.window = mData.mXid;
        ev.xclient.message_type = wmState;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
        ev.xclient.data.l[1] = static_cast<long>(maxH);
        ev.xclient.data.l[2] = static_cast<long>(maxV);
        XSendEvent(
            mData.mDisplay,
            DefaultRootWindow(mData.mDisplay),
            False,
            SubstructureNotifyMask | SubstructureRedirectMask,
            &ev);
        XFlush(mData.mDisplay);
    }

    void NkWindow::Restore() {
        if (mData.mDisplay && mData.mXid) {
            XMapWindow(mData.mDisplay, mData.mXid);
            XFlush(mData.mDisplay);
        }
    }

    void NkWindow::SetFullscreen(bool fullscreen) {
        if (!mData.mDisplay || !mData.mXid) {
            return;
        }
        mConfig.fullscreen = fullscreen;
        Atom wmState = XInternAtom(mData.mDisplay, "_NET_WM_STATE", False);
        Atom wmFs = XInternAtom(mData.mDisplay, "_NET_WM_STATE_FULLSCREEN", False);
        XEvent ev = {};
        ev.type = ClientMessage;
        ev.xclient.window = mData.mXid;
        ev.xclient.message_type = wmState;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = fullscreen ? 1 : 0;
        ev.xclient.data.l[1] = static_cast<long>(wmFs);
        XSendEvent(
            mData.mDisplay,
            DefaultRootWindow(mData.mDisplay),
            False,
            SubstructureNotifyMask | SubstructureRedirectMask,
            &ev);
        XFlush(mData.mDisplay);
    }

    // =============================================================================
    // Orientation (desktop no-op)
    // =============================================================================

    bool NkWindow::SupportsOrientationControl() const { return false; }
    void NkWindow::SetScreenOrientation(NkScreenOrientation) {}
    NkScreenOrientation NkWindow::GetScreenOrientation() const {
        return NkScreenOrientation::NK_SCREEN_ORIENTATION_AUTO;
    }
    void NkWindow::SetAutoRotateEnabled(bool) {}
    bool NkWindow::IsAutoRotateEnabled() const { return false; }

    // =============================================================================
    // Mouse
    // =============================================================================

    void NkWindow::SetMousePosition(uint32 x, uint32 y) {
        if (mData.mDisplay && mData.mXid) {
            XWarpPointer(
                mData.mDisplay, None, mData.mXid,
                0, 0, 0, 0, static_cast<int>(x), static_cast<int>(y));
        }
    }

    void NkWindow::ShowMouse(bool show) {
        if (!mData.mDisplay || !mData.mXid) {
            return;
        }
        if (show) {
            XUndefineCursor(mData.mDisplay, mData.mXid);
        } else {
            static Cursor blankCursor = 0;
            if (!blankCursor) {
                XColor dummy = {};
                char data[1] = {};
                Pixmap blank = XCreateBitmapFromData(mData.mDisplay, mData.mXid, data, 1, 1);
                blankCursor = XCreatePixmapCursor(
                    mData.mDisplay, blank, blank, &dummy, &dummy, 0, 0);
                XFreePixmap(mData.mDisplay, blank);
            }
            XDefineCursor(mData.mDisplay, mData.mXid, blankCursor);
        }
        XFlush(mData.mDisplay);
    }

    void NkWindow::CaptureMouse(bool) {}

    // =============================================================================
    // Web / extras (desktop no-op)
    // =============================================================================

    void NkWindow::SetWebInputOptions(const NkWebInputOptions&) {}
    NkWebInputOptions NkWindow::GetWebInputOptions() const { return {}; }
    void NkWindow::SetProgress(float) {}
    NkSafeAreaInsets NkWindow::GetSafeAreaInsets() const { return {}; }

    // =============================================================================
    // Surface
    // =============================================================================

    NkSurfaceDesc NkWindow::GetSurfaceDesc() const {
        NkSurfaceDesc sd;
        auto sz         = GetSize();
        sd.width        = sz.x;
        sd.height       = sz.y;
        sd.dpi          = GetDpiScale();
        sd.display      = mData.mDisplay;
        sd.window       = mData.mXid;
        sd.screen       = mData.mScreen;
        sd.appliedHints = mData.mAppliedHints;   // ← ajout
        return sd;
    }

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_XLIB
