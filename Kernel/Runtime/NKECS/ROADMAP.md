# NKECS — Roadmap

État actuel (juin 2026) : noyau ECS bas niveau opérationnel (archetypes SoA, queries variadic, scheduler DAG mono-thread/jobs basique, event bus gameplay STL-free). Sérialisation des composants/entités **livrée** via le pont réflexion (`NkReflectBridge` + `NkEntitySerialization` sur NKReflection/NKSerialization), couverte par 2 suites de tests (`test_reflect_bridge` 42/42, `test_entity_serialization` 29/0). L'API gameplay haut niveau (GameObject, Scene, Behaviour, Prefab) vit dans `Engine/Noge/src/Noge/ECS/` au-dessus de NKECS.

> **Bloquants standalone corrigés (2026-06)** : alias morts `NkOnButtonClicked/NkOnSliderChanged` retirés de `NKECS.h` (l'umbrella compile désormais) ; `NKECS.jenga` ré-écrit (était une copie non adaptée de `NKFont.jenga` : docstring, `includedirs(["src/NKFont"])`, et `links` de tests faux) ; `NkArchetype::mPoolIndex` désormais initialisé à `kInvalidPoolIndex` (était non initialisé). `NkArchetypeGraph` et `NkEntityIndex` ont reçu un vrai move (anti double-free) ; `NkWorld` est marqué `move=delete` explicitement (le `NkMutex` de l'EventBus rend NkWorld non-déplaçable — le `=default` antérieur mentait sur l'API).
>
> **Rouille build** : NKECS n'est PAS branché dans le workspace racine `Nkentseu.jenga` (aucun `include(...)` pour `Kernel/Runtime/NKECS/NKECS.jenga`) — la cible n'apparaît donc pas dans `jenga build --target NKECS`. À brancher (CI/workspace). En attendant, la compilation a été prouvée hors-workspace (clang + libs Debug des dépendances).

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
| `NkScheduler` + `NkJobPool` (DAG complet, mais 100% STL + non branché sur NkWorld) | Partiel | M | P1 |
| `NkLambdaSystem` | Livré | — | — |
| `NkGameplayEventBus` (Emit/Queue/Drain, NkFunction) | Livré | — | — |
| `NkReflect` (FieldInfo, métadonnées flags 64 bits) | Partiel | M | P1 |
| `NkReflectBridge` (pont composant ↔ NKReflection/NKSerialization) | Livré | — | — |
| `NkEntitySerialization` (entité/world ↔ archive JSON) | Livré | — | — |
| `NkJsonSerialization` (réflexion via NkReflectBridge + NkArchive) | Livré | — | — |
| Tests `test_reflect_bridge` (42/42) + `test_entity_serialization` (29/0) | Livré | — | — |
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

### Sérialisation (livrée — P0 résolu)
- La sérialisation des composants/entités passe désormais par le **pont réflexion** : `Reflect/NkReflectBridge.h` (composant ↔ `NKReflection` + `NKSerialization`) et `Serialization/NkEntitySerialization.h` (entité/world ↔ `NkArchive`). `Serialization/NkJsonSerialization.h` s'appuie sur la réflexion + `NkArchive` (JSON via `NKSerialization`), sans dépendre des types haut niveau Prefab/Blueprint de Noge.
- Couverture : `tests/test_reflect_bridge.cpp` (42/42) round-trip composant réfléchi, `tests/test_entity_serialization.cpp` (29/0).
- Reste : sérialisation **binaire** rapide (snapshot/save game) distincte du JSON éditeur ; round-trip d'un world complet (cf. section « Sérialisation propre »).

### Scheduler (P1) — DAG complet mais 100% STL + non branché sur NkWorld
- `NkScheduler.cpp` implémente le tri topologique + détection de conflits read/write, mais le scheduler reste **entièrement STL** (`std::unique_ptr`, `std::vector`, `std::type_index`, `std::function`, `std::thread`, `std::mutex`) — incompatible avec la politique zero-STL affichée par `NkWorld`/`NkGameplayEventBus`. C'est le **chantier #3** (conversion zero-STL), volontairement laissé de côté pour l'instant.
- Bug de compilation corrigé en passant (NON la conversion zero-STL) : `SystemEntry::typeId` (`std::type_index`) n'avait pas d'initialiseur → ctor par défaut implicitement supprimé → l'umbrella `NKECS.h` ne compilait pas. Fix : `typeId{typeid(void)}`.
- Le scheduler n'est **pas branché sur NkWorld** (NkWorld ne possède pas de scheduler ; l'orchestration des systèmes est externe).
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
- **[CORRIGÉ 2026-06]** `NKECS.h` aliasait `ecs::NkOnButtonClicked` / `ecs::NkOnSliderChanged` (types inexistants) → l'umbrella ne compilait pas. Alias retirés.
- **[CORRIGÉ 2026-06]** `NKECS.jenga` était une copie non adaptée de `NKFont.jenga` (docstring NKFont, `includedirs(["src/NKFont"])`, `links` de tests = `["NKFont","NKMemory","NKCore"]`). Ré-écrit pour NKECS (modèle `NKReflection.jenga`/`NKSerialization.jenga`).
- **[CORRIGÉ 2026-06]** `NkArchetype::mPoolIndex[kMaxComponentTypes]` était déclaré sans initialiseur et seules les entrées des composants présents étaient écrites → lecture de valeurs indéterminées possible. Désormais rempli à `kInvalidPoolIndex` (sentinelle ≠ 0) dans le constructeur.
- **[CORRIGÉ 2026-06]** `NkArchetypeGraph` et `NkEntityIndex` supprimaient la copie sans déclarer de move alors qu'ils possèdent des ressources dynamiques (`mArchetypes[]` alloués, `mEntries`) → move implicite supprimé / copie dangereuse (double-free). Vrai move ajouté (transfert + neutralisation de la source). `NkWorld` marqué `move=delete` explicitement (le `NkMutex` de `NkGameplayEventBus` rend NkWorld non-déplaçable — l'ancien `=default` était silencieusement supprimé).
- **[CORRIGÉ 2026-06]** `NkScheduler::SystemEntry::typeId` (`std::type_index`) sans initialiseur → ctor par défaut implicitement supprimé → l'umbrella ne compilait pas. Fix `typeId{typeid(void)}` (fix de compilation, PAS la conversion zero-STL).
- `NkSystem.h` réimporte `<functional>` / `<string>` / `<vector>` / `<typeindex>` alors que la doctrine `NkWorld.h` est STL-free en public — incohérence à résoudre (chantier #3 scheduler/systems zero-STL).
- `NkScheduler.h`/`.cpp` utilise `std::function`, `std::thread`, `std::mutex`, `std::vector`, `std::unique_ptr`, `std::type_index` — STL omniprésente, incompatible avec la politique "STL-free public surface" (chantier #3, séparé).
- `NkEntityIndex::FreeList` perd silencieusement les indices au-delà de 256 entrées : pas un crash mais fragmentation progressive. À remplacer par une free-list extensible / chaînage intrusif.
- `NkArchetype` réserve `mPools[kMaxComponentTypes]` (256 pools tableau de taille fixe par archétype) ce qui est très coûteux en mémoire (256 * sizeof(NkComponentPool) par archetype) et empêche d'avoir 4096 archetypes simultanés en pratique.
- `NkBatchBuilder::Build` itère naïvement (création N fois + apply M components) au lieu d'allouer en bloc dans l'archetype cible.
- Tests unitaires présents : `tests/test_reflect_bridge.cpp` (42/42) et `tests/test_entity_serialization.cpp` (29/0). Manquent : tests cœur Add/Get/Remove/Query, scheduler, archetype graph.

---

## Dépendances
- **Couches en dessous (utilisées)** :
  - `NKCore` (NkTypes, NkTraits)
  - `NKContainers` (NkVector, NkFunction)
  - `NKLogger` (NkLog)
  - `NKThreading` (NkMutex pour l'event bus)
  - `NKReflection` + `NKSerialization` (pont réflexion `NkReflectBridge` + sérialisation `NkArchive`/JSON ; remplace l'ancienne dépendance directe à `nlohmann/json`)
  - `NKMath`, `NKPlatform` (types de base)
- **Modules au-dessus qui en dépendent** :
  - `Engine/Noge/src/Noge/ECS/` : `NkGameObject`, `NkActor`, `NkBehaviour`, `NkBehaviourSystem`, `NkPrefab`, `NkSceneGraph`, `NkSceneManager`, `NkSceneLifecycleSystem`, `NkComponentRegistry`, `NkComponentHandle`, `NkTransform`, `NkTag`, `NkRenderSystem`, `NkTransformSystem`, `NkSceneSerializer`, `NkReflectComponents`, `NkBlueprint`, `NkBlueprintHotReload`, `NkScriptSystem`, `NkScriptBridge`, `NkScriptPython`, `NkScriptCSharp`, `NkNetWorld`, `NkGameObjectFactory`.
- **Relation à Noge** : NKECS = noyau ECS générique (entities, archetypes, queries, scheduler bas niveau, event bus). Noge/ECS = couche gameplay-friendly (GameObject hérité d'`NkEntityId`, Prefab/Scene/Behaviour/Script, hiérarchie, sérialisation projet). La séparation est volontaire : NKECS ne **doit pas** dépendre de Noge (cf. commentaire en tête de `NkWorld.h`). Cette doctrine est globalement respectée sauf dans `NkJsonSerialization.h`.
