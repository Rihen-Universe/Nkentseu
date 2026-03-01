#pragma once

// =============================================================================
// NkWindow.h â€” Header principal NkWindow v2
//
// Inclure uniquement ce fichier dans vos sources.
// NkMain.h uniquement dans le .cpp qui dÃ©finit nkmain().
//
// Exemple d'usage complet :
//
//   #include <NKWindow/NkWindow.h>
//   #include <NKWindow/Core/NkMain.h>
//
//   int nkmain(const nkentseu::NkEntryState& state)
//   {
//       // 1. Init (crÃ©e EventImpl + GamepadSystem)
//       nkentseu::NkAppData app;
//       app.preferredRenderer = nkentseu::NkRendererApi::NK_SOFTWARE;
//       nkentseu::NkInitialise(app);
//
//       // 2. FenÃªtre (pas d'EventImpl Ã  passer)
//       nkentseu::NkWindowConfig cfg;
//       cfg.title = "Hello"; cfg.width = 800; cfg.height = 600;
//       nkentseu::Window window(cfg);
//
//       // 3. Safe Area (mobile : notch / home indicator)
//       auto insets = window.GetSafeAreaInsets();
//
//       // 4. NkRenderer
//       nkentseu::NkRenderer renderer(window);
//       renderer.SetBackgroundColor(0x1A1A2EFF);
//
//       // 5. Gamepad
//       auto& gp = nkentseu::NkGamepads();
//       gp.SetButtonCallback([](NkU32 idx, nkentseu::NkGamepadButton btn,
//                               nkentseu::NkButtonState st) { ... });
//
//       // 6. Boucle principale
//       auto& es = nkentseu::EventSystem::Instance();
//       while (window.IsOpen()) {
//           while (auto* ev = es.PollEvent()) {
//               if (ev->type == nkentseu::NkEventType::NK_WINDOW_CLOSE) {
//                   window.Close();
//               }
//           }
//           gp.PollGamepads();
//
//           renderer.BeginFrame();
//           renderer.SetPixel(100, 100, 0xFF5733FF);
//           renderer.EndFrame();
//           renderer.Present();
//       }
//       nkentseu::NkClose();
//       return 0;
//   }
// =============================================================================

#include "NkPlatformDetect.h"
#include "Core/NkTypes.h"
#include "Core/NkWindowConfig.h"
#include "Core/NkSurface.h"
#include "Core/NkSystem.h"
#include "Events/NkGamepadSystem.h"

#include "Events/NkWindowEvent.h"
#include "Events/NkKeyboardEvent.h"
#include "Events/NkMouseEvent.h"
#include "Events/NkTouchEvent.h"
#include "Events/NkGamepadEvent.h"
#include "Events/NkDropEvent.h"
#include "Core/NkEvents.h"

#include "Core/NkWindow.h"
#include "Events/NkEventSystem.h"
#include "Core/NkRenderer.h"
#include "Core/IRendererImpl.h"

#include "Core/NkDialogs.h"
#include "Core/NkEntry.h"
