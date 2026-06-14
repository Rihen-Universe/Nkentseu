# Les flux

> Couche **System** · NKStream · Lire et écrire des **octets** à travers une seule abstraction —
> l'interface de base `NkStream`, et ses trois incarnations concrètes : le tampon mémoire
> `NkBinaryStream`, le flux fichier `NkFileStream` et le flux console `NkConsoleStream`.

Dès qu'un programme **échange des données avec le monde** — charger un maillage depuis le disque,
sérialiser une scène, accumuler un paquet réseau en mémoire, écrire une trace de log dans la
console — il a besoin d'un *flux* : un objet par lequel les octets entrent et sortent dans un ordre.
La tentation est d'écrire un bout de code pour le fichier, un autre pour la mémoire, un autre pour
la console. NKStream prend le parti inverse : **une seule interface**, `NkStream`, que tout le reste
du moteur sait manipuler sans jamais savoir *d'où* viennent réellement les octets. On écrit son
sérialiseur une fois, et il fonctionne aussi bien sur un fichier que sur un tampon RAM ou un test
unitaire en mémoire.

Le contrat est volontairement minimal et **orienté octets** : deux primitives brutes
(`ReadRaw` / `WriteRaw`), un positionnement absolu (`Seek` / `Tell`), de quoi connaître la taille
et la fin (`Size` / `IsEOF`), et par-dessus, deux templates de confort (`Read<T>` / `Write<T>`) qui
raisonnent en **éléments typés**. Ce n'est **pas** un système de sérialisation : NKStream ne sait
rien des champs, des versions, ni du JSON — il transporte des octets, et c'est tout. La couche
sérialisation (`NKSerialization`) se construit *au-dessus*.

- **Namespace** : `nkentseu`
- **Headers** (pas de parapluie — on inclut chaque flux individuellement) :
  `#include "NKStream/NkStream.h"`, `#include "NKStream/NkBinaryStream.h"`,
  `#include "NKStream/NkFileStream.h"`, `#include "NKStream/NkConsoleStream.h"`
- **Macro d'export** : `NKSTREAM_API`. Types primitifs : `uint8`, `uint16`, `uint32`, `usize`,
  `nk_int64` (de `NKCore/NkTypes.h`).

---

## L'interface de base : `NkStream`

`NkStream` est la **classe de base polymorphe pure** dont héritent tous les flux. Elle ne stocke
rien d'autre qu'un `mOpenMode` (les drapeaux d'ouverture) et déclare le vocabulaire commun. Le cœur,
ce sont deux primitives **abstraites** que chaque flux concret doit fournir :

```cpp
virtual usize ReadRaw(void* buffer, usize byteCount) = 0;     // lit des octets bruts
virtual usize WriteRaw(const void* data, usize byteCount) = 0; // écrit des octets bruts
```

Tout le reste de l'interface (`Open`, `Close`, `IsOpen`, `Seek`, `Tell`, `Size`, `IsEOF`) est lui
aussi **pur virtuel** : `NkStream` ne s'instancie pas, on en dérive. Le destructeur est `virtual`,
ce qui rend la destruction polymorphe sûre — on peut tenir un `NkStream*` sur n'importe quel flux et
le détruire proprement.

Au-dessus des primitives octets viennent **deux templates de confort**, eux **non virtuels** :

```cpp
template<typename T> usize Read(T* buffer, usize count);   // count éléments de type T
template<typename T> usize Write(const T* data, usize count);
```

`Read<T>` délègue à `ReadRaw(buffer, count * sizeof(T))` puis **divise le résultat par
`sizeof(T)`** : il rend donc le nombre d'**éléments complets** lus. C'est tout le confort — on
raisonne en `NkVertex`, en `float`, en `uint32`, pas en octets. Mais c'est aussi le piège : si le
flux rend un nombre d'octets qui n'est **pas** un multiple de `sizeof(T)`, l'élément partiel est
**perdu** (la division entière le tronque). À retenir : les templates parlent éléments, les
primitives parlent octets, et la conversion est une simple division.

Deux détails à connaître. D'abord, **`Seek` est toujours absolu** : il prend une seule `usize`,
l'offset *depuis le début* — il n'y a pas d'origine relative (« depuis la position courante » ou
« depuis la fin »). Ensuite, l'encodage : `SetEncoding(Encoding)` et `GetEncoding()` sont virtuels
mais possèdent un **corps par défaut** (le premier ignore son argument et renvoie `true`, le second
renvoie `NK_SYSTEM_DEFAULT`) — un flux binaire pur peut donc les laisser tels quels, un flux texte
les redéfinit.

> **En résumé.** `NkStream` = l'interface commune orientée **octets** : `ReadRaw`/`WriteRaw` sont les
> primitives à implémenter, `Read<T>`/`Write<T>` sont le confort typé (qui **tronque** au multiple de
> `sizeof(T)`). `Seek` est **absolu**, le destructeur est virtuel. On ne l'instancie pas, on en
> dérive.

---

## Le tampon mémoire : `NkBinaryStream`

`NkBinaryStream` est un flux qui vit **entièrement en RAM** : ses octets sont dans un tampon mémoire
qu'il sait faire **grandir tout seul**. C'est l'outil de la sérialisation sans disque — construire
un paquet réseau, composer un message avant de l'envoyer, capturer la sortie d'un sérialiseur pour
la comparer dans un test, ou décoder un fichier déjà chargé en mémoire.

Il propose deux modes, choisis par le **constructeur** — et c'est là le premier piège, parce que la
surcharge se joue sur le type de l'argument :

```cpp
NkBinaryStream view(externalPtr, capacity);  // VUE non-propriétaire sur un tampon externe
NkBinaryStream owned(1024);                   // ALLOUE son propre tampon de 1024 octets
```

La version `(void* data, usize capacity)` crée une **vue** : le flux n'est pas propriétaire
(`mOwned = false`), il écrit dans la mémoire qu'on lui prête. La version `(usize capacity)` **alloue**
un tampon neuf et en devient propriétaire. Attention : `NkBinaryStream(0)` ou
`NkBinaryStream(maTaille)` — un seul argument entier — choisit la version **propriétaire**, tandis
que `NkBinaryStream(monPointeur)` choisit la **vue**. Ce n'est **pas** interchangeable.

L'écriture est auto-extensible : `WriteRaw` appelle au besoin une croissance **×2** du tampon (comme
un vecteur), recopie, et avance la position. Conséquence subtile mais importante : **une vue
non-propriétaire devient propriétaire dès la première réallocation** — à ce moment le moteur alloue
un nouveau bloc, et le pointeur externe d'origine n'est plus le *backing* du flux. Si vous comptiez
relire les octets via votre pointeur de départ, ils ne s'y trouvent plus.

```cpp
NkBinaryStream out(256);                 // possède son tampon
uint32 magic = 0x4E4B4253;
out.Write(&magic, 1);                    // 4 octets
out.Write(vertices.Data(), vertices.Size());  // N * sizeof(NkVertex)
const uint8* bytes = out.Data();         // accès brut au tampon
usize n = out.Size();                    // octets écrits jusqu'ici
```

`Seek` ici **refuse de dépasser `mSize`** (il renvoie `false` au-delà) : impossible de se positionner
loin devant puis d'écrire en *sparse*. `Close()` est volontairement un **no-op** — il ne libère
**rien** ; c'est le **destructeur** qui rend la mémoire, et seulement si le flux est propriétaire.
Dernier point de vigilance : `NkBinaryStream` alloue avec `new[]`/`delete[]` **CRT**, pas avec
NKMemory — ne passez donc jamais son `Data()` à `NkFree`, et ne mélangez pas son tampon avec un
allocateur custom (risque de heap corruption).

> **En résumé.** `NkBinaryStream` = flux **en RAM**, auto-extensible (croissance ×2). Constructeur à
> un argument entier = **propriétaire** (alloue) ; à pointeur + capacité = **vue** (emprunte). Une vue
> **devient propriétaire** à la première réallocation. `Close()` ne libère rien (le destructeur s'en
> charge), `Seek` ne dépasse pas la taille, et la mémoire vient du **CRT** (`new[]`), pas de NKMemory.

---

## Le flux fichier : `NkFileStream`

`NkFileStream` lit et écrit dans un **fichier sur disque**. Plutôt que de réimplémenter l'accès
fichier, il se construit **au-dessus de `NkFile`** (module NKFileSystem) — et hérite gratuitement de
tout ce que `NkFile` apporte : le path handling via `NkPath`, et surtout le **fallback
`AAssetManager` sur Android**, qui permet d'ouvrir un fichier empaqueté dans l'APK exactement comme un
fichier normal. C'est le flux de tous les chargements d'assets.

L'ouverture mappe les drapeaux `OpenMode` de `NkStream` vers un `NkFileMode` de NkFile, en **forçant
toujours le mode binaire** — pas de conversion de fins de ligne, ce qui est exactement ce qu'on veut
pour des données :

```cpp
NkFileStream f;
if (f.Open("assets/level.nkb", NkStream::NK_READ_MODE | NkStream::NK_BINARY_MODE)) {
    Header h;
    f.Read(&h, 1);                  // lit un Header
    f.Seek(h.dataOffset);           // positionnement absolu
    f.Read(buffer, h.count);        // lit le bloc de données
}                                   // destructeur : Close() automatique (RAII)
```

La table de correspondance est : lecture+écriture+ajout → `NK_READ | NK_APPEND | NK_BINARY` ;
lecture+écriture sans ajout → `NK_READ_WRITE_BINARY` ; lecture seule → `NK_READ_BINARY` ; écriture
seule avec ajout → `NK_APPEND_BINARY` ; écriture seule sans ajout → `NK_WRITE_BINARY`. Le `Seek` est,
comme partout, **absolu** : il appelle `mFile.Seek(position, NkSeekOrigin::NK_BEGIN)`.

Le flux est **RAII** : son destructeur appelle `Close()`, donc on n'oublie jamais de fermer le
fichier. `IsEOF()` renvoie `Tell() >= Size()` quand le fichier est ouvert (et `true` sinon).

La grande particularité de `NkFileStream`, c'est qu'il **prend en charge l'encodage et le BOM** que
`NkFile` ignore. `SetEncoding` écrit (en mode écriture) le *byte order mark* approprié :

- Sous **Windows** : `NK_UTF16_LE` écrit le BOM `0xFEFF`, `NK_UTF8` écrit `EF BB BF` ; toute autre
  valeur retombe sur `NK_SYSTEM_DEFAULT` et renvoie `false`.
- Sous **les autres OS** : toute valeur autre que `NK_SYSTEM_DEFAULT` est conservée ;
  `NK_SYSTEM_DEFAULT` est forcé en `NK_UTF8` ; renvoie `true`.

Pour l'interopérabilité, `GetFile()` (en versions mutable et const, les **seules méthodes
`noexcept`** de la classe) donne accès direct au `NkFile` sous-jacent. Enfin, `Flush()` est un
**no-op silencieux** : `NkFile` gère lui-même le vidage en interne, il n'y a rien à forcer.

> **En résumé.** `NkFileStream` = flux **fichier**, bâti sur `NkFile` (donc path handling + fallback
> assets Android offerts). Ouverture **toujours binaire**, `Seek` **absolu** depuis le début, **RAII**
> (le destructeur ferme). Il ajoute la gestion **BOM/encodage** que `NkFile` n'a pas, et expose
> `GetFile()` pour l'interop. `Flush()` ne fait rien (NkFile flush seul).

---

## Le flux console : `NkConsoleStream`

`NkConsoleStream` branche un flux sur les **canaux standards** du terminal — l'entrée (stdin), la
sortie (stdout) ou l'erreur (stderr) — avec une gestion d'encodage UTF-8 / UTF-16. On choisit le
canal à la construction via l'énum imbriqué `StreamType` :

```cpp
NkConsoleStream out(NkConsoleStream::NK_OUTPUT);
out.SetEncoding(Encoding::NK_UTF8);
const char* msg = "Démarrage du moteur\n";
out.Write(msg, /*count*/ 20);            // template Write<char>
```

En interne, c'est `WriteRaw` qui parle aux API système : sur Windows il choisit `WriteConsoleW`
quand l'encodage est `NK_UTF16_LE` (et renvoie alors le nombre d'octets en `wchar_t`), sinon
`WriteConsoleA` ; sur les autres OS, un simple `write(fd, …)`. La lecture (`ReadRaw`) ne fonctionne
que pour le canal `NK_INPUT` : sur Windows via `ReadConsoleW` en `wchar_t`, ailleurs via `read`.

`NkConsoleStream` **masque** (au sens C++ : *hides*) les templates `Read`/`Write` de la base par ses
propres `Read<CharT>` / `Write<CharT>`, qui font exactement le même travail (division par
`sizeof(CharT)`) mais s'adaptent au type de caractère. La console n'a évidemment pas de notion de
position : `Seek` renvoie `false`, `Tell` et `Size` renvoent `0`, et `Close()` est un no-op (on ne
ferme jamais les handles système). `SetEncoding`, sous Windows, appelle `SetConsoleOutputCP(CP_UTF8)`
pour l'UTF-8 et laisse `WriteConsoleW` gérer l'UTF-16.

**Deux mises en garde sur le code réel** (à ne pas reproduire, mais à connaître pour ne pas être
surpris) :

- Le ternaire du constructeur teste **deux fois** `NK_INPUT` au lieu de tester `NK_ERROR` en seconde
  position. Conséquence : sur Windows, `mHandle` du canal `NK_ERROR` reçoit `STD_OUTPUT_HANDLE`.
  Cela reste largement sans effet en écriture, car `WriteRaw` **recalcule localement** le bon handle
  (`STD_ERROR_HANDLE` si `NK_ERROR`), indépendamment du `mHandle` mal initialisé.
- D'après les headers, `NkConsoleStream` **ne redéfinit pas `IsEOF()`** — qui reste donc le pur
  virtuel de `NkStream` non implémenté ici. En l'état de ces seuls fichiers, la classe reste
  **abstraite** sauf si une définition existe ailleurs.

> **En résumé.** `NkConsoleStream` = flux sur **stdin/stdout/stderr** (canal choisi par
> `StreamType`), avec encodage UTF-8/UTF-16. Pas de positionnement (`Seek`/`Tell`/`Size` neutres),
> `Close()` ne ferme rien. Il masque les templates `Read`/`Write` par des versions `CharT`. Garde en
> tête le bug du handle `NK_ERROR` (compensé en écriture) et l'absence d'`IsEOF()` redéfini.

---

## Aperçu de l'API

Tous les symboles vivent dans le namespace `nkentseu`. Complexités et `noexcept` entre crochets quand
c'est utile. Chaque élément est détaillé dans la « Référence complète ».

### Énumération et drapeaux

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Encodage | `enum class Encoding` | `NK_SYSTEM_DEFAULT`, `NK_UTF8`, `NK_UTF16_LE`, `NK_UTF16_BE` (déclaré, **jamais traité**). |
| Drapeaux d'ouverture | `NkStream::OpenMode` | `NK_READ_MODE`, `NK_WRITE_MODE`, `NK_APPEND_MODE`, `NK_BINARY_MODE`, `NK_TEXT_MODE` (combinables OR). |
| Canal console | `NkConsoleStream::StreamType` | `NK_INPUT` (0), `NK_OUTPUT` (1), `NK_ERROR` (2). |

### `NkStream` — interface de base abstraite

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `virtual ~NkStream()`, `Open(path, mode)`, `Close`, `IsOpen` `[purs]` | Destruction polymorphe ; ouverture (OR d'`OpenMode`) ; fermeture ; état. |
| Primitives octets | `ReadRaw(buf, n)`, `WriteRaw(data, n)` `[purs, O(n)]` | Lecture / écriture **brutes** en octets ; rendent les octets effectifs. |
| Confort typé | `Read<T>(buf, count)`, `Write<T>(data, count)` `[non virtuels]` | Lecture / écriture en **éléments** ; **tronque** au multiple de `sizeof(T)`. |
| Positionnement | `Seek(position)`, `Tell`, `Size`, `IsEOF` `[purs]` | `Seek` **absolu** (depuis le début) ; position / taille / fin. |
| Encodage | `SetEncoding(e)`, `GetEncoding` `[virtuels, corps défaut]` | Par défaut : ignore et renvoie `true` / renvoie `NK_SYSTEM_DEFAULT`. |
| Donnée | `uint32 mOpenMode` `[protégé]` | Drapeaux d'ouverture mémorisés. |

### `NkBinaryStream : NkStream` — flux mémoire

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkBinaryStream(void* data, usize cap)` | **Vue** non-propriétaire sur un tampon externe. |
| Construction | `NkBinaryStream(usize cap)` | **Alloue** son tampon (propriétaire). **Piège de surcharge** (un seul `usize`). |
| Destruction | `~NkBinaryStream()` | Libère (`delete[]`) **si propriétaire**. |
| Overrides | `Open`→`true`, `Close`→no-op, `IsOpen`→`true` | Toujours « ouvert » ; `Close` **ne libère rien**. |
| Overrides | `ReadRaw` `[O(n)]`, `WriteRaw` `[amorti O(n)]` | `memcpy` + avance position ; écriture auto-extensible (×2). |
| Overrides | `Seek` `[O(1)]`, `Tell`, `Size`, `IsEOF` `[O(1)]` | `Seek` refuse `> mSize` ; position / taille / fin. |
| Tampon | `Resize(n)`, `Reserve(cap)` | Fixe la taille (avec capacité) / pré-réserve. |
| Tampon | `Data()` (const et mutable) | Accès **brut** au tampon (mémoire **CRT**, pas NKMemory). |

### `NkFileStream : NkStream` — flux fichier

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkFileStream()`, `~NkFileStream()` | Défaut ; destructeur **RAII** (`Close()`). |
| Overrides | `Open(path, mode)` | Mappe vers `NkFileMode`, **force le binaire** ; délègue à `NkFile`. |
| Overrides | `Close`, `IsOpen`, `ReadRaw`, `WriteRaw` | Délèguent à `NkFile`. |
| Overrides | `Seek` (→`NK_BEGIN`), `Tell`, `Size`, `IsEOF` | `Seek` **absolu** ; `IsEOF` = `Tell() >= Size()`. |
| Overrides | `SetEncoding(e)`, `GetEncoding` | Gère le **BOM** (UTF-8 / UTF-16-LE), spécifique Windows / autres. |
| Interop | `GetFile()` `[noexcept]` (const et mutable) | Accès direct au `NkFile` sous-jacent. |
| Interop | `Flush()` | **No-op** (NkFile flush en interne). |

### `NkConsoleStream : NkStream` — flux console

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkConsoleStream(StreamType type)` | Choisit le canal (stdin/stdout/stderr). **Bug** ternaire `NK_ERROR`. |
| Overrides | `Open`→`true`, `Close`→no-op, `IsOpen`→`true` | Toujours « ouvert » ; ne ferme pas les handles système. |
| Overrides | `ReadRaw` (input seul), `WriteRaw` | `ReadConsoleW`/`read` ; `WriteConsoleW`/`WriteConsoleA`/`write`. |
| Confort | `Read<CharT>`, `Write<CharT>` | **Masquent** les templates de base ; division par `sizeof(CharT)`. |
| Overrides | `Seek`→`false`, `Tell`→`0`, `Size`→`0` | Pas de positionnement. |
| Overrides | `SetEncoding(e)`, `GetEncoding` | `SetConsoleOutputCP(CP_UTF8)` / UTF-16 auto. |

---

## Référence complète

Chaque élément est repris ici en détail : comportement, complexité, et cas d'usage dans les
différents domaines du moteur. Les éléments triviaux sont décrits brièvement ; les mécanismes
importants le sont à fond.

### `Encoding` — l'encodage des flux texte

Énum scopé à quatre valeurs : `NK_SYSTEM_DEFAULT` (0), `NK_UTF8`, `NK_UTF16_LE`, `NK_UTF16_BE`. Il
gouverne la manière dont un flux **texte** interprète et écrit les octets (BOM, page de code
console). À connaître : `NK_UTF16_BE` est **déclaré mais jamais traité** par aucune implémentation —
`SetEncoding` le fait tomber dans son `default`. En pratique, on utilise `NK_UTF8` (portable) ou, sur
Windows, `NK_UTF16_LE` pour la console. Pour des données **binaires**, l'encodage n'a aucun sens et on
laisse `NK_SYSTEM_DEFAULT`.

### `NkStream::OpenMode` — les drapeaux d'ouverture

Cinq flags bit-à-bit combinables par OR : `NK_READ_MODE` (0x01), `NK_WRITE_MODE` (0x02),
`NK_APPEND_MODE` (0x04), `NK_BINARY_MODE` (0x08), `NK_TEXT_MODE` (0x10). On compose le mode désiré au
moment d'`Open` : `NK_READ_MODE | NK_BINARY_MODE` pour relire un asset, `NK_WRITE_MODE |
NK_APPEND_MODE` pour ajouter à un fichier de log. Le membre protégé `mOpenMode` mémorise ce masque
pour que les flux puissent interroger leur état (par exemple, savoir si l'on est en écriture avant
d'émettre un BOM). À noter que `NkFileStream` **force toujours le binaire** quel que soit le drapeau
texte demandé.

### `Read<T>` / `Write<T>` — le confort typé

C'est l'API qu'on utilise au quotidien, parce qu'on raisonne presque toujours en *éléments*, pas en
octets. `Read<T>(buffer, count)` lit `count` valeurs de type `T` et **renvoie le nombre d'éléments
complets réellement lus** (les octets lus divisés par `sizeof(T)`). `Write<T>` est strictement
symétrique. La complexité est `O(n)` sur les octets transférés. Le **piège** central : un transfert
partiel (octets non multiples de `sizeof(T)`) **perd** l'élément incomplet par troncation entière —
si vous lisez une structure et que le flux se termine au milieu, vous obtenez un élément de moins, pas
une erreur. Domaines :

- **Rendu / GPU** : lire un bloc de `NkVertex` ou d'indices depuis un `.nkb`, ou écrire un tampon de
  sommets dans un `NkBinaryStream` avant *upload*.
- **ECS / outils** : sérialiser des composants d'un même type en bloc (`Write(comps.Data(), n)`).
- **IO / réseau** : composer un en-tête (`Write(&header, 1)`) suivi d'une charge utile typée.
- **Audio** : lire des échantillons `int16`/`float` d'un fichier wave en un seul `Read`.

### `ReadRaw` / `WriteRaw` — les primitives octets

Ce sont les **deux seules méthodes** que chaque flux concret doit vraiment implémenter ; tout le reste
de la lecture/écriture se construit dessus. Elles travaillent en **octets bruts** et renvoient le
nombre d'octets effectivement transférés (qui peut être inférieur au demandé en fin de flux ou si la
console refuse). Complexité `O(n)`. On les appelle directement quand on manipule des **tailles en
octets** plutôt que des éléments : copier un blob opaque, transférer une longueur arbitraire venue
d'un en-tête réseau, ou ponter deux flux (`src.ReadRaw` → `dst.WriteRaw`). Pour des données
**typées**, préférez les templates `Read<T>`/`Write<T>` qui évitent les calculs de `sizeof` manuels.

### `Seek`, `Tell`, `Size`, `IsEOF` — naviguer dans le flux

`Seek(position)` est **absolu sur toute la hiérarchie** : il positionne le curseur à `position`
octets *depuis le début* — il n'existe pas d'origine relative. `Tell()` rend la position courante,
`Size()` la taille totale, `IsEOF()` indique la fin. Usages :

- **IO / formats** : un fichier binaire avec table des matières — on lit l'en-tête, on `Seek` au
  *offset* d'une section, on lit, on `Seek` ailleurs. C'est le cœur du chargement aléatoire d'assets.
- **Outils / éditeur** : revenir en arrière pour réécrire un champ de taille une fois le bloc connu
  (sur `NkBinaryStream`, où `Seek` reste dans `[0, mSize]`).
- **Réseau / parsing** : boucle de lecture pilotée par `IsEOF()` jusqu'à épuisement du flux.

Selon le flux, certaines de ces opérations sont **neutres** : la console ne sait pas se positionner
(`Seek`→`false`, `Tell`/`Size`→`0`), et `NkBinaryStream` **refuse** de `Seek` au-delà de sa taille.

### `NkBinaryStream` à fond

Le flux mémoire est l'outil de toute manipulation d'octets **sans disque**. Son comportement clé est
la **croissance ×2** à l'écriture (`EnsureCapacity` double la capacité, plancher = la taille
requise), ce qui rend `WriteRaw` **amorti `O(n)`** comme un vecteur. `Reserve(cap)` pré-réserve
(réalloue seulement si `cap` dépasse la capacité actuelle) et supprime les réallocations
intermédiaires ; `Resize(n)` fixe directement la taille logique (en s'assurant de la capacité).
`Data()` donne le pointeur brut — indispensable pour pousser le tampon vers le GPU, une socket, ou un
décodeur. Domaines :

- **IO / réseau** : assembler un paquet (en-tête + corps) en RAM, puis envoyer `Data()`/`Size()` en
  une fois ; ou recevoir un blob et le *parser* via une vue `NkBinaryStream(recvPtr, recvLen)`.
- **Outils / éditeur** : capturer la sortie d'un sérialiseur en mémoire pour la diffuser, la hacher,
  ou la comparer dans un test sans toucher le disque.
- **Rendu / GPU** : construire un tampon de sommets ou un blob de constantes côté CPU avant *upload*.
- **Audio** : décoder un fichier compressé déjà chargé en mémoire en lisant via une vue.

Trois pièges à garder en tête : (1) le **choix de surcharge** au constructeur (`(usize)` = alloue,
`(void*, usize)` = vue) ; (2) une **vue devient propriétaire** dès qu'une écriture force une
réallocation — le pointeur externe d'origine n'est alors plus le *backing* ; (3) la mémoire est
**CRT** (`new[]`/`delete[]`), donc ne libérez jamais `Data()` via `NkFree` et ne mélangez pas avec un
allocateur custom. Enfin, ni `Close()` (no-op) ni un flux non-propriétaire ne libèrent quoi que ce
soit ; seul le destructeur d'un flux propriétaire rend la mémoire. Aucune protection thread.

### `NkFileStream` à fond

C'est le flux des **fichiers**, et son atout est de s'appuyer sur `NkFile` (NKFileSystem) : il hérite
du *path handling* `NkPath` et du **fallback `AAssetManager` sur Android** — on ouvre un asset
empaqueté dans l'APK comme un fichier ordinaire, sans code spécifique. L'ouverture **force toujours le
mode binaire** (pas de traduction de fins de ligne), ce qui est le bon défaut pour toutes les
données ; la table de mapping read/write/append vers `NkFileMode` est donnée plus haut. Le flux est
**RAII** : son destructeur ferme le fichier, on n'oublie donc jamais un descripteur. Domaines :

- **Rendu / assets** : charger maillages, textures, shaders compilés, scènes (`.nkb`, `.nkscene`),
  y compris depuis un APK Android grâce au fallback NkFile.
- **Outils / éditeur** : écrire un projet ou une sauvegarde, relire pour l'inspecteur, journaliser.
- **Audio** : ouvrir un fichier son en flux pour le *streaming* progressif.
- **IO / configuration** : lire/écrire des fichiers texte avec **BOM** correct grâce à `SetEncoding`.

`SetEncoding` est la vraie valeur ajoutée par rapport à `NkBinaryStream` : NkFile ne connaît pas
l'encodage, donc `NkFileStream` **écrit lui-même le BOM** en mode écriture (sous Windows :
`0xFEFF` pour UTF-16-LE, `EF BB BF` pour UTF-8 ; ailleurs : conserve l'encodage demandé, sinon force
UTF-8). `GetFile()` (les seules méthodes `noexcept`) ouvre la porte à l'interop bas niveau avec
`NkFile`. `Flush()` est délibérément un no-op (NkFile gère son vidage), et `Seek` reste **absolu** via
`NkSeekOrigin::NK_BEGIN`.

### `NkConsoleStream` à fond

Le flux console donne un canal d'**entrée/sortie terminal** unifié, choisi à la construction
(`NK_INPUT`, `NK_OUTPUT`, `NK_ERROR`). À l'écriture, il route vers les API natives — `WriteConsoleW`
si l'encodage est `NK_UTF16_LE`, `WriteConsoleA` sinon sous Windows, un `write(fd, …)` ailleurs — et
recalcule **localement** le bon handle (sortie ou erreur), ce qui le rend robuste malgré le bug
d'initialisation de `mHandle`. La lecture ne vaut que pour `NK_INPUT` (via `ReadConsoleW` / `read`).
Domaines :

- **Outils / éditeur** : sortie d'un outil en ligne de commande, lecture d'une saisie utilisateur.
- **Threading / diagnostic** : trace de log brute vers stderr (mais NKStream **n'est pas
  thread-safe** — sérialiser les écritures concurrentes reste à la charge de l'appelant).
- **IO** : pont console → fichier, en lisant un canal et en réécrivant dans un `NkFileStream`.

`SetEncoding`, sous Windows, bascule la page de code (`SetConsoleOutputCP(CP_UTF8)` pour l'UTF-8) ou
laisse `WriteConsoleW` gérer l'UTF-16 ; ailleurs il force l'UTF-8. La console n'ayant pas de notion de
position, `Seek`/`Tell`/`Size` sont neutres et `Close()` ne ferme **pas** les handles système. Les
templates `Read<CharT>`/`Write<CharT>` **masquent** ceux de la base (même logique, division par
`sizeof(CharT)`). Deux faits du code réel à connaître : le **ternaire du constructeur** attribue par
erreur `STD_OUTPUT_HANDLE` au `mHandle` du canal `NK_ERROR` (sans conséquence en écriture, car le
handle est recalculé), et **`IsEOF()` n'est pas redéfini** ici — il reste le pur virtuel de
`NkStream`, ce qui maintient la classe abstraite au vu de ces seuls headers.

### Le socle commun et les pièges transverses

- **Paire de primitives.** Tous les flux reposent sur le couple `ReadRaw`/`WriteRaw` ; les templates
  `Read<T>`/`Write<T>` ne sont qu'une couche de confort qui raisonne en éléments (et **tronque** au
  multiple de `sizeof(T)`).
- **`Seek` absolu partout.** Sur toute la hiérarchie, `Seek` prend une position `usize` depuis le
  début — jamais d'origine relative.
- **Mémoire.** `NkBinaryStream` utilise `new[]`/`delete[]` **CRT** : ne mélangez pas son `Data()`
  avec `NkFree`/NKMemory.
- **RAII inégal.** `NkFileStream` ferme dans son destructeur ; `NkBinaryStream` libère **si
  propriétaire** ; `NkConsoleStream` et `NkBinaryStream::Close()` ne libèrent rien.
- **Pas thread-safe.** Aucun flux ne se protège — sérialisez les accès concurrents vous-même.
- **Bugs réels à ne pas reproduire** (mais à connaître) : ternaire `NK_INPUT` dupliqué dans le ctor
  `NkConsoleStream` ; `Encoding::NK_UTF16_BE` jamais géré.

---

### Exemple récapitulatif

```cpp
#include "NKStream/NkBinaryStream.h"
#include "NKStream/NkFileStream.h"
#include "NKStream/NkConsoleStream.h"
using namespace nkentseu;

// 1) Composer un blob en mémoire (auto-extensible), puis l'écrire sur disque.
NkBinaryStream mem(64);                       // propriétaire (1 seul usize)
uint32 magic = 0x4E4B4253;
mem.Write(&magic, 1);                         // 4 octets
mem.Write(vertices.Data(), vertices.Size());  // N * sizeof(NkVertex)

NkFileStream file;
if (file.Open("out/mesh.nkb",
              NkStream::NK_WRITE_MODE | NkStream::NK_BINARY_MODE)) {
    file.Write(mem.Data(), mem.Size());       // déverse le tampon mémoire
}                                             // destructeur : Close() (RAII)

// 2) Relire avec positionnement absolu.
NkFileStream in;
if (in.Open("out/mesh.nkb",
            NkStream::NK_READ_MODE | NkStream::NK_BINARY_MODE)) {
    uint32 m = 0;
    in.Read(&m, 1);                           // lit le magic
    in.Seek(4);                               // Seek ABSOLU depuis le début
    // ... in.Read(buffer, count) pour la suite
}

// 3) Tracer vers la console en UTF-8.
NkConsoleStream out(NkConsoleStream::NK_OUTPUT);
out.SetEncoding(Encoding::NK_UTF8);
const char* done = "Maillage écrit\n";
out.Write(done, 16);                          // Write<char>
```

---

[← Index NKStream](README.md) · [Récap NKStream](../NKStream.md) · [Couche System](../README.md)
