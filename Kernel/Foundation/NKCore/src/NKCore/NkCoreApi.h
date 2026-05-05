// =============================================================================
// NKCore/NkCoreApi.h
// Gestion de la visibilité des symboles et de l'API publique du module NKCore.
//
// Design :
//  - Réutilisation DIRECTE des macros de NKPlatform (ZÉRO duplication)
//  - Macros spécifiques NKCore uniquement pour la configuration de build NKCore
//  - Indépendance totale des modes de build : NKCore et NKPlatform peuvent
//    avoir des configurations différentes (static vs shared)
//  - Compatibilité multiplateforme et multi-compilateur via NKPlatform
//  - Aucune redéfinition de deprecated, alignement, pragma, etc.
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CORE_NKCOREAPI_H
#define NKENTSEU_CORE_NKCOREAPI_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // NKCore dépend de NKPlatform. Nous importons ses macros d'export.
    // AUCUNE duplication : nous utilisons directement les macros NKPlatform.

    #include "NKPlatform/NkPlatformExport.h"
    #include "NKPlatform/NkPlatformInline.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION DU MODE DE BUILD NKCORE
    // -------------------------------------------------------------------------
    /**
     * @defgroup CoreBuildConfig Configuration du Build NKCore
     * @brief Macros pour contrôler le mode de compilation de NKCore
     *
     * Ces macros sont INDÉPENDANTES de celles de NKPlatform :
     *  - NKENTSEU_CORE_BUILD_SHARED_LIB : Compiler NKCore en bibliothèque partagée
     *  - NKENTSEU_CORE_STATIC_LIB : Utiliser NKCore en mode bibliothèque statique
     *  - NKENTSEU_CORE_HEADER_ONLY : Mode header-only (tout inline)
     *
     * @note NKCore et NKPlatform peuvent avoir des modes de build différents.
     * Exemple valide : NKPlatform en DLL + NKCore en static.
     *
     * @example CMakeLists.txt
     * @code
     * # NKCore en DLL, NKPlatform en static
     * target_compile_definitions(nkcore PRIVATE NKENTSEU_CORE_BUILD_SHARED_LIB)
     * target_compile_definitions(nkcore PRIVATE NKENTSEU_STATIC_LIB)  # Pour NKPlatform
     *
     * # NKCore en static, NKPlatform en DLL
     * target_compile_definitions(monapp PRIVATE NKENTSEU_CORE_STATIC_LIB)
     * # (NKPlatform en DLL par défaut, pas de define nécessaire)
     * @endcode
     */

    // -------------------------------------------------------------------------
    // SECTION 3 : MACRO PRINCIPALE NKENTSEU_CORE_API
    // -------------------------------------------------------------------------
    /**
     * @brief Macro principale pour l'export/import des symboles NKCore
     * @def NKENTSEU_CORE_API
     * @ingroup CoreApiMacros
     *
     * Cette macro gère UNIQUEMENT la visibilité des symboles de NKCore.
     * Elle est indépendante de NKENTSEU_PLATFORM_API.
     *
     * Logique :
     *  - NKENTSEU_CORE_BUILD_SHARED_LIB : export (compilation de NKCore en DLL)
     *  - NKENTSEU_CORE_STATIC_LIB ou NKENTSEU_CORE_HEADER_ONLY : vide (pas d'export)
     *  - Sinon : import (utilisation de NKCore en mode DLL)
     *
     * @note Cette macro utilise NKENTSEU_PLATFORM_API_EXPORT/IMPORT comme base,
     * garantissant la compatibilité multiplateforme sans duplication.
     *
     * @example
     * @code
     * // Dans un header public de NKCore :
     * NKENTSEU_CORE_API void CoreInitialize();
     *
     * // Pour compiler NKCore en DLL : -DNKENTSEU_CORE_BUILD_SHARED_LIB
     * // Pour utiliser NKCore en DLL : (aucun define, import par défaut)
     * // Pour utiliser NKCore en static : -DNKENTSEU_CORE_STATIC_LIB
     * @endcode
     */
    #if defined(NKENTSEU_CORE_BUILD_SHARED_LIB)
        // Compilation de NKCore en bibliothèque partagée : exporter
        #define NKENTSEU_CORE_API NKENTSEU_PLATFORM_API_EXPORT
    #elif defined(NKENTSEU_CORE_STATIC_LIB) || defined(NKENTSEU_CORE_HEADER_ONLY)
        // Build statique ou header-only : pas de décoration
        #define NKENTSEU_CORE_API
    #else
        // Utilisation de NKCore en mode DLL : importer
        #define NKENTSEU_CORE_API NKENTSEU_PLATFORM_API_IMPORT
    #endif

    // -------------------------------------------------------------------------
    // SECTION 4 : MACROS DE CONVENANCE SPÉCIFIQUES À NKCORE
    // -------------------------------------------------------------------------
    // Seules les macros qui ajoutent une valeur spécifique à NKCore sont définies.
    // Tout le reste (deprecated, alignement, pragma, etc.) est utilisé directement
    // depuis NKPlatform via le préfixe NKENTSEU_*.

    /**
     * @brief Macro pour exporter une classe complète de NKCore
     * @def NKENTSEU_CORE_CLASS_EXPORT
     * @ingroup CoreApiMacros
     *
     * Alias vers NKENTSEU_CORE_API pour les déclarations de classe.
     *
     * @example
     * @code
     * class NKENTSEU_CORE_CLASS_EXPORT CoreManager {
     * public:
     *     void Initialize();
     * };
     * @endcode
     */
    #define NKENTSEU_CORE_CLASS_EXPORT NKENTSEU_CORE_API

    /**
     * @brief Fonction inline exportée pour NKCore
     * @def NKENTSEU_CORE_API_INLINE
     * @ingroup CoreApiMacros
     *
     * Combinaison de NKENTSEU_CORE_API et NKENTSEU_INLINE.
     * En mode header-only, équivaut à NKENTSEU_FORCE_INLINE.
     */
    #if defined(NKENTSEU_CORE_HEADER_ONLY)
        #define NKENTSEU_CORE_API_INLINE NKENTSEU_FORCE_INLINE
    #else
        #define NKENTSEU_CORE_API_INLINE NKENTSEU_CORE_API NKENTSEU_INLINE
    #endif

    /**
     * @brief Fonction force_inline exportée pour NKCore
     * @def NKENTSEU_CORE_API_FORCE_INLINE
     * @ingroup CoreApiMacros
     *
     * Combinaison de NKENTSEU_CORE_API et NKENTSEU_FORCE_INLINE.
     * Pour les fonctions critiques en performance dans l'API publique.
     */
    #if defined(NKENTSEU_CORE_HEADER_ONLY)
        #define NKENTSEU_CORE_API_FORCE_INLINE NKENTSEU_FORCE_INLINE
    #else
        #define NKENTSEU_CORE_API_FORCE_INLINE NKENTSEU_CORE_API NKENTSEU_FORCE_INLINE
    #endif

    /**
     * @brief Fonction no_inline exportée pour NKCore
     * @def NKENTSEU_CORE_API_NO_INLINE
     * @ingroup CoreApiMacros
     *
     * Combinaison de NKENTSEU_CORE_API et NKENTSEU_NO_INLINE.
     * Pour éviter l'explosion de code dans les DLL ou faciliter le debugging.
     */
    #define NKENTSEU_CORE_API_NO_INLINE NKENTSEU_CORE_API NKENTSEU_NO_INLINE

    /**
     * @brief Macro pour fonction exportée NKCore avec liaison C (extern "C")
     * @def NKENTSEU_CORE_C_API
     * @ingroup CoreApiMacros
     *
     * Combinaison de NKENTSEU_CORE_API et extern "C" pour les fonctions
     * de l'API publique C (interopérabilité C/C++).
     */
    #ifdef __cplusplus
        #define NKENTSEU_CORE_C_API extern "C" NKENTSEU_CORE_API
    #else
        #define NKENTSEU_CORE_C_API NKENTSEU_CORE_API
    #endif

    // -------------------------------------------------------------------------
    // SECTION 5 : UTILISATION DIRECTE DES MACROS NKPLATFORM (PAS DE DUPLICATION)
    // -------------------------------------------------------------------------
    /**
     * @defgroup CoreApiReuse Réutilisation des Macros NKPlatform
     * @brief Guide d'utilisation des macros NKPlatform dans NKCore
     *
     * Pour éviter la duplication, NKCore réutilise directement les macros
     * de NKPlatform. Voici les équivalences :
     *
     * | Besoin dans NKCore | Macro à utiliser | Source |
     * |-------------------|-----------------|--------|
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
     * // Dépréciation dans NKCore (pas de NKENTSEU_CORE_DEPRECATED) :
     * NKENTSEU_DEPRECATED_MESSAGE("Utiliser NewCoreFunction()")
     * NKENTSEU_CORE_API void OldCoreFunction();
     *
     * // Alignement dans NKCore (pas de NKENTSEU_CORE_ALIGN_32) :
     * struct NKENTSEU_ALIGN_32 NKENTSEU_CORE_CLASS_EXPORT CoreData {
     *     float data[8];
     * };
     * @endcode
     */

    // -------------------------------------------------------------------------
    // SECTION 6 : VALIDATION DES MACROS DE BUILD NKCORE
    // -------------------------------------------------------------------------
    // Vérifications de cohérence pour les macros spécifiques à NKCore.

    #if defined(NKENTSEU_CORE_BUILD_SHARED_LIB) && defined(NKENTSEU_CORE_STATIC_LIB)
        #warning "NKCore: NKENTSEU_CORE_BUILD_SHARED_LIB et NKENTSEU_CORE_STATIC_LIB définis - NKENTSEU_CORE_STATIC_LIB ignoré"
        #undef NKENTSEU_CORE_STATIC_LIB
    #endif

    #if defined(NKENTSEU_CORE_BUILD_SHARED_LIB) && defined(NKENTSEU_CORE_HEADER_ONLY)
        #warning "NKCore: NKENTSEU_CORE_BUILD_SHARED_LIB et NKENTSEU_CORE_HEADER_ONLY définis - NKENTSEU_CORE_HEADER_ONLY ignoré"
        #undef NKENTSEU_CORE_HEADER_ONLY
    #endif

    #if defined(NKENTSEU_CORE_STATIC_LIB) && defined(NKENTSEU_CORE_HEADER_ONLY)
        #warning "NKCore: NKENTSEU_CORE_STATIC_LIB et NKENTSEU_CORE_HEADER_ONLY définis - NKENTSEU_CORE_HEADER_ONLY ignoré"
        #undef NKENTSEU_CORE_HEADER_ONLY
    #endif

    // -------------------------------------------------------------------------
    // SECTION 7 : MESSAGES DE DEBUG OPTIONNELS
    // -------------------------------------------------------------------------
    #ifdef NKENTSEU_CORE_DEBUG
        #pragma message("NKCore Export Config:")
        #if defined(NKENTSEU_CORE_BUILD_SHARED_LIB)
            #pragma message("  NKCore mode: Shared (export)")
        #elif defined(NKENTSEU_CORE_STATIC_LIB)
            #pragma message("  NKCore mode: Static")
        #elif defined(NKENTSEU_CORE_HEADER_ONLY)
            #pragma message("  NKCore mode: Header-only")
        #else
            #pragma message("  NKCore mode: DLL import (default)")
        #endif
        #pragma message("  NKENTSEU_CORE_API = " NKENTSEU_STRINGIZE(NKENTSEU_CORE_API))
    #endif

#endif // NKENTSEU_CORE_NKCOREAPI_H

// =============================================================================
// EXEMPLES D'UTILISATION
// =============================================================================
/*
    // Exemple 1 : Déclaration d'une fonction publique NKCore
    #include "NKCore/NkCoreApi.h"

    NKENTSEU_CORE_EXTERN_C_BEGIN
    NKENTSEU_CORE_API void Core_Initialize(void);
    NKENTSEU_CORE_API void Core_Shutdown(void);
    NKENTSEU_CORE_EXTERN_C_END

    // Exemple 2 : Déclaration d'une classe avec dépréciation (via NKPlatform)
    #include "NKCore/NkCoreApi.h"

    class NKENTSEU_CORE_CLASS_EXPORT CoreManager {
    public:
        CoreManager();
        ~CoreManager();

        // Dépréciation via macro NKPlatform (pas de duplication)
        NKENTSEU_DEPRECATED_MESSAGE("Utiliser Initialize(const Config&)")
        bool Initialize(const char* path);

        void Shutdown();

        // Fonction inline critique
        NKENTSEU_CORE_API_FORCE_INLINE int GetVersion() const {
            return 0x010200;
        }
    };

    // Exemple 3 : Alignement via macro NKPlatform (pas de duplication)
    struct NKENTSEU_ALIGN_32 NKENTSEU_CORE_CLASS_EXPORT SimdBuffer {
        float data[16];  // 64 bytes, aligné pour AVX
    };

    // Exemple 4 : Modes de build indépendants (CMake)
    // NKPlatform en DLL, NKCore en static :
    //   target_compile_definitions(nkcore PRIVATE NKENTSEU_CORE_STATIC_LIB)
    //   target_compile_definitions(nkcore PRIVATE NKENTSEU_BUILD_SHARED_LIB)  # Pour NKPlatform
    //
    // NKPlatform en static, NKCore en DLL :
    //   target_compile_definitions(nkcore PRIVATE NKENTSEU_CORE_BUILD_SHARED_LIB)
    //   target_compile_definitions(nkcore PRIVATE NKENTSEU_STATIC_LIB)  # Pour NKPlatform

    // Exemple 5 : Utilisation dans le code client
    #include <NKCore/CoreManager.h>

    int main() {
        nkentseu::core::CoreManager manager;
        if (manager.Initialize("config.ini")) {
            // ...
            manager.Shutdown();
        }
        return 0;
    }
*/

// =============================================================================
// EXEMPLES D'UTILISATION DE NKCOREAPI.H
// =============================================================================
// Ce fichier fournit les macros pour gérer la visibilité des symboles du module
// NKCore, avec support multiplateforme et multi-compilateur.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Déclaration d'une fonction publique NKCore
// -----------------------------------------------------------------------------
/*
    // Dans nkcore_public.h :
    #include "NKCore/NkCoreApi.h"

    NKENTSEU_CORE_EXTERN_C_BEGIN

    // Fonction C publique exportée
    NKENTSEU_CORE_API void Core_Initialize(void);
    NKENTSEU_CORE_API void Core_Shutdown(void);
    NKENTSEU_CORE_API int Core_GetVersion(void);

    NKENTSEU_CORE_EXTERN_C_END

    // Dans nkcore.cpp :
    #include "nkcore_public.h"

    void Core_Initialize(void) {
        // Implémentation...
    }

    void Core_Shutdown(void) {
        // Implémentation...
    }

    int Core_GetVersion(void) {
        return 0x010200;  // Version 1.2.0
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Déclaration d'une classe publique NKCore
// -----------------------------------------------------------------------------
/*
    // Dans CoreManager.h :
    #include "NKCore/NkCoreApi.h"

    class NKENTSEU_CORE_CLASS_EXPORT CoreManager {
    public:
        NKENTSEU_CORE_API CoreManager();
        NKENTSEU_CORE_API ~CoreManager();

        NKENTSEU_CORE_API bool Initialize(const char* configPath);
        NKENTSEU_CORE_API void Shutdown();
        NKENTSEU_CORE_API bool IsInitialized() const;

        // Fonction inline exportée pour performance
        NKENTSEU_CORE_API_FORCE_INLINE const char* GetAppName() const {
            return m_appName;
        }

    private:
        // Méthode privée : pas d'export nécessaire
        void InternalCleanup();

        char* m_appName;
        bool m_initialized;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Gestion des modes de build via CMake
// -----------------------------------------------------------------------------
/*
    # CMakeLists.txt pour NKCore

    cmake_minimum_required(VERSION 3.15)
    project(NKCore VERSION 1.2.0)

    # Options de build
    option(NKCORE_BUILD_SHARED "Build NKCore as shared library" ON)
    option(NKCORE_HEADER_ONLY "Use NKCore in header-only mode" OFF)

    # Configuration des defines
    if(NKCORE_HEADER_ONLY)
        add_definitions(-DNKENTSEU_CORE_HEADER_ONLY)
    elseif(NKCORE_BUILD_SHARED)
        add_definitions(-DNKENTSEU_CORE_BUILD_SHARED_LIB)
        set(NKCORE_LIBRARY_TYPE SHARED)
    else()
        add_definitions(-DNKENTSEU_CORE_STATIC_LIB)
        set(NKCORE_LIBRARY_TYPE STATIC)
    endif()

    # Création de la bibliothèque
    add_library(nkcore ${NKCORE_LIBRARY_TYPE}
        src/CoreManager.cpp
        src/CoreLogger.cpp
        # ... autres fichiers sources
    )

    # Installation des en-têtes publics
    install(DIRECTORY include/NKCore DESTINATION include)

    # Pour les consommateurs de NKCore
    target_include_directories(nkcore PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Utilisation de NKCore dans une application cliente
// -----------------------------------------------------------------------------
/*
    // Dans l'application cliente (CMakeLists.txt) :

    find_package(NKCore REQUIRED)

    add_executable(monapp main.cpp)
    target_link_libraries(monapp PRIVATE NKCore::NKCore)

    # Si NKCore est en mode DLL, le define d'import est géré par le package
    # Sinon, définir manuellement si nécessaire :
    # target_compile_definitions(monapp PRIVATE NKENTSEU_CORE_STATIC_LIB)

    // Dans main.cpp :
    #include <NKCore/CoreManager.h>

    int main() {
        nkentseu::core::CoreManager manager;

        if (manager.Initialize("config.ini")) {
            // Utiliser le core...
            manager.Shutdown();
        }

        return 0;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Mode header-only pour NKCore
// -----------------------------------------------------------------------------
/*
    // Pour utiliser NKCore en mode header-only :

    // 1. Définir la macro avant toute inclusion
    #define NKENTSEU_CORE_HEADER_ONLY
    #include <NKCore/NkCoreApi.h>
    #include <NKCore/CoreManager.h>

    // 2. Toutes les fonctions sont inline, pas de linkage nécessaire
    // 3. Idéal pour les petits projets ou l'intégration directe

    void QuickStart() {
        nkentseu::core::CoreManager manager;  // Tout est inline
        // ... utilisation ...
    }

    // Note : Le mode header-only peut augmenter la taille du binaire
    // car le code est dupliqué dans chaque unité de traduction.
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Dépréciation et migration d'API NKCore
// -----------------------------------------------------------------------------
/*
    #include <NKCore/NkCoreApi.h>

    // Ancienne API dépréciée
    NKENTSEU_CORE_DEPRECATED_MESSAGE("Utiliser CoreManager::Initialize()")
    NKENTSEU_CORE_API void Legacy_Init(const char* path);

    // Nouvelle API recommandée
    NKENTSEU_CORE_API bool CoreManager_Initialize(const char* configPath);

    // Dans le code client :
    void MigrateCode() {
        // Ceci génère un warning de dépréciation :
        // Legacy_Init("old_config.ini");

        // Utiliser la nouvelle API :
        CoreManager_Initialize("new_config.ini");
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Combinaison avec NKPlatform pour code multi-module
// -----------------------------------------------------------------------------
/*
    #include <NKPlatform/NkPlatformDetect.h>   // NKENTSEU_PLATFORM_*
    #include <NKPlatform/NkPlatformExport.h>   // NKENTSEU_PLATFORM_API
    #include <NKCore/NkCoreApi.h>              // NKENTSEU_CORE_API

    // Fonction qui utilise les deux modules
    NKENTSEU_CORE_API void CrossModuleInit() {
        // Logging via NKPlatform
        NK_FOUNDATION_LOG_INFO("Initializing cross-module...");

        // Configuration via NKCore
        #if defined(NKENTSEU_CORE_BUILD_SHARED_LIB)
            NK_FOUNDATION_LOG_DEBUG("NKCore mode: Shared DLL");
        #elif defined(NKENTSEU_CORE_STATIC_LIB)
            NK_FOUNDATION_LOG_DEBUG("NKCore mode: Static library");
        #endif

        // Code spécifique à la plateforme
        #ifdef NKENTSEU_PLATFORM_WINDOWS
            InitializeWindowsSpecific();
        #elif defined(NKENTSEU_PLATFORM_LINUX)
            InitializeLinuxSpecific();
        #endif
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================