# NKSerialization

Système de sérialisation multi-formats C++17 production-ready pour le moteur **Nkentseu**.  
Supporte JSON, YAML, XML, Binary (NKS0) et NkNative (NKS1) avec un système de fichiers d'assets inspiré d'Unreal Engine.

---

## Architecture

```
NKSerialization/
├── NkArchive.h/cpp              Archive centrale clé→valeur hiérarchique
├── NkISerializable.h            Interface + registre d'instanciation polymorphique
├── NkSerializer.h               Point d'entrée unifié (templates)
├── NkSerializableRegistry.h     Registre des composants ECS
├── NkSchemaVersioning.h         Versioning de schéma + migrations
├── NkSerializationBenchmark.h   Benchmark comparatif des formats
├── NkAllHeaders.h               Include unique
├── JSON/
│   ├── NkJSONReader.h/cpp       Parser JSON complet (nested objects/arrays)
│   ├── NkJSONWriter.h/cpp       Générateur JSON pretty/compact
│   └── NkJSONValue.h/cpp        Helpers escape/unescape
├── XML/
│   ├── NkXMLReader.h/cpp        Parser XML <archive>/<entry>/<object>/<array>
│   └── NkXMLWriter.h/cpp        Générateur XML
├── YAML/
│   ├── NkYAMLReader.h/cpp       Parser YAML block (indentation, sequences)
│   └── NkYAMLWriter.h/cpp       Générateur YAML
├── Binary/
│   ├── NkBinaryReader.h/cpp     Lecteur format NKS0 (flat, simple)
│   └── NkBinaryWriter.h/cpp     Générateur format NKS0
├── Native/
│   ├── NkNativeFormat.h         Format NKS1 (binaire optimisé, CRC32, nested)
│   └── NkReflect.h              Réflexion statique C++17
└── Asset/
    ├── NkAssetMetadata.h        NkAssetId, NkAssetMetadata, NkAssetRegistry, NkAssetIO
    └── NkAssetImporter.h        NkAssetImporter, NkAssetCooker
```

---

## Démarrage rapide

### 1. Sérialisation d'un objet

```cpp
#include "NKSerialization/NkAllHeaders.h"
using namespace nkentseu;

struct Player : public NkISerializable {
    NkString name  = "Hero";
    nk_int32 level = 1;
    nk_float64 hp = 100.0;

    nk_bool Serialize(NkArchive& arc) const override {
        arc.SetString("name",  name.View());
        arc.SetInt64 ("level", level);
        arc.SetFloat64("hp",   hp);
        return true;
    }
    nk_bool Deserialize(const NkArchive& arc) override {
        arc.GetString("name",  name);
        arc.GetInt64 ("level", (nk_int64&)level);
        arc.GetFloat64("hp",   hp);
        return true;
    }
    const char* GetTypeName() const noexcept override { return "Player"; }
};

// Sauvegarder (détection automatique du format depuis l'extension)
Player p;
SaveToFile(p, "player.json");    // JSON
SaveToFile(p, "player.yaml");    // YAML
SaveToFile(p, "player.xml");     // XML
SaveToFile(p, "player.nk");      // NkNative binaire

// Charger
Player p2;
LoadFromFile("player.json", p2);
```

### 2. Archive directe (sans NkISerializable)

```cpp
NkArchive archive;
archive.SetString("title",   "My Game");
archive.SetUInt64("version", 20260413ull);
archive.SetBool("debug",     true);

// JSON
NkString json;
NkJSONWriter::WriteArchive(archive, json, true /*pretty*/);

NkArchive loaded;
NkJSONReader::ReadArchive(json.View(), loaded);
```

### 3. Objets et tableaux imbriqués

```cpp
NkArchive root;

// Objet imbriqué
NkArchive config;
config.SetString("resolution", "1920x1080");
config.SetUInt32("fps",        60u);
root.SetObject("graphics", config);

// Chemin hiérarchique style "a.b.c"
root.SetPath("audio.master.volume",
    NkArchiveNode(NkArchiveValue::FromFloat64(0.8)));

const NkArchiveNode* vol = root.GetPath("audio.master.volume");

// Tableau de scalaires
NkVector<NkArchiveValue> arr;
arr.PushBack(NkArchiveValue::FromInt64(1));
arr.PushBack(NkArchiveValue::FromInt64(2));
arr.PushBack(NkArchiveValue::FromInt64(3));
root.SetArray("ids", arr);
```

### 4. Format NkNative (binaire optimisé)

```cpp
NkArchive arc;
arc.SetString("name", "entity_001");
arc.SetUInt64("id", 0xDEADBEEFull);

// Écriture binaire NKS1
NkVector<nk_uint8> data;
native::NkNativeWriter::WriteArchive(arc, data);

// Lecture
NkArchive loaded;
NkString err;
native::NkNativeReader::ReadArchive(data.Data(), data.Size(), loaded, &err);
```

### 5. Réflexion automatique

```cpp
#include "NKSerialization/Native/NkReflect.h"

struct Vec3 {
    nk_float32 x = 0.f, y = 0.f, z = 0.f;

    NK_REFLECT_BEGIN(Vec3)
        NK_REFLECT_FIELD(x)
        NK_REFLECT_FIELD(y)
        NK_REFLECT_FIELD(z)
    NK_REFLECT_END_SERIALIZE()
        NK_REFLECT_FIELD_D(x)
        NK_REFLECT_FIELD_D(y)
        NK_REFLECT_FIELD_D(z)
    NK_REFLECT_END()
};

Vec3 v{1.f, 2.f, 3.f};
NkArchive arc;
NkReflect::Serialize(v, arc);    // → {x:1, y:2, z:3}

Vec3 v2;
NkReflect::Deserialize(arc, v2);
```

---

## Système de fichiers d'assets (style Unreal Engine)

### Import d'un fichier source

```cpp
#include "NKSerialization/Asset/NkAssetImporter.h"

NkImportOptions opts;
opts.targetPath = "/Game/Textures";
opts.autoTags   = { NkString("albedo"), NkString("srgb") };

NkImportResult result = NkAssetImporter::Import(
    "art/albedo.png",        // fichier source
    "Content/Textures/",     // répertoire de sortie
    opts);

if (result.success) {
    printf("Asset ID: %s\n", result.assetId.ToString().CStr());
    printf("Output:   %s\n", result.outputPath.CStr());
    printf("Size:     %zu bytes\n", result.outputSize);
}
```

### Lecture des métadonnées d'un asset

```cpp
NkAssetMetadata meta;
NkAssetIO::ReadMetadata("Content/Textures/albedo.nkasset", meta);

printf("ID:      %s\n",  meta.id.ToString().CStr());
printf("Path:    %s\n",  meta.assetPath.path.CStr());
printf("Type:    %s\n",  NkAssetTypeName(meta.type));
printf("Version: %u\n",  meta.assetVersion);

for (nk_size i = 0; i < meta.tags.Size(); ++i)
    printf("Tag: %s\n", meta.tags[i].CStr());
```

### Registre global d'assets

```cpp
// Scanner un répertoire (appel unitaire par asset)
NkAssetRegistry::Global().RegisterAsset("Content/Meshes/Cube.nkasset");
NkAssetRegistry::Global().RegisterAsset("Content/Textures/albedo.nkasset");

// Lookup
const NkAssetRecord* rec = NkAssetRegistry::Global().FindByPath("/Game/Meshes/Cube");
if (rec) {
    printf("Disk: %s\n", rec->diskPath.CStr());
}

// Sauvegarde/chargement du registre
NkAssetRegistry::Global().SaveRegistry("Content/.registry");
NkAssetRegistry::Global().LoadRegistry("Content/.registry");
```

### Format du fichier .nkasset

```
[NkAssetFileHeader : 40 bytes]
  magic[8]       = "NKASSET0"
  formatVersion  = 1
  flags          = 0 (réservé)
  metadataSize   = N (taille du bloc metadata en bytes)
  payloadOffset  = 40 + N
  payloadSize    = taille du payload en bytes
  payloadCRC     = CRC32 du payload
  headerCRC      = CRC32 du header (champ mis à 0 pendant calcul)

[Metadata : N bytes]
  Format NkNative (NKS1) contenant les NkAssetMetadata sérialisées

[Payload : payloadSize bytes]
  Données brutes de l'asset (texture, mesh, audio…)
```

---

## Versioning de schéma

```cpp
// Déclarer la version courante d'un type
NK_SCHEMA_CURRENT_VERSION(PlayerStats, 2, 0, 0)

// Fonction de migration (free function)
nk_bool MigratePlayerV1toV2(NkArchive& arc,
    NkSchemaVersion from, NkSchemaVersion to) noexcept {
    // Ajouter le nouveau champ "maxHp" avec valeur par défaut
    if (!arc.Has("maxHp"))
        arc.SetFloat64("maxHp", 100.0);
    return true;
}

// Enregistrer la migration
NK_REGISTER_MIGRATION(PlayerStats, 1,0,0, 2,0,0, MigratePlayerV1toV2)

// Usage :
NkArchive arc;
// … charger arc depuis le disque …
nk_uint64 vPacked = 0;
arc.GetUInt64("__schema_version__", vPacked);
NkSchemaVersion stored = NkSchemaVersion::Unpack(vPacked);
NkSchemaRegistry::MigrateArchive(NkTypeOf<PlayerStats>(), arc, stored);
```

---

## Formats supportés

| Format | Extension | Type | Nested | Lisible | Taille relative |
|--------|-----------|------|--------|---------|-----------------|
| JSON   | `.json`   | Texte | ✅ | ✅ | 100% |
| XML    | `.xml`    | Texte | ✅ | ✅ | 150% |
| YAML   | `.yaml`   | Texte | ✅ | ✅ | 80% |
| Binary | `.bin`    | Binaire | ❌ (flat) | ❌ | 40% |
| NkNative | `.nk` / `.nkasset` | Binaire | ✅ | ❌ | 35% |

---

## Compilation

```cmake
# CMakeLists.txt minimal
cmake_minimum_required(VERSION 3.16)
project(NKSerialization CXX)
set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE NK_SOURCES "src/NKSerialization/*.cpp")
add_library(NKSerialization STATIC ${NK_SOURCES})
target_include_directories(NKSerialization PUBLIC include src)

# Tests
add_executable(NKSerializationTests tests/test_smoke.cpp)
target_link_libraries(NKSerializationTests NKSerialization)
```

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
./NKSerializationTests
```

---

## Corrections appliquées vs version originale

### Bugs critiques
- `NkNativeFormat.h` : `WriteValue()` accédait à `v.data.b/i/f` inexistants → `v.raw.*`
- `NkNativeFormat.h` : `Int64` stocké sur 4 bytes → troncature → 8 bytes
- `NkNativeFormat.h` : `Float64` stocké comme `float` → perte précision → 8 bytes IEEE754
- `NkNativeFormat.h` : Footer CRC calculé après écriture (invalide) → avant
- `NkSerializer.h` : variable `outError` non déclarée dans `Serialize()`
- `NkArchive` : `SetObject/GetObject/SetArray/GetArray` déclarés mais non implémentés
- `NkJSONReader` : objets/tableaux imbriqués ignorés silencieusement → parsing récursif
- Tous les namespaces `entseu` isolés → `nkentseu` unifié

### Améliorations
- `NkArchiveNode` : union discriminée Scalar/Object/Array avec big-5 RAII corrects
- `NkArchiveValue::raw` : union C synchronisée avec `.text`
- Chemins hiérarchiques `SetPath("a.b.c")` / `GetPath("a.b.c")`
- YAML : parser par indentation (objets imbriqués, séquences, `---`)
- XML : `<object>` et `<array>` récursifs
- `NkArchive::Merge()` avec option overwrite
- Réflexion statique C++17 via macros `NK_REFLECT_*`
- Système d'assets complet (NkAssetId, NkAssetMetadata, NkAssetIO, NkAssetRegistry, NkAssetImporter, NkAssetCooker)

---

## Licence

Propriétaire — © Nkentseu Engine 2026
