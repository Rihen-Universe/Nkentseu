# NKMemory — Roadmap

État actuel (mai 2026) : module mémoire complet et instrumenté. Allocateurs
multiples (malloc, pool fixe/variable, container size-classes + TLS,
multi-level dispatch), smart pointers, garbage collector mark-and-sweep,
tracker O(1), profiler à hooks, tagging. 18 fichiers de tests dans `tests/`,
couverture solide. Reste à exposer un vrai allocateur d'arène / linear /
stack / virtual pour fermer la matrice des stratégies évoquée dans les
benchmarks.

---

## 📊 Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| `NkAllocator` interface + `NkMallocAllocator` | ✅ Livré | — | — |
| `NkPoolAllocator` (fixe + variable) | ✅ Livré | — | — |
| `NkContainerAllocator` (size-classes + TLS) | ✅ Livré | — | — |
| `NkMultiLevelAllocator` (dispatch UE5-style) | ✅ Livré | — | — |
| `NkUniquePtr` / `NkSharedPtr` / `NkIntrusivePtr` | ✅ Livré | — | — |
| `NkGarbageCollector` mark-and-sweep | ✅ Livré | — | — |
| `NkMemoryTracker` leak detection O(1) | ✅ Livré | — | — |
| `NkMemoryProfiler` hooks runtime | ✅ Livré | — | — |
| `NkMemoryTag` budgets par domaine | ✅ Livré | — | — |
| `NkMemorySystem` singleton + macros `NK_MEM_*` | ✅ Livré | — | — |
| `NkHash` / `NkPointerHashMap` | ✅ Livré | — | — |
| `NkStlAdapter` (interop std::*) | ✅ Livré | — | — |
| Arena / Linear / Stack / Buddy / Virtual | 🔶 Partiel (tests existent) | M | Haute |
| `NkGlobalOperators` (override `new`/`delete` globaux) | ✅ Livré | — | — |
| SIMD memcopy/memset/search (`NkFunctionSIMD`) | 🔶 Partiel | M | Moyenne |
| Documentation publique consolidée | 🔶 Partiel (Readme énorme) | S | Basse |

Légende : ✅ Livré · 🔶 Partiel · ⏳ En cours · ❌ TODO · 🚫 Abandonné

---

## ✅ Livré

### Interface et allocateurs de base
- [NkAllocator.h/.cpp](src/NKMemory/NkAllocator.h) : `NkAllocatorBase` (nom +
  destruction virtuelle), `NkAllocator` (Allocate / Deallocate / Reallocate /
  Calloc / Reset / Name)
- Constante `NK_MEMORY_DEFAULT_ALIGNMENT = alignof(std::max_align_t)`
- Flags `NkMemoryFlag` : `READ`, `WRITE`, `EXECUTE`, `RESERVE`, `COMMIT`,
  `ANONYMOUS` (utilisés par les mappings bas niveau)
- `NkMallocAllocator` wrapper malloc/free

### Allocateurs spécialisés
- [NkPoolAllocator.h/.cpp](src/NKMemory/NkPoolAllocator.h) :
  - `NkFixedPoolAllocator<BlockSize, NumBlocks>` : template ultra-rapide
    (~8-15 ns/alloc), zéro fragmentation, free-list O(1), thread-safe via
    `NkSpinLock`
  - `NkVariablePoolAllocator` : tailles variables (~40-70 ns/alloc)
  - Tests : `test_allocator_pool.cpp`
- [NkPoolAllocatorTyped.h](src/NKMemory/NkPoolAllocatorTyped.h) : variante
  `NkTypedPool<T>` avec placement new intégré
- [NkContainerAllocator.h/.cpp](src/NKMemory/NkContainerAllocator.h) :
  13 size classes + cache TLS (Thread-Local Storage) → ~5-12 ns/alloc en
  hit cache. Conçu pour conteneurs STL. Tests : `test_allocator_container.cpp`
- [NkMultiLevelAllocator.h/.cpp](src/NKMemory/NkMultiLevelAllocator.h) :
  dispatch automatique Tiny/Small/Medium/Large selon taille demandée. Style
  UE5. Header de dispatch unifié pour tracking + deallocation cohérent.
- Tests existants couvrant aussi : arena, buddy, freelist, linear, malloc,
  stack, virtual (cf. `tests/test_allocator_*.cpp`) — **les implémentations
  correspondantes ne sont PAS toutes dans le `src/` actuel** (cf. section
  TODO).

### Smart pointers
- [NkUniquePtr.h](src/NKMemory/NkUniquePtr.h) : ownership unique header-only,
  support allocateur custom + deleter custom
- [NkSharedPtr.h](src/NKMemory/NkSharedPtr.h) : référence comptée header-only
  avec bloc de contrôle séparé, thread-safe
- [NkIntrusivePtr.h](src/NKMemory/NkIntrusivePtr.h) : référence comptée
  intrusive (compteur dans l'objet), zéro bloc séparé

### Garbage collection
- [NkGc.h/.cpp](src/NKMemory/NkGc.h) : `NkGarbageCollector` mark-and-sweep
  avec hash table O(1), racines externes (`NkGcRoot`), `NkGcObject` base
  class avec virtual `Trace(NkGcTracer&)`, support multi-GC (`NkMemorySystem`
  peut créer N GC nommés)
- Tests : `test_gc.cpp`

### Système unifié
- [NkMemory.h/.cpp](src/NKMemory/NkMemory.h) : `NkMemorySystem` singleton
  Meyer thread-safe avec `Initialize/Shutdown`, allocation avec metadata
  (`file/line/function/tag`), templates `New<T>` / `Delete<T>` / `NewArray<T>`
  / `DeleteArray<T>`, stats (`NkMemoryStats`), `DumpLeaks()`, gestion multi-GC
  (`CreateGc/DestroyGc/SetGcName/GetGcProfile/GetGcCount`)
- Macros legacy-friendly : `NK_MEM_ALLOC`, `NK_MEM_FREE`, `NK_MEM_NEW`,
  `NK_MEM_DELETE`, `NK_MEM_NEW_ARRAY`, `NK_MEM_DELETE_ARRAY`,
  `NK_MEMORY_SYSTEM`, `NK_GC_GENERAL`
- Tests : `test_memory_system.cpp` (AllocateFreeAndStats,
  HashIndexedTrackingUnderChurn, CreateGcDestroyGcAndProfile, ...)

### Tracking, profiling, tagging
- [NkTracker.h/.cpp](src/NKMemory/NkTracker.h) : `NkMemoryTracker` avec hash
  table O(1) pour détection de fuites, `NkAllocationInfo`
- [NkProfiler.h/.cpp](src/NKMemory/NkProfiler.h) : hooks `SetAllocCallback`,
  `SetDeallocCallback` pour monitoring runtime sans recompilation
- [NkTag.h/.cpp](src/NKMemory/NkTag.h) : `NkMemoryTag` (ENGINE, GAME, RENDER,
  AUDIO, etc.), budgets par tag avec reporting `Used / Budget / % / Peak`

### Hash et utilitaires
- [NkHash.h/.cpp](src/NKMemory/NkHash.h) : `NkPointerHashMap` pour index O(1)
  des pointeurs trackés (utilisé en interne par `NkMemorySystem`)
- [NkFunction.h/.cpp](src/NKMemory/NkFunction.h) : `NkCopy`, `NkMove`, `NkSet`,
  `NkSearchPattern`, `NkAlignPointer`, `NkMemZero`, `NkSecureZero`
- [NkFunctionSIMD.h/.cpp](src/NKMemory/NkFunctionSIMD.h) : variantes SIMD
  des fonctions ci-dessus (skeleton — voir TODO)
- [NkUtils.h/.cpp](src/NKMemory/NkUtils.h) : `NkAlignUp`, `NkIsPowerOfTwo`,
  `NkMemCopy`, etc.

### Interop
- [NkStlAdapter.h](src/NKMemory/NkStlAdapter.h) : adaptateur pour utiliser
  un `NkAllocator` dans `std::vector`, `std::unordered_map`, etc. via
  `allocator_traits`
- [NkGlobalOperators.cpp](src/NKMemory/NkGlobalOperators.cpp) : override des
  `operator new` / `operator delete` globaux (à compiler UNIQUEMENT dans
  l'exécutable, pas dans les bibliothèques — règle ODR documentée dans le
  Readme)

### Tests (18 fichiers)
`tests/` : benchmarks (allocators, allocator_vs_stl, container_allocator),
tests fonctionnels (allocator_arena, allocator_buddy, allocator_container,
allocator_freelist, allocator_linear, allocator_malloc, allocator_pool,
allocator_stack, allocator_virtual, core_utility_smoke, gc, hash_memory,
memory_fn, memory_stress, memory_system, memory_utils)

---

## 🔄 En cours / TODO immédiat

### Allocateurs spécialisés à exposer dans `src/`
Plusieurs tests existent (`test_allocator_arena.cpp`,
`test_allocator_buddy.cpp`, `test_allocator_freelist.cpp`,
`test_allocator_linear.cpp`, `test_allocator_stack.cpp`,
`test_allocator_virtual.cpp`) mais les classes correspondantes ne sont
**pas** présentes en tant que headers publics dans `src/NKMemory/` (seuls
`NkAllocator.cpp`, `NkPoolAllocator.cpp`, `NkContainerAllocator.cpp`,
`NkMultiLevelAllocator.cpp` exposent leurs types). À auditer :

- `NkArenaAllocator` — allocation linéaire incrémentale, free global, parfait
  pour frame-based / scope-based
- `NkLinearAllocator` — variante minimale de l'arena, sans bookkeeping
- `NkStackAllocator` — LIFO avec markers, idéal pour scopes imbriqués
- `NkFreeListAllocator` — free-list générique pour tailles variables
- `NkBuddyAllocator` — buddy system pour gros blocs (pages mémoire)
- `NkVirtualAllocator` — `VirtualAlloc` (Windows) / `mmap` (POSIX) avec
  `NkMemoryFlag` (READ/WRITE/EXECUTE/RESERVE/COMMIT)

Effort estimé : M par allocateur, L total si les 6 sont exposés en tant que
classes publiques.

### `NkFunctionSIMD` implémentation
- Le squelette est livré mais les paths SIMD effectifs (AVX2 memcpy, SSE2
  memset, NEON copy) ne sont pas implémentés ou marqués TODO. Effort : M.
  À corréler avec NkMath/NkSIMD (mêmes intrinsics).

### Robustesse threading
- Le module utilise `NkSpinLock` partout — bon pour les sections courtes.
  Mais sous très haute contention (multi-thread renderer en attente du job
  system NKThreading), un lock-free SLAB allocator pourrait améliorer le
  throughput. Effort : XL. Priorité : Moyenne (à reconsidérer quand
  NKThreading existe).

### Audit thread-safety GC
- `NkGarbageCollector::Collect()` bloque tous les threads pendant l'exécution
  (stop-the-world). Acceptable pour démarrage / shutdown, problématique pour
  hot path. À reconcevoir en concurrent / incremental GC si pression CPU
  observée. Priorité basse tant que pas mesuré.

---

## ❌ À venir / À ajouter (futur proche)

### Allocateur frame / double-buffered
- Pas livré : `NkFrameAllocator` qui se reset automatiquement à chaque
  `EndFrame()`. Le Readme.md décrit le pattern mais avec un
  `NkContainerAllocator::Reset()` manuel. Effort : S (wrapper autour d'arena).

### Allocateur slab / size-class lock-free
- Pour scaler à 10k+ allocations/frame multithread (NkRenderer NkVSM v2
  prévoit `Dynamic offsets UBO` pour 10k+ draws). Effort : XL.

### Allocateur GPU-side helper
- Côté NKMemory CPU, manquent les helpers pour gérer des "ring buffers"
  CPU staging GPU (NkRenderer en a déjà un implémenté en interne dans
  `NkRingBufferUbo`). À promouvoir si réutilisable. Effort : M.

### Profiler intégré aux outils externes
- Export Tracy / Optick / Chrome Tracing JSON pour visualisation des
  allocations. Aujourd'hui seul `DumpLeaks()` texte est disponible. Effort : M.

### Validation / sanity hooks debug
- Pattern de fill (DEADBEEF, FEEDFACE) pour memory poison en debug —
  à exposer si pas déjà dans `NkSecureZero`. Effort : S.

### Documentation publique
- `Readme.md` fait ~3000 lignes (énorme). Un overview court + un fichier
  `CHANGELOG.md` officiel manqueraient. Effort : S.

---

## Bugs / quirks connus

- Plusieurs fichiers tests référencent des classes (`NkArenaAllocator`,
  `NkBuddyAllocator`, `NkLinearAllocator`, `NkStackAllocator`,
  `NkFreeListAllocator`, `NkVirtualAllocator`) qui ne semblent pas exposées
  publiquement dans `src/NKMemory/*.h` — soit elles sont déclarées en
  interne dans un autre header, soit l'audit du module a un trou. À clarifier
  avant de promettre une matrice complète d'allocateurs.
- `NkGlobalOperators.cpp` doit être compilé **uniquement** dans l'exécutable
  final, pas dans les libs (ODR violation). Documenté mais facile à se
  tromper en CMake.
- `NkMemoryTracker` ajoute ~50-100 cycles/alloc en debug. En release,
  `NKENTSEU_DISABLE_MEMORY_TRACKING` doit être défini pour viser le perf
  baseline (~5-12 ns).
- L'accès `NkMemorySystem::Instance().mTracker.Register(info)` dans les
  exemples du Readme casse l'encapsulation (membre privé) — à exposer
  proprement ou retirer de la doc.

---

## Dépendances

- **Couches en dessous (utilisées)** : NKPlatform (export, alignment macros),
  NKCore (types `nk_size`/`nk_uint64`/`nk_char`, `NkSpinLock` via
  `NkAtomic.h`, traits pour perfect forwarding, asserts)
- **Modules au-dessus qui en dépendent** : NKContainers (toute classe
  accepte un `NkAllocator*` custom), NKMath (pas directement, mais via les
  options de couleur random), NKRHI (allocation handles), NKRenderer
  (ring buffers, descriptor pools), tous les services moteur, application
  framework Nkentseu/Core
