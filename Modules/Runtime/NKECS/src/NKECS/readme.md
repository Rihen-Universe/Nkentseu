# 📘 NKECS : Guide de Référence & Architecture Complète

Ce document résume l'architecture, l'API publique et les workflows de production de votre système ECS. Il est structuré comme un manuel technique destiné aux développeurs du moteur et aux programmeurs gameplay.

---

## 📐 1. Vue d'Ensemble & Philosophie

NKECS est un moteur **hybride ECS + OOP** conçu pour la production AAA. Il sépare strictement :

- **Stockage pur (SoA/Archétypes)** : Pour la localité mémoire, le SIMD et la parallélisation.
- **Façade OOP (GameObject/Scene)** : Pour l'ergonomie Unity/Unreal, le hot-reload et le scripting.
- **Orchestrateur (Scheduler)** : DAG basé sur les lectures/écritures, exécution parallèle, flush différé automatique.

### 🏗️ Architecture en Couches

```
┌─────────────────────────────────────────────────────┐
│  🎮 Gameplay / Éditeur (Unity/Unreal Style)         │
│  NkGameObject • NkActor • NkScene • NkBlueprint     │
├─────────────────────────────────────────────────────┤
│  🔌 Scripting & Hot-Reload                          │
│  NkScriptComponent • C# Bridge • Python Bridge      │
├─────────────────────────────────────────────────────┤
│  ⚙️ Orchestration & Événements                      │
│  NkScheduler • NkEventBus • NkProfiler              │
├─────────────────────────────────────────────────────┤
│  🧱 Cœur ECS (Data-Oriented)                        │
│  NkWorld • NkArchetypeGraph • NkQuery • NkTypeReg   │
└─────────────────────────────────────────────────────┘
```

---

## 🧩 2. Couche Cœur (Core ECS)

### 🔹 `NkWorld`

Conteneur central. Gère entités, composants, queries, événements et opérations différées.

```cpp
nkentseu::NkWorld world;
auto id = world.CreateEntity();
world.Add<NkTransform>(id);
world.AddDeferred<NkInactive>(id); // Sûr pendant l'itération
world.FlushDeferred();             // Applique en fin de frame
```

### 🔹 `NkEntityId`

Identifiant 64-bit (`index` + `gen`). Protégé contre les dangling pointers.

```cpp
struct NkEntityId { uint32 index, gen; };
[[nodiscard]] constexpr bool IsValid() const noexcept;
[[nodiscard]] constexpr uint64 Pack() const noexcept;
```

### 🔹 `NkQuery`

Itération zéro-allocation, cache-friendly, filtres `.With<T>()` / `.Without<T>()`.

```cpp
world.Query<const NkTransform, NkVelocity>()
     .Without<NkInactive>()
     .ForEach([](nkentseu::NkEntityId, const NkTransform& t, NkVelocity& v) {
         v.vx += 1.0f;
     });
```

### 🔹 `NkSystem` & `NkScheduler`

Les systèmes déclarent leurs accès. Le scheduler construit un DAG et parallélise.

```cpp
class MovementSystem final : public nkentseu::NkSystem {
public:
    NkSystemDesc Describe() const override {
        return NkSystemDesc{}
            .Reads<NkTransform>()
            .Writes<NkVelocity>()
            .After<InputSystem>()
            .InGroup(NkSystemGroup::Update)
            .Named("MovementSystem");
    }
    void Execute(nkentseu::NkWorld& world, float32 dt) override { /* ... */ }
};

nkentseu::NkScheduler scheduler;
scheduler.AddSystem<MovementSystem>();
scheduler.Init(world);
scheduler.Run(world, dt); // Appelle Drain → PreUpdate → Update → PostUpdate → FlushDeferred → Drain
```

### 🔹 `NkEventBus`

Publish/Subscribe thread-safe, modes immédiat (`Emit`) ou différé (`Queue` → `Drain`).

```cpp
world.Subscribe<OnPlayerDamaged>([](const OnPlayerDamaged& e) { /* ... */ });
world.Emit<OnPlayerDamaged>({ targetId, 10.0f });
```

---

## 🎮 3. Couche GameObject & Acteurs

### 🔹 `NkGameObject`

Handle léger (16-24 bytes). Routeur vers les composants ECS.

```cpp
auto go = world.CreateGameObject<NkGameObject>("Crate");
go.Add<NkCollider3D>();
go.Get<NkTransform>()->position = {0, 0, 5};
go.SetParent(parentGO);
go.AddBehaviour<PlayerController>();
```

### 🔹 `NkActor` / `NkPawn` / `NkCharacter`

Hiérarchie de classes avec cycle de vie unifié.

```cpp
class MyCharacter : public nkentseu::NkCharacter {
public:
    void BeginPlay() noexcept override { NkCharacter::BeginPlay(); /* ... */ }
    void Tick(float32 dt) noexcept override { /* Logique par frame */ }
};
```

### 🔹 `NkBehaviour`

Style Unity `MonoBehaviour`. Héberger jusqu'à 32 scripts par entité.

```cpp
class RotateBehaviour : public nkentseu::NkBehaviour {
    void OnUpdate(float dt) override { /* ... */ }
};
go.AddBehaviour<RotateBehaviour>();
```

---

## 🌍 4. Scène & Hiérarchie

### 🔹 `NkScene`

Wrapper lifecycle. Gère `BeginPlay`, `Tick`, `LateTick`, `FixedTick`, recherche et spawn.

```cpp
nkentseu::NkScene scene(world, "MainLevel");
scene.AddSceneScript<GameManager>();
auto player = scene.SpawnActor<NkCharacter>("Player", {0,0,0});
```

### 🔹 `NkSceneTransformSystem`

Calcul hiérarchique batché (DFS itératif, dirty flags `dirty` + `hierarchyDirty`, zéro récursion).

```cpp
scheduler.AddSystem<NkSceneTransformSystem>();
// Exécuté automatiquement dans PreUpdate avec priorité haute
```

### 🔹 Sockets

Points d'attache nommés sur `NkSceneComponent`.

```cpp
root->AddSocket("Muzzle", {0.3f, 0.0f, 1.0f}, NkQuat::Identity(), NkVec3::One());
NkVec3 pos = nkentseu::ecs::GetSocketWorldPosition(world, entityId, "Muzzle");
```

---

## 🧠 5. Scripting & Blueprints Visuels

### 🔹 `NkScriptComponent`

Cycle de vie C++ natif.

```cpp
void OnStart(NkWorld&, NkEntityId) noexcept override {}
void OnUpdate(NkWorld&, NkEntityId, float32 dt) noexcept override {}
void OnLateUpdate(NkWorld&, NkEntityId, float32 dt) noexcept override {}
```

### 🔹 `NkBlueprint` (Visuel)

Graphe orienté pins dynamiques (`NkPinType::Exec/Int/Float/Vec3/Custom`).

```cpp
class NkBlueprintNode {
    std::vector<NkBlueprintPin> Inputs;
    std::vector<NkBlueprintPin> Outputs;
    virtual void Execute(NkWorld&, NkEntityId, float32) noexcept = 0;
};
```

- **Évaluation paresseuse** : Les pins de données ne sont résolues que si connectées.
- **Flux d'exécution** : Stack itérative suivant les pins `Exec`.
- **Registre** : `NK_REGISTER_BLUEPRINT_NODE(NkNodePrintString)` pour l'éditeur/JSON.

### 🔹 Bridges

- **C++ DLL** : ABI stable, hot-reload avec sérialisation d'état.
- **C# (Mono)** : `NkCSharpBridge`, P/Invoke interne, `NkCSContext`.
- **Python** : `NkPythonBridge`, module `nkecs` injecté, hot-reload via `importlib`.

---

## 📦 6. Préfabriqués & Profilage

### 🔹 `NkPrefab`

Builder data-driven, sérialisable JSON, hiérarchique, overrides.

```cpp
nkentseu::NkPrefab enemyTemplate("Enemy");
enemyTemplate.WithComponent<NkTransform>()
             .WithTag(nkentseu::NkTagBit::Enemy)
             .WithBlueprint("Assets/Blueprints/EnemyAI.json");
auto go = enemyTemplate.Instantiate(world, "Goblin_01");
```

- `InstantiateBatch()` pour le spawn en masse.
- `NkPrefabInstance` tracke les overrides runtime.

### 🔹 `NkProfiler`

Zero-overhead en release, scope-based, stats agrégées.

```cpp
void Execute(NkWorld& w, float32 dt) override {
    NK_PROFILE_SCOPE("PhysicsSystem.Step");
    // ...
    NK_PROFILE_SAMPLE("Raycast.Cast");
}
```

- Export JSON pour l'éditeur.
- Archive les 60 dernières frames.

---

## 🔄 7. Exemples Complets de Boucle de Jeu

### 🟢 Exemple A : Boucle Minimale (Core ECS Pur)

```cpp
#include "NKECS/NKECS.h"

int main() {
    nkentseu::NkWorld world;
    nkentseu::NkScheduler scheduler;

    // 1. Enregistrement des systèmes
    scheduler.AddSystem<MovementSystem>();
    scheduler.AddSystem<RenderSystem>();
    scheduler.Init(world);

    // 2. Spawn d'entités
    for (uint32 i = 0; i < 100; ++i) {
        auto id = world.CreateEntity();
        world.Add<NkTransform>(id);
        world.Add<NkVelocity>(id, {1.0f, 0.0f, 0.0f});
    }

    // 3. Boucle principale
    float32 dt = 0.016f;
    while (true) {
        NK_PROFILE_SCOPE("MainLoop.Frame");
        scheduler.Run(world, dt); // Drain → PreUpdate → Update → PostUpdate → FlushDeferred → Drain
    }
    return 0;
}
```

### 🔵 Exemple B : Boucle OOP/GameObject (Style Unreal/Unity)

```cpp
#include "NKECS/NKECS.h"

class GameModeScript : public nkentseu::NkScriptComponent {
public:
    void OnStart(nkentseu::NkWorld& world, nkentseu::NkEntityId self) noexcept override {
        printf("[GameMode] Démarré\n");
    }
    void OnUpdate(nkentseu::NkWorld& world, nkentseu::NkEntityId self, float32 dt) noexcept override {
        // Logique globale
    }
    [[nodiscard]] const char* GetTypeName() const noexcept override { return "GameMode"; }
};
NK_REGISTER_SCRIPT(GameModeScript)

int main() {
    nkentseu::NkWorld world;
    nkentseu::NkScene scene(world, "TestLevel");
    nkentseu::NkScheduler scheduler;

    // 1. Ajouter scripts de scène
    scene.AddSceneScript<GameModeScript>();

    // 2. Systèmes nécessaires
    scheduler.AddSystem<NkBehaviourSystem>();
    scheduler.AddSystem<NkSceneTransformSystem>();
    scheduler.AddSystem<NkScriptSystem>();
    scheduler.AddSystem<NkSceneLifecycleSystem>(&scene);
    scheduler.Init(world);

    // 3. Spawn GameObjects
    auto player = world.CreateGameObject<NkCharacter>("Hero");
    player.SetPosition({0, 0, 0});
    player.AddBehaviour<PlayerInput>();

    // 4. Boucle
    float32 dt = 0.016f;
    while (true) {
        NK_PROFILE_SCOPE("OOPGameLoop");
        scheduler.Run(world, dt);
        // Le scheduler appelle automatiquement BeginPlay/Tick/LateTick/FixedTick via NkSceneLifecycleSystem
    }
}
```

### 🟣 Exemple C : Boucle avec Blueprints & Profilage Avancé

```cpp
#include "NKECS/NKECS.h"

int main() {
    nkentseu::NkWorld world;
    nkentseu::NkScene scene(world, "BlueprintLevel");
    nkentseu::NkScheduler scheduler;

    // 1. Charger des Blueprints depuis JSON (via éditeur ou runtime)
    nkentseu::ecs::blueprint::NkBlueprint bp;
    // bp.LoadFromJSON("Assets/Blueprints/EnemyAI.json");
    // auto enemy = scene.SpawnActor<NkGameObject>("Enemy", {10,0,0});
    // enemy.Add<nkentseu::ecs::blueprint::NkBlueprintComponent>(std::move(bp));

    // 2. Systèmes
    scheduler.AddSystem<NkScriptSystem>();
    scheduler.AddSystem<NkSceneTransformSystem>();
    scheduler.Init(world);

    // 3. Boucle avec export profilage périodique
    uint32 frameCount = 0;
    float32 dt = 0.016f;
    char profileBuffer[65536];

    while (true) {
        {
            NK_PROFILE_SCOPE("Frame.Update");
            scheduler.Run(world, dt);
            nkentseu::NkProfiler::Global().NextFrame();
            ++frameCount;
        }

        // Export stats toutes les 300 frames (~5 secondes à 60 FPS)
        if (frameCount % 300 == 0) {
            auto stats = nkentseu::NkProfiler::Global().GetStats("Frame.Update");
            printf("Avg Frame: %llu ns | Min: %llu | Max: %llu | Samples: %u\n",
                   stats.AvgTime(), stats.minTime, stats.maxTime, stats.sampleCount);

            if (nkentseu::NkProfiler::Global().ExportToJSON(profileBuffer, sizeof(profileBuffer))) {
                // File::Write("profile_dump.json", profileBuffer);
            }
        }
    }
}
```

---

## 📖 8. Référence API Rapide


| Module         | Classe / Fonction                        | Description                                |
| -------------- | ---------------------------------------- | ------------------------------------------ |
| **World**      | `CreateEntity()`                         | Crée entité vide                         |
|                | `Add<T>(id, val)`                        | Ajoute/migre composant (immédiat)         |
|                | `AddDeferred<T>(id, val)`                | Planifie ajout (sûr en itération)        |
|                | `Query<T...>()`                          | Retourne`NkQueryResult`                    |
|                | `FlushDeferred()`                        | Applique changements différés            |
|                | `Emit<T>(evt)` / `Queue<T>(evt)`         | Événements immédiats / différés       |
| **GameObject** | `CreateGameObject<T>(name)`              | Factory avec composants obligatoires       |
|                | `Add<T>()` / `Get<T>()` / `GetAll<T>()`  | Accès composants (supporte multi via Bag) |
|                | `SetParent(go)` / `GetChildren()`        | Hiérarchie ECS                            |
|                | `AddBehaviour<T>()`                      | Attache script C++                         |
| **Scene**      | `BeginPlay()` / `Tick(dt)` / `EndPlay()` | Lifecycle scène                           |
|                | `SpawnActor<T>(name, pos)`               | Instancie acteur avec transform            |
|                | `FindActorByName(name)`                  | Recherche par nom                          |
| **Systems**    | `Describe()`                             | Déclare reads/writes/group/priority/after |
|                | `Execute(world, dt)`                     | Logique par frame                          |
|                | `Run(world, dt)`                         | Scheduler orchestre tout                   |
| **Blueprint**  | `NkPinType::Exec/Int/Float/Vec3`         | Types de données                          |
|                | `Graph.Link(src, srcPin, tgt, tgtPin)`   | Connexion nœuds                           |
|                | `Graph.Execute(world, self, dt)`         | Évaluation graphe                         |
| **Prefab**     | `WithComponent<T>()` / `WithChild()`     | Builder pattern                            |
|                | `Instantiate(world, name)`               | Création instance                         |
|                | `Serialize()` / `Deserialize()`          | JSON I/O                                   |
| **Profiler**   | `NK_PROFILE_SCOPE(name)`                 | Marqueur début/fin scope                  |
|                | `NK_PROFILE_SAMPLE(name)`                | Marqueur ponctuel                          |
|                | `NextFrame()` / `ExportToJSON()`         | Archivage & export                         |

---

## 🛡️ 9. Bonnes Pratiques & Performance


| Règle                                                                         | Impact         | Explication                                                                                           |
| ------------------------------------------------------------------------------ | -------------- | ----------------------------------------------------------------------------------------------------- |
| **Toujours utiliser `AddDeferred` / `DestroyDeferred` dans les queries**       | 🔴 Critique    | Évite l'invalidation d'itérateurs et les corruptions mémoire.                                      |
| **Garder les composants POD et < 64 bytes**                                    | 🟢 Performance | Améliore la localité SoA, permet vectorisation SIMD.                                                |
| **Préférer les entités enfants aux multi-composants massifs**               | 🟡 Moyen       | `NkComponentBag` est optimisé pour ≤8 instances. Au-delà, la hiérarchie ECS est plus performante. |
| **Déclarer précisément `Reads<T>()` / `Writes<T>()`**                       | 🔴 Critique    | Permet au scheduler de paralléliser correctement. Les conflits forcent l'exécution séquentielle.   |
| **Utiliser `const T&` dans les queries quand possible**                        | 🟡 Moyen       | Le compilateur optimise mieux les lectures pures (aliasing rules).                                    |
| **Regrouper les systèmes par groupe logique**                                 | 🟢 Performance | `PreUpdate` (Input) → `Update` (Gameplay) → `PostUpdate` (Camera/Audio) → `Render`.                |
| **Profiler avec `NK_PROFILE_SCOPE` en développement, désactiver en Release** | 🟢 Performance | Les macros deviennent des`no-op` en mode Release (`#define NK_PROFILE_SCOPE(x)`).                     |
| **Éviter les `dynamic_cast` dans les boucles chaudes**                        | 🔴 Critique    | Utiliser les registres de types ou les IDs ECS pour le polymorphisme runtime.                         |

---

## ✅ Prochaines Étapes Recommandées

1. **Intégrer un système de sérialisation JSON robuste** (RapidJSON / nlohmann) pour `NkPrefab` et `NkBlueprint`.
2. **Ajouter un éditeur visuel** qui consomme `NkBlueprintNodeRegistry` et `NkPrefab` pour le drag-and-drop.
3. **Implémenter le hot-reload complet** pour les Blueprints (restauration d'état via `Serialize`/`Deserialize` des pins).
4. **Benchmark de production** : 100k entités, 50 systèmes, validation du DAG scheduler et du flush différé.

Ce guide couvre l'intégralité du système tel qu'implémenté dans vos fichiers. Il est prêt à être distribué comme documentation officielle aux développeurs gameplay et outils. 📘🛠️
