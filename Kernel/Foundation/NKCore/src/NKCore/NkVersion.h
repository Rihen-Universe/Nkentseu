// =============================================================================
// NKCore/NkVersion.h
// Gestion du versioning du framework NKCore.
//
// Design :
//  - Définition des macros de version (major/minor/patch) avec encodage 32-bit
//  - Séparation version framework vs version API publique
//  - Macros de compatibilité pour les checks conditionnels à la compilation
//  - Intégration avec NKPlatform pour les attributs deprecated/hidden
//  - Informations de build automatisées via __DATE__/__TIME__
//  - Header-only, aucune dépendance runtime
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CORE_NKVERSION_H
#define NKENTSEU_CORE_NKVERSION_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules requis pour les macros utilitaires et d'export.
    // Chemins normalisés avec préfixe NKCore/ ou NKPlatform/.

    #include "NKCore/NkMacros.h"
    #include "NKPlatform/NkPlatformExport.h"

    // =========================================================================
    // SECTION 2 : VERSION DU FRAMEWORK NKCORE
    // =========================================================================
    // Définition des composants de version avec support de surcharge par build system.
    // Format SemVer : MAJOR.MINOR.PATCH (https://semver.org/)

    /**
     * @defgroup CoreVersion Version NKCore
     * @brief Macros définissant la version du framework NKCore
     *
     * Le versioning suit le schéma SemVer :
     *  - MAJOR : Changements incompatibles (breaking changes)
     *  - MINOR : Nouvelles fonctionnalités rétrocompatibles
     *  - PATCH : Corrections de bugs rétrocompatibles
     *
     * @see https://semver.org/
     */

    /**
     * @brief Version majeure (breaking changes)
     * @def NKENTSEU_VERSION_CORE_MAJOR
     * @ingroup CoreVersion
     * @value 1 (par défaut)
     *
     * Peut être surchargé via -DNKENTSEU_VERSION_CORE_MAJOR=X dans le build system.
     *
     * @example CMake
     * @code
     * target_compile_definitions(nkcore PRIVATE NKENTSEU_VERSION_CORE_MAJOR=2)
     * @endcode
     */
    #ifndef NKENTSEU_VERSION_CORE_MAJOR
        #define NKENTSEU_VERSION_CORE_MAJOR 1
    #endif

    /**
     * @brief Version mineure (nouvelles fonctionnalités)
     * @def NKENTSEU_VERSION_CORE_MINOR
     * @ingroup CoreVersion
     * @value 0 (par défaut)
     *
     * Peut être surchargé via -DNKENTSEU_VERSION_CORE_MINOR=X.
     */
    #ifndef NKENTSEU_VERSION_CORE_MINOR
        #define NKENTSEU_VERSION_CORE_MINOR 0
    #endif

    /**
     * @brief Version patch (corrections de bugs)
     * @def NKENTSEU_VERSION_CORE_PATCH
     * @ingroup CoreVersion
     * @value 0 (par défaut)
     *
     * Peut être surchargé via -DNKENTSEU_VERSION_CORE_PATCH=X.
     */
    #ifndef NKENTSEU_VERSION_CORE_PATCH
        #define NKENTSEU_VERSION_CORE_PATCH 0
    #endif

    /**
     * @brief Version complète encodée sur 32 bits
     * @def NKENTSEU_VERSION_CORE
     * @ingroup CoreVersion
     * @value 0xMMmmpppp (Major 8 bits, Minor 8 bits, Patch 16 bits)
     *
     * Format binaire : [00000000][MAJOR][MINOR][PATCH]
     * Permet des comparaisons numériques efficaces à la compilation.
     *
     * @example
     * @code
     * // Version 1.2.3 → 0x01020003
     * #if NKENTSEU_VERSION_CORE >= 0x01020000
     *     // Fonctionnalité disponible depuis 1.2.0
     * #endif
     * @endcode
     */
    #define NKENTSEU_VERSION_CORE \
        NKENTSEU_VERSION_ENCODE( \
            NKENTSEU_VERSION_CORE_MAJOR, \
            NKENTSEU_VERSION_CORE_MINOR, \
            NKENTSEU_VERSION_CORE_PATCH \
        )

    /**
     * @brief Version sous forme de chaîne de caractères
     * @def NKENTSEU_VERSION_CORE_STRING
     * @ingroup CoreVersion
     * @value "MAJOR.MINOR.PATCH"
     *
     * Utile pour l'affichage, le logging, ou l'identification runtime.
     *
     * @example
     * @code
     * printf("NKCore version: %s\n", NKENTSEU_VERSION_CORE_STRING);
     * // Output: "NKCore version: 1.0.0"
     * @endcode
     */
    #define NKENTSEU_VERSION_CORE_STRING \
        NKENTSEU_STRINGIFY(NKENTSEU_VERSION_CORE_MAJOR) "." \
        NKENTSEU_STRINGIFY(NKENTSEU_VERSION_CORE_MINOR) "." \
        NKENTSEU_STRINGIFY(NKENTSEU_VERSION_CORE_PATCH)

    /**
     * @brief Nom identifiant du framework
     * @def NKENTSEU_FRAMEWORK_CORE_NAME
     * @ingroup CoreVersion
     * @value "NKCore"
     *
     * Nom constant utilisé pour l'identification dans les logs et messages d'erreur.
     */
    #define NKENTSEU_FRAMEWORK_CORE_NAME "NKCore"

    /**
     * @brief Nom complet avec version intégrée
     * @def NKENTSEU_FRAMEWORK_CORE_FULL_NAME
     * @ingroup CoreVersion
     * @value "NKCore vMAJOR.MINOR.PATCH"
     *
     * Prêt à l'emploi pour l'affichage ou l'identification.
     *
     * @example
     * @code
     * NK_FOUNDATION_LOG_INFO("Initialisation de %s", NKENTSEU_FRAMEWORK_CORE_FULL_NAME);
     * // Output: "Initialisation de NKCore v1.0.0"
     * @endcode
     */
    #define NKENTSEU_FRAMEWORK_CORE_FULL_NAME \
        NKENTSEU_FRAMEWORK_CORE_NAME " v" NKENTSEU_VERSION_CORE_STRING

    // =========================================================================
    // SECTION 3 : INFORMATIONS DE BUILD (MÉTADONNÉES DE COMPILATION)
    // =========================================================================
    // Macros fournissant des informations sur le contexte de compilation.
    // Utiles pour le debugging, le logging, et le support technique.

    /**
     * @defgroup BuildInfo Informations de Build
     * @brief Macros avec métadonnées de compilation
     *
     * Ces informations sont générées automatiquement par le préprocesseur.
     * Elles peuvent être utiles pour :
     *  - Identifier la version exacte d'un binaire en production
     *  - Débugger des problèmes spécifiques à un build
     *  - Générer des rapports de compatibilité
     */

    /**
     * @brief Date de compilation au format "MMM DD YYYY"
     * @def NKENTSEU_BUILD_DATE
     * @ingroup BuildInfo
     * @value Macro prédéfinie __DATE__ du compilateur
     *
     * @note Format dépend du compilateur : généralement "Feb 12 2026"
     * Pour un format ISO 8601 ("YYYY-MM-DD"), utiliser une macro personnalisée.
     *
     * @example
     * @code
     * printf("Compilé le : %s\n", NKENTSEU_BUILD_DATE);
     * @endcode
     */
    #define NKENTSEU_BUILD_DATE __DATE__

    /**
     * @brief Heure de compilation au format "HH:MM:SS"
     * @def NKENTSEU_BUILD_TIME
     * @ingroup BuildInfo
     * @value Macro prédéfinie __TIME__ du compilateur
     *
     * @note Format 24h avec secondes : "14:30:45"
     *
     * @example
     * @code
     * printf("Compilé à : %s\n", NKENTSEU_BUILD_TIME);
     * @endcode
     */
    #define NKENTSEU_BUILD_TIME __TIME__

    /**
     * @brief Timestamp complet de compilation
     * @def NKENTSEU_BUILD_TIMESTAMP
     * @ingroup BuildInfo
     * @value "DATE TIME"
     *
     * Combinaison pratique pour un identifiant unique de build.
     *
     * @example
     * @code
     * // Identifiant unique pour le logging
     * const char* buildId = NKENTSEU_BUILD_TIMESTAMP;
     * @endcode
     */
    #define NKENTSEU_BUILD_TIMESTAMP NKENTSEU_BUILD_DATE " " NKENTSEU_BUILD_TIME

    /**
     * @brief Numéro de build incrémental
     * @def NKENTSEU_BUILD_NUMBER
     * @ingroup BuildInfo
     * @value 0 (par défaut)
     *
     * Peut être surchargé par le système de build (CI/CD) pour un suivi précis.
     *
     * @example Jenkins/GitLab CI
     * @code
     * # Dans le script de build :
     * -DNKENTSEU_BUILD_NUMBER=$BUILD_NUMBER
     * @endcode
     */
    #ifndef NKENTSEU_BUILD_NUMBER
        #define NKENTSEU_BUILD_NUMBER 0
    #endif

    // =========================================================================
    // SECTION 4 : VERSION DE L'API PUBLIQUE
    // =========================================================================
    // Séparation entre version du framework et version de l'API publique.
    // Permet d'évoluer l'implémentation interne sans casser la compatibilité API.

    /**
     * @defgroup APIVersion Version API Publique
     * @brief Macros pour la version de l'interface publique de NKCore
     *
     * L'API version peut différer de la version framework :
     *  - Changements internes (optimisations, refactoring) → PATCH framework uniquement
     *  - Nouvelles fonctions publiques → MINOR API + framework
     *  - Suppression de fonctions publiques → MAJOR API (breaking change)
     *
     * @note Par défaut, API version == framework version.
     * Surcharger uniquement si nécessaire.
     */

    /**
     * @brief Version majeure de l'API publique
     * @def NKENTSEU_API_VERSION_MAJOR
     * @ingroup APIVersion
     * @value 1 (par défaut, aligné sur NKENTSEU_VERSION_CORE_MAJOR)
     *
     * Augmenter lors de changements incompatibles dans l'API publique.
     */
    #ifndef NKENTSEU_API_VERSION_MAJOR
        #define NKENTSEU_API_VERSION_MAJOR 1
    #endif

    /**
     * @brief Version mineure de l'API publique
     * @def NKENTSEU_API_VERSION_MINOR
     * @ingroup APIVersion
     * @value 0 (par défaut, aligné sur NKENTSEU_VERSION_CORE_MINOR)
     *
     * Augmenter lors de l'ajout de fonctionnalités rétrocompatibles.
     */
    #ifndef NKENTSEU_API_VERSION_MINOR
        #define NKENTSEU_API_VERSION_MINOR 0
    #endif

    /**
     * @brief Version patch de l'API publique
     * @def NKENTSEU_API_VERSION_PATCH
     * @ingroup APIVersion
     * @value 0 (par défaut, aligné sur NKENTSEU_VERSION_CORE_PATCH)
     *
     * Augmenter pour les corrections de bugs dans l'API publique.
     */
    #ifndef NKENTSEU_API_VERSION_PATCH
        #define NKENTSEU_API_VERSION_PATCH 0
    #endif

    /**
     * @brief Version API complète encodée sur 32 bits
     * @def NKENTSEU_API_VERSION
     * @ingroup APIVersion
     * @value 0xMMmmpppp (format identique à NKENTSEU_VERSION_CORE)
     *
     * Permet des comparaisons numériques pour la compatibilité API.
     *
     * @example
     * @code
     * // Vérifier que l'API est au moins en version 1.2.0
     * #if NKENTSEU_API_VERSION >= NkVersionEncode(1, 2, 0)
     *     UseNewApiFeature();
     * #else
     *     UseLegacyFallback();
     * #endif
     * @endcode
     */
    #define NKENTSEU_API_VERSION \
        NKENTSEU_VERSION_ENCODE( \
            NKENTSEU_API_VERSION_MAJOR, \
            NKENTSEU_API_VERSION_MINOR, \
            NKENTSEU_API_VERSION_PATCH \
        )

    /**
     * @brief Marqueur d'API stable (production-ready)
     * @def NKENTSEU_API_STABLE
     * @ingroup APIVersion
     *
     * Macro vide par défaut. Peut être étendue pour ajouter des annotations
     * de documentation ou des vérifications statiques.
     *
     * @example
     * @code
     * NKENTSEU_API_STABLE
     * NKENTSEU_CORE_API void StableFunction();
     * @endcode
     */
    #define NKENTSEU_API_STABLE

    /**
     * @brief Marqueur d'API expérimentale (sujette à changements)
     * @def NKENTSEU_API_EXPERIMENTAL
     * @ingroup APIVersion
     *
     * Applique automatiquement l'attribut deprecated avec message explicite.
     * Génère un warning à la compilation pour alerter les développeurs.
     *
     * @note Utilise NKENTSEU_DEPRECATED_MESSAGE de NKPlatform (pas de duplication).
     *
     * @example
     * @code
     * NKENTSEU_API_EXPERIMENTAL
     * NKENTSEU_CORE_API void ExperimentalFeature();
     * // Warning: "This API is experimental and may change"
     * @endcode
     */
    #define NKENTSEU_API_EXPERIMENTAL \
        NKENTSEU_DEPRECATED_MESSAGE("This API is experimental and may change")

    /**
     * @brief Marqueur d'API interne (non publique, usage interne uniquement)
     * @def NKENTSEU_API_INTERNAL
     * @ingroup APIVersion
     *
     * Applique l'attribut de visibilité réduite pour limiter l'usage externe.
     *
     * @note Utilise NKENTSEU_API_LOCAL de NKPlatform pour visibility("hidden").
     * Sur Windows, cela n'a pas d'effet sur l'export DLL mais sert de documentation.
     *
     * @example
     * @code
     * NKENTSEU_API_INTERNAL
     * NKENTSEU_CORE_API void InternalHelper();  // Ne pas appeler depuis l'extérieur
     * @endcode
     */
    #define NKENTSEU_API_INTERNAL NKENTSEU_API_LOCAL

    // =========================================================================
    // SECTION 5 : MACROS DE COMPATIBILITÉ DE VERSION
    // =========================================================================
    // Utilitaires pour vérifier la compatibilité de version à la compilation.
    // Essentiels pour gérer les dépendances et les features conditionnelles.

    /**
     * @defgroup VersionCompatibility Compatibilité de Version
     * @brief Macros pour vérifier la compatibilité de version à la compilation
     *
     * Ces macros permettent d'écrire du code conditionnel basé sur la version
     * de NKCore, facilitant la maintenance de la compatibilité rétroactive.
     */

    /**
     * @brief Vérifier si la version actuelle est au moins égale à une version cible
     * @def NKENTSEU_VERSION_AT_LEAST
     * @param major Version majeure minimale requise
     * @param minor Version mineure minimale requise
     * @param patch Version patch minimale requise
     * @return Expression booléenne évaluée à la compilation (1 ou 0)
     * @ingroup VersionCompatibility
     *
     * Utilise l'encodage 32-bit pour une comparaison numérique efficace.
     *
     * @example
     * @code
     * #if NKENTSEU_VERSION_AT_LEAST(1, 2, 0)
     *     // Fonctionnalité disponible depuis NKCore 1.2.0
     *     UseNewFeature();
     * #else
     *     // Fallback pour les versions antérieures
     *     UseLegacyFeature();
     * #endif
     * @endcode
     */
    #define NKENTSEU_VERSION_AT_LEAST(major, minor, patch) \
        (NKENTSEU_VERSION_CORE >= NKENTSEU_VERSION_ENCODE(major, minor, patch))

    /**
     * @brief Vérifier si la version actuelle correspond exactement à une version cible
     * @def NKENTSEU_VERSION_EQUALS
     * @param major Version majeure exacte attendue
     * @param minor Version mineure exacte attendue
     * @param patch Version patch exacte attendue
     * @return Expression booléenne évaluée à la compilation (1 ou 0)
     * @ingroup VersionCompatibility
     *
     * Utile pour appliquer des workarounds spécifiques à une version.
     *
     * @example
     * @code
     * #if NKENTSEU_VERSION_EQUALS(1, 0, 3)
     *     // Workaround pour un bug connu dans la version 1.0.3
     *     ApplyVersion103Workaround();
     * #endif
     * @endcode
     */
    #define NKENTSEU_VERSION_EQUALS(major, minor, patch) \
        (NKENTSEU_VERSION_CORE == NKENTSEU_VERSION_ENCODE(major, minor, patch))

#endif // NKENTSEU_CORE_NKVERSION_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKVERSION.H
// =============================================================================
// Ce fichier fournit des macros pour gérer le versioning de NKCore de manière
// portable et maintenable, avec support pour les checks conditionnels.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Affichage des informations de version au runtime
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkVersion.h"
    #include "NKPlatform/NkFoundationLog.h"

    void LogVersionInfo() {
        NK_FOUNDATION_LOG_INFO("=== NKCore Version Information ===");
        NK_FOUNDATION_LOG_INFO("Framework : %s", NKENTSEU_FRAMEWORK_CORE_FULL_NAME);
        NK_FOUNDATION_LOG_INFO("API       : %d.%d.%d (encoded: 0x%08X)",
            NKENTSEU_API_VERSION_MAJOR,
            NKENTSEU_API_VERSION_MINOR,
            NKENTSEU_API_VERSION_PATCH,
            NKENTSEU_API_VERSION);
        NK_FOUNDATION_LOG_INFO("Build     : %s (#%d)",
            NKENTSEU_BUILD_TIMESTAMP,
            NKENTSEU_BUILD_NUMBER);
        NK_FOUNDATION_LOG_INFO("=================================");
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Sélection de code basée sur la version du framework
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkVersion.h"

    void InitializeCore() {
        // Fonctionnalité ajoutée en version 1.2.0
        #if NKENTSEU_VERSION_AT_LEAST(1, 2, 0)
            CoreInitializeWithConfig(GetDefaultConfig());
        #else
            // Fallback pour les versions antérieures
            CoreInitializeLegacy();
        #endif

        // Workaround spécifique à une version buggy
        #if NKENTSEU_VERSION_EQUALS(1, 0, 3)
            ApplyKnownBugWorkaround_1_0_3();
        #endif
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Gestion de la compatibilité API dans une bibliothèque cliente
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkVersion.h"

    // Fonction qui utilise l'API NKCore de manière compatible
    void ClientFunction() {
        #if NKENTSEU_API_VERSION_AT_LEAST(2, 0, 0)
            // API v2+ : nouvelle signature
            nkcore::NewStyleFunction(param1, param2);
        #elif NKENTSEU_API_VERSION_AT_LEAST(1, 5, 0)
            // API v1.5+ : signature intermédiaire
            nkcore::TransitionalFunction(param1);
        #else
            // API v1.x ancienne : signature legacy
            nkcore::LegacyFunction();
        #endif
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Déclaration d'API avec marqueurs de stabilité
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkVersion.h"
    #include "NKCore/NkCoreApi.h"  // Pour NKENTSEU_CORE_API

    // API stable : prête pour la production
    NKENTSEU_API_STABLE
    NKENTSEU_CORE_API void StablePublicAPI(int value);

    // API expérimentale : génère un warning d'utilisation
    NKENTSEU_API_EXPERIMENTAL
    NKENTSEU_CORE_API void ExperimentalFeature(const char* config);

    // API interne : visibilité réduite, documentation-only sur Windows
    NKENTSEU_API_INTERNAL
    NKENTSEU_CORE_API void InternalHelperFunction();

    // Utilisation dans le code client :
    // - StablePublicAPI() : aucun warning
    // - ExperimentalFeature() : warning "This API is experimental..."
    // - InternalHelperFunction() : pas d'export sur GCC/Clang, documentation-only sur MSVC
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Surcharger les versions via le système de build (CMake)
// -----------------------------------------------------------------------------
/*
    # CMakeLists.txt pour NKCore

    cmake_minimum_required(VERSION 3.15)
    project(NKCore VERSION 1.2.3)  # Version du projet

    # Propager la version vers les defines de compilation
    target_compile_definitions(nkcore PRIVATE
        NKENTSEU_VERSION_CORE_MAJOR=${PROJECT_VERSION_MAJOR}
        NKENTSEU_VERSION_CORE_MINOR=${PROJECT_VERSION_MINOR}
        NKENTSEU_VERSION_CORE_PATCH=${PROJECT_VERSION_PATCH}
        NKENTSEU_BUILD_NUMBER=$ENV{CI_BUILD_NUMBER}  # Depuis l'environnement CI
    )

    # Pour un client qui veut vérifier la version minimale requise
    target_compile_definitions(monapp PRIVATE
        NKENTSEU_REQUIRE_VERSION_MAJOR=1
        NKENTSEU_REQUIRE_VERSION_MINOR=2
        NKENTSEU_REQUIRE_VERSION_PATCH=0
    )

    # Dans le code du client :
    // #if !NKENTSEU_VERSION_AT_LEAST( \
    //     NKENTSEU_REQUIRE_VERSION_MAJOR, \
    //     NKENTSEU_REQUIRE_VERSION_MINOR, \
    //     NKENTSEU_REQUIRE_VERSION_PATCH)
    // #error "NKCore version 1.2.0 or higher required"
    // #endif
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Génération d'un rapport de compatibilité pour le support technique
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkVersion.h"
    #include <cstdio>

    void GenerateCompatibilityReport() {
        printf("=== NKCore Compatibility Report ===\n");
        printf("Framework Version : %s\n", NKENTSEU_VERSION_CORE_STRING);
        printf("API Version       : %d.%d.%d\n",
            NKENTSEU_API_VERSION_MAJOR,
            NKENTSEU_API_VERSION_MINOR,
            NKENTSEU_API_VERSION_PATCH);
        printf("Build Timestamp   : %s\n", NKENTSEU_BUILD_TIMESTAMP);
        printf("Build Number      : %d\n", NKENTSEU_BUILD_NUMBER);

        // Checks de compatibilité courants
        printf("\nFeature Availability:\n");
        printf("  NewFeatureX : %s\n",
            NKENTSEU_VERSION_AT_LEAST(1, 2, 0) ? "Yes" : "No");
        printf("  ExperimentalAPI : %s\n",
            NKENTSEU_API_VERSION_AT_LEAST(1, 5, 0) ? "Yes" : "No");

        printf("=================================\n");
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Combinaison avec d'autres modules NKCore/NKPlatform
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkPlatformDetect.h"   // NKENTSEU_PLATFORM_*
    #include "NKPlatform/NkPlatformExport.h"   // NKENTSEU_DEPRECATED_MESSAGE
    #include "NKCore/NkVersion.h"              // Version macros
    #include "NKCore/NkCoreApi.h"              // NKENTSEU_CORE_API

    // Fonction qui combine versioning, export, et détection plateforme
    NKENTSEU_CORE_API NKENTSEU_DEPRECATED_MESSAGE("Use NewInit() in NKCore >= 1.3.0")
    void LegacyInitialize() {
        // Logging avec informations de version
        #ifdef NKENTSEU_PLATFORM_WINDOWS
            OutputDebugStringA("LegacyInitialize called (NKCore " NKENTSEU_VERSION_CORE_STRING ")\n");
        #endif

        // Code conditionnel basé sur la version
        #if NKENTSEU_VERSION_AT_LEAST(1, 1, 0)
            UseNewInitializationPath();
        #else
            UseLegacyInitializationPath();
        #endif
    }

    // Nouvelle API stable (sans dépréciation)
    NKENTSEU_API_STABLE
    NKENTSEU_CORE_API void NewInitialize(const Config* cfg) {
        // Implémentation moderne
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================