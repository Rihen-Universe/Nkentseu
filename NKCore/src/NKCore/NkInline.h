// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\NkInline.h
// DESCRIPTION: Macros d'inlining et optimisation fonctions
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKINLINE_H_INCLUDED
#define NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKINLINE_H_INCLUDED

#include "NkCompilerDetect.h"

// ============================================================
// MACROS D'INLINING
// ============================================================

/**
 * @defgroup InlineMacros Macros d'Inlining
 * @brief Macros pour contrÃ´le de l'inlining des fonctions
 */

/**
 * @brief Inline standard C++
 * @note Suggestion au compilateur, peut Ãªtre ignorÃ©e
 * @ingroup InlineMacros
 */
#define NKENTSEU_INLINE inline

/**
 * @brief Inline agressif avec attributs d'optimisation
 * @note Combine inline + always_inline + optimisations
 * @ingroup InlineMacros
 */
#if defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NKENTSEU_AGGRESSIVE_INLINE inline __attribute__((always_inline, hot, optimize("O3")))
#elif defined(NKENTSEU_COMPILER_MSVC)
#define NKENTSEU_AGGRESSIVE_INLINE __forceinline __pragma(inline_depth(255))
#else
#define NKENTSEU_AGGRESSIVE_INLINE inline
#endif

/**
 * @brief Inline pour code critique (hot path)
 * @note SuggÃ¨re que la fonction est appelÃ©e frÃ©quemment
 * @ingroup InlineMacros
 */
#if defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NKENTSEU_HOT_INLINE inline __attribute__((hot, always_inline))
#elif defined(NKENTSEU_COMPILER_MSVC)
#define NKENTSEU_HOT_INLINE __forceinline
#else
#define NKENTSEU_HOT_INLINE NKENTSEU_FORCE_INLINE
#endif

/**
 * @brief Inline pour code rarement exÃ©cutÃ© (cold path)
 * @note SuggÃ¨re que la fonction est rarement appelÃ©e
 * @ingroup InlineMacros
 */
#if defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NKENTSEU_COLD_INLINE inline __attribute__((cold))
#else
#define NKENTSEU_COLD_INLINE inline
#endif

// ============================================================
// OPTIMISATION TAILLE VS VITESSE
// ============================================================

/**
 * @defgroup OptimizeMacros Macros d'Optimisation
 * @brief Macros pour contrÃ´le des optimisations de taille/vitesse
 */

/**
 * @brief Optimiser pour vitesse
 * @note PrivilÃ©gie performance au dÃ©triment de la taille
 * @ingroup OptimizeMacros
 */
#if defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NKENTSEU_OPTIMIZE_SPEED __attribute__((optimize("O3")))
#elif defined(NKENTSEU_COMPILER_MSVC)
#define NKENTSEU_OPTIMIZE_SPEED __pragma(optimize("t", on))
#else
#define NKENTSEU_OPTIMIZE_SPEED
#endif

/**
 * @brief Optimiser pour taille
 * @note PrivilÃ©gie taille code au dÃ©triment de la performance
 * @ingroup OptimizeMacros
 */
#if defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NKENTSEU_OPTIMIZE_SIZE __attribute__((optimize("Os")))
#elif defined(NKENTSEU_COMPILER_MSVC)
#define NKENTSEU_OPTIMIZE_SIZE __pragma(optimize("s", on))
#else
#define NKENTSEU_OPTIMIZE_SIZE
#endif

/**
 * @brief DÃ©sactiver optimisations
 * @note Utile pour debugging
 * @ingroup OptimizeMacros
 */
#if defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NKENTSEU_NO_OPTIMIZE __attribute__((optimize("O0")))
#elif defined(NKENTSEU_COMPILER_MSVC)
#define NKENTSEU_NO_OPTIMIZE __pragma(optimize("", off))
#else
#define NKENTSEU_NO_OPTIMIZE
#endif

// ============================================================
// INLINE SELON CONFIGURATION BUILD
// ============================================================

/**
 * @defgroup BuildInline Inlining selon Build
 * @brief Macros pour inlining adaptatif selon mode Debug/Release
 */

/**
 * @brief Inline en Release, normal en Debug
 * @note Facilite le debugging
 * @ingroup BuildInline
 */
#if defined(NDEBUG) || defined(NKENTSEU_RELEASE)
#define NKENTSEU_INLINE_RELEASE NKENTSEU_FORCE_INLINE
#else
#define NKENTSEU_INLINE_RELEASE inline
#endif

/**
 * @brief Inline en Debug, normal en Release
 * @note Rare, mais peut Ãªtre utile pour profiling
 * @ingroup BuildInline
 */
#if !defined(NDEBUG) && !defined(NKENTSEU_RELEASE)
#define NKENTSEU_INLINE_DEBUG NKENTSEU_FORCE_INLINE
#else
#define NKENTSEU_INLINE_DEBUG inline
#endif

// ============================================================
// INLINE AVEC PURE/CONST
// ============================================================

/**
 * @defgroup PureConstMacros Macros Pures/Constantes
 * @brief Macros pour fonctions sans effets de bord
 */

/**
 * @brief Fonction pure (pas d'effets de bord, dÃ©pend seulement des args)
 * @note Permet optimisations agressives
 * @ingroup PureConstMacros
 */
#if defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NKENTSEU_PURE __attribute__((pure))
#define NKENTSEU_INLINE_PURE inline __attribute__((pure, always_inline))
#else
#define NKENTSEU_PURE
#define NKENTSEU_INLINE_PURE inline
#endif

/**
 * @brief Fonction const (pas d'effets de bord, ne lit pas mÃ©moire globale)
 * @note Plus restrictif que pure, permet plus d'optimisations
 * @ingroup PureConstMacros
 */
#if defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NKENTSEU_CONST __attribute__((const))
#define NKENTSEU_INLINE_CONST inline __attribute__((const, always_inline))
#else
#define NKENTSEU_CONST
#define NKENTSEU_INLINE_CONST inline
#endif

// ============================================================
// MACROS DE CONVENANCE
// ============================================================

/**
 * @defgroup ConvenienceInline Inlining de Convenance
 * @brief Macros combinant inlining avec autres qualificateurs
 */

/**
 * @brief Inline + constexpr (C++11+)
 * @ingroup ConvenienceInline
 */
#if defined(NKENTSEU_HAS_CONSTEXPR)
#define NKENTSEU_INLINE_CONSTEXPR constexpr inline
#else
#define NKENTSEU_INLINE_CONSTEXPR inline
#endif

/**
 * @brief Force inline + constexpr (C++11+)
 * @ingroup ConvenienceInline
 */
#if defined(NKENTSEU_HAS_CONSTEXPR)
#define NKENTSEU_FORCE_INLINE_CONSTEXPR constexpr NKENTSEU_FORCE_INLINE
#else
#define NKENTSEU_FORCE_INLINE_CONSTEXPR NKENTSEU_FORCE_INLINE
#endif

/**
 * @brief Inline + noexcept (C++11+)
 * @ingroup ConvenienceInline
 */
#if defined(NKENTSEU_HAS_NOEXCEPT)
#define NKENTSEU_INLINE_NOEXCEPT inline noexcept
#else
#define NKENTSEU_INLINE_NOEXCEPT inline
#endif

/**
 * @brief Force inline + noexcept (C++11+)
 * @ingroup ConvenienceInline
 */
#if defined(NKENTSEU_HAS_NOEXCEPT)
#define NKENTSEU_FORCE_INLINE_NOEXCEPT NKENTSEU_FORCE_INLINE noexcept
#else
#define NKENTSEU_FORCE_INLINE_NOEXCEPT NKENTSEU_FORCE_INLINE
#endif

/**
 * @brief Inline + constexpr + noexcept (trio complet C++11+)
 * @ingroup ConvenienceInline
 */
#if defined(NKENTSEU_HAS_CONSTEXPR) && defined(NKENTSEU_HAS_NOEXCEPT)
#define NKENTSEU_INLINE_CONSTEXPR_NOEXCEPT constexpr inline noexcept
#else
#define NKENTSEU_INLINE_CONSTEXPR_NOEXCEPT inline
#endif

// ============================================================
// MACROS SPÃ‰CIFIQUES COMPILATEUR
// ============================================================

/**
 * @defgroup CompilerSpecificMacros Macros SpÃ©cifiques Compilateur
 * @brief Macros pour fonctionnalitÃ©s spÃ©cifiques au compilateur
 */

/**
 * @brief Inline pour fonctions flatten (aplatir rÃ©cursivitÃ©)
 * @note GCC/Clang seulement
 * @ingroup CompilerSpecificMacros
 */
#if defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NKENTSEU_FLATTEN __attribute__((flatten))
#define NKENTSEU_INLINE_FLATTEN inline __attribute__((flatten, always_inline))
#else
#define NKENTSEU_FLATTEN
#define NKENTSEU_INLINE_FLATTEN inline
#endif

/**
 * @brief Fonction toujours instrumentÃ©e pour profiling
 * @ingroup CompilerSpecificMacros
 */
#if defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NKENTSEU_INSTRUMENT __attribute__((no_instrument_function))
#else
#define NKENTSEU_INSTRUMENT
#endif

// ============================================================
// MACROS D'OPTIMISATION MEMOIRE
// ============================================================

/**
 * @defgroup MemoryOptMacros Macros d'Optimisation MÃ©moire
 * @brief Macros pour optimisation de l'utilisation mÃ©moire
 */

/**
 * @brief Fonction qui ne nÃ©cessite pas de stack frame
 * @ingroup MemoryOptMacros
 */
#if defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NKENTSEU_NO_STACK_FRAME __attribute__((naked))
#else
#define NKENTSEU_NO_STACK_FRAME
#endif

/**
 * @brief Fonction qui utilise peu de registres
 * @ingroup MemoryOptMacros
 */
#if defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NKENTSEU_REGISTER_SAFE __attribute__((regparm(3)))
#else
#define NKENTSEU_REGISTER_SAFE
#endif

// ============================================================
// MACROS DE VISIBILITÃ‰
// ============================================================

/**
 * @defgroup VisibilityMacros Macros de VisibilitÃ©
 * @brief Macros pour contrÃ´le de la visibilitÃ© des fonctions
 */

/**
 * @brief Fonction visible uniquement dans ce module
 * @ingroup VisibilityMacros
 */
#if defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NKENTSEU_INTERNAL_VISIBILITY __attribute__((visibility("hidden")))
#else
#define NKENTSEU_INTERNAL_VISIBILITY
#endif

/**
 * @brief Fonction visible partout (par dÃ©faut)
 * @ingroup VisibilityMacros
 */
#if defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NKENTSEU_EXTERNAL_VISIBILITY __attribute__((visibility("default")))
#else
#define NKENTSEU_EXTERNAL_VISIBILITY
#endif

// ============================================================
// MACROS D'OPTIMISATION
// ============================================================

/**
 * @defgroup Optimization Optimisation
 * @brief Macros pour contrÃ´ler l'optimisation et l'inlining
 */

#if defined(_MSC_VER)
/**
 * @brief EmpÃªche l'inlining
 * @ingroup Optimization
 */
#define NKENTSEU_NOINLINE __declspec(noinline)

/**
 * @brief Force l'inlining
 * @ingroup Optimization
 */
#define NKENTSEU_FORCE_INLINE __forceinline

#elif defined(__GNUC__) || defined(__clang__)
#define NKENTSEU_NOINLINE __attribute__((noinline))
#define NKENTSEU_FORCE_INLINE __attribute__((always_inline)) inline

#else
#define NKENTSEU_NOINLINE
#define NKENTSEU_FORCE_INLINE inline
#endif

// ============================================================
// EXEMPLES D'UTILISATION
// ============================================================

#if 0
// Pour Ã©viter les conflits avec les fichiers existants,
// les exemples sont commentÃ©s mais montrent l'usage correct

/**
 * @example Fonction mathÃ©matique simple
 */
NKENTSEU_INLINE_CONSTEXPR_NOEXCEPT int32 NkAdd(int32 a, int32 b) {
    return a + b;
}

/**
 * @example Fonction hot path (boucle principale)
 */
NKENTSEU_HOT_INLINE void NkProcessFrame(float32 deltaTime) {
    // Code critique performance
}

/**
 * @example Fonction cold path (gestion erreurs)
 */
NKENTSEU_COLD_INLINE void NkHandleError(const char* message) {
    // Code rarement exÃ©cutÃ©
}

/**
 * @example Fonction pure (calcul mathÃ©matique)
 */
NKENTSEU_INLINE_PURE float32 NkSquareRoot(float32 x) {
    // Pas d'effets de bord
    return ::sqrt(x);
}

/**
 * @example Fonction const (calcul pur)
 */
NKENTSEU_INLINE_CONST int32 NkAbs(int32 x) {
    return x < 0 ? -x : x;
}
#endif

#endif // NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKINLINE_H_INCLUDED

// ============================================================
// Copyright Â© 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-07
// Creation Date: 2026-02-07
// ============================================================
