// =============================================================================
// NKMemory/NkUtils.
// Utilitaires mémoire génériques : opérations mémoire optimisées.
//
// Design :
//  - NK_ALIGN_UP / NK_ALIGN_DOWN : fournis par NKCore/NkTypeUtils.h (zero duplication)
//  - Implémentations optimisées spécifiques à NKMemory uniquement si nécessaire
//  - Macros NKENTSEU_MEMORY_API pour la visibilité des symboles
//  - Compatibilité multiplateforme via NKPlatform/NkArchDetect.h
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_MEMORY_NKUTILS_H
#define NKENTSEU_MEMORY_NKUTILS_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKMemory/NkMemoryApi.h"      // NKENTSEU_MEMORY_API, inline macros
    #include "NKCore/NkTypes.h"            // nk_size, nk_bool, nk_uintptr, etc.
    #include "NKCore/NkTypeUtils.h"        // NK_ALIGN_UP, NK_ALIGN_DOWN, NK_BIT, etc.
    #include "NKPlatform/NkArchDetect.h"   // Détection architecture/CPU features

    // -------------------------------------------------------------------------
    // SECTION 2 : PRÉ-DÉCLARATIONS
    // -------------------------------------------------------------------------
    // Note: NK_ALIGN_UP(x, a) et NK_ALIGN_DOWN(x, a) sont fournis par NKCore/NkTypeUtils.h.

    namespace nkentseu {
        namespace memory {

            // =================================================================
            // @defgroup MemoryUtilsAlign Utilitaires d'Alignement
            // =================================================================

            /** @brief Retourne true si value est une puissance de 2. */
            [[nodiscard]] NKENTSEU_MEMORY_API_INLINE
            nk_bool NkIsPowerOfTwo(nk_size value) noexcept;

            /** @brief Retourne true si ptr est aligné sur alignment (puissance de 2). */
            [[nodiscard]] NKENTSEU_MEMORY_API_INLINE
            nk_bool NkIsAlignedPtr(const void* ptr, nk_size alignment) noexcept;

            // =================================================================
            // @defgroup MemoryUtilsOps Opérations Mémoire Optimisées
            // =================================================================

            /** @brief Remplit size octets de destination avec value. */
            NKENTSEU_MEMORY_API
            void* NkMemSet(void* destination, nk_int32 value, nk_size size) noexcept;

            /** @brief Copie size octets de source vers destination (non chevauchants). */
            NKENTSEU_MEMORY_API
            void* NkMemCopy(void* destination, const void* source, nk_size size) noexcept;

            /** @brief Copie size octets (chevauchement autorisé). */
            NKENTSEU_MEMORY_API
            void* NkMemMove(void* destination, const void* source, nk_size size) noexcept;

            /** @brief Compare size octets entre a et b. */
            NKENTSEU_MEMORY_API
            nk_int32 NkMemCompare(const void* a, const void* b, nk_size size) noexcept;

            /** @brief Met size octets de destination à 0. */
            NKENTSEU_MEMORY_API_INLINE
            void* NkMemZero(void* destination, nk_size size) noexcept;

        } // namespace memory
    } // namespace nkentseu

    // -------------------------------------------------------------------------
    // SECTION 3 : IMPLÉMENTATIONS INLINE
    // -------------------------------------------------------------------------

    namespace nkentseu {
        namespace memory {

            NKENTSEU_MEMORY_API_INLINE
            nk_bool NkIsPowerOfTwo(nk_size value) noexcept {
                return value != 0u && (value & (value - 1u)) == 0u;
            }

            NKENTSEU_MEMORY_API_INLINE
            nk_bool NkIsAlignedPtr(const void* ptr, nk_size alignment) noexcept {
                if (alignment <= 1u) {
                    return true;
                }
                const nk_uintptr addr = reinterpret_cast<nk_uintptr>(ptr);
                return (addr & (alignment - 1u)) == 0u;
            }

            NKENTSEU_MEMORY_API_INLINE
            void* NkMemZero(void* destination, nk_size size) noexcept {
                return NkMemSet(destination, 0, size);
            }

        } // namespace memory
    } // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKUTILS_H

// =============================================================================
// NOTES D'UTILISATION
// =============================================================================
/*
    // Exemple 1 : Alignement de valeurs
    nk_size aligned = nkentseu::memory::NkAlignUp(100, 64);  // Retourne 128
    nk_size down    = nkentseu::memory::NkAlignDown(100, 64); // Retourne 64

    // Exemple 2 : Vérification d'alignement de pointeur
    void* buffer = malloc(1024);
    if (nkentseu::memory::NkIsAlignedPtr(buffer, 16)) {
        // Buffer est aligné 16 bytes - sûr pour SSE
    }

    // Exemple 3 : Copie mémoire optimisée
    char src[256], dst[256];
    nkentseu::memory::NkMemCopy(dst, src, sizeof(src));  // Peut utiliser AVX2

    // Exemple 4 : Zéro-initialisation rapide
    struct Data { int x, y, z; } data;
    nkentseu::memory::NkMemZero(&data, sizeof(data));
*/

// =============================================================================
// OPTIMISATIONS PLATEFORME-SPECIFIQUES
// =============================================================================
/*
    Ce fichier implémente des optimisations conditionnelles :

    | Feature              | Condition                                    | Impact |
    |---------------------|----------------------------------------------|--------|
    | AVX2 MemCopy        | NKENTSEU_CPU_HAS_AVX2 && x86/x86_64         | +30-50% throughput grandes copies |
    | Streaming stores    | Aligned dest + taille > 256KB               | Évite la pollution du cache L1/L2 |
    | Prefetching         | Copies > 64 bytes avec AVX2                 | Réduction des stalls mémoire |
    | Builtins compiler | Clang/GCC : __builtin_* pour meilleures optis | Vectorisation auto par le compiler |

    Les fallbacks standards (memset/memcpy/memmove) sont toujours disponibles
    et utilisés automatiquement si les optimisations ne sont pas applicables.
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================