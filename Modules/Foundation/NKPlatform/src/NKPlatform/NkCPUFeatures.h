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

#include "NKPlatform/NkArchDetect.h"
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
	/**
	 * @brief Namespace platform.
	 */
	namespace platform {

		/**
		 * @brief CPU cache information
		 */
		struct CacheInfo {
			int32_t lineSize;		   // Cache line size in bytes
			int32_t l1DataSize;		   // L1 data cache size in KB
			int32_t l1InstructionSize; // L1 instruction cache size in KB
			int32_t l2Size;			   // L2 cache size in KB
			int32_t l3Size;			   // L3 cache size in KB

			CacheInfo();
		};

		/**
		 * @brief CPU topology information
		 */
		struct CPUTopology {
			int32_t numPhysicalCores;	// Physical cores
			int32_t numLogicalCores;	// Logical cores (with HT/SMT)
			int32_t numSockets;			// CPU sockets
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
			static constexpr size_t VENDOR_CAPACITY = 32u;
			static constexpr size_t BRAND_CAPACITY = 128u;

			// Vendor and model
			char vendor[VENDOR_CAPACITY]; // "GenuineIntel", "AuthenticAMD", "ARM"
			char brand[BRAND_CAPACITY];   // Full processor name
			int32_t family;
			int32_t model;
			int32_t stepping;

			// Topology
			CPUTopology topology;

			// Cache
			CacheInfo cache;

			// SIMD
			SIMDFeatures simd;

			// Extended features
			ExtendedFeatures extended;

			// Frequency (MHz)
			int32_t baseFrequency;
			int32_t maxFrequency;

			/**
			 * @brief Get singleton instance (lazy initialization)
			 */
			static const CPUFeatures &Get();

			/**
			 * @brief Format CPU information in caller-provided buffer.
			 */
			void ToString(char* outBuffer, size_t outBufferSize) const;

			/**
			 * @brief Convenience c-string representation (non thread-safe).
			 */
			const char* ToString() const;

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
				void CPUID(int32_t function, int32_t subfunction, int32_t *eax, int32_t *ebx, int32_t *ecx, int32_t *edx);
			#endif
		};

		// Alias nomenclature NK (compat API existante conservee).
		using NkCacheInfo = CacheInfo;
		using NkCPUTopology = CPUTopology;
		using NkSIMDFeatures = SIMDFeatures;
		using NkExtendedFeatures = ExtendedFeatures;
		using NkCPUFeatures = CPUFeatures;

		inline const NkCPUFeatures &NkGetCPUFeatures() {
			return CPUFeatures::Get();
		}

		/**
		 * @brief Runtime CPU feature checks (API NK officielle).
		 */
		inline bool NkHasSSE2() {
			return CPUFeatures::Get().simd.hasSSE2;
		}

		inline bool NkHasAVX() {
			return CPUFeatures::Get().simd.hasAVX;
		}

		inline bool NkHasAVX2() {
			return CPUFeatures::Get().simd.hasAVX2;
		}

		inline bool NkHasAVX512() {
			return CPUFeatures::Get().simd.hasAVX512F;
		}

		inline bool NkHasNEON() {
			return CPUFeatures::Get().simd.hasNEON;
		}

		inline bool NkHasFMA() {
			return CPUFeatures::Get().simd.hasFMA;
		}

		inline int32_t NkGetCacheLineSize() {
			return CPUFeatures::Get().cache.lineSize;
		}

		inline int32_t NkGetPhysicalCoreCount() {
			return CPUFeatures::Get().topology.numPhysicalCores;
		}

		inline int32_t NkGetLogicalCoreCount() {
			return CPUFeatures::Get().topology.numLogicalCores;
		}

		// Legacy API wrappers are opt-in only.
		#if defined(NKENTSEU_ENABLE_LEGACY_PLATFORM_API)
		[[deprecated("Use NkHasSSE2()")]] inline bool HasSSE2() {
			return NkHasSSE2();
		}
		[[deprecated("Use NkHasAVX()")]] inline bool HasAVX() {
			return NkHasAVX();
		}
		[[deprecated("Use NkHasAVX2()")]] inline bool HasAVX2() {
			return NkHasAVX2();
		}
		[[deprecated("Use NkHasAVX512()")]] inline bool HasAVX512() {
			return NkHasAVX512();
		}
		[[deprecated("Use NkHasNEON()")]] inline bool HasNEON() {
			return NkHasNEON();
		}
		[[deprecated("Use NkHasFMA()")]] inline bool HasFMA() {
			return NkHasFMA();
		}
		[[deprecated("Use NkGetCacheLineSize()")]] inline int32_t GetCacheLineSize() {
			return NkGetCacheLineSize();
		}
		[[deprecated("Use NkGetPhysicalCoreCount()")]] inline int32_t GetPhysicalCoreCount() {
			return NkGetPhysicalCoreCount();
		}
		[[deprecated("Use NkGetLogicalCoreCount()")]] inline int32_t GetLogicalCoreCount() {
			return NkGetLogicalCoreCount();
		}
		#endif

	} // namespace platform
} // namespace nkentseu

#endif // NK_CORE_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKCPUFEATURES_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================
