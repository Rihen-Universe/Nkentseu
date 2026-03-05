#pragma once

// =============================================================================
// NkEvents.h
// En-tête maître — inclut tout le système d'événements NKWindow.
//
// Usage simple :
//   #include "NKWindow/Events/NkEvents.h"
//
//   void OnEvent(nkentseu::NkEvent& ev) {
//       if (auto* e = ev.As<nkentseu::NkKeyPressEvent>()) {
//           // e->GetKey(), e->GetModifiers()…
//       }
//       if (auto* e = ev.As<nkentseu::NkWindowResizeEvent>()) {
//           // e->GetWidth(), e->GetHeight()…
//       }
//       switch (ev.GetType()) {
//           case nkentseu::NkEventType::NK_MOUSE_MOVE: …
//           case nkentseu::NkEventType::NK_TOUCH_BEGIN: …
//       }
//   }
// =============================================================================

#include "NKWindow/Events/NkEvent.h"           // Base abstraite + énumérations fondamentales
#include "NKWindow/Events/NkWindowEvent.h"    // Fenêtre  (create/close/resize/dpi/thème…)
#include "NKWindow/Events/NkKeyboardEvent.h"  // Clavier  (press/release/repeat/text input)
#include "NKWindow/Events/NkMouseEvent.h"     // Souris   (move/raw/button/wheel/enter/leave)
#include "NKWindow/Events/NkTouchEvent.h"     // Tactile  (touch begin/move/end + gestes)
#include "NKWindow/Events/NkGamepadEvent.h"   // Manette  (connect/button/axis/rumble)
#include "NKWindow/Events/NkGamepadMappingPersistence.h"
#include "NKWindow/Events/NkDropEvent.h"      // Drop     (enter/over/leave/file/text/image)
#include "NKWindow/Events/NkCustomEvent.h"     // Générique (payload inline, userData)
#include "NKWindow/Events/NkSystemEvent.h"     // Système   (low-level OS events)
#include "NKWindow/Events/NkTransferEvent.h"   // Transfert (fich
#include "NKWindow/Events/NkGenericHidEvent.h"
#include "NKWindow/Events/NkGenericHidMapper.h"
#include "NKWindow/Events/NkGraphicsEvent.h"   // Graphique (perte de contexte, etc.)
#include "NKWindow/Events/NkApplicationEvent.h"
#include "NKWindow/Events/NkDropSystem.h"
#include "NKWindow/Events/NkEventState.h"
#include "NKWindow/Events/NkEventSystem.h"
#include "NKWindow/Events/NkGamepadSystem.h"
#include "NKWindow/Events/NkEventDispatcher.h" // Dispatcheur d'événements (pour callbacks)
