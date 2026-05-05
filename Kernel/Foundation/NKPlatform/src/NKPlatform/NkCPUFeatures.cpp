// =============================================================================
// NKPlatform/NkCPUFeatures.cpp
// Implémentation de la détection des fonctionnalités CPU.
//
// Design :
//  - Implémentation multi-plateforme de la détection CPU
//  - Utilisation de CPUID sur x86/x64, /proc/cpuinfo sur Linux/Android
//  - APIs système (sysctl, GetSystemInfo) sur macOS/iOS/Windows
//  - Fallback vers des valeurs par défaut conservatrices
//  - Thread-safe grâce au singleton avec initialisation statique locale
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "NKPlatform/NkCPUFeatures.h"

// Inclusion des en-têtes système nécessaires pour la détection
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// -------------------------------------------------------------------------
// En-têtes spécifiques à la plateforme
// -------------------------------------------------------------------------
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    #include <windows.h>
    #include <intrin.h>
#elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_ANDROID)
    #include <unistd.h>
    #include <sys/sysinfo.h>
#elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
    #include <sys/sysctl.h>
#endif

// -------------------------------------------------------------------------
// Espace de noms principal
// -------------------------------------------------------------------------
namespace nkentseu {

    namespace platform {

        // ====================================================================
        // SECTION 1 : NAMESPACE ANONYME (FONCTIONS UTILITAIRES INTERNES)
        // ====================================================================
        // Fonctions helper non exposées dans l'API publique.
        // Indentées d'un niveau supplémentaire car dans namespace anonyme.

        namespace {

            /**
             * @brief Copier une chaîne avec troncature sécurisée
             * @param dst Buffer de destination
             * @param dstCount Taille du buffer de destination
             * @param src Chaîne source à copier
             * @ingroup CPUFeaturesInternals
             *
             * Garantit que dst est toujours null-terminé.
             * Ne fait rien si dst ou dstCount sont invalides.
             */
            NKENTSEU_API_INLINE void CopyStringTruncated(
                char* dst,
                size_t dstCount,
                const char* src
            ) {
                if (!dst || dstCount == 0u) {
                    return;
                }

                dst[0] = '\0';

                if (!src) {
                    return;
                }

                ::snprintf(dst, static_cast<size_t>(dstCount), "%s", src);
            }

            /**
             * @brief Copier une chaîne après avoir trimmé les espaces
             * @param text Chaîne source à traiter
             * @param dst Buffer de destination
             * @param dstCount Taille du buffer de destination
             * @ingroup CPUFeaturesInternals
             *
             * Supprime les espaces en début et fin de chaîne avant copie.
             * Utile pour parser les fichiers /proc/cpuinfo.
             */
            NKENTSEU_API_INLINE void TrimStringToBuffer(
                const char* text,
                char* dst,
                size_t dstCount
            ) {
                if (!dst || dstCount == 0u) {
                    return;
                }

                dst[0] = '\0';

                if (!text) {
                    return;
                }

                // Trouver le début non-espace
                const char* start = text;
                while (*start && ::isspace(static_cast<unsigned char>(*start)) != 0) {
                    ++start;
                }

                // Trouver la fin non-espace
                const char* end = text + ::strlen(text);
                while (end > start && ::isspace(static_cast<unsigned char>(*(end - 1))) != 0) {
                    --end;
                }

                // Copier la portion trimmée
                const size_t trimmedLen = static_cast<size_t>(end - start);
                if (trimmedLen == 0u) {
                    return;
                }

                const size_t copyLen = (trimmedLen < static_cast<size_t>(dstCount - 1u))
                    ? trimmedLen
                    : static_cast<size_t>(dstCount - 1u);

                ::memcpy(dst, start, copyLen);
                dst[copyLen] = '\0';
            }

            /**
             * @brief Vérifier si une chaîne contient un sous-chaîne
             * @param text Chaîne dans laquelle chercher
             * @param pattern Sous-chaîne à rechercher
             * @return true si pattern est trouvé dans text, false sinon
             * @ingroup CPUFeaturesInternals
             */
            NKENTSEU_API_INLINE bool ContainsSubString(
                const char* text,
                const char* pattern
            ) {
                return (text && pattern && ::strstr(text, pattern) != nullptr);
            }

        } // namespace anonyme

        // ====================================================================
        // SECTION 2 : CONSTRUCTEURS DES STRUCTURES DE DONNÉES
        // ====================================================================
        // Initialisation avec des valeurs par défaut conservatrices.
        // Ces valeurs seront mises à jour par les fonctions de détection.

        // -------------------------------------------------------------------------
        // Constructeur CacheInfo
        // -------------------------------------------------------------------------
        NKENTSEU_API_INLINE CacheInfo::CacheInfo()
            : lineSize(64),
              l1DataSize(32),
              l1InstructionSize(32),
              l2Size(256),
              l3Size(8192) {
            // Valeurs par défaut typiques pour un CPU moderne
            // Seront écrasées par DetectCache() si la détection réussit
        }

        // -------------------------------------------------------------------------
        // Constructeur CPUTopology
        // -------------------------------------------------------------------------
        NKENTSEU_API_INLINE CPUTopology::CPUTopology()
            : numPhysicalCores(1),
              numLogicalCores(1),
              numSockets(1),
              hasHyperThreading(false) {
            // Minimum : 1 cœur physique, 1 logique, 1 socket
            // Seront mis à jour par DetectTopology()
        }

        // -------------------------------------------------------------------------
        // Constructeur SIMDFeatures
        // -------------------------------------------------------------------------
        NKENTSEU_API_INLINE SIMDFeatures::SIMDFeatures()
            : hasMMX(false),
              hasSSE(false),
              hasSSE2(false),
              hasSSE3(false),
              hasSSSE3(false),
              hasSSE41(false),
              hasSSE42(false),
              hasAVX(false),
              hasAVX2(false),
              hasAVX512F(false),
              hasAVX512DQ(false),
              hasAVX512BW(false),
              hasAVX512VL(false),
              hasFMA(false),
              hasFMA4(false),
              hasNEON(false),
              hasSVE(false),
              hasSVE2(false) {
            // Toutes les fonctionnalités désactivées par défaut
            // Seront activées par DetectSIMDFeatures() si détectées
        }

        // -------------------------------------------------------------------------
        // Constructeur ExtendedFeatures
        // -------------------------------------------------------------------------
        NKENTSEU_API_INLINE ExtendedFeatures::ExtendedFeatures()
            : hasAES(false),
              hasSHA(false),
              hasRDRAND(false),
              hasRDSEED(false),
              hasCLFLUSH(false),
              hasCLFLUSHOPT(false),
              hasPREFETCHWT1(false),
              hasMOVBE(false),
              hasPOPCNT(false),
              hasLZCNT(false),
              hasBMI1(false),
              hasBMI2(false),
              hasADX(false),
              hasVMX(false),
              hasSVM(false) {
            // Toutes les fonctionnalités étendues désactivées par défaut
            // Seront activées par DetectExtendedFeatures() si détectées
        }

        // ====================================================================
        // SECTION 3 : IMPLÉMENTATION DE CPUFeatures
        // ====================================================================

        // -------------------------------------------------------------------------
        // Constructeur principal (privé)
        // -------------------------------------------------------------------------
        NKENTSEU_NO_INLINE CPUFeatures::CPUFeatures()
            : vendor{0},
              brand{0},
              family(0),
              model(0),
              stepping(0),
              baseFrequency(0),
              maxFrequency(0) {
            // Appel séquentiel des fonctions de détection
            // L'ordre n'est pas critique car chaque fonction est indépendante
            DetectVendorAndBrand();
            DetectTopology();
            DetectCache();
            DetectSIMDFeatures();
            DetectExtendedFeatures();
            DetectFrequency();
        }

        // -------------------------------------------------------------------------
        // Singleton : obtention de l'instance
        // -------------------------------------------------------------------------
        NKENTSEU_NO_INLINE const CPUFeatures &CPUFeatures::Get() {
            // Initialisation statique locale : thread-safe en C++11+
            // La détection n'est effectuée qu'une seule fois, au premier appel
            static CPUFeatures instance;
            return instance;
        }

        // ====================================================================
        // SECTION 4 : HELPER CPUID (X86/X64 UNIQUEMENT)
        // ====================================================================

        #if defined(NKENTSEU_ARCH_X86) || defined(NKENTSEU_ARCH_X86_64)
            /**
             * @brief Exécuter l'instruction CPUID de manière portable
             * @param function Numéro de leaf CPUID à exécuter
             * @param subfunction Numéro de sub-leaf (pour CPUID étendu)
             * @param eax Pointeur vers registre EAX de sortie
             * @param ebx Pointeur vers registre EBX de sortie
             * @param ecx Pointeur vers registre ECX de sortie
             * @param edx Pointeur vers registre EDX de sortie
             *
             * Abstraction des différences entre MSVC (__cpuidex) et GCC/Clang (inline asm).
             * Les registres de sortie sont copiés dans les pointeurs fournis.
             */
            NKENTSEU_API_INLINE void CPUFeatures::CPUID(
                int32_t function,
                int32_t subfunction,
                int32_t *eax,
                int32_t *ebx,
                int32_t *ecx,
                int32_t *edx
            ) {
                int32_t regs[4] = {0};

                #if defined(NKENTSEU_COMPILER_MSVC)
                    // MSVC : utiliser l'intrinsèque __cpuidex
                    __cpuidex(regs, function, subfunction);
                #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
                    // GCC/Clang : inline assembly avec contraintes de registres
                    __asm__ __volatile__(
                        "cpuid"
                        : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
                        : "a"(function), "c"(subfunction)
                    );
                #endif

                // Copier les résultats dans les pointeurs de sortie
                *eax = regs[0];
                *ebx = regs[1];
                *ecx = regs[2];
                *edx = regs[3];
            }
        #endif

        // ====================================================================
        // SECTION 5 : DÉTECTION DU VENDOR ET DU BRAND STRING
        // ====================================================================

        NKENTSEU_NO_INLINE void CPUFeatures::DetectVendorAndBrand() {
            #if defined(NKENTSEU_ARCH_X86) || defined(NKENTSEU_ARCH_X86_64)
                // -----------------------------------------------------------------
                // x86/x64 : utilisation de CPUID pour obtenir vendor et brand
                // -----------------------------------------------------------------

                int32_t eax = 0;
                int32_t ebx = 0;
                int32_t ecx = 0;
                int32_t edx = 0;

                char vendorStr[13] = {0};

                // CPUID leaf 0 : retourne le vendor string en EBX:EDX:ECX
                CPUID(0, 0, &eax, &ebx, &ecx, &edx);
                ::memcpy(vendorStr + 0, &ebx, 4);
                ::memcpy(vendorStr + 4, &edx, 4);
                ::memcpy(vendorStr + 8, &ecx, 4);
                vendorStr[12] = '\0';

                CopyStringTruncated(vendor, VENDOR_CAPACITY, vendorStr);

                // CPUID leaves 0x80000002-0x80000004 : brand string (48 octets + null)
                char brandStr[49] = {0};
                int32_t maxExtended = 0;

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

                // CPUID leaf 1 : family/model/stepping
                CPUID(1, 0, &eax, &ebx, &ecx, &edx);
                stepping = eax & 0xF;
                model = (eax >> 4) & 0xF;
                family = (eax >> 8) & 0xF;

                // Extended family/model pour les CPU modernes
                int32_t extModel = (eax >> 16) & 0xF;
                int32_t extFamily = (eax >> 20) & 0xFF;

                if (family == 0xF) {
                    family += extFamily;
                }
                if (family == 0x6 || family == 0xF) {
                    model += (extModel << 4);
                }

            #elif defined(NKENTSEU_ARCH_ARM) || defined(NKENTSEU_ARCH_ARM64)
                // -----------------------------------------------------------------
                // ARM/ARM64 : détection via /proc/cpuinfo (Android/Linux)
                // -----------------------------------------------------------------

                CopyStringTruncated(vendor, VENDOR_CAPACITY, "ARM");

                #if defined(NKENTSEU_PLATFORM_ANDROID)
                    // Sur Android, parser /proc/cpuinfo pour obtenir le hardware name
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

                // Fallback si la détection a échoué
                if (brand[0] == '\0') {
                    CopyStringTruncated(brand, BRAND_CAPACITY, "ARM Processor");
                }

            #else
                // -----------------------------------------------------------------
                // Autres architectures : valeurs par défaut
                // -----------------------------------------------------------------
                CopyStringTruncated(vendor, VENDOR_CAPACITY, "Unknown");
                CopyStringTruncated(brand, BRAND_CAPACITY, "Unknown Processor");
            #endif
        }

        // ====================================================================
        // SECTION 6 : DÉTECTION DE LA TOPOLOGIE CPU
        // ====================================================================

        NKENTSEU_NO_INLINE void CPUFeatures::DetectTopology() {
            // Initialisation avec des valeurs conservatrices
            topology.numLogicalCores = 1;
            topology.numPhysicalCores = 1;

            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                // -----------------------------------------------------------------
                // Windows : utilisation de GetSystemInfo et GetLogicalProcessorInformation
                // -----------------------------------------------------------------

                SYSTEM_INFO sysInfo;
                GetSystemInfo(&sysInfo);
                topology.numLogicalCores = static_cast<int32_t>(sysInfo.dwNumberOfProcessors);

                // Obtenir le nombre de cœurs physiques via GetLogicalProcessorInformation
                DWORD length = 0;
                GetLogicalProcessorInformation(nullptr, &length);

                if (length > 0) {
                    auto *buffer = new SYSTEM_LOGICAL_PROCESSOR_INFORMATION[
                        length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION)
                    ];

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

            #elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_ANDROID)
                // -----------------------------------------------------------------
                // Linux/Android : utilisation de sysconf et parsing de /sys
                // -----------------------------------------------------------------

                // Obtenir le nombre de processeurs en ligne via sysconf
                long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
                if (nprocs > 0) {
                    topology.numLogicalCores = static_cast<int32_t>(nprocs);
                }

                // Simplification : supposer que physical = logical
                // Pour une détection plus précise, parser /sys/devices/system/cpu/
                topology.numPhysicalCores = topology.numLogicalCores;

            #elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
                // -----------------------------------------------------------------
                // macOS/iOS : utilisation de sysctlbyname
                // -----------------------------------------------------------------

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
                // -----------------------------------------------------------------
                // Plateformes non supportées : fallback conservateur
                // -----------------------------------------------------------------
                topology.numPhysicalCores = topology.numLogicalCores;
            #endif

            // Détection de l'Hyper-Threading / SMT
            topology.hasHyperThreading = (topology.numLogicalCores > topology.numPhysicalCores);

            // Par défaut, supposer un seul socket (systèmes grand public)
            topology.numSockets = 1;
        }

        // ====================================================================
        // SECTION 7 : DÉTECTION DES INFORMATIONS DE CACHE
        // ====================================================================

        NKENTSEU_NO_INLINE void CPUFeatures::DetectCache() {
            #if defined(NKENTSEU_ARCH_X86) || defined(NKENTSEU_ARCH_X86_64)
                // -----------------------------------------------------------------
                // x86/x64 : détection via CPUID
                // -----------------------------------------------------------------

                int32_t eax = 0;
                int32_t ebx = 0;
                int32_t ecx = 0;
                int32_t edx = 0;

                // -----------------------------------------------------------------
                // Détection Intel : CPUID leaf 4 (cache parameters)
                // -----------------------------------------------------------------
                if (ContainsSubString(vendor, "Intel")) {
                    for (int32_t i = 0; i < 16; ++i) {
                        CPUID(4, i, &eax, &ebx, &ecx, &edx);

                        int32_t cacheType = eax & 0x1F;
                        if (cacheType == 0) {
                            break; // Plus de caches à énumérer
                        }

                        int32_t cacheLevel = (eax >> 5) & 0x7;
                        int32_t sets = ecx + 1;
                        int32_t lineSize = (ebx & 0xFFF) + 1;
                        int32_t partitions = ((ebx >> 12) & 0x3FF) + 1;
                        int32_t ways = ((ebx >> 22) & 0x3FF) + 1;
                        int32_t size = (ways * partitions * lineSize * sets) / 1024;

                        // Mise à jour de la taille de ligne de cache
                        cache.lineSize = lineSize;

                        // Classification par niveau et type de cache
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
                // -----------------------------------------------------------------
                // Détection AMD : CPUID leaf 0x80000006
                // -----------------------------------------------------------------
                else if (ContainsSubString(vendor, "AMD")) {
                    CPUID(0x80000006, 0, &eax, &ebx, &ecx, &edx);

                    // L1/L2 cache info dans ECX
                    cache.lineSize = ecx & 0xFF;
                    cache.l2Size = (ecx >> 16) & 0xFFFF;

                    // L3 cache info dans EDX (en unités de 512 KB)
                    CPUID(0x80000006, 0, &eax, &ebx, &ecx, &edx);
                    cache.l3Size = ((edx >> 18) & 0x3FFF) * 512;
                }

            #elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
                // -----------------------------------------------------------------
                // macOS/iOS : utilisation de sysctlbyname pour les informations de cache
                // -----------------------------------------------------------------

                size_t len = sizeof(int32_t);
                int32_t value = 0;

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
                // -----------------------------------------------------------------
                // Plateformes non supportées : conserver les valeurs par défaut
                // -----------------------------------------------------------------
                // Les valeurs par défaut ont été initialisées dans le constructeur CacheInfo
            #endif
        }

        // ====================================================================
        // SECTION 8 : DÉTECTION DES FONCTIONNALITÉS SIMD
        // ====================================================================

        NKENTSEU_NO_INLINE void CPUFeatures::DetectSIMDFeatures() {
            #if defined(NKENTSEU_ARCH_X86) || defined(NKENTSEU_ARCH_X86_64)
                // -----------------------------------------------------------------
                // x86/x64 : détection via CPUID leaves 1 et 7
                // -----------------------------------------------------------------

                int32_t eax = 0;
                int32_t ebx = 0;
                int32_t ecx = 0;
                int32_t edx = 0;

                // -----------------------------------------------------------------
                // CPUID(1) : fonctionnalités de base (ECX/EDX)
                // -----------------------------------------------------------------
                CPUID(1, 0, &eax, &ebx, &ecx, &edx);

                // Bits EDX : fonctionnalités legacy
                simd.hasMMX = (edx & (1 << 23)) != 0;
                simd.hasSSE = (edx & (1 << 25)) != 0;
                simd.hasSSE2 = (edx & (1 << 26)) != 0;

                // Bits ECX : fonctionnalités modernes
                simd.hasSSE3 = (ecx & (1 << 0)) != 0;
                simd.hasSSSE3 = (ecx & (1 << 9)) != 0;
                simd.hasSSE41 = (ecx & (1 << 19)) != 0;
                simd.hasSSE42 = (ecx & (1 << 20)) != 0;
                simd.hasAVX = (ecx & (1 << 28)) != 0;
                simd.hasFMA = (ecx & (1 << 12)) != 0;

                // -----------------------------------------------------------------
                // CPUID(7,0) : fonctionnalités étendues (EBX/ECX/EDX)
                // -----------------------------------------------------------------
                CPUID(7, 0, &eax, &ebx, &ecx, &edx);

                simd.hasAVX2 = (ebx & (1 << 5)) != 0;
                simd.hasAVX512F = (ebx & (1 << 16)) != 0;
                simd.hasAVX512DQ = (ebx & (1 << 17)) != 0;
                simd.hasAVX512BW = (ebx & (1 << 30)) != 0;
                simd.hasAVX512VL = (ebx & (1 << 31)) != 0;

                // -----------------------------------------------------------------
                // Détection spécifique AMD : FMA4 via CPUID(0x80000001)
                // -----------------------------------------------------------------
                if (ContainsSubString(vendor, "AMD")) {
                    CPUID(0x80000001, 0, &eax, &ebx, &ecx, &edx);
                    simd.hasFMA4 = (ecx & (1 << 16)) != 0;
                }

            #elif defined(NKENTSEU_ARCH_ARM64)
                // -----------------------------------------------------------------
                // ARM64 : NEON est toujours présent, détection de SVE via /proc/cpuinfo
                // -----------------------------------------------------------------

                // ARMv8-A garantit la présence de NEON
                simd.hasNEON = true;

                #if defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_ANDROID)
                    // Parser /proc/cpuinfo pour détecter SVE/SVE2
                    FILE *f = fopen("/proc/cpuinfo", "r");
                    if (f) {
                        char line[256];
                        while (fgets(line, sizeof(line), f)) {
                            if (strstr(line, "sve") != nullptr) {
                                simd.hasSVE = true;
                            }
                            if (strstr(line, "sve2") != nullptr) {
                                simd.hasSVE2 = true;
                            }
                        }
                        fclose(f);
                    }
                #endif

            #elif defined(NKENTSEU_ARCH_ARM)
                // -----------------------------------------------------------------
                // ARM 32-bit : NEON est optionnel, détecté via macro de compilation
                // -----------------------------------------------------------------

                #ifdef __ARM_NEON
                    simd.hasNEON = true;
                #endif

            #endif
        }

        // ====================================================================
        // SECTION 9 : DÉTECTION DES FONCTIONNALITÉS ÉTENDUES
        // ====================================================================

        NKENTSEU_NO_INLINE void CPUFeatures::DetectExtendedFeatures() {
            #if defined(NKENTSEU_ARCH_X86) || defined(NKENTSEU_ARCH_X86_64)
                // -----------------------------------------------------------------
                // x86/x64 : détection via CPUID leaves 1, 7, et 0x80000001
                // -----------------------------------------------------------------

                int32_t eax = 0;
                int32_t ebx = 0;
                int32_t ecx = 0;
                int32_t edx = 0;

                // -----------------------------------------------------------------
                // CPUID(1) : fonctionnalités de sécurité et mémoire
                // -----------------------------------------------------------------
                CPUID(1, 0, &eax, &ebx, &ecx, &edx);

                extended.hasCLFLUSH = (edx & (1 << 19)) != 0;
                extended.hasMOVBE = (ecx & (1 << 22)) != 0;
                extended.hasPOPCNT = (ecx & (1 << 23)) != 0;
                extended.hasAES = (ecx & (1 << 25)) != 0;
                extended.hasRDRAND = (ecx & (1 << 30)) != 0;

                // -----------------------------------------------------------------
                // CPUID(7,0) : fonctionnalités de performance et chiffrement
                // -----------------------------------------------------------------
                CPUID(7, 0, &eax, &ebx, &ecx, &edx);

                extended.hasBMI1 = (ebx & (1 << 3)) != 0;
                extended.hasBMI2 = (ebx & (1 << 8)) != 0;
                extended.hasADX = (ebx & (1 << 19)) != 0;
                extended.hasSHA = (ebx & (1 << 29)) != 0;
                extended.hasCLFLUSHOPT = (ebx & (1 << 23)) != 0;
                extended.hasPREFETCHWT1 = (ecx & (1 << 0)) != 0;
                extended.hasRDSEED = (ebx & (1 << 18)) != 0;

                // -----------------------------------------------------------------
                // CPUID(0x80000001) : fonctionnalités AMD/extended
                // -----------------------------------------------------------------
                CPUID(0x80000001, 0, &eax, &ebx, &ecx, &edx);
                extended.hasLZCNT = (ecx & (1 << 5)) != 0;

                // -----------------------------------------------------------------
                // Virtualisation : détection via CPUID(1) ECX bit 5 (VMX) ou bit 2 (SVM)
                // -----------------------------------------------------------------
                // Note : la détection précise de la virtualisation nécessite des checks
                // supplémentaires (MSR, privilèges), cette détection est indicative.
                extended.hasVMX = (ecx & (1 << 5)) != 0;
                // SVM est détecté via CPUID(0x80000001) ECX bit 2
                CPUID(0x80000001, 0, &eax, &ebx, &ecx, &edx);
                extended.hasSVM = (ecx & (1 << 2)) != 0;

            #endif
            // Pour ARM et autres architectures, les fonctionnalités étendues
            // peuvent être détectées via des APIs spécifiques (non implémentées ici)
        }

        // ====================================================================
        // SECTION 10 : DÉTECTION DE LA FRÉQUENCE CPU
        // ====================================================================

        NKENTSEU_NO_INLINE void CPUFeatures::DetectFrequency() {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                // -----------------------------------------------------------------
                // Windows : lecture du registre ~MHz via RegOpenKeyEx
                // -----------------------------------------------------------------

                HKEY hKey;
                const char* processorKey = "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";

                if (RegOpenKeyExA(
                        HKEY_LOCAL_MACHINE,
                        processorKey,
                        0,
                        KEY_READ,
                        &hKey
                    ) == ERROR_SUCCESS) {

                    DWORD mhz = 0;
                    DWORD size = sizeof(mhz);

                    if (RegQueryValueExA(hKey, "~MHz", nullptr, nullptr, (LPBYTE)&mhz, &size) == ERROR_SUCCESS) {
                        baseFrequency = static_cast<int32_t>(mhz);
                        maxFrequency = baseFrequency; // Turbo non détecté via cette méthode
                    }

                    RegCloseKey(hKey);
                }

            #elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
                // -----------------------------------------------------------------
                // macOS/iOS : sysctlbyname("hw.cpufrequency")
                // -----------------------------------------------------------------

                uint64_t freq = 0;
                size_t len = sizeof(freq);

                if (sysctlbyname("hw.cpufrequency", &freq, &len, nullptr, 0) == 0) {
                    // Conversion de Hz vers MHz
                    baseFrequency = static_cast<int32_t>(freq / 1000000);
                    maxFrequency = baseFrequency;
                }

            #elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_ANDROID)
                // -----------------------------------------------------------------
                // Linux/Android : parsing de /proc/cpuinfo pour "cpu MHz"
                // -----------------------------------------------------------------

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

            #else
                // -----------------------------------------------------------------
                // Plateformes non supportées : fréquences à 0 (non détectées)
                // -----------------------------------------------------------------
                // baseFrequency et maxFrequency restent à 0 (valeur par défaut)
            #endif
        }

        // ====================================================================
        // SECTION 11 : MÉTHODES TOSTRING (SÉRIALISATION)
        // ====================================================================

        NKENTSEU_API_INLINE void CPUFeatures::ToString(
            char* outBuffer,
            size_t outBufferSize
        ) const {
            if (!outBuffer || outBufferSize == 0u) {
                return;
            }

            outBuffer[0] = '\0';

            // Buffer temporaire pour construire la chaîne
            char buffer[2048];
            int offset = 0;

            // En-tête général
            offset += snprintf(
                buffer + offset,
                sizeof(buffer) - static_cast<size_t>(offset),
                "CPU Information:\n"
            );
            offset += snprintf(
                buffer + offset,
                sizeof(buffer) - static_cast<size_t>(offset),
                "  Vendor: %s\n",
                vendor
            );
            offset += snprintf(
                buffer + offset,
                sizeof(buffer) - static_cast<size_t>(offset),
                "  Brand: %s\n",
                brand
            );
            offset += snprintf(
                buffer + offset,
                sizeof(buffer) - static_cast<size_t>(offset),
                "  Family: %d, Model: %d, Stepping: %d\n",
                family,
                model,
                stepping
            );
            offset += snprintf(
                buffer + offset,
                sizeof(buffer) - static_cast<size_t>(offset),
                "  Base Frequency: %d MHz, Max: %d MHz\n",
                baseFrequency,
                maxFrequency
            );

            // Section Topologie
            offset += snprintf(
                buffer + offset,
                sizeof(buffer) - static_cast<size_t>(offset),
                "\nTopology:\n"
            );
            offset += snprintf(
                buffer + offset,
                sizeof(buffer) - static_cast<size_t>(offset),
                "  Physical Cores: %d\n",
                topology.numPhysicalCores
            );
            offset += snprintf(
                buffer + offset,
                sizeof(buffer) - static_cast<size_t>(offset),
                "  Logical Cores: %d\n",
                topology.numLogicalCores
            );
            offset += snprintf(
                buffer + offset,
                sizeof(buffer) - static_cast<size_t>(offset),
                "  Hyper-Threading: %s\n",
                topology.hasHyperThreading ? "Yes" : "No"
            );

            // Section Cache
            offset += snprintf(
                buffer + offset,
                sizeof(buffer) - static_cast<size_t>(offset),
                "\nCache:\n"
            );
            offset += snprintf(
                buffer + offset,
                sizeof(buffer) - static_cast<size_t>(offset),
                "  Line Size: %d bytes\n",
                cache.lineSize
            );
            offset += snprintf(
                buffer + offset,
                sizeof(buffer) - static_cast<size_t>(offset),
                "  L1 Data: %d KB, L1 Instruction: %d KB\n",
                cache.l1DataSize,
                cache.l1InstructionSize
            );
            offset += snprintf(
                buffer + offset,
                sizeof(buffer) - static_cast<size_t>(offset),
                "  L2: %d KB, L3: %d KB\n",
                cache.l2Size,
                cache.l3Size
            );

            // Section SIMD Features
            offset += snprintf(
                buffer + offset,
                sizeof(buffer) - static_cast<size_t>(offset),
                "\nSIMD Features:\n"
            );
            if (simd.hasSSE) {
                offset += snprintf(
                    buffer + offset,
                    sizeof(buffer) - static_cast<size_t>(offset),
                    "  SSE "
                );
            }
            if (simd.hasSSE2) {
                offset += snprintf(
                    buffer + offset,
                    sizeof(buffer) - static_cast<size_t>(offset),
                    "SSE2 "
                );
            }
            if (simd.hasSSE3) {
                offset += snprintf(
                    buffer + offset,
                    sizeof(buffer) - static_cast<size_t>(offset),
                    "SSE3 "
                );
            }
            if (simd.hasSSE41) {
                offset += snprintf(
                    buffer + offset,
                    sizeof(buffer) - static_cast<size_t>(offset),
                    "SSE4.1 "
                );
            }
            if (simd.hasSSE42) {
                offset += snprintf(
                    buffer + offset,
                    sizeof(buffer) - static_cast<size_t>(offset),
                    "SSE4.2 "
                );
            }
            if (simd.hasAVX) {
                offset += snprintf(
                    buffer + offset,
                    sizeof(buffer) - static_cast<size_t>(offset),
                    "AVX "
                );
            }
            if (simd.hasAVX2) {
                offset += snprintf(
                    buffer + offset,
                    sizeof(buffer) - static_cast<size_t>(offset),
                    "AVX2 "
                );
            }
            if (simd.hasAVX512F) {
                offset += snprintf(
                    buffer + offset,
                    sizeof(buffer) - static_cast<size_t>(offset),
                    "AVX-512F "
                );
            }
            if (simd.hasFMA) {
                offset += snprintf(
                    buffer + offset,
                    sizeof(buffer) - static_cast<size_t>(offset),
                    "FMA "
                );
            }
            if (simd.hasNEON) {
                offset += snprintf(
                    buffer + offset,
                    sizeof(buffer) - static_cast<size_t>(offset),
                    "NEON "
                );
            }
            offset += snprintf(
                buffer + offset,
                sizeof(buffer) - static_cast<size_t>(offset),
                "\n"
            );

            // Copie finale dans le buffer de sortie avec vérification de taille
            ::snprintf(
                outBuffer,
                static_cast<size_t>(outBufferSize),
                "%s",
                buffer
            );
        }

        NKENTSEU_API_INLINE const char* CPUFeatures::ToString() const {
            // Buffer statique interne : NON THREAD-SAFE
            // Ne pas conserver le pointeur retourné entre les appels
            static char sBuffer[2048];
            ToString(sBuffer, sizeof(sBuffer));
            return sBuffer;
        }

    } // namespace platform

} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================