# Spécification technique d'implémentation — Aetherion Engine UI
### Document 2/3 — Destiné à Claude (agent d'implémentation)

> Ce document traduit `01-specification-humaine.md` en contraintes techniques exploitables directement pour générer le code (React/HTML/CSS ou équivalent). Il définit : design tokens, architecture de composants, arborescence de fichiers suggérée, contrats de props, et règles d'implémentation du docking. Toujours lire ce fichier avant de coder un écran ; se référer au document 1 pour le contexte fonctionnel et au document 3 pour la déclinaison "prompt par écran".

---

## 1. Design tokens (CSS custom properties)

Définir les deux thèmes comme des attributs `data-theme="light"` / `data-theme="dark"` sur l'élément racine. Ne jamais coder une couleur en dur dans un composant : toujours passer par une variable.

```css
:root[data-theme="light"] {
  --bg-canvas: #ffffff;
  --bg-subtle: #f6f8fa;
  --bg-inset: #eaeef2;
  --bg-overlay: #ffffff;
  --border-default: #d0d7de;
  --border-muted: #d8dee4;
  --fg-default: #1f2328;
  --fg-muted: #656d76;
  --fg-subtle: #6e7781;
  --accent-fg: #0969da;
  --accent-emphasis: #0969da;
  --accent-subtle: #ddf4ff;
  --success-fg: #1a7f37;
  --attention-fg: #9a6700;
  --danger-fg: #d1242f;
  --danger-emphasis: #cf222e;
  --done-fg: #8250df;
  --selection-outline: #bc4c00;
  --shadow-elevation: 0 8px 24px rgba(140,149,159,0.2);
  --viewport-bg: #1e1e1e; /* fixe, ne suit pas le thème */
}

:root[data-theme="dark"] {
  --bg-canvas: #0d1117;
  --bg-subtle: #161b22;
  --bg-inset: #010409;
  --bg-overlay: #1c2128;
  --border-default: #30363d;
  --border-muted: #21262d;
  --fg-default: #e6edf3;
  --fg-muted: #8b949e;
  --fg-subtle: #7d8590;
  --accent-fg: #2f81f7;
  --accent-emphasis: #1f6feb;
  --accent-subtle: #0c2d6b;
  --success-fg: #3fb950;
  --attention-fg: #d29922;
  --danger-fg: #f85149;
  --danger-emphasis: #da3633;
  --done-fg: #a371f7;
  --selection-outline: #f0883e;
  --shadow-elevation: 0 8px 24px rgba(1,4,9,0.6);
  --viewport-bg: #1e1e1e;
}
```

### 1.1 Tokens communs (indépendants du thème)

```css
:root {
  --font-ui: "Segoe UI", "Inter", -apple-system, sans-serif;
  --font-mono: "Cascadia Code", "JetBrains Mono", monospace;
  --font-size-xs: 11px;
  --font-size-sm: 12px;
  --font-size-md: 13px;
  --font-size-lg: 15px;
  --radius-sm: 4px;
  --radius-md: 6px;
  --space-1: 4px;
  --space-2: 8px;
  --space-3: 12px;
  --space-4: 16px;
  --row-height: 24px;
  --header-height: 28px;
  --toolbar-height: 36px;
  --menubar-height: 30px;
  --titlebar-height: 32px;
  --statusbar-height: 24px;
  --transition-fast: 120ms ease-out;
  --transition-medium: 200ms ease-out;
  --z-panel: 1;
  --z-dock-overlay: 500;
  --z-dropdown: 800;
  --z-modal-backdrop: 900;
  --z-modal: 901;
  --z-toast: 1000;
  --z-tooltip: 1100;

  /* couleurs d'axes transform, fixes quel que soit le thème */
  --axis-x: #f14c4c;
  --axis-y: #3fb950;
  --axis-z: #2f81f7;

  /* couleurs par catégorie d'asset, fixes */
  --asset-mesh: #e3833b;
  --asset-material: #8250df;
  --asset-blueprint: #2f81f7;
  --asset-sound: #3fb950;
  --asset-texture: #d29922;
  --asset-animation: #f778ba;
}
```

Contraste minimal AA à respecter partout : `fg-default` sur `bg-canvas`/`bg-subtle` ≥ 4.5:1 (déjà validé pour les valeurs ci-dessus, ne pas les modifier sans revalider).

---

## 2. Arborescence de fichiers suggérée

```
/src
  /app
    Launcher/               → cible du doc §3 (fichier humain)
    EditorShell/            → §4
  /panels
    Viewport3D/
    WorldOutliner/
    DetailsPanel/
    ContentBrowser/
    ReferenceViewer/         → §9bis doc humain (graphe de dépendances)
    SizeMap/                 → §9bis doc humain (treemap de poids)
    ProjectNavigator/        → §9ter doc humain (arborescence disque réelle)
    ClassViewer/             → §9quater doc humain (hiérarchie de classes)
    PlaceActors/
    OutputLog/
  /editors
    MaterialEditor/
    BlueprintEditor/
    Sequencer/
  /settings
    ProjectSettings/
    WorldSettings/
    EditorPreferences/
  /build
    PackagingDialog/
    BuildLightingDialog/
  /ui-kit                    → composants génériques réutilisables (doc humain §19)
    Toolbar/
    TabStrip/
    TreeView/
    PropertyRow/
    ThumbnailCard/
    Modal/
    Toast/
    ContextMenu/
    NodeGraphCanvas/
    ColorSwatch/
  /docking                   → moteur de docking, indépendant des panneaux
    DockManager.ts
    DockZone.tsx
    DockableWindow.tsx
    FloatingWindow.tsx
  /theme
    tokens.css
    ThemeProvider.tsx
```

Règle : un panneau dockable = un dossier sous `/panels` ou `/editors`, exportant un composant qui ne connaît **rien** du docking (il reçoit juste sa taille via CSS flex/grid). Le docking est un système générique dans `/docking`, jamais dupliqué par panneau.

---

## 3. Modèle du système de docking

### 3.1 Concepts
- `DockNode` : nœud d'un arbre binaire représentant soit un `split` (horizontal/vertical, avec ratio 0–1), soit un `tabset` (liste de panelId + index actif).
- L'état du layout est sérialisable en JSON → permet `Window > Layouts > Save/Load` (doc humain §5).
- Chaque panneau enregistré a un `panelId` unique, un `title`, une `icon`, et un composant React/DOM associé via un registre `panelRegistry[panelId] = Component`.

### 3.2 Interactions à implémenter
| Action | Comportement |
|---|---|
| Drag d'un onglet | Affiche un `DockDropOverlay` avec 5 zones (N/S/E/W/Center) sur le `DockNode` survolé |
| Drop sur Center | Ajoute le panelId au `tabset` existant, devient actif |
| Drop sur un bord | Transforme le `tabset` cible en `split`, insère un nouveau `tabset` contenant seulement ce panelId, ratio par défaut 50/50 |
| Drag hors de toute zone dockable | Crée une `FloatingWindow` (position = curseur, taille = taille précédente du panneau) |
| Fermeture d'un panneau | Retire son panelId du tabset ; si le tabset devient vide, il est supprimé et le split parent absorbe l'espace du frère restant |
| Redimensionnement | Poignée de 4px sur chaque frontière de `split`, drag met à jour le `ratio`, min 15% par côté |

### 3.3 État par défaut (`defaultLayout.json`)
```json
{
  "type": "split", "direction": "row", "ratio": 0.8,
  "a": {
    "type": "split", "direction": "column", "ratio": 0.75,
    "a": { "type": "tabset", "panels": ["viewport3d"], "active": 0 },
    "b": { "type": "tabset", "panels": ["contentBrowser", "outputLog"], "active": 0 }
  },
  "b": {
    "type": "split", "direction": "column", "ratio": 0.55,
    "a": { "type": "tabset", "panels": ["worldOutliner"], "active": 0 },
    "b": { "type": "tabset", "panels": ["detailsPanel"], "active": 0 }
  }
}
```

---

## 4. Contrats de composants (props) — extraits prioritaires

### 4.1 `<PropertyRow>`
```ts
type PropertyType = "number" | "vector3" | "text" | "boolean" | "color" | "enum" | "assetRef" | "curve";

interface PropertyRowProps {
  label: string;
  type: PropertyType;
  value: unknown;
  onChange: (v: unknown) => void;
  isOverridden?: boolean;   // affiche pastille jaune + label en gras (voir doc humain §8)
  min?: number; max?: number; step?: number;
  enumOptions?: { label: string; value: string }[];
  axisColors?: [string, string, string]; // pour vector3 → --axis-x/y/z
  onResetToDefault?: () => void;         // icône reset au survol
}
```

### 4.2 `<ThumbnailCard>`
```ts
interface ThumbnailCardProps {
  assetName: string;
  assetType: "mesh" | "material" | "blueprint" | "sound" | "texture" | "animation" | "folder";
  thumbnailUrl?: string;       // fallback = icône générique par type + couleur --asset-*
  isSelected: boolean;
  size: "sm" | "md" | "lg";    // piloté par le slider de taille du Content Browser
  onDoubleClick: () => void;
  onDragStart: (e: DragEvent) => void; // pour drop dans Viewport3D / WorldOutliner
  onContextMenu: (e: MouseEvent) => void;
}
```

### 4.3 `<TreeView>` (Outliner + arbre de dossiers)
```ts
interface TreeNode {
  id: string;
  label: string;
  icon?: string;
  visible?: boolean;          // toggle œil, seulement pour WorldOutliner
  children?: TreeNode[];
  isFolder?: boolean;
}
interface TreeViewProps {
  nodes: TreeNode[];
  selectedIds: string[];
  onSelect: (ids: string[], additive: boolean) => void;
  onToggleVisibility?: (id: string) => void;
  onReorder?: (draggedId: string, targetId: string, position: "before"|"after"|"inside") => void;
  onRename?: (id: string, newLabel: string) => void;
  extraColumns?: { key: string; header: string; render: (n: TreeNode) => string }[];
}
```

### 4.3bis `<ProjectNavigatorTree>` (arborescence disque réelle)

Distinct de `<TreeView>` utilisé par le WorldOutliner/Content Browser : source de données = système de fichiers réel, pas une hiérarchie virtuelle d'assets.

```ts
interface FileSystemNode {
  path: string;                 // chemin relatif projet, ex: "Source/Game/Hero.cpp"
  name: string;
  kind: "folder" | "cpp" | "header" | "ini" | "uasset" | "plugin" | "other";
  isGenerated?: boolean;        // true pour Binaries/Intermediate/Saved → grisé, masqué par défaut
  pluginEnabled?: boolean;      // uniquement pertinent si kind === "plugin"
  children?: FileSystemNode[];
}
interface ProjectNavigatorProps {
  root: FileSystemNode;
  showGeneratedFolders: boolean;
  onToggleShowGenerated: (v: boolean) => void;
  onOpenInIDE: (path: string) => void;
  onCreateCppClass: () => void;   // ouvre le wizard "Nouvelle classe C++"
  searchQuery: string;
  onSearchChange: (q: string) => void;
}
```
Icônes par `kind` : `.cpp` → bleu `--accent-fg`, `.h` → violet `--done-fg`, `.ini` → gris `--fg-muted`, `.uasset` → couleur `--asset-*` selon sous-type détecté. Les dossiers `Binaries/`, `Intermediate/`, `Saved/` ont `isGenerated: true` par défaut et sont filtrés hors de l'arbre tant que `showGeneratedFolders === false`.

### 4.3ter `<ClassHierarchyTree>` (Class Viewer)

```ts
interface ClassNode {
  id: string; name: string;
  origin: "engine-cpp" | "blueprint";
  isAbstract?: boolean;
  children?: ClassNode[];
}
interface ClassViewerProps {
  root: ClassNode;               // racine = "Object"
  filter: "all" | "blueprintOnly";
  onFilterChange: (f: ClassViewerProps["filter"]) => void;
  onCreateChildBlueprint: (classId: string) => void;
  onOpenClass: (classId: string) => void;
  onFindReferences: (classId: string) => void; // ouvre ReferenceViewerProps filtré
}
```

### 4.3quater `<ReferenceViewer>` / `<SizeMap>`

Réutilisent `<NodeGraphCanvas>` (ReferenceViewer) et un composant `<Treemap>` dédié (SizeMap) :

```ts
interface ReferenceViewerProps {
  centerAssetId: string;
  depth: number;                 // 1-10, slider toolbar
  showEditorOnlyRefs: boolean;
  getReferencers: (assetId: string) => AssetRefNode[];   // flèches entrantes
  getDependencies: (assetId: string) => AssetRefNode[];  // flèches sortantes
  onLocateInContentBrowser: (assetId: string) => void;
}
interface AssetRefNode { id: string; name: string; type: string; isMissing?: boolean; }

interface TreemapNode { id: string; name: string; sizeBytes: number; assetType: string; }
interface SizeMapProps {
  rootId: string;              // asset ou dossier de départ
  nodes: TreemapNode[];        // déjà aplatis, tri par sizeBytes desc pour le pavage
  colorByType: Record<string, string>; // → --asset-* tokens
  onHoverNode?: (id: string) => void;  // tooltip poids exact + chemin
}
```

### 4.4 `<NodeGraphCanvas>` (Material Editor + Blueprint Editor)
```ts
interface GraphNode {
  id: string; type: string; title: string;
  position: { x: number; y: number };
  inputs: { id: string; label: string; dataType: string }[];
  outputs: { id: string; label: string; dataType: string }[];
}
interface GraphEdge { id: string; fromNode: string; fromPort: string; toNode: string; toPort: string; }
interface NodeGraphCanvasProps {
  nodes: GraphNode[];
  edges: GraphEdge[];
  onNodeMove: (id: string, pos: {x:number;y:number}) => void;
  onConnect: (edge: Omit<GraphEdge,"id">) => void;
  onDeleteEdge: (id: string) => void;
  dataTypeColors: Record<string, string>; // ex: {vector:"#fff", scalar:"#3fb950", texture:"#d29922", exec:"#ffffff"}
  showMinimap?: boolean;
}
```
Note d'implémentation : les câbles sont des courbes de Bézier cubiques ; le point de contrôle horizontal = `abs(dx) * 0.5` minimum `40px`, pour garder une sortie/entrée toujours horizontale au nœud (comportement Unreal/Blueprint standard).

### 4.5 `<Toolbar>` / `<PlayButtonGroup>`
```ts
interface PlayButtonGroupProps {
  mode: "editor" | "simulate" | "standalone" | "device";
  isPlaying: boolean;
  onPlay: (mode: PlayButtonGroupProps["mode"]) => void;
  onStop: () => void;
  onPause: () => void;
}
```
Couleur du bouton Play = `--success-fg`. Pendant Play, la bordure du Viewport3D entier passe en `2px solid var(--success-fg)` pour signaler visuellement le mode (comme PIE dans Unreal).

---

## 5. Spécification écran par écran — check-list d'implémentation

Pour chaque écran listé ci-dessous, implémenter dans cet ordre : (1) layout statique avec tokens, (2) états interactifs (hover/selected/dragover), (3) branchement docking si applicable, (4) responsive minimum (résolution mini supportée : 1366x768 pour l'éditeur, 1024x768 pour le launcher).

1. **Launcher – Splash** : composant standalone, pas de docking, auto-transition après chargement simulé (mock 1.5s).
2. **Launcher – Bibliothèque** : grid responsive `repeat(auto-fill, minmax(240px, 1fr))`, cartes avec `aspect-ratio: 16/9` pour la miniature.
3. **Launcher – New Project Wizard** : `<Modal size="full">` + composant `<Stepper>` générique (interface `steps: {label:string; content:ReactNode}[]`).
4. **EditorShell** : conteneur racine flex-column (titlebar / menubar / toolbar / body / statusbar), body = `<DockManager>`.
5. **Viewport3D** : `<canvas>` (ou placeholder texturé si pas de rendu 3D réel) + overlays en `position:absolute` avec `pointer-events` désactivés sauf sur les contrôles.
6. **WorldOutliner** : `<TreeView>` + barre de recherche + filtre dropdown.
7. **DetailsPanel** : liste de sections `<Accordion>` contenant des `<PropertyRow>`.
8. **ContentBrowser** : split gauche (arbre dossiers) / droite (grid `<ThumbnailCard>`), breadcrumb en haut.
8bis. **ReferenceViewer** : réutilise `<NodeGraphCanvas>` en lecture seule (pas d'édition de câbles), nœud central non déplaçable hors centre, flèches entrantes à gauche / sortantes à droite.
8ter. **SizeMap** : `<Treemap>` custom (algorithme squarified treemap suffisant), pas de lib externe requise pour un premier jet.
8quater. **ProjectNavigator** : `<ProjectNavigatorTree>`, masqué par défaut tant qu'aucun dossier `Source/` non vide n'est détecté dans les données mock.
8quinquies. **ClassViewer** : `<ClassHierarchyTree>`, accessible en panneau dockable ET en modal depuis le wizard de création de Blueprint (même composant, deux points d'entrée).
9. **PlaceActors** : `<TabStrip>` horizontal + grille d'icônes draggables.
10. **OutputLog** : liste virtualisée (si > 500 lignes, prévoir virtualisation) + filtre par sévérité.
11. **MaterialEditor** : `<NodeGraphCanvas>` + panneau preview 3D + palette recherche à gauche.
12. **BlueprintEditor** : identique structurellement à MaterialEditor + `<TreeView>` composants en plus.
13. **Sequencer** : grille timeline custom (pas de lib externe requise) — colonne pistes (`<TreeView>` simplifié) + zone keyframes en SVG.
14. **ProjectSettings / WorldSettings / EditorPreferences** : `<Modal size="full">` avec sidebar catégories (liste simple) + zone de contenu scrollable en sections repliables.
15. **PackagingDialog** : `<Modal>` en stepper simplifié (pas de retour arrière une fois lancé) + composant `<StepProgressList>` (icône par étape : pending/spinner/success/error).
16. **Toast / Modal / ContextMenu / Tooltip** : composants globaux montés une seule fois à la racine de l'app via un `UIOverlayProvider`, invoqués par hooks (`useToast()`, `useContextMenu()`, `useModal()`).

---

## 6. Règles d'accessibilité et de robustesse

- Tous les éléments interactifs (icônes toolbar, onglets, lignes d'arbre) sont des `<button>` ou ont `role` + `tabindex` appropriés, jamais des `<div onClick>` nus.
- Focus visible obligatoire : `outline: 2px solid var(--accent-fg); outline-offset: 1px;` sur `:focus-visible` uniquement (pas sur `:focus` au clic souris).
- Les couleurs ne sont jamais le seul vecteur d'information (ex : erreurs = icône + couleur, pas couleur seule).
- Tout composant de saisie numérique (Transform X/Y/Z, etc.) doit supporter : drag horizontal pour incrémenter (comme Unreal), double-clic pour édition texte directe, `Tab` pour passer au champ suivant.
- Aucune couleur codée en dur dans les composants — lint à prévoir (`no-hardcoded-colors`) si le projet a un linter CSS.

---

## 7. Priorité d'implémentation recommandée

1. Design tokens + `ThemeProvider` + switch Light/Dark
2. `DockManager` générique (sans contenu) + layout par défaut
3. EditorShell (chrome vide : titlebar, menubar, toolbar, statusbar)
4. Viewport3D (placeholder) + gizmo statique
5. WorldOutliner + DetailsPanel (branchés sur un état mock d'objets)
6. ContentBrowser + ThumbnailCard
7. ProjectNavigator + ClassViewer (réutilisent respectivement TreeView adapté et un arbre simple)
8. Launcher (peut être fait en parallèle, indépendant du reste)
9. MaterialEditor / BlueprintEditor (réutilisent NodeGraphCanvas commun)
10. ReferenceViewer (réutilise NodeGraphCanvas en lecture seule) + SizeMap (Treemap dédié)
11. Sequencer
12. Settings modaux + PackagingDialog
13. Polish : notifications, tooltips, context menus, raccourcis clavier globaux

---

**Fin du document 2/3.** Voir `03-specification-banani.md` pour les prompts de génération visuelle écran par écran destinés à Banani.
