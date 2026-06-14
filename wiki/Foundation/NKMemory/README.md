# NKMemory — documentation détaillée

Ce dossier décompose le module **NKMemory** partie par partie. Pour une vue
d'ensemble synthétique, voir le récap : [../NKMemory.md](../NKMemory.md).

## Parties

| Fichier | Contenu | Headers couverts |
|---------|---------|------------------|
| [Allocators.md](Allocators.md) | Le système d'allocateurs : `NkAllocator`, `New`/`Delete`, pools, multi-niveaux. Quand utiliser quoi. | `NkAllocator.h`, `NkPoolAllocator.h`, `NkPoolAllocatorTyped.h`, `NkMultiLevelAllocator.h`, `NkContainerAllocator.h` |
| [SmartPointers.md](SmartPointers.md) | `NkUniquePtr`, `NkSharedPtr`/`NkWeakPtr`, `NkIntrusivePtr` : sémantique, choix, pièges. | `NkUniquePtr.h`, `NkSharedPtr.h`, `NkIntrusivePtr.h` |
| [Memory-Operations.md](Memory-Operations.md) | Opérations mémoire : copy/move/set/compare, recherche, transformation, construct/destroy typés, alignement + variantes SIMD. | `NkFunction.h`, `NkFunctionSIMD.h` |
| [Tracking-Profiling.md](Tracking-Profiling.md) | Détection de fuites (`NkMemoryTracker`) et hooks de profiling (`NkMemoryProfiler`). | `NkTracker.h`, `NkProfiler.h` |
| [GarbageCollector.md](GarbageCollector.md) | GC mark-and-sweep optionnel. | `NkGc.h` |
| [Tags-Budgets.md](Tags-Budgets.md) | Budgets mémoire par sous-système (`NkMemoryTag` / `NkMemoryBudget`). | `NkTag.h` |
| [Pointer-Hash.md](Pointer-Hash.md) | Tables de hachage clé-pointeur (`NkPointerHashMap`/`Set`). | `NkHash.h` |

## Règle transverse

> **Toute** allocation passe par un allocateur NKMemory (`New`/`Delete`), **jamais**
> `new`/`delete` directs. Toute classe avec des méthodes de **création** expose les
> méthodes de **destruction** symétriques. Voir [Allocators.md](Allocators.md).

[← Récap NKMemory](../NKMemory.md) · [← Couche Foundation](../README.md)
