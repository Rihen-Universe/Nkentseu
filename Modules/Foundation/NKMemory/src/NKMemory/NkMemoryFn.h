#pragma once

#ifndef NKENTSEU_MEMORY_NKMEMORYFN_H_INCLUDED
#define NKENTSEU_MEMORY_NKMEMORYFN_H_INCLUDED

#include "NKMemory/NkAllocator.h"
#include <new>

// Legacy macro from NKCore/NkMacros.h collides with function names below.
#ifdef NkSwap
#undef NkSwap
#endif

namespace nkentseu {
    namespace memory {

        using usize = nk_size;
        using uint8 = nk_uint8;
        using int32 = nk_int32;
        using uint32 = nk_uint32;
        using nknullptr = decltype(nullptr);

        NKMEMORY_API void* NkCopy(void* dst, const void* src, usize size);
        NKMEMORY_API void* NkCopy(void* dst, usize dstSize, usize dstOffset,
                                const void* src, usize srcSize, usize srcOffset, usize size);

        NKMEMORY_API void* NkMove(void* dst, const void* src, usize size);
        NKMEMORY_API void* NkMove(void* dst, usize dstSize, usize dstOffset,
                                const void* src, usize srcSize, usize srcOffset, usize size);

        NKMEMORY_API void* NkSet(void* dst, int32 value, usize size);
        NKMEMORY_API void* NkSet(void* dst, usize dstSize, usize offset, int32 value, usize size);

        NKMEMORY_API int32 NkCompare(const void* a, usize aSize, usize offsetA,
                                    const void* b, usize bSize, usize offsetB, usize size);
        NKMEMORY_API int32 NkCompare(const void* a, const void* b, usize size);
        NKMEMORY_API int32 NkCompare(const void* a, usize aSize, const void* b, usize bSize, usize size);

        NKMEMORY_API void* NkFind(const void* ptr, uint8 value, usize size);
        NKMEMORY_API void* NkFindLast(const void* buffer, usize size, uint8 value);
        NKMEMORY_API usize NkFindDifference(const void* ptr1, const void* ptr2, usize count);
        NKMEMORY_API bool NkIsZero(const void* ptr, usize count);
        NKMEMORY_API bool NkOverlaps(const void* ptr1, void* ptr2, usize count);
        NKMEMORY_API bool NkEqualsPattern(const void* buffer, usize size,
                                        const void* pattern, usize patternSize);
        NKMEMORY_API bool NkValidateMemory(const void* buffer, usize size);

        NKMEMORY_API void* NkSearchPattern(const void* buffer, usize size,
                                        const void* pattern, usize patternSize);
        NKMEMORY_API void* NkFindLastPattern(const void* buffer, usize size,
                                            const void* pattern, usize patternSize);
        NKMEMORY_API void NkReverse(void* buffer, usize size);
        NKMEMORY_API void NkRotate(void* buffer, usize size, int32 offset);
        NKMEMORY_API void NkFill(void* dst, usize size, int32 value);
        NKMEMORY_API void NkTransform(void* dst, const void* src, usize size, int32 (*func)(int32));
        NKMEMORY_API void NkConditionalSet(void* dst, const void* conditionMask, const void* values, usize size);
        NKMEMORY_API void* NkDuplicate(const void* src, usize size);
        NKMEMORY_API uint32 NkChecksum(const void* data, usize size);
        NKMEMORY_API void NkSwapEndian(void* buffer, usize elementSize, usize count);

        template <typename T>
        void NkSwap(T& a, T& b) noexcept {
            T temp = core::traits::NkMove(a);
            a = core::traits::NkMove(b);
            b = core::traits::NkMove(temp);
        }

        template <typename T>
        void NkSwapPtr(T* a, T* b) noexcept {
            if (!a || !b || a == b) {
                return;
            }
            T temp = core::traits::NkMove(*a);
            *a = core::traits::NkMove(*b);
            *b = core::traits::NkMove(temp);
        }

        template <typename T>
        void NkSwapTrivial(T& a, T& b) noexcept {
            static_assert(__is_trivially_copyable(T), "NkSwapTrivial requires trivially copyable types.");
            if (&a == &b) {
                return;
            }
            uint8* pa = reinterpret_cast<uint8*>(&a);
            uint8* pb = reinterpret_cast<uint8*>(&b);
            for (usize i = 0; i < sizeof(T); ++i) {
                const uint8 tmp = pa[i];
                pa[i] = pb[i];
                pb[i] = tmp;
            }
        }

        template <typename T>
        void NkSwap(T*& a, T*& b) noexcept {
            T* temp = a;
            a = b;
            b = temp;
        }

        template <typename T, typename... Args>
        T* NkConstruct(T* ptr, Args&&... args) {
            if (!ptr) {
                return nullptr;
            }
            return new (ptr) T(core::traits::NkForward<Args>(args)...);
        }

        template <typename T>
        void NkDestroy(T* ptr) noexcept {
            if (!ptr) {
                return;
            }
            ptr->~T();
        }

        NKMEMORY_API void NkZero(void* ptr, usize size);
        NKMEMORY_API void NkSecureZero(void* ptr, usize size);

        NKMEMORY_API void* NkAllocAligned(usize alignment, usize size);
        NKMEMORY_API void NkFreeAligned(void* ptr, usize size);
        NKMEMORY_API int32 NkPosixAligned(void** ptr, usize alignment, usize size);

        NKMEMORY_API bool NkAlignPointer(usize align, usize size, void*& outPtr, usize& space);
        NKMEMORY_API void* NkAlignPointer(void* ptr, usize align, usize size);

        NKMEMORY_API void* NkMap(usize size);
        NKMEMORY_API void NkUnmap(void* ptr, usize size);

    } // namespace memory

    namespace mem = memory;

} // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKMEMORYFN_H_INCLUDED
