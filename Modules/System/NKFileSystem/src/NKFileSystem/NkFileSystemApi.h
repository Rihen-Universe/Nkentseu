// =============================================================================
// NKFileSystem/NkFileSystemApi.h
// Gestion de la visibilité des symboles et de l'API publique du module NKFileSystem.
//
// Design :
//  - Réutilisation DIRECTE des macros de NKPlatform et NKCore (ZÉRO duplication)
//  - Macros spécifiques NKFileSystem uniquement pour la configuration de build
//  - Indépendance totale des modes de build entre modules
//  - Compatibilité multiplateforme et multi-compilateur via NKPlatform
//  - Aucune redéfinition de deprecated, alignement, pragma, etc.
//
// Règles d'application des macros :
//  - NKENTSEU_FILESYSTEM_API sur les fonctions libres et variables globales publiques
//  - PAS sur les méthodes de classes (l'export de la classe suffit)
//  - PAS sur les classes/structs/templates (géré par NKENTSEU_FILESYSTEM_CLASS_EXPORT)
//  - PAS sur les fonctions inline définies dans les headers
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_FILESYSTEM_NKFILESYSTEMAPI_H
#define NKENTSEU_FILESYSTEM_NKFILESYSTEMAPI_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // NKFileSystem dépend de NKCore et NKPlatform.
    // Nous importons leurs macros d'export pour assurer la cohérence.
    // AUCUNE duplication : nous utilisons directement les macros existantes.

    #include "NKCore/NkCoreApi.h"
    #include "NKPlatform/NkPlatformExport.h"
    #include "NKPlatform/NkPlatformInline.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION DU MODE DE BUILD NKFILESYSTEM
    // -------------------------------------------------------------------------
    /**
     * @defgroup FileSystemBuildConfig Configuration du Build NKFileSystem
     * @brief Macros pour contrôler le mode de compilation de NKFileSystem
     *
     * Ces macros sont INDÉPENDANTES de celles de NKCore et NKPlatform :
     *  - NKENTSEU_FILESYSTEM_BUILD_SHARED_LIB : Compiler NKFileSystem en DLL
     *  - NKENTSEU_FILESYSTEM_STATIC_LIB : Utiliser NKFileSystem en mode static
     *  - NKENTSEU_FILESYSTEM_HEADER_ONLY : Mode header-only (tout inline)
     *
     * @note NKFileSystem, NKCore et NKPlatform peuvent avoir des modes de build différents.
     *
     * @example CMakeLists.txt
     * @code
     * # NKFileSystem en DLL
     * target_compile_definitions(nkfilesystem PRIVATE NKENTSEU_FILESYSTEM_BUILD_SHARED_LIB)
     *
     * # NKFileSystem en static
     * target_compile_definitions(monapp PRIVATE NKENTSEU_FILESYSTEM_STATIC_LIB)
     * @endcode
     */

    // -------------------------------------------------------------------------
    // SECTION 3 : MACRO PRINCIPALE NKENTSEU_FILESYSTEM_API
    // -------------------------------------------------------------------------
    /**
     * @brief Macro principale pour l'export/import des symboles NKFileSystem
     * @def NKENTSEU_FILESYSTEM_API
     * @ingroup FileSystemApiMacros
     *
     * Cette macro gère UNIQUEMENT la visibilité des symboles de NKFileSystem.
     * Elle est indépendante de NKENTSEU_CORE_API et NKENTSEU_PLATFORM_API.
     *
     * Logique :
     *  - NKENTSEU_FILESYSTEM_BUILD_SHARED_LIB : export (compilation en DLL)
     *  - NKENTSEU_FILESYSTEM_STATIC_LIB ou NKENTSEU_FILESYSTEM_HEADER_ONLY : vide
     *  - Sinon : import (utilisation en mode DLL)
     *
     * @note Cette macro utilise NKENTSEU_PLATFORM_API_EXPORT/IMPORT comme base.
     */
    #if defined(NKENTSEU_FILESYSTEM_BUILD_SHARED_LIB)
        // Compilation de NKFileSystem en bibliothèque partagée : exporter les symboles
        #define NKENTSEU_FILESYSTEM_API NKENTSEU_PLATFORM_API_EXPORT
    #elif defined(NKENTSEU_FILESYSTEM_STATIC_LIB) || defined(NKENTSEU_FILESYSTEM_HEADER_ONLY)
        // Build statique ou header-only : aucune décoration de symbole nécessaire
        #define NKENTSEU_FILESYSTEM_API
    #else
        // Utilisation de NKFileSystem en mode DLL : importer les symboles
        #define NKENTSEU_FILESYSTEM_API NKENTSEU_PLATFORM_API_IMPORT
    #endif

    // -------------------------------------------------------------------------
    // SECTION 4 : MACROS DE CONVENANCE SPÉCIFIQUES À NKFILESYSTEM
    // -------------------------------------------------------------------------
    // Seules les macros qui ajoutent une valeur spécifique à NKFileSystem sont définies.

    /**
     * @brief Macro pour exporter une classe complète de NKFileSystem
     * @def NKENTSEU_FILESYSTEM_CLASS_EXPORT
     * @ingroup FileSystemApiMacros
     *
     * Alias vers NKENTSEU_FILESYSTEM_API pour les déclarations de classe.
     * Les méthodes de la classe n'ont PAS besoin de NKENTSEU_FILESYSTEM_API.
     *
     * @example
     * @code
     * class NKENTSEU_FILESYSTEM_CLASS_EXPORT FileSystemManager {
     * public:
     *     void Initialize();  // Pas de macro ici
     * };
     * @endcode
     */
    #define NKENTSEU_FILESYSTEM_CLASS_EXPORT NKENTSEU_FILESYSTEM_API

    /**
     * @brief Fonction inline exportée pour NKFileSystem
     * @def NKENTSEU_FILESYSTEM_API_INLINE
     * @ingroup FileSystemApiMacros
     *
     * Combinaison de NKENTSEU_FILESYSTEM_API et NKENTSEU_INLINE.
     * En mode header-only, équivaut à NKENTSEU_FORCE_INLINE.
     */
    #if defined(NKENTSEU_FILESYSTEM_HEADER_ONLY)
        // Mode header-only : forcer l'inlining pour éviter les erreurs de linkage
        #define NKENTSEU_FILESYSTEM_API_INLINE NKENTSEU_FORCE_INLINE
    #else
        // Mode bibliothèque : combiner export et inline hint
        #define NKENTSEU_FILESYSTEM_API_INLINE NKENTSEU_FILESYSTEM_API NKENTSEU_INLINE
    #endif

    /**
     * @brief Fonction force_inline exportée pour NKFileSystem
     * @def NKENTSEU_FILESYSTEM_API_FORCE_INLINE
     * @ingroup FileSystemApiMacros
     *
     * Combinaison de NKENTSEU_FILESYSTEM_API et NKENTSEU_FORCE_INLINE.
     * Pour les fonctions critiques en performance dans l'API publique.
     */
    #if defined(NKENTSEU_FILESYSTEM_HEADER_ONLY)
        // Mode header-only : NKENTSEU_FORCE_INLINE suffit
        #define NKENTSEU_FILESYSTEM_API_FORCE_INLINE NKENTSEU_FORCE_INLINE
    #else
        // Mode bibliothèque : combiner export et force inline
        #define NKENTSEU_FILESYSTEM_API_FORCE_INLINE NKENTSEU_FILESYSTEM_API NKENTSEU_FORCE_INLINE
    #endif

    /**
     * @brief Fonction no_inline exportée pour NKFileSystem
     * @def NKENTSEU_FILESYSTEM_API_NO_INLINE
     * @ingroup FileSystemApiMacros
     *
     * Combinaison de NKENTSEU_FILESYSTEM_API et NKENTSEU_NO_INLINE.
     * Pour éviter l'explosion de code dans les DLL ou faciliter le debugging.
     */
    #define NKENTSEU_FILESYSTEM_API_NO_INLINE NKENTSEU_FILESYSTEM_API NKENTSEU_NO_INLINE

    // -------------------------------------------------------------------------
    // SECTION 5 : UTILISATION DIRECTE DES MACROS NKPLATFORM (PAS DE DUPLICATION)
    // -------------------------------------------------------------------------
    /**
     * @defgroup FileSystemApiReuse Réutilisation des Macros NKPlatform
     * @brief Guide d'utilisation des macros NKPlatform dans NKFileSystem
     *
     * Pour éviter la duplication, NKFileSystem réutilise directement les macros
     * de NKPlatform. Voici les équivalences recommandées :
     *
     * | Besoin dans NKFileSystem | Macro à utiliser | Source |
     * |-------------------------|-----------------|--------|
     * | Dépréciation | `NKENTSEU_DEPRECATED` | NKPlatform |
     * | Dépréciation avec message | `NKENTSEU_DEPRECATED_MESSAGE(msg)` | NKPlatform |
     * | Alignement cache | `NKENTSEU_ALIGN_CACHE` | NKPlatform |
     * | Alignement 16/32/64 | `NKENTSEU_ALIGN_16`, etc. | NKPlatform |
     * | Liaison C | `NKENTSEU_EXTERN_C_BEGIN/END` | NKPlatform |
     * | Pragmas | `NKENTSEU_PRAGMA(x)` | NKPlatform |
     * | Gestion warnings | `NKENTSEU_DISABLE_WARNING_*` | NKPlatform |
     *
     * @example
     * @code
     * // Dépréciation dans NKFileSystem :
     * NKENTSEU_DEPRECATED_MESSAGE("Utiliser NewPathResolver()")
     * NKENTSEU_FILESYSTEM_API void LegacyPathResolver();
     * @endcode
     */

    // -------------------------------------------------------------------------
    // SECTION 6 : VALIDATION DES MACROS DE BUILD NKFILESYSTEM
    // -------------------------------------------------------------------------
    // Vérifications de cohérence pour les macros spécifiques à NKFileSystem.

    #if defined(NKENTSEU_FILESYSTEM_BUILD_SHARED_LIB) && defined(NKENTSEU_FILESYSTEM_STATIC_LIB)
        // Conflit : shared et static ne peuvent coexister
        #warning "NKFileSystem: NKENTSEU_FILESYSTEM_BUILD_SHARED_LIB et NKENTSEU_FILESYSTEM_STATIC_LIB définis - NKENTSEU_FILESYSTEM_STATIC_LIB ignoré"
        #undef NKENTSEU_FILESYSTEM_STATIC_LIB
    #endif

    #if defined(NKENTSEU_FILESYSTEM_BUILD_SHARED_LIB) && defined(NKENTSEU_FILESYSTEM_HEADER_ONLY)
        // Conflit : shared et header-only sont mutuellement exclusifs
        #warning "NKFileSystem: NKENTSEU_FILESYSTEM_BUILD_SHARED_LIB et NKENTSEU_FILESYSTEM_HEADER_ONLY définis - NKENTSEU_FILESYSTEM_HEADER_ONLY ignoré"
        #undef NKENTSEU_FILESYSTEM_HEADER_ONLY
    #endif

    #if defined(NKENTSEU_FILESYSTEM_STATIC_LIB) && defined(NKENTSEU_FILESYSTEM_HEADER_ONLY)
        // Conflit : static et header-only ne peuvent coexister
        #warning "NKFileSystem: NKENTSEU_FILESYSTEM_STATIC_LIB et NKENTSEU_FILESYSTEM_HEADER_ONLY définis - NKENTSEU_FILESYSTEM_HEADER_ONLY ignoré"
        #undef NKENTSEU_FILESYSTEM_HEADER_ONLY
    #endif

    // -------------------------------------------------------------------------
    // SECTION 7 : MESSAGES DE DEBUG OPTIONNELS
    // -------------------------------------------------------------------------
    // Affichage des informations de configuration au moment de la compilation.

    #ifdef NKENTSEU_FILESYSTEM_DEBUG
        #pragma message("NKFileSystem Export Config:")
        #if defined(NKENTSEU_FILESYSTEM_BUILD_SHARED_LIB)
            #pragma message("  NKFileSystem mode: Shared (export)")
        #elif defined(NKENTSEU_FILESYSTEM_STATIC_LIB)
            #pragma message("  NKFileSystem mode: Static")
        #elif defined(NKENTSEU_FILESYSTEM_HEADER_ONLY)
            #pragma message("  NKFileSystem mode: Header-only")
        #else
            #pragma message("  NKFileSystem mode: DLL import (default)")
        #endif
        #pragma message("  NKENTSEU_FILESYSTEM_API = " NKENTSEU_STRINGIZE(NKENTSEU_FILESYSTEM_API))
    #endif

#endif // NKENTSEU_FILESYSTEM_NKFILESYSTEMAPI_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKFILESYSTEMAPI.H
// =============================================================================
// Ce fichier fournit les macros pour gérer la visibilité des symboles du module
// NKFileSystem, avec support multiplateforme et multi-compilateur.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Déclaration d'une fonction publique NKFileSystem (API C)
// -----------------------------------------------------------------------------
/*
    // Dans nkfilesystem_public.h :
    #include "NKFileSystem/NkFileSystemApi.h"

    NKENTSEU_FILESYSTEM_EXTERN_C_BEGIN

    // Fonctions C publiques exportées avec liaison C
    NKENTSEU_FILESYSTEM_API void FileSystem_Initialize(void);
    NKENTSEU_FILESYSTEM_API void FileSystem_Shutdown(void);
    NKENTSEU_FILESYSTEM_API int32 FileSystem_GetWorkingDir(char* buffer, int32 size);

    NKENTSEU_FILESYSTEM_EXTERN_C_END
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Déclaration d'une classe publique NKFileSystem (API C++)
// -----------------------------------------------------------------------------
/*
    // Dans PathResolver.h :
    #include "NKFileSystem/NkFileSystemApi.h"
    #include "NKContainers/String/NkString.h"

    namespace nkentseu {
    namespace filesystem {

        // Classe exportée : les méthodes n'ont PAS besoin de NKENTSEU_FILESYSTEM_API
        class NKENTSEU_FILESYSTEM_CLASS_EXPORT PathResolver {
        public:
            // Constructeurs/Destructeur : pas de macro nécessaire
            PathResolver();
            ~PathResolver();

            // Méthodes publiques : pas de macro
            bool SetRootPath(const NkString& path);
            NkString Resolve(const NkString& relativePath) const;
            bool DirectoryExists(const NkString& path) const;

            // Fonction inline dans la classe : pas de macro d'export
            NKENTSEU_FORCE_INLINE bool IsInitialized() const {
                return m_initialized;
            }

        protected:
            // Méthodes protégées : pas de macro non plus
            virtual void OnPathChanged(const NkString& newPath);

        private:
            // Membres privés : pas de macro
            NkString m_rootPath;
            bool m_initialized;
        };

    } // namespace filesystem
    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Fonctions templates et inline (PAS de macro d'export)
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkFileSystemApi.h"

    namespace nkentseu {
    namespace filesystem {

        // Template : JAMAIS de NKENTSEU_FILESYSTEM_API sur les templates
        template<typename PathType>
        class PathValidator {
        public:
            PathValidator() = default;

            // Méthode template : pas de macro d'export
            template<typename CharT>
            static bool Validate(const CharT* path) {
                // Logique de validation...
                return true;
            }
        };

        // Fonction inline définie dans le header : pas de macro
        NKENTSEU_FORCE_INLINE bool IsAbsolutePath(const NkString& path) {
            if (path.Empty()) return false;
            return path.Front() == '/' || path.Front() == '\\';
        }

    } // namespace filesystem
    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Gestion des modes de build via CMake pour NKFileSystem
// -----------------------------------------------------------------------------
/*
    # CMakeLists.txt pour NKFileSystem

    cmake_minimum_required(VERSION 3.15)
    project(NKFileSystem VERSION 1.0.0)

    # Options de build configurables
    option(NKFILESYSTEM_BUILD_SHARED "Build NKFileSystem as shared library" ON)
    option(NKFILESYSTEM_HEADER_ONLY "Use NKFileSystem in header-only mode" OFF)

    # Configuration des defines de build
    if(NKFILESYSTEM_HEADER_ONLY)
        add_definitions(-DNKENTSEU_FILESYSTEM_HEADER_ONLY)
    elseif(NKFILESYSTEM_BUILD_SHARED)
        add_definitions(-DNKENTSEU_FILESYSTEM_BUILD_SHARED_LIB)
        set(NKFILESYSTEM_LIBRARY_TYPE SHARED)
    else()
        add_definitions(-DNKENTSEU_FILESYSTEM_STATIC_LIB)
        set(NKFILESYSTEM_LIBRARY_TYPE STATIC)
    endif()

    # Dépendances requises
    find_package(NKCore REQUIRED)
    find_package(NKPlatform REQUIRED)

    # Création de la bibliothèque NKFileSystem
    add_library(nkfilesystem ${NKFILESYSTEM_LIBRARY_TYPE}
        src/FileSystem.cpp
        src/PathResolver.cpp
        src/DirectoryWatcher.cpp
    )

    # Liaison avec les dépendances
    target_link_libraries(nkfilesystem
        PUBLIC NKCore::NKCore
        PUBLIC NKPlatform::NKPlatform
    )

    # Configuration des include directories
    target_include_directories(nkfilesystem PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Utilisation dans un fichier .cpp (avec pch.h)
// -----------------------------------------------------------------------------
/*
    // Dans PathResolver.cpp :

    #include "pch.h"                        // Precompiled header en premier
    #include "NKFileSystem/NkFileSystemApi.h" // Remplace NkFileSystemExport.h
    #include "NKFileSystem/PathResolver.h"
    #include "NKCore/Assert/NkAssert.h"

    namespace nkentseu {
    namespace filesystem {

        // Implémentation : pas besoin de macros ici
        PathResolver::PathResolver()
            : m_initialized(false)
        {
            // Construction...
        }

        PathResolver::~PathResolver() {
            // Nettoyage...
        }

        bool PathResolver::SetRootPath(const NkString& path) {
            if (path.Empty()) return false;
            m_rootPath = path;
            m_initialized = true;
            return true;
        }

        NkString PathResolver::Resolve(const NkString& relativePath) const {
            NKENTSEU_ASSERT(m_initialized && "PathResolver not initialized");
            return m_rootPath + "/" + relativePath;
        }

        bool PathResolver::DirectoryExists(const NkString& path) const {
            // Implémentation spécifique plateforme...
            return true;
        }

        void PathResolver::OnPathChanged(const NkString& newPath) {
            // Hook pour sous-classes...
        }

    } // namespace filesystem
    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Indentation stricte conforme aux standards NKEntseu
// -----------------------------------------------------------------------------
/*
    // Respect strict des règles d'indentation :

    namespace nkentseu {

        // Namespace imbriqué : indenté d'un niveau
        namespace filesystem {

            // Classe : indentée d'un niveau supplémentaire
            class NKENTSEU_FILESYSTEM_CLASS_EXPORT FileMonitor {
            public:
                // Section public : indentée d'un niveau supplémentaire
                // Une instruction par ligne, pas de macro sur les méthodes
                FileMonitor();
                ~FileMonitor();

                void StartWatching(const NkString& path);
                void StopWatching();

            protected:
                // Section protected : même indentation que public
                virtual void OnFileModified(const NkString& path);

            private:
                // Section private : même indentation
                // Membres : une déclaration par ligne
                void* m_platformHandle;
                NkString m_watchedPath;
                bool m_isActive;
            };

        } // namespace filesystem

        // Bloc conditionnel : contenu indenté
        #if defined(NKENTSEU_FILESYSTEM_ENABLE_ASYNC_IO)

            // Fonction d'export uniquement si async IO activé
            NKENTSEU_FILESYSTEM_API void FileSystem_SubmitAsyncRead(const NkString& path);

        #endif

    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Combinaison avec NKCore et NKPlatform
// -----------------------------------------------------------------------------
/*
    #include <NKPlatform/NkPlatformDetect.h>
    #include <NKPlatform/NkPlatformExport.h>
    #include <NKCore/NkCoreApi.h>
    #include <NKFileSystem/NkFileSystemApi.h>

    namespace nkentseu {
    namespace filesystem {

        // Fonction utilisant les macros des trois modules
        NKENTSEU_FILESYSTEM_API void FileSystem_CrossModuleInit() {

            // Logging via NKCore/NKPlatform
            NK_FOUNDATION_LOG_INFO("[NKFileSystem] Initializing...");

            // Détection du mode de build NKFileSystem
            #if defined(NKENTSEU_FILESYSTEM_BUILD_SHARED_LIB)
                NK_FOUNDATION_LOG_DEBUG("[NKFileSystem] Mode: Shared DLL");
            #elif defined(NKENTSEU_FILESYSTEM_STATIC_LIB)
                NK_FOUNDATION_LOG_DEBUG("[NKFileSystem] Mode: Static library");
            #endif

            // Code spécifique plateforme
            #ifdef NKENTSEU_PLATFORM_WINDOWS
                FileSystem_InitWindowsHandles();
            #elif defined(NKENTSEU_PLATFORM_LINUX)
                FileSystem_InitLinuxFDs();
            #endif

        }

    } // namespace filesystem
    } // namespace nkentseu
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================