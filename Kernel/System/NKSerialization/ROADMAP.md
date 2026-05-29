# NKSerialization — Roadmap

État actuel (mai 2026) : **Module le plus mature du dossier System après
NKLogger/NKThreading**. Architecture mature autour d'une `NkArchive` clé/valeur
hiérarchique servant d'intermédiaire universel. **5 formats round-trip
fonctionnels** (JSON, XML, YAML, Binary générique, NkNative binaire avec
header+CRC32). **Système d'assets complet** (NkAssetId GUID 128-bit,
NkAssetMetadata, NkAssetRegistry, format `.nkasset` = Header+Metadata+Payload).
**15 tests smoke en suite standalone**, tous formats validés. Outil CLI
`nkecs-convert`. Reste à câbler en profondeur les formats projet
`.nkproj`/`.nkscene`/`.nkcase`/`.nkb` (cf. ARCHITECTURE.md §8), la réflexion
runtime via NKReflection, et le versioning de schéma.

---

## Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| NkArchive (scalaires, objets, arrays, paths "a.b.c", meta __meta__) | Livré | — | — |
| NkArchiveValue + NkArchiveNode + NkArchiveValueType (NULL/BOOL/INT64/UINT64/FLOAT64/STRING) | Livré | — | — |
| NkISerializable + NkSerializableRegistry (factory polymorphique) | Livré | — | — |
| NkSerializer (API haut niveau Serialize/Deserialize, SaveToFile/LoadFromFile) | Livré | — | — |
| Détection format auto (extension + magic bytes) | Livré | — | — |
| JSON reader/writer + NkJSONValue | Livré | — | — |
| XML reader/writer | Livré | — | — |
| YAML reader/writer | Livré | — | — |
| Binary generic reader/writer | Livré | — | — |
| NkNative format (header + archive + payload + CRC32) | Livré | — | — |
| NkReflect statique (macros NK_REFLECT_BEGIN/END, sérialisation auto struct C++) | Livré | — | — |
| NkAssetId GUID 128-bit + NkAssetPath logique "/Game/..." | Livré | — | — |
| NkAssetMetadata (tags, properties, dependencies, version) | Livré | — | — |
| NkAssetRegistry (scan + lookup par type/path) | Livré | — | — |
| NkAssetIO Write/Read fichier `.nkasset` | Livré | — | — |
| NkSchemaVersioning (compat ascendante/descendante) | Partiel | M | Haute |
| NkAssetImporter (pipeline source → .nkasset) | Partiel | L | Haute |
| Suite de tests 15 cas (Archive, formats, AssetIO, Registry, Merge) | Livré | — | — |
| CLI tool `nkecs-convert` | Livré | — | — |
| Câblage formats projet `.nkproj` / `.nkscene` / `.nkcase` / `.nkb` | TODO | L | Haute |
| Intégration NKReflection runtime (Phase NkSerializeReflect<T>) | TODO | L | Haute |
| Hot-reload via NkFileWatcher (assets) | TODO | M | Moyenne |
| Compression LZ4/Zstd dans NkNative payload | TODO | M | Moyenne |
| Streaming partiel (load partiel d'archive énorme) | TODO | L | Basse |
| Benchmarks réels (NkSerializationBenchmark commenté actuellement) | TODO | S | Moyenne |

Légende : Livré · Partiel · En cours · TODO · Abandonné

---

## Livré

### Cœur — NkArchive
- [NkArchive](src/NKSerialization/NkArchive.h) :
  - Hiérarchie : `NkArchiveValue` (scalaire portable) → `NkArchiveNode`
    (scalaire | objet | array) → `NkArchive` (table associative ordonnée).
  - Scalars : NULL, BOOL, INT64, UINT64, FLOAT64, STRING (UTF-8).
  - API plate : `SetString/Int64/UInt64/Bool/Float64`,
    `GetString/Int64/...`, `Has`, `Remove`.
  - API hiérarchique : `SetObject(name, child)`, `GetObject`, `SetArray`,
    `GetArray<NkArchiveValue>`, `SetPath("a.b.c", node)`, `GetPath(...)`.
  - Métadonnées : `SetMeta("author", ...)`, `GetMeta`.
  - `Merge(other, overwrite)` pour fusion.

### Interface et registry
- [NkISerializable](src/NKSerialization/NkISerializable.h) : interface
  STL-free avec `Serialize(NkArchive&)` / `Deserialize(const NkArchive&)`.
- [NkSerializableRegistry](src/NKSerialization/NkSerializableRegistry.h) :
  factory polymorphique. `Register<T>("Player")`, `Create(typeName)` →
  `NkUniquePtr<NkISerializable>`. Thread-safe en lecture après init.
- `CreateFromArchive(archive)` : lit `__type__` et délègue au registry.

### API haut niveau
- [NkSerializer](src/NKSerialization/NkSerializer.h) :
  - Énum [NkSerializationFormat](src/NKSerialization/NkSerializer.h) :
    NK_JSON / NK_XML / NK_YAML / NK_BINARY / NK_NATIVE / NK_AUTO_DETECT.
  - `Serialize<T>(obj, fmt, NkString&)` texte +
    `SerializeBinary<T>(...)` → `NkVector<uint8>`.
  - `Deserialize<T>(text, fmt, obj)` /
    `DeserializeBinary<T>(data, size, fmt, obj)`.
  - `SaveToFile<T>(obj, path, fmt)` / `LoadFromFile<T>(path, obj, fmt)`.
  - Détection auto via extension (`.json`/`.xml`/`.yaml`/`.yml`/`.bin`/`.nk`/
    `.nkasset`) puis magic bytes (`NKS1` Native, `NKS\0` Binary, `{` JSON,
    `<` XML).
  - `static_assert` que `T : NkISerializable`.
  - API bas niveau `SerializeArchiveText/Binary` /
    `DeserializeArchiveText/Binary` opérant directement sur `NkArchive`.

### Formats
- **JSON** : [NkJSONReader / NkJSONWriter / NkJSONValue](src/NKSerialization/JSON/) :
  parsing + écriture pretty/compact, support nested objects + arrays.
- **XML** : [NkXMLReader / NkXMLWriter](src/NKSerialization/XML/) avec
  écriture pretty.
- **YAML** : [NkYAMLReader / NkYAMLWriter](src/NKSerialization/YAML/),
  indentation minimale.
- **Binary** générique : [NkBinaryReader / NkBinaryWriter](src/NKSerialization/Binary/) :
  format compact non typé strict.
- **NkNative** : [NkNativeFormat](src/NKSerialization/Native/NkNativeFormat.h) :
  magic `NKS1`, header versionné, archive sérialisée, CRC32 final.
- **NkReflect** : [NkReflect.h](src/NKSerialization/Native/NkReflect.h) :
  réflexion **statique** compile-time via macros `NK_REFLECT_BEGIN/END`,
  SFINAE-based, type-safe sans RTTI runtime. Couvre scalaires + NkString +
  types imbriqués.

### Système d'assets
- [NkAssetMetadata](src/NKSerialization/Asset/NkAssetMetadata.h) :
  - `NkAssetId` : GUID 128-bit (`{hex16-hex16}`), `Generate()` / `ToString()`
    / `FromString()`.
  - `NkAssetPath` : chemin logique style Unreal `/Game/Meshes/Cube`.
  - `NkAssetMetadata` : id, assetPath, type (StaticMesh / Texture2D /
    Material / ...), typeName, version, tags, properties (NkArchive),
    dependencies (vector de `{AssetId, AssetPath}`).
  - `NkAssetRegistry` : `Register(record)`, `FindByPath`, `GetByType`,
    `SaveRegistry(.nkreg)`, `LoadRegistry(.nkreg)`, `Count()`.
  - `NkAssetIO` : `Write(path, meta, payload, size)`,
    `ReadMetadata(path, meta)`, `ReadPayload(path, vector)`.
  - Format `.nkasset` : `[FileHeader:32][MetadataSize:4][Metadata:NkNative]
    [PayloadSize:8][Payload:bytes]`.
- [NkAssetImporter](src/NKSerialization/Asset/NkAssetImporter.h) : pipeline
  source → .nkasset (header-only, à enrichir).

### Tests — suite standalone
[test_smoke.cpp](tests/test_smoke.cpp) — 15 tests sans framework externe :
1. Archive flat (Set/Get scalars, Remove, Has).
2. Archive hierarchical (SetObject/GetObject + array de scalars).
3. Archive path (`a.b.c` deep set/get) + metadata.
4. JSON round-trip flat.
5. JSON round-trip nested.
6. XML round-trip.
7. YAML round-trip.
8. Binary round-trip.
9. NkNative round-trip flat (`0xDEADBEEFCAFEBABE`).
10. NkNative nested objects.
11. NkAssetId generate/toString/fromString (35 chars `{16-16}`).
12. NkAssetMetadata serialize/deserialize (tags + deps + properties).
13. NkAssetIO write/read fichier `.nkasset` (2KB payload).
14. NkAssetRegistry save/load registry + lookups.
15. Archive Merge (overwrite=true|false).

### Outils
- `cli/tools/nkecs-convert` : utilitaire de conversion (probablement entre
  formats ou pour ECS — auditer en détail si besoin).

### Quirks réglés
- `NkAssetMetadata.h` undefines `GetObject` Win32 macro pour éviter collision
  avec `NkArchive::GetObject` quand `<windows.h>` est inclus en amont.

---

## En cours / TODO immédiat

### Câblage formats projet PV3DE / Unkeny (priorité 1)
ARCHITECTURE.md §8 demande explicitement :
- `.nkproj` (projet Unkeny) : JSON via `SaveToFile<NkProject>(path, NK_JSON)`.
- `.nkscene` (scène ECS) : JSON, sérialisation de `NkScene` (entities +
  components réfléchies).
- `.nkcase` (cas clinique PV3DE) : JSON étendu, sérialisation de
  `NkCaseLoader` data (cf. §5.3 NKDiagnostic).
- `.nkb` (assets binaires compilés) : NkNative + payload binaire.

Aujourd'hui les extensions ne sont **pas reconnues** par
`DetectFormatFromExtension` (seuls `json/xml/yaml/yml/bin/nk/nkasset` le sont).
Ajouter `nkproj → NK_JSON`, `nkscene → NK_JSON`, `nkcase → NK_JSON`,
`nkb → NK_NATIVE`.

### Intégration NKReflection runtime (priorité 1)
`NkReflect.h` (Native/) est **statique compile-time** (macros). Pour
InspectorPanel et édition live, il faut **runtime reflection** via
NKReflection :
- `NkSerializeReflect<T>(writer, instance)` itère sur `NkClass::GetProperty(i)`
  et appelle le sérialiseur générique selon `NkTypeCategory`.
- Bidirectionnel : `NkDeserializeReflect<T>` symétrique.
- Skip `NK_TRANSIENT` flag.
- Dépendance : NKReflection (voir leur ROADMAP — bloque ici).

### NkSchemaVersioning à finir
[NkSchemaVersioning](src/NKSerialization/NkSchemaVersioning.h) existe.
À durcir :
- Champ `__version__` auto dans tout NkArchive racine.
- Pipeline de migration `Migrate(archive, fromVersion, toVersion)` avec
  registry de migrateurs par type.
- Skipping/defaulting des champs ajoutés/retirés.
- Tests dédiés.

### Tests à étendre
- Tester `Serialize<MyClass>(obj, fmt, str)` end-to-end via
  `NkISerializable`, pas seulement `NkArchive`.
- Tester la factory polymorphique `CreateFromArchive` avec un type
  enregistré.
- Tester `NkReflect` macros statiques.
- Tester le détecteur magic bytes sur chaque format.
- Benchmark : décommenter `NkSerializationBenchmark.h` et publier des
  baselines (taille comparée JSON vs Binary vs Native, temps de serialize +
  deserialize, throughput MB/s par format).

---

## À venir / À ajouter (futur proche)

### Hot-reload des assets
- Couplage `NkFileWatcher` (NKFileSystem) + `NkAssetRegistry` : détection
  modification → rechargement automatique → notification consommateurs
  (`onAssetReloaded(NkAssetId)`).
- Cas d'usage Unkeny : modifier un `.nkasset` matériau → renderer recharge à
  la volée (cf. NKRenderer Phase G material hot-reload déjà livré).

### Compression dans NkNative
- Flag dans le file header : `COMPRESSED_LZ4` / `COMPRESSED_ZSTD`.
- Compress at write, decompress at read.
- Bénéfice immédiat sur `.nkb` (meshes, textures) et archives volumineuses.

### Streaming partiel
- API `NkAssetIO::ReadMetadataLazy(path)` qui ne lit que le header sans
  charger le payload — utile pour AssetBrowser thumbnails sans I/O complet.
- Streaming par chunk pour les gros assets (animation clips, audio long).

### NkAssetImporter pipeline complet
- Plugins d'import par type source : `.fbx`/`.gltf` → mesh, `.png`/`.exr` →
  texture, `.wav`/`.ogg` → audio.
- Dépendances inter-asset détectées à l'import.
- Generation des thumbnails (PNG 256x256) stockés dans le `.nkasset` payload
  ou en sidecar.

### Conteneurs réflechis et types custom
- Sérialiseurs natifs pour `NkVec3f`, `NkMat4f`, `NkQuat` (NKMath).
- Sérialiseurs pour les types NKTime (`NkDate`, `NkTimeSpan`).
- Pattern d'extension : `template<> void NkSerialize<MyType>(...)` spécialisé.

### CLI tools
- Étendre `nkecs-convert` : commandes `nk-validate <file>`, `nk-inspect <file>
  --json`, `nk-pack <dir> --out=archive.nkb`.
- Diff sémantique de deux archives (utile pour Git merge `.nkscene`).

### Sécurité / sanity
- Vérifier les CRC32 systématiquement à la lecture NkNative.
- Limites de profondeur de récursion (anti-DoS sur archive malicieuse).
- Bounds checking strict sur le parser binaire.

---

## Bugs / quirks connus
- Le `static_assert` de `Deserialize<T>` utilise `std::is_base_of` alors que
  `LoadFromFile<T>` utilise `traits::NkIsBaseOf` — incohérence stylistique
  (les deux fonctionnent mais le projet vise STL-free).
- `NkSerializationBenchmark.h` est commenté dans l'umbrella header. Ré-enable
  optionnel via macro à exposer.
- Doublon de commentaire doctring en haut de `NkISerializable.h` (le bloc est
  répété deux fois).
- Tests utilisent `/tmp/` en path littéral — non portable Windows. Devrait
  passer par `NkFileSystem` pour un tmp cross-platform.

---

## Dépendances
- **Couches en dessous (utilisées)** : NKCore (Types, Traits, Assert),
  NKContainers (NkString, NkStringView, NkVector), NKFileSystem (NkFile,
  NkFileMode), NKMemory (NkUniquePtr).
- **Couches en dessous optionnelles** : NKReflection (pour
  `NkSerializeReflect<T>` runtime — pas encore câblé).
- **Modules au-dessus qui en dépendent** :
  - **Unkeny / ProjectManager** : I/O `.nkproj`, `.nkscene`
    (cf. ARCHITECTURE.md §4.3).
  - **Unkeny / AssetManager** : `.nkasset` registry, hot-reload.
  - **NKRenderer / NkMaterialAsset** : déjà livré, charge `.nkasset` JSON
    pour les matériaux (cf. NKRenderer ROADMAP Phase G).
  - **NKScene** : sérialisation entities + components (via NKReflection une
    fois câblée).
  - **PV3DE / NkCaseLoader** : chargement `.nkcase` (JSON étendu) pour les
    scenarios cliniques (cf. ARCHITECTURE.md §5.3).
  - **PV3DE / ReportPanel** : export rapport FHIR (JSON) /
    PDF (cf. ARCHITECTURE.md §5.10).
