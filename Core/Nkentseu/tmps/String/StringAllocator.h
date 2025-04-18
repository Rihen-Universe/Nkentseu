#pragma once

#include "Nkentseu/Platform/Export.h"
#include "Nkentseu/Platform/Types.h"

namespace nkentseu
{
    // =============================================
    // Allocator
    // =============================================
    template<typename T>
    class NKENTSEU_TEMPLATE_API BasicStringAllocator {
    public:
        using ValueType = T;
        
        BasicStringAllocator() = default;
        
        template<typename U>
        struct Rebind { using Other = BasicStringAllocator<U>; };

        T* Allocate(usize n) {
            return static_cast<T*>(::operator new(n * sizeof(T)));
        }

        void Deallocate(T* p, usize n) noexcept {
            if (!p) return;

            if constexpr (std::is_class_v<T>) {
                for (usize i = 0; i < n; ++i)
                    p[i].~T();
            }
            ::operator delete(static_cast<void*>(p));
        }

        bool operator==(const BasicStringAllocator&) const { return true; }
        bool operator!=(const BasicStringAllocator&) const { return false; }
    };
} // namespace nkentseu
