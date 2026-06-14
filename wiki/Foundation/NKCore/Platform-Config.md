# Plateforme et configuration

> Couche **Foundation** · NKCore · Savoir **où** le moteur s'exécute (système, architecture,
> API graphique), tracer **d'où vient** chaque ligne de code (`NkSourceLocation`), et régler le
> comportement à la compilation via les drapeaux `NKENTSEU_*` (config, macros, version, export).

Un moteur cross-plateforme a constamment besoin de savoir *où* il s'exécute : sur quel système,
quelle architecture, avec quelle API graphique, avec combien de cœurs, sous quel mode de build.
Sans cette connaissance, impossible d'inclure le bon header système, de choisir le bon chemin de
code SIMD, d'adapter le nombre de threads, ou même de dire à un développeur *où exactement* une
assertion a sauté. NKCore centralise toute cette information et y ajoute les utilitaires transverses
qui s'appuient dessus : traçage de la source du code, macros de service, version du framework, et
export de symboles pour la compilation en bibliothèque partagée.

La règle d'or de ce chapitre est la distinction entre **compilation** et **exécution**. Certaines
décisions doivent être tranchées par le préprocesseur, avant même que le programme existe (inclure
tel header, compiler tel chemin de code) : ce sont les macros `NKENTSEU_PLATFORM_*` et les fonctions
`inline constexpr`. D'autres ne se découvrent qu'au lancement (combien de cœurs ? combien de RAM
disponible ? quel jeu d'instructions SIMD ?) : c'est le rôle de `NkPlatformInfo` et de l'API C
runtime. Les deux mécanismes sont **complémentaires**, jamais concurrents.

- **Namespace** : `nkentseu` (helpers compile-time), `nkentseu::platform` (types et enums),
  `nkentseu::arch` (utilitaires CPU/alignement), `nkentseu::memory` (allocation alignée).
- **Headers** : `#include "NKCore/NkPlatform.h"` (détection, `NkSourceLocation`, API C),
  `NkConfig.h` (drapeaux), `NkMacros.h` (macros utilitaires), `NkVersion.h` (version),
  `NkCoreApi.h` / `NkExport.h` (export).

> **Convention transverse.** Tout est sous `namespace nkentseu`. Les macros sont préfixées
> `NKENTSEU_` (UPPER_SNAKE) ; quelques helpers gardent le préfixe court `NK_` (`NK_MIN`, `NK_MAX`…).
> Les chaînes retournées (`osName`, `versionString`, noms de plateforme…) pointent vers du
> **statique** : on les lit, on ne les libère **jamais**.

---

## Détecter la plateforme

L'environnement est décrit par une poignée d'**énumérations compactes** (toutes sur `nk_uint8`,
avec une valeur `0` = inconnu/aucun, donc une initialisation par défaut sûre et des valeurs stables
pour la sérialisation). `NkPlatformType` énumère les systèmes (`NK_WINDOWS`, `NK_LINUX`, `NK_MACOS`,
`NK_IOS`, `NK_ANDROID`, `NK_HARMONYOS`, `NK_EMSCRIPTEN`, les consoles, les BSD…) ;
`NkArchitectureType` les architectures (`NK_X64`, `NK_ARM64`, `NK_WASM`, `NK_RISCV64`…) ;
`NkDisplayType` le serveur d'affichage (`NK_WIN32`, `NK_COCOA`, `NK_WAYLAND`, `NK_XCB`…) ; et
`NkGraphicsAPI` l'API de rendu (`NK_VULKAN`, `NK_METAL`, `NK_OPENGL`, `NK_DIRECTX`, `NK_SOFTWARE`).

Regroupées, ces enums forment un `NkPlatformInfo` — la **fiche d'identité complète** de
l'environnement, peuplée une seule fois au démarrage et lisible à l'exécution : OS et compilateur,
nombre de cœurs et de threads, tailles de caches, RAM totale et disponible, jeux d'instructions SIMD
disponibles, *endianness*, mode de build, et l'API graphique choisie. On l'obtient via
`NkGetPlatformInfo()` (l'initialisation est paresseuse et thread-safe).

```cpp
const NkPlatformInfo* info = NkGetPlatformInfo();
nk_uint32 cores  = info->cpuCoreCount;       // dimensionner un thread pool
nk_bool   avx2   = info->hasAVX2;            // activer un chemin SIMD
nk_uint64 ramMb  = info->totalMemory / (1024 * 1024);
```

Pour ce qui doit être tranché **à la compilation**, on n'utilise pas `NkPlatformInfo` mais les macros
`NKENTSEU_PLATFORM_*` (définies dans `NkPlatformDetect.h`) ou, plus lisible, les fonctions
`inline constexpr` qui les enveloppent :

```cpp
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    // header système Windows, choisi par le préprocesseur
#endif

if constexpr (nkentseu::NkIsDesktop()) {
    // chemin desktop, éliminé du binaire sur mobile/console/web
}
```

Ce n'est **pas** la même chose qu'un `if` runtime : `if constexpr` sur `NkIsDesktop()` supprime
purement et simplement le code mort du binaire, alors que `NkGetPlatformInfo()->architecture` se lit
pendant l'exécution. On prend le premier pour ce que le compilateur sait déjà, le second pour ce qui
se découvre au lancement.

> **En résumé.** Quatre enums `nk_uint8` (plateforme / architecture / affichage / API graphique)
> + l'agrégat `NkPlatformInfo` décrivent l'environnement. Macros `NKENTSEU_PLATFORM_*` et fonctions
> `inline constexpr` (`NkIsDesktop`, `NkGetPlatformName`…) pour la **compilation** ;
> `NkGetPlatformInfo()` pour l'**exécution**. `0` = inconnu, et les chaînes sont statiques.

---

## Savoir d'où vient le code : NkSourceLocation

`NkSourceLocation` capture le **fichier, la fonction, la ligne et la colonne** à l'endroit exact de
son appel — l'équivalent maison de `std::source_location`. C'est une petite classe **immuable** et
entièrement `constexpr noexcept` : on n'en hérite rien, on n'en paie rien à l'exécution. On la
fabrique via la *factory* `Current()`, dont les paramètres ont pour valeurs par défaut les builtins
de localisation (`NKENTSEU_BUILTIN_FILE`, `NKENTSEU_BUILTIN_FUNCTION`, `NKENTSEU_BUILTIN_LINE`,
`NKENTSEU_BUILTIN_COLUMN`).

L'idiome consacré n'est pas d'appeler `Current()` à la main, mais de la poser comme **valeur par
défaut d'un argument** de fonction, via la macro-objet `NkCurrentSourceLocation` (sans parenthèses) :

```cpp
void Log(const nk_char* message,
         NkSourceLocation loc = NkCurrentSourceLocation);   // capté chez l'appelant
```

Ce n'est pas un gadget : c'est exactement cette information qui rend exploitables les **assertions**
(« échec à `NkPhysics.cpp:214`, dans `ResolveContact` ») et le
[traceur de fuites](../NKMemory.md) (« allocation non libérée, créée dans tel fichier, telle
fonction »). Sans `NkSourceLocation`, ces outils diraient seulement « il y a un problème » ; avec,
ils disent *où*.

> Notez que `NkSourceLocation` vit dans le namespace `nkentseu::platform`, et que la macro
> `NkCurrentSourceLocation` (sans `()`) est à préférer à l'ancien alias `NkCurrentLocation()`
> (avec `()`), déprécié.

---

## Configuration, macros, version, export

Le reste du chapitre regroupe des fichiers de **service**, tous à base de macros préprocesseur.

`NkConfig.h` porte les **drapeaux de configuration de compilation**, dérivés automatiquement du mode
de build : `NKENTSEU_DEBUG` / `NKENTSEU_RELEASE` / `NKENTSEU_DISTRIBUTION` pilotent en cascade les
assertions (`NKENTSEU_ENABLE_ASSERTS`), le tracking mémoire, le niveau de log
(`NKENTSEU_LOG_LEVEL`), le profiling, le SIMD (`NKENTSEU_ENABLE_SIMD`), les valeurs par défaut des
conteneurs et des chaînes, ainsi que le backend graphique par défaut de la plateforme. C'est le
panneau de contrôle compile-time du moteur.

`NkMacros.h` rassemble les **macros utilitaires** : stringification et concaténation, manipulation de
bits, tailles mémoire lisibles (`NKENTSEU_MEGABYTES(x)`), `NKENTSEU_UNUSED(x)` pour faire taire un
argument inutilisé, les messages de compilation `NKENTSEU_TODO(msg)` / `NKENTSEU_FIXME(msg)`, les
hints d'optimisation (`NKENTSEU_LIKELY`/`NKENTSEU_UNLIKELY`), et les helpers `NK_MIN`/`NK_MAX`/
`NK_CLAMP`. `NkVersion.h` expose la **version du framework** (majeur/mineur/patch encodés) et les
informations de build (date, horodatage). Enfin, `NkCoreApi.h` et son alias `NkExport.h` définissent
les macros de **visibilité** (`NKENTSEU_CORE_API` et dérivées) qui marquent les symboles exportés
quand le moteur est compilé en bibliothèque partagée.

> **En résumé.** `NkConfig.h` = drapeaux de build (debug/release → asserts, log, SIMD…) ;
> `NkMacros.h` = macros de service (bits, tailles, `NKENTSEU_UNUSED`, `NKENTSEU_TODO`,
> `NK_MIN`/`NK_MAX`…) ; `NkVersion.h` = version et build info ; `NkCoreApi.h`/`NkExport.h` =
> export de symboles (`NKENTSEU_CORE_API`). Tout en compile-time.

---

## Aperçu de l'API

Tous les éléments publics, par fichier. Le détail (comportement, complexité, cas d'usage) est dans la
« Référence complète ».

### `NkPlatform.h` — détection, source location, runtime

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Builtins source | `NKENTSEU_BUILTIN_FILE` `NKENTSEU_BUILTIN_FUNCTION` `NKENTSEU_BUILTIN_LINE` `NKENTSEU_BUILTIN_COLUMN` | Localisation source brute (builtins compilateur ou fallback). |
| Capture source | `NkCurrentSourceLocation` (sans `()`) | Valeur par défaut d'argument → `NkSourceLocation::Current(...)`. |
| Capture source | `NkCurrentLocation()` (**déprécié**) | Ancien alias, utilise `__FUNCTION__`. |
| Type version | `using NkVersion = nk_uint32` | Version encodée. |
| Enum | `NkPlatformType` (`NK_UNKNOWN`, `NK_WINDOWS`, `NK_LINUX`, `NK_MACOS`, `NK_ANDROID`…) | Système d'exploitation. |
| Enum | `NkArchitectureType` (`NK_X86`, `NK_X64`, `NK_ARM64`, `NK_WASM`…) | Architecture CPU. |
| Enum | `NkDisplayType` (`NK_WIN32`, `NK_COCOA`, `NK_WAYLAND`, `NK_XCB`…) | Serveur d'affichage. |
| Enum | `NkGraphicsAPI` (`NK_VULKAN`, `NK_METAL`, `NK_OPENGL`, `NK_DIRECTX`, `NK_SOFTWARE`) | API de rendu. |
| Struct | `NkVersionInfo` | `major`/`minor`/`patch` + `versionString`. |
| Struct | `NkPlatformInfo` | Agrégat runtime complet (OS, CPU, RAM, SIMD, build, GFX). |
| Classe | `NkSourceLocation` · `FileName` `FunctionName` `Line` `Column` `Current` | Localisation immuable `constexpr noexcept`. |
| C runtime | `NkGetPlatformInfo` `NkInitializePlatformInfo` | Accès / init (lazy, thread-safe) à l'agrégat. |
| C runtime | `NkGetPlatformName` `NkGetArchitectureName` `NkPrintPlatformInfo` | Noms (statiques) ; dump diagnostique. |
| C runtime | `NkHasSIMDFeature` | Test SIMD par nom (`"AVX2"`…). |
| C runtime · CPU | `NkGetCPUCoreCount` `NkGetCPUThreadCount` `NkGetL1/L2/L3CacheSize` `NkGetCacheLineSize` | Topologie CPU. |
| C runtime · Mémoire | `NkGetTotalMemory` `NkGetAvailableMemory` `NkGetPageSize` `NkGetAllocationGranularity` | Mémoire système. |
| C runtime · Build | `NkIsDebugBuild` `NkIsSharedLibrary` `NkGetBuildType` | Infos de build. |
| C runtime · Arch | `NkGetEndianness` `NkIs64Bit` | *Endianness*, largeur. |
| C runtime · Align | `NkIsAligned` `NkAlignAddress` `NkAlignAddressConst` | Alignement d'adresses. |
| Compile-time (`nkentseu`) | `NkGetPlatformName` `NkGetPlatformVersion` `NkIsDesktop` `NkIsMobile` `NkIsConsole` `NkIsEmbedded` `NkIsWeb` | Catégorie de plateforme, zéro-overhead. |
| `nkentseu::arch` | `NkGetArchName` `NkGetArchVersion` `NkIs64Bit` `NkIs32Bit` `NkIsLittleEndian` `NkIsBigEndian` `NkGetCacheLineSize` `NkGetPageSize` `NkGetWordSize` | Caractéristiques CPU compile-time. |
| `nkentseu::arch` (templates) | `NkAlignUp` `NkAlignDown` `NkIsAligned` `NkCalculatePadding` | Calculs d'alignement typés. |
| `nkentseu::memory` | `NkAllocateAligned` `NkFreeAligned` `NkIsPointerAligned` `NkAllocateAlignedArray` `NkConstructAligned` `NkDestroyAligned` | Allocation/construction alignée. |
| Macro | `NkAllocPageAligned(size)` | Allocation alignée sur la taille de page. |

### `NkConfig.h` — drapeaux de build

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Mode | `NKENTSEU_DEBUG` `NKENTSEU_RELEASE` `NKENTSEU_DISTRIBUTION` | Mode de build (mutuellement exclusifs). |
| Asserts | `NKENTSEU_ENABLE_ASSERTS` `NKENTSEU_FORCE_ENABLE_ASSERTS` `NKENTSEU_FORCE_DISABLE_ASSERTS` | Activation des assertions. |
| Mémoire | `NKENTSEU_ENABLE_MEMORY_TRACKING` `NKENTSEU_ENABLE_LEAK_DETECTION` `NKENTSEU_ENABLE_MEMORY_STATS` | Suivi mémoire (debug). |
| Logging | `NKENTSEU_LOG_LEVEL` `NKENTSEU_ENABLE_FILE_LOGGING` | Niveau (0–5) et log fichier. |
| Profiling | `NKENTSEU_ENABLE_PROFILING` `NKENTSEU_ENABLE_INSTRUMENTATION` | Profilage / instrumentation. |
| SIMD/threads | `NKENTSEU_ENABLE_SIMD` `NKENTSEU_ENABLE_THREADING` `NKENTSEU_DEFAULT_THREAD_COUNT` | SIMD détecté, threading, nb threads. |
| Allocateur | `NKENTSEU_DEFAULT_ALIGNMENT` `NKENTSEU_DEFAULT_PAGE_SIZE` `NKENTSEU_DEFAULT_ALLOCATOR` | Réglages allocateur par défaut. |
| Strings | `NKENTSEU_STRING_DEFAULT_CAPACITY` `NKENTSEU_ENABLE_STRING_SSO` `NKENTSEU_STRING_SSO_SIZE` `NK_STRING_SSO_SIZE` | Capacité et SSO des chaînes. |
| Conteneurs | `NKENTSEU_VECTOR_DEFAULT_CAPACITY` `NKENTSEU_VECTOR_GROWTH_FACTOR` `NKENTSEU_HASHMAP_DEFAULT_CAPACITY` `NKENTSEU_HASHMAP_MAX_LOAD_FACTOR` | Défauts des conteneurs. |
| Math | `NKENTSEU_MATH_PRECISION_FLOAT`/`_DOUBLE` `NKENTSEU_MATH_EPSILON` `NKENTSEU_PI` (+ `_DOUBLE`) | Précision et constantes math. |
| Graphique/plateforme | `NKENTSEU_ENABLE_REFLECTION` `NKENTSEU_ENABLE_EXCEPTIONS` `NKENTSEU_USE_WIN32_API` `NKENTSEU_USE_POSIX_API` `NKENTSEU_GRAPHICS_BACKEND_DEFAULT` | Réflexion, exceptions, API OS, backend GFX. |
| Conditionnel | `NKENTSEU_IS_DEBUG` `NKENTSEU_IS_RELEASE` `NKENTSEU_DEBUG_ONLY(code)` `NKENTSEU_RELEASE_ONLY(code)` | Branches debug/release. |

### `NkMacros.h` — macros utilitaires

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Texte | `NKENTSEU_STRINGIFY(x)` `NKENTSEU_CONCAT(a,b)` (`_CONCAT3`/`_CONCAT4`) | Stringification, concaténation. |
| Tableaux | `NKENTSEU_ARRAY_SIZE(arr)` `NKENTSEU_VA_ARGS_COUNT(...)` | Taille de tableau, comptage variadique. |
| Bits | `NKENTSEU_BIT(x)` `NKENTSEU_BIT64(x)` `NKENTSEU_BIT_TEST/SET/CLEAR/TOGGLE(v,b)` | Manipulation de drapeaux binaires. |
| Tailles | `NKENTSEU_KILOBYTES/MEGABYTES/GIGABYTES/TERABYTES(x)` | Tailles mémoire lisibles. |
| Alignement | `NKENTSEU_ALIGN_PTR(ptr,a)` | Aligner un pointeur. |
| Min/Max | `NK_MIN(a,b)` `NK_MAX(a,b)` `NK_CLAMP(v,lo,hi)` `NK_ABS(x)` `NK_SWAP(a,b,type)` | Helpers numériques (préfixe `NK_`). |
| Inutilisé | `NKENTSEU_UNUSED(x)` (`_UNUSED2/3/4`) | Marquer un argument non utilisé. |
| Structs | `NKENTSEU_OFFSETOF(type,member)` `NKENTSEU_CONTAINER_OF(ptr,type,member)` | Décalage de champ, conteneur depuis membre. |
| Blocs/scope | `NKENTSEU_BLOCK_BEGIN`/`_BLOCK_END` `NKENTSEU_DEFER(code)` | Bloc `do/while(0)`, scope guard (GCC/Clang). |
| Assertions | `NKENTSEU_STATIC_ASSERT(cond,msg)` | Vérification compile-time. |
| Types | `NKENTSEU_SAME_TYPE(a,b)` `NKENTSEU_SIZEOF_TYPE(type)` `NKENTSEU_SIZEOF_MEMBER(type,member)` | Comparaison/taille de types. |
| Sûreté arith. | `NKENTSEU_WILL_ADD_OVERFLOW(a,b,max)` `NKENTSEU_WILL_MUL_OVERFLOW(a,b,max)` | Détection de débordement. |
| Messages | `NKENTSEU_COMPILE_MESSAGE(msg)` `NKENTSEU_TODO(msg)` `NKENTSEU_FIXME(msg)` | Notes émises à la compilation. |
| Version | `NKENTSEU_VERSION_ENCODE(M,m,p)` `NKENTSEU_VERSION_MAJOR/MINOR/PATCH(v)` | Encodage/décodage de version. |
| Variadique | `NKENTSEU_FOR_EACH(f, ...)` | Applique `f` à chaque argument (max 4). |
| Pointeurs | `NKENTSEU_MASK_ADDRESS(ptr)` `NKENTSEU_POINTER_DISTANCE(p1,p2)` | Masquage page, distance signée. |
| Angles | `NKENTSEU_DEGREES_TO_RADIANS(d)` `NKENTSEU_RADIANS_TO_DEGREES(r)` | Conversions d'angle (double). |
| Hints | `NKENTSEU_LIKELY(x)` `NKENTSEU_UNLIKELY(x)` `NKENTSEU_UNREACHABLE()` | Indices d'optimisation au compilateur. |
| C99 | `NKENTSEU_STATIC_ARRAY(size)` | Paramètre tableau de taille minimale (C99). |

### `NkVersion.h` — version du framework

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Version core | `NKENTSEU_VERSION_CORE_MAJOR/MINOR/PATCH` `NKENTSEU_VERSION_CORE` `NKENTSEU_VERSION_CORE_STRING` | Version encodée + chaîne. |
| Identité | `NKENTSEU_FRAMEWORK_CORE_NAME` `NKENTSEU_FRAMEWORK_CORE_FULL_NAME` | `"NKCore"` / `"NKCore vX.Y.Z"`. |
| Build | `NKENTSEU_BUILD_DATE` `NKENTSEU_BUILD_TIME` `NKENTSEU_BUILD_TIMESTAMP` `NKENTSEU_BUILD_NUMBER` | Horodatage et numéro de build. |
| Version API | `NKENTSEU_API_VERSION_MAJOR/MINOR/PATCH` `NKENTSEU_API_VERSION` | Version de l'API publique. |
| Marqueurs API | `NKENTSEU_API_STABLE` `NKENTSEU_API_EXPERIMENTAL` `NKENTSEU_API_INTERNAL` | Stabilité d'une API. |
| Comparaison | `NKENTSEU_VERSION_AT_LEAST(M,m,p)` `NKENTSEU_VERSION_EQUALS(M,m,p)` | Tests de compatibilité. |

### `NkCoreApi.h` / `NkExport.h` — export de symboles

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Config build | `NKENTSEU_CORE_BUILD_SHARED_LIB` `NKENTSEU_CORE_STATIC_LIB` `NKENTSEU_CORE_HEADER_ONLY` | Mode de liaison (défini par le build). |
| Export principal | `NKENTSEU_CORE_API` `NKENTSEU_CORE_CLASS_EXPORT` | Visibilité export/import des symboles. |
| Inline + export | `NKENTSEU_CORE_API_INLINE` `NKENTSEU_CORE_API_FORCE_INLINE` `NKENTSEU_CORE_API_NO_INLINE` | Combinaisons inline/visibilité. |
| C API | `NKENTSEU_CORE_C_API` | `extern "C"` + export. |
| Réutilisées (NKPlatform) | `NKENTSEU_DEPRECATED` `NKENTSEU_DEPRECATED_MESSAGE(msg)` `NKENTSEU_ALIGN_CACHE/16/32/64` `NKENTSEU_EXTERN_C_BEGIN/END` `NKENTSEU_API_LOCAL` | Attributs partagés (zéro duplication). |

---

## Référence complète

### Les enums de plateforme

Les quatre énumérations partagent une discipline commune : base `nk_uint8` (compactes, sérialisables)
et valeur `0` réservée à l'inconnu (`NK_UNKNOWN` / `NK_NONE`), si bien qu'un `NkPlatformInfo`
fraîchement mis à zéro est dans un état neutre valide. L'ordre des valeurs est croissant et **stable**
— on peut donc les écrire telles quelles dans un fichier binaire et les relire sur une autre version.

- **`NkPlatformType`** — `NK_WINDOWS`, `NK_LINUX`, `NK_BSD`, `NK_MACOS`, `NK_IOS`, `NK_ANDROID`,
  `NK_HARMONYOS`, les consoles (`NK_NINTENDO_SWITCH`, `NK_PSP`…), `NK_EMSCRIPTEN` (Web), les variantes
  BSD. C'est ce qui pilote, côté **IO**, le choix d'une API fichier ; côté **fenêtrage/2D**, le
  backend natif ; côté **réseau**, la pile sockets.
- **`NkArchitectureType`** — `NK_X86`, `NK_X64`, `NK_ARM32`, `NK_ARM64`, `NK_MIPS`, `NK_RISCV32/64`,
  `NK_PPC32/64`, `NK_WASM`, `NK_ARM9`. Détermine, côté **math/physique**, le chemin SIMD (SSE vs
  NEON), et côté **mémoire**, la largeur de pointeur.
- **`NkDisplayType`** — `NK_WIN32`, `NK_COCOA`, `NK_WAYLAND`, `NK_XCB`, `NK_XLIB`, `NK_ANDROID`,
  `NK_HARMONY_OS`. Brique de la couche **fenêtrage/UI** : un même OS Linux peut tourner sous Wayland
  ou X11.
- **`NkGraphicsAPI`** — `NK_VULKAN`, `NK_METAL`, `NK_OPENGL`, `NK_DIRECTX`, `NK_SOFTWARE`. Pilote la
  sélection du backend **rendu/GPU** : c'est le même choix que fait le RHI pour instancier son
  *device*.

### `NkVersionInfo` et `NkPlatformInfo`

`NkVersionInfo` est un petit agrégat `major`/`minor`/`patch` + une chaîne `versionString` (statique,
à ne pas libérer), utilisé entre autres pour la version de l'API graphique détectée.

`NkPlatformInfo` est la **fiche d'identité runtime** complète, peuplée une fois (initialisation
paresseuse thread-safe) puis lue partout, en lecture seule. Ses champs couvrent :

- **OS & compilateur** : `platform`, `architecture`, et les chaînes `osName`/`osVersion`/`archName`/
  `compilerName`/`compilerVersion` — pour les **logs**, les rapports de crash, la télémétrie.
- **CPU** : `cpuCoreCount`, `cpuThreadCount`, les tailles de caches L1/L2/L3, `cacheLineSize`. C'est
  ce qui dimensionne un **thread pool** et guide l'alignement des structures *cache-friendly*.
- **Mémoire** : `totalMemory`, `availableMemory`, `pageSize`, `allocationGranularity` — pour
  calibrer un **allocateur** ou un budget de streaming d'**assets**.
- **SIMD** : `hasSSE`…`hasSSE4_2`, `hasAVX`, `hasAVX2`, `hasAVX512`, `hasNEON`. Détermine le chemin
  vectorisé en **math/physique/audio** (mixage, FFT) au lancement.
- **Caractéristiques** : `isLittleEndian`, `is64Bit`, `endianness` — cruciaux pour la
  **sérialisation** binaire et le **réseau** (ordre des octets).
- **Build & capacités** : `isDebugBuild`, `isSharedLibrary`, `buildType`, et les capacités
  `hasThreading`/`hasVirtualMemory`/`hasFileSystem`/`hasNetwork` — pour adapter le comportement sur
  les plateformes contraintes (Web, consoles, embarqué).
- **Affichage/GFX** : `display`, `graphicsApi`, `graphicsApiVersion`.

### `NkSourceLocation`

Classe immuable et entièrement `constexpr noexcept`. Ses membres privés sont initialisés à des
valeurs sûres (`"unknown"`, `0`), si bien qu'une instance par défaut ne ment jamais. Quatre
accesseurs : `FileName()`, `FunctionName()`, `Line()` (1-based, `0` si inconnu), `Column()`
(`0` si non supporté par le compilateur). La *factory* `Current(file, function, line, column)` prend
pour défauts les builtins de localisation — ce qui permet de la capturer **chez l'appelant** quand on
la pose comme valeur par défaut d'argument.

- **IO / debug** : signaler dans quel fichier/fonction un flux a échoué, sans coder en dur le nom.
- **Mémoire** : étiqueter chaque allocation de sa source pour le [traceur de fuites](../NKMemory.md).
- **Assertions / gameplay-IA** : un *assert* qui pète au milieu d'une machine à états donne
  immédiatement le site fautif. Voir [Assertions](Assertions.md).
- **Threading** : tracer quel site a soumis une tâche à un pool, pour reconstruire une chaîne de
  causalité asynchrone.

Le détail des builtins (`NKENTSEU_BUILTIN_FILE`…) est purement mécanique : ils mappent vers les
builtins du compilateur (`__builtin_FILE()`…) quand ils existent, vers `__FILE__`/`__LINE__` sinon.
On les utilise rarement seuls ; ils servent de défauts à `Current()`.

### L'API C runtime de `NkPlatform.h`

Encadrée par `NKENTSEU_EXTERN_C_BEGIN/END` et exportée en `NKENTSEU_CORE_C_API`, cette couche expose
l'information de `NkPlatformInfo` au format C plat, avec initialisation paresseuse thread-safe.
`NkGetPlatformInfo()` rend le pointeur (statique, à ne pas libérer) ; `NkInitializePlatformInfo()`
force l'init (idempotent). Les accesseurs ciblés évitent de passer par l'agrégat :

- **Threading** : `NkGetCPUCoreCount()` / `NkGetCPUThreadCount()` dimensionnent un pool ;
  `NkGetCacheLineSize()` aligne les structures partagées pour éviter le *false sharing*.
- **Mémoire / GPU** : `NkGetTotalMemory()` / `NkGetAvailableMemory()` calibrent un budget de
  streaming ; `NkGetPageSize()` / `NkGetAllocationGranularity()` règlent un allocateur de pages.
- **Math / audio** : `NkHasSIMDFeature("AVX2")` choisit un noyau vectorisé (comparaison `strcmp`
  *case-sensitive* ; `false` si `nullptr` ou inconnu).
- **Sérialisation / réseau** : `NkGetEndianness()` et `NkIs64Bit()` décident du *byte-swapping*.
- **Diagnostic** : `NkGetPlatformName()` / `NkGetArchitectureName()` / `NkGetBuildType()` pour les
  bannières de log ; `NkPrintPlatformInfo()` dumpe tout d'un coup au démarrage.

Trois helpers d'alignement complètent l'ensemble : `NkIsAligned(addr, alignment)` (vrai si
`alignment == 0`), `NkAlignAddress(addr, alignment)` qui arrondit vers le haut
(`(addr + align - 1) & ~(align - 1)`), et sa variante const-correcte `NkAlignAddressConst`. Utiles
côté **GPU** (offsets de buffers alignés) et **mémoire** (placement dans une arène).

> **Attention homonymie.** `NkGetPlatformName()` existe en **deux** versions : la fonction C runtime
> ci-dessus (valeur découverte à l'exécution) et la fonction `inline` de `nkentseu` ci-dessous
> (constante de compilation `NKENTSEU_PLATFORM_NAME`). Choisir selon qu'on veut du runtime ou du
> compile-time.

### Les fonctions compile-time (`nkentseu`)

Inline, `noexcept`, zéro-overhead, elles enveloppent les macros `NKENTSEU_PLATFORM_*` :
`NkGetPlatformName()`, `NkGetPlatformVersion()`, et surtout les prédicats de catégorie —
`NkIsDesktop()`, `NkIsMobile()`, `NkIsConsole()`, `NkIsEmbedded()`, `NkIsWeb()`. L'idiome est
`if constexpr (nkentseu::NkIsDesktop())`, qui **supprime** la branche morte du binaire au lieu de la
brancher à l'exécution. Domaines : activer un chemin **UI** clavier/souris sur desktop vs tactile sur
mobile, désactiver le **threading** sur Web, charger des **assets** haute résolution selon la classe
de plateforme.

> Note : la documentation interne écrit parfois `platform::NkIsDesktop()`, mais ces fonctions vivent
> réellement dans `nkentseu` (pas `nkentseu::platform`).

### `nkentseu::arch` — caractéristiques CPU et alignement

Même esprit compile-time, côté CPU : `NkGetArchName()`/`NkGetArchVersion()`, les prédicats
`NkIs64Bit()`/`NkIs32Bit()` et `NkIsLittleEndian()`/`NkIsBigEndian()`, et les constantes matérielles
`NkGetCacheLineSize()`, `NkGetPageSize()`, `NkGetWordSize()` (4 ou 8). Les templates d'alignement —
`NkAlignUp`, `NkAlignDown`, `NkIsAligned`, `NkCalculatePadding` — déduisent automatiquement le type
`T*` et sont la base de tout **allocateur** et de la pose d'objets dans une **arène mémoire** ou un
buffer **GPU**.

> **Piège.** L'argument `align` de ces templates **doit être une puissance de 2**, et il n'y a
> **aucune garde** si `align == 0` (les formules masquent avec `align - 1`). Passer une valeur
> invalide donne un résultat faux silencieusement.

### `nkentseu::memory` — allocation alignée

Le couple de base est `NkAllocateAligned(size, alignment)` / `NkFreeAligned(ptr)` : la première
alloue (via `_aligned_malloc` / `posix_memalign` / fallback) un bloc dont `alignment` est ≥
`sizeof(void*)`, la seconde le libère (accepte `nullptr`). **Règle dure** : un pointeur obtenu par
`NkAllocateAligned` se libère **uniquement** par `NkFreeAligned` — jamais `free()`, sous peine de
corruption de tas. Les templates `NkAllocateAlignedArray<T>(count, alignment)` (mémoire **non**
initialisée), `NkConstructAligned<T>(ptr, args...)` (placement-new in-place) et `NkDestroyAligned<T>`
(appelle `~T()`, ne libère pas) formalisent le cycle **Create/Destroy** que tout le moteur respecte :
`NkAllocateAligned` → `NkConstructAligned` … `NkDestroyAligned` → `NkFreeAligned`. Domaines : pools
d'objets **ECS**, structures SIMD-alignées en **physique/math**, buffers **audio** alignés sur la
ligne de cache.

> **Piège namespace.** La macro de convenance `NkAllocPageAligned(size)` et certains commentaires
> Doxygen réfèrent `platform::memory::`, alors que le namespace réellement défini est
> `nkentseu::memory` (sans `platform`). Vérifier le chemin effectif avant d'écrire un appel qualifié
> complet.

### `NkConfig.h` — les drapeaux de build

Tout part du **mode de build**. `NKENTSEU_DEBUG` s'active si ni `NDEBUG` ni `NKENTSEU_RELEASE` ;
`NKENTSEU_RELEASE` si `NDEBUG` ; `NKENTSEU_DISTRIBUTION` (via `NKENTSEU_DIST`) implique aussi
`_RELEASE`. Une garde `#error` interdit DEBUG et RELEASE simultanés. De ce mode découlent en cascade :

- **Assertions** : `NKENTSEU_ENABLE_ASSERTS` (auto en debug), forçable par
  `NKENTSEU_FORCE_ENABLE_ASSERTS` / `NKENTSEU_FORCE_DISABLE_ASSERTS`.
- **Mémoire** : `NKENTSEU_ENABLE_MEMORY_TRACKING` (auto en debug, sauf
  `NKENTSEU_DISABLE_MEMORY_TRACKING`) entraîne `_ENABLE_LEAK_DETECTION` et `_ENABLE_MEMORY_STATS`.
- **Logging** : `NKENTSEU_LOG_LEVEL` vaut `4` (DEBUG) en debug, `2` (WARN) sinon, sur l'échelle
  0=OFF…5=TRACE ; `NKENTSEU_ENABLE_FILE_LOGGING` auto en debug.
- **Profiling** : `NKENTSEU_ENABLE_PROFILING` si debug ou `NKENTSEU_PROFILE`.
- **SIMD/threads** : `NKENTSEU_ENABLE_SIMD` si non désactivé **et** support détecté
  (`_CPU_HAS_SSE2` ou `_CPU_HAS_NEON`) — sinon `#warning` + undef ; `NKENTSEU_ENABLE_THREADING`
  sauf désactivation ; `NKENTSEU_DEFAULT_THREAD_COUNT` à `0` (auto).
- **Allocateur** : `NKENTSEU_DEFAULT_ALIGNMENT` = `16` si SIMD sinon `8` ;
  `NKENTSEU_DEFAULT_PAGE_SIZE` = `4*1024` ; `NKENTSEU_DEFAULT_ALLOCATOR` = `_MALLOC`.
- **Strings/conteneurs** : capacités et facteurs par défaut (`_STRING_DEFAULT_CAPACITY` = 64, SSO 23,
  `_VECTOR_GROWTH_FACTOR` = 1.5f, `_HASHMAP_MAX_LOAD_FACTOR` = 0.75f…).
- **Math** : précision float/double, `NKENTSEU_MATH_EPSILON` = `1e-6f`, `NKENTSEU_PI`.
- **Plateforme/GFX** : `NKENTSEU_USE_WIN32_API` / `_USE_POSIX_API`, et
  `NKENTSEU_GRAPHICS_BACKEND_DEFAULT` (Windows → D3D11, Apple → Metal, Android → GLES3, autres →
  OpenGL).

Enfin les utilitaires conditionnels — `NKENTSEU_IS_DEBUG`/`_IS_RELEASE` (`1`/`0`) et
`NKENTSEU_DEBUG_ONLY(code)` / `NKENTSEU_RELEASE_ONLY(code)` — laissent écrire des branches qui
disparaissent en release.

### `NkMacros.h` — la boîte à outils du préprocesseur

La plupart sont triviales et se passent de commentaire : **stringification** (`NKENTSEU_STRINGIFY`),
**concaténation** (`NKENTSEU_CONCAT`/`_CONCAT3`/`_CONCAT4`), **taille de tableau**
(`NKENTSEU_ARRAY_SIZE`), **bits** (`NKENTSEU_BIT(x)` = `1U << x`, plus `_BIT_TEST/SET/CLEAR/TOGGLE` —
la base d'un masque de **layers** de collision ou de flags de **rendu**), **tailles mémoire** lisibles
(`NKENTSEU_MEGABYTES(64)` pour un budget de pool), et `NKENTSEU_UNUSED(x)` pour faire taire un
paramètre intentionnellement ignoré (handlers d'**événements**, overrides). Quelques-unes méritent
qu'on s'y arrête :

- **`NK_MIN` / `NK_MAX` / `NK_CLAMP` / `NK_ABS` / `NK_SWAP`** (préfixe court `NK_`) — pratiques mais
  **évaluent leurs arguments plusieurs fois** : `NK_MAX(i++, n)` est un bug. À réserver aux
  expressions sans effet de bord.
- **`NKENTSEU_OFFSETOF` / `NKENTSEU_CONTAINER_OF`** — décalage d'un champ et reconstruction de
  l'objet conteneur depuis un pointeur de membre : le cœur des **listes intrusives** (un nœud
  embarqué dans une entité **ECS**).
- **`NKENTSEU_LIKELY` / `NKENTSEU_UNLIKELY` / `NKENTSEU_UNREACHABLE`** — hints au compilateur sur les
  branches chaudes/froides ; précieux dans les boucles serrées de **rendu** ou de **physique**.
  No-op hors GCC/Clang.
- **`NKENTSEU_DEFER(code)`** — scope guard qui exécute `code` à la sortie du bloc (libérer un handle
  **GPU**, fermer un fichier **IO**). **GCC/Clang seulement** — vide ailleurs, ne pas en dépendre
  pour la correction.
- **`NKENTSEU_TODO(msg)` / `NKENTSEU_FIXME(msg)`** — émettent un message à la compilation (« TODO: …»).
  Pour marquer un argument inutilisé c'est bien `NKENTSEU_UNUSED`, pas une macro `TODO`.
- **`NKENTSEU_WILL_ADD_OVERFLOW` / `NKENTSEU_WILL_MUL_OVERFLOW`** — garde-fous avant un calcul de
  taille d'allocation (**mémoire**) ou d'index de buffer (**GPU**).
- **`NKENTSEU_STATIC_ASSERT` / `NKENTSEU_SAME_TYPE` / `NKENTSEU_SIZEOF_MEMBER`** — vérifications de
  contrat à la compilation (taille d'un vertex **GPU**, compatibilité de types). `NKENTSEU_SAME_TYPE`
  est `0` hors GCC/Clang.
- **Version** : `NKENTSEU_VERSION_ENCODE(M, m, p)` = `(M << 24) | (m << 16) | p`, avec les extracteurs
  `_VERSION_MAJOR/MINOR/PATCH`. Encodage réel **8 bits major / 8 minor / 16 patch**.

> **Incohérence à connaître.** L'encodage de `NKENTSEU_VERSION_ENCODE` (8/8/16) **diffère** du
> commentaire de `using NkVersion` dans `NkPlatform.h` (qui annonce 16/8/8). L'encodage qui fait foi
> est celui de `NkMacros.h`.

### `NkVersion.h` — version du framework

Header-only. La version du noyau est `NKENTSEU_VERSION_CORE_MAJOR/MINOR/PATCH` (1.0.0 par défaut,
surchargeable par `-D`), encodée dans `NKENTSEU_VERSION_CORE` (format `0xMMmmpppp`) et lisible en
clair via `NKENTSEU_VERSION_CORE_STRING`. L'identité du module est exposée par
`NKENTSEU_FRAMEWORK_CORE_NAME` (`"NKCore"`) et `_FULL_NAME` (`"NKCore vX.Y.Z"`). Les infos de build —
`NKENTSEU_BUILD_DATE`/`_TIME`/`_TIMESTAMP` (depuis `__DATE__`/`__TIME__`) et `_BUILD_NUMBER` (0,
surchargeable en CI) — alimentent les **bannières de log** et les rapports de crash. Côté API
publique, `NKENTSEU_API_VERSION` et les marqueurs `NKENTSEU_API_STABLE` / `_API_EXPERIMENTAL`
(génère un warning de dépréciation) / `_API_INTERNAL` (visibilité cachée) documentent la stabilité.
Enfin, `NKENTSEU_VERSION_AT_LEAST(M, m, p)` et `NKENTSEU_VERSION_EQUALS(M, m, p)` permettent de
garder du code derrière une exigence de version — utile quand un module **runtime** dépend d'une
fonctionnalité ajoutée à une révision précise.

### `NkCoreApi.h` / `NkExport.h` — export de symboles

`NkExport.h` n'est qu'un alias (`#include "NkCoreApi.h"`) ; toute la logique est dans `NkCoreApi.h`,
qui **réutilise** les macros de NKPlatform (zéro duplication). Le build système définit l'un de
`NKENTSEU_CORE_BUILD_SHARED_LIB` / `_STATIC_LIB` / `_HEADER_ONLY` (une garde `#warning` rejette les
combinaisons incohérentes). La macro pivot est **`NKENTSEU_CORE_API`** : elle vaut l'attribut
d'export en build shared, rien en static/header-only, l'attribut d'import par défaut sinon (cas DLL
consommée). On la pose devant les classes (`NKENTSEU_CORE_CLASS_EXPORT`), on la combine avec
l'inlining (`NKENTSEU_CORE_API_INLINE`, `_FORCE_INLINE`, `_NO_INLINE`), et `NKENTSEU_CORE_C_API`
ajoute `extern "C"`. Tout symbole **runtime** destiné à franchir la frontière d'une DLL (l'API C de
`NkPlatform.h`, par exemple) en est décoré. Les attributs partagés — `NKENTSEU_DEPRECATED`,
`NKENTSEU_DEPRECATED_MESSAGE(msg)`, `NKENTSEU_ALIGN_CACHE/16/32/64`, `NKENTSEU_EXTERN_C_BEGIN/END`,
`NKENTSEU_API_LOCAL` — sont réutilisés **directement** depuis NKPlatform.

> **Piège.** Il n'existe pas de variante `NKENTSEU_CORE_DEPRECATED_MESSAGE` ni
> `NKENTSEU_CORE_EXTERN_C_BEGIN` : utiliser les versions NKPlatform (`NKENTSEU_DEPRECATED_MESSAGE`,
> `NKENTSEU_EXTERN_C_BEGIN`).

---

### Exemple récapitulatif

```cpp
#include "NKCore/NkPlatform.h"

using namespace nkentseu;

// 1. Compile-time : un chemin éliminé du binaire hors desktop.
if constexpr (NkIsDesktop()) {
    // chemin clavier/souris
}

// 2. Runtime : dimensionner un thread pool selon le CPU réel.
const platform::NkPlatformInfo* info = NkGetPlatformInfo();
nk_uint32 workers = info->cpuThreadCount;
nk_bool   simd    = NkHasSIMDFeature("AVX2");

// 3. Source location : capter le site appelant comme défaut d'argument.
void Trace(const nk_char* msg,
           platform::NkSourceLocation loc = NkCurrentSourceLocation);
// Trace("oops");  // loc.FileName()/Line()/FunctionName() = ceux de l'appel

// 4. Mémoire alignée : cycle Create/Destroy complet.
void* raw = memory::NkAllocateAligned(sizeof(NkVertex) * 1024, NKENTSEU_DEFAULT_ALIGNMENT);
NkVertex* v = memory::NkConstructAligned<NkVertex>(raw /*, args... */);
memory::NkDestroyAligned(v);
memory::NkFreeAligned(raw);

// 5. Macros de service.
nk_uint32 flags = NKENTSEU_BIT(2) | NKENTSEU_BIT(5);   // masque de layers
NKENTSEU_TODO("brancher le backend Vulkan ici");
```

---

[← L'énumération](Enumeration.md) · [Index NKCore](README.md) · [Récap NKCore](../NKCore.md)
