// NkFunctionSIMD.cpp
#include "pch.h"
#include "NkFunctionSIMD.h"
#include "NKMemory/NkUtils.h"

namespace nkentseu::memory {
    namespace {
        static nk_uint32 NkCrc32Table[256];
        static nk_bool NkCrc32TableReady = false;

        static void NkInitCrc32Table() noexcept {
            if (NkCrc32TableReady) {
                return;
            }
            for (nk_uint32 i = 0; i < 256u; ++i) {
                nk_uint32 crc = i;
                for (nk_uint32 j = 0; j < 8u; ++j) {
                    const nk_uint32 mask = -(crc & 1u);
                    crc = (crc >> 1u) ^ (0xEDB88320u & mask);
                }
                NkCrc32Table[i] = crc;
            }
            NkCrc32TableReady = true;
        }

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
                for (nk_size i = 0u; i < haystackSize; ++i) {
                    if (h[i] == n[0]) {
                        return const_cast<nk_uint8*>(h + i);
                    }
                }
                return nullptr;
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
                    if (NkMemCompare(h + pos, n, last) == 0) {
                        return const_cast<nk_uint8*>(h + pos);
                    }
                }
                pos += shift[tail];
            }
            return nullptr;
        }
    } // namespace

    void NkMemoryCopySIMD(void* dst, const void* src, nk_size bytes) noexcept {
        NkMemCopy(dst, src, bytes);
    }
    
    void NkMemoryMoveSIMD(void* dst, const void* src, nk_size bytes) noexcept {
        NkMemMove(dst, src, bytes);
    }
    
    void NkMemorySetSIMD(void* dst, int value, nk_size bytes) noexcept {
        NkMemSet(dst, value, bytes);
    }
    
    void NkMemoryTransposeSIMD(void* dst, const void* src, nk_size rows, nk_size cols, nk_size elementSize) noexcept {
        if (dst == nullptr || src == nullptr || rows == 0u || cols == 0u || elementSize == 0u) {
            return;
        }

        if (dst == src) {
            // In-place transpose is not supported in this generic helper.
            return;
        }

        if (elementSize == 1u) {
            const nk_uint8* in = static_cast<const nk_uint8*>(src);
            nk_uint8* out = static_cast<nk_uint8*>(dst);
            for (nk_size r = 0u; r < rows; ++r) {
                for (nk_size c = 0u; c < cols; ++c) {
                    out[c * rows + r] = in[r * cols + c];
                }
            }
            return;
        }

        if (elementSize == 2u) {
            const nk_uint16* in = static_cast<const nk_uint16*>(src);
            nk_uint16* out = static_cast<nk_uint16*>(dst);
            for (nk_size r = 0u; r < rows; ++r) {
                for (nk_size c = 0u; c < cols; ++c) {
                    out[c * rows + r] = in[r * cols + c];
                }
            }
            return;
        }

        if (elementSize == 4u) {
            const nk_uint32* in = static_cast<const nk_uint32*>(src);
            nk_uint32* out = static_cast<nk_uint32*>(dst);
            for (nk_size r = 0u; r < rows; ++r) {
                for (nk_size c = 0u; c < cols; ++c) {
                    out[c * rows + r] = in[r * cols + c];
                }
            }
            return;
        }

        if (elementSize == 8u) {
            const nk_uint64* in = static_cast<const nk_uint64*>(src);
            nk_uint64* out = static_cast<nk_uint64*>(dst);
            for (nk_size r = 0u; r < rows; ++r) {
                for (nk_size c = 0u; c < cols; ++c) {
                    out[c * rows + r] = in[r * cols + c];
                }
            }
            return;
        }

        const nk_uint8* in = static_cast<const nk_uint8*>(src);
        nk_uint8* out = static_cast<nk_uint8*>(dst);
        for (nk_size r = 0u; r < rows; ++r) {
            for (nk_size c = 0u; c < cols; ++c) {
                const nk_size srcOffset = (r * cols + c) * elementSize;
                const nk_size dstOffset = (c * rows + r) * elementSize;
                NkMemCopy(out + dstOffset, in + srcOffset, elementSize);
            }
        }
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
        if (data == nullptr || bytes == 0u) {
            return 0u;
        }

        NkInitCrc32Table();

        nk_uint32 crc = 0xFFFFFFFFu;
        const nk_uint8* ptr = static_cast<const nk_uint8*>(data);
        for (nk_size i = 0u; i < bytes; ++i) {
            const nk_uint8 index = static_cast<nk_uint8>((crc ^ ptr[i]) & 0xFFu);
            crc = (crc >> 8u) ^ NkCrc32Table[index];
        }
        return crc ^ 0xFFFFFFFFu;
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
