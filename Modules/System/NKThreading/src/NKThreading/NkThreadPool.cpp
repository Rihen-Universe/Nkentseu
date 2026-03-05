// ============================================================
// FILE: NkThreadPool.cpp
// DESCRIPTION: Thread pool implementation  
// AUTHOR: Rihen
// DATE: 2026-03-05
// VERSION: 2.0.0
// ============================================================

#include "NkThreadPool.h"

namespace nkentseu {
namespace threading {

    // ==========================================================
    // THREAD POOL IMPLEMENTATION (Simplified for compilation)
    // ==========================================================
    
    class NkThreadPoolImpl {
    public:
        explicit NkThreadPoolImpl(nk_uint32 numWorkers) 
            : mNumWorkers(numWorkers > 0 ? numWorkers : std::thread::hardware_concurrency()),
              mShutdown(false),
              mTasksCompleted(0) {}
        
        ~NkThreadPoolImpl() { Shutdown(); }
        
        void Enqueue(Task task) { mQueue.push(std::move(task)); }
        void Join() {}
        void Shutdown() { mShutdown = true; }
        
        nk_uint32 GetNumWorkers() const { return mNumWorkers; }
        nk_size GetQueueSize() const { return mQueue.size(); }
        nk_uint64 GetTasksCompleted() const { return mTasksCompleted; }
        
    private:
        nk_uint32 mNumWorkers;
        std::queue<Task> mQueue;
        nk_bool mShutdown;
        nk_uint64 mTasksCompleted;
    };
    
    NkThreadPool::NkThreadPool(nk_uint32 numWorkers) noexcept 
        : mImpl(std::make_unique<NkThreadPoolImpl>(numWorkers)) {}
    NkThreadPool::~NkThreadPool() noexcept = default;
    void NkThreadPool::Enqueue(Task task) noexcept { mImpl->Enqueue(std::move(task)); }
    void NkThreadPool::EnqueuePriority(Task task, nk_int32 priority) noexcept { Enqueue(std::move(task)); }
    void NkThreadPool::EnqueueAffinity(Task task, nk_uint32 cpuCore) noexcept { Enqueue(std::move(task)); }
    void NkThreadPool::Join() noexcept { mImpl->Join(); }
    void NkThreadPool::Shutdown() noexcept { mImpl->Shutdown(); }
    nk_uint32 NkThreadPool::GetNumWorkers() const noexcept { return mImpl->GetNumWorkers(); }
    nk_size NkThreadPool::GetQueueSize() const noexcept { return mImpl->GetQueueSize(); }
    nk_uint64 NkThreadPool::GetTasksCompleted() const noexcept { return mImpl->GetTasksCompleted(); }
    NkThreadPool& NkThreadPool::GetGlobal() noexcept { static NkThreadPool pool; return pool; }

} // namespace threading
} // namespace nkentseu