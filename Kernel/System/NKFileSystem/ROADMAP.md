# NKFileSystem — Roadmap

État actuel (mai 2026) : Module robuste et opérationnel. Couvre la
manipulation de chemins (`NkPath`, normalisation `/`), les opérations
fichiers RAII (`NkFile` avec modes flags), répertoires et énumération
(`NkDirectory` + `NkDirectoryEntry` avec récursion et glob), métadonnées
système (`NkFileSystem` + `NkDriveInfo` + `NkFileAttributes`) et surveillance
temps-réel (`NkFileWatcher` via inotify / ReadDirectoryChangesW). Pas de
dépendance à `std::filesystem`.

---

## Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| NkPath (normalisation, jointure, décomposition, extension, root, cwd) | Livré | — | — |
| NkFile (RAII, modes binaires/texte, ReadAllText/Bytes, WriteAll, Copy/Delete/Rename) | Livré | — | — |
| NkFileMode flags + opérateurs | Livré | — | — |
| NkDirectory (Create/Delete récursifs, Enumerate avec glob, NkSearchOption) | Livré | — | — |
| NkFileSystem (volumes, NkDriveInfo, NkFileAttributes, timestamps) | Livré | — | — |
| NkFileWatcher (inotify Linux / ReadDirectoryChangesW Windows / callback + functor) | Livré | — | — |
| Fallback Android AAssetManager dans NkFile | Livré | — | — |
| Tests smoke (NkPath, NkFile, NkDirectory) | Livré | — | — |
| Memory-mapped files (NkMmapFile) | TODO | M | Moyenne |
| Symlinks read/write cross-platform robuste | Partiel | S | Basse |
| AsyncIO (NkFile::ReadAsync, futures) | TODO | L | Basse |
| Tests étendus (FileWatcher events, NkFileSystem drives, attributes) | TODO | M | Haute |
| VFS / archive mount (.nkb, .zip mountés en répertoire virtuel) | TODO | XL | Moyenne |
| Glob avancé (**/, [a-z], extglob) | Partiel | M | Basse |
| iCloud / OneDrive / sandbox iOS validations | TODO | L | Basse |

Légende : Livré · Partiel · En cours · TODO · Abandonné

---

## Livré

### Manipulation de chemins
- [NkPath](src/NKFileSystem/NkPath.h) :
  - Stockage interne normalisé avec séparateur `/`, gestion des conversions
    Windows `\`.
  - `Append`, `Concat`, `Parent`, `Filename`, `Stem`, `Extension`,
    `HasExtension`, `HasFileName`, `IsAbsolute`, `IsRelative`, `Normalize`.
  - Statics `GetCurrentDirectory`, `SetCurrentDirectory` (avec annulation des
    macros Win32 conflictuelles).

### Opérations fichiers
- [NkFile](src/NKFileSystem/NkFile.h) :
  - RAII : fermeture auto à la destruction.
  - [NkFileMode](src/NKFileSystem/NkFile.h) flags bitmask combinables :
    READ/WRITE/APPEND/TRUNCATE/BINARY + combinaisons `NK_READ_BINARY`,
    `NK_WRITE_BINARY`, `NK_READ_WRITE_BINARY`, `NK_APPEND_BINARY`, etc.
  - API instance : `Open`, `Close`, `IsOpen`, `Read(void*, n)`, `Write`,
    `Seek(offset, NkSeekOrigin)`, `Tell`, `GetSize`, `Flush`.
  - API statique : `Exists`, `Delete`, `Copy`, `Rename/Move`, `ReadAllText`,
    `ReadAllBytes`, `WriteAllText`, `WriteAllBytes`.
  - Fallback Android `AAssetManager` (assets de l'APK lus quand `fopen`
    échoue).

### Répertoires
- [NkDirectory](src/NKFileSystem/NkDirectory.h) :
  - `Create`, `CreateRecursive`, `Delete(recursive=true/false)`, `Exists`,
    `Move`, `Copy`.
  - `Enumerate(path, pattern, NkSearchOption::TOP_DIRECTORY_ONLY|ALL_DIRECTORIES)`
    retourne un `NkVector<NkDirectoryEntry>` avec name, full path, size,
    mtime, isDir/isFile.
  - Glob basique style `*`/`?`.

### Métadonnées système
- [NkFileSystem](src/NKFileSystem/NkFileSystem.h) :
  - `NkDriveInfo` : nom, label, type FS (NTFS/FAT32/exFAT/ext4/HFS/APFS/...),
    capacité totale, espace libre/disponible.
  - `NkFileAttributes` : ReadOnly/Hidden/System/Archive/Compressed/Encrypted
    + timestamps creation/access/write.
  - APIs statiques : `GetDrives()`, `GetAttributes(path)`,
    `SetAttributes(path, attr)`, `GetTotalSpace`, `GetFreeSpace`, etc.

### Surveillance temps-réel
- [NkFileWatcher](src/NKFileSystem/NkFileWatcher.h) :
  - Backends : `inotify` (Linux), `ReadDirectoryChangesW` (Windows), fallback
    silencieux (Emscripten / minimal).
  - `NkFileChangeEvent { type, path, oldPath, timestamp }`,
    `NkFileChangeType::CREATED/DELETED/MODIFIED/RENAMED/ATTRIBUTE_CHANGED`.
  - Interface `NkFileWatcherCallback` + helper `NkSimpleFileWatcher`
    (functor `std::function`-like).
  - Thread de surveillance dédié avec cycle de vie propre (Start/Stop).

### Tests
- [test_smoke.cpp](tests/test_smoke.cpp) :
  - `NkPath` : jointure + extension.
  - `NkDirectory` + `NkFile` : create dir, write/read text, delete file,
    delete dir.

---

## En cours / TODO immédiat

### Tests à étendre
- `NkFileWatcher` : tests d'intégration (créer un fichier, vérifier que
  l'event CREATED remonte ; rename → RENAMED ; delete → DELETED).
- `NkFileSystem::GetDrives` : couvrir au moins le drive du test runner.
- `NkFileAttributes` : tests Win32 (Hidden, Archive bits).
- Tests cross-platform CI (Linux + Windows simultanés).

### Liens symboliques
La doc mentionne un "fallback silencieux sur plateformes limitées" pour les
symlinks. À durcir : exposer `NkFile::IsSymlink`, `NkFile::ResolveSymlink`,
`NkFile::CreateSymlink`. Windows nécessite privilège élevé ou
DEVELOPMENT_MODE.

### Glob avancé
- Actuellement `*` et `?` ; ajouter `**` (récursif), classes `[a-z]`,
  alternance `{a,b,c}`. Utile pour AssetBrowser de Unkeny.

---

## À venir / À ajouter (futur proche)

### Memory-mapped files
- `NkMmapFile` : `mmap` POSIX / `CreateFileMappingW`+`MapViewOfFile` Windows.
- Lecture/écriture rapide d'assets binaires (mesh, image, audio) sans copie.
- Indispensable pour `.nkb` (assets compilés) volumineux.

### VFS / archives montées
- `NkVirtualFileSystem` : montage de plusieurs sources (disque, archive
  `.zip`/`.nkpack`, mémoire) sous un même arbre virtuel.
- Cas d'usage : packaging final PV3DE avec assets compilés en archive
  unique.
- Hot-reload : un fichier physique override un fichier d'archive si présent.

### Async IO
- `NkFile::ReadAllBytesAsync()` retournant `NkFuture<NkVector<uint8>>`.
- Backend : Win32 `OVERLAPPED`, Linux `io_uring`/`aio`, macOS GCD.
- Intégration `NkThreadPool` pour fallback générique.

### Outils annexes
- `NkPath::Glob(pattern, recursive)` direct retournant `NkVector<NkPath>`.
- Helpers JSON/Binary pour `NkFileAttributes` (dépendance NKSerialization).
- Snapshot d'un répertoire (hash récursif des fichiers) pour cache de build.

### Plateformes
- Validation iOS (sandbox + scoped URLs).
- Validation Web : NkFileSystem.GetDrives() doit retourner un drive virtuel
  Emscripten IDBFS / MEMFS, NkFileWatcher = no-op documenté.

---

## Bugs / quirks connus
- Enum `NkFileSystemType::NK_UNKNOW` (sic — typo conservée pour
  compatibilité). Documenter ou ajouter alias `NK_UNKNOWN`.
- Le test `test_smoke.cpp` utilise `using namespace nkentseu::entseu;` —
  vestige legacy. Vérifier que le namespace `entseu` est encore exposé en
  alias, sinon le test ne compile plus.
- Sur Android, le fallback AAssetManager est en lecture seule ; le test des
  WriteAllText ne doit pas viser un asset.

---

## Dépendances
- **Couches en dessous (utilisées)** : NKCore (types, int64), NKContainers
  (NkString, NkVector), NKPlatform (détection OS, headers natifs).
- **Modules au-dessus qui en dépendent** : NKStream (NkFileStream wrap NkFile),
  NKSerialization (lecture/écriture .nkproj/.nkscene/.nkcase/.nkb via NkFile),
  NKImage / NKAudio / NKFont (chargement assets), NKRenderer (chargement
  shaders, textures, HDRI), Unkeny (AssetBrowser → enumerate + watch,
  ProjectManager → I/O .nkproj), Runtime (config files).
