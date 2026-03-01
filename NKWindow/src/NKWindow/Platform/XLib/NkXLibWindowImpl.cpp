// =============================================================================
// NkXLibWindowImpl.cpp  —  Fenêtre XLib
//
// Registre : NkXLibWindowRegistry (::Window → NkXLibWindowImpl*)
// EventImpl : NkGetEventImpl()->Initialize/Shutdown avec window id encodé void*
// =============================================================================

#include "NkXLibWindowImpl.h"
#include "NKWindow/Core/NkSystem.h"
#include <X11/Xatom.h>
#include <cstring>

namespace nkentseu {

// Définition du display global (sera initialisé dans NkXLib.cpp)
Display *nk_xlib_global_display = nullptr;

// ---------------------------------------------------------------------------
// Destructeur
// ---------------------------------------------------------------------------

NkXLibWindowImpl::~NkXLibWindowImpl() {
    if (mData.isOpen)
        Close();
}

// ---------------------------------------------------------------------------
// Create
// ---------------------------------------------------------------------------

bool NkXLibWindowImpl::Create(const NkWindowConfig &config) {
    mConfig  = config;
    mBgColor = config.bgColor;
    mData.width   = config.width;
    mData.height  = config.height;
    mData.display = nk_xlib_global_display;

    if (!mData.display) {
        mLastError = NkError(1, "Display XLib non disponible");
        return false;
    }

    mData.screen = DefaultScreen(mData.display);

    NkU8 r = (mBgColor >> 24) & 0xFF;
    NkU8 g = (mBgColor >> 16) & 0xFF;
    NkU8 b = (mBgColor >>  8) & 0xFF;
    unsigned long bg = (static_cast<unsigned long>(r) << 16)
                     | (static_cast<unsigned long>(g) <<  8)
                     |  static_cast<unsigned long>(b);

    NkI32 wx = config.x, wy = config.y;
    if (config.centered) {
        wx = (DisplayWidth (mData.display, mData.screen) - static_cast<int>(config.width))  / 2;
        wy = (DisplayHeight(mData.display, mData.screen) - static_cast<int>(config.height)) / 2;
    }

    XSetWindowAttributes xa{};
    xa.background_pixel = bg;
    xa.event_mask = ExposureMask        | StructureNotifyMask |
                    KeyPressMask        | KeyReleaseMask       |
                    ButtonPressMask     | ButtonReleaseMask    |
                    PointerMotionMask   | FocusChangeMask;

    mData.window = XCreateWindow(
        mData.display,
        RootWindow(mData.display, mData.screen),
        wx, wy,
        config.width, config.height,
        0,
        DefaultDepth(mData.display, mData.screen),
        InputOutput,
        DefaultVisual(mData.display, mData.screen),
        CWBackPixel | CWEventMask, &xa);

    XStoreName(mData.display, mData.window, config.title.c_str());

    // Protocoles WM_DELETE_WINDOW
    mData.wmProtocols = XInternAtom(mData.display, "WM_PROTOCOLS",    False);
    mData.wmDelete    = XInternAtom(mData.display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(mData.display, mData.window, &mData.wmDelete, 1);

    // Contraintes de taille
    XSizeHints hints{};
    hints.flags      = PMinSize | PMaxSize;
    hints.min_width  = static_cast<int>(config.minWidth);
    hints.min_height = static_cast<int>(config.minHeight);
    hints.max_width  = static_cast<int>(config.maxWidth);
    hints.max_height = static_cast<int>(config.maxHeight);
    XSetWMNormalHints(mData.display, mData.window, &hints);

    // Contexte graphique
    mData.gc = XCreateGC(mData.display, mData.window, 0, nullptr);

    // Curseur invisible
    Pixmap bm = XCreateBitmapFromData(mData.display, mData.window, "\0", 1, 1);
    XColor blk{};
    mData.blankCursor = XCreatePixmapCursor(mData.display, bm, bm, &blk, &blk, 0, 0);
    XFreePixmap(mData.display, bm);

    if (config.visible)
        XMapWindow(mData.display, mData.window);
    XFlush(mData.display);
    mData.isOpen = true;

    // Enregistre dans le registre global
    gXLibWindowRegistry().Insert(mData.window, this);

    // Enregistre dans l'EventImpl (window id encodé comme void*)
    if (auto *ev = NkGetEventImpl())
        ev->Initialize(this, EncodeWindow(mData.window));

    return true;
}

// ---------------------------------------------------------------------------
// Close
// ---------------------------------------------------------------------------

void NkXLibWindowImpl::Close() {
    if (!mData.isOpen)
        return;

    // Désenregistre de l'EventImpl
    if (auto *ev = NkGetEventImpl())
        ev->Shutdown(EncodeWindow(mData.window));

    // Désenregistre du registre global
    gXLibWindowRegistry().Remove(mData.window);

    if (mData.blankCursor) {
        XFreeCursor(mData.display, mData.blankCursor);
        mData.blankCursor = 0;
    }
    if (mData.gc) {
        XFreeGC(mData.display, mData.gc);
        mData.gc = nullptr;
    }
    if (mData.window) {
        XDestroyWindow(mData.display, mData.window);
        mData.window = 0;
    }
    XFlush(mData.display);
    mData.isOpen = false;
}

// ---------------------------------------------------------------------------
// Propriétés
// ---------------------------------------------------------------------------

bool NkXLibWindowImpl::IsOpen() const {
    return mData.isOpen;
}

NkError NkXLibWindowImpl::GetLastError() const {
    return mLastError;
}

NkU32 NkXLibWindowImpl::GetBackgroundColor() const {
    return mBgColor;
}

void NkXLibWindowImpl::SetBackgroundColor(NkU32 c) {
    mBgColor = c;
}

std::string NkXLibWindowImpl::GetTitle() const {
    char *n = nullptr;
    XFetchName(mData.display, mData.window, &n);
    std::string s = n ? n : "";
    if (n) XFree(n);
    return s;
}

void NkXLibWindowImpl::SetTitle(const std::string &t) {
    mConfig.title = t;
    XStoreName(mData.display, mData.window, t.c_str());
    XFlush(mData.display);
}

NkVec2u NkXLibWindowImpl::GetSize() const {
    if (!mData.display || mData.window == 0)
        return {mData.width, mData.height};
    ::Window root;
    int x = 0, y = 0;
    unsigned int w = 0, h = 0, bw = 0, d = 0;
    if (XGetGeometry(mData.display, mData.window, &root, &x, &y, &w, &h, &bw, &d) && w > 0 && h > 0)
        return {static_cast<NkU32>(w), static_cast<NkU32>(h)};
    return {mData.width, mData.height};
}

NkVec2u NkXLibWindowImpl::GetDisplaySize() const {
    return {
        static_cast<NkU32>(DisplayWidth (mData.display, mData.screen)),
        static_cast<NkU32>(DisplayHeight(mData.display, mData.screen))
    };
}

NkVec2u NkXLibWindowImpl::GetPosition() const {
    ::Window root, child;
    int x = 0, y = 0, rx = 0, ry = 0;
    unsigned w, h, bw, d;
    XGetGeometry(mData.display, mData.window, &root, &x, &y, &w, &h, &bw, &d);
    XTranslateCoordinates(mData.display, mData.window, root, 0, 0, &rx, &ry, &child);
    return {static_cast<NkU32>(rx), static_cast<NkU32>(ry)};
}

// ---------------------------------------------------------------------------
// Contrôles
// ---------------------------------------------------------------------------

void NkXLibWindowImpl::SetSize(NkU32 w, NkU32 h) {
    mData.width  = w;
    mData.height = h;
    XResizeWindow(mData.display, mData.window, w, h);
    XFlush(mData.display);
}

void NkXLibWindowImpl::SetPosition(NkI32 x, NkI32 y) {
    XMoveWindow(mData.display, mData.window, x, y);
    XFlush(mData.display);
}

void NkXLibWindowImpl::SetVisible(bool v) {
    v ? XMapWindow(mData.display, mData.window)
      : XUnmapWindow(mData.display, mData.window);
    XFlush(mData.display);
}

void NkXLibWindowImpl::Minimize() {
    XIconifyWindow(mData.display, mData.window, mData.screen);
    XFlush(mData.display);
}

void NkXLibWindowImpl::Maximize() {
    XMapWindow(mData.display, mData.window);
    XFlush(mData.display);
}

void NkXLibWindowImpl::Restore() {
    XMapWindow(mData.display, mData.window);
    XFlush(mData.display);
}

void NkXLibWindowImpl::SetFullscreen(bool fs) {
    Atom st = XInternAtom(mData.display, "_NET_WM_STATE",            False);
    Atom fa = XInternAtom(mData.display, "_NET_WM_STATE_FULLSCREEN", False);
    XEvent ev{};
    ev.type                 = ClientMessage;
    ev.xclient.window       = mData.window;
    ev.xclient.message_type = st;
    ev.xclient.format       = 32;
    ev.xclient.data.l[0]    = fs ? 1 : 0;
    ev.xclient.data.l[1]    = static_cast<long>(fa);
    XSendEvent(mData.display, DefaultRootWindow(mData.display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XFlush(mData.display);
    mConfig.fullscreen = fs;
}

void NkXLibWindowImpl::SetMousePosition(NkU32 x, NkU32 y) {
    XWarpPointer(mData.display, None, mData.window, 0, 0, 0, 0,
                 static_cast<int>(x), static_cast<int>(y));
    XFlush(mData.display);
}

void NkXLibWindowImpl::ShowMouse(bool show) {
    XDefineCursor(mData.display, mData.window, show ? None : mData.blankCursor);
    XFlush(mData.display);
}

void NkXLibWindowImpl::CaptureMouse(bool cap) {
    if (cap)
        XGrabPointer(mData.display, mData.window, True,
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                     GrabModeAsync, GrabModeAsync, mData.window, None, CurrentTime);
    else
        XUngrabPointer(mData.display, CurrentTime);
    XFlush(mData.display);
}

NkSurfaceDesc NkXLibWindowImpl::GetSurfaceDesc() const {
    NkSurfaceDesc sd;
    sd.width   = mData.width;
    sd.height  = mData.height;
    sd.display = mData.display;
    sd.window  = mData.window;
    return sd;
}

} // namespace nkentseu