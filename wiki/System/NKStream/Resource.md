# L'interface de ressource

> Couche **System** · NKStream · Le **contrat** que toute ressource média décodable doit
> respecter pour se charger et se sauvegarder côté CPU : `NKIResource`.

Un moteur passe son temps à **lire et écrire des médias** : une texture vient d'un `.png`, une
police d'un `.ttf`, un son d'un `.wav`. Chacun a son propre format binaire, son propre codec — et
pourtant, du point de vue du reste du moteur, ce sont tous « la même chose » : quelque chose qu'on
**charge depuis quelque part** (un fichier, un bloc mémoire, un flux réseau) et qu'on **rend en
RAM** sous une forme exploitable. La tentation serait que chaque type invente ses propres méthodes
(`NkImage::Open`, `NkFont::ReadFile`, `NkAudioSample::Decode`…), chacune avec sa signature. Le code
qui orchestre le chargement — un gestionnaire d'assets, un éditeur, un *streamer* — devrait alors
connaître chaque type en particulier. `NKIResource` casse ce couplage : c'est une **interface
abstraite** unique qui dit, une fois pour toutes, *comment* on charge et sauvegarde une ressource,
sans rien dire de *ce qu'elle est*.

L'idée tient en une phrase : **`NKIResource` décrit un codec CPU**. Elle décode un fichier, un
buffer ou un flux vers de la mémoire vive, et fait le chemin inverse pour sauvegarder. C'est tout.
Ce n'est **pas** une couche GPU : une texture sur la carte graphique n'implémente pas cette
interface — elle *consomme* une `NKIResource` CPU (un `NkImage` chargé), puis uploade les pixels via
son propre device. Et ce n'est **pas** de la sérialisation d'objets : `NKIResource` parle de
**formats média** (PNG, TTF, WAV), là où NKSerialization (`NkISerializable`) parle d'**objets
structurés** (une scène, une entité, leurs champs). Les deux cohabitent, ils ne se recouvrent pas.

- **Namespace** : `nkentseu`
- **Header** : `#include "NKStream/NKIResource.h"` (s'inclut directement, pas de parapluie)
- **Dépendance** : `NKCore/NkTypes.h` (fournit `usize`, `uint8`). Pour les surcharges `*Stream`,
  inclure aussi `NKStream/NkStream.h` vous-même (le header ne fait qu'une *forward declaration* de
  `NkStream`).

---

## Charger : les cinq méthodes du contrat

Une ressource doit pouvoir naître de **trois sources** différentes, parce que les contextes diffèrent.
On charge depuis un **fichier** (`LoadFromFile`) dans le cas courant, l'asset posé sur le disque. On
charge depuis la **mémoire** (`LoadFromMemory`) quand les octets sont déjà là — un asset empaqueté
dans une archive, reçu par le réseau, ou embarqué dans le binaire. Et on charge depuis un **flux**
(`LoadFromStream`) quand la source est un `NkStream` qu'on lit au fil de l'eau — un fichier ouvert
en lecture continue, un tampon décompressé à la volée.

Ces trois portes mènent au même endroit : après un chargement réussi, la ressource contient ses
données en RAM. Deux méthodes complètent l'état : `IsValid()` répond « ai-je des données valides
chargées ? » (c'est un `const`, il n'altère rien), et `Unload()` libère ce qui a été chargé et
remet l'objet à zéro. Les cinq sont **pures** (`= 0`) : toute classe qui se prétend `NKIResource`
**doit** les fournir, sans exception. C'est le minimum vital.

```cpp
NkImage img;                          // implémente NKIResource
if (img.LoadFromFile("hero.png")) {   // décode le PNG en RAM
    // ... img.IsValid() == true, pixels disponibles
    img.Unload();                     // libère les pixels
}
```

Ce n'est **pas** une fabrique : `NKIResource` ne crée rien, ne devine pas le type d'un fichier, ne
choisit pas le codec. C'est à l'implémentation concrète (`NkImage`, `NkFont`…) de savoir lire son
format. L'interface se contente d'imposer la *forme* des appels.

> **En résumé.** Cinq méthodes pures, obligatoires : `LoadFromFile` / `LoadFromMemory` /
> `LoadFromStream` (trois sources, même destination = la RAM), `IsValid()` (état, `const`) et
> `Unload()` (libération). Toutes retournent `bool` sauf `Unload` (`void`). C'est le contrat
> minimal de tout codec CPU.

---

## Sauvegarder : trois méthodes optionnelles

Tous les codecs ne savent pas **écrire** leur format — beaucoup ne font que décoder. La sauvegarde
est donc **optionnelle** : `SaveToFile`, `SaveToMemory` et `SaveToStream` ont une **implémentation
par défaut** dans l'interface qui se contente de retourner `false` (« je ne sais pas écrire ce
format »). Une implémentation qui *sait* encoder les surcharge ; les autres les laissent telles
quelles. Les trois sont `const` : sauvegarder ne modifie pas la ressource.

`SaveToMemory(uint8*& out, usize& size)` mérite une attention particulière, car c'est le **seul
endroit de toute l'interface où la mémoire est en jeu**. Quand l'écriture réussit, l'implémentation
**alloue** le buffer de sortie et le renvoie via `out` (référence sur pointeur) avec sa taille dans
`size`. Et ici tombe la règle dure de Nkentseu : ce buffer est alloué par
`nkentseu::memory::NkAlloc` — l'appelant **doit** le libérer par `nkentseu::memory::NkFree`, et
**jamais** par `std::free` ni `delete[]`. Mélanger l'allocateur maison et le tas CRT provoque une
corruption du tas (Windows `c0000374`). C'est non négociable.

```cpp
uint8* bytes = nullptr;
usize  count = 0;
if (img.SaveToMemory(bytes, count)) {     // encode en PNG dans un buffer alloué
    network.Send(bytes, count);
    nkentseu::memory::NkFree(bytes);      // OBLIGATOIRE — surtout pas std::free !
}
```

> **En résumé.** Trois sauvegardes optionnelles (défaut = `false`, `const`). `SaveToFile` /
> `SaveToStream` écrivent vers une destination fournie. `SaveToMemory` **alloue** le buffer
> (`out`, `size` en out-params) : libérez-le impérativement avec `nkentseu::memory::NkFree`, jamais
> `std::free`/`delete[]`.

---

## Aperçu de l'API

`NKIResource` est une pure interface : un destructeur virtuel et huit méthodes virtuelles, rien
d'autre (aucun `static`, aucune fabrique, aucun opérateur, enum, struct ni macro). Annotations :
**aucune** (`noexcept`, `nodiscard`, `constexpr` absents partout).

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `virtual ~NKIResource()` | Destructeur virtuel (destruction polymorphe via pointeur sur base). |
| Chargement (pur `=0`) | `LoadFromFile(const char* path)` → `bool` | Charge/décode depuis un chemin fichier. |
| Chargement (pur `=0`) | `LoadFromMemory(const void* data, usize size)` → `bool` | Charge/décode depuis un buffer mémoire. |
| Chargement (pur `=0`) | `LoadFromStream(NkStream& stream)` → `bool` | Charge/décode depuis un flux (référence non-const). |
| État (pur `=0`) | `IsValid() const` → `bool` | Données valides chargées ? |
| État (pur `=0`) | `Unload()` → `void` | Libère / réinitialise les données. |
| Sauvegarde (défaut `false`) | `SaveToFile(const char* path) const` → `bool` | Écrit vers un chemin fichier. |
| Sauvegarde (défaut `false`) | `SaveToMemory(uint8*& out, usize& size) const` → `bool` | Sérialise vers un buffer **alloué par l'impl** (NkAlloc → NkFree). |
| Sauvegarde (défaut `false`) | `SaveToStream(NkStream& stream) const` → `bool` | Écrit vers un flux (référence non-const). |
| Types | `usize`, `uint8` | De `NKCore/NkTypes.h` : taille mémoire, octet. |

---

## Référence complète

Chaque élément est repris ici en détail. Les méthodes triviales sont décrites brièvement ; le
contrat de chargement et la règle d'ownership de `SaveToMemory` le sont **à fond**, avec leurs
usages dans les différents domaines du moteur.

### `~NKIResource` — le destructeur virtuel

Déclaré `virtual ~NKIResource() = default;`. Sa seule raison d'être est de permettre la **destruction
polymorphe** : si vous tenez une ressource par un `NKIResource*` (par exemple dans un cache générique
d'assets indexé par chemin), `delete` sur ce pointeur de base appellera bien le destructeur de la
classe concrète — donc libérera réellement les pixels d'un `NkImage` ou les glyphes d'un `NkFont`.
Sans ce `virtual`, détruire par la base laisserait fuir les données concrètes. Trivial mais
indispensable à toute hiérarchie destinée à être manipulée par pointeur de base.

### Les trois `LoadFrom*` — naître de trois sources

C'est le cœur du contrat. Les trois méthodes mènent au même résultat (les données décodées en RAM)
mais répondent à des contextes d'I/O distincts, et c'est précisément cette **uniformité de forme sur
trois sources** qui rend l'interface utile.

- `LoadFromFile(const char* path)` — la voie courante. On donne un chemin, l'implémentation ouvre,
  lit, décode, renvoie `true` au succès. Comportement et complexité dépendent **entièrement du
  codec concret** : décoder un PNG, c'est inflater + défiltrer ; décoder un WAV, c'est lire un
  en-tête puis copier des échantillons. L'interface ne promet rien d'autre que la signature.
- `LoadFromMemory(const void* data, usize size)` — les octets sont déjà en mémoire. On passe le
  pointeur et la longueur en octets.
- `LoadFromStream(NkStream& stream)` — la source est un flux lu au fil de l'eau (référence
  **mutable** : lire avance la position du flux). Voir [NkStream](Stream.md).

Cas d'usage, par domaine :
- **Rendu / GPU** — `NkImage` charge une texture (`LoadFromFile("albedo.png")`), puis le device la
  consomme pour uploader vers la VRAM. La ressource CPU est jetable une fois l'upload fait
  (`Unload()`), ce qui libère la copie RAM.
- **UI / 2D** — atlas d'icônes, sprites, fonds : tous arrivent via `LoadFromFile` ou, s'ils sont
  empaquetés dans une archive de jeu, via `LoadFromMemory` (on a déjà le bloc décompressé).
- **Audio** — `NkAudioSample` décode un `.wav`/`.mp3`/`.ogg`/`.flac` ; pour un asset *streamé*,
  `LoadFromStream` lit progressivement plutôt que de tout charger d'un coup.
- **IO / réseau** — un asset reçu en TCP arrive sous forme de buffer : `LoadFromMemory(data, size)`
  le décode sans toucher au disque. Idéal pour le contenu distant ou les patchs téléchargés.
- **Outils / éditeur** — un gestionnaire d'assets générique tient un `NKIResource*` par chemin,
  appelle `LoadFromFile`, surveille la validité via `IsValid()`, recharge à chaud après édition.
- **ECS / gameplay** — les composants ne stockent en général qu'un *handle* vers la ressource
  chargée par le cache ; le composant lui-même n'implémente pas `NKIResource` (il ne décode rien).

### `IsValid` — l'état chargé

`virtual bool IsValid() const = 0;`. Une question d'état, `const`, sans effet de bord : la ressource
contient-elle des données valides ? On l'interroge typiquement **après** un `LoadFrom*` pour
confirmer le succès au-delà du seul `bool` de retour, ou **avant** d'utiliser une ressource dont on
n'est pas sûr qu'elle ait été chargée (un *handle* paresseux, une entrée de cache encore vide). Dans
un gestionnaire d'assets, c'est le drapeau qui distingue « pas encore chargé », « chargé OK » et
« échec de chargement » (couplé au retour de `LoadFrom*`).

### `Unload` — libérer

`virtual void Unload() = 0;` — la seule méthode pure qui retourne `void`. Elle libère les données en
RAM et remet la ressource dans un état « vide » (après quoi `IsValid()` doit renvoir `false`).
Utile pour :
- **Gestion mémoire / streaming** — décharger les assets hors champ (une zone de niveau qu'on quitte)
  pour récupérer la RAM, sans détruire l'objet lui-même (qu'on rechargera peut-être plus tard).
- **Rendu / GPU** — après l'upload vers la VRAM, `Unload()` libère la copie CPU devenue inutile.
- **Éditeur** — recharger un asset modifié : `Unload()` puis `LoadFromFile` sur le nouveau contenu.

Notez la distinction avec le destructeur : `Unload()` vide les données mais **garde l'objet vivant**
(réutilisable), là où `~NKIResource` détruit l'objet entier.

### Les trois `SaveTo*` — la sauvegarde optionnelle

Contrairement au chargement, sauvegarder n'est **pas** obligatoire : les trois `SaveTo*` ont une
implémentation par défaut qui retourne `false`. Sémantiquement, ce `false` signifie « ce codec ne
sait pas écrire ce format » — beaucoup de décodeurs sont en lecture seule. Les trois sont `const` :
écrire n'altère pas la ressource. À noter que `SaveToStream` prend tout de même un `NkStream&`
**mutable** : c'est le *flux* qu'on modifie (on y écrit), pas la ressource.

- `SaveToFile(const char* path) const` — écrit vers un chemin. Défaut : ignore `path`, retourne
  `false`. Usage : exporter une texture procédurale, capturer une frame en `.png`, sauver un asset
  édité depuis l'éditeur.
- `SaveToStream(NkStream& stream) const` — écrit vers un flux. Défaut : `false`. Usage : empaqueter
  plusieurs ressources dans une archive ouverte en écriture, sérialiser vers un tampon réseau.
- `SaveToMemory(uint8*& out, usize& size) const` — détaillé ci-dessous, c'est le cas sensible.

Cas d'usage de la sauvegarde, par domaine :
- **Outils / éditeur** — bouton « Exporter » qui réencode une texture/un atlas généré.
- **Rendu** — screenshots, débogage de cibles de rendu (dump d'une G-buffer en image).
- **IO / réseau** — produire un buffer à envoyer (`SaveToMemory`) sans passer par un fichier
  temporaire.

### `SaveToMemory` — l'ownership du buffer (règle dure)

`virtual bool SaveToMemory(uint8*& out, usize& size) const` est la **seule** méthode de l'interface
qui touche à l'allocation, et donc la seule qui porte un contrat mémoire. Quand l'écriture réussit
(`true`), l'implémentation a **alloué** le buffer de sortie : `out` pointe vers les octets encodés,
`size` contient leur nombre. Les deux sont des **out-params** (références : `uint8*&` et `usize&`).

La règle, documentée dans le header lui-même : le buffer est alloué via `nkentseu::memory::NkAlloc`,
donc l'appelant **doit** le libérer avec `nkentseu::memory::NkFree`. **Jamais** `std::free`, jamais
`delete[]`. La raison est mécanique : l'allocateur maison de NKMemory et le tas du CRT sont deux tas
distincts ; rendre à l'un un bloc pris à l'autre corrompt le tas (sous Windows, plantage `c0000374`,
souvent loin du site fautif et difficile à diagnostiquer). C'est le même piège que pour les buffers
de sortie des codecs `Encode` de NKImage. Pensez « tout ce qui sort par `out` doit retourner par
`NkFree` ». L'interface elle-même ne fait **aucune** autre gestion mémoire et **aucune** gestion de
*threading* : ce contrat d'ownership est le seul.

### Les types `usize` et `uint8`

Issus de `NKCore/NkTypes.h`. `usize` est le type des tailles mémoire (paramètre `size` de
`LoadFromMemory` et `SaveToMemory`). `uint8` est l'octet, élément du buffer de `SaveToMemory`. Voir
[NKCore](../../Foundation/NKCore.md).

### Le tableau d'ensemble : qui implémente, qui ne fait que consommer

| Rôle | Type | Implémente `NKIResource` ? |
|------|------|----------------------------|
| Image (PNG/JPG/HDR…) | `NkImage` | **Oui** — décode/encode des pixels en RAM. |
| Police (TTF/OTF) | `NkFont` | **Oui** — charge les glyphes en RAM. |
| Échantillon audio (WAV/MP3/OGG/FLAC) | `NkAudioSample` | **Oui** — décode les échantillons en RAM. |
| Texture GPU | (device propre) | **Non** — *consomme* un `NkImage` CPU puis uploade en VRAM. |
| Objet structuré (scène, entité) | `NkISerializable` (NKSerialization) | **Non** — sérialisation d'objets, pas de format média. |

La frontière est nette : `NKIResource` = **codecs de formats média, côté CPU**. Le GPU est au-dessus
(il consomme), la sérialisation d'objets est à côté (autre interface, autre but).

---

### Exemple récapitulatif

```cpp
#include "NKStream/NKIResource.h"
#include "NKStream/NkStream.h"        // requis pour les surcharges *Stream
using namespace nkentseu;

void ChargerEtRedistribuer(NKIResource& res) {
    // 1. Charger depuis trois sources possibles (ici, fichier).
    if (!res.LoadFromFile("asset.png")) return;
    if (!res.IsValid()) return;       // confirme l'état

    // 2. La consommer (upload GPU, lecture des échantillons, etc.)
    // ... le device GPU lit les données CPU et uploade ...

    // 3. Réencoder vers un buffer mémoire (si le codec sait écrire).
    uint8* out = nullptr;
    usize  size = 0;
    if (res.SaveToMemory(out, size)) {
        // ... envoyer 'out' / 'size' sur le réseau ...
        nkentseu::memory::NkFree(out);   // OWNERSHIP : NkFree, jamais std::free !
    }

    // 4. Libérer la copie RAM sans détruire l'objet.
    res.Unload();                     // après quoi IsValid() == false
}
```

---

[← Index NKStream](README.md) · [Récap NKStream](../NKStream.md) · [Couche System](../README.md)
