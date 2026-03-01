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

#if defined(NKENTSEU_PLATFORM_WIN32)
#include "../EntryPoints/NkWindowsDesktop.h"

#elif defined(NKENTSEU_PLATFORM_UWP) || defined(NKENTSEU_PLATFORM_XBOX)
#include "../EntryPoints/NkUWP.h"

#elif defined(NKENTSEU_PLATFORM_COCOA)
#include "../EntryPoints/NkCocoa.h"

#elif defined(NKENTSEU_PLATFORM_UIKIT)
#include "../EntryPoints/NkAppleMobile.h"

#elif defined(NKENTSEU_PLATFORM_XCB)
#include "../EntryPoints/NkXCB.h"

#elif defined(NKENTSEU_PLATFORM_XLIB)
#include "../EntryPoints/NkXLib.h"

#elif defined(NKENTSEU_PLATFORM_ANDROID)
#include "../EntryPoints/NkAndroid.h"

#elif defined(NKENTSEU_PLATFORM_WASM)
#include "../EntryPoints/NkWasm.h"

#else
#include "../EntryPoints/NkNoob.h"
#endif
