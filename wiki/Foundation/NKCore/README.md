# NKCore — documentation détaillée

Décomposition du module **NKCore** partie par partie. Récap synthétique :
[../NKCore.md](../NKCore.md).

| Fichier | Contenu | Headers |
|---------|---------|---------|
| [Types.md](Types.md) | Alias de types primitifs (`nk_uint32`, `nk_bool`, `nk_byte`, `float32`…). | `NkTypes.h` |
| [Atomics.md](Atomics.md) | `NkAtomic<T>`, `NkAtomicFlag`, `NkSpinLock`, `NkScopedSpinLock`, `NkMemoryOrder`. | `NkAtomic.h` |
| [Traits.md](Traits.md) | Traits de types (zéro-STL), `NkIsInvocable`, utilitaires de types. | `NkTraits.h`, `NkTypeUtils.h`, `NkInvoke.h` |
| [Vocabulary-Types.md](Vocabulary-Types.md) | `NkOptional<T>`, `NkVariant<...>`. | `NkOptional.h`, `NkVariant.h` |
| [Assertions.md](Assertions.md) | `NkAssert`, `NkAssertHandler`, `NkAssertAction`, debug break. | `Assert/*.h` |
| [Bits-Limits.md](Bits-Limits.md) | `NkBits`, `NkNumericLimits`, builtins. | `NkBits.h`, `NkLimits.h`, `NkBuiltin.h` |
| [Enumeration.md](Enumeration.md) | `NkEnumeration`. | `NkEnumeration.h` |
| [Platform-Config.md](Platform-Config.md) | Détection plateforme, `NkSourceLocation`, config, macros, version, exports. | `NkPlatform.h`, `NkConfig.h`, `NkMacros.h`, `NkVersion.h`… |

[← Récap NKCore](../NKCore.md) · [← Couche Foundation](../README.md)
