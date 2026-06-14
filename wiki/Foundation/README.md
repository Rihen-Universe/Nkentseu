# Couche Foundation

La couche **Foundation** est le socle du moteur : elle ne dépend d'**aucune** autre
couche Nkentseu (uniquement du compilateur/OS). Tout le reste du moteur est bâti
dessus. Elle applique le principe **zéro-STL** (conteneurs, allocateurs et smart
pointers maison) et la règle **« toute allocation passe par NKMemory »**.

## Modules

| Module | Rôle | Doc |
|--------|------|-----|
| **NKCore** | Types primitifs (`nk_uint32`…), traits, atomics, `NkSpinLock`, utilitaires bas niveau | [NKCore.md](NKCore.md) |
| **NKMemory** | Allocateurs, smart pointers (`NkUniquePtr`/`NkSharedPtr`/`NkIntrusivePtr`), tracking de fuites, GC, profiling | [NKMemory.md](NKMemory.md) |
| **NKContainers** | Conteneurs zéro-STL : `NkVector`, `NkString`, `NkHashMap`, `NkArray`… | [NKContainers.md](NKContainers.md) |
| **NKMath** | Vecteurs, matrices, quaternions, couleurs, trigonométrie, géométrie 2D/3D | [NKMath.md](NKMath.md) |
| **NKPlatform** | Détection plateforme (OS/compilo/arch), abstractions système de bas niveau | [NKPlatform.md](NKPlatform.md) |

## Ordre de dépendance interne

```
NKCore  (types, traits, atomics)
   ▲
   ├── NKMemory      (alloue via les types NKCore)
   ├── NKMath        (utilise les types NKCore)
   └── NKPlatform    (détection bas niveau)
         ▲
         └── NKContainers  (conteneurs : allouent via NKMemory, types NKCore)
```

> **Règle dure** : à partir de NKMemory, **toute** création/destruction d'objet passe
> par l'allocateur (`memory::NkGetDefaultAllocator().New<T>()` / `.Delete(ptr)`), jamais
> `new`/`delete` directs. Voir [Conventions](../Conventions.md) et [NKMemory.md](NKMemory.md).

## Conventions de nommage (rappel)

- `NkPascalCase` — classes, structs, méthodes publiques, fonctions.
- `NKI*` — interfaces abstraites (ex. `NkIRenderer2D`).
- `mCamelCase` — membres privés.
- `NKENTSEU_UPPER_SNAKE` — macros.
- `nk_snake_case` — alias de types primitifs (`nk_uint32`, `nk_size`…).

Détail complet : [Conventions.md](../Conventions.md).
