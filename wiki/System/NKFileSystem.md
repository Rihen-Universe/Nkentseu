# NKFileSystem

> Couche **System** · Le système de fichiers portable du moteur : chemins, lecture/écriture
> de fichiers, opérations sur les dossiers et surveillance des changements (hot-reload).

Dès qu'une chose doit être **chargée** ou **sauvegardée** — une scène, une texture, un
fichier de configuration, un log, un asset packagé dans un APK Android — elle passe par
NKFileSystem. C'est la frontière propre entre le moteur et le disque : on y manipule des
chemins sans se soucier du séparateur de la plateforme, on lit et écrit des fichiers en RAII,
on énumère et copie des dossiers, et on surveille en arrière-plan les fichiers qui changent
pour recharger à chaud. Aucune dépendance à `std::filesystem` (compatibilité embarquée et
zéro-STL), aucune exception : tout échec renvoie une valeur neutre (`false`, `-1`, chaîne ou
vecteur vide).

L'API tourne autour d'un type pivot, `NkPath`, qui normalise toujours en interne les chemins
avec le séparateur `'/'` (même sur Windows) et ne se reconvertit au format natif qu'à la
frontière des appels système. Les opérations lourdes (fichiers, dossiers, volumes) sont
exposées comme **utilitaires statiques** sur trois façades : `NkFile`, `NkDirectory`,
`NkFileSystem`.

- **Namespace** : `nkentseu` (alias de compatibilité legacy : `nkentseu::entseu`)
- **Header parapluie** : `#include "NKFileSystem/NkFileSystem.h"`

---

## Par où commencer

Selon ce que vous cherchez à faire :

| Besoin | Partie |
|--------|--------|
| Construire, joindre, décomposer un chemin (extension, dossier parent, normalisation) | [Les chemins](NKFileSystem/Paths.md) |
| Lire ou écrire un fichier (texte, binaire, ligne à ligne), RAII, assets Android | [Fichiers & dossiers](NKFileSystem/Files-Directories.md) |
| Créer, énumérer, copier, supprimer des dossiers ; répertoires spéciaux (home, temp, appdata) | [Fichiers & dossiers](NKFileSystem/Files-Directories.md) |
| Interroger volumes, espace disque, attributs, timestamps, liens symboliques | [Fichiers & dossiers](NKFileSystem/Files-Directories.md) |
| Surveiller des fichiers/dossiers et recharger à chaud quand ils changent | [La surveillance](NKFileSystem/Watcher.md) |

---

## Aperçu des familles

- **Chemins** (`NkPath.h`) — `NkPath`, wrapper de chaîne qui normalise systématiquement
  `'\\'`→`'/'`. Jointure (`Append`, `operator/`), décomposition (`GetDirectory`,
  `GetFileName`, `GetExtension`, `GetRoot`), prédicats (`IsAbsolute`, `HasExtension`),
  modification (`ReplaceExtension`, `RemoveFileName`, `GetParent`), conversion (`CStr`,
  `ToString`, `ToNative`), et utilitaires statiques (`GetCurrentDirectory`,
  `GetExecutableDirectory`, `GetTempDirectory`, `Combine`, `IsValidPath`).
- **Fichiers** (`NkFile.h`) — `NkFile` RAII (non copiable, movable) pour lire/écrire
  (`Read`/`Write`, `ReadLine`/`ReadAll`/`ReadLines`, `Seek`/`Tell`/`GetSize`), modes
  bitmask `NkFileMode`, plus des utilitaires statiques one-shot (`ReadAllText`,
  `WriteAllBytes`, `Exists`, `Copy`, `Move`, `Delete`) et le support des assets Android.
- **Dossiers** (`NkDirectory.h`) — `NkDirectory`, 100 % statique : création
  (`Create`/`CreateRecursive`), énumération avec glob (`GetFiles`/`GetDirectories`/
  `GetEntries`, option récursive), copie/déplacement, et répertoires spéciaux
  (`GetHomeDirectory`, `GetTempDirectory`, `GetAppDataDirectory`).
- **Façade système** (`NkFileSystem.h`) — `NkFileSystem`, 100 % statique : volumes
  (`GetDrives`/`GetDriveInfo`), espace disque, attributs (`NkFileAttributes`),
  timestamps, validation de chemins et liens symboliques.
- **Surveillance** (`NkFileWatcher.h`) — `NkFileWatcher` surveille de façon asynchrone
  (thread dédié, inotify/ReadDirectoryChangesW) ; notifie via `NkFileWatcherCallback`
  ou, plus simplement, via `NkSimpleFileWatcher` (lambda sans capture).

---

## Index des headers

| Header | Contenu | Documenté dans |
|--------|---------|----------------|
| `NkFileSystem.h` | Parapluie + façade `NkFileSystem` (volumes, espace, attributs, timestamps, liens). | [Fichiers & dossiers](NKFileSystem/Files-Directories.md) |
| `NkPath.h` | `NkPath` : manipulation de chemins portable. | [Chemins](NKFileSystem/Paths.md) |
| `NkFile.h` | `NkFile` (RAII), `NkFileMode`, `NkSeekOrigin`, utilitaires fichiers. | [Fichiers & dossiers](NKFileSystem/Files-Directories.md) |
| `NkDirectory.h` | `NkDirectory`, `NkDirectoryEntry`, `NkSearchOption`. | [Fichiers & dossiers](NKFileSystem/Files-Directories.md) |
| `NkFileWatcher.h` | `NkFileWatcher`, `NkFileWatcherCallback`, `NkSimpleFileWatcher`, événements. | [Surveillance](NKFileSystem/Watcher.md) |
| `NkFileSystemApi.h` | Macros d'export. | — |

---

[← Couche System](README.md) · [Index du wiki](../README.md)
