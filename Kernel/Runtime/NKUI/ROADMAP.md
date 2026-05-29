# NKUI — Roadmap

État actuel (mai 2026) : bibliothèque UI immédiat-mode mature et fonctionnelle, ~40 fichiers couvrant Core / Layout / Widgets / Window / Dock / Menu / Animation / ColorPicker / Tools (Tree, FileBrowser, Gizmo, Viewport3D). Quatre thèmes prédéfinis, 4 layers de DrawList, police bitmap embarquée, renderer CPU + abstraction GPU. Plusieurs widgets éditeur classiques manquent encore (Table avancée, vrai Dropdown, Tabs natifs hors dock, FilePicker modale), la gestion clavier/IME est très basique, et la sérialisation JSON du thème et du layout reste partielle.

---

## Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| Session 1 — Core (Export, Input, Theme, DrawList, Context, Renderer) | Livré | — | — |
| Session 2 — Layout + Font + Widgets | Livré | — | — |
| Session 3 — Window + Dock | Livré | — | — |
| Session 4 — Menu + Animation + ColorPicker + Layout2 | Livré | — | — |
| Renderer CPU (`NkUICPURenderer`) | Livré | — | — |
| Renderer OpenGL (`NkUIOpenGLRenderer`, opt) | Livré | — | — |
| Multi-Viewport (`NkUIMultiViewport`) | Livré | — | — |
| Tools/Tree (callback-first, DnD, rename) | Livré | — | — |
| Tools/FileBrowser (List/Icons/Tiles, breadcrumb) | Livré | — | — |
| Tools/Gizmo 2D/3D (TRS, snap, grille infinie) | Livré | — | — |
| Tools/Viewport3D (orbit, gizmo, outliner) | Partiel | M | P1 |
| `NkUIFontBridge` (intégration NKFont TTF) | Partiel | S | P1 |
| `NkUIThemeJSON` parse complet | TODO | S | P1 |
| `NkUIDockManager::LoadLayout` parse complet | TODO | S | P1 |
| Widget Table avancée (tri, resize, gel colonne) | Partiel | M | P1 |
| Widget Dropdown / ComboBox custom render | Partiel | S | P2 |
| Widget Tabs (hors docking) | TODO | S | P2 |
| Widget DatePicker / TimePicker | TODO | M | P3 |
| Widget Notifications / Toasts | TODO | S | P2 |
| Widget Modal natif (vs popup actuel) | Partiel | S | P2 |
| Text Input avancé (IME, sélection multi-ligne) | TODO | L | P1 |
| Accessibilité (focus traversal, screen reader) | TODO | L | P2 |
| Renderer Vulkan / DX11 / DX12 | TODO | L | P2 |
| Pixel-perfect line / AA primitives | Partiel | M | P2 |
| Tests unitaires / snapshots visuels | TODO | M | P2 |
| Encodage UTF-8 propre dans tous les .cpp | TODO | S | P1 |
| Asserts sur capacités fixes (tweens, windows, dock nodes) | TODO | S | P2 |

Légende : Livré · Partiel · En cours · TODO · Abandonné

---

## Livré

### Session 1 — Core
- `NkUIExport.h` : macros DLL (`NKUI_API`), alias types NKMath, helpers géométriques `NkColor`, `NkRect`, `NkVec2`.
- `NkUIInput.h` : abstraction input plateforme-agnostique (souris 5 boutons, clavier 128 touches, touch 10 points, dt, modifiers, charInput).
- `NkUITheme.h` : palette sémantique 40+ couleurs (`bgPrimary`, `accent`, `border`, `textPrimary`...), métriques (paddings, radii, font sizes), 4 presets `Default / Dark / Minimal / HighContrast`.
- `NkUIDrawList.h/.cpp` : géométrie vertex/index/cmd, 4 layers (`LAYER_BG`, `LAYER_WINDOWS`, `LAYER_POPUPS`, `LAYER_OVERLAY`), primitives rect, cercle, ellipse, arc, Bézier, path SVG-like, image, texte, clip rect.
- `NkUIContext.h/.cpp` : état global, IDs FNV-1a, animations smooth, style stack, shape overrides, layout cursor, références aux 4 DrawLists.
- `NkUIRenderer.h/.cpp` : interface abstraite + `NkUICPURenderer` software offline (pixels RGBA32).
- `NkUIMath.h/.cpp` : helpers UI (lerp, clamp, distance, intersection rect).

### Session 2 — Layout + Font + Widgets
- `NkUILayout.h/.cpp` : `NkUILayoutStack` (32 niveaux), `NkUILayoutNode` types `Row / Column / Grid / Split / Tab / Scroll / Group / Free`, contraintes flex `AUTO / FIXED / PERCENT / GROW`, scroll régions avec scrollbar auto.
- `NkUIFont.h/.cpp` : atlas glyphes, police bitmap embarquée `kBitmapFont[96*10]` (ASCII 32-127, 6 bits/ligne) — fallback 0-dépendance.
- `NkUIWidgets.h/.cpp` : 40+ widgets (Button, Checkbox, Toggle, Radio, SliderFloat/Int, DragFloat/Int, InputText/Int/Float/Multiline, Combo, ListBox, ProgressBar, ColorButton, TreeNode, Table, Text, Tooltip, Separator, Image, Spacer). Conventions `Label##id`, ShapeOverride par type de widget, `SliderValueOptions` (placement, gap).

### Session 3 — Window + Dock
- `NkUIWindow.h/.cpp` : fenêtres flottantes (titre, fermeture, minimisation, maximisation, drag, resize 6 axes / 8-way, scroll, collapse, z-order, animation, BeginChild, SetNextPos/Size, flags `NK_NO_TABS`, `NK_ALLOW_DOCK_CHILD`).
- `NkUIDock.h/.cpp` : docking complet — arbre binaire split/leaf, drag & drop, dropzones visuelles (Left/Right/Top/Bottom/Center/Tab), tabs multiples conditionnels, detach, splitters draggable, surbrillance directionnelle, child docking, décorations onglet via callbacks (`NkTabDecoration`).

### Session 4 — Menu + Animation + ColorPicker + Layout2
- `NkUIMenu.h/.cpp` : MenuBar, Menu déroulant, MenuItem (raccourcis, checkmarks), ContextMenu (clic droit), `BeginPopup`, `BeginModal`.
- `NkUIAnimation.h/.cpp` : 22-24 fonctions d'easing (Linear, Quad, Cubic, Quart, Quint, Sine, Expo, Circ, Elastic, Bounce, Back, In/Out/InOut), `NkUITween` pool (512 max), `NkUIAnimator` (256 anims), effets prêts `Shake / Pulse / FadeIn / FadeOut / SlideIn / Bounce`.
- `NkUILayout2.h/.cpp` : `NkUIColorPickerFull` (carré SV, barre hue, barre alpha, sliders RGBA, input hex), serializer JSON layout (sauvegarde uniquement), `NkUIFontNKFont` (intégration NKFont TTF/OTF/WOFF, opt), `NkUIOpenGLRenderer` GPU OpenGL 3.3+ Core Profile (opt).

### Tools/
- `Tools/Tree/NkUITree.h/.cpp` : arbre générique callback-first (`getChildCount`, `getChild`, `getLabel`, `getIcon`, `canDrag`, `canDrop`, `onRename`, `onMove`), multi-select, rename inline F2, DnD reparent/reorder, lignes de connexion, virtual scroll.
- `Tools/FileSystem/NkUIFileBrowser.h/.cpp` : Content Browser style Unreal — provider callback (list/read/write/mkdir/move/delete/stat), provider par défaut `NKFileSystemProvider` (Win32+POSIX), 4 view modes (List / Icons Small / Icons Large / Tiles), tri par Name/Size/Date/Type, breadcrumb cliquable, toolbar (créer, renommer, supprimer), DnD natif, filtres extension, modes Open/Save/SelectDir/Multi.
- `Tools/Gizmo/NkUIGizmo.h/.cpp` : gizmo TRS 2D/3D, modes Translate/Rotate/Scale, espaces Local/World/Normal, masques d'axes XYZ, snapping, grille 2D et grille 3D infinie (fade horizon façon Blender), plan de sol plein ou wireframe.
- `Tools/Viewport/NkUIViewport3D.h/.cpp` : viewport production-ready style Unreal — barre d'outils (mode transform, vue, snap, stats), grille infinie, axes XYZ, gizmo multi-mode, outliner liste objets, panel détails (transform, couleur), barre de statut, navigation (LMB orbite, RMB pan, molette zoom, F focus, Numpad vues, G/R/S modes, Ctrl+Z, Del), modes caméra perspective/orthos.

### Intégrations
- `NkUIMultiViewport.h/.cpp` (à la racine du module) : support de multiples fenêtres OS par viewport.
- `NkUIFontBridge.h/.cpp` : pont vers NKFont pour fonts TTF/OTF.

---

## En cours / TODO immédiat

### Bugs et hygiène (P0-P1)
- Encodage UTF-8 cassé dans plusieurs .cpp (`ImplÃƒÂ©mentation`, `FenÃªtres`) — re-encoder proprement.
- Aucun `assert` sur les limites de pool atteintes (`Tweens=512`, `Animations=256`, `Windows=64`, `DockNodes=128`, `LayoutStack=32`) : ajouter `NK_ASSERT` + log warning.
- Couplage implicite `ctx.wm` (utilisé par Window/Layout/Dock mais non déclaré dans `NkUIContext.h`) — rendre explicite ou injecter par paramètre.
- `NkUIDrawList::IsOccluded()` retourne toujours false : pas un bug mais inefficace si beaucoup de fenêtres se chevauchent.
- État global mutable non thread-safe dans `NkUIWindow` / `NkUIMenu` (statics locales aux .cpp) — fragile pour un rendu multi-thread futur.

### Sérialisation JSON incomplète (P1)
- `NkUIContext::ParseThemeJSON()` déclaré mais non implémenté (commentaire dans NKUI_ANALYSIS.md).
- `NkUIDockManager::LoadLayout()` stub (lit le fichier, ne parse pas).
- `NkUILayoutSerializer::Save()` fonctionne, `Load()` manquant.

### Widgets manquants côté éditeur (P1-P2)
- **Table avancée** : tri par colonne via clic header, resize de colonne par drag, gel des colonnes / freezePane, virtual scrolling sur grandes datasets, sélection cellule/ligne.
- **Dropdown / ComboBox custom render** : permettre un render arbitraire par item (icône + texte + sous-titre), search-as-you-type.
- **Tabs** widget natif (hors du dock) — utile pour Inspector multi-pages, Settings.
- **Modal** natif (overlay + focus capture + ESC = cancel) plus robuste que les popups actuels.
- **Toasts / Notifications** (corner overlay, auto-dismiss, stack).
- **DatePicker / TimePicker** (calendrier mensuel, sélection range).
- **NumericUpDown** distinct du DragFloat (flèches haut/bas, contrôle clavier).
- **TagInput** (multi-tags séparés par enter/virgule).
- **CodeEditor** simple (line numbers, syntax highlight basique via callback).

### Text input et clavier (P1)
- Pas de support **IME** (Input Method Editor) — bloquant pour le japonais / chinois / coréen.
- Sélection texte rudimentaire — pas de double-clic word, triple-clic ligne, shift+arrow extend.
- Pas d'undo/redo dans `InputText`.
- Pas de raccourcis Ctrl+A, Ctrl+Backspace, Ctrl+Left/Right (word jump) systématiques.
- Pas de tabulation focus traversal (Tab / Shift+Tab pour passer entre widgets).

### Renderer (P1-P2)
- Vérifier la qualité AA dans `NkUIDrawList` : aliasing visible sur cercles / Bézier en CPU renderer.
- Renderer Vulkan / DX11 / DX12 absents (seuls CPU et OpenGL sont fournis) — alors que NKRenderer cible 4 backends. Décision : déléguer entièrement à NKRenderer + supprimer le renderer GL embarqué, ou bien maintenir des backends NKUI dédiés ?
- Pixel-perfect lines (snap aux demi-pixels selon l'orientation) pour rendu net dans l'éditeur.

### Viewport3D (P1)
- Camera fly mode (FPS-style WASD) en plus de l'orbite.
- Sélection objet par picking (raycast contre les bounding volumes).
- Outliner doit utiliser `NkUITree` au lieu de sa liste interne.
- Persistence des préférences viewport (mode, snap, grille) dans le layout JSON.

---

## À venir / À ajouter (futur proche)

### Accessibilité
- Navigation clavier complète (Tab focus, Enter pour valider, ESC pour annuler).
- Lecteur d'écran : exposer un arbre AccessibleNode + rôle/label/value (UI Automation Win32, AT-SPI Linux).
- Mode high-contrast déjà présent en thème, mais sans support contraste runtime (fonts épaissies, focus ring marqué).
- Tooltip avec timing configurable, lecture vocale.
- Tailles de hit-box minimales 44x44 px (touch).

### Système de thème étendu
- Hot-reload JSON : déjà annoncé, à câbler avec watcher fichier (`NKFileSystem`).
- Thèmes Light "Material You" et "Fluent" en plus des 4 existants.
- Variables CSS-like (`$primary-500`, `$radius-md`) référencables dans le JSON.
- Animation des transitions de thème (cross-fade colors over 250ms).

### Composition / Architecture
- Decoupler `NkUIContext` (qui est gros) en `NkUIInputState`, `NkUIStyleStack`, `NkUIDrawState`, `NkUILayoutState`.
- Préparer une couche "Retained UI" optionnelle au-dessus (NkUIView, NkUIBinding) pour des écrans data-bound performants (Settings, HUD complexe).
- API "Command" pour enregistrer des actions et les rejouer (undo Inspector).

### Performance
- DrawList batching : merger les commands consécutifs de même material/clipRect.
- Texte : caching par chaîne+font+size (mesh-level), pas seulement par glyph.
- Occlusion culling réel pour les fenêtres entièrement masquées.
- Profiler intégré (CPU time par section : layout / draw / submit) avec affichage in-game.

### Plateforme
- Touch gestures dignes (pan, pinch zoom dans Scroll/Viewport, long press = right-click).
- Drag-and-drop natif OS (drop de fichiers depuis l'explorateur dans NkUIFileBrowser).
- Clipboard intégré (`NkUIClipboard` : text + image).
- Curseurs custom (resize NE-SW, IBeam, hand, crosshair).

### Tests
- Tests unitaires layout (vérifier que `Row + 3 widgets PERCENT` tombe juste au pixel).
- Snapshots visuels (rendre une scène déterministe, comparer la framebuffer pixel-à-pixel).
- Test de stress 1000 fenêtres, 10000 widgets/frame.
- Replay enregistré : sérialiser une séquence d'input + comparer le DrawList résultant.

### Outils éditeur supplémentaires
- **NkUIPropertyGrid** générique branché sur `NkReflect` (NKECS) pour Inspector auto.
- **NkUITimeline** (clips, keyframes, drag, scrub) — pour éditeur d'animation.
- **NkUINodeGraph** (Blueprint visuel) — déjà esquissé côté Noge (`NkBlueprint.h`), backend NKUI à créer.
- **NkUICurveEditor** (édition de courbes Bézier, easing).
- **NkUISpreadsheet** (édition CSV/data assets).
- **NkUIConsole** (logger + commandes — déjà partiellement présent côté Unkeny).

---

## Bugs / quirks connus
- v2 a corrigé : `NkUIDock.cpp:116` (bracket/parenthèse), division par zéro dans `RenderNode` quand `numWindows==0`, seuil d'undock trop restrictif (5px vertical -> norme delta 25), absence de flag `NK_NO_TABS`.
- v2 a ajouté : `NodeAllowsTabs()`, `DrawDirectionalHighlight()`, filtrage CENTER des dropzones, `BeginChildDock` / `EndChildDock`, `NkUIFileBrowser`, `NkUITree`.
- Restant : `ParseThemeJSON()`, `LoadLayout()`, asserts pools, UTF-8 cassé, occlusion culling désactivé, état statique non thread-safe, couplage implicite `ctx.wm`.
- `NkUIGizmo copy.h` traîne dans `Tools/Gizmo/` — fichier doublon à supprimer.
- `pch/` présent à la racine du module mais usage non audité — vérifier qu'il ne casse pas la compilation incrémentale.

---

## Dépendances
- **Couches en dessous (utilisées)** :
  - `NKCore` (NkTypes, NkColor, NkRect, NkVec2)
  - `NKMath` (Vec2/3/4, transformations 2D/3D — via `NkUIGizmo` et `NkUIViewport3D`)
  - `NKFont` (optionnel, `NkUIFontBridge` + `NkUIFontNKFont` derrière `-DNKUI_WITH_NKFONT`)
  - `NKFileSystem` (provider par défaut de `NkUIFileBrowser`)
  - `NKRenderer` (cible recommandée pour soumission des DrawLists côté GPU final)
  - `OpenGL 3.3+` (optionnel, `NkUIOpenGLRenderer` derrière `-DNKUI_WITH_OPENGL`)
  - `nlohmann/json` (sauvegarde/chargement layout et thème)
- **Modules au-dessus qui en dépendent** :
  - `Engine/Noge/src/Noge/ECS/Components/UI/NkUIComponent.h` (composant ECS qui pilote un panel NkUI)
  - `Applications/Unkeny/` (éditeur) : `EditorLayer`, `ViewportPanel`, `SceneTreePanel`, `InspectorPanel`, `AssetBrowser`, `ConsolePanel`, `DiagnosticPanel`
  - `Sandbox/` (démos et bancs d'essai)
  - PV3DE (`MedicalUILayer` selon ARCHITECTURE.md §4.5)
- **Relation à NKRenderer** : NKUI produit des DrawLists indépendantes du backend. Le `NkUIRenderer` interne fournit deux implémentations (CPU + GL) en bout de chaîne. La cible long terme est de déléguer la soumission à `NKRenderer` (BeginRenderPass / BindPipeline / Draw) pour bénéficier de Vulkan/DX12 et de l'unification des viewports.
