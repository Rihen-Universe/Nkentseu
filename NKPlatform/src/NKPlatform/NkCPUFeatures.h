// -----------------------------------------------------------------------------
// FICHIER: Core\Nkentseu\src\Nkentseu\Platform\NkCPUFeatures.h
// DESCRIPTION: Advanced CPU feature detection (SIMD, cache, performance)
// AUTEUR: Rihen
// DATE: 2026-02-12
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKCPUFEATURES_H_INCLUDED
#define NK_CORE_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKCPUFEATURES_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NkArchDetect.h"
#include <string>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
/**
 * @brief Namespace platform.
 */
namespace platform {

using core::i32;

/**
 * @brief CPU cache information
 */
struct CacheInfo {
	i32 lineSize;		   // Cache line size in bytes
	i32 l1DataSize;		   // L1 data cache size in KB
	i32 l1InstructionSize; // L1 instruction cache size in KB
	i32 l2Size;			   // L2 cache size in KB
	i32 l3Size;			   // L3 cache size in KB

	CacheInfo();
};

/**
 * @brief CPU topology information
 */
struct CPUTopology {
	i32 numPhysicalCores;	// Physical cores
	i32 numLogicalCores;	// Logical cores (with HT/SMT)
	i32 numSockets;			// CPU sockets
	bool hasHyperThreading; // Hyper-Threading/SMT enabled

	CPUTopology();
};

/**
 * @brief SIMD instruction set support
 */
struct SIMDFeatures {
	// x86/x64 features
	bool hasMMX;
	bool hasSSE;
	bool hasSSE2;
	bool hasSSE3;
	bool hasSSSE3;
	bool hasSSE41;
	bool hasSSE42;
	bool hasAVX;
	bool hasAVX2;
	bool hasAVX512F; // Foundation
	bool hasAVX512DQ;
	bool hasAVX512BW;
	bool hasAVX512VL;
	bool hasFMA; // Fused Multiply-Add
	bool hasFMA4;

	// ARM features
	bool hasNEON;
	bool hasSVE; // Scalable Vector Extension
	bool hasSVE2;

	SIMDFeatures();
};

/**
 * @brief Other CPU features
 */
struct ExtendedFeatures {
	// Security
	bool hasAES;	// AES-NI instructions
	bool hasSHA;	// SHA extensions
	bool hasRDRAND; // Hardware random number generator
	bool hasRDSEED; // Seed for PRNG

	// Memory
	bool hasCLFLUSH;	 // Cache line flush
	bool hasCLFLUSHOPT;	 // Optimized cache line flush
	bool hasPREFETCHWT1; // Prefetch to L2
	bool hasMOVBE;		 // Move with byte swap

	// Performance
	bool hasPOPCNT; // Population count
	bool hasLZCNT;	// Leading zero count
	bool hasBMI1;	// Bit Manipulation Instruction Set 1
	bool hasBMI2;	// Bit Manipulation Instruction Set 2
	bool hasADX;	// Multi-precision add-carry

	// Virtualization
	bool hasVMX; // Intel VT-x
	bool hasSVM; // AMD-V

	ExtendedFeatures();
};

/**
 * @brief Complete CPU features structure
 *
 * Détecte et stocke toutes les capacités CPU disponibles.
 * Utilise CPUID sur x86/x64 et runtime checks sur ARM.
 *
 * @example
 * const auto& cpu = CPUFeatures::Get();
 * if (cpu.simd.hasAVX2) {
 *     // Use AVX2 optimized code
 * } else if (cpu.simd.hasSSE42) {
 *     // Use SSE4.2 fallback
 * }
 */
struct CPUFeatures {
	// Vendor and model
	std::string vendor; // "GenuineIntel", "AuthenticAMD", "ARM"
	std::string brand;  // Full processor name
	i32 family;
	i32 model;
	i32 stepping;

	// Topology
	CPUTopology topology;

	// Cache
	CacheInfo cache;

	// SIMD
	SIMDFeatures simd;

	// Extended features
	ExtendedFeatures extended;

	// Frequency (MHz)
	i32 baseFrequency;
	i32 maxFrequency;

	/**
	 * @brief Get singleton instance (lazy initialization)
	 */
	static const CPUFeatures &Get();

	/**
	 * @brief Print CPU information to string
	 */
	std::string ToString() const;

private:
	CPUFeatures();

	// Detection functions
	void DetectVendorAndBrand();
	void DetectTopology();
	void DetectCache();
	void DetectSIMDFeatures();
	void DetectExtendedFeatures();
	void DetectFrequency();

#if NK_ARCH_X86 || NK_ARCH_X64
	void CPUID(i32 function, i32 subfunction, i32 *eax, i32 *ebx, i32 *ecx, i32 *edx);
#endif
};

/**
 * @brief Runtime CPU feature check (inline for performance)
 */
inline bool HasSSE2() {
	return CPUFeatures::Get().simd.hasSSE2;
}

inline bool HasAVX() {
	return CPUFeatures::Get().simd.hasAVX;
}

inline bool HasAVX2() {
	return CPUFeatures::Get().simd.hasAVX2;
}

inline bool HasAVX512() {
	return CPUFeatures::Get().simd.hasAVX512F;
}

inline bool HasNEON() {
	return CPUFeatures::Get().simd.hasNEON;
}

inline bool HasFMA() {
	return CPUFeatures::Get().simd.hasFMA;
}

inline i32 GetCacheLineSize() {
	return CPUFeatures::Get().cache.lineSize;
}

inline i32 GetPhysicalCoreCount() {
	return CPUFeatures::Get().topology.numPhysicalCores;
}

inline i32 GetLogicalCoreCount() {
	return CPUFeatures::Get().topology.numLogicalCores;
}

} // namespace platform
} // namespace nkentseu

#endif // NK_CORE_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKCPUFEATURES_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================
