# Fichiers et répertoires

> Couche **System** · NKFileSystem · Lire et écrire des fichiers (`NkFile`), parcourir et gérer
> des répertoires (`NkDirectory`), interroger les volumes, attributs et liens du système de
> fichiers (`NkFileSystem`).

Tôt ou tard, tout moteur doit **toucher le disque** : charger une texture, sauvegarder une scène,
lister les niveaux d'un dossier, écrire un journal, repérer combien d'espace reste sur le volume.
La tentation serait d'appeler `fopen`/`fread` partout — mais ces API C sont fragiles (on oublie un
`fclose`, on confond `"r"` et `"rb"`, on ne sait pas si le fichier existe avant de l'ouvrir), et
elles ne disent rien des **répertoires**, des **volumes** ou des **attributs**. NKFileSystem
remplace ce patchwork par trois outils nets : un objet **fichier RAII** qui se ferme tout seul, une
collection d'**utilitaires de répertoires** entièrement statiques, et une façade de **système de
fichiers** pour tout ce qui dépasse le simple fichier (lecteurs, espace disque, dates, liens).

Une règle traverse tout le module et il faut l'avoir en tête dès le départ : **rien ne lève
d'exception**. En cas d'échec — fichier absent, droits refusés, chemin invalide — on récupère une
valeur neutre : `false`, `0`, `-1`, une chaîne vide ou un vecteur vide. C'est à vous de tester. Ce
n'est **pas** un système qui « plante bruyamment » ; c'est un système qui « échoue discrètement »,
et donc qui exige de la discipline côté appelant.

- **Namespace** : `nkentseu` (un namespace de compatibilité `nkentseu::entseu` ré-exporte certains
  symboles de `NkFile.h` et `NkDirectory.h`)
- **Headers** : `#include "NKFileSystem/NkFile.h"`, `#include "NKFileSystem/NkDirectory.h"`,
  `#include "NKFileSystem/NkFileSystem.h"`

---

## Le fichier RAII : `NkFile`

Le cœur du module est `NkFile`, un objet qui **possède** un fichier ouvert et le **referme tout
seul** quand il sort du scope. C'est l'inverse du couple `fopen`/`fclose` où la fermeture est à
votre charge : ici, dès que l'objet est détruit, le handle est rendu. On ne peut **pas** copier un
`NkFile` (la copie est interdite, `= delete`) — un fichier ouvert est une ressource unique — mais on
peut le **déplacer** (`move`), ce qui transfère le handle et laisse la source vide-mais-valide.

```cpp
NkFile file("config.ini", NkFileMode::NK_READ);
if (!file.IsOpen()) {            // pas d'exception : on VÉRIFIE
    NK_LOG_WARN("config absente");
    return;
}
NkString contenu = file.ReadAll();
// pas de Close() : le destructeur s'en charge à la fin du scope
```

Le piège du débutant est d'oublier le `IsOpen()` : le constructeur tente l'ouverture mais ne
signale **jamais** un échec autrement que par `IsOpen() == false`. Construire un `NkFile` sur un
chemin inexistant ne plante pas — il faut le demander.

Le **mode** d'ouverture est un `NkFileMode`, un jeu de drapeaux bit à bit calqué sur les modes de
`fopen` : `NK_READ`, `NK_WRITE`, `NK_APPEND`, `NK_TRUNCATE`, `NK_BINARY` se combinent avec
`operator|`, et des combinaisons toutes prêtes (`NK_READ_BINARY`, `NK_WRITE_BINARY`,
`NK_READ_WRITE`…) couvrent les cas courants. **Attention** : pour tout ce qui n'est pas du texte
pur (images, audio, `.nkb` binaires), pensez à `NK_BINARY` — sinon, sous Windows, les `\r\n` seront
traduits et vos octets corrompus.

> **En résumé.** `NkFile` est un **handle RAII** : il s'ouvre à la construction (vérifiez
> `IsOpen()`, pas d'exception), se ferme au destructeur, ne se copie pas mais se déplace. Le mode est
> un masque de bits (`NkFileMode`) ; ajoutez `NK_BINARY` pour les fichiers non-texte.

### Lire et écrire

Une fois ouvert, `NkFile` offre **deux niveaux de lecture**. Le niveau brut, `Read(buffer, size)`,
remplit votre tampon d'au plus `size` octets et retourne le nombre réellement lu — c'est ce qu'on
utilise pour les formats binaires, par blocs. Le niveau confort donne le fichier **en une fois** :
`ReadAll()` renvoie tout le contenu dans une `NkString`, `ReadLine()` une ligne sans son saut (il
gère `\r\n` *et* `\n`), `ReadLines()` toutes les lignes d'un coup dans un `NkVector<NkString>`.

```cpp
// Lecture ligne à ligne (parsing d'un .obj, d'un .csv, d'un log)
NkFile f("model.obj", NkFileMode::NK_READ);
while (!f.IsEOF()) {
    NkString ligne = f.ReadLine();
    // ... traiter la ligne
}
```

Symétriquement, l'écriture a `Write(data, size)` (octets bruts), `Write(const NkString&)` (toute la
chaîne, renvoie `true` si tout est passé) et `WriteLine(text)` qui **ajoute un `\n`**
automatiquement. Le `Flush()` force l'écriture sur disque sans attendre la fermeture — utile pour un
journal qu'on veut voir apparaître tout de suite.

Ce n'est **pas** un flux bufferisé à la C++ (`std::fstream`) avec `operator<<` : on travaille en
appels explicites, et le retour (nombre d'octets / booléen) est toujours à vérifier.

> **En résumé.** Lecture brute (`Read`) pour le binaire par blocs ; lecture confort (`ReadAll`,
> `ReadLine`, `ReadLines`) pour le texte d'un coup. Écriture par `Write`/`WriteLine` (qui ajoute le
> `\n`). `Flush()` pour matérialiser tout de suite. Toujours tester la valeur de retour.

### Naviguer dans le fichier

Pour lire au milieu d'un fichier (un format avec table d'offsets, un cache binaire), on déplace le
**curseur**. `Tell()` donne la position courante en octets, `Seek(offset, origin)` la change —
`origin` étant `NK_BEGIN` (depuis le début), `NK_CURRENT` (relatif) ou `NK_END` (depuis la fin) — et
`SeekToBegin()`/`SeekToEnd()` sont les deux raccourcis usuels. `GetSize()` (alias `Size()`) donne la
taille du fichier.

Un **piège réel** ici : `GetSize()` fonctionne en faisant un seek temporaire jusqu'à la fin, et
peut donc **déplacer le curseur**. Si vous avez besoin de la taille *et* de continuer à lire d'où
vous étiez, sauvegardez la position avant.

> **En résumé.** `Seek`/`Tell` + origines `NK_BEGIN/CURRENT/END` pour se déplacer dans le fichier ;
> `SeekToBegin/End` en raccourci. Méfiez-vous : `GetSize()`/`Size()` peut bouger le curseur.

### Les utilitaires statiques : sans même ouvrir

Souvent on veut juste *savoir* ou *agir d'un coup* sans gérer un objet ouvert. `NkFile` expose une
batterie de **fonctions statiques** pour ça : `Exists`, `Delete`, `Copy`, `Move`, `GetFileSize`, et
surtout les quatre raccourcis « tout-en-un » : `ReadAllText`/`ReadAllBytes` pour aspirer un fichier
entier, `WriteAllText`/`WriteAllBytes` pour en écrire un d'un trait (en tronquant l'existant).

```cpp
if (NkFile::Exists("save.nkscene"))
    NkString json = NkFile::ReadAllText("save.nkscene");

NkFile::WriteAllBytes("blob.nkb", octets);     // octets = NkVector<nk_uint8>
NkFile::Copy("a.png", "b.png", /*overwrite*/ true);
```

Chaque utilitaire a deux surcharges, l'une prenant un `const char*`, l'autre un `const NkPath&` —
utilisez celle qui correspond à ce que vous tenez déjà en main, sans conversion manuelle.

> **En résumé.** Pour des opérations ponctuelles, préférez les **statiques** (`Exists`, `Copy`,
> `ReadAllText`, `WriteAllBytes`…) à l'ouverture d'un objet. Toutes existent en `const char*` et en
> `const NkPath&`.

### Le cas Android : les assets

Sur Android, les fichiers livrés dans l'APK ne sont **pas** accessibles par `fopen` : il faut passer
par l'`AAssetManager`. `NkFile` gère ça en transparence : si une ouverture par `fopen` échoue, il
**bascule** automatiquement sur l'AAssetManager (en lecture seule). Il suffit d'enregistrer le
gestionnaire au démarrage via `SetAndroidAssetManager`, et éventuellement un sous-dossier à
str">retirer du préfixe via `SetAndroidAssetSubFolder`. Sur les autres plateformes, ces appels sont des
no-op.

> **En résumé.** Sur Android, enregistrez l'`AAssetManager` une fois (`SetAndroidAssetManager`) et
> `NkFile` lira vos assets de l'APK en repli automatique. Ailleurs : sans effet.

---

## Les répertoires : `NkDirectory`

`NkDirectory` n'est **pas** une classe qu'on instancie : c'est un sac d'**utilitaires statiques**
pour tout ce qui touche aux dossiers. Créer, supprimer, tester l'existence, lister le contenu,
copier une arborescence, trouver les répertoires spéciaux du système.

La création vient en deux saveurs : `Create(path)` crée **un** dossier (et échoue si son parent
n'existe pas), tandis que `CreateRecursive(path)` crée toute la **chaîne** de parents manquants —
c'est presque toujours celle qu'on veut pour fabriquer une arborescence de sauvegarde. La
suppression `Delete(path, recursive)` refuse par défaut d'effacer un dossier non vide ; il faut
passer `recursive = true` pour qu'elle emporte tout le contenu.

Un **piège à connaître** : `Empty(path)` retourne `true` aussi bien pour un dossier réellement vide
que pour un dossier **inexistant**. « Vide » et « absent » ne se distinguent pas par cette seule
fonction — combinez avec `Exists` si la nuance compte.

> **En résumé.** `NkDirectory` est 100 % statique. `CreateRecursive` (et non `Create`) pour bâtir
> une arborescence ; `Delete(path, true)` pour supprimer un dossier plein. Souvenez-vous : `Empty`
> renvoie `true` aussi pour un dossier inexistant.

### Lister le contenu

C'est l'usage quotidien : énumérer les fichiers d'un dossier. Trois méthodes, selon ce qu'on veut
récupérer — `GetFiles` (les fichiers), `GetDirectories` (les sous-dossiers), `GetEntries` (les deux,
sous forme de structures `NkDirectoryEntry` riches en métadonnées). Chacune accepte un **motif
glob** (`pattern`, par défaut `"*"`, avec `*` = séquence et `?` = un caractère) et une **option de
profondeur** (`option`) : `NK_TOP_DIRECTORY_ONLY` (le dossier seul) ou `NK_ALL_DIRECTORIES`
(récursif).

```cpp
// Tous les .png du dossier d'assets, récursivement
NkVector<NkString> textures =
    NkDirectory::GetFiles("assets", "*.png", NkSearchOption::NK_ALL_DIRECTORIES);

// Métadonnées de chaque entrée du dossier courant
for (const NkDirectoryEntry& e : NkDirectory::GetEntries(".")) {
    if (e.IsDirectory) NK_LOG_INFO("[dir]  {}", e.Name);
    else               NK_LOG_INFO("{} ({} o)", e.Name, e.Size);
}
```

Les chemins renvoyés par `GetFiles`/`GetDirectories` sont **complets et normalisés avec `/`**. Côté
`NkDirectoryEntry`, les champs `Size` et `ModificationTime` peuvent valoir `0` sur certaines
plateformes — ne construisez pas de logique critique dessus sans vérifier.

> **En résumé.** `GetFiles`/`GetDirectories`/`GetEntries` listent avec un **motif glob** et une
> profondeur (`NK_TOP_DIRECTORY_ONLY` vs `NK_ALL_DIRECTORIES`). Chemins complets normalisés `/` ;
> `Size`/`ModificationTime` parfois `0` selon l'OS.

### Les répertoires spéciaux

Plutôt que de coder en dur `C:\Users\...` ou `/home/...`, demandez au système. `NkDirectory` donne
le répertoire **courant** (`GetCurrentDirectory`/`SetCurrentDirectory`), le **temporaire**
(`GetTempDirectory`), le **home** de l'utilisateur (`GetHomeDirectory`) et le dossier de **données
applicatives** (`GetAppDataDirectory` — `%APPDATA%` sous Windows, `~/.config` sous Unix). C'est là
qu'un jeu range ses sauvegardes, ses configs et ses caches de façon portable.

> **En résumé.** `GetCurrentDirectory`, `GetTempDirectory`, `GetHomeDirectory`,
> `GetAppDataDirectory` localisent les dossiers standards de chaque OS — utilisez-les pour ranger
> sauvegardes et configs de façon portable.

---

## Le système de fichiers : `NkFileSystem`

`NkFileSystem` est la troisième façade, également **100 % statique**, pour tout ce qui dépasse le
fichier ou le dossier individuel : les **volumes** (lecteurs, espace libre), les **attributs**
(lecture seule, caché), les **timestamps**, la **validation de chemins** et les **liens
symboliques**. C'est la couche qu'on sollicite dans un éditeur (afficher l'espace disponible),
dans un installeur, ou dans un outil de gestion d'assets.

On y interroge les lecteurs avec `GetDrives()` (sous Windows, énumère A–Z ; sous Unix, renvoie
uniquement `/`) et `GetDriveInfo(path)`, qui remplit une `NkDriveInfo` (nom, label, type de FS,
taille totale, espace libre/disponible, et un drapeau **`IsReady` à tester avant** de se fier aux
tailles). L'espace disque s'obtient aussi directement par `GetTotalSpace`/`GetFreeSpace`/
`GetAvailableSpace` — où « free » inclut le réservé système et « available » l'exclut (ce qui
**reste réellement à l'utilisateur**).

```cpp
NkDriveInfo d = NkFileSystem::GetDriveInfo("C:/");
if (d.IsReady) {
    nk_int64 dispo = NkFileSystem::GetAvailableSpace("C:/");  // octets utilisables
    NK_LOG_INFO("{} libres sur {}", dispo, d.TotalSize);
}
```

> **En résumé.** `NkFileSystem` couvre volumes, espace, attributs, dates et liens. Pour l'espace,
> `GetAvailableSpace` (ce qui reste à l'utilisateur) ≠ `GetFreeSpace` (inclut le réservé système) ;
> vérifiez `NkDriveInfo::IsReady` avant d'utiliser les tailles.

### Attributs, dates et chemins

Les **attributs** d'un fichier (`NkFileAttributes` : lecture seule, caché, système, archive,
compressé, chiffré) se lisent par `GetFileAttributes` et se posent par `SetFileAttributes`, avec des
raccourcis `SetReadOnly`/`SetHidden`. Plusieurs de ces drapeaux (caché, système, chiffré) n'ont de
sens que **sous Windows** ; `SetHidden` est par exemple toujours `false` sous Unix. Les
**timestamps** suivent la même logique : getters `GetCreationTime`/`GetLastAccessTime`/
`GetLastWriteTime` (epoch Unix, `0` si erreur), setters disponibles mais **Windows seulement** côté
écriture.

Côté **chemins**, `IsValidFileName` rejette les caractères interdits (`< > " | ? *` et les
contrôles), `IsValidPath` délègue au type `NkPath`, et `GetAbsolutePath` résout un chemin complet.
**Deux pièges documentés** : `GetRelativePath` est une implémentation **basique qui retourne sa
cible telle quelle** (ce n'est pas un vrai calcul relatif), et `GetSymbolicLinkTarget` **renvoie une
chaîne vide sous Windows** (non implémenté). Ne vous appuyez pas dessus pour de la logique critique.

> **En résumé.** Attributs et dates lisibles partout, mais plusieurs drapeaux et tous les *setters*
> de date sont **Windows seulement**. Validez avec `IsValidFileName`/`IsValidPath`. Attention :
> `GetRelativePath` et `GetSymbolicLinkTarget` (Windows) sont des stubs.

---

## Aperçu de l'API

Tous les éléments publics, regroupés par fichier. `[noexcept]` signalé là où c'est le cas
(uniquement les move-ops de `NkFile`) ; toutes les opérations « chemin » existent en `const char*`
et `const NkPath&` sauf mention contraire.

### `NkFile.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Mode | `enum NkFileMode` : `NK_NONE`, `NK_READ`, `NK_WRITE`, `NK_APPEND`, `NK_TRUNCATE`, `NK_BINARY` + combinaisons (`NK_READ_BINARY`, `NK_WRITE_BINARY`, `NK_READ_WRITE`…) | Masque de bits façon `fopen`. |
| Mode | `operator\|`, `operator&`, `NkHasFlag(value, flag)` `[O(1)]`, alias `NkFileModeFlags` | Combiner/tester les drapeaux. |
| Mode | `enum NkSeekOrigin` : `NK_BEGIN`, `NK_CURRENT`, `NK_END` | Origine d'un `Seek`. |
| Cycle de vie | `NkFile()`, `NkFile(const char*, mode)`, `NkFile(const NkPath&, mode)`, `~NkFile()` | Construit (ouvre) / ferme automatiquement. |
| Cycle de vie | copie `= delete`, `NkFile(NkFile&&)` `[noexcept]`, `operator=(NkFile&&)` `[noexcept]` | Non copiable, déplaçable. |
| Ouverture | `Open(path, mode)`, `Close()`, `IsOpen()` | Ouvrir / fermer (idempotent) / état. |
| Lecture | `Read(buffer, size)`, `ReadLine()`, `ReadAll()`, `ReadLines()` | Brut / ligne / tout / toutes les lignes. |
| Écriture | `Write(data, size)`, `Write(const NkString&)`, `WriteLine(text)` | Brut / chaîne / ligne (+`\n`). |
| Curseur | `Tell()`, `Seek(offset, origin)`, `SeekToBegin()`, `SeekToEnd()`, `GetSize()`, `Size()` | Position / déplacement / taille (peut bouger le curseur). |
| Tampon | `Flush()` | Forcer l'écriture disque. |
| Propriétés | `GetPath()`, `GetMode()`, `IsEOF()` | Chemin / mode / fin de fichier. |
| Statiques | `Exists`, `Delete`, `Copy(src, dst, overwrite)`, `Move`, `GetFileSize` | Opérations sans ouvrir. |
| Statiques | `ReadAllText`, `ReadAllBytes`, `WriteAllText`, `WriteAllBytes` | Lecture/écriture « tout-en-un ». |
| Android | `SetAndroidAssetManager`, `GetAndroidAssetManager`, `SetAndroidAssetSubFolder` | Repli AAssetManager (read-only). |

### `NkDirectory.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Entrée | `struct NkDirectoryEntry` : `Name`, `FullPath`, `IsDirectory`, `IsFile`, `Size`, `ModificationTime` | Métadonnées d'un élément. |
| Profondeur | `enum NkSearchOption` : `NK_TOP_DIRECTORY_ONLY`, `NK_ALL_DIRECTORIES` | Plat ou récursif. |
| Base | `Create`, `CreateRecursive`, `Delete(path, recursive)`, `Exists`, `Empty` | Créer / supprimer / tester (`Empty` = `true` si absent). |
| Énumération | `GetFiles(path, pattern, option)`, `GetDirectories(...)`, `GetEntries(...)` | Lister fichiers / dossiers / entrées (glob `*` `?`). |
| Copie | `Copy(src, dst, recursive, overwrite)`, `Move` | Copier/déplacer une arborescence. |
| Spéciaux | `GetCurrentDirectory`, `SetCurrentDirectory`, `GetTempDirectory`, `GetHomeDirectory`, `GetAppDataDirectory` | Dossiers standards de l'OS (renvoient `NkPath`). |

### `NkFileSystem.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Types | `enum NkFileSystemType` : `NK_UNKNOW`, `NK_NTFS`, `NK_FAT32`, `NK_EXFAT`, `NK_EXT4`, `NK_EXT3`, `NK_HFS`, `NK_APFS`, `NK_NETWORK` | Type de FS. |
| Structs | `NkDriveInfo` (`Name`, `Label`, `Type`, `TotalSize`, `FreeSpace`, `AvailableSpace`, `IsReady`) | Info volume (tester `IsReady`). |
| Structs | `NkFileAttributes` (`IsReadOnly`, `IsHidden`, `IsSystem`, `IsArchive`, `IsCompressed`, `IsEncrypted`, `CreationTime`, `LastAccessTime`, `LastWriteTime`) | Attributs + dates. |
| Volumes | `GetDrives()`, `GetDriveInfo(path)` | Lister / interroger les lecteurs. |
| Espace | `GetTotalSpace`, `GetFreeSpace`, `GetAvailableSpace` | Octets (free inclut réservé, available non). |
| Attributs | `GetFileAttributes`, `SetFileAttributes`, `SetReadOnly`, `SetHidden` | Lire/poser (caché = Windows). |
| Dates | `GetCreationTime`, `GetLastAccessTime`, `GetLastWriteTime`, `SetCreationTime`, `SetLastAccessTime`, `SetLastWriteTime` | Epoch Unix (setters = Windows). |
| Chemins | `IsValidFileName` *(C-string)*, `IsValidPath` *(C-string)*, `GetAbsolutePath`, `GetRelativePath` | Valider / convertir (`GetRelativePath` = stub). |
| FS | `GetFileSystemType`, `IsCaseSensitive` | Type / sensibilité à la casse. |
| Liens | `IsSymbolicLink`, `GetSymbolicLinkTarget`, `CreateSymbolicLink` | Liens (`GetSymbolicLinkTarget` vide sous Windows). |

---

## Référence complète

Chaque élément est repris en détail. Les opérations triviales (ouverture, état) le sont brièvement ;
les opérations structurantes (modes, lecture/écriture, énumération, espace disque) le sont à fond,
avec leurs usages dans les différents domaines du moteur.

### `NkFileMode` et les drapeaux — comment ouvrir

`NkFileMode` est un `enum class : uint32` de **drapeaux combinables**. Les atomiques (`NK_READ`,
`NK_WRITE`, `NK_APPEND`, `NK_TRUNCATE`, `NK_BINARY`) se composent par `operator|`, et `NkHasFlag`
teste un drapeau en `O(1)`. Des combinaisons prêtes (`NK_READ_BINARY` = "rb", `NK_WRITE_BINARY` =
"wb", `NK_READ_WRITE` = "r+", etc.) couvrent les usages classiques.

Le **piège documenté** : `NK_READ_WRITE` et `NK_WRITE_READ` portent **les mêmes bits** (0 et 1) et
ne sont donc pas distinguables — la sémantique « truncate » de "w+" ne tient pas au seul masque. Si
vous voulez vraiment tronquer en ouvrant en lecture/écriture, ajoutez `NK_TRUNCATE` explicitement.

- **IO / réseau** : `NK_WRITE_BINARY` pour sérialiser un `.nkb` ou un paquet ; `NK_APPEND_BINARY`
  pour un journal qu'on ne réécrit jamais.
- **Rendu / GPU** : `NK_READ_BINARY` pour charger une texture, un SPIR-V compilé, un cache de
  pipeline — toujours `NK_BINARY`, jamais le mode texte.
- **Outils / éditeur** : `NK_READ_WRITE` pour rouvrir une scène et la patcher en place.

### `NkSeekOrigin` — d'où compter

Trivial : `NK_BEGIN`/`NK_CURRENT`/`NK_END` correspondent à `SEEK_SET`/`SEEK_CUR`/`SEEK_END`. C'est
l'argument d'`Seek`, qui détermine si l'offset est absolu, relatif à la position courante, ou
mesuré depuis la fin (un offset négatif depuis `NK_END` recule depuis la fin).

### `NkFile` — le cycle de vie

Le constructeur (`const char*` ou `const NkPath&`, mode par défaut `NK_READ`) **ouvre** le fichier
immédiatement ; le destructeur le **ferme**. La copie est interdite (`= delete`) parce qu'un handle
de fichier est une ressource unique qu'on ne saurait dupliquer ; le **déplacement** (`noexcept`)
transfère ce handle et vide la source — c'est ce qui permet de stocker un `NkFile` dans un
`NkVector`, de le renvoyer d'une fonction, ou de l'échanger.

- **Outils / éditeur** : ouvrir une poignée de fichiers, les ranger par déplacement dans un
  conteneur, et laisser le scope tout refermer en sortie.
- **Threading** : un `NkFile` n'est pas partageable entre threads sans synchronisation — donnez à
  chaque thread le sien (par déplacement), n'en partagez jamais un seul par copie (impossible de
  toute façon).

`Open(path, mode)` referme le fichier précédent s'il y en avait un puis ouvre le nouveau ; `Close()`
est **idempotent** (sans effet si déjà fermé) ; `IsOpen()` est le seul moyen de savoir si tout va
bien (rappel : aucune exception).

### Lecture — `Read`, `ReadLine`, `ReadAll`, `ReadLines`

`Read(buffer, size)` est le primitif binaire : il lit au plus `size` octets dans votre tampon et
retourne le nombre **réellement lu** (`0` si erreur ou fin). C'est l'outil des formats à structure
fixe, où l'on lit en-tête puis blocs.

- **Rendu / GPU** : charger un fichier de texture ou un blob de shader compilé par blocs, en
  vérifiant le nombre d'octets lus à chaque appel.
- **Audio** : streamer un fichier son par paquets dans un tampon plutôt que de tout charger.
- **IO / réseau** : lire un format binaire maison (`.nkb`) champ par champ.

Les confort lisent **tout d'un coup** : `ReadAll()` rend tout le contenu en `NkString` (il alloue
`GetSize()` octets), `ReadLine()` une ligne sans son saut (gère `\r\n` et `\n`, vide à EOF),
`ReadLines()` toutes les lignes dans un `NkVector<NkString>`.

- **Outils** : parser un `.obj`, un `.csv`, un fichier de config en parcourant `ReadLines()`.
- **Gameplay / IA** : charger une table de dialogues ou un script texte ligne à ligne.
- **UI / 2D** : relire un fichier de localisation, un thème, une feuille de style maison.

### Écriture — `Write`, `WriteLine`, `Flush`

`Write(data, size)` écrit des octets bruts et retourne combien sont passés ; la surcharge
`Write(const NkString&)` écrit toute la chaîne et renvoie `true` si elle l'a fait entièrement ;
`WriteLine(text)` ajoute un `\n` derrière. `Flush()` pousse le tampon vers le disque sans attendre la
fermeture.

- **IO / réseau** : sérialiser une scène, un état réseau, un paquet (`Write` binaire).
- **Outils / éditeur** : exporter un rapport, un fichier de log lisible (`WriteLine`), avec `Flush()`
  pour que la ligne apparaisse immédiatement même si l'appli plante ensuite.
- **Threading** : un logger asynchrone qui `Flush()` après chaque rafale de messages.

### Curseur et taille — `Seek`, `Tell`, `GetSize`

`Tell()` rend la position en octets (`-1` si erreur), `Seek(offset, origin)` la change,
`SeekToBegin()`/`SeekToEnd()` sont les deux raccourcis. `GetSize()` (et son alias inline `Size()`)
renvoie la taille du fichier en octets — **mais** il l'obtient par un seek temporaire et peut donc
**laisser le curseur ailleurs** : sauvegardez la position si vous lisez ensuite.

- **IO** : un format avec table d'offsets en en-tête — on lit la table, puis on `Seek` vers chaque
  ressource.
- **Audio** : se repositionner dans un fichier pour reprendre un streaming, ou implémenter le seek
  d'un lecteur.

`IsEOF()` indique la fin de fichier, avec une nuance : à cause du buffering, il peut renvoyer `false`
alors qu'un `Read()` rendrait déjà `0`. Pour boucler sûrement, testez plutôt le **retour** de
`Read`/`ReadLine` que `IsEOF()` seul.

### Utilitaires statiques de `NkFile`

Tout un pan de l'API évite d'instancier un objet. `Exists` teste la présence d'un fichier régulier ;
`Delete` le supprime (échoue s'il est ouvert/utilisé) ; `Copy(src, dst, overwrite)` recopie via un
tampon interne de 8 Ko ; `Move(src, dst)` déplace (peut échouer en **cross-volume**, le système ne
sachant pas toujours déplacer entre disques). `GetFileSize` rend la taille sans ouvrir.

Les quatre « tout-en-un » sont les plus utilisées au quotidien : `ReadAllText` (chaîne vide si
erreur), `ReadAllBytes` (`NkVector<nk_uint8>` vide si erreur), `WriteAllText` (tronque l'existant),
`WriteAllBytes`. Les octets transitent par `NkVector<nk_uint8>`, donc par **NKContainers / NKMemory**
— pas par le heap CRT, ce qui évite les corruptions de tas.

- **Rendu / GPU** : `ReadAllBytes("shader.spv")` puis envoi direct au pipeline.
- **ECS / scène** : `ReadAllText("level.nkscene")` → désérialisation JSON.
- **Outils** : `Copy`/`Move` pour des opérations d'export ou de packaging d'assets.

### Assets Android

`SetAndroidAssetManager(void* manager)` enregistre l'`AAssetManager` global (non-owning ; `nullptr`
pour le retirer) ; le type est `void*` exprès, pour ne pas faire dépendre l'en-tête du NDK.
`GetAndroidAssetManager()` le relit. `SetAndroidAssetSubFolder(name)` indique un sous-dossier
additionnel à retirer du préfixe (en plus de "Resources/") lors de la résolution dans l'APK. La
stratégie : si `Open()` échoue via `fopen`, `NkFile` retente via l'AAssetManager en strippant le
préfixe. Sur les autres plateformes, tout cela est sans effet.

### `NkDirectoryEntry` et `NkSearchOption`

`NkDirectoryEntry` est la fiche d'un élément listé : `Name` (nom court), `FullPath` (chemin complet),
les booléens `IsDirectory`/`IsFile`, `Size` (`0` pour un dossier) et `ModificationTime` (epoch).
`NkSearchOption` choisit la profondeur d'une énumération : `NK_TOP_DIRECTORY_ONLY` (le dossier seul)
ou `NK_ALL_DIRECTORIES` (descente récursive). `Size`/`ModificationTime` peuvent rester à `0` selon
la plateforme — à ne pas tenir pour acquis.

### Opérations de répertoire — `Create`, `Delete`, `Exists`, `Empty`

`Create(path)` crée un seul dossier (retourne `true` s'il existe déjà, échoue si le parent manque) ;
`CreateRecursive(path)` crée toute la chaîne manquante et est **idempotent** — c'est celle qu'on
prend pour fabriquer `saves/profil1/auto/` d'un coup. `Delete(path, recursive)` refuse par défaut un
dossier non vide ; `recursive = true` emporte tout. `Exists` teste la présence. `Empty` retourne
`true` pour un dossier vide **ou inexistant** (piège).

- **Outils / éditeur** : préparer l'arborescence de sortie d'un export (`CreateRecursive`), nettoyer
  un dossier de cache (`Delete(path, true)`).
- **Gameplay** : créer le dossier de sauvegarde au premier lancement.

### Énumération — `GetFiles`, `GetDirectories`, `GetEntries`

C'est l'opération de répertoire la plus fréquente. Les trois méthodes partagent les mêmes
paramètres : un `pattern` glob (`*` = séquence quelconque, `?` = un caractère, défaut `"*"`) et une
`NkSearchOption` (défaut `NK_TOP_DIRECTORY_ONLY`). `GetFiles`/`GetDirectories` renvoient des
**chemins complets normalisés avec `/`** ; `GetEntries` renvoie des `NkDirectoryEntry` complets.

- **Outils / éditeur** : peupler un navigateur d'assets, lister les `*.nkscene` d'un projet, scanner
  récursivement `assets/` pour bâtir un index.
- **Rendu / GPU** : découvrir tous les shaders `*.nksl` à compiler, toutes les textures d'un atlas.
- **Audio** : énumérer les bancs de sons d'un dossier.
- **Gameplay** : lister les niveaux ou les profils de sauvegarde disponibles pour un menu.

### Copie/déplacement de répertoires

`Copy(src, dst, recursive, overwrite)` recopie une arborescence (crée la destination si absente,
récursif par défaut) ; `Move(src, dst)` la déplace (peut échouer **cross-volume**). Usages :
packaging d'un build, duplication d'un template de projet dans un éditeur, archivage d'un dossier de
sauvegarde.

### Répertoires spéciaux

`GetCurrentDirectory`/`SetCurrentDirectory` lisent et changent le dossier courant ;
`GetTempDirectory` donne le temporaire (Windows `GetTempPathA` ou "C:/Temp" ; Unix `$TMPDIR` ou
"/tmp") ; `GetHomeDirectory` le home (Windows CSIDL_PROFILE/HOMEDRIVE+HOMEPATH ; Unix `$HOME`/passwd)
; `GetAppDataDirectory` le dossier de données (Windows `%APPDATA%` ; Unix `~/.config`). Tous rendent
un `NkPath`. C'est là qu'on range, de façon portable, sauvegardes, configs et caches — au lieu de
chemins codés en dur.

### `NkFileSystemType`, `NkDriveInfo`, `NkFileAttributes`

`NkFileSystemType` énumère les systèmes de fichiers reconnus — noter `NK_UNKNOW` (faute conservée
pour compat, pas `NK_UNKNOWN`), renvoyé en cas d'erreur. `NkDriveInfo` décrit un volume : `Name`
(ex. "C:/" ou "/"), `Label`, `Type`, `TotalSize`, `FreeSpace` (inclut le réservé système),
`AvailableSpace` (ce qui reste à l'utilisateur), et `IsReady` — **à tester avant** de se fier aux
tailles. `NkFileAttributes` rassemble les drapeaux (lecture seule, caché, système, archive,
compressé, chiffré — caché/système/chiffré = **Windows seulement**) et trois timestamps epoch
(création, dernier accès, dernière écriture).

### Volumes et espace disque

`GetDrives()` énumère les lecteurs (Windows : A–Z ; Unix : seulement `/`), `GetDriveInfo(path)`
détaille un volume. L'espace s'obtient par `GetTotalSpace`/`GetFreeSpace`/`GetAvailableSpace`
(octets, `0` si erreur), avec la distinction clé : `GetFreeSpace` **inclut** le réservé système,
`GetAvailableSpace` ne compte que ce que l'utilisateur peut réellement écrire (hors quotas).

- **Outils / éditeur** : afficher l'espace disponible avant un export volumineux, prévenir avant
  saturation.
- **IO** : refuser une sauvegarde si l'espace disponible est insuffisant.

### Attributs et timestamps

`GetFileAttributes` lit (objet vide si erreur), `SetFileAttributes` pose, et `SetReadOnly`/
`SetHidden` sont des raccourcis — `SetReadOnly` correspond au bit `S_IWUSR` sous Unix, `SetHidden`
est toujours `false` hors Windows. Les dates se lisent par `GetCreationTime`/`GetLastAccessTime`/
`GetLastWriteTime` (epoch, `0` si erreur) ; les setters existent mais sont **Windows seulement** en
écriture.

- **Outils / éditeur** : afficher la date de dernière modification d'un asset, marquer un fichier
  en lecture seule, détecter un asset modifié (comparer `GetLastWriteTime` à un cache) pour
  recharger à chaud.
- **Build / packaging** : conserver les dates lors d'une copie d'archive (sur Windows).

### Validation et conversion de chemins

`IsValidFileName(name)` (C-string seulement) rejette `< > " | ? *` et les caractères de contrôle
(<32) ; `IsValidPath(path)` (C-string seulement) délègue à `NkPath::IsValidPath()` ;
`GetAbsolutePath` résout un chemin complet (chaîne vide si erreur). **Piège majeur** :
`GetRelativePath(from, to)` est une **implémentation basique qui retourne `to` tel quel** — ce n'est
pas un vrai calcul de chemin relatif, ne construisez pas de logique dessus.

- **Outils / éditeur** : valider le nom saisi dans une boîte de dialogue « Enregistrer sous »
  (`IsValidFileName`) avant d'écrire.

### Type de FS et casse

`GetFileSystemType(path)` rend le type (`NK_UNKNOW` si erreur) ; `IsCaseSensitive(path)` indique si
le FS distingue la casse (`true` sous Unix, `false` sous Windows par défaut). Utile pour un outil
multiplateforme qui doit décider si `Texture.png` et `texture.png` désignent le même fichier.

### Liens symboliques

`IsSymbolicLink(path)` teste si un chemin est un lien ; `GetSymbolicLinkTarget(path)` rend sa cible —
**mais renvoie une chaîne vide sous Windows** (non implémenté) ; `CreateSymbolicLink(link, target)`
crée un lien (Windows : nécessite des privilèges admin ou le mode développeur). Usage typique : un
éditeur qui partage un dossier d'assets commun entre plusieurs projets via des liens, en gardant à
l'esprit la limite Windows.

---

### Exemple récapitulatif

```cpp
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFileSystem.h"
using namespace nkentseu;

// 1. Préparer le dossier de sauvegarde de façon portable.
NkPath base = NkDirectory::GetAppDataDirectory();   // %APPDATA% / ~/.config
NkDirectory::CreateRecursive(base);                 // crée toute la chaîne

// 2. Lister les niveaux disponibles (récursif, motif glob).
NkVector<NkString> niveaux =
    NkDirectory::GetFiles("assets/levels", "*.nkscene",
                          NkSearchOption::NK_ALL_DIRECTORIES);

// 3. Charger un niveau en texte (statique « tout-en-un »).
if (!niveaux.IsEmpty()) {
    NkString json = NkFile::ReadAllText(niveaux[0]);   // vide si erreur
    // ... désérialiser json
}

// 4. Écrire une sauvegarde binaire via un objet RAII.
{
    NkFile save("save.nkb", NkFileMode::NK_WRITE_BINARY);
    if (save.IsOpen())                 // pas d'exception : on vérifie
        save.Write(blob);              // NkString ou octets
}                                      // fermeture automatique ici

// 5. Refuser la sauvegarde si l'espace manque.
nk_int64 dispo = NkFileSystem::GetAvailableSpace("C:/");
if (dispo < tailleEstimee)
    NK_LOG_WARN("espace disque insuffisant");
```

---

[← Index NKFileSystem](README.md) · [Récap NKFileSystem](../NKFileSystem.md) · [Couche System](../README.md)
