// =============================================================================
// NkXdgDecorationProtocol.cpp
// Thin C++ wrapper that compiles the wayland-scanner-generated
// xdg-decoration-protocol.c into the NKWindow C++ project.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_WAYLAND)

#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>

struct wl_interface;

extern "C" {
extern const struct wl_interface zxdg_decoration_manager_v1_interface;
extern const struct wl_interface zxdg_toplevel_decoration_v1_interface;

// #define WL_PRIVATE
#include "xdg-decoration-protocol.c"
// #undef WL_PRIVATE
}

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_WAYLAND
