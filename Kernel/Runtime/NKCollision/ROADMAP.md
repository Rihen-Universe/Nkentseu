# NKCollision — Roadmap

État actuel (mai 2026) : module **non intégré au framework Nkentseu**. Le dossier ne contient qu'un prototype tiers `collision_lib/` (namespace `col::`, en-têtes C++17 STL, CMake autonome, archive `collision_lib_complete.tar.gz`) qui n'utilise aucune convention Nkentseu (`Nk*` types, `NKContainers`, `NKMath`, `NkAllocator`, `NkLog`). Les fonctionnalités prévues (AABB/OBB/Sphere/Capsule, raycast, broadphase DBVH, narrowphase GJK/EPA, CCD, contacts persistants, backends GPU) sont **présentes en code prototype** mais doivent être ré-écrites/portées au style Nkentseu pour devenir le module `NKCollision` officiel mentionné par ARCHITECTURE.md §2.4 (`NKPhysics: Rigidbody, collision AABB/OBB/sphere/capsule, raycast`).

---

## Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| Module Nkentseu (`src/`, `include/`, `pch/`, `<Name>.jenga`) | Manquant | S | P0 |
| Prototype tiers `collision_lib/` (référence) | Livré | — | — |
| Types math Nkentseu-aligned (Vec2/3, AABB, OBB, Mat3/4) | TODO | S | P0 |
| Shapes 2D : Circle, AABB, OBB, Capsule, Polygon | TODO | M | P0 |
| Shapes 3D : Sphere, AABB, OBB, Capsule, ConvexHull, TriangleMesh, Heightfield, Compound | TODO | L | P0 |
| Raycast (Vec3 origine + direction + tMax) | TODO | M | P0 |
| Broadphase : Sweep & Prune (SAP) | TODO | M | P1 |
| Broadphase : Dynamic BVH (DBVT Bullet-style) | TODO | L | P1 |
| Broadphase : Uniform Grid / Spatial hash | TODO | M | P2 |
| Narrowphase analytique (Sphere-Sphere, Sphere-AABB...) | TODO | M | P1 |
| Narrowphase GJK (3D) + EPA | TODO | L | P1 |
| Continuous Collision Detection (Swept sphere, TOI bisection) | TODO | L | P2 |
| Persistent contacts (manifold cache, warm-starting) | TODO | M | P2 |
| Layer mask / filtering | TODO | S | P1 |
| Triggers / overlap events | TODO | S | P1 |
| Backend GPU compute (OpenGL/Vulkan/DX11/DX12/CUDA) | TODO | XL | P3 |
| Debug draw (gizmos via NKUI) | TODO | S | P1 |
| Tests unitaires | TODO | M | P1 |
| Benchmarks vs Bullet/PhysX | TODO | M | P2 |
| Intégration ECS (composants `NkColliderBody`, système) | TODO | M | P1 |
| Doc API + exemples Nkentseu | TODO | S | P1 |

Légende : Livré · Partiel · En cours · TODO · Abandonné

---

## Livré

### Prototype tiers `collision_lib/`
Conservé comme **référence d'implémentation** — à porter, pas à intégrer tel quel.

- `include/collision/math.h` : Vec2/Vec3, Quat, Mat4, AABB3D, Transform (STL-based).
- `include/collision/shapes.h` : enum `ShapeType` (Circle2D, AABB2D, OBB2D, Capsule2D, Polygon2D, Sphere, AABB3D, OBB3D, Capsule3D, ConvexHull, TriangleMesh, Heightfield, Compound), `CollisionShape` union-like, factories `makeSphere / makeAABB / makeOBB / makeCapsule / makeConvexHull`, intersections/contains/overlaps/expand/fatten 2D et 3D.
- `include/collision/broadphase.h` : `DynamicBVH` self-balancing (kFattenFactor=0.1, O(log n) insert/remove, O(n) query), `CollidingPair`, pair management.
- `include/collision/narrowphase.h` : `ContactPoint` / `ContactManifold` (max 4 pts 3D, 2 pts 2D), GJK simplex, EPA déclaré.
- `include/collision/world.h` : `CollisionWorld`, `CollisionBody` (id, shape, transform, velocity, flags, layer, mask, userData, cachedAABB), `BodyFlags` (Static, Kinematic, Trigger, Sleeping, Is2D, CCD), filtering par layer/mask.
- `include/collision/persistent_contacts.h` : cache de manifolds entre frames.
- `include/collision/ccd.h` : `TOIResult`, `SweptSphere::sphereVsSphere`, GJK-based shape cast, TOI bisection.
- `include/collision/gpu_backend.h` : interface abstraite + `src/gpu/opengl_backend.h`, `src/gpu/vulkan_backend.h`, `src/gpu/dx12_backend.h`, `src/gpu/dx12_backend_impl.h`.
- `include/collision/debug_draw.h` : helpers de visualisation (lignes, sphères wireframe).
- `src/shapes.cpp` : implémentations non-inline.
- `tests/` : `test_main.cpp`, `test_suite.cpp`, `unit_tests.cpp`, `benchmarks.cpp`.
- `examples/example_usage.cpp` : démo `CollisionWorld` minimale.
- `CMakeLists.txt` : options `COL_ENABLE_CUDA / OPENGL / VULKAN / DX11 / DX12 / WEBGPU`, SIMD `SSE4.2 / AVX2`.

---

## En cours / TODO immédiat

### Phase 0 — Bootstrapping module (P0)
- Créer la structure standard Nkentseu : `NKCollision/src/NKCollision/`, `NKCollision/pch/`, `NKCollision.jenga`, header umbrella `NKCollision/NKCollision.h`.
- Adopter les conventions : namespace `nkentseu::collision`, types `NkVec3` / `NkMat4` venant de `NKMath`, conteneurs `NkVector` / `NkHashMap` (`NKContainers`), allocateurs `NkAllocator`, logging `NkLog`, assertions `NK_ASSERT`.
- Supprimer la dépendance brute à `std::vector`, `std::function`, `std::unordered_map`, `std::array`, `std::memory`, `std::thread`, `std::mutex` sur la surface publique (politique cohérente avec NKECS / NKUI).
- Décider si on garde `col::` interne et on expose un wrapper `Nk*`, ou si on réécrit en `nkentseu::collision::*`. Recommandation : réécriture progressive fichier par fichier en gardant l'algorithme.

### Phase 1 — Géométrie de base (P0)
- Shapes 2D : `NkCircle2D`, `NkAABB2D`, `NkOBB2D`, `NkCapsule2D`, `NkPolygon2D` (convexe, ≤16 sommets).
- Shapes 3D : `NkSphere`, `NkAABB3D`, `NkOBB3D`, `NkCapsule3D`, `NkConvexHull` (≤64 sommets, Quickhull pour la construction).
- Méthodes communes : `Contains(point)`, `Overlaps(other)`, `BoundingAABB()`, `Transform(NkTransform)`, `Area / Volume`, `ClosestPoint(p)`.
- `NkTriangleMesh` : indexation triangles, BVH précompilée pour les meshes statiques.
- `NkHeightfield` : grille 2D + samples Y, raycast et sphere-overlap optimisés.
- `NkCompound` : tableau de shapes enfants avec transforms locaux.

### Phase 2 — Raycast & queries (P0-P1)
- `NkRaycastHit { Vec3 point, Vec3 normal, float t, NkColliderId body, NkShapeIndex shape, uint32 face }`.
- `Raycast(origin, dir, tMax, layerMask)` retournant le premier ou tous les hits.
- `SphereCast` / `BoxCast` / `CapsuleCast` (shape cast = raycast épaissi).
- `OverlapSphere` / `OverlapBox` / `OverlapCapsule` (queries non-cast).
- Sorting par `t` croissant, filtrage callback `bool(NkColliderId)`.

### Phase 3 — Broadphase (P1)
- `NkBroadphaseSAP` : Sweep & Prune incrémental sur 3 axes, idéal pour des mondes avec peu de mouvement.
- `NkBroadphaseDBVH` : Dynamic BVH self-balancing avec fat AABB (porter `DynamicBVH` de `collision_lib`).
- `NkBroadphaseGrid` : grille uniforme spatiale (hash 3D), pour les mondes denses uniformes.
- Interface commune `INkBroadphase` : `Insert/Update/Remove/Query(AABB) -> [ColliderId]`.
- Sélection auto / configurable selon la densité.

### Phase 4 — Narrowphase (P1)
- Tests analytiques rapides pour les paires simples (Sphere-Sphere, Sphere-AABB, Sphere-Capsule, AABB-AABB, Capsule-Capsule, OBB-OBB via SAT).
- `NkGJK` 3D pour les convexes arbitraires.
- `NkEPA` 3D pour la profondeur de pénétration et la normale après GJK positif.
- `NkSAT` 2D et 3D pour les polygones convexes (axe séparateur).
- `NkContactPoint { Vec3 posA, Vec3 posB, Vec3 normal, float penetration, float restitution, float friction }`.
- `NkContactManifold` jusqu'à 4 points 3D (porté) ou 2 points 2D.
- Génération de manifold par clipping (Sutherland-Hodgman pour les contacts surface-surface).

### Phase 5 — Continuous Collision (P2)
- Swept sphere (`SphereVsSphere`, `SphereVsAABB`, `SphereVsTriangleMesh`) — O(1).
- TOI bisection générique entre deux shapes via GJK conservativement avancé.
- Flag `NkBodyFlag_CCD` activable par body, applique CCD seulement pour les paires Static<->CCDDynamic.

### Phase 6 — Persistent contacts (P2)
- Cache des manifolds inter-frame (clé = paire ordonnée bodyA/bodyB).
- Warm-starting : conservation des indices de feature, des impulses applicables (utile pour NKPhysics au-dessus).
- Invalidation après séparation > tolérance.

### Phase 7 — World, layers, triggers (P1)
- `NkCollisionWorld` : registre des bodies, broadphase, narrowphase, callbacks `OnEnter / OnStay / OnExit`.
- Layer/Mask 32 bits (`body.layer & other.mask` symétrique).
- Trigger bodies (no resolve, juste événements).
- Sleeping bodies (skip broadphase update si AABB stable).
- API ECS : composant `NkColliderComponent { ShapeHandle shape, layer, mask, flags, offset }` + système `NkCollisionSystem` qui pousse les changements vers le world.

### Phase 8 — Debug & tooling (P1)
- Debug draw via `NKUI` / `NKRenderer` debug pass : AABB, OBB, sphères wireframe, contacts (point + normal), broadphase tree.
- Statistiques : nb collider, nb pairs, temps broadphase/narrowphase/CCD par frame.
- Visualiseur DBVH (export DOT).

---

## À venir / À ajouter (futur proche)

### Backends GPU
- Décider de la stratégie : GPU broadphase utile au-delà de ~10k bodies, sinon overkill.
- Port progressif des backends prototype : `OpenGLBackend`, `VulkanBackend`, `DX12Backend`, `CUDABackend`, `WebGPUBackend`.
- Compute shaders : building d'AABB world, broadphase grid+pairs, narrowphase analytique batched (sphere/aabb).
- Interop avec NKRenderer (partage des buffers GPU pour le rendu debug).

### Optimisations
- SIMD : SSE4.2 / AVX2 (`Vec3 * 4` packé) pour les batches Sphere-Sphere, Sphere-AABB.
- Parallélisation broadphase (multi-thread) via `NKThreading` (pas `std::thread` directement).
- Allocateurs frame-temp pour les paires (linéaire, reset/frame).
- Cache-friendly memory layout : bodies en SoA, regroupement par layer.

### Algorithmes avancés
- MPR (Minkowski Portal Refinement) en alternative à GJK/EPA (plus stable pour les pénétrations profondes).
- V-Clip pour les polyèdres (manifold persistant naturel).
- Détection multi-contact pour Box-Box (face/face, edge/edge, vertex/face).
- TOI Conservative Advancement (CCD plus rapide qu'EPA-bisection).

### Tooling éditeur
- Collider gizmos dans `NkUIViewport3D` (édition AABB/OBB/Sphere/Capsule par drag des handles).
- Auto-collider depuis mesh (decomposition convexe via V-HACD ou approx).
- Profiler intégré : flame graph broadphase / narrowphase / CCD.

### Intégration moteur
- Composant `NkColliderComponent` + composant `NkRaycastTarget`.
- Trigger zones avec callbacks Blueprint (`NkBlueprint` côté Noge).
- Couplage avec `NkPhysics` (au-dessus) : `NkCollision` produit les contacts, `NkPhysics` résout les impulses.
- Hooks Lua / Python pour scripter `OnCollisionEnter / Stay / Exit`.

### Réseau
- Sérialisation deterministe du résultat de collision (utile pour la lockstep / rollback netcode).
- Hash de l'état des collisions pour détection de désync.

### Tests et qualité
- Suite de tests unitaires : générer 1000 paires aléatoires par shape combo, vérifier symétrie `A vs B == B vs A`, normales bien orientées, depth positif quand overlap.
- Régression visuelle : screenshots debug draw vs golden image.
- Benchmark vs Bullet, PhysX (1k / 10k / 100k bodies, mix static/dynamic).

---

## Bugs / quirks connus
- Le module n'est **pas démarré** côté Nkentseu — toute la suite "TODO immédiat" est greenfield.
- Le prototype `collision_lib/` utilise abondamment la STL et un namespace `col::` distinct — il **ne doit pas** être linké tel quel dans le moteur (pollution d'API, doublon de types math).
- L'archive `collision_lib_complete.tar.gz` est commitée — décider si on la garde comme snapshot historique ou si on la déplace dans `docs/`.
- `CMakeLists.txt` du prototype force `/arch:AVX2` et `-O3 -ffast-math` sans détecter le CPU runtime — à harmoniser avec les flags globaux du moteur (`Nkentseu.jenga`).

---

## Dépendances
- **Couches en dessous (utilisées)** (cibles, pas encore câblées) :
  - `NKCore` (types entiers, assertions)
  - `NKMath` (Vec2/3/4, Quat, Mat3/4, NkTransform)
  - `NKContainers` (NkVector, NkHashMap, NkFreeList)
  - `NKLogger` (debug, warnings, stats)
  - `NKThreading` (parallélisme broadphase / narrowphase)
  - `NKRenderer` (debug draw — optionnel, derrière une option)
  - `NKUI` (gizmos d'édition de colliders dans l'éditeur — optionnel)
- **Modules au-dessus qui en dépendront** :
  - `Engine/Noge/.../Components/Physics/NkPhysics.h` et `NkPhysicsComponents.h` (déjà déclarés, attendent NKCollision)
  - `NKPhysics` (à créer ou existant ?) — résolution d'impulses, contraintes, joints au-dessus des contacts produits par NKCollision
  - `Engine/Noge/.../ECS/Systems/NkRaycastSystem` (futur)
  - Applications : `Unkeny` (gizmos collider), `Sandbox`, `PV3DE` (si physique de salle d'opération)
- **Relation à NKPhysics** : ARCHITECTURE.md §2.4 mentionne `NKPhysics: Rigidbody, collision AABB/OBB/sphere/capsule, raycast`. La décomposition recommandée est `NKCollision` (géométrie + détection seule) + `NKPhysics` (dynamique + résolution + contraintes) au-dessus. Cette séparation est saine et alignée avec PhysX (`PxScene` / `PxRigidActor`) et Bullet (`btCollisionWorld` / `btDynamicsWorld`).
