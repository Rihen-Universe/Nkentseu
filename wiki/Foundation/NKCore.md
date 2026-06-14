# NKCore

> Couche **Foundation** · Briques fondamentales : types primitifs, atomiques &
> synchronisation, traits de types (zéro-STL), types-vocabulaire (`NkOptional`/`NkVariant`),
> assertions, manipulation de bits, détection plateforme.

NKCore est le module **le plus bas** de la pile : il ne dépend que du compilateur. Il
fournit les types et utilitaires que **tous** les autres modules utilisent (y compris
NKMemory). C'est l'équivalent maison de `<cstdint>`, `<type_traits>`, `<atomic>`,
`<optional>`, `<variant>`, `<limits>` — en **zéro-STL**.

- **Namespace** : `nkentseu` (types), `nkentseu::core` selon les parties
- **Header parapluie** : `#include "NKCore/NKCore.h"`

---

## 1. Documentation détaillée

| Partie | Contenu | Headers |
|--------|---------|---------|
| [Types](NKCore/Types.md) | Alias de types primitifs (`nk_uint32`, `nk_bool`, `nk_byte`…). | `NkTypes.h` |
| [Atomics & synchro](NKCore/Atomics.md) | `NkSpinLock`, `NkAtomicFlag`, `NkScopedSpinLock`, `NkMemoryOrder`. | `NkAtomic.h` |
| [Traits](NKCore/Traits.md) | Traits de types (zéro-STL `<type_traits>`), `NkIsInvocable`, helpers de variant. | `NkTraits.h`, `NkTypeUtils.h`, `NkInvoke.h` |
| [Types-vocabulaire](NKCore/Vocabulary-Types.md) | `NkOptional<T>`, `NkVariant<...>`. | `NkOptional.h`, `NkVariant.h` |
| [Assertions](NKCore/Assertions.md) | `NkAssert`, `NkAssertHandler`, `NkAssertAction`, debug break. | `Assert/*.h` |
| [Bits & limites](NKCore/Bits-Limits.md) | `NkBits`, `NkNumericLimits`, builtins compilateur. | `NkBits.h`, `NkLimits.h`, `NkBuiltin.h` |
| [Énumération](NKCore/Enumeration.md) | `NkEnumeration` (réflexion/itération d'enums). | `NkEnumeration.h` |
| [Plateforme & config](NKCore/Platform-Config.md) | Détection plateforme (`NkPlatformType`, `NkArchitectureType`, `NkSourceLocation`…), config, macros, version, exports. | `NkPlatform.h`, `NkConfig.h`, `NkMacros.h`, `NkVersion.h`, `NkExport.h`… |

---

## 2. Aperçu des éléments majeurs

### Types primitifs (`NkTypes.h`)
Alias stables et portables : `nk_bool`, `nk_byte`, `nk_int8/16/32/64`,
`nk_uint8/16/32/64`, `nk_size`, `nk_uptr`, `float32/64`… Base de tout le moteur (les
aliases courts `int32`, `uint32`, `usize` en dérivent).

### Atomiques & synchronisation (`NkAtomic.h`)
- `NkSpinLock` — verrou par attente active (léger, sections critiques courtes).
- `NkScopedSpinLock` — RAII autour d'un `NkSpinLock`.
- `NkAtomicFlag` — drapeau atomique.
- `enum class NkMemoryOrder` — ordres mémoire (relaxed/acquire/release…).

### Traits (`NkTraits.h`, `NkTypeUtils.h`, `NkInvoke.h`)
Métaprogrammation maison : `NkConditional`, `NkConjunction`, `NkAddPointer`,
`NkAddLValueReference`, `NkBitSize`, `NkIsInvocable`, etc. — l'équivalent de
`<type_traits>` sans la STL.

### Types-vocabulaire (`NkOptional.h`, `NkVariant.h`)
- `NkOptional<T>` (+ `NkNullOpt`) — valeur optionnelle (équivalent `std::optional`).
- `NkVariant<...>` — union type-safe (équivalent `std::variant`), avec helpers
  `NkTypeAt`, `NkTypeIndex`, `NkMaxSizeOf`, `NkMaxAlignOf`.

### Assertions (`Assert/*.h`)
- `NkAssertHandler`, `enum class NkAssertAction`, `struct NkAssertionInfo` —
  système d'assertions configurable.
- `NkDebugBreak.h` — point d'arrêt débogueur portable.

### Bits & limites (`NkBits.h`, `NkLimits.h`)
- `NkBits` — manipulation de bits (set/clear/test/count…).
- `NkNumericLimits` — bornes numériques (équivalent `std::numeric_limits`).

### Plateforme (`NkPlatform.h`)
`enum class NkPlatformType / NkArchitectureType / NkDisplayType / NkGraphicsAPI`,
`struct NkPlatformInfo`, `class NkSourceLocation` (fichier/ligne/fonction, pour le debug
et le tracking mémoire).

---

## 3. Index des 23 headers

| Header | Contenu |
|--------|---------|
| `NKCore.h` | Parapluie (inclut tout). |
| `NkTypes.h` | Alias de types primitifs. |
| `NkAtomic.h` | `NkSpinLock`, `NkAtomicFlag`, `NkScopedSpinLock`, `NkMemoryOrder`. |
| `NkTraits.h` | Traits de types. |
| `NkTypeUtils.h` | Utilitaires de types complémentaires. |
| `NkInvoke.h` | `NkIsInvocable` (invocabilité). |
| `NkOptional.h` | `NkOptional<T>`, `NkNullOpt`. |
| `NkVariant.h` | `NkVariant<...>` + helpers de types. |
| `NkBits.h` | `NkBits`. |
| `NkLimits.h` | `NkNumericLimits`. |
| `NkBuiltin.h` | Wrappers de builtins compilateur. |
| `NkEnumeration.h` | `NkEnumeration`. |
| `NkPlatform.h` | Détection plateforme/arch/API + `NkSourceLocation`. |
| `NkConfig.h` | Configuration de compilation. |
| `NkMacros.h` | Macros utilitaires. |
| `NkVersion.h` | Version du moteur. |
| `NkLimits.h` | Limites numériques. |
| `Assert/NkAssert.h` | Macros d'assertion. |
| `Assert/NkAssertHandler.h` | `NkAssertHandler`. |
| `Assert/NkAssertion.h` | `NkAssertAction`, `NkAssertionInfo`. |
| `Assert/NkDebugBreak.h` | Point d'arrêt débogueur. |
| `NkCoreApi.h` · `NkCoreExport.h` · `NkExport.h` | Macros d'export/visibilité. |

---

[← Couche Foundation](README.md) · [Index du wiki](../README.md)
