#include "NKMemory/NkContainerAllocator.h"

namespace nkentseu {
    namespace memory {

        namespace {

            static constexpr nk_size NK_CONTAINER_CLASS_SIZES[NkContainerAllocator::NK_CONTAINER_CLASS_COUNT] = {
                16u,  32u,  64u,  96u,  128u, 192u, 256u,
                384u, 512u, 768u, 1024u, 1536u, 2048u
            };

            inline nk_size NkAlignUpPow2(nk_size value, nk_size alignment) noexcept {
                return (value + (alignment - 1u)) & ~(alignment - 1u);
            }

            inline void NkAtomicSaturatingDecrement(NkAtomicSize& value) noexcept {
                nk_size expected = value.Load(NkMemoryOrder::NK_RELAXED);
                while (expected > 0u) {
                    if (value.CompareExchangeWeak(expected,
                                                  expected - 1u,
                                                  NkMemoryOrder::NK_RELAXED)) {
                        return;
                    }
                }
            }

            struct NkContainerTlsCacheSlot {
                NkContainerAllocator* Owner;
                nk_uint64 Generation;
                void* Head;
                nk_size Count;
            };

            thread_local NkContainerTlsCacheSlot gNkContainerTls[NkContainerAllocator::NK_CONTAINER_CLASS_COUNT] = {};

            inline NkSpinLock& NkContainerAllocatorRegistryLock() noexcept {
                static NkSpinLock lock;
                return lock;
            }

            inline NkContainerAllocator*& NkContainerAllocatorRegistryHead() noexcept {
                static NkContainerAllocator* head = nullptr;
                return head;
            }

        } // namespace

        NkContainerAllocator::NkContainerAllocator(NkAllocator* backingAllocator,
                                                   nk_size pageSize,
                                                   const nk_char* name)
            : NkAllocator(name ? name : "NkContainerAllocator"),
              mBacking(backingAllocator ? backingAllocator : &NkGetDefaultAllocator()),
              mPageSize(pageSize < 4096u ? 4096u : pageSize),
              mHeaderOffset(NkAlignUpPow2(sizeof(NkContainerAllocHeader), NK_CONTAINER_SMALL_ALIGNMENT)),
              mClasses(),
              mLargeHead(nullptr),
              mLargeAllocationCount(0u),
              mLargeAllocationBytes(0u),
              mGeneration(1u),
              mRegistryNext(nullptr),
              mClassLocks(),
              mLargeLock() {
            for (nk_uint32 i = 0u; i < NK_CONTAINER_CLASS_COUNT; ++i) {
                mClasses[i].UserSize = NK_CONTAINER_CLASS_SIZES[i];
                mClasses[i].SlotSize = NkAlignUpPow2(mHeaderOffset + mClasses[i].UserSize,
                                                     NK_CONTAINER_SMALL_ALIGNMENT);
                mClasses[i].FreeList = nullptr;
                mClasses[i].Pages = nullptr;
                mClasses[i].TotalSlots = 0u;
                mClasses[i].UsedSlots.Store(0u, NkMemoryOrder::NK_RELAXED);
            }

            RegisterAllocatorInstance();
        }

        NkContainerAllocator::~NkContainerAllocator() {
            Reset();
            UnregisterAllocatorInstance();
        }

        void NkContainerAllocator::RegisterAllocatorInstance() noexcept {
            NkScopedSpinLock guard(NkContainerAllocatorRegistryLock());
            mRegistryNext = NkContainerAllocatorRegistryHead();
            NkContainerAllocatorRegistryHead() = this;
        }

        void NkContainerAllocator::UnregisterAllocatorInstance() noexcept {
            NkScopedSpinLock guard(NkContainerAllocatorRegistryLock());

            NkContainerAllocator* previous = nullptr;
            NkContainerAllocator* current = NkContainerAllocatorRegistryHead();
            while (current) {
                if (current == this) {
                    if (previous) {
                        previous->mRegistryNext = current->mRegistryNext;
                    } else {
                        NkContainerAllocatorRegistryHead() = current->mRegistryNext;
                    }
                    break;
                }
                previous = current;
                current = current->mRegistryNext;
            }

            mRegistryNext = nullptr;
        }

        nk_bool NkContainerAllocator::IsAllocatorInstanceLive(const NkContainerAllocator* allocator) noexcept {
            if (!allocator) {
                return false;
            }

            NkScopedSpinLock guard(NkContainerAllocatorRegistryLock());
            NkContainerAllocator* current = NkContainerAllocatorRegistryHead();
            while (current) {
                if (current == allocator) {
                    return true;
                }
                current = current->mRegistryNext;
            }
            return false;
        }

        nk_bool NkContainerAllocator::IsSmallAlignment(SizeType alignment) const noexcept {
            return NkIsPowerOfTwo(alignment) && alignment <= NK_CONTAINER_SMALL_ALIGNMENT;
        }

        nk_size NkContainerAllocator::HeaderOffset() const noexcept {
            return mHeaderOffset;
        }

        nk_uint32 NkContainerAllocator::FindClassIndex(SizeType size) const noexcept {
            for (nk_uint32 i = 0u; i < NK_CONTAINER_CLASS_COUNT; ++i) {
                if (size <= mClasses[i].UserSize) {
                    return i;
                }
            }
            return NK_CONTAINER_LARGE_CLASS;
        }

        NkAllocator::Pointer NkContainerAllocator::TryAllocateSmallFromTls(SizeType size,
                                                                            nk_uint32 classIndex) noexcept {
            if (classIndex >= NK_CONTAINER_CLASS_COUNT) {
                return nullptr;
            }

            NkContainerTlsCacheSlot& slot = gNkContainerTls[classIndex];
            const nk_uint64 generation = mGeneration.Load(NkMemoryOrder::NK_ACQUIRE);

            if (slot.Owner == this) {
                if (slot.Generation != generation) {
                    slot.Generation = generation;
                    slot.Head = nullptr;
                    slot.Count = 0u;
                    return nullptr;
                }

                if (slot.Head) {
                    auto* node = static_cast<NkFreeNode*>(slot.Head);
                    slot.Head = node->Next;
                    if (slot.Count > 0u) {
                        --slot.Count;
                    }

                    nk_uint8* slotStart = reinterpret_cast<nk_uint8*>(node);
                    nk_uint8* user = slotStart + HeaderOffset();
                    auto* header = reinterpret_cast<NkContainerAllocHeader*>(
                        user - sizeof(NkContainerAllocHeader));
                    header->Offset = HeaderOffset();
                    header->RequestedSize = size;
                    header->ClassIndex = classIndex;
                    header->Reserved = 0u;
                    header->PrevLarge = nullptr;
                    header->NextLarge = nullptr;

                    mClasses[classIndex].UsedSlots.FetchAdd(1u, NkMemoryOrder::NK_RELAXED);
                    return user;
                }

                return nullptr;
            }

            if (slot.Owner == nullptr || !IsAllocatorInstanceLive(slot.Owner)) {
                slot.Owner = this;
                slot.Generation = generation;
                slot.Head = nullptr;
                slot.Count = 0u;
            }

            return nullptr;
        }

        NkAllocator::Pointer NkContainerAllocator::Allocate(SizeType size, SizeType alignment) {
            if (size == 0u) {
                return nullptr;
            }

            if (!NkIsPowerOfTwo(alignment)) {
                alignment = NK_MEMORY_DEFAULT_ALIGNMENT;
            }
            if (alignment < NK_MEMORY_DEFAULT_ALIGNMENT) {
                alignment = NK_MEMORY_DEFAULT_ALIGNMENT;
            }

            const nk_uint32 classIndex = FindClassIndex(size);

            if (classIndex != NK_CONTAINER_LARGE_CLASS && IsSmallAlignment(alignment)) {
                if (Pointer cached = TryAllocateSmallFromTls(size, classIndex)) {
                    return cached;
                }

                NkScopedSpinLock guard(mClassLocks[classIndex]);
                return AllocateSmallLocked(size, classIndex);
            }

            NkScopedSpinLock guard(mLargeLock);
            return AllocateLargeLocked(size, alignment);
        }

        void NkContainerAllocator::TryDeallocateSmallToTls(Pointer ptr,
                                                           NkContainerAllocHeader* header) noexcept {
            if (!ptr || !header) {
                return;
            }

            const nk_uint32 classIndex = header->ClassIndex;
            if (classIndex >= NK_CONTAINER_CLASS_COUNT) {
                return;
            }

            NkContainerTlsCacheSlot& slot = gNkContainerTls[classIndex];
            const nk_uint64 generation = mGeneration.Load(NkMemoryOrder::NK_ACQUIRE);

            if (slot.Owner == this) {
                if (slot.Generation != generation) {
                    slot.Generation = generation;
                    slot.Head = nullptr;
                    slot.Count = 0u;
                }
            } else if (slot.Owner == nullptr || !IsAllocatorInstanceLive(slot.Owner)) {
                slot.Owner = this;
                slot.Generation = generation;
                slot.Head = nullptr;
                slot.Count = 0u;
            }

            if (slot.Owner == this && slot.Generation == generation) {
                NkAtomicSaturatingDecrement(mClasses[classIndex].UsedSlots);

                nk_uint8* slotStart = static_cast<nk_uint8*>(ptr) - header->Offset;
                auto* node = reinterpret_cast<NkFreeNode*>(slotStart);
                node->Next = static_cast<NkFreeNode*>(slot.Head);
                slot.Head = node;
                ++slot.Count;

                if (slot.Count > NK_CONTAINER_TLS_CACHE_LIMIT) {
                    NkScopedSpinLock guard(mClassLocks[classIndex]);
                    NkSizeClass& sizeClass = mClasses[classIndex];
                    while (slot.Count > NK_CONTAINER_TLS_CACHE_RETAIN) {
                        auto* cached = static_cast<NkFreeNode*>(slot.Head);
                        if (!cached) {
                            slot.Count = 0u;
                            break;
                        }

                        slot.Head = cached->Next;
                        --slot.Count;

                        cached->Next = sizeClass.FreeList;
                        sizeClass.FreeList = cached;
                    }
                }
                return;
            }

            NkScopedSpinLock guard(mClassLocks[classIndex]);
            DeallocateSmallLocked(ptr, header);
        }

        void NkContainerAllocator::Deallocate(Pointer ptr) {
            if (!ptr) {
                return;
            }

            auto* header = reinterpret_cast<NkContainerAllocHeader*>(
                static_cast<nk_uint8*>(ptr) - sizeof(NkContainerAllocHeader));

            if (header->ClassIndex == NK_CONTAINER_LARGE_CLASS) {
                NkScopedSpinLock guard(mLargeLock);
                DeallocateLargeLocked(ptr, header);
                return;
            }

            if (header->ClassIndex < NK_CONTAINER_CLASS_COUNT) {
                TryDeallocateSmallToTls(ptr, header);
                return;
            }

            nk_uint8* raw = static_cast<nk_uint8*>(ptr) - header->Offset;
            if (mBacking && raw) {
                mBacking->Deallocate(raw);
            }
        }

        NkAllocator::Pointer NkContainerAllocator::Reallocate(Pointer ptr,
                                                              SizeType oldSize,
                                                              SizeType newSize,
                                                              SizeType alignment) {
            if (!ptr) {
                return Allocate(newSize, alignment);
            }
            if (newSize == 0u) {
                Deallocate(ptr);
                return nullptr;
            }

            auto* header = reinterpret_cast<NkContainerAllocHeader*>(
                static_cast<nk_uint8*>(ptr) - sizeof(NkContainerAllocHeader));
            const SizeType knownOldSize = header->RequestedSize > 0u ? header->RequestedSize : oldSize;

            if (newSize <= knownOldSize) {
                header->RequestedSize = newSize;
                return ptr;
            }

            Pointer replacement = Allocate(newSize, alignment);
            if (!replacement) {
                return nullptr;
            }

            NkMemCopy(replacement, ptr, knownOldSize);
            Deallocate(ptr);
            return replacement;
        }

        NkAllocator::Pointer NkContainerAllocator::Calloc(SizeType size, SizeType alignment) {
            Pointer ptr = Allocate(size, alignment);
            if (ptr) {
                NkMemZero(ptr, size);
            }
            return ptr;
        }

        const nk_char* NkContainerAllocator::Name() const noexcept {
            return mName;
        }

        void NkContainerAllocator::Reset() noexcept {
            const nk_uint64 nextGeneration = mGeneration.Load(NkMemoryOrder::NK_RELAXED) + 1u;
            mGeneration.Store(nextGeneration, NkMemoryOrder::NK_RELEASE);

            // Clear current thread cache entries for this allocator.
            for (nk_uint32 i = 0u; i < NK_CONTAINER_CLASS_COUNT; ++i) {
                NkContainerTlsCacheSlot& slot = gNkContainerTls[i];
                if (slot.Owner == this) {
                    slot.Generation = nextGeneration;
                    slot.Head = nullptr;
                    slot.Count = 0u;
                }
            }

            {
                NkScopedSpinLock guard(mLargeLock);
                ReleaseAllLargeLocked();
            }

            for (nk_uint32 i = 0u; i < NK_CONTAINER_CLASS_COUNT; ++i) {
                NkScopedSpinLock guard(mClassLocks[i]);
                NkPageHeader* page = mClasses[i].Pages;
                while (page) {
                    NkPageHeader* next = page->Next;
                    if (mBacking) {
                        mBacking->Deallocate(page);
                    }
                    page = next;
                }

                mClasses[i].FreeList = nullptr;
                mClasses[i].Pages = nullptr;
                mClasses[i].TotalSlots = 0u;
                mClasses[i].UsedSlots.Store(0u, NkMemoryOrder::NK_RELAXED);
            }
        }

        NkContainerAllocator::NkStats NkContainerAllocator::GetStats() const noexcept {
            NkStats stats{};

            nk_size totalSlots = 0u;
            nk_size usedSlots = 0u;

            for (nk_uint32 i = 0u; i < NK_CONTAINER_CLASS_COUNT; ++i) {
                NkScopedSpinLock guard(mClassLocks[i]);

                totalSlots += mClasses[i].TotalSlots;
                usedSlots += mClasses[i].UsedSlots.Load(NkMemoryOrder::NK_RELAXED);

                NkPageHeader* page = mClasses[i].Pages;
                while (page) {
                    ++stats.PageCount;
                    stats.ReservedBytes += page->RawSize;
                    page = page->Next;
                }
            }

            {
                NkScopedSpinLock guard(mLargeLock);
                stats.LargeAllocationCount = mLargeAllocationCount;
                stats.LargeAllocationBytes = mLargeAllocationBytes;
            }

            stats.SmallUsedBlocks = usedSlots;
            stats.SmallFreeBlocks = totalSlots >= usedSlots ? (totalSlots - usedSlots) : 0u;
            return stats;
        }

        nk_bool NkContainerAllocator::AllocatePageForClassLocked(nk_uint32 classIndex) noexcept {
            if (classIndex >= NK_CONTAINER_CLASS_COUNT || !mBacking) {
                return false;
            }

            NkSizeClass& sizeClass = mClasses[classIndex];
            const nk_size pageHeaderSize = NkAlignUpPow2(sizeof(NkPageHeader), NK_CONTAINER_SMALL_ALIGNMENT);
            nk_size pageBytes = mPageSize;
            const nk_size minimumBytes = pageHeaderSize + sizeClass.SlotSize;
            if (pageBytes < minimumBytes) {
                pageBytes = minimumBytes;
            }

            nk_size slotCount = (pageBytes - pageHeaderSize) / sizeClass.SlotSize;
            if (slotCount == 0u) {
                slotCount = 1u;
                pageBytes = pageHeaderSize + sizeClass.SlotSize;
            }

            nk_uint8* raw = static_cast<nk_uint8*>(mBacking->Allocate(pageBytes, NK_CONTAINER_SMALL_ALIGNMENT));
            if (!raw) {
                return false;
            }

            auto* page = reinterpret_cast<NkPageHeader*>(raw);
            page->Next = sizeClass.Pages;
            page->RawSize = pageBytes;
            page->ClassIndex = classIndex;
            page->Reserved = 0u;
            sizeClass.Pages = page;

            nk_uint8* slotBase = raw + pageHeaderSize;
            for (nk_size i = 0u; i < slotCount; ++i) {
                auto* node = reinterpret_cast<NkFreeNode*>(slotBase + (i * sizeClass.SlotSize));
                node->Next = sizeClass.FreeList;
                sizeClass.FreeList = node;
            }

            sizeClass.TotalSlots += slotCount;
            return true;
        }

        NkAllocator::Pointer NkContainerAllocator::AllocateSmallLocked(SizeType size, nk_uint32 classIndex) noexcept {
            NkSizeClass& sizeClass = mClasses[classIndex];
            if (!sizeClass.FreeList) {
                if (!AllocatePageForClassLocked(classIndex)) {
                    return nullptr;
                }
            }

            NkFreeNode* node = sizeClass.FreeList;
            sizeClass.FreeList = node->Next;

            nk_uint8* slotStart = reinterpret_cast<nk_uint8*>(node);
            nk_uint8* user = slotStart + HeaderOffset();
            auto* header = reinterpret_cast<NkContainerAllocHeader*>(user - sizeof(NkContainerAllocHeader));
            header->Offset = HeaderOffset();
            header->RequestedSize = size;
            header->ClassIndex = classIndex;
            header->Reserved = 0u;
            header->PrevLarge = nullptr;
            header->NextLarge = nullptr;

            sizeClass.UsedSlots.FetchAdd(1u, NkMemoryOrder::NK_RELAXED);
            return user;
        }

        NkAllocator::Pointer NkContainerAllocator::AllocateLargeLocked(SizeType size, SizeType alignment) noexcept {
            if (!mBacking) {
                return nullptr;
            }

            if (!NkIsPowerOfTwo(alignment)) {
                alignment = NK_MEMORY_DEFAULT_ALIGNMENT;
            }
            if (alignment < NK_MEMORY_DEFAULT_ALIGNMENT) {
                alignment = NK_MEMORY_DEFAULT_ALIGNMENT;
            }

            const nk_size totalBytes = size + alignment + sizeof(NkContainerAllocHeader);
            nk_uint8* raw = static_cast<nk_uint8*>(mBacking->Allocate(totalBytes, NK_MEMORY_DEFAULT_ALIGNMENT));
            if (!raw) {
                return nullptr;
            }

            const nk_size rawAddress = reinterpret_cast<nk_size>(raw);
            const nk_size userAddress = NkAlignUp(rawAddress + sizeof(NkContainerAllocHeader), alignment);
            nk_uint8* user = reinterpret_cast<nk_uint8*>(userAddress);

            auto* header = reinterpret_cast<NkContainerAllocHeader*>(user - sizeof(NkContainerAllocHeader));
            header->Offset = userAddress - rawAddress;
            header->RequestedSize = size;
            header->ClassIndex = NK_CONTAINER_LARGE_CLASS;
            header->Reserved = 0u;
            header->PrevLarge = nullptr;
            header->NextLarge = mLargeHead;

            if (mLargeHead) {
                mLargeHead->PrevLarge = header;
            }
            mLargeHead = header;

            ++mLargeAllocationCount;
            mLargeAllocationBytes += size;
            return user;
        }

        void NkContainerAllocator::DeallocateSmallLocked(Pointer ptr, NkContainerAllocHeader* header) noexcept {
            if (!ptr || !header) {
                return;
            }
            const nk_uint32 classIndex = header->ClassIndex;
            if (classIndex >= NK_CONTAINER_CLASS_COUNT) {
                return;
            }

            NkSizeClass& sizeClass = mClasses[classIndex];
            nk_uint8* slotStart = static_cast<nk_uint8*>(ptr) - header->Offset;
            auto* node = reinterpret_cast<NkFreeNode*>(slotStart);
            node->Next = sizeClass.FreeList;
            sizeClass.FreeList = node;

            NkAtomicSaturatingDecrement(sizeClass.UsedSlots);
        }

        void NkContainerAllocator::DeallocateLargeLocked(Pointer ptr, NkContainerAllocHeader* header) noexcept {
            if (!ptr || !header || !mBacking) {
                return;
            }

            if (header->PrevLarge) {
                header->PrevLarge->NextLarge = header->NextLarge;
            } else {
                mLargeHead = header->NextLarge;
            }
            if (header->NextLarge) {
                header->NextLarge->PrevLarge = header->PrevLarge;
            }

            if (mLargeAllocationCount > 0u) {
                --mLargeAllocationCount;
            }
            if (mLargeAllocationBytes >= header->RequestedSize) {
                mLargeAllocationBytes -= header->RequestedSize;
            } else {
                mLargeAllocationBytes = 0u;
            }

            nk_uint8* raw = static_cast<nk_uint8*>(ptr) - header->Offset;
            mBacking->Deallocate(raw);
        }

        void NkContainerAllocator::ReleaseAllPagesLocked() noexcept {
            if (!mBacking) {
                return;
            }

            for (nk_uint32 i = 0u; i < NK_CONTAINER_CLASS_COUNT; ++i) {
                NkPageHeader* page = mClasses[i].Pages;
                while (page) {
                    NkPageHeader* next = page->Next;
                    mBacking->Deallocate(page);
                    page = next;
                }

                mClasses[i].FreeList = nullptr;
                mClasses[i].Pages = nullptr;
                mClasses[i].TotalSlots = 0u;
                mClasses[i].UsedSlots.Store(0u, NkMemoryOrder::NK_RELAXED);
            }
        }

        void NkContainerAllocator::ReleaseAllLargeLocked() noexcept {
            if (!mBacking) {
                return;
            }

            NkContainerAllocHeader* current = mLargeHead;
            while (current) {
                NkContainerAllocHeader* next = current->NextLarge;
                nk_uint8* user = reinterpret_cast<nk_uint8*>(current) + sizeof(NkContainerAllocHeader);
                nk_uint8* raw = user - current->Offset;
                mBacking->Deallocate(raw);
                current = next;
            }

            mLargeHead = nullptr;
            mLargeAllocationCount = 0u;
            mLargeAllocationBytes = 0u;
        }

    } // namespace memory
} // namespace nkentseu
