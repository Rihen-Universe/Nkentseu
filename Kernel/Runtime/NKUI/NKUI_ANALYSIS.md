# Analyse détaillée du module NKUI

> Date : 2026-03-28
> Auteur : Analyse automatique (Claude Sonnet 4.6)

---

## Vue d'ensemble

NKUI est une **UI en mode immédiat** (à la Dear ImGui) entièrement custom, sans dépendance externe,
multi-backend (OpenGL, Vulkan, DX11, DX12, Software). Bibliothèque ambitieuse et bien structurée
pour un moteur de jeu.

---

## Architecture des fichiers

```
src/NkUI/
├── NkUIExport.h          — Macros DLL, alias types NKMath, helpers géométriques
├── NkUIInput.h           — État input complet (souris 5 boutons, clavier 128 touches, touch 10 points)
├── NkUITheme.h           — Palette 40+ couleurs, métriques, polices, 4 presets (Default/Dark/Minimal/HighContrast)
├── NkUIAnimation.h/.cpp  — 24 fonctions easing, NkUIAnimator (256 tweens), effets Shake/Pulse/FadeIn
├── NkUIDrawList.h/.cpp   — Géométrie CPU (vtx/idx/cmds), 4 layers, path builder, primitives
├── NkUIContext.h/.cpp    — État global, IDs FNV-1a, style stack, animations, 4 drawlists
├── NkUIMath.h/.cpp       — Helpers mathématiques UI
├── NkUIRenderer.h/.cpp   — Interface abstraite GPU + NkUICPURenderer (software)
├── NkUILayout.h/.cpp     — Layout flex-like (Row/Column/Grid/Split/Scroll/Tab)
├── NkUIFont.h/.cpp       — Atlas glyphes, police bitmap intégrée ASCII 32-127
├── NkUIWidgets.h/.cpp    — 40+ widgets stateless (Button, Slider, Input, Combo, Tree, Table…)
├── NkUIWindow.h/.cpp     — Fenêtres flottantes (titre, resize 8-way, z-order, scroll)
├── NkUIMenu.h/.cpp       — MenuBar, Menu déroulant, ContextMenu, Popup, Modal
├── NkUIDock.h/.cpp       — Docking arbre binaire (split H/V, tabs, drag & drop, detach)
├── NkUILayout2.h/.cpp    — Sérialisation JSON layout + NkUIColorPickerFull
├── NkUITools.h           — Header d'agrégation pour Tools/
└── Tools/
    ├── Gizmo/
    │   ├── NkUIGizmo.h/.cpp  — Gizmo TRS 2D/3D, grille 2D/3D, snapping
    ├── FileSystem/
    │   ├── NkUIFileBrowser.h/.cpp  — Browser fichiers callback-first + provider NKFileSystem
    └── Tree/
        ├── NkUITree.h/.cpp   — Arbre générique callbacks, DnD, rename, multi-select
```

---

## Points forts

### Architecture propre et cohérente
Séparation stricte des couches : contexte → dessin → layout → widgets → fenêtres → docking.
Chaque couche ne dépend que de la couche inférieure.

### Système d'animation intégré
`NkUIAnimator` avec 24 fonctions d'easing (Elastic, Bounce, Back…). Effets prêts à l'emploi :
`Pulse()`, `Shake()`, `FadeIn()`, `FadeOut()`, `SlideInFromTop()`, `Bounce()`.
Absent de la plupart des libs UI immediate-mode (ImGui le laisse à l'utilisateur).

### Système de thèmes flexible
4 thèmes prêts + palette sémantique 40+ couleurs + `NkUIMetrics` pour tous les espacements.
Chargement/sauvegarde JSON.

### DrawList à 4 layers
`LAYER_BG / LAYER_WINDOWS / LAYER_POPUPS / LAYER_OVERLAY` — ordre de rendu correct sans tri manuel.
Même pattern qu'ImGui, éprouvé.

### Police bitmap intégrée
`kBitmapFont[96*10]` (ASCII 32–127) encodé en 6 bits/ligne — fallback garanti sans fichier TTF.

### Gizmos 2D/3D natifs
Translate/Rotate/Scale, snapping, espaces Local/World, grille 2D/3D. Rare dans une lib UI custom.

### Docking complet
Arbre binaire de split, onglets, drag-and-drop, sérialisation JSON.
Niveau de complexité comparable à ImGui Docking Branch.

---

## Points faibles et bugs

### 1. Bug syntaxique (corrigé v2)
`NkUIDock.cpp:116` — `nodes[nodeIdx)` (bracket/parenthèse mélangés).
Corrigé dans la version v2.

### 2. Division par zéro potentielle dans RenderNode (corrigé v2)
```cpp
// AVANT
const float32 tabW = r.w / node.numWindows;  // crash si numWindows == 0
// APRÈS
if (node.numWindows <= 0) return;
const float32 tabW = r.w / static_cast<float32>(node.numWindows);
```

### 3. Seuil undock trop restrictif (corrigé v2)
```cpp
// AVANT : ne détache que si drag vertical > 5px
ctx.input.mouseDelta.y > 5.f
// APRÈS : norme du vecteur delta (drag dans n'importe quelle direction)
dx*dx + dy*dy > 25.f
```

### 4. Tabs non conditionnels (corrigé v2)
Pas de flag `NK_NO_TABS` — toute fenêtre pouvait être ajoutée en tab même si indésirable.
Ajout de `NK_NO_TABS` (bit 13) et `NK_ALLOW_DOCK_CHILD` (bit 14).

### 5. État global mutable non thread-safe
`NkUIWindow`, `NkUIMenu` utilisent des `static` locales au .cpp.
Non critique si single-threaded, mais fragile pour un futur rendu multi-thread.

### 6. `ctx.wm` couplage implicite
Le code de Window/Layout/Dock accède à `ctx.wm` mais `NkUIContext.h` ne le déclare pas explicitement.
Le contrat est implicite — risque de confusion.

### 7. Pools fixes sans garde-fous
| Limite | Valeur |
|--------|--------|
| Tweens max | 512 |
| Animations (NkUIAnimator) | 256 |
| Fenêtres max | 64 |
| Nodes de dock max | 128 |
| Layout stack depth | 32 |

Pas d'`assert` visible quand ces limites sont atteintes.

### 8. JSON parsing incomplet
`NkUIContext::ParseThemeJSON()` déclaré mais non implémenté.
`NkUIDockManager::LoadLayout()` stub (lit le fichier, ne parse pas).

### 9. Encodage UTF-8 cassé dans les .cpp
Commentaires français corrompus (`ImplÃƒÂ©mentation`, `FenÃªtres`).
Fichiers réencodés sans respecter UTF-8 sur Windows.

### 10. Occlusion culling désactivé (intentionnel)
`NkUIDrawList::IsOccluded()` retourne `false` — tous les rects sont envoyés au GPU
même s'ils sont derrière d'autres fenêtres. Correct pour immediate-mode mais inefficace.

---

## Comparaison Dear ImGui

| Aspect | NKUI | Dear ImGui |
|--------|------|------------|
| Mode | Immediate | Immediate |
| Animations intégrées | Oui (24 easing) | Non |
| Docking | Oui | Oui (branch) |
| Thèmes JSON | Oui | Non |
| Gizmos | Oui (2D+3D) | Non |
| Police bitmap intégrée | Oui | Oui |
| Thread-safety | Non | Non |
| Maturité | En cours | Très mature |

---

## Roadmap corrections (v2)

- [x] Fix `NkUIDock.cpp:116` — bracket/parenthèse
- [x] Fix division par zéro `RenderNode` (garde `numWindows <= 0`)
- [x] Améliorer seuil undock (norme delta)
- [x] Ajouter `NK_NO_TABS` / `NK_ALLOW_DOCK_CHILD` flags
- [x] `NodeAllowsTabs()` — tabs conditionnels par node
- [x] `DrawDirectionalHighlight()` — surbrillance bord cible
- [x] `DrawDropZones` — filtrage CENTER si tabs interdits
- [x] `BeginChildDock` / `EndChildDock` — dock dans une fenêtre
- [x] `NkUIFileBrowser` — browser fichiers callback-first + provider NKFileSystem
- [x] `NkUITree` — arbre générique callbacks, DnD, rename, multi-select
- [ ] `ParseThemeJSON()` — parsing JSON thème complet
- [ ] `LoadLayout()` — parsing JSON layout dock
- [ ] Asserts sur limites de pools
- [ ] Re-encoder les .cpp en UTF-8 propre

---

## Structure des nouveaux Tools (v1)

### NkUIFileBrowser
- Provider par callbacks (`list/read/write/mkdir/move/delete/stat`)
- Provider par défaut `NKFileSystemProvider()` (Win32 + POSIX)
- DnD natif entre entrées
- Modes : OpenFile, OpenFolder, SaveFile
- Opérations : créer, renommer, supprimer, naviguer
- Breadcrumb cliquable, toolbar, filtres par extension

### NkUITree
- Callbacks : `getChildCount`, `getChild`, `getLabel`, `getIcon`, `canDrag`, `canDrop`, `onRename`, `onMove`
- Sélection simple/multiple
- Rename inline (F2 ou double-clic)
- Drag-and-drop reparent/reorder
- Lignes de connexion parent→enfant
- Virtual scroll (skip nodes hors clip)
