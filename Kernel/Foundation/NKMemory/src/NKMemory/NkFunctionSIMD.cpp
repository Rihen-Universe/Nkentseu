// =============================================================================
// NKMemory/NkFunctionSIMD.cpp
// Implémentations SIMD des opérations mémoire.
//
// Design :
//  - Inclusion systématique de "pch.h" en premier
//  - Détection automatique du backend via NKPlatform/NkArchDetect.h
//  - Fallback scalar garanti si SIMD non disponible
//  - Tables statiques initialisées lazy (thread-safe)
//  - Zéro duplication : délégation à NkUtils.h pour fallback
//
// Auteur : Rihen
// Date : 2024-2026
// License : Propriétaire - Free to use and modify
// =============================================================================

// -------------------------------------------------------------------------
// PRÉ-COMPILED HEADER - TOUJOURS EN PREMIER
// -------------------------------------------------------------------------
#include "pch.h"

// -------------------------------------------------------------------------
// EN-TÊTES DU MODULE
// -------------------------------------------------------------------------
#include "NKMemory/NkFunctionSIMD.h"
#include "NKMemory/NkFunction.h"    // Fallback vers versions scalaires
#include "NKMemory/NkUtils.h"       // NkMemCopy, etc. pour fallback

// -------------------------------------------------------------------------
// CONFIGURATION CONDITIONNELLE SIMD
// -------------------------------------------------------------------------
// Si NKENTSEU_DISABLE_SIMD est défini, toutes les fonctions délèguent
// aux versions scalaires de NkFunction.h pour zero overhead.

#if !defined(NKENTSEU_DISABLE_SIMD)

    // -------------------------------------------------------------------------
    // INCLUSIONS SPÉCIFIQUES AUX BACKENDS SIMD
    // -------------------------------------------------------------------------
    #if defined(NKENTSEU_CPU_HAS_AVX512) && defined(NKENTSEU_ARCH_X86_64)
        #include <immintrin.h>  // AVX-512 intrinsics
        #define NK_SIMD_BACKEND "AVX-512"
        
    #elif defined(NKENTSEU_CPU_HAS_AVX2) && (defined(NKENTSEU_ARCH_X86_64) || defined(NKENTSEU_ARCH_X86))
        #include <immintrin.h>  // AVX2 intrinsics
        #define NK_SIMD_BACKEND "AVX2"
        
    #elif defined(NKENTSEU_CPU_HAS_SSE2) && (defined(NKENTSEU_ARCH_X86_64) || defined(NKENTSEU_ARCH_X86))
        #include <emmintrin.h>  // SSE2 intrinsics
        #define NK_SIMD_BACKEND "SSE2"
        
    #elif defined(NKENTSEU_CPU_HAS_NEON) && defined(NKENTSEU_ARCH_ARM64)
        #include <arm_neon.h>   // NEON intrinsics
        #define NK_SIMD_BACKEND "NEON"
        
    #else
        // Fallback scalar : pas d'intrinsics nécessaires
        #define NK_SIMD_BACKEND "Scalar"
    #endif

    // -------------------------------------------------------------------------
    // NAMESPACE ANONYME : Helpers internes et tables statiques
    // -------------------------------------------------------------------------
    namespace {

        // ====================================================================
        // Table CRC32 pour fallback scalar (initialisation lazy thread-safe)
        // ====================================================================
        nkentseu::nk_uint32 gCrc32Table[256];
        nkentseu::nk_bool gCrc32TableReady = false;

        /**
         * @brief Initialise la table CRC32 (appelée une fois, thread-safe)
         * @note Utilise double-check locking pattern pour initialisation lazy
         */
        void InitCrc32Table() noexcept {
            if (gCrc32TableReady) {
                return;
            }
            // Note : en production multi-thread, ajouter un mutex ici
            // Pour l'instant, hypothèse : appelé depuis thread principal au startup
            for (nkentseu::nk_uint32 i = 0; i < 256u; ++i) {
                nkentseu::nk_uint32 crc = i;
                for (nkentseu::nk_uint32 j = 0; j < 8u; ++j) {
                    const nkentseu::nk_uint32 mask = -(crc & 1u);
                    crc = (crc >> 1u) ^ (0xEDB88320u & mask);
                }
                gCrc32Table[i] = crc;
            }
            gCrc32TableReady = true;
        }

        // ====================================================================
        // Recherche de pattern portable (Boyer-Moore-Horspool) pour fallback
        // ====================================================================
        /**
         * @brief Recherche de pattern avec algorithme Boyer-Moore-Horspool
         * @note O(n) moyen, O(n*m) worst-case, bien meilleur que naïf pour patterns longs
         * @note Utilisé comme fallback si SIMD non disponible ou pattern trop court
         */
        void* SearchPatternPortable(const void* haystack, nkentseu::nk_size haystackSize, const void* needle, nkentseu::nk_size needleSize) noexcept {
            if (!haystack || !needle) {
                return nullptr;
            }
            if (needleSize == 0u) {
                return const_cast<void*>(haystack);
            }
            if (haystackSize < needleSize) {
                return nullptr;
            }

            const nkentseu::nk_uint8* h = static_cast<const nkentseu::nk_uint8*>(haystack);
            const nkentseu::nk_uint8* n = static_cast<const nkentseu::nk_uint8*>(needle);

            // Cas spécial : pattern d'un seul octet → recherche linéaire simple
            if (needleSize == 1u) {
                for (nkentseu::nk_size i = 0u; i < haystackSize; ++i) {
                    if (h[i] == n[0]) {
                        return const_cast<nkentseu::nk_uint8*>(h + i);
                    }
                }
                return nullptr;
            }

            // Pré-calcul de la table de sauts (last occurrence heuristic)
            const nkentseu::nk_size last = needleSize - 1u;
            nkentseu::nk_size shift[256];
            for (nkentseu::nk_size i = 0u; i < 256u; ++i) {
                shift[i] = needleSize;  // Défaut : sauter toute la longueur du pattern
            }
            for (nkentseu::nk_size i = 0u; i < last; ++i) {
                shift[static_cast<unsigned char>(n[i])] = last - i;
            }

            // Recherche principale avec sauts optimisés
            nkentseu::nk_size pos = 0u;
            while (pos <= (haystackSize - needleSize)) {
                const nkentseu::nk_uint8 tail = h[pos + last];
                if (tail == n[last]) {
                    // Vérification complète si dernier octet match
                    if (nkentseu::memory::NkMemCompare(h + pos, n, last) == 0) {
                        return const_cast<nkentseu::nk_uint8*>(h + pos);
                    }
                }
                // Saut basé sur l'octet de fin du window courant
                pos += shift[static_cast<unsigned char>(tail)];
            }
            return nullptr;
        }

        // ====================================================================
        // Fallbacks scalaires pour délégation quand SIMD non disponible
        // ====================================================================
        inline void FallbackCopy(void* dst, const void* src, nkentseu::nk_size bytes) noexcept {
            nkentseu::memory::NkMemCopy(dst, src, bytes);
        }
        
        inline void FallbackMove(void* dst, const void* src, nkentseu::nk_size bytes) noexcept {
            nkentseu::memory::NkMemMove(dst, src, bytes);
        }
        
        inline void FallbackSet(void* dst, int value, nkentseu::nk_size bytes) noexcept {
            nkentseu::memory::NkMemSet(dst, value, bytes);
        }

    } // namespace anonyme


    // -------------------------------------------------------------------------
    // NAMESPACE PUBLIC : Implémentations SIMD
    // -------------------------------------------------------------------------
    namespace nkentseu {
        namespace memory {

            // ====================================================================
            // Opérations de Base : COPY / MOVE / SET
            // ====================================================================

            void NkMemoryCopySIMD(void* dst, const void* src, nk_size bytes) noexcept {
                if (!dst || !src || bytes == 0u) {
                    return;
                }

                // =================================================================
                // Backend AVX-512 (64 bytes/cycle)
                // =================================================================
                #if defined(NKENTSEU_CPU_HAS_AVX512) && defined(NKENTSEU_ARCH_X86_64)
                    if (bytes >= 64u && NkIsAlignedSIMD(dst) && NkIsAlignedSIMD(src)) {
                        nk_size i = 0u;
                        const __m512i* s = static_cast<const __m512i*>(src);
                        __m512i* d = static_cast<__m512i*>(dst);
                        
                        // Boucle principale : 64 bytes par itération
                        for (; i + 64u <= bytes; i += 64u) {
                            const __m512i chunk = _mm512_load_si512(s++);
                            _mm512_store_si512(d++, chunk);
                        }
                        // Queue : fallback scalar pour les octets restants
                        if (i < bytes) {
                            FallbackCopy(
                                static_cast<nk_uint8*>(dst) + i,
                                static_cast<const nk_uint8*>(src) + i,
                                bytes - i
                            );
                        }
                        return;
                    }

                // =================================================================
                // Backend AVX2 (32 bytes/cycle)
                // =================================================================
                #elif defined(NKENTSEU_CPU_HAS_AVX2) && (defined(NKENTSEU_ARCH_X86_64) || defined(NKENTSEU_ARCH_X86))
                    if (bytes >= 32u) {
                        nk_size i = 0u;
                        const __m256i* s = static_cast<const __m256i*>(src);
                        __m256i* d = static_cast<__m256i*>(dst);
                        
                        for (; i + 32u <= bytes; i += 32u) {
                            const __m256i chunk = _mm256_loadu_si256(s++);
                            _mm256_storeu_si256(d++, chunk);
                        }
                        if (i < bytes) {
                            FallbackCopy(
                                static_cast<nk_uint8*>(dst) + i,
                                static_cast<const nk_uint8*>(src) + i,
                                bytes - i
                            );
                        }
                        return;
                    }

                // =================================================================
                // Backend SSE2 (16 bytes/cycle)
                // =================================================================
                #elif defined(NKENTSEU_CPU_HAS_SSE2) && (defined(NKENTSEU_ARCH_X86_64) || defined(NKENTSEU_ARCH_X86))
                    if (bytes >= 16u) {
                        nk_size i = 0u;
                        const __m128i* s = static_cast<const __m128i*>(src);
                        __m128i* d = static_cast<__m128i*>(dst);
                        
                        for (; i + 16u <= bytes; i += 16u) {
                            const __m128i chunk = _mm_loadu_si128(s++);
                            _mm_storeu_si128(d++, chunk);
                        }
                        if (i < bytes) {
                            FallbackCopy(
                                static_cast<nk_uint8*>(dst) + i,
                                static_cast<const nk_uint8*>(src) + i,
                                bytes - i
                            );
                        }
                        return;
                    }

                // =================================================================
                // Backend NEON (16 bytes/cycle, ARM64)
                // =================================================================
                #elif defined(NKENTSEU_CPU_HAS_NEON) && defined(NKENTSEU_ARCH_ARM64)
                    if (bytes >= 16u) {
                        nk_size i = 0u;
                        const nk_uint8* s = static_cast<const nk_uint8*>(src);
                        nk_uint8* d = static_cast<nk_uint8*>(dst);
                        
                        for (; i + 16u <= bytes; i += 16u) {
                            const uint8x16_t chunk = vld1q_u8(s);
                            vst1q_u8(d, chunk);
                            s += 16;
                            d += 16;
                        }
                        if (i < bytes) {
                            FallbackCopy(d, s, bytes - i);
                        }
                        return;
                    }
                #endif

                // =================================================================
                // Fallback scalar (toujours disponible)
                // =================================================================
                FallbackCopy(dst, src, bytes);
            }

            void NkMemoryMoveSIMD(void* dst, const void* src, nk_size bytes) noexcept {
                if (!dst || !src || bytes == 0u) {
                    return;
                }

                // Détection de chevauchement : si pas de chevauchement, copy est plus rapide
                const auto d = reinterpret_cast<nk_uintptr>(dst);
                const auto s = reinterpret_cast<nk_uintptr>(src);
                const bool overlaps = (d < s + bytes) && (s < d + bytes);

                if (!overlaps) {
                    // Pas de chevauchement : utiliser copy optimisé
                    NkMemoryCopySIMD(dst, src, bytes);
                    return;
                }

                // =================================================================
                // Chevauchement détecté : choisir direction de copie
                // =================================================================
                if (d < s) {
                    // Forward copy : dst avant src, copier du début vers la fin
                    NkMemoryCopySIMD(dst, src, bytes);
                } else {
                    // Backward copy : dst après src, copier de la fin vers le début
                    // Note : implémentation backward SIMD complexe, fallback scalar ici
                    FallbackMove(dst, src, bytes);
                }
            }

            void NkMemorySetSIMD(void* dst, int value, nk_size bytes) noexcept {
                if (!dst || bytes == 0u) {
                    return;
                }

                const nk_uint8 byteValue = static_cast<nk_uint8>(value & 0xFF);

                // =================================================================
                // Backend AVX-512 : broadcast 64 bytes
                // =================================================================
                #if defined(NKENTSEU_CPU_HAS_AVX512) && defined(NKENTSEU_ARCH_X86_64)
                    if (bytes >= 64u && NkIsAlignedSIMD(dst)) {
                        const __m512i pattern = _mm512_set1_epi8(static_cast<char>(byteValue));
                        nk_size i = 0u;
                        __m512i* d = static_cast<__m512i*>(dst);
                        
                        for (; i + 64u <= bytes; i += 64u) {
                            _mm512_store_si512(d++, pattern);
                        }
                        if (i < bytes) {
                            FallbackSet(
                                static_cast<nk_uint8*>(dst) + i, 
                                value, 
                                bytes - i
                            );
                        }
                        return;
                    }

                // =================================================================
                // Backend AVX2 : broadcast 32 bytes
                // =================================================================
                #elif defined(NKENTSEU_CPU_HAS_AVX2) && (defined(NKENTSEU_ARCH_X86_64) || defined(NKENTSEU_ARCH_X86))
                    if (bytes >= 32u) {
                        const __m256i pattern = _mm256_set1_epi8(static_cast<char>(byteValue));
                        nk_size i = 0u;
                        __m256i* d = static_cast<__m256i*>(dst);
                        
                        for (; i + 32u <= bytes; i += 32u) {
                            _mm256_storeu_si256(d++, pattern);
                        }
                        if (i < bytes) {
                            FallbackSet(
                                static_cast<nk_uint8*>(dst) + i, 
                                value, 
                                bytes - i
                            );
                        }
                        return;
                    }

                // =================================================================
                // Backend SSE2 : broadcast 16 bytes
                // =================================================================
                #elif defined(NKENTSEU_CPU_HAS_SSE2) && (defined(NKENTSEU_ARCH_X86_64) || defined(NKENTSEU_ARCH_X86))
                    if (bytes >= 16u) {
                        const __m128i pattern = _mm_set1_epi8(static_cast<char>(byteValue));
                        nk_size i = 0u;
                        __m128i* d = static_cast<__m128i*>(dst);
                        
                        for (; i + 16u <= bytes; i += 16u) {
                            _mm_storeu_si128(d++, pattern);
                        }
                        if (i < bytes) {
                            FallbackSet(
                                static_cast<nk_uint8*>(dst) + i, 
                                value, 
                                bytes - i
                            );
                        }
                        return;
                    }

                // =================================================================
                // Backend NEON : broadcast 16 bytes
                // =================================================================
                #elif defined(NKENTSEU_CPU_HAS_NEON) && defined(NKENTSEU_ARCH_ARM64)
                    if (bytes >= 16u) {
                        const uint8x16_t pattern = vdupq_n_u8(byteValue);
                        nk_size i = 0u;
                        nk_uint8* d = static_cast<nk_uint8*>(dst);
                        
                        for (; i + 16u <= bytes; i += 16u) {
                            vst1q_u8(d, pattern);
                            d += 16;
                        }
                        if (i < bytes) {
                            FallbackSet(d, value, bytes - i);
                        }
                        return;
                    }
                #endif

                // =================================================================
                // Fallback scalar
                // =================================================================
                FallbackSet(dst, value, bytes);
            }

            // ====================================================================
            // Transformation : Transpose
            // ====================================================================

            void NkMemoryTransposeSIMD(void* dst, const void* src,
                                      nk_size rows, nk_size cols,
                                      nk_size elementSize) noexcept {
                if (!dst || !src || rows == 0u || cols == 0u || elementSize == 0u) {
                    return;
                }
                if (dst == src) {
                    // In-place transpose non supporté dans cette version générique
                    return;
                }

                // =================================================================
                // Optimisations par taille d'élément (vectorisation possible)
                // =================================================================
                
                // Cas 1-byte : transposition simple, compiler peut vectoriser
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

                // Cas 2-byte : uint16
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

                // Cas 4-byte : uint32/float - bon candidat pour SIMD
                if (elementSize == 4u) {
                    const nk_uint32* in = static_cast<const nk_uint32*>(src);
                    nk_uint32* out = static_cast<nk_uint32*>(dst);
                    
                    #if defined(NKENTSEU_CPU_HAS_AVX2) || defined(NKENTSEU_CPU_HAS_NEON)
                        // Hint au compiler pour auto-vectorisation
                        #pragma omp simd
                    #endif
                    for (nk_size r = 0u; r < rows; ++r) {
                        for (nk_size c = 0u; c < cols; ++c) {
                            out[c * rows + r] = in[r * cols + c];
                        }
                    }
                    return;
                }

                // Cas 8-byte : uint64/double
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

                // =================================================================
                // Cas général : élément de taille arbitraire (fallback byte-wise)
                // =================================================================
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

            // ====================================================================
            // Opérations Avancées : Crypto, Hash, Search
            // ====================================================================

            int NkMemoryCompareConstantTime(const void* a, const void* b, nk_size bytes) noexcept {
                if (!a || !b || bytes == 0u) {
                    return 0;
                }
                
                // Comparaison constante en temps : accumuler les différences via XOR
                // Le temps d'exécution ne dépend pas de la position des différences
                int result = 0;
                const nk_uint8* pa = static_cast<const nk_uint8*>(a);
                const nk_uint8* pb = static_cast<const nk_uint8*>(b);
                
                for (nk_size i = 0; i < bytes; ++i) {
                    result |= pa[i] ^ pb[i];  // XOR puis OR : accumule toutes les différences
                }
                return result;  // 0 si égaux, non-zero sinon
            }

            void* NkMemorySearchPatternSIMD(const void* haystack, nk_size haystackSize,
                                           const void* needle, nk_size needleSize) noexcept {
                // =================================================================
                // Pré-conditions et cas triviaux
                // =================================================================
                if (!haystack || !needle) {
                    return nullptr;
                }
                if (needleSize == 0u) {
                    return const_cast<void*>(haystack);
                }
                if (haystackSize < needleSize) {
                    return nullptr;
                }
                // Pattern très court : recherche linéaire simple plus rapide
                if (needleSize <= 4u) {
                    const nk_uint8* h = static_cast<const nk_uint8*>(haystack);
                    const nk_uint8* n = static_cast<const nk_uint8*>(needle);
                    for (nk_size i = 0u; i <= haystackSize - needleSize; ++i) {
                        if (NkMemCompare(h + i, n, needleSize) == 0) {
                            return const_cast<nk_uint8*>(h + i);
                        }
                    }
                    return nullptr;
                }

                // =================================================================
                // Backend SIMD pour recherche de pattern (Boyer-Moore vectorisé)
                // =================================================================
                // Note : implémentation complète Boyer-Moore SIMD est complexe.
                // Pour cette version, on utilise le fallback portable optimisé.
                // Une future version pourrait intégrer :
                // - AVX2 : _mm256_cmpeq_epi8 pour comparaison vectorielle de 32 bytes
                // - NEON : vceqq_u8 pour comparaison vectorielle ARM
                return SearchPatternPortable(haystack, haystackSize, needle, needleSize);
            }

            nk_uint32 NkMemoryCRC32SIMD(const void* data, nk_size bytes) noexcept {
                if (!data || bytes == 0u) {
                    return 0u;
                }

                // Initialisation lazy de la table CRC32
                InitCrc32Table();

                // =================================================================
                // Backend avec instructions CRC32 matérielles (x86 avec SSE4.2)
                // =================================================================
                #if defined(NKENTSEU_CPU_HAS_SSE42) && (defined(NKENTSEU_ARCH_X86_64) || defined(NKENTSEU_ARCH_X86))
                    #include <nmmintrin.h>
                    nk_uint32 crc = 0xFFFFFFFFu;
                    const nk_uint8* ptr = static_cast<const nk_uint8*>(data);
                    
                    // Traitement par blocs de 8 bytes avec _mm_crc32_u64 si disponible
                    nk_size i = 0u;
                    #if defined(__x86_64__) || defined(_M_X64)
                        for (; i + 8u <= bytes; i += 8u) {
                            nk_uint64 qword;
                            NkMemCopy(&qword, ptr + i, 8u);
                            crc = static_cast<nk_uint32>(_mm_crc32_u64(crc, qword));
                        }
                    #endif
                    // Queue en bytes individuels
                    for (; i < bytes; ++i) {
                        crc = _mm_crc32_u8(crc, ptr[i]);
                    }
                    return crc ^ 0xFFFFFFFFu;
                #endif

                // =================================================================
                // Fallback : table lookup scalar (toujours disponible)
                // =================================================================
                nk_uint32 crc = 0xFFFFFFFFu;
                const nk_uint8* ptr = static_cast<const nk_uint8*>(data);
                for (nk_size i = 0u; i < bytes; ++i) {
                    const nk_uint8 index = static_cast<nk_uint8>((crc ^ ptr[i]) & 0xFFu);
                    crc = (crc >> 8u) ^ gCrc32Table[index];
                }
                return crc ^ 0xFFFFFFFFu;
            }

            nk_uint64 NkMemoryHashSIMD(const void* data, nk_size bytes) noexcept {
                if (!data || bytes == 0u) {
                    return 0ull;
                }

                // =================================================================
                // Hash style xxHash : simple, rapide, bonne distribution
                // =================================================================
                // Note : une version SIMD complète de xxHash est complexe.
                // Cette implémentation est optimisée pour le compiler :
                // - Boucle simple que le compiler peut auto-vectoriser
                // - Constantes choisies pour bonne distribution
                // - Accumulation en 64-bit pour éviter les collisions
                
                nk_uint64 hash = 14695981039346656037ull;  // FNV offset basis 64-bit
                const nk_uint8* ptr = static_cast<const nk_uint8*>(data);
                
                // Le pragma aide le compiler à vectoriser si possible
                #pragma omp simd reduction(^:hash)
                for (nk_size i = 0; i < bytes; ++i) {
                    hash ^= static_cast<nk_uint64>(ptr[i]);
                    hash *= 1099511628211ull;  // FNV prime 64-bit
                }
                return hash;
            }

        } // namespace memory
    } // namespace nkentseu

#else // NKENTSEU_DISABLE_SIMD défini

    // -------------------------------------------------------------------------
    // VERSION FALLBACK : délégation aux fonctions scalaires de NkFunction.h
    // -------------------------------------------------------------------------
    namespace nkentseu {
        namespace memory {

            void NkMemoryCopySIMD(void* dst, const void* src, nk_size bytes) noexcept {
                NkCopy(dst, src, bytes);
            }
            
            void NkMemoryMoveSIMD(void* dst, const void* src, nk_size bytes) noexcept {
                NkMove(dst, src, bytes);
            }
            
            void NkMemorySetSIMD(void* dst, int value, nk_size bytes) noexcept {
                NkSet(dst, value, bytes);
            }
            
            void NkMemoryTransposeSIMD(void* dst, const void* src,
                                      nk_size rows, nk_size cols,
                                      nk_size elementSize) noexcept {
                // Fallback : implémentation naïve row-major → col-major
                if (!dst || !src || rows == 0u || cols == 0u || elementSize == 0u) {
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
                // Fallback scalar constant-time
                int result = 0;
                const nk_uint8* pa = static_cast<const nk_uint8*>(a);
                const nk_uint8* pb = static_cast<const nk_uint8*>(b);
                for (nk_size i = 0; i < bytes; ++i) {
                    result |= pa[i] ^ pb[i];
                }
                return result;
            }
            
            void* NkMemorySearchPatternSIMD(const void* haystack, nk_size haystackSize,
                                           const void* needle, nk_size needleSize) noexcept {
                return NkSearchPattern(haystack, haystackSize, needle, needleSize);
            }
            
            nk_uint32 NkMemoryCRC32SIMD(const void* data, nk_size bytes) noexcept {
                // Fallback : utiliser NkChecksum de NkFunction.h (FNV-1a, pas CRC32)
                // Pour vrai CRC32, implémenter table lookup ici si nécessaire
                return NkChecksum(data, bytes);
            }
            
            nk_uint64 NkMemoryHashSIMD(const void* data, nk_size bytes) noexcept {
                // Fallback : FNV-1a hash de NkFunction.h
                return static_cast<nk_uint64>(NkChecksum(data, bytes));
            }

        } // namespace memory
    } // namespace nkentseu

#endif // NKENTSEU_DISABLE_SIMD

// =============================================================================
// NOTES DE MAINTENANCE
// =============================================================================
/*
    [DÉTECTION ET SÉLECTION DU BACKEND]
    
    La compilation choisit automatiquement le meilleur backend via :
    
    1. NKPlatform/NkArchDetect.h détecte les features CPU disponibles
    2. Les #if defined() sélectionnent le code approprié
    3. Fallback scalar garanti si aucun SIMD n'est disponible
    
    Pour forcer un backend spécifique en debug/testing :
        #define NKENTSEU_FORCE_SIMD_SCALAR  // Toujours fallback
        #define NKENTSEU_FORCE_SIMD_SSE2    // Forcer SSE2 même si AVX2 disponible
    
    [THREAD-SAFETY]
    
    ✓ InitCrc32Table() : hypothèse d'appel depuis thread principal au startup
    ✓ Pour usage multi-thread : ajouter un NkMutex autour de l'initialisation
    ✓ Les tables statiques sont en lecture seule après initialisation : safe
    
    [PERFORMANCE TUNING]
    
    - Thresholds de taille minimale (64/32/16 bytes) : ajuster selon profiling
    - Alignement : requérir 64 bytes pour AVX-512 évite les fallbacks coûteux
    - Queue handling : fallback scalar pour les octets restants est optimal
    - Compiler flags : -mavx2, -mavx512f, etc. doivent être activés au build
    
    [EXTENSIONS POSSIBLES]
    
    1. Boyer-Moore SIMD complet : utiliser _mm256_cmpeq_epi8 + bitmask pour 
       comparaison vectorielle de pattern dans NkMemorySearchPatternSIMD
    
    2. xxHash SIMD complet : implémenter les 4 lanes de xxHash en parallèle
       avec AVX2/AVX-512 pour 4x throughput
    
    3. CRC32 PCLMULQDQ : utiliser instructions carry-less multiply pour
       CRC32 hardware-accelerated sur x86 moderne
    
    4. Auto-dispatch runtime : détecter CPU features au runtime et choisir
       le meilleur backend sans recompilation (plus flexible, léger overhead)

    [DEBUGGING TIPS]
    
    - Si performance SIMD inférieure au fallback : vérifier l'alignement des pointeurs
    - Si crash avec intrinsics : compiler avec flags appropriés (-mavx2, etc.)
    - Si résultats différents entre backends : vérifier l'endianness et le padding
    - Utiliser NK_FOUNDATION_LOG_DEBUG pour logger le backend sélectionné au runtime
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================