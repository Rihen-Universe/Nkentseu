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
// License : Proprietary - All Rights Reserved (see LICENSE)
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
				CacheInfo();
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
				CPUTopology();
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
				SIMDFeatures();
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
				ExtendedFeatures();
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
				void ToString(char *outBuffer, size_t outBufferSize) const;

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
				const char *ToString() const;

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
				void CPUID(int32_t function, int32_t subfunction, int32_t *eax, int32_t *ebx, int32_t *ecx,
						   int32_t *edx);
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
// Exemples : les exemples COMPILÉS et exécutés sont dans Docs/exemples/Foundation/NKPlatform/. Ceux qui étaient
// ici en commentaire sont gardés tels quels dans Docs/archives/NKPlatform/exemples_des_entetes.md (à convertir).

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - All Rights Reserved (see LICENSE)
// ============================================================