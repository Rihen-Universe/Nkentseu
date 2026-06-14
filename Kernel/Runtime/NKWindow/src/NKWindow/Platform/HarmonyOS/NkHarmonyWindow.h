#pragma once
// =============================================================================
// NkHarmonyWindow.h — HarmonyOS platform data for NkWindow
//
// Supporte :
//   - Phone / Tablet   : plein écran, tactile, orientation
//   - PC / 2in1        : fenêtre redimensionnable, minimiser/maximiser/restaurer
//   - Safe Area        : insets lus depuis OH_NativeXComponent
//   - Rotation         : callbacks d'orientation système
//   - Clavier virtuel  : show/hide via InputMethod ArkTS bridge
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKEvent/NkSafeArea.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKEvent/NkWindowId.h"

#if defined(NKENTSEU_PLATFORM_HARMONYOS)

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <ace/xcomponent/native_xcomponent_key_event.h>
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {

    // -------------------------------------------------------------------------
    // Type de device HarmonyOS (détecté au runtime)
    // -------------------------------------------------------------------------
    enum class NkHarmonyDeviceType : uint32 {
        UNKNOWN  = 0,
        PHONE    = 1,   // Smartphone
        TABLET   = 2,   // Tablette
        PC_2IN1  = 3,   // PC / foldable / MateBook avec HarmonyOS PC
        TV       = 4,
        WATCH    = 5,
    };

    // -------------------------------------------------------------------------
    // NkWindowData — données platform portées par NkWindow::mData
    // -------------------------------------------------------------------------
    struct NkWindowData {
        // ── Surface native ───────────────────────────────────────────────────
        OHNativeWindow*          mNativeWindow   = nullptr;
        OH_NativeXComponent*     mXComponent     = nullptr;
        char                     mXComponentId[128] = {};

        // ── Dimensions (pixels physiques) ─────────────────────────────────
        uint64_t                 mWidth          = 0;
        uint64_t                 mHeight         = 0;
        uint64_t                 mPrevWidth      = 0;
        uint64_t                 mPrevHeight     = 0;

        // ── Safe Area (notch, barre nav, punch-hole) ──────────────────────
        NkSafeAreaInsets         mSafeArea{};

        // ── Orientation ───────────────────────────────────────────────────
        NkScreenOrientation      mOrientation    = NkScreenOrientation::NK_SCREEN_ORIENTATION_AUTO;
        // Angle courant en degrés (0=portrait, 90=landscape, 180, 270)
        int32                    mRotationDeg    = 0;

        // ── Hints de surface ─────────────────────────────────────────────
        NkSurfaceHints           mAppliedHints{};

        // ── Flags ─────────────────────────────────────────────────────────
        bool                     mExternal       = false;
        bool                     mFullscreen     = true;     // phone/tablet
        bool                     mHideSystemUI   = false;
        bool                     mLockOrientation = false;
        bool                     mVisible        = true;

        // ── État fenêtre PC (2in1 uniquement) ────────────────────────────
        bool                     mMinimized      = false;
        bool                     mMaximized      = false;
        // Taille/position avant maximise (pour Restore)
        uint64_t                 mRestoreWidth   = 0;
        uint64_t                 mRestoreHeight  = 0;

        // ── Type de device ────────────────────────────────────────────────
        NkHarmonyDeviceType      mDeviceType     = NkHarmonyDeviceType::UNKNOWN;

        // ── Clavier virtuel ───────────────────────────────────────────────
        bool                     mVirtualKeyboardVisible = false;
        // Hauteur occupée par le clavier virtuel (pour ajuster le layout)
        uint32                   mVirtualKeyboardHeight  = 0;

        // ── DPI / densité ─────────────────────────────────────────────────
        float                    mDpiScale       = 1.0f;
    };

    // -------------------------------------------------------------------------
    // Helpers thread-safe — registre de fenêtres
    // -------------------------------------------------------------------------
    class NkWindow;

    NkWindow*            NkHarmonyFindWindowById(NkWindowId id);
    NkVector<NkWindow*>  NkHarmonyGetWindowsSnapshot();
    NkWindow*            NkHarmonyGetLastWindow();
    void                 NkHarmonyRegisterWindow(NkWindow* window);
    void                 NkHarmonyUnregisterWindow(NkWindow* window);

    // ── Callbacks surface (appelés par NkHarmonyEventSystem) ─────────────────
    void NkHarmonyOnSurfaceCreated(OH_NativeXComponent*, OHNativeWindow*);
    void NkHarmonyOnSurfaceChanged(OH_NativeXComponent*, OHNativeWindow*);
    void NkHarmonyOnSurfaceDestroyed(OH_NativeXComponent*);

    // ── Callbacks système (appelés depuis ArkTS via NAPI bridge) ─────────────
    // Orientation changée par le système (rotationDeg = 0/90/180/270)
    void NkHarmonyOnOrientationChanged(int32 rotationDeg);

    // Safe area mise à jour (insets en pixels)
    void NkHarmonyOnSafeAreaChanged(float top, float right, float bottom, float left);

    // Clavier virtuel affiché/caché — height en pixels
    void NkHarmonyOnVirtualKeyboardChanged(bool visible, uint32 height);

    // Fenêtre PC : minimize / maximize / restore depuis ArkTS windowStage
    void NkHarmonyOnWindowMinimized();
    void NkHarmonyOnWindowMaximized();
    void NkHarmonyOnWindowRestored();
    void NkHarmonyOnWindowFocusChanged(bool focused);

    // Détection du type de device
    NkHarmonyDeviceType NkHarmonyDetectDeviceType();

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_HARMONYOS