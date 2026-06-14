# Le monde de collision

> Couche **Runtime** · NKCollision · Le cœur du module : le `CollisionWorld` qui détient les corps
> et fait avancer la détection, son backend de calcul enfichable (`ICollisionBackend`, GPU ou CPU),
> et le visualiseur de débogage `DebugDraw`.

Détecter des collisions, ce n'est pas seulement « est-ce que A touche B ». C'est gérer **des
milliers de corps** qui bougent chaque frame, ne tester que les paires qui en valent la peine,
remonter les contacts au gameplay, et le faire assez vite pour tenir le budget temps réel. NKCollision
organise tout cela autour d'un objet central, le `CollisionWorld` : on lui **ajoute des corps**, on
appelle `step()` une fois par frame, et on **écoute** les contacts via des callbacks. Le reste —
quel algorithme de partition spatiale, calcul sur CPU ou GPU, un thread ou plusieurs — est décidé
par le monde selon sa configuration, sans que l'appelant s'en mêle.

Cette page couvre la « famille World » : la configuration (`WorldConfig`), le corps collidable
(`CollisionBody`), le monde lui-même (`CollisionWorld`), la couche de calcul abstraite
(`ICollisionBackend` et son repli `CPUBackend`), le pool d'objets utilitaire (`ObjectPool`), et le
visualiseur (`DebugDraw`). Ce **n'est pas** une bibliothèque de formes ni l'implémentation des tests
géométriques fins (sphère-sphère, GJK…) : ceux-ci vivent dans `shapes.h` / `narrowphase.h`, hors de
cette page.

- **Namespace** : `col` (et non `nkentseu::…` — voir l'avertissement plus bas)
- **Headers réels** : `#include "NkWorld.h"`, `#include "NKGPUBackend.h"`, `#include "NkDebugDraw.h"`
- **Header parapluie** : `NKCollision.h`

> **Avertissement — module greenfield.** NKCollision est marqué « non démarré » dans la ROADMAP, et
> cela se voit. Le namespace est `col` au lieu de la convention `nkentseu`. Le code est **massivement
> STL** (`std::vector`, `std::unordered_map`, `std::function`, `std::thread`, `std::malloc`…), à
> rebours de la règle zéro-STL du moteur. Les includes du header parapluie ne correspondent **pas** à
> la casse des fichiers réels (`world.h` vs `NkWorld.h`), donc il **ne compile pas tel quel**.
> Plusieurs symboles utilisés ici (`Vec3`, `Transform`, `CollisionShape`, `Broadphase`,
> `ContactManifold`…) sont déclarés dans des fichiers non fournis. Cette page documente **uniquement
> l'API réelle des trois headers ci-dessus**, en signalant les pièges au fil de l'eau.

---

## Le corps collidable : `CollisionBody`

Avant le monde, il faut l'unité qu'il manipule. Un `CollisionBody` est **une entité que l'on peut
heurter** : une forme (`shape`), une position dans l'espace (`transform`), une vitesse (`velocity`,
utilisée pour calculer une AABB *prédictive*), et un sac de drapeaux et de filtres qui décident
comment elle participe à la détection. Ce n'est **pas** un corps physique au sens dynamique : il n'y
a ni masse, ni intégrateur, ni force ici — juste de la géométrie qui se déplace et que l'on teste.

Les **drapeaux** (`BodyFlags`, un bitmask) règlent le comportement : `BodyFlag_Static` pour ce qui
ne bouge jamais (les murs), `BodyFlag_Kinematic` pour ce que l'utilisateur déplace à la main,
`BodyFlag_Trigger` pour les zones qui ne renvoient pas de réponse mais déclenchent des événements
(une porte, une zone de dégât), `BodyFlag_Sleeping` pour endormir un corps au repos (la broadphase
le saute), `BodyFlag_Is2D` pour une collision dans le plan XY, et `BodyFlag_CCD` pour la détection
continue. On les lit via les helpers `isStatic()`, `isKinematic()`, `isTrigger()`, `isSleeping()`,
`is2D()` — mais attention, **il n'y a pas de `isCCD()`** malgré le drapeau correspondant.

Les **filtres de couches** (`layer` et `mask`, deux bitmasks à `0xFFFFFFFF` par défaut) décident
*qui peut heurter qui*. La règle est symétrique : `collidesWith(o)` renvoie vrai si
`(layer & o.mask) && (o.layer & mask)`. Autrement dit, A et B ne se testent que si chacun est dans
le masque de l'autre — exactement le mécanisme classique des « collision layers » (les balles du
joueur ignorent le joueur, les projectiles ennemis ignorent les ennemis, etc.).

```cpp
col::CollisionBody wall;
wall.shape     = /* une forme issue de shapes.h */;
wall.transform = /* une Transform */;
wall.flags     = col::BodyFlag_Static;     // ne bouge jamais
wall.layer     = LAYER_WORLD;
wall.mask      = LAYER_PLAYER | LAYER_ENEMY;
```

Le champ `userData` (un `void*`) est votre crochet vers le moteur : on y range le pointeur de
l'entité ECS, du `GameObject`, de l'acteur — c'est ce qu'on récupère dans les callbacks pour relier
un contact à un objet de gameplay. Enfin `cachedAABB` est la boîte englobante mémorisée du corps :
on **ne la remplit pas soi-même**, le monde la recalcule à chaque `addBody` / `setTransform`.

> **En résumé.** `CollisionBody` = forme + transform + vitesse + drapeaux + filtres de couches. Les
> drapeaux règlent static/kinematic/trigger/sleeping/2D/CCD ; `collidesWith` applique le filtre
> `layer`/`mask` symétrique. `userData` relie au moteur, `cachedAABB` est géré par le monde. Pas de
> masse ni de dynamique : c'est de la **détection**, pas de la simulation.

---

## Configurer puis créer le monde : `WorldConfig` et `CollisionWorld`

Le `CollisionWorld` se construit à partir d'une `WorldConfig` — et tout se joue dans cette config,
car elle fixe les choix qu'on ne pourra plus changer ensuite : quel backend de calcul
(`gpuBackend`, `enableGPU`), quel algorithme de partition (`broadphase`), combien de threads
(`threadCount`, `multithreaded`), et les **plafonds** mémoire (`maxBodies`, `maxPairs`,
`maxContacts`) qui dimensionnent les pools préalloués. Le champ `gpuThreshold` (512 par défaut) est
subtil : en dessous de ce nombre de corps, le monde reste sur CPU même si un GPU est disponible —
parce que le coût de transfert vers le GPU ne se rentabilise qu'à grande échelle.

La création suit le **modèle RAII**, pas le pattern Create/Destroy du moteur : `CollisionWorld
world(cfg);` construit tout (backend choisi via `BackendFactory`, threads, buffers GPU si le backend
est GPU), et le destructeur libère les buffers. On ne fait **aucun** `Create`/`Destroy` manuel.

```cpp
col::WorldConfig cfg;
cfg.gpuBackend   = col::GPUBackend::Auto;   // meilleur backend disponible
cfg.maxBodies    = 65536;
cfg.gpuThreshold = 512;                      // bascule GPU au-delà
col::CollisionWorld world(cfg);             // ou simplement: col::CollisionWorld world;
```

Une fois le monde vivant, on lui **ajoute des corps** avec `addBody`, qui renvoie un `uint64_t` —
**l'identifiant à conserver**. Piège important : l'`id` que vous avez mis dans le `CollisionBody`
est **écrasé** ; seul l'id retourné fait foi. C'est cet id qu'on passe ensuite à `setTransform`,
`setVelocity`, `getBody` et `removeBody`. `addBody` calcule aussi la `cachedAABB` et insère le corps
dans la broadphase ; `setTransform` recalcule l'AABB et met la broadphase à jour. Les setters sont
des **no-op silencieux** si l'id est inconnu (pas d'erreur).

```cpp
uint64_t ballId = world.addBody(ball);       // garder cet id !
world.setTransform(ballId, newTransform);    // chaque frame
col::CollisionBody* b = world.getBody(ballId);  // peut être nullptr
```

`getBody` renvoie un pointeur **dans une `unordered_map`** : il peut être invalidé par un `addBody`
ou `removeBody` ultérieur. À ne pas garder entre deux frames.

> **En résumé.** Construisez une `WorldConfig` (backend, plafonds, `gpuThreshold`) puis
> `CollisionWorld world(cfg)` — RAII, pas de Create/Destroy. `addBody` retourne l'id (qui écrase
> celui du corps) : conservez-le. `setTransform`/`setVelocity`/`removeBody`/`getBody` prennent cet
> id ; les setters sont no-op si l'id est inconnu, et `getBody` peut renvoyer `nullptr` ou un
> pointeur invalidable.

---

## Faire avancer la détection : `step` et les callbacks

Le cœur de la boucle, c'est `world.step(dt)`, appelé une fois par frame. Il incrémente le compteur
de frame, choisit le **chemin de calcul** — GPU si le backend est GPU *et* qu'il y a au moins
`gpuThreshold` corps, sinon CPU (mono- ou multi-thread selon la config et le nombre de paires) —,
fait la broadphase puis la narrowphase, et enfin **déclenche les callbacks**. Détail à connaître :
**`dt` est actuellement ignoré** (`(void)dt`) ; il fait partie de la signature mais n'influe sur
rien pour l'instant.

Pour réagir aux collisions, on branche **deux callbacks avant le premier `step`**.
`setContactCallback` reçoit, pour chaque vraie collision avec réponse, le manifold de contact et les
deux corps concernés — c'est là qu'on applique des dégâts, joue un son, déclenche un effet.
`setTriggerCallback` reçoit, quand au moins un des deux corps est un trigger, les deux ids et un
booléen `entered`. Piège majeur à connaître : **`entered` vaut toujours `true`** — le module
n'émet jamais d'événement de sortie (« exit »), donc on ne peut pas détecter qu'un corps **quitte**
une zone trigger avec cette API.

```cpp
world.setContactCallback(
    [](const col::ContactManifold& m, const col::CollisionBody& a, const col::CollisionBody& b) {
        // a.userData / b.userData → remonter aux entités du moteur
    });
world.setTriggerCallback(
    [](uint64_t a, uint64_t b, bool entered) {
        // entered est TOUJOURS true : pas d'événement de sortie
    });

while (running) {
    world.step(dt);   // dt ignoré pour l'instant
}
```

> **En résumé.** `step(dt)` une fois par frame fait broadphase → narrowphase → callbacks, mais
> **ignore `dt`** et choisit seul CPU/GPU/multi-thread. Branchez `setContactCallback` (collisions
> avec réponse) et `setTriggerCallback` (zones) **avant** le premier step. Le trigger n'émet que des
> « enter » (`entered` toujours `true`), jamais de « exit ».

---

## Les requêtes : `raycast`, `overlapAABB`, `testBodyBody`

À côté de la boucle automatique, le monde répond à des **questions ponctuelles**. `raycast` lance un
rayon et remplit un `RaycastHit` (id du corps touché, point, normale, distance, `userData`). Mais
attention : dans l'état actuel, le test détaillé rayon-contre-forme est un **TODO** — `raycast` ne
teste que l'**AABB** du corps. Il est donc utile pour une présélection grossière, pas pour un tir
précis. `overlapAABB` remplit un `std::vector<uint64_t>` avec les ids de tous les corps chevauchant
une boîte donnée (sélection à la souris, requête de zone, déclencheurs d'IA). `testBodyBody` fait un
test **direct** entre deux corps connus et renvoie le manifold résultant — pratique pour vérifier une
paire précise hors de la boucle.

> **En résumé.** `raycast` (AABB seulement pour l'instant, TODO sur le test fin), `overlapAABB`
> (tous les ids dans une boîte), `testBodyBody` (test direct d'une paire). Des requêtes à la demande,
> en complément du `step` automatique.

---

## Le backend de calcul : `ICollisionBackend` et `CPUBackend`

Ce qui distingue NKCollision d'un détecteur naïf, c'est que **le calcul lourd est abstrait** derrière
`ICollisionBackend`. Le monde en détient un (`unique_ptr`) et lui délègue la broadphase et la
narrowphase « en masse » : à plat dans des buffers, traitables en parallèle. L'idée est qu'un même
monde tourne sur **CPU** ou sur **GPU** (CUDA, OpenGL Compute…) sans rien changer au code appelant —
seul `WorldConfig::gpuBackend` change.

Le backend manipule des **buffers agnostiques** (`GPUBuffer` : un `handle` opaque, une taille, le
type de backend) et travaille sur des layouts plats pensés pour le GPU : `GPUBodyAABB` (une AABB par
corps, alignée 16), `GPUPair` (une paire de la broadphase, par **indices** et non par ids), et
`GPUContact` (un contact issu de la narrowphase). Les méthodes clés : `createBuffer` /
`destroyBuffer` / `uploadBuffer` / `downloadBuffer` pour la mémoire, `gpuBroadphase` (teste les AABB
en parallèle, renvoie le nombre de paires) et `gpuNarrowphase` (renvoie le nombre de contacts),
`sync` / `flush` pour la synchronisation, et un jeu de **capacités** (`maxBufferSize`, `warpSize`,
`maxWorkgroupSize`, `supportsAtomics`) pour s'adapter au matériel.

Le **repli toujours disponible** est `CPUBackend`. C'est une implémentation complète et inline qui
fait partie de l'API publique (contrairement aux backends GPU spécifiques, gardés par `#ifdef` et à
considérer comme du détail plateforme). Son `gpuBroadphase` est un **brute-force O(n²)** AABB-AABB,
son `gpuNarrowphase` renvoie `0` (la narrowphase CPU est faite ailleurs, dans le monde), et ses
buffers sont alloués via **`std::malloc` / `std::free`**. C'est le piège ownership à retenir : ce
backend parle au **heap CRT**, incompatible avec l'allocateur NKMemory — ne mélangez jamais ces
mémoires.

On ne crée pas le backend à la main : `BackendFactory::create(requested)` le choisit, et c'est ce
que fait le constructeur du monde. Sachez seulement que `create` ne gère **que** CUDA et OpenGL (si
compilés) plus `Auto` ; `VulkanCompute`, `DirectX11`, `DirectX12` et `WebGPU` retombent
**silencieusement** sur `CPUBackend`.

> **En résumé.** `ICollisionBackend` abstrait broadphase et narrowphase « en masse » sur buffers
> plats (`GPUBuffer`, `GPUBodyAABB`, `GPUPair`, `GPUContact`), pour faire tourner le même monde sur
> CPU ou GPU. `CPUBackend` est le repli public : broadphase O(n²), `std::malloc` (heap CRT, hors
> NKMemory). `BackendFactory` choisit ; Vulkan/DX/WebGPU non gérés → repli CPU silencieux.

---

## L'outil du module : `ObjectPool`

`ObjectPool<T>` est un **pool d'objets à taille fixe**, à allocation zéro après construction : on
réserve `capacity` éléments d'un coup, et ensuite `alloc()` / `free()` se contentent de pousser et
piocher des indices dans une free-list, en `O(1)`. Le monde s'en sert en interne pour ses manifolds
de contact, mais c'est un utilitaire générique. Le piège : `free(ptr)` calcule l'index par
**arithmétique de pointeur** (`ptr - storage_.data()`) **sans vérifier** que le pointeur appartient
bien au pool — passer un pointeur étranger corrompt l'état. `reset()` remet tout libre en `O(n)`,
`capacity()` et `used()` renseignent l'occupation.

> **En résumé.** `ObjectPool<T>` = capacité fixe, `alloc`/`free` en `O(1)` via free-list d'indices,
> zéro allocation après le ctor. `free` ne valide pas l'appartenance du pointeur. Utilisé en interne
> pour les manifolds, mais réutilisable.

---

## Visualiser : `DebugDraw` et `Color`

Une détection de collision invisible est impossible à déboguer. `DebugDraw` règle cela en restant
**agnostique du moteur de rendu** : on lui branche **ses propres** primitives de dessin via trois
callbacks (`setLineCallback`, `setPointCallback`, `setTextCallback`), et tous ses helpers de haut
niveau (sphères, AABB, capsules, contacts, flèches de normale…) se ramènent à ces trois primitives.
Si un callback n'est pas branché, l'émetteur correspondant est simplement un no-op. Les couleurs
passent par `Color` (RGBA 0-255) avec des fabriques prêtes (`Color::Red()`, `Green()`, `Blue()`,
`Yellow()`, `White()`, `Gray()`, `Cyan()`, `Orange()`, `Magenta()`).

```cpp
col::DebugDraw dbg;
dbg.setLineCallback([&](const col::Vec3& a, const col::Vec3& b, col::Color c) {
    myRenderer.DrawLine(a, b, c);   // on branche notre propre rendu
});
dbg.drawAABBs = true;               // flags publics, modifiables directement
for (auto id : myBodyIds) dbg.drawBody(*world.getBody(id));
```

Deux **limites importantes** à connaître. D'abord `drawWorld(world)` **ne peut pas itérer les
corps** (ils sont privés dans `CollisionWorld`) : il ne dessine que les contacts via
`world.contacts()`. Pour voir vos corps, vous devez appeler `drawBody` / `drawShape` **vous-même**
sur vos propres corps. Ensuite `drawBVHNode` est un **placeholder non fonctionnel** : le vrai dessin
des nœuds du BVH demande un accès aux internes qui n'est pas exposé.

> **En résumé.** `DebugDraw` est agnostique du rendu : branchez 3 callbacks (ligne/point/texte) et
> ses helpers s'y ramènent. Réglez les flags publics (`drawAABBs`, `drawContacts`…) directement.
> Deux limites : `drawWorld` ne dessine que les contacts (corps privés → appelez `drawBody`
> vous-même), et `drawBVHNode` est un placeholder.

---

## Aperçu de l'API

Tous les éléments publics des trois headers, regroupés par fichier. Complexités/pièges signalés.

### `NkWorld.h` — corps, configuration, monde

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Drapeaux | `enum BodyFlags` : `None`, `Static`, `Kinematic`, `Trigger`, `Sleeping`, `Is2D`, `CCD` | Bitmask de comportement d'un corps (préfixe `BodyFlag_`). |
| Corps | `struct CollisionBody` : `id`, `shape`, `transform`, `velocity`, `flags`, `layer`, `mask`, `userData`, `cachedAABB` | Entité collidable (géométrie + filtres), pas de dynamique. |
| Corps (helpers) | `isStatic` `isKinematic` `isTrigger` `isSleeping` `is2D` | Tests de drapeau (**pas** de `isCCD`). |
| Corps (helpers) | `collidesWith(o)` | Filtre `layer`/`mask` symétrique. |
| Requête | `struct RaycastHit` : `bodyId`, `point`, `normal`, `distance`, `userData` | Résultat de raycast. |
| Callbacks | `using ContactCallback`, `using TriggerCallback` | Signatures des callbacks de contact / trigger. |
| Config | `struct WorldConfig` : `gpuBackend`, `broadphase`, `threadCount`, `maxBodies`, `maxPairs`, `maxContacts`, `enableGPU`, `gpuThreshold`, `persistContacts`, `contactBreakDist`, `multithreaded` | Paramètres de création du monde. |
| Pool | `template ObjectPool<T>` : ctor `(capacity)`, `alloc` `[O(1)]`, `free` `[O(1)]`, `reset` `[O(n)]`, `capacity`, `used` | Pool taille fixe, free-list d'indices. |
| Monde — vie | `CollisionWorld(cfg = {})`, `~CollisionWorld()` | Création RAII (backend + buffers GPU), destruction. |
| Monde — corps | `addBody(b)` → `uint64_t`, `removeBody(id)`, `setTransform(id, t)`, `setVelocity(id, v)`, `getBody(id)` → `CollisionBody*` | Gestion des corps par id (l'id du corps est **écrasé**). |
| Monde — callbacks | `setContactCallback(cb)`, `setTriggerCallback(cb)` | Branche les callbacks (move). |
| Monde — update | `step(dt = 0.016f)` | Une frame de détection (**`dt` ignoré**). |
| Monde — requêtes | `raycast(ray, hit&)`, `overlapAABB(aabb, results&)`, `testBodyBody(idA, idB, out&)` | Raycast (**AABB seul**), recouvrement, test direct de paire. |
| Monde — état | `bodyCount`, `contactCount`, `frameStamp`, `backend()`, `contacts()` | Compteurs, accès backend, contacts de la frame. |

### `NKGPUBackend.h` — backends de calcul

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Sélection | `enum class GPUBackend` : `None`, `CUDA`, `OpenGLCompute`, `VulkanCompute`, `DirectX11`, `DirectX12`, `WebGPU`, `Auto` | Choix du backend de calcul. |
| Buffer | `struct GPUBuffer` : `handle`, `byteSize`, `backend`, `mapped`, `valid()` | Handle de buffer GPU agnostique. |
| Layouts | `struct GPUBodyAABB` (16), `GPUPair` (8, **indices**), `GPUContact` (16) | Données plates GPU : AABB / paire / contact. |
| Interface | `class ICollisionBackend` | Base virtuelle pure du calcul (broadphase/narrowphase en masse). |
| Interface | `type` `isGPU` `name` | Identité du backend (`isGPU` non-pure : `type()!=None`). |
| Interface | `createBuffer` `destroyBuffer` `uploadBuffer` `downloadBuffer` | Gestion mémoire des buffers. |
| Interface | `gpuBroadphase` `gpuNarrowphase` | Calcul parallèle (renvoient des comptes). |
| Interface | `sync` `flush` | Synchronisation / soumission. |
| Interface | `maxBufferSize` `warpSize` `maxWorkgroupSize` `supportsAtomics` | Capacités matérielles. |
| Repli | `class CPUBackend : ICollisionBackend` | Repli public : broadphase O(n²), `std::malloc`. |
| Plateforme | `class CUDABackend` (`COL_ENABLE_CUDA`), `class OpenGLComputeBackend` (`COL_ENABLE_OPENGL`) | Backends GPU spécifiques (détail plateforme). |
| Fabrique | `BackendFactory::create(requested)`, `createBest()` | Choix automatique (Vulkan/DX/WebGPU → repli CPU). |

### `NkDebugDraw.h` — visualisation

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Couleur | `struct Color` : `r g b a`, ctor, `Red/Green/Blue/Yellow/White/Gray/Cyan/Orange/Magenta()` | RGBA 0-255 + fabriques. |
| Callbacks | `using LineCB` `PointCB` `TextCB` ; `setLineCallback` `setPointCallback` `setTextCallback` | Branche les primitives de rendu. |
| Flags | `drawAABBs` `drawShapes` `drawContacts` `drawNormals` `drawBVH` `drawBVHLeaves` `drawSleeping` `drawVelocities` `drawBodyIDs` `bvhDepthLimit` | Réglages publics (membres données). |
| Primitives | `line` `point` `text` `cross` `arrow` | Émetteurs de base (no-op sans callback). |
| Formes | `drawSphere` `drawAABB` `drawOBB` `drawCapsule` `drawShape` | Helpers de formes 3D. |
| Monde | `drawContact` `drawBVHNode` `drawBody` `drawWorld` | Helpers liés au monde (`drawBVHNode` placeholder, `drawWorld` contacts seulement). |
| 2D | `drawCircle2D` `drawAABB2D` | Helpers 2D (plan Z=0). |
| Stats | `drawStats(world, screenPos)` | Overlay « Bodies/Contacts/Frame ». |

---

## Référence complète

Chaque élément est repris ici en détail, avec son comportement et ses usages dans les différents
domaines. Les éléments triviaux sont brefs ; les pièces centrales (corps, monde, backend) sont
traitées à fond.

### `BodyFlags` et `CollisionBody` — l'unité collidable

`BodyFlags` est un `enum : uint32_t` combinable en bitmask. Chaque bit a un effet **observable** sur
la détection :

- `BodyFlag_Static` — le corps ne bouge jamais ; le filtre CPU **saute les paires static-static**
  (deux murs ne se testent pas entre eux). C'est l'optimisation la plus rentable d'une scène : la
  géométrie statique domine en nombre.
- `BodyFlag_Kinematic` — déplacé par l'utilisateur, pas par une physique (plateformes mobiles,
  ascenseurs, objets scriptés).
- `BodyFlag_Trigger` — pas de réponse au contact, seulement des **événements** : zones de dégât,
  checkpoints, déclencheurs de cinématique, capteurs d'IA.
- `BodyFlag_Sleeping` — corps au repos ; la broadphase **saute les paires endormies** pour
  économiser du calcul (un tas d'objets posés au sol).
- `BodyFlag_Is2D` — collision dans le plan XY (jeux 2D, UI spatiale, gameplay top-down).
- `BodyFlag_CCD` — détection continue (tunneling des objets rapides). Note : **aucun `isCCD()`** ne
  l'expose, on doit tester le bit soi-même.

`CollisionBody` agrège tout : `shape` (la géométrie), `transform` (position/orientation),
`velocity` (sert à étendre l'AABB dans le sens du mouvement — AABB prédictive), `flags`, les filtres
`layer`/`mask`, `userData` et `cachedAABB`. Selon le domaine :

- **Gameplay / IA** — `userData` pointe l'entité ; `layer`/`mask` séparent joueur, ennemis,
  projectiles, décor ; `Trigger` arme les zones.
- **Physique** — bien que NKCollision ne simule pas, c'est la couche de **détection** qu'un moteur
  physique consommerait : les manifolds remontés alimenteraient le solveur.
- **Rendu / éditeur** — `cachedAABB` sert à du culling grossier ou à l'affichage d'une boîte de
  sélection.
- **2D** — `BodyFlag_Is2D` confine les tests au plan, pour un moteur de plateforme ou de puzzle.

Les helpers `isStatic`/`isKinematic`/`isTrigger`/`isSleeping`/`is2D` sont de simples tests de bit.
`collidesWith(o)` est la règle de filtrage : **les deux** conditions `(layer & o.mask)` et
`(o.layer & mask)` doivent être vraies — un filtrage volontairement symétrique pour éviter les
asymétries (A heurte B mais pas l'inverse).

### `RaycastHit` — le résultat d'un rayon

Structure simple remplie par `raycast` : `bodyId` (le corps touché), `point` (l'impact), `normal`
(la normale à l'impact, par défaut « vers le haut »), `distance` (par défaut l'infini), et
`userData` (recopié depuis le corps, pour remonter à l'entité). Usages : tir d'arme, sélection à la
souris (picking éditeur), capteur de sol pour un personnage, ligne de vue d'une IA. **Rappel** :
tant que le test fin est un TODO, ces champs reflètent un impact **sur l'AABB**, pas sur la forme
réelle.

### `WorldConfig` — la configuration

Tous les choix structurants du monde. `gpuBackend` + `enableGPU` décident du calcul (`enableGPU =
false` force `None`/CPU). `broadphase` choisit la partition spatiale (par défaut un BVH dynamique).
`threadCount` (-1 = autant que de cœurs) + `multithreaded` règlent le parallélisme CPU. Les plafonds
`maxBodies` (65536), `maxPairs` (1M), `maxContacts` (256K) **dimensionnent les pools et buffers
préalloués** — à ajuster selon la scène. `gpuThreshold` (512) est le seuil de bascule GPU : en
dessous, le surcoût de transfert ne vaut pas le parallélisme. `persistContacts` + `contactBreakDist`
concernent la persistance des contacts (warm-starting), non exposée publiquement par ailleurs.
Domaines : un jeu 2D modeste met `enableGPU=false` et de petits plafonds ; une simulation à grande
échelle (foule, particules solides) relève `maxBodies` et baisse `gpuThreshold` pour profiter du GPU.

### `ObjectPool<T>` — le pool d'objets

Pool générique à **capacité fixe**, pensé pour l'allocation zéro en boucle chaude. Le ctor
`ObjectPool(capacity)` réserve `capacity` `T` dans un `std::vector` et remplit une free-list
d'indices (à l'envers) — `O(n)` une seule fois. Ensuite `alloc()` pioche un index libre et renvoie
`&storage_[idx]` (ou `nullptr` si plein), `free(ptr)` recalcule l'index par arithmétique de pointeur
et le réinjecte — tout en `O(1)`. `reset()` remet tout libre (`O(n)`), `capacity()`/`used()`
renseignent l'occupation. Usages : manifolds de contact (usage interne du monde), mais aussi tout
recyclage d'objets de taille fixe (nœuds, événements). **Piège** : `free` ne vérifie pas que le
pointeur vient du pool — un pointeur étranger corrompt la free-list.

### `CollisionWorld` — le monde

C'est la pièce maîtresse. Sa **création** (ctor RAII) choisit le backend via
`BackendFactory::create(enableGPU ? gpuBackend : None)`, calcule le nombre de threads, et — si le
backend est GPU — préalloue trois `GPUBuffer` (AABBs, paires, et contacts en lecture-arrière). Le
**destructeur** libère ces buffers. Pas de Create/Destroy manuel.

La **gestion des corps** tourne autour de l'id `uint64_t` auto-incrémenté :

- `addBody(b)` assigne un nouvel id (**écrase celui du corps**), calcule la `cachedAABB` via
  `shape.worldAABB(transform)`, copie dans la map, insère dans la broadphase, et **retourne l'id** —
  la valeur à conserver côté gameplay/ECS.
- `removeBody(id)` retire le corps de la broadphase, de la map, et des manifolds persistants.
- `setTransform(id, t)` met à jour la transform, recalcule l'AABB, met la broadphase à jour ;
  **no-op si l'id est inconnu**.
- `setVelocity(id, v)` ajuste la vitesse (AABB prédictive) ; no-op si inconnu.
- `getBody(id)` renvoie un `CollisionBody*` ou `nullptr` — **pointeur dans une `unordered_map`,
  invalidable** par un `addBody`/`removeBody` ultérieur : à ne pas conserver.

L'**update** `step(dt = 0.016f)` est le moteur de la frame. Il incrémente `frameStamp_`, choisit le
chemin GPU (`backend isGPU` *et* `bodyCount >= gpuThreshold`) ou CPU, fait la broadphase puis la
narrowphase, et tire les callbacks. Côté CPU, la narrowphase est mono-thread, ou **multi-thread** si
`threadCount_ > 1` et plus de 128 paires (les paires sont découpées en chunks, un `std::thread` par
chunk, join, fusion). Le filtrage CPU applique `collidesWith`, saute les paires de deux dormeurs et
les paires static-static. **Deux pièges** : `dt` est **ignoré** (`(void)dt`), et le multithreading
**recrée/joint des threads à chaque step** (pas de pool de threads réutilisé) — un coût non
négligeable. Selon le domaine, `step` est la frontière entre la détection et le reste : un système
ECS lirait les manifolds après `step` ; un moteur physique les passerait à son solveur ; le gameplay
réagit dans les callbacks.

Les **requêtes** complètent la boucle :

- `raycast(ray, hit&)` — broadphase `rayQuery` puis test **AABB uniquement** (le ray-vs-shape fin
  est un TODO). Picking, ligne de vue, tir grossier.
- `overlapAABB(aabb, results&)` — vide `results` puis y met tous les ids chevauchant la boîte.
  Sélection rectangle (éditeur), requête de zone, capteur d'IA.
- `testBodyBody(idA, idB, out&)` — test **direct** via `Narrowphase::generateContacts` ; `false` si
  un id est inconnu, sinon renvoie `out.hit`. Vérifier une paire précise hors boucle.

L'**état** se lit via `bodyCount()`, `contactCount()`, `frameStamp()` (compteur de frame),
`backend()` (accès au backend, non const) et `contacts()` (le `std::vector<ContactManifold>` de la
frame courante — c'est par là qu'un consommateur lit les résultats sans passer par les callbacks).
Détail logique des **callbacks** : `fireCallbacks()` route vers le **trigger** dès qu'au moins un des
deux corps est trigger (avec `entered` toujours `true`), sinon vers le **contact**.

### `GPUBackend`, `GPUBuffer`, `GPUBodyAABB`, `GPUPair`, `GPUContact` — la couche données GPU

`GPUBackend` énumère les cibles de calcul (`None` = CPU seul jusqu'à `Auto` = meilleur dispo).
`GPUBuffer` est le handle agnostique : `handle` (opaque : pointeur CUDA, `VkBuffer`…), `byteSize`,
`backend`, `mapped`, et `valid()` (handle non nul). Les trois structs alignées décrivent les
**données plates** que les kernels traitent en parallèle : `GPUBodyAABB` (min/max + `bodyId`, padding
pour l'alignement 16), `GPUPair` (deux `uint32_t` qui sont des **indices**, pas des ids — distinction
à retenir au moment de retrouver le corps), et `GPUContact` (positions A/B, normale, profondeur, et
les **ids** des deux corps). C'est le contrat entre le monde et n'importe quel backend GPU ; en GPU,
ces layouts évitent les indirections coûteuses.

### `ICollisionBackend` — l'abstraction de calcul

Interface virtuelle pure (destructeur virtuel) que le monde détient en `unique_ptr`. Son rôle :
exécuter broadphase et narrowphase **en masse** et gérer la mémoire des buffers, quel que soit le
matériel.

- `type()` / `name()` (purs) identifient le backend ; `isGPU()` est **non-pure** et renvoie par
  défaut `type() != None`.
- Mémoire : `createBuffer(byteSize, readback)` / `destroyBuffer` / `uploadBuffer` / `downloadBuffer`
  — le flag `readback` indique qu'on relira le buffer côté CPU (contacts).
- Calcul : `gpuBroadphase(aabbsIn, count, pairsOut, maxPairs)` teste les AABB en parallèle et
  **retourne le nombre de paires** ; `gpuNarrowphase(aabbsIn, pairsIn, pairCount, contactsOut,
  maxContacts)` retourne le **nombre de contacts**. Les buffers de sortie sont préalloués par
  l'appelant.
- Synchro : `sync()` attend la fin du travail GPU, `flush()` soumet les commandes en attente.
- Capacités : `maxBufferSize`, `warpSize` (32 NVIDIA / 64 AMD), `maxWorkgroupSize`,
  `supportsAtomics` — pour dimensionner les lancements de kernels.

Domaines : c'est le point d'extension **GPU/compute** du module — on pourrait y brancher un backend
NKRHI compute pour mutualiser avec le reste du moteur ; côté **threading**, c'est aussi l'endroit où
la parallélisation massive (data-parallel) remplace le multithreading CPU par chunks.

### `CPUBackend` — le repli public

Implémentation inline complète, **toujours disponible** et **publique** (à la différence des backends
GPU sous `#ifdef`). `type()` = `None`, `isGPU()` = `false`, `name()` = `"CPU"`. Sa broadphase est un
**brute-force O(n²)** AABB-AABB qui s'arrête à `maxPairs` ; sa narrowphase renvoie `0` (déléguée à la
narrowphase CPU du monde). `sync`/`flush` sont des no-op ; `maxBufferSize` = `SIZE_MAX`, `warpSize` =
1, `maxWorkgroupSize` = 1, `supportsAtomics` = `true`. **Le piège central** : ses buffers passent par
`std::malloc`/`std::free` — donc le **heap CRT**, incompatible avec l'allocateur NKMemory du moteur.
Ne jamais mélanger ces deux mémoires (risque de corruption de tas).

### `CUDABackend`, `OpenGLComputeBackend` — backends plateforme (détail)

Gardés respectivement par `COL_ENABLE_CUDA` et `COL_ENABLE_OPENGL`, ce sont des **implémentations
plateforme** à considérer comme du détail interne. `CUDABackend` (ctor `(deviceIndex)`) alloue via
`cudaMalloc`/`cudaMallocManaged` (managed si `readback`) et lance des kernels définis dans
`cuda_kernels.cu`. `OpenGLComputeBackend` utilise SSBO + atomic counter, kernels dans
`opengl_kernels.cpp`. On ne les instancie pas à la main : la fabrique s'en charge.

### `BackendFactory` — la fabrique

`create(requested)` réalise un switch : CUDA/OpenGL si compilés, `Auto` → `createBest()`, **défaut →
`CPUBackend`**. À retenir : `VulkanCompute`, `DirectX11`, `DirectX12`, `WebGPU` ne sont **pas gérés**
et retombent **silencieusement** sur le repli CPU — demander un de ces backends n'échoue pas, il est
juste ignoré. `createBest()` préfère CUDA (si un device est détecté), puis OpenGL, sinon CPU. C'est
le ctor du monde qui l'appelle ; on n'a normalement pas à l'invoquer directement.

### `Color` — la couleur de débogage

Structure RGBA simple (`uint8_t r, g, b, a`), ctor avec alpha par défaut 255, et un jeu de
**fabriques** de couleurs prédéfinies (`Red()`, `Green()`, `Blue()`, `Yellow()`, `White()`, `Gray()`,
`Cyan()`, `Orange()`, `Magenta()`). Strictement utilitaire pour `DebugDraw`.

### `DebugDraw` — le visualiseur

Le visualiseur **agnostique du rendu**. On lui branche trois primitives via callbacks (`LineCB`,
`PointCB`, `TextCB` ; setters par move) et tous ses helpers s'y ramènent ; sans callback, l'émetteur
est un no-op silencieux. Les **flags publics** sont des membres données qu'on modifie directement
(`drawAABBs`, `drawShapes`, `drawContacts`, `drawNormals`, `drawBVH`, `drawBVHLeaves`,
`drawSleeping`, `drawVelocities`, `drawBodyIDs`, `bvhDepthLimit`).

- **Primitives** — `line`, `point`, `text` ; `cross(p, size)` trace 3 segments (axes X/Y/Z) ;
  `arrow(from, dir, len)` une ligne + 2 segments de pointe (perpendiculaire via
  `dir.cross(Up)`, repli `Right`).
- **Formes** — `drawSphere` (3 cercles XY/XZ/YZ), `drawAABB` (12 arêtes), `drawOBB` (8 coins + 12
  arêtes), `drawCapsule` (cylindre via `Quat::fromAxisAngle` + 2 hémisphères), `drawShape` (switch
  sur le type de forme ; pour un ConvexHull, parcourt les faces et trace les arêtes ; par défaut,
  l'AABB monde).
- **Monde** — `drawContact(m)` dessine, par point du manifold, un point rouge en A, un bleu en B, une
  ligne jaune entre eux, et une flèche orange de la normale si `drawNormals` (no-op si `!m.hit`).
  `drawBody(body)` choisit la couleur selon trigger/static/sleeping/dynamique et dessine
  forme/AABB/vitesse/id selon les flags (saute un corps endormi si `!drawSleeping`).
- **Limites** — `drawBVHNode` est un **placeholder** (les internes du BVH ne sont pas accessibles) ;
  `drawWorld(world)` **ne dessine que les contacts** (`world.contacts()`) car les corps sont privés
  dans le monde — il faut appeler `drawBody`/`drawShape` soi-même.
- **2D** — `drawCircle2D(c, r)` et `drawAABB2D(aabb)` dessinent dans le plan Z=0 (débogage d'un
  monde 2D).
- **Stats** — `drawStats(world, screenPos)` affiche `"Bodies:.. Contacts:.. Frame:.."` via le text
  callback (lit `bodyCount`, `contactCount`, `frameStamp`).

Domaines : indispensable à l'**outillage / éditeur** (voir les boîtes, les contacts, les normales),
utile au **gameplay/IA** (visualiser un cône de vision via les flèches), et au **rendu/2D** pour
poser des repères visuels par-dessus la scène.

### `NKCollision.h` — le header parapluie

Il n'a **aucune déclaration propre** : il inclut les sous-headers. **Mais** ses includes utilisent
des noms/casse (`world.h`, `gpu_backend.h`, `debug_draw.h`, plus `math.h`/`shapes.h`/
`narrowphase.h`/`broadphase.h`/`persistent_contacts.h`/`ccd.h`) qui **ne correspondent pas** aux
fichiers réels (`NkWorld.h`, `NKGPUBackend.h`, `NkDebugDraw.h`) — d'où la non-compilabilité en
l'état. Son commentaire d'usage montre l'idiome cible :
`col::CollisionWorld world; auto id = world.addBody({ .shape = col::CollisionShape::makeSphere(...) }); world.step();`
(à noter : `CollisionShape::makeSphere` est référencé là mais **déclaré dans `shapes.h`**, hors de
cette page).

---

### Exemple récapitulatif

```cpp
#include "NkWorld.h"
#include "NkDebugDraw.h"
using namespace col;

// 1. Configurer puis créer le monde (RAII, pas de Create/Destroy).
WorldConfig cfg;
cfg.gpuBackend   = GPUBackend::Auto;
cfg.maxBodies    = 4096;
cfg.gpuThreshold = 512;          // CPU en dessous, GPU au-delà
CollisionWorld world(cfg);

// 2. Ajouter des corps — CONSERVER l'id retourné (celui du corps est écrasé).
CollisionBody ball;
ball.shape    = /* shapes.h */;
ball.flags    = BodyFlag_None;
ball.userData = myEntity;
uint64_t ballId = world.addBody(ball);

// 3. Brancher les callbacks AVANT le premier step.
world.setContactCallback([](const ContactManifold& m, const CollisionBody& a, const CollisionBody& b) {
    // remonter au gameplay via a.userData / b.userData
});
world.setTriggerCallback([](uint64_t a, uint64_t b, bool entered) {
    // entered toujours true : pas d'événement de sortie
});

// 4. Boucle : déplacer puis step (dt actuellement ignoré).
world.setTransform(ballId, newTransform);
world.step(dt);

// 5. Requêtes ponctuelles (raycast = AABB seulement pour l'instant).
RaycastHit hit;
if (world.raycast(myRay, hit)) { /* hit.bodyId, hit.point... */ }

// 6. Débogage : brancher un rendu, puis dessiner SES corps soi-même.
DebugDraw dbg;
dbg.setLineCallback([&](const Vec3& a, const Vec3& b, Color c){ myRenderer.DrawLine(a, b, c); });
dbg.drawAABBs   = true;
dbg.drawContacts = true;
dbg.drawWorld(world);                       // ne dessine QUE les contacts
if (auto* b = world.getBody(ballId)) dbg.drawBody(*b);  // les corps, à la main
```

---

[← Index NKCollision](README.md) · [Récap NKCollision](../NKCollision.md) · [Couche Runtime](../README.md)
