#pragma once
// =============================================================================
// NkMaterialLibrary.h  — NKRenderer Phase G
//
// Charge / cache / hot-reload des materiaux .nkasset.
//
// Architecture (pattern Hazel Engine) :
//
//   .nkasset (disque)
//     |
//     v   NkAssetIO::ReadMetadata + ReadPayload
//   NkMaterialAsset (struct serialisable)
//     |
//     v   NkMaterialLibrary::Load()
//   NkMaterialInstance* (cache par NkAssetId, ref-counted)
//     |
//     v   NkMatInstHandle pour NkDrawCall3D::material
//
// Convention chemin logique : "/Materials/<categorie>/<nom>".
// Convention disque : "<assetRoot>/Materials/<categorie>/<nom>.nkasset".
//
// Hot-reload :
//   NkFileWatcher surveille <assetRoot>/Materials/, callback OnFileChanged
//   declenche reload de l'asset modifie : la NkMaterialInstance existante
//   est patchee sur place (les NkMatInstHandle restent valides).
//
// Lazy resolution textures :
//   Les NkMaterialTextureRef.assetId sont resolus en NkTexHandle au moment
//   du Bind, pas au Load. Evite de tirer toutes les textures meme pour des
//   materiaux jamais utilises.
// =============================================================================
#include "NKRenderer/Materials/NkMaterialAsset.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKSerialization/Asset/NkAssetMetadata.h"
#include "NKContainers/Associative/NkHashMap.h"

// Forward NkFileWatcher (defined in NKFileSystem) pour eviter inclusion lourde
namespace nkentseu {
    class NkFileWatcher;
    class NkFileWatcherCallback;
}

namespace nkentseu {
    namespace renderer {

        class NkMaterialSystem;

        // =====================================================================
        // NkMaterialLibrary — Cache + scan + hot-reload de .nkasset Material
        // =====================================================================
        class NkMaterialLibrary {
            public:
                NkMaterialLibrary() = default;
                ~NkMaterialLibrary();

                // ── Cycle de vie ──────────────────────────────────────────────
                bool Init(NkIDevice* device,
                          NkMaterialSystem* materialSystem,
                          NkTextureLibrary* textureLibrary);
                void Shutdown();
                bool IsValid() const noexcept { return mDevice != nullptr; }

                // ── Scan & registry ───────────────────────────────────────────
                // Scanne recursivement le dossier (typiquement
                // "Resources/NKRenderer/Materials") et enregistre chaque
                // .nkasset Material dans NkAssetRegistry::Global() sans charger
                // les payloads. Retourne le nombre d'assets indexes.
                uint32 ScanDirectory(const NkString& assetRootDir);

                // ── Load ──────────────────────────────────────────────────────
                // Load par chemin logique : "/Materials/Metals/Gold".
                // Cache ref-counted : 2x meme chemin -> meme NkMatInstHandle,
                // pas d'allocation supplementaire (pattern Hazel).
                NkMatInstHandle Load(const NkString& logicalPath);

                // Load par ID asset (plus rapide, evite la lookup par path).
                NkMatInstHandle LoadById(const NkAssetId& id);

                // Force le rechargement depuis disque (utilise par hot-reload).
                // Si l'asset etait deja loade, son NkMaterialInstance existant
                // est patche sur place — le NkMatInstHandle reste valide.
                bool Reload(const NkAssetId& id);

                // ── Hot-reload ────────────────────────────────────────────────
                // Active NkFileWatcher sur le dossier scanne (events natifs
                // ReadDirectoryChangesW / inotify). No-op si IsValid()==false.
                void EnableHotReload(bool enable);

                // Polling fallback (a appeler chaque frame depuis le renderer
                // si on prefere mtime polling au NkFileWatcher native events).
                // Throttle interne 1Hz.
                void PollHotReload(float32 deltaTime);

                // ── Save ──────────────────────────────────────────────────────
                // Sauvegarde un NkMaterialAsset dans un .nkasset (pour tooling
                // ou editor). Genere automatiquement un NkAssetId si manquant.
                // outPath : chemin disque complet du .nkasset.
                bool Save(const NkMaterialAsset& asset,
                          const NkString& outPath,
                          NkAssetId* outGeneratedId = nullptr);

                // ── Introspection ─────────────────────────────────────────────
                uint32 CountLoaded() const noexcept { return (uint32)mLoaded.Size(); }
                const NkMaterialAsset* GetAsset(const NkAssetId& id) const;

            private:
                struct LoadedEntry {
                    NkAssetId        assetId;
                    NkString         diskPath;
                    NkMaterialAsset  asset;        // donnees serialisees
                    NkMatInstHandle  instHandle;   // instance NkMaterialSystem
                    uint64           mtime = 0;    // derniere mtime pour polling
                    uint32           refCount = 0; // futur ref-counting actif
                };

                NkIDevice*         mDevice    = nullptr;
                NkMaterialSystem*  mMatSys    = nullptr;
                NkTextureLibrary*  mTexLib    = nullptr;

                NkHashMap<uint64, LoadedEntry> mLoaded; // key = NkAssetId.lo XOR hi
                NkString                       mScanRoot;
                NkFileWatcher*                 mWatcher = nullptr;
                NkFileWatcherCallback*         mWatcherCb = nullptr;
                float32                        mPollAccum = 0.f;

                // Helpers
                uint64 KeyOf(const NkAssetId& id) const noexcept {
                    return id.lo ^ id.hi;
                }
                NkMatInstHandle CreateOrUpdateInstance(LoadedEntry& entry, bool isReload);
                bool ReadAssetFromDisk(const NkString& diskPath, NkMaterialAsset& out, NkAssetMetadata& outMeta);
        };

    } // namespace renderer
} // namespace nkentseu
