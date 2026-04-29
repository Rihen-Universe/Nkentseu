// =============================================================================
// NKCore/NkPlatform.h
// Informations plateforme runtime et utilitaires système multi-plateforme.
//
// Design :
//  - Énumérations typées pour plateforme, architecture, affichage, graphiques
//  - Structure NkPlatformInfo : agrégat complet des capacités système détectées
//  - Classe NkSourceLocation : capture automatique fichier/fonction/ligne/colonne
//  - API C pour interopérabilité (NkGetPlatformInfo(), NkHasSIMDFeature(), etc.)
//  - Fonctions inline compile-time pour détection rapide (NkIsDesktop(), etc.)
//  - Namespace arch : utilitaires d'alignement et métadonnées CPU
//  - Namespace memory : allocation mémoire alignée avec fallback portable
//  - Intégration avec NkPlatformDetect.h, NkArchDetect.h, NkCompilerDetect.h
//
// Intégration :
//  - Utilise NKCore/NkTypes.h pour types portables nk_uint32, nk_ptr, etc.
//  - Utilise NKCore/NkExport.h et NkCoreExport.h pour visibilité des symboles
//  - Utilise NKPlatform/NkEndianness.h pour détection endianness
//  - Compatible avec NKCore/Assert/NkAssert.h pour validations debug
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CORE_NKPLATFORM_H_INCLUDED
#define NKENTSEU_CORE_NKPLATFORM_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules de détection et types fondamentaux.

    #include "NKPlatform/NkPlatformDetect.h"
    #include "NKPlatform/NkArchDetect.h"
    #include "NKPlatform/NkCompilerDetect.h"
    #include "NKPlatform/NkEndianness.h"
    #include "NkTypes.h"
    #include "NkCoreApi.h"
    #include "NkVersion.h"

    #include <cstddef>

    // -------------------------------------------------------------------------
    // SECTION 2 : MACROS BUILTIN POUR LOCALISATION SOURCE
    // -------------------------------------------------------------------------
    // Définition portable des macros __builtin_FILE(), __builtin_LINE(), etc.

    /**
     * @defgroup SourceLocationMacros Macros de Localisation Source
     * @brief Accès portable aux informations de localisation compile-time
     * @ingroup PlatformUtilities
     *
     * Ces macros fournissent un accès uniforme aux informations de localisation
     * dans le code source, avec fallback pour les compilateurs anciens.
     *
     * @note
     *   - MSVC >= 1926 (VS 2019 16.6) supporte les builtins GCC-style
     *   - GCC/Clang : utilisation directe de __builtin_FILE(), etc.
     *   - Fallback : __FILE__, __LINE__, "unknown" pour __FUNCTION__
     *
     * @example
     * @code
     * // Logging avec localisation automatique
     * void LogDebug(const char* msg) {
     *     printf("[%s:%d] %s: %s\n",
     *         NKENTSEU_BUILTIN_FILE,
     *         NKENTSEU_BUILTIN_LINE,
     *         NKENTSEU_BUILTIN_FUNCTION,
     *         msg);
     * }
     * @endcode
     */
    /** @{ */

    // ============================================================
    // DÉTECTION COMPILATEUR POUR BUILTINS
    // ============================================================

    #if defined(NKENTSEU_COMPILER_MSVC)
        // MSVC supporte les builtins depuis VS 2019 16.6 (_MSC_VER >= 1926)
        #if _MSC_VER >= 1926
            /** @brief Nom du fichier source courant (MSVC builtin) */
            #define NKENTSEU_BUILTIN_FILE __builtin_FILE()
            /** @brief Nom de la fonction courante (MSVC builtin) */
            #define NKENTSEU_BUILTIN_FUNCTION __builtin_FUNCTION()
            /** @brief Numéro de ligne courant (MSVC builtin) */
            #define NKENTSEU_BUILTIN_LINE __builtin_LINE()
            /** @brief Numéro de colonne courant (MSVC builtin) */
            #define NKENTSEU_BUILTIN_COLUMN __builtin_COLUMN()
        #else
            // Fallback pour MSVC ancien : éviter l'erreur C1036 avec __FUNCTION__
            #define NKENTSEU_BUILTIN_FILE __FILE__
            #define NKENTSEU_BUILTIN_FUNCTION "unknown"
            #define NKENTSEU_BUILTIN_LINE __LINE__
            #define NKENTSEU_BUILTIN_COLUMN 0
        #endif
    #else
        // GCC et Clang : support natif des builtins
        #define NKENTSEU_BUILTIN_FILE __builtin_FILE()
        #define NKENTSEU_BUILTIN_FUNCTION __builtin_FUNCTION()
        #define NKENTSEU_BUILTIN_LINE __builtin_LINE()

        // Détection optionnelle de __builtin_COLUMN()
        #if defined(__has_builtin)
            #if __has_builtin(__builtin_COLUMN)
                #define NKENTSEU_BUILTIN_COLUMN __builtin_COLUMN()
            #else
                #define NKENTSEU_BUILTIN_COLUMN 0
            #endif
        #elif defined(__clang__)
            #define NKENTSEU_BUILTIN_COLUMN __builtin_COLUMN()
        #else
            #define NKENTSEU_BUILTIN_COLUMN 0
        #endif
    #endif

    /**
     * @brief Capture la localisation actuelle via NkSourceLocation
     * @def NkCurrentSourceLocation
     * @ingroup SourceLocationMacros
     * @note Utilise les builtins pour fichier, fonction, ligne, colonne
     */
    #define NkCurrentSourceLocation \
        nkentseu::platform::NkSourceLocation::Current( \
            NKENTSEU_BUILTIN_FILE, \
            NKENTSEU_BUILTIN_FUNCTION, \
            NKENTSEU_BUILTIN_LINE, \
            NKENTSEU_BUILTIN_COLUMN)

    /**
     * @brief Alias de compatibilité pour NkCurrentSourceLocation
     * @def NkCurrentLocation
     * @ingroup SourceLocationMacros
     * @deprecated Préférer NkCurrentSourceLocation
     */
    #define NkCurrentLocation() \
        nkentseu::platform::NkSourceLocation::Current( \
            NKENTSEU_BUILTIN_FILE, \
            __FUNCTION__, \
            NKENTSEU_BUILTIN_LINE, \
            NKENTSEU_BUILTIN_COLUMN)

    /** @} */ // End of SourceLocationMacros

    // -------------------------------------------------------------------------
    // SECTION 3 : ESPACE DE NOMS PRINCIPAL
    // -------------------------------------------------------------------------
    // Déclaration de l'espace de noms principal nkentseu.

    namespace nkentseu {

        // Indentation niveau 1 : namespace platform
        namespace platform {

            // ====================================================================
            // SECTION 4 : TYPES ET ÉNUMÉRATIONS PLATEFORME
            // ====================================================================
            // Définitions type-safe pour identification des capacités système.

            /**
             * @defgroup PlatformTypes Types et Énumérations Plateforme
             * @brief Types fondamentaux pour identification plateforme/architecture
             * @ingroup PlatformCore
             *
             * Ces énumérations fournissent une identification type-safe des
             * différentes facettes de l'environnement d'exécution.
             *
             * @note
             *   - Toutes les enum sont basées sur nk_uint8 pour compacité
             *   - NK_UNKNOWN = 0 permet l'initialisation par défaut sécurisée
             *   - Les valeurs sont stables pour sérialisation/configuration
             *
             * @example
             * @code
             * // Vérification de plateforme pour code spécifique
             * auto platform = nkentseu::platform::NkGetPlatformInfo()->platform;
             * if (platform == nkentseu::platform::NkPlatformType::NK_WINDOWS) {
             *     // Code Windows-specific
             * }
             *
             * // Filtrage par capacité SIMD
             * if (nkentseu::platform::NkHasSIMDFeature("AVX2")) {
             *     UseAVX2OptimizedPath();
             * }
             * @endcode
             */
            /** @{ */

            /**
             * @brief Alias pour les numéros de version (Major.Minor.Patch encodés)
             * @typedef NkVersion
             * @ingroup PlatformTypes
             * @note Utilise nk_uint32 : 16 bits major, 8 bits minor, 8 bits patch
             */
            using NkVersion = nk_uint32;

            /**
             * @brief Types de plateformes supportées
             * @enum NkPlatformType
             * @ingroup PlatformTypes
             *
             * Liste exhaustive des systèmes d'exploitation cibles.
             * Utilisé pour la sélection de code platform-specific et
             * l'affichage d'informations de diagnostic.
             *
             * @note
             *   - Les variantes BSD sont listées séparément pour précision
             *   - NK_WSL permet de détecter Windows Subsystem for Linux
             *   - NK_EMSCRIPTEN couvre WebAssembly/Emscripten
             */
            enum class NkPlatformType : nk_uint8 {
                NK_UNKNOWN = 0,       ///< Plateforme non reconnue
                NK_WINDOWS,           ///< Windows (NT kernel)
                NK_LINUX,             ///< Linux (glibc/musl)
                NK_BSD,               ///< BSD générique (fallback)
                NK_MACOS,             ///< macOS (Darwin)
                NK_IOS,               ///< iOS (Apple mobile)
                NK_ANDROID,           ///< Android (Linux-based)
                NK_HARMONYOS,         ///< HarmonyOS (Huawei)
                NK_NINTENDO_SWITCH,   ///< Nintendo Switch
                NK_NINTENDO_DS,       ///< Nintendo DS/3DS family
                NK_PSP,               ///< PlayStation Portable
                NK_EMSCRIPTEN,        ///< WebAssembly via Emscripten
                NK_WSL,               ///< Windows Subsystem for Linux
                NK_WATCHOS,           ///< watchOS (Apple wearable)
                NK_TVOS,              ///< tvOS (Apple TV)
                NK_FREEBSD,           ///< FreeBSD
                NK_NETBSD,            ///< NetBSD
                NK_OPENBSD,           ///< OpenBSD
                NK_DRAGONFLYBSD       ///< DragonFly BSD
            };

            /**
             * @brief Types d'architectures CPU supportées
             * @enum NkArchitectureType
             * @ingroup PlatformTypes
             *
             * Identification de l'architecture du processeur pour
             * la sélection d'instructions optimisées (SIMD) et
             * l'adaptation des algorithmes.
             *
             * @note
             *   - NK_X86 = 32-bit x86, NK_X64 = 64-bit x86-64
             *   - NK_ARM32/NK_ARM64 couvrent les variantes ARMv7/AArch64
             *   - NK_WASM pour WebAssembly (architecture virtuelle)
             */
            enum class NkArchitectureType : nk_uint8 {
                NK_UNKNOWN = 0,   ///< Architecture non reconnue
                NK_X86,           ///< x86 32-bit (IA-32)
                NK_X64,           ///< x86-64 64-bit (AMD64/Intel 64)
                NK_ARM32,         ///< ARM 32-bit (ARMv7, etc.)
                NK_ARM64,         ///< ARM 64-bit (AArch64)
                NK_MIPS,          ///< MIPS (embarqué/legacy)
                NK_RISCV32,       ///< RISC-V 32-bit
                NK_RISCV64,       ///< RISC-V 64-bit
                NK_PPC32,         ///< PowerPC 32-bit
                NK_PPC64,         ///< PowerPC 64-bit
                NK_WASM,          ///< WebAssembly (virtuel)
                NK_ARM9           ///< ARM9 (legacy embarqué)
            };

            /**
             * @brief Types de systèmes d'affichage (windowing)
             * @enum NkDisplayType
             * @ingroup PlatformTypes
             *
             * Identification du système de fenêtrage pour
             * l'intégration graphique et la gestion des événements.
             *
             * @note
             *   - NK_NONE : environnement headless ou console
             *   - NK_COCOA : système natif macOS (AppKit)
             *   - NK_XCB/XLIB : variantes de l'API X11
             */
            enum class NkDisplayType : nk_uint8 {
                NK_NONE = 0,    ///< Pas de système d'affichage (headless)
                NK_WIN32,       ///< Win32 API (Windows)
                NK_COCOA,       ///< Cocoa/AppKit (macOS)
                NK_ANDROID,     ///< Android Surface/WindowManager
                NK_HARMONY_OS,  ///< HarmonyOS windowing
                NK_WAYLAND,     ///< Wayland (Linux moderne)
                NK_XCB,         ///< XCB (X11 low-level)
                NK_XLIB         ///< Xlib (X11 traditional)
            };

            /**
             * @brief Types d'APIs graphiques supportées
             * @enum NkGraphicsAPI
             * @ingroup PlatformTypes
             *
             * Identification du backend graphique détecté ou sélectionné.
             * Utilisé pour le routing vers l'implémentation appropriée.
             *
             * @note
             *   - NK_NONE : mode software ou non-initialisé
             *   - NK_SOFTWARE : rasterisation logicielle (fallback)
             *   - Les autres valeurs correspondent aux APIs hardware
             */
            enum class NkGraphicsAPI : nk_uint8 {
                NK_NONE = 0,      ///< Aucune API graphique
                NK_VULKAN,        ///< Vulkan (cross-platform moderne)
                NK_METAL,         ///< Metal (Apple platforms)
                NK_OPENGL,        ///< OpenGL (legacy cross-platform)
                NK_DIRECTX,       ///< Direct3D (Windows/Xbox)
                NK_SOFTWARE       ///< Rasterisation logicielle
            };

            /** @} */ // End of PlatformTypes

            // ====================================================================
            // SECTION 5 : STRUCTURES D'INFORMATIONS
            // ====================================================================
            // Conteneurs de données pour métadonnées plateforme.

            /**
             * @defgroup PlatformStructs Structures d'Informations Plateforme
             * @brief Conteneurs pour métadonnées version et système
             * @ingroup PlatformCore
             *
             * Ces structures agrègent les informations détectées au runtime
             * pour un accès centralisé et type-safe aux capacités système.
             */
            /** @{ */

            /**
             * @brief Informations de version sémantique (Major.Minor.Patch)
             * @struct NkVersionInfo
             * @ingroup PlatformStructs
             *
             * Représentation structurée d'un numéro de version, avec
             * à la fois les composants numériques et une représentation
             * string pour l'affichage.
             *
             * @note
             *   - versionString pointe vers un littéral ou buffer statique
             *   - Ne pas libérer/modifier versionString
             *
             * @example
             * @code
             * nkentseu::platform::NkVersionInfo vulkanVersion;
             * vulkanVersion.major = 1;
             * vulkanVersion.minor = 3;
             * vulkanVersion.patch = 250;
             * vulkanVersion.versionString = "1.3.250";
             *
             * printf("Vulkan %s detected\n", vulkanVersion.versionString);
             * @endcode
             */
            struct NKENTSEU_CORE_API NkVersionInfo {
                nk_uint32 major;              ///< Numéro de version majeure
                nk_uint32 minor;              ///< Numéro de version mineure
                nk_uint32 patch;              ///< Numéro de version patch
                const nk_char *versionString; ///< Représentation string "X.Y.Z"
            };

            /**
             * @brief Informations complètes sur la plateforme runtime
             * @struct NkPlatformInfo
             * @ingroup PlatformStructs
             *
             * Agrégat exhaustif des capacités système détectées au runtime :
             * OS, architecture CPU, mémoire, cache, SIMD, build config, etc.
             *
             * @note
             *   - Toutes les chaînes pointent vers des données statiques
             *   - Thread-safe : lecture seule après initialisation
             *   - Initialisé automatiquement au premier appel de NkGetPlatformInfo()
             *
             * @warning
             *   Ne pas modifier les champs directement : utiliser les fonctions
             *   d'initialisation dédiées si extension nécessaire.
             *
             * @example
             * @code
             * const auto* info = nkentseu::platform::NkGetPlatformInfo();
             *
             * // Informations CPU
             * printf("CPU: %s (%u cores, %u threads)\n",
             *     info->archName,
             *     info->cpuCoreCount,
             *     info->cpuThreadCount);
             *
             * // Vérification SIMD
             * if (info->hasAVX2) {
             *     UseAVX2OptimizedAlgorithm();
             * }
             *
             * // Mémoire disponible
             * printf("Available RAM: %llu MB\n",
             *     static_cast<unsigned long long>(info->availableMemory / (1024*1024)));
             * @endcode
             */
            struct NKENTSEU_CORE_API NkPlatformInfo {
                // --------------------------------------------------------
                // Informations OS et Compilateur
                // --------------------------------------------------------

                /** @brief Type de plateforme détectée */
                NkPlatformType platform;

                /** @brief Type d'architecture CPU détectée */
                NkArchitectureType architecture;

                /** @brief Nom lisible de l'OS (ex: "Windows", "Linux") */
                const nk_char *osName;

                /** @brief Version de l'OS (ex: "10.0.19041", "5.15.0") */
                const nk_char *osVersion;

                /** @brief Nom lisible de l'architecture (ex: "x86_64", "ARM64") */
                const nk_char *archName;

                /** @brief Nom du compilateur (ex: "MSVC", "GCC", "Clang") */
                const nk_char *compilerName;

                /** @brief Version du compilateur (ex: "19.29", "11.3.0") */
                const nk_char *compilerVersion;

                // --------------------------------------------------------
                // Informations CPU
                // --------------------------------------------------------

                /** @brief Nombre de cœurs physiques */
                nk_uint32 cpuCoreCount;

                /** @brief Nombre de threads matériels (incluant hyperthreading) */
                nk_uint32 cpuThreadCount;

                /** @brief Taille du cache L1 en bytes */
                nk_uint32 cpuL1CacheSize;

                /** @brief Taille du cache L2 en bytes */
                /** @note Peut être par cœur ou partagé selon l'architecture */
                nk_uint32 cpuL2CacheSize;

                /** @brief Taille du cache L3 en bytes */
                /** @note Généralement partagé entre tous les cœurs */
                nk_uint32 cpuL3CacheSize;

                /** @brief Taille d'une ligne de cache en bytes */
                /** @note Typiquement 64 bytes sur x86/x64 modernes */
                nk_uint32 cacheLineSize;

                // --------------------------------------------------------
                // Informations Mémoire
                // --------------------------------------------------------

                /** @brief RAM totale installée en bytes */
                nk_uint64 totalMemory;

                /** @brief RAM disponible (non-allouée) en bytes */
                /** @note Valeur snapshot au moment de l'initialisation */
                nk_uint64 availableMemory;

                /** @brief Taille d'une page mémoire en bytes */
                /** @note Typiquement 4096 (4KB) sur la plupart des OS */
                nk_uint32 pageSize;

                /** @brief Granularité d'allocation virtuelle en bytes */
                /** @note Souvent égale à pageSize, peut différer sur Windows */
                nk_uint32 allocationGranularity;

                // --------------------------------------------------------
                // Support SIMD (Single Instruction Multiple Data)
                // --------------------------------------------------------

                /** @brief Support des instructions SSE (x86) */
                nk_bool hasSSE;

                /** @brief Support des instructions SSE2 */
                nk_bool hasSSE2;

                /** @brief Support des instructions SSE3 */
                nk_bool hasSSE3;

                /** @brief Support des instructions SSE4.1 */
                nk_bool hasSSE4_1;

                /** @brief Support des instructions SSE4.2 */
                nk_bool hasSSE4_2;

                /** @brief Support des instructions AVX */
                nk_bool hasAVX;

                /** @brief Support des instructions AVX2 */
                nk_bool hasAVX2;

                /** @brief Support des instructions AVX-512 */
                nk_bool hasAVX512;

                /** @brief Support des instructions NEON (ARM) */
                nk_bool hasNEON;

                // --------------------------------------------------------
                // Caractéristiques Plateforme
                // --------------------------------------------------------

                /** @brief true si architecture little-endian */
                nk_bool isLittleEndian;

                /** @brief true si architecture 64-bit */
                nk_bool is64Bit;

                /** @brief Type d'endianness détecté */
                NkEndianness endianness;

                // --------------------------------------------------------
                // Informations Build
                // --------------------------------------------------------

                /** @brief true si compilation en mode Debug */
                nk_bool isDebugBuild;

                /** @brief true si bibliothèque partagée (.dll/.so/.dylib) */
                nk_bool isSharedLibrary;

                /** @brief Type de build : "Debug", "Release", etc. */
                const nk_char *buildType;

                // --------------------------------------------------------
                // Capacités Plateforme
                // --------------------------------------------------------

                /** @brief true si support du multi-threading */
                nk_bool hasThreading;

                /** @brief true si support de la mémoire virtuelle */
                nk_bool hasVirtualMemory;

                /** @brief true si accès au système de fichiers */
                nk_bool hasFileSystem;

                /** @brief true si support réseau (sockets, etc.) */
                nk_bool hasNetwork;

                // --------------------------------------------------------
                // Affichage et Graphiques
                // --------------------------------------------------------

                /** @brief Type de système d'affichage détecté */
                NkDisplayType display;

                /** @brief API graphique détectée ou sélectionnée */
                NkGraphicsAPI graphicsApi;

                /** @brief Version de l'API graphique */
                NkVersionInfo graphicsApiVersion;
            };

            /** @} */ // End of PlatformStructs

            // ====================================================================
            // SECTION 6 : CLASSE NKSOURCELOCATION
            // ====================================================================
            // Capture type-safe de la localisation dans le code source.

            /**
             * @defgroup PlatformUtils Utilitaires Plateforme
             * @brief Classes et fonctions utilitaires pour développement
             * @ingroup PlatformCore
             *
             * Composants pour faciliter le logging, le débogage et
             * la gestion d'erreurs avec contexte source automatique.
             */
            /** @{ */

            /**
             * @brief Informations de localisation dans le code source
             * @class NkSourceLocation
             * @ingroup PlatformUtils
             *
             * Capture immutable du contexte source (fichier, fonction, ligne, colonne)
             * pour usage dans le logging, les assertions et le profiling.
             *
             * @note
             *   - Tous les membres sont constexpr pour évaluation compile-time
             *   - Les pointeurs de chaîne pointent vers des littéraux statiques
             *   - Thread-safe : pas d'état mutable
             *
             * @example
             * @code
             * // Logging avec localisation automatique
             * void LogError(const char* msg, nkentseu::platform::NkSourceLocation loc = NkCurrentSourceLocation) {
             *     fprintf(stderr, "[%s:%d] %s: %s\n",
             *         loc.FileName(),
             *         loc.Line(),
             *         loc.FunctionName(),
             *         msg);
             * }
             *
             * // Usage avec macro pour éviter la verbosité
             * #define LOG_ERROR(msg) LogError(msg, NkCurrentSourceLocation)
             * LOG_ERROR("Connection failed");  // [network.cpp:42] Connect(): Connection failed
             * @endcode
             */
            class NKENTSEU_CORE_API NkSourceLocation {
                public:
                    /**
                     * @brief Constructeur par défaut (valeurs "unknown")
                     * @note constexpr pour usage dans des contextes compile-time
                     */
                    constexpr NkSourceLocation() noexcept = default;

                    /**
                     * @brief Récupère le nom du fichier source
                     * @return Pointeur vers chaîne statique (ne pas libérer)
                     * @note Peut être "unknown" si builtin non supporté
                     */
                    constexpr const nk_char *FileName() const noexcept {
                        return mFile;
                    }

                    /**
                     * @brief Récupère le nom de la fonction courante
                     * @return Pointeur vers chaîne statique (ne pas libérer)
                     * @note Format dépend du compilateur : nom simple ou signature complète
                     */
                    constexpr const nk_char *FunctionName() const noexcept {
                        return mFunction;
                    }

                    /**
                     * @brief Récupère le numéro de ligne source
                     * @return Numéro de ligne (1-based), 0 si inconnu
                     */
                    constexpr nk_uint32 Line() const noexcept {
                        return mLine;
                    }

                    /**
                     * @brief Récupère le numéro de colonne source
                     * @return Numéro de colonne (1-based), 0 si inconnu/non-supporté
                     * @note __builtin_COLUMN() n'est pas supporté par tous les compilateurs
                     */
                    constexpr nk_uint32 Column() const noexcept {
                        return mColumn;
                    }

                    /**
                     * @brief Crée une instance avec la localisation actuelle
                     * @param file Nom du fichier (défaut: NKENTSEU_BUILTIN_FILE)
                     * @param function Nom de la fonction (défaut: NKENTSEU_BUILTIN_FUNCTION)
                     * @param line Numéro de ligne (défaut: NKENTSEU_BUILTIN_LINE)
                     * @param column Numéro de colonne (défaut: NKENTSEU_BUILTIN_COLUMN)
                     * @return Nouvelle instance NkSourceLocation
                     * @note constexpr : peut être évalué à la compilation si arguments constexpr
                     */
                    static constexpr NkSourceLocation Current(
                        const nk_char *file = NKENTSEU_BUILTIN_FILE,
                        const nk_char *function = NKENTSEU_BUILTIN_FUNCTION,
                        nk_uint32 line = NKENTSEU_BUILTIN_LINE,
                        nk_uint32 column = NKENTSEU_BUILTIN_COLUMN
                    ) noexcept {
                        NkSourceLocation loc;
                        loc.mFile = file;
                        loc.mFunction = function;
                        loc.mLine = line;
                        loc.mColumn = column;
                        return loc;
                    }

                private:
                    const nk_char *mFile = "unknown";       ///< Nom du fichier source
                    const nk_char *mFunction = "unknown";   ///< Nom de la fonction
                    nk_uint32 mLine = 0;                    ///< Numéro de ligne
                    nk_uint32 mColumn = 0;                  ///< Numéro de colonne
            };

            /** @} */ // End of PlatformUtils

        } // namespace platform

    } // namespace nkentseu

    // -------------------------------------------------------------------------
    // SECTION 7 : API C - FONCTIONS RUNTIME PLATEFORME
    // -------------------------------------------------------------------------
    // Interface C pour interopérabilité et usage depuis du code non-C++.

    /**
     * @defgroup PlatformAPI API Plateforme Runtime (C)
     * @brief Fonctions C pour accès aux informations plateforme
     * @ingroup PlatformInterop
     *
     * Cette API C fournit un accès thread-safe et stable aux informations
     * système détectées, utilisable depuis du code C, C++, ou via FFI.
     *
     * @note
     *   - Toutes les fonctions retournent des pointeurs vers données statiques
     *   - Ne pas libérer les chaînes ou structures retournées
     *   - Thread-safe : initialisation lazy avec atomic guard
     *
     * @example (C)
     * @code
     * #include "NKCore/NkPlatform.h"
     *
     * void PrintSystemInfo() {
     *     const NkPlatformInfo* info = NkGetPlatformInfo();
     *
     *     printf("Running on %s %s\n", info->osName, info->osVersion);
     *     printf("CPU: %s (%u cores)\n", info->archName, info->cpuCoreCount);
     *
     *     if (NkHasSIMDFeature("AVX2")) {
     *         printf("AVX2 instructions available\n");
     *     }
     * }
     * @endcode
     */
    /** @{ */

    // Alias C pour la structure C++ (interopérabilité)
    typedef nkentseu::platform::NkPlatformInfo NkPlatformInfo;

    // Début des déclarations C (name mangling désactivé)
    NKENTSEU_EXTERN_C_BEGIN

    /**
     * @brief Récupère les informations complètes de la plateforme
     * @return Pointeur const vers NkPlatformInfo initialisé
     * @note
     *   - Thread-safe : initialisation lazy au premier appel
     *   - Les données pointées sont statiques : durée de vie du programme
     *   - Ne pas libérer le pointeur retourné
     * @ingroup PlatformAPI
     */
    NKENTSEU_CORE_C_API const nkentseu::platform::NkPlatformInfo* NkGetPlatformInfo();

    /**
     * @brief Initialise explicitement les informations plateforme
     * @note
     *   - Appelée automatiquement par NkGetPlatformInfo()
     *   - Peut être appelée manuellement pour forcer l'initialisation early
     *   - Idempotent : appel multiple sans effet secondaire
     * @ingroup PlatformAPI
     */
    NKENTSEU_CORE_C_API void NkInitializePlatformInfo();

    /**
     * @brief Récupère le nom lisible de la plateforme
     * @return Chaîne statique : "Windows", "Linux", "macOS", etc.
     * @note Ne pas libérer la chaîne retournée
     * @ingroup PlatformAPI
     */
    NKENTSEU_CORE_C_API const nkentseu::nk_char* NkGetPlatformName();

    /**
     * @brief Récupère le nom lisible de l'architecture CPU
     * @return Chaîne statique : "x86_64", "ARM64", "RISC-V", etc.
     * @note Ne pas libérer la chaîne retournée
     * @ingroup PlatformAPI
     */
    NKENTSEU_CORE_C_API const nkentseu::nk_char* NkGetArchitectureName();

    /**
     * @brief Vérifie le support d'une extension SIMD spécifique
     * @param feature Nom de la feature : "SSE", "AVX2", "NEON", etc.
     * @return true si supportée sur la plateforme courante, false sinon
     * @note
     *   - Comparaison case-sensitive avec strcmp
     *   - Retourne false si feature est nullptr ou inconnue
     * @ingroup PlatformAPI
     */
    NKENTSEU_CORE_C_API nkentseu::nk_bool NkHasSIMDFeature(const nkentseu::nk_char *feature);

    // --------------------------------------------------------
    // Fonctions d'information CPU
    // --------------------------------------------------------

    /**
     * @brief Récupère le nombre de cœurs CPU physiques
     * @return Nombre de cœurs physiques (sans hyperthreading)
     * @note
     *   - Sur Windows : utilise GetSystemInfo()
     *   - Sur POSIX : utilise sysconf(_SC_NPROCESSORS_ONLN)
     * @ingroup PlatformAPI
     */
    NKENTSEU_CORE_C_API nkentseu::nk_uint32 NkGetCPUCoreCount();

    /**
     * @brief Récupère le nombre total de threads matériels
     * @return Nombre de threads logiques (incluant hyperthreading/SMT)
     * @note
     *   - Peut être > cpuCoreCount si SMT/hyperthreading activé
     *   - Utilisé pour dimensionner les thread pools
     * @ingroup PlatformAPI
     */
    NKENTSEU_CORE_C_API nkentseu::nk_uint32 NkGetCPUThreadCount();

    /**
     * @brief Récupère la taille du cache L1
     * @return Taille en bytes (typiquement 32KB par cœur)
     * @note Peut être la somme des caches L1 data+instruction selon la plateforme
     * @ingroup PlatformAPI
     */
    NKENTSEU_CORE_C_API nkentseu::nk_uint32 NkGetL1CacheSize();

    /**
     * @brief Récupère la taille du cache L2
     * @return Taille en bytes (typiquement 256KB-1MB par cœur ou partagé)
     * @ingroup PlatformAPI
     */
    NKENTSEU_CORE_C_API nkentseu::nk_uint32 NkGetL2CacheSize();

    /**
     * @brief Récupère la taille du cache L3
     * @return Taille en bytes (typiquement 8-32MB partagé)
     * @note Peut être 0 si pas de cache L3 présent
     * @ingroup PlatformAPI
     */
    NKENTSEU_CORE_C_API nkentseu::nk_uint32 NkGetL3CacheSize();

    /**
     * @brief Récupère la taille de ligne de cache
     * @return Taille en bytes (typiquement 64 sur x86/x64 modernes)
     * @note Important pour l'alignement des structures de données critiques
     * @ingroup PlatformAPI
     */
    NKENTSEU_CORE_C_API nkentseu::nk_uint32 NkGetCacheLineSize();

    // --------------------------------------------------------
    // Fonctions d'information Mémoire
    // --------------------------------------------------------

    /**
     * @brief Récupère la RAM totale installée
     * @return Taille en bytes
     * @note
     *   - Sur Windows : GlobalMemoryStatusEx()
     *   - Sur Linux : /proc/meminfo ou sysinfo()
     *   - Sur macOS : sysctlbyname("hw.memsize")
     * @ingroup PlatformAPI
     */
    NKENTSEU_CORE_C_API nkentseu::nk_uint64 NkGetTotalMemory();

    /**
     * @brief Récupère la RAM disponible (non-allouée)
     * @return Taille en bytes au moment de l'appel
     * @note
     *   - Valeur snapshot : peut changer immédiatement après
     *   - Ne pas utiliser pour décisions d'allocation critiques sans marge de sécurité
     * @ingroup PlatformAPI
     */
    NKENTSEU_CORE_C_API nkentseu::nk_uint64 NkGetAvailableMemory();

    /**
     * @brief Récupère la taille de page mémoire système
     * @return Taille en bytes (typiquement 4096)
     * @note
     *   - Utilisé pour l'alignement des allocations mémoire
     *   - Peut varier : 4KB, 16KB (ARM), 64KB (certains systèmes)
     * @ingroup PlatformAPI
     */
    NKENTSEU_CORE_C_API nkentseu::nk_uint32 NkGetPageSize();

    /**
     * @brief Récupère la granularité d'allocation mémoire virtuelle
     * @return Taille en bytes
     * @note
     *   - Sur Windows : souvent 64KB (dwAllocationGranularity)
     *   - Sur POSIX : généralement égal à pageSize
     * @ingroup PlatformAPI
     */
    NKENTSEU_CORE_C_API nkentseu::nk_uint32 NkGetAllocationGranularity();

    // --------------------------------------------------------
    // Fonctions d'information Build et Configuration
    // --------------------------------------------------------

    /**
     * @brief Vérifie si le build courant est en mode Debug
     * @return true si NKENTSEU_DEBUG défini, false sinon
     * @note Déterminé à la compilation, ne change pas au runtime
     * @ingroup PlatformAPI
     */
    NKENTSEU_CORE_C_API nkentseu::nk_bool NkIsDebugBuild();

    /**
     * @brief Vérifie si le code est compilé en bibliothèque partagée
     * @return true si NKENTSEU_SHARED_BUILD défini, false pour statique
     * @note Affecte la visibilité des symboles et le linking
     * @ingroup PlatformAPI
     */
    NKENTSEU_CORE_C_API nkentseu::nk_bool NkIsSharedLibrary();

    /**
     * @brief Récupère le type de build en chaîne lisible
     * @return "Debug", "Release", "RelWithDebInfo", ou "Unknown"
     * @note Utile pour le logging et le diagnostic de configuration
     * @ingroup PlatformAPI
     */
    NKENTSEU_CORE_C_API const nkentseu::nk_char* NkGetBuildType();

    // --------------------------------------------------------
    // Fonctions d'information Endianness et Architecture
    // --------------------------------------------------------

    /**
     * @brief Détecte l'endianness du système au runtime
     * @return NkEndianness::NK_LITTLE ou NkEndianness::NK_BIG
     * @note
     *   - Détection via union byte-order test
     *   - Mis en cache après premier appel pour performance
     * @ingroup PlatformAPI
     */
    NKENTSEU_CORE_C_API nkentseu::platform::NkEndianness NkGetEndianness();

    /**
     * @brief Vérifie si l'architecture est 64-bit
     * @return true si NKENTSEU_ARCH_64BIT défini, false pour 32-bit
     * @note Déterminé à la compilation via détection d'architecture
     * @ingroup PlatformAPI
     */
    NKENTSEU_CORE_C_API nkentseu::nk_bool NkIs64Bit();

    // --------------------------------------------------------
    // Fonctions Utilitaires Diverses
    // --------------------------------------------------------

    /**
     * @brief Affiche les informations plateforme sur stdout
     * @note
     *   - Format lisible pour diagnostic manuel
     *   - Utilise NK_FOUNDATION_LOG_INFO si disponible, sinon printf
     * @ingroup PlatformAPI
     */
    NKENTSEU_CORE_C_API void NkPrintPlatformInfo();

    /**
     * @brief Vérifie si une adresse mémoire est alignée
     * @param address Adresse à vérifier (nk_ptr = void*)
     * @param alignment Alignement requis (doit être puissance de 2)
     * @return true si (address % alignment) == 0, false sinon
     * @note
     *   - Retourne true si alignment == 0 (cas dégénéré)
     *   - Utilisé pour valider les préconditions d'allocations alignées
     * @ingroup PlatformAPI
     */
    NKENTSEU_CORE_C_API nkentseu::nk_bool NkIsAligned(const nkentseu::nk_ptr address,
                                                          nkentseu::nk_size alignment);

    /**
     * @brief Aligne une adresse vers le haut à la prochaine borne d'alignement
     * @param address Adresse à aligner
     * @param alignment Alignement requis (puissance de 2)
     * @return Adresse alignée >= address
     * @note
     *   - Formule : (addr + align - 1) & ~(align - 1)
     *   - Retourne address inchangée si alignment == 0 ou déjà alignée
     * @ingroup PlatformAPI
     */
    NKENTSEU_CORE_C_API nkentseu::nk_ptr NkAlignAddress(nkentseu::nk_ptr address,
                                                          nkentseu::nk_size alignment);

    /**
     * @brief Aligne une adresse const vers le haut (version const-correct)
     * @param address Adresse const à aligner
     * @param alignment Alignement requis (puissance de 2)
     * @return Adresse const alignée >= address
     * @note Équivalent à NkAlignAddress mais préserve la constance
     * @ingroup PlatformAPI
     */
    NKENTSEU_CORE_C_API const nkentseu::nk_ptr NkAlignAddressConst(const nkentseu::nk_ptr address,
                                                                     nkentseu::nk_size alignment);

    // Fin des déclarations C
    NKENTSEU_EXTERN_C_END

    /** @} */ // End of PlatformAPI

    // -------------------------------------------------------------------------
    // SECTION 8 : NAMESPACE NKENTSEU::PLATFORM - FONCTIONS INLINE
    // -------------------------------------------------------------------------
    // Fonctions compile-time pour détection rapide sans overhead runtime.

    namespace nkentseu {

        // Indentation niveau 1 : namespace platform
        namespace platform {

            /**
             * @defgroup PlatformInlineUtils Utilitaires Inline Plateforme
             * @brief Fonctions inline constexpr pour détection compile-time
             * @ingroup PlatformCore
             *
             * Ces fonctions fournissent un accès zéro-overhead aux informations
             * de plateforme déterminées à la compilation, éliminées par l'optimiseur
             * quand le résultat est constant.
             *
             * @note
             *   - Toutes les fonctions sont inline pour éviter les multiples définitions
             *   - Utilisent des macros de détection (NKENTSEU_PLATFORM_*) définies ailleurs
             *   - Peuvent être utilisées dans des expressions constexpr (C++11+)
             *
             * @example
             * @code
             * // Sélection compile-time d'un algorithme
             * template <typename T>
             * T Compute(T value) {
             *     if constexpr (nkentseu::platform::NkIsDesktop()) {
             *         return DesktopOptimizedCompute(value);
             *     } else {
             *         return MobileOptimizedCompute(value);
             *     }
             * }
             * @endcode
             */
            /** @{ */

            /**
             * @brief Retourne le nom de la plateforme (compile-time)
             * @return Chaîne statique : valeur de NKENTSEU_PLATFORM_NAME
             * @note Ne pas libérer la chaîne retournée
             * @ingroup PlatformInlineUtils
             */
            inline const nk_char *NkGetPlatformName() noexcept {
                return NKENTSEU_PLATFORM_NAME;
            }

            /**
             * @brief Retourne la version/description de la plateforme
             * @return Chaîne statique : valeur de NKENTSEU_PLATFORM_VERSION
             * @note Peut inclure numéro de version ou nom de distribution
             * @ingroup PlatformInlineUtils
             */
            inline const nk_char *NkGetPlatformVersion() noexcept {
                return NKENTSEU_PLATFORM_VERSION;
            }

            /**
             * @brief Vérifie si la plateforme est de type desktop
             * @return true si NKENTSEU_PLATFORM_DESKTOP défini, false sinon
             * @note
             *   - Desktop = Windows, Linux, macOS, BSD
             *   - Exclut mobile, console, embarqué, web
             * @ingroup PlatformInlineUtils
             */
            inline nk_bool NkIsDesktop() noexcept {
                #ifdef NKENTSEU_PLATFORM_DESKTOP
                    return true;
                #else
                    return false;
                #endif
            }

            /**
             * @brief Vérifie si la plateforme est de type mobile
             * @return true si NKENTSEU_PLATFORM_MOBILE défini, false sinon
             * @note
             *   - Mobile = iOS, Android, HarmonyOS
             *   - Exclut desktop, console, embarqué
             * @ingroup PlatformInlineUtils
             */
            inline nk_bool NkIsMobile() noexcept {
                #ifdef NKENTSEU_PLATFORM_MOBILE
                    return true;
                #else
                    return false;
                #endif
            }

            /**
             * @brief Vérifie si la plateforme est une console de jeu
             * @return true si NKENTSEU_PLATFORM_CONSOLE défini, false sinon
             * @note
             *   - Console = Switch, PS4/5, Xbox, etc.
             *   - Souvent combiné avec restrictions API spécifiques
             * @ingroup PlatformInlineUtils
             */
            inline nk_bool NkIsConsole() noexcept {
                #ifdef NKENTSEU_PLATFORM_CONSOLE
                    return true;
                #else
                    return false;
                #endif
            }

            /**
             * @brief Vérifie si la plateforme est embarquée/contrainte
             * @return true si NKENTSEU_PLATFORM_EMBEDDED défini, false sinon
             * @note
             *   - Embedded = ressources limitées (mémoire, CPU, pas de MMU)
             *   - Peut nécessiter des fallbacks algorithmiques
             * @ingroup PlatformInlineUtils
             */
            inline nk_bool NkIsEmbedded() noexcept {
                #ifdef NKENTSEU_PLATFORM_EMBEDDED
                    return true;
                #else
                    return false;
                #endif
            }

            /**
             * @brief Vérifie si la plateforme est web/WebAssembly
             * @return true si NKENTSEU_PLATFORM_EMSCRIPTEN défini, false sinon
             * @note
             *   - Web = exécution dans un navigateur via Emscripten
             *   - Restrictions : pas de threads natifs, mémoire limitée, etc.
             * @ingroup PlatformInlineUtils
             */
            inline nk_bool NkIsWeb() noexcept {
                #ifdef NKENTSEU_PLATFORM_EMSCRIPTEN
                    return true;
                #else
                    return false;
                #endif
            }

            // ============================================================
            // NAMESPACE ARCH - UTILITAIRES ARCHITECTURE CPU
            // ============================================================

            /**
             * @namespace arch
             * @brief Fonctions utilitaires spécifiques à l'architecture CPU
             * @ingroup PlatformInlineUtils
             *
             * Regroupe les helpers pour manipulation d'adresses, alignement,
             * et métadonnées CPU déterminées à la compilation.
             */
            namespace arch {

                /**
                 * @brief Retourne le nom de l'architecture CPU
                 * @return Chaîne statique : valeur de NKENTSEU_ARCH_NAME
                 * @note Ex: "x86_64", "ARM64", "RISC-V", "WASM"
                 * @ingroup PlatformInlineUtils
                 */
                inline const nk_char *NkGetArchName() noexcept {
                    return NKENTSEU_ARCH_NAME;
                }

                /**
                 * @brief Retourne la version/description de l'architecture
                 * @return Chaîne statique : valeur de NKENTSEU_ARCH_VERSION
                 * @note Peut inclure niveau d'instruction : "ARMv8-A+NEON", etc.
                 * @ingroup PlatformInlineUtils
                 */
                inline const nk_char *NkGetArchVersion() noexcept {
                    return NKENTSEU_ARCH_VERSION;
                }

                /**
                 * @brief Vérifie si l'architecture est 64-bit
                 * @return true si NKENTSEU_ARCH_64BIT défini, false sinon
                 * @note Déterminé à la compilation via détection d'architecture
                 * @ingroup PlatformInlineUtils
                 */
                inline nk_bool NkIs64Bit() noexcept {
                    #ifdef NKENTSEU_ARCH_64BIT
                        return true;
                    #else
                        return false;
                    #endif
                }

                /**
                 * @brief Vérifie si l'architecture est 32-bit
                 * @return true si NKENTSEU_ARCH_32BIT défini, false sinon
                 * @note Mutuellement exclusif avec NkIs64Bit()
                 * @ingroup PlatformInlineUtils
                 */
                inline nk_bool NkIs32Bit() noexcept {
                    #ifdef NKENTSEU_ARCH_32BIT
                        return true;
                    #else
                        return false;
                    #endif
                }

                /**
                 * @brief Vérifie si l'architecture est little-endian
                 * @return true si NKENTSEU_ARCH_LITTLE_ENDIAN défini, false sinon
                 * @note
                 *   - Little-endian = byte de poids faible à l'adresse basse
                 *   - x86/x64, ARM (typique), RISC-V : little-endian
                 *   - Certains PowerPC, SPARC : big-endian
                 * @ingroup PlatformInlineUtils
                 */
                inline nk_bool NkIsLittleEndian() noexcept {
                    #ifdef NKENTSEU_ARCH_LITTLE_ENDIAN
                        return true;
                    #else
                        return false;
                    #endif
                }

                /**
                 * @brief Vérifie si l'architecture est big-endian
                 * @return true si NKENTSEU_ARCH_BIG_ENDIAN défini, false sinon
                 * @note Mutuellement exclusif avec NkIsLittleEndian()
                 * @ingroup PlatformInlineUtils
                 */
                inline nk_bool NkIsBigEndian() noexcept {
                    #ifdef NKENTSEU_ARCH_BIG_ENDIAN
                        return true;
                    #else
                        return false;
                    #endif
                }

                /**
                 * @brief Retourne la taille de ligne de cache
                 * @return Taille en bytes (valeur de NKENTSEU_CACHE_LINE_SIZE)
                 * @note Typiquement 64 bytes sur x86/x64 modernes
                 * @ingroup PlatformInlineUtils
                 */
                inline nk_uint32 NkGetCacheLineSize() noexcept {
                    return NKENTSEU_CACHE_LINE_SIZE;
                }

                /**
                 * @brief Retourne la taille de page mémoire
                 * @return Taille en bytes (valeur de NKENTSEU_PAGE_SIZE)
                 * @note Typiquement 4096 (4KB), peut être 16KB/64KB sur ARM
                 * @ingroup PlatformInlineUtils
                 */
                inline nk_uint32 NkGetPageSize() noexcept {
                    return NKENTSEU_PAGE_SIZE;
                }

                /**
                 * @brief Retourne la taille de mot machine
                 * @return Taille en bytes : 4 pour 32-bit, 8 pour 64-bit
                 * @note Équivalent à sizeof(void*) ou sizeof(nk_ptr)
                 * @ingroup PlatformInlineUtils
                 */
                inline nk_uint32 NkGetWordSize() noexcept {
                    return NKENTSEU_WORD_SIZE;
                }

                /**
                 * @brief Aligne un pointeur vers le haut à la prochaine borne d'alignement
                 * @tparam T Type du pointeur (déduit automatiquement)
                 * @param addr Adresse à aligner
                 * @param align Alignement requis (doit être puissance de 2)
                 * @return Pointeur aligné >= addr
                 * @note
                 *   - Formule : (addr + align - 1) & ~(align - 1)
                 *   - constexpr-friendly : peut être évalué à la compilation si arguments constexpr
                 * @ingroup PlatformInlineUtils
                 */
                template <typename T>
                inline T *NkAlignUp(T *addr, nk_size align) noexcept {
                    return reinterpret_cast<T *>(
                        (reinterpret_cast<nk_uintptr>(addr) + (align - 1)) & ~(align - 1)
                    );
                }

                /**
                 * @brief Aligne un pointeur vers le bas à la borne d'alignement inférieure
                 * @tparam T Type du pointeur (déduit automatiquement)
                 * @param addr Adresse à aligner
                 * @param align Alignement requis (doit être puissance de 2)
                 * @return Pointeur aligné <= addr
                 * @note
                 *   - Formule : addr & ~(align - 1)
                 *   - Utile pour trouver le début d'un bloc aligné contenant addr
                 * @ingroup PlatformInlineUtils
                 */
                template <typename T>
                inline T *NkAlignDown(T *addr, nk_size align) noexcept {
                    return reinterpret_cast<T *>(
                        reinterpret_cast<nk_uintptr>(addr) & ~(align - 1)
                    );
                }

                /**
                 * @brief Vérifie si un pointeur est aligné sur une borne donnée
                 * @tparam T Type du pointeur (déduit automatiquement)
                 * @param addr Adresse à vérifier
                 * @param align Alignement requis (doit être puissance de 2)
                 * @return true si (addr % align) == 0, false sinon
                 * @note
                 *   - Retourne true si align == 0 (cas dégénéré)
                 *   - Utilisé pour valider les préconditions d'API alignées
                 * @ingroup PlatformInlineUtils
                 */
                template <typename T>
                inline nk_bool NkIsAligned(const T *addr, nk_size align) noexcept {
                    return (reinterpret_cast<nk_uintptr>(addr) & (align - 1)) == 0;
                }

                /**
                 * @brief Calcule le padding nécessaire pour atteindre l'alignement
                 * @tparam T Type du pointeur (déduit automatiquement)
                 * @param addr Adresse de référence
                 * @param align Alignement cible (puissance de 2)
                 * @return Nombre de bytes à ajouter pour atteindre l'alignement
                 * @note
                 *   - Retourne 0 si addr est déjà alignée
                 *   - Formule : (align - (addr % align)) % align
                 * @ingroup PlatformInlineUtils
                 */
                template <typename T>
                inline nk_size NkCalculatePadding(const T *addr, nk_size align) noexcept {
                    nk_uintptr mask = align - 1;
                    nk_uintptr misalignment = reinterpret_cast<nk_uintptr>(addr) & mask;
                    return misalignment ? (align - misalignment) : 0;
                }

            } // namespace arch

            // ============================================================
            // NAMESPACE MEMORY - ALLOCATION MÉMOIRE ALIGNÉE
            // ============================================================

            /**
             * @namespace memory
             * @brief Fonctions pour allocation mémoire avec alignement contrôlé
             * @ingroup PlatformUtils
             *
             * Fournit des primitives pour allouer/libérer de la mémoire
             * avec un alignement garanti, essentiel pour :
             *   - Instructions SIMD (requièrent alignement 16/32/64 bytes)
             *   - DMA et opérations hardware directes
             *   - Optimisation de cache (évite le false sharing)
             */
            namespace memory {

                /**
                 * @brief Alloue de la mémoire avec alignement garanti
                 * @param size Taille en bytes à allouer
                 * @param alignment Alignement requis (doit être puissance de 2)
                 * @return Pointeur vers mémoire alignée, nullptr en cas d'échec
                 * @note
                 *   - Utilise _aligned_malloc (Windows), posix_memalign (POSIX), ou fallback portable
                 *   - Le pointeur retourné doit être libéré avec NkFreeAligned()
                 *   - alignment doit être >= sizeof(void*) pour portabilité
                 * @ingroup MemoryAllocation
                 */
                NKENTSEU_CORE_API nk_ptr NkAllocateAligned(nk_size size, nk_size alignment) noexcept;

                /**
                 * @brief Libère de la mémoire allouée avec NkAllocateAligned
                 * @param ptr Pointeur à libérer (doit provenir de NkAllocateAligned)
                 * @note
                 *   - Ne pas utiliser free() sur un pointeur aligné : comportement indéfini
                 *   - Accepte nullptr (no-op) pour compatibilité avec les patterns de cleanup
                 * @ingroup MemoryAllocation
                 */
                NKENTSEU_CORE_API void NkFreeAligned(nk_ptr ptr) noexcept;

                /**
                 * @brief Vérifie si un pointeur est aligné sur une borne donnée
                 * @param ptr Pointeur à vérifier
                 * @param alignment Alignement à tester (puissance de 2)
                 * @return true si aligné, false sinon
                 * @note Utile pour valider les préconditions avant opérations SIMD
                 * @ingroup MemoryAllocation
                 */
                NKENTSEU_CORE_API nk_bool NkIsPointerAligned(const nk_ptr ptr, nk_size alignment) noexcept;

                /**
                 * @brief Alloue un tableau d'éléments avec alignement garanti
                 * @tparam T Type des éléments du tableau
                 * @param count Nombre d'éléments à allouer
                 * @param alignment Alignement requis (puissance de 2)
                 * @return Pointeur vers le tableau aligné, nullptr en cas d'échec
                 * @note
                 *   - Équivalent à NkAllocateAligned(count * sizeof(T), alignment) avec cast
                 *   - Les éléments ne sont pas initialisés : utiliser placement-new si nécessaire
                 * @ingroup MemoryAllocation
                 */
                template <typename T>
                inline T *NkAllocateAlignedArray(nk_size count, nk_size alignment) noexcept {
                    nk_size size = count * sizeof(T);
                    nk_ptr ptr = NkAllocateAligned(size, alignment);
                    return static_cast<T *>(ptr);
                }

                /**
                 * @brief Construit un objet dans de la mémoire pré-allouée et alignée
                 * @tparam T Type de l'objet à construire
                 * @tparam Args Types des arguments du constructeur (forwarded)
                 * @param ptr Pointeur vers mémoire alignée et suffisamment grande pour T
                 * @param args Arguments à forwarder au constructeur de T
                 * @return Pointeur vers l'objet nouvellement construit
                 * @note
                 *   - Utilise placement new : n'alloue pas de mémoire supplémentaire
                 *   - La mémoire doit être alignée selon les exigences de T (alignof(T))
                 *   - L'objet doit être détruit explicitement via NkDestroyAligned() avant libération
                 * @ingroup MemoryAllocation
                 */
                template <typename T, typename... Args>
                inline T *NkConstructAligned(nk_ptr ptr, Args &&...args) noexcept {
                    return new (ptr) T(static_cast<Args &&>(args)...);
                }

                /**
                 * @brief Détruit un objet construit via NkConstructAligned
                 * @tparam T Type de l'objet à détruire
                 * @param ptr Pointeur vers l'objet à détruire (peut être nullptr)
                 * @note
                 *   - Appelle explicitement le destructeur ~T()
                 *   - Ne libère pas la mémoire : utiliser NkFreeAligned() ensuite si nécessaire
                 *   - Accepte nullptr (no-op) pour patterns de cleanup sécurisés
                 * @ingroup MemoryAllocation
                 */
                template <typename T>
                inline void NkDestroyAligned(T *ptr) noexcept {
                    if (ptr) {
                        ptr->~T();
                    }
                }

            } // namespace memory

        } // namespace platform

    } // namespace nkentseu

    /** @} */ // End of PlatformInlineUtils

    // -------------------------------------------------------------------------
    // SECTION 9 : MACROS DE CONVENANCE
    // -------------------------------------------------------------------------
    // Helpers pour usage courant avec valeurs par défaut du framework.

    /**
     * @defgroup PlatformConvenienceMacros Macros de Convenance Plateforme
     * @brief Macros utilitaires pour patterns d'allocation courants
     * @ingroup PlatformUtilities
     *
     * Simplifie l'usage des fonctions d'allocation alignée avec
     * des valeurs par défaut adaptées au framework.
     *
     * @example
     * @code
     * // Allouer un buffer aligné sur la taille de page pour DMA
     * void* dmaBuffer = NkAllocPageAligned(4096);
     * if (dmaBuffer) {
     *     // ... utilisation ...
     *     nkentseu::platform::memory::NkFreeAligned(dmaBuffer);
     * }
     * @endcode
     */
    /** @{ */

    /**
     * @brief Alloue de la mémoire alignée sur la taille de page système
     * @param size Taille en bytes à allouer
     * @return Pointeur vers mémoire alignée sur NKENTSEU_PAGE_SIZE, ou nullptr
     * @note
     *   - Équivalent à nkentseu::platform::memory::NkAllocateAligned(size, NKENTSEU_PAGE_SIZE)
     *   - Utile pour les buffers DMA, les mappings mémoire, ou l'optimisation de TLB
     * @ingroup PlatformConvenienceMacros
     */
    #define NkAllocPageAligned(size) \
        nkentseu::platform::memory::NkAllocateAligned((size), NKENTSEU_PAGE_SIZE)

    /** @} */ // End of PlatformConvenienceMacros

#endif // NKENTSEU_CORE_NKPLATFORM_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION DE NKPLATFORM.H
// =============================================================================
// Ce fichier fournit les primitives pour interrogation et adaptation à
// l'environnement d'exécution multi-plateforme.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Sélection d'algorithme selon les capacités SIMD
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkPlatform.h"
    #include "NKCore/NkMath.h"

    // Implémentations multiples d'une fonction de produit scalaire
    float DotProduct_Scalar(const float* a, const float* b, nk_size count);
    float DotProduct_SSE4(const float* a, const float* b, nk_size count);
    float DotProduct_AVX2(const float* a, const float* b, nk_size count);
    float DotProduct_NEON(const float* a, const float* b, nk_size count);

    // Dispatcher automatique au premier appel
    typedef float (*DotProductFn)(const float*, const float*, nk_size);
    static DotProductFn sDotProductImpl = nullptr;

    void InitializeDotProduct() {
        const auto* info = nkentseu::platform::NkGetPlatformInfo();

        // Sélection par ordre de préférence (plus optimisé en premier)
        if (info->hasAVX2) {
            sDotProductImpl = DotProduct_AVX2;
        } else if (info->hasSSE4_2) {
            sDotProductImpl = DotProduct_SSE4;
        } else if (info->hasNEON) {
            sDotProductImpl = DotProduct_NEON;
        } else {
            sDotProductImpl = DotProduct_Scalar;  // Fallback universel
        }
    }

    float DotProduct(const float* a, const float* b, nk_size count) {
        if (!sDotProductImpl) {
            InitializeDotProduct();  // Initialisation lazy thread-safe
        }
        return sDotProductImpl(a, b, count);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Allocation mémoire alignée pour SIMD
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkPlatform.h"
    #include <immintrin.h>  // Pour __m256 (AVX)

    // Structure nécessitant un alignement 32 bytes pour AVX
    struct alignas(32) SimdBuffer {
        float data[8];  // 8 floats = 32 bytes = 1 registre AVX

        // Constructeur avec allocation alignée
        static SimdBuffer* Create() {
            void* ptr = nkentseu::platform::memory::NkAllocateAligned(
                sizeof(SimdBuffer),
                alignof(SimdBuffer)  // 32 bytes
            );
            if (!ptr) return nullptr;

            // Construction in-place avec placement new
            return nkentseu::platform::memory::NkConstructAligned<SimdBuffer>(ptr);
        }

        // Destruction et libération
        void Destroy() {
            nkentseu::platform::memory::NkDestroyAligned(this);
            nkentseu::platform::memory::NkFreeAligned(this);
        }

        // Opération AVX exemple
        void LoadAndAdd(const float* source) {
            #if defined(__AVX__)
            __m256 a = _mm256_load_ps(data);      // Requiert alignement 32
            __m256 b = _mm256_loadu_ps(source);   // unaligned load si nécessaire
            __m256 result = _mm256_add_ps(a, b);
            _mm256_store_ps(data, result);         // Requiert alignement 32
            #else
            // Fallback scalaire si AVX non disponible
            for (int i = 0; i < 8; ++i) {
                data[i] += source[i];
            }
            #endif
        }
    };

    // Usage
    void Example2() {
        SimdBuffer* buffer = SimdBuffer::Create();
        if (buffer) {
            float input[8] = {1,2,3,4,5,6,7,8};
            buffer->LoadAndAdd(input);
            buffer->Destroy();  // Détruit + libère en une étape
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Logging avec localisation source automatique
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkPlatform.h"
    #include "NKCore/NkFoundationLog.h"

    // Macro de logging avec contexte source automatique
    #define NK_LOG_WITH_LOCATION(level, msg, ...) \
    do { \
        auto loc = NkCurrentSourceLocation; \
        NK_FOUNDATION_LOG_##level( \
            "[%s:%d] %s: " msg, \
            loc.FileName(), \
            loc.Line(), \
            loc.FunctionName(), \
            ##__VA_ARGS__ \
        ); \
    } while(0)

    // Usage dans du code applicatif
    bool ConnectToServer(const char* host, int port) {
        NK_LOG_WITH_LOCATION(INFO, "Connecting to %s:%d", host, port);

        if (!host || host[0] == '\0') {
            NK_LOG_WITH_LOCATION(ERROR, "Invalid host parameter");
            return false;
        }

        // ... logique de connexion ...

        NK_LOG_WITH_LOCATION(DEBUG, "Connection established");
        return true;
    }

    // Output typique :
    // [network.cpp:42] ConnectToServer: Connecting to example.com:443
    // [network.cpp:45] ConnectToServer: Connection established
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Adaptation comportement selon type de plateforme
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkPlatform.h"
    #include "NKCore/NkConfig.h"  // Pour NKENTSEU_IS_DEBUG, etc.

    class ResourceLoader {
    public:
        // Chargement adaptatif selon la plateforme
        bool LoadResource(const char* path) {
            // Stratégie différente selon le type de plateforme
            if (nkentseu::platform::NkIsMobile()) {
                // Mobile : chargement asynchrone pour ne pas bloquer l'UI
                return LoadResourceAsync(path);
            } else if (nkentseu::platform::NkIsEmbedded()) {
                // Embarqué : chargement minimaliste avec fallback
                return LoadResourceMinimal(path);
            } else {
                // Desktop/console : chargement synchrone avec cache
                return LoadResourceCached(path);
            }
        }

        // Ajustement de la qualité selon la mémoire disponible
        void AdjustQualitySettings() {
            const auto* info = nkentseu::platform::NkGetPlatformInfo();

            // Seuil de mémoire pour qualité haute
            constexpr nk_uint64 HIGH_QUALITY_THRESHOLD = 4ULL * 1024 * 1024 * 1024; // 4 GB

            if (info->availableMemory >= HIGH_QUALITY_THRESHOLD) {
                SetQualityPreset(HIGH);
                EnableAdvancedEffects();
            } else if (info->availableMemory >= HIGH_QUALITY_THRESHOLD / 4) {
                SetQualityPreset(MEDIUM);
                DisableRayTracing();
            } else {
                SetQualityPreset(LOW);
                DisablePostProcessing();
            }

            // Ajustement supplémentaire en mode debug pour tests
            if (NKENTSEU_IS_DEBUG) {
                EnableDebugOverlays();
            }
        }

    private:
        bool LoadResourceAsync(const char* path);
        bool LoadResourceMinimal(const char* path);
        bool LoadResourceCached(const char* path);
        void SetQualityPreset(QualityLevel level);
        // ... autres membres ...
    };
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Diagnostic système pour rapport de bug
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkPlatform.h"
    #include <string>

    std::string GenerateSystemReport() {
        const auto* info = nkentseu::platform::NkGetPlatformInfo();
        std::string report;

        // En-tête
        report += "=== Nkentseu System Report ===\n";

        // Informations OS
        report += "OS: ";
        report += info->osName;
        if (info->osVersion) {
            report += " ";
            report += info->osVersion;
        }
        report += "\n";

        // Informations CPU
        report += "CPU: ";
        report += info->archName;
        report += " (";
        report += std::to_string(info->cpuCoreCount);
        report += " cores, ";
        report += std::to_string(info->cpuThreadCount);
        report += " threads)\n";

        // Cache
        report += "Cache: L1=";
        report += std::to_string(info->cpuL1CacheSize / 1024);
        report += "KB, L2=";
        report += std::to_string(info->cpuL2CacheSize / 1024);
        report += "KB, L3=";
        report += std::to_string(info->cpuL3CacheSize / (1024*1024));
        report += "MB\n";

        // Mémoire
        report += "Memory: ";
        report += std::to_string(info->totalMemory / (1024*1024));
        report += "MB total, ";
        report += std::to_string(info->availableMemory / (1024*1024));
        report += "MB available\n";

        // SIMD
        report += "SIMD:";
        if (info->hasSSE) report += " SSE";
        if (info->hasSSE2) report += " SSE2";
        if (info->hasSSE3) report += " SSE3";
        if (info->hasSSE4_1) report += " SSE4.1";
        if (info->hasSSE4_2) report += " SSE4.2";
        if (info->hasAVX) report += " AVX";
        if (info->hasAVX2) report += " AVX2";
        if (info->hasAVX512) report += " AVX-512";
        if (info->hasNEON) report += " NEON";
        report += "\n";

        // Build info
        report += "Build: ";
        report += info->buildType;
        report += info->isDebugBuild ? " (Debug)" : " (Release)";
        report += ", ";
        report += info->isSharedLibrary ? "Shared" : "Static";
        report += " library\n";

        // Footer
        report += "==============================\n";

        return report;
    }

    // Usage : attacher ce rapport aux tickets de bug
    void ReportCrash(const char* message) {
        std::string report = GenerateSystemReport();
        report += "\nError: ";
        report += message;

        // Envoyer au serveur de collecte de crash ou écrire dans un fichier
        SaveCrashReport(report);
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================