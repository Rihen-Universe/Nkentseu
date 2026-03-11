// -----------------------------------------------------------------------------
// FICHIER: Core\Nkentseu\src\Nkentseu\Platform\NkCPUFeatures.cpp
// DESCRIPTION: CPU feature detection implementation
// AUTEUR: Rihen
// DATE: 2026-02-12
// -----------------------------------------------------------------------------

#include "NkCPUFeatures.h"
#include "NKPlatform/NkArchDetect.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#ifdef NK_PLATFORM_WINDOWS
#include <windows.h>
#include <intrin.h>
#elif NK_PLATFORM_LINUX || NK_PLATFORM_ANDROID
#include <unistd.h>
#include <sys/sysinfo.h>
#elif NK_PLATFORM_MACOS || NK_PLATFORM_IOS
#include <sys/sysctl.h>
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
/**
 * @brief Namespace platform.
 */
namespace platform {

namespace {

static void CopyStringTruncated(char* dst, size_t dstCount, const char* src) {
	if (!dst || dstCount == 0u) {
		return;
	}
	dst[0] = '\0';
	if (!src) {
		return;
	}
	::snprintf(dst, static_cast<size_t>(dstCount), "%s", src);
}

static void TrimStringToBuffer(const char* text, char* dst, size_t dstCount) {
	if (!dst || dstCount == 0u) {
		return;
	}
	dst[0] = '\0';
	if (!text) {
		return;
	}

	const char* start = text;
	while (*start && ::isspace(static_cast<unsigned char>(*start)) != 0) {
		++start;
	}

	const char* end = text + ::strlen(text);
	while (end > start && ::isspace(static_cast<unsigned char>(*(end - 1))) != 0) {
		--end;
	}

	const size_t trimmedLen = static_cast<size_t>(end - start);
	if (trimmedLen == 0u) {
		return;
	}

	const size_t copyLen = (trimmedLen < static_cast<size_t>(dstCount - 1u)) ? trimmedLen
																			  : static_cast<size_t>(dstCount - 1u);
	::memcpy(dst, start, copyLen);
	dst[copyLen] = '\0';
}

static bool ContainsSubString(const char* text, const char* pattern) {
	return (text && pattern && ::strstr(text, pattern) != nullptr);
}

} // namespace

// ====================================================================
// Constructors
// ====================================================================

CacheInfo::CacheInfo() : lineSize(64), l1DataSize(32), l1InstructionSize(32), l2Size(256), l3Size(8192) {
}

CPUTopology::CPUTopology() : numPhysicalCores(1), numLogicalCores(1), numSockets(1), hasHyperThreading(false) {
}

SIMDFeatures::SIMDFeatures()
	: hasMMX(false), hasSSE(false), hasSSE2(false), hasSSE3(false), hasSSSE3(false), hasSSE41(false), hasSSE42(false),
	  hasAVX(false), hasAVX2(false), hasAVX512F(false), hasAVX512DQ(false), hasAVX512BW(false), hasAVX512VL(false),
	  hasFMA(false), hasFMA4(false), hasNEON(false), hasSVE(false), hasSVE2(false) {
}

ExtendedFeatures::ExtendedFeatures()
	: hasAES(false), hasSHA(false), hasRDRAND(false), hasRDSEED(false), hasCLFLUSH(false), hasCLFLUSHOPT(false),
	  hasPREFETCHWT1(false), hasMOVBE(false), hasPOPCNT(false), hasLZCNT(false), hasBMI1(false), hasBMI2(false),
	  hasADX(false), hasVMX(false), hasSVM(false) {
}

// ====================================================================
// CPUFeatures Implementation
// ====================================================================

CPUFeatures::CPUFeatures()
	: vendor{0}, brand{0}, family(0), model(0), stepping(0), baseFrequency(0), maxFrequency(0) {

	DetectVendorAndBrand();
	DetectTopology();
	DetectCache();
	DetectSIMDFeatures();
	DetectExtendedFeatures();
	DetectFrequency();
}

const CPUFeatures &CPUFeatures::Get() {
	static CPUFeatures instance;
	return instance;
}

// ====================================================================
// CPUID Helper (x86/x64 only)
// ====================================================================

#if NK_ARCH_X86 || NK_ARCH_X64

void CPUFeatures::CPUID(int32_t function, int32_t subfunction, int32_t *eax, int32_t *ebx, int32_t *ecx, int32_t *edx) {
	int32_t regs[4] = {0};

#if defined(_MSC_VER)
		__cpuidex(regs, function, subfunction);
#elif defined(__GNUC__) || defined(__clang__)
		__asm__ __volatile__("cpuid"
							 : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
							 : "a"(function), "c"(subfunction));
#endif

	*eax = regs[0];
	*ebx = regs[1];
	*ecx = regs[2];
	*edx = regs[3];
}

#endif

// ====================================================================
// Vendor and Brand Detection
// ====================================================================

void CPUFeatures::DetectVendorAndBrand() {
#if NK_ARCH_X86 || NK_ARCH_X64

	// Get vendor string
	int32_t eax, ebx, ecx, edx;
	char vendorStr[13] = {0};

	CPUID(0, 0, &eax, &ebx, &ecx, &edx);
	::memcpy(vendorStr + 0, &ebx, 4);
	::memcpy(vendorStr + 4, &edx, 4);
	::memcpy(vendorStr + 8, &ecx, 4);
	vendorStr[12] = '\0';

	CopyStringTruncated(vendor, VENDOR_CAPACITY, vendorStr);

	// Get brand string (CPUID 0x80000002-0x80000004)
	char brandStr[49] = {0};
	int32_t maxExtended;
	CPUID(0x80000000, 0, &maxExtended, &ebx, &ecx, &edx);

	if (maxExtended >= 0x80000004) {
		for (int32_t i = 0; i < 3; ++i) {
			CPUID(0x80000002 + i, 0, &eax, &ebx, &ecx, &edx);
			::memcpy(brandStr + i * 16 + 0, &eax, 4);
			::memcpy(brandStr + i * 16 + 4, &ebx, 4);
			::memcpy(brandStr + i * 16 + 8, &ecx, 4);
			::memcpy(brandStr + i * 16 + 12, &edx, 4);
		}
		brandStr[48] = '\0';
		CopyStringTruncated(brand, BRAND_CAPACITY, brandStr);
	}

	// Get family/model/stepping
	CPUID(1, 0, &eax, &ebx, &ecx, &edx);
	stepping = eax & 0xF;
	model = (eax >> 4) & 0xF;
	family = (eax >> 8) & 0xF;

	// Extended family/model
	int32_t extModel = (eax >> 16) & 0xF;
	int32_t extFamily = (eax >> 20) & 0xFF;

	if (family == 0xF) {
		family += extFamily;
	}
	if (family == 0x6 || family == 0xF) {
		model += (extModel << 4);
	}

#elif NK_ARCH_ARM || NK_ARCH_ARM64

	CopyStringTruncated(vendor, VENDOR_CAPACITY, "ARM");

#ifdef NK_PLATFORM_ANDROID
	// On Android, try to read /proc/cpuinfo
	FILE *f = fopen("/proc/cpuinfo", "r");
	if (f) {
		char line[256];
		while (fgets(line, sizeof(line), f)) {
			if (strncmp(line, "Hardware", 8) == 0) {
				char *colon = strchr(line, ':');
				if (colon) {
					TrimStringToBuffer(colon + 2, brand, BRAND_CAPACITY);
				}
				break;
			}
		}
		fclose(f);
	}
#endif

	if (brand[0] == '\0') {
		CopyStringTruncated(brand, BRAND_CAPACITY, "ARM Processor");
	}

#else

	CopyStringTruncated(vendor, VENDOR_CAPACITY, "Unknown");
	CopyStringTruncated(brand, BRAND_CAPACITY, "Unknown Processor");

#endif
}

// ====================================================================
// Topology Detection
// ====================================================================

void CPUFeatures::DetectTopology() {
	// Start with conservative defaults, then refine per platform.
	topology.numLogicalCores = 1;
	topology.numPhysicalCores = 1;

#ifdef NK_PLATFORM_WINDOWS

	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	topology.numLogicalCores = static_cast<int32_t>(sysInfo.dwNumberOfProcessors);

	// Try to get physical core count
	DWORD length = 0;
	GetLogicalProcessorInformation(nullptr, &length);
	if (length > 0) {
		auto *buffer = new SYSTEM_LOGICAL_PROCESSOR_INFORMATION[length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION)];
		if (GetLogicalProcessorInformation(buffer, &length)) {
			int32_t physicalCores = 0;
			for (DWORD i = 0; i < length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); ++i) {
				if (buffer[i].Relationship == RelationProcessorCore) {
					++physicalCores;
				}
			}
			topology.numPhysicalCores = physicalCores;
		}
		delete[] buffer;
	}

#elif NK_PLATFORM_LINUX || NK_PLATFORM_ANDROID

	// Try sysconf
	long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
	if (nprocs > 0) {
		topology.numLogicalCores = static_cast<int32_t>(nprocs);
	}

	// Try to parse /sys/devices/system/cpu/cpu*/topology/thread_siblings_list
	// to determine physical cores (simplified)
	topology.numPhysicalCores = topology.numLogicalCores;

#elif NK_PLATFORM_MACOS || NK_PLATFORM_IOS

	int logicalCores = 0;
	int physicalCores = 0;
	size_t len = sizeof(logicalCores);

	if (sysctlbyname("hw.logicalcpu", &logicalCores, &len, nullptr, 0) == 0) {
		topology.numLogicalCores = logicalCores;
	}

	len = sizeof(physicalCores);
	if (sysctlbyname("hw.physicalcpu", &physicalCores, &len, nullptr, 0) == 0) {
		topology.numPhysicalCores = physicalCores;
	}

#else

	topology.numPhysicalCores = topology.numLogicalCores;

#endif

	// Detect Hyper-Threading
	topology.hasHyperThreading = (topology.numLogicalCores > topology.numPhysicalCores);
	topology.numSockets = 1; // Default
}

// ====================================================================
// Cache Detection
// ====================================================================

void CPUFeatures::DetectCache() {
#if NK_ARCH_X86 || NK_ARCH_X64

	int32_t eax, ebx, ecx, edx;

	// Intel cache detection (CPUID leaf 4)
	if (ContainsSubString(vendor, "Intel")) {
		for (int32_t i = 0; i < 16; ++i) {
			CPUID(4, i, &eax, &ebx, &ecx, &edx);

			int32_t cacheType = eax & 0x1F;
			if (cacheType == 0)
				break; // No more caches

			int32_t cacheLevel = (eax >> 5) & 0x7;
			int32_t sets = ecx + 1;
			int32_t lineSize = (ebx & 0xFFF) + 1;
			int32_t partitions = ((ebx >> 12) & 0x3FF) + 1;
			int32_t ways = ((ebx >> 22) & 0x3FF) + 1;
			int32_t size = (ways * partitions * lineSize * sets) / 1024;

			cache.lineSize = lineSize;

			if (cacheLevel == 1 && cacheType == 1) {
				cache.l1DataSize = size;
			} else if (cacheLevel == 1 && cacheType == 2) {
				cache.l1InstructionSize = size;
			} else if (cacheLevel == 2) {
				cache.l2Size = size;
			} else if (cacheLevel == 3) {
				cache.l3Size = size;
			}
		}
	}
	// AMD cache detection (CPUID 0x80000006)
	else if (ContainsSubString(vendor, "AMD")) {
		CPUID(0x80000006, 0, &eax, &ebx, &ecx, &edx);
		cache.lineSize = ecx & 0xFF;
		cache.l2Size = (ecx >> 16) & 0xFFFF;

		// L3 cache
		CPUID(0x80000006, 0, &eax, &ebx, &ecx, &edx);
		cache.l3Size = ((edx >> 18) & 0x3FFF) * 512;
	}

#elif NK_PLATFORM_MACOS || NK_PLATFORM_IOS

	size_t len = sizeof(int32_t);
	int32_t value;

	if (sysctlbyname("hw.cachelinesize", &value, &len, nullptr, 0) == 0) {
		cache.lineSize = value;
	}
	if (sysctlbyname("hw.l1dcachesize", &value, &len, nullptr, 0) == 0) {
		cache.l1DataSize = value / 1024;
	}
	if (sysctlbyname("hw.l2cachesize", &value, &len, nullptr, 0) == 0) {
		cache.l2Size = value / 1024;
	}
	if (sysctlbyname("hw.l3cachesize", &value, &len, nullptr, 0) == 0) {
		cache.l3Size = value / 1024;
	}

#else

	// Defaults already set in constructor

#endif
}

// ====================================================================
// SIMD Features Detection
// ====================================================================

void CPUFeatures::DetectSIMDFeatures() {
#if NK_ARCH_X86 || NK_ARCH_X64

	int32_t eax, ebx, ecx, edx;

	// CPUID(1): Basic features
	CPUID(1, 0, &eax, &ebx, &ecx, &edx);

	simd.hasMMX = (edx & (1 << 23)) != 0;
	simd.hasSSE = (edx & (1 << 25)) != 0;
	simd.hasSSE2 = (edx & (1 << 26)) != 0;
	simd.hasSSE3 = (ecx & (1 << 0)) != 0;
	simd.hasSSSE3 = (ecx & (1 << 9)) != 0;
	simd.hasSSE41 = (ecx & (1 << 19)) != 0;
	simd.hasSSE42 = (ecx & (1 << 20)) != 0;
	simd.hasAVX = (ecx & (1 << 28)) != 0;
	simd.hasFMA = (ecx & (1 << 12)) != 0;

	// CPUID(7): Extended features
	CPUID(7, 0, &eax, &ebx, &ecx, &edx);

	simd.hasAVX2 = (ebx & (1 << 5)) != 0;
	simd.hasAVX512F = (ebx & (1 << 16)) != 0;
	simd.hasAVX512DQ = (ebx & (1 << 17)) != 0;
	simd.hasAVX512BW = (ebx & (1 << 30)) != 0;
	simd.hasAVX512VL = (ebx & (1 << 31)) != 0;

	// AMD-specific
	if (ContainsSubString(vendor, "AMD")) {
		CPUID(0x80000001, 0, &eax, &ebx, &ecx, &edx);
		simd.hasFMA4 = (ecx & (1 << 16)) != 0;
	}

#elif NK_ARCH_ARM64

	// ARM64 always has NEON
	simd.hasNEON = true;

#ifdef NK_PLATFORM_LINUX || NK_PLATFORM_ANDROID
	// Check for SVE support
	FILE *f = fopen("/proc/cpuinfo", "r");
	if (f) {
		char line[256];
		while (fgets(line, sizeof(line), f)) {
			if (strstr(line, "sve")) {
				simd.hasSVE = true;
			}
			if (strstr(line, "sve2")) {
				simd.hasSVE2 = true;
			}
		}
		fclose(f);
	}
#endif

#elif NK_ARCH_ARM

// ARMv7 may or may not have NEON
#ifdef __ARM_NEON
	simd.hasNEON = true;
#endif

#endif
}

// ====================================================================
// Extended Features Detection
// ====================================================================

void CPUFeatures::DetectExtendedFeatures() {
#if NK_ARCH_X86 || NK_ARCH_X64

	int32_t eax, ebx, ecx, edx;

	// CPUID(1)
	CPUID(1, 0, &eax, &ebx, &ecx, &edx);

	extended.hasCLFLUSH = (edx & (1 << 19)) != 0;
	extended.hasMOVBE = (ecx & (1 << 22)) != 0;
	extended.hasPOPCNT = (ecx & (1 << 23)) != 0;
	extended.hasAES = (ecx & (1 << 25)) != 0;
	extended.hasRDRAND = (ecx & (1 << 30)) != 0;

	// CPUID(7)
	CPUID(7, 0, &eax, &ebx, &ecx, &edx);

	extended.hasBMI1 = (ebx & (1 << 3)) != 0;
	extended.hasBMI2 = (ebx & (1 << 8)) != 0;
	extended.hasADX = (ebx & (1 << 19)) != 0;
	extended.hasSHA = (ebx & (1 << 29)) != 0;
	extended.hasCLFLUSHOPT = (ebx & (1 << 23)) != 0;
	extended.hasPREFETCHWT1 = (ecx & (1 << 0)) != 0;
	extended.hasRDSEED = (ebx & (1 << 18)) != 0;

	// Extended CPUID
	CPUID(0x80000001, 0, &eax, &ebx, &ecx, &edx);
	extended.hasLZCNT = (ecx & (1 << 5)) != 0;

#endif
}

// ====================================================================
// Frequency Detection
// ====================================================================

void CPUFeatures::DetectFrequency() {
#ifdef NK_PLATFORM_WINDOWS

	HKEY hKey;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) ==
		ERROR_SUCCESS) {
		DWORD mhz = 0;
		DWORD size = sizeof(mhz);
		if (RegQueryValueExA(hKey, "~MHz", nullptr, nullptr, (LPBYTE)&mhz, &size) == ERROR_SUCCESS) {
			baseFrequency = static_cast<int32_t>(mhz);
			maxFrequency = baseFrequency;
		}
		RegCloseKey(hKey);
	}

#elif NK_PLATFORM_MACOS || NK_PLATFORM_IOS

	uint64_t freq = 0;
	size_t len = sizeof(freq);
	if (sysctlbyname("hw.cpufrequency", &freq, &len, nullptr, 0) == 0) {
		baseFrequency = static_cast<int32_t>(freq / 1000000); // Convert to MHz
		maxFrequency = baseFrequency;
	}

#elif NK_PLATFORM_LINUX || NK_PLATFORM_ANDROID

	FILE *f = fopen("/proc/cpuinfo", "r");
	if (f) {
		char line[256];
		while (fgets(line, sizeof(line), f)) {
			if (strncmp(line, "cpu MHz", 7) == 0) {
				float mhz = 0;
				if (sscanf(line, "cpu MHz : %f", &mhz) == 1) {
					baseFrequency = static_cast<int32_t>(mhz);
					maxFrequency = baseFrequency;
					break;
				}
			}
		}
		fclose(f);
	}

#endif
}

// ====================================================================
// ToString
// ====================================================================

void CPUFeatures::ToString(char* outBuffer, size_t outBufferSize) const {
	if (!outBuffer || outBufferSize == 0u) {
		return;
	}
	outBuffer[0] = '\0';

	char buffer[2048];
	int offset = 0;

	offset += snprintf(buffer + offset, sizeof(buffer) - offset, "CPU Information:\n");
	offset += snprintf(buffer + offset, sizeof(buffer) - offset, "  Vendor: %s\n", vendor);
	offset += snprintf(buffer + offset, sizeof(buffer) - offset, "  Brand: %s\n", brand);
	offset += snprintf(buffer + offset, sizeof(buffer) - offset, "  Family: %d, Model: %d, Stepping: %d\n", family,
					   model, stepping);
	offset += snprintf(buffer + offset, sizeof(buffer) - offset, "  Base Frequency: %d MHz, Max: %d MHz\n",
					   baseFrequency, maxFrequency);

	offset += snprintf(buffer + offset, sizeof(buffer) - offset, "\nTopology:\n");
	offset += snprintf(buffer + offset, sizeof(buffer) - offset, "  Physical Cores: %d\n", topology.numPhysicalCores);
	offset += snprintf(buffer + offset, sizeof(buffer) - offset, "  Logical Cores: %d\n", topology.numLogicalCores);
	offset += snprintf(buffer + offset, sizeof(buffer) - offset, "  Hyper-Threading: %s\n",
					   topology.hasHyperThreading ? "Yes" : "No");

	offset += snprintf(buffer + offset, sizeof(buffer) - offset, "\nCache:\n");
	offset += snprintf(buffer + offset, sizeof(buffer) - offset, "  Line Size: %d bytes\n", cache.lineSize);
	offset += snprintf(buffer + offset, sizeof(buffer) - offset, "  L1 Data: %d KB, L1 Instruction: %d KB\n",
					   cache.l1DataSize, cache.l1InstructionSize);
	offset +=
		snprintf(buffer + offset, sizeof(buffer) - offset, "  L2: %d KB, L3: %d KB\n", cache.l2Size, cache.l3Size);

	offset += snprintf(buffer + offset, sizeof(buffer) - offset, "\nSIMD Features:\n");
	if (simd.hasSSE)
		offset += snprintf(buffer + offset, sizeof(buffer) - offset, "  SSE ");
	if (simd.hasSSE2)
		offset += snprintf(buffer + offset, sizeof(buffer) - offset, "SSE2 ");
	if (simd.hasSSE3)
		offset += snprintf(buffer + offset, sizeof(buffer) - offset, "SSE3 ");
	if (simd.hasSSE41)
		offset += snprintf(buffer + offset, sizeof(buffer) - offset, "SSE4.1 ");
	if (simd.hasSSE42)
		offset += snprintf(buffer + offset, sizeof(buffer) - offset, "SSE4.2 ");
	if (simd.hasAVX)
		offset += snprintf(buffer + offset, sizeof(buffer) - offset, "AVX ");
	if (simd.hasAVX2)
		offset += snprintf(buffer + offset, sizeof(buffer) - offset, "AVX2 ");
	if (simd.hasAVX512F)
		offset += snprintf(buffer + offset, sizeof(buffer) - offset, "AVX-512F ");
	if (simd.hasFMA)
		offset += snprintf(buffer + offset, sizeof(buffer) - offset, "FMA ");
	if (simd.hasNEON)
		offset += snprintf(buffer + offset, sizeof(buffer) - offset, "NEON ");
	offset += snprintf(buffer + offset, sizeof(buffer) - offset, "\n");

	::snprintf(outBuffer, static_cast<size_t>(outBufferSize), "%s", buffer);
}

const char* CPUFeatures::ToString() const {
	static char sBuffer[2048];
	ToString(sBuffer, sizeof(sBuffer));
	return sBuffer;
}

} // namespace platform
} // namespace nkentseu

// ============================================================
// Copyright Â© 2024-2026 Rihen. All rights reserved.
// ============================================================
