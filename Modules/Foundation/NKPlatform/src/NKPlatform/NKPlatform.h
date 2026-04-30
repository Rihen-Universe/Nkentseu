// =============================================================================
// NKPlatform/NKPlatform.h
// Point d'entrée principal du module NKPlatform.
//
// Design :
//  - Fichier "umbrella header" regroupant tous les exports publics du module
//  - Inclut les headers de détection, types, et utilitaires système
//  - Fournit une interface unifiée pour l'interrogation de la plateforme
//  - Conçu pour être le seul fichier à inclure pour utiliser NKPlatform
//
// Intégration :
//  - Inclut tous les headers publics de NKPlatform dans l'ordre de dépendance
//  - Compatible avec NKCore/NkTypes.h pour les types fondamentaux
//  - Peut être inclus avant ou après NKCore.h selon les besoins
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_PLATFORM_H_INCLUDED
#define NKENTSEU_PLATFORM_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : DÉTECTION ET CONFIGURATION DE BASE
    // -------------------------------------------------------------------------
    // Headers fondamentaux pour la détection de l'environnement de compilation.

    /**
     * @defgroup PlatformModule Module NKPlatform
     * @brief Module de détection et d'abstraction plateforme multi-système
     * @ingroup FrameworkModules
     *
     * NKPlatform fournit une interface unifiée pour :
     *   - Détecter l'OS, l'architecture CPU, le compilateur au runtime
     *   - Accéder aux capacités système (mémoire, cache, SIMD, threading)
     *   - Abstraire les APIs système spécifiques (Win32, POSIX, etc.)
     *   - Fournir des utilitaires portables (alignement, endianness, etc.)
     *
     * @note
     *   - Ce fichier est le seul à inclure pour utiliser le module NKPlatform
     *   - Tous les autres headers du module sont inclus indirectement
     *   - L'ordre d'inclusion est géré automatiquement pour éviter les dépendances circulaires
     *
     * @example
     * @code
     * // Inclusion unique du module Platform
     * #include "NKPlatform/NKPlatform.h"
     *
     * void PrintSystemInfo() {
     *     const auto* info = nkentseu::platform::NkGetPlatformInfo();
     *
     *     printf("Running on %s %s\n", info->osName, info->osVersion);
     *     printf("CPU: %s (%u cores)\n", info->archName, info->cpuCoreCount);
     *
     *     if (nkentseu::platform::NkHasSIMDFeature("AVX2")) {
     *         printf("AVX2 instructions available\n");
     *     }
     * }
     * @endcode
     */
    /** @{ */

    // ============================================================
    // DÉTECTION PLATEFORME, ARCHITECTURE, COMPILATEUR
    // ============================================================

    /**
     * @brief Détection de la plateforme cible (OS)
     * @ingroup PlatformDetection
     * @note Définit les macros NKENTSEU_PLATFORM_* selon l'OS détecté
     */
    #include "NKPlatform/NkPlatformDetect.h"

    /**
     * @brief Détection de l'architecture CPU cible
     * @ingroup PlatformDetection
     * @note Définit les macros NKENTSEU_ARCH_* selon le CPU détecté
     */
    #include "NKPlatform/NkArchDetect.h"

    /**
     * @brief Détection du compilateur et de ses capacités
     * @ingroup PlatformDetection
     * @note Définit les macros NKENTSEU_COMPILER_* et NKENTSEU_HAS_*
     */
    #include "NKPlatform/NkCompilerDetect.h"

    /**
     * @brief Détection de l'endianness (ordre des octets)
     * @ingroup PlatformDetection
     * @note Définit NkEndianness et macros associées
     */
    #include "NKPlatform/NkEndianness.h"

    // ============================================================
    // TYPES ET UTILITAIRES FONDAMENTAUX
    // ============================================================

    /**
     * @brief Détection des APIs graphiques et compute
     * @ingroup PlatformGraphics
     * @note Fournit NkGraphicsApi, NkGPUVendor, macros de détection GPU
     */
    #include "NKPlatform/NkCGXDetect.h"

    // ============================================================
    // UTILITAIRES SYSTÈME ET ABSTRACTIONS
    // ============================================================

    /**
     * @brief Utilitaires de logging système
     * @ingroup PlatformLogging
     * @note Fournit NK_FOUNDATION_LOG_* macros et backend portable
     */
    #include "NKPlatform/NkFoundationLog.h"

    // ============================================================
    // EXPORT ET VISIBILITÉ DES SYMBOLES
    // ============================================================

    /**
     * @brief Macros d'export pour bibliothèques partagées
     * @ingroup PlatformExport
     * @note Définit NKENTSEU_PLATFORM_API pour visibilité DLL/.so/.dylib
     */
    #include "NKPlatform/NkPlatformExport.h"

    /**
     * @brief Macros d'inlining forcé et optimisations
     * @ingroup PlatformInline
     * @note Définit NKENTSEU_FORCE_INLINE, NKENTSEU_NOINLINE, etc.
     */
    #include "NKPlatform/NkPlatformInline.h"

    /** @} */ // End of PlatformModule

#endif // NKENTSEU_PLATFORM_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION DE NKPLATFORM.H
// =============================================================================
// Ce fichier est le point d'entrée unique pour le module NKPlatform.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Inclusion minimale pour interrogation plateforme
// -----------------------------------------------------------------------------
/*
    // Fichier : main.cpp
    #include "NKPlatform/NKPlatform.h"  // Seul include nécessaire

    int main() {
        // Accès aux informations système
        const auto* info = nkentseu::platform::NkGetPlatformInfo();

        printf("Platform: %s %s\n", info->osName, info->osVersion);
        printf("Architecture: %s\n", info->archName);
        printf("Compiler: %s %s\n", info->compilerName, info->compilerVersion);

        return 0;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Utilisation des utilitaires système portables
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NKPlatform.h"

    void PortableFileOperation() {
        // Chemin de fichier portable (gère / et \ automatiquement)
        nkentseu::platform::NkPath configPath =
            nkentseu::platform::NkPath::Join({"config", "settings.json"});

        // Vérification d'existence portable
        if (nkentseu::platform::NkFile::Exists(configPath)) {
            // Lecture portable avec gestion d'erreur unifiée
            auto result = nkentseu::platform::NkFile::ReadAllText(configPath);
            if (result.IsSuccess()) {
                ProcessConfig(result.GetValue());
            } else {
                NK_FOUNDATION_LOG_ERROR("Failed to read config: %s",
                    nkentseu::platform::NkGetErrorString(result.GetError()));
            }
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Threading portable avec synchronisation
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NKPlatform.h"
    #include <atomic>

    class WorkerPool {
    public:
        WorkerPool(size_t threadCount) {
            for (size_t i = 0; i < threadCount; ++i) {
                m_threads.emplace_back([this]() {
                    WorkerThread();
                });
            }
        }

        ~WorkerPool() {
            m_stop.store(true);
            m_condition.notify_all();
            for (auto& t : m_threads) {
                if (t.joinable()) {
                    t.join();
                }
            }
        }

        void SubmitTask(std::function<void()> task) {
            {
                nkentseu::platform::NkScopedLock lock(m_mutex);
                m_tasks.push(std::move(task));
            }
            m_condition.notify_one();
        }

    private:
        void WorkerThread() {
            while (!m_stop.load()) {
                std::function<void()> task;
                {
                    nkentseu::platform::NkScopedLock lock(m_mutex);
                    m_condition.wait(lock, [this]() {
                        return m_stop.load() || !m_tasks.empty();
                    });
                    if (!m_tasks.empty()) {
                        task = std::move(m_tasks.front());
                        m_tasks.pop();
                    }
                }
                if (task) {
                    task();
                }
            }
        }

        std::vector<nkentseu::platform::NkThread> m_threads;
        std::queue<std::function<void()>> m_tasks;
        nkentseu::platform::NkMutex m_mutex;
        nkentseu::platform::NkConditionVariable m_condition;
        std::atomic<bool> m_stop{false};
    };
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Logging portable avec contexte plateforme
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NKPlatform.h"

    void InitializeApplication() {
        // Logging avec niveau configurable via NKENTSEU_LOG_LEVEL
        NK_FOUNDATION_LOG_INFO("Application starting...");

        // Logging avec localisation source automatique
        auto loc = NkCurrentSourceLocation;
        NK_FOUNDATION_LOG_DEBUG("Init at %s:%d in %s",
            loc.FileName(), loc.Line(), loc.FunctionName());

        // Logging d'erreur avec code plateforme
        auto result = nkentseu::platform::NkNetworkInit();
        if (!result.IsSuccess()) {
            NK_FOUNDATION_LOG_ERROR("Network init failed: %s (code: %d)",
                nkentseu::platform::NkGetErrorString(result.GetError()),
                static_cast<int>(result.GetError()));
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Détection runtime des capacités pour optimisation
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NKPlatform.h"

    // Dispatcher de fonction optimisée selon les capacités CPU
    typedef void (*ComputeFn)(float*, const float*, size_t);

    static ComputeFn sComputeImpl = nullptr;

    void Compute_Scalar(float* out, const float* in, size_t count);
    void Compute_SSE4(float* out, const float* in, size_t count);
    void Compute_AVX2(float* out, const float* in, size_t count);

    void InitializeComputeDispatcher() {
        const auto* info = nkentseu::platform::NkGetPlatformInfo();

        // Sélection par ordre de préférence (plus optimisé en premier)
        if (info->hasAVX2) {
            sComputeImpl = Compute_AVX2;
        } else if (info->hasSSE4_2) {
            sComputeImpl = Compute_SSE4;
        } else {
            sComputeImpl = Compute_Scalar;  // Fallback universel
        }
    }

    void Compute(float* out, const float* in, size_t count) {
        if (!sComputeImpl) {
            InitializeComputeDispatcher();  // Initialisation lazy thread-safe
        }
        sComputeImpl(out, in, count);
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================