# NKPlatform

> Couche **Foundation** · La couche d'abstraction plateforme du moteur : détection
> compile-time de l'OS / du CPU / du compilateur, endianness, export & inline de symboles,
> environnement, logging bas niveau et configuration matérielle.

Avant qu'une seule ligne de code « métier » ne s'exécute, le moteur doit savoir **où** il
tourne : Windows ou Linux ? x86_64 ou ARM64 ? little ou big endian ? MSVC, GCC ou Clang ?
Quel jeu d'instructions SIMD est disponible ? Quelle API graphique par défaut ? NKPlatform
répond à toutes ces questions, l'essentiel **à la compilation** (zéro coût runtime) via un
vaste arsenal de macros préprocesseur, et le reste **au démarrage** via une poignée de
singletons (features CPU, capacités hardware).

C'est la toute première brique : chaque autre module en dépend, ne serait-ce que pour
décorer ses symboles publics (`NKENTSEU_PLATFORM_API`), forcer l'inlining d'un hot path
(`NKENTSEU_FORCE_INLINE`) ou émettre du code conditionnel par plateforme
(`NKENTSEU_WINDOWS_ONLY({ ... });`). Comprendre NKPlatform, c'est comprendre comment le
moteur reste portable sans `#ifdef` éparpillés partout.

La grande majorité du module est **macro-only** : tout est résolu par le préprocesseur. Les
seuls vrais symboles C++ vivent dans `NkCPUFeatures.h`, `NkEndianness.h`, `NkEnv.h`,
`NkFoundationLog.h`, `NkPlatformConfig.h` et `NkCGXDetect.h` (enums GPU).

- **Namespace** : `nkentseu::platform` (+ `nkentseu::env` pour l'environnement,
  `nkentseu::graphics` pour les enums GPU)
- **Header parapluie** : `#include "NKPlatform/NKPlatform.h"`

---

## Par où commencer

Selon ce que vous cherchez à faire :

| Besoin | Partie |
|--------|--------|
| Savoir sur quel OS / arch / compilateur on compile, émettre du code conditionnel | [Détection](NKPlatform/Detection.md) |
| Tester les features CPU au runtime (SSE2/AVX2/NEON), gérer l'endianness, détecter l'API graphique | [CPU, endianness & GPU](NKPlatform/CPU-Endianness.md) |
| Décorer les symboles publics (DLL/statique) et contrôler l'inlining / l'optimisation | [Export & inline](NKPlatform/Export-Inline.md) |
| Lire/écrire des variables d'environnement, logger en couche basse, interroger le hardware | [Environnement & logging](NKPlatform/Environment-Logging.md) |

Chaque page suit la même structure : un **tutoriel** narratif, un **aperçu** tabulaire de
toute l'API, puis une **référence-cours** où chaque macro / symbole est expliqué avec ses
conditions de définition, ses pièges et ses cas d'usage concrets.

---

## Aperçu des familles

- **Détection** (`NkPlatformDetect.h`, `NkArchDetect.h`, `NkCompilerDetect.h`) — macros
  compile-time `NKENTSEU_PLATFORM_*` (Windows/Linux/macOS/iOS/Android/Emscripten/HarmonyOS +
  consoles + embarqué, catégories Desktop/Mobile/Console/Embedded), `NKENTSEU_ARCH_*`
  (x86_64/ARM64/RISC-V…, 64BIT/32BIT, endianness, SIMD, tailles cache/page),
  `NKENTSEU_COMPILER_*` (MSVC/GCC/Clang/Intel + standard C++ + attributs portables), et le
  patron `NKENTSEU_<X>_ONLY(...)` / `NKENTSEU_NOT_<X>(...)` pour le code conditionnel.
- **CPU, endianness & GPU** (`NkCPUFeatures.h`, `NkEndianness.h`, `NkCGXDetect.h`) — détection
  CPU **runtime** (`CPUFeatures::Get()`, helpers `NkHasSSE2()`/`NkHasAVX2()`/`NkHasNEON()`,
  topologie, cache), endianness (`NkIsLittleEndian()` constexpr, `ByteSwap16/32/64`,
  `HostToNetwork*`/`NetworkToHost*`, `ReadBE*`/`WriteLE*`), et détection des API
  graphiques/compute (`NkGraphicsApi`, `NkGPUVendor`, `NkGPUType`, macros `*_AVAILABLE`).
- **Export & inline** (`NkPlatformExport.h`, `NkPlatformInline.h`) — décoration des symboles
  (`NKENTSEU_PLATFORM_API`, `NKENTSEU_CLASS_EXPORT`, `NKENTSEU_API_LOCAL`, liaison C
  `NKENTSEU_EXTERN_C_*`), modes shared/static/import, et spécificateurs d'inlining /
  optimisation (`NKENTSEU_INLINE`, `NKENTSEU_FORCE_INLINE`, `NKENTSEU_NO_INLINE`,
  `NKENTSEU_LIKELY`/`UNLIKELY`, `PURE`/`CONST`/`HOT`/`COLD`/`NORETURN`).
- **Environnement & logging** (`NkEnv.h`, `NkFoundationLog.h`, `NkPlatformConfig.h`) —
  variables d'environnement portables sans STL (`NkGet`/`NkSet`/`NkUnset`/`NkExists`,
  `NkPrependToPath`, conteneurs maison `NkEnvString`/`NkEnvVector`/`NkEnvUMap`), logging
  minimaliste filtré à la compilation (`NK_FOUNDATION_LOG_ERROR/WARN/INFO/DEBUG/TRACE` +
  `_VALUE`, sink redirigeable), et config/capacités matérielles
  (`GetPlatformConfig()`/`GetPlatformCapabilities()`, macros `NKENTSEU_PATH_SEPARATOR`,
  `NKENTSEU_MAX_PATH`, `NKENTSEU_HAS_*`).

---

## Index des 12 headers

| Header | Contenu | Documenté dans |
|--------|---------|----------------|
| `NKPlatform.h` | Parapluie (inclut tout le module). | — |
| `NkPlatformDetect.h` | Macros OS, fenêtrage Linux, consoles, catégories, `_ONLY`/`NOT_`. | [Détection](NKPlatform/Detection.md) |
| `NkArchDetect.h` | Macros architecture CPU, bitness, endianness, SIMD compile-time, alignements. | [Détection](NKPlatform/Detection.md) |
| `NkCompilerDetect.h` | Macros compilateur, standard C++, attributs portables, pragmas. | [Détection](NKPlatform/Detection.md) |
| `NkCPUFeatures.h` | Détection CPU runtime (`CPUFeatures::Get()`, `NkHasSSE2()`…), topologie, cache. | [CPU, endianness & GPU](NKPlatform/CPU-Endianness.md) |
| `NkEndianness.h` | `NkEndianness`, `NkIsLittleEndian()`, `ByteSwap*`, `HostToNetwork*`, `Read/Write LE/BE`. | [CPU, endianness & GPU](NKPlatform/CPU-Endianness.md) |
| `NkCGXDetect.h` | Enums `NkGraphicsApi`/`NkGPUVendor`/`NkGPUType`, macros `*_AVAILABLE`, compute, GFX actif. | [CPU, endianness & GPU](NKPlatform/CPU-Endianness.md) |
| `NkPlatformExport.h` | `NKENTSEU_PLATFORM_API`, `NKENTSEU_CLASS_EXPORT`, `NKENTSEU_API_LOCAL`, `EXTERN_C_*`. | [Export & inline](NKPlatform/Export-Inline.md) |
| `NkPlatformInline.h` | `NKENTSEU_INLINE`, `FORCE_INLINE`, `NO_INLINE`, `LIKELY`/`UNLIKELY`, `API_*INLINE`. | [Export & inline](NKPlatform/Export-Inline.md) |
| `NkEnv.h` | Environnement (`NkGet`/`NkSet`…), conteneurs `NkEnvString`/`NkEnvVector`/`NkEnvUMap`. | [Environnement & logging](NKPlatform/Environment-Logging.md) |
| `NkFoundationLog.h` | Logging bas niveau (`NK_FOUNDATION_LOG_*`, sink), niveaux compile-time. | [Environnement & logging](NKPlatform/Environment-Logging.md) |
| `NkPlatformConfig.h` | `GetPlatformConfig()`/`GetPlatformCapabilities()`, macros chemins/features/build. | [Environnement & logging](NKPlatform/Environment-Logging.md) |

---

[← Couche Foundation](README.md) · [Index du wiki](../README.md)
