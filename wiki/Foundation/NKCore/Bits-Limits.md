# Bits, limites numériques et builtins

> Couche **Foundation** · NKCore · Trois outils de bas niveau qui rendent service un peu partout :
> la manipulation de bits `NkBits` (compter, faire tourner, extraire, inverser), les bornes des
> types numériques `NkNumericLimits<T>`, et les *builtins* de compilation `NkBuiltin`
> (`__FILE__`/`__LINE__`/fonction, identifiants uniques, messages de compilation, profiling).

Dès qu'on descend sous la couche du gameplay, on tombe sur des problèmes qui ne parlent plus de
positions ni d'entités mais de **bits, de bornes et de contexte de compilation**. Un allocateur
veut arrondir une taille à la puissance de deux supérieure ; un système ECS veut itérer sur les
composants présents d'une entité codés dans un masque ; un format de fichier veut lire des entiers
en *big-endian* sur une machine *little-endian* ; un test veut savoir si `nk_uint32` peut contenir
une valeur sans déborder ; une assertion veut afficher le fichier et la ligne exacts où elle a
sauté. On *peut* écrire tout cela à la main — des boucles sur les bits, des constantes magiques
comme `0xFFFFFFFF`, des `__FILE__` recopiés partout — mais c'est verbeux, fragile, et on rate les
**instructions matérielles dédiées** (`popcnt`, `bsf`, `bsr`, `bswap`, `rol`) que les processeurs
offrent justement pour ça.

Cette page réunit les trois headers qui répondent à ces trois familles de besoins. Ils n'ont rien
en commun fonctionnellement — c'est leur **niveau** qui les rassemble : ce sont les briques
zéro-STL sur lesquelles tout le reste s'appuie. Toute l'API publique reste en types `nk_*` (jamais
`int`/`size_t` bruts dans les signatures), même si les implémentations s'autorisent `<cstdint>`,
`<intrin.h>` et les *builtins* GCC/Clang en interne.

- **Namespace** : `nkentseu` (sous-namespaces `nkentseu::debug` et `nkentseu::log` pour le profiling
  et le logging de `NkBuiltin`)
- **Headers** : `#include "NKCore/NkBits.h"` · `#include "NKCore/NkLimits.h"` ·
  `#include "NKCore/NkBuiltin.h"`

---

## Manipuler les bits : `NkBits`

`NkBits` est une classe **sans état** dont toutes les méthodes sont `static` — on ne l'instancie
jamais, on écrit `NkBits::CountBits(x)`. Chaque méthode prend automatiquement la **version
matérielle** quand le compilateur l'expose (intrinsics MSVC `__popcnt`/`_BitScanForward`, *builtins*
GCC/Clang `__builtin_popcount`/`__builtin_ctz`…), et retombe sur une **version logicielle** portable
sinon. Le choix se fait à la compilation via les macros `NKENTSEU_COMPILER_*` ; vous n'avez rien à
faire.

```cpp
nk_uint32 v = 0b1011000;

nk_int32 pop = NkBits::CountBits(v);            // 3 bits à 1
nk_int32 ctz = NkBits::CountTrailingZeros(v);   // 3 zéros avant le premier 1
nk_int32 ffs = NkBits::FindFirstSet(v);         // 3 : indice du premier bit à 1
nk_uint32 r  = NkBits::RotateLeft(v, 8);        // rotation circulaire gauche
```

La distinction la plus importante à retenir oppose les deux familles de **recherche de bit**. Les
méthodes `CountTrailingZeros` / `CountLeadingZeros` (et leurs cousines bas niveau `NK_CTZ32`,
`NK_CLZ32`…) **renvoient la largeur en bits** — `sizeof(T)*8`, donc 32 ou 64 — quand l'entrée vaut
**zéro**. Ce n'est *pas* un indice utilisable : si vous vous en servez pour décaler ou indexer un
tableau de 32 entrées sans vérifier le cas zéro, vous débordez. C'est pourquoi `FindFirstSet` et
`FindLastSet` existent : ce sont les versions **sûres**, qui renvoient **−1** sur une entrée nulle.
Préférez-les dès que l'entrée peut être zéro.

L'autre piège classique concerne les **puissances de deux**. `IsPowerOfTwo` considère que **0 n'est
pas une puissance de deux** (il teste `value > 0 && (value & (value-1)) == 0`). Et `Log2` n'est
**valide que sur une vraie puissance de deux** : il l'assERTe en debug (`NKENTSEU_ASSERT_MSG`) mais a
un comportement indéfini en release si vous lui passez autre chose.

> **En résumé.** `NkBits` = comptage/recherche/rotation/extraction de bits, en prenant les
> instructions matérielles automatiquement. Retenez deux pièges : `CountTrailing/LeadingZeros`
> renvoient `32`/`64` (la largeur) sur l'entrée 0 — utilisez `FindFirstSet`/`FindLastSet` (qui
> renvoient `−1`) si l'entrée peut être nulle ; et `Log2` n'est défini que sur une puissance de deux.

---

## Connaître les bornes : `NkNumericLimits<T>`

`NkNumericLimits<T>` est l'équivalent zéro-STL de `std::numeric_limits` : un `template` **spécialisé
par type** qui répond à « quelle est la plus grande valeur d'un `nk_uint32` ? », « quel est le plus
petit écart représentable autour de 1 pour un `nk_float32` ? », « ce type est-il signé ? ». On
remplace ainsi les constantes magiques (`0xFFFFFFFF`, `3.4e38f`…), fragiles et illisibles, par des
appels portables et auto-documentés.

```cpp
auto maxU32 = NkNumericLimits<nk_uint32>::max();       // 4294967295
auto eps    = NkNumericLimits<nk_float32>::epsilon();  // ~1.19e-7

static_assert(NkNumericLimits<nk_int32>::is_specialized, "type entier supporté");
static_assert(NkNumericLimits<nk_int32>::is_signed,      "int32 est signé");
```

Le template **générique** (non spécialisé) n'expose qu'un seul membre : `is_specialized = false`.
C'est volontaire — il sert de garde-fou. Dans du code générique, un
`static_assert(NkNumericLimits<T>::is_specialized, …)` refuse à la compilation tout type que le
moteur ne sait pas borner. Les spécialisations livrées couvrent les huit entiers
(`nk_int8`…`nk_uint64`) et les deux flottants (`nk_float32`, `nk_float64`).

Deux nuances valent un avertissement. D'abord, pour un **flottant**, `min()` n'est *pas* la valeur
la plus négative : c'est la **plus petite valeur normalisée positive** (~1.18e-38 en `float32`).
Pour la borne basse réelle (négative), prenez `lowest()` (== `-max()`). Sur un **entier**, en
revanche, `min()` et `lowest()` coïncident. Ensuite, `infinity()` et `quiet_NaN()` sont **`noexcept`
mais pas `constexpr`** (elles fabriquent leur valeur par manipulation de bits dans le `.cpp`) : on
ne peut donc pas les employer dans un contexte `constexpr`, contrairement à `min`/`max`/`lowest`/
`epsilon`.

L'`epsilon()` mérite enfin une mention pratique : comparer deux flottants avec `==` est presque
toujours un bug, car le moindre arrondi fait diverger des valeurs « égales ». La bonne pratique est
de comparer leur **différence** à un epsilon — et pour tester un NaN, l'idiome est `value != value`
(seul un NaN est différent de lui-même).

> **En résumé.** `NkNumericLimits<T>` = bornes et propriétés numériques portables, par type.
> `is_specialized` garde le code générique ; `epsilon()` pour comparer des flottants (jamais `==`) ;
> `value != value` pour détecter un NaN. Piège flottant : `min()` = plus petite **normalisée
> positive**, utilisez `lowest()` pour la borne négative. `infinity()`/`quiet_NaN()` ne sont **pas**
> `constexpr`.

---

## Le contexte de compilation : `NkBuiltin`

`NkBuiltin` rassemble les macros et helpers qui exposent ce que **seul le compilateur connaît** : le
fichier et la ligne courants, le nom de la fonction, la date de build, des identifiants uniques, des
messages affichés à la compilation. Il sert de socle aux assertions, au logging contextuel et à
l'instrumentation. Il inclut d'abord `NkPlatform.h` (source canonique de
`NKENTSEU_BUILTIN_FILE/LINE/FUNCTION/COLUMN`) et `NkMacros.h` (`NKENTSEU_TODO/FIXME/UNUSED`,
`CONCAT`/`STRINGIFY`), précisément pour **éviter la double définition** (`-Wmacro-redefined`).

```cpp
NKENTSEU_LOG_ERROR("chargement échoué");   // ajoute fichier:ligne:fonction automatiquement
NKENTSEU_NOTE("revoir ce chemin");          // message affiché pendant la compilation

int NKENTSEU_UNIQUE_NAME(tmp_) = compute(); // nom unique dérivé de __LINE__
NKENTSEU_PROFILE_SCOPE("UpdatePhysics");    // mesure RAII si profiling activé
```

Deux mises en garde dominent ce header. Premièrement, **`NKENTSEU_BUILTIN_FILE/LINE/FUNCTION` sont
les formes `__builtin_FILE()`/`__builtin_LINE()`/`__builtin_FUNCTION()`** — des *appels*, pas des
littéraux. On ne peut donc **pas les concaténer** à une chaîne (`"…" NKENTSEU_BUILTIN_FILE` ne
compile pas) ; pour fabriquer un littéral concaténé (comme le fait `NKENTSEU_NOTE`), utilisez les
`__FILE__`/`__LINE__` bruts. Deuxièmement, **plusieurs macros de ce header sont des placeholders
no-op** : `NKENTSEU_SIMPLE_ASSERT` n'agit pas réellement, et `nkentseu::log::Error/Warning/Info`
sont des coquilles vides à connecter au vrai logger. La **vraie** assertion du moteur est
`NKENTSEU_ASSERT_MSG`, déclarée dans `NKCore/Assert/NkAssert.h` (c'est elle que `NkBits` utilise) —
voir [Les assertions](Assertions.md).

> **En résumé.** `NkBuiltin` expose le contexte de compilation (fichier/ligne/fonction, date,
> identifiants uniques, messages, profiling RAII). Deux pièges : `NKENTSEU_BUILTIN_FILE/LINE/FUNCTION`
> sont des `__builtin_*()` **non concaténables** (utilisez `__FILE__`/`__LINE__` pour la concat) ;
> `NKENTSEU_SIMPLE_ASSERT` et `log::Error/Warning/Info` sont des **placeholders** — la vraie
> assertion est `NKENTSEU_ASSERT_MSG` (NkAssert.h).

---

## Aperçu de l'API

Tous les éléments publics des trois headers, en un coup d'œil. Chacun est détaillé dans la
« Référence complète ». Complexités entre crochets quand elles comptent.

### `NkBits` — manipulation de bits (`NkBits.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Comptage | `CountBits(value)` `[O(1) HW / O(n) SW]` | Nombre de bits à 1 (population count). |
| Comptage | `CountTrailingZeros(value)` | Zéros de poids faible ; **`sizeof(T)*8` si 0**. |
| Comptage | `CountLeadingZeros(value)` | Zéros de poids fort ; **`sizeof(T)*8` si 0**. |
| Recherche | `FindFirstSet(value)` | Indice 0-based du 1er bit à 1 ; **`−1` si 0**. |
| Recherche | `FindLastSet(value)` | Indice 0-based du dernier bit à 1 ; **`−1` si 0**. |
| Rotation | `RotateLeft(value, shift)` | Rotation circulaire gauche (shift masqué `&= bits-1`). |
| Rotation | `RotateRight(value, shift)` | Rotation circulaire droite (shift masqué). |
| Puissances de 2 | `IsPowerOfTwo(value)` | `value>0 && (value&(value-1))==0` (0 → faux). |
| Puissances de 2 | `NextPowerOfTwo(nk_uint32)` / `(nk_uint64)` | Plus petite puissance de 2 ≥ value (1 si 0). |
| Puissances de 2 | `Log2(value)` | log₂ d'une puissance de 2 (assertée en debug). |
| Champs de bits | `ExtractBits(value, position, count)` | Extrait `count` bits dès `position`, alignés à droite. |
| Champs de bits | `InsertBits(dest, src, position, count)` | Insère les `count` LSB de `src` dans `dest`. |
| Inversion | `ReverseBits(value)` `[O(n)]` | Inverse l'ordre de **tous les bits**. |
| Inversion | `ReverseBytes(value)` | Inverse l'ordre des **octets** (dispatch par taille). |
| Endianness | `ByteSwap16/32/64(value)` | Échange d'octets type-safe (wrappers des macros). |
| Macros HW | `NK_POPCOUNT32/64(x)` | Population count brut (macro). |
| Macros HW | `NK_CTZ32/64(x)` · `NK_CLZ32/64(x)` | Trailing/leading zeros bruts (largeur si 0). |
| Macros HW | `NK_BYTESWAP16/32/64(x)` | Byte swap brut (macro ou fonction selon compilateur). |

### `NkNumericLimits<T>` — bornes numériques (`NkLimits.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Garde | `is_specialized` | `false` sur le générique, `true` sur chaque type supporté. |
| Propriétés | `is_signed`, `is_integer` | Signé ? entier ? (`is_exact` pour les flottants). |
| Bornes | `min()` | Plus petite valeur (entier : minimale ; **flottant : plus petite normalisée positive**). |
| Bornes | `max()` | Plus grande valeur représentable. |
| Bornes | `lowest()` | Plus petite valeur finie (`== min()` entier ; `== -max()` flottant). |
| Flottant | `epsilon()` | Plus petit écart représentable autour de 1. |
| Flottant | `infinity()`, `quiet_NaN()` | Infini / NaN — **`noexcept` mais non `constexpr`**. |
| Précision | `digits`, `digits10`, `max_digits10` | Bits/chiffres significatifs. |
| Flottant | `radix`, `min/max_exponent`, `min/max_exponent10` | Base et plage d'exposants. |
| Types | `nk_int8…nk_uint64`, `nk_float32`, `nk_float64` | Les 10 spécialisations livrées. |

### `NkBuiltin` — contexte de compilation (`NkBuiltin.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Builtins | `NKENTSEU_BUILTIN_FILE/LINE/FUNCTION` (+ alias `NK_*`) | `__builtin_*()` — **non concaténables** à un littéral. |
| Builtins | `NKENTSEU_BUILTIN_DATE/TIME/TIMESTAMP` (+ `NK_*`) | `__DATE__` / `__TIME__` / les deux. |
| Identifiants | `NKENTSEU_UNIQUE_ID`, `NKENTSEU_UNIQUE_NAME(prefix)` | Id / nom uniques (basés `__LINE__`). |
| Identifiants | `NKENTSEU_CONCAT(a,b)`, `NKENTSEU_STRINGIFY(x)` | Concaténation / mise en chaîne (sous `#ifndef`). |
| Messages | `NKENTSEU_COMPILE_MESSAGE(msg)`, `NKENTSEU_NOTE(msg)` | Message affiché à la compilation. |
| Assertions | `NKENTSEU_SIMPLE_ASSERT(cond)` | **Placeholder no-op** (la vraie = `NKENTSEU_ASSERT_MSG`). |
| Profiling | `NKENTSEU_PROFILE_SCOPE(name)` | Mesure RAII si `NKENTSEU_ENABLE_PROFILING`. |
| Profiling | `NKENTSEU_INSTRUMENT_FUNCTION` | Trace RAII si `NKENTSEU_ENABLE_INSTRUMENTATION`. |
| Logging | `NKENTSEU_LOG_ERROR/WARNING/INFO(msg)` | Log avec fichier/ligne/fonction ajoutés. |
| Logging | `..._LOG_*_CONTEXT(msg,file,line,func)` | Variante explicite (redéfinissable via `..._IMPL`). |
| Utilitaires | `NKENTSEU_DECLARE_WITH_CONTEXT(type,name,value)` | Déclare + marque `UNUSED`. |
| Utilitaires | `NKENTSEU_CHECK_RETURN(cond,retval)`, `..._CHECK_CONTINUE(cond)` | Garde avec `return` / `continue` + warning. |
| Tests | `NKENTSEU_TEST_ASSERT(cond,msg)`, `NKENTSEU_TEST_EQUAL(a,b,msg)` | Assertions de test (retour `false`). |
| Legacy | `NKENTSEU_CURRENT_FILE/LINE/FUNCTION` | Alias dépréciés des builtins. |
| Classes | `nkentseu::debug::ProfileScope`, `InstrumentFunction` | RAII (compilées si flags activés). |
| Fonctions | `nkentseu::log::Error/Warning/Info(...)` | **Placeholders no-op** à connecter au logger. |

---

## Référence complète

Chaque élément est repris ici en détail : comportement, complexité, et cas d'usage dans les
différents domaines du moteur. Les éléments triviaux sont décrits brièvement ; les opérations
structurantes le sont **à fond**.

### NkBits — comptage et recherche de bits

`CountBits(value)` renvoie le **nombre de bits à 1** (population count). Le dispatch se fait par
`sizeof(T)` : 8 octets → `NK_POPCOUNT64`, 4 → `NK_POPCOUNT32`, toute autre taille → la version
logicielle `O(n)` (attention : seuls `sizeof` 4 et 8 câblent l'intrinsic — un `nk_uint16` passe par
le software). C'est l'opération qui répond à « combien de choses sont actives dans ce masque ? » :

- **ECS / scène** — un masque de composants (*signature* d'archétype) : `CountBits` donne le nombre
  de composants d'une entité ; un masque de calques de rendu ou de couches de collision : combien
  sont allumés.
- **Rendu** — compter les bits d'un masque d'échantillons en MSAA, ou les *render targets* actifs
  d'un MRT.
- **Gameplay / IA** — un inventaire ou un ensemble de capacités codé en bits : `CountBits` = nombre
  d'objets/talents possédés.

`CountTrailingZeros` et `CountLeadingZeros` comptent les zéros consécutifs depuis le bit de poids
**faible** (LSB) ou **fort** (MSB). Le point critique, déjà signalé : sur une **entrée nulle**, elles
renvoient la largeur du type (`sizeof(T)*8`, soit 32 ou 64), pas un indice. `CountLeadingZeros` est
le cœur de bien des calculs de log et d'alignement : `bits - 1 - CLZ(x)` donne la position du bit de
poids fort.

`FindFirstSet` et `FindLastSet` sont les versions **sûres** orientées indice : 0-based, et surtout
**`−1` sur l'entrée 0**. `FindFirstSet` est l'outil canonique pour **itérer sur les bits allumés**
d'un masque, un par un, sans tester les 32/64 positions :

- **ECS** — parcourir les composants présents d'une entité : `ffs` donne le prochain bit allumé, on
  le traite, on l'efface (`mask &= mask - 1`), on recommence — itération en `O(bits allumés)`, pas
  `O(largeur)`.
- **Threading / scheduling** — un *bitset* de tâches prêtes ou de workers libres : trouver le
  premier libre.
- **GPU / allocation** — une *free-list* de slots de descripteurs ou de pages codée en masque :
  `FindFirstSet` localise le premier slot disponible.

### NkBits — rotation : `RotateLeft`, `RotateRight`

La rotation **circulaire** fait ressortir par un bout les bits qui sortent par l'autre (contrairement
au décalage simple qui les jette). `RotateLeft(value, shift)` et `RotateRight(value, shift)`
**masquent automatiquement** le shift (`shift &= bits - 1`) : passer un shift ≥ largeur n'est donc
**pas** un comportement indéfini, il est ramené modulo la largeur. Domaines :

- **Hachage** — la quasi-totalité des fonctions de hachage non cryptographiques (FNV, MurmurHash,
  xxHash) mélangent leurs bits par rotations ; indispensable pour les tables de hachage du moteur
  (assets, strings internées, clés ECS).
- **Compression / IO** — codecs et générateurs pseudo-aléatoires (un xorshift-rotate) reposent sur
  la rotation.
- **Cryptographie légère** — checksums et MAC simples.

### NkBits — puissances de deux : `IsPowerOfTwo`, `NextPowerOfTwo`, `Log2`

Ce trio est le pain quotidien des **allocateurs** et de tout ce qui s'aligne. `IsPowerOfTwo` teste
`value > 0 && (value & (value-1)) == 0` — donc **0 renvoie faux**. `NextPowerOfTwo` (deux surcharges
non-template, `nk_uint32` et `nk_uint64`, définies dans `NkBits.cpp`) renvoie la plus petite
puissance de deux ≥ value (et 1 pour value == 0). `Log2` renvoie l'exposant d'une **vraie puissance
de deux** : il l'assERTe en debug via `NKENTSEU_ASSERT_MSG(IsPowerOfTwo(value), …)` et délègue à
`FindLastSet` ; en release sur une non-puissance-de-deux, le résultat est indéfini.

- **Mémoire / allocateurs** — arrondir une taille de bloc ou un alignement à la puissance de deux
  supérieure (`NextPowerOfTwo`), vérifier qu'un alignement demandé est légal (`IsPowerOfTwo`),
  calculer le rang d'un *buddy allocator* ou la taille d'un *pool* (`Log2`). Voir
  [NKMemory](../NKMemory.md).
- **Rendu / GPU** — dimensionner une texture ou un atlas en puissance de deux (mipmaps), un tampon
  circulaire, un *ring buffer* de frames.
- **Conteneurs** — une table de hachage dont la capacité est une puissance de deux remplace le
  modulo par un `& (cap - 1)` ; `NextPowerOfTwo` choisit la capacité, `Log2` indexe des paliers.

### NkBits — champs de bits : `ExtractBits`, `InsertBits`

`ExtractBits(value, position, count)` lit `count` bits à partir de `position` (LSB = 0) et les
renvoie **alignés à droite** ; `InsertBits(dest, src, position, count)` écrit les `count` bits de
poids faible de `src` dans `dest` à `position` (en effaçant d'abord le champ ; les bits de `src`
au-delà de `count` sont masqués). Les deux **assertent** leurs bornes
(`position >= 0 && count > 0 && position + count <= sizeof(T)*8`). C'est l'outil du **packing** de
données :

- **Rendu / GPU** — décoder/encoder des formats compacts : couleur RGBA8 ou RGB565 dans un entier,
  normales encodées, indices de palette, *bitfields* de matériaux. Lire/écrire des flags GPU groupés.
- **ECS / sérialisation** — empaqueter plusieurs petits champs (état, type, génération) dans un seul
  *handle* 32 ou 64 bits ; `ExtractBits` les ressort à la lecture.
- **IO / formats de fichier** — lire des en-têtes binaires où plusieurs champs partagent un même mot.

### NkBits — inversion et endianness : `ReverseBits`, `ReverseBytes`, `ByteSwap16/32/64`

`ReverseBits(value)` inverse l'ordre de **tous les bits** (le LSB devient le MSB) en `O(n)` —
indispensable à la **FFT** (réordonnancement bit-reversé des échantillons) en audio, et à certains
décodeurs entropiques. À ne pas confondre avec `ReverseBytes(value)`, qui inverse l'ordre des
**octets** (dispatch par `sizeof` : 8 → `ByteSwap64`, 4 → `ByteSwap32`, 2 → `ByteSwap16`, 1 →
inchangé). Les trois `ByteSwap16/32/64` sont les wrappers type-safe des macros `NK_BYTESWAP*`. Leur
domaine est l'**endianness** :

- **IO / réseau / fichiers** — lire ou écrire des entiers *big-endian* (l'ordre réseau, et celui de
  nombreux formats : PNG, TTF, WAV…) sur une machine *little-endian* : `ByteSwap32` recadre. Voir
  [NKFileSystem](../../System/NKFileSystem.md) et NKNetwork.
- **Audio** — réordonnancement bit-reversé d'une FFT (`ReverseBits`), conversion d'échantillons PCM
  d'un *endianness* à l'autre.
- **GPU** — adapter des données binaires à l'ordre attendu par une API ou un format de texture.

### NkBits — macros bas niveau (`NK_POPCOUNT*`, `NK_CTZ*`, `NK_CLZ*`, `NK_BYTESWAP*`)

Sous les méthodes de `NkBits` vivent les macros et fonctions `static` qui font le vrai travail.
`NK_POPCOUNT32/64`, `NK_CTZ32/64`, `NK_CLZ32/64`, `NK_BYTESWAP16/32/64` sont à **portée globale**
(hors namespace) et sélectionnent l'intrinsic adéquat par compilateur. On peut les appeler
directement quand on travaille déjà sur des `nk_uint32`/`nk_uint64` fixes et qu'on veut éviter le
dispatch par `sizeof`. Même réserve que plus haut : `NK_CTZ*`/`NK_CLZ*` renvoient la **largeur**
(32/64) sur l'entrée 0 — utilisez les méthodes `FindFirstSet`/`FindLastSet` si vous voulez l'indice
sûr `−1`.

### NkNumericLimits — le garde générique et `is_specialized`

Le template non spécialisé n'expose que `is_specialized = false`. Cela transforme « ce type est-il
supporté ? » en une vérification **à la compilation** : dans une fonction ou un conteneur générique,
`static_assert(NkNumericLimits<T>::is_specialized, "type numérique non supporté")` rejette tout type
inattendu avant l'exécution. Combiné à `is_integer` / `is_signed`, il permet d'écrire du code qui se
spécialise proprement (un sérialiseur, un convertisseur de format, un clamp générique) sans toucher
à la STL.

### NkNumericLimits — bornes des entiers : `min`, `max`, `lowest`, `digits`

Les huit spécialisations entières (`nk_int8`…`nk_uint64`) exposent `min()`, `max()` et `lowest()`
(tous `constexpr noexcept`, avec `lowest() == min()` pour les entiers), plus `digits` (bits de
valeur, hors signe : 7 pour `nk_int8`, 8 pour `nk_uint8`, 31 pour `nk_int32`…) et `digits10` (chiffres
décimaux pleins). Les valeurs s'appuient sur les macros canoniques `NKENTSEU_*_MIN/MAX` (NkTypes.h).

- **Tests / validation** — vérifier qu'une valeur tient dans un type cible avant un cast étroit
  (`value <= NkNumericLimits<nk_uint16>::max()`), détecter un dépassement.
- **Sérialisation / IO** — choisir le plus petit type qui contient une plage, formater un nombre
  (`digits10` dit combien de chiffres réserver).
- **Gameplay** — initialiser un accumulateur de minimum/maximum (un meilleur score, la distance la
  plus proche) à `max()` / `min()`/`lowest()` avant la boucle de réduction.

### NkNumericLimits — propriétés des flottants : `min`, `max`, `lowest`, `epsilon`, `infinity`, `quiet_NaN`

Les spécialisations `nk_float32` (IEEE 754 binary32) et `nk_float64` (binary64) ajoutent
`is_exact = false` et toute la métadonnée d'exposant (`radix = 2`, `min/max_exponent`,
`min/max_exponent10`, `max_digits10`). Les points à connaître :

- **`min()` = plus petite valeur normalisée positive** (~1.18e-38 en float32, pas la borne
  négative). Pour la valeur **la plus négative**, c'est `lowest()` (== `-max()`).
- **`epsilon()`** vaut `2^-23` (~1.19e-7) en float32, `2^-52` (~2.22e-16) en float64 : le plus petit
  écart représentable autour de 1. C'est la référence pour comparer des flottants — voir ci-dessous.
- **`infinity()` et `quiet_NaN()`** sont `noexcept` mais **pas `constexpr`** (fabriquées par bits
  dans le `.cpp`) : utilisables à l'exécution, pas dans un `constexpr`.

Domaines :

- **Physique / animation** — comparer des flottants par `fabs(a - b) < epsilon` (positions,
  vitesses) plutôt que `==` ; clamper avec `lowest()`/`max()` ; détecter une division qui a produit
  `infinity()` ou un `quiet_NaN()` (instabilité numérique) via l'idiome `value != value`.
- **Rendu** — initialiser une *bounding box* à `+max()` / `-max()` (lowest) avant d'agréger les
  sommets ; détecter un NaN dans un shader résultat ou un calcul de matrice.
- **Audio** — repérer un échantillon devenu NaN/Inf (un filtre instable) pour le couper avant qu'il
  ne « claque » dans les haut-parleurs.

### NkBuiltin — builtins de localisation : `BUILTIN_FILE/LINE/FUNCTION`, dates, identifiants

`NKENTSEU_BUILTIN_FILE/LINE/FUNCTION` (alias `NK_BUILTIN_*`) sont les formes `__builtin_FILE()` /
`__builtin_LINE()` / `__builtin_FUNCTION()` — déclarées canoniquement dans NkPlatform.h, simplement
ré-exposées ici. Rappel décisif : ce sont des **appels**, pas des littéraux, donc **non
concaténables** à une chaîne. `NKENTSEU_BUILTIN_DATE/TIME/TIMESTAMP` valent `__DATE__` / `__TIME__` /
les deux (eux *sont* des littéraux). `NKENTSEU_UNIQUE_ID` (== `__LINE__`) et
`NKENTSEU_UNIQUE_NAME(prefix)` fabriquent des identifiants uniques par concaténation
(`NKENTSEU_CONCAT`) — utiles pour générer un nom de variable temporaire dans une macro RAII. Piège :
deux usages **sur la même ligne** collisionnent (ils partagent `__LINE__`).

- **Logging / debug** — joindre l'origine exacte (`file:line:function`) à chaque message, sans la
  recopier à la main.
- **Macros RAII** — un *scope guard*, un verrou, un timer ont besoin d'un nom local unique :
  `NKENTSEU_UNIQUE_NAME` le donne.
- **Build** — afficher la date/heure de compilation dans un écran « à propos » ou un log de version.

### NkBuiltin — messages, assertions, garde et tests

`NKENTSEU_COMPILE_MESSAGE(msg)` émet un message **pendant la compilation** (via `#pragma message` /
`_Pragma`) ; `NKENTSEU_NOTE(msg)` le préfixe du fichier et de la ligne (en `__FILE__`/`__LINE__`
bruts, justement parce que les builtins ne se concatènent pas). `NKENTSEU_SIMPLE_ASSERT(cond)` est un
**placeholder** : sa branche d'échec est vide tant qu'on ne la connecte pas — pour une vraie
assertion, on passe par `NKENTSEU_ASSERT_MSG` (NkAssert.h), celle qu'utilise `NkBits`. Les gardes de
flux `NKENTSEU_CHECK_RETURN(cond, retval)` et `NKENTSEU_CHECK_CONTINUE(cond)` court-circuitent
proprement (avec un warning loggé) une fonction ou une itération de boucle quand une précondition
échoue. Les macros de test `NKENTSEU_TEST_ASSERT(cond, msg)` et `NKENTSEU_TEST_EQUAL(a, b, msg)`
servent aux fonctions de test renvoyant `bool` ; `TEST_EQUAL` affiche les **expressions
stringifiées** (pas les valeurs) en cas d'échec.

- **IO / chargement** — `NKENTSEU_CHECK_RETURN(file.IsOpen(), false)` sort tôt d'un loader si un
  fichier manque, en journalisant la cause.
- **Boucles de systèmes** — `NKENTSEU_CHECK_CONTINUE(entity.IsValid())` saute une entité invalide
  sans planter le système ECS.
- **Tests unitaires** — valider les conteneurs, le math, les codecs avec `TEST_ASSERT`/`TEST_EQUAL`.

### NkBuiltin — profiling et instrumentation (`ProfileScope`, `InstrumentFunction`)

`NKENTSEU_PROFILE_SCOPE(name)` et `NKENTSEU_INSTRUMENT_FUNCTION` instancient un objet **RAII** qui
mesure la durée d'un bloc ou d'une fonction — mais **seulement** si les flags
`NKENTSEU_ENABLE_PROFILING` / `NKENTSEU_ENABLE_INSTRUMENTATION` sont définis (sinon ils se réduisent
à `((void)0)`, zéro coût en release). Les classes sous-jacentes `nkentseu::debug::ProfileScope` et
`InstrumentFunction` ne sont d'ailleurs **compilées** que sous ces flags. Pour l'instant, leur
horloge interne (`GetCurrentTimestamp`) est un placeholder renvoyant 0 — l'ossature est là, la
mesure réelle reste à brancher.

- **Rendu / threading** — encadrer une passe de rendu, une mise à jour de système, un job pour
  alimenter un *frame profiler* et repérer les pics par frame.
- **Physique / animation** — mesurer le coût d'une étape de simulation ou d'un *blend* d'animation
  sans polluer le code de release.

### NkBuiltin — logging contextuel et placeholders `nkentseu::log`

`NKENTSEU_LOG_ERROR/WARNING/INFO(msg)` ajoutent automatiquement `BUILTIN_FILE/LINE/FUNCTION` et
délèguent à `..._LOG_*_CONTEXT(msg, file, line, func)`, lesquels appellent par défaut
`nkentseu::log::Error/Warning/Info`. **Ces trois fonctions sont des placeholders no-op** (corps
vide) : tant qu'on ne les redirige pas vers le vrai logger — soit en redéfinissant
`NKENTSEU_LOG_*_CONTEXT_IMPL`, soit en branchant le module [NKLogger](../../System/NKLogger.md) —
elles n'affichent rien. C'est le point d'extension prévu pour connecter NKCore au système de log de
la couche System sans créer de dépendance circulaire.

### Le socle commun

- **Zéro-STL.** `NkBits`, `NkNumericLimits` et `NkBuiltin` remplacent respectivement `<bit>`,
  `<limits>` et les recettes `__FILE__`/`__LINE__` éparpillées, sans inclure la STL dans l'API
  publique (types `nk_*` partout).
- **Stateless et matériel d'abord.** `NkBits` n'a aucun état, toutes ses méthodes sont `static` et
  thread-safe, et prennent l'instruction matérielle quand elle existe.
- **Macros canoniques uniques.** Pour éviter `-Wmacro-redefined`, chaque macro a **une** déclaration
  d'origine : `BUILTIN_FILE/LINE/FUNCTION` → NkPlatform.h, `TODO/FIXME/UNUSED` + `CONCAT/STRINGIFY` →
  NkMacros.h, `ASSERT_MSG` → NkAssert.h. `NkBuiltin` les ré-expose ou les complète sous `#ifndef`.
- **Constantes déléguées.** Les valeurs numériques (`NKENTSEU_*_MIN/MAX`) viennent de NkTypes.h ;
  `NextPowerOfTwo`, `infinity()` et `quiet_NaN()` sont implémentées dans les `.cpp`.

---

### Exemple récapitulatif

```cpp
#include "NKCore/NkBits.h"
#include "NKCore/NkLimits.h"
#include "NKCore/NkBuiltin.h"
using namespace nkentseu;

// NkBits : itérer sur les composants présents d'une entité (masque ECS).
nk_uint32 signature = entity.componentMask;
while (signature) {
    nk_int32 i = NkBits::FindFirstSet(signature);  // prochain bit à 1 (−1 si fini)
    ProcessComponent(i);
    signature &= signature - 1;                     // efface le bit traité
}

// NkBits : arrondir une taille de bloc à la puissance de deux supérieure (allocateur).
nk_uint32 cap = NkBits::NextPowerOfTwo(requested);  // ex. 1000 -> 1024

// NkNumericLimits : comparer des flottants sans '=='.
float32 eps = NkNumericLimits<nk_float32>::epsilon();
bool equal  = (a - b < eps) && (b - a < eps);

// NkNumericLimits : initialiser une bounding box avant agrégation.
NkVec3 mn{  NkNumericLimits<nk_float32>::max()   , /* … */ };
NkVec3 mx{  NkNumericLimits<nk_float32>::lowest(), /* … */ };

// NkBuiltin : garde de chargement + log contextuel automatique.
NKENTSEU_CHECK_RETURN(file.IsOpen(), false);        // sort tôt + warning si fermé
NKENTSEU_LOG_INFO("ressource chargée");             // ajoute fichier:ligne:fonction
```

---

[← Les assertions](Assertions.md) · [Index NKCore](README.md) · [Récap NKCore](../NKCore.md) · [L'énumération →](Enumeration.md)
