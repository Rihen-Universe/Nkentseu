#pragma once
// =============================================================================
// NkStreamingSystem.h  — NKRenderer v4.0  (Streaming/)
//
// Streaming de ressources GPU (textures, meshes) pour les mondes ouverts.
//
// Fonctionnement :
//   • Budget mémoire configurable (VRAM max à utiliser)
//   • Files de priorité par distance caméra
//   • Chargement asynchrone (thread worker dédié)
//   • Éviction LRU quand le budget est dépassé
//   • Mip streaming pour textures (résolution progressive)
//   • LOD streaming pour meshes (swipe automatique entre LOD0/1/2/3)
//
// Usage :
//   sys.Init(device, texLib, meshSys, cfg);
//   sys.SetCameraPosition(camPos);
//   sys.Request(texHandle, priority);
//   sys.Update(deltaTime);   // chaque frame — lance/termine async jobs
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKRHI/Core/NkIDevice.h"

namespace nkentseu {
namespace renderer {

    class NkTextureLibrary;
    class NkMeshSystem;

    // =========================================================================
    // Configuration du streaming
    // =========================================================================
    struct NkStreamingConfig {
        uint64  budgetBytes        = 512ULL * 1024 * 1024;  // 512 MiB VRAM
        uint32  maxJobsPerFrame    = 4;     // uploads async par frame
        uint32  mipBias            = 0;     // mip de départ (0=full, 1=demi-res…)
        float32 distanceMult       = 1.f;   // scale sur les distances de streaming
        float32 streamInDist       = 300.f; // distance au-delà de laquelle on streame
        float32 streamOutDist      = 400.f; // distance d'éviction
        bool    enableMipStreaming  = true;
        bool    enableMeshStreaming = true;
        bool    async              = true;  // désactiver = tout synchrone (debug)
    };

    // =========================================================================
    // État d'une ressource streamée
    // =========================================================================
    enum class NkStreamState : uint8 {
        NK_UNLOADED   = 0,  // pas en mémoire GPU
        NK_PENDING    = 1,  // dans la file de chargement
        NK_LOADING    = 2,  // job async en cours
        NK_RESIDENT   = 3,  // en mémoire GPU, prêt à l'emploi
        NK_EVICTING   = 4,  // en cours d'éviction
    };

    // =========================================================================
    // Descripteur d'une ressource streamable
    // =========================================================================
    struct NkStreamEntry {
        uint64       id          = 0;
        NkString     sourcePath;          // chemin disque ou virtuel
        NkStreamState state      = NkStreamState::NK_UNLOADED;
        float32       priority   = 0.f;   // calculé depuis la distance
        float32       lastUsed   = 0.f;   // timestamp (LRU)
        uint64        sizeBytes  = 0;
        bool          isMesh     = false; // texture sinon
        uint32        residentMip= 0;     // mip le plus fin actuellement chargé
        uint32        targetMip  = 0;     // mip voulu selon la distance
    };

    // =========================================================================
    // Callbacks de streaming
    // =========================================================================
    using NkStreamCallback = NkFunction<void(uint64 id, NkStreamState newState)>;

    // =========================================================================
    // NkStreamingSystem
    // =========================================================================
    class NkStreamingSystem {
    public:
        NkStreamingSystem()  = default;
        ~NkStreamingSystem();

        // ── Cycle de vie ──────────────────────────────────────────────────────
        bool Init(NkIDevice*        device,
                  NkTextureLibrary* texLib,
                  NkMeshSystem*     meshSys,
                  const NkStreamingConfig& cfg = {});
        void Shutdown();

        // ── Enregistrement ────────────────────────────────────────────────────
        void RegisterTexture(uint64 id, const NkString& path,
                             uint64 sizeEstimateBytes = 0);
        void RegisterMesh   (uint64 id, const NkString& path,
                             uint64 sizeEstimateBytes = 0);
        void Unregister     (uint64 id);

        // ── Requêtes de visibilité (appelé depuis le culling ou la scène) ─────
        void Request(uint64 id, float32 distFromCamera);
        void Release(uint64 id);   // signale que l'objet n'est plus visible

        // ── Mise à jour par frame ─────────────────────────────────────────────
        void SetCameraPosition(NkVec3f pos);
        void Update(float32 deltaTimeSec);

        // Callbacks
        void SetStateCallback(NkStreamCallback cb);

        // ── Accès ─────────────────────────────────────────────────────────────
        NkStreamState GetState(uint64 id) const;
        bool          IsResident(uint64 id) const;
        uint64        GetUsedBytes()  const { return mUsedBytes; }
        uint64        GetBudgetBytes()const { return mCfg.budgetBytes; }
        float32       GetUsageRatio() const;

        // Stats
        uint32  GetPendingCount()  const;
        uint32  GetLoadingCount()  const;
        uint32  GetResidentCount() const;
        uint32  GetEvictedCount()  const { return mEvictCount; }

    private:
        NkIDevice*        mDevice  = nullptr;
        NkTextureLibrary* mTexLib  = nullptr;
        NkMeshSystem*     mMesh    = nullptr;
        NkStreamingConfig mCfg;
        NkVec3f           mCamPos  = {};
        bool              mReady   = false;

        NkHashMap<uint64, NkStreamEntry> mEntries;
        NkVector<uint64>                 mPendingQueue; // sorted by priority
        NkVector<uint64>                 mLoadingQueue;

        NkStreamCallback  mCallback;
        uint64            mUsedBytes   = 0;
        mutable uint32    mEvictCount  = 0;

        void  TickQueue(float32 dt);
        void  StartLoadJob(uint64 id);
        void  FinalizeLoad(uint64 id);
        void  Evict(uint64 id);
        void  EvictLRU();
        void  SortPendingQueue();
        float32 ComputePriority(const NkStreamEntry& e, NkVec3f cam) const;
    };

} // namespace renderer
} // namespace nkentseu
