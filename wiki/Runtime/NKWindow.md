# NKWindow

> Couche **Runtime** · Les fenêtres natives multi-OS : création et cycle de vie d'une
> fenêtre, surface de rendu, contexte GPU, configuration, dialogues système, launcher
> et mécanisme de point d'entrée portable.

NKWindow est la porte d'entrée de toute application graphique du moteur. C'est lui qui ouvre
une fenêtre native sur chaque plateforme (Windows, macOS, Linux X11/XCB/Wayland, Android,
iOS, Web, HarmonyOS, UWP, Xbox), expose la **surface** sur laquelle un backend graphique va
dessiner, négocie le **contexte GPU** (style GLFW/SDL), et fournit le **point d'entrée**
portable `nkmain()` qui remplace votre `main()`/`WinMain()`. Tout le reste du Runtime — le
rendu, l'UI, l'audio piloté par la boucle — démarre derrière une `NkWindow` ouverte.

Le module suit partout le **pattern Create/Destroy** : `NkInitialise()`/`NkClose()`,
`NkWindow::Create`/`Close`, `NkContextCreate`/`NkContextDestroy`. On apparie toujours.
Le système d'événements (poll, callbacks, gamepad) n'est PAS dans NKWindow : il vit dans le
module séparé **NKEvent**, simplement possédé par `NkWESystem` et inclus par le parapluie.

- **Namespace** : `nkentseu` (sous-namespaces référencés : `math::`, `graphics::`, `memory::`)
- **Header parapluie** : `#include "NKWindow/NKWindow.h"`
- **Point d'entrée** : `#include "NKWindow/NKMain.h"` (dans UN seul `.cpp`)

---

## Par où commencer

Selon ce que vous cherchez à faire :

| Besoin | Page |
|--------|------|
| Ouvrir une fenêtre, gérer taille/titre/visibilité, plein écran, souris | [La fenêtre](NKWindow/Window.md) |
| Récupérer la surface et créer un contexte GPU (OpenGL/Vulkan/DX/Metal) | [La fenêtre](NKWindow/Window.md) |
| Configurer la fenêtre (taille, comportement, apparence, hints) | [La fenêtre](NKWindow/Window.md) |
| Énumérer les moniteurs, gérer le DPI et le hot-plug d'écran | [La fenêtre](NKWindow/Window.md) |
| Ouvrir une boîte de dialogue fichier / message / sélecteur de couleur | [Dialogues & launcher](NKWindow/Dialogs-Launcher.md) |
| Ouvrir une URL, un fichier ou un dossier avec l'application système par défaut | [Dialogues & launcher](NKWindow/Dialogs-Launcher.md) |
| Écrire le point d'entrée `nkmain()` portable et configurer l'AppData | [Point d'entrée](NKWindow/EntryPoint.md) |

Chaque page suit la même structure : un **tutoriel** narratif, un **aperçu** tabulaire de
l'API, puis une **référence-cours** où chaque type/fonction est expliqué avec ses idiomes et
ses pièges réels (ownership des événements, hints surface, pixel format, sécurité launcher…).

---

## Aperçu des familles

- **Fenêtre** (`Core/NkWindow.h`, `Core/NkWindowConfig.h`) — `NkWindow` : façade
  cross-plateforme non copiable mais **movable**. Cycle de vie `Create`/`Close`, taille,
  position, titre, visibilité, plein écran, orientation (mobile), souris, safe area, progress
  bar, énumération des moniteurs (hot-plug + DPI runtime). `NkWindowConfig` regroupe toute la
  configuration (position/taille, comportement, apparence, identité, options natives, hints).
- **Surface & types** (`Core/NkSurface.h`, `Core/NkSurfaceHint.h`, `Core/NkTypes.h`) —
  `NkSurfaceDesc` est le **contrat unique** entre `NkWindow` et un backend graphique (handles
  natifs conditionnels par plateforme). `NkSurfaceHints` porte des hints opaques pré/post
  création (utiles surtout OpenGL/GLX Linux). `NkPixelFormat`, `NkError`.
- **Contexte GPU** (`Core/NkContext.h`) — API libre-fonctions style GLFW/SDL :
  `NkContextInit`, `NkContextWindowHint`, `NkContextApplyWindowHints`, `NkContextCreate`,
  `NkContextMakeCurrent`, `NkContextSwapBuffers`, `NkContextGetProcAddress`. Mode
  surface-only pour Vulkan/DX/Metal/Software.
- **Système & registre** (`Core/NkWESystem.h`) — `NkWESystem` : singleton propriétaire du
  système d'événements, du gamepad et du registre de fenêtres. Free functions globales
  `NkInitialise`/`NkClose`/`NkEvents`/`NkGamepads`.
- **Événements fenêtre** (`Core/NkEvent.h`, `Core/NkEventSystem.h`) — *intégration* du module
  **NKEvent** (dépendance, pas un sous-composant) : `NkEventSystem` (poll, callbacks typés,
  RAII guard, ring buffer double-priorité), pompage des events OS.
- **Dialogues & launcher** (`Core/NkDialogs.h`, `Core/NkLauncher.h`) — `NkDialogs` (file/save
  dialog, message box, color picker, via `NkString`) et `NkLauncher` (ouvrir URL/fichier/
  dossier, via `const char*`, `noexcept`). Classes purement statiques.
- **Point d'entrée** (`NKMain.h`, `Core/NkMain.h`, `Core/NkEntry.h`, `EntryPoints/…`) —
  inversion d'entrée façon SDL/SFML : on implémente `int nkmain(const NkEntryState&)` au lieu
  de `main`. `NkEntryState` porte args + handles natifs par plateforme. Helpers de
  configuration d'`NkAppData` (override / fallback / updater) et macros d'enregistrement.

---

## Index des headers

| Header | Contenu | Documenté dans |
|--------|---------|----------------|
| `NKWindow.h` | Parapluie principal (agrège Core/* + NKEvent/*). | — |
| `NKMain.h` | Parapluie du point d'entrée (NkMain.h + NkEntry.h). | [Point d'entrée](NKWindow/EntryPoint.md) |
| `Core/NkWindow.h` | Classe `NkWindow` (cycle de vie, taille, surface, moniteurs). | [La fenêtre](NKWindow/Window.md) |
| `Core/NkWindowConfig.h` | `NkWindowConfig`, `NkWebInputOptions`, `NkNativeWindowOptions`. | [La fenêtre](NKWindow/Window.md) |
| `Core/NkTypes.h` | `NkPixelFormat`, `NkError`. | [La fenêtre](NKWindow/Window.md) |
| `Core/NkSurface.h` | `NkSurfaceDesc` (contrat backend graphique). | [La fenêtre](NKWindow/Window.md) |
| `Core/NkSurfaceHint.h` | `NkSurfaceHintKey`, `NkSurfaceHint`, `NkSurfaceHints`. | [La fenêtre](NKWindow/Window.md) |
| `Core/NkContext.h` | Contexte GPU (free functions style GLFW/SDL). | [La fenêtre](NKWindow/Window.md) |
| `Core/NkWESystem.h` | `NkWESystem`, `NkAppData`, `NkInitialise`/`NkClose`. | [La fenêtre](NKWindow/Window.md) |
| `Core/NkEvent.h` | Maître d'inclusion du module NKEvent. | [La fenêtre](NKWindow/Window.md) |
| `Core/NkEventSystem.h` | `NkEventSystem` (vit sous NKEvent/, inclus ici). | [La fenêtre](NKWindow/Window.md) |
| `Core/NkDialogs.h` | `NkDialogs`, `NkDialogResult`. | [Dialogues & launcher](NKWindow/Dialogs-Launcher.md) |
| `Core/NkLauncher.h` | `NkLauncher`. | [Dialogues & launcher](NKWindow/Dialogs-Launcher.md) |
| `Core/NkMain.h` | Sélecteur de plateforme du point d'entrée. | [Point d'entrée](NKWindow/EntryPoint.md) |
| `Core/NkEntry.h` | `NkEntryState`, `gState`, helpers/macros AppData, runtime init. | [Point d'entrée](NKWindow/EntryPoint.md) |
| `EntryPoints/NkWindowsDesktop.h` | `WinMain` Win32 (variante de référence). | [Point d'entrée](NKWindow/EntryPoint.md) |

---

[← Couche Runtime](README.md) · [Index du wiki](../README.md)
