# NKPlatform — documentation détaillée

Le module **NKPlatform**, partie par partie. Pour une vue d'ensemble et un guide « par où
commencer », voir le récap : [../NKPlatform.md](../NKPlatform.md).

Chaque page suit la même structure : un **tutoriel** narratif, un **aperçu** tabulaire de
toute l'API, puis une **référence-cours** où chaque macro / symbole est expliqué avec ses
conditions de définition, ses pièges et ses cas d'usage concrets.

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [Detection.md](Detection.md) | Détection compile-time OS / architecture CPU / compilateur : macros `NKENTSEU_PLATFORM_*`, `NKENTSEU_ARCH_*`, `NKENTSEU_COMPILER_*`, standard C++, attributs portables, et le patron `_ONLY`/`NOT_` pour le code conditionnel. | `NkPlatformDetect.h`, `NkArchDetect.h`, `NkCompilerDetect.h` |
| [CPU-Endianness.md](CPU-Endianness.md) | Features CPU **runtime** (`CPUFeatures::Get()`, `NkHasSSE2/AVX2/NEON`…), endianness (`NkIsLittleEndian`, `ByteSwap*`, `HostToNetwork*`, `Read/Write LE/BE`) et détection API graphique/compute (`NkGraphicsApi`, `NkGPUVendor`, macros `*_AVAILABLE`). | `NkCPUFeatures.h`, `NkEndianness.h`, `NkCGXDetect.h` |
| [Export-Inline.md](Export-Inline.md) | Décoration / visibilité des symboles (`NKENTSEU_PLATFORM_API`, `NKENTSEU_CLASS_EXPORT`, `NKENTSEU_API_LOCAL`, `EXTERN_C_*`, modes shared/static/import) et spécificateurs d'inline / optimisation (`INLINE`, `FORCE_INLINE`, `NO_INLINE`, `LIKELY`/`UNLIKELY`, `PURE`/`CONST`/`HOT`/`COLD`). | `NkPlatformExport.h`, `NkPlatformInline.h` |
| [Environment-Logging.md](Environment-Logging.md) | Variables d'environnement portables sans STL (`NkGet`/`NkSet`/`NkUnset`/`NkExists`, `NkPrependToPath`, conteneurs `NkEnvString`/`NkEnvVector`/`NkEnvUMap`), logging minimaliste (`NK_FOUNDATION_LOG_*` + `_VALUE`, sink, niveaux compile-time) et config/capacités matérielles (`GetPlatformConfig`/`GetPlatformCapabilities`, macros chemins/features/build). | `NkEnv.h`, `NkFoundationLog.h`, `NkPlatformConfig.h` |

[← Récap NKPlatform](../NKPlatform.md) · [← Couche Foundation](../README.md)
