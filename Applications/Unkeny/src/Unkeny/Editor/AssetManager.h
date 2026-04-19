#pragma once
// =============================================================================
// Unkeny/Editor/AssetManager.h
// =============================================================================
// Cache centralisé des assets chargés en mémoire GPU/CPU.
// Identifie chaque asset par son chemin relatif (clé).
// Retourne des handles opaques typés compatibles NkRHI.
//
// Types d'assets gérés :
//   Texture  (NkTextureHandle)   — PNG, JPG, TGA, HDR, etc.
//   Mesh     (NkMeshHandle)      — .obj, .nkmesh, etc. (Phase 5+)
//   Shader   (NkShaderHandle)    — .glsl, .hlsl, .nksl
//   Font     (NkFontHandle)      — .ttf, .otf, .woff
//   Audio    (NkAudioHandle)     — .wav, .ogg (Phase 5+)
//   Scene    (path string only)  — .nkscene
//
// Hot-reload :
//   Watch() enregistre un watcher sur le fichier. Si le fichier change,
//   l'asset est rechargé automatiquement au prochain frame (flag dirty).
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkHandles.h"

namespace nkentseu {
    namespace Unkeny {

        // =====================================================================
        // NkAssetType
        // =====================================================================
        enum class NkAssetType : nk_uint8 {
            Unknown = 0,
            Texture,
            Mesh,
            Shader,
            Font,
            Audio,
            Scene,
            Material,
        };

        // =====================================================================
        // NkAssetEntry — entrée dans le cache
        // =====================================================================
        struct NkAssetEntry {
            NkString      path;
            NkAssetType   type       = NkAssetType::Unknown;
            nk_uint64     handle     = 0;   // handle opaque (cast selon le type)
            nk_uint32     refCount   = 0;
            bool          loaded     = false;
            bool          dirty      = false;  // hot-reload pending
        };

        // =====================================================================
        // AssetManager
        // =====================================================================
        class AssetManager {
        public:
            AssetManager() = default;
            ~AssetManager() noexcept { Shutdown(); }

            void Init(NkIDevice* device, const char* projectDir) noexcept;
            void Shutdown() noexcept;

            // ── Textures ──────────────────────────────────────────────────────
            // Charge et met en cache. Deuxième appel avec le même chemin retourne
            // le handle déjà chargé. srgb=true pour les albedos, false pour les masks.
            NkTextureHandle LoadTexture(const char* path, bool srgb = true) noexcept;
            void            ReleaseTexture(const char* path) noexcept;

            // ── Polices ───────────────────────────────────────────────────────
            // Retourne un identifiant uint32 (index dans NkUIFontManager).
            nk_uint32 LoadFont(const char* path, float32 sizePx = 16.f) noexcept;

            // ── Requêtes ──────────────────────────────────────────────────────
            [[nodiscard]] bool      IsLoaded(const char* path) const noexcept;
            [[nodiscard]] NkAssetType TypeOf(const char* path) const noexcept;

            // Liste tous les assets d'un type
            void ListAssets(NkAssetType type,
                            NkVector<NkString>& out) const noexcept;

            // ── Type detection ────────────────────────────────────────────────
            static NkAssetType DetectType(const char* path) noexcept;

            // ── Hot-reload ────────────────────────────────────────────────────
            // Appelé depuis EditorLayer::OnUpdate() — recharge les assets dirty.
            void ProcessHotReload() noexcept;

            // ── Thumbnail ─────────────────────────────────────────────────────
            // Retourne une texture de preview pour l'AssetBrowser.
            // Si pas disponible, retourne un handle invalide.
            NkTextureHandle GetThumbnail(const char* path) noexcept;

            // ── Accès brut ────────────────────────────────────────────────────
            const NkVector<NkAssetEntry>& Entries() const noexcept { return mEntries; }

        private:
            NkIDevice*            mDevice     = nullptr;
            NkString              mProjectDir;
            NkVector<NkAssetEntry> mEntries;

            // Cache chemin → index dans mEntries
            NkHashMap<NkString, nk_uint32> mIndex;

            // Thumbnails générées (chemin source → NkTextureHandle)
            NkHashMap<NkString, NkTextureHandle> mThumbnails;

            NkAssetEntry* FindOrCreate(const char* path, NkAssetType type) noexcept;
            NkAssetEntry* Find(const char* path) noexcept;
            const NkAssetEntry* Find(const char* path) const noexcept;

            bool LoadTextureImpl(NkAssetEntry& e) noexcept;
            void GenerateThumbnail(const char* path) noexcept;
        };

    } // namespace Unkeny
} // namespace nkentseu
