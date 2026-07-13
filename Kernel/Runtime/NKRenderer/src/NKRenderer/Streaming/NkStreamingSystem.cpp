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
			// Raffinements en attente : pleine res jamais uploadee.
			for (auto &pr : mRefines)
				if (pr.imgFull)
					memory::NkGetDefaultAllocator().Delete(pr.imgFull);
			mRefines.Clear();
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
					mResults.PushBack(r);
				}
			}
		}

		NkStreamingSystem::LoadResult NkStreamingSystem::DoLoadCPU(const LoadJob &job) const {
			LoadResult r;
			r.id = job.id;
			r.isMesh = job.isMesh;
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
				auto *img = memory::NkGetDefaultAllocator().New<NkImage>();
				// Force RGBA8 (4 canaux) : format d'upload unique.
				if (img->Load(job.path.CStr(), 4) && img->Pixels() && img->Width() > 0) {
					r.img = img;
					r.ok = true;
					// V2 mip streaming : version basse resolution (floue) fabriquee
					// ICI (worker, CPU) — uploadee EN PREMIER cote main thread.
					if (mCfg.enableMipStreaming && mCfg.lowResMax > 0) {
						const int32 w = img->Width(), h = img->Height();
						const int32 side = w > h ? w : h;
						if ((uint32)side > mCfg.lowResMax * 2) { // inutil si deja petite
							const float32 k = (float32)mCfg.lowResMax / (float32)side;
							const int32 lw = (int32)(w * k) > 1 ? (int32)(w * k) : 1;
							const int32 lh = (int32)(h * k) > 1 ? (int32)(h * k) : 1;
							r.imgLow = img->Resize(lw, lh, NkResizeFilter::NK_BILINEAR);
						}
					}
				} else {
					memory::NkGetDefaultAllocator().Delete(img);
				}
			}
			return r;
		}

		void NkStreamingSystem::FreePayload(LoadResult &r) {
			if (r.img) {
				memory::NkGetDefaultAllocator().Delete(r.img);
				r.img = nullptr;
			}
			if (r.imgLow) {
				// ⚠ imgLow vient de NkImage::Resize -> NkImage::Alloc : liberation
				// par Free() UNIQUEMENT (jamais Delete -> double-free c0000374).
				r.imgLow->Free();
				r.imgLow = nullptr;
			}
			if (r.meshData) {
				memory::NkGetDefaultAllocator().Delete(r.meshData);
				r.meshData = nullptr;
			}
		}

		// ── Main thread ──────────────────────────────────────────────────────────

		// V2 mip streaming : upgrades basse -> pleine resolution quand la camera
		// est a moins de refineDist. Budget d'uploads partage avec les loads.
		void NkStreamingSystem::TickRefines(uint32 &budgetJobs) {
			const float32 refineDist = mCfg.streamInDist * mCfg.refineDistMult;
			for (uint32 i = 0; i < (uint32)mRefines.Size();) {
				PendingRefine &pr = mRefines[i];
				auto *e = mEntries.Find(pr.id);
				// Entree disparue ou evincee : la pleine res n'a plus de cible.
				if (!e || e->state != NkStreamState::NK_RESIDENT || e->residentMip == 0) {
					if (pr.imgFull)
						memory::NkGetDefaultAllocator().Delete(pr.imgFull);
					mRefines.Erase(mRefines.Begin() + i);
					continue;
				}
				e->targetMip = (e->dist <= refineDist) ? 0u : 1u;
				if (e->targetMip != 0 || budgetJobs == 0) {
					++i;
					continue; // pas encore assez proche, ou plus de budget frame
				}
				// Upload pleine resolution + swap du handle (l'appelant re-binde
				// en surveillant GetTexture). L'ancienne basse res est liberee.
				NkTextureCreateDesc td;
				td.pixels = pr.imgFull->Pixels();
				td.width = (uint32)pr.imgFull->Width();
				td.height = (uint32)pr.imgFull->Height();
				td.srgb = true;
				td.genMips = true;
				td.debugName = e->sourcePath.CStr();
				NkTexHandle full = mTexLib ? mTexLib->Create(td) : NkTexHandle{};
				if (full.IsValid()) {
					const uint64 fullBytes = (uint64)td.width * td.height * 4ULL * 4ULL / 3ULL;
					if (e->tex.IsValid() && mTexLib)
						mTexLib->Release(e->tex); // libere la basse res
					mUsedBytes = (mUsedBytes >= e->sizeBytes) ? mUsedBytes - e->sizeBytes : 0;
					e->tex = full;
					e->sizeBytes = fullBytes;
					mUsedBytes += fullBytes;
					e->residentMip = 0;
					if (mCallback)
						mCallback(pr.id, NkStreamState::NK_RESIDENT); // raffinee
					--budgetJobs;
				}
				memory::NkGetDefaultAllocator().Delete(pr.imgFull);
				pr.imgFull = nullptr;
				mRefines.Erase(mRefines.Begin() + i);
			}
		}

		void NkStreamingSystem::TickQueue(float32 /*dt*/) {
			// 1) Draine les resultats du worker -> uploads GPU (bornes par frame).
			NkVector<LoadResult> ready;
			{
				threading::NkScopedLockMutex lock(mJobMutex);
				uint32 take = 0;
				while (!mResults.Empty() && take < mCfg.maxJobsPerFrame) {
					ready.PushBack(mResults[0]);
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
				const bool progressive = (r.imgLow != nullptr);
				const NkImage *up = progressive ? r.imgLow : r.img;
				NkTextureCreateDesc td;
				td.pixels = up->Pixels();
				td.width = (uint32)up->Width();
				td.height = (uint32)up->Height();
				td.srgb = true;
				td.genMips = true;
				td.debugName = e->sourcePath.CStr();
				e->tex = mTexLib->Create(td);
				// +33% pour la chaine de mips.
				realBytes = (uint64)td.width * td.height * 4ULL * 4ULL / 3ULL;
				if (e->tex.IsValid() && progressive) {
					e->residentMip = 1; // basse res residente, pleine res en attente
					PendingRefine pr;
					pr.id = r.id;
					pr.imgFull = r.img;
					r.img = nullptr; // possession transferee a mRefines
					mRefines.PushBack(pr);
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
