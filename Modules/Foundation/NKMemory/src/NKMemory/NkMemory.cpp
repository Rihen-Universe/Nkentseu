#include "NKMemory/NkMemory.h"
#include "NKPlatform/NkFoundationLog.h"

#include <stdio.h>

namespace nkentseu {
    namespace memory {

        NkMemorySystem& NkMemorySystem::Instance() noexcept {
            static NkMemorySystem instance;
            return instance;
        }

        NkMemorySystem::NkMemorySystem() noexcept
            : mAllocator(&NkGetDefaultAllocator()),
            mHead(nullptr),
            mLiveBytes(0u),
            mPeakBytes(0u),
            mLiveAllocations(0u),
            mTotalAllocations(0u),
            mLogCallback(nullptr),
            mInitialized(false),
            mLock(),
            mAllocIndex(&NkGetMallocAllocator()),
            mGc(&NkGetDefaultAllocator()),
            mDefaultGcNode(),
            mGcHead(nullptr),
            mGcCount(0u),
            mNextGcId(1u) {
            mDefaultGcNode.Collector = &mGc;
            mDefaultGcNode.Allocator = mAllocator;
            mDefaultGcNode.Id = 0u;
            CopyGcName(mDefaultGcNode.Name, sizeof(mDefaultGcNode.Name), "Default");
            mDefaultGcNode.IsDefault = true;
            mDefaultGcNode.Prev = nullptr;
            mDefaultGcNode.Next = nullptr;
            mGcHead = &mDefaultGcNode;
            mGcCount = 1u;
        }

        void NkMemorySystem::Initialize(NkAllocator* allocator) noexcept {
            NkScopedSpinLock guard(mLock);
            mAllocator = allocator ? allocator : &NkGetDefaultAllocator();
            mGc.SetAllocator(mAllocator);
            mDefaultGcNode.Allocator = mAllocator;
            if (!mAllocIndex.IsInitialized()) {
                (void)mAllocIndex.Initialize(256u, &NkGetMallocAllocator());
            } else {
                mAllocIndex.Clear();
            }
            mInitialized = true;
        }

        void NkMemorySystem::Shutdown(nk_bool reportLeaks) noexcept {
            NkAllocationNode* leakedHead = nullptr;
            NkGcNode* customGcHead = nullptr;

            {
                NkScopedSpinLock guard(mLock);
                leakedHead = mHead;
                mHead = nullptr;
                mAllocIndex.Clear();

                NkGcNode* node = mGcHead;
                while (node) {
                    NkGcNode* next = node->Next;
                    if (!node->IsDefault) {
                        node->Prev = nullptr;
                        node->Next = customGcHead;
                        if (customGcHead) {
                            customGcHead->Prev = node;
                        }
                        customGcHead = node;
                    }
                    node = next;
                }

                mDefaultGcNode.Prev = nullptr;
                mDefaultGcNode.Next = nullptr;
                mDefaultGcNode.Allocator = mAllocator;
                mGcHead = &mDefaultGcNode;
                mGcCount = 1u;

                mInitialized = false;
                mLiveBytes = 0u;
                mLiveAllocations = 0u;
            }

            NkAllocator& registryAllocator = NkGetMallocAllocator();
            while (customGcHead) {
                NkGcNode* next = customGcHead->Next;
                if (customGcHead->Collector) {
                    customGcHead->Collector->~NkGarbageCollector();
                    registryAllocator.Deallocate(customGcHead->Collector);
                    customGcHead->Collector = nullptr;
                }
                registryAllocator.Deallocate(customGcHead);
                customGcHead = next;
            }

            while (leakedHead) {
                NkAllocationNode* next = leakedHead->next;

                if (reportLeaks) {
                    nk_char line[512];
                    NK_FOUNDATION_SPRINT(line,
                            sizeof(line),
                            "[NKMemory] leak ptr=%p size=%llu count=%llu tag=%s at %s:%d (%s)",
                            leakedHead->userPtr,
                            static_cast<unsigned long long>(leakedHead->size),
                            static_cast<unsigned long long>(leakedHead->count),
                            leakedHead->tag ? leakedHead->tag : "<none>",
                            leakedHead->file ? leakedHead->file : "<unknown>",
                            leakedHead->line,
                            leakedHead->function ? leakedHead->function : "<unknown>");
                    LogLine(line);
                }

                if (leakedHead->destroy) {
                    leakedHead->destroy(leakedHead->userPtr, leakedHead->basePtr, mAllocator, leakedHead->count);
                }
                ReleaseNode(leakedHead);
                leakedHead = next;
            }
        }

        void NkMemorySystem::SetLogCallback(NkLogCallback callback) noexcept {
            NkScopedSpinLock guard(mLock);
            mLogCallback = callback;
        }

        void* NkMemorySystem::Allocate(nk_size size,
                                    nk_size alignment,
                                    const nk_char* file,
                                    nk_int32 line,
                                    const nk_char* function,
                                    const nk_char* tag) {
            if (!mInitialized) {
                Initialize(nullptr);
            }

            void* memory = mAllocator->Allocate(size, alignment);
            if (!memory) {
                return nullptr;
            }

            RegisterAllocation(memory, memory, size, 1u, alignment,
                            &NkMemorySystem::DestroyRaw, file, line, function, tag);
            return memory;
        }

        void NkMemorySystem::Free(void* pointer) noexcept {
            if (!pointer) {
                return;
            }

            NkAllocationNode* node = FindAndDetach(pointer);
            if (!node) {
                // Fallback: best effort if pointer was not tracked.
                if (mAllocator) {
                    mAllocator->Deallocate(pointer);
                }
                return;
            }

            if (node->destroy) {
                node->destroy(node->userPtr, node->basePtr, mAllocator, node->count);
            }
            ReleaseNode(node);
        }

        NkMemoryStats NkMemorySystem::GetStats() const noexcept {
            NkScopedSpinLock guard(mLock);
            NkMemoryStats stats{};
            stats.liveBytes = mLiveBytes;
            stats.peakBytes = mPeakBytes;
            stats.liveAllocations = mLiveAllocations;
            stats.totalAllocations = mTotalAllocations;
            return stats;
        }

        void NkMemorySystem::DumpLeaks() noexcept {
            NkScopedSpinLock guard(mLock);
            NkAllocationNode* current = mHead;
            if (!current) {
                LogLine("[NKMemory] no leaks detected.");
                return;
            }

            while (current) {
                nk_char line[512];
                NK_FOUNDATION_SPRINT(line,
                        sizeof(line),
                        "[NKMemory] leak ptr=%p size=%llu count=%llu tag=%s at %s:%d (%s)",
                        current->userPtr,
                        static_cast<unsigned long long>(current->size),
                        static_cast<unsigned long long>(current->count),
                        current->tag ? current->tag : "<none>",
                        current->file ? current->file : "<unknown>",
                        current->line,
                        current->function ? current->function : "<unknown>");
                LogLine(line);
                current = current->next;
            }
        }

        NkGarbageCollector* NkMemorySystem::CreateGc(NkAllocator* allocator) noexcept {
            if (!mInitialized) {
                Initialize(nullptr);
            }

            NkAllocator* gcAllocator = allocator ? allocator : mAllocator;
            if (!gcAllocator) {
                gcAllocator = &NkGetDefaultAllocator();
            }

            NkAllocator& registryAllocator = NkGetMallocAllocator();
            void* collectorMemory = registryAllocator.Allocate(sizeof(NkGarbageCollector), alignof(NkGarbageCollector));
            if (!collectorMemory) {
                return nullptr;
            }

            void* nodeMemory = registryAllocator.Allocate(sizeof(NkGcNode), alignof(NkGcNode));
            if (!nodeMemory) {
                registryAllocator.Deallocate(collectorMemory);
                return nullptr;
            }

            auto* collector = new (collectorMemory) NkGarbageCollector(gcAllocator);
            auto* node = static_cast<NkGcNode*>(nodeMemory);

            {
                NkScopedSpinLock guard(mLock);
                node->Collector = collector;
                node->Allocator = gcAllocator;
                node->Id = mNextGcId++;
                NK_FOUNDATION_SPRINT(node->Name, sizeof(node->Name), "Gc_%llu",
                                     static_cast<unsigned long long>(node->Id));
                node->IsDefault = false;
                node->Prev = nullptr;
                node->Next = mGcHead;
                if (mGcHead) {
                    mGcHead->Prev = node;
                }
                mGcHead = node;
                ++mGcCount;
            }

            return collector;
        }

        nk_bool NkMemorySystem::DestroyGc(NkGarbageCollector* collector) noexcept {
            if (!collector) {
                return false;
            }

            NkGcNode* node = nullptr;
            {
                NkScopedSpinLock guard(mLock);
                node = FindGcNodeLocked(collector);
                if (!node || node->IsDefault) {
                    return false;
                }

                if (node->Prev) {
                    node->Prev->Next = node->Next;
                } else {
                    mGcHead = node->Next;
                }
                if (node->Next) {
                    node->Next->Prev = node->Prev;
                }
                node->Prev = nullptr;
                node->Next = nullptr;

                if (mGcCount > 0u) {
                    --mGcCount;
                }
            }

            NkAllocator& registryAllocator = NkGetMallocAllocator();
            collector->~NkGarbageCollector();
            registryAllocator.Deallocate(collector);
            registryAllocator.Deallocate(node);
            return true;
        }

        nk_bool NkMemorySystem::SetGcName(NkGarbageCollector* collector, const nk_char* name) noexcept {
            if (!collector || !name || !name[0]) {
                return false;
            }

            NkScopedSpinLock guard(mLock);
            NkGcNode* node = FindGcNodeLocked(collector);
            if (!node) {
                return false;
            }
            CopyGcName(node->Name, sizeof(node->Name), name);
            return true;
        }

        nk_bool NkMemorySystem::GetGcProfile(const NkGarbageCollector* collector,
                                             NkGcProfile* outProfile) const noexcept {
            if (!collector || !outProfile) {
                return false;
            }

            NkScopedSpinLock guard(mLock);
            const NkGcNode* node = FindGcNodeLocked(collector);
            if (!node || !node->Collector) {
                return false;
            }

            outProfile->Id = node->Id;
            CopyGcName(outProfile->Name, sizeof(outProfile->Name), node->Name);
            outProfile->ObjectCount = node->Collector->ObjectCount();
            outProfile->Allocator = node->Allocator;
            outProfile->IsDefault = node->IsDefault;
            return true;
        }

        nk_size NkMemorySystem::GetGcCount() const noexcept {
            NkScopedSpinLock guard(mLock);
            return mGcCount;
        }

        void NkMemorySystem::DestroyRaw(void* /*userPtr*/,
                                        void* basePtr,
                                        NkAllocator* allocator,
                                        nk_size /*count*/) {
            if (allocator && basePtr) {
                allocator->Deallocate(basePtr);
            }
        }

        void NkMemorySystem::RegisterAllocation(void* userPtr,
                                                void* basePtr,
                                                nk_size size,
                                                nk_size count,
                                                nk_size alignment,
                                                NkDestroyFn destroy,
                                                const nk_char* file,
                                                nk_int32 line,
                                                const nk_char* function,
                                                const nk_char* tag) {
            NkAllocator& nodeAllocator = NkGetMallocAllocator();
            void* nodeMemory = nodeAllocator.Allocate(sizeof(NkAllocationNode), alignof(NkAllocationNode));
            if (!nodeMemory) {
                // Allocation metadata failed: release user memory immediately to avoid leak.
                if (destroy) {
                    destroy(userPtr, basePtr, mAllocator, count);
                }
                return;
            }

            auto* node = static_cast<NkAllocationNode*>(nodeMemory);
            node->userPtr = userPtr;
            node->basePtr = basePtr;
            node->size = size;
            node->count = count;
            node->alignment = alignment;
            node->destroy = destroy;
            node->file = file;
            node->line = line;
            node->function = function;
            node->tag = tag;
            node->prev = nullptr;
            node->next = nullptr;

            NkScopedSpinLock guard(mLock);
            if (!mAllocIndex.IsInitialized()) {
                (void)mAllocIndex.Initialize(256u, &NkGetMallocAllocator());
            }
            node->next = mHead;
            node->prev = nullptr;
            if (mHead) {
                mHead->prev = node;
            }
            mHead = node;
            if (mAllocIndex.IsInitialized()) {
                (void)mAllocIndex.Insert(userPtr, node);
            }

            ++mLiveAllocations;
            ++mTotalAllocations;
            mLiveBytes += size;
            if (mLiveBytes > mPeakBytes) {
                mPeakBytes = mLiveBytes;
            }
        }

        NkMemorySystem::NkAllocationNode* NkMemorySystem::FindAndDetach(void* userPtr) noexcept {
            NkScopedSpinLock guard(mLock);

            NkAllocationNode* cur = nullptr;
            if (mAllocIndex.IsInitialized()) {
                void* value = nullptr;
                if (mAllocIndex.Erase(userPtr, &value)) {
                    cur = static_cast<NkAllocationNode*>(value);
                }
            }

            if (!cur) {
                cur = mHead;
                while (cur) {
                    if (cur->userPtr == userPtr) {
                        if (mAllocIndex.IsInitialized()) {
                            (void)mAllocIndex.Erase(userPtr, nullptr);
                        }
                        break;
                    }
                    cur = cur->next;
                }
            }

            if (!cur) {
                return nullptr;
            }

            if (cur->prev) {
                cur->prev->next = cur->next;
            } else {
                mHead = cur->next;
            }
            if (cur->next) {
                cur->next->prev = cur->prev;
            }
            cur->prev = nullptr;
            cur->next = nullptr;

            if (mLiveAllocations > 0u) {
                --mLiveAllocations;
            }
            if (mLiveBytes >= cur->size) {
                mLiveBytes -= cur->size;
            } else {
                mLiveBytes = 0u;
            }

            return cur;
        }

        void NkMemorySystem::ReleaseNode(NkAllocationNode* node) noexcept {
            if (!node) {
                return;
            }
            NkGetMallocAllocator().Deallocate(node);
        }

        void NkMemorySystem::LogLine(const nk_char* message) noexcept {
            if (!message) {
                return;
            }
            if (mLogCallback) {
                mLogCallback(message);
                return;
            }
            NK_FOUNDATION_LOG_WARN("%s", message);
        }

        NkMemorySystem::NkGcNode* NkMemorySystem::FindGcNodeLocked(const NkGarbageCollector* collector) noexcept {
            NkGcNode* node = mGcHead;
            while (node) {
                if (node->Collector == collector) {
                    return node;
                }
                node = node->Next;
            }
            return nullptr;
        }

        const NkMemorySystem::NkGcNode* NkMemorySystem::FindGcNodeLocked(
            const NkGarbageCollector* collector) const noexcept {
            const NkGcNode* node = mGcHead;
            while (node) {
                if (node->Collector == collector) {
                    return node;
                }
                node = node->Next;
            }
            return nullptr;
        }

        void NkMemorySystem::CopyGcName(nk_char* destination,
                                        nk_size destinationSize,
                                        const nk_char* source) noexcept {
            if (!destination || destinationSize == 0u) {
                return;
            }

            if (!source) {
                destination[0] = '\0';
                return;
            }

            nk_size i = 0u;
            for (; i + 1u < destinationSize && source[i] != '\0'; ++i) {
                destination[i] = source[i];
            }
            destination[i] = '\0';
        }

    } // namespace memory
} // namespace nkentseu
