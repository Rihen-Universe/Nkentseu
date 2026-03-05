// NkPoolAllocator.cpp
#include "NkPoolAllocator.h"

namespace nkentseu::memory {
    template class NkFixedPoolAllocator<64, 4096>;
    template class NkFixedPoolAllocator<1024, 1024>;
    
    NkVariablePoolAllocator::Pointer NkVariablePoolAllocator::Allocate(SizeType size, SizeType alignment) {
        return nullptr;
    }
    void NkVariablePoolAllocator::Deallocate(Pointer ptr) {}
    void NkVariablePoolAllocator::Reset() noexcept {}
}
