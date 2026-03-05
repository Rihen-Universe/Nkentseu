// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\NkCompilerDetect.h
// DESCRIPTION: Détection compilateur et features C++
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKCOMPILER_H_INCLUDED
#define NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKCOMPILER_H_INCLUDED

// ============================================================
// DÉTECTION COMPILATEUR
// ============================================================

/**
 * @defgroup CompilerDetection Détection de Compilateur
 * @brief Macros pour identifier le compilateur et sa version
 */

/**
 * @brief Macro de détection MSVC
 * @def NKENTSEU_COMPILER_MSVC
 * @ingroup CompilerDetection
 */

/**
 * @brief Version du compilateur
 * @def NKENTSEU_COMPILER_VERSION
 * @ingroup CompilerDetection
 * @note Valeur numérique représentant la version (format spécifique au compilateur)
 */

// Microsoft Visual C++
#if defined(_MSC_VER)
#define NKENTSEU_COMPILER_MSVC
#define NKENTSEU_COMPILER_VERSION _MSC_VER

/**
 * @brief MSVC 2024 ou supérieur
 * @def NKENTSEU_COMPILER_MSVC_2024
 * @ingroup CompilerDetection
 */
#if _MSC_VER >= 1940
#define NKENTSEU_COMPILER_MSVC_2024
#endif

/**
 * @brief MSVC 2022 ou supérieur
 * @def NKENTSEU_COMPILER_MSVC_2022
 * @ingroup CompilerDetection
 */
#if _MSC_VER >= 1930
#define NKENTSEU_COMPILER_MSVC_2022
#endif

/**
 * @brief MSVC 2019 ou supérieur
 * @def NKENTSEU_COMPILER_MSVC_2019
 * @ingroup CompilerDetection
 */
#if _MSC_VER >= 1920
#define NKENTSEU_COMPILER_MSVC_2019
#endif

/**
 * @brief MSVC 2017 ou supérieur
 * @def NKENTSEU_COMPILER_MSVC_2017
 * @ingroup CompilerDetection
 */
#if _MSC_VER >= 1910
#define NKENTSEU_COMPILER_MSVC_2017
#endif
#endif

// Clang (y compris Apple Clang)
#if defined(__clang__)
/**
 * @brief Détection compilateur Clang
 * @def NKENTSEU_COMPILER_CLANG
 * @ingroup CompilerDetection
 */
#ifndef NKENTSEU_COMPILER_CLANG
#define NKENTSEU_COMPILER_CLANG
#endif
#define NKENTSEU_COMPILER_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)

/**
 * @brief Détection Apple Clang
 * @def NKENTSEU_COMPILER_APPLE_CLANG
 * @ingroup CompilerDetection
 */
#if defined(__apple_build_version__)
#define NKENTSEU_COMPILER_APPLE_CLANG
#endif
#endif

// GCC (GNU Compiler Collection)
#if defined(__GNUC__) && !defined(__clang__)
/**
 * @brief Détection compilateur GCC
 * @def NKENTSEU_COMPILER_GCC
 * @ingroup CompilerDetection
 */
#define NKENTSEU_COMPILER_GCC
#define NKENTSEU_COMPILER_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#endif

// Intel C++ Compiler
#if defined(__INTEL_COMPILER) || defined(__ICL) || defined(__ICC) || defined(__ECC)
/**
 * @brief Détection compilateur Intel
 * @def NKENTSEU_COMPILER_INTEL
 * @ingroup CompilerDetection
 */
#define NKENTSEU_COMPILER_INTEL
#define NKENTSEU_COMPILER_VERSION __INTEL_COMPILER
#endif

// Emscripten
#if defined(__EMSCRIPTEN__)
/**
 * @brief Détection Emscripten
 * @def NKENTSEU_COMPILER_EMSCRIPTEN
 * @ingroup CompilerDetection
 */
#define NKENTSEU_COMPILER_EMSCRIPTEN
#endif

// NVIDIA CUDA Compiler
#if defined(__NVCC__)
/**
 * @brief Détection NVCC (CUDA)
 * @def NKENTSEU_COMPILER_NVCC
 * @ingroup CompilerDetection
 */
#define NKENTSEU_COMPILER_NVCC
#endif

// Oracle/Sun Studio
#if defined(__SUNPRO_C) || defined(__SUNPRO_CC)
/**
 * @brief Détection SunPro
 * @def NKENTSEU_COMPILER_SUNPRO
 * @ingroup CompilerDetection
 */
#define NKENTSEU_COMPILER_SUNPRO
#endif

// IBM XL C/C++
#if defined(__xlC__) || defined(__xlc__)
/**
 * @brief Détection IBM XL
 * @def NKENTSEU_COMPILER_XLC
 * @ingroup CompilerDetection
 */
#define NKENTSEU_COMPILER_XLC
#endif

// ============================================================
// DÉTECTION STANDARD C++
// ============================================================

/**
 * @defgroup CppStandards Standards C++
 * @brief Macros pour identifier la version du standard C++
 */

#if __cplusplus >= 202302L
/**
 * @brief Standard C++23 ou supérieur
 * @def NKENTSEU_CPP23
 * @ingroup CppStandards
 */
#define NKENTSEU_CPP23
#define NKENTSEU_CPP_VERSION 23
#elif __cplusplus >= 202002L
/**
 * @brief Standard C++20 ou supérieur
 * @def NKENTSEU_CPP20
 * @ingroup CppStandards
 */
#ifndef NKENTSEU_CPP20
#define NKENTSEU_CPP20
#endif
#define NKENTSEU_CPP_VERSION 20
#elif __cplusplus >= 201703L
/**
 * @brief Standard C++17 ou supérieur
 * @def NKENTSEU_CPP17
 * @ingroup CppStandards
 */
#define NKENTSEU_CPP17
#define NKENTSEU_CPP_VERSION 17
#elif __cplusplus >= 201402L
/**
 * @brief Standard C++14 ou supérieur
 * @def NKENTSEU_CPP14
 * @ingroup CppStandards
 */
#define NKENTSEU_CPP14
#define NKENTSEU_CPP_VERSION 14
#elif __cplusplus >= 201103L
/**
 * @brief Standard C++11 ou supérieur
 * @def NKENTSEU_CPP11
 * @ingroup CppStandards
 */
#define NKENTSEU_CPP11
#define NKENTSEU_CPP_VERSION 11
#elif __cplusplus >= 199711L
/**
 * @brief Standard C++98/C++03
 * @def NKENTSEU_CPP98
 * @ingroup CppStandards
 */
#define NKENTSEU_CPP98
#define NKENTSEU_CPP_VERSION 98
#else
#error "C++ standard not detected - minimum required is C++11"
#endif

// ============================================================
// FEATURES C++11
// ============================================================

/**
 * @defgroup Cpp11Features Fonctionnalités C++11
 * @brief Macros indiquant la disponibilité des fonctionnalités C++11
 */

#if defined(NKENTSEU_CPP11) || defined(NKENTSEU_CPP14) || defined(NKENTSEU_CPP17) || defined(NKENTSEU_CPP20) ||        \
	defined(NKENTSEU_CPP23)
/**
 * @brief Support général C++11
 * @def NKENTSEU_HAS_CPP11
 * @ingroup Cpp11Features
 */
#define NKENTSEU_HAS_CPP11
#define NKENTSEU_HAS_NULLPTR
#define NKENTSEU_HAS_AUTO
#define NKENTSEU_HAS_DECLTYPE
#define NKENTSEU_HAS_RVALUE_REFERENCES
#define NKENTSEU_HAS_VARIADIC_TEMPLATES
#define NKENTSEU_HAS_STATIC_ASSERT
#define NKENTSEU_HAS_CONSTEXPR
#define NKENTSEU_HAS_NOEXCEPT
#define NKENTSEU_HAS_OVERRIDE
#define NKENTSEU_HAS_FINAL
#define NKENTSEU_HAS_DEFAULT_DELETE
#define NKENTSEU_HAS_LAMBDA
#define NKENTSEU_HAS_RANGE_FOR
#endif

// ============================================================
// FEATURES C++14
// ============================================================

/**
 * @defgroup Cpp14Features Fonctionnalités C++14
 * @brief Macros indiquant la disponibilité des fonctionnalités C++14
 */

#if defined(NKENTSEU_CPP14) || defined(NKENTSEU_CPP17) || defined(NKENTSEU_CPP20) || defined(NKENTSEU_CPP23)
/**
 * @brief Support général C++14
 * @def NKENTSEU_HAS_CPP14
 * @ingroup Cpp14Features
 */
#define NKENTSEU_HAS_CPP14
#define NKENTSEU_HAS_GENERIC_LAMBDAS
#define NKENTSEU_HAS_VARIABLE_TEMPLATES
#define NKENTSEU_HAS_RELAXED_CONSTEXPR
#define NKENTSEU_HAS_BINARY_LITERALS
#define NKENTSEU_HAS_DECLTYPE_AUTO
#endif

// ============================================================
// FEATURES C++17
// ============================================================

/**
 * @defgroup Cpp17Features Fonctionnalités C++17
 * @brief Macros indiquant la disponibilité des fonctionnalités C++17
 */

#if defined(NKENTSEU_CPP17) || defined(NKENTSEU_CPP20) || defined(NKENTSEU_CPP23)
/**
 * @brief Support général C++17
 * @def NKENTSEU_HAS_CPP17
 * @ingroup Cpp17Features
 */
#define NKENTSEU_HAS_CPP17
#define NKENTSEU_HAS_INLINE_VARIABLES
#define NKENTSEU_HAS_FOLD_EXPRESSIONS
#define NKENTSEU_HAS_IF_CONSTEXPR
#define NKENTSEU_HAS_STRUCTURED_BINDINGS
#define NKENTSEU_HAS_CONSTEXPR_IF
#define NKENTSEU_HAS_NODISCARD
#define NKENTSEU_HAS_MAYBE_UNUSED
#define NKENTSEU_HAS_FALLTHROUGH
#endif

// ============================================================
// FEATURES C++20
// ============================================================

/**
 * @defgroup Cpp20Features Fonctionnalités C++20
 * @brief Macros indiquant la disponibilité des fonctionnalités C++20
 */

#if defined(NKENTSEU_CPP20) || defined(NKENTSEU_CPP23)
/**
 * @brief Support général C++20
 * @def NKENTSEU_HAS_CPP20
 * @ingroup Cpp20Features
 */
#define NKENTSEU_HAS_CPP20
#define NKENTSEU_HAS_CONCEPTS
#define NKENTSEU_HAS_MODULES
#define NKENTSEU_HAS_COROUTINES
#define NKENTSEU_HAS_CONSTEXPR_VIRTUAL
#define NKENTSEU_HAS_CONSTEVAL
#define NKENTSEU_HAS_CONSTINIT
#define NKENTSEU_HAS_THREE_WAY_COMPARISON
#define NKENTSEU_HAS_DESIGNATED_INITIALIZERS
#endif

// ============================================================
// FEATURES C++23
// ============================================================

/**
 * @defgroup Cpp23Features Fonctionnalités C++23
 * @brief Macros indiquant la disponibilité des fonctionnalités C++23
 */

#if defined(NKENTSEU_CPP23)
/**
 * @brief Support général C++23
 * @def NKENTSEU_HAS_CPP23
 * @ingroup Cpp23Features
 */
#define NKENTSEU_HAS_CPP23
#define NKENTSEU_HAS_DEDUCING_THIS
#define NKENTSEU_HAS_IF_CONSTEVAL
#define NKENTSEU_HAS_MULTIDIMENSIONAL_SUBSCRIPT
#endif

// ============================================================
// MACROS DE CONVENANCE POUR FEATURES
// ============================================================

/**
 * @defgroup CppConvenience Conveniences C++
 * @brief Macros pratiques pour utiliser les fonctionnalités C++
 */

/**
 * @brief Macro constexpr (C++11+)
 * @def NKENTSEU_CONSTEXPR
 * @ingroup CppConvenience
 * @note S'étend à constexpr si disponible, vide sinon
 */
#if defined(NKENTSEU_HAS_CONSTEXPR)
#define NKENTSEU_CONSTEXPR constexpr
#else
#define NKENTSEU_CONSTEXPR
#endif

/**
 * @brief Macro noexcept (C++11+)
 * @def NKENTSEU_NOEXCEPT
 * @ingroup CppConvenience
 */
#if defined(NKENTSEU_HAS_NOEXCEPT)
#define NKENTSEU_NOEXCEPT noexcept
#define NKENTSEU_NOEXCEPT_IF(condition) noexcept(condition)
#else
#define NKENTSEU_NOEXCEPT throw()
#define NKENTSEU_NOEXCEPT_IF(condition)
#endif

/**
 * @brief Macro override (C++11+)
 * @def NKENTSEU_OVERRIDE
 * @ingroup CppConvenience
 */
#if defined(NKENTSEU_HAS_OVERRIDE)
#define NKENTSEU_OVERRIDE override
#else
#define NKENTSEU_OVERRIDE
#endif

/**
 * @brief Macro final (C++11+)
 * @def NKENTSEU_FINAL
 * @ingroup CppConvenience
 */
#if defined(NKENTSEU_HAS_FINAL)
#define NKENTSEU_FINAL final
#else
#define NKENTSEU_FINAL
#endif

/**
 * @brief Attribut [[nodiscard]] (C++17+)
 * @def NKENTSEU_NODISCARD
 * @ingroup CppConvenience
 */
#if defined(__cplusplus) && __cplusplus >= 202002L
#define NKENTSEU_NODISCARD [[nodiscard]]
#define NKENTSEU_NODISCARD_MSG(msg) [[nodiscard(msg)]]
#define NKENTSEU_NODISCARD_SIMPLE [[nodiscard]]
#elif defined(__cplusplus) && __cplusplus >= 201703L
#define NKENTSEU_NODISCARD [[nodiscard]]
#define NKENTSEU_NODISCARD_MSG(msg) [[nodiscard(msg)]]
#define NKENTSEU_NODISCARD_SIMPLE [[nodiscard]]
#else
#define NKENTSEU_NODISCARD
#define NKENTSEU_NODISCARD_MSG(msg)
#define NKENTSEU_NODISCARD_SIMPLE
#endif

/**
 * @brief Attribut [[maybe_unused]] (C++17+)
 * @def NKENTSEU_MAYBE_UNUSED
 * @ingroup CppConvenience
 */
#if defined(NKENTSEU_HAS_MAYBE_UNUSED)
#define NKENTSEU_MAYBE_UNUSED [[maybe_unused]]
#else
#define NKENTSEU_MAYBE_UNUSED
#endif

/**
 * @brief Attribut [[fallthrough]] (C++17+)
 * @def NKENTSEU_FALLTHROUGH
 * @ingroup CppConvenience
 */
#if defined(NKENTSEU_HAS_FALLTHROUGH)
#define NKENTSEU_FALLTHROUGH [[fallthrough]]
#else
#define NKENTSEU_FALLTHROUGH
#endif

// ============================================================
// ATTRIBUTES SPÉCIFIQUES COMPILATEUR
// ============================================================

/**
 * @defgroup CompilerAttributes Attributs Compilateur
 * @brief Macros pour les attributs spécifiques au compilateur
 */

/**
 * @brief Structure packed (sans padding)
 * @def NKENTSEU_PACKED
 * @ingroup CompilerAttributes
 * @note Utile pour les structures de protocole réseau ou formats binaires
 */
#if defined(NKENTSEU_COMPILER_MSVC)
#define NKENTSEU_PACKED
#define NKENTSEU_PACK_BEGIN __pragma(pack(push, 1))
#define NKENTSEU_PACK_END __pragma(pack(pop))
#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NKENTSEU_PACKED __attribute__((packed))
#define NKENTSEU_PACK_BEGIN
#define NKENTSEU_PACK_END
#else
#define NKENTSEU_PACKED
#define NKENTSEU_PACK_BEGIN
#define NKENTSEU_PACK_END
#endif

/**
 * @brief Keyword restrict (pointeur aliasing)
 * @def NKENTSEU_RESTRICT
 * @ingroup CompilerAttributes
 */
#if defined(NKENTSEU_COMPILER_MSVC)
#define NKENTSEU_RESTRICT __restrict
#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NKENTSEU_RESTRICT __restrict__
#else
#define NKENTSEU_RESTRICT
#endif

/**
 * @brief Stockage thread-local
 * @def NKENTSEU_THREAD_LOCAL
 * @ingroup CompilerAttributes
 */
#if defined(NKENTSEU_CPP11)
#define NKENTSEU_THREAD_LOCAL thread_local
#elif defined(NKENTSEU_COMPILER_MSVC)
#define NKENTSEU_THREAD_LOCAL __declspec(thread)
#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NKENTSEU_THREAD_LOCAL __thread
#else
#define NKENTSEU_THREAD_LOCAL
#endif

/**
 * @brief Attribut deprecated
 * @def NKENTSEU_DEPRECATED
 * @ingroup CompilerAttributes
 */
#if defined(NKENTSEU_CPP14)
#define NKENTSEU_DEPRECATED [[deprecated]]
#define NKENTSEU_DEPRECATED_MSG(msg) [[deprecated(msg)]]
#elif defined(NKENTSEU_COMPILER_MSVC)
#define NKENTSEU_DEPRECATED __declspec(deprecated)
#define NKENTSEU_DEPRECATED_MSG(msg) __declspec(deprecated(msg))
#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NKENTSEU_DEPRECATED __attribute__((deprecated))
#define NKENTSEU_DEPRECATED_MSG(msg) __attribute__((deprecated(msg)))
#else
#define NKENTSEU_DEPRECATED
#define NKENTSEU_DEPRECATED_MSG(msg)
#endif

// ============================================================
// MACROS STANDARD FUNCTION
// ============================================================

/**
 * @defgroup StandardMacros Macros Standard
 * @brief Macros fournies par le compilateur
 */

/**
 * @brief Nom de la fonction actuelle
 * @def NKENTSEU_FUNCTION_NAME
 * @ingroup StandardMacros
 */
#if defined(NKENTSEU_COMPILER_MSVC)
#define NKENTSEU_FUNCTION_NAME __FUNCSIG__
#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NKENTSEU_FUNCTION_NAME __PRETTY_FUNCTION__
#else
#define NKENTSEU_FUNCTION_NAME __func__
#endif

/**
 * @brief Fichier source actuel
 * @def NKENTSEU_FILE_NAME
 * @ingroup StandardMacros
 */
#define NKENTSEU_FILE_NAME __FILE__

/**
 * @brief Ligne source actuelle
 * @def NKENTSEU_LINE_NUMBER
 * @ingroup StandardMacros
 */
#define NKENTSEU_LINE_NUMBER __LINE__

// ============================================================
// DÉTECTION CAPACITÉS SPÉCIALES
// ============================================================

/**
 * @defgroup SpecialCapabilities Capacités Spéciales
 * @brief Détection de fonctionnalités spécifiques au compilateur
 */

/**
 * @brief Support RTTI (Run-Time Type Information)
 * @def NKENTSEU_HAS_RTTI
 * @ingroup SpecialCapabilities
 */
#if defined(__GXX_RTTI) || defined(_CPPRTTI) || defined(__INTEL_RTTI__)
#define NKENTSEU_HAS_RTTI
#endif

/**
 * @brief Support exceptions C++
 * @def NKENTSEU_HAS_EXCEPTIONS
 * @ingroup SpecialCapabilities
 */
#if defined(__EXCEPTIONS) || defined(_CPPUNWIND) || defined(__cpp_exceptions)
#define NKENTSEU_HAS_EXCEPTIONS
#endif

/**
 * @brief Support 128-bit integers
 * @def NKENTSEU_HAS_INT128
 * @ingroup SpecialCapabilities
 */
#if defined(__SIZEOF_INT128__) && !defined(NKENTSEU_COMPILER_MSVC)
#define NKENTSEU_HAS_INT128
typedef __int128 NKENTSEU_int128;
typedef unsigned __int128 NKENTSEU_uint128;
#endif

// ============================================================
// PRAGMAS UTILITAIRES
// ============================================================

/**
 * @defgroup Pragmas Pragmas
 * @brief Macros pour manipuler les pragmas
 */

/**
 * @brief Macro pour définir des pragmas
 * @def NKENTSEU_PRAGMA(x)
 * @param x Instruction pragma
 * @ingroup Pragmas
 */
#if defined(NKENTSEU_COMPILER_MSVC)
#define NKENTSEU_PRAGMA(x) __pragma(x)
#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NKENTSEU_PRAGMA(x) _Pragma(#x)
#else
#define NKENTSEU_PRAGMA(x)
#endif

/**
 * @brief Désactiver temporairement un warning (push)
 * @def NKENTSEU_DISABLE_WARNING_PUSH
 * @ingroup Pragmas
 */
/**
 * @brief Restaurer les warnings (pop)
 * @def NKENTSEU_DISABLE_WARNING_POP
 * @ingroup Pragmas
 */
/**
 * @brief Désactiver un warning spécifique
 * @def NKENTSEU_DISABLE_WARNING(warningNumber)
 * @param warningNumber Numéro/identifiant du warning
 * @ingroup Pragmas
 */
#if defined(NKENTSEU_COMPILER_MSVC)
#define NKENTSEU_DISABLE_WARNING_PUSH NKENTSEU_PRAGMA(warning(push))
#define NKENTSEU_DISABLE_WARNING_POP NKENTSEU_PRAGMA(warning(pop))
#define NKENTSEU_DISABLE_WARNING(warningNumber) NKENTSEU_PRAGMA(warning(disable : warningNumber))
#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NKENTSEU_DISABLE_WARNING_PUSH NKENTSEU_PRAGMA(GCC diagnostic push)
#define NKENTSEU_DISABLE_WARNING_POP NKENTSEU_PRAGMA(GCC diagnostic pop)
#define NKENTSEU_DISABLE_WARNING(warningName) NKENTSEU_PRAGMA(GCC diagnostic ignored warningName)
#else
#define NKENTSEU_DISABLE_WARNING_PUSH
#define NKENTSEU_DISABLE_WARNING_POP
#define NKENTSEU_DISABLE_WARNING(warning)
#endif

// ============================================================
// VALIDATION
// ============================================================

// S'assurer qu'au moins C++11 est supporté
#if !defined(NKENTSEU_HAS_CPP11)
#error "NKCore requires at least C++11 compiler support"
#endif

#endif // NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKCOMPILER_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================