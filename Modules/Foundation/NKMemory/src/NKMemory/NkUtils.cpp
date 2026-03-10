#include "NKMemory/NkUtils.h"
#include "NKPlatform/NkArchDetect.h"

#if !defined(__clang__) && !defined(__GNUC__)
#include <string.h>
#endif

#if defined(NKENTSEU_CPU_HAS_AVX2) && (defined(NKENTSEU_ARCH_X86_64) || defined(NKENTSEU_ARCH_X86))
#include <immintrin.h>
#endif

namespace nkentseu {
    namespace memory {
        namespace {

            inline void* NkBuiltinMemSet(void* destination, nk_int32 value, nk_size size) noexcept {
#if defined(__clang__) || defined(__GNUC__)
                return __builtin_memset(destination, value, size);
#else
                return ::memset(destination, value, static_cast<size_t>(size));
#endif
            }

            inline void* NkBuiltinMemCopy(void* destination, const void* source, nk_size size) noexcept {
#if defined(__clang__) || defined(__GNUC__)
                return __builtin_memcpy(destination, source, size);
#else
                return ::memcpy(destination, source, static_cast<size_t>(size));
#endif
            }

            inline void* NkBuiltinMemMove(void* destination, const void* source, nk_size size) noexcept {
#if defined(__clang__) || defined(__GNUC__)
                return __builtin_memmove(destination, source, size);
#else
                return ::memmove(destination, source, static_cast<size_t>(size));
#endif
            }

            inline nk_int32 NkBuiltinMemCompare(const void* a, const void* b, nk_size size) noexcept {
#if defined(__clang__) || defined(__GNUC__)
                return __builtin_memcmp(a, b, size);
#else
                return static_cast<nk_int32>(::memcmp(a, b, static_cast<size_t>(size)));
#endif
            }

#if defined(NKENTSEU_CPU_HAS_AVX2) && (defined(NKENTSEU_ARCH_X86_64) || defined(NKENTSEU_ARCH_X86))
            constexpr nk_size NK_AVX2_COPY_CHUNK = 32u;
            constexpr nk_size NK_AVX2_PREFETCH_T0_DISTANCE = 256u;
            constexpr nk_size NK_AVX2_PREFETCH_NTA_DISTANCE = 512u;
            constexpr nk_size NK_AVX2_STREAMING_THRESHOLD = 256u * 1024u;

            inline void NkMemCopyAvx2Regular(nk_uint8* destination,
                                             const nk_uint8* source,
                                             nk_size size) noexcept {
                nk_size index = 0u;

                _mm_prefetch(reinterpret_cast<const char*>(source), _MM_HINT_T0);
                if (size > 64u) {
                    _mm_prefetch(reinterpret_cast<const char*>(source + 64u), _MM_HINT_T0);
                }
                if (size > 128u) {
                    _mm_prefetch(reinterpret_cast<const char*>(source + 128u), _MM_HINT_T0);
                }

                for (; index + NK_AVX2_COPY_CHUNK <= size; index += NK_AVX2_COPY_CHUNK) {
                    if (index + NK_AVX2_PREFETCH_T0_DISTANCE < size) {
                        _mm_prefetch(reinterpret_cast<const char*>(source + index + NK_AVX2_PREFETCH_T0_DISTANCE), _MM_HINT_T0);
                    }
                    const __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(source + index));
                    _mm256_storeu_si256(reinterpret_cast<__m256i*>(destination + index), chunk);
                }

                if (index < size) {
                    NkBuiltinMemCopy(destination + index, source + index, size - index);
                }
            }

            inline void NkMemCopyAvx2Streaming(nk_uint8* destination,
                                               const nk_uint8* source,
                                               nk_size size) noexcept {
                nk_size index = 0u;

                for (; index + NK_AVX2_COPY_CHUNK <= size; index += NK_AVX2_COPY_CHUNK) {
                    if (index + NK_AVX2_PREFETCH_NTA_DISTANCE < size) {
                        _mm_prefetch(reinterpret_cast<const char*>(source + index + NK_AVX2_PREFETCH_NTA_DISTANCE), _MM_HINT_NTA);
                    }
                    const __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(source + index));
                    _mm256_stream_si256(reinterpret_cast<__m256i*>(destination + index), chunk);
                }

                _mm_sfence();

                if (index < size) {
                    NkBuiltinMemCopy(destination + index, source + index, size - index);
                }
            }
#endif
        } // namespace

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
            const nk_uintptr value = reinterpret_cast<nk_uintptr>(ptr);
            return (value & (alignment - 1u)) == 0u;
        }

        void* NkMemSet(void* destination, nk_int32 value, nk_size size) noexcept {
            if (!destination || size == 0u) {
                return destination;
            }
            return NkBuiltinMemSet(destination, value, size);
        }

        void* NkMemCopy(void* destination, const void* source, nk_size size) noexcept {
            if (!destination || !source || size == 0u || destination == source) {
                return destination;
            }

#if defined(NKENTSEU_CPU_HAS_AVX2) && (defined(NKENTSEU_ARCH_X86_64) || defined(NKENTSEU_ARCH_X86))
            if (size >= NK_AVX2_COPY_CHUNK) {
                nk_uint8* dst = static_cast<nk_uint8*>(destination);
                const nk_uint8* src = static_cast<const nk_uint8*>(source);

                if (size >= NK_AVX2_STREAMING_THRESHOLD && NkIsAlignedPtr(destination, NK_AVX2_COPY_CHUNK)) {
                    NkMemCopyAvx2Streaming(dst, src, size);
                    return destination;
                }

                NkMemCopyAvx2Regular(dst, src, size);
                return destination;
            }
#endif

            NkBuiltinMemCopy(destination, source, size);
            return destination;
        }

        void* NkMemMove(void* destination, const void* source, nk_size size) noexcept {
            if (!destination || !source || size == 0u || destination == source) {
                return destination;
            }

            nk_uint8* dst = static_cast<nk_uint8*>(destination);
            const nk_uint8* src = static_cast<const nk_uint8*>(source);

            if ((dst + size) <= src || (src + size) <= dst) {
                return NkMemCopy(destination, source, size);
            }

            return NkBuiltinMemMove(destination, source, size);
        }

        nk_int32 NkMemCompare(const void* a, const void* b, nk_size size) noexcept {
            if (size == 0u || a == b) {
                return 0;
            }
            if (!a || !b) {
                return a ? 1 : -1;
            }
            return NkBuiltinMemCompare(a, b, size);
        }

        void* NkMemZero(void* destination, nk_size size) noexcept {
            return NkMemSet(destination, 0, size);
        }

    } // namespace memory
} // namespace nkentseu
