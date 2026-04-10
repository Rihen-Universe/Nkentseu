// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\NkPlatform.h
// DESCRIPTION: Informations plateforme runtime et utilitaires systÃ¨me
// AUTEUR: Rihen
// DATE: 2026-02-10
// VERSION: 3.0.0
// MODIFICATIONS: RÃ©implÃ©mentation complÃ¨te avec types nk_xxx, organisation amÃ©liorÃ©e
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKPLATFORM_H_INCLUDED
#define NKENTSEU_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKPLATFORM_H_INCLUDED

// ============================================================
// INCLUDES
// ============================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKPlatform/NkArchDetect.h"
#include "NKPlatform/NkCompilerDetect.h"
#include "NKPlatform/NkEndianness.h"
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
		// Fallback pour les versions trÃ¨s anciennes de MSVC
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
	nkentseu::platform::MkSourceLocation::Current(NKENTSEU_BUILTIN_FILE, NKENTSEU_BUILTIN_FUNCTION,              \
														NKENTSEU_BUILTIN_LINE, NKENTSEU_BUILTIN_COLUMN)

#define NkCurrentLocation()                                                                                            \
	nkentseu::NkSourceLocation::Current(NKENTSEU_BUILTIN_FILE, __FUNCTION__, NKENTSEU_BUILTIN_LINE,              \
											  NKENTSEU_BUILTIN_COLUMN)

// ============================================================
// NAMESPACE NKENTSEU::PLATFORM
// ============================================================

namespace nkentseu {
	/**
	 * @brief Namespace platform.
	 */
	namespace platform {

		// ============================================================
		// TYPES ET ENUMS - INFORMATIONS PLATEFORME
		// ============================================================

		/**
		 * @brief Type pour les numÃ©ros de version
		 * @ingroup PlatformTypes
		 */
		using NkVersion = nk_uint32;

		/**
		 * @brief Types de plateformes supportÃ©es
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
		 * @brief Types de systÃ¨mes d'affichage
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
			const nk_char *versionString; ///< Version en chaÃ®ne (ex: "1.0.0")
		};

		// ============================================================
		// STRUCTURES - INFORMATIONS PLATEFORME COMPLÃˆTES
		// ============================================================

		/**
		 * @brief Informations complÃ¨tes sur la plateforme runtime
		 * @ingroup PlatformStructs
		 *
		 * Cette structure contient toutes les informations dÃ©tectÃ©es au runtime
		 * concernant le systÃ¨me d'exploitation, l'architecture CPU, les capacitÃ©s
		 * SIMD, la mÃ©moire disponible, etc.
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
			nk_uint32 cpuCoreCount;	  ///< Nombre de cÅ“urs physiques
			nk_uint32 cpuThreadCount; ///< Nombre de threads matÃ©riels
			nk_uint32 cpuL1CacheSize; ///< Taille cache L1 (bytes)
			nk_uint32 cpuL2CacheSize; ///< Taille cache L2 (bytes)
			nk_uint32 cpuL3CacheSize; ///< Taille cache L3 (bytes)
			nk_uint32 cacheLineSize;  ///< Taille ligne cache (bytes)

			// --------------------------------------------------------
			// Informations MÃ©moire
			// --------------------------------------------------------
			nk_uint64 totalMemory;			 ///< RAM totale (bytes)
			nk_uint64 availableMemory;		 ///< RAM disponible (bytes)
			nk_uint32 pageSize;				 ///< Taille page mÃ©moire (bytes)
			nk_uint32 allocationGranularity; ///< GranularitÃ© allocation

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
			// CaractÃ©ristiques Plateforme
			// --------------------------------------------------------
			nk_bool isLittleEndian;	 ///< Little endian ?
			nk_bool is64Bit;		 ///< Architecture 64-bit ?
			NkEndianness endianness; ///< Type d'endianness

			// --------------------------------------------------------
			// Informations Build
			// --------------------------------------------------------
			nk_bool isDebugBuild;	  ///< Build debug ?
			nk_bool isSharedLibrary;  ///< BibliothÃ¨que partagÃ©e ?
			const nk_char *buildType; ///< Type de build (Debug/Release)

			// --------------------------------------------------------
			// CapacitÃ©s Plateforme
			// --------------------------------------------------------
			nk_bool hasThreading;	  ///< Support threading
			nk_bool hasVirtualMemory; ///< Support mÃ©moire virtuelle
			nk_bool hasFileSystem;	  ///< Support systÃ¨me de fichiers
			nk_bool hasNetwork;		  ///< Support rÃ©seau

			// --------------------------------------------------------
			// Affichage et Graphiques
			// --------------------------------------------------------
			NkDisplayType display;			  ///< Type de systÃ¨me d'affichage
			NkGraphicsAPI graphicsApi;		  ///< API graphique dÃ©tectÃ©e
			NkVersionInfo graphicsApiVersion; ///< Version de l'API graphique
		};

		// ============================================================
		// CLASSE UTILITAIRE - SOURCE LOCATION
		// ============================================================

		/**
		 * @brief Informations de localisation dans le code source
		 * @ingroup PlatformUtils
		 *
		 * UtilisÃ© pour le logging, debugging et gestion d'erreurs.
		 * Capture automatiquement le fichier, la fonction, la ligne et la colonne.
		 */
		class NKENTSEU_CORE_API NkSourceLocation {
		public:
			/**
			 * @brief Constructeur par dÃ©faut
			 */
			constexpr NkSourceLocation() noexcept = default;

			/**
			 * @brief RÃ©cupÃ¨re le nom du fichier source
			 * @return Nom du fichier
			 */
			constexpr const nk_char *FileName() const noexcept {
				return mFile;
			}

			/**
			 * @brief RÃ©cupÃ¨re le nom de la fonction
			 * @return Nom de la fonction
			 */
			constexpr const nk_char *FunctionName() const noexcept {
				return mFunction;
			}

			/**
			 * @brief RÃ©cupÃ¨re le numÃ©ro de ligne
			 * @return NumÃ©ro de ligne
			 */
			constexpr nk_uint32 Line() const noexcept {
				return mLine;
			}

			/**
			 * @brief RÃ©cupÃ¨re le numÃ©ro de colonne
			 * @return NumÃ©ro de colonne
			 */
			constexpr nk_uint32 Column() const noexcept {
				return mColumn;
			}

			/**
			 * @brief CrÃ©e une instance avec la localisation actuelle
			 * @param file Nom du fichier
			 * @param function Nom de la fonction
			 * @param line NumÃ©ro de ligne
			 * @param column NumÃ©ro de colonne
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
			nk_uint32 mLine = 0;				  ///< NumÃ©ro de ligne
			nk_uint32 mColumn = 0;				  ///< NumÃ©ro de colonne
		};

		} // namespace platform

} // namespace nkentseu

// ============================================================
// API C - FONCTIONS RUNTIME PLATEFORME
// ============================================================

typedef nkentseu::platform::NkPlatformInfo NkPlatformInfo;

NKENTSEU_EXTERN_C_BEGIN

/**
 * @defgroup PlatformAPI API Plateforme Runtime
 * @brief Fonctions pour rÃ©cupÃ©rer les informations plateforme
 */

/**
 * @brief RÃ©cupÃ¨re les informations complÃ¨tes de la plateforme
 * @return RÃ©fÃ©rence constante vers PlatformInfo
 * @note Thread-safe, initialisation automatique au premier appel
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API const nkentseu::platform::NkPlatformInfo* NkGetPlatformInfo();

/**
 * @brief Initialise explicitement les informations plateforme
 * @note AppelÃ© automatiquement par NkGetPlatformInfo()
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API void NkInitializePlatformInfo();

/**
 * @brief RÃ©cupÃ¨re le nom de la plateforme
 * @return Nom de la plateforme (ex: "Windows", "Linux", "macOS")
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API const nkentseu::nk_char* NkGetPlatformName();

/**
 * @brief RÃ©cupÃ¨re le nom de l'architecture
 * @return Nom de l'architecture (ex: "x86_64", "ARM64")
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API const nkentseu::nk_char* NkGetArchitectureName();

/**
 * @brief VÃ©rifie le support d'une fonctionnalitÃ© SIMD
 * @param feature Nom de la fonctionnalitÃ© (ex: "SSE", "AVX", "NEON")
 * @return true si supportÃ©e, false sinon
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::nk_bool NkHasSIMDFeature(const nkentseu::nk_char *feature);

// --------------------------------------------------------
// Fonctions CPU
// --------------------------------------------------------

/**
 * @brief RÃ©cupÃ¨re le nombre de cÅ“urs CPU physiques
 * @return Nombre de cÅ“urs
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::nk_uint32 NkGetCPUCoreCount();

/**
 * @brief RÃ©cupÃ¨re le nombre total de threads matÃ©riels
 * @return Nombre de threads (incluant hyperthreading)
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::nk_uint32 NkGetCPUThreadCount();

/**
 * @brief RÃ©cupÃ¨re la taille du cache L1
 * @return Taille en bytes
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::nk_uint32 NkGetL1CacheSize();

/**
 * @brief RÃ©cupÃ¨re la taille du cache L2
 * @return Taille en bytes
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::nk_uint32 NkGetL2CacheSize();

/**
 * @brief RÃ©cupÃ¨re la taille du cache L3
 * @return Taille en bytes
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::nk_uint32 NkGetL3CacheSize();

/**
 * @brief RÃ©cupÃ¨re la taille de ligne de cache
 * @return Taille en bytes
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::nk_uint32 NkGetCacheLineSize();

// --------------------------------------------------------
// Fonctions MÃ©moire
// --------------------------------------------------------

/**
 * @brief RÃ©cupÃ¨re la RAM totale du systÃ¨me
 * @return Taille en bytes
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::nk_uint64 NkGetTotalMemory();

/**
 * @brief RÃ©cupÃ¨re la RAM disponible
 * @return Taille en bytes
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::nk_uint64 NkGetAvailableMemory();

/**
 * @brief RÃ©cupÃ¨re la taille de page mÃ©moire
 * @return Taille en bytes
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::nk_uint32 NkGetPageSize();

/**
 * @brief RÃ©cupÃ¨re la granularitÃ© d'allocation mÃ©moire
 * @return Taille en bytes
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::nk_uint32 NkGetAllocationGranularity();

// --------------------------------------------------------
// Fonctions Build et Configuration
// --------------------------------------------------------

/**
 * @brief VÃ©rifie si c'est un build debug
 * @return true si debug, false sinon
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::nk_bool NkIsDebugBuild();

/**
 * @brief VÃ©rifie si c'est une bibliothÃ¨que partagÃ©e
 * @return true si partagÃ©e, false si statique
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::nk_bool NkIsSharedLibrary();

/**
 * @brief RÃ©cupÃ¨re le type de build
 * @return "Debug", "Release", "RelWithDebInfo", etc.
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API const nkentseu::nk_char* NkGetBuildType();

// --------------------------------------------------------
// Fonctions Endianness et Architecture
// --------------------------------------------------------

/**
 * @brief DÃ©tecte l'endianness du systÃ¨me
 * @return Type d'endianness
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::platform::NkEndianness NkGetEndianness();

/**
 * @brief VÃ©rifie si l'architecture est 64-bit
 * @return true si 64-bit, false si 32-bit
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::nk_bool NkIs64Bit();

// --------------------------------------------------------
// Fonctions Utilitaires
// --------------------------------------------------------

/**
 * @brief Affiche les informations plateforme sur stdout
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API void NkPrintPlatformInfo();

/**
 * @brief VÃ©rifie si une adresse est alignÃ©e
 * @param address Adresse Ã  vÃ©rifier
 * @param alignment Alignement requis (doit Ãªtre puissance de 2)
 * @return true si alignÃ©e, false sinon
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::nk_bool NkIsAligned(const nkentseu::nk_ptr address,
														nkentseu::nk_size alignment);

/**
 * @brief Aligne une adresse vers le haut
 * @param address Adresse Ã  aligner
 * @param alignment Alignement requis (doit Ãªtre puissance de 2)
 * @return Adresse alignÃ©e
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API nkentseu::nk_ptr NkAlignAddress(nkentseu::nk_ptr address,
														nkentseu::nk_size alignment);

/**
 * @brief Aligne une adresse constante vers le haut
 * @param address Adresse Ã  aligner
 * @param alignment Alignement requis (doit Ãªtre puissance de 2)
 * @return Adresse alignÃ©e
 * @ingroup PlatformAPI
 */
NKENTSEU_CORE_C_API const nkentseu::nk_ptr NkAlignAddressConst(const nkentseu::nk_ptr address,
																	nkentseu::nk_size alignment);

NKENTSEU_EXTERN_C_END

// ============================================================
// NAMESPACE NKENTSEU::PLATFORM - FONCTIONS INLINE
// ============================================================

namespace nkentseu {

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
		 * @brief VÃ©rifie si la plateforme est desktop
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
		 * @brief VÃ©rifie si la plateforme est mobile
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
		 * @brief VÃ©rifie si la plateforme est une console
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
		 * @brief VÃ©rifie si la plateforme est embarquÃ©e
		 * @return true si embarquÃ©, false sinon
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
		 * @brief VÃ©rifie si la plateforme est web
		 * @return true si web, false sinon
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
			 * @brief VÃ©rifie si l'architecture est 64-bit
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
			 * @brief VÃ©rifie si l'architecture est 32-bit
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
			 * @brief VÃ©rifie si l'architecture est little-endian
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
			 * @brief VÃ©rifie si l'architecture est big-endian
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
			 * @brief Retourne la taille de page mÃ©moire
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
			 * @param addr Adresse Ã  aligner
			 * @param align Alignement (puissance de 2)
			 * @return Adresse alignÃ©e
			 */
			template <typename T> inline T *NkAlignUp(T *addr, nk_size align) noexcept {
				return reinterpret_cast<T *>((reinterpret_cast<nk_uintptr>(addr) + (align - 1)) & ~(align - 1));
			}

			/**
			 * @brief Aligne une adresse vers le bas
			 * @tparam T Type du pointeur
			 * @param addr Adresse Ã  aligner
			 * @param align Alignement (puissance de 2)
			 * @return Adresse alignÃ©e
			 */
			template <typename T> inline T *NkAlignDown(T *addr, nk_size align) noexcept {
				return reinterpret_cast<T *>(reinterpret_cast<nk_uintptr>(addr) & ~(align - 1));
			}

			/**
			 * @brief VÃ©rifie si une adresse est alignÃ©e
			 * @tparam T Type du pointeur
			 * @param addr Adresse Ã  vÃ©rifier
			 * @param align Alignement (puissance de 2)
			 * @return true si alignÃ©e, false sinon
			 */
			template <typename T> inline nk_bool NkIsAligned(const T *addr, nk_size align) noexcept {
				return (reinterpret_cast<nk_uintptr>(addr) & (align - 1)) == 0;
			}

			/**
			 * @brief Calcule le padding nÃ©cessaire pour l'alignement
			 * @tparam T Type du pointeur
			 * @param addr Adresse Ã  vÃ©rifier
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
		// NAMESPACE MEMORY - ALLOCATION ALIGNÃ‰E
		// ============================================================

		namespace memory {

			/**
			 * @brief Alloue de la mÃ©moire alignÃ©e
			 * @param size Taille Ã  allouer
			 * @param alignment Alignement requis (puissance de 2)
			 * @return Pointeur vers mÃ©moire alignÃ©e, nullptr en cas d'erreur
			 */
			NKENTSEU_CORE_API nk_ptr NkAllocateAligned(nk_size size, nk_size alignment) noexcept;

			/**
			 * @brief LibÃ¨re de la mÃ©moire allouÃ©e avec AllocateAligned
			 * @param ptr Pointeur Ã  libÃ©rer
			 */
			NKENTSEU_CORE_API void NkFreeAligned(nk_ptr ptr) noexcept;

			/**
			 * @brief VÃ©rifie si un pointeur est alignÃ©
			 * @param ptr Pointeur Ã  vÃ©rifier
			 * @param alignment Alignement Ã  tester
			 * @return true si alignÃ©, false sinon
			 */
			NKENTSEU_CORE_API nk_bool NkIsPointerAligned(const nk_ptr ptr, nk_size alignment) noexcept;

			/**
			 * @brief Alloue un tableau d'Ã©lÃ©ments alignÃ©s
			 * @tparam T Type des Ã©lÃ©ments
			 * @param count Nombre d'Ã©lÃ©ments
			 * @param alignment Alignement requis
			 * @return Pointeur vers le tableau
			 */
			template <typename T> inline T *NkAllocateAlignedArray(nk_size count, nk_size alignment) noexcept {
				nk_size size = count * sizeof(T);
				nk_ptr ptr = NkAllocateAligned(size, alignment);
				return static_cast<T *>(ptr);
			}

			/**
			 * @brief Construit un objet dans de la mÃ©moire alignÃ©e
			 * @tparam T Type Ã  construire
			 * @tparam Args Types des arguments
			 * @param ptr MÃ©moire alignÃ©e
			 * @param args Arguments pour le constructeur
			 * @return Pointeur vers l'objet construit
			 */
			template <typename T, typename... Args> inline T *NkConstructAligned(nk_ptr ptr, Args &&...args) noexcept {
				return new (ptr) T(static_cast<Args &&>(args)...);
			}

			/**
			 * @brief DÃ©truit un objet dans de la mÃ©moire alignÃ©e
			 * @tparam T Type Ã  dÃ©truire
			 * @param ptr Pointeur vers l'objet
			 */
			template <typename T> inline void NkDestroyAligned(T *ptr) noexcept {
				if (ptr) {
					ptr->~T();
				}
			}

		} // namespace memory
	} // namespace platform

} // namespace nkentseu

// ============================================================
// MACROS DE CONVENANCE
// ============================================================

/**
 * @brief Alloue mÃ©moire alignÃ©e sur page
 */
#define NkAllocPageAligned(size) nkentseu::platform::memory::NkAllocateAligned((size), NKENTSEU_PAGE_SIZE)

#endif // NKENTSEU_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKPLATFORM_H_INCLUDED

// ============================================================
// Copyright Â© 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================
