# NKCollision

> Couche **Runtime** · La détection de collision du moteur : monde de collision, formes
> géométriques, broad/narrow phase, contacts persistants, détection continue (CCD), debug draw
> et backend de calcul GPU.

NKCollision répond à une question simple mais omniprésente : **est-ce que ces deux objets se
touchent, et où ?** C'est la brique sur laquelle s'appuient la physique, le gameplay (triggers,
zones, ramassage d'objets), les requêtes de visée (raycast) et les outils de débogage visuel.
Le module organise le travail en deux temps — une **broad phase** rapide qui élimine la grande
majorité des paires impossibles, puis une **narrow phase** précise qui calcule les vrais
contacts — et ajoute par-dessus le **warm-starting** (contacts persistants frame-à-frame) et le
**CCD** anti-tunneling pour les objets rapides.

> **État du module — à lire avant usage.** NKCollision est un chantier **greenfield non
> intégré** (cohérent avec la ROADMAP « non démarré »). Trois écarts majeurs avec les
> conventions du moteur :
> - **Namespace réel `col`** (et non `nkentseu::collision`), types en `PascalCase` simple,
>   méthodes en `camelCase`, membres privés suffixés `_`, constantes `kXxx`. Aucun préfixe
>   `Nk`/`NKI`.
> - **Non zéro-STL** : `std::vector`, `std::unordered_map`, `std::function`, `std::shared_ptr`,
>   `std::thread`, `std::malloc/free`… partout. `CPUBackend` alloue via le heap CRT
>   (`std::malloc`), incompatible avec l'allocateur NKMemory → risque de heap corruption si
>   mélangé.
> - **Header parapluie cassé** : `NKCollision.h` inclut des chemins en minuscules
>   (`math.h`, `shapes.h`, `world.h`…) qui **ne correspondent pas** aux fichiers physiques
>   réels (`NkShapes.h`, `NkWorld.h`…). De plus `NkMath.h` est **entièrement commenté** : les
>   types `col::Vec2/Vec3/Mat3/Quat/Transform/Ray/Interval` et les constantes
>   `kEpsilon/kPi/kInfinity` dont tout le module dépend ne sont **pas réellement déclarés**. En
>   l'état, le module **ne compile pas tel quel**.

- **Namespace** : `col`
- **Header parapluie** : `#include "NKCollision/NKCollision.h"` (voir avertissement ci-dessus)

---

## Par où commencer

Selon ce que vous cherchez à faire :

| Besoin | Partie |
|--------|--------|
| Créer un monde, ajouter des corps, lancer le `step`, faire des requêtes (raycast, overlap) | [Le monde](NKCollision/World.md) |
| Brancher un backend de calcul GPU (CUDA/OpenGL) ou rester sur CPU | [Le monde](NKCollision/World.md) |
| Dessiner les AABB, formes, contacts et normales pour déboguer | [Le monde](NKCollision/World.md) |
| Décrire une forme : sphère, box, capsule, convex hull, terrain… | [Les formes](NKCollision/Shapes.md) |
| Utilitaires géométriques : `support` (GJK), `project` (SAT), `toAABB`, raycast slab | [Les formes](NKCollision/Shapes.md) |
| Comprendre la broad phase (BVH, Sweep-and-Prune) | [La détection](NKCollision/Detection.md) |
| Comprendre la narrow phase (GJK/EPA/SAT, dispatch par paire) | [La détection](NKCollision/Detection.md) |
| Réutiliser les contacts d'une frame à l'autre (warm-starting) | [La détection](NKCollision/Detection.md) |
| Empêcher le tunneling des objets rapides (CCD / TOI) | [La détection](NKCollision/Detection.md) |

Chaque page décrit l'**API réelle telle que déclarée** dans les headers, avec ses pièges
connus (fonctions seulement déclarées, dispatch ordre-dépendant, stubs non implémentés).

---

## Aperçu des familles

- **Monde de collision** (`NkWorld.h`) — `CollisionWorld` est la façade : on lui ajoute des
  `CollisionBody` (forme + transform + flags + couches/masques), on appelle `step()` par frame,
  on branche des callbacks de contact/trigger, et on fait des requêtes (`raycast`,
  `overlapAABB`, `testBodyBody`). `WorldConfig` règle broadphase, threads, seuil GPU et budget
  mémoire. `ObjectPool<T>` est le pool zéro-allocation interne.
- **Backend GPU** (`NKGPUBackend.h`) — `ICollisionBackend` abstrait le calcul ; `CPUBackend`
  (fallback brute-force O(n²), toujours disponible) et des backends GPU optionnels
  (`CUDABackend`, `OpenGLComputeBackend`, compilés sous garde). `BackendFactory` auto-sélectionne
  le meilleur. Structures de buffers plats (`GPUBuffer`, `GPUBodyAABB`, `GPUPair`, `GPUContact`).
- **Debug draw** (`NkDebugDraw.h`) — `DebugDraw` est agnostique du moteur de rendu : on branche
  3 callbacks (ligne / point / texte), on règle des flags (`drawAABBs`, `drawContacts`…) et on
  appelle des helpers (`drawSphere`, `drawAABB`, `drawCapsule`, `drawContact`, `drawBody`…).
  `Color` est un RGBA 8 bits avec fabriques (`Red()`, `Green()`…).
- **Formes** (`NkShapes.h`) — primitives 2D/3D : `Sphere`/`Circle2D`, `AABB2D/3D`, `OBB2D/3D`,
  `Capsule2D/3D`, `ConvexHull3D`, `ConvexPolygon2D`, `Triangle`/`TriangleMesh`, `Heightfield`,
  toutes équipées de `support` (GJK) et `project`/`toAABB` (SAT/broadphase). `CollisionShape`
  est le wrapper tag-union (sans vtable) avec fabriques `makeSphere`, `makeCapsule3D`…
- **Math de collision** (`NkMath.h`) — header de spécification **intégralement commenté** :
  `Vec2/3/4`, `Mat3/4`, `Quat`, `Transform`, `Ray`, `Interval`, constantes — prévus mais **non
  livrés** (à activer).
- **Broad phase** (`NkBroadPhase.h`) — `DynamicBVH` (DBVT auto-équilibré, fat AABB prédictives,
  SAH), `SweepAndPrune` (tri 1D), façade `Broadphase` (`BroadphaseType::DynamicBVH/SAP/Grid`).
  Sortie : `CollidingPair`.
- **Narrow phase** (`NkNarrowPhase.h`) — `GJK3D`/`EPA3D` (test + profondeur de pénétration),
  `SAT3D`/`SAT2D` (fast-paths boîtes/sphères), dispatcher `Narrowphase`. Sorties :
  `ContactPoint`, `ContactManifold` (≤4 points), `ContactManifold2D` (≤2 points).
- **Contacts persistants** (`NkPersistentContacts.h`) — `ContactCache` + `PersistentManifold`
  pour le warm-starting du solveur (réutilisation des impulsions entre frames), purge des
  contacts dérivés, éviction par âge.
- **CCD** (`CCD.h`) — détection continue anti-tunneling : `SweptSphere` (analytique),
  `LinearCast` (shape-cast GJK par dichotomie), `TOISolver`/`CCDManager` (résolution du temps
  d'impact le plus précoce).

---

## Index des headers

| Header | Contenu | Documenté dans |
|--------|---------|----------------|
| `NKCollision.h` | Parapluie (includes cassés — voir avertissement). | — |
| `NkWorld.h` | `CollisionWorld`, `CollisionBody`, `WorldConfig`, `BodyFlags`, `RaycastHit`, `ObjectPool<T>`. | [Le monde](NKCollision/World.md) |
| `NKGPUBackend.h` | `ICollisionBackend`, `CPUBackend`, `BackendFactory`, `GPUBackend`, `GPUBuffer`/`GPUPair`/`GPUContact`. | [Le monde](NKCollision/World.md) |
| `NkDebugDraw.h` | `DebugDraw`, `Color`. | [Le monde](NKCollision/World.md) |
| `NkShapes.h` | `CollisionShape`, `ShapeType`, primitives 2D/3D, `ConvexHull3D`, `TriangleMesh`, `Heightfield`. | [Les formes](NKCollision/Shapes.md) |
| `NkMath.h` | Types math attendus (Vec/Mat/Quat/Transform/Ray/Interval) — **entièrement commenté**. | [Les formes](NKCollision/Shapes.md) |
| `NkBroadPhase.h` | `DynamicBVH`, `SweepAndPrune`, `Broadphase`, `BroadphaseType`, `CollidingPair`. | [La détection](NKCollision/Detection.md) |
| `NkNarrowPhase.h` | `GJK3D`, `EPA3D`, `SAT3D`/`SAT2D`, `Narrowphase`, `ContactManifold`/`ContactPoint`. | [La détection](NKCollision/Detection.md) |
| `NkPersistentContacts.h` | `ContactCache`, `PersistentManifold`, `PersistedContact`, `makePairKey`. | [La détection](NKCollision/Detection.md) |
| `CCD.h` | `SweptSphere`, `LinearCast`, `TOISolver`, `CCDManager`, `TOIResult`/`CCDPair`. | [La détection](NKCollision/Detection.md) |

---

[← Couche Runtime](README.md) · [Index du wiki](../README.md)
