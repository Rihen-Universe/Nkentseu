// =============================================================================
// NKPlatform/NkCGXDetect.h
// Détection GPU, APIs graphiques et calcul (CUDA/OpenCL/Vulkan/Metal/etc.)
//
// Design :
//  - Détection automatique des APIs graphiques par plateforme (Windows/Linux/macOS/etc.)
//  - Énumérations typées pour NkGraphicsApi, NkGPUVendor, NkGPUType
//  - Macros conditionnelles pour compilation sélective (_ONLY / _NOT_)
//  - Détection des APIs de calcul GPU : CUDA, OpenCL, SYCL
//  - Détection des systèmes d'affichage : X11, XCB, Wayland
//  - Constantes Vendor ID PCI pour identification matérielle
//  - Configuration automatique de l'API graphique active avec fallback software
//  - Intégration avec NkPlatformDetect.h et NkArchDetect.h
//
// Intégration :
//  - Utilise NKPlatform/NkPlatformDetect.h pour détection OS
//  - Utilise NKPlatform/NkArchDetect.h pour détection architecture CPU
//  - Compatible avec NKCore/NkTypes.h pour les types fondamentaux
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_PLATFORM_NKCGXDETECT_H_INCLUDED
#define NKENTSEU_PLATFORM_NKCGXDETECT_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules de détection de plateforme et d'architecture.

    #include "NKPlatform/NkPlatformDetect.h"
    #include "NKPlatform/NkArchDetect.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : DÉTECTION DES APIS GRAPHIQUES PAR PLATEFORME
    // -------------------------------------------------------------------------
    // Définition conditionnelle des macros de disponibilité selon l'OS cible.

    /**
     * @defgroup NkGraphicsAPIDetection Détection API Graphique
     * @brief Macros pour détecter les APIs graphiques disponibles
     * @ingroup PlatformDetection
     *
     * Ces macros sont automatiquement définies selon la plateforme détectée
     * par NkPlatformDetect.h. Elles permettent de conditionner le code selon
     * les APIs graphiques disponibles sur la cible de compilation.
     *
     * @note
     *   - Utiliser les macros NKENTSEU_*_ONLY() pour inclure du code spécifique
     *   - Les macros de disponibilité sont définies en compilation, pas à l'exécution
     *   - Une plateforme peut supporter plusieurs APIs simultanément
     *
     * @example
     * @code
     * // Code compilé uniquement si Vulkan est disponible
     * NKENTSEU_VULKAN_ONLY(
     *     void InitVulkanRenderer() { /\* ... *\/ }
     * )
     *
     * // Sélection à l'exécution selon disponibilité
     * #if defined(NKENTSEU_GRAPHICS_VULKAN_AVAILABLE)
     *     if (userPrefersVulkan && CheckVulkanSupport()) {
     *         UseVulkanBackend();
     *     }
     * #endif
     * @endcode
     */
    /** @{ */

    // ============================================================
    // WINDOWS - Direct3D + Vulkan + OpenGL
    // ============================================================

    NKENTSEU_WINDOWS_ONLY(
        /** @brief Direct3D 11 disponible sur Windows */
        #define NKENTSEU_GRAPHICS_D3D11_AVAILABLE

        /** @brief Direct3D 12 disponible sur Windows 10+ */
        #define NKENTSEU_GRAPHICS_D3D12_AVAILABLE

        /** @brief OpenGL disponible sur Windows */
        #define NKENTSEU_GRAPHICS_OPENGL_AVAILABLE

        /** @brief Vulkan disponible sur Windows */
        #define NKENTSEU_GRAPHICS_VULKAN_AVAILABLE
    )

    // ============================================================
    // LINUX/BSD - Vulkan + OpenGL
    // ============================================================

    NKENTSEU_LINUX_ONLY(
        #define NKENTSEU_GRAPHICS_OPENGL_AVAILABLE
        #define NKENTSEU_GRAPHICS_VULKAN_AVAILABLE
    )

    NKENTSEU_FREEBSD_ONLY(
        #define NKENTSEU_GRAPHICS_OPENGL_AVAILABLE
        #define NKENTSEU_GRAPHICS_VULKAN_AVAILABLE
    )

    // ============================================================
    // MACOS/IOS - Metal + Vulkan (MoltenVK)
    // ============================================================

    NKENTSEU_MACOS_ONLY(
        /** @brief Metal disponible sur macOS */
        #define NKENTSEU_GRAPHICS_METAL_AVAILABLE

        /** @brief OpenGL disponible sur macOS (deprecated depuis 10.14) */
        #define NKENTSEU_GRAPHICS_OPENGL_AVAILABLE
        #define NKENTSEU_GRAPHICS_OPENGL_DEPRECATED

        /** @brief Vulkan disponible via MoltenVK sur macOS */
        #define NKENTSEU_GRAPHICS_VULKAN_AVAILABLE
        #define NKENTSEU_GRAPHICS_VULKAN_VIA_MOLTENVK
    )

    NKENTSEU_IOS_ONLY(
        #define NKENTSEU_GRAPHICS_METAL_AVAILABLE
        #define NKENTSEU_GRAPHICS_GLES3_AVAILABLE
        #define NKENTSEU_GRAPHICS_VULKAN_AVAILABLE
        #define NKENTSEU_GRAPHICS_VULKAN_VIA_MOLTENVK
    )

    NKENTSEU_TVOS_ONLY(
        #define NKENTSEU_GRAPHICS_METAL_AVAILABLE
    )

    // ============================================================
    // ANDROID - OpenGL ES + Vulkan
    // ============================================================

    NKENTSEU_ANDROID_ONLY(
        /** @brief OpenGL ES 3.x disponible sur Android */
        #define NKENTSEU_GRAPHICS_GLES3_AVAILABLE

        /** @brief Vulkan disponible sur Android (API level 24+) */
        #define NKENTSEU_GRAPHICS_VULKAN_AVAILABLE
    )

    // ============================================================
    // WEB (Emscripten) - WebGL + WebGPU
    // ============================================================

    NKENTSEU_EMSCRIPTEN_ONLY(
        /** @brief WebGL 2.0 disponible via Emscripten */
        #define NKENTSEU_GRAPHICS_WEBGL2_AVAILABLE

        /** @brief WebGPU disponible (support expérimental) */
        #define NKENTSEU_GRAPHICS_WEBGPU_AVAILABLE
    )

    // ============================================================
    // CONSOLES PLAYSTATION
    // ============================================================

    NKENTSEU_PS5_ONLY(
        /** @brief GNM (API Sony) disponible sur PS5 */
        #define NKENTSEU_GRAPHICS_GNM_AVAILABLE

        /** @brief Vulkan disponible sur PS5 */
        #define NKENTSEU_GRAPHICS_VULKAN_AVAILABLE
    )

    NKENTSEU_PS4_ONLY(
        #define NKENTSEU_GRAPHICS_GNM_AVAILABLE
    )

    // ============================================================
    // CONSOLES XBOX
    // ============================================================

    NKENTSEU_XBOX_SERIES_ONLY(
        /** @brief Direct3D 12 disponible sur Xbox Series X|S */
        #define NKENTSEU_GRAPHICS_D3D12_AVAILABLE
    )

    NKENTSEU_XBOXONE_ONLY(
        #define NKENTSEU_GRAPHICS_D3D11_AVAILABLE
        #define NKENTSEU_GRAPHICS_D3D12_AVAILABLE
    )

    // ============================================================
    // NINTENDO SWITCH
    // ============================================================

    NKENTSEU_SWITCH_ONLY(
        /** @brief NVN (API NVIDIA) disponible sur Nintendo Switch */
        #define NKENTSEU_GRAPHICS_NVN_AVAILABLE

        /** @brief Vulkan disponible sur Nintendo Switch */
        #define NKENTSEU_GRAPHICS_VULKAN_AVAILABLE
    )

    /** @} */ // End of NkGraphicsAPIDetection

    // -------------------------------------------------------------------------
    // SECTION 3 : ESPACE DE NOMS PRINCIPAL
    // -------------------------------------------------------------------------
    // Déclaration hiérarchique des namespaces pour organisation du code.

    namespace nkentseu {

        // Indentation niveau 1 : namespace platform
        namespace platform {

            // Indentation niveau 2 : namespace graphics
            namespace graphics {

                // ====================================================================
                // SECTION 4 : ÉNUMÉRATION DES APIS GRAPHIQUES
                // ====================================================================
                // Liste exhaustive des APIs graphiques supportées par le framework.

                /**
                 * @brief Types d'API graphiques supportées
                 * @enum NkGraphicsApi
                 * @ingroup GraphicsEnums
                 *
                 * Cette énumération liste toutes les APIs graphiques que le framework
                 * peut utiliser comme backend de rendu. Les valeurs sont stables et
                 * peuvent être sérialisées pour la configuration ou le logging.
                 *
                 * @note
                 *   - Les alias NK_API_* sont fournis pour compatibilité avec l'ancien code
                 *   - NK_GFX_API_MAX indique le nombre total d'APIs (pour itération)
                 *   - NK_GFX_API_SOFTWARE est le fallback universel si aucune API hardware n'est disponible
                 *
                 * @example
                 * @code
                 * // Vérification de disponibilité à l'exécution
                 * bool IsApiSupported(nkentseu::platform::graphics::NkGraphicsApi api) {
                 *     switch (api) {
                 *         #if defined(NKENTSEU_GRAPHICS_VULKAN_AVAILABLE)
                 *         case nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_VULKAN:
                 *             return CheckVulkanDrivers();
                 *         #endif
                 *         // ... autres cas
                 *         default: return false;
                 *     }
                 * }
                 * @endcode
                 */
                enum class NkGraphicsApi : unsigned int {
                    // Valeurs canoniques utilisées par les événements NKWindow
                    NK_GFX_API_NONE       = 0,    ///< Aucune API (état invalide ou non-initialisé)
                    NK_GFX_API_OPENGL,            ///< OpenGL 3.3+ (desktop cross-platform)
                    NK_GFX_API_OPENGLES,          ///< OpenGL ES 2.0 / 3.x (mobile/embarqué)
                    NK_GFX_API_VULKAN,            ///< Vulkan 1.0+ (moderne, cross-platform)
                    NK_GFX_API_D3D11,             ///< Direct3D 11 (Windows)
                    NK_GFX_API_D3D12,             ///< Direct3D 12 (Windows 10+, Xbox)
                    NK_GFX_API_METAL,             ///< Metal (macOS, iOS, tvOS)
                    NK_GFX_API_WEBGL,             ///< WebGL 1.0 (navigateur via Emscripten)
                    NK_GFX_API_WEBGL2,            ///< WebGL 2.0 (navigateur via Emscripten)
                    NK_GFX_API_WEBGPU,            ///< WebGPU (navigateur moderne, expérimental)
                    NK_GFX_API_SOFTWARE,          ///< Rasterisation logicielle (fallback universel)
                    NK_GFX_API_GNM,               ///< GNM (PlayStation 4/5, API propriétaire Sony)
                    NK_GFX_API_NVN,               ///< NVN (Nintendo Switch, API NVIDIA)
                    NK_GFX_API_MAX,               ///< Valeur sentinelle : nombre total d'APIs
                };

                // ====================================================================
                // SECTION 5 : ÉNUMÉRATION DES FABRICANTS DE GPU
                // ====================================================================
                // IDs PCI des vendors pour identification matérielle.

                /**
                 * @brief Fabricants de GPU identifiés par Vendor ID PCI
                 * @enum NkGPUVendor
                 * @ingroup GraphicsEnums
                 *
                 * Ces valeurs correspondent aux Vendor IDs assignés par le PCI-SIG.
                 * Utile pour l'identification du matériel, l'application de workarounds,
                 * ou l'affichage d'informations système.
                 *
                 * @note
                 *   - Les valeurs hexadécimales sont les IDs PCI officiels
                 *   - NK_UNKNOWN (0) est utilisé quand le vendor ne peut être déterminé
                 *   - Apple utilise son propre ID même pour les GPU intégrés M-series
                 *
                 * @example
                 * @code
                 * // Application de workaround pour un vendor spécifique
                 * void ApplyGPUWorkarounds(nkentseu::platform::graphics::NkGPUVendor vendor) {
                 *     using namespace nkentseu::platform::graphics;
                 *
                 *     if (vendor == NkGPUVendor::NK_NVIDIA) {
                 *         // Workaround pour les drivers NVIDIA anciens
                 *         EnableNvidiaSpecificFixes();
                 *     } else if (vendor == NkGPUVendor::NK_INTEL) {
                 *         // Optimisations pour GPU intégrés Intel
                 *         ReduceTextureResolution();
                 *     }
                 * }
                 * @endcode
                 */
                enum class NkGPUVendor : uint16_t {
                    NK_UNKNOWN    = 0x0000,  ///< Vendor non identifié ou inconnu
                    NK_NVIDIA     = 0x10DE,  ///< NVIDIA Corporation
                    NK_AMD        = 0x1002,  ///< AMD / ATI Technologies
                    NK_INTEL      = 0x8086,  ///< Intel Corporation (GPU intégrés)
                    NK_ARM        = 0x13B5,  ///< ARM Holdings (Mali GPU)
                    NK_QUALCOMM   = 0x5143,  ///< Qualcomm (Adreno GPU)
                    NK_APPLE      = 0x106B,  ///< Apple Inc. (M-series, GPU intégrés)
                    NK_IMGTEC     = 0x1010,  ///< Imagination Technologies (PowerVR)
                    NK_BROADCOM   = 0x14E4,  ///< Broadcom (VideoCore, Raspberry Pi)
                    NK_MICROSOFT  = 0x1414   ///< Microsoft (Software renderer, WARP)
                };

                // ====================================================================
                // SECTION 6 : ÉNUMÉRATION DES TYPES DE GPU
                // ====================================================================
                // Classification des GPU par catégorie d'architecture.

                /**
                 * @brief Types de GPU par catégorie d'architecture
                 * @enum NkGPUType
                 * @ingroup GraphicsEnums
                 *
                 * Classification utile pour l'adaptation des paramètres graphiques
                 * selon les capacités matérielles (mémoire, bande passante, etc.).
                 *
                 * @note
                 *   - NK_DISCRETE : GPU dédié avec mémoire vidéo propre
                 *   - NK_INTEGRATED : GPU partagé avec la RAM système
                 *   - NK_VIRTUAL : GPU virtualisé (VM, cloud gaming)
                 *   - NK_SOFTWARE : Émulation logicielle sans accélération hardware
                 *
                 * @example
                 * @code
                 * // Adaptation des paramètres selon le type de GPU
                 * void ConfigureGraphicsSettings(nkentseu::platform::graphics::NkGPUType gpuType) {
                 *     using namespace nkentseu::platform::graphics;
                 *
                 *     switch (gpuType) {
                 *         case NkGPUType::NK_DISCRETE:
                 *             SetQualityPreset(HIGH);
                 *             EnableRayTracing();
                 *             break;
                 *         case NkGPUType::NK_INTEGRATED:
                 *             SetQualityPreset(MEDIUM);
                 *             LimitTextureMemory(512 * 1024 * 1024);  // 512 MB max
                 *             break;
                 *         case NkGPUType::NK_SOFTWARE:
                 *             SetQualityPreset(LOW);
                 *             DisablePostProcessing();
                 *             break;
                 *         default:
                 *             SetQualityPreset(LOW);  // Fallback conservateur
                 *             break;
                 *     }
                 * }
                 * @endcode
                 */
                enum class NkGPUType : uint8_t {
                    NK_UNKNOWN    = 0,  ///< Type non déterminé
                    NK_DISCRETE   = 1,  ///< GPU dédié (carte graphique séparée)
                    NK_INTEGRATED = 2,  ///< GPU intégré au CPU (iGPU)
                    NK_VIRTUAL    = 3,  ///< GPU virtualisé (VM, cloud)
                    NK_SOFTWARE   = 4   ///< Renderer logiciel (pas d'accélération hardware)
                };

                // ====================================================================
                // SECTION 7 : FONCTIONS TEMPLATE UTILITAIRES (DÉCLARATIONS)
                // ====================================================================
                // Déclarations de fonctions utilitaires - implémentations dans le .cpp

                /**
                 * @brief Convertit une enum NkGraphicsApi en chaîne de caractères
                 * @tparam CharT Type de caractère pour la chaîne de retour (char, wchar_t, etc.)
                 * @param api L'API graphique à convertir
                 * @return Pointeur vers chaîne statique représentant l'API
                 * @note Retourne "Unknown" pour les valeurs hors plage
                 * @ingroup GraphicsUtilities
                 */
                template <typename CharT = char>
                inline constexpr const CharT *ToString(NkGraphicsApi api) noexcept;

                /**
                 * @brief Convertit une enum NkGPUVendor en chaîne de caractères
                 * @tparam CharT Type de caractère pour la chaîne de retour
                 * @param vendor Le fabricant de GPU à convertir
                 * @return Pointeur vers chaîne statique représentant le vendor
                 * @note Retourne "Unknown" pour NK_UNKNOWN ou valeurs invalides
                 * @ingroup GraphicsUtilities
                 */
                template <typename CharT = char>
                inline constexpr const CharT *ToString(NkGPUVendor vendor) noexcept;

                /**
                 * @brief Convertit une enum NkGPUType en chaîne de caractères
                 * @tparam CharT Type de caractère pour la chaîne de retour
                 * @param type Le type de GPU à convertir
                 * @return Pointeur vers chaîne statique représentant le type
                 * @ingroup GraphicsUtilities
                 */
                template <typename CharT = char>
                inline constexpr const CharT *ToString(NkGPUType type) noexcept;

                /**
                 * @brief Vérifie à la compilation si une API graphique est disponible
                 * @tparam API L'API graphique à vérifier (valeur de NkGraphicsApi)
                 * @return true si l'API est disponible sur la plateforme cible, false sinon
                 * @note Évaluation constexpr : résultat connu à la compilation
                 * @ingroup GraphicsUtilities
                 *
                 * @example
                 * @code
                 * // Sélection compile-time du backend
                 * template <nkentseu::platform::graphics::NkGraphicsApi API>
                 * void CreateRenderer() {
                 *     static_assert(
                 *         nkentseu::platform::graphics::IsAPIAvailable<API>(),
                 *         "Selected graphics API not available on this platform"
                 *     );
                 *     // ... initialisation spécifique à l'API
                 * }
                 * @endcode
                 */
                template <NkGraphicsApi API>
                inline constexpr bool IsAPIAvailable() noexcept;

                /**
                 * @brief Obtient l'API graphique par défaut recommandée pour la plateforme
                 * @tparam SFINAE parameter (pour spécialisations futures)
                 * @return L'API graphique recommandée comme premier choix
                 * @note Basé sur NKENTSEU_GRAPHICS_DEFAULT défini plus bas dans ce fichier
                 * @ingroup GraphicsUtilities
                 */
                template <typename = void>
                inline constexpr NkGraphicsApi GetDefaultAPI() noexcept;

                /**
                 * @brief Obtient l'API graphique la plus moderne disponible sur la plateforme
                 * @tparam SFINAE parameter (pour spécialisations futures)
                 * @return L'API graphique la plus récente supportée
                 * @note Utile pour les applications voulant utiliser les dernières features
                 * @ingroup GraphicsUtilities
                 */
                template <typename = void>
                inline constexpr NkGraphicsApi GetModernAPI() noexcept;

            } // namespace graphics

        } // namespace platform

    } // namespace nkentseu

    // -------------------------------------------------------------------------
    // SECTION 8 : MACROS D'API PAR DÉFAUT SELON LA PLATEFORME
    // -------------------------------------------------------------------------
    // Définition des APIs recommandées (default) et modernes par OS.

    /**
     * @defgroup DefaultAPIMacros Macros d'API par Défaut
     * @brief Définit l'API graphique par défaut et moderne pour chaque plateforme
     * @ingroup PlatformConfiguration
     *
     * Ces macros sont utilisées par le système d'initialisation graphique
     * pour sélectionner automatiquement le backend approprié.
     *
     * @note
     *   - NKENTSEU_GRAPHICS_DEFAULT : API stable et largement supportée
     *   - NKENTSEU_GRAPHICS_MODERN : API la plus récente avec meilleures performances
     *   - Peut être surchargé via définition manuelle avant inclusion de ce fichier
     *
     * @example
     * @code
     * // Forcer l'utilisation de Vulkan même sur Windows
     * #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsApi::NK_VULKAN
     * #include "NKPlatform/NkCGXDetect.h"
     * @endcode
     */
    /** @{ */

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_D3D11
        #define NKENTSEU_GRAPHICS_MODERN  nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_D3D12

    #elif defined(NKENTSEU_PLATFORM_LINUX)     || \
          defined(NKENTSEU_PLATFORM_FREEBSD)   || \
          defined(NKENTSEU_PLATFORM_OPENBSD)   || \
          defined(NKENTSEU_PLATFORM_NETBSD)
        #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_OPENGL
        #define NKENTSEU_GRAPHICS_MODERN  nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_VULKAN

    #elif defined(NKENTSEU_PLATFORM_MACOS)
        #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_METAL
        #define NKENTSEU_GRAPHICS_MODERN  nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_METAL

    #elif defined(NKENTSEU_PLATFORM_IOS)      || \
          defined(NKENTSEU_PLATFORM_TVOS)     || \
          defined(NKENTSEU_PLATFORM_WATCHOS)  || \
          defined(NKENTSEU_PLATFORM_VISIONOS)
        #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_METAL
        #define NKENTSEU_GRAPHICS_MODERN  nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_METAL

    #elif defined(NKENTSEU_PLATFORM_ANDROID)
        #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_OPENGLES
        #define NKENTSEU_GRAPHICS_MODERN  nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_VULKAN

    #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_WEBGL2
        #define NKENTSEU_GRAPHICS_MODERN  nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_WEBGPU

    #elif defined(NKENTSEU_PLATFORM_PS5)
        #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_VULKAN
        #define NKENTSEU_GRAPHICS_MODERN  nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_VULKAN

    #elif defined(NKENTSEU_PLATFORM_PS4)  || \
          defined(NKENTSEU_PLATFORM_PS3)  || \
          defined(NKENTSEU_PLATFORM_PSP)  || \
          defined(NKENTSEU_PLATFORM_PSVITA)
        #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_GNM
        #define NKENTSEU_GRAPHICS_MODERN  nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_GNM

    #elif defined(NKENTSEU_PLATFORM_XBOX_SERIES) || \
          defined(NKENTSEU_PLATFORM_XBOXONE)
        #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_D3D12
        #define NKENTSEU_GRAPHICS_MODERN  nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_D3D12

    #elif defined(NKENTSEU_PLATFORM_XBOX360)      || \
          defined(NKENTSEU_PLATFORM_XBOX_ORIGINAL)
        #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_D3D11
        #define NKENTSEU_GRAPHICS_MODERN  nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_D3D11

    #elif defined(NKENTSEU_PLATFORM_SWITCH)
        #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_NVN
        #define NKENTSEU_GRAPHICS_MODERN  nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_VULKAN

    #elif defined(NKENTSEU_PLATFORM_WIIU)     || \
          defined(NKENTSEU_PLATFORM_WII)      || \
          defined(NKENTSEU_PLATFORM_GAMECUBE) || \
          defined(NKENTSEU_PLATFORM_N64)      || \
          defined(NKENTSEU_PLATFORM_3DS)      || \
          defined(NKENTSEU_PLATFORM_NDS)      || \
          defined(NKENTSEU_PLATFORM_GBA)      || \
          defined(NKENTSEU_PLATFORM_GAMEBOY)
        #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_OPENGL
        #define NKENTSEU_GRAPHICS_MODERN  nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_OPENGL

    #elif defined(NKENTSEU_PLATFORM_SEGA)
        #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_OPENGL
        #define NKENTSEU_GRAPHICS_MODERN  nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_OPENGL

    #elif defined(NKENTSEU_PLATFORM_HARMONYOS)
        #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_OPENGLES
        #define NKENTSEU_GRAPHICS_MODERN  nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_VULKAN

    #elif defined(NKENTSEU_PLATFORM_EMBEDDED)
        #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_SOFTWARE
        #define NKENTSEU_GRAPHICS_MODERN  nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_SOFTWARE

    #else
        // Fallback universel si aucune plateforme spécifique n'est détectée
        #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_OPENGL
        #define NKENTSEU_GRAPHICS_MODERN  nkentseu::platform::graphics::NkGraphicsApi::NK_GFX_API_VULKAN
    #endif

    /** @} */ // End of DefaultAPIMacros

    // -------------------------------------------------------------------------
    // SECTION 9 : MACROS CONDITIONNELLES PAR API GRAPHIQUE
    // -------------------------------------------------------------------------
    // Macros utilitaires pour inclusion/exclusion de code selon l'API disponible.

    /**
     * @defgroup GraphicsConditionalMacros Macros Conditionnelles Graphiques
     * @brief Macros pour compilation conditionnelle selon l'API graphique
     * @ingroup PlatformMacros
     *
     * Ces macros permettent d'inclure ou exclure du code selon la disponibilité
     * d'une API graphique spécifique, sans imbriquer manuellement des #if/#endif.
     *
     * @note
     *   - Format : NKENTSEU_*_ONLY(code) inclut le code si l'API est disponible
     *   - Format : NKENTSEU_NOT_*(code) inclut le code si l'API n'est PAS disponible
     *   - Le code passé en argument n'est pas évalué si la condition est fausse
     *
     * @example
     * @code
     * // Inclusion conditionnelle de fichiers d'en-tête
     * NKENTSEU_VULKAN_ONLY(
     *     #include <vulkan/vulkan.h>
     * )
     * NKENTSEU_METAL_ONLY(
     *     #import <Metal/Metal.h>
     * )
     *
     * // Définition de fonctions spécifiques à une API
     * NKENTSEU_D3D12_ONLY(
     *     void CreateD3D12PipelineState() { /\* ... *\/ }
     * )
     * NKENTSEU_NOT_D3D12(
     *     inline void CreateD3D12PipelineState() {
     *         // Stub ou fallback pour autres plateformes
     *     }
     * )
     * @endcode
     */
    /** @{ */

    #ifdef NKENTSEU_GRAPHICS_D3D11_AVAILABLE
        #define NKENTSEU_D3D11_ONLY(...) __VA_ARGS__
        #define NKENTSEU_NOT_D3D11(...)
    #else
        #define NKENTSEU_D3D11_ONLY(...)
        #define NKENTSEU_NOT_D3D11(...) __VA_ARGS__
    #endif

    #ifdef NKENTSEU_GRAPHICS_D3D12_AVAILABLE
        #define NKENTSEU_D3D12_ONLY(...) __VA_ARGS__
        #define NKENTSEU_NOT_D3D12(...)
    #else
        #define NKENTSEU_D3D12_ONLY(...)
        #define NKENTSEU_NOT_D3D12(...) __VA_ARGS__
    #endif

    #ifdef NKENTSEU_GRAPHICS_VULKAN_AVAILABLE
        #define NKENTSEU_VULKAN_ONLY(...) __VA_ARGS__
        #define NKENTSEU_NOT_VULKAN(...)
    #else
        #define NKENTSEU_VULKAN_ONLY(...)
        #define NKENTSEU_NOT_VULKAN(...) __VA_ARGS__
    #endif

    #ifdef NKENTSEU_GRAPHICS_METAL_AVAILABLE
        #define NKENTSEU_METAL_ONLY(...) __VA_ARGS__
        #define NKENTSEU_NOT_METAL(...)
    #else
        #define NKENTSEU_METAL_ONLY(...)
        #define NKENTSEU_NOT_METAL(...) __VA_ARGS__
    #endif

    #ifdef NKENTSEU_GRAPHICS_OPENGL_AVAILABLE
        #define NKENTSEU_OPENGL_ONLY(...) __VA_ARGS__
        #define NKENTSEU_NOT_OPENGL(...)
    #else
        #define NKENTSEU_OPENGL_ONLY(...)
        #define NKENTSEU_NOT_OPENGL(...) __VA_ARGS__
    #endif

    #ifdef NKENTSEU_GRAPHICS_GLES3_AVAILABLE
        #define NKENTSEU_GLES_ONLY(...) __VA_ARGS__
        #define NKENTSEU_NOT_GLES(...)
    #else
        #define NKENTSEU_GLES_ONLY(...)
        #define NKENTSEU_NOT_GLES(...) __VA_ARGS__
    #endif

    #ifdef NKENTSEU_GRAPHICS_WEBGL2_AVAILABLE
        #define NKENTSEU_WEBGL_ONLY(...) __VA_ARGS__
        #define NKENTSEU_NOT_WEBGL(...)
    #else
        #define NKENTSEU_WEBGL_ONLY(...)
        #define NKENTSEU_NOT_WEBGL(...) __VA_ARGS__
    #endif

    #ifdef NKENTSEU_GRAPHICS_WEBGPU_AVAILABLE
        #define NKENTSEU_WEBGPU_ONLY(...) __VA_ARGS__
        #define NKENTSEU_NOT_WEBGPU(...)
    #else
        #define NKENTSEU_WEBGPU_ONLY(...)
        #define NKENTSEU_NOT_WEBGPU(...) __VA_ARGS__
    #endif

    #ifdef NKENTSEU_GRAPHICS_GNM_AVAILABLE
        #define NKENTSEU_GNM_ONLY(...) __VA_ARGS__
        #define NKENTSEU_NOT_GNM(...)
    #else
        #define NKENTSEU_GNM_ONLY(...)
        #define NKENTSEU_NOT_GNM(...) __VA_ARGS__
    #endif

    #ifdef NKENTSEU_GRAPHICS_NVN_AVAILABLE
        #define NKENTSEU_NVN_ONLY(...) __VA_ARGS__
        #define NKENTSEU_NOT_NVN(...)
    #else
        #define NKENTSEU_NVN_ONLY(...)
        #define NKENTSEU_NOT_NVN(...) __VA_ARGS__
    #endif

    /** @} */ // End of GraphicsConditionalMacros

    // -------------------------------------------------------------------------
    // SECTION 10 : DÉTECTION DES APIS DE CALCUL (CUDA, OpenCL, SYCL)
    // -------------------------------------------------------------------------
    // Macros pour détection des frameworks de calcul GPU/CPU hétérogène.

    /**
     * @defgroup ComputeDetection Détection API Calcul
     * @brief Macros pour détecter les APIs de calcul parallèle (GPU/CPU)
     * @ingroup PlatformDetection
     *
     * Ces macros indiquent la disponibilité des frameworks de calcul
     * pour l'accélération des tâches parallèles (machine learning,
     * traitement d'image, simulations, etc.).
     *
     * @note
     *   - NKENTSEU_COMPUTE_AVAILABLE : définie si au moins une API compute est présente
     *   - Les APIs compute peuvent être utilisées indépendamment des APIs graphiques
     *   - La détection se fait via les macros prédéfinies par les compilateurs/SDK
     *
     * @example
     * @code
     * // Sélection du backend compute à la compilation
     * #if defined(NKENTSEU_COMPUTE_CUDA_AVAILABLE)
     *     #define COMPUTE_BACKEND "CUDA"
     *     #include "Compute/CudaKernels.h"
     * #elif defined(NKENTSEU_COMPUTE_OPENCL_AVAILABLE)
     *     #define COMPUTE_BACKEND "OpenCL"
     *     #include "Compute/OpenCLKernels.h"
     * #else
     *     #define COMPUTE_BACKEND "CPU"
     *     // Fallback vers implémentation CPU multithreaded
     * #endif
     * @endcode
     */
    /** @{ */

    #if defined(__CUDACC__)
        /** @brief CUDA Compiler detected - NVIDIA CUDA available */
        #define NKENTSEU_COMPUTE_CUDA_AVAILABLE
        #define NKENTSEU_COMPUTE_AVAILABLE
    #endif

    #if defined(__OPENCL_VERSION__)
        /** @brief OpenCL C version macro detected - OpenCL available */
        #define NKENTSEU_COMPUTE_OPENCL_AVAILABLE
        #define NKENTSEU_COMPUTE_AVAILABLE
    #endif

    #if defined(__SYCL_DEVICE_ONLY__) || defined(SYCL_LANGUAGE_VERSION)
        /** @brief SYCL language macros detected - SYCL available */
        #define NKENTSEU_COMPUTE_SYCL_AVAILABLE
        #define NKENTSEU_COMPUTE_AVAILABLE
    #endif

    // Macros conditionnelles pour code compute-specific
    #ifdef NKENTSEU_COMPUTE_CUDA_AVAILABLE
        #define NKENTSEU_CUDA_ONLY(...) __VA_ARGS__
        #define NKENTSEU_NOT_CUDA(...)
    #else
        #define NKENTSEU_CUDA_ONLY(...)
        #define NKENTSEU_NOT_CUDA(...) __VA_ARGS__
    #endif

    #ifdef NKENTSEU_COMPUTE_OPENCL_AVAILABLE
        #define NKENTSEU_OPENCL_ONLY(...) __VA_ARGS__
        #define NKENTSEU_NOT_OPENCL(...)
    #else
        #define NKENTSEU_OPENCL_ONLY(...)
        #define NKENTSEU_NOT_OPENCL(...) __VA_ARGS__
    #endif

    #ifdef NKENTSEU_COMPUTE_SYCL_AVAILABLE
        #define NKENTSEU_SYCL_ONLY(...) __VA_ARGS__
        #define NKENTSEU_NOT_SYCL(...)
    #else
        #define NKENTSEU_SYCL_ONLY(...)
        #define NKENTSEU_NOT_SYCL(...) __VA_ARGS__
    #endif

    /** @} */ // End of ComputeDetection

    // -------------------------------------------------------------------------
    // SECTION 11 : DÉTECTION DU SYSTÈME D'AFFICHAGE (Linux/Unix)
    // -------------------------------------------------------------------------
    // Identification du serveur d'affichage pour l'intégration fenêtre.

    /**
     * @defgroup DisplaySystemDetection Détection Système d'Affichage
     * @brief Macros pour identifier le backend de fenêtrage sous Linux/Unix
     * @ingroup PlatformDetection
     *
     * Sous les systèmes Unix-like, plusieurs systèmes de fenêtrage coexistent.
     * Ces macros aident à sélectionner le bon backend pour la création de fenêtres
     * et la gestion des événements d'entrée.
     *
     * @note
     *   - Wayland est le successeur moderne de X11, mais X11 reste largement supporté
     *   - XCB est l'interface bas-niveau de X11, plus légère que Xlib
     *   - La détection se fait via les macros d'environnement ou de compilation
     *
     * @example
     * @code
     * // Sélection du backend de création de fenêtre
     * #if defined(NKENTSEU_DISPLAY_WAYLAND)
     *     WindowHandle CreateWindowWayland(const WindowConfig& cfg);
     * #elif defined(NKENTSEU_DISPLAY_XCB)
     *     WindowHandle CreateWindowXCB(const WindowConfig& cfg);
     * #elif defined(NKENTSEU_DISPLAY_XLIB)
     *     WindowHandle CreateWindowXlib(const WindowConfig& cfg);
     * #else
     *     #error "No display system detected for this Unix platform"
     * #endif
     * @endcode
     */
    /** @{ */

    NKENTSEU_LINUX_ONLY(
        #if defined(WAYLAND_DISPLAY) || defined(__WAYLAND__)
            /** @brief Wayland display server detected */
            #define NKENTSEU_DISPLAY_WAYLAND
        #elif defined(DISPLAY) || defined(__X11__)
            #if defined(__USE_XCB)
                /** @brief XCB (X C Binding) detected - low-level X11 interface */
                #define NKENTSEU_DISPLAY_XCB
            #else
                /** @brief Xlib detected - traditional X11 interface */
                #define NKENTSEU_DISPLAY_XLIB
            #endif
        #endif
    )

    /** @} */ // End of DisplaySystemDetection

    // -------------------------------------------------------------------------
    // SECTION 12 : CONSTANTES VENDOR ID PCI
    // -------------------------------------------------------------------------
    // Définitions redondantes pour compatibilité avec code legacy.

    /**
     * @defgroup VendorIDConstants Constantes Vendor ID PCI
     * @brief IDs PCI des fabricants de GPU pour identification matérielle
     * @ingroup HardwareConstants
     *
     * Ces constantes sont des alias vers les valeurs de l'enum NkGPUVendor,
     * fournies pour compatibilité avec du code utilisant des macros plutôt
     * que des enums typées.
     *
     * @deprecated Préférer l'utilisation de nkentseu::platform::graphics::NkGPUVendor
     *
     * @example
     * @code
     * // Ancien style (encore supporté)
     * if (gpuVendorId == NKENTSEU_GPU_VENDOR_NVIDIA_ID) { /\* ... *\/ }
     *
     * // Nouveau style recommandé
     * if (gpuVendor == nkentseu::platform::graphics::NkGPUVendor::NK_NVIDIA) { /\* ... *\/ }
     * @endcode
     */
    /** @{ */

    #define NKENTSEU_GPU_VENDOR_NVIDIA_ID    0x10DE  ///< NVIDIA Corporation
    #define NKENTSEU_GPU_VENDOR_AMD_ID       0x1002  ///< AMD / ATI Technologies
    #define NKENTSEU_GPU_VENDOR_INTEL_ID     0x8086  ///< Intel Corporation
    #define NKENTSEU_GPU_VENDOR_ARM_ID       0x13B5  ///< ARM Holdings (Mali)
    #define NKENTSEU_GPU_VENDOR_QUALCOMM_ID  0x5143  ///< Qualcomm (Adreno)
    #define NKENTSEU_GPU_VENDOR_APPLE_ID     0x106B  ///< Apple Inc.
    #define NKENTSEU_GPU_VENDOR_IMGTEC_ID    0x1010  ///< Imagination Technologies (PowerVR)
    #define NKENTSEU_GPU_VENDOR_BROADCOM_ID  0x14E4  ///< Broadcom (VideoCore)
    #define NKENTSEU_GPU_VENDOR_MICROSOFT_ID 0x1414  ///< Microsoft (WARP software renderer)

    /** @} */ // End of VendorIDConstants

    // -------------------------------------------------------------------------
    // SECTION 13 : CONFIGURATION DE L'API GRAPHIQUE ACTIVE
    // -------------------------------------------------------------------------
    // Détection automatique et sélection de l'API à utiliser à l'exécution.

    /**
     * @defgroup GraphicsConfigMacros Configuration API Graphique Active
     * @brief Macros pour détermination et sélection de l'API graphique active
     * @ingroup PlatformConfiguration
     *
     * Ce système détermine automatiquement quelle API graphique utiliser,
     * avec possibilité de forçage manuel via NKENTSEU_GFX_FORCE.
     *
     * @note
     *   - NKENTSEU_GFX_ACTIVE : ID numérique de l'API sélectionnée
     *   - NKENTSEU_GFX_VERSION : Version encodée (major << 16 | minor)
     *   - La détection utilise __has_include() pour vérifier la présence des headers
     *
     * @example
     * @code
     * // Vérification de l'API active à l'exécution
     * void InitGraphics() {
     *     #if NKENTSEU_GFX_ACTIVE == NKENTSEU_GFX_VULKAN
     *         InitVulkanBackend(NKENTSEU_GFX_VERSION);
     *     #elif NKENTSEU_GFX_ACTIVE == NKENTSEU_GFX_METAL
     *         InitMetalBackend();
     *     #elif NKENTSEU_GFX_ACTIVE == NKENTSEU_GFX_SOFTWARE
     *         InitSoftwareRenderer();  // Fallback
     *     #endif
     *
     *     // Logging de la configuration détectée
     *     NK_LOG_INFO("Graphics API: %s v%d.%d",
     *         GetGraphicsApiName(NKENTSEU_GFX_ACTIVE),
     *         (NKENTSEU_GFX_VERSION >> 16) & 0xFFFF,
     *         NKENTSEU_GFX_VERSION & 0xFFFF);
     * }
     * @endcode
     */
    /** @{ */

    // Identifiants numériques pour NKENTSEU_GFX_ACTIVE
    #define NKENTSEU_GFX_NONE      0  ///< Aucune API (état invalide)
    #define NKENTSEU_GFX_VULKAN    1  ///< Vulkan
    #define NKENTSEU_GFX_METAL     2  ///< Metal
    #define NKENTSEU_GFX_DIRECTX   3  ///< Direct3D (11 ou 12)
    #define NKENTSEU_GFX_OPENGL    4  ///< OpenGL / OpenGL ES
    #define NKENTSEU_GFX_SOFTWARE  5  ///< Renderer logiciel (fallback)

    /**
     * @brief Encode une version (major, minor) en entier 32 bits
     * @param major Numéro de version majeur
     * @param minor Numéro de version mineur
     * @return Valeur encodée : (major << 16) | minor
     * @ingroup GraphicsConfigMacros
     */
    #define NKENTSEU_GFX_VERSION_CALC(major, minor) \
        (((major) << 16) | ((minor) & 0xFFFF))

    // Détection automatique de l'API active
    #if defined(NKENTSEU_GFX_FORCE)
        // Forçage manuel par l'utilisateur via définition préalable
        #define NKENTSEU_GFX_ACTIVE NKENTSEU_GFX_FORCE
        #define NKENTSEU_GFX_VERSION NKENTSEU_GFX_VERSION_CALC(1, 0)  // Version par défaut

    #else
        // Détection automatique selon la plateforme et les headers disponibles

        // macOS : priorité à Metal si headers disponibles
        #if defined(NKENTSEU_PLATFORM_MACOS)
            #if __has_include(<Metal/Metal.h>)
                #define NKENTSEU_GFX_ACTIVE NKENTSEU_GFX_METAL
                #define NKENTSEU_GFX_VERSION NKENTSEU_GFX_VERSION_CALC(3, 0)  // Metal 3 sur macOS récent
            #endif

        // Windows : priorité à Direct3D 12, fallback 11
        #elif defined(NKENTSEU_PLATFORM_WINDOWS)
            #if defined(_DIRECTX12_) && __has_include(<d3d12.h>)
                #define NKENTSEU_GFX_ACTIVE NKENTSEU_GFX_DIRECTX
                #define NKENTSEU_GFX_VERSION NKENTSEU_GFX_VERSION_CALC(12, 0)
            #elif defined(_DIRECTX11_) && __has_include(<d3d11.h>)
                #define NKENTSEU_GFX_ACTIVE NKENTSEU_GFX_DIRECTX
                #define NKENTSEU_GFX_VERSION NKENTSEU_GFX_VERSION_CALC(11, 1)
            #endif

        // Détection cross-platform : Vulkan en priorité si headers présents
        #if !defined(NKENTSEU_GFX_ACTIVE)
            #if __has_include(<vulkan/vulkan.h>)
                #define NKENTSEU_GFX_ACTIVE NKENTSEU_GFX_VULKAN
                #if defined(VK_API_VERSION_1_3)
                    #define NKENTSEU_GFX_VERSION NKENTSEU_GFX_VERSION_CALC(1, 3)
                #elif defined(VK_API_VERSION_1_2)
                    #define NKENTSEU_GFX_VERSION NKENTSEU_GFX_VERSION_CALC(1, 2)
                #elif defined(VK_API_VERSION_1_1)
                    #define NKENTSEU_GFX_VERSION NKENTSEU_GFX_VERSION_CALC(1, 1)
                #else
                    #define NKENTSEU_GFX_VERSION NKENTSEU_GFX_VERSION_CALC(1, 0)
                #endif

            // Fallback vers OpenGL si Vulkan non disponible
            #elif __has_include(<GL/gl.h>) || __has_include(<GLES3/gl3.h>)
                #define NKENTSEU_GFX_ACTIVE NKENTSEU_GFX_OPENGL
                #if defined(GL_VERSION_4_6)
                    #define NKENTSEU_GFX_VERSION NKENTSEU_GFX_VERSION_CALC(4, 6)
                #elif defined(GL_VERSION_4_5)
                    #define NKENTSEU_GFX_VERSION NKENTSEU_GFX_VERSION_CALC(4, 5)
                #elif defined(GL_VERSION_3_3)
                    #define NKENTSEU_GFX_VERSION NKENTSEU_GFX_VERSION_CALC(3, 3)
                #else
                    #define NKENTSEU_GFX_VERSION NKENTSEU_GFX_VERSION_CALC(4, 5)  // Estimation conservative
                #endif

            // Fallback ultime : renderer logiciel
            #else
                #define NKENTSEU_GFX_ACTIVE NKENTSEU_GFX_SOFTWARE
                #define NKENTSEU_GFX_VERSION NKENTSEU_GFX_VERSION_CALC(1, 0)
            #endif
        #endif
    #endif

    // Validation finale : avertissement si aucune API valide détectée
    #if !defined(NKENTSEU_GFX_ACTIVE) || (NKENTSEU_GFX_ACTIVE == NKENTSEU_GFX_NONE)
        #warning "Nkentseu: Aucune API graphique valide détectée, utilisation du renderer software"
        #define NKENTSEU_GFX_ACTIVE NKENTSEU_GFX_SOFTWARE
        #define NKENTSEU_GFX_VERSION NKENTSEU_GFX_VERSION_CALC(1, 0)
    #endif

    /** @} */ // End of GraphicsConfigMacros

    // -------------------------------------------------------------------------
    // SECTION 14 : MACROS DE DEBUG ET LOGGING DE CONFIGURATION
    // -------------------------------------------------------------------------
    // Output de compilation pour vérification de la configuration détectée.

    /**
     * @defgroup GraphicsDebugMacros Macros de Debug Graphique
     * @brief Macros pour logging de la configuration graphique à la compilation
     * @ingroup DebugUtilities
     *
     * Lorsque NKENTSEU_CGX_DEBUG est défini, ce fichier émet des messages
     * de compilation (#pragma message) listant les APIs détectées comme
     * disponibles. Utile pour déboguer les problèmes de détection de plateforme.
     *
     * @note
     *   - Ces messages n'apparaissent qu'en mode compilation avec warnings activés
     *   - Ne pas définir NKENTSEU_CGX_DEBUG en production (impact sur les logs de build)
     *
     * @example
     * @code
     * // Dans votre fichier de configuration de build (CMakeLists.txt, etc.)
     * # Pour activer le debug de détection graphique :
     * add_definitions(-DNKENTSEU_CGX_DEBUG)
     *
     * // Output de compilation typique :
     * // Nkentseu Graphics Detection:
     * //   - Vulkan: Available
     * //   - OpenGL: Available
     * //   - Direct3D 12: Available
     * @endcode
     */
    /** @{ */

    #ifdef NKENTSEU_CGX_DEBUG
        #pragma message("Nkentseu Graphics Detection:")

        #ifdef NKENTSEU_GRAPHICS_D3D11_AVAILABLE
            #pragma message("  - Direct3D 11: Available")
        #endif

        #ifdef NKENTSEU_GRAPHICS_D3D12_AVAILABLE
            #pragma message("  - Direct3D 12: Available")
        #endif

        #ifdef NKENTSEU_GRAPHICS_VULKAN_AVAILABLE
            #pragma message("  - Vulkan: Available")
        #endif

        #ifdef NKENTSEU_GRAPHICS_METAL_AVAILABLE
            #pragma message("  - Metal: Available")
        #endif

        #ifdef NKENTSEU_GRAPHICS_OPENGL_AVAILABLE
            #pragma message("  - OpenGL: Available")
        #endif

        #ifdef NKENTSEU_GRAPHICS_GLES3_AVAILABLE
            #pragma message("  - OpenGL ES 3: Available")
        #endif

        #ifdef NKENTSEU_GRAPHICS_WEBGL2_AVAILABLE
            #pragma message("  - WebGL 2: Available")
        #endif

        #ifdef NKENTSEU_GRAPHICS_WEBGPU_AVAILABLE
            #pragma message("  - WebGPU: Available (experimental)")
        #endif

        #pragma message("  => Selected API: " NK_STRINGIZE(NKENTSEU_GFX_ACTIVE) " v" NK_STRINGIZE(NKENTSEU_GFX_VERSION))
    #endif

    /** @} */ // End of GraphicsDebugMacros

#endif // NKENTSEU_PLATFORM_NKCGXDETECT_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION DE NKCGXDETECT.H
// =============================================================================
// Ce fichier fournit la détection compile-time des capacités graphiques et compute.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Sélection conditionnelle du backend graphique
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkCGXDetect.h"
    #include "NKCore/NkFoundationLog.h"

    class GraphicsManager {
    public:
        static GraphicsManager& Instance() {
            static GraphicsManager instance;
            return instance;
        }

        bool Initialize() {
            // Logging de la configuration détectée
            NK_FOUNDATION_LOG_INFO(
                "Graphics config: API=%d, Version=%d.%d",
                NKENTSEU_GFX_ACTIVE,
                (NKENTSEU_GFX_VERSION >> 16) & 0xFFFF,
                NKENTSEU_GFX_VERSION & 0xFFFF
            );

            // Dispatch vers l'initialisation spécifique
            #if NKENTSEU_GFX_ACTIVE == NKENTSEU_GFX_VULKAN
                return InitVulkan();
            #elif NKENTSEU_GFX_ACTIVE == NKENTSEU_GFX_METAL
                return InitMetal();
            #elif NKENTSEU_GFX_ACTIVE == NKENTSEU_GFX_DIRECTX
                return InitDirectX();
            #elif NKENTSEU_GFX_ACTIVE == NKENTSEU_GFX_OPENGL
                return InitOpenGL();
            #else
                NK_FOUNDATION_LOG_WARNING("Falling back to software renderer");
                return InitSoftware();
            #endif
        }

    private:
        // Déclarations conditionnelles selon disponibilité
        NKENTSEU_VULKAN_ONLY(
            bool InitVulkan();
        )
        NKENTSEU_METAL_ONLY(
            bool InitMetal();
        )
        NKENTSEU_D3D12_ONLY(
            bool InitDirectX();
        )
        NKENTSEU_OPENGL_ONLY(
            bool InitOpenGL();
        )
        bool InitSoftware();  // Toujours disponible
    };
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Utilisation des énumérations typées
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkCGXDetect.h"
    #include <string>

    // Conversion runtime d'une enum vers string (complément aux templates constexpr)
    std::string GraphicsApiToString(nkentseu::platform::graphics::NkGraphicsApi api) {
        using namespace nkentseu::platform::graphics;

        switch (api) {
            case NkGraphicsApi::NK_GFX_API_VULKAN:  return "Vulkan";
            case NkGraphicsApi::NK_GFX_API_METAL:   return "Metal";
            case NkGraphicsApi::NK_GFX_API_D3D12:   return "Direct3D 12";
            case NkGraphicsApi::NK_GFX_API_D3D11:   return "Direct3D 11";
            case NkGraphicsApi::NK_GFX_API_OPENGL:  return "OpenGL";
            case NkGraphicsApi::NK_GFX_API_OPENGLES:return "OpenGL ES";
            case NkGraphicsApi::NK_GFX_API_WEBGL2:  return "WebGL 2";
            case NkGraphicsApi::NK_GFX_API_SOFTWARE:return "Software";
            default: return "Unknown";
        }
    }

    // Vérification de compatibilité GPU
    bool IsGPUSupported(
        nkentseu::platform::graphics::NkGPUVendor vendor,
        nkentseu::platform::graphics::NkGPUType type
    ) {
        using namespace nkentseu::platform::graphics;

        // Refuser les GPU software pour les applications exigeantes
        if (type == NkGPUType::NK_SOFTWARE) {
            return false;
        }

        // Workaround pour anciens drivers Intel sur Windows
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            if (vendor == NkGPUVendor::NK_INTEL && type == NkGPUType::NK_INTEGRATED) {
                return CheckIntelDriverVersion();  // Fonction personnalisée
            }
        #endif

        return true;  // Par défaut, accepter
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Configuration compute pour accélération parallèle
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkCGXDetect.h"

    // Wrapper unifié pour kernels de calcul
    class ComputeKernel {
    public:
        virtual ~ComputeKernel() = default;
        virtual void Execute(void* inputData, void* outputData, size_t elementCount) = 0;
    };

    // Factory conditionnelle selon disponibilité
    ComputeKernel* CreateImageFilterKernel() {
        #if defined(NKENTSEU_COMPUTE_CUDA_AVAILABLE)
            return new CudaImageFilterKernel();  // Implémentation CUDA
        #elif defined(NKENTSEU_COMPUTE_OPENCL_AVAILABLE)
            return new OpenCLImageFilterKernel();  // Implémentation OpenCL
        #elif defined(NKENTSEU_COMPUTE_SYCL_AVAILABLE)
            return new SYCLImageFilterKernel();  // Implémentation SYCL
        #else
            // Fallback CPU multithreaded via NKCore threading
            return new CPUImageFilterKernel();
        #endif
    }

    // Exécution avec fallback automatique
    void ProcessImage(void* pixels, size_t width, size_t height) {
        auto kernel = CreateImageFilterKernel();
        if (kernel) {
            kernel->Execute(pixels, pixels, width * height);
            delete kernel;
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Détection des capacités à l'exécution
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkCGXDetect.h"
    #include "NKPlatform/NkPlatformQuery.h"  // Pour QueryGPUInfo()

    struct GraphicsCapabilities {
        nkentseu::platform::graphics::NkGraphicsApi api;
        nkentseu::platform::graphics::NkGPUVendor vendor;
        nkentseu::platform::graphics::NkGPUType type;
        uint32_t apiVersion;
        size_t videoMemoryMB;
        bool supportsRayTracing;
        bool supportsVariableRateShading;
    };

    GraphicsCapabilities DetectGraphicsCaps() {
        using namespace nkentseu::platform::graphics;

        GraphicsCapabilities caps{};

        // API compile-time (détection header)
        #if NKENTSEU_GFX_ACTIVE == NKENTSEU_GFX_VULKAN
            caps.api = NkGraphicsApi::NK_GFX_API_VULKAN;
            caps.apiVersion = NKENTSEU_GFX_VERSION;
        #elif NKENTSEU_GFX_ACTIVE == NKENTSEU_GFX_METAL
            caps.api = NkGraphicsApi::NK_GFX_API_METAL;
            caps.apiVersion = NKENTSEU_GFX_VERSION;
        // ... autres APIs
        #else
            caps.api = NkGraphicsApi::NK_GFX_API_SOFTWARE;
            caps.apiVersion = NKENTSEU_GFX_VERSION;
        #endif

        // Query hardware info via NKPlatform (implémentation spécifique)
        auto gpuInfo = nkentseu::platform::QueryGPUInfo();
        caps.vendor = static_cast<NkGPUVendor>(gpuInfo.vendorId);
        caps.type = static_cast<NkGPUType>(gpuInfo.deviceType);
        caps.videoMemoryMB = gpuInfo.dedicatedVideoMemoryMB;

        // Détection des features avancées (runtime)
        #if defined(NKENTSEU_GRAPHICS_VULKAN_AVAILABLE)
            if (caps.api == NkGraphicsApi::NK_GFX_API_VULKAN) {
                caps.supportsRayTracing = CheckVulkanRayTracingSupport();
                caps.supportsVariableRateShading = CheckVulkanVRSSupport();
            }
        #endif

        return caps;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Macros conditionnelles pour code cross-platform
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkCGXDetect.h"

    // Déclaration de fonction avec implémentations conditionnelles
    void CreateSwapchain(void* windowHandle) {
        NKENTSEU_VULKAN_ONLY(
            // Vulkan : vkCreateSwapchainKHR
            VkSwapchainCreateInfoKHR createInfo{};
            createInfo.surface = GetVulkanSurface(windowHandle);
            // ... configuration ...
            vkCreateSwapchainKHR(vkDevice, &createInfo, nullptr, &swapchain);
        )

        NKENTSEU_METAL_ONLY(
            // Metal : CAMetalLayer
            id<CAMetalLayer> metalLayer = (__bridge id<CAMetalLayer>)windowHandle;
            metalLayer.device = GetMetalDevice();
            metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        )

        NKENTSEU_D3D12_ONLY(
            // Direct3D 12 : IDXGISwapChain3
            DXGI_SWAP_CHAIN_DESC1 swapDesc{};
            swapDesc.BufferCount = 3;
            swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            // ... configuration ...
            dxgiFactory->CreateSwapChainForHwnd(
                d3dCommandQueue, windowHandle, &swapDesc, nullptr, nullptr, &swapChain
            );
        )

        NKENTSEU_OPENGL_ONLY(
            // OpenGL : contexte géré par la fenêtre (GLFW/SDL/etc.)
            // Pas de création explicite de swapchain
        )
    }

    // Fonction utilitaire avec fallback
    void SetVSyncEnabled(bool enabled) {
        NKENTSEU_VULKAN_ONLY(
            SetVulkanVSync(enabled);  // Via VK_KHR_present_wait ou mailbox mode
        )
        NKENTSEU_METAL_ONLY(
            GetMetalDrawableQueue()->setVSyncEnabled(enabled);
        )
        NKENTSEU_D3D12_ONLY(
            dxgiSwapChain->SetSyncInterval(enabled ? 1 : 0);
        )
        NKENTSEU_OPENGL_ONLY(
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                wglSwapIntervalEXT(enabled ? 1 : 0);
            #elif defined(NKENTSEU_PLATFORM_LINUX)
                glXSwapIntervalEXT(display, drawable, enabled ? 1 : 0);
            #endif
        )
        NKENTSEU_NOT_VULKAN(
        NKENTSEU_NOT_METAL(
        NKENTSEU_NOT_D3D12(
        NKENTSEU_NOT_OPENGL(
            // Fallback software : pas de VSync supporté
            NK_UNUSED(enabled);
        ))))
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Configuration via variables d'environnement
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkCGXDetect.h"
    #include "NKPlatform/NkEnvironment.h"  // Pour GetEnvVar()

    // Sélection de l'API via variable d'environnement NK_GFX_BACKEND
    nkentseu::platform::graphics::NkGraphicsApi SelectGraphicsAPI() {
        using namespace nkentseu::platform::graphics;

        const char* envBackend = nkentseu::platform::GetEnvVar("NK_GFX_BACKEND");

        if (envBackend) {
            // Override manuel par l'utilisateur
            #if defined(NKENTSEU_GRAPHICS_VULKAN_AVAILABLE)
                if (strcmp(envBackend, "vulkan") == 0) return NkGraphicsApi::NK_GFX_API_VULKAN;
            #endif
            #if defined(NKENTSEU_GRAPHICS_METAL_AVAILABLE)
                if (strcmp(envBackend, "metal") == 0) return NkGraphicsApi::NK_GFX_API_METAL;
            #endif
            #if defined(NKENTSEU_GRAPHICS_D3D12_AVAILABLE)
                if (strcmp(envBackend, "d3d12") == 0) return NkGraphicsApi::NK_GFX_API_D3D12;
            #endif
            #if defined(NKENTSEU_GRAPHICS_OPENGL_AVAILABLE)
                if (strcmp(envBackend, "opengl") == 0) return NkGraphicsApi::NK_GFX_API_OPENGL;
            #endif

            NK_LOG_WARNING("Unknown graphics backend '%s' in NK_GFX_BACKEND", envBackend);
        }

        // Fallback vers l'API par défaut détectée
        return NKENTSEU_GRAPHICS_DEFAULT;
    }
*/

#endif // NKENTSEU_PLATFORM_NKCGXDETECT_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================