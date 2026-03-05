#pragma once
// =============================================================================
// NkWaylandEventSystem.h - Wayland platform data for NkEventSystem
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_WAYLAND)

struct wl_display;

namespace nkentseu {

    struct NkEventSystemData {
        struct wl_display* mDisplay = nullptr;
    };

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_WAYLAND
