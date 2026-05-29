# NKThreading — Roadmap

État actuel (mai 2026) : Module mature, version interne 2.0.0. Toutes les
primitives classiques sont livrées (mutex, spinlock, condition variables,
sémaphores, shared mutex, barrier, latch, event, RW lock, thread, thread-local,
thread pool, future/promise). Le pool de threads expose Enqueue/Join/Shutdown
mais l'algorithme work-stealing annoncé reste partiellement câblé (priorité et
affinité CPU sont stubs). Pas encore de Job System multi-fibers ni de
parallel_invoke.

---

## Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| Primitives de base (NkMutex, NkRecursiveMutex, NkSpinLock) | Livré | — | — |
| Synchronisation avancée (CV, Semaphore, SharedMutex) | Livré | — | — |
| Synchronisation de phase (Barrier, Latch, Event) | Livré | — | — |
| Reader-Writer Lock + guards RAII | Livré | — | — |
| NkThread + NkThreadLocal<T> | Livré | — | — |
| NkFuture<T> / NkPromise<T> | Livré | — | — |
| NkScopedLock<T> guard générique | Livré | — | — |
| Utilitaires (Yield, Pause, GetHardwareConcurrency) | Livré | — | — |
| NkThreadPool — API enqueue/join/shutdown | Livré | — | — |
| NkThreadPool — Work-stealing réel | Partiel | M | Haute |
| NkThreadPool — Priority queue | Partiel (stub) | M | Moyenne |
| NkThreadPool — Affinité CPU | Partiel (stub) | M | Basse |
| ParallelFor template | Livré | — | — |
| Tests (smoke, sync, threadpool, semaphore) | Livré | — | — |
| NkJobSystem / NkFiberPool | TODO | XL | Moyenne |
| ParallelInvoke / ParallelReduce / ParallelTransform | TODO | M | Moyenne |
| Coroutines C++20 (NkTask<T>, co_await) | TODO | L | Basse |
| Metrics / introspection (queue depth, contention stats) | TODO | M | Moyenne |

Légende : Livré · Partiel · En cours · TODO · Abandonné

---

## Livré

### Primitives de base
- [NkMutex](src/NKThreading/NkMutex.h) : SRWLock (Win) / pthread_mutex (POSIX).
- [NkRecursiveMutex](src/NKThreading/NkRecursiveMutex.h) : alias legacy +
  variante réentrante (overhead 2-3x).
- [NkSpinLock](src/NKThreading/NkSpinLock.h) : spin-lock atomique avec PAUSE
  hint x86/ARM yield.
- [NkScopedLock](src/NKThreading/NkScopedLock.h) : guard RAII templatisé
  (duck-typing sur Lock()/Unlock()).

### Synchronisation avancée
- [NkConditionVariable](src/NKThreading/NkConditionVariable.h) : Wait /
  WaitUntil / NotifyOne / NotifyAll.
- [NkSemaphore](src/NKThreading/NkSemaphore.h) : compteur de ressources +
  TryAcquire avec timeout.
- [NkSharedMutex](src/NKThreading/NkSharedMutex.h) : alias `NkReaderWriterLock`.

### Synchronisation de phase
- [NkBarrier](src/NKThreading/Synchronization/NkBarrier.h) : barrière
  réutilisable type C++20 std::barrier.
- [NkLatch](src/NKThreading/Synchronization/NkLatch.h) : compteur décrémental
  à usage unique avec timeout.
- [NkEvent](src/NKThreading/Synchronization/NkEvent.h) : style Win32
  (ManualReset / AutoReset).
- [NkReaderWriterLock](src/NKThreading/Synchronization/NkReaderWriterLock.h)
  avec guards `NkReadLock` / `NkWriteLock`.

### Threads natifs et patterns async
- [NkThread](src/NKThreading/NkThread.h) : wrapper pthread/Win32, Join, Detach,
  GetCurrentThreadId.
- [NkThreadLocal<T>](src/NKThreading/NkThreadLocal.h) : stockage thread-local
  templatisé.
- [NkFuture<T>](src/NKThreading/NkFuture.h) / [NkPromise<T>](src/NKThreading/NkPromise.h)
  : producteur/consommateur async typé.

### Thread Pool
- [NkThreadPool](src/NKThreading/NkThreadPool.h) : pool avec workers
  configurable (0 = auto-détection), Pimpl idiom, API noexcept :
  - `Enqueue(Task)` — soumission FIFO.
  - `Join()` — attente complétion sans bloquer nouvelles soumissions.
  - `Shutdown()` — arrêt gracieux.
  - `ParallelFor<Func>(count, func, grainSize)` — découpage automatique.
  - `EnqueuePriority(task, priority)` — **priorité ignorée actuellement (stub)**.
  - `EnqueueAffinity(task, cpuCore)` — **affinité ignorée actuellement (stub)**.

### Utilitaires globaux
- `nkentseu::threading::Yield()` — SwitchToThread / sched_yield.
- `nkentseu::threading::Pause()` — PAUSE x86 / YIELD ARM.
- `nkentseu::threading::GetHardwareConcurrency()` — cores logiques.

### Tests
- [test_smoke.cpp](tests/test_smoke.cpp) : sanity checks primitives de base.
- [test_synchronization.cpp](tests/test_synchronization.cpp) : Barrier, Latch,
  Event, RW Lock.
- [test_threadpool_semaphore.cpp](tests/test_threadpool_semaphore.cpp) : pool
  enqueue/join + sémaphore.
- [benchmark_smoke.cpp](tests/benchmark_smoke.cpp) + dossier `bench/`.

---

## En cours / TODO immédiat

### NkThreadPool — finir le work-stealing réel
- Implémenter les **deques locales par worker** + steal aléatoire avec
  back-off exponentiel (Chase-Lev deque ou équivalent).
- Câbler `EnqueuePriority` : queue à 3 niveaux (high/normal/low) avec
  starvation prevention.
- Câbler `EnqueueAffinity` : si worker idle sur le core demandé, routage
  direct ; sinon fallback queue générale.
- Métriques internes : `GetQueueSize()`, `GetTasksCompleted()`, contention
  spinlock counter.

### Tests à étendre
- Tests de stress work-stealing : 1000+ tâches récursives, vérifier balance
  entre workers.
- Tests `NkFuture` chained (`then()`/continuation) si exposé.
- Tests stress `NkPromise::set_exception` quand exposé.

### Alias legacy à clean
Le header `NKThreading.h` expose des alias `nkentseu::NkMutex`, etc., marqués
`@deprecated`. Décider : retirer en v3.0 ou maintenir indéfiniment. Documenter
la deadline.

---

## À venir / À ajouter (futur proche)

### NkJobSystem — au-dessus du ThreadPool
Job System fibres-aware façon Naughty Dog / Unreal :
- `NkJob` avec dépendances DAG (`Job::DependsOn(other)`).
- Scheduler qui dispatche les jobs prêts vers le pool.
- Wait par fibre (yield au lieu de bloquer) — nécessite `boost::context` ou
  implémentation custom des fibres POSIX/ucontext + Win32 Fibers.
- Cas d'usage : animation, physique, ECS systems parallèles.

### Algorithmes parallèles génériques
- `NkParallelInvoke(f1, f2, f3, ...)` — n callables en parallèle, join à la
  fin.
- `NkParallelReduce(begin, end, init, reducer, combiner)` — fold parallèle.
- `NkParallelTransform(input, output, func)` — map parallèle.

### Coroutines C++20
- `NkTask<T>` avec `co_await` / `co_return`.
- Awaitable sur `NkFuture<T>`.
- Awaitable sur `NkThreadPool::Schedule()` pour migrer un coroutine sur le
  pool.
- Intégration avec un éventuel `NkAsyncIO` (NKNetwork, NKFileSystem
  asynchrone).

### Introspection et debug
- Hook profiler : `OnTaskStart/OnTaskEnd` pour timeline (Tracy / Optick).
- Détection de deadlock : graphe d'acquisition de locks instrumenté en debug.
- Wrappers debug (cf. Readme `DebugLatch` exemple) à packager officiellement.

### Plateformes additionnelles
- Validation Android / iOS / macOS au-delà des tests POSIX génériques.
- Affinité CPU sur Linux (`pthread_setaffinity_np`), Windows
  (`SetThreadAffinityMask`), macOS (`thread_policy_set`).

---

## Bugs / quirks connus
- `EnqueuePriority` et `EnqueueAffinity` sont des stubs : les paramètres sont
  ignorés silencieusement. Documenter ou changer en `[[deprecated]]` jusqu'à
  implémentation réelle.
- Le header `NKThreading.h` inclut `NkPromise.h` deux fois (sections 7 et 9).
  Doublon inoffensif (header guard) mais à nettoyer.

---

## Dépendances
- **Couches en dessous (utilisées)** : NKCore (Types, Atomic), NKContainers
  (Queue, Function), NKMemory (UniquePtr), NKPlatform (détection OS + arch +
  `NkGetLogicalCoreCount`).
- **Modules au-dessus qui en dépendent** : NKLogger (mutex, condvar, thread),
  NKFileSystem (FileWatcher), NKNetwork (sockets blocking), NKRenderer (ring
  buffer multi-frame, command submission), Runtime (frame loop), Unkeny
  (PluginSystem chargements concurrents).
