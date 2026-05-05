// =============================================================================
// NKMemory/NkFunction.cpp
// Implémentations des fonctions utilitaires mémoire.
//
// Design :
//  - Inclusion systématique de "pch.h" en premier pour la précompilation
//  - Réutilisation maximale de NKMemory/NkUtils.h (ZÉRO duplication)
//  - Bounds checking explicite pour les variantes avec offsets
//  - Fallbacks portables avec guards pour optimisations futures
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

// -------------------------------------------------------------------------
// PRÉ-COMPILED HEADER - TOUJOURS EN PREMIER
// -------------------------------------------------------------------------
#include "pch.h"

// -------------------------------------------------------------------------
// EN-TÊTES DU MODULE
// -------------------------------------------------------------------------
#include "NKMemory/NkFunction.h"
#include "NKMemory/NkUtils.h"           // NkMemCopy, NkMemMove, etc.

// -------------------------------------------------------------------------
// NAMESPACE ANONYME : Helpers internes non-exportés
// -------------------------------------------------------------------------
namespace {

    /**
     * @brief Minimum de deux valeurs size_t
     * @note Helper local pour éviter dépendance supplémentaire
     */
    [[nodiscard]] inline nkentseu::nk_size Min(nkentseu::nk_size a, nkentseu::nk_size b) noexcept {
        return (a < b) ? a : b;
    }

    /**
     * @brief Valide qu'un slice est dans les bounds d'un buffer
     * @param totalSize Taille totale du buffer
     * @param offset Offset de début du slice
     * @param requestedSize Taille demandée pour le slice
     * @param clampedSize Output : taille effectivement disponible (clamped)
     * @return true si slice valide et non-vide
     */
    [[nodiscard]] inline bool IsValidSlice(nkentseu::nk_size totalSize, 
                                          nkentseu::nk_size offset, 
                                          nkentseu::nk_size requestedSize,
                                          nkentseu::nk_size* clampedSize) noexcept {
        if (!clampedSize || offset >= totalSize) {
            return false;
        }
        const nkentseu::nk_size available = totalSize - offset;
        *clampedSize = Min(available, requestedSize);
        return *clampedSize > 0u;
    }

    /**
     * @brief Inverse un range d'octets [begin, end)
     * @note Helper pour NkReverse et NkRotate
     */
    inline void ReverseRange(nkentseu::nk_uint8* begin, nkentseu::nk_uint8* end) noexcept {
        while (begin < end) {
            --end;
            const nkentseu::nk_uint8 tmp = *begin;
            *begin = *end;
            *end = tmp;
            ++begin;
        }
    }

} // namespace anonyme


// -------------------------------------------------------------------------
// NAMESPACE PUBLIC : Implémentations
// -------------------------------------------------------------------------
namespace nkentseu {
    namespace memory {

        // ====================================================================
        // Opérations de Base : COPY
        // ====================================================================

        void* NkCopy(void* dst, const void* src, usize size) noexcept {
            if (!dst || !src || size == 0u) {
                return dst;
            }
            // Délègue à NkMemCopy de NkUtils.h (optimisations AVX2 si disponibles)
            return NkMemCopy(dst, src, size);
        }

        void* NkCopy(void* dst, usize dstSize, usize dstOffset,
                    const void* src, usize srcSize, usize srcOffset, usize size) noexcept {
            if (!dst || !src || size == 0u) {
                return dst;
            }

            // Validation et clamping des slices
            usize dstSlice = 0u;
            usize srcSlice = 0u;
            if (!IsValidSlice(dstSize, dstOffset, size, &dstSlice)) {
                return dst;
            }
            if (!IsValidSlice(srcSize, srcOffset, size, &srcSlice)) {
                return dst;
            }

            // Copie la taille minimale disponible
            const usize copySize = Min(dstSlice, srcSlice);
            return NkMemCopy(
                static_cast<uint8*>(dst) + dstOffset,
                static_cast<const uint8*>(src) + srcOffset,
                copySize
            );
        }

        // ====================================================================
        // Opérations de Base : MOVE
        // ====================================================================

        void* NkMove(void* dst, const void* src, usize size) noexcept {
            if (!dst || !src || size == 0u) {
                return dst;
            }
            return NkMemMove(dst, src, size);
        }

        void* NkMove(void* dst, usize dstSize, usize dstOffset,
                    const void* src, usize srcSize, usize srcOffset, usize size) noexcept {
            if (!dst || !src || size == 0u) {
                return dst;
            }

            usize dstSlice = 0u;
            usize srcSlice = 0u;
            if (!IsValidSlice(dstSize, dstOffset, size, &dstSlice)) {
                return dst;
            }
            if (!IsValidSlice(srcSize, srcOffset, size, &srcSlice)) {
                return dst;
            }

            const usize moveSize = Min(dstSlice, srcSlice);
            return NkMemMove(
                static_cast<uint8*>(dst) + dstOffset,
                static_cast<const uint8*>(src) + srcOffset,
                moveSize
            );
        }

        // ====================================================================
        // Opérations de Base : SET
        // ====================================================================

        void* NkSet(void* dst, int32 value, usize size) noexcept {
            if (!dst || size == 0u) {
                return dst;
            }
            return NkMemSet(dst, value, size);
        }

        void* NkSet(void* dst, usize dstSize, usize offset, int32 value, usize size) noexcept {
            if (!dst || size == 0u || offset >= dstSize) {
                return dst;
            }
            const usize setSize = Min(size, dstSize - offset);
            return NkMemSet(static_cast<uint8*>(dst) + offset, value, setSize);
        }

        // ====================================================================
        // Opérations de Base : COMPARE
        // ====================================================================

        int32 NkCompare(const void* a, const void* b, usize size) noexcept {
            if (size == 0u || a == b) {
                return 0;
            }
            if (!a || !b) {
                return a ? 1 : -1;
            }
            return NkMemCompare(a, b, size);
        }

        int32 NkCompare(const void* a, usize aSize, const void* b, usize bSize, usize size) noexcept {
            if (!a || !b || size == 0u) {
                return 0;
            }
            const usize compareSize = Min(size, Min(aSize, bSize));
            if (compareSize == 0u) {
                return 0;
            }
            return NkMemCompare(a, b, compareSize);
        }

        int32 NkCompare(const void* a, usize aSize, usize offsetA,
                        const void* b, usize bSize, usize offsetB, usize size) noexcept {
            if (!a || !b || size == 0u) {
                return 0;
            }
            if (offsetA >= aSize || offsetB >= bSize) {
                return 0;
            }

            const usize sizeA = Min(size, aSize - offsetA);
            const usize sizeB = Min(size, bSize - offsetB);
            const usize compareSize = Min(sizeA, sizeB);
            if (compareSize == 0u) {
                return 0;
            }
            return NkMemCompare(
                static_cast<const uint8*>(a) + offsetA,
                static_cast<const uint8*>(b) + offsetB,
                compareSize
            );
        }

        // ====================================================================
        // Recherche et Analyse
        // ====================================================================

        void* NkFind(const void* ptr, uint8 value, usize size) noexcept {
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

        void* NkFindLast(const void* buffer, usize size, uint8 value) noexcept {
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

        usize NkFindDifference(const void* ptr1, const void* ptr2, usize count) noexcept {
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

        bool NkIsZero(const void* ptr, usize count) noexcept {
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

        bool NkOverlaps(const void* ptr1, const void* ptr2, usize count) noexcept {
            if (!ptr1 || !ptr2 || count == 0u) {
                return false;
            }
            const auto a = reinterpret_cast<usize>(ptr1);
            const auto b = reinterpret_cast<usize>(ptr2);
            const auto aEnd = a + count;
            const auto bEnd = b + count;
            return (a < bEnd) && (b < aEnd);
        }

        bool NkEqualsPattern(const void* buffer, usize size, 
                            const void* pattern, usize patternSize) noexcept {
            if (!buffer || !pattern || patternSize == 0u || size < patternSize) {
                return false;
            }
            return NkMemCompare(buffer, pattern, patternSize) == 0;
        }

        bool NkValidateMemory(const void* buffer, usize size) noexcept {
            if (!buffer || size == 0u) {
                return false;
            }
            // Lecture volatile pour éviter l'optimisation du compiler
            const volatile uint8* bytes = static_cast<const volatile uint8*>(buffer);
            volatile uint8 sink = bytes[0];
            if (size > 1u) {
                sink ^= bytes[size - 1u];
            }
            (void)sink;
            return true;
        }

        void* NkSearchPattern(const void* buffer, usize size,
                             const void* pattern, usize patternSize) noexcept {
            if (!buffer || !pattern || patternSize == 0u || size < patternSize) {
                return nullptr;
            }
            const uint8* bytes = static_cast<const uint8*>(buffer);
            const uint8* pat = static_cast<const uint8*>(pattern);
            
            // Recherche naïve O(n*m) - suffisant pour patterns courts
            for (usize i = 0u; i <= size - patternSize; ++i) {
                if (NkMemCompare(bytes + i, pat, patternSize) == 0) {
                    return const_cast<uint8*>(bytes + i);
                }
            }
            return nullptr;
        }

        void* NkFindLastPattern(const void* buffer, usize size,
                               const void* pattern, usize patternSize) noexcept {
            if (!buffer || !pattern || patternSize == 0u || size < patternSize) {
                return nullptr;
            }
            const uint8* bytes = static_cast<const uint8*>(buffer);
            const uint8* pat = static_cast<const uint8*>(pattern);
            
            // Parcours de fin vers début
            for (usize i = size - patternSize + 1u; i > 0u; --i) {
                const usize index = i - 1u;
                if (NkMemCompare(bytes + index, pat, patternSize) == 0) {
                    return const_cast<uint8*>(bytes + index);
                }
            }
            return nullptr;
        }

        // ====================================================================
        // Transformations
        // ====================================================================

        void NkReverse(void* buffer, usize size) noexcept {
            if (!buffer || size <= 1u) {
                return;
            }
            uint8* bytes = static_cast<uint8*>(buffer);
            ReverseRange(bytes, bytes + size);
        }

        void NkRotate(void* buffer, usize size, int32 offset) noexcept {
            if (!buffer || size <= 1u || offset == 0) {
                return;
            }

            // Normalisation de l'offset : [0, size)
            int32 normalized = offset % static_cast<int32>(size);
            if (normalized < 0) {
                normalized += static_cast<int32>(size);
            }
            if (normalized == 0) {
                return;
            }

            // Algorithme triple-reverse pour rotation O(n)
            uint8* bytes = static_cast<uint8*>(buffer);
            const usize pivot = static_cast<usize>(normalized);
            
            ReverseRange(bytes, bytes + pivot);           // Reverse [0, pivot)
            ReverseRange(bytes + pivot, bytes + size);    // Reverse [pivot, size)
            ReverseRange(bytes, bytes + size);            // Reverse [0, size)
        }

        void NkFill(void* dst, usize size, int32 value) noexcept {
            NkSet(dst, value, size);
        }

        void NkTransform(void* dst, const void* src, usize size, 
                        int32 (*func)(int32)) noexcept {
            if (!dst || !src || !func || size == 0u) {
                return;
            }
            uint8* out = static_cast<uint8*>(dst);
            const uint8* in = static_cast<const uint8*>(src);
            for (usize i = 0u; i < size; ++i) {
                out[i] = static_cast<uint8>(func(static_cast<int32>(in[i])));
            }
        }

        void NkConditionalSet(void* dst, const void* conditionMask, 
                             const void* values, usize size) noexcept {
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

        // ====================================================================
        // Utilitaires d'Allocation
        // ====================================================================

        void* NkDuplicate(const void* src, usize size) noexcept {
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

        uint32 NkChecksum(const void* data, usize size) noexcept {
            if (!data || size == 0u) {
                return 0u;
            }
            // FNV-1a hash : simple et rapide pour checksum non-crypto
            const uint8* bytes = static_cast<const uint8*>(data);
            uint32 hash = 2166136261u;  // FNV offset basis
            for (usize i = 0u; i < size; ++i) {
                hash ^= static_cast<uint32>(bytes[i]);
                hash *= 16777619u;       // FNV prime
            }
            return hash;
        }

        void NkSwapEndian(void* buffer, usize elementSize, usize count) noexcept {
            if (!buffer || elementSize <= 1u || count == 0u) {
                return;
            }
            uint8* bytes = static_cast<uint8*>(buffer);
            for (usize i = 0u; i < count; ++i) {
                uint8* elem = bytes + i * elementSize;
                ReverseRange(elem, elem + elementSize);
            }
        }

        void NkSecureZero(void* ptr, usize size) noexcept {
            if (!ptr || size == 0u) {
                return;
            }
            // volatile empêche l'optimisation du compiler
            volatile uint8* bytes = static_cast<volatile uint8*>(ptr);
            for (usize i = 0u; i < size; ++i) {
                bytes[i] = 0u;
            }
        }

        void NkZero(void* ptr, usize size) noexcept {
            if (!ptr || size == 0u) {
                return;
            }
            NkMemZero(ptr, size);
        }

        void* NkAllocAligned(usize alignment, usize size) noexcept {
            if (size == 0u) {
                return nullptr;
            }
            // Normalisation de l'alignement
            if (!NkIsPowerOfTwo(alignment)) {
                alignment = NK_MEMORY_DEFAULT_ALIGNMENT;
            }
            return NkAlloc(size, nullptr, alignment);
        }

        void NkFreeAligned(void* ptr, usize /*size*/) noexcept {
            NkFree(ptr);
        }

        int32 NkPosixAligned(void** ptr, usize alignment, usize size) noexcept {
            if (!ptr || size == 0u) {
                return -1;
            }
            *ptr = NkAllocAligned(alignment, size);
            return (*ptr != nullptr) ? 0 : -1;
        }

        bool NkAlignPointer(usize align, usize size, void*& outPtr, usize& space) noexcept {
            if (!outPtr || align == 0u || !NkIsPowerOfTwo(align)) {
                return false;
            }

            const usize address = reinterpret_cast<usize>(outPtr);
            const usize alignedAddress = NK_ALIGN_UP(address, align);
            const usize adjustment = alignedAddress - address;
            
            // Vérification qu'il reste assez d'espace après alignement
            if (adjustment > space) {
                return false;
            }
            if (size > (space - adjustment)) {
                return false;
            }

            // Mise à jour des paramètres out
            outPtr = reinterpret_cast<void*>(alignedAddress);
            space -= adjustment;
            return true;
        }

        void* NkAlignPointer(void* ptr, usize align, usize size) noexcept {
            if (!ptr || align == 0u || !NkIsPowerOfTwo(align)) {
                return nullptr;
            }
            void* out = ptr;
            usize space = static_cast<usize>(-1);  // "Infini" pour cette surcharge
            if (!NkAlignPointer(align, size, out, space)) {
                return nullptr;
            }
            return out;
        }

        void* NkMap(usize size) noexcept {
            if (size == 0u) {
                return nullptr;
            }
            return NkGetVirtualAllocator().Allocate(size, NK_MEMORY_DEFAULT_ALIGNMENT);
        }

        void NkUnmap(void* ptr, usize /*size*/) noexcept {
            if (!ptr) {
                return;
            }
            NkGetVirtualAllocator().Deallocate(ptr);
        }

    } // namespace memory
} // namespace nkentseu

// =============================================================================
// NOTES DE MAINTENANCE
// =============================================================================
/*
    [CORRECTNESS]
    ✓ Toutes les fonctions gèrent nullptr et size==0 de façon définie (no-op ou retour sécurisé)
    ✓ Bounds checking explicite pour les variantes avec offsets : pas de dépassement possible
    ✓ NkOverlaps utilise l'arithmétique d'adresses pour détection fiable de chevauchement
    ✓ NkSecureZero utilise volatile pour garantir que le zeroing n'est pas optimisé

    [PERFORMANCE]
    - NkCopy/NkMove/NkSet : délèguent à NkUtils avec optimisations AVX2 si disponibles
    - NkCompare : utilise NkMemCompare avec fallback builtin
    - Recherche de pattern : O(n*m) naïf - voir NkFunctionSIMD.h pour Boyer-Moore optimisé
    - NkRotate : triple-reverse O(n) optimal en temps et espace
    - Checksum/Hash : algorithmes simples, remplacer par SIMD si besoin de performance

    [EXTENSIONS POSSIBLES]
    1. Version SIMD des fonctions de recherche : NkSearchPatternSIMD avec Boyer-Moore vectorisé
    2. NkCompareConstantTime pour comparaison crypto-safe (timing attack resistant)
    3. NkTransform avec support de fonctions multi-octets (16-bit, 32-bit operations)
    4. NkScatter/Gather pour opérations non-contiguës optimisées

    [DEBUGGING TIPS]
    - Si NkCopy avec offsets copie moins que demandé : vérifier bounds checking avec IsValidSlice
    - Si NkFind ne trouve pas une valeur présente : vérifier que value est bien un uint8 (0-255)
    - Si NkOverlaps retourne faux pour des régions qui se chevauchent : vérifier que count est correct
    - Si NkSecureZero semble inefficace : compiler avec -fno-builtin-memset ou vérifier l'optimisation
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================