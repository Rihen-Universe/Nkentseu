# NKSerialization — documentation détaillée

Le module **NKSerialization**, partie par partie. Pour une vue d'ensemble et un guide « par où
commencer », voir le récap : [../NKSerialization.md](../NKSerialization.md).

Tout le module gravite autour du type pivot **`NkArchive`** : on peuple une archive avec des
`Set*`, un writer la sérialise dans un format, un reader la reconstruit, puis on relit avec des
`Get*`. Les readers/writers de format sont des classes purement statiques sans état, et les
erreurs remontent par `nk_bool` + `NkString*` optionnel (jamais par exception).

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [Core.md](Core.md) | Le type pivot `NkArchive` (`Set*`/`Get*`, chemins hiérarchiques, méta-données), l'interface `NkISerializable`, les registres par nom et par `NkTypeId`, le versioning de schéma et les migrations, l'API haut niveau du serializer (`Serialize`/`SaveToFile`/`LoadFromFile`, enum `NkSerializationFormat`, détection de format). | `NkSerializer.h`, `NkArchive.h`, `NkISerializable.h`, `NkSerializableRegistry.h`, `NkSchemaVersioning.h` |
| [JSON.md](JSON.md) | Échappement JSON RFC 8259 (`NkJSONEscapeString`/`NkJSONUnescapeString`), lecture (`NkJSONReader::ReadArchive`) et écriture (`NkJSONWriter::WriteArchive`, pretty/compact configurable). Pas de DOM `NkJSONValue`. | `JSON/NkJSONValue.h`, `JSON/NkJSONReader.h`, `JSON/NkJSONWriter.h` |
| [XML-YAML.md](XML-YAML.md) | Quatre classes statiques symétriques `ReadArchive`/`WriteArchive` : XML avec attribut `type=` pour le round-trip et indentation configurable, YAML piloté par l'indentation (toujours 2 espaces, marqueur `---`). | `XML/NkXMLReader.h`, `XML/NkXMLWriter.h`, `YAML/NkYAMLReader.h`, `YAML/NkYAMLWriter.h` |
| [Binary-Native.md](Binary-Native.md) | Format plat **NKS0** (valeurs en texte, objets dégradés en JSON), format natif récursif **NKS1** (binaire réel, CRC32, helpers I/O little-endian, compression LZ4 en stub), et la réflexion compile-time (`namespace NkReflect`, macros `NK_REFLECT_*`). | `Binary/NkBinaryReader.h`, `Binary/NkBinaryWriter.h`, `Native/NkNativeFormat.h`, `Native/NkReflect.h` |
| [Assets.md](Assets.md) | Pipeline d'assets `.nkasset` : GUID `NkAssetId`, chemins logiques, métadonnées, I/O fichier (`NkAssetIO`), registre global (`NkAssetRegistry`), importeur (`NkAssetImporter`) et cooker par plateforme (`NkAssetCooker`). | `Asset/NkAssetImporter.h`, `Asset/NkAssetManager.h` (vide), `Asset/NkAssetMetadata.h` |

[← Récap NKSerialization](../NKSerialization.md) · [← Couche System](../README.md)
