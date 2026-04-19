#include "AssetManager.h"
#include "NKLogger/NkLog.h"
#include "NKImage/NKImage.h"
#include "NKFileSystem/NkPath.h"
#include <cstring>

namespace nkentseu {
    namespace Unkeny {

        // =====================================================================
        NkAssetType AssetManager::DetectType(const char* path) noexcept {
            if (!path) return NkAssetType::Unknown;
            const char* ext = strrchr(path, '.');
            if (!ext) return NkAssetType::Unknown;
            ++ext;
            if (!strcmp(ext,"png")||!strcmp(ext,"jpg")||!strcmp(ext,"jpeg")||
                !strcmp(ext,"tga")||!strcmp(ext,"bmp")||!strcmp(ext,"hdr")||
                !strcmp(ext,"qoi")||!strcmp(ext,"webp"))  return NkAssetType::Texture;
            if (!strcmp(ext,"obj")||!strcmp(ext,"nkmesh")||
                !strcmp(ext,"fbx")||!strcmp(ext,"gltf"))  return NkAssetType::Mesh;
            if (!strcmp(ext,"glsl")||!strcmp(ext,"hlsl")||
                !strcmp(ext,"nksl")||!strcmp(ext,"metal")) return NkAssetType::Shader;
            if (!strcmp(ext,"ttf") ||!strcmp(ext,"otf")||
                !strcmp(ext,"woff"))                       return NkAssetType::Font;
            if (!strcmp(ext,"wav") ||!strcmp(ext,"ogg")||
                !strcmp(ext,"mp3"))                        return NkAssetType::Audio;
            if (!strcmp(ext,"nkscene"))                    return NkAssetType::Scene;
            if (!strcmp(ext,"nkmat"))                      return NkAssetType::Material;
            return NkAssetType::Unknown;
        }

        // =====================================================================
        void AssetManager::Init(NkIDevice* device, const char* dir) noexcept {
            mDevice     = device;
            mProjectDir = NkString(dir ? dir : ".");
            logger.Infof("[AssetManager] Init — projet: {}\n", mProjectDir.CStr());
        }

        void AssetManager::Shutdown() noexcept {
            // Détruire toutes les textures chargées
            if (mDevice) {
                for (auto& e : mEntries) {
                    if (e.type == NkAssetType::Texture && e.loaded && e.handle) {
                        NkTextureHandle h; h.id = e.handle;
                        mDevice->DestroyTexture(h);
                    }
                }
            }
            mEntries.Clear();
            mIndex.Clear();
            mThumbnails.Clear();
        }

        // =====================================================================
        NkTextureHandle AssetManager::LoadTexture(const char* path,
                                                   bool /*srgb*/) noexcept {
            NkAssetEntry* e = FindOrCreate(path, NkAssetType::Texture);
            if (!e) return {};

            if (e->loaded && !e->dirty) {
                NkTextureHandle h; h.id = e->handle;
                ++e->refCount;
                return h;
            }

            if (!LoadTextureImpl(*e)) {
                logger.Errorf("[AssetManager] Texture non chargée: {}\n", path);
                return {};
            }

            NkTextureHandle h; h.id = e->handle;
            return h;
        }

        bool AssetManager::LoadTextureImpl(NkAssetEntry& e) noexcept {
            if (!mDevice) return false;

            // Construire le chemin absolu
            NkString absPath = mProjectDir + "/" + e.path;

            // Charger via NKImage
            NkImage* img = NkImageCodec::Load(absPath.CStr());
            if (!img || !img->data || !img->width || !img->height) {
                logger.Errorf("[AssetManager] NkImage::Load échec: {}\n", absPath.CStr());
                if (img) img->Free();
                return false;
            }

            // Créer la texture GPU
            NkTextureDesc td = NkTextureDesc::Tex2D(
                (nk_uint32)img->width,
                (nk_uint32)img->height,
                (img->channels == 4) ? NkGPUFormat::NK_RGBA8_UNORM
                                     : NkGPUFormat::NK_RGB8_UNORM);
            td.bindFlags = NkBindFlags::NK_SHADER_RESOURCE;
            td.debugName = e.path.CStr();

            NkTextureHandle h = mDevice->CreateTexture(td);
            if (h.IsValid()) {
                mDevice->WriteTexture(h, img->data,
                    (nk_uint32)img->width * (nk_uint32)img->channels);
                e.handle  = h.id;
                e.loaded  = true;
                e.dirty   = false;
                ++e.refCount;
            }
            img->Free();
            return h.IsValid();
        }

        void AssetManager::ReleaseTexture(const char* path) noexcept {
            NkAssetEntry* e = Find(path);
            if (!e || e->refCount == 0) return;
            --e->refCount;
            if (e->refCount == 0 && mDevice && e->handle) {
                NkTextureHandle h; h.id = e->handle;
                mDevice->DestroyTexture(h);
                e->handle = 0;
                e->loaded = false;
            }
        }

        // =====================================================================
        nk_uint32 AssetManager::LoadFont(const char* /*path*/,
                                          float32 /*sizePx*/) noexcept {
            // TODO Phase 5 : charger via NKFont + uploader atlas GPU
            return 0;
        }

        // =====================================================================
        NkTextureHandle AssetManager::GetThumbnail(const char* path) noexcept {
            auto it = mThumbnails.Find(NkString(path));
            if (it != mThumbnails.End()) return it->second;
            // Générer (async Phase 5 — pour l'instant synchrone)
            GenerateThumbnail(path);
            it = mThumbnails.Find(NkString(path));
            if (it != mThumbnails.End()) return it->second;
            return {};
        }

        void AssetManager::GenerateThumbnail(const char* path) noexcept {
            NkAssetType t = DetectType(path);
            if (t == NkAssetType::Texture) {
                // La texture elle-même est son thumbnail
                NkTextureHandle h = LoadTexture(path);
                if (h.IsValid()) mThumbnails.Insert(NkString(path), h);
            }
            // Autres types : icônes génériques (Phase 5)
        }

        // =====================================================================
        bool AssetManager::IsLoaded(const char* path) const noexcept {
            const NkAssetEntry* e = Find(path);
            return e && e->loaded;
        }

        NkAssetType AssetManager::TypeOf(const char* path) const noexcept {
            const NkAssetEntry* e = Find(path);
            return e ? e->type : DetectType(path);
        }

        void AssetManager::ListAssets(NkAssetType type,
                                       NkVector<NkString>& out) const noexcept {
            for (const auto& e : mEntries)
                if (e.type == type) out.PushBack(e.path);
        }

        void AssetManager::ProcessHotReload() noexcept {
            for (auto& e : mEntries) {
                if (!e.dirty) continue;
                if (e.type == NkAssetType::Texture) LoadTextureImpl(e);
                // Autres types : Phase 5
            }
        }

        // =====================================================================
        NkAssetEntry* AssetManager::FindOrCreate(const char* path,
                                                  NkAssetType type) noexcept {
            NkString key(path);
            auto it = mIndex.Find(key);
            if (it != mIndex.End()) return &mEntries[it->second];

            NkAssetEntry e;
            e.path = key;
            e.type = (type == NkAssetType::Unknown) ? DetectType(path) : type;
            nk_uint32 idx = (nk_uint32)mEntries.Size();
            mEntries.PushBack(e);
            mIndex.Insert(key, idx);
            return &mEntries[idx];
        }

        NkAssetEntry* AssetManager::Find(const char* path) noexcept {
            auto it = mIndex.Find(NkString(path));
            return it != mIndex.End() ? &mEntries[it->second] : nullptr;
        }

        const NkAssetEntry* AssetManager::Find(const char* path) const noexcept {
            auto it = mIndex.Find(NkString(path));
            return it != mIndex.End() ? &mEntries[it->second] : nullptr;
        }

    } // namespace Unkeny
} // namespace nkentseu
