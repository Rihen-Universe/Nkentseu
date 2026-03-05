#pragma once

// =============================================================================
// NkMain.h
// Inclut automatiquement le point d'entrée natif de la plateforme courante.
//
// À inclure UNE SEULE FOIS dans le fichier source qui implémente nkmain().
//
// Exemple :
//   #include <NkentseuWindow/Core/NkMain.h>
//
//   int nkmain(const nkentseu::NkEntryState& state) {
//       nkentseu::NkWindowConfig cfg;
//       cfg.title = "Hello NK";
//       // ...
//       return 0;
//   }
// =============================================================================

#include "NkPlatformDetect.h"
#include "NkEntry.h"

#if defined(NKENTSEU_PLATFORM_UWP)
#include "NKWindow/EntryPoints/NkUWP.h"

#elif defined(NKENTSEU_PLATFORM_XBOX)
#include "NKWindow/EntryPoints/NkXbox.h"

#elif defined(NKENTSEU_PLATFORM_WINDOWS)
#include "NKWindow/EntryPoints/NkWindowsDesktop.h"

#elif defined(NKENTSEU_PLATFORM_MACOS)
#include "NKWindow/EntryPoints/NkCocoa.h"

#elif defined(NKENTSEU_PLATFORM_IOS)
#include "NKWindow/EntryPoints/NkAppleMobile.h"

#elif defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY)
#include "NKWindow/EntryPoints/NkNoob.h"

#elif defined(NKENTSEU_WINDOWING_WAYLAND)
#include "NKWindow/EntryPoints/NkWayland.h"

#elif defined(NKENTSEU_WINDOWING_XCB)
#include "NKWindow/EntryPoints/NkXCB.h"

#elif defined(NKENTSEU_WINDOWING_XLIB)
#include "NKWindow/EntryPoints/NkXLib.h"

#elif defined(NKENTSEU_PLATFORM_ANDROID)
#include "NKWindow/EntryPoints/NkAndroid.h"

#elif defined(NKENTSEU_PLATFORM_WEB) || defined(__EMSCRIPTEN__)
#include "NKWindow/EntryPoints/NkWasm.h"

#else
#include "NKWindow/EntryPoints/NkNoob.h"
#endif
