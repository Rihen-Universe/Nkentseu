# NKEvent

> Couche **Runtime** · Le système d'événements typés du moteur : bus/dispatcher,
> état d'entrée, clavier/souris/tactile, manettes & HID, événements application/fenêtre/système.

Tout ce qui « arrive » à une application passe par NKEvent : une touche pressée, la souris qui
bouge, la fenêtre redimensionnée, une manette branchée, un fichier déposé, l'OS qui signale une
batterie faible. Le module définit une hiérarchie d'**événements typés** dérivant tous de
`NkEvent`, un **système d'événements** (`NkEventSystem`) qui pompe l'OS et distribue ces
événements, et un **état d'entrée** (`NkEventState`) interrogeable à tout instant. C'est la
frontière entre le monde natif (Win32, X11, Cocoa, Android…) et le reste du moteur.

On consomme NKEvent de deux façons complémentaires : en **pull** (boucle `PollEvent()` +
`event.Is<T>()` / `event.As<T>()`, ou l'instance globale `NkInput` pour lire l'état clavier/souris/manette),
et en **push** (callbacks typés `AddEventCallback<T>` ou routage local via `NkEventDispatcher`).
Le dispatch est type-safe et O(1) : chaque événement concret expose un `NkEventType` unique, et
`Is<T>` / `As<T>` reposent sur `T::GetStaticType()`.

- **Namespace** : `nkentseu` (pas de sous-namespace `event`)
- **Header parapluie** : `#include "NKEvent/NkEvent.h"` (les headers concrets se composent ; `NkEventSystem.h` tire tous les événements)

> Note mémoire : `NkEvent::Clone()` alloue via `new` (heap CRT) et le caller doit `delete` — c'est
> l'un des rares endroits du moteur qui n'utilise pas NKMemory. Le pointeur rendu par `PollEvent()`
> n'est valide que jusqu'au prochain `PollEvent()` ; pour conserver un événement, utiliser `PollEventCopy()`.

---

## Par où commencer

Selon ce que vous cherchez à faire :

| Besoin | Page |
|--------|------|
| Pomper les événements, dispatcher, lire l'état d'entrée global | [Le cœur du système](NKEvent/Events.md) |
| Réagir au clavier, à la souris, au tactile et aux gestes | [Événements d'entrée](NKEvent/Input-Events.md) |
| Gérer les manettes (rumble, remapping) et les périphériques HID | [Manettes & HID](NKEvent/Gamepad-Hid.md) |
| Réagir au cycle de vie app/fenêtre, au système, au drag-drop, à la safe area | [Application, fenêtre & système](NKEvent/App-Window-Events.md) |

Chaque page suit la même structure : un **tutoriel** narratif, un **aperçu** tabulaire de
l'API, puis une **référence** où chaque type, événement et piège est expliqué.

---

## Aperçu des familles

- **Cœur du système** (`NkEvent.h`, `NkEventDispatcher.h`, `NkEventSystem.h`, `NkEventState.h`) —
  la classe de base `NkEvent` (catégories, type, `Clone`, `Is<T>`/`As<T>`), le `NkEventDispatcher`
  (push, routeur type-safe), `NkInputQuery`/`NkInput` (pull), les managers d'actions/axes
  (`NkActionManager`, `NkAxisManager`), le `NkEventSystem` (pompe OS, callbacks, ring buffer
  dual-priorité) et `NkEventState` (snapshots clavier/souris/tactile/manette).
- **Entrée** (`NkKeyboardEvent.h`, `NkMouseEvent.h`, `NkTouchEvent.h`, `NkKeycodeMap.h`) —
  événements clavier (press/repeat/release, saisie texte), souris (move/boutons/molette/scroll/
  enter-leave/capture), tactile (begin/move/end/cancel) et gestes (pinch/rotate/pan/swipe/tap/
  long-press), plus le mapping cross-platform des keycodes (`NkKey`, `NkScancode`, `NkKeycodeMap`).
- **Manettes & HID** (`NkGamepadEvent.h`, `NkGamepadSystem.h`, `NkGamepadMappingPersistence.h`,
  `NkGenericHidEvent.h`, `NkGenericHidMapper.h`) — événements et système manette (layout universel
  Xbox-compatible, deadzone, remapping par slot, rumble, LED, persistance `.nkmap`) et HID générique
  (événements + mapper autonome par `deviceId`).
- **Application / Fenêtre / Système** (`NkApplicationEvent.h`, `NkWindowEvent.h`, `NkWindowId.h`,
  `NkSystemEvent.h`, `NkGraphicsEvent.h`, `NkCustomEvent.h`, `NkDropEvent.h`, `NkDropSystem.h`,
  `NkTransferEvent.h`, `NkSafeArea.h`) — cycle de vie app (launch/tick/update/render/close),
  fenêtre (resize/move/focus/dpi/thème/surface…), système (power/locale/display/memory),
  graphique, événements custom, drag-drop, transfert de fichiers et safe area mobile.

---

## Index des headers

| Header | Contenu | Documenté dans |
|--------|---------|----------------|
| `NkEvent.h` | Parapluie ; classe de base `NkEvent`, `NkEventCategory`, `NkEventType`, macros. | [Le cœur du système](NKEvent/Events.md) |
| `NkEventDispatcher.h` | `NkEventDispatcher`, `NkInputQuery`/`NkInput`, `NkActionManager`, `NkAxisManager`. | [Le cœur du système](NKEvent/Events.md) |
| `NkEventSystem.h` | `NkEventSystem`, ring buffer, callbacks, `NkCallbackGuard`. | [Le cœur du système](NKEvent/Events.md) |
| `NkEventState.h` | `NkEventState` + snapshots clavier/souris/tactile/manette. | [Le cœur du système](NKEvent/Events.md) |
| `NkKeyboardEvent.h` | `NkKey`, `NkScancode`, `NkModifierState`, events clavier + texte. | [Événements d'entrée](NKEvent/Input-Events.md) |
| `NkMouseEvent.h` | `NkMouseButton`, events souris (move/boutons/molette/scroll/capture). | [Événements d'entrée](NKEvent/Input-Events.md) |
| `NkTouchEvent.h` | `NkTouchPoint`, events tactiles + gestes. | [Événements d'entrée](NKEvent/Input-Events.md) |
| `NkKeycodeMap.h` | `NkKeycodeMap` : conversions codes natifs → `NkKey`. | [Événements d'entrée](NKEvent/Input-Events.md) |
| `NkGamepadEvent.h` | `NkGamepadType/Button/Axis`, `NkGamepadInfo`, events manette. | [Manettes & HID](NKEvent/Gamepad-Hid.md) |
| `NkGamepadSystem.h` | `NkGamepadSystem`, `NkIGamepad`, remapping, rumble. | [Manettes & HID](NKEvent/Gamepad-Hid.md) |
| `NkGamepadMappingPersistence.h` | Profils de mapping persistants (`.nkmap`). | [Manettes & HID](NKEvent/Gamepad-Hid.md) |
| `NkGenericHidEvent.h` | `NkHidUsagePage`, `NkHidDeviceInfo`, events HID génériques. | [Manettes & HID](NKEvent/Gamepad-Hid.md) |
| `NkGenericHidMapper.h` | `NkGenericHidMapper` : mapping HID par `deviceId`. | [Manettes & HID](NKEvent/Gamepad-Hid.md) |
| `NkApplicationEvent.h` | Cycle de vie app (launch/tick/update/render/close). | [Application, fenêtre & système](NKEvent/App-Window-Events.md) |
| `NkWindowEvent.h` | Events fenêtre (resize/move/focus/dpi/thème/surface…). | [Application, fenêtre & système](NKEvent/App-Window-Events.md) |
| `NkWindowId.h` | `NkWindowId`, `NK_INVALID_WINDOW_ID`. | [Application, fenêtre & système](NKEvent/App-Window-Events.md) |
| `NkSystemEvent.h` | Events système (power/locale/display/memory/thème/a11y). | [Application, fenêtre & système](NKEvent/App-Window-Events.md) |
| `NkGraphicsEvent.h` | Events graphiques (context ready/lost/resize, frame, VSync). | [Application, fenêtre & système](NKEvent/App-Window-Events.md) |
| `NkCustomEvent.h` | Events custom (payload inline / dynamique / texte). | [Application, fenêtre & système](NKEvent/App-Window-Events.md) |
| `NkDropEvent.h` | Drag & drop (fichiers/texte/image, enter/over/leave). | [Application, fenêtre & système](NKEvent/App-Window-Events.md) |
| `NkDropSystem.h` | `NkEnableDropTarget` / `NkDisableDropTarget`. | [Application, fenêtre & système](NKEvent/App-Window-Events.md) |
| `NkTransferEvent.h` | Transfert de fichiers (begin/progress/complete/error…). | [Application, fenêtre & système](NKEvent/App-Window-Events.md) |
| `NkSafeArea.h` | `NkSafeAreaInsets`, `NkScreenOrientation`. | [Application, fenêtre & système](NKEvent/App-Window-Events.md) |
| `NkEventApi.h` | Macros d'export. | — |

---

[← Couche Runtime](README.md) · [Index du wiki](../README.md)
