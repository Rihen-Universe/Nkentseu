# NKStream

> Couche **System** · Les flux de données du moteur : interface de flux abstraite, flux
> binaire en mémoire, flux fichier, flux console — plus l'interface ressource `NKIResource`.

Dès qu'une donnée doit **circuler** — être lue, écrite, déplacée d'un fichier vers la RAM,
affichée sur la console ou décodée depuis un buffer — elle passe par NKStream. C'est la
plomberie d'entrée/sortie commune au moteur : les codecs lisent leurs octets via un flux, les
ressources média (image, police, son) se chargent et se sauvegardent à travers le contrat
`NKIResource`, et les outils écrivent sur la console en UTF-8 ou UTF-16.

Toute la hiérarchie repose sur une même paire de primitives brutes (`ReadRaw`/`WriteRaw`)
exposée par la classe de base `NkStream` ; les flux concrets (mémoire, fichier, console) ne
font que les implémenter. Au-dessus, `NKIResource` est une interface indépendante : un contrat
CPU pour charger/sauver des médias depuis un fichier, un buffer mémoire ou un flux.

- **Namespace** : `nkentseu`
- **Header parapluie** : `#include "NKStream/NkStream.h"`

---

## Par où commencer

Selon ce que vous cherchez à faire :

| Besoin | Partie |
|--------|--------|
| Lire / écrire des octets et des éléments typés sur un flux | [Les flux](NKStream/Streams.md) |
| Travailler sur un tampon mémoire en RAM (auto-extensible) | [Les flux](NKStream/Streams.md) |
| Ouvrir, lire, écrire, positionner un fichier | [Les flux](NKStream/Streams.md) |
| Écrire sur stdout/stderr ou lire stdin (UTF-8 / UTF-16) | [Les flux](NKStream/Streams.md) |
| Charger ou sauver une ressource média (image, police, son) | [La ressource](NKStream/Resource.md) |

Chaque page suit la même structure : un **tutoriel** narratif, un **aperçu** tabulaire de
toute l'API, puis une **référence-cours** où chaque élément est expliqué avec son contrat, sa
complexité et ses pièges réels.

---

## Aperçu des familles

- **Interface de flux** (`NkStream.h`) — `NkStream`, la base abstraite polymorphe. Définit le
  contrat `Open`/`Close`/`IsOpen`, les primitives brutes `ReadRaw`/`WriteRaw`, les templates
  typés `Read<T>`/`Write<T>` (troncature au multiple de `sizeof(T)`), le positionnement absolu
  `Seek`/`Tell`/`Size`/`IsEOF`, et l'encodage (`Encoding`, `SetEncoding`/`GetEncoding`). Les
  modes d'ouverture (`OpenMode`) sont des flags combinables.
- **Flux binaire** (`NkBinaryStream.h`) — `NkBinaryStream`, flux sur un tampon mémoire RAM,
  soit vue non-propriétaire sur un buffer externe, soit tampon alloué et auto-extensible
  (croissance ×2). `Resize`/`Reserve`/`Data()`.
- **Flux fichier** (`NkFileStream.h`) — `NkFileStream`, bâti au-dessus de `NkFile`
  (NKFileSystem) : path handling, fallback `AAssetManager` Android, gestion du BOM dans
  `SetEncoding`. RAII (ferme au destructeur).
- **Flux console** (`NkConsoleStream.h`) — `NkConsoleStream`, lecture/écriture sur les handles
  standards (stdin/stdout/stderr) avec encodage UTF-8/UTF-16 (`StreamType`).
- **Ressource** (`NKIResource.h`) — `NKIResource`, interface CPU pure de chargement/sauvegarde
  d'un média via fichier / mémoire / flux. Implémentée par NkImage, NkFont, NkAudioSample.

---

## Index des 5 headers

| Header | Contenu | Documenté dans |
|--------|---------|----------------|
| `NkStream.h` | `NkStream` (base abstraite), enum `Encoding`, `OpenMode`. | [Les flux](NKStream/Streams.md) |
| `NkBinaryStream.h` | `NkBinaryStream` (flux mémoire). | [Les flux](NKStream/Streams.md) |
| `NkFileStream.h` | `NkFileStream` (flux fichier via `NkFile`). | [Les flux](NKStream/Streams.md) |
| `NkConsoleStream.h` | `NkConsoleStream` (flux console, `StreamType`). | [Les flux](NKStream/Streams.md) |
| `NKIResource.h` | `NKIResource` (interface ressource CPU). | [La ressource](NKStream/Resource.md) |

---

[← Couche System](README.md) · [Index du wiki](../README.md)
