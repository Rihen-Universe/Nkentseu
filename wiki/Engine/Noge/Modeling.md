# La modélisation 3D

> Couche **Engine** · Noge · Construire et éditer des **maillages** à la main : un mesh
> polygonal éditable (`NkEditableMesh`), une pile de modificateurs non-destructifs
> (`NkModifierStack`), des primitives procédurales (`proc::*`), une topologie half-edge pour
> les requêtes (`NkHalfEdgeMesh`), des opérations booléennes, du sculpt, l'historique
> undo/redo et un éditeur d'UV.

Tôt ou tard, un moteur doit **fabriquer de la géométrie** plutôt que se contenter d'en charger.
On veut un cube paramétrable pour boucher un trou de niveau, fusionner deux volumes pour creuser
une porte, biseauter une arête pour adoucir une silhouette, défaire la dernière opération d'un
clic. C'est exactement le métier d'un éditeur de modélisation type Blender — et c'est ce que ce
sous-système de Noge porte côté moteur. La question n'est pas « comment dessiner un triangle »
(ça, c'est le rôle de [NKRenderer](../../Runtime/NKRenderer/README.md)), mais « comment
**représenter, transformer et historiser** un maillage qu'un humain édite interactivement ».

Le cœur du compromis tient en une distinction qu'on retrouve partout : **la représentation
simple est facile à uploader au GPU mais lente à interroger ; la représentation topologique est
chère à construire mais répond en `O(1)` aux questions de voisinage.** Noge fournit les deux et
sait passer de l'une à l'autre. La représentation simple, `NkEditableMesh`, est un sac de
sommets, d'arêtes et de faces qu'on convertit en `renderer::NkMeshDesc` pour l'affichage. La
représentation topologique, `NkHalfEdgeMesh`, relie chaque demi-arête à sa jumelle, à la
suivante et à sa face, ce qui rend instantanées les questions du genre « quelles faces touchent
ce sommet ? ».

Ce n'est **pas** un format d'asset, ni un système d'animation, ni un moteur de physique : on
manipule de la géométrie statique en mémoire CPU. Et il faut le dire d'emblée — **une grande
partie de ce sous-système est à l'état de spécification** : les en-têtes déclarent l'API
publique et les algorithmes (Catmull-Clark, QEM, BSP booléen, LSCM/ABF), mais les corps lourds
vivent dans des `.cpp` non encore écrits. Cette page documente l'**API telle qu'elle est
déclarée**, et signale clairement ce qui n'est pas encore implémenté.

- **Namespace racine** : `nkentseu` (avec `using namespace math;` dans la plupart des headers)
- **Sous-namespace procédural** : `nkentseu::proc`
- **Headers** : `Noge/Modeling/NkEditableMesh.h`, `NkMeshModifier.h`, `NkProceduralMesh.h`,
  `NkUndoStack.h` · `Noge/Topology/NkHalfEdge.h`, `NkBooleanOp.h` ·
  `Noge/Sculpt/NkSculpting.h` · `Noge/UV/NkUVEditor.h`

---

## Le maillage éditable : `NkEditableMesh`

C'est la pièce maîtresse, le maillage « par défaut » de tout le sous-système. Il range trois
listes parallèles — des `NkEditVertex`, des `NkEditEdge` (en demi-arêtes appariées) et des
`NkEditFace` — dans des `NkVector`, et expose les outils de modélisation classiques :
`AddVertex` / `AddTri` / `AddQuad` pour construire la topologie, puis `ExtrudeFaces`,
`BevelEdges`, `LoopCut`, `Subdivide`, `MergeByDistance` pour la sculpter à coups d'opérations.

Un sommet (`NkEditVertex`) porte sa `position`, sa `normal` (recalculée par `RecalcNormals`),
deux jeux d'UV (`uv` et `uv2`/lightmap), une `color`, un drapeau de sélection et de masquage.
Une face (`NkEditFace`) est soit un **triangle**, soit un **quad** — au plus quatre sommets
(`kMaxVerts = 4`) — avec un `materialId`, un drapeau `smooth` (lissage des normales) et son
propre `normal`. Tout l'intérêt de garder les quads, plutôt que de tout trianguler tout de
suite, est de préserver une topologie propre pour la subdivision et l'édition ; on ne triangule
qu'au moment d'uploader, via `ToMeshDesc`.

```cpp
NkEditableMesh m;
uint32 a = m.AddVertex({0,0,0}, {0,0});
uint32 b = m.AddVertex({1,0,0}, {1,0});
uint32 c = m.AddVertex({1,1,0}, {1,1});
uint32 d = m.AddVertex({0,1,0}, {0,1});
m.AddQuad(a, b, c, d);            // une face, ordre CCW
m.RecalcNormals(true);           // normales lissées
renderer::NkMeshDesc desc;
m.ToMeshDesc(desc);              // quad triangulé, prêt pour le GPU
```

Deux pièges sont structurels. D'abord, `NkEditableMesh` est **non copiable** : le constructeur
de copie et l'affectation sont `= delete`. Pour dupliquer, on appelle `Clone()` (copie profonde
explicite) ; le déplacement, lui, est autorisé. Ensuite, **toute opération qui ajoute ou retire
de la topologie invalide les indices** que vous gardiez : un `uint32` de face renvoyé avant un
`Subdivide` ne désigne plus la même chose après. La sentinelle d'index invalide est
`kNkInvalidIdx` (= `0xFFFFFFFFu`).

Beaucoup d'opérations prennent un `NkSpan<const uint32>` de faces ou d'arêtes. **Un span vide y
signifie souvent « toutes » ou « la sélection courante »** — c'est le cas pour `ExtrudeFaces`,
`FlipNormals` et `RecalcFaceNormals`.

> **En résumé.** `NkEditableMesh` est le maillage CPU éditable : trois listes (verts/edges/faces),
> des faces triangle **ou** quad, et la palette de modélisation (extrude/bevel/loopcut/subdivide).
> Non copiable → `Clone()` ; tout ajout/retrait invalide les indices ; on convertit vers
> `renderer::NkMeshDesc` (triangulé) pour le GPU. **Les opérations lourdes sont déclarées mais
> implémentées en `.cpp` (statut spec).**

---

## Les modificateurs non-destructifs : `NkModifierStack`

`NkEditableMesh` modifie le maillage **en place** : une fois biseauté, c'est biseauté. Pour de
nombreux flux de travail, on veut au contraire empiler des transformations **sans toucher
l'original**, pouvoir en changer un paramètre et tout revoir se recalculer — exactement le mode
Object de Blender. C'est le rôle de `NkModifierStack`.

Chaque modificateur dérive de `NkMeshModifier` et est **fonctionnel** : sa méthode
`Apply(input, output)` lit un mesh et en **produit** un autre, sans modifier l'entrée. La pile
enchaîne ces fonctions — la sortie de l'un devient l'entrée du suivant. On dispose des
modificateurs habituels : `NkBevelModifier`, `NkMirrorModifier`, `NkSubdivModifier`,
`NkSolidifyModifier`, `NkArrayModifier`, `NkDecimateModifier`, `NkSmoothModifier`. Chacun a ses
paramètres publics (largeur de biseau, axe de miroir, niveaux de subdivision, épaisseur,
nombre de copies, ratio de décimation, itérations de lissage…) et un `GetTypeName()`.

```cpp
NkModifierStack stack;
auto& mir = stack.Add<NkMirrorModifier>();   // Add<T>() retourne T& pour le configurer
mir.axis = NkMirrorModifier::Axis::X;
mir.mergeMirror = true;
stack.Add<NkSubdivModifier>().levels = 2;

NkEditableMesh preview = stack.Apply(base);  // base reste intact
```

L'API de pile est directe : `Add<T>(...)` insère et **retourne la référence** pour la
configurer ; `Remove`, `MoveUp`/`MoveDown` réordonnent ; `Apply(base)` calcule le résultat
complet, `ApplyUpTo(base, i)` une **prévisualisation partielle**, et `ApplyAll(base)` fusionne
définitivement dans le base mesh puis vide la pile. Un drapeau `IsDirty()`/`MarkDirty()` permet
de ne recalculer que si nécessaire.

Deux mises en garde. `NkMirrorModifier::Axis` est un **bitmask** (`X=1, Y=2, Z=4`), pas un
simple énuméré ordinal — à qualifier `NkMirrorModifier::Axis::X`. Et surtout, **`NkModifierStack`
alloue ses modificateurs avec `new` brut** (et les libère avec `delete`), ce qui **n'est pas
conforme** à la règle dure NKMemory du projet (allouer/libérer uniquement via NKMemory) —
risque de heap-corruption `c0000374` si on mélange avec `NkAlloc`/`NkFree`. À surveiller.

> **En résumé.** `NkModifierStack` empile des modificateurs **fonctionnels** (entrée → sortie,
> jamais en place), façon Blender Object-Mode : bevel/mirror/subdiv/solidify/array/decimate/smooth.
> `Add<T>()` retourne la référence à configurer ; `Apply`/`ApplyUpTo`/`ApplyAll`. Axe miroir =
> **bitmask**. Allocation via `new`/`delete` = **non conforme NKMemory**, et `Apply` est en `.cpp`.

---

## Les primitives : `nkentseu::proc`

Avant de modéliser quoi que ce soit, il faut un point de départ — un cube, une sphère, un plan.
Le sous-namespace `nkentseu::proc` rassemble des **générateurs procéduraux** : des fonctions
libres qui prennent une petite struct de paramètres et **renvoient un `NkEditableMesh`** prêt à
être édité ou affiché. C'est l'équivalent du menu « Add Mesh ».

Chaque primitive a sa struct de paramètres à valeurs par défaut : `NkPlaneParams`,
`NkCubeParams`, `NkSphereParams`, `NkCylParams`, `NkConeParams`, `NkTorusParams`,
`NkGridParams`, `NkIcoParams`. On ne renseigne que ce qu'on veut changer.

```cpp
using namespace nkentseu::proc;
NkEditableMesh cube   = Cube();                              // 1×1×1, 1 segment
NkEditableMesh ball   = Sphere({ 0.5f, 32, 64 });           // r, rings, segs
NkEditableMesh donut  = Torus({ 1.f, 0.25f, 48, 12 });
NkEditableMesh ground = Grid();                             // 10×10, 10×10 segments
```

Deux générateurs ne prennent **pas** de struct mais des arguments directs : `Arrow(len, headR)`
(une flèche, typiquement pour un gizmo) et `Capsule(r, h, segs)`. Toutes ces fonctions sont
`[[nodiscard]] noexcept` et renvoient par **déplacement** (RVO).

> **En résumé.** `proc::Plane/Cube/Sphere/Cylinder/Cone/Torus/Grid/Icosphere` prennent une struct
> de params (défauts fournis) et renvoient un `NkEditableMesh`. `Arrow` et `Capsule` prennent des
> arguments directs. Retour par move.

---

## La topologie half-edge : `NkHalfEdgeMesh`

`NkEditableMesh` est parfait pour stocker et afficher, mais répondre à « quelles faces entourent
ce sommet ? » ou « quel est le valence de ce sommet ? » l'oblige à **tout parcourir**. Pour
l'édition fine — flip d'arête, collapse, subdivision Catmull-Clark, requêtes de 1-ring — on veut
une structure où ces réponses sont **immédiates**. C'est la **half-edge mesh**.

L'idée : chaque arête est scindée en deux **demi-arêtes** dirigées et opposées (`twin`), chacune
connaissant sa demi-arête `next` et `prev` autour de sa face, le `vertex` d'où elle part et la
`face` qu'elle borde. Ce maillage de pointeurs rend le voisinage navigable en `O(1)`. On y entre
depuis un `NkEditableMesh` via `FromEditable` (`O(n log n)`, tri des arêtes pour apparier les
twins) et on en ressort via `ToEditable`. Comme `NkEditableMesh`, il est **non copiable** (move
seulement) ; sa sentinelle est `kNkHEInvalid` (même valeur `0xFFFFFFFFu`).

```cpp
NkHalfEdgeMesh he = NkHalfEdgeMesh::FromEditable(base);
NkVector<uint32> faces;
he.VertexFaces(vid, faces);        // 1-ring, O(valence)
if (he.IsManifold() && he.IsClosed())
    NkHalfEdgeMesh sub = he.Subdivide(2);   // Catmull-Clark, nouveau mesh
```

Elle offre une vraie boîte à outils de géométrie discrète : des **opérateurs d'Euler**
(`FlipEdge`, `SplitEdge`, `CollapseEdge`, `SubdivideFace`, `DissolveEdge`, `DissolveVertex`…),
le **parcours** (`VertexStar`, `VertexFaces`, `VertexNeighbors`, `EdgeLoop`, `EdgeRing`,
`BoundaryEdges`, `ConnectedComponents`), l'**analyse** (`IsManifold`, `IsClosed`, `HasBoundary`,
`EulerCharacteristic`, `Genus`) et des **algorithmes** lourds (`Subdivide` Catmull-Clark,
`Decimate` par QEM, `SmoothLaplacian`, `Triangulate`).

Attention : certaines opérations renvoient un **nouveau mesh par move** (`Subdivide`, `Decimate`,
`Triangulate`) tandis que d'autres travaillent **en place** (`SmoothLaplacian`, `RecalcNormals`,
les opérateurs d'Euler). Les accesseurs indexés `V(i)`/`H(i)`/`F(i)` et `EdgeCount()` ne font
**aucune vérification de borne**. Et là encore, **tout est déclaré pour `.cpp`** — pas
d'implémentation in-header.

> **En résumé.** `NkHalfEdgeMesh` est la représentation topologique : demi-arêtes appariées →
> voisinage en `O(1)`. `FromEditable`/`ToEditable` pour passer de/vers `NkEditableMesh`.
> Opérateurs d'Euler, parcours (loop/ring/star), analyse (manifold/genus), algos (Catmull-Clark,
> QEM). Non copiable, `V/H/F` sans bounds-check, mix new-mesh/in-place. **Statut spec.**

---

## Les opérations booléennes : `NkBooleanOp`

Creuser une fenêtre dans un mur, fusionner deux volumes, garder seulement l'intersection de
deux formes : ce sont des **opérations booléennes** de solides, et `NkBooleanOp` les expose en
utilitaire **100 % statique** opérant sur des `NkHalfEdgeMesh`. `Union`, `Subtract`, `Intersect`
sont les trois primitives ; `Compute(a, b, op)` dispatche selon un `NkBoolType`.

```cpp
NkHalfEdgeMesh result = NkBooleanOp::Subtract(wall, windowHole);
// ou, génériquement :
NkHalfEdgeMesh r2 = NkBooleanOp::Compute(a, b, NkBoolType::Intersect);
```

L'énuméré `NkBoolType` a **quatre** valeurs : `Union`, `Subtract`, `Intersect`, **`Difference`**
— `Difference` y est distincte de `Subtract`. À l'intérieur, l'algorithme planifié est un
**arbre BSP + clipping de Sutherland-Hodgman**. **Aucune implémentation in-header : c'est de la
spécification**, le `.cpp` reste à écrire.

> **En résumé.** `NkBooleanOp::Union/Subtract/Intersect` (+ `Compute` générique) font de la CSG
> sur `NkHalfEdgeMesh`. Enum `NkBoolType` à **4** valeurs (`Difference` ≠ `Subtract`). Algorithme
> BSP planifié — **non implémenté**.

---

## Le sculpt : `NkSculpting`

Au-delà de l'édition par sommet, on veut parfois **pousser la matière à la brosse** comme dans
ZBrush : peindre du relief, lisser, gonfler, pincer. `NkSculptSession` orchestre ça en temps
réel — raycast depuis l'écran sur le mesh, application d'une brosse dans un rayon, marquage d'une
région « sale » pour un **upload GPU partiel**.

On décrit la brosse par un `NkSculptBrush` : son `type` (`NkSculptBrushType`), son `radius`
(monde), sa `strength`, sa `hardness` (falloff), les axes de symétrie, l'usage de la pression du
stylet, l'espacement, et une éventuelle texture de brosse. L'énuméré `NkSculptBrushType` compte
**12** valeurs : `Draw, Grab, Smooth, Flatten, Clay, Inflate, Pinch, Crease, Snake, Mask,
SmoothMask, FillMask`.

```cpp
NkSculptSession sess;
sess.Init(&mesh, &undoStack);
NkSculptBrush b; b.type = NkSculptBrushType::Clay; b.radius = 0.3f;
sess.SetBrush(b);
sess.SetCamera(&viewCam);
sess.PointerDown({mx,my}, vpW, vpH);
sess.PointerMove({mx,my}, vpW, vpH);   // dépose de la matière
sess.PointerUp();
if (sess.HasChanges()) { /* upload sess.GetDirtyRegion() */ sess.ClearChanges(); }
```

Trois remarques importantes. D'abord, **`NkSculptBrush::autosmooth` est déclaré `bool` mais
initialisé `= 0.2f`** : il vaut donc `true`, alors que le nom et le commentaire suggèrent un
`float32` — **bug à signaler**. Ensuite, les handlers privés ne couvrent que
`Draw/Grab/Smooth/Flatten/Clay/Inflate/Pinch` : les brosses `Crease/Snake/Mask/SmoothMask/FillMask`
de l'enum n'ont **aucun handler déclaré**, donc **non encore prises en charge**. Enfin, le
header est de la **spec** : seuls les setters/getters triviaux ont un corps, le reste (raycast,
BVH, brosses) est un gros chantier `.cpp`.

> **En résumé.** `NkSculptSession` = sculpt temps réel (raycast → brosse → région sale → upload
> partiel via `GetDirtyRegion`). `NkSculptBrushType` a **12** valeurs mais **7** brosses
> seulement ont un handler ; `autosmooth` a un **bug de type**. **Statut spec.**

---

## Undo/redo : `NkUndoStack`

Un éditeur sans annulation est inutilisable. `NkUndoStack` implémente l'historique selon le
**Command pattern** : chaque action devient une `NkEditCommand` qui sait s'**exécuter** et se
**défaire**. La stack exécute, mémorise, et peut rembobiner.

Une commande dérive de `NkEditCommand` et implémente `Execute()` / `Undo()` (et, par défaut,
`Redo()` rappelle `Execute()`). `MemoryUsage()` permet à la stack de **respecter un budget
mémoire** (256 Mo par défaut) en taillant les vieilles entrées. Trois variantes structurent
l'usage : `NkCommandGroup` réunit N commandes en une **seule entrée undo** (et les défait en
ordre inverse) ; `NkLambdaCommand` capture deux closures `do`/`undo` pour les opérations simples ;
et la stack elle-même gère groupes (`BeginGroup`/`EndGroup`), état propre (`SetCleanState`,
`IsDirty`) et noms d'action (`GetUndoName`/`GetRedoName`).

```cpp
NkUndoStack undo;                         // 100 pas, 256 Mo
undo.BeginGroup("Extrude + Bevel");
undo.Execute(new ExtrudeCommand(...));    // Execute() est appelé PAR la stack
undo.Execute(new BevelCommand(...));
undo.EndGroup();                          // une seule entrée undo
if (undo.CanUndo()) undo.Undo();
```

Deux pièges. **N'appelez jamais `Execute()` à la main** : on passe la commande à
`undo.Execute(cmd)`, qui l'exécute et la mémorise — et qui **invalide le redo** en avant. Et,
comme `NkModifierStack`, **`NkUndoStack` et `NkCommandGroup` font `delete` sur les commandes**,
et les exemples allouent en `new` brut : **non conforme à la règle NKMemory** du projet.

> **En résumé.** `NkUndoStack` = historique Command pattern avec budget mémoire, groupes et état
> propre. Commandes : `NkEditCommand` (sur-mesure), `NkCommandGroup` (N→1), `NkLambdaCommand`
> (closures). On passe la commande à `Execute(cmd)` — jamais l'inverse ; `Execute` invalide le
> redo. Allocation `new`/`delete` = **non conforme NKMemory**.

---

## L'éditeur d'UV : `NkUVEditor`

Quand le maillage est prêt, il faut **déplier sa surface dans le plan UV** pour y plaquer une
texture. `NkUVEditor` est l'outil de dépliage : il découpe la surface en **îlots** (`NkUVIsland`),
les sélectionne, les transforme, et les empaquette dans le carré [0,1].

On lie un `NkEditableMesh` (`SetMesh`), on construit les îlots (`BuildIslands` → remplit le membre
public `islands`), puis on agit : sélection (`SelectIsland`, `SelectIslandsInRect`), transforms UV
(`TranslateSelected`, `RotateSelected`, `ScaleSelected`, `FlipH/VSelected`), dépliage
(`SmartUnwrap`, `UnwrapFromSeams`, `PackIslands`, `RelaxIslands`) et rendu de l'éditeur lui-même
via `Draw(r2d, viewport, texHandle)`. Navigation par `Pan`/`Zoom` (zoom clampé à [0.1, 50]) —
ce sont d'ailleurs les **seules** méthodes implémentées in-header.

```cpp
NkUVEditor uv;
uv.SetMesh(&mesh);
uv.SmartUnwrap(66.f);     // déplie selon un angle limite
uv.PackIslands(0.02f);    // range les îlots avec marge
uv.Draw(r2d, viewportRect, texHandle);
```

`NkUVEditor` expose plusieurs membres **en dur** (`viewOffset`, `zoom`, `islands`, `undoStack`).
Le dépliage `RelaxIslands` est commenté LSCM/ABF ; tout sauf `Pan`/`Zoom` est en `.cpp` — **statut
spec**.

> **En résumé.** `NkUVEditor` déplie le maillage en **îlots UV** : `BuildIslands`, sélection,
> transforms (translate/rotate/scale/flip), `SmartUnwrap`/`PackIslands`/`RelaxIslands`, rendu via
> `Draw`. État public en dur ; seules `Pan`/`Zoom` sont implémentées — **statut spec**.

---

## Aperçu de l'API

Tous les types lourds (`NkEditableMesh`, `NkHalfEdgeMesh`, `NkModifierStack`, `NkUndoStack`) sont
**non copiables** (move seulement). Chaque élément est détaillé dans la « Référence complète ».

### Maillage éditable (`NkEditableMesh.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Structs | `NkEditVertex`, `NkEditEdge`, `NkEditFace` | Sommet (pos/normal/uv/uv2/color), demi-arête appariée, face triangle/quad (`kMaxVerts=4`) |
| Sentinelle | `kNkInvalidIdx` (`0xFFFFFFFFu`) | Index invalide |
| Cycle de vie | `NkEditableMesh()`, move, `Clone()`, `Clear()`, copie **supprimée** | Création / déplacement / copie profonde explicite / vidage |
| Topologie | `AddVertex`, `AddTri`, `AddQuad` | Construire sommets et faces (CCW), renvoient un index |
| Modélisation | `ExtrudeFaces`, `BevelEdges`, `LoopCut`, `MergeByDistance`, `FlipNormals`, `Triangulate`, `Subdivide` | Opérations d'édition (span vide = toutes/sélection) |
| Normales | `RecalcNormals`, `RecalcFaceNormals` | Recalcul lisse/plat |
| Sélection | `SelectAll`/`DeselectAll`/`InvertSelection`, `SelectVertex/Face/Edge`, `Grow/ShrinkSelection`, `GetSelected*` | Gérer la sélection |
| UV | `SmartUVProject`, `CubeProject`, `CylindricalProject` | Projections UV |
| AABB | `GetBounds`, `RecomputeBounds` | Boîte englobante (dirty flag) |
| Données | `Vertices`/`Edges`/`Faces` (+ const), `VertexCount`/`FaceCount`/`EdgeCount` | Accès aux listes et compteurs |
| Conversion | `ToMeshDesc`, `FromMeshDesc` (static) | Vers/depuis `renderer::NkMeshDesc` (GPU) |

### Modificateurs (`NkMeshModifier.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Base | `NkMeshModifier` (`Apply`, `GetTypeName`, `enabled`, `showInViewport/Render`) | Interface modificateur **fonctionnel** |
| Built-in | `NkBevelModifier`, `NkMirrorModifier`, `NkSubdivModifier`, `NkSolidifyModifier`, `NkArrayModifier`, `NkDecimateModifier`, `NkSmoothModifier` | Modificateurs concrets |
| Enum | `NkMirrorModifier::Axis { X=1, Y=2, Z=4 }` | Axe miroir (**bitmask**) |
| Pile | `Add<T>`, `Remove`, `MoveUp`/`MoveDown`, `Clear`, `Count`, `Get` | Gérer la pile (`Add` retourne `T&`) |
| Application | `Apply`, `ApplyUpTo`, `ApplyAll` | Résultat complet / preview partielle / fusion définitive |
| État | `MarkDirty`, `IsDirty` | Recalcul conditionnel |

### Primitives procédurales (`NkProceduralMesh.h`, `nkentseu::proc`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Params | `NkPlaneParams`, `NkCubeParams`, `NkSphereParams`, `NkCylParams`, `NkConeParams`, `NkTorusParams`, `NkGridParams`, `NkIcoParams` | Paramètres à défauts |
| Générateurs | `Plane`, `Cube`, `Sphere`, `Cylinder`, `Cone`, `Torus`, `Grid`, `Icosphere` | Primitives (struct params) → `NkEditableMesh` |
| Générateurs | `Arrow(len, headR)`, `Capsule(r, h, segs)` | Args directs (pas de struct) |

### Topologie half-edge (`NkHalfEdge.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Structs | `NkHEVertex`, `NkHEHalfEdge`, `NkHEFace`, sentinelle `kNkHEInvalid` | Donnée half-edge |
| Conversion | `FromEditable` (static), `ToEditable` | De/vers `NkEditableMesh` |
| Euler | `FlipEdge`, `SplitEdge`, `CollapseEdge`, `SubdivideFace`, `ExtrudeFaces`, `BevelEdge`, `CollapseFace`, `DissolveEdge`, `DissolveVertex` | Édition topologique |
| Parcours | `VertexStar/Faces/Neighbors`, `VertexValence`, `EdgeLoop/Ring`, `BoundaryEdges`, `ConnectedComponents` | Requêtes de voisinage |
| Analyse | `IsManifold`, `IsClosed`, `HasBoundary`, `EulerCharacteristic`, `Genus` | Propriétés topologiques |
| Algorithmes | `Subdivide`, `Decimate`, `SmoothLaplacian`, `RecalcNormals`, `Triangulate` | Catmull-Clark, QEM, Laplacien |
| Sélection | `SelectAll/DeselectAll`, `SelectEdgeLoop/Ring`, `SelectLinked`, `Grow/ShrinkVertexSelection` | Sélection topologique |
| Accès | `Vertices`/`HalfEdges`/`Faces`, `V`/`H`/`F`, `*Count`, `Clear`, `Validate` | Listes, accès indexé (sans bounds-check), debug |

### Booléennes, sculpt, undo, UV

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Booléen | `NkBoolType { Union, Subtract, Intersect, Difference }`, `NkBooleanOp::Union/Subtract/Intersect/Compute` | CSG sur `NkHalfEdgeMesh` (statique) |
| Sculpt | `NkSculptBrushType` (12 valeurs), `NkSculptBrush`, `NkSculptSession` (`Init`, `SetBrush/Camera/Dynamic`, `Pointer*`, `HasChanges`, `GetDirtyRegion`) | Sculpt temps réel |
| Undo | `NkEditCommand`, `NkCommandGroup`, `NkLambdaCommand`, `NkUndoStack` (`Execute`, `Undo`/`Redo`, `Begin/EndGroup`, `Can*`, `IsDirty`) | Historique Command pattern |
| UV | `NkUVIsland`, `NkUVEditor` (`SetMesh`, `BuildIslands`, sélection, transforms, unwrap, `Draw`, `Pan`/`Zoom`) | Dépliage UV |

---

## Référence complète

Chaque élément est repris ici en détail, avec ses usages dans les différents domaines —
gameplay/IA, rendu, animation, physique, audio, UI/éditeur, IO, scène. **Rappel transversal :
seules les méthodes triviales (setters/getters inline, constructeurs, conteneurs) ont un corps
dans les headers ; tous les algorithmes lourds sont déclarés pour `.cpp` et constituent du
travail planifié (statut spec).**

### `NkEditVertex`, `NkEditEdge`, `NkEditFace` — les briques

Le **sommet** (`NkEditVertex`) ne se réduit pas à une position : il porte sa `normal` (par défaut
`{0,1,0}`, recalculée par `RecalcNormals`), deux jeux de coordonnées de texture (`uv` pour le
diffus, `uv2` pour le lightmap/lighting baked), une `color` vertex, un index `edgeStart` vers la
première demi-arête adjacente, et les drapeaux `selected`/`hidden`. Cette richesse sert tous les
domaines :

- **Rendu** — la `color` vertex module les matériaux (gradients, ombrage vertex), `uv2` adresse
  l'atlas de lightmap, `normal` alimente l'éclairage.
- **Animation** — un mesh éditable peut servir de base à du skinning ou à des blendshapes (les
  positions sont mutables côté CPU).
- **Éditeur** — `selected`/`hidden` pilotent l'affichage et les opérations ciblées.

L'**arête** (`NkEditEdge`) est en réalité une **demi-arête** : `v0→v1` dirigée, avec `twin` vers
l'opposée `v1→v0`, `next` vers la demi-arête suivante de la même face, et `face`. Ses drapeaux
qualifient la géométrie : `seam` (couture UV, là où l'on coupe pour déplier), `sharp` (arête vive,
qui casse le lissage des normales). `IsValid()` teste que `v0` et `v1` ne sont pas la sentinelle.

La **face** (`NkEditFace`) est un triangle (`vertCount==3`) ou un quad (`vertCount==4`, plafonné
par `kMaxVerts=4`) : tableaux `verts[]` et `edges[]`, un `materialId` (pour le tri par matériau
au rendu), une `normal`, et `smooth` qui décide entre ombrage lissé et facetté. `IsTriangle()`,
`IsQuad()`, `IsValid()` (≥ 3 sommets) renseignent. **Garder les quads** plutôt que tout
trianguler est ce qui rend la subdivision et l'édition propres ; la triangulation n'arrive qu'à
l'upload.

### `NkEditableMesh` — création, modélisation, conversion

- **Construire** — `AddVertex(pos, uv, normal, color)` renvoie l'index du sommet ; `AddTri` et
  `AddQuad` créent une face en ordre **CCW** et renvoient son index. C'est l'API de bas niveau,
  celle que les générateurs `proc::*` et les importeurs utilisent.
- **Modéliser** — `ExtrudeFaces(faceIds, dir)` extrude (span vide = toutes les faces sélectionnées),
  `BevelEdges(edgeIds, width, segments)` biseaute, `LoopCut(edgeId, factor)` insère une boucle d'arêtes,
  `MergeByDistance(threshold)` soude les sommets proches (renvoie le nombre fusionné),
  `FlipNormals(faceIds)` retourne les normales, `Triangulate(method)` (0=Fan rapide, 1=Beauty optimal)
  et `Subdivide(levels)` (Catmull-Clark) raffinent. Cas d'usage : **éditeur** (modélisation
  interactive de niveaux/props), **rendu** (générer du LOD via `Triangulate`/décimation),
  **scène** (assembler de la géométrie au runtime).
- **Normales** — `RecalcNormals(smooth)` recalcule tout (lissé = moyenne des faces adjacentes) ;
  `RecalcFaceNormals(faceIds)` ne touche que des faces. Indispensable après toute déformation, sous
  peine d'éclairage faux.
- **Sélection** — `SelectAll`/`DeselectAll`/`InvertSelection`, `SelectVertex/Face/Edge(id, v)`,
  `GrowSelection`/`ShrinkSelection` (étendre/réduire d'un anneau), et les récupérateurs
  `GetSelectedVertices/Faces/Edges(out)`. C'est la mécanique d'**UI/éditeur** par excellence.
- **UV** — `SmartUVProject(angleLimit, islandMargin)` déplie automatiquement, `CubeProject(scale)`
  et `CylindricalProject(scale)` (axe Y) projettent simplement. Sert au **rendu** (texturer un mesh
  généré au runtime).
- **AABB** — `GetBounds()` (peut nécessiter `RecomputeBounds()` car le cache est `dirty`) ; utile
  pour le **culling**, le **picking** (sélection à la souris), la **physique** (broad-phase).
- **Données & conversion** — `Vertices()`/`Edges()`/`Faces()` (paires non-const/const) et les
  compteurs donnent l'accès direct ; `ToMeshDesc(out)` produit le `renderer::NkMeshDesc` (quads
  triangulés, normales générées si absentes) pour l'**upload GPU** ; `FromMeshDesc(desc)`
  reconstruit un mesh éditable depuis un desc GPU (utile en **IO** : ré-éditer un asset importé) ;
  `Clone()` contourne la copie supprimée.

**Pièges** : non copiable (utiliser `Clone()`) ; tout add/remove invalide les indices stockés ;
`GetBounds()` peut nécessiter un `RecomputeBounds()` préalable (flag `mutable`).

### Les modificateurs built-in — chaque effet

Tous dérivent de `NkMeshModifier` (méthodes pures `Apply`/`GetTypeName`, drapeaux `enabled`,
`showInViewport`, `showInRender`), et leur `Apply` est **en `.cpp`** — donc potentiellement
non implémenté selon l'état du module :

- **`NkBevelModifier`** (`"Bevel"`) — `width`, `segments`, `profile` ([0=concave, 1=convexe]),
  `onlyVerts`. Adoucit les arêtes (rendu : capter la lumière sur un chanfrein plutôt qu'une arête
  parfaite).
- **`NkMirrorModifier`** (`"Mirror"`) — `axis` (bitmask `X=1/Y=2/Z=4`), `mergeMirror` +
  `mergeThreshold`, `flipNormals`, `mirrorUV`. Symétrie de modélisation (modéliser une moitié de
  personnage, l'autre suit).
- **`NkSubdivModifier`** (`"Subdivide"`) — `levels`, `catmullClark`, `useCrease`. Lisse et densifie
  (rendu haute résolution depuis une cage basse résolution).
- **`NkSolidifyModifier`** (`"Solidify"`) — `thickness`, `offset` ([-1=intérieur, +1=extérieur]),
  `fillRim`, `flipNormals`. Donne de l'épaisseur à une surface (un plan devient une plaque — utile
  pour le rendu double-face ou la physique).
- **`NkArrayModifier`** (`"Array"`) — `count`, `offset`, `mergeEnds`. Réplique linéairement (une
  rambarde, une chaîne, un escalier).
- **`NkDecimateModifier`** (`"Decimate"`) — `ratio` ([0..1] faces conservées), `planar`,
  `angleLimit` (degrés). Réduit le compte de faces (génération de **LOD**).
- **`NkSmoothModifier`** (`"Smooth"`) — `iterations`, `factor` ([0..1]), `onlyX/Y/Z`. Lisse les
  positions (atténuer le bruit d'un mesh scanné/généré).

### `NkModifierStack` — la pile

- **Gestion** — `Add<T>(args...)` (inline) construit le modificateur via `new T(...)`, le pousse,
  marque la pile dirty et **retourne `T&`** pour configurer les paramètres immédiatement.
  `Remove(i)`, `MoveUp`/`MoveDown(i)` réordonnent ; `Clear()` détruit tout. **PIÈGE NKMemory** :
  `new`/`delete` brut — non conforme à la règle du projet.
- **Application** — `Apply(base)` calcule le mesh final (clone si aucun modificateur actif) ;
  `ApplyUpTo(base, i)` une **prévisualisation** jusqu'au i-ème (l'UI montre l'effet d'un cran de la
  pile) ; `ApplyAll(base)` **fusionne** dans le base mesh puis vide la pile (« appliquer » au sens
  destructif).
- **Accès** — `Count()`, `Get(i)` (+ const, `nullptr` hors borne), `MarkDirty()`/`IsDirty()` pour
  ne recalculer que si la pile a changé (cache de preview côté éditeur).

### Les primitives `proc::*`

Chaque fonction renvoie un `NkEditableMesh` immédiatement éditable. Domaines : **prototypage**
(boucher un niveau), **rendu** (gizmos, debug-draw), **collision** (formes simples), **éditeur**
(menu Add Mesh). Les structs de params (`NkSphereParams{ r, rings, segs }`, etc.) ont toutes des
défauts ; `Arrow(len, headR)` et `Capsule(r, h, segs)` se passent de struct. Retour par move
(RVO), donc gratuit.

### `NkHalfEdgeMesh` — la topologie en détail

- **Conversion** — `FromEditable(mesh)` (`O(n log n)`, tri pour apparier les twins) entre dans la
  représentation half-edge ; `ToEditable()` en ressort. On bascule dans la half-edge pour une
  passe d'édition topologique lourde, puis on revient.
- **Opérateurs d'Euler** — `FlipEdge` (sur arête interne entre deux triangles), `SplitEdge(he, t)`
  (ajoute un sommet à `t`, renvoie son index), `CollapseEdge(he, t)` (fusionne deux sommets, renvoie
  `true` si sûr — valence ≥ 3 partout), `SubdivideFace` (sommet au centre), `ExtrudeFaces`,
  `BevelEdge`, `CollapseFace`, `DissolveEdge` (supprime l'arête, fond deux faces), `DissolveVertex`.
  Ce sont les **briques atomiques** de tout remaillage (édition, simplification, génération
  procédurale avancée).
- **Parcours** — `VertexStar` (1-ring de demi-arêtes), `VertexFaces`, `VertexNeighbors`,
  `VertexValence`, `EdgeLoop`, `EdgeRing`, `BoundaryEdges`, `ConnectedComponents`. Les requêtes en
  `O(1)`/`O(valence)` sont l'argument même de la half-edge : **rendu** (calcul de normales lissées
  pondérées par voisinage), **animation** (poids de skinning par voisinage), **édition** (sélection
  par loop/ring).
- **Analyse** — `IsManifold`, `IsClosed`, `HasBoundary`, `EulerCharacteristic` (V−E+F), `Genus`
  (nombre de trous). Indispensable avant un **boolean** ou une **impression 3D** (un solide doit être
  manifold et fermé).
- **Algorithmes** — `Subdivide(levels)` (Catmull-Clark, **nouveau mesh**), `Decimate(ratio)` (QEM,
  **nouveau mesh** — LOD), `SmoothLaplacian(iter, lambda)` (**in-place**), `RecalcNormals`,
  `Triangulate` (**nouveau mesh**). Bien distinguer ce qui renvoie par move de ce qui modifie en
  place.
- **Sélection** — `SelectAll`/`DeselectAll`, `SelectEdgeLoop/Ring(he)`, `SelectLinked(face)`
  (composante connexe), `Grow/ShrinkVertexSelection`. La sélection par loop/ring est *le* geste
  d'édition topologique.
- **Accès** — `Vertices()`/`HalfEdges()`/`Faces()` (non-const), `V(i)`/`H(i)`/`F(i)` (const indexé,
  **sans bounds-check**), les compteurs (`EdgeCount() == HalfEdgeCount()/2`), `Clear()`, et
  `Validate()` qui renvoie une liste d'erreurs de cohérence (vide = OK), précieux en **debug**.

### `NkBooleanOp` — CSG

Quatre points d'entrée statiques (`Union`, `Subtract`, `Intersect`, `Compute(a, b, op)`), tous
sur des `NkHalfEdgeMesh` non copiables (retour par move). L'enum `NkBoolType` a **4** valeurs et
`Difference` y est distincte de `Subtract`. Domaines : **éditeur/level design** (creuser, fusionner,
intersecter des volumes), **procédural** (génération de bâtiments, de tunnels). L'algorithme prévu
(BSP + Sutherland-Hodgman) **n'est pas implémenté** — spec uniquement.

### `NkSculptBrush` / `NkSculptSession` — le sculpt

- **La brosse** — `type` (parmi 12 : `Draw, Grab, Smooth, Flatten, Clay, Inflate, Pinch, Crease,
  Snake, Mask, SmoothMask, FillMask`), `radius`, `strength`, `hardness` (falloff), symétries
  `symmetryX/Y/Z`, modes pression (`pressureRadius`, `pressureStrength`), `spacing`, et texture de
  brosse (`texHandle`, `texScale`, `texStrength`). **Bug à signaler** : `autosmooth` est `bool` mais
  initialisé `= 0.2f` (vaut `true`, alors qu'un `float32` était visiblement attendu).
- **La session** — `Init(mesh, undoStack)` lie le mesh et (optionnellement) le stack undo ;
  `SetBrush`, `SetCamera`, `SetDynamic` (remesh dynamique) configurent ; `PointerDown/Move/Up`
  (avec `screenPos`, `vpW`, `vpH`, `pressure`) tracent ; `HasChanges()`, `ClearChanges()` (après
  upload), `GetDirtyRegion()` (AABB des sommets modifiés) pilotent l'**upload GPU partiel**.
- **Implémentation** — seuls les setters/getters triviaux ont un corps ; le raycast, le BVH
  (`RaycastBVH`), et les handlers de brosse sont en `.cpp`. **Seuls 7 handlers privés existent**
  (`Draw/Grab/Smooth/Flatten/Clay/Inflate/Pinch`) : `Crease/Snake/Mask/SmoothMask/FillMask` de
  l'enum n'ont **aucun handler** → non pris en charge.

### Undo/redo — `NkEditCommand` et `NkUndoStack`

- **La commande** — `NkEditCommand` : `Execute()` (appelé **par la stack**, jamais à la main),
  `Undo()`, `Redo()` (défaut = `Execute()`, à surcharger pour l'efficacité), `MemoryUsage()` (défaut
  `sizeof(*this)`, à surcharger pour les grosses données afin de respecter le budget).
- **Le groupe** — `NkCommandGroup(name)` : `Add(cmd)` (ownership transféré), `Execute()` (ordre),
  `Undo()` (**ordre inverse**), `Redo()` (ordre), `MemoryUsage()` (somme), `CommandCount()`. Une
  seule entrée undo pour N actions — l'idiome « une opération composite ».
- **La closure** — `NkLambdaCommand(name, execFn, undoFn)` (`Fn = NkFunction<void()>`) : pour les
  opérations simples dont l'état tient dans la closure (`Execute`/`Undo` inline).
- **La stack** — ctor `NkUndoStack(maxSteps=100, memBudget=256 Mo)`. `Execute(cmd)` exécute, pousse
  (ownership) et **invalide le redo** ; `Undo()`/`Redo()` renvoient `true` si possible ;
  `BeginGroup(name)`/`EndGroup()` regroupent ; `IsGroupOpen()`. État : `CanUndo`/`CanRedo`,
  `GetUndoName`/`GetRedoName` (`nullptr` si rien), `HistorySize`, `CurrentIndex`, `Clear`,
  `SetCleanState` (marque l'état sauvegardé), `IsDirty` (modifié depuis le dernier `SetCleanState`),
  `MemoryUsed`. C'est le cœur d'un **éditeur** (annulation), mais aussi utile pour un système de
  **replay** ou de transactions.
- **PIÈGE NKMemory** — la stack et le groupe font `delete` sur les commandes, et les exemples
  allouent en `new` brut : **non conforme** à la règle du projet.

### `NkUVEditor` — le dépliage

- **Îlot** — `NkUVIsland` : `faceIndices`, `bounds` (`NkAABB2f`), `rotation`, `pivot`, `selected`.
- **Setup** — `SetMesh(mesh)`, `BuildIslands()` (remplit le membre public `islands`).
- **Navigation** — `Pan(delta)` (déplace `viewOffset`), `Zoom(delta)` (clamp [0.1, 50]) — les
  **seules** méthodes avec corps in-header.
- **Sélection** — `SelectIsland(idx, add)`, `SelectAll`/`DeselectAll`, `SelectIslandsInRect(uvRect)`.
- **Transforms UV** — `TranslateSelected`, `RotateSelected(deg)`, `ScaleSelected`, `FlipHSelected`,
  `FlipVSelected`.
- **Unwrap** — `SmartUnwrap(angleLimit)`, `UnwrapFromSeams()`, `PackIslands(margin)`,
  `RelaxIslands(iterations)` (LSCM/ABF, planifié).
- **Rendu** — `Draw(r2d, viewport, texHandle)` rend l'éditeur via le renderer 2D (`texHandle=0` =
  pas de fond). État public en dur (`viewOffset`, `zoom`, `islands`, `undoStack`). Hors `Pan`/`Zoom`,
  tout est en `.cpp` — **statut spec**.

### Idiomes & pièges transversaux

- **Non-copiables** : `NkEditableMesh`, `NkHalfEdgeMesh`, `NkModifierStack`, `NkUndoStack`
  (copie `= delete`, move OK). Dupliquer un mesh éditable via `Clone()`, un half-edge via
  `ToEditable()`/`FromEditable`. `proc::*`, `NkBooleanOp::*` et `Subdivide`/`Decimate`/`Triangulate`
  renvoient par **move**.
- **Enums à qualifier** : `NkMirrorModifier::Axis::X/Y/Z` (**bitmask** 1/2/4),
  `NkBoolType::Union/Subtract/Intersect/Difference`, `NkSculptBrushType::Draw…FillMask`.
- **Sentinelles** : `kNkInvalidIdx` et `kNkHEInvalid` valent toutes deux `0xFFFFFFFFu`.
- **`NkSpan<const uint32>`** est l'idiome de passage de listes d'IDs ; **un span vide signifie
  souvent « toutes » / « la sélection »** (`ExtrudeFaces`, `FlipNormals`, `RecalcFaceNormals`).
- **PIÈGE NKMemory (règle dure)** : `NkModifierStack`, `NkUndoStack`, `NkCommandGroup` font
  `new`/`delete` brut — non conforme à « allouer/libérer uniquement via NKMemory » (risque
  heap-corruption `c0000374`).
- **Bug visible** : `NkSculptBrush::autosmooth` déclaré `bool` mais initialisé `= 0.2f`.
- **Incohérences de chemins** : `NkBooleanOp.h` inclut `"Nkentseu/Modeling/NkHalfEdge.h"` alors que
  le fichier réel est sous `Topology/` ; plusieurs bandeaux de commentaire annoncent un chemin
  `Modeling/...` différent de l'emplacement physique (`Topology/`, `UV/`, `Sculpt/`) — à vérifier
  côté compilation `.cpp`.
- **Statut spec** : aucun header ne contient l'implémentation des algorithmes lourds (Catmull-Clark,
  QEM, BSP booléen, LSCM/ABF, brosses de sculpt). Tout est déclaré pour `.cpp` ; les commentaires
  DESIGN/ALGORITHME confirment l'état planifié. Certaines brosses
  (`Crease/Snake/Mask/SmoothMask/FillMask`) n'ont aucun handler privé → non encore prises en charge.

---

### Exemple

```cpp
#include "Noge/Modeling/NkProceduralMesh.h"
#include "Noge/Modeling/NkMeshModifier.h"
#include "Noge/Modeling/NkUndoStack.h"
#include "Noge/Topology/NkHalfEdge.h"
#include "Noge/Topology/NkBooleanOp.h"
using namespace nkentseu;

// 1) Une primitive, éditée puis biseautée via la pile non-destructive.
NkEditableMesh cube = proc::Cube({ 2.f, 1 });
cube.RecalcNormals(true);

NkModifierStack stack;
auto& bev = stack.Add<NkBevelModifier>();      // Add<T>() -> T& à configurer
bev.width = 0.05f; bev.segments = 2;
stack.Add<NkSubdivModifier>().levels = 1;
NkEditableMesh display = stack.Apply(cube);    // cube reste intact

// 2) CSG : creuser un trou dans un mur (statut spec — API).
NkHalfEdgeMesh wall = NkHalfEdgeMesh::FromEditable(display);
NkHalfEdgeMesh hole = NkHalfEdgeMesh::FromEditable(proc::Cylinder());
NkHalfEdgeMesh pierced = NkBooleanOp::Subtract(wall, hole);

// 3) Undo : on regroupe deux actions en une seule entrée annulable.
NkUndoStack undo;
undo.BeginGroup("Build wall");
// undo.Execute(new MyExtrudeCommand(...));   // Execute() est appelé par la stack
undo.EndGroup();
if (undo.CanUndo()) undo.Undo();

// 4) Upload GPU : conversion vers le desc renderer (quads triangulés).
renderer::NkMeshDesc desc;
pierced.ToEditable().ToMeshDesc(desc);
```

---

[← Index Noge](README.md) · [Récap Noge](../Noge.md) · [Couche Engine](../README.md)
