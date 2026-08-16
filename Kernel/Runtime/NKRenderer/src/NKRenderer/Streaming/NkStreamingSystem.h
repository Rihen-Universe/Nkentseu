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
// NkImage est un TYPE VALEUR (non copiable, deplacable) : les payloads CPU sont
// des membres par valeur, il faut donc le type complet ici.
#include "NKImage/Core/NkImage.h"

namespace nkentseu {

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
				// V2 mip streaming PROGRESSIF : une version basse resolution
				// (lowResMax px sur le grand cote) est uploadee EN PREMIER
				// (quasi instantane -> texture floue), puis la pleine resolution
				// remplace quand la camera est a moins de refineDistMult *
				// streamInDist. GetTexture() retourne toujours le meilleur
				// disponible ; surveiller le handle pour re-binder au raffinement.
				bool enableMipStreaming = true;
				uint32 lowResMax = 128;			// grand cote de la version floue
				float32 refineDistMult = 0.6f;	// refineDist = streamInDist * mult
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
				uint32 residentMip = 0;	 // 0 = pleine resolution, 1 = basse (floue)
				uint32 targetMip = 0;	 // vise selon la distance (0 pres, 1 loin)
				NkTexHandle tex;		 // handle REEL une fois resident (texture)
				NkMeshHandle mesh;		 // handle REEL une fois resident (mesh)
				bool loadFailed = false; // vrai si la derniere tentative a echoue
				// V2 mip streaming : version basse resolution GARDEE en RAM
				// (quelques dizaines de Ko) -> la RETROGRADATION loin est
				// instantanee ; la remontee en pleine res re-decode (worker).
				//
				// ⚠ L'IMAGE n'est PAS stockee ici mais dans
				// NkStreamingSystem::mLowPayloads, et l'entry n'en garde qu'un
				// INDICE de slot (POD, copiable). Raison : NkHashMap::Insert
				// prend son argument par `const Value &` et le COPIE dans le
				// noeud ; or NkImage est non copiable depuis la migration
				// valeur. Un `NkImage lowPayload;` ici rendrait NkStreamEntry
				// non copiable et RegisterTexture/RegisterMesh ne compileraient
				// plus. La propriete des pixels appartient donc au systeme
				// (destruction automatique du NkImage du slot), pas a un
				// pointeur nu : il n'y a plus rien a liberer a la main.
				int32 lowSlot = -1;			 // -1 = aucune basse res gardee
				bool refineInFlight = false; // re-decode pleine res en cours
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

				// Config ajustable au RUNTIME (ex. curseur de qualite / debug) :
				// distances de stream-in/out, budget, jobs par frame. Prise en
				// compte au prochain Update.
				NkStreamingConfig &GetConfig() {
					return mCfg;
				}

				const NkStreamingConfig &GetConfig() const {
					return mCfg;
				}

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
						bool fullOnly = false; // refine : re-decode pleine res seule
				};

				// Resultat du worker : payload CPU decode, upload GPU fait au Update.
				// ⚠ NON COPIABLE (NkImage par valeur) et c'est voulu : un
				// LoadResult ne se DUPLIQUE pas, il se DEPLACE. Il nait sur le
				// worker, entre dans mResults par move SOUS mJobMutex, et en
				// ressort par move sous le MEME mutex cote thread de rendu. Le
				// compilateur refuse tout autre transfert de propriete.
				struct LoadResult {
						uint64 id = 0;
						bool isMesh = false;
						bool ok = false;
						bool fullOnly = false; // resultat d'un job de refine
						NkImage img;						// texture PLEINE resolution (RGBA8), possedee
						NkImage imgLow;						// version basse resolution (mip streaming), possedee
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

				// ── Basse resolution CPU gardee en RAM (mip streaming) ───────────
				// Pool de slots : l'entry ne porte qu'un indice (cf. le
				// commentaire de NkStreamEntry::lowSlot). Les NkImage sont
				// POSSEDEES par ce vecteur et se detruisent avec lui — plus rien
				// a liberer a la main, plus de Free().
				// ⚠ THREAD : uniquement depuis le thread de rendu (FinalizeLoad /
				// TickRefines / Evict / Unregister / Shutdown). Le worker n'y
				// touche JAMAIS — il ne connait que mJobsIn / mResults.
				NkVector<NkImage> mLowPayloads;
				NkVector<int32> mFreeLowSlots; // slots vides recyclables

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
				LoadResult DoLoadCPU(const LoadJob &job) const; // pur CPU, thread-safe
				void FreePayload(LoadResult &r);

				// ── Basse res CPU : propriete par valeur, adressee par slot ──────
				// (thread de rendu uniquement — voir mLowPayloads)
				int32 AcquireLowSlot();								   // slot libre (recycle ou neuf)
				void StoreLowPayload(NkStreamEntry &e, NkImage &&img);  // MOVE vers le slot
				void ReleaseLowPayload(NkStreamEntry &e);			   // vide le slot et le recycle
				const NkImage *GetLowPayload(const NkStreamEntry &e) const; // observateur (ne possede rien)
				void TickRefines(uint32 &budgetJobs); // upgrades/downgrades de mip par distance

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
