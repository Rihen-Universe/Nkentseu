# Les opérations mémoire

> Couche **Foundation** · NKMemory · Manipuler des **octets** et des **objets** sur de la mémoire
> brute : copier, déplacer, remplir, comparer, chercher, transformer, aligner, construire et
> détruire — plus les variantes **SIMD** pour les gros volumes.

Commençons par lever une ambiguïté que le nom du fichier entretient : `NkFunction.h` **ne contient
pas** un équivalent de `std::function`. Malgré son nom, c'est la bibliothèque d'**opérations sur la
mémoire** du moteur — copier, déplacer, remplir, comparer, chercher, transformer des blocs d'octets,
et construire ou détruire des objets sur de la mémoire déjà réservée. Si vous cherchez à stocker un
callable, ce n'est **pas** ici (c'est du côté de NKContainers) ; ici, on manipule des octets et des
slots.

Pourquoi une couche maison plutôt que `memcpy`/`memset` directement ? Pour deux raisons. D'abord,
ces fonctions choisissent automatiquement la meilleure implémentation disponible — elles s'appuient
sur les primitives de `NkUtils.h` (`NkMemCopy`, `NkMemSet`…) qui peuvent recourir à AVX2, au
*streaming store* ou au *prefetch* selon le processeur détecté. Ensuite, elles offrent des variantes
**bornées** qui vérifient les tailles : un filet de sécurité qui transforme un débordement
silencieux en copie tronquée plutôt qu'en corruption mémoire. Toutes gèrent `nullptr` et `size == 0`
de façon sûre, et **toutes sont `noexcept`**.

Trois headers se partagent le sujet, du plus bas au plus haut niveau : `NkUtils.h` pose les
primitives et l'alignement, `NkFunction.h` enveloppe le tout en API riche (offsets bornés, recherche,
transformations, templates typés), et `NkFunctionSIMD.h` ajoute des variantes vectorisées
interchangeables.

- **Namespace** : `nkentseu::memory` (alias court `nkentseu::mem`)
- **Headers** : `#include "NKMemory/NkUtils.h"` · `#include "NKMemory/NkFunction.h"` ·
  `#include "NKMemory/NkFunctionSIMD.h"`

---

## Copier, déplacer, remplir, comparer

Le quatuor de base se lit comme du C familier, en plus sûr :

```cpp
using namespace nkentseu::memory;   // ou l'alias court : nkentseu::mem

NkCopy(dst, src, n);       // copie n octets (zones NON chevauchantes)
NkMove(dst, src, n);       // déplace n octets (chevauchement autorisé)
NkSet(dst, value, n);      // remplit n octets avec un octet (value tronquée à 8 bits)
NkZero(dst, n);            // met n octets à zéro
if (NkCompare(a, b, n) == 0) { /* identiques */ }
```

La distinction entre `NkCopy` et `NkMove` est la même qu'entre `memcpy` et `memmove` : `NkCopy`
suppose que la source et la destination ne se chevauchent **pas** (et va plus vite à cette
condition), tandis que `NkMove` gère correctement le cas où elles se recouvrent. En cas de doute,
`NkMove`. Tous deux retournent `dst`, comme leurs cousins C — pratique pour chaîner. `NkCompare`
renvoie un entier négatif / nul / positif, jamais un simple booléen.

Chacune de ces fonctions a une variante **avec offsets et tailles** qui ajoute un contrôle de
bornes. Au lieu de faire confiance aveuglément aux pointeurs, on passe les tailles totales des
tampons, et la fonction ne copie (ou ne compare) que ce qui tient réellement
(`min(place_dispo_dst, dispo_src, taille_demandée)`) :

```cpp
// copie au plus 'size' octets, sans jamais déborder ni de dst ni de src
NkCopy(dst, dstSize, dstOffset, src, srcSize, srcOffset, size);
```

Un dernier mot sur l'effacement. `NkZero` (alias de `NkMemZero`) met un bloc à zéro, mais un
compilateur a le droit de le *supprimer* s'il juge que le tampon n'est plus relu ensuite — une
optimisation légitime, sauf quand le tampon contenait un secret (mot de passe, clé) qu'on voulait
réellement effacer. `NkSecureZero` emploie un accès `volatile` pour garantir que l'effacement a bien
lieu.

> **En résumé.** Le quatuor `NkCopy` / `NkMove` / `NkSet` / `NkCompare` couvre l'essentiel ; chacun a
> une variante bornée à offsets qui tronque plutôt que de déborder. En cas de chevauchement possible,
> `NkMove`. Pour effacer un secret, `NkSecureZero` (pas `NkZero`, qui peut être optimisé).

---

## Chercher, analyser, transformer

Au-delà du quatuor de base, le module sait fouiller et remodeler un bloc. `NkFind` et `NkFindLast`
localisent la première (ou dernière) occurrence d'un octet ; `NkSearchPattern` cherche une
sous-séquence entière (le *needle* dans le *haystack*) ; `NkFindDifference` renvoie l'indice où deux
blocs commencent à diverger. Quelques prédicats complètent l'inventaire : `NkIsZero` (le bloc est-il
entièrement nul ?), `NkOverlaps` (deux blocs se chevauchent-ils ?), `NkEqualsPattern` (le bloc
commence-t-il par tel motif ?), et `NkChecksum` pour une somme de contrôle 32 bits rapide.

Côté remodelage : `NkReverse` inverse l'ordre des octets, `NkRotate` effectue une rotation
circulaire, `NkTransform` applique une fonction octet par octet, et `NkSwapEndian` échange le
**boutisme** — indispensable dès qu'on lit ou écrit des données binaires venant d'une autre
plateforme (fichiers, réseau) :

```cpp
NkSwapEndian(&value, sizeof(value), 1);   // big-endian <-> little-endian
```

Ces fonctions évitent de réécrire à la main des boucles d'octets — boucles qui, en plus d'être
verbeuses, ratent souvent les optimisations vectorielles que la version maison applique.

> **En résumé.** Recherche d'octet (`NkFind`/`NkFindLast`) ou de motif (`NkSearchPattern`),
> diagnostic (`NkFindDifference`, `NkIsZero`, `NkOverlaps`, `NkEqualsPattern`, `NkChecksum`), et
> remodelage (`NkReverse`, `NkRotate`, `NkSwapEndian`, `NkTransform`) — sans réécrire de boucle
> d'octets à la main.

---

## Construire et détruire sur de la mémoire brute

Les fonctions précédentes manipulent des octets. Les suivantes manipulent des **objets** posés sur
de la mémoire déjà allouée — c'est la brique qu'utilisent les conteneurs et les allocateurs pour
séparer *réserver la mémoire* de *y construire un objet*.

```cpp
T* slot = static_cast<T*>(alloc.Allocate(sizeof(T)));
NkConstruct<T>(slot, arg1, arg2);   // placement-new typé : construit EN PLACE, n'alloue rien
// ...
NkDestroy(slot);                    // appelle ~T() — la mémoire reste à libérer à part
```

`NkConstruct` est un placement-new typé avec *perfect forwarding* (c'est la seule fonction du lot qui
n'est **pas** `noexcept`, puisque le constructeur de `T` peut lever) ; `NkDestroy` appelle le
destructeur **sans** rendre la mémoire et n'appelle jamais `delete`. À côté, `NkSwap` échange deux
objets, avec des variantes spécialisées (`NkSwapPtr`, `NkSwapTrivial`) selon le type.

> **En résumé.** `NkConstruct` / `NkDestroy` = le pont entre mémoire brute et objets : l'un construit
> en place sans allouer, l'autre détruit sans libérer. La symétrie *allouer↔libérer* reste à votre
> charge, comme tout `Create`↔`Destroy` du moteur.

---

## Aligner et dupliquer

Beaucoup d'API bas niveau — surtout le GPU et le SIMD — exigent des pointeurs **alignés**.
`NkIsPowerOfTwo` et `NkIsAlignedPtr` testent ces conditions ; `NkAlignPointer` calcule (ou avance)
un pointeur aligné ; `NkAllocAligned` / `NkFreeAligned` allouent et libèrent un bloc aligné via
l'allocateur du moteur. Pour dupliquer un bloc d'un coup (allocation + copie), `NkDuplicate` ; pour
réserver de la **mémoire virtuelle** brute, `NkMap` / `NkUnmap`.

```cpp
void* buf = NkAllocAligned(64, size);   // aligné 64 octets (cache line / AVX-512)
// ...
NkFreeAligned(buf, size);               // jamais std::free !
```

La règle d'or de NKMemory s'applique partout ici : ce qui sort de `NkAlloc` / `NkAllocAligned` /
`NkDuplicate` / `NkMap` se libère **uniquement** par les fonctions jumelles de NKMemory — jamais par
`std::free` ni `delete[]`, sous peine de corruption de tas Windows (`c0000374`).

> **En résumé.** `NkAllocAligned`↔`NkFreeAligned`, `NkMap`↔`NkUnmap`, `NkDuplicate`/`NkAlloc`↔`NkFree`
> : symétrie obligatoire, allocateur custom jamais mélangé au heap CRT.

---

## Quand la performance prime : les variantes SIMD

`NkFunctionSIMD.h` propose des versions vectorisées des opérations les plus chaudes —
`NkMemoryCopySIMD`, `NkMemorySetSIMD`, `NkMemorySearchPatternSIMD`, `NkMemoryHashSIMD`,
`NkMemoryTransposeSIMD`… Le backend (AVX-512 / AVX2 / SSE2 / NEON / scalaire) est choisi
**automatiquement** selon le processeur ; elles brillent sur de **gros** volumes traités en boucle
serrée (transformées de particules, traitement audio, hachage de blocs).

En pratique, ne les sortez que pour les hot-paths mesurés, et **alignez** vos tampons (par exemple
`NkAllocAligned(64, …)`, ou `NkAlignSIMD` qui aligne sur 64 octets). Pour le cas courant, les
fonctions de base (`NkCopy`, `NkSet`…) choisissent déjà l'implémentation matérielle disponible — vous
n'avez rien à faire pour en profiter.

> **En résumé.** SIMD = gros volumes alignés uniquement ; en dessous de ~64 octets l'overhead dépasse
> le gain (fallback scalaire automatique pour `NkMemoryCopySIMD`). Attention : les versions `*SIMD`
> retournent `void`, là où leurs cousines scalaires retournaient `void*`.

---

## Aperçu de l'API

La liste de **tous** les éléments publics, en un coup d'œil. Complexités et particularités entre
crochets. Tout est détaillé dans la « Référence complète » qui suit.

### `NkUtils.h` — primitives et alignement

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Alignement | `NkIsPowerOfTwo(value)` `[O(1), inline, nodiscard]` | `value` est-il une puissance de 2 ? |
| Alignement | `NkIsAlignedPtr(ptr, align)` `[O(1), inline, nodiscard]` | `ptr` aligné sur `align` ? (`true` si `align ≤ 1`). |
| Mémoire | `NkMemSet(dst, value, size)` `[O(n)]` | Remplit `size` octets ; retourne `dst`. |
| Mémoire | `NkMemCopy(dst, src, size)` `[O(n)]` | Copie non chevauchante (AVX2/streaming possible). |
| Mémoire | `NkMemMove(dst, src, size)` `[O(n)]` | Copie chevauchement autorisé. |
| Mémoire | `NkMemCompare(a, b, size)` `[O(n)]` | Compare ; `<0` / `0` / `>0`. |
| Mémoire | `NkMemZero(dst, size)` `[O(n), inline]` | Met à zéro (= `NkMemSet(dst, 0, size)`). |

### `NkFunction.h` — wrappers et opérations typées (toutes `noexcept`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Base | `NkCopy(dst, src, size)` / surcharge à offsets bornés `[O(n)]` | Copie non chevauchante ; retourne `dst`. |
| Base | `NkMove(dst, src, size)` / surcharge à offsets bornés `[O(n)]` | Copie chevauchement autorisé. |
| Base | `NkSet(dst, value, size)` / surcharge à offset borné `[O(n)]` | Remplit (`value` tronquée 8 bits). |
| Base | `NkCompare(a, b, size)` (+ 2 surcharges bornées) `[O(n)]` | Compare `<0`/`0`/`>0`. |
| Recherche | `NkFind(ptr, value, size)` `[O(n)]` | 1ʳᵉ occurrence d'un octet, ou `nullptr`. |
| Recherche | `NkFindLast(buffer, size, value)` `[O(n)]` | Dernière occurrence (ordre params différent !). |
| Recherche | `NkSearchPattern(buf, size, pat, patSize)` `[O(n·m)]` | 1ʳᵉ occurrence d'un motif, ou `nullptr`. |
| Recherche | `NkFindLastPattern(buf, size, pat, patSize)` `[O(n·m)]` | Dernière occurrence d'un motif. |
| Analyse | `NkFindDifference(p1, p2, count)` `[O(n)]` | Index de la 1ʳᵉ différence, ou `count`. |
| Analyse | `NkIsZero(ptr, count)` `[O(n)]` | Bloc entièrement nul ? |
| Analyse | `NkOverlaps(p1, p2, count)` `[O(1)]` | Deux régions se chevauchent ? |
| Analyse | `NkEqualsPattern(buf, size, pat, patSize)` `[O(patSize)]` | `buf` commence-t-il par `pat` ? |
| Analyse | `NkValidateMemory(buf, size)` | Test défensif d'accessibilité (`volatile`). |
| Analyse | `NkChecksum(data, size)` `[O(n)]` | Checksum 32 bits FNV-1a (non crypto). |
| Transformation | `NkReverse(buffer, size)` `[O(n/2)]` | Inverse l'ordre des octets in-place. |
| Transformation | `NkRotate(buffer, size, offset)` `[O(n)]` | Rotation circulaire (triple-reverse). |
| Transformation | `NkFill(dst, size, value)` `[O(n)]` | Alias de `NkSet` (ordre params inversé !). |
| Transformation | `NkTransform(dst, src, size, func)` `[O(n)]` | Applique `int32(*)(int32)` octet par octet. |
| Transformation | `NkConditionalSet(dst, mask, values, size)` `[O(n)]` | `dst[i] = values[i]` si `mask[i]`. |
| Transformation | `NkSwapEndian(buf, elemSize, count)` `[O(elemSize·count)]` | Inverse le boutisme par élément. |
| Mémoire | `NkDuplicate(src, size)` | Alloue (`NkAlloc`) + copie ; à libérer par `NkFree`. |
| Mémoire | `NkSecureZero(ptr, size)` `[O(n)]` | Zéro anti-optimisation (`volatile`). |
| Mémoire | `NkZero(ptr, size)` `[O(n)]` | Alias de `NkMemZero`. |
| Alignement | `NkAllocAligned(align, size)` | Bloc aligné via `NkAlloc`, ou `nullptr`. |
| Alignement | `NkFreeAligned(ptr, size)` | Libère un bloc de `NkAllocAligned`. |
| Alignement | `NkPosixAligned(&ptr, align, size)` | Style `posix_memalign` : `0` succès / `-1` échec. |
| Alignement | `NkAlignPointer(align, size, ptr&, space&)` `[O(1)]` | Aligne **in-place** (mute `ptr`/`space`). |
| Alignement | `NkAlignPointer(ptr, align, size)` `[O(1)]` | Aligne **pur** (retourne, ne mute pas). |
| Mémoire virtuelle | `NkMap(size)` / `NkUnmap(ptr, size)` | mmap/VirtualAlloc via l'allocateur virtuel. |
| Template | `NkSwap(a, b)` / `NkSwap(ptr&, ptr&)` | Échange par move / échange des pointeurs. |
| Template | `NkSwapPtr(a, b)` | Échange `*a`/`*b` (safe si `nullptr`/égaux). |
| Template | `NkSwapTrivial(a, b)` | Échange byte-wise POD (`static_assert` trivial). |
| Template | `NkConstruct<T>(ptr, args…)` **[non `noexcept`]** | Placement-new typé ; n'alloue pas. |
| Template | `NkDestroy<T>(ptr)` | Appelle `~T()` ; ne libère pas. |

### `NkFunctionSIMD.h` — variantes SIMD (toutes `noexcept`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Alignement | `NkAlignSIMD(ptr)` `[O(1), inline]` | Aligne vers le haut sur **64 octets**. |
| Alignement | `NkIsAlignedSIMD(ptr)` `[O(1), inline]` | Aligné sur 64 octets ? |
| Mémoire | `NkMemoryCopySIMD(dst, src, bytes)` | Copie SIMD ; **retourne `void`**. |
| Mémoire | `NkMemoryMoveSIMD(dst, src, bytes)` | Déplacement SIMD avec détection chevauchement. |
| Mémoire | `NkMemorySetSIMD(dst, value, bytes)` | Remplissage broadcast (~20×+ memset >1 Ko). |
| Mémoire | `NkMemoryCopyTypeSIMD<Dst,Src>(dst, src, count)` | Copie avec conversion de type (`#pragma omp simd`). |
| Mémoire | `NkMemoryTransposeSIMD(dst, src, rows, cols, elemSize)` | Transposition matrice (in-place non supporté). |
| Avancé | `NkMemoryCompareConstantTime(a, b, bytes)` | Comparaison temps constant (anti-timing). |
| Avancé | `NkMemorySearchPatternSIMD(hay, haySize, ndl, ndlSize)` | Recherche Boyer-Moore + SIMD. |
| Avancé | `NkMemoryCRC32SIMD(data, bytes)` | CRC32 (`0xEDB88320`), table lazy thread-safe. |
| Avancé | `NkMemoryHashSIMD(data, bytes)` | Hash 64 bits type xxHash (non crypto). |

---

## Référence complète

Chaque élément est repris ici en détail. Les triviaux sont décrits brièvement ; les opérations
importantes le sont **à fond**, avec leurs usages dans les différents domaines du temps réel — rendu,
ECS, physique, animation, gameplay/IA, audio, UI/2D, IO, GPU. La règle qui chapeaute tout : dans le
moteur, **pas de `new`/`delete` bruts**, et toute fonction qui réserve (allocation, *map*,
*duplicate*, construction) a sa jumelle de libération.

### Les primitives de `NkUtils.h`

Ce header est le **socle** : `NkFunction.h` n'est qu'une couche de wrappers au-dessus. `NkMemSet`,
`NkMemCopy`, `NkMemMove`, `NkMemCompare` sont les versions « nues » de la copie/comparaison, en
`O(n)`, susceptibles d'utiliser AVX2, le *streaming store* ou le *prefetch* selon le processeur
détecté (via `NKPlatform/NkArchDetect.h`). `NkMemZero` est leur cas particulier le plus courant (mise
à zéro). En général, on n'appelle pas ces primitives directement — on passe par `NkCopy`/`NkSet`/… de
`NkFunction.h`, qui ajoutent les variantes bornées et le confort. On les croise surtout dans le code
des allocateurs et des conteneurs eux-mêmes.

`NkIsPowerOfTwo` et `NkIsAlignedPtr` sont deux prédicats `O(1)` *inline* : le premier vérifie qu'une
valeur est une puissance de 2 (toute valeur d'alignement valide l'est), le second qu'un pointeur
respecte un alignement donné. On les retrouve partout où l'alignement compte :

- **GPU** : valider qu'un tampon d'upload respecte l'alignement requis par l'API (256 octets côté
  DX12 pour les *constant buffers*, par exemple) avant de mapper.
- **Audio** : confirmer qu'un buffer de samples est aligné SIMD avant un traitement vectorisé.
- **Mémoire / allocateurs** : un *arena* ou un *pool* contrôle ses invariants d'alignement à chaque
  réservation.

### `NkCopy`, `NkMove`, `NkSet`, `NkCompare` — le quatuor de base

Ce sont les opérations les plus utilisées du module. `NkCopy` délègue à `NkMemCopy` (zones **non**
chevauchantes, plus rapide), `NkMove` à `NkMemMove` (chevauchement géré). `NkSet` remplit avec un
octet (la valeur passée est tronquée à 8 bits). `NkCompare` renvoie le signe de la différence
lexicographique. Les trois premiers retournent `dst`.

La force du module, ce sont les **surcharges à offsets bornés** : on passe les tailles totales des
tampons et des décalages, et la fonction ne traite que `min(dispo_dst, dispo_src, taille_demandée)`
octets — un débordement devient une copie tronquée, pas une corruption. `NkCompare` a même trois
formes (simple, deux tailles, deux tailles + deux offsets). Domaines :

- **Rendu / GPU** : assembler un *vertex buffer* CPU avant l'upload, recopier un sous-rectangle de
  texture, comparer deux *snapshots* d'uniformes pour éviter un upload inutile.
- **ECS** : déplacer un *component* d'un archétype à un autre lors d'une transition d'entité ;
  compacter un tableau de composants après suppression (le `NkMove` borné évite de déborder le
  tableau cible).
- **Physique / animation** : copier des poses, des matrices de *skinning*, des états de
  *broadphase* d'une frame à l'autre.
- **Audio** : remplir un buffer de silence (`NkSet(dst, 0, n)`), recopier un bloc de samples d'un
  *ring buffer* vers le mixeur.
- **IO / sérialisation** : `NkCompare` pour vérifier une signature de fichier (*magic bytes*),
  `NkCopy` borné pour extraire un champ d'un en-tête sans déborder le buffer lu.

`NkZero` (alias de `NkMemZero`) et **surtout** `NkSecureZero` complètent l'effacement : le second
emploie un accès `volatile` que le compilateur n'a pas le droit d'élider — réservez-le aux secrets
(clés, tokens, mots de passe) dont l'effacement doit réellement avoir lieu.

### `NkFind`, `NkFindLast`, `NkSearchPattern`, `NkFindLastPattern` — la recherche

`NkFind` / `NkFindLast` localisent un **octet** (première ou dernière occurrence), en `O(n)` ;
`NkSearchPattern` / `NkFindLastPattern` cherchent un **motif** entier, en `O(n·m)` naïf (préférez la
variante SIMD Boyer-Moore pour les longs motifs). **Piège d'ordre des paramètres** : `NkFind(ptr,
value, size)` mais `NkFindLast(buffer, size, value)` — la valeur change de place. Domaines :

- **IO / parsing** : trouver un séparateur dans un buffer (`\n`, `\0`), localiser une balise ou un
  *chunk* dans un fichier binaire (PNG, WAV, glTF) via `NkSearchPattern`.
- **Réseau** : repérer un délimiteur de trame dans un flux entrant.
- **Rendu / assets** : scanner un blob de shader compilé à la recherche d'une section.

### `NkFindDifference`, `NkIsZero`, `NkOverlaps`, `NkEqualsPattern`, `NkValidateMemory`, `NkChecksum` — analyse

Petits utilitaires de diagnostic. `NkFindDifference` renvoie l'index de la première divergence entre
deux blocs (ou `count` s'ils sont identiques) — idéal pour un *diff* binaire, détecter qu'un
*component* a changé (dirty-tracking ECS), ou trouver où deux frames d'animation se mettent à
différer. `NkIsZero` teste qu'un bloc est entièrement nul (plus rapide qu'un `NkCompare` contre un
buffer de zéros sur les petits buffers) : vérifier qu'une zone fraîchement allouée est propre, qu'un
masque est vide. `NkOverlaps` (`O(1)`) dit si deux régions se recouvrent — un garde-fou avant de
choisir entre `NkCopy` et `NkMove`. `NkEqualsPattern` teste un préfixe (*magic bytes* d'un format
de fichier). `NkValidateMemory` est un test défensif d'accessibilité (via `volatile`) qui **ne
garantit pas** la sécurité — un simple filet. `NkChecksum` produit une somme FNV-1a 32 bits **non
cryptographique**, parfaite pour détecter une corruption d'asset au chargement, mais jamais pour la
sécurité.

### `NkReverse`, `NkRotate`, `NkFill`, `NkTransform`, `NkConditionalSet`, `NkSwapEndian` — transformations

`NkReverse` inverse l'ordre des octets in-place (`O(n/2)` swaps). `NkRotate` fait une rotation
circulaire par l'algorithme triple-reverse, `O(n)` en temps et `O(1)` en espace (offset positif =
droite, négatif = gauche). `NkFill` est un **alias sémantique** de `NkSet` — mais **attention à
l'ordre** : `NkFill(dst, size, value)` contre `NkSet(dst, value, size)`. `NkTransform` applique un
pointeur de fonction `int32(*)(int32)` octet par octet (in-place autorisé, `dst == src`) — utile pour
un *toupper*/*tolower* maison ou une table de correspondance. `NkConditionalSet` copie sélectivement
selon un masque (`if (mask[i]) dst[i] = values[i]`).

`NkSwapEndian` mérite une mention à part : il inverse le **boutisme** de chaque élément (`elementSize`
typiquement 2/4/8) sur `count` éléments — la brique de toute conversion big↔little-endian :

- **IO / sérialisation** : lire un format big-endian (PNG, certains WAV/AIFF, formats réseau) sur une
  machine little-endian.
- **Réseau** : convertir les champs d'un en-tête de paquet (*network byte order*).
- **Rendu / assets** : importer un *mesh* ou une texture dont les entiers sont stockés dans l'autre
  boutisme.

```cpp
NkSwapEndian(&value, sizeof(value), 1);   // big-endian <-> little-endian
```

### `NkConstruct`, `NkDestroy` — objets sur mémoire brute

Voici le pont entre les octets et les **objets**. `NkConstruct<T>(ptr, args…)` est un placement-new
typé avec *perfect forwarding* : il construit un `T` **dans** la mémoire pointée, sans rien allouer,
et retourne `ptr` casté (ou `nullptr` si `ptr` l'était). C'est la **seule** fonction du lot qui n'est
pas `noexcept` — logique, puisque le constructeur de `T` peut lever. `NkDestroy<T>(ptr)` appelle
`ptr->~T()` et **n'appelle jamais `delete`** : la mémoire reste à libérer séparément (no-op si
`nullptr`).

```cpp
alignas(MyClass) nk_uint8 storage[sizeof(MyClass)];
MyClass* obj = NkConstruct<MyClass>(reinterpret_cast<MyClass*>(storage), arg1, arg2);
// ...
NkDestroy(obj);   // appelle ~MyClass(), ne libère PAS storage
```

C'est exactement ce que font, en interne, les conteneurs (`NkVector` construit ses éléments dans la
capacité réservée) et les allocateurs (un *pool* construit puis détruit ses slots sans réallouer).
La **symétrie** `NkConstruct` ↔ `NkDestroy` reflète la règle générale du moteur : toute classe avec
`Create` doit avoir `Destroy`, et toute mémoire réservée doit être rendue par la fonction jumelle.
Domaines :

- **ECS** : construire un *component* dans le stockage d'archétype, le détruire lors du retrait de
  l'entité, sans jamais réallouer le tableau.
- **Rendu** : recycler des *command buffers* ou des descripteurs dans un *pool* (construct/destroy
  sans toucher l'allocation).
- **Gameplay** : *object pool* d'entités/projectiles réutilisées frame après frame.

### `NkSwap`, `NkSwapPtr`, `NkSwapTrivial` — échanges typés

Quatre formes pour échanger deux objets. `NkSwap(a, b)` échange par *move semantics* ; une surcharge
`NkSwap(ptr&, ptr&)` échange les **pointeurs eux-mêmes** (pas les valeurs pointées). `NkSwapPtr(a,
b)` échange `*a` et `*b` de façon **sûre** (no-op si l'un est `nullptr` ou si `a == b`).
`NkSwapTrivial(a, b)` fait un échange byte-wise pour les types POD (`static_assert` de trivialité),
plus rapide quand le type s'y prête. On les croise dans les tris, les *swap-and-pop* de tableaux
(ECS, listes d'entités), l'échange de *back/front buffers* logiques.

### `NkAllocAligned`, `NkFreeAligned`, `NkPosixAligned`, `NkAlignPointer`, `NkDuplicate`, `NkMap`, `NkUnmap` — allocation et alignement

`NkAllocAligned(align, size)` réserve un bloc aligné (alignement puissance de 2) via l'allocateur du
moteur ; `NkFreeAligned` le rend. `NkPosixAligned` offre la même chose à la mode `posix_memalign`
(écrit dans `*ptr`, retourne `0`/`-1`). `NkDuplicate(src, size)` alloue **et** copie en un appel — le
buffer retourné est à libérer par `NkFree`. `NkMap` / `NkUnmap` réservent et rendent de la **mémoire
virtuelle** brute (mmap/VirtualAlloc) pour les très gros blocs.

`NkAlignPointer` existe en **deux saveurs** à ne pas confondre :

- la surcharge **in-place** `NkAlignPointer(align, size, ptr&, space&)` ajuste `ptr` et **réduit**
  `space` de l'ajustement, retournant `true` si l'alignement tient — c'est le pattern d'allocation
  dans un *arena* pré-alloué (on aligne, on réserve `size`, on avance) ;
- la surcharge **pure** `NkAlignPointer(ptr, align, size)` calcule un pointeur aligné **sans** muter
  l'original, et retourne `nullptr` si c'est impossible.

Domaines : tout le GPU (constant buffers, vertex/index buffers, *staging* alignés), le SIMD (buffers
audio et de particules alignés 16/32/64), les *arenas* et *pools* de l'allocateur. **Règle dure** :
ce qui sort de `NkAllocAligned` / `NkDuplicate` / `NkMap` se libère **uniquement** par
`NkFreeAligned` / `NkFree` / `NkUnmap`. Mélanger l'allocateur custom et `std::free` / `delete[]`
provoque une corruption de tas Windows (`c0000374`).

### Les variantes SIMD de `NkFunctionSIMD.h`

API **interchangeable** avec celle de `NkFunction.h` pour une substitution transparente, mais le
backend (AVX-512 / AVX2 / SSE2 / NEON / scalaire) est choisi automatiquement dans le `.cpp`. Définir
`NKENTSEU_DISABLE_SIMD` rebascule tout vers les versions scalaires. Les helpers `NkAlignSIMD` (aligne
vers le haut sur 64 octets — peut avancer jusqu'à +63, prévoir la marge) et `NkIsAlignedSIMD`
encadrent l'alignement requis pour la performance maximale.

- **`NkMemoryCopySIMD` / `NkMemoryMoveSIMD` / `NkMemorySetSIMD`** — copie/déplacement/remplissage
  vectorisés (~20×+ memset pour > 1 Ko). `NkMemoryMoveSIMD` détecte le chevauchement (sens
  forward/backward) et coûte donc un peu plus que `CopySIMD` à l'absence de chevauchement. **Toutes
  retournent `void`**, contrairement à leurs cousines scalaires qui retournaient `void*`. Usage :
  upload massif vers le GPU, copie de gros blocs audio, *blit* de framebuffers logiciels.
- **`NkMemoryCopyTypeSIMD<Dst,Src>`** — copie **avec conversion de type** (`static_cast` par élément,
  boucle annotée `#pragma omp simd`). Pour convertir un tableau `int16` audio en `float`, des
  positions `double` en `float` pour le GPU.
- **`NkMemoryTransposeSIMD`** — transposition de matrice / réorganisation **SOA↔AOS** (in-place non
  supporté). Pour passer un tableau de structures à une structure de tableaux (data-oriented), ou
  transposer des blocs en traitement d'image.
- **`NkMemoryCompareConstantTime`** — comparaison à **temps constant**, résistante aux *timing
  attacks* : le temps ne dépend pas de la position des différences. Réservée aux mots de passe,
  tokens, signatures (réseau / sécurité).
- **`NkMemorySearchPatternSIMD`** — recherche **Boyer-Moore + SIMD**, ~100×+ le naïf pour des motifs
  > 16 octets (au prix d'un pré-traitement). Pour scanner de gros assets ou flux.
- **`NkMemoryCRC32SIMD`** — CRC32 (polynôme `0xEDB88320`, table lazy thread-safe), ~50×+ scalaire au
  delà de 1 Ko : intégrité d'assets, de paquets réseau.
- **`NkMemoryHashSIMD`** — hash 64 bits type xxHash, **non cryptographique** : clé de table de
  hachage, *content addressing* d'assets, *dirty hash* d'un buffer.

En pratique : SIMD **uniquement** sur de gros volumes alignés et mesurés. En dessous de ~64 octets,
l'overhead dépasse le gain (le fallback scalaire est automatique pour `NkMemoryCopySIMD`).

---

### Exemple récapitulatif

```cpp
#include "NKMemory/NkFunction.h"
#include "NKMemory/NkFunctionSIMD.h"
using namespace nkentseu::memory;

// 1. Quatuor de base + variante bornée
nk_uint8 header[16];
NkCopy(header, file.data(), file.size(), 0, /*src*/file.ptr, file.bytes, 0, 16);  // tronqué, jamais débordé
if (NkEqualsPattern(header, 16, "\x89PNG", 4)) { /* c'est un PNG */ }

// 2. Construire / détruire un objet sur de la mémoire brute (pattern pool)
alignas(Sound) nk_uint8 slot[sizeof(Sound)];
Sound* s = NkConstruct<Sound>(reinterpret_cast<Sound*>(slot), clip, volume);
// ...
NkDestroy(s);                       // ~Sound(), la mémoire 'slot' reste à part

// 3. Endianness à la lecture d'un format big-endian
nk_uint32 length = ReadRaw32();
NkSwapEndian(&length, sizeof(length), 1);

// 4. Bloc aligné + copie SIMD (hot-path mesuré)
void* buf = NkAllocAligned(64, frame.bytes);    // marge d'alignement SIMD
NkMemoryCopySIMD(buf, frame.ptr, frame.bytes);  // retourne void
NkFreeAligned(buf, frame.bytes);                // jamais std::free !

// 5. Effacer un secret sans qu'il soit optimisé
NkSecureZero(token, tokenLen);
```

---

[← Les smart pointers](SmartPointers.md) · [Index NKMemory](README.md) · [Récap NKMemory](../NKMemory.md) · [Suivi & profilage →](Tracking-Profiling.md)
