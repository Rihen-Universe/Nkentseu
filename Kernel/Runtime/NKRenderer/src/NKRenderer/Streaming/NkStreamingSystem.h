#pragma once
// =============================================================================
// NkStreamingSystem.h  — NKRenderer v5.0  (Streaming/)
//
// Streaming de ressources GPU (textures, meshes) pour les mondes ouverts.
// V1 REELLE (2026-07-13) — plus un simulateur :
//   • VRAIES E/S : disque + decodage CPU (NkImage / loaders mesh from-scratch)
//     sur un THREAD WORKER dedie ; l'upload GPU se fait sur le thread de rendu
//     au Update() (contrainte contexte GL + pattern NKRHI).
//   • Budget memoire configurable + eviction LRU (Release des VRAIES textures/
//     meshes via NkTextureLibrary/NkMeshSystem).
//   • Priorite par DISTANCE camera (fournie par Request), file triee.
//   • Mode synchrone (cfg.async=false) pour le debug/les tests.
//   • PAS ENCORE en v1 : mip streaming progressif (residentMip/targetMip
//     reserves), LOD meshes, ecriture differee — cf. ROADMAP.
//
// Usage :
//   sys.Init(device, texLib, meshSys, cfg);
//   sys.RegisterTexture(1, "Resources/.../albedo.png");
//   sys.Request(1, distanceCamera);       // chaque frame ou a la visibilite
//   sys.Update(dt);                       // draine les jobs, uploads, eviction
//   if (sys.IsResident(1)) use(sys.GetTexture(1));
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKThreading/NkThread.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkConditionVariable.h"

namespace nkentseu {
	class NkImage;

	namespace renderer {

		class NkTextureLibrary;
		class NkMeshSystem;
		struct NkGLTFMeshData;

		// =========================================================================
		// Configuration du streaming
		// =========================================================================
		struct NkStreamingConfig {
				uint64 budgetBytes = 512ULL * 1024 * 1024; // 512 MiB VRAM
				uint32 maxJobsPerFrame = 4;				   // uploads GPU par frame
				uint32 mipBias = 0;						   // reserve (mip streaming futur)
				float32 distanceMult = 1.f;				   // scale sur les distances de streaming
				float32 streamInDist = 300.f;			   // distance au-dela de laquelle on streame
				float32 streamOutDist = 400.f;			   // distance d'eviction
				bool enableMipStreaming = true;			   // reserve
				bool enableMeshStreaming = true;
				bool async = true; // false = tout synchrone (debug/tests)
		};

		// =========================================================================
		// Etat d'une ressource streamee
		// =========================================================================
		enum class NkStreamState : uint8 {
			NK_UNLOADED = 0, // pas en memoire GPU
			NK_PENDING = 1,	 // dans la file de chargement
			NK_LOADING = 2,	 // job async en cours (disque + decode CPU)
			NK_RESIDENT = 3, // en memoire GPU, pret a l'emploi
			NK_EVICTING = 4, // en cours d'eviction
		};

		// =========================================================================
		// Descripteur d'une ressource streamable
		// =========================================================================
		struct NkStreamEntry {
				uint64 id = 0;
				NkString sourcePath; // chemin disque
				NkStreamState state = NkStreamState::NK_UNLOADED;
				float32 priority = 0.f;	 // calcule depuis la distance (Request)
				float32 dist = 1e9f;	 // derniere distance camera connue
				float32 lastUsed = 0.f;	 // age en secondes (LRU)
				uint64 sizeBytes = 0;	 // estimation avant load, REEL apres
				bool isMesh = false;	 // texture sinon
				uint32 residentMip = 0;	 // reserve (mip streaming futur)
				uint32 targetMip = 0;	 // reserve
				NkTexHandle tex;		 // handle REEL une fois resident (texture)
				NkMeshHandle mesh;		 // handle REEL une fois resident (mesh)
				bool loadFailed = false; // vrai si la derniere tentative a echoue
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
				NkStreamingSystem() = default;
				~NkStreamingSystem();

				// ── Cycle de vie ──────────────────────────────────────────────────────
				bool Init(NkIDevice *device, NkTextureLibrary *texLib, NkMeshSystem *meshSys,
						  const NkStreamingConfig &cfg = {});
				void Shutdown();

				// ── Enregistrement ────────────────────────────────────────────────────
				void RegisterTexture(uint64 id, const NkString &path, uint64 sizeEstimateBytes = 0);
				void RegisterMesh(uint64 id, const NkString &path, uint64 sizeEstimateBytes = 0);
				void Unregister(uint64 id);

				// ── Requetes de visibilite (appele depuis le culling ou la scene) ─────
				void Request(uint64 id, float32 distFromCamera);
				void Release(uint64 id); // signale que l'objet n'est plus visible

				// ── Mise a jour par frame ─────────────────────────────────────────────
				void SetCameraPosition(NkVec3f pos);
				void Update(float32 deltaTimeSec);

				// Callbacks
				void SetStateCallback(NkStreamCallback cb);

				// ── Acces ─────────────────────────────────────────────────────────────
				NkStreamState GetState(uint64 id) const;
				bool IsResident(uint64 id) const;

				// Handles REELS (invalides tant que non resident).
				NkTexHandle GetTexture(uint64 id) const;
				NkMeshHandle GetMesh(uint64 id) const;

				uint64 GetUsedBytes() const {
					return mUsedBytes;
				}

				uint64 GetBudgetBytes() const {
					return mCfg.budgetBytes;
				}

				float32 GetUsageRatio() const;

				// Stats
				uint32 GetPendingCount() const;
				uint32 GetLoadingCount() const;
				uint32 GetResidentCount() const;

				uint32 GetEvictedCount() const {
					return mEvictCount;
				}

				uint32 GetFailedCount() const {
					return mFailCount;
				}

			private:
				// Job d'entree du worker : copie de ce qu'il faut pour charger SANS
				// toucher mEntries (le worker ne partage AUCUNE structure du main
				// thread hors des deux files protegees).
				struct LoadJob {
						uint64 id = 0;
						NkString path;
						bool isMesh = false;
				};

				// Resultat du worker : payload CPU decode, upload GPU fait au Update.
				struct LoadResult {
						uint64 id = 0;
						bool isMesh = false;
						bool ok = false;
						NkImage *img = nullptr;			 // texture decodee (RGBA8)
						NkGLTFMeshData *meshData = nullptr; // mesh parse
				};

				NkIDevice *mDevice = nullptr;
				NkTextureLibrary *mTexLib = nullptr;
				NkMeshSystem *mMesh = nullptr;
				NkStreamingConfig mCfg;
				NkVec3f mCamPos = {};
				bool mReady = false;

				NkHashMap<uint64, NkStreamEntry> mEntries;
				NkVector<uint64> mPendingQueue; // triee par priorite
				NkVector<uint64> mLoadingQueue;

				NkStreamCallback mCallback;
				uint64 mUsedBytes = 0;
				mutable uint32 mEvictCount = 0;
				uint32 mFailCount = 0;

				// ── Worker (disque + decode CPU) ─────────────────────────────────
				threading::NkThread mWorker;
				threading::NkMutex mJobMutex;
				threading::NkConditionVariable mJobCv;
				NkVector<LoadJob> mJobsIn;		 // protegee par mJobMutex
				NkVector<LoadResult> mResults;	 // protegee par mJobMutex
				bool mStop = false;				 // protegee par mJobMutex

				void WorkerLoop();
				static LoadResult DoLoadCPU(const LoadJob &job); // pur CPU, thread-safe
				void FreePayload(LoadResult &r);

				void TickQueue(float32 dt);
				void StartLoadJob(uint64 id);
				void FinalizeLoad(LoadResult &r); // upload GPU (thread de rendu)
				void Evict(uint64 id);
				void EvictLRU();
				void SortPendingQueue();
				float32 ComputePriority(const NkStreamEntry &e, NkVec3f cam) const;
		};

	} // namespace renderer
} // namespace nkentseu
