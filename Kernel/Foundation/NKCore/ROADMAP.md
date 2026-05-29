# NKCore — Roadmap

État actuel (mai 2026) : socle de types et utilitaires bas niveau livré et
stable. Suffisant pour faire compiler NKMath, NKMemory, NKContainers, NKRHI et
NKRenderer. Manque encore les briques transverses du tableau ARCHITECTURE
"2.2 Bibliothèques de base" (NKTime, NKStream, NKThreading, NKLogger) qui
restent à créer ailleurs.

---

## 📊 Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| Types fondamentaux (`NkTypes.h`) | ✅ Livré | — | — |
| Macros / export / config (`NkMacros.h`, `NkConfig.h`, `NkCoreExport.h`) | ✅ Livré | — | — |
| Traits / limites / type utils (`NkTraits.h`, `NkLimits.h`, `NkTypeUtils.h`) | ✅ Livré | — | — |
| Assertions (`Assert/*`) | ✅ Livré | — | — |
| Bits / Atomic / Variant / Optional / Invoke | ✅ Livré | — | — |
| Détection runtime plateforme (`NkPlatform.h/.cpp`) | ✅ Livré | — | — |
| Énumération (`NkEnumeration.h`) | 🔶 Partiel | S | Basse |
| Sortie module dans ARCHITECTURE : NKTime / NKStream / NKThreading / NKLogger | ❌ TODO | XL | Haute |
| Coverage tests | 🔶 Partiel | M | Moyenne |
| Documentation publique consolidée | ❌ TODO | S | Basse |

Légende : ✅ Livré · 🔶 Partiel · ⏳ En cours · ❌ TODO · 🚫 Abandonné

---

## ✅ Livré

### Types fondamentaux
- Types entiers fixes signés / non signés 8 → 64 bits + support optionnel 128
  bits (`__int128` natif, fallback struct sinon) dans
  [NkTypes.h](src/NKCore/NkTypes.h)
- Flottants `float32` / `float64` / `float80` + alias `nk_*` officiels
- Types caractères avec support C++20 `char8_t`/`char16_t`/`char32_t` +
  fallback ; `nk_wchar` portable
- Booléens : `nk_bool`, `nk_boolean` (8 bits), `nk_bool32` (32 bits pour
  alignement)
- Pointeurs : `nk_ptr`, `nk_byteptr`, `nk_intptr`, `nk_uintptr` + helpers
  `SafeConstCast`, `NkSafeCast`
- Wrapper `Byte` constexpr type-safe (opérateurs bitwise, conversions `from`)
- Constantes `NKENTSEU_*_MIN/MAX`, `NK_NULL`, `NKENTSEU_INVALID_SIZE/INDEX/ID*`
- Macros endianness `NkToBigEndian{16,32,64}` / `NkToLittleEndian{16,32,64}`
- Sous-namespaces `nkentseu::core` (`NkHashValue`, `NkHandle`, `NkID32/64`,
  `INVALID_HANDLE`) et `nkentseu::math` (`NkReal`, `NkRadians`, `NkDegrees`
  avec switch `NKENTSEU_MATH_USE_DOUBLE`)

### Métaprogrammation et limites
- [NkTraits.h](src/NKCore/NkTraits.h) : `NkRemoveReference`, `NkEnableIf`,
  `NkIsSame`, `NkMove`, `NkForward`, etc. (réimplémentation sans `<type_traits>`
  dans le hot path)
- [NkLimits.h](src/NKCore/NkLimits.h) : `NkNumericLimits<T>` (min / max /
  epsilon) sans `<limits>`
- [NkTypeUtils.h](src/NKCore/NkTypeUtils.h) : `NkClamp`, `NkBit`,
  `NkArraySize`, littéraux

### Macros / export / configuration
- [NkMacros.h](src/NKCore/NkMacros.h) : `NKENTSEU_STRINGIFY`, `NKENTSEU_CONCAT`,
  `NKENTSEU_DEPRECATED`, `NKENTSEU_DEBUG_ONLY`, `NKENTSEU_PROFILE_SCOPE`, etc.
- [NkConfig.h](src/NKCore/NkConfig.h) : flags globaux (`NKENTSEU_DEBUG`,
  `NKENTSEU_ENABLE_ASSERTS`, `NKENTSEU_MATH_USE_DOUBLE`, `NK_STRING_SSO_SIZE`,
  etc.)
- [NkCoreExport.h](src/NKCore/NkCoreExport.h) +
  [NkExport.h](src/NKCore/NkExport.h) : `NKENTSEU_CORE_API` qui réutilise
  `NKENTSEU_PLATFORM_API_EXPORT` (zéro duplication, conformément à la philosophie
  documentée dans NKPlatform/Readme.md)
- [NkVersion.h](src/NKCore/NkVersion.h) : `NKENTSEU_VERSION_*`

### Assertions et debug
- `Assert/NkAssertion.h` : structure `NkAssertionInfo` (file/line/function/msg)
- `Assert/NkAssertHandler.h` : handler centralisé configurable via callback
- `Assert/NkAssert.h` : `NKENTSEU_ASSERT`, `NKENTSEU_ASSERT_MSG`,
  `NKENTSEU_STATIC_ASSERT`, `NKENTSEU_CHECK_RETURN`
- `Assert/NkDebugBreak.h` : `NKENTSEU_DEBUGBREAK()` via intrinsics par
  compilateur

### Briques utilitaires
- [NkBits.h/.cpp](src/NKCore/NkBits.h) : `CountBits`, `CountTrailingZeros`,
  `NextPowerOfTwo`, `RotateLeft/Right`, `ExtractBits`, `ByteSwap{16,32,64}`
  (couvert par `tests/test_smoke.cpp`)
- [NkAtomic.h](src/NKCore/NkAtomic.h) : `NkAtomic<T>` (Relaxed / Acquire /
  Release / SeqCst), typedefs `NkAtomicInt32/Bool/Ptr`, `NkAtomicFlag`,
  `NkSpinLock` (backoff exponentiel) + `NkScopedSpinLock`, fences explicites,
  fonctions globales `NkAtomicIncrement/Add/CompareExchange` (test couvre
  `FetchAdd`/`CompareExchangeStrong`)
- [NkOptional.h](src/NKCore/NkOptional.h) : `NkOptional<T>` complet
  (`HasValue`, `Value`, `ValueOr`, `Emplace`, `Reset`, `Swap`, `GetIf`) sans
  `<optional>` (test smoke OK)
- [NkVariant.h](src/NKCore/NkVariant.h) : `NkVariant<Ts...>` header-only avec
  `Get<T>`, `GetChecked<T>`, `GetIf<T>`, `HoldsAlternative<T>`, `Visit()`,
  `Emplace<T>`, `Reset()`, `Swap()` (test smoke OK)
- [NkInvoke.h](src/NKCore/NkInvoke.h) : `NkInvoke()` portable pour callables
- [NkBuiltin.h](src/NKCore/NkBuiltin.h) : intrinsics builtins compilateur

### Détection plateforme runtime
- [NkPlatform.h/.cpp](src/NKCore/NkPlatform.h) : ré-exporte `NKPlatform.h` et
  ajoute des helpers runtime côté Core

### Tests
- `tests/test_smoke.cpp` : Optional lifecycle, Optional swap/getIf, Variant
  set/visit, Variant swap/getIf, Atomic counter / CompareExchange, Bits
  utilities, Byte wrapper
- `tests/benchmark_smoke.cpp` : harness benchmark de base

---

## 🔄 En cours / TODO immédiat

### Énumération (`NkEnumeration.h`)
- Le header existe mais l'API publique n'est pas couverte par les smoke tests.
  À auditer : `NkEnumIterate`, conversions enum ↔ string, support flag bitset.
  Effort estimé : S.

### Renforcer la couverture des tests Core
- Ajouter tests pour : `NkTraits` (specialization tests), `NkLimits`
  (round-trip min/max), `NkAtomic` modes `Acquire/Release`, `NkSpinLock` sous
  contention, `NkInvoke` méthode membre, `NkVariant` constructeurs explicites
  avec types non-triviaux (RAII probe)
- Effort estimé : M.

### Macro `NKENTSEU_PROFILE_SCOPE`
- Documentée dans `NKCore.h` mais l'implémentation dépend d'un système de
  profiling qui n'existe pas encore côté Core. Soit la stub (no-op release),
  soit la wirer une fois NKMemory `NkProfiler` exposé au-dessus.

---

## ❌ À venir / À ajouter (futur proche)

### Modules `2.2 Bibliothèques de base` non encore créés
ARCHITECTURE.md liste 8 modules, seuls 4 existent (`NKCore`, `NKMath`,
`NKMemory`, `NKContainers`). Les 4 autres dépendent fortement de NKCore et
devront être créés :

- **NKTime** — `NkClock`, `NkChrono`, delta time, sleep portable. Probable
  dépendance directe vers NKPlatform pour `QueryPerformanceCounter` /
  `clock_gettime`. Effort : M-L. Priorité : Haute (NkRenderer simule
  actuellement le temps via une variable locale).
- **NKStream** — Lecture / écriture binaire et texte (réseau, fichiers,
  buffers en mémoire). Dépendances : NKCore + NKMemory + endianness. Effort :
  L. Priorité : Haute (NkRenderer charge déjà des .hdr / .nkasset, NKAudio /
  NKImage en auront besoin pour la sérialisation).
- **NKThreading** — Threads, mutex (au-delà du SpinLock déjà dans NKCore), job
  system, condition variables, futures. Dépendances : NkAtomic existant +
  NKPlatform. Effort : XL. Priorité : Haute (NkRenderer NkVSM v2 a noté que
  "Dynamic offsets UBO" + multi-threading sont nécessaires pour scale à 10k+
  draws).
- **NKLogger** — Logger typé avec niveaux et sinks. NkFoundationLog existe
  côté NKPlatform mais c'est un logger ultra-léger : un vrai logger (rotation
  fichier, sinks multiples, format `{}`) reste à concevoir. Effort : L.
  Priorité : Moyenne.

### Améliorations potentielles dans NKCore lui-même
- `NkResult<T, E>` (Rust-style) en complément de `NkOptional` — actuellement
  livré dans NKContainers (`Utilities/NkResult.h`) mais sémantiquement aurait
  sa place dans NKCore puisque les containers réfèrent un type Core.
- `NkSpan<T>` (vue non-owning) — actuellement seulement dans
  `NKContainers/Views/NkSpan.h`. Idem, candidat à promouvoir en NKCore pour
  briser la dépendance NKContainers → NKCore qui empêche NKCore de l'utiliser.
- `NkRange<Begin, End>` léger sans dépendance sur NKMath (NkMath fournit
  `NkRange` géométrique numérique, sémantique différente). Pour ranges
  d'itérateurs uniquement.
- `NkSourceLocation` portable (compat `<source_location>` C++20) — aujourd'hui
  utilise `__FILE__/__LINE__/__func__` via macros.

### Documentation
- Pas de `CHANGELOG.md` ni de doc d'architecture interne (seul le Doxygen dans
  les headers existe). Voir au moins un fichier d'overview public quand le
  module se stabilisera.

---

## Bugs / quirks connus

- `NkConfig.h` documenté côté NKCore.h mais certaines macros (ex.
  `NKENTSEU_DEBUG_ONLY`, `NKENTSEU_PI_DOUBLE`) sont référencées dans les
  exemples sans vérification de présence dans le fichier — à auditer (cf.
  exemples dans le bas de `NKCore.h`).
- Double définition `Byte::Value` enum + struct `Byte` peut prêter à confusion
  (chiffres hex `_0`..`_f` comme membres d'enum).
- L'inclusion de `<cfloat>` dans `NkTypes.h` viole partiellement la philosophie
  "zero-STL". Ce n'est pas un container mais une dépendance C standard
  acceptée ; à documenter explicitement comme exception.

---

## Dépendances

- **Couches en dessous (utilisées)** : NKPlatform (détection compile-time,
  export, inline, foundation log)
- **Modules au-dessus qui en dépendent** : NKMath, NKMemory, NKContainers,
  NKRHI, NKRenderer, tous les services moteur (NKFont, NKImage, NKAudio,
  NKPhysics, NKAnimation, NKScript, NKScene, NKUI), Nkentseu/Core application
  framework, Unkeny éditeur, PV3DE
