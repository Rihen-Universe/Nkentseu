# NKSerialization

> Couche **System** · La sérialisation multi-format du moteur : un cœur d'archives portable,
> les formats texte (JSON, XML, YAML), les formats binaires (plat NKS0, natif NKS1), la
> réflexion compile-time et un pipeline d'assets `.nkasset`.

Dès qu'une donnée doit **quitter la mémoire** — pour être écrite sur disque, envoyée sur le
réseau, ou rejouée plus tard — elle passe par NKSerialization. Le module est construit autour
d'un type pivot unique, **`NkArchive`** : une table associative ordonnée clé→valeur (plate ou
hiérarchique via des chemins `"a.b.c"`) qui sert de représentation intermédiaire commune. On
peuple une archive avec des `Set*`, puis n'importe quel writer (JSON, XML, YAML, binaire,
natif) la sérialise ; à l'inverse, n'importe quel reader reconstruit une archive qu'on relit
avec des `Get*`. Sauvegardes de scènes (`.nkscene`), projets (`.nkproj`), assets cuits, états
réseau, fichiers de config : tout converge vers ce même modèle.

Le module est **STL-free** : il manipule `NkArchive`, `NkString`/`NkStringView`, `NkVector`,
`NkFunction`, `NkUniquePtr` et les types primitifs `nk_bool`, `nk_int64`, `nk_uint64`,
`nk_float64`, `nk_size`. Les readers/writers de format sont des **classes purement statiques**
sans état (thread-safe entre appels), et les erreurs remontent par valeur de retour `nk_bool`
plus un `NkString*` optionnel — jamais par exception.

- **Namespace** : `nkentseu` (sous-namespaces réels : `nkentseu::native` pour le format NKS1,
  `nkentseu::native::io` pour les helpers little-endian, `nkentseu::NkReflect` qui est un
  **namespace** et non une classe)
- **Header parapluie** : `#include "NKSerialization/NKSerialization.h"`

---

## Par où commencer

Selon ce que vous cherchez à faire :

| Besoin | Page |
|--------|------|
| Construire/lire une archive, sérialiser un objet, gérer le versioning de schéma | [Cœur : archive & serializer](NKSerialization/Core.md) |
| Lire ou écrire du JSON | [JSON](NKSerialization/JSON.md) |
| Lire ou écrire du XML ou du YAML | [XML & YAML](NKSerialization/XML-YAML.md) |
| Sérialiser en binaire compact ou au format natif `.nk` + réfléchir une struct automatiquement | [Binaire & natif](NKSerialization/Binary-Native.md) |
| Importer, cuire et indexer des assets `.nkasset` | [Pipeline d'assets](NKSerialization/Assets.md) |

Chaque page détaille l'**API réellement déclarée** dans les headers (signatures exactes,
comportements, complexité) et signale les pièges concrets : stubs non implémentés, asymétries
de configuration, méthodes invalidant des pointeurs, et symboles qui n'apparaissent que dans
des exemples Doxygen sans être déclarés.

---

## Aperçu des familles

- **Cœur** (`NkSerializer.h`, `NkArchive.h`, `NkISerializable.h`, `NkSerializableRegistry.h`,
  `NkSchemaVersioning.h`) — `NkArchive` (le type pivot, API `Set*`/`Get*`, plate + chemins
  hiérarchiques + méta-données `__meta__`), l'interface `NkISerializable`
  (`Serialize`/`Deserialize`/`GetTypeName`), les registres (`NkSerializableRegistry` par nom,
  `NkComponentRegistry` par `NkTypeId`), le versioning de schéma (`NkSchemaVersion`,
  migrations, `NkSchemaRegistry`), et l'API haut niveau du serializer
  (`Serialize`/`Deserialize`/`SaveToFile`/`LoadFromFile`, enum `NkSerializationFormat`,
  détection de format).
- **JSON** (`JSON/NkJSONValue.h`, `JSON/NkJSONReader.h`, `JSON/NkJSONWriter.h`) — deux
  fonctions d'échappement RFC 8259 (`NkJSONEscapeString`/`NkJSONUnescapeString`) et les classes
  statiques `NkJSONReader::ReadArchive` / `NkJSONWriter::WriteArchive` (pretty configurable).
- **XML & YAML** (`XML/NkXMLReader.h`, `XML/NkXMLWriter.h`, `YAML/NkYAMLReader.h`,
  `YAML/NkYAMLWriter.h`) — quatre classes statiques symétriques `ReadArchive`/`WriteArchive`,
  XML avec attribut `type=` pour le round-trip, YAML piloté par l'indentation.
- **Binaire & natif** (`Binary/NkBinaryReader.h`, `Binary/NkBinaryWriter.h`,
  `Native/NkNativeFormat.h`, `Native/NkReflect.h`) — format plat **NKS0** (valeurs en texte,
  objets dégradés en JSON), format natif récursif **NKS1** (binaire réel, CRC32, helpers I/O
  little-endian), et la **réflexion** compile-time (`namespace NkReflect`, macros
  `NK_REFLECT_*`).
- **Assets** (`Asset/NkAssetMetadata.h`, `Asset/NkAssetImporter.h`) — GUID `NkAssetId`,
  chemins logiques `NkAssetPath`, métadonnées, I/O `.nkasset` (`NkAssetIO`), registre global
  (`NkAssetRegistry`), importeur (`NkAssetImporter`) et cooker par plateforme (`NkAssetCooker`).
  Note : `Asset/NkAssetManager.h` est un fichier **vide**.

---

## Index des headers

| Header | Contenu | Documenté dans |
|--------|---------|----------------|
| `NKSerialization.h` | Parapluie (inclut tout). | — |
| `NkSerializationApi.h` | Macros d'export (`NKENTSEU_SERIALIZATION_API`, `NKENTSEU_SERIALIZATION_CLASS_EXPORT`). | — |
| `NkSerializer.h` | API haut niveau + enum `NkSerializationFormat` + détection de format + `CreateFromArchive`. | [Cœur](NKSerialization/Core.md) |
| `NkArchive.h` | `NkArchive`, `NkArchiveValue`, `NkArchiveNode`, `NkArchiveEntry`, enums associés. | [Cœur](NKSerialization/Core.md) |
| `NkISerializable.h` | Interface `NkISerializable` + `NkSerializableRegistry` (par nom). | [Cœur](NKSerialization/Core.md) |
| `NkSerializableRegistry.h` | `NkComponentRegistry` (composants ECS par `NkTypeId`). | [Cœur](NKSerialization/Core.md) |
| `NkSchemaVersioning.h` | `NkTypeId`/`NkTypeOf`, `NkSchemaVersion`, migrations, `NkSchemaRegistry`. | [Cœur](NKSerialization/Core.md) |
| `JSON/NkJSONValue.h` | `NkJSONEscapeString` / `NkJSONUnescapeString` (pas de DOM). | [JSON](NKSerialization/JSON.md) |
| `JSON/NkJSONReader.h` | `NkJSONReader::ReadArchive`. | [JSON](NKSerialization/JSON.md) |
| `JSON/NkJSONWriter.h` | `NkJSONWriter::WriteArchive` (×2). | [JSON](NKSerialization/JSON.md) |
| `XML/NkXMLReader.h` | `NkXMLReader::ReadArchive`. | [XML & YAML](NKSerialization/XML-YAML.md) |
| `XML/NkXMLWriter.h` | `NkXMLWriter::WriteArchive` (×2). | [XML & YAML](NKSerialization/XML-YAML.md) |
| `YAML/NkYAMLReader.h` | `NkYAMLReader::ReadArchive`. | [XML & YAML](NKSerialization/XML-YAML.md) |
| `YAML/NkYAMLWriter.h` | `NkYAMLWriter::WriteArchive` (×2). | [XML & YAML](NKSerialization/XML-YAML.md) |
| `Binary/NkBinaryReader.h` | `NkBinaryReader::ReadArchive` (NKS0). | [Binaire & natif](NKSerialization/Binary-Native.md) |
| `Binary/NkBinaryWriter.h` | `NkBinaryWriter::WriteArchive` (NKS0). | [Binaire & natif](NKSerialization/Binary-Native.md) |
| `Native/NkNativeFormat.h` | NKS1 : `NkNativeType`, `NkCRC32`, `NkNativeWriter/Reader`, helpers `io`. | [Binaire & natif](NKSerialization/Binary-Native.md) |
| `Native/NkReflect.h` | `namespace NkReflect` + macros `NK_REFLECT_*`. | [Binaire & natif](NKSerialization/Binary-Native.md) |
| `Asset/NkAssetMetadata.h` | `NkAssetId`, `NkAssetPath`, métadonnées, `NkAssetIO`, `NkAssetRegistry`. | [Assets](NKSerialization/Assets.md) |
| `Asset/NkAssetImporter.h` | `NkAssetImporter`, `NkAssetCooker`. | [Assets](NKSerialization/Assets.md) |
| `Asset/NkAssetManager.h` | Fichier **vide** (0 octet) — aucun symbole. | — |

---

[← Couche System](README.md) · [Index du wiki](../README.md)
