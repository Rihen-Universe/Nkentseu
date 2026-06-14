# NKFileSystem — documentation détaillée

Le module **NKFileSystem**, partie par partie. Pour une vue d'ensemble et un guide « par où
commencer », voir le récap : [../NKFileSystem.md](../NKFileSystem.md).

Chaque page décrit l'**API publique réelle** : namespace, headers, signatures exactes,
comportements de bord et pièges concrets (gestion d'erreur silencieuse, mutant vs
non-mutant, threading du watcher, assets Android…).

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [Paths.md](Paths.md) | Manipulation de chemins portable : jointure (`Append`/`operator/`), décomposition, normalisation `'\\'`→`'/'`, absolu/relatif, conversion native. | `NkPath.h` |
| [Files-Directories.md](Files-Directories.md) | Lecture/écriture de fichiers en RAII, modes bitmask, opérations sur dossiers (création, énumération glob, copie), façade système (volumes, espace, attributs, timestamps, liens), assets Android. | `NkFile.h`, `NkDirectory.h`, `NkFileSystem.h` |
| [Watcher.md](Watcher.md) | Surveillance asynchrone cross-platform de fichiers/dossiers (hot-reload) : `NkFileWatcher`, callback threadé, adaptateur lambda `NkSimpleFileWatcher`. | `NkFileWatcher.h` |

[← Récap NKFileSystem](../NKFileSystem.md) · [← Couche System](../README.md)
