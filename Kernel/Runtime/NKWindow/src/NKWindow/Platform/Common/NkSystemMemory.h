#pragma once

// =============================================================================
// NkSystemMemory.h
// Helpers de liberation de memoire allouee par les API SYSTEME (OS / drivers).
//
// ATTENTION : ces helpers NE doivent PAS utiliser nkentseu::memory::NkFree.
// La memoire qu'ils liberent provient des bibliotheques natives (Xlib, libxcb,
// xkbcommon, ...) et DOIT etre rendue avec le free natif correspondant. Utiliser
// NkFree ici corromprait le tas (la memoire n'a jamais ete prise au pool NKMemory).
//
// Le but est uniquement de CENTRALISER et NOMMER ces frees systeme pour qu'aucun
// XFree()/free() nu ne traine disperse dans les backends.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKCore/NkTypes.h"

// X11/Xlib est present des qu'on utilise le backend XLIB ou XCB (XCB+GLX inclut
// aussi Xlib pour la creation de contexte GL).
#if defined(NKENTSEU_PLATFORM_LINUX) && (defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB))
    #define NK_SYSMEM_HAS_X11 1
#else
    #define NK_SYSMEM_HAS_X11 0
#endif

#if defined(NKENTSEU_PLATFORM_LINUX)
    #if NK_SYSMEM_HAS_X11
        #include <X11/Xlib.h>
    #endif
    #include <cstdlib>   // ::free pour les replies/events libxcb et les keymaps xkb
#endif

namespace nkentseu { namespace platform {

#if NK_SYSMEM_HAS_X11
    // Libere la memoire allouee par X11 (Xlib) — wrappe XFree.
    // NE PAS remplacer par NkFree : memoire allouee par le driver Xlib.
    inline void NkX11Free(void* p) noexcept {
        if (p) { XFree(p); }
    }
#endif

#if defined(NKENTSEU_PLATFORM_LINUX)
    // Libere la memoire allouee par libxcb / xkbcommon (replies, events, keymaps)
    // via la libc — wrappe le free() natif.
    // NE PAS remplacer par NkFree : memoire allouee par libxcb/xkb (malloc libc).
    inline void NkXcbFree(void* p) noexcept {
        if (p) { ::free(p); }
    }
#endif

}} // namespace nkentseu::platform
