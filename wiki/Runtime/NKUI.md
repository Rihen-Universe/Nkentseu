# NKUI

> Couche **Runtime** · L'interface utilisateur en mode immédiat : contexte et input,
> widgets, fenêtres, menus, layout, docking, animation, rendu (draw list, font, thème)
> et outils (navigateur de fichiers, gizmo, arbre, viewport 3D).

NKUI dessine toute l'UI du moteur : panneaux d'éditeur, HUD, inspecteurs, boîtes de dialogue.
Le modèle est **immediate-mode** (comme Dear ImGui) : à chaque frame on (re)déclare les
widgets, qui calculent leur rectangle, gèrent l'interaction et émettent leur géométrie — pas
d'arbre d'objets à maintenir, seulement vos données métier. Particularité Nkentseu : NKUI est
**multi-instance explicite** (plusieurs `NkUIContext` peuvent coexister, ce n'est pas un
singleton global) et **zero-allocation par frame** (toutes les piles internes sont des tableaux
à taille fixe).

Le cœur d'une frame tient en trois temps : `ctx.BeginFrame(input, dt)` → appels de widgets →
`ctx.EndFrame()`, puis un backend de rendu consomme les `NkUIDrawList` via `renderer.Submit(ctx)`.
Tout passe par un `NkUIContext` (état), une `NkUILayoutStack` (placement) et un `NkUIDrawList`
(géométrie). Les widgets, fenêtres, menus et outils sont des **collections de méthodes statiques**
prenant ces objets en paramètre.

- **Namespace** : `nkentseu::nkui`
- **Header parapluie** : `#include "NKUI/NkUI.h"`

> **Convention Create/Destroy** (règle moteur NKMemory) : les objets à ressources
> (`NkUIContext`, `NkUIDrawList`, `NkUIWindowManager`, `NkUIDockManager`, `NkUIFontManager`,
> `NkUICPURenderer`) s'initialisent avec `Init(...)` et se libèrent avec `Destroy()`. Jamais
> de `new`/`delete`. Les utilitaires purement statiques (`NkUILayout`, `NkUIEasing`, widgets,
> outils) n'ont pas de cycle de vie.

---

## Par où commencer

Selon ce que vous cherchez à faire :

| Besoin | Partie |
|--------|--------|
| Démarrer une frame, router souris/clavier, gérer l'état immediate-mode | [Contexte, Input, Math](NKUI/Context.md) |
| Afficher boutons, sliders, champs, listes, arbres, tables | [Widgets, Menus, Fenêtres](NKUI/Widgets.md) |
| Créer des fenêtres déplaçables, des menus, des popups, des modales | [Widgets, Menus, Fenêtres](NKUI/Widgets.md) |
| Positionner les widgets (lignes/colonnes/grilles), docker, animer | [Layout & Docking](NKUI/Layout-Dock.md) |
| Dessiner des primitives, brancher une font, écrire un backend, thémer | [Rendu](NKUI/Rendering.md) |
| Intégrer un navigateur de fichiers, un gizmo, un arbre, un viewport 3D | [Outils](NKUI/Tools.md) |

Chaque page suit la même structure : une **présentation** du sous-système, un **aperçu** de
l'API, puis une **référence** détaillée des types, méthodes et pièges concrets.

---

## Aperçu des familles

- **Contexte & input** (`NkUIContext.h`, `NkUIInput.h`, `NkUIExport.h`, `NkUIMath.h`) —
  `NkUIContext` (objet central : IDs hot/active/focus, piles d'ID et de style, animations,
  curseur de layout, thème, draw lists en couches), `NkUIInputState` (état souris/clavier/touch
  rempli par l'appelant), et les types de base `NkUIID`/`NkVec2`/`NkRect`/`NkColor` + helpers
  géométriques (`NkRectContains`, `NkRectIntersect`).
- **Widgets** (`NkUIWidgets.h`) — `NkUI`, namespace statique de tous les widgets : texte,
  boutons, checkbox/radio/toggle, sliders, champs de saisie, combos, listes, arbres, tables,
  progress bars, color picker, tooltips, et les conteneurs de layout (`BeginRow`/`BeginGroup`…).
- **Fenêtres & menus** (`NkUIWindow.h`, `NkUIMenu.h`) — `NkUIWindowManager`/`NkUIWindow`
  (fenêtres déplaçables/redimensionnables, flags, z-order) ; `NkUIMenu` (menubar, menus
  déroulants, contextuels, popups, modales).
- **Layout & docking** (`NkUILayout.h`, `NkUILayout2.h`, `NkUIDock.h`, `NkUIAnimation.h`) —
  `NkUILayoutStack`/`NkUILayout` (placement flex-like, contraintes, scroll, splitter),
  `NkUIDockManager` (arbre de dock style éditeur), `NkUIAnimator`/`NkUIEasing` (tweens + 24
  fonctions d'easing), plus sérialisation de layout et color picker complet (`NkUILayout2.h`).
- **Rendu** (`NkUIDrawList.h`, `NkUIRenderer.h`, `NkUIFont.h`, `NkUIFontBridge.h`, `NkUITheme.h`) —
  `NkUIDrawList` (primitives 2D + path builder, agnostique du backend), `NkUIRenderer` (contrat
  backend) et `NkUICPURenderer` (rendu logiciel), `NkUIFont`/`NkUIFontManager` + pont NKFont
  (`NkUIFontBridge`), et `NkUITheme` (palette, métriques, polices, animations).
- **Outils** (`NkUITools.h`, `Tools/NkUIFileBrowser.h`, `Tools/NkUIGizmo.h`, `Tools/NkUITree.h`,
  `Tools/NkUIViewport3D.h`) — widgets composites d'éditeur : navigateur de fichiers + content
  browser, gizmo 2D/3D, arbre callback-first, viewport 3D embarqué.

---

## Index des headers

| Header | Contenu | Documenté dans |
|--------|---------|----------------|
| `NkUI.h` | Parapluie (inclut tout). | — |
| `NkUIExport.h` | Macros `NKUI_API`/`NKUI_INLINE`, types `NkUIID`/`NkVec2`/`NkRect`/`NkColor`, helpers géométriques. | [Contexte, Input, Math](NKUI/Context.md) |
| `NkUIMath.h` | Agrégateur math (vers `NkUIExport.h` + NKMath). | [Contexte, Input, Math](NKUI/Context.md) |
| `NkUIInput.h` | `NkUIInputState` (souris/clavier/touch). | [Contexte, Input, Math](NKUI/Context.md) |
| `NkUIContext.h` | `NkUIContext`, IDs, piles, animations, styles, hit-testing. | [Contexte, Input, Math](NKUI/Context.md) |
| `NkUIWidgets.h` | `NkUI` : tous les widgets + layout inline. | [Widgets, Menus, Fenêtres](NKUI/Widgets.md) |
| `NkUIMenu.h` | `NkUIMenu` : menubar, menus, popups, modales. | [Widgets, Menus, Fenêtres](NKUI/Widgets.md) |
| `NkUIWindow.h` | `NkUIWindowManager`, `NkUIWindow`, flags fenêtre. | [Widgets, Menus, Fenêtres](NKUI/Widgets.md) |
| `NkUILayout.h` | `NkUILayoutStack`, `NkUILayout`, `NkUIConstraint`. | [Layout & Docking](NKUI/Layout-Dock.md) |
| `NkUILayout2.h` | Sérialisation layout, color picker, font NKFont, backend GL. | [Layout & Docking](NKUI/Layout-Dock.md) |
| `NkUIDock.h` | `NkUIDockManager`, arbre de dock, dropzones. | [Layout & Docking](NKUI/Layout-Dock.md) |
| `NkUIAnimation.h` | `NkUIAnimator`, `NkUITween`, `NkUIEasing`, `NkEase`. | [Layout & Docking](NKUI/Layout-Dock.md) |
| `NkUIDrawList.h` | `NkUIDrawList`, primitives 2D, path builder. | [Rendu](NKUI/Rendering.md) |
| `NkUIRenderer.h` | `NkUIRenderer` (contrat), `NkUICPURenderer`. | [Rendu](NKUI/Rendering.md) |
| `NkUIFont.h` | `NkUIFont`, `NkUIFontAtlas`, `NkUIFontManager`. | [Rendu](NKUI/Rendering.md) |
| `NkUIFontBridge.h` | Pont NKFont, loaders custom (callbacks). | [Rendu](NKUI/Rendering.md) |
| `NkUITheme.h` | `NkUITheme`, palette, métriques, `NkUIWidgetType`/`State`. | [Rendu](NKUI/Rendering.md) |
| `NkUITools.h` | Agrégateur Gizmo + Tree + FileBrowser (PAS Viewport3D). | [Outils](NKUI/Tools.md) |
| `Tools/FileSystem/NkUIFileBrowser.h` | `NkUIFileBrowser`, `NkUIContentBrowser`, provider FS. | [Outils](NKUI/Tools.md) |
| `Tools/Gizmo/NkUIGizmo.h` | `NkUIGizmo` (manipulation 2D/3D, grilles). | [Outils](NKUI/Tools.md) |
| `Tools/Tree/NkUITree.h` | `NkUITree` callback-first (multi-select, DnD). | [Outils](NKUI/Tools.md) |
| `Tools/Viewport/NkUIViewport3D.h` | `NkUIViewport3D` (viewport 3D embarqué). | [Outils](NKUI/Tools.md) |

---

[← Couche Runtime](README.md) · [Index du wiki](../README.md)
