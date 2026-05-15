// =============================================================================
// NkMaterialLibrary.cpp  — NKRenderer Phase G
// =============================================================================
#include "NkMaterialLibrary.h"
#include "NkMaterialSystem.h"
#include "NKSerialization/Asset/NkAssetImporter.h"
#include "NKFileSystem/NkFileWatcher.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
    namespace renderer {

        NkMaterialLibrary::~NkMaterialLibrary() {
            if (mDevice) Shutdown();
        }

        // =====================================================================
        // Init / Shutdown
        // =====================================================================
        bool NkMaterialLibrary::Init(NkIDevice* device,
                                     NkMaterialSystem* materialSystem,
                                     NkTextureLibrary* textureLibrary) {
            if (!device || !materialSystem) {
                logger.Errorf("[NkMaterialLibrary] Init: device or materialSystem null\n");
                return false;
            }
            mDevice = device;
            mMatSys = materialSystem;
            mTexLib = textureLibrary;
            logger.Info("[NkMaterialLibrary] Initialized\n");
            return true;
        }

        void NkMaterialLibrary::Shutdown() {
            EnableHotReload(false);
            // Les NkMaterialInstance appartiennent au NkMaterialSystem qui les
            // libere a son propre Shutdown. On nettoie juste notre cache.
            mLoaded.Clear();
            mDevice = nullptr;
            mMatSys = nullptr;
            mTexLib = nullptr;
            logger.Info("[NkMaterialLibrary] Shutdown\n");
        }

        // =====================================================================
        // Scan : enregistre tous les .nkasset Material du dossier
        // =====================================================================
        uint32 NkMaterialLibrary::ScanDirectory(const NkString& assetRootDir) {
            if (!IsValid()) return 0;
            mScanRoot = assetRootDir;

            NkVector<NkString> files;
            files = NkDirectory::GetFiles(assetRootDir.CStr(),
                                          "*.nkasset",
                                          NkSearchOption::NK_ALL_DIRECTORIES);

            uint32 added = 0;
            for (nk_size i = 0; i < files.Size(); ++i) {
                NkString err;
                NkAssetMetadata meta;
                if (!NkAssetIO::ReadMetadata(files[i].CStr(), meta, &err)) {
                    logger.Warnf("[NkMaterialLibrary] skip '%s' : %s\n",
                                 files[i].CStr(), err.CStr());
                    continue;
                }
                if (meta.type != NkAssetType::Material) continue;

                NkAssetRecord rec;
                rec.id        = meta.id;
                rec.assetPath = meta.assetPath;
                rec.type      = meta.type;
                rec.typeName  = meta.typeName;
                rec.diskPath  = files[i];
                NkAssetRegistry::Global().Register(rec);
                ++added;
            }
            logger.Info("[NkMaterialLibrary] Scanned '{0}' : {1} Material assets indexed\n", assetRootDir, added);
            return added;
        }

        // =====================================================================
        // Load par chemin logique
        // =====================================================================
        NkMatInstHandle NkMaterialLibrary::Load(const NkString& logicalPath) {
            if (!IsValid()) return NkMatInstHandle::Null();
            const NkAssetRecord* rec = NkAssetRegistry::Global().FindByPath(logicalPath.View());
            if (!rec) {
                logger.Warnf("[NkMaterialLibrary] Load '%s' : not in registry\n",
                             logicalPath.CStr());
                return NkMatInstHandle::Null();
            }
            return LoadById(rec->id);
        }

        // =====================================================================
        // Load par ID (cache ref-counted)
        // =====================================================================
        NkMatInstHandle NkMaterialLibrary::LoadById(const NkAssetId& id) {
            if (!IsValid() || !id.IsValid()) return NkMatInstHandle::Null();

            // Cache hit : reutilise l'instance existante (ref count++).
            auto* existing = mLoaded.Find(KeyOf(id));
            if (existing) {
                existing->refCount++;
                return existing->instHandle;
            }

            // Cache miss : lookup record + read disk + deserialize.
            const NkAssetRecord* rec = NkAssetRegistry::Global().FindById(id);
            if (!rec) {
                logger.Warnf("[NkMaterialLibrary] LoadById : asset %s not in registry\n",
                             id.ToString().CStr());
                return NkMatInstHandle::Null();
            }

            LoadedEntry entry;
            entry.assetId  = id;
            entry.diskPath = rec->diskPath;
            entry.refCount = 1;

            NkAssetMetadata meta;
            if (!ReadAssetFromDisk(entry.diskPath, entry.asset, meta)) {
                return NkMatInstHandle::Null();
            }
            entry.mtime = meta.importTimestamp;

            entry.instHandle = CreateOrUpdateInstance(entry, /*isReload=*/false);
            if (!entry.instHandle.IsValid()) {
                logger.Errorf("[NkMaterialLibrary] LoadById %s : instance creation failed\n",
                              id.ToString().CStr());
                return NkMatInstHandle::Null();
            }

            mLoaded.Insert(KeyOf(id), std::move(entry));
            logger.Info("[NkMaterialLibrary] Loaded '{0}' (id={1})\n",
                        rec->assetPath.path, id.ToString());
            return mLoaded.Find(KeyOf(id))->instHandle;
        }

        // =====================================================================
        // Reload : patche l'instance existante
        // =====================================================================
        bool NkMaterialLibrary::Reload(const NkAssetId& id) {
            if (!IsValid() || !id.IsValid()) return false;
            auto* entry = mLoaded.Find(KeyOf(id));
            if (!entry) return false;

            NkAssetMetadata meta;
            if (!ReadAssetFromDisk(entry->diskPath, entry->asset, meta)) return false;
            entry->mtime = meta.importTimestamp;

            // Re-applique au NkMaterialInstance existant (handle preserve).
            CreateOrUpdateInstance(*entry, /*isReload=*/true);
            logger.Info("[NkMaterialLibrary] Reloaded '{0}'\n", entry->diskPath);
            return true;
        }

        // =====================================================================
        // Save : ecrit un NkMaterialAsset en .nkasset
        // =====================================================================
        bool NkMaterialLibrary::Save(const NkMaterialAsset& asset,
                                     const NkString& outPath,
                                     NkAssetId* outGeneratedId) {
            NkAssetMetadata meta;
            meta.id              = NkAssetId::Generate();
            meta.type            = NkAssetType::Material;
            meta.typeName        = NkString("Material");
            meta.assetPath       = NkAssetPath(NkString("/Materials/" + asset.name).View());
            meta.assetVersion    = 1u;
            meta.importTimestamp = 0u;
            meta.AddTag(NkAssetTypeName(meta.type));

            // Serialise le payload NkMaterialAsset en NkNative binaire
            NkArchive payloadArchive;
            asset.Serialize(payloadArchive);
            NkVector<nk_uint8> payload;
            if (!native::NkNativeWriter::WriteArchive(payloadArchive, payload)) {
                logger.Errorf("[NkMaterialLibrary] Save: failed to encode payload\n");
                return false;
            }

            NkString err;
            if (!NkAssetIO::Write(outPath.CStr(), meta,
                                  payload.Data(), payload.Size(), &err)) {
                logger.Errorf("[NkMaterialLibrary] Save: %s\n", err.CStr());
                return false;
            }
            if (outGeneratedId) *outGeneratedId = meta.id;
            return true;
        }

        // =====================================================================
        // Hot-reload via NkFileWatcher
        // =====================================================================
        // Callback dérivé qui forwarde vers NkMaterialLibrary::Reload.
        class MaterialLibraryWatcherCb : public NkFileWatcherCallback {
            public:
                NkMaterialLibrary* library = nullptr;
                void OnFileChanged(const NkFileChangeEvent& ev) override {
                    if (!library) return;
                    if (ev.Type != NkFileChangeType::NK_MODIFIED &&
                        ev.Type != NkFileChangeType::NK_CREATED) return;
                    // Filtre extension .nkasset
                    if (ev.Path.Size() < 8) return;
                    const NkString& p = ev.Path;
                    bool isAsset = (p.SubStr(p.Size() - 8u) == NkString(".nkasset"));
                    if (!isAsset) return;
                    NkAssetMetadata meta;
                    if (NkAssetIO::ReadMetadata(ev.Path.CStr(), meta)) {
                        library->Reload(meta.id);
                    }
                }
        };

        void NkMaterialLibrary::EnableHotReload(bool enable) {
            if (enable && !mWatcher && !mScanRoot.Empty()) {
                auto* cb = new MaterialLibraryWatcherCb();
                cb->library = this;
                mWatcherCb = cb;
                mWatcher = new NkFileWatcher();
                mWatcher->SetPath(mScanRoot.CStr());
                mWatcher->SetRecursive(true);
                mWatcher->SetCallback(mWatcherCb);
                mWatcher->Start();
                logger.Info("[NkMaterialLibrary] Hot-reload enabled on '{0}'\n", mScanRoot);
            } else if (!enable && mWatcher) {
                mWatcher->Stop();
                delete mWatcher;
                mWatcher = nullptr;
                delete mWatcherCb;
                mWatcherCb = nullptr;
                logger.Info("[NkMaterialLibrary] Hot-reload disabled\n");
            }
        }

        // =====================================================================
        // Poll fallback mtime (1Hz)
        // =====================================================================
        void NkMaterialLibrary::PollHotReload(float32 deltaTime) {
            mPollAccum += deltaTime;
            if (mPollAccum < 1.0f) return;
            mPollAccum = 0.f;

            for (auto& [key, entry] : mLoaded) {
                NkAssetMetadata meta;
                if (!NkAssetIO::ReadMetadata(entry.diskPath.CStr(), meta)) continue;
                if (meta.importTimestamp != entry.mtime) {
                    Reload(entry.assetId);
                }
            }
        }

        const NkMaterialAsset* NkMaterialLibrary::GetAsset(const NkAssetId& id) const {
            auto* e = mLoaded.Find(KeyOf(id));
            return e ? &e->asset : nullptr;
        }

        // =====================================================================
        // CreateOrUpdateInstance : transfert NkMaterialAsset -> NkMaterialInstance
        // =====================================================================
        NkMatInstHandle NkMaterialLibrary::CreateOrUpdateInstance(LoadedEntry& entry,
                                                                   bool isReload) {
            NkMatInstHandle handle = entry.instHandle;
            NkMaterialInstance* inst = nullptr;

            if (!isReload || !handle.IsValid()) {
                // Cree une nouvelle instance via NkMaterialSystem.
                // Trouve le template adequat selon type.
                NkMatHandle tmpl;
                switch (entry.asset.type) {
                    case NkMaterialType::NK_PBR_METALLIC:
                    case NkMaterialType::NK_PBR_SPECULAR:
                        tmpl = mMatSys->DefaultPBR();      break;
                    case NkMaterialType::NK_TOON:
                        tmpl = mMatSys->DefaultToon();     break;
                    case NkMaterialType::NK_UNLIT:
                        tmpl = mMatSys->DefaultUnlit();    break;
                    case NkMaterialType::NK_SKIN:
                        tmpl = mMatSys->DefaultSkin();     break;
                    case NkMaterialType::NK_HAIR:
                        tmpl = mMatSys->DefaultHair();     break;
                    case NkMaterialType::NK_ANIME:
                        tmpl = mMatSys->DefaultAnime();    break;
                    case NkMaterialType::NK_ARCHIVIZ:
                        tmpl = mMatSys->DefaultArchviz();  break;
                    case NkMaterialType::NK_REFL_FLOOR:
                        tmpl = mMatSys->DefaultReflFloor();break;
                    default:
                        tmpl = mMatSys->DefaultPBR();      break;
                }
                if (!tmpl.IsValid()) {
                    logger.Errorf("[NkMaterialLibrary] No template for type=%u\n",
                                  (uint32)entry.asset.type);
                    return NkMatInstHandle::Null();
                }
                inst = mMatSys->CreateInstance(tmpl);
                if (!inst) return NkMatInstHandle::Null();
                handle = inst->GetHandle();
            } else {
                inst = mMatSys->GetInstance(handle);
                if (!inst) return NkMatInstHandle::Null();
            }

            // Applique les parametres serialises a l'instance.
            // Pour PBR types : copie NkPBRParams direct via setters typiques.
            // Pour Toon/Anime : copie NkToonParams.
            const bool isToonLike = (entry.asset.type == NkMaterialType::NK_TOON ||
                                     entry.asset.type == NkMaterialType::NK_TOON_INK ||
                                     entry.asset.type == NkMaterialType::NK_ANIME);

            if (isToonLike) {
                const auto& t = entry.asset.toon;
                inst->SetAlbedo({t.albedoColor.x, t.albedoColor.y, t.albedoColor.z},
                                t.albedoColor.w);
                inst->SetToonThreshold(t.shadowThreshold);
                inst->SetToonSmooth(t.shadowSmooth);
                inst->SetToonShadowColor({t.shadowColor.x, t.shadowColor.y, t.shadowColor.z});
                inst->SetOutline(t.outlineWidth,
                                 {t.outlineColor.x, t.outlineColor.y, t.outlineColor.z});
                inst->SetRim(t.rimIntensity,
                             {t.rimColor.x, t.rimColor.y, t.rimColor.z});
                inst->SetSpecHardness(t.specHardness);
                inst->SetMatcapStrength(t.matcapStrength);
            } else {
                const auto& p = entry.asset.pbr;
                inst->SetAlbedo({p.albedo.x, p.albedo.y, p.albedo.z}, p.albedo.w);
                inst->SetMetallic(p.metallic);
                inst->SetRoughness(p.roughness);
                inst->SetEmissive({p.emissive.x, p.emissive.y, p.emissive.z},
                                  p.emissiveStrength);
                if (p.subsurface > 0.f) {
                    inst->SetSubsurface(p.subsurface,
                        {p.subsurfaceColor.x, p.subsurfaceColor.y, p.subsurfaceColor.z});
                }
                if (p.clearcoat > 0.f) {
                    inst->SetClearcoat(p.clearcoat, p.clearcoatRough);
                }
            }

            // TODO : Lazy resolution textures via mTexLib + NkAssetRegistry.
            // Pour Phase G v1, on resout les fallbackPath si presents (chemins
            // directs vers les fichiers texture sur disque). Le chemin asset
            // complet (resolution NkAssetId -> Texture2D .nkasset -> upload GPU)
            // viendra avec la generalisation NkTextureAsset Phase G+1.
            for (nk_size i = 0; i < entry.asset.textures.Size(); ++i) {
                const auto& ref = entry.asset.textures[i];
                if (!mTexLib) continue;
                NkTexHandle tex;
                if (!ref.fallbackPath.Empty()) {
                    tex = mTexLib->Load(ref.fallbackPath);
                }
                // TODO : sinon resolve ref.assetId via NkAssetRegistry
                if (tex.IsValid()) inst->SetTexture(ref.slot, tex);
            }

            return handle;
        }

        // =====================================================================
        // ReadAssetFromDisk : lit metadata + payload, deserialize NkMaterialAsset
        // =====================================================================
        bool NkMaterialLibrary::ReadAssetFromDisk(const NkString& diskPath,
                                                   NkMaterialAsset& out,
                                                   NkAssetMetadata& outMeta) {
            NkVector<nk_uint8> payload;
            NkString err;
            if (!NkAssetIO::ReadFull(diskPath.CStr(), outMeta, payload, &err)) {
                logger.Errorf("[NkMaterialLibrary] read '%s' failed : %s\n",
                              diskPath.CStr(), err.CStr());
                return false;
            }
            NkArchive payloadArchive;
            if (!native::NkNativeReader::ReadArchive(payload.Data(), payload.Size(),
                                                     payloadArchive, &err)) {
                logger.Errorf("[NkMaterialLibrary] decode payload '%s' failed : %s\n",
                              diskPath.CStr(), err.CStr());
                return false;
            }
            return out.Deserialize(payloadArchive);
        }

    } // namespace renderer
} // namespace nkentseu
