// =============================================================================
// NkXdgShellProtocol.cpp
// Thin C++ wrapper that compiles the wayland-scanner-generated
// xdg-shell-protocol.c into the NKWindow C++ project.
//
// extern "C" ensures exported symbols (xdg_wm_base_interface, etc.) have
// C linkage so they match the declarations in xdg-shell-client-protocol.h.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_WAYLAND)

#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>

// Forward-declare the Wayland struct type (defined in wayland-util.h).
struct wl_interface;

extern "C" {
// In C++, `const` at namespace scope has internal linkage by default.
// Pre-declare each interface symbol as `extern` so their subsequent definition
// in xdg-shell-protocol.c inherits external linkage.
extern const struct wl_interface xdg_wm_base_interface;
extern const struct wl_interface xdg_positioner_interface;
extern const struct wl_interface xdg_surface_interface;
extern const struct wl_interface xdg_toplevel_interface;
extern const struct wl_interface xdg_popup_interface;

// Remove visibility("hidden") — not needed for a static archive, and it
// interacts badly with C++ const internal linkage rules.
// #define WL_PRIVATE
#include "xdg-shell-protocol.c"
// #undef WL_PRIVATE
}

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_WAYLAND
