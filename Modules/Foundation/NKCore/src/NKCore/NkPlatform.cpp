// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\NkPlatform.cpp
// DESCRIPTION: ImplÃ©mentation des fonctions de dÃ©tection plateforme runtime
// AUTEUR: Rihen
// DATE: 2026-02-10
// VERSION: 3.0.0
// MODIFICATIONS: ImplÃ©mentation complÃ¨te pour NkPlatform.h v3.0.0
// -----------------------------------------------------------------------------

#include "NkPlatform.h"
#include "NkAtomic.h"
#include "NKPlatform/NkFoundationLog.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Avoid macro/function name collision from NkPlatform.h convenience macro.
#ifdef NkFreeAligned
#undef NkFreeAligned
#endif

// ============================================================
// INCLUDES PLATFORM-SPECIFIC
// ============================================================

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#if defined(__MINGW32__) || defined(__MINGW64__)
#include <psapi.h>
#else
#include <psapi.h>
#endif
#pragma comment(lib, "psapi.lib")
#elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_ANDROID)
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#if defined(NKENTSEU_ARCH_X86) || defined(NKENTSEU_ARCH_X86_64)
#include <cpuid.h>
#endif
#elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
#include <TargetConditionals.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>
#if defined(NKENTSEU_ARCH_X86) || defined(NKENTSEU_ARCH_X86_64)
#include <cpuid.h>
#endif
#endif


using namespace nkentseu::platform;
using namespace nkentseu;

// ============================================================
// VARIABLES STATIQUES
// ============================================================

static NkPlatformInfo sPlatformInfo;
static NkAtomic<nk_bool> sInitialized(false);

// ============================================================
// FONCTIONS DÃ‰TECTION SIMD - WINDOWS
// ============================================================

#if defined(NKENTSEU_PLATFORM_WINDOWS)

static void NkDetectSIMDWindows() {
	nk_int32 cpuInfo[4] = {0};

	// DÃ©tection SSE/SSE2/SSE3/SSE4
	__cpuid(cpuInfo, 1);
	sPlatformInfo.hasSSE = (cpuInfo[3] & (1 << 25)) != 0;
	sPlatformInfo.hasSSE2 = (cpuInfo[3] & (1 << 26)) != 0;
	sPlatformInfo.hasSSE3 = (cpuInfo[2] & (1 << 0)) != 0;
	sPlatformInfo.hasSSE4_1 = (cpuInfo[2] & (1 << 19)) != 0;
	sPlatformInfo.hasSSE4_2 = (cpuInfo[2] & (1 << 20)) != 0;

	// DÃ©tection AVX
	nk_bool osXSaveEnabled = false;
#if defined(NKENTSEU_ARCH_X86_64)
	if ((cpuInfo[2] & (1 << 27)) != 0) {
#if defined(NKENTSEU_COMPILER_MSVC)
		nk_uint64 xcr0 = _xgetbv(0);
		osXSaveEnabled = (xcr0 & 6) == 6;
#elif defined(__GNUC__) || defined(__clang__)
		nk_uint32 eax, edx;
		__asm__ __volatile__("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
		nk_uint64 xcr0 = ((nk_uint64)edx << 32) | eax;
		osXSaveEnabled = (xcr0 & 6) == 6;
#endif
	}
#endif

	sPlatformInfo.hasAVX = (cpuInfo[2] & (1 << 28)) != 0 && osXSaveEnabled;

	// DÃ©tection AVX2/AVX-512
	__cpuidex(cpuInfo, 7, 0);
	sPlatformInfo.hasAVX2 = (cpuInfo[1] & (1 << 5)) != 0 && osXSaveEnabled;
	sPlatformInfo.hasAVX512 = (cpuInfo[1] & (1 << 16)) != 0 && osXSaveEnabled;
}

static void NkDetectCacheWindows() {
	DWORD bufferSize = 0;
	GetLogicalProcessorInformation(nullptr, &bufferSize);

	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		return;

	auto *buffer = static_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION *>(malloc(bufferSize));
	if (!buffer)
		return;

	if (!GetLogicalProcessorInformation(buffer, &bufferSize)) {
		free(buffer);
		return;
	}

	DWORD byteOffset = 0;
	while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= bufferSize) {
		auto *info =
			reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION *>(reinterpret_cast<nk_uint8 *>(buffer) + byteOffset);

		if (info->Relationship == RelationCache) {
			if (info->Cache.Level == 1) {
				sPlatformInfo.cpuL1CacheSize = static_cast<nk_uint32>(info->Cache.Size);
			} else if (info->Cache.Level == 2) {
				sPlatformInfo.cpuL2CacheSize = static_cast<nk_uint32>(info->Cache.Size);
			} else if (info->Cache.Level == 3) {
				sPlatformInfo.cpuL3CacheSize = static_cast<nk_uint32>(info->Cache.Size);
			}
		}

		byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
	}

	free(buffer);
}

#endif // NKENTSEU_PLATFORM_WINDOWS

// ============================================================
// FONCTIONS DÃ‰TECTION SIMD - LINUX
// ============================================================

#if defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_ANDROID)

static void NkDetectSIMDLinux() {
#if defined(NKENTSEU_ARCH_X86) || defined(NKENTSEU_ARCH_X86_64)
	nk_uint32 eax = 0, ebx = 0, ecx = 0, edx = 0;

	if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
		sPlatformInfo.hasSSE = (edx & (1 << 25)) != 0;
		sPlatformInfo.hasSSE2 = (edx & (1 << 26)) != 0;
		sPlatformInfo.hasSSE3 = (ecx & (1 << 0)) != 0;
		sPlatformInfo.hasSSE4_1 = (ecx & (1 << 19)) != 0;
		sPlatformInfo.hasSSE4_2 = (ecx & (1 << 20)) != 0;
		sPlatformInfo.hasAVX = (ecx & (1 << 28)) != 0;
	}

	if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
		sPlatformInfo.hasAVX2 = (ebx & (1 << 5)) != 0;
		sPlatformInfo.hasAVX512 = (ebx & (1 << 16)) != 0;
	}
#elif defined(NKENTSEU_ARCH_ARM) || defined(NKENTSEU_ARCH_ARM64)
	sPlatformInfo.hasNEON = true; // ARM Linux a gÃ©nÃ©ralement NEON
#endif
}

static void NkDetectCacheLinux() {
	FILE *fp = fopen("/sys/devices/system/cpu/cpu0/cache/index0/size", "r");
	if (fp) {
		nk_int32 size;
		if (fscanf(fp, "%d", &size) == 1) {
			sPlatformInfo.cpuL1CacheSize = static_cast<nk_uint32>(size * 1024);
		}
		fclose(fp);
	}

	fp = fopen("/sys/devices/system/cpu/cpu0/cache/index2/size", "r");
	if (fp) {
		nk_int32 size;
		if (fscanf(fp, "%d", &size) == 1) {
			sPlatformInfo.cpuL2CacheSize = static_cast<nk_uint32>(size * 1024);
		}
		fclose(fp);
	}

	fp = fopen("/sys/devices/system/cpu/cpu0/cache/index3/size", "r");
	if (fp) {
		nk_int32 size;
		if (fscanf(fp, "%d", &size) == 1) {
			sPlatformInfo.cpuL3CacheSize = static_cast<nk_uint32>(size * 1024);
		}
		fclose(fp);
	}
}

#endif // NKENTSEU_PLATFORM_LINUX

// ============================================================
// FONCTIONS DÃ‰TECTION SIMD - MACOS
// ============================================================

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)

static void NkDetectSIMDApple() {
#if defined(NKENTSEU_ARCH_X86) || defined(NKENTSEU_ARCH_X86_64)
	nk_uint32 eax = 0, ebx = 0, ecx = 0, edx = 0;

	if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
		sPlatformInfo.hasSSE = (edx & (1 << 25)) != 0;
		sPlatformInfo.hasSSE2 = (edx & (1 << 26)) != 0;
		sPlatformInfo.hasSSE3 = (ecx & (1 << 0)) != 0;
		sPlatformInfo.hasSSE4_1 = (ecx & (1 << 19)) != 0;
		sPlatformInfo.hasSSE4_2 = (ecx & (1 << 20)) != 0;
		sPlatformInfo.hasAVX = (ecx & (1 << 28)) != 0;
	}

	if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
		sPlatformInfo.hasAVX2 = (ebx & (1 << 5)) != 0;
		sPlatformInfo.hasAVX512 = (ebx & (1 << 16)) != 0;
	}
#else
	// Apple Silicon (ARM)
	sPlatformInfo.hasNEON = true;
#endif
}

static void NkDetectCacheApple() {
	size_t size = sizeof(nk_uint32);

	nk_uint32 l1Size = 0;
	if (sysctlbyname("hw.l1dcachesize", &l1Size, &size, nullptr, 0) == 0) {
		sPlatformInfo.cpuL1CacheSize = l1Size;
	}

	nk_uint32 l2Size = 0;
	size = sizeof(nk_uint32);
	if (sysctlbyname("hw.l2cachesize", &l2Size, &size, nullptr, 0) == 0) {
		sPlatformInfo.cpuL2CacheSize = l2Size;
	}

	nk_uint32 l3Size = 0;
	size = sizeof(nk_uint32);
	if (sysctlbyname("hw.l3cachesize", &l3Size, &size, nullptr, 0) == 0) {
		sPlatformInfo.cpuL3CacheSize = l3Size;
	}
}

#endif // NKENTSEU_PLATFORM_MACOS

// ============================================================
// IMPLÃ‰MENTATION - FONCTION D'INITIALISATION
// ============================================================

void NkInitializePlatformInfo() {
	if (sInitialized.Load()) {
		return;
	}

	// Initialiser Ã  zÃ©ro
	::memset(&sPlatformInfo, 0, sizeof(NkPlatformInfo));

	// --------------------------------------------------------
	// 1. INFORMATIONS OS ET COMPILATEUR
	// --------------------------------------------------------
	sPlatformInfo.osName = NKENTSEU_PLATFORM_NAME;
	sPlatformInfo.archName = NKENTSEU_ARCH_NAME;

// DÃ©terminer le type de plateforme
#if defined(NKENTSEU_PLATFORM_WINDOWS)
	sPlatformInfo.platform = NkPlatformType::NK_WINDOWS;
	sPlatformInfo.osVersion = "Windows";
#elif defined(NKENTSEU_PLATFORM_LINUX)
	sPlatformInfo.platform = NkPlatformType::NK_LINUX;
	struct utsname uts;
	if (uname(&uts) == 0) {
		static nk_char osVersion[256];
		snprintf(osVersion, sizeof(osVersion), "%s %s", uts.release, uts.version);
		sPlatformInfo.osVersion = osVersion;
	} else {
		sPlatformInfo.osVersion = "Linux";
	}
#elif defined(NKENTSEU_PLATFORM_MACOS)
	sPlatformInfo.platform = NkPlatformType::NK_MACOS;
	sPlatformInfo.osVersion = "macOS";
#elif defined(NKENTSEU_PLATFORM_IOS)
	sPlatformInfo.platform = NkPlatformType::NK_IOS;
	sPlatformInfo.osVersion = "iOS";
#elif defined(NKENTSEU_PLATFORM_ANDROID)
	sPlatformInfo.platform = NkPlatformType::NK_ANDROID;
	sPlatformInfo.osVersion = "Android";
#elif defined(NKENTSEU_PLATFORM_FREEBSD)
	sPlatformInfo.platform = NkPlatformType::NK_FREEBSD;
	sPlatformInfo.osVersion = "FreeBSD";
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
	sPlatformInfo.platform = NkPlatformType::NK_EMSCRIPTEN;
	sPlatformInfo.osVersion = "Emscripten";
#else
	sPlatformInfo.platform = NkPlatformType::NK_UNKNOWN;
	sPlatformInfo.osVersion = "Unknown";
#endif

// DÃ©terminer le type d'architecture
#if defined(NKENTSEU_ARCH_X86_64)
	sPlatformInfo.architecture = NkArchitectureType::NK_X64;
#elif defined(NKENTSEU_ARCH_X86)
	sPlatformInfo.architecture = NkArchitectureType::NK_X86;
#elif defined(NKENTSEU_ARCH_ARM64)
	sPlatformInfo.architecture = NkArchitectureType::NK_ARM64;
#elif defined(NKENTSEU_ARCH_ARM32)
	sPlatformInfo.architecture = NkArchitectureType::NK_ARM32;
#elif defined(NKENTSEU_ARCH_WASM)
	sPlatformInfo.architecture = NkArchitectureType::NK_WASM;
#else
	sPlatformInfo.architecture = NkArchitectureType::NK_UNKNOWN;
#endif

// Informations compilateur
#if defined(NKENTSEU_COMPILER_MSVC)
	sPlatformInfo.compilerName = "MSVC";
	static nk_char compilerVersion[32];
	snprintf(compilerVersion, sizeof(compilerVersion), "%d", _MSC_VER);
	sPlatformInfo.compilerVersion = compilerVersion;
#elif defined(NKENTSEU_COMPILER_GCC)
	sPlatformInfo.compilerName = "GCC";
	static nk_char compilerVersion[32];
	snprintf(compilerVersion, sizeof(compilerVersion), "%d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
	sPlatformInfo.compilerVersion = compilerVersion;
#elif defined(NKENTSEU_COMPILER_CLANG)
	sPlatformInfo.compilerName = "Clang";
	static nk_char compilerVersion[32];
	snprintf(compilerVersion, sizeof(compilerVersion), "%d.%d.%d", __clang_major__, __clang_minor__,
			 __clang_patchlevel__);
	sPlatformInfo.compilerVersion = compilerVersion;
#else
	sPlatformInfo.compilerName = "Unknown";
	sPlatformInfo.compilerVersion = "Unknown";
#endif

	// --------------------------------------------------------
	// 2. INFORMATIONS CPU
	// --------------------------------------------------------
	sPlatformInfo.cpuCoreCount = NkGetCPUCoreCount();
	sPlatformInfo.cpuThreadCount = NkGetCPUThreadCount();
	sPlatformInfo.cacheLineSize = NKENTSEU_CACHE_LINE_SIZE;

// DÃ©tection cache
#if defined(NKENTSEU_PLATFORM_WINDOWS)
	NkDetectCacheWindows();
#elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_ANDROID)
	NkDetectCacheLinux();
#elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
	NkDetectCacheApple();
#endif

	// Valeurs par dÃ©faut si non dÃ©tectÃ©
	if (sPlatformInfo.cpuL1CacheSize == 0)
		sPlatformInfo.cpuL1CacheSize = 32 * 1024;
	if (sPlatformInfo.cpuL2CacheSize == 0)
		sPlatformInfo.cpuL2CacheSize = 256 * 1024;
	if (sPlatformInfo.cpuL3CacheSize == 0)
		sPlatformInfo.cpuL3CacheSize = 8 * 1024 * 1024;

// --------------------------------------------------------
// 3. DÃ‰TECTION SIMD
// --------------------------------------------------------
#if defined(NKENTSEU_PLATFORM_WINDOWS)
	NkDetectSIMDWindows();
#elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_ANDROID)
	NkDetectSIMDLinux();
#elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
	NkDetectSIMDApple();
#endif

	// --------------------------------------------------------
	// 4. INFORMATIONS MÃ‰MOIRE
	// --------------------------------------------------------
	sPlatformInfo.totalMemory = NkGetTotalMemory();
	sPlatformInfo.availableMemory = NkGetAvailableMemory();
	sPlatformInfo.pageSize = NkGetPageSize();
	sPlatformInfo.allocationGranularity = NkGetAllocationGranularity();

	// --------------------------------------------------------
	// 5. INFORMATIONS PLATEFORME
	// --------------------------------------------------------
	sPlatformInfo.endianness = NkGetEndianness();
	sPlatformInfo.isLittleEndian = (sPlatformInfo.endianness == NkEndianness::NK_LITTLE);
	sPlatformInfo.is64Bit = NkIs64Bit();

	// --------------------------------------------------------
	// 6. INFORMATIONS BUILD
	// --------------------------------------------------------
	sPlatformInfo.isDebugBuild = NkIsDebugBuild();
	sPlatformInfo.isSharedLibrary = NkIsSharedLibrary();
	sPlatformInfo.buildType = NkGetBuildType();

	// --------------------------------------------------------
	// 7. CAPACITÃ‰S PLATEFORME
	// --------------------------------------------------------
	sPlatformInfo.hasThreading = true;
	sPlatformInfo.hasVirtualMemory = true;
	sPlatformInfo.hasFileSystem = true;
	sPlatformInfo.hasNetwork = true;

// --------------------------------------------------------
// 8. SYSTÃˆME D'AFFICHAGE
// --------------------------------------------------------
#if defined(NKENTSEU_PLATFORM_WINDOWS)
	sPlatformInfo.display = NkDisplayType::NK_WIN32;
#elif defined(NKENTSEU_PLATFORM_MACOS)
	sPlatformInfo.display = NkDisplayType::NK_COCOA;
#elif defined(NKENTSEU_PLATFORM_ANDROID)
	sPlatformInfo.display = NkDisplayType::NK_ANDROID;
#elif defined(NKENTSEU_DISPLAY_WAYLAND)
	sPlatformInfo.display = NkDisplayType::NK_WAYLAND;
#elif defined(NKENTSEU_DISPLAY_XCB)
	sPlatformInfo.display = NkDisplayType::NK_XCB;
#elif defined(NKENTSEU_DISPLAY_XLIB)
	sPlatformInfo.display = NkDisplayType::NK_XLIB;
#else
	sPlatformInfo.display = NkDisplayType::NK_NONE;
#endif

	// --------------------------------------------------------
	// 9. API GRAPHIQUE
	// --------------------------------------------------------
	sPlatformInfo.graphicsApi = NkGraphicsAPI::NK_NONE;
	sPlatformInfo.graphicsApiVersion.major = 0;
	sPlatformInfo.graphicsApiVersion.minor = 0;
	sPlatformInfo.graphicsApiVersion.patch = 0;
	sPlatformInfo.graphicsApiVersion.versionString = "0.0.0";

	// Marquer comme initialisÃ©
	sInitialized.Store(true);
}

// ============================================================
// IMPLÃ‰MENTATION - FONCTIONS PUBLIQUES
// ============================================================

const NkPlatformInfo* NkGetPlatformInfo() {
	if (!sInitialized.Load()) {
		NkInitializePlatformInfo();
	}
	return &sPlatformInfo;
}

const nk_char* NkGetPlatformName() {
	return NkGetPlatformInfo()->osName;
}

const nk_char *NkGetArchitectureName() {
	return NkGetPlatformInfo()->archName;
}

nk_bool NkHasSIMDFeature(const nk_char *feature) {
	if (!feature)
		return false;

	const NkPlatformInfo &info = *NkGetPlatformInfo();

	if (::strcmp(feature, "SSE") == 0)
		return info.hasSSE;
	if (::strcmp(feature, "SSE2") == 0)
		return info.hasSSE2;
	if (::strcmp(feature, "SSE3") == 0)
		return info.hasSSE3;
	if (::strcmp(feature, "SSE4.1") == 0)
		return info.hasSSE4_1;
	if (::strcmp(feature, "SSE4.2") == 0)
		return info.hasSSE4_2;
	if (::strcmp(feature, "AVX") == 0)
		return info.hasAVX;
	if (::strcmp(feature, "AVX2") == 0)
		return info.hasAVX2;
	if (::strcmp(feature, "AVX512") == 0)
		return info.hasAVX512;
	if (::strcmp(feature, "NEON") == 0)
		return info.hasNEON;

	return false;
}

// ============================================================
// FONCTIONS CPU
// ============================================================

nk_uint32 NkGetCPUCoreCount() {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	return static_cast<nk_uint32>(sysInfo.dwNumberOfProcessors);
#elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_ANDROID)
	return static_cast<nk_uint32>(sysconf(_SC_NPROCESSORS_ONLN));
#elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
	nk_int32 coreCount = 0;
	size_t size = sizeof(coreCount);
	if (sysctlbyname("hw.physicalcpu", &coreCount, &size, nullptr, 0) == 0) {
		return static_cast<nk_uint32>(coreCount);
	}
	return 1;
#else
	return 1;
#endif
}

nk_uint32 NkGetCPUThreadCount() {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	return static_cast<nk_uint32>(sysInfo.dwNumberOfProcessors);
#elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_ANDROID)
	return static_cast<nk_uint32>(sysconf(_SC_NPROCESSORS_CONF));
#elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
	nk_int32 threadCount = 0;
	size_t size = sizeof(threadCount);
	if (sysctlbyname("hw.logicalcpu", &threadCount, &size, nullptr, 0) == 0) {
		return static_cast<nk_uint32>(threadCount);
	}
	return NkGetCPUCoreCount();
#else
	return NkGetCPUCoreCount();
#endif
}

nk_uint32 NkGetL1CacheSize() {
	return NkGetPlatformInfo()->cpuL1CacheSize;
}

nk_uint32 NkGetL2CacheSize() {
	return NkGetPlatformInfo()->cpuL2CacheSize;
}

nk_uint32 NkGetL3CacheSize() {
	return NkGetPlatformInfo()->cpuL3CacheSize;
}

nk_uint32 NkGetCacheLineSize() {
	return NKENTSEU_CACHE_LINE_SIZE;
}

// ============================================================
// FONCTIONS MÃ‰MOIRE
// ============================================================

nk_uint64 NkGetTotalMemory() {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
	MEMORYSTATUSEX memInfo;
	memInfo.dwLength = sizeof(MEMORYSTATUSEX);
	if (GlobalMemoryStatusEx(&memInfo)) {
		return static_cast<nk_uint64>(memInfo.ullTotalPhys);
	}
#elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_ANDROID)
	struct sysinfo info;
	if (sysinfo(&info) == 0) {
		return static_cast<nk_uint64>(info.totalram) * info.mem_unit;
	}
#elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
	nk_uint64 totalMemory = 0;
	size_t size = sizeof(totalMemory);
	if (sysctlbyname("hw.memsize", &totalMemory, &size, nullptr, 0) == 0) {
		return static_cast<nk_uint64>(totalMemory);
	}
#endif
	return 0;
}

nk_uint64 NkGetAvailableMemory() {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
	MEMORYSTATUSEX memInfo;
	memInfo.dwLength = sizeof(MEMORYSTATUSEX);
	if (GlobalMemoryStatusEx(&memInfo)) {
		return static_cast<nk_uint64>(memInfo.ullAvailPhys);
	}
#elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_ANDROID)
	struct sysinfo info;
	if (sysinfo(&info) == 0) {
		return static_cast<nk_uint64>(info.freeram) * info.mem_unit;
	}
#elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
	// Approximation simple
	return NkGetTotalMemory() / 2;
#endif
	return 0;
}

nk_uint32 NkGetPageSize() {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	return static_cast<nk_uint32>(sysInfo.dwPageSize);
#elif defined(__EMSCRIPTEN__)
	return 4096; // Page size fixe pour WebAssembly
#elif defined(NKENTSEU_PLATFORM_POSIX)
	return static_cast<nk_uint32>(sysconf(_SC_PAGESIZE));
#else
	return 4096;
#endif
}

nk_uint32 NkGetAllocationGranularity() {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	return static_cast<nk_uint32>(sysInfo.dwAllocationGranularity);
#else
	return NkGetPageSize();
#endif
}

// ============================================================
// FONCTIONS BUILD
// ============================================================

nk_bool NkIsDebugBuild() {
#if defined(NKENTSEU_DEBUG)
	return true;
#else
	return false;
#endif
}

nk_bool NkIsSharedLibrary() {
#if NKENTSEU_SHARED_BUILD
	return true;
#else
	return false;
#endif
}

const nk_char *NkGetBuildType() {
#if defined(NKENTSEU_DEBUG)
	return "Debug";
#elif defined(NKENTSEU_RELEASE)
	return "Release";
#else
	return "Unknown";
#endif
}

// ============================================================
// FONCTIONS ENDIANNESS
// ============================================================

NkEndianness NkGetEndianness() {
	static NkEndianness endianness = NkEndianness::NK_UNKNOWN;

	if (endianness == NkEndianness::NK_UNKNOWN) {
		union {
			nk_uint32 i;
			nk_uint8 c[4];
		} test = {0x01020304};

		if (test.c[0] == 0x01) {
			endianness = NkEndianness::NK_BIG;
		} else {
			endianness = NkEndianness::NK_LITTLE;
		}
	}

	return endianness;
}

nk_bool NkIs64Bit() {
#if defined(NKENTSEU_ARCH_64BIT)
	return true;
#else
	return false;
#endif
}

// ============================================================
// FONCTIONS UTILITAIRES
// ============================================================

void NkPrintPlatformInfo() {
	const NkPlatformInfo &info = *NkGetPlatformInfo();

	NK_FOUNDATION_LOG_INFO("=== Nkentseu Platform Information ===");
	NK_FOUNDATION_LOG_INFO("OS: %s (%s)", info.osName, info.osVersion ? info.osVersion : "Unknown");
	NK_FOUNDATION_LOG_INFO("Architecture: %s (%s-bit)", info.archName, info.is64Bit ? "64" : "32");
	NK_FOUNDATION_LOG_INFO("Compiler: %s %s", info.compilerName, info.compilerVersion);
	NK_FOUNDATION_LOG_INFO("CPU Cores: %u (Threads: %u)", info.cpuCoreCount, info.cpuThreadCount);
	NK_FOUNDATION_LOG_INFO("CPU Cache: L1=%uKB, L2=%uKB, L3=%uMB", info.cpuL1CacheSize / 1024, info.cpuL2CacheSize / 1024,
	                       info.cpuL3CacheSize / (1024 * 1024));
	NK_FOUNDATION_LOG_INFO("Memory: Total=%lluMB, Available=%lluMB",
	                       static_cast<unsigned long long>(info.totalMemory / (1024 * 1024)),
	                       static_cast<unsigned long long>(info.availableMemory / (1024 * 1024)));
	NK_FOUNDATION_LOG_INFO("Page Size: %u bytes, Allocation Granularity: %u bytes", info.pageSize,
	                       info.allocationGranularity);
	NK_FOUNDATION_LOG_INFO("Cache Line Size: %u bytes", info.cacheLineSize);

	nk_char simd[128] = {};
	nk_size offset = 0;
	const auto appendFeature = [&](const char* name) {
		if (!name || name[0] == '\0' || offset >= sizeof(simd) - 1) {
			return;
		}
		if (offset > 0 && offset < sizeof(simd) - 1) {
			const nk_size remaining = sizeof(simd) - offset;
			const int sepWritten = snprintf(simd + offset, remaining, ", ");
			if (sepWritten > 0) {
				const nk_size advanced = static_cast<nk_size>(sepWritten);
				offset += (advanced < remaining) ? advanced : (remaining - 1);
			}
		}
		if (offset < sizeof(simd) - 1) {
			const nk_size remaining = sizeof(simd) - offset;
			const int nameWritten = snprintf(simd + offset, remaining, "%s", name);
			if (nameWritten > 0) {
				const nk_size advanced = static_cast<nk_size>(nameWritten);
				offset += (advanced < remaining) ? advanced : (remaining - 1);
			}
		}
	};

	if (info.hasSSE) {
		appendFeature("SSE");
	}
	if (info.hasSSE2) {
		appendFeature("SSE2");
	}
	if (info.hasSSE3) {
		appendFeature("SSE3");
	}
	if (info.hasSSE4_1) {
		appendFeature("SSE4.1");
	}
	if (info.hasSSE4_2) {
		appendFeature("SSE4.2");
	}
	if (info.hasAVX) {
		appendFeature("AVX");
	}
	if (info.hasAVX2) {
		appendFeature("AVX2");
	}
	if (info.hasAVX512) {
		appendFeature("AVX-512");
	}
	if (info.hasNEON) {
		appendFeature("NEON");
	}
	NK_FOUNDATION_LOG_INFO("SIMD Support: %s", (offset > 0) ? simd : "None");

	NK_FOUNDATION_LOG_INFO("Endianness: %s", info.isLittleEndian ? "Little Endian" : "Big Endian");
	NK_FOUNDATION_LOG_INFO("Build Type: %s (%s)", info.buildType, info.isDebugBuild ? "Debug" : "Release");
	NK_FOUNDATION_LOG_INFO("Library Type: %s", info.isSharedLibrary ? "Shared" : "Static");
	NK_FOUNDATION_LOG_INFO("======================================");
}

nk_bool NkIsAligned(const nk_ptr address, nk_size alignment) {
	if (alignment == 0)
		return true;
	return (reinterpret_cast<nk_uintptr>(address) & (alignment - 1)) == 0;
}

nk_ptr NkAlignAddress(nk_ptr address, nk_size alignment) {
	if (alignment == 0)
		return address;

	nk_uintptr addr = reinterpret_cast<nk_uintptr>(address);
	nk_uintptr alignedAddr = (addr + alignment - 1) & ~(alignment - 1);

	return reinterpret_cast<nk_ptr>(alignedAddr);
}

const nk_ptr NkAlignAddressConst(const nk_ptr address, nk_size alignment) {
	if (alignment == 0)
		return address;

	nk_uintptr addr = reinterpret_cast<nk_uintptr>(address);
	nk_uintptr alignedAddr = (addr + alignment - 1) & ~(alignment - 1);

	return reinterpret_cast<const nk_ptr>(alignedAddr);
}

// ============================================================
// NAMESPACE MEMORY - ALLOCATION ALIGNÃ‰E
// ============================================================

namespace nkentseu {
/**
 * @brief Namespace core.
 */

/**
 * @brief Namespace platform.
 */
namespace platform {
/**
 * @brief Namespace memory.
 */
namespace memory {

nk_ptr NkAllocateAligned(nk_size size, nk_size alignment) noexcept {
	if (size == 0)
		return nullptr;

	// VÃ©rifier que l'alignement est une puissance de 2
	if ((alignment & (alignment - 1)) != 0)
		return nullptr;

	// Alignement minimum
	if (alignment < sizeof(void *))
		alignment = sizeof(void *);

	void *ptr = nullptr;

#if defined(NKENTSEU_PLATFORM_WINDOWS)
	ptr = _aligned_malloc(size, alignment);
#elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
	if (posix_memalign(&ptr, alignment, size) != 0) {
		ptr = nullptr;
	}
#elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_ANDROID)
#if __STDC_VERSION__ >= 201112L
	// Ajuster la taille pour Ãªtre multiple de l'alignement
	if (size % alignment != 0) {
		size = ((size + alignment - 1) / alignment) * alignment;
	}
	ptr = aligned_alloc(alignment, size);
#else
	if (posix_memalign(&ptr, alignment, size) != 0) {
		ptr = nullptr;
	}
#endif
#else
	// Fallback: sur-allocation
	nk_size totalSize = size + alignment + sizeof(void *);
	void *originalPtr = malloc(totalSize);
	if (!originalPtr)
		return nullptr;

	nk_uintptr addr = reinterpret_cast<nk_uintptr>(originalPtr);
	nk_uintptr alignedAddr = (addr + alignment + sizeof(void *) - 1) & ~(alignment - 1);

	void **storage = reinterpret_cast<void **>(alignedAddr) - 1;
	*storage = originalPtr;

	ptr = reinterpret_cast<void *>(alignedAddr);
#endif

	return static_cast<nk_ptr>(ptr);
}

void NkFreeAligned(nk_ptr ptr) noexcept {
	if (!ptr)
		return;

#if defined(NKENTSEU_PLATFORM_WINDOWS)
	_aligned_free(ptr);
#else
	free(ptr);
#endif
}

nk_bool NkIsPointerAligned(const nk_ptr ptr, nk_size alignment) noexcept {
	return (reinterpret_cast<nk_uintptr>(ptr) & (alignment - 1)) == 0;
}

} // namespace memory
} // namespace platform

} // namespace nkentseu

// ============================================================
// Copyright Â© 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================
