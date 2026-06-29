# NKPhysics

**Dynamique du corps rigide 2D + 3D** pour Nkentseu — **zéro-STL**, posée sur
**NKCollision**. Bibliothèque de simulation **pure** (manipule des *bodies*, pas
d'entités ECS). Namespace `nkentseu::physics`.

> État : **scaffold** (structure + specs). Voir [ROADMAP.md](ROADMAP.md) pour les jalons.

## Place dans l'architecture

```
            Noge  (GameObject/Actor + NkRigidBodyComponent/NkColliderComponent)  <-- intégration ECS ICI
              │            (pont : Transform ECS  <->  body NKPhysics/NKCollision)
   ┌──────────┴───────────┐
NKPhysics  ───utilise──►  NKCollision        (deux libs de simulation pures, sans ECS)
   │                          │
   └──────────┬───────────────┘
        NKMath · NKContainers · NKMemory · NKLogger · NKCore · NKPlatform
```

- **NKCollision** répond à « *qui se touche, où, de combien ?* » (manifolds).
- **NKPhysics** répond à « *comment ça bouge en réaction ?* » (vitesses, impulses, repos).
- **NKECS** reste l'ECS générique bas niveau ; **Noge** crée les composants gameplay et
  fait le pont avec ces deux modules.

## Structure

```
NKPhysics/
  ROADMAP.md                 # jalons M0..M10 (format hybride)
  README.md                  # ce fichier
  NKPhysics.jenga            # module staticlib (dep NKCollision) — non enregistré (scaffold)
  src/NKPhysics/
    NKPhysics.h              # umbrella
    NkPhysicsTypes.h         # NkBodyType, flags, ids, NkPhysicsConfig
    NkPhysicsMaterial.h      # densité, friction, restitution + combinaison
    NkRigidBody.h            # masse/inertie, vitesses, état (données)
    NkPhysicsWorld.h         # monde : CreateBody/DestroyBody/Step + requêtes (spec)
    NkIntegrator.h           # intégration semi-implicite (spec)
    NkContactSolver.h        # solveur d'impulses séquentiel + warm-start (spec)
  tests/                     # self-tests par jalon (à venir)
```

## Esquisse d'usage (cible M2)

```cpp
#include "NKPhysics/NKPhysics.h"
using namespace nkentseu::physics;

NkPhysicsWorld world(NkPhysicsConfig{ /*gravity*/ {0,-9.81f,0} });

// sol statique
NkBodyDef ground; ground.type = NkBodyType::STATIC;
world.CreateBody(ground, collision::NkShape::Box3D({0,-1,0}, {50,1,50}));

// caisse dynamique qui va tomber et reposer
NkBodyDef box; box.type = NkBodyType::DYNAMIC; box.material.restitution = 0.2f;
NkBodyId id = world.CreateBody(box, collision::NkShape::Box3D({0,5,0}, {0.5f,0.5f,0.5f}));

for (int i = 0; i < 300; ++i) world.Step(1.f/60.f);   // ~5 s
NkVec3f rest = world.GetBody(id)->position;            // ~ posée sur le sol
```

## Conception

Voir la section « Principes » de [ROADMAP.md](ROADMAP.md) : zéro-STL, ECS-free,
inspiration Box2D/Bullet *puis amélioration*, 2D **et** 3D, testé par vague.
