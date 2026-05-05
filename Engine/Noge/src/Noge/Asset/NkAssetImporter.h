// =============================================================================
// NKSerialization/Asset/NkAssetImporter.h
// =============================================================================
// Système d'import d'assets inspiré d'Unreal Engine.
//
// Pipeline :
//   Fichier source (fbx, png, wav…)
//      → NkAssetImporter::Import()
//          → NkAssetMetadata + payload binaire
//              → NkAssetIO::Write() → .nkasset
//
// Fonctionnalités :
//   - Détection automatique du type par extension
//   - Génération de l'AssetId unique
//   - Stockage du chemin source pour re-import
//   - Timestamps pour détecter les changements
//   - Compression optionnelle du payload
// =============================================================================
#pragma once

#ifndef NKENTSEU_SERIALIZATION_ASSET_NKASSETIMPORTER_H
#define NKENTSEU_SERIALIZATION_ASSET_NKASSETIMPORTER_H

#include "NKSerialization/Asset/NkAssetMetadata.h"
#include <ctime>

namespace nkentseu {

// =============================================================================
// NkImportOptions — Options d'import configurables
// =============================================================================
struct NkImportOptions {
    // Compression du payload (requiert LZ4 en production)
    bool   compressPayload    = false;

    // Générer un thumbnail 128x128 (pas implémenté sans librairie image)
    bool   generateThumbnail  = false;

    // Chemin logique cible ex: "/Game/Textures"
    NkString targetPath;

    // Nom custom (vide = nom du fichier source sans extension)
    NkString overrideName;

    // Tags à ajouter automatiquement
    NkVector<NkString> autoTags;

    // Propriétés custom
    NkArchive extraProperties;
};

// =============================================================================
// NkImportResult — Résultat de l'import
// =============================================================================
struct NkImportResult {
    bool        success      = false;
    NkAssetId   assetId;
    NkString    outputPath;  // chemin du .nkasset généré
    NkString    error;
    nk_size     payloadSize  = 0u;
    nk_size     outputSize   = 0u; // taille totale du .nkasset
};

// =============================================================================
// NkAssetImporter — Import de fichiers sources vers .nkasset
// =============================================================================
class NkAssetImporter {
public:

    // ── Détection du type par extension ───────────────────────────────────────
    [[nodiscard]] static NkAssetType DetectType(const char* sourcePath) noexcept {
        if (!sourcePath) return NkAssetType::Unknown;
        const char* dot = strrchr(sourcePath, '.');
        if (!dot) return NkAssetType::Unknown;
        const char* ext = dot + 1;

        // Meshes
        if (strcasecmp(ext, "fbx") == 0 || strcasecmp(ext, "obj") == 0 ||
            strcasecmp(ext, "gltf") == 0 || strcasecmp(ext, "glb") == 0 ||
            strcasecmp(ext, "dae") == 0)
            return NkAssetType::StaticMesh;

        // Textures
        if (strcasecmp(ext, "png") == 0 || strcasecmp(ext, "jpg") == 0 ||
            strcasecmp(ext, "jpeg") == 0 || strcasecmp(ext, "tga") == 0 ||
            strcasecmp(ext, "bmp") == 0 || strcasecmp(ext, "hdr") == 0 ||
            strcasecmp(ext, "exr") == 0 || strcasecmp(ext, "dds") == 0)
            return NkAssetType::Texture2D;

        // Sons
        if (strcasecmp(ext, "wav") == 0 || strcasecmp(ext, "ogg") == 0 ||
            strcasecmp(ext, "mp3") == 0 || strcasecmp(ext, "flac") == 0)
            return NkAssetType::Sound;

        // Shaders
        if (strcasecmp(ext, "glsl") == 0 || strcasecmp(ext, "hlsl") == 0 ||
            strcasecmp(ext, "vert") == 0 || strcasecmp(ext, "frag") == 0 ||
            strcasecmp(ext, "comp") == 0 || strcasecmp(ext, "spv") == 0)
            return NkAssetType::Shader;

        // Scripts
        if (strcasecmp(ext, "lua") == 0 || strcasecmp(ext, "py") == 0 || strcasecmp(ext, "js") == 0)
            return NkAssetType::Script;

        // Fonts
        if (strcasecmp(ext, "ttf") == 0 || strcasecmp(ext, "otf") == 0)
            return NkAssetType::Font;

        // Data
        if (strcasecmp(ext, "json") == 0 || strcasecmp(ext, "csv") == 0 ||
            strcasecmp(ext, "xml") == 0 || strcasecmp(ext, "yaml") == 0)
            return NkAssetType::DataTable;

        return NkAssetType::Custom;
    }

    // ── Import principal ───────────────────────────────────────────────────────
    // Lit le fichier source, génère les métadonnées et écrit un .nkasset.
    // Le payload est le contenu brut du fichier source (non transformé).
    // En production, on brancherait ici les convertisseurs (FBX→NkMesh, etc.)
    [[nodiscard]] static NkImportResult Import(
        const char*          sourcePath,
        const char*          outputDir,
        const NkImportOptions& opts = {}) noexcept
    {
        NkImportResult result;

        if (!sourcePath || !outputDir) {
            result.error = NkString("Null path");
            return result;
        }

        // ── Lire le fichier source ────────────────────────────────────────────
        NkFile file;
        if (!file.Open(sourcePath, NkFileMode::NK_READ)) {
            result.error = NkString::Fmtf("Cannot open source: %s", sourcePath);
            return result;
        }

        nk_size srcSize = static_cast<nk_size>(file.GetSize());
        NkVector<nk_uint8> payload(srcSize);

        if (srcSize > 0 && file.Read(payload.Data(), srcSize) != srcSize) {
            file.Close();
            result.error = NkString("Read error on source file");
            return result;
        }
        
        file.Close();

        // ── Construire les métadonnées ────────────────────────────────────────
        NkAssetMetadata meta;
        meta.id   = NkAssetId::Generate();
        meta.type = DetectType(sourcePath);

        // Nom de l'asset
        NkString name;
        if (!opts.overrideName.Empty()) {
            name = opts.overrideName;
        } else {
            // Extraire le nom du fichier sans extension
            const char* slash = strrchr(sourcePath, '/');
            if (!slash) slash = strrchr(sourcePath, '\\');
            const char* basename = slash ? slash + 1 : sourcePath;
            const char* dot      = strrchr(basename, '.');
            name = dot ? NkString(basename, static_cast<nk_size>(dot - basename))
                       : NkString(basename);
        }

        // Chemin logique
        NkString logicalPath;
        if (!opts.targetPath.Empty()) {
            logicalPath = opts.targetPath;
            if (logicalPath.Back() != '/') logicalPath.Append('/');
            logicalPath.Append(name);
        } else {
            logicalPath = NkString("/Game/") + name;
        }
        meta.assetPath      = NkAssetPath(logicalPath.View());
        meta.typeName       = NkString(NkAssetTypeName(meta.type));
        meta.sourceFilePath = NkString(sourcePath);
        meta.assetVersion   = 1u;
        meta.importTimestamp = static_cast<nk_uint64>(time(nullptr));

        // Tags automatiques
        for (nk_size i = 0; i < opts.autoTags.Size(); ++i)
            meta.AddTag(opts.autoTags[i].View());
        // Tag type
        meta.AddTag(NkAssetTypeName(meta.type));

        // Propriétés supplémentaires
        meta.properties.Merge(opts.extraProperties);
        meta.properties.SetString("source_format",
            NkString(strrchr(sourcePath, '.') ? strrchr(sourcePath, '.') + 1 : "unknown").View());
        meta.properties.SetUInt64("source_size", srcSize);

        // ── Chemin de sortie ──────────────────────────────────────────────────
        NkString outPath(outputDir);
        if (outPath.Back() != '/' && outPath.Back() != '\\') outPath.Append('/');
        outPath.Append(name);
        outPath.Append(".nkasset");

        // ── Écriture du .nkasset ──────────────────────────────────────────────
        NkString writeErr;
        if (!NkAssetIO::Write(
                outPath.CStr(), meta,
                srcSize > 0 ? payload.Data() : nullptr, srcSize,
                &writeErr)) {
            result.error = writeErr;
            return result;
        }

        // ── Enregistrer dans le registre global ───────────────────────────────
        NkAssetRecord rec;
        rec.id        = meta.id;
        rec.assetPath = meta.assetPath;
        rec.type      = meta.type;
        rec.typeName  = meta.typeName;
        rec.diskPath  = outPath;
        NkAssetRegistry::Global().Register(rec);

        // ── Résultat ──────────────────────────────────────────────────────────
        result.success     = true;
        result.assetId     = meta.id;
        result.outputPath  = outPath;
        result.payloadSize = srcSize;

        // Taille du fichier de sortie
        FILE* fo = fopen(outPath.CStr(), "rb");
        if (fo) {
            fseek(fo, 0, SEEK_END);
            result.outputSize = static_cast<nk_size>(ftell(fo));
            fclose(fo);
        }

        return result;
    }

    // ── Re-import : mettre à jour un asset existant ───────────────────────────
    [[nodiscard]] static NkImportResult Reimport(
        const char*            assetPath,    // chemin du .nkasset existant
        const NkImportOptions& opts = {}) noexcept
    {
        NkImportResult result;

        // Lire les métadonnées existantes pour récupérer le sourceFilePath
        NkAssetMetadata existingMeta;
        NkString err;
        if (!NkAssetIO::ReadMetadata(assetPath, existingMeta, &err)) {
            result.error = NkString::Fmtf("Cannot read existing asset: %s", err.CStr());
            return result;
        }

        if (existingMeta.sourceFilePath.Empty()) {
            result.error = NkString("Asset has no source file path (manual asset?)");
            return result;
        }

        // Récupérer le répertoire de sortie depuis le chemin existant
        NkString outDir(assetPath);
        nk_size slash = outDir.RFind('/');
        if (slash == NkString::npos) slash = outDir.RFind('\\');
        if (slash != NkString::npos) outDir = outDir.SubStr(0, slash);
        else outDir = NkString(".");

        // Ré-importer depuis la source
        NkImportOptions reimportOpts = opts;
        if (reimportOpts.targetPath.Empty())
            reimportOpts.targetPath = existingMeta.assetPath.GetParent();
        if (reimportOpts.overrideName.Empty())
            reimportOpts.overrideName = existingMeta.assetPath.name;

        return Import(existingMeta.sourceFilePath.CStr(), outDir.CStr(), reimportOpts);
    }

    // ── Vérification si le fichier source a changé ────────────────────────────
    [[nodiscard]] static bool NeedsReimport(const char* assetPath) noexcept {
        NkAssetMetadata meta;
        if (!NkAssetIO::ReadMetadata(assetPath, meta)) return false;
        if (meta.sourceFilePath.Empty()) return false;

        // Vérifier le timestamp du fichier source
        struct stat st {};
        if (stat(meta.sourceFilePath.CStr(), &st) != 0) return false;
        nk_uint64 srcTimestamp = static_cast<nk_uint64>(st.st_mtime);
        return srcTimestamp > meta.importTimestamp;
    }

private:
    // strcasecmp portable
    static int strcasecmp(const char* a, const char* b) noexcept {
        while (*a && *b) {
            char ca = (*a >= 'A' && *a <= 'Z') ? (*a + 32) : *a;
            char cb = (*b >= 'A' && *b <= 'Z') ? (*b + 32) : *b;
            if (ca != cb) return ca - cb;
            ++a; ++b;
        }
        return *a - *b;
    }
};

// =============================================================================
// NkAssetCooker — Cuisson d'assets (optimisation pour la plateforme cible)
// =============================================================================
class NkAssetCooker {
public:
    enum class Platform { PC, Console, Mobile, Web };

    struct CookOptions {
        Platform   platform          = Platform::PC;
        bool       compressPayload   = true;
        bool       stripSourcePaths  = true;  // Retirer sourceFilePath du cooked build
        bool       stripThumbnails   = true;
        nk_uint32  maxTextureSize    = 4096u; // px (0 = pas de limite)
    };

    // Cuit un .nkasset vers un répertoire de destination
    [[nodiscard]] static bool Cook(
        const char*        inputAsset,
        const char*        outputDir,
        const CookOptions& opts = {},
        NkString*          err  = nullptr) noexcept
    {
        // Lire l'asset complet
        NkAssetMetadata meta;
        NkVector<nk_uint8> payload;
        NkString readErr;
        if (!NkAssetIO::ReadFull(inputAsset, meta, payload, &readErr)) {
            if (err) *err = NkString::Fmtf("Cook read error: %s", readErr.CStr());
            return false;
        }

        // Appliquer les transformations de cuisson
        if (opts.stripSourcePaths) meta.sourceFilePath.Clear();
        if (opts.stripThumbnails)  meta.thumbnailData.Clear();

        // Ajouter des tags de plateforme
        const char* platformTag = "PC";
        switch (opts.platform) {
            case Platform::Console: platformTag = "console"; break;
            case Platform::Mobile:  platformTag = "mobile";  break;
            case Platform::Web:     platformTag = "web";     break;
            default:                break;
        }
        meta.AddTag(platformTag);
        meta.AddTag("cooked");

        // Chemin de sortie
        NkString outPath(outputDir);
        if (outPath.Back() != '/' && outPath.Back() != '\\') outPath.Append('/');
        outPath.Append(meta.assetPath.name);
        outPath.Append(".nkasset");

        // Écriture
        return NkAssetIO::Write(
            outPath.CStr(), meta,
            payload.Empty() ? nullptr : payload.Data(),
            payload.Size(),
            err);
    }
};

} // namespace nkentseu

#endif // NKENTSEU_SERIALIZATION_ASSET_NKASSETIMPORTER_H
