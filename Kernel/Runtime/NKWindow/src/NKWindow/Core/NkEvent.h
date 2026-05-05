#pragma once

// =============================================================================
// NkEvents.h
// En-tête maître — inclut tout le système d'événements NKWindow.
//
// Usage simple :
//   #include "NKWindow/Core/NkEvent.h"
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

#include "NKEvent/NkEvent.h"           // Base abstraite + énumérations fondamentales
#include "NKEvent/NkWindowEvent.h"    // Fenêtre  (create/close/resize/dpi/thème…)
#include "NKEvent/NkKeyboardEvent.h"  // Clavier  (press/release/repeat/text input)
#include "NKEvent/NkMouseEvent.h"     // Souris   (move/raw/button/wheel/enter/leave)
#include "NKEvent/NkTouchEvent.h"     // Tactile  (touch begin/move/end + gestes)
#include "NKEvent/NkGamepadEvent.h"   // Manette  (connect/button/axis/rumble)
#include "NKEvent/NkGamepadMappingPersistence.h"
#include "NKEvent/NkDropEvent.h"      // Drop     (enter/over/leave/file/text/image)
#include "NKEvent/NkCustomEvent.h"     // Générique (payload inline, userData)
#include "NKEvent/NkSystemEvent.h"     // Système   (low-level OS events)
#include "NKEvent/NkTransferEvent.h"   // Transfert (fich
#include "NKEvent/NkGenericHidEvent.h"
#include "NKEvent/NkGenericHidMapper.h"
#include "NKEvent/NkGraphicsEvent.h"   // Graphique (perte de contexte, etc.)
#include "NKEvent/NkApplicationEvent.h"
#include "NKEvent/NkDropSystem.h"
#include "NKEvent/NkEventState.h"
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkGamepadSystem.h"
#include "NKEvent/NkEventDispatcher.h" // Dispatcheur d'événements (pour callbacks)
