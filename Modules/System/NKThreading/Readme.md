# NKThreading

> **Module de primitives de threading et synchronisation haute performance**  
> *Multiplateforme • Zero-overhead • Production-ready*

[![Version](https://img.shields.io/badge/version-2.0.0-blue.svg)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-Proprietary-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS%20%7C%20Android%20%7C%20iOS-lightgrey.svg)](#support-multiplateforme)
[![C++ Standard](https://img.shields.io/badge/c%2B%2B-17-orange.svg)](#configuration-requise)

---

## 📋 Table des matières

- [🎯 Vue d'ensemble](#-vue-densemble)
- [✨ Fonctionnalités clés](#-fonctionnalités-clés)
- [🚀 Démarrage rapide](#-démarrage-rapide)
- [📦 Installation et build](#-installation-et-build)
- [🧩 Architecture du module](#-architecture-du-module)
- [📚 Référence API par catégorie](#-référence-api-par-catégorie)
- [💡 Patterns et bonnes pratiques](#-patterns-et-bonnes-pratiques)
- [⚡ Considérations de performance](#-considérations-de-performance)
- [🌐 Support multiplateforme](#-support-multiplateforme)
- [🔄 Migration depuis l'ancien namespace](#-migration-depuis-lancien-namespace)
- [🔍 Dépannage et FAQ](#-dépannage-et-faq)
- [🤝 Contribuer](#-contribuer)
- [📄 Licence](#-licence)

---

## 🎯 Vue d'ensemble

**NKThreading** est un module C++ moderne fournissant des primitives de synchronisation et de parallélisme haute performance, conçues pour :

- ✅ **Simplicité d'utilisation** : API intuitive inspirée de la STL, avec guards RAII
- ✅ **Performance optimale** : Implémentations lock-free quand possible, zero-copy semantics
- ✅ **Sécurité thread-safe** : Garanties documentées, prévention des deadlocks par design
- ✅ **Portabilité maximale** : Abstraction native Windows/POSIX avec comportement uniforme
- ✅ **Maintenabilité** : Documentation Doxygen complète, tests unitaires, exemples réalistes

### Cas d'usage typiques

| Domaine | Exemple d'utilisation | Primitive recommandée |
|---------|----------------------|---------------------|
| **Serveur concurrent** | Handling de requêtes HTTP parallèles | `NkThreadPool` + `NkMutex` |
| **Simulation parallèle** | Time-stepping avec synchronisation de phase | `NkBarrier` |
| **Cache partagé** | Lectures fréquentes, mises à jour rares | `NkReaderWriterLock` |
| **Pipeline de traitement** | Étapes synchronisées avec agrégation | `NkLatch` + `NkThreadPool` |
| **Application temps-réel** | Sections critiques ultra-courtes | `NkSpinLock` |
| **Initialisation parallèle** | Attendre N services avant démarrage | `NkLatch` |
| **Signal de shutdown** | Arrêt gracieux de workers | `NkEvent` (ManualReset) |
| **Résultat asynchrone** | Fetch de données en background | `NkFuture`/`NkPromise` |

---

## ✨ Fonctionnalités clés

### 🔹 Primitives de synchronisation

| Primitive | Description | Performance | Thread-safety |
|-----------|-------------|-------------|---------------|
| `NkMutex` | Exclusion mutuelle standard | ⭐⭐⭐ | Exclusive |
| `NkSpinLock` | Spin-lock pour sections <100 cycles | ⭐⭐⭐⭐⭐ | Exclusive |
| `NkRecursiveMutex` | Mutex réentrant pour API imbriquées | ⭐⭐ | Exclusive + recursive |
| `NkConditionVariable` | Attente sur prédicat complexe | ⭐⭐⭐⭐ | Wait/Notify pattern |
| `NkSemaphore` | Compteur pour pools de ressources | ⭐⭐⭐ | Compteur avec limite |
| `NkSharedMutex` | Alias reader-writer (compatibilité STL) | ⭐⭐⭐⭐ | Multiple readers OR single writer |

### 🔹 Synchronisation de phase et événements

| Primitive | Réutilisable | Usage principal | Particularité |
|-----------|-------------|----------------|---------------|
| `NkBarrier` | ✅ Oui | Synchronisation de phase itérative | Tous threads progressent ensemble |
| `NkLatch` | ❌ Non | Attendre completion de N tâches | Compteur décrémente à zéro |
| `NkEvent` | ✅ Oui | Signal booléen entre threads | ManualReset/AutoReset modes |

### 🔹 Accès concurrent optimisé

| Type | Accès simultané | Usage recommandé |
|------|----------------|-----------------|
| `NkReaderWriterLock` | Multiple readers **OR** single writer | Cache, config, données read-heavy |
| `NkReadLock` (RAII) | Partagé avec autres readers | Lectures thread-safe automatiques |
| `NkWriteLock` (RAII) | Exclusif (exclut tous) | Modifications thread-safe automatiques |

### 🔹 Gestion de threads et parallélisme

| Composant | Description | Avantage clé |
|-----------|-------------|-------------|
| `NkThread` | Wrapper C++ de thread natif | API moderne, move semantics |
| `NkThreadLocal<T>` | Stockage isolé par thread | Zero overhead de synchronisation |
| `NkThreadPool` | Pool avec work-stealing | Parallélisme automatique, load-balancing |

### 🔹 Patterns asynchrones

| Composant | Rôle | Équivalent STL |
|-----------|------|---------------|
| `NkPromise<T>` | Producteur de résultat async | `std::promise` |
| `NkFuture<T>` | Consommateur de résultat async | `std::future` |
| `NkScopedLock<T>` | Guard RAII générique | `std::lock_guard` |

---

## 🚀 Démarrage rapide

### Installation minimale

```cpp
// 1. Inclure le master header
#include <NKThreading/NKThreading.h>

// 2. Utiliser le namespace moderne
using namespace nkentseu::threading;

// 3. Coder ! 🎉
```

### Exemple 1 : Mutex + Guard RAII (le plus courant)

```cpp
#include <NKThreading/NKThreading.h>

NkMutex mutex;
int sharedCounter = 0;

void SafeIncrement() {
    NkScopedLock guard(mutex);  // Acquisition automatique
    ++sharedCounter;             // Section critique protégée
}  // Déverrouillage automatique, même en cas d'exception
```

### Exemple 2 : Parallélisation avec Thread Pool

```cpp
#include <NKThreading/NKThreading.h>
#include <vector>

void ParallelProcessing() {
    NkThreadPool pool(4);  // 4 workers (ou 0 = auto-détection CPU)
    std::vector<float> data(10000);

    // Paralléliser une boucle avec chunks de 100 éléments
    pool.ParallelFor(data.size(), [&data](size_t i) {
        data[i] = ComputeValue(i);  // Thread-safe si ComputeValue l'est
    }, 100);  // grainSize = 100

    pool.Join();  // Attendre la fin de toutes les tâches
}
```

### Exemple 3 : Synchronisation de completion avec Latch

```cpp
#include <NKThreading/NKThreading.h>

void ParallelInitialization() {
    constexpr nk_uint32 kNumServices = 5;
    NkLatch startupLatch(kNumServices);  // Attendre 5 initialisations

    // Lancer les initialisations en parallèle
    for (nk_uint32 i = 0; i < kNumServices; ++i) {
        NkThread([i, &startupLatch]() {
            if (InitializeService(i)) {
                startupLatch.CountDown();  // Signaler la completion
            }
        });
    }

    // Attendre que tous les services soient prêts (avec timeout)
    if (!startupLatch.Wait(30000)) {  // 30 secondes max
        HandleStartupTimeout(startupLatch.GetCount());
    }
}
```

### Exemple 4 : Cache thread-safe avec Reader-Writer Lock

```cpp
#include <NKThreading/NKThreading.h>
#include <unordered_map>

template<typename Key, typename Value>
class ThreadSafeCache {
public:
    Optional<Value> Get(const Key& key) const {
        NkReadLock lock(mRwLock);  // ✅ Multiple readers OK
        auto it = mCache.find(key);
        return (it != mCache.end()) ? Optional<Value>(it->second) : nullopt;
    }

    void Insert(const Key& key, const Value& value) {
        NkWriteLock lock(mRwLock);  // ✅ Exclusive access
        mCache[key] = value;
    }

private:
    mutable NkReaderWriterLock mRwLock;
    mutable std::unordered_map<Key, Value> mCache;
};
```

---

## 📦 Installation et build

### Prérequis

| Dépendance | Version minimale | Note |
|------------|-----------------|------|
| **Compilateur C++** | GCC 7+, Clang 5+, MSVC 2017+ | Support C++17 requis |
| **CMake** | 3.15+ | Pour le build système |
| **Système d'exploitation** | Windows 10+, Linux (glibc 2.17+), macOS 10.14+, Android 5+, iOS 12+ | Support natif pthread/Win32 |

### Intégration via CMake

```cmake
# CMakeLists.txt de votre projet

# 1. Ajouter NKThreading comme submodule ou package
add_subdirectory(external/NKThreading)  # Si submodule
# OU
find_package(NKThreading REQUIRED CONFIG)  # Si package installé

# 2. Lier votre cible avec NKThreading
add_executable(mon_app main.cpp)
target_link_libraries(mon_app PRIVATE NKThreading::NKThreading)

# 3. (Optionnel) Configurer les options de build
option(NKTHREADING_BUILD_SHARED "Build as shared library" ON)
option(NKTHREADING_ENABLE_DEBUG_LOGGING "Enable debug logging" OFF)
```

### Build manuel (sans CMake)

```bash
# 1. Cloner le dépôt
git clone https://github.com/yourorg/NKThreading.git
cd NKThreading

# 2. Créer un dossier de build
mkdir build && cd build

# 3. Configurer avec CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# 4. Compiler
cmake --build . --config Release

# 5. (Optionnel) Installer
cmake --install . --prefix /usr/local
```

### Options de configuration CMake

| Option | Valeur par défaut | Description |
|--------|------------------|-------------|
| `NKTHREADING_BUILD_SHARED` | `ON` | Build en bibliothèque partagée (DLL/.so) |
| `NKTHREADING_BUILD_STATIC` | `OFF` | Build en bibliothèque statique (.lib/.a) |
| `NKTHREADING_ENABLE_DEBUG_LOGGING` | `OFF` | Activer le logging debug dans le module |
| `NKTHREADING_ENABLE_ASSERTS` | `ON` (Debug) / `OFF` (Release) | Activer les assertions NKENTSEU_ASSERT |
| `NKTHREADING_SANITIZE_THREADS` | `OFF` | Activer ThreadSanitizer pour le debugging |

### Variables d'environnement utiles

```bash
# Forcer l'utilisation de pthread sur Linux (si détection automatique échoue)
export NKENTSEU_FORCE_PTHREAD=1

# Activer le logging debug pour tous les composants NKThreading
export NKENTSEU_THREADING_DEBUG=1

# Définir le nombre de workers par défaut pour le pool global
export NKENTSEU_THREADPOOL_DEFAULT_WORKERS=8
```

---

## 🧩 Architecture du module

### Structure des fichiers

```
include/NKThreading/
├── NKThreading.h                 ← Master header (point d'entrée unique)
├── NkThreadingApi.h             ← Macros d'export et configuration
│
├── Primitives de base
│   ├── NkMutex.h                ← Mutex standard
│   ├── NkSpinLock.h             ← Spin-lock haute performance
│   └── NkRecursiveMutex.h       ← Mutex réentrant
│
├── Synchronisation avancée
│   ├── NkConditionVariable.h    ← Attente sur prédicat
│   ├── NkSemaphore.h            ← Sémaphore compteur
│   └── NkSharedMutex.h          ← Alias reader-writer
│
├── Synchronization/             ← Primitives spécialisées
│   ├── NkBarrier.h             ← Synchronisation de phase
│   ├── NkLatch.h               ← Completion de tâches
│   ├── NkEvent.h               ← Signal booléen
│   └── NkReaderWriterLock.h    ← Verrou lecteur-rédacteur
│
├── Gestion de threads
│   ├── NkThread.h              ← Wrapper thread natif
│   ├── NkThreadLocal.h         ← Stockage thread-local
│   └── NkThreadPool.h          ← Pool avec work-stealing
│
├── Patterns async
│   ├── NkFuture.h              ← Consommateur async
│   ├── NkPromise.h             ← Producteur async
│   └── NkScopedLock.h          ← Guard RAII générique
│
└── Implémentations (.cpp)      ← Fichiers sources (non inclus dans l'API publique)
```

### Pattern Pimpl (Pointer to Implementation)

Pour certains composants (`NkThreadPool`, `NkThread`), nous utilisons le pattern **Pimpl** pour :

- 🔒 **Isoler les dépendances** : Les inclusions système (`windows.h`, `pthread.h`) restent dans les `.cpp`
- ⚡ **Réduire les temps de compilation** : Modification de l'implémentation ≠ recompilation des utilisateurs
- 🔄 **Maintenir une ABI stable** : Changements internes sans breaking change binaire

```cpp
// Header public (léger) :
class NkThreadPool {
    memory::NkUniquePtr<NkThreadPoolImpl> mImpl;  // Pointeur opaque
public:
    void Enqueue(Task task) noexcept;  // Déclarations uniquement
};

// Implementation privée (détaillée, dans .cpp) :
class NkThreadPoolImpl {
    NkVector<NkThread> mWorkers;  // Détails cachés
    NkQueue<Task> mQueue;
    // ... autres membres privés ...
};
```

---

## 📚 Référence API par catégorie

### 🔹 Primitives de synchronisation de base

#### `NkMutex` - Exclusion mutuelle standard

```cpp
class NkMutex {
public:
    NkMutex() noexcept;
    ~NkMutex() noexcept;

    void Lock() noexcept;                    // Bloquant
    [[nodiscard]] nk_bool TryLock() noexcept; // Non-bloquant
    void Unlock() noexcept;

    // Non-copiable, non-déplaçable
    NkMutex(const NkMutex&) = delete;
    NkMutex& operator=(const NkMutex&) = delete;
};
```

**Usage typique** :
```cpp
NkMutex mutex;
{
    NkScopedLock guard(mutex);  // RAII : acquisition/déverrouillage automatiques
    // Section critique protégée
}  // Déverrouillage automatique ici
```

#### `NkSpinLock` - Spin-lock haute performance

```cpp
class NkSpinLock {
public:
    NkSpinLock() noexcept;
    ~NkSpinLock() = default;

    void Lock() noexcept;                    // Avec pause instructions + backoff
    [[nodiscard]] nk_bool TryLock() noexcept;
    void Unlock() noexcept;
    [[nodiscard]] nk_bool IsLocked() const noexcept;
};
```

**Quand l'utiliser** :
- ✅ Sections critiques < 100 cycles CPU
- ✅ Faible contention attendue
- ✅ Code temps-réel où les syscalls sont interdits
- ❌ Sections longues (>1µs) → préférer `NkMutex`

#### `NkRecursiveMutex` - Mutex réentrant

```cpp
class NkRecursiveMutex {
public:
    NkRecursiveMutex() noexcept;
    ~NkRecursiveMutex() noexcept;

    void Lock() noexcept;
    [[nodiscard]] nk_bool TryLock() noexcept;
    void Unlock() noexcept;
};
```

**Cas d'usage** : API publique appelant des méthodes internes déjà protégées.

> ⚠️ **Attention** : Overhead de 2-3x vs `NkMutex`. Utiliser uniquement si nécessaire.

---

### 🔹 Synchronisation avancée

#### `NkConditionVariable` - Attente sur prédicat

```cpp
class NkConditionVariable {
public:
    NkConditionVariable() noexcept;
    ~NkConditionVariable() noexcept;

    void Wait(NkScopedLock& lock) noexcept;
    [[nodiscard]] nk_bool WaitFor(NkScopedLock& lock, nk_uint32 ms) noexcept;
    template<typename Predicate>
    void WaitUntil(NkScopedLock& lock, Predicate&& pred) noexcept;

    void NotifyOne() noexcept;   // Réveille un thread
    void NotifyAll() noexcept;   // Réveille tous les threads
};
```

**Pattern recommandé** :
```cpp
NkMutex mutex;
NkConditionVariable cond;
bool dataReady = false;

// Thread producteur
{
    NkScopedLock lock(mutex);
    data = ComputeData();
    dataReady = true;
}
cond.NotifyOne();  // Notifier hors section critique (optimisation)

// Thread consommateur
{
    NkScopedLock lock(mutex);
    cond.WaitUntil(lock, []() { return dataReady; });  // Prédicat pour spurious wakeups
    ProcessData(data);
}
```

#### `NkSemaphore` - Compteur de ressources

```cpp
class NkSemaphore {
public:
    explicit NkSemaphore(nk_uint32 initial = 0, nk_uint32 max = 0xFFFFFFFF) noexcept;
    ~NkSemaphore() = default;

    void Acquire() noexcept;                              // Bloquant
    [[nodiscard]] nk_bool TryAcquire() noexcept;          // Non-bloquant
    [[nodiscard]] nk_bool TryAcquireFor(nk_uint32 ms) noexcept;

    [[nodiscard]] nk_bool Release(nk_uint32 count = 1) noexcept;
    [[nodiscard]] nk_uint32 GetCount() const noexcept;
};
```

**Cas d'usage** : Pool de connexions, limite de concurrence, producteur-consommateur borné.

---

### 🔹 Synchronisation de phase et événements

#### `NkBarrier` - Synchronisation de phase itérative

```cpp
class NkBarrier {
public:
    explicit NkBarrier(nk_uint32 numThreads) noexcept;

    [[nodiscard]] nk_bool Wait() noexcept;  // Retourne true pour le dernier thread
    void Reset() noexcept;                  // Forcer la completion (usage avancé)
};
```

**Pattern classique** :
```cpp
NkBarrier barrier(4);  // 4 threads

// Chaque thread :
for (int phase = 0; phase < NUM_PHASES; ++phase) {
    ComputeLocalData(phase);    // Phase 1 : calcul local
    barrier.Wait();             // Sync avant échange
    ExchangeBoundaryData();     // Phase 2 : échange avec voisins
    barrier.Wait();             // Sync avant prochaine itération
}
```

#### `NkLatch` - Completion de N tâches (usage unique)

```cpp
class NkLatch {
public:
    explicit NkLatch(nk_uint32 initialCount) noexcept;

    void CountDown(nk_uint32 count = 1) noexcept;  // Décrémenter
    [[nodiscard]] nk_bool Wait(nk_int32 timeoutMs = -1) noexcept;
    [[nodiscard]] nk_bool IsReady() const noexcept;
    [[nodiscard]] nk_uint32 GetCount() const noexcept;
};
```

**Différence clé vs Barrier** : `NkLatch` est à **usage unique** (pas de Reset() pour réutilisation).

#### `NkEvent` - Signal booléen (style Win32)

```cpp
class NkEvent {
public:
    explicit NkEvent(nk_bool manualReset = false, nk_bool initialState = false) noexcept;

    void Set() noexcept;      // Signaler
    void Reset() noexcept;    // Remettre à zéro
    void Pulse() noexcept;    // Signal éphémère (réveille uniquement waiters actuels)

    [[nodiscard]] nk_bool Wait(nk_int32 timeoutMs = -1) noexcept;
    [[nodiscard]] nk_bool IsSignaled() const noexcept;
};
```

**Modes** :
| Mode | Comportement de `Set()` | Usage typique |
|------|------------------------|---------------|
| `ManualReset=true` | Signal persiste jusqu'à `Reset()` | Flags de shutdown, initialisation |
| `AutoReset=false` (défaut) | Signal consommé par premier `Wait()` | Handoff de ressource, notification un-à-un |

---

### 🔹 Accès concurrent : Reader-Writer Lock

#### `NkReaderWriterLock` + Guards RAII

```cpp
class NkReaderWriterLock {
public:
    NkReaderWriterLock() noexcept;

    // Interface lecture (partagée)
    void LockRead() noexcept;
    [[nodiscard]] nk_bool TryLockRead() noexcept;
    void UnlockRead() noexcept;

    // Interface écriture (exclusive)
    void LockWrite() noexcept;
    [[nodiscard]] nk_bool TryLockWrite() noexcept;
    void UnlockWrite() noexcept;
};

// Guards RAII (recommandés)
class NkReadLock {
public:
    explicit NkReadLock(NkReaderWriterLock& lock) noexcept;  // Acquiert LockRead()
    ~NkReadLock() noexcept;                                   // Libère UnlockRead()
};

class NkWriteLock {
public:
    explicit NkWriteLock(NkReaderWriterLock& lock) noexcept;  // Acquiert LockWrite()
    ~NkWriteLock() noexcept;                                   // Libère UnlockWrite()
};
```

**Politique d'ordonnancement** :
- ✅ **Prévention de starvation des writers** : Les nouveaux `LockRead()` bloquent si des writers attendent
- ✅ **Lectures concurrentes maximales** : Tous les readers actifs peuvent lire simultanément
- ✅ **Notification sélective** : `NotifyAll()` pour readers, `NotifyOne()` pour writers

---

### 🔹 Gestion de threads et parallélisme

#### `NkThreadPool` - Pool avec work-stealing

```cpp
class NkThreadPool {
public:
    explicit NkThreadPool(nk_uint32 numWorkers = 0) noexcept;  // 0 = auto-détection CPU
    ~NkThreadPool() noexcept;  // Appel automatique à Shutdown()

    // Soumission de tâches
    void Enqueue(Task task) noexcept;
    void EnqueuePriority(Task task, nk_int32 priority = 0) noexcept;  // Stub pour extension
    void EnqueueAffinity(Task task, nk_uint32 cpuCore) noexcept;      // Stub pour extension

    // Parallélisation de boucle
    template<typename Func>
    void ParallelFor(nk_size count, Func&& func, nk_size grainSize = 1) noexcept;

    // Contrôle
    void Join() noexcept;           // Attendre completion
    void Shutdown() noexcept;       // Arrêt gracieux

    // Métriques
    [[nodiscard]] nk_uint32 GetNumWorkers() const noexcept;
    [[nodiscard]] nk_size GetQueueSize() const noexcept;
    [[nodiscard]] nk_uint64 GetTasksCompleted() const noexcept;

    // Singleton global (pratique pour accès rapide)
    static NkThreadPool& GetGlobal() noexcept;
};
```

**Choix du grainSize pour `ParallelFor`** :
```cpp
// Règle empirique : grainSize = max(1, count / (numWorkers * 4))
// • Petit (1-10) : meilleur équilibrage, plus d'overhead de scheduling
// • Grand (100-1000) : moins d'overhead, risque de déséquilibre si tâches hétérogènes

pool.ParallelFor(10000, [](size_t i) { Process(i); }, 250);  // 40 tâches de 250 itérations
```

---

## 💡 Patterns et bonnes pratiques

### ✅ Checklist avant de soumettre du code

#### Synchronisation de base
- [ ] Utiliser `NkScopedLock` plutôt que `Lock()`/`Unlock()` manuels
- [ ] Choisir `NkSpinLock` uniquement pour sections <100 cycles
- [ ] Utiliser `WaitUntil()` avec prédicat pour éviter les spurious wakeups
- [ ] Minimiser la durée de détention des locks

#### Synchronisation de phase
- [ ] `NkBarrier` pour phases itératives réutilisables
- [ ] `NkLatch` pour completion unique de N tâches
- [ ] `NkEvent` pour signaux booléens persistants ou consommables
- [ ] Toujours appeler `Wait()`/`CountDown()` le même nombre de fois par thread

#### Accès concurrent
- [ ] `NkReadLock` pour lectures (partagé avec autres readers)
- [ ] `NkWriteLock` uniquement pour modifications (exclusif)
- [ ] Éviter les conversions read→write sur le même lock (risque de deadlock)
- [ ] Préparer les données hors section critique quand possible

#### Parallélisme
- [ ] Choisir un `grainSize` adapté dans `ParallelFor` (100-1000 typiquement)
- [ ] Vérifier que la fonction parallélisée est thread-safe
- [ ] Utiliser `Join()` avant d'accéder aux résultats partagés
- [ ] Gérer les exceptions dans les tâches soumises au pool

#### Gestion des threads
- [ ] Toujours `Join()` ou `Detach()` avant destruction d'un `NkThread`
- [ ] Ne pas appeler `Join()` depuis le thread lui-même (deadlock garanti)
- [ ] Utiliser `NkThreadLocal` pour données thread-specific, pas pour communication

### ❌ Anti-patterns à éviter absolument

```cpp
// ❌ Lock nested sans mutex réentrant = deadlock garanti
void BadNestedLock(NkMutex& mutex) {
    mutex.Lock();
    mutex.Lock();  // ❌ Deadlock !
    // ...
}

// ❌ ParallelFor avec grainSize=1 sur grande boucle = overhead massif
pool.ParallelFor(1000000, [](size_t i) { Process(i); }, 1);  // ❌ 1M tâches !

// ❌ Thread-local pour communication inter-threads = bug garanti
NkThreadLocal<int> shared;  // ❌ Chaque thread a sa propre copie !
*shared = 42;  // Un autre thread ne verra jamais cette valeur

// ❌ Oublier de libérer un lock sur exception = deadlock potentiel
void RiskyLock(NkMutex& mutex) {
    mutex.Lock();
    RiskyOperation();  // ❌ Si exception ici → mutex jamais libéré !
    mutex.Unlock();
}

// ❌ Wait() infini sans garantie de signal = hang potentiel
void RiskyWait(NkEvent& event) {
    event.Wait();  // ❌ Peut bloquer indéfiniment si personne n'appelle Set() !
}
```

### 🎯 Patterns réutilisables

#### Pattern 1 : Producer-Consumer avec ConditionVariable

```cpp
template<typename T>
class ThreadSafeQueue {
public:
    void Push(T item) {
        {
            NkScopedLock lock(mMutex);
            mQueue.push(std::move(item));
        }
        mCond.NotifyOne();  // Notifier hors section critique
    }

    bool Pop(T& out, nk_uint32 timeoutMs = 0) {
        NkScopedLock lock(mMutex);
        if (timeoutMs > 0) {
            if (!mCond.WaitFor(lock, timeoutMs)) return false;
        } else {
            mCond.WaitUntil(lock, [this]() { return !mQueue.empty(); });
        }
        if (mQueue.empty()) return false;
        out = std::move(mQueue.front());
        mQueue.pop();
        return true;
    }

private:
    NkMutex mMutex;
    NkConditionVariable mCond;
    std::queue<T> mQueue;
};
```

#### Pattern 2 : Copy-on-Write avec ReaderWriterLock

```cpp
template<typename Data>
class CopyOnWriteContainer {
public:
    // Lecture : accès partagé à la donnée courante
    std::shared_ptr<const Data> Read() const {
        NkReadLock lock(mRwLock);
        return mData;  // Copie du shared_ptr (atomique, léger)
    }

    // Écriture : création d'une nouvelle copie, puis swap atomique
    void Update(std::function<Data(const Data&)> transformer) {
        NkWriteLock lock(mRwLock);
        auto newData = std::make_shared<Data>(transformer(*mData));
        mData = std::move(newData);  // Swap atomique
    }

private:
    mutable NkReaderWriterLock mRwLock;
    std::shared_ptr<Data> mData;  // Pointeur vers donnée immuable courante
};
```

#### Pattern 3 : Shutdown gracieux avec Event

```cpp
class GracefulWorker {
public:
    void Start() {
        mThread = NkThread([this]() {
            while (!mShutdownEvent.IsSignaled()) {
                if (auto task = TryDequeue()) {
                    ExecuteTask(*task);
                } else {
                    mShutdownEvent.Wait(100);  // Polling avec timeout court
                }
            }
            Cleanup();  // Nettoyage avant sortie
        });
    }

    void Stop() {
        mShutdownEvent.Set();  // ManualReset : persiste pour tous les Wait()
        if (mThread.Joinable()) mThread.Join();
    }

private:
    NkThread mThread;
    NkEvent mShutdownEvent{true};  // ManualReset pour persistance du signal
};
```

---

## ⚡ Considérations de performance

### 🔹 Benchmarks indicatifs (x86_64, Linux, CPU moderne)

| Opération | Cycles CPU | Note |
|-----------|-----------|------|
| `NkMutex::Lock()` (lock libre) | ~25-50 | Avec futex optimisé |
| `NkMutex::Lock()` (contension) | ~200-1000 | Dépend du scheduler |
| `NkSpinLock::Lock()` (lock libre) | ~3-5 | Fast-path avec Exchange |
| `NkSpinLock::Lock()` (contension) | ~50-500 | Avec PAUSE + backoff |
| `NkConditionVariable::Wait()` | ~100-300 | Suspension kernel |
| `NkSemaphore::Acquire()` | ~30-80 | Similaire à mutex |
| `NkReaderWriterLock::LockRead()` | ~40-100 | Avec vérification writers |
| `NkReaderWriterLock::LockWrite()` | ~50-150 | Exclusion complète |
| `NkLatch::CountDown()` | ~20-60 | Décrémentation + notification |
| `NkThreadPool::Enqueue()` | ~100-300 | Avec notification condition |

> 📊 **Note** : Ces valeurs sont indicatives et varient selon la plateforme, la charge système et la contention. Toujours profiler avec vos workloads réels.

### 🔹 Optimisations applicatives

#### 1. Choisir la bonne primitive pour le bon usage

```cpp
// ❌ Utiliser un mutex pour une lecture seule
NkMutex mutex;
int ReadValue() {
    NkScopedLock lock(mutex);  // ❌ Exclusion inutile
    return sharedValue;
}

// ✅ Utiliser un read lock pour permettre la concurrence
NkReaderWriterLock rwlock;
int ReadValue() {
    NkReadLock lock(rwlock);  // ✅ Multiple readers OK
    return sharedValue;
}
```

#### 2. Minimiser la durée de détention des locks

```cpp
// ❌ Lock acquis trop tôt
void BadLocking(NkMutex& mutex) {
    NkScopedLock lock(mutex);
    auto data = ExpensiveComputation();  // ❌ Lock maintenu inutilement
    sharedData = data;
}

// ✅ Préparer hors section critique
void GoodLocking(NkMutex& mutex) {
    auto data = ExpensiveComputation();  // Hors lock
    {
        NkScopedLock lock(mutex);        // Scope minimal
        sharedData = data;
    }
}
```

#### 3. Ajuster le grainSize pour ParallelFor

```cpp
// ❌ grainSize trop petit = overhead de scheduling massif
pool.ParallelFor(1000000, Process, 1);  // 1M tâches soumises !

// ✅ grainSize adapté = bon équilibre overhead/équilibrage
constexpr nk_size kGrainSize = 256;  // ~4000 tâches pour 1M itérations
pool.ParallelFor(1000000, Process, kGrainSize);
```

#### 4. Utiliser ThreadLocal pour éviter la synchronisation

```cpp
// ❌ Mutex pour compteur par thread (contention inutile)
NkMutex counterMutex;
std::vector<int> perThreadCounters;

void Increment(int threadId) {
    NkScopedLock lock(counterMutex);  // ❌ Tous les threads compétent
    ++perThreadCounters[threadId];
}

// ✅ ThreadLocal pour isolation naturelle (zero sync)
NkThreadLocal<int> threadCounter(0);

void Increment(int) {  // threadId non nécessaire
    ++(*threadCounter);  // ✅ Accès lock-free, isolation garantie
}
```

### 🔹 Profiling et debugging

#### Activer le logging debug

```cmake
# Dans votre CMakeLists.txt
target_compile_definitions(mon_app PRIVATE NKENTSEU_THREADING_DEBUG)
```

#### Utiliser ThreadSanitizer (GCC/Clang)

```bash
# Build avec sanitizers
cmake .. -DCMAKE_BUILD_TYPE=Debug -DNKTHREADING_SANITIZE_THREADS=ON

# Exécuter avec détection de data races
./mon_app 2> tsan_report.txt
```

#### Métriques intégrées

```cpp
void MonitorPerformance(NkThreadPool& pool) {
    auto workers = pool.GetNumWorkers();
    auto queued = pool.GetQueueSize();
    auto completed = pool.GetTasksCompleted();

    NK_LOG_INFO("Pool: {} workers, {} queued, {} completed",
               workers, queued, completed);

    // Détecter les goulots d'étranglement
    if (queued > workers * 10) {
        NK_LOG_WARNING("High queue depth: consider adding workers");
    }
}
```

---

## 🌐 Support multiplateforme

### 🔹 Plateformes supportées

| Plateforme | Version minimale | Backend natif | Notes |
|------------|-----------------|---------------|-------|
| **Windows** | 10 (1809+) | SRWLock, CONDITION_VARIABLE, CreateThread | SetThreadDescription() requis pour NkThread::SetName() |
| **Linux** | glibc 2.17+ (Kernel 3.2+) | pthread_mutex, pthread_cond, pthread_create | pthread_setname_np() limité à 16 caractères |
| **macOS** | 10.14 (Mojave)+ | pthread avec adaptations spécifiques | pthread_setname_np() uniquement sur thread courant |
| **iOS** | 12+ | pthread avec restrictions sandbox | Pas de SetAffinity() (non supporté par iOS) |
| **Android** | 5.0 (API 21)+ | pthread avec optimisations Bionic | GetHardwareConcurrency() fiable depuis API 21 |

### 🔹 Détection automatique des features

Le module détecte automatiquement :

- ✅ Nombre de cores logiques (`GetHardwareConcurrency()`)
- ✅ Taille de ligne de cache (`NK_CACHE_LINE_SIZE` pour alignement)
- ✅ Instructions CPU spécifiques (PAUSE pour x86, YIELD pour ARM)
- ✅ APIs optionnelles (SetThreadDescription sur Windows 10+)

### 🔹 Fallbacks portables

Quand une API native n'est pas disponible :

| Feature non-disponible | Fallback portable | Impact performance |
|----------------------|------------------|-------------------|
| `SetThreadDescription()` (Windows <10) | Logging debug uniquement | Aucun (feature debug) |
| `pthread_setname_np()` (macOS thread distant) | Ignorer silencieusement | Aucun |
| `SetThreadAffinityMask()` (iOS) | Ignorer silencieusement | Possible migration de cache |
| `sched_getcpu()` (plateformes anciennes) | Retourner 0 | Métriques moins précises |

### 🔹 Compilation croisée

```bash
# Exemple : Cross-compile pour Linux ARM64 depuis x86_64
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=toolchains/linux-aarch64.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DNKENTSEU_ARCH_ARM64=ON \
  -DNK_CACHE_LINE_SIZE=128  # ARM64 a typiquement 128-byte cache lines
```

---

## 🔄 Migration depuis l'ancien namespace

### Contexte

Les primitives de threading étaient précédemment exposées dans le namespace `nkentseu::entseu`. Pour une meilleure organisation et cohérence, elles ont été déplacées vers `nkentseu::threading`.

### Impact

| Type de code | Impact | Action requise |
|-------------|--------|---------------|
| **Nouveau code** | Aucun | Utiliser directement `nkentseu::threading::` |
| **Code legacy** | Aucun (pour l'instant) | Les aliases `nkentseu::entseu::` redirigent vers `threading::` |
| **Binaires existants** | Aucun | ABI stable grâce au pattern Pimpl |

### Étapes de migration

#### Étape 1 : Mettre à jour les includes (optionnel mais recommandé)

```cpp
// AVANT
#include <NKThreading/NkMutex.h>
using namespace nkentseu::entseu;

// APRÈS (recommandé)
#include <NKThreading/NKThreading.h>  // Master header unique
using namespace nkentseu::threading;
```

#### Étape 2 : Mettre à jour les qualifications de namespace

```cpp
// AVANT
nkentseu::entseu::NkMutex mutex;
nkentseu::entseu::NkThreadPool pool(4);

// APRÈS
nkentseu::threading::NkMutex mutex;
nkentseu::threading::NkThreadPool pool(4);

// OU avec using (plus lisible)
using namespace nkentseu::threading;
NkMutex mutex;
NkThreadPool pool(4);
```

#### Étape 3 : Activer les warnings de dépréciation (optionnel)

```cmake
# Dans votre CMakeLists.txt
option(NKENTSEU_WARN_ON_LEGACY_THREADING
       "Emit warnings when using nkentseu::entseu namespace"
       ON)

if(NKENTSEU_WARN_ON_LEGACY_THREADING)
    target_compile_definitions(mon_app PRIVATE NKENTSEU_WARN_ON_LEGACY_USAGE)
endif()
```

#### Étape 4 : Tester et valider

```bash
# Build avec warnings activés
cmake --build . --config Release -DNKENTSEU_WARN_ON_LEGACY_THREADING=ON

# Exécuter les tests unitaires
ctest --output-on-failure

# Profiler pour vérifier l'absence de régressions
./mon_app --benchmark
```

#### Étape 5 : Désactiver les aliases legacy (après migration complète)

```cmake
# Quand tout le codebase a migré :
target_compile_definitions(mon_app PRIVATE NKENTSEU_DISABLE_LEGACY_ALIASES)
```

### Calendrier de dépréciation

| Version | État des aliases legacy | Action |
|---------|------------------------|--------|
| **v2.x** | Disponibles, warnings optionnels | Migration progressive recommandée |
| **v3.0** | Disponibles, warnings par défaut | Migration requise pour nouveaux projets |
| **v4.0** | Supprimés (breaking change) | Migration obligatoire |

### Outils d'assistance

#### Script de migration automatique (conceptuel)

```bash
# scripts/migrate_threading_namespace.py --dry-run src/ include/
# Règles de remplacement :
# 1. nkentseu::entseu::Nk* → nkentseu::threading::Nk*
# 2. using namespace nkentseu::entseu; → using namespace nkentseu::threading;
# 3. #include <NKThreading/Nk*.h> → #include <NKThreading/NKThreading.h> (optionnel)

# Exclusions (ne pas modifier) :
# - Commentaires contenant les anciens noms
# - Strings littérales
# - Code dans des blocs #if NKENTSEU_USE_LEGACY_THREADING
```

#### Vérification de type avec static_assert

```cpp
// Dans vos tests de migration :
static_assert(
    std::is_same_v<
        nkentseu::entseu::NkMutex,
        nkentseu::threading::NkMutex
    >,
    "Alias legacy doit pointer vers l'implémentation moderne"
);
```

---

## 🔍 Dépannage et FAQ

### ❓ Problèmes courants

#### 🔴 Deadlock avec mutex réentrant

**Symptôme** : Application freeze, thread bloqué dans `Lock()`.

**Cause probable** : Appel récursif à `Lock()` sur un `NkMutex` standard (non-réentrant).

**Solution** :
```cpp
// ❌ MAUVAIS : NkMutex ne supporte pas le re-lock
NkMutex mutex;
void Outer() {
    mutex.Lock();
    Inner();  // ❌ Deadlock si Inner() appelle aussi Lock()
    mutex.Unlock();
}

// ✅ BON : Utiliser NkRecursiveMutex si vraiment nécessaire
NkRecursiveMutex mutex;
void Outer() {
    NkScopedLock lock(mutex);  // Re-lock autorisé
    Inner();
}
```

> 💡 **Conseil** : Préférer refactoriser l'API pour éviter la récursivité de lock plutôt que d'utiliser `NkRecursiveMutex`.

#### 🔴 Thread ne voit pas les modifications d'un autre thread

**Symptôme** : Variable partagée semble "figée" dans un thread.

**Cause probable** : Absence de synchronisation ou mauvaise primitive.

**Solutions** :
```cpp
// ❌ MAUVAIS : Pas de synchronisation
int sharedFlag = 0;
// Thread 1: sharedFlag = 1;
// Thread 2: while (sharedFlag == 0) {}  // ❌ Peut boucler indéfiniment

// ✅ BON : Utiliser NkMutex pour synchronisation
NkMutex mutex;
int sharedFlag = 0;
// Thread 1:
{
    NkScopedLock lock(mutex);
    sharedFlag = 1;
}
// Thread 2:
while (true) {
    NkScopedLock lock(mutex);
    if (sharedFlag == 1) break;
}

// ✅ MIEUX : Utiliser NkConditionVariable pour attente efficace
NkMutex mutex;
NkConditionVariable cond;
bool sharedFlag = false;
// Thread 1:
{
    NkScopedLock lock(mutex);
    sharedFlag = true;
}
cond.NotifyOne();
// Thread 2:
{
    NkScopedLock lock(mutex);
    cond.WaitUntil(lock, []() { return sharedFlag; });
}
```

#### 🔴 Performance dégradée avec NkSpinLock

**Symptôme** : CPU usage élevé, application lente sous charge.

**Cause probable** : Utilisation de `NkSpinLock` pour des sections critiques longues ou avec forte contention.

**Solution** :
```cpp
// ❌ MAUVAIS : SpinLock pour section longue
NkSpinLock lock;
void Process() {
    NkScopedSpinLock guard(lock);
    ExpensiveOperation();  // ❌ 1000+ cycles = waste CPU massif
}

// ✅ BON : Mutex pour sections longues
NkMutex mutex;
void Process() {
    NkScopedLock guard(mutex);  // Bloque efficacement sans waste CPU
    ExpensiveOperation();
}
```

#### 🔴 Latch.Wait() retourne immédiatement sans attendre

**Symptôme** : `Wait()` retourne `true` immédiatement même si `CountDown()` n'a pas été appelé.

**Cause probable** : Initialisation incorrecte du `NkLatch`.

**Solution** :
```cpp
// ❌ MAUVAIS : Initialisation à 0
NkLatch latch(0);  // Wait() retourne immédiatement !

// ✅ BON : Initialiser avec le nombre exact de signaux attendus
constexpr nk_uint32 kExpectedSignals = 5;
NkLatch latch(kExpectedSignals);  // Wait() bloque jusqu'à 5 CountDown()
```

### ❓ Questions fréquentes

#### Q: Puis-je utiliser NKThreading dans une bibliothèque partagée ?

**R**: Oui, grâce au pattern Pimpl et aux macros d'export configurables. Assurez-vous de :
- Définir `NKENTSEU_THREADING_BUILD_SHARED_LIB` lors de la compilation de la bibliothèque
- Définir `NKENTSEU_THREADING_STATIC_LIB` ou rien lors de l'utilisation (import par défaut)

#### Q: Comment gérer les exceptions dans les tâches soumises à NkThreadPool ?

**R**: Les exceptions non catchées dans une tâche terminent le thread worker (comportement C++11+). Toujours wrapper :

```cpp
pool.Enqueue([]() {
    try {
        RiskyOperation();
    } catch (const std::exception& e) {
        NK_LOG_ERROR("Task exception: {}", e.what());
    } catch (...) {
        NK_LOG_ERROR("Unknown exception in task");
    }
});
```

#### Q: Puis-je appeler Wait() depuis le thread lui-même ?

**R**: Non, c'est un deadlock garanti. Exemple à éviter :

```cpp
NkThread t([](NkThread* self) {  // Hypothétique : passage de self
    self->Join();  // ❌ Deadlock : thread s'attend lui-même !
});
```

#### Q: Comment choisir entre NkBarrier et NkLatch ?

**R**:
| Besoin | Primitive recommandée |
|--------|---------------------|
| Synchroniser des phases itératives (boucle avec sync à chaque itération) | `NkBarrier` (réutilisable) |
| Attendre que N tâches indépendantes soient terminées (une fois) | `NkLatch` (usage unique) |
| Signal booléen persistant ou consommable | `NkEvent` |

#### Q: NKThreading est-il compatible avec std::thread et autres primitives STL ?

**R**: Oui, mais déconseillé. Mélanger les APIs peut conduire à des comportements inattendus. Préférer utiliser exclusivement les primitives NKThreading pour la cohérence.

---

## 🤝 Contribuer

### 📋 Guidelines de contribution

1. **Fork** le dépôt et créez une branche pour votre feature : `git checkout -b feature/ma-nouvelle-primitive`
2. **Codez** en respectant le style existant (indentation, documentation Doxygen, 1 instruction/ligne)
3. **Testez** avec les tests unitaires existants et ajoutez-en pour votre feature
4. **Documentez** avec des exemples d'utilisation dans les commentaires
5. **Soumettez** une Pull Request avec une description claire des changements

### 🧪 Exécution des tests

```bash
# Build avec tests activés
cmake .. -DNKTHREADING_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug

# Exécuter tous les tests
ctest --output-on-failure

# Exécuter un test spécifique
ctest -R NkLatchTest --output-on-failure

# Build avec sanitizers pour détection de bugs
cmake .. -DNKTHREADING_SANITIZE_THREADS=ON -DNKTHREADING_SANITIZE_ADDRESS=ON
```

### 📝 Style de code requis

- ✅ **Indentation** : 4 espaces par niveau, namespaces imbriqués indentés
- ✅ **Documentation** : Doxygen complet (`@brief`, `@param`, `@return`, `@note`, `@warning`)
- ✅ **Formatage** : 1 instruction par ligne, guards RAII pour les locks
- ✅ **Nommage** : CamelCase pour types/fonctions, snake_case pour variables locales
- ✅ **Export** : `NKENTSEU_THREADING_CLASS_EXPORT` sur les classes, pas sur les méthodes internes

### 🚫 Ce que nous ne mergeons pas

- ❌ Code sans tests unitaires
- ❌ Modifications breaking sans plan de migration
- ❌ Optimisations prématurées sans benchmark
- ❌ Dépendances externes non approuvées
- ❌ Documentation incomplète ou exemples non fonctionnels

### 🏆 Reconnaissance

Les contributeurs significatifs sont listés dans [CONTRIBUTORS.md](CONTRIBUTORS.md) et leurs noms apparaissent dans les notes de version.

---

## 📄 Licence

```
Copyright © 2024-2026 Rihen. Tous droits réservés.

Licence Propriétaire - Libre d'utilisation et de modification

Ce module est fourni "tel quel", sans garantie d'aucune sorte, expresse ou implicite.
L'auteur ne peut être tenu responsable des dommages résultant de l'utilisation de ce logiciel.

Vous êtes libre de :
✅ Utiliser ce module dans des projets personnels ou commerciaux
✅ Modifier le code source pour vos besoins
✅ Distribuer des versions modifiées (avec attribution)

Vous devez :
📝 Conserver cette notice de licence dans toutes les copies
📝 Mentionner l'auteur original dans la documentation
📝 Ne pas utiliser le nom "Rihen" ou "NKEntseu" pour promouvoir des produits dérivés sans autorisation

Pour toute question concernant la licence, contactez : license@nkentseu.example
```

---

## 📞 Support et ressources

### 🔗 Liens utiles

- 📚 [Documentation Doxygen générée](https://docs.nkentseu.example/threading)
- 🔧 [Exemples de code complets](https://github.com/yourorg/NKThreading-examples)
- 🐛 [Tracker de bugs et feature requests](https://github.com/yourorg/NKThreading/issues)
- 💬 [Forum de discussion communautaire](https://community.nkentseu.example/threading)

### 🆘 Obtenir de l'aide

1. **Consultez la documentation** : La plupart des questions ont une réponse dans ce README ou la doc Doxygen
2. **Recherchez dans les issues** : Votre problème a peut-être déjà été résolu
3. **Ouvrez une issue** : Fournissez un exemple minimal reproductible, la plateforme, et la version utilisée
4. **Contactez le support** : Pour les utilisateurs enterprise, support prioritaire disponible

### 🔄 Changelog et versions

Voir [CHANGELOG.md](CHANGELOG.md) pour l'historique complet des versions, les breaking changes et les notes de migration.

---

> **NKThreading** — *Synchronisation thread-safe, performance native, simplicité C++.*  
> Développé avec ❤️ par Rihen et la communauté NKEntseu.