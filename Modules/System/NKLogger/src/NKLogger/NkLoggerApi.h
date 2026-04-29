// =============================================================================
// NKLogger/NkLoggerApi.h
// Gestion de la visibilité des symboles et de l'API publique du module NKLogger.
//
// Design :
//  - Réutilisation DIRECTE des macros de NKPlatform (ZÉRO duplication)
//  - Dépendance optionnelle vers NKCore pour l'intégration logging avancée
//  - Macros spécifiques NKLogger uniquement pour la configuration de build
//  - Indépendance totale des modes de build entre modules
//  - Compatibilité multiplateforme et multi-compilateur via NKPlatform
//  - Aucune redéfinition de deprecated, alignement, pragma, etc.
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_LOGGER_NKLOGGERAPI_H
#define NKENTSEU_LOGGER_NKLOGGERAPI_H


    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // NKLogger dépend de NKPlatform pour les macros d'export multiplateforme.
    // Dépendance optionnelle vers NKCore si l'intégration logging avancée est requise.
    // AUCUNE duplication : nous utilisons directement les macros NKPlatform.

    #include "NKPlatform/NkPlatformExport.h"
    #include "NKPlatform/NkPlatformInline.h"


    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION DU MODE DE BUILD NKLOGGER
    // -------------------------------------------------------------------------
    /**
     * @defgroup LoggerBuildConfig Configuration du Build NKLogger
     * @brief Macros pour contrôler le mode de compilation de NKLogger
     *
     * Ces macros sont INDÉPENDANTES de celles de NKPlatform et NKCore :
     *  - NKENTSEU_LOGGER_BUILD_SHARED_LIB : Compiler NKLogger en bibliothèque partagée
     *  - NKENTSEU_LOGGER_STATIC_LIB : Utiliser NKLogger en mode bibliothèque statique
     *  - NKENTSEU_LOGGER_HEADER_ONLY : Mode header-only (tout inline)
     *
     * @note NKLogger, NKCore et NKPlatform peuvent avoir des modes de build différents.
     * Exemple valide : NKPlatform (DLL) + NKCore (static) + NKLogger (header-only).
     *
     * @example CMakeLists.txt
     * @code
     * # NKLogger en DLL, autres modules en static
     * target_compile_definitions(nklogger PRIVATE NKENTSEU_LOGGER_BUILD_SHARED_LIB)
     * target_compile_definitions(nklogger PRIVATE NKENTSEU_CORE_STATIC_LIB)
     * target_compile_definitions(nklogger PRIVATE NKENTSEU_STATIC_LIB)
     *
     * # NKLogger en header-only pour intégration légère
     * target_compile_definitions(monapp PRIVATE NKENTSEU_LOGGER_HEADER_ONLY)
     * @endcode
     */


    // -------------------------------------------------------------------------
    // SECTION 3 : MACRO PRINCIPALE NKENTSEU_LOGGER_API
    // -------------------------------------------------------------------------
    /**
     * @brief Macro principale pour l'export/import des symboles NKLogger
     * @def NKENTSEU_LOGGER_API
     * @ingroup LoggerApiMacros
     *
     * Cette macro gère UNIQUEMENT la visibilité des symboles de NKLogger.
     * Elle est indépendante de NKENTSEU_PLATFORM_API et NKENTSEU_CORE_API.
     *
     * Logique de résolution :
     *  - NKENTSEU_LOGGER_BUILD_SHARED_LIB : export (compilation de NKLogger en DLL)
     *  - NKENTSEU_LOGGER_STATIC_LIB ou NKENTSEU_LOGGER_HEADER_ONLY : vide (pas d'export)
     *  - Sinon : import (utilisation de NKLogger en mode DLL par défaut)
     *
     * @note Cette macro utilise NKENTSEU_PLATFORM_API_EXPORT/IMPORT comme base,
     * garantissant la compatibilité multiplateforme sans duplication de code.
     *
     * @example
     * @code
     * // Déclaration d'une fonction publique dans un header NKLogger :
     * NKENTSEU_LOGGER_API void Logger_Initialize(const char* configPath);
     *
     * // Compilation de NKLogger en DLL : -DNKENTSEU_LOGGER_BUILD_SHARED_LIB
     * // Utilisation de NKLogger en DLL : (aucun define, import par défaut)
     * // Utilisation de NKLogger en static : -DNKENTSEU_LOGGER_STATIC_LIB
     * @endcode
     */
    #if defined(NKENTSEU_LOGGER_BUILD_SHARED_LIB)
        #define NKENTSEU_LOGGER_API NKENTSEU_PLATFORM_API_EXPORT
    #elif defined(NKENTSEU_LOGGER_STATIC_LIB) || defined(NKENTSEU_LOGGER_HEADER_ONLY)
        #define NKENTSEU_LOGGER_API
    #else
        #define NKENTSEU_LOGGER_API NKENTSEU_PLATFORM_API_IMPORT
    #endif


    // -------------------------------------------------------------------------
    // SECTION 4 : MACROS DE CONVENANCE SPÉCIFIQUES À NKLOGGER
    // -------------------------------------------------------------------------
    // Seules les macros qui ajoutent une valeur spécifique à NKLogger sont définies.
    // Tout le reste (deprecated, alignement, pragma, etc.) est utilisé directement
    // depuis NKPlatform via le préfixe NKENTSEU_*.

    /**
     * @brief Macro pour exporter une classe complète de NKLogger
     * @def NKENTSEU_LOGGER_CLASS_EXPORT
     * @ingroup LoggerApiMacros
     *
     * Alias vers NKENTSEU_LOGGER_API pour les déclarations de classe.
     * Améliore la lisibilité lors de la déclaration de types publics.
     *
     * @example
     * @code
     * class NKENTSEU_LOGGER_CLASS_EXPORT LoggerManager {
     * public:
     *     NKENTSEU_LOGGER_API void LogInfo(const char* message);
     * };
     * @endcode
     */
    #define NKENTSEU_LOGGER_CLASS_EXPORT NKENTSEU_LOGGER_API

    /**
     * @brief Fonction inline exportée pour NKLogger
     * @def NKENTSEU_LOGGER_API_INLINE
     * @ingroup LoggerApiMacros
     *
     * Combinaison de NKENTSEU_LOGGER_API et NKENTSEU_INLINE.
     * En mode header-only, équivaut à NKENTSEU_FORCE_INLINE pour l'optimisation.
     */
    #if defined(NKENTSEU_LOGGER_HEADER_ONLY)
        #define NKENTSEU_LOGGER_API_INLINE NKENTSEU_FORCE_INLINE
    #else
        #define NKENTSEU_LOGGER_API_INLINE NKENTSEU_LOGGER_API NKENTSEU_INLINE
    #endif

    /**
     * @brief Fonction force_inline exportée pour NKLogger
     * @def NKENTSEU_LOGGER_API_FORCE_INLINE
     * @ingroup LoggerApiMacros
     *
     * Combinaison de NKENTSEU_LOGGER_API et NKENTSEU_FORCE_INLINE.
     * Pour les fonctions critiques en performance dans l'API publique de logging.
     *
     * @note À utiliser avec parcimonie : l'inlining excessif peut augmenter la taille du binaire.
     */
    #if defined(NKENTSEU_LOGGER_HEADER_ONLY)
        #define NKENTSEU_LOGGER_API_FORCE_INLINE NKENTSEU_FORCE_INLINE
    #else
        #define NKENTSEU_LOGGER_API_FORCE_INLINE NKENTSEU_LOGGER_API NKENTSEU_FORCE_INLINE
    #endif

    /**
     * @brief Fonction no_inline exportée pour NKLogger
     * @def NKENTSEU_LOGGER_API_NO_INLINE
     * @ingroup LoggerApiMacros
     *
     * Combinaison de NKENTSEU_LOGGER_API et NKENTSEU_NO_INLINE.
     * Pour éviter l'explosion de code dans les DLL ou faciliter le debugging/profiling.
     */
    #define NKENTSEU_LOGGER_API_NO_INLINE NKENTSEU_LOGGER_API NKENTSEU_NO_INLINE

    /**
     * @brief Macro pour les fonctions de logging à appel fréquent
     * @def NKENTSEU_LOGGER_FAST_CALL
     * @ingroup LoggerApiMacros
     *
     * Optimisation d'appel pour les fonctions de logging intensif.
     * Utilise NKENTSEU_FAST_CALL de NKPlatform si disponible, sinon vide.
     */
    #if defined(NKENTSEU_FAST_CALL)
        #define NKENTSEU_LOGGER_FAST_CALL NKENTSEU_LOGGER_API NKENTSEU_FAST_CALL
    #else
        #define NKENTSEU_LOGGER_FAST_CALL NKENTSEU_LOGGER_API
    #endif


    // -------------------------------------------------------------------------
    // SECTION 5 : UTILISATION DIRECTE DES MACROS NKPLATFORM (PAS DE DUPLICATION)
    // -------------------------------------------------------------------------
    /**
     * @defgroup LoggerApiReuse Réutilisation des Macros NKPlatform
     * @brief Guide d'utilisation des macros NKPlatform dans NKLogger
     *
     * Pour éviter la duplication et maintenir la cohérence, NKLogger réutilise
     * directement les macros de NKPlatform. Voici les équivalences recommandées :
     *
     * | Besoin dans NKLogger              | Macro à utiliser                    | Source     |
     * |-----------------------------------|-------------------------------------|------------|
     * | Dépréciation de fonction          | `NKENTSEU_DEPRECATED`               | NKPlatform |
     * | Dépréciation avec message explicite | `NKENTSEU_DEPRECATED_MESSAGE(msg)` | NKPlatform |
     * | Alignement sur ligne de cache     | `NKENTSEU_ALIGN_CACHE`              | NKPlatform |
     * | Alignement 16/32/64/128 bits      | `NKENTSEU_ALIGN_16`, `NKENTSEU_ALIGN_32`, etc. | NKPlatform |
     * | Liaison C pour compatibilité ABI  | `NKENTSEU_EXTERN_C_BEGIN/END`       | NKPlatform |
     * | Pragmas compiler-specific         | `NKENTSEU_PRAGMA(x)`                | NKPlatform |
     * | Suppression de warnings locaux    | `NKENTSEU_DISABLE_WARNING_*`        | NKPlatform |
     * | Symbole à visibilité locale       | `NKENTSEU_API_LOCAL`                | NKPlatform |
     * | Annotation de branch prediction   | `NKENTSEU_LIKELY/UNLIKELY`          | NKPlatform |
     *
     * @example
     * @code
     * // Dépréciation dans NKLogger (pas de NKENTSEU_LOGGER_DEPRECATED) :
     * NKENTSEU_DEPRECATED_MESSAGE("Utiliser Logger::Log() avec niveau explicite")
     * NKENTSEU_LOGGER_API void Legacy_LogMessage(const char* msg);
     *
     * // Structure alignée pour optimisation SIMD dans le logging binaire :
     * struct NKENTSEU_ALIGN_32 NKENTSEU_LOGGER_CLASS_EXPORT LogBatch {
     *     uint64_t timestamps[8];  // Aligné pour accès vectorisé
     *     char messages[8][256];
     * };
     * @endcode
     */


    // -------------------------------------------------------------------------
    // SECTION 6 : VALIDATION DES MACROS DE BUILD NKLOGGER
    // -------------------------------------------------------------------------
    // Vérifications de cohérence pour les macros spécifiques à NKLogger.
    // Ces warnings aident à détecter les configurations de build conflictuelles.

    #if defined(NKENTSEU_LOGGER_BUILD_SHARED_LIB) && defined(NKENTSEU_LOGGER_STATIC_LIB)
        #warning "NKLogger: NKENTSEU_LOGGER_BUILD_SHARED_LIB et NKENTSEU_LOGGER_STATIC_LIB définis simultanément - NKENTSEU_LOGGER_STATIC_LIB ignoré"
        #undef NKENTSEU_LOGGER_STATIC_LIB
    #endif

    #if defined(NKENTSEU_LOGGER_BUILD_SHARED_LIB) && defined(NKENTSEU_LOGGER_HEADER_ONLY)
        #warning "NKLogger: NKENTSEU_LOGGER_BUILD_SHARED_LIB et NKENTSEU_LOGGER_HEADER_ONLY définis simultanément - NKENTSEU_LOGGER_HEADER_ONLY ignoré"
        #undef NKENTSEU_LOGGER_HEADER_ONLY
    #endif

    #if defined(NKENTSEU_LOGGER_STATIC_LIB) && defined(NKENTSEU_LOGGER_HEADER_ONLY)
        #warning "NKLogger: NKENTSEU_LOGGER_STATIC_LIB et NKENTSEU_LOGGER_HEADER_ONLY définis simultanément - NKENTSEU_LOGGER_HEADER_ONLY ignoré"
        #undef NKENTSEU_LOGGER_HEADER_ONLY
    #endif


    // -------------------------------------------------------------------------
    // SECTION 7 : MESSAGES DE DEBUG OPTIONNELS POUR LE BUILD
    // -------------------------------------------------------------------------
    // Affichage d'informations de configuration lors de la compilation en mode debug.
    // Utile pour vérifier que les macros sont correctement résolues.

    #ifdef NKENTSEU_LOGGER_DEBUG
        #pragma message("NKLogger Export Configuration:")
        #if defined(NKENTSEU_LOGGER_BUILD_SHARED_LIB)
            #pragma message("  NKLogger mode: Shared library (symbols exported)")
        #elif defined(NKENTSEU_LOGGER_STATIC_LIB)
            #pragma message("  NKLogger mode: Static library (no symbol decoration)")
        #elif defined(NKENTSEU_LOGGER_HEADER_ONLY)
            #pragma message("  NKLogger mode: Header-only (all functions inline)")
        #else
            #pragma message("  NKLogger mode: DLL import (default - symbols imported)")
        #endif
        #pragma message("  NKENTSEU_LOGGER_API resolves to: " NKENTSEU_STRINGIZE(NKENTSEU_LOGGER_API))
    #endif


#endif // NKENTSEU_LOGGER_NKLOGGERAPI_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKLOGGERAPI.H
// =============================================================================
// Ce fichier fournit les macros pour gérer la visibilité des symboles du module
// NKLogger, avec support multiplateforme et multi-compilateur via NKPlatform.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Déclaration d'une fonction C publique NKLogger
// -----------------------------------------------------------------------------
/*
    // Dans un header public : nklogger_public.h
    #include "NKLogger/NkLoggerApi.h"

    NKENTSEU_LOGGER_EXTERN_C_BEGIN

    // Fonctions C publiques avec visibilité contrôlée
    NKENTSEU_LOGGER_API void Logger_Initialize(const char* configPath);
    NKENTSEU_LOGGER_API void Logger_Shutdown(void);
    NKENTSEU_LOGGER_API void Logger_LogInfo(const char* message);
    NKENTSEU_LOGGER_API void Logger_LogError(const char* message);
    NKENTSEU_LOGGER_API int Logger_GetLogLevel(void);
    NKENTSEU_LOGGER_API void Logger_SetLogLevel(int level);

    NKENTSEU_LOGGER_EXTERN_C_END

    // Dans l'implémentation : nklogger_public.cpp
    #include "nklogger_public.h"
    #include "NKLogger/Internal/LoggerImpl.h"

    void Logger_Initialize(const char* configPath) {
        nkentseu::logger::internal::LoggerImpl::GetInstance().initialize(configPath);
    }

    void Logger_Shutdown(void) {
        nkentseu::logger::internal::LoggerImpl::GetInstance().shutdown();
    }

    void Logger_LogInfo(const char* message) {
        nkentseu::logger::internal::LoggerImpl::GetInstance().logInfo(message);
    }

    void Logger_LogError(const char* message) {
        nkentseu::logger::internal::LoggerImpl::GetInstance().logError(message);
    }

    int Logger_GetLogLevel(void) {
        return nkentseu::logger::internal::LoggerImpl::GetInstance().getLogLevel();
    }

    void Logger_SetLogLevel(int level) {
        nkentseu::logger::internal::LoggerImpl::GetInstance().setLogLevel(level);
    }
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Déclaration d'une classe publique NKLogger avec membres variés
// -----------------------------------------------------------------------------
/*
    // Dans LoggerManager.h
    #include "NKLogger/NkLoggerApi.h"
    #include <string>
    #include <memory>

    namespace nkentseu {
    namespace logger {

        class NKENTSEU_LOGGER_CLASS_EXPORT LoggerManager {
        public:
            // Constructeur/Destructeur avec visibilité explicite
            NKENTSEU_LOGGER_API LoggerManager();
            NKENTSEU_LOGGER_API ~LoggerManager();

            // Méthodes publiques de l'API
            NKENTSEU_LOGGER_API bool initialize(const char* configPath);
            NKENTSEU_LOGGER_API void shutdown();
            NKENTSEU_LOGGER_API bool isInitialized() const;

            // Méthodes de logging avec différents niveaux
            NKENTSEU_LOGGER_API void logDebug(const char* message);
            NKENTSEU_LOGGER_API void logInfo(const char* message);
            NKENTSEU_LOGGER_API void logWarning(const char* message);
            NKENTSEU_LOGGER_API void logError(const char* message);
            NKENTSEU_LOGGER_API void logCritical(const char* message);

            // Fonction inline critique pour performance (évite appel de fonction)
            NKENTSEU_LOGGER_API_FORCE_INLINE const char* getLoggerName() const {
                return m_loggerName.c_str();
            }

            // Fonction avec optimisation d'appel fréquent
            NKENTSEU_LOGGER_FAST_CALL void logQuick(const char* message);

            // Getter/Setter avec contrôle de visibilité
            NKENTSEU_LOGGER_API int getLogLevel() const;
            NKENTSEU_LOGGER_API void setLogLevel(int level);

            // Méthode dépréciée avec message de migration
            NKENTSEU_DEPRECATED_MESSAGE("Utiliser logInfo() avec formatage explicite")
            NKENTSEU_LOGGER_API void legacyLog(const char* format, ...);

        protected:
            // Méthodes protégées pour héritage contrôlé
            NKENTSEU_LOGGER_API virtual void onLogMessage(int level, const char* message);
            NKENTSEU_LOGGER_API virtual void onConfigChanged();

        private:
            // Méthodes privées : pas d'export nécessaire (implémentation interne)
            void internalCleanup();
            bool validateConfig(const char* configPath);
            void formatMessage(char* buffer, size_t size, int level, const char* message);

            // Membres privés
            std::string m_loggerName;
            int m_logLevel;
            bool m_initialized;
            std::unique_ptr<void> m_internalHandle;  // Pimpl idiom
        };

    } // namespace logger
    } // namespace nkentseu
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Structure de données alignée pour logging haute performance
// -----------------------------------------------------------------------------
/*
    #include "NKLogger/NkLoggerApi.h"
    #include "NKPlatform/NkPlatformAlign.h"

    // Structure optimisée pour le batching de logs en mémoire partagée
    struct NKENTSEU_ALIGN_64 NKENTSEU_LOGGER_CLASS_EXPORT LogBatchEntry {
        uint64_t timestamp;           // 8 bytes - aligné
        uint32_t threadId;            // 4 bytes
        uint32_t logLevel;            // 4 bytes - total 16 bytes
        uint64_t contextId;           // 8 bytes
        char message[240];            // 240 bytes - total 264 bytes
        // Padding automatique à 64 bytes par NKENTSEU_ALIGN_64
    };

    // Tableau circulaire de logs pour consommation asynchrone
    struct NKENTSEU_ALIGN_CACHE NKENTSEU_LOGGER_CLASS_EXPORT LogRingBuffer {
        volatile uint64_t writeIndex;  // Aligné sur ligne de cache pour atomicité
        volatile uint64_t readIndex;
        LogBatchEntry entries[1024];   // Buffer pré-alloué

        NKENTSEU_LOGGER_API_INLINE bool push(const LogBatchEntry& entry);
        NKENTSEU_LOGGER_API_INLINE bool pop(LogBatchEntry& outEntry);
        NKENTSEU_LOGGER_API_INLINE size_t available() const;
    };
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Configuration des modes de build via CMake
// -----------------------------------------------------------------------------
/*
    # CMakeLists.txt pour le module NKLogger

    cmake_minimum_required(VERSION 3.15)
    project(NKLogger VERSION 1.0.0 LANGUAGES CXX)

    # Options de configuration du build
    option(NKLOGGER_BUILD_SHARED "Build NKLogger as shared library" ON)
    option(NKLOGGER_HEADER_ONLY "Use NKLogger in header-only mode" OFF)
    option(NKLOGGER_ENABLE_ASYNC "Enable asynchronous logging backend" ON)
    option(NKLOGGER_ENABLE_FILE "Enable file output backend" ON)
    option(NKLOGGER_ENABLE_CONSOLE "Enable console output backend" ON)

    # Configuration des defines de compilation
    if(NKLOGGER_HEADER_ONLY)
        add_definitions(-DNKENTSEU_LOGGER_HEADER_ONLY)
        set(NKLOGGER_INTERFACE_LIB TRUE)
    elseif(NKLOGGER_BUILD_SHARED)
        add_definitions(-DNKENTSEU_LOGGER_BUILD_SHARED_LIB)
        set(NKLOGGER_LIBRARY_TYPE SHARED)
    else()
        add_definitions(-DNKENTSEU_LOGGER_STATIC_LIB)
        set(NKLOGGER_LIBRARY_TYPE STATIC)
    endif()

    # Options de fonctionnalités
    if(NKLOGGER_ENABLE_ASYNC)
        add_definitions(-DNKLOGGER_ASYNC_ENABLED)
    endif()
    if(NKLOGGER_ENABLE_FILE)
        add_definitions(-DNKLOGGER_FILE_ENABLED)
    endif()
    if(NKLOGGER_ENABLE_CONSOLE)
        add_definitions(-DNKLOGGER_CONSOLE_ENABLED)
    endif()

    # Création de la bibliothèque
    add_library(nklogger ${NKLOGGER_LIBRARY_TYPE}
        src/LoggerManager.cpp
        src/LoggerBackend.cpp
        src/backends/ConsoleBackend.cpp
        src/backends/FileBackend.cpp
        src/backends/AsyncBackend.cpp
        # ... autres fichiers sources
    )

    # Configuration des include directories
    target_include_directories(nklogger PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    # Liaison avec les dépendances
    target_link_libraries(nklogger PUBLIC
        NKPlatform::NKPlatform
        $<$<NOT:$<BOOL:${NKLOGGER_HEADER_ONLY}>>:NKCore::NKCore>
    )

    # Installation des en-têtes publics
    install(DIRECTORY include/NKLogger DESTINATION include)

    # Export de la cible pour les consommateurs
    install(TARGETS nklogger
        EXPORT NKLoggerTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
    )
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Utilisation de NKLogger dans une application cliente
// -----------------------------------------------------------------------------
/*
    // Dans CMakeLists.txt de l'application cliente :

    find_package(NKLogger REQUIRED)

    add_executable(monapplication main.cpp)
    target_link_libraries(monapplication PRIVATE NKLogger::NKLogger)

    # Gestion automatique des defines d'import/export via le package config
    # Si besoin manuel (build statique) :
    # target_compile_definitions(monapplication PRIVATE NKENTSEU_LOGGER_STATIC_LIB)

    // Dans main.cpp :
    #include <NKLogger/LoggerManager.h>
    #include <iostream>

    int main(int argc, char* argv[]) {
        // Initialisation du logger avec configuration
        nkentseu::logger::LoggerManager logger;

        if (!logger.initialize("config/logger.conf")) {
            std::cerr << "Failed to initialize logger" << std::endl;
            return 1;
        }

        // Logging à différents niveaux
        logger.logDebug("Application starting...");
        logger.logInfo("Configuration loaded successfully");
        logger.logWarning("Deprecated API usage detected");
        logger.logError("Connection timeout to database");

        // Utilisation de la fonction inline pour accès rapide
        std::cout << "Logger name: " << logger.getLoggerName() << std::endl;

        // Nettoyage
        logger.shutdown();
        return 0;
    }
*/


// -----------------------------------------------------------------------------
// Exemple 6 : Mode header-only pour intégration légère
// -----------------------------------------------------------------------------
/*
    // Pour utiliser NKLogger en mode header-only (idéal pour petits projets) :

    // Étape 1 : Définir la macro AVANT toute inclusion de NKLogger
    #define NKENTSEU_LOGGER_HEADER_ONLY
    #include <NKLogger/NkLoggerApi.h>
    #include <NKLogger/LoggerManager.h>

    // Étape 2 : Toutes les fonctions marquées API_INLINE sont maintenant inline
    // Étape 3 : Aucun linking de bibliothèque nécessaire

    void QuickLoggingExample() {
        nkentseu::logger::LoggerManager logger;  // Construction inline si header-only

        // Les appels inline évitent le overhead d'appel de fonction
        logger.logQuick("Fast log entry");

        // Attention : le mode header-only peut augmenter la taille du binaire
        // car le code est dupliqué dans chaque unité de traduction (.cpp)
    }

    // Bonnes pratiques header-only :
    // - Utiliser NKENTSEU_LOGGER_API_FORCE_INLINE uniquement pour fonctions < 10 lignes
    // - Préférer NKENTSEU_LOGGER_API_NO_INLINE pour les fonctions complexes
    // - Documenter clairement les dépendances inline dans l'API
*/


// -----------------------------------------------------------------------------
// Exemple 7 : Migration d'API et gestion de dépréciation
// -----------------------------------------------------------------------------
/*
    #include <NKLogger/NkLoggerApi.h>

    // Ancienne API dépréciée - sera retirée dans la version 2.0
    NKENTSEU_DEPRECATED_MESSAGE(
        "Utiliser LoggerManager::logInfo() avec niveau explicite. "
        "Cette fonction sera retirée dans NKLogger 2.0"
    )
    NKENTSEU_LOGGER_API void Legacy_SimpleLog(const char* message);

    // Nouvelle API recommandée avec typage fort des niveaux
    enum class NKENTSEU_LOGGER_CLASS_EXPORT LogLevel : int {
        Debug = 0,
        Info = 1,
        Warning = 2,
        Error = 3,
        Critical = 4
    };

    NKENTSEU_LOGGER_API void LoggerManager_LogTyped(LogLevel level, const char* message);

    // Dans le code client : migration progressive
    void MigrateLoggingCode() {
        // Ancien code - génère un warning de compilation
        // Legacy_SimpleLog("Old style log");

        // Nouveau code - type-safe et extensible
        LoggerManager_LogTyped(LogLevel::Info, "New style typed log");

        // Utilisation avec macro de convenience si fournie
        #define LOG_INFO(msg) LoggerManager_LogTyped(LogLevel::Info, msg)
        LOG_INFO("Convenience macro usage");
    }
*/


// -----------------------------------------------------------------------------
// Exemple 8 : Intégration multi-modules (Platform + Core + Logger)
// -----------------------------------------------------------------------------
/*
    #include <NKPlatform/NkPlatformDetect.h>   // NKENTSEU_PLATFORM_*
    #include <NKPlatform/NkPlatformExport.h>   // NKENTSEU_PLATFORM_API
    #include <NKCore/NkCoreApi.h>              // NKENTSEU_CORE_API
    #include <NKLogger/NkLoggerApi.h>          // NKENTSEU_LOGGER_API

    // Fonction utilisant les trois modules avec gestion cohérente des exports
    NKENTSEU_LOGGER_API void InitializeUnifiedLogging() {
        // Logging via NKLogger (ce module)
        NKENTSEU_LOGGER_API_NO_INLINE void InternalInit();

        // Vérification de cohérence des modes de build
        #if defined(NKENTSEU_LOGGER_BUILD_SHARED_LIB)
            NK_LOG_DEBUG("NKLogger: Shared DLL mode");
        #elif defined(NKENTSEU_LOGGER_STATIC_LIB)
            NK_LOG_DEBUG("NKLogger: Static library mode");
        #endif

        // Intégration avec NKCore si disponible
        #if !defined(NKENTSEU_CORE_HEADER_ONLY)
            nkentseu::core::CoreManager::GetInstance().registerLogger();
        #endif

        // Code spécifique plateforme via NKPlatform
        #ifdef NKENTSEU_PLATFORM_WINDOWS
            InitializeWindowsEventLog();
        #elif defined(NKENTSEU_PLATFORM_LINUX)
            InitializeSyslogBackend();
        #elif defined(NKENTSEU_PLATFORM_MACOS)
            InitializeOSLogBackend();
        #endif

        // Appel de la fonction interne (pas d'export nécessaire)
        InternalInit();
    }

    // Callback pour logging asynchrone avec optimisation d'appel
    NKENTSEU_LOGGER_FAST_CALL void OnLogReceived(int level, const char* msg) {
        // Traitement optimisé pour chemin critique
        if (NKENTSEU_LIKELY(level >= NKENTSEU_LOGGER_MIN_LEVEL)) {
            ProcessLogEntry(level, msg);
        }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 9 : Pattern Pimpl avec gestion d'export
// -----------------------------------------------------------------------------
/*
    #include <NKLogger/NkLoggerApi.h>
    #include <memory>

    namespace nkentseu {
    namespace logger {

        // Déclaration forward de l'implémentation interne (non exportée)
        namespace internal {
            class LoggerImpl;  // Définition dans .cpp, pas dans header public
        }

        class NKENTSEU_LOGGER_CLASS_EXPORT LoggerPimpl {
        public:
            NKENTSEU_LOGGER_API LoggerPimpl();
            NKENTSEU_LOGGER_API ~LoggerPimpl();

            // API publique stable - binaire compatible entre versions
            NKENTSEU_LOGGER_API bool initialize(const char* config);
            NKENTSEU_LOGGER_API void log(int level, const char* message);
            NKENTSEU_LOGGER_API void flush();

            // Copy/Move semantics avec contrôle explicite
            NKENTSEU_LOGGER_API LoggerPimpl(const LoggerPimpl& other) = delete;
            NKENTSEU_LOGGER_API LoggerPimpl& operator=(const LoggerPimpl& other) = delete;
            NKENTSEU_LOGGER_API LoggerPimpl(LoggerPimpl&& other) noexcept;
            NKENTSEU_LOGGER_API LoggerPimpl& operator=(LoggerPimpl&& other) noexcept;

        private:
            // Pointeur vers implémentation cachée (Pimpl idiom)
            // Pas d'export nécessaire pour les membres privés
            std::unique_ptr<internal::LoggerImpl> m_impl;
        };

    } // namespace logger
    } // namespace nkentseu

    // Avantages du pattern Pimpl avec NKENTSEU_LOGGER_API :
    // - ABI stable : modification de l'implémentation sans recompilation client
    // - Réduction des dépendances : moins d'includes dans les headers publics
    // - Compilation plus rapide : séparation interface/implémentation
    // - Encapsulation : détails internes non exposés dans l'API publique
*/


// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
    1. AJOUT DE NOUVELLES MACROS :
       - Toujours vérifier si NKPlatform fournit déjà l'équivalent
       - Préférer la réutilisation à la duplication
       - Documenter avec Doxygen et ajouter aux tables d'équivalence

    2. GESTION DES MODES DE BUILD :
       - Tester les 3 modes : SHARED, STATIC, HEADER_ONLY
       - Valider que NKENTSEU_LOGGER_API se résout correctement dans chaque cas
       - Utiliser NKENTSEU_LOGGER_DEBUG pour le debugging de configuration

    3. COMPATIBILITÉ ABI :
       - Ne jamais modifier l'ordre des paramètres dans les fonctions exportées
       - Utiliser le pattern Pimpl pour les classes avec état interne complexe
       - Versionner les APIs dépréciées avec des messages de migration clairs

    4. PERFORMANCE :
       - Réserver NKENTSEU_LOGGER_API_FORCE_INLINE aux fonctions < 10 lignes
       - Utiliser NKENTSEU_LOGGER_FAST_CALL pour les chemins de logging critiques
       - Profiler l'impact de l'inlining sur la taille du binaire en mode Release

    5. DOCUMENTATION :
       - Mettre à jour les exemples lors de l'ajout de nouvelles fonctionnalités
       - Maintenir la table d'équivalence NKPlatform/NKLogger à jour
       - Inclure des exemples CMake pour chaque mode de build supporté
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================