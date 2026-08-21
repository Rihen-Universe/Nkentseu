# Spécification technique d'implémentation — Aetherion Animate & FX
### Document 5/6 — Destiné à Claude (agent d'implémentation)

> Étend `02-specification-claude.md` (design tokens, docking, composants génériques déjà définis — à réutiliser tels quels). Ce document ne redéfinit **que** ce qui est nouveau pour l'animation/VFX : tokens additionnels, arborescence de fichiers étendue, contrats de props des nouveaux composants, et check-list d'implémentation. Toujours partir de `02-specification-claude.md` comme socle.

---

## 1. Tokens additionnels

À ajouter à `/theme/tokens.css`, en plus des tokens déjà définis (doc 2 §1) :

```css
:root {
  /* Squelette / rigging */
  --bone-line: #3fd0e0;
  --bone-selected: var(--selection-outline);
  --bone-locked: #6e7781;
  --skin-weight-low: #1f6feb;   /* poids 0 */
  --skin-weight-high: #f85149;  /* poids 1 */

  /* Courbes d'animation (mêmes couleurs que les axes de transform) */
  --curve-x: var(--axis-x);
  --curve-y: var(--axis-y);
  --curve-z: var(--axis-z);
  --curve-tangent-handle: #d29922;

  /* AnimGraph */
  --pose-wire: #a371f7; /* câble de flux de pose, distinct des câbles data/exec */

  /* Onion skinning */
  --onion-past: #2f81f7;
  --onion-future: #e3833b;

  /* Workspace accent (liseré 2px sous le nom du workspace actif) */
  --workspace-rigging: #e3833b;
  --workspace-animation: #2f81f7;
  --workspace-vfx: #a371f7;
  --workspace-2d: #3fb950;

  /* VFX */
  --vfx-module-category-spawn: #3fb950;
  --vfx-module-category-init: #2f81f7;
  --vfx-module-category-update: #d29922;
  --vfx-module-category-render: #a371f7;
}
```

---

## 2. Arborescence de fichiers étendue

```
/src
  /app
    ...  (inchangé, voir doc 2 §2)
  /workspaces
    WorkspaceSwitcher.tsx
    workspaceLayouts.ts        → JSON de layout par workspace (réutilise le format DockNode du doc 2 §3)
  /panels/animation
    SkeletonTree/
    SkeletalMeshViewportOverlay/
    PhysicsAssetEditor/
    DopeSheet/
    CurveEditor/
    AnimGraphEditor/
    StateMachineEditor/
    BlendSpaceEditor/
    RetargetingEditor/
    MontageEditor/
    TakeBrowser/
    MocapImportWizard/
  /panels/2d
    Rig2DEditor/
    Rig2DTree/
    SpriteAtlasEditor/
    OnionSkinOverlay/
  /panels/vfx
    EmitterStack/
    VfxNodeGraphEditor/
    VfxPreviewViewport/
    VfxLibraryBrowser/
  /ui-kit
    ...  (inchangé, réutiliser Toolbar/TreeView/PropertyRow/NodeGraphCanvas/Modal/Toast existants)
    TransportBar/               → nouveau, générique
    DistributionField/          → nouveau composant de propriété
```

Règle inchangée du doc 2 : chaque panneau ne connaît rien du docking, reçoit sa taille par flex/grid, s'enregistre dans le `panelRegistry` global.

---

## 3. Système de Workspace (au-dessus du DockManager)

```ts
interface Workspace {
  id: "rigging" | "animation" | "vfx" | "2d";
  label: string;
  accentToken: string;              // ex. "--workspace-animation"
  defaultLayout: DockNode;          // même format que doc 2 §3.3
}
interface WorkspaceSwitcherProps {
  workspaces: Workspace[];
  activeWorkspaceId: Workspace["id"];
  onSwitch: (id: Workspace["id"]) => void;
  // Persistance : au switch, si l'utilisateur a déjà modifié le layout de ce
  // workspace dans la session, restaurer SA version modifiée plutôt que defaultLayout.
  perWorkspaceOverrides: Partial<Record<Workspace["id"], DockNode>>;
}
```
Implémentation : `WorkspaceSwitcher` appelle `DockManager.loadLayout(layout)` (même méthode que `Window > Layouts`, doc 2 §3.1) — ne pas créer un second mécanisme de layout, réutiliser celui existant avec une couche de présélection nommée.

---

## 4. Contrats de composants — Rigging / Animation

### 4.1 `<SkeletonTree>`
```ts
interface BoneNode {
  id: string; name: string;
  visible: boolean; locked: boolean;
  hasConstraint?: "ik" | "socket" | null;
  children?: BoneNode[];
}
interface SkeletonTreeProps {
  root: BoneNode;
  selectedBoneIds: string[];
  onSelect: (ids: string[], additive: boolean) => void;
  onToggleVisible: (id: string) => void;
  onToggleLocked: (id: string) => void;
  filterConstraintsOnly: boolean;
  onAddSocket: (boneId: string) => void;
  onAddIkConstraint: (boneId: string) => void;
}
```
Réutilise `<TreeView>` en interne avec une colonne d'icônes supplémentaire (verrou) — ne pas dupliquer le composant, étendre `extraColumns`/`extraRowActions` de `<TreeView>` si nécessaire (ajouter cette prop générique à `TreeView` plutôt que de forker un composant).

### 4.2 `<SkinWeightHeatmapOverlay>` (overlay du Viewport3D, pas un panneau séparé)
```ts
interface SkinWeightHeatmapOverlayProps {
  enabled: boolean;
  selectedBoneId: string | null;
  vertexWeights: Map<string /*vertexId*/, number /*0-1*/>;
  colorScale: [string, string]; // [--skin-weight-low, --skin-weight-high]
}
```

### 4.3 `<BoneWeightSlider>` (utilisé dans PropertyRow catégorie "Skin Weights")
```ts
interface BoneWeightSliderProps {
  boneName: string;
  weight: number;         // 0-1
  onChange: (w: number) => void;
  totalWeightAcrossBones: number; // affiché ailleurs, sert au warning si != 1.0
}
```

### 4.4 `<DopeSheet>`
```ts
interface AnimTrack {
  boneOrPropertyId: string; label: string;
  keyframeTimes: number[];         // vue résumée (mode Dope Sheet)
  channels?: { axis: "x"|"y"|"z"; keyframeTimes: number[] }[]; // vue détaillée si déplié
}
interface AnimNotify { id: string; time: number; kind: "sound"|"effect"|"custom"; label: string; }
interface DopeSheetProps {
  tracks: AnimTrack[];
  notifies: AnimNotify[];
  playheadTime: number;
  durationFrames: number;
  fps: number;
  onScrub: (time: number) => void;
  onMoveKeyframe: (trackId: string, oldTime: number, newTime: number) => void;
  mode: "dopeSheet" | "curveEditor";
  onModeChange: (m: DopeSheetProps["mode"]) => void;
}
```

### 4.5 `<CurveEditorCanvas>`
```ts
interface CurveKeyframe {
  time: number; value: number;
  interpolation: "auto" | "linear" | "constant" | "broken";
  inTangent?: number; outTangent?: number;
}
interface CurveChannel { id: string; color: string; keyframes: CurveKeyframe[]; visible: boolean; }
interface CurveEditorCanvasProps {
  channels: CurveChannel[];
  onKeyframeChange: (channelId: string, index: number, kf: CurveKeyframe) => void;
  onSetInterpolation: (channelId: string, index: number, interp: CurveKeyframe["interpolation"]) => void;
  onFrameSelection: () => void;
  smoothingStrength?: number;
}
```

### 4.6 `<StateMachineCanvas>` (extension de `NodeGraphCanvas`, doc 2 §4.4)
```ts
interface AnimStateNode extends GraphNode {
  clipPreviewThumbnail?: string;
  isEntryState?: boolean; // non supprimable, rond plein
}
interface StateTransition {
  id: string; fromState: string; toState: string;
  conditionSummary: string; // ex. "Speed > 0.1"
}
interface StateMachineCanvasProps {
  states: AnimStateNode[];
  transitions: StateTransition[];
  breadcrumb: string[];              // ex. ["AnimGraph", "Locomotion", "Jump"]
  onEnterState: (stateId: string) => void; // double-clic → sous-graphe
  onSelectTransition: (transitionId: string) => void; // ouvre l'éditeur de condition dans Details
}
```
Câbles de flux de pose : forcer `dataTypeColors["pose"] = "var(--pose-wire)"` et une épaisseur de trait supérieure (4px vs 2px pour les câbles data classiques) dans les props passées à `NodeGraphCanvas`.

### 4.7 `<BlendSpaceGrid>`
```ts
interface BlendSample { id: string; assetName: string; x: number; y: number; }
interface BlendSpaceGridProps {
  mode: "1d" | "2d";
  axisXLabel: string; axisYLabel?: string;
  samples: BlendSample[];
  previewPoint: { x: number; y: number };
  onPreviewPointChange: (p: {x:number;y:number}) => void;
  onAddSampleFromDrop: (assetId: string, pos: {x:number;y:number}) => void;
  showTriangulation: boolean;
}
```

### 4.8 `<RetargetMappingList>`
```ts
interface RetargetMapping {
  sourceBoneId: string | null;
  targetBoneId: string | null;
  autoMatched: boolean;
  isUnmapped: boolean; // true → surlignage rouge/orange
}
interface RetargetingEditorProps {
  sourceSkeleton: BoneNode;
  targetSkeleton: BoneNode;
  mappings: RetargetMapping[];
  onRemapByDrag: (sourceBoneId: string, targetBoneId: string) => void;
  onAutoMapByName: () => void;
  onAutoMapByProximity: () => void;
}
```

---

## 5. Contrats de composants — 2D

### 5.1 `<Rig2DEditor>` / `<Rig2DTree>`
Réutilise `SkeletonTree` (§4.1) en 2D — les os n'ont que rotation/longueur (pas de Z), adapter `BoneNode` avec un flag `is2D: true` désactivant les champs Z dans `PropertyRow`.

```ts
interface Slot2D { id: string; boneId: string; assignedAssetId: string | null; }
interface Rig2DEditorProps {
  bones: BoneNode[];
  slots: Slot2D[];
  onAssignSlotAsset: (slotId: string, assetId: string) => void; // via drop depuis ContentBrowser
  meshMode: boolean; // bascule "Mesh" (weight paint 2D)
  brushSize?: number; brushStrength?: number; // actifs seulement si meshMode
}
```

### 5.2 `<SpriteAtlasEditor>`
```ts
interface SpriteRect { id: string; name: string; x: number; y: number; w: number; h: number; pivot: {x:number;y:number}; }
interface SpriteAtlasEditorProps {
  atlasImageUrl: string;
  sprites: SpriteRect[];
  onUpdateRect: (id: string, rect: Partial<SpriteRect>) => void;
  onAutoSlice: (mode: "grid" | "alphaDetect", params: Record<string, number>) => void;
}
```

### 5.3 `<OnionSkinOverlay>`
```ts
interface OnionSkinOverlayProps {
  enabled: boolean;
  framesBefore: number; framesAfter: number;
  silhouetteOnly: boolean;
  pastColor: string;   // --onion-past
  futureColor: string; // --onion-future
  renderFrame: (offset: number) => ReactNode; // callback fourni par le viewport hôte
}
```

---

## 6. Contrats de composants — VFX

### 6.1 `<EmitterStack>`
```ts
type VfxModuleCategory = "spawn" | "initialize" | "update" | "render";
interface VfxModule {
  id: string; name: string; category: VfxModuleCategory;
  enabled: boolean; hasExposedParams: boolean;
}
interface VfxEmitter {
  id: string; name: string; particleType: "sprite" | "mesh" | "ribbon";
  enabled: boolean; activeParticleCount: number;
  modulesByCategory: Record<VfxModuleCategory, VfxModule[]>;
  expanded: boolean;
}
interface EmitterStackProps {
  emitters: VfxEmitter[];
  onToggleEmitter: (id: string) => void;
  onToggleExpand: (id: string) => void;
  onToggleModule: (emitterId: string, moduleId: string) => void;
  onReorderModule: (emitterId: string, category: VfxModuleCategory, fromIdx: number, toIdx: number) => void;
  onAddModule: (emitterId: string, category: VfxModuleCategory) => void; // ouvre recherche
  onSelectModule: (emitterId: string, moduleId: string) => void; // alimente Details Panel
}
```
Couleur d'en-tête de catégorie de module = `--vfx-module-category-*` correspondant.

### 6.2 `<DistributionField>` (nouveau type de `PropertyRow`)
```ts
type DistributionMode = "constant" | "randomRange" | "curve";
interface DistributionFieldProps {
  mode: DistributionMode;
  onModeChange: (m: DistributionMode) => void;
  constantValue?: number;
  rangeMin?: number; rangeMax?: number;
  curve?: CurveChannel; // réutilise §4.5
  onChange: (value: unknown) => void;
}
```
Ajouter `"distribution"` à l'union `PropertyType` du doc 2 §4.1 (`PropertyRow`) plutôt que de créer un composant totalement disjoint — `DistributionField` est le renderer interne appelé quand `type === "distribution"`.

### 6.3 `<VfxPreviewViewport>` (extension du Viewport3D, doc 2 §5 item 5)
```ts
interface VfxPreviewViewportProps {
  activeParticleCount: number;
  onResetSimulation: () => void;
  isolatedEmitterId: string | null;
  onIsolateEmitter: (id: string | null) => void;
  backgroundMode: "checker" | "hdri" | "black";
  simulationSpeed: number; // 0.1–2.0
  showGroundGrid: boolean;
}
```

---

## 7. `<TransportBar>` (générique, réutilisable Animation/VFX/2D)

```ts
interface TransportBarProps {
  currentFrame: number;
  totalFrames: number;
  fps: number;
  isPlaying: boolean;
  isLooping: boolean;
  playbackSpeed: 0.25 | 0.5 | 1 | 2;
  realtimeMode: boolean; // true = "Temps réel", false = "Frame par frame"
  onPlayPause: () => void;
  onStepFrame: (delta: 1 | -1) => void;
  onGoToStart: () => void;
  onGoToEnd: () => void;
  onScrub: (frame: number) => void;
  onToggleLoop: () => void;
  onSpeedChange: (s: TransportBarProps["playbackSpeed"]) => void;
  onToggleRealtimeMode: () => void;
}
```
Monté une fois sous la barre d'outils principale, visible seulement si `activeWorkspace !== null` (masqué en workspace "Rigging" pur, visible en Animation/VFX/2D). Piloté par le même état de lecture que le panneau actif (DopeSheet, VfxPreviewViewport, OnionSkinOverlay s'y abonnent tous via un store partagé `usePlaybackStore()`), pour rester synchronisés sans prop-drilling profond.

---

## 8. Check-list d'implémentation (nouveaux écrans uniquement)

1. `WorkspaceSwitcher` + 4 layouts par défaut (JSON) — dépend du `DockManager` déjà existant (doc 2).
2. `TransportBar` + `usePlaybackStore` — brique transverse à faire tôt, tout le reste en dépend.
3. `SkeletonTree` (extension de `TreeView`) + overlay squelette dans le Viewport3D.
4. `DetailsPanel` : ajouter les catégories Bone / Skin Weights / IK Constraint (extension, pas nouveau composant).
5. `PhysicsAssetEditor` (viewport + Bodies list + Details étendu).
6. `DopeSheet` + `CurveEditor` (toggle du même panneau, pas deux panneaux séparés).
7. `StateMachineCanvas` + `AnimGraphEditor` (réutilisent `NodeGraphCanvas`).
8. `BlendSpaceGrid`.
9. `RetargetingEditor` + `MocapImportWizard` (réutilise `RetargetMappingList`).
10. `MontageEditor` (réutilise le rendu de `DopeSheet` pour les pistes/segments, ajoute les blocs de clip redimensionnables).
11. `Rig2DEditor` + `SpriteAtlasEditor` + `OnionSkinOverlay`.
12. `EmitterStack` + `VfxNodeGraphEditor` (réutilise `NodeGraphCanvas`) + `DistributionField` (extension de `PropertyRow`) + `VfxPreviewViewport`.
13. `VfxLibraryBrowser` / `TakeBrowser` (variantes filtrées du `ContentBrowser` existant — passer une prop `typeFilter` plutôt que forker le composant).
14. Export dialogs (FBX/Alembic, Send to game project, Compression report) — réutilisent `<Modal>` + `<StepProgressList>` du doc 2.

**Principe directeur** : à chaque étape, vérifier d'abord si un composant du doc 2 (`TreeView`, `NodeGraphCanvas`, `PropertyRow`, `ThumbnailCard`, `Modal`) peut être étendu par une prop plutôt que dupliqué. Cette suite partage volontairement son design system avec le moteur de jeu (docs 1-3) ; toute divergence de style entre les deux produits est un bug.

---

**Fin du document 5/6.** Voir `06-specification-banani-animation-vfx.md` pour les prompts de génération visuelle écran par écran.
