// @File Assertion.h
// @Description Système d'assertions avancé avec gestion des symboles de debug
// @Author TEUGUIA TADJUIDJE Rodolf Séderis
// @Date [AAAA-MM-JJ]
// @License Rihen

#pragma once

#include "PlatformDetection.h"
#include "Nkentseu/Logger/Logger.h"

///////////////////////////////////////////////////////////////////////////////
// Configuration des symboles de debug
///////////////////////////////////////////////////////////////////////////////
#if !defined(NKENTSEU_DEBUG_SYMBOLS)
    #if defined(NKENTSEU_DEBUG) || !defined(NDEBUG)
        #define NKENTSEU_DEBUG_SYMBOLS 1
    #else
        #define NKENTSEU_DEBUG_SYMBOLS 0
    #endif
#endif

///////////////////////////////////////////////////////////////////////////////
// Macros d'assertion avancées
///////////////////////////////////////////////////////////////////////////////
#if NKENTSEU_DEBUG_SYMBOLS
    #define NKENTSEU_ASSERT(condition, message) \
        do { \
            if (!(condition)) { \
                nkentseu::logger.Fatal(message); \
                NKENTSEU_DEBUG_BREAK(); \
            } \
        } while(0)
#else
    #define NKENTSEU_ASSERT(condition, message) \
        do { \
            if (!(condition)) { \
                nkentseu::logger.Fatal(message); \
            } \
        } while(0)
#endif

///////////////////////////////////////////////////////////////////////////////
// Gestion du debug break
///////////////////////////////////////////////////////////////////////////////
#if NKENTSEU_DEBUG_SYMBOLS
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        #define NKENTSEU_DEBUG_BREAK() __debugbreak()
    #elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_MACOS)
        #include <signal.h>
        #define NKENTSEU_DEBUG_BREAK() raise(SIGTRAP)
    #elif defined(NKENTSEU_PLATFORM_ANDROID)
        #include <android/log.h>
        #define NKENTSEU_DEBUG_BREAK() __builtin_trap()
    #else
        #define NKENTSEU_DEBUG_BREAK() ((void)0)
    #endif
#else
    #define NKENTSEU_DEBUG_BREAK() ((void)0)
#endif

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.