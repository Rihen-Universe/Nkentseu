// NkPoolAllocator.cpp
#include "NkPoolAllocator.h"
#include "NKMemory/NkUtils.h"

#include <cstdlib>

namespace nkentseu::memory {
    namespace {
        static constexpr nk_uint64 NK_VARIABLE_POOL_MAGIC = 0x4E4B564152504F4Full; // "NKVARPOL"
        
        static nk_size NkSanitizeAlignment(nk_size alignment) noexcept {
            if (alignment == 0u) {
                return NK_MEMORY_DEFAULT_ALIGNMENT;
            }
            if ((alignment & (alignment - 1u)) != 0u) {
                return NK_MEMORY_DEFAULT_ALIGNMENT;
            }
            return alignment;
        }
    }
    
    template class NkFixedPoolAllocator<64, 4096>;
    template class NkFixedPoolAllocator<1024, 1024>;
    
    NkVariablePoolAllocator::Pointer NkVariablePoolAllocator::Allocate(SizeType size, SizeType alignment) {
        if (size == 0u) {
            return nullptr;
        }
        
        const SizeType safeAlignment = NkSanitizeAlignment(alignment);
        const SizeType totalSize = sizeof(Header) + size + safeAlignment;
        
        nk_uint8* base = static_cast<nk_uint8*>(::malloc(totalSize));
        if (!base) {
            return nullptr;
        }
        
        const nk_uintptr firstUser = reinterpret_cast<nk_uintptr>(base + sizeof(Header));
        const nk_uintptr alignedUser = static_cast<nk_uintptr>(NkAlignUp(static_cast<nk_size>(firstUser), safeAlignment));
        nk_uint8* userPtr = reinterpret_cast<nk_uint8*>(alignedUser);
        
        Header* header = reinterpret_cast<Header*>(userPtr - sizeof(Header));
        header->magic = NK_VARIABLE_POOL_MAGIC;
        header->requestedSize = size;
        header->offsetToBase = static_cast<SizeType>(userPtr - base);
        header->prev = nullptr;
        header->next = nullptr;
        
        {
            NkScopedSpinLock guard(mLock);
            header->next = mHead;
            if (mHead) {
                mHead->prev = header;
            }
            mHead = header;
            mLiveBytes += size;
            ++mLiveAllocations;
        }
        
        return userPtr;
    }
    
    void NkVariablePoolAllocator::Deallocate(Pointer ptr) {
        if (!ptr) {
            return;
        }
        
        Header* header = reinterpret_cast<Header*>(static_cast<nk_uint8*>(ptr) - sizeof(Header));
        if (header->magic != NK_VARIABLE_POOL_MAGIC) {
            return;
        }
        
        nk_uint8* base = nullptr;
        {
            NkScopedSpinLock guard(mLock);
            if (header->prev) {
                header->prev->next = header->next;
            } else {
                mHead = header->next;
            }
            if (header->next) {
                header->next->prev = header->prev;
            }
            
            if (mLiveBytes >= header->requestedSize) {
                mLiveBytes -= header->requestedSize;
            } else {
                mLiveBytes = 0u;
            }
            if (mLiveAllocations > 0u) {
                --mLiveAllocations;
            }
            
            base = static_cast<nk_uint8*>(ptr) - header->offsetToBase;
            header->magic = 0u;
            header->prev = nullptr;
            header->next = nullptr;
        }
        
        ::free(base);
    }
    
    void NkVariablePoolAllocator::Reset() noexcept {
        Header* list = nullptr;
        {
            NkScopedSpinLock guard(mLock);
            list = mHead;
            mHead = nullptr;
            mLiveBytes = 0u;
            mLiveAllocations = 0u;
        }
        
        while (list) {
            Header* next = list->next;
            nk_uint8* userPtr = reinterpret_cast<nk_uint8*>(list) + sizeof(Header);
            nk_uint8* base = userPtr - list->offsetToBase;
            list->magic = 0u;
            ::free(base);
            list = next;
        }
    }
    
    bool NkVariablePoolAllocator::Owns(Pointer ptr) const noexcept {
        if (!ptr) {
            return false;
        }
        
        NkScopedSpinLock guard(mLock);
        const Header* node = mHead;
        while (node) {
            const void* userPtr = reinterpret_cast<const nk_uint8*>(node) + sizeof(Header);
            if (userPtr == ptr) {
                return true;
            }
            node = node->next;
        }
        return false;
    }
}
