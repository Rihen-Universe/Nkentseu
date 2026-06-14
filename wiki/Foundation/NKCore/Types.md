# Les types primitifs

> Couche **Foundation** · NKCore · Le socle de typage du moteur : des alias de taille **fixe et
> explicite** pour les entiers (`nk_uint32`, `nk_int8`…), les flottants (`nk_float32`/`nk_float64`),
> les tailles et adresses (`nk_size`/`usize`, `nk_uptr`), les octets (`nk_byte`, `Byte`), les
> caractères Unicode et les booléens — plus les constantes de limites, les sentinelles et la
> conversion d'endianness.

Avant même de parler de conteneurs, de maths ou de rendu, il faut s'entendre sur une chose toute
bête : comment nommer un entier de 32 bits, un octet, une taille ? En C++ « brut », les types
fondamentaux (`int`, `long`, `unsigned`…) n'ont **pas de taille garantie** — un `int` peut faire
16, 32 ou 64 bits selon la plateforme et le compilateur. Pour un moteur cross-plateforme qui
manipule des formats binaires — fichiers, paquets réseau, buffers GPU — c'est inacceptable : on a
besoin de tailles **explicites et fixes**, sinon une structure écrite sur une machine devient
illisible sur une autre.

NKCore répond par un jeu d'alias stables, le socle sur lequel **tout le reste** est typé. C'est
l'équivalent maison de `<cstdint>`, mais plus complet : il garantit aussi la précision flottante,
distingue les encodages de caractères, sécurise les opérations bitwise (le `struct Byte`), propose
des entiers 128 bits optionnels, des constantes de limites et de sentinelles, et une conversion
d'endianness portable. Ce n'est **pas** une simple recopie de la bibliothèque standard : c'est une
**hiérarchie de noms** pensée pour le moteur, qui s'intègre à NKPlatform pour détecter
l'architecture et le compilateur.

- **Namespace** : `nkentseu` (racine), avec deux sous-espaces `nkentseu::core` et `nkentseu::math`
- **Header** : `#include "NKCore/NkTypes.h"`

---

## Des entiers à la taille connue

```cpp
nk_uint32 color = 0xFF8240FF;   // exactement 32 bits, partout
nk_int8   delta = -3;           // 8 bits signés
uint16    port  = 8080;         // primitive sans préfixe (équivalente à nk_uint16)
```

Pour chaque taille — 8, 16, 32, 64, et même 128 bits si le compilateur le supporte — il existe une
version signée (`nk_int8`…`nk_int128`) et non signée (`nk_uint8`…`nk_uint128`). Le contrat est
simple : ce que le nom annonce est ce que vous obtenez, sur toutes les plateformes. En coulisse,
NKCore choisit la bonne définition selon le compilateur (les types `__int32` de MSVC, les `int` /
`short` / `long long` de GCC/Clang), mais cela vous est invisible.

Le piège qu'il faut comprendre, c'est qu'il existe **trois familles de noms pour les mêmes types** :
les alias préfixés `nk_*` (la **nomenclature officielle**, à utiliser dans l'API publique), les
alias **sans préfixe** (`uint32`, `int8`… — les primitives de base, acceptables), et les alias
**courts** (`u32`, `i8`, `f32`… — réservés au code **interne** du framework, explicitement
déconseillés en API publique). Les trois désignent strictement les mêmes types ; la convention
n'est qu'une question de lisibilité et de portée.

> **En résumé.** Un alias par taille et par signe, garanti identique partout. Préférez les `nk_*`
> dans tout code public ; les noms courts (`u32`, `i32`) sont pour l'interne du moteur uniquement.

---

## Tailles, adresses et le découpage CPU/GPU

Manipuler des tailles et des pointeurs demande des types dédiés, parce que leur largeur **dépend de
l'architecture** (32 ou 64 bits). `nk_size` (alias court `usize`) est le type des tailles et des
indices — c'est lui qu'on emploie pour la longueur d'un conteneur. Sa version signée est `isize`
(= `nk_ptrdiff`). Et quand on a besoin de traiter une **adresse comme un entier** — pour de
l'arithmétique d'alignement, par exemple — `nk_uintptr` / `nk_uptr` est un entier non signé assez
large pour contenir un pointeur (l'équivalent de `uintptr_t`) :

```cpp
nk_usize count = vector.Size();                       // une taille
nk_uptr  addr  = reinterpret_cast<nk_uptr>(ptr);
bool aligned   = (addr % 16) == 0;                    // alignement sur 16 octets
```

S'ajoutent `nk_ptrdiff` (différence de pointeurs), `nk_ptr`/`nk_voidptr` (`void*`),
`nk_constvoidptr`, et les pointeurs d'octets `nk_byteptr`/`nk_constbyteptr`. Deux helpers de cast
accompagnent ces pointeurs : `NkSafeCast<T>(VoidPtr)` (un `static_cast` typé) et
`SafeConstCast<T>(ConstVoidPtr)` (qui retire le `const`) — ils n'ajoutent **aucune** vérification
runtime, ce sont des raccourcis lisibles, pas des `dynamic_cast`.

NKCore va plus loin que la STL avec des types **conscients du fossé CPU/GPU** : `usize_cpu` (une
taille alignée naturellement pour le CPU) et `usize_gpu` (toujours aligné **16 octets**, pour le
SIMD et les transferts DMA vers la carte graphique). Attention à un détail surprenant : malgré son
préfixe `u`, **`usize_gpu` est signé** (`int64`) — incohérence à garder en tête. De même,
`intptr`/`uintptr` adaptent leur taille à l'architecture.

> **En résumé.** `nk_size`/`usize` pour les tailles et indices, `isize` pour sa version signée,
> `nk_uptr` pour traiter une adresse comme un entier. `usize_gpu` est aligné 16 octets (mais signé,
> piège).

---

## Octets, flottants, caractères et booléens

Pour les données brutes, `nk_byte` (octet non signé, = `uint8`) et son pendant signé `nk_sbyte`. Au
delà du simple alias, NKCore fournit un vrai **`struct Byte`** : un wrapper `constexpr` type-safe
qui rend les opérations bitwise explicites et sûres (`|`, `&`, `^`, `~`, décalages). On y reviendra
dans la référence.

Les flottants suivent la même logique de taille explicite : `nk_float32` (alias `float32`),
`nk_float64` (alias `float64`), plus `nk_float80` pour la précision étendue là où elle existe
(80 bits sur x86, souvent ramenée à 64 sur ARM).

Côté texte, le moteur distingue clairement les **encodages** : `nk_char` est le caractère natif,
tandis que `nk_char8`, `nk_char16` et `nk_char32` correspondent à l'UTF-8, 16 et 32 — une
distinction qui évite bien des confusions quand on traite de l'Unicode. `nk_wchar` couvre le
caractère large (`wchar_t` sur Windows, ramené à un `char32` sous POSIX où `wchar_t` est instable).

Enfin les booléens. `nk_bool` est le booléen ordinaire (`bool` C++). Mais vous croiserez aussi
`nk_boolean` (8 bits), `nk_bool8` et `nk_bool32`, et on peut se demander pourquoi un booléen aurait
besoin d'une taille fixe. La réponse : dès qu'un booléen vit dans une **structure binaire** — un
fichier, un paquet réseau, un buffer GPU — sa taille en octets compte (le `bool` C++ n'a pas de
taille standardisée). `nk_bool8`/`nk_bool32` garantissent cette taille là où un format l'impose ;
`bool32` sert aussi à éviter le *padding* dans des structures alignées. Pour de la logique
ordinaire, `nk_bool` suffit.

> **En résumé.** `nk_byte`/`Byte` pour l'octet brut, `nk_float32/64/80` pour les flottants,
> `nk_char8/16/32` pour distinguer les encodages Unicode, et `nk_bool8/32` **uniquement** quand la
> taille binaire du booléen est imposée par un format.

---

## Aperçu de l'API

La liste de **tous** les éléments publics, en un coup d'œil. Chacun est détaillé dans la
« Référence complète » qui suit. Les éléments vivent dans le namespace `nkentseu`, sauf indication
de sous-namespace (`core::`, `math::`).

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Détection | `NKENTSEU_HAS_STDINT` | `1` si `<stdint.h>` est disponible (C99+). |
| Détection | `NKENTSEU_INT128_AVAILABLE` | `1` si entiers 128 bits natifs disponibles. |
| Utilitaire | `BIT(x)` | Masque à un seul bit, `(1u << x)`. |
| Entiers (primitives) | `int8`/`uint8` … `int64`/`uint64`, `uintl32` | Entiers fixes signés/non signés, sans préfixe. |
| Octet | `struct Byte` | Wrapper bitwise type-safe `constexpr` sur `uint8`. |
| Octet | `Byte::Value` (`_0`…`_f`), `Byte::from<T>`, opérateurs `\| & ^ ~ << >>` | Constantes hexa, conversion sûre, bitwise. |
| Entiers 128 | `int128` / `uint128` | Natif (`__int128`) ou émulé (`{low, high}`). |
| Flottants | `float32` `float64` `float80` | `float` / `double` / `long double`. |
| Caractères | `Char`, `char8` `char16` `char32`, `wchar` | Caractère natif + encodages Unicode + large. |
| Booléens | `Bool`, `Boolean`, `bool32`, `True` / `False` | bool C++, 8 bits, 32 bits, constantes. |
| Pointeurs | `PTR` `VoidPtr` `BytePtr` `ConstVoidPtr` `ConstBytePtr`, `UPTR` | Alias de pointeurs bruts. |
| Tailles | `usize`, `ptrdiff`, `usize_cpu`, `usize_gpu`, `intptr`/`uintptr` | Tailles/différences + variantes CPU/GPU. |
| Casts | `NkSafeCast<T>(VoidPtr)`, `SafeConstCast<T>(ConstVoidPtr)` | `static_cast` typé / retrait de `const`. |
| Alias `nk_` | `nk_int8`…`nk_uint128`, `nk_float32/64/80`, `nk_size`, `nk_ptrdiff`, `nk_intptr`/`nk_uintptr` | Nomenclature **officielle** (entiers/flottants/tailles). |
| Alias `nk_` | `nk_bool` `nk_boolean` `nk_bool8` `nk_bool32` | Booléens préfixés. |
| Alias `nk_` | `nk_char` `nk_char8/16/32` `nk_uchar` `nk_wchar`, `nk_byte` `nk_sbyte` | Caractères et octets préfixés. |
| Alias `nk_` | `nk_ptr` `nk_voidptr` `nk_byteptr` `nk_constvoidptr` `nk_constbyteptr` `nk_uptr` `nk_usize` | Pointeurs/tailles préfixés. |
| Alias courts | `i8`…`i128`, `u8`…`u128`, `f32/64/80`, `usize`, `isize` | Usage **interne** (déconseillés en API publique). |
| Limites entières | `NKENTSEU_INT8_MIN/MAX` … `NKENTSEU_UINT64_MAX` | Bornes min/max des entiers. |
| Limites flottantes | `NKENTSEU_FLOAT32_MIN/MAX`, `NKENTSEU_FLOAT64_MIN/MAX`, `NKENTSEU_MAX_FLOAT80`/`MIN_FLOAT80` | Bornes IEEE 754 (+ via `<cfloat>`). |
| Limites taille | `NKENTSEU_SIZE_MAX`, `NKENTSEU_USIZE_MAX` | Valeur max de `nk_size`. |
| Limites (compat) | `NKENTSEU_MAX_UINT8`…`NKENTSEU_MIN_INT64`, `NKENTSEU_MAX_FLOAT32`… | Anciens noms hexa (préférer `*_MIN/MAX`). |
| Sentinelles | `NK_NULL`, `NKENTSEU_INVALID_SIZE`, `NKENTSEU_INVALID_INDEX` | Pointeur nul, taille/index invalide. |
| Sentinelles | `NKENTSEU_INVALID_ID`, `…_ID_UINT8/16/32/64` | Identifiants invalides par largeur. |
| Endianness | `NkToBigEndian16/32/64`, `NkToLittleEndian16/32/64` | Byte-swap portable (no-op sur big-endian). |
| `core::` Hash | `NkHashValue`, `NkHash32`, `NkHash64` | Types de valeurs de hachage. |
| `core::` Handle | `NkHandle`, `INVALID_HANDLE` | Poignée opaque (taille pointeur). |
| `core::` ID | `NkID32`, `NkID64`, `INVALID_ID32`, `INVALID_ID64` | Identifiants typés + sentinelles. |
| `math::` | `NkReal`, `NkRadians`, `NkDegrees` | Scalaire mathématique + unités d'angle. |

---

## Référence complète

Chaque élément est repris ici en détail. Les triviaux (alias directs) sont décrits brièvement ;
les éléments porteurs de comportement — `Byte`, les tailles CPU/GPU, l'endianness, les sentinelles —
le sont **à fond**, avec leurs usages à travers les domaines du moteur (rendu, ECS, physique,
animation, gameplay/IA, audio, UI/2D, IO, GPU, threading).

### Macros de détection : `NKENTSEU_HAS_STDINT`, `NKENTSEU_INT128_AVAILABLE`

`NKENTSEU_HAS_STDINT` vaut `1` quand `__STDC_VERSION__ >= 199901L` (C99 et au-delà), auquel cas
`<stdint.h>` est inclus et les `UINTxx_MAX` deviennent disponibles. C'est le commutateur dont
dépendent plusieurs sentinelles plus bas — en environnement pré-C99 sans stdint, celles-ci ne sont
pas accessibles.

`NKENTSEU_INT128_AVAILABLE` vaut `1` lorsque le compilateur expose `__SIZEOF_INT128__` **et** qu'on
n'est pas sur PSP/NDS. Il commande à la fois la définition de `int128`/`uint128` (natif vs émulé) et
l'existence des alias courts `i128`/`u128`. À tester avant tout code qui suppose une arithmétique
128 bits matérielle (hachage large, *fixed-point* haute précision en physique, accumulateurs audio).

### `BIT(x)` — masque à un bit

`BIT(x)` produit `(1u << x)`, un `unsigned int` n'ayant que le bit de position `x` allumé (0–31 pour
un `uint32`). C'est l'outil de base des **drapeaux** que l'on croise partout :

- **Rendu / GPU** : composer des masques d'état (`COLOR | DEPTH`), des bits de *dirty flags* de
  pipeline, des aspects d'image.
- **ECS** : signatures d'archétypes (un bit par type de composant) pour matcher rapidement les
  entités d'un système.
- **Événements / IO** : combinaisons de modificateurs clavier, de boutons de souris, de flags
  d'ouverture de fichier.
- **Collision** : couches de collision et masques de filtrage (un objet teste `layerA & maskB`).

Étant une **macro**, `x` est substitué tel quel : éviter d'y mettre une expression à effet de bord.

### Entiers fixes : `int8`…`uint64`, `uintl32`

Les primitives sans préfixe (`int8`/`uint8`, `int16`/`uint16`, `int32`/`uint32`, `int64`/`uint64`)
sont les briques de tout le reste. NKCore les définit sur la bonne base selon le compilateur (les
intrinsèques `__int32` de MSVC, sinon les types fondamentaux C++), de sorte que la **largeur est
garantie** : un `uint8` est toujours 0–255, un `uint32` toujours 0–4294967295. `uintl32` est un cas
à part (`unsigned long int`) conservé pour compatibilité — la note du header recommande explicitement
de lui préférer `uint32` pour la portabilité.

Tous les domaines reposent dessus : couleurs `RGBA8` packées en `uint32` (rendu/2D), indices et
identifiants ECS, échantillons audio 16/32 bits, octets de fichiers et de paquets (IO/réseau),
tailles de buffers GPU.

### `struct Byte` — l'octet bitwise sécurisé

`Byte` est un wrapper **`constexpr` et type-safe** autour d'un `uint8`, conçu pour rendre les
manipulations de bits explicites et empêcher les conversions implicites accidentelles. Tout y est
`NKENTSEU_FORCE_INLINE constexpr noexcept` : aucune surcharge à l'exécution, le compilateur réduit
l'objet à un simple `uint8`.

- **Construction** : `Byte()` (valeur 0) ou `Byte(uint8 v)` — ce dernier est **`explicit`**, donc
  pas de conversion silencieuse depuis un `uint8`. La fabrique `Byte::from<T>(v)` convertit depuis
  **n'importe quel** type entier, en tronquant silencieusement les bits de poids fort si `T` est
  plus large que 8 bits.
- **Constantes** : l'enum membre `Value` expose `_0`…`_f` (les valeurs hexadécimales `0x0`…`0xF`),
  pratiques pour composer des nibbles lisibles.
- **Opérateurs bitwise** : `|` (OU), `&` (ET), `^` (OU exclusif), `~` (NON), plus les décalages
  templatés `<<` et `>>` — tous renvoient un `Byte`. La conversion **vers** un autre type passe par
  un `operator T()` lui aussi **`explicit`** : il faut écrire `static_cast<uint8>(flags)`.
- **Membre** : `value` (le `uint8` brut sous-jacent).

L'idiome typique : `Byte flags = Byte::from(0xFF); flags = flags & Byte::from(0x0F);` puis
`uint8 v = static_cast<uint8>(flags);`. Ce type sert dès qu'on veut un drapeau d'octet sans risquer
les promotions entières silencieuses du C++ :

- **IO / sérialisation** : lecture/écriture d'en-têtes binaires, champs de bits d'un format de
  fichier, masques de permission.
- **Rendu / GPU** : drapeaux d'un canal, *stencil* 8 bits, attributs packés.
- **Réseau** : octets de contrôle d'un protocole, flags de paquet.

Piège : la double présence de `explicit` (ctor `uint8` **et** opérateur de conversion) est
**voulue** — elle force des conversions explicites dans les deux sens ; et `from<T>()` tronque sans
prévenir au-delà de 8 bits.

### Entiers 128 bits : `int128`, `uint128`

Lorsque `NKENTSEU_INT128_AVAILABLE` vaut `1`, `int128`/`uint128` aliasent directement les types
natifs `__int128_t`/`__uint128_t` du compilateur, avec arithmétique matérielle complète. Sinon,
NKCore fournit des **structures d'émulation** minimales — `struct int128 { int64 low; int64 high; }`
et son équivalent non signé — qui stockent les deux moitiés mais **ne fournissent aucune
arithmétique** : à vous d'implémenter les opérations si vous devez supporter une plateforme dépourvue
de 128 bits natif. Usages : hachages larges (UUID, *content addressing* d'assets), accumulateurs de
précision étendue en physique ou en audio, *fixed-point* 64.64.

### Flottants : `float32`, `float64`, `float80`

`float32` (`float`) est la simple précision IEEE 754, monnaie courante du temps réel : positions,
normales, UV, couleurs HDR, échantillons audio, poids d'animation — partout où la vitesse prime sur
la précision absolue. `float64` (`double`) est la double précision, qu'on réserve aux calculs
sensibles à l'accumulation d'erreur : grandes coordonnées de monde, intégration physique longue,
temps absolu, géométrie CAO. `float80` (`long double`) offre la précision étendue **là où le
matériel l'a** (80 bits sur x86 ; souvent rabattu à 64 sur ARM) — à n'utiliser que ponctuellement,
sa portabilité étant variable.

### Caractères : `Char`, `char8`/`char16`/`char32`, `wchar`

`Char` est le caractère natif (`char`). Les trois suivants matérialisent les **unités de code**
Unicode : sous C++20, `char8`/`char16`/`char32` aliasent les vrais `char8_t`/`char16_t`/`char32_t`,
sinon ils retombent sur `uint8`/`uint16`/`uint32`. Cette séparation évite la confusion classique
entre un octet UTF-8 et un point de code. `wchar` enfin couvre le caractère large — `wchar_t` sur
Windows (où il est stable et 16 bits), mais ramené à `char32` sous POSIX (où `wchar_t` varie selon
la plateforme). Domaines : moteur de texte et rendu de police (UI/2D, NKFont), chemins de fichiers
(IO), noms localisés en gameplay.

### Booléens : `Bool`, `Boolean`, `bool32`, `True`/`False`

`Bool` est le `bool` C++ ordinaire — celui de la logique de tous les jours. `Boolean` est un booléen
**8 bits** (`uint8`) pour l'interop C et les structures binaires, où la taille du `bool` C++ n'est
pas standardisée. `bool32` (un `int32`) sert quand un format ou une structure GPU exige un booléen
sur 4 octets, ou pour éviter le *padding* d'alignement. Les constantes `True`/`False` (de type
`Boolean`) accompagnent ces booléens dimensionnés. Usages : flags dans des *constant buffers* GPU
(souvent 4 octets), champs de fichiers `.nkproj`/`.nkscene`, drapeaux de paquets réseau.

### Pointeurs et casts : `PTR`, `VoidPtr`, `BytePtr`…, `NkSafeCast`, `SafeConstCast`

`PTR`/`VoidPtr` (`void*`), `ConstVoidPtr` (`const void*`), `BytePtr` (`uint8*`),
`ConstBytePtr` (`const uint8*`) et `UPTR` (entier large contenant un pointeur) donnent des noms
lisibles aux pointeurs bruts qui circulent entre couches : mémoire (NKMemory), IO (lectures
d'octets), GPU (mappings de buffers). Les deux helpers `NkSafeCast<T>(VoidPtr)` (un `static_cast`
typé) et `SafeConstCast<T>(ConstVoidPtr)` (un `const_cast` après `static_cast`) ne font **aucune**
vérification runtime — leur « sécurité » est purement une question de lisibilité et d'intention, pas
un `dynamic_cast`. À employer pour caster un buffer générique vers son type concret (un
`VoidPtr` de NKMemory vers un `NkVertex*`, par exemple).

### Tailles machine : `usize`, `ptrdiff`, `usize_cpu`/`usize_gpu`, `intptr`/`uintptr`

`usize` est le type des tailles et indices, large d'un mot machine (`uint64` en 64 bits) ; `ptrdiff`
est la différence signée de deux pointeurs (`int64` ou `int32` selon l'architecture). Au-delà des
tailles génériques, NKCore distingue le besoin **CPU** du besoin **GPU** :

- `usize_cpu` — une taille alignée pour le CPU (8 octets en 64 bits, 4 en 32 bits) ; le défaut pour
  les structures côté processeur.
- `usize_gpu` — toujours aligné **16 octets** (la granularité des transferts SIMD/DMA et des
  *constant buffers*) : à utiliser pour dimensionner ce qui part vers la carte graphique. **Piège :
  ce type est signé** (`int64`) malgré le préfixe `u`.
- `intptr`/`uintptr` — entiers de la largeur d'un pointeur, pour l'arithmétique d'adresses
  (alignement, calcul d'offsets).

Piège supplémentaire : `usize` est **déclaré deux fois** dans le header (une fois `= uint64`
directement, une fois via `= nk_size`) ; les deux résolvent vers `uint64`, c'est une redéfinition
d'alias identique, sans conséquence — mais c'est bon à savoir si on lit le source.

### Les alias `nk_*` — la nomenclature officielle

C'est la **famille canonique**, celle à employer dans toute API publique. Elle couvre l'intégralité
du jeu : entiers signés (`nk_int8`…`nk_int128`) et non signés (`nk_uint8`…`nk_uint128`) ; flottants
(`nk_float32`, `nk_float64`, `nk_float80`) ; tailles machine (`nk_size` = `usize`, `nk_ptrdiff`,
`nk_intptr`, `nk_uintptr`) ; booléens (`nk_bool` = `Bool`, `nk_boolean`, `nk_bool8`, `nk_bool32`) ;
caractères (`nk_char`, `nk_char8/16/32`, `nk_uchar`, `nk_wchar`) ; octets (`nk_byte` = `uint8`,
`nk_sbyte` = `int8`) ; pointeurs (`nk_ptr`, `nk_voidptr`, `nk_byteptr`, `nk_constvoidptr`,
`nk_constbyteptr`, `nk_uptr`, `nk_usize`). Ce ne sont que des **renommages** des types ci-dessus :
même représentation, intention explicite. C'est la forme que vous verrez dans les signatures
publiques de tous les modules.

### Les alias courts — usage interne uniquement

`i8`…`i64` (et `i128` si dispo), `u8`…`u64` (et `u128`), `f32`/`f64`/`f80`, plus `usize` et `isize`
(= `ptrdiff`, signé). Ils existent pour la **densité de lecture** dans le code interne du moteur
(boucles de maths, kernels SIMD, code de bas niveau), mais le header les déconseille **explicitement
en API publique** : un `u32` dans une signature exposée est à proscrire au profit de `nk_uint32`.
À noter : `isize` (taille signée) n'apparaît qu'ici, il n'a pas de variante `nk_*`.

### Constantes de limites

Les bornes d'entiers — `NKENTSEU_INT8_MIN`/`MAX`, `NKENTSEU_UINT8_MAX`, et leurs équivalents 16, 32,
64 bits jusqu'à `NKENTSEU_UINT64_MAX` — sont des macros castées vers le type `nk_*` correspondant.
Les bornes flottantes (`NKENTSEU_FLOAT32_MIN`/`MAX`, `NKENTSEU_FLOAT64_MIN`/`MAX`) donnent les
littéraux IEEE 754 ; une seconde voie passe par `<cfloat>` avec `NKENTSEU_MAX_FLOAT32`/`MIN`,
`…_FLOAT64`, `…_FLOAT80` (`FLT_MAX`, `DBL_MAX`, `LDBL_MAX`…). `NKENTSEU_SIZE_MAX` (alias
`NKENTSEU_USIZE_MAX`) donne la valeur max de `nk_size` selon l'architecture. Un **jeu de
compatibilité** d'anciens noms hexa (`NKENTSEU_MAX_UINT8` = `0xFFU`, `NKENTSEU_MAX_INT32`, etc.)
subsiste, mais le header recommande de lui préférer les formes `*_MIN`/`*_MAX`. Usages : saturation
de couleurs (clamp à `UINT8_MAX`), bornes d'un quantizeur audio, valeurs de garde, tests de
débordement.

### Sentinelles : `NK_NULL`, valeurs invalides

`NK_NULL` est `nullptr` (ou `0` en pré-C++11). Les autres sentinelles encodent l'**absence** ou
l'**invalidité** :

- `NKENTSEU_INVALID_SIZE` et `NKENTSEU_INVALID_INDEX` valent tous deux `static_cast<nk_size>(-1)`
  (la valeur max non signée) — c'est le « pas trouvé » classique d'une recherche, le `npos` du
  moteur.
- `NKENTSEU_INVALID_ID` / `…_ID_UINT64` = `UINT64_MAX`, et leurs déclinaisons
  `…_ID_UINT32`/`UINT16`/`UINT8` = la valeur max de la largeur correspondante : la convention pour un
  identifiant **non assigné** (entité ECS, handle de ressource, slot de pool).

Ces macros s'appuient sur les `UINTxx_MAX` de `<stdint.h>` : elles dépendent donc de
`NKENTSEU_HAS_STDINT` (indisponibles en C89/pré-C99 sans stdint). On les croise en ECS (entité
détruite), en gestion de ressources (texture/buffer GPU non chargé), dans les pools d'objets (slot
libre), dans les structures de recherche (index non trouvé).

### Conversion d'endianness : `NkToBigEndian*`, `NkToLittleEndian*`

Six macros — `NkToBigEndian16/32/64` et `NkToLittleEndian16/32/64` — assurent la portabilité des
formats binaires entre machines *little* et *big endian*. Sur une machine **little-endian** (le cas
courant x86/ARM moderne), elles effectuent un **byte-swap** via les intrinsèques du compilateur
(`_byteswap_ushort`/`ulong`/`uint64` sous MSVC, `__builtin_bswap16/32/64` ailleurs) ; sur une
machine **big-endian**, ce sont des **no-op** qui rendent l'argument tel quel. Les versions
`ToLittleEndian` sont symétriques des `ToBigEndian` (le byte-swap est sa propre inverse). Usages :
écriture/lecture de formats imposant un ordre d'octets (réseau = big-endian « network byte order »,
formats d'image/audio/police qui fixent l'ordre), sérialisation portable inter-plateformes. Piège :
ce sont des **macros**, donc l'argument est évalué tel quel — n'y mettez pas d'expression à effet de
bord.

### Sous-namespace `nkentseu::core` — hash, handle, ID

Cet espace regroupe des types « vocabulaire » de plus haut niveau, bâtis sur les primitives :

- **Hash** : `NkHashValue` (un mot machine — `uint64` en 64 bits, `uint32` en 32 bits), plus les
  variantes de largeur fixe `NkHash32` et `NkHash64`. Pour les tables de hachage, le *content
  addressing* d'assets, les clés de cache de pipeline.
- **Handle** : `NkHandle` (un `uintptr`, taille pointeur) avec sa sentinelle
  `constexpr NkHandle INVALID_HANDLE = 0` — une poignée opaque vers une ressource, où 0 signifie
  « invalide ». Idéal pour exposer une ressource GPU/OS sans révéler son type interne.
- **ID** : `NkID32` (`uint32`) et `NkID64` (`uint64`) avec leurs sentinelles typées
  `constexpr INVALID_ID32 = 0xFFFFFFFF` et `INVALID_ID64 = 0xFFFFFFFFFFFFFFFF`. À distinguer des
  macros `NKENTSEU_INVALID_ID*` du namespace racine : ici ce sont des `constexpr` **typés** (donc
  sûrs en surcharge et en comparaison). On les emploie pour les identifiants d'entités, de
  composants, de sons actifs, d'éléments d'UI.

### Sous-namespace `nkentseu::math` — `NkReal`, `NkRadians`, `NkDegrees`

`NkReal` est le **scalaire mathématique** du moteur : `float64` si la macro `NKENTSEU_MATH_USE_DOUBLE`
est définie **avant** l'inclusion, sinon `float32`. C'est la bascule globale de précision de toute la
couche maths — la définir change d'un coup le type des vecteurs, matrices et quaternions.
`NkRadians` et `NkDegrees` sont deux alias de `NkReal` dont le seul rôle est **documentaire** :
signaler dans une signature qu'un angle est attendu en radians ou en degrés (la conversion étant
`radians = degrés × π/180`). Attention : comme ils partagent le même type sous-jacent, le compilateur
**ne distingue pas** un `NkRadians` d'un `NkDegrees` — aucune sécurité de type, c'est purement une
convention de lecture. Usages : toutes les API d'angle en animation (interpolation de rotations),
caméra (FOV en degrés vs calculs en radians), gameplay (orientation d'un agent), UI (rotation d'un
widget).

---

### Exemple récapitulatif

```cpp
#include "NKCore/NkTypes.h"
using namespace nkentseu;

// Entiers à taille fixe : une couleur RGBA8 packée, garantie 32 bits partout.
nk_uint32 color = 0xFF8240FFu;

// Tailles et adresses : indices, et test d'alignement via une adresse-entier.
nk_size  count   = 1024;
nk_uptr  addr    = reinterpret_cast<nk_uptr>(buffer);
bool     aligned = (addr % 16) == 0;

// Byte : drapeaux 8 bits, bitwise explicite et type-safe.
Byte flags = Byte::from(0xFF);
flags      = flags & Byte::from(0x0F);          // ne garde que le nibble bas
nk_uint8 raw = static_cast<nk_uint8>(flags);    // conversion explicite (requise)

// Sentinelle : un identifiant d'entité « non assigné ».
core::NkID32 entity = core::INVALID_ID32;       // 0xFFFFFFFF

// Endianness : écrire un entier 32 bits en network byte order (big-endian).
nk_uint32 netValue = NkToBigEndian32(0x0A0B0C0Du);

// Math : un angle, documenté en radians (même type sous-jacent que NkReal).
math::NkRadians angle = 3.14159f;
```

---

[← Index NKCore](README.md) · [Récap NKCore](../NKCore.md) · [Atomiques & synchronisation →](Atomics.md)
