# NKWindow — documentation détaillée

Le module **NKWindow**, partie par partie. Pour une vue d'ensemble et un guide « par où
commencer », voir le récap : [../NKWindow.md](../NKWindow.md).

Chaque page suit la même structure : un **tutoriel** narratif, un **aperçu** tabulaire de
l'API, puis une **référence-cours** où chaque type/fonction est expliqué avec ses idiomes et
ses pièges réels (ownership des événements, hints surface, pixel format, sécurité launcher…).

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [Window.md](Window.md) | Ouvrir une fenêtre et gérer son cycle de vie : taille, position, titre, visibilité, plein écran, souris, moniteurs (hot-plug/DPI) ; récupérer la surface et créer un contexte GPU ; configurer via `NkWindowConfig` et le système global `NkWESystem` + intégration des événements fenêtre. | `Core/NkWindow.h`, `Core/NkWindowConfig.h`, `Core/NkTypes.h`, `Core/NkSurface.h`, `Core/NkSurfaceHint.h`, `Core/NkContext.h`, `Core/NkWESystem.h`, `Core/NkEvent.h`, `Core/NkEventSystem.h`, `NKWindow.h` |
| [Dialogs-Launcher.md](Dialogs-Launcher.md) | Boîtes de dialogue natives (ouverture/sauvegarde de fichier, message box, sélecteur de couleur) via `NkDialogs`, et ouverture d'URL/fichier/dossier avec l'application système par défaut via `NkLauncher` (avec le piège sécurité `system()`). | `Core/NkDialogs.h`, `Core/NkLauncher.h` |
| [EntryPoint.md](EntryPoint.md) | Le mécanisme de point d'entrée portable : implémenter `nkmain()` au lieu de `main`, le `NkEntryState` par plateforme, la configuration d'`NkAppData` (override/fallback/updater + macros), l'init/shutdown runtime, et la variante Windows `WinMain` comme exemple du patron commun. | `NKMain.h`, `Core/NkMain.h`, `Core/NkEntry.h`, `EntryPoints/NkWindowsDesktop.h` |

[← Récap NKWindow](../NKWindow.md) · [← Couche Runtime](../README.md)
