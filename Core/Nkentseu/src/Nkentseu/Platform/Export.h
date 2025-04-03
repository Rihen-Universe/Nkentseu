// @File Exports.h
// @Description Gestion optimisée des symboles d'exportation multiplateforme
// @Author TEUGUIA TADJUIDJE Rodolf Séderis
// @Date [AAAA-MM-JJ]
// @License Rihen

#pragma once

#include "PlatformDetection.h"

///////////////////////////////////////////////////////////////////////////////
// Configuration hiérarchique des symboles
///////////////////////////////////////////////////////////////////////////////

// 1. Configuration manuelle prioritaire
#if defined(NKENTSEU_EXPORT)
    #define NKENTSEU_API_EXPORT NKENTSEU_EXPORT
    #define NKENTSEU_API_IMPORT NKENTSEU_EXPORT

#elif defined(NKENTSEU_IMPORT)
    #define NKENTSEU_API_EXPORT NKENTSEU_IMPORT
    #define NKENTSEU_API_IMPORT NKENTSEU_IMPORT

// 2. Configuration statique unifiée
#elif defined(NKENTSEU_STATIC)
    #define NKENTSEU_API_EXPORT
    #define NKENTSEU_API_IMPORT

// 3. Configuration dynamique automatique
#else
    // Export/Import Windows
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        #define NKENTSEU_API_EXPORT __declspec(dllexport)
        #define NKENTSEU_API_IMPORT __declspec(dllimport)
        #ifdef _MSC_VER
            #pragma warning(disable : 4251)
        #endif

    // Export/Import WebAssembly
    #elif defined(NKENTSEU_PLATFORM_WEB)
        #define NKENTSEU_API_EXPORT __attribute__((used, visibility("default")))
        #define NKENTSEU_API_IMPORT __attribute__((import_module("nkentseu")))

    // Export/Import Unix-like (Clang/GCC)
    #elif (defined(__clang__) || defined(__GNUC__)) && !defined(NKENTSEU_NO_VISIBILITY)
        #define NKENTSEU_API_EXPORT __attribute__((visibility("default")))
        #define NKENTSEU_API_IMPORT __attribute__((visibility("default")))

    // Fallback pour autres compilateurs
    #else
        #define NKENTSEU_API_EXPORT
        #define NKENTSEU_API_IMPORT
    #endif
#endif

///////////////////////////////////////////////////////////////////////////////
// Sélection automatique de l'API
///////////////////////////////////////////////////////////////////////////////
#if !defined(NKENTSEU_API)
    #if defined(NKENTSEU_BUILD_CORE) || defined(NKENTSEU_SHARED_LIB)
        #define NKENTSEU_API NKENTSEU_API_EXPORT
    #else
        #define NKENTSEU_API NKENTSEU_API_IMPORT
    #endif
#endif

///////////////////////////////////////////////////////////////////////////////
// Gestion unifiée des fonctions inline
///////////////////////////////////////////////////////////////////////////////
#if !defined(NKENTSEU_INLINE_API)
    #if defined(NKENTSEU_SHARED_LIB) && defined(NKENTSEU_API_EXPORT)
        #define NKENTSEU_INLINE_API NKENTSEU_API __attribute__((visibility_inline))
    #else
        #define NKENTSEU_INLINE_API inline
    #endif
#endif

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.