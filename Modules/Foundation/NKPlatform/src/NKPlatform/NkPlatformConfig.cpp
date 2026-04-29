// =============================================================================
// NKPlatform/NkPlatformConfig.cpp
// Implémentation de la configuration de plateforme et détection hardware.
//
// Design :
//  - Initialisation des structures PlatformConfig et PlatformCapabilities
//  - Détection runtime de la mémoire, CPU, et affichage via APIs système
//  - Fallbacks portables pour les plateformes non supportées
//  - Singleton thread-safe via initialisation statique locale (C++11+)
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "NKPlatform/NkPlatformConfig.h"

// -------------------------------------------------------------------------
// En-têtes système pour la détection hardware
// -------------------------------------------------------------------------
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    #include <windows.h>
#elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_ANDROID)
    #include <unistd.h>
    #include <sys/sysinfo.h>
#elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/sysctl.h>
#else
    // Fallback générique POSIX
    #include <unistd.h>
#endif

// -------------------------------------------------------------------------
// En-têtes standards pour les opérations de base
// -------------------------------------------------------------------------
#include <cstdlib>

// -------------------------------------------------------------------------
// Espace de noms principal
// -------------------------------------------------------------------------
namespace nkentseu {

    namespace platform {

        // ====================================================================
        // SECTION 1 : IMPLÉMENTATION DE PLATFORMCONFIG
        // ====================================================================
        // Initialisation des valeurs de configuration déterminées à la compilation.

        PlatformConfig::PlatformConfig()
            : platformName(GetPlatformName()),
              archName(GetArchName()),
              compilerName(GetCompilerName()),
              compilerVersion(NKENTSEU_COMPILER_VERSION),
              isDebugBuild(NKENTSEU_DEBUG_BUILD),
              isReleaseBuild(NKENTSEU_RELEASE_BUILD),
              is64Bit(Is64Bit()),
              isLittleEndian(IsLittleEndian()),
              hasUnicode(NKENTSEU_HAS_UNICODE),
              hasThreading(NKENTSEU_HAS_THREADING),
              hasFilesystem(NKENTSEU_HAS_FILESYSTEM),
              hasNetwork(NKENTSEU_HAS_NETWORK),
              maxPathLength(NKENTSEU_MAX_PATH),
              cacheLineSize(NKENTSEU_CACHE_LINE_SIZE) {
            // Toutes les valeurs sont initialisées via des macros compile-time
            // ou des fonctions inline déterministes. Aucune détection runtime ici.
        }

        const PlatformConfig& GetPlatformConfig() {
            // Initialisation statique locale : thread-safe en C++11+
            // La configuration est déterminée une seule fois au premier appel
            static PlatformConfig config;
            return config;
        }

        // ====================================================================
        // SECTION 2 : IMPLÉMENTATION DE PLATFORMCAPABILITIES
        // ====================================================================
        // Détection runtime des capacités matérielles via APIs système.

        PlatformCapabilities::PlatformCapabilities()
            : totalPhysicalMemory(0),
              availablePhysicalMemory(0),
              pageSize(0),
              processorCount(0),
              logicalProcessorCount(0),
              hasDisplay(false),
              primaryScreenWidth(0),
              primaryScreenHeight(0),
              hasSSE(false),
              hasSSE2(false),
              hasAVX(false),
              hasAVX2(false),
              hasNEON(false) {

            // -----------------------------------------------------------------
            // Détection de la mémoire physique
            // -----------------------------------------------------------------

            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                // Windows : utiliser GlobalMemoryStatusEx pour mémoire et SYSTEM_INFO pour page size
                MEMORYSTATUSEX memStatus;
                memStatus.dwLength = sizeof(memStatus);

                if (GlobalMemoryStatusEx(&memStatus)) {
                    totalPhysicalMemory = static_cast<size_t>(memStatus.ullTotalPhys);
                    availablePhysicalMemory = static_cast<size_t>(memStatus.ullAvailPhys);
                }

                SYSTEM_INFO sysInfo;
                GetSystemInfo(&sysInfo);
                pageSize = static_cast<size_t>(sysInfo.dwPageSize);
                processorCount = static_cast<int>(sysInfo.dwNumberOfProcessors);
                logicalProcessorCount = static_cast<int>(sysInfo.dwNumberOfProcessors);

                // Détection de l'affichage via GetSystemMetrics
                primaryScreenWidth = GetSystemMetrics(SM_CXSCREEN);
                primaryScreenHeight = GetSystemMetrics(SM_CYSCREEN);
                hasDisplay = (primaryScreenWidth > 0 && primaryScreenHeight > 0);

            #elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_ANDROID)
                // Linux/Android : utiliser sysconf pour mémoire et processeurs
                long pages = sysconf(_SC_PHYS_PAGES);
                long pageSizeBytes = sysconf(_SC_PAGE_SIZE);

                if (pages > 0 && pageSizeBytes > 0) {
                    totalPhysicalMemory = static_cast<size_t>(pages) * static_cast<size_t>(pageSizeBytes);
                }

                long availPages = sysconf(_SC_AVPHYS_PAGES);
                if (availPages > 0) {
                    availablePhysicalMemory = static_cast<size_t>(availPages) * static_cast<size_t>(pageSizeBytes);
                }

                pageSize = static_cast<size_t>(pageSizeBytes);

                long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
                if (nprocs > 0) {
                    processorCount = static_cast<int>(nprocs);
                    logicalProcessorCount = processorCount;
                }

                // Détection basique de l'affichage via variable d'environnement DISPLAY
                const char* display = ::getenv("DISPLAY");
                hasDisplay = (display != nullptr && display[0] != '\0');

            #elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
                // Apple platforms : utiliser sysctlbyname pour mémoire et sysconf pour le reste
                size_t memSize = sizeof(totalPhysicalMemory);

                if (sysctlbyname("hw.memsize", &totalPhysicalMemory, &memSize, nullptr, 0) != 0) {
                    totalPhysicalMemory = 0;
                }

                // Estimation conservative de la mémoire disponible (50% du total)
                // Une détection précise nécessiterait vm_statistics64 sur macOS
                availablePhysicalMemory = totalPhysicalMemory / 2;

                long pageSizeBytes = sysconf(_SC_PAGESIZE);
                if (pageSizeBytes > 0) {
                    pageSize = static_cast<size_t>(pageSizeBytes);
                }

                long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
                if (nprocs > 0) {
                    processorCount = static_cast<int>(nprocs);
                    logicalProcessorCount = processorCount;
                }

                // macOS/iOS ont toujours un affichage (même headless via VNC)
                hasDisplay = true;

            #else
                // Fallback générique POSIX
                long pageSizeBytes = sysconf(_SC_PAGESIZE);
                if (pageSizeBytes > 0) {
                    pageSize = static_cast<size_t>(pageSizeBytes);
                }

                long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
                if (nprocs > 0) {
                    processorCount = static_cast<int>(nprocs);
                    logicalProcessorCount = processorCount;
                }
            #endif

            // -----------------------------------------------------------------
            // Détection des fonctionnalités CPU SIMD (compile-time via macros)
            // -----------------------------------------------------------------
            // Ces flags sont déterminés à la compilation via les macros du compilateur.
            // Pour une détection runtime précise (CPUID, etc.), utiliser NkCPUFeatures.h.

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

        const PlatformCapabilities& GetPlatformCapabilities() {
            // Initialisation statique locale : thread-safe en C++11+
            // La détection hardware est effectuée une seule fois au premier appel
            static PlatformCapabilities caps;
            return caps;
        }

    } // namespace platform

} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================