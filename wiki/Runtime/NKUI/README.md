# NKUI — documentation détaillée

Le module **NKUI**, partie par partie. Pour une vue d'ensemble et un guide « par où
commencer », voir le récap : [../NKUI.md](../NKUI.md).

Chaque page suit la même structure : une **présentation** du sous-système, un **aperçu**
tabulaire de l'API, puis une **référence** où chaque type et méthode est expliqué avec ses
idiomes et ses pièges concrets (immediate-mode, tableaux à taille fixe, Create/Destroy…).

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [Context.md](Context.md) | Démarrer une frame, router souris/clavier/touch, états hot/active/focus, piles d'ID et de style, hit-testing, types de base (`NkUIID`/`NkVec2`/`NkRect`). | `NkUI.h`, `NkUIExport.h`, `NkUIMath.h`, `NkUIInput.h`, `NkUIContext.h` |
| [Widgets.md](Widgets.md) | Tous les widgets (texte, boutons, sliders, champs, combos, arbres, tables…), les menus (menubar/déroulant/contextuel/popup/modale) et les fenêtres déplaçables. | `NkUIWidgets.h`, `NkUIMenu.h`, `NkUIWindow.h` |
| [Layout-Dock.md](Layout-Dock.md) | Placement flex-like (contraintes, lignes/colonnes/grilles/scroll/splitter), arbre de docking, animations (tweens + 24 easings), sérialisation de layout. | `NkUILayout.h`, `NkUILayout2.h`, `NkUIDock.h`, `NkUIAnimation.h` |
| [Rendering.md](Rendering.md) | Émettre la géométrie (draw list + path builder), écrire/brancher un backend de rendu, charger des polices via NKFont, configurer le thème complet. | `NkUIDrawList.h`, `NkUIRenderer.h`, `NkUIFont.h`, `NkUIFontBridge.h`, `NkUITheme.h` |
| [Tools.md](Tools.md) | Widgets composites d'éditeur : navigateur de fichiers + content browser, gizmo 2D/3D, arbre callback-first, viewport 3D embarqué. | `NkUITools.h`, `Tools/FileSystem/NkUIFileBrowser.h`, `Tools/Gizmo/NkUIGizmo.h`, `Tools/Tree/NkUITree.h`, `Tools/Viewport/NkUIViewport3D.h` |

[← Récap NKUI](../NKUI.md) · [← Couche Runtime](../README.md)
