#pragma once

#ifndef NKENTSEU_MEMORY_NKUTILS_H_INCLUDED
#define NKENTSEU_MEMORY_NKUTILS_H_INCLUDED

#include "NKCore/NkTypes.h"

namespace nkentseu {
    namespace memory {
        [[nodiscard]] nk_size NkAlignUp(nk_size value, nk_size alignment) noexcept;
        [[nodiscard]] nk_size NkAlignDown(nk_size value, nk_size alignment) noexcept;
        [[nodiscard]] nk_bool NkIsPowerOfTwo(nk_size value) noexcept;
        [[nodiscard]] nk_bool NkIsAlignedPtr(const void* ptr, nk_size alignment) noexcept;

        void* NkMemSet(void* destination, nk_int32 value, nk_size size) noexcept;
        void* NkMemCopy(void* destination, const void* source, nk_size size) noexcept;
        void* NkMemMove(void* destination, const void* source, nk_size size) noexcept;
        nk_int32 NkMemCompare(const void* a, const void* b, nk_size size) noexcept;
        void* NkMemZero(void* destination, nk_size size) noexcept;

    } // namespace memory
} // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKUTILS_H_INCLUDED
