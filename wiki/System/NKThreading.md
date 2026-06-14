# NKThreading

> Couche **System** · La concurrence du moteur : threads, stockage thread-local, pool de
> threads, mutex (récursif / partagé / spin), verrous RAII scoped, primitives de
> synchronisation (variable de condition, sémaphore, barrière, event, latch, RW lock) et
> futures / promesses — le tout en zéro-STL.

Dès qu'un calcul doit s'exécuter **en parallèle** — découper une boucle sur plusieurs cœurs,
faire dialoguer un producteur et un consommateur, protéger une ressource partagée — c'est
NKThreading qui fournit les briques. Le module remplace `<thread>`, `<mutex>`,
`<condition_variable>`, `<future>` et `<atomic>` (pour les verrous) par des classes maison
RAII, `noexcept` et sans allocation dynamique après construction. Tous les verrous de base
sont **non-copiables et non-déplaçables** : on les détient via des *guards* scoped qui
verrouillent à la construction et déverrouillent à la destruction.

Le **namespace principal** est `nkentseu::threading`. Deux exceptions importantes : les
spin-locks (`NkSpinLock`, `NkScopedSpinLock`) vivent dans `nkentseu::memory` (considérés comme
bas niveau atomique). De nombreux symboles ont en plus un **alias legacy déprécié** dans le
namespace parent `nkentseu::` (zéro overhead, résolu à la compilation) — préférez toujours la
forme `nkentseu::threading::`.

- **Namespace** : `nkentseu::threading` (spin-locks : `nkentseu::memory`)
- **Header parapluie** : `#include "NKThreading/NKThreading.h"`

---

## Par où commencer

Selon ce que vous cherchez à faire :

| Besoin | Page |
|--------|------|
| Lancer un thread, donner un nom / une affinité, stockage par-thread | [Les threads](NKThreading/Threads.md) |
| Distribuer N tâches sur les cœurs, paralléliser une boucle | [Les threads](NKThreading/Threads.md) |
| Protéger une donnée partagée (mutex, récursif, spin), verrou RAII | [Les mutex](NKThreading/Mutexes.md) |
| Autoriser plusieurs lecteurs mais un seul rédacteur (RW lock) | [Les mutex](NKThreading/Mutexes.md) · [Synchronisation](NKThreading/Synchronization.md) |
| Faire attendre / réveiller des threads (condition, event) | [Synchronisation](NKThreading/Synchronization.md) |
| Limiter l'accès à K ressources (sémaphore) | [Synchronisation](NKThreading/Synchronization.md) |
| Rendez-vous de N threads (barrière) ou attente d'un compte (latch) | [Synchronisation](NKThreading/Synchronization.md) |
| Récupérer le résultat d'un calcul asynchrone (future / promesse) | [Futures & promesses](NKThreading/Futures.md) |

Chaque page suit la même structure : un **tutoriel** narratif, un **aperçu** tabulaire de
toute l'API, puis une **référence-cours** où chaque élément est expliqué avec son
comportement réel, ses pièges (réentrance, deadlock, move-only) et ses cas d'usage concrets.

---

## Aperçu des familles

- **Threads** (`NkThread.h`, `NkThreadLocal.h`, `NkThreadPool.h`) — `NkThread` (wrapper RAII
  move-only d'un thread OS natif : `Join`/`Detach`, `SetName`/`SetAffinity`/`SetPriority`),
  `NkThreadLocal<T>` (stockage par-thread lock-free à init lazy) et `NkThreadPool` (workers,
  `Enqueue`, `ParallelFor`, singleton `GetGlobal()`).
- **Mutex & verrous** (`NkMutex.h`, `NkRecursiveMutex.h`, `NkSharedMutex.h`, `NkSpinLock.h`,
  `NkScopedLock.h`) — `NkMutex` (non-réentrant, `TryLockFor`), `NkRecursiveMutex` (réentrant),
  `NkReaderWriterLock` (alias `NkSharedMutex`, writer-preferring), `NkSpinLock` (attente
  active < 100 cycles, dans `memory`) et les guards RAII `NkScopedLock<T>` / `NkLockGuard`,
  `NkScopedLockMutex`, `NkScopedSpinLock`, `NkReadLock` / `NkWriteLock`.
- **Synchronisation** (`NkConditionVariable.h`, `NkSemaphore.h`, `Synchronization/NkBarrier.h`,
  `Synchronization/NkEvent.h`, `Synchronization/NkLatch.h`,
  `Synchronization/NkReaderWriterLock.h`) — `NkConditionVariable` (`Wait`/`WaitFor`/`WaitUntil`,
  `NotifyOne`/`NotifyAll`), `NkSemaphore` (compteur borné), `NkBarrier` (rendez-vous
  réutilisable), `NkEvent` (manual/auto reset, `Set`/`Reset`/`Pulse`), `NkLatch` (compte à
  rebours à usage unique) et le RW lock.
- **Futures & promesses** (`NkFuture.h`, `NkPromise.h`) — `NkFuture<T>` (handle de lecture
  copiable, `Get`/`Wait`/`WaitFor`/`IsReady`), `NkPromise<T>` (handle d'écriture move-only,
  `SetValue`/`SetException`/`GetFuture`), spécialisation `NkFuture<void>`.

---

## Index des headers

| Header | Contenu | Documenté dans |
|--------|---------|----------------|
| `NKThreading.h` | Parapluie (inclut tout). | — |
| `NkThread.h` | `NkThread` (thread OS natif RAII). | [Threads](NKThreading/Threads.md) |
| `NkThreadLocal.h` | `NkThreadLocal<T>` (stockage par-thread). | [Threads](NKThreading/Threads.md) |
| `NkThreadPool.h` | `NkThreadPool` (`Enqueue`, `ParallelFor`, `GetGlobal`). | [Threads](NKThreading/Threads.md) |
| `NkMutex.h` | `NkMutex`, `NkRecursiveMutex`, `NkScopedLockMutex`. | [Mutex](NKThreading/Mutexes.md) |
| `NkRecursiveMutex.h` | Alias legacy `nkentseu::NkRecursiveMutex`. | [Mutex](NKThreading/Mutexes.md) |
| `NkScopedLock.h` | `NkScopedLock<T>`, `NkLockGuard`. | [Mutex](NKThreading/Mutexes.md) |
| `NkSpinLock.h` | `NkSpinLock`, `NkScopedSpinLock` (`memory`). | [Mutex](NKThreading/Mutexes.md) |
| `NkSharedMutex.h` | Alias `NkSharedMutex`/`NkSharedLock`/`NkUniqueLock`. | [Mutex](NKThreading/Mutexes.md) · [Synchro](NKThreading/Synchronization.md) |
| `NkConditionVariable.h` | `NkConditionVariable`. | [Synchronisation](NKThreading/Synchronization.md) |
| `NkSemaphore.h` | `NkSemaphore`. | [Synchronisation](NKThreading/Synchronization.md) |
| `Synchronization/NkBarrier.h` | `NkBarrier`. | [Synchronisation](NKThreading/Synchronization.md) |
| `Synchronization/NkEvent.h` | `NkEvent`. | [Synchronisation](NKThreading/Synchronization.md) |
| `Synchronization/NkLatch.h` | `NkLatch`. | [Synchronisation](NKThreading/Synchronization.md) |
| `Synchronization/NkReaderWriterLock.h` | `NkReaderWriterLock`, `NkReadLock`, `NkWriteLock`. | [Mutex](NKThreading/Mutexes.md) · [Synchro](NKThreading/Synchronization.md) |
| `NkFuture.h` | `NkFuture<T>`, `NkPromise<T>`, `NkFuture<void>`, `NkExceptionHandle`. | [Futures](NKThreading/Futures.md) |
| `NkPromise.h` | Alias legacy `nkentseu::NkFuture`/`NkPromise`. | [Futures](NKThreading/Futures.md) |
| `NkThreadingApi.h` | Macro d'export `NKENTSEU_THREADING_CLASS_EXPORT`. | — |

---

[← Couche System](README.md) · [Index du wiki](../README.md)
