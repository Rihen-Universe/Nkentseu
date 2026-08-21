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
  - **Trivia — mise à jour du 2026-08-21** : `NkArchiveTrivia`, bloc
    **facultatif** attaché aux nœuds et à l'archive. Voir la section dédiée
    ci-dessous.

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

### ⚠️ Les bancs de `tests/` NE S'EXECUTENT PAS — mesuré le 2026-08-21

`jenga test --project NKSerialization --config Debug` répond
**« Unit-test execution is disabled by workspace policy (`disableunittestexecution`) »**.
Les `testfiles(["tests/**.cpp"])` du `.jenga` (l. 77-80) **compilent sans jamais
tourner**, et `find Build -ipath "*NKSerialization*" -name "*.exe"` ne renvoie
rien. Les 15 tests de `test_smoke.cpp` et les trois bancs réflexion listés
ci-dessous **n'ont donc jamais produit un seul résultat**.

C'est la convention du dépôt : un banc doit être une **application console**
(voir `Applications/NKGuiDrawTest/NKGuiDrawTest.jenga` l. 10-13, qui documente
ce choix). D'où :

**`Sandbox/System/NKSerialization` — banc EXÉCUTABLE, 47/47 le 2026-08-21.**
`jenga build --target SandboxNKSerialization --config Debug` puis
`./Build/Bin/Debug-Windows/SandboxNKSerialization/SandboxNKSerialization.exe`
(code de sortie 0 = tout passe). Couvre C1 conteneur de scalaires, C2 conteneur
d'objets, C3 conteneurs **imbriqués** sur trois niveaux (la forme de
`NkUIDocument`), C4 provenance jamais omise même au défaut.

**`Sandbox/System/NKArchive` — second banc EXÉCUTABLE, 101/101 le 2026-08-21.**
`jenga build --target SandboxNKArchive --config Debug` puis
`./Build/Bin/Debug-Windows/SandboxNKArchive/SandboxNKArchive.exe`. Couvre la
mise à jour trivia / ordre / forme littérale de `NkArchive` — détail dans la
section dédiée plus bas.

**Ce que ça a coûté de ne pas l'avoir** : l'en-tête de `NkReflectSerializer.h`
annonçait la Phase 3 « repoussée, non gérée » alors que le `.cpp` l'implémentait
déjà. Aucun test ne tournant, rien ne contredisait l'en-tête, et cette
affirmation fausse a servi de base à une décision d'architecture du chantier
NkUIDesign. **Un en-tête est une promesse ; seul un banc qui tourne est une
preuve.**

### Phase 3 du pont réflexion — FAITE (et non « repoussée »)

Vérifié dans le corps de `Reflection/NkReflectSerializer.cpp`, pas dans son
en-tête, et **mesuré** par le banc ci-dessus :

| capacité | implémentation | état |
|---|---|---|
| conteneur de scalaires / strings `NkVector<T>` | `WriteContainerProperty` / `ReadContainerProperty` (l. 386 / 440) | ✅ |
| conteneur d'**objets réfléchis** (object-array récursif) | `WriteObjectContainerProperty` / `ReadObjectContainerProperty` (l. 358 / 416) | ✅ |
| conteneurs **imbriqués** (objets portant eux-mêmes des conteneurs) | par récursion de `SerializeReflected` | ✅ |
| **pointeurs** | — | ❌ **reste à faire** |

Support des deux côtés : `NKReflection/NkContainerTrait.h`
(`NkContainerDescriptor`, thunks templates spécialisés pour `NkVector<T>`) et
`NkArchive` (`SetArray`/`GetArray`, `SetObjectArray`/`GetObjectArray`,
`SetNodeArray`/`GetNodeArray`).

**Dette nommée — les pointeurs.** Seul morceau de Phase 3 non couvert. Sans
objet pour un document d'interface (`NkUIDocument` est **plat** : `NkVector<NkUINode>`
et une parenté en indices `int32`, zéro pointeur), mais à traiter avant tout
modèle qui en contiendrait — un graphe de scène, typiquement.

### `NkArchive` porte la mise en forme d'origine — 2026-08-21

**Le manque, en une phrase.** `NkArchive` ne portait rien de ce qu'un format
texte éditable à la main doit rendre à l'octet. Trois choses, pas une :

| ce qui manquait | pourquoi ça comptait |
|---|---|
| commentaires et lignes vides | exigence explicite de Rodolf ; leur perte avait déjà été refusée une fois sur `.nkgui` v0.3 |
| **l'ordre du fichier** | l'archive est ordonnée par insertion, donc déterministe — mais l'ordre obtenu est celui de la **déclaration du schéma**. Dès qu'un aller-retour passe par l'archive, les propriétés sont réordonnées et « octet pour octet » tombe |
| la forme littérale | `0.50` contre `0.5`, la casse d'une couleur hexadécimale, les guillemets. Réécrire une valeur canoniquement, c'est modifier une ligne que l'utilisateur n'a pas touchée |

**Ce qui a été ajouté** — `NkArchiveTrivia` (`leading`, `trailing`, `literal`,
`literalOf`, `sourceOrder`), attaché **par pointeur possédant facultatif** à
`NkArchiveNode` et à `NkArchive`. Plus, sur l'archive : `SetLeadingTrivia` /
`SetTrailingTrivia` / `SetLiteral` / `SetSourceOrder` par clé,
`SetHeaderTrivia` / `SetFooterTrivia`, `Lexeme(key)`, `SortBySourceOrder()` et
surtout **`AdoptFormatting(source)`**, qui greffe la mise en forme d'une archive
lue sur une archive reconstruite depuis le modèle. C'est cette dernière qui rend
l'aller-retour possible en pratique.

**Trois garanties, et comment elles tiennent :**

1. **Additive.** Un nœud sans trivia n'alloue rien (8 octets de pointeur nul) et
   se comporte exactement comme avant. Mesuré : mêmes octets JSON et mêmes
   octets NKS1 pour une archive avec et sans trivia (banc, T5 et T6).
2. **Facultative en mémoire.** Un document fabriqué par le code n'a ni
   commentaire, ni ordre d'origine, ni forme littérale : c'est valide, et
   `Lexeme()` retombe alors sur la forme canonique.
3. **Le piège de `NkGValue::raw` fermé PAR CONSTRUCTION.** L'écrivain `.nkgui`
   réémet `raw` verbatim ; un document construit en mémoire a un `raw` vide et
   s'écrit donc **vide, sans erreur**. Ici c'est impossible : `Lexeme()` n'a pas
   de branche qui rende du vide par défaut d'information, et pour `NK_VALUE_NULL`
   la forme canonique est le mot-clé `null`. Second piège, plus vicieux : un
   littéral **périmé**. `literalOf` retient le texte canonique auquel le littéral
   correspondait ; une valeur éditée (0.50 → 0.75) **désarme son littéral toute
   seule**. Sans ce garde-fou, l'écrivain réimprimerait `0.50` — c'est-à-dire
   **perdrait la modification de l'utilisateur**.

⚠️ **`NkArchive` a cessé d'être `= default` sur ses cinq méthodes spéciales.**
Elle porte désormais un pointeur possédant ; un `= default` provoquerait une
double libération. Mesuré : la mutation qui partage le pointeur au lieu de le
dupliquer tue le banc par **corruption de tas** (`0xC0000374`), avant même la
première ligne de sortie.

**Preuve — `Sandbox/System/NKArchive`, application console, 101/101, sortie 0.**
Sous `Sandbox/` et pas sous `tests/`, pour la raison de la section précédente.
Le cas qui tranche est **T1** : un fichier dont les propriétés sont dans un ordre
différent de l'ordre de déclaration du schéma, avec un commentaire en fin de
ligne et un flottant écrit `0.50`, fait l'aller-retour complet — lecture,
modèle réfléchi, réécriture — **octet pour octet**. Et **T0** garde la trace du
problème : le même aller-retour avec un écrivain qui ignore la trivia **abîme**
le fichier, et le banc nomme les trois causes une par une. Si T0 devenait vert,
c'est que le banc ne mesurerait plus rien.

**Discrimination prouvée par cinq mutations du code testé** (`NkArchive.cpp`) :

| mutation | résultat |
|---|---|
| `SortBySourceOrder` neutralisée | 93/101 — T1, T2, T8 rouges |
| `Lexeme()` renvoie toujours la forme canonique | 93/101 — T1, T3, T7, T8 rouges |
| garde-fou anti-périmé désactivé | 95/101 — **T1 reste VERT** : un aller-retour sans édition ne peut pas révéler un littéral périmé. Seul un contrôle qui ÉDITE l'attrape |
| trivia partagée au lieu d'être dupliquée | **crash**, corruption de tas |
| commentaires non greffés par `AdoptFormatting` | 96/101 — T1, T3, T8 rouges |

**Limite nommée.** La trivia est portée par les nœuds et par l'archive ; elle ne
couvre donc pas ce qui n'est ni l'un ni l'autre — typiquement un commentaire
posé entre le nom d'une clé et son `=`. Aucun besoin connu à ce jour.

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

### Câblage formats projet PV3DE / Noge (priorité 1)
ARCHITECTURE.md §8 demande explicitement :
- `.nkproj` (projet Noge) : JSON via `SaveToFile<NkProject>(path, NK_JSON)`.
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
- Cas d'usage Noge : modifier un `.nkasset` matériau → renderer recharge à
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
  - **Noge / ProjectManager** : I/O `.nkproj`, `.nkscene`
    (cf. ARCHITECTURE.md §4.3).
  - **Noge / AssetManager** : `.nkasset` registry, hot-reload.
  - **NKRenderer / NkMaterialAsset** : déjà livré, charge `.nkasset` JSON
    pour les matériaux (cf. NKRenderer ROADMAP Phase G).
  - **NKScene** : sérialisation entities + components (via NKReflection une
    fois câblée).
  - **PV3DE / NkCaseLoader** : chargement `.nkcase` (JSON étendu) pour les
    scenarios cliniques (cf. ARCHITECTURE.md §5.3).
  - **PV3DE / ReportPanel** : export rapport FHIR (JSON) /
    PDF (cf. ARCHITECTURE.md §5.10).
