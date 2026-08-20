# NkUIDesign — Spécification technique d'implémentation
### Document 5 — Destiné à Claude (agent d'implémentation)

> Traduit `3_NkUIDesign_Interface_Humaine.md` en contraintes techniques. Réutilise
> tels quels les design tokens et composants génériques déjà définis dans
> `02-specification-claude.md` (Aetherion Engine) : `TreeView`, `PropertyRow`,
> `NodeGraphCanvas`, `Modal`, `Toast`, `Toolbar`, `TabStrip`, tokens de couleur
> Light/Dark. Ce document ne redéfinit que ce qui est nouveau : le modèle de
> document `.nkgui`, le système de Dock Rail à pastilles, le canvas infini
> multi-pages, et l'intégration du Behavior Compiler.

---

## 1. Modèle de document (Core Document Model)

Toute l'application lit/écrit un unique état normalisé, reflet direct de la
grammaire du document 2 :

```ts
interface NkGuiDocument {
  version: string;                     // "0.2"
  includes: string[];
  geometry: ShapeNode[];               // section geometry, optionnelle
  widgets: WidgetNode[];               // racines = pages/cadres
  behaviors: BehaviorProgram[];        // script ou graph, cf. §3
  animations: WidgetAnimationProgram[]; // §3bis — un par EventBinding kind="animation"
  contracts: { controllers: ControllerDecl[]; callbacks: CallbackSig[] };
}

interface ShapeNode {
  id: string; kind: "rect"|"ellipse"|"text"|"image"|"path"|"frame";
  transform: { x:number; y:number; w:number; h:number; rotation?:number };
  style: Record<string, unknown>;      // fond, bordure, ombre, typo...
  effects?: EffectStack;               // §3bis — ombres/flou/dégradés/fusion
  vectorData?: VectorPath;             // présent seulement si kind === "path"
  promotedWidgetId?: string;           // lien vers WidgetNode si promu
  aiGenerated?: boolean;               // badge "Généré par IA" (doc 3 §17)
}

// --- Édition vectorielle au sommet (doc 3 §8bis) ---
type AnchorCornerType = "hard" | "mirrored" | "asymmetric" | "free";
interface PathAnchor {
  id: string; position: { x: number; y: number };
  inHandle?: { x: number; y: number };   // relatif à position
  outHandle?: { x: number; y: number };
  cornerType: AnchorCornerType;
  cornerRadius?: number;                 // seulement si cornerType === "hard"
}
interface VectorPath {
  anchors: PathAnchor[];
  closed: boolean;
  booleanOp?: "union" | "subtract" | "intersect" | "exclude"; // si tracé composé
  sourcePathIds?: string[];              // tracés d'origine, pour "Détacher les composants"
}

// --- Effets (doc 3 §8ter) ---
interface EffectStack {
  fills: FillEffect[];
  strokes: StrokeEffect[];
  shadows: ShadowEffect[];        // ordonnés, réordonnables
  layerBlur?: number;
  backgroundBlur?: number;
  blendMode: "normal"|"multiply"|"screen"|"overlay"|"darken"|"lighten";
  opacity: number;                // 0-1, distinct de l'opacité par fill
}
interface FillEffect {
  kind: "solid" | "linearGradient" | "radialGradient" | "angularGradient" | "diamondGradient" | "image";
  color?: string;
  stops?: { offset: number; color: string }[];  // dégradés
  imageUrl?: string; imageFit?: "cover"|"contain"|"tile"|"stretch";
  opacity: number;
}
interface StrokeEffect {
  fill: FillEffect; width: number;
  position: "inside" | "center" | "outside";
  dashPattern?: number[];
}
interface ShadowEffect {
  offsetX: number; offsetY: number; blur: number; spread: number;
  color: string; kind: "drop" | "inner";
}

interface WidgetNode {
  id: string; role: string;            // "Button","SliderFloat"... (catalogue doc 2 §8)
  label: string;                       // second argument String de node_decl
  props: Record<string, unknown>;      // typé par rôle, validé contre le schéma
  events: EventBinding[];
  children: WidgetNode[];
  isPage?: boolean;                    // racine = page/cadre du canvas infini
  pagePosition?: { x: number; y: number }; // position du cadre sur le canvas infini

  // --- Bibliothèque de composants (doc 3 §14bis) ---
  isComponentMaster?: boolean;         // true = ce nœud est stocké dans la Bibliothèque
  instanceOf?: string;                 // id du composant maître si c'est une instance
  overrides?: Record<string, unknown>; // propriétés localement modifiées (clé = chemin de propriété)
                                        // présence d'une clé ici → InstanceOverrideBadge affiché
}

interface EventBinding {
  eventName: string;                   // catalogue doc 2 §9
  target:
    | { kind: "callback"; ref: string }
    | { kind: "behavior"; ref: string }
    | { kind: "inline"; program: BehaviorProgram }
    | { kind: "animation"; ref: string };   // §3bis — déclenche une transition d'AnimationProgram
}

// --- Système d'animation de widget (doc 3 §9bis) ---
// Réutilise intégralement les types définis dans
// 05-specification-claude-animation-vfx.md (CurveChannel, AnimStateNode,
// StateTransition) plutôt que d'en recréer une variante.
interface WidgetAnimationProgram {
  id: string; name: string;
  scope: "widget" | "group" | "page";        // cf. BehaviorScopeSelector, réutilisé ici
  states: AnimStateNode[];                    // type importé du package animation partagé
  transitions: (StateTransition & { triggerEvent: string; durationMs: number; easingPresetOrCurve: "linear"|"easeIn"|"easeOut"|"easeInOut"|"bounce"|"elastic"|CurveChannel })[];
  animatedProperties: Record<string /*stateId*/, Record<string /*propPath*/, CurveChannel | unknown>>;
}
```

**Règle non négociable** (doc 1 §3) : ce modèle est la **seule** source de
vérité. Le canvas Design, la Hiérarchie, l'Inspecteur, le canvas Behavior et
le chat IA lisent/écrivent tous dans `NkGuiDocument` — jamais d'état parallèle
dupliqué par panneau. Chaque panneau tient un `useNkGuiDocumentStore()`
partagé (ou équivalent), pas de copie locale qui pourrait diverger.

---

## 1ter. Cible, ancrage, responsive, alignement — additions au modèle

> Découle de `3_NkUIDesign_Interface_Humaine.md` §8quater. Ces quatre notions
> entrent dans le **document**, pas dans l'éditeur : elles doivent survivre à la
> sérialisation et être relues à l'identique.

### 1ter.1 Cible portée par le cadre

```
NkFrameTarget {
    kind      : Desktop | Mobile | Tablet | Free
    width     : uint32        // px logiques
    height    : uint32
    density   : float32       // px physiques par px logique, défaut 1.0
    safeArea  : { top, right, bottom, left : uint32 }
}
```

⚠️ **La cible n'est pas cosmétique** : elle fournit la zone sûre (règle du corpus
« la zone sûre est une ancre, pas une marge »), la densité de référence, et la
taille de cible tactile minimale que la validation vérifie. Un cadre sans cible
explicite vaut `Free` **et le déclare** — il ne prend pas silencieusement les
valeurs du bureau.

### 1ter.2 Ancrage — quatre arêtes, relatives au PARENT

```
NkAnchorEdge { mode : None | Fixed | Proportional ; value : float32 }
NkAnchors    { left, right, top, bottom : NkAnchorEdge }
```

Règles de résolution, dans cet ordre :

1. deux arêtes opposées `Fixed` ou `Proportional` → l'élément **s'étire** sur cet
   axe, sa taille devient dépendante du parent ;
2. une seule arête active → distance conservée à ce bord, taille libre ;
3. aucune → **centré** sur cet axe.

⚠️ **Relatives au parent, jamais au cadre.** Un ancrage résolu contre le cadre
casserait dès qu'un élément change de parent — et le déplacement de parent est un
geste courant dans l'arbre de composition.

### 1ter.3 Taille — réutiliser le vocabulaire existant

`fixed <n>` · `content` · `fraction <n>` · `weight <n>` · `expand`

⚠️ **Ne pas introduire un second vocabulaire.** L'aperçu écrit déjà `fixed 260` →
`fixed 340` quand on tire un bord, et la sonde le vérifie. Deux vocabulaires pour
une même chose = deux sources de vérité, et c'est le défaut qui a laissé vivre le
magenta.

**Bornes.** Chaque dimension porte en plus un couple de bornes optionnelles :

```
NkSizeBounds {
    minPx : Optional<uint32>     // absent = pas de borne basse
    maxPx : Optional<uint32>     // absent = pas de borne haute
}
```

⚠️ **`Optional`, pas une sentinelle.** `0` pour « pas de minimum » et `UINT32_MAX`
pour « pas de maximum » sont des valeurs légitimes qu'un concepteur peut vouloir
saisir ; les employer comme sentinelles rend « borne à zéro » et « aucune borne »
indistinguables dans le modèle, donc dans le fichier, donc dans le diff.
L'interface les rend par un tiret `—` et non par un champ vide (doc 3 §8quater.2).

**Ordre d'application, normatif et non négociable :**

```
1. mode      -> taille candidate (fixed | content | fraction | weight | expand)
2. bounds    -> clamp(candidate, minPx, maxPx)
3. remplissage du parent  -> retranché de l'espace offert AVANT l'étape 1
```

⚠️ **Le clamp intervient après le mode, jamais avant.** Un `expand` borné à 320
prend l'espace restant *puis* est ramené à 320 ; l'espace libéré retourne à la
répartition entre frères. Clamper d'abord donnerait une répartition calculée sur
une taille que personne n'occupe.

**Validation :**

| code | condition |
|---|---|
| `E-BOUNDS-INVERTED` | `minPx > maxPx` — refusé à la saisie, pas arbitré |
| `W-BOUNDS-UNREACHABLE` | le mode ne peut jamais atteindre l'intervalle (ex. `fraction 1` avec `maxPx` très inférieur à la dimension du parent) |
| `W-BOUNDS-INERT` | bornes posées sur un `fixed` — sans effet, signalées, non supprimées |

⚠️ **`W-BOUNDS-INERT` avertit, il ne nettoie pas.** Une borne inerte aujourd'hui
redevient active dès que le mode change ; l'effacer d'office détruirait une
intention au moment précis où elle est invisible.

**Pas de troisième champ de taille.** Le modèle porte `mode` (avec son paramètre)
et `bounds`. Aucune `defaultSize`. Sur `fixed`, un tel champ dupliquerait le
paramètre du mode ; sur `expand`/`content`, il désignerait une valeur que
personne ne calcule.

```
NkSizeSpec {
    mode   : Fixed(u32) | Content | Fraction(f32) | Weight(f32) | Expand
    bounds : NkSizeBounds
}
```

**Mémoire d'édition — hors modèle, hors fichier :**

```
// Etat d'EDITEUR uniquement. Jamais serialise dans .nkgui.
NkSizeModeMemory {
    lastFixed    : Optional<uint32>
    lastFraction : Optional<float>
    lastWeight   : Optional<float>
}
```

⚠️ **Cette mémoire ne va pas dans le document.** La sérialiser la ferait
apparaître dans les diffs, les revues et les fusions, pour une valeur sans aucun
effet sur le rendu — du bruit que les relecteurs apprendraient à ignorer, ce
qu'ils finissent toujours par faire aussi pour le reste.

**Rendu de la ligne (doc 3 §8quater.2) :** une ligne par dimension ; marqueur
lecture seule `min–max` affiché **si et seulement si** `bounds` n'est pas vide
(`…` du côté absent) ; clic = popover `mode + min + max`.

⚠️ **Le marqueur n'est pas décoratif, il est la condition du popover.** Masquer
les champs sans afficher l'existence de la borne échange un encombrement mesuré
contre une classe de bug non mesurable.

### 1ter.4 Points de rupture

```
NkBreakpoint {
    axis       : Width | Height
    comparator : Below | AtLeast
    threshold  : uint32
    overrides  : liste de (propriété, valeur)   // ancrage, taille, visibilité
}
```

Portés par l'élément, évalués **du plus général au plus spécifique**, la valeur de
base restant toujours présente et lisible.

⚠️ **Un point de rupture inatteignable doit se signaler** — seuil de 2000px sur un
cadre mobile de 390px. La validation le compte et le nomme ; elle ne le supprime
pas. *Une règle morte qui ne se dit pas est du travail perdu qu'on croit fait.*

### 1ter.5 Marge, remplissage, espacement

```
NkEdges { top, right, bottom, left : float32 }

NkSpacing {
    padding : NkEdges      // bord -> contenu, vers l'interieur
    margin  : NkEdges      // bord -> voisinage, vers l'exterieur
    gap     : { h, v : float32 }   // entre enfants
}
```

⚠️ **La règle de composition avec l'ancrage, à implémenter une seule fois et à
documenter dans le code** :

> **Le `padding` du parent définit sa zone de contenu. Le `margin` de l'enfant
> définit sa boîte de marge. L'ancre se mesure entre les deux.**

Donc `distance visible = padding(parent) + ancre + margin(enfant)`. **Les trois
s'additionnent ; aucun ne remplace l'autre.** Toute autre convention produira des
écarts de quelques pixels que personne ne saura expliquer — et qu'on attribuera au
moteur de rendu.

⚠️ **`gap` n'est pas simulable par des marges** et ne doit pas être implémenté
ainsi : des marges de `n/2` sur chaque enfant donnent bien `n` entre eux, **mais
aussi `n/2` contre les bords du parent**. La sérialisation doit distinguer les deux
cas, sinon relire un document produit une disposition différente de celle
enregistrée.

---

### 1ter.6 Alignement

```
NkAlign      { Start | Center | End | Stretch }
NkDistribute { None | SpaceBetween | SpaceAround | SpaceEvenly }

NkLayoutAlign {
    self     : { h : NkAlign, v : NkAlign }        // dans le parent
    content  : { h : NkAlign, v : NkAlign, d : NkDistribute }  // des enfants
}
NkTextAlign {
    horizontal : Start | Center | End | Justify
    vertical   : Start | Center | End
    baseline   : bool     // aligner sur la ligne de base plutôt que sur la boîte
}
```

⚠️ `baseline` n'est pas un raffinement typographique : deux textes de corps
différents posés côte à côte paraissent bancals sans lui, **et personne ne sait
dire pourquoi**. C'est exactement le genre de défaut qui se ressent avant de se
voir.

---

## 1quater. Rôle d'un composant — le contrat

> Découle de `3_NkUIDesign_Interface_Humaine.md` §14ter.

```
NkRoleContract {
    id             : NkString              // "button", "checkbox", …
    states         : liste de NkStateId    // repos, survol, pressé, désactivé, focus
    events         : liste de NkEventDecl  // nom + charge utile
    properties     : liste de NkPropDecl
    expectedChildren : liste de NkChildSlot // libellé requis, icône optionnelle…
    a11y           : { minTargetPx : uint32, minContrast : float32 }
}

NkRoleBinding {                 // ce que porte l'élément
    contract     : NkRoleContract*   // nul si aucun rôle
    userEvents   : liste de NkEventDecl   // ajoutés par l'utilisateur
    mutedEvents  : liste de NkEventId     // du contrat, désactivés
    stateOverrides : ...                  // apparence redéfinie par état
}
```

**Trois invariants, à faire respecter par le code :**

1. **`userEvents` survit au retrait du rôle.** Détacher un contrat vide
   `contract`, `mutedEvents` et `stateOverrides` — **jamais** `userEvents`. On perd
   le contrat, on ne perd pas le travail.
2. **Un événement du contrat n'est jamais supprimé, seulement `muted`.** Sinon
   réattribuer le rôle plus tard ne le restaure pas.
3. **`expectedChildren` non satisfait n'est pas une erreur** — c'est un
   **avertissement de validation nommé**. Un bouton sans libellé est peut-être un
   bouton-icône délibéré.

⚠️ **La composition est autorisée** : un élément porte un rôle **et** peut contenir
des enfants qui portent les leurs. Aucune règle d'exclusion.

---

## 1bis. Extensions de langage nécessaires — proposition pour le document 2

**Point d'attention important** : la grammaire actuelle de
`2_NkUIDesign_Langage_Description_NodeBlueprint.md` (§3) ne couvre pas
encore les structures composées requises par les nouveautés de ce document
(§1). `shape_prop := Identifier '=' value` avec `value` limité à
`String|Number|Color|Vec2|flags|Identifier` ne peut pas exprimer nativement
une liste d'ancres de tracé, une pile d'ombres, des arrêts de dégradé, ou une
courbe d'animation. Proposition minimale à valider côté langage (non
implémentée par ce document, qui se contente de la présupposer) :

- Un type `value` étendu avec `array := '[' value (',' value)* ']'` et
  `object := '{' (Identifier '=' value)* '}'` (structures anonymes
  imbriquées), suffisant pour représenter `EffectStack`, `VectorPath` et les
  arrêts de dégradé sans nouveaux mots-clés.
- Un nouveau `shape` de `kind = "path"` acceptant une propriété `anchors` de
  type `array` d'`object` (§1 `PathAnchor`).
- Une nouvelle section top-level `animation`, symétrique de `behavior` :
  `animation_sec := "animation" String '{' state_decl* transition_decl* '}'`
  — mêmes familles de nœuds `node`/`wire` que `behavior ... graph` pour la
  cohérence de compilateur (doc 2 §6), mais un vocabulaire de nœuds différent
  (`StateNode`, `Transition`, `AnimatedProperty`) plutôt que le catalogue
  §6.2 du document 2.
- Un mot-clé `component`/`instance` dans `widgets_sec`, avec un identifiant
  de composant maître et un bloc d'overrides — équivalent structurel à un
  `node_decl` classique mais avec un flag de provenance.

Tant que le document 2 n'est pas mis à jour en conséquence, considérer ces
structures comme **réservées** dans le modèle en mémoire (§1) : le parseur
actuel (§2) sait les lire s'il est étendu en parallèle, mais le langage texte
`.nkgui` documenté aujourd'hui ne les couvre pas encore formellement.

---

## 2. Parser/Sérialiseur `.nkgui`

- Implémenté comme module isolé `nkgui-parser` (idéalement portable vers le
  runtime C++ comme l'exige doc 1 §5 — côté web/éditeur, prévoir une
  implémentation TypeScript qui suit **exactement** la grammaire EBNF du
  document 2 §3, avec les mêmes codes d'erreur §12, pour que le rapport de
  validation (doc 3 §19) soit identique à ce que produirait le vrai
  compilateur).
- Round-trip obligatoire : `serialize(parse(text)) === normalize(text)` doit
  être un test automatisé dès l'implémentation du module.
- Le parseur alimente `NkGuiDocument` (§1) ; le sérialiseur en repart pour
  écrire un `.nkgui` texte, y compris le découpage `include` choisi dans le
  dialogue d'export (doc 3 §19).

---

## 3. Behavior Compiler — représentation intermédiaire commune

```ts
type IRInstruction =
  | { kind: "Assign"; target: string; value: IRExpr }
  | { kind: "Branch"; cond: IRExpr; then: IRInstruction[]; else?: IRInstruction[] }
  | { kind: "Call"; callback: string; args: IRExpr[] }
  | { kind: "ForEachChild"; parentRef: string; body: IRInstruction[] };

type IRExpr =
  | { kind: "literal"; value: string|number|boolean }
  | { kind: "var"; name: string }
  | { kind: "binop"; op: string; left: IRExpr; right: IRExpr };

interface BehaviorProgram {
  id: string; name: string;
  representation: "code" | "graph";
  ir: IRInstruction[];              // toujours dérivable, même si representation="graph"
  graphOnly?: boolean;              // true si hors sous-ensemble représentable en Code (doc 2 §6.4)
  sourceCode?: string;              // vue Code, générée en pretty-print si graphOnly
  graph?: { nodes: GraphNode[]; edges: GraphEdge[] }; // réutilise le type du doc 2 §4.4
}
```

- Le composant `<CanvasModeSwitch>` (§6) ne fait **jamais** de conversion
  lui-même : il change seulement quelle vue (`sourceCode` vs `graph`) est
  affichée. La conversion Code⇄Graph passe toujours par la compilation vers
  `ir` puis la régénération de l'autre front, dans le module
  `behavior-compiler`, jamais bricolée dans le composant UI.
- Poser `graphOnly = true` dès que l'IR contient une boucle non-`ForEachChild`
  ou une construction hors grammaire §5 du doc 2 → la vue Code bascule alors
  en lecture seule avec un bandeau d'avertissement (doc 2 §6.4).

---

## 4. Tokens additionnels

Réutiliser intégralement `02-specification-claude.md` §1. Ajouts spécifiques
à NkUIDesign :

```css
:root {
  /* Rôle promu vs forme brute */
  --role-badge-bg: var(--accent-subtle);
  --role-badge-fg: var(--accent-fg);

  /* Statuts de callback (Inspecteur Behavior, doc 3 §12.2) */
  --status-unbound: var(--fg-muted);
  --status-bound: var(--accent-fg);
  --status-invalid: var(--danger-fg);

  /* Guides de snapping (canvas Design, doc 3 §8) */
  --snap-guide: #ff4fd8;   /* rose/violet façon Figma, volontairement hors palette GitHub pour rester visible sur tout fond */

  /* Badge IA */
  --ai-badge-fg: var(--done-fg);

  /* Rail de pastilles */
  --dock-rail-bg: var(--bg-subtle);
  --dock-rail-width: 28px;
  --dock-rail-overlay-shadow: var(--shadow-elevation);
}
```

---

## 5. Arborescence de fichiers

```
/src
  /app
    Launcher/
    EditorShell/
      TitleBar.tsx
      ProjectTabStrip.tsx
      DesignToolbar.tsx
  /canvas
    InfiniteDesignCanvas/         → §8 doc humain
    VectorPathEditor/              → §8bis, mode d'édition de points (overlay du canvas Design)
    BehaviorCanvas/                → §9 (réutilise NodeGraphCanvas + éditeur code)
    WidgetAnimationCanvas/         → §9bis (réutilise StateMachineCanvas + DopeSheet + CurveEditor)
    SplitCanvasView.tsx            → §10, gère le diviseur + choix de paire + sync de sélection
  /panels
    Hierarchy/                     → panneau fixe, §11
    Inspector/
      InspectorDesignTab.tsx
      InspectorWidgetTab.tsx
      InspectorBehaviorTab.tsx
      EffectsStackPanel.tsx         → §8ter, section de l'onglet Design
      SceneObjectsGrid.tsx         → vue par défaut si rien sélectionné, §12.1
    ComponentPalette/              → contenu pastille, §14
    ComponentLibrary/              → contenu pastille, §14bis
    CallbackManager/                → contenu pastille, §15
    AiChatPanel/                    → contenu pastille + popover contextuel, §16
    ConsoleValidation/
    PreviewTest/
  /dock-rail
    DockRail.tsx                   → rail générique gauche/droite/bas
    DockRailPill.tsx                → composant pastille à 4 états
    useDockRailState.ts             → state machine (§6.3)
  /document
    NkGuiDocumentStore.ts           → état partagé unique (§1)
    nkgui-parser/                   → parser/sérialiseur (§2)
    behavior-compiler/              → IR + conversion Code⇄Graph (§3)
  /ai
    aiGenerationService.ts          → tous les points d'entrée (§7), un seul service
```

---

## 6. Contrats de composants

### 6.1 `<ProjectTabStrip>`
```ts
interface ProjectTab { id: string; name: string; isDirty: boolean; thumbnailUrl?: string; }
interface ProjectTabStripProps {
  tabs: ProjectTab[];
  activeTabId: string;
  onSelect: (id: string) => void;
  onClose: (id: string) => void;
  onReorder: (fromIdx: number, toIdx: number) => void;
  onDetachToNewWindow: (id: string) => void;   // drag hors de la bande
  onNewProject: () => void;                     // bouton "+", ouvre Launcher modal
}
```

### 6.2 `<CanvasModeSwitch>`
```ts
type CanvasMode = "design" | "behavior" | "split";
type SplitOrientation = "vertical" | "horizontal";
interface CanvasModeSwitchProps {
  mode: CanvasMode;
  splitOrientation: SplitOrientation;
  onModeChange: (m: CanvasMode) => void;
  onSplitOrientationChange: (o: SplitOrientation) => void;
}
```

### 6.3 `<DockRailPill>` — state machine
```ts
type PillState = "collapsed" | "overlay" | "docked" | "floating";
interface DockRailPillProps {
  id: string; icon: string; label: string;
  state: PillState;
  onStateChange: (s: PillState) => void;
  badgeCount?: number;               // ex. Chat IA en attente de réponse
  renderPanelContent: () => ReactNode;
}
```
Règles de transition à implémenter dans `useDockRailState.ts` (pas dans le
composant, pour rester testable indépendamment de React) :
- `collapsed → overlay` : clic simple sur la pastille.
- `overlay → collapsed` : clic sur la pastille à nouveau, clic hors panneau,
  ou `Échap`.
- `overlay → docked` : clic sur l'icône "épingle" dans l'en-tête du panneau
  overlay ; insère le panneau dans le `DockManager` (réutilise le système du
  doc 2 §3) à un emplacement par défaut adjacent au rail d'origine.
- `docked → overlay` ou `docked → floating` : clic sur l'icône "détacher".
- **Contrainte de rail** (doc 3 §13.3) : au moment où une pillule passe à
  `overlay`, toute autre pastille du **même rail** actuellement en `overlay`
  repasse à `collapsed` automatiquement — sauf les pastilles en `docked` ou
  `floating`, non affectées par cette règle. Implémenter via un registre par
  rail (`railId → activeOverlayPillId | null`), pas par un simple `useState`
  local à chaque pastille (elles doivent se coordonner).
- Cette règle est désactivable globalement depuis les Préférences (doc 3
  §20) : prévoir `enforceSingleOverlayPerRail: boolean` dans les settings
  globaux, lu par `useDockRailState`.

### 6.4 `<InfiniteDesignCanvas>`
```ts
interface InfiniteDesignCanvasProps {
  pages: WidgetNode[];              // isPage === true, avec pagePosition
  shapes: ShapeNode[];
  selection: string[];
  onSelectionChange: (ids: string[], additive: boolean) => void;
  viewport: { x: number; y: number; zoom: number };
  onViewportChange: (v: InfiniteDesignCanvasProps["viewport"]) => void;
  showGrid: boolean; showRulers: boolean; snapEnabled: boolean;
  onFrameToPage: (pageId: string) => void;   // double-clic depuis Hiérarchie/SceneObjectsGrid
  onPromote: (shapeId: string, role: string) => void;
  onDemote: (widgetId: string) => void;
}
```
Rendu recommandé : un seul système de coordonnées monde partagé entre toutes
les pages (elles ne sont que des `WidgetNode` racines positionnées par
`pagePosition`, pas des documents séparés) — le "zoom to fit" sur une page
n'est qu'un calcul de viewport centré sur son `transform`, pas un changement
de contexte de rendu.

### 6.5 `<SceneObjectsGrid>` (Inspecteur, aucune sélection)
```ts
interface SceneObjectsGridProps {
  pages: { id: string; name: string; thumbnailUrl?: string }[];
  onSelectPage: (id: string) => void;   // sélectionne + appelle onFrameToPage du canvas
  onCreatePage: () => void;
}
```

### 6.5bis `<InspectorPanel>` — ordre des sections et sélection multiple

```ts
type InspectorSection =
  | "target" | "layout" | "anchors" | "align" | "spacing"
  | "appearance" | "typography" | "effects" | "breakpoints" | "parentLayout";

// Ordre d'affichage NORMATIF (doc 3 §12.2). Une section absente disparaît,
// elle ne se rend jamais vide.
const SECTION_ORDER: InspectorSection[] = [
  "target", "layout", "anchors", "align", "spacing",
  "appearance", "typography", "effects", "breakpoints", "parentLayout",
];

type FieldValue<T> = { kind: "same"; value: T } | { kind: "mixed" };

interface InspectorPanelProps {
  selection: ElementId[];                        // 0 = SceneObjectsGrid (§6.5)
  visibleSections: InspectorSection[];           // intersection sur toute la sélection
  collapsed: Record<InspectorSection, boolean>;  // mémorisé PAR TYPE d'élément
  activeTab: "design" | "widget" | "behavior";
}
```

⚠️ **`SECTION_ORDER` est une constante, jamais un tri calculé.** Un ordre dérivé
des données varie d'un élément à l'autre et oblige l'œil à relire le panneau à
chaque sélection ; c'est le genre de coût qu'on ne mesure pas et qui se paie à
chaque clic.

⚠️ **`FieldValue` a un état `mixed` explicite.** En sélection multiple, afficher la
valeur du premier élément à la place de `mixed` est un mensonge silencieux :
l'utilisateur croit constater une uniformité, et sa première édition la crée
au lieu de la préserver. Écrire dans un champ `mixed` applique à toute la
sélection — c'est l'opération, pas un effet de bord.

⚠️ **`collapsed` est indexé par type d'élément.** Un état replié global fait
réapparaître Typographie dépliée sur un panneau qui ne porte pas de texte.

⚠️ **Un champ inapplicable se grise ; une section inapplicable disparaît.** Les
deux règles sont différentes exprès : garder la ligne préserve la position que
l'œil a mémorisée, garder la section ferait croire à une propriété manquante.

### 6.6 `<InspectorBehaviorTab>`
```ts
interface EventRow {
  eventName: string;
  status: "unbound" | "bound" | "invalid";
  boundProgramId?: string;
}
interface InspectorBehaviorTabProps {
  targetKind: "widget" | "page";
  roleEvents:   EventRow[];   // issus du contrat de rôle (doc 2 §9), ou portée "page"
  customEvents: EventRow[];   // ajoutés à la main par le concepteur (doc 3 §14ter)
  onSelectEvent: (eventName: string) => void; // bascule CanvasModeSwitch → "behavior", filtre le graphe sur cet event
  onAddCustomEvent: () => void;
  onWithdrawRoleEvent: (eventName: string) => void;
  onRestoreRoleEvent:  (eventName: string) => void;
}
```
Couleur de la pastille de statut = `--status-unbound` / `--status-bound` /
`--status-invalid` (§4).

⚠️ **`roleEvents` et `customEvents` sont deux listes distinctes dans le modèle, pas
une liste avec un drapeau d'origine.** Retirer le rôle emporte la première et doit
laisser la seconde intacte (doc 3 §14ter) ; une liste unique rendrait cette
opération dépendante d'un filtrage correct, c'est-à-dire réversible par un bug.

⚠️ **Un événement de rôle `withdrawn: true` reste dans `roleEvents`, barré et
restaurable.** Le supprimer rendrait « écarté volontairement » et « jamais vu »
indistinguables.

### 6.7 `<BehaviorScopeSelector>`
```ts
type BehaviorScope = "component" | "page" | "global";
interface BehaviorScopeSelectorProps {
  scope: BehaviorScope;
  currentTargetName: string;         // nom du composant/page/"Projet" affiché
  onScopeChange: (s: BehaviorScope) => void;
}
```

### 6.8 `<AiChatPanel>` / `<AIContextButton>` / `<AIPreviewCard>`
```ts
interface AiChatMessage {
  id: string; role: "user" | "assistant";
  text: string;
  previewCard?: { summary: string; thumbnailUrl?: string; diffKind: "add"|"modify"|"behavior" };
}
interface AiChatPanelProps {
  messages: AiChatMessage[];
  onSend: (prompt: string, quickAction?: "generateScreen"|"editSelection"|"generateBehavior") => void;
  onApplyPreview: (messageId: string) => void;
  onRejectPreview: (messageId: string) => void;
  isFloating: boolean;               // reflète l'état "floating" du DockRailPill parent
}
interface AIContextButtonProps {
  anchorSelectionBounds: { x:number;y:number;w:number;h:number };
  onOpenPopover: () => void;         // ouvre un mini-panel, PAS le AiChatPanel complet
}
```
Toutes les générations, quel que soit le point d'entrée (§17 doc humain),
passent par `aiGenerationService.generate(context) → Promise<DocumentPatch>`
— un `DocumentPatch` est un diff partiel du `NkGuiDocument`, jamais appliqué
tant que `onApplyPreview` n'est pas appelé. Un seul service, pas un par point
d'entrée, pour garantir la garantie « aperçu avant application » (doc 1 §6.2)
de façon centralisée plutôt que reproduite N fois.

### 6.9 `<VectorPathEditor>` (édition au sommet, doc 3 §8bis)
```ts
interface VectorPathEditorProps {
  path: VectorPath;
  onAnchorMove: (anchorId: string, pos: {x:number;y:number}) => void;
  onHandleMove: (anchorId: string, handle: "in"|"out", pos: {x:number;y:number}) => void;
  onSetCornerType: (anchorId: string, type: AnchorCornerType) => void;
  onSetCornerRadius: (anchorId: string, radius: number) => void;
  onAddAnchorOnSegment: (afterAnchorId: string, t: number) => void;
  onDeleteAnchor: (anchorId: string) => void;
  onBooleanOp: (op: VectorPath["booleanOp"], shapeIds: string[]) => void;
  onDetachComponents: (shapeId: string) => void;
}
```
Overlay du `InfiniteDesignCanvas`, pas un canvas séparé — actif uniquement
en mode édition de points (double-clic d'une forme, ou outil Tracé actif).

### 6.10 `<EffectsStackPanel>` (doc 3 §8ter)
```ts
interface EffectsStackPanelProps {
  effects: EffectStack;
  onChangeFill: (index: number, fill: FillEffect) => void;
  onAddShadow: () => void;
  onReorderShadow: (fromIdx: number, toIdx: number) => void;
  onChangeShadow: (index: number, shadow: ShadowEffect) => void;
  onChangeBlur: (kind: "layer"|"background", value: number | undefined) => void;
  onChangeBlendMode: (mode: EffectStack["blendMode"]) => void;
  onChangeOpacity: (value: number) => void;
}
```
Le sélecteur de dégradé (`FillEffect` avec `stops`) réutilise un composant
`<GradientRamp>` générique (curseurs de couleur draggables sur une bande),
nouveau composant `ui-kit`, pas de dépendance externe requise pour un
premier jet.

### 6.11 `<ComponentLibraryPanel>` (doc 3 §14bis)
```ts
interface ComponentSummary {
  id: string; name: string; thumbnailUrl?: string; instanceCount: number;
  source: "local" | "imported" | "marketplace";
}
interface ComponentLibraryPanelProps {
  components: ComponentSummary[];
  onDragToCanvas: (componentId: string) => void;    // crée une instance
  onImportFromFile: () => void;
  onEditMaster: (componentId: string) => void;       // ouvre l'onglet canvas isolé
  searchQuery: string; onSearchChange: (q: string) => void;
}
interface InstanceOverrideBadgeProps {
  isOverridden: boolean;
  onResetToMaster: () => void;
}
```
`InstanceOverrideBadge` s'intègre à `<PropertyRow>` (doc 2 §4.1) exactement
comme `isOverridden` y est déjà prévu pour Aetherion — même prop, même
rendu, réutilisation directe sans nouveau composant visuel.

### 6.12 `<WidgetAnimationCanvas>` (doc 3 §9bis)
Composant composite réutilisant **directement** les contrats déjà définis
dans `05-specification-claude-animation-vfx.md` (§4.6 `StateMachineCanvas`,
§4.4 `DopeSheet`, §4.5 `CurveEditorCanvas`) — importés depuis le package
partagé plutôt que redéfinis :
```ts
interface WidgetAnimationCanvasProps {
  program: WidgetAnimationProgram;
  viewMode: "stateMachine" | "dopeSheet" | "curveEditor";
  onViewModeChange: (m: WidgetAnimationCanvasProps["viewMode"]) => void;
  scope: BehaviorScope;                          // réutilise §6.7, même sélecteur de portée
  onScopeChange: (s: BehaviorScope) => void;
  onAddState: () => void;
  onPreviewTransition: (transitionId: string) => void; // joue dans InfiniteDesignCanvas en incrustation
  easingPresets: Record<string, CurveChannel>;    // Linéaire/EaseIn/EaseOut/EaseInOut/Bounce/Élastique
}
```
Implémentation recommandée : extraire `StateMachineCanvas`, `DopeSheet` et
`CurveEditorCanvas` dans un **package partagé** entre Aetherion Animate & FX
et NkUIDesign (ex. `@nkentseu/anim-editors`) plutôt que de les dupliquer dans
les deux applications — la divergence entre les deux outils sur ces trois
composants serait un bug, exactement comme pour le design system de base
(doc 2 §7 principe directeur, déjà établi pour Aetherion).

---

## 7. Check-list d'implémentation

1. `NkGuiDocumentStore` + `nkgui-parser` (round-trip testé) — fondation, tout
   en dépend.
2. `EditorShell` (TitleBar + ProjectTabStrip + DesignToolbar, chrome vide).
3. `InfiniteDesignCanvas` en lecture seule (rendu de pages mock), puis
   sélection, puis édition (drag/resize/promote).
4. `Hierarchy` + `Inspector` (`SceneObjectsGrid` quand rien sélectionné,
   onglets Design/Widget quand sélection).
5. `DockRail` générique + `useDockRailState` (state machine testée
   indépendamment de l'UI avant de la brancher).
6. `ComponentPalette` (contenu de la première pastille, valide le mécanisme
   de bout en bout).
7. `BehaviorCanvas` (réutilise `NodeGraphCanvas` du doc 2, catalogue de
   nœuds `.nkgui` du doc 2 §6.2) + `behavior-compiler` (IR).
8. `CanvasModeSwitch` (4 états) + `SplitCanvasView` (dépend de 3 et 7 déjà
   stables ; le mode Animation et sa paire Split arrivent après l'étape 13).
9. `InspectorBehaviorTab` + `CallbackManager` (dépendent du modèle de
   contrats §1).
10. `AiChatPanel` + `AIContextButton` + `aiGenerationService` (mock de
    génération d'abord, intégration réelle ensuite).
11. `PreviewTest` (dogfooding NKGui réel — dépendance externe, peut être
    stubée longtemps avec un rendu placeholder fidèle au style).
12. Export/Validation modal (dépend du parseur/sérialiseur stabilisé).
13. `VectorPathEditor` + `EffectsStackPanel` (dépendent de `InfiniteDesignCanvas`
    déjà stable à l'étape 3 ; extension du modèle `ShapeNode`, pas de nouveau
    canvas).
14. `ComponentLibraryPanel` + logique instance/override/détachement (dépend
    du modèle `WidgetNode` étendu §1 et de `InstanceOverrideBadge` sur
    `PropertyRow`).
15. `WidgetAnimationCanvas` : extraire d'abord `StateMachineCanvas`/
    `DopeSheet`/`CurveEditorCanvas` en package partagé avec Aetherion Animate
    & FX (ou les dupliquer temporairement si les deux apps ne sont pas
    développées en parallèle, en notant la dette explicitement), puis
    brancher le mode `Animation` du `CanvasModeSwitch` et sa paire dans
    `SplitCanvasView`.

**Principe directeur inchangé** : avant de créer un composant, vérifier s'il
peut être une extension d'un composant déjà défini dans `02-specification-
claude.md` (TreeView, PropertyRow, NodeGraphCanvas, Modal, TabStrip). La
divergence de style entre Aetherion et NkUIDesign est un bug, pas une
option.

---

**Fin du document 5.** Voir `4_NkUIDesign_Brief_Banani.md` pour les prompts de
génération visuelle écran par écran.
