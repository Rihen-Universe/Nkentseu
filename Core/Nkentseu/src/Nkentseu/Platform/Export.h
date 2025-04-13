/**
* @File Exports.h
* @Description Gestion optimisée des symboles d'exportation multiplateforme
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2025-01-05
* @License Rihen
*/

#pragma once

#include "PlatformDetection.h"

///////////////////////////////////////////////////////////////////////////////
// Configuration hiérarchique des symboles
///////////////////////////////////////////////////////////////////////////////

// 1. Configuration statique prioritaire
#if defined(NKENTSEU_STATIC)
    #define NKENTSEU_API_EXPORT
    #define NKENTSEU_API_IMPORT
    #define NKENTSEU_API_HIDDEN

// 2. Configuration dynamique automatique
#else
    // Export/Import Windows
    #if defined(NKENTSEU_PLATFORM_WINDOWS)

        #define NKENTSEU_API_EXPORT __declspec(dllexport)
        #define NKENTSEU_API_IMPORT __declspec(dllimport)
        #define NKENTSEU_API_HIDDEN

        #ifdef _MSC_VER
            #pragma warning(disable : 4251)
        #endif

    // Export/Import Unix-like (Clang/GCC)
    #elif (defined(__clang__) || defined(__GNUC__))

        #define NKENTSEU_API_EXPORT __attribute__((visibility("default")))
        #define NKENTSEU_API_IMPORT __attribute__((visibility("default")))
        #define NKENTSEU_API_HIDDEN __attribute__((visibility("hidden")))

    // Fallback pour autres plateformes
    #else

        #define NKENTSEU_API_EXPORT
        #define NKENTSEU_API_IMPORT
        #define NKENTSEU_API_HIDDEN
        
    #endif
#endif

///////////////////////////////////////////////////////////////////////////////
// Sélection automatique de l'API
///////////////////////////////////////////////////////////////////////////////
#if !defined(NKENTSEU_API)
    #if defined(NKENTSEU_EXPORT)
        #define NKENTSEU_API NKENTSEU_API_EXPORT
    #else
        #define NKENTSEU_API NKENTSEU_API_IMPORT
    #endif
#endif

// Gestion explicite de la visibilité des templates
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    #define NKENTSEU_TEMPLATE_API
#else
    #define NKENTSEU_TEMPLATE_API NKENTSEU_API_HIDDEN
#endif

///////////////////////////////////////////////////////////////////////////////
// Gestion unifiée des fonctions inline
///////////////////////////////////////////////////////////////////////////////
#if defined(__GNUC__) || defined(__clang__)
    #define NKENTSEU_INLINE_API __attribute__((visibility("hidden"))) inline
#else
    #define NKENTSEU_INLINE_API inline
#endif

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.