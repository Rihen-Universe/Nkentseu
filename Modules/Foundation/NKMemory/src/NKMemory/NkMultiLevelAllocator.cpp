// NkMultiLevelAllocator.cpp
#include "NkMultiLevelAllocator.h"
#include "NKMemory/NkFunction.h"
#include "NKMemory/NkUtils.h"
#include "NKPlatform/NkFoundationLog.h"
#include <cstdlib>

namespace nkentseu::memory {

    namespace {
        static constexpr nk_uint64 NK_MULTI_LEVEL_BLOCK_MAGIC = 0x4E4B4D4C424C4B31ull; // "NKMLBLK1"
        static constexpr nk_uint64 NK_MULTI_LEVEL_LARGE_MAGIC = 0x4E4B4D4C41524745ull; // legacy
        
        enum class NkAllocTier : nk_uint8 {
            Tiny = 0,
            Small = 1,
            Medium = 2,
            Large = 3
        };
        
        struct NkDispatchHeader {
            nk_uint64 magic;
            nk_size requestedSize;
            nk_size offsetToBase;
            nk_uint8 tier;
            nk_uint8 reserved[7];
        };
        
        struct NkLegacyLargeAllocHeader {
            nk_uint64 magic;
            nk_size size;
        };
        
        static nk_size NkSanitizeAlignment(nk_size alignment) noexcept {
            if (alignment == 0u) {
                return NK_MEMORY_DEFAULT_ALIGNMENT;
            }
            if ((alignment & (alignment - 1u)) != 0u) {
                return NK_MEMORY_DEFAULT_ALIGNMENT;
            }
            return alignment;
        }
        
        static NkAllocTier NkChooseTier(nk_size totalBytes) noexcept {
            if (totalBytes <= NkMultiLevelAllocator::TINY_SIZE) {
                return NkAllocTier::Tiny;
            }
            if (totalBytes <= NkMultiLevelAllocator::SMALL_SIZE) {
                return NkAllocTier::Small;
            }
            if (totalBytes <= NkMultiLevelAllocator::MEDIUM_THRESHOLD) {
                return NkAllocTier::Medium;
            }
            return NkAllocTier::Large;
        };
    }
    
    float32 NkMultiLevelAllocator::Stats::GetFragmentation() const noexcept {
        const nk_size used = tiny.allocated + small.allocated + medium.allocated + largeBytes;
        const nk_size capacity = tiny.capacity + small.capacity + medium.capacity + largeBytes;
        if (capacity == 0u) {
            return 0.0f;
        }
        return 1.0f - (static_cast<float32>(used) / static_cast<float32>(capacity));
    }

    NkMultiLevelAllocator::NkMultiLevelAllocator() noexcept 
        : NkAllocator("NkMultiLevelAllocator"),
          mTinyPool(NkMakeUnique<NkFixedPoolAllocator<TINY_SIZE, TINY_COUNT>>()),
          mSmallPool(NkMakeUnique<NkFixedPoolAllocator<SMALL_SIZE, SMALL_COUNT>>()),
          mMediumPool(NkMakeUnique<NkVariablePoolAllocator>()) {}
    
    NkMultiLevelAllocator::~NkMultiLevelAllocator() = default;
    
    NkMultiLevelAllocator::Pointer NkMultiLevelAllocator::Allocate(SizeType size, SizeType alignment) {
        if (size == 0u) {
            return nullptr;
        }
        
        const SizeType safeAlignment = NkSanitizeAlignment(alignment);
        const SizeType totalBytes = sizeof(NkDispatchHeader) + size + safeAlignment;
        const NkAllocTier tier = NkChooseTier(totalBytes);
        
        void* rawBase = nullptr;
        switch (tier) {
            case NkAllocTier::Tiny:
                rawBase = mTinyPool->Allocate(totalBytes, alignof(void*));
                break;
            case NkAllocTier::Small:
                rawBase = mSmallPool->Allocate(totalBytes, alignof(void*));
                break;
            case NkAllocTier::Medium:
                rawBase = mMediumPool->Allocate(totalBytes, alignof(void*));
                break;
            case NkAllocTier::Large:
                rawBase = ::malloc(totalBytes);
                break;
        }
        
        if (!rawBase) {
            return nullptr;
        }
        
        nk_uint8* base = static_cast<nk_uint8*>(rawBase);
        const nk_uintptr firstUser = reinterpret_cast<nk_uintptr>(base + sizeof(NkDispatchHeader));
        const nk_uintptr alignedUser = static_cast<nk_uintptr>(
            NkAlignUp(static_cast<nk_size>(firstUser), safeAlignment)
        );
        nk_uint8* userPtr = reinterpret_cast<nk_uint8*>(alignedUser);
        
        NkDispatchHeader* header = reinterpret_cast<NkDispatchHeader*>(userPtr - sizeof(NkDispatchHeader));
        header->magic = NK_MULTI_LEVEL_BLOCK_MAGIC;
        header->requestedSize = size;
        header->offsetToBase = static_cast<SizeType>(userPtr - base);
        header->tier = static_cast<nk_uint8>(tier);
        NkMemZero(header->reserved, sizeof(header->reserved));
        
        if (tier == NkAllocTier::Large) {
            NkScopedSpinLock guard(mLock);
            mLarge.totalBytes += size;
            ++mLarge.allocationCount;
        }
        
        return userPtr;
    }
    
    void NkMultiLevelAllocator::Deallocate(Pointer ptr) {
        if (!ptr) {
            return;
        }
        
        nk_uint8* userPtr = static_cast<nk_uint8*>(ptr);
        NkDispatchHeader* header = reinterpret_cast<NkDispatchHeader*>(userPtr - sizeof(NkDispatchHeader));
        
        if (header->magic != NK_MULTI_LEVEL_BLOCK_MAGIC) {
            // Backward compatibility for old large allocations.
            NkLegacyLargeAllocHeader* legacy = reinterpret_cast<NkLegacyLargeAllocHeader*>(ptr) - 1;
            if (legacy->magic == NK_MULTI_LEVEL_LARGE_MAGIC) {
                {
                    NkScopedSpinLock guard(mLock);
                    if (mLarge.totalBytes >= legacy->size) {
                        mLarge.totalBytes -= legacy->size;
                    } else {
                        mLarge.totalBytes = 0;
                    }
                    if (mLarge.allocationCount > 0) {
                        --mLarge.allocationCount;
                    }
                }
                ::free(legacy);
                return;
            }
            
            // Fallback for pointers that were not wrapped.
            NkAllocator* allocator = FindAllocatorFor(ptr);
            if (allocator) {
                allocator->Deallocate(ptr);
            } else {
                ::free(ptr);
            }
            return;
        }
        
        nk_uint8* base = userPtr - header->offsetToBase;
        const NkAllocTier tier = static_cast<NkAllocTier>(header->tier);
        
        switch (tier) {
            case NkAllocTier::Tiny:
                mTinyPool->Deallocate(base);
                break;
            case NkAllocTier::Small:
                mSmallPool->Deallocate(base);
                break;
            case NkAllocTier::Medium:
                mMediumPool->Deallocate(base);
                break;
            case NkAllocTier::Large: {
                NkScopedSpinLock guard(mLock);
                if (mLarge.totalBytes >= header->requestedSize) {
                    mLarge.totalBytes -= header->requestedSize;
                } else {
                    mLarge.totalBytes = 0u;
                }
                if (mLarge.allocationCount > 0u) {
                    --mLarge.allocationCount;
                }
                ::free(base);
                break;
            }
        }
    }
    
    NkMultiLevelAllocator::Pointer NkMultiLevelAllocator::Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize, SizeType alignment) {
        if (!ptr) {
            return Allocate(newSize, alignment);
        }
        if (newSize == 0u) {
            Deallocate(ptr);
            return nullptr;
        }
        
        SizeType copySize = oldSize;
        NkDispatchHeader* header = reinterpret_cast<NkDispatchHeader*>(
            static_cast<nk_uint8*>(ptr) - sizeof(NkDispatchHeader)
        );
        if (header->magic == NK_MULTI_LEVEL_BLOCK_MAGIC) {
            copySize = header->requestedSize;
        }
        if (copySize > newSize) {
            copySize = newSize;
        }
        
        void* newPtr = Allocate(newSize, alignment);
        if (newPtr) {
            NkMemCopy(newPtr, ptr, copySize);
            Deallocate(ptr);
        }
        return newPtr;
    }
    
    void NkMultiLevelAllocator::Reset() noexcept {
        mTinyPool->Reset();
        mSmallPool->Reset();
        mMediumPool->Reset();
        NkScopedSpinLock guard(mLock);
        mLarge.totalBytes = 0u;
        mLarge.allocationCount = 0u;
    }
    
    NkMultiLevelAllocator::Stats NkMultiLevelAllocator::GetStats() const noexcept {
        Stats stats{};
        
        if (mTinyPool) {
            const nk_size freeBlocks = mTinyPool->GetNumFreeBlocks();
            stats.tiny.capacity = TINY_SIZE * TINY_COUNT;
            stats.tiny.allocated = (TINY_COUNT - freeBlocks) * TINY_SIZE;
            stats.tiny.usage = stats.tiny.capacity > 0u
                ? static_cast<float32>(stats.tiny.allocated) / static_cast<float32>(stats.tiny.capacity)
                : 0.0f;
        }
        
        if (mSmallPool) {
            const nk_size freeBlocks = mSmallPool->GetNumFreeBlocks();
            stats.small.capacity = SMALL_SIZE * SMALL_COUNT;
            stats.small.allocated = (SMALL_COUNT - freeBlocks) * SMALL_SIZE;
            stats.small.usage = stats.small.capacity > 0u
                ? static_cast<float32>(stats.small.allocated) / static_cast<float32>(stats.small.capacity)
                : 0.0f;
        }
        
        if (mMediumPool) {
            stats.medium.allocated = mMediumPool->GetLiveBytes();
            stats.medium.capacity = stats.medium.allocated;
            stats.medium.usage = stats.medium.capacity > 0u ? 1.0f : 0.0f;
        }
        
        {
            NkScopedSpinLock guard(mLock);
            stats.largeAllocations = mLarge.allocationCount;
            stats.largeBytes = mLarge.totalBytes;
        }
        
        return stats;
    }
    
    void NkMultiLevelAllocator::DumpStats() const noexcept {
        const Stats stats = GetStats();
        NK_FOUNDATION_LOG_INFO("[NKMemory][MultiLevel] tiny: %llu / %llu (%.2f%%)",
            static_cast<unsigned long long>(stats.tiny.allocated),
            static_cast<unsigned long long>(stats.tiny.capacity),
            static_cast<double>(stats.tiny.usage * 100.0f));
        NK_FOUNDATION_LOG_INFO("[NKMemory][MultiLevel] small: %llu / %llu (%.2f%%)",
            static_cast<unsigned long long>(stats.small.allocated),
            static_cast<unsigned long long>(stats.small.capacity),
            static_cast<double>(stats.small.usage * 100.0f));
        NK_FOUNDATION_LOG_INFO("[NKMemory][MultiLevel] medium live: %llu bytes (%llu allocs)",
            static_cast<unsigned long long>(stats.medium.allocated),
            static_cast<unsigned long long>(mMediumPool ? mMediumPool->GetLiveAllocations() : 0u));
        NK_FOUNDATION_LOG_INFO("[NKMemory][MultiLevel] large: %llu allocations, %llu bytes",
            static_cast<unsigned long long>(stats.largeAllocations),
            static_cast<unsigned long long>(stats.largeBytes));
    }
    
    NkAllocator* NkMultiLevelAllocator::FindAllocatorFor(void* ptr) const noexcept {
        if (!ptr) {
            return nullptr;
        }
        if (mTinyPool && mTinyPool->Owns(ptr)) {
            return mTinyPool.Get();
        }
        if (mSmallPool && mSmallPool->Owns(ptr)) {
            return mSmallPool.Get();
        }
        if (mMediumPool && mMediumPool->Owns(ptr)) {
            return mMediumPool.Get();
        }
        return nullptr;
    }
    
    NkMultiLevelAllocator& NkGetMultiLevelAllocator() noexcept {
        static NkMultiLevelAllocator instance;
        return instance;
    }
}
