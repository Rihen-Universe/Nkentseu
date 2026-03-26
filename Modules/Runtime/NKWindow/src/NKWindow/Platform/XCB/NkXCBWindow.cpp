// =============================================================================
// NkXCBWindow.cpp — XCB implementation of NkWindow (sans PIMPL)
//
// Pattern identique à NkWin32Window.cpp / NkXLibWindow.cpp :
//   - NkWindow porte ses données natives dans mData (NkXCBWindow.h).
//   - Connexion xcb_connection_t partagée entre toutes les fenêtres.
//   - Registre xcb_window_t → NkWindow* (statique, invisible à l'extérieur).
//   - Fonctions d'accès NkXCBFindWindow / NkXCBRegisterWindow /
//     NkXCBUnregisterWindow pour NkXCBEventSystem.cpp.
//   - Synchronisation complète entre mData et mConfig (v2)
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_XCB)

#include "NKWindow/Platform/XCB/NkXCBWindow.h"
#include "NKWindow/Platform/XCB/NkXCBDropTarget.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKEvent/NkEventSystem.h"
#include "NKCore/NkAtomic.h"

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_icccm.h>

#include <unordered_map>
#include <cstring>
#include <string>

namespace nkentseu {
    using namespace math;

    // =============================================================================
    // Globals partagés
    // =============================================================================

    static xcb_connection_t* sConnection    = nullptr;
    static xcb_screen_t*     sDefaultScreen = nullptr;
    static bool              sConnectionOwned = false;
    static int               sWindowCount   = 0;
    static NkSpinLock        sConnectionMutex;

    // Atoms partagés
    static xcb_atom_t sAtomWmDeleteWindow = XCB_ATOM_NONE;
    static xcb_atom_t sAtomWmProtocols    = XCB_ATOM_NONE;
    static xcb_atom_t sAtomNetWmName      = XCB_ATOM_NONE;
    static xcb_atom_t sAtomUtf8String     = XCB_ATOM_NONE;

    // Function-local static avoids static init order fiasco with NkAllocator.
    static NkUnorderedMap<xcb_window_t, NkWindow*>& XCBWindowMap() {
        static NkUnorderedMap<xcb_window_t, NkWindow*> sMap;
        if (sMap.BucketCount() == 0) {
            sMap.Rehash(32);
        }
        return sMap;
    }

    // =============================================================================
    // Registre accessor functions (déclarées dans NkXCBWindow.h)
    // =============================================================================

    NkWindow* NkXCBFindWindow(xcb_window_t xid) {
        auto* win = XCBWindowMap().Find(xid);
        return win ? *win : nullptr;
    }

    void NkXCBRegisterWindow(xcb_window_t xid, NkWindow* win) {
        XCBWindowMap()[xid] = win;
    }

    void NkXCBUnregisterWindow(xcb_window_t xid) {
        XCBWindowMap().Erase(xid);
    }

    // Accesseur connexion pour NkXCBEventSystem.cpp
    xcb_connection_t* NkXCBGetConnection() { return sConnection; }
    xcb_screen_t*     NkXCBGetScreen()     { return sDefaultScreen; }
    xcb_atom_t NkXCBGetWmDeleteWindowAtom() { return sAtomWmDeleteWindow; }
    xcb_atom_t NkXCBGetWmProtocolsAtom()    { return sAtomWmProtocols; }

    // =============================================================================
    // Helper : intern atom
    // =============================================================================

    static xcb_atom_t NkXCBInternAtom(xcb_connection_t* c, const char* name, bool onlyIfExists = false) {
        xcb_intern_atom_cookie_t cookie = xcb_intern_atom(c, onlyIfExists ? 1 : 0,
                                                        static_cast<uint16_t>(strlen(name)), name);
        xcb_intern_atom_reply_t* reply  = xcb_intern_atom_reply(c, cookie, nullptr);
        xcb_atom_t atom = reply ? reply->atom : XCB_ATOM_NONE;
        free(reply);
        return atom;
    }

    // =============================================================================
    // Fonctions de synchronisation mData ↔ mConfig
    // =============================================================================

    static void SyncConfigFromWindow(xcb_connection_t* conn, xcb_window_t window, 
                                     NkWindowConfig& config, const NkWindowData& data) {
        if (!conn || !window) return;

        // Récupérer la géométrie (position et taille)
        xcb_get_geometry_cookie_t geomCookie = xcb_get_geometry(conn, window);
        xcb_get_geometry_reply_t* geomReply = xcb_get_geometry_reply(conn, geomCookie, nullptr);
        if (geomReply) {
            config.width = geomReply->width;
            config.height = geomReply->height;
            free(geomReply);
        }

        // Récupérer la position relative à la racine
        xcb_translate_coordinates_cookie_t transCookie = 
            xcb_translate_coordinates(conn, window, sDefaultScreen->root, 0, 0);
        xcb_translate_coordinates_reply_t* transReply = 
            xcb_translate_coordinates_reply(conn, transCookie, nullptr);
        if (transReply) {
            config.x = transReply->dst_x;
            config.y = transReply->dst_y;
            free(transReply);
        }

        // Récupérer le titre (WM_NAME)
        xcb_icccm_get_wm_name_reply_t nameReply;
        xcb_icccm_get_wm_name_cookie_t nameCookie = xcb_icccm_get_wm_name(conn, window);
        if (xcb_icccm_get_wm_name_reply(conn, nameCookie, &nameReply, nullptr)) {
            config.title = NkString((const char*)nameReply.name, nameReply.name_len);
            xcb_icccm_get_wm_name_reply_wipe(&nameReply);
        }

        // Récupérer l'état de visibilité (mapped)
        xcb_get_window_attributes_cookie_t attrCookie = xcb_get_window_attributes(conn, window);
        xcb_get_window_attributes_reply_t* attrReply = 
            xcb_get_window_attributes_reply(conn, attrCookie, nullptr);
        if (attrReply) {
            config.visible = (attrReply->map_state == XCB_MAP_STATE_VIEWABLE);
            free(attrReply);
        }

        // Récupérer l'état plein écran via _NET_WM_STATE
        xcb_atom_t wmStateAtom = NkXCBInternAtom(conn, "_NET_WM_STATE", true);
        if (wmStateAtom != XCB_ATOM_NONE) {
            xcb_get_property_cookie_t propCookie = xcb_get_property(conn, 0, window, wmStateAtom,
                                                                   XCB_ATOM_ATOM, 0, 1024);
            xcb_get_property_reply_t* propReply = xcb_get_property_reply(conn, propCookie, nullptr);
            if (propReply && propReply->type == XCB_ATOM_ATOM && propReply->format == 32) {
                xcb_atom_t* atoms = (xcb_atom_t*)xcb_get_property_value(propReply);
                xcb_atom_t wmFsAtom = NkXCBInternAtom(conn, "_NET_WM_STATE_FULLSCREEN", true);
                config.fullscreen = false;
                if (wmFsAtom != XCB_ATOM_NONE) {
                    int numAtoms = propReply->length / (propReply->format / 8);
                    for (int i = 0; i < numAtoms; ++i) {
                        if (atoms[i] == wmFsAtom) {
                            config.fullscreen = true;
                            break;
                        }
                    }
                }
            }
            free(propReply);
        }
    }

    static void SyncWindowFromConfig(xcb_connection_t* conn, xcb_window_t window, 
                                     const NkWindowConfig& config) {
        if (!conn || !window) return;

        // Titre
        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME,
                            XCB_ATOM_STRING, 8,
                            static_cast<uint32_t>(config.title.Size()), config.title.CStr());
        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, window, sAtomNetWmName,
                            sAtomUtf8String, 8,
                            static_cast<uint32_t>(config.title.Size()), config.title.CStr());

        // Taille
        xcb_get_geometry_cookie_t geomCookie = xcb_get_geometry(conn, window);
        xcb_get_geometry_reply_t* geomReply = xcb_get_geometry_reply(conn, geomCookie, nullptr);
        if (geomReply) {
            if (geomReply->width != config.width || geomReply->height != config.height) {
                uint32_t values[] = { config.width, config.height };
                xcb_configure_window(conn, window,
                                    XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                                    values);
            }
            free(geomReply);
        }

        // Position
        xcb_translate_coordinates_cookie_t transCookie = 
            xcb_translate_coordinates(conn, window, sDefaultScreen->root, 0, 0);
        xcb_translate_coordinates_reply_t* transReply = 
            xcb_translate_coordinates_reply(conn, transCookie, nullptr);
        if (transReply) {
            if (transReply->dst_x != config.x || transReply->dst_y != config.y) {
                uint32_t values[] = { (uint32_t)config.x, (uint32_t)config.y };
                xcb_configure_window(conn, window,
                                    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                                    values);
            }
            free(transReply);
        }

        // Visibilité
        xcb_get_window_attributes_cookie_t attrCookie = xcb_get_window_attributes(conn, window);
        xcb_get_window_attributes_reply_t* attrReply = 
            xcb_get_window_attributes_reply(conn, attrCookie, nullptr);
        if (attrReply) {
            bool isVisible = (attrReply->map_state == XCB_MAP_STATE_VIEWABLE);
            if (isVisible != config.visible) {
                if (config.visible) {
                    xcb_map_window(conn, window);
                } else {
                    xcb_unmap_window(conn, window);
                }
            }
            free(attrReply);
        }

        xcb_flush(conn);
    }

    // =============================================================================
    // Constructeurs / Destructeur
    // =============================================================================

    NkWindow::NkWindow()                        = default;
    NkWindow::NkWindow(const NkWindowConfig& c) { Create(c); }
    NkWindow::~NkWindow()                       { if (mIsOpen) Close(); }

    // =============================================================================
    // Create
    // =============================================================================

    bool NkWindow::Create(const NkWindowConfig& config) {
        mConfig = config;

        NkScopedSpinLock lock(sConnectionMutex);

        // Ouvrir la connexion xcb si nécessaire
        if (!sConnection) {
            if (config.native.externalDisplayHandle != 0) {
                sConnection = reinterpret_cast<xcb_connection_t*>(config.native.externalDisplayHandle);
                sConnectionOwned = false;
                if (!sConnection || xcb_connection_has_error(sConnection)) {
                    mLastError = NkError(1, "Invalid external xcb_connection_t.");
                    sConnection = nullptr;
                    return false;
                }
                const xcb_setup_t* setup = xcb_get_setup(sConnection);
                xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
                sDefaultScreen = iter.data;
            } else {
                int screenNum = 0;
                sConnection = xcb_connect(nullptr, &screenNum);
                sConnectionOwned = true;
                if (!sConnection || xcb_connection_has_error(sConnection)) {
                    mLastError = NkError(1, "xcb_connect failed");
                    sConnection = nullptr;
                    sConnectionOwned = false;
                    return false;
                }

                const xcb_setup_t* setup = xcb_get_setup(sConnection);
                xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
                for (int i = 0; i < screenNum; ++i)
                    xcb_screen_next(&iter);
                sDefaultScreen = iter.data;
            }

            // Intern atoms utiles
            sAtomWmDeleteWindow = NkXCBInternAtom(sConnection, "WM_DELETE_WINDOW");
            sAtomWmProtocols    = NkXCBInternAtom(sConnection, "WM_PROTOCOLS");
            sAtomNetWmName      = NkXCBInternAtom(sConnection, "_NET_WM_NAME");
            sAtomUtf8String     = NkXCBInternAtom(sConnection, "UTF8_STRING");
        }
        ++sWindowCount;

        mData.mConnection = sConnection;
        mData.mScreen     = sDefaultScreen;
        mData.mAppliedHints = config.surfaceHints;
        mData.mParentWindow = static_cast<xcb_window_t>(config.native.parentWindowHandle);

        const bool useExternal = config.native.useExternalWindow &&
                                 config.native.externalWindowHandle != 0;
        if (useExternal) {
            mData.mWindow = static_cast<xcb_window_t>(config.native.externalWindowHandle);
            mData.mExternal = true;

            xcb_get_geometry_cookie_t c = xcb_get_geometry(mData.mConnection, mData.mWindow);
            xcb_generic_error_t* err = nullptr;
            xcb_get_geometry_reply_t* r = xcb_get_geometry_reply(mData.mConnection, c, &err);
            if (!r || err) {
                free(r);
                free(err);
                --sWindowCount;
                if (sWindowCount <= 0) {
                    if (sConnectionOwned) xcb_disconnect(sConnection);
                    sConnection = nullptr;
                    sConnectionOwned = false;
                    sDefaultScreen = nullptr;
                    sWindowCount = 0;
                }
                mLastError = NkError(2, "Invalid external XCB window.");
                return false;
            }
            free(r);

            NkXCBRegisterWindow(mData.mWindow, this);
            mId = NkSystem::Instance().RegisterWindow(this);

            // Synchroniser mConfig depuis l'état réel de la fenêtre externe
            SyncConfigFromWindow(sConnection, mData.mWindow, mConfig, mData);

            mIsOpen = true;
            return true;
        }

        // --- Position ---
        int x = config.centered
            ? ((int)sDefaultScreen->width_in_pixels  - (int)config.width)  / 2
            : config.x;
        int y = config.centered
            ? ((int)sDefaultScreen->height_in_pixels - (int)config.height) / 2
            : config.y;

        // Mettre à jour mConfig avec les coordonnées calculées si centré
        if (config.centered) {
            mConfig.x = x;
            mConfig.y = y;
        }

        // --- Créer le colormap ---
        mData.mColormap = xcb_generate_id(sConnection);
        xcb_create_colormap(sConnection, XCB_COLORMAP_ALLOC_NONE,
                            mData.mColormap,
                            sDefaultScreen->root,
                            sDefaultScreen->root_visual);

        // --- Créer la fenêtre ---
        mData.mWindow = xcb_generate_id(sConnection);

        uint32_t eventMask =
            XCB_EVENT_MASK_EXPOSURE              |
            XCB_EVENT_MASK_STRUCTURE_NOTIFY      |
            XCB_EVENT_MASK_KEY_PRESS             |
            XCB_EVENT_MASK_KEY_RELEASE           |
            XCB_EVENT_MASK_BUTTON_PRESS          |
            XCB_EVENT_MASK_BUTTON_RELEASE        |
            XCB_EVENT_MASK_POINTER_MOTION        |
            XCB_EVENT_MASK_FOCUS_CHANGE          |
            XCB_EVENT_MASK_ENTER_WINDOW          |
            XCB_EVENT_MASK_LEAVE_WINDOW;

        uint32_t valueList[]  = { sDefaultScreen->black_pixel, eventMask, mData.mColormap };
        uint32_t valueMask    = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;

        xcb_create_window(
            sConnection,
            XCB_COPY_FROM_PARENT,
            mData.mWindow,
            mData.mParentWindow ? mData.mParentWindow : sDefaultScreen->root,
            static_cast<int16_t>(x), static_cast<int16_t>(y),
            static_cast<uint16_t>(config.width),
            static_cast<uint16_t>(config.height),
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            sDefaultScreen->root_visual,
            valueMask, valueList);

        // --- WM_DELETE_WINDOW ---
        xcb_change_property(sConnection, XCB_PROP_MODE_REPLACE,
                            mData.mWindow, sAtomWmProtocols,
                            XCB_ATOM_ATOM, 32,
                            1, &sAtomWmDeleteWindow);

        // --- Titre ---
        xcb_change_property(sConnection, XCB_PROP_MODE_REPLACE,
                            mData.mWindow, XCB_ATOM_WM_NAME,
                            XCB_ATOM_STRING, 8,
                            static_cast<uint32_t>(config.title.Size()),
                            config.title.CStr());
        xcb_change_property(sConnection, XCB_PROP_MODE_REPLACE,
                            mData.mWindow, sAtomNetWmName,
                            sAtomUtf8String, 8,
                            static_cast<uint32_t>(config.title.Size()),
                            config.title.CStr());

        if (config.native.utilityWindow) {
            const xcb_atom_t wmType = NkXCBInternAtom(sConnection, "_NET_WM_WINDOW_TYPE");
            const xcb_atom_t wmTypeUtility = NkXCBInternAtom(sConnection, "_NET_WM_WINDOW_TYPE_UTILITY");
            xcb_change_property(sConnection, XCB_PROP_MODE_REPLACE,
                                mData.mWindow, wmType, XCB_ATOM_ATOM, 32,
                                1, &wmTypeUtility);
        }

        if (config.transparent) {
            const xcb_atom_t opacityAtom = NkXCBInternAtom(sConnection, "_NET_WM_WINDOW_OPACITY");
            uint32_t alpha = config.bgColor & 0xFFu;
            if (alpha == 0xFFu) alpha = 230u;
            const uint32_t opacity = static_cast<uint32_t>((static_cast<uint64_t>(alpha) * 0xFFFFFFFFull) / 255ull);
            xcb_change_property(sConnection, XCB_PROP_MODE_REPLACE,
                                mData.mWindow, opacityAtom, XCB_ATOM_CARDINAL, 32,
                                1, &opacity);
        }

        // --- WM size hints (non-resizable) ---
        if (!config.resizable) {
            xcb_size_hints_t hints{};
            xcb_icccm_size_hints_set_min_size(&hints, (int)config.width, (int)config.height);
            xcb_icccm_size_hints_set_max_size(&hints, (int)config.width, (int)config.height);
            xcb_icccm_set_wm_normal_hints(sConnection, mData.mWindow, &hints);
        }

        NkXCBRegisterWindow(mData.mWindow, this);
        mId = NkSystem::Instance().RegisterWindow(this);

        // XDND drop target integration (events forwarded to NkEventSystem queue).
        mData.mDropTarget = new NkXCBDropTarget(mData.mConnection, mData.mWindow);
        if (mData.mDropTarget) {
            mData.mDropTarget->SetDropEnterCallback(NkXCBDropTarget::DropEnterCallback([this](const NkDropEnterEvent& ev) {
                NkDropEnterEvent copy(ev);
                NkSystem::Events().Enqueue_Public(copy, mId);
            }));
            mData.mDropTarget->SetDropLeaveCallback(NkXCBDropTarget::DropLeaveCallback([this](const NkDropLeaveEvent& ev) {
                NkDropLeaveEvent copy(ev);
                NkSystem::Events().Enqueue_Public(copy, mId);
            }));
            mData.mDropTarget->SetDropFileCallback(NkXCBDropTarget::DropFileCallback([this](const NkDropFileEvent& ev) {
                NkDropFileEvent copy(ev);
                NkSystem::Events().Enqueue_Public(copy, mId);
            }));
            mData.mDropTarget->SetDropTextCallback(NkXCBDropTarget::DropTextCallback([this](const NkDropTextEvent& ev) {
                NkDropTextEvent copy(ev);
                NkSystem::Events().Enqueue_Public(copy, mId);
            }));
        }

        if (config.visible) {
            xcb_map_window(sConnection, mData.mWindow);
            xcb_flush(sConnection);
        }

        // Synchronisation initiale : mConfig reflète l'état réel
        SyncConfigFromWindow(sConnection, mData.mWindow, mConfig, mData);

        mIsOpen = true;
        return true;
    }

    // =============================================================================
    // Close
    // =============================================================================

    void NkWindow::Close() {
        if (!mIsOpen) return;
        mIsOpen = false;

        NkXCBUnregisterWindow(mData.mWindow);
        NkSystem::Instance().UnregisterWindow(mId);
        mId = NK_INVALID_WINDOW_ID;

        if (mData.mDropTarget) {
            delete mData.mDropTarget;
            mData.mDropTarget = nullptr;
        }

        if (mData.mWindow && mData.mConnection) {
            if (!mData.mExternal) {
                xcb_destroy_window(mData.mConnection, mData.mWindow);
                xcb_flush(mData.mConnection);
            }
            mData.mWindow = 0;
        }
        if (mData.mColormap && mData.mConnection) {
            xcb_free_colormap(mData.mConnection, mData.mColormap);
            mData.mColormap = 0;
        }

        {
            NkScopedSpinLock lock(sConnectionMutex);
            --sWindowCount;
            if (sWindowCount <= 0 && sConnection) {
                if (sConnectionOwned) xcb_disconnect(sConnection);
                sConnection    = nullptr;
                sConnectionOwned = false;
                sDefaultScreen = nullptr;
                sWindowCount   = 0;
            }
        }
        mData.mConnection = nullptr;
        mData.mScreen     = nullptr;
        mData.mExternal   = false;
    }

    // =============================================================================
    // Queries
    // =============================================================================

    bool NkWindow::IsOpen()  const { return mIsOpen; }
    bool NkWindow::IsValid() const { return mIsOpen && mData.mWindow != 0; }

    NkError        NkWindow::GetLastError() const { return mLastError; }

    NkWindowConfig NkWindow::GetConfig()    const { 
        // Synchroniser avant de retourner
        if (mIsOpen && mData.mConnection && mData.mWindow) {
            // Appel à la fonction libre, pas à une méthode de classe
            SyncConfigFromWindow(mData.mConnection, mData.mWindow, 
                                 const_cast<NkWindow*>(this)->mConfig, mData);
        }
        return mConfig; 
    }

    NkString NkWindow::GetTitle() const {
        if (!mData.mConnection || !mData.mWindow) {
            return mConfig.title;
        }
        
        xcb_icccm_get_wm_name_reply_t nameReply;
        xcb_icccm_get_wm_name_cookie_t nameCookie = xcb_icccm_get_wm_name(mData.mConnection, mData.mWindow);
        
        if (xcb_icccm_get_wm_name_reply(mData.mConnection, nameCookie, &nameReply, nullptr)) {
            NkString title = NkString((const char*)nameReply.name, nameReply.name_len);
            xcb_icccm_get_wm_name_reply_wipe(&nameReply);
            
            // Synchroniser mConfig
            const_cast<NkWindow*>(this)->mConfig.title = title;
            
            return title;
        }
        return mConfig.title;
    }

    NkVec2u NkWindow::GetSize() const {
        if (!mData.mConnection || !mData.mWindow)
            return { mConfig.width, mConfig.height };
            
        xcb_get_geometry_cookie_t c = xcb_get_geometry(mData.mConnection, mData.mWindow);
        xcb_get_geometry_reply_t* r = xcb_get_geometry_reply(mData.mConnection, c, nullptr);
        if (!r) return { mConfig.width, mConfig.height };
        
        NkVec2u sz = { (uint32)r->width, (uint32)r->height };
        free(r);
        
        // Synchroniser mConfig
        const_cast<NkWindow*>(this)->mConfig.width = sz.x;
        const_cast<NkWindow*>(this)->mConfig.height = sz.y;
        
        return sz;
    }

    NkVec2u NkWindow::GetPosition() const {
        if (!mData.mConnection || !mData.mWindow)
            return { (uint32)mConfig.x, (uint32)mConfig.y };
            
        xcb_translate_coordinates_cookie_t c =
            xcb_translate_coordinates(mData.mConnection, mData.mWindow,
                                    sDefaultScreen->root, 0, 0);
        xcb_translate_coordinates_reply_t* r =
            xcb_translate_coordinates_reply(mData.mConnection, c, nullptr);
        if (!r) return { (uint32)mConfig.x, (uint32)mConfig.y };
        
        NkVec2u pos = { (uint32)r->dst_x, (uint32)r->dst_y };
        free(r);
        
        // Synchroniser mConfig
        const_cast<NkWindow*>(this)->mConfig.x = pos.x;
        const_cast<NkWindow*>(this)->mConfig.y = pos.y;
        
        return pos;
    }

    float   NkWindow::GetDpiScale()        const { return 1.f; }
    
    NkVec2u NkWindow::GetDisplaySize()     const {
        if (!sDefaultScreen) return { 1920, 1080 };
        return { (uint32)sDefaultScreen->width_in_pixels,
                (uint32)sDefaultScreen->height_in_pixels };
    }
    
    NkVec2u NkWindow::GetDisplayPosition() const { return { 0, 0 }; }

    // =============================================================================
    // Setters
    // =============================================================================

    void NkWindow::SetTitle(const NkString& title) {
        mConfig.title = title;
        if (!mData.mConnection || !mData.mWindow) return;
        
        xcb_change_property(mData.mConnection, XCB_PROP_MODE_REPLACE,
                            mData.mWindow, XCB_ATOM_WM_NAME,
                            XCB_ATOM_STRING, 8,
                            static_cast<uint32_t>(title.Size()), title.CStr());
        xcb_change_property(mData.mConnection, XCB_PROP_MODE_REPLACE,
                            mData.mWindow, sAtomNetWmName,
                            sAtomUtf8String, 8,
                            static_cast<uint32_t>(title.Size()), title.CStr());
        xcb_flush(mData.mConnection);
    }

    void NkWindow::SetSize(uint32 w, uint32 h) {
        mConfig.width = w; 
        mConfig.height = h;
        if (!mData.mConnection || !mData.mWindow) return;
        
        uint32_t values[] = { w, h };
        xcb_configure_window(mData.mConnection, mData.mWindow,
                            XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                            values);
        xcb_flush(mData.mConnection);
    }

    void NkWindow::SetPosition(int32 x, int32 y) {
        mConfig.x = x; 
        mConfig.y = y;
        if (!mData.mConnection || !mData.mWindow) return;
        
        uint32_t values[] = { (uint32_t)x, (uint32_t)y };
        xcb_configure_window(mData.mConnection, mData.mWindow,
                            XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                            values);
        xcb_flush(mData.mConnection);
    }

    void NkWindow::SetVisible(bool visible) {
        mConfig.visible = visible;
        if (!mData.mConnection || !mData.mWindow) return;
        
        if (visible) 
            xcb_map_window  (mData.mConnection, mData.mWindow);
        else 
            xcb_unmap_window(mData.mConnection, mData.mWindow);
        xcb_flush(mData.mConnection);
    }

    void NkWindow::Minimize() {
        // XCB n'expose pas iconify directement — envoyer WM_CHANGE_STATE
        if (!mData.mConnection || !mData.mWindow || !sDefaultScreen) return;
        
        xcb_client_message_event_t ev{};
        ev.response_type  = XCB_CLIENT_MESSAGE;
        ev.format         = 32;
        ev.window         = mData.mWindow;
        ev.type           = NkXCBInternAtom(mData.mConnection, "WM_CHANGE_STATE");
        ev.data.data32[0] = 3; // IconicState
        
        xcb_send_event(mData.mConnection, 0, sDefaultScreen->root,
                    XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
                    (const char*)&ev);
        xcb_flush(mData.mConnection);
        
        // L'état de visibilité change
        mConfig.visible = false;
    }

    void NkWindow::Maximize() {
        if (!mData.mConnection || !mData.mWindow || !sDefaultScreen) return;
        
        xcb_atom_t wmState   = NkXCBInternAtom(mData.mConnection, "_NET_WM_STATE");
        xcb_atom_t maxH      = NkXCBInternAtom(mData.mConnection, "_NET_WM_STATE_MAXIMIZED_HORZ");
        xcb_atom_t maxV      = NkXCBInternAtom(mData.mConnection, "_NET_WM_STATE_MAXIMIZED_VERT");
        
        xcb_client_message_event_t ev{};
        ev.response_type  = XCB_CLIENT_MESSAGE;
        ev.format         = 32;
        ev.window         = mData.mWindow;
        ev.type           = wmState;
        ev.data.data32[0] = 1; // _NET_WM_STATE_ADD
        ev.data.data32[1] = maxH;
        ev.data.data32[2] = maxV;
        
        xcb_send_event(mData.mConnection, 0, sDefaultScreen->root,
                    XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
                    (const char*)&ev);
        xcb_flush(mData.mConnection);
        
        // Mettre à jour la taille après maximisation
        xcb_get_geometry_cookie_t c = xcb_get_geometry(mData.mConnection, mData.mWindow);
        xcb_get_geometry_reply_t* r = xcb_get_geometry_reply(mData.mConnection, c, nullptr);
        if (r) {
            mConfig.width = r->width;
            mConfig.height = r->height;
            free(r);
        }
        mConfig.visible = true;
    }

    void NkWindow::Restore() {
        if (!mData.mConnection || !mData.mWindow) return;
        
        xcb_map_window(mData.mConnection, mData.mWindow);
        xcb_flush(mData.mConnection);
        
        // Mettre à jour après restauration
        xcb_get_geometry_cookie_t c = xcb_get_geometry(mData.mConnection, mData.mWindow);
        xcb_get_geometry_reply_t* r = xcb_get_geometry_reply(mData.mConnection, c, nullptr);
        if (r) {
            mConfig.width = r->width;
            mConfig.height = r->height;
            free(r);
        }
        mConfig.visible = true;
    }

    void NkWindow::SetFullscreen(bool fullscreen) {
        if (!mData.mConnection || !mData.mWindow || !sDefaultScreen) return;
        
        mConfig.fullscreen = fullscreen;
        
        xcb_atom_t wmState = NkXCBInternAtom(mData.mConnection, "_NET_WM_STATE");
        xcb_atom_t wmFs    = NkXCBInternAtom(mData.mConnection, "_NET_WM_STATE_FULLSCREEN");
        
        xcb_client_message_event_t ev{};
        ev.response_type  = XCB_CLIENT_MESSAGE;
        ev.format         = 32;
        ev.window         = mData.mWindow;
        ev.type           = wmState;
        ev.data.data32[0] = fullscreen ? 1 : 0;
        ev.data.data32[1] = wmFs;
        
        xcb_send_event(mData.mConnection, 0, sDefaultScreen->root,
                    XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
                    (const char*)&ev);
        xcb_flush(mData.mConnection);
        
        // Mettre à jour la taille
        if (fullscreen) {
            mConfig.width = sDefaultScreen->width_in_pixels;
            mConfig.height = sDefaultScreen->height_in_pixels;
        } else {
            xcb_get_geometry_cookie_t c = xcb_get_geometry(mData.mConnection, mData.mWindow);
            xcb_get_geometry_reply_t* r = xcb_get_geometry_reply(mData.mConnection, c, nullptr);
            if (r) {
                mConfig.width = r->width;
                mConfig.height = r->height;
                free(r);
            }
        }
    }

    // =============================================================================
    // Orientation (desktop — no-op)
    // =============================================================================

    bool NkWindow::SupportsOrientationControl() const { return false; }
    void NkWindow::SetScreenOrientation(NkScreenOrientation) {}
    NkScreenOrientation NkWindow::GetScreenOrientation() const {
        return NkScreenOrientation::NK_SCREEN_ORIENTATION_LANDSCAPE;
    }
    void NkWindow::SetAutoRotateEnabled(bool) {}
    bool NkWindow::IsAutoRotateEnabled() const { return false; }

    // =============================================================================
    // Mouse
    // =============================================================================

    void NkWindow::SetMousePosition(uint32 x, uint32 y) {
        if (mData.mConnection && mData.mWindow)
            xcb_warp_pointer(mData.mConnection, XCB_NONE, mData.mWindow,
                            0, 0, 0, 0, (int16_t)x, (int16_t)y);
    }

    void NkWindow::ShowMouse(bool show) {
        if (!mData.mConnection || !mData.mWindow) return;
        
        if (show) {
            uint32_t cursor = XCB_CURSOR_NONE;
            xcb_change_window_attributes(mData.mConnection, mData.mWindow,
                                        XCB_CW_CURSOR, &cursor);
        } else {
            // Créer un curseur invisible si pas encore fait
            if (!mData.mHiddenCursor) {
                xcb_pixmap_t pix = xcb_generate_id(mData.mConnection);
                xcb_create_pixmap(mData.mConnection, 1, pix, mData.mWindow, 1, 1);
                mData.mHiddenCursor = xcb_generate_id(mData.mConnection);
                xcb_create_cursor(mData.mConnection, mData.mHiddenCursor,
                                pix, pix, 0, 0, 0, 0, 0, 0, 0, 0);
                xcb_free_pixmap(mData.mConnection, pix);
            }
            xcb_change_window_attributes(mData.mConnection, mData.mWindow,
                                        XCB_CW_CURSOR, &mData.mHiddenCursor);
        }
        xcb_flush(mData.mConnection);
    }

    void NkWindow::CaptureMouse(bool) {} // XCB grab nécessite xcb_grab_pointer — laissé en no-op

    // =============================================================================
    // Web / extras (no-op on desktop)
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
        auto sz        = GetSize();  // GetSize synchronise déjà mConfig
        sd.width       = sz.x;
        sd.height      = sz.y;
        sd.dpi         = GetDpiScale();
        sd.connection  = mData.mConnection;
        sd.window      = mData.mWindow;
        sd.screen      = mData.mScreen;
        sd.appliedHints = mData.mAppliedHints;
        return sd;
    }

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_XCB