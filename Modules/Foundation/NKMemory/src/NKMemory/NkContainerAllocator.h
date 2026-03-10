#pragma once

#ifndef NKENTSEU_MEMORY_NKCONTAINERALLOCATOR_H_INCLUDED
#define NKENTSEU_MEMORY_NKCONTAINERALLOCATOR_H_INCLUDED

#include "NKMemory/NkAllocator.h"
#include "NKCore/NkAtomic.h"

namespace nkentseu {
    namespace memory {

        class NKENTSEU_MEMORY_API NkContainerAllocator final : public NkAllocator {
            public:
                struct NkStats {
                    nk_size PageCount;
                    nk_size ReservedBytes;
                    nk_size SmallUsedBlocks;
                    nk_size SmallFreeBlocks;
                    nk_size LargeAllocationCount;
                    nk_size LargeAllocationBytes;
                };

                static constexpr nk_uint32 NK_CONTAINER_CLASS_COUNT = 13u;
                static constexpr nk_uint32 NK_CONTAINER_LARGE_CLASS = 0xFFFFFFFFu;
                static constexpr nk_size NK_CONTAINER_SMALL_ALIGNMENT = 16u;
                static constexpr nk_size NK_CONTAINER_TLS_CACHE_LIMIT = 32u;
                static constexpr nk_size NK_CONTAINER_TLS_CACHE_RETAIN = 8u;

                explicit NkContainerAllocator(NkAllocator* backingAllocator = nullptr,
                                              nk_size pageSize = 64u * 1024u,
                                              const nk_char* name = "NkContainerAllocator");
                ~NkContainerAllocator() override;

                NkContainerAllocator(const NkContainerAllocator&) = delete;
                NkContainerAllocator& operator=(const NkContainerAllocator&) = delete;

                Pointer Allocate(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                void Deallocate(Pointer ptr) override;
                Pointer Reallocate(Pointer ptr,
                                   SizeType oldSize,
                                   SizeType newSize,
                                   SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                Pointer Calloc(SizeType size, SizeType alignment = NK_MEMORY_DEFAULT_ALIGNMENT) override;
                const nk_char* Name() const noexcept override;
                void Reset() noexcept override;

                [[nodiscard]] NkStats GetStats() const noexcept;

            private:
                struct NkFreeNode {
                    NkFreeNode* Next;
                };

                struct NkPageHeader {
                    NkPageHeader* Next;
                    nk_size RawSize;
                    nk_uint32 ClassIndex;
                    nk_uint32 Reserved;
                };

                struct NkContainerAllocHeader {
                    nk_size Offset;
                    nk_size RequestedSize;
                    nk_uint32 ClassIndex;
                    nk_uint32 Reserved;
                    NkContainerAllocHeader* PrevLarge;
                    NkContainerAllocHeader* NextLarge;
                };

                struct NkSizeClass {
                    nk_size UserSize;
                    nk_size SlotSize;
                    NkFreeNode* FreeList;
                    NkPageHeader* Pages;
                    nk_size TotalSlots;
                    NkAtomicSize UsedSlots;
                };

                nk_bool IsSmallAlignment(SizeType alignment) const noexcept;
                nk_size HeaderOffset() const noexcept;
                nk_uint32 FindClassIndex(SizeType size) const noexcept;

                nk_bool AllocatePageForClassLocked(nk_uint32 classIndex) noexcept;
                Pointer AllocateSmallLocked(SizeType size, nk_uint32 classIndex) noexcept;
                Pointer AllocateLargeLocked(SizeType size, SizeType alignment) noexcept;
                Pointer TryAllocateSmallFromTls(SizeType size, nk_uint32 classIndex) noexcept;
                void TryDeallocateSmallToTls(Pointer ptr, NkContainerAllocHeader* header) noexcept;

                void DeallocateSmallLocked(Pointer ptr, NkContainerAllocHeader* header) noexcept;
                void DeallocateLargeLocked(Pointer ptr, NkContainerAllocHeader* header) noexcept;

                void ReleaseAllPagesLocked() noexcept;
                void ReleaseAllLargeLocked() noexcept;
                void RegisterAllocatorInstance() noexcept;
                void UnregisterAllocatorInstance() noexcept;
                static nk_bool IsAllocatorInstanceLive(const NkContainerAllocator* allocator) noexcept;

                NkAllocator* mBacking;
                nk_size mPageSize;
                nk_size mHeaderOffset;
                NkSizeClass mClasses[NK_CONTAINER_CLASS_COUNT];
                NkContainerAllocHeader* mLargeHead;
                nk_size mLargeAllocationCount;
                nk_size mLargeAllocationBytes;
                NkAtomicUInt64 mGeneration;
                NkContainerAllocator* mRegistryNext;
                mutable NkSpinLock mClassLocks[NK_CONTAINER_CLASS_COUNT];
                mutable NkSpinLock mLargeLock;
        };

    } // namespace memory
} // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKCONTAINERALLOCATOR_H_INCLUDED
