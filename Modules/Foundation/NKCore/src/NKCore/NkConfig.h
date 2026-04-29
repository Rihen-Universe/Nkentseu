// =============================================================================
// NKCore/NkConfig.h
// Configuration globale du framework NKCore.
//
// Design :
//  - Définition centralisée des modes de build (Debug/Release/Distribution)
//  - Configuration conditionnelle des fonctionnalités (assertions, logging, profiling)
//  - Constantes pour allocateurs, conteneurs, mathématiques, strings
//  - Macros utilitaires pour code conditionnel debug/release
//  - Validation compile-time des conflits de configuration
//  - Intégration avec NkPlatform.h, NkCompilerDetect.h, NkVersion.h
//
// Intégration :
//  - Utilise NKCore/NkPlatform.h pour détection plateforme
//  - Utilise NKPlatform/NkCompilerDetect.h pour détection compilateur
//  - Utilise NKCore/NkVersion.h pour versionnement du framework
//  - Utilise NKCore/NkCGXDetect.h pour configuration graphique
//  - Utilise NKCore/NkMacros.h pour utilitaires de préprocesseur
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CORE_NKCONFIG_H_INCLUDED
#define NKENTSEU_CORE_NKCONFIG_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules de base requis pour la configuration.

    #include "NkPlatform.h"
    #include "NKPlatform/NkCompilerDetect.h"
    #include "NkVersion.h"
    #include "NkCGXDetect.h"
    #include "NkMacros.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION DES MODES DE BUILD
    // -------------------------------------------------------------------------
    // Définition et normalisation des macros Debug/Release/Distribution.

    /**
     * @defgroup BuildConfig Configuration de Build
     * @brief Macros définissant le mode de compilation et ses implications
     * @ingroup CoreConfiguration
     *
     * Ces macros contrôlent le comportement global du framework selon
     * l'environnement de compilation. Elles sont définies automatiquement
     * ou peuvent être forcées via le build system.
     *
     * @note
     *   - NKENTSEU_DEBUG : mode développement avec assertions et logging verbeux
     *   - NKENTSEU_RELEASE : mode production avec optimisations
     *   - NKENTSEU_DISTRIBUTION : mode release final avec stripping maximal
     *
     * @warning
     *   NKENTSEU_DEBUG et NKENTSEU_RELEASE sont mutuellement exclusifs.
     *   Définir les deux simultanément cause une erreur de compilation.
     *
     * @example
     * @code
     * // Dans CMakeLists.txt ou fichier de build :
     *
     * // Mode debug
     * add_definitions(-DNKENTSEU_DEBUG)
     *
     * // Mode release
     * add_definitions(-DNDEBUG -DNKENTSEU_RELEASE)
     *
     * // Mode distribution (release optimisé final)
     * add_definitions(-DNDEBUG -DNKENTSEU_DISTRIBUTION)
     * @endcode
     */
    /** @{ */

    /**
     * @brief Mode Debug : développement avec vérifications complètes
     * @def NKENTSEU_DEBUG
     * @ingroup BuildConfig
     *
     * Défini automatiquement si ni NDEBUG ni NKENTSEU_RELEASE ne sont définis.
     * Active les assertions, logging debug, memory tracking, et désactive
     * certaines optimisations pour faciliter le débogage.
     *
     * @note
     *   Peut être forcé via -DNKENTSEU_DEBUG dans les flags de compilation.
     */
    #if !defined(NDEBUG) && !defined(NKENTSEU_DEBUG) && !defined(NKENTSEU_RELEASE)
        #define NKENTSEU_DEBUG
    #endif

    /**
     * @brief Mode Release : production avec optimisations
     * @def NKENTSEU_RELEASE
     * @ingroup BuildConfig
     *
     * Défini automatiquement si NDEBUG est défini (convention standard C/C++).
     * Désactive les assertions, réduit le logging, active les optimisations.
     *
     * @note
     *   Peut être forcé via -DNKENTSEU_RELEASE même sans NDEBUG.
     */
    #if defined(NDEBUG) && !defined(NKENTSEU_RELEASE)
        #define NKENTSEU_RELEASE
    #endif

    /**
     * @brief Mode Distribution : release final avec stripping maximal
     * @def NKENTSEU_DISTRIBUTION
     * @ingroup BuildConfig
     *
     * Défini manuellement pour les builds destinés à la distribution finale.
     * Implique NKENTSEU_RELEASE et active des optimisations supplémentaires :
     *   - Suppression des symboles de debug
     *   - Désactivation des checks de sécurité non-essentiels
     *   - Optimisations agressives (LTO, PGO si supporté)
     *
     * @note
     *   À utiliser uniquement pour les builds officiels publiés aux utilisateurs.
     */
    #if defined(NKENTSEU_DIST) || defined(NKENTSEU_DISTRIBUTION)
        #define NKENTSEU_RELEASE
        #define NKENTSEU_DISTRIBUTION
    #endif

    /** @} */ // End of BuildConfig

    // -------------------------------------------------------------------------
    // SECTION 3 : CONFIGURATION DES ASSERTIONS
    // -------------------------------------------------------------------------
    // Contrôle de l'activation/désactivation du système d'assertions.

    /**
     * @defgroup AssertConfig Configuration des Assertions
     * @brief Macros pour le contrôle fin du système d'assertions
     * @ingroup CoreConfiguration
     *
     * Le système d'assertions permet de valider les invariants et préconditions
     * en développement. Ces macros contrôlent son activation selon le mode de build.
     *
     * @note
     *   - NKENTSEU_ENABLE_ASSERTS : active les macros NK_ASSERT() et NK_VERIFY()
     *   - En release, les assertions sont désactivées par défaut (zéro overhead)
     *   - Peut être forcé via NKENTSEU_FORCE_ENABLE_ASSERTS / NKENTSEU_FORCE_DISABLE_ASSERTS
     *
     * @example
     * @code
     * // Forcer les assertions même en release (pour tests de robustesse)
     * add_definitions(-DNKENTSEU_FORCE_ENABLE_ASSERTS)
     *
     * // Désactiver totalement les assertions (même en debug)
     * add_definitions(-DNKENTSEU_FORCE_DISABLE_ASSERTS)
     * @endcode
     */
    /** @{ */

    /**
     * @brief Activer les assertions en mode Debug
     * @def NKENTSEU_ENABLE_ASSERTS
     * @ingroup AssertConfig
     *
     * Défini automatiquement si NKENTSEU_DEBUG est actif.
     * Permet l'utilisation des macros NK_ASSERT() et NK_VERIFY() avec effets.
     */
    #if defined(NKENTSEU_DEBUG)
        #define NKENTSEU_ENABLE_ASSERTS
    #endif

    /**
     * @brief Forcer l'activation des assertions indépendamment du mode
     * @def NKENTSEU_FORCE_ENABLE_ASSERTS
     * @ingroup AssertConfig
     * @note Redéfinit NKENTSEU_ENABLE_ASSERTS même si non défini par défaut
     */
    #if defined(NKENTSEU_FORCE_ENABLE_ASSERTS)
        #undef NKENTSEU_ENABLE_ASSERTS
        #define NKENTSEU_ENABLE_ASSERTS
    #endif

    /**
     * @brief Forcer la désactivation des assertions indépendamment du mode
     * @def NKENTSEU_FORCE_DISABLE_ASSERTS
     * @ingroup AssertConfig
     * @note Undef NKENTSEU_ENABLE_ASSERTS même en mode debug
     */
    #if defined(NKENTSEU_FORCE_DISABLE_ASSERTS)
        #undef NKENTSEU_ENABLE_ASSERTS
    #endif

    /** @} */ // End of AssertConfig

    // -------------------------------------------------------------------------
    // SECTION 4 : CONFIGURATION DU TRACKING MÉMOIRE
    // -------------------------------------------------------------------------
    // Contrôle des fonctionnalités de diagnostic mémoire.

    /**
     * @defgroup MemoryConfig Configuration Mémoire
     * @brief Macros pour le tracking, détection de leaks et statistiques mémoire
     * @ingroup CoreConfiguration
     *
     * Ces options permettent d'activer des outils de diagnostic mémoire
     * pour identifier les fuites, les accès invalides, et analyser l'usage.
     *
     * @note
     *   - NKENTSEU_ENABLE_MEMORY_TRACKING : intercepte alloc/free pour logging
     *   - NKENTSEU_ENABLE_LEAK_DETECTION : rapport des allocations non-libérées à l'exit
     *   - NKENTSEU_ENABLE_MEMORY_STATS : collecte de statistiques d'usage (pics, moyennes)
     *
     * @warning
     *   Le tracking mémoire a un overhead significatif (10-30%) : à utiliser
     *   uniquement en debug ou pour des sessions de profiling ciblées.
     *
     * @example
     * @code
     * // Activer le tracking mémoire en debug
     * #define NKENTSEU_DEBUG
     * #include "NKCore/NkConfig.h"  // NKENTSEU_ENABLE_MEMORY_TRACKING sera défini
     *
     * // Désactiver le tracking même en debug (pour performance)
     * #define NKENTSEU_DEBUG
     * #define NKENTSEU_DISABLE_MEMORY_TRACKING
     * #include "NKCore/NkConfig.h"
     * @endcode
     */
    /** @{ */

    /**
     * @brief Activer le tracking mémoire en mode Debug
     * @def NKENTSEU_ENABLE_MEMORY_TRACKING
     * @ingroup MemoryConfig
     * @note Défini automatiquement si NKENTSEU_DEBUG et non désactivé explicitement
     */
    #if defined(NKENTSEU_DEBUG) && !defined(NKENTSEU_DISABLE_MEMORY_TRACKING)
        #define NKENTSEU_ENABLE_MEMORY_TRACKING
    #endif

    /**
     * @brief Activer la détection de fuites mémoire
     * @def NKENTSEU_ENABLE_LEAK_DETECTION
     * @ingroup MemoryConfig
     * @note Requiert NKENTSEU_ENABLE_MEMORY_TRACKING
     */
    #if defined(NKENTSEU_ENABLE_MEMORY_TRACKING)
        #define NKENTSEU_ENABLE_LEAK_DETECTION
    #endif

    /**
     * @brief Activer les statistiques d'usage mémoire
     * @def NKENTSEU_ENABLE_MEMORY_STATS
     * @ingroup MemoryConfig
     * @note Requiert NKENTSEU_ENABLE_MEMORY_TRACKING
     */
    #if defined(NKENTSEU_ENABLE_MEMORY_TRACKING)
        #define NKENTSEU_ENABLE_MEMORY_STATS
    #endif

    /** @} */ // End of MemoryConfig

    // -------------------------------------------------------------------------
    // SECTION 5 : CONFIGURATION DU SYSTÈME DE LOGGING
    // -------------------------------------------------------------------------
    // Niveau de verbosité et destinations des logs.

    /**
     * @defgroup LogConfig Configuration Logging
     * @brief Macros pour la configuration du système de logs
     * @ingroup CoreConfiguration
     *
     * Contrôle le niveau de détail des logs et leurs destinations
     * (console, fichier, réseau) selon l'environnement d'exécution.
     *
     * @note
     *   - NKENTSEU_LOG_LEVEL : 0=OFF, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=TRACE
     *   - NKENTSEU_ENABLE_FILE_LOGGING : écriture des logs dans un fichier en plus de la console
     *
     * @example
     * @code
     * // Configuration typique debug : logs verbeux avec fichier
     * #define NKENTSEU_DEBUG
     * // NKENTSEU_LOG_LEVEL sera 4 (DEBUG) par défaut
     * // NKENTSEU_ENABLE_FILE_LOGGING sera défini
     *
     * // Configuration release minimaliste : seulement warnings+
     * #define NKENTSEU_RELEASE
     * // NKENTSEU_LOG_LEVEL sera 2 (WARN) par défaut
     * // NKENTSEU_ENABLE_FILE_LOGGING ne sera pas défini
     *
     * // Forcer un niveau de log personnalisé
     * #define NKENTSEU_LOG_LEVEL 3  // INFO même en release
     * #include "NKCore/NkConfig.h"
     * @endcode
     */
    /** @{ */

    /**
     * @brief Niveau de logging par défaut selon le mode de build
     * @def NKENTSEU_LOG_LEVEL
     * @ingroup LogConfig
     * @values
     *   - 0 = OFF : aucun log
     *   - 1 = ERROR : seulement les erreurs critiques
     *   - 2 = WARN : erreurs + avertissements (défaut release)
     *   - 3 = INFO : + informations opérationnelles
     *   - 4 = DEBUG : + détails de débogage (défaut debug)
     *   - 5 = TRACE : + traces d'exécution très verbeuses
     *
     * @note
     *   Les logs de niveau supérieur au NKENTSEU_LOG_LEVEL sont compilés
     *   mais filtrés à l'exécution. Pour un zéro overhead total, utiliser
     *   des macros conditionnelles #if NKENTSEU_LOG_LEVEL >= N autour du code de log.
     */
    #if defined(NKENTSEU_DEBUG)
        #define NKENTSEU_LOG_LEVEL 4  // DEBUG
    #else
        #define NKENTSEU_LOG_LEVEL 2  // WARN
    #endif

    /**
     * @brief Activer l'écriture des logs dans un fichier
     * @def NKENTSEU_ENABLE_FILE_LOGGING
     * @ingroup LogConfig
     * @note Défini automatiquement en debug pour persistance des diagnostics
     */
    #if defined(NKENTSEU_DEBUG)
        #define NKENTSEU_ENABLE_FILE_LOGGING
    #endif

    /** @} */ // End of LogConfig

    // -------------------------------------------------------------------------
    // SECTION 6 : CONFIGURATION PERFORMANCE ET PROFILING
    // -------------------------------------------------------------------------
    // Outils d'analyse et d'optimisation des performances.

    /**
     * @defgroup PerfConfig Configuration Performance
     * @brief Macros pour profiling, instrumentation et analyse de performances
     * @ingroup CoreConfiguration
     *
     * Active les outils d'analyse de performance pour identifier
     * les goulots d'étranglement et optimiser le code critique.
     *
     * @note
     *   - NKENTSEU_ENABLE_PROFILING : active les macros NKENTSEU_PROFILE_SCOPE()
     *   - NKENTSEU_ENABLE_INSTRUMENTATION : logging très verbeux de l'exécution (déconseillé en production)
     *
     * @warning
     *   L'instrumentation complète peut ralentir l'exécution de 10x ou plus.
     *   À utiliser uniquement pour des sessions de débogage ciblé.
     *
     * @example
     * @code
     * // Profiling basique en debug
     * #define NKENTSEU_DEBUG
     * #include "NKCore/NkConfig.h"  // NKENTSEU_ENABLE_PROFILING sera défini
     *
     * // Instrumentation complète pour analyse détaillée
     * #define NKENTSEU_DEBUG
     * #define NKENTSEU_ENABLE_INSTRUMENTATION
     * #include "NKCore/NkConfig.h"  // Activera aussi NKENTSEU_ENABLE_PROFILING
     *
     * // Usage dans le code
     * void ExpensiveFunction() {
     *     NKENTSEU_PROFILE_SCOPE("ExpensiveFunction");  // Timing automatique
     *     // ... code à profiler ...
     * }
     * @endcode
     */
    /** @{ */

    /**
     * @brief Activer le profiling en modes Debug ou Profile
     * @def NKENTSEU_ENABLE_PROFILING
     * @ingroup PerfConfig
     * @note Défini si NKENTSEU_DEBUG ou NKENTSEU_PROFILE est actif
     */
    #if defined(NKENTSEU_DEBUG) || defined(NKENTSEU_PROFILE)
        #define NKENTSEU_ENABLE_PROFILING
    #endif

    /**
     * @brief Activer l'instrumentation code complète (très verbeux)
     * @def NKENTSEU_ENABLE_INSTRUMENTATION
     * @ingroup PerfConfig
     * @note Active automatiquement NKENTSEU_ENABLE_PROFILING
     */
    #if defined(NKENTSEU_ENABLE_INSTRUMENTATION)
        #define NKENTSEU_ENABLE_PROFILING
    #endif

    /** @} */ // End of PerfConfig

    // -------------------------------------------------------------------------
    // SECTION 7 : CONFIGURATION OPTIMISATIONS (SIMD, THREADING)
    // -------------------------------------------------------------------------
    // Activation des optimisations matérielles et parallélisme.

    /**
     * @defgroup OptimConfig Configuration Optimisation
     * @brief Macros pour optimisations SIMD et support multi-threading
     * @ingroup CoreConfiguration
     *
     * Contrôle l'utilisation des extensions matérielles (SIMD) et
     * des fonctionnalités de parallélisme pour maximiser les performances.
     *
     * @note
     *   - NKENTSEU_ENABLE_SIMD : utilise SSE2/AVX/NEON si disponible
     *   - NKENTSEU_ENABLE_THREADING : active le support multi-thread (par défaut)
     *   - NKENTSEU_DEFAULT_THREAD_COUNT : 0 = auto-détection, N = nombre fixe
     *
     * @warning
     *   SIMD nécessite un support CPU détecté à la compilation. Si activé
     *   sans support détecté, un avertissement est émis et la fonctionnalité
     *   est désactivée automatiquement (voir validation en fin de fichier).
     *
     * @example
     * @code
     * // Désactiver SIMD pour compatibilité maximale (ex: builds universels)
     * #define NKENTSEU_DISABLE_SIMD
     * #include "NKCore/NkConfig.h"
     *
     * // Forcer un nombre fixe de threads (ex: embarqué avec CPU connu)
     * #undef NKENTSEU_DEFAULT_THREAD_COUNT
     * #define NKENTSEU_DEFAULT_THREAD_COUNT 4
     * #include "NKCore/NkConfig.h"
     *
     * // Désactiver totalement le threading (ex: environnement single-thread contraint)
     * #define NKENTSEU_DISABLE_THREADING
     * #include "NKCore/NkConfig.h"  // NKENTSEU_ENABLE_THREADING ne sera pas défini
     * @endcode
     */
    /** @{ */

    /**
     * @brief Activer les optimisations SIMD (SSE, AVX, NEON) si disponibles
     * @def NKENTSEU_ENABLE_SIMD
     * @ingroup OptimConfig
     * @note Nécessite NKENTSEU_CPU_HAS_SSE2 ou NKENTSEU_CPU_HAS_NEON détecté
     */
    #if !defined(NKENTSEU_DISABLE_SIMD)
        #if defined(NKENTSEU_CPU_HAS_SSE2) || defined(NKENTSEU_CPU_HAS_NEON)
            #define NKENTSEU_ENABLE_SIMD
        #endif
    #endif

    /**
     * @brief Activer le support multi-threading
     * @def NKENTSEU_ENABLE_THREADING
     * @ingroup OptimConfig
     * @note Activé par défaut sauf si NKENTSEU_DISABLE_THREADING est défini
     */
    #if !defined(NKENTSEU_DISABLE_THREADING)
        #define NKENTSEU_ENABLE_THREADING
    #endif

    /**
     * @brief Nombre de threads par défaut pour les pools et parallélisme
     * @def NKENTSEU_DEFAULT_THREAD_COUNT
     * @ingroup OptimConfig
     * @value 0 = auto-détection via std::thread::hardware_concurrency() ou équivalent plateforme
     * @note Peut être redéfini avant inclusion de ce fichier pour forcer une valeur
     */
    #ifndef NKENTSEU_DEFAULT_THREAD_COUNT
        #define NKENTSEU_DEFAULT_THREAD_COUNT 0
    #endif

    /** @} */ // End of OptimConfig

    // -------------------------------------------------------------------------
    // SECTION 8 : CONFIGURATION DE L'ALLOCATEUR MÉMOIRE
    // -------------------------------------------------------------------------
    // Paramètres pour le système d'allocation dynamique.

    /**
     * @defgroup AllocConfig Configuration Allocateur Mémoire
     * @brief Macros pour configuration de l'allocateur mémoire
     * @ingroup CoreConfiguration
     *
     * Définit les paramètres par défaut pour l'allocation dynamique :
     * alignement, taille de page, stratégie d'allocateur.
     *
     * @note
     *   - NKENTSEU_DEFAULT_ALIGNMENT : 16 bytes si SIMD activé, 8 sinon
     *   - NKENTSEU_DEFAULT_PAGE_SIZE : 4 KB (standard page mémoire)
     *   - NKENTSEU_DEFAULT_ALLOCATOR : stratégie par défaut (malloc, TLSF, pool)
     *
     * @example
     * @code
     * // Utiliser un allocateur personnalisé (TLSF pour fragmentation réduite)
     * #undef NKENTSEU_DEFAULT_ALLOCATOR
     * #define NKENTSEU_DEFAULT_ALLOCATOR NKENTSEU_ALLOCATOR_TLSF
     * #include "NKCore/NkConfig.h"
     *
     * // Augmenter l'alignement pour compatibilité AVX-512 (64 bytes)
     * #undef NKENTSEU_DEFAULT_ALIGNMENT
     * #define NKENTSEU_DEFAULT_ALIGNMENT 64
     * #include "NKCore/NkConfig.h"
     * @endcode
     */
    /** @{ */

    /**
     * @brief Alignement mémoire par défaut pour les allocations
     * @def NKENTSEU_DEFAULT_ALIGNMENT
     * @ingroup AllocConfig
     * @value 16 si SIMD activé (compatibilité SSE/AVX), 8 sinon
     * @note Peut être redéfini avant inclusion pour besoins spécifiques
     */
    #if !defined(NKENTSEU_DEFAULT_ALIGNMENT)
        #if defined(NKENTSEU_ENABLE_SIMD)
            #define NKENTSEU_DEFAULT_ALIGNMENT 16
        #else
            #define NKENTSEU_DEFAULT_ALIGNMENT 8
        #endif
    #endif

    /**
     * @brief Taille de page mémoire par défaut
     * @def NKENTSEU_DEFAULT_PAGE_SIZE
     * @ingroup AllocConfig
     * @value 4096 bytes (4 KB, standard sur la plupart des OS)
     * @note Peut être ajusté pour des plateformes embarquées avec pages différentes
     */
    #ifndef NKENTSEU_DEFAULT_PAGE_SIZE
        #define NKENTSEU_DEFAULT_PAGE_SIZE (4 * 1024)  // 4 KB
    #endif

    /**
     * @brief Stratégie d'allocateur par défaut
     * @def NKENTSEU_DEFAULT_ALLOCATOR
     * @ingroup AllocConfig
     * @values
     *   - NKENTSEU_ALLOCATOR_MALLOC : utilise malloc/free standard (défaut)
     *   - NKENTSEU_ALLOCATOR_TLSF : Two-Level Segregated Fit (faible fragmentation)
     *   - NKENTSEU_ALLOCATOR_POOL : allocateurs par pool pour objets fréquents
     * @note Peut être redéfini avant inclusion pour changer de stratégie globale
     */
    #ifndef NKENTSEU_DEFAULT_ALLOCATOR
        #define NKENTSEU_DEFAULT_ALLOCATOR NKENTSEU_ALLOCATOR_MALLOC
    #endif

    /** @} */ // End of AllocConfig

    // -------------------------------------------------------------------------
    // SECTION 9 : CONFIGURATION DU SYSTÈME DE STRINGS
    // -------------------------------------------------------------------------
    // Paramètres pour la gestion des chaînes de caractères.

    /**
     * @defgroup StringConfig Configuration Strings
     * @brief Macros pour configuration du système de chaînes de caractères
     * @ingroup CoreConfiguration
     *
     * Contrôle le comportement et les optimisations du type nk_string
     * et des utilitaires associés.
     *
     * @note
     *   - NKENTSEU_STRING_DEFAULT_CAPACITY : capacité initiale des strings dynamiques
     *   - NKENTSEU_ENABLE_STRING_SSO : active Small String Optimization (évite allocation heap pour petites strings)
     *   - NKENTSEU_STRING_SSO_SIZE : taille max du buffer inline pour SSO (23 chars typique)
     *
     * @example
     * @code
     * // Désactiver SSO pour réduire la taille des objets string (embarqué mémoire contrainte)
     * #undef NKENTSEU_ENABLE_STRING_SSO
     * #include "NKCore/NkConfig.h"
     *
     * // Augmenter le buffer SSO pour éviter les allocations sur strings courantes
     * #undef NKENTSEU_STRING_SSO_SIZE
     * #define NKENTSEU_STRING_SSO_SIZE 31  // 31 chars + null terminator = 32 bytes alignés
     * #include "NKCore/NkConfig.h"
     * @endcode
     */
    /** @{ */

    /**
     * @brief Capacité initiale par défaut pour les strings dynamiques
     * @def NKENTSEU_STRING_DEFAULT_CAPACITY
     * @ingroup StringConfig
     * @value 64 characters
     * @note Ajustable selon l'usage typique des strings dans l'application
     */
    #ifndef NKENTSEU_STRING_DEFAULT_CAPACITY
        #define NKENTSEU_STRING_DEFAULT_CAPACITY 64
    #endif

    /**
     * @brief Activer Small String Optimization (SSO)
     * @def NKENTSEU_ENABLE_STRING_SSO
     * @ingroup StringConfig
     * @note Évite l'allocation heap pour les strings <= NKENTSEU_STRING_SSO_SIZE
     */
    #ifndef NKENTSEU_DISABLE_STRING_SSO
        #define NKENTSEU_ENABLE_STRING_SSO
    #endif

    /**
     * @brief Taille du buffer inline pour Small String Optimization
     * @def NKENTSEU_STRING_SSO_SIZE
     * @ingroup StringConfig
     * @value 23 characters (typique pour alignement 32 bytes avec overhead)
     * @note Doit être <= NKENTSEU_STRING_DEFAULT_CAPACITY
     */
    #ifndef NKENTSEU_STRING_SSO_SIZE
        #define NKENTSEU_STRING_SSO_SIZE 23
    #endif

    /** @} */ // End of StringConfig

    // -------------------------------------------------------------------------
    // SECTION 10 : CONFIGURATION DES CONTENEURS
    // -------------------------------------------------------------------------
    // Paramètres pour les conteneurs génériques (Vector, HashMap, etc.).

    /**
     * @defgroup ContainerConfig Configuration Conteneurs
     * @brief Macros pour configuration des conteneurs génériques
     * @ingroup CoreConfiguration
     *
     * Définit les paramètres par défaut pour les conteneurs de la STL-like
     * du framework : capacité initiale, facteur de croissance, load factor.
     *
     * @note
     *   - NKENTSEU_VECTOR_DEFAULT_CAPACITY : capacité initiale des vectors
     *   - NKENTSEU_VECTOR_GROWTH_FACTOR : multiplicateur lors du resize (1.5 ou 2.0)
     *   - NKENTSEU_HASHMAP_DEFAULT_CAPACITY : capacité initiale des hash maps
     *   - NKENTSEU_HASHMAP_MAX_LOAD_FACTOR : seuil de rehash (0.75 = 75% rempli)
     *
     * @example
     * @code
     * // Optimiser pour beaucoup de petits vectors (réduire allocation initiale)
     * #undef NKENTSEU_VECTOR_DEFAULT_CAPACITY
     * #define NKENTSEU_VECTOR_DEFAULT_CAPACITY 4
     * #include "NKCore/NkConfig.h"
     *
     * // Réduire la fréquence de rehash pour HashMap au prix de plus de mémoire
     * #undef NKENTSEU_HASHMAP_MAX_LOAD_FACTOR
     * #define NKENTSEU_HASHMAP_MAX_LOAD_FACTOR 0.5f  // Rehash à 50% au lieu de 75%
     * #include "NKCore/NkConfig.h"
     * @endcode
     */
    /** @{ */

    /**
     * @brief Capacité initiale par défaut pour les vectors
     * @def NKENTSEU_VECTOR_DEFAULT_CAPACITY
     * @ingroup ContainerConfig
     * @value 8 éléments
     * @note Compromis entre allocation initiale et fréquence de resize
     */
    #ifndef NKENTSEU_VECTOR_DEFAULT_CAPACITY
        #define NKENTSEU_VECTOR_DEFAULT_CAPACITY 8
    #endif

    /**
     * @brief Facteur de croissance lors du resize d'un vector
     * @def NKENTSEU_VECTOR_GROWTH_FACTOR
     * @ingroup ContainerConfig
     * @value 1.5f (compromis mémoire/performance) ou 2.0f (moins de reallocs, plus de mémoire gaspillée)
     * @note Doit être > 1.0 pour garantir la terminaison des resize successifs
     */
    #ifndef NKENTSEU_VECTOR_GROWTH_FACTOR
        #define NKENTSEU_VECTOR_GROWTH_FACTOR 1.5f
    #endif

    /**
     * @brief Capacité initiale par défaut pour les hash maps
     * @def NKENTSEU_HASHMAP_DEFAULT_CAPACITY
     * @ingroup ContainerConfig
     * @value 16 buckets (puissance de 2 pour hashing efficace)
     */
    #ifndef NKENTSEU_HASHMAP_DEFAULT_CAPACITY
        #define NKENTSEU_HASHMAP_DEFAULT_CAPACITY 16
    #endif

    /**
     * @brief Load factor maximum avant rehash d'une hash map
     * @def NKENTSEU_HASHMAP_MAX_LOAD_FACTOR
     * @ingroup ContainerConfig
     * @value 0.75f (75% de remplissage avant doublement de capacité)
     * @note Valeur typique : 0.5-0.9 selon compromis performance/mémoire
     */
    #ifndef NKENTSEU_HASHMAP_MAX_LOAD_FACTOR
        #define NKENTSEU_HASHMAP_MAX_LOAD_FACTOR 0.75f
    #endif

    /** @} */ // End of ContainerConfig

    // -------------------------------------------------------------------------
    // SECTION 11 : CONFIGURATION MATHÉMATIQUE
    // -------------------------------------------------------------------------
    // Précision numérique et constantes mathématiques.

    /**
     * @defgroup MathConfig Configuration Mathématique
     * @brief Macros pour configuration du système mathématique
     * @ingroup CoreConfiguration
     *
     * Contrôle la précision numérique (float vs double) et définit
     * les constantes et tolérances pour les calculs géométriques/numériques.
     *
     * @note
     *   - NKENTSEU_MATH_PRECISION_DOUBLE : utilise double au lieu de float (plus précis, plus lent)
     *   - NKENTSEU_MATH_EPSILON : tolérance pour comparaisons flottantes (float)
     *   - NKENTSEU_MATH_EPSILON_DOUBLE : tolérance pour comparaisons doubles
     *   - NKENTSEU_PI / NKENTSEU_PI_DOUBLE : constantes π avec précision adaptée
     *
     * @example
     * @code
     * // Activer la double précision pour simulations numériques exigeantes
     * #define NKENTSEU_MATH_USE_DOUBLE
     * #include "NKCore/NkConfig.h"  // Définira NKENTSEU_MATH_PRECISION_DOUBLE
     *
     * // Ajuster l'epsilon pour tolérance plus stricte (CAD, physique haute précision)
     * #undef NKENTSEU_MATH_EPSILON
     * #define NKENTSEU_MATH_EPSILON 1e-8f  // Au lieu de 1e-6f par défaut
     * #include "NKCore/NkConfig.h"
     *
     * // Usage dans le code
     * bool FloatEqual(nk_float32 a, nk_float32 b) {
     *     return nkentseu::NkAbs(a - b) < NKENTSEU_MATH_EPSILON;
     * }
     * @endcode
     */
    /** @{ */

    /**
     * @brief Utiliser double précision pour les calculs mathématiques
     * @def NKENTSEU_MATH_PRECISION_DOUBLE
     * @ingroup MathConfig
     * @note Défini si NKENTSEU_MATH_USE_DOUBLE est défini avant inclusion
     */
    #if defined(NKENTSEU_MATH_USE_DOUBLE)
        #define NKENTSEU_MATH_PRECISION_DOUBLE
    #else
        #define NKENTSEU_MATH_PRECISION_FLOAT
    #endif

    /**
     * @brief Epsilon pour comparaisons flottantes (précision simple)
     * @def NKENTSEU_MATH_EPSILON
     * @ingroup MathConfig
     * @value 1e-6f (tolérance typique pour float 32-bit)
     * @note Ajuster selon la précision requise par l'application
     */
    #ifndef NKENTSEU_MATH_EPSILON
        #define NKENTSEU_MATH_EPSILON 1e-6f
    #endif

    /**
     * @brief Epsilon pour comparaisons flottantes (précision double)
     * @def NKENTSEU_MATH_EPSILON_DOUBLE
     * @ingroup MathConfig
     * @value 1e-12 (tolérance typique pour double 64-bit)
     */
    #ifndef NKENTSEU_MATH_EPSILON_DOUBLE
        #define NKENTSEU_MATH_EPSILON_DOUBLE 1e-12
    #endif

    /**
     * @brief Constante π (Pi) en précision simple
     * @def NKENTSEU_PI
     * @ingroup MathConfig
     * @value 3.14159265358979323846f
     */
    #ifndef NKENTSEU_PI
        #define NKENTSEU_PI 3.14159265358979323846f
    #endif

    /**
     * @brief Constante π (Pi) en précision double
     * @def NKENTSEU_PI_DOUBLE
     * @ingroup MathConfig
     * @value 3.14159265358979323846
     */
    #ifndef NKENTSEU_PI_DOUBLE
        #define NKENTSEU_PI_DOUBLE 3.14159265358979323846
    #endif

    /** @} */ // End of MathConfig

    // -------------------------------------------------------------------------
    // SECTION 12 : CONFIGURATION DU SYSTÈME DE RÉFLEXION
    // -------------------------------------------------------------------------
    // Options pour la métaprogrammation et introspection runtime.

    /**
     * @defgroup ReflectConfig Configuration Réflexion
     * @brief Macros pour système de réflexion et métadonnées runtime
     * @ingroup CoreConfiguration
     *
     * Active le système de réflexion pour l'introspection des types,
     * la sérialisation automatique, et les bindings script.
     *
     * @note
     *   - NKENTSEU_ENABLE_REFLECTION : active le système de réflexion complet
     *   - Requiert RTTI (Run-Time Type Information) activé dans le compilateur
     *
     * @warning
     *   Si NKENTSEU_ENABLE_REFLECTION est défini sans RTTI, une erreur
     *   de compilation est générée pour éviter un comportement indéfini.
     *
     * @example
     * @code
     * // Activer la réflexion (nécessite RTTI dans les flags compilateur)
     * // GCC/Clang : -frtti
     * // MSVC : /GR (activé par défaut)
     * #define NKENTSEU_ENABLE_REFLECTION
     * #include "NKCore/NkConfig.h"  // Vérifiera NKENTSEU_HAS_RTTI
     *
     * // Désactiver pour réduire la taille binaire si pas utilisé
     * #define NKENTSEU_DISABLE_REFLECTION
     * #include "NKCore/NkConfig.h"  // NKENTSEU_ENABLE_REFLECTION ne sera pas défini
     * @endcode
     */
    /** @{ */

    /**
     * @brief Activer le système de réflexion runtime
     * @def NKENTSEU_ENABLE_REFLECTION
     * @ingroup ReflectConfig
     * @note Requiert NKENTSEU_HAS_RTTI défini par NkCompilerDetect.h
     */
    #if defined(NKENTSEU_ENABLE_REFLECTION)
        #if !defined(NKENTSEU_HAS_RTTI) || !NKENTSEU_HAS_RTTI
            #error "Reflection requires RTTI enabled (compile with -frtti or /GR)"
        #endif
    #endif

    /** @} */ // End of ReflectConfig

    // -------------------------------------------------------------------------
    // SECTION 13 : CONFIGURATION DES EXCEPTIONS C++
    // -------------------------------------------------------------------------
    // Gestion des erreurs via mécanisme d'exceptions.

    /**
     * @defgroup ExceptionConfig Configuration Exceptions
     * @brief Macros pour gestion des erreurs via exceptions C++
     * @ingroup CoreConfiguration
     *
     * Contrôle l'utilisation du mécanisme d'exceptions C++ pour
     * la gestion des erreurs, avec fallback vers codes de retour si désactivé.
     *
     * @note
     *   - NKENTSEU_ENABLE_EXCEPTIONS : active throw/catch/try
     *   - Requiert NKENTSEU_HAS_EXCEPTIONS défini par NkCompilerDetect.h
     *   - Peut être désactivé via NKENTSEU_DISABLE_EXCEPTIONS même si supporté
     *
     * @warning
     *   Désactiver les exceptions dans un code qui en utilise causera
     *   des erreurs de liaison ou un comportement indéfini. Vérifier
     *   la cohérence dans tout le projet.
     *
     * @example
     * @code
     * // Désactiver les exceptions pour code embarqué ou performance critique
     * #define NKENTSEU_DISABLE_EXCEPTIONS
     * #include "NKCore/NkConfig.h"  // NKENTSEU_ENABLE_EXCEPTIONS ne sera pas défini
     *
     * // Code compatible avec/sans exceptions via macro conditionnelle
     * nkentseu::Result Initialize() {
     *     #if defined(NKENTSEU_ENABLE_EXCEPTIONS)
     *         try {
     *             RiskyOperation();
     *             return nkentseu::Result::Success;
     *         } catch (const nkentseu::Exception& e) {
     *             return nkentseu::Result::Error(e.what());
     *         }
     *     #else
     *         // Fallback sans exceptions : codes de retour
     *         nkentseu::Result res = RiskyOperation_NoExcept();
     *         if (!res.IsSuccess()) {
     *             LogError(res.GetMessage());
     *         }
     *         return res;
     *     #endif
     * }
     * @endcode
     */
    /** @{ */

    /**
     * @brief Activer le support des exceptions C++
     * @def NKENTSEU_ENABLE_EXCEPTIONS
     * @ingroup ExceptionConfig
     * @note Défini si NKENTSEU_HAS_EXCEPTIONS et non désactivé explicitement
     */
    #if defined(NKENTSEU_HAS_EXCEPTIONS) && !defined(NKENTSEU_DISABLE_EXCEPTIONS)
        #define NKENTSEU_ENABLE_EXCEPTIONS
    #endif

    /** @} */ // End of ExceptionConfig

    // -------------------------------------------------------------------------
    // SECTION 14 : CONFIGURATION SPÉCIFIQUE PLATEFORME
    // -------------------------------------------------------------------------
    // Options activées selon la plateforme cible détectée.

    /**
     * @defgroup PlatformConfig Configuration Plateforme-Spécifique
     * @brief Macros activées conditionnellement selon la plateforme cible
     * @ingroup CoreConfiguration
     *
     * Définit les APIs et fonctionnalités spécifiques à utiliser
     * selon la plateforme détectée par NkPlatform.h.
     *
     * @note
     *   - Windows : NKENTSEU_USE_WIN32_API, NKENTSEU_ENABLE_UNICODE (sauf si désactivé)
     *   - POSIX (Linux/macOS/BSD) : NKENTSEU_USE_POSIX_API
     *
     * @example
     * @code
     * // Désactiver Unicode sur Windows pour compatibilité avec code legacy ANSI
     * #define NKENTSEU_PLATFORM_WINDOWS
     * #define NKENTSEU_DISABLE_UNICODE
     * #include "NKCore/NkConfig.h"  // NKENTSEU_ENABLE_UNICODE ne sera pas défini
     *
     * // Code conditionnel selon l'API plateforme
     * void* AllocateOSMemory(nk_size size) {
     *     #if defined(NKENTSEU_USE_WIN32_API)
     *         return VirtualAlloc(nullptr, size, MEM_COMMIT, PAGE_READWRITE);
     *     #elif defined(NKENTSEU_USE_POSIX_API)
     *         return mmap(nullptr, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
     *     #else
     *         return std::malloc(size);  // Fallback portable
     *     #endif
     * }
     * @endcode
     */
    /** @{ */

    // ============================================================
    // WINDOWS-SPECIFIC
    // ============================================================

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        /**
         * @brief Utiliser les APIs Win32 natives
         * @def NKENTSEU_USE_WIN32_API
         * @ingroup PlatformConfig
         */
        #define NKENTSEU_USE_WIN32_API

        /**
         * @brief Activer le support Unicode (UTF-16 via WCHAR)
         * @def NKENTSEU_ENABLE_UNICODE
         * @ingroup PlatformConfig
         * @note Défini par défaut sauf si NKENTSEU_DISABLE_UNICODE est spécifié
         */
        #if !defined(NKENTSEU_DISABLE_UNICODE)
            #define NKENTSEU_ENABLE_UNICODE
        #endif
    #endif

    // ============================================================
    // POSIX-SPECIFIC (Linux, macOS, BSD, etc.)
    // ============================================================

    #if defined(NKENTSEU_PLATFORM_POSIX)
        /**
         * @brief Utiliser les APIs POSIX natives
         * @def NKENTSEU_USE_POSIX_API
         * @ingroup PlatformConfig
         */
        #define NKENTSEU_USE_POSIX_API
    #endif

    /** @} */ // End of PlatformConfig

    // -------------------------------------------------------------------------
    // SECTION 15 : CONFIGURATION GRAPHIQUE (SI NKGRAPHICS LIÉ)
    // -------------------------------------------------------------------------
    // Sélection du backend graphique par défaut selon la plateforme.

    /**
     * @defgroup GraphicsConfig Configuration Backend Graphique
     * @brief Sélection du backend graphique par défaut selon la plateforme
     * @ingroup CoreConfiguration
     *
     * Définit le backend graphique à utiliser par défaut quand
     * le module NKGraphics est inclus dans le build.
     *
     * @note
     *   - Windows : Direct3D 11 par défaut (plus stable), D3D12 disponible
     *   - macOS/iOS : Metal (seul backend natif supporté)
     *   - Android : OpenGL ES 3 (plus compatible), Vulkan disponible sur API 24+
     *   - Autres : OpenGL (fallback cross-platform)
     *
     * @example
     * @code
     * // Forcer Vulkan sur Windows même si D3D11 est le défaut
     * #if defined(NKENTSEU_PLATFORM_WINDOWS)
     *     #undef NKENTSEU_GRAPHICS_BACKEND_DEFAULT
     *     #define NKENTSEU_GRAPHICS_BACKEND_DEFAULT NKENTSEU_GRAPHICS_VULKAN
     * #endif
     * #include "NKCore/NkConfig.h"
     *
     * // Sélection runtime avec fallback
     * nkentseu::GraphicsAPI ChooseBackend() {
     *     #if defined(NKENTSEU_GRAPHICS_BACKEND_DEFAULT)
     *         if (IsBackendAvailable(NKENTSEU_GRAPHICS_BACKEND_DEFAULT)) {
     *             return NKENTSEU_GRAPHICS_BACKEND_DEFAULT;
     *         }
     *     #endif
     *     // Fallback vers OpenGL si backend préféré indisponible
     *     return NKENTSEU_GRAPHICS_OPENGL;
     * }
     * @endcode
     */
    /** @{ */

    /**
     * @brief Backend graphique par défaut selon la plateforme
     * @def NKENTSEU_GRAPHICS_BACKEND_DEFAULT
     * @ingroup GraphicsConfig
     * @values
     *   - NKENTSEU_GRAPHICS_D3D11 : Windows par défaut
     *   - NKENTSEU_GRAPHICS_METAL : macOS/iOS
     *   - NKENTSEU_GRAPHICS_GLES3 : Android
     *   - NKENTSEU_GRAPHICS_OPENGL : fallback cross-platform
     * @note Peut être redéfini avant inclusion pour forcer un backend spécifique
     */
    #if !defined(NKENTSEU_GRAPHICS_BACKEND_DEFAULT)
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            #define NKENTSEU_GRAPHICS_BACKEND_DEFAULT NKENTSEU_GRAPHICS_D3D11
        #elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
            #define NKENTSEU_GRAPHICS_BACKEND_DEFAULT NKENTSEU_GRAPHICS_METAL
        #elif defined(NKENTSEU_PLATFORM_ANDROID)
            #define NKENTSEU_GRAPHICS_BACKEND_DEFAULT NKENTSEU_GRAPHICS_GLES3
        #else
            #define NKENTSEU_GRAPHICS_BACKEND_DEFAULT NKENTSEU_GRAPHICS_OPENGL
        #endif
    #endif

    /** @} */ // End of GraphicsConfig

    // -------------------------------------------------------------------------
    // SECTION 16 : VALIDATION DE LA CONFIGURATION
    // -------------------------------------------------------------------------
    // Checks compile-time pour détecter les conflits et incohérences.

    /**
     * @defgroup ConfigValidation Validation Configuration
     * @brief Vérifications compile-time des incohérences de configuration
     * @ingroup CoreConfiguration
     * @internal
     *
     * Ces directives génèrent des erreurs ou avertissements si la
     * configuration contient des conflits ou des combinaisons invalides.
     *
     * @note
     *   Les erreurs (#error) bloquent la compilation pour éviter des bugs subtils.
     *   Les avertissements (#warning) informent mais permettent de continuer.
     */
    /** @{ */

    // Vérifier conflits Debug/Release (mutuellement exclusifs)
    #if defined(NKENTSEU_DEBUG) && defined(NKENTSEU_RELEASE)
        #error "Configuration error: Cannot define both NKENTSEU_DEBUG and NKENTSEU_RELEASE"
    #endif

    // Vérifier SIMD : avertir si activé sans support CPU détecté
    #if defined(NKENTSEU_ENABLE_SIMD)
        #if !defined(NKENTSEU_CPU_HAS_SSE2) && !defined(NKENTSEU_CPU_HAS_NEON)
            #warning "Configuration warning: SIMD enabled but no SIMD instruction set detected (SSE2/NEON). Disabling SIMD for compatibility."
            #undef NKENTSEU_ENABLE_SIMD
        #endif
    #endif

    /** @} */ // End of ConfigValidation

    // -------------------------------------------------------------------------
    // SECTION 17 : MACROS UTILITAIRES POUR CODE CONDITIONNEL
    // -------------------------------------------------------------------------
    // Helpers pour écrire du code qui varie selon la configuration.

    /**
     * @defgroup ConfigUtils Utilitaires Configuration
     * @brief Macros utilitaires pour code conditionnel debug/release
     * @ingroup CoreConfiguration
     *
     * Facilite l'écriture de code qui se comporte différemment
     * selon le mode de build, sans imbriquer manuellement des #if.
     *
     * @note
     *   - NKENTSEU_IS_DEBUG / NKENTSEU_IS_RELEASE : valeurs 0/1 pour usage dans expressions
     *   - NKENTSEU_DEBUG_ONLY(code) / NKENTSEU_RELEASE_ONLY(code) : inclut/exclut du code
     *
     * @example
     * @code
     * // Utilisation des macros conditionnelles
     * void DebugPrint(const char* msg) {
     *     NKENTSEU_DEBUG_ONLY(
     *         printf("[DEBUG] %s\n", msg);  // Compilé seulement en debug
     *     )
     * }
     *
     * // Utilisation dans des expressions
     * int GetOptimizationLevel() {
     *     return NKENTSEU_IS_DEBUG ? 0 : 3;  // 0 = no opt, 3 = full opt
     * }
     *
     * // Alternative plus lisible avec if constexpr (C++17+)
     * template <bool IsDebug = NKENTSEU_IS_DEBUG>
     * void OptimizedFunction() {
     *     if constexpr (IsDebug) {
     *         ValidateInvariants();  // Éliminé à la compilation en release
     *     }
     *     // ... code commun ...
     * }
     * @endcode
     */
    /** @{ */

    /**
     * @brief Valeur booléenne indiquant si en mode Debug
     * @def NKENTSEU_IS_DEBUG
     * @ingroup ConfigUtils
     * @value 1 si NKENTSEU_DEBUG défini, 0 sinon
     * @note Utile dans des expressions constexpr ou conditions runtime
     */
    #if defined(NKENTSEU_DEBUG)
        #define NKENTSEU_IS_DEBUG 1
    #else
        #define NKENTSEU_IS_DEBUG 0
    #endif

    /**
     * @brief Valeur booléenne indiquant si en mode Release
     * @def NKENTSEU_IS_RELEASE
     * @ingroup ConfigUtils
     * @value 1 si NKENTSEU_RELEASE défini, 0 sinon
     */
    #if defined(NKENTSEU_RELEASE)
        #define NKENTSEU_IS_RELEASE 1
    #else
        #define NKENTSEU_IS_RELEASE 0
    #endif

    /**
     * @brief Inclut du code uniquement en mode Debug
     * @def NKENTSEU_DEBUG_ONLY(code)
     * @param code Bloc de code à inclure seulement en debug
     * @ingroup ConfigUtils
     * @note En release, le code est complètement éliminé (pas même compilé)
     */
    #if defined(NKENTSEU_DEBUG)
        #define NKENTSEU_DEBUG_ONLY(code) code
        #define NKENTSEU_RELEASE_ONLY(code)
    #else
        #define NKENTSEU_DEBUG_ONLY(code)
        #define NKENTSEU_RELEASE_ONLY(code) code
    #endif

    /** @} */ // End of ConfigUtils

    // -------------------------------------------------------------------------
    // Small String Optimization (SSO) Configuration
    // -------------------------------------------------------------------------

    /**
     * @brief Taille du buffer interne pour l'optimisation SSO
     * 
     * Les chaînes de longueur <= NK_STRING_SSO_SIZE sont stockées
     * directement dans l'objet NkString, sans allocation heap.
     * 
     * Valeur recommandée : 23
     * - Permet un objet NkString de 24-32 bytes total (selon alignement)
     * - Couvre la majorité des cas d'usage courants (noms, clés, tokens)
     * - Évite les allocations pour les petites chaînes fréquentes
     * 
     * @note Ajuster selon les besoins :
     *       - 15 : objet plus compact, moins de SSO hits
     *       - 31 : plus de SSO hits, objet légèrement plus gros
     *       - >31 : gains marginaux, impact mémoire notable à grande échelle
     */
    #ifndef NK_STRING_SSO_SIZE
        #define NK_STRING_SSO_SIZE 23
    #endif

#endif // NKENTSEU_CORE_NKCONFIG_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION DE NKCONFIG.H
// =============================================================================
// Ce fichier centralise la configuration globale du framework NKCore.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Configuration de build via CMake
// -----------------------------------------------------------------------------
/*
    # CMakeLists.txt - Configuration du projet

    cmake_minimum_required(VERSION 3.15)
    project(MyNKCoreApp)

    # Options de build
    option(BUILD_DEBUG "Build in debug mode" ON)
    option(ENABLE_SIMD "Enable SIMD optimizations" ON)
    option(ENABLE_PROFILING "Enable performance profiling" OFF)

    # Flags de compilation selon l'option
    if(BUILD_DEBUG)
        add_definitions(-DNKENTSEU_DEBUG)
        set(CMAKE_BUILD_TYPE Debug)
    else()
        add_definitions(-DNDEBUG -DNKENTSEU_RELEASE)
        set(CMAKE_BUILD_TYPE Release)
    endif()

    # Options supplémentaires
    if(ENABLE_SIMD)
        if(MSVC)
            add_compile_options(/arch:AVX2)  # Ou /arch:SSE2 pour compatibilité
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            add_compile_options(-march=native)  # Ou -msse2 -mavx2 pour ciblage précis
        endif()
    else()
        add_definitions(-DNKENTSEU_DISABLE_SIMD)
    endif()

    if(ENABLE_PROFILING)
        add_definitions(-DNKENTSEU_PROFILE)
    endif()

    # Inclure NKCore
    add_subdirectory(external/NKCore)
    target_link_libraries(MyNKCoreApp PRIVATE NKCore)
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Code conditionnel selon la configuration
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkConfig.h"
    #include "NKCore/NkFoundationLog.h"

    void InitializeSubsystem() {
        // Logging conditionnel selon le niveau configuré
        #if NKENTSEU_LOG_LEVEL >= 4  // DEBUG
            NK_FOUNDATION_LOG_DEBUG("Initializing subsystem with verbose logging");
        #elif NKENTSEU_LOG_LEVEL >= 3  // INFO
            NK_FOUNDATION_LOG_INFO("Initializing subsystem");
        #endif

        // Assertions seulement en debug
        NKENTSEU_ASSERT_MSG(CheckPrerequisites(), "Subsystem prerequisites not met");

        // Code de profiling seulement si activé
        NKENTSEU_ENABLE_PROFILING_ONLY(
            NKENTSEU_PROFILE_SCOPE("InitializeSubsystem");
        )

        // Allocation avec alignement configuré
        void* buffer = AlignedAllocate(
            BUFFER_SIZE,
            NKENTSEU_DEFAULT_ALIGNMENT  // 16 si SIMD, 8 sinon
        );

        // Thread pool avec nombre de threads configuré
        #if defined(NKENTSEU_ENABLE_THREADING)
            ThreadPool pool(NKENTSEU_DEFAULT_THREAD_COUNT);  // 0 = auto-detect
            pool.SubmitTask([](){ /* ... *\/ });
        #else
            // Fallback single-thread
            ExecuteTaskDirectly();
        #endif
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Configuration runtime basée sur les macros compile-time
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkConfig.h"
    #include "NKCore/NkSystemInfo.h"

    class AppConfig {
    public:
        static AppConfig& Instance() {
            static AppConfig instance;
            return instance;
        }

        // Paramètres influencés par la configuration compile-time
        struct BuildFlags {
            bool isDebug = NKENTSEU_IS_DEBUG;
            bool hasAssertions = defined(NKENTSEU_ENABLE_ASSERTS);
            bool hasMemoryTracking = defined(NKENTSEU_ENABLE_MEMORY_TRACKING);
            bool hasSIMD = defined(NKENTSEU_ENABLE_SIMD);
            int logLevel = NKENTSEU_LOG_LEVEL;
        };

        const BuildFlags& GetBuildFlags() const {
            return m_buildFlags;
        }

        // Ajustements runtime basés sur les capacités détectées
        void OptimizeForHardware() {
            #if defined(NKENTSEU_ENABLE_SIMD)
                if (nkentseu::SystemInfo::HasAVX2()) {
                    m_processingMode = ProcessingMode::AVX2;
                } else if (nkentseu::SystemInfo::HasSSE2()) {
                    m_processingMode = ProcessingMode::SSE2;
                } else {
                    m_processingMode = ProcessingMode::Scalar;
                }
            #else
                m_processingMode = ProcessingMode::Scalar;  // Fallback si SIMD désactivé
            #endif

            // Nombre de threads : config compile-time ou override runtime
            m_threadCount = (NKENTSEU_DEFAULT_THREAD_COUNT > 0)
                ? NKENTSEU_DEFAULT_THREAD_COUNT
                : nkentseu::SystemInfo::GetLogicalCoreCount();
        }

    private:
        BuildFlags m_buildFlags;
        ProcessingMode m_processingMode = ProcessingMode::Scalar;
        int m_threadCount = 1;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Validation de configuration dans un script de build
// -----------------------------------------------------------------------------
/*
    # validate_config.py - Script de validation pré-build

    import sys
    import re

    def validate_config(config_file):
        with open(config_file, 'r') as f:
            content = f.read()

        errors = []

        # Vérifier conflits Debug/Release
        if '#define NKENTSEU_DEBUG' in content and '#define NKENTSEU_RELEASE' in content:
            errors.append("ERROR: Both DEBUG and RELEASE defined")

        # Vérifier cohérence SIMD
        if 'NKENTSEU_ENABLE_SIMD' in content:
            if 'NKENTSEU_CPU_HAS_SSE2' not in content and 'NKENTSEU_CPU_HAS_NEON' not in content:
                errors.append("WARNING: SIMD enabled but no CPU feature detected")

        # Vérifier niveau de log valide
        log_level_match = re.search(r'#define NKENTSEU_LOG_LEVEL\s+(\d+)', content)
        if log_level_match:
            level = int(log_level_match.group(1))
            if not (0 <= level <= 5):
                errors.append(f"ERROR: Invalid LOG_LEVEL {level} (must be 0-5)")

        if errors:
            print("Configuration validation failed:")
            for err in errors:
                print(f"  - {err}")
            return False
        return True

    if __name__ == "__main__":
        config_path = sys.argv[1] if len(sys.argv) > 1 else "NKCore/NkConfig.h"
        sys.exit(0 if validate_config(config_path) else 1)
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Documentation générée depuis les macros de config
// -----------------------------------------------------------------------------
/*
    // generate_config_docs.cpp - Extrait la configuration pour documentation

    #include <iostream>
    #include <fstream>
    #include <regex>
    #include "NKCore/NkConfig.h"  // Inclure après définition des macros personnalisées

    void GenerateConfigMarkdown() {
        std::ofstream out("CONFIGURATION.md");

        out << "# Configuration du Framework NKCore\n\n";
        out << "Ce document liste les macros de configuration disponibles.\n\n";

        // Exemple d'extraction automatique (pseudo-code)
        auto ExtractMacroDocs = [](const std::string& group) {
            // Parser les commentaires Doxygen des macros du groupe
            // Générer une table Markdown : Macro | Description | Valeur par défaut
        };

        out << "## Modes de Build\n";
        out << "| Macro | Description | Défaut |\n";
        out << "|-------|-------------|--------|\n";
        out << "| `NKENTSEU_DEBUG` | Mode développement avec assertions | Auto si pas RELEASE |\n";
        out << "| `NKENTSEU_RELEASE` | Mode production optimisé | Auto si NDEBUG |\n";
        // ... autres groupes ...

        out << "\n## Exemple d'utilisation\n";
        out << "```cmake\n";
        out << "add_definitions(-DNKENTSEU_DEBUG -DNKENTSEU_ENABLE_PROFILING)\n";
        out << "```\n";
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================