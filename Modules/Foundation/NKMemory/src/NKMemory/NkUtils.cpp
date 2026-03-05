#include "NKMemory/NkUtils.h"

namespace nkentseu {
    namespace memory {

        nk_size NkAlignUp(nk_size value, nk_size alignment) noexcept {
            if (alignment == 0u) {
                return value;
            }
            const nk_size mask = alignment - 1u;
            return (value + mask) & ~mask;
        }

        nk_size NkAlignDown(nk_size value, nk_size alignment) noexcept {
            if (alignment == 0u) {
                return value;
            }
            const nk_size mask = alignment - 1u;
            return value & ~mask;
        }

        nk_bool NkIsPowerOfTwo(nk_size value) noexcept {
            return value != 0u && (value & (value - 1u)) == 0u;
        }

nk_bool NkIsAlignedPtr(const void* ptr, nk_size alignment) noexcept {
    if (alignment == 0u) {
        return true;
    }
    const core::nk_uintptr value = reinterpret_cast<core::nk_uintptr>(ptr);
    return (value & (alignment - 1u)) == 0u;
}

        void* NkMemSet(void* destination, nk_int32 value, nk_size size) noexcept {
            nk_uint8* dst = static_cast<nk_uint8*>(destination);
            const nk_uint8 byteValue = static_cast<nk_uint8>(value);
            for (nk_size i = 0; i < size; ++i) {
                dst[i] = byteValue;
            }
            return destination;
        }

        void* NkMemCopy(void* destination, const void* source, nk_size size) noexcept {
            nk_uint8* dst = static_cast<nk_uint8*>(destination);
            const nk_uint8* src = static_cast<const nk_uint8*>(source);
            for (nk_size i = 0; i < size; ++i) {
                dst[i] = src[i];
            }
            return destination;
        }

        void* NkMemMove(void* destination, const void* source, nk_size size) noexcept {
            nk_uint8* dst = static_cast<nk_uint8*>(destination);
            const nk_uint8* src = static_cast<const nk_uint8*>(source);
            if (dst == src || size == 0u) {
                return destination;
            }

            if (dst < src) {
                for (nk_size i = 0; i < size; ++i) {
                    dst[i] = src[i];
                }
            } else {
                for (nk_size i = size; i > 0u; --i) {
                    dst[i - 1u] = src[i - 1u];
                }
            }
            return destination;
        }

        nk_int32 NkMemCompare(const void* a, const void* b, nk_size size) noexcept {
            const nk_uint8* lhs = static_cast<const nk_uint8*>(a);
            const nk_uint8* rhs = static_cast<const nk_uint8*>(b);
            for (nk_size i = 0; i < size; ++i) {
                if (lhs[i] != rhs[i]) {
                    return static_cast<nk_int32>(lhs[i]) - static_cast<nk_int32>(rhs[i]);
                }
            }
            return 0;
        }

        void* NkMemZero(void* destination, nk_size size) noexcept {
            return NkMemSet(destination, 0, size);
        }

    } // namespace memory
} // namespace nkentseu
