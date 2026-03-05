#include "NKMemory/NkMemory.h"

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
            mGc(NkGarbageCollector::Instance()) {}

        void NkMemorySystem::Initialize(NkAllocator* allocator) noexcept {
            core::NkScopedSpinLock guard(mLock);
            mAllocator = allocator ? allocator : &NkGetDefaultAllocator();
            mInitialized = true;
        }

        void NkMemorySystem::Shutdown(nk_bool reportLeaks) noexcept {
            NkAllocationNode* leakedHead = nullptr;

            {
                core::NkScopedSpinLock guard(mLock);
                leakedHead = mHead;
                mHead = nullptr;
                mInitialized = false;
                mLiveBytes = 0u;
                mLiveAllocations = 0u;
            }

            while (leakedHead) {
                NkAllocationNode* next = leakedHead->next;

                if (reportLeaks) {
                    nk_char line[512];
                    snprintf(line,
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
            core::NkScopedSpinLock guard(mLock);
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
            core::NkScopedSpinLock guard(mLock);
            NkMemoryStats stats{};
            stats.liveBytes = mLiveBytes;
            stats.peakBytes = mPeakBytes;
            stats.liveAllocations = mLiveAllocations;
            stats.totalAllocations = mTotalAllocations;
            return stats;
        }

        void NkMemorySystem::DumpLeaks() noexcept {
            core::NkScopedSpinLock guard(mLock);
            NkAllocationNode* current = mHead;
            if (!current) {
                LogLine("[NKMemory] no leaks detected.");
                return;
            }

            while (current) {
                nk_char line[512];
                snprintf(line,
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
            node->next = nullptr;

            core::NkScopedSpinLock guard(mLock);
            node->next = mHead;
            mHead = node;

            ++mLiveAllocations;
            ++mTotalAllocations;
            mLiveBytes += size;
            if (mLiveBytes > mPeakBytes) {
                mPeakBytes = mLiveBytes;
            }
        }

        NkMemorySystem::NkAllocationNode* NkMemorySystem::FindAndDetach(void* userPtr) noexcept {
            core::NkScopedSpinLock guard(mLock);

            NkAllocationNode* prev = nullptr;
            NkAllocationNode* cur = mHead;
            while (cur) {
                if (cur->userPtr == userPtr) {
                    if (prev) {
                        prev->next = cur->next;
                    } else {
                        mHead = cur->next;
                    }
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

                prev = cur;
                cur = cur->next;
            }
            return nullptr;
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
            fprintf(stderr, "%s\n", message);
        }

    } // namespace memory
} // namespace nkentseu
