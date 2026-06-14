# L'encodage des chaînes

> Couche **Foundation** · NKContainers · Manipuler du **texte Unicode** au plus bas niveau :
> les codecs `NkUTF8` / `NkUTF16` / `NkUTF32`, les utilitaires 7 bits `NkASCII`, le
> transport binaire `NkBase64`, et la façade `NkEncoding` qui détecte et valide.

Dès qu'on veut afficher un nom de joueur en japonais, sauvegarder un fichier `.nkscene` lisible
partout, ou parler à une API Windows en `wchar_t`, on bute sur la même réalité : **un texte n'est
pas une suite d'octets, c'est une suite de codepoints Unicode**, et il existe plusieurs façons
incompatibles de ranger ces codepoints en mémoire. UTF-8 les code sur 1 à 4 octets, UTF-16 sur 1
ou 2 unités de 16 bits, UTF-32 sur un mot fixe de 32 bits. La question n'est jamais « lequel est
correct » (ils décrivent le même texte), mais « lequel parle l'interlocuteur d'en face » — le
disque, le réseau, l'OS, le GPU. Ce groupe de headers est la **passerelle** entre ces formats.

Contrairement au reste de NKContainers, ici **aucune classe, aucun template, aucun allocateur**.
Ce sont des **fonctions libres** rangées en sous-namespaces, qui travaillent sur des **buffers
bruts** que l'appelant possède : `const char*` pour l'UTF-8, `const uint16*` pour l'UTF-16,
`const uint32*` pour l'UTF-32. Ce n'est **pas** une classe `NkString` (celle-ci vit ailleurs et
*utilise* ces fonctions) : c'est la mécanique en dessous, celle qui décode un octet, compte des
caractères, ou convertit un format en un autre. Seul `NkBase64` fait exception et renvoie des
`NkString`. Toute l'API est en **PascalCase** avec préfixe `Nk` — pas de variantes minuscules STL.

- **Namespace** : `nkentseu::encoding` (sous-namespaces `utf8` / `utf16` / `utf32` / `ascii` / `base64`)
- **Header parapluie** : `#include "NKContainers/NKContainers.h"`

---

## Le format à largeur variable : `NkUTF8`

L'UTF-8 est le format **par défaut du moteur** — celui des fichiers `.nkproj`/`.nkscene`, du
réseau, des logs. Son génie : un texte purement ASCII reste octet pour octet identique à de
l'ASCII, et les caractères plus exotiques s'étalent sur 2, 3 ou 4 octets sans jamais ressembler à
de l'ASCII. Mais cette compacité a un prix : **on ne peut pas sauter au i-ème caractère par un
simple `str[i]`**, parce qu'un « caractère » occupe un nombre variable d'octets. Il faut *décoder*
en marchant.

C'est exactement ce que fait `utf8::NkDecodeChar(str, cp)` : il lit un codepoint, le pose dans
`cp`, et renvoie **le nombre d'octets consommés** (1 à 4). La valeur de retour est aussi le signal
d'erreur — **`0` signifie séquence invalide**. L'idiome de parcours en découle directement :

```cpp
usize pos = 0;
uint32 cp;
while (pos < byteLen) {
    usize consumed = utf8::NkDecodeChar(data + pos, cp);
    if (consumed == 0) break;          // octet illégal : on s'arrête
    // ... traiter cp ...
    pos += consumed;
}
```

Pour aller plus vite quand on n'a pas besoin du codepoint, `NkNextChar`/`NkPrevChar` sautent d'un
caractère à l'autre (le recul saute les octets de continuation `10xxxxxx`, borné par `start`), et
`NkCountChars` compte les codepoints d'un buffer. Côté écriture, `NkEncodeChar(cp, out)` pose un
codepoint dans un buffer d'au moins 4 octets. Et toute une famille `NkToUTF16` / `NkToUTF32` /
`NkFromUTF16` / `NkFromUTF32` traduit vers et depuis les deux autres formats.

Ce n'est **pas** un format où l'indexation est gratuite, et ce n'est **pas** un endroit où l'on
alloue : tous les buffers de sortie viennent de l'appelant.

> **En résumé.** `NkUTF8` = le format par défaut, 1 à 4 octets par caractère. On parcourt avec
> `NkDecodeChar` (`0` = invalide), on saute avec `NkNextChar`/`NkPrevChar`, on compte avec
> `NkCountChars`, on convertit avec `NkTo*`/`NkFrom*`. Largeur variable : pas de `str[i]` direct.

---

## Les deux formats à unités : `NkUTF16` et `NkUTF32`

`NkUTF16` est le format de **l'API Windows** (`wchar_t` y fait 16 bits). Un caractère y tient en
une unité de 16 bits pour tout le plan de base (BMP), ou en **deux** unités — une *paire de
surrogates* — pour les codepoints au-delà de `0x10000` (emojis, certaines écritures). D'où des
constantes et prédicats dédiés : `NK_HIGH_SURROGATE_START`/`END`, `NK_LOW_SURROGATE_START`/`END`,
et les inline `NkIsHighSurrogate` / `NkIsLowSurrogate` / `NkIsSurrogate`. Le décodage suit le même
contrat que l'UTF-8 : `utf16::NkDecodeChar` renvoie le nombre d'**unités** consommées (1 ou 2, `0`
si erreur), et `NkToUTF8` / `NkToUTF32` convertissent sortants. Il n'y a **pas** de `NkFrom*` ici :
les conversions entrantes vivent toujours côté du format source (`utf8::NkToUTF16`, `utf32::NkToUTF16`).

`NkUTF32` est l'opposé : **largeur fixe**, un `uint32` = un codepoint, point final. C'est le format
de travail idéal quand on doit **manipuler les caractères un par un** — couper une chaîne au i-ème
caractère, comparer codepoint à codepoint, appliquer une transformation — car là, `str[i]` est
enfin un vrai accès direct. La contrepartie est qu'il consomme quatre octets même pour un simple
`'a'`. La plupart de ses fonctions sont **triviales et inline** (présentes par symétrie avec les
deux autres) : `NkCharLength` renvoie **toujours 1**, `NkCountChars` renvoie **simplement
`length`**, `NkNextChar` fait `str + 1`. Attention : ces trois-là **ignorent** certains de leurs
paramètres (`str`, `start`) — ne vous y fiez pas.

```cpp
// UTF-32 comme format pivot : on décode, on travaille à largeur fixe, on recode.
uint32 buf[256]; usize read, written;
utf8::NkToUTF32(src, srcLen, buf, 256, read, written);
for (usize i = 0; i < written; ++i) buf[i] = /* transformer buf[i] */;
```

> **En résumé.** `NkUTF16` = 1 ou 2 unités de 16 bits (paires de surrogates), le format Windows ;
> ses prédicats `NkIsHighSurrogate`/`NkIsLowSurrogate`/`NkIsSurrogate` gardent les paires. `NkUTF32`
> = largeur fixe, un codepoint = un `uint32`, idéal comme format pivot pour manipuler caractère par
> caractère. Pas de `NkFrom*` côté UTF-16 ; trois fonctions UTF-32 ignorent leurs paramètres.

---

## Le format universel des conversions : les deux passes

Toutes les fonctions `NkTo*` / `NkFrom*` partagent un **même idiome incontournable**. Comme elles
n'allouent jamais, elles ne peuvent pas deviner la taille du buffer de sortie. On les appelle donc
**deux fois** : d'abord pour **dimensionner** (`dst = nullptr, dstLen = 0`), ce qui renvoie
`NK_TARGET_EXHAUSTED` et remplit le compteur de sortie avec la taille nécessaire ; puis pour
**convertir** dans le buffer exact qu'on vient d'allouer, ce qui renvoie `NK_SUCCESS`.

```cpp
usize read, needed;
auto r = utf8::NkToUTF16(src, srcLen, nullptr, 0, read, needed);   // 1re passe : dimensionner
if (r != NkConversionResult::NK_TARGET_EXHAUSTED) { /* source illégale ! */ }
uint16* dst = /* allouer 'needed' unités */;
utf8::NkToUTF16(src, srcLen, dst, needed, read, needed);           // 2e passe : convertir
```

Le piège documenté : après la 1re passe on teste `!= NK_TARGET_EXHAUSTED` (pour distinguer un
manque de place légitime d'une **source illégale**), **pas** `== NK_SUCCESS`. Les codes de retour
sont `NK_SUCCESS`, `NK_SOURCE_EXHAUSTED` (source tronquée), `NK_TARGET_EXHAUSTED` (dst trop petit)
et `NK_SOURCE_ILLEGAL` (séquence invalide).

> **En résumé.** Conversions = **deux passes** : `dst=nullptr` pour obtenir la taille
> (`NK_TARGET_EXHAUSTED` + compteur rempli), puis convertir dans le buffer alloué (`NK_SUCCESS`).
> Tester `!= NK_TARGET_EXHAUSTED` après la 1re passe pour détecter une source illégale.

---

## Le clavier latin : `NkASCII`

Quand on traite du texte qu'on sait **purement 7 bits** — un parseur de commande, une clé de
config, un identifiant, un nombre tapé au clavier — l'UTF-8 est une lourdeur inutile. `NkASCII`
offre la batterie classique de tests et de transformations sur des `char`, toutes **inline** et
**`noexcept`** : `NkIsDigit`, `NkIsAlpha`, `NkIsAlphaNumeric`, `NkIsUpper`/`NkIsLower`,
`NkIsHexDigit`, `NkIsWhitespace`, `NkIsControl`, `NkIsPrintable`, `NkIsPunctuation`. Côté casse,
`NkToUpper`/`NkToLower` transforment un caractère (et laissent les autres intacts), et leurs
cousines `NkToUpperInPlace`/`NkToLowerInPlace` opèrent sur un **buffer mutable** entier.

Pour le numérique, `NkToDigit`/`NkToHexDigit` convertissent un caractère en valeur (**`-1`** si ce
n'en est pas un), `NkFromDigit`/`NkFromHexDigit` font l'inverse (**`'\0'`** si hors plage). Enfin
`NkCompareIgnoreCase` et `NkEqualsIgnoreCase` comparent deux chaînes sans tenir compte de la casse —
exactement ce qu'il faut pour des clés ou des extensions de fichier insensibles à la casse.

> **En résumé.** `NkASCII` = utilitaires 7 bits inline `noexcept` : prédicats `NkIs*`, casse
> `NkToUpper`/`NkToLower` (+ versions `InPlace`), conversions numériques `NkToDigit`/`NkToHexDigit`
> (`-1`/`'\0'` en cas d'échec), comparaison insensible à la casse `NkCompareIgnoreCase`.

---

## Le transport binaire : `NkBase64`

Le Base64 ne change pas un *texte* mais transforme un **buffer binaire quelconque** en une chaîne
de caractères imprimables — pour glisser une image, une clé ou un blob dans un JSON, une URL ou un
en-tête réseau qui n'accepte que du texte. C'est le **seul** header de ce groupe qui dépend de
`NkString` et qui **alloue** : `NkEncode(data, length)` rend une `NkString`, `NkEncodeString` fait
de même depuis une `NkStringView`, et la variante `NkEncodeURLSafe` utilise l'alphabet `-`/`_`
compatible URL. Le décodage `NkDecode` écrit dans un `out` **fourni par l'appelant** (à
pré-dimensionner à ≈ 3/4 de la longueur du Base64), tandis que `NkDecodeToString` rend directement
une `NkString`. `NkIsValid` vérifie qu'une chaîne est du Base64 bien formé.

> **En résumé.** `NkBase64` = binaire → texte imprimable et retour. `NkEncode`/`NkEncodeString`/
> `NkEncodeURLSafe` allouent une `NkString` ; `NkDecode`/`NkDecodeURLSafe` écrivent dans un `out`
> à pré-dimensionner (~3/4 de la taille) ; `NkDecodeToString` rend une `NkString` ; `NkIsValid` valide.

---

## La façade : `NkEncoding`

`NkEncoding` est le header de base que tous les autres incluent. Il porte les enums partagés
(`NkEncodingType`, `NkConversionResult`) et les validations de haut niveau qui ne présupposent pas
le format : `NkDetectBOM(data, size)` lit le *Byte Order Mark* en tête de buffer pour deviner
l'encodage d'un fichier qu'on vient de lire (`NK_UNKNOWN` s'il n'y a pas de BOM), et les quatre
`NkIsValidASCII` / `NkIsValidUTF8` / `NkIsValidUTF16` / `NkIsValidUTF32` valident un buffer **avant
de lui faire confiance**.

> **En résumé.** `NkEncoding` = la façade : enums `NkEncodingType`/`NkConversionResult`, détection
> d'encodage par BOM (`NkDetectBOM`), et validations `NkIsValidASCII/UTF8/UTF16/UTF32` à passer
> avant tout stockage. C'est par lui qu'on devine et qu'on vérifie ; les autres traduisent.

---

## Aperçu de l'API

Tous les éléments publics, par sous-namespace. Complexités entre crochets quand utile.

### `nkentseu::encoding` — façade `NkEncoding.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkEncodingType` | `NK_UNKNOWN` / `NK_ASCII` / `NK_UTF8` / `NK_UTF16LE` / `NK_UTF16BE` / `NK_UTF32LE` / `NK_UTF32BE` / `NK_LATIN1`. |
| Enum | `NkConversionResult` | `NK_SUCCESS` / `NK_SOURCE_EXHAUSTED` / `NK_TARGET_EXHAUSTED` / `NK_SOURCE_ILLEGAL`. |
| Détection | `NkDetectBOM(data, size)` `[O(1)]` | Devine l'encodage via le BOM ; `NK_UNKNOWN` si absent. |
| Validation | `NkIsValidASCII(str, length)` `[O(n)]` | Tous les caractères 0–127. |
| Validation | `NkIsValidUTF8(str, length)` `[O(n)]` | UTF-8 bien formé (ni sur-encodage ni surrogate). |
| Validation | `NkIsValidUTF16(str, length)` `[O(n)]` | Paires de surrogates correctes. |
| Validation | `NkIsValidUTF32(str, length)` `[O(n)]` | Codepoints dans la plage Unicode. |

### `nkentseu::encoding::utf8` — `NkUTF8.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Codec | `NkDecodeChar(str, outCp)` `[O(1)]` | Décode 1 codepoint ; renvoie octets consommés (1–4), **0** si invalide. |
| Codec | `NkEncodeChar(cp, outBuffer)` `[O(1)]` | Encode 1 codepoint (buffer ≥ 4 o) ; renvoie octets écrits, 0 si invalide. |
| Longueur | `NkCharLength(firstByte)` `[O(1)]` | Octets attendus (1–4) selon le 1er octet ; 0 si invalide. |
| Longueur | `NkCodepointLength(cp)` `[O(1)]` | Octets requis (1–4) pour encoder ; 0 si hors plage. |
| Parcours | `NkCountChars(str, byteLength)` `[O(n)]` | Compte les codepoints ; s'arrête au 1er octet invalide. |
| Parcours | `NkNextChar(str)` `[O(1)]` | Avance au caractère suivant (`str` si invalide). |
| Parcours | `NkPrevChar(str, start)` `[O(1)]` | Recule, borné par `start` (saute les octets de continuation). |
| Prédicat | `NkIsContinuationByte(byte)` (inline) | Vrai si `(byte & 0xC0) == 0x80`. |
| Validation | `NkIsValid(str, length)` `[O(n)]` | UTF-8 bien formé complet. |
| Validation | `NkIsValidCodepoint(cp)` `[O(1)]` | `0 ≤ cp ≤ 0x10FFFF` et hors `D800–DFFF`. |
| Conversion | `NkToUTF16(src, srcLen, dst, dstLen, bytesRead, charsWritten)` `[O(n)]` | UTF-8 → UTF-16 (surrogates si cp > 0xFFFF). |
| Conversion | `NkToUTF32(src, srcLen, dst, dstLen, bytesRead, charsWritten)` `[O(n)]` | UTF-8 → UTF-32. |
| Conversion | `NkFromUTF16(src, srcLen, dst, dstLen, charsRead, bytesWritten)` `[O(n)]` | UTF-16 → UTF-8. |
| Conversion | `NkFromUTF32(src, srcLen, dst, dstLen, charsRead, bytesWritten)` `[O(n)]` | UTF-32 → UTF-8. |

### `nkentseu::encoding::utf16` — `NkUTF16.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Constantes | `NK_HIGH_SURROGATE_START`/`END`, `NK_LOW_SURROGATE_START`/`END` | Bornes `0xD800`/`0xDBFF` et `0xDC00`/`0xDFFF`. |
| Prédicat | `NkIsHighSurrogate(ch)` (inline) | `0xD800–0xDBFF`. |
| Prédicat | `NkIsLowSurrogate(ch)` (inline) | `0xDC00–0xDFFF`. |
| Prédicat | `NkIsSurrogate(ch)` (inline) | `0xD800–0xDFFF`. |
| Codec | `NkDecodeChar(str, outCp)` `[O(1)]` | Renvoie unités consommées (1 BMP, 2 pair), 0 si erreur. |
| Codec | `NkEncodeChar(cp, outBuffer)` `[O(1)]` | Buffer ≥ 2 unités ; 1 ou 2 unités, 0 si invalide. |
| Longueur | `NkCharLength(firstUnit)` `[O(1)]` | 1 BMP / 2 pair / 0 si unité isolée invalide. |
| Longueur | `NkCodepointLength(cp)` `[O(1)]` | 1 si `< 0x10000`, 2 si `≥ 0x10000`, 0 hors plage. |
| Parcours | `NkCountChars(str, unitLength)` `[O(n)]` | Compte les codepoints. |
| Parcours | `NkNextChar(str)` `[O(1)]` | Avance (`str+1` ou `str+2`). |
| Parcours | `NkPrevChar(str, start)` `[O(1)]` | Recule (gère low → high surrogate). |
| Validation | `NkIsValid(str, length)` `[O(n)]` | Paires bien formées (rejette high isolé, low orphelin). |
| Conversion | `NkToUTF8(src, srcLen, dst, dstLen, charsRead, bytesWritten)` `[O(n)]` | UTF-16 → UTF-8. |
| Conversion | `NkToUTF32(src, srcLen, dst, dstLen, charsRead, unitsWritten)` `[O(n)]` | UTF-16 → UTF-32. |

### `nkentseu::encoding::utf32` — `NkUTF32.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Constante | `NK_MAX_UNICODE` | `0x10FFFF`. |
| Prédicat | `NkIsValidCodepoint(cp)` (inline) | `≤ 0x10FFFF` et hors `D800–DFFF`. |
| Longueur | `NkCharLength(cp)` (inline) | **Toujours 1** (param ignoré). |
| Parcours | `NkCountChars(str, length)` (inline) | **Renvoie `length`** (param `str` ignoré). |
| Parcours | `NkNextChar(str)` (inline) | `str + 1`. |
| Parcours | `NkPrevChar(str, start)` (inline) | `str - 1` (param `start` ignoré). |
| Validation | `NkIsValid(str, length)` `[O(n)]` | Tous codepoints `≤ 0x10FFFF`, hors surrogates. |
| Conversion | `NkToUTF8(src, srcLen, dst, dstLen, charsRead, bytesWritten)` `[O(n)]` | UTF-32 → UTF-8 (1–4 o/cp). |
| Conversion | `NkToUTF16(src, srcLen, dst, dstLen, charsRead, unitsWritten)` `[O(n)]` | UTF-32 → UTF-16 (surrogate si cp ≥ 0x10000). |

### `nkentseu::encoding::ascii` — `NkASCII.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Prédicat | `NkIsASCII` `NkIsControl` `NkIsPrintable` `NkIsWhitespace` (inline) | `< 128` / contrôle / imprimable `32–126` / espace. |
| Prédicat | `NkIsDigit` `NkIsAlpha` `NkIsAlphaNumeric` (inline) | Chiffre / lettre / alphanumérique. |
| Prédicat | `NkIsUpper` `NkIsLower` `NkIsHexDigit` `NkIsPunctuation` (inline) | Majuscule / minuscule / hex / ponctuation. |
| Casse | `NkToUpper(ch)` `NkToLower(ch)` (inline) | Convertit un caractère (sinon inchangé). |
| Casse | `NkToUpperInPlace(str, length)` `NkToLowerInPlace(str, length)` `[O(n)]` | Convertit un **buffer mutable** entier. |
| Numérique | `NkToDigit(ch)` `NkToHexDigit(ch)` (inline) | Caractère → valeur ; **-1** sinon. |
| Numérique | `NkFromDigit(d)` `NkFromHexDigit(d, uppercase=true)` (inline) | Valeur → caractère ; **`'\0'`** sinon. |
| Validation | `NkIsValid(str, length)` `[O(n)]` | Tous les caractères ASCII. |
| Comparaison | `NkCompareIgnoreCase(lhs, rhs, length)` `[O(n)]` | `<0` / `0` / `>0`, insensible à la casse. |
| Comparaison | `NkEqualsIgnoreCase(lhs, rhs, length)` `[O(n)]` | Égalité insensible à la casse. |

### `nkentseu::encoding::base64` — `NkBase64.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Encodage | `NkEncode(data, length)` `[O(n)]` | Buffer binaire → `NkString` Base64 standard. |
| Encodage | `NkEncodeString(str)` `[O(n)]` | `NkStringView` → `NkString` Base64. |
| Encodage | `NkEncodeURLSafe(data, length)` `[O(n)]` | Variante URL-safe (`-`/`_`). |
| Décodage | `NkDecode(base64, out, outLength)` `[O(n)]` | Base64 → `out` fourni ; `false` si invalide. |
| Décodage | `NkDecodeURLSafe(base64, out, outLength)` `[O(n)]` | Décodage URL-safe vers buffer brut. |
| Décodage | `NkDecodeToString(base64)` `[O(n)]` | Base64 → `NkString`. |
| Validation | `NkIsValid(base64)` `[O(n)]` | Vérifie qu'une chaîne est du Base64 valide. |

---

## Référence complète

Les éléments triviaux (prédicats inline, longueurs constantes) sont décrits brièvement ; les
codecs, conversions et idiomes le sont **à fond**, avec leurs usages dans les différents domaines
du temps réel.

### Le contrat universel : valeur de retour = signal d'erreur

Avant tout, un réflexe vaut pour toutes ces fonctions : **un `0` rendu par un codec ou une fonction
de longueur n'est jamais une taille, c'est une erreur**. `NkDecodeChar`, `NkEncodeChar`,
`NkCharLength`, `NkCodepointLength` renvoient `0` dès qu'ils butent sur une séquence invalide — il
faut le tester explicitement avant d'utiliser le résultat. De même, les conversions renvoient un
`NkConversionResult`, jamais un simple booléen, parce que distinguer « buffer trop petit » d'une
« source illégale » change la suite des opérations.

### `NkDecodeChar` / `NkEncodeChar` — le cœur des codecs

Ces deux fonctions (déclinées en `utf8::`, `utf16::`, `utf32` implicite) sont la brique de base de
tout traitement de texte. `NkDecodeChar` lit un codepoint et renvoie le nombre d'unités avalées ;
`NkEncodeChar` fait l'inverse dans un buffer fourni. Leurs usages traversent tout le moteur :

- **UI / 2D** — un moteur de rendu de texte décode caractère par caractère pour aller chercher
  chaque glyphe dans son atlas ; un emoji (cp > 0xFFFF en UTF-16) impose de gérer les paires de
  surrogates, d'où l'importance que `NkDecodeChar` rende `2` et non `1`.
- **IO** — relire un nom de fichier Unicode, parser un `.nkscene` octet par octet sans présupposer
  la longueur des caractères.
- **Gameplay / IA** — afficher un pseudo de joueur multilingue, un dialogue traduit, sans le
  tronquer au milieu d'un caractère multi-octets.
- **GPU** — préparer un tampon de glyphes : on décode en codepoints côté CPU avant d'indexer les
  quads à pousser au shader.

### Les conversions `NkTo*` / `NkFrom*` — traduire entre formats

C'est ici que se joue la vraie valeur du module : faire dialoguer des mondes qui ne parlent pas le
même encodage. La règle des **deux passes** (dimensionner avec `dst=nullptr` → `NK_TARGET_EXHAUSTED`,
puis convertir → `NK_SUCCESS`) est non négociable puisque rien n'alloue à votre place.

- **IO / sérialisation** — un fichier `.nkproj` est en UTF-8 sur disque ; pour l'ouvrir via une API
  Win32 qui veut un `wchar_t*`, on passe par `utf8::NkToUTF16`. À la sauvegarde, le chemin de retour.
- **Plateforme** — `utf16::NkToUTF8` côté Windows, le buffer brut côté Unix : une seule couche de
  texte au-dessus, plusieurs OS dessous.
- **Édition de texte** — convertir vers **UTF-32** (largeur fixe) pour insérer, supprimer ou
  réordonner des caractères sans se battre avec la largeur variable, puis reconvertir en UTF-8.
- **Réseau** — normaliser des chaînes entrantes vers l'UTF-8 du moteur avant de les stocker.

### `NkCharLength`, `NkCodepointLength`, `NkCountChars` — mesurer

Ces fonctions répondent à « combien d'unités » ou « combien de caractères ». `NkCharLength` lit la
longueur d'un caractère depuis sa première unité (utile pour avancer sans décoder le codepoint
complet) ; `NkCodepointLength` dit combien d'unités il faudra pour *encoder* un codepoint donné
(pour pré-dimensionner un buffer) ; `NkCountChars` parcourt et compte les caractères réels — qui
n'a **rien à voir** avec le nombre d'octets en UTF-8/UTF-16. C'est cette distinction qui fait qu'un
champ de saisie limité « à 16 caractères » se mesure en codepoints (`NkCountChars`), pas en octets.

En UTF-32, rappelons-le, `NkCharLength` renvoie **toujours 1** et `NkCountChars` **renvoie
directement `length`** : la largeur fixe rend le comptage gratuit. Ces deux-là ignorent une partie
de leurs paramètres — c'est voulu, pour garder une signature identique aux autres formats.

### `NkNextChar`, `NkPrevChar` — naviguer dans le texte

Avancer ou reculer d'un caractère sans le décoder entièrement : indispensable pour un curseur
d'édition, une sélection, un retour arrière. `NkPrevChar` est le plus subtil — en UTF-8 il saute les
octets de continuation `10xxxxxx`, en UTF-16 il gère le passage d'une *low surrogate* à sa *high
surrogate*, le tout borné par `start` pour ne pas sortir du buffer. L'idiome de remontée :

```cpp
const char* p = end;
while (p > start) p = utf8::NkPrevChar(p, start);   // remonte caractère par caractère
```

- **UI** — déplacer un curseur dans un champ texte d'un caractère à gauche/droite, même au milieu
  d'un emoji.
- **Gameplay** — un effet « machine à écrire » qui révèle un dialogue caractère par caractère.

### Les surrogates `NkUTF16` — `NkIsHighSurrogate` & co

Le talon d'Achille de l'UTF-16 est la *paire de surrogates* : un caractère hors BMP s'y écrit en
deux unités, une *high* (`0xD800–0xDBFF`) suivie d'une *low* (`0xDC00–0xDFFF`). Les prédicats inline
`NkIsHighSurrogate` / `NkIsLowSurrogate` / `NkIsSurrogate`, adossés aux constantes
`NK_HIGH_SURROGATE_START`/`END` et `NK_LOW_SURROGATE_START`/`END`, servent à reconnaître ces unités,
à valider qu'une *high* est bien suivie d'une *low* (et pas isolée), et à détecter une *low*
orpheline — ce que fait `NkIsValid`. Sans ce contrôle, un emoji coupé en deux corrompt l'affichage.

### `NkDetectBOM` & les validations — faire confiance à un buffer

`NkDetectBOM` lit les premiers octets pour deviner l'encodage d'un fichier qu'on vient de charger
(UTF-8, UTF-16 LE/BE, UTF-32 LE/BE…) ; sans BOM, il rend `NK_UNKNOWN` et c'est à l'application de
décider (le moteur suppose alors UTF-8). Les validations `NkIsValid*` sont la **ceinture de
sécurité** avant tout stockage : un texte reçu du réseau, lu d'un fichier inconnu ou tapé par un
utilisateur peut être malformé, et un buffer invalide qui traverse le moteur produit des bugs
diffus. Le réflexe : `if (NkIsValidUTF8(buf, len)) { /* stocker */ }`.

### `NkASCII` à fond — le travail 7 bits

Là où Unicode est lourd, l'ASCII est immédiat. La plupart des fonctions sont **inline `noexcept`**,
donc gratuites : les prédicats `NkIs*` classent un caractère, `NkToUpper`/`NkToLower` changent sa
casse, `NkToDigit`/`NkToHexDigit` (et leurs inverses) font le pont avec les nombres.

- **IO / parsing** — découper une ligne de config, sauter les espaces (`NkIsWhitespace`), lire un
  entier ou une couleur hexadécimale (`NkToHexDigit`) sans dépendre de la locale du système.
- **Gameplay** — interpréter une commande console insensible à la casse via `NkEqualsIgnoreCase`.
- **IO** — comparer une extension de fichier (`".PNG"` vs `".png"`) avec `NkCompareIgnoreCase`.
- **UI** — normaliser une saisie en majuscules avec `NkToUpperInPlace` directement dans le buffer.

`NkToDigit`/`NkToHexDigit` rendent **-1** sur un caractère non numérique, `NkFromDigit`/
`NkFromHexDigit` rendent **`'\0'`** hors plage : ces sentinelles se testent toujours.

### `NkBase64` à fond — binaire dans un canal texte

Le Base64 est le pont entre le **binaire** et les **canaux qui n'acceptent que du texte**. C'est le
seul codec du groupe qui alloue (il rend des `NkString`). Ses domaines :

- **IO / sérialisation** — embarquer une petite texture, un blob de données ou un buffer binaire
  *à l'intérieur* d'un fichier JSON `.nkscene`/`.nkproj` qui, par nature, ne stocke que du texte.
- **Réseau** — transmettre un identifiant binaire ou un jeton dans une URL (`NkEncodeURLSafe`, qui
  remplace `+`/`/` par `-`/`_`) ou un en-tête HTTP.
- **Audio / GPU** — sérialiser un petit échantillon ou un buffer de constantes dans un format texte
  d'échange, quand un format binaire dédié serait disproportionné.

Côté décodage, attention : `NkDecode` n'alloue **pas**, il écrit dans un `out` que vous fournissez
et qu'il faut pré-dimensionner à environ **3/4** de la longueur du Base64 ; `NkDecodeToString`
préfère vous rendre une `NkString` toute faite quand la taille importe peu.

### Le socle commun

- **Aucun allocateur (sauf Base64).** En utf8/utf16/utf32/ascii/encoding, l'appelant possède tous
  les buffers (`dst`, `outBuffer`, `out`). Pas de mémoire NKMemory ici — au contraire du reste de
  NKContainers. Seul `NkBase64` alloue, via l'allocateur interne de `NkString`.
- **Toujours qualifier le namespace.** `NkIsValid` existe dans **cinq** namespaces distincts
  (`ascii`, `utf8`, `utf16`, `utf32`, `base64`), et `NkIsValidCodepoint` à la fois dans `utf8::`
  (exporté) et `utf32::` (inline) — sémantique identique, entités distinctes. Écrivez
  `utf8::NkIsValid(...)`, jamais `NkIsValid(...)` seul.
- **Paramètres ignorés en UTF-32.** `utf32::NkCharLength`, `NkCountChars` et `NkPrevChar` ignorent
  certains paramètres (présents pour l'uniformité d'API) — ne vous appuyez pas sur `str`/`start`
  pour ces trois.
- **Cross-platform `wchar_t`.** Sur Windows, traiter `wchar_t*` comme `uint16*` (UTF-16,
  `utf16::NkIsValid`) ; sur Unix, comme `uint32*` (UTF-32, `NkIsValidUTF32`).

---

### Exemple

```cpp
#include "NKContainers/NKContainers.h"
using namespace nkentseu::encoding;

// 1. Deviner l'encodage d'un fichier chargé, puis valider avant de l'utiliser.
NkEncodingType enc = NkDetectBOM(fileData, fileSize);
if (NkIsValidUTF8(text, textLen)) { /* sûr à stocker */ }

// 2. Parcourir un UTF-8 caractère par caractère (range, gameplay, UI).
usize pos = 0; uint32 cp;
while (pos < textLen) {
    usize consumed = utf8::NkDecodeChar(text + pos, cp);
    if (consumed == 0) break;            // séquence invalide
    // ... router cp vers un atlas de glyphes ...
    pos += consumed;
}

// 3. UTF-8 -> UTF-16 (API Windows) en deux passes.
usize read, needed;
auto r = utf8::NkToUTF16(text, textLen, nullptr, 0, read, needed);   // dimensionner
if (r == NkConversionResult::NK_TARGET_EXHAUSTED) {
    uint16* wide = /* allouer 'needed' unités */;
    utf8::NkToUTF16(text, textLen, wide, needed, read, needed);      // convertir
}

// 4. ASCII : parser une commande console insensible à la casse.
if (ascii::NkEqualsIgnoreCase(input, "QUIT", 4)) { /* ... */ }

// 5. Base64 : embarquer un blob binaire dans un JSON, puis le relire.
NkString b64 = base64::NkEncode(blob, blobSize);
// ... plus tard ...
usize outLen = /* ~3/4 de b64.Size() */;
base64::NkDecode(b64, outBuffer, &outLen);
```

---

[← Index NKContainers](README.md) · [Récap NKContainers](../NKContainers.md) · [Strings →](Strings.md)
