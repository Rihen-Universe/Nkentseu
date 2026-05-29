# NKECS — Roadmap

État actuel (mai 2026) : noyau ECS bas niveau opérationnel (archetypes SoA, queries variadic, scheduler DAG mono-thread/jobs basique, event bus gameplay STL-free). Sérialisation, prefabs et reflection présents mais partiellement câblés ; l'API gameplay haut niveau (GameObject, Scene, Behaviour, Prefab) vit dans `Engine/Noge/src/Noge/ECS/` au-dessus de NKECS.

---

## Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| `NkEntityId` (index + gen, anti-dangling) | Livré | — | — |
| `NkTypeRegistry` (lazy id, meta ctor/dtor/move) | Livré | — | — |
| `NkComponentPool` (stockage dense, swap-remove) | Livré | — | — |
| `NkArchetype` + `NkEntityIndex` (SoA) | Livré | — | — |
| `NkArchetypeGraph` (cache mask/edge, FNV-1a) | Livré | — | — |
| `NkWorld` (Create/Destroy/Add/Remove/Has/Get/Set) | Livré | — | — |
| `NkEntityBuilder` / `NkBatchBuilder` fluent | Livré | — | — |
| Opérations différées (`AddDeferred`, `RemoveDeferred`, `FlushDeferred`) | Livré | — | — |
| `NkQuery<Ts...>` (ForEach, With, Without) | Livré | — | — |
| `ForEachBatch` SIMD-friendly | Livré | — | — |
| `NkSystem` + `NkSystemDesc` (read/write/exclude/after) | Livré | — | — |
| `NkScheduler` + `NkJobPool` (DAG simple) | Partiel | M | P1 |
| `NkLambdaSystem` | Livré | — | — |
| `NkGameplayEventBus` (Emit/Queue/Drain, NkFunction) | Livré | — | — |
| `NkReflect` (FieldInfo, métadonnées flags 64 bits) | Partiel | M | P1 |
| `NkJsonSerialization` (Prefab + Blueprint) | Cassé | M | P0 |
| Macro `NK_COMPONENT` opt-in | Livré | — | — |
| Benchmarks / stress tests | Partiel | S | P2 |
| Scripting Python / C# | TODO | L | P3 |
| Prefabs (déclarés mais absents du module) | TODO | M | P0 |
| Visual Script / Blueprint runtime | TODO | L | P2 |
| Hiérarchie / parent-child native ECS | TODO | M | P1 |
| Replication / NetWorld | TODO | L | P3 |

Légende : Livré · Partiel · En cours · TODO · Abandonné

---

## Livré

### Phase 1 — Fondations
- `NkEntityId` 64 bits packé (index 32 + génération 32) avec garde anti-dangling.
- Constantes de dimensionnement : `kMaxComponentTypes=256`, `kMaxArchetypes=4096`, `kMaxEntities=1<<20`, `kChunkSize=16Ko`.
- `NkSpan<T>` (mini std::span), assertions `NKECS_ASSERT`, hash FNV-1a constexpr.
- `NkTypeRegistry::Global()` thread-safe : enregistrement lazy d'un `ComponentMeta` (id, size, align, ctor/dtor/move/copy, name, typeHash).

### Phase 2 — Stockage
- `NkComponentPool` dense, support des composants triviaux et non-triviaux via pointeurs sur fonctions.
- `NkArchetype` : layout SoA, pools indexées par `NkComponentId`, `AddEntity` / `RemoveEntity` (swap-remove O(1)), `MigrateFrom` pour transitions.
- `NkEntityIndex` : table `index -> (archetypeId, row, gen, alive)`, free-list LIFO (capacité 256, débordement silencieux).
- `NkArchetypeGraph` : tables de hachage open-addressing FNV-1a, cache `mask -> archId`, cache edge `(srcId, cid, op) -> dstId`.

### Phase 3 — API World
- `NkWorld` façade : `CreateEntity`, `Create()` builder fluide, `CreateBatch(count, out)`, `Destroy`, `DestroyDeferred`, `IsAlive`, `EntityCount`.
- Composants : `Add<T>` (copy + move), `Remove<T>`, `Get<T>`, `GetRef<T>`, `Has<T>`, `Set<T>`.
- Opérations différées : `AddDeferred`, `RemoveDeferred`, `FlushDeferred` pour sécuriser les itérations.
- Conteneurs maison `NkVector` / `NkFunction` sur la surface publique (STL bannie en API).

### Phase 4 — Queries & Systèmes
- `NkQueryResult<Ts...>` : masque `required | with` + `without`, `ForEach`, `ForEachBatch` avec accès direct aux tableaux d'archetype (SIMD-friendly).
- `NkSystem` interface (`Describe()` + `Execute(world, dt)`, hooks `OnCreate/OnDestroy/OnEnable/OnDisable`).
- `NkSystemDesc` fluent : `Reads<T>`, `Writes<T>` (implique read), `Excludes<T>`, `After<OtherSystem>`, `InGroup`, `WithPriority`, `Sequential`, `Named`. Détection de conflits read/write au niveau bit-mask.
- `NkSystemGroup` : `PreUpdate` / `Update` / `PostUpdate` / `FixedUpdate` / `Render`.
- `NkLambdaSystem` pour prototypage.
- `NkScheduler` + `NkJobPool` (std::thread) : soumission de jobs, `WaitAll`, parallélisation grossière au sein d'un groupe.

### Phase 5 — Événements
- `NkGameplayEventBus` STL-free : `Subscribe<T>`, `Emit<T>` immédiat, `Queue<T>` + `Drain()` différé, `Unsubscribe` par `SubscriptionId`, mutex `NkMutex`.
- Renommage explicite `NkEventBus -> NkGameplayEventBus` pour éviter le conflit avec l'EventBus moteur (input / fenêtre).

### Phase 6 — Réflexion (partielle)
- `NkReflect` : `NkFieldType` (primitifs, math, ECS, Array/Object/Enum/Flags), `NkMetaFlag` 64 bits (Visible, Serialize, EditAnywhere, BlueprintReadWrite, Replicated, Range, ColorPicker, Instanced, Transient, User0..15).
- Macros `NK_COMPONENT` documentaires.

---

## En cours / TODO immédiat

### Sérialisation cassée (P0)
- `Serialization/NkJsonSerialization.h` inclut `../Prefab/NkPrefab.h` et `../VisualScript/NkBlueprint.h` qui **n'existent pas** dans le module NKECS. Les fichiers réels sont dans `Engine/Noge/src/Noge/ECS/Prefab/` et `Engine/Noge/src/Noge/ECS/VisualScript/`. Soit déplacer le serializer dans Noge, soit créer des stubs Prefab/Blueprint dans NKECS, soit refactoriser le serializer pour qu'il ne dépende que de la réflexion et soit indépendant des types haut niveau.
- Dépendance directe à `nlohmann/json.hpp` au sein du runtime — à remplacer par `NKJson` (conteneur maison) si on respecte la même règle STL-free que `NkWorld`.

### Scheduler (P1)
- Construction du DAG : actuellement, l'algorithme décrit en commentaire (tri topologique + détection de conflits write/read) n'est pas complètement implémenté côté .cpp ; vérifier `NkScheduler.cpp` (non audité ici car non listé).
- Pas de partition explicite par `NkSystemGroup` exécutée séquentiellement (PreUpdate -> Update -> PostUpdate -> FixedUpdate -> Render).
- Job pool très basique (LIFO `mJobs.back()`, pas de work stealing, pas d'affinité), pas d'API `ParallelFor` exposée aux systèmes.
- Profilage / scoped timers par système absent.

### Réflexion (P1)
- `NkReflect` définit les types mais aucune génération automatique de `FieldInfo[]` pour les composants utilisateurs (pas de codegen, pas de macros `NK_FIELD(...)` opérationnelles vérifiées).
- Manque de pont avec NKUI Inspector (rendre les champs éditables via `NkUIWidgets`).

### Hiérarchie native (P1)
- Aucun composant `Parent/Children` officiel dans NKECS — la hiérarchie est gérée par Noge (`NkSceneGraph`). Évaluer si un composant `NkHierarchy` léger doit descendre dans NKECS pour les usages génériques.

### Free-list (P2)
- `NkEntityIndex::FreeList` capacité fixe 256 : au-delà, les indices libérés sont perdus (commentaire explicite). À remplacer par une free-list extensible ou par un chaînage intrusif dans `Entry`.

### Benchmarks (P2)
- `Benchmark/NkECSBenchmark.h` et `NkECSStressTest.h` présents mais aucun rapport publié. Cibler 1M entités, 50 archetypes, 10 systèmes parallélisés.

---

## À venir / À ajouter (futur proche)

### Stockage et performance
- Stockage sparse-set optionnel pour les composants à faible cardinalité (vs archetype par défaut) — utile pour singletons, tags rares.
- Chunked storage (`kChunkSize=16Ko` déjà constanté mais le code utilise `new[]` 1.5x : ne respecte pas vraiment le pattern Unity DOTS chunk-based). Migrer `NkComponentPool` et `NkArchetype` vers de vrais chunks de 16 Ko.
- SIMD intrinsics (`AVX2`) dans les passes `ForEachBatch` typiques (transform update, integration).
- Allocateurs `NkAllocator` (linear, pool, scratch) au lieu de `new[]` brut dans `GrowEntityArray` / `GrowIfNeeded`.

### Scheduler avancé
- Vraie compilation du DAG : tri topologique de Kahn, détection de cycles, regroupement en "waves" parallèles.
- API `ParallelForChunk` exposée aux systèmes pour itérer en parallèle sur un archetype par tranches.
- Phase commands (command buffers par worker, flush au sync point) pour les structural changes sans verrou global.
- Intégration `NKThreading` (job system maison) au lieu de `std::thread`.

### Prefab / Scene (à descendre depuis Noge ?)
- Soit déplacer `NkPrefab` / `NkSceneGraph` de `Engine/Noge/src/Noge/ECS/` vers NKECS pour avoir une feature complète, soit assumer la séparation et supprimer toute référence à Prefab/Blueprint dans NKECS (notamment le serializer).
- Format `.nkprefab` versionné, migrations entre versions.
- Instanciation avec overrides champ par champ.

### Sérialisation propre
- Binary serialization rapide (frame snapshot, save game) — séparée du JSON éditeur.
- Versioning des composants (`NkComponentVersion`, migration au load).
- Round-trip ECS world : World -> JSON -> World identique (test bit-exact sur le sous-ensemble réfléchi).

### Réflexion / Inspector
- Codegen ou macros `NK_REFLECT(Type, FieldList)` qui poussent les `FieldInfo[]` dans `NkTypeRegistry`.
- Backend Inspector dans NKUI : pour chaque `FieldInfo`, choisir le widget (DragFloat, ColorPicker, Combo enum, Tree...) selon `NkFieldType` + `NkMetaFlag`.

### Networking / Replication
- `NkNetWorld` (déjà présent côté Noge) : descendre la couche bas niveau (snapshots delta, interest management) dans NKECS si elle reste générique.
- Flag `NkMeta_Replicated` exploité côté serializer / delta encoder.

### Scripting
- Bindings Python (CPython embed) et C# (Mono ou CoreCLR) référencés dans `NKECS.h` (commentés). Décision : on garde dans Noge ou on descend dans NKECS ?
- Lifecycle scripts : `OnCreate/OnUpdate/OnDestroy` côté script avec marshalling auto des composants.

### Outils
- Profiler ECS (temps par système, miss rate cache archetype, taille des chunks).
- Visualiseur du graphe d'archetypes (export DOT/Graphviz).
- Validator : détecter les composants enregistrés mais jamais utilisés, archetypes vides persistants.

---

## Bugs / quirks connus
- `NkJsonSerialization.h` inclut des chemins inexistants dans NKECS (`../Prefab/NkPrefab.h`, `../VisualScript/NkBlueprint.h`). Le header **ne compile probablement pas** tel quel dans une unit standalone NKECS.
- `NkSystem.h` réimporte `<functional>` / `<string>` / `<vector>` / `<typeindex>` alors que la doctrine `NkWorld.h` est STL-free en public — incohérence à résoudre.
- `NkScheduler.h` utilise `std::function`, `std::thread`, `std::mutex`, `std::vector` — STL omniprésente, incompatible avec la politique "STL-free public surface" affichée dans NkWorld/NkGameplayEventBus.
- `NkEntityIndex::FreeList` perd silencieusement les indices au-delà de 256 entrées : pas un crash mais fragmentation progressive.
- `NkArchetype` réserve `mPools[kMaxComponentTypes]` (256 pools tableau de taille fixe par archétype) ce qui est très coûteux en mémoire (256 * sizeof(NkComponentPool) par archetype) et empêche d'avoir 4096 archetypes simultanés en pratique.
- `NkBatchBuilder::Build` itère naïvement (création N fois + apply M components) au lieu d'allouer en bloc dans l'archetype cible.
- Aucun test unitaire référencé directement dans le module (seulement des en-têtes de benchmark).

---

## Dépendances
- **Couches en dessous (utilisées)** :
  - `NKCore` (NkTypes, NkTraits)
  - `NKContainers` (NkVector, NkFunction)
  - `NKLogger` (NkLog)
  - `NKThreading` (NkMutex pour l'event bus)
  - `nlohmann/json` (uniquement via `NkJsonSerialization`, à isoler)
- **Modules au-dessus qui en dépendent** :
  - `Engine/Noge/src/Noge/ECS/` : `NkGameObject`, `NkActor`, `NkBehaviour`, `NkBehaviourSystem`, `NkPrefab`, `NkSceneGraph`, `NkSceneManager`, `NkSceneLifecycleSystem`, `NkComponentRegistry`, `NkComponentHandle`, `NkTransform`, `NkTag`, `NkRenderSystem`, `NkTransformSystem`, `NkSceneSerializer`, `NkReflectComponents`, `NkBlueprint`, `NkBlueprintHotReload`, `NkScriptSystem`, `NkScriptBridge`, `NkScriptPython`, `NkScriptCSharp`, `NkNetWorld`, `NkGameObjectFactory`.
- **Relation à Noge** : NKECS = noyau ECS générique (entities, archetypes, queries, scheduler bas niveau, event bus). Noge/ECS = couche gameplay-friendly (GameObject hérité d'`NkEntityId`, Prefab/Scene/Behaviour/Script, hiérarchie, sérialisation projet). La séparation est volontaire : NKECS ne **doit pas** dépendre de Noge (cf. commentaire en tête de `NkWorld.h`). Cette doctrine est globalement respectée sauf dans `NkJsonSerialization.h`.
