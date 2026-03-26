// =============================================================================
// NkXLibWindow.cpp - implementation XLib de NkWindow (sans PIMPL)
//
// Pattern identique a Win32/XCB:
// - donnees natives dans NkWindow::mData
// - Display global partage entre fenetres
// - registre backend xid -> NkWindow* expose via helpers
// - Synchronisation complète entre mData et mConfig (v2)
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_XLIB)

#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKEvent/NkEventSystem.h"
#include "NKWindow/Platform/XLib/NkXLibWindow.h"
#include "NKWindow/Platform/XLib/NkXLibDropTarget.h"
#include "NKCore/NkAtomic.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <unordered_map>

namespace nkentseu {
    using namespace math;

    // =============================================================================
    // Globals partages (backend registry)
    // =============================================================================

    static Display*   sDisplay        = nullptr;
    static bool       sDisplayOwned   = false;
    static int        sWindowCount    = 0;
    static NkSpinLock sDisplayMutex;
    static NkWindow*  sXLibLastWindow = nullptr;

    // Function-local static avoids static init order fiasco with NkAllocator.
    static NkUnorderedMap<::Window, NkWindow*>& XLibWindowMap() {
        static NkUnorderedMap<::Window, NkWindow*> sMap;
        if (sMap.BucketCount() == 0) {
            sMap.Rehash(32);
        }
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
    // Fonctions de synchronisation mData ↔ mConfig
    // =============================================================================

    static void SyncConfigFromWindow(Display* display, ::Window xid, NkWindowConfig& config, const NkWindowData& data) {
        if (!display || !xid) return;

        // Récupérer la position
        ::Window child;
        int x = 0, y = 0;
        if (XTranslateCoordinates(display, xid, DefaultRootWindow(display), 0, 0, &x, &y, &child)) {
            config.x = x;
            config.y = y;
        }

        // Récupérer la taille
        XWindowAttributes attrs;
        if (XGetWindowAttributes(display, xid, &attrs)) {
            config.width = attrs.width;
            config.height = attrs.height;
        }

        // Récupérer le titre
        char* title = nullptr;
        if (XFetchName(display, xid, &title) && title) {
            config.title = title;
            XFree(title);
        }

        // Visibilité (approximative)
        XWindowAttributes rootAttrs;
        if (XGetWindowAttributes(display, xid, &rootAttrs)) {
            config.visible = (rootAttrs.map_state == IsViewable);
        }

        // État plein écran (via _NET_WM_STATE)
        Atom wmState = XInternAtom(display, "_NET_WM_STATE", True);
        if (wmState != None) {
            Atom actualType;
            int actualFormat;
            unsigned long numItems, bytesAfter;
            unsigned char* data = nullptr;
            
            if (XGetWindowProperty(display, xid, wmState, 0, 1024, False, XA_ATOM,
                                   &actualType, &actualFormat, &numItems, &bytesAfter, &data) == Success) {
                if (actualType == XA_ATOM && actualFormat == 32) {
                    Atom* atoms = reinterpret_cast<Atom*>(data);
                    Atom wmFs = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", True);
                    config.fullscreen = false;
                    
                    for (unsigned long i = 0; i < numItems; ++i) {
                        if (atoms[i] == wmFs) {
                            config.fullscreen = true;
                            break;
                        }
                    }
                }
                XFree(data);
            }
        }
    }

    static void SyncWindowFromConfig(Display* display, ::Window xid, const NkWindowConfig& config) {
        if (!display || !xid) return;

        // Titre
        XStoreName(display, xid, config.title.CStr());

        // Taille
        XWindowAttributes currentAttrs;
        if (XGetWindowAttributes(display, xid, &currentAttrs)) {
            if (currentAttrs.width != (int)config.width || currentAttrs.height != (int)config.height) {
                XResizeWindow(display, xid, config.width, config.height);
            }
        }

        // Position
        ::Window child;
        int currentX = 0, currentY = 0;
        if (XTranslateCoordinates(display, xid, DefaultRootWindow(display), 0, 0, &currentX, &currentY, &child)) {
            if (currentX != config.x || currentY != config.y) {
                XMoveWindow(display, xid, config.x, config.y);
            }
        }

        // Visibilité
        XWindowAttributes rootAttrs;
        if (XGetWindowAttributes(display, xid, &rootAttrs)) {
            bool isVisible = (rootAttrs.map_state == IsViewable);
            if (isVisible != config.visible) {
                if (config.visible) {
                    XMapWindow(display, xid);
                } else {
                    XUnmapWindow(display, xid);
                }
            }
        }

        XFlush(display);
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

        NkScopedSpinLock lock(sDisplayMutex);

        if (!sDisplay) {
            if (config.native.externalDisplayHandle != 0) {
                sDisplay = reinterpret_cast<Display*>(config.native.externalDisplayHandle);
                sDisplayOwned = false;
            } else {
                sDisplay = XOpenDisplay(nullptr);
                sDisplayOwned = true;
            }
            if (!sDisplay) {
                mLastError = NkError(1, "XOpenDisplay failed");
                return false;
            }
        }
        ++sWindowCount;

        mData.mDisplay = sDisplay;
        mData.mScreen  = DefaultScreen(sDisplay);
        mData.mAppliedHints = config.surfaceHints;
        mData.mParentXid = static_cast<::Window>(config.native.parentWindowHandle);

        const bool useExternal = config.native.useExternalWindow &&
                                 config.native.externalWindowHandle != 0;
        if (useExternal) {
            mData.mXid = static_cast<::Window>(config.native.externalWindowHandle);
            mData.mExternal = true;
            XWindowAttributes attrs = {};
            if (XGetWindowAttributes(sDisplay, mData.mXid, &attrs) == 0) {
                --sWindowCount;
                if (sWindowCount <= 0) {
                    if (sDisplayOwned) XCloseDisplay(sDisplay);
                    sDisplay = nullptr;
                    sDisplayOwned = false;
                    sWindowCount = 0;
                }
                mLastError = NkError(2, "Invalid external X11 window.");
                return false;
            }
            NkXLibRegisterWindow(mData.mXid, this);
            mId = NkSystem::Instance().RegisterWindow(this);
            
            // Synchroniser mConfig depuis l'état réel de la fenêtre externe
            SyncConfigFromWindow(sDisplay, mData.mXid, mConfig, mData);
            
            mIsOpen = true;
            return true;
        }

        int x = config.centered
            ? (DisplayWidth(sDisplay, mData.mScreen) - static_cast<int>(config.width)) / 2
            : config.x;
        int y = config.centered
            ? (DisplayHeight(sDisplay, mData.mScreen) - static_cast<int>(config.height)) / 2
            : config.y;

        // Mettre à jour mConfig avec les coordonnées calculées si centré
        if (config.centered) {
            mConfig.x = x;
            mConfig.y = y;
        }

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
            sDisplay, mData.mParentXid ? mData.mParentXid : DefaultRootWindow(sDisplay),
            x, y, config.width, config.height,
            0,
            depth,       // ← depth éventuellement issu du hint
            InputOutput,
            visual,      // ← visual éventuellement issue du hint
            vmask,
            &attrs);

        if (!mData.mXid) {
            mLastError = NkError(2, "XCreateWindow failed");
            --sWindowCount;
            if (sWindowCount <= 0) {
                if (sDisplayOwned) XCloseDisplay(sDisplay);
                sDisplay = nullptr;
                sDisplayOwned = false;
                sWindowCount = 0;
            }
            return false;
        }

        // WM_DELETE_WINDOW
        mData.mWmDeleteWindow = XInternAtom(sDisplay, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(sDisplay, mData.mXid, &mData.mWmDeleteWindow, 1);

        if (config.native.utilityWindow) {
            Atom wmWindowType = XInternAtom(sDisplay, "_NET_WM_WINDOW_TYPE", False);
            Atom wmWindowTypeUtility = XInternAtom(sDisplay, "_NET_WM_WINDOW_TYPE_UTILITY", False);
            XChangeProperty(
                sDisplay, mData.mXid, wmWindowType, XA_ATOM, 32, PropModeReplace,
                reinterpret_cast<unsigned char*>(&wmWindowTypeUtility), 1);
        }

        if (config.transparent) {
            Atom opacityAtom = XInternAtom(sDisplay, "_NET_WM_WINDOW_OPACITY", False);
            unsigned long alpha = static_cast<unsigned long>(config.bgColor & 0xFFu);
            if (alpha == 0xFFu) alpha = 230u;
            unsigned long opacity = (alpha * 0xFFFFFFFFul) / 255ul;
            XChangeProperty(
                sDisplay, mData.mXid, opacityAtom, XA_CARDINAL, 32, PropModeReplace,
                reinterpret_cast<unsigned char*>(&opacity), 1);
        }

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
            mData.mDropTarget->SetDropEnterCallback(NkXLibDropTarget::DropEnterCallback([this](const NkDropEnterEvent& ev) {
                NkDropEnterEvent copy(ev);
                NkSystem::Events().Enqueue_Public(copy, mId);
            }));
            mData.mDropTarget->SetDropLeaveCallback(NkXLibDropTarget::DropLeaveCallback([this](const NkDropLeaveEvent& ev) {
                NkDropLeaveEvent copy(ev);
                NkSystem::Events().Enqueue_Public(copy, mId);
            }));
            mData.mDropTarget->SetDropFileCallback(NkXLibDropTarget::DropFileCallback([this](const NkDropFileEvent& ev) {
                NkDropFileEvent copy(ev);
                NkSystem::Events().Enqueue_Public(copy, mId);
            }));
            mData.mDropTarget->SetDropTextCallback(NkXLibDropTarget::DropTextCallback([this](const NkDropTextEvent& ev) {
                NkDropTextEvent copy(ev);
                NkSystem::Events().Enqueue_Public(copy, mId);
            }));
        }

        // Synchronisation initiale : mConfig reflète l'état réel
        SyncConfigFromWindow(sDisplay, mData.mXid, mConfig, mData);

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
            if (!mData.mExternal) {
                XDestroyWindow(mData.mDisplay, mData.mXid);
                XFlush(mData.mDisplay);
            }
            mData.mXid = 0;
        }

        {
            NkScopedSpinLock lock(sDisplayMutex);
            --sWindowCount;
            if (sWindowCount <= 0 && sDisplay) {
                if (sDisplayOwned) XCloseDisplay(sDisplay);
                sDisplay = nullptr;
                sDisplayOwned = false;
                sWindowCount = 0;
            }
        }
        mData.mDisplay = nullptr;
        mData.mExternal = false;
    }

    // =============================================================================
    // Queries
    // =============================================================================

    bool NkWindow::IsOpen() const { return mIsOpen; }
    bool NkWindow::IsValid() const { return mIsOpen && mData.mXid != 0; }

    NkError NkWindow::GetLastError() const { return mLastError; }

    NkWindowConfig NkWindow::GetConfig() const { 
        // Synchroniser avant de retourner
        if (mIsOpen && mData.mDisplay && mData.mXid) {
            // Appel à la fonction libre, pas à une méthode de classe
            SyncConfigFromWindow(mData.mDisplay, mData.mXid, const_cast<NkWindow*>(this)->mConfig, mData);
        }
        return mConfig; 
    }

    NkString NkWindow::GetTitle() const {
        if (!mData.mDisplay || !mData.mXid) {
            return mConfig.title;
        }
        char* title = nullptr;
        if (XFetchName(mData.mDisplay, mData.mXid, &title) && title) {
            NkString result = title;
            XFree(title);
            
            // Synchroniser mConfig
            const_cast<NkWindow*>(this)->mConfig.title = result;
            
            return result;
        }
        return mConfig.title;
    }

    NkVec2u NkWindow::GetSize() const {
        if (!mData.mDisplay || !mData.mXid) {
            return { mConfig.width, mConfig.height };
        }
        XWindowAttributes a;
        XGetWindowAttributes(mData.mDisplay, mData.mXid, &a);
        NkVec2u size = { static_cast<uint32>(a.width), static_cast<uint32>(a.height) };
        
        // Synchroniser mConfig
        const_cast<NkWindow*>(this)->mConfig.width = size.x;
        const_cast<NkWindow*>(this)->mConfig.height = size.y;
        
        return size;
    }

    NkVec2u NkWindow::GetPosition() const {
        if (!mData.mDisplay || !mData.mXid) {
            return { static_cast<uint32>(mConfig.x), static_cast<uint32>(mConfig.y) };
        }
        ::Window child;
        int x = 0, y = 0;
        XTranslateCoordinates(
            mData.mDisplay, mData.mXid,
            DefaultRootWindow(mData.mDisplay),
            0, 0, &x, &y, &child);
        NkVec2u pos = { static_cast<uint32>(x), static_cast<uint32>(y) };
        
        // Synchroniser mConfig
        const_cast<NkWindow*>(this)->mConfig.x = pos.x;
        const_cast<NkWindow*>(this)->mConfig.y = pos.y;
        
        return pos;
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
        mConfig.visible = visible;
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
        // L'état de visibilité change
        mConfig.visible = false;
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
        
        // Mettre à jour la taille après maximisation
        XWindowAttributes attrs;
        XGetWindowAttributes(mData.mDisplay, mData.mXid, &attrs);
        mConfig.width = attrs.width;
        mConfig.height = attrs.height;
        mConfig.visible = true;
    }

    void NkWindow::Restore() {
        if (mData.mDisplay && mData.mXid) {
            XMapWindow(mData.mDisplay, mData.mXid);
            XFlush(mData.mDisplay);
        }
        // Mettre à jour après restauration
        XWindowAttributes attrs;
        XGetWindowAttributes(mData.mDisplay, mData.mXid, &attrs);
        mConfig.width = attrs.width;
        mConfig.height = attrs.height;
        mConfig.visible = true;
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
        
        // Mettre à jour la taille
        if (fullscreen) {
            mConfig.width = DisplayWidth(mData.mDisplay, mData.mScreen);
            mConfig.height = DisplayHeight(mData.mDisplay, mData.mScreen);
        } else {
            XWindowAttributes attrs;
            XGetWindowAttributes(mData.mDisplay, mData.mXid, &attrs);
            mConfig.width = attrs.width;
            mConfig.height = attrs.height;
        }
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
        auto sz         = GetSize();  // GetSize synchronise déjà mConfig
        sd.width        = sz.x;
        sd.height       = sz.y;
        sd.dpi          = GetDpiScale();
        sd.display      = mData.mDisplay;
        sd.window       = mData.mXid;
        sd.screen       = mData.mScreen;
        sd.appliedHints = mData.mAppliedHints;
        return sd;
    }

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_XLIB