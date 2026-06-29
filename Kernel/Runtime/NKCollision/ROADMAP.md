# NKCollision — Roadmap

État actuel (**2026-06-29**) : **module zéro-STL qui COMPILE et passe son self-test (47/47)**. Phases 0/1/2/4/7 livrées + **narrowphase générique GJK/EPA 2D+3D** (vague 2). Sur **NKMath** (zéro math maison), conteneurs **NKContainers** (`NkVector`), `NKLogger` (pas de printf), zéro `std::`. Le prototype tiers STL (`col::`) a été **remplacé**. Objectif final = module `NKCollision` officiel d'ARCHITECTURE.md §2.4.

### Livré 2026-06-29 — vague 2 : GJK/EPA générique + taxonomie « type PhysX »
- **Taxonomie de formes étendue** (`NkColTypes.h` `NkShapeType`) : 2D = Point, Segment, Cercle, Capsule, Triangle, Box/OBB, Polygone (+ Chain concave) ; 3D = Sphère, Capsule, Triangle, Box, **Cylindre**, **Cône**, **Convex hull**, **Plan/half-space** (+ Heightfield, Trimesh, Compound concaves). Helpers `NkShapeIs2D/IsConvex/IsConcave`. ✅
- **NkShape étendu** (`NkColShapes.h`) : sommets non-ownants (triangle/polygone/convexe), params cylindre/cône/plan, fabriques dédiées + AABB exacte de chaque type. ✅
- **GJK + EPA génériques** (`NkColGJK.h`, **le cœur « puissant »**) : modèle **cœur + marge** (Bullet-like) — GJK-distance sur les cœurs (formes arrondies gérées par la marge analytiquement), GJK booléen + EPA pour la pénétration profonde. Fonctions de support pour TOUTE la famille convexe 2D+3D → **ajouter un type convexe = écrire son support**. Sortie : normale A→B + profondeur + 1 point de contact. ✅
- **Plan / half-space** : test analytique dédié (infini, hors GJK) vs toute forme convexe. ✅
- **Dispatch** (`NkCollisionWorld.cpp`) : fast-paths analytiques conservés (sphère/box/capsule) + **fallback GJK/EPA** pour toute paire convexe non couverte (box-capsule, cône, cylindre, convexe, triangle…) + plan. ✅
- **Self-test** : **47 assertions, 0 échec** (cohérence GJK vs analytique sphère/box, box-capsule, cylindre, cône, convexe-tétraèdre, triangle, plan, polygones 2D, intégration world). ✅
- **Reste (concaves)** : Chain2D / Heightfield / Trimesh / Compound = enum + dispatch prêts, **décomposition à implémenter (vague 3)**.

---

### Vague 1 (2026-06-29) — bootstrap + analytique

### Livré 2026-06-29 (commit côté worktree `Nkentseu-anima`)
- **Phase 0** : structure `src/NKCollision/`, jenga, umbrella `NKCollision.h`, enregistrement workspace. ✅
- **Types** (`NkColTypes.h`) : `NkAABB2D/3D` (overlap/contains/merge/grow), `NkManifold2D/3D`, `NkRay2D/3D`, `NkRayHit2D/3D` — sur NKMath. ✅
- **Formes** (`NkColShapes.h`) : `NkShape` (cercle/sphère, boîte, capsule 2D+3D) + `NkComputeAABB2D/3D`. ✅ (OBB plein/polygone/convexe = à venir)
- **Narrowphase analytique** (`NkColTests.h`) : sphère-sphère, sphère-boîte, boîte-boîte, sphère-capsule, capsule-capsule (3D) ; cercle-cercle, cercle-boîte, boîte-boîte (2D) → manifolds (normale A→B + depth). ✅
- **Raycast** : sphère/cercle (analytique), AABB (slab). ✅
- **World** (`NkCollisionWorld.*`) : corps + broadphase O(n²) (AABB) + narrowphase dispatch + **layer/mask** + raycast monde. ✅ (SAP/DBVH/grille = Phase 3)
- **OBB 2D** (`NkColSAT.h`) : boîtes ORIENTÉES via SAT — OBB-OBB (4 axes) + OBB-cercle
  (repère local). Câblé dans le narrowphase : la rotation des boîtes 2D est désormais
  prise en compte (avant : traitées AABB). ✅
- **Self-test** (`tests/test_collision.cpp`, **NKLogger** pas printf) : **26 assertions, 0 échec**. ✅

### Livré 2026-06-29 — vague 3 (concaves) + vague 4 (OBB3D + SAP) — self-test 63/63
- **Vague 3** : **Trimesh3D** (sommets+indices) / **Heightfield3D** (grille) / **Chain2D**
  (polyligne) décomposés en triangles/segments via GJK/EPA + cull AABB (`NkColConcave.h`) ;
  **Compound** (sous-formes + offset) ; **événements** `EnterEvents/StayEvents/ExitEvents`
  (suivi frame-à-frame) + flag `trigger`/`SetTrigger`. ✅
- **Vague 4** : **OBB 3D orientée** (`NkShape::orientation` quaternion + `OBB3D(...)`,
  support GJK & AABB orientés, fast-paths analytiques gardés sur boîtes alignées via
  `NkBoxAligned`) ; **broadphase Sweep-and-Prune** (tri AABB par min.x quicksort + balayage,
  remplace le O(n²) de `Step()`). ✅

### Livré 2026-06-29 — vague 5 : requêtes de scène — self-test 70/70
- **Raycast exact par forme** : OBB 3D (slab en repère local), plan, triangle
  (Möller–Trumbore), trimesh (triangle le plus proche) ; câblés dans `Raycast3D`
  (capsule/cylindre/cône/convexe restent en AABB approx). ✅
- **Overlap query** : `NkWorld::Overlap(shape, out, mask)` — tous les corps chevauchant
  une forme arbitraire (broadphase AABB + narrowphase). ✅

### Livré 2026-06-29 — vague 6 : casts génériques (GJK / conservative advancement) — 76/76
- **GJK ray-cast / conservative advancement** (`NkColCast.h`) : `NkConvexCast3D` (TOI d'une
  forme convexe translatée vers une autre) via fonctions de support + GJK-distance avec offset. ✅
- **Raycast exact** capsule/cylindre/cône/convexe (`NkRayConvex3D` = point casté) — câblé dans `Raycast3D`. ✅
- **Shape cast** : `NkWorld::ShapeCast(shape, dir, maxDist, hit, mask)` (Sphere/Box/CapsuleCast) avec
  cull par AABB balayée. ✅

### Livré 2026-06-29 — vague 7 : Dynamic AABB Tree (DBVH) — self-test 83/83
- **`NkDbvh`** (`NkDbvh.h`) : arbre binaire d'AABB index-based (zéro pointeur), free-list,
  AABB grossies (marge), insertion par coût SAH. `Insert/Remove/Update(proxy)` incrémentaux,
  `Query(AABB)` et `RayCast` en O(log n). Vérifié vs brute force (grille 3×3 : query/raycast/
  remove/update). **Équilibrage par rotations DIFFÉRÉ** (no-op : arbre toujours correct, SAH
  garde une profondeur raisonnable). ✅

### Livré 2026-06-29 — vague 8 : DBVH branché comme broadphase du world — 83/83
- **`NkWorld` utilise le `NkDbvh`** : `AddBody`/`RemoveBody`/`SetShape` maintiennent l'arbre
  (Insert/Remove/Update incrémentaux) ; `Step()` trouve les paires par `Query(AABB)` du DBVH
  (dédup par id) au lieu du SAP O(n²)/sweep. Aucune régression (83 tests verts). ✅

### Livré 2026-06-29 — vague 9 : équilibrage DBVH (AVL) + queries world via l'arbre — 87/87
- **Équilibrage par rotations** (algorithme b2DynamicTree) : hauteur bornée même en insertion
  triée (64 inserts -> hauteur <= 12 au lieu de 63). ✅
- **Queries world accélérées** : `Raycast3D` (préfiltre `mTree.RayCast`) et `Overlap`
  (`mTree.Query`) passent par le DBVH au lieu d'itérer tous les corps. ✅

### Reste prioritaire (vague 10)
- **Manifold multi-points** (clipping Sutherland-Hodgman) : 1 pt -> 2 (2D)/4 (3D) pour la
  stabilité de la résolution physique (LE chantier majeur restant pour NKPhysics au-dessus).
- **CCD** intégré au world (swept des corps rapides via ShapeCast) + **contacts persistants**.
- **Debug draw** (NKUI/NKRenderer) + **intégration ECS** (`NkColliderComponent` + système).

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
  - Applications : `Noge` (gizmos collider), `Sandbox`, `PV3DE` (si physique de salle d'opération)
- **Relation à NKPhysics** : ARCHITECTURE.md §2.4 mentionne `NKPhysics: Rigidbody, collision AABB/OBB/sphere/capsule, raycast`. La décomposition recommandée est `NKCollision` (géométrie + détection seule) + `NKPhysics` (dynamique + résolution + contraintes) au-dessus. Cette séparation est saine et alignée avec PhysX (`PxScene` / `PxRigidActor`) et Bullet (`btCollisionWorld` / `btDynamicsWorld`).
