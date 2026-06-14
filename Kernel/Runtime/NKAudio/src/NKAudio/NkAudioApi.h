// =============================================================================
// NKAudio/NkAudioApi.h
// Gestion de la visibilité des symboles et de l'API publique du module NKAudio.
//
// Design :
//  - Réutilisation DIRECTE des macros de NKPlatform (ZÉRO duplication)
//  - Macros spécifiques NKAudio uniquement pour la configuration de build NKAudio
//  - Indépendance totale des modes de build : NKAudio et NKPlatform peuvent
//    avoir des configurations différentes (static vs shared)
//  - Compatibilité multiplateforme et multi-compilateur via NKPlatform
//    (Windows / Linux / macOS / iOS / Android / Web / UWP / Xbox)
//  - Aucune redéfinition de deprecated, alignement, pragma, etc.
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_AUDIO_NKAUDIOAPI_H
#define NKENTSEU_AUDIO_NKAUDIOAPI_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // NKAudio dépend de NKPlatform. Nous importons ses macros d'export.
    // AUCUNE duplication : nous utilisons directement les macros NKPlatform
    // qui gèrent déjà toutes les plateformes (Windows DLL, Linux .so, macOS
    // .dylib, Android .so, iOS framework, Emscripten, UWP, Xbox...).

    #include "NKPlatform/NkPlatformExport.h"
    #include "NKPlatform/NkPlatformInline.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION DU MODE DE BUILD NKAUDIO
    // -------------------------------------------------------------------------
    /**
     * @defgroup AudioBuildConfig Configuration du Build NKAudio
     * @brief Macros pour contrôler le mode de compilation de NKAudio
     *
     * Ces macros sont INDÉPENDANTES de celles de NKPlatform :
     *  - NKENTSEU_AUDIO_BUILD_SHARED_LIB : Compiler NKAudio en bibliothèque partagée
     *  - NKENTSEU_AUDIO_STATIC_LIB       : Utiliser NKAudio en mode bibliothèque statique
     *  - NKENTSEU_AUDIO_HEADER_ONLY      : Mode header-only (tout inline)
     *
     * @note NKAudio et NKPlatform peuvent avoir des modes de build différents.
     *       Exemple valide : NKPlatform en DLL + NKAudio en static.
     */

    // -------------------------------------------------------------------------
    // SECTION 3 : MACRO PRINCIPALE NKENTSEU_AUDIO_API
    // -------------------------------------------------------------------------
    /**
     * @brief Macro principale pour l'export/import des symboles NKAudio
     * @def NKENTSEU_AUDIO_API
     * @ingroup AudioApiMacros
     *
     * Cette macro gère UNIQUEMENT la visibilité des symboles de NKAudio.
     * Elle est indépendante de NKENTSEU_PLATFORM_API.
     *
     * Logique :
     *  - NKENTSEU_AUDIO_BUILD_SHARED_LIB : export (compilation NKAudio en DLL/SO)
     *  - NKENTSEU_AUDIO_STATIC_LIB ou NKENTSEU_AUDIO_HEADER_ONLY : vide
     *  - Sinon : import (utilisation NKAudio en mode DLL)
     *
     * @note Multi-plateforme automatique via NKPlatform (Windows __declspec,
     *       Unix __attribute__((visibility("default"))), etc.)
     */
    #if defined(NKENTSEU_AUDIO_BUILD_SHARED_LIB)
        // Compilation de NKAudio en bibliothèque partagée : exporter
        #define NKENTSEU_AUDIO_API NKENTSEU_PLATFORM_API_EXPORT
    #elif defined(NKENTSEU_AUDIO_STATIC_LIB) || defined(NKENTSEU_AUDIO_HEADER_ONLY)
        // Build statique ou header-only : pas de décoration
        #define NKENTSEU_AUDIO_API
    #else
        // Utilisation de NKAudio en mode DLL : importer
        #define NKENTSEU_AUDIO_API NKENTSEU_PLATFORM_API_IMPORT
    #endif

    // -------------------------------------------------------------------------
    // SECTION 4 : MACROS DE CONVENANCE SPÉCIFIQUES À NKAUDIO
    // -------------------------------------------------------------------------

    /**
     * @brief Macro pour exporter une classe complète de NKAudio
     * @def NKENTSEU_AUDIO_CLASS_EXPORT
     * @ingroup AudioApiMacros
     */
    #define NKENTSEU_AUDIO_CLASS_EXPORT NKENTSEU_AUDIO_API

    /**
     * @brief Fonction inline exportée pour NKAudio
     * @def NKENTSEU_AUDIO_API_INLINE
     * @ingroup AudioApiMacros
     */
    #if defined(NKENTSEU_AUDIO_HEADER_ONLY)
        #define NKENTSEU_AUDIO_API_INLINE NKENTSEU_FORCE_INLINE
    #else
        #define NKENTSEU_AUDIO_API_INLINE NKENTSEU_AUDIO_API NKENTSEU_INLINE
    #endif

    /**
     * @brief Fonction force_inline exportée pour NKAudio
     * @def NKENTSEU_AUDIO_API_FORCE_INLINE
     * @ingroup AudioApiMacros
     *
     * Pour les fonctions critiques en performance dans l'API publique
     * (chemins audio temps réel, DSP, mixage).
     */
    #if defined(NKENTSEU_AUDIO_HEADER_ONLY)
        #define NKENTSEU_AUDIO_API_FORCE_INLINE NKENTSEU_FORCE_INLINE
    #else
        #define NKENTSEU_AUDIO_API_FORCE_INLINE NKENTSEU_AUDIO_API NKENTSEU_FORCE_INLINE
    #endif

    /**
     * @brief Fonction no_inline exportée pour NKAudio
     * @def NKENTSEU_AUDIO_API_NO_INLINE
     * @ingroup AudioApiMacros
     */
    #define NKENTSEU_AUDIO_API_NO_INLINE NKENTSEU_AUDIO_API NKENTSEU_NO_INLINE

    /**
     * @brief Macro pour fonction exportée NKAudio avec liaison C (extern "C")
     * @def NKENTSEU_AUDIO_C_API
     * @ingroup AudioApiMacros
     */
    #ifdef __cplusplus
        #define NKENTSEU_AUDIO_C_API extern "C" NKENTSEU_AUDIO_API
    #else
        #define NKENTSEU_AUDIO_C_API NKENTSEU_AUDIO_API
    #endif

    // -------------------------------------------------------------------------
    // SECTION 5 : ALIAS DE COMPATIBILITÉ (ancien NkAudioExport.h)
    // -------------------------------------------------------------------------
    // Alias court pour le code existant qui utilise NK_AUDIO_API.
    #define NK_AUDIO_API NKENTSEU_AUDIO_API

    // -------------------------------------------------------------------------
    // SECTION 6 : VALIDATION DES MACROS DE BUILD NKAUDIO
    // -------------------------------------------------------------------------

    #if defined(NKENTSEU_AUDIO_BUILD_SHARED_LIB) && defined(NKENTSEU_AUDIO_STATIC_LIB)
        #warning "NKAudio: NKENTSEU_AUDIO_BUILD_SHARED_LIB et NKENTSEU_AUDIO_STATIC_LIB définis - NKENTSEU_AUDIO_STATIC_LIB ignoré"
        #undef NKENTSEU_AUDIO_STATIC_LIB
    #endif

    #if defined(NKENTSEU_AUDIO_BUILD_SHARED_LIB) && defined(NKENTSEU_AUDIO_HEADER_ONLY)
        #warning "NKAudio: NKENTSEU_AUDIO_BUILD_SHARED_LIB et NKENTSEU_AUDIO_HEADER_ONLY définis - NKENTSEU_AUDIO_HEADER_ONLY ignoré"
        #undef NKENTSEU_AUDIO_HEADER_ONLY
    #endif

    #if defined(NKENTSEU_AUDIO_STATIC_LIB) && defined(NKENTSEU_AUDIO_HEADER_ONLY)
        #warning "NKAudio: NKENTSEU_AUDIO_STATIC_LIB et NKENTSEU_AUDIO_HEADER_ONLY définis - NKENTSEU_AUDIO_HEADER_ONLY ignoré"
        #undef NKENTSEU_AUDIO_HEADER_ONLY
    #endif

    // -------------------------------------------------------------------------
    // SECTION 7 : MESSAGES DE DEBUG OPTIONNELS
    // -------------------------------------------------------------------------
    #ifdef NKENTSEU_AUDIO_DEBUG
        #pragma message("NKAudio Export Config:")
        #if defined(NKENTSEU_AUDIO_BUILD_SHARED_LIB)
            #pragma message("  NKAudio mode: Shared (export)")
        #elif defined(NKENTSEU_AUDIO_STATIC_LIB)
            #pragma message("  NKAudio mode: Static")
        #elif defined(NKENTSEU_AUDIO_HEADER_ONLY)
            #pragma message("  NKAudio mode: Header-only")
        #else
            #pragma message("  NKAudio mode: DLL import (default)")
        #endif
    #endif

#endif // NKENTSEU_AUDIO_NKAUDIOAPI_H

// =============================================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// =============================================================================
