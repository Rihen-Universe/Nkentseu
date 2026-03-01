// -----------------------------------------------------------------------------
// FICHIER: Core\Nkentseu\src\Nkentseu\Platform\NkArchDetect.h
// DESCRIPTION: Détection d'architecture CPU et définition des macros
// AUTEUR: Rihen
// DATE: 2026-02-10
// VERSION: 1.1.0
// MODIFICATIONS: Ajout de NKENTSEU_ARCH_VERSION pour toutes les architectures
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKARCHDETECT_H_INCLUDED
#define NKENTSEU_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKARCHDETECT_H_INCLUDED

// ============================================================
// INCLUDES
// ============================================================

#include <cstddef>
#include <cstdint>

// ============================================================
// MACROS DE DÉTECTION D'ARCHITECTURE
// ============================================================

/**
 * @brief Macro de détection d'architecture x86_64
 * @def NKENTSEU_ARCH_X86_64
 * @ingroup ArchitectureDetection
 * @see NKENTSEU_ARCH_64BIT
 */

/**
 * @brief Macro de détection d'architecture ARM64
 * @def NKENTSEU_ARCH_ARM64
 * @ingroup ArchitectureDetection
 * @see NKENTSEU_ARCH_64BIT
 */

/**
 * @brief Macro de détection d'architecture 64-bit
 * @def NKENTSEU_ARCH_64BIT
 * @ingroup ArchitectureDetection
 */

/**
 * @brief Macro de nom d'architecture
 * @def NKENTSEU_ARCH_NAME
 * @ingroup ArchitectureDetection
 * @return Chaîne constante représentant l'architecture détectée
 */

/**
 * @brief Macro de version d'architecture
 * @def NKENTSEU_ARCH_VERSION
 * @ingroup ArchitectureDetection
 * @return Chaîne constante représentant la version de l'architecture
 */

// Architectures 64-bit
#if defined(__x86_64__) || defined(_M_X64) || defined(__amd64__) || defined(__amd64)
#define NKENTSEU_ARCH_X86_64
#define NKENTSEU_ARCH_64BIT
#define NKENTSEU_ARCH_NAME "x86_64"
#define NKENTSEU_ARCH_VERSION "AMD64/Intel 64-bit"

#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__ARM64__) || defined(__arm64__)
#define NKENTSEU_ARCH_ARM64
#define NKENTSEU_ARCH_64BIT
#define NKENTSEU_ARCH_NAME "ARM64"
#define NKENTSEU_ARCH_VERSION "ARMv8 64-bit"

#elif defined(__ppc64__) || defined(__PPC64__) || defined(_ARCH_PPC64)
#define NKENTSEU_ARCH_PPC64
#define NKENTSEU_ARCH_64BIT
#define NKENTSEU_ARCH_NAME "PowerPC64"
#define NKENTSEU_ARCH_VERSION "PowerPC 64-bit"

#elif defined(__mips64) || defined(__mips64__)
#define NKENTSEU_ARCH_MIPS64
#define NKENTSEU_ARCH_64BIT
#define NKENTSEU_ARCH_NAME "MIPS64"
#define NKENTSEU_ARCH_VERSION "MIPS 64-bit"

#elif defined(__riscv) && (__riscv_xlen == 64)
#define NKENTSEU_ARCH_RISCV64
#define NKENTSEU_ARCH_64BIT
#define NKENTSEU_ARCH_NAME "RISC-V 64"
#define NKENTSEU_ARCH_VERSION "RISC-V 64-bit"

#elif defined(__sparc_v9__) || defined(__sparcv9)
#define NKENTSEU_ARCH_SPARC64
#define NKENTSEU_ARCH_64BIT
#define NKENTSEU_ARCH_NAME "SPARC64"
#define NKENTSEU_ARCH_VERSION "SPARC 64-bit"

// Architectures 32-bit
#elif defined(__i386__) || defined(_M_IX86) || defined(__i386)
#define NKENTSEU_ARCH_X86
#define NKENTSEU_ARCH_32BIT
#define NKENTSEU_ARCH_NAME "x86"
#define NKENTSEU_ARCH_VERSION "Intel x86 32-bit"

#elif defined(__arm__) || defined(_M_ARM) || defined(__ARM__) || defined(__arm)
#define NKENTSEU_ARCH_ARM
#define NKENTSEU_ARCH_32BIT
#define NKENTSEU_ARCH_NAME "ARM"

// Détection de version ARM spécifique
#if defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__)
#define NKENTSEU_ARCH_VERSION "ARMv7 32-bit"
#elif defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__)
#define NKENTSEU_ARCH_VERSION "ARMv6 32-bit"
#elif defined(__ARM_ARCH_5TEJ__)
#define NKENTSEU_ARCH_VERSION "ARMv5TEJ 32-bit"
#elif defined(__ARM_ARCH_5TE__)
#define NKENTSEU_ARCH_VERSION "ARMv5TE 32-bit"
#else
#define NKENTSEU_ARCH_VERSION "ARM 32-bit"
#endif

#elif defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC)
#define NKENTSEU_ARCH_PPC
#define NKENTSEU_ARCH_32BIT
#define NKENTSEU_ARCH_NAME "PowerPC"
#define NKENTSEU_ARCH_VERSION "PowerPC 32-bit"

#elif defined(__mips__) || defined(__mips) || defined(_MIPS_ISA)
#define NKENTSEU_ARCH_MIPS
#define NKENTSEU_ARCH_32BIT
#define NKENTSEU_ARCH_NAME "MIPS"
#define NKENTSEU_ARCH_VERSION "MIPS 32-bit"

#elif defined(__sparc__) || defined(__sparc)
#define NKENTSEU_ARCH_SPARC
#define NKENTSEU_ARCH_32BIT
#define NKENTSEU_ARCH_NAME "SPARC"
#define NKENTSEU_ARCH_VERSION "SPARC 32-bit"

#elif defined(__e2k__) || defined(__E2K__)
#define NKENTSEU_ARCH_ELBRUS
#define NKENTSEU_ARCH_ELBRUS_VLIW
#define NKENTSEU_ARCH_NAME "Elbrus"
#define NKENTSEU_ARCH_VERSION "Elbrus VLIW"

#elif defined(__sh__) || defined(__sh)
#define NKENTSEU_ARCH_SUPERH
#define NKENTSEU_ARCH_NAME "SuperH"
#define NKENTSEU_ARCH_VERSION "Hitachi SuperH"

#elif defined(__alpha__) || defined(__alpha)
#define NKENTSEU_ARCH_ALPHA
#define NKENTSEU_ARCH_NAME "Alpha"
#define NKENTSEU_ARCH_VERSION "DEC Alpha"

#elif defined(__hppa__) || defined(__hppa)
#define NKENTSEU_ARCH_PARISC
#define NKENTSEU_ARCH_NAME "PA-RISC"
#define NKENTSEU_ARCH_VERSION "HP PA-RISC"

#elif defined(__ia64__) || defined(_M_IA64)
#define NKENTSEU_ARCH_IA64
#define NKENTSEU_ARCH_NAME "IA-64 (Itanium)"
#define NKENTSEU_ARCH_VERSION "Intel Itanium 64-bit"

#elif defined(__m68k__) || defined(__m68k)
#define NKENTSEU_ARCH_M68K
#define NKENTSEU_ARCH_NAME "Motorola 68000"
#define NKENTSEU_ARCH_VERSION "Motorola 68K"

#elif defined(__s390__) || defined(__s390)
#define NKENTSEU_ARCH_S390
#define NKENTSEU_ARCH_NAME "IBM System/390"
#define NKENTSEU_ARCH_VERSION "IBM S/390"

#elif defined(__tile__) || defined(__tile)
#define NKENTSEU_ARCH_TILE
#define NKENTSEU_ARCH_NAME "TILE"
#define NKENTSEU_ARCH_VERSION "Tilera TILE"

#elif defined(__xtensa__) || defined(__xtensa)
#define NKENTSEU_ARCH_XTENSA
#define NKENTSEU_ARCH_NAME "Xtensa"
#define NKENTSEU_ARCH_VERSION "Tensilica Xtensa"

// Architectures spécifiques consoles
#elif defined(_EE) || defined(__R5900__)
#define NKENTSEU_ARCH_R5900
#define NKENTSEU_ARCH_MIPS
#define NKENTSEU_ARCH_64BIT
#define NKENTSEU_ARCH_NAME "R5900"
#define NKENTSEU_ARCH_VERSION "MIPS R5900 (PS2 Emotion Engine)"

#elif defined(__PPU__)
#define NKENTSEU_ARCH_CELL_PPU
#define NKENTSEU_ARCH_PPC
#define NKENTSEU_ARCH_64BIT
#define NKENTSEU_ARCH_NAME "Cell PPU"
#define NKENTSEU_ARCH_VERSION "Cell Broadband Engine PPU (PS3)"

// Architecture inconnue - tentative de détection par taille de pointeur
#else
#if defined(__SIZEOF_POINTER__)
#if __SIZEOF_POINTER__ == 8
#define NKENTSEU_ARCH_64BIT
#define NKENTSEU_ARCH_UNKNOWN_64
#define NKENTSEU_ARCH_NAME "Unknown 64-bit"
#define NKENTSEU_ARCH_VERSION "Unknown 64-bit Architecture"
#elif __SIZEOF_POINTER__ == 4
#define NKENTSEU_ARCH_32BIT
#define NKENTSEU_ARCH_UNKNOWN_32
#define NKENTSEU_ARCH_NAME "Unknown 32-bit"
#define NKENTSEU_ARCH_VERSION "Unknown 32-bit Architecture"
#else
#define NKENTSEU_ARCH_UNKNOWN
#define NKENTSEU_ARCH_NAME "Unknown"
#define NKENTSEU_ARCH_VERSION "Unknown Architecture"
#endif
#else
// Dernière tentative : utiliser la taille d'un pointeur
#if UINTPTR_MAX == UINT64_MAX
#define NKENTSEU_ARCH_64BIT
#define NKENTSEU_ARCH_UNKNOWN_64
#define NKENTSEU_ARCH_NAME "Unknown 64-bit"
#define NKENTSEU_ARCH_VERSION "Unknown 64-bit Architecture"
#elif UINTPTR_MAX == UINT32_MAX
#define NKENTSEU_ARCH_32BIT
#define NKENTSEU_ARCH_UNKNOWN_32
#define NKENTSEU_ARCH_NAME "Unknown 32-bit"
#define NKENTSEU_ARCH_VERSION "Unknown 32-bit Architecture"
#else
#define NKENTSEU_ARCH_UNKNOWN
#define NKENTSEU_ARCH_NAME "Unknown"
#define NKENTSEU_ARCH_VERSION "Unknown Architecture"
#endif
#endif
#endif

// ============================================================
// MACROS DE CONVENANCE POUR COMPATIBILITÉ
// ============================================================

/**
 * @brief Macro de détection d'architecture Intel (x86/x86_64)
 * @def NKENTSEU_ARCH_INTEL
 * @ingroup ArchitectureDetection
 */

/**
 * @brief Macro de détection d'architecture ARM (ARM/ARM64)
 * @def NKENTSEU_ARCH_ARM_FAMILY
 * @ingroup ArchitectureDetection
 */

/**
 * @brief Macro de détection d'endianness little-endian
 * @def NKENTSEU_ARCH_LITTLE_ENDIAN
 * @ingroup ArchitectureDetection
 */

/**
 * @brief Macro de détection d'endianness big-endian
 * @def NKENTSEU_ARCH_BIG_ENDIAN
 * @ingroup ArchitectureDetection
 */

// Définitions rétrocompatibles avec l'ancien système
#if defined(NKENTSEU_ARCH_X86_64) || defined(NKENTSEU_ARCH_X86)
#define NKENTSEU_ARCH_INTEL
#endif

#if defined(NKENTSEU_ARCH_ARM64) || defined(NKENTSEU_ARCH_ARM)
#define NKENTSEU_ARCH_ARM_FAMILY
#endif

// Définition générique pour l'endianness (détection séparée)
#if !defined(NKENTSEU_ARCH_LITTLE_ENDIAN) && !defined(NKENTSEU_ARCH_BIG_ENDIAN)
// Tentative de détection automatique
#if defined(__BYTE_ORDER__)
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define NKENTSEU_ARCH_LITTLE_ENDIAN
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define NKENTSEU_ARCH_BIG_ENDIAN
#endif
#elif defined(__LITTLE_ENDIAN__) || defined(_WIN32) || defined(__i386__) || defined(__x86_64__)
// Windows, x86 et x86_64 sont little-endian
#define NKENTSEU_ARCH_LITTLE_ENDIAN
#elif defined(__BIG_ENDIAN__) || defined(__ARMEB__) || defined(__THUMBEB__)
#define NKENTSEU_ARCH_BIG_ENDIAN
#else
// Par défaut, supposer little-endian (plus courant)
#define NKENTSEU_ARCH_LITTLE_ENDIAN
#endif
#endif

// ============================================================
// MACROS DE DÉTECTION DE FONCTIONNALITÉS CPU SPÉCIFIQUES
// ============================================================

/**
 * @brief Macro de détection de support SSE
 * @def NKENTSEU_CPU_HAS_SSE
 * @ingroup CpuFeatures
 */

/**
 * @brief Macro de détection de support SSE2
 * @def NKENTSEU_CPU_HAS_SSE2
 * @ingroup CpuFeatures
 */

/**
 * @brief Macro de détection de support SSE3
 * @def NKENTSEU_CPU_HAS_SSE3
 * @ingroup CpuFeatures
 */

/**
 * @brief Macro de détection de support SSSE3
 * @def NKENTSEU_CPU_HAS_SSSE3
 * @ingroup CpuFeatures
 */

/**
 * @brief Macro de détection de support SSE4.1
 * @def NKENTSEU_CPU_HAS_SSE4_1
 * @ingroup CpuFeatures
 */

/**
 * @brief Macro de détection de support SSE4.2
 * @def NKENTSEU_CPU_HAS_SSE4_2
 * @ingroup CpuFeatures
 */

/**
 * @brief Macro de détection de support AVX
 * @def NKENTSEU_CPU_HAS_AVX
 * @ingroup CpuFeatures
 */

/**
 * @brief Macro de détection de support AVX2
 * @def NKENTSEU_CPU_HAS_AVX2
 * @ingroup CpuFeatures
 */

/**
 * @brief Macro de détection de support AVX512
 * @def NKENTSEU_CPU_HAS_AVX512
 * @ingroup CpuFeatures
 */

/**
 * @brief Macro de détection de support AES-NI
 * @def NKENTSEU_CPU_HAS_AES
 * @ingroup CpuFeatures
 */

/**
 * @brief Macro de détection de support BMI
 * @def NKENTSEU_CPU_HAS_BMI
 * @ingroup CpuFeatures
 */

/**
 * @brief Macro de détection de support FMA
 * @def NKENTSEU_CPU_HAS_FMA
 * @ingroup CpuFeatures
 */

/**
 * @brief Macro de détection de support NEON (ARM)
 * @def NKENTSEU_CPU_HAS_NEON
 * @ingroup CpuFeatures
 */

/**
 * @brief Macro de détection de support crypto ARM
 * @def NKENTSEU_CPU_HAS_ARM_CRYPTO
 * @ingroup CpuFeatures
 */

/**
 * @brief Macro de détection de support CRC32 ARM
 * @def NKENTSEU_CPU_HAS_ARM_CRC32
 * @ingroup CpuFeatures
 */

/**
 * @brief Macro de détection de support Altivec/VMX (PowerPC)
 * @def NKENTSEU_CPU_HAS_ALTIVEC
 * @ingroup CpuFeatures
 */

/**
 * @brief Macro de détection de support VSX (PowerPC)
 * @def NKENTSEU_CPU_HAS_VSX
 * @ingroup CpuFeatures
 */

/**
 * @brief Macro de détection de support MSA (MIPS)
 * @def NKENTSEU_CPU_HAS_MSA
 * @ingroup CpuFeatures
 */

// SIMD et extensions vectorielles
#if defined(NKENTSEU_ARCH_X86) || defined(NKENTSEU_ARCH_X86_64)
// SSE - Streaming SIMD Extensions
#if defined(__SSE__) || (_M_IX86_FP >= 1) || defined(_M_X64)
#define NKENTSEU_CPU_HAS_SSE
#endif

#if defined(__SSE2__) || (_M_IX86_FP >= 2) || defined(_M_X64)
#define NKENTSEU_CPU_HAS_SSE2
#endif

#if defined(__SSE3__)
#define NKENTSEU_CPU_HAS_SSE3
#endif

#if defined(__SSSE3__)
#define NKENTSEU_CPU_HAS_SSSE3
#endif

#if defined(__SSE4_1__)
#define NKENTSEU_CPU_HAS_SSE4_1
#endif

#if defined(__SSE4_2__)
#define NKENTSEU_CPU_HAS_SSE4_2
#endif

#if defined(__AVX__)
#define NKENTSEU_CPU_HAS_AVX
#endif

#if defined(__AVX2__)
#define NKENTSEU_CPU_HAS_AVX2
#endif

#if defined(__AVX512F__)
#define NKENTSEU_CPU_HAS_AVX512
#endif

// AES - Advanced Encryption Standard
#if defined(__AES__)
#define NKENTSEU_CPU_HAS_AES
#endif

// BMI - Bit Manipulation Instructions
#if defined(__BMI__) || defined(__BMI2__)
#define NKENTSEU_CPU_HAS_BMI
#endif

// FMA - Fused Multiply-Add
#if defined(__FMA__)
#define NKENTSEU_CPU_HAS_FMA
#endif

#elif defined(NKENTSEU_ARCH_ARM) || defined(NKENTSEU_ARCH_ARM64)
// NEON - ARM SIMD
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#define NKENTSEU_CPU_HAS_NEON
#endif

// Crypto extensions ARM
#if defined(__ARM_FEATURE_CRYPTO)
#define NKENTSEU_CPU_HAS_ARM_CRYPTO
#endif

// CRC32 extensions
#if defined(__ARM_FEATURE_CRC32)
#define NKENTSEU_CPU_HAS_ARM_CRC32
#endif

#elif defined(NKENTSEU_ARCH_PPC) || defined(NKENTSEU_ARCH_PPC64)
// Altivec/VMX - PowerPC SIMD
#if defined(__ALTIVEC__) || defined(__VEC__)
#define NKENTSEU_CPU_HAS_ALTIVEC
#endif

// VSX - Vector Scalar Extensions
#if defined(__VSX__)
#define NKENTSEU_CPU_HAS_VSX
#endif

#elif defined(NKENTSEU_ARCH_MIPS) || defined(NKENTSEU_ARCH_MIPS64)
// MSA - MIPS SIMD Architecture
#if defined(__mips_msa)
#define NKENTSEU_CPU_HAS_MSA
#endif
#endif

// ============================================================
// MACROS CONSTANTES POUR L'ALIGNEMENT MÉMOIRE
// ============================================================

/**
 * @brief Taille de ligne de cache en octets
 * @def NKENTSEU_CACHE_LINE_SIZE
 * @value 64 pour les architectures 64-bit modernes
 * @value 32 pour les architectures 32-bit
 * @ingroup MemoryConstants
 */

/**
 * @brief Alignement maximal supporté
 * @def NKENTSEU_MAX_ALIGNMENT
 * @ingroup MemoryConstants
 */

#if defined(NKENTSEU_ARCH_X86_64) || defined(NKENTSEU_ARCH_ARM64)
// Architectures 64-bit modernes - alignement sur 64 octets pour les caches
#define NKENTSEU_CACHE_LINE_SIZE 64
#define NKENTSEU_MAX_ALIGNMENT 64

#elif defined(NKENTSEU_ARCH_X86) || defined(NKENTSEU_ARCH_ARM)
// Architectures 32-bit - alignement sur 32 octets
#define NKENTSEU_CACHE_LINE_SIZE 32
#define NKENTSEU_MAX_ALIGNMENT 32

#else
// Valeur par défaut sécurisée
#define NKENTSEU_CACHE_LINE_SIZE 64
#define NKENTSEU_MAX_ALIGNMENT 64
#endif

// ============================================================
// MACROS CONSTANTES DE TAILLE DE PAGE MÉMOIRE
// ============================================================

/**
 * @brief Taille de page mémoire standard en octets
 * @def NKENTSEU_PAGE_SIZE
 * @value 4096 pour x86/x64 (4KB)
 * @value 16384 pour ARM avec pages 16KB
 * @value 65536 pour ARM avec pages 64KB
 * @ingroup MemoryConstants
 */

/**
 * @brief Taille de page énorme (huge page) en octets
 * @def NKENTSEU_HUGE_PAGE_SIZE
 * @value 2MB pour la plupart des architectures
 * @ingroup MemoryConstants
 */

#if defined(NKENTSEU_ARCH_X86_64) || defined(NKENTSEU_ARCH_X86)
// x86/x64: généralement 4KB, parfois 2MB/1GB avec huge pages
#define NKENTSEU_PAGE_SIZE 4096
#define NKENTSEU_HUGE_PAGE_SIZE (2 * 1024 * 1024) // 2MB

#elif defined(NKENTSEU_ARCH_ARM64) || defined(NKENTSEU_ARCH_ARM)
// ARM: généralement 4KB, peut être 16KB ou 64KB
#if defined(__ARM_PAGE_SIZE_4KB) || !defined(__ARM_PAGE_SIZE)
#define NKENTSEU_PAGE_SIZE 4096
#elif defined(__ARM_PAGE_SIZE_16KB)
#define NKENTSEU_PAGE_SIZE 16384
#elif defined(__ARM_PAGE_SIZE_64KB)
#define NKENTSEU_PAGE_SIZE 65536
#else
#define NKENTSEU_PAGE_SIZE 4096 // Valeur par défaut
#endif
#define NKENTSEU_HUGE_PAGE_SIZE (2 * 1024 * 1024)

#else
// Valeur par défaut sécurisée
#define NKENTSEU_PAGE_SIZE 4096
#define NKENTSEU_HUGE_PAGE_SIZE (2 * 1024 * 1024)
#endif

// ============================================================
// MACROS CONSTANTES POUR LA PORTABILITÉ
// ============================================================

/**
 * @brief Taille de mot machine en octets
 * @def NKENTSEU_WORD_SIZE
 * @value 8 pour 64-bit
 * @value 4 pour 32-bit
 * @ingroup ArchitectureConstants
 */

/**
 * @brief Nombre de bits dans un mot machine
 * @def NKENTSEU_WORD_BITS
 * @value 64 pour 64-bit
 * @value 32 pour 32-bit
 * @ingroup ArchitectureConstants
 */

/**
 * @brief Nombre de bits dans un pointeur
 * @def NKENTSEU_PTR_BITS
 * @value 64 pour 64-bit
 * @value 32 pour 32-bit
 * @ingroup ArchitectureConstants
 */

// Taille de mot machine
#if defined(NKENTSEU_ARCH_64BIT)
#define NKENTSEU_WORD_SIZE 8
#define NKENTSEU_WORD_BITS 64
#define NKENTSEU_PTR_BITS 64
#else
#define NKENTSEU_WORD_SIZE 4
#define NKENTSEU_WORD_BITS 32
#define NKENTSEU_PTR_BITS 32
#endif

// ============================================================
// MACROS D'ATTRIBUTS POUR ALIGNEMENT
// ============================================================

/**
 * @brief Attribut d'alignement sur ligne de cache
 * @def NKENTSEU_ALIGN_CACHE
 * @ingroup AlignmentMacros
 */

/**
 * @brief Attribut d'alignement sur 16 octets
 * @def NKENTSEU_ALIGN_16
 * @ingroup AlignmentMacros
 */

/**
 * @brief Attribut d'alignement sur 32 octets
 * @def NKENTSEU_ALIGN_32
 * @ingroup AlignmentMacros
 */

/**
 * @brief Attribut d'alignement sur 64 octets
 * @def NKENTSEU_ALIGN_64
 * @ingroup AlignmentMacros
 */

/**
 * @brief Attribut d'alignement personnalisé
 * @def NKENTSEU_ALIGN(n)
 * @param n Valeur d'alignement
 * @ingroup AlignmentMacros
 */

#if defined(_MSC_VER)
// Pour MSVC
#define NKENTSEU_ALIGN_CACHE __declspec(align(NKENTSEU_CACHE_LINE_SIZE))
#define NKENTSEU_ALIGN_16 __declspec(align(16))
#define NKENTSEU_ALIGN_32 __declspec(align(32))
#define NKENTSEU_ALIGN_64 __declspec(align(64))
#define NKENTSEU_ALIGN(n) __declspec(align(n))

#elif defined(__GNUC__) || defined(__clang__)
// Pour GCC/Clang
#define NKENTSEU_ALIGN_CACHE __attribute__((aligned(NKENTSEU_CACHE_LINE_SIZE)))
#define NKENTSEU_ALIGN_16 __attribute__((aligned(16)))
#define NKENTSEU_ALIGN_32 __attribute__((aligned(32)))
#define NKENTSEU_ALIGN_64 __attribute__((aligned(64)))
#define NKENTSEU_ALIGN(n) __attribute__((aligned(n)))

#else
// Compilateur non supporté
#define NKENTSEU_ALIGN_CACHE
#define NKENTSEU_ALIGN_16
#define NKENTSEU_ALIGN_32
#define NKENTSEU_ALIGN_64
#define NKENTSEU_ALIGN(n)
#endif

// ============================================================
// MACROS CONDITIONNELLES - EXÉCUTION SÉLECTIVE DE CODE
// ============================================================

/**
 * @brief Macros pour exécuter du code uniquement sur une architecture spécifique
 * @defgroup ConditionalArchExecution Exécution Conditionnelle par Architecture
 *
 * Ces macros permettent d'écrire du code qui ne sera compilé et exécuté
 * que sur l'architecture cible spécifiée.
 *
 * @example
 * @code
 * NKENTSEU_X86_64_ONLY({
 *     // Ce code ne s'exécute que sur x86_64
 *     __asm__("cpuid");
 * });
 *
 * NKENTSEU_ARM64_ONLY({
 *     // Ce code ne s'exécute que sur ARM64
 *     InitializeNEON();
 * });
 *
 * NKENTSEU_64BIT_ONLY({
 *     // Ce code ne s'exécute que sur architectures 64-bit
 *     uint64_t largePointer = 0xFFFFFFFF00000000ULL;
 * });
 * @endcode
 */

// ============================================================
// MACROS CONDITIONNELLES - ARCHITECTURES 64-BIT
// ============================================================

#ifdef NKENTSEU_ARCH_X86_64
#define NKENTSEU_X86_64_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_X86_64(...)
#else
#define NKENTSEU_X86_64_ONLY(...)
#define NKENTSEU_NOT_X86_64(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_ARCH_ARM64
#define NKENTSEU_ARM64_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_ARM64(...)
#else
#define NKENTSEU_ARM64_ONLY(...)
#define NKENTSEU_NOT_ARM64(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_ARCH_PPC64
#define NKENTSEU_PPC64_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_PPC64(...)
#else
#define NKENTSEU_PPC64_ONLY(...)
#define NKENTSEU_NOT_PPC64(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_ARCH_MIPS64
#define NKENTSEU_MIPS64_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_MIPS64(...)
#else
#define NKENTSEU_MIPS64_ONLY(...)
#define NKENTSEU_NOT_MIPS64(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_ARCH_RISCV64
#define NKENTSEU_RISCV64_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_RISCV64(...)
#else
#define NKENTSEU_RISCV64_ONLY(...)
#define NKENTSEU_NOT_RISCV64(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_ARCH_SPARC64
#define NKENTSEU_SPARC64_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_SPARC64(...)
#else
#define NKENTSEU_SPARC64_ONLY(...)
#define NKENTSEU_NOT_SPARC64(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_ARCH_IA64
#define NKENTSEU_IA64_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_IA64(...)
#else
#define NKENTSEU_IA64_ONLY(...)
#define NKENTSEU_NOT_IA64(...) __VA_ARGS__
#endif

// ============================================================
// MACROS CONDITIONNELLES - ARCHITECTURES 32-BIT
// ============================================================

#ifdef NKENTSEU_ARCH_X86
#define NKENTSEU_X86_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_X86(...)
#else
#define NKENTSEU_X86_ONLY(...)
#define NKENTSEU_NOT_X86(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_ARCH_ARM
#define NKENTSEU_ARM_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_ARM(...)
#else
#define NKENTSEU_ARM_ONLY(...)
#define NKENTSEU_NOT_ARM(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_ARCH_PPC
#define NKENTSEU_PPC_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_PPC(...)
#else
#define NKENTSEU_PPC_ONLY(...)
#define NKENTSEU_NOT_PPC(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_ARCH_MIPS
#define NKENTSEU_MIPS_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_MIPS(...)
#else
#define NKENTSEU_MIPS_ONLY(...)
#define NKENTSEU_NOT_MIPS(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_ARCH_SPARC
#define NKENTSEU_SPARC_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_SPARC(...)
#else
#define NKENTSEU_SPARC_ONLY(...)
#define NKENTSEU_NOT_SPARC(...) __VA_ARGS__
#endif

// ============================================================
// MACROS CONDITIONNELLES - ARCHITECTURES SPÉCIALES
// ============================================================

#ifdef NKENTSEU_ARCH_R5900
#define NKENTSEU_R5900_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_R5900(...)
#else
#define NKENTSEU_R5900_ONLY(...)
#define NKENTSEU_NOT_R5900(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_ARCH_CELL_PPU
#define NKENTSEU_CELL_PPU_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_CELL_PPU(...)
#else
#define NKENTSEU_CELL_PPU_ONLY(...)
#define NKENTSEU_NOT_CELL_PPU(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_ARCH_ELBRUS
#define NKENTSEU_ELBRUS_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_ELBRUS(...)
#else
#define NKENTSEU_ELBRUS_ONLY(...)
#define NKENTSEU_NOT_ELBRUS(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_ARCH_SUPERH
#define NKENTSEU_SUPERH_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_SUPERH(...)
#else
#define NKENTSEU_SUPERH_ONLY(...)
#define NKENTSEU_NOT_SUPERH(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_ARCH_ALPHA
#define NKENTSEU_ALPHA_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_ALPHA(...)
#else
#define NKENTSEU_ALPHA_ONLY(...)
#define NKENTSEU_NOT_ALPHA(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_ARCH_M68K
#define NKENTSEU_M68K_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_M68K(...)
#else
#define NKENTSEU_M68K_ONLY(...)
#define NKENTSEU_NOT_M68K(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_ARCH_XTENSA
#define NKENTSEU_XTENSA_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_XTENSA(...)
#else
#define NKENTSEU_XTENSA_ONLY(...)
#define NKENTSEU_NOT_XTENSA(...) __VA_ARGS__
#endif

// ============================================================
// MACROS CONDITIONNELLES - FAMILLES D'ARCHITECTURES
// ============================================================

#ifdef NKENTSEU_ARCH_INTEL
#define NKENTSEU_INTEL_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_INTEL(...)
#else
#define NKENTSEU_INTEL_ONLY(...)
#define NKENTSEU_NOT_INTEL(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_ARCH_ARM_FAMILY
#define NKENTSEU_ARM_FAMILY_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_ARM_FAMILY(...)
#else
#define NKENTSEU_ARM_FAMILY_ONLY(...)
#define NKENTSEU_NOT_ARM_FAMILY(...) __VA_ARGS__
#endif

// ============================================================
// MACROS CONDITIONNELLES - BITNESS
// ============================================================

#ifdef NKENTSEU_ARCH_64BIT
#define NKENTSEU_64BIT_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_64BIT(...)
#else
#define NKENTSEU_64BIT_ONLY(...)
#define NKENTSEU_NOT_64BIT(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_ARCH_32BIT
#define NKENTSEU_32BIT_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_32BIT(...)
#else
#define NKENTSEU_32BIT_ONLY(...)
#define NKENTSEU_NOT_32BIT(...) __VA_ARGS__
#endif

// ============================================================
// MACROS CONDITIONNELLES - ENDIANNESS
// ============================================================

#ifdef NKENTSEU_ARCH_LITTLE_ENDIAN
#define NKENTSEU_LITTLE_ENDIAN_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_LITTLE_ENDIAN(...)
#else
#define NKENTSEU_LITTLE_ENDIAN_ONLY(...)
#define NKENTSEU_NOT_LITTLE_ENDIAN(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_ARCH_BIG_ENDIAN
#define NKENTSEU_BIG_ENDIAN_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_BIG_ENDIAN(...)
#else
#define NKENTSEU_BIG_ENDIAN_ONLY(...)
#define NKENTSEU_NOT_BIG_ENDIAN(...) __VA_ARGS__
#endif

// ============================================================
// MACROS CONDITIONNELLES - FONCTIONNALITÉS CPU (SIMD)
// ============================================================

#ifdef NKENTSEU_CPU_HAS_SSE
#define NKENTSEU_SSE_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_SSE(...)
#else
#define NKENTSEU_SSE_ONLY(...)
#define NKENTSEU_NOT_SSE(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_CPU_HAS_SSE2
#define NKENTSEU_SSE2_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_SSE2(...)
#else
#define NKENTSEU_SSE2_ONLY(...)
#define NKENTSEU_NOT_SSE2(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_CPU_HAS_SSE3
#define NKENTSEU_SSE3_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_SSE3(...)
#else
#define NKENTSEU_SSE3_ONLY(...)
#define NKENTSEU_NOT_SSE3(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_CPU_HAS_SSSE3
#define NKENTSEU_SSSE3_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_SSSE3(...)
#else
#define NKENTSEU_SSSE3_ONLY(...)
#define NKENTSEU_NOT_SSSE3(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_CPU_HAS_SSE4_1
#define NKENTSEU_SSE4_1_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_SSE4_1(...)
#else
#define NKENTSEU_SSE4_1_ONLY(...)
#define NKENTSEU_NOT_SSE4_1(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_CPU_HAS_SSE4_2
#define NKENTSEU_SSE4_2_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_SSE4_2(...)
#else
#define NKENTSEU_SSE4_2_ONLY(...)
#define NKENTSEU_NOT_SSE4_2(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_CPU_HAS_AVX
#define NKENTSEU_AVX_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_AVX(...)
#else
#define NKENTSEU_AVX_ONLY(...)
#define NKENTSEU_NOT_AVX(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_CPU_HAS_AVX2
#define NKENTSEU_AVX2_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_AVX2(...)
#else
#define NKENTSEU_AVX2_ONLY(...)
#define NKENTSEU_NOT_AVX2(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_CPU_HAS_AVX512
#define NKENTSEU_AVX512_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_AVX512(...)
#else
#define NKENTSEU_AVX512_ONLY(...)
#define NKENTSEU_NOT_AVX512(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_CPU_HAS_AES
#define NKENTSEU_AES_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_AES(...)
#else
#define NKENTSEU_AES_ONLY(...)
#define NKENTSEU_NOT_AES(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_CPU_HAS_BMI
#define NKENTSEU_BMI_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_BMI(...)
#else
#define NKENTSEU_BMI_ONLY(...)
#define NKENTSEU_NOT_BMI(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_CPU_HAS_FMA
#define NKENTSEU_FMA_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_FMA(...)
#else
#define NKENTSEU_FMA_ONLY(...)
#define NKENTSEU_NOT_FMA(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_CPU_HAS_NEON
#define NKENTSEU_NEON_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_NEON(...)
#else
#define NKENTSEU_NEON_ONLY(...)
#define NKENTSEU_NOT_NEON(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_CPU_HAS_ARM_CRYPTO
#define NKENTSEU_ARM_CRYPTO_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_ARM_CRYPTO(...)
#else
#define NKENTSEU_ARM_CRYPTO_ONLY(...)
#define NKENTSEU_NOT_ARM_CRYPTO(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_CPU_HAS_ARM_CRC32
#define NKENTSEU_ARM_CRC32_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_ARM_CRC32(...)
#else
#define NKENTSEU_ARM_CRC32_ONLY(...)
#define NKENTSEU_NOT_ARM_CRC32(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_CPU_HAS_ALTIVEC
#define NKENTSEU_ALTIVEC_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_ALTIVEC(...)
#else
#define NKENTSEU_ALTIVEC_ONLY(...)
#define NKENTSEU_NOT_ALTIVEC(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_CPU_HAS_VSX
#define NKENTSEU_VSX_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_VSX(...)
#else
#define NKENTSEU_VSX_ONLY(...)
#define NKENTSEU_NOT_VSX(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_CPU_HAS_MSA
#define NKENTSEU_MSA_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_MSA(...)
#else
#define NKENTSEU_MSA_ONLY(...)
#define NKENTSEU_NOT_MSA(...) __VA_ARGS__
#endif

// ============================================================
// MACROS DE DÉBOGAGE
// ============================================================

#ifdef NKENTSEU_ARCH_DEBUG
#pragma message("Nkentseu: Architecture détectée: " NKENTSEU_ARCH_NAME)
#pragma message("Nkentseu: Version: " NKENTSEU_ARCH_VERSION)
#ifdef NKENTSEU_ARCH_64BIT
#pragma message("Nkentseu: Architecture 64-bit")
#else
#pragma message("Nkentseu: Architecture 32-bit")
#endif
#ifdef NKENTSEU_ARCH_LITTLE_ENDIAN
#pragma message("Nkentseu: Architecture little-endian")
#elif defined(NKENTSEU_ARCH_BIG_ENDIAN)
#pragma message("Nkentseu: Architecture big-endian")
#endif
#endif

// ============================================================
// MACROS DE VALIDATION COMPILE-TIME
// ============================================================

#if defined(NKENTSEU_ARCH_64BIT) && defined(NKENTSEU_ARCH_32BIT)
#error "Nkentseu: Architecture ne peut pas être à la fois 64-bit et 32-bit"
#endif

#if !defined(NKENTSEU_ARCH_64BIT) && !defined(NKENTSEU_ARCH_32BIT)
#warning "Nkentseu: Bitness de l'architecture non détectée, supposition 64-bit"
#define NKENTSEU_ARCH_64BIT
#endif

// Vérification de cohérence
#if defined(NKENTSEU_ARCH_X86_64) && !defined(NKENTSEU_ARCH_64BIT)
#error "Nkentseu: x86_64 doit être 64-bit"
#endif

#if defined(NKENTSEU_ARCH_X86) && defined(NKENTSEU_ARCH_64BIT)
#error "Nkentseu: x86 doit être 32-bit"
#endif

#endif // NKENTSEU_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKARCHDETECT_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-07
// Creation Date: 2026-02-07
// ============================================================