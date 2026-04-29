// =============================================================================
// Core/Nkentseu/Platform/NkArchDetect.h
// Détection d'architecture CPU et définition des macros associées.
//
// Design :
//  - Détection automatique de l'architecture CPU (x86, ARM, RISC-V, etc.)
//  - Définition des macros de bitness (32/64-bit), d'endianness et d'alignement
//  - Détection des extensions CPU (SIMD : SSE, AVX, NEON, Altivec, etc.)
//  - Fournit des macros conditionnelles pour l'exécution sélective de code
//  - Aucune dépendance externe — se base uniquement sur les macros du préprocesseur
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_PLATFORM_NKARCHDETECT_H
#define NKENTSEU_PLATFORM_NKARCHDETECT_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES STANDARD NÉCESSAIRES
    // -------------------------------------------------------------------------
    // Inclusion des en-têtes pour les définitions de types de base.
    // Ces en-têtes sont requis pour garantir la portabilité des types numériques.

    #include <cstddef>
    #include <cstdint>

    // -------------------------------------------------------------------------
    // SECTION 2 : DOCUMENTATION DES MACROS PRINCIPALES
    // -------------------------------------------------------------------------
    // Documentation Doxygen pour les macros publiques de détection d'architecture.
    // Ces macros sont destinées à être utilisées par le code utilisateur.

    /**
     * @brief Macro de détection d'architecture x86_64
     * @def NKENTSEU_ARCH_X86_64
     * @ingroup ArchitectureDetection
     * @see NKENTSEU_ARCH_64BIT
     *
     * Définie lorsque le code est compilé pour une architecture x86_64
     * (AMD64 ou Intel 64). Détectée via les macros __x86_64__, _M_X64, etc.
     */

    /**
     * @brief Macro de détection d'architecture ARM64
     * @def NKENTSEU_ARCH_ARM64
     * @ingroup ArchitectureDetection
     * @see NKENTSEU_ARCH_64BIT
     *
     * Définie lorsque le code est compilé pour ARM 64-bit (AArch64/ARMv8).
     * Détectée via __aarch64__, _M_ARM64, __ARM64__, etc.
     */

    /**
     * @brief Macro de détection d'architecture 64-bit
     * @def NKENTSEU_ARCH_64BIT
     * @ingroup ArchitectureDetection
     *
     * Définie pour toute architecture 64-bit détectée.
     * Permet d'écrire du code conditionnel basé sur la largeur des pointeurs.
     */

    /**
     * @brief Macro de nom d'architecture lisible
     * @def NKENTSEU_ARCH_NAME
     * @ingroup ArchitectureDetection
     * @return Chaîne constante représentant l'architecture détectée
     *
     * Utile pour les logs, le debug et l'affichage d'informations système.
     * Exemple : "x86_64", "ARM64", "RISC-V 64", etc.
     */

    /**
     * @brief Macro de version d'architecture détaillée
     * @def NKENTSEU_ARCH_VERSION
     * @ingroup ArchitectureDetection
     * @return Chaîne constante avec des détails sur la version de l'architecture
     *
     * Fournit des informations supplémentaires comme le jeu d'instructions
     * ou la génération de l'architecture. Exemple : "AMD64/Intel 64-bit".
     */

    // =========================================================================
    // SECTION 3 : DÉTECTION DES ARCHITECTURES 64-BIT
    // =========================================================================
    // Détection séquentielle des architectures 64-bit connues.
    // La première correspondance gagne (ordre d'importance décroissant).

    // -------------------------------------------------------------------------
    // Sous-section : x86_64 (AMD64 / Intel 64)
    // -------------------------------------------------------------------------
    // Architecture dominante sur PC/desktop/serveurs.
    // Macros de détection : __x86_64__, _M_X64, __amd64__, __amd64

    #if defined(__x86_64__) || defined(_M_X64) || defined(__amd64__) || defined(__amd64)
        #define NKENTSEU_ARCH_X86_64
        #define NKENTSEU_ARCH_64BIT
        #define NKENTSEU_ARCH_NAME "x86_64"
        #define NKENTSEU_ARCH_VERSION "AMD64/Intel 64-bit"

    // -------------------------------------------------------------------------
    // Sous-section : ARM64 (AArch64 / ARMv8)
    // -------------------------------------------------------------------------
    // Architecture mobile moderne et Apple Silicon.
    // Macros de détection : __aarch64__, _M_ARM64, __ARM64__, __arm64__

    #elif defined(__aarch64__) || defined(_M_ARM64) || defined(__ARM64__) || defined(__arm64__)
        #define NKENTSEU_ARCH_ARM64
        #define NKENTSEU_ARCH_64BIT
        #define NKENTSEU_ARCH_NAME "ARM64"
        #define NKENTSEU_ARCH_VERSION "ARMv8 64-bit"

    // -------------------------------------------------------------------------
    // Sous-section : PowerPC 64-bit
    // -------------------------------------------------------------------------
    // Architecture historique IBM, encore utilisée dans certains serveurs/embarqué.
    // Macros de détection : __ppc64__, __PPC64__, _ARCH_PPC64

    #elif defined(__ppc64__) || defined(__PPC64__) || defined(_ARCH_PPC64)
        #define NKENTSEU_ARCH_PPC64
        #define NKENTSEU_ARCH_64BIT
        #define NKENTSEU_ARCH_NAME "PowerPC64"
        #define NKENTSEU_ARCH_VERSION "PowerPC 64-bit"

    // -------------------------------------------------------------------------
    // Sous-section : MIPS 64-bit
    // -------------------------------------------------------------------------
    // Architecture historique, encore présente dans l'embarqué et les routeurs.
    // Macros de détection : __mips64, __mips64__

    #elif defined(__mips64) || defined(__mips64__)
        #define NKENTSEU_ARCH_MIPS64
        #define NKENTSEU_ARCH_64BIT
        #define NKENTSEU_ARCH_NAME "MIPS64"
        #define NKENTSEU_ARCH_VERSION "MIPS 64-bit"

    // -------------------------------------------------------------------------
    // Sous-section : RISC-V 64-bit
    // -------------------------------------------------------------------------
    // Architecture open-source émergente, populaire dans l'embarqué moderne.
    // Macros de détection : __riscv avec __riscv_xlen == 64

    #elif defined(__riscv) && (__riscv_xlen == 64)
        #define NKENTSEU_ARCH_RISCV64
        #define NKENTSEU_ARCH_64BIT
        #define NKENTSEU_ARCH_NAME "RISC-V 64"
        #define NKENTSEU_ARCH_VERSION "RISC-V 64-bit"

    // -------------------------------------------------------------------------
    // Sous-section : SPARC 64-bit
    // -------------------------------------------------------------------------
    // Architecture historique Sun/Oracle, utilisée dans les serveurs enterprise.
    // Macros de détection : __sparc_v9__, __sparcv9

    #elif defined(__sparc_v9__) || defined(__sparcv9)
        #define NKENTSEU_ARCH_SPARC64
        #define NKENTSEU_ARCH_64BIT
        #define NKENTSEU_ARCH_NAME "SPARC64"
        #define NKENTSEU_ARCH_VERSION "SPARC 64-bit"

    // =========================================================================
    // SECTION 4 : DÉTECTION DES ARCHITECTURES 32-BIT
    // =========================================================================
    // Détection séquentielle des architectures 32-bit connues.

    // -------------------------------------------------------------------------
    // Sous-section : x86 (IA-32)
    // -------------------------------------------------------------------------
    // Architecture historique PC, encore utilisée dans l'embarqué et legacy.
    // Macros de détection : __i386__, _M_IX86, __i386

    #elif defined(__i386__) || defined(_M_IX86) || defined(__i386)
        #define NKENTSEU_ARCH_X86
        #define NKENTSEU_ARCH_32BIT
        #define NKENTSEU_ARCH_NAME "x86"
        #define NKENTSEU_ARCH_VERSION "Intel x86 32-bit"

    // -------------------------------------------------------------------------
    // Sous-section : ARM 32-bit avec détection de version
    // -------------------------------------------------------------------------
    // Architecture mobile historique et embarqué.
    // Détection fine de la version ARMv5/6/7 via les macros __ARM_ARCH_*

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

    // -------------------------------------------------------------------------
    // Sous-section : PowerPC 32-bit
    // -------------------------------------------------------------------------
    #elif defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC)
        #define NKENTSEU_ARCH_PPC
        #define NKENTSEU_ARCH_32BIT
        #define NKENTSEU_ARCH_NAME "PowerPC"
        #define NKENTSEU_ARCH_VERSION "PowerPC 32-bit"

    // -------------------------------------------------------------------------
    // Sous-section : MIPS 32-bit
    // -------------------------------------------------------------------------
    #elif defined(__mips__) || defined(__mips) || defined(_MIPS_ISA)
        #define NKENTSEU_ARCH_MIPS
        #define NKENTSEU_ARCH_32BIT
        #define NKENTSEU_ARCH_NAME "MIPS"
        #define NKENTSEU_ARCH_VERSION "MIPS 32-bit"

    // -------------------------------------------------------------------------
    // Sous-section : SPARC 32-bit
    // -------------------------------------------------------------------------
    #elif defined(__sparc__) || defined(__sparc)
        #define NKENTSEU_ARCH_SPARC
        #define NKENTSEU_ARCH_32BIT
        #define NKENTSEU_ARCH_NAME "SPARC"
        #define NKENTSEU_ARCH_VERSION "SPARC 32-bit"

    // -------------------------------------------------------------------------
    // Sous-section : Architectures spécialisées / exotiques
    // -------------------------------------------------------------------------

    // Elbrus (VLIW, Russie)
    #elif defined(__e2k__) || defined(__E2K__)
        #define NKENTSEU_ARCH_ELBRUS
        #define NKENTSEU_ARCH_ELBRUS_VLIW
        #define NKENTSEU_ARCH_NAME "Elbrus"
        #define NKENTSEU_ARCH_VERSION "Elbrus VLIW"

    // SuperH (Hitachi, embarqué historique)
    #elif defined(__sh__) || defined(__sh)
        #define NKENTSEU_ARCH_SUPERH
        #define NKENTSEU_ARCH_NAME "SuperH"
        #define NKENTSEU_ARCH_VERSION "Hitachi SuperH"

    // Alpha (DEC, historique)
    #elif defined(__alpha__) || defined(__alpha)
        #define NKENTSEU_ARCH_ALPHA
        #define NKENTSEU_ARCH_NAME "Alpha"
        #define NKENTSEU_ARCH_VERSION "DEC Alpha"

    // PA-RISC (HP, historique)
    #elif defined(__hppa__) || defined(__hppa)
        #define NKENTSEU_ARCH_PARISC
        #define NKENTSEU_ARCH_NAME "PA-RISC"
        #define NKENTSEU_ARCH_VERSION "HP PA-RISC"

    // IA-64 / Itanium (Intel, historique)
    #elif defined(__ia64__) || defined(_M_IA64)
        #define NKENTSEU_ARCH_IA64
        #define NKENTSEU_ARCH_NAME "IA-64 (Itanium)"
        #define NKENTSEU_ARCH_VERSION "Intel Itanium 64-bit"

    // Motorola 68000 (rétro-computing, embarqué ancien)
    #elif defined(__m68k__) || defined(__m68k)
        #define NKENTSEU_ARCH_M68K
        #define NKENTSEU_ARCH_NAME "Motorola 68000"
        #define NKENTSEU_ARCH_VERSION "Motorola 68K"

    // IBM System/390 (mainframe historique)
    #elif defined(__s390__) || defined(__s390)
        #define NKENTSEU_ARCH_S390
        #define NKENTSEU_ARCH_NAME "IBM System/390"
        #define NKENTSEU_ARCH_VERSION "IBM S/390"

    // Tilera TILE (architectures many-core)
    #elif defined(__tile__) || defined(__tile)
        #define NKENTSEU_ARCH_TILE
        #define NKENTSEU_ARCH_NAME "TILE"
        #define NKENTSEU_ARCH_VERSION "Tilera TILE"

    // Xtensa (Tensilica, embarqué configurable)
    #elif defined(__xtensa__) || defined(__xtensa)
        #define NKENTSEU_ARCH_XTENSA
        #define NKENTSEU_ARCH_NAME "Xtensa"
        #define NKENTSEU_ARCH_VERSION "Tensilica Xtensa"

    // =========================================================================
    // SECTION 5 : ARCHITECTURES SPÉCIFIQUES AUX CONSOLES
    // =========================================================================

    // -------------------------------------------------------------------------
    // Sous-section : PlayStation 2 (R5900 / Emotion Engine)
    // -------------------------------------------------------------------------
    // Architecture MIPS personnalisée avec extensions vectorielles VU0/VU1
    #elif defined(_EE) || defined(__R5900__)
        #define NKENTSEU_ARCH_R5900
        #define NKENTSEU_ARCH_MIPS
        #define NKENTSEU_ARCH_64BIT
        #define NKENTSEU_ARCH_NAME "R5900"
        #define NKENTSEU_ARCH_VERSION "MIPS R5900 (PS2 Emotion Engine)"

    // -------------------------------------------------------------------------
    // Sous-section : PlayStation 3 (Cell Broadband Engine - PPU)
    // -------------------------------------------------------------------------
    // Architecture PowerPC personnalisée avec coprocesseurs SPU
    #elif defined(__PPU__)
        #define NKENTSEU_ARCH_CELL_PPU
        #define NKENTSEU_ARCH_PPC
        #define NKENTSEU_ARCH_64BIT
        #define NKENTSEU_ARCH_NAME "Cell PPU"
        #define NKENTSEU_ARCH_VERSION "Cell Broadband Engine PPU (PS3)"

    // =========================================================================
    // SECTION 6 : FALLBACK POUR ARCHITECTURE INCONNUE
    // =========================================================================
    // Si aucune architecture connue n'est détectée, tentative de déduction
    // via la taille du pointeur (__SIZEOF_POINTER__ ou UINTPTR_MAX).

    #else
        // Méthode 1 : utilisation de __SIZEOF_POINTER__ (GCC/Clang)
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

        // Méthode 2 : utilisation de UINTPTR_MAX (C99+)
        #else
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

    // =========================================================================
    // SECTION 7 : MACROS DE CONVENANCE ET COMPATIBILITÉ
    // =========================================================================
    // Macros dérivées pour simplifier les conditions de compilation
    // et assurer la rétrocompatibilité avec d'anciens codes.

    /**
     * @brief Macro de détection d'architecture Intel (x86/x86_64)
     * @def NKENTSEU_ARCH_INTEL
     * @ingroup ArchitectureDetection
     *
     * Définie si l'architecture est de la famille Intel/AMD x86.
     * Utile pour activer des optimisations spécifiques à cette famille.
     */

    /**
     * @brief Macro de détection d'architecture ARM (ARM/ARM64)
     * @def NKENTSEU_ARCH_ARM_FAMILY
     * @ingroup ArchitectureDetection
     *
     * Définie si l'architecture est de la famille ARM (32 ou 64-bit).
     * Permet de factoriser le code commun aux variantes ARM.
     */

    /**
     * @brief Macro de détection d'endianness little-endian
     * @def NKENTSEU_ARCH_LITTLE_ENDIAN
     * @ingroup ArchitectureDetection
     *
     * Définie si l'architecture utilise l'ordre little-endian
     * (octet de poids faible en premier). Majoritaire sur x86, ARM moderne.
     */

    /**
     * @brief Macro de détection d'endianness big-endian
     * @def NKENTSEU_ARCH_BIG_ENDIAN
     * @ingroup ArchitectureDetection
     *
     * Définie si l'architecture utilise l'ordre big-endian
     * (octet de poids fort en premier). Présent sur certains PowerPC, SPARC, réseau.
     */

    // -------------------------------------------------------------------------
    // Sous-section : Définition des familles d'architectures
    // -------------------------------------------------------------------------

    #if defined(NKENTSEU_ARCH_X86_64) || defined(NKENTSEU_ARCH_X86)
        #define NKENTSEU_ARCH_INTEL
    #endif

    #if defined(NKENTSEU_ARCH_ARM64) || defined(NKENTSEU_ARCH_ARM)
        #define NKENTSEU_ARCH_ARM_FAMILY
    #endif

    // -------------------------------------------------------------------------
    // Sous-section : Détection automatique de l'endianness
    // -------------------------------------------------------------------------
    // Tentative de détection via __BYTE_ORDER__ (GCC/Clang) ou heuristiques.

    #if !defined(NKENTSEU_ARCH_LITTLE_ENDIAN) && !defined(NKENTSEU_ARCH_BIG_ENDIAN)
        #if defined(__BYTE_ORDER__)
            #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
                #define NKENTSEU_ARCH_LITTLE_ENDIAN
            #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
                #define NKENTSEU_ARCH_BIG_ENDIAN
            #endif
        #elif defined(__LITTLE_ENDIAN__) || defined(_WIN32) || defined(__i386__) || defined(__x86_64__)
            // Windows, x86 et x86_64 sont garantis little-endian
            #define NKENTSEU_ARCH_LITTLE_ENDIAN
        #elif defined(__BIG_ENDIAN__) || defined(__ARMEB__) || defined(__THUMBEB__)
            // ARM big-endian ou plateformes explicitement big-endian
            #define NKENTSEU_ARCH_BIG_ENDIAN
        #else
            // Fallback : supposer little-endian (cas le plus courant)
            #define NKENTSEU_ARCH_LITTLE_ENDIAN
        #endif
    #endif

    // =========================================================================
    // SECTION 8 : DÉTECTION DES EXTENSIONS CPU / FONCTIONNALITÉS SIMD
    // =========================================================================
    // Détection des jeux d'instructions vectoriels et cryptographiques
    // disponibles à la compilation (pas à l'exécution).

    /**
     * @brief Macro de détection de support SSE
     * @def NKENTSEU_CPU_HAS_SSE
     * @ingroup CpuFeatures
     *
     * Définie si le compilateur cible supporte les instructions SSE.
     * Permet d'utiliser <xmmintrin.h> et les types __m128.
     */

    /**
     * @brief Macro de détection de support SSE2
     * @def NKENTSEU_CPU_HAS_SSE2
     * @ingroup CpuFeatures
     *
     * Définie si le compilateur cible supporte SSE2.
     * Requis pour les opérations SIMD sur doubles et entiers 64-bit.
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
     *
     * Définie si le compilateur cible supporte AVX (registres YMM 256-bit).
     * Nécessite un alignement mémoire sur 32 octets pour les opérations SIMD.
     */

    /**
     * @brief Macro de détection de support AVX2
     * @def NKENTSEU_CPU_HAS_AVX2
     * @ingroup CpuFeatures
     *
     * Définie si le compilateur cible supporte AVX2 (opérations entières SIMD).
     */

    /**
     * @brief Macro de détection de support AVX-512
     * @def NKENTSEU_CPU_HAS_AVX512
     * @ingroup CpuFeatures
     *
     * Définie si le compilateur cible supporte AVX-512 (registres ZMM 512-bit).
     * Disponible sur les CPU Intel récents et certains AMD.
     */

    /**
     * @brief Macro de détection de support AES-NI
     * @def NKENTSEU_CPU_HAS_AES
     * @ingroup CpuFeatures
     *
     * Définie si le CPU supporte les instructions AES matérielles.
     * Accélère significativement le chiffrement/déchiffrement AES.
     */

    /**
     * @brief Macro de détection de support BMI/BMI2
     * @def NKENTSEU_CPU_HAS_BMI
     * @ingroup CpuFeatures
     *
     * Définie si le CPU supporte Bit Manipulation Instructions.
     * Utile pour les opérations bit-à-bit optimisées.
     */

    /**
     * @brief Macro de détection de support FMA
     * @def NKENTSEU_CPU_HAS_FMA
     * @ingroup CpuFeatures
     *
     * Définie si le CPU supporte Fused Multiply-Add.
     * Améliore la précision et la performance des calculs flottants.
     */

    /**
     * @brief Macro de détection de support NEON (ARM)
     * @def NKENTSEU_CPU_HAS_NEON
     * @ingroup CpuFeatures
     *
     * Définie si le compilateur cible supporte NEON (SIMD ARM).
     * Équivalent ARM des extensions SSE/AVX sur x86.
     */

    /**
     * @brief Macro de détection de support crypto ARM
     * @def NKENTSEU_CPU_HAS_ARM_CRYPTO
     * @ingroup CpuFeatures
     *
     * Définie si le CPU ARM supporte les extensions cryptographiques matérielles.
     * Inclut AES, SHA-1, SHA-2 accélérés.
     */

    /**
     * @brief Macro de détection de support CRC32 ARM
     * @def NKENTSEU_CPU_HAS_ARM_CRC32
     * @ingroup CpuFeatures
     *
     * Définie si le CPU ARM supporte l'instruction CRC32 matérielle.
     */

    /**
     * @brief Macro de détection de support Altivec/VMX (PowerPC)
     * @def NKENTSEU_CPU_HAS_ALTIVEC
     * @ingroup CpuFeatures
     *
     * Définie si le compilateur cible supporte Altivec (SIMD PowerPC).
     */

    /**
     * @brief Macro de détection de support VSX (PowerPC)
     * @def NKENTSEU_CPU_HAS_VSX
     * @ingroup CpuFeatures
     *
     * Définie si le compilateur cible supporte VSX (Vector Scalar Extensions).
     * Extension moderne d'Altivec avec plus de registres.
     */

    /**
     * @brief Macro de détection de support MSA (MIPS)
     * @def NKENTSEU_CPU_HAS_MSA
     * @ingroup CpuFeatures
     *
     * Définie si le compilateur cible supporte MSA (MIPS SIMD Architecture).
     */

    // -------------------------------------------------------------------------
    // Sous-section : Détection des extensions x86/x86_64
    // -------------------------------------------------------------------------

    #if defined(NKENTSEU_ARCH_X86) || defined(NKENTSEU_ARCH_X86_64)
        // SSE - Streaming SIMD Extensions (base)
        #if defined(__SSE__) || (_M_IX86_FP >= 1) || defined(_M_X64)
            #define NKENTSEU_CPU_HAS_SSE
        #endif

        // SSE2 - obligatoire sur x86_64
        #if defined(__SSE2__) || (_M_IX86_FP >= 2) || defined(_M_X64)
            #define NKENTSEU_CPU_HAS_SSE2
        #endif

        // SSE3 et extensions supérieures
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

        // AVX et dérivés
        #if defined(__AVX__)
            #define NKENTSEU_CPU_HAS_AVX
        #endif

        #if defined(__AVX2__)
            #define NKENTSEU_CPU_HAS_AVX2
        #endif

        #if defined(__AVX512F__)
            #define NKENTSEU_CPU_HAS_AVX512
        #endif

        // AES-NI pour chiffrement accéléré
        #if defined(__AES__)
            #define NKENTSEU_CPU_HAS_AES
        #endif

        // BMI pour manipulation de bits
        #if defined(__BMI__) || defined(__BMI2__)
            #define NKENTSEU_CPU_HAS_BMI
        #endif

        // FMA pour opérations flottantes fusionnées
        #if defined(__FMA__)
            #define NKENTSEU_CPU_HAS_FMA
        #endif

    // -------------------------------------------------------------------------
    // Sous-section : Détection des extensions ARM/ARM64
    // -------------------------------------------------------------------------

    #elif defined(NKENTSEU_ARCH_ARM) || defined(NKENTSEU_ARCH_ARM64)
        // NEON - SIMD ARM
        #if defined(__ARM_NEON) || defined(__ARM_NEON__)
            #define NKENTSEU_CPU_HAS_NEON
        #endif

        // Crypto extensions ARMv8
        #if defined(__ARM_FEATURE_CRYPTO)
            #define NKENTSEU_CPU_HAS_ARM_CRYPTO
        #endif

        // CRC32 hardware
        #if defined(__ARM_FEATURE_CRC32)
            #define NKENTSEU_CPU_HAS_ARM_CRC32
        #endif

    // -------------------------------------------------------------------------
    // Sous-section : Détection des extensions PowerPC
    // -------------------------------------------------------------------------

    #elif defined(NKENTSEU_ARCH_PPC) || defined(NKENTSEU_ARCH_PPC64)
        // Altivec/VMX
        #if defined(__ALTIVEC__) || defined(__VEC__)
            #define NKENTSEU_CPU_HAS_ALTIVEC
        #endif

        // VSX (PowerPC moderne)
        #if defined(__VSX__)
            #define NKENTSEU_CPU_HAS_VSX
        #endif

    // -------------------------------------------------------------------------
    // Sous-section : Détection des extensions MIPS
    // -------------------------------------------------------------------------

    #elif defined(NKENTSEU_ARCH_MIPS) || defined(NKENTSEU_ARCH_MIPS64)
        // MSA - MIPS SIMD Architecture
        #if defined(__mips_msa)
            #define NKENTSEU_CPU_HAS_MSA
        #endif
    #endif

    // =========================================================================
    // SECTION 9 : CONSTANTES MÉMOIRE ET ALIGNEMENT
    // =========================================================================
    // Définition de constantes portables pour l'alignement mémoire,
    // la taille des pages et l'organisation du cache.

    /**
     * @brief Taille de ligne de cache en octets
     * @def NKENTSEU_CACHE_LINE_SIZE
     * @value 64 pour les architectures 64-bit modernes
     * @value 32 pour les architectures 32-bit
     * @ingroup MemoryConstants
     *
     * Utilisé pour l'alignement des structures fréquemment accédées
     * afin d'éviter le false sharing dans le code multithreadé.
     */

    /**
     * @brief Alignement maximal supporté par l'architecture
     * @def NKENTSEU_MAX_ALIGNMENT
     * @ingroup MemoryConstants
     *
     * Valeur maximale recommandée pour les alignements mémoire.
     * Utile pour l'allocation dynamique alignée (aligned_alloc, _mm_malloc).
     */

    // -------------------------------------------------------------------------
    // Sous-section : Définition des constantes d'alignement
    // -------------------------------------------------------------------------

    #if defined(NKENTSEU_ARCH_X86_64) || defined(NKENTSEU_ARCH_ARM64)
        // Architectures 64-bit modernes : cache line typique = 64 octets
        #define NKENTSEU_CACHE_LINE_SIZE 64
        #define NKENTSEU_MAX_ALIGNMENT 64

    #elif defined(NKENTSEU_ARCH_X86) || defined(NKENTSEU_ARCH_ARM)
        // Architectures 32-bit : cache line typique = 32 octets
        #define NKENTSEU_CACHE_LINE_SIZE 32
        #define NKENTSEU_MAX_ALIGNMENT 32

    #else
        // Valeur par défaut sécurisée pour architectures inconnues
        #define NKENTSEU_CACHE_LINE_SIZE 64
        #define NKENTSEU_MAX_ALIGNMENT 64
    #endif

    // =========================================================================
    // SECTION 10 : CONSTANTES DE TAILLE DE PAGE MÉMOIRE
    // =========================================================================

    /**
     * @brief Taille de page mémoire standard en octets
     * @def NKENTSEU_PAGE_SIZE
     * @value 4096 pour x86/x64 (4KB)
     * @value 16384 pour ARM avec pages 16KB
     * @value 65536 pour ARM avec pages 64KB
     * @ingroup MemoryConstants
     *
     * Utilisé pour les opérations de mémoire virtuelle (mmap, VirtualAlloc).
     * Les allocations inférieures à cette taille peuvent être sous-optimales.
     */

    /**
     * @brief Taille de page énorme (huge page) en octets
     * @def NKENTSEU_HUGE_PAGE_SIZE
     * @value 2MB pour la plupart des architectures
     * @ingroup MemoryConstants
     *
     * Utilisé pour les allocations de grande taille où la réduction des TLB misses
     * justifie l'utilisation de huge pages (performance critique).
     */

    // -------------------------------------------------------------------------
    // Sous-section : Définition des constantes de page
    // -------------------------------------------------------------------------

    #if defined(NKENTSEU_ARCH_X86_64) || defined(NKENTSEU_ARCH_X86)
        // x86/x64 : page standard = 4KB, huge page = 2MB (ou 1GB)
        #define NKENTSEU_PAGE_SIZE 4096
        #define NKENTSEU_HUGE_PAGE_SIZE (2 * 1024 * 1024)

    #elif defined(NKENTSEU_ARCH_ARM64) || defined(NKENTSEU_ARCH_ARM)
        // ARM : page configurable (4KB/16KB/64KB selon le kernel)
        #if defined(__ARM_PAGE_SIZE_4KB) || !defined(__ARM_PAGE_SIZE)
            #define NKENTSEU_PAGE_SIZE 4096
        #elif defined(__ARM_PAGE_SIZE_16KB)
            #define NKENTSEU_PAGE_SIZE 16384
        #elif defined(__ARM_PAGE_SIZE_64KB)
            #define NKENTSEU_PAGE_SIZE 65536
        #else
            #define NKENTSEU_PAGE_SIZE 4096
        #endif
        #define NKENTSEU_HUGE_PAGE_SIZE (2 * 1024 * 1024)

    #else
        // Valeur par défaut sécurisée
        #define NKENTSEU_PAGE_SIZE 4096
        #define NKENTSEU_HUGE_PAGE_SIZE (2 * 1024 * 1024)
    #endif

    // =========================================================================
    // SECTION 11 : CONSTANTES DE PORTABILITÉ ARCHITECTURE
    // =========================================================================

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

    // -------------------------------------------------------------------------
    // Sous-section : Définition des constantes de mot/pointeur
    // -------------------------------------------------------------------------

    #if defined(NKENTSEU_ARCH_64BIT)
        #define NKENTSEU_WORD_SIZE 8
        #define NKENTSEU_WORD_BITS 64
        #define NKENTSEU_PTR_BITS 64
    #else
        #define NKENTSEU_WORD_SIZE 4
        #define NKENTSEU_WORD_BITS 32
        #define NKENTSEU_PTR_BITS 32
    #endif

    // =========================================================================
    // SECTION 12 : MACROS D'ATTRIBUTS D'ALIGNEMENT
    // =========================================================================
    // Macros portables pour spécifier l'alignement mémoire des types
    // et variables. Utilise la syntaxe native de chaque compilateur.

    /**
     * @brief Attribut d'alignement sur ligne de cache
     * @def NKENTSEU_ALIGN_CACHE
     * @ingroup AlignmentMacros
     *
     * Aligne la variable/structure sur NKENTSEU_CACHE_LINE_SIZE octets.
     * Recommandé pour les données partagées entre threads pour éviter le false sharing.
     */

    /**
     * @brief Attribut d'alignement sur 16 octets
     * @def NKENTSEU_ALIGN_16
     * @ingroup AlignmentMacros
     *
     * Requis pour les opérations SSE/NEON de base.
     */

    /**
     * @brief Attribut d'alignement sur 32 octets
     * @def NKENTSEU_ALIGN_32
     * @ingroup AlignmentMacros
     *
     * Requis pour les opérations AVX/AVX2.
     */

    /**
     * @brief Attribut d'alignement sur 64 octets
     * @def NKENTSEU_ALIGN_64
     * @ingroup AlignmentMacros
     *
     * Requis pour les opérations AVX-512 et optimisation cache.
     */

    /**
     * @brief Attribut d'alignement personnalisé
     * @def NKENTSEU_ALIGN(n)
     * @param n Valeur d'alignement (doit être une puissance de 2)
     * @ingroup AlignmentMacros
     */

    // -------------------------------------------------------------------------
    // Sous-section : Implémentation multi-compilateurs des attributs d'alignement
    // -------------------------------------------------------------------------

    #if defined(_MSC_VER)
        // MSVC utilise __declspec(align())
        #define NKENTSEU_ALIGN_CACHE __declspec(align(NKENTSEU_CACHE_LINE_SIZE))
        #define NKENTSEU_ALIGN_16 __declspec(align(16))
        #define NKENTSEU_ALIGN_32 __declspec(align(32))
        #define NKENTSEU_ALIGN_64 __declspec(align(64))
        #define NKENTSEU_ALIGN(n) __declspec(align(n))

    #elif defined(__GNUC__) || defined(__clang__)
        // GCC/Clang utilise __attribute__((aligned()))
        #define NKENTSEU_ALIGN_CACHE __attribute__((aligned(NKENTSEU_CACHE_LINE_SIZE)))
        #define NKENTSEU_ALIGN_16 __attribute__((aligned(16)))
        #define NKENTSEU_ALIGN_32 __attribute__((aligned(32)))
        #define NKENTSEU_ALIGN_64 __attribute__((aligned(64)))
        #define NKENTSEU_ALIGN(n) __attribute__((aligned(n)))

    #else
        // Compilateur non supporté : macros no-op (risque de mauvaises performances)
        #define NKENTSEU_ALIGN_CACHE
        #define NKENTSEU_ALIGN_16
        #define NKENTSEU_ALIGN_32
        #define NKENTSEU_ALIGN_64
        #define NKENTSEU_ALIGN(n)
    #endif

    // =========================================================================
    // SECTION 13 : MACROS CONDITIONNELLES D'EXÉCUTION PAR ARCHITECTURE
    // =========================================================================
    // Macros permettant d'écrire du code qui ne sera compilé que pour
    // l'architecture cible spécifiée. Syntaxe élégante avec __VA_ARGS__.

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

    // -------------------------------------------------------------------------
    // Sous-section : Macros pour architectures 64-bit spécifiques
    // -------------------------------------------------------------------------

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

    // -------------------------------------------------------------------------
    // Sous-section : Macros pour architectures 32-bit spécifiques
    // -------------------------------------------------------------------------

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

    // -------------------------------------------------------------------------
    // Sous-section : Macros pour architectures spéciales / consoles
    // -------------------------------------------------------------------------

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

    // -------------------------------------------------------------------------
    // Sous-section : Macros pour familles d'architectures
    // -------------------------------------------------------------------------

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

    // -------------------------------------------------------------------------
    // Sous-section : Macros pour bitness (32/64-bit)
    // -------------------------------------------------------------------------

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

    // -------------------------------------------------------------------------
    // Sous-section : Macros pour endianness
    // -------------------------------------------------------------------------

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

    // -------------------------------------------------------------------------
    // Sous-section : Macros pour fonctionnalités CPU (SIMD)
    // -------------------------------------------------------------------------

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

    // =========================================================================
    // SECTION 14 : MACROS DE DÉBOGAGE ET VALIDATION
    // =========================================================================

    // -------------------------------------------------------------------------
    // Sous-section : Messages de compilation pour le debug
    // -------------------------------------------------------------------------
    // Émet des pragma message lors de la compilation si NKENTSEU_ARCH_DEBUG est défini.
    // Utile pour vérifier quelle architecture a été détectée sans exécuter le code.

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

    // -------------------------------------------------------------------------
    // Sous-section : Vérifications de cohérence à la compilation
    // -------------------------------------------------------------------------
    // Erreurs et warnings pour détecter les configurations incohérentes
    // qui pourraient indiquer un problème de détection ou de configuration.

    // Une architecture ne peut pas être à la fois 32-bit et 64-bit
    #if defined(NKENTSEU_ARCH_64BIT) && defined(NKENTSEU_ARCH_32BIT)
        #error "Nkentseu: Architecture ne peut pas être à la fois 64-bit et 32-bit"
    #endif

    // Si aucune bitness n'est détectée, émettre un warning et supposer 64-bit
    #if !defined(NKENTSEU_ARCH_64BIT) && !defined(NKENTSEU_ARCH_32BIT)
        #warning "Nkentseu: Bitness de l'architecture non détectée, supposition 64-bit"
        #define NKENTSEU_ARCH_64BIT
    #endif

    // Vérifications de cohérence architecture/bitness
    #if defined(NKENTSEU_ARCH_X86_64) && !defined(NKENTSEU_ARCH_64BIT)
        #error "Nkentseu: x86_64 doit être 64-bit"
    #endif

    #if defined(NKENTSEU_ARCH_X86) && defined(NKENTSEU_ARCH_64BIT)
        #error "Nkentseu: x86 doit être 32-bit"
    #endif

#endif // NKENTSEU_PLATFORM_NKARCHDETECT_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKARCHDETECT.H
// =============================================================================
// Ce fichier fournit des macros pour détecter l'architecture CPU à la compilation
// et exécuter du code conditionnel optimisé pour chaque plateforme.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Affichage d'informations d'architecture au runtime
// -----------------------------------------------------------------------------
/*
    #include "NkArchDetect.h"
    #include <iostream>

    void PrintArchInfo() {
        std::cout << "Architecture : " << NKENTSEU_ARCH_NAME << "\n";
        std::cout << "Version      : " << NKENTSEU_ARCH_VERSION << "\n";
        std::cout << "Bitness      : " << NKENTSEU_WORD_BITS << " bits\n";
        std::cout << "Endianness   : "
            #ifdef NKENTSEU_ARCH_LITTLE_ENDIAN
                  << "Little-endian\n";
            #elif defined(NKENTSEU_ARCH_BIG_ENDIAN)
                  << "Big-endian\n";
            #else
                  << "Unknown\n";
            #endif

        // Extensions SIMD disponibles
        #if defined(NKENTSEU_CPU_HAS_AVX2)
            std::cout << "SIMD         : AVX2 disponible\n";
        #elif defined(NKENTSEU_CPU_HAS_AVX)
            std::cout << "SIMD         : AVX disponible\n";
        #elif defined(NKENTSEU_CPU_HAS_SSE2)
            std::cout << "SIMD         : SSE2 disponible\n";
        #elif defined(NKENTSEU_CPU_HAS_NEON)
            std::cout << "SIMD         : NEON disponible\n";
        #else
            std::cout << "SIMD         : Aucun support SIMD détecté\n";
        #endif
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Sélection de chemin de code optimisé par architecture
// -----------------------------------------------------------------------------
/*
    #include "NkArchDetect.h"

    // Fonction de multiplication de vecteurs avec fallback portable
    void VectorMultiply(float* result, const float* a, const float* b, size_t count) {
        NKENTSEU_AVX2_ONLY({
            // Chemin optimisé AVX2 : traitement par blocs de 8 floats
            for (size_t i = 0; i < count; i += 8) {
                __m256 va = _mm256_loadu_ps(&a[i]);
                __m256 vb = _mm256_loadu_ps(&b[i]);
                __m256 vr = _mm256_mul_ps(va, vb);
                _mm256_storeu_ps(&result[i], vr);
            }
            return;
        });

        NKENTSEU_NEON_ONLY({
            // Chemin optimisé NEON : traitement par blocs de 4 floats
            for (size_t i = 0; i < count; i += 4) {
                float32x4_t va = vld1q_f32(&a[i]);
                float32x4_t vb = vld1q_f32(&b[i]);
                float32x4_t vr = vmulq_f32(va, vb);
                vst1q_f32(&result[i], vr);
            }
            return;
        });

        // Fallback portable pour architectures sans SIMD
        for (size_t i = 0; i < count; ++i) {
            result[i] = a[i] * b[i];
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Allocation mémoire alignée selon l'architecture
// -----------------------------------------------------------------------------
/*
    #include "NkArchDetect.h"
    #include <cstdlib>

    // Alloue un buffer aligné sur la ligne de cache pour éviter le false sharing
    void* AllocateCacheAligned(size_t size) {
        #if defined(_MSC_VER)
            return _aligned_malloc(size, NKENTSEU_CACHE_LINE_SIZE);
        #elif defined(__GNUC__) || defined(__clang__)
            void* ptr = nullptr;
            posix_memalign(&ptr, NKENTSEU_CACHE_LINE_SIZE, size);
            return ptr;
        #else
            // Fallback non-aligné (moins performant mais portable)
            return malloc(size);
        #endif
    }

    void FreeCacheAligned(void* ptr) {
        #if defined(_MSC_VER)
            _aligned_free(ptr);
        #else
            free(ptr);
        #endif
    }

    // Utilisation :
    // float* data = static_cast<float*>(AllocateCacheAligned(1024 * sizeof(float)));
    // ... utilisation ...
    // FreeCacheAligned(data);
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Code conditionnel par famille d'architecture
// -----------------------------------------------------------------------------
/*
    #include "NkArchDetect.h"

    void InitializePlatformOptimizations() {
        NKENTSEU_INTEL_ONLY({
            // Optimisations spécifiques x86/x86_64
            EnableFastStringOps();
            ConfigurePrefetchHints();
        });

        NKENTSEU_ARM_FAMILY_ONLY({
            // Optimisations spécifiques ARM
            ConfigureNEONRegisters();
            EnableHardwareCrypto();
        });

        NKENTSEU_64BIT_ONLY({
            // Code utilisant des pointeurs 64-bit ou registres larges
            UseWideRegisters();
        });

        NKENTSEU_LITTLE_ENDIAN_ONLY({
            // Optimisations dépendant de l'ordre des octets
            UseNativeByteOrder();
        });
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Structure alignée pour performance cache/SIMD
// -----------------------------------------------------------------------------
/*
    #include "NkArchDetect.h"

    // Structure de particule alignée sur 32 octets pour AVX/NEON
    struct NKENTSEU_ALIGN_32 Particle {
        float position[3];    // 12 octets
        float velocity[3];    // 12 octets
        float mass;           // 4 octets
        uint32_t flags;       // 4 octets
        // Total : 32 octets exactement = alignement parfait
    };

    // Tableau de particules aligné sur ligne de cache
    NKENTSEU_ALIGN_CACHE static Particle g_ParticleBuffer[4096];

    // Fonction de mise à jour utilisant l'alignement pour SIMD
    NKENTSEU_FORCE_INLINE void UpdateParticles(Particle* particles, size_t count) {
        NKENTSEU_AVX_ONLY({
            // Traitement AVX : 8 particules par itération grâce à l'alignement
            for (size_t i = 0; i < count; i += 8) {
                // ... code SIMD optimisé ...
            }
        });
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Détection de fonctionnalités CPU à la compilation
// -----------------------------------------------------------------------------
/*
    #include "NkArchDetect.h"

    // Sélection de l'implémentation de chiffrement selon les extensions disponibles
    void EncryptBlock(uint8_t* output, const uint8_t* input, const uint8_t* key) {
        NKENTSEU_AES_ONLY({
            // Utiliser les instructions AES-NI matérielles (très rapide)
            EncryptWithAESNI(output, input, key);
        });

        NKENTSEU_NOT_AES({
            // Fallback logiciel (plus lent mais portable)
            EncryptWithSoftwareAES(output, input, key);
        });
    }

    // Note : Pour une détection à l'exécution (CPUID, etc.), utiliser
    // une bibliothèque comme cpu_features ou implémenter votre propre détecteur.
    // Les macros de ce fichier ne détectent que ce qui est disponible À LA COMPILATION.
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Combinaison avec NkPlatformDetect.h pour code multi-plateforme
// -----------------------------------------------------------------------------
/*
    #include "NkPlatformDetect.h"  // Détection OS
    #include "NkArchDetect.h"      // Détection architecture

    void CrossPlatformInit() {
        // Combinaison OS + Architecture pour un ciblage précis
        NKENTSEU_WINDOWS_ONLY({
            NKENTSEU_X86_64_ONLY({
                // Windows sur x86_64 : utiliser les APIs spécifiques
                InitializeWindowsX64();
            });
        });

        NKENTSEU_PLATFORM_LINUX({
            NKENTSEU_ARM64_ONLY({
                // Linux sur ARM64 : optimisations serveur mobile
                InitializeLinuxARM64();
            });
        });

        NKENTSEU_PLATFORM_CONSOLE({
            NKENTSEU_CPU_HAS_NEON({
                // Console avec support NEON : activer les optimisations SIMD
                EnableConsoleNEONOptimizations();
            });
        });
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================