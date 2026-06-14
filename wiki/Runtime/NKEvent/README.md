# NKEvent — documentation détaillée

Le module **NKEvent**, partie par partie. Pour une vue d'ensemble et un guide « par où
commencer », voir le récap : [../NKEvent.md](../NKEvent.md).

Chaque page suit la même structure : un **tutoriel** narratif, un **aperçu** tabulaire de
l'API, puis une **référence** où chaque type, événement et piège est expliqué avec ses cas
d'usage concrets (boucle de jeu, input, fenêtrage, mobile…).

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [Events.md](Events.md) | Le cœur : événement de base, dispatcher/bus, système d'événements (`PollEvent`, `Is<T>`/`As<T>`), état d'entrée (`NkInput`), actions & axes. | `NkEvent.h`, `NkEventDispatcher.h`, `NkEventSystem.h`, `NkEventState.h` |
| [Input-Events.md](Input-Events.md) | Clavier, souris, tactile et gestes ; mapping cross-platform des keycodes (`NkKey`, `NkScancode`). | `NkKeyboardEvent.h`, `NkMouseEvent.h`, `NkTouchEvent.h`, `NkKeycodeMap.h` |
| [Gamepad-Hid.md](Gamepad-Hid.md) | Manettes (événements, système, deadzone, remapping, rumble, persistance des mappings) et HID générique (événements, mapper par `deviceId`). | `NkGamepadEvent.h`, `NkGamepadSystem.h`, `NkGamepadMappingPersistence.h`, `NkGenericHidEvent.h`, `NkGenericHidMapper.h` |
| [App-Window-Events.md](App-Window-Events.md) | Cycle de vie application/fenêtre/système/graphique, événements custom, drag-drop, transfert, safe area, identifiant de fenêtre. | `NkApplicationEvent.h`, `NkWindowEvent.h`, `NkWindowId.h`, `NkSystemEvent.h`, `NkGraphicsEvent.h`, `NkCustomEvent.h`, `NkDropEvent.h`, `NkDropSystem.h`, `NkTransferEvent.h`, `NkSafeArea.h` |

[← Récap NKEvent](../NKEvent.md) · [← Couche Runtime](../README.md)
