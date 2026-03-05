// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\NkConfig.h
// DESCRIPTION: Configuration globale du framework
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKCONFIG_H_INCLUDED
#define NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKCONFIG_H_INCLUDED

#include "NkPlatform.h"
#include "NkCompilerDetect.h"
#include "NkVersion.h"
#include "NkCGXDetect.h"
#include "NkMacros.h"

// ============================================================
// CONFIGURATION BUILD
// ============================================================

/**
 * @defgroup BuildConfig Configuration de Build
 * @brief Macros définissant le mode de compilation
 */

/**
 * @brief Mode Debug (défini par le compilateur ou build system)
 * @ingroup BuildConfig
 */
#if !defined(NDEBUG) && !defined(NKENTSEU_DEBUG) && !defined(NKENTSEU_RELEASE)
#define NKENTSEU_DEBUG
#endif

/**
 * @brief Mode Release
 * @ingroup BuildConfig
 */
#if defined(NDEBUG) && !defined(NKENTSEU_RELEASE)
#define NKENTSEU_RELEASE
#endif

/**
 * @brief Mode Distribution (release optimisé final)
 * @ingroup BuildConfig
 */
#if defined(NKENTSEU_DIST) || defined(NKENTSEU_DISTRIBUTION)
#define NKENTSEU_RELEASE
#define NKENTSEU_DISTRIBUTION
#endif

// ============================================================
// CONFIGURATION ASSERTIONS
// ============================================================

/**
 * @defgroup AssertConfig Configuration des Assertions
 * @brief Macros pour le contrôle des assertions
 */

/**
 * @brief Activer les assertions en Debug
 * @ingroup AssertConfig
 */
#if defined(NKENTSEU_DEBUG)
#define NKENTSEU_ENABLE_ASSERTS
#endif

/**
 * @brief Forcer activation/désactivation assertions
 * @ingroup AssertConfig
 */
#if defined(NKENTSEU_FORCE_ENABLE_ASSERTS)
#undef NKENTSEU_ENABLE_ASSERTS
#define NKENTSEU_ENABLE_ASSERTS
#endif

#if defined(NKENTSEU_FORCE_DISABLE_ASSERTS)
#undef NKENTSEU_ENABLE_ASSERTS
#endif

// ============================================================
// CONFIGURATION MEMORY TRACKING
// ============================================================

/**
 * @defgroup MemoryConfig Configuration Mémoire
 * @brief Macros pour le tracking et la détection mémoire
 */

/**
 * @brief Activer le tracking mémoire en Debug
 * @ingroup MemoryConfig
 */
#if defined(NKENTSEU_DEBUG) && !defined(NKENTSEU_DISABLE_MEMORY_TRACKING)
#define NKENTSEU_ENABLE_MEMORY_TRACKING
#endif

/**
 * @brief Activer détection de leaks mémoire
 * @ingroup MemoryConfig
 */
#if defined(NKENTSEU_ENABLE_MEMORY_TRACKING)
#define NKENTSEU_ENABLE_LEAK_DETECTION
#endif

/**
 * @brief Activer statistiques mémoire
 * @ingroup MemoryConfig
 */
#if defined(NKENTSEU_ENABLE_MEMORY_TRACKING)
#define NKENTSEU_ENABLE_MEMORY_STATS
#endif

// ============================================================
// CONFIGURATION LOGGING
// ============================================================

/**
 * @defgroup LogConfig Configuration Logging
 * @brief Macros pour la configuration du système de logs
 */

/**
 * @brief Niveau de logging par défaut
 * @values 0=OFF, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=TRACE
 * @ingroup LogConfig
 */
#if defined(NKENTSEU_DEBUG)
#define NKENTSEU_LOG_LEVEL 4 // DEBUG
#else
#define NKENTSEU_LOG_LEVEL 2 // WARN
#endif

/**
 * @brief Activer logging dans fichier
 * @ingroup LogConfig
 */
#if defined(NKENTSEU_DEBUG)
#define NKENTSEU_ENABLE_FILE_LOGGING
#endif

// ============================================================
// CONFIGURATION PERFORMANCE
// ============================================================

/**
 * @defgroup PerfConfig Configuration Performance
 * @brief Macros pour profiling et instrumentation
 */

/**
 * @brief Activer profiling en Debug/Profile
 * @ingroup PerfConfig
 */
#if defined(NKENTSEU_DEBUG) || defined(NKENTSEU_PROFILE)
#define NKENTSEU_ENABLE_PROFILING
#endif

/**
 * @brief Activer instrumentation code (très verbeux)
 * @ingroup PerfConfig
 */
#if defined(NKENTSEU_ENABLE_INSTRUMENTATION)
#define NKENTSEU_ENABLE_PROFILING
#endif

// ============================================================
// CONFIGURATION OPTIMISATION
// ============================================================

/**
 * @defgroup OptimConfig Configuration Optimisation
 * @brief Macros pour optimisations SIMD et multi-threading
 */

/**
 * @brief Utiliser SIMD (SSE, AVX, NEON) quand disponible
 * @ingroup OptimConfig
 */
#if !defined(NKENTSEU_DISABLE_SIMD)
#if defined(NKENTSEU_CPU_HAS_SSE2) || defined(NKENTSEU_CPU_HAS_NEON)
#define NKENTSEU_ENABLE_SIMD
#endif
#endif

/**
 * @brief Utiliser multi-threading
 * @ingroup OptimConfig
 */
#if !defined(NKENTSEU_DISABLE_THREADING)
#define NKENTSEU_ENABLE_THREADING
#endif

/**
 * @brief Nombre de threads par défaut (0 = auto-detect)
 * @ingroup OptimConfig
 */
#define NKENTSEU_DEFAULT_THREAD_COUNT 0

// ============================================================
// CONFIGURATION MEMORY ALLOCATOR
// ============================================================

/**
 * @defgroup AllocConfig Configuration Allocateur
 * @brief Macros pour configuration allocateur mémoire
 */

/**
 * @brief Alignement mémoire par défaut (en bytes)
 * @ingroup AllocConfig
 */
#if defined(NKENTSEU_ENABLE_SIMD)
#define NKENTSEU_DEFAULT_ALIGNMENT 16
#else
#define NKENTSEU_DEFAULT_ALIGNMENT 8
#endif

/**
 * @brief Taille page mémoire par défaut
 * @ingroup AllocConfig
 */
#define NKENTSEU_DEFAULT_PAGE_SIZE (4 * 1024) // 4 KB

/**
 * @brief Allocateur par défaut
 * @values NKENTSEU_ALLOCATOR_MALLOC, NKENTSEU_ALLOCATOR_TLSF, NKENTSEU_ALLOCATOR_POOL
 * @ingroup AllocConfig
 */
#ifndef NKENTSEU_DEFAULT_ALLOCATOR
#define NKENTSEU_DEFAULT_ALLOCATOR NKENTSEU_ALLOCATOR_MALLOC
#endif

// ============================================================
// CONFIGURATION STRINGS
// ============================================================

/**
 * @defgroup StringConfig Configuration Strings
 * @brief Macros pour système de strings
 */

/**
 * @brief Taille buffer string par défaut
 * @ingroup StringConfig
 */
#define NKENTSEU_STRING_DEFAULT_CAPACITY 64

/**
 * @brief Activer Small String Optimization (SSO)
 * @ingroup StringConfig
 */
#define NKENTSEU_ENABLE_STRING_SSO

/**
 * @brief Taille SSO buffer
 * @ingroup StringConfig
 */
#define NKENTSEU_STRING_SSO_SIZE 23

// ============================================================
// CONFIGURATION CONTAINERS
// ============================================================

/**
 * @defgroup ContainerConfig Configuration Conteneurs
 * @brief Macros pour configuration des conteneurs
 */

/**
 * @brief Capacité initiale Vector par défaut
 * @ingroup ContainerConfig
 */
#define NKENTSEU_VECTOR_DEFAULT_CAPACITY 8

/**
 * @brief Facteur de croissance Vector (1.5 ou 2.0)
 * @ingroup ContainerConfig
 */
#define NKENTSEU_VECTOR_GROWTH_FACTOR 1.5f

/**
 * @brief Capacité initiale HashMap par défaut
 * @ingroup ContainerConfig
 */
#define NKENTSEU_HASHMAP_DEFAULT_CAPACITY 16

/**
 * @brief Load factor max HashMap (0.75 = 75%)
 * @ingroup ContainerConfig
 */
#define NKENTSEU_HASHMAP_MAX_LOAD_FACTOR 0.75f

// ============================================================
// CONFIGURATION MATH
// ============================================================

/**
 * @defgroup MathConfig Configuration Mathématiques
 * @brief Macros pour configuration système mathématique
 */

/**
 * @brief Utiliser double précision pour math (sinon float)
 * @ingroup MathConfig
 */
#if defined(NKENTSEU_MATH_USE_DOUBLE)
#define NKENTSEU_MATH_PRECISION_DOUBLE
#else
#define NKENTSEU_MATH_PRECISION_FLOAT
#endif

/**
 * @brief Epsilon pour comparaisons flottantes
 * @ingroup MathConfig
 */
#define NKENTSEU_MATH_EPSILON 1e-6f
#define NKENTSEU_MATH_EPSILON_DOUBLE 1e-12

/**
 * @brief Constante PI
 * @ingroup MathConfig
 */
#define NKENTSEU_PI 3.14159265358979323846f
#define NKENTSEU_PI_DOUBLE 3.14159265358979323846

// ============================================================
// CONFIGURATION REFLECTION
// ============================================================

/**
 * @defgroup ReflectConfig Configuration Réflexion
 * @brief Macros pour système de réflexion
 */

/**
 * @brief Activer système de réflexion
 * @ingroup ReflectConfig
 */
#if defined(NKENTSEU_ENABLE_REFLECTION)
#if !defined(NKENTSEU_HAS_RTTI)
#error "Reflection requires RTTI enabled"
#endif
#endif

// ============================================================
// CONFIGURATION EXCEPTIONS
// ============================================================

/**
 * @defgroup ExceptionConfig Configuration Exceptions
 * @brief Macros pour gestion des exceptions
 */

/**
 * @brief Utiliser exceptions C++ (si supporté)
 * @ingroup ExceptionConfig
 */
#if defined(NKENTSEU_HAS_EXCEPTIONS) && !defined(NKENTSEU_DISABLE_EXCEPTIONS)
#define NKENTSEU_ENABLE_EXCEPTIONS
#endif

// ============================================================
// CONFIGURATION PLATFORM-SPECIFIC
// ============================================================

/**
 * @defgroup PlatformConfig Configuration Plateforme
 * @brief Macros spécifiques à la plateforme
 */

#if defined(NKENTSEU_PLATFORM_WINDOWS)
/**
 * @brief Utiliser Win32 API
 * @ingroup PlatformConfig
 */
#define NKENTSEU_USE_WIN32_API

/**
 * @brief Activer support Unicode Windows (UNICODE/WCHAR)
 * @ingroup PlatformConfig
 */
#if !defined(NKENTSEU_DISABLE_UNICODE)
#define NKENTSEU_ENABLE_UNICODE
#endif
#endif

#if defined(NKENTSEU_PLATFORM_POSIX)
/**
 * @brief Utiliser POSIX API
 * @ingroup PlatformConfig
 */
#define NKENTSEU_USE_POSIX_API
#endif

// ============================================================
// CONFIGURATION GRAPHICS (si NKGraphics lié)
// ============================================================

/**
 * @defgroup GraphicsConfig Configuration Graphique
 * @brief Macros pour backend graphique
 */

/**
 * @brief Backend graphique par défaut
 * @ingroup GraphicsConfig
 */
#if defined(NKENTSEU_PLATFORM_WINDOWS)
#define NKENTSEU_GRAPHICS_BACKEND_DEFAULT NKENTSEU_GRAPHICS_D3D11
#elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
#define NKENTSEU_GRAPHICS_BACKEND_DEFAULT NKENTSEU_GRAPHICS_METAL
#elif defined(NKENTSEU_PLATFORM_ANDROID)
#define NKENTSEU_GRAPHICS_BACKEND_DEFAULT NKENTSEU_GRAPHICS_GLES3
#else
#define NKENTSEU_GRAPHICS_BACKEND_DEFAULT NKENTSEU_GRAPHICS_OPENGL
#endif

// ============================================================
// VALIDATION CONFIGURATION
// ============================================================

// Vérifier conflits Debug/Release
#if defined(NKENTSEU_DEBUG) && defined(NKENTSEU_RELEASE)
#error "Cannot define both NKENTSEU_DEBUG and NKENTSEU_RELEASE"
#endif

// Vérifier SIMD supporté si activé
#if defined(NKENTSEU_ENABLE_SIMD)
#if !defined(NKENTSEU_CPU_HAS_SSE2) && !defined(NKENTSEU_CPU_HAS_NEON)
#warning "SIMD enabled but no SIMD instruction set detected"
#undef NKENTSEU_ENABLE_SIMD
#endif
#endif

// ============================================================
// MACROS UTILITAIRES CONFIGURATION
// ============================================================

/**
 * @defgroup ConfigUtils Utilitaires Configuration
 * @brief Macros utilitaires pour la configuration
 */

/**
 * @brief Vérifie si en mode Debug
 * @ingroup ConfigUtils
 */
#if defined(NKENTSEU_DEBUG)
#define NKENTSEU_IS_DEBUG 1
#else
#define NKENTSEU_IS_DEBUG 0
#endif

/**
 * @brief Vérifie si en mode Release
 * @ingroup ConfigUtils
 */
#if defined(NKENTSEU_RELEASE)
#define NKENTSEU_IS_RELEASE 1
#else
#define NKENTSEU_IS_RELEASE 0
#endif

/**
 * @brief Code exécuté seulement en Debug
 * @ingroup ConfigUtils
 */
#if defined(NKENTSEU_DEBUG)
#define NKENTSEU_DEBUG_ONLY(code) code
#define NKENTSEU_RELEASE_ONLY(code)
#else
#define NKENTSEU_DEBUG_ONLY(code)
#define NKENTSEU_RELEASE_ONLY(code) code
#endif


#endif // NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKCONFIG_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-08
// Creation Date: 2026-02-08
// ============================================================