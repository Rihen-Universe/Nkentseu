#include "NKMemory/NkMemoryFn.h"

#include "NKMemory/NkUtils.h"

namespace nkentseu {
    namespace memory {

        namespace {

            inline usize NkMin(usize a, usize b) {
                return a < b ? a : b;
            }

            inline nk_bool NkIsValidSlice(usize totalSize, usize offset, usize requestedSize, usize* clampedSize) {
                if (clampedSize == nullptr || offset >= totalSize) {
                    return false;
                }
                const usize available = totalSize - offset;
                *clampedSize = NkMin(available, requestedSize);
                return *clampedSize > 0u;
            }

            inline void NkReverseRange(uint8* begin, uint8* end) {
                while (begin < end) {
                    --end;
                    const uint8 tmp = *begin;
                    *begin = *end;
                    *end = tmp;
                    ++begin;
                }
            }

        } // namespace

        void* NkCopy(void* dst, const void* src, usize size) {
            if (!dst || !src || size == 0u) {
                return dst;
            }
            return NkMemCopy(dst, src, size);
        }

        void* NkCopy(void* dst, usize dstSize, usize dstOffset,
                    const void* src, usize srcSize, usize srcOffset, usize size) {
            if (!dst || !src || size == 0u) {
                return dst;
            }

            usize dstSlice = 0u;
            usize srcSlice = 0u;
            if (!NkIsValidSlice(dstSize, dstOffset, size, &dstSlice)) {
                return dst;
            }
            if (!NkIsValidSlice(srcSize, srcOffset, size, &srcSlice)) {
                return dst;
            }

            const usize copySize = NkMin(dstSlice, srcSlice);
            return NkMemCopy(static_cast<uint8*>(dst) + dstOffset,
                            static_cast<const uint8*>(src) + srcOffset,
                            copySize);
        }

        void* NkMove(void* dst, const void* src, usize size) {
            if (!dst || !src || size == 0u) {
                return dst;
            }
            return NkMemMove(dst, src, size);
        }

        void* NkMove(void* dst, usize dstSize, usize dstOffset,
                    const void* src, usize srcSize, usize srcOffset, usize size) {
            if (!dst || !src || size == 0u) {
                return dst;
            }

            usize dstSlice = 0u;
            usize srcSlice = 0u;
            if (!NkIsValidSlice(dstSize, dstOffset, size, &dstSlice)) {
                return dst;
            }
            if (!NkIsValidSlice(srcSize, srcOffset, size, &srcSlice)) {
                return dst;
            }

            const usize moveSize = NkMin(dstSlice, srcSlice);
            return NkMemMove(static_cast<uint8*>(dst) + dstOffset,
                            static_cast<const uint8*>(src) + srcOffset,
                            moveSize);
        }

        void* NkSet(void* dst, int32 value, usize size) {
            if (!dst || size == 0u) {
                return dst;
            }
            return NkMemSet(dst, value, size);
        }

        void* NkSet(void* dst, usize dstSize, usize offset, int32 value, usize size) {
            if (!dst || size == 0u || offset >= dstSize) {
                return dst;
            }
            const usize setSize = NkMin(size, dstSize - offset);
            return NkMemSet(static_cast<uint8*>(dst) + offset, value, setSize);
        }

        int32 NkCompare(const void* a, const void* b, usize size) {
            if (size == 0u || a == b) {
                return 0;
            }
            if (!a || !b) {
                return a ? 1 : -1;
            }
            return NkMemCompare(a, b, size);
        }

        int32 NkCompare(const void* a, usize aSize, const void* b, usize bSize, usize size) {
            if (!a || !b || size == 0u) {
                return 0;
            }
            const usize compareSize = NkMin(size, NkMin(aSize, bSize));
            if (compareSize == 0u) {
                return 0;
            }
            return NkMemCompare(a, b, compareSize);
        }

        int32 NkCompare(const void* a, usize aSize, usize offsetA,
                        const void* b, usize bSize, usize offsetB, usize size) {
            if (!a || !b || size == 0u) {
                return 0;
            }
            if (offsetA >= aSize || offsetB >= bSize) {
                return 0;
            }

            const usize sizeA = NkMin(size, aSize - offsetA);
            const usize sizeB = NkMin(size, bSize - offsetB);
            const usize compareSize = NkMin(sizeA, sizeB);
            if (compareSize == 0u) {
                return 0;
            }
            return NkMemCompare(static_cast<const uint8*>(a) + offsetA,
                                static_cast<const uint8*>(b) + offsetB,
                                compareSize);
        }

        void* NkFind(const void* ptr, uint8 value, usize size) {
            if (!ptr || size == 0u) {
                return nullptr;
            }
            const uint8* bytes = static_cast<const uint8*>(ptr);
            for (usize i = 0u; i < size; ++i) {
                if (bytes[i] == value) {
                    return const_cast<uint8*>(bytes + i);
                }
            }
            return nullptr;
        }

        void* NkFindLast(const void* buffer, usize size, uint8 value) {
            if (!buffer || size == 0u) {
                return nullptr;
            }
            const uint8* bytes = static_cast<const uint8*>(buffer);
            for (usize i = size; i > 0u; --i) {
                if (bytes[i - 1u] == value) {
                    return const_cast<uint8*>(bytes + (i - 1u));
                }
            }
            return nullptr;
        }

        usize NkFindDifference(const void* ptr1, const void* ptr2, usize count) {
            if (!ptr1 || !ptr2 || count == 0u) {
                return 0u;
            }
            const uint8* a = static_cast<const uint8*>(ptr1);
            const uint8* b = static_cast<const uint8*>(ptr2);
            for (usize i = 0u; i < count; ++i) {
                if (a[i] != b[i]) {
                    return i;
                }
            }
            return count;
        }

        bool NkIsZero(const void* ptr, usize count) {
            if (!ptr || count == 0u) {
                return true;
            }
            const uint8* bytes = static_cast<const uint8*>(ptr);
            for (usize i = 0u; i < count; ++i) {
                if (bytes[i] != 0u) {
                    return false;
                }
            }
            return true;
        }

        bool NkOverlaps(const void* ptr1, void* ptr2, usize count) {
            if (!ptr1 || !ptr2 || count == 0u) {
                return false;
            }
            const auto a = reinterpret_cast<usize>(ptr1);
            const auto b = reinterpret_cast<usize>(ptr2);
            const auto aEnd = a + count;
            const auto bEnd = b + count;
            return (a < bEnd) && (b < aEnd);
        }

        bool NkEqualsPattern(const void* buffer, usize size, const void* pattern, usize patternSize) {
            if (!buffer || !pattern || patternSize == 0u || size < patternSize) {
                return false;
            }
            return NkMemCompare(buffer, pattern, patternSize) == 0;
        }

        bool NkValidateMemory(const void* buffer, usize size) {
            if (!buffer || size == 0u) {
                return false;
            }
            const volatile uint8* bytes = static_cast<const volatile uint8*>(buffer);
            volatile uint8 sink = bytes[0];
            sink ^= bytes[size - 1u];
            (void)sink;
            return true;
        }

        void* NkSearchPattern(const void* buffer, usize size, const void* pattern, usize patternSize) {
            if (!buffer || !pattern || patternSize == 0u || size < patternSize) {
                return nullptr;
            }
            const uint8* bytes = static_cast<const uint8*>(buffer);
            for (usize i = 0u; i <= size - patternSize; ++i) {
                if (NkMemCompare(bytes + i, pattern, patternSize) == 0) {
                    return const_cast<uint8*>(bytes + i);
                }
            }
            return nullptr;
        }

        void* NkFindLastPattern(const void* buffer, usize size, const void* pattern, usize patternSize) {
            if (!buffer || !pattern || patternSize == 0u || size < patternSize) {
                return nullptr;
            }
            const uint8* bytes = static_cast<const uint8*>(buffer);
            for (usize i = size - patternSize + 1u; i > 0u; --i) {
                const usize index = i - 1u;
                if (NkMemCompare(bytes + index, pattern, patternSize) == 0) {
                    return const_cast<uint8*>(bytes + index);
                }
            }
            return nullptr;
        }

        void NkReverse(void* buffer, usize size) {
            if (!buffer || size <= 1u) {
                return;
            }
            uint8* bytes = static_cast<uint8*>(buffer);
            NkReverseRange(bytes, bytes + size);
        }

        void NkRotate(void* buffer, usize size, int32 offset) {
            if (!buffer || size <= 1u || offset == 0) {
                return;
            }

            int32 normalized = offset % static_cast<int32>(size);
            if (normalized < 0) {
                normalized += static_cast<int32>(size);
            }
            if (normalized == 0) {
                return;
            }

            uint8* bytes = static_cast<uint8*>(buffer);
            const usize pivot = static_cast<usize>(normalized);
            NkReverseRange(bytes, bytes + pivot);
            NkReverseRange(bytes + pivot, bytes + size);
            NkReverseRange(bytes, bytes + size);
        }

        void NkFill(void* dst, usize size, int32 value) {
            NkSet(dst, value, size);
        }

        void NkTransform(void* dst, const void* src, usize size, int32 (*func)(int32)) {
            if (!dst || !src || !func || size == 0u) {
                return;
            }
            uint8* out = static_cast<uint8*>(dst);
            const uint8* in = static_cast<const uint8*>(src);
            for (usize i = 0u; i < size; ++i) {
                out[i] = static_cast<uint8>(func(static_cast<int32>(in[i])));
            }
        }

        void NkConditionalSet(void* dst, const void* conditionMask, const void* values, usize size) {
            if (!dst || !conditionMask || !values || size == 0u) {
                return;
            }
            uint8* out = static_cast<uint8*>(dst);
            const uint8* mask = static_cast<const uint8*>(conditionMask);
            const uint8* src = static_cast<const uint8*>(values);
            for (usize i = 0u; i < size; ++i) {
                if (mask[i] != 0u) {
                    out[i] = src[i];
                }
            }
        }

        void* NkDuplicate(const void* src, usize size) {
            if (!src || size == 0u) {
                return nullptr;
            }
            void* dst = NkAlloc(size);
            if (!dst) {
                return nullptr;
            }
            NkMemCopy(dst, src, size);
            return dst;
        }

        uint32 NkChecksum(const void* data, usize size) {
            if (!data || size == 0u) {
                return 0u;
            }
            const uint8* bytes = static_cast<const uint8*>(data);
            uint32 hash = 2166136261u;
            for (usize i = 0u; i < size; ++i) {
                hash ^= static_cast<uint32>(bytes[i]);
                hash *= 16777619u;
            }
            return hash;
        }

        void NkSwapEndian(void* buffer, usize elementSize, usize count) {
            if (!buffer || elementSize <= 1u || count == 0u) {
                return;
            }
            uint8* bytes = static_cast<uint8*>(buffer);
            for (usize i = 0u; i < count; ++i) {
                uint8* elem = bytes + i * elementSize;
                NkReverseRange(elem, elem + elementSize);
            }
        }

        void NkZero(void* ptr, usize size) {
            if (!ptr || size == 0u) {
                return;
            }
            NkMemZero(ptr, size);
        }

        void NkSecureZero(void* ptr, usize size) {
            if (!ptr || size == 0u) {
                return;
            }
            volatile uint8* bytes = static_cast<volatile uint8*>(ptr);
            for (usize i = 0u; i < size; ++i) {
                bytes[i] = 0u;
            }
        }

        void* NkAllocAligned(usize alignment, usize size) {
            if (size == 0u) {
                return nullptr;
            }
            if (!NkIsPowerOfTwo(alignment)) {
                alignment = NK_MEMORY_DEFAULT_ALIGNMENT;
            }
            return NkAlloc(size, nullptr, alignment);
        }

        void NkFreeAligned(void* ptr, usize /*size*/) {
            NkFree(ptr);
        }

        int32 NkPosixAligned(void** ptr, usize alignment, usize size) {
            if (!ptr || size == 0u) {
                return -1;
            }
            *ptr = NkAllocAligned(alignment, size);
            return (*ptr != nullptr) ? 0 : -1;
        }

        bool NkAlignPointer(usize align, usize size, void*& outPtr, usize& space) {
            if (!outPtr || align == 0u || !NkIsPowerOfTwo(align)) {
                return false;
            }

            const usize address = reinterpret_cast<usize>(outPtr);
            const usize alignedAddress = NkAlignUp(address, align);
            const usize adjustment = alignedAddress - address;
            if (adjustment > space) {
                return false;
            }
            if (size > (space - adjustment)) {
                return false;
            }

            outPtr = reinterpret_cast<void*>(alignedAddress);
            space -= adjustment;
            return true;
        }

        void* NkAlignPointer(void* ptr, usize align, usize size) {
            if (!ptr || align == 0u || !NkIsPowerOfTwo(align)) {
                return nullptr;
            }
            void* out = ptr;
            usize space = static_cast<usize>(-1);
            if (!NkAlignPointer(align, size, out, space)) {
                return nullptr;
            }
            return out;
        }

        void* NkMap(usize size) {
            if (size == 0u) {
                return nullptr;
            }
            return NkGetVirtualAllocator().Allocate(size, NK_MEMORY_DEFAULT_ALIGNMENT);
        }

        void NkUnmap(void* ptr, usize /*size*/) {
            if (!ptr) {
                return;
            }
            NkGetVirtualAllocator().Deallocate(ptr);
        }

    } // namespace memory
} // namespace nkentseu

