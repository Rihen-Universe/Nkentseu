// NkMemoryFnSIMD.cpp
#include "pch.h"
#include "NkMemoryFnSIMD.h"
#include <cstring>

namespace nkentseu::memory {
    namespace {
        // Portable fallback (Boyer-Moore-Horspool) for pattern search.
        void* NkSearchPatternPortable(const void* haystack, nk_size haystackSize,
                                      const void* needle, nk_size needleSize) noexcept {
            if (!haystack || !needle) {
                return nullptr;
            }
            if (needleSize == 0u) {
                return const_cast<void*>(haystack);
            }
            if (haystackSize < needleSize) {
                return nullptr;
            }

            const nk_uint8* h = static_cast<const nk_uint8*>(haystack);
            const nk_uint8* n = static_cast<const nk_uint8*>(needle);

            if (needleSize == 1u) {
                return const_cast<void*>(std::memchr(haystack, static_cast<int>(n[0]), haystackSize));
            }

            const nk_size last = needleSize - 1u;
            nk_size shift[256];
            for (nk_size i = 0u; i < 256u; ++i) {
                shift[i] = needleSize;
            }
            for (nk_size i = 0u; i < last; ++i) {
                shift[n[i]] = last - i;
            }

            nk_size pos = 0u;
            while (pos <= (haystackSize - needleSize)) {
                const nk_uint8 tail = h[pos + last];
                if (tail == n[last]) {
                    if (std::memcmp(h + pos, n, last) == 0) {
                        return const_cast<nk_uint8*>(h + pos);
                    }
                }
                pos += shift[tail];
            }
            return nullptr;
        }
    } // namespace

    void NkMemoryCopySIMD(void* dst, const void* src, nk_size bytes) noexcept {
        // Fallback: use memcpy (can be optimized with actual SIMD later)
        std::memcpy(dst, src, bytes);
    }
    
    void NkMemoryMoveSIMD(void* dst, const void* src, nk_size bytes) noexcept {
        std::memmove(dst, src, bytes);
    }
    
    void NkMemorySetSIMD(void* dst, int value, nk_size bytes) noexcept {
        std::memset(dst, value, bytes);
    }
    
    void NkMemoryTransposeSIMD(void* dst, const void* src, nk_size rows, nk_size cols, nk_size elementSize) noexcept {
        // TODO: SIMD transpose implementation
    }
    
    int NkMemoryCompareConstantTime(const void* a, const void* b, nk_size bytes) noexcept {
        int result = 0;
        const nk_uint8* pa = static_cast<const nk_uint8*>(a);
        const nk_uint8* pb = static_cast<const nk_uint8*>(b);
        for (nk_size i = 0; i < bytes; ++i) {
            result |= pa[i] ^ pb[i];  // Constant time 
        }
        return result;
    }
    
    void* NkMemorySearchPatternSIMD(const void* haystack, nk_size haystackSize, const void* needle, nk_size needleSize) noexcept {
        return NkSearchPatternPortable(haystack, haystackSize, needle, needleSize);
    }
    
    nk_uint32 NkMemoryCRC32SIMD(const void* data, nk_size bytes) noexcept {
        // TODO: CRC32 SIMD optimized
        return 0;
    }
    
    nk_uint64 NkMemoryHashSIMD(const void* data, nk_size bytes) noexcept {
        // Simple xxHash-like hash (optimizable with SIMD)
        nk_uint64 hash = 14695981039346656037ull;
        const nk_uint8* ptr = static_cast<const nk_uint8*>(data);
        for (nk_size i = 0; i < bytes; ++i) {
            hash ^= ptr[i];
            hash *= 1099511628211ull;
        }
        return hash;
    }
}
