// -----------------------------------------------------------------------------
// FICHIER: Core\Nkentseu\src\Nkentseu\Platform\NkCGXDetect.h
// DESCRIPTION: Détection GPU, APIs graphiques et calcul (CUDA/OpenCL)
// AUTEUR: Rihen
// DATE: 2026-02-10
// VERSION: 2.0.1
// MODIFICATIONS: Intégration avec NkPlatformDetect.h et NkArchDetect.h
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKCGXDETECT_H_INCLUDED
#define NKENTSEU_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKCGXDETECT_H_INCLUDED

// ============================================================
// INCLUDES
// ============================================================

#include "NkPlatformDetect.h"
#include "NkArchDetect.h"

// ============================================================
// DÉTECTION APIS GRAPHIQUES PAR PLATEFORME
// ============================================================

/**
 * @defgroup NkGraphicsAPIDetection Détection API Graphique
 * @brief Macros pour détecter les APIs graphiques disponibles
 *
 * Ces macros sont automatiquement définies selon la plateforme détectée
 * par NkPlatformDetect.h. Elles permettent de conditionner le code selon
 * les APIs graphiques disponibles.
 */

// ============================================================
// WINDOWS - Direct3D + Vulkan + OpenGL
// ============================================================

NKENTSEU_WINDOWS_ONLY(
/**
 * @brief Direct3D 11 disponible sur Windows
 * @ingroup NkGraphicsAPIDetection
 */
#define NKENTSEU_GRAPHICS_D3D11_AVAILABLE

/**
 * @brief Direct3D 12 disponible sur Windows 10+
 * @ingroup NkGraphicsAPIDetection
 */
#define NKENTSEU_GRAPHICS_D3D12_AVAILABLE

/**
 * @brief OpenGL disponible sur Windows
 * @ingroup NkGraphicsAPIDetection
 */
#define NKENTSEU_GRAPHICS_OPENGL_AVAILABLE

/**
 * @brief Vulkan disponible sur Windows
 * @ingroup NkGraphicsAPIDetection
 */
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
/**
 * @brief Metal disponible sur macOS
 * @ingroup NkGraphicsAPIDetection
 */
#define NKENTSEU_GRAPHICS_METAL_AVAILABLE

/**
 * @brief OpenGL disponible sur macOS (deprecated depuis 10.14)
 * @ingroup NkGraphicsAPIDetection
 */
#define NKENTSEU_GRAPHICS_OPENGL_AVAILABLE
#define NKENTSEU_GRAPHICS_OPENGL_DEPRECATED

/**
 * @brief Vulkan disponible via MoltenVK
 * @ingroup NkGraphicsAPIDetection
 */
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
/**
 * @brief OpenGL ES 3.x disponible sur Android
 * @ingroup NkGraphicsAPIDetection
 */
#define NKENTSEU_GRAPHICS_GLES3_AVAILABLE

/**
 * @brief Vulkan disponible sur Android (API 24+)
 * @ingroup NkGraphicsAPIDetection
 */
#define NKENTSEU_GRAPHICS_VULKAN_AVAILABLE
)

// ============================================================
// WEB - WebGL + WebGPU
// ============================================================

NKENTSEU_EMSCRIPTEN_ONLY(
/**
 * @brief WebGL 2.0 disponible via Emscripten
 * @ingroup NkGraphicsAPIDetection
 */
#define NKENTSEU_GRAPHICS_WEBGL2_AVAILABLE

/**
 * @brief WebGPU disponible (expérimental)
 * @ingroup NkGraphicsAPIDetection
 */
#define NKENTSEU_GRAPHICS_WEBGPU_AVAILABLE
)

// ============================================================
// CONSOLES PLAYSTATION
// ============================================================

NKENTSEU_PS5_ONLY(
/**
 * @brief GNM disponible sur PS5
 * @ingroup NkGraphicsAPIDetection
 */
#define NKENTSEU_GRAPHICS_GNM_AVAILABLE

/**
 * @brief Vulkan disponible sur PS5
 * @ingroup NkGraphicsAPIDetection
 */
#define NKENTSEU_GRAPHICS_VULKAN_AVAILABLE
)

NKENTSEU_PS4_ONLY(
#define NKENTSEU_GRAPHICS_GNM_AVAILABLE
)

// ============================================================
// CONSOLES XBOX
// ============================================================

NKENTSEU_XBOX_SERIES_ONLY(
/**
 * @brief Direct3D 12 disponible sur Xbox Series X|S
 * @ingroup NkGraphicsAPIDetection
 */
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
/**
 * @brief NVN (NVIDIA API) disponible sur Switch
 * @ingroup NkGraphicsAPIDetection
 */
#define NKENTSEU_GRAPHICS_NVN_AVAILABLE

/**
 * @brief Vulkan disponible sur Switch
 * @ingroup NkGraphicsAPIDetection
 */
#define NKENTSEU_GRAPHICS_VULKAN_AVAILABLE
)

// ============================================================
// ENUMS D'APIS GRAPHIQUES
// ============================================================

namespace nkentseu {
/**
 * @brief Namespace platform.
 */
namespace platform {
/**
 * @brief Namespace graphics.
 */
namespace graphics {

/**
 * @brief Types d'API graphiques supportées
 * @ingroup GraphicsEnums
 */
enum class NkGraphicsAPI : uint8_t {
	NK_UNKNOWN = 0,

	// Desktop APIs
	NK_OPENGL,	   ///< OpenGL (Desktop)
	NK_OPENGL_ES,  ///< OpenGL ES (Mobile/Embedded)
	NK_DIRECT3D11, ///< Direct3D 11
	NK_DIRECT3D12, ///< Direct3D 12
	NK_VULKAN,	   ///< Vulkan
	NK_METAL,	   ///< Metal (Apple)

	// Web APIs
	NK_WEBGL,  ///< WebGL 1.0
	NK_WEBGL2, ///< WebGL 2.0
	NK_WEBGPU, ///< WebGPU

	// Console APIs
	NK_GNM, ///< GNM (PlayStation)
	NK_NVN, ///< NVN (Nintendo Switch)

	// Software
	NK_SOFTWARE ///< CPU Renderer
};

/**
 * @brief Fabricants de GPU
 * @ingroup GraphicsEnums
 */
enum class NkGPUVendor : uint16_t {
	NK_UNKNOWN = 0,
	NK_NVIDIA = 0x10DE,	  ///< NVIDIA Corporation
	NK_AMD = 0x1002,	  ///< AMD/ATI
	NK_INTEL = 0x8086,	  ///< Intel Corporation
	NK_ARM = 0x13B5,	  ///< ARM Holdings
	NK_QUALCOMM = 0x5143, ///< Qualcomm
	NK_APPLE = 0x106B,	  ///< Apple (M-series)
	NK_IMGTEC = 0x1010,	  ///< Imagination Technologies
	NK_BROADCOM = 0x14E4, ///< Broadcom (Raspberry Pi)
	NK_MICROSOFT = 0x1414 ///< Microsoft (Software renderer)
};

/**
 * @brief Types de GPU
 * @ingroup GraphicsEnums
 */
enum class NkGPUType : uint8_t {
	NK_UNKNOWN = 0,
	NK_DISCRETE,   ///< GPU dédié
	NK_INTEGRATED, ///< GPU intégré
	NK_VIRTUAL,	   ///< GPU virtuel
	NK_SOFTWARE	   ///< Renderer software
};

// ============================================================
// FONCTIONS TEMPLATE UTILITAIRES (IMPLÉMENTATION FUTURE)
// ============================================================

/**
 * @brief Convertit une enum NkGraphicsAPI en string
 * @param api L'API graphique
 * @return Nom de l'API en chaîne
 * @note Implémentation à fournir dans le .cpp
 */
template <typename CharT = char> inline constexpr const CharT *ToString(NkGraphicsAPI api) noexcept;

/**
 * @brief Convertit une enum NkGPUVendor en string
 * @param vendor Le fabricant de GPU
 * @return Nom du fabricant
 * @note Implémentation à fournir dans le .cpp
 */
template <typename CharT = char> inline constexpr const CharT *ToString(NkGPUVendor vendor) noexcept;

/**
 * @brief Convertit une enum NkGPUType en string
 * @param type Le type de GPU
 * @return Nom du type
 * @note Implémentation à fournir dans le .cpp
 */
template <typename CharT = char> inline constexpr const CharT *ToString(NkGPUType type) noexcept;

/**
 * @brief Vérifie si une API graphique est disponible
 * @tparam API L'API à vérifier
 * @return true si l'API est disponible
 */
template <NkGraphicsAPI API> inline constexpr bool IsAPIAvailable() noexcept;

/**
 * @brief Obtient l'API graphique par défaut pour la plateforme
 * @return L'API recommandée
 */
template <typename = void> inline constexpr NkGraphicsAPI GetDefaultAPI() noexcept;

/**
 * @brief Obtient l'API graphique moderne pour la plateforme
 * @return L'API la plus récente disponible
 */
template <typename = void> inline constexpr NkGraphicsAPI GetModernAPI() noexcept;

} // namespace graphics
} // namespace platform
} // namespace nkentseu

// ============================================================
// MACROS D'API PAR DÉFAUT SELON LA PLATEFORME
// ============================================================

/**
 * @defgroup DefaultAPIMacros Macros d'API par Défaut
 * @brief Macros définissant l'API graphique par défaut et moderne
 */

#if defined(NKENTSEU_PLATFORM_WINDOWS)
    #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsAPI::NK_DIRECT3D11
    #define NKENTSEU_GRAPHICS_MODERN nkentseu::platform::graphics::NkGraphicsAPI::NK_DIRECT3D12
#elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_FREEBSD) || defined(NKENTSEU_PLATFORM_OPENBSD) || defined(NKENTSEU_PLATFORM_NETBSD)
    #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsAPI::NK_OPENGL
    #define NKENTSEU_GRAPHICS_MODERN nkentseu::platform::graphics::NkGraphicsAPI::NK_VULKAN
#elif defined(NKENTSEU_PLATFORM_MACOS)
    #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsAPI::NK_METAL
    #define NKENTSEU_GRAPHICS_MODERN nkentseu::platform::graphics::NkGraphicsAPI::NK_METAL
#elif defined(NKENTSEU_PLATFORM_IOS) || defined(NKENTSEU_PLATFORM_TVOS) || defined(NKENTSEU_PLATFORM_WATCHOS) || defined(NKENTSEU_PLATFORM_VISIONOS)
    #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsAPI::NK_METAL
    #define NKENTSEU_GRAPHICS_MODERN nkentseu::platform::graphics::NkGraphicsAPI::NK_METAL
#elif defined(NKENTSEU_PLATFORM_ANDROID)
    #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsAPI::NK_OPENGL_ES
    #define NKENTSEU_GRAPHICS_MODERN nkentseu::platform::graphics::NkGraphicsAPI::NK_VULKAN
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsAPI::NK_WEBGL2
    #define NKENTSEU_GRAPHICS_MODERN nkentseu::platform::graphics::NkGraphicsAPI::NK_WEBGPU
#elif defined(NKENTSEU_PLATFORM_PS5)
    #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsAPI::NK_VULKAN
    #define NKENTSEU_GRAPHICS_MODERN nkentseu::platform::graphics::NkGraphicsAPI::NK_VULKAN
#elif defined(NKENTSEU_PLATFORM_PS4) || defined(NKENTSEU_PLATFORM_PS3) || defined(NKENTSEU_PLATFORM_PSP) || defined(NKENTSEU_PLATFORM_PSVITA)
    // À ajuster selon les APIs disponibles sur ces plateformes
    #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsAPI::NK_GNM
    #define NKENTSEU_GRAPHICS_MODERN nkentseu::platform::graphics::NkGraphicsAPI::NK_GNM
#elif defined(NKENTSEU_PLATFORM_XBOX_SERIES) || defined(NKENTSEU_PLATFORM_XBOXONE)
    #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsAPI::NK_DIRECT3D12
    #define NKENTSEU_GRAPHICS_MODERN nkentseu::platform::graphics::NkGraphicsAPI::NK_DIRECT3D12
#elif defined(NKENTSEU_PLATFORM_XBOX360) || defined(NKENTSEU_PLATFORM_XBOX_ORIGINAL)
    #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsAPI::NK_DIRECT3D11
    #define NKENTSEU_GRAPHICS_MODERN nkentseu::platform::graphics::NkGraphicsAPI::NK_DIRECT3D11
#elif defined(NKENTSEU_PLATFORM_SWITCH)
    #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsAPI::NK_NVN
    #define NKENTSEU_GRAPHICS_MODERN nkentseu::platform::graphics::NkGraphicsAPI::NK_VULKAN
#elif defined(NKENTSEU_PLATFORM_WIIU) || defined(NKENTSEU_PLATFORM_WII) || defined(NKENTSEU_PLATFORM_GAMECUBE) || defined(NKENTSEU_PLATFORM_N64) || defined(NKENTSEU_PLATFORM_3DS) || defined(NKENTSEU_PLATFORM_NDS) || defined(NKENTSEU_PLATFORM_GBA) || defined(NKENTSEU_PLATFORM_GAMEBOY)
    // Consoles Nintendo plus anciennes, à adapter
    #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsAPI::NK_OPENGL
    #define NKENTSEU_GRAPHICS_MODERN nkentseu::platform::graphics::NkGraphicsAPI::NK_OPENGL
#elif defined(NKENTSEU_PLATFORM_SEGA)
    #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsAPI::NK_OPENGL
    #define NKENTSEU_GRAPHICS_MODERN nkentseu::platform::graphics::NkGraphicsAPI::NK_OPENGL
#elif defined(NKENTSEU_PLATFORM_HARMONYOS)
    #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsAPI::NK_OPENGL_ES
    #define NKENTSEU_GRAPHICS_MODERN nkentseu::platform::graphics::NkGraphicsAPI::NK_VULKAN
#elif defined(NKENTSEU_PLATFORM_EMBEDDED)
    // Pour les systèmes embarqués sans GPU dédié
    #define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsAPI::NK_SOFTWARE
    #define NKENTSEU_GRAPHICS_MODERN nkentseu::platform::graphics::NkGraphicsAPI::NK_SOFTWARE
#endif

// Fallback si aucune plateforme spécifique détectée
#ifndef NKENTSEU_GRAPHICS_DEFAULT
#define NKENTSEU_GRAPHICS_DEFAULT nkentseu::platform::graphics::NkGraphicsAPI::NK_OPENGL
#define NKENTSEU_GRAPHICS_MODERN nkentseu::platform::graphics::NkGraphicsAPI::NK_VULKAN
#endif

// ============================================================
// MACROS CONDITIONNELLES POUR CHAQUE API
// ============================================================

/**
 * @defgroup GraphicsConditionalMacros Macros Conditionnelles Graphiques
 * @brief Macros pour exécuter du code selon l'API graphique
 */

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

// ============================================================
// DÉTECTION COMPUTE (CUDA, OpenCL, SYCL)
// ============================================================

/**
 * @defgroup ComputeDetection Détection API Calcul
 * @brief Macros pour détecter les APIs de calcul GPU
 */

#if defined(__CUDACC__)
/**
 * @brief CUDA disponible
 * @ingroup ComputeDetection
 */
#define NKENTSEU_COMPUTE_CUDA_AVAILABLE
#define NKENTSEU_COMPUTE_AVAILABLE
#endif

#if defined(__OPENCL_VERSION__)
/**
 * @brief OpenCL disponible
 * @ingroup ComputeDetection
 */
#define NKENTSEU_COMPUTE_OPENCL_AVAILABLE
#define NKENTSEU_COMPUTE_AVAILABLE
#endif

#if defined(__SYCL_DEVICE_ONLY__)
/**
 * @brief SYCL disponible
 * @ingroup ComputeDetection
 */
#define NKENTSEU_COMPUTE_SYCL_AVAILABLE
#define NKENTSEU_COMPUTE_AVAILABLE
#endif

// Macros conditionnelles pour compute
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

// ============================================================
// DÉTECTION SYSTÈME D'AFFICHAGE (X11, Wayland, etc.)
// ============================================================

/**
 * @defgroup DisplaySystemDetection Détection Système d'Affichage
 * @brief Macros pour détecter le système de fenêtrage
 */

NKENTSEU_LINUX_ONLY(
#if defined(WAYLAND_DISPLAY) || defined(__WAYLAND__)
/**
 * @brief Wayland disponible
 * @ingroup DisplaySystemDetection
 */
#define NKENTSEU_DISPLAY_WAYLAND
#elif defined(DISPLAY) || defined(__X11__)
#if defined(__USE_XCB)
/**
 * @brief XCB disponible
 * @ingroup DisplaySystemDetection
 */
#define NKENTSEU_DISPLAY_XCB
#else
/**
 * @brief Xlib disponible
 * @ingroup DisplaySystemDetection
 */
#define NKENTSEU_DISPLAY_XLIB
#endif
#endif
)

// ============================================================
// CONSTANTES VENDOR ID (PCI)
// ============================================================

/**
 * @defgroup VendorIDConstants Constantes Vendor ID
 * @brief IDs PCI des fabricants de GPU
 */

#define NKENTSEU_GPU_VENDOR_NVIDIA_ID 0x10DE	///< NVIDIA
#define NKENTSEU_GPU_VENDOR_AMD_ID 0x1002		///< AMD/ATI
#define NKENTSEU_GPU_VENDOR_INTEL_ID 0x8086		///< Intel
#define NKENTSEU_GPU_VENDOR_ARM_ID 0x13B5		///< ARM
#define NKENTSEU_GPU_VENDOR_QUALCOMM_ID 0x5143	///< Qualcomm
#define NKENTSEU_GPU_VENDOR_APPLE_ID 0x106B		///< Apple
#define NKENTSEU_GPU_VENDOR_IMGTEC_ID 0x1010	///< Imagination Tech
#define NKENTSEU_GPU_VENDOR_BROADCOM_ID 0x14E4	///< Broadcom
#define NKENTSEU_GPU_VENDOR_MICROSOFT_ID 0x1414 ///< Microsoft

// ============================================================
// CONFIGURATION API GRAPHIQUE ACTIVE
// ============================================================

/**
 * @defgroup GraphicsConfigMacros Configuration API Graphique Active
 * @brief Macros pour sélectionner l'API graphique active
 */

#define NKENTSEU_GFX_NONE 0
#define NKENTSEU_GFX_VULKAN 1
#define NKENTSEU_GFX_METAL 2
#define NKENTSEU_GFX_DIRECTX 3
#define NKENTSEU_GFX_OPENGL 4
#define NKENTSEU_GFX_SOFTWARE 5

/**
 * @brief Calcule une version encodée (major << 16) | minor
 * @ingroup GraphicsConfigMacros
 */
#define NKENTSEU_GFX_VERSION_CALC(major, minor) ((major << 16) | (minor & 0xFFFF))

// Détection automatique de l'API active
#if defined(NKENTSEU_GFX_FORCE)
// Force manuelle de l'API
#define NKENTSEU_GFX_ACTIVE NKENTSEU_GFX_FORCE

#else
	// Détection automatique selon la plateforme
	#if defined(NKENTSEU_PLATFORM_MACOS)
        #if __has_include(<Metal/Metal.h>)
            #define NKENTSEU_GFX_ACTIVE NKENTSEU_GFX_METAL
            #define NKENTSEU_GFX_VERSION 3
        #endif
    #elif defined(NKENTSEU_PLATFORM_WINDOWS)
        #if defined(_DIRECTX12_)
            #define NKENTSEU_GFX_ACTIVE NKENTSEU_GFX_DIRECTX
            #define NKENTSEU_GFX_VERSION 12
        #elif defined(_DIRECTX11_)
            #define NKENTSEU_GFX_ACTIVE NKENTSEU_GFX_DIRECTX
            #define NKENTSEU_GFX_VERSION 11
        #endif
    #endif

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
#elif __has_include(<GL/gl.h>)
#define NKENTSEU_GFX_ACTIVE NKENTSEU_GFX_OPENGL
#if defined(GL_VERSION_4_6)
#define NKENTSEU_GFX_VERSION NKENTSEU_GFX_VERSION_CALC(4, 6)
#elif defined(GL_VERSION_4_5)
#define NKENTSEU_GFX_VERSION NKENTSEU_GFX_VERSION_CALC(4, 5)
#elif defined(GL_VERSION_3_3)
#define NKENTSEU_GFX_VERSION NKENTSEU_GFX_VERSION_CALC(3, 3)
#else
#define NKENTSEU_GFX_VERSION NKENTSEU_GFX_VERSION_CALC(4, 5)
#endif
#else
#define NKENTSEU_GFX_ACTIVE NKENTSEU_GFX_SOFTWARE
#define NKENTSEU_GFX_VERSION NKENTSEU_GFX_VERSION_CALC(1, 0)
#endif
#endif
#endif

// Validation
#if !defined(NKENTSEU_GFX_ACTIVE) || NKENTSEU_GFX_ACTIVE == NKENTSEU_GFX_NONE
#warning "Nkentseu: Aucune API graphique valide détectée, utilisation du renderer software"
#define NKENTSEU_GFX_ACTIVE NKENTSEU_GFX_SOFTWARE
#define NKENTSEU_GFX_VERSION NKENTSEU_GFX_VERSION_CALC(1, 0)
#endif

// ============================================================
// MACROS DE DEBUG
// ============================================================

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
#endif

#endif // NKENTSEU_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKCGXDETECT_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================