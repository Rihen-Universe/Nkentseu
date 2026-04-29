// =============================================================================
// NKThreading/NKThreadingApi.h
// Gestion de la visibilité des symboles et de l'API publique du module NKThreading.
//
// Design :
//  - Réutilisation DIRECTE des macros de NKPlatform (ZÉRO duplication)
//  - Macros spécifiques NKThreading uniquement pour la configuration de build
//  - Indépendance totale des modes de build : NKThreading et NKPlatform peuvent
//    avoir des configurations différentes (static vs shared)
//  - Compatibilité multiplateforme et multi-compilateur via NKPlatform
//  - Aucune redéfinition de deprecated, alignement, pragma, etc.
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_THREADING_NKTHREADINGAPI_H
#define NKENTSEU_THREADING_NKTHREADINGAPI_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // NKThreading dépend de NKPlatform. Nous importons ses macros d'export.
    // AUCUNE duplication : nous utilisons directement les macros NKPlatform.

    #include "NKPlatform/NkPlatformExport.h"
    #include "NKPlatform/NkPlatformInline.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION DU MODE DE BUILD NKTHREADING
    // -------------------------------------------------------------------------
    /**
     * @defgroup ThreadingBuildConfig Configuration du Build NKThreading
     * @brief Macros pour contrôler le mode de compilation de NKThreading
     *
     * Ces macros sont INDÉPENDANTES de celles de NKPlatform :
     *  - NKENTSEU_THREADING_BUILD_SHARED_LIB : Compiler NKThreading en bibliothèque partagée
     *  - NKENTSEU_THREADING_STATIC_LIB : Utiliser NKThreading en mode bibliothèque statique
     *  - NKENTSEU_THREADING_HEADER_ONLY : Mode header-only (tout inline)
     *
     * @note NKThreading et NKPlatform peuvent avoir des modes de build différents.
     * Exemple valide : NKPlatform en DLL + NKThreading en static.
     *
     * @example CMakeLists.txt
     * @code
     * # NKThreading en DLL, NKPlatform en static
     * target_compile_definitions(nkthreading PRIVATE NKENTSEU_THREADING_BUILD_SHARED_LIB)
     * target_compile_definitions(nkthreading PRIVATE NKENTSEU_STATIC_LIB)
     *
     * # NKThreading en static, NKPlatform en DLL
     * target_compile_definitions(monapp PRIVATE NKENTSEU_THREADING_STATIC_LIB)
     * @endcode
     */

    // -------------------------------------------------------------------------
    // SECTION 3 : MACRO PRINCIPALE NKENTSEU_THREADING_API
    // -------------------------------------------------------------------------
    /**
     * @brief Macro principale pour l'export/import des symboles NKThreading
     * @def NKENTSEU_THREADING_API
     * @ingroup ThreadingApiMacros
     *
     * Cette macro gère UNIQUEMENT la visibilité des symboles de NKThreading.
     * Elle est indépendante de NKENTSEU_PLATFORM_API.
     *
     * Logique :
     *  - NKENTSEU_THREADING_BUILD_SHARED_LIB : export (compilation de NKThreading en DLL)
     *  - NKENTSEU_THREADING_STATIC_LIB ou NKENTSEU_THREADING_HEADER_ONLY : vide
     *  - Sinon : import (utilisation de NKThreading en mode DLL)
     *
     * @note Cette macro utilise NKENTSEU_PLATFORM_API_EXPORT/IMPORT comme base,
     * garantissant la compatibilité multiplateforme sans duplication.
     *
     * @example
     * @code
     * // Dans un header public de NKThreading :
     * NKENTSEU_THREADING_API void ThreadingInitialize();
     *
     * // Pour compiler NKThreading en DLL : -DNKENTSEU_THREADING_BUILD_SHARED_LIB
     * // Pour utiliser NKThreading en DLL : (aucun define, import par défaut)
     * // Pour utiliser NKThreading en static : -DNKENTSEU_THREADING_STATIC_LIB
     * @endcode
     */
    #if defined(NKENTSEU_THREADING_BUILD_SHARED_LIB)
        #define NKENTSEU_THREADING_API NKENTSEU_PLATFORM_API_EXPORT
    #elif defined(NKENTSEU_THREADING_STATIC_LIB) || defined(NKENTSEU_THREADING_HEADER_ONLY)
        #define NKENTSEU_THREADING_API
    #else
        #define NKENTSEU_THREADING_API NKENTSEU_PLATFORM_API_IMPORT
    #endif

    // -------------------------------------------------------------------------
    // SECTION 4 : MACROS DE CONVENANCE SPÉCIFIQUES À NKTHREADING
    // -------------------------------------------------------------------------
    // Seules les macros qui ajoutent une valeur spécifique à NKThreading sont définies.
    // Tout le reste (deprecated, alignement, pragma, etc.) est utilisé directement
    // depuis NKPlatform via le préfixe NKENTSEU_*.

    /**
     * @brief Macro pour exporter une classe complète de NKThreading
     * @def NKENTSEU_THREADING_CLASS_EXPORT
     * @ingroup ThreadingApiMacros
     *
     * Alias vers NKENTSEU_THREADING_API pour les déclarations de classe.
     *
     * @example
     * @code
     * class NKENTSEU_THREADING_CLASS_EXPORT ThreadPool {
     * public:
     *     void SubmitTask(Task* task);
     * };
     * @endcode
     */
    #define NKENTSEU_THREADING_CLASS_EXPORT NKENTSEU_THREADING_API

    /**
     * @brief Fonction inline exportée pour NKThreading
     * @def NKENTSEU_THREADING_API_INLINE
     * @ingroup ThreadingApiMacros
     *
     * Combinaison de NKENTSEU_THREADING_API et NKENTSEU_INLINE.
     * En mode header-only, équivaut à NKENTSEU_FORCE_INLINE.
     */
    #if defined(NKENTSEU_THREADING_HEADER_ONLY)
        #define NKENTSEU_THREADING_API_INLINE NKENTSEU_FORCE_INLINE
    #else
        #define NKENTSEU_THREADING_API_INLINE NKENTSEU_THREADING_API NKENTSEU_INLINE
    #endif

    /**
     * @brief Fonction force_inline exportée pour NKThreading
     * @def NKENTSEU_THREADING_API_FORCE_INLINE
     * @ingroup ThreadingApiMacros
     *
     * Combinaison de NKENTSEU_THREADING_API et NKENTSEU_FORCE_INLINE.
     * Pour les fonctions critiques en performance dans l'API publique.
     */
    #if defined(NKENTSEU_THREADING_HEADER_ONLY)
        #define NKENTSEU_THREADING_API_FORCE_INLINE NKENTSEU_FORCE_INLINE
    #else
        #define NKENTSEU_THREADING_API_FORCE_INLINE NKENTSEU_THREADING_API NKENTSEU_FORCE_INLINE
    #endif

    /**
     * @brief Fonction no_inline exportée pour NKThreading
     * @def NKENTSEU_THREADING_API_NO_INLINE
     * @ingroup ThreadingApiMacros
     *
     * Combinaison de NKENTSEU_THREADING_API et NKENTSEU_NO_INLINE.
     * Pour éviter l'explosion de code dans les DLL ou faciliter le debugging.
     */
    #define NKENTSEU_THREADING_API_NO_INLINE NKENTSEU_THREADING_API NKENTSEU_NO_INLINE

    // -------------------------------------------------------------------------
    // SECTION 5 : UTILISATION DIRECTE DES MACROS NKPLATFORM (PAS DE DUPLICATION)
    // -------------------------------------------------------------------------
    /**
     * @defgroup ThreadingApiReuse Réutilisation des Macros NKPlatform
     * @brief Guide d'utilisation des macros NKPlatform dans NKThreading
     *
     * Pour éviter la duplication, NKThreading réutilise directement les macros
     * de NKPlatform. Voici les équivalences :
     *
     * | Besoin dans NKThreading | Macro à utiliser | Source |
     * |------------------------|-----------------|--------|
     * | Dépréciation | `NKENTSEU_DEPRECATED` | NKPlatform |
     * | Dépréciation avec message | `NKENTSEU_DEPRECATED_MESSAGE(msg)` | NKPlatform |
     * | Alignement cache | `NKENTSEU_ALIGN_CACHE` | NKPlatform |
     * | Alignement 16/32/64 | `NKENTSEU_ALIGN_16`, etc. | NKPlatform |
     * | Liaison C | `NKENTSEU_EXTERN_C_BEGIN/END` | NKPlatform |
     * | Pragmas | `NKENTSEU_PRAGMA(x)` | NKPlatform |
     * | Gestion warnings | `NKENTSEU_DISABLE_WARNING_*` | NKPlatform |
     * | Symbole local | `NKENTSEU_API_LOCAL` | NKPlatform |
     *
     * @example
     * @code
     * // Dépréciation dans NKThreading :
     * NKENTSEU_DEPRECATED_MESSAGE("Utiliser ThreadPool::Submit()")
     * NKENTSEU_THREADING_API void LegacySubmit(Task* task);
     *
     * // Alignement dans NKThreading :
     * struct NKENTSEU_ALIGN_64 NKENTSEU_THREADING_CLASS_EXPORT ThreadLocalData {
     *     uint64_t counters[8];
     * };
     * @endcode
     */

    // -------------------------------------------------------------------------
    // SECTION 6 : VALIDATION DES MACROS DE BUILD NKTHREADING
    // -------------------------------------------------------------------------
    // Vérifications de cohérence pour les macros spécifiques à NKThreading.

    #if defined(NKENTSEU_THREADING_BUILD_SHARED_LIB) && defined(NKENTSEU_THREADING_STATIC_LIB)
        #warning "NKThreading: BUILD_SHARED_LIB et STATIC_LIB définis - STATIC_LIB ignoré"
        #undef NKENTSEU_THREADING_STATIC_LIB
    #endif

    #if defined(NKENTSEU_THREADING_BUILD_SHARED_LIB) && defined(NKENTSEU_THREADING_HEADER_ONLY)
        #warning "NKThreading: BUILD_SHARED_LIB et HEADER_ONLY définis - HEADER_ONLY ignoré"
        #undef NKENTSEU_THREADING_HEADER_ONLY
    #endif

    #if defined(NKENTSEU_THREADING_STATIC_LIB) && defined(NKENTSEU_THREADING_HEADER_ONLY)
        #warning "NKThreading: STATIC_LIB et HEADER_ONLY définis - HEADER_ONLY ignoré"
        #undef NKENTSEU_THREADING_HEADER_ONLY
    #endif

    // -------------------------------------------------------------------------
    // SECTION 7 : MESSAGES DE DEBUG OPTIONNELS
    // -------------------------------------------------------------------------
    #ifdef NKENTSEU_THREADING_DEBUG
        #pragma message("NKThreading Export Config:")
        #if defined(NKENTSEU_THREADING_BUILD_SHARED_LIB)
            #pragma message("  NKThreading mode: Shared (export)")
        #elif defined(NKENTSEU_THREADING_STATIC_LIB)
            #pragma message("  NKThreading mode: Static")
        #elif defined(NKENTSEU_THREADING_HEADER_ONLY)
            #pragma message("  NKThreading mode: Header-only")
        #else
            #pragma message("  NKThreading mode: DLL import (default)")
        #endif
        #pragma message("  NKENTSEU_THREADING_API = " NKENTSEU_STRINGIZE(NKENTSEU_THREADING_API))
    #endif

#endif // NKENTSEU_THREADING_NKTHREADINGAPI_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKTHREADINGAPI.H
// =============================================================================
// Ce fichier fournit les macros pour gérer la visibilité des symboles du module
// NKThreading, avec support multiplateforme et multi-compilateur.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Déclaration d'une fonction publique NKThreading (style C)
// -----------------------------------------------------------------------------
/*
    // Dans nkthreading_public.h :
    #include "NKThreading/NKThreadingApi.h"

    NKENTSEU_THREADING_EXTERN_C_BEGIN

    // Fonction C publique exportée pour l'initialisation du module
    NKENTSEU_THREADING_API void Threading_Initialize(void);

    // Fonction C publique pour la libération des ressources
    NKENTSEU_THREADING_API void Threading_Shutdown(void);

    // Fonction C publique pour récupérer la version du module
    NKENTSEU_THREADING_API uint32_t Threading_GetVersion(void);

    NKENTSEU_THREADING_EXTERN_C_END

    // Dans nkthreading.cpp :
    #include "nkthreading_public.h"

    void Threading_Initialize(void) {
        // Implémentation de l'initialisation des primitives de threading...
    }

    void Threading_Shutdown(void) {
        // Implémentation du nettoyage des ressources threading...
    }

    uint32_t Threading_GetVersion(void) {
        return 0x010200;  // Version 1.2.0 encodée en hexadécimal
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Déclaration d'une classe publique NKThreading (style C++)
// -----------------------------------------------------------------------------
/*
    // Dans ThreadPool.h :
    #include "NKThreading/NKThreadingApi.h"
    #include <functional>

    namespace nkentseu {
    namespace threading {

        class NKENTSEU_THREADING_CLASS_EXPORT ThreadPool {
        public:
            // Constructeur avec paramètres de configuration
            NKENTSEU_THREADING_API explicit ThreadPool(size_t threadCount);

            // Destructeur virtuel pour polymorphisme sécurisé
            NKENTSEU_THREADING_API virtual ~ThreadPool();

            // Soumission d'une tâche au pool de threads
            NKENTSEU_THREADING_API void Submit(std::function<void()> task);

            // Arrêt gracieux du pool de threads
            NKENTSEU_THREADING_API void Shutdown();

            // Vérification de l'état d'initialisation
            NKENTSEU_THREADING_API bool IsRunning() const;

            // Fonction inline critique pour performance (accès rapide)
            NKENTSEU_THREADING_API_FORCE_INLINE size_t GetActiveThreadCount() const {
                return m_activeThreads.load(std::memory_order_relaxed);
            }

            // Fonction no-inline pour faciliter le profiling et le debugging
            NKENTSEU_THREADING_API_NO_INLINE void DumpStatistics() const;

        protected:
            // Méthode protégée pour extension par héritage
            NKENTSEU_THREADING_API virtual void OnThreadStart();

            NKENTSEU_THREADING_API virtual void OnThreadEnd();

        private:
            // Méthode privée : pas d'export nécessaire (implémentation interne)
            void WorkerThreadFunction();

            // Membres privés : gestion interne du pool
            std::atomic<size_t> m_activeThreads;
            std::atomic<bool> m_shutdown;
            // ... autres membres privés ...
        };

    } // namespace threading
    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Gestion des modes de build via CMake pour NKThreading
// -----------------------------------------------------------------------------
/*
    # CMakeLists.txt pour NKThreading

    cmake_minimum_required(VERSION 3.15)
    project(NKThreading VERSION 1.2.0 LANGUAGES CXX)

    # Options de build configurables par l'utilisateur
    option(NKTHREADING_BUILD_SHARED "Build NKThreading as shared library" ON)
    option(NKTHREADING_HEADER_ONLY "Use NKThreading in header-only mode" OFF)
    option(NKTHREADING_ENABLE_SANITIZERS "Enable thread sanitizers for debug" OFF)

    # Configuration des defines de compilation
    if(NKTHREADING_HEADER_ONLY)
        add_definitions(-DNKENTSEU_THREADING_HEADER_ONLY)
    elseif(NKTHREADING_BUILD_SHARED)
        add_definitions(-DNKENTSEU_THREADING_BUILD_SHARED_LIB)
        set(NKTHREADING_LIBRARY_TYPE SHARED)
    else()
        add_definitions(-DNKENTSEU_THREADING_STATIC_LIB)
        set(NKTHREADING_LIBRARY_TYPE STATIC)
    endif()

    # Création de la bibliothèque avec le type configuré
    add_library(nkthreading ${NKTHREADING_LIBRARY_TYPE}
        src/ThreadPool.cpp
        src/Mutex.cpp
        src/ConditionVariable.cpp
        src/ThreadLocal.cpp
        # ... autres fichiers sources du module ...
    )

    # Configuration des propriétés de la cible
    set_target_properties(nkthreading PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )

    # Installation des en-têtes publics dans l'arborescence include
    install(DIRECTORY include/NKThreading DESTINATION include)

    # Exposition des include directories pour les consommateurs
    target_include_directories(nkthreading PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    # Liaison avec les dépendances requises (NKPlatform)
    target_link_libraries(nkthreading PUBLIC NKPlatform::NKPlatform)

    # Configuration optionnelle des sanitizers pour le debug
    if(NKTHREADING_ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(nkthreading PRIVATE -fsanitize=thread)
        target_link_options(nkthreading PRIVATE -fsanitize=thread)
    endif()
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Utilisation de NKThreading dans une application cliente
// -----------------------------------------------------------------------------
/*
    // Dans l'application cliente (CMakeLists.txt) :

    find_package(NKThreading REQUIRED CONFIG)

    add_executable(monapp main.cpp worker.cpp)

    # Liaison avec la bibliothèque NKThreading
    target_link_libraries(monapp PRIVATE NKThreading::NKThreading)

    # Gestion automatique du mode d'import/export via le package config
    # Définir manuellement uniquement si le package ne gère pas les defines :
    # target_compile_definitions(monapp PRIVATE NKENTSEU_THREADING_STATIC_LIB)

    // Dans main.cpp :
    #include <NKThreading/ThreadPool.h>
    #include <NKThreading/Mutex.h>
    #include <iostream>

    int main() {
        // Initialisation du pool avec 4 threads workers
        nkentseu::threading::ThreadPool pool(4);

        // Soumission de tâches concurrentes
        for (int i = 0; i < 100; ++i) {
            pool.Submit([i]() {
                // Traitement parallèle de l'itération i
                std::cout << "Task " << i << " executed\n";
            });
        }

        // Attente de la complétion et nettoyage
        pool.Shutdown();

        return 0;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Mode header-only pour NKThreading (intégration légère)
// -----------------------------------------------------------------------------
/*
    // Pour utiliser NKThreading en mode header-only :

    // 1. Définir la macro AVANT toute inclusion des headers du module
    #define NKENTSEU_THREADING_HEADER_ONLY
    #include <NKThreading/NKThreadingApi.h>
    #include <NKThreading/ThreadPool.h>
    #include <NKThreading/Atomic.h>

    // 2. Toutes les fonctions marquées inline sont compilées dans chaque TU
    // 3. Aucun fichier .lib/.dll/.so n'est requis au linkage
    // 4. Idéal pour les petits projets, les tests unitaires, ou l'embarqué

    void LightweightParallelProcessing() {
        // Le pool est entièrement inline en mode header-only
        nkentseu::threading::ThreadPool pool(2);

        nkentseu::threading::AtomicCounter counter;

        for (int i = 0; i < 10; ++i) {
            pool.Submit([&counter]() {
                counter.Increment();  // Appel inline optimisé
            });
        }

        pool.Shutdown();
    }

    // Note importante :
    // Le mode header-only peut augmenter la taille du binaire final
    // car le code des fonctions inline est dupliqué dans chaque unité
    // de traduction. À utiliser avec discernement pour les gros projets.
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Dépréciation et migration d'API NKThreading
// -----------------------------------------------------------------------------
/*
    #include <NKThreading/NKThreadingApi.h>

    // Ancienne API dépréciée - sera retirée dans la version 2.0
    NKENTSEU_THREADING_DEPRECATED_MESSAGE("Utiliser ThreadPool::Submit(std::function<void()>)")
    NKENTSEU_THREADING_API void LegacySubmitTask(void (*callback)(void*), void* userData);

    // Nouvelle API recommandée avec std::function pour plus de flexibilité
    NKENTSEU_THREADING_API bool ThreadPool_SubmitTask(std::function<void()> task);

    // Dans le code client pour une migration progressive :
    void MigrateThreadingCode() {
        // Appel à l'ancienne API - génère un warning de dépréciation
        // LegacySubmitTask(&OldCallback, userData);

        // Migration vers la nouvelle API avec lambda capture
        ThreadPool_SubmitTask([userData]() {
            // Nouvelle implémentation avec capture de contexte
            ProcessData(userData);
        });
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Combinaison NKThreading + NKPlatform pour code multi-module
// -----------------------------------------------------------------------------
/*
    #include <NKPlatform/NkPlatformDetect.h>      // NKENTSEU_PLATFORM_*
    #include <NKPlatform/NkPlatformExport.h>      // NKENTSEU_PLATFORM_API
    #include <NKThreading/NKThreadingApi.h>       // NKENTSEU_THREADING_API

    // Fonction qui orchestre l'initialisation croisée des modules
    NKENTSEU_THREADING_API void CrossModuleThreadingInit() {
        // Logging via le module NKPlatform pour le diagnostic
        NK_FOUNDATION_LOG_INFO("Initializing threading subsystem...");

        // Affichage du mode de build actuel pour le debugging
        #if defined(NKENTSEU_THREADING_BUILD_SHARED_LIB)
            NK_FOUNDATION_LOG_DEBUG("NKThreading mode: Shared DLL");
        #elif defined(NKENTSEU_THREADING_STATIC_LIB)
            NK_FOUNDATION_LOG_DEBUG("NKThreading mode: Static library");
        #elif defined(NKENTSEU_THREADING_HEADER_ONLY)
            NK_FOUNDATION_LOG_DEBUG("NKThreading mode: Header-only");
        #else
            NK_FOUNDATION_LOG_DEBUG("NKThreading mode: DLL import (default)");
        #endif

        // Code spécifique à la plateforme pour l'optimisation threading
        #ifdef NKENTSEU_PLATFORM_WINDOWS
            // Configuration Windows : affinité des threads, priorité, etc.
            InitializeWindowsThreadingOptimizations();
        #elif defined(NKENTSEU_PLATFORM_LINUX)
            // Configuration Linux : scheduling policy, CPU affinity, etc.
            InitializeLinuxThreadingOptimizations();
        #elif defined(NKENTSEU_PLATFORM_MACOS)
            // Configuration macOS : QoS classes, thread naming, etc.
            InitializeMacOSThreadingOptimizations();
        #endif

        // Finalisation de l'initialisation croisée
        NK_FOUNDATION_LOG_INFO("Threading subsystem ready");
    }
*/

// -----------------------------------------------------------------------------
// Exemple 8 : Structures alignées pour performances thread-safe
// -----------------------------------------------------------------------------
/*
    #include <NKThreading/NKThreadingApi.h>
    #include <atomic>

    // Structure optimisée pour éviter le false sharing entre threads
    // Alignement sur ligne de cache typique (64 bytes)
    struct NKENTSEU_ALIGN_64 NKENTSEU_THREADING_CLASS_EXPORT CacheAlignedCounters {
        // Chaque compteur occupe sa propre ligne de cache
        std::atomic<uint64_t> readCount;   // Offset 0-63
        std::atomic<uint64_t> writeCount;  // Offset 64-127
        std::atomic<uint64_t> errorCount;  // Offset 128-191

        // Méthode inline pour lecture performante
        NKENTSEU_THREADING_API_FORCE_INLINE uint64_t GetTotalOperations() const {
            return readCount.load(std::memory_order_relaxed) +
                   writeCount.load(std::memory_order_relaxed);
        }
    };

    // Utilisation dans un contexte multi-threadé :
    void ThreadSafeProcessing() {
        static CacheAlignedCounters stats;  // Alignement garanti par la macro

        // Thread 1 : incrémente readCount
        // Thread 2 : incrémente writeCount
        // Aucun false sharing grâce à l'alignement NKENTSEU_ALIGN_64
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================