// =============================================================================
// NKPlatform/NkPlatformExport.h
// Gestion de la visibilité des symboles et de l'API publique.
//
// Design :
//  - Définit NKENTSEU_PLATFORM_API pour l'export/import de symboles.
//  - Gère les spécificités Windows (__declspec), GCC/Clang (visibility).
//  - Supporte les builds partagés, statiques, et header-only.
//  - Détection robuste du compilateur (MSVC, GCC, Clang, MinGW).
//  - Aucune dépendance externe — compatible C/C++.
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_PLATFORM_NKPLATFORMEXPORT_H
#define NKENTSEU_PLATFORM_NKPLATFORMEXPORT_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES DE DÉTECTION (nécessaires pour la logique d'export)
    // -------------------------------------------------------------------------
    // Ces en-têtes fournissent les macros de détection de plateforme et compilateur.
    // Ils sont requis pour déterminer la stratégie d'export appropriée.

    #include "NKPlatform/NkPlatformDetect.h"
    #include "NKPlatform/NkArchDetect.h"
    #include "NKPlatform/NkCompilerDetect.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION DU MODE DE BUILD NKPLATFORM
    // -------------------------------------------------------------------------
    /**
     * @defgroup PlatformBuildConfig Configuration du Build NKPlatform
     * @brief Macros pour contrôler le mode de compilation de NKPlatform
     *
     * Ces macros doivent être définies AVANT l'inclusion de ce fichier :
     *  - NKENTSEU_PLATFORM_BUILD_SHARED_LIB : Compiler NKPlatform en bibliothèque partagée
     *  - NKENTSEU_PLATFORM_STATIC_LIB : Utiliser NKPlatform en mode bibliothèque statique
     *
     * Si aucune n'est définie, le mode par défaut est l'import de DLL (pour les clients).
     *
     * @note Ces macros contrôlent UNIQUEMENT NKPlatform. Les modules dépendants
     * (comme NKCore) ont leurs propres macros de configuration.
     */

    // -------------------------------------------------------------------------
    // SECTION 3 : DÉTECTION DU COMPILATEUR ET ATTRIBUTS D'EXPORT
    // -------------------------------------------------------------------------
    // Détection robuste combinant plateforme ET compilateur pour choisir
    // la bonne stratégie d'export (__declspec vs visibility attribute).

    /**
     * @brief Indique si le compilateur supporte __declspec(dllexport/dllimport)
     * @def NKENTSEU_EXPORT_HAS_DECLSPEC
     * @ingroup PlatformExportInternals
     * @value 1 si supporté, 0 sinon
     *
     * Supporté par MSVC et MinGW sur Windows.
     */
    #if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)) && \
        (defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__))
        #define NKENTSEU_EXPORT_HAS_DECLSPEC 1
    #else
        #define NKENTSEU_EXPORT_HAS_DECLSPEC 0
    #endif

    /**
     * @brief Indique si le compilateur supporte __attribute__((visibility))
     * @def NKENTSEU_EXPORT_HAS_VISIBILITY
     * @ingroup PlatformExportInternals
     * @value 1 si supporté, 0 sinon
     *
     * Supporté par GCC, Clang (y compris Clang sur Windows), et compatibles.
     */
    #if defined(__GNUC__) || defined(__clang__)
        #define NKENTSEU_EXPORT_HAS_VISIBILITY 1
    #else
        #define NKENTSEU_EXPORT_HAS_VISIBILITY 0
    #endif

    // -------------------------------------------------------------------------
    // SECTION 4 : DÉFINITION DES MACROS D'EXPORT/IMPORT DE BASE
    // -------------------------------------------------------------------------
    // Ces macros sont les briques de base. NKENTSEU_PLATFORM_API les combine.

    /**
     * @brief Macro pour exporter un symbole depuis NKPlatform
     * @def NKENTSEU_PLATFORM_API_EXPORT
     * @ingroup PlatformExportInternals
     *
     * À utiliser lors de la compilation de NKPlatform en mode partagé.
     * Ne pas utiliser directement dans le code client.
     */
    #if NKENTSEU_EXPORT_HAS_DECLSPEC
        #define NKENTSEU_PLATFORM_API_EXPORT __declspec(dllexport)
    #elif NKENTSEU_EXPORT_HAS_VISIBILITY
        #define NKENTSEU_PLATFORM_API_EXPORT __attribute__((visibility("default")))
    #else
        #define NKENTSEU_PLATFORM_API_EXPORT
    #endif

    /**
     * @brief Macro pour importer un symbole depuis NKPlatform
     * @def NKENTSEU_PLATFORM_API_IMPORT
     * @ingroup PlatformExportInternals
     *
     * À utiliser lors de l'utilisation de NKPlatform en mode DLL.
     * Ne pas utiliser directement dans le code client.
     */
    #if NKENTSEU_EXPORT_HAS_DECLSPEC
        #define NKENTSEU_PLATFORM_API_IMPORT __declspec(dllimport)
    #elif NKENTSEU_EXPORT_HAS_VISIBILITY
        #define NKENTSEU_PLATFORM_API_IMPORT __attribute__((visibility("default")))
    #else
        #define NKENTSEU_PLATFORM_API_IMPORT
    #endif

    /**
     * @brief Macro pour rendre un symbole privé à NKPlatform
     * @def NKENTSEU_API_LOCAL
     * @ingroup PlatformExport
     *
     * Équivalent à visibility("hidden"). Les symboles ainsi marqués
     * ne sont pas exportés depuis la bibliothèque partagée.
     */
    #if NKENTSEU_EXPORT_HAS_VISIBILITY
        #define NKENTSEU_API_LOCAL __attribute__((visibility("hidden")))
    #else
        #define NKENTSEU_API_LOCAL
    #endif

    // -------------------------------------------------------------------------
    // SECTION 5 : MACRO PRINCIPALE NKENTSEU_PLATFORM_API
    // -------------------------------------------------------------------------
    /**
     * @brief Macro principale pour l'export/import des symboles NKPlatform
     * @def NKENTSEU_PLATFORM_API
     * @ingroup PlatformExport
     *
     * S'adapte automatiquement au contexte de compilation :
     *  - Si NKENTSEU_PLATFORM_BUILD_SHARED_LIB est défini : export (compilation de NKPlatform)
     *  - Si NKENTSEU_PLATFORM_STATIC_LIB est défini : vide (build statique)
     *  - Sinon : import (utilisation de NKPlatform en mode DLL)
     *
     * @example
     * @code
     * // Dans un en-tête public de NKPlatform :
     * NKENTSEU_PLATFORM_API void PlatformInitialize();
     *
     * // Lors de la compilation de NKPlatform en DLL :
     * //   -DNKENTSEU_PLATFORM_BUILD_SHARED_LIB
     * // → La fonction est exportée
     *
     * // Lors de l'utilisation par un client :
     * //   (aucun define nécessaire)
     * // → La fonction est importée
     * @endcode
     */
    #if defined(NKENTSEU_PLATFORM_BUILD_SHARED_LIB)
        // Compilation de NKPlatform en bibliothèque partagée : exporter
        #define NKENTSEU_PLATFORM_API NKENTSEU_PLATFORM_API_EXPORT
    #elif defined(NKENTSEU_PLATFORM_STATIC_LIB)
        // Build statique : pas de décoration nécessaire
        #define NKENTSEU_PLATFORM_API
    #else
        // Utilisation en mode DLL : importer (ou vide si pas de support)
        #define NKENTSEU_PLATFORM_API NKENTSEU_PLATFORM_API_IMPORT
    #endif

    // -------------------------------------------------------------------------
    // SECTION 6 : MACROS DE CONVENANCE (sans duplication)
    // -------------------------------------------------------------------------
    /**
     * @brief Macro pour exporter une classe complète
     * @def NKENTSEU_CLASS_EXPORT
     * @ingroup PlatformExport
     *
     * Alias vers NKENTSEU_PLATFORM_API pour les déclarations de classe.
     */
    #define NKENTSEU_CLASS_EXPORT NKENTSEU_PLATFORM_API

    /**
     * @brief Macro pour la liaison C (extern "C")
     * @def NKENTSEU_EXTERN_C
     * @ingroup PlatformExport
     */
    #ifdef __cplusplus
        #define NKENTSEU_EXTERN_C extern "C"
        #define NKENTSEU_EXTERN_C_BEGIN extern "C" {
        #define NKENTSEU_EXTERN_C_END }
    #else
        #define NKENTSEU_EXTERN_C
        #define NKENTSEU_EXTERN_C_BEGIN
        #define NKENTSEU_EXTERN_C_END
    #endif

    // -------------------------------------------------------------------------
    // SECTION 7 : VALIDATION ET MESSAGES DE DEBUG
    // -------------------------------------------------------------------------
    // Vérifications de cohérence des macros de build.

    #if defined(NKENTSEU_PLATFORM_BUILD_SHARED_LIB) && defined(NKENTSEU_PLATFORM_STATIC_LIB)
        #error "NKPlatform: NKENTSEU_PLATFORM_BUILD_SHARED_LIB et NKENTSEU_PLATFORM_STATIC_LIB sont mutuellement exclusifs"
    #endif

    #ifdef NKENTSEU_EXPORT_DEBUG
        #pragma message("NKPlatform Export Config:")
        #if defined(NKENTSEU_PLATFORM_BUILD_SHARED_LIB)
            #pragma message("  Mode: Shared library (export)")
        #elif defined(NKENTSEU_PLATFORM_STATIC_LIB)
            #pragma message("  Mode: Static library")
        #else
            #pragma message("  Mode: DLL import (client)")
        #endif
        #if NKENTSEU_EXPORT_HAS_DECLSPEC
            #pragma message("  Export method: __declspec")
        #elif NKENTSEU_EXPORT_HAS_VISIBILITY
            #pragma message("  Export method: visibility attribute")
        #else
            #pragma message("  Export method: none (fallback)")
        #endif
    #endif

#endif // NKENTSEU_PLATFORM_NKPLATFORMEXPORT_H

// =============================================================================
// EXEMPLES D'UTILISATION
// =============================================================================
/*
    // Exemple 1 : Compilation de NKPlatform en DLL (CMake)
    // target_compile_definitions(nkplatform PRIVATE NKENTSEU_PLATFORM_BUILD_SHARED_LIB)

    // Exemple 2 : Utilisation statique de NKPlatform
    // target_compile_definitions(monapp PRIVATE NKENTSEU_PLATFORM_STATIC_LIB)

    // Exemple 3 : Utilisation par défaut (DLL import)
    // Aucun define nécessaire, NKENTSEU_PLATFORM_API devient dllimport

    // Exemple 4 : Déclaration dans un header public
    #include "NKPlatform/NkPlatformExport.h"
    NKENTSEU_CLASS_EXPORT class PlatformManager {
    public:
        NKENTSEU_PLATFORM_API void Initialize();
    };
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================