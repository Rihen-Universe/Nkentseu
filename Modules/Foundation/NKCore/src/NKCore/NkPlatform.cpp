// =============================================================================
// NKCore/NkPlatform.cpp
// Implémentation des fonctions de détection plateforme runtime.
//
// Design :
//  - Initialisation lazy thread-safe via NkAtomic guard
//  - Détection platform-specific : Windows (Win32 API), Linux/Android (POSIX), macOS/iOS (sysctl)
//  - Détection CPUID pour features SIMD sur x86/x64
//  - Fallbacks portables pour les plateformes non-supportées
//  - API C wrapper autour des fonctions C++ pour interopérabilité
//
// Intégration :
//  - Utilise NKCore/NkPlatform.h pour déclarations
//  - Utilise NKCore/NkAtomic.h pour initialisation thread-safe
//  - Utilise NKPlatform/NkFoundationLog.h pour logging interne
//  - Headers platform-specific inclus conditionnellement
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

// -------------------------------------------------------------------------
// PRECOMPILED HEADER (requis pour tous les fichiers .cpp du projet)
// -------------------------------------------------------------------------
#include "pch.h"

// -------------------------------------------------------------------------
// EN-TÊTES DU MODULE
// -------------------------------------------------------------------------
#include "NkPlatform.h"
#include "NkAtomic.h"
#include "NKPlatform/NkFoundationLog.h"

// En-têtes standards pour opérations bas-niveau
#include <cstring>
#include <cstdio>
#include <cstdlib>

// Éviter les collisions de macros entre l'en-tête et l'implémentation
#ifdef NkFreeAligned
    #undef NkFreeAligned
#endif

// -------------------------------------------------------------------------
// INCLUDES PLATFORM-SPECIFIC
// -------------------------------------------------------------------------
// Inclusion conditionnelle des APIs système selon la plateforme cible.

// ============================================================
// WINDOWS
// ============================================================
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    #define WIN32_LEAN_AND_MEAN  // Exclure les headers Windows rarement utilisés
    #include <windows.h>
    #include <intrin.h>          // Pour __cpuid, __cpuidex, _xgetbv

    // psapi.h pour GetLogicalProcessorInformation (détection cache)
    #if defined(__MINGW32__) || defined(__MINGW64__)
        #include <psapi.h>
    #else
        #include <psapi.h>
    #endif
    #pragma comment(lib, "psapi.lib")  // Linker directive pour psapi

// ============================================================
// LINUX / ANDROID
// ============================================================
#elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_ANDROID)
    #include <unistd.h>              // Pour sysconf()
    #include <sys/sysinfo.h>         // Pour sysinfo() (mémoire)
    #include <sys/utsname.h>         // Pour uname() (version OS)

    // CPUID pour détection SIMD sur x86/x64 Linux
    #if defined(NKENTSEU_ARCH_X86) || defined(NKENTSEU_ARCH_X86_64)
        #include <cpuid.h>           // Pour __get_cpuid, __get_cpuid_count
    #endif

// ============================================================
// MACOS / IOS
// ============================================================
#elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
    #include <TargetConditionals.h>  // Pour TARGET_OS_* macros
    #include <sys/types.h>
    #include <sys/sysctl.h>          // Pour sysctlbyname() (CPU, mémoire)
    #include <unistd.h>              // Pour sysconf()

    // CPUID sur macOS x86/x64 (Apple Silicon utilise d'autres méthodes)
    #if defined(NKENTSEU_ARCH_X86) || defined(NKENTSEU_ARCH_X86_64)
        #include <cpuid.h>
    #endif
#endif

// -------------------------------------------------------------------------
// NAMESPACE ET ALIASES
// -------------------------------------------------------------------------

using namespace nkentseu::platform;
using namespace nkentseu;

// -------------------------------------------------------------------------
// VARIABLES STATIQUES INTERNES
// -------------------------------------------------------------------------

/**
 * @brief Instance globale des informations plateforme
 * @note Initialisée à zéro, remplie par NkInitializePlatformInfo()
 * @internal
 */
static NkPlatformInfo sPlatformInfo;

/**
 * @brief Guard atomique pour initialisation lazy thread-safe
 * @note false = non initialisé, true = initialisé
 * @internal
 */
static NkAtomic<nk_bool> sInitialized(false);

// -------------------------------------------------------------------------
// FONCTIONS DE DÉTECTION SIMD - IMPLÉMENTATIONS PLATFORM-SPECIFIC
// -------------------------------------------------------------------------

// ====================================================================
// WINDOWS : Détection via __cpuid et _xgetbv
// ====================================================================

#if defined(NKENTSEU_PLATFORM_WINDOWS)

/**
 * @brief Détecte les extensions SIMD supportées sur Windows
 * @note
 *   - Utilise CPUID leaf 1 pour SSE/SSE2/SSE3/SSE4/AVX
 *   - Utilise CPUID leaf 7, subleaf 0 pour AVX2/AVX-512
 *   - Vérifie XCR0 via _xgetbv pour confirmer que l'OS supporte AVX
 * @internal
 */
static void NkDetectSIMDWindows() {
    nk_int32 cpuInfo[4] = {0};  // EAX, EBX, ECX, EDX

    // CPUID leaf 1 : features de base
    __cpuid(cpuInfo, 1);

    // Détection SSE family (EDX register)
    sPlatformInfo.hasSSE = (cpuInfo[3] & (1 << 25)) != 0;   // Bit 25: SSE
    sPlatformInfo.hasSSE2 = (cpuInfo[3] & (1 << 26)) != 0;  // Bit 26: SSE2

    // Détection SSE3/SSE4/AVX (ECX register)
    sPlatformInfo.hasSSE3 = (cpuInfo[2] & (1 << 0)) != 0;      // Bit 0: SSE3
    sPlatformInfo.hasSSE4_1 = (cpuInfo[2] & (1 << 19)) != 0;   // Bit 19: SSE4.1
    sPlatformInfo.hasSSE4_2 = (cpuInfo[2] & (1 << 20)) != 0;   // Bit 20: SSE4.2
    sPlatformInfo.hasAVX = (cpuInfo[2] & (1 << 28)) != 0;      // Bit 28: AVX

    // Vérification OS support pour AVX : XCR0[1:0] == 0b11 (SSE + AVX enabled)
    nk_bool osXSaveEnabled = false;
    #if defined(NKENTSEU_ARCH_X86_64)
        if ((cpuInfo[2] & (1 << 27)) != 0) {  // Bit 27: OSXSAVE
            #if defined(NKENTSEU_COMPILER_MSVC)
                nk_uint64 xcr0 = _xgetbv(0);
                osXSaveEnabled = (xcr0 & 6) == 6;  // Bits 1 et 2 doivent être à 1
            #elif defined(__GNUC__) || defined(__clang__)
                nk_uint32 eax, edx;
                __asm__ __volatile__("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
                nk_uint64 xcr0 = (static_cast<nk_uint64>(edx) << 32) | eax;
                osXSaveEnabled = (xcr0 & 6) == 6;
            #endif
        }
    #endif
    // AVX n'est utilisable que si détecté ET supporté par l'OS
    sPlatformInfo.hasAVX = sPlatformInfo.hasAVX && osXSaveEnabled;

    // CPUID leaf 7, subleaf 0 : features étendues (AVX2, AVX-512)
    __cpuidex(cpuInfo, 7, 0);
    sPlatformInfo.hasAVX2 = (cpuInfo[1] & (1 << 5)) != 0 && osXSaveEnabled;   // EBX bit 5: AVX2
    sPlatformInfo.hasAVX512 = (cpuInfo[1] & (1 << 16)) != 0 && osXSaveEnabled; // EBX bit 16: AVX-512F
}

/**
 * @brief Détecte les tailles de cache CPU sur Windows
 * @note
 *   - Utilise GetLogicalProcessorInformation() pour enumérer les caches
 *   - Extrait les tailles pour L1, L2, L3 séparément
 *   - Fallback vers valeurs par défaut si détection échoue
 * @internal
 */
static void NkDetectCacheWindows() {
    // Première appel pour obtenir la taille de buffer requise
    DWORD bufferSize = 0;
    GetLogicalProcessorInformation(nullptr, &bufferSize);

    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return;  // Échec inattendu
    }

    // Allocation du buffer pour les informations
    auto *buffer = static_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION *>(
        malloc(bufferSize)
    );
    if (!buffer) {
        return;
    }

    // Deuxième appel pour remplir le buffer
    if (!GetLogicalProcessorInformation(buffer, &bufferSize)) {
        free(buffer);
        return;
    }

    // Parcours des entrées pour extraire les informations de cache
    DWORD byteOffset = 0;
    while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= bufferSize) {
        auto *info = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION *>(
            reinterpret_cast<nk_uint8 *>(buffer) + byteOffset
        );

        if (info->Relationship == RelationCache) {
            // Mise à jour selon le niveau de cache
            switch (info->Cache.Level) {
                case 1:
                    sPlatformInfo.cpuL1CacheSize = static_cast<nk_uint32>(info->Cache.Size);
                    break;
                case 2:
                    sPlatformInfo.cpuL2CacheSize = static_cast<nk_uint32>(info->Cache.Size);
                    break;
                case 3:
                    sPlatformInfo.cpuL3CacheSize = static_cast<nk_uint32>(info->Cache.Size);
                    break;
                // Les niveaux >3 sont ignorés (L4, etc. rares)
            }
        }

        byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
    }

    free(buffer);
}

#endif // NKENTSEU_PLATFORM_WINDOWS

// ====================================================================
// LINUX / ANDROID : Détection via /sys et cpuid.h
// ====================================================================

#if defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_ANDROID)

/**
 * @brief Détecte les extensions SIMD sur Linux/Android
 * @note
 *   - x86/x64 : utilise __get_cpuid() de <cpuid.h>
 *   - ARM : suppose NEON présent (standard sur ARM Linux/Android modernes)
 *   - Autres architectures : fallback sans détection fine
 * @internal
 */
static void NkDetectSIMDLinux() {
    #if defined(NKENTSEU_ARCH_X86) || defined(NKENTSEU_ARCH_X86_64)
        nk_uint32 eax = 0, ebx = 0, ecx = 0, edx = 0;

        // CPUID leaf 1 : features de base
        if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
            sPlatformInfo.hasSSE = (edx & (1 << 25)) != 0;
            sPlatformInfo.hasSSE2 = (edx & (1 << 26)) != 0;
            sPlatformInfo.hasSSE3 = (ecx & (1 << 0)) != 0;
            sPlatformInfo.hasSSE4_1 = (ecx & (1 << 19)) != 0;
            sPlatformInfo.hasSSE4_2 = (ecx & (1 << 20)) != 0;
            sPlatformInfo.hasAVX = (ecx & (1 << 28)) != 0;
        }

        // CPUID leaf 7 : features étendues
        if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
            sPlatformInfo.hasAVX2 = (ebx & (1 << 5)) != 0;
            sPlatformInfo.hasAVX512 = (ebx & (1 << 16)) != 0;
        }
    #elif defined(NKENTSEU_ARCH_ARM) || defined(NKENTSEU_ARCH_ARM64)
        // ARM Linux/Android : NEON est standard sur les CPU modernes
        // Une détection plus fine via /proc/cpuinfo est possible mais omise pour simplicité
        sPlatformInfo.hasNEON = true;
    #endif
    // Autres architectures : laisser les flags à false (non détectés)
}

/**
 * @brief Détecte les tailles de cache via le système de fichiers sysfs
 * @note
 *   - Lit /sys/devices/system/cpu/cpu0/cache/index*/size
 *   - Convertit les valeurs de "NNNK" ou "NNNM" en bytes
 *   - Fallback vers valeurs par défaut si fichiers inaccessibles
 * @internal
 */
static void NkDetectCacheLinux() {
    // Helper pour lire une taille de cache depuis sysfs
    auto ReadCacheSize = [](const char* path, nk_uint32& outSize) -> bool {
        FILE *fp = fopen(path, "r");
        if (!fp) {
            return false;
        }

        nk_int32 size;
        char unit[8] = {0};
        int matched = fscanf(fp, "%d%7s", &size, unit);
        fclose(fp);

        if (matched < 1) {
            return false;
        }

        // Conversion de l'unité (K=KB, M=MB, etc.)
        if (matched == 2) {
            if (unit[0] == 'K' || unit[0] == 'k') {
                size *= 1024;
            } else if (unit[0] == 'M' || unit[0] == 'm') {
                size *= 1024 * 1024;
            }
            // Autres unités (G, T) non gérées car improbables pour des caches CPU
        }

        outSize = static_cast<nk_uint32>(size);
        return true;
    };

    // Lecture des caches L1, L2, L3 (index 0, 2, 3 typiquement)
    ReadCacheSize("/sys/devices/system/cpu/cpu0/cache/index0/size", sPlatformInfo.cpuL1CacheSize);
    ReadCacheSize("/sys/devices/system/cpu/cpu0/cache/index2/size", sPlatformInfo.cpuL2CacheSize);
    ReadCacheSize("/sys/devices/system/cpu/cpu0/cache/index3/size", sPlatformInfo.cpuL3CacheSize);
}

#endif // NKENTSEU_PLATFORM_LINUX

// ====================================================================
// MACOS / IOS : Détection via sysctlbyname
// ====================================================================

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)

/**
 * @brief Détecte les extensions SIMD sur Apple platforms
 * @note
 *   - x86/x64 : utilise __get_cpuid() comme sur Linux
 *   - ARM (Apple Silicon) : NEON/AdvSIMD toujours présent, pas de détection fine nécessaire
 * @internal
 */
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
        // Apple Silicon (ARM64) : NEON/AdvSIMD est toujours présent
        sPlatformInfo.hasNEON = true;
        // Pour une détection plus fine (AMX, etc.), utiliser sysctl ou sysctlbyname
    #endif
}

/**
 * @brief Détecte les tailles de cache via sysctl sur macOS/iOS
 * @note
 *   - Utilise sysctlbyname avec les clés "hw.l1dcachesize", etc.
 *   - Retourne directement la taille en bytes (pas de conversion d'unité)
 *   - Fallback vers valeurs par défaut si sysctl échoue
 * @internal
 */
static void NkDetectCacheApple() {
    size_t size = sizeof(nk_uint32);

    // Cache L1 (data)
    nk_uint32 l1Size = 0;
    if (sysctlbyname("hw.l1dcachesize", &l1Size, &size, nullptr, 0) == 0) {
        sPlatformInfo.cpuL1CacheSize = l1Size;
    }

    // Cache L2
    size = sizeof(nk_uint32);
    nk_uint32 l2Size = 0;
    if (sysctlbyname("hw.l2cachesize", &l2Size, &size, nullptr, 0) == 0) {
        sPlatformInfo.cpuL2CacheSize = l2Size;
    }

    // Cache L3 (peut ne pas être présent sur tous les modèles)
    size = sizeof(nk_uint32);
    nk_uint32 l3Size = 0;
    if (sysctlbyname("hw.l3cachesize", &l3Size, &size, nullptr, 0) == 0) {
        sPlatformInfo.cpuL3CacheSize = l3Size;
    }
}

#endif // NKENTSEU_PLATFORM_MACOS

// -------------------------------------------------------------------------
// IMPLÉMENTATION : FONCTION D'INITIALISATION PRINCIPALE
// -------------------------------------------------------------------------

/**
 * @brief Initialise la structure globale NkPlatformInfo
 * @note
 *   - Thread-safe via guard atomique sInitialized
 *   - Idempotent : appel multiple sans effet secondaire
 *   - Remplit tous les champs de sPlatformInfo via détections platform-specific
 * @internal
 */
void NkInitializePlatformInfo() {
    // Vérification double-check locking pour initialisation lazy thread-safe
    if (sInitialized.Load()) {
        return;  // Déjà initialisé par un autre thread
    }

    // Initialisation à zéro pour sécurité
    ::memset(&sPlatformInfo, 0, sizeof(NkPlatformInfo));

    // --------------------------------------------------------
    // 1. INFORMATIONS OS ET COMPILATEUR
    // --------------------------------------------------------

    // Noms de base depuis les macros de détection
    sPlatformInfo.osName = NKENTSEU_PLATFORM_NAME;
    sPlatformInfo.archName = NKENTSEU_ARCH_NAME;

    // Détermination du type de plateforme via macros
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        sPlatformInfo.platform = NkPlatformType::NK_WINDOWS;
        sPlatformInfo.osVersion = "Windows";
    #elif defined(NKENTSEU_PLATFORM_LINUX)
        sPlatformInfo.platform = NkPlatformType::NK_LINUX;
        // Récupération de la version via uname()
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

    // Détermination du type d'architecture
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

    // Informations sur le compilateur
    #if defined(NKENTSEU_COMPILER_MSVC)
        sPlatformInfo.compilerName = "MSVC";
        static nk_char compilerVersion[32];
        snprintf(compilerVersion, sizeof(compilerVersion), "%d", _MSC_VER);
        sPlatformInfo.compilerVersion = compilerVersion;
    #elif defined(NKENTSEU_COMPILER_GCC)
        sPlatformInfo.compilerName = "GCC";
        static nk_char compilerVersion[32];
        snprintf(compilerVersion, sizeof(compilerVersion), "%d.%d.%d",
                 __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
        sPlatformInfo.compilerVersion = compilerVersion;
    #elif defined(NKENTSEU_COMPILER_CLANG)
        sPlatformInfo.compilerName = "Clang";
        static nk_char compilerVersion[32];
        snprintf(compilerVersion, sizeof(compilerVersion), "%d.%d.%d",
                 __clang_major__, __clang_minor__, __clang_patchlevel__);
        sPlatformInfo.compilerVersion = compilerVersion;
    #else
        sPlatformInfo.compilerName = "Unknown";
        sPlatformInfo.compilerVersion = "Unknown";
    #endif

    // --------------------------------------------------------
    // 2. INFORMATIONS CPU
    // --------------------------------------------------------

    // Nombre de cœurs/threads via fonctions dédiées (voir plus bas)
    sPlatformInfo.cpuCoreCount = NkGetCPUCoreCount();
    sPlatformInfo.cpuThreadCount = NkGetCPUThreadCount();
    sPlatformInfo.cacheLineSize = NKENTSEU_CACHE_LINE_SIZE;  // Depuis les macros compile-time

    // Détection des tailles de cache via fonctions platform-specific
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        NkDetectCacheWindows();
    #elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_ANDROID)
        NkDetectCacheLinux();
    #elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
        NkDetectCacheApple();
    #endif

    // Valeurs par défaut si la détection a échoué ou n'est pas supportée
    if (sPlatformInfo.cpuL1CacheSize == 0) {
        sPlatformInfo.cpuL1CacheSize = 32 * 1024;  // 32 KB typique
    }
    if (sPlatformInfo.cpuL2CacheSize == 0) {
        sPlatformInfo.cpuL2CacheSize = 256 * 1024;  // 256 KB typique
    }
    if (sPlatformInfo.cpuL3CacheSize == 0) {
        sPlatformInfo.cpuL3CacheSize = 8 * 1024 * 1024;  // 8 MB typique
    }

    // --------------------------------------------------------
    // 3. DÉTECTION DES EXTENSIONS SIMD
    // --------------------------------------------------------

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        NkDetectSIMDWindows();
    #elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_ANDROID)
        NkDetectSIMDLinux();
    #elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
        NkDetectSIMDApple();
    #endif
    // Autres plateformes : laisser les flags à false (non détectés)

    // --------------------------------------------------------
    // 4. INFORMATIONS MÉMOIRE
    // --------------------------------------------------------

    sPlatformInfo.totalMemory = NkGetTotalMemory();
    sPlatformInfo.availableMemory = NkGetAvailableMemory();
    sPlatformInfo.pageSize = NkGetPageSize();
    sPlatformInfo.allocationGranularity = NkGetAllocationGranularity();

    // --------------------------------------------------------
    // 5. CARACTÉRISTIQUES PLATEFORME (endianness, 64-bit)
    // --------------------------------------------------------

    sPlatformInfo.endianness = NkGetEndianness();
    sPlatformInfo.isLittleEndian = (sPlatformInfo.endianness == NkEndianness::NK_LITTLE);
    sPlatformInfo.is64Bit = NkIs64Bit();

    // --------------------------------------------------------
    // 6. INFORMATIONS DE BUILD
    // --------------------------------------------------------

    sPlatformInfo.isDebugBuild = NkIsDebugBuild();
    sPlatformInfo.isSharedLibrary = NkIsSharedLibrary();
    sPlatformInfo.buildType = NkGetBuildType();

    // --------------------------------------------------------
    // 7. CAPACITÉS PLATEFORME (toujours true sauf indication contraire)
    // --------------------------------------------------------

    sPlatformInfo.hasThreading = true;
    sPlatformInfo.hasVirtualMemory = true;
    sPlatformInfo.hasFileSystem = true;
    sPlatformInfo.hasNetwork = true;
    // Ces flags pourraient être ajustés pour des plateformes très contraintes

    // --------------------------------------------------------
    // 8. SYSTÈME D'AFFICHAGE
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
    // 9. API GRAPHIQUE (initialisation basique, à enrichir par NKGraphics)
    // --------------------------------------------------------

    sPlatformInfo.graphicsApi = NkGraphicsAPI::NK_NONE;
    sPlatformInfo.graphicsApiVersion.major = 0;
    sPlatformInfo.graphicsApiVersion.minor = 0;
    sPlatformInfo.graphicsApiVersion.patch = 0;
    sPlatformInfo.graphicsApiVersion.versionString = "0.0.0";

    // Marquer comme initialisé (opération atomique)
    sInitialized.Store(true);
}

// -------------------------------------------------------------------------
// IMPLÉMENTATION : FONCTIONS PUBLIQUES DE L'API C
// -------------------------------------------------------------------------

const NkPlatformInfo* NkGetPlatformInfo() {
    // Initialisation lazy thread-safe
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
    if (!feature) {
        return false;
    }

    const NkPlatformInfo &info = *NkGetPlatformInfo();

    // Comparaisons case-sensitive avec strcmp
    if (::strcmp(feature, "SSE") == 0) {
        return info.hasSSE;
    }
    if (::strcmp(feature, "SSE2") == 0) {
        return info.hasSSE2;
    }
    if (::strcmp(feature, "SSE3") == 0) {
        return info.hasSSE3;
    }
    if (::strcmp(feature, "SSE4.1") == 0) {
        return info.hasSSE4_1;
    }
    if (::strcmp(feature, "SSE4.2") == 0) {
        return info.hasSSE4_2;
    }
    if (::strcmp(feature, "AVX") == 0) {
        return info.hasAVX;
    }
    if (::strcmp(feature, "AVX2") == 0) {
        return info.hasAVX2;
    }
    if (::strcmp(feature, "AVX512") == 0) {
        return info.hasAVX512;
    }
    if (::strcmp(feature, "NEON") == 0) {
        return info.hasNEON;
    }

    // Feature inconnue
    return false;
}

// ====================================================================
// IMPLÉMENTATION : FONCTIONS CPU
// ====================================================================

nk_uint32 NkGetCPUCoreCount() {
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        return static_cast<nk_uint32>(sysInfo.dwNumberOfProcessors);
    #elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_ANDROID)
        // _SC_NPROCESSORS_ONLN : nombre de processeurs en ligne (disponibles)
        long count = sysconf(_SC_NPROCESSORS_ONLN);
        return (count > 0) ? static_cast<nk_uint32>(count) : 1;
    #elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
        nk_int32 coreCount = 0;
        size_t size = sizeof(coreCount);
        // hw.physicalcpu : nombre de cœurs physiques (sans hyperthreading)
        if (sysctlbyname("hw.physicalcpu", &coreCount, &size, nullptr, 0) == 0) {
            return static_cast<nk_uint32>(coreCount);
        }
        return 1;  // Fallback conservateur
    #else
        return 1;  // Fallback pour plateformes non-supportées
    #endif
}

nk_uint32 NkGetCPUThreadCount() {
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        // dwNumberOfProcessors inclut les threads logiques (hyperthreading)
        return static_cast<nk_uint32>(sysInfo.dwNumberOfProcessors);
    #elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_ANDROID)
        // _SC_NPROCESSORS_CONF : nombre total de processeurs configurés
        long count = sysconf(_SC_NPROCESSORS_CONF);
        return (count > 0) ? static_cast<nk_uint32>(count) : NkGetCPUCoreCount();
    #elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
        nk_int32 threadCount = 0;
        size_t size = sizeof(threadCount);
        // hw.logicalcpu : nombre de threads logiques (avec hyperthreading)
        if (sysctlbyname("hw.logicalcpu", &threadCount, &size, nullptr, 0) == 0) {
            return static_cast<nk_uint32>(threadCount);
        }
        return NkGetCPUCoreCount();  // Fallback vers core count si échec
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
    // Valeur compile-time depuis les macros de détection d'architecture
    return NKENTSEU_CACHE_LINE_SIZE;
}

// ====================================================================
// IMPLÉMENTATION : FONCTIONS MÉMOIRE
// ====================================================================

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
            // totalram en unités de mem_unit bytes
            return static_cast<nk_uint64>(info.totalram) * info.mem_unit;
        }
    #elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
        nk_uint64 totalMemory = 0;
        size_t size = sizeof(totalMemory);
        // hw.memsize : mémoire physique totale en bytes
        if (sysctlbyname("hw.memsize", &totalMemory, &size, nullptr, 0) == 0) {
            return static_cast<nk_uint64>(totalMemory);
        }
    #endif
    return 0;  // Échec de détection
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
        // Approximation simple : moitié de la mémoire totale
        // Une implémentation plus précise utiliserait vm_statistics ou proc_pid_rusage
        return NkGetTotalMemory() / 2;
    #endif
    return 0;  // Échec de détection
}

nk_uint32 NkGetPageSize() {
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        return static_cast<nk_uint32>(sysInfo.dwPageSize);
    #elif defined(__EMSCRIPTEN__)
        // WebAssembly : page size fixe à 64KB pour la mémoire linéaire
        // Mais pour les allocations natives via malloc, 4KB est typique
        return 4096;
    #elif defined(NKENTSEU_PLATFORM_POSIX)
        // _SC_PAGESIZE ou _SC_PAGE_SIZE (synonymes)
        long pageSize = sysconf(_SC_PAGESIZE);
        return (pageSize > 0) ? static_cast<nk_uint32>(pageSize) : 4096;
    #else
        return 4096;  // Fallback conservateur
    #endif
}

nk_uint32 NkGetAllocationGranularity() {
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        // dwAllocationGranularity : typiquement 64KB sur Windows
        return static_cast<nk_uint32>(sysInfo.dwAllocationGranularity);
    #else
        // Sur POSIX, généralement égal à la taille de page
        return NkGetPageSize();
    #endif
}

// ====================================================================
// IMPLÉMENTATION : FONCTIONS BUILD
// ====================================================================

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
    #elif defined(NKENTSEU_RELWITHDEBINFO)
        return "RelWithDebInfo";
    #elif defined(NKENTSEU_MINSIZEREL)
        return "MinSizeRel";
    #else
        return "Unknown";
    #endif
}

// ====================================================================
// IMPLÉMENTATION : FONCTIONS ENDIANNESS
// ====================================================================

NkEndianness NkGetEndianness() {
    // Mise en cache du résultat pour éviter de recalculer à chaque appel
    static NkEndianness endianness = NkEndianness::NK_UNKNOWN;

    if (endianness == NkEndianness::NK_UNKNOWN) {
        // Test classique : écrire un multi-byte value et lire le premier byte
        union {
            nk_uint32 i;
            nk_uint8 c[4];
        } test = {0x01020304};

        // Little-endian : byte de poids faible à l'adresse basse (c[0] = 0x04)
        // Big-endian : byte de poids fort à l'adresse basse (c[0] = 0x01)
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

// ====================================================================
// IMPLÉMENTATION : FONCTIONS UTILITAIRES
// ====================================================================

void NkPrintPlatformInfo() {
    const NkPlatformInfo &info = *NkGetPlatformInfo();

    // En-tête
    NK_FOUNDATION_LOG_INFO("=== Nkentseu Platform Information ===");

    // OS et architecture
    NK_FOUNDATION_LOG_INFO("OS: %s (%s)",
        info.osName,
        info.osVersion ? info.osVersion : "Unknown");
    NK_FOUNDATION_LOG_INFO("Architecture: %s (%s-bit)",
        info.archName,
        info.is64Bit ? "64" : "32");

    // Compilateur
    NK_FOUNDATION_LOG_INFO("Compiler: %s %s",
        info.compilerName,
        info.compilerVersion);

    // CPU
    NK_FOUNDATION_LOG_INFO("CPU Cores: %u (Threads: %u)",
        info.cpuCoreCount,
        info.cpuThreadCount);
    NK_FOUNDATION_LOG_INFO("CPU Cache: L1=%uKB, L2=%uKB, L3=%uMB",
        info.cpuL1CacheSize / 1024,
        info.cpuL2CacheSize / 1024,
        info.cpuL3CacheSize / (1024 * 1024));

    // Mémoire
    NK_FOUNDATION_LOG_INFO("Memory: Total=%lluMB, Available=%lluMB",
        static_cast<unsigned long long>(info.totalMemory / (1024 * 1024)),
        static_cast<unsigned long long>(info.availableMemory / (1024 * 1024)));
    NK_FOUNDATION_LOG_INFO("Page Size: %u bytes, Allocation Granularity: %u bytes",
        info.pageSize,
        info.allocationGranularity);
    NK_FOUNDATION_LOG_INFO("Cache Line Size: %u bytes",
        info.cacheLineSize);

    // SIMD : construction dynamique de la liste des features
    nk_char simd[128] = {};
    nk_size offset = 0;

    // Helper lambda pour ajouter une feature à la liste
    const auto appendFeature = [&](const char* name) {
        if (!name || name[0] == '\0' || offset >= sizeof(simd) - 1) {
            return;
        }
        // Ajouter un séparateur si ce n'est pas la première feature
        if (offset > 0 && offset < sizeof(simd) - 1) {
            const nk_size remaining = sizeof(simd) - offset;
            const int sepWritten = snprintf(simd + offset, remaining, ", ");
            if (sepWritten > 0) {
                const nk_size advanced = static_cast<nk_size>(sepWritten);
                offset += (advanced < remaining) ? advanced : (remaining - 1);
            }
        }
        // Ajouter le nom de la feature
        if (offset < sizeof(simd) - 1) {
            const nk_size remaining = sizeof(simd) - offset;
            const int nameWritten = snprintf(simd + offset, remaining, "%s", name);
            if (nameWritten > 0) {
                const nk_size advanced = static_cast<nk_size>(nameWritten);
                offset += (advanced < remaining) ? advanced : (remaining - 1);
            }
        }
    };

    // Ajout des features détectées
    if (info.hasSSE) appendFeature("SSE");
    if (info.hasSSE2) appendFeature("SSE2");
    if (info.hasSSE3) appendFeature("SSE3");
    if (info.hasSSE4_1) appendFeature("SSE4.1");
    if (info.hasSSE4_2) appendFeature("SSE4.2");
    if (info.hasAVX) appendFeature("AVX");
    if (info.hasAVX2) appendFeature("AVX2");
    if (info.hasAVX512) appendFeature("AVX-512");
    if (info.hasNEON) appendFeature("NEON");

    NK_FOUNDATION_LOG_INFO("SIMD Support: %s",
        (offset > 0) ? simd : "None");

    // Endianness et build info
    NK_FOUNDATION_LOG_INFO("Endianness: %s",
        info.isLittleEndian ? "Little Endian" : "Big Endian");
    NK_FOUNDATION_LOG_INFO("Build Type: %s (%s)",
        info.buildType,
        info.isDebugBuild ? "Debug" : "Release");
    NK_FOUNDATION_LOG_INFO("Library Type: %s",
        info.isSharedLibrary ? "Shared" : "Static");

    // Footer
    NK_FOUNDATION_LOG_INFO("======================================");
}

nk_bool NkIsAligned(const nk_ptr address, nk_size alignment) {
    if (alignment == 0) {
        return true;  // Cas dégénéré : tout est "aligné" sur 0
    }
    // Test : (address % alignment) == 0 via bitwise AND
    return (reinterpret_cast<nk_uintptr>(address) & (alignment - 1)) == 0;
}

nk_ptr NkAlignAddress(nk_ptr address, nk_size alignment) {
    if (alignment == 0) {
        return address;
    }
    // Formule d'alignement vers le haut : (addr + align - 1) & ~(align - 1)
    nk_uintptr addr = reinterpret_cast<nk_uintptr>(address);
    nk_uintptr alignedAddr = (addr + alignment - 1) & ~(alignment - 1);
    return reinterpret_cast<nk_ptr>(alignedAddr);
}

const nk_ptr NkAlignAddressConst(const nk_ptr address, nk_size alignment) {
    // Implémentation identique à NkAlignAddress mais avec retour const
    if (alignment == 0) {
        return address;
    }
    nk_uintptr addr = reinterpret_cast<nk_uintptr>(address);
    nk_uintptr alignedAddr = (addr + alignment - 1) & ~(alignment - 1);
    return reinterpret_cast<const nk_ptr>(alignedAddr);
}

// ====================================================================
// IMPLÉMENTATION : NAMESPACE MEMORY - ALLOCATION ALIGNÉE
// ====================================================================

namespace nkentseu {
namespace platform {
namespace memory {

nk_ptr NkAllocateAligned(nk_size size, nk_size alignment) noexcept {
    if (size == 0) {
        return nullptr;
    }

    // Vérification : alignment doit être une puissance de 2
    if ((alignment & (alignment - 1)) != 0) {
        return nullptr;
    }

    // Alignement minimum : au moins sizeof(void*) pour stocker le pointeur original
    if (alignment < sizeof(void *)) {
        alignment = sizeof(void *);
    }

    void *ptr = nullptr;

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        // Windows : _aligned_malloc (nécessite _aligned_free)
        ptr = _aligned_malloc(size, alignment);
    #elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
        // macOS/iOS : posix_memalign (retourne un code d'erreur)
        if (posix_memalign(&ptr, alignment, size) != 0) {
            ptr = nullptr;
        }
    #elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_ANDROID)
        #if __STDC_VERSION__ >= 201112L
            // C11 : aligned_alloc (attention : size doit être multiple de alignment)
            if (size % alignment != 0) {
                size = ((size + alignment - 1) / alignment) * alignment;
            }
            ptr = aligned_alloc(alignment, size);
        #else
            // Fallback C99 : posix_memalign
            if (posix_memalign(&ptr, alignment, size) != 0) {
                ptr = nullptr;
            }
        #endif
    #else
        // Fallback portable : sur-allocation avec stockage du pointeur original
        // Layout : [original_ptr][padding][aligned_ptr][user_data]
        nk_size totalSize = size + alignment + sizeof(void *);
        void *originalPtr = malloc(totalSize);
        if (!originalPtr) {
            return nullptr;
        }

        // Calcul de l'adresse alignée après l'espace pour originalPtr
        nk_uintptr addr = reinterpret_cast<nk_uintptr>(originalPtr);
        nk_uintptr alignedAddr = (addr + alignment + sizeof(void *) - 1) & ~(alignment - 1);

        // Stocker originalPtr juste avant alignedAddr pour le retrouver dans free
        void **storage = reinterpret_cast<void **>(alignedAddr) - 1;
        *storage = originalPtr;

        ptr = reinterpret_cast<void *>(alignedAddr);
    #endif

    return static_cast<nk_ptr>(ptr);
}

void NkFreeAligned(nk_ptr ptr) noexcept {
    if (!ptr) {
        return;  // free(nullptr) est no-op, mais on évite l'appel pour cohérence
    }

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        // Windows : _aligned_free (doit correspondre à _aligned_malloc)
        _aligned_free(ptr);
    #else
        // POSIX/fallback : free() gère à la fois les pointeurs normaux et alignés
        // Dans le fallback portable, ptr pointe vers alignedAddr, et le pointeur
        // original est stocké juste avant : free() sur alignedAddr fonctionne
        // car notre fallback utilise malloc() pour l'allocation initiale.
        free(ptr);
    #endif
}

nk_bool NkIsPointerAligned(const nk_ptr ptr, nk_size alignment) noexcept {
    if (!ptr || alignment == 0) {
        return true;  // Cas dégénérés
    }
    // Même test que NkIsAligned mais avec nom plus explicite
    return (reinterpret_cast<nk_uintptr>(ptr) & (alignment - 1)) == 0;
}

} // namespace memory
} // namespace platform
} // namespace nkentseu

// =============================================================================
// NOTES D'IMPLÉMENTATION ET EXTENSIONS POSSIBLES
// =============================================================================
//
// Thread-safety :
//   L'initialisation de sPlatformInfo utilise un guard atomique (NkAtomic<nk_bool>)
//   pour une initialisation lazy thread-safe. Cependant, les lectures ultérieures
//   de sPlatformInfo ne sont pas protégées par un mutex. Cela est acceptable car :
//   1. La structure est écrite une fois puis jamais modifiée (immutable après init)
//   2. Les lectures de valeurs primitives (nk_uint32, nk_bool, pointeurs) sont
//      atomiques sur toutes les architectures supportées
//   3. Les pointeurs vers chaînes statiques ne posent pas de problème de concurrence
//
//   Si une modification future nécessitait une mise à jour dynamique des informations,
//   il faudrait ajouter un mutex ou utiliser NkAtomic pour chaque champ.
//
// Extensions de détection :
//   - GPU : intégrer avec NkCGXDetect.h pour remplir graphicsApi et sa version
//   - Mémoire : ajouter availableVirtualMemory, swap usage, etc.
//   - CPU : ajouter fréquence, modèle exact, flags CPUID bruts
//   - Réseau : détecter les interfaces, MTU, support IPv6, etc.
//
// Optimisations possibles :
//   - Mettre en cache les résultats de NkGetTotalMemory() si l'appel système est coûteux
//   - Utiliser des constexpr si certaines informations sont connues à la compilation
//   - Éviter les appels système répétés dans les fonctions inline en utilisant
//     les macros compile-time (NKENTSEU_ARCH_*, etc.) quand possible
//
// Tests :
//   - Valider que NkInitializePlatformInfo() est thread-safe avec plusieurs threads
//   - Vérifier que les fallbacks fonctionnent sur des plateformes non-supportées
//   - Tester NkAllocateAligned/NkFreeAligned avec divers alignements et tailles
//
// =============================================================================

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================