// -----------------------------------------------------------------------------
// FICHIER: Core\Nkentseu\src\Nkentseu\Platform\NkPlatformConfig.h
// DESCRIPTION: Platform configuration and feature detection
// AUTEUR: Rihen
// DATE: 2026-02-12
// VERSION: 2.0.1
// MODIFICATIONS: Suppression du bridge NK_â†’NKENTSEU_, utilisation directe des macros NKENTSEU_
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKPLATFORMCONFIG_H_INCLUDED
#define NK_CORE_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKPLATFORMCONFIG_H_INCLUDED

#include "NKPlatform/NkPlatformDetect.h"
#include "NKPlatform/NkArchDetect.h"
#include "NKPlatform/NkCompilerDetect.h"

// ============================================================================
// PLATFORM CONFIGURATION
// ============================================================================

namespace nkentseu {
	/**
	 * @brief Namespace platform.
	 */
	namespace platform {

		/**
		 * @brief Platform configuration structure
		 */
		struct PlatformConfig {
			// Platform information
			const char *platformName;
			const char *archName;
			const char *compilerName;
			int compilerVersion;

			// Build configuration
			bool isDebugBuild;
			bool isReleaseBuild;
			bool is64Bit;
			bool isLittleEndian;

			// Feature flags
			bool hasUnicode;
			bool hasThreading;
			bool hasFilesystem;
			bool hasNetwork;

			// Limits
			int maxPathLength;
			int cacheLineSize;

			PlatformConfig();
		};

		/**
		 * @brief Get platform configuration
		 */
		const PlatformConfig &GetPlatformConfig();

		/**
		 * @brief Platform capabilities
		 */
		struct PlatformCapabilities {
			// Memory
			size_t totalPhysicalMemory;
			size_t availablePhysicalMemory;
			size_t pageSize;

			// CPU
			int processorCount;
			int logicalProcessorCount;

			// Display
			bool hasDisplay;
			int primaryScreenWidth;
			int primaryScreenHeight;

			// Features
			bool hasSSE;
			bool hasSSE2;
			bool hasAVX;
			bool hasAVX2;
			bool hasNEON;

			PlatformCapabilities();
		};

		/**
		 * @brief Get platform capabilities (runtime detection)
		 */
		const PlatformCapabilities &GetPlatformCapabilities();

	} // namespace platform
} // namespace nkentseu

// ============================================================================
// PLATFORM-SPECIFIC CONFIGURATION
// ============================================================================

// Windows specific
#ifdef NKENTSEU_PLATFORM_WINDOWS
	#define NKENTSEU_PATH_SEPARATOR '\\'
	#define NKENTSEU_PATH_SEPARATOR_STR "\\"
	#define NKENTSEU_LINE_ENDING "\r\n"
	#define NKENTSEU_MAX_PATH 260
	#define NKENTSEU_DYNAMIC_LIB_EXT ".dll"
	#define NKENTSEU_STATIC_LIB_EXT ".lib"
	#define NKENTSEU_EXECUTABLE_EXT ".exe"
#endif

// Linux specific
#ifdef NKENTSEU_PLATFORM_LINUX
	#define NKENTSEU_PATH_SEPARATOR '/'
	#define NKENTSEU_PATH_SEPARATOR_STR "/"
	#define NKENTSEU_LINE_ENDING "\n"
	#define NKENTSEU_MAX_PATH 4096
	#define NKENTSEU_DYNAMIC_LIB_EXT ".so"
	#define NKENTSEU_STATIC_LIB_EXT ".a"
	#define NKENTSEU_EXECUTABLE_EXT ""
#endif

// macOS specific
#ifdef NKENTSEU_PLATFORM_MACOS
	#define NKENTSEU_PATH_SEPARATOR '/'
	#define NKENTSEU_PATH_SEPARATOR_STR "/"
	#define NKENTSEU_LINE_ENDING "\n"
	#define NKENTSEU_MAX_PATH 1024
	#define NKENTSEU_DYNAMIC_LIB_EXT ".dylib"
	#define NKENTSEU_STATIC_LIB_EXT ".a"
	#define NKENTSEU_EXECUTABLE_EXT ""
#endif

// Android specific
#ifdef NKENTSEU_PLATFORM_ANDROID
	#define NKENTSEU_PATH_SEPARATOR '/'
	#define NKENTSEU_PATH_SEPARATOR_STR "/"
	#define NKENTSEU_LINE_ENDING "\n"
	#define NKENTSEU_MAX_PATH 4096
	#define NKENTSEU_DYNAMIC_LIB_EXT ".so"
	#define NKENTSEU_STATIC_LIB_EXT ".a"
	#define NKENTSEU_EXECUTABLE_EXT ""
#endif

// iOS specific
#ifdef NKENTSEU_PLATFORM_IOS
	#define NKENTSEU_PATH_SEPARATOR '/'
	#define NKENTSEU_PATH_SEPARATOR_STR "/"
	#define NKENTSEU_LINE_ENDING "\n"
	#define NKENTSEU_MAX_PATH 1024
	#define NKENTSEU_DYNAMIC_LIB_EXT ".dylib"
	#define NKENTSEU_STATIC_LIB_EXT ".a"
	#define NKENTSEU_EXECUTABLE_EXT ""
#endif

// Web specific
#ifdef NKENTSEU_PLATFORM_EMSCRIPTEN
	#define NKENTSEU_PATH_SEPARATOR '/'
	#define NKENTSEU_PATH_SEPARATOR_STR "/"
	#define NKENTSEU_LINE_ENDING "\n"
	#define NKENTSEU_MAX_PATH 4096
	#define NKENTSEU_DYNAMIC_LIB_EXT ".wasm"
	#define NKENTSEU_STATIC_LIB_EXT ".a"
	#define NKENTSEU_EXECUTABLE_EXT ".html"
#endif

// Fallback for NKENTSEU_MAX_PATH when platform is not one of the above
#ifndef NKENTSEU_MAX_PATH
	#define NKENTSEU_MAX_PATH 4096
#endif

// ============================================================================
// FEATURE DETECTION MACROS
// ============================================================================

// Unicode support
#ifdef NKENTSEU_PLATFORM_WINDOWS
	#define NKENTSEU_HAS_UNICODE 1
#elif defined(__STDC_ISO_10646__)
	#define NKENTSEU_HAS_UNICODE 1
#else
	#define NKENTSEU_HAS_UNICODE 0
#endif

// Threading support
#if defined(_REENTRANT) || defined(_MT) || (__cplusplus >= 201103L)
	#define NKENTSEU_HAS_THREADING 1
#else
	#define NKENTSEU_HAS_THREADING 0
#endif

// Filesystem support
#if (__cplusplus >= 201703L) && __has_include(<filesystem>)
	#define NKENTSEU_HAS_FILESYSTEM 1
#else
	#define NKENTSEU_HAS_FILESYSTEM 0
#endif

// Network support
#if defined(NKENTSEU_PLATFORM_WINDOWS) || defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_MACOS)
	#define NKENTSEU_HAS_NETWORK 1
#else
	#define NKENTSEU_HAS_NETWORK 0
#endif

// ============================================================================
// BUILD CONFIGURATION
// ============================================================================

// Debug build detection
#if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
	#define NKENTSEU_DEBUG_BUILD 1
	#define NKENTSEU_RELEASE_BUILD 0
#else
	#define NKENTSEU_DEBUG_BUILD 0
	#define NKENTSEU_RELEASE_BUILD 1
#endif

// Optimization level
#if defined(__OPTIMIZE__) || defined(NDEBUG)
	#define NKENTSEU_OPTIMIZED_BUILD 1
#else
	#define NKENTSEU_OPTIMIZED_BUILD 0
#endif

// ============================================================================
// ALIGNMENT MACROS (uses NKENTSEU_CACHE_LINE_SIZE from NkArchDetect.h)
// ============================================================================

// Cache line alignment
#define NKENTSEU_CACHE_ALIGNED NKENTSEU_ALIGN(NKENTSEU_CACHE_LINE_SIZE)

// SIMD alignment
#if defined(__AVX__) || defined(__AVX2__)
	#define NKENTSEU_SIMD_ALIGNMENT 32
#elif defined(__SSE__) || defined(__SSE2__)
	#define NKENTSEU_SIMD_ALIGNMENT 16
#elif defined(__ARM_NEON__) || defined(__ARM_NEON)
	#define NKENTSEU_SIMD_ALIGNMENT 16
#else
	#define NKENTSEU_SIMD_ALIGNMENT 16
#endif

#define NKENTSEU_SIMD_ALIGNED NKENTSEU_ALIGN(NKENTSEU_SIMD_ALIGNMENT)

// ============================================================================
// PLATFORM UTILITIES
// ============================================================================

namespace nkentseu {
	/**
	 * @brief Namespace platform.
	 */
	namespace platform {

		/**
		 * @brief Get platform name string
		 */
		inline const char *GetPlatformName() {
			#ifdef NKENTSEU_PLATFORM_WINDOWS
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
		 * @brief Get architecture name string
		 */
		inline const char *GetArchName() {
			#ifdef NKENTSEU_ARCH_X86_64
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
		 * @brief Get compiler name string
		 */
		inline const char *GetCompilerName() {
			#ifdef NKENTSEU_COMPILER_MSVC
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
		 * @brief Check if running on 64-bit architecture
		 */
		inline bool Is64Bit() {
			#ifdef NKENTSEU_ARCH_64BIT
				return true;
			#else
				return false;
			#endif
		}

		/**
		 * @brief Check if running on little-endian system
		 */
		inline bool IsLittleEndian() {
			#ifdef NKENTSEU_ARCH_LITTLE_ENDIAN
				return true;
			#else
				return false;
			#endif
		}

	} // namespace platform
} // namespace nkentseu

#endif // NK_CORE_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKPLATFORMCONFIG_H_INCLUDED

// ============================================================
// Copyright Â© 2024-2026 Rihen. All rights reserved.
// ============================================================
