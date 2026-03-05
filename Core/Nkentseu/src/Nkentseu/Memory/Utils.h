#pragma once

#include "Nkentseu/Platform/Types.h"
#include "Nkentseu/Platform/Inline.h"
#include "Nkentseu/Platform/Export.h"
#include "Nkentseu/Platform/PlatformDetection.h"

namespace nkentseu
{
    ///////////////////////////////////////////////////////////////////////////////
    // Utilitaires mémoire
    ///////////////////////////////////////////////////////////////////////////////
    namespace memory {

        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            #define AlignedMalloc(size, alignment) _aligned_malloc(size, alignment)
            #define AlignedFree(ptr) _aligned_free(ptr)
        #else
            #define AlignedMalloc(size, alignment) aligned_alloc(alignment, size)
            #define AlignedFree(ptr) free(ptr)
        #endif
        
        NKENTSEU_INLINE_API uint64 GetAligned(uint64 operand, uint64 granularity) {
            return (operand + (granularity - 1)) & ~(granularity - 1);
        }

        template <typename T>
        T AlignUp(T value, T alignment) {
            return (value + alignment - 1) & ~(alignment - 1);
        }

        // Conversions de tailles
        constexpr uint64 GiB(uint64 amount) { return amount * 1024ULL * 1024ULL * 1024ULL; }
        constexpr uint64 MiB(uint64 amount) { return amount * 1024ULL * 1024ULL; }
        constexpr uint64 KiB(uint64 amount) { return amount * 1024ULL; }

        // Configuration alignement mémoire (16 bytes par défaut)
        constexpr usize DEFAULT_ALIGNMENT = 16;
        constexpr usize MIN_ALIGNMENT = sizeof(void*);
        constexpr usize MAX_ALIGNMENT = 4096;
        constexpr bool ALLOCATOR_THROW_ON_FAIL = true;

        NKENTSEU_FORCE_INLINE void* MemoryCopy(void* NKENTSEU_RESTRICT destination, const void* NKENTSEU_RESTRICT source, usize size) NKENTSEU_NOEXCEPT {
            if (NKENTSEU_UNLIKELY(size == 0)) return destination;
            if (NKENTSEU_UNLIKELY(destination == nullptr || source == nullptr)) return nullptr;
        
            auto* dest = static_cast<uint8*>(destination);
            const auto* src = static_cast<const uint8*>(source);
        
            // Copie par mots de 64 bits quand possible
            const usize alignedSize = size & ~7ull;
            for (usize i = 0; i < alignedSize; i += 8) {
                *reinterpret_cast<uint64*>(dest + i) = *reinterpret_cast<const uint64*>(src + i);
            }
        
            // Copie des octets restants
            for (usize i = alignedSize; i < size; ++i) {
                dest[i] = src[i];
            }
        
            return destination;
        }

        NKENTSEU_FORCE_INLINE void* MemoryMove(void* NKENTSEU_RESTRICT destination, const void* NKENTSEU_RESTRICT source, usize size) NKENTSEU_NOEXCEPT {
            if (NKENTSEU_UNLIKELY(size == 0 || destination == source)) return destination;
            
            auto* dest = static_cast<uint8*>(destination);
            const auto* src = static_cast<const uint8*>(source);
        
            // Gestion du chevauchement
            if(dest > src && dest < src + size) {
                // Copie inversée pour les zones qui se chevauchent
                for(usize i = size; i > 0; ) {
                    --i;
                    dest[i] = src[i];
                }
            } else {
                // Copie optimisée par mots de 64 bits
                const usize alignedSize = size & ~7ull;
                for(usize i = 0; i < alignedSize; i += 8) {
                    *reinterpret_cast<uint64*>(dest + i) = *reinterpret_cast<const uint64*>(src + i);
                }
                // Copie des octets restants
                for(usize i = alignedSize; i < size; ++i) {
                    dest[i] = src[i];
                }
            }
            
            return destination;
        }
        
        NKENTSEU_FORCE_INLINE int MemoryCompare(const void* NKENTSEU_RESTRICT ptr1, const void* NKENTSEU_RESTRICT ptr2, usize size) NKENTSEU_NOEXCEPT {
            if (NKENTSEU_UNLIKELY(size == 0 || ptr1 == ptr2)) return 0;
            
            const auto* p1 = static_cast<const uint8*>(ptr1);
            const auto* p2 = static_cast<const uint8*>(ptr2);
        
            // Comparaison par blocs de 64 bits
            const usize alignedSize = size & ~7ull;
            for(usize i = 0; i < alignedSize; i += 8) {
                const uint64 v1 = *reinterpret_cast<const uint64*>(p1 + i);
                const uint64 v2 = *reinterpret_cast<const uint64*>(p2 + i);
                if(v1 != v2) {
                    // Trouver le premier octet différent
                    for(usize j = 0; j < 8; ++j) {
                        const int diff = static_cast<int>(p1[i + j]) - static_cast<int>(p2[i + j]);
                        if(diff != 0) return diff;
                    }
                }
            }
        
            // Comparaison des octets restants
            for(usize i = alignedSize; i < size; ++i) {
                const int diff = static_cast<int>(p1[i]) - static_cast<int>(p2[i]);
                if(diff != 0) return diff;
            }
            
            return 0;
        }
        
        NKENTSEU_FORCE_INLINE void* MemorySet(void* NKENTSEU_RESTRICT destination, int32 value, usize size) NKENTSEU_NOEXCEPT {
            if (NKENTSEU_UNLIKELY(size == 0)) return destination;
            
            auto* dest = static_cast<uint8*>(destination);
            const uint8 byteValue = static_cast<uint8>(value);
            
            // Création d'un motif 64-bit
            const uint64 pattern = 
                (static_cast<uint64>(byteValue) << 56) |
                (static_cast<uint64>(byteValue) << 48) |
                (static_cast<uint64>(byteValue) << 40) |
                (static_cast<uint64>(byteValue) << 32) |
                (static_cast<uint64>(byteValue) << 24) |
                (static_cast<uint64>(byteValue) << 16) |
                (static_cast<uint64>(byteValue) << 8)  |
                static_cast<uint64>(byteValue);
        
            // Remplissage par blocs de 64 bits
            const usize alignedSize = size & ~7ull;
            for(usize i = 0; i < alignedSize; i += 8) {
                *reinterpret_cast<uint64*>(dest + i) = pattern;
            }
        
            // Remplissage des octets restants
            for(usize i = alignedSize; i < size; ++i) {
                dest[i] = byteValue;
            }
            
            return destination;
        }
    }
} // namespace nkentseu
