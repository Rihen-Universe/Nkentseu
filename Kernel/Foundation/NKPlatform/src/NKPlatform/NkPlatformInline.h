// =============================================================================
// Core/Nkentseu/Platform/NkPlatformInline.h
// Gestion des spécificateurs d'inline et d'optimisation.
//
// Design :
//  - Définit NKENTSEU_INLINE pour l'inlining multi-compilateurs.
//  - Fournit NKENTSEU_FORCE_INLINE pour forcer l'inlining agressif.
//  - Fournit NKENTSEU_NO_INLINE pour empêcher l'inlining quand nécessaire.
//  - Compatible C99/C++ et tous les compilateurs majeurs.
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================

#pragma once

#ifndef NKENTSEU_PLATFORM_NKPLATFORMINLINE_H
#define NKENTSEU_PLATFORM_NKPLATFORMINLINE_H

// -------------------------------------------------------------------------
// SECTION 1 : DÉTECTION DU COMPILATEUR
// -------------------------------------------------------------------------
// Identification du compilateur pour adapter les spécificateurs d'inline.
// Cette détection est nécessaire car chaque compilateur a sa propre syntaxe.

// Détection de MSVC (Visual Studio)
#if defined(_MSC_VER)
/** @brief Définie, sans valeur, quand le compilateur est MSVC (_MSC_VER). */
#define NKENTSEU_INLINE_COMPILER_MSVC
/** @brief Nom du compilateur vu par NkPlatformInline.h, en chaîne ("MSVC", "Clang", "GCC"...). */
#define NKENTSEU_INLINE_COMPILER_NAME "MSVC"

// Détection de Clang (y compris Apple Clang)
#elif defined(__clang__)
/** @brief Définie, sans valeur, quand le compilateur est Clang (et pas MSVC). */
#define NKENTSEU_INLINE_COMPILER_CLANG
#define NKENTSEU_INLINE_COMPILER_NAME "Clang"

// Détection de GCC
#elif defined(__GNUC__)
/** @brief Définie, sans valeur, quand le compilateur est GCC (ni MSVC ni Clang). */
#define NKENTSEU_INLINE_COMPILER_GCC
#define NKENTSEU_INLINE_COMPILER_NAME "GCC"

// Détection de ICC (Intel Compiler)
#elif defined(__INTEL_COMPILER)
/** @brief Définie, sans valeur, quand le compilateur est celui d'Intel. */
#define NKENTSEU_INLINE_COMPILER_INTEL
#define NKENTSEU_INLINE_COMPILER_NAME "Intel"

// Détection de compilateurs embarqués (ARMCC, IAR, etc.)
#elif defined(__ARMCC_VERSION)
/** @brief Définie, sans valeur, quand le compilateur est ARM Compiler (armcc). */
#define NKENTSEU_INLINE_COMPILER_ARMCC
#define NKENTSEU_INLINE_COMPILER_NAME "ARMCC"
#elif defined(__IAR_SYSTEMS_ICC__)
/** @brief Définie, sans valeur, quand le compilateur est IAR. */
#define NKENTSEU_INLINE_COMPILER_IAR
#define NKENTSEU_INLINE_COMPILER_NAME "IAR"

// Fallback pour compilateurs inconnus
#else
/** @brief Définie, sans valeur, quand aucun compilateur connu n'est reconnu. */
#define NKENTSEU_INLINE_COMPILER_UNKNOWN
#define NKENTSEU_INLINE_COMPILER_NAME "Unknown"
#endif

// -------------------------------------------------------------------------
// SECTION 2 : MACRO NKENTSEU_INLINE (INLINE STANDARD)
// -------------------------------------------------------------------------
// Définit le spécificateur inline portable pour toutes les plateformes.
// En C++, 'inline' est standard. En C99+, on utilise également 'inline'.
// Pour les vieux compilateurs C, on fournit un fallback.

// C++ : le mot-clé inline est standard depuis C++98
#if defined(__cplusplus)
/** @brief Le mot-clé inline du langage (inline en C++ et en C99, __inline ou __inline__ ailleurs). */
#define NKENTSEU_INLINE inline

// C99 et supérieur : inline est supporté
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#define NKENTSEU_INLINE inline

// MSVC en mode C : utilise __inline
#elif defined(NKENTSEU_INLINE_COMPILER_MSVC)
#define NKENTSEU_INLINE __inline

// GCC/Clang en mode C ancien : utilise __inline__
#elif defined(NKENTSEU_INLINE_COMPILER_GCC) || defined(NKENTSEU_INLINE_COMPILER_CLANG)
#define NKENTSEU_INLINE __inline__

// Fallback ultime : définition vide (risque de multiples définitions)
#else
#define NKENTSEU_INLINE
#endif

// -------------------------------------------------------------------------
// SECTION 3 : MACRO NKENTSEU_FORCE_INLINE (INLINING AGRESSIF)
// -------------------------------------------------------------------------
// Force le compilateur à inliner la fonction quand c'est techniquement possible.
// Utile pour les fonctions critiques en performance (hot paths).
// Attention : peut augmenter la taille du binaire si mal utilisé.

// MSVC : utilise __forceinline
#if defined(NKENTSEU_INLINE_COMPILER_MSVC)
/** @brief Demande au compilateur d'insérer la fonction à chaque appel (__forceinline, always_inline). */
#define NKENTSEU_FORCE_INLINE __forceinline

// GCC/Clang : utilise l'attribut always_inline avec inline
#elif defined(NKENTSEU_INLINE_COMPILER_GCC) || defined(NKENTSEU_INLINE_COMPILER_CLANG)
#define NKENTSEU_FORCE_INLINE inline __attribute__((always_inline))

// Intel Compiler : supporte à la fois __forceinline et l'attribut
#elif defined(NKENTSEU_INLINE_COMPILER_INTEL)
#define NKENTSEU_FORCE_INLINE __forceinline

// Compilateurs embarqués : tentatives de fallback
#elif defined(NKENTSEU_INLINE_COMPILER_ARMCC)
#define NKENTSEU_FORCE_INLINE __forceinline
#elif defined(NKENTSEU_INLINE_COMPILER_IAR)
#define NKENTSEU_FORCE_INLINE _Pragma("inline=forced")

// Fallback : inline standard sans garantie de forçage
#else
#define NKENTSEU_FORCE_INLINE NKENTSEU_INLINE
#endif

// -------------------------------------------------------------------------
// SECTION 4 : MACRO NKENTSEU_NO_INLINE (EMPÊCHER L'INLINING)
// -------------------------------------------------------------------------
// Indique au compilateur de ne jamais inliner la fonction.
// Utile pour :
//  - Réduire la taille du code dans les fonctions rarement appelées
//  - Améliorer le temps de compilation
//  - Préserver des points d'arrêt pour le debugging
//  - Éviter l'explosion de code dans les fonctions récursives

// MSVC : utilise __declspec(noinline)
#if defined(NKENTSEU_INLINE_COMPILER_MSVC)
/** @brief Interdit au compilateur d'insérer la fonction (__declspec(noinline), noinline). */
#define NKENTSEU_NO_INLINE __declspec(noinline)

// GCC/Clang/Intel : utilise l'attribut noinline
#elif defined(NKENTSEU_INLINE_COMPILER_GCC) || defined(NKENTSEU_INLINE_COMPILER_CLANG) ||                              \
    defined(NKENTSEU_INLINE_COMPILER_INTEL)
#define NKENTSEU_NO_INLINE __attribute__((noinline))

// ARMCC/IAR : attributs spécifiques
#elif defined(NKENTSEU_INLINE_COMPILER_ARMCC)
#define NKENTSEU_NO_INLINE __attribute__((noinline))
#elif defined(NKENTSEU_INLINE_COMPILER_IAR)
#define NKENTSEU_NO_INLINE _Pragma("inline=never")

// Fallback : définition vide (le compilateur décide)
#else
#define NKENTSEU_NO_INLINE
#endif

// -------------------------------------------------------------------------
// SECTION 5 : MACROS D'OPTIMISATION AVANCÉES
// -------------------------------------------------------------------------

/**
 * @brief Indique qu'une fonction n'a pas d'effets de bord
 * @def NKENTSEU_PURE_FUNCTION
 * @ingroup PlatformInline
 *
 * Permet au compilateur d'optimiser agressivement les appels.
 * La fonction ne doit pas modifier l'état global ni dépendre de variables volatiles.
 *
 * @example
 * @code
 * NKENTSEU_PURE_FUNCTION int Square(int x) { return x * x; }
 * @endcode
 */
#if defined(NKENTSEU_INLINE_COMPILER_GCC) || defined(NKENTSEU_INLINE_COMPILER_CLANG)
#define NKENTSEU_PURE_FUNCTION __attribute__((pure))
#else
#define NKENTSEU_PURE_FUNCTION
#endif

/**
 * @brief Indique qu'une fonction dépend uniquement de ses paramètres
 * @def NKENTSEU_CONST_FUNCTION
 * @ingroup PlatformInline
 *
 * Plus restrictif que PURE : la fonction ne lit aucune variable globale.
 * Permet des optimisations supplémentaires comme la CSE (Common Subexpression Elimination).
 *
 * @example
 * @code
 * NKENTSEU_CONST_FUNCTION int Add(int a, int b) { return a + b; }
 * @endcode
 */
#if defined(NKENTSEU_INLINE_COMPILER_GCC) || defined(NKENTSEU_INLINE_COMPILER_CLANG)
#define NKENTSEU_CONST_FUNCTION __attribute__((const))
#else
#define NKENTSEU_CONST_FUNCTION
#endif

/**
 * @brief Indique que le pointeur de retour n'alias aucun autre pointeur
 * @def NKENTSEU_RESTRICT_RETURN
 * @ingroup PlatformInline
 *
 * Utile pour les fonctions d'allocation ou de factory.
 * Aide le compilateur à optimiser l'accès mémoire.
 */
#if defined(__cplusplus) && __cplusplus >= 201103L
#define NKENTSEU_RESTRICT_RETURN __restrict__
#elif defined(NKENTSEU_INLINE_COMPILER_MSVC)
#define NKENTSEU_RESTRICT_RETURN __restrict
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#define NKENTSEU_RESTRICT_RETURN restrict
#else
#define NKENTSEU_RESTRICT_RETURN
#endif

/**
 * @brief Indique que les paramètres pointeurs ne s'aliasent pas entre eux
 * @def NKENTSEU_RESTRICT_PARAM
 * @param ptr Nom du paramètre pointeur
 * @ingroup PlatformInline
 *
 * À utiliser sur les paramètres de fonction pour les optimisations vectorielles.
 *
 * @example
 * @code
 * void VectorAdd(
 *     float* NKENTSEU_RESTRICT_PARAM(out),
 *     const float* NKENTSEU_RESTRICT_PARAM(a),
 *     const float* NKENTSEU_RESTRICT_PARAM(b),
 *     size_t count
 * );
 * @endcode
 */
#if defined(NKENTSEU_INLINE_COMPILER_MSVC)
#define NKENTSEU_RESTRICT_PARAM(ptr) __restrict ptr
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#define NKENTSEU_RESTRICT_PARAM(ptr) restrict ptr
#else
#define NKENTSEU_RESTRICT_PARAM(ptr) ptr
#endif

/**
 * @brief Indique que la fonction est susceptible de ne pas retourner
 * @def NKENTSEU_NORETURN
 * @ingroup PlatformInline
 *
 * Permet au compilateur d'optimiser le flux de contrôle après l'appel.
 * Pour les fonctions qui terminent le programme ou lancent une exception.
 *
 * @example
 * @code
 * NKENTSEU_NORETURN void FatalError(const char* message);
 * @endcode
 */
#if defined(__cplusplus) && __cplusplus >= 201103L
#define NKENTSEU_NORETURN [[noreturn]]
#elif defined(NKENTSEU_INLINE_COMPILER_MSVC)
#define NKENTSEU_NORETURN __declspec(noreturn)
#elif defined(NKENTSEU_INLINE_COMPILER_GCC) || defined(NKENTSEU_INLINE_COMPILER_CLANG)
#define NKENTSEU_NORETURN __attribute__((noreturn))
#else
#define NKENTSEU_NORETURN
#endif

/**
 * @brief Indique que la fonction est improbable d'être appelée
 * @def NKENTSEU_COLD_FUNCTION
 * @ingroup PlatformInline
 *
 * Aide le compilateur à optimiser le layout du code en plaçant
 * les fonctions "froides" (error handlers, init rare) loin du hot path.
 */
#if defined(NKENTSEU_INLINE_COMPILER_GCC) || defined(NKENTSEU_INLINE_COMPILER_CLANG)
#define NKENTSEU_COLD_FUNCTION __attribute__((cold))
#else
#define NKENTSEU_COLD_FUNCTION
#endif

/**
 * @brief Indique que la fonction est très probable d'être appelée
 * @def NKENTSEU_HOT_FUNCTION
 * @ingroup PlatformInline
 *
 * L'inverse de COLD : place la fonction dans une section optimisée pour le cache.
 * À réserver aux fonctions critiques du hot path.
 */
#if defined(NKENTSEU_INLINE_COMPILER_GCC) || defined(NKENTSEU_INLINE_COMPILER_CLANG)
#define NKENTSEU_HOT_FUNCTION __attribute__((hot))
#else
#define NKENTSEU_HOT_FUNCTION
#endif

// -------------------------------------------------------------------------
// SECTION 6 : MACROS DE PRÉFÉRENCE DE BRANCHEMENT
// -------------------------------------------------------------------------
// Aide le compilateur à optimiser les prédictions de branche.
// Ces macros sont des hints : le compilateur peut les ignorer.

/**
 * @brief Indique qu'une condition est probablement vraie
 * @def NKENTSEU_LIKELY
 * @param condition Expression booléenne à évaluer
 * @ingroup PlatformInline
 *
 * Optimise le layout du code pour que le chemin "vrai" soit contigu.
 * Réduit les défauts de prédiction de branche sur CPU modernes.
 *
 * @example
 * @code
 * if (NKENTSEU_LIKELY(ptr != nullptr)) {
 *     // Hot path : code fréquemment exécuté
 *     ProcessData(ptr);
 * } else {
 *     // Cold path : gestion d'erreur rare
 *     HandleNullPointer();
 * }
 * @endcode
 */
#if defined(NKENTSEU_INLINE_COMPILER_GCC) || defined(NKENTSEU_INLINE_COMPILER_CLANG)
#define NKENTSEU_LIKELY(condition) (__builtin_expect(!!(condition), 1))
#else
#define NKENTSEU_LIKELY(condition) (condition)
#endif

/**
 * @brief Indique qu'une condition est probablement fausse
 * @def NKENTSEU_UNLIKELY
 * @param condition Expression booléenne à évaluer
 * @ingroup PlatformInline
 *
 * Optimise le layout pour que le chemin "faux" soit le hot path.
 * Idéal pour les checks d'erreur ou les assertions en release.
 *
 * @example
 * @code
 * if (NKENTSEU_UNLIKELY(errorCode != 0)) {
 *     LogError(errorCode);
 *     return false;
 * }
 * // Hot path continue ici sans saut
 * @endcode
 */
#if defined(NKENTSEU_INLINE_COMPILER_GCC) || defined(NKENTSEU_INLINE_COMPILER_CLANG)
#define NKENTSEU_UNLIKELY(condition) (__builtin_expect(!!(condition), 0))
#else
#define NKENTSEU_UNLIKELY(condition) (condition)
#endif

// -------------------------------------------------------------------------
// SECTION 7 : MACROS DE COMBINAISON AVEC NKPLATFORMEXPORT
// -------------------------------------------------------------------------
// Combinaisons pratiques pour exporter des fonctions inline/force_inline.
// À utiliser dans les en-têtes publics des bibliothèques partagées.

/**
 * @brief Exporte une fonction inline
 * @def NKENTSEU_API_INLINE
 * @ingroup PlatformInline
 *
 * Combinaison de NKENTSEU_PLATFORM_API et NKENTSEU_INLINE.
 * Pour les fonctions inline qui doivent être visibles depuis une DLL.
 */
#define NKENTSEU_API_INLINE NKENTSEU_INLINE

/**
 * @brief Exporte une fonction force_inline
 * @def NKENTSEU_API_FORCE_INLINE
 * @ingroup PlatformInline
 *
 * Combinaison de NKENTSEU_PLATFORM_API et NKENTSEU_FORCE_INLINE.
 * Pour les fonctions critiques en performance dans une API publique.
 * Attention : l'inlining forcé peut causer des problèmes de linkage dans certains cas.
 */
#define NKENTSEU_API_FORCE_INLINE NKENTSEU_PLATFORM_API NKENTSEU_FORCE_INLINE

/**
 * @brief Exporte une fonction no_inline
 * @def NKENTSEU_API_NO_INLINE
 * @ingroup PlatformInline
 *
 * Combinaison de NKENTSEU_PLATFORM_API et NKENTSEU_NO_INLINE.
 * Pour les fonctions publiques qu'on veut garder hors ligne pour le debugging.
 */
#define NKENTSEU_API_NO_INLINE NKENTSEU_PLATFORM_API NKENTSEU_NO_INLINE

#endif // NKENTSEU_PLATFORM_NKPLATFORMINLINE_H

// =============================================================================
// Exemples : les exemples COMPILÉS et exécutés sont dans Docs/exemples/Foundation/NKPlatform/. Ceux qui étaient
// ici en commentaire sont gardés tels quels dans Docs/archives/NKPlatform/exemples_des_entetes.md (à convertir).

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - All Rights Reserved (see LICENSE)
// ============================================================