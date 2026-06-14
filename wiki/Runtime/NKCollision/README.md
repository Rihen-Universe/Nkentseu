# NKCollision — documentation détaillée

Le module **NKCollision**, partie par partie. Pour une vue d'ensemble et un guide « par où
commencer », voir le récap : [../NKCollision.md](../NKCollision.md).

> Rappel d'état : module **greenfield non intégré** — namespace réel `col` (pas
> `nkentseu::collision`), non zéro-STL, header parapluie aux includes cassés et `NkMath.h`
> entièrement commenté (types math non livrés). Les pages décrivent l'**API réelle telle que
> déclarée**, avec ses pièges (fonctions seulement déclarées, dispatch ordre-dépendant, stubs).

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [World.md](World.md) | Monde de collision : ajouter des `CollisionBody`, `step()`, callbacks contact/trigger, requêtes (`raycast`/`overlapAABB`/`testBodyBody`), `WorldConfig`/`ObjectPool`. Backend de calcul GPU (`ICollisionBackend`/`CPUBackend`/`BackendFactory`). Debug draw enfichable (`DebugDraw`/`Color`). | `NkWorld.h`, `NKGPUBackend.h`, `NkDebugDraw.h`, `NKCollision.h` |
| [Shapes.md](Shapes.md) | Formes de collision : `Sphere`/`Circle2D`, `AABB`/`OBB` 2D/3D, `Capsule`, `ConvexHull3D`, `ConvexPolygon2D`, `Triangle`/`TriangleMesh`, `Heightfield`, wrapper `CollisionShape` (tag-union + fabriques). Utilitaires `support` (GJK), `project`/`toAABB` (SAT/broadphase). Types math attendus mais non livrés. | `NkShapes.h`, `NkMath.h` |
| [Detection.md](Detection.md) | Broad phase (`DynamicBVH`, `SweepAndPrune`, façade `Broadphase`), narrow phase (`GJK3D`/`EPA3D`, `SAT3D`/`SAT2D`, dispatcher `Narrowphase`), contacts persistants (`ContactCache`/`PersistentManifold`, warm-starting), détection continue CCD (`SweptSphere`/`LinearCast`/`TOISolver`/`CCDManager`). | `NkBroadPhase.h`, `NkNarrowPhase.h`, `NkPersistentContacts.h`, `CCD.h` |

[← Récap NKCollision](../NKCollision.md) · [← Couche Runtime](../README.md)
