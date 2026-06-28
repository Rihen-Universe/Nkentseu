// =============================================================================
// NkHarmonyWindow.cpp — NkWindow implementation for HarmonyOS (complète)
// Supporte phone/tablet/PC 2in1 + orientation + safe area + clavier virtuel
// =============================================================================

/**
 * @file NkHarmonyWindow.cpp
 * @brief Implémentation de NkWindow pour HarmonyOS
 * @details Supporte phone/tablet/PC 2in1 + orientation + safe area + clavier virtuel
 */

#include "NKPlatform/NkPlatformDetect.h"
#if defined(NKENTSEU_PLATFORM_HARMONYOS)

#include "NKWindow/Platform/HarmonyOS/NkHarmonyWindow.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWESystem.h"
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKCore/NkAtomic.h"
#include "NKLogger/NkLog.h"
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <cstring>

#if __has_include(<display_manager/oh_display_manager.h>)
#   include <display_manager/oh_display_manager.h>
#   define NK_HARMONY_HAS_DISPLAY_API 1
#else
#   define NK_HARMONY_HAS_DISPLAY_API 0
#endif

namespace nkentseu {
    using namespace math;

    // =========================================================================
    // Variables globales statiques
    // =========================================================================

    /** @brief Mutex pour protéger l'accès aux listes de fenêtres HarmonyOS */
    static NkSpinLock sHarmonyWindowsMutex;

    /** @brief Pointeur vers la dernière fenêtre HarmonyOS créée */
    static NkWindow* sHarmonyLastWindow = nullptr;

    /**
     * @brief Retourne le vecteur global des fenêtres HarmonyOS
     * @return NkVector<NkWindow*>& Référence vers le vecteur statique des fenêtres
     */
    static NkVector<NkWindow*>& HarmonyWindows()
    {
        static NkVector<NkWindow*> s;
        return s;
    }

    /**
     * @brief Retourne la map globale des fenêtres HarmonyOS par ID
     * @return NkUnorderedMap<NkWindowId,NkWindow*>& Référence vers la map statique
     */
    static NkUnorderedMap<NkWindowId, NkWindow*>& HarmonyWindowById()
    {
        static NkUnorderedMap<NkWindowId, NkWindow*> s;

        if (s.BucketCount() == 0)
        {
            s.Rehash(32);
        }

        return s;
    }

    // =========================================================================
    // Fonctions de gestion des fenêtres HarmonyOS
    // =========================================================================

    /**
     * @brief Recherche une fenêtre HarmonyOS par son ID
     * @param id L'ID de la fenêtre à rechercher
     * @return NkWindow* Pointeur vers la fenêtre trouvée, ou nullptr si non trouvée
     */
    NkWindow* NkHarmonyFindWindowById(NkWindowId id)
    {
        NkScopedSpinLock l(sHarmonyWindowsMutex);

        auto* w = HarmonyWindowById().Find(id);

        return w ? *w : nullptr;
    }

    /**
     * @brief Retourne un snapshot de toutes les fenêtres HarmonyOS
     * @return NkVector<NkWindow*> Copie du vecteur des fenêtres
     */
    NkVector<NkWindow*> NkHarmonyGetWindowsSnapshot()
    {
        NkScopedSpinLock l(sHarmonyWindowsMutex);

        return HarmonyWindows();
    }

    /**
     * @brief Retourne la dernière fenêtre HarmonyOS créée
     * @return NkWindow* Pointeur vers la dernière fenêtre, ou nullptr
     */
    NkWindow* NkHarmonyGetLastWindow()
    {
        NkScopedSpinLock l(sHarmonyWindowsMutex);

        return sHarmonyLastWindow;
    }

    /**
     * @brief Enregistre une fenêtre HarmonyOS dans les listes globales
     * @param window Pointeur vers la fenêtre à enregistrer
     */
    void NkHarmonyRegisterWindow(NkWindow* window)
    {
        if (!window)
        {
            return;
        }

        const NkWindowId id = window->GetId();

        if (id == NK_INVALID_WINDOW_ID)
        {
            return;
        }

        NkScopedSpinLock l(sHarmonyWindowsMutex);

        auto& v = HarmonyWindows();
        bool f = false;

        for (uint32 i = 0; i < v.Size(); ++i)
        {
            if (v[i] == window)
            {
                f = true;
                break;
            }
        }

        if (!f)
        {
            v.PushBack(window);
        }

        HarmonyWindowById()[id] = window;
        sHarmonyLastWindow = window;
    }

    /**
     * @brief Désenregistre une fenêtre HarmonyOS des listes globales
     * @param window Pointeur vers la fenêtre à désenregistrer
     */
    void NkHarmonyUnregisterWindow(NkWindow* window)
    {
        if (!window)
        {
            return;
        }

        NkScopedSpinLock l(sHarmonyWindowsMutex);

        auto& v = HarmonyWindows();

        for (uint32 i = 0; i < v.Size(); ++i)
        {
            if (v[i] == window)
            {
                v.Erase(v.begin() + i);
                break;
            }
        }

        NkWindowId s = NK_INVALID_WINDOW_ID;

        HarmonyWindowById().ForEach([&](NkWindowId id, NkWindow* const& vv)
        {
            if (vv == window && s == NK_INVALID_WINDOW_ID)
            {
                s = id;
            }
        });

        if (s != NK_INVALID_WINDOW_ID)
        {
            HarmonyWindowById().Erase(s);
        }

        if (sHarmonyLastWindow == window)
        {
            sHarmonyLastWindow = v.Empty() ? nullptr : v.Back();
        }
    }

    /**
     * @brief Détecte le type de périphérique HarmonyOS
     * @return NkHarmonyDeviceType Le type de périphérique détecté (PHONE, TABLET ou PC_2IN1)
     * @details Utilise l'API Display Manager si disponible, sinon interroge les propriétés système
     */
    NkHarmonyDeviceType NkHarmonyDetectDeviceType()
    {
        #if NK_HARMONY_HAS_DISPLAY_API
        NativeDisplayManager_DisplayType dtype = DISPLAY_TYPE_UNKNOWN;

        if (OH_NativeDisplayManager_GetDefaultDisplayType(&dtype) == DISPLAY_MANAGER_OK)
        {
            switch (dtype)
            {
                case DISPLAY_TYPE_PHONE:
                    return NkHarmonyDeviceType::PHONE;

                case DISPLAY_TYPE_TABLET:
                    return NkHarmonyDeviceType::TABLET;

                case DISPLAY_TYPE_DESKTOP:
                    return NkHarmonyDeviceType::PC_2IN1;

                default:
                    break;
            }
        }
        #endif

        FILE* f = popen("param get const.product.devicetype 2>/dev/null", "r");

        if (f)
        {
            char buf[64] = {};
            fgets(buf, sizeof(buf), f);
            pclose(f);

            NkString t(buf);
            t = t.Trim();

            if (t == "phone")
            {
                return NkHarmonyDeviceType::PHONE;
            }

            if (t == "tablet")
            {
                return NkHarmonyDeviceType::TABLET;
            }

            if (t == "2in1")
            {
                return NkHarmonyDeviceType::PC_2IN1;
            }
        }

        return NkHarmonyDeviceType::PHONE;
    }

    /**
     * @brief Recherche une fenêtre par son composant XComponent
     * @param xcomp Le composant XComponent à rechercher
     * @return NkWindow* Pointeur vers la fenêtre trouvée, ou nullptr
     */
    static NkWindow* _FindWindowByXComp(OH_NativeXComponent* xcomp)
    {
        char id[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
        uint64_t len = OH_XCOMPONENT_ID_LEN_MAX;

        if (OH_NativeXComponent_GetXComponentId(xcomp, id, &len) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS)
        {
            return nullptr;
        }

        for (NkWindow* w : NkHarmonyGetWindowsSnapshot())
        {
            if (w && std::strncmp(w->mData.mXComponentId, id, OH_XCOMPONENT_ID_LEN_MAX) == 0)
            {
                return w;
            }
        }

        return nullptr;
    }

    // =========================================================================
    // Callbacks de surface XComponent
    // =========================================================================

    /**
     * @brief Callback appelé lors de la création de la surface XComponent
     * @param x Le composant XComponent
     * @param n La fenêtre native OHNativeWindow
     */
    void NkHarmonyOnSurfaceCreated(OH_NativeXComponent* x, OHNativeWindow* n)
    {
        if (!x || !n)
        {
            return;
        }

        NkWindow* win = _FindWindowByXComp(x);

        if (!win)
        {
            return;
        }

        win->mData.mXComponent = x;
        win->mData.mNativeWindow = n;

        uint64_t w = 0;
        uint64_t h = 0;

        OH_NativeXComponent_GetXComponentSize(x, n, &w, &h);

        win->mData.mPrevWidth = win->mData.mWidth;
        win->mData.mPrevHeight = win->mData.mHeight;
        win->mData.mWidth = static_cast<uint32>(w);
        win->mData.mHeight = static_cast<uint32>(h);

        #if NK_HARMONY_HAS_DISPLAY_API
        uint32_t dpi = 160;
        OH_NativeDisplayManager_GetDefaultDisplayDensityDpi(&dpi);
        const float prevScale = win->mData.mDpiScale;
        const float newScale = static_cast<float>(dpi) / 160.0f;
        win->mData.mDpiScale = newScale;

        // Émet un NkWindowDpiEvent si la densité a réellement changé (ex.
        // déplacement vers un écran de densité différente, changement de
        // configuration). Tolérance pour éviter les faux positifs flottants.
        const float scaleDelta = (newScale > prevScale) ? (newScale - prevScale) : (prevScale - newScale);
        if (newScale > 0.0f && scaleDelta > 0.001f)
        {
            NkWindowDpiEvent dpiEvt(newScale, prevScale, dpi);
            NkWESystem::Events().Enqueue_Public(dpiEvt, win->GetId());
        }
        #endif

        NkWindowSurfaceCreatedEvent evt(static_cast<uint32>(w), static_cast<uint32>(h));
        NkWESystem::Events().Enqueue_Public(evt, win->GetId());
    }

    /**
     * @brief Callback appelé lors du changement de la surface XComponent
     * @param x Le composant XComponent
     * @param n La fenêtre native OHNativeWindow
     */
    void NkHarmonyOnSurfaceChanged(OH_NativeXComponent* x, OHNativeWindow* n)
    {
        if (!x || !n)
        {
            return;
        }

        NkWindow* win = _FindWindowByXComp(x);

        if (!win)
        {
            return;
        }

        uint64_t w = 0;
        uint64_t h = 0;

        OH_NativeXComponent_GetXComponentSize(x, n, &w, &h);

        const uint32 ow = static_cast<uint32>(win->mData.mWidth);
        const uint32 oh = static_cast<uint32>(win->mData.mHeight);

        win->mData.mPrevWidth = ow;
        win->mData.mPrevHeight = oh;
        win->mData.mWidth = static_cast<uint32>(w);
        win->mData.mHeight = static_cast<uint32>(h);

        if (win->mData.mWidth != ow || win->mData.mHeight != oh)
        {
            NkWindowResizeEvent evt(win->mData.mWidth, win->mData.mHeight, ow, oh);
            NkWESystem::Events().Enqueue_Public(evt, win->GetId());
        }
    }

    /**
     * @brief Callback appelé lors de la destruction de la surface XComponent
     * @param x Le composant XComponent
     */
    void NkHarmonyOnSurfaceDestroyed(OH_NativeXComponent* x)
    {
        if (!x)
        {
            return;
        }

        NkWindow* win = _FindWindowByXComp(x);

        if (!win)
        {
            return;
        }

        win->mData.mNativeWindow = nullptr;
        win->mData.mXComponent = nullptr;

        NkWindowSurfaceDestroyedEvent evt;
        NkWESystem::Events().Enqueue_Public(evt, win->GetId());
    }

    // =========================================================================
    // Callbacks de fenêtre
    // =========================================================================

    /**
     * @brief Callback appelé lors du changement d'orientation
     * @param deg Degrés de rotation (0, 90, 180, 270)
     */
    void NkHarmonyOnOrientationChanged(int32 deg)
    {
        for (NkWindow* win : NkHarmonyGetWindowsSnapshot())
        {
            if (!win)
            {
                continue;
            }

            win->mData.mRotationDeg = deg;

            NkScreenOrientation o = (deg == 0 || deg == 180)
                ? NkScreenOrientation::NK_SCREEN_ORIENTATION_PORTRAIT
                : NkScreenOrientation::NK_SCREEN_ORIENTATION_LANDSCAPE;

            win->mData.mOrientation = o;

            NkWindowOrientationChangedEvent evt(o, deg);
            NkWESystem::Events().Enqueue_Public(evt, win->GetId());
        }
    }

    /**
     * @brief Callback appelé lors du changement des marges de sécurité (safe area)
     * @param top Marge supérieure
     * @param right Marge droite
     * @param bottom Marge inférieure
     * @param left Marge gauche
     */
    void NkHarmonyOnSafeAreaChanged(float top, float right, float bottom, float left)
    {
        for (NkWindow* win : NkHarmonyGetWindowsSnapshot())
        {
            if (!win)
            {
                continue;
            }

            NkSafeAreaInsets& ins = win->mData.mSafeArea;

            if (ins.top == top && ins.right == right && ins.bottom == bottom && ins.left == left)
            {
                continue;
            }

            ins.top = top;
            ins.right = right;
            ins.bottom = bottom;
            ins.left = left;

            NkWindowSafeAreaChangedEvent evt(ins);
            NkWESystem::Events().Enqueue_Public(evt, win->GetId());
        }
    }

    /**
     * @brief Callback appelé lors du changement du clavier virtuel
     * @param visible true si le clavier est visible
     * @param height Hauteur du clavier virtuel en pixels
     */
    void NkHarmonyOnVirtualKeyboardChanged(bool visible, uint32 height)
    {
        for (NkWindow* win : NkHarmonyGetWindowsSnapshot())
        {
            if (!win)
            {
                continue;
            }

            win->mData.mVirtualKeyboardVisible = visible;
            win->mData.mVirtualKeyboardHeight = height;

            NkWindowVirtualKeyboardChangedEvent evt(visible, height);
            NkWESystem::Events().Enqueue_Public(evt, win->GetId());
        }
    }

    /**
     * @brief Callback appelé lors de la minimisation de la fenêtre
     */
    void NkHarmonyOnWindowMinimized()
    {
        for (NkWindow* win : NkHarmonyGetWindowsSnapshot())
        {
            if (!win || win->mData.mMinimized)
            {
                continue;
            }

            win->mData.mMinimized = true;

            NkWindowMinimizeEvent evt;
            NkWESystem::Events().Enqueue_Public(evt, win->GetId());
        }
    }

    /**
     * @brief Callback appelé lors de la maximisation de la fenêtre
     */
    void NkHarmonyOnWindowMaximized()
    {
        for (NkWindow* win : NkHarmonyGetWindowsSnapshot())
        {
            if (!win || win->mData.mMaximized)
            {
                continue;
            }

            win->mData.mRestoreWidth = win->mData.mWidth;
            win->mData.mRestoreHeight = win->mData.mHeight;
            win->mData.mMaximized = true;
            win->mData.mMinimized = false;

            NkWindowMaximizeEvent evt;
            NkWESystem::Events().Enqueue_Public(evt, win->GetId());
        }
    }

    /**
     * @brief Callback appelé lors de la restauration de la fenêtre
     */
    void NkHarmonyOnWindowRestored()
    {
        for (NkWindow* win : NkHarmonyGetWindowsSnapshot())
        {
            if (!win || (!win->mData.mMinimized && !win->mData.mMaximized))
            {
                continue;
            }

            win->mData.mMinimized = false;
            win->mData.mMaximized = false;

            NkWindowRestoreEvent evt;
            NkWESystem::Events().Enqueue_Public(evt, win->GetId());
        }
    }

    /**
     * @brief Callback appelé lors du changement de focus de la fenêtre
     * @param focused true si la fenêtre a gagné le focus, false si elle l'a perdu
     */
    void NkHarmonyOnWindowFocusChanged(bool focused)
    {
        for (NkWindow* win : NkHarmonyGetWindowsSnapshot())
        {
            if (!win)
            {
                continue;
            }

            if (focused)
            {
                NkWindowFocusGainedEvent e;
                NkWESystem::Events().Enqueue_Public(e, win->GetId());
            }
            else
            {
                NkWindowFocusLostEvent e;
                NkWESystem::Events().Enqueue_Public(e, win->GetId());
            }
        }
    }

    // =========================================================================
    // Implémentation de NkWindow
    // =========================================================================

    /**
     * @brief Constructeur par défaut
     */
    NkWindow::NkWindow() = default;

    /**
     * @brief Constructeur avec configuration
     * @param c Configuration de la fenêtre
     */
    NkWindow::NkWindow(const NkWindowConfig& c)
    {
        Create(c);
    }

    /**
     * @brief Destructeur
     */
    NkWindow::~NkWindow()
    {
        if (mIsOpen)
        {
            Close();
        }
    }

    /**
     * @brief Crée la fenêtre avec la configuration spécifiée
     * @param config Configuration de la fenêtre
     * @return true si la création a réussi, false sinon
     */
    bool NkWindow::Create(const NkWindowConfig& config)
    {
        if (mIsOpen)
        {
            Close();
        }

        mConfig = config;
        mData.mDeviceType = NkHarmonyDetectDeviceType();
        mData.mAppliedHints = config.surfaceHints;
        mData.mHideSystemUI = config.hideSystemUI;
        mData.mLockOrientation = config.lockOrientation;
        mData.mOrientation = config.screenOrientation;
        mData.mVisible = config.visible;
        mData.mExternal = false;

        const bool isPC = (mData.mDeviceType == NkHarmonyDeviceType::PC_2IN1);
        mData.mFullscreen = isPC ? config.fullscreen : true;

        mData.mWidth = config.width > 0 ? config.width : 1280u;
        mData.mHeight = config.height > 0 ? config.height : 720u;
        mData.mRestoreWidth = mData.mWidth;
        mData.mRestoreHeight = mData.mHeight;

        if (config.native.useExternalWindow)
        {
            if (!config.native.externalWindowHandle)
            {
                mLastError = NkError(1, "HarmonyOS: handle null");
                return false;
            }

            mData.mNativeWindow = reinterpret_cast<OHNativeWindow*>(config.native.externalWindowHandle);
            mData.mExternal = true;
        }

        mId = NkWESystem::Instance().RegisterWindow(this);

        if (mId == NK_INVALID_WINDOW_ID)
        {
            mLastError = NkError(1, "HarmonyOS: register failed");
            return false;
        }

        mIsOpen = true;
        NkHarmonyRegisterWindow(this);

        NkWindowCreateEvent ce(static_cast<uint32>(mData.mWidth), static_cast<uint32>(mData.mHeight));
        NkWESystem::Events().Enqueue_Public(ce, mId);

        if (config.visible)
        {
            NkWindowShownEvent se;
            NkWESystem::Events().Enqueue_Public(se, mId);
        }

        return true;
    }

    /**
     * @brief Ferme la fenêtre
     */
    void NkWindow::Close()
    {
        if (!mIsOpen)
        {
            return;
        }

        const NkWindowId cid = mId;

        NkWindowCloseEvent ce(false);
        NkWESystem::Events().Enqueue_Public(ce, cid);

        NkWindowDestroyEvent de;
        NkWESystem::Events().Enqueue_Public(de, cid);

        NkHarmonyUnregisterWindow(this);
        NkWESystem::Instance().UnregisterWindow(cid);

        mId = NK_INVALID_WINDOW_ID;
        mIsOpen = false;
        mData.mNativeWindow = nullptr;
        mData.mXComponent = nullptr;
        mData.mWidth = 0;
        mData.mHeight = 0;
    }

    /**
     * @brief Vérifie si la fenêtre est ouverte
     * @return true si la fenêtre est ouverte
     */
    bool NkWindow::IsOpen() const
    {
        return mIsOpen;
    }

    /**
     * @brief Vérifie si la fenêtre est valide
     * @return true si la fenêtre est ouverte et a une fenêtre native ou externe
     */
    bool NkWindow::IsValid() const
    {
        return mIsOpen && (mData.mNativeWindow || mData.mExternal);
    }

    /**
     * @brief Retourne la dernière erreur
     * @return NkError La dernière erreur survenue
     */
    NkError NkWindow::GetLastError() const
    {
        return mLastError;
    }

    /**
     * @brief Retourne la configuration de la fenêtre
     * @return NkWindowConfig La configuration actuelle
     */
    NkWindowConfig NkWindow::GetConfig() const
    {
        return mConfig;
    }

    /**
     * @brief Retourne le titre de la fenêtre
     * @return NkString Le titre de la fenêtre
     */
    NkString NkWindow::GetTitle() const
    {
        return mConfig.title;
    }

    /**
     * @brief Définit le titre de la fenêtre
     * @param t Le nouveau titre
     */
    void NkWindow::SetTitle(const NkString& t)
    {
        mConfig.title = t;
    }

    /**
     * @brief Retourne la taille de la fenêtre
     * @return NkVec2u La taille (largeur, hauteur)
     */
    NkVec2u NkWindow::GetSize() const
    {
        return {static_cast<uint32>(mData.mWidth), static_cast<uint32>(mData.mHeight)};
    }

    /**
     * @brief Retourne la position de la fenêtre
     * @return NkVec2u La position (toujours {0, 0} sur HarmonyOS)
     */
    NkVec2u NkWindow::GetPosition() const
    {
        return {0u, 0u};
    }

    /**
     * @brief Retourne la taille de l'affichage
     * @return NkVec2u La taille de l'affichage
     */
    NkVec2u NkWindow::GetDisplaySize() const
    {
        return GetSize();
    }

    /**
     * @brief Retourne la position de l'affichage
     * @return NkVec2u La position (toujours {0, 0})
     */
    NkVec2u NkWindow::GetDisplayPosition() const
    {
        return {0u, 0u};
    }

    /**
     * @brief Retourne l'échelle DPI
     * @return float L'échelle DPI
     */
    float NkWindow::GetDpiScale() const
    {
        return mData.mDpiScale;
    }

    // =========================================================================
    // Énumération des moniteurs / DPI
    //
    // HarmonyOS (phone/tablet/2in1) est mono-écran du point de vue de
    // l'application : un seul NkDisplayInfo est exposé. Les caractéristiques
    // proviennent du Display Manager natif (OH_NativeDisplayManager_*) quand il
    // est disponible, avec repli sur la taille de la fenêtre. La densité de
    // référence HarmonyOS est 160 dpi (comme Android), donc dpiScale = dpi/160.
    // =========================================================================

    // Construit l'unique NkDisplayInfo décrivant l'écran HarmonyOS courant.
    static NkDisplayInfo NkHarmonyFillDisplayInfo(const NkWindow& window)
    {
        NkDisplayInfo info;
        info.index = 0;
        info.isPrimary = true;

        // Taille par défaut = taille de la fenêtre/surface courante.
        uint32 width = static_cast<uint32>(window.mData.mWidth);
        uint32 height = static_cast<uint32>(window.mData.mHeight);
        uint32 densityDpi = 160;
        uint32 refresh = 60;

        #if NK_HARMONY_HAS_DISPLAY_API
        int32_t dispW = 0;
        if (OH_NativeDisplayManager_GetDefaultDisplayWidth(&dispW) == DISPLAY_MANAGER_OK && dispW > 0)
        {
            width = static_cast<uint32>(dispW);
        }

        int32_t dispH = 0;
        if (OH_NativeDisplayManager_GetDefaultDisplayHeight(&dispH) == DISPLAY_MANAGER_OK && dispH > 0)
        {
            height = static_cast<uint32>(dispH);
        }

        uint32_t dpi = 160;
        if (OH_NativeDisplayManager_GetDefaultDisplayDensityDpi(&dpi) == DISPLAY_MANAGER_OK && dpi > 0)
        {
            densityDpi = static_cast<uint32>(dpi);
        }

        uint32_t hz = 0;
        if (OH_NativeDisplayManager_GetDefaultDisplayRefreshRate(&hz) == DISPLAY_MANAGER_OK && hz > 0)
        {
            refresh = static_cast<uint32>(hz);
        }
        #endif

        info.width      = width;
        info.height     = height;
        info.physWidth  = width;
        info.physHeight = height;
        info.refreshRate = refresh;
        info.dpiX     = static_cast<float>(densityDpi);
        info.dpiY     = static_cast<float>(densityDpi);
        info.dpiScale = static_cast<float>(densityDpi) / 160.0f;

        const char* name = "HarmonyOS Display";
        usize i = 0;
        for (; name[i] != '\0' && i < sizeof(info.name) - 1; ++i) info.name[i] = name[i];
        info.name[i] = '\0';
        return info;
    }

    /**
     * @brief Énumère les moniteurs connectés (un seul sur HarmonyOS)
     * @return NkVector<NkDisplayInfo> Vecteur contenant l'unique écran
     */
    NkVector<NkDisplayInfo> NkWindow::EnumerateMonitors() const
    {
        NkVector<NkDisplayInfo> out;
        out.PushBack(NkHarmonyFillDisplayInfo(*this));
        return out;
    }

    /**
     * @brief Retourne le moniteur contenant la fenêtre (écran courant)
     * @return NkDisplayInfo L'écran HarmonyOS courant
     */
    NkDisplayInfo NkWindow::GetCurrentMonitor() const
    {
        return NkHarmonyFillDisplayInfo(*this);
    }

    /**
     * @brief Retourne le nombre de moniteurs connectés (toujours 1 sur HarmonyOS)
     * @return uint32 Toujours 1
     */
    uint32 NkWindow::GetMonitorCount() const
    {
        return 1u;
    }

    /**
     * @brief Définit la taille de la fenêtre
     * @param w Largeur en pixels
     * @param h Hauteur en pixels
     */
    void NkWindow::SetSize(uint32 w, uint32 h)
    {
        const uint32 ow = static_cast<uint32>(mData.mWidth);
        const uint32 oh = static_cast<uint32>(mData.mHeight);

        mData.mWidth = math::NkMax(w, 1u);
        mData.mHeight = math::NkMax(h, 1u);
        mConfig.width = mData.mWidth;
        mConfig.height = mData.mHeight;

        NkWindowResizeEvent e(mData.mWidth, mData.mHeight, ow, oh);
        NkWESystem::Events().Enqueue_Public(e, mId);
    }

    /**
     * @brief Définit la position de la fenêtre (non supporté sur HarmonyOS)
     * @param x Position X (ignorée)
     * @param y Position Y (ignorée)
     */
    void NkWindow::SetPosition(int32, int32)
    {
    }

    /**
     * @brief Définit la visibilité de la fenêtre
     * @param v true pour visible, false pour cachée
     */
    void NkWindow::SetVisible(bool v)
    {
        if (mData.mVisible == v)
        {
            return;
        }

        mData.mVisible = v;
        mConfig.visible = v;

        if (v)
        {
            NkWindowShownEvent e;
            NkWESystem::Events().Enqueue_Public(e, mId);
        }
        else
        {
            NkWindowHiddenEvent e;
            NkWESystem::Events().Enqueue_Public(e, mId);
        }
    }

    /**
     * @brief Minimise la fenêtre (PC 2-en-1 uniquement)
     */
    void NkWindow::Minimize()
    {
        if (mData.mDeviceType != NkHarmonyDeviceType::PC_2IN1 || mData.mMinimized)
        {
            return;
        }

        mData.mMinimized = true;

        NkWindowMinimizeEvent e;
        NkWESystem::Events().Enqueue_Public(e, mId);
    }

    /**
     * @brief Maximise la fenêtre (PC 2-en-1 uniquement)
     */
    void NkWindow::Maximize()
    {
        if (mData.mDeviceType != NkHarmonyDeviceType::PC_2IN1 || mData.mMaximized)
        {
            return;
        }

        mData.mRestoreWidth = mData.mWidth;
        mData.mRestoreHeight = mData.mHeight;
        mData.mMaximized = true;
        mData.mMinimized = false;

        NkWindowMaximizeEvent e;
        NkWESystem::Events().Enqueue_Public(e, mId);
    }

    /**
     * @brief Restaure la fenêtre (PC 2-en-1 uniquement)
     */
    bool NkWindow::IsMaximized() const { return false; }
    void NkWindow::BeginDragMove() {}
    void NkWindow::BeginResize(NkResizeEdge) {}

    void NkWindow::Restore()
    {
        if (!mData.mMinimized && !mData.mMaximized)
        {
            return;
        }

        mData.mMinimized = false;
        mData.mMaximized = false;

        if (mData.mRestoreWidth > 0 && mData.mRestoreHeight > 0)
        {
            SetSize(static_cast<uint32>(mData.mRestoreWidth), static_cast<uint32>(mData.mRestoreHeight));
        }

        NkWindowRestoreEvent e;
        NkWESystem::Events().Enqueue_Public(e, mId);
    }

    /**
     * @brief Définit le mode plein écran (PC 2-en-1 uniquement)
     * @param fs true pour plein écran, false pour fenêtré
     */
    void NkWindow::SetFullscreen(bool fs)
    {
        if (mData.mDeviceType != NkHarmonyDeviceType::PC_2IN1)
        {
            mData.mFullscreen = true;
            return;
        }

        if (mData.mFullscreen == fs)
        {
            return;
        }

        mData.mFullscreen = fs;
        mConfig.fullscreen = fs;

        if (fs)
        {
            NkWindowFullscreenEvent e;
            NkWESystem::Events().Enqueue_Public(e, mId);
        }
        else
        {
            NkWindowWindowedEvent e;
            NkWESystem::Events().Enqueue_Public(e, mId);
        }
    }

    /**
     * @brief Vérifie si le contrôle d'orientation est supporté
     * @return true toujours sur HarmonyOS
     */
    bool NkWindow::SupportsOrientationControl() const
    {
        return true;
    }

    /**
     * @brief Définit l'orientation de l'écran
     * @param o L'orientation souhaitée
     */
    void NkWindow::SetScreenOrientation(NkScreenOrientation o)
    {
        mData.mOrientation = o;
        mConfig.screenOrientation = o;
    }

    /**
     * @brief Retourne l'orientation actuelle de l'écran
     * @return NkScreenOrientation L'orientation actuelle
     */
    NkScreenOrientation NkWindow::GetScreenOrientation() const
    {
        return mData.mOrientation;
    }

    /**
     * @brief Active ou désactive la rotation automatique
     * @param e true pour activer, false pour désactiver
     */
    void NkWindow::SetAutoRotateEnabled(bool e)
    {
        mData.mLockOrientation = !e;
        mConfig.lockOrientation = !e;
    }

    /**
     * @brief Vérifie si la rotation automatique est activée
     * @return true si activée
     */
    bool NkWindow::IsAutoRotateEnabled() const
    {
        return !mData.mLockOrientation;
    }

    /**
     * @brief Définit si l'UI système doit être cachée
     * @param h true pour cacher
     */
    void NkWindow::SetHideSystemUI(bool h)
    {
        mData.mHideSystemUI = h;
        mConfig.hideSystemUI = h;
    }

    /**
     * @brief Vérifie si l'UI système est cachée
     * @return true si cachée
     */
    bool NkWindow::GetHideSystemUI() const
    {
        return mData.mHideSystemUI;
    }

    /**
     * @brief Verrouille ou déverrouille l'orientation
     * @param l true pour verrouiller
     */
    void NkWindow::SetLockOrientation(bool l)
    {
        mData.mLockOrientation = l;
        mConfig.lockOrientation = l;
    }

    /**
     * @brief Vérifie si l'orientation est verrouillée
     * @return true si verrouillée
     */
    bool NkWindow::GetLockOrientation() const
    {
        return mData.mLockOrientation;
    }

    /**
     * @brief Définit la position de la souris (non supporté sur HarmonyOS)
     * @param x Position X (ignorée)
     * @param y Position Y (ignorée)
     */
    void NkWindow::SetMousePosition(uint32, uint32)
    {
    }

    /**
     * @brief Affiche ou cache le curseur de la souris (non supporté sur HarmonyOS)
     * @param visible true pour afficher (ignoré)
     */
    void NkWindow::ShowMouse(bool)
    {
    }

    /**
     * @brief Capture ou libère la souris (non supporté sur HarmonyOS)
     * @param capture true pour capturer (ignoré)
     */
    void NkWindow::CaptureMouse(bool)
    {
    }

    /**
     * @brief Confine la souris à la zone cliente (non supporté sur HarmonyOS)
     * @param clip true pour confiner (ignoré)
     */
    void NkWindow::ClipMouseToClient(bool)
    {
    }

    /**
     * @brief Définit les options d'entrée web (non supporté sur HarmonyOS)
     * @param options Options (ignorées)
     */
    void NkWindow::SetWebInputOptions(const NkWebInputOptions&)
    {
    }

    /**
     * @brief Retourne les options d'entrée web
     * @return NkWebInputOptions Options par défaut
     */
    NkWebInputOptions NkWindow::GetWebInputOptions() const
    {
        return {};
    }

    /**
     * @brief Définit la progression (non supporté sur HarmonyOS)
     * @param progress Valeur de progression (ignorée)
     */
    void NkWindow::SetProgress(float)
    {
    }

    /**
     * @brief Retourne les marges de sécurité (safe area)
     * @return NkSafeAreaInsets Les marges actuelles
     */
    NkSafeAreaInsets NkWindow::GetSafeAreaInsets() const
    {
        return mData.mSafeArea;
    }

    /**
     * @brief Retourne la description de la surface
     * @return NkSurfaceDesc La description de la surface
     */
    NkSurfaceDesc NkWindow::GetSurfaceDesc() const
    {
        NkSurfaceDesc d;

        d.width = static_cast<uint32>(mData.mWidth);
        d.height = static_cast<uint32>(mData.mHeight);
        d.dpi = mData.mDpiScale;
        // NkSurfaceDesc Harmony : `ohNativeWindow` (OHNativeWindow*) — pas
        // `nativeWindow`. Voir NkSurface.h #elif NKENTSEU_PLATFORM_HARMONYOS.
        d.ohNativeWindow = reinterpret_cast<OHNativeWindow*>(mData.mNativeWindow);
        d.appliedHints = mData.mAppliedHints;

        return d;
    }

} // namespace nkentseu
#endif // NKENTSEU_PLATFORM_HARMONYOS