// =============================================================================
// NkStreamingSystem.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkStreamingSystem.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include <cmath>

namespace nkentseu {
namespace renderer {

    NkStreamingSystem::~NkStreamingSystem() {
        Shutdown();
    }

    bool NkStreamingSystem::Init(NkIDevice* device, NkTextureLibrary* texLib,
                                   NkMeshSystem* meshSys,
                                   const NkStreamingConfig& cfg) {
        mDevice  = device;
        mTexLib  = texLib;
        mMesh    = meshSys;
        mCfg     = cfg;
        mReady   = true;
        return true;
    }

    void NkStreamingSystem::Shutdown() {
        if (!mReady) return;
        mEntries.Clear();
        mPendingQueue.Clear();
        mLoadingQueue.Clear();
        mUsedBytes = 0;
        mReady = false;
    }

    void NkStreamingSystem::RegisterTexture(uint64 id, const NkString& path,
                                              uint64 sizeEstimateBytes) {
        NkStreamEntry e;
        e.id         = id;
        e.sourcePath = path;
        e.sizeBytes  = sizeEstimateBytes;
        e.isMesh     = false;
        mEntries.Insert(id, e);
    }

    void NkStreamingSystem::RegisterMesh(uint64 id, const NkString& path,
                                          uint64 sizeEstimateBytes) {
        NkStreamEntry e;
        e.id         = id;
        e.sourcePath = path;
        e.sizeBytes  = sizeEstimateBytes;
        e.isMesh     = true;
        mEntries.Insert(id, e);
    }

    void NkStreamingSystem::Unregister(uint64 id) {
        auto* e = mEntries.Find(id);
        if (!e) return;
        if (e->state == NkStreamState::NK_RESIDENT) {
            mUsedBytes -= e->sizeBytes;
            ++mEvictCount;
        }
        mEntries.Erase(id);
    }

    void NkStreamingSystem::Request(uint64 id, float32 distFromCamera) {
        auto* e = mEntries.Find(id);
        if (!e) return;
        e->priority  = ComputePriority(*e, mCamPos);
        e->lastUsed  = 0.f; // reset LRU age
        if (e->state == NkStreamState::NK_UNLOADED) {
            e->state = NkStreamState::NK_PENDING;
            mPendingQueue.PushBack(id);
        }
    }

    void NkStreamingSystem::Release(uint64 id) {
        auto* e = mEntries.Find(id);
        if (e) e->lastUsed += 999.f; // age quickly for LRU eviction
    }

    void NkStreamingSystem::SetCameraPosition(NkVec3f pos) {
        mCamPos = pos;
    }

    void NkStreamingSystem::Update(float32 deltaTimeSec) {
        if (!mReady) return;
        // Age all entries for LRU
        for (auto& kv : mEntries)
            kv.Second.lastUsed += deltaTimeSec;

        TickQueue(deltaTimeSec);
    }

    void NkStreamingSystem::SetStateCallback(NkStreamCallback cb) {
        mCallback = cb;
    }

    NkStreamState NkStreamingSystem::GetState(uint64 id) const {
        auto* e = mEntries.Find(id);
        return e ? e->state : NkStreamState::NK_UNLOADED;
    }

    bool NkStreamingSystem::IsResident(uint64 id) const {
        return GetState(id) == NkStreamState::NK_RESIDENT;
    }

    float32 NkStreamingSystem::GetUsageRatio() const {
        if (mCfg.budgetBytes == 0) return 0.f;
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
        for (auto& kv : mEntries)
            if (kv.Second.state == NkStreamState::NK_RESIDENT) ++n;
        return n;
    }

    // ── Private ──────────────────────────────────────────────────────────────

    void NkStreamingSystem::TickQueue(float32 /*dt*/) {
        // Finalize completed loads
        NkVector<uint64> doneLoading;
        for (uint64 id : mLoadingQueue) {
            // In a real implementation, check async job completion
            // For now: immediately finalize (sync mode or stub)
            if (mCfg.async == false) {
                FinalizeLoad(id);
                doneLoading.PushBack(id);
            }
        }
        for (uint64 id : doneLoading) {
            for (uint32 i = 0; i < mLoadingQueue.Size(); ++i) {
                if (mLoadingQueue[i] == id) {
                    mLoadingQueue.Erase(mLoadingQueue.Begin() + i);
                    break;
                }
            }
        }

        // Evict if over budget
        while (mUsedBytes > mCfg.budgetBytes)
            EvictLRU();

        // Sort pending by priority and start new jobs
        SortPendingQueue();
        uint32 started = 0;
        while (!mPendingQueue.Empty() &&
               started < mCfg.maxJobsPerFrame &&
               (uint32)mLoadingQueue.Size() < mCfg.maxJobsPerFrame) {
            uint64 id = mPendingQueue[0];
            mPendingQueue.Erase(0);
            StartLoadJob(id);
            ++started;
        }
    }

    void NkStreamingSystem::StartLoadJob(uint64 id) {
        auto* e = mEntries.Find(id);
        if (!e) return;
        e->state = NkStreamState::NK_LOADING;
        mLoadingQueue.PushBack(id);
        if (mCallback) mCallback(id, NkStreamState::NK_LOADING);

        if (!mCfg.async) {
            FinalizeLoad(id);
        }
    }

    void NkStreamingSystem::FinalizeLoad(uint64 id) {
        auto* e = mEntries.Find(id);
        if (!e || e->state != NkStreamState::NK_LOADING) return;

        // Evict to make room if needed
        while (mUsedBytes + e->sizeBytes > mCfg.budgetBytes)
            EvictLRU();

        // In a real impl: trigger NkTextureLibrary::Load or NkMeshSystem::Load
        // For now: mark resident with estimated size
        mUsedBytes += e->sizeBytes;
        e->state      = NkStreamState::NK_RESIDENT;
        e->lastUsed   = 0.f;
        if (mCallback) mCallback(id, NkStreamState::NK_RESIDENT);
    }

    void NkStreamingSystem::Evict(uint64 id) {
        auto* e = mEntries.Find(id);
        if (!e || e->state != NkStreamState::NK_RESIDENT) return;
        e->state = NkStreamState::NK_EVICTING;
        if (mCallback) mCallback(id, NkStreamState::NK_EVICTING);
        mUsedBytes -= e->sizeBytes;
        ++mEvictCount;
        e->state = NkStreamState::NK_UNLOADED;
        if (mCallback) mCallback(id, NkStreamState::NK_UNLOADED);
    }

    void NkStreamingSystem::EvictLRU() {
        uint64  oldestId  = 0;
        float32 maxAge    = -1.f;
        float32 minPri    = 1e30f;

        for (auto& kv : mEntries) {
            if (kv.Second.state != NkStreamState::NK_RESIDENT) continue;
            float32 score = kv.Second.lastUsed - kv.Second.priority;
            if (score > maxAge || (score == maxAge && kv.Second.priority < minPri)) {
                maxAge   = score;
                minPri   = kv.Second.priority;
                oldestId = kv.First;
            }
        }
        if (oldestId) Evict(oldestId);
    }

    float32 NkStreamingSystem::ComputePriority(const NkStreamEntry& /*e*/,
                                                 NkVec3f /*cam*/) const {
        // Higher = more urgent. Simple inverse distance.
        // In practice: use e's world position and distance to cam.
        return 1.f;
    }

    void NkStreamingSystem::SortPendingQueue() {
        // Simple insertion sort by priority (desc)
        uint32 n = (uint32)mPendingQueue.Size();
        for (uint32 i = 1; i < n; ++i) {
            uint64 key = mPendingQueue[i];
            auto*  ke  = mEntries.Find(key);
            int32  j   = (int32)i - 1;
            while (j >= 0) {
                auto* je = mEntries.Find(mPendingQueue[j]);
                float32 kp = ke  ? ke->priority  : 0.f;
                float32 jp = je  ? je->priority  : 0.f;
                if (jp >= kp) break;
                mPendingQueue[j+1] = mPendingQueue[j];
                --j;
            }
            mPendingQueue[j+1] = key;
        }
    }

} // namespace renderer
} // namespace nkentseu
