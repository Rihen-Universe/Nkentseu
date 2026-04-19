// =============================================================================
// NKSerialization/Asset/NkAssetMetadata.h
// =============================================================================
// Système de fichiers d'assets et de métadonnées inspiré d'Unreal Engine.
//
// Concepts :
//   NkAssetId       — GUID 128-bit unique par asset
//   NkAssetPath     — Chemin logique style "/Game/Meshes/Cube"
//   NkAssetMetadata — Métadonnées attachées à tout asset
//   NkAssetRegistry — Registre global (scan de répertoire + lookup)
//   NkAssetFile     — Fichier .nkasset = Header + Archive + Payload binaire optionnel
//
// Format fichier .nkasset :
//   [FileHeader:32][MetadataSize:4][Metadata:NkNative][PayloadSize:8][Payload:bytes]
//   L'archive de métadonnées est en format NkNative (avec CRC).
//   Le payload brut est la donnée d'asset (texture, mesh, audio…).
// =============================================================================
#pragma once

#ifndef NKENTSEU_SERIALIZATION_ASSET_NKASSETMETADATA_H_INCLUDED
#define NKENTSEU_SERIALIZATION_ASSET_NKASSETMETADATA_H_INCLUDED

#include "NKSerialization/NkArchive.h"
#include "NKSerialization/Native/NkNativeFormat.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKFileSystem/NkFile.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace nkentseu {

// =============================================================================
// NkAssetId — GUID 128-bit
// =============================================================================
struct NkAssetId {
    nk_uint64 lo = 0;
    nk_uint64 hi = 0;

    [[nodiscard]] bool IsValid()  const noexcept { return lo != 0 || hi != 0; }
    [[nodiscard]] bool IsNull()   const noexcept { return !IsValid(); }
    bool operator==(const NkAssetId& o) const noexcept { return lo==o.lo && hi==o.hi; }
    bool operator!=(const NkAssetId& o) const noexcept { return !(*this == o); }
    bool operator< (const NkAssetId& o) const noexcept {
        return hi < o.hi || (hi == o.hi && lo < o.lo);
    }

    // Génère un GUID pseudo-aléatoire basé sur le clock + adresse mémoire
    [[nodiscard]] static NkAssetId Generate() noexcept {
        NkAssetId id;
        // Simple PRNG basé sur les compteurs système (non-cryptographique)
        static nk_uint64 s_counter = 0x517CC1B727220A95ULL;
        s_counter ^= s_counter << 13u;
        s_counter ^= s_counter >> 7u;
        s_counter ^= s_counter << 17u;
        id.lo = s_counter ^ reinterpret_cast<nk_uint64>(&id);
        id.hi = s_counter ^ static_cast<nk_uint64>(
            reinterpret_cast<nk_size>(&s_counter));
        return id;
    }

    // Format : "{xxxxxxxxxxxxxxxx-yyyyyyyyyyyyyyyy}"
    [[nodiscard]] NkString ToString() const noexcept {
        return NkString::Fmtf("{%016llX-%016llX}",
            static_cast<unsigned long long>(hi),
            static_cast<unsigned long long>(lo));
    }

    [[nodiscard]] static NkAssetId FromString(NkStringView s) noexcept {
        NkAssetId id;
        if (s.Length() < 35u) return id;
        char buf[64] = {};
        memcpy(buf, s.Data() + 1, 16u);
        id.hi = strtoull(buf, nullptr, 16);
        memcpy(buf, s.Data() + 18u, 16u); buf[16] = '\0';
        id.lo = strtoull(buf, nullptr, 16);
        return id;
    }

    static const NkAssetId& Invalid() noexcept {
        static NkAssetId s_invalid;
        return s_invalid;
    }
};

// =============================================================================
// NkAssetPath — Chemin logique "/Game/Meshes/Cube"
// =============================================================================
struct NkAssetPath {
    NkString path;   // ex: "/Game/Meshes/Cube"
    NkString name;   // ex: "Cube"

    NkAssetPath() noexcept = default;
    explicit NkAssetPath(NkStringView p) noexcept : path(p) {
        // Extraire le nom
        const char* raw = p.Data();
        nk_size len = p.Length();
        nk_size last = len;
        for (nk_size i = len; i-- > 0;) {
            if (raw[i] == '/' || raw[i] == '\\') { last = i + 1; break; }
            if (i == 0) { last = 0; break; }
        }
        name = NkString(raw + last, len - last);
    }

    [[nodiscard]] bool IsValid() const noexcept { return !path.Empty(); }
    bool operator==(const NkAssetPath& o) const noexcept { return path == o.path; }

    // Retourne le répertoire parent : "/Game/Meshes/Cube" → "/Game/Meshes"
    [[nodiscard]] NkString GetParent() const noexcept {
        nk_size slash = path.RFind('/');
        return (slash != NkString::npos) ? path.SubStr(0, slash) : NkString("/");
    }

    // Chemin physique : "/Game/Meshes/Cube" + rootDir → "C:/Content/Meshes/Cube.nkasset"
    [[nodiscard]] NkString ToFilePath(NkStringView rootDir,
                                      NkStringView ext = ".nkasset") const noexcept {
        NkString result(rootDir);
        // Retirer le préfixe "/Game" ou similaire (premier segment)
        NkStringView stripped = path.View();
        if (stripped.Length() > 0 && stripped[0] == '/') {
            nk_size next = 1;
            while (next < stripped.Length() && stripped[next] != '/') ++next;
            stripped = stripped.SubStr(next);
        }
        result.Append(stripped);
        result.Append(ext);
        return result;
    }
};

// =============================================================================
// NkAssetType — Catégorie d'asset
// =============================================================================
enum class NkAssetType : nk_uint16 {
    Unknown    = 0,
    StaticMesh = 1,
    SkeletalMesh = 2,
    Texture2D  = 3,
    TextureCube = 4,
    Material   = 5,
    MaterialInstance = 6,
    Sound      = 7,
    Animation  = 8,
    Blueprint  = 9,
    DataTable  = 10,
    Map        = 11,
    World      = 12,
    Prefab     = 13,
    Font       = 14,
    Shader     = 15,
    Script     = 16,
    Custom     = 255,
};

inline const char* NkAssetTypeName(NkAssetType t) noexcept {
    switch (t) {
        case NkAssetType::StaticMesh:      return "StaticMesh";
        case NkAssetType::SkeletalMesh:    return "SkeletalMesh";
        case NkAssetType::Texture2D:       return "Texture2D";
        case NkAssetType::TextureCube:     return "TextureCube";
        case NkAssetType::Material:        return "Material";
        case NkAssetType::MaterialInstance:return "MaterialInstance";
        case NkAssetType::Sound:           return "Sound";
        case NkAssetType::Animation:       return "Animation";
        case NkAssetType::Blueprint:       return "Blueprint";
        case NkAssetType::DataTable:       return "DataTable";
        case NkAssetType::Map:             return "Map";
        case NkAssetType::World:           return "World";
        case NkAssetType::Prefab:          return "Prefab";
        case NkAssetType::Font:            return "Font";
        case NkAssetType::Shader:          return "Shader";
        case NkAssetType::Script:          return "Script";
        case NkAssetType::Custom:          return "Custom";
        default:                           return "Unknown";
    }
}

// =============================================================================
// NkAssetDependency — Référence vers un autre asset
// =============================================================================
struct NkAssetDependency {
    NkAssetId   id;
    NkAssetPath path;
    bool        hardRef = true; // hard = bloque le chargement, soft = lazy
};

// =============================================================================
// NkAssetMetadata — Métadonnées d'un asset
// =============================================================================
struct NkAssetMetadata {
    // ── Identité ─────────────────────────────────────────────────────────────
    NkAssetId   id;
    NkAssetPath assetPath;
    NkAssetType type        = NkAssetType::Unknown;
    NkString    typeName;        // Nom de classe C++ (ex: "UStaticMesh")
    NkString    sourceFilePath;  // Fichier source avant import (ex: .fbx, .png)

    // ── Versioning ────────────────────────────────────────────────────────────
    nk_uint32   assetVersion    = 1u;  // Version du schéma de cet asset
    nk_uint32   engineVersion   = 0u;  // Version du moteur lors de la sauvegarde
    nk_uint64   importTimestamp = 0u;  // Unix timestamp de l'import

    // ── Payload ───────────────────────────────────────────────────────────────
    nk_uint64   payloadOffset = 0u; // Offset dans le fichier vers les données brutes
    nk_uint64   payloadSize   = 0u; // Taille des données brutes (0 si inexistant)
    nk_uint32   payloadCRC    = 0u; // CRC32 du payload

    // ── Tags et propriétés utilisateur ───────────────────────────────────────
    NkVector<NkString> tags;
    NkArchive          properties; // Propriétés custom (clé/valeur libres)

    // ── Dépendances ───────────────────────────────────────────────────────────
    NkVector<NkAssetDependency> dependencies;

    // ── Thumbnail ────────────────────────────────────────────────────────────
    NkVector<nk_uint8> thumbnailData; // PNG 128x128 ou vide

    // ── Helpers ───────────────────────────────────────────────────────────────
    void AddTag(NkStringView tag) noexcept {
        // Évite les doublons
        for (nk_size i = 0; i < tags.Size(); ++i)
            if (tags[i].Compare(tag) == 0) return;
        tags.PushBack(NkString(tag));
    }
    bool HasTag(NkStringView tag) const noexcept {
        for (nk_size i = 0; i < tags.Size(); ++i)
            if (tags[i].Compare(tag) == 0) return true;
        return false;
    }
    void AddDependency(const NkAssetId& depId, NkStringView depPath, bool hard = true) noexcept {
        for (nk_size i = 0; i < dependencies.Size(); ++i)
            if (dependencies[i].id == depId) return;
        NkAssetDependency dep;
        dep.id      = depId;
        dep.path    = NkAssetPath(depPath);
        dep.hardRef = hard;
        dependencies.PushBack(std::move(dep));
    }

    // ── Sérialisation vers NkArchive ─────────────────────────────────────────
    nk_bool Serialize(NkArchive& archive) const noexcept {
        // Identité
        archive.SetString("__id__",         id.ToString().View());
        archive.SetString("__path__",       assetPath.path.View());
        archive.SetString("__name__",       assetPath.name.View());
        archive.SetUInt32("__type__",       static_cast<nk_uint32>(type));
        archive.SetString("__typeName__",   typeName.View());
        archive.SetString("__source__",     sourceFilePath.View());
        // Versions
        archive.SetUInt32("__assetVer__",   assetVersion);
        archive.SetUInt32("__engineVer__",  engineVersion);
        archive.SetUInt64("__importTs__",   importTimestamp);
        // Payload
        archive.SetUInt64("__payloadOff__", payloadOffset);
        archive.SetUInt64("__payloadSz__",  payloadSize);
        archive.SetUInt32("__payloadCRC__", payloadCRC);

        // Tags
        {
            NkArchive tagArchive;
            tagArchive.SetUInt32("count", static_cast<nk_uint32>(tags.Size()));
            for (nk_size i = 0; i < tags.Size(); ++i) {
                NkString k = NkString::Fmtf("tag_%llu", (unsigned long long)i);
                tagArchive.SetString(k.View(), tags[i].View());
            }
            archive.SetObject("__tags__", tagArchive);
        }

        // Propriétés utilisateur
        if (!properties.Empty()) {
            archive.SetObject("__props__", properties);
        }

        // Dépendances
        {
            NkArchive depsArchive;
            depsArchive.SetUInt32("count", static_cast<nk_uint32>(dependencies.Size()));
            for (nk_size i = 0; i < dependencies.Size(); ++i) {
                const auto& dep = dependencies[i];
                NkArchive depArc;
                depArc.SetString("id",   dep.id.ToString().View());
                depArc.SetString("path", dep.path.path.View());
                depArc.SetBool("hard",   dep.hardRef);
                NkString k = NkString::Fmtf("dep_%llu", (unsigned long long)i);
                depsArchive.SetObject(k.View(), depArc);
            }
            archive.SetObject("__deps__", depsArchive);
        }

        return true;
    }

    nk_bool Deserialize(const NkArchive& archive) noexcept {
        NkString idStr;
        archive.GetString("__id__",       idStr);       id       = NkAssetId::FromString(idStr.View());
        NkString pathStr;
        archive.GetString("__path__",     pathStr);     assetPath = NkAssetPath(pathStr.View());
        archive.GetString("__typeName__", typeName);
        archive.GetString("__source__",   sourceFilePath);

        nk_uint32 typeRaw = 0; archive.GetUInt32("__type__",    typeRaw); type = static_cast<NkAssetType>(typeRaw);
        archive.GetUInt32("__assetVer__",  assetVersion);
        archive.GetUInt32("__engineVer__", engineVersion);
        archive.GetUInt64("__importTs__",  importTimestamp);
        archive.GetUInt64("__payloadOff__",payloadOffset);
        archive.GetUInt64("__payloadSz__", payloadSize);
        archive.GetUInt32("__payloadCRC__",payloadCRC);

        // Tags
        NkArchive tagArchive;
        if (archive.GetObject("__tags__", tagArchive)) {
            nk_uint32 cnt = 0; tagArchive.GetUInt32("count", cnt);
            for (nk_uint32 i = 0; i < cnt; ++i) {
                NkString k = NkString::Fmtf("tag_%u", i);
                NkString tv; tagArchive.GetString(k.View(), tv);
                tags.PushBack(std::move(tv));
            }
        }

        // Propriétés
        archive.GetObject("__props__", properties);

        // Dépendances
        NkArchive depsArchive;
        if (archive.GetObject("__deps__", depsArchive)) {
            nk_uint32 cnt = 0; depsArchive.GetUInt32("count", cnt);
            for (nk_uint32 i = 0; i < cnt; ++i) {
                NkString k = NkString::Fmtf("dep_%u", i);
                NkArchive depArc;
                if (!depsArchive.GetObject(k.View(), depArc)) continue;
                NkAssetDependency dep;
                NkString depId, depPath;
                depArc.GetString("id",   depId);
                depArc.GetString("path", depPath);
                depArc.GetBool("hard",   dep.hardRef);
                dep.id   = NkAssetId::FromString(depId.View());
                dep.path = NkAssetPath(depPath.View());
                dependencies.PushBack(std::move(dep));
            }
        }
        return true;
    }
};

// =============================================================================
// NkAssetFileHeader — En-tête du fichier .nkasset (32 bytes, taille fixe)
// =============================================================================
struct NkAssetFileHeader {
    nk_uint8  magic[8]       = {'N','K','A','S','S','E','T','0'};
    nk_uint16 formatVersion  = 1u;
    nk_uint16 flags          = 0u;  // réservé
    nk_uint32 metadataSize   = 0u;  // taille du bloc metadata NkNative
    nk_uint64 payloadOffset  = 0u;  // offset absolu dans le fichier vers le payload
    nk_uint64 payloadSize    = 0u;  // taille en bytes du payload
    nk_uint32 payloadCRC     = 0u;  // CRC32 du payload
    nk_uint32 headerCRC      = 0u;  // CRC32 de ce header (champ mis à 0 pendant calcul)
};
static_assert(sizeof(NkAssetFileHeader) == 40u, "NkAssetFileHeader must be 40 bytes");

// =============================================================================
// NkAssetIO — Lecture/écriture de fichiers .nkasset
// =============================================================================
class NkAssetIO {
public:
    // ── Écriture ──────────────────────────────────────────────────────────────
    [[nodiscard]] static nk_bool Write(const char* filePath,
                                       const NkAssetMetadata& meta,
                                       const nk_uint8* payloadData = nullptr,
                                       nk_size payloadSize = 0u,
                                       NkString* err = nullptr) noexcept {
        if (!filePath) { if (err) *err = NkString("Null file path"); return false; }

        // Sérialiser les métadonnées
        NkArchive metaArchive;
        if (!meta.Serialize(metaArchive)) {
            if (err) *err = NkString("Failed to serialize metadata");
            return false;
        }

        NkVector<nk_uint8> metaBinary;
        if (!native::NkNativeWriter::WriteArchive(metaArchive, metaBinary)) {
            if (err) *err = NkString("Failed to encode metadata");
            return false;
        }

        // Calculer le CRC du payload avant de l'injecter dans les métadonnées
        nk_uint32 payloadCRC = (payloadSize > 0 && payloadData)
            ? native::NkCRC32::Compute(payloadData, payloadSize) : 0u;

        // Injecter offset/size/CRC dans les métadonnées pour qu'ils soient
        // lisibles après ReadMetadata() sans re-parser le header binaire.
        // NOTE: On re-sérialise après injection (deux passes).
        {
            nk_size estimatedMetaSize = metaBinary.Size();
            nk_uint64 estimatedOffset = sizeof(NkAssetFileHeader) + estimatedMetaSize;
            metaArchive.SetUInt64("__payloadOff__", estimatedOffset);
            metaArchive.SetUInt64("__payloadSz__",  static_cast<nk_uint64>(payloadSize));
            metaArchive.SetUInt32("__payloadCRC__", payloadCRC);
            // Re-encoder avec les valeurs finales
            metaBinary.Clear();
            if (!native::NkNativeWriter::WriteArchive(metaArchive, metaBinary)) {
                if (err) *err = NkString("Failed to re-encode metadata");
                return false;
            }
        }

        // Construire le header
        NkAssetFileHeader header;
        header.metadataSize  = static_cast<nk_uint32>(metaBinary.Size());
        header.payloadOffset = sizeof(NkAssetFileHeader) + metaBinary.Size();
        header.payloadSize   = static_cast<nk_uint64>(payloadSize);
        header.payloadCRC    = payloadCRC;

        // CRC du header (avec payloadCRC déjà calculé, headerCRC = 0)
        header.headerCRC = native::NkCRC32::Compute(&header, sizeof(header));

        // Écriture sur disque
        NkFile file;
        if (!file.Open(filePath, NkFileMode::NK_WRITE)) {
            if (err) *err = NkString("Cannot create asset file");
            return false;
        }

        bool ok = true;
        ok &= (file.Write(&header, sizeof(header)) == sizeof(header));
        ok &= (file.Write(metaBinary.Data(), metaBinary.Size()) == metaBinary.Size());
        if (payloadData && payloadSize > 0u)
            ok &= (file.Write(payloadData, payloadSize) == payloadSize);

        file.Close();
        if (!ok && err) *err = NkString("Write error");
        return ok;
    }

    // ── Lecture des métadonnées uniquement (sans charger le payload) ──────────
    [[nodiscard]] static nk_bool ReadMetadata(const char* filePath,
                                               NkAssetMetadata& out,
                                               NkString* err = nullptr) noexcept {
        if (!filePath) { if (err) *err = NkString("Null file path"); return false; }

        NkFile file;
        if (!file.Open(filePath, NkFileMode::NK_READ)) {
            if (err) *err = NkString("Cannot open asset file");
            return false;
        }

        NkAssetFileHeader header;
        if (file.Read(&header, sizeof(header)) != sizeof(header)) {
            file.Close();
            if (err) *err = NkString("Cannot read header");
            return false;
        }

        // Valider le magic
        if (memcmp(header.magic, "NKASSET0", 8u) != 0) {
            file.Close();
            if (err) *err = NkString("Invalid asset magic");
            return false;
        }

        // Valider le CRC du header
        {
            nk_uint32 storedCRC = header.headerCRC;
            header.headerCRC = 0u;
            nk_uint32 computed = native::NkCRC32::Compute(&header, sizeof(header));
            header.headerCRC = storedCRC;
            if (computed != storedCRC) {
                file.Close();
                if (err) *err = NkString("Header CRC mismatch");
                return false;
            }
        }

        // Lire le bloc metadata
        NkVector<nk_uint8> metaBinary(header.metadataSize);
        if (file.Read(metaBinary.Data(), metaBinary.Size()) != metaBinary.Size()) {
            file.Close();
            if (err) *err = NkString("Cannot read metadata block");
            return false;
        }
        file.Close();

        // Décoder
        NkArchive metaArchive;
        if (!native::NkNativeReader::ReadArchive(
                metaBinary.Data(), metaBinary.Size(), metaArchive, err)) return false;

        return out.Deserialize(metaArchive);
    }

    // ── Lecture du payload uniquement ─────────────────────────────────────────
    [[nodiscard]] static nk_bool ReadPayload(const char* filePath,
                                              NkVector<nk_uint8>& out,
                                              NkString* err = nullptr) noexcept {
        if (!filePath) { if (err) *err = NkString("Null file path"); return false; }

        NkFile file;
        if (!file.Open(filePath, NkFileMode::NK_READ)) {
            if (err) *err = NkString("Cannot open asset file");
            return false;
        }

        NkAssetFileHeader header;
        if (file.Read(&header, sizeof(header)) != sizeof(header)) {
            file.Close();
            if (err) *err = NkString("Cannot read header");
            return false;
        }

        if (memcmp(header.magic, "NKASSET0", 8u) != 0) {
            file.Close();
            if (err) *err = NkString("Invalid asset magic");
            return false;
        }

        if (header.payloadSize == 0u) {
            file.Close(); out.Clear(); return true; // pas de payload
        }

        file.Seek(static_cast<nk_int64>(header.payloadOffset), NkSeekOrigin::NK_BEGIN);
        out.Resize(static_cast<nk_size>(header.payloadSize));
        if (file.Read(out.Data(), out.Size()) != out.Size()) {
            file.Close();
            if (err) *err = NkString("Cannot read payload");
            return false;
        }
        file.Close();

        // Vérifier le CRC
        if (header.payloadCRC != 0u) {
            nk_uint32 crc = native::NkCRC32::Compute(out.Data(), out.Size());
            if (crc != header.payloadCRC) {
                if (err) *err = NkString("Payload CRC mismatch");
                return false;
            }
        }
        return true;
    }

    // ── Lecture complète (metadata + payload) ─────────────────────────────────
    [[nodiscard]] static nk_bool ReadFull(const char* filePath,
                                           NkAssetMetadata& meta,
                                           NkVector<nk_uint8>& payload,
                                           NkString* err = nullptr) noexcept {
        if (!ReadMetadata(filePath, meta, err)) return false;
        return ReadPayload(filePath, payload, err);
    }
};

// =============================================================================
// NkAssetRecord — Entrée légère dans le registre (pas de payload)
// =============================================================================
struct NkAssetRecord {
    NkAssetId   id;
    NkAssetPath assetPath;
    NkAssetType type     = NkAssetType::Unknown;
    NkString    typeName;
    NkString    diskPath; // Chemin réel du fichier .nkasset sur le disque
    bool        loaded   = false;
};

// =============================================================================
// NkAssetRegistry — Registre global de tous les assets du projet
// =============================================================================
class NkAssetRegistry {
public:
    [[nodiscard]] static NkAssetRegistry& Global() noexcept {
        static NkAssetRegistry s_instance;
        return s_instance;
    }

    // ── Enregistrement ────────────────────────────────────────────────────────

    // Enregistre un asset depuis son chemin disque (lit seulement les métadonnées)
    [[nodiscard]] nk_bool RegisterAsset(NkStringView diskPath,
                                        NkString* err = nullptr) noexcept {
        NkString pathStr(diskPath);
        NkAssetMetadata meta;
        if (!NkAssetIO::ReadMetadata(pathStr.CStr(), meta, err)) return false;

        // Chercher si déjà enregistré
        for (nk_size i = 0; i < mRecords.Size(); ++i) {
            if (mRecords[i].id == meta.id) {
                // Mise à jour
                mRecords[i].diskPath  = pathStr;
                mRecords[i].assetPath = meta.assetPath;
                mRecords[i].type      = meta.type;
                mRecords[i].typeName  = meta.typeName;
                return true;
            }
        }

        NkAssetRecord rec;
        rec.id        = meta.id;
        rec.assetPath = meta.assetPath;
        rec.type      = meta.type;
        rec.typeName  = meta.typeName;
        rec.diskPath  = pathStr;
        mRecords.PushBack(std::move(rec));
        return true;
    }

    // Enregistre manuellement un record (sans fichier)
    void Register(const NkAssetRecord& rec) noexcept {
        for (nk_size i = 0; i < mRecords.Size(); ++i) {
            if (mRecords[i].id == rec.id) { mRecords[i] = rec; return; }
        }
        mRecords.PushBack(rec);
    }

    void Unregister(const NkAssetId& id) noexcept {
        for (nk_size i = 0; i < mRecords.Size(); ++i) {
            if (mRecords[i].id == id) { mRecords.Erase(mRecords.begin() + i); return; }
        }
    }

    // ── Lookup ────────────────────────────────────────────────────────────────
    [[nodiscard]] const NkAssetRecord* FindById(const NkAssetId& id) const noexcept {
        for (nk_size i = 0; i < mRecords.Size(); ++i)
            if (mRecords[i].id == id) return &mRecords[i];
        return nullptr;
    }

    [[nodiscard]] const NkAssetRecord* FindByPath(NkStringView path) const noexcept {
        for (nk_size i = 0; i < mRecords.Size(); ++i)
            if (mRecords[i].assetPath.path.Compare(path) == 0) return &mRecords[i];
        return nullptr;
    }

    [[nodiscard]] const NkAssetRecord* FindByName(NkStringView name) const noexcept {
        for (nk_size i = 0; i < mRecords.Size(); ++i)
            if (mRecords[i].assetPath.name.Compare(name) == 0) return &mRecords[i];
        return nullptr;
    }

    // Retourne tous les assets d'un type donné
    void GetByType(NkAssetType type, NkVector<const NkAssetRecord*>& out) const noexcept {
        for (nk_size i = 0; i < mRecords.Size(); ++i)
            if (mRecords[i].type == type) out.PushBack(&mRecords[i]);
    }

    // Retourne tous les assets ayant un tag
    void GetByTag(NkStringView tag, NkVector<NkAssetId>& out) const noexcept {
        (void)tag; // Les tags sont dans les métadonnées complètes, pas dans le record
        // Pour les tags, il faut charger les métadonnées depuis le disque.
        // On expose ici juste la signature — implémentation projet-dépendante.
    }

    [[nodiscard]] nk_size Count() const noexcept { return mRecords.Size(); }
    [[nodiscard]] const NkVector<NkAssetRecord>& Records() const noexcept { return mRecords; }

    void Clear() noexcept { mRecords.Clear(); }

    // ── Sauvegarde/chargement du registre lui-même ────────────────────────────
    [[nodiscard]] nk_bool SaveRegistry(const char* path,
                                        NkString* err = nullptr) const noexcept {
        NkArchive archive;
        archive.SetUInt32("count", static_cast<nk_uint32>(mRecords.Size()));
        for (nk_size i = 0; i < mRecords.Size(); ++i) {
            NkArchive rec;
            rec.SetString("id",       mRecords[i].id.ToString().View());
            rec.SetString("path",     mRecords[i].assetPath.path.View());
            rec.SetString("typeName", mRecords[i].typeName.View());
            rec.SetUInt32("type",     static_cast<nk_uint32>(mRecords[i].type));
            rec.SetString("diskPath", mRecords[i].diskPath.View());
            NkString k = NkString::Fmtf("rec_%llu", (unsigned long long)i);
            archive.SetObject(k.View(), rec);
        }
        NkVector<nk_uint8> binary;
        if (!native::NkNativeWriter::WriteArchive(archive, binary)) {
            if (err) *err = NkString("Failed to encode registry");
            return false;
        }

        NkFile file;
        if (file.Open(path, NkFileMode::NK_WRITE)) {
            if (!file.Write(binary.Data(), binary.Size())) {
                file.Close();
                if (err) *err = NkString("Failed to write registry");
                return false;
            }
            file.Close();
            return true;
        }

        if (err) *err = NkString("Cannot open registry file");
        return false;
    }

    [[nodiscard]] nk_bool LoadRegistry(const char* path, NkString* err = nullptr) noexcept {
        NkFile file;
        if (!file.Open(path, NkFileMode::NK_READ)) {
            if (err) *err = NkString("Cannot open registry file");
            return false;
        }

        nk_size sz = file.Size();
        NkVector<nk_uint8> binary(sz);

        if (file.Read(binary.Data(), sz) != sz) {
            file.Close();
            if (err) *err = NkString("Read error");
            return false;
        }
        file.Close();

        NkArchive archive;
        if (!native::NkNativeReader::ReadArchive(binary.Data(), sz, archive, err)) return false;

        mRecords.Clear();
        nk_uint32 count = 0; archive.GetUInt32("count", count);
        for (nk_uint32 i = 0; i < count; ++i) {
            NkString k = NkString::Fmtf("rec_%u", i);
            NkArchive recArc;
            if (!archive.GetObject(k.View(), recArc)) continue;
            NkAssetRecord rec;
            NkString idStr;
            recArc.GetString("id",       idStr);     rec.id = NkAssetId::FromString(idStr.View());
            NkString pathStr;
            recArc.GetString("path",     pathStr);   rec.assetPath = NkAssetPath(pathStr.View());
            recArc.GetString("typeName", rec.typeName);
            nk_uint32 typeRaw = 0; recArc.GetUInt32("type", typeRaw);
            rec.type = static_cast<NkAssetType>(typeRaw);
            recArc.GetString("diskPath", rec.diskPath);
            mRecords.PushBack(std::move(rec));
        }
        return true;
    }

private:
    NkVector<NkAssetRecord> mRecords;
};

} // namespace nkentseu

#endif // NKENTSEU_SERIALIZATION_ASSET_NKASSETMETADATA_H_INCLUDED
