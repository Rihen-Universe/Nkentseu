# Binaire et format natif

> Couche **System** · NKSerialization · Écrire une archive **sur disque** ou **dans un tampon** :
> le format plat **NKS0** (`NkBinaryWriter` / `NkBinaryReader`), le format binaire récursif **NKS1**
> (`NkNativeWriter` / `NkNativeReader`), et la **réflexion** qui sérialise un struct sans code à la
> main (`NkReflect` + macros).

Une fois qu'on a peuplé un `NkArchive` (l'arbre clé→valeur du module), il faut bien le **transformer
en octets** pour l'enregistrer, l'envoyer sur le réseau ou le ranger dans un cache. La question
n'est pas « comment sérialiser » — il y a déjà JSON pour ça — mais « **quel format d'octets** » : un
format qui se relit à l'œil et survit aux changements, ou un format **compact et rapide** qui empile
les valeurs en binaire. NKSerialization en propose deux, et c'est le cœur de cette page : **NKS0**,
un format *plat* où chaque valeur est stockée en **texte**, et **NKS1**, un format *récursif* où
tout est en **binaire** little-endian. Le compromis tient en une phrase : **le plat est simple et
debuggable mais dégrade les objets imbriqués en JSON texte ; le natif est compact et imbrique
vraiment, au prix d'octets illisibles.**

La dernière partie de la page règle un autre problème : écrire `archive.SetInt64("hp", e.hp)` à la
main pour chaque champ d'un struct est fastidieux et source d'oublis. La **réflexion** (`NkReflect`)
génère ce code à partir d'une simple liste de champs, en compile-time, sans RTTI ni allocation.

- **Namespaces** : `nkentseu` (binaire NKS0), `nkentseu::native` (natif NKS1), `nkentseu::NkReflect`
  (réflexion — c'est un **namespace**, pas une classe)
- **Headers** : `#include "NKSerialization/NkBinaryWriter.h"` · `NkBinaryReader.h` ·
  `NkNativeFormat.h` · `NkReflect.h`

Tous ces types s'appuient sur `NkArchive` (l'arbre de valeurs, documenté ailleurs) et travaillent
sur des `NkVector<nk_uint8>` (le tampon d'octets).

---

## Le format plat : NKS0 (`NkBinaryWriter` / `NkBinaryReader`)

C'est le format **le plus simple** du module, et le bon point de départ. Une archive NKS0 est un
**en-tête de 12 octets** (magic `'NKS0'` = `0x304B534E`, version 1, un compteur d'entrées) suivi
d'une **liste plate** d'entrées : pour chacune, la longueur de la clé (4 octets), la clé, un octet de
type, la longueur de la valeur (4 octets), puis la valeur. La particularité qui définit ce format :
**les valeurs sont stockées en texte brut**, pas en binaire. Un entier `42` est écrit `"42"`, un
booléen `"true"`, un flottant `"3.14"`. C'est ce qui le rend trivial à inspecter dans un éditeur
hexadécimal.

`NkBinaryWriter::WriteArchive(archive, outData)` produit ce tampon, `NkBinaryReader::ReadArchive(
data, size, outArchive, outError)` le relit. Les deux sont **statiques et sans état** — donc
thread-safe entre appels (seuls les tampons fournis par l'appelant exigent une synchro s'ils sont
partagés). Le `NkString* outError` optionnel du Reader reçoit un message détaillé en cas d'échec.

```cpp
NkArchive arc;
arc.SetInt64("score", 4200);
arc.SetString("name", "Rihen");

NkVector<nk_uint8> bytes;
NkBinaryWriter::WriteArchive(arc, bytes);     // -> tampon NKS0

NkArchive back;
NkString err;
if (!NkBinaryReader::ReadArchive(bytes.Data(), bytes.Size(), back, &err)) {
    // err = "Invalid binary magic" / "Unsupported binary version" / "Corrupted entry"
}
```

Le piège **essentiel** : NKS0 est un format *plat*. Un objet ou un tableau imbriqué n'a pas de place
dans cette liste à un niveau — le Writer le **dégrade en texte JSON** rangé dans le champ valeur
(type STRING). Et le Reader **ne le re-parse pas** : il vous rend la chaîne JSON telle quelle, à vous
de la repasser dans un JSON reader si vous voulez l'arbre. Pour de l'imbrication réelle, c'est NKS1
qu'il faut, pas NKS0.

> **En résumé.** NKS0 = en-tête 12 octets + liste plate d'entrées, **valeurs en texte** (`"42"`,
> `"true"`). `WriteArchive` / `ReadArchive`, statiques et sans état. Lisible à l'œil, mais les
> objets/tableaux imbriqués deviennent du **JSON texte non re-parsé**. Toujours tester le `bool` de
> retour : en erreur partielle, l'archive de sortie peut être incomplète.

---

## Le format natif : NKS1 (`NkNativeWriter` / `NkNativeReader`)

Quand on veut de la **compacité** et de l'**imbrication réelle**, on passe à NKS1. Ici tout est en
**binaire little-endian** : un entier 64 bits tient en 8 octets bruts, pas en chaîne de chiffres ; un
flottant est son motif IEEE 754 ; et surtout, un Object ou un Array imbriqué est encodé **comme une
sous-archive récursive**, pas dégradé en texte. C'est le format à préférer dès qu'une scène, un
prefab ou un message a une structure en arbre.

La disposition : un **en-tête de 12 octets** [magic `'NKS1'` = `0x314B534E`, version, flags, taille
décompressée], le **payload** [nombre d'entrées + entrées], puis un **footer optionnel de 8 octets**
(CRC32 + réservé). Deux différences de format avec NKS0 méritent attention : la clé est encodée sur
**2 octets** (contre 4 en NKS0), et la version courante est **2** — la v2 stocke les int64/uint64/
float64 en pleine largeur 8 octets, là où la v1 les tronquait à 4 octets (le Reader relit toujours
les deux).

`NkNativeWriter` offre **trois** points d'entrée, tous statiques, `[[nodiscard]]` et `noexcept` :

```cpp
NkVector<nk_uint8> out;
NkNativeWriter::WriteArchive(arc, out);                 // brut, sans CRC
NkNativeWriter::WriteArchiveWithChecksum(arc, out);     // ajoute un footer CRC32
NkNativeWriter::WriteArchiveCompressed(arc, out, true); // compression... = stub (voir piège)

NkArchive back;
NkString err;
NkNativeReader::ReadArchive(out.Data(), out.Size(), back, &err);
```

Le **CRC32** mérite qu'on s'y arrête : `WriteArchiveWithChecksum` calcule un CRC32 IEEE 802.3
(polynôme `0xEDB88320`, le même que zlib/libpng) sur tout le tampon sauf le footer, et le range en
fin. À la lecture, si le flag CHECKSUM est posé, `NkNativeReader::ReadArchive` recalcule le CRC et
**rejette** le tampon en cas de différence (`"CRC32 mismatch"`). C'est une **détection de corruption**,
pas une signature cryptographique — n'y voyez aucune garantie de sécurité.

Le piège **majeur** : la **compression LZ4 est un stub partout**. `WriteArchiveCompressed(...,true)`
n'écrit jamais de données réellement compressées et ne pose **jamais** le flag COMPRESSED ; et le
Reader ignore ce flag. Concrètement, le paramètre `compress` n'a aucun effet aujourd'hui, et un
fichier *réellement* compressé par un autre outil ne se relirait pas. Branchez LZ4 manuellement si
vous en avez besoin.

> **En résumé.** NKS1 = binaire **little-endian** récursif (Object/Array natifs), clé sur 2 octets,
> int/float pleine largeur 8 octets en v2. `WriteArchive` / `WithChecksum` / `Compressed` +
> `NkNativeReader::ReadArchive`. Compact et imbriqué — le format à préférer dès qu'il y a un arbre.
> **CRC32** = détection de corruption (pas crypto). **LZ4 = stub** : `compress` est sans effet.

---

## Sérialiser un objet : les helpers et la réflexion

Écrire `WriteArchive` puis relire `ReadArchive` à la main reste verbeux. Deux niveaux d'aide
existent. D'abord, les **helpers templates** de `nkentseu::native` enchaînent archive + écriture en
un appel, pourvu que votre type expose `Serialize`/`Deserialize` :

```cpp
struct Save { nk_bool Serialize(NkArchive&) const; nk_bool Deserialize(const NkArchive&); };

Save s;
NkVector<nk_uint8> bytes;
nkentseu::native::SerializeToNative(s, bytes);            // archive + WriteArchive en NKS1
nkentseu::native::DeserializeFromNative(bytes.Data(), bytes.Size(), s);
```

Mais écrire `Serialize`/`Deserialize` champ par champ est encore du travail manuel. C'est là
qu'intervient la **réflexion**. `NkReflect` est un **namespace** (et non une classe : on appelle
`nkentseu::NkReflect::Serialize(obj, arc)`) dont les fonctions, en compile-time, détectent si votre
type sait se sérialiser et génèrent l'aller-retour. Vous décrivez vos champs **une fois** avec des
macros, et le code de sérialisation pour **tous les types scalaires** (bool, entiers signés/non
signés de toute taille, flottants, `NkString`) est émis pour vous :

```cpp
struct Transform {
    float    x, y, z;
    int32    layer;
    NkString tag;

    NK_REFLECT_STRUCT(Transform,
        NK_REFLECT_FIELD(x) NK_REFLECT_FIELD(y) NK_REFLECT_FIELD(z)
        NK_REFLECT_FIELD(layer) NK_REFLECT_FIELD(tag),
        NK_REFLECT_FIELD_D(x) NK_REFLECT_FIELD_D(y) NK_REFLECT_FIELD_D(z)
        NK_REFLECT_FIELD_D(layer) NK_REFLECT_FIELD_D(tag))
};

Transform t;
NkArchive arc;
nkentseu::NkReflect::Serialize(t, arc);     // remplit l'archive depuis les champs
nkentseu::NkReflect::Deserialize(arc, t);   // relit les champs depuis l'archive
```

Le piège **silencieux** à connaître : un champ d'un type **non supporté** (pointeur, tableau C,
`void*`, type custom non réfléchi) est **ignoré sans erreur** — ni à la compilation, ni à
l'exécution. Si un champ « disparaît » de votre archive, c'est probablement ça : il faut alors lui
donner son propre `Serialize`/`Deserialize` (via `NkISerializable`) ou ajouter une branche manuelle.
Notez aussi que `NkString` est la **seule** chaîne supportée — pas de `std::string`.

> **En résumé.** Trois niveaux : à la main (`WriteArchive`/`ReadArchive`), helper
> (`SerializeToNative`/`DeserializeFromNative` si le type a `Serialize`/`Deserialize`), ou réflexion
> (`NkReflect::Serialize`/`Deserialize` + macros `NK_REFLECT_*`). La réflexion est **compile-time,
> zéro RTTI, zéro alloc**, mais **ignore silencieusement** les champs non supportés. `NkReflect` est
> un **namespace**.

---

## Aperçu de l'API

La liste de **tous** les éléments publics, en un coup d'œil. Chacun est détaillé dans la « Référence
complète ». Complexités et `noexcept` entre crochets quand c'est utile.

### Binaire NKS0 — `namespace nkentseu`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Écriture | `NkBinaryWriter::WriteArchive(archive, outData)` `[O(n×m)]` | Sérialise un `NkArchive` en tampon **NKS0** (valeurs en texte). |
| Lecture | `NkBinaryReader::ReadArchive(data, size, outArchive, outError = nullptr)` `[O(n)]` | Relit un tampon NKS0 ; `outError` reçoit un message d'échec. |

### Format natif NKS1 — `namespace nkentseu::native`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Constantes | `NK_NATIVE_MAGIC`, `NK_NATIVE_VERSION` (=2), `NK_NATIVE_FLAG_COMPRESSED`, `NK_NATIVE_FLAG_CHECKSUM` | Magic `'NKS1'`, version, bits de flags (compressé / checksum). |
| Types | `enum class NkNativeType : nk_uint8` | NULL/BOOL/INT64/UINT64/FLOAT64/STRING/OBJECT/ARRAY. |
| Intégrité | `NkCRC32::Compute(data, size)` `[noexcept, nodiscard, O(n)]` | CRC32 IEEE 802.3 (`0xEDB88320`), non cryptographique. |
| Écriture | `NkNativeWriter::WriteArchive(archive, out)` `[noexcept, nodiscard]` | Écrit NKS1 brut (sans CRC ni compression). |
| Écriture | `NkNativeWriter::WriteArchiveWithChecksum(archive, out)` `[noexcept]` | Ajoute un footer CRC32 (8 octets). |
| Écriture | `NkNativeWriter::WriteArchiveCompressed(archive, out, compress = true)` `[noexcept]` | Compression **stub** (`compress` sans effet). |
| Lecture | `NkNativeReader::ReadArchive(data, size, out, err = nullptr)` `[noexcept, nodiscard, O(n)]` | Relit NKS1, valide magic/version/CRC. |
| Helpers | `SerializeToNative<T>(obj, out, compress = false)` `[noexcept]` | Archive + écriture NKS1 d'un `T` ayant `Serialize`. |
| Helpers | `DeserializeFromNative<T>(data, size, obj, err = nullptr)` `[noexcept]` | Lecture NKS1 + `T::Deserialize`. |

### I/O little-endian — `namespace nkentseu::native::io` (inline, tous `noexcept`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Écriture | `WriteU8`, `WriteU16LE`, `WriteU32LE`, `WriteU64LE`, `WriteF64`, `WriteBytes` | Empile des octets LE dans un `NkVector<nk_uint8>`. |
| Lecture | `ReadU8`, `ReadU16LE`, `ReadU32LE`, `ReadU64LE`, `ReadF64` (offset par référence) | Relit en LE, **bornes vérifiées** (0 / 0.0 si dépassement). |

### Réflexion — `namespace nkentseu::NkReflect` + macros

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| API | `NkReflect::Serialize<T>(obj, arc)` `[noexcept]` | Sérialise via tag réflexion ou `Serialize` ; `false` sinon. |
| API | `NkReflect::Deserialize<T>(arc, obj)` `[noexcept]` | Symétrique. |
| Macro (forme longue) | `NK_REFLECT_BEGIN(Type)` … `NK_REFLECT_FIELD(f)` / `NK_REFLECT_FIELD_AS(f, key)` … `NK_REFLECT_END_SERIALIZE()` … `NK_REFLECT_FIELD_D(f)` / `NK_REFLECT_FIELD_AS_D(f, key)` … `NK_REFLECT_END()` | Décrit les champs **deux fois** (ser. puis déser.). |
| Macro (forme `_BASE`) | `NK_REFLECT_BEGIN(Type)` … `NK_REFLECT_FIELD_BASE(f)` / `NK_REFLECT_FIELD_BASE_AS(f, keySer, keyDeser)` … `NK_REFLECT_END()` | Un seul listage, **sans** `END_SERIALIZE`. |
| Macro (tout-en-un) | `NK_REFLECT_STRUCT(Type, SerFields, DeserFields)` | `BEGIN` + ser. + `END_SERIALIZE` + déser. + `END`. |

---

## Référence complète

Chaque élément repris en détail : comportement, complexité, et usages dans les différents domaines du
moteur. Les éléments triviaux sont brefs ; les pièges et les choix de format, à fond.

### `NkBinaryWriter::WriteArchive` — produire du NKS0

Le Writer vide d'abord `outData`, réserve approximativement `16 + entrées × 32` octets, écrit
l'en-tête (magic, version, réservé, compteur), puis chaque entrée. Les **scalaires** sont écrits en
texte canonique (`"42"`, `"true"`, `"3.14"`), le **null** comme une valeur de longueur 0, et les
**objets/tableaux** comme du **texte JSON** dans un champ de type STRING. Complexité `O(n × m)`
(`n` entrées, `m` taille moyenne des valeurs JSON). Sans état → thread-safe.

- **Outils / éditeur** — c'est le format à choisir pour des **fichiers de config lisibles à l'œil**
  ou un dump diagnostic qu'on ouvre dans un éditeur hexa : on relit directement `"true"` ou `"42"`.
- **IO / réseau** — convient pour de petits *payloads* plats (paires clé/valeur scalaires) où la
  lisibilité prime sur la taille ; au-delà, la verbosité du texte coûte cher.
- **Gameplay / sauvegarde** — pour un état **plat** (un score, un nom, un niveau atteint), c'est
  immédiat ; pour un état **en arbre** (inventaire, monde), NKS0 dégrade l'imbrication en JSON texte,
  préférez NKS1.

### `NkBinaryReader::ReadArchive` — relire du NKS0

Le Reader valide le header (≥ 12 octets, magic, version 1), puis lit chaque entrée
(keyLen/type/valueLen) avec un *bounds-check*, reconstruit la valeur depuis son texte
(`NkString::ToInt64/ToUInt64/ToDouble`, `"true"`/`"false"` **sensible à la casse** pour les booléens,
texte tel quel pour String/Null) et l'insère via `SetValue`. Des octets en trop à la fin produisent
un **avertissement non fatal** (rangé dans `outError`). Complexité `O(n)`.

Le **comportement d'erreur** est ce qu'il faut retenir : en cas de problème, l'archive de sortie peut
être **partielle** — testez **toujours** le `bool` de retour. Les préfixes de message observables :
`"Invalid binary magic"`, `"Unsupported binary version"`, `"Corrupted entry"`. Et le rappel central :
les objets/tableaux stockés en JSON texte ne sont **pas re-parsés** — vous récupérez la chaîne, à
vous de la passer à un JSON reader.

- **Outils / éditeur** — charger une config écrite par `WriteArchive`, en surveillant `outError` pour
  signaler une corruption à l'utilisateur.
- **IO / réseau** — valider un *payload* reçu avant de l'exploiter ; ne jamais consommer l'archive
  sans avoir vérifié le retour.

### Constantes NKS1 et `NkNativeType`

Les `constexpr` du format : `NK_NATIVE_MAGIC` (`'NKS1'`), `NK_NATIVE_VERSION` (=2),
`NK_NATIVE_FLAG_COMPRESSED` (bit 0) et `NK_NATIVE_FLAG_CHECKSUM` (bit 1). L'`enum class NkNativeType`
énumère les huit types d'une valeur native : `NULL` (0 octet), `BOOL` (1), `INT64`/`UINT64` (8 octets
LE), `FLOAT64` (8 octets IEEE 754), `STRING` (longueur UTF-8), `OBJECT` (sous-archive récursive) et
`ARRAY` (compteur + N éléments). Ce sont des constantes de **format** : on les croise surtout en
debug ou si l'on écrit un outil tiers qui lit nos fichiers. Le seul vrai point d'attention est la
**version 2** : elle stocke les entiers/flottants en pleine largeur 8 octets, là où la v1 tronquait à
4 — le Reader relit les deux, mais écrivez toujours en v2.

### `NkCRC32::Compute` — l'intégrité

`Compute(data, size)` renvoie le CRC32 IEEE 802.3 (polynôme `0xEDB88320`), compatible zlib/libpng.
L'algorithme est classique : `crc = 0xFFFFFFFF`, puis `crc = table[(crc ^ octet) & 0xFF] ^ (crc >>
8)` pour chaque octet, et l'on renvoie `crc ^ 0xFFFFFFFF`. La table de 256 entrées est un singleton
de Meyer (initialisation thread-safe en C++11). `[[nodiscard]] noexcept`, `O(n)`. C'est une
**somme de contrôle de corruption**, pas une empreinte cryptographique.

- **IO / réseau** — vérifier qu'un fichier ou un paquet n'a pas été tronqué/altéré en transit ; c'est
  exactement ce que `WriteArchiveWithChecksum` automatise.
- **Outils / cache** — taguer un asset compilé par le CRC de sa source pour détecter qu'il est
  périmé ; comparer deux versions d'un blob.
- **Sécurité** — **à ne pas utiliser** : un CRC se forge trivialement ; pour une intégrité
  adversariale, il faut un hash cryptographique (hors de ce module).

### `NkNativeWriter` — les trois écritures NKS1

Toutes statiques, sans état, `[[nodiscard]] noexcept`, et **renvoient `true`** actuellement
(le payload est pré-réservé à 512 octets). Elles diffèrent par le footer :

- `WriteArchive(archive, out)` — NKS1 brut, ni CRC ni compression. Le choix par défaut.
- `WriteArchiveWithChecksum(archive, out)` — ajoute le footer CRC32 (4 octets de CRC + 4 réservés),
  le CRC couvrant tout sauf le footer. À préférer dès que le tampon part sur disque ou réseau.
- `WriteArchiveCompressed(archive, out, compress = true)` — **stub** : le bloc de compression est un
  no-op (`compress` n'a aucun effet, le flag COMPRESSED n'est jamais posé). Présent pour brancher LZ4
  plus tard ; aujourd'hui équivalent à `WriteArchive`.

En interne, la clé est encodée sur **2 octets** (contre 4 en NKS0), et l'écriture est récursive pour
les Object/Array — c'est ce qui fait la vraie imbrication de NKS1.

- **Rendu / GPU** — sérialiser des descripteurs de matériaux, de pipelines ou un cache de shaders
  compilés : compact, et l'arbre (passes, paramètres) est préservé sans dégradation JSON.
- **ECS / scène** — enregistrer une scène ou un prefab via `SerializeToNative` ; l'imbrication
  (entités → composants → sous-objets) tient nativement.
- **Animation / audio** — empaqueter des courbes, des clips, des banques de sons dans un blob compact
  avec checksum.
- **IO / réseau** — `WithChecksum` pour un *payload* dont on veut détecter la corruption à l'arrivée.

### `NkNativeReader::ReadArchive` — relire du NKS1

Valide `size ≥ 12`, le magic, et `version >= 1 && version <= NK_NATIVE_VERSION`. Si le flag CHECKSUM
est posé, il lit le CRC stocké aux derniers 8 octets, recalcule `NkCRC32::Compute(data, size - 8)` et
**rejette** en cas de différence. Puis il parse le payload récursivement (Object → sous-archive,
Array → compteur + N nœuds). `[[nodiscard]] noexcept`, `O(n)`. Les messages d'erreur **exacts** sont
explicites : `"Native archive too small"`, `"Invalid native magic"`, `"Unsupported native version
%u"`, `"Missing checksum footer"`, `"CRC32 mismatch"`, et côté parsing `"Truncated entry count"`,
`"Truncated key length"`, `"Truncated key"`, `"Truncated node header"`, `"Truncated node value"`,
`"Unknown native type %u"`.

Deux pièges : en erreur partielle, `out` peut être **incomplet** (testez le retour) ; et la
**compression réelle n'est pas gérée** — un fichier authentiquement compressé ne se lirait pas (le
flag COMPRESSED est ignoré).

- **Chargement d'asset** — relire une scène/un prefab/un cache, en remontant l'erreur exacte à
  l'éditeur si le fichier est corrompu ou trop vieux.
- **IO / réseau** — désérialiser un message reçu, le CRC garantissant qu'on n'agit pas sur des
  octets tronqués.

### Helpers `SerializeToNative` / `DeserializeFromNative`

Sucre syntaxique pour les types qui exposent leur propre (dé)sérialisation. `SerializeToNative(obj,
out, compress)` crée un `NkArchive`, appelle `obj.Serialize(archive)` (et renvoie `false` si elle
échoue), puis écrit en NKS1 — avec checksum/compression si `compress` (rappel : compression = stub).
`DeserializeFromNative(data, size, obj, err)` relit l'archive puis appelle `obj.Deserialize(archive)`.
Le type `T` doit donc fournir `nk_bool Serialize(NkArchive&) const` et `nk_bool Deserialize(const
NkArchive&)`. C'est la voie courte pour **persister un objet du moteur** sans manipuler le Writer/
Reader à la main.

### Les helpers `io::` little-endian

Briques bas niveau, inline et toutes `noexcept`, utilisées par le Writer/Reader natifs mais
exploitables directement quand on écrit son **propre format binaire**. Les `WriteU8/16/32/64LE`
empilent des octets de poids croissant dans un `NkVector<nk_uint8>` ; `WriteF64` *bit-cast* le
flottant en `uint64` (via `memcpy`) avant de l'écrire ; `WriteBytes` copie un bloc brut. À la lecture,
`ReadU8/16/32/64LE` et `ReadF64` prennent un **offset par référence** qu'elles avancent, et sont
**bornes vérifiées** : un dépassement renvoie silencieusement `0` (ou `0.0`) sans signaler l'erreur —
le vrai *bounds-check* applicatif est dans le Reader, pas ici. Toujours **little-endian**, aucun
support big-endian.

- **Outils / formats maison** — écrire un en-tête de fichier personnalisé, un index, un tampon réseau
  compact sans passer par tout l'appareillage `NkArchive`.
- **GPU / sérialisation de mesh** — empaqueter des sommets/indices dans un blob d'octets LE.
- **Piège** — le retour `0` silencieux sur dépassement : c'est à l'appelant de vérifier que l'offset
  reste cohérent, ces helpers ne lèvent rien.

### `NkReflect::Serialize` / `Deserialize`

Le point d'entrée public de la réflexion (rappel : **namespace**, pas classe). `Serialize<T>(obj,
arc)` : si `T` porte le tag réflexion (posé par les macros), appelle son `NkReflectSerialize` et
renvoie `true` ; sinon, si `T` a une méthode `Serialize`, la délègue ; sinon renvoie `false`.
`Deserialize<T>(arc, obj)` est symétrique. Tout est résolu en **compile-time** (`if constexpr`),
**sans RTTI ni allocation dynamique** dans les helpers — seule l'archive sous-jacente n'est pas
thread-safe (à synchroniser si partagée). Les types scalaires gérés : bool, entiers signés/non signés
de **toute** taille (8→64 bits, castés en int64/uint64), float32/float64, et `NkString`.

- **Éditeur / inspecteur** — sérialiser automatiquement les composants d'une entité pour les afficher
  ou les sauvegarder, sans écrire un `Serialize` par type.
- **ECS / gameplay** — persister l'état d'un composant déclaré avec les macros ; ajouter un champ se
  résume à ajouter une ligne `NK_REFLECT_FIELD`.
- **Piège transverse** — un champ **non supporté** (pointeur, tableau C, type non réfléchi) est
  **ignoré sans erreur**. S'il manque dans l'archive, c'est qu'il faut un `Serialize`/`Deserialize`
  manuel (via `NkISerializable`) ou une branche `else if constexpr`.

### Les macros de réflexion

À placer dans la section **public** d'un struct/classe ; les noms de champs sont stringifiés (`#f`)
pour servir de clés d'archive. Elles génèrent `using NkReflectTag = void;` (le tag détecté par
`NkReflect`) plus deux statiques `NkReflectSerialize` / `NkReflectDeserialize`. **Deux styles** qui
**ne se mélangent pas** :

- **Forme longue** — `NK_REFLECT_BEGIN(Type)`, puis la liste de sérialisation
  (`NK_REFLECT_FIELD(f)` ou `NK_REFLECT_FIELD_AS(f, "clef")` pour une clé personnalisée),
  `NK_REFLECT_END_SERIALIZE()`, puis la liste **symétrique** de désérialisation
  (`NK_REFLECT_FIELD_D(f)` / `NK_REFLECT_FIELD_AS_D(f, "clef")` — la clé doit matcher), et
  `NK_REFLECT_END()`. Les champs sont listés **deux fois**.
- **Forme `_BASE`** — `NK_REFLECT_BEGIN(Type)`, un seul listage avec `NK_REFLECT_FIELD_BASE(f)` (qui
  émet ser. **et** déser. d'un coup) ou `NK_REFLECT_FIELD_BASE_AS(f, "clefSer", "clefDeser")` (clés
  distinctes, utile pour migrer un format), puis `NK_REFLECT_END()` **sans** `END_SERIALIZE`.
- **Tout-en-un** — `NK_REFLECT_STRUCT(Type, SerFields, DeserFields)` enchaîne `BEGIN` + champs de
  sérialisation + `END_SERIALIZE` + champs de désérialisation + `END`. Pratique mais liste, là encore,
  les champs deux fois.

Le piège : **ne pas mélanger** `FIELD`/`FIELD_D` avec `FIELD_BASE` dans le même type — les deux
formes génèrent des structures d'accolades différentes, et les combiner casse l'appariement
(erreur de compilation). Choisissez une forme et tenez-vous-y. Et rappel : `NkString` est la **seule**
chaîne supportée.

### Pièges transverses

- **NKS0 vs NKS1** — NKS0 est plat, valeurs en texte, objets/tableaux dégradés en JSON **non
  re-parsé** (clé 4 octets) ; NKS1 est binaire récursif réel, clé 2 octets, int/float pleine largeur.
  NKS1 pour l'imbrication et la compacité, NKS0 pour le legacy lisible.
- **LZ4 = stub partout** — `WriteArchiveCompressed(...,true)` n'écrit jamais de données compressées
  et ne pose jamais le flag ; un fichier réellement compressé ne se lirait pas.
- **CRC32 footer** — lu via un `reinterpret_cast` aux derniers 8 octets : suppose un tampon contigu
  valide, toujours little-endian, sans portage big-endian.
- **Statelessness** — tous les Writer/Reader sont statiques sans état (thread-safe entre appels) ;
  mais les tampons de sortie et les `NkArchive` fournis par l'appelant exigent une synchro externe
  s'ils sont partagés entre threads.

---

### Exemple

```cpp
#include "NKSerialization/NkBinaryWriter.h"
#include "NKSerialization/NkBinaryReader.h"
#include "NKSerialization/NkNativeFormat.h"
#include "NKSerialization/NkReflect.h"
using namespace nkentseu;

// 1. NKS0 plat : config lisible à l'œil.
NkArchive cfg;
cfg.SetInt64("volume", 80);
cfg.SetString("lang", "fr");
NkVector<nk_uint8> flat;
NkBinaryWriter::WriteArchive(cfg, flat);          // valeurs en texte ("80", "fr")

// 2. NKS1 natif + checksum : un blob compact, corruption détectable.
NkVector<nk_uint8> blob;
native::NkNativeWriter::WriteArchiveWithChecksum(cfg, blob);
NkArchive back;
NkString err;
if (!native::NkNativeReader::ReadArchive(blob.Data(), blob.Size(), back, &err)) {
    // err = "CRC32 mismatch" / "Invalid native magic" / ...
}

// 3. Réflexion : un struct se sérialise sans code à la main.
struct Player {
    int32 hp; float speed; NkString name;
    NK_REFLECT_STRUCT(Player,
        NK_REFLECT_FIELD(hp) NK_REFLECT_FIELD(speed) NK_REFLECT_FIELD(name),
        NK_REFLECT_FIELD_D(hp) NK_REFLECT_FIELD_D(speed) NK_REFLECT_FIELD_D(name))
};
Player p{ 100, 5.5f, "Rihen" };
NkArchive parc;
NkReflect::Serialize(p, parc);                    // remplit depuis les champs
NkReflect::Deserialize(parc, p);                  // relit dans les champs
```

---

[← Index NKSerialization](README.md) · [Récap NKSerialization](../NKSerialization.md) · [Couche System](../README.md)
