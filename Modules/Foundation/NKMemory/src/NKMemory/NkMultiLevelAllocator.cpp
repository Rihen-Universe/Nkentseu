// NkMultiLevelAllocator.cpp
#include "NkMultiLevelAllocator.h"

namespace nkentseu::memory {
    NkMultiLevelAllocator::NkMultiLevelAllocator() noexcept 
        : NkAllocator("NkMultiLevelAllocator"),
          mTinyPool(std::make_unique<NkFixedPoolAllocator<TINY_SIZE, TINY_COUNT>>()),
          mSmallPool(std::make_unique<NkFixedPoolAllocator<SMALL_SIZE, SMALL_COUNT>>()),
          mMediumPool(std::make_unique<NkVariablePoolAllocator>()) {}
    
    NkMultiLevelAllocator::~NkMultiLevelAllocator() = default;
    
    NkMultiLevelAllocator::Pointer NkMultiLevelAllocator::Allocate(SizeType size, SizeType alignment) {
        if (size <= TINY_SIZE) return mTinyPool->Allocate(size, alignment);
        if (size <= SMALL_SIZE) return mSmallPool->Allocate(size, alignment);
        if (size <= MEDIUM_THRESHOLD) return mMediumPool->Allocate(size, alignment);
        return ::operator new(size, std::nothrow);
    }
    
    void NkMultiLevelAllocator::Deallocate(Pointer ptr) {
        if (!ptr) return;
        // Remove from large allocs or delegate to pools
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
