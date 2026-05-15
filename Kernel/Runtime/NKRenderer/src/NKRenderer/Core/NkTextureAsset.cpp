// =============================================================================
// NkTextureAsset.cpp  — NKRenderer Phase H
// =============================================================================
#include "NkTextureAsset.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
    namespace renderer {

        bool NkTextureAssetIO::Save(const NkTextureAsset& asset,
                                     const NkString& outDiskPath,
                                     const NkString& logicalPath,
                                     NkAssetId* outId) noexcept {
            // Metadata : id stable depuis logicalPath, type Texture2D.
            NkAssetMetadata meta;
            meta.id              = NkAssetId::FromName(logicalPath.View());
            meta.type            = NkAssetType::Texture2D;
            meta.typeName        = NkString("Texture2D");
            meta.assetPath       = NkAssetPath(logicalPath.View());
            meta.assetVersion    = 1u;
            meta.importTimestamp = 0u;
            meta.AddTag(NkAssetTypeName(meta.type));
            // Trace du fichier source pour reimport eventuel
            meta.sourceFilePath  = asset.sourceFilePath;

            // Payload = NkTextureAsset serialise en NkNative (compact + CRC).
            NkArchive payloadArchive;
            if (!asset.Serialize(payloadArchive)) {
                logger.Errorf("[NkTextureAssetIO] Serialize failed for '%s'\n",
                              logicalPath.CStr());
                return false;
            }
            NkVector<nk_uint8> payload;
            if (!native::NkNativeWriter::WriteArchive(payloadArchive, payload)) {
                logger.Errorf("[NkTextureAssetIO] NkNative encode failed for '%s'\n",
                              logicalPath.CStr());
                return false;
            }

            NkString err;
            if (!NkAssetIO::Write(outDiskPath.CStr(), meta,
                                  payload.Data(), payload.Size(), &err)) {
                logger.Errorf("[NkTextureAssetIO] Save failed : %s\n", err.CStr());
                return false;
            }
            // Enregistre dans le registry pour LoadById ulterieur.
            NkAssetRecord rec;
            rec.id        = meta.id;
            rec.assetPath = meta.assetPath;
            rec.type      = meta.type;
            rec.typeName  = meta.typeName;
            rec.diskPath  = outDiskPath;
            NkAssetRegistry::Global().Register(rec);

            if (outId) *outId = meta.id;
            return true;
        }

        NkTexHandle NkTextureAssetIO::Load(const NkString& diskPath,
                                            NkTextureLibrary* texLib) noexcept {
            if (!texLib) return NkTexHandle::Null();

            NkAssetMetadata meta;
            NkVector<nk_uint8> payload;
            NkString err;
            if (!NkAssetIO::ReadFull(diskPath.CStr(), meta, payload, &err)) {
                logger.Errorf("[NkTextureAssetIO] Load read failed : %s\n", err.CStr());
                return NkTexHandle::Null();
            }
            if (meta.type != NkAssetType::Texture2D) {
                logger.Warnf("[NkTextureAssetIO] '%s' is not Texture2D (type=%d)\n",
                             diskPath.CStr(), (int)meta.type);
                return NkTexHandle::Null();
            }

            NkArchive payloadArchive;
            if (!native::NkNativeReader::ReadArchive(payload.Data(), payload.Size(),
                                                     payloadArchive, &err)) {
                logger.Errorf("[NkTextureAssetIO] payload decode failed : %s\n",
                              err.CStr());
                return NkTexHandle::Null();
            }

            NkTextureAsset asset;
            if (!asset.Deserialize(payloadArchive)) {
                logger.Errorf("[NkTextureAssetIO] Deserialize failed\n");
                return NkTexHandle::Null();
            }

            // Charge effectivement la texture via NkTextureLibrary (lui-meme
            // s'appuie sur NkImage::Load qui auto-detecte le format par magic).
            NkLoadOptions opts;
            opts.genMipmaps = asset.generateMips;
            opts.srgb       = asset.sRGB;
            NkTexHandle h = texLib->Load(asset.sourceFilePath, opts);
            if (!h.IsValid()) {
                logger.Warnf("[NkTextureAssetIO] underlying texture load failed : '%s'\n",
                             asset.sourceFilePath.CStr());
            }
            return h;
        }

        NkTexHandle NkTextureAssetIO::LoadById(const NkAssetId& id,
                                                NkTextureLibrary* texLib) noexcept {
            if (!id.IsValid()) return NkTexHandle::Null();
            const NkAssetRecord* rec = NkAssetRegistry::Global().FindById(id);
            if (!rec) {
                logger.Warnf("[NkTextureAssetIO] LoadById : %s not in registry\n",
                             id.ToString().CStr());
                return NkTexHandle::Null();
            }
            return Load(rec->diskPath, texLib);
        }

    } // namespace renderer
} // namespace nkentseu
