// =============================================================================
// NKPlatform/NkEndianness.h
// Détection d'endianness et conversion d'ordre des octets.
//
// Design :
//  - Détection compile-time et runtime de l'endianness du système
//  - Fonctions de byte-swap optimisées (intrinsèques compilateur ou fallback)
//  - Conversions host/network byte order (big-endian réseau)
//  - Conversions little-endian / big-endian génériques via templates
//  - Accès mémoire non-aligné sécurisé via memcpy
//  - Fonctions combinées lecture/écriture + conversion d'endianness
//  - Compatible C++11+ avec constexpr pour évaluation à la compilation
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_PLATFORM_NKENDIANNESS_H
#define NKENTSEU_PLATFORM_NKENDIANNESS_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules de détection de plateforme/compilateur et des macros d'export.
    // NkPlatformDetect.h fournit les macros NKENTSEU_PLATFORM_* pour la détection OS.
    // NkCompilerDetect.h fournit les macros NKENTSEU_COMPILER_* pour la détection compilateur.
    // NkPlatformExport.h fournit NKENTSEU_PLATFORM_API pour l'export de symboles.
    // NkPlatformInline.h fournit NKENTSEU_FORCE_INLINE pour l'inlining forcé portable.

    #include "NKPlatform/NkPlatformDetect.h"
    #include "NKPlatform/NkCompilerDetect.h"
    #include "NKPlatform/NkPlatformExport.h"
    #include "NKPlatform/NkPlatformInline.h"

    #include <stdint.h>
    #include <string.h>

    // -------------------------------------------------------------------------
    // SECTION 2 : DÉFINITION DE NKENTSEU_FORCE_INLINE (FALLBACK)
    // -------------------------------------------------------------------------
    // Cette macro est normalement fournie par NkPlatformInline.h.
    // Cette définition de fallback assure la compatibilité si le fichier est inclus seul.

    #ifndef NKENTSEU_FORCE_INLINE
        #if defined(NKENTSEU_COMPILER_MSVC)
            #define NKENTSEU_FORCE_INLINE __forceinline
        #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
            #define NKENTSEU_FORCE_INLINE __attribute__((always_inline)) inline
        #else
            #define NKENTSEU_FORCE_INLINE inline
        #endif
    #endif

    // =========================================================================
    // SECTION 3 : ESPACE DE NOMS PRINCIPAL (INDENTATION HIÉRARCHIQUE)
    // =========================================================================
    // Déclaration de l'espace de noms principal nkentseu::platform.
    // Les symboles publics sont exportés via NKENTSEU_PLATFORM_API si nécessaire.
    //
    // NOTE SUR L'INDENTATION :
    //  - Le contenu de 'nkentseu' est indenté d'un niveau
    //  - Le contenu de 'platform' (dans 'nkentseu') est indenté de deux niveaux

    namespace nkentseu {

        namespace platform {

            // =================================================================
            // SECTION 4 : ÉNUMÉRATION ET DÉTECTION D'ENDIANNESS
            // =================================================================
            // Types et fonctions pour détecter l'ordre des octets du système.

            /**
             * @brief Énumération des types d'endianness supportés
             * @enum NkEndianness
             * @ingroup EndiannessAPI
             *
             * Représente l'ordre des octets utilisé par la plateforme cible.
             * Utilisé pour les conversions conditionnelles à la compilation.
             */
            enum class NKENTSEU_CLASS_EXPORT NkEndianness : uint8_t {
                /** @brief Little-endian : octet de poids faible en premier (x86, ARM moderne) */
                NK_LITTLE = 0,

                /** @brief Big-endian : octet de poids fort en premier (réseau, certains CPU embarqués) */
                NK_BIG = 1,

                /** @brief Endianness inconnue ou non détectée */
                NK_UNKNOWN = 2
            };

            /**
             * @brief Détection d'endianness à la compilation (API NK)
             * @return NkEndianness détectée via les macros du préprocesseur
             * @ingroup EndiannessAPI
             *
             * Cette fonction constexpr permet d'évaluer l'endianness à la compilation,
             * permettant au compilateur d'optimiser les branches conditionnelles.
             *
             * @note Sur les plateformes courantes (Windows, Linux x86/ARM), retourne NK_LITTLE.
             *
             * @example
             * @code
             * if constexpr (NkGetCompileTimeEndianness() == NkEndianness::NK_LITTLE) {
             *     // Code optimisé pour little-endian
             * }
             * @endcode
             */
            NKENTSEU_FORCE_INLINE constexpr NkEndianness NkGetCompileTimeEndianness() {
                #if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
                    return NkEndianness::NK_LITTLE;
                #elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
                    return NkEndianness::NK_BIG;
                #elif defined(_WIN32) || defined(__i386__) || defined(__x86_64__) || defined(__arm__) || defined(__aarch64__)
                    return NkEndianness::NK_LITTLE;
                #elif defined(__BIG_ENDIAN__) || defined(_BIG_ENDIAN)
                    return NkEndianness::NK_BIG;
                #else
                    return NkEndianness::NK_UNKNOWN;
                #endif
            }

            /**
             * @brief Détection d'endianness à l'exécution (API NK)
             * @return NkEndianness détectée via test mémoire runtime
             * @ingroup EndiannessAPI
             *
             * Utilise un test mémoire avec union pour déterminer l'ordre des octets.
             * Utile lorsque la détection compile-time échoue ou pour validation.
             *
             * @note Cette fonction est constexpr en C++20+ grâce au support des unions constexpr.
             */
            NKENTSEU_FORCE_INLINE NkEndianness NkGetRuntimeEndianness() {
                union {
                    uint32_t value;
                    uint8_t bytes[4];
                } test = {0x01020304};

                if (test.bytes[0] == 0x04) {
                    return NkEndianness::NK_LITTLE;
                } else if (test.bytes[0] == 0x01) {
                    return NkEndianness::NK_BIG;
                } else {
                    return NkEndianness::NK_UNKNOWN;
                }
            }

            /**
             * @brief Vérifier si le système est little-endian
             * @return true si little-endian, false sinon
             * @ingroup EndiannessChecks
             *
             * Fonction constexpr évaluée à la compilation pour optimisation.
             */
            NKENTSEU_FORCE_INLINE constexpr bool NkIsLittleEndian() {
                return NkGetCompileTimeEndianness() == NkEndianness::NK_LITTLE;
            }

            /**
             * @brief Vérifier si le système est big-endian
             * @return true si big-endian, false sinon
             * @ingroup EndiannessChecks
             *
             * Fonction constexpr évaluée à la compilation pour optimisation.
             */
            NKENTSEU_FORCE_INLINE constexpr bool NkIsBigEndian() {
                return NkGetCompileTimeEndianness() == NkEndianness::NK_BIG;
            }

            // -----------------------------------------------------------------
            // Sous-section : API Legacy (dépréciée, opt-in uniquement)
            // -----------------------------------------------------------------
            // Wrappers pour compatibilité avec l'ancienne API utilisant le préfixe sans "Nk".
            // Ces fonctions sont fournies uniquement si NKENTSEU_ENABLE_LEGACY_PLATFORM_API est défini.

            #if defined(NKENTSEU_ENABLE_LEGACY_PLATFORM_API)
                /**
                 * @deprecated Utiliser NkEndianness à la place
                 */
                enum class Endianness {
                    Little,
                    Big,
                    Unknown
                };

                /**
                 * @brief Convertir Endianness legacy vers NkEndianness
                 * @deprecated Utiliser directement NkEndianness
                 */
                NKENTSEU_FORCE_INLINE constexpr NkEndianness NkToEndianness(Endianness value) {
                    switch (value) {
                        case Endianness::Little:
                            return NkEndianness::NK_LITTLE;
                        case Endianness::Big:
                            return NkEndianness::NK_BIG;
                        default:
                            return NkEndianness::NK_UNKNOWN;
                    }
                }

                /**
                 * @brief Convertir NkEndianness vers Endianness legacy
                 * @deprecated Utiliser directement NkEndianness
                 */
                NKENTSEU_FORCE_INLINE constexpr Endianness NkToLegacyEndianness(NkEndianness value) {
                    switch (value) {
                        case NkEndianness::NK_LITTLE:
                            return Endianness::Little;
                        case NkEndianness::NK_BIG:
                            return Endianness::Big;
                        default:
                            return Endianness::Unknown;
                    }
                }

                /**
                 * @deprecated Utiliser NkGetCompileTimeEndianness() à la place
                 */
                [[deprecated("Use NkGetCompileTimeEndianness()")]]
                NKENTSEU_FORCE_INLINE constexpr Endianness GetCompileTimeEndianness() {
                    return NkToLegacyEndianness(NkGetCompileTimeEndianness());
                }

                /**
                 * @deprecated Utiliser NkGetRuntimeEndianness() à la place
                 */
                [[deprecated("Use NkGetRuntimeEndianness()")]]
                NKENTSEU_FORCE_INLINE constexpr Endianness GetRuntimeEndianness() {
                    return NkToLegacyEndianness(NkGetRuntimeEndianness());
                }

                /**
                 * @deprecated Utiliser NkIsLittleEndian() à la place
                 */
                [[deprecated("Use NkIsLittleEndian()")]]
                NKENTSEU_FORCE_INLINE constexpr bool IsLittleEndian() {
                    return NkIsLittleEndian();
                }

                /**
                 * @deprecated Utiliser NkIsBigEndian() à la place
                 */
                [[deprecated("Use NkIsBigEndian()")]]
                NKENTSEU_FORCE_INLINE constexpr bool IsBigEndian() {
                    return NkIsBigEndian();
                }
            #endif

        } // namespace platform

    } // namespace nkentseu

    // =========================================================================
    // SECTION 5 : MACROS COMPILE-TIME D'ENDIANNESS
    // =========================================================================
    // Macros préprocesseur pour les conditions de compilation basées sur l'endianness.
    // Ces macros sont évaluées à la compilation et n'ont aucun coût runtime.

    /**
     * @brief Macro indiquant si la plateforme est little-endian
     * @def NK_LITTLE_ENDIAN
     * @value 1 si little-endian, 0 sinon
     * @ingroup EndiannessMacros
     */

    /**
     * @brief Macro indiquant si la plateforme est big-endian
     * @def NK_BIG_ENDIAN
     * @value 1 si big-endian, 0 sinon
     * @ingroup EndiannessMacros
     */

    #if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        #define NK_LITTLE_ENDIAN 1
        #define NK_BIG_ENDIAN 0
    #elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        #define NK_LITTLE_ENDIAN 0
        #define NK_BIG_ENDIAN 1
    #elif defined(_WIN32) || defined(__i386__) || defined(__x86_64__) || defined(__arm__) || defined(__aarch64__)
        #define NK_LITTLE_ENDIAN 1
        #define NK_BIG_ENDIAN 0
    #else
        #define NK_LITTLE_ENDIAN 0
        #define NK_BIG_ENDIAN 0
        /**
         * @brief Macro indiquant une endianness inconnue
         * @def NK_ENDIAN_UNKNOWN
         * @value 1 si endianness non détectée
         * @ingroup EndiannessMacros
         */
        #define NK_ENDIAN_UNKNOWN 1
    #endif

    // =========================================================================
    // SECTION 6 : ESPACE DE NOMS POUR LES FONCTIONS DE CONVERSION
    // =========================================================================
    // Déclaration des fonctions de byte-swap et conversion d'ordre des octets.
    // Indentées d'un niveau supplémentaire car dans namespace nkentseu::platform.

    namespace nkentseu {

        namespace platform {

            // =================================================================
            // SECTION 7 : FONCTIONS DE BYTE-SWAP (INVERSION D'OCTETS)
            // =================================================================
            // Fonctions optimisées pour inverser l'ordre des octets d'une valeur.
            // Utilisent les intrinsèques du compilateur quand disponibles.

            /**
             * @brief Inverser les octets d'un entier 16 bits
             * @param value Valeur uint16_t à convertir
             * @return Valeur avec octets inversés
             * @ingroup ByteSwapFunctions
             *
             * Utilise _byteswap_ushort (MSVC) ou __builtin_bswap16 (GCC/Clang)
             * quand disponibles, sinon fallback portable en C++ pur.
             *
             * @note Fonction constexpr : peut être évaluée à la compilation si l'argument est constant.
             */
            NKENTSEU_FORCE_INLINE constexpr uint16_t ByteSwap16(uint16_t value) {
                #if defined(NKENTSEU_COMPILER_MSVC)
                    return _byteswap_ushort(value);
                #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
                    return __builtin_bswap16(value);
                #else
                    return (value >> 8) | (value << 8);
                #endif
            }

            /**
             * @brief Alias NK pour ByteSwap16
             * @param value Valeur uint16_t à convertir
             * @return Valeur avec octets inversés
             * @ingroup ByteSwapFunctions
             */
            NKENTSEU_FORCE_INLINE constexpr uint16_t NkByteSwap16(uint16_t value) {
                return ByteSwap16(value);
            }

            /**
             * @brief Inverser les octets d'un entier 32 bits
             * @param value Valeur uint32_t à convertir
             * @return Valeur avec octets inversés
             * @ingroup ByteSwapFunctions
             *
             * Utilise _byteswap_ulong (MSVC) ou __builtin_bswap32 (GCC/Clang)
             * quand disponibles, sinon fallback portable en C++ pur.
             */
            NKENTSEU_FORCE_INLINE constexpr uint32_t ByteSwap32(uint32_t value) {
                #if defined(NKENTSEU_COMPILER_MSVC)
                    return _byteswap_ulong(value);
                #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
                    return __builtin_bswap32(value);
                #else
                    return ((value >> 24) & 0x000000FF) |
                           ((value >> 8)  & 0x0000FF00) |
                           ((value << 8)  & 0x00FF0000) |
                           ((value << 24) & 0xFF000000);
                #endif
            }

            /**
             * @brief Alias NK pour ByteSwap32
             * @param value Valeur uint32_t à convertir
             * @return Valeur avec octets inversés
             * @ingroup ByteSwapFunctions
             */
            NKENTSEU_FORCE_INLINE constexpr uint32_t NkByteSwap32(uint32_t value) {
                return ByteSwap32(value);
            }

            /**
             * @brief Inverser les octets d'un entier 64 bits
             * @param value Valeur uint64_t à convertir
             * @return Valeur avec octets inversés
             * @ingroup ByteSwapFunctions
             *
             * Utilise _byteswap_uint64 (MSVC) ou __builtin_bswap64 (GCC/Clang)
             * quand disponibles, sinon fallback portable en C++ pur.
             */
            NKENTSEU_FORCE_INLINE constexpr uint64_t ByteSwap64(uint64_t value) {
                #if defined(NKENTSEU_COMPILER_MSVC)
                    return _byteswap_uint64(value);
                #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
                    return __builtin_bswap64(value);
                #else
                    return ((value >> 56) & 0x00000000000000FFULL) |
                           ((value >> 40) & 0x000000000000FF00ULL) |
                           ((value >> 24) & 0x0000000000FF0000ULL) |
                           ((value >> 8)  & 0x00000000FF000000ULL) |
                           ((value << 8)  & 0x000000FF00000000ULL) |
                           ((value << 24) & 0x0000FF0000000000ULL) |
                           ((value << 40) & 0x00FF000000000000ULL) |
                           ((value << 56) & 0xFF00000000000000ULL);
                #endif
            }

            /**
             * @brief Alias NK pour ByteSwap64
             * @param value Valeur uint64_t à convertir
             * @return Valeur avec octets inversés
             * @ingroup ByteSwapFunctions
             */
            NKENTSEU_FORCE_INLINE constexpr uint64_t NkByteSwap64(uint64_t value) {
                return ByteSwap64(value);
            }

            /**
             * @brief Byte-swap générique via template
             * @tparam T Type de la valeur (doit faire 2, 4 ou 8 octets)
             * @param value Valeur à convertir
             * @return Valeur avec octets inversés
             * @ingroup ByteSwapFunctions
             *
             * Utilise if constexpr (C++17) pour sélectionner l'implémentation
             * appropriée selon la taille du type.
             *
             * @note static_assert garantit que seuls les types 2/4/8 octets sont acceptés.
             *
             * @example
             * @code
             * uint32_t swapped = ByteSwap(myValue);
             * float floatSwapped = ByteSwap(myFloat); // via union interne
             * @endcode
             */
            template <typename T>
            NKENTSEU_FORCE_INLINE constexpr T ByteSwap(T value) {
                static_assert(
                    sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8,
                    "ByteSwap only supports 2, 4, or 8 byte types"
                );

                if constexpr (sizeof(T) == 2) {
                    union {
                        T val;
                        uint16_t u16;
                    } u;
                    u.val = value;
                    u.u16 = ByteSwap16(u.u16);
                    return u.val;
                } else if constexpr (sizeof(T) == 4) {
                    union {
                        T val;
                        uint32_t u32;
                    } u;
                    u.val = value;
                    u.u32 = ByteSwap32(u.u32);
                    return u.val;
                } else if constexpr (sizeof(T) == 8) {
                    union {
                        T val;
                        uint64_t u64;
                    } u;
                    u.val = value;
                    u.u64 = ByteSwap64(u.u64);
                    return u.val;
                }
            }

            // =================================================================
            // SECTION 8 : CONVERSION NETWORK BYTE ORDER (BIG-ENDIAN)
            // =================================================================
            // Fonctions pour convertir entre l'ordre host et l'ordre réseau (big-endian).
            // Équivalent portable des fonctions POSIX htonl/htons/ntohl/ntohs.

            /**
             * @brief Convertir uint16 de host vers network byte order (big-endian)
             * @param value Valeur en ordre host
             * @return Valeur en ordre réseau (big-endian)
             * @ingroup NetworkByteOrder
             *
             * Sur les systèmes little-endian, inverse les octets.
             * Sur les systèmes big-endian, retourne la valeur inchangée.
             */
            NKENTSEU_FORCE_INLINE constexpr uint16_t HostToNetwork16(uint16_t value) {
                #if NK_LITTLE_ENDIAN
                    return ByteSwap16(value);
                #else
                    return value;
                #endif
            }

            /**
             * @brief Alias NK pour HostToNetwork16
             * @param value Valeur en ordre host
             * @return Valeur en ordre réseau (big-endian)
             * @ingroup NetworkByteOrder
             */
            NKENTSEU_FORCE_INLINE constexpr uint16_t NkHostToNetwork16(uint16_t value) {
                return HostToNetwork16(value);
            }

            /**
             * @brief Convertir uint32 de host vers network byte order (big-endian)
             * @param value Valeur en ordre host
             * @return Valeur en ordre réseau (big-endian)
             * @ingroup NetworkByteOrder
             */
            NKENTSEU_FORCE_INLINE constexpr uint32_t HostToNetwork32(uint32_t value) {
                #if NK_LITTLE_ENDIAN
                    return ByteSwap32(value);
                #else
                    return value;
                #endif
            }

            /**
             * @brief Alias NK pour HostToNetwork32
             * @param value Valeur en ordre host
             * @return Valeur en ordre réseau (big-endian)
             * @ingroup NetworkByteOrder
             */
            NKENTSEU_FORCE_INLINE constexpr uint32_t NkHostToNetwork32(uint32_t value) {
                return HostToNetwork32(value);
            }

            /**
             * @brief Convertir uint64 de host vers network byte order (big-endian)
             * @param value Valeur en ordre host
             * @return Valeur en ordre réseau (big-endian)
             * @ingroup NetworkByteOrder
             */
            NKENTSEU_FORCE_INLINE constexpr uint64_t HostToNetwork64(uint64_t value) {
                #if NK_LITTLE_ENDIAN
                    return ByteSwap64(value);
                #else
                    return value;
                #endif
            }

            /**
             * @brief Alias NK pour HostToNetwork64
             * @param value Valeur en ordre host
             * @return Valeur en ordre réseau (big-endian)
             * @ingroup NetworkByteOrder
             */
            NKENTSEU_FORCE_INLINE constexpr uint64_t NkHostToNetwork64(uint64_t value) {
                return HostToNetwork64(value);
            }

            /**
             * @brief Convertir uint16 de network vers host byte order
             * @param value Valeur en ordre réseau (big-endian)
             * @return Valeur en ordre host
             * @ingroup NetworkByteOrder
             *
             * @note Opération symétrique : network-to-host = host-to-network
             */
            NKENTSEU_FORCE_INLINE constexpr uint16_t NetworkToHost16(uint16_t value) {
                return HostToNetwork16(value);
            }

            /**
             * @brief Alias NK pour NetworkToHost16
             * @param value Valeur en ordre réseau (big-endian)
             * @return Valeur en ordre host
             * @ingroup NetworkByteOrder
             */
            NKENTSEU_FORCE_INLINE constexpr uint16_t NkNetworkToHost16(uint16_t value) {
                return NetworkToHost16(value);
            }

            /**
             * @brief Convertir uint32 de network vers host byte order
             * @param value Valeur en ordre réseau (big-endian)
             * @return Valeur en ordre host
             * @ingroup NetworkByteOrder
             */
            NKENTSEU_FORCE_INLINE constexpr uint32_t NetworkToHost32(uint32_t value) {
                return HostToNetwork32(value);
            }

            /**
             * @brief Alias NK pour NetworkToHost32
             * @param value Valeur en ordre réseau (big-endian)
             * @return Valeur en ordre host
             * @ingroup NetworkByteOrder
             */
            NKENTSEU_FORCE_INLINE constexpr uint32_t NkNetworkToHost32(uint32_t value) {
                return NetworkToHost32(value);
            }

            /**
             * @brief Convertir uint64 de network vers host byte order
             * @param value Valeur en ordre réseau (big-endian)
             * @return Valeur en ordre host
             * @ingroup NetworkByteOrder
             */
            NKENTSEU_FORCE_INLINE constexpr uint64_t NetworkToHost64(uint64_t value) {
                return HostToNetwork64(value);
            }

            /**
             * @brief Alias NK pour NetworkToHost64
             * @param value Valeur en ordre réseau (big-endian)
             * @return Valeur en ordre host
             * @ingroup NetworkByteOrder
             */
            NKENTSEU_FORCE_INLINE constexpr uint64_t NkNetworkToHost64(uint64_t value) {
                return NetworkToHost64(value);
            }

            // =================================================================
            // SECTION 9 : CONVERSIONS LITTLE-ENDIAN GÉNÉRIQUES
            // =================================================================
            // Templates pour convertir vers/depuis le little-endian pour tout type supporté.

            /**
             * @brief Convertir une valeur vers little-endian
             * @tparam T Type de la valeur (2, 4 ou 8 octets)
             * @param value Valeur en ordre host
             * @return Valeur en ordre little-endian
             * @ingroup EndianConversion
             *
             * Sur les systèmes little-endian, retourne la valeur inchangée.
             * Sur les systèmes big-endian, inverse les octets.
             */
            template <typename T>
            NKENTSEU_FORCE_INLINE constexpr T ToLittleEndian(T value) {
                #if NK_LITTLE_ENDIAN
                    return value;
                #else
                    return ByteSwap(value);
                #endif
            }

            /**
             * @brief Convertir une valeur depuis little-endian
             * @tparam T Type de la valeur (2, 4 ou 8 octets)
             * @param value Valeur en ordre little-endian
             * @return Valeur en ordre host
             * @ingroup EndianConversion
             *
             * @note Opération symétrique : from-little = to-little
             */
            template <typename T>
            NKENTSEU_FORCE_INLINE constexpr T FromLittleEndian(T value) {
                return ToLittleEndian(value);
            }

            // =================================================================
            // SECTION 10 : CONVERSIONS BIG-ENDIAN GÉNÉRIQUES
            // =================================================================
            // Templates pour convertir vers/depuis le big-endian pour tout type supporté.

            /**
             * @brief Convertir une valeur vers big-endian
             * @tparam T Type de la valeur (2, 4 ou 8 octets)
             * @param value Valeur en ordre host
             * @return Valeur en ordre big-endian
             * @ingroup EndianConversion
             *
             * Sur les systèmes big-endian, retourne la valeur inchangée.
             * Sur les systèmes little-endian, inverse les octets.
             */
            template <typename T>
            NKENTSEU_FORCE_INLINE constexpr T ToBigEndian(T value) {
                #if NK_BIG_ENDIAN
                    return value;
                #else
                    return ByteSwap(value);
                #endif
            }

            /**
             * @brief Convertir une valeur depuis big-endian
             * @tparam T Type de la valeur (2, 4 ou 8 octets)
             * @param value Valeur en ordre big-endian
             * @return Valeur en ordre host
             * @ingroup EndianConversion
             *
             * @note Opération symétrique : from-big = to-big
             */
            template <typename T>
            NKENTSEU_FORCE_INLINE constexpr T FromBigEndian(T value) {
                return ToBigEndian(value);
            }

            // =================================================================
            // SECTION 11 : BYTE-SWAP DE BUFFERS
            // =================================================================
            // Fonctions pour convertir des tableaux entiers d'un ordre d'octets à un autre.

            /**
             * @brief Inverser les octets de chaque élément d'un buffer
             * @tparam T Type des éléments (doit supporter ByteSwap)
             * @param data Pointeur vers le buffer de données
             * @param count Nombre d'éléments dans le buffer (pas en octets)
             * @ingroup BufferConversion
             *
             * @note La fonction itère sur chaque élément et applique ByteSwap.
             * Pour de grands buffers, envisager une version vectorisée/SIMD.
             */
            template <typename T>
            NKENTSEU_API_INLINE void ByteSwapBuffer(T *data, size_t count) {
                for (size_t i = 0; i < count; ++i) {
                    data[i] = ByteSwap(data[i]);
                }
            }

            /**
             * @brief Convertir un buffer vers little-endian
             * @tparam T Type des éléments
             * @param data Pointeur vers le buffer de données
             * @param count Nombre d'éléments dans le buffer
             * @ingroup BufferConversion
             *
             * Sur les systèmes little-endian, cette fonction est un no-op.
             */
            template <typename T>
            NKENTSEU_API_INLINE void BufferToLittleEndian(T *data, size_t count) {
                #if !NK_LITTLE_ENDIAN
                    ByteSwapBuffer(data, count);
                #endif
            }

            /**
             * @brief Convertir un buffer depuis little-endian
             * @tparam T Type des éléments
             * @param data Pointeur vers le buffer de données
             * @param count Nombre d'éléments dans le buffer
             * @ingroup BufferConversion
             *
             * @note Opération symétrique : from-little = to-little
             */
            template <typename T>
            NKENTSEU_API_INLINE void BufferFromLittleEndian(T *data, size_t count) {
                BufferToLittleEndian(data, count);
            }

            /**
             * @brief Convertir un buffer vers big-endian
             * @tparam T Type des éléments
             * @param data Pointeur vers le buffer de données
             * @param count Nombre d'éléments dans le buffer
             * @ingroup BufferConversion
             *
             * Sur les systèmes big-endian, cette fonction est un no-op.
             */
            template <typename T>
            NKENTSEU_API_INLINE void BufferToBigEndian(T *data, size_t count) {
                #if !NK_BIG_ENDIAN
                    ByteSwapBuffer(data, count);
                #endif
            }

            /**
             * @brief Convertir un buffer depuis big-endian
             * @tparam T Type des éléments
             * @param data Pointeur vers le buffer de données
             * @param count Nombre d'éléments dans le buffer
             * @ingroup BufferConversion
             *
             * @note Opération symétrique : from-big = to-big
             */
            template <typename T>
            NKENTSEU_API_INLINE void BufferFromBigEndian(T *data, size_t count) {
                BufferToBigEndian(data, count);
            }

            // =================================================================
            // SECTION 12 : ACCÈS MÉMOIRE NON-ALIGNÉ
            // =================================================================
            // Fonctions pour lire/écrire des valeurs depuis/vers une adresse potentiellement non-alignée.
            // Utilise memcpy pour éviter les undefined behavior sur les architectures strictes (ARM, etc.).

            /**
             * @brief Lire un uint16 depuis une adresse potentiellement non-alignée
             * @param ptr Pointeur vers la source (peut être non-alignée)
             * @return Valeur uint16 lue
             * @ingroup UnalignedAccess
             *
             * @note Utilise memcpy pour garantir la portabilité sur toutes les architectures.
             * Plus lent qu'un accès direct sur x86, mais safe sur ARM/RISC-V.
             */
            NKENTSEU_API_INLINE uint16_t ReadUnaligned16(const void *ptr) {
                uint16_t value;
                ::memcpy(&value, ptr, sizeof(uint16_t));
                return value;
            }

            /**
             * @brief Lire un uint32 depuis une adresse potentiellement non-alignée
             * @param ptr Pointeur vers la source (peut être non-alignée)
             * @return Valeur uint32 lue
             * @ingroup UnalignedAccess
             */
            NKENTSEU_API_INLINE uint32_t ReadUnaligned32(const void *ptr) {
                uint32_t value;
                ::memcpy(&value, ptr, sizeof(uint32_t));
                return value;
            }

            /**
             * @brief Lire un uint64 depuis une adresse potentiellement non-alignée
             * @param ptr Pointeur vers la source (peut être non-alignée)
             * @return Valeur uint64 lue
             * @ingroup UnalignedAccess
             */
            NKENTSEU_API_INLINE uint64_t ReadUnaligned64(const void *ptr) {
                uint64_t value;
                ::memcpy(&value, ptr, sizeof(uint64_t));
                return value;
            }

            /**
             * @brief Écrire un uint16 vers une adresse potentiellement non-alignée
             * @param ptr Pointeur vers la destination (peut être non-alignée)
             * @param value Valeur uint16 à écrire
             * @ingroup UnalignedAccess
             */
            NKENTSEU_API_INLINE void WriteUnaligned16(void *ptr, uint16_t value) {
                ::memcpy(ptr, &value, sizeof(uint16_t));
            }

            /**
             * @brief Écrire un uint32 vers une adresse potentiellement non-alignée
             * @param ptr Pointeur vers la destination (peut être non-alignée)
             * @param value Valeur uint32 à écrire
             * @ingroup UnalignedAccess
             */
            NKENTSEU_API_INLINE void WriteUnaligned32(void *ptr, uint32_t value) {
                ::memcpy(ptr, &value, sizeof(uint32_t));
            }

            /**
             * @brief Écrire un uint64 vers une adresse potentiellement non-alignée
             * @param ptr Pointeur vers la destination (peut être non-alignée)
             * @param value Valeur uint64 à écrire
             * @ingroup UnalignedAccess
             */
            NKENTSEU_API_INLINE void WriteUnaligned64(void *ptr, uint64_t value) {
                ::memcpy(ptr, &value, sizeof(uint64_t));
            }

            // =================================================================
            // SECTION 13 : ACCÈS NON-ALIGNÉ + CONVERSION D'ENDIANNESS
            // =================================================================
            // Fonctions combinées pour lire/écrire des valeurs avec conversion d'endianness
            // depuis/vers une adresse potentiellement non-alignée. Très utile pour parser
            // des formats binaires réseau ou des fichiers avec en-têtes non-alignés.

            /**
             * @brief Lire un uint16 little-endian depuis mémoire non-alignée
             * @param ptr Pointeur vers la source (peut être non-alignée)
             * @return Valeur convertie en ordre host
             * @ingroup CombinedAccess
             */
            NKENTSEU_API_INLINE uint16_t ReadLE16(const void *ptr) {
                return FromLittleEndian(ReadUnaligned16(ptr));
            }

            /**
             * @brief Lire un uint32 little-endian depuis mémoire non-alignée
             * @param ptr Pointeur vers la source (peut être non-alignée)
             * @return Valeur convertie en ordre host
             * @ingroup CombinedAccess
             */
            NKENTSEU_API_INLINE uint32_t ReadLE32(const void *ptr) {
                return FromLittleEndian(ReadUnaligned32(ptr));
            }

            /**
             * @brief Lire un uint64 little-endian depuis mémoire non-alignée
             * @param ptr Pointeur vers la source (peut être non-alignée)
             * @return Valeur convertie en ordre host
             * @ingroup CombinedAccess
             */
            NKENTSEU_API_INLINE uint64_t ReadLE64(const void *ptr) {
                return FromLittleEndian(ReadUnaligned64(ptr));
            }

            /**
             * @brief Lire un uint16 big-endian depuis mémoire non-alignée
             * @param ptr Pointeur vers la source (peut être non-alignée)
             * @return Valeur convertie en ordre host
             * @ingroup CombinedAccess
             */
            NKENTSEU_API_INLINE uint16_t ReadBE16(const void *ptr) {
                return FromBigEndian(ReadUnaligned16(ptr));
            }

            /**
             * @brief Lire un uint32 big-endian depuis mémoire non-alignée
             * @param ptr Pointeur vers la source (peut être non-alignée)
             * @return Valeur convertie en ordre host
             * @ingroup CombinedAccess
             */
            NKENTSEU_API_INLINE uint32_t ReadBE32(const void *ptr) {
                return FromBigEndian(ReadUnaligned32(ptr));
            }

            /**
             * @brief Lire un uint64 big-endian depuis mémoire non-alignée
             * @param ptr Pointeur vers la source (peut être non-alignée)
             * @return Valeur convertie en ordre host
             * @ingroup CombinedAccess
             */
            NKENTSEU_API_INLINE uint64_t ReadBE64(const void *ptr) {
                return FromBigEndian(ReadUnaligned64(ptr));
            }

            /**
             * @brief Écrire un uint16 little-endian vers mémoire non-alignée
             * @param ptr Pointeur vers la destination (peut être non-alignée)
             * @param value Valeur en ordre host à convertir et écrire
             * @ingroup CombinedAccess
             */
            NKENTSEU_API_INLINE void WriteLE16(void *ptr, uint16_t value) {
                WriteUnaligned16(ptr, ToLittleEndian(value));
            }

            /**
             * @brief Écrire un uint32 little-endian vers mémoire non-alignée
             * @param ptr Pointeur vers la destination (peut être non-alignée)
             * @param value Valeur en ordre host à convertir et écrire
             * @ingroup CombinedAccess
             */
            NKENTSEU_API_INLINE void WriteLE32(void *ptr, uint32_t value) {
                WriteUnaligned32(ptr, ToLittleEndian(value));
            }

            /**
             * @brief Écrire un uint64 little-endian vers mémoire non-alignée
             * @param ptr Pointeur vers la destination (peut être non-alignée)
             * @param value Valeur en ordre host à convertir et écrire
             * @ingroup CombinedAccess
             */
            NKENTSEU_API_INLINE void WriteLE64(void *ptr, uint64_t value) {
                WriteUnaligned64(ptr, ToLittleEndian(value));
            }

            /**
             * @brief Écrire un uint16 big-endian vers mémoire non-alignée
             * @param ptr Pointeur vers la destination (peut être non-alignée)
             * @param value Valeur en ordre host à convertir et écrire
             * @ingroup CombinedAccess
             */
            NKENTSEU_API_INLINE void WriteBE16(void *ptr, uint16_t value) {
                WriteUnaligned16(ptr, ToBigEndian(value));
            }

            /**
             * @brief Écrire un uint32 big-endian vers mémoire non-alignée
             * @param ptr Pointeur vers la destination (peut être non-alignée)
             * @param value Valeur en ordre host à convertir et écrire
             * @ingroup CombinedAccess
             */
            NKENTSEU_API_INLINE void WriteBE32(void *ptr, uint32_t value) {
                WriteUnaligned32(ptr, ToBigEndian(value));
            }

            /**
             * @brief Écrire un uint64 big-endian vers mémoire non-alignée
             * @param ptr Pointeur vers la destination (peut être non-alignée)
             * @param value Valeur en ordre host à convertir et écrire
             * @ingroup CombinedAccess
             */
            NKENTSEU_API_INLINE void WriteBE64(void *ptr, uint64_t value) {
                WriteUnaligned64(ptr, ToBigEndian(value));
            }

        } // namespace platform

    } // namespace nkentseu

    // =========================================================================
    // SECTION 14 : MACROS DE CONVENANCE (OPTIONNELLES)
    // =========================================================================
    // Alias courts pour les opérations courantes. Ces macros sont optionnelles
    // et peuvent être évitées au profit des fonctions nommées pour plus de clarté.

    /**
     * @brief Alias court pour ByteSwap16
     * @def NK_BSWAP16
     * @param x Valeur uint16_t à convertir
     * @ingroup ConvenienceMacros
     */
    #define NK_BSWAP16(x) nkentseu::platform::ByteSwap16(x)

    /**
     * @brief Alias court pour ByteSwap32
     * @def NK_BSWAP32
     * @param x Valeur uint32_t à convertir
     * @ingroup ConvenienceMacros
     */
    #define NK_BSWAP32(x) nkentseu::platform::ByteSwap32(x)

    /**
     * @brief Alias court pour ByteSwap64
     * @def NK_BSWAP64
     * @param x Valeur uint64_t à convertir
     * @ingroup ConvenienceMacros
     */
    #define NK_BSWAP64(x) nkentseu::platform::ByteSwap64(x)

    /**
     * @brief Alias court pour HostToNetwork16 (équivalent htons)
     * @def NK_HTON16
     * @param x Valeur uint16_t à convertir
     * @ingroup ConvenienceMacros
     */
    #define NK_HTON16(x) nkentseu::platform::HostToNetwork16(x)

    /**
     * @brief Alias court pour HostToNetwork32 (équivalent htonl)
     * @def NK_HTON32
     * @param x Valeur uint32_t à convertir
     * @ingroup ConvenienceMacros
     */
    #define NK_HTON32(x) nkentseu::platform::HostToNetwork32(x)

    /**
     * @brief Alias court pour HostToNetwork64
     * @def NK_HTON64
     * @param x Valeur uint64_t à convertir
     * @ingroup ConvenienceMacros
     */
    #define NK_HTON64(x) nkentseu::platform::HostToNetwork64(x)

    /**
     * @brief Alias court pour NetworkToHost16 (équivalent ntohs)
     * @def NK_NTOH16
     * @param x Valeur uint16_t à convertir
     * @ingroup ConvenienceMacros
     */
    #define NK_NTOH16(x) nkentseu::platform::NetworkToHost16(x)

    /**
     * @brief Alias court pour NetworkToHost32 (équivalent ntohl)
     * @def NK_NTOH32
     * @param x Valeur uint32_t à convertir
     * @ingroup ConvenienceMacros
     */
    #define NK_NTOH32(x) nkentseu::platform::NetworkToHost32(x)

    /**
     * @brief Alias court pour NetworkToHost64
     * @def NK_NTOH64
     * @param x Valeur uint64_t à convertir
     * @ingroup ConvenienceMacros
     */
    #define NK_NTOH64(x) nkentseu::platform::NetworkToHost64(x)

#endif // NKENTSEU_PLATFORM_NKENDIANNESS_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKENDIANNESS.H
// =============================================================================
// Ce fichier fournit des utilitaires pour la détection d'endianness et la conversion
// d'ordre des octets, essentiels pour le parsing de formats binaires et la communication réseau.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Détection d'endianness et logging
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkEndianness.h"
    #include "NKPlatform/NkFoundationLog.h"

    void LogEndiannessInfo() {
        constexpr auto compileEndianness = nkentseu::platform::NkGetCompileTimeEndianness();
        const auto runtimeEndianness = nkentseu::platform::NkGetRuntimeEndianness();

        NK_FOUNDATION_LOG_INFO("Compile-time endianness: %d", static_cast<int>(compileEndianness));
        NK_FOUNDATION_LOG_INFO("Runtime endianness: %d", static_cast<int>(runtimeEndianness));

        if (nkentseu::platform::NkIsLittleEndian()) {
            NK_FOUNDATION_LOG_INFO("System is little-endian (x86/ARM typical)");
        } else if (nkentseu::platform::NkIsBigEndian()) {
            NK_FOUNDATION_LOG_INFO("System is big-endian (network order, some embedded)");
        } else {
            NK_FOUNDATION_LOG_WARN("Endianness unknown - using fallback conversions");
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Conversion réseau (big-endian) pour protocole TCP/IP
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkEndianness.h"
    #include <cstdint>

    struct NetworkHeader {
        uint16_t magic;      // 0xABCD en big-endian
        uint16_t length;     // Longueur du payload en big-endian
        uint32_t timestamp;  // Timestamp en big-endian
    };

    void SendPacket(int socket, const void* payload, size_t payloadSize) {
        NetworkHeader header;
        header.magic = nkentseu::platform::HostToNetwork16(0xABCD);
        header.length = nkentseu::platform::HostToNetwork16(static_cast<uint16_t>(payloadSize));
        header.timestamp = nkentseu::platform::HostToNetwork32(GetCurrentTimestamp());

        // Envoi : header + payload
        send(socket, &header, sizeof(header), 0);
        send(socket, payload, payloadSize, 0);
    }

    NetworkHeader ReceivePacket(int socket) {
        NetworkHeader header;
        recv(socket, &header, sizeof(header), MSG_WAITALL);

        // Conversion retour vers host order
        header.magic = nkentseu::platform::NetworkToHost16(header.magic);
        header.length = nkentseu::platform::NetworkToHost16(header.length);
        header.timestamp = nkentseu::platform::NetworkToHost32(header.timestamp);

        return header;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Parsing de fichier binaire avec valeurs little-endian
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkEndianness.h"
    #include <fstream>

    struct FileHeader {
        uint32_t magic;      // "NKFF" = 0x46464B4E en little-endian
        uint16_t version;
        uint32_t dataSize;
    };

    bool LoadCustomFile(const char* filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            return false;
        }

        // Lecture du header depuis une adresse potentiellement non-alignée
        FileHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));

        // Conversion depuis little-endian (format du fichier)
        const uint32_t magic = nkentseu::platform::ReadLE32(&header.magic);
        const uint16_t version = nkentseu::platform::ReadLE16(&header.version);
        const uint32_t dataSize = nkentseu::platform::ReadLE32(&header.dataSize);

        if (magic != 0x46464B4E) {  // "NKFF"
            return false;  // Mauvais format de fichier
        }

        // Lecture du payload...
        return true;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Conversion générique via templates
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkEndianness.h"

    template <typename T>
    void SerializeValue(std::vector<uint8_t>& buffer, T value, bool littleEndian) {
        if (littleEndian) {
            value = nkentseu::platform::ToLittleEndian(value);
        } else {
            value = nkentseu::platform::ToBigEndian(value);
        }

        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
        buffer.insert(buffer.end(), bytes, bytes + sizeof(T));
    }

    template <typename T>
    T DeserializeValue(const uint8_t* data, bool littleEndian) {
        T value;
        ::memcpy(&value, data, sizeof(T));

        if (littleEndian) {
            return nkentseu::platform::FromLittleEndian(value);
        } else {
            return nkentseu::platform::FromBigEndian(value);
        }
    }

    // Utilisation :
    // std::vector<uint8_t> buffer;
    // SerializeValue(buffer, uint32_t(0x12345678), true);  // little-endian
    // auto val = DeserializeValue<uint32_t>(buffer.data(), true);
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Conversion de buffer entier pour sérialisation
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkEndianness.h"
    #include <vector>

    void PrepareNetworkBuffer(std::vector<uint32_t>& values) {
        // Convertir tout le buffer vers big-endian (ordre réseau)
        nkentseu::platform::BufferToBigEndian(values.data(), values.size());
    }

    void ProcessReceivedBuffer(std::vector<uint32_t>& values) {
        // Convertir depuis big-endian vers host order après réception
        nkentseu::platform::BufferFromBigEndian(values.data(), values.size());
    }

    // Pour les types flottants (via union dans ByteSwap) :
    void SerializeFloatArray(std::vector<uint8_t>& out, const float* values, size_t count) {
        out.reserve(count * sizeof(float));

        for (size_t i = 0; i < count; ++i) {
            // ByteSwap supporte float via union interne (taille 4 octets)
            const uint32_t swapped = nkentseu::platform::ByteSwap(*reinterpret_cast<const uint32_t*>(&values[i]));
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&swapped);
            out.insert(out.end(), bytes, bytes + sizeof(float));
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Accès non-aligné pour parsing de formats binaires
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkEndianness.h"

    // Parser un en-tête de format binaire avec champs non-alignés
    struct PackedHeader {
        uint8_t type;      // offset 0
        uint32_t id;       // offset 1 (non-aligné sur 4!)
        uint16_t flags;    // offset 5 (non-aligné sur 2)
    } NKENTSEU_PACKED;     // Pas de padding entre les membres

    bool ParsePackedHeader(const uint8_t* data, PackedHeader& out) {
        // Lecture safe depuis adresses non-alignées
        out.type = data[0];
        out.id = nkentseu::platform::ReadLE32(data + 1);      // little-endian, non-aligné
        out.flags = nkentseu::platform::ReadLE16(data + 5);   // little-endian, non-aligné

        return true;
    }

    // Alternative : copier dans une structure alignée puis convertir
    bool ParsePackedHeaderSafe(const uint8_t* data, PackedHeader& out) {
        // Copie byte-à-byte pour éviter l'aliasing
        ::memcpy(&out, data, sizeof(PackedHeader));

        // Conversion des champs multi-octets
        out.id = nkentseu::platform::FromLittleEndian(out.id);
        out.flags = nkentseu::platform::FromLittleEndian(out.flags);

        return true;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Combinaison avec d'autres modules NKPlatform
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkPlatformDetect.h"   // NKENTSEU_PLATFORM_*
    #include "NKPlatform/NkArchDetect.h"       // NKENTSEU_ARCH_*
    #include "NKPlatform/NkEndianness.h"       // Conversions d'endianness

    void PlatformAwareSerialization(uint8_t* buffer, size_t offset, uint32_t value) {
        // Adaptation selon la plateforme cible du format binaire

        #if defined(NKENTSEU_PLATFORM_WINDOWS) || defined(NKENTSEU_PLATFORM_LINUX)
            // Ces plateformes sont typiquement little-endian
            // Si le format cible est big-endian (réseau), convertir
            nkentseu::platform::WriteBE32(buffer + offset, value);
        #elif defined(NKENTSEU_PLATFORM_UNKNOWN)
            // Plateforme inconnue : détection runtime + conversion conditionnelle
            if (nkentseu::platform::NkIsLittleEndian()) {
                nkentseu::platform::WriteBE32(buffer + offset, value);
            } else {
                nkentseu::platform::WriteUnaligned32(buffer + offset, value);
            }
        #else
            // Fallback générique : toujours convertir vers big-endian
            nkentseu::platform::WriteBE32(buffer + offset, value);
        #endif
    }

    // Optimisation compile-time : si l'endianness est connue, le compiler
    // éliminera la branche inutile grâce aux constexpr et NK_LITTLE_ENDIAN
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================