// =============================================================================
// NKGui/NkGuiApi.h
// Visibilité des symboles et API publique du module NKGui.
//
// Design (calqué sur NKCore/NkCoreApi.h) :
//  - Réutilisation DIRECTE des macros de NKPlatform (ZÉRO duplication).
//  - Macros spécifiques NKGui seulement pour la configuration de build NKGui.
//  - Modes indépendants : NKENTSEU_NKGUI_BUILD_SHARED_LIB / _STATIC_LIB /
//    _USE_SHARED_LIB / _HEADER_ONLY. Défaut = statique (aucune décoration).
//
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_NKGUI_NKGUIAPI_H
#define NKENTSEU_NKGUI_NKGUIAPI_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DÉPENDANCES — macros NKPlatform (réutilisées, pas dupliquées)
    // -------------------------------------------------------------------------
    #include "NKPlatform/NkPlatformExport.h"
    #include "NKPlatform/NkPlatformInline.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : MACRO PRINCIPALE NKENTSEU_NKGUI_API
    // -------------------------------------------------------------------------
    //  - NKENTSEU_NKGUI_BUILD_SHARED_LIB : export (compilation de NKGui en DLL)
    //  - NKENTSEU_NKGUI_STATIC_LIB / _HEADER_ONLY : vide (aucune décoration)
    //  - NKENTSEU_NKGUI_USE_SHARED_LIB : import (consommation de NKGui en DLL)
    //  - Sinon (défaut) : statique / monolithique -> vide
    #if defined(NKENTSEU_NKGUI_BUILD_SHARED_LIB)
        #define NKENTSEU_NKGUI_API NKENTSEU_PLATFORM_API_EXPORT
    #elif defined(NKENTSEU_NKGUI_STATIC_LIB) || defined(NKENTSEU_NKGUI_HEADER_ONLY)
        #define NKENTSEU_NKGUI_API
    #elif defined(NKENTSEU_NKGUI_USE_SHARED_LIB)
        #define NKENTSEU_NKGUI_API NKENTSEU_PLATFORM_API_IMPORT
    #else
        #define NKENTSEU_NKGUI_API
    #endif

    // -------------------------------------------------------------------------
    // SECTION 3 : MACROS DE CONVENANCE SPÉCIFIQUES À NKGUI
    // -------------------------------------------------------------------------
    // Export d'une classe complète.
    #define NKENTSEU_NKGUI_CLASS_EXPORT NKENTSEU_NKGUI_API

    // Fonction inline exportée.
    #if defined(NKENTSEU_NKGUI_HEADER_ONLY)
        #define NKENTSEU_NKGUI_API_INLINE NKENTSEU_FORCE_INLINE
    #else
        #define NKENTSEU_NKGUI_API_INLINE NKENTSEU_INLINE
    #endif

    // Fonction force_inline exportée.
    #if defined(NKENTSEU_NKGUI_HEADER_ONLY)
        #define NKENTSEU_NKGUI_API_FORCE_INLINE NKENTSEU_FORCE_INLINE
    #else
        #define NKENTSEU_NKGUI_API_FORCE_INLINE NKENTSEU_NKGUI_API NKENTSEU_FORCE_INLINE
    #endif

    // Fonction no_inline exportée.
    #define NKENTSEU_NKGUI_API_NO_INLINE NKENTSEU_NKGUI_API NKENTSEU_NO_INLINE

    // Fonction exportée à liaison C.
    #ifdef __cplusplus
        #define NKENTSEU_NKGUI_C_API extern "C" NKENTSEU_NKGUI_API
    #else
        #define NKENTSEU_NKGUI_C_API NKENTSEU_NKGUI_API
    #endif

    // -------------------------------------------------------------------------
    // SECTION 4 : VALIDATION DES MODES DE BUILD
    // -------------------------------------------------------------------------
    #if defined(NKENTSEU_NKGUI_BUILD_SHARED_LIB) && defined(NKENTSEU_NKGUI_STATIC_LIB)
        #warning "NKGui: NKENTSEU_NKGUI_BUILD_SHARED_LIB et NKENTSEU_NKGUI_STATIC_LIB définis - STATIC_LIB ignoré"
        #undef NKENTSEU_NKGUI_STATIC_LIB
    #endif

#endif // NKENTSEU_NKGUI_NKGUIAPI_H
