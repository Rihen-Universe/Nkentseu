// =============================================================================
// NkStreamingSystem.cpp  — NKRenderer v5.0
// V1 REELLE : disque + decode CPU sur worker thread, upload GPU au Update()
// (thread de rendu), eviction LRU de vraies ressources. Cf. header.
// =============================================================================
#include "NkStreamingSystem.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKRenderer/Mesh/NkOBJLoader.h"
#include "NKImage/NKImage.h"
#include "NKMemory/NkAllocator.h"
#include "NKCore/NkTraits.h" // traits::NkMove (zero-STL : jamais std::move)
#include "NKLogger/NkLog.h"
#include "NKThreading/NkScopedLock.h"
#include <cmath>

namespace nkentseu {
	namespace renderer {

		NkStreamingSystem::~NkStreamingSystem() {
			Shutdown();
		}

		bool NkStreamingSystem::Init(NkIDevice *device, NkTextureLibrary *texLib, NkMeshSystem *meshSys,
									 const NkStreamingConfig &cfg) {
			mDevice = device;
			mTexLib = texLib;
			mMesh = meshSys;
			mCfg = cfg;
			mStop = false;
			if (mCfg.async) {
				// Worker unique : boucle disque + decode CPU. L'upload GPU reste
				// sur le thread de rendu (contrainte contexte GL + pattern NKRHI).
				mWorker.Start([this](void *) { WorkerLoop(); });
			}
			mReady = true;
			return true;
		}

		void NkStreamingSystem::Shutdown() {
			if (!mReady)
				return;
			// Arret du worker : flag + reveil + join.
			{
				threading::NkScopedLockMutex lock(mJobMutex);
				mStop = true;
			}
			mJobCv.NotifyOne();
			if (mWorker.Joinable())
				mWorker.Join();
			// Libere les payloads CPU orphelins (jobs finis jamais consommes).
			for (auto &r : mResults)
				FreePayload(r);
			mResults.Clear();
			mJobsIn.Clear();
			// Basse res CPU gardees par les entries : les NkImage se detruisent
			// avec le vecteur (le worker est deja joint, personne n'y touche).
			for (auto &kv : mEntries)
				kv.Second.lowSlot = -1;
			mLowPayloads.Clear();
			mFreeLowSlots.Clear();
			// Evince toutes les ressources residentes (rend la VRAM).
			for (auto &kv : mEntries) {
				NkStreamEntry &e = kv.Second;
				if (e.state == NkStreamState::NK_RESIDENT) {
					if (!e.isMesh && e.tex.IsValid() && mTexLib)
						mTexLib->Release(e.tex);
					if (e.isMesh && e.mesh.IsValid() && mMesh)
						mMesh->Release(e.mesh);
				}
			}
			mEntries.Clear();
			mPendingQueue.Clear();
			mLoadingQueue.Clear();
			mUsedBytes = 0;
			mReady = false;
		}

		void NkStreamingSystem::RegisterTexture(uint64 id, const NkString &path, uint64 sizeEstimateBytes) {
			NkStreamEntry e;
			e.id = id;
			e.sourcePath = path;
			e.sizeBytes = sizeEstimateBytes;
			e.isMesh = false;
			mEntries.Insert(id, e);
		}

		void NkStreamingSystem::RegisterMesh(uint64 id, const NkString &path, uint64 sizeEstimateBytes) {
			NkStreamEntry e;
			e.id = id;
			e.sourcePath = path;
			e.sizeBytes = sizeEstimateBytes;
			e.isMesh = true;
			mEntries.Insert(id, e);
		}

		void NkStreamingSystem::Unregister(uint64 id) {
			auto *e = mEntries.Find(id);
			if (!e)
				return;
			if (e->state == NkStreamState::NK_RESIDENT)
				Evict(id);
			// L'entry disparait de la map : son slot de basse res doit repartir
			// au pool, sinon les pixels resteraient vivants sans proprietaire.
			// (No-op apres Evict, qui l'a deja rendu.)
			ReleaseLowPayload(*e);
			mEntries.Erase(id);
		}

		void NkStreamingSystem::Request(uint64 id, float32 distFromCamera) {
			auto *e = mEntries.Find(id);
			if (!e)
				return;
			e->dist = distFromCamera * mCfg.distanceMult;
			e->priority = ComputePriority(*e, mCamPos);
			e->lastUsed = 0.f; // reset LRU age
			// Trop loin = pas de stream-in (garde le budget pour le proche).
			if (e->dist > mCfg.streamInDist)
				return;
			if (e->state == NkStreamState::NK_UNLOADED && !e->loadFailed) {
				e->state = NkStreamState::NK_PENDING;
				mPendingQueue.PushBack(id);
				if (mCallback)
					mCallback(id, NkStreamState::NK_PENDING);
			}
		}

		void NkStreamingSystem::Release(uint64 id) {
			auto *e = mEntries.Find(id);
			if (e)
				e->lastUsed += 999.f; // vieillit vite pour l'eviction LRU
		}

		void NkStreamingSystem::SetCameraPosition(NkVec3f pos) {
			mCamPos = pos;
		}

		void NkStreamingSystem::Update(float32 deltaTimeSec) {
			if (!mReady)
				return;
			for (auto &kv : mEntries)
				kv.Second.lastUsed += deltaTimeSec;

			TickQueue(deltaTimeSec);
		}

		void NkStreamingSystem::SetStateCallback(NkStreamCallback cb) {
			mCallback = cb;
		}

		NkStreamState NkStreamingSystem::GetState(uint64 id) const {
			auto *e = mEntries.Find(id);
			return e ? e->state : NkStreamState::NK_UNLOADED;
		}

		bool NkStreamingSystem::IsResident(uint64 id) const {
			return GetState(id) == NkStreamState::NK_RESIDENT;
		}

		NkTexHandle NkStreamingSystem::GetTexture(uint64 id) const {
			auto *e = mEntries.Find(id);
			return (e && e->state == NkStreamState::NK_RESIDENT) ? e->tex : NkTexHandle{};
		}

		NkMeshHandle NkStreamingSystem::GetMesh(uint64 id) const {
			auto *e = mEntries.Find(id);
			return (e && e->state == NkStreamState::NK_RESIDENT) ? e->mesh : NkMeshHandle{};
		}

		float32 NkStreamingSystem::GetUsageRatio() const {
			if (mCfg.budgetBytes == 0)
				return 0.f;
			return (float32)mUsedBytes / (float32)mCfg.budgetBytes;
		}

		uint32 NkStreamingSystem::GetPendingCount() const {
			return (uint32)mPendingQueue.Size();
		}

		uint32 NkStreamingSystem::GetLoadingCount() const {
			return (uint32)mLoadingQueue.Size();
		}

		uint32 NkStreamingSystem::GetResidentCount() const {
			uint32 n = 0;
			for (auto &kv : mEntries)
				if (kv.Second.state == NkStreamState::NK_RESIDENT)
					++n;
			return n;
		}

		// ── Worker : disque + decode CPU (ne touche JAMAIS mEntries) ─────────────

		void NkStreamingSystem::WorkerLoop() {
			for (;;) {
				LoadJob job;
				{
					threading::NkScopedLockMutex lock(mJobMutex);
					while (!mStop && mJobsIn.Empty())
						mJobCv.Wait(lock);
					if (mStop)
						return;
					job = mJobsIn[0];
					mJobsIn.Erase(mJobsIn.Begin());
				}
				LoadResult r = DoLoadCPU(job);
				{
					threading::NkScopedLockMutex lock(mJobMutex);
					// TRANSFERT DE PROPRIETE worker -> file partagee : move, et
					// il a lieu DANS la meme section critique que l'ancienne
					// copie (mJobMutex tenu). Apres le move, `r` ne detient plus
					// aucun pixel ; sa destruction en fin d'iteration est un
					// no-op, meme hors du verrou.
					mResults.PushBack(traits::NkMove(r));
				}
			}
		}

		NkStreamingSystem::LoadResult NkStreamingSystem::DoLoadCPU(const LoadJob &job) const {
			LoadResult r;
			r.id = job.id;
			r.isMesh = job.isMesh;
			r.fullOnly = job.fullOnly;
			if (job.isMesh) {
				auto *md = memory::NkGetDefaultAllocator().New<NkGLTFMeshData>();
				NkString lower = job.path;
				for (uint32 i = 0; i < (uint32)lower.Size(); ++i) {
					char c = lower[i];
					if (c >= 'A' && c <= 'Z')
						lower[i] = (char)(c + 32);
				}
				bool ok = false;
				if (lower.EndsWith(".gltf") || lower.EndsWith(".glb"))
					ok = LoadGLTF(job.path, *md);
				else if (lower.EndsWith(".obj"))
					ok = LoadOBJ(job.path, *md);
				if (ok && !md->vertices.Empty()) {
					r.meshData = md;
					r.ok = true;
				} else {
					memory::NkGetDefaultAllocator().Delete(md);
				}
			} else {
				// Image locale PAR VALEUR : si le chargement echoue, elle se
				// libere toute seule en sortant de la portee (plus de Delete).
				NkImage img;
				// Force RGBA8 (4 canaux) : format d'upload unique.
				if (img.Load(job.path.CStr(), 4) && img.Pixels() && img.Width() > 0) {
					r.img = traits::NkMove(img); // propriete -> le resultat
					r.ok = true;
					// V2 mip streaming : version basse resolution (floue) fabriquee
					// ICI (worker, CPU). Pas pour les jobs de REFINE (pleine res
					// seule : la basse est deja gardee en RAM cote entry).
					if (!job.fullOnly && mCfg.enableMipStreaming && mCfg.lowResMax > 0) {
						const int32 w = r.img.Width(), h = r.img.Height();
						const int32 side = w > h ? w : h;
						if ((uint32)side > mCfg.lowResMax * 2) { // inutil si deja petite
							const float32 k = (float32)mCfg.lowResMax / (float32)side;
							const int32 lw = (int32)(w * k) > 1 ? (int32)(w * k) : 1;
							const int32 lh = (int32)(h * k) > 1 ? (int32)(h * k) : 1;
							// Resize rend une NkImage PAR VALEUR ; en cas d'echec
							// elle est simplement INVALIDE (plus de nullptr).
							r.imgLow = r.img.Resize(lw, lh, NkResizeFilter::NK_BILINEAR);
						}
					}
				}
			}
			return r;
		}

		void NkStreamingSystem::FreePayload(LoadResult &r) {
			// Les deux images sont des VALEURS : Unload() rend les pixels et
			// laisse l'objet reutilisable. Rien a distinguer entre une image
			// issue de Load et une issue de Resize — c'etait ca, le piege.
			// Sur une image deja deplacee (propriete transferee), c'est un no-op.
			r.img.Unload();
			r.imgLow.Unload();
			if (r.meshData) {
				memory::NkGetDefaultAllocator().Delete(r.meshData);
				r.meshData = nullptr;
			}
		}

		// ── Basse res CPU : propriete par valeur, adressee par slot ─────────────
		// NkStreamEntry doit rester COPIABLE (NkHashMap::Insert copie la valeur
		// dans son noeud) alors que NkImage ne l'est plus. Les images vivent donc
		// dans mLowPayloads et l'entry n'en garde qu'un indice. La propriete est
		// au vecteur : rien a liberer a la main, Unload() suffit a rendre un slot.
		// ⚠ Thread de rendu uniquement (FinalizeLoad / TickRefines / Evict /
		// Unregister / Shutdown) — le worker n'y accede jamais.

		int32 NkStreamingSystem::AcquireLowSlot() {
			if (!mFreeLowSlots.Empty()) {
				const int32 slot = mFreeLowSlots[mFreeLowSlots.Size() - 1];
				mFreeLowSlots.PopBack();
				return slot;
			}
			mLowPayloads.EmplaceBack(); // NkImage vide
			return (int32)mLowPayloads.Size() - 1;
		}

		void NkStreamingSystem::StoreLowPayload(NkStreamEntry &e, NkImage &&img) {
			if (e.lowSlot < 0)
				e.lowSlot = AcquireLowSlot();
			// Move-assign : libere l'eventuelle image precedente du slot puis
			// transfere le buffer, sans copier un seul pixel.
			mLowPayloads[(uint32)e.lowSlot] = traits::NkMove(img);
		}

		void NkStreamingSystem::ReleaseLowPayload(NkStreamEntry &e) {
			if (e.lowSlot < 0)
				return;
			mLowPayloads[(uint32)e.lowSlot].Unload(); // rend les pixels, slot reutilisable
			mFreeLowSlots.PushBack(e.lowSlot);
			e.lowSlot = -1;
		}

		// ⚠ Le pointeur rendu OBSERVE un element de mLowPayloads : il devient
		// invalide si un StoreLowPayload ulterieur fait grandir le vecteur.
		// Ne pas le conserver au-dela de l'usage immediat.
		const NkImage *NkStreamingSystem::GetLowPayload(const NkStreamEntry &e) const {
			if (e.lowSlot < 0)
				return nullptr;
			const NkImage &img = mLowPayloads[(uint32)e.lowSlot];
			return img.IsValid() ? &img : nullptr;
		}

		// ── Main thread ──────────────────────────────────────────────────────────

		// V2 mip streaming pilote par la DISTANCE (hysterese anti ping-pong) :
		//   dist <= refineDist          -> pleine resolution (re-decode worker)
		//   dist >  refineDist * 1.25   -> retrogradation basse res (instantanee,
		//                                  la basse res CPU est gardee en RAM)
		void NkStreamingSystem::TickRefines(uint32 &budgetJobs) {
			const float32 refineDist = mCfg.streamInDist * mCfg.refineDistMult;
			const float32 downDist = refineDist * 1.25f;
			for (auto &kv : mEntries) {
				NkStreamEntry &e = kv.Second;
				// Observateur : ne possede rien, la basse res reste dans le pool.
				const NkImage *lowImg = GetLowPayload(e);
				if (e.isMesh || e.state != NkStreamState::NK_RESIDENT || !lowImg)
					continue;
				e.targetMip = (e.dist <= refineDist) ? 0u : 1u;

				// Remontee basse -> pleine : re-decode sur le worker (fullOnly).
				if (e.residentMip == 1 && e.targetMip == 0 && !e.refineInFlight && budgetJobs > 0) {
					e.refineInFlight = true;
					LoadJob job;
					job.id = e.id;
					job.path = e.sourcePath;
					job.isMesh = false;
					job.fullOnly = true;
					if (mCfg.async) {
						{
							threading::NkScopedLockMutex lock(mJobMutex);
							mJobsIn.PushBack(job);
						}
						mJobCv.NotifyOne();
					} else {
						LoadResult r = DoLoadCPU(job);
						FinalizeLoad(r);
					}
					--budgetJobs;
					continue;
				}

				// Retrogradation pleine -> basse : upload direct du payload RAM.
				if (e.residentMip == 0 && e.dist > downDist && mTexLib && budgetJobs > 0) {
					NkTextureCreateDesc td;
					td.pixels = lowImg->Pixels();
					td.width = (uint32)lowImg->Width();
					td.height = (uint32)lowImg->Height();
					td.srgb = true;
					td.genMips = true;
					td.debugName = e.sourcePath.CStr();
					NkTexHandle low = mTexLib->Create(td);
					if (low.IsValid()) {
						if (e.tex.IsValid())
							mTexLib->Release(e.tex); // libere la pleine res GPU
						mUsedBytes = (mUsedBytes >= e.sizeBytes) ? mUsedBytes - e.sizeBytes : 0;
						e.tex = low;
						e.sizeBytes = (uint64)td.width * td.height * 4ULL * 4ULL / 3ULL;
						mUsedBytes += e.sizeBytes;
						e.residentMip = 1;
						if (mCallback)
							mCallback(e.id, NkStreamState::NK_RESIDENT); // retrogradee
						--budgetJobs;
					}
				}
			}
		}

		void NkStreamingSystem::TickQueue(float32 /*dt*/) {
			// 1) Draine les resultats du worker -> uploads GPU (bornes par frame).
			NkVector<LoadResult> ready;
			{
				threading::NkScopedLockMutex lock(mJobMutex);
				uint32 take = 0;
				while (!mResults.Empty() && take < mCfg.maxJobsPerFrame) {
					// Sortie de propriete de la file partagee : move, toujours
					// sous mJobMutex (section critique inchangee). L'element
					// deplace est ensuite detruit vide par Erase.
					ready.PushBack(traits::NkMove(mResults[0]));
					mResults.Erase(mResults.Begin());
					++take;
				}
			}
			for (auto &r : ready)
				FinalizeLoad(r);

			// 1b) Raffinements basse -> pleine resolution (budget partage).
			uint32 refineBudget = mCfg.maxJobsPerFrame;
			TickRefines(refineBudget);

			// 2) Eviction distance (stream-out) + budget.
			for (auto &kv : mEntries) {
				NkStreamEntry &e = kv.Second;
				if (e.state == NkStreamState::NK_RESIDENT && e.dist > mCfg.streamOutDist)
					Evict(e.id);
			}
			while (mUsedBytes > mCfg.budgetBytes)
				EvictLRU();

			// 3) Demarre de nouveaux jobs (file triee par priorite).
			SortPendingQueue();
			uint32 started = 0;
			while (!mPendingQueue.Empty() && started < mCfg.maxJobsPerFrame &&
				   (uint32)mLoadingQueue.Size() < mCfg.maxJobsPerFrame * 2) {
				uint64 id = mPendingQueue[0];
				mPendingQueue.Erase(mPendingQueue.Begin());
				StartLoadJob(id);
				++started;
			}
		}

		void NkStreamingSystem::StartLoadJob(uint64 id) {
			auto *e = mEntries.Find(id);
			if (!e)
				return;
			e->state = NkStreamState::NK_LOADING;
			mLoadingQueue.PushBack(id);
			if (mCallback)
				mCallback(id, NkStreamState::NK_LOADING);

			LoadJob job;
			job.id = id;
			job.path = e->sourcePath;
			job.isMesh = e->isMesh;

			if (mCfg.async) {
				{
					threading::NkScopedLockMutex lock(mJobMutex);
					mJobsIn.PushBack(job);
				}
				mJobCv.NotifyOne();
			} else {
				// Mode synchrone : tout sur le thread appelant (debug/tests).
				LoadResult r = DoLoadCPU(job);
				FinalizeLoad(r);
			}
		}

		void NkStreamingSystem::FinalizeLoad(LoadResult &r) {
			// Retire de la file de chargement.
			for (uint32 i = 0; i < (uint32)mLoadingQueue.Size(); ++i) {
				if (mLoadingQueue[i] == r.id) {
					mLoadingQueue.Erase(mLoadingQueue.Begin() + i);
					break;
				}
			}
			auto *e = mEntries.Find(r.id);
			// ── Resultat d'un job de REFINE (remontee basse -> pleine res) ───
			if (r.fullOnly) {
				if (e)
					e->refineInFlight = false;
				if (!e || e->state != NkStreamState::NK_RESIDENT || e->residentMip != 1 || !r.ok || !mTexLib) {
					FreePayload(r); // entry evincee/disparue entre-temps : on jette
					return;
				}
				NkTextureCreateDesc td;
				td.pixels = r.img.Pixels();
				td.width = (uint32)r.img.Width();
				td.height = (uint32)r.img.Height();
				td.srgb = true;
				td.genMips = true;
				td.debugName = e->sourcePath.CStr();
				NkTexHandle full = mTexLib->Create(td);
				if (full.IsValid()) {
					if (e->tex.IsValid())
						mTexLib->Release(e->tex); // libere la basse res GPU
					mUsedBytes = (mUsedBytes >= e->sizeBytes) ? mUsedBytes - e->sizeBytes : 0;
					e->tex = full;
					e->sizeBytes = (uint64)td.width * td.height * 4ULL * 4ULL / 3ULL;
					mUsedBytes += e->sizeBytes;
					e->residentMip = 0;
					if (mCallback)
						mCallback(r.id, NkStreamState::NK_RESIDENT); // raffinee
				}
				FreePayload(r);
				return;
			}
			if (!e || e->state != NkStreamState::NK_LOADING) {
				// Unregister pendant le vol : payload a liberer.
				FreePayload(r);
				return;
			}
			if (!r.ok) {
				e->state = NkStreamState::NK_UNLOADED;
				e->loadFailed = true; // pas de retry automatique (evite le spam disque)
				++mFailCount;
				logger.Warnf("[NkStreaming] echec chargement id=%llu : %s\n", (unsigned long long)r.id,
							 e->sourcePath.CStr());
				if (mCallback)
					mCallback(r.id, NkStreamState::NK_UNLOADED);
				FreePayload(r);
				return;
			}

			// ── Upload GPU (thread de rendu) ─────────────────────────────────
			uint64 realBytes = 0;
			if (!r.isMesh && mTexLib) {
				// V2 mip streaming : si une version BASSE resolution existe, on
				// l'uploade EN PREMIER (quasi instantane -> texture floue tout de
				// suite) et la pleine resolution part en file de RAFFINEMENT
				// (TickRefines l'echangera quand la camera sera assez proche).
				// L'echec de Resize se lit sur IsValid(), plus sur nullptr.
				const bool progressive = r.imgLow.IsValid();
				// Observateur : `up` ne possede rien, il designe l'une des deux
				// images du resultat, qui restent proprietaires de leurs pixels.
				const NkImage &up = progressive ? r.imgLow : r.img;
				NkTextureCreateDesc td;
				td.pixels = up.Pixels();
				td.width = (uint32)up.Width();
				td.height = (uint32)up.Height();
				td.srgb = true;
				td.genMips = true;
				td.debugName = e->sourcePath.CStr();
				e->tex = mTexLib->Create(td);
				// +33% pour la chaine de mips.
				realBytes = (uint64)td.width * td.height * 4ULL * 4ULL / 3ULL;
				if (e->tex.IsValid() && progressive) {
					e->residentMip = 1; // basse res residente (floue)
					// La basse res CPU est GARDEE cote systeme : la retrogradation
					// (loin) sera instantanee. La pleine res sera RE-DECODEE par
					// un job de refine quand la camera approchera (TickQueue).
					// MOVE : la propriete quitte le LoadResult pour le slot de
					// l'entry ; apres l'appel, r.imgLow est vide (l'ancien
					// `r.imgLow = nullptr` explicite n'est plus necessaire — et
					// une simple affectation ne compilerait pas).
					StoreLowPayload(*e, traits::NkMove(r.imgLow));
				} else if (e->tex.IsValid()) {
					e->residentMip = 0;
				}
				if (!e->tex.IsValid()) {
					r.ok = false;
				}
			} else if (r.isMesh && mMesh) {
				NkMeshDesc d;
				d.layout = NkVertexLayout::Default3D();
				d.vertices = r.meshData->vertices.Data();
				d.vertexCount = (uint32)r.meshData->vertices.Size();
				d.indices = r.meshData->indices.Data();
				d.indexCount = (uint32)r.meshData->indices.Size();
				d.subMeshes = r.meshData->subMeshes;
				d.bounds = r.meshData->bounds;
				d.debugName = e->sourcePath;
				e->mesh = mMesh->Create(d);
				realBytes = (uint64)d.vertexCount * d.layout.stride + (uint64)d.indexCount * 4ULL;
				if (!e->mesh.IsValid()) {
					r.ok = false;
				}
			}
			FreePayload(r);

			if (!r.ok) {
				e->state = NkStreamState::NK_UNLOADED;
				e->loadFailed = true;
				++mFailCount;
				if (mCallback)
					mCallback(r.id, NkStreamState::NK_UNLOADED);
				return;
			}

			e->sizeBytes = realBytes;
			mUsedBytes += realBytes;
			e->state = NkStreamState::NK_RESIDENT;
			e->lastUsed = 0.f;
			if (mCallback)
				mCallback(r.id, NkStreamState::NK_RESIDENT);

			// Si le budget deborde APRES cet upload, l'eviction LRU du prochain
			// TickQueue s'en charge (jamais la ressource la plus fraiche).
		}

		void NkStreamingSystem::Evict(uint64 id) {
			auto *e = mEntries.Find(id);
			if (!e || e->state != NkStreamState::NK_RESIDENT)
				return;
			e->state = NkStreamState::NK_EVICTING;
			if (mCallback)
				mCallback(id, NkStreamState::NK_EVICTING);
			// Libere la VRAIE ressource GPU.
			if (!e->isMesh && e->tex.IsValid() && mTexLib)
				mTexLib->Release(e->tex);
			if (e->isMesh && e->mesh.IsValid() && mMesh)
				mMesh->Release(e->mesh);
			e->tex = {};
			e->mesh = {};
			ReleaseLowPayload(*e); // basse res CPU rendue avec l'eviction
			e->refineInFlight = false;
			e->residentMip = 0;
			mUsedBytes = (mUsedBytes >= e->sizeBytes) ? mUsedBytes - e->sizeBytes : 0;
			++mEvictCount;
			e->state = NkStreamState::NK_UNLOADED;
			if (mCallback)
				mCallback(id, NkStreamState::NK_UNLOADED);
		}

		void NkStreamingSystem::EvictLRU() {
			uint64 oldestId = 0;
			float32 maxScore = -1e30f;

			for (auto &kv : mEntries) {
				if (kv.Second.state != NkStreamState::NK_RESIDENT)
					continue;
				// Score = age - priorite : vieux ET peu prioritaire d'abord.
				float32 score = kv.Second.lastUsed - kv.Second.priority;
				if (score > maxScore) {
					maxScore = score;
					oldestId = kv.First;
				}
			}
			if (oldestId)
				Evict(oldestId);
			else
				mUsedBytes = 0; // rien d'evincable : evite une boucle infinie
		}

		float32 NkStreamingSystem::ComputePriority(const NkStreamEntry &e, NkVec3f /*cam*/) const {
			// Plus PROCHE = plus prioritaire. La distance vient de Request()
			// (fournie par le culling/la scene qui connait la position monde).
			return 1.f / (1.f + (e.dist < 0.f ? 0.f : e.dist));
		}

		void NkStreamingSystem::SortPendingQueue() {
			// Tri insertion par priorite decroissante (files courtes).
			uint32 n = (uint32)mPendingQueue.Size();
			for (uint32 i = 1; i < n; ++i) {
				uint64 key = mPendingQueue[i];
				auto *ke = mEntries.Find(key);
				int32 j = (int32)i - 1;
				while (j >= 0) {
					auto *je = mEntries.Find(mPendingQueue[j]);
					float32 kp = ke ? ke->priority : 0.f;
					float32 jp = je ? je->priority : 0.f;
					if (jp >= kp)
						break;
					mPendingQueue[j + 1] = mPendingQueue[j];
					--j;
				}
				mPendingQueue[j + 1] = key;
			}
		}

	} // namespace renderer
} // namespace nkentseu
