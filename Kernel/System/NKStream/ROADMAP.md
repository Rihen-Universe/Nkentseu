# NKStream — Roadmap

État actuel (mai 2026) : Module minimaliste mais fonctionnel. Interface
`NkStream` (Open/Close/Read/Write/Seek/Tell/Size/IsEOF) avec trois
implémentations livrées : binaire en mémoire (`NkBinaryStream`), fichier
(`NkFileStream`, désormais bâti sur `NkFile` de NKFileSystem) et console
(`NkConsoleStream`). Encodage déclaré dans l'interface mais peu exploité.

---

## Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| Interface NkStream + flags OpenMode + Encoding | Livré | — | — |
| NkBinaryStream (buffer mémoire, owned/borrowed, resize) | Livré | — | — |
| NkFileStream (au-dessus de NkFile, mapping mode → NkFileMode) | Livré | — | — |
| NkConsoleStream (stdin/stdout/stderr, Win + POSIX) | Livré | — | — |
| Tests smoke (BinaryStream + FileStream round-trip) | Livré | — | — |
| Helpers typés (lecteur/écrivain bit-stream, varint, endian) | TODO | M | Haute |
| NkMemoryViewStream (read-only sur span sans owning) | TODO | S | Moyenne |
| Encoding réel (UTF-8 ↔ UTF-16 sur ConsoleStream Windows) | Partiel | S | Moyenne |
| NkBufferedStream (wrap d'un stream + buffering interne) | TODO | M | Moyenne |
| NkCompressedStream (LZ4/Zstd transparent) | TODO | L | Basse |
| NkNetworkStream (socket TCP/UDP wrapped) | TODO | M | Basse (dép. NKNetwork) |
| Async stream (NkAsyncRead/Write retournant NkFuture<T>) | TODO | L | Basse |
| Tests étendus (Seek-past-end, EOF semantics, encoding) | TODO | S | Haute |

Légende : Livré · Partiel · En cours · TODO · Abandonné

---

## Livré

### Interface et types
- [NkStream](src/NKStream/NkStream.h) : interface abstraite avec
  `OpenMode` (READ/WRITE/APPEND/BINARY/TEXT en flags), `Encoding`
  (SYSTEM/UTF-8/UTF-16 LE/BE), `Read<T>`/`Write<T>` templatisés sur octets.
- `Encoding` énuméré dans l'API mais `SetEncoding` / `GetEncoding` reviennent
  au défaut système dans la base.

### Implémentations
- [NkBinaryStream](src/NKStream/NkBinaryStream.h) : tampon mémoire avec
  `mBuffer/mCapacity/mSize/mPosition`, `mOwned` flag pour `delete[]`,
  growth × 2 dans `EnsureCapacity`, `Resize`/`Reserve` exposés, accès direct
  via `Data()`. Open/Close sont des no-op (le buffer existe toujours).
- [NkFileStream](src/NKStream/NkFileStream.h) : refactor récent, bâti sur
  `NkFile` (NKFileSystem). Bénéficie du fallback `AAssetManager` Android et
  du `NkPath`. Mapping `NkStream::OpenMode` ↔ `NkFileMode` (BINARY forcé pour
  éviter conversion newline).
- [NkConsoleStream](src/NKStream/NkConsoleStream.h) : Windows via
  `GetStdHandle` + `ReadConsoleW`/WriteConsole ; POSIX via fd 0/1/2.

### Tests
- [test_smoke.cpp](tests/test_smoke.cpp) : round-trip `NkBinaryStream` (write
  3 ints, seek 0, read 3 ints) ; round-trip fichier (`hello-stream` écrit puis
  relu via `NkFileStream`).
- [benchmark_smoke.cpp](tests/benchmark_smoke.cpp) : micro-bench Read/Write.

---

## En cours / TODO immédiat

### Encoding réel sur ConsoleStream
Windows lit déjà via `ReadConsoleW` (UTF-16) mais `WriteRaw` ne convertit pas
encore UTF-8 → UTF-16 explicitement (à vérifier dans le .cpp). À câbler avec
`SetEncoding(NK_UTF8)` honoré bidirectionnellement.

### Helpers typés
- `Read<T>(&value)` retourne déjà `usize` count, manque `WriteInt32LE`,
  `WriteFloat32BE`, etc. avec gestion endianness explicite.
- Encodage varint (protobuf-style) pour économie de bytes.
- `NkBitWriter` / `NkBitReader` pour pack bit-précis (utile NKNetwork
  `NkBitStream` — qui existe déjà séparément côté Protocol).

### Tests à étendre
- Sémantique EOF : `Read` partiel doit retourner le nombre d'octets lus puis
  `IsEOF()` true.
- `Seek` au-delà de la taille : pour `NkBinaryStream` retourne false ; pour
  `NkFileStream` valider le comportement (POSIX étend, Win32 idem).
- Test ConsoleStream non trivial (mock stdout) — actuellement aucun.

### Bug à vérifier
Dans [NkConsoleStream.h](src/NKStream/NkConsoleStream.h) le ternaire `type ==
NK_INPUT ? STD_INPUT_HANDLE : type == NK_INPUT ? STD_ERROR_HANDLE :
STD_OUTPUT_HANDLE` répète `NK_INPUT` deux fois — le second devrait être
`NK_ERROR`. Probable bug copy-paste qui shunte la sortie stderr.

---

## À venir / À ajouter (futur proche)

### Streams additionnels
- **NkMemoryViewStream** : read-only sur un `const uint8*` + length, sans
  copie, sans ownership.
- **NkBufferedStream** : wrap d'un autre `NkStream` avec un buffer interne
  configurable, pour réduire les appels système des `NkFileStream`.
- **NkCompressedStream** : adapter qui compresse/décompresse à la volée via
  LZ4 ou Zstd (dépend d'un module de compression à introduire).
- **NkNetworkStream** : wrap d'un `NkSocket` (NKNetwork) pour traiter une
  connexion comme un stream. Utile pour HTTP body streaming.
- **NkPipeStream** : flux nommé/unnamed pipe (IPC local) cross-platform.

### API moderne
- Migrer vers `nkentseu::span<byte>` (si NKContainers expose un span) pour
  `Read`/`Write` au lieu de pointeur brut + count.
- Variante `Read(NkVector<uint8>&, count)` avec growth automatique.
- Lecteurs textuels : `ReadLine(NkString&)`, `ReadUntil(char delim, ...)`.

### Asynchrone
- `NkFuture<usize> ReadAsync(buffer, count)` au-dessus du `NkThreadPool`.
- Modèle alternatif : callback-based avec `NkCompletion<T>`.

### Intégration
- Câbler `NkStream` comme source/sink interchangeable pour les readers
  NKSerialization (JSON/XML/YAML/Binary) au lieu de prendre `const char*`
  ou `NkFile&` directement.
- Branchement `NkConsoleStream` ↔ NKLogger `NkConsoleSink` (aujourd'hui le
  sink utilise stderr direct, pourrait passer par `NkConsoleStream`).

---

## Bugs / quirks connus
- `NkBinaryStream(usize capacity)` alloue via `new uint8[]` brut (pas
  d'allocateur custom NK). À reconnecter sur les allocateurs de NKMemory si
  une politique mémoire est requise.
- Le ternaire défectueux dans `NkConsoleStream` constructor (cf. ci-dessus)
  envoie stderr vers stdout.
- `NkStream::OpenMode` est exposé en énum non-scopé : risque de collision
  (`Read`/`Write` sont aussi des méthodes templates de la classe). Tests
  utilisent `NkStream::ReadMode` / `NkStream::BinaryMode` — vérifier que ces
  alias existent ou que le test est à jour.

---

## Dépendances
- **Couches en dessous (utilisées)** : NKPlatform (détection OS, headers
  Windows.h / POSIX), NKCore (types, usize), NKFileSystem (NkFile, NkFileMode,
  NkSeekOrigin — dépendance forte de `NkFileStream`).
- **Modules au-dessus qui en dépendent** : NKSerialization (lecture/écriture
  des `.nkproj`, `.nkscene`, `.nkb`, etc.), NKNetwork (peut wrap socket en
  stream), NKAudio/NKImage (chargement de fichiers via flux), NKFont
  (lecture TTF zero-copy via flux), Runtime (assets loaders).
