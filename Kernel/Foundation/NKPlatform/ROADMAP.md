# NKPlatform — Roadmap

État actuel (mai 2026) : couche d'abstraction plateforme compile-time
livrée et stable. Toutes les détections (OS, architecture, compilateur,
endianness, GPU APIs, CPU features) fonctionnent. Logging foundation
minimaliste opérationnel et utilisé par tous les modules au-dessus. Reste
à compléter `NkCPUFeatures` runtime, l'abstraction `NkEnv` et l'export pour
plateformes mobiles / consoles peu testées.

---

## 📊 Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| Détection OS (`NkPlatformDetect.h`) | ✅ Livré | — | — |
| Détection architecture (`NkArchDetect.h`) | ✅ Livré | — | — |
| Détection compilateur (`NkCompilerDetect.h`) | ✅ Livré | — | — |
| Endianness (`NkEndianness.h/.cpp`) | ✅ Livré | — | — |
| Détection GPU / APIs graphiques (`NkCGXDetect.h/.cpp`) | ✅ Livré | — | — |
| Export / inline (`NkPlatformExport.h`, `NkPlatformInline.h`) | ✅ Livré | — | — |
| Configuration runtime (`NkPlatformConfig.h/.cpp`) | ✅ Livré | — | — |
| Foundation log (`NkFoundationLog.h`) | ✅ Livré | — | — |
| Variables d'environnement (`NkEnv.h/.cpp`) | ✅ Livré | — | — |
| Détection CPU runtime (`NkCPUFeatures.h/.cpp`) | 🔶 Partiel | M | Haute |
| Tests unitaires | ❌ TODO (aucun dossier `tests/`) | M | Haute |
| Plateformes consoles validées (PS5, Xbox, Switch) | ❌ TODO | XL | Basse |
| Plateformes mobiles validées (Android, iOS) | 🔶 Partiel (détection seulement) | L | Moyenne |
| Web / Emscripten / WASM | 🔶 Partiel (détection seulement) | M | Moyenne |

Légende : ✅ Livré · 🔶 Partiel · ⏳ En cours · ❌ TODO · 🚫 Abandonné

---

## ✅ Livré

### Détection compile-time
- [NkPlatformDetect.h](src/NKPlatform/NkPlatformDetect.h) : macros
  `NKENTSEU_PLATFORM_*` (WINDOWS, LINUX, MACOS, IOS, ANDROID, EMSCRIPTEN,
  consoles, embarqué) + catégories (`DESKTOP`, `MOBILE`, `CONSOLE`,
  `EMBEDDED`) + macros conditionnelles `NKENTSEU_<PLATFORM>_ONLY({...})` et
  `NKENTSEU_NOT_<PLATFORM>({...})` + constantes lisibles (`NKENTSEU_PLATFORM_NAME`,
  `NKENTSEU_PLATFORM_VERSION`)
- [NkArchDetect.h](src/NKPlatform/NkArchDetect.h) : `NKENTSEU_ARCH_X86_64`,
  `ARM64`, `X86`, `ARM`, `RISCV64`, bitness (`NKENTSEU_ARCH_64BIT/32BIT`),
  endianness compile-time (`NKENTSEU_ARCH_LITTLE_ENDIAN/BIG_ENDIAN`), macros
  d'alignement, détection compile-time des extensions SIMD
  (`NKENTSEU_CPU_HAS_SSE2/AVX2/NEON`)
- [NkCompilerDetect.h/.cpp](src/NKPlatform/NkCompilerDetect.h) :
  `NKENTSEU_COMPILER_MSVC/GCC/CLANG`, version, standards C++ supportés,
  features (nullptr, constexpr, source_location, etc.)
- [NkEndianness.h/.cpp](src/NKPlatform/NkEndianness.h) : détection
  compile-time + runtime, helpers `HostToNetwork{16,32,64}`,
  `NetworkToHost{16,32,64}`, templates `ByteSwap<T>` génériques, `ReadBE*` /
  `WriteBE*` pour buffers non-alignés
- [NkCGXDetect.h/.cpp](src/NKPlatform/NkCGXDetect.h) : énumérations
  `NkGraphicsApi`, `NkGPUVendor`, `NkGPUType`, détection des APIs par
  plateforme (Vulkan / OpenGL / DX11 / DX12 / Metal / WebGL / WebGPU), APIs
  de calcul (CUDA / OpenCL / SYCL), systèmes d'affichage Linux (X11 / XCB /
  Wayland), Vendor IDs PCI (NVIDIA / AMD / Intel / Apple / Qualcomm /
  Imagination / ARM)

### Export, inline, optimisation
- [NkPlatformExport.h](src/NKPlatform/NkPlatformExport.h) :
  `NKENTSEU_PLATFORM_API` (`__declspec(dllexport/import)` Windows /
  `__attribute__((visibility("default")))` GCC/Clang), 3 modes mutuellement
  exclusifs `BUILD_SHARED_LIB`, `STATIC_LIB`, `HEADER_ONLY` (avec `#error`
  si plusieurs définis)
- [NkPlatformInline.h](src/NKPlatform/NkPlatformInline.h) :
  `NKENTSEU_INLINE`, `NKENTSEU_FORCE_INLINE`, `NKENTSEU_NO_INLINE`,
  `NKENTSEU_LIKELY/UNLIKELY` (hints branche), `NKENTSEU_API_INLINE`,
  `NKENTSEU_API_FORCE_INLINE` pour fonctions exportées inline

### Runtime
- [NkPlatformConfig.h/.cpp](src/NKPlatform/NkPlatformConfig.h) : singleton
  thread-safe `PlatformConfig` (compile-time : nom, arch, bitness, version
  compilateur) et `PlatformCapabilities` (runtime : RAM totale / disponible,
  cpu cores physiques / logiques, flags SIMD). `GetPlatformConfig()` et
  `GetPlatformCapabilities()` accesseurs globaux.
- [NkEnv.h/.cpp](src/NKPlatform/NkEnv.h) : variables d'environnement
  portables `NkGet/NkSet/NkUnset/NkExists`, manipulation PATH
  (`NkPrependToPath`, `NkAppendToPath`), conteneurs internes
  `NkEnvString` / `NkEnvVector<T>` / `NkEnvUMap<K, V>` (sans STL, recherche
  linéaire pour map)

### Logging
- [NkFoundationLog.h](src/NKPlatform/NkFoundationLog.h) : ultra-léger,
  niveaux configurables (`NK_FOUNDATION_LOG_LEVEL_ERROR/WARN/INFO/DEBUG/TRACE`)
  filtrés compile-time, backend cross-platform (`fprintf(stderr)` desktop +
  Android `logcat` via `<android/log.h>`), formatage de valeurs via ADL
  (`NKFoundationToString`), sink utilisateur via `NkFoundationSetLogSink()`,
  macros `NK_FOUNDATION_LOG_*` et `NK_FOUNDATION_LOG_*_VALUE`

### Header umbrella
- [NKPlatform.h](src/NKPlatform/NKPlatform.h) : umbrella header qui inclut
  tous les sous-headers publics dans l'ordre des dépendances

### Documentation
- `Readme.md` complet (~1200 lignes) : architecture, build, API reference,
  exemples, CMake integration, tests, FAQ, troubleshooting, bonnes pratiques

---

## 🔄 En cours / TODO immédiat

### `NkCPUFeatures.h/.cpp` runtime
- Le squelette est en place avec API singleton `nkentseu::platform::*` :
  détection SIMD runtime (SSE / AVX / AVX2 / AVX-512 / NEON / SVE), topologie
  CPU (cœurs physiques / logiques / HT), caches L1/L2/L3 (taille + ligne),
  features étendues (AES, SHA, RDRAND, virtualisation)
- L'implémentation CPUID x86 / sysctl macOS / `/proc/cpuinfo` Linux est à
  auditer (le `.cpp` existe mais le contenu n'a pas été vérifié dans cet
  audit). Effort : M.
- Coordination avec NkMath/NkSIMD : utilise déjà les macros compile-time
  `NKENTSEU_CPU_HAS_*`, mais le dispatch runtime (cf. exemple
  `InitializeComputeDispatcher` dans `NKPlatform.h`) dépend de cette API
  runtime.

### Tests unitaires
- **Aucun dossier `tests/` dans NKPlatform** (contrairement à NKCore, NKMath,
  NKMemory, NKContainers qui ont tous des smoke tests). À ajouter :
  - Tests endianness (round-trip ByteSwap, HostToNetwork, NetworkToHost)
  - Tests `NkEnv` (Set/Get/Unset, PrependToPath round-trip)
  - Tests `NkFoundationLog` (niveau filtering, sink redirect)
  - Tests `PlatformConfig` / `PlatformCapabilities` (snapshot non-vide)
  - Smoke tests des macros de détection (au moins valider qu'UNE macro
    plateforme est définie par configuration)
- Effort estimé : M.

### Logging async
- `NkFoundationLog` est synchrone (write direct vers stderr / logcat).
  Acceptable pour debug mais peut bloquer le thread principal en gros
  volume. Une variante double-buffered async serait utile, mais c'est
  probablement le job du futur `NKLogger` (couche au-dessus).

---

## ❌ À venir / À ajouter (futur proche)

### Plateformes pas / peu validées
ARCHITECTURE.md annonce le support Windows / Linux / macOS / Android / iOS /
Web / Xbox. État réel :

- ✅ **Windows** : validé (NkRenderer testé Vulkan + OpenGL + DX en cours)
- ✅ **Linux** : détection complète, NkRenderer testé Vulkan + OpenGL
- 🔶 **macOS** : détection + Metal API détecté, mais binding Metal renderer
  pas validé runtime
- 🔶 **iOS** : détection présente, runtime jamais testé. Backend Metal
  shared avec macOS.
- 🔶 **Android** : détection + foundation log via logcat, mais NkRenderer
  Vulkan Android pas validé. Build NDK non testé dans CI.
- 🔶 **Emscripten / WebAssembly** : détection présente (`NKENTSEU_PLATFORM_EMSCRIPTEN`),
  backend WebGL/WebGPU à wirer dans NKRHI
- ❌ **Consoles** : détection PS4/PS5/Xbox/Switch présente en pré-processeur
  mais SDK propriétaires non accessibles → pas de validation possible sans
  partenariat éditeur. Effort XL bloqué.

### `NkPath` portable
- Manipulation de chemins fichier (séparateurs Windows `\` vs POSIX `/`,
  join, normalize, extension, parent). Référencé dans les exemples
  `NKPlatform.h` mais pas implémenté. Effort : S-M. Sa place exacte
  (NKPlatform vs futur NKStream) à trancher.

### `NkFile` / `NkFileSystem`
- Ouverture / lecture / écriture portable. Référencé dans les exemples
  Readme. Devrait probablement vivre dans le futur `NKStream` plutôt que
  dans NKPlatform.

### `NkThread` + `NkMutex` + `NkConditionVariable`
- Référencés dans les exemples Readme `WorkerPool` mais non implémentés
  côté NKPlatform. Doivent vivre dans le futur `NKThreading` (module
  manquant côté NKCore).

### `NkNetworkInit` + APIs réseau
- Mentionné dans les exemples (`nkentseu::platform::NkNetworkInit()`) sans
  implémentation. Sortie de scope NKPlatform à mon avis — devrait être un
  module à part `NKNetwork` quand pertinent.

### Build et CI multi-plateforme
- `Readme.md` propose un workflow GitHub Actions avec matrice 3 OS × 3
  compilateurs × 2 build types × 3 modes = 54 configurations. À mettre en
  place réellement. Effort : L.

### Détection capacités CPU étendues
- `NkCPUFeatures` devrait inclure : SVE / SVE2 (ARM), AMX (Intel),
  AVX-512 sous-versions (F / VL / DQ / BW / VNNI / etc.), cache associativity,
  cache topology par cœur. Effort : M.

---

## Bugs / quirks connus

- Le dossier `tests/` est **absent** côté NKPlatform — décalage avec les
  4 autres modules Foundation. Inhabituel pour un module au cœur du framework.
- `Readme.md` mentionne `impl/NkCPUFeatures.h` et `impl/NkPlatformRuntime.c`
  comme "Implémentations Internes" mais ces fichiers ne sont pas dans
  `src/NKPlatform/` — soit ils n'existent pas, soit ils ne sont pas exposés.
  À clarifier.
- L'umbrella header `NKPlatform.h` n'inclut **pas** `NkCPUFeatures.h`, `NkEnv.h`,
  `NkPlatformConfig.h` — l'utilisateur doit les inclure individuellement.
  Inversement à ce qu'annonce la documentation.
- `NkPlatform.h` mentionne `NkCurrentSourceLocation` dans les exemples
  sans préciser où elle est définie — probablement ailleurs (NKCore ?).
- Quelques exemples de `Readme.md` utilisent encore `std::function`,
  `std::vector`, `std::queue` ce qui contredit la philosophie zero-STL.
  Les `NkEnv*` conteneurs internes le respectent en revanche.

---

## Dépendances

- **Couches en dessous (utilisées)** : système d'exploitation natif
  (Win32 / POSIX / Mach / NDK), aucune autre dépendance NK (NKPlatform est
  la base de la pyramide)
- **Modules au-dessus qui en dépendent** : **tous** — NKCore (types + asserts),
  NKMemory (export + alignment), NKMath (SIMD detection), NKContainers
  (export), NKRHI (graphics API detection), NKRenderer, services moteur,
  application framework Nkentseu/Core, Unkeny éditeur, PV3DE. Le `Readme.md`
  documente explicitement la position au sommet de la pyramide
  "Bibliothèques de base".
