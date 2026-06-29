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

    // Serial du dernier evenement d'entree pointeur (bouton/enter). Requis par
    // xdg_toplevel_move / xdg_toplevel_resize (grab implicite cote compositeur).
    unsigned int NkWaylandLastInputSerial();

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_WAYLAND
