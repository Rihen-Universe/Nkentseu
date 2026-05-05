// =============================================================================
// NKCore/NkBuiltin.h
// Macros pour fonctionnalités intégrées du compilateur et utilitaires de débogage.
//
// Design :
//  - Accès portable aux builtins du compilateur (__FILE__, __LINE__, __func__, etc.)
//  - Macros de débogage avec localisation automatique (TODO, FIXME, NOTE)
//  - Assertions améliorées avec contexte de compilation
//  - Profiling et instrumentation scope-based via RAII
//  - Logging centralisé avec injection automatique de fichier/ligne/fonction
//  - Macros utilitaires pour tests unitaires et gestion d'erreurs
//  - Compatibilité multi-compilateurs (MSVC, GCC, Clang, standards C/C++)
//
// Intégration :
//  - Utilise NKCore/NkTypes.h pour les types fondamentaux si nécessaire
//  - Compatible avec NKPlatform/NkCompilerDetect.h pour détection de compilateur
//  - Peut être inclus dans tout fichier .c/.cpp sans dépendances circulaires
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CORE_NKBUILTIN_H_INCLUDED
#define NKENTSEU_CORE_NKBUILTIN_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Ce fichier est conçu pour être autonome et sans dépendances lourdes.

    // Inclusion optionnelle des types de base si nécessaire pour les namespaces log/debug
    // #include "NKCore/NkTypes.h"  // Décommenter si nkentseu::log::Error etc. nécessitent des types

    // -------------------------------------------------------------------------
    // SECTION 2 : MACROS BUILTINS DU COMPILATEUR
    // -------------------------------------------------------------------------
    // Accès portable aux informations de compilation standards.

    /**
     * @defgroup BuiltinMacros Macros de Fonctionnalités Intégrées
     * @brief Accès portable aux builtins du compilateur (__FILE__, __LINE__, etc.)
     * @ingroup CoreUtilities
     *
     * Ces macros fournissent un accès uniforme aux informations de compilation
     * quel que soit le compilateur cible (MSVC, GCC, Clang, etc.).
     *
     * @note
     *   - Les macros NKENTSEU_* sont la nomenclature officielle du framework
     *   - Les alias NK_* sont fournis pour compatibilité avec l'ancien code
     *   - NKENTSEU_BUILTIN_FUNCTION varie selon le compilateur pour plus de détails
     *
     * @example
     * @code
     * void DebugPrint() {
     *     printf("[%s:%d] %s\n",
     *         NKENTSEU_BUILTIN_FILE,
     *         NKENTSEU_BUILTIN_LINE,
     *         NKENTSEU_BUILTIN_FUNCTION);
     *     // Output: [myfile.cpp:42] void DebugPrint()
     * }
     * @endcode
     */
    /** @{ */

    /**
     * @brief Nom du fichier source courant (chemin complet ou relatif)
     * @def NKENTSEU_BUILTIN_FILE
     * @ingroup BuiltinMacros
     * @value Équivalent à __FILE__ du compilateur
     *
     * @note Le format du chemin dépend du compilateur et des flags de compilation.
     * Sur MSVC : chemin relatif au projet, sur GCC : peut être absolu selon -I.
     */
    #define NKENTSEU_BUILTIN_FILE __FILE__

    /** @brief Alias de compatibilité pour NKENTSEU_BUILTIN_FILE */
    #define NK_BUILTIN_FILE NKENTSEU_BUILTIN_FILE

    /**
     * @brief Numéro de ligne courante dans le fichier source
     * @def NKENTSEU_BUILTIN_LINE
     * @ingroup BuiltinMacros
     * @value Équivalent à __LINE__ du compilateur (entier)
     */
    #define NKENTSEU_BUILTIN_LINE __LINE__

    /** @brief Alias de compatibilité pour NKENTSEU_BUILTIN_LINE */
    #define NK_BUILTIN_LINE NKENTSEU_BUILTIN_LINE

    /**
     * @brief Nom de la fonction courante (format dépendant du compilateur)
     * @def NKENTSEU_BUILTIN_FUNCTION
     * @ingroup BuiltinMacros
     *
     * Cette macro ne peut être utilisée qu'à l'intérieur d'une fonction.
     * Le format de la chaîne retournée varie selon le compilateur :
     *   - MSVC : nom simple de la fonction (ex: "MyFunction")
     *   - GCC/Clang : signature complète avec types (ex: "void MyClass::MyFunction(int)")
     *   - C99 standard : nom simple via __func__
     *
     * @warning
     *   L'utilisation en dehors d'une fonction peut causer des erreurs de compilation
     *   ou retourner une chaîne vide selon le compilateur.
     *
     * @example
     * @code
     * void Example() {
     *     // GCC/Clang: "void Example()"
     *     // MSVC: "Example"
     *     printf("Called: %s\n", NKENTSEU_BUILTIN_FUNCTION);
     * }
     * @endcode
     */
    #ifdef __cplusplus
        // C++ : sélection du builtin le plus informatif selon le compilateur
        #if defined(_MSC_VER)
            // MSVC : __FUNCTION__ donne le nom simple, suffisant pour la plupart des usages
            #define NKENTSEU_BUILTIN_FUNCTION __FUNCTION__
        #elif defined(__GNUC__) || defined(__clang__)
            // GCC/Clang : __PRETTY_FUNCTION__ inclut la signature complète avec types
            #define NKENTSEU_BUILTIN_FUNCTION __PRETTY_FUNCTION__
        #else
            // Fallback vers le standard C++ : __func__ (nom simple)
            #define NKENTSEU_BUILTIN_FUNCTION __func__
        #endif
    #else
        // C : utilisation de __func__ (standard C99+)
        #if __STDC_VERSION__ >= 199901L
            #define NKENTSEU_BUILTIN_FUNCTION __func__
        #else
            // Compilateurs C très anciens : fallback vers chaîne vide
            #define NKENTSEU_BUILTIN_FUNCTION ""
        #endif
    #endif

    /** @brief Alias de compatibilité pour NKENTSEU_BUILTIN_FUNCTION */
    #ifndef NK_BUILTIN_FUNCTION
        #define NK_BUILTIN_FUNCTION NKENTSEU_BUILTIN_FUNCTION
    #endif

    /**
     * @brief Date de compilation au format "Mmm dd yyyy"
     * @def NKENTSEU_BUILTIN_DATE
     * @ingroup BuiltinMacros
     * @value Équivalent à __DATE__ du compilateur
     *
     * @note Format fixe : "Feb  8 2026" (mois abrégé, jour potentiellement space-padded)
     * Pour un parsing fiable, préférer NKENTSEU_BUILTIN_TIMESTAMP ou une solution runtime.
     */
    #define NKENTSEU_BUILTIN_DATE __DATE__

    /** @brief Alias de compatibilité pour NKENTSEU_BUILTIN_DATE */
    #define NK_BUILTIN_DATE NKENTSEU_BUILTIN_DATE

    /**
     * @brief Heure de compilation au format "hh:mm:ss"
     * @def NKENTSEU_BUILTIN_TIME
     * @ingroup BuiltinMacros
     * @value Équivalent à __TIME__ du compilateur
     *
     * @note Format 24h avec zéros non significatifs : "09:05:32"
     */
    #define NKENTSEU_BUILTIN_TIME __TIME__

    /** @brief Alias de compatibilité pour NKENTSEU_BUILTIN_TIME */
    #define NK_BUILTIN_TIME NKENTSEU_BUILTIN_TIME

    /**
     * @brief Timestamp complet de compilation (date + heure)
     * @def NKENTSEU_BUILTIN_TIMESTAMP
     * @ingroup BuiltinMacros
     * @value Concaténation de __DATE__ et __TIME__
     *
     * @example "Feb  8 2026 09:05:32"
     * @note Utile pour l'empreinte versionnelle des builds
     */
    #define NKENTSEU_BUILTIN_TIMESTAMP __DATE__ " " __TIME__

    /** @brief Alias de compatibilité pour NKENTSEU_BUILTIN_TIMESTAMP */
    #define NK_BUILTIN_TIMESTAMP NKENTSEU_BUILTIN_TIMESTAMP

    /** @} */ // End of BuiltinMacros

    // -------------------------------------------------------------------------
    // SECTION 3 : MACROS D'IDENTIFIANTS UNIQUES COMPILE-TIME
    // -------------------------------------------------------------------------
    // Génération de noms uniques pour éviter les collisions dans les macros.

    /**
     * @defgroup CompileInfoMacros Macros d'Informations de Compilation
     * @brief Utilitaires pour génération d'identifiants uniques à la compilation
     * @ingroup CoreUtilities
     *
     * Ces macros sont utiles pour créer des variables temporaires dans des macros
     * complexes sans risque de collision de noms.
     *
     * @warning
     *   NKENTSEU_UNIQUE_ID utilise __LINE__ : deux appels sur la même ligne
     *   généreront le même ID. Pour une unicité garantie, utiliser des techniques
     *   plus avancées (compteur préprocesseur, __COUNTER__ si disponible).
     *
     * @example
     * @code
     * // Création d'une variable temporaire unique dans une macro
     * #define MY_MACRO(x) \
     *     do { \
     *         auto NKENTSEU_UNIQUE_NAME(tmp_) = (x); \
     *         process(tmp_); \
     *     } while(0)
     * @endcode
     */
    /** @{ */

    /**
     * @brief Identifiant unique basé sur le numéro de ligne
     * @def NKENTSEU_UNIQUE_ID
     * @ingroup CompileInfoMacros
     * @value __LINE__ (peut entrer en collision sur la même ligne)
     *
     * @note Pour une meilleure unicité, certains compilateurs supportent __COUNTER__
     * (non standard mais largement disponible). À activer via détection si nécessaire.
     */
    #define NKENTSEU_UNIQUE_ID __LINE__

    /** @brief Alias de compatibilité pour NKENTSEU_UNIQUE_ID */
    #define NK_UNIQUE_ID NKENTSEU_UNIQUE_ID

    /**
     * @brief Concatène un préfixe avec un identifiant unique
     * @def NKENTSEU_UNIQUE_NAME(prefix)
     * @param prefix Préfixe pour l'identifiant généré
     * @return Nom unique de la forme prefixNNNN
     * @ingroup CompileInfoMacros
     *
     * @note Nécessite que NKENTSEU_CONCAT et NKENTSEU_STRINGIFY soient définis
     * dans un header de base (typiquement NkMacros.h ou NkPreprocessor.h).
     * Si ce n'est pas le cas, définir les macros de concaténation ici :
     * @code
     * #define NKENTSEU_CONCAT_IMPL(a, b) a##b
     * #define NKENTSEU_CONCAT(a, b) NKENTSEU_CONCAT_IMPL(a, b)
     * #define NKENTSEU_STRINGIFY_IMPL(x) #x
     * #define NKENTSEU_STRINGIFY(x) NKENTSEU_STRINGIFY_IMPL(x)
     * @endcode
     */
    #ifndef NKENTSEU_CONCAT
        #define NKENTSEU_CONCAT_IMPL(a, b) a##b
        #define NKENTSEU_CONCAT(a, b) NKENTSEU_CONCAT_IMPL(a, b)
    #endif
    #ifndef NKENTSEU_STRINGIFY
        #define NKENTSEU_STRINGIFY_IMPL(x) #x
        #define NKENTSEU_STRINGIFY(x) NKENTSEU_STRINGIFY_IMPL(x)
    #endif

    #define NKENTSEU_UNIQUE_NAME(prefix) NKENTSEU_CONCAT(prefix, NKENTSEU_UNIQUE_ID)

    /** @brief Alias de compatibilité pour NKENTSEU_UNIQUE_NAME */
    #define NK_UNIQUE_NAME(prefix) NKENTSEU_UNIQUE_NAME(prefix)

    /** @} */ // End of CompileInfoMacros

    // -------------------------------------------------------------------------
    // SECTION 4 : MACROS DE DÉBOGAGE AVEC LOCALISATION
    // -------------------------------------------------------------------------
    // Messages de compilation pour TODO/FIXME/NOTE avec contexte automatique.

    /**
     * @defgroup DebugMacros Macros de Débogage
     * @brief Macros pour messages de compilation et débogage avec localisation
     * @ingroup DebugUtilities
     *
     * Ces macros émettent des avertissements ou notes lors de la compilation
     * pour signaler du code incomplet, des problèmes connus, ou des remarques
     * importantes pour les développeurs.
     *
     * @note
     *   - Les messages apparaissent dans la sortie du compilateur, pas à l'exécution
     *   - Utile pour le suivi des tâches dans un codebase en évolution
     *   - Peut être désactivé en production via définition de NKENTSEU_DISABLE_DEBUG_MACROS
     *
     * @example
     * @code
     * void LegacyFunction() {
     *     NKENTSEU_TODO("Refactor this function to use new API by Q2 2026");
     *     // ... code legacy ...
     *
     *     NKENTSEU_FIXME("Handle edge case when input is null");
     *     if (!input) { /\* ... *\/ }
     *
     *     NKENTSEU_NOTE("Performance: O(n²) - consider optimization for large datasets");
     * }
     * @endcode
     */
    /** @{ */

    /**
     * @brief Émet un message personnalisé lors de la compilation
     * @def NKENTSEU_COMPILE_MESSAGE(msg)
     * @param msg Chaîne littérale du message à afficher
     * @ingroup DebugMacros
     *
     * @note Implémentation dépendante du compilateur :
     *   - MSVC : __pragma(message(...))
     *   - GCC/Clang : _Pragma("message ...")
     *   - Autres : no-op (silencieux)
     */
    #if defined(_MSC_VER)
        #define NKENTSEU_COMPILE_MESSAGE(msg) __pragma(message(msg))
    #elif defined(__GNUC__) || defined(__clang__)
        #define NKENTSEU_COMPILE_MESSAGE(msg) _Pragma(NKENTSEU_STRINGIFY(message msg))
    #else
        #define NKENTSEU_COMPILE_MESSAGE(msg)
    #endif

    /**
     * @brief Marque une tâche TODO avec localisation fichier:ligne
     * @def NKENTSEU_TODO(msg)
     * @param msg Description de la tâche à implémenter
     * @ingroup DebugMacros
     *
     * @example Output de compilation :
     * @code
     * warning: TODO at myfile.cpp:123: Refactor this function to use new API by Q2 2026
     * @endcode
     */
    #define NKENTSEU_TODO(msg) \
    NKENTSEU_COMPILE_MESSAGE( \
        "TODO at " NKENTSEU_BUILTIN_FILE ":" NKENTSEU_STRINGIFY(NKENTSEU_BUILTIN_LINE) ": " msg)

    /**
     * @brief Signale un bug connu ou problème à corriger
     * @def NKENTSEU_FIXME(msg)
     * @param msg Description du problème à résoudre
     * @ingroup DebugMacros
     *
     * @note Différence avec TODO : FIXME indique un bug, TODO une fonctionnalité manquante
     */
    #define NKENTSEU_FIXME(msg) \
    NKENTSEU_COMPILE_MESSAGE( \
        "FIXME at " NKENTSEU_BUILTIN_FILE ":" NKENTSEU_STRINGIFY(NKENTSEU_BUILTIN_LINE) ": " msg)

    /**
     * @brief Ajoute une note informative pour les développeurs
     * @def NKENTSEU_NOTE(msg)
     * @param msg Note ou remarque importante
     * @ingroup DebugMacros
     *
     * @note Moins urgent que TODO/FIXME, utile pour la documentation inline
     */
    #define NKENTSEU_NOTE(msg) \
    NKENTSEU_COMPILE_MESSAGE( \
        "NOTE at " NKENTSEU_BUILTIN_FILE ":" NKENTSEU_STRINGIFY(NKENTSEU_BUILTIN_LINE) ": " msg)

    /** @} */ // End of DebugMacros

    // -------------------------------------------------------------------------
    // SECTION 5 : MACROS D'ASSERTION AVEC CONTEXTE
    // -------------------------------------------------------------------------
    // Assertions améliorées avec informations de débogage automatiques.

    /**
     * @defgroup AssertMacros Macros d'Assertion
     * @brief Assertions avec injection automatique de contexte (fichier, ligne, fonction)
     * @ingroup DebugUtilities
     *
     * Ces macros étendent les assertions standards en ajoutant automatiquement
     * le contexte de compilation pour faciliter le débogage.
     *
     * @warning
     *   Les implémentations ci-dessous sont des squelettes : elles doivent être
     *   connectées au système de logging ou d'assertion du projet pour être utiles.
     *   Voir NKENTSEU_LOG_ERROR_CONTEXT et similaires plus bas.
     *
     * @example
     * @code
     * void ProcessData(int* data, size_t size) {
     *     NKENTSEU_ASSERT_MSG(data != nullptr, "Data pointer cannot be null");
     *     NKENTSEU_SIMPLE_ASSERT(size > 0);
     *     // ... traitement ...
     * }
     *
     * // En mode debug, si l'assertion échoue :
     * // [ERROR] Assertion failed at myfile.cpp:45 in ProcessData: Data pointer cannot be null
     * @endcode
     */
    /** @{ */

    /**
     * @brief Assertion basique avec bloc do-while pour sécurité syntaxique
     * @def NKENTSEU_SIMPLE_ASSERT(condition)
     * @param condition Expression booléenne à vérifier
     * @ingroup AssertMacros
     *
     * @note
     *   - Utilise do-while(0) pour éviter les problèmes avec if/else
     *   - L'implémentation actuelle est un placeholder : connecter à un handler d'erreur
     *   - En production, peut être désactivée via #define NKENTSEU_DISABLE_ASSERTIONS
     */
    #define NKENTSEU_SIMPLE_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            /* Placeholder: connecter à NKENTSEU_LOG_ERROR ou handler d'assertion */ \
            /* Exemple: NKENTSEU_LOG_ERROR("Assertion failed: " #condition); */ \
        } \
    } while (0)

    /**
     * @brief Assertion avec message d'erreur personnalisé
     * @def NKENTSEU_ASSERT_MSG(condition, msg)
     * @param condition Expression booléenne à vérifier
     * @param msg Message explicatif en cas d'échec
     * @ingroup AssertMacros
     *
     * @note Le message est concaténé avec le contexte automatique (fichier, ligne, fonction)
     * via le système de logging pour un débogage complet.
     */
    #define NKENTSEU_ASSERT_MSG(condition, msg) \
    do { \
        if (!(condition)) { \
            /* Placeholder: connecter à NKENTSEU_LOG_ERROR_CONTEXT */ \
            /* Exemple: NKENTSEU_LOG_ERROR_CONTEXT(msg, NKENTSEU_BUILTIN_FILE, NKENTSEU_BUILTIN_LINE, NKENTSEU_BUILTIN_FUNCTION); */ \
        } \
    } while (0)

    /** @} */ // End of AssertMacros

    // -------------------------------------------------------------------------
    // SECTION 6 : MACROS DE PROFILING ET INSTRUMENTATION
    // -------------------------------------------------------------------------
    // Instrumentation scope-based pour analyse de performance.

    /**
     * @defgroup ProfileMacros Macros de Profiling
     * @brief Macros pour profiling et instrumentation de code avec RAII
     * @ingroup PerformanceUtilities
     *
     * Ces macros permettent d'instrumenter facilement des sections de code
     * pour l'analyse de performance, avec activation conditionnelle via
     * les defines NKENTSEU_ENABLE_PROFILING et NKENTSEU_ENABLE_INSTRUMENTATION.
     *
     * @note
     *   - Basé sur le pattern RAII : début à la construction, fin à la destruction
     *   - Zéro coût en production quand les features sont désactivées
     *   - Requiert les classes nkentseu::debug::ProfileScope et InstrumentFunction
     *
     * @example
     * @code
     * void ExpensiveOperation() {
     *     NKENTSEU_PROFILE_SCOPE("ExpensiveOperation");  // Timing de la fonction entière
     *
     *     {
     *         NKENTSEU_PROFILE_SCOPE("Initialization");  // Sous-section
     *         // ... code d'initialisation ...
     *     }
     *
     *     // ... reste du traitement ...
     * }
     *
     * // Avec instrumentation activée :
     * void ProcessRequest() {
     *     NKENTSEU_INSTRUMENT_FUNCTION;  // Log entrée/sortie automatique
     *     // ... traitement ...
     * }
     * @endcode
     */
    /** @{ */

    /**
     * @brief Déclare un scope de profiling nommé
     * @def NKENTSEU_PROFILE_SCOPE(name)
     * @param name Nom littéral pour identifier la section dans les rapports
     * @ingroup ProfileMacros
     *
     * @note
     *   - Crée une variable locale avec nom unique via NKENTSEU_UNIQUE_NAME
     *   - En production sans profiling : se compile en ((void)0) - zéro overhead
     *   - Requiert que nkentseu::debug::ProfileScope soit défini (voir namespace debug plus bas)
     */
    #if defined(NKENTSEU_ENABLE_PROFILING) && !defined(NKENTSEU_DISABLE_PROFILING)
        #define NKENTSEU_PROFILE_SCOPE(name) \
        nkentseu::debug::ProfileScope NKENTSEU_UNIQUE_NAME(profile_scope_)(name)
    #else
        #define NKENTSEU_PROFILE_SCOPE(name) ((void)0)
    #endif

    /**
     * @brief Instrumente l'entrée et sortie d'une fonction
     * @def NKENTSEU_INSTRUMENT_FUNCTION
     * @ingroup ProfileMacros
     *
     * @note
     *   - Doit être placé au début de la fonction (après les déclarations de variables)
     *   - Utilise NKENTSEU_BUILTIN_FUNCTION pour le nom automatique
     *   - Requiert nkentseu::debug::InstrumentFunction (voir namespace debug)
     */
    #if defined(NKENTSEU_ENABLE_INSTRUMENTATION) && !defined(NKENTSEU_DISABLE_INSTRUMENTATION)
        #define NKENTSEU_INSTRUMENT_FUNCTION \
        nkentseu::debug::InstrumentFunction NKENTSEU_UNIQUE_NAME(instrument_)(NKENTSEU_BUILTIN_FUNCTION)
    #else
        #define NKENTSEU_INSTRUMENT_FUNCTION ((void)0)
    #endif

    /** @} */ // End of ProfileMacros

    // -------------------------------------------------------------------------
    // SECTION 7 : MACROS DE LOGGING AVEC CONTEXTE AUTOMATIQUE
    // -------------------------------------------------------------------------
    // Logging centralisé avec injection de fichier/ligne/fonction.

    /**
     * @defgroup ErrorMacros Macros de Gestion d'Erreurs et Logging
     * @brief Macros pour logging hiérarchisé avec contexte automatique
     * @ingroup LoggingUtilities
     *
     * Ces macros simplifient l'ajout de logs en injectant automatiquement
     * le contexte de compilation (fichier, ligne, fonction) dans chaque appel.
     *
     * @note
     *   - Trois niveaux : Error (critique), Warning (attention), Info (diagnostique)
     *   - Les macros *_CONTEXT sont les implémentations réelles à connecter au logger
     *   - En production, peut être redirigé vers un système de logging asynchrone
     *
     * @example
     * @code
     * bool LoadConfig(const char* path) {
     *     if (!path) {
     *         NKENTSEU_LOG_ERROR("Config path is null");
     *         return false;
     *     }
     *
     *     if (!FileExists(path)) {
     *         NKENTSEU_LOG_WARNING("Config file not found, using defaults");
     *         return LoadDefaultConfig();
     *     }
     *
     *     NKENTSEU_LOG_INFO("Loading config from: %s", path);
     *     return ParseConfig(path);
     * }
     *
     * // Output typique :
     * // [ERROR] config.cpp:42 in LoadConfig: Config path is null
     * // [WARNING] config.cpp:47 in LoadConfig: Config file not found, using defaults
     * // [INFO] config.cpp:51 in LoadConfig: Loading config from: /etc/app/config.ini
     * @endcode
     */
    /** @{ */

    /**
     * @brief Log de niveau ERROR avec contexte automatique
     * @def NKENTSEU_LOG_ERROR(msg)
     * @param msg Message d'erreur (supporte formatage printf-style si implémenté)
     * @ingroup ErrorMacros
     *
     * @note Redirige vers NKENTSEU_LOG_ERROR_CONTEXT avec contexte injecté
     */
    #define NKENTSEU_LOG_ERROR(msg) \
    NKENTSEU_LOG_ERROR_CONTEXT(msg, NKENTSEU_BUILTIN_FILE, NKENTSEU_BUILTIN_LINE, NKENTSEU_BUILTIN_FUNCTION)

    /**
     * @brief Log de niveau WARNING avec contexte automatique
     * @def NKENTSEU_LOG_WARNING(msg)
     * @param msg Message d'avertissement
     * @ingroup ErrorMacros
     */
    #define NKENTSEU_LOG_WARNING(msg) \
    NKENTSEU_LOG_WARNING_CONTEXT(msg, NKENTSEU_BUILTIN_FILE, NKENTSEU_BUILTIN_LINE, NKENTSEU_BUILTIN_FUNCTION)

    /**
     * @brief Log de niveau INFO avec contexte automatique
     * @def NKENTSEU_LOG_INFO(msg)
     * @param msg Message informatif
     * @ingroup ErrorMacros
     */
    #define NKENTSEU_LOG_INFO(msg) \
    NKENTSEU_LOG_INFO_CONTEXT(msg, NKENTSEU_BUILTIN_FILE, NKENTSEU_BUILTIN_LINE, NKENTSEU_BUILTIN_FUNCTION)

    // -------------------------------------------------------------------------
    // Implémentations des macros *_CONTEXT (à connecter au système de logging)
    // -------------------------------------------------------------------------
    // Ces définitions sont des placeholders : les remplacer par des appels
    // au logger réel du projet (spdlog, custom logger, etc.)

    /**
     * @brief Implémentation réelle du log ERROR avec contexte explicite
     * @def NKENTSEU_LOG_ERROR_CONTEXT(msg, file, line, func)
     * @param msg Message à logger
     * @param file Nom du fichier source
     * @param line Numéro de ligne
     * @param func Nom de la fonction
     * @ingroup ErrorMacros
     *
     * @note Cette macro doit être redéfinie pour utiliser le logger du projet.
     * Exemple avec un logger hypothétique :
     * @code
     * #define NKENTSEU_LOG_ERROR_CONTEXT(msg, file, line, func) \
     *     nkentseu::log::GetLogger().error("[{}:{}] {} - {}", file, line, func, msg)
     * @endcode
     */
    #ifndef NKENTSEU_LOG_ERROR_CONTEXT_IMPL
        #define NKENTSEU_LOG_ERROR_CONTEXT(msg, file, line, func) \
        nkentseu::log::Error(msg, file, line, func)
    #else
        #define NKENTSEU_LOG_ERROR_CONTEXT(msg, file, line, func) \
        NKENTSEU_LOG_ERROR_CONTEXT_IMPL(msg, file, line, func)
    #endif

    /**
     * @brief Implémentation réelle du log WARNING avec contexte explicite
     * @def NKENTSEU_LOG_WARNING_CONTEXT(msg, file, line, func)
     * @ingroup ErrorMacros
     */
    #ifndef NKENTSEU_LOG_WARNING_CONTEXT_IMPL
        #define NKENTSEU_LOG_WARNING_CONTEXT(msg, file, line, func) \
        nkentseu::log::Warning(msg, file, line, func)
    #else
        #define NKENTSEU_LOG_WARNING_CONTEXT(msg, file, line, func) \
        NKENTSEU_LOG_WARNING_CONTEXT_IMPL(msg, file, line, func)
    #endif

    /**
     * @brief Implémentation réelle du log INFO avec contexte explicite
     * @def NKENTSEU_LOG_INFO_CONTEXT(msg, file, line, func)
     * @ingroup ErrorMacros
     */
    #ifndef NKENTSEU_LOG_INFO_CONTEXT_IMPL
        #define NKENTSEU_LOG_INFO_CONTEXT(msg, file, line, func) \
        nkentseu::log::Info(msg, file, line, func)
    #else
        #define NKENTSEU_LOG_INFO_CONTEXT(msg, file, line, func) \
        NKENTSEU_LOG_INFO_CONTEXT_IMPL(msg, file, line, func)
    #endif

    /** @} */ // End of ErrorMacros

    // -------------------------------------------------------------------------
    // SECTION 8 : MACROS UTILITAIRES AVEC CONTEXTE
    // -------------------------------------------------------------------------
    // Helpers pour gestion de flux de contrôle et débogage.

    /**
     * @defgroup ContextMacros Macros Utilitaires avec Contexte
     * @brief Macros pour gestion de code avec injection automatique de contexte
     * @ingroup CoreUtilities
     *
     * Ces macros facilitent les patterns courants de gestion d'erreurs et
     * de débogage en encapsulant la logique répétitive avec contexte.
     *
     * @example
     * @code
     * Result ProcessData(Data* data) {
     *     NKENTSEU_CHECK_RETURN(data != nullptr, Result::Error_NullPointer);
     *     NKENTSEU_CHECK_RETURN(data->IsValid(), Result::Error_InvalidData);
     *
     *     for (auto& item : data->items) {
     *         NKENTSEU_CHECK_CONTINUE(item.IsProcessable());  // Skip les items invalides
     *         ProcessItem(item);
     *     }
     *
     *     return Result::Success;
     * }
     * @endcode
     */
    /** @{ */

    /**
     * @brief Déclare une variable avec suppression de warning "unused"
     * @def NKENTSEU_DECLARE_WITH_CONTEXT(type, name, value)
     * @param type Type de la variable
     * @param name Nom de la variable
     * @param value Valeur d'initialisation
     * @ingroup ContextMacros
     *
     * @note Utile pour les variables de débogage qui ne sont utilisées qu'en mode debug
     * ou pour les paramètres temporairement non utilisés pendant le développement.
     */
    #ifndef NKENTSEU_UNUSED
        #if defined(_MSC_VER)
            #define NKENTSEU_UNUSED(var) (void)(var)
        #else
            #define NKENTSEU_UNUSED(var) (void)(var)
        #endif
    #endif

    #define NKENTSEU_DECLARE_WITH_CONTEXT(type, name, value) \
    type name = (value); \
    NKENTSEU_UNUSED(name)

    /**
     * @brief Vérifie une condition et retourne une valeur si échec
     * @def NKENTSEU_CHECK_RETURN(condition, retval)
     * @param condition Expression à évaluer
     * @param retval Valeur de retour en cas d'échec
     * @ingroup ContextMacros
     *
     * @note
     *   - Utilise do-while(0) pour sécurité syntaxique dans tous les contextes
     *   - Log un warning automatique avec contexte via NKENTSEU_LOG_WARNING
     *   - Idéal pour la validation de préconditions en début de fonction
     */
    #define NKENTSEU_CHECK_RETURN(condition, retval) \
    do { \
        if (!(condition)) { \
            NKENTSEU_LOG_WARNING("Check failed: " #condition); \
            return retval; \
        } \
    } while (0)

    /**
     * @brief Vérifie une condition et continue la boucle si échec
     * @def NKENTSEU_CHECK_CONTINUE(condition)
     * @param condition Expression à évaluer
     * @ingroup ContextMacros
     *
     * @note
     *   - Doit être utilisé uniquement à l'intérieur d'une boucle (for/while/do-while)
     *   - Log un warning avec contexte pour tracer les skips
     *   - Utile pour filtrer des éléments dans une itération sans interrompre le flux
     */
    #define NKENTSEU_CHECK_CONTINUE(condition) \
    do { \
        if (!(condition)) { \
            NKENTSEU_LOG_WARNING("Check failed: " #condition); \
            continue; \
        } \
    } while (0)

    /** @} */ // End of ContextMacros

    // -------------------------------------------------------------------------
    // SECTION 9 : MACROS POUR TESTS UNITAIRES
    // -------------------------------------------------------------------------
    // Assertions simplifiées pour frameworks de test maison.

    /**
     * @defgroup TestMacros Macros pour Tests Unitaires
     * @brief Assertions et helpers pour tests unitaires avec messages explicites
     * @ingroup TestingUtilities
     *
     * Ces macros sont conçues pour être utilisées dans des fonctions de test
     * retournant bool (true = succès, false = échec). Elles loggent automatiquement
     * les échecs avec contexte pour faciliter le débogage des tests.
     *
     * @note
     *   - NKENTSEU_TEST_ASSERT : assertion générique avec message personnalisé
     *   - NKENTSEU_TEST_EQUAL : assertion d'égalité avec affichage des valeurs
     *   - Pour des frameworks plus avancés, intégrer avec Google Test, Catch2, etc.
     *
     * @example
     * @code
     * bool Test_StringUtils() {
     *     NKENTSEU_TEST_EQUAL(ToUpper("hello"), "HELLO", "ToUpper should convert to uppercase");
     *     NKENTSEU_TEST_ASSERT(IsValidEmail("test@example.com"), "Email validation failed");
     *     return true;  // Tous les tests ont passé
     * }
     *
     * // En cas d'échec :
     * // [ERROR] test.cpp:15 in Test_StringUtils: Test failed: ToUpper should convert... (actual: ToUpper("hello"), expected: "HELLO")
     * @endcode
     */
    /** @{ */

    /**
     * @brief Assertion de test avec message d'erreur personnalisé
     * @def NKENTSEU_TEST_ASSERT(condition, msg)
     * @param condition Condition à vérifier
     * @param msg Message affiché en cas d'échec
     * @ingroup TestMacros
     *
     * @note
     *   - Retourne false immédiatement en cas d'échec (conçu pour fonctions bool)
     *   - Log l'erreur via NKENTSEU_LOG_ERROR avec contexte automatique
     *   - Pour continuer après un échec, utiliser une macro différente ou un framework
     */
    #define NKENTSEU_TEST_ASSERT(condition, msg) \
    do { \
        if (!(condition)) { \
            NKENTSEU_LOG_ERROR("Test failed: " msg); \
            return false; \
        } \
    } while (0)

    /**
     * @brief Assertion d'égalité avec affichage des valeurs comparées
     * @def NKENTSEU_TEST_EQUAL(actual, expected, msg)
     * @param actual Valeur obtenue
     * @param expected Valeur attendue
     * @param msg Message de base en cas d'échec
     * @ingroup TestMacros
     *
     * @note
     *   - Utilise NKENTSEU_STRINGIFY pour afficher les expressions, pas les valeurs
     *   - Pour afficher les valeurs réelles, nécessiterait une surcharge template ou macro avancée
     *   - Fonctionne mieux avec des types ayant operator<< ou conversion vers string
     */
    #define NKENTSEU_TEST_EQUAL(actual, expected, msg) \
    NKENTSEU_TEST_ASSERT( \
        (actual) == (expected), \
        msg " (actual: " NKENTSEU_STRINGIFY(actual) ", expected: " NKENTSEU_STRINGIFY(expected) ")")

    /** @} */ // End of TestMacros

    // -------------------------------------------------------------------------
    // SECTION 10 : NAMESPACES POUR CLASSES D'AIDE (DEBUG/LOGGING)
    // -------------------------------------------------------------------------
    // Déclarations de classes utilitaires pour profiling et logging.

    namespace nkentseu {

        // Indentation niveau 1 : namespace debug
        namespace debug {

            // ====================================================================
            // CLASSE PROFILESCOPE (PROFILING RAII)
            // ====================================================================

            #if defined(NKENTSEU_ENABLE_PROFILING) && !defined(NKENTSEU_DISABLE_PROFILING)
            /**
             * @brief Scope guard pour profiling automatique de sections de code
             * @class ProfileScope
             * @ingroup DebugClasses
             *
             * Classe RAII qui enregistre le temps d'entrée et de sortie d'une
             * section de code pour analyse de performance.
             *
             * @note
             *   - Construction : enregistre timestamp de début
             *   - Destruction : calcule durée et envoie aux statistiques de profiling
             *   - Thread-safe si l'implémentation sous-jacente l'est
             *
             * @warning
             *   Ne pas utiliser dans des boucles très fréquentes sans activation conditionnelle :
             *   même avec overhead minimal, l'accumulation peut impacter les performances.
             *
             * @example
             * @code
             * void ProcessLargeDataset() {
             *     nkentseu::debug::ProfileScope scope("ProcessLargeDataset");
             *     // ... traitement ...
             * }  // Durée automatiquement loggée à la sortie du scope
             * @endcode
             */
            class ProfileScope {
                public:
                    /**
                     * @brief Constructeur : démarre le timing
                     * @param name Nom identifiant la section dans les rapports
                     */
                    explicit ProfileScope(const char *name)
                        : m_name(name)
                        , m_startTime(GetCurrentTimestamp())
                    {
                        // Optionnel : notifier le début du profiling
                        // Profiler::OnSectionStart(m_name);
                    }

                    /**
                     * @brief Destructeur : termine le timing et enregistre la durée
                     */
                    ~ProfileScope()
                    {
                        const auto endTime = GetCurrentTimestamp();
                        const auto duration = endTime - m_startTime;
                        // Optionnel : enregistrer la durée
                        // Profiler::RecordSample(m_name, duration);
                    }

                private:
                    /** @brief Récupère un timestamp haute résolution */
                    static inline uint64_t GetCurrentTimestamp() noexcept
                    {
                        // Placeholder : remplacer par implémentation réelle
                        // Ex: std::chrono::high_resolution_clock::now().time_since_epoch().count()
                        return 0;
                    }

                    const char *m_name;    ///< Nom de la section profilée
                    uint64_t m_startTime;  ///< Timestamp de début (unités dépend de l'implémentation)
            };
            #endif  // NKENTSEU_ENABLE_PROFILING

            // ====================================================================
            // CLASSE INSTRUMENTFUNCTION (INSTRUMENTATION DE FONCTIONS)
            // ====================================================================

            #if defined(NKENTSEU_ENABLE_INSTRUMENTATION) && !defined(NKENTSEU_DISABLE_INSTRUMENTATION)
            /**
             * @brief Scope guard pour instrumentation d'entrée/sortie de fonction
             * @class InstrumentFunction
             * @ingroup DebugClasses
             *
             * Classe RAII qui log automatiquement l'entrée et la sortie d'une
             * fonction, utile pour le tracing d'exécution et le debugging.
             *
             * @note
             *   - Construction : log "Entering functionName"
             *   - Destruction : log "Leaving functionName" avec durée optionnelle
             *   - Peut être combiné avec ProfileScope pour profiling + tracing
             *
             * @example
             * @code
             * void ComplexOperation() {
             *     nkentseu::debug::InstrumentFunction instrument(__func__);
             *     // Logs automatiques à l'entrée et sortie
             *     // [TRACE] Entering ComplexOperation
             *     // ... traitement ...
             *     // [TRACE] Leaving ComplexOperation (duration: 15.3ms)
             * }
             * @endcode
             */
            class InstrumentFunction {
                public:
                    /**
                     * @brief Constructeur : log l'entrée dans la fonction
                     * @param functionName Nom de la fonction (typiquement via __func__)
                     */
                    explicit InstrumentFunction(const char *functionName)
                        : m_functionName(functionName)
                        , m_startTime(GetCurrentTimestamp())
                    {
                        // Optionnel : log d'entrée
                        // Logger::Trace("Entering %s", m_functionName);
                    }

                    /**
                     * @brief Destructeur : log la sortie avec durée optionnelle
                     */
                    ~InstrumentFunction()
                    {
                        const auto endTime = GetCurrentTimestamp();
                        const auto duration = endTime - m_startTime;
                        // Optionnel : log de sortie avec durée
                        // Logger::Trace("Leaving %s (duration: %llu units)", m_functionName, duration);
                    }

                private:
                    /** @brief Récupère un timestamp pour calcul de durée */
                    static inline uint64_t GetCurrentTimestamp() noexcept
                    {
                        // Placeholder : même implémentation que ProfileScope
                        return 0;
                    }

                    const char *m_functionName;  ///< Nom de la fonction instrumentée
                    uint64_t m_startTime;        ///< Timestamp d'entrée
            };
            #endif  // NKENTSEU_ENABLE_INSTRUMENTATION

        } // namespace debug

        // Indentation niveau 1 : namespace log
        namespace log {

            // ====================================================================
            // FONCTIONS DE LOGGING SIMPLIFIÉES (PLACEHOLDERS)
            // ====================================================================

            /**
             * @brief Log de niveau ERROR avec contexte complet
             * @param msg Message d'erreur
             * @param file Fichier source où l'erreur s'est produite
             * @param line Numéro de ligne
             * @param func Nom de la fonction
             * @ingroup LoggingFunctions
             *
             * @note
             *   Cette fonction doit être implémentée pour rediriger vers le système
             *   de logging réel du projet (console, fichier, réseau, etc.).
             *   L'implémentation ci-dessous est un placeholder.
             *
             * @example d'implémentation réelle :
             * @code
             * void Error(const char* msg, const char* file, int line, const char* func) {
             *     fprintf(stderr, "[ERROR] %s:%d in %s: %s\n", file, line, func, msg);
             *     // Ou avec un logger asynchrone :
             *     // AsyncLogger::GetInstance().push(LogLevel::Error, file, line, func, msg);
             * }
             * @endcode
             */
            inline void Error(const char *msg, const char *file, int line, const char *func)
            {
                // Placeholder : remplacer par implémentation réelle
                // Exemple minimal pour compilation :
                (void)msg; (void)file; (void)line; (void)func;
                // fprintf(stderr, "[ERROR] %s:%d in %s: %s\n", file, line, func, msg);
            }

            /**
             * @brief Log de niveau WARNING avec contexte complet
             * @param msg Message d'avertissement
             * @param file Fichier source
             * @param line Numéro de ligne
             * @param func Nom de la fonction
             * @ingroup LoggingFunctions
             */
            inline void Warning(const char *msg, const char *file, int line, const char *func)
            {
                // Placeholder : même pattern que Error()
                (void)msg; (void)file; (void)line; (void)func;
                // fprintf(stderr, "[WARNING] %s:%d in %s: %s\n", file, line, func, msg);
            }

            /**
             * @brief Log de niveau INFO avec contexte complet
             * @param msg Message informatif
             * @param file Fichier source
             * @param line Numéro de ligne
             * @param func Nom de la fonction
             * @ingroup LoggingFunctions
             */
            inline void Info(const char *msg, const char *file, int line, const char *func)
            {
                // Placeholder : même pattern que Error()
                (void)msg; (void)file; (void)line; (void)func;
                // fprintf(stdout, "[INFO] %s:%d in %s: %s\n", file, line, func, msg);
            }

        } // namespace log

    } // namespace nkentseu

    // -------------------------------------------------------------------------
    // SECTION 11 : MACROS DE COMPATIBILITÉ ET ALIASES LEGACY
    // -------------------------------------------------------------------------
    // Alias pour transition douce depuis l'ancienne nomenclature.

    /**
     * @defgroup CompatibilityMacros Macros de Compatibilité
     * @brief Alias pour compatibilité avec l'ancien code utilisant NK_* au lieu de NKENTSEU_*
     * @ingroup CompatibilityLayer
     *
     * Ces macros sont fournies pour faciliter la migration vers la nomenclature
     * officielle NKENTSEU_*. Elles seront dépréciées dans une future version.
     *
     * @deprecated Préférer les macros NKENTSEU_* pour tout nouveau code
     *
     * @example
     * @code
     * // Ancien code (encore supporté)
     * printf("Error at %s:%d\n", NK_CURRENT_FILE, NK_CURRENT_LINE);
     *
     * // Nouveau code recommandé
     * printf("Error at %s:%d\n", NKENTSEU_BUILTIN_FILE, NKENTSEU_BUILTIN_LINE);
     * @endcode
     */
    /** @{ */

    /**
     * @brief Alias pour NKENTSEU_BUILTIN_FUNCTION (compatibilité)
     * @def NKENTSEU_CURRENT_FUNCTION
     * @ingroup CompatibilityMacros
     * @deprecated Utiliser NKENTSEU_BUILTIN_FUNCTION à la place
     */
    #define NKENTSEU_CURRENT_FUNCTION NKENTSEU_BUILTIN_FUNCTION

    /**
     * @brief Alias pour NKENTSEU_BUILTIN_FILE (compatibilité)
     * @def NKENTSEU_CURRENT_FILE
     * @ingroup CompatibilityMacros
     * @deprecated Utiliser NKENTSEU_BUILTIN_FILE à la place
     */
    #define NKENTSEU_CURRENT_FILE NKENTSEU_BUILTIN_FILE

    /**
     * @brief Alias pour NKENTSEU_BUILTIN_LINE (compatibilité)
     * @def NKENTSEU_CURRENT_LINE
     * @ingroup CompatibilityMacros
     * @deprecated Utiliser NKENTSEU_BUILTIN_LINE à la place
     */
    #define NKENTSEU_CURRENT_LINE NKENTSEU_BUILTIN_LINE

    /** @} */ // End of CompatibilityMacros

#endif // NKENTSEU_CORE_NKBUILTIN_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION DE NKBUILTIN.H
// =============================================================================
// Ce fichier fournit des macros utilitaires pour le débogage, le logging et
// l'instrumentation du code NKCore.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Utilisation des builtins de compilation pour logging manuel
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkBuiltin.h"
    #include <cstdio>

    void DebugPrintContext() {
        // Affichage simple avec contexte automatique
        printf("[%s:%d] %s\n",
            NKENTSEU_BUILTIN_FILE,
            NKENTSEU_BUILTIN_LINE,
            NKENTSEU_BUILTIN_FUNCTION);
        // Output typique : [myproject/src/main.cpp:42] void DebugPrintContext()
    }

    void LogBuildInfo() {
        // Affichage des informations de build pour diagnostic
        printf("Build timestamp: %s\n", NKENTSEU_BUILTIN_TIMESTAMP);
        printf("Compiled on: %s at %s\n", NKENTSEU_BUILTIN_DATE, NKENTSEU_BUILTIN_TIME);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Macros de débogage TODO/FIXME pendant le développement
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkBuiltin.h"

    void LegacyDataProcessor(Data* data) {
        // Signalisation de dette technique pour revue future
        NKENTSEU_TODO("Migrate to new DataProcessorV2 API by Q3 2026");

        // Marqueur de bug connu avec description
        NKENTSEU_FIXME("Handle null data pointer gracefully instead of crashing");
        if (!data) {
            // Workaround temporaire
            return;
        }

        // Note informative pour les mainteneurs
        NKENTSEU_NOTE("Complexity: O(n²) - consider hash-based optimization for large datasets");

        // ... implémentation legacy ...
    }

    // Output de compilation (GCC/Clang) :
    // note: TODO at processor.cpp:15: Migrate to new DataProcessorV2 API by Q3 2026
    // note: FIXME at processor.cpp:18: Handle null data pointer gracefully instead of crashing
    // note: NOTE at processor.cpp:25: Complexity: O(n²) - consider hash-based optimization...
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Assertions améliorées pour validation de préconditions
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkBuiltin.h"

    Result InitializeSubsystem(Config* config, Logger* logger) {
        // Validation avec message explicite et contexte automatique
        NKENTSEU_ASSERT_MSG(config != nullptr, "Config pointer cannot be null");
        NKENTSEU_ASSERT_MSG(logger != nullptr, "Logger pointer cannot be null");
        NKENTSEU_ASSERT_MSG(config->IsValid(), "Config validation failed");

        // Assertion simple pour invariants internes
        NKENTSEU_SIMPLE_ASSERT(config->GetVersion() >= MIN_SUPPORTED_VERSION);

        // ... initialisation ...
        return Result::Success;
    }

    // En cas d'échec (après connexion à un logger réel) :
    // [ERROR] init.cpp:23 in InitializeSubsystem: Config pointer cannot be null
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Profiling scope-based pour analyse de performance
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkBuiltin.h"

    // Activer le profiling via flag de compilation :
    // -DNKENTSEU_ENABLE_PROFILING

    void ProcessFrame(const Frame& frame) {
        // Profiling de la fonction entière
        NKENTSEU_PROFILE_SCOPE("ProcessFrame");

        // Sous-sections pour analyse granulaire
        {
            NKENTSEU_PROFILE_SCOPE("InputValidation");
            if (!frame.IsValid()) {
                return;
            }
        }

        {
            NKENTSEU_PROFILE_SCOPE("PhysicsUpdate");
            UpdatePhysics(frame.GetDeltaTime());
        }

        {
            NKENTSEU_PROFILE_SCOPE("Rendering");
            RenderScene(frame.GetViewMatrix());
        }

        // À l'exécution avec profiling activé :
        // [PROFILER] ProcessFrame: 16.2ms
        //   ├─ InputValidation: 0.3ms
        //   ├─ PhysicsUpdate: 8.1ms
        //   └─ Rendering: 7.8ms
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Logging hiérarchisé avec contexte automatique
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkBuiltin.h"
    // Note: NKENTSEU_LOG_ERROR_CONTEXT etc. doivent être connectés à un logger réel

    bool LoadAsset(const char* assetPath, Asset& outAsset) {
        NKENTSEU_LOG_INFO("Loading asset: %s", assetPath);

        if (!assetPath || !*assetPath) {
            NKENTSEU_LOG_ERROR("Asset path is null or empty");
            return false;
        }

        FileHandle file = OpenFile(assetPath);
        if (!file.IsValid()) {
            NKENTSEU_LOG_WARNING("Asset file not found: %s", assetPath);
            return false;
        }

        if (!ParseAsset(file, outAsset)) {
            NKENTSEU_LOG_ERROR("Failed to parse asset: %s", assetPath);
            return false;
        }

        NKENTSEU_LOG_INFO("Successfully loaded asset: %s", assetPath);
        return true;
    }

    // Output typique avec logger connecté :
    // [INFO] asset_loader.cpp:45 in LoadAsset: Loading asset: textures/brick.png
    // [WARNING] asset_loader.cpp:52 in LoadAsset: Asset file not found: textures/brick.png
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Gestion de flux de contrôle avec macros utilitaires
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkBuiltin.h"

    Result ProcessBatch(Batch* batch) {
        // Validation rapide avec retour automatique
        NKENTSEU_CHECK_RETURN(batch != nullptr, Result::Error_NullPointer);
        NKENTSEU_CHECK_RETURN(batch->itemCount > 0, Result::Error_EmptyBatch);

        size_t processedCount = 0;
        for (size_t i = 0; i < batch->itemCount; ++i) {
            Item& item = batch->items[i];

            // Skip les items invalides sans interrompre la boucle
            NKENTSEU_CHECK_CONTINUE(item.IsValid());

            // Déclaration de variable de debug avec suppression de warning "unused"
            NKENTSEU_DECLARE_WITH_CONTEXT(int, debugCounter, static_cast<int>(i));

            if (!ProcessItem(item)) {
                NKENTSEU_LOG_WARNING("Failed to process item %zu", i);
                continue;
            }

            ++processedCount;
        }

        return (processedCount > 0) ? Result::Success : Result::Warning_NoItemsProcessed;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Tests unitaires simples avec macros dédiées
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkBuiltin.h"

    // Fonction de test retournant bool : true = tous les tests passés
    bool Test_MathUtils() {
        // Assertion d'égalité avec affichage des expressions en cas d'échec
        NKENTSEU_TEST_EQUAL(Clamp(5, 0, 10), 5, "Clamp should return value within range");
        NKENTSEU_TEST_EQUAL(Clamp(-1, 0, 10), 0, "Clamp should return min when value < min");
        NKENTSEU_TEST_EQUAL(Clamp(15, 0, 10), 10, "Clamp should return max when value > max");

        // Assertion générique avec message personnalisé
        NKENTSEU_TEST_ASSERT(IsPowerOfTwo(256), "256 should be recognized as power of two");
        NKENTSEU_TEST_ASSERT(!IsPowerOfTwo(255), "255 should not be power of two");

        // Tous les tests ont passé
        return true;
    }

    // En cas d'échec du premier test :
    // [ERROR] test_math.cpp:12 in Test_MathUtils: Test failed: Clamp should return... (actual: Clamp(-1,0,10), expected: 0)

    // Exécution des tests
    void RunAllTests() {
        if (Test_MathUtils()) {
            printf("✓ MathUtils tests passed\n");
        } else {
            printf("✗ MathUtils tests failed\n");
        }
        // ... autres tests ...
    }
*/

// -----------------------------------------------------------------------------
// Exemple 8 : Combinaison avec d'autres modules NKCore
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkBuiltin.h"
    #include "NKCore/NkTypes.h"
    #include "NKPlatform/NkFoundationLog.h"  // Logger réel du projet

    // Redéfinition des macros de logging pour utiliser le logger du projet
    #define NKENTSEU_LOG_ERROR_CONTEXT_IMPL(msg, file, line, func) \
        NK_FOUNDATION_LOG_ERROR("[%s:%d] %s - %s", file, line, func, msg)

    #define NKENTSEU_LOG_WARNING_CONTEXT_IMPL(msg, file, line, func) \
        NK_FOUNDATION_LOG_WARNING("[%s:%d] %s - %s", file, line, func, msg)

    #define NKENTSEU_LOG_INFO_CONTEXT_IMPL(msg, file, line, func) \
        NK_FOUNDATION_LOG_INFO("[%s:%d] %s - %s", file, line, func, msg)

    // Maintenant les macros NKENTSEU_LOG_* utilisent automatiquement le logger réel
    class ResourceManager {
    public:
        bool LoadResource(const char* path) {
            NKENTSEU_LOG_INFO("Loading resource: %s", path);

            NKENTSEU_CHECK_RETURN(path != nullptr, false);

            // ... chargement ...

            return true;
        }
    };
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================