// NkMultiLevelAllocator.cpp
#include "NkMultiLevelAllocator.h"
#include <cstdlib>

namespace nkentseu::memory {

    namespace {
        static constexpr nk_uint64 NK_MULTI_LEVEL_LARGE_MAGIC = 0x4E4B4D4C41524745ull; // "NKMLARGE"

        struct NkLargeAllocHeader {
            nk_uint64 magic;
            nk_size size;
        };
    }

    NkMultiLevelAllocator::NkMultiLevelAllocator() noexcept 
        : NkAllocator("NkMultiLevelAllocator"),
          mTinyPool(NkMakeUnique<NkFixedPoolAllocator<TINY_SIZE, TINY_COUNT>>()),
          mSmallPool(NkMakeUnique<NkFixedPoolAllocator<SMALL_SIZE, SMALL_COUNT>>()),
          mMediumPool(NkMakeUnique<NkVariablePoolAllocator>()) {}
    
    NkMultiLevelAllocator::~NkMultiLevelAllocator() = default;
    
    NkMultiLevelAllocator::Pointer NkMultiLevelAllocator::Allocate(SizeType size, SizeType alignment) {
        if (size <= TINY_SIZE) return mTinyPool->Allocate(size, alignment);
        if (size <= SMALL_SIZE) return mSmallPool->Allocate(size, alignment);
        if (size <= MEDIUM_THRESHOLD) return mMediumPool->Allocate(size, alignment);
        NkLargeAllocHeader* header = static_cast<NkLargeAllocHeader*>(
            ::malloc(sizeof(NkLargeAllocHeader) + size)
        );
        if (!header) {
            return nullptr;
        }
        header->magic = NK_MULTI_LEVEL_LARGE_MAGIC;
        header->size = size;
        mLarge.totalBytes += size;
        ++mLarge.allocationCount;
        return static_cast<void*>(header + 1);
    }
    
    void NkMultiLevelAllocator::Deallocate(Pointer ptr) {
        if (!ptr) return;
        NkLargeAllocHeader* header = reinterpret_cast<NkLargeAllocHeader*>(ptr) - 1;
        if (header->magic == NK_MULTI_LEVEL_LARGE_MAGIC) {
            if (mLarge.totalBytes >= header->size) {
                mLarge.totalBytes -= header->size;
            } else {
                mLarge.totalBytes = 0;
            }
            if (mLarge.allocationCount > 0) {
                --mLarge.allocationCount;
            }
            ::free(header);
        }
    }
    
    NkMultiLevelAllocator::Pointer NkMultiLevelAllocator::Reallocate(Pointer ptr, SizeType oldSize, SizeType newSize, SizeType alignment) {
        void* newPtr = Allocate(newSize, alignment);
        if (newPtr && ptr) {
            NkMemCopy(newPtr, ptr, (oldSize < newSize) ? oldSize : newSize);
            Deallocate(ptr);
        }
        return newPtr;
    }
    
    void NkMultiLevelAllocator::Reset() noexcept {
        mTinyPool->Reset();
        mSmallPool->Reset();
        mMediumPool->Reset();
    }
    
    NkMultiLevelAllocator::Stats NkMultiLevelAllocator::GetStats() const noexcept {
        return Stats();
    }
    
    void NkMultiLevelAllocator::DumpStats() const noexcept {}
    
    NkMultiLevelAllocator& NkGetMultiLevelAllocator() noexcept {
        static NkMultiLevelAllocator instance;
        return instance;
    }
}
