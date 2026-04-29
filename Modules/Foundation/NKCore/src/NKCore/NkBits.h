// =============================================================================
// NKCore/NkBits.h
// Manipulation de bits avancée avec optimisations plateforme-spécifiques.
//
// Design :
//  - Macros NK_POPCOUNT32/64, NK_CTZ32/64, NK_CLZ32/64 avec intrinsics compiler
//  - Macros NK_BYTESWAP16/32/64 pour changement d'endianness optimisé
//  - Classe NkBits avec méthodes templates pour opérations bitwise génériques
//  - Fallback software portable quand les intrinsics ne sont pas disponibles
//  - Fonctions NextPowerOfTwo() implémentées dans NkBits.cpp
//  - Intégration avec NkTypes.h pour types portables nk_uint32, etc.
//  - Assertions NKENTSEU_ASSERT_MSG pour validation des paramètres critiques
//
// Intégration :
//  - Utilise NKPlatform/NkCompilerDetect.h pour détection du compilateur
//  - Utilise NKCore/NkTypes.h pour les types fondamentaux portables
//  - Utilise NKCore/Assert/NkAssert.h pour assertions de débogage
//  - Compatible avec <intrin.h> sur MSVC, built-ins sur GCC/Clang
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CORE_NKBITS_H_INCLUDED
#define NKENTSEU_CORE_NKBITS_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules de détection, types et assertions.

    #include "NKPlatform/NkCompilerDetect.h"
    #include "NkTypes.h"
    #include "NkPlatform.h"
    #include "Assert/NkAssert.h"

    // En-têtes standards pour types fixes et fonctions utilitaires
    #include <cstdint>
    #include <cstdlib>
    #include <stdlib.h>

    // Intrinsics MSVC/Windows pour opérations bits optimisées
    #if defined(_MSC_VER) || defined(_WIN32)
        #include <intrin.h>
    #endif

    // -------------------------------------------------------------------------
    // SECTION 2 : MACROS FONDAMENTALES DE MANIPULATION DE BITS
    // -------------------------------------------------------------------------
    // Définitions conditionnelles selon le compilateur pour performance maximale.

    /**
     * @defgroup BitMacros Macros de Manipulation de Bits
     * @brief Macros optimisées pour opérations bitwise courantes
     * @ingroup BitUtilities
     *
     * Ces macros fournissent un accès portable aux opérations de bits
     * les plus fréquentes, avec sélection automatique des intrinsics
     * du compilateur quand disponibles, ou fallback software sinon.
     *
     * @note
     *   - NK_POPCOUNT* : compte les bits à 1 (population count)
     *   - NK_CTZ* : compte les zéros trailing (depuis la droite)
     *   - NK_CLZ* : compte les zéros leading (depuis la gauche)
     *   - NK_BYTESWAP* : inverse l'ordre des octets (endianness)
     *
     * @warning
     *   Les fonctions CTZ/CLZ retournent la taille du type en bits
     *   quand l'entrée est zéro : vérifier avant d'utiliser le résultat
     *   comme index de tableau ou shift.
     *
     * @example
     * @code
     * // Compter les bits actifs dans un mask de permissions
     * nkentseu::nk_uint32 permissions = 0b10110100;
     * int activeCount = NK_POPCOUNT32(permissions);  // activeCount == 3
     *
     * // Trouver le premier bit actif (pour allocation de slot)
     * nkentseu::nk_uint32 freeSlots = 0b00001000;
     * int firstFree = NK_CTZ32(freeSlots);  // firstFree == 3
     *
     * // Conversion big-endian ↔ little-endian pour réseau
     * nkentseu::nk_uint32 hostValue = 0x12345678;
     * nkentseu::nk_uint32 netValue = NK_BYTESWAP32(hostValue);  // 0x78563412
     * @endcode
     */
    /** @{ */

    // ------------------------------------------------------------------
    // POPULATION COUNT (Compter les bits à 1)
    // ------------------------------------------------------------------

    /**
     * @brief Compte le nombre de bits à 1 dans un entier 32-bit
     * @def NK_POPCOUNT32(x)
     * @param x Valeur 32-bit à analyser
     * @return Nombre de bits positionnés à 1 (0-32)
     * @ingroup BitMacros
     *
     * @note
     *   - MSVC : utilise __popcnt (instruction POPCNT si CPU supporte)
     *   - GCC/Clang : utilise __builtin_popcount (peut générer POPCNT)
     *   - Fallback : implémentation software via NkBits::CountBitsSoftware()
     *
     * @warning
     *   L'instruction POPCNT nécessite un CPU avec support SSE4.2.
     *   Le fallback software est utilisé automatiquement si non disponible.
     */
    #if defined(NKENTSEU_COMPILER_MSVC)
        #define NK_POPCOUNT32(x) __popcnt(static_cast<nkentseu::nk_uint32>(x))
        #define NK_POPCOUNT64(x) __popcnt64(static_cast<nkentseu::nk_uint64>(x))
    #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
        #define NK_POPCOUNT32(x) __builtin_popcount(static_cast<nkentseu::nk_uint32>(x))
        #define NK_POPCOUNT64(x) __builtin_popcountll(static_cast<nkentseu::nk_uint64>(x))
    #else
        #define NK_POPCOUNT32(x) nkentseu::NkBits::CountBitsSoftware(static_cast<nkentseu::nk_uint32>(x))
        #define NK_POPCOUNT64(x) nkentseu::NkBits::CountBitsSoftware(static_cast<nkentseu::nk_uint64>(x))
    #endif

    // ------------------------------------------------------------------
    // COUNT TRAILING ZEROS (Compter les zéros à droite / LSB)
    // ------------------------------------------------------------------

    /**
     * @brief Compte les zéros consécutifs depuis le bit de poids faible
     * @param x Valeur 32-bit à analyser
     * @return Nombre de zéros trailing (0-31), ou 32 si x == 0
     * @ingroup BitMacros
     *
     * @note
     *   Utile pour trouver le premier bit à 1 depuis la droite :
     *   - Allocation de ressources par bit mask
     *   - Détection de facteurs de puissance de 2
     *   - Optimisation de boucles sur bits actifs
     *
     * @warning
     *   Retourne 32 si x == 0 : vérifier avant d'utiliser comme index.
     *   Utiliser FindFirstSet() pour une interface plus safe (-1 si aucun bit).
     */
    NKENTSEU_FORCE_INLINE
    static nkentseu::nk_uint32 NK_CTZ32(nkentseu::nk_uint32 x) {
        if (x == 0) {
            return 32;
        }
        #if defined(NKENTSEU_COMPILER_MSVC)
            unsigned long index;
            _BitScanForward(&index, x);
            return static_cast<nkentseu::nk_uint32>(index);
        #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
            return static_cast<nkentseu::nk_uint32>(__builtin_ctz(x));
        #else
            nkentseu::nk_uint32 count = 0;
            while ((x & 1u) == 0u) {
                ++count;
                x >>= 1;
            }
            return count;
        #endif
    }

    /**
     * @brief Compte les zéros consécutifs depuis le bit de poids faible (64-bit)
     * @param x Valeur 64-bit à analyser
     * @return Nombre de zéros trailing (0-63), ou 64 si x == 0
     * @ingroup BitMacros
     *
     * @note
     *   Sur MSVC 32-bit sans _BitScanForward64, fallback vers traitement
     *   séparé des parties basse et haute du 64-bit.
     */
    NKENTSEU_FORCE_INLINE
    static nkentseu::nk_uint32 NK_CTZ64(nkentseu::nk_uint64 x) {
        if (x == 0) {
            return 64;
        }
        #if defined(NKENTSEU_COMPILER_MSVC)
            #if defined(NKENTSEU_ARCH_X86_64) || defined(NKENTSEU_ARCH_ARM64)
                unsigned long index;
                _BitScanForward64(&index, x);
                return static_cast<nkentseu::nk_uint32>(index);
            #else
                // Fallback pour MSVC 32-bit : traiter les deux halves
                unsigned long index;
                if (_BitScanForward(&index, static_cast<nkentseu::nk_uint32>(x & 0xFFFFFFFFu))) {
                    return static_cast<nkentseu::nk_uint32>(index);
                }
                if (_BitScanForward(&index, static_cast<nkentseu::nk_uint32>(x >> 32))) {
                    return static_cast<nkentseu::nk_uint32>(index + 32);
                }
                return 64;
            #endif
        #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
            return static_cast<nkentseu::nk_uint32>(__builtin_ctzll(x));
        #else
            nkentseu::nk_uint32 count = 0;
            while ((x & 1u) == 0u) {
                ++count;
                x >>= 1;
            }
            return count;
        #endif
    }

    // ------------------------------------------------------------------
    // COUNT LEADING ZEROS (Compter les zéros à gauche / MSB)
    // ------------------------------------------------------------------

    /**
     * @brief Compte les zéros consécutifs depuis le bit de poids fort
     * @param x Valeur 32-bit à analyser
     * @return Nombre de zéros leading (0-31), ou 32 si x == 0
     * @ingroup BitMacros
     *
     * @note
     *   Utile pour :
     *   - Normalisation de nombres (trouver le bit de poids fort)
     *   - Calcul de log2 approximatif
     *   - Compression de données bit-level
     *
     * @warning
     *   Retourne 32 si x == 0 : vérifier avant utilisation comme shift.
     *   Utiliser FindLastSet() pour interface plus safe (-1 si aucun bit).
     */
    NKENTSEU_FORCE_INLINE
    static nkentseu::nk_uint32 NK_CLZ32(nkentseu::nk_uint32 x) {
        if (x == 0) {
            return 32;
        }
        #if defined(NKENTSEU_COMPILER_MSVC)
            unsigned long index;
            _BitScanReverse(&index, x);
            return static_cast<nkentseu::nk_uint32>(31 - index);
        #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
            return static_cast<nkentseu::nk_uint32>(__builtin_clz(x));
        #else
            nkentseu::nk_uint32 count = 0;
            nkentseu::nk_uint32 mask = 0x80000000u;
            while ((x & mask) == 0u) {
                ++count;
                mask >>= 1;
            }
            return count;
        #endif
    }

    /**
     * @brief Compte les zéros consécutifs depuis le bit de poids fort (64-bit)
     * @param x Valeur 64-bit à analyser
     * @return Nombre de zéros leading (0-63), ou 64 si x == 0
     * @ingroup BitMacros
     */
    NKENTSEU_FORCE_INLINE
    static nkentseu::nk_uint32 NK_CLZ64(nkentseu::nk_uint64 x) {
        if (x == 0) {
            return 64;
        }
        #if defined(NKENTSEU_COMPILER_MSVC)
            #if defined(NKENTSEU_ARCH_X86_64) || defined(NKENTSEU_ARCH_ARM64)
                unsigned long index;
                _BitScanReverse64(&index, x);
                return static_cast<nkentseu::nk_uint32>(63 - index);
            #else
                // Fallback pour MSVC 32-bit : traiter les deux halves
                if (x >> 32) {
                    return NK_CLZ32(static_cast<nkentseu::nk_uint32>(x >> 32));
                } else {
                    return NK_CLZ32(static_cast<nkentseu::nk_uint32>(x)) + 32;
                }
            #endif
        #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
            return static_cast<nkentseu::nk_uint32>(__builtin_clzll(x));
        #else
            nkentseu::nk_uint32 count = 0;
            nkentseu::nk_uint64 mask = static_cast<nkentseu::nk_uint64>(1) << 63;
            while ((x & mask) == 0u) {
                ++count;
                mask >>= 1;
            }
            return count;
        #endif
    }

    /** @} */ // End of BitMacros

    // -------------------------------------------------------------------------
    // SECTION 3 : MACROS DE CHANGEMENT D'ENDIANNESS
    // -------------------------------------------------------------------------
    // Inversion de l'ordre des octets pour conversion réseau/hôte.

    /**
     * @defgroup ByteSwapMacros Macros de Changement d'Endianness
     * @brief Inversion de l'ordre des octets pour conversions big/little-endian
     * @ingroup BitUtilities
     *
     * Ces macros permettent de convertir entre l'ordre des octets de l'hôte
     * et l'ordre réseau (big-endian), essentiel pour :
     *   - Protocoles réseau (TCP/IP, HTTP, etc.)
     *   - Formats de fichier binaires portables
     *   - Communication inter-plateformes hétérogènes
     *
     * @note
     *   - Sur plateformes little-endian (x86, x64, ARM typique) : swap effectif
     *   - Sur plateformes big-endian (certains PowerPC, SPARC) : no-op potentiel
     *   - Toujours utiliser ces macros plutôt que des shifts manuels
     *
     * @example
     * @code
     * // Envoi d'un entier 32-bit sur le réseau (big-endian)
     * nkentseu::nk_uint32 hostValue = 0x12345678;
     * nkentseu::nk_uint32 netValue = NK_BYTESWAP32(hostValue);
     * send(socket, &netValue, sizeof(netValue), 0);
     *
     * // Réception et conversion inverse
     * nkentseu::nk_uint32 received;
     * recv(socket, &received, sizeof(received), 0);
     * nkentseu::nk_uint32 hostResult = NK_BYTESWAP32(received);
     * // hostResult == 0x12345678
     * @endcode
     */
    /** @{ */

    /**
     * @brief Inverse l'ordre des deux octets d'une valeur 16-bit
     * @def NK_BYTESWAP16(x)
     * @param x Valeur 16-bit à convertir
     * @return Valeur avec octets inversés
     * @ingroup ByteSwapMacros
     */
    #if defined(NKENTSEU_COMPILER_MSVC)
        #define NK_BYTESWAP16(x) _byteswap_ushort(static_cast<nkentseu::nk_uint16>(x))
        #define NK_BYTESWAP32(x) _byteswap_ulong(static_cast<nkentseu::nk_uint32>(x))
        #define NK_BYTESWAP64(x) _byteswap_uint64(static_cast<nkentseu::nk_uint64>(x))
    #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
        #define NK_BYTESWAP16(x) __builtin_bswap16(static_cast<nkentseu::nk_uint16>(x))
        #define NK_BYTESWAP32(x) __builtin_bswap32(static_cast<nkentseu::nk_uint32>(x))
        #define NK_BYTESWAP64(x) __builtin_bswap64(static_cast<nkentseu::nk_uint64>(x))
    #else
        // Implémentations manuelles fallback (portables mais moins optimales)
        NKENTSEU_FORCE_INLINE
        static nkentseu::nk_uint16 NK_BYTESWAP16(nkentseu::nk_uint16 x) {
            return static_cast<nkentseu::nk_uint16>((x << 8) | (x >> 8));
        }

        NKENTSEU_FORCE_INLINE
        static nkentseu::nk_uint32 NK_BYTESWAP32(nkentseu::nk_uint32 x) {
            return ((x << 24) | ((x << 8) & 0x00FF0000u) | ((x >> 8) & 0x0000FF00u) | (x >> 24));
        }

        NKENTSEU_FORCE_INLINE
        static nkentseu::nk_uint64 NK_BYTESWAP64(nkentseu::nk_uint64 x) {
            x = ((x << 32) | (x >> 32));
            x = (((x & 0xFFFF0000FFFF0000ULL) >> 16) | ((x & 0x0000FFFF0000FFFFULL) << 16));
            return (((x & 0xFF00FF00FF00FF00ULL) >> 8) | ((x & 0x00FF00FF00FF00FFULL) << 8));
        }
    #endif

    /** @} */ // End of ByteSwapMacros

    // -------------------------------------------------------------------------
    // SECTION 4 : ESPACE DE NOMS PRINCIPAL
    // -------------------------------------------------------------------------
    // Déclaration de l'espace de noms principal nkentseu.

    namespace nkentseu {

        // ====================================================================
        // SECTION 5 : CLASSE NK_BITS - UTILITAIRES BITS AVANCÉS
        // ====================================================================
        // Interface unifiée pour opérations bitwise avec fallback portable.

        /**
         * @defgroup BitManipulationClass Classe NkBits - Manipulation de Bits
         * @brief Collection de méthodes statiques pour opérations bitwise avancées
         * @ingroup BitUtilities
         *
         * NkBits fournit une interface type-safe et portable pour les opérations
         * de manipulation de bits les plus courantes, avec :
         *   - Templates génériques pour support de multiples tailles d'entiers
         *   - Sélection automatique des intrinsics optimisés quand disponibles
         *   - Fallback software garanti pour compatibilité maximale
         *   - Assertions debug pour validation des paramètres critiques
         *
         * @note
         *   - Toutes les méthodes sont statiques : pas d'instanciation requise
         *   - NKENTSEU_FORCE_INLINE : suggère l'inlining pour performance
         *   - NKENTSEU_CORE_API : export pour compatibilité DLL/shared libraries
         *
         * @example
         * @code
         * // Compter les bits actifs dans un mask
         * nkentseu::nk_uint32 flags = 0b10110100;
         * int active = nkentseu::NkBits::CountBits(flags);  // active == 3
         *
         * // Rotation de bits pour chiffrement simple
         * nkentseu::nk_uint32 value = 0x12345678;
         * nkentseu::nk_uint32 rotated = nkentseu::NkBits::RotateLeft(value, 8);
         * // rotated == 0x34567812
         *
         * // Extraction de champ de bits (ex: format de paquet protocolaire)
         * nkentseu::nk_uint32 packet = 0xABCD1234;
         * nkentseu::nk_uint32 header = nkentseu::NkBits::ExtractBits(packet, 28, 4);
         * // header == 0xA (bits 28-31)
         * @endcode
         */
        /** @{ */

        /**
         * @brief Classe utilitaire pour manipulation avancée de bits
         * @class NkBits
         * @ingroup BitManipulationClass
         *
         * Cette classe regroupe des méthodes statiques pour opérations bitwise
         * courantes et avancées, avec support template pour différents types
         * d'entiers et fallback portable garanti.
         *
         * @note
         *   - Design stateless : aucune donnée membre, uniquement méthodes statiques
         *   - Thread-safe : pas d'état partagé, toutes les méthodes sont pures
         *   - Zero-overhead : inline + intrinsics quand disponibles
         */
        class NKENTSEU_CORE_API NkBits {
            public:
                // --------------------------------------------------------
                // BIT OPERATIONS - Comptage et recherche de bits
                // --------------------------------------------------------

                /**
                 * @brief Compte le nombre de bits à 1 (population count / popcount)
                 * @tparam T Type entier (nk_uint32, nk_uint64, etc.)
                 * @param value Valeur à analyser
                 * @return Nombre de bits positionnés à 1
                 * @note
                 *   - Sélection automatique : NK_POPCOUNT32/64 ou fallback software
                 *   - O(1) avec intrinsics hardware, O(n) avec fallback software
                 *
                 * @example
                 * @code
                 * // Compter les permissions actives dans un bitmask
                 * nkentseu::nk_uint64 perms = 0x00000000000000F3;  // bits 0,1,4,5,6,7
                 * int count = nkentseu::NkBits::CountBits(perms);  // count == 6
                 * @endcode
                 */
                template <typename T>
                NKENTSEU_FORCE_INLINE
                static nk_int32 CountBits(T value) {
                    if (sizeof(T) == 8) {
                        return NK_POPCOUNT64(value);
                    }
                    if (sizeof(T) == 4) {
                        return NK_POPCOUNT32(value);
                    }
                    // Fallback pour autres tailles (nk_uint8, nk_uint16, etc.)
                    return CountBitsSoftware(value);
                }

                /**
                 * @brief Compte les zéros consécutifs depuis le bit de poids faible
                 * @tparam T Type entier
                 * @param value Valeur à analyser
                 * @return Nombre de zéros trailing (0 à sizeof(T)*8), ou sizeof(T)*8 si value==0
                 * @note
                 *   - Utile pour trouver le premier bit actif depuis la droite
                 *   - Retourne la taille en bits si value == 0 : vérifier avant usage
                 *
                 * @warning
                 *   Ne pas utiliser le résultat comme index sans vérifier value != 0.
                 *   Préférer FindFirstSet() pour interface plus safe.
                 */
                template <typename T>
                NKENTSEU_FORCE_INLINE
                static nk_int32 CountTrailingZeros(T value) {
                    if (value == 0) {
                        return sizeof(T) * 8;
                    }
                    if (sizeof(T) == 8) {
                        return NK_CTZ64(value);
                    }
                    if (sizeof(T) == 4) {
                        return NK_CTZ32(value);
                    }
                    return CountTrailingZerosSoftware(value);
                }

                /**
                 * @brief Compte les zéros consécutifs depuis le bit de poids fort
                 * @tparam T Type entier
                 * @param value Valeur à analyser
                 * @return Nombre de zéros leading (0 à sizeof(T)*8), ou sizeof(T)*8 si value==0
                 * @note
                 *   - Utile pour normalisation, calcul de log2, compression
                 *   - Retourne la taille en bits si value == 0 : vérifier avant usage
                 */
                template <typename T>
                NKENTSEU_FORCE_INLINE
                static nk_int32 CountLeadingZeros(T value) {
                    if (value == 0) {
                        return sizeof(T) * 8;
                    }
                    if (sizeof(T) == 8) {
                        return NK_CLZ64(value);
                    }
                    if (sizeof(T) == 4) {
                        return NK_CLZ32(value);
                    }
                    return CountLeadingZerosSoftware(value);
                }

                /**
                 * @brief Trouve l'index du premier bit à 1 depuis la droite
                 * @tparam T Type entier
                 * @param value Valeur à analyser
                 * @return Index 0-based du premier bit à 1, ou -1 si value == 0
                 * @note
                 *   - Interface safe : retourne -1 au lieu de taille pour value==0
                 *   - Équivalent à CountTrailingZeros() avec gestion explicite du cas zéro
                 *
                 * @example
                 * @code
                 * // Allocation du premier slot libre dans un bitmask
                 * nkentseu::nk_uint32 freeSlots = 0b00001100;  // slots 2 et 3 libres
                 * int slot = nkentseu::NkBits::FindFirstSet(freeSlots);  // slot == 2
                 * if (slot >= 0) {
                 *     AllocateSlot(slot);
                 * }
                 * @endcode
                 */
                template <typename T>
                NKENTSEU_FORCE_INLINE
                static nk_int32 FindFirstSet(T value) {
                    if (value == 0) {
                        return -1;
                    }
                    return CountTrailingZeros(value);
                }

                /**
                 * @brief Trouve l'index du dernier bit à 1 depuis la gauche
                 * @tparam T Type entier
                 * @param value Valeur à analyser
                 * @return Index 0-based du dernier bit à 1, ou -1 si value == 0
                 * @note
                 *   - Utile pour trouver la position du bit de poids fort actif
                 *   - Équivalent à (sizeof(T)*8 - 1 - CountLeadingZeros(value))
                 */
                template <typename T>
                NKENTSEU_FORCE_INLINE
                static nk_int32 FindLastSet(T value) {
                    if (value == 0) {
                        return -1;
                    }
                    return static_cast<nk_int32>(sizeof(T) * 8 - 1 - CountLeadingZeros(value));
                }

                // --------------------------------------------------------
                // ROTATION DE BITS
                // --------------------------------------------------------

                /**
                 * @brief Rotation circulaire gauche (rotate left / rol)
                 * @tparam T Type entier
                 * @param value Valeur à faire tourner
                 * @param shift Nombre de positions de rotation (automatiquement modulo bits)
                 * @return Valeur après rotation gauche
                 * @note
                 *   - Rotation circulaire : les bits sortants réentrent par l'autre bout
                 *   - shift est automatiquement modulo sizeof(T)*8 pour éviter UB
                 *
                 * @example
                 * @code
                 * // Chiffrement simple par rotation
                 * nkentseu::nk_uint32 key = 0x12345678;
                 * nkentseu::nk_uint32 encrypted = nkentseu::NkBits::RotateLeft(key, 13);
                 * // decrypted = RotateRight(encrypted, 13) pour retrouver key
                 * @endcode
                 */
                template <typename T>
                NKENTSEU_FORCE_INLINE
                static T RotateLeft(T value, nk_int32 shift) {
                    const nk_int32 bits = static_cast<nk_int32>(sizeof(T) * 8);
                    shift &= (bits - 1);  // Modulo bits pour éviter shift >= width
                    return static_cast<T>((value << shift) | (value >> (bits - shift)));
                }

                /**
                 * @brief Rotation circulaire droite (rotate right / ror)
                 * @tparam T Type entier
                 * @param value Valeur à faire tourner
                 * @param shift Nombre de positions de rotation (automatiquement modulo bits)
                 * @return Valeur après rotation droite
                 * @note Inverse de RotateLeft : RotateRight(RotateLeft(x, n), n) == x
                 */
                template <typename T>
                NKENTSEU_FORCE_INLINE
                static T RotateRight(T value, nk_int32 shift) {
                    const nk_int32 bits = static_cast<nk_int32>(sizeof(T) * 8);
                    shift &= (bits - 1);  // Modulo bits pour éviter shift >= width
                    return static_cast<T>((value >> shift) | (value << (bits - shift)));
                }

                // --------------------------------------------------------
                // CHANGEMENT D'ENDIANNESS
                // --------------------------------------------------------

                /**
                 * @brief Inverse l'ordre des octets d'une valeur 16-bit
                 * @param value Valeur 16-bit à convertir
                 * @return Valeur avec octets inversés (big↔little endian)
                 * @note Wrapper type-safe vers la macro NK_BYTESWAP16
                 */
                NKENTSEU_FORCE_INLINE
                static nkentseu::nk_uint16 ByteSwap16(nkentseu::nk_uint16 value) {
                    return NK_BYTESWAP16(value);
                }

                /**
                 * @brief Inverse l'ordre des octets d'une valeur 32-bit
                 * @param value Valeur 32-bit à convertir
                 * @return Valeur avec octets inversés
                 * @note Essentiel pour conversion réseau/hôte des entiers 32-bit
                 */
                NKENTSEU_FORCE_INLINE
                static nkentseu::nk_uint32 ByteSwap32(nkentseu::nk_uint32 value) {
                    return NK_BYTESWAP32(value);
                }

                /**
                 * @brief Inverse l'ordre des octets d'une valeur 64-bit
                 * @param value Valeur 64-bit à convertir
                 * @return Valeur avec octets inversés
                 * @note Essentiel pour conversion réseau/hôte des entiers 64-bit
                 */
                NKENTSEU_FORCE_INLINE
                static nkentseu::nk_uint64 ByteSwap64(nkentseu::nk_uint64 value) {
                    return NK_BYTESWAP64(value);
                }

                // --------------------------------------------------------
                // PUISSANCE DE DEUX
                // --------------------------------------------------------

                /**
                 * @brief Vérifie si une valeur est une puissance de deux exacte
                 * @tparam T Type entier
                 * @param value Valeur à vérifier
                 * @return true si value > 0 et value & (value-1) == 0, false sinon
                 * @note
                 *   - Test classique : une puissance de 2 a exactement un bit à 1
                 *   - value doit être > 0 : 0 n'est pas une puissance de 2
                 *
                 * @example
                 * @code
                 * // Validation de taille d'allocation (doit être puissance de 2)
                 * nkentseu::nk_size size = GetUserRequestedSize();
                 * if (!nkentseu::NkBits::IsPowerOfTwo(size)) {
                 *     size = nkentseu::NkBits::NextPowerOfTwo(size);  // Arrondir
                 * }
                 * @endcode
                 */
                template <typename T>
                NKENTSEU_FORCE_INLINE
                static nk_bool IsPowerOfTwo(T value) {
                    return value > 0 && (value & (value - 1)) == 0;
                }

                /**
                 * @brief Arrondit vers le haut à la prochaine puissance de 2 (32-bit)
                 * @param value Valeur 32-bit à arrondir
                 * @return Plus petite puissance de 2 >= value
                 * @note
                 *   - Implémentation dans NkBits.cpp pour éviter duplication template
                 *   - Retourne 1 si value == 0 (convention utile pour allocations)
                 *
                 * @example
                 * @code
                 * // Arrondir une taille de buffer à puissance de 2 pour alignement
                 * nkentseu::nk_uint32 requested = 1000;
                 * nkentseu::nk_uint32 aligned = nkentseu::NkBits::NextPowerOfTwo(requested);
                 * // aligned == 1024
                 * @endcode
                 */
                static nkentseu::nk_uint32 NextPowerOfTwo(nkentseu::nk_uint32 value);

                /**
                 * @brief Arrondit vers le haut à la prochaine puissance de 2 (64-bit)
                 * @param value Valeur 64-bit à arrondir
                 * @return Plus petite puissance de 2 >= value
                 * @note Même comportement que la version 32-bit, adapté aux 64-bit
                 */
                static nkentseu::nk_uint64 NextPowerOfTwo(nkentseu::nk_uint64 value);

                /**
                 * @brief Calcule le logarithme base 2 d'une puissance de 2
                 * @tparam T Type entier
                 * @param value Valeur (doit être une puissance de 2 exacte)
                 * @return Exposant n tel que 2^n == value
                 * @pre value doit être une puissance de 2 (vérifié par assertion en debug)
                 * @note
                 *   - Équivalent à FindLastSet(value) pour les puissances de 2
                 *   - En release : pas de vérification, comportement indéfini si pre-condition violée
                 *
                 * @warning
                 *   En debug, NKENTSEU_ASSERT_MSG vérifie IsPowerOfTwo(value).
                 *   En release, passer une valeur non-puissance-de-2 donne un résultat incorrect.
                 */
                template <typename T>
                NKENTSEU_FORCE_INLINE
                static nk_int32 Log2(T value) {
                    NKENTSEU_ASSERT_MSG(IsPowerOfTwo(value), "Value must be power of two");
                    return FindLastSet(value);
                }

                // --------------------------------------------------------
                // MANIPULATION DE CHAMPS DE BITS
                // --------------------------------------------------------

                /**
                 * @brief Extrait un champ de bits contiguous d'une valeur
                 * @tparam T Type entier
                 * @param value Valeur source contenant le champ
                 * @param position Position du bit de poids faible du champ (0-based, LSB=0)
                 * @param count Nombre de bits à extraire
                 * @return Champ extrait, aligné à droite (bits de poids fort à 0)
                 * @note
                 *   - Masque automatiquement les bits hors du champ demandé
                 *   - Assertion en debug pour validation des bornes
                 *
                 * @example
                 * @code
                 * // Extraire le champ "opcode" (bits 26-31) d'une instruction MIPS
                 * nkentseu::nk_uint32 instruction = 0x8C020010;  // lw $v0, 16($0)
                 * nkentseu::nk_uint32 opcode = nkentseu::NkBits::ExtractBits(instruction, 26, 6);
                 * // opcode == 0b100011 (35 décimal) = opcode pour lw
                 * @endcode
                 */
                template <typename T>
                NKENTSEU_FORCE_INLINE
                static T ExtractBits(T value, nk_int32 position, nk_int32 count) {
                    NKENTSEU_ASSERT_MSG(
                        position >= 0 && count > 0 && position + count <= static_cast<nk_int32>(sizeof(T) * 8),
                        "Invalid bit range"
                    );
                    T mask = (static_cast<T>(1) << count) - 1;
                    return (value >> position) & mask;
                }

                /**
                 * @brief Insère un champ de bits dans une valeur destination
                 * @tparam T Type entier
                 * @param dest Valeur destination où insérer le champ
                 * @param src Valeur source contenant les bits à insérer (seuls les 'count' LSB sont utilisés)
                 * @param position Position du bit de poids faible du champ cible (0-based)
                 * @param count Nombre de bits à insérer
                 * @return Nouvelle valeur avec le champ inséré (autres bits inchangés)
                 * @note
                 *   - Efface d'abord le champ cible dans dest, puis insère les bits de src
                 *   - Les bits de src au-delà de 'count' sont ignorés (masqués)
                 *
                 * @example
                 * @code
                 * // Construire une instruction MIPS : opcode (6 bits) + rs (5 bits) + rt (5 bits) + imm (16 bits)
                 * nkentseu::nk_uint32 instr = 0;
                 * instr = nkentseu::NkBits::InsertBits(instr, 0b100011, 26, 6);  // opcode = lw
                 * instr = nkentseu::NkBits::InsertBits(instr, 0, 21, 5);          // rs = $0
                 * instr = nkentseu::NkBits::InsertBits(instr, 2, 16, 5);          // rt = $v0
                 * instr = nkentseu::NkBits::InsertBits(instr, 16, 0, 16);         // immediate = 16
                 * // instr == 0x8C020010
                 * @endcode
                 */
                template <typename T>
                NKENTSEU_FORCE_INLINE
                static T InsertBits(T dest, T src, nk_int32 position, nk_int32 count) {
                    NKENTSEU_ASSERT_MSG(
                        position >= 0 && count > 0 && position + count <= static_cast<nk_int32>(sizeof(T) * 8),
                        "Invalid bit range"
                    );
                    T mask = (static_cast<T>(1) << count) - 1;
                    T cleared = dest & ~(mask << position);
                    return cleared | ((src & mask) << position);
                }

                /**
                 * @brief Inverse l'ordre de tous les bits d'une valeur (bit-reversal)
                 * @tparam T Type entier
                 * @param value Valeur dont les bits doivent être inversés
                 * @return Valeur avec ordre des bits complètement inversé
                 * @note
                 *   - Différent de ByteSwap : inverse les bits, pas les octets
                 *   - Utile pour algorithmes FFT, codage de Huffman, cryptographie
                 *   - Implémentation software : O(n) avec n = nombre de bits
                 *
                 * @example
                 * @code
                 * // Bit-reversal pour permutation dans FFT radix-2
                 * nkentseu::nk_uint8 index = 0b00000110;  // 6 décimal
                 * nkentseu::nk_uint8 reversed = nkentseu::NkBits::ReverseBits(index);
                 * // reversed == 0b01100000 (96 décimal) = bit-reversal de 6 sur 8 bits
                 * @endcode
                 */
                template <typename T>
                NKENTSEU_FORCE_INLINE
                static T ReverseBits(T value) {
                    return ReverseBitsSoftware(value);
                }

                /**
                 * @brief Inverse l'ordre des octets d'une valeur (byte-reversal)
                 * @tparam T Type entier
                 * @param value Valeur dont les octets doivent être inversés
                 * @return Valeur avec ordre des octets inversé
                 * @note
                 *   - Différent de ReverseBits : inverse les octets, pas les bits individuels
                 *   - Équivalent à ByteSwap16/32/64 selon sizeof(T)
                 *   - Utile pour conversions endianness génériques
                 */
                template <typename T>
                NKENTSEU_FORCE_INLINE
                static T ReverseBytes(T value) {
                    if (sizeof(T) == 8) {
                        return ByteSwap64(value);
                    }
                    if (sizeof(T) == 4) {
                        return ByteSwap32(value);
                    }
                    if (sizeof(T) == 2) {
                        return ByteSwap16(value);
                    }
                    return value;  // sizeof(T) == 1 : rien à inverser
                }

            private:
                // --------------------------------------------------------
                // IMPLÉMENTATIONS SOFTWARE (FALLBACK PORTABLE)
                // --------------------------------------------------------
                // Ces méthodes sont utilisées quand les intrinsics compiler ne sont pas disponibles.

                /**
                 * @brief Implémentation software pour CountBits (population count)
                 * @tparam T Type entier
                 * @param value Valeur à analyser
                 * @return Nombre de bits à 1
                 * @internal
                 * @note O(n) avec n = nombre de bits : utiliser intrinsics quand possible
                 */
                template <typename T>
                static nk_int32 CountBitsSoftware(T value) {
                    nk_int32 count = 0;
                    while (value) {
                        count += static_cast<nk_int32>(value & 1);
                        value >>= 1;
                    }
                    return count;
                }

                /**
                 * @brief Implémentation software pour CountTrailingZeros
                 * @tparam T Type entier
                 * @param value Valeur à analyser
                 * @return Nombre de zéros trailing
                 * @internal
                 */
                template <typename T>
                static nk_int32 CountTrailingZerosSoftware(T value) {
                    if (value == 0) {
                        return sizeof(T) * 8;
                    }
                    nk_int32 count = 0;
                    while ((value & 1) == 0) {
                        ++count;
                        value >>= 1;
                    }
                    return count;
                }

                /**
                 * @brief Implémentation software pour CountLeadingZeros
                 * @tparam T Type entier
                 * @param value Valeur à analyser
                 * @return Nombre de zéros leading
                 * @internal
                 */
                template <typename T>
                static nk_int32 CountLeadingZerosSoftware(T value) {
                    if (value == 0) {
                        return sizeof(T) * 8;
                    }
                    nk_int32 count = 0;
                    T mask = static_cast<T>(1) << (sizeof(T) * 8 - 1);
                    while ((value & mask) == 0) {
                        ++count;
                        mask >>= 1;
                    }
                    return count;
                }

                /**
                 * @brief Implémentation software pour ReverseBits
                 * @tparam T Type entier
                 * @param value Valeur dont inverser les bits
                 * @return Valeur avec bits inversés
                 * @internal
                 * @note O(n) avec n = nombre de bits : algorithme naïf mais portable
                 */
                template <typename T>
                static T ReverseBitsSoftware(T value) {
                    T result = 0;
                    const nk_int32 bits = sizeof(T) * 8;
                    for (nk_int32 i = 0; i < bits; ++i) {
                        result = static_cast<T>((result << 1) | (value & 1));
                        value >>= 1;
                    }
                    return result;
                }
        };

        /** @} */ // End of BitManipulationClass

    } // namespace nkentseu

#endif // NKENTSEU_CORE_NKBITS_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION DE NKBITS.H
// =============================================================================
// Ce fichier fournit des utilitaires optimisés pour manipulation de bits.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Comptage de bits pour gestion de ressources
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkBits.h"

    // Gestionnaire de slots avec bitmask
    class SlotAllocator {
    public:
        static constexpr int MAX_SLOTS = 32;

        SlotAllocator() : m_freeSlots(0xFFFFFFFFu) {}  // Tous libres initialement

        // Allouer le premier slot libre
        int Allocate() {
            int slot = nkentseu::NkBits::FindFirstSet(m_freeSlots);
            if (slot < 0) {
                return -1;  // Aucun slot libre
            }
            // Marquer comme utilisé (clear le bit)
            m_freeSlots &= ~(1u << slot);
            return slot;
        }

        // Libérer un slot
        void Free(int slot) {
            NKENTSEU_ASSERT_MSG(slot >= 0 && slot < MAX_SLOTS, "Invalid slot index");
            m_freeSlots |= (1u << slot);  // Set le bit = libre
        }

        // Compter les slots libres
        int CountFree() const {
            return nkentseu::NkBits::CountBits(m_freeSlots);
        }

        // Vérifier si un slot est libre
        bool IsFree(int slot) const {
            return (m_freeSlots & (1u << slot)) != 0;
        }

    private:
        nkentseu::nk_uint32 m_freeSlots;  // Bit i = 1 → slot i libre
    };

    // Usage
    void Example1() {
        SlotAllocator allocator;

        int s1 = allocator.Allocate();  // s1 == 0
        int s2 = allocator.Allocate();  // s2 == 1

        printf("Free slots: %d/%d\n",
            allocator.CountFree(),
            SlotAllocator::MAX_SLOTS);  // "Free slots: 30/32"

        allocator.Free(s1);
        printf("Slot 0 is %s\n",
            allocator.IsFree(0) ? "free" : "used");  // "free"
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Conversion endianness pour protocoles réseau
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkBits.h"
    #include <cstring>  // Pour memcpy

    // En-tête de paquet réseau (format big-endian)
    #pragma pack(push, 1)
    struct NetworkHeader {
        nkentseu::nk_uint16 magic;      // 0xABCD en big-endian
        nkentseu::nk_uint16 version;    // Version du protocole
        nkentseu::nk_uint32 payloadSize;// Taille des données suivantes
        nkentseu::nk_uint32 checksum;   // CRC32 du payload
    };
    #pragma pack(pop)

    // Sérialisation : hôte → réseau (big-endian)
    void SerializeHeader(const NetworkHeader& host, void* buffer) {
        NetworkHeader net;
        net.magic = nkentseu::NkBits::ByteSwap16(host.magic);
        net.version = nkentseu::NkBits::ByteSwap16(host.version);
        net.payloadSize = nkentseu::NkBits::ByteSwap32(host.payloadSize);
        net.checksum = nkentseu::NkBits::ByteSwap32(host.checksum);

        std::memcpy(buffer, &net, sizeof(NetworkHeader));
    }

    // Désérialisation : réseau (big-endian) → hôte
    bool DeserializeHeader(const void* buffer, NetworkHeader& host) {
        NetworkHeader net;
        std::memcpy(&net, buffer, sizeof(NetworkHeader));

        // Validation du magic number après conversion
        host.magic = nkentseu::NkBits::ByteSwap16(net.magic);
        if (host.magic != 0xABCD) {
            return false;  // Magic incorrect : paquet corrompu ou mauvais protocole
        }

        host.version = nkentseu::NkBits::ByteSwap16(net.version);
        host.payloadSize = nkentseu::NkBits::ByteSwap32(net.payloadSize);
        host.checksum = nkentseu::NkBits::ByteSwap32(net.checksum);

        return true;
    }

    // Usage
    void Example2() {
        NetworkHeader hostHeader;
        hostHeader.magic = 0xABCD;
        hostHeader.version = 0x0002;
        hostHeader.payloadSize = 1024;
        hostHeader.checksum = 0xDEADBEEF;

        // Sérialiser pour envoi réseau
        uint8_t buffer[sizeof(NetworkHeader)];
        SerializeHeader(hostHeader, buffer);

        // ... envoyer buffer sur le réseau ...

        // Désérialiser à la réception
        NetworkHeader received;
        if (DeserializeHeader(buffer, received)) {
            printf("Received v%d, payload %u bytes\n",
                received.version,
                received.payloadSize);
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Rotation de bits pour algorithmes cryptographiques simples
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkBits.h"

    // Chiffrement/déchiffrement XOR + rotation (toy example, pas pour production)
    class SimpleCipher {
    public:
        explicit SimpleCipher(nkentseu::nk_uint32 key) : m_key(key) {}

        // Chiffrer un bloc 32-bit
        nkentseu::nk_uint32 Encrypt(nkentseu::nk_uint32 plaintext) {
            nkentseu::nk_uint32 state = plaintext ^ m_key;
            state = nkentseu::NkBits::RotateLeft(state, 7);
            state ^= nkentseu::NkBits::RotateLeft(m_key, 3);
            return state;
        }

        // Déchiffrer (opération inverse)
        nkentseu::nk_uint32 Decrypt(nkentseu::nk_uint32 ciphertext) {
            nkentseu::nk_uint32 state = ciphertext;
            state ^= nkentseu::NkBits::RotateLeft(m_key, 3);
            state = nkentseu::NkBits::RotateRight(state, 7);  // Inverse de RotateLeft
            return state ^ m_key;
        }

        // Vérifier l'intégrité via checksum bit-count based
        static nkentseu::nk_uint8 ComputeChecksum(nkentseu::nk_uint32 data) {
            // Checksum simple : popcount modulo 256
            return static_cast<nkentseu::nk_uint8>(
                nkentseu::NkBits::CountBits(data) & 0xFF
            );
        }

    private:
        nkentseu::nk_uint32 m_key;
    };

    // Usage
    void Example3() {
        SimpleCipher cipher(0x12345678);

        nkentseu::nk_uint32 original = 0xDEADBEEF;
        nkentseu::nk_uint32 encrypted = cipher.Encrypt(original);
        nkentseu::nk_uint32 decrypted = cipher.Decrypt(encrypted);

        printf("Original:  0x%08X\n", original);
        printf("Encrypted: 0x%08X\n", encrypted);
        printf("Decrypted: 0x%08X\n", decrypted);
        // decrypted == original si la clé est correcte

        // Vérification d'intégrité
        nkentseu::nk_uint8 checksum = SimpleCipher::ComputeChecksum(original);
        printf("Checksum: 0x%02X (%d bits set)\n",
            checksum,
            nkentseu::NkBits::CountBits(original));
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Extraction/insertion de champs pour parsing binaire
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkBits.h"

    // Format de paquet personnalisé : [flags:4][type:4][length:16][payload:*]
    struct CustomPacket {
        nkentseu::nk_uint8 flags;    // Bits 0-3: version, 4-7: options
        nkentseu::nk_uint8 type;     // Type de payload
        nkentseu::nk_uint16 length;  // Taille du payload
        // ... payload suit ...

        // Extraire la version (bits 0-3 de flags)
        nkentseu::nk_uint8 GetVersion() const {
            return nkentseu::NkBits::ExtractBits(flags, 0, 4);
        }

        // Définir la version (bits 0-3 de flags)
        void SetVersion(nkentseu::nk_uint8 version) {
            NKENTSEU_ASSERT_MSG(version < 16, "Version must fit in 4 bits");
            flags = nkentseu::NkBits::InsertBits(flags, version, 0, 4);
        }

        // Extraire les options (bits 4-7 de flags)
        nkentseu::nk_uint8 GetOptions() const {
            return nkentseu::NkBits::ExtractBits(flags, 4, 4);
        }

        // Définir les options
        void SetOptions(nkentseu::nk_uint8 options) {
            NKENTSEU_ASSERT_MSG(options < 16, "Options must fit in 4 bits");
            flags = nkentseu::NkBits::InsertBits(flags, options, 4, 4);
        }
    };

    // Usage
    void Example4() {
        CustomPacket pkt;
        pkt.flags = 0;  // Init à zéro

        // Construire un paquet : version=3, options=0b1010, type=7, length=256
        pkt.SetVersion(3);
        pkt.SetOptions(0b1010);
        pkt.type = 7;
        pkt.length = 256;

        printf("Flags byte: 0x%02X (binary: ", pkt.flags);
        for (int i = 7; i >= 0; --i) {
            printf("%d", (pkt.flags >> i) & 1);
        }
        printf(")\n");
        // Output: Flags byte: 0xA3 (binary: 10100011)
        //         ^^^^ options=1010, ^^^^ version=0011

        // Parser un paquet reçu
        nkentseu::nk_uint8 receivedFlags = 0xE5;  // 11100101
        nkentseu::nk_uint8 version = nkentseu::NkBits::ExtractBits(receivedFlags, 0, 4);  // 0101 = 5
        nkentseu::nk_uint8 options = nkentseu::NkBits::ExtractBits(receivedFlags, 4, 4);  // 1110 = 14

        printf("Parsed: version=%d, options=0b%04b\n", version, options);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Alignement mémoire via NextPowerOfTwo
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkBits.h"
    #include <cstdlib>  // Pour malloc/free

    // Allocateur avec alignement puissance de 2
    class AlignedAllocator {
    public:
        // Allouer size bytes alignés sur alignement (doit être puissance de 2)
        static void* Allocate(nkentseu::nk_size size, nkentseu::nk_size alignment) {
            NKENTSEU_ASSERT_MSG(
                nkentseu::NkBits::IsPowerOfTwo(alignment),
                "Alignment must be power of two"
            );

            // Arrondir la taille à la prochaine puissance de 2 si demandée
            nkentseu::nk_size alignedSize = nkentseu::NkBits::NextPowerOfTwo(
                static_cast<nkentseu::nk_uint32>(size)
            );

            // Allocation avec sur-allocation pour alignement
            void* raw = std::malloc(alignedSize + alignment);
            if (!raw) {
                return nullptr;
            }

            // Calcul de l'adresse alignée
            uintptr_t addr = reinterpret_cast<uintptr_t>(raw);
            uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);

            // Stocker le pointeur original juste avant l'adresse alignée pour free()
            reinterpret_cast<void**>(aligned)[-1] = raw;

            return reinterpret_cast<void*>(aligned);
        }

        // Libérer une mémoire allouée par Allocate()
        static void Free(void* ptr) {
            if (!ptr) {
                return;
            }
            // Récupérer le pointeur original stocké avant l'adresse alignée
            void* raw = reinterpret_cast<void**>(ptr)[-1];
            std::free(raw);
        }
    };

    // Usage
    void Example5() {
        // Allouer 1000 bytes alignés sur 64 bytes
        void* buffer = AlignedAllocator::Allocate(1000, 64);
        if (buffer) {
            printf("Allocated at %p (aligned to 64 bytes)\n", buffer);

            // Vérifier l'alignement
            uintptr_t addr = reinterpret_cast<uintptr_t>(buffer);
            printf("Address mod 64: %zu (should be 0)\n", addr % 64);

            // ... utiliser buffer ...

            AlignedAllocator::Free(buffer);
        }
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================