# La détection de plateforme

> Couche **Foundation** · NKPlatform · Savoir **où** et **avec quoi** on compile : le système
> d'exploitation (`NKENTSEU_PLATFORM_*`), l'architecture du CPU (`NKENTSEU_ARCH_*`) et le
> compilateur (`NKENTSEU_COMPILER_*`) — le tout résolu **à la compilation**, sans une seule
> instruction exécutée à l'exécution.

Un moteur zero-STL qui vise Windows, Linux, macOS, iOS, Android, le Web et les consoles doit, à
un moment, écrire du code qui **diffère selon la cible** : ici un `HWND` Win32, là un `XComponent`
HarmonyOS ; ici un `_mm256_load_ps` AVX2, là un `vld1q_f32` NEON ; ici un export `__declspec(dllexport)`,
là un attribut GCC. La question n'est pas *« mon code marche-t-il partout »* — il ne peut pas, les
API natives diffèrent — mais *« comment je laisse chaque plateforme prendre **son** chemin sans
polluer les autres ».* NKPlatform répond en posant, dès l'inclusion de ses trois en-têtes, **un jeu
de macros préprocesseur** qui décrit exactement la cible : OS, fenêtrage, architecture, jeu
d'instructions SIMD, compilateur, standard C++ supporté.

Le point essentiel à comprendre tout de suite : **ce n'est pas de la détection à l'exécution.** Il
n'y a pas de fonction `NkGetPlatformInfo()` qui interroge le CPU au lancement, pas de `if` qui
teste l'OS pendant que le jeu tourne. Tout est tranché **avant** que le binaire existe : le
préprocesseur regarde les macros que le compilateur lui donne (`_WIN32`, `__aarch64__`, `__clang__`…)
et active les bonnes branches. Une branche non prise n'est **pas compilée du tout** — elle n'existe
pas dans le binaire final. C'est ce qui permet d'écrire `#include <windows.h>` dans une branche
Windows sans casser la compilation Linux : sur Linux, cette branche est invisible.

Concrètement, NKPlatform est **100 % macros** : zéro classe, zéro fonction, une seule paire de
`typedef` (`__int128`, voir plus bas). On n'instancie rien, on ne construit rien — on **inclut**, et
les macros sont là.

- **Namespace** : aucun (ce sont des macros préprocesseur, hors de tout espace de noms)
- **Headers** : `#include "NKPlatform/NkPlatformDetect.h"` (OS / fenêtrage),
  `#include "NKPlatform/NkArchDetect.h"` (CPU / SIMD), `#include "NKPlatform/NkCompilerDetect.h"`
  (compilateur / C++)

---

## Détecter le système d'exploitation

La première chose qu'on veut savoir, c'est **sur quel OS on tourne**. `NkPlatformDetect.h` pose pour
cela une famille de macros mutuellement exclusives : sur une cible donnée, **une seule** des
`NKENTSEU_PLATFORM_WINDOWS`, `_LINUX`, `_MACOS`, `_IOS`, `_ANDROID`, `_EMSCRIPTEN`, `_HARMONYOS`…
est définie. Elles se déduisent des macros natives du compilateur (`_WIN32` pour Windows,
`__APPLE__ && __MACH__` pour l'écosystème Apple, `__ANDROID__` pour Android, `__EMSCRIPTEN__` pour
le Web…), via une cascade de `#elif` : la première qui correspond gagne, les autres sont écartées.

```cpp
#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)
    // crée une fenêtre Win32, charge d3d11.dll…
#elif defined(NKENTSEU_PLATFORM_LINUX)
    // ouvre une connexion X11 ou Wayland…
#elif defined(NKENTSEU_PLATFORM_MACOS)
    // instancie un NSWindow…
#endif
```

Quelques familles se subdivisent. Windows distingue `NKENTSEU_PLATFORM_WINDOWS_64` /
`_WINDOWS_32`, et une cible UWP pose en plus `NKENTSEU_PLATFORM_UWP`. L'écosystème Apple — détecté
via `<TargetConditionals.h>` — sépare `_MACOS`, `_IOS` (avec `_IOS_SIMULATOR` sous le simulateur),
`_TVOS`, `_WATCHOS`, `_VISIONOS`, `_MACCATALYST`. Détail qui piège : **Android est `__linux__`**,
mais le header le sort de la branche Linux (Android pose `_ANDROID`, pas `_LINUX`). De même
**Emscripten est posé après le bloc OS** : une cible Web peut donc traîner des macros résiduelles,
et la validation interne l'exclut explicitement de l'avertissement « plusieurs plateformes ».

Deux macros d'**identité** sont *toujours* définies, quelle que soit la cible :
`NKENTSEU_PLATFORM_NAME` (chaîne lisible : `"Windows"`, `"macOS"`, `"Web"`, `"HarmonyOS"`, …, ou
`"Unknown"`) et `NKENTSEU_PLATFORM_VERSION` (chaîne plus détaillée : `"Windows 64-bit"`,
`"Emscripten/WebAssembly"`…). Pratiques pour un log de démarrage ou une bannière de version. Si rien
n'a été reconnu, `NKENTSEU_PLATFORM_UNKNOWN` est posée et un `#warning` prévient.

> **En résumé.** Une macro `NKENTSEU_PLATFORM_<OS>` par cible, mutuellement exclusive, déduite des
> macros natives du compilateur à la compilation. `NKENTSEU_PLATFORM_NAME` / `_VERSION` donnent
> toujours une chaîne lisible. Pièges : Android est un Linux mais pose `_ANDROID` ; Emscripten est
> posé *après* le bloc OS.

---

## Le fenêtrage Linux et les catégories

Sous Linux, savoir « c'est Linux » ne suffit pas : il faut encore choisir **le système de
fenêtrage** — Xlib, XCB ou Wayland. NKPlatform le tranche aussi à la compilation. Par défaut, sans
rien faire, c'est `NKENTSEU_WINDOWING_XLIB` (compatibilité maximale). Pour forcer un backend, on
`#define` **avant** d'inclure le header l'un des `NKENTSEU_FORCE_WINDOWING_*_ONLY` (priorité
NOOP > XCB > XLIB > WAYLAND) :

```cpp
#define NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY
#include "NKPlatform/NkPlatformDetect.h"
// → NKENTSEU_WINDOWING_WAYLAND est défini ; ni Xlib ni XCB
```

Le header dérive ensuite `NKENTSEU_WINDOWING_X11` (si Xlib ou XCB), et
`NKENTSEU_WINDOWING_PREFERRED` (chaîne `"Wayland"`/`"XCB"`/`"Xlib"`/`"Noop"`). Subtilité : **HarmonyOS
est `__linux__` mais exclu de cette sélection** — sinon on inclurait `<X11/Xlib.h>` sur une cible qui
n'a pas X11.

Au-delà de l'OS exact, on raisonne souvent par **familles**. NKPlatform pose donc des macros de
*groupement* : `NKENTSEU_PLATFORM_DESKTOP` (Windows hors UWP/Xbox, ou macOS, ou Linux hors Android),
`NKENTSEU_PLATFORM_MOBILE` (iOS/Android/watchOS/visionOS), `NKENTSEU_PLATFORM_CONSOLE`,
`NKENTSEU_PLATFORM_HANDHELD` (PSP/Vita/NDS/3DS/Switch…), `NKENTSEU_PLATFORM_EMBEDDED` (Arduino/ESP32/
STM32/Raspberry Pi…), `NKENTSEU_PLATFORM_POSIX` / `_UNIX_LIKE` (Linux/macOS/BSD/**Android** — la branche
POSIX du NDK). On choisit ainsi « tout ce qui a un clavier et une souris » (desktop) ou « tout ce
qui a un écran tactile » (mobile) sans énumérer chaque OS.

> **En résumé.** Fenêtrage Linux : Xlib par défaut, forçable via `NKENTSEU_FORCE_WINDOWING_*_ONLY`
> **avant** l'include (HarmonyOS exclu). Macros de famille (`_DESKTOP`, `_MOBILE`, `_CONSOLE`,
> `_HANDHELD`, `_EMBEDDED`, `_POSIX`) pour raisonner par catégorie plutôt que par OS exact.

---

## Détecter l'architecture et le SIMD

`NkArchDetect.h` répond à une autre question : **quel CPU**. Comme pour l'OS, une cascade de `#elif`
pose une macro `NKENTSEU_ARCH_<X>` exclusive — `_X86_64`, `_ARM64`, `_RISCV64`, `_X86`, `_ARM`,
`_PPC`… — accompagnée de `NKENTSEU_ARCH_64BIT` ou `_32BIT`, et des chaînes `NKENTSEU_ARCH_NAME` /
`_VERSION`. Deux familles pratiques regroupent les variantes : `NKENTSEU_ARCH_INTEL` (x86 ou
x86_64) et `NKENTSEU_ARCH_ARM_FAMILY` (ARM ou ARM64).

Vient ensuite ce qui intéresse le plus le calcul haute performance : l'**endianness** et le
**SIMD**. `NKENTSEU_ARCH_LITTLE_ENDIAN` / `_BIG_ENDIAN` indiquent l'ordre des octets (presque
toujours little aujourd'hui, mais crucial dès qu'on sérialise pour le réseau ou un fichier). Et une
batterie de macros `NKENTSEU_CPU_HAS_*` signale les jeux d'instructions vectorielles disponibles —
`_SSE2`, `_AVX`, `_AVX2`, `_AVX512`, `_FMA` côté x86, `_NEON` côté ARM, `_ALTIVEC`/`_VSX` côté PPC :

```cpp
#include "NKPlatform/NkArchDetect.h"

#if defined(NKENTSEU_CPU_HAS_AVX2)
    // chemin AVX2 : 8 floats par instruction
#elif defined(NKENTSEU_CPU_HAS_NEON)
    // chemin NEON : 4 floats par instruction
#else
    // chemin scalaire de repli
#endif
```

Attention au sens exact : **ce sont des features *à la compilation*, pas une détection CPUID à
l'exécution.** `NKENTSEU_CPU_HAS_AVX2` est définie si **le compilateur** a reçu `-mavx2` (le header
le déduit de `__AVX2__`), pas parce qu'on a interrogé le processeur au lancement. Pour un *dispatch*
runtime (« ce CPU a-t-il AVX2 ? »), il faut un vrai CPUID ailleurs ; ces macros, elles, disent juste
« le code AVX2 est-il *compilable* ici ».

Enfin, l'archi fixe des **constantes mémoire** utiles aux allocateurs et au cache :
`NKENTSEU_CACHE_LINE_SIZE` (64 sur x86_64/ARM64), `NKENTSEU_MAX_ALIGNMENT`, `NKENTSEU_PAGE_SIZE`,
`NKENTSEU_HUGE_PAGE_SIZE`, `NKENTSEU_WORD_SIZE`/`_BITS`, `NKENTSEU_PTR_BITS` ; et des attributs
d'alignement portables `NKENTSEU_ALIGN_CACHE`, `_ALIGN_16/32/64`, `NKENTSEU_ALIGN(n)`.

> **En résumé.** `NKENTSEU_ARCH_<X>` + `_64BIT`/`_32BIT` pour le CPU, familles `_INTEL`/`_ARM_FAMILY`,
> endianness `_LITTLE/_BIG_ENDIAN`, et `NKENTSEU_CPU_HAS_*` pour le SIMD — **compile-time, pas
> CPUID**. Plus des constantes (`CACHE_LINE_SIZE`, `PAGE_SIZE`…) et attributs d'alignement portables.

---

## Détecter le compilateur et le standard C++

`NkCompilerDetect.h` (qui n'inclut **aucun** autre header) identifie **qui compile** :
`NKENTSEU_COMPILER_MSVC`, `_GCC`, `_CLANG` (avec `_APPLE_CLANG` si build Apple), `_INTEL`, plus des
additives (`_EMSCRIPTEN`, `_NVCC`…), chacune avec un `NKENTSEU_COMPILER_VERSION` numérique. En
parallèle, il déduit de `__cplusplus` le **standard supporté** : `NKENTSEU_CPP11` … `NKENTSEU_CPP23`,
et un entier `NKENTSEU_CPP_VERSION` (`11`, `17`, `20`, `23`…). Piège bien connu : sous MSVC,
`__cplusplus` reste figé à `199711L` **sans le flag `/Zc:__cplusplus`** — la détection de standard est
alors fausse.

L'intérêt n'est pas tant de tester le compilateur que d'utiliser les **macros de convenance
portables** qu'il en dérive. Plutôt que d'écrire `[[nodiscard]]` (qui ne compile pas en C++11) ou
`__restrict` (orthographe variable selon le compilateur), on écrit la macro neutre et NKPlatform
choisit la bonne forme :

```cpp
#include "NKPlatform/NkCompilerDetect.h"

NKENTSEU_NODISCARD int ComputeHash() NKENTSEU_NOEXCEPT;   // [[nodiscard]] … noexcept, ou repli
void Copy(float* NKENTSEU_RESTRICT dst, const float* NKENTSEU_RESTRICT src);
NKENTSEU_THREAD_LOCAL static int gTlsCounter;            // thread_local / __declspec(thread) / __thread
```

On dispose ainsi de `NKENTSEU_CONSTEXPR`, `_NOEXCEPT`, `_OVERRIDE`, `_FINAL`, `_NODISCARD`,
`_MAYBE_UNUSED`, `_FALLTHROUGH`, `_DEPRECATED`, `_RESTRICT`, `_THREAD_LOCAL`, du *packing* portable
(`NKENTSEU_PACK_BEGIN` / `_PACKED` / `_PACK_END`), des macros standard (`NKENTSEU_FUNCTION_NAME`,
`_FILE_NAME`, `_LINE_NUMBER`) et d'un contrôle de diagnostics (`NKENTSEU_DISABLE_WARNING_PUSH/POP`,
`_DISABLE_WARNING(w)`). Enfin, trois capacités **réellement** testées : `NKENTSEU_HAS_RTTI`,
`NKENTSEU_HAS_EXCEPTIONS` (sensibles à `-fno-rtti`/`-fno-exceptions`) et `NKENTSEU_HAS_INT128` — cette
dernière étant la **seule** macro à introduire du vrai C++ : `typedef __int128 NKENTSEU_int128;` et
son équivalent non signé.

> **En résumé.** `NKENTSEU_COMPILER_<X>` + `NKENTSEU_CPP_VERSION` pour identifier compilateur et
> standard ; surtout des **macros de convenance portables** (`NKENTSEU_NODISCARD`, `_NOEXCEPT`,
> `_RESTRICT`, `_THREAD_LOCAL`…) qui choisissent la bonne forme selon la cible. `HAS_RTTI`/
> `_EXCEPTIONS`/`_INT128` sont, elles, de vraies détections de capacité.

---

## Émettre du code par plateforme : `_ONLY` et `NOT_`

Les `#if`/`#elif` conviennent pour de gros blocs, mais pour une ligne ou deux, NKPlatform offre un
idiome plus compact : les macros **fonctionnelles** `NKENTSEU_<X>_ONLY(...)` et `NKENTSEU_NOT_<X>(...)`.
Si la plateforme `<X>` est active, `_ONLY(...)` **émet** son contenu et `NOT_(...)` le supprime ;
sinon l'inverse. Là encore, c'est du **filtrage à la compilation** : la branche non prise n'est pas
compilée, donc elle peut contenir des appels à des API qui n'existent que sur l'autre cible.

```cpp
NKENTSEU_WINDOWS_ONLY({ SetWindowTextW(hwnd, L"Nkentseu"); });
NKENTSEU_NOT_ANDROID({ EnableDesktopMenuBar(); });
NKENTSEU_NOT_WINDOWS({ NKENTSEU_NOT_MACOS({ UsePosixPath(); }); });  // imbricable
```

L'idiome : passer le bloc entre accolades suivi d'un `;`. Ces macros existent pour les OS
(`WINDOWS`, `LINUX`, `MACOS`, `IOS`, `ANDROID`, `HARMONYOS`…), les consoles, le fenêtrage (`XCB`,
`XLIB`, `WAYLAND`, `X11`), les catégories (`DESKTOP`, `MOBILE`, `CONSOLE`…), l'architecture
(`X86_64`, `ARM64`, `64BIT`, `LITTLE_ENDIAN`…) et le SIMD (`AVX2`, `NEON`…).

> **En résumé.** `NKENTSEU_<X>_ONLY(...)` émet du code si `<X>` est la cible, `NKENTSEU_NOT_<X>(...)`
> si elle ne l'est pas — du **filtrage compile-time imbricable**, pas un `if` runtime. Idiome :
> `NKENTSEU_WINDOWS_ONLY({ … });`.

---

## Aperçu de l'API

Toutes les entrées sont des **macros préprocesseur** (sauf les deux `typedef` `__int128`). Listées
par header. Chacune est détaillée dans la « Référence complète ».

### `NkPlatformDetect.h` — OS, fenêtrage, catégories

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Identité | `NKENTSEU_PLATFORM_NAME`, `NKENTSEU_PLATFORM_VERSION` | Chaîne lisible / détaillée (toujours définies). |
| Identité | `NKENTSEU_PLATFORM_UNKNOWN` | Posée + `#warning` si aucun OS reconnu. |
| OS Windows | `NKENTSEU_PLATFORM_WINDOWS` (+ `_WINDOWS_64`/`_32`, `_WINDOWS_10_OR_LATER`/`_8_1`/`_8`/`_7`, `_UWP`) | Famille Windows et ses variantes. |
| OS Apple | `NKENTSEU_PLATFORM_MACOS`, `_IOS` (+ `_IOS_SIMULATOR`), `_TVOS`, `_WATCHOS`, `_VISIONOS` (+ `_VISIONOS_SIMULATOR`), `_MACCATALYST` | Écosystème Apple. |
| OS Linux/Unix | `NKENTSEU_PLATFORM_LINUX`, `_ANDROID`, `_FREEBSD`, `_OPENBSD`, `_NETBSD`, `_SOLARIS`, `_UNIX` | Linux, Android et BSD/Unix. |
| OS Web | `NKENTSEU_PLATFORM_EMSCRIPTEN` | Cible WebAssembly. |
| OS Harmony | `NKENTSEU_PLATFORM_HARMONYOS` (pose aussi `_MOBILE`) | HarmonyOS. |
| Fenêtrage (forçage) | `NKENTSEU_FORCE_WINDOWING_XCB_ONLY` / `_XLIB_ONLY` / `_WAYLAND_ONLY` / `_NOOP_ONLY` | À `#define` **avant** l'include. |
| Fenêtrage (résultat) | `NKENTSEU_WINDOWING_XCB`, `_XLIB`, `_WAYLAND`, `_X11`, `_PREFERRED` | Backend retenu (défaut Xlib) + chaîne. |
| Console PlayStation | `NKENTSEU_PLATFORM_PLAYSTATION` (+ `_PS3`/`_PS4`/`_PS5`/`_PSP`/`_PSVITA`) | Famille PlayStation. |
| Console Xbox | `NKENTSEU_PLATFORM_XBOX` (+ `_XBOX_ORIGINAL`/`_XBOX360`/`_XBOXONE`/`_XBOX_SERIES`) | Famille Xbox. |
| Console Nintendo | `NKENTSEU_PLATFORM_NINTENDO` (+ `_SWITCH`/`_WII`/`_WIIU`/`_N64`/`_GAMECUBE`/`_3DS`/`_NDS`/`_GBA`/`_GAMEBOY`…) | Famille Nintendo. |
| Console Sega/rétro | `NKENTSEU_PLATFORM_SEGA` (+ `_DREAMCAST`/`_SATURN`/`_GENESIS`…), `_ATARI2600`, `_NEOGEO`, `_3DO`… | Sega et rétro. |
| Embarqué | `NKENTSEU_PLATFORM_ESP32`/`_ARDUINO`/`_STM32`/`_RASPBERRY_PI`/`_TEENSY`/`_ESP8266` | Microcontrôleurs / SBC. |
| Steam | `NKENTSEU_PLATFORM_STEAM_DECK`, `_STEAM`, `_STEAM_RUNTIME` | Steam Deck / runtime Steam. |
| Catégories | `NKENTSEU_PLATFORM_DESKTOP`, `_MOBILE`, `_HANDHELD`, `_CONSOLE`, `_EMBEDDED`, `_POSIX`, `_UNIX_LIKE`, `_EMSCRIPTEN_BROWSER` | Groupements par famille. |
| Conditionnelles | `NKENTSEU_<X>_ONLY(...)`, `NKENTSEU_NOT_<X>(...)` | Émission de code compile-time (OS, console, fenêtrage, catégories). |

### `NkArchDetect.h` — architecture, SIMD, mémoire

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Identité | `NKENTSEU_ARCH_NAME`, `NKENTSEU_ARCH_VERSION` | Chaîne lisible / détaillée. |
| Arch 64-bit | `NKENTSEU_ARCH_X86_64`, `_ARM64`, `_PPC64`, `_MIPS64`, `_RISCV64`, `_SPARC64` | CPU 64 bits (pose `_64BIT`). |
| Arch 32-bit | `NKENTSEU_ARCH_X86`, `_ARM`, `_PPC`, `_MIPS`, `_SPARC` | CPU 32 bits (pose `_32BIT`). |
| Arch spéciales | `NKENTSEU_ARCH_ELBRUS`, `_SUPERH`, `_ALPHA`, `_IA64`, `_M68K`, `_S390`, `_XTENSA`, `_R5900`, `_CELL_PPU`… | Architectures exotiques / consoles. |
| Arch fallback | `NKENTSEU_ARCH_UNKNOWN` (+ `_UNKNOWN_64`/`_32`) | Cible non reconnue, bitness déduite. |
| Bitness | `NKENTSEU_ARCH_64BIT`, `NKENTSEU_ARCH_32BIT` | Largeur de pointeur. |
| Familles | `NKENTSEU_ARCH_INTEL`, `NKENTSEU_ARCH_ARM_FAMILY` | Regroupement x86 / ARM. |
| Endianness | `NKENTSEU_ARCH_LITTLE_ENDIAN`, `NKENTSEU_ARCH_BIG_ENDIAN` | Ordre des octets (défaut little). |
| SIMD x86 | `NKENTSEU_CPU_HAS_SSE`/`_SSE2`/`_SSE3`/`_SSSE3`/`_SSE4_1`/`_SSE4_2`/`_AVX`/`_AVX2`/`_AVX512`/`_AES`/`_BMI`/`_FMA` | Jeux d'instructions x86 (compile-time). |
| SIMD ARM/PPC/MIPS | `NKENTSEU_CPU_HAS_NEON`/`_ARM_CRYPTO`/`_ARM_CRC32`/`_ALTIVEC`/`_VSX`/`_MSA` | SIMD ARM / PowerPC / MIPS. |
| Constantes | `NKENTSEU_CACHE_LINE_SIZE`, `_MAX_ALIGNMENT`, `_PAGE_SIZE`, `_HUGE_PAGE_SIZE`, `_WORD_SIZE`, `_WORD_BITS`, `_PTR_BITS` | Tailles mémoire / mot / pointeur. |
| Alignement | `NKENTSEU_ALIGN_CACHE`, `_ALIGN_16`, `_ALIGN_32`, `_ALIGN_64`, `NKENTSEU_ALIGN(n)` | Attributs d'alignement portables. |
| Conditionnelles | `NKENTSEU_<X>_ONLY(...)`, `NKENTSEU_NOT_<X>(...)` | Émission compile-time (arch, bitness, endianness, SIMD). |
| Debug | `NKENTSEU_ARCH_DEBUG` (si défini → `#pragma message`) | Affiche l'archi détectée. |

### `NkCompilerDetect.h` — compilateur, standard C++, attributs

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Compilateur | `NKENTSEU_COMPILER_MSVC`/`_CLANG`/`_GCC`/`_INTEL` (+ `_APPLE_CLANG`, `_EMSCRIPTEN`, `_NVCC`, `_SUNPRO`, `_XLC`), `_VERSION` | Identité + version du compilateur. |
| Sous-versions MSVC | `NKENTSEU_COMPILER_MSVC_2017`/`_2019`/`_2022`/`_2024` | Génération Visual Studio (cumulatives). |
| Standard C++ | `NKENTSEU_CPP98`/`_CPP11`/`_CPP14`/`_CPP17`/`_CPP20`/`_CPP23`, `NKENTSEU_CPP_VERSION` | Standard supporté (déduit de `__cplusplus`). |
| Features (HAS) | `NKENTSEU_HAS_CPP11/14/17/20/23`, `_HAS_NULLPTR`, `_HAS_CONSTEXPR`, `_HAS_NOEXCEPT`, `_HAS_OVERRIDE`, `_HAS_LAMBDA`, `_HAS_IF_CONSTEXPR`, `_HAS_NODISCARD`, `_HAS_CONCEPTS`, `_HAS_COROUTINES`, `_HAS_CONSTEVAL`… | Disponibilité de features par seuil de standard. |
| Convenance | `NKENTSEU_CONSTEXPR`, `_NOEXCEPT`, `_NOEXCEPT_IF(c)`, `_OVERRIDE`, `_FINAL`, `_NODISCARD`, `_NODISCARD_MSG(m)`, `_NODISCARD_SIMPLE`, `_MAYBE_UNUSED`, `_FALLTHROUGH` | Mots-clés portables (forme correcte ou repli). |
| Attributs | `NKENTSEU_PACKED`, `_PACK_BEGIN`, `_PACK_END`, `_RESTRICT`, `_THREAD_LOCAL`, `_DEPRECATED`, `_DEPRECATED_MSG(m)` | Packing, restrict, TLS, dépréciation portables. |
| Macros standard | `NKENTSEU_FUNCTION_NAME`, `_FILE_NAME`, `_LINE_NUMBER` | `__FUNCSIG__`/`__PRETTY_FUNCTION__`/`__func__`, fichier, ligne. |
| Capacités | `NKENTSEU_HAS_RTTI`, `_HAS_EXCEPTIONS`, `_HAS_INT128` (+ `NKENTSEU_int128`/`_uint128`) | Détections réelles ; `_INT128` déclare 2 `typedef`. |
| Pragmas | `NKENTSEU_PRAGMA(x)`, `_DISABLE_WARNING_PUSH`/`_POP`, `_DISABLE_WARNING(w)`, `_DISABLE_WARNING_MSVC(x)`/`_CLANG(x)`/`_GCC(x)` | Contrôle de diagnostics portable. |
| Debug | `NKENTSEU_COMPILER_DEBUG` (si défini → `#pragma message`) | Affiche compilateur / standard. |

---

## Référence complète

Chaque élément est repris ici. Les triviaux (chaînes d'identité, sous-versions) sont brefs ; les
mécanismes structurants — endianness réseau, dispatch SIMD, export DLL, alignement cache — sont
détaillés avec leurs usages par domaine du temps réel.

### Les macros OS et leurs sous-divisions

`NKENTSEU_PLATFORM_<OS>` est le socle : une seule active par cible, déduite des macros natives
(`_WIN32`, `__APPLE__ && __MACH__`, `__ANDROID__`, `__OHOS__ || __HARMONY__`, `__EMSCRIPTEN__`…) via
une cascade `#elif`. C'est sur elles que reposent **tous** les modules au-dessus de Foundation pour
choisir leur implémentation native :

- **Rendu / GPU** — `NKENTSEU_PLATFORM_WINDOWS` sélectionne D3D11/D3D12 ; `_LINUX` choisit le
  swapchain Vulkan KHR adéquat selon `NKENTSEU_WINDOWING_*` ; `_MACOS`/`_IOS` orientent vers Metal ;
  `_EMSCRIPTEN` vers WebGL. La bonne extension de surface Vulkan (`VK_KHR_win32_surface` vs
  `_xlib_surface` vs `_wayland_surface`) découle directement de ces macros.
- **Fenêtre / événements** — NKWindow et NKEvent ont un backend par OS (Win32, XLib/XCB/Wayland,
  Cocoa, UIKit, Android, Emscripten, HarmonyOS) compilé exclusivement par sa macro.
- **IO / réseau** — chemins de fichiers (séparateur, casse), API socket (Winsock vs BSD sockets),
  notifications de système de fichiers diffèrent par OS ; `_POSIX` regroupe ce qui partage l'API
  Unix.
- **Audio** — backend par OS (WASAPI/Windows, ALSA/Linux, CoreAudio/Apple, OpenSL ES|AAudio/Android).
- **UI / 2D** — DPI, conventions d'écran *safe-area* (mobile), événements tactiles vs souris se
  branchent sur `_MOBILE`/`_DESKTOP`.

Les sous-divisions affinent : `_WINDOWS_64`/`_32` (taille de pointeur), `_WINDOWS_10_OR_LATER`
(features récentes), `_UWP` (sandbox application, API restreinte), `_IOS_SIMULATOR` (pas de vrai
GPU/caméra). `NKENTSEU_PLATFORM_NAME`/`_VERSION` servent surtout aux logs et bannières de version.
Si rien n'est reconnu, `_UNKNOWN` + `#warning` alertent — un garde-fou pour les portages exotiques.

### Le fenêtrage Linux : `NKENTSEU_WINDOWING_*`

Sur Linux, un même OS expose **plusieurs** systèmes de fenêtrage incompatibles. Le forçage se fait
en amont : `#define NKENTSEU_FORCE_WINDOWING_XCB_ONLY` (ou `_XLIB_ONLY`/`_WAYLAND_ONLY`/`_NOOP_ONLY`)
**avant** l'include, priorité NOOP > XCB > XLIB > WAYLAND. Sans forçage, défaut `NKENTSEU_WINDOWING_XLIB`
(compatibilité maximale). Le header dérive `_X11` (Xlib ou XCB) et `_PREFERRED` (chaîne pour les
logs). Usages :

- **Rendu** — détermine l'extension de surface Vulkan/EGL à charger ; un mauvais choix fait échouer
  la création de swapchain.
- **Fenêtre / événements** — sélectionne la boucle d'événements (XEvent vs `xcb_*` vs `wl_*`) ; le
  mode `NOOP` permet un build **headless** (serveur, tests CI sans display) sans tirer X11.
- **Piège** — HarmonyOS est `__linux__` mais **exclu** de cette sélection, sinon `<X11/Xlib.h>`
  serait inclus à tort.

### Les catégories de plateforme

Plutôt que d'énumérer chaque OS, on cible une **famille**. `NKENTSEU_PLATFORM_DESKTOP` (clavier +
souris + fenêtres), `_MOBILE` (tactile + capteurs + cycle de vie suspend/resume), `_HANDHELD`
(consoles portables), `_CONSOLE` (manette + sortie TV fixe), `_EMBEDDED` (microcontrôleur, mémoire
contrainte), `_POSIX`/`_UNIX_LIKE` (API Unix partagée, **Android inclus** pour la branche NDK).
Usages :

- **UI / 2D** — `_MOBILE` active les contrôles tactiles et le respect des *safe-areas* ; `_DESKTOP`
  active la barre de menus et le redimensionnement libre.
- **Gameplay** — schémas d'entrée (souris/clavier vs manette vs tactile) sélectionnés par catégorie.
- **Threading / mémoire** — `_EMBEDDED` réduit les *pools* et le nombre de threads ; le desktop
  suppose plusieurs cœurs.
- **IO** — `_POSIX` regroupe l'implémentation de fichiers/sockets style Unix au lieu de la dupliquer
  par OS.

### L'architecture : `NKENTSEU_ARCH_*` et la bitness

Une macro `NKENTSEU_ARCH_<X>` exclusive (X86_64, ARM64, RISCV64, X86, ARM…) plus `_64BIT`/`_32BIT`.
Les familles `_INTEL` et `_ARM_FAMILY` regroupent les variantes. Usages :

- **Mémoire / threading** — la bitness conditionne la largeur de pointeur (`NKENTSEU_PTR_BITS`), donc
  la taille des structures, la stratégie d'allocateur et la réservation d'adresses virtuelles.
- **GPU / asset** — certaines tailles de blocs de compression de textures ou alignements de buffers
  dépendent de l'archi.
- **Rendu** — le code SIMD de transformation de sommets/skinning branche sur la famille (voir SIMD).

Pour un CPU non reconnu, `_UNKNOWN` déduit la bitness via `__SIZEOF_POINTER__` puis `UINTPTR_MAX`,
avec un repli 64-bit — le moteur compile même sur une cible exotique, quitte à perdre les chemins
optimisés.

### L'endianness : `LITTLE_ENDIAN` / `BIG_ENDIAN`

`NKENTSEU_ARCH_LITTLE_ENDIAN` / `_BIG_ENDIAN` donnent l'ordre des octets d'un mot multi-octets, déduit
de `__BYTE_ORDER__` (avec heuristiques de repli, **défaut little-endian**). C'est *trivial* au
quotidien (presque tout est little aujourd'hui) mais **critique** dès qu'un octet quitte la machine :

- **IO / réseau** — un protocole réseau impose l'ordre **big-endian** (« network byte order ») ;
  sérialiser un entier sans corriger l'endianness produit des paquets illisibles par un pair d'archi
  différente. La macro choisit s'il faut byte-swapper.
- **Fichiers / assets** — un format binaire (`.nkb`, textures, audio) lu sur une archi et écrit sur
  une autre doit fixer un ordre canonique ; la macro pilote la conversion au chargement.
- **GPU** — l'empaquetage de certains formats de couleur/normales packés dépend de l'ordre des
  octets.

C'est l'un des rares endroits où ignorer la détection donne un bug **silencieux et insidieux** : ça
marche en local (même archi) et casse en production multi-plateforme.

### Le SIMD : `NKENTSEU_CPU_HAS_*`

Famille de macros par jeu d'instructions vectorielles : x86 (`SSE2`, `AVX`, `AVX2`, `AVX512`, `FMA`,
`AES`, `BMI`), ARM (`NEON`, `ARM_CRYPTO`, `ARM_CRC32`), PPC (`ALTIVEC`, `VSX`), MIPS (`MSA`),
mutuellement exclusives par famille d'archi. Le point capital, répété car souvent mal compris : **ce
sont des features à la *compilation*, déduites des flags du compilateur (`-mavx2` → `__AVX2__`), pas
un CPUID runtime.** Elles disent « ce code vectoriel est-il *compilable* ici », pas « ce processeur
le supporte-t-il maintenant ». Usages :

- **Rendu** — transformer des milliers de sommets, faire le *skinning* d'un mesh, packer des
  attributs : un chemin AVX2 (8 floats/instruction) ou NEON (4 floats) vs un repli scalaire,
  sélectionné par `#if defined(NKENTSEU_CPU_HAS_AVX2)`.
- **Physique / collision** — tests de boîtes englobantes, *broadphase*, résolution de contraintes en
  batch profitent énormément du SIMD.
- **Animation** — *blending* de poses, interpolation de quaternions en masse.
- **Audio** — mixage de centaines de voix, filtres, convolution HRTF se vectorisent naturellement.
- **IA / gameplay** — distances en masse (voisinage, *flocking*), tests de champ de vision.
- **Math** — les opérations `NkVec`/`NkMat` SIMD de NKMath choisissent leur implémentation sur ces
  macros.

Pour activer/désactiver un chemin selon le *vrai* CPU à l'exécution (un binaire unique servant des
machines hétérogènes), il faut un CPUID runtime distinct ; ces macros ne couvrent que la
compilation.

### Constantes mémoire et attributs d'alignement

`NKENTSEU_CACHE_LINE_SIZE` (64 sur x86_64/ARM64), `_MAX_ALIGNMENT`, `_PAGE_SIZE`, `_HUGE_PAGE_SIZE`
(2 Mo), `_WORD_SIZE`/`_BITS`, `_PTR_BITS` décrivent la machine pour les couches basses. Les attributs
`NKENTSEU_ALIGN_CACHE`, `_ALIGN_16/32/64`, `NKENTSEU_ALIGN(n)` posent l'alignement de façon portable
(`__declspec(align)` sous MSVC, `__attribute__((aligned))` sous GCC/Clang). Usages :

- **Threading** — aligner sur `CACHE_LINE_SIZE` les variables touchées par plusieurs threads évite
  le *false sharing* (deux cœurs se disputant la même ligne de cache) ; c'est l'usage emblématique.
- **Mémoire** — `PAGE_SIZE`/`HUGE_PAGE_SIZE` dimensionnent les arènes virtuelles et l'alignement des
  *mmap* ; `MAX_ALIGNMENT` borne ce que l'allocateur doit garantir.
- **GPU / rendu** — les buffers uniformes et de sommets exigent souvent un alignement précis
  (`ALIGN_16` pour des `vec4`) ; les structures de *constant buffer* s'alignent via `NKENTSEU_ALIGN`.
- **Math / SIMD** — un `NkMat4` chargé en AVX veut un alignement 32 octets (`ALIGN_32`).

### Le compilateur et le standard C++

`NKENTSEU_COMPILER_<X>` identifie le compilateur, `NKENTSEU_CPP_VERSION` / `NKENTSEU_CPPxx` le standard
(déduit de `__cplusplus` — d'où le piège MSVC `/Zc:__cplusplus`). Les macros `NKENTSEU_HAS_*`
signalent la disponibilité d'une feature par seuil de standard (constexpr, lambdas, concepts,
coroutines…). En pratique on les consomme via les **macros de convenance** : `NKENTSEU_CONSTEXPR`,
`_NOEXCEPT`/`_NOEXCEPT_IF`, `_OVERRIDE`, `_FINAL`, `_NODISCARD`(+`_MSG`/`_SIMPLE`), `_MAYBE_UNUSED`,
`_FALLTHROUGH`, qui rendent la bonne forme si le standard la supporte, sinon un repli vide ou
ancien. Usages transverses : tout le code du moteur (ECS, rendu, conteneurs) les utilise pour rester
compilable du C++11 au C++23 sans `#if` partout — par exemple `NKENTSEU_NODISCARD` sur un
`ComputeHash()` pour qu'oublier le résultat soit signalé, ou `NKENTSEU_OVERRIDE` sur les méthodes
virtuelles des backends.

### Les attributs portables et l'export DLL

`NKENTSEU_PACKED` / `_PACK_BEGIN` / `_PACK_END` empaquètent une structure sans *padding* (formats de
fichiers binaires, en-têtes réseau, descripteurs GPU dont la disposition mémoire est imposée).
`NKENTSEU_RESTRICT` promet au compilateur l'absence d'*aliasing* (boucles de copie/mix, kernels math
— gain réel en vectorisation). `NKENTSEU_THREAD_LOCAL` donne un stockage par thread portable (contexte
de rendu courant, pile d'erreurs par thread, allocateur *scratch* local). `NKENTSEU_DEPRECATED`(+`_MSG`)
marque une API obsolète. Ces attributs sont aussi le socle d'une éventuelle macro d'**export DLL** :
le choix `__declspec(dllexport/dllimport)` (MSVC) vs `__attribute__((visibility("default")))`
(GCC/Clang) se branche exactement sur `NKENTSEU_COMPILER_MSVC` vs GCC/Clang — la même logique que ces
attributs portables. Enfin `NKENTSEU_FUNCTION_NAME`/`_FILE_NAME`/`_LINE_NUMBER` alimentent les logs,
les asserts et les *crash reports* avec un contexte source uniforme.

### Capacités réelles : RTTI, exceptions, int128

Contrairement aux `HAS_*` déduites du standard, trois macros testent une **vraie** capacité du build :
`NKENTSEU_HAS_RTTI` (sensible à `-fno-rtti`/`/GR-`), `NKENTSEU_HAS_EXCEPTIONS` (sensible à
`-fno-exceptions`) et `NKENTSEU_HAS_INT128`. Un moteur zero-STL compilé souvent sans RTTI ni
exceptions s'en sert pour basculer entre un `dynamic_cast` et un système de type maison, ou entre un
chemin avec/sans `throw`. `NKENTSEU_HAS_INT128` est la **seule** macro à introduire du vrai C++ :
`typedef __int128 NKENTSEU_int128;` et son équivalent non signé — utiles pour le hachage 128 bits,
les compteurs larges, certains calculs de chiffrement, là où ils sont disponibles (GCC/Clang hors
MSVC).

### Le contrôle de diagnostics

`NKENTSEU_DISABLE_WARNING_PUSH`/`_POP` encadrent une zone où l'on neutralise un *warning* précis via
`NKENTSEU_DISABLE_WARNING(w)` (numéro sous MSVC, chaîne `"-Wxxx"` sous GCC/Clang), avec les variantes
ciblées `_DISABLE_WARNING_MSVC/_CLANG/_GCC`. Indispensable pour inclure proprement un header tiers
bruyant, ou pour assumer localement un *warning* dans du code générique — sans baisser le niveau de
*warnings* globalement (le moteur compile en `-Werror`).

---

### Exemple récapitulatif

```cpp
#include "NKPlatform/NkPlatformDetect.h"
#include "NKPlatform/NkArchDetect.h"
#include "NKPlatform/NkCompilerDetect.h"

// 1. Bannière de démarrage (chaînes toujours définies).
NkLog("Nkentseu sur %s / %s / C++%d",
      NKENTSEU_PLATFORM_NAME, NKENTSEU_ARCH_NAME, NKENTSEU_CPP_VERSION);

// 2. Choix d'API graphique par OS — branche non prise jamais compilée.
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    CreateD3D12Device();
#elif defined(NKENTSEU_PLATFORM_LINUX)
    CreateVulkanDevice(NKENTSEU_WINDOWING_PREFERRED);   // Xlib par défaut
#elif defined(NKENTSEU_PLATFORM_MACOS)
    CreateMetalDevice();
#endif

// 3. Dispatch SIMD compile-time pour transformer des sommets.
#if defined(NKENTSEU_CPU_HAS_AVX2)
    TransformVerticesAVX2(mesh);
#elif defined(NKENTSEU_CPU_HAS_NEON)
    TransformVerticesNEON(mesh);
#else
    TransformVerticesScalar(mesh);
#endif

// 4. Sérialisation réseau : corriger l'endianness avant l'envoi.
#if defined(NKENTSEU_ARCH_BIG_ENDIAN)
    value = ByteSwap32(value);   // network byte order
#endif

// 5. Une ligne par plateforme via l'idiome _ONLY (filtrage compile-time).
NKENTSEU_WINDOWS_ONLY({ SetProcessDpiAwareness(); });

// 6. Attributs portables : alignement cache + nodiscard + restrict.
struct NKENTSEU_ALIGN_CACHE FrameStats { nk_uint64 frame; };
NKENTSEU_NODISCARD nk_uint32 Hash(const float* NKENTSEU_RESTRICT data, nk_uint32 n) NKENTSEU_NOEXCEPT;
```

---

[← Index NKPlatform](README.md) · [Récap NKPlatform](../NKPlatform.md) · [Index NKCore →](../NKCore/README.md)
