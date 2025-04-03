// @File PlatformDetection.h
// @Description Détection avancée de la plateforme d'exécution et des composants système
// @Author [Votre Nom]
// @Date [AAAA-MM-JJ]
// @License Rihen

#pragma once

///////////////////////////////////////////////////////////////////////////////
// Version de NKENTSEU
///////////////////////////////////////////////////////////////////////////////
#define NKENTSEU_VERSION_MAJOR 0
#define NKENTSEU_VERSION_MINOR 0
#define NKENTSEU_VERSION_PATCH 1
#define NKENTSEU_VERSION_STRING "0.0.1"

///////////////////////////////////////////////////////////////////////////////
// Détection de l'architecture
///////////////////////////////////////////////////////////////////////////////
#if defined(_M_X64) || defined(__x86_64__)
    #define NKENTSEU_ARCH_X64
#elif defined(_M_IX86) || defined(__i386__)
    #define NKENTSEU_ARCH_X86
#elif defined(__arm__) || defined(_M_ARM)
    #define NKENTSEU_ARCH_ARM
#elif defined(__aarch64__)
    #define NKENTSEU_ARCH_ARM64
#elif defined(__mips__)
    #define NKENTSEU_ARCH_MIPS
#elif defined(__powerpc__)
    #define NKENTSEU_ARCH_PPC
#endif

///////////////////////////////////////////////////////////////////////////////
// Détection des plateformes principales
///////////////////////////////////////////////////////////////////////////////
// Nintendo DS doit être vérifié en premier (environnement spécialisé)
#if defined(__NDS__) || defined(__DS__)
    #define NKENTSEU_PLATFORM_NINTENDO_DS

// Plateformes Apple
#elif defined(__APPLE__) && defined(__MACH__)
    #define NKENTSEU_PLATFORM_APPLE
    #include <TargetConditionals.h>
    #if TARGET_OS_SIMULATOR
        #define NKENTSEU_PLATFORM_APPLE_SIMULATOR
    #endif
    
    #if TARGET_OS_IOS
        #define NKENTSEU_PLATFORM_IOS
    #elif TARGET_OS_WATCH
        #define NKENTSEU_PLATFORM_WATCHOS
    #elif TARGET_OS_TV
        #define NKENTSEU_PLATFORM_TVOS
    #elif TARGET_OS_MAC
        #define NKENTSEU_PLATFORM_MACOS
    #endif

// Plateformes Microsoft
#elif defined(_WIN32)
    #define NKENTSEU_PLATFORM_WINDOWS
    #ifdef _WIN64
        #define NKENTSEU_PLATFORM_WIN64
    #else
        #define NKENTSEU_PLATFORM_WIN32
    #endif

// Android (doit être avant Linux)
#elif defined(__ANDROID__)
    #define NKENTSEU_PLATFORM_ANDROID

// Systèmes Unix-like
#elif defined(__linux__)
    #define NKENTSEU_PLATFORM_LINUX
    
    // Détection WSL
    #if __has_include("/proc/version")
        #include <cstring>
        
        #define NKENTSEU_PLATFORM_WSL nkentseu::platform::IsWSL()
        namespace nkentseu {
            namespace platform {
                bool IsWSL() noexcept;
            }
        }
    #endif

// BSD Family
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
    #define NKENTSEU_PLATFORM_FREEBSD
    #define NKENTSEU_PLATFORM_BSD
#elif defined(__OpenBSD__)
    #define NKENTSEU_PLATFORM_OPENBSD
    #define NKENTSEU_PLATFORM_BSD
#elif defined(__NetBSD__)
    #define NKENTSEU_PLATFORM_NETBSD
    #define NKENTSEU_PLATFORM_BSD
#elif defined(__DragonFly__)
    #define NKENTSEU_PLATFORM_DRAGONFLYBSD
    #define NKENTSEU_PLATFORM_BSD

// Unix générique
#elif defined(__unix__)
    #define NKENTSEU_PLATFORM_UNIX
#endif

///////////////////////////////////////////////////////////////////////////////
// Détection du système graphique (Unix-like)
///////////////////////////////////////////////////////////////////////////////
#if defined(NKENTSEU_PLATFORM_UNIX) || defined(NKENTSEU_PLATFORM_LINUX)
    #if defined(__WAYLAND__)
        #define NKENTSEU_DISPLAY_WAYLAND
        #if defined(_X11_SOURCE)
            #define NKENTSEU_DISPLAY_X11_FALLBACK
        #endif
    #elif defined(__XCB__)
        #define NKENTSEU_DISPLAY_XCB
        #if defined(_X11_SOURCE)
            #define NKENTSEU_DISPLAY_XLIB_COMPAT
        #endif
    #elif defined(_X11_SOURCE)
        #define NKENTSEU_DISPLAY_X11
    #elif defined(__DIRECTFB__)
        #define NKENTSEU_DISPLAY_DIRECTFB
    #else
        #warning "Aucun système d'affichage détecté!"
    #endif
#elif defined(NKENTSEU_PLATFORM_WINDOWS)
    #define NKENTSEU_DISPLAY_WIN32
#endif

///////////////////////////////////////////////////////////////////////////////
// Détection de l'environnement d'exécution
///////////////////////////////////////////////////////////////////////////////
#if defined(__ANDROID__)
    #define NKENTSEU_ENV_ANDROID_NDK
    #define NKENTSEU_ENV_MOBILE
#elif defined(__CHEERP__)
    #define NKENTSEU_ENV_CHEERP
    #define NKENTSEU_ENV_WEBASM
#elif defined(__EMSCRIPTEN__)
    #define NKENTSEU_ENV_EMSCRIPTEN
    #define NKENTSEU_ENV_WEB
    #define NKENTSEU_PLATFORM_WEB
#elif defined(__NX__)
    #define NKENTSEU_ENV_NINTENDO_SWITCH
    #define NKENTSEU_ENV_CONSOLE
#endif

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.