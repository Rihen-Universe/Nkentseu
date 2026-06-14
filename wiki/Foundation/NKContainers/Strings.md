# Les chaînes de caractères

> Couche **Foundation** · NKContainers · Manipuler du **texte** sans la STL : la chaîne
> possédante `NkString`, la vue non-possessive `NkStringView`, le builder `NkStringBuilder`,
> le hachage `NkStringHash`, la boîte à outils `NkStringUtils`, et le moteur de formatage
> `NkFormat`.

Le texte est partout dans un moteur — un nom de mesh, un chemin de fichier, un message de log,
une clé de ressource, une commande console, un identifiant d'entité. La question n'est jamais
« comment stocker des caractères » (un tableau d'octets suffirait), mais « qui **possède** ces
caractères, qui les **lit**, et comment éviter de **copier** ce qui n'a pas besoin de l'être ».
Toute la famille String de NKContainers tourne autour de cette distinction : **possession contre
emprunt**. D'un côté `NkString`, qui détient sa mémoire et peut la faire grandir ; de l'autre
`NkStringView`, qui ne fait que **pointer** vers des caractères qu'un autre possède. Choisir le bon
des deux est l'essentiel de la maîtrise du module.

Comme tout NKContainers, ces types passent par **NKMemory** (un `memory::NkIAllocator&` passé par
référence, jamais `new`/`delete`) et exposent une API **maison** en `PascalCase` (`Append`, `Find`,
`Length`) doublée des itérateurs minuscules (`begin`, `end`) pour le `range-based for`. Une nuance
de taille par rapport au reste du module : **`NkString` n'est pas templaté** — c'est `char` fixe,
avec *Small String Optimization*. Le générique multi-encodage existe à part : c'est
`NkBasicString<CharT, Traits>` (voir [String-Encoding](String-Encoding.md)).

- **Namespace** : `nkentseu` (et `nkentseu::string` pour `NkStringHash` / `NkStringUtils`)
- **Header parapluie** : `#include "NKContainers/NKContainers.h"`

---

## Posséder ou emprunter : `NkString` et `NkStringView`

Voici le couple fondateur. **`NkString` possède** ses caractères : elle alloue, copie, fait grandir,
libère. **`NkStringView` emprunte** : elle ne contient qu'un pointeur et une longueur (`const char*
mData; SizeType mLength;`), se copie en `O(1)`, et ne touche jamais à la mémoire. Quand une fonction
ne fait que **lire** du texte — chercher, comparer, parser, découper — elle doit prendre une
`NkStringView` en paramètre, pas une `NkString` par valeur : on évite ainsi toute copie inutile, et
on accepte indifféremment une `NkString`, un littéral `"..."`, un sous-fragment d'un buffer plus
grand. La conversion `NkString → NkStringView` est implicite et gratuite.

`NkString` embarque la **Small String Optimization** : les chaînes courtes (jusqu'à `SSO_SIZE`
caractères) tiennent **dans l'objet lui-même**, sans la moindre allocation. Au-delà, elle bascule
sur le tas avec une croissance géométrique (~×1.5/×2). Comme un `NkVector`, on évite les
réallocations en boucle de concaténation avec `Reserve(n)`. Son `CStr()` garantit un terminateur
`'\0'` — indispensable pour parler aux API C.

```cpp
// Une fonction qui LIT du texte prend une vue : zéro copie, accepte tout.
bool IsMeshFile(NkStringView path) {
    return path.EndsWith(".obj") || path.EndsWith(".fbx");
}

NkString full = "models/hero.fbx";   // possède, peut grandir
IsMeshFile(full);                     // conversion implicite, gratuite
IsMeshFile("ui/cursor.png");          // littéral → vue directe
```

Le piège mortel des vues est la **durée de vie** : une `NkStringView` (et tout ce qui en dérive —
`SubStr`, `Left`, `Trimmed`, `View()`) ne fait que pointer. Si la `NkString` source est **modifiée**
(une réallocation invalide le pointeur) ou **détruite**, la vue devient pendante. Ce n'est **pas**
une mini-copie : c'est un emprunt qui doit mourir avant son propriétaire. Et le `CStr()` d'une
`NkStringView` n'est **pas** garanti null-terminé (contrairement à celui de `NkString`).

> **En résumé.** `NkString` **possède** (alloue, grandit, libère, SSO, `CStr()` null-terminé) ;
> `NkStringView` **emprunte** (`O(1)` à copier, ne possède rien, ne survit pas à sa source). Prenez
> des `NkStringView` en **paramètre de lecture**, des `NkString` quand vous devez **conserver** le
> texte. Ne retournez jamais une vue sur un local.

---

## Vue centrale : tout ce que `NkStringView` sait faire sans rien posséder

`NkStringView` n'est pas un simple « pointeur + longueur » : c'est la classe **la plus riche** du
module. Comme elle ne modifie jamais la mémoire qu'elle observe, **toutes** ses opérations de
lecture sont gratuites en allocation — chercher (`Find`/`RFind`/`FindFirstOf`…), comparer
(`Compare`, `CompareIgnoreCase`, `CompareNatural`), tester (`StartsWith`, `Contains`, `IsDigits`,
`IsPalindrome`), découper (`SubStr`, `Left`, `Right`, `Mid`, `Slice`), rogner (`Trimmed`,
`TrimmedLeft`). Ces dernières renvoient **une autre vue** sur le même buffer : `path.Trimmed()` ne
copie rien, il déplace juste les bornes.

Elle parse aussi sans rien allouer : `ToInt`, `ToFloat`, `ToBool` (stricts, retournent un `bool` de
succès et écrivent le résultat par référence) ou leurs jumeaux tolérants `ToIntOrDefault`,
`ToFloatOrDefault`. Et lorsqu'on a **vraiment** besoin d'une copie possédante, on franchit
explicitement la frontière avec `ToString()`, `ToLower()`, `ToUpper()` (qui, eux, renvoient une
`NkString`).

```cpp
NkStringView line = "  ambient = 0.35  ";
NkStringView trimmed = line.Trimmed();        // "ambient = 0.35", aucune alloc
SizeType eq = trimmed.Find('=');
float32 value;
trimmed.SubStr(eq + 1).Trimmed().ToFloat(value);   // 0.35, toujours zéro alloc
```

**Un piège documenté, à ne jamais réintroduire** : les opérateurs `<`, `<=`, `>`, `>=` de
`NkStringView` sont des **templates contraints par SFINAE** (`kBothSvLike`). Cette contrainte est
obligatoire : sans elle, une comparaison aussi banale que `int <= enum` quelque part dans le
namespace `nkentseu` casse la résolution de surcharge (régression réelle de 2026-05-28). On ne
décontraint **jamais** ces opérateurs.

> **En résumé.** `NkStringView` est la classe de lecture par excellence : recherche, comparaison
> (dont *ignore-case* et *natural*), tests, sous-vues et trim sont **tous gratuits** (ils renvoient
> d'autres vues) ; le passage vers une copie possédante est **explicite** (`ToString`/`ToLower`…).
> Ses opérateurs de comparaison sont SFINAE-contraints — règle dure.

---

## Construire par morceaux : `NkStringBuilder`

Concaténer une `NkString` à coups de `+` ou `+=` dans une boucle, c'est risquer une réallocation à
chaque étape. `NkStringBuilder` répond exactement à ce besoin : **assembler du texte par fragments
nombreux**, dans un unique buffer qui croît géométriquement (`DEFAULT_CAPACITY = 256`). On enchaîne
les `Append` — ou l'`operator<<`, qui accepte directement des **entiers, des flottants, des
booléens** (pas seulement du texte) — puis on récupère le résultat avec `ToString()`.

C'est l'outil du **log**, du **dump de debug**, de la **génération de source** (un shader assemblé à
la volée), du **CSV** ou du **JSON** émis ligne par ligne. Il offre des représentations numériques
prêtes (`AppendHex`, `AppendBinary`, `AppendOctal`), un `AppendFormat` style printf, des
`AppendLine`, du padding (`PadLeft`/`PadCenter`), et même des `Join` statiques pour recoller une
liste avec un séparateur.

```cpp
NkStringBuilder sb;
sb << "Entity #" << entityId << " pos=(" << pos.x << ", " << pos.y << ")";
sb.AppendLine();
sb.AppendFormat("  health: %d/%d", hp, maxHp);
NkString report = sb.ToString();
```

Ce n'est **pas** une `NkString` qu'on manipule en place pour son contenu final ; c'est un
**échafaudage** optimisé pour l'écriture séquentielle. Attention à sa conversion
`operator const char*()` **implicite** (les autres conversions, vers `NkString`/`NkStringView`, sont
`explicit`) : elle peut déclencher des conversions involontaires.

> **En résumé.** `NkStringBuilder` = un buffer croissant pour **assembler** efficacement (logs,
> codegen, sérialisation texte) ; `<<` et `Append` avalent nombres et booléens, `ToString()` clôt
> l'opération. Préférez-le à une cascade de `+` dès qu'il y a plusieurs fragments.

---

## Hacher et outiller : `NkStringHash` et `NkStringUtils`

Deux compagnons libres, dans le sous-namespace `nkentseu::string`.

**`NkStringHash`** fournit un éventail de fonctions de hachage non-cryptographiques (FNV-1a, Murmur3,
xxHash, CityHash, DJB2, CRC32…) plus un `NkHash` par défaut (FNV-1a 64 bits) et des **foncteurs**
(`NkStringHash`, `NkStringHashIgnoreCase`) directement branchables sur les tables de hachage du
module. C'est le socle des **clés de ressources** (transformer `"textures/wood.png"` en `uint64`),
des **identifiants compilés à la fonction de construction** (`"_hash` en compile-time), et de la
combinaison de hachages (`NkHashCombine`, formule Boost).

**`NkStringUtils`** est la boîte à outils libre : tout ce qu'on attend d'une bibliothèque texte —
`NkSplit`/`NkJoin`, `NkReplaceAll`, `NkTrim`, `NkPadLeft`, parsing, échappement (C/HTML/URL), et
surtout des helpers de **chemin** (`NkGetFileName`, `NkGetExtension`, `NkCombinePaths`,
`NkNormalizePath`) précieux pour l'IO. La convention est limpide : `Nk*` renvoie une copie
possédante, `Nk*InPlace(NkString&)` modifie sur place, et les fonctions renvoyant une `NkStringView`
sont des vues non-possessives.

```cpp
using namespace nkentseu::string;
NkStringView path = "assets/levels/forest.nkscene";
NkStringView ext = NkGetExtension(path);          // ".nkscene" (avec le point ; vue, zéro alloc)
uint64 key = NkStringHash{}(path);                 // clé pour une table de ressources
```

> **En résumé.** `NkStringHash` = familles de hash non-crypto + foncteurs pour les tables (clés de
> ressources, IDs compile-time, combinaison). `NkStringUtils` = split/join/replace/trim/pad/parse +
> helpers de **chemin** ; `Nk*` copie, `Nk*InPlace` modifie, retour `NkStringView` = vue.

---

## Formater proprement : `NkFormat`

`NkString::Format` reste un `printf` classique (positions implicites, spécificateurs `%`). Mais le
moteur moderne du module est **`NkFormat`** : une syntaxe à accolades `{0}` / `{1:spec}`, type-safe,
extensible. On désigne les arguments par index, on précise alignement/largeur/précision dans le
`spec`, et — point clé — on peut **enseigner à `NkFormat` à afficher ses propres types** via la
spécialisation `NkFormatter<T>` ou la macro `NK_FORMATTER`.

```cpp
NkString s = NkFormat("frame {0} @ {1:.2f} ms ({2:>6} draws)", frameId, ms, drawCount);
NkPrintln("entity {0} at {1}", id, position);     // affiche directement (ADL sur NkVec3)
```

C'est l'outil naturel des **messages de log**, des **overlays de debug** (HUD, compteurs), de tout
affichage destiné à l'humain. Une **mise en garde unique dans tout le module** : `NkFormat`
**utilise la STL** (il lève `std::runtime_error`/`std::out_of_range` sur un format malformé, appelle
`snprintf`) — c'est la **seule** entorse délibérée à la règle zero-STL de NKContainers, parce que le
formatage générique l'exige. Le corps de `NkString::Fmt` est d'ailleurs défini ici, et délègue à
`NkFormat`.

> **En résumé.** `NkFormat` = formatage moderne `{i:spec}`, type-safe et **extensible**
> (`NkFormatter<T>` / `NK_FORMATTER`) pour vos propres types ; `NkPrint`/`NkPrintln` pour afficher.
> Seule partie **non zero-STL** du module (lève des exceptions STL sur format invalide).

---

## Aperçu de l'API

Tous les types vivent dans `nkentseu` (sauf `NkStringHash`/`NkStringUtils` → `nkentseu::string`).
`SizeType = usize`, `npos = (SizeType)-1`, allocateur `memory::NkIAllocator&`. Complexités entre
crochets quand utile.

### `NkString` — chaîne possédante (char, SSO)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Types | `ValueType=char`, `SizeType`, `npos`, `SSO_SIZE` | Caractère ; index ; non-trouvé ; capacité SSO. |
| Construction | `NkString()`, `(const char*)`, `(const char*, len)`, `(NkStringView)`, `(count, ch)`, copie, déplacement `[O(1)]`, surcharges `+ allocator&` | Vide (SSO, 0 alloc) / depuis C-string / vue / remplissage / copie / transfert. |
| Affectation | `operator=` (`NkString&&`, `const char*`, `NkStringView`, `char`) | Réaffectation. |
| Accès | `operator[]`, `At` (assert), `Front`, `Back`, `Data`, `CStr` | Indexé non vérifié / vérifié / extrémités / pointeur brut / **null-terminé**. |
| Capacité | `Length`/`Size`, `Capacity`, `Empty`, `Reserve`, `ShrinkToFit`, `Clear` | Longueur / réservé / vide ? / pré-réserver / compacter / vider. |
| Ajout | `Append` (×6), `operator+=`, `PushBack`, `Insert`, `Erase`, `PopBack`, `Replace`, `Resize`, `Swap` | Concat fin, insertion, suppression, remplacement, redim, échange. |
| Sous-chaîne | `SubStr` (alloue), `Compare` (×3) | Extraire (copie), comparer. |
| Prédicats | `StartsWith`, `EndsWith`, `Contains` (×3 chacun) | Tests de motif. |
| Recherche | `Find`/`RFind`, `FindFirstOf`/`FindLastOf`/`FindFirstNotOf`/`FindLastNotOf` | Localiser. |
| Conversion | `operator NkStringView()` (implicite), `View`, `begin`/`end`, `GetAllocator` | Vue / itérateurs / allocateur. |
| Transfos | `ToLower`/`ToUpper`/`Trim`/`TrimLeft`/`TrimRight`/`Reverse` `[O(n/2)]`/`Capitalize`/`TitleCase`/`RemoveChars`/`RemoveAll` | In-place, renvoient `NkString&`. |
| Parsing | `ToInt`/`ToFloat`/`ToInt64`/`ToUInt`/`ToUInt64`/`ToDouble`/`ToBool` ; `ToInt32`/`ToFloat32` (défaut) | Strict (bool + out) ou tolérant. |
| Contenu | `IsDigits`/`IsAlpha`/`IsAlphaNumeric`/`IsWhitespace`/`IsNumeric`/`IsInteger` | Tests de contenu. |
| Hash / Format | `Hash`, `Format`/`VFormat`/`Fmtf`/`VFmtf` (printf), `Fmt` (`{i}` → NkFormat) | Hachage, formatage. |
| Libre | `operator+` (×5), `==`/`!=`/`<`/`<=`/`>`/`>=` | Concaténation, comparaison. |

### `NkStringView` — vue non-possessive (char)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkStringView()`, `(const char*)`, `(const char*, len)`, `(const NkString&)`, `(const char(&)[N])` | Toutes `[O(1)]`, la plupart `constexpr`. |
| Accès | `operator[]`, `At`, `Front`, `Back`, `Data`, `CStr` (**pas** null-terminé), `DataMutable` | Lecture ; `DataMutable` = `const_cast` dangereux. |
| Capacité | `Length`/`Size`/`Count`, `Empty`, `IsNull`, `IsNullOrEmpty`, `Capacity`, `MaxSize` | État. |
| Bornes | `RemovePrefix`/`RemoveSuffix`, `Clear`/`Reset` (no-op mémoire), `ShrinkToFit` (no-op) | Ajuster la fenêtre. |
| Sous-vues | `SubStr`, `Slice`, `Left`, `Right`, `Mid`, `Trimmed`/`TrimmedLeft`/`TrimmedRight`, `TrimmedChars`… | Renvoient des **vues** `[O(1)]`. |
| Comparaison | `Compare`/`CompareIgnoreCase`/`CompareNatural`, `Equals`*, `StartsWith`/`EndsWith`/`Contains` (+ *IgnoreCase*), `ContainsAny`/`All`/`None` | Comparer / tester. |
| Recherche | `Find`/`RFind` (+ *IgnoreCase*), `FindFirstOf`/`FindLastOf`/`…NotOf`, `Count`/`CountAny`, `FindLast` | Localiser / compter. |
| Parsing | `ToInt`/`ToInt32`/`ToInt64`/`ToUInt`/`ToUInt64`/`ToFloat`/`ToDouble`/`ToBool` ; `…OrDefault` | Strict ou tolérant. |
| Vers possédant | `ToString`, `ToLower`, `ToUpper`, `ToCapitalized`, `ToTitleCase` | Renvoient `NkString`. |
| Prédicats | `IsWhitespace`/`IsDigits`/`IsAlpha`/`IsHexDigits`/`IsLower`/`IsUpper`/`IsNumeric`/`IsInteger`/`IsFloat`/`IsBoolean`/`IsPalindrome`… | Tests de contenu. |
| Itérateurs | `begin`/`end`/`cbegin`/`cend`, `rbegin`/`rend`/`crbegin`/`crend` | Avant / inverse. |
| Avancé | `Hash`/`HashIgnoreCase`/`HashFNV1a`, `LevenshteinDistance`, `Similarity`, `WithoutPrefix`/`Suffix`, `MatchesPattern` (`*`/`?`), `MatchesRegex` | Hash, distance, motifs. |
| Opérateurs | `operator*` (=Front), `==`/`!=`, `<`/`<=`/`>`/`>=` (**SFINAE `kBothSvLike`**) | Comparaison contrainte. |
| Littéraux | `""_sv`, `""_nv` (si `NK_CPP11`) | Construire une vue. |

### `NkStringBuilder` — assemblage par fragments (char)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkStringBuilder()`, `(SizeType cap)`, `(NkStringView)`, `(const char*)`, `(const NkString&)`, copie, déplacement ; `DEFAULT_CAPACITY=256` | Buffer croissant. |
| Append texte | `Append` (×N), `AppendLine`, `AppendFormat`/`AppendVFormat`, `operator<<` (texte) | Concat fragments. |
| Append num | `Append(int/uint/float/bool)`, `AppendHex`/`AppendBinary`/`AppendOctal`, `operator<<` (nombres, bool) | Nombres directement. |
| Insertion | `Insert` (×N), `Replace`/`ReplaceAll`, `Remove`/`RemoveAt`/`RemovePrefix`/`RemoveSuffix`/`RemoveAll`, `RemoveWhitespace`, `Trim`/`TrimLeft`/`TrimRight`, `Clear`/`Reset`/`Release` | Éditer. |
| Capacité | `Length`/`Size`/`Count`, `Capacity`, `Empty`/`IsFull`, `Reserve`, `Resize`, `ShrinkToFit` | État. |
| Accès | `operator[]`, `At`, `Front`/`Back`, `Data`, `CStr` | Lecture/écriture. |
| Extraction | `SubStr`/`Slice`/`Left`/`Right`/`Mid` (vues), `ToString` (×2) | Vers vue / copie. |
| Recherche | `Find`/`FindLast`, `Contains`/`StartsWith`/`EndsWith`, `Compare`/`Equals` | Localiser/comparer. |
| Transfos | `ToLower`/`ToUpper`/`Capitalize`/`TitleCase`/`Reverse`/`PadLeft`/`PadRight`/`PadCenter` | In-place. |
| Flux | `Write`/`WriteChar`/`WriteLine` | Interface flux. |
| Conversion | `operator NkString()` (`explicit`), `operator NkStringView()` (`explicit`), `operator const char*()` (**implicite !**), `View` | Sortie. |
| Divers | `Swap`, `Hash` (FNV-1a), `Join` (statics ×3) | Échange, hash, jointure. |

### `nkentseu::string` — hachage et utilitaires

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Hash algos | `NkHashFNV1a32/64`, `NkHashMurmur3_32/128`, `NkHashCity64/128`, `NkHashXX32/64`, `NkHashDJB2_32/64`, `NkHashSDBM32`, `NkHashCRC32`, `NkHashAdler32`, `NkHashJenkins32`, `NkHashLookup3`, `NkHashSpooky128` | Familles non-crypto. |
| Hash défaut | `NkHash`/`NkHash32`/`NkHash64` (=FNV-1a), `NkHashIgnoreCase*` | Choix par défaut. |
| Hash combine | `NkHashCombine` (Boost), `NkHashCompileTime32/64` (constexpr), `""_hash`/`""_hash32`/`""_hash64` | Combiner / compile-time. |
| Hash foncteurs | `NkStringHash`, `NkStringHashIgnoreCase`, `NkSeededStringHash<>`, `NkStringHashEqual<>`, `…IgnoreCase` | Pour tables de hachage. |
| Hash utils | `NkHashToHex32/64/128`, `NkHashToBase64_*`, `NkHashMulti`, `NkHashSHA1`, `NkHashBenchmark`, `NkHashCollisionProbability` | Affichage / multi / mesure. |
| Utils casse | `NkToLower`/`NkToUpper`/`NkToggleCase`/`NkSwapCase` (+ `InPlace`) | Casse. |
| Utils trim | `NkTrim`/`NkTrimLeft`/`NkTrimRight` (vues), `…Copy`, `…InPlace`, `…Chars` | Rogner. |
| Utils split/join | `NkSplit`/`NkSplitAny`/`NkSplitLines`/`NkSplitN`/`NkSplitEach`, `NkJoin`/`NkJoinTransform` | Découper / recoller. |
| Utils replace | `NkReplace`/`NkReplaceAll`/`NkReplaceFirst`/`NkReplaceLast` (+ `InPlace`, `Callback`) | Remplacer. |
| Utils recherche | `NkStartsWith`/`NkEndsWith`/`NkContains` (+ *IgnoreCase*), `NkFindFirstOf`/`…`, `NkCount` | Tester / localiser. |
| Utils extraction | `NkSubstringBetween`/`Before`/`After`/`…Last`, `NkFirstChar`/`NkLastChar`, `NkMid` | Extraire (vues). |
| Utils transfos | `NkCapitalize`/`NkTitleCase`/`NkReverse`/`NkRemoveChars`/`NkRemoveExtraSpaces`/`NkInsert`/`NkErase` (+ `InPlace`) | Transformer. |
| Utils parsing | `NkParseInt`/`Int64`/`UInt`/`Float`/`Double`/`Bool`, `NkToHex`/`NkParseHex` | Parser / hex. |
| Utils prédicats | `NkIsDigit`/`NkIsAlpha`/`…` (char), `NkIsAllDigits`/`NkIsNumeric`/`NkIsPalindrome`/`…` (chaîne) | Tester contenu. |
| Utils texte | `NkPadLeft`/`Right`/`Center`, `NkRepeat`, `NkEscape`/`NkUnescape`/`NkC*`/`NkHTML*`/`NkURL*` | Padding / échappement. |
| Utils chemins | `NkGetFileName`/`…WithoutExtension`/`NkGetDirectory`/`NkGetExtension`/`NkChangeExtension`/`NkCombinePaths`/`NkNormalizePath`/`NkIsAbsolutePath` | **IO / chemins.** |
| Utils avancés | `NkRandomString`, `NkGenerateUUID` (v4), `NkObfuscate`/`Deobfuscate`, `NkLevenshteinDistance`, `NkSimilarity`, `NkMatchesPattern`, `NkIsEmail`/`URL`/`Identifier` | Génération / validation. |
| Utils sûrs | `NkSafeCopy`/`NkSafeConcat`, `NkMakeView` | Copie bornée / vue. |

### `NkFormat` — moteur de formatage (`nkentseu`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Format | `NkFormat` (`{i:spec}`), `NkPrintf` (`%`), `NkFormatTo`/`NkPrintfTo` | Construire une `NkString`. |
| Affichage | `NkPrint`/`NkPrintln` (stdout), `NkEPrint`/`NkEPrintln` (stderr) | Imprimer. |
| Extension | `NkFormatter<T>`, `NK_FORMATTER`/`NK_FORMATTER_END`, `NkToString` (ADL), `NkFormatProps` | Formater ses propres types. |

---

## Référence complète

Chaque élément est repris ici à fond. Les bases (construction, accès) sont brèves ; les opérations
décisives — possession contre vue, recherche, formatage, hachage — le sont en détail, avec leurs
usages dans les différents domaines du moteur.

### `NkString` à fond

**SSO et croissance.** Une `NkString` vide ou courte ne fait **aucune allocation** : les caractères
tiennent dans l'objet (jusqu'à `SSO_SIZE`). C'est crucial dans les boucles chaudes où l'on
manipule des noms courts (clés, tags, identifiants). Au-delà, la chaîne bascule sur le tas et croît
géométriquement ; `Reserve(n)` supprime les réallocations intermédiaires d'une concaténation, et
`ShrinkToFit()` rend l'excédent. `Length()` et `Capacity()` jouent le même rôle que sur `NkVector`.

**Accès sûr ou rapide.** `operator[]` ne vérifie pas (rapide), `At` asserte en debug. `CStr()`
**garantit le `'\0'` final** — c'est par lui qu'on passe un chemin à `fopen`, un nom à une API
système, une source à un compilateur de shader. `Data()` donne le pointeur brut, `View()` la vue.

**Transformations et parsing intégrés.** Contrairement à `std::string`, `NkString` embarque
directement `ToLower`/`ToUpper`/`Trim`/`Reverse`/`Capitalize` (in-place, chaînables) et le parsing
(`ToInt`, `ToFloat`…). Pratique pour normaliser une saisie sans dépendances externes.

Cas d'usage, par domaine :
- **IO / ressources** — chemins de fichiers, noms d'assets que l'on **conserve** (clés d'un cache de
  textures) ; `CStr()` pour les API système ; voir [String-Encoding](String-Encoding.md) pour
  l'UTF-16/wide sur Windows.
- **UI / 2D** — labels, champs de saisie, texte d'un overlay : on stocke et on édite (`Insert`,
  `Erase`, `Replace`) au fil de la frappe.
- **Gameplay / IA** — noms d'entités, identifiants de quêtes, commandes console parsées via
  `ToInt`/`ToFloat`.
- **Audio** — noms de banques de sons, clés d'événements sonores tenues dans un dictionnaire.
- **GPU / rendu** — noms d'uniforms, de passes, de techniques ; sources de shaders assemblées (mais
  pour l'assemblage massif, préférez `NkStringBuilder`).

### `NkStringView` à fond

C'est la classe centrale. **Tout ce qui lit du texte** doit la prendre en paramètre : on évite une
copie par appel, et un même code accepte `NkString`, littéral et sous-fragment. Ses méthodes de
**recherche** (`Find`, `RFind`, `FindFirstOf`, `Count`), de **comparaison** (jusqu'au tri *natural*
qui ordonne `"img2"` avant `"img10"`), de **test** (`StartsWith`, `Contains`, `IsDigits`,
`MatchesPattern` avec `*`/`?`) et de **sous-vue** (`Left`, `Right`, `Mid`, `Slice`, `Trimmed`) sont
**toutes sans allocation** : les sous-vues renvoient d'autres vues sur le même buffer.

Le **parsing** y est sans allocation : `ToFloat`/`ToInt` (stricts) ou `…OrDefault` (tolérants).
Les fonctions avancées — `LevenshteinDistance`, `Similarity`, `Hash`/`HashIgnoreCase` — servent la
recherche floue et l'indexation.

Cas d'usage, par domaine :
- **IO / parsing** — parcourir un fichier de config ou un CSV en mémoire **sans copier** : on
  `Find('\n')`, on `SubStr`, on `Trimmed`, on `ToFloat`, le tout sur le buffer d'origine.
- **Gameplay** — analyser une ligne de commande console, des arguments, sans allouer.
- **ECS** — comparer/tester des noms de composants ou de tags reçus depuis l'extérieur.
- **UI** — découper un texte en mots pour le retour à la ligne (word-wrap) en émettant des vues, pas
  des copies.
- **Rendu / shaders** — scanner une source GLSL/HLSL (repérer `#include`, des directives) en lecture
  seule.

**Rappel dur** : une vue meurt avec sa source. Ne stockez pas durablement une `NkStringView` sur le
contenu d'une `NkString` que vous allez modifier — copiez dans une `NkString` si vous devez la
garder. Et **ne décontraignez jamais** les opérateurs `<`/`<=`/`>`/`>=` (SFINAE `kBothSvLike`,
sous peine de casser `int <= enum` ailleurs).

### `NkStringBuilder` à fond

Le builder existe pour **une** raison : **assembler beaucoup de fragments** sans la cascade de
réallocations qu'imposerait `NkString::operator+`. Il maintient un buffer unique qui croît
géométriquement (départ `DEFAULT_CAPACITY = 256`) et reste null-terminé. Son `operator<<` et ses
`Append` typés acceptent **directement** les nombres et booléens — pas besoin de les convertir à la
main — et il offre `AppendHex`/`AppendBinary`/`AppendOctal`, `AppendFormat` (printf), `AppendLine`,
du padding et des `Join` statiques.

Cas d'usage, par domaine :
- **Log / debug** — construire un message multi-champ (`<< id << " " << pos.x`) avant de l'émettre
  d'un coup ; un dump d'état de la scène.
- **Codegen GPU** — assembler une source de shader à partir de fragments conditionnels (defines,
  permutations) puis `ToString()` vers le compilateur.
- **Sérialisation texte** — émettre du JSON/CSV/INI ligne par ligne (`AppendLine`, `Join`).
- **UI** — composer un texte d'info-bulle ou de console à partir de plusieurs sources.

Ce n'est **pas** la structure du texte final manipulé en place (c'est le rôle de `NkString`) ;
récupérez le résultat avec `ToString()`/`View()`. Méfiance avec l'`operator const char*()`
**implicite**, source de conversions surprises.

### `NkStringHash` à fond

Une **famille** de hachages non-cryptographiques, chacun avec son compromis vitesse/qualité :
FNV-1a (simple, rapide, le défaut), Murmur3 et xxHash (excellente dispersion), CityHash, DJB2,
CRC32, etc. Le `NkHash` par défaut est FNV-1a 64 bits. Les **foncteurs** `NkStringHash` /
`NkStringHashIgnoreCase` se branchent directement sur les tables de hachage du module. La version
compile-time (`""_hash`, `NkHashCompileTime*`) calcule le hash d'une constante littérale **à la
compilation** — zéro coût au runtime.

Cas d'usage, par domaine :
- **Ressources / IO** — transformer un chemin (`"textures/wood.png"`) en `uint64` pour indexer un
  cache d'assets sans comparer les chaînes à chaque accès.
- **ECS** — hacher les noms de types de composants pour des lookups rapides.
- **Rendu / GPU** — clés de pipelines, de matériaux, de passes (souvent une chaîne hachée).
- **Gameplay** — identifiants d'événements compile-time (`"on_player_died"_hash`), comparés en
  `O(1)` au lieu de comparer des chaînes.
- **Réseau / sérialisation** — empreintes de version, sommes de contrôle légères (`CRC32`,
  `Adler32`).

`NkHashCombine` (formule Boost) assemble plusieurs hashes en un seul — la base pour hacher une clé
composite (par ex. `nom + variante`).

### `NkStringUtils` à fond

La boîte à outils libre, `nkentseu::string`. La règle de nommage structure tout : `Nk*` produit une
**copie** (`NkString`), `Nk*InPlace(NkString&)` **modifie** la chaîne reçue, et les fonctions
renvoyant une `NkStringView` sont des **vues** (zéro alloc). On y trouve `NkSplit`/`NkJoin`,
`NkReplaceAll`, `NkTrim`, `NkPadLeft`, le parsing, l'échappement (C, HTML, URL) et la génération
(`NkRandomString`, `NkGenerateUUID`).

Le sous-ensemble le plus précieux est celui des **chemins**, indispensable côté IO :
`NkGetFileName`, `NkGetFileNameWithoutExtension`, `NkGetDirectory`, `NkGetExtension` (qui gèrent à la
fois `/` et `\`), `NkChangeExtension`, `NkCombinePaths`, `NkNormalizePath`, `NkIsAbsolutePath`.

Cas d'usage, par domaine :
- **IO** — décomposer/recomposer des chemins d'assets, normaliser les séparateurs, changer une
  extension (`.png` → `.dds`).
- **Gameplay / console** — `NkSplit` une commande en arguments, valider une saisie
  (`NkIsIdentifier`, `NkMatchesPattern`).
- **UI** — `NkPadCenter`/`NkPadLeft` pour aligner des colonnes de texte, `NkReplaceTemplates`
  (`{{nom}}`) pour des messages localisés.
- **Sérialisation / réseau** — `NkURLEncode`/`NkHTMLEscape` pour échapper, `NkLevenshteinDistance` /
  `NkSimilarity` pour la recherche tolérante aux fautes.

### `NkFormat` à fond

Le moteur de formatage moderne du module : syntaxe `{0}` / `{1:spec}`, type-safe, **extensible**. Le
`spec` reprend la grammaire familière (`[[fill]align][sign][#][0][width][grouping][.precision][type]`),
avec alignement `<`/`>`/`^`/`=` et types `d`/`x`/`f`/`e`/`g`/`b`/`%`… Les conteneurs du module
s'affichent tout seuls (séquentiels `[a, b]`, associatifs `{k: v}`) par ADL. Et l'on **enseigne** à
`NkFormat` n'importe quel type maison via `NkFormatter<T>` ou la macro `NK_FORMATTER(MonType){ … }
NK_FORMATTER_END`.

Cas d'usage, par domaine :
- **Log** — messages lisibles, alignés, typés (`NkFormat("[{0:>8}] {1}", level, msg)`).
- **Debug / UI** — overlays et HUD (`NkFormat("FPS {0:.1f}", fps)`), inspecteurs affichant des
  `NkVec3`, des couleurs, des entités via leur `NkFormatter`.
- **Outils** — génération de rapports, de tables, d'export texte.

**Mise en garde — la seule entorse zero-STL du module.** `NkFormat` utilise la STL : il **lève**
`std::runtime_error`/`std::out_of_range` si le format est malformé et s'appuie sur `snprintf`. Partout
ailleurs NKContainers est zero-STL ; ici, c'est un compromis assumé pour le formatage générique. À
garder en tête si vous compilez sans exceptions ou sans STL.

### Le socle commun

- **Possession vs emprunt.** `NkString`/`NkStringBuilder`/`NkBasicString` **possèdent** ;
  `NkStringView`/`BasicStringView` **empruntent**. Une vue ne survit jamais à sa source.
- **Allocateur conscient.** Toute la mémoire passe par un `memory::NkIAllocator&` (défaut
  `memory::NkGetDefaultAllocator()`) — jamais `new`/`delete`. L'allocateur passé par référence doit
  **survivre** à l'instance. Voir [NKMemory](../NKMemory.md).
- **`CStr()` null-terminé** sur `NkString`/`NkStringBuilder`/`NkBasicString` ; **pas** garanti sur
  `NkStringView`.
- **SSO** : `NkString`/`NkBasicString` rangent les courtes chaînes en interne, zéro alloc ;
  `Reserve` avant une boucle de concat.
- **Double API numérique** : `ToX(out&)` strict (renvoie un succès) ou `ToXOrDefault` tolérant.
- **Règle SFINAE des vues** : ne jamais décontraindre `<`/`<=`/`>`/`>=` de `NkStringView`.
- **Multi-encodage** : le générique est `NkBasicString<CharT, Traits>` (`NkString8/16/32`,
  `NkWString`) — page [String-Encoding](String-Encoding.md). `NkString` global ≠ `NkString8`.

---

### Exemple

```cpp
#include "NKContainers/NKContainers.h"
using namespace nkentseu;
using namespace nkentseu::string;

// 1. Vue : lire et parser une ligne de config SANS copier.
NkStringView line = "  spawn_rate = 0.75  ";
NkStringView trimmed = line.Trimmed();          // "spawn_rate = 0.75", zéro alloc
SizeType eq = trimmed.Find('=');
NkStringView key = trimmed.SubStr(0, eq).Trimmed();        // "spawn_rate"
float32 rate;
trimmed.SubStr(eq + 1).Trimmed().ToFloat(rate);            // 0.75

// 2. String possédante + hash : une clé de ressource.
NkString assetPath = NkCombinePaths("textures", "wood.png");   // "textures/wood.png"
uint64 cacheKey = NkStringHash{}(assetPath);                   // pour la table d'assets
NkStringView ext = NkGetExtension(assetPath);                  // ".png" (avec le point ; vue)

// 3. Builder : assembler un rapport de debug d'un coup.
NkStringBuilder sb;
sb << "asset='" << assetPath << "' key=";
sb.AppendHex(cacheKey, true);                                  // 0x....
NkString report = sb.ToString();

// 4. Format moderne : message de log type-safe.
NkPrintln("loaded {0} ({1:.2f} ms)", assetPath, loadMs);
```

---

[← Index NKContainers](README.md) · [Récap NKContainers](../NKContainers.md) · [Encodage des chaînes →](String-Encoding.md)
