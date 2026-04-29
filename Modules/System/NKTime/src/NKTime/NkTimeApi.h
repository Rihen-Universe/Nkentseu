// =============================================================================
// NKTime/NkTimeApi.h
// Gestion de la visibilité des symboles et de l'API publique du module NKTime.
//
// Design :
//  - Réutilisation DIRECTE des macros de NKPlatform et NKCore (ZÉRO duplication)
//  - Macros spécifiques NKTime uniquement pour la configuration de build NKTime
//  - Indépendance totale des modes de build entre modules
//  - Compatibilité multiplateforme et multi-compilateur via NKPlatform
//  - Aucune redéfinition de deprecated, alignement, pragma, etc.
//
// Règles d'application des macros :
//  - NKENTSEU_TIME_API sur les fonctions libres et variables globales publiques
//  - PAS sur les méthodes de classes (l'export de la classe suffit)
//  - PAS sur les classes/structs/templates (géré par NKENTSEU_TIME_CLASS_EXPORT)
//  - PAS sur les fonctions inline définies dans les headers
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_TIME_NKTIMEAPI_H
#define NKENTSEU_TIME_NKTIMEAPI_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // NKTime dépend de NKCore et NKPlatform.
    // Nous importons leurs macros d'export pour assurer la cohérence.
    // AUCUNE duplication : nous utilisons directement les macros existantes.

    #include "NKCore/NkCoreApi.h"
    #include "NKPlatform/NkPlatformExport.h"
    #include "NKPlatform/NkPlatformInline.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION DU MODE DE BUILD NKTIME
    // -------------------------------------------------------------------------
    /**
     * @defgroup TimeBuildConfig Configuration du Build NKTime
     * @brief Macros pour contrôler le mode de compilation de NKTime
     *
     * Ces macros sont INDÉPENDANTES de celles de NKCore et NKPlatform :
     *  - NKENTSEU_TIME_BUILD_SHARED_LIB : Compiler NKTime en bibliothèque partagée
     *  - NKENTSEU_TIME_STATIC_LIB : Utiliser NKTime en mode bibliothèque statique
     *  - NKENTSEU_TIME_HEADER_ONLY : Mode header-only (tout inline)
     *
     * @note NKTime, NKCore et NKPlatform peuvent avoir des modes de build différents.
     *
     * @example CMakeLists.txt
     * @code
     * # NKTime en DLL
     * target_compile_definitions(nktime PRIVATE NKENTSEU_TIME_BUILD_SHARED_LIB)
     *
     * # NKTime en static
     * target_compile_definitions(monapp PRIVATE NKENTSEU_TIME_STATIC_LIB)
     * @endcode
     */

    // -------------------------------------------------------------------------
    // SECTION 3 : MACRO PRINCIPALE NKENTSEU_TIME_API
    // -------------------------------------------------------------------------
    /**
     * @brief Macro principale pour l'export/import des symboles NKTime
     * @def NKENTSEU_TIME_API
     * @ingroup TimeApiMacros
     *
     * Cette macro gère UNIQUEMENT la visibilité des symboles de NKTime.
     * Elle est indépendante de NKENTSEU_CORE_API et NKENTSEU_PLATFORM_API.
     *
     * Logique :
     *  - NKENTSEU_TIME_BUILD_SHARED_LIB : export (compilation de NKTime en DLL)
     *  - NKENTSEU_TIME_STATIC_LIB ou NKENTSEU_TIME_HEADER_ONLY : vide
     *  - Sinon : import (utilisation de NKTime en mode DLL)
     *
     * @note Cette macro utilise NKENTSEU_PLATFORM_API_EXPORT/IMPORT comme base.
     */
    #if defined(NKENTSEU_TIME_BUILD_SHARED_LIB)
        // Compilation de NKTime en bibliothèque partagée : exporter les symboles
        #define NKENTSEU_TIME_API NKENTSEU_PLATFORM_API_EXPORT
    #elif defined(NKENTSEU_TIME_STATIC_LIB) || defined(NKENTSEU_TIME_HEADER_ONLY)
        // Build statique ou header-only : aucune décoration de symbole nécessaire
        #define NKENTSEU_TIME_API
    #else
        // Utilisation de NKTime en mode DLL : importer les symboles
        #define NKENTSEU_TIME_API NKENTSEU_PLATFORM_API_IMPORT
    #endif

    // -------------------------------------------------------------------------
    // SECTION 4 : MACROS DE CONVENANCE SPÉCIFIQUES À NKTIME
    // -------------------------------------------------------------------------
    // Seules les macros qui ajoutent une valeur spécifique à NKTime sont définies.

    /**
     * @brief Macro pour exporter une classe complète de NKTime
     * @def NKENTSEU_TIME_CLASS_EXPORT
     * @ingroup TimeApiMacros
     *
     * Alias vers NKENTSEU_TIME_API pour les déclarations de classe.
     * Les méthodes de la classe n'ont PAS besoin de NKENTSEU_TIME_API.
     *
     * @example
     * @code
     * class NKENTSEU_TIME_CLASS_EXPORT TimeManager {
     * public:
     *     void StartTimer();  // Pas de NKENTSEU_TIME_API ici
     * };
     * @endcode
     */
    #define NKENTSEU_TIME_CLASS_EXPORT NKENTSEU_TIME_API

    /**
     * @brief Fonction inline exportée pour NKTime
     * @def NKENTSEU_TIME_API_INLINE
     * @ingroup TimeApiMacros
     *
     * Combinaison de NKENTSEU_TIME_API et NKENTSEU_INLINE.
     * En mode header-only, équivaut à NKENTSEU_FORCE_INLINE.
     */
    #if defined(NKENTSEU_TIME_HEADER_ONLY)
        // Mode header-only : forcer l'inlining pour éviter les erreurs de linkage
        #define NKENTSEU_TIME_API_INLINE NKENTSEU_FORCE_INLINE
    #else
        // Mode bibliothèque : combiner export et inline hint
        #define NKENTSEU_TIME_API_INLINE NKENTSEU_TIME_API NKENTSEU_INLINE
    #endif

    /**
     * @brief Fonction force_inline exportée pour NKTime
     * @def NKENTSEU_TIME_API_FORCE_INLINE
     * @ingroup TimeApiMacros
     *
     * Combinaison de NKENTSEU_TIME_API et NKENTSEU_FORCE_INLINE.
     * Pour les fonctions critiques en performance dans l'API publique.
     */
    #if defined(NKENTSEU_TIME_HEADER_ONLY)
        // Mode header-only : NKENTSEU_FORCE_INLINE suffit
        #define NKENTSEU_TIME_API_FORCE_INLINE NKENTSEU_FORCE_INLINE
    #else
        // Mode bibliothèque : combiner export et force inline
        #define NKENTSEU_TIME_API_FORCE_INLINE NKENTSEU_TIME_API NKENTSEU_FORCE_INLINE
    #endif

    /**
     * @brief Fonction no_inline exportée pour NKTime
     * @def NKENTSEU_TIME_API_NO_INLINE
     * @ingroup TimeApiMacros
     *
     * Combinaison de NKENTSEU_TIME_API et NKENTSEU_NO_INLINE.
     * Pour éviter l'explosion de code dans les DLL ou faciliter le debugging.
     */
    #define NKENTSEU_TIME_API_NO_INLINE NKENTSEU_TIME_API NKENTSEU_NO_INLINE

    // -------------------------------------------------------------------------
    // SECTION 5 : UTILISATION DIRECTE DES MACROS NKPLATFORM (PAS DE DUPLICATION)
    // -------------------------------------------------------------------------
    /**
     * @defgroup TimeApiReuse Réutilisation des Macros NKPlatform
     * @brief Guide d'utilisation des macros NKPlatform dans NKTime
     *
     * Pour éviter la duplication, NKTime réutilise directement les macros
     * de NKPlatform. Voici les équivalences :
     *
     * | Besoin dans NKTime | Macro à utiliser | Source |
     * |-------------------|-----------------|--------|
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
     * // Dépréciation dans NKTime :
     * NKENTSEU_DEPRECATED_MESSAGE("Utiliser NewTimeFunction()")
     * NKENTSEU_TIME_API void OldTimeFunction();
     * @endcode
     */

    // -------------------------------------------------------------------------
    // SECTION 6 : VALIDATION DES MACROS DE BUILD NKTIME
    // -------------------------------------------------------------------------
    // Vérifications de cohérence pour les macros spécifiques à NKTime.

    #if defined(NKENTSEU_TIME_BUILD_SHARED_LIB) && defined(NKENTSEU_TIME_STATIC_LIB)
        // Conflit : shared et static ne peuvent coexister
        #warning "NKTime: NKENTSEU_TIME_BUILD_SHARED_LIB et NKENTSEU_TIME_STATIC_LIB définis - NKENTSEU_TIME_STATIC_LIB ignoré"
        #undef NKENTSEU_TIME_STATIC_LIB
    #endif

    #if defined(NKENTSEU_TIME_BUILD_SHARED_LIB) && defined(NKENTSEU_TIME_HEADER_ONLY)
        // Conflit : shared et header-only sont mutuellement exclusifs
        #warning "NKTime: NKENTSEU_TIME_BUILD_SHARED_LIB et NKENTSEU_TIME_HEADER_ONLY définis - NKENTSEU_TIME_HEADER_ONLY ignoré"
        #undef NKENTSEU_TIME_HEADER_ONLY
    #endif

    #if defined(NKENTSEU_TIME_STATIC_LIB) && defined(NKENTSEU_TIME_HEADER_ONLY)
        // Conflit : static et header-only ne peuvent coexister
        #warning "NKTime: NKENTSEU_TIME_STATIC_LIB et NKENTSEU_TIME_HEADER_ONLY définis - NKENTSEU_TIME_HEADER_ONLY ignoré"
        #undef NKENTSEU_TIME_HEADER_ONLY
    #endif

    // -------------------------------------------------------------------------
    // SECTION 7 : MESSAGES DE DEBUG OPTIONNELS
    // -------------------------------------------------------------------------
    // Affichage des informations de configuration au moment de la compilation.

    #ifdef NKENTSEU_TIME_DEBUG
        #pragma message("NKTime Export Config:")
        #if defined(NKENTSEU_TIME_BUILD_SHARED_LIB)
            #pragma message("  NKTime mode: Shared (export)")
        #elif defined(NKENTSEU_TIME_STATIC_LIB)
            #pragma message("  NKTime mode: Static")
        #elif defined(NKENTSEU_TIME_HEADER_ONLY)
            #pragma message("  NKTime mode: Header-only")
        #else
            #pragma message("  NKTime mode: DLL import (default)")
        #endif
        #pragma message("  NKENTSEU_TIME_API = " NKENTSEU_STRINGIZE(NKENTSEU_TIME_API))
    #endif

    #if defined(NKENTSEU_HAS_NODISCARD)
        #define NKTIME_NODISCARD NKENTSEU_NODISCARD
    #else
        #define NKTIME_NODISCARD
    #endif

#endif // NKENTSEU_TIME_NKTIMEAPI_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKTIMEAPI.H
// =============================================================================
// Ce fichier fournit les macros pour gérer la visibilité des symboles du module
// NKTime, avec support multiplateforme et multi-compilateur.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Déclaration d'une fonction publique NKTime (API C)
// -----------------------------------------------------------------------------
/*
    // Dans nktime_public.h :
    #include "NKTime/NkTimeApi.h"

    NKENTSEU_TIME_EXTERN_C_BEGIN

    // Fonctions C publiques exportées avec liaison C
    NKENTSEU_TIME_API void Time_Initialize(void);
    NKENTSEU_TIME_API void Time_Shutdown(void);
    NKENTSEU_TIME_API uint64_t Time_GetTimestamp(void);

    NKENTSEU_TIME_EXTERN_C_END
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Déclaration d'une classe publique NKTime (API C++)
// -----------------------------------------------------------------------------
/*
    // Dans TimeManager.h :
    #include "NKTime/NkTimeApi.h"

    namespace nkentseu {

        // Classe exportée : les méthodes n'ont PAS besoin de NKENTSEU_TIME_API
        class NKENTSEU_TIME_CLASS_EXPORT TimeManager {
        public:
            // Constructeurs : pas de macro sur les méthodes
            TimeManager();
            ~TimeManager();

            // Méthodes publiques : pas de NKENTSEU_TIME_API nécessaire
            bool Initialize(const char* configPath);
            void Shutdown();
            bool IsRunning() const;

            // Fonction inline dans la classe : pas de macro d'export
            NKENTSEU_FORCE_INLINE uint64_t GetCurrentTick() const {
                return m_currentTick;
            }

        protected:
            // Méthodes protégées : pas de macro non plus
            void UpdateInternal();

        private:
            // Membres privés
            uint64_t m_currentTick;
            bool m_isRunning;
        };

    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Fonctions templates et inline (PAS de macro d'export)
// -----------------------------------------------------------------------------
/*
    #include "NKTime/NkTimeApi.h"

    namespace nkentseu {

        // Template : JAMAIS de NKENTSEU_TIME_API sur les templates
        template<typename ClockType>
        class TimeAdapter {
        public:
            TimeAdapter() = default;

            // Méthode template : pas de macro d'export
            template<typename Duration>
            auto Convert(Duration d) -> decltype(d.count()) {
                return d.count();
            }
        };

        // Fonction inline définie dans le header : pas de macro
        NKENTSEU_FORCE_INLINE uint64_t Time_NowNanoseconds() {
            // Implémentation inline...
            return 0;
        }

    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Gestion des modes de build via CMake pour NKTime
// -----------------------------------------------------------------------------
/*
    # CMakeLists.txt pour NKTime

    cmake_minimum_required(VERSION 3.15)
    project(NKTime VERSION 1.0.0)

    # Options de build configurables
    option(NKTIME_BUILD_SHARED "Build NKTime as shared library" ON)
    option(NKTIME_HEADER_ONLY "Use NKTime in header-only mode" OFF)

    # Configuration des defines de build
    if(NKTIME_HEADER_ONLY)
        add_definitions(-DNKENTSEU_TIME_HEADER_ONLY)
    elseif(NKTIME_BUILD_SHARED)
        add_definitions(-DNKENTSEU_TIME_BUILD_SHARED_LIB)
        set(NKTIME_LIBRARY_TYPE SHARED)
    else()
        add_definitions(-DNKENTSEU_TIME_STATIC_LIB)
        set(NKTIME_LIBRARY_TYPE STATIC)
    endif()

    # Dépendances requises
    find_package(NKCore REQUIRED)
    find_package(NKPlatform REQUIRED)

    # Création de la bibliothèque NKTime
    add_library(nktime ${NKTIME_LIBRARY_TYPE}
        src/TimeManager.cpp
        src/Timestamp.cpp
    )

    # Liaison avec les dépendances
    target_link_libraries(nktime
        PUBLIC NKCore::NKCore
        PUBLIC NKPlatform::NKPlatform
    )

    # Configuration des include directories
    target_include_directories(nktime PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Utilisation dans un fichier .cpp (avec pch.h)
// -----------------------------------------------------------------------------
/*
    // Dans TimeManager.cpp :

    #include "pch.h"                    // Precompiled header en premier
    #include "NKTime/NkTimeApi.h"       // Remplace NkTimeExport.h
    #include "NKTime/TimeManager.h"

    namespace nkentseu {

        // Implémentation : pas besoin de macros ici
        TimeManager::TimeManager()
            : m_currentTick(0)
            , m_isRunning(false)
        {
            // Construction...
        }

        TimeManager::~TimeManager() {
            Shutdown();
        }

        bool TimeManager::Initialize(const char* configPath) {
            // Initialisation...
            return true;
        }

        void TimeManager::Shutdown() {
            // Nettoyage...
            m_isRunning = false;
        }

        bool TimeManager::IsRunning() const {
            return m_isRunning;
        }

        void TimeManager::UpdateInternal() {
            // Mise à jour interne...
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
        namespace internal {

            // Classe : indentée d'un niveau supplémentaire
            class NKENTSEU_TIME_CLASS_EXPORT InternalTimer {
            public:
                // Section public : indentée d'un niveau supplémentaire
                // Une instruction par ligne, pas de macro sur les méthodes
                InternalTimer();
                ~InternalTimer();

                void Start();
                void Stop();

            protected:
                // Section protected : même indentation que public
                void Reset();

            private:
                // Section private : même indentation
                // Membres : une déclaration par ligne
                uint64_t m_startTick;
                uint64_t m_stopTick;
                bool m_isRunning;
            };

        } // namespace internal

        // Bloc conditionnel : contenu indenté
        #if defined(NKENTSEU_TIME_ENABLE_PROFILING)

            // Fonction d'export uniquement si profiling activé
            NKENTSEU_TIME_API void Time_RecordProfileSample(const char* label);

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
    #include <NKTime/NkTimeApi.h>

    namespace nkentseu {

        // Fonction utilisant les macros des trois modules
        NKENTSEU_TIME_API void Time_CrossModuleInit() {

            // Logging via NKPlatform
            NK_FOUNDATION_LOG_INFO("[NKTime] Initializing...");

            // Détection du mode de build NKTime
            #if defined(NKENTSEU_TIME_BUILD_SHARED_LIB)
                NK_FOUNDATION_LOG_DEBUG("[NKTime] Mode: Shared DLL");
            #elif defined(NKENTSEU_TIME_STATIC_LIB)
                NK_FOUNDATION_LOG_DEBUG("[NKTime] Mode: Static library");
            #endif

            // Code spécifique plateforme
            #ifdef NKENTSEU_PLATFORM_WINDOWS
                Time_InitializeWindowsTimer();
            #elif defined(NKENTSEU_PLATFORM_LINUX)
                Time_InitializeLinuxClock();
            #endif

        }

    } // namespace nkentseu
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================