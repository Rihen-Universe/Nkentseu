// =============================================================================
// NKSerialization/NkSerializationApi.h
// Gestion de la visibilité des symboles et de l'API publique du module NKSerialization.
//
// Design :
//  - Réutilisation DIRECTE des macros de NKPlatform et NKCore (ZÉRO duplication)
//  - Macros spécifiques NKSerialization uniquement pour la configuration de build
//  - Indépendance totale des modes de build entre modules
//  - Compatibilité multiplateforme et multi-compilateur via NKPlatform
//  - Aucune redéfinition de deprecated, alignement, pragma, etc.
//
// Règles d'application des macros :
//  - NKENTSEU_SERIALIZATION_API sur les fonctions libres et variables globales publiques
//  - PAS sur les méthodes de classes (l'export de la classe suffit)
//  - PAS sur les classes/structs/templates (géré par NKENTSEU_SERIALIZATION_CLASS_EXPORT)
//  - PAS sur les fonctions inline définies dans les headers
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_SERIALIZATION_NKSERIALIZATIONAPI_H
#define NKENTSEU_SERIALIZATION_NKSERIALIZATIONAPI_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // NKSerialization dépend de NKCore et NKPlatform.
    // Nous importons leurs macros d'export pour assurer la cohérence.
    // AUCUNE duplication : nous utilisons directement les macros existantes.

    #include "NKCore/NkCoreApi.h"
    #include "NKPlatform/NkPlatformExport.h"
    #include "NKPlatform/NkPlatformInline.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION DU MODE DE BUILD NKSÉRIALISATION
    // -------------------------------------------------------------------------
    /**
     * @defgroup SerializationBuildConfig Configuration du Build NKSerialization
     * @brief Macros pour contrôler le mode de compilation de NKSerialization
     *
     * Ces macros sont INDÉPENDANTES de celles de NKCore et NKPlatform :
     *  - NKENTSEU_SERIALIZATION_BUILD_SHARED_LIB : Compiler en bibliothèque partagée
     *  - NKENTSEU_SERIALIZATION_STATIC_LIB : Utiliser en mode bibliothèque statique
     *  - NKENTSEU_SERIALIZATION_HEADER_ONLY : Mode header-only (tout inline)
     *
     * @note NKSerialization, NKCore et NKPlatform peuvent avoir des modes de build différents.
     *
     * @example CMakeLists.txt
     * @code
     * # NKSerialization en DLL
     * target_compile_definitions(nkserialization PRIVATE NKENTSEU_SERIALIZATION_BUILD_SHARED_LIB)
     *
     * # NKSerialization en static
     * target_compile_definitions(monapp PRIVATE NKENTSEU_SERIALIZATION_STATIC_LIB)
     * @endcode
     */

    // -------------------------------------------------------------------------
    // SECTION 3 : MACRO PRINCIPALE NKENTSEU_SERIALIZATION_API
    // -------------------------------------------------------------------------
    /**
     * @brief Macro principale pour l'export/import des symboles NKSerialization
     * @def NKENTSEU_SERIALIZATION_API
     * @ingroup SerializationApiMacros
     *
     * Cette macro gère UNIQUEMENT la visibilité des symboles de NKSerialization.
     * Elle est indépendante de NKENTSEU_CORE_API et NKENTSEU_PLATFORM_API.
     *
     * Logique :
     *  - NKENTSEU_SERIALIZATION_BUILD_SHARED_LIB : export (compilation en DLL)
     *  - NKENTSEU_SERIALIZATION_STATIC_LIB ou NKENTSEU_SERIALIZATION_HEADER_ONLY : vide
     *  - Sinon : import (utilisation en mode DLL)
     *
     * @note Cette macro utilise NKENTSEU_PLATFORM_API_EXPORT/IMPORT comme base.
     */
    #if defined(NKENTSEU_SERIALIZATION_BUILD_SHARED_LIB)
        // Compilation de NKSerialization en bibliothèque partagée : exporter les symboles
        #define NKENTSEU_SERIALIZATION_API NKENTSEU_PLATFORM_API_EXPORT
    #elif defined(NKENTSEU_SERIALIZATION_STATIC_LIB) || defined(NKENTSEU_SERIALIZATION_HEADER_ONLY)
        // Build statique ou header-only : aucune décoration de symbole nécessaire
        #define NKENTSEU_SERIALIZATION_API
    #else
        // Utilisation de NKSerialization en mode DLL : importer les symboles
        #define NKENTSEU_SERIALIZATION_API NKENTSEU_PLATFORM_API_IMPORT
    #endif

    // -------------------------------------------------------------------------
    // SECTION 4 : MACROS DE CONVENANCE SPÉCIFIQUES À NKSÉRIALISATION
    // -------------------------------------------------------------------------
    // Seules les macros qui ajoutent une valeur spécifique à NKSerialization sont définies.

    /**
     * @brief Macro pour exporter une classe complète de NKSerialization
     * @def NKENTSEU_SERIALIZATION_CLASS_EXPORT
     * @ingroup SerializationApiMacros
     *
     * Alias vers NKENTSEU_SERIALIZATION_API pour les déclarations de classe.
     * Les méthodes de la classe n'ont PAS besoin de NKENTSEU_SERIALIZATION_API.
     *
     * @example
     * @code
     * class NKENTSEU_SERIALIZATION_CLASS_EXPORT Serializer {
     * public:
     *     void Serialize();  // Pas de NKENTSEU_SERIALIZATION_API ici
     * };
     * @endcode
     */
    #define NKENTSEU_SERIALIZATION_CLASS_EXPORT NKENTSEU_SERIALIZATION_API

    /**
     * @brief Fonction inline exportée pour NKSerialization
     * @def NKENTSEU_SERIALIZATION_API_INLINE
     * @ingroup SerializationApiMacros
     *
     * Combinaison de NKENTSEU_SERIALIZATION_API et NKENTSEU_INLINE.
     * En mode header-only, équivaut à NKENTSEU_FORCE_INLINE.
     */
    #if defined(NKENTSEU_SERIALIZATION_HEADER_ONLY)
        // Mode header-only : forcer l'inlining pour éviter les erreurs de linkage
        #define NKENTSEU_SERIALIZATION_API_INLINE NKENTSEU_FORCE_INLINE
    #else
        // Mode bibliothèque : combiner export et inline hint
        #define NKENTSEU_SERIALIZATION_API_INLINE NKENTSEU_SERIALIZATION_API NKENTSEU_INLINE
    #endif

    /**
     * @brief Fonction force_inline exportée pour NKSerialization
     * @def NKENTSEU_SERIALIZATION_API_FORCE_INLINE
     * @ingroup SerializationApiMacros
     *
     * Combinaison de NKENTSEU_SERIALIZATION_API et NKENTSEU_FORCE_INLINE.
     * Pour les fonctions critiques en performance dans l'API publique.
     */
    #if defined(NKENTSEU_SERIALIZATION_HEADER_ONLY)
        // Mode header-only : NKENTSEU_FORCE_INLINE suffit
        #define NKENTSEU_SERIALIZATION_API_FORCE_INLINE NKENTSEU_FORCE_INLINE
    #else
        // Mode bibliothèque : combiner export et force inline
        #define NKENTSEU_SERIALIZATION_API_FORCE_INLINE NKENTSEU_SERIALIZATION_API NKENTSEU_FORCE_INLINE
    #endif

    /**
     * @brief Fonction no_inline exportée pour NKSerialization
     * @def NKENTSEU_SERIALIZATION_API_NO_INLINE
     * @ingroup SerializationApiMacros
     *
     * Combinaison de NKENTSEU_SERIALIZATION_API et NKENTSEU_NO_INLINE.
     * Pour éviter l'explosion de code dans les DLL ou faciliter le debugging.
     */
    #define NKENTSEU_SERIALIZATION_API_NO_INLINE NKENTSEU_SERIALIZATION_API NKENTSEU_NO_INLINE

    // -------------------------------------------------------------------------
    // SECTION 5 : UTILISATION DIRECTE DES MACROS NKPLATFORM (PAS DE DUPLICATION)
    // -------------------------------------------------------------------------
    /**
     * @defgroup SerializationApiReuse Réutilisation des Macros NKPlatform
     * @brief Guide d'utilisation des macros NKPlatform dans NKSerialization
     *
     * Pour éviter la duplication, NKSerialization réutilise directement les macros
     * de NKPlatform. Voici les équivalences :
     *
     * | Besoin dans NKSerialization | Macro à utiliser | Source |
     * |---------------------------|-----------------|--------|
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
     * // Dépréciation dans NKSerialization :
     * NKENTSEU_DEPRECATED_MESSAGE("Utiliser NewSerializeFunction()")
     * NKENTSEU_SERIALIZATION_API void OldSerializeFunction();
     * @endcode
     */

    // -------------------------------------------------------------------------
    // SECTION 6 : VALIDATION DES MACROS DE BUILD NKSÉRIALISATION
    // -------------------------------------------------------------------------
    // Vérifications de cohérence pour les macros spécifiques à NKSerialization.

    #if defined(NKENTSEU_SERIALIZATION_BUILD_SHARED_LIB) && defined(NKENTSEU_SERIALIZATION_STATIC_LIB)
        // Conflit : shared et static ne peuvent coexister
        #warning "NKSerialization: NKENTSEU_SERIALIZATION_BUILD_SHARED_LIB et NKENTSEU_SERIALIZATION_STATIC_LIB définis - NKENTSEU_SERIALIZATION_STATIC_LIB ignoré"
        #undef NKENTSEU_SERIALIZATION_STATIC_LIB
    #endif

    #if defined(NKENTSEU_SERIALIZATION_BUILD_SHARED_LIB) && defined(NKENTSEU_SERIALIZATION_HEADER_ONLY)
        // Conflit : shared et header-only sont mutuellement exclusifs
        #warning "NKSerialization: NKENTSEU_SERIALIZATION_BUILD_SHARED_LIB et NKENTSEU_SERIALIZATION_HEADER_ONLY définis - NKENTSEU_SERIALIZATION_HEADER_ONLY ignoré"
        #undef NKENTSEU_SERIALIZATION_HEADER_ONLY
    #endif

    #if defined(NKENTSEU_SERIALIZATION_STATIC_LIB) && defined(NKENTSEU_SERIALIZATION_HEADER_ONLY)
        // Conflit : static et header-only ne peuvent coexister
        #warning "NKSerialization: NKENTSEU_SERIALIZATION_STATIC_LIB et NKENTSEU_SERIALIZATION_HEADER_ONLY définis - NKENTSEU_SERIALIZATION_HEADER_ONLY ignoré"
        #undef NKENTSEU_SERIALIZATION_HEADER_ONLY
    #endif

    // -------------------------------------------------------------------------
    // SECTION 7 : MESSAGES DE DEBUG OPTIONNELS
    // -------------------------------------------------------------------------
    // Affichage des informations de configuration au moment de la compilation.

    #ifdef NKENTSEU_SERIALIZATION_DEBUG
        #pragma message("NKSerialization Export Config:")
        #if defined(NKENTSEU_SERIALIZATION_BUILD_SHARED_LIB)
            #pragma message("  NKSerialization mode: Shared (export)")
        #elif defined(NKENTSEU_SERIALIZATION_STATIC_LIB)
            #pragma message("  NKSerialization mode: Static")
        #elif defined(NKENTSEU_SERIALIZATION_HEADER_ONLY)
            #pragma message("  NKSerialization mode: Header-only")
        #else
            #pragma message("  NKSerialization mode: DLL import (default)")
        #endif
        #pragma message("  NKENTSEU_SERIALIZATION_API = " NKENTSEU_STRINGIZE(NKENTSEU_SERIALIZATION_API))
    #endif

    #if defined(NKENTSEU_HAS_NODISCARD)
        #define NKSERA_NODISCARD NKENTSEU_NODISCARD
    #else
        #define NKSERA_NODISCARD
    #endif

#endif // NKENTSEU_SERIALIZATION_NKSERIALIZATIONAPI_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKSÉRIALIZATIONAPI.H
// =============================================================================
// Ce fichier fournit les macros pour gérer la visibilité des symboles du module
// NKSerialization, avec support multiplateforme et multi-compilateur.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Déclaration d'une fonction publique NKSerialization (API C)
// -----------------------------------------------------------------------------
/*
    // Dans nkserialization_public.h :
    #include "NKSerialization/NkSerializationApi.h"

    NKENTSEU_SERIALIZATION_EXTERN_C_BEGIN

    // Fonctions C publiques exportées avec liaison C
    NKENTSEU_SERIALIZATION_API void Serialization_Initialize(void);
    NKENTSEU_SERIALIZATION_API void Serialization_Shutdown(void);
    NKENTSEU_SERIALIZATION_API int32_t Serialization_RegisterFormat(const char* formatName);

    NKENTSEU_SERIALIZATION_EXTERN_C_END
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Déclaration d'une classe publique NKSerialization (API C++)
// -----------------------------------------------------------------------------
/*
    // Dans Serializer.h :
    #include "NKSerialization/NkSerializationApi.h"

    namespace nkentseu {

        // Classe exportée : les méthodes n'ont PAS besoin de NKENTSEU_SERIALIZATION_API
        class NKENTSEU_SERIALIZATION_CLASS_EXPORT Serializer {
        public:
            // Constructeurs : pas de macro sur les méthodes
            Serializer();
            ~Serializer();

            // Méthodes publiques : pas de NKENTSEU_SERIALIZATION_API nécessaire
            bool LoadFromFile(const char* filePath);
            bool SaveToFile(const char* filePath);
            bool IsValid() const;

            // Fonction inline dans la classe : pas de macro d'export
            NKENTSEU_FORCE_INLINE const char* GetFormat() const {
                return m_format;
            }

        protected:
            // Méthodes protégées : pas de macro non plus
            void ParseInternal();

        private:
            // Membres privés : une déclaration par ligne
            const char* m_format;
            bool m_isValid;
            void* m_internalData;
        };

    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Fonctions templates et inline (PAS de macro d'export)
// -----------------------------------------------------------------------------
/*
    #include "NKSerialization/NkSerializationApi.h"

    namespace nkentseu {

        // Template : JAMAIS de NKENTSEU_SERIALIZATION_API sur les templates
        template<typename DataType>
        class DataAdapter {
        public:
            DataAdapter() = default;

            // Méthode template : pas de macro d'export
            template<typename Format>
            auto Convert(const DataType& data) -> Format {
                return Format(data);
            }
        };

        // Fonction inline définie dans le header : pas de macro
        NKENTSEU_FORCE_INLINE bool Serialization_IsSupported(const char* ext) {
            // Implémentation inline...
            return ext != nullptr;
        }

    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Gestion des modes de build via CMake pour NKSerialization
// -----------------------------------------------------------------------------
/*
    # CMakeLists.txt pour NKSerialization

    cmake_minimum_required(VERSION 3.15)
    project(NKSerialization VERSION 1.0.0)

    # Options de build configurables
    option(NKSERA_BUILD_SHARED "Build NKSerialization as shared library" ON)
    option(NKSERA_HEADER_ONLY "Use NKSerialization in header-only mode" OFF)

    # Configuration des defines de build
    if(NKSERA_HEADER_ONLY)
        add_definitions(-DNKENTSEU_SERIALIZATION_HEADER_ONLY)
    elseif(NKSERA_BUILD_SHARED)
        add_definitions(-DNKENTSEU_SERIALIZATION_BUILD_SHARED_LIB)
        set(NKSERA_LIBRARY_TYPE SHARED)
    else()
        add_definitions(-DNKENTSEU_SERIALIZATION_STATIC_LIB)
        set(NKSERA_LIBRARY_TYPE STATIC)
    endif()

    # Dépendances requises
    find_package(NKCore REQUIRED)
    find_package(NKPlatform REQUIRED)

    # Création de la bibliothèque NKSerialization
    add_library(nkserialization ${NKSERA_LIBRARY_TYPE}
        src/Serializer.cpp
        src/FormatRegistry.cpp
    )

    # Liaison avec les dépendances
    target_link_libraries(nkserialization
        PUBLIC NKCore::NKCore
        PUBLIC NKPlatform::NKPlatform
    )

    # Configuration des include directories
    target_include_directories(nkserialization PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Utilisation dans un fichier .cpp (avec pch.h)
// -----------------------------------------------------------------------------
/*
    // Dans Serializer.cpp :

    #include "pch.h"                              // Precompiled header en premier
    #include "NKSerialization/NkSerializationApi.h" // Remplace NkSerializationExport.h
    #include "NKSerialization/Serializer.h"

    namespace nkentseu {

        // Implémentation : pas besoin de macros ici
        Serializer::Serializer()
            : m_format(nullptr)
            , m_isValid(false)
            , m_internalData(nullptr)
        {
            // Construction...
        }

        Serializer::~Serializer() {
            // Nettoyage des ressources...
        }

        bool Serializer::LoadFromFile(const char* filePath) {
            // Implémentation du chargement...
            return true;
        }

        bool Serializer::SaveToFile(const char* filePath) {
            // Implémentation de la sauvegarde...
            return true;
        }

        bool Serializer::IsValid() const {
            return m_isValid;
        }

        void Serializer::ParseInternal() {
            // Logique interne de parsing...
        }

    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Indentation stricte conforme aux standards NKEntseu
// -----------------------------------------------------------------------------
/*
    // Respect strict des règles d'indentation :

    namespace nkentseu {

        // Namespace imbriqué : indenté d'un niveau
        namespace serialization {

            // Namespace interne : indenté d'un niveau supplémentaire
            namespace internal {

                // Classe : indentée d'un niveau supplémentaire
                class NKENTSEU_SERIALIZATION_CLASS_EXPORT FormatParser {
                public:
                    // Section public : indentée d'un niveau supplémentaire
                    // Une instruction par ligne, pas de macro sur les méthodes
                    FormatParser();
                    ~FormatParser();

                    bool Parse(const uint8_t* data, size_t size);
                    void Reset();

                protected:
                    // Section protected : même indentation que public
                    virtual bool ValidateHeader();

                private:
                    // Section private : même indentation
                    // Membres : une déclaration par ligne
                    const uint8_t* m_data;
                    size_t m_size;
                    size_t m_offset;
                    bool m_isValid;
                };

            } // namespace internal

        } // namespace serialization

        // Bloc conditionnel : contenu indenté
        #if defined(NKENTSEU_SERIALIZATION_ENABLE_COMPRESSION)

            // Fonction d'export uniquement si compression activée
            NKENTSEU_SERIALIZATION_API void Serialization_CompressBuffer(uint8_t* buffer, size_t* size);

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
    #include <NKSerialization/NkSerializationApi.h>

    namespace nkentseu {

        // Fonction utilisant les macros des trois modules
        NKENTSEU_SERIALIZATION_API void Serialization_CrossModuleInit() {

            // Logging via NKPlatform
            NK_FOUNDATION_LOG_INFO("[NKSerialization] Initializing...");

            // Détection du mode de build NKSerialization
            #if defined(NKENTSEU_SERIALIZATION_BUILD_SHARED_LIB)
                NK_FOUNDATION_LOG_DEBUG("[NKSerialization] Mode: Shared DLL");
            #elif defined(NKENTSEU_SERIALIZATION_STATIC_LIB)
                NK_FOUNDATION_LOG_DEBUG("[NKSerialization] Mode: Static library");
            #endif

            // Code spécifique plateforme
            #ifdef NKENTSEU_PLATFORM_WINDOWS
                Serialization_InitializeWindowsIO();
            #elif defined(NKENTSEU_PLATFORM_LINUX)
                Serialization_InitializeLinuxIO();
            #endif

        }

    } // namespace nkentseu
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================