// =============================================================================
// NKCore/NkTypes.h
// Définition des types fondamentaux et alias pour le framework NKCore.
//
// Design :
//  - Types entiers fixes portables (int8, uint32, int64, etc.)
//  - Types flottants avec précision explicite (float32, float64, float80)
//  - Types caractères avec support Unicode (char8, char16, char32, wchar)
//  - Wrapper Byte constexpr pour opérations bitwise sécurisées
//  - Support optionnel des entiers 128 bits
//  - Alias hiérarchiques : court (u32), préfixé (nk_uint32), sans préfixe (uint32)
//  - Constantes de limites et valeurs spéciales (NK_NULL, INVALID_ID, etc.)
//  - Macros de conversion d'endianness portables
//  - Types avancés : Hash, Handle, ID pour les systèmes internes
//  - Intégration avec NKPlatform pour détection d'architecture et exports
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CORE_NKTYPES_H
#define NKENTSEU_CORE_NKTYPES_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules de détection d'architecture et des macros d'export.
    // NkArchDetect.h fournit NKENTSEU_ARCH_* pour la détection CPU.
    // NkPlatformExport.h fournit NKENTSEU_PLATFORM_API pour l'export de symboles.
    // NkPlatformInline.h fournit NKENTSEU_FORCE_INLINE pour l'inlining forcé.

    #include "NKPlatform/NkArchDetect.h"
    #include "NKPlatform/NkPlatformExport.h"
    #include "NKPlatform/NkPlatformInline.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : DÉTECTION DE stdint.h ET TYPES DE BASE
    // -------------------------------------------------------------------------
    // Vérification de la disponibilité des types fixes standards.

    /**
     * @brief Indicateur de disponibilité de <stdint.h>
     * @def NKENTSEU_HAS_STDINT
     * @ingroup TypeDetection
     * @value 1 si disponible, 0 sinon
     *
     * Défini si le compilateur supporte C99+ ou fournit les types fixes.
     */
    #if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
        #include <stdint.h>
        #define NKENTSEU_HAS_STDINT 1
    #else
        #define NKENTSEU_HAS_STDINT 0
    #endif

    // -------------------------------------------------------------------------
    // SECTION 3 : ESPACE DE NOMS PRINCIPAL (INDENTATION HIÉRARCHIQUE)
    // -------------------------------------------------------------------------
    // Déclaration de l'espace de noms principal nkentseu.
    // Les types publics sont exportés via NKENTSEU_CLASS_EXPORT si nécessaire.
    //
    // NOTE SUR L'INDENTATION :
    //  - Le contenu de 'nkentseu' est indenté d'un niveau
    //  - Les sous-namespaces (math) sont indentés d'un niveau supplémentaire

    namespace nkentseu {

        // ====================================================================
        // SECTION 4 : MACROS UTILITAIRES DE BASE
        // ====================================================================
        // Macros génériques utilisées dans les définitions de types.

        /**
         * @brief Macro pour créer un masque de bit unique
         * @def BIT
         * @param x Position du bit (0-31 pour uint32)
         * @return Valeur avec seul le bit x activé
         * @ingroup TypeMacros
         *
         * @example
         * @code
         * uint32 flags = BIT(0) | BIT(3);  // 0b00001001 = 9
         * @endcode
         */
        #ifndef BIT
            #define BIT(x) (1u << (x))
        #endif

        // ====================================================================
        // SECTION 5 : TYPES ENTIERS FIXES (PRIMITIVES)
        // ====================================================================
        // Définition de types entiers de taille garantie, indépendants de la plateforme.

        /**
         * @brief Entier signé 8 bits
         * @typedef int8
         * @ingroup PrimitiveTypes
         * @value Plage : -128 à 127
         */
        using int8 = signed char;

        /**
         * @brief Entier non signé 8 bits
         * @typedef uint8
         * @ingroup PrimitiveTypes
         * @value Plage : 0 à 255
         */
        using uint8 = unsigned char;

        /**
         * @brief Entier non signé 32 bits (alias pour unsigned long)
         * @typedef uintl32
         * @ingroup PrimitiveTypes
         * @note Préférer uint32 pour la portabilité
         */
        using uintl32 = unsigned long int;

        // -------------------------------------------------------------------------
        // Sous-section : Définitions dépendantes du compilateur
        // -------------------------------------------------------------------------
        // Adaptation des types 16/32/64 bits selon le compilateur cible.

        #if defined(NKENTSEU_COMPILER_MSVC)
            // MSVC : types spécifiques avec préfixe __
            using int16 = __int16;
            using uint16 = unsigned __int16;
            using int32 = __int32;
            using uint32 = unsigned __int32;
            using int64 = __int64;
            using uint64 = unsigned __int64;
        #else
            // GCC/Clang et compatibles : types standards C++
            using int16 = short;
            using uint16 = unsigned short;
            using int32 = int;
            using uint32 = unsigned int;
            using int64 = long long;
            using uint64 = unsigned long long;
        #endif

        // ====================================================================
        // SECTION 6 : STRUCTURE BYTE (WRAPPER CONSTEXPR POUR OPÉRATIONS BITWISE)
        // ====================================================================
        // Wrapper type-safe pour uint8 avec opérateurs constexpr et conversions sécurisées.

        /**
         * @brief Wrapper type-safe pour opérations bitwise sur octets
         * @struct Byte
         * @ingroup PrimitiveTypes
         *
         * Fournit des opérations bitwise constexpr avec vérification de type.
         * Toutes les opérations sont noexcept et évaluables à la compilation.
         *
         * @note Utiliser Byte::from<T>() pour des conversions sécurisées depuis d'autres types.
         *
         * @example
         * @code
         * Byte flags = Byte::from(0xFF);
         * Byte masked = flags & Byte::from(0x0F);  // 0x0F
         * uint8 value = static_cast<uint8>(masked); // Conversion explicite
         *
         * // Opérations constexpr
         * constexpr Byte b = Byte::from(0x80) >> 4;  // 0x08
         * @endcode
         */
        struct NKENTSEU_CLASS_EXPORT Byte {
            /**
             * @brief Valeurs hexadécimales prédéfinies pour commodité
             * @enum Value
             * @ingroup PrimitiveTypes
             */
            enum Value : uint8 {
                _0 = 0x0, _1 = 0x1, _2 = 0x2, _3 = 0x3,
                _4 = 0x4, _5 = 0x5, _6 = 0x6, _7 = 0x7,
                _8 = 0x8, _9 = 0x9, _a = 0xA, _b = 0xB,
                _c = 0xC, _d = 0xD, _e = 0xE, _f = 0xF
            };

            /** @brief Valeur brute stockée */
            uint8 value;

            /**
             * @brief Constructeur explicite depuis uint8
             * @param v Valeur initiale
             */
            NKENTSEU_FORCE_INLINE constexpr explicit Byte(uint8 v) noexcept : value(v) {
            }

            /**
             * @brief Constructeur par défaut (valeur 0)
             */
            NKENTSEU_FORCE_INLINE constexpr Byte() noexcept : value(0) {
            }

            // -----------------------------------------------------------------
            // Opérateurs bitwise (constexpr, noexcept)
            // -----------------------------------------------------------------

            /** @brief OU bitwise */
            NKENTSEU_FORCE_INLINE constexpr Byte operator|(Byte b) const noexcept {
                return Byte(value | b.value);
            }

            /** @brief ET bitwise */
            NKENTSEU_FORCE_INLINE constexpr Byte operator&(Byte b) const noexcept {
                return Byte(value & b.value);
            }

            /** @brief OU exclusif bitwise */
            NKENTSEU_FORCE_INLINE constexpr Byte operator^(Byte b) const noexcept {
                return Byte(value ^ b.value);
            }

            /** @brief NON bitwise */
            NKENTSEU_FORCE_INLINE constexpr Byte operator~() const noexcept {
                return Byte(~value);
            }

            /**
             * @brief Décalage à gauche
             * @tparam T Type du décalage (int, uint, etc.)
             * @param shift Nombre de positions à décaler
             * @return Nouveau Byte avec bits décalés
             */
            template <typename T>
            NKENTSEU_FORCE_INLINE constexpr Byte operator<<(T shift) const noexcept {
                return Byte(static_cast<uint8>(value << shift));
            }

            /**
             * @brief Décalage à droite
             * @tparam T Type du décalage
             * @param shift Nombre de positions à décaler
             * @return Nouveau Byte avec bits décalés
             */
            template <typename T>
            NKENTSEU_FORCE_INLINE constexpr Byte operator>>(T shift) const noexcept {
                return Byte(static_cast<uint8>(value >> shift));
            }

            // -----------------------------------------------------------------
            // Conversions sécurisées
            // -----------------------------------------------------------------

            /**
             * @brief Conversion sécurisée depuis n'importe quel type entier
             * @tparam T Type source (doit être convertible en uint8)
             * @param v Valeur à convertir
             * @return Byte avec valeur tronquée si nécessaire
             *
             * @note La conversion est silencieuse : les bits de poids fort sont perdus
             * si T est plus large que uint8.
             */
            template <typename T>
            NKENTSEU_FORCE_INLINE static constexpr Byte from(T v) noexcept {
                return Byte(static_cast<uint8>(v));
            }

            /**
             * @brief Opérateur de conversion explicite vers n'importe quel type
             * @tparam T Type cible
             * @return Valeur convertie
             *
             * @note Conversion explicite requise pour éviter les ambiguïtés.
             */
            template <typename T>
            NKENTSEU_FORCE_INLINE constexpr explicit operator T() const noexcept {
                return static_cast<T>(value);
            }
        };

        // ====================================================================
        // SECTION 7 : SUPPORT OPTIONNEL DES ENTIERS 128 BITS
        // ====================================================================
        // Détection et définition conditionnelle des types 128 bits.

        /**
         * @brief Indicateur de disponibilité des entiers 128 bits natifs
         * @def NKENTSEU_INT128_AVAILABLE
         * @ingroup TypeDetection
         * @value 1 si __int128 disponible, 0 sinon
         *
         * Exclut les plateformes embarquées (PSP, NDS) qui ne supportent pas __int128.
         */
        #if defined(__SIZEOF_INT128__) && !defined(__PSP__) && !defined(__NDS__)
            #define NKENTSEU_INT128_AVAILABLE 1

            /**
             * @brief Entier signé 128 bits (natif GCC/Clang)
             * @typedef int128
             * @ingroup PrimitiveTypes
             */
            using int128 = __int128_t;

            /**
             * @brief Entier non signé 128 bits (natif GCC/Clang)
             * @typedef uint128
             * @ingroup PrimitiveTypes
             */
            using uint128 = __uint128_t;
        #else
            #define NKENTSEU_INT128_AVAILABLE 0

            /**
             * @brief Structure émulant un entier signé 128 bits
             * @struct int128
             * @ingroup PrimitiveTypes
             *
             * Représentation logicielle : deux int64 (low/high).
             * Les opérations arithmétiques doivent être implémentées manuellement.
             */
            struct NKENTSEU_CLASS_EXPORT int128 {
                int64 low;   /** @brief 64 bits de poids faible */
                int64 high;  /** @brief 64 bits de poids fort */
            };

            /**
             * @brief Structure émulant un entier non signé 128 bits
             * @struct uint128
             * @ingroup PrimitiveTypes
             */
            struct NKENTSEU_CLASS_EXPORT uint128 {
                uint64 low;   /** @brief 64 bits de poids faible */
                uint64 high;  /** @brief 64 bits de poids fort */
            };
        #endif

        // ====================================================================
        // SECTION 8 : TYPES FLOTTANTS
        // ====================================================================
        // Alias avec précision explicite pour les calculs numériques.

        /**
         * @brief Flottant simple précision (32 bits, IEEE 754)
         * @typedef float32
         * @ingroup PrimitiveTypes
         * @note Équivalent à float standard
         */
        using float32 = float;

        /**
         * @brief Flottant double précision (64 bits, IEEE 754)
         * @typedef float64
         * @ingroup PrimitiveTypes
         * @note Équivalent à double standard
         */
        using float64 = double;

        /**
         * @brief Flottant précision étendue (80+ bits, dépend de la plateforme)
         * @typedef float80
         * @ingroup PrimitiveTypes
         * @note Sur x86 : 80 bits (long double), sur ARM : souvent 64 bits
         */
        using float80 = long double;

        // ====================================================================
        // SECTION 9 : TYPES CARACTÈRES ET ENCODAGE
        // ====================================================================
        // Support des différents encodages de caractères (ASCII, UTF-8, UTF-16, UTF-32).

        /**
         * @brief Type de caractère par défaut du framework
         * @typedef Char
         * @ingroup CharacterTypes
         * @note Équivalent à char (encoding dépendant de la plateforme)
         */
        using Char = char;

        // -------------------------------------------------------------------------
        // Sous-section : Support C++20 char8_t/char16_t/char32_t
        // -------------------------------------------------------------------------
        #if defined(__cplusplus) && __cplusplus >= 202002L
            /** @brief Caractère UTF-8 (C++20 char8_t) */
            using char8 = char8_t;
            /** @brief Caractère UTF-16 (C++20 char16_t) */
            using char16 = char16_t;
            /** @brief Caractère UTF-32 (C++20 char32_t) */
            using char32 = char32_t;
        #else
            /** @brief Caractère UTF-8 (fallback uint8) */
            using char8 = uint8;
            /** @brief Caractère UTF-16 (fallback uint16) */
            using char16 = uint16;
            /** @brief Caractère UTF-32 (fallback uint32) */
            using char32 = uint32;
        #endif

        /**
         * @brief Caractère large (wchar_t sur Windows, char32 sur POSIX)
         * @typedef wchar
         * @ingroup CharacterTypes
         *
         * Abstraction portable pour le traitement de texte large.
         * Sur Windows : wchar_t (UTF-16), sur POSIX : char32 (UTF-32).
         */
        #if defined(_WIN32) || defined(_WIN64)
            using wchar = wchar_t;
        #else
            using wchar = char32;
        #endif

        // ====================================================================
        // SECTION 10 : TYPES BOOLÉENS
        // ====================================================================
        // Définitions de types booléens avec tailles explicites.

        /**
         * @brief Type booléen par défaut
         * @typedef Bool
         * @ingroup BooleanTypes
         * @note Équivalent à bool standard si non redéfini
         */
        #ifndef Bool
            using Bool = bool;
        #endif

        /**
         * @brief Booléen stocké sur 8 bits (0 = false, 1 = true)
         * @typedef Boolean
         * @ingroup BooleanTypes
         * @note Utile pour les structures binaires ou l'interop C
         */
        using Boolean = uint8;

        /**
         * @brief Booléen stocké sur 32 bits (pour alignement mémoire)
         * @typedef bool32
         * @ingroup BooleanTypes
         * @note Utile pour éviter le padding dans les structures
         */
        using bool32 = int32;

        /**
         * @brief Constante true pour le type Boolean
         * @def True
         * @ingroup BooleanTypes
         * @value 1
         */
        #ifndef True
            static constexpr Boolean True = 1;
        #endif

        /**
         * @brief Constante false pour le type Boolean
         * @def False
         * @ingroup BooleanTypes
         * @value 0
         */
        #ifndef False
            static constexpr Boolean False = 0;
        #endif

        // ====================================================================
        // SECTION 11 : TYPES POINTEURS ET TAILLES
        // ====================================================================
        // Alias pour les pointeurs et types de taille avec sémantique explicite.

        /** @brief Pointeur générique vers void */
        using PTR = void*;

        /** @brief Pointeur constant vers uint8 */
        using ConstBytePtr = const uint8*;

        /** @brief Pointeur constant vers void */
        using ConstVoidPtr = const void*;

        /** @brief Pointeur mutable vers uint8 */
        using BytePtr = uint8*;

        /** @brief Pointeur mutable vers void */
        using VoidPtr = void*;

        /** @brief Entier non signé capable de contenir un pointeur */
        using UPTR = unsigned long long int;

        /** @brief Taille non signée (alias pour uint64) */
        using usize = uint64;

        /**
         * @brief Type pour les différences de pointeurs
         * @typedef ptrdiff
         * @ingroup PointerTypes
         * @note Dépend de l'architecture : int32 sur 32-bit, int64 sur 64-bit
         */
        #if !defined(NKENTSEU_USING_STD_PTRDIFF_T)
            #ifdef NKENTSEU_ARCH_64BIT
                using ptrdiff = int64;
            #else
                using ptrdiff = int32;
            #endif
        #endif

        /**
         * @brief Conversion sécurisée de ConstVoidPtr vers T*
         * @tparam T Type cible du pointeur
         * @param ptr Pointeur source const void*
         * @return Pointeur converti vers T*
         * @ingroup PointerUtilities
         *
         * @note Cette fonction n'effectue pas de vérification de type à l'exécution.
         * La responsabilité de la validité du cast incombe à l'appelant.
         */
        template <typename T>
        NKENTSEU_FORCE_INLINE T* SafeConstCast(ConstVoidPtr ptr) noexcept {
            return const_cast<T*>(static_cast<const T*>(ptr));
        }

        /**
         * @brief Conversion sécurisée de VoidPtr vers T*
         * @tparam T Type cible du pointeur
         * @param ptr Pointeur source void*
         * @return Pointeur converti vers T*
         * @ingroup PointerUtilities
         */
        template <typename T>
        NKENTSEU_FORCE_INLINE T* NkSafeCast(VoidPtr ptr) noexcept {
            return static_cast<T*>(ptr);
        }

        // ====================================================================
        // SECTION 12 : TYPES DE TAILLE UNIVERSELS (CPU/GPU)
        // ====================================================================
        // Types de taille avec alignement spécifique pour CPU ou GPU.

        /**
         * @brief Taille alignée pour opérations CPU
         * @typedef usize_cpu
         * @ingroup SizeTypes
         * @note Aligné sur 8 bytes en 64-bit, 4 bytes en 32-bit
         */
        #ifdef NKENTSEU_ARCH_64BIT
            using usize_cpu = NKENTSEU_ALIGN(8) uint64;
        #else
            using usize_cpu = NKENTSEU_ALIGN(4) uint32;
        #endif

        /**
         * @brief Taille alignée pour opérations GPU/transfert
         * @typedef usize_gpu
         * @ingroup SizeTypes
         * @note Toujours aligné sur 16 bytes pour compatibilité SIMD/DMA
         */
        using usize_gpu = NKENTSEU_ALIGN(16) int64;

        /**
         * @brief Entier signé capable de contenir un pointeur
         * @typedef intptr
         * @ingroup PointerTypes
         */
        #if defined(NKENTSEU_ARCH_64BIT)
            using intptr = int64;
            using uintptr = uint64;
        #else
            using intptr = int32;
            using uintptr = uint32;
        #endif

        // ====================================================================
        // SECTION 13 : ALIAS AVEC PRÉFIXE NKENTSEU_ (NOMENCLATURE OFFICIELLE)
        // ====================================================================
        // Alias avec préfixe nk_ pour une identification claire dans le code.

        // -------------------------------------------------------------------------
        // Types entiers signés
        // -------------------------------------------------------------------------
        using nk_int8 = int8;
        using nk_int16 = int16;
        using nk_int32 = int32;
        using nk_int64 = int64;
        using nk_int128 = int128;

        // -------------------------------------------------------------------------
        // Types entiers non signés
        // -------------------------------------------------------------------------
        using nk_uint8 = uint8;
        using nk_uint16 = uint16;
        using nk_uint32 = uint32;
        using nk_uint64 = uint64;
        using nk_uint128 = uint128;

        // -------------------------------------------------------------------------
        // Types flottants
        // -------------------------------------------------------------------------
        using nk_float32 = float32;
        using nk_float64 = float64;
        using nk_float80 = float80;

        // -------------------------------------------------------------------------
        // Types de taille machine
        // -------------------------------------------------------------------------
        using nk_size = usize;
        using nk_ptrdiff = ptrdiff;
        using nk_intptr = intptr;
        using nk_uintptr = uintptr;

        // -------------------------------------------------------------------------
        // Types booléens
        // -------------------------------------------------------------------------
        using nk_bool = Bool;
        using nk_boolean = Boolean;
        using nk_bool8 = uint8;
        using nk_bool32 = bool32;

        // -------------------------------------------------------------------------
        // Types caractères
        // -------------------------------------------------------------------------
        using nk_char = Char;
        using nk_char8 = char8;
        using nk_uchar = unsigned char;
        using nk_char16 = char16;
        using nk_char32 = char32;
        using nk_wchar = wchar;

        // -------------------------------------------------------------------------
        // Types octets
        // -------------------------------------------------------------------------
        using nk_byte = uint8;
        using nk_sbyte = int8;

        // -------------------------------------------------------------------------
        // Types pointeurs
        // -------------------------------------------------------------------------
        using nk_ptr = PTR;
        using nk_constbyteptr = ConstBytePtr;
        using nk_constvoidptr = ConstVoidPtr;
        using nk_byteptr = BytePtr;
        using nk_voidptr = VoidPtr;
        using nk_uptr = UPTR;
        using nk_usize = usize;

        // ====================================================================
        // SECTION 14 : ALIAS COURTS POUR USAGE INTERNE (COMPATIBILITÉ)
        // ====================================================================
        // Alias très courts (i8, u32, f32) pour le code interne du framework.
        // Déconseillés pour le code application (préférer nk_* ou types standards).

        // -------------------------------------------------------------------------
        // Entiers signés courts
        // -------------------------------------------------------------------------
        using i8 = nk_int8;
        using i16 = nk_int16;
        using i32 = nk_int32;
        using i64 = nk_int64;
        #if NKENTSEU_INT128_AVAILABLE
            using i128 = nk_int128;
        #endif

        // -------------------------------------------------------------------------
        // Entiers non signés courts
        // -------------------------------------------------------------------------
        using u8 = nk_uint8;
        using u16 = nk_uint16;
        using u32 = nk_uint32;
        using u64 = nk_uint64;
        #if NKENTSEU_INT128_AVAILABLE
            using u128 = nk_uint128;
        #endif

        // -------------------------------------------------------------------------
        // Flottants courts
        // -------------------------------------------------------------------------
        using f32 = nk_float32;
        using f64 = nk_float64;
        using f80 = nk_float80;

        // -------------------------------------------------------------------------
        // Tailles courtes
        // -------------------------------------------------------------------------
        using usize = nk_size;
        using isize = nk_ptrdiff;

        // ====================================================================
        // SECTION 15 : CONSTANTES ET LIMITES DES TYPES
        // ====================================================================
        // Définitions des valeurs minimales et maximales pour chaque type.

        // -------------------------------------------------------------------------
        // Limites int8 / uint8
        // -------------------------------------------------------------------------
        #define NKENTSEU_INT8_MIN  static_cast<nk_int8>(-128)
        #define NKENTSEU_INT8_MAX  static_cast<nk_int8>(127)
        #define NKENTSEU_UINT8_MAX static_cast<nk_uint8>(255U)

        // -------------------------------------------------------------------------
        // Limites int16 / uint16
        // -------------------------------------------------------------------------
        #define NKENTSEU_INT16_MIN  static_cast<nk_int16>(-32768)
        #define NKENTSEU_INT16_MAX  static_cast<nk_int16>(32767)
        #define NKENTSEU_UINT16_MAX static_cast<nk_uint16>(65535U)

        // -------------------------------------------------------------------------
        // Limites int32 / uint32
        // -------------------------------------------------------------------------
        #define NKENTSEU_INT32_MIN  static_cast<nk_int32>(-2147483647 - 1)
        #define NKENTSEU_INT32_MAX  static_cast<nk_int32>(2147483647)
        #define NKENTSEU_UINT32_MAX static_cast<nk_uint32>(4294967295U)

        // -------------------------------------------------------------------------
        // Limites int64 / uint64
        // -------------------------------------------------------------------------
        #define NKENTSEU_INT64_MIN  static_cast<nk_int64>(-9223372036854775807LL - 1)
        #define NKENTSEU_INT64_MAX  static_cast<nk_int64>(9223372036854775807LL)
        #define NKENTSEU_UINT64_MAX static_cast<nk_uint64>(18446744073709551615ULL)

        // -------------------------------------------------------------------------
        // Limites flottantes (valeurs approximatives IEEE 754)
        // -------------------------------------------------------------------------
        #define NKENTSEU_FLOAT32_MIN 1.175494351e-38f
        #define NKENTSEU_FLOAT32_MAX 3.402823466e+38f
        #define NKENTSEU_FLOAT64_MIN 2.2250738585072014e-308
        #define NKENTSEU_FLOAT64_MAX 1.7976931348623158e+308

        // -------------------------------------------------------------------------
        // Limite size_t (dépend de l'architecture)
        // -------------------------------------------------------------------------
        #if defined(NKENTSEU_ARCH_64BIT)
            #define NKENTSEU_SIZE_MAX NKENTSEU_UINT64_MAX
        #else
            #define NKENTSEU_SIZE_MAX NKENTSEU_UINT32_MAX
        #endif

        // -------------------------------------------------------------------------
        // Macros de compatibilité (noms alternatifs)
        // -------------------------------------------------------------------------
        // Ces macros sont fournies pour la compatibilité avec l'ancien code.
        // Préférer les versions NKENTSEU_*_MIN/MAX ci-dessus.

        #define NKENTSEU_MAX_UINT8  0xFFU
        #define NKENTSEU_MAX_INT8   0x7F
        #define NKENTSEU_MIN_INT8   (-0x7F - 1)
        #define NKENTSEU_MAX_UINT16 0xFFFFU
        #define NKENTSEU_MAX_INT16  0x7FFF
        #define NKENTSEU_MIN_INT16  (-0x7FFF - 1)
        #define NKENTSEU_MAX_UINT32 0xFFFFFFFFU
        #define NKENTSEU_MAX_INT32  0x7FFFFFFF
        #define NKENTSEU_MIN_INT32  (-0x7FFFFFFF - 1)
        #define NKENTSEU_MAX_UINT64 0xFFFFFFFFFFFFFFFFULL
        #define NKENTSEU_MAX_INT64  0x7FFFFFFFFFFFFFFFLL
        #define NKENTSEU_MIN_INT64  (-0x7FFFFFFFFFFFFFFFLL - 1)

        // -------------------------------------------------------------------------
        // Limites flottantes via en-têtes standards
        // -------------------------------------------------------------------------
        #include <cfloat>

        #define NKENTSEU_MAX_FLOAT32 FLT_MAX
        #define NKENTSEU_MIN_FLOAT32 FLT_MIN
        #define NKENTSEU_MAX_FLOAT64 DBL_MAX
        #define NKENTSEU_MIN_FLOAT64 DBL_MIN
        #define NKENTSEU_MAX_FLOAT80 LDBL_MAX
        #define NKENTSEU_MIN_FLOAT80 LDBL_MIN

        // ====================================================================
        // SECTION 16 : VALEURS SPÉCIALES ET CONSTANTES D'ERREUR
        // ====================================================================
        // Constantes représentant des valeurs invalides ou sentinelles.

        /**
         * @brief Macro pour le pointeur nul portable
         * @def NK_NULL
         * @ingroup SpecialValues
         * @value nullptr si C++11+, 0 sinon
         */
        #if defined(NKENTSEU_HAS_NULLPTR)
            #define NK_NULL nullptr
        #else
            #define NK_NULL 0
        #endif

        /**
         * @brief Valeur invalide pour nk_size
         * @def NKENTSEU_INVALID_SIZE
         * @ingroup SpecialValues
         * @value (nk_size)(-1) = valeur maximale
         */
        #define NKENTSEU_INVALID_SIZE static_cast<nk_size>(-1)

        /**
         * @brief Valeur invalide pour les indices
         * @def NKENTSEU_INVALID_INDEX
         * @ingroup SpecialValues
         * @value Identique à NKENTSEU_INVALID_SIZE
         */
        #define NKENTSEU_INVALID_INDEX static_cast<nk_size>(-1)

        /**
         * @brief Identifiant invalide générique (64-bit)
         * @def NKENTSEU_INVALID_ID
         * @ingroup SpecialValues
         * @value UINT64_MAX
         */
        #define NKENTSEU_INVALID_ID UINT64_MAX

        /**
         * @brief Alias pour NKENTSEU_INVALID_ID
         * @def NKENTSEU_INVALID_ID_UINT64
         * @ingroup SpecialValues
         */
        #define NKENTSEU_INVALID_ID_UINT64 UINT64_MAX

        /**
         * @brief Identifiant invalide 32-bit
         * @def NKENTSEU_INVALID_ID_UINT32
         * @ingroup SpecialValues
         * @value UINT32_MAX
         */
        #define NKENTSEU_INVALID_ID_UINT32 UINT32_MAX

        /**
         * @brief Identifiant invalide 16-bit
         * @def NKENTSEU_INVALID_ID_UINT16
         * @ingroup SpecialValues
         * @value UINT16_MAX
         */
        #define NKENTSEU_INVALID_ID_UINT16 UINT16_MAX

        /**
         * @brief Identifiant invalide 8-bit
         * @def NKENTSEU_INVALID_ID_UINT8
         * @ingroup SpecialValues
         * @value UINT8_MAX
         */
        #define NKENTSEU_INVALID_ID_UINT8 UINT8_MAX

        /**
         * @brief Valeur maximale pour usize
         * @def NKENTSEU_USIZE_MAX
         * @ingroup SpecialValues
         * @value UINT64_MAX (usize est toujours uint64)
         */
        #define NKENTSEU_USIZE_MAX UINT64_MAX

        // ====================================================================
        // SECTION 17 : MACROS DE CONVERSION D'ENDIANNESS
        // ====================================================================
        // Conversion entre little-endian et big-endian (ordre réseau).

        /**
         * @brief Convertir uint16 vers big-endian
         * @def NkToBigEndian16
         * @param x Valeur uint16 en host order
         * @return Valeur en big-endian
         * @ingroup EndianConversion
         *
         * @note Utilise les intrinsèques du compilateur si disponibles.
         * Sur little-endian : byte-swap, sur big-endian : no-op.
         */
        #ifdef NKENTSEU_ARCH_LITTLE_ENDIAN
            #if defined(NKENTSEU_COMPILER_MSVC)
                #define NkToBigEndian16(x) _byteswap_ushort(x)
            #else
                #define NkToBigEndian16(x) __builtin_bswap16(x)
            #endif
        #else
            #define NkToBigEndian16(x) (x)
        #endif

        /**
         * @brief Convertir uint32 vers big-endian
         * @def NkToBigEndian32
         * @param x Valeur uint32 en host order
         * @return Valeur en big-endian
         * @ingroup EndianConversion
         */
        #ifdef NKENTSEU_ARCH_LITTLE_ENDIAN
            #if defined(NKENTSEU_COMPILER_MSVC)
                #define NkToBigEndian32(x) _byteswap_ulong(x)
            #else
                #define NkToBigEndian32(x) __builtin_bswap32(x)
            #endif
        #else
            #define NkToBigEndian32(x) (x)
        #endif

        /**
         * @brief Convertir uint64 vers big-endian
         * @def NkToBigEndian64
         * @param x Valeur uint64 en host order
         * @return Valeur en big-endian
         * @ingroup EndianConversion
         */
        #ifdef NKENTSEU_ARCH_LITTLE_ENDIAN
            #if defined(NKENTSEU_COMPILER_MSVC)
                #define NkToBigEndian64(x) _byteswap_uint64(x)
            #else
                #define NkToBigEndian64(x) __builtin_bswap64(x)
            #endif
        #else
            #define NkToBigEndian64(x) (x)
        #endif

        /**
         * @brief Convertir uint16 depuis big-endian vers host order
         * @def NkToLittleEndian16
         * @param x Valeur uint16 en big-endian
         * @return Valeur en host order
         * @ingroup EndianConversion
         *
         * @note Opération symétrique : from-big = to-big sur little-endian.
         */
        #ifdef NKENTSEU_ARCH_LITTLE_ENDIAN
            #define NkToLittleEndian16(x) NkToBigEndian16(x)
        #else
            #define NkToLittleEndian16(x) (x)
        #endif

        /**
         * @brief Convertir uint32 depuis big-endian vers host order
         * @def NkToLittleEndian32
         * @param x Valeur uint32 en big-endian
         * @return Valeur en host order
         * @ingroup EndianConversion
         */
        #ifdef NKENTSEU_ARCH_LITTLE_ENDIAN
            #define NkToLittleEndian32(x) NkToBigEndian32(x)
        #else
            #define NkToLittleEndian32(x) (x)
        #endif

        /**
         * @brief Convertir uint64 depuis big-endian vers host order
         * @def NkToLittleEndian64
         * @param x Valeur uint64 en big-endian
         * @return Valeur en host order
         * @ingroup EndianConversion
         */
        #ifdef NKENTSEU_ARCH_LITTLE_ENDIAN
            #define NkToLittleEndian64(x) NkToBigEndian64(x)
        #else
            #define NkToLittleEndian64(x) (x)
        #endif

    } // namespace nkentseu

    // ====================================================================
    // SECTION 18 : SOUS-NAMESPACE NKENTSEU::CORE - TYPES AVANCÉS
    // ====================================================================
    // Types spécialisés pour les systèmes internes du core.

    namespace nkentseu {

        // Indentation supplémentaire pour le sous-namespace
        namespace core {

            // =================================================================
            // TYPES HASH
            // =================================================================

            /**
             * @brief Type pour les valeurs de hash (taille dépend de l'architecture)
             * @typedef NkHashValue
             * @ingroup AdvancedTypes
             * @value uint64 sur 64-bit, uint32 sur 32-bit
             *
             * Utilisé pour les tables de hachage, caches, et identifiants dérivés.
             */
            #if defined(NKENTSEU_ARCH_64BIT)
                using NkHashValue = uint64;
            #else
                using NkHashValue = uint32;
            #endif

            /**
             * @brief Hash 32 bits explicite
             * @typedef NkHash32
             * @ingroup AdvancedTypes
             */
            using NkHash32 = uint32;

            /**
             * @brief Hash 64 bits explicite
             * @typedef NkHash64
             * @ingroup AdvancedTypes
             */
            using NkHash64 = uint64;

            // =================================================================
            // TYPES HANDLE (RÉFÉRENCES OPAQUES)
            // =================================================================

            /**
             * @brief Handle opaque pour références à des ressources
             * @typedef NkHandle
             * @ingroup AdvancedTypes
             * @value uintptr (taille d'un pointeur)
             *
             * Utilisé pour les références à des ressources gérées (textures, buffers, etc.).
             * La valeur 0 représente un handle invalide.
             *
             * @example
             * @code
             * NkHandle texture = CreateTexture(...);
             * if (texture != INVALID_HANDLE) {
             *     UseTexture(texture);
             * }
             * @endcode
             */
            using NkHandle = uintptr;

            /**
             * @brief Valeur représentant un handle invalide
             * @def INVALID_HANDLE
             * @ingroup AdvancedTypes
             * @value 0
             */
            constexpr NkHandle INVALID_HANDLE = 0;

            // =================================================================
            // TYPES ID (IDENTIFIANTS UNIQUES)
            // =================================================================

            /**
             * @brief Identifiant unique 32 bits
             * @typedef NkID32
             * @ingroup AdvancedTypes
             */
            using NkID32 = uint32;

            /**
             * @brief Identifiant unique 64 bits
             * @typedef NkID64
             * @ingroup AdvancedTypes
             */
            using NkID64 = uint64;

            /**
             * @brief Valeur représentant un ID32 invalide
             * @def INVALID_ID32
             * @ingroup AdvancedTypes
             * @value 0xFFFFFFFF
             */
            constexpr NkID32 INVALID_ID32 = 0xFFFFFFFF;

            /**
             * @brief Valeur représentant un ID64 invalide
             * @def INVALID_ID64
             * @ingroup AdvancedTypes
             * @value 0xFFFFFFFFFFFFFFFF
             */
            constexpr NkID64 INVALID_ID64 = 0xFFFFFFFFFFFFFFFF;

        } // namespace core

    } // namespace nkentseu

    // ====================================================================
    // SECTION 19 : SOUS-NAMESPACE NKENTSEU::MATH - TYPES MATHÉMATIQUES
    // ====================================================================
    // Types pour les calculs mathématiques avec précision configurable.

    namespace nkentseu {

        // Indentation supplémentaire pour le sous-namespace
        namespace math {

            /**
             * @brief Type flottant par défaut pour les calculs mathématiques
             * @typedef NkReal
             * @ingroup MathTypes
             * @value float32 par défaut, float64 si NKENTSEU_MATH_USE_DOUBLE défini
             *
             * Permet de basculer globalement la précision des calculs mathématiques
             * en définissant NKENTSEU_MATH_USE_DOUBLE avant l'inclusion de ce fichier.
             *
             * @example
             * @code
             * // Pour utiliser double précision partout :
             * #define NKENTSEU_MATH_USE_DOUBLE
             * #include "NKCore/NkTypes.h"
             *
             * NkReal x = 3.14159;  // float64 si NKENTSEU_MATH_USE_DOUBLE défini
             * @endcode
             */
            #if defined(NKENTSEU_MATH_USE_DOUBLE)
                using NkReal = float64;
            #else
                using NkReal = float32;
            #endif

            /**
             * @brief Angle exprimé en radians
             * @typedef NkRadians
             * @ingroup MathTypes
             * @value Alias vers NkReal
             *
             * Utiliser pour documenter l'unité des paramètres/retours angulaires.
             */
            using NkRadians = NkReal;

            /**
             * @brief Angle exprimé en degrés
             * @typedef NkDegrees
             * @ingroup MathTypes
             * @value Alias vers NkReal
             *
             * Utiliser pour documenter l'unité des paramètres/retours angulaires.
             * Conversion : radians = degrés * (π / 180)
             */
            using NkDegrees = NkReal;

        } // namespace math

    } // namespace nkentseu

#endif // NKENTSEU_CORE_NKTYPES_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKTYPES.H
// =============================================================================
// Ce fichier fournit des types portables et des utilitaires pour le développement
// multiplateforme avec NKCore.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Utilisation des types entiers fixes
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkTypes.h"

    void ProcessData() {
        // Types avec taille garantie
        nkentseu::int8 smallValue = -128;
        nkentseu::uint32 counter = 0;
        nkentseu::int64 largeNumber = 9223372036854775807LL;

        // Alias courts pour code interne (déconseillé en API publique)
        using namespace nkentseu;
        u32 fastCounter = 0;  // Équivalent à uint32

        // Alias avec préfixe nk_ pour API publique
        nk_uint64 uniqueId = GenerateUniqueId();
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Utilisation du wrapper Byte pour opérations bitwise
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkTypes.h"

    constexpr nkentseu::Byte ComputeFlags() {
        using namespace nkentseu;

        // Construction constexpr
        Byte flags = Byte::from(0xFF);

        // Opérations bitwise constexpr
        flags = flags & Byte::from(0x0F);  // Masquage bas nibble
        flags = flags | Byte::from(0x80);  // Set bit 7

        // Décalages constexpr
        flags = flags << 2;  // Décalage gauche

        return flags;
    }

    void UseByte() {
        constexpr Byte result = ComputeFlags();

        // Conversion explicite vers uint8
        uint8 value = static_cast<uint8>(result);

        // Conversion depuis d'autres types
        Byte fromInt = Byte::from(42);
        Byte fromLong = Byte::from(1000L);  // Tronqué à 0xE8 (232)
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Gestion des identifiants et handles
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkTypes.h"

    class ResourceManager {
    public:
        using ResourceHandle = nkentseu::core::NkHandle;
        using ResourceId = nkentseu::core::NkID64;

        ResourceHandle CreateResource(const char* name) {
            ResourceId id = GenerateResourceId(name);
            ResourceHandle handle = static_cast<ResourceHandle>(id);

            if (handle != nkentseu::core::INVALID_HANDLE) {
                m_resources[handle] = LoadResource(id);
            }
            return handle;
        }

        bool IsValid(ResourceHandle handle) const {
            return handle != nkentseu::core::INVALID_HANDLE;
        }

    private:
        nkentseu::core::NkID64 GenerateResourceId(const char* name) {
            // Génération d'ID unique (hash, compteur, etc.)
            return static_cast<nkentseu::core::NkID64>(ComputeHash(name));
        }

        nkentseu::core::NkID64 ComputeHash(const char* str);
        // ... autres membres ...
    };
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Conversion d'endianness pour protocole réseau
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkTypes.h"
    #include <cstring>

    struct NetworkHeader {
        nkentseu::uint16 magic;      // 0xABCD en big-endian
        nkentseu::uint32 payloadSize;
        nkentseu::uint64 timestamp;
    };

    void SendPacket(int socket, const void* payload, nkentseu::usize size) {
        using namespace nkentseu;

        NetworkHeader header;
        header.magic = NkToBigEndian16(0xABCD);
        header.payloadSize = NkToBigEndian32(static_cast<uint32>(size));
        header.timestamp = NkToBigEndian64(GetTimestamp());

        // Envoi : header en big-endian + payload
        send(socket, &header, sizeof(header), 0);
        send(socket, payload, static_cast<size_t>(size), 0);
    }

    bool ReceiveHeader(int socket, NetworkHeader& outHeader) {
        using namespace nkentseu;

        if (recv(socket, &outHeader, sizeof(outHeader), MSG_WAITALL) != sizeof(outHeader)) {
            return false;
        }

        // Conversion depuis big-endian vers host order
        outHeader.magic = NkToLittleEndian16(outHeader.magic);
        outHeader.payloadSize = NkToLittleEndian32(outHeader.payloadSize);
        outHeader.timestamp = NkToLittleEndian64(outHeader.timestamp);

        return true;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Types mathématiques avec précision configurable
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkTypes.h"

    // Définir avant inclusion pour utiliser double précision
    // #define NKENTSEU_MATH_USE_DOUBLE
    #include "NKCore/NkTypes.h"

    namespace nkentseu::math {

        // Fonction utilisant NkReal (précision configurable)
        NkReal ComputeDistance(NkReal x1, NkReal y1, NkReal x2, NkReal y2) {
            NkReal dx = x2 - x1;
            NkReal dy = y2 - y1;
            return sqrt(dx * dx + dy * dy);
        }

        // Conversion degrés <-> radians avec types explicites
        NkRadians DegreesToRadians(NkDegrees degrees) {
            constexpr NkReal DegToRad = static_cast<NkReal>(3.14159265358979323846 / 180.0);
            return degrees * DegToRad;
        }

        NkDegrees RadiansToDegrees(NkRadians radians) {
            constexpr NkReal RadToDeg = static_cast<NkReal>(180.0 / 3.14159265358979323846);
            return radians * RadToDeg;
        }

    } // namespace nkentseu::math
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Utilisation des constantes de limites et valeurs spéciales
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkTypes.h"
    #include "NKPlatform/NkFoundationLog.h"

    void ValidateInputs(nkentseu::int32 value, nkentseu::usize index) {
        using namespace nkentseu;

        // Vérification des limites
        if (value < NKENTSEU_INT32_MIN || value > NKENTSEU_INT32_MAX) {
            NK_FOUNDATION_LOG_ERROR("Value out of int32 range");
            return;
        }

        // Vérification d'index valide
        if (index == NKENTSEU_INVALID_INDEX) {
            NK_FOUNDATION_LOG_ERROR("Invalid index provided");
            return;
        }

        // Utilisation de NK_NULL pour les pointeurs
        void* ptr = NK_NULL;
        if (ptr == NK_NULL) {
            NK_FOUNDATION_LOG_DEBUG("Pointer is null as expected");
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Combinaison avec d'autres modules NKCore/NKPlatform
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkPlatformDetect.h"   // NKENTSEU_PLATFORM_*
    #include "NKPlatform/NkArchDetect.h"       // NKENTSEU_ARCH_*
    #include "NKCore/NkTypes.h"                // Types portables
    #include "NKCore/NkCoreApi.h"              // NKENTSEU_CORE_API

    // Fonction utilisant types NKCore avec export API
    NKENTSEU_CORE_API nkentseu::uint64 ComputeChecksum(
        const nkentseu::uint8* data,
        nkentseu::usize size
    ) {
        using namespace nkentseu;

        uint64 hash = 0;

        // Boucle optimisée selon l'architecture
        #if defined(NKENTSEU_ARCH_64BIT)
            // Traitement par blocs de 8 bytes sur 64-bit
            for (usize i = 0; i + 8 <= size; i += 8) {
                uint64 block;
                memcpy(&block, data + i, sizeof(block));
                hash ^= NkToLittleEndian64(block);
                hash = (hash << 5) | (hash >> 59);  // Rotation
            }
        #else
            // Traitement byte-à-byte sur 32-bit
            for (usize i = 0; i < size; ++i) {
                hash ^= data[i];
                hash = (hash << 3) | (hash >> 61);
            }
        #endif

        return hash;
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================