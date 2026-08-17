#include "AssetManager.h"
#include "NKLogger/NkLog.h"
#include "NKImage/NKImage.h"
#include "NKFileSystem/NkPath.h"
#include "NKRHI/Core/NkDescs.h"
#include <cstring>

namespace nkentseu {
	namespace noge {

		// =====================================================================
		NkAssetType AssetManager::DetectType(const char *path) noexcept {
			if (!path)
				return NkAssetType::Unknown;
			const char *ext = strrchr(path, '.');
			if (!ext)
				return NkAssetType::Unknown;
			++ext;
			if (!strcmp(ext, "png") || !strcmp(ext, "jpg") || !strcmp(ext, "jpeg") || !strcmp(ext, "tga") ||
				!strcmp(ext, "bmp") || !strcmp(ext, "hdr") || !strcmp(ext, "qoi") || !strcmp(ext, "webp"))
				return NkAssetType::Texture;
			if (!strcmp(ext, "obj") || !strcmp(ext, "nkmesh") || !strcmp(ext, "fbx") || !strcmp(ext, "gltf"))
				return NkAssetType::Mesh;
			if (!strcmp(ext, "glsl") || !strcmp(ext, "hlsl") || !strcmp(ext, "nksl") || !strcmp(ext, "metal"))
				return NkAssetType::Shader;
			if (!strcmp(ext, "ttf") || !strcmp(ext, "otf") || !strcmp(ext, "woff"))
				return NkAssetType::Font;
			if (!strcmp(ext, "wav") || !strcmp(ext, "ogg") || !strcmp(ext, "mp3"))
				return NkAssetType::Audio;
			if (!strcmp(ext, "nkscene"))
				return NkAssetType::Scene;
			if (!strcmp(ext, "nkmat"))
				return NkAssetType::Material;
			return NkAssetType::Unknown;
		}

		// =====================================================================
		void AssetManager::Init(NkIDevice *device, const char *dir) noexcept {
			mDevice = device;
			mProjectDir = NkString(dir ? dir : ".");
			logger.Infof("[AssetManager] Init — projet: {}\n", mProjectDir.CStr());
		}

		void AssetManager::Shutdown() noexcept {
			// Détruire toutes les textures chargées
			if (mDevice) {
				for (auto &e : mEntries) {
					if (e.type == NkAssetType::Texture && e.loaded && e.handle) {
						NkTextureHandle h;
						h.id = e.handle;
						mDevice->DestroyTexture(h);
					}
				}
			}
			mEntries.Clear();
			mIndex.Clear();
			mThumbnails.Clear();
			// Le second appel (destructeur statique, a l'atexit) ne doit RIEN
			// toucher : a ce moment le device est detruit et ce pointeur
			// pendouillerait. On le rend inoffensif ici, pas chez l'appelant.
			mDevice = nullptr;
		}

		// =====================================================================
		NkTextureHandle AssetManager::LoadTexture(const char *path, bool /*srgb*/) noexcept {
			NkAssetEntry *e = FindOrCreate(path, NkAssetType::Texture);
			if (!e)
				return {};

			if (e->loaded && !e->dirty) {
				NkTextureHandle h;
				h.id = e->handle;
				++e->refCount;
				return h;
			}

			if (!LoadTextureImpl(*e)) {
				logger.Errorf("[AssetManager] Texture non chargée: {}\n", path);
				return {};
			}

			NkTextureHandle h;
			h.id = e->handle;
			return h;
		}

		bool AssetManager::LoadTextureImpl(NkAssetEntry &e) noexcept {
			if (!mDevice)
				return false;

			// Construire le chemin absolu
			NkString absPath = mProjectDir + "/" + e.path;

			// Charger via NKImage (canaux forcés à 4 → NK_RGBA8_UNORM)
			NkImage img;
			if (!img.Load(absPath.CStr(), 4) || !img.Pixels() || img.Width() <= 0 || img.Height() <= 0) {
				logger.Errorf("[AssetManager] NkImage::Load échec: {}\n", absPath.CStr());
				return false;
			}

			// Créer la texture GPU
			NkTextureDesc td =
				NkTextureDesc::Tex2D((nk_uint32)img.Width(), (nk_uint32)img.Height(), NkGPUFormat::NK_RGBA8_UNORM);
			td.bindFlags = NkBindFlags::NK_SHADER_RESOURCE;
			td.debugName = e.path.CStr();

			NkTextureHandle h = mDevice->CreateTexture(td);
			if (h.IsValid()) {
				mDevice->WriteTexture(h, img.Pixels(), (nk_uint32)img.Width() * 4u);
				e.handle = h.id;
				e.loaded = true;
				e.dirty = false;
				++e.refCount;
			}
			return h.IsValid();
		}

		void AssetManager::ReleaseTexture(const char *path) noexcept {
			NkAssetEntry *e = Find(path);
			if (!e || e->refCount == 0)
				return;
			--e->refCount;
			if (e->refCount == 0 && mDevice && e->handle) {
				NkTextureHandle h;
				h.id = e->handle;
				mDevice->DestroyTexture(h);
				e->handle = 0;
				e->loaded = false;
			}
		}

		// =====================================================================
		nk_uint32 AssetManager::LoadFont(const char * /*path*/, float32 /*sizePx*/) noexcept {
			// TODO Phase 5 : charger via NKFont + uploader atlas GPU
			return 0;
		}

		// =====================================================================
		NkTextureHandle AssetManager::GetThumbnail(const char *path) noexcept {
			// NkHashMap::Find retourne un pointeur vers la valeur (nullptr si absent)
			NkTextureHandle *th = mThumbnails.Find(NkString(path));
			if (th)
				return *th;
			// Générer (async Phase 5 — pour l'instant synchrone)
			GenerateThumbnail(path);
			th = mThumbnails.Find(NkString(path));
			if (th)
				return *th;
			return {};
		}

		void AssetManager::GenerateThumbnail(const char *path) noexcept {
			NkAssetType t = DetectType(path);
			if (t == NkAssetType::Texture) {
				// La texture elle-même est son thumbnail
				NkTextureHandle h = LoadTexture(path);
				if (h.IsValid())
					mThumbnails.Insert(NkString(path), h);
			}
			// Autres types : icônes génériques (Phase 5)
		}

		// =====================================================================
		bool AssetManager::IsLoaded(const char *path) const noexcept {
			const NkAssetEntry *e = Find(path);
			return e && e->loaded;
		}

		NkAssetType AssetManager::TypeOf(const char *path) const noexcept {
			const NkAssetEntry *e = Find(path);
			return e ? e->type : DetectType(path);
		}

		void AssetManager::ListAssets(NkAssetType type, NkVector<NkString> &out) const noexcept {
			for (const auto &e : mEntries)
				if (e.type == type)
					out.PushBack(e.path);
		}

		void AssetManager::ProcessHotReload() noexcept {
			for (auto &e : mEntries) {
				if (!e.dirty)
					continue;
				if (e.type == NkAssetType::Texture)
					LoadTextureImpl(e);
				// Autres types : Phase 5
			}
		}

		// =====================================================================
		NkAssetEntry *AssetManager::FindOrCreate(const char *path, NkAssetType type) noexcept {
			NkString key(path);
			if (nk_uint32 *idxPtr = mIndex.Find(key))
				return &mEntries[*idxPtr];

			NkAssetEntry e;
			e.path = key;
			e.type = (type == NkAssetType::Unknown) ? DetectType(path) : type;
			nk_uint32 idx = (nk_uint32)mEntries.Size();
			mEntries.PushBack(e);
			mIndex.Insert(key, idx);
			return &mEntries[idx];
		}

		NkAssetEntry *AssetManager::Find(const char *path) noexcept {
			nk_uint32 *idxPtr = mIndex.Find(NkString(path));
			return idxPtr ? &mEntries[*idxPtr] : nullptr;
		}

		const NkAssetEntry *AssetManager::Find(const char *path) const noexcept {
			const nk_uint32 *idxPtr = mIndex.Find(NkString(path));
			return idxPtr ? &mEntries[*idxPtr] : nullptr;
		}

	} // namespace noge
} // namespace nkentseu
