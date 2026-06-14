# La physique de personnage

> Couche **Engine** · Noge · La famille **Physics** : la simulation des matières souples
> attachées à un personnage — tissu (`NkClothSim`), cheveux (`NkHairSim`), os secondaires
> dynamiques (`NkJiggleBone`), corps mous (`NkSoftBody`), ragdoll (`NkRagdoll`), capture de
> mouvement (`NkMotionCapture`) et morph targets (`NkBlendShapeController`) — plus les huit
> systèmes ECS qui les pilotent en fin de frame.

Une fois qu'un personnage est animé par son squelette, il reste tout ce que l'animation **ne**
décrit **pas** : la cape qui flotte, les cheveux qui retombent, le ventre qui tressaute au pas de
course, le héros qui s'effondre quand il est touché, la bouche qui suit le son d'une réplique. Ce
ne sont pas des poses scriptées — ce sont des matières qui **réagissent** au mouvement, à la
gravité, au vent, aux chocs. La famille Physics de Noge regroupe précisément ces *systèmes
secondaires* : des composants ECS qui s'ajoutent à une entité déjà animée et qui, après que
l'animation a posé le squelette, le complètent par de la simulation. La question n'est jamais
« est-ce du tissu ou des cheveux » dans l'absolu, mais « **quelle matière** je veux faire bouger,
et **à quel coût** » — de l'os secondaire ultra-léger au cloth GPU complet.

Tout ici suit le même moule **zéro-STL** du moteur : des **structs de données POD** (les
composants) séparés des **classes système** (la logique). Les composants ne portent que de l'état
— des paramètres physiques, des tableaux fixes, des handles GPU ; les systèmes lisent et écrivent
ces composants en `PostUpdate`, dans un **ordre de priorité documenté**. Ce n'est **pas** un moteur
de physique rigide généraliste (pas de solveur de contraintes inter-objets, pas de monde physique
global) : c'est la **physique de la matière d'un personnage**, branchée sur l'ECS et, pour les plus
lourdes, sur le compute GPU via NKRHI.

> **Statut.** Les deux headers de cette famille (`NkPhysicsMesh.h`, `NkPhysicsSystems.h`) sont
> aujourd'hui une **spécification** : ils déclarent les structures de données complètes et la
> surface publique des systèmes, mais l'essentiel de la *logique* — les `Execute` des systèmes,
> `LoadBVH`, `Apply`, `Retarget`, et toutes les méthodes privées de simulation — est **déclaré sans
> corps** dans le header (implémentation hors header, vraisemblablement non livrée). Seules les
> **méthodes inline** énumérées dans cette page (les `Add*`/`Remove*`/`Set*`, les helpers d'état,
> `Play`/`Stop`/`Pause`, les `Describe`, les constructeurs et `SetCommandBuffer`) ont un corps réel.
> Cette page distingue partout ce qui est **défini** de ce qui est **spec**.

- **Namespace** : `nkentseu` (avec `using namespace math;` et `using namespace ecs;` actifs — d'où
  `NkVec3f`, `NkQuatf`, `NkMat4f`, `NkClamp` côté math et `NkEntityId`, `NkWorld`, `NkSystem` côté ecs)
- **Headers réels** : `Noge/Physics/NkPhysicsMesh.h` (composants), `Noge/Systems/NkPhysicsSystems.h`
  (systèmes)

---

## Le tissu : `NkClothSim`

C'est le composant phare, et celui qui résume le mieux la philosophie de la famille. On l'ajoute à
une entité, on règle ses paramètres physiques, on épingle quelques sommets, et le système
correspondant (`NkClothSystem`) se charge du reste — uploader le mesh, lancer la simulation sur le
GPU, redescendre les positions. L'usage type tient en trois lignes :

```cpp
auto& cloth = go.AddComponent<NkClothSim>().Get();
cloth.stiffness = 0.9f;            // 0 = gelée, 1 = rigide
cloth.AddPin(0, {});               // sommet 0 fixé à l'origine du monde
cloth.AddBonePin(3, boneEnt, 5);   // sommet 3 collé à l'os 5 d'un autre squelette
```

Le composant porte deux familles de champs. D'abord les **paramètres physiques** : `stiffness`,
`bendStiffness` (résistance au pliage), `shearStiffness` (au cisaillement), `damping`, `mass`
(en kg par sommet), la `gravity` (un `NkVec3f`), le `wind` avec sa turbulence et sa fréquence, la
`friction` et la distance d'auto-collision. Puis les **réglages de simulation** : le nombre de
`substeps` (8 par défaut), le pas fixe `fixedDt`, les bascules `selfCollision` (coûteuse, off par
défaut) et `simulating`.

Les **pins** — les sommets épinglés — sont l'unique partie réellement *codée* du composant. Ils
sont stockés dans un **tableau fixe** `pins[kMaxPins]` (512 max) doublé d'un compteur `pinCount`,
le motif zéro-STL universel de cette famille. Trois méthodes inline les gèrent vraiment :

- `AddPin(vertIdx, worldPos, weight)` épingle un sommet à une **position du monde**. Elle renvoie
  `false` si le tableau est plein (`pinCount >= kMaxPins`), sinon range l'entrée et incrémente —
  en `O(1)`. **Vérifiez toujours ce retour.**
- `AddBonePin(vertIdx, boneEnt, boneIdx, weight)` épingle un sommet à un **os** d'un autre squelette
  (typiquement la cape attachée aux épaules) — même garde de capacité, même `O(1)`.
- `RemovePin(vertIdx)` retire le **premier** pin de ce sommet par **swap-with-last**
  (`pins[i] = pins[--pinCount]`). Conséquence à connaître : cela **casse l'ordre** des pins et ne
  supprime que la première occurrence.

Le reste — les handles GPU (`posBufferA`/`posBufferB` pour le ping-pong Verlet, `constraintBuffer`,
`normalBuffer`, `pinBuffer`), les compteurs `vertexCount`/`constraintCount`, le flag `initialized`
et l'entité source `meshEntity` — est **géré par le système**, pas par vous. Deux structs de données
accompagnent le composant : `NkClothConstraint` (une contrainte de solveur, de type `Distance`,
`Bend`, `Shear` ou `Pin`) et `NkPinConstraint` (l'entrée d'un pin telle que stockée dans le tableau).

> **En résumé.** `NkClothSim` = un composant de tissu : on règle les paramètres physiques, on
> épingle des sommets via `AddPin`/`AddBonePin` (tableau fixe de 512, retour `bool` à vérifier), et
> `NkClothSystem` simule sur GPU. Tout ce qui touche aux *buffers* est géré par le système. Ce
> n'est **pas** un solveur que vous pilotez à la main — vous décrivez la matière, le système la fait
> vivre.

---

## Les variations de matière : cheveux, jiggle, corps mou

Le même schéma se décline pour d'autres matières, chacune avec ses paramètres propres et son
système dédié. Ce sont, pour l'essentiel, des **structs de données purs** (aucune méthode inline) :
on remplit des champs, le système fait le travail.

`NkHairSim` simule cheveux, poils et fourrure par **guide strands** interpolés. Ses paramètres
ressemblent à ceux du tissu — `stiffness`, `damping`, `friction`, `wind` — mais avec des réglages
de **style** en plus : `thickness`, `curliness` (0 droit → 1 frisé), `waviness`, et un dégradé
`rootColor` → `tipColor`. La mèche elle-même est décrite par `NkHairStrand` (jusqu'à 16 particules
par mèche). **Attention** : ici `gravity` est un **scalaire** `float32`, pas un `NkVec3f` comme dans
le tissu.

`NkJiggleBone` est l'option **la plus légère** : il fait trembler un seul os secondaire par inertie
— seins, ventre, oreilles, queue, cheveux lourds, pans de vêtement. Plutôt qu'un mesh entier, il ne
simule qu'un point (`currentPos`/`targetPos`) avec une `velocity`, rappelé vers la pose animée par
sa `stiffness`, et **contraint en angle** (`maxAngleDeg`, plus les bascules `constrainX/Y/Z`).
C'est le compromis quand `NkClothSim` serait excessif.

`NkSoftBody` simule les corps **gélatineux** (chair, muscles, slime) par contraintes
**volumétriques** sur des tétraèdres — d'où le champ `volumeStiffness` (incompressibilité) en plus
de la `stiffness` de surface, et un `tetraBuffer` côté GPU. Comme le tissu, il porte une `gravity`
vectorielle et des handles GPU gérés par son système.

```cpp
auto& jiggle = go.AddComponent<NkJiggleBone>().Get();
jiggle.boneIndex = 12;          // l'os à faire trembler
jiggle.stiffness = 0.7f;        // rappel vers la pose anim
jiggle.maxAngleDeg = 30.f;      // amplitude bornée
```

> **En résumé.** Trois matières, trois composants de **données** sans méthode : `NkHairSim`
> (cheveux par guides, `gravity` **scalaire**, réglages de style), `NkJiggleBone` (un os secondaire,
> le plus léger, contraint en angle), `NkSoftBody` (gélatine, contraintes de **volume** sur
> tétraèdres). On les remplit ; le système associé les anime.

---

## Le ragdoll et la capture de mouvement

Deux composants se distinguent parce qu'ils portent un peu de **logique d'état** inline, là où les
autres sont purement passifs.

`NkRagdoll` bascule un personnage entre animation et physique. Son enum `State` décrit les quatre
modes — `Animated` (ragdoll éteint), `Blending` (transition), `FullRagdoll` (tout physique),
`Kinematic` (le ragdoll pilote le corps physique, *active ragdoll*) — et trois helpers inline les
manient : `SetState(s)` force un état, `BlendIn(speed)` met `state = Blending` et arme la vitesse,
`BlendOut(speed)` arme une vitesse **négative**. **Piège majeur** : `BlendOut` **ne change pas**
`state` (la convention est que `blendSpeed` signé pilote la direction du fondu). Le ragdoll relie
des os à des rigidbodies via un tableau fixe `bones[kMaxBones]` (64 max) de `NkRagdollBoneLink`.

`NkMotionCapture` importe et rejoue de la mocap. Ses contrôles de lecture sont inline et triviaux —
`Play()` (`playing = true`), `Stop()` (`playing = false` **et** remet `currentTime` à zéro),
`Pause()` (`playing = false` **sans** remettre le temps). Le cœur, lui, est **spec** : `LoadBVH`
(import Biovision Hierarchy), `Apply` (poser le squelette à l'instant courant) et `Retarget`
(reciblage source → destination) sont **déclarés sans corps**. Les frames sont des `NkMocapFrame`.

> **En résumé.** `NkRagdoll` : enum `State` à 4 valeurs + helpers `SetState`/`BlendIn`/`BlendOut` —
> mais `BlendOut` ne touche **pas** `state`. `NkMotionCapture` : `Play`/`Stop`/`Pause` réels
> (`Stop` reset le temps, `Pause` non), mais `LoadBVH`/`Apply`/`Retarget` sont de la **spec**.

---

## Les morph targets : `NkBlendShapeController`

Là où les composants précédents déforment des os ou des sommets, `NkBlendShapeController` pilote des
**morph targets** (blend shapes) — les déformations faciales et de forme stockées dans le mesh. Son
idée centrale est le **driver** : au lieu de fixer un poids à la main, on **branche** une source de
données sur un blend shape. L'enum `DriverType` énumère ces sources : `Manual` (valeur directe),
`BoneAngle` (l'angle d'un os pilote le poids), `AudioLevel` (le RMS audio → lip-sync procédural),
`FacialAU` (une *action unit* de `NkFacialRig`), `Physics` (vitesse/contact) et `ECSProperty` (une
propriété ECS arbitraire). Chaque driver (struct imbriquée `Driver`) porte sa plage d'entrée
(`minIn`/`maxIn`), sa plage de sortie (`minOut`/`maxOut`), un lissage temporel et sa source.

Deux méthodes inline gèrent les drivers, sur le même tableau fixe (`drivers[kMaxDrivers]`, 64 max) :

- `AddDriver(d)` ajoute un driver ; `false` si plein, sinon `O(1)`.
- `SetWeight(shapeIdx, w)` cherche le **premier driver `Manual`** de ce shape et lui fixe
  `NkClamp(w, 0, 1)`. **Piège** : elle **ne fait rien** s'il n'existe pas déjà un driver `Manual`
  pour ce shape (il faut l'avoir `AddDriver` au préalable), et elle **ignore** les drivers non-Manual.

> **En résumé.** `NkBlendShapeController` branche des **drivers** (6 types : Manual, BoneAngle,
> AudioLevel, FacialAU, Physics, ECSProperty) sur des morph targets. `AddDriver` les ajoute,
> `SetWeight` ne pilote que les drivers **Manual** déjà présents — sinon elle est silencieuse.

---

## Les systèmes : qui anime quoi, et dans quel ordre

À chaque composant correspond un **système ECS** `final : public NkSystem`, exécuté dans le groupe
`PostUpdate` après l'animation. L'**ordre de priorité** (décroissante) est documenté et important —
le facial pose le visage avant que les blend shapes ne s'y appliquent, le squelette est posé avant
que cloth et cheveux ne s'y accrochent :

```
NkFacialSystem      800   (déclaré ailleurs, dans NkFacialRig.h)
NkBlendShapeSystem  750
NkJiggleBoneSystem  700
NkClothSystem       600
NkHairSystem        550
NkSoftBodySystem    500
NkRagdollSystem     450
NkMocapSystem       400
```

Deux familles de systèmes se distinguent. Les **systèmes GPU compute** — Cloth, Hair, SoftBody —
prennent un `NkIDevice*` au constructeur (qui peut être `nullptr`), sont `.Sequential()` (les
compute shaders ne se parallélisent pas entre eux), et exposent `SetCommandBuffer(cmd)` à appeler
**avant** `Execute`. Les **systèmes CPU** — Jiggle, Ragdoll, Mocap, BlendShape — n'ont pas de
device, et Jiggle/Ragdoll/Mocap/BlendShape ne sont **pas** `.Sequential()` (ils peuvent tourner en
parallèle). Pour tous, seul `Describe()` (la déclaration des dépendances) est inline et réel ;
`Execute()` et toute la logique privée sont **spec**.

> **En résumé.** Huit systèmes en `PostUpdate`, par priorité décroissante (750 → 400 ici). Cloth /
> Hair / SoftBody = **GPU** (device au ctor, `.Sequential()`, `SetCommandBuffer`). Jiggle / Ragdoll
> / Mocap / BlendShape = **CPU**. Seuls les `Describe` et constructeurs sont implémentés ; les
> `Execute` restent de la **spec**.

---

## Aperçu de l'API

Tout ce que les deux headers exposent publiquement. Le détail (champs, pièges, statut) est en
« Référence complète ». Statut : **défini** = corps inline réel ; **spec** = déclaré sans corps.

### Composants de données — `NkPhysicsMesh.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cloth | `struct NkClothSim` `NK_COMPONENT` | Tissu : paramètres physiques + pins + handles GPU. |
| Cloth | `NkClothSim::AddPin` / `AddBonePin` / `RemovePin` | **défini** — gérer les pins (tableau de 512), retour `bool`. |
| Cloth | `struct NkClothConstraint` (+ enum `Type`) | Contrainte de solveur (Distance / Bend / Shear / Pin). |
| Cloth | `struct NkPinConstraint` | Entrée d'un pin (monde ou os). |
| Cheveux | `struct NkHairSim` `NK_COMPONENT` | Cheveux/poils/fourrure (guides) — **aucune méthode**, `gravity` scalaire. |
| Cheveux | `struct NkHairStrand` | Mèche : ≤ 16 particules. |
| Jiggle | `struct NkJiggleBone` `NK_COMPONENT` | Os secondaire dynamique — **aucune méthode**, contraintes d'angle. |
| Corps mou | `struct NkSoftBody` `NK_COMPONENT` | Gélatine/chair (tétraèdres) — **aucune méthode**. |
| Ragdoll | `struct NkRagdoll` `NK_COMPONENT` (+ enum `State`) | Ragdoll : 4 états + helpers. |
| Ragdoll | `NkRagdoll::SetState` / `BlendIn` / `BlendOut` | **défini** — piloter l'état (`BlendOut` ne change pas `state`). |
| Ragdoll | `struct NkRagdollBoneLink` | Lien os ↔ rigidbody (`rigidbodyEntity` **non initialisé**). |
| Mocap | `struct NkMotionCapture` `NK_COMPONENT` | Import/playback mocap. |
| Mocap | `Play` / `Stop` / `Pause` | **défini** — contrôle de lecture. |
| Mocap | `LoadBVH` / `Apply` / `Retarget` | **spec** — import BVH, pose, reciblage. |
| Mocap | `struct NkMocapFrame` | Une frame : temps + transforms par os. |

### Systèmes — `NkPhysicsSystems.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Blend shapes | `struct NkBlendShapeController` `NK_COMPONENT` (+ enums `DriverType`, struct `Driver`) | Pilote les morph targets par drivers. |
| Blend shapes | `NkBlendShapeController::AddDriver` / `SetWeight` | **défini** — `SetWeight` ne pilote que les drivers Manual. |
| Système GPU | `class NkClothSystem` (prio 600) | Ctor `NkIDevice*`, `.Sequential()`, `SetCommandBuffer`. |
| Système GPU | `class NkHairSystem` (prio 550) | Ctor `NkIDevice*`, `.Sequential()`, `SetCommandBuffer`. |
| Système GPU | `class NkSoftBodySystem` (prio 500) | Ctor `NkIDevice*`, `.Sequential()`, écrit `NkSoftBody` seul. |
| Système CPU | `class NkJiggleBoneSystem` (prio 700) | Pas de device, **pas** `.Sequential()`. |
| Système CPU | `class NkRagdollSystem` (prio 450) | Pas de device, **pas** `.Sequential()`. |
| Système CPU | `class NkMocapSystem` (prio 400) | Pas de device, **pas** `.Sequential()`. |
| Système CPU | `class NkBlendShapeSystem` (prio 750) | Pas de device, **pas** `.Sequential()`. |
| Surface système | `Describe()` (chaque système) | **défini** (inline) — dépendances ECS, groupe, priorité, nom. |
| Surface système | `Execute(NkWorld&, float32 dt)` (chaque système) | **spec** — la logique réelle. |

---

## Référence complète

Chaque élément est repris à fond, avec ses usages multi-domaines. Le statut **spec** /
**défini** est rappelé là où il importe.

### `NkClothSim` à fond

Le composant central. Ses **paramètres physiques** modélisent un tissu PBD/Verlet : `stiffness`
(0 = gelée, 1 = rigide) règle la raideur globale, déclinée par `bendStiffness` (pliage) et
`shearStiffness` (cisaillement) ; `damping` amortit, `mass` est par sommet (kg) ; la `gravity` est
un **`NkVec3f`** (à distinguer du `gravity` scalaire de `NkHairSim`/`NkJiggleBone`) ; le `wind`
vectoriel se module par `windTurbulence` et `windFrequency` ; `friction` et `selfCollisionDist`
ferment la liste. Les **réglages de sim** sont `substeps` (8), `fixedDt` (1/60), `selfCollision`
(coûteuse, off) et `simulating`.

Les **pins** sont la seule logique réelle. Tableau fixe `pins[512]` + `pinCount`, motif zéro-STL :

- `AddPin(vertIdx, worldPos, weight=1)` (**défini**, `O(1)`) — épingle au monde ; range
  `{vertIdx, worldPos, Invalid, 0, weight}` ; renvoie `false` si plein.
- `AddBonePin(vertIdx, boneEnt, boneIdx, weight=1)` (**défini**, `O(1)`) — épingle à un os ; range
  `{vertIdx, {}, boneEnt, boneIdx, weight}` ; même garde de capacité.
- `RemovePin(vertIdx)` (**défini**, `O(n)` sur `pinCount`) — swap-with-last sur la **première**
  occurrence ; casse l'ordre.

Les **handles GPU** (`posBufferA`/`posBufferB`, `constraintBuffer`, `normalBuffer`, `pinBuffer`),
les compteurs et `initialized` sont remplis par `NkClothSystem` ; `meshEntity` désigne l'entité
portant le `NkMeshComponent` source.

Cas d'usage, par domaine :

- **Animation / personnage** — capes, jupes, manteaux, drapeaux portés : `AddBonePin` accroche le
  bord du tissu aux épaules, la sim fait le reste.
- **Rendu** — le `normalBuffer` recalculé par le système alimente l'éclairage du tissu déformé.
- **Gameplay** — bannières et voiles qui réagissent au `wind`, signal visuel de direction du vent.
- **Physique** — `selfCollision` (off par défaut, coûteuse) évite l'auto-traversée d'un tissu épais.
- **Scène / éditeur** — exposer `stiffness`/`damping`/`gravity` dans l'inspecteur pour régler une
  matière en direct.

`NkClothConstraint` (struct de données) décrit une contrainte du solveur : son enum `Type` vaut
`Distance` (garde la distance v0–v1), `Bend` (pliage entre quads adjacents), `Shear` (diagonale du
quad) ou `Pin` (sommet fixé) ; les champs sont `v0`, `v1` (inutilisé pour `Pin`), `restLen`
(auto-calculé), `stiffness` (override local [0..1]). `NkPinConstraint` est l'entrée stockée dans le
tableau de pins : `vertexIdx`, `worldPos` (si l'os est invalide), `boneEntity`, `boneIndex`,
`weight` (0 = libre, 1 = fixé).

### `NkHairSim` et `NkHairStrand` à fond

`NkHairSim` (**aucune méthode** — données pures) simule cheveux, poils et fourrure par **guide
strands** (jusqu'à 2048) interpolés en mèches de rendu. Physique : `stiffness`, `damping`,
`friction`, `wind` vectoriel, `length` (m) et `particlesPerStrand` (8). **Important** : `gravity`
est ici un **scalaire** `float32`. Style : `thickness` (m), `curliness` (0 droit → 1 frisé),
`waviness`, et le dégradé `rootColor` → `tipColor`. Côté GPU : `guideBuffer`, `interpBuffer`,
`guideCount`, `renderCount`, `guideInfluenceRadius`, plus `simulating`, `initialized` et
`bodyEntity` (le corps pour les collisions).

`NkHairStrand` est la mèche : `particles[16]` (positions), `prevPos[16]` (Verlet), `particleCount`,
`rootPos` (le follicule, fixe) et `boneIndex` (l'os que suit le follicule).

Cas d'usage :

- **Animation / personnage** — chevelure, fourrure d'animal, crinière qui suivent la tête via
  `boneIndex` du follicule.
- **Rendu** — `tipColor`/`rootColor` et `thickness` alimentent le shader de cheveux ; `renderCount`
  borne le nombre de mèches rendues.
- **Gameplay** — fourrure qui réagit au `wind` d'une zone, retour visuel d'ambiance.

### `NkJiggleBone` à fond

Le plus **léger** des composants (**aucune méthode** — données pures) : il ne simule qu'**un seul
os secondaire** par inertie, là où `NkClothSim` simulerait un mesh entier. La doc cite seins,
ventre, oreilles, queue, cheveux lourds, pans de vêtement. `boneIndex` désigne l'os cible ;
`stiffness` ([0..1]) règle le rappel vers la pose animée, complété par `damping`, `mass`, `gravity`
(**scalaire**). Les **contraintes d'angle** (`maxAngleDeg`, `constrainX/Y/Z`) bornent l'amplitude.
L'état interne — `velocity`, `currentPos` (tip courant), `targetPos` (cible anim), `currentRot`
(`NkQuatf::Identity()`), `enabled` — est mis à jour par `NkJiggleBoneSystem`.

Cas d'usage :

- **Animation / personnage** — secondary motion crédible à coût minime (queue, oreilles).
- **Gameplay** — accessoires qui ballottent (pendentif, sacoche) sans budget cloth.

### `NkSoftBody` à fond

Corps **gélatineux** (**aucune méthode** — données pures) : chair, muscles, slime. La différence
avec le tissu est le `volumeStiffness` (incompressibilité) et le `tetraBuffer` — la sim impose des
contraintes **volumétriques** sur des tétraèdres, pas seulement de surface. Physique : `stiffness`,
`volumeStiffness`, `damping`, `mass`, `gravity` (**`NkVec3f`**), `substeps` (4), `simulating`. GPU
(géré par `NkSoftBodySystem`) : `posBuffer`, `prevPosBuffer`, `tetraBuffer`, `vertexCount`,
`tetraCount`, `initialized`, `meshEntity`.

Cas d'usage :

- **Animation / personnage** — chair qui se déforme aux impacts, muscles sous tension.
- **Gameplay** — créatures molles, blobs, projectiles déformables.
- **Rendu** — déformation volumétrique sans skin rigide.

### `NkRagdoll` à fond

Bascule un personnage entre **animation** et **physique**. Son enum `State` :

- `Animated` — purement animé, ragdoll éteint.
- `Blending` — transition animation → ragdoll.
- `FullRagdoll` — purement physique.
- `Kinematic` — le ragdoll **pilote** le corps pour le moteur physique (*active ragdoll*).

Champs : tableau fixe `bones[64]` de `NkRagdollBoneLink` + `boneCount` ; `state` (départ
`Animated`), `blendWeight` (0 anim → 1 ragdoll), `blendSpeed` (3 u/s), `impactThreshold` (10 m/s
déclenchant l'auto-ragdoll), `autoActivate`, `skeletonEntity`.

Helpers inline (**définis**) :

- `SetState(s)` — affecte `state = s`.
- `BlendIn(speed=2)` — `state = Blending` **et** `blendSpeed = speed`.
- `BlendOut(speed=2)` — `blendSpeed = -speed` ; **ne change PAS `state`**. La convention est qu'un
  `blendSpeed` **signé** pilote la direction du fondu : `BlendIn` entre en blending, `BlendOut`
  inverse simplement le sens de la vitesse.

`NkRagdollBoneLink` (struct de données) relie un os à un rigidbody : `skeletonBoneIdx`,
`rigidbodyEntity` (**piège : seul `NkEntityId` du fichier sans `= Invalid()` par défaut — valeur
indéterminée hors agrégat, à initialiser explicitement**), `boneToBody` (`NkMat4f::Identity()`,
offset d'alignement), `mass`.

Cas d'usage :

- **Gameplay** — mort/KO d'un personnage : `BlendIn` puis `FullRagdoll` ; `autoActivate` +
  `impactThreshold` déclenchent un ragdoll au choc.
- **Animation** — `Blending`/`blendWeight` pour mêler une pose animée et la physique (hit reaction).
- **Physique** — `Kinematic` pour un *active ragdoll* qui conduit le solveur rigide.

### `NkMotionCapture` et `NkMocapFrame` à fond

Import et playback de capture de mouvement. Champs : `frames` (`NkVector<NkMocapFrame>`), `fps`
(30), `duration`, `currentTime`, `playing`, `loop`, `speed`, `blendWeight` (mélange avec l'anim
normale), `skeletonEntity`. `NkMocapFrame` porte `time` et `boneTransforms`
(`NkVector<NkMat4f>`, transform monde par os).

Contrôles de lecture inline (**définis**) :

- `Play()` — `playing = true`.
- `Stop()` — `playing = false` **et** `currentTime = 0` (rembobine).
- `Pause()` — `playing = false` **sans** toucher `currentTime` (reprise au même point).

Cœur **spec** (déclaré sans corps) :

- `LoadBVH(path)` — import d'un fichier Biovision Hierarchy. **Non implémenté ici.**
- `Apply(skeleton)` — applique la pose courante au squelette. **Non implémenté ici.**
- `Retarget(srcSkeleton, dstSkeleton)` — reciblage source → destination. **Non implémenté ici.**

Cas d'usage (à la livraison de la logique) :

- **Animation** — rejouer une mocap importée, la mélanger à une anim via `blendWeight`.
- **IO** — `LoadBVH` comme pont vers les pipelines de capture externes.
- **Animation / production** — `Retarget` pour appliquer une capture d'un acteur à un autre rig.

### `NkBlendShapeController` et son système à fond

Pilote les **morph targets** par **drivers** : on branche une source de données sur un blend shape
au lieu d'en fixer le poids à la main. L'enum `DriverType` :

- `Manual` — valeur directe via `SetWeight()`.
- `BoneAngle` — l'angle d'un os → poids (via courbe).
- `AudioLevel` — niveau audio RMS → poids (lip-sync procédural).
- `FacialAU` — une *action unit* `NkFacialRig.auWeights[X]` → poids.
- `Physics` — vitesse/contact → poids.
- `ECSProperty` — propriété ECS arbitraire via offset.

La struct imbriquée `Driver` porte : `shapeIdx`, `type`, `weight` (valeur courante [0..1]), la plage
d'entrée `minIn`/`maxIn`, la plage de sortie `minOut`/`maxOut`, `smoothing` (lissage temporel),
`srcEntity`, `srcBoneIdx` (pour `BoneAngle`), `srcAUIdx` (pour `FacialAU`). Tableau fixe
`drivers[64]` + `driverCount` + `meshEntity`.

Méthodes inline (**définies**) :

- `AddDriver(d)` — append ; `false` si `driverCount >= 64`, sinon `O(1)`.
- `SetWeight(shapeIdx, w)` — cherche le **premier** driver de ce `shapeIdx` **dont le type est
  `Manual`** et lui fixe `NkClamp(w, 0, 1)` (`O(n)`). **Piège** : ne fait rien si aucun driver
  `Manual` pour ce shape (il faut `AddDriver` un driver Manual au préalable), et **ignore** les
  drivers non-Manual.

`NkBlendShapeSystem` (priorité 750, CPU, pas `.Sequential()`) écrit `NkBlendShapeController`, lit
`NkSkeleton` + `NkFacialRig` ; sa privée `ResolveDriver(...)` (qui transforme une source en poids)
est **spec**.

Cas d'usage :

- **Animation faciale** — `FacialAU` relie les action units du `NkFacialRig` aux blend shapes du
  visage.
- **Audio / dialogue** — `AudioLevel` produit un lip-sync procédural à partir du RMS.
- **Gameplay** — `Physics` (vitesse, contact) déforme une bouche/joue à l'effort ; `ECSProperty`
  branche n'importe quelle donnée de jeu sur une forme.
- **UI / éditeur** — `Manual` + `SetWeight` pour régler une expression à la main dans l'inspecteur.

### Les systèmes à fond

Chaque système est `final : public NkSystem`. Son `Describe()` (inline, **défini**) déclare ses
dépendances ECS, son groupe, sa priorité et son nom via le builder fluent `NkSystemDesc`
(`.Reads<T>()`, `.Writes<T>()`, `.InGroup(...)`, `.WithPriority(...)`, `.Sequential()`,
`.Named(...)`). Son `Execute(NkWorld&, float32 dt)` et toutes ses privées sont **spec**.

- **`NkJiggleBoneSystem`** (prio **700**, CPU, **pas** `.Sequential()`) — lit `NkTransform`, écrit
  `NkJiggleBone` + `NkSkeleton`. Constructible par défaut, pas de device. Privées **spec** :
  `UpdateJiggle(...)`, `ClampAngle(q, maxDeg)`.
- **`NkClothSystem`** (prio **600**, GPU, `.Sequential()`) — ctor `explicit NkClothSystem(NkIDevice*
  device = nullptr)`. Lit `NkTransform`, écrit `NkClothSim`. `SetCommandBuffer(cmd)` (**défini**) à
  appeler avant `Execute`. Privées **spec** : `InitCloth`, `SimulateCloth`, `ApplyWind`,
  `ResolvePins`, `UploadResult`. Membres : `mDevice`, `mCmd`, et les pipelines compute
  `mSimPipeline` (Verlet), `mConstraintPipeline`, `mNormalPipeline`, `mPipelinesReady`.
- **`NkHairSystem`** (prio **550**, GPU, `.Sequential()`) — ctor `NkIDevice*`. Lit `NkTransform` +
  `NkSkeleton`, écrit `NkHairSim`. `SetCommandBuffer` (**défini**). Privées **spec** : `InitHair`,
  `SimulateHair`, `InterpolateStrands`, `BuildRenderBuffer`. Membres : `mDevice`, `mCmd`,
  `mSimPipeline`, `mInterpPipeline`, `mReady`.
- **`NkSoftBodySystem`** (prio **500**, GPU, `.Sequential()`) — ctor `NkIDevice*`. Écrit
  `NkSoftBody` **uniquement** (ne lit pas `NkTransform`). `SetCommandBuffer` (**défini**). Privées
  **spec** : `InitSoftBody`, `SimulateSoftBody`, `BuildTetrahedra`. Membres : `mDevice`, `mCmd`,
  `mPipeline`, `mReady`.
- **`NkRagdollSystem`** (prio **450**, CPU, **pas** `.Sequential()`) — pas de device (pont CPU vers
  les rigidbodies ECS). Lit `NkTransform`, écrit `NkRagdoll` + `NkSkeleton`. Privées **spec** :
  `TransitionToRagdoll`, `ApplyRagdollToSkeleton`, `BlendAnimRagdoll`.
- **`NkMocapSystem`** (prio **400**, CPU, **pas** `.Sequential()`) — pas de device. Écrit
  `NkMotionCapture` + `NkSkeleton`. Privées **spec** : `PlaybackMocap`, `InterpolateFrames(a, b, t,
  boneIdx)`.
- **`NkBlendShapeSystem`** (prio **750**, CPU, **pas** `.Sequential()`) — pas de device. Écrit
  `NkBlendShapeController`, lit `NkSkeleton` + `NkFacialRig`. Privée **spec** : `ResolveDriver`.

L'ordre de priorité décroissante garantit la chaîne de dépendances : `NkFacialSystem` (800, déclaré
dans `NkFacialRig.h`) pose le visage → `NkBlendShapeSystem` (750) applique les morph targets →
`NkJiggleBoneSystem` (700) → `NkClothSystem` (600) → `NkHairSystem` (550) → `NkSoftBodySystem`
(500) → `NkRagdollSystem` (450) → `NkMocapSystem` (400).

### Idiomes et pièges transverses

- **Tableaux fixes + compteur** partout (zéro-STL) : `pins[512]`/`pinCount`, `bones[64]`/`boneCount`,
  `drivers[64]`/`driverCount`, plus `kMaxParticles=16` par mèche et `kMaxGuideStrands=2048`. Toujours
  **vérifier le retour `bool`** des `Add*` (capacité atteinte = `false`, rien n'est ajouté).
- **`NkRagdollBoneLink::rigidbodyEntity` non initialisé** — seul `NkEntityId` du fichier sans
  `= Invalid()` : valeur indéterminée si construit hors agrégat, à initialiser explicitement.
- **`NkRagdoll::BlendOut` ne change pas `state`** (il arme seulement un `blendSpeed` négatif), alors
  que `BlendIn` met `state = Blending`. **`Pause()` (mocap) ne reset pas `currentTime`** ; `Stop()`
  le fait.
- **`SetWeight` (blend shapes) est silencieuse** sans driver `Manual` préalable pour le shape visé,
  et ignore les drivers non-Manual.
- **Cohérence de la gravité** : `NkClothSim.gravity` et `NkSoftBody.gravity` sont des `NkVec3f` ;
  `NkHairSim.gravity` et `NkJiggleBone.gravity` sont des `float32` **scalaires**. Ne pas confondre.
- **Systèmes GPU vs CPU** : Cloth/Hair/SoftBody prennent un `NkIDevice*` (peut être `nullptr`), sont
  `.Sequential()` et exigent `SetCommandBuffer` avant `Execute` ; les autres sont CPU, sans device.
- **Statut** : tout ce qui est marqué **spec** (les `Execute`, `LoadBVH`/`Apply`/`Retarget`, toutes
  les privées de simulation) est **déclaré sans corps** dans ces headers — implémentation hors header,
  vraisemblablement non livrée. Seules les méthodes **inline** énumérées ont une implémentation réelle.

---

### Exemple

```cpp
#include "Noge/Physics/NkPhysicsMesh.h"
#include "Noge/Systems/NkPhysicsSystems.h"
using namespace nkentseu;   // math + ecs sont déjà 'using' dans le header

// 1) Une cape de tissu accrochée aux épaules (sommets épinglés à des os).
auto& cloth = capeGO.AddComponent<NkClothSim>().Get();
cloth.stiffness = 0.85f;
cloth.gravity   = { 0.f, -9.81f, 0.f };       // NkVec3f
if (!cloth.AddBonePin(0, spineEnt, 7)) { /* tableau de pins plein */ }
cloth.AddBonePin(1, spineEnt, 8);

// 2) Une queue qui ballotte (le plus léger : un seul os).
auto& jiggle = tailGO.AddComponent<NkJiggleBone>().Get();
jiggle.boneIndex   = 12;
jiggle.stiffness   = 0.7f;
jiggle.maxAngleDeg = 30.f;

// 3) Le héros s'effondre quand il est touché.
auto& rag = heroGO.AddComponent<NkRagdoll>().Get();
rag.BlendIn(2.f);                 // state = Blending, blendSpeed = +2
// ... plus tard, retour à l'animation :
rag.BlendOut(2.f);               // blendSpeed = -2 (state inchangé)

// 4) Lip-sync procédural via un driver audio sur un morph target.
auto& morph = headGO.AddComponent<NkBlendShapeController>().Get();
NkBlendShapeController::Driver d{};
d.shapeIdx = jawOpenShape;
d.type     = NkBlendShapeController::DriverType::AudioLevel;
morph.AddDriver(d);

// 5) Enregistrer les systèmes GPU avec un device + command buffer.
NkClothSystem clothSys{ device };
clothSys.SetCommandBuffer(cmd);  // avant Execute
```

---

[← Index Noge](README.md) · [Récap Noge](../Noge.md) · [Couche Engine](../README.md)
