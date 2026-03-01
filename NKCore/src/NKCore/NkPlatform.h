// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\NkPlatform.h
// DESCRIPTION: Informations plateforme runtime et utilitaires système
// AUTEUR: Rihen
// DATE: 2026-02-10
// VERSION: 3.0.0
// MODIFICATIONS: Réimplémentation complète avec types nk_xxx, organisation améliorée
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKPLATFORM_H_INCLUDED
#define NKENTSEU_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKPLATFORM_H_INCLUDED

// ============================================================
// INCLUDES
// ============================================================

#include "NkPlatformDetect.h"
#include "NkArchDetect.h"
#include "NkCompilerDetect.h"
#include "NkTypes.h"
#include "NkExport.h"
#include "NkCoreExport.h"
#include "NkVersion.h"

#include <cstddef>

// ============================================================
// MACROS BUILTIN
// ============================================================

#if defined(NKENTSEU_COMPILER_MSVC)
// MSVC supporte les builtins depuis VS 2019 16.6 (/_MSC_VER >= 1926)
#if _MSC_VER >= 1926
#define NKENTSEU_BUILTIN_FILE __builtin_FILE()
#define NKENTSEU_BUILTIN_FUNCTION __builtin_FUNCTION()
#define NKENTSEU_BUILTIN_LINE __builtin_LINE()
#define NKENTSEU_BUILTIN_COLUMN __builtin_COLUMN()
#else
// Fallback pour les versions très anciennes de MSVC
#define NKENTSEU_BUILTIN_FILE __FILE__
#define NKENTSEU_BUILTIN_FUNCTION "unknown" // __FUNCTION__ ici causerait l'erreur 1036
#define NKENTSEU_BUILTIN_LINE __LINE__
#define NKENTSEU_BUILTIN_COLUMN 0
#endif
#else
// GCC et Clang
#define NKENTSEU_BUILTIN_FILE __builtin_FILE()
#define NKENTSEU_BUILTIN_FUNCTION __builtin_FUNCTION()
#define NKENTSEU_BUILTIN_LINE __builtin_LINE()

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
 * @brief Capture la localisation actuelle
 */
#define NkCurrentSourceLocation                                                                                        \
	nkentseu::core::platform::MkSourceLocation::Current(NKENTSEU_BUILTIN_FILE, NKENTSEU_BUILTIN_FUNCTION,              \
														NKENTSEU_BUILTIN_LINE, NKENTSEU_BUILTIN_COLUMN)

#define NkCurrentLocation()                                                                                            \
	nkentseu::core::NkSourceLocation::Current(NKENTSEU_BUILTIN_FILE, __FUNCTION__, NKENTSEU_BUILTIN_LINE,              \
											  NKENTSEU_BUILTIN_COLUMN)

// ============================================================
// NAMESPACE NKENTSEU::PLATFORM
// ============================================================

namespace nkentseu {
/**
 * @brief Namespace core.
 */
namespace core {
/**
 * @brief Namespace platform.
 */
namespace platform {

// ============================================================
// TYPES ET ENUMS - INFORMATIONS PLATEFORME
// ============================================================

/**
 * @brief Type pour les numéros de version
 * @ingroup PlatformTypes
 */
using NkVersion = nk_uint32;

/**
 * @brief Types de plateformes supportées
 * @ingroup PlatformTypes
 */
enum class NkPlatformType : nk_uint8 {
	NK_UNKNOWN = 0,
	NK_WINDOWS,
	NK_LINUX,
	NK_BSD,
	NK_MACOS,
	NK_IOS,
	NK_ANDROID,
	NK_HARMONYOS,
	NK_NINTENDO_SWITCH,
	NK_NINTENDO_DS,
	NK_PSP,
	NK_EMSCRIPTEN,
	NK_WSL,
	NK_WATCHOS,
	NK_TVOS,
	NK_FREEBSD,
	NK_NETBSD,
	NK_OPENBSD,
	NK_DRAGONFLYBSD
};

/**
 * @brief Types d'architectures CPU
 * @ingroup PlatformTypes
 */
enum class NkArchitectureType : nk_uint8 {
	NK_UNKNOWN = 0,
	NK_X86,
	NK_X64,
	NK_ARM32,
	NK_ARM64,
	NK_MIPS,
	NK_RISCV32,
	NK_RISCV64,
	NK_PPC32,
	NK_PPC64,
	NK_WASM,
	NK_ARM9
};

/**
 * @brief Types de systèmes d'affichage
 * @ingroup PlatformTypes
 */
enum class NkDisplayType : nk_uint8 {
	NK_NONE = 0,
	NK_WIN32,
	NK_COCOA,
	NK_ANDROID,
	NK_HARMONY_OS,
	NK_WAYLAND,
	NK_XCB,
	NK_XLIB
};

/**
 * @brief Types d'APIs graphiques
 * @ingroup PlatformTypes
 */
enum class NkGraphicsAPI : nk_uint8 { NK_NONE = 0, NK_VULKAN, NK_METAL, NK_OPENGL, NK_DIRECTX, NK_SOFTWARE };

/**
 * @brief Type d'endianness
 * @ingroup PlatformTypes
 */
enum class NkEndianness : nk_uint8 { NK_UNKNOWN = 0, NK_LITTLE = 1, NK_BIG = 2 };

// ============================================================
// STRUCTURES - INFORMATIONS VERSION
// ============================================================

/**
 * @brief Informations de version (Major.Minor.Patch)
 * @ingroup PlatformStructs
 */
struct NKENTSEU_CORE_API NkVersionInfo {
	nk_uint32 major;			  ///< Version majeure
	nk_uint32 minor;			  ///< Version mineure
	nk_uint32 patch;			  ///< Version patch
	const nk_char *versionString; ///< Version en chaîne (ex: "1.0.0")
};

// ============================================================
// STRUCTURES - INFORMATIONS PLATEFORME COMPLÈTES
// ============================================================

/**
 * @brief Informations complètes sur la plateforme runtime
 * @ingroup PlatformStructs
 *
 * Cette structure contient toutes les informations détectées au runtime
 * concernant le système d'exploitation, l'architecture CPU, les capacités
 * SIMD, la mémoire disponible, etc.
 */
struct NKENTSEU_CORE_API NkPlatformInfo {
	// --------------------------------------------------------
	// Informations OS et Compilateur
	// --------------------------------------------------------
	NkPlatformType platform;		 ///< Type de plateforme
	NkArchitectureType architecture; ///< Type d'architecture CPU
	const nk_char *osName;			 ///< Nom de l'OS
	const nk_char *osVersion;		 ///< Version de l'OS
	const nk_char *archName;		 ///< Nom de l'architecture
	const nk_char *compilerName;	 ///< Nom du compilateur
	const nk_char *compilerVersion;	 ///< Version du compilateur

	// --------------------------------------------------------
	// Informations CPU
	// --------------------------------------------------------
	nk_uint32 cpuCoreCount;	  ///< Nombre de cœurs physiques
	nk_uint32 cpuThreadCount; ///< Nombre de threads matériels
	nk_uint32 cpuL1CacheSize; ///< Taille cache L1 (bytes)
	nk_uint32 cpuL2CacheSize; ///< Taille cache L2 (bytes)
	nk_uint32 cpuL3CacheSize; ///< Taille cache L3 (bytes)
	nk_uint32 cacheLineSize;  ///< Taille ligne cache (bytes)

	// --------------------------------------------------------
	// Informations Mémoire
	// --------------------------------------------------------
	nk_uint64 totalMemory;			 ///< RAM totale (bytes)
	nk_uint64 availableMemory;		 ///< RAM disponible (bytes)
	nk_uint32 pageSize;				 ///< Taille page mémoire (bytes)
	nk_uint32 allocationGranularity; ///< Granularité allocation

	// --------------------------------------------------------
	// Support SIMD
	// --------------------------------------------------------
	nk_bool hasSSE;	   ///< Support SSE
	nk_bool hasSSE2;   ///< Support SSE2
	nk_bool hasSSE3;   ///< Support SSE3
	nk_bool hasSSE4_1; ///< Support SSE4.1
	nk_bool hasSSE4_2; ///< Support SSE4.2
	nk_bool hasAVX;	   ///< Support AVX
	nk_bool hasAVX2;   ///< Support AVX2
	nk_bool hasAVX512; ///< Support AVX-512
	nk_bool hasNEON;   ///< Support NEON (ARM)

	// --------------------------------------------------------
	// Caractéristiques Plateforme
	// --------------------------------------------------------
	nk_bool isLittleEndian;	 ///< Little endian ?
	nk_bool is64Bit;		 ///< Architecture 64-bit ?
	NkEndianness endianness; ///< Type d'endianness

	// --------------------------------------------------------
	// Informations Build
	// --------------------------------------------------------
	nk_bool isDebugBuild;	  ///< Build debug ?
	nk_bool isSharedLibrary;  ///< Bibliothèque partagée ?
	const nk_char *buildType; ///< Type de build (Debug/Release)

	// --------------------------------------------------------
	// Capacités Plateforme
	// --------------------------------------------------------
	nk_bool hasThreading;	  ///< Support threading
	nk_bool hasVirtualMemory; ///< Support mémoire virtuelle
	nk_bool hasFileSystem;	  ///< Support système de fichiers
	nk_bool hasNetwork;		  ///< Support réseau

	// --------------------------------------------------------
	// Affichage et Graphiques
	// --------------------------------------------------------
	NkDisplayType display;			  ///< Type de système d'affichage
	NkGraphicsAPI graphicsApi;		  ///< API graphique détectée
	NkVersionInfo graphicsApiVersion; ///< Version de l'API graphique
};

// ============================================================
// CLASSE UTILITAIRE - SOURCE LOCATION
// ============================================================

/**
 * @brief Informations de localisation dans le code source
 * @ingroup PlatformUtils
 *
 * Utilisé pour le logging, debugging et gestion d'erreurs.
 * Capture automatiquement le fichier, la fonction, la ligne et la colonne.
 */
class NKENTSEU_CORE_API NkSourceLocation {
public:
	/**
	 * @brief Constructeur par défaut
	 */
	constexpr NkSourceLocation() noexcept = default;

	/**
	 * @brief Récupère le nom du fichier source
	 * @return Nom du fichier
	 */
	constexpr const nk_char *FileName() const noexcept {
		return mFile;
	}

	/**
	 * @brief Récupère le nom de la fonction
	 * @return Nom de la fonction
	 */
	constexpr const nk_char *FunctionName() const noexcept {
		return mFunction;
	}

	/**
	 * @brief Récupère le numéro de ligne
	 * @return Numéro de ligne
	 */
	constexpr nk_uint32 Line() const noexcept {
		return mLine;
	}

	/**
	 * @brief Récupère le numéro de colonne
	 * @return Numéro de colonne
	 */
	constexpr nk_uint32 Column() const noexcept {
		return mColumn;
	}

	/**
	 * @brief Crée une instance avec la localisation actuelle
	 * @param file Nom du fichier
	 * @param function Nom de la fonction
	 * @param line Numéro de ligne
	 * @param column Numéro de colonne
	 * @return Instance de SourceLocation
	 */
	static constexpr NkSourceLocation Current(const nk_char *file = NKENTSEU_BUILTIN_FILE,
											  const nk_char *function = NKENTSEU_BUILTIN_FUNCTION,
											  nk_uint32 line = NKENTSEU_BUILTIN_LINE,
											  nk_uint32 column = NKENTSEU_BUILTIN_COLUMN) noexcept {
		NkSourceLocation loc;
		loc.mFile = file;
		loc.mFunction = function;
		loc.mLine = line;
		loc.mColumn = column;
		return loc;
	}

private:
	const nk_char *mFile = "unknown";	  ///< Nom du fichier
	const nk_char *mFunction = "unknown"; ///< Nom de la fonction
	nk_uint32 mLine = 0;				  ///< Numéro de ligne
	nk_uint32 mColumn = 0;				  ///< Numéro de colonne
};

} // namespace platform
} // namespace core
} // namespace nkentseu

// ============================================================
// API C - FONCTIONS RUNTIME PLATEFORME
// ============================================================

typedef nkentseu::core::platform::NkPlatformInfo NkPlatformInfo;

NKENTSEU_EXTERN_C_BEGIN

/**
 * @defgroup PlatformAPI API Plateforme Runtime
 * @brief Fonctions pour récupérer les informations plateforme
 */

/**
 * @brief Récupère les informations complètes de la plateforme
 * @return Référence constante vers PlatformInfo
 * @note Thread-safe, initialisation automatique au premier appel
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API const nkentseu::core::platform::NkPlatformInfo* NkGetPlatformInfo();

/**
 * @brief Initialise explicitement les informations plateforme
 * @note Appelé automatiquement par NkGetPlatformInfo()
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API void NkInitializePlatformInfo();

/**
 * @brief Récupère le nom de la plateforme
 * @return Nom de la plateforme (ex: "Windows", "Linux", "macOS")
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API const nkentseu::core::nk_char* NkGetPlatformName();

/**
 * @brief Récupère le nom de l'architecture
 * @return Nom de l'architecture (ex: "x86_64", "ARM64")
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API const nkentseu::core::nk_char* NkGetArchitectureName();

/**
 * @brief Vérifie le support d'une fonctionnalité SIMD
 * @param feature Nom de la fonctionnalité (ex: "SSE", "AVX", "NEON")
 * @return true si supportée, false sinon
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::core::nk_bool NkHasSIMDFeature(const nkentseu::core::nk_char *feature);

// --------------------------------------------------------
// Fonctions CPU
// --------------------------------------------------------

/**
 * @brief Récupère le nombre de cœurs CPU physiques
 * @return Nombre de cœurs
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::core::nk_uint32 NkGetCPUCoreCount();

/**
 * @brief Récupère le nombre total de threads matériels
 * @return Nombre de threads (incluant hyperthreading)
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::core::nk_uint32 NkGetCPUThreadCount();

/**
 * @brief Récupère la taille du cache L1
 * @return Taille en bytes
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::core::nk_uint32 NkGetL1CacheSize();

/**
 * @brief Récupère la taille du cache L2
 * @return Taille en bytes
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::core::nk_uint32 NkGetL2CacheSize();

/**
 * @brief Récupère la taille du cache L3
 * @return Taille en bytes
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::core::nk_uint32 NkGetL3CacheSize();

/**
 * @brief Récupère la taille de ligne de cache
 * @return Taille en bytes
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::core::nk_uint32 NkGetCacheLineSize();

// --------------------------------------------------------
// Fonctions Mémoire
// --------------------------------------------------------

/**
 * @brief Récupère la RAM totale du système
 * @return Taille en bytes
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::core::nk_uint64 NkGetTotalMemory();

/**
 * @brief Récupère la RAM disponible
 * @return Taille en bytes
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::core::nk_uint64 NkGetAvailableMemory();

/**
 * @brief Récupère la taille de page mémoire
 * @return Taille en bytes
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::core::nk_uint32 NkGetPageSize();

/**
 * @brief Récupère la granularité d'allocation mémoire
 * @return Taille en bytes
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::core::nk_uint32 NkGetAllocationGranularity();

// --------------------------------------------------------
// Fonctions Build et Configuration
// --------------------------------------------------------

/**
 * @brief Vérifie si c'est un build debug
 * @return true si debug, false sinon
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::core::nk_bool NkIsDebugBuild();

/**
 * @brief Vérifie si c'est une bibliothèque partagée
 * @return true si partagée, false si statique
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::core::nk_bool NkIsSharedLibrary();

/**
 * @brief Récupère le type de build
 * @return "Debug", "Release", "RelWithDebInfo", etc.
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API const nkentseu::core::nk_char* NkGetBuildType();

// --------------------------------------------------------
// Fonctions Endianness et Architecture
// --------------------------------------------------------

/**
 * @brief Détecte l'endianness du système
 * @return Type d'endianness
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::core::platform::NkEndianness NkGetEndianness();

/**
 * @brief Vérifie si l'architecture est 64-bit
 * @return true si 64-bit, false si 32-bit
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::core::nk_bool NkIs64Bit();

// --------------------------------------------------------
// Fonctions Utilitaires
// --------------------------------------------------------

/**
 * @brief Affiche les informations plateforme sur stdout
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API void NkPrintPlatformInfo();

/**
 * @brief Vérifie si une adresse est alignée
 * @param address Adresse à vérifier
 * @param alignment Alignement requis (doit être puissance de 2)
 * @return true si alignée, false sinon
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::core::nk_bool NkIsAligned(const nkentseu::core::nk_ptr address,
														nkentseu::core::nk_size alignment);

/**
 * @brief Aligne une adresse vers le haut
 * @param address Adresse à aligner
 * @param alignment Alignement requis (doit être puissance de 2)
 * @return Adresse alignée
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::core::nk_ptr NkAlignAddress(nkentseu::core::nk_ptr address,
														  nkentseu::core::nk_size alignment);

/**
 * @brief Aligne une adresse constante vers le haut
 * @param address Adresse à aligner
 * @param alignment Alignement requis (doit être puissance de 2)
 * @return Adresse alignée
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API const nkentseu::core::nk_ptr NkAlignAddressConst(const nkentseu::core::nk_ptr address,
																	 nkentseu::core::nk_size alignment);

NKENTSEU_EXTERN_C_END

// ============================================================
// NAMESPACE NKENTSEU::PLATFORM - FONCTIONS INLINE
// ============================================================

namespace nkentseu {
/**
 * @brief Namespace core.
 */
namespace core {
/**
 * @brief Namespace platform.
 */
namespace platform {

/**
 * @defgroup PlatformInlineUtils Utilitaires Inline Plateforme
 * @brief Fonctions inline pour informations plateforme compile-time
 */

/**
 * @brief Retourne le nom de la plateforme (compile-time)
 * @return Nom de la plateforme
 * @ingroup PlatformInlineUtils
 */
inline const nk_char *NkGetPlatformName() noexcept {
	return NKENTSEU_PLATFORM_NAME;
}

/**
 * @brief Retourne la version/description de la plateforme
 * @return Description de la plateforme
 * @ingroup PlatformInlineUtils
 */
inline const nk_char *NkGetPlatformVersion() noexcept {
	return NKENTSEU_PLATFORM_VERSION;
}

/**
 * @brief Vérifie si la plateforme est desktop
 * @return true si desktop, false sinon
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
 * @brief Vérifie si la plateforme est mobile
 * @return true si mobile, false sinon
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
 * @brief Vérifie si la plateforme est une console
 * @return true si console, false sinon
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
 * @brief Vérifie si la plateforme est embarquée
 * @return true si embarqué, false sinon
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
 * @brief Vérifie si la plateforme est web
 * @return true si web, false sinon
 * @ingroup PlatformInlineUtils
 */
inline nk_bool NkIsWeb() noexcept {
#ifdef NKENTSEU_PLATFORM_WEB
	return true;
#else
	return false;
#endif
}

// ============================================================
// NAMESPACE ARCH - FONCTIONS ARCHITECTURE
// ============================================================

namespace arch {

/**
 * @brief Retourne le nom de l'architecture
 * @return Nom de l'architecture
 */
inline const nk_char *NkGetArchName() noexcept {
	return NKENTSEU_ARCH_NAME;
}

/**
 * @brief Retourne la version de l'architecture
 * @return Version de l'architecture
 */
inline const nk_char *NkGetArchVersion() noexcept {
	return NKENTSEU_ARCH_VERSION;
}

/**
 * @brief Vérifie si l'architecture est 64-bit
 * @return true si 64-bit, false sinon
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
 * @return true si 32-bit, false sinon
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
 * @return true si little-endian, false sinon
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
 * @return true si big-endian, false sinon
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
 * @return Taille en bytes
 */
inline nk_uint32 NkGetCacheLineSize() noexcept {
	return NKENTSEU_CACHE_LINE_SIZE;
}

/**
 * @brief Retourne la taille de page mémoire
 * @return Taille en bytes
 */
inline nk_uint32 NkGetPageSize() noexcept {
	return NKENTSEU_PAGE_SIZE;
}

/**
 * @brief Retourne la taille de mot machine
 * @return Taille en bytes
 */
inline nk_uint32 NkGetWordSize() noexcept {
	return NKENTSEU_WORD_SIZE;
}

/**
 * @brief Aligne une adresse vers le haut
 * @tparam T Type du pointeur
 * @param addr Adresse à aligner
 * @param align Alignement (puissance de 2)
 * @return Adresse alignée
 */
template <typename T> inline T *NkAlignUp(T *addr, nk_size align) noexcept {
	return reinterpret_cast<T *>((reinterpret_cast<nk_uintptr>(addr) + (align - 1)) & ~(align - 1));
}

/**
 * @brief Aligne une adresse vers le bas
 * @tparam T Type du pointeur
 * @param addr Adresse à aligner
 * @param align Alignement (puissance de 2)
 * @return Adresse alignée
 */
template <typename T> inline T *NkAlignDown(T *addr, nk_size align) noexcept {
	return reinterpret_cast<T *>(reinterpret_cast<nk_uintptr>(addr) & ~(align - 1));
}

/**
 * @brief Vérifie si une adresse est alignée
 * @tparam T Type du pointeur
 * @param addr Adresse à vérifier
 * @param align Alignement (puissance de 2)
 * @return true si alignée, false sinon
 */
template <typename T> inline nk_bool NkIsAligned(const T *addr, nk_size align) noexcept {
	return (reinterpret_cast<nk_uintptr>(addr) & (align - 1)) == 0;
}

/**
 * @brief Calcule le padding nécessaire pour l'alignement
 * @tparam T Type du pointeur
 * @param addr Adresse à vérifier
 * @param align Alignement (puissance de 2)
 * @return Nombre de bytes de padding
 */
template <typename T> inline nk_size NkCalculatePadding(const T *addr, nk_size align) noexcept {
	nk_uintptr mask = align - 1;
	nk_uintptr misalignment = reinterpret_cast<nk_uintptr>(addr) & mask;
	return misalignment ? (align - misalignment) : 0;
}

} // namespace arch

// ============================================================
// NAMESPACE MEMORY - ALLOCATION ALIGNÉE
// ============================================================

namespace memory {

/**
 * @brief Alloue de la mémoire alignée
 * @param size Taille à allouer
 * @param alignment Alignement requis (puissance de 2)
 * @return Pointeur vers mémoire alignée, nullptr en cas d'erreur
 */
NKENTSEU_CORE_API core::nk_ptr NkAllocateAligned(nk_size size, nk_size alignment) noexcept;

/**
 * @brief Libère de la mémoire allouée avec AllocateAligned
 * @param ptr Pointeur à libérer
 */
NKENTSEU_CORE_API void NkFreeAligned(core::nk_ptr ptr) noexcept;

/**
 * @brief Vérifie si un pointeur est aligné
 * @param ptr Pointeur à vérifier
 * @param alignment Alignement à tester
 * @return true si aligné, false sinon
 */
NKENTSEU_CORE_API nk_bool NkIsPointerAligned(const core::nk_ptr ptr, nk_size alignment) noexcept;

/**
 * @brief Alloue un tableau d'éléments alignés
 * @tparam T Type des éléments
 * @param count Nombre d'éléments
 * @param alignment Alignement requis
 * @return Pointeur vers le tableau
 */
template <typename T> inline T *NkAllocateAlignedArray(nk_size count, nk_size alignment) noexcept {
	nk_size size = count * sizeof(T);
	core::nk_ptr ptr = NkAllocateAligned(size, alignment);
	return static_cast<T *>(ptr);
}

/**
 * @brief Construit un objet dans de la mémoire alignée
 * @tparam T Type à construire
 * @tparam Args Types des arguments
 * @param ptr Mémoire alignée
 * @param args Arguments pour le constructeur
 * @return Pointeur vers l'objet construit
 */
template <typename T, typename... Args> inline T *NkConstructAligned(core::nk_ptr ptr, Args &&...args) noexcept {
	return new (ptr) T(static_cast<Args &&>(args)...);
}

/**
 * @brief Détruit un objet dans de la mémoire alignée
 * @tparam T Type à détruire
 * @param ptr Pointeur vers l'objet
 */
template <typename T> inline void NkDestroyAligned(T *ptr) noexcept {
	if (ptr) {
		ptr->~T();
	}
}

} // namespace memory
} // namespace platform
} // namespace core
} // namespace nkentseu

// ============================================================
// MACROS DE CONVENANCE
// ============================================================

/**
 * @brief Alloue mémoire alignée sur cache line
 */
#define NkAllocAligned(size) nkentseu::core::platform::memory::NkAllocateAligned((size), NKENTSEU_CACHE_LINE_SIZE)

/**
 * @brief Alloue mémoire alignée sur page
 */
#define NkAllocPageAligned(size) nkentseu::core::platform::memory::NkAllocateAligned((size), NKENTSEU_PAGE_SIZE)

/**
 * @brief Libère mémoire alignée
 */
#define NkFreeAligned(ptr) nkentseu::core::platform::memory::NkFreeAligned(ptr)

#endif // NKENTSEU_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKPLATFORM_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================
