# NKMemory - Validation production (Go/No-Go)

Date: 2026-03-04  
Plateforme de mesure: Windows x86_64 (Debug, clang-mingw)  
Exécutable: `Build/Tests/Debug-Windows/NKMemory_Tests.exe`

## 1) Injection des tests via Jenga

Intégration activée dans `Modules/Foundation/NKMemory/NKMemory.jenga`:

```python
with filter("system:Linux || system:macOS || system:Windows && !options:windows-runtime=uwp && !system:XboxSeries && !system:XboxOne"):
    with test():
        testfiles(["tests/**.cpp"])
```

Effet:

- Jenga génère automatiquement le projet `NKMemory_Tests` (type `TEST_SUITE`).
- Les fichiers `tests/**.cpp` sont compilés et liés avec `__Unitest__`.

## 2) Campagne exécutée

Commande build:

```powershell
jenga build
```

Commande tests:

```powershell
.\Build\Tests\Debug-Windows\NKMemory_Tests.exe
```

Portée:

- 12 tests
- 55 assertions
- Unit tests allocateurs (malloc/new/linear/arena/stack/pool/freelist/buddy/virtual)
- Stress multi-thread
- Test coalescing/free list
- Benchmark p99 (allocateurs principaux)
- Smoke tests `NkOptional` / `NkVariant` / `NkInvoke`

## 3) Résultats de test

- Tests: **12/12 passés**
- Assertions: **55/55 passées**
- Temps total suite: **20 ms**

## 4) Benchmark p99 (nanosecondes)

| Allocateur | Avg (ns) | P99 (ns) | Failures |
|---|---:|---:|---:|
| NkMallocAllocator | 307.67 | 500.00 | 0/2048 |
| NkLinearAllocator | 100.11 | 100.00 | 0/875 |
| NkPoolAllocator | 100.13 | 100.00 | 0/755 |
| NkFreeListAllocator | 380.30 | 1600.00 | 0/2036 |
| NkBuddyAllocator | 101.02 | 100.00 | 0/1280 |
| NkVirtualAllocator | 2376.95 | 2700.00 | 0/256 |

## 5) Critères Go/No-Go

Critères utilisés (campagne locale):

- A. Tous les tests unitaires/stress doivent passer.
- B. `failures == 0` sur tous les benchmarks.
- C. P99 borné (ordre de grandeur cohérent) pour usage desktop temps réel soft.

Evaluation:

- A: PASS
- B: PASS
- C: PASS (dans ce contexte local Debug)

## 6) Verdict

Verdict global (ce run):

- **GO** pour:
  - applications desktop générales,
  - rendu temps réel soft (Vulkan/OpenGL/DirectX côté CPU allocations maîtrisées),
  - pipelines IA en pré-production/prod contrôlée.

- **GO conditionnel** pour:
  - hard real-time strict (audio ultra-faible latence, contraintes certifiées),
  - déploiements multi-plateformes critiques sans validation cible dédiée.

## 7) Limites de cette campagne

- Mesures faites en **Debug** (pas Release).
- Une seule machine de benchmark.
- Pas encore de campagne endurance longue (plusieurs heures/jours).
- Pas encore de comparaison inter-OS (Linux/macOS/Android/Web).

## 8) Recommandation finale

- Etat actuel: **production-ready contrôlé**.
- Pour "production durci" (No surprises):
  - ajouter runs Release + CI périodique,
  - ajouter endurance long-run,
  - ajouter benchmarks par plateforme cible,
  - figer seuils p99/p999 par sous-système.

