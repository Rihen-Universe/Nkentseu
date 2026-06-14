# CPU, endianness et détection GPU

> Couche **Foundation** · NKPlatform · Trois questions que tout moteur portable se pose au
> démarrage : **de quoi est faite la machine** (cœurs, caches, SSE2/AVX2/NEON), **dans quel sens
> elle range les octets** (little/big-endian), et **quelle API graphique** est disponible.

Un moteur qui se veut « cross-platform » ne peut pas se contenter de compiler partout : il doit
**s'adapter à la machine sur laquelle il tourne**. Trois besoins reviennent sans cesse. D'abord,
choisir le bon chemin de code selon le **CPU** : faut-il prendre la boucle scalaire, la version
SSE2 ou la version AVX2 ? Combien de cœurs pour dimensionner le *thread pool* ? Ensuite, savoir
**dans quel ordre la machine range les octets d'un entier** — détail invisible tant qu'on reste en
mémoire, mais qui devient critique dès qu'on écrit un fichier ou qu'on envoie un paquet réseau qui
sera relu sur une autre machine. Enfin, savoir **quelle API graphique** (Vulkan, DirectX, Metal,
OpenGL…) est disponible sur la plateforme cible, pour choisir le backend à compiler et à activer.

NKPlatform répond à ces trois besoins avec trois en-têtes indépendants. Le premier interroge le CPU
**à l'exécution** (CPUID sur x86, registres système sur ARM) et met le résultat en cache. Le
deuxième résout l'endianness **à la compilation** (`constexpr`) et fournit tout l'outillage de
permutation d'octets et de conversion vers l'ordre réseau. Le troisième est presque entièrement du
**préprocesseur** : il déduit de la plateforme les API graphiques disponibles et expose des macros
de compilation conditionnelle.

- **Namespaces** : `nkentseu::platform` (CPU, endianness) · `nkentseu::graphics` (enums GPU)
- **Headers** : `#include "NKPlatform/NkCPUFeatures.h"` · `#include "NKPlatform/NkEndianness.h"` ·
  `#include "NKPlatform/NkCGXDetect.h"`

---

## Interroger le CPU : `NkCPUFeatures`

Quand on écrit une routine mathématique critique — transformer un million de sommets, mélanger un
buffer audio, parcourir un tableau de composants ECS — on veut profiter des instructions **SIMD**
du processeur. Mais on ne sait pas, à la compilation, sur quelle machine le binaire tournera : un
exécutable AVX2 plante sur un CPU qui ne connaît pas AVX2. La solution est le **dispatch à
l'exécution** : on détecte les capacités au démarrage, puis on branche vers la meilleure
implémentation disponible. C'est exactement ce que fait `CPUFeatures` (alias `NkCPUFeatures`).

L'objet est un **singleton à détection paresseuse** : le premier appel à `CPUFeatures::Get()`
exécute `CPUID` (sur x86/x64) ou lit les registres système, remplit toutes les sous-structures,
puis **met le résultat en cache** ; les appels suivants renvoient la même instance sans rien
recalculer. L'initialisation s'appuie sur une statique locale C++11, donc **thread-safe** par
construction.

```cpp
const NkCPUFeatures& cpu = NkGetCPUFeatures();          // détection une seule fois, en cache
if (cpu.simd.hasAVX2)        TransformVerticesAVX2(mesh);
else if (cpu.simd.hasSSE2)   TransformVerticesSSE2(mesh);
else                         TransformVerticesScalar(mesh);

int pool = cpu.topology.numLogicalCores;                // dimensionner le thread pool
```

Le résultat n'est **pas** un simple drapeau « SIMD oui/non » : c'est une fiche complète de la
machine. `vendor` (`"GenuineIntel"`, `"AuthenticAMD"`…) et `brand` (la chaîne marketing complète),
les `family`/`model`/`stepping`, la **topologie** (`numPhysicalCores`, `numLogicalCores`,
`numSockets`, `hasHyperThreading`), la **hiérarchie de cache** (`lineSize` en octets, `l1DataSize`,
`l2Size`, `l3Size` en KB), les **extensions SIMD** (toute la famille SSE/AVX/AVX-512 et NEON/SVE),
et les **extensions étendues** (AES, SHA, RDRAND, POPCNT, BMI, VT-x/AMD-V…).

Pour les capacités les plus courantes, inutile de naviguer dans les sous-structures : des
**helpers** lisent directement le bon champ — `NkHasSSE2()`, `NkHasAVX()`, `NkHasAVX2()`,
`NkHasAVX512()`, `NkHasNEON()`, `NkHasFMA()`, plus `NkGetCacheLineSize()`,
`NkGetPhysicalCoreCount()`, `NkGetLogicalCoreCount()`.

Un piège à connaître sur l'affichage : `ToString(buffer, taille)` formate dans **votre** buffer
(toujours terminé par `\0`, tronqué si nécessaire) et est sûr ; mais la surcharge `ToString()`
sans argument renvoie un **buffer statique interne partagé** — pratique pour un log rapide, mais
**non thread-safe**, et il ne faut pas conserver le pointeur entre deux appels.

> **En résumé.** `NkGetCPUFeatures()` (= `CPUFeatures::Get()`) détecte une fois, met en cache, et
> renvoie une fiche complète : vendeur, topologie, caches, SIMD, extensions. Le dispatch SIMD à
> l'exécution (`hasAVX2` → branche AVX2) est l'usage central. Les helpers `NkHasX()` raccourcissent
> l'accès. `ToString()` sans argument = buffer statique partagé, **non thread-safe**.

---

## Ranger les octets : `NkEndianness`

Un entier 32 bits comme `0x01020304` occupe quatre octets en mémoire — mais **dans quel ordre** ?
Sur une machine *little-endian* (x86, ARM courant), l'octet de poids faible vient en premier :
`04 03 02 01`. Sur une machine *big-endian*, c'est l'inverse : `01 02 03 04`. Tant que la donnée
reste dans la RAM de la même machine, cela n'a aucune importance : on lit toujours `0x01020304`.
Le problème surgit dès qu'on **sérialise** — un fichier binaire écrit sur une machine et relu sur
une autre, un paquet réseau émis par un PC et reçu par une console — car les octets traversent la
frontière dans leur ordre brut. C'est là qu'il faut **convertir**.

`NkEndianness.h` traite cela presque entièrement **à la compilation**. L'endianness de la machine
cible est connue du compilateur, donc `NkIsLittleEndian()` et `NkIsBigEndian()` sont `constexpr` :
le compilateur élimine la branche morte, et une conversion vers l'ordre réseau sur x86 se réduit à
une simple instruction de permutation, sans le moindre test à l'exécution.

```cpp
// Écrire un en-tête réseau : convertir vers le big-endian (ordre réseau) avant l'envoi
uint32_t magic = 0xCAFEBABE;
uint32_t wire  = NkHostToNetwork32(magic);   // no-op sur big-endian, byte-swap sur little-endian
WriteBE32(packet + 0, magic);                // ou directement, sans variable intermédiaire

// Relire : du big-endian du fichier vers l'ordre machine
uint32_t value = ReadBE32(buffer);
```

Au cœur du module, **trois primitives de permutation** : `ByteSwap16`, `ByteSwap32`, `ByteSwap64`
(et leurs alias `NkByteSwap*`), toutes `constexpr` et adossées aux intrinsèques du compilateur
(`_byteswap_*` sous MSVC, `__builtin_bswap*` sous GCC/Clang), avec un repli portable. Un template
`ByteSwap(T)` généralise à n'importe quel type de 2, 4 ou 8 octets — y compris `float`/`double` —
via un `static_assert` sur la taille.

Au-dessus se construit l'**ordre réseau** : `HostToNetwork16/32/64` et `NetworkToHost16/32/64` (le
réseau est *big-endian* par convention, comme les `htonl`/`ntohl` POSIX), les conversions
**explicites** `ToLittleEndian`/`ToBigEndian` et leurs réciproques, les variantes **par buffer**
(`ByteSwapBuffer`, `BufferToBigEndian`…) qui parcourent un tableau — attention, elles comptent des
**éléments**, pas des octets — et l'**accès non-aligné** (`ReadUnaligned*`/`WriteUnaligned*` via
`memcpy`, puis les combinés `ReadLE*`/`ReadBE*`/`WriteLE*`/`WriteBE*` qui lisent et convertissent
en une fois). Tout cela existe aussi en **macros** (`NK_BSWAP*`, `NK_HTON*`, `NK_NTOH*`).

> **En résumé.** L'endianness se résout à la **compilation** (`NkIsLittleEndian()` constexpr). Pour
> sérialiser, convertir vers l'**ordre réseau** (`NkHostToNetwork32` / `NetworkToHost32`) ou
> directement avec `ReadBE*`/`WriteBE*` qui combinent accès non-aligné et conversion. `ByteSwap*`
> sont les primitives ; `ByteSwapBuffer` compte des **éléments**, pas des octets.

---

## Détecter l'API graphique : `NkCGXDetect`

Avant même d'instancier un backend de rendu, le moteur doit savoir **ce que la plateforme offre** :
sur Windows on a DirectX et Vulkan, sur macOS c'est Metal, sur Android OpenGL ES et Vulkan, dans le
navigateur WebGL2 et WebGPU. `NkCGXDetect.h` répond à cette question **entièrement par le
préprocesseur** : selon la plateforme détectée, il définit un jeu de macros `*_AVAILABLE` et fournit
des macros de **compilation conditionnelle** pour n'embarquer que le code pertinent.

```cpp
// Ne compiler le backend Vulkan QUE là où il existe
NKENTSEU_VULKAN_ONLY(
    InitVulkanBackend();
)
NKENTSEU_NOT_VULKAN(
    // chemin de repli sur les plateformes sans Vulkan
)

using namespace nkentseu::graphics;
NkGraphicsApi api = NkGraphicsApi::NK_GFX_API_VULKAN;   // enum séquentiel, NK_GFX_API_NONE = 0
```

Le module fournit aussi des **enums** côté `nkentseu::graphics` : `NkGraphicsApi` (la liste des API,
de `NK_GFX_API_OPENGL` à `NK_GFX_API_NVN`), `NkGPUVendor` (les *vendor IDs* PCI réels —
`NK_NVIDIA = 0x10DE`, `NK_AMD = 0x1002`, `NK_INTEL = 0x8086`…) et `NkGPUType`
(discret/intégré/virtuel/logiciel). Deux pièges à garder en tête. D'abord, `NK_GFX_API_AUTO` (14)
est placée **après** la sentinelle `NK_GFX_API_MAX` (13) : une boucle `[0, NK_GFX_API_MAX)`
n'inclut **pas** AUTO. Ensuite, les enums vivent dans `nkentseu::graphics`, et **non** dans
`nkentseu::platform::graphics` — un chemin de namespace incohérent traîne d'ailleurs dans les
macros `NKENTSEU_GRAPHICS_DEFAULT`/`MODERN`.

> **En résumé.** `NkCGXDetect.h` déduit de la plateforme les API graphiques disponibles
> (`*_AVAILABLE`) et expose des paires `NKENTSEU_<API>_ONLY(...)` / `NKENTSEU_NOT_<API>(...)` pour
> compiler le bon backend. Les enums (`NkGraphicsApi`, `NkGPUVendor`, `NkGPUType`) sont dans
> `nkentseu::graphics`. Méfiance : `NK_GFX_API_AUTO` est hors du compte de `NK_GFX_API_MAX`.

---

## Aperçu de l'API

La liste de **tous** les éléments publics (macros incluses), en un coup d'œil. Chacun est détaillé
dans la « Référence complète » qui suit.

### `NkCPUFeatures.h` — namespace `nkentseu::platform`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Structs données | `CacheInfo` / `NkCacheInfo` | Hiérarchie de cache : `lineSize`, `l1DataSize`, `l1InstructionSize`, `l2Size`, `l3Size`. |
| Structs données | `CPUTopology` / `NkCPUTopology` | `numPhysicalCores`, `numLogicalCores`, `numSockets`, `hasHyperThreading`. |
| Structs données | `SIMDFeatures` / `NkSIMDFeatures` | Drapeaux SIMD x86 (`hasMMX`…`hasAVX512VL`, `hasFMA`, `hasFMA4`) et ARM (`hasNEON`, `hasSVE`, `hasSVE2`). |
| Structs données | `ExtendedFeatures` / `NkExtendedFeatures` | Crypto (`hasAES`, `hasSHA`, `hasRDRAND`, `hasRDSEED`), mémoire (`hasCLFLUSH`…), perf (`hasPOPCNT`, `hasLZCNT`, `hasBMI1/2`, `hasADX`), virtualisation (`hasVMX`, `hasSVM`). |
| Singleton | `CPUFeatures` / `NkCPUFeatures` | Fiche complète : `vendor`, `brand`, `family`, `model`, `stepping`, sous-structs, `baseFrequency`, `maxFrequency`. |
| Constantes | `VENDOR_CAPACITY`, `BRAND_CAPACITY` | Tailles des buffers `vendor` (32) et `brand` (128). |
| Singleton | `CPUFeatures::Get()` | Instance unique, détection paresseuse thread-safe. |
| Singleton | `ToString(buf, size)` · `ToString()` | Formatage dans votre buffer (sûr) · buffer statique interne (non thread-safe). |
| Helpers | `NkGetCPUFeatures()` | Raccourci vers `CPUFeatures::Get()`. |
| Helpers | `NkHasSSE2/AVX/AVX2/AVX512/NEON/FMA()` | Tests SIMD directs (`bool`). |
| Helpers | `NkGetCacheLineSize()` | Taille de ligne de cache. |
| Helpers | `NkGetPhysicalCoreCount()` · `NkGetLogicalCoreCount()` | Comptes de cœurs. |
| Legacy (opt-in) | `HasSSE2/AVX/…`, `GetCacheLineSize/…` | Wrappers `[[deprecated]]`, seulement si `NKENTSEU_ENABLE_LEGACY_PLATFORM_API`. |

### `NkEndianness.h` — namespace `nkentseu::platform` (+ macros globales)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkEndianness` | `NK_LITTLE = 0`, `NK_BIG = 1`, `NK_UNKNOWN = 2`. |
| Détection | `NkGetCompileTimeEndianness()` (constexpr) | Endianness résolu à la compilation. |
| Détection | `NkGetRuntimeEndianness()` | Test runtime (union `0x01020304`), **non** constexpr. |
| Détection | `NkIsLittleEndian()` · `NkIsBigEndian()` (constexpr) | Machine little- / big-endian ? |
| Macros | `NK_LITTLE_ENDIAN`, `NK_BIG_ENDIAN`, `NK_ENDIAN_UNKNOWN` | Drapeaux préprocesseur (0/1). |
| Byte-swap | `ByteSwap16/32/64` + alias `NkByteSwap16/32/64` (constexpr) | Permutation d'octets 16/32/64 bits. |
| Byte-swap | `template ByteSwap(T)` (constexpr) | Générique 2/4/8 octets (incl. `float`/`double`). |
| Ordre réseau | `HostToNetwork16/32/64` + alias `NkHostToNetwork*` | Vers le big-endian réseau. |
| Ordre réseau | `NetworkToHost16/32/64` + alias `NkNetworkToHost*` | Du big-endian réseau vers la machine. |
| Conversions | `ToLittleEndian`/`FromLittleEndian`, `ToBigEndian`/`FromBigEndian` (templates) | Conversion explicite vers un ordre donné. |
| Buffers | `ByteSwapBuffer`, `BufferTo/FromLittleEndian`, `BufferTo/FromBigEndian` | Conversion d'un tableau (compte des **éléments**). |
| Non-aligné | `ReadUnaligned16/32/64` · `WriteUnaligned16/32/64` | Lecture/écriture non-alignée via `memcpy`. |
| Non-aligné + conv. | `ReadLE16/32/64` · `ReadBE16/32/64` · `WriteLE*` · `WriteBE*` | Lire/écrire en convertissant LE ou BE en une fois. |
| Legacy (opt-in) | `Endianness`, `NkToEndianness`, `NkToLegacyEndianness`, `GetCompileTimeEndianness/…` | Seulement si `NKENTSEU_ENABLE_LEGACY_PLATFORM_API`. |
| Macros | `NK_BSWAP16/32/64`, `NK_HTON16/32/64`, `NK_NTOH16/32/64` | Raccourcis macro des fonctions correspondantes. |

### `NkCGXDetect.h` — enums `nkentseu::graphics` (+ macros globales)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkGraphicsApi` | `NK_GFX_API_NONE`=0 … `NK_GFX_API_NVN`=12, `NK_GFX_API_MAX`=13 (sentinelle), `NK_GFX_API_AUTO`=14. |
| Enum | `NkGPUVendor` | Vendor IDs PCI (`NK_NVIDIA = 0x10DE`, `NK_AMD`, `NK_INTEL`, `NK_ARM`, `NK_QUALCOMM`, `NK_APPLE`…). |
| Enum | `NkGPUType` | `NK_UNKNOWN`, `NK_DISCRETE`, `NK_INTEGRATED`, `NK_VIRTUAL`, `NK_SOFTWARE`. |
| Fonctions (déclarées) | `ToString(api/vendor/type)`, `IsAPIAvailable<API>()`, `GetDefaultAPI()`, `GetModernAPI()` | Corps dans un `.cpp` (non constexpr-utilisables depuis le header seul). |
| Disponibilité API | `NKENTSEU_GRAPHICS_<X>_AVAILABLE` | `D3D11/D3D12/OPENGL/VULKAN/METAL/GLES3/WEBGL2/WEBGPU/GNM/NVN` selon plateforme. |
| API défaut/moderne | `NKENTSEU_GRAPHICS_DEFAULT` · `NKENTSEU_GRAPHICS_MODERN` | API par défaut et moderne par plateforme. |
| Conditionnelles API | `NKENTSEU_<X>_ONLY(...)` · `NKENTSEU_NOT_<X>(...)` | Compilation conditionnelle (D3D11/D3D12/VULKAN/METAL/OPENGL/GLES/WEBGL/WEBGPU/GNM/NVN). |
| Compute | `NKENTSEU_COMPUTE_CUDA/OPENCL/SYCL_AVAILABLE`, `NKENTSEU_COMPUTE_AVAILABLE` | Détection CUDA/OpenCL/SYCL. |
| Compute | `NKENTSEU_CUDA_ONLY/NOT_CUDA`, `OPENCL_ONLY/NOT_OPENCL`, `SYCL_ONLY/NOT_SYCL` | Compilation conditionnelle compute. |
| Affichage (Linux) | `NKENTSEU_DISPLAY_WAYLAND` · `NKENTSEU_DISPLAY_XCB` · `NKENTSEU_DISPLAY_XLIB` | Serveur d'affichage détecté. |
| Vendor IDs (macros) | `NKENTSEU_GPU_VENDOR_NVIDIA_ID 0x10DE`, `_AMD_ID`, `_INTEL_ID`… | Doublons macro des valeurs `NkGPUVendor` (deprecated). |
| API active | `NKENTSEU_GFX_NONE/VULKAN/METAL/DIRECTX/OPENGL/SOFTWARE` | IDs numériques d'API. |
| API active | `NKENTSEU_GFX_VERSION_CALC(maj, min)`, `NKENTSEU_GFX_ACTIVE`, `NKENTSEU_GFX_VERSION`, `NKENTSEU_GFX_FORCE` | API sélectionnée + version + override forcé. |
| Debug | `NKENTSEU_CGX_DEBUG` | Émet des `#pragma message` listant les API détectées. |

---

## Référence complète

Chaque élément est repris ici en détail, avec ses usages dans les différents domaines du moteur. Le
trivial est décrit brièvement ; ce qui compte — dispatch SIMD, ordre réseau, sélection de backend
GPU — est traité **à fond**.

### Les sous-structures de `CPUFeatures`

`CacheInfo` décrit la **hiérarchie mémoire** : `lineSize` (la taille d'une ligne de cache, presque
toujours 64 octets) et les tailles L1 données, L1 instructions, L2, L3 (en KB). Cette information
sert surtout côté **performance et structures de données** : aligner un tableau de composants ECS
ou un *ring buffer* audio sur la ligne de cache (`NkGetCacheLineSize()`) évite le *false sharing*
entre threads ; dimensionner un *tile* de rendu ou un bloc de travail de façon à tenir en L2 réduit
les défauts de cache.

`CPUTopology` distingue cœurs **physiques** et **logiques** (l'hyper-threading double les seconds).
C'est la donnée qui pilote le **threading** : le *thread pool* de NKThreading se dimensionne
typiquement sur `numLogicalCores`, mais une charge purement *compute-bound* (transformation de
vertices, mixage audio, simulation physique) gagne parfois à se limiter à `numPhysicalCores`, car
deux threads logiques sur un même cœur se disputent les mêmes unités d'exécution.

`SIMDFeatures` énumère **toutes** les extensions SIMD détectées, drapeau par drapeau : la famille
x86 complète (`hasMMX`, `hasSSE`…`hasSSE42`, `hasAVX`, `hasAVX2`, le bloc AVX-512 `hasAVX512F/DQ/
BW/VL`, `hasFMA`, `hasFMA4`) et la famille ARM (`hasNEON`, `hasSVE`, `hasSVE2`). C'est le cœur du
**dispatch SIMD** décrit plus bas.

`ExtendedFeatures` couvre le reste : la **crypto matérielle** (`hasAES`, `hasSHA` accélèrent un
chiffrement de sauvegarde ou un hachage d'assets ; `hasRDRAND`/`hasRDSEED` fournissent un générateur
aléatoire matériel), des instructions **mémoire** (`hasCLFLUSH`, `hasMOVBE`…), des instructions de
**performance** (`hasPOPCNT` pour compter des bits dans un masque d'entités ou de collisions,
`hasLZCNT`, `hasBMI1`/`hasBMI2` pour la manipulation de bits, `hasADX`), et la **virtualisation**
(`hasVMX` = Intel VT-x, `hasSVM` = AMD-V).

### `CPUFeatures::Get`, `NkGetCPUFeatures` — le point d'entrée

`Get()` est l'unique porte d'accès. Marqué `NKENTSEU_NO_INLINE`, il construit l'instance singleton
**au premier appel** (statique locale → thread-safe en C++11) en lançant les détecteurs internes
(`CPUID` sur x86/x64, lecture des registres ailleurs), puis renvoie une **référence constante** sur
une fiche figée. La détection ne se paie donc qu'**une fois** dans toute la vie du programme. En
pratique on passe par `NkGetCPUFeatures()`, qui n'est qu'un alias de `Get()`.

Les constantes `VENDOR_CAPACITY` (32) et `BRAND_CAPACITY` (128) dimensionnent les buffers fixes
`vendor` et `brand` : le module reste **zéro-STL**, aucune `std::string` n'apparaît, ce qui rend la
fiche copiable et utilisable très tôt au démarrage, avant même que les allocateurs ne soient prêts.

### Le dispatch SIMD — l'usage central

L'objet vit pour permettre de **choisir le bon chemin de code à l'exécution**. On compile plusieurs
variantes d'une même routine (scalaire, SSE2, AVX2) et, au démarrage, on branche vers la meilleure
que la machine supporte réellement. Les domaines concernés sont tous ceux où l'on traite de grandes
quantités de données homogènes :

- **Rendu / mathématiques** — transformer un *batch* de sommets, multiplier des matrices, *culler*
  une nuée de bounding boxes : `NkHasAVX2()` débloque le traitement de 8 flottants par instruction.
- **Physique / collision** — résoudre des contraintes, tester des AABB par paquets, intégrer des
  particules : SIMD multiplie le débit du *broadphase*.
- **Animation** — *skinning* (mélange de matrices osseuses) et *blending* de poses se vectorisent
  naturellement.
- **Audio** — mixer 256 voix, appliquer des filtres et le panning HRTF : le mixage est l'archétype
  du code vectorisable, et le CPU audio se dimensionne souvent sur `numPhysicalCores`.
- **ECS / gameplay** — parcours en masse de composants contigus ; `hasPOPCNT` accélère les masques
  d'archétypes et de visibilité.
- **IO / réseau** — `hasAES`/`hasSHA` accélèrent chiffrement et hachage ; `hasRDRAND` sème un PRNG.

Les **helpers** raccourcissent les tests les plus fréquents : `NkHasSSE2()`, `NkHasAVX()`,
`NkHasAVX2()`, `NkHasAVX512()` (mappé sur `hasAVX512F`), `NkHasNEON()`, `NkHasFMA()`, et côté
threading `NkGetPhysicalCoreCount()` / `NkGetLogicalCoreCount()` / `NkGetCacheLineSize()`.

### `ToString` — diagnostic

Deux surcharges, deux contrats différents. `ToString(buffer, taille)` écrit la fiche dans **votre**
buffer, toujours terminé par `\0` et tronqué si la place manque — c'est la forme **sûre**, à
préférer dès qu'il y a du parallélisme. `ToString()` sans argument renvoie un **buffer statique
interne partagé** : pratique pour cracher une ligne de log au démarrage, mais **non thread-safe**,
et le pointeur ne survit pas à un second appel. Typiquement on logue les capacités CPU une fois au
boot (« AVX2 détecté, 8 cœurs physiques »), ce qui aide énormément au support de bugs
plateforme-spécifiques.

### L'API legacy CPU — à éviter

Sous la macro `NKENTSEU_ENABLE_LEGACY_PLATFORM_API` uniquement, d'anciens noms sans préfixe
(`HasSSE2()`, `HasAVX()`, `GetCacheLineSize()`…) restent disponibles, tous marqués `[[deprecated]]`
et redirigeant vers les `Nk*`. Sans la macro, **ces symboles n'existent pas** : le code neuf
emploie directement les helpers `NkHasX()`.

### `NkEndianness` et la détection — peu de cas, mais à comprendre

L'enum `NkEndianness` (`NK_LITTLE`, `NK_BIG`, `NK_UNKNOWN`) nomme les trois issues possibles.
`NkGetCompileTimeEndianness()` la résout **à la compilation** à partir des macros du compilateur
(`__BYTE_ORDER__` et compagnie, avec des replis sur `_WIN32`, `__x86_64__`, `__aarch64__` → little,
`__BIG_ENDIAN__` → big, sinon unknown). De là découlent `NkIsLittleEndian()` et `NkIsBigEndian()`,
**constexpr** — donc utilisables dans un `if constexpr` qui supprime purement la branche inutile.

`NkGetRuntimeEndianness()` fait le test **à l'exécution** en lisant le premier octet d'une union
initialisée à `0x01020304` ; il n'est **pas** `constexpr` ici. On s'en sert rarement (la version
compile-time suffit presque toujours), surtout en vérification ponctuelle ou sur une cible exotique.
En parallèle, les **macros** `NK_LITTLE_ENDIAN` / `NK_BIG_ENDIAN` (valant 0 ou 1) et
`NK_ENDIAN_UNKNOWN` (défini seulement dans le cas de repli) servent au `#if` du préprocesseur quand
on veut compiler deux chemins distincts.

### `ByteSwap16/32/64` et le template `ByteSwap` — la permutation

Ces fonctions **inversent l'ordre des octets** d'une valeur : `ByteSwap32(0x01020304)` donne
`0x04030201`. Elles sont `constexpr` et s'appuient sur les **intrinsèques** du compilateur
(`_byteswap_ulong` sous MSVC, `__builtin_bswap32` sous GCC/Clang) qui se compilent en une seule
instruction `bswap`/`rev` — avec un repli portable à base de décalages quand l'intrinsèque manque.
Les alias `NkByteSwap16/32/64` existent ; le **template** `ByteSwap(T)` (sans alias `Nk`) couvre
n'importe quel type de 2, 4 ou 8 octets via un `static_assert` sur `sizeof(T)` et un `if constexpr`,
ce qui le rend utilisable sur des `float`/`double` (via union) autant que sur des entiers.

- **IO / sérialisation** — relire un format de fichier d'origine big-endian (PNG, certains formats
  3D, TIFF *big-endian*) sur une machine little-endian.
- **Réseau** — fondation de toute la conversion d'ordre réseau ci-dessous.
- **GPU** — réorganiser des octets de pixels ou des indices selon ce qu'attend un format de
  texture/buffer.

### Ordre réseau et conversions LE/BE — le vrai enjeu de la portabilité

C'est ici que l'endianness cesse d'être un détail. Le **réseau** transporte les entiers en
*big-endian* (« ordre réseau ») par convention universelle ; un fichier binaire portable fixe lui
aussi un ordre. Pour rester correct sur **toutes** les machines, on convertit systématiquement à la
frontière. `HostToNetwork16/32/64` (alias `NkHostToNetwork*`) convertit de l'ordre machine vers le
réseau ; `NetworkToHost16/32/64` (alias `NkNetworkToHost*`) fait l'inverse. Sur une machine
big-endian, ces fonctions sont l'**identité** (aucun coût) ; sur little-endian, elles délèguent à
`ByteSwap*` — et comme tout est `constexpr` et que `NK_LITTLE_ENDIAN`/`NK_BIG_ENDIAN` sont évalués à
la compilation, le compilateur choisit le bon chemin sans test à l'exécution.

Pour des conversions explicites vers un ordre nommé (pas forcément le réseau), les templates
`ToLittleEndian`/`FromLittleEndian` et `ToBigEndian`/`FromBigEndian` font le travail : identité si la
machine est déjà dans le bon ordre, `ByteSwap` sinon. Et pour traiter un **tableau** d'un coup,
`ByteSwapBuffer`, `BufferToLittleEndian`, `BufferToBigEndian` (et leurs `From*`) itèrent sur la
séquence — avec ce **piège majeur** : ils prennent un **compte d'éléments**, pas un nombre d'octets
(`ByteSwapBuffer(samples, 1024)` permute 1024 valeurs, pas 1024 octets).

- **IO / réseau** — sérialiser un en-tête de paquet, un identifiant, une longueur : `NkHostToNetwork32`
  avant l'envoi, `NkNetworkToHost32` à la réception (cf. NKNetwork, NKSerialization binaire `.nkb`).
- **Audio** — convertir d'un coup un bloc de samples PCM d'un fichier WAV/AIFF big-endian
  (`BufferFromBigEndian`).
- **Rendu / GPU** — corriger l'ordre d'octets d'indices ou de blocs de pixels lus d'un format
  imposé.

### Accès non-aligné et lecture/écriture combinées `ReadBE*` / `WriteBE*`

Sérialiser, c'est lire et écrire des entiers à des **offsets arbitraires** d'un buffer d'octets — où
rien ne garantit l'alignement. Déréférencer un `uint32_t*` non aligné est un comportement indéfini
sur certaines architectures (et lent ailleurs) : `ReadUnaligned16/32/64` et `WriteUnaligned16/32/64`
passent donc par `memcpy`, ce que le compilateur optimise en un accès direct quand c'est sûr.

Au-dessus, les combinés font tout en un appel : `ReadLE16/32/64` et `ReadBE16/32/64` lisent à
l'offset **et** convertissent (depuis little- ou big-endian vers l'ordre machine) ; `WriteLE*` /
`WriteBE*` convertissent **et** écrivent. C'est l'outil de prédilection d'un *parser* binaire :
`ReadBE32(packet + 8)` extrait un champ d'un en-tête réseau proprement, sans variable intermédiaire,
sans souci d'alignement, sans appel manuel à `ByteSwap`. On les retrouve naturellement dans la
**lecture de formats** (NKImage, NKFont qui lisent du TTF/OTF *big-endian*), le **réseau** et la
**sérialisation binaire**. Enfin, les macros `NK_BSWAP*`, `NK_HTON*`, `NK_NTOH*` offrent les mêmes
opérations sous forme préprocesseur quand un contexte macro l'exige.

### L'API legacy endianness — à éviter

Comme côté CPU, et seulement sous `NKENTSEU_ENABLE_LEGACY_PLATFORM_API`, un ancien `enum class
Endianness { Little, Big, Unknown }` et ses ponts `NkToEndianness`/`NkToLegacyEndianness` subsistent,
avec des fonctions `[[deprecated]]` (`GetCompileTimeEndianness()`, `IsLittleEndian()`…) renvoyant ce
type legacy. Le code neuf utilise `NkEndianness` et `NkIsLittleEndian()`.

### Les enums GPU : `NkGraphicsApi`, `NkGPUVendor`, `NkGPUType`

`NkGraphicsApi` (sous-jacent `unsigned int`) énumère les API de rendu/compute, valeurs séquentielles
à partir de `NK_GFX_API_NONE = 0` : `OPENGL`, `OPENGLES`, `VULKAN`, `DX11`, `DX12`, `METAL`, `WEBGL`,
`WEBGL2`, `WEBGPU`, `SOFTWARE`, `GNM`, `NVN`, puis la sentinelle `NK_GFX_API_MAX = 13` (= le nombre
d'API réelles) et enfin `NK_GFX_API_AUTO = 14` (demande de sélection automatique). **Piège** : comme
`AUTO` est placée *après* `MAX`, une boucle d'itération `[0, NK_GFX_API_MAX)` ne la couvre pas — ce
qui est voulu, mais surprend.

`NkGPUVendor` (sous-jacent `uint16_t`) reprend les **vendor IDs PCI réels** : `NK_NVIDIA = 0x10DE`,
`NK_AMD = 0x1002`, `NK_INTEL = 0x8086`, `NK_ARM = 0x13B5`, `NK_QUALCOMM = 0x5143`, `NK_APPLE =
0x106B`, `NK_IMGTEC = 0x1010`, `NK_BROADCOM = 0x14E4`, `NK_MICROSOFT = 0x1414`, plus `NK_UNKNOWN`.
Comme ce sont les vrais IDs, on les compare directement à l'ID retourné par le driver. `NkGPUType`
(`uint8_t`) classe le GPU : discret, intégré, virtuel (machine virtuelle), logiciel (WARP, swrast).

Ces enums servent partout où l'on **paramètre le rendu** : choisir le backend RHI, adapter une
heuristique de mémoire ou de batching selon discret/intégré, contourner un bug connu d'un vendeur
précis, afficher l'adaptateur dans un écran d'options. Attention : les enums vivent dans
`nkentseu::graphics` (et **non** `nkentseu::platform::graphics`).

### Fonctions template de `NkCGXDetect` — déclarées, pas définies

`ToString(NkGraphicsApi/NkGPUVendor/NkGPUType)`, `IsAPIAvailable<API>()`, `GetDefaultAPI()` et
`GetModernAPI()` sont **déclarées** `constexpr` dans le header, mais leur **corps est dans un
`.cpp`** : on ne peut donc pas les évaluer en `constexpr` à partir du seul header sans
l'implémentation correspondante. À l'usage, on s'appuie davantage sur les **macros** ci-dessous, qui
elles sont entièrement préprocesseur.

### Macros de disponibilité et de compilation conditionnelle — l'outil de portage

C'est le cœur pratique de `NkCGXDetect`. Selon la plateforme détectée (via `NKENTSEU_<PLATFORM>_
ONLY(...)`), le header définit le bon jeu de `NKENTSEU_GRAPHICS_<X>_AVAILABLE` : Windows ouvre
D3D11/D3D12/OpenGL/Vulkan ; Linux/FreeBSD OpenGL/Vulkan ; macOS Metal/OpenGL (déprécié)/Vulkan (via
MoltenVK) ; iOS Metal/GLES3/Vulkan ; Android GLES3/Vulkan ; Emscripten WebGL2/WebGPU ; et les
consoles leurs API propres (GNM sur PlayStation, NVN sur Switch, D3D12 sur Xbox).

À partir de là, **chaque API** dispose d'une paire `NKENTSEU_<X>_ONLY(...)` / `NKENTSEU_NOT_<X>(...)`
(pour `D3D11`, `D3D12`, `VULKAN`, `METAL`, `OPENGL`, `GLES`, `WEBGL`, `WEBGPU`, `GNM`, `NVN`) : la
première développe son contenu si l'API est disponible et l'efface sinon, la seconde fait l'inverse.
C'est ainsi qu'on **n'embarque le code d'un backend que là où il a un sens** — exactement le besoin
de la couche **GPU/RHI** qui doit compiler proprement sur huit plateformes sans `#ifdef` éparpillés.
Deux macros complémentaires, `NKENTSEU_GRAPHICS_DEFAULT` et `NKENTSEU_GRAPHICS_MODERN`, désignent
l'API conseillée par défaut et l'API moderne de la plateforme (Windows → DX11/DX12, Android →
GLES/Vulkan, macOS → Metal/Metal…). **Attention** : ces deux macros écrivent leur valeur via le
chemin `nkentseu::platform::graphics::NkGraphicsApi::…`, **incohérent** avec le namespace réel
`nkentseu::graphics` — un détail à connaître si on s'en sert tel quel.

### Détection compute, affichage et vendor IDs macro

Le même mécanisme s'applique au **calcul GPGPU** : `NKENTSEU_COMPUTE_CUDA_AVAILABLE` (si `__CUDACC__`),
`_OPENCL_AVAILABLE` (si `__OPENCL_VERSION__`), `_SYCL_AVAILABLE`, chacun activant aussi le parapluie
`NKENTSEU_COMPUTE_AVAILABLE`, avec leurs paires `NKENTSEU_CUDA_ONLY/NOT_CUDA`, `OPENCL_*`, `SYCL_*`.
Sur **Linux**, le header devine le serveur d'affichage — `NKENTSEU_DISPLAY_WAYLAND`,
`NKENTSEU_DISPLAY_XCB` ou `NKENTSEU_DISPLAY_XLIB` — selon les variables d'environnement et les
défines, ce qui oriente le choix du backend fenêtre (cf. NKWindow). Enfin, des constantes macro
`NKENTSEU_GPU_VENDOR_<NOM>_ID` (`NVIDIA_ID 0x10DE`, `AMD_ID 0x1002`…) doublonnent les valeurs de
`NkGPUVendor` ; elles sont marquées dépréciées, l'enum étant préférable.

### Configuration de l'API active : `NKENTSEU_GFX_ACTIVE`, `NKENTSEU_GFX_FORCE`

À distinguer absolument des `*_AVAILABLE`. Ceux-ci listent **tout ce que la plateforme permet**
(plusieurs API à la fois) ; le système `NKENTSEU_GFX_ACTIVE` sélectionne **une seule** API effective
sous forme d'ID numérique (`NKENTSEU_GFX_NONE`=0, `_VULKAN`=1, `_METAL`=2, `_DIRECTX`=3, `_OPENGL`=4,
`_SOFTWARE`=5), avec sa version dans `NKENTSEU_GFX_VERSION` (construite par
`NKENTSEU_GFX_VERSION_CALC(major, minor)` = `((major)<<16)|(minor&0xFFFF)`). La détection combine la
plateforme et la présence d'en-têtes via `__has_include` (Metal 3 sur macOS, DirectX 12/11 sur
Windows, Vulkan si `<vulkan/vulkan.h>` est là, sinon OpenGL, sinon Software). Deux leviers
importants : prédéfinir `NKENTSEU_GFX_FORCE` **court-circuite toute la détection** (utile pour forcer
un backend en test), et une garde finale retombe sur **Software 1.0** si rien n'a été choisi. Noter
que `NKENTSEU_GFX_DIRECTX` ne distingue pas 11 de 12 — c'est la version qui le précise. Enfin,
définir `NKENTSEU_CGX_DEBUG` fait émettre des `#pragma message` listant les API disponibles et celle
retenue, très utile pour diagnostiquer une compilation qui choisit le mauvais backend.

---

### Exemple

```cpp
#include "NKPlatform/NkCPUFeatures.h"
#include "NKPlatform/NkEndianness.h"
#include "NKPlatform/NkCGXDetect.h"
using namespace nkentseu::platform;

// 1) Dispatch SIMD selon le CPU réel, détecté une seule fois.
const NkCPUFeatures& cpu = NkGetCPUFeatures();
if      (cpu.simd.hasAVX2) TransformAVX2(mesh);
else if (cpu.simd.hasSSE2) TransformSSE2(mesh);
else                       TransformScalar(mesh);

int workers = cpu.topology.numPhysicalCores;     // dimensionner le thread pool compute

// 2) Sérialiser un en-tête réseau en ordre réseau (big-endian), sans souci d'alignement.
uint8_t packet[16];
WriteBE32(packet + 0, 0xCAFEBABE);               // magic
WriteBE16(packet + 4, (uint16_t)payloadSize);    // longueur
// ... à la réception :
uint32_t magic = ReadBE32(packet + 0);
uint32_t len   = NkNetworkToHost16(ReadUnaligned16(packet + 4));

// 3) Ne compiler le backend Vulkan QUE là où il est disponible.
NKENTSEU_VULKAN_ONLY( InitVulkanBackend(); )
NKENTSEU_NOT_VULKAN(  InitFallbackBackend(); )
```

---

[← Index NKPlatform](README.md) · [Récap NKPlatform](../NKPlatform.md) · [Détection OS & architecture →](OS-Architecture.md)
