# NKThreading — documentation détaillée

Le module **NKThreading**, partie par partie. Pour une vue d'ensemble et un guide « par où
commencer », voir le récap : [../NKThreading.md](../NKThreading.md).

Chaque page suit la même structure : un **tutoriel** narratif, un **aperçu** tabulaire de
toute l'API, puis une **référence-cours** où chaque élément est expliqué avec son comportement
réel et ses pièges (réentrance, deadlock, guards move-only, primitives non-déplaçables).

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [Threads.md](Threads.md) | Lancer un thread OS (`NkThread` : `Join`/`Detach`, nom/affinité/priorité), stockage par-thread (`NkThreadLocal<T>`), distribuer des tâches sur un pool (`NkThreadPool` : `Enqueue`, `ParallelFor`, `GetGlobal`). | `NkThread.h`, `NkThreadLocal.h`, `NkThreadPool.h` |
| [Mutexes.md](Mutexes.md) | Protéger une donnée partagée : `NkMutex` (non-réentrant, `TryLockFor`), `NkRecursiveMutex` (réentrant), `NkSharedMutex`/`NkReaderWriterLock` (RW, writer-preferring), `NkSpinLock` (attente active) et les guards RAII (`NkScopedLock<T>`/`NkLockGuard`, `NkScopedLockMutex`, `NkScopedSpinLock`). | `NkMutex.h`, `NkRecursiveMutex.h`, `NkSharedMutex.h`, `NkSpinLock.h`, `NkScopedLock.h` |
| [Synchronization.md](Synchronization.md) | Faire attendre et réveiller des threads : variable de condition, sémaphore borné, barrière réutilisable, event (manual/auto reset), latch (compte à rebours), reader-writer lock. | `NkConditionVariable.h`, `NkSemaphore.h`, `Synchronization/NkBarrier.h`, `Synchronization/NkEvent.h`, `Synchronization/NkLatch.h`, `Synchronization/NkReaderWriterLock.h` |
| [Futures.md](Futures.md) | Résultat asynchrone producteur/consommateur : `NkFuture<T>` (lecture, copiable), `NkPromise<T>` (écriture, move-only), spécialisation `NkFuture<void>`, handle d'exception opaque. | `NkFuture.h`, `NkPromise.h` |

[← Récap NKThreading](../NKThreading.md) · [← Couche System](../README.md)
