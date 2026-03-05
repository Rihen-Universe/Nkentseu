// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\NkMacros.h
// DESCRIPTION: Macros utilitaires générales
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKMACROS_H_INCLUDED
#define NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKMACROS_H_INCLUDED

// ============================================================
// INCLUDES
// ============================================================

#include "NkCompilerDetect.h"
#include "NkVersion.h"
#include "NkTypes.h"

// ============================================================
// MACROS STRINGIFICATION
// ============================================================

/**
 * @defgroup StringificationMacros Macros de Stringification
 * @brief Macros pour convertir des tokens en chaînes
 */

/**
 * @brief Convertit un token en chaîne de caractères
 * @param x Token à convertir
 * @return Chaîne de caractères représentant le token
 * @example NkStringify(Hello) → "Hello"
 * @ingroup StringificationMacros
 */
#define NkStringify(x) NkStringifyHelper(x)
#define NkStringifyHelper(x) #x

/**
 * @brief Concatène deux tokens
 * @param a Premier token
 * @param b Second token
 * @return Token concaténé
 * @example NkConcat(Hello, World) → HelloWorld
 * @ingroup StringificationMacros
 */
#define NkConcat(a, b) NkConcatHelper(a, b)
#define NkConcatHelper(a, b) a##b

/**
 * @brief Concatène trois tokens
 * @param a Premier token
 * @param b Second token
 * @param c Troisième token
 * @return Token concaténé
 * @ingroup StringificationMacros
 */
#define NkConcat3(a, b, c) NkConcat(NkConcat(a, b), c)

/**
 * @brief Concatène quatre tokens
 * @param a Premier token
 * @param b Second token
 * @param c Troisième token
 * @param d Quatrième token
 * @return Token concaténé
 * @ingroup StringificationMacros
 */
#define NkConcat4(a, b, c, d) NkConcat(NkConcat3(a, b, c), d)

// ============================================================
// MACROS TAILLE & COMPTAGE
// ============================================================

/**
 * @defgroup SizeMacros Macros de Taille et Comptage
 * @brief Macros pour calculer tailles et compter arguments
 */

/**
 * @brief Taille d'un tableau statique
 * @param arr Tableau statique
 * @return Nombre d'éléments dans le tableau
 * @warning Ne fonctionne QUE sur tableaux statiques, pas pointeurs
 * @ingroup SizeMacros
 */
#define NkArraySize(arr) (sizeof(arr) / sizeof((arr)[0]))

/**
 * @brief Nombre d'arguments variadiques (max 16)
 * @param ... Arguments variadiques
 * @return Nombre d'arguments passés
 * @ingroup SizeMacros
 */
#define NkVaArgsCount(...) NkVaArgsCountHelper(__VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define NkVaArgsCountHelper(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, N, ...) N

// ============================================================
// MACROS MANIPULATION BITS
// ============================================================

/**
 * @defgroup BitMacros Macros de Manipulation de Bits
 * @brief Macros pour opérations bitwise
 */

/**
 * @brief Crée un masque avec le bit N à 1 (32-bit par défaut)
 * @param x Position du bit (0-based)
 * @return Masque avec le bit x à 1
 * @example NkBit(3) → 0b1000 (8)
 * @ingroup BitMacros
 */
#define NkBit(x) (1U << (x))

/**
 * @brief Crée un masque 64-bit
 * @param x Position du bit (0-based)
 * @return Masque 64-bit avec le bit x à 1
 * @ingroup BitMacros
 */
#define NkBit64(x) (1ULL << (x))

/**
 * @brief Teste si un bit est à 1
 * @param value Valeur à tester
 * @param bit Position du bit (0-based)
 * @return true si le bit est à 1, false sinon
 * @ingroup BitMacros
 */
#define NkBitTest(value, bit) (((value) & NkBit(bit)) != 0)

/**
 * @brief Met un bit à 1
 * @param value Variable à modifier
 * @param bit Position du bit (0-based)
 * @ingroup BitMacros
 */
#define NkBitSet(value, bit) ((value) |= NkBit(bit))

/**
 * @brief Met un bit à 0
 * @param value Variable à modifier
 * @param bit Position du bit (0-based)
 * @ingroup BitMacros
 */
#define NkBitClear(value, bit) ((value) &= ~NkBit(bit))

/**
 * @brief Inverse un bit
 * @param value Variable à modifier
 * @param bit Position du bit (0-based)
 * @ingroup BitMacros
 */
#define NkBitToggle(value, bit) ((value) ^= NkBit(bit))

// ============================================================
// MACROS TAILLE MÉMOIRE
// ============================================================

/**
 * @defgroup MemoryMacros Macros de Taille Mémoire
 * @brief Macros pour conversions de taille mémoire
 */

/**
 * @brief Convertit en kilobytes
 * @param x Nombre de kilobytes
 * @return Taille en octets
 * @ingroup MemoryMacros
 */
#define NkKilobytes(x) ((x) * 1024LL)

/**
 * @brief Convertit en megabytes
 * @param x Nombre de megabytes
 * @return Taille en octets
 * @ingroup MemoryMacros
 */
#define NkMegabytes(x) (NkKilobytes(x) * 1024LL)

/**
 * @brief Convertit en gigabytes
 * @param x Nombre de gigabytes
 * @return Taille en octets
 * @ingroup MemoryMacros
 */
#define NkGigabytes(x) (NkMegabytes(x) * 1024LL)

/**
 * @brief Convertit en terabytes
 * @param x Nombre de terabytes
 * @return Taille en octets
 * @ingroup MemoryMacros
 */
#define NkTerabytes(x) (NkGigabytes(x) * 1024LL)

// ============================================================
// MACROS ALIGNEMENT
// ============================================================

/**
 * @defgroup AlignmentMacros Macros d'Alignement
 * @brief Macros pour alignement mémoire
 */

/**
 * @brief Aligne un pointeur
 * @param ptr Pointeur à aligner
 * @param alignment Alignement (puissance de 2)
 * @return Pointeur aligné
 * @ingroup AlignmentMacros
 */
#define NkAlignPtr(ptr, alignment) ((void *)NkAlignUp((uintptr_t)(ptr), (alignment)))

// ============================================================
// MACROS MIN/MAX/CLAMP
// ============================================================

/**
 * @defgroup MathMacros Macros Mathématiques
 * @brief Macros pour opérations mathématiques simples
 */

/**
 * @brief Retourne le minimum de deux valeurs
 * @param a Première valeur
 * @param b Deuxième valeur
 * @return La plus petite des deux valeurs
 * @warning Évalue les arguments plusieurs fois
 * @ingroup MathMacros
 */
#define NkMin(a, b) (((a) < (b)) ? (a) : (b))

/**
 * @brief Retourne le maximum de deux valeurs
 * @param a Première valeur
 * @param b Deuxième valeur
 * @return La plus grande des deux valeurs
 * @ingroup MathMacros
 */
#define NkMax(a, b) (((a) > (b)) ? (a) : (b))

/**
 * @brief Limite une valeur entre min et max
 * @param value Valeur à clamper
 * @param min Valeur minimale
 * @param max Valeur maximale
 * @return Valeur clampée dans [min, max]
 * @ingroup MathMacros
 */
#define NkClamp(value, min, max) (((value) < (min)) ? (min) : (((value) > (max)) ? (max) : (value)))

/**
 * @brief Retourne la valeur absolue
 * @param x Valeur
 * @return Valeur absolue
 * @ingroup MathMacros
 */
#define NkAbs(x) (((x) < 0) ? -(x) : (x))

// ============================================================
// MACROS SWAP
// ============================================================

/**
 * @defgroup UtilityMacros Macros Utilitaires
 * @brief Macros utilitaires diverses
 */

/**
 * @brief Échange deux valeurs (nécessite temporaire)
 * @param a Première variable
 * @param b Deuxième variable
 * @param type Type des variables
 * @ingroup UtilityMacros
 */
#define NkSwap(a, b, type)                                                                                             \
	do {                                                                                                               \
		type NkConcat(tmp_, __LINE__) = (a);                                                                           \
		(a) = (b);                                                                                                     \
		(b) = NkConcat(tmp_, __LINE__);                                                                                \
	} while (0)

// ============================================================
// MACROS UNUSED
// ============================================================

/**
 * @brief Marque une variable/paramètre comme intentionnellement inutilisé
 * @param x Variable à marquer
 * @note Évite les warnings compilateur
 * @ingroup UtilityMacros
 */
#define NKENTSEU_UNUSED(x) (void)(x)

/**
 * @brief Marque deux variables comme inutilisées
 * @param x Première variable
 * @param y Deuxième variable
 * @ingroup UtilityMacros
 */
#define NKENTSEU_UNUSED2(x, y)                                                                                         \
	NKENTSEU_UNUSED(x);                                                                                                \
	NKENTSEU_UNUSED(y)

/**
 * @brief Marque trois variables comme inutilisées
 * @param x Première variable
 * @param y Deuxième variable
 * @param z Troisième variable
 * @ingroup UtilityMacros
 */
#define NKENTSEU_UNUSED3(x, y, z)                                                                                      \
	NKENTSEU_UNUSED2(x, y);                                                                                            \
	NKENTSEU_UNUSED(z)

/**
 * @brief Marque quatre variables comme inutilisées
 * @param x Première variable
 * @param y Deuxième variable
 * @param z Troisième variable
 * @param w Quatrième variable
 * @ingroup UtilityMacros
 */
#define NKENTSEU_UNUSED4(x, y, z, w)                                                                                   \
	NKENTSEU_UNUSED3(x, y, z);                                                                                         \
	NKENTSEU_UNUSED(w)

// ============================================================
// MACROS OFFSET & CONTAINER_OF
// ============================================================

/**
 * @brief Offset d'un membre dans une structure
 * @param type Type de la structure
 * @param member Nom du membre
 * @return Offset en octets
 * @ingroup UtilityMacros
 */
#if defined(_MSC_VER)
#define NkOffsetOf(type, member) offsetof(type, member)
#else
#define NkOffsetOf(type, member) __builtin_offsetof(type, member)
#endif

/**
 * @brief Récupère le pointeur du conteneur depuis un pointeur membre
 * @param ptr Pointeur vers le membre
 * @param type Type du conteneur
 * @param member Nom du membre dans le conteneur
 * @return Pointeur vers le conteneur
 * @note Technique utilisée dans le kernel Linux
 * @ingroup UtilityMacros
 */
#define NkContainerOf(ptr, type, member) ((type *)((char *)(ptr) - NkOffsetOf(type, member)))

// ============================================================
// MACROS STATIC ARRAY
// ============================================================

/**
 * @brief Déclare un tableau statique avec taille en paramètre fonction
 * @param size Taille du tableau
 * @note C99+ feature
 * @ingroup UtilityMacros
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define NKENTSEU_STATIC_ARRAY(size) static size
#else
#define NKENTSEU_STATIC_ARRAY(size)
#endif

// ============================================================
// MACROS DO-WHILE(0)
// ============================================================

/**
 * @brief Wrapper do-while(0) pour macros multi-statements
 * @note Permet d'utiliser des macros comme des statements
 * @ingroup UtilityMacros
 */
#define NKENTSEU_BLOCK_BEGIN do {
#define NKENTSEU_BLOCK_END                                                                                             \
	}                                                                                                                  \
	while (0)

// ============================================================
// MACROS SCOPE
// ============================================================

/**
 * @brief Exécute du code à la sortie du scope (RAII-like en C)
 * @param code Code à exécuter
 * @example NkDefer({ cleanup(); });
 * @ingroup UtilityMacros
 */
#if defined(__GNUC__) || defined(__clang__)
#define NkDeferConcat(a, b) a##b
#define NkDeferVarname(a, b) NkDeferConcat(a, b)
#define NkDefer(code) auto NkDeferVarname(defer_, __LINE__) = nkentseu::core::NkScopeGuard([&]() { code; })
#else
#define NkDefer(code)
#endif

// ============================================================
// MACROS STATIC_ASSERT
// ============================================================

#ifndef NKENTSEU_STATIC_ASSERT
#if defined(__cplusplus) && (__cplusplus >= 201103L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201103L))
// C++11 et supérieur
#define NKENTSEU_STATIC_ASSERT(condition, message) static_assert(condition, message)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
// C11 et supérieur
#define NKENTSEU_STATIC_ASSERT(condition, message) _Static_assert(condition, message)
#else
// Fallback pour anciens compilateurs
#define NKENTSEU_STATIC_ASSERT(condition, message) typedef char NKENTSEU_STATIC_ASSERT_##__LINE__[(condition) ? 1 : -1]
#endif
#endif

// ============================================================
// MACROS TYPE CHECKING
// ============================================================

/**
 * @brief Vérifie que deux types sont identiques (compile-time)
 * @param a Premier type/variable
 * @param b Deuxième type/variable
 * @return true si types identiques, false sinon
 * @ingroup UtilityMacros
 */
#if defined(__GNUC__) || defined(__clang__)
#define NKENTSEU_SAME_TYPE(a, b) __builtin_types_compatible_p(typeof(a), typeof(b))
#else
#define NKENTSEU_SAME_TYPE(a, b) (0)
#endif

/**
 * @brief Taille d'un type
 * @param type Type à mesurer
 * @return Taille en octets
 * @ingroup UtilityMacros
 */
#define NKENTSEU_SIZEOF_TYPE(type) sizeof(type)

/**
 * @brief Taille d'un membre de structure
 * @param type Type de la structure
 * @param member Nom du membre
 * @return Taille du membre en octets
 * @ingroup UtilityMacros
 */
#define NKENTSEU_SIZEOF_MEMBER(type, member) sizeof(((type *)0)->member)

// ============================================================
// MACROS ARITHMÉTIQUE SÉCURISÉE
// ============================================================

/**
 * @brief Vérifie si une addition déborde
 * @param a Premier opérande
 * @param b Deuxième opérande
 * @param max Valeur maximale
 * @return true si a + b > max, false sinon
 * @ingroup UtilityMacros
 */
#define NKENTSEU_WILL_ADD_OVERFLOW(a, b, max) ((a) > (max) - (b))

/**
 * @brief Vérifie si une multiplication déborde
 * @param a Premier opérande
 * @param b Deuxième opérande
 * @param max Valeur maximale
 * @return true si a * b > max, false sinon
 * @ingroup UtilityMacros
 */
#define NKENTSEU_WILL_MUL_OVERFLOW(a, b, max) ((b) != 0 && (a) > (max) / (b))

// ============================================================
// MACROS DEBUGGING
// ============================================================

/**
 * @defgroup DebugMacros Macros de Débogage
 * @brief Macros pour messages de compilation et annotations
 */

/**
 * @brief Message de compilation (warning custom)
 * @param msg Message à afficher
 * @ingroup DebugMacros
 */
#if defined(_MSC_VER)
#define NKENTSEU_COMPILE_MESSAGE(msg) __pragma(message(msg))
#elif defined(__GNUC__) || defined(__clang__)
#define NKENTSEU_COMPILE_MESSAGE(msg) _Pragma(NkStringify(message msg))
#else
#define NKENTSEU_COMPILE_MESSAGE(msg)
#endif

/**
 * @brief TODO avec message
 * @param msg Message TODO
 * @ingroup DebugMacros
 */
#define NKENTSEU_TODO(msg) NKENTSEU_COMPILE_MESSAGE("TODO: " msg)

/**
 * @brief FIXME avec message
 * @param msg Message FIXME
 * @ingroup DebugMacros
 */
#define NKENTSEU_FIXME(msg) NKENTSEU_COMPILE_MESSAGE("FIXME: " msg)

// ============================================================
// MACROS VERSION
// ============================================================

/**
 * @defgroup VersionMacros Macros de Version
 * @brief Macros pour encodage et décodage de versions
 */

/**
 * @brief Encode une version en entier 32-bit
 * @param major Version majeure
 * @param minor Version mineure
 * @param patch Version patch
 * @return Version encodée (format: 0xMMmmpppp)
 * @example NkVersionEncode(1, 2, 3) → 0x00010203
 * @ingroup VersionMacros
 */
#define NkVersionEncode(major, minor, patch) (((major) << 24) | ((minor) << 16) | (patch))

/**
 * @brief Extrait le major d'une version encodée
 * @param version Version encodée
 * @return Numéro de version majeure
 * @ingroup VersionMacros
 */
#define NkVersionMajor(version) (((version) >> 24) & 0xFF)

/**
 * @brief Extrait le minor d'une version encodée
 * @param version Version encodée
 * @return Numéro de version mineure
 * @ingroup VersionMacros
 */
#define NkVersionMinor(version) (((version) >> 16) & 0xFF)

/**
 * @brief Extrait le patch d'une version encodée
 * @param version Version encodée
 * @return Numéro de version patch
 * @ingroup VersionMacros
 */
#define NkVersionPatch(version) ((version) & 0xFFFF)

// ============================================================
// MACROS FONCTIONNELLES UTILITAIRES
// ============================================================

/**
 * @defgroup FunctionalMacros Macros Fonctionnelles
 * @brief Macros utilitaires qui effectuent des opérations
 */

/**
 * @brief Applique une fonction à chaque argument variadique
 * @param func Fonction à appliquer
 * @param ... Arguments variadiques
 * @ingroup FunctionalMacros
 */
#define NkForEach(func, ...) NkForEachHelper(func, NkVaArgsCount(__VA_ARGS__), __VA_ARGS__)

/**
 * @brief Helper pour NkForEach
 * @internal
 */
#define NkForEachHelper(func, count, ...) NkForEachHelper##count(func, __VA_ARGS__)
#define NkForEachHelper1(func, a) func(a)
#define NkForEachHelper2(func, a, b)                                                                                   \
	func(a);                                                                                                           \
	func(b)
#define NkForEachHelper3(func, a, b, c)                                                                                \
	func(a);                                                                                                           \
	func(b);                                                                                                           \
	func(c)
#define NkForEachHelper4(func, a, b, c, d)                                                                             \
	func(a);                                                                                                           \
	func(b);                                                                                                           \
	func(c);                                                                                                           \
	func(d)

/**
 * @brief Masque une adresse mémoire (pour debug)
 * @param ptr Pointeur à masquer
 * @return Pointeur masqué
 * @ingroup FunctionalMacros
 */
#define NkMaskAddress(ptr) ((void *)((uintptr_t)(ptr) & 0xFFFFF000))

/**
 * @brief Calcule la distance entre deux pointeurs en octets
 * @param ptr1 Premier pointeur
 * @param ptr2 Second pointeur
 * @return Distance en octets (ptr2 - ptr1)
 * @ingroup FunctionalMacros
 */
#define NkPointerDistance(ptr1, ptr2) ((intptr_t)((const char *)(ptr2) - (const char *)(ptr1)))

// ============================================================
// MACROS DE CONVERSION
// ============================================================

/**
 * @brief Convertit degrés en radians
 * @param degrees Valeur en degrés
 * @return Valeur en radians
 * @ingroup FunctionalMacros
 */
#define NkDegreesToRadians(degrees) ((degrees) * 3.14159265358979323846 / 180.0)

/**
 * @brief Convertit radians en degrés
 * @param radians Valeur en radians
 * @return Valeur en degrés
 * @ingroup FunctionalMacros
 */
#define NkRadiansToDegrees(radians) ((radians) * 180.0 / 3.14159265358979323846)

// ============================================================
// MACROS D'OPTIMISATION
// ============================================================

/**
 * @defgroup OptimizationMacros Macros d'Optimisation
 * @brief Macros pour hints d'optimisation du compilateur
 */

/**
 * @brief Marque une branche comme probablement prise
 * @param x Condition
 * @return Condition (avec hint pour le compilateur)
 * @ingroup OptimizationMacros
 */
#if defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NKENTSEU_LIKELY(x) __builtin_expect(!!(x), 1)
#define NKENTSEU_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define NKENTSEU_LIKELY(x) (x)
#define NKENTSEU_UNLIKELY(x) (x)
#endif

/**
 * @brief Marque un point de code comme inaccessible
 * @note Utilisé pour optimisations du compilateur
 * @ingroup OptimizationMacros
 */
#if defined(NKENTSEU_COMPILER_MSVC)
#define NKENTSEU_UNREACHABLE() __assume(0)
#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NKENTSEU_UNREACHABLE() __builtin_unreachable()
#else
#define NKENTSEU_UNREACHABLE()
#endif

#endif // NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKMACROS_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================