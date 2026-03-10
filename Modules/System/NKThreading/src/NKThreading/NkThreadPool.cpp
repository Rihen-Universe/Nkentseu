// ============================================================
// FILE: NkThreadPool.cpp
// DESCRIPTION: Thread pool implementation  
// AUTHOR: Rihen
// DATE: 2026-03-05
// VERSION: 2.0.0
// ============================================================

#include "NKThreading/NkThreadPool.h"
#include "NKCore/NkTraits.h"
#include "NKPlatform/NkCPUFeatures.h"
#include "NKThreading/NkConditionVariable.h"
#include "NKThreading/NkThread.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
namespace threading {

    // ==========================================================
    // THREAD POOL IMPLEMENTATION (Simplified for compilation)
    // ==========================================================
    
    class NkThreadPoolImpl {
    public:
        explicit NkThreadPoolImpl(nk_uint32 numWorkers)
            : mNumWorkers(numWorkers > 0
                              ? numWorkers
                              : static_cast<nk_uint32>(platform::NkGetLogicalCoreCount())),
              mWorkers(),
              mQueue(),
              mShutdown(false),
              mTasksCompleted(0u),
              mActiveWorkers(0u),
              mMutex(),
              mWorkAvailable(),
              mIdle() {
            if (mNumWorkers == 0u) {
                mNumWorkers = 1u;
            }

            mWorkers.Reserve(mNumWorkers);
            for (nk_uint32 i = 0u; i < mNumWorkers; ++i) {
                NkThread worker([this]() { WorkerLoop(); });
                mWorkers.PushBack(traits::NkMove(worker));
            }
        }

        ~NkThreadPoolImpl() {
            Shutdown();
        }

        void Enqueue(Task task) {
            if (!task) {
                return;
            }

            NkScopedLock lock(mMutex);
            if (mShutdown) {
                return;
            }
            mQueue.Push(traits::NkMove(task));
            mWorkAvailable.NotifyOne();
        }

        void Join() {
            NkScopedLock lock(mMutex);
            while (!mQueue.Empty() || mActiveWorkers > 0u) {
                mIdle.Wait(lock);
            }
        }

        void Shutdown() {
            {
                NkScopedLock lock(mMutex);
                if (mShutdown) {
                    return;
                }
                mShutdown = true;
                mWorkAvailable.NotifyAll();
            }

            for (nk_size i = 0u; i < mWorkers.Size(); ++i) {
                if (mWorkers[i].Joinable()) {
                    mWorkers[i].Join();
                }
            }
            mWorkers.Clear();
        }

        nk_uint32 GetNumWorkers() const {
            return mNumWorkers;
        }

        nk_size GetQueueSize() const {
            NkScopedLock lock(mMutex);
            return mQueue.Size();
        }

        nk_uint64 GetTasksCompleted() const {
            NkScopedLock lock(mMutex);
            return mTasksCompleted;
        }

    private:
        void WorkerLoop() {
            for (;;) {
                Task task;
                {
                    NkScopedLock lock(mMutex);
                    while (!mShutdown && mQueue.Empty()) {
                        mWorkAvailable.Wait(lock);
                    }

                    if (mShutdown && mQueue.Empty()) {
                        return;
                    }

                    task = traits::NkMove(mQueue.Front());
                    mQueue.Pop();
                    ++mActiveWorkers;
                }

                if (task) {
                    task();
                }

                {
                    NkScopedLock lock(mMutex);
                    ++mTasksCompleted;
                    if (mActiveWorkers > 0u) {
                        --mActiveWorkers;
                    }
                    if (mQueue.Empty() && mActiveWorkers == 0u) {
                        mIdle.NotifyAll();
                    }
                }
            }
        }

        nk_uint32 mNumWorkers;
        NkVector<NkThread> mWorkers;
        NkQueue<Task> mQueue;
        nk_bool mShutdown;
        nk_uint64 mTasksCompleted;
        nk_uint32 mActiveWorkers;
        mutable NkMutex mMutex;
        NkConditionVariable mWorkAvailable;
        NkConditionVariable mIdle;
    };
    
    NkThreadPool::NkThreadPool(nk_uint32 numWorkers) noexcept 
        : mImpl(memory::NkMakeUnique<NkThreadPoolImpl>(numWorkers)) {}
    NkThreadPool::~NkThreadPool() noexcept = default;
    void NkThreadPool::Enqueue(Task task) noexcept { mImpl->Enqueue(traits::NkMove(task)); }
    void NkThreadPool::EnqueuePriority(Task task, nk_int32 priority) noexcept { Enqueue(traits::NkMove(task)); }
    void NkThreadPool::EnqueueAffinity(Task task, nk_uint32 cpuCore) noexcept { Enqueue(traits::NkMove(task)); }
    void NkThreadPool::Join() noexcept { mImpl->Join(); }
    void NkThreadPool::Shutdown() noexcept { mImpl->Shutdown(); }
    nk_uint32 NkThreadPool::GetNumWorkers() const noexcept { return mImpl->GetNumWorkers(); }
    nk_size NkThreadPool::GetQueueSize() const noexcept { return mImpl->GetQueueSize(); }
    nk_uint64 NkThreadPool::GetTasksCompleted() const noexcept { return mImpl->GetTasksCompleted(); }
    NkThreadPool& NkThreadPool::GetGlobal() noexcept { static NkThreadPool pool; return pool; }

} // namespace threading
} // namespace nkentseu
