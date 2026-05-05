// =============================================================================
// NKReflection/NkReflectionApi.h
// Gestion de la visibilité des symboles et de l'API publique du module NKReflection.
//
// Description :
//  - Système complet de réflexion permettant l'introspection de types,
//    classes, propriétés et méthodes. Support des macros de registration.
//
// Design :
//  - Réutilisation DIRECTE des macros de NKPlatform et NKCore (ZÉRO duplication)
//  - Macros spécifiques NKReflection uniquement pour la configuration de build
//  - Indépendance totale des modes de build entre modules
//  - Compatibilité multiplateforme et multi-compilateur via NKPlatform
//  - Aucune redéfinition de deprecated, alignement, pragma, etc.
//
// Règles d'application des macros :
//  - NKENTSEU_REFLECTION_API sur les fonctions libres et variables globales publiques
//  - PAS sur les méthodes de classes (l'export de la classe suffit)
//  - PAS sur les classes/structs/templates (géré par NKENTSEU_REFLECTION_CLASS_EXPORT)
//  - PAS sur les fonctions inline définies dans les headers
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_REFLECTION_NKREFLECTIONAPI_H
#define NKENTSEU_REFLECTION_NKREFLECTIONAPI_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // NKReflection dépend de NKCore et NKPlatform.
    // Nous importons leurs macros d'export pour assurer la cohérence.
    // AUCUNE duplication : nous utilisons directement les macros existantes.

    #include "NKCore/NkCoreApi.h"
    #include "NKPlatform/NkPlatformExport.h"
    #include "NKPlatform/NkPlatformInline.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION DU MODE DE BUILD NKREFLECTION
    // -------------------------------------------------------------------------
    /**
     * @defgroup ReflectionBuildConfig Configuration du Build NKReflection
     * @brief Macros pour contrôler le mode de compilation de NKReflection
     *
     * Ces macros sont INDÉPENDANTES de celles de NKCore et NKPlatform :
     *  - NKENTSEU_REFLECTION_BUILD_SHARED_LIB : Compiler NKReflection en bibliothèque partagée
     *  - NKENTSEU_REFLECTION_STATIC_LIB : Utiliser NKReflection en mode bibliothèque statique
     *  - NKENTSEU_REFLECTION_HEADER_ONLY : Mode header-only (tout inline)
     *
     * @note NKReflection, NKCore et NKPlatform peuvent avoir des modes de build différents.
     *
     * @example CMakeLists.txt
     * @code
     * # NKReflection en DLL
     * target_compile_definitions(nkreflection PRIVATE NKENTSEU_REFLECTION_BUILD_SHARED_LIB)
     *
     * # NKReflection en static
     * target_compile_definitions(monapp PRIVATE NKENTSEU_REFLECTION_STATIC_LIB)
     * @endcode
     */

    // -------------------------------------------------------------------------
    // SECTION 3 : MACRO PRINCIPALE NKENTSEU_REFLECTION_API
    // -------------------------------------------------------------------------
    /**
     * @brief Macro principale pour l'export/import des symboles NKReflection
     * @def NKENTSEU_REFLECTION_API
     * @ingroup ReflectionApiMacros
     *
     * Cette macro gère UNIQUEMENT la visibilité des symboles de NKReflection.
     * Elle est indépendante de NKENTSEU_CORE_API et NKENTSEU_PLATFORM_API.
     *
     * Logique :
     *  - NKENTSEU_REFLECTION_BUILD_SHARED_LIB : export (compilation de NKReflection en DLL)
     *  - NKENTSEU_REFLECTION_STATIC_LIB ou NKENTSEU_REFLECTION_HEADER_ONLY : vide
     *  - Sinon : import (utilisation de NKReflection en mode DLL)
     *
     * @note Cette macro utilise NKENTSEU_PLATFORM_API_EXPORT/IMPORT comme base.
     */
    #if defined(NKENTSEU_REFLECTION_BUILD_SHARED_LIB)
        // Compilation de NKReflection en bibliothèque partagée : exporter les symboles
        #define NKENTSEU_REFLECTION_API NKENTSEU_PLATFORM_API_EXPORT
    #elif defined(NKENTSEU_REFLECTION_STATIC_LIB) || defined(NKENTSEU_REFLECTION_HEADER_ONLY)
        // Build statique ou header-only : aucune décoration de symbole nécessaire
        #define NKENTSEU_REFLECTION_API
    #else
        // Utilisation de NKReflection en mode DLL : importer les symboles
        #define NKENTSEU_REFLECTION_API NKENTSEU_PLATFORM_API_IMPORT
    #endif

    // -------------------------------------------------------------------------
    // SECTION 4 : MACROS DE CONVENANCE SPÉCIFIQUES À NKREFLECTION
    // -------------------------------------------------------------------------
    // Seules les macros qui ajoutent une valeur spécifique à NKReflection sont définies.

    /**
     * @brief Macro pour exporter une classe complète de NKReflection
     * @def NKENTSEU_REFLECTION_CLASS_EXPORT
     * @ingroup ReflectionApiMacros
     *
     * Alias vers NKENTSEU_REFLECTION_API pour les déclarations de classe.
     * Les méthodes de la classe n'ont PAS besoin de NKENTSEU_REFLECTION_API.
     *
     * @example
     * @code
     * class NKENTSEU_REFLECTION_CLASS_EXPORT ReflectableClass {
     * public:
     *     void RegisterProperty();
     * };
     * @endcode
     */
    #define NKENTSEU_REFLECTION_CLASS_EXPORT NKENTSEU_REFLECTION_API

    /**
     * @brief Fonction inline exportée pour NKReflection
     * @def NKENTSEU_REFLECTION_API_INLINE
     * @ingroup ReflectionApiMacros
     *
     * Combinaison de NKENTSEU_REFLECTION_API et NKENTSEU_INLINE.
     * En mode header-only, équivaut à NKENTSEU_FORCE_INLINE.
     */
    #if defined(NKENTSEU_REFLECTION_HEADER_ONLY)
        // Mode header-only : forcer l'inlining pour éviter les erreurs de linkage
        #define NKENTSEU_REFLECTION_API_INLINE NKENTSEU_FORCE_INLINE
    #else
        // Mode bibliothèque : combiner export et inline hint
        #define NKENTSEU_REFLECTION_API_INLINE NKENTSEU_REFLECTION_API NKENTSEU_INLINE
    #endif

    /**
     * @brief Fonction force_inline exportée pour NKReflection
     * @def NKENTSEU_REFLECTION_API_FORCE_INLINE
     * @ingroup ReflectionApiMacros
     *
     * Combinaison de NKENTSEU_REFLECTION_API et NKENTSEU_FORCE_INLINE.
     * Pour les fonctions critiques en performance dans l'API publique.
     */
    #if defined(NKENTSEU_REFLECTION_HEADER_ONLY)
        // Mode header-only : NKENTSEU_FORCE_INLINE suffit
        #define NKENTSEU_REFLECTION_API_FORCE_INLINE NKENTSEU_FORCE_INLINE
    #else
        // Mode bibliothèque : combiner export et force inline
        #define NKENTSEU_REFLECTION_API_FORCE_INLINE NKENTSEU_REFLECTION_API NKENTSEU_FORCE_INLINE
    #endif

    /**
     * @brief Fonction no_inline exportée pour NKReflection
     * @def NKENTSEU_REFLECTION_API_NO_INLINE
     * @ingroup ReflectionApiMacros
     *
     * Combinaison de NKENTSEU_REFLECTION_API et NKENTSEU_NO_INLINE.
     * Pour éviter l'explosion de code dans les DLL ou faciliter le debugging.
     */
    #define NKENTSEU_REFLECTION_API_NO_INLINE NKENTSEU_REFLECTION_API NKENTSEU_NO_INLINE

    // -------------------------------------------------------------------------
    // SECTION 5 : UTILISATION DIRECTE DES MACROS NKPLATFORM (PAS DE DUPLICATION)
    // -------------------------------------------------------------------------
    /**
     * @defgroup ReflectionApiReuse Réutilisation des Macros NKPlatform
     * @brief Guide d'utilisation des macros NKPlatform dans NKReflection
     *
     * Pour éviter la duplication, NKReflection réutilise directement les macros
     * de NKPlatform. Voici les équivalences :
     *
     * | Besoin dans NKReflection    | Macro à utiliser                    | Source     |
     * |----------------------------|-------------------------------------|------------|
     * | Dépréciation               | NKENTSEU_DEPRECATED                 | NKPlatform |
     * | Dépréciation avec message  | NKENTSEU_DEPRECATED_MESSAGE(msg)    | NKPlatform |
     * | Alignement cache           | NKENTSEU_ALIGN_CACHE                | NKPlatform |
     * | Alignement 16/32/64        | NKENTSEU_ALIGN_16, etc.             | NKPlatform |
     * | Liaison C                  | NKENTSEU_EXTERN_C_BEGIN/END         | NKPlatform |
     * | Pragmas                    | NKENTSEU_PRAGMA(x)                  | NKPlatform |
     * | Gestion warnings           | NKENTSEU_DISABLE_WARNING_*          | NKPlatform |
     *
     * @example
     * @code
     * NKENTSEU_DEPRECATED_MESSAGE("Utiliser NewReflectionAPI()")
     * NKENTSEU_REFLECTION_API void OldReflectionFunction();
     * @endcode
     */

    // -------------------------------------------------------------------------
    // SECTION 6 : VALIDATION DES MACROS DE BUILD NKREFLECTION
    // -------------------------------------------------------------------------
    // Vérifications de cohérence pour les macros spécifiques à NKReflection.

    #if defined(NKENTSEU_REFLECTION_BUILD_SHARED_LIB) && defined(NKENTSEU_REFLECTION_STATIC_LIB)
        // Conflit : shared et static ne peuvent coexister
        #warning "NKReflection: NKENTSEU_REFLECTION_BUILD_SHARED_LIB et NKENTSEU_REFLECTION_STATIC_LIB définis - NKENTSEU_REFLECTION_STATIC_LIB ignoré"
        #undef NKENTSEU_REFLECTION_STATIC_LIB
    #endif

    #if defined(NKENTSEU_REFLECTION_BUILD_SHARED_LIB) && defined(NKENTSEU_REFLECTION_HEADER_ONLY)
        // Conflit : shared et header-only sont mutuellement exclusifs
        #warning "NKReflection: NKENTSEU_REFLECTION_BUILD_SHARED_LIB et NKENTSEU_REFLECTION_HEADER_ONLY définis - NKENTSEU_REFLECTION_HEADER_ONLY ignoré"
        #undef NKENTSEU_REFLECTION_HEADER_ONLY
    #endif

    #if defined(NKENTSEU_REFLECTION_STATIC_LIB) && defined(NKENTSEU_REFLECTION_HEADER_ONLY)
        // Conflit : static et header-only ne peuvent coexister
        #warning "NKReflection: NKENTSEU_REFLECTION_STATIC_LIB et NKENTSEU_REFLECTION_HEADER_ONLY définis - NKENTSEU_REFLECTION_HEADER_ONLY ignoré"
        #undef NKENTSEU_REFLECTION_HEADER_ONLY
    #endif

    // -------------------------------------------------------------------------
    // SECTION 7 : MESSAGES DE DEBUG OPTIONNELS
    // -------------------------------------------------------------------------
    // Affichage des informations de configuration au moment de la compilation.

    #ifdef NKENTSEU_REFLECTION_DEBUG
        #pragma message("NKReflection Export Config:")
        #if defined(NKENTSEU_REFLECTION_BUILD_SHARED_LIB)
            #pragma message("  NKReflection mode: Shared (export)")
        #elif defined(NKENTSEU_REFLECTION_STATIC_LIB)
            #pragma message("  NKReflection mode: Static")
        #elif defined(NKENTSEU_REFLECTION_HEADER_ONLY)
            #pragma message("  NKReflection mode: Header-only")
        #else
            #pragma message("  NKReflection mode: DLL import (default)")
        #endif
        #pragma message("  NKENTSEU_REFLECTION_API = " NKENTSEU_STRINGIZE(NKENTSEU_REFLECTION_API))
    #endif

    #if defined(NKENTSEU_HAS_NODISCARD)
        #define NKREFLECTION_NODISCARD NKENTSEU_NODISCARD
    #else
        #define NKREFLECTION_NODISCARD
    #endif

#endif // NKENTSEU_REFLECTION_NKREFLECTIONAPI_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKREFLECTIONAPI.H
// =============================================================================
// Ce fichier fournit les macros pour gérer la visibilité des symboles du module
// NKReflection, avec support multiplateforme et multi-compilateur.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Déclaration d'une fonction publique NKReflection (API C)
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkReflectionApi.h"

    NKENTSEU_EXTERN_C_BEGIN

    NKENTSEU_REFLECTION_API void Reflection_Initialize(void);
    NKENTSEU_REFLECTION_API void Reflection_Shutdown(void);
    NKENTSEU_REFLECTION_API uint32_t Reflection_GetTypeId(const char* name);

    NKENTSEU_EXTERN_C_END
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Déclaration d'une classe publique NKReflection (API C++)
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkReflectionApi.h"

    namespace nkentseu {
    namespace reflection {

        class NKENTSEU_REFLECTION_CLASS_EXPORT TypeMetadata {
        public:
            TypeMetadata();
            ~TypeMetadata();

            const char* GetName() const;
            size_t GetSize() const;
            bool IsAbstract() const;

        protected:
            void SetName(const char* name);

        private:
            const char* m_name;
            size_t m_size;
            bool m_isAbstract;
        };

    }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Fonctions templates et inline (PAS de macro d'export)
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkReflectionApi.h"

    namespace nkentseu {
    namespace reflection {

        template<typename T>
        class TypeAdapter {
        public:
            TypeAdapter() = default;

            template<typename U>
            auto Convert(U value) -> decltype(value) {
                return value;
            }
        };

        NKENTSEU_FORCE_INLINE const char* Reflection_GetDefaultNamespace() {
            return "nkentseu::reflection";
        }

    }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Gestion des modes de build via CMake pour NKReflection
// -----------------------------------------------------------------------------
/*
    cmake_minimum_required(VERSION 3.15)
    project(NKReflection VERSION 1.0.0)

    option(NKREFLECTION_BUILD_SHARED "Build NKReflection as shared library" ON)
    option(NKREFLECTION_HEADER_ONLY "Use NKReflection in header-only mode" OFF)

    if(NKREFLECTION_HEADER_ONLY)
        add_definitions(-DNKENTSEU_REFLECTION_HEADER_ONLY)
    elseif(NKREFLECTION_BUILD_SHARED)
        add_definitions(-DNKENTSEU_REFLECTION_BUILD_SHARED_LIB)
        set(NKREFLECTION_LIBRARY_TYPE SHARED)
    else()
        add_definitions(-DNKENTSEU_REFLECTION_STATIC_LIB)
        set(NKREFLECTION_LIBRARY_TYPE STATIC)
    endif()

    find_package(NKCore REQUIRED)
    find_package(NKPlatform REQUIRED)

    add_library(nkreflection ${NKREFLECTION_LIBRARY_TYPE}
        src/TypeMetadata.cpp
        src/PropertyInfo.cpp
        src/ReflectionEngine.cpp
    )

    target_link_libraries(nkreflection
        PUBLIC NKCore::NKCore
        PUBLIC NKPlatform::NKPlatform
    )

    target_include_directories(nkreflection PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Utilisation dans un fichier .cpp (avec pch.h)
// -----------------------------------------------------------------------------
/*
    #include "pch.h"
    #include "NKReflection/NkReflectionApi.h"
    #include "NKReflection/TypeMetadata.h"

    namespace nkentseu {
    namespace reflection {

        TypeMetadata::TypeMetadata()
            : m_name(nullptr)
            , m_size(0)
            , m_isAbstract(false)
        {
        }

        TypeMetadata::~TypeMetadata() {
        }

        const char* TypeMetadata::GetName() const {
            return m_name;
        }

        size_t TypeMetadata::GetSize() const {
            return m_size;
        }

        bool TypeMetadata::IsAbstract() const {
            return m_isAbstract;
        }

        void TypeMetadata::SetName(const char* name) {
            m_name = name;
        }

    }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Indentation stricte conforme aux standards NKEntseu
// -----------------------------------------------------------------------------
/*
    namespace nkentseu {

        namespace reflection {

            namespace internal {

                class NKENTSEU_REFLECTION_CLASS_EXPORT RegistryImpl {
                public:
                    RegistryImpl();
                    ~RegistryImpl();

                    void Register(const char* name, void* data);
                    void* Find(const char* name) const;

                protected:
                    void Clear();

                private:
                    std::unordered_map<std::string, void*> m_entries;
                    mutable std::mutex m_mutex;
                };

            }

            #if defined(NKENTSEU_REFLECTION_ENABLE_SERIALIZATION)

                NKENTSEU_REFLECTION_API bool Reflection_Serialize(
                    const void* instance,
                    const TypeMetadata& meta,
                    std::string& output);

            #endif

        }

    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Combinaison avec NKCore et NKPlatform
// -----------------------------------------------------------------------------
/*
    #include <NKPlatform/NkPlatformDetect.h>
    #include <NKPlatform/NkPlatformExport.h>
    #include <NKCore/NkCoreApi.h>
    #include <NKReflection/NkReflectionApi.h>

    namespace nkentseu {
    namespace reflection {

        NKENTSEU_REFLECTION_API void Reflection_CrossModuleInit() {

            NK_FOUNDATION_LOG_INFO("[NKReflection] Initializing...");

            #if defined(NKENTSEU_REFLECTION_BUILD_SHARED_LIB)
                NK_FOUNDATION_LOG_DEBUG("[NKReflection] Mode: Shared DLL");
            #elif defined(NKENTSEU_REFLECTION_STATIC_LIB)
                NK_FOUNDATION_LOG_DEBUG("[NKReflection] Mode: Static library");
            #endif

            #ifdef NKENTSEU_PLATFORM_WINDOWS
                Reflection_InitializeWindowsSupport();
            #elif defined(NKENTSEU_PLATFORM_LINUX)
                Reflection_InitializeLinuxSupport();
            #endif

        }

    }
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================