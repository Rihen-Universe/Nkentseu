# NKMemory

> Couche **Foundation** · Gestion mémoire centralisée : allocateurs, smart pointers,
> tracking de fuites, garbage collector, profiling, budgets par tag.

NKMemory est le **point d'entrée unique de toute allocation** dans Nkentseu. Le moteur
applique la règle dure : **on ne fait jamais `new`/`delete` directs** — tout passe par un
allocateur NKMemory. Cela permet le tracking des fuites, le profiling, les budgets par
sous-système, et garantit la symétrie allocation/libération (pas de mismatch).

- **Namespace** : `nkentseu::memory`
- **Header parapluie** : `#include "NKMemory/NKMemory.h"`
- **Auteur / éditeur** : Rihen

---

## 1. La règle d'or

```cpp
auto& alloc = nkentseu::memory::NkGetDefaultAllocator();

MyType* obj = alloc.New<MyType>(arg1, arg2);   // construit (jamais `new`)
// ... utilisation ...
alloc.Delete(obj);                              // détruit + libère (jamais `delete`)
```

**Pourquoi** : `New`/`Delete` enregistrent l'allocation (fichier/ligne/fonction/tag),
alimentent le tracker de fuites et le profiler, et garantissent que la libération
emprunte le **même** allocateur que l'allocation. Mélanger `alloc.New` avec `delete`
global (ou l'inverse) provoque une **corruption de tas** (`STATUS_HEAP_CORRUPTION`,
code `0xC0000374`).

> **Conséquence pour les classes** : toute classe qui expose des méthodes de **création**
> doit exposer les méthodes de **destruction** symétriques (ex. `NkXxxFactory::Create` ↔
> `NkXxxFactory::Destroy`), et celles-ci utilisent `alloc.New`/`alloc.Delete` en interne.

---

## 2. Système d'allocateurs

### 2.1 `NkAllocatorBase`

Classe de base abstraite de tous les allocateurs. Définit l'interface bas niveau
(allocation/libération de blocs bruts) et `Reset()`.

### 2.2 `NkAllocator`

Allocateur général (le plus utilisé). Hérite de `NkAllocatorBase` et ajoute des helpers
**type-safe** templatés (inline, dans le header, pour la performance) :

| Méthode | Rôle |
|---------|------|
| `template<class T, class... Args> [[nodiscard]] T* New(Args&&... args)` | Alloue + construit un `T` (perfect-forwarding des args). |
| `template<class T> void Delete(T* ptr) noexcept` | Appelle `~T()` puis libère. No-op si `ptr == nullptr`. |
| `template<class T, class... Args> T* NewArray(...)` | Alloue + construit un tableau de `T`. |
| `template<class T> void DeleteArray(T* ptr) noexcept` | Détruit + libère un tableau créé par `NewArray`. |
| `virtual void Reset() noexcept` | Libère tout d'un coup (selon la stratégie de l'allocateur). |
| `GetStats()` | Statistiques d'allocation (octets vivants, pics, compte…). |

### 2.3 Allocateur par défaut

```cpp
NKENTSEU_MEMORY_API NkAllocator& NkGetDefaultAllocator() noexcept;
```

Renvoie le singleton d'allocateur global thread-safe (Meyer's singleton). C'est celui
que **tout le moteur** utilise par défaut.

### 2.4 Allocateurs spécialisés

| Classe | Fichier | Stratégie |
|--------|---------|-----------|
| `NkFixedPoolAllocator` | `NkPoolAllocator.h` | Pool de blocs de **taille fixe** : allocation/libération O(1), zéro fragmentation. Idéal pour des objets homogènes (particules, nœuds…). |
| `NkVariablePoolAllocator` | `NkPoolAllocator.h` | Pool gérant des **tailles variables**. |
| `NkPoolAllocatorTyped<T>` | `NkPoolAllocatorTyped.h` | Pool typé : variante template type-safe du pool fixe. |
| `NkMultiLevelAllocator` | `NkMultiLevelAllocator.h` | Allocateur **multi-niveaux** (`enum class NkAllocTier`) qui route la demande vers le tier adapté selon la taille. |
| `NkContainerAllocator` | `NkContainerAllocator.h` | Adaptateur d'allocateur pour les conteneurs NKContainers (croissance, realloc). |

Tous redéfinissent `Reset()` et s'utilisent via la même interface `New`/`Delete`.

---

## 3. Smart pointers

NKMemory fournit des pointeurs intelligents maison (zéro-STL), construits eux aussi via
l'allocateur.

### 3.1 `NkUniquePtr<T, Deleter>` — possession exclusive

`#include "NKMemory/NkUniquePtr.h"` · équivalent de `std::unique_ptr`.

| Élément | Description |
|---------|-------------|
| `NkUniquePtr(pointer p)` | Prend possession de `p`. |
| `operator=(NkUniquePtr&&)` | Move-assignable. Copie **interdite** (`= delete`). |
| `element_type& operator*() const` | Déréférence. |
| `pointer operator->() const` | Accès membre. |
| `explicit operator bool() const` | `true` si non nul. |
| `pointer Get() const` | Pointeur brut (sans transfert). |
| `pointer Release()` | Relâche la possession et renvoie le pointeur. |
| `void Reset(pointer p = nullptr)` | Détruit l'objet courant et adopte `p`. |
| **Spécialisation `NkUniquePtr<T[]>`** | Pour les tableaux : `operator[]` au lieu de `*`/`->`. |
| `NkMakeUnique<T>(args...)` | Fabrique recommandée (construit via l'allocateur). |
| `NkMakeUniqueArray<T>(n)` | Fabrique pour les tableaux. |

> Le `Deleter` par défaut libère **via l'allocateur NKMemory** — d'où l'importance de
> ne pas mélanger avec `delete`.

### 3.2 `NkSharedPtr<T>` / `NkWeakPtr<T>` — possession partagée

`#include "NKMemory/NkSharedPtr.h"` · équivalent de `std::shared_ptr` / `weak_ptr`.

- Compteur de références fort/faible via un **bloc de contrôle** (`NkSharedControlBlock<T>`,
  base `NkSharedControlBlockBase` avec `ReleaseStrongRef()` / `ReleaseWeakRef()`).
- `NkWeakPtr<T>` : référence faible, ne maintient pas l'objet en vie (casse les cycles).
- `NkMakeShared<T>(args...)` : fabrique (bloc de contrôle + objet en une allocation).

### 3.3 `NkIntrusivePtr<T>` — comptage intrusif

`#include "NKMemory/NkIntrusivePtr.h"` · le compteur vit **dans l'objet**.

- Hériter de `NkIntrusiveRefCounted` (fournit `AddRef()` / `ReleaseRef()`).
- `NkMakeIntrusive<T>(args...)` : fabrique.
- Plus léger que `NkSharedPtr` (pas de bloc de contrôle séparé), idéal pour les
  ressources/entités fortement partagées.

---

## 4. Opérations mémoire — `NkFunction.h`

`#include "NKMemory/NkFunction.h"` (+ `NkFunctionSIMD.h`) · **bibliothèque d'opérations
mémoire** : `NkCopy`/`NkMove`/`NkSet`/`NkCompare`, recherche (`NkFind`, `NkSearchPattern`),
transformation (`NkReverse`, `NkSwapEndian`), construction/destruction typées
(`NkConstruct`/`NkDestroy`), alignement — wrappers sûrs autour de `NkUtils.h` (optimisés
AVX2) + variantes **SIMD**. Détail : [NKMemory/Memory-Operations.md](NKMemory/Memory-Operations.md).

> _Le nom est trompeur : ce header ne contient **pas** de `std::function`._

---

## 5. Outils mémoire avancés

### 5.1 `NkMemoryTracker` — détection de fuites

`#include "NKMemory/NkTracker.h"`. Enregistre chaque allocation avec ses **métadonnées**
(fichier/ligne/fonction/tag). `GetStats()` renvoie l'état courant ; un **rapport de
fuites** peut être émis à la fermeture (tout ce qui n'a pas été libéré).

### 5.2 `NkMemoryProfiler` — hooks runtime

`#include "NKMemory/NkProfiler.h"`. Hooks de profiling sur les allocations/libérations.
`GetGlobalStats()` agrège les statistiques globales (débit, pics, compte).

### 5.3 Garbage Collector — `NkGarbageCollector`

`#include "NKMemory/NkGc.h"`. GC **mark-and-sweep** optionnel pour les objets gérés :
- `NkGcObject` : base des objets collectables.
- `NkGcTracer(NkGarbageCollector&)` : visiteur qui marque les objets atteignables.
- `NkGarbageCollector` : orchestre mark & sweep. Plusieurs GC peuvent coexister
  (gérés centralement par la façade NKMemory).

### 5.4 Budgets par tag — `NkMemoryTag` / `NkMemoryBudget`

`#include "NKMemory/NkTag.h"`. Permet d'attribuer un **tag** (`enum class NkMemoryTag`) à
chaque sous-système et de plafonner sa consommation :

| Méthode (`NkMemoryBudget`) | Rôle |
|-----------|------|
| `void SetBudget(NkMemoryTag tag, nk_uint64 bytes)` | Fixe le plafond d'un tag. |
| `nk_bool IsOverBudget(NkMemoryTag tag)` | Le tag dépasse-t-il son budget ? |
| `NkMemoryTagStats GetStats(NkMemoryTag tag)` | Conso courante du tag. |
| `void ResetStats()` | Remet les compteurs à zéro. |

### 5.5 Index de pointeurs — `NkPointerHashMap` / `NkPointerHashSet`

`#include "NKMemory/NkHash.h"`. Tables de hachage spécialisées **clé = pointeur**, O(1),
utilisées en interne pour indexer les allocations (`TryGet(key, &outValue)`…).

---

## 6. Adaptateurs

| Header | Rôle |
|--------|------|
| `NkStlAdapter.h` | Adaptateur permettant d'utiliser un allocateur NKMemory là où une interface façon-STL est attendue (interop). |
| `NkContainerAllocator.h` | Allocateur dédié aux conteneurs NKContainers. |
| `NkUtils.h` | Utilitaires mémoire (copie, set, alignement…). |
| `NkMemoryApi.h` | Macro d'export `NKENTSEU_MEMORY_API` (visibilité symboles). |

---

## 7. Exemples

### Création/destruction simple

```cpp
#include "NKMemory/NKMemory.h"
using namespace nkentseu::memory;

struct Foo { Foo(int v) : value(v) {} int value; };

auto& a = NkGetDefaultAllocator();
Foo* f = a.New<Foo>(42);
// ...
a.Delete(f);
```

### Smart pointer

```cpp
auto ptr = NkMakeUnique<Foo>(7);   // possession exclusive
int v = ptr->value;                // accès
// libération automatique en fin de portée (via l'allocateur)
```

### Pool d'objets homogènes

```cpp
NkFixedPoolAllocator pool(/* blockSize = */ sizeof(Foo), /* count = */ 1024);
Foo* a = pool.New<Foo>(1);
Foo* b = pool.New<Foo>(2);
pool.Delete(a);
pool.Reset();   // libère tout le pool d'un coup
```

---

## 8. Index des 19 headers

| Header | Contenu principal |
|--------|-------------------|
| `NKMemory.h` | Façade unifiée (inclut tout, singleton, API macro-friendly). |
| `NkMemoryApi.h` | Macro d'export. |
| `NkAllocator.h` | `NkAllocatorBase`, `NkAllocator`, `NkGetDefaultAllocator()`, helpers `New`/`Delete`. |
| `NkPoolAllocator.h` | `NkFixedPoolAllocator`, `NkVariablePoolAllocator`. |
| `NkPoolAllocatorTyped.h` | `NkPoolAllocatorTyped<T>`. |
| `NkMultiLevelAllocator.h` | `NkMultiLevelAllocator`, `NkAllocTier`. |
| `NkContainerAllocator.h` | Allocateur pour conteneurs. |
| `NkUniquePtr.h` | `NkUniquePtr<T>` (+ tableau), `NkMakeUnique`. |
| `NkSharedPtr.h` | `NkSharedPtr<T>`, `NkWeakPtr<T>`, blocs de contrôle, `NkMakeShared`. |
| `NkIntrusivePtr.h` | `NkIntrusivePtr<T>`, `NkIntrusiveRefCounted`, `NkMakeIntrusive`. |
| `NkFunction.h` | Opérations mémoire (copy/move/set/compare, recherche, transformation, construct/destroy, alignement). |
| `NkFunctionSIMD.h` | Variantes SIMD des opérations mémoire. |
| `NkTracker.h` | `NkMemoryTracker` (fuites). |
| `NkProfiler.h` | `NkMemoryProfiler` (hooks runtime). |
| `NkGc.h` | `NkGarbageCollector`, `NkGcObject`, `NkGcTracer`. |
| `NkTag.h` | `NkMemoryTag`, `NkMemoryBudget`. |
| `NkHash.h` | `NkPointerHashMap`, `NkPointerHashSet`. |
| `NkStlAdapter.h` | Adaptateur STL-like. |
| `NkUtils.h` | Utilitaires mémoire. |

> **Note** : ce document décrit l'API publique stable. Pour les détails d'implémentation
> (stratégies internes, structures privées), se référer aux headers eux-mêmes.

---

[← Couche Foundation](README.md) · [Index du wiki](../README.md)
