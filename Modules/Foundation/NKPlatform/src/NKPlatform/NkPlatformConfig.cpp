// -----------------------------------------------------------------------------
// FICHIER: Core\Nkentseu\src\Nkentseu\Platform\NkPlatformConfig.cpp
// DESCRIPTION: Platform configuration implementation
// AUTEUR: Rihen
// DATE: 2026-02-12
// MODIFICATIONS: Utilisation directe des macros NKENTSEU_
// -----------------------------------------------------------------------------

#include "NkPlatformConfig.h"
#include <stdlib.h>

#ifdef NKENTSEU_PLATFORM_WINDOWS
#include <windows.h>
#elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_ANDROID)
#include <unistd.h>
#include <sys/sysinfo.h>
#elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#else
#include <unistd.h>
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
/**
 * @brief Namespace platform.
 */
namespace platform {

// ====================================================================
// PlatformConfig Implementation
// ====================================================================

PlatformConfig::PlatformConfig()
	: platformName(GetPlatformName()), archName(GetArchName()), compilerName(GetCompilerName()),
	  compilerVersion(NKENTSEU_COMPILER_VERSION), isDebugBuild(NKENTSEU_DEBUG_BUILD),
	  isReleaseBuild(NKENTSEU_RELEASE_BUILD), is64Bit(Is64Bit()), isLittleEndian(IsLittleEndian()),
	  hasUnicode(NKENTSEU_HAS_UNICODE), hasThreading(NKENTSEU_HAS_THREADING), hasFilesystem(NKENTSEU_HAS_FILESYSTEM),
	  hasNetwork(NKENTSEU_HAS_NETWORK), maxPathLength(NKENTSEU_MAX_PATH), cacheLineSize(NKENTSEU_CACHE_LINE_SIZE) {
}

const PlatformConfig &GetPlatformConfig() {
	static PlatformConfig config;
	return config;
}

// ====================================================================
// PlatformCapabilities Implementation
// ====================================================================

PlatformCapabilities::PlatformCapabilities()
	: totalPhysicalMemory(0), availablePhysicalMemory(0), pageSize(0), processorCount(0), logicalProcessorCount(0),
	  hasDisplay(false), primaryScreenWidth(0), primaryScreenHeight(0), hasSSE(false), hasSSE2(false), hasAVX(false),
	  hasAVX2(false), hasNEON(false) {

// Detect memory
#ifdef NKENTSEU_PLATFORM_WINDOWS
	MEMORYSTATUSEX memStatus;
	memStatus.dwLength = sizeof(memStatus);
	if (GlobalMemoryStatusEx(&memStatus)) {
		totalPhysicalMemory = memStatus.ullTotalPhys;
		availablePhysicalMemory = memStatus.ullAvailPhys;
	}

	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	pageSize = sysInfo.dwPageSize;
	processorCount = sysInfo.dwNumberOfProcessors;
	logicalProcessorCount = sysInfo.dwNumberOfProcessors;

	// Display info
	primaryScreenWidth = GetSystemMetrics(SM_CXSCREEN);
	primaryScreenHeight = GetSystemMetrics(SM_CYSCREEN);
	hasDisplay = (primaryScreenWidth > 0 && primaryScreenHeight > 0);

#elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_ANDROID)
	// Linux/Android
	long pages = sysconf(_SC_PHYS_PAGES);
	long pageSize_bytes = sysconf(_SC_PAGE_SIZE);
	if (pages > 0 && pageSize_bytes > 0) {
		totalPhysicalMemory = pages * pageSize_bytes;
	}

	long availPages = sysconf(_SC_AVPHYS_PAGES);
	if (availPages > 0) {
		availablePhysicalMemory = availPages * pageSize_bytes;
	}

	pageSize = pageSize_bytes;

	long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
	if (nprocs > 0) {
		processorCount = static_cast<int>(nprocs);
		logicalProcessorCount = processorCount;
	}

	// Display detection (basic)
	const char *display = ::getenv("DISPLAY");
	hasDisplay = (display != nullptr && display[0] != '\0');
#elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
	// Apple platforms
	size_t memSize = sizeof(totalPhysicalMemory);
	if (sysctlbyname("hw.memsize", &totalPhysicalMemory, &memSize, nullptr, 0) != 0) {
		totalPhysicalMemory = 0;
	}
	// Lightweight fallback (exact free RAM query is more involved on Apple).
	availablePhysicalMemory = totalPhysicalMemory / 2;

	long pageSize_bytes = sysconf(_SC_PAGESIZE);
	if (pageSize_bytes > 0) {
		pageSize = static_cast<size_t>(pageSize_bytes);
	}

	long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
	if (nprocs > 0) {
		processorCount = static_cast<int>(nprocs);
		logicalProcessorCount = processorCount;
	}

	hasDisplay = true;
#else
	// Generic Unix fallback
	long pageSize_bytes = sysconf(_SC_PAGESIZE);
	if (pageSize_bytes > 0) {
		pageSize = static_cast<size_t>(pageSize_bytes);
	}
	long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
	if (nprocs > 0) {
		processorCount = static_cast<int>(nprocs);
		logicalProcessorCount = processorCount;
	}
#endif

// CPU features (compile-time detection)
#ifdef __SSE__
	hasSSE = true;
#endif
#ifdef __SSE2__
	hasSSE2 = true;
#endif
#ifdef __AVX__
	hasAVX = true;
#endif
#ifdef __AVX2__
	hasAVX2 = true;
#endif
#if defined(__ARM_NEON__) || defined(__ARM_NEON)
	hasNEON = true;
#endif
}

const PlatformCapabilities &GetPlatformCapabilities() {
	static PlatformCapabilities caps;
	return caps;
}

} // namespace platform
} // namespace nkentseu

// ============================================================
// Copyright Â© 2024-2026 Rihen. All rights reserved.
// ============================================================

