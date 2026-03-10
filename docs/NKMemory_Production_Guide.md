# NKMemory / NKCore - Guide d'utilisation orienté production

Ce document couvre:

- `NKMemory`: allocateurs, smart pointers, GC, fonctions mémoire.
- `NKCore`: `NkOptional`, `NkVariant`, `NkInvoke`.
- Des recettes par type d'application: IA, rendu (Vulkan/OpenGL/DirectX), temps réel, application légère.

## 1. Verdict Production Ready

Etat actuel:

- Le build passe (`jenga build` OK).
- Les briques majeures existent: allocateurs spécialisés, API mémoire, smart pointers, GC.
- `NkOptional`, `NkVariant`, `NkInvoke` sont disponibles dans `NKCore`.

Verdict:

- **Utilisable en pré-production et production contrôlée**.
- **Pas encore "production-ready durci"** pour tous contextes critiques sans campagne de validation supplémentaire.

Pourquoi:

- Pas encore de batterie de tests dédiée aux allocateurs sous charge (fragmentation, patterns adverses, long run).
- Pas encore de bench/latence documentés par plateforme (Windows/Linux/Android/Web/etc.).
- Pas encore de tests de robustesse multi-thread ciblés sur tous les chemins d'alloc.

## 2. Où placer quoi (architecture)

- `NKCore`:
  - `NkOptional`
  - `NkVariant`
  - `NkInvoke`
  - Raison: utilitaires génériques de langage, indépendants de la mémoire.

- `NKMemory`:
  - Allocateurs (`NkMallocAllocator`, `NkLinearAllocator`, `NkPoolAllocator`, etc.)
  - Smart pointers (`NkSharedPtr`, `NkUniquePtr`, `NkIntrusivePtr`)
  - GC (`NkGarbageCollector`)
  - Fonctions mémoire (`NkMemoryFn`, `NkUtils`)

## 3. Règles de choix d'allocateur

- `NkMallocAllocator`:
  - Par défaut, généraliste, simple.
- `NkLinearAllocator`:
  - Temporaire/frame scratch, reset global rapide.
- `NkArenaAllocator`:
  - Scope de travail (requête, frame, tâche) avec marker.
- `NkStackAllocator`:
  - LIFO strict (parsing, étapes empilées).
- `NkPoolAllocator`:
  - Objets de taille fixe (particles, commandes, composants).
- `NkFreeListAllocator`:
  - Tailles variées, durées de vie hétérogènes.
- `NkBuddyAllocator`:
  - Blocs puissance de 2, compromis fragmentation/perf.
- `NkVirtualAllocator`:
  - Grandes réserves virtuelles.

## 4. Exemples de base

### 4.1 Objet unique / tableau

```cpp
#include "NKMemory/NkAllocator.h"

using namespace nkentseu::memory;

struct Foo { int x; Foo(int v) : x(v) {} };

void ExampleBasic() {
    NkAllocator& alloc = NkGetDefaultAllocator();

    Foo* a = alloc.New<Foo>(42);
    alloc.Delete(a);

    Foo* arr = alloc.NewArray<Foo>(128, 7);
    alloc.DeleteArray(arr);
}
```

### 4.2 Fonctions mémoire avancées

```cpp
#include "NKMemory/NkMemoryFn.h"

using namespace nkentseu::memory;

void ExampleMemFn() {
    char src[64] = {};
    char dst[64] = {};

    NkSet(src, 0xAB, sizeof(src));
    NkCopy(dst, src, sizeof(src));

    void* p = NkSearchPattern(dst, sizeof(dst), "\xAB\xAB", 2);
    (void)p;
}
```

## 5. Cas d'usage par type d'application

## 5.1 IA / Inference (CPU/GPU)

Objectif:

- Réduire la variance de latence.
- Limiter la fragmentation.

Pattern recommandé:

- `NkArenaAllocator` par requête d'inference.
- `NkPoolAllocator` pour métadonnées tensor/op.
- `NkVirtualAllocator` pour grosses régions temporaires.

```cpp
using namespace nkentseu::memory;

void RunInferenceRequest() {
    NkArenaAllocator arena(32 * 1024 * 1024); // 32 MB request-scope

    float* logits = static_cast<float*>(arena.Allocate(4096 * sizeof(float), alignof(float)));
    // ... pipeline

    arena.Reset(); // fin requête
}
```

## 5.2 Rendu Vulkan / OpenGL / DirectX

Objectif:

- Allocation transiente par frame.
- Zéro coût de free individuel.

Pattern recommandé:

- `NkLinearAllocator` pour données frame (command buffers CPU-side, staging metadata).
- `NkPoolAllocator` pour commandes de rendu de taille fixe.

```cpp
using namespace nkentseu::memory;

struct DrawCmd { /* fixed-size */ };

void RenderLoop() {
    NkLinearAllocator frameAlloc(8 * 1024 * 1024); // 8 MB/frame
    NkPoolAllocator cmdPool(sizeof(DrawCmd), 4096);

    while (true) {
        frameAlloc.Reset();
        DrawCmd* cmd = static_cast<DrawCmd*>(cmdPool.Allocate(sizeof(DrawCmd)));
        // remplir et soumettre...
        cmdPool.Deallocate(cmd);
    }
}
```

## 5.3 Temps réel (jeu, audio, simulation)

Objectif:

- Eviter les allocations non déterministes dans la boucle critique.

Pattern recommandé:

- Pré-allocation au boot.
- `NkPoolAllocator` + `NkStackAllocator`.
- Interdiction de `NkMallocAllocator` dans la section hard real-time.

```cpp
using namespace nkentseu::memory;

NkPoolAllocator eventPool(64, 8192); // events fixes
NkStackAllocator rtStack(2 * 1024 * 1024);

void AudioCallback() {
    void* tmp = rtStack.Allocate(1024, 16);
    // ...
    rtStack.Deallocate(tmp); // LIFO strict
}
```

## 5.4 Application légère / utilitaire desktop

Objectif:

- Simplicité et maintenance.

Pattern recommandé:

- `NkGetDefaultAllocator()` (malloc-based).
- `NkUniquePtr` / `NkSharedPtr`.
- `NkOptional` pour données optionnelles.

```cpp
#include "NKMemory/NkUniquePtr.h"
#include "NKCore/NkOptional.h"

using namespace nkentseu::memory;
using nkentseu::NkOptional;

void SmallApp() {
    auto p = NkMakeUnique<int>(12);
    NkOptional<int> maybePort = 8080;
    if (maybePort) {
        // use *maybePort
    }
}
```

## 6. Usage NKCore: Optional / Variant / Invoke

### 6.1 NkOptional

```cpp
#include "NKCore/NkOptional.h"
using nkentseu::NkOptional;
using nkentseu::NkNullOpt;

NkOptional<int> ParsePort(bool ok) {
    if (!ok) return NkNullOpt;
    return 7777;
}
```

### 6.2 NkVariant

```cpp
#include "NKCore/NkVariant.h"

using nkentseu::NkVariant;
using nkentseu::NkVisit;

using Msg = NkVariant<int, float>;

void Handle(Msg& m) {
    NkVisit([](auto& v) {
        // traitement
        (void)v;
    }, m);
}
```

### 6.3 NkInvoke

```cpp
#include "NKCore/NkInvoke.h"

struct Worker {
    int Mul(int a, int b) { return a * b; }
};

int x = nkentseu::NkInvoke([](int v) { return v + 1; }, 41);
Worker w;
int y = nkentseu::NkInvoke(&Worker::Mul, w, 6, 7);
```

## 7. Smart pointers et ownership

- Ownership unique: `NkUniquePtr`.
- Ownership partagé: `NkSharedPtr` + `NkWeakPtr`.
- Type intrusif moteur: `NkIntrusivePtr`.

Règle:

- Pour code gameplay/app simple: `NkUniquePtr` d'abord.
- `NkSharedPtr` uniquement si ownership réellement partagé.

## 8. Checklist de durcissement avant release production

- Ajouter tests unitaires allocateur par allocateur:
  - alignement, fragmentation, coalescing, out-of-memory.
- Ajouter stress test long run (heures) avec patterns mixtes.
- Ajouter bench:
  - alloc/free throughput
  - p99 latency
  - memory overhead
- Tester multi-thread sur vos workloads réels.
- Activer et valider leak reports `NkMemorySystem`.
- Figer une policy d'alloc par sous-système (render, IA, gameplay, IO).

## 9. Profiling conseillé

- Mesurer:
  - `liveBytes`, `peakBytes`, `liveAllocations`, `totalAllocations`
  - temps moyen et p99 d'alloc/dealloc
  - taux de reset réussi pour linear/arena
- Corréler avec frame time (rendu) ou tail latency (backend IA).

## 10. Résumé rapide de décision

- Si priorité perf déterministe:
  - `Linear/Arena/Pool/Stack`.
- Si priorité simplicité:
  - `MallocAllocator` + smart pointers.
- Si grosses zones mémoire:
  - `VirtualAllocator`.
- Si type polymorphe de messages/events:
  - `NkVariant`.
- Si champ optionnel:
  - `NkOptional`.
- Si invocation générique/callback:
  - `NkInvoke`.

