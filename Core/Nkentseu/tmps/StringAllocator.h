#pragma once
#include "Nkentseu/Platform/Platform.h"
#include "Nkentseu/Platform/Assertion.h"
#include <memory>
#include <mutex>

namespace nkentseu {

    class NKENTSEU_API StringAllocator {
    private:
        static constexpr usize ALIGNMENT = 16;
        std::mutex poolMutex;

    public:
        StringAllocator() = default;
        
        // Suppression des copies/déplacements
        StringAllocator(const StringAllocator&) = delete;
        StringAllocator(StringAllocator&&) = delete;
        StringAllocator& operator=(const StringAllocator&) = delete;
        StringAllocator& operator=(StringAllocator&&) = delete;

        void* Allocate(usize size) {
            std::lock_guard<std::mutex> lock(poolMutex);
            return AlignedMalloc(size, ALIGNMENT);
        }

        void Deallocate(void* ptr) noexcept {
            std::lock_guard<std::mutex> lock(poolMutex);
            AlignedFree(ptr);
        }

        template<typename T>
        T* AllocateArray(usize count) {
            return static_cast<T*>(Allocate(count * sizeof(T)));
        }

        template<typename T>
        void DeallocateArray(T* ptr) noexcept {
            Deallocate(ptr);
        }
    };

} // namespace nkentseu