// =============================================================================
// NKPlatform/NkPlatformConfig.h
// Configuration de plateforme et détection de fonctionnalités.
//
// Design :
//  - Centralisation des macros de configuration spécifiques à chaque plateforme
//  - Structures PlatformConfig et PlatformCapabilities pour accès runtime
//  - Détection de fonctionnalités : Unicode, threading, filesystem, réseau
//  - Macros d'alignement cache/SIMD basées sur NkArchDetect.h
//  - Aucune dépendance STL, compatible header-only pour les parties inline
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_PLATFORM_NKPLATFORMCONFIG_H
#define NKENTSEU_PLATFORM_NKPLATFORMCONFIG_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules de détection de plateforme, architecture et compilateur.
    // Ces modules fournissent les macros NKENTSEU_PLATFORM_*, NKENTSEU_ARCH_*, etc.
    // NkPlatformExport.h fournit NKENTSEU_PLATFORM_API pour l'export de symboles.
    // NkPlatformInline.h fournit NKENTSEU_API_INLINE pour les fonctions inline exportées.

    #include "NKPlatform/NkPlatformDetect.h"
    #include "NKPlatform/NkArchDetect.h"
    #include "NKPlatform/NkCompilerDetect.h"
    #include "NKPlatform/NkPlatformExport.h"
    #include "NKPlatform/NkPlatformInline.h"

    #include <stddef.h>

    // =========================================================================
    // SECTION 2 : ESPACE DE NOMS PRINCIPAL (INDENTATION HIÉRARCHIQUE)
    // =========================================================================
    // Déclaration de l'espace de noms principal nkentseu::platform.
    // Les symboles publics sont exportés via NKENTSEU_PLATFORM_API.
    //
    // NOTE SUR L'INDENTATION :
    //  - Le contenu de 'nkentseu' est indenté d'un niveau
    //  - Le contenu de 'platform' (dans 'nkentseu') est indenté de deux niveaux

    namespace nkentseu {

        namespace platform {

            // =================================================================
            // SECTION 3 : STRUCTURE PLATFORMCONFIG (INFORMATION STATIQUE)
            // =================================================================
            // Contient les informations de configuration déterminées à la compilation.
            // Ces valeurs ne changent pas pendant l'exécution du programme.

            /**
             * @brief Structure de configuration de plateforme (compile-time)
             * @struct PlatformConfig
             * @ingroup PlatformConfig
             *
             * Regroupe toutes les informations de configuration déterminées
             * à la compilation : nom de plateforme, architecture, compilateur,
             * flags de build, et fonctionnalités disponibles.
             *
             * @note Cette structure est en lecture seule après initialisation.
             * Utiliser GetPlatformConfig() pour obtenir l'instance singleton.
             *
             * @example
             * @code
             * const auto& config = nkentseu::platform::GetPlatformConfig();
             * if (config.isDebugBuild) {
             *     EnableDebugLogging();
             * }
             * if (config.hasFilesystem) {
             *     UseStdFilesystem();
             * }
             * @endcode
             */
            struct NKENTSEU_CLASS_EXPORT PlatformConfig {
                // -----------------------------------------------------------------
                // Informations d'identification de la plateforme
                // -----------------------------------------------------------------

                /**
                 * @brief Nom lisible de la plateforme cible
                 * @value Exemples : "Windows", "Linux", "macOS", "Android", "Web"
                 */
                const char* platformName;

                /**
                 * @brief Nom lisible de l'architecture CPU
                 * @value Exemples : "x64", "x86", "ARM64", "ARM", "WebAssembly"
                 */
                const char* archName;

                /**
                 * @brief Nom du compilateur utilisé
                 * @value Exemples : "MSVC", "Clang", "GCC"
                 */
                const char* compilerName;

                /**
                 * @brief Version numérique du compilateur
                 * @note Format dépend du compilateur (voir NkCompilerDetect.h)
                 */
                int compilerVersion;

                // -----------------------------------------------------------------
                // Configuration du build
                // -----------------------------------------------------------------

                /**
                 * @brief Indique si le build est en mode debug
                 * @note Défini via _DEBUG, DEBUG, ou absence de NDEBUG
                 */
                bool isDebugBuild;

                /**
                 * @brief Indique si le build est en mode release
                 * @note Défini via NDEBUG
                 */
                bool isReleaseBuild;

                /**
                 * @brief Indique si l'architecture cible est 64-bit
                 * @note Basé sur NKENTSEU_ARCH_64BIT
                 */
                bool is64Bit;

                /**
                 * @brief Indique si le système utilise le little-endian
                 * @note Basé sur NKENTSEU_ARCH_LITTLE_ENDIAN
                 */
                bool isLittleEndian;

                // -----------------------------------------------------------------
                // Flags de fonctionnalités disponibles
                // -----------------------------------------------------------------

                /**
                 * @brief Support de l'Unicode (wchar_t, APIs UTF-16/UTF-8)
                 * @note Détecté via __STDC_ISO_10646__ ou plateforme Windows
                 */
                bool hasUnicode;

                /**
                 * @brief Support du multithreading (C++11+, pthreads, Windows threads)
                 * @note Détecté via _REENTRANT, _MT, ou __cplusplus >= 201103L
                 */
                bool hasThreading;

                /**
                 * @brief Support de la bibliothèque <filesystem> (C++17)
                 * @note Nécessite __cplusplus >= 201703L et <filesystem> disponible
                 */
                bool hasFilesystem;

                /**
                 * @brief Support des APIs réseau (sockets, etc.)
                 * @note Disponible sur Windows, Linux, macOS ; limité sur mobile/Web
                 */
                bool hasNetwork;

                // -----------------------------------------------------------------
                // Limites et constantes système
                // -----------------------------------------------------------------

                /**
                 * @brief Longueur maximale d'un chemin de fichier
                 * @value Windows: 260, Linux/Android: 4096, macOS/iOS: 1024
                 */
                int maxPathLength;

                /**
                 * @brief Taille d'une ligne de cache CPU en octets
                 * @note Typiquement 64 pour les CPU modernes (via NkArchDetect.h)
                 */
                int cacheLineSize;

                /**
                 * @brief Constructeur par défaut (initialisation via macros)
                 */
                PlatformConfig();
            };

            /**
             * @brief Obtenir l'instance singleton de PlatformConfig
             * @return Référence constante vers la configuration de plateforme
             * @ingroup PlatformConfigAPI
             *
             * @note Thread-safe en C++11+ grâce à l'initialisation statique locale.
             * La configuration est déterminée une seule fois au premier appel.
             */
            NKENTSEU_API_NO_INLINE const PlatformConfig& GetPlatformConfig();

            // =================================================================
            // SECTION 4 : STRUCTURE PLATFORMCAPABILITIES (DÉTECTION RUNTIME)
            // =================================================================
            // Contient les informations détectées à l'exécution sur le hardware.
            // Ces valeurs peuvent varier selon la machine d'exécution.

            /**
             * @brief Structure des capacités matérielles de la plateforme (runtime)
             * @struct PlatformCapabilities
             * @ingroup PlatformConfig
             *
             * Regroupe les informations détectées à l'exécution sur le hardware :
             * mémoire, CPU, affichage, et extensions SIMD disponibles.
             *
             * @note Ces valeurs sont détectées une fois au premier appel de
             * GetPlatformCapabilities(), puis mises en cache.
             *
             * @example
             * @code
             * const auto& caps = nkentseu::platform::GetPlatformCapabilities();
             * if (caps.hasAVX2) {
             *     UseAVX2Optimizations();
             * }
             * if (caps.totalPhysicalMemory > 8ULL * 1024 * 1024 * 1024) {
             *     EnableLargeCache();
             * }
             * @endcode
             */
            struct NKENTSEU_CLASS_EXPORT PlatformCapabilities {
                // -----------------------------------------------------------------
                // Informations mémoire
                // -----------------------------------------------------------------

                /**
                 * @brief Mémoire physique totale en octets
                 * @note 0 si la détection échoue
                 */
                size_t totalPhysicalMemory;

                /**
                 * @brief Mémoire physique disponible en octets
                 * @note Estimation ; peut être 0 sur certaines plateformes
                 */
                size_t availablePhysicalMemory;

                /**
                 * @brief Taille d'une page mémoire système en octets
                 * @value Typiquement 4096 (4KB) sur la plupart des systèmes
                 */
                size_t pageSize;

                // -----------------------------------------------------------------
                // Informations CPU
                // -----------------------------------------------------------------

                /**
                 * @brief Nombre de cœurs physiques détectés
                 * @note Peut être approximatif sur certaines plateformes
                 */
                int processorCount;

                /**
                 * @brief Nombre de cœurs logiques (avec HT/SMT)
                 * @note Égal à processorCount si Hyper-Threading désactivé
                 */
                int logicalProcessorCount;

                // -----------------------------------------------------------------
                // Informations d'affichage
                // -----------------------------------------------------------------

                /**
                 * @brief Indique si un affichage graphique est disponible
                 * @note false pour les serveurs headless, true pour desktop/mobile
                 */
                bool hasDisplay;

                /**
                 * @brief Largeur de l'écran principal en pixels
                 * @note 0 si hasDisplay est false ou détection échouée
                 */
                int primaryScreenWidth;

                /**
                 * @brief Hauteur de l'écran principal en pixels
                 * @note 0 si hasDisplay est false ou détection échouée
                 */
                int primaryScreenHeight;

                // -----------------------------------------------------------------
                // Fonctionnalités CPU SIMD (détection compile-time)
                // -----------------------------------------------------------------

                /** @brief Support de SSE (x86/x64) */
                bool hasSSE;

                /** @brief Support de SSE2 (x86/x64) */
                bool hasSSE2;

                /** @brief Support de AVX (x86/x64) */
                bool hasAVX;

                /** @brief Support de AVX2 (x86/x64) */
                bool hasAVX2;

                /** @brief Support de NEON (ARM) */
                bool hasNEON;

                /**
                 * @brief Constructeur par défaut (détection via APIs système)
                 */
                PlatformCapabilities();
            };

            /**
             * @brief Obtenir l'instance singleton de PlatformCapabilities
             * @return Référence constante vers les capacités détectées
             * @ingroup PlatformConfigAPI
             *
             * @note Thread-safe en C++11+ grâce à l'initialisation statique locale.
             * La détection hardware est effectuée une seule fois au premier appel.
             */
            NKENTSEU_API_NO_INLINE const PlatformCapabilities& GetPlatformCapabilities();

        } // namespace platform

    } // namespace nkentseu

    // =========================================================================
    // SECTION 5 : MACROS DE CONFIGURATION SPÉCIFIQUES À LA PLATEFORME
    // =========================================================================
    // Définition des constantes spécifiques à chaque plateforme cible.
    // Ces macros sont évaluées à la compilation et n'ont aucun coût runtime.

    // -------------------------------------------------------------------------
    // Sous-section : Configuration Windows
    // -------------------------------------------------------------------------

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        /**
         * @brief Séparateur de chemin pour Windows (caractère)
         * @def NKENTSEU_PATH_SEPARATOR
         * @value '\\'
         * @ingroup PlatformMacros
         */
        #define NKENTSEU_PATH_SEPARATOR '\\'

        /**
         * @brief Séparateur de chemin pour Windows (chaîne)
         * @def NKENTSEU_PATH_SEPARATOR_STR
         * @value "\\"
         * @ingroup PlatformMacros
         */
        #define NKENTSEU_PATH_SEPARATOR_STR "\\"

        /**
         * @brief Séquence de fin de ligne pour Windows
         * @def NKENTSEU_LINE_ENDING
         * @value "\r\n"
         * @ingroup PlatformMacros
         */
        #define NKENTSEU_LINE_ENDING "\r\n"

        /**
         * @brief Longueur maximale de chemin pour Windows (MAX_PATH)
         * @def NKENTSEU_MAX_PATH
         * @value 260
         * @ingroup PlatformMacros
         * @note Pour les chemins > 260, utiliser le préfixe "\\?\" sur Windows
         */
        #define NKENTSEU_MAX_PATH 260

        /** @brief Extension des bibliothèques dynamiques Windows */
        #define NKENTSEU_DYNAMIC_LIB_EXT ".dll"

        /** @brief Extension des bibliothèques statiques Windows */
        #define NKENTSEU_STATIC_LIB_EXT ".lib"

        /** @brief Extension des exécutables Windows */
        #define NKENTSEU_EXECUTABLE_EXT ".exe"

    // -------------------------------------------------------------------------
    // Sous-section : Configuration Linux
    // -------------------------------------------------------------------------

    #elif defined(NKENTSEU_PLATFORM_LINUX)
        #define NKENTSEU_PATH_SEPARATOR '/'
        #define NKENTSEU_PATH_SEPARATOR_STR "/"
        #define NKENTSEU_LINE_ENDING "\n"
        #define NKENTSEU_MAX_PATH 4096
        #define NKENTSEU_DYNAMIC_LIB_EXT ".so"
        #define NKENTSEU_STATIC_LIB_EXT ".a"
        #define NKENTSEU_EXECUTABLE_EXT ""

    // -------------------------------------------------------------------------
    // Sous-section : Configuration macOS
    // -------------------------------------------------------------------------

    #elif defined(NKENTSEU_PLATFORM_MACOS)
        #define NKENTSEU_PATH_SEPARATOR '/'
        #define NKENTSEU_PATH_SEPARATOR_STR "/"
        #define NKENTSEU_LINE_ENDING "\n"
        #define NKENTSEU_MAX_PATH 1024
        #define NKENTSEU_DYNAMIC_LIB_EXT ".dylib"
        #define NKENTSEU_STATIC_LIB_EXT ".a"
        #define NKENTSEU_EXECUTABLE_EXT ""

    // -------------------------------------------------------------------------
    // Sous-section : Configuration Android
    // -------------------------------------------------------------------------

    #elif defined(NKENTSEU_PLATFORM_ANDROID)
        #define NKENTSEU_PATH_SEPARATOR '/'
        #define NKENTSEU_PATH_SEPARATOR_STR "/"
        #define NKENTSEU_LINE_ENDING "\n"
        #define NKENTSEU_MAX_PATH 4096
        #define NKENTSEU_DYNAMIC_LIB_EXT ".so"
        #define NKENTSEU_STATIC_LIB_EXT ".a"
        #define NKENTSEU_EXECUTABLE_EXT ""

    // -------------------------------------------------------------------------
    // Sous-section : Configuration iOS
    // -------------------------------------------------------------------------

    #elif defined(NKENTSEU_PLATFORM_IOS)
        #define NKENTSEU_PATH_SEPARATOR '/'
        #define NKENTSEU_PATH_SEPARATOR_STR "/"
        #define NKENTSEU_LINE_ENDING "\n"
        #define NKENTSEU_MAX_PATH 1024
        #define NKENTSEU_DYNAMIC_LIB_EXT ".dylib"
        #define NKENTSEU_STATIC_LIB_EXT ".a"
        #define NKENTSEU_EXECUTABLE_EXT ""

    // -------------------------------------------------------------------------
    // Sous-section : Configuration Web (Emscripten)
    // -------------------------------------------------------------------------

    #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        #define NKENTSEU_PATH_SEPARATOR '/'
        #define NKENTSEU_PATH_SEPARATOR_STR "/"
        #define NKENTSEU_LINE_ENDING "\n"
        #define NKENTSEU_MAX_PATH 4096
        #define NKENTSEU_DYNAMIC_LIB_EXT ".wasm"
        #define NKENTSEU_STATIC_LIB_EXT ".a"
        #define NKENTSEU_EXECUTABLE_EXT ".html"

    #endif

    // -------------------------------------------------------------------------
    // Sous-section : Fallback pour NKENTSEU_MAX_PATH
    // -------------------------------------------------------------------------
    // Si aucune plateforme connue n'est détectée, utiliser une valeur conservative.

    #ifndef NKENTSEU_MAX_PATH
        /**
         * @brief Valeur par défaut pour NKENTSEU_MAX_PATH
         * @def NKENTSEU_MAX_PATH
         * @value 4096
         * @ingroup PlatformMacros
         */
        #define NKENTSEU_MAX_PATH 4096
    #endif

    // =========================================================================
    // SECTION 6 : MACROS DE DÉTECTION DE FONCTIONNALITÉS
    // =========================================================================
    // Macros booléennes indiquant la disponibilité de fonctionnalités
    // spécifiques sur la plateforme cible.

    // -------------------------------------------------------------------------
    // Sous-section : Support Unicode
    // -------------------------------------------------------------------------

    /**
     * @brief Indicateur de support Unicode
     * @def NKENTSEU_HAS_UNICODE
     * @value 1 si supporté, 0 sinon
     * @ingroup FeatureMacros
     *
     * Détecté via __STDC_ISO_10646__ (wchar_t en Unicode) ou plateforme Windows.
     */
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        #define NKENTSEU_HAS_UNICODE 1
    #elif defined(__STDC_ISO_10646__)
        #define NKENTSEU_HAS_UNICODE 1
    #else
        #define NKENTSEU_HAS_UNICODE 0
    #endif

    // -------------------------------------------------------------------------
    // Sous-section : Support Threading
    // -------------------------------------------------------------------------

    /**
     * @brief Indicateur de support du multithreading
     * @def NKENTSEU_HAS_THREADING
     * @value 1 si supporté, 0 sinon
     * @ingroup FeatureMacros
     *
     * Détecté via _REENTRANT (POSIX), _MT (MSVC), ou C++11+.
     */
    #if defined(_REENTRANT) || defined(_MT) || (__cplusplus >= 201103L)
        #define NKENTSEU_HAS_THREADING 1
    #else
        #define NKENTSEU_HAS_THREADING 0
    #endif

    // -------------------------------------------------------------------------
    // Sous-section : Support Filesystem (C++17)
    // -------------------------------------------------------------------------

    /**
     * @brief Indicateur de support de <filesystem>
     * @def NKENTSEU_HAS_FILESYSTEM
     * @value 1 si supporté, 0 sinon
     * @ingroup FeatureMacros
     *
     * Nécessite C++17 (__cplusplus >= 201703L) et l'en-tête <filesystem> disponible.
     * Certains compilateurs peuvent nécessiter des flags supplémentaires (-lstdc++fs).
     */
    #if (__cplusplus >= 201703L) && __has_include(<filesystem>)
        #define NKENTSEU_HAS_FILESYSTEM 1
    #else
        #define NKENTSEU_HAS_FILESYSTEM 0
    #endif

    // -------------------------------------------------------------------------
    // Sous-section : Support Réseau
    // -------------------------------------------------------------------------

    /**
     * @brief Indicateur de support des APIs réseau
     * @def NKENTSEU_HAS_NETWORK
     * @value 1 si supporté, 0 sinon
     * @ingroup FeatureMacros
     *
     * Disponible sur les plateformes desktop/server ; limité ou absent
     * sur certaines plateformes embarquées ou Web.
     */
    #if defined(NKENTSEU_PLATFORM_WINDOWS) || \
        defined(NKENTSEU_PLATFORM_LINUX) || \
        defined(NKENTSEU_PLATFORM_MACOS)
        #define NKENTSEU_HAS_NETWORK 1
    #else
        #define NKENTSEU_HAS_NETWORK 0
    #endif

    // =========================================================================
    // SECTION 7 : MACROS DE CONFIGURATION DU BUILD
    // =========================================================================
    // Indicateurs du mode de compilation (debug/release/optimisation).

    // -------------------------------------------------------------------------
    // Sous-section : Détection Debug/Release
    // -------------------------------------------------------------------------

    /**
     * @brief Indicateur de build debug
     * @def NKENTSEU_DEBUG_BUILD
     * @value 1 si debug, 0 sinon
     * @ingroup BuildMacros
     *
     * Détecté via _DEBUG, DEBUG, ou absence de NDEBUG.
     */
    #if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
        #define NKENTSEU_DEBUG_BUILD 1
        #define NKENTSEU_RELEASE_BUILD 0
    #else
        #define NKENTSEU_DEBUG_BUILD 0
        #define NKENTSEU_RELEASE_BUILD 1
    #endif

    // -------------------------------------------------------------------------
    // Sous-section : Niveau d'optimisation
    // -------------------------------------------------------------------------

    /**
     * @brief Indicateur de build optimisé
     * @def NKENTSEU_OPTIMIZED_BUILD
     * @value 1 si optimisé, 0 sinon
     * @ingroup BuildMacros
     *
     * Détecté via __OPTIMIZE__ (GCC/Clang) ou définition de NDEBUG.
     * Note : un build peut être "release" mais non optimisé (ex: -O0).
     */
    #if defined(__OPTIMIZE__) || defined(NDEBUG)
        #define NKENTSEU_OPTIMIZED_BUILD 1
    #else
        #define NKENTSEU_OPTIMIZED_BUILD 0
    #endif

    // =========================================================================
    // SECTION 8 : MACROS D'ALIGNEMENT MÉMOIRE
    // =========================================================================
    // Utilise NKENTSEU_ALIGN de NkArchDetect.h pour l'alignement cache/SIMD.

    /**
     * @brief Alignement sur ligne de cache CPU
     * @def NKENTSEU_CACHE_ALIGNED
     * @ingroup AlignmentMacros
     *
     * Utilise NKENTSEU_CACHE_LINE_SIZE de NkArchDetect.h (typiquement 64 octets).
     * Recommandé pour les structures fréquemment accédées en multithreading
     * afin d'éviter le false sharing.
     *
     * @example
     * @code
     * struct NKENTSEU_CACHE_ALIGNED ThreadLocalData {
     *     int counter;  // Aligné sur ligne de cache
     * };
     * @endcode
     */
    #define NKENTSEU_CACHE_ALIGNED NKENTSEU_ALIGN(NKENTSEU_CACHE_LINE_SIZE)

    /**
     * @brief Alignement requis pour les instructions SIMD
     * @def NKENTSEU_SIMD_ALIGNMENT
     * @value 32 pour AVX/AVX2, 16 pour SSE/NEON, 16 par défaut
     * @ingroup AlignmentMacros
     *
     * Détecté via les macros de compilation __AVX__, __SSE__, __ARM_NEON, etc.
     */
    #if defined(__AVX__) || defined(__AVX2__)
        #define NKENTSEU_SIMD_ALIGNMENT 32
    #elif defined(__SSE__) || defined(__SSE2__)
        #define NKENTSEU_SIMD_ALIGNMENT 16
    #elif defined(__ARM_NEON__) || defined(__ARM_NEON)
        #define NKENTSEU_SIMD_ALIGNMENT 16
    #else
        #define NKENTSEU_SIMD_ALIGNMENT 16
    #endif

    /**
     * @brief Alignement pour les données SIMD
     * @def NKENTSEU_SIMD_ALIGNED
     * @ingroup AlignmentMacros
     *
     * Utilise NKENTSEU_SIMD_ALIGNMENT pour garantir l'alignement requis
     * par les instructions vectorielles (SSE, AVX, NEON, etc.).
     *
     * @example
     * @code
     * NKENTSEU_SIMD_ALIGNED float simdBuffer[16];  // Aligné pour AVX/SSE
     * @endcode
     */
    #define NKENTSEU_SIMD_ALIGNED NKENTSEU_ALIGN(NKENTSEU_SIMD_ALIGNMENT)

    // =========================================================================
    // SECTION 9 : FONCTIONS UTILITAIRES INLINE (API PUBLIQUE)
    // =========================================================================
    // Fonctions inline fournissant un accès rapide aux informations de plateforme.

    namespace nkentseu {

        namespace platform {

            /**
             * @brief Obtenir le nom de la plateforme sous forme de chaîne
             * @return Chaîne constante : "Windows", "Linux", "macOS", etc.
             * @ingroup PlatformUtilities
             *
             * @note Cette fonction utilise des macros de compilation, donc
             * le résultat est déterminé à la compilation, pas à l'exécution.
             */
            NKENTSEU_FORCE_INLINE const char* GetPlatformName() {
                #if defined(NKENTSEU_PLATFORM_WINDOWS)
                    return "Windows";
                #elif defined(NKENTSEU_PLATFORM_LINUX)
                    return "Linux";
                #elif defined(NKENTSEU_PLATFORM_MACOS)
                    return "macOS";
                #elif defined(NKENTSEU_PLATFORM_ANDROID)
                    return "Android";
                #elif defined(NKENTSEU_PLATFORM_IOS)
                    return "iOS";
                #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
                    return "Web";
                #else
                    return "Unknown";
                #endif
            }

            /**
             * @brief Obtenir le nom de l'architecture sous forme de chaîne
             * @return Chaîne constante : "x64", "x86", "ARM64", etc.
             * @ingroup PlatformUtilities
             */
            NKENTSEU_FORCE_INLINE const char* GetArchName() {
                #if defined(NKENTSEU_ARCH_X86_64)
                    return "x64";
                #elif defined(NKENTSEU_ARCH_X86)
                    return "x86";
                #elif defined(NKENTSEU_ARCH_ARM64)
                    return "ARM64";
                #elif defined(NKENTSEU_ARCH_ARM)
                    return "ARM";
                #elif defined(__wasm__) || defined(__EMSCRIPTEN__)
                    return "WebAssembly";
                #else
                    return "Unknown";
                #endif
            }

            /**
             * @brief Obtenir le nom du compilateur sous forme de chaîne
             * @return Chaîne constante : "MSVC", "Clang", "GCC"
             * @ingroup PlatformUtilities
             */
            NKENTSEU_FORCE_INLINE const char* GetCompilerName() {
                #if defined(NKENTSEU_COMPILER_MSVC)
                    return "MSVC";
                #elif defined(NKENTSEU_COMPILER_CLANG)
                    return "Clang";
                #elif defined(NKENTSEU_COMPILER_GCC)
                    return "GCC";
                #else
                    return "Unknown";
                #endif
            }

            /**
             * @brief Vérifier si l'architecture cible est 64-bit
             * @return true si 64-bit, false sinon
             * @ingroup PlatformChecks
             *
             * @note Fonction constexpr implicite : évaluée à la compilation.
             */
            NKENTSEU_FORCE_INLINE bool Is64Bit() {
                #if defined(NKENTSEU_ARCH_64BIT)
                    return true;
                #else
                    return false;
                #endif
            }

            /**
             * @brief Vérifier si le système utilise le little-endian
             * @return true si little-endian, false sinon
             * @ingroup PlatformChecks
             *
             * @note Basé sur NKENTSEU_ARCH_LITTLE_ENDIAN de NkArchDetect.h.
             */
            NKENTSEU_FORCE_INLINE bool IsLittleEndian() {
                #if defined(NKENTSEU_ARCH_LITTLE_ENDIAN)
                    return true;
                #else
                    return false;
                #endif
            }

        } // namespace platform

    } // namespace nkentseu

#endif // NKENTSEU_PLATFORM_NKPLATFORMCONFIG_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKPLATFORMCONFIG.H
// =============================================================================
// Ce fichier fournit des macros et structures pour accéder aux informations
// de configuration de plateforme, tant à la compilation qu'à l'exécution.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Configuration conditionnelle basée sur les macros
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkPlatformConfig.h"

    void ConfigureApplication() {
        // Ajustement basé sur le mode de build
        #if NKENTSEU_DEBUG_BUILD
            EnableVerboseLogging();
            DisableOptimizations();
        #elif NKENTSEU_RELEASE_BUILD
            EnablePerformanceMode();
        #endif

        // Ajustement basé sur la plateforme
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            UseWindowsSpecificAPIs();
        #elif defined(NKENTSEU_PLATFORM_LINUX)
            UseLinuxSpecificAPIs();
        #endif

        // Ajustement basé sur l'architecture
        #if NKENTSEU_ARCH_64BIT
            Enable64BitOptimizations();
        #endif
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Accès aux informations via PlatformConfig
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkPlatformConfig.h"
    #include <cstdio>

    void PrintBuildInfo() {
        const auto& config = nkentseu::platform::GetPlatformConfig();

        std::printf("Platform : %s\n", config.platformName);
        std::printf("Arch     : %s\n", config.archName);
        std::printf("Compiler : %s v%d\n", config.compilerName, config.compilerVersion);
        std::printf("Build    : %s\n", config.isDebugBuild ? "Debug" : "Release");
        std::printf("64-bit   : %s\n", config.is64Bit ? "Yes" : "No");
        std::printf("Endian   : %s\n", config.isLittleEndian ? "Little" : "Big");

        std::printf("\nFeatures:\n");
        if (config.hasUnicode)    std::printf("  Unicode\n");
        if (config.hasThreading)  std::printf("  Threading\n");
        if (config.hasFilesystem) std::printf("  Filesystem (C++17)\n");
        if (config.hasNetwork)    std::printf("  Network\n");

        std::printf("\nLimits:\n");
        std::printf("  Max path length : %d\n", config.maxPathLength);
        std::printf("  Cache line size : %d bytes\n", config.cacheLineSize);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Sélection de code optimisé via PlatformCapabilities
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkPlatformConfig.h"

    void InitializeComputeEngine() {
        const auto& caps = nkentseu::platform::GetPlatformCapabilities();

        // Sélection du meilleur chemin SIMD disponible
        if (caps.hasAVX2) {
            InitializeAVX2Backend();
        } else if (caps.hasAVX) {
            InitializeAVXBackend();
        } else if (caps.hasSSE2) {
            InitializeSSE2Backend();
        } else if (caps.hasNEON) {
            InitializeNEONBackend();
        } else {
            InitializeScalarBackend();
        }

        // Ajustement basé sur la mémoire disponible
        if (caps.totalPhysicalMemory >= 16ULL * 1024 * 1024 * 1024) {
            EnableLargeMemoryMode();
        } else if (caps.totalPhysicalMemory >= 4ULL * 1024 * 1024 * 1024) {
            EnableStandardMemoryMode();
        } else {
            EnableLowMemoryMode();
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Gestion portable des chemins de fichiers
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkPlatformConfig.h"
    #include <string>

    std::string BuildConfigPath(const char* filename) {
        // Utiliser le séparateur de chemin de la plateforme cible
        std::string path = "config";
        path += NKENTSEU_PATH_SEPARATOR_STR;
        path += filename;
        return path;
    }

    // Vérifier que le chemin ne dépasse pas la limite de la plateforme
    bool IsValidPath(const char* path) {
        return (path != nullptr) &&
               (std::strlen(path) < static_cast<size_t>(NKENTSEU_MAX_PATH));
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Utilisation des macros d'alignement pour performance
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkPlatformConfig.h"

    // Structure alignée sur ligne de cache pour éviter le false sharing
    struct NKENTSEU_CACHE_ALIGNED ThreadCounter {
        alignas(NKENTSEU_CACHE_LINE_SIZE) int64_t counter;
    };

    // Buffer aligné pour opérations SIMD
    NKENTSEU_SIMD_ALIGNED float simdBuffer[32];  // 32 floats = 128 bytes (AVX)

    void ProcessVectorized(float* data, size_t count) {
        // Utiliser le buffer aligné pour des opérations SIMD safe
        #if defined(__AVX__)
            // AVX nécessite un alignement 32 bytes
            ProcessWithAVX(data, count, simdBuffer);
        #elif defined(__SSE2__)
            // SSE2 nécessite un alignement 16 bytes
            ProcessWithSSE2(data, count, simdBuffer);
        #else
            // Fallback scalaire
            ProcessScalar(data, count);
        #endif
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Combinaison avec d'autres modules NKPlatform
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkPlatformDetect.h"   // NKENTSEU_PLATFORM_*
    #include "NKPlatform/NkArchDetect.h"       // NKENTSEU_ARCH_*
    #include "NKPlatform/NkPlatformConfig.h"   // Configuration

    void PlatformAwareInitialization() {
        const auto& config = nkentseu::platform::GetPlatformConfig();
        const auto& caps = nkentseu::platform::GetPlatformCapabilities();

        // Logging des informations de démarrage
        NK_FOUNDATION_LOG_INFO("Starting on %s/%s with %s",
            config.platformName,
            config.archName,
            config.compilerName);

        // Configuration basée sur les capacités détectées
        if (caps.hasDisplay) {
            InitializeGraphics(caps.primaryScreenWidth, caps.primaryScreenHeight);
        } else {
            InitializeHeadlessMode();
        }

        // Ajustement du nombre de threads de travail
        int workerThreads = caps.processorCount;
        if (config.isDebugBuild) {
            workerThreads = 1;  // Simplifier le debug
        }
        InitializeThreadPool(workerThreads);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Feature detection pour activation conditionnelle
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkPlatformConfig.h"

    class FeatureManager {
    public:
        FeatureManager() {
            const auto& config = nkentseu::platform::GetPlatformConfig();

            // Activer les fonctionnalités selon ce qui est disponible
            if (config.hasFilesystem) {
                m_useStdFilesystem = true;
            } else {
                m_useStdFilesystem = false;
                NK_FOUNDATION_LOG_WARN("Filesystem not available, using fallback");
            }

            if (config.hasNetwork) {
                InitializeNetworkStack();
            }

            if (config.hasThreading) {
                InitializeThreadingPrimitives();
            }
        }

        bool UseStdFilesystem() const {
            return m_useStdFilesystem;
        }

    private:
        bool m_useStdFilesystem = false;
    };
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================