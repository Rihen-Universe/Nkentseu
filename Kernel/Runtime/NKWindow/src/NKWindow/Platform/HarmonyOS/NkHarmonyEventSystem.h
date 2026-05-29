#pragma once
// =============================================================================
// NkHarmonyEventSystem.h — HarmonyOS platform data for NkEventSystem (data only)
//
// Miroir de NkAndroidEventSystem.h adapté à HarmonyOS.
//
// Sur HarmonyOS, les événements tactiles et de cycle de vie de surface
// arrivent via les callbacks OH_NativeXComponent_Callback et
// OH_NativeXComponent_MouseEvent_Callback enregistrés au démarrage
// de la NativeAbility.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_HARMONYOS)

#include <ace/xcomponent/native_interface_xcomponent.h>

namespace nkentseu {

    // -------------------------------------------------------------------------
    // NkEventSystemData — données opaques portées par NkEventSystem::mData
    // -------------------------------------------------------------------------

    struct NkEventSystemData {
        // XComponent courant (registré via NkHarmonyOnSurfaceCreated)
        OH_NativeXComponent* mXComponent = nullptr;

        // true après Init(), false après Shutdown()
        bool                 mInitialized = false;
    };

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_HARMONYOS