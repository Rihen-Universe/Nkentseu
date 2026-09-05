// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkTextureAsset.cpp  — NKRenderer Phase H
// =============================================================================
#include "NkTextureAsset.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace renderer {

		bool NkTextureAssetIO::Save(const NkTextureAsset &asset, const NkString &outDiskPath,
									const NkString &logicalPath, NkAssetId *outId) noexcept {
			// Metadata : id stable depuis logicalPath, type Texture2D.
			NkAssetMetadata meta;
			meta.id = NkAssetId::FromName(logicalPath.View());
			meta.type = NkAssetType::Texture2D;
			meta.typeName = NkString("Texture2D");
			meta.assetPath = NkAssetPath(logicalPath.View());
			meta.assetVersion = 1u;
			meta.importTimestamp = 0u;
			meta.AddTag(NkAssetTypeName(meta.type));
			// Trace du fichier source pour reimport eventuel
			meta.sourceFilePath = asset.sourceFilePath;

			// Payload = NkTextureAsset serialise en NkNative (compact + CRC).
			NkArchive payloadArchive;
			if (!asset.Serialize(payloadArchive)) {
				logger.Errorf("[NkTextureAssetIO] Serialize failed for '%s'\n", logicalPath.CStr());
				return false;
			}
			NkVector<nk_uint8> payload;
			if (!native::NkNativeWriter::WriteArchive(payloadArchive, payload)) {
				logger.Errorf("[NkTextureAssetIO] NkNative encode failed for '%s'\n", logicalPath.CStr());
				return false;
			}

			NkString err;
			if (!NkAssetIO::Write(outDiskPath.CStr(), meta, payload.Data(), payload.Size(), &err)) {
				logger.Errorf("[NkTextureAssetIO] Save failed : %s\n", err.CStr());
				return false;
			}
			// Enregistre dans le registry pour LoadById ulterieur.
			NkAssetRecord rec;
			rec.id = meta.id;
			rec.assetPath = meta.assetPath;
			rec.type = meta.type;
			rec.typeName = meta.typeName;
			rec.diskPath = outDiskPath;
			NkAssetRegistry::Global().Register(rec);

			if (outId)
				*outId = meta.id;
			return true;
		}

		// =====================================================================
		// Actif CUIT — ce que le four produit et que le moteur televerse tel quel
		// =====================================================================
		bool NkTextureAssetIO::SaveBaked(const nk_uint8 *payload, nk_size payloadSize, const NkString &outDiskPath,
										 const NkString &logicalPath, const NkString &sourceFilePath,
										 NkAssetId *outId) noexcept {
			if (!payload || payloadSize == 0u) {
				logger.Errorf("[NkTextureAssetIO] SaveBaked : payload vide pour '%s'\n", logicalPath.CStr());
				return false;
			}
			// Refuser d'ecrire un payload qui n'est pas une texture cuite : sans
			// ce controle on produirait un `.nktex` que le chargeur refuserait
			// plus tard, loin d'ici.
			NkTexVue controle;
			NkString err;
			if (!NkTexturePayload::Decode(payload, payloadSize, controle, &err)) {
				logger.Errorf("[NkTextureAssetIO] SaveBaked : payload invalide (%s)\n", err.CStr());
				return false;
			}

			NkAssetMetadata meta;
			meta.id = NkAssetId::FromName(logicalPath.View());
			meta.type = NkAssetType::Texture2D;
			meta.typeName = NkString("Texture2D");
			meta.assetPath = NkAssetPath(logicalPath.View());
			meta.assetVersion = 1u;
			meta.AddTag(NkAssetTypeName(meta.type));
			meta.AddTag("cuit");
			// Trace de l'original : elle ne sert qu'a proposer une RECUISSON si
			// le fichier source change. Le chargement ne l'ouvre jamais.
			meta.sourceFilePath = sourceFilePath;

			if (!NkAssetIO::Write(outDiskPath.CStr(), meta, payload, payloadSize, &err)) {
				logger.Errorf("[NkTextureAssetIO] SaveBaked : ecriture refusee (%s)\n", err.CStr());
				return false;
			}

			NkAssetRecord rec;
			rec.id = meta.id;
			rec.assetPath = meta.assetPath;
			rec.type = meta.type;
			rec.typeName = meta.typeName;
			rec.diskPath = outDiskPath;
			NkAssetRegistry::Global().Register(rec);
			if (outId)
				*outId = meta.id;
			return true;
		}

		NkTexHandle NkTextureAssetIO::LoadBaked(const NkString &diskPath, NkTextureLibrary *texLib) noexcept {
			if (!texLib)
				return NkTexHandle::Null();

			NkAssetMetadata meta;
			NkVector<nk_uint8> payload;
			NkString err;
			if (!NkAssetIO::ReadFull(diskPath.CStr(), meta, payload, &err))
				return NkTexHandle::Null(); // absent ou abime : l'appelant repliera
			if (meta.type != NkAssetType::Texture2D && meta.type != NkAssetType::TextureCube) {
				logger.Warnf("[NkTextureAssetIO] '%s' n'est pas une texture (type=%d)\n", diskPath.CStr(),
							 (int)meta.type);
				return NkTexHandle::Null();
			}

			NkTexVue vue;
			if (!NkTexturePayload::Decode(payload.Data(), payload.Size(), vue, &err)) {
				// Ce n'est pas une erreur : un `.nktex` d'ancienne generation porte
				// des REGLAGES d'import, pas des pixels. On le dit, sans crier.
				return NkTexHandle::Null();
			}

			NkLoadOptions opts;
			opts.srgb = vue.EstSrgb();
			opts.genMipmaps = false; // ils sont DANS le fichier
			opts.useClampEdge = (vue.addressMode == NKTEXADDR_CLAMP);
			opts.debugName = diskPath.CStr();
			return texLib->CreateFromBaked(vue, opts);
		}

		// Compteur du message de repli — voir `CompteRepliDit`.
		static nk_uint32 s_repliDit = 0u;

		nk_uint32 NkTextureAssetIO::CompteRepliDit() noexcept {
			return s_repliDit;
		}

		NkTexHandle NkTextureAssetIO::Load(const NkString &diskPath, NkTextureLibrary *texLib) noexcept {
			if (!texLib)
				return NkTexHandle::Null();

			// D'abord l'actif CUIT : s'il porte des pixels, aucun codec ne tourne.
			{
				NkTexHandle cuit = LoadBaked(diskPath, texLib);
				if (cuit.IsValid())
					return cuit;
			}

			// Repli : l'actif ne porte que des REGLAGES d'import, il faut decoder
			// l'image source. On le DIT UNE FOIS par execution — repete a chaque
			// texture, le message deviendrait du bruit que personne ne lit.
			if (s_repliDit == 0u) {
				++s_repliDit;
				logger.Warn("[NkTextureAssetIO] '{0}' ne porte pas de texture cuite : repli sur le codec (decodage a "
							"chaque chargement). Passe-le au four pour supprimer ce cout. Ce message ne sera pas "
							"repete.\n",
							diskPath.CStr());
			}

			NkAssetMetadata meta;
			NkVector<nk_uint8> payload;
			NkString err;
			if (!NkAssetIO::ReadFull(diskPath.CStr(), meta, payload, &err)) {
				logger.Errorf("[NkTextureAssetIO] Load read failed : %s\n", err.CStr());
				return NkTexHandle::Null();
			}
			if (meta.type != NkAssetType::Texture2D) {
				logger.Warnf("[NkTextureAssetIO] '%s' is not Texture2D (type=%d)\n", diskPath.CStr(), (int)meta.type);
				return NkTexHandle::Null();
			}

			NkArchive payloadArchive;
			if (!native::NkNativeReader::ReadArchive(payload.Data(), payload.Size(), payloadArchive, &err)) {
				logger.Errorf("[NkTextureAssetIO] payload decode failed : %s\n", err.CStr());
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
			opts.srgb = asset.sRGB;
			NkTexHandle h = texLib->Load(asset.sourceFilePath, opts);
			if (!h.IsValid()) {
				logger.Warnf("[NkTextureAssetIO] underlying texture load failed : '%s'\n", asset.sourceFilePath.CStr());
			}
			return h;
		}

		NkTexHandle NkTextureAssetIO::LoadById(const NkAssetId &id, NkTextureLibrary *texLib) noexcept {
			if (!id.IsValid())
				return NkTexHandle::Null();
			const NkAssetRecord *rec = NkAssetRegistry::Global().FindById(id);
			if (!rec) {
				logger.Warnf("[NkTextureAssetIO] LoadById : %s not in registry\n", id.ToString().CStr());
				return NkTexHandle::Null();
			}
			return Load(rec->diskPath, texLib);
		}

	} // namespace renderer
} // namespace nkentseu
