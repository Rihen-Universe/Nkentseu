// =============================================================================
// NKPlatform/NkCPUFeatures.h
// Détection avancée des fonctionnalités CPU (SIMD, cache, topologie, performance).
//
// Design :
//  - Détection runtime des extensions SIMD (SSE, AVX, NEON, SVE, etc.)
//  - Informations sur la topologie CPU (cœurs physiques/logiques, HT/SMT)
//  - Informations sur les caches (L1/L2/L3, taille de ligne)
//  - Fonctionnalités étendues (AES, SHA, RDRAND, virtualisation, etc.)
//  - Singleton thread-safe avec initialisation paresseuse
//  - API ergonomique avec macros de convenance pour les checks courants
//  - Compatible x86/x64, ARM/ARM64, avec fallback portable
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_PLATFORM_NKCPUFEATURES_H
#define NKENTSEU_PLATFORM_NKCPUFEATURES_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules de détection d'architecture et des macros d'export.
    // NkArchDetect.h fournit les macros NKENTSEU_ARCH_* pour la détection CPU.
    // NkPlatformExport.h fournit NKENTSEU_PLATFORM_API pour l'export de symboles.
    // NkPlatformInline.h fournit NKENTSEU_API_INLINE pour les fonctions inline exportées.

    #include "NKPlatform/NkArchDetect.h"
    #include "NKPlatform/NkPlatformExport.h"
    #include "NKPlatform/NkPlatformInline.h"

    #include <stddef.h>
    #include <stdint.h>

    // =========================================================================
    // SECTION 2 : ESPACE DE NOMS PRINCIPAL (INDENTATION HIÉRARCHIQUE)
    // =========================================================================
    // Déclaration de l'espace de noms principal nkentseu::platform.
    // Les symboles publics sont exportés via NKENTSEU_PLATFORM_API.
    //
    // NOTE SUR L'INDENTATION :
    //  - Le contenu de 'nkentseu' est indenté d'un niveau
    //  - Le contenu de 'platform' (dans 'nkentseu') est indenté de deux niveaux

    namespace nkentseu {

        namespace platform {

            // =================================================================
            // SECTION 3 : STRUCTURES DE DONNÉES PUBLIQUES
            // =================================================================
            // Structures contenant les informations détectées sur le CPU.
            // Toutes les classes/structs publiques utilisent NKENTSEU_CLASS_EXPORT
            // pour garantir l'export/import correct dans les bibliothèques partagées.

            // -----------------------------------------------------------------
            // Sous-section : Informations sur les caches CPU
            // -----------------------------------------------------------------

            /**
             * @brief Informations sur la hiérarchie de cache du CPU
             * @struct CacheInfo
             * @ingroup CPUFeatures
             *
             * Contient les tailles et caractéristiques des différents niveaux
             * de cache du processeur. Les valeurs sont en kilo-octets (KB)
             * sauf pour lineSize qui est en octets.
             *
             * @note Les valeurs par défaut sont conservatrices et peuvent être
             * mises à jour par la détection runtime si disponible.
             */
            struct NKENTSEU_CLASS_EXPORT CacheInfo {
                /**
                 * @brief Taille d'une ligne de cache en octets
                 * @value Typiquement 64 pour les CPU modernes
                 */
                int32_t lineSize;

                /**
                 * @brief Taille du cache L1 données en KB
                 */
                int32_t l1DataSize;

                /**
                 * @brief Taille du cache L1 instructions en KB
                 */
                int32_t l1InstructionSize;

                /**
                 * @brief Taille du cache L2 en KB
                 */
                int32_t l2Size;

                /**
                 * @brief Taille du cache L3 en KB
                 */
                int32_t l3Size;

                /**
                 * @brief Constructeur par défaut avec valeurs conservatrices
                 */
                NKENTSEU_INLINE CacheInfo();
            };

            // -----------------------------------------------------------------
            // Sous-section : Informations sur la topologie CPU
            // -----------------------------------------------------------------

            /**
             * @brief Informations sur la topologie du processeur
             * @struct CPUTopology
             * @ingroup CPUFeatures
             *
             * Décrit l'organisation physique et logique des cœurs du CPU,
             * incluant le support de l'Hyper-Threading/SMT.
             */
            struct NKENTSEU_CLASS_EXPORT CPUTopology {
                /**
                 * @brief Nombre de cœurs physiques réels
                 */
                int32_t numPhysicalCores;

                /**
                 * @brief Nombre de cœurs logiques (avec HT/SMT activé)
                 */
                int32_t numLogicalCores;

                /**
                 * @brief Nombre de sockets CPU dans le système
                 */
                int32_t numSockets;

                /**
                 * @brief Indique si l'Hyper-Threading/SMT est activé
                 */
                bool hasHyperThreading;

                /**
                 * @brief Constructeur par défaut avec valeurs minimales
                 */
                NKENTSEU_INLINE CPUTopology();
            };

            // -----------------------------------------------------------------
            // Sous-section : Fonctionnalités SIMD supportées
            // -----------------------------------------------------------------

            /**
             * @brief Détection des extensions SIMD disponibles
             * @struct SIMDFeatures
             * @ingroup CPUFeatures
             *
             * Liste exhaustive des jeux d'instructions vectoriels supportés
             * par le CPU. Utilisé pour sélectionner le chemin de code optimal
             * à l'exécution (dispatch runtime).
             *
             * @example
             * @code
             * const auto& cpu = CPUFeatures::Get();
             * if (cpu.simd.hasAVX2) {
             *     ProcessDataAVX2(data);
             * } else if (cpu.simd.hasSSE42) {
             *     ProcessDataSSE42(data);
             * } else {
             *     ProcessDataScalar(data);
             * }
             * @endcode
             */
            struct NKENTSEU_CLASS_EXPORT SIMDFeatures {
                // -----------------------------------------------------------------
                // Fonctionnalités x86/x64
                // -----------------------------------------------------------------

                /** @brief Support de MMX (MultiMedia eXtensions) */
                bool hasMMX;

                /** @brief Support de SSE (Streaming SIMD Extensions) */
                bool hasSSE;

                /** @brief Support de SSE2 */
                bool hasSSE2;

                /** @brief Support de SSE3 */
                bool hasSSE3;

                /** @brief Support de SSSE3 (Supplemental SSE3) */
                bool hasSSSE3;

                /** @brief Support de SSE4.1 */
                bool hasSSE41;

                /** @brief Support de SSE4.2 */
                bool hasSSE42;

                /** @brief Support de AVX (Advanced Vector Extensions) */
                bool hasAVX;

                /** @brief Support de AVX2 */
                bool hasAVX2;

                /** @brief Support de AVX-512 Foundation */
                bool hasAVX512F;

                /** @brief Support de AVX-512 Doubleword & Quadword */
                bool hasAVX512DQ;

                /** @brief Support de AVX-512 Byte & Word */
                bool hasAVX512BW;

                /** @brief Support de AVX-512 Vector Length */
                bool hasAVX512VL;

                /** @brief Support de FMA (Fused Multiply-Add) */
                bool hasFMA;

                /** @brief Support de FMA4 (AMD-specific) */
                bool hasFMA4;

                // -----------------------------------------------------------------
                // Fonctionnalités ARM
                // -----------------------------------------------------------------

                /** @brief Support de NEON (ARM SIMD) */
                bool hasNEON;

                /** @brief Support de SVE (Scalable Vector Extension) */
                bool hasSVE;

                /** @brief Support de SVE2 */
                bool hasSVE2;

                /**
                 * @brief Constructeur par défaut avec toutes les fonctionnalités désactivées
                 */
                NKENTSEU_INLINE SIMDFeatures();
            };

            // -----------------------------------------------------------------
            // Sous-section : Fonctionnalités CPU étendues
            // -----------------------------------------------------------------

            /**
             * @brief Fonctionnalités CPU supplémentaires (sécurité, mémoire, performance)
             * @struct ExtendedFeatures
             * @ingroup CPUFeatures
             *
             * Contient les détections pour les extensions spécialisées :
             * chiffrement matériel, manipulation de bits, virtualisation, etc.
             */
            struct NKENTSEU_CLASS_EXPORT ExtendedFeatures {
                // -----------------------------------------------------------------
                // Sécurité et chiffrement
                // -----------------------------------------------------------------

                /** @brief Support de AES-NI (chiffrement AES matériel) */
                bool hasAES;

                /** @brief Support des extensions SHA matérielles */
                bool hasSHA;

                /** @brief Support de RDRAND (générateur aléatoire matériel) */
                bool hasRDRAND;

                /** @brief Support de RDSEED (graine pour PRNG) */
                bool hasRDSEED;

                // -----------------------------------------------------------------
                // Optimisations mémoire
                // -----------------------------------------------------------------

                /** @brief Support de CLFLUSH (flush de ligne de cache) */
                bool hasCLFLUSH;

                /** @brief Support de CLFLUSHOPT (version optimisée) */
                bool hasCLFLUSHOPT;

                /** @brief Support de PREFETCHWT1 (préchargement vers L2) */
                bool hasPREFETCHWT1;

                /** @brief Support de MOVBE (move avec swap d'octets) */
                bool hasMOVBE;

                // -----------------------------------------------------------------
                // Instructions de performance
                // -----------------------------------------------------------------

                /** @brief Support de POPCNT (comptage de bits à 1) */
                bool hasPOPCNT;

                /** @brief Support de LZCNT (comptage de zéros en tête) */
                bool hasLZCNT;

                /** @brief Support de BMI1 (Bit Manipulation Instruction Set 1) */
                bool hasBMI1;

                /** @brief Support de BMI2 (Bit Manipulation Instruction Set 2) */
                bool hasBMI2;

                /** @brief Support de ADX (addition multi-précision avec carry) */
                bool hasADX;

                // -----------------------------------------------------------------
                // Virtualisation
                // -----------------------------------------------------------------

                /** @brief Support de VMX (Intel VT-x) */
                bool hasVMX;

                /** @brief Support de SVM (AMD-V) */
                bool hasSVM;

                /**
                 * @brief Constructeur par défaut avec toutes les fonctionnalités désactivées
                 */
                NKENTSEU_INLINE ExtendedFeatures();
            };

            // =================================================================
            // SECTION 4 : CLASSE PRINCIPALE CPUFeatures
            // =================================================================

            /**
             * @brief Structure complète des fonctionnalités CPU détectées
             * @struct CPUFeatures
             * @ingroup CPUFeatures
             *
             * Singleton thread-safe qui détecte et stocke toutes les capacités
             * du processeur hôte. Utilise CPUID sur x86/x64 et les APIs système
             * sur ARM/Linux/macOS/Windows.
             *
             * @note La détection est effectuée une seule fois à la première appel
             * de Get(), puis les résultats sont mis en cache pour les appels suivants.
             *
             * @example
             * @code
             * // Obtenir l'instance singleton
             * const auto& cpu = nkentseu::platform::CPUFeatures::Get();
             *
             * // Vérifier une fonctionnalité SIMD
             * if (cpu.simd.hasAVX2) {
             *     // Utiliser le chemin optimisé AVX2
             *     ProcessVectorized(data);
             * }
             *
             * // Obtenir des informations de topologie
             * int cores = cpu.topology.numPhysicalCores;
             * int threads = cpu.topology.numLogicalCores;
             *
             * // Afficher un résumé lisible
             * NK_FOUNDATION_LOG_INFO("CPU: %s", cpu.ToString());
             * @endcode
             */
            struct NKENTSEU_CLASS_EXPORT CPUFeatures {
                // -----------------------------------------------------------------
                // Constantes de capacité pour les chaînes
                // -----------------------------------------------------------------

                /** @brief Capacité maximale pour la chaîne vendor */
                static constexpr size_t VENDOR_CAPACITY = 32u;

                /** @brief Capacité maximale pour la chaîne brand */
                static constexpr size_t BRAND_CAPACITY = 128u;

                // -----------------------------------------------------------------
                // Informations d'identification du CPU
                // -----------------------------------------------------------------

                /**
                 * @brief Chaîne d'identification du vendor
                 * @value Exemples : "GenuineIntel", "AuthenticAMD", "ARM"
                 */
                char vendor[VENDOR_CAPACITY];

                /**
                 * @brief Nom complet du processeur (brand string)
                 * @value Exemple : "Intel(R) Core(TM) i7-9700K CPU @ 3.60GHz"
                 */
                char brand[BRAND_CAPACITY];

                /** @brief Numéro de famille du CPU (CPUID) */
                int32_t family;

                /** @brief Numéro de modèle du CPU (CPUID) */
                int32_t model;

                /** @brief Numéro de stepping du CPU (CPUID) */
                int32_t stepping;

                // -----------------------------------------------------------------
                // Sous-structures de fonctionnalités
                // -----------------------------------------------------------------

                /** @brief Informations de topologie */
                CPUTopology topology;

                /** @brief Informations de cache */
                CacheInfo cache;

                /** @brief Fonctionnalités SIMD */
                SIMDFeatures simd;

                /** @brief Fonctionnalités étendues */
                ExtendedFeatures extended;

                // -----------------------------------------------------------------
                // Informations de fréquence
                // -----------------------------------------------------------------

                /**
                 * @brief Fréquence de base en MHz
                 * @note Peut être 0 si la détection échoue
                 */
                int32_t baseFrequency;

                /**
                 * @brief Fréquence maximale en MHz (Turbo)
                 * @note Peut être égal à baseFrequency si non détecté
                 */
                int32_t maxFrequency;

                // -----------------------------------------------------------------
                // API publique statique
                // -----------------------------------------------------------------

                /**
                 * @brief Obtenir l'instance singleton (initialisation paresseuse thread-safe)
                 * @return Référence constante vers l'instance CPUFeatures
                 * @ingroup CPUFeaturesAPI
                 *
                 * @note Thread-safe en C++11+ grâce à l'initialisation statique locale.
                 * La détection n'est effectuée qu'une seule fois, au premier appel.
                 */
                NKENTSEU_NO_INLINE static const CPUFeatures &Get();

                // -----------------------------------------------------------------
                // Méthodes d'affichage et de sérialisation
                // -----------------------------------------------------------------

                /**
                 * @brief Formater les informations CPU dans un buffer fourni par l'appelant
                 * @param outBuffer Buffer de destination pour la chaîne formatée
                 * @param outBufferSize Taille du buffer en octets
                 * @ingroup CPUFeaturesAPI
                 *
                 * @note La sortie est tronquée si elle dépasse outBufferSize.
                 * Le buffer est toujours null-terminé.
                 */
                NKENTSEU_INLINE void ToString(
                    char* outBuffer,
                    size_t outBufferSize
                ) const;

                /**
                 * @brief Obtenir une représentation chaîne pratique (buffer statique interne)
                 * @return Pointeur vers une chaîne null-terminée contenant le résumé CPU
                 * @ingroup CPUFeaturesAPI
                 *
                 * @warning Non thread-safe : utilise un buffer statique interne.
                 * Ne pas conserver le pointeur retourné entre les appels.
                 *
                 * @example
                 * @code
                 * // Usage rapide pour logging
                 * NK_FOUNDATION_LOG_INFO("CPU: %s", CPUFeatures::Get().ToString());
                 * @endcode
                 */
                NKENTSEU_INLINE const char* ToString() const;

            private:
                // -----------------------------------------------------------------
                // Constructeur et méthodes privées de détection
                // -----------------------------------------------------------------

                /**
                 * @brief Constructeur privé (singleton pattern)
                 */
                CPUFeatures();

                /**
                 * @brief Détecter le vendor et le brand string du CPU
                 */
                void DetectVendorAndBrand();

                /**
                 * @brief Détecter la topologie (cœurs, threads, sockets)
                 */
                void DetectTopology();

                /**
                 * @brief Détecter les informations de cache
                 */
                void DetectCache();

                /**
                 * @brief Détecter les fonctionnalités SIMD supportées
                 */
                void DetectSIMDFeatures();

                /**
                 * @brief Détecter les fonctionnalités étendues
                 */
                void DetectExtendedFeatures();

                /**
                 * @brief Détecter les fréquences de fonctionnement
                 */
                void DetectFrequency();

                // -----------------------------------------------------------------
                // Helper x86/x64 uniquement
                // -----------------------------------------------------------------

                #if defined(NKENTSEU_ARCH_X86) || defined(NKENTSEU_ARCH_X86_64)
                    /**
                     * @brief Exécuter l'instruction CPUID (wrapper portable)
                     * @param function Numéro de leaf CPUID
                     * @param subfunction Numéro de sub-leaf (pour CPUID étendu)
                     * @param eax Pointeur vers registre EAX de sortie
                     * @param ebx Pointeur vers registre EBX de sortie
                     * @param ecx Pointeur vers registre ECX de sortie
                     * @param edx Pointeur vers registre EDX de sortie
                     * @ingroup CPUFeaturesInternals
                     */
                    void CPUID(
                        int32_t function,
                        int32_t subfunction,
                        int32_t *eax,
                        int32_t *ebx,
                        int32_t *ecx,
                        int32_t *edx
                    );
                #endif
            };

            // =================================================================
            // SECTION 5 : ALIASES ET API DE CONVENANCE (NOMENCLATURE NK)
            // =================================================================
            // Aliases pour compatibilité avec l'API existante utilisant le préfixe Nk.
            // Ces alias n'ajoutent pas de surcoût et sont résolus à la compilation.

            /** @brief Alias pour CacheInfo (compatibilité API Nk) */
            using NkCacheInfo = CacheInfo;

            /** @brief Alias pour CPUTopology (compatibilité API Nk) */
            using NkCPUTopology = CPUTopology;

            /** @brief Alias pour SIMDFeatures (compatibilité API Nk) */
            using NkSIMDFeatures = SIMDFeatures;

            /** @brief Alias pour ExtendedFeatures (compatibilité API Nk) */
            using NkExtendedFeatures = ExtendedFeatures;

            /** @brief Alias pour CPUFeatures (compatibilité API Nk) */
            using NkCPUFeatures = CPUFeatures;

            // -----------------------------------------------------------------
            // Fonctions inline de convenance (API Nk officielle)
            // -----------------------------------------------------------------
            // Ces fonctions fournissent un accès rapide aux fonctionnalités
            // couramment vérifiées, sans avoir à accéder aux structures internes.

            /**
             * @brief Obtenir l'instance CPUFeatures via l'alias Nk
             * @return Référence constante vers l'instance singleton
             * @ingroup CPUFeaturesAPI
             */
            NKENTSEU_API_INLINE const NkCPUFeatures &NkGetCPUFeatures() {
                return CPUFeatures::Get();
            }

            /**
             * @brief Vérifier le support de SSE2 à l'exécution
             * @return true si SSE2 est supporté, false sinon
             * @ingroup CPUFeaturesChecks
             */
            NKENTSEU_API_INLINE bool NkHasSSE2() {
                return CPUFeatures::Get().simd.hasSSE2;
            }

            /**
             * @brief Vérifier le support de AVX à l'exécution
             * @return true si AVX est supporté, false sinon
             * @ingroup CPUFeaturesChecks
             */
            NKENTSEU_API_INLINE bool NkHasAVX() {
                return CPUFeatures::Get().simd.hasAVX;
            }

            /**
             * @brief Vérifier le support de AVX2 à l'exécution
             * @return true si AVX2 est supporté, false sinon
             * @ingroup CPUFeaturesChecks
             */
            NKENTSEU_API_INLINE bool NkHasAVX2() {
                return CPUFeatures::Get().simd.hasAVX2;
            }

            /**
             * @brief Vérifier le support de AVX-512 à l'exécution
             * @return true si AVX-512 Foundation est supporté, false sinon
             * @ingroup CPUFeaturesChecks
             */
            NKENTSEU_API_INLINE bool NkHasAVX512() {
                return CPUFeatures::Get().simd.hasAVX512F;
            }

            /**
             * @brief Vérifier le support de NEON à l'exécution
             * @return true si NEON est supporté, false sinon
             * @ingroup CPUFeaturesChecks
             */
            NKENTSEU_API_INLINE bool NkHasNEON() {
                return CPUFeatures::Get().simd.hasNEON;
            }

            /**
             * @brief Vérifier le support de FMA à l'exécution
             * @return true si FMA est supporté, false sinon
             * @ingroup CPUFeaturesChecks
             */
            NKENTSEU_API_INLINE bool NkHasFMA() {
                return CPUFeatures::Get().simd.hasFMA;
            }

            /**
             * @brief Obtenir la taille de ligne de cache en octets
             * @return Taille en octets (typiquement 64)
             * @ingroup CPUFeaturesInfo
             */
            NKENTSEU_API_INLINE int32_t NkGetCacheLineSize() {
                return CPUFeatures::Get().cache.lineSize;
            }

            /**
             * @brief Obtenir le nombre de cœurs physiques
             * @return Nombre de cœurs physiques réels
             * @ingroup CPUFeaturesInfo
             */
            NKENTSEU_API_INLINE int32_t NkGetPhysicalCoreCount() {
                return CPUFeatures::Get().topology.numPhysicalCores;
            }

            /**
             * @brief Obtenir le nombre de cœurs logiques (threads)
             * @return Nombre de cœurs logiques (avec HT/SMT)
             * @ingroup CPUFeaturesInfo
             */
            NKENTSEU_API_INLINE int32_t NkGetLogicalCoreCount() {
                return CPUFeatures::Get().topology.numLogicalCores;
            }

            // -----------------------------------------------------------------
            // API legacy (dépréciée, opt-in uniquement)
            // -----------------------------------------------------------------
            // Ces wrappers sont fournis uniquement si NKENTSEU_ENABLE_LEGACY_PLATFORM_API
            // est défini. Ils redirigent vers les nouvelles fonctions Nk*.

            #if defined(NKENTSEU_ENABLE_LEGACY_PLATFORM_API)
                /**
                 * @deprecated Utiliser NkHasSSE2() à la place
                 */
                [[deprecated("Use NkHasSSE2()")]]
                NKENTSEU_API_INLINE bool HasSSE2() {
                    return NkHasSSE2();
                }

                /**
                 * @deprecated Utiliser NkHasAVX() à la place
                 */
                [[deprecated("Use NkHasAVX()")]]
                NKENTSEU_API_INLINE bool HasAVX() {
                    return NkHasAVX();
                }

                /**
                 * @deprecated Utiliser NkHasAVX2() à la place
                 */
                [[deprecated("Use NkHasAVX2()")]]
                NKENTSEU_API_INLINE bool HasAVX2() {
                    return NkHasAVX2();
                }

                /**
                 * @deprecated Utiliser NkHasAVX512() à la place
                 */
                [[deprecated("Use NkHasAVX512()")]]
                NKENTSEU_API_INLINE bool HasAVX512() {
                    return NkHasAVX512();
                }

                /**
                 * @deprecated Utiliser NkHasNEON() à la place
                 */
                [[deprecated("Use NkHasNEON()")]]
                NKENTSEU_API_INLINE bool HasNEON() {
                    return NkHasNEON();
                }

                /**
                 * @deprecated Utiliser NkHasFMA() à la place
                 */
                [[deprecated("Use NkHasFMA()")]]
                NKENTSEU_API_INLINE bool HasFMA() {
                    return NkHasFMA();
                }

                /**
                 * @deprecated Utiliser NkGetCacheLineSize() à la place
                 */
                [[deprecated("Use NkGetCacheLineSize()")]]
                NKENTSEU_API_INLINE int32_t GetCacheLineSize() {
                    return NkGetCacheLineSize();
                }

                /**
                 * @deprecated Utiliser NkGetPhysicalCoreCount() à la place
                 */
                [[deprecated("Use NkGetPhysicalCoreCount()")]]
                NKENTSEU_API_INLINE int32_t GetPhysicalCoreCount() {
                    return NkGetPhysicalCoreCount();
                }

                /**
                 * @deprecated Utiliser NkGetLogicalCoreCount() à la place
                 */
                [[deprecated("Use NkGetLogicalCoreCount()")]]
                NKENTSEU_API_INLINE int32_t GetLogicalCoreCount() {
                    return NkGetLogicalCoreCount();
                }
            #endif

        } // namespace platform

    } // namespace nkentseu

#endif // NKENTSEU_PLATFORM_NKCPUFEATURES_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKCPUFEATURES.H
// =============================================================================
// Ce fichier fournit une API complète pour détecter les fonctionnalités CPU
// à l'exécution et adapter le code en conséquence.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Sélection de chemin de code optimisé par SIMD
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkCPUFeatures.h"

    void ProcessData(float* data, size_t count) {
        const auto& cpu = nkentseu::platform::CPUFeatures::Get();

        // Sélection du meilleur chemin disponible
        if (cpu.simd.hasAVX512F) {
            ProcessDataAVX512(data, count);
        } else if (cpu.simd.hasAVX2) {
            ProcessDataAVX2(data, count);
        } else if (cpu.simd.hasSSE42) {
            ProcessDataSSE42(data, count);
        } else {
            ProcessDataScalar(data, count);
        }
    }

    // Alternative avec les macros de convenance Nk* :
    void ProcessDataSimple(float* data, size_t count) {
        if (nkentseu::platform::NkHasAVX2()) {
            ProcessDataAVX2(data, count);
        } else if (nkentseu::platform::NkHasSSE2()) {
            ProcessDataSSE2(data, count);
        } else {
            ProcessDataScalar(data, count);
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Configuration multi-threading basée sur la topologie
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkCPUFeatures.h"
    #include <thread>
    #include <vector>

    void LaunchParallelWork() {
        const auto& cpu = nkentseu::platform::CPUFeatures::Get();

        // Déterminer le nombre optimal de threads
        int32_t threadCount = cpu.topology.numPhysicalCores;

        // Ajuster si Hyper-Threading est activé
        if (cpu.topology.hasHyperThreading) {
            // Pour les charges de travail CPU-bound, privilégier les cœurs physiques
            // Pour les charges I/O-bound, on peut utiliser plus de threads
            if (IsIOBoundWorkload()) {
                threadCount = cpu.topology.numLogicalCores;
            }
        }

        // Lancer les threads
        std::vector<std::thread> workers;
        workers.reserve(static_cast<size_t>(threadCount));

        for (int32_t i = 0; i < threadCount; ++i) {
            workers.emplace_back(WorkerFunction, i);
        }

        // Attendre la complétion
        for (auto& t : workers) {
            t.join();
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Optimisation basée sur la taille de cache
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkCPUFeatures.h"

    void ProcessLargeArray(const float* input, float* output, size_t elementCount) {
        const auto& cpu = nkentseu::platform::CPUFeatures::Get();

        // Taille de ligne de cache pour l'alignement
        const size_t cacheLine = static_cast<size_t>(cpu.cache.lineSize);

        // Taille de bloc pour le tiling (adapter à L1/L2)
        const size_t blockSize = static_cast<size_t>(cpu.cache.l2Size) * 1024 / 4;

        // Allocation alignée sur ligne de cache
        float* alignedOutput = static_cast<float*>(
            aligned_alloc(cacheLine, elementCount * sizeof(float))
        );

        // Traitement par blocs pour optimiser l'utilisation du cache
        for (size_t blockStart = 0; blockStart < elementCount; blockStart += blockSize) {
            size_t blockEnd = (blockStart + blockSize < elementCount)
                ? blockStart + blockSize
                : elementCount;

            ProcessBlock(input + blockStart, alignedOutput + blockStart, blockEnd - blockStart);
        }

        // Copie finale si nécessaire
        memcpy(output, alignedOutput, elementCount * sizeof(float));
        free(alignedOutput);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Logging des informations CPU au démarrage
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkCPUFeatures.h"
    #include "NKPlatform/NkFoundationLog.h"

    void LogSystemInfo() {
        const auto& cpu = nkentseu::platform::CPUFeatures::Get();

        NK_FOUNDATION_LOG_INFO("=== CPU Information ===");
        NK_FOUNDATION_LOG_INFO("Vendor: %s", cpu.vendor);
        NK_FOUNDATION_LOG_INFO("Brand: %s", cpu.brand);
        NK_FOUNDATION_LOG_INFO("Family/Model/Stepping: %d/%d/%d",
            cpu.family, cpu.model, cpu.stepping);
        NK_FOUNDATION_LOG_INFO("Frequency: %d MHz (base) / %d MHz (max)",
            cpu.baseFrequency, cpu.maxFrequency);

        NK_FOUNDATION_LOG_INFO("\n=== Topology ===");
        NK_FOUNDATION_LOG_INFO("Physical cores: %d", cpu.topology.numPhysicalCores);
        NK_FOUNDATION_LOG_INFO("Logical cores: %d", cpu.topology.numLogicalCores);
        NK_FOUNDATION_LOG_INFO("Hyper-Threading: %s",
            cpu.topology.hasHyperThreading ? "Yes" : "No");

        NK_FOUNDATION_LOG_INFO("\n=== Cache ===");
        NK_FOUNDATION_LOG_INFO("Line size: %d bytes", cpu.cache.lineSize);
        NK_FOUNDATION_LOG_INFO("L1 Data: %d KB, L1 Inst: %d KB",
            cpu.cache.l1DataSize, cpu.cache.l1InstructionSize);
        NK_FOUNDATION_LOG_INFO("L2: %d KB, L3: %d KB",
            cpu.cache.l2Size, cpu.cache.l3Size);

        NK_FOUNDATION_LOG_INFO("\n=== SIMD Features ===");
        if (cpu.simd.hasSSE2) NK_FOUNDATION_PRINT("  SSE2");
        if (cpu.simd.hasSSE42) NK_FOUNDATION_PRINT("  SSE4.2");
        if (cpu.simd.hasAVX) NK_FOUNDATION_PRINT("  AVX");
        if (cpu.simd.hasAVX2) NK_FOUNDATION_PRINT("  AVX2");
        if (cpu.simd.hasAVX512F) NK_FOUNDATION_PRINT("  AVX-512");
        if (cpu.simd.hasNEON) NK_FOUNDATION_PRINT("  NEON");
        if (cpu.simd.hasSVE) NK_FOUNDATION_PRINT("  SVE");

        NK_FOUNDATION_LOG_INFO("\n=== Extended Features ===");
        if (cpu.extended.hasAES) NK_FOUNDATION_PRINT("  AES-NI");
        if (cpu.extended.hasSHA) NK_FOUNDATION_PRINT("  SHA");
        if (cpu.extended.hasRDRAND) NK_FOUNDATION_PRINT("  RDRAND");
        if (cpu.extended.hasBMI2) NK_FOUNDATION_PRINT("  BMI2");
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Dispatch runtime avec fallback portable
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkCPUFeatures.h"

    // Déclaration des implémentations spécialisées
    void Compute_Scalar(const float* a, const float* b, float* out, size_t n);
    void Compute_SSE2(const float* a, const float* b, float* out, size_t n);
    void Compute_AVX2(const float* a, const float* b, float* out, size_t n);
    void Compute_AVX512(const float* a, const float* b, float* out, size_t n);

    // Pointeur de fonction pour le dispatch
    using ComputeFunc = void(*)(const float*, const float*, float*, size_t);

    // Initialisation du dispatch au démarrage
    ComputeFunc g_ComputeImpl = nullptr;

    void InitializeComputeDispatch() {
        const auto& cpu = nkentseu::platform::CPUFeatures::Get();

        // Sélection de la meilleure implémentation disponible
        if (cpu.simd.hasAVX512F) {
            g_ComputeImpl = Compute_AVX512;
        } else if (cpu.simd.hasAVX2) {
            g_ComputeImpl = Compute_AVX2;
        } else if (cpu.simd.hasSSE2) {
            g_ComputeImpl = Compute_SSE2;
        } else {
            g_ComputeImpl = Compute_Scalar;
        }
    }

    // Fonction publique : appelle l'implémentation optimale
    void Compute(const float* a, const float* b, float* out, size_t n) {
        // g_ComputeImpl est initialisé au démarrage, donc safe à appeler
        g_ComputeImpl(a, b, out, n);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Vérification de fonctionnalités pour activation conditionnelle
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkCPUFeatures.h"

    class CryptoEngine {
    public:
        CryptoEngine() {
            const auto& cpu = nkentseu::platform::CPUFeatures::Get();

            // Activer les accélérations matérielles si disponibles
            if (cpu.extended.hasAES) {
                m_useAESNI = true;
                NK_FOUNDATION_LOG_DEBUG("AES-NI enabled");
            } else {
                m_useAESNI = false;
                NK_FOUNDATION_LOG_WARN("AES-NI not available, using software fallback");
            }

            if (cpu.extended.hasSHA) {
                m_useSHAExtensions = true;
            }

            // RDRAND pour la génération de clés
            if (cpu.extended.hasRDRAND) {
                m_useHardwareRNG = true;
            }
        }

        void EncryptBlock(uint8_t* output, const uint8_t* input, const uint8_t* key) {
            if (m_useAESNI) {
                EncryptWithAESNI(output, input, key);
            } else {
                EncryptWithSoftwareAES(output, input, key);
            }
        }

    private:
        bool m_useAESNI = false;
        bool m_useSHAExtensions = false;
        bool m_useHardwareRNG = false;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Combinaison avec d'autres modules NKPlatform
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkPlatformDetect.h"   // NKENTSEU_PLATFORM_*
    #include "NKPlatform/NkArchDetect.h"       // NKENTSEU_ARCH_*
    #include "NKPlatform/NkCPUFeatures.h"      // Détection CPU runtime

    void PlatformOptimizedInit() {
        const auto& cpu = nkentseu::platform::CPUFeatures::Get();

        // Combinaison plateforme + architecture + fonctionnalités CPU
        #if defined(NKENTSEU_ARCH_X86_64)
            if (cpu.simd.hasAVX2) {
                InitializeAVX2Optimizations();
            }
        #elif defined(NKENTSEU_ARCH_ARM64)
            if (cpu.simd.hasNEON) {
                InitializeNEONOptimizations();
            }
            if (cpu.simd.hasSVE) {
                InitializeSVEOptimizations();
            }
        #endif

        // Ajustement basé sur la topologie
        SetThreadAffinity(cpu.topology.numPhysicalCores);

        // Configuration du cache-aware allocator
        ConfigureCacheAwareAllocator(
            static_cast<size_t>(cpu.cache.lineSize),
            static_cast<size_t>(cpu.cache.l3Size) * 1024
        );
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================