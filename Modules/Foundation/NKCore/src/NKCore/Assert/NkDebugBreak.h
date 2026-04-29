// =============================================================================
// NKCore/Assert/NkDebugBreak.h
// Macro portable pour point d'arrêt débogueur multi-plateforme.
//
// Design :
//  - Macro NKENTSEU_DEBUGBREAK() pour interruption contrôlée du débogueur
//  - Détection automatique du compilateur (MSVC, GCC, Clang) et architecture
//  - Implémentations spécifiques : __debugbreak(), int $0x03, __builtin_trap(), SIGTRAP
//  - Macro conditionnelle NKENTSEU_DEBUGBREAK_IF() active uniquement en mode debug
//  - Alias NKENTSEU_DEBUG_BREAK() pour compatibilité avec l'ancienne nomenclature
//  - Zéro overhead en production : macros expansées en ((void)0) quand NDEBUG défini
//
// Intégration :
//  - Utilise NKPlatform/NkPlatformDetect.h pour détection de l'OS cible
//  - Utilise NKPlatform/NkCompilerDetect.h pour détection du compilateur
//  - Utilise NKPlatform/NkArchDetect.h (indirectement) pour détection CPU
//  - Header-only : aucune implémentation .cpp requise
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CORE_NKDEBUGBREAK_H_INCLUDED
#define NKENTSEU_CORE_NKDEBUGBREAK_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules de détection de plateforme et compilateur.

    #include "NKPlatform/NkPlatformDetect.h"
    #include "NKPlatform/NkCompilerDetect.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : MACRO PRINCIPALE NKENTSEU_DEBUGBREAK
    // -------------------------------------------------------------------------
    // Définition conditionnelle selon le compilateur et l'architecture cible.

    /**
     * @defgroup DebugBreakMacros Macros de Point d'Arrêt Débogueur
     * @brief Macro portable pour interruption contrôlée dans le débogueur
     * @ingroup DebugUtilities
     *
     * Ces macros permettent de déclencher un point d'arrêt programme
     * de manière portable sur toutes les plateformes supportées.
     *
     * @note
     *   - En mode release (NDEBUG défini), les macros conditionnelles deviennent no-op
     *   - NKENTSEU_DEBUGBREAK() est toujours actif : utiliser avec précaution en production
     *   - NKENTSEU_DEBUGBREAK_IF() est désactivé en production : sûr pour les checks runtime
     *
     * @warning
     *   L'utilisation de NKENTSEU_DEBUGBREAK() en production peut causer un crash
     *   ou un comportement indéfini selon la plateforme. Préférer NKENTSEU_DEBUGBREAK_IF()
     *   avec une condition dépendant de NKENTSEU_DEBUG pour un usage sécurisé.
     *
     * @example
     * @code
     * void CriticalAssert(bool condition, const char* msg) {
     *     if (!condition) {
     *         // Point d'arrêt inconditionnel (dangereux en production)
     *         // NKENTSEU_DEBUGBREAK();
     *
     *         // Point d'arrêt conditionnel sécurisé (désactivé en release)
     *         NKENTSEU_DEBUGBREAK_IF(true);  // S'arrête seulement en debug
     *
     *         LogError(msg);
     *         AbortGracefully();
     *     }
     * }
     * @endcode
     */
    /** @{ */

    // ============================================================
    // COMPILATEUR MSVC (Windows)
    // ============================================================

    #if defined(NKENTSEU_COMPILER_MSVC)
        /**
         * @brief Point d'arrêt débogueur pour MSVC
         * @def NKENTSEU_DEBUGBREAK
         * @ingroup DebugBreakMacros
         * @note Utilise l'intrinsèque __debugbreak() de MSVC
         */
        #define NKENTSEU_DEBUGBREAK() __debugbreak()

    // ============================================================
    // COMPILATEURS GCC / CLANG (Unix-like, multi-arch)
    // ============================================================

    #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)

        // --------------------------------------------------------
        // Architecture x86 / x86_64 : instruction INT 3
        // --------------------------------------------------------

        #if defined(NKENTSEU_ARCH_X86) || defined(NKENTSEU_ARCH_X86_64)
            /**
             * @brief Point d'arrêt débogueur pour x86/x86_64 avec GCC/Clang
             * @def NKENTSEU_DEBUGBREAK
             * @ingroup DebugBreakMacros
             * @note Utilise l'instruction assembleur "int $0x03" (software breakpoint)
             */
            #define NKENTSEU_DEBUGBREAK() __asm__ volatile("int $0x03")

        // --------------------------------------------------------
        // Architecture ARM / ARM64 : builtin trap
        // --------------------------------------------------------

        #elif defined(NKENTSEU_ARCH_ARM) || defined(NKENTSEU_ARCH_ARM64)
            /**
             * @brief Point d'arrêt débogueur pour ARM/ARM64 avec GCC/Clang
             * @def NKENTSEU_DEBUGBREAK
             * @ingroup DebugBreakMacros
             * @note Utilise __builtin_trap() qui génère une instruction breakpoint ARM
             */
            #define NKENTSEU_DEBUGBREAK() __builtin_trap()

        // --------------------------------------------------------
        // Autres architectures GCC/Clang : fallback générique
        // --------------------------------------------------------

        #else
            /**
             * @brief Point d'arrêt débogueur fallback pour GCC/Clang
             * @def NKENTSEU_DEBUGBREAK
             * @ingroup DebugBreakMacros
             * @note Utilise __builtin_trap() comme solution portable générique
             */
            #define NKENTSEU_DEBUGBREAK() __builtin_trap()
        #endif

    // ============================================================
    // COMPILATEURS INCONNUS / FALLBACK POSIX
    // ============================================================

    #else
        /**
         * @brief Point d'arrêt débogueur fallback POSIX
         * @def NKENTSEU_DEBUGBREAK
         * @ingroup DebugBreakMacros
         * @note Utilise raise(SIGTRAP) pour compatibilité Unix générique
         * @warning Requiert <signal.h> : inclus automatiquement dans cette branche
         */
        #include <signal.h>
        #define NKENTSEU_DEBUGBREAK() raise(SIGTRAP)
    #endif

    /**
     * @brief Alias de compatibilité pour NKENTSEU_DEBUGBREAK
     * @def NKENTSEU_DEBUG_BREAK
     * @ingroup DebugBreakMacros
     * @deprecated Préférer NKENTSEU_DEBUGBREAK (nomenclature officielle)
     */
    #define NKENTSEU_DEBUG_BREAK() NKENTSEU_DEBUGBREAK()

    /** @} */ // End of DebugBreakMacros

    // -------------------------------------------------------------------------
    // SECTION 3 : MACRO CONDITIONNELLE DEBUG-ONLY
    // -------------------------------------------------------------------------
    // Macro qui n'agit qu'en mode debug, sans overhead en production.

    /**
     * @defgroup ConditionalDebugBreakMacros Macros Conditionnelles Debug-Only
     * @brief Macros de point d'arrêt activées uniquement en mode debug
     * @ingroup DebugUtilities
     *
     * Ces macros sont conçues pour être utilisées dans du code runtime
     * sans impact sur les performances en production. Quand NKENTSEU_DEBUG
     * n'est pas défini (mode release), elles s'expandent en ((void)0).
     *
     * @note
     *   - NKENTSEU_DEBUGBREAK_IF(condition) : exécute DEBUGBREAK si condition vraie
     *   - NKENTSEU_DEBUG_BREAK_IF(condition) : alias de compatibilité
     *   - Aucune évaluation de la condition en mode release (zéro overhead)
     *
     * @example
     * @code
     * void* Allocate(size_t size) {
     *     // Vérification de sanity en debug uniquement
     *     NKENTSEU_DEBUGBREAK_IF(size == 0);        // Break si taille nulle
     *     NKENTSEU_DEBUGBREAK_IF(size > MAX_ALLOC); // Break si trop grand
     *
     *     // En production, ces checks sont compilés mais n'exécutent rien
     *     return PlatformAllocate(size);
     * }
     *
     * void ProcessData(int* data, size_t count) {
     *     // Invariants de débogage
     *     NKENTSEU_DEBUGBREAK_IF(data == nullptr && count > 0);
     *     NKENTSEU_DEBUGBREAK_IF(count > MAX_BUFFER_SIZE);
     *
     *     // ... traitement ...
     * }
     * @endcode
     */
    /** @{ */

    // ============================================================
    // MODE DEBUG : macros actives
    // ============================================================

    #if defined(NKENTSEU_DEBUG)
        /**
         * @brief Point d'arrêt conditionnel activé en mode debug
         * @def NKENTSEU_DEBUGBREAK_IF(condition)
         * @param condition Expression booléenne à évaluer
         * @ingroup ConditionalDebugBreakMacros
         * @note Si condition est vraie, exécute NKENTSEU_DEBUGBREAK()
         * @note Utilise do-while(0) pour sécurité syntaxique dans tous les contextes
         */
        #define NKENTSEU_DEBUGBREAK_IF(condition) \
        do { \
            if (condition) { \
                NKENTSEU_DEBUGBREAK(); \
            } \
        } while (0)

        /**
         * @brief Alias de compatibilité pour NKENTSEU_DEBUGBREAK_IF
         * @def NKENTSEU_DEBUG_BREAK_IF
         * @param condition Expression booléenne à évaluer
         * @ingroup ConditionalDebugBreakMacros
         * @deprecated Préférer NKENTSEU_DEBUGBREAK_IF (nomenclature officielle)
         */
        #define NKENTSEU_DEBUG_BREAK_IF(condition) NKENTSEU_DEBUGBREAK_IF(condition)

    // ============================================================
    // MODE RELEASE : macros no-op (zéro overhead)
    // ============================================================

    #else
        /**
         * @brief Version no-op de NKENTSEU_DEBUGBREAK_IF pour le mode release
         * @def NKENTSEU_DEBUGBREAK_IF(condition)
         * @param condition Expression ignorée en production
         * @ingroup ConditionalDebugBreakMacros
         * @note S'expand en ((void)0) : aucun code généré, condition non évaluée
         */
        #define NKENTSEU_DEBUGBREAK_IF(condition) ((void)0)

        /**
         * @brief Version no-op de NKENTSEU_DEBUG_BREAK_IF pour le mode release
         * @def NKENTSEU_DEBUG_BREAK_IF(condition)
         * @param condition Expression ignorée en production
         * @ingroup ConditionalDebugBreakMacros
         */
        #define NKENTSEU_DEBUG_BREAK_IF(condition) ((void)0)
    #endif

    /** @} */ // End of ConditionalDebugBreakMacros

#endif // NKENTSEU_CORE_NKDEBUGBREAK_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION DE NKDEBUGBREAK.H
// =============================================================================
// Ce fichier fournit des macros portables pour points d'arrêt débogueur.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Assertions de débogage avec point d'arrêt automatique
// -----------------------------------------------------------------------------
/*
    #include "NKCore/Assert/NkDebugBreak.h"
    #include "NKCore/NkFoundationLog.h"

    // Macro d'assertion personnalisée avec break debugger
    #define NK_DEBUG_ASSERT(condition, msg) \
    do { \
        if (!(condition)) { \
            NK_FOUNDATION_LOG_ERROR("Assertion failed: %s at %s:%d", \
                msg, __FILE__, __LINE__); \
            NKENTSEU_DEBUGBREAK_IF(true); \
        } \
    } while (0)

    void ProcessBuffer(void* buffer, size_t size) {
        // Vérifications de préconditions en debug
        NK_DEBUG_ASSERT(buffer != nullptr, "Buffer pointer is null");
        NK_DEBUG_ASSERT(size > 0, "Buffer size must be positive");
        NK_DEBUG_ASSERT(size <= MAX_BUFFER_SIZE, "Buffer size exceeds limit");

        // ... traitement du buffer ...
    }

    // En mode debug : si une assertion échoue, le débogueur s'arrête ici
    // En mode release : les checks sont compilés mais NKENTSEU_DEBUGBREAK_IF est no-op
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Validation d'invariants avec break conditionnel
// -----------------------------------------------------------------------------
/*
    #include "NKCore/Assert/NkDebugBreak.h"

    class DataStructure {
    public:
        void AddItem(int value) {
            // Invariant : la taille ne doit pas dépasser la capacité
            NKENTSEU_DEBUGBREAK_IF(m_size >= m_capacity);

            // Invariant : pas de doublons si mode strict
            #if defined(ENABLE_STRICT_MODE)
                NKENTSEU_DEBUGBREAK_IF(Contains(value));
            #endif

            // ... ajout effectif ...
            m_items[m_size++] = value;
        }

        void RemoveItem(int index) {
            // Invariant : index dans les bornes valides
            NKENTSEU_DEBUGBREAK_IF(index < 0 || index >= static_cast<int>(m_size));

            // ... suppression effective ...
        }

        // Méthode de vérification d'intégrité (debug only)
        void ValidateIntegrity() const {
            NKENTSEU_DEBUGBREAK_IF(m_size > m_capacity);
            NKENTSEU_DEBUGBREAK_IF(m_capacity > MAX_CAPACITY);
            NKENTSEU_DEBUGBREAK_IF(m_items == nullptr && m_capacity > 0);
        }

    private:
        int* m_items = nullptr;
        size_t m_size = 0;
        size_t m_capacity = 0;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Points d'arrêt stratégiques pour débogage pas-à-pas
// -----------------------------------------------------------------------------
/*
    #include "NKCore/Assert/NkDebugBreak.h"

    void ComplexAlgorithm(InputData* input, OutputData* output) {
        // Point d'arrêt à l'entrée pour inspection initiale
        // (à activer manuellement en commentant/décommentant)
        // NKENTSEU_DEBUGBREAK();

        // Validation des paramètres
        NKENTSEU_DEBUGBREAK_IF(input == nullptr);
        NKENTSEU_DEBUGBREAK_IF(output == nullptr);

        // Phase 1 : Pré-traitement
        {
            // Point d'arrêt optionnel après pré-traitement
            // NKENTSEU_DEBUGBREAK_IF(input->flags & DEBUG_BREAK_AFTER_PREPROCESS);

            Preprocess(input);
        }

        // Phase 2 : Calcul principal
        {
            // Break si les données intermédiaires sont incohérentes
            NKENTSEU_DEBUGBREAK_IF(!ValidateIntermediateState());

            auto result = ComputeCore(input);

            // Break si le résultat est hors limites attendues
            NKENTSEU_DEBUGBREAK_IF(result.value < MIN_EXPECTED);
            NKENTSEU_DEBUGBREAK_IF(result.value > MAX_EXPECTED);

            *output = result;
        }

        // Phase 3 : Post-traitement
        {
            NKENTSEU_DEBUGBREAK_IF(!ValidateOutputConsistency(output));
            Postprocess(output);
        }

        // Point d'arrêt final pour inspection du résultat
        // NKENTSEU_DEBUGBREAK_IF(input->flags & DEBUG_BREAK_AT_END);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Intégration avec système d'assertions du framework
// -----------------------------------------------------------------------------
/*
    #include "NKCore/Assert/NkDebugBreak.h"
    #include "NKCore/NkTypes.h"
    #include "NKCore/NkBuiltin.h"  // Pour NKENTSEU_BUILTIN_*

    // Macro d'assertion complète avec contexte et break debugger
    #define NK_CORE_ASSERT(condition, msg) \
    do { \
        if (!(condition)) { \
            /* Log l'erreur avec contexte automatique *\/ \
            NKENTSEU_LOG_ERROR_CONTEXT( \
                "Assertion failed: " msg, \
                NKENTSEU_BUILTIN_FILE, \
                NKENTSEU_BUILTIN_LINE, \
                NKENTSEU_BUILTIN_FUNCTION \
            ); \
            /* Point d'arrêt pour inspection immédiate *\/ \
            NKENTSEU_DEBUGBREAK(); \
            /* Optionnel : abort pour éviter la continuation en état invalide *\/ \
            /* abort(); *\/ \
        } \
    } while (0)

    // Version "soft" qui ne break pas mais logge uniquement
    #define NK_CORE_VERIFY(condition, msg) \
    do { \
        if (!(condition)) { \
            NKENTSEU_LOG_WARNING_CONTEXT( \
                "Verification failed: " msg, \
                NKENTSEU_BUILTIN_FILE, \
                NKENTSEU_BUILTIN_LINE, \
                NKENTSEU_BUILTIN_FUNCTION \
            ); \
            /* Pas de break : continue l'exécution *\/ \
        } \
    } while (0)

    // Usage dans du code critique
    nk_bool InitializeSubsystem(Config* config) {
        NK_CORE_ASSERT(config != nullptr, "Config cannot be null");
        NK_CORE_ASSERT(config->IsValid(), "Config validation failed");

        // Vérification non-fatale : log mais continue
        NK_CORE_VERIFY(config->GetVersion() >= MIN_VERSION,
            "Config version may be outdated");

        // ... initialisation ...
        return true;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Débogage de code multithread avec breaks conditionnels
// -----------------------------------------------------------------------------
/*
    #include "NKCore/Assert/NkDebugBreak.h"
    #include "NKCore/NkAtomic.h"  // Pour compteurs thread-safe

    class ThreadSafeCounter {
    public:
        void Increment() {
            auto prev = m_counter.FetchAdd(1);

            // Break si overflow détecté (debug only)
            NKENTSEU_DEBUGBREAK_IF(prev == NKENTSEU_MAX_INT64);

            // Break si le compteur atteint une valeur de débogage spécifique
            #if defined(DEBUG_BREAK_AT_COUNT)
                NKENTSEU_DEBUGBREAK_IF(m_counter.Load() == DEBUG_BREAK_AT_COUNT);
            #endif
        }

        void Decrement() {
            auto prev = m_counter.FetchSub(1);

            // Break si underflow (négatif pour compteur non-signé)
            NKENTSEU_DEBUGBREAK_IF(prev == 0);
        }

        int64_t Get() const {
            auto value = m_counter.Load();

            // Break si valeur incohérente (ex: négative pour compteur)
            NKENTSEU_DEBUGBREAK_IF(value < 0);

            return value;
        }

        void Reset() {
            // Break si reset inattendu en production (debug only)
            NKENTSEU_DEBUGBREAK_IF(m_counter.Load() != 0);

            m_counter.Store(0);
        }

    private:
        nkentseu::NkAtomicInt64 m_counter{0};
    };

    // Usage dans un contexte multithread
    void WorkerThread(ThreadSafeCounter* counter, int iterations) {
        for (int i = 0; i < iterations; ++i) {
            counter->Increment();

            // Break si progression anormale (ex: trop rapide)
            // NKENTSEU_DEBUGBREAK_IF(i % 1000 == 0 && GetTimestamp() < expectedTime);

            ProcessItem(i);
        }
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================