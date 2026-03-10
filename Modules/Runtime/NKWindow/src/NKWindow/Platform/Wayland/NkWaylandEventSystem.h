#pragma once
// =============================================================================
// NkWaylandEventSystem.h - Wayland platform data for NkEventSystem
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_WAYLAND)

struct wl_display;
struct wl_seat;

namespace nkentseu {

    class NkEventSystem;

    struct NkEventSystemData {
        ::wl_display* mDisplay = nullptr;
    };

    // Attache le listener wl_seat dès que le seat est disponible.
    // Appelé tôt dans la création de fenêtre pour ne pas rater l'event
    // initial "capabilities" (clavier/pointeur).
    void NkWaylandAttachSeatListener(NkEventSystem* eventSystem, ::wl_seat* seat);
    void NkWaylandNotifySeatDestroy(::wl_seat* seat);

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_WAYLAND
