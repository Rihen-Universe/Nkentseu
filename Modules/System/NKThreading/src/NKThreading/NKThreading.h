//
// NKThreading.h
// =============================================================================
// Description :
//   Header principal du module NKThreading : point d'entrée unique pour
//   accéder à toutes les primitives de synchronisation et de parallélisme.
//
// Objectif :
//   - Fournir un include unique pour accéder à l'ensemble du module
//   - Garantir un ordre d'inclusion cohérent et sans conflits
//   - Documenter clairement les composants disponibles et leurs cas d'usage
//   - Faciliter la migration et l'adoption du module par les nouveaux développeurs
//
// Architecture :
//   - Inclut tous les headers publics du module dans un ordre logique
//   - N'expose que les APIs publiques stables (pas d'implémentations internes)
//   - Compatible avec le pattern Pimpl : les détails restent cachés dans les .cpp
//
// Composants inclus :
//   🔹 Primitives de base       : NkMutex, NkRecursiveMutex, NkSpinLock
//   🔹 Synchronisation avancée : NkConditionVariable, NkSemaphore, NkSharedMutex
//   🔹 Synchronisation de phase: NkBarrier, NkLatch, NkEvent
//   🔹 Accès concurrent        : NkReaderWriterLock (+ guards RAII)
//   🔹 Gestion de threads      : NkThread, NkThreadLocal, NkThreadPool
//   🔹 Patterns async          : NkFuture, NkPromise, NkScopedLock
//   🔹 Utilitaires             : Guards RAII, helpers de synchronisation
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_THREADING_H__
#define __NKENTSEU_THREADING_H__

    // -------------------------------------------------------------------------
    // SECTION 1 : CONFIGURATION ET MACROS DE BASE
    // -------------------------------------------------------------------------
    // Doit être inclus en premier : définit les macros d'export et de détection
    // plateforme utilisées par tous les autres headers du module.
    #include "NKThreading/NkThreadingApi.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : PRIMITIVES DE SYNCHRONISATION DE BASE
    // -------------------------------------------------------------------------
    // Mutex standards pour l'exclusion mutuelle simple.
    #include "NKThreading/NkMutex.h"

    // Spin-lock haute performance pour sections critiques ultra-courtes.
    #include "NKThreading/NkSpinLock.h"

    // -------------------------------------------------------------------------
    // SECTION 3 : SYNCHRONISATION AVANCÉE
    // -------------------------------------------------------------------------
    // Variables de condition pour l'attente efficace sur prédicats.
    #include "NKThreading/NkConditionVariable.h"

    // Sémaphores compteurs pour la gestion de pools de ressources.
    #include "NKThreading/NkSemaphore.h"

    // Mutex partagés (reader-writer) pour accès concurrent en lecture.
    #include "NKThreading/NkSharedMutex.h"

    // -------------------------------------------------------------------------
    // SECTION 4 : SYNCHRONISATION DE PHASE ET ÉVÉNEMENTS
    // -------------------------------------------------------------------------
    // Barrière de synchronisation pour coordonner N threads sur un point de rendez-vous.
    #include "NKThreading/Synchronization/NkBarrier.h"

    // Latch de comptage pour attendre la completion de N tâches (usage unique).
    #include "NKThreading/Synchronization/NkLatch.h"

    // Événement de signalisation style Win32 (ManualReset/AutoReset).
    #include "NKThreading/Synchronization/NkEvent.h"

    // -------------------------------------------------------------------------
    // SECTION 5 : VERROUS LECTEUR-RÉDACTEUR
    // -------------------------------------------------------------------------
    // Reader-writer lock optimisé pour lectures fréquentes / écritures rares.
    #include "NKThreading/Synchronization/NkReaderWriterLock.h"

    // -------------------------------------------------------------------------
    // SECTION 6 : GESTION DE THREADS NATIFS
    // -------------------------------------------------------------------------
    // Wrapper C++ autour des threads natifs du système d'exploitation.
    #include "NKThreading/NkThread.h"

    // Stockage thread-local pour données isolées par thread.
    #include "NKThreading/NkThreadLocal.h"

    // -------------------------------------------------------------------------
    // SECTION 7 : PATTERNS ASYNCHRONES ET PARALLÉLISME
    // -------------------------------------------------------------------------
    // Future/Promise pour la programmation async/await style.
    #include "NKThreading/NkFuture.h"
    #include "NKThreading/NkPromise.h"

    // Thread pool avec work-stealing pour parallélisme automatique.
    #include "NKThreading/NkThreadPool.h"

    // -------------------------------------------------------------------------
    // SECTION 8 : UTILITAIRES RAII ET HELPERS
    // -------------------------------------------------------------------------
    // Guards génériques pour gestion automatique des verrous (duck-typing).
    #include "NKThreading/NkScopedLock.h"

    // -------------------------------------------------------------------------
    // SECTION 9 : ALIAS DE COMPATIBILITÉ LEGACY
    // -------------------------------------------------------------------------
    // Redirige les anciens namespaces vers les implémentations modernes.
    // Nouveau code : utiliser directement nkentseu::threading::
    #include "NKThreading/NkPromise.h"          // Alias entseu::NkPromise/Future
    #include "NKThreading/NkRecursiveMutex.h"   // Alias entseu::NkRecursiveMutex
    #include "NKThreading/NkSharedMutex.h"      // Alias entseu::NkSharedMutex/Lock
    #include "NKThreading/NkThreadLocal.h"      // Alias entseu::NkThreadLocal

    // -------------------------------------------------------------------------
    // SECTION 10 : NAMESPACE PRINCIPAL ET DOCUMENTATION
    // -------------------------------------------------------------------------
    namespace nkentseu {

        // =====================================================================
        // SOUS-NAMESPACE : threading (API publique principale)
        // =====================================================================
        /// @namespace nkentseu::threading
        /// @brief Module de primitives de threading et synchronisation.
        ///
        /// Ce namespace regroupe toutes les APIs publiques pour :
        /// - La création et gestion de threads natifs (@ref NkThread)
        /// - La synchronisation entre threads (@ref NkMutex, @ref NkConditionVariable)
        /// - La synchronisation de phase (@ref NkBarrier, @ref NkLatch, @ref NkEvent)
        /// - L'accès concurrent lecteur-rédacteur (@ref NkReaderWriterLock)
        /// - Le parallélisme automatique (@ref NkThreadPool, @ref ParallelFor)
        /// - Les patterns asynchrones (@ref NkFuture, @ref NkPromise)
        ///
        /// @section threading_quickstart Guide de démarrage rapide
        /// @code
        /// #include <NKThreading/NKThreading.h>
        /// using namespace nkentseu::threading;
        ///
        /// // Exemple 1 : Mutex + RAII guard
        /// NkMutex mutex;
        /// int shared = 0;
        /// void SafeIncrement() {
        ///     NkScopedLock guard(mutex);
        ///     ++shared;  // Protégé
        /// }
        ///
        /// // Exemple 2 : Thread pool pour parallélisation
        /// NkThreadPool pool(4);  // 4 workers
        /// pool.ParallelFor(1000, [](size_t i) {
        ///     ProcessItem(i);
        /// }, 100);  // Chunks de 100
        /// pool.Join();  // Attendre la fin
        ///
        /// // Exemple 3 : Latch pour synchronisation de completion
        /// NkLatch latch(3);  // Attendre 3 tâches
        /// // ... threads appellent latch.CountDown() ...
        /// latch.Wait();  // Bloque jusqu'à completion
        ///
        /// // Exemple 4 : Reader-writer lock pour cache
        /// NkReaderWriterLock rwlock;
        /// void ReadCache() {
        ///     NkReadLock lock(rwlock);  // Multiple readers OK
        ///     // ... lecture ...
        /// }
        /// void WriteCache() {
        ///     NkWriteLock lock(rwlock);  // Exclusive access
        ///     // ... écriture ...
        /// }
        /// @endcode
        ///
        /// @section threading_components Composants disponibles
        ///
        /// ### Primitives de synchronisation de base
        /// | Classe | Usage recommandé | Performance | Thread-safety |
        /// |--------|-----------------|-------------|---------------|
        /// | @ref NkMutex | Sections critiques moyennes/longues (>1µs) | ⭐⭐⭐ | Exclusive |
        /// | @ref NkSpinLock | Sections ultra-courtes (<100 cycles) | ⭐⭐⭐⭐⭐ | Exclusive |
        /// | @ref NkRecursiveMutex | API imbriquées avec re-lock | ⭐⭐ | Exclusive + recursive |
        ///
        /// ### Synchronisation avancée
        /// | Classe | Usage recommandé | Performance | Particularité |
        /// |--------|-----------------|-------------|---------------|
        /// | @ref NkConditionVariable | Attente sur prédicat complexe | ⭐⭐⭐⭐ | Wait/Notify pattern |
        /// | @ref NkSemaphore | Pool de ressources limitées | ⭐⭐⭐ | Compteur avec limite |
        /// | @ref NkSharedMutex | Lecture fréquente, écriture rare | ⭐⭐⭐⭐ | Alias NkReaderWriterLock |
        ///
        /// ### Synchronisation de phase et événements
        /// | Classe | Usage recommandé | Réutilisable | Particularité |
        /// |--------|-----------------|--------------|---------------|
        /// | @ref NkBarrier | Synchronisation de phase itérative | ✅ Oui | Tous threads progressent ensemble |
        /// | @ref NkLatch | Attendre completion de N tâches | ❌ Non | Compteur décrémente à zéro |
        /// | @ref NkEvent | Signal booléen persistant/éphémère | ✅ Oui | ManualReset/AutoReset modes |
        ///
        /// ### Accès concurrent lecteur-rédacteur
        /// | Classe | Usage recommandé | Performance | Accès simultané |
        /// |--------|-----------------|-------------|-----------------|
        /// | @ref NkReaderWriterLock | Cache, config, données read-heavy | ⭐⭐⭐⭐ | Multiple readers OR single writer |
        /// | @ref NkReadLock | Guard RAII pour lecture | ⭐⭐⭐⭐⭐ | Partagé avec autres readers |
        /// | @ref NkWriteLock | Guard RAII pour écriture | ⭐⭐⭐ | Exclusif (exclut tous) |
        ///
        /// ### Gestion de threads
        /// | Classe | Usage recommandé | Note |
        /// |--------|-----------------|------|
        /// | @ref NkThread | Thread natif avec API C++ moderne | Wrapper pthread/Win32 |
        /// | @ref NkThreadLocal<T> | Données isolées par thread | Zero overhead sync |
        /// | @ref NkThreadPool | Parallélisme automatique de tâches | Work-stealing |
        ///
        /// ### Patterns asynchrones
        /// | Classe | Usage recommandé | Équivalent STL |
        /// |--------|-----------------|----------------|
        /// | @ref NkPromise<T> | Producteur de résultat async | std::promise |
        /// | @ref NkFuture<T> | Consommateur de résultat async | std::future |
        /// | @ref NkScopedLock<T> | Guard RAII générique | std::lock_guard |
        ///
        /// @section threading_best_practices Bonnes pratiques
        /// - ✅ Toujours utiliser les guards RAII (@ref NkScopedLock, @ref NkReadLock, @ref NkWriteLock)
        /// - ✅ Préférer @ref NkSpinLock pour sections <100 cycles, @ref NkMutex sinon
        /// - ✅ Utiliser @ref NkConditionVariable::WaitUntil avec prédicat pour éviter les spurious wakeups
        /// - ✅ Choisir un grainSize adapté dans @ref NkThreadPool::ParallelFor (100-1000 typiquement)
        /// - ✅ Ne jamais appeler Join() depuis le thread lui-même (deadlock garanti)
        /// - ✅ Utiliser @ref NkLatch pour synchronisation de completion, @ref NkBarrier pour phases itératives
        /// - ✅ Préférer @ref NkReadLock pour les lectures, @ref NkWriteLock uniquement pour modifications
        /// - ❌ Éviter les mutex réentrants sauf nécessité absolue (overhead 2-3x)
        /// - ❌ Ne pas utiliser thread-local pour la communication inter-threads (isolation garantie)
        /// - ❌ Ne pas garder un lock plus longtemps que nécessaire (minimiser la section critique)
        /// - ❌ Éviter les conversions read→write sur le même reader-writer lock (risque de deadlock)
        ///
        /// @section threading_thread_safety Thread-safety guarantees
        /// - Toutes les classes publiques sont thread-safe pour leurs opérations documentées
        /// - Les méthodes const peuvent être appelées simultanément depuis plusieurs threads
        /// - Les méthodes non-const nécessitent une synchronisation externe si appelées concurremment
        /// - Les guards RAII garantissent la libération même en cas d'exception
        /// - @ref NkReaderWriterLock permet lectures concurrentes mais écritures exclusives
        ///
        /// @section threading_platform_support Support multiplateforme
        /// - Windows : utilise SRWLock, CONDITION_VARIABLE, CreateThread, CRITICAL_SECTION
        /// - Linux/POSIX : utilise pthread_mutex, pthread_cond, pthread_create, pthread_rwlock
        /// - macOS/iOS : support natif via pthread avec adaptations spécifiques
        /// - Android : support via pthread avec optimisations Bionic
        ///
        /// @warning L'ordre d'inclusion de ce header est important : il inclut NkThreadingApi.h
        /// en premier pour définir les macros d'export. Ne pas inclure les headers individuels
        /// avant ce fichier pour éviter les conflits de définition.
        namespace threading {

            // -----------------------------------------------------------------
            // CONSTANTES ET CONFIGURATIONS GLOBALES DU MODULE
            // -----------------------------------------------------------------

            /// @brief Version majeure du module NKThreading.
            /// @note Incrémentée pour les changements incompatibles d'API.
            constexpr nk_uint32 kVersionMajor = 2;

            /// @brief Version mineure du module NKThreading.
            /// @note Incrémentée pour les nouvelles fonctionnalités rétro-compatibles.
            constexpr nk_uint32 kVersionMinor = 0;

            /// @brief Version de patch du module NKThreading.
            /// @note Incrémentée pour les corrections de bugs sans changement d'API.
            constexpr nk_uint32 kVersionPatch = 0;

            /// @brief Version complète encodée en hexadécimal (0xMMmmpp).
            /// @example 0x020000 = version 2.0.0
            constexpr nk_uint32 kVersionEncoded =
                (kVersionMajor << 16) | (kVersionMinor << 8) | kVersionPatch;

            /// @brief Chaîne de version lisible pour logging/debugging.
            /// @return Chaîne formatée "M.m.p" (ex: "2.0.0").
            [[nodiscard]] inline const nk_char* GetVersionString() noexcept
            {
                return "2.0.0";
            }

            // -----------------------------------------------------------------
            // FONCTIONS UTILITAIRES GLOBALES (hors classes)
            // -----------------------------------------------------------------

            /// @brief Cède volontairement le temps CPU au scheduler.
            /// @note Utile dans les boucles d'attente active pour réduire la consommation CPU.
            /// @note Windows : SwitchToThread(), POSIX : sched_yield()
            /// @note Plus efficace qu'un sleep(0) car cède immédiatement le quantum restant.
            inline void Yield() noexcept
            {
                #if defined(NKENTSEU_PLATFORM_WINDOWS)
                    ::SwitchToThread();
                #else
                    (void)::sched_yield();
                #endif
            }

            /// @brief Récupère le nombre de cores logiques disponibles.
            /// @return Nombre de cores détectés (>= 1).
            /// @note Thread-safe : lecture de l'info système, pas d'état partagé.
            /// @note Utile pour configurer automatiquement NkThreadPool(0).
            [[nodiscard]] inline nk_uint32 GetHardwareConcurrency() noexcept
            {
                return platform::NkGetLogicalCoreCount();
            }

            /// @brief Pause instruction hint pour optimiser les boucles de spin.
            /// @note x86/x64 : PAUSE, ARM : YIELD, autres : fallback NOP.
            /// @note Réduit la contention mémoire et la consommation power pendant l'attente active.
            /// @note Utilisé en interne par NkSpinLock, exposé pour les implémentations custom.
            inline void Pause() noexcept
            {
                #if defined(NKENTSEU_ARCH_X86) || defined(NKENTSEU_ARCH_X86_64)
                    __builtin_ia32_pause();
                #elif defined(NKENTSEU_ARCH_ARM) || defined(NKENTSEU_ARCH_ARM64)
                    __asm__ __volatile__("yield" ::: "memory");
                #endif
                // Fallback : rien sur les architectures non-supportées
            }

        } // namespace threading

        // -------------------------------------------------------------------------
        // ALIAS GLOBAUX DE COMPATIBILITÉ (namespace nkentseu::)
        // -------------------------------------------------------------------------
        // Ces alias permettent d'accéder aux types principaux depuis le namespace
        // parent sans qualification threading::, pour compatibilité avec le code legacy.
        // Nouveau code recommandé : utiliser nkentseu::threading:: directement.
        // =====================================================================

        /// @brief Alias global pour NkMutex (compatibilité legacy).
        /// @deprecated Utiliser nkentseu::threading::NkMutex directement.
        using NkMutex = threading::NkMutex;

        /// @brief Alias global pour NkRecursiveMutex (compatibilité legacy).
        /// @deprecated Utiliser nkentseu::threading::NkRecursiveMutex directement.
        using NkRecursiveMutex = threading::NkRecursiveMutex;

        /// @brief Alias global pour NkSpinLock (compatibilité legacy).
        /// @deprecated Utiliser nkentseu::threading::NkSpinLock directement.
        using NkSpinLock = threading::NkSpinLock;

        /// @brief Alias global pour NkConditionVariable (compatibilité legacy).
        /// @deprecated Utiliser nkentseu::threading::NkConditionVariable directement.
        using NkConditionVariable = threading::NkConditionVariable;

        /// @brief Alias global pour NkSemaphore (compatibilité legacy).
        /// @deprecated Utiliser nkentseu::threading::NkSemaphore directement.
        using NkSemaphore = threading::NkSemaphore;

        /// @brief Alias global pour NkSharedMutex (compatibilité legacy).
        /// @deprecated Utiliser nkentseu::threading::NkSharedMutex directement.
        using NkSharedMutex = threading::NkSharedMutex;

        /// @brief Alias global pour NkBarrier (compatibilité legacy).
        /// @deprecated Utiliser nkentseu::threading::NkBarrier directement.
        using NkBarrier = threading::NkBarrier;

        /// @brief Alias global pour NkLatch (compatibilité legacy).
        /// @deprecated Utiliser nkentseu::threading::NkLatch directement.
        using NkLatch = threading::NkLatch;

        /// @brief Alias global pour NkEvent (compatibilité legacy).
        /// @deprecated Utiliser nkentseu::threading::NkEvent directement.
        using NkEvent = threading::NkEvent;

        /// @brief Alias global pour NkReaderWriterLock (compatibilité legacy).
        /// @deprecated Utiliser nkentseu::threading::NkReaderWriterLock directement.
        using NkReaderWriterLock = threading::NkReaderWriterLock;

        /// @brief Alias global pour NkReadLock (compatibilité legacy).
        /// @deprecated Utiliser nkentseu::threading::NkReadLock directement.
        using NkReadLock = threading::NkReadLock;

        /// @brief Alias global pour NkWriteLock (compatibilité legacy).
        /// @deprecated Utiliser nkentseu::threading::NkWriteLock directement.
        using NkWriteLock = threading::NkWriteLock;

        /// @brief Alias global pour NkThread (compatibilité legacy).
        /// @deprecated Utiliser nkentseu::threading::NkThread directement.
        using NkThread = threading::NkThread;

        /// @brief Alias global template pour NkThreadLocal (compatibilité legacy).
        /// @deprecated Utiliser nkentseu::threading::NkThreadLocal<T> directement.
        /// @tparam T Type de la valeur stockée en thread-local.
        template<typename T>
        using NkThreadLocal = threading::NkThreadLocal<T>;

        /// @brief Alias global pour NkThreadPool (compatibilité legacy).
        /// @deprecated Utiliser nkentseu::threading::NkThreadPool directement.
        using NkThreadPool = threading::NkThreadPool;

        /// @brief Alias global pour Task (compatibilité legacy).
        /// @deprecated Utiliser nkentseu::threading::Task directement.
        using Task = threading::Task;

        /// @brief Alias global template pour NkFuture (compatibilité legacy).
        /// @deprecated Utiliser nkentseu::threading::NkFuture<T> directement.
        /// @tparam T Type de la valeur retournée par le future.
        template<typename T>
        using NkFuture = threading::NkFuture<T>;

        /// @brief Alias global template pour NkPromise (compatibilité legacy).
        /// @deprecated Utiliser nkentseu::threading::NkPromise<T> directement.
        /// @tparam T Type de la valeur produite par le promise.
        template<typename T>
        using NkPromise = threading::NkPromise<T>;

        /// @brief Alias global template pour NkScopedLock (compatibilité legacy).
        /// @deprecated Utiliser nkentseu::threading::NkScopedLock<TMutex> directement.
        /// @tparam TMutex Type du mutex à protéger (doit implémenter Lock()/Unlock()).
        template<typename TMutex>
        using NkScopedLock = threading::NkScopedLock<TMutex>;

        /// @brief Alias global pour NkLockGuard (compatibilité legacy).
        /// @deprecated Utiliser nkentseu::threading::NkLockGuard directement.
        using NkLockGuard = threading::NkLockGuard;

        /// @brief Alias global pour Yield() (compatibilité legacy).
        /// @deprecated Utiliser nkentseu::threading::Yield() directement.
        inline void Yield() noexcept { threading::Yield(); }

        /// @brief Alias global pour Pause() (compatibilité legacy).
        /// @deprecated Utiliser nkentseu::threading::Pause() directement.
        inline void Pause() noexcept { threading::Pause(); }

        /// @brief Alias global pour GetHardwareConcurrency() (compatibilité legacy).
        /// @deprecated Utiliser nkentseu::threading::GetHardwareConcurrency() directement.
        [[nodiscard]] inline nk_uint32 GetHardwareConcurrency() noexcept
        {
            return threading::GetHardwareConcurrency();
        }

    } // namespace nkentseu

#endif // __NKENTSEU_THREADING_H__

// =============================================================================
// EXEMPLES D'UTILISATION COMPLÈTE (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Include unique + usage basique complet
    // ---------------------------------------------------------------------
    // Fichier : main.cpp
    #include <NKThreading/NKThreading.h>  // ✅ Un seul include pour tout le module
    using namespace nkentseu::threading;   // ✅ Namespace moderne recommandé

    int main()
    {
        // 1. Mutex + RAII guard
        NkMutex mutex;
        int counter = 0;
        auto increment = [&mutex, &counter]() {
            NkScopedLock guard(mutex);
            ++counter;
        };

        // 2. Thread pool pour parallélisation
        NkThreadPool pool(4);
        pool.ParallelFor(1000, [](size_t i) {
            ProcessData(i);
        }, 100);
        pool.Join();

        // 3. Latch pour synchronisation de completion
        NkLatch initLatch(3);
        // ... threads appellent initLatch.CountDown() ...
        initLatch.Wait();  // Attendre que les 3 initialisations soient terminées

        // 4. Reader-writer lock pour cache
        NkReaderWriterLock cacheLock;
        auto readCache = [&cacheLock]() {
            NkReadLock lock(cacheLock);  // Multiple readers OK
            return LookupCache();
        };
        auto writeCache = [&cacheLock](const Value& v) {
            NkWriteLock lock(cacheLock);  // Exclusive access
            UpdateCache(v);
        };

        // 5. Event pour signal de shutdown
        NkEvent shutdownEvent(true);  // ManualReset
        // ... threads vérifient shutdownEvent.IsSignaled() ...
        shutdownEvent.Set();  // Signaler l'arrêt à tous

        return 0;
    }

    // ---------------------------------------------------------------------
    // Exemple 2 : Pattern producteur-consommateur avec Barrier
    // ---------------------------------------------------------------------
    #include <NKThreading/NKThreading.h>
    #include <queue>

    using namespace nkentseu::threading;

    class BarrieredPipeline {
    public:
        explicit BarrieredPipeline(nk_uint32 numStages, nk_uint32 workersPerStage)
            : mNumStages(numStages)
        {
            // Une barrière par transition entre stages
            for (nk_uint32 i = 0; i < numStages - 1; ++i) {
                mBarriers.emplace_back(workersPerStage);
            }
        }

        void RunStage(nk_uint32 stageId, std::function<void(Data&)> processor)
        {
            Data data;
            InitializeData(data);

            for (nk_uint32 iteration = 0; iteration < mNumIterations; ++iteration) {
                processor(data);  // Traitement du stage courant

                // Synchroniser avant de passer au stage suivant
                if (stageId < mNumStages - 1) {
                    mBarriers[stageId].Wait();
                }
            }
        }

    private:
        nk_uint32 mNumStages;
        nk_uint32 mNumIterations = 100;
        std::vector<NkBarrier> mBarriers;
    };

    // ---------------------------------------------------------------------
    // Exemple 3 : Synchronisation d'initialisation avec Latch
    // ---------------------------------------------------------------------
    #include <NKThreading/NKThreading.h>

    void ParallelServiceStartup()
    {
        constexpr nk_uint32 kNumServices = 5;
        NkLatch startupLatch(kNumServices);  // Attendre 5 initialisations
        std::vector<NkThread> workers;

        // Lancer l'initialisation parallèle des services
        for (nk_uint32 i = 0; i < kNumServices; ++i) {
            workers.emplace_back([&startupLatch, i]() {
                if (InitializeService(i)) {
                    startupLatch.CountDown();  // Signaler la completion
                }
            });
        }

        // Attendre que tous les services soient initialisés (avec timeout)
        if (!startupLatch.Wait(30000)) {  // 30 secondes max
            NK_LOG_ERROR("Service startup timeout - {} services pending",
                        startupLatch.GetCount());
            // Gérer l'erreur : fallback ou abort
        }

        // Tous les services sont prêts
        NK_LOG_INFO("All {} services initialized", kNumServices);

        for (auto& t : workers) {
            t.Join();
        }
    }

    // ---------------------------------------------------------------------
    // Exemple 4 : Signal d'événement avec timeout
    // ---------------------------------------------------------------------
    #include <NKThreading/NKThreading.h>

    bool WaitForInitialization(NkEvent& initEvent, nk_uint32 timeoutMs)
    {
        if (initEvent.Wait(timeoutMs)) {
            return true;  // Initialisation complétée dans le délai
        } else {
            NK_LOG_WARNING("Initialization timeout after {}ms", timeoutMs);
            return false;  // Timeout : gérer l'erreur ou fallback
        }
    }

    // Usage avec ManualReset (persistant) :
    // NkEvent initComplete(true);  // ManualReset
    // initComplete.Reset();  // Commencer non-signalé
    // // ... thread d'initialisation ...
    // initComplete.Set();  // Signal persistant pour tous les waiters

    // Usage avec AutoReset (consommable) :
    // NkEvent dataReady(false);  // AutoReset (défaut)
    // // ... producteur ...
    // dataReady.Set();  // Réveille un seul consommateur

    // ---------------------------------------------------------------------
    // Exemple 5 : Cache thread-safe avec ReaderWriterLock
    // ---------------------------------------------------------------------
    #include <NKThreading/NKThreading.h>
    #include <unordered_map>

    using namespace nkentseu::threading;

    template<typename Key, typename Value>
    class ConcurrentCache {
    public:
        Optional<Value> Get(const Key& key) const
        {
            NkReadLock readLock(mRwLock);  // Multiple readers OK
            auto it = mCache.find(key);
            return (it != mCache.end()) ? Optional<Value>(it->second) : nullopt;
        }

        bool Insert(const Key& key, const Value& value)
        {
            NkWriteLock writeLock(mRwLock);  // Exclusive access
            auto [it, inserted] = mCache.emplace(key, value);
            return inserted;
        }

        void Clear()
        {
            NkWriteLock lock(mRwLock);
            mCache.clear();
        }

        size_t Size() const
        {
            NkReadLock lock(mRwLock);
            return mCache.size();
        }

    private:
        mutable NkReaderWriterLock mRwLock;
        mutable std::unordered_map<Key, Value> mCache;
    };

    // ---------------------------------------------------------------------
    // Exemple 6 : Monitoring et métriques de performance
    // ---------------------------------------------------------------------
    #include <NKThreading/NKThreading.h>
    #include <NKCore/NkLogger.h>

    void MonitorThreadPool(NkThreadPool& pool, const char* poolName)
    {
        nk_uint32 workers = pool.GetNumWorkers();
        nk_size queued = pool.GetQueueSize();
        nk_uint64 completed = pool.GetTasksCompleted();

        NK_LOG_INFO("[{}] Workers: {}, Queued: {}, Completed: {}",
                   poolName, workers, queued, completed);

        // Détection de goulot d'étranglement
        if (queued > workers * 10) {
            NK_LOG_WARNING("[{}] High queue depth: consider adding workers", poolName);
        }
    }

    void MonitorLatch(NkLatch& latch, const char* operationName)
    {
        while (!latch.IsReady()) {
            nk_uint32 remaining = latch.GetCount();
            NK_LOG_DEBUG("{}: {} units remaining", operationName, remaining);
            NkThread::Yield();  // Céder le CPU pendant le polling
        }
        NK_LOG_INFO("{}: completed", operationName);
    }

    // ---------------------------------------------------------------------
    // Exemple 7 : Migration depuis l'ancien namespace legacy
    // ---------------------------------------------------------------------
    // AVANT (code utilisant les aliases legacy) :
    /*
    #include <NKThreading/NKThreading.h>
    using namespace nkentseu::entseu;  // ❌ Ancien namespace (déprécié)

    void LegacyCode() {
        NkMutex mutex;
        NkThreadPool pool(4);
        NkLatch latch(3);  // Résolu via alias vers threading::NkLatch
    }
    *\/

    // APRÈS (code moderne recommandé) :
    /\*
    #include <NKThreading/NKThreading.h>
    using namespace nkentseu::threading;  // ✅ Nouveau namespace

    void ModernCode() {
        NkMutex mutex;
        NkThreadPool pool(4);
        NkLatch latch(3);  // Accès direct, pas d'alias
    }
    *\/

    // Les deux fonctionnent grâce aux alias de compatibilité,
    // mais le nouveau code doit utiliser nkentseu::threading directement.

    // ---------------------------------------------------------------------
    // Exemple 8 : Configuration CMake pour le module complet
    // ---------------------------------------------------------------------
    # CMakeLists.txt - Projet utilisant NKThreading

    cmake_minimum_required(VERSION 3.15)
    project(MyApp VERSION 1.0.0 LANGUAGES CXX)

    # Options de build
    option(USE_SHARED_THREADING "Link against shared NKThreading library" ON)
    option(ENABLE_THREADING_DEBUG "Enable debug logging in threading module" OFF)

    # Trouver le package NKThreading (via config ou submodule)
    find_package(NKThreading REQUIRED CONFIG)

    # Créer l'exécutable
    add_executable(myapp
        src/main.cpp
        src/worker.cpp
    )

    # Lier avec NKThreading
    if(USE_SHARED_THREADING)
        target_link_libraries(myapp PRIVATE NKThreading::NKThreading)
    else()
        target_link_libraries(myapp PRIVATE NKThreading::NKThreadingStatic)
    endif()

    # Options de compilation
    target_compile_features(myapp PRIVATE cxx_std_17)

    if(ENABLE_THREADING_DEBUG)
        target_compile_definitions(myapp PRIVATE NKENTSEU_THREADING_DEBUG)
        target_compile_options(myapp PRIVATE -g -O0)
    else()
        target_compile_options(myapp PRIVATE -O3 -DNDEBUG)
    endif()

    # Optimisations plateforme-spécifiques
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(myapp PRIVATE
            -pthread
            -ftls-model=global-dynamic
        )
    endif()

    // ---------------------------------------------------------------------
    // Exemple 9 : Testing unitaire avec le module complet
    // ---------------------------------------------------------------------
    #include <gtest/gtest.h>
    #include <NKThreading/NKThreading.h>
    #include <atomic>
    #include <vector>

    using namespace nkentseu::threading;

    TEST(NKThreadingModule, LatchSynchronization)
    {
        constexpr nk_uint32 kCount = 4;
        NkLatch latch(kCount);
        std::atomic<nk_uint32> signaled{0};

        std::vector<NkThread> workers;
        for (nk_uint32 i = 0; i < kCount; ++i) {
            workers.emplace_back([&latch, &signaled]() {
                signaled.fetch_add(1);
                latch.CountDown();
            });
        }

        NkThread waiter([&latch]() {
            EXPECT_TRUE(latch.Wait(5000));  // Doit retourner dans le délai
        });

        for (auto& t : workers) t.Join();
        waiter.Join();

        EXPECT_EQ(signaled.load(), kCount);
        EXPECT_TRUE(latch.IsReady());
    }

    TEST(NKThreadingModule, ReaderWriterLockConcurrentReads)
    {
        NkReaderWriterLock rwlock;
        std::atomic<nk_uint32> activeReaders{0};
        std::atomic<nk_uint32> maxConcurrent{0};

        constexpr nk_uint32 kNumReaders = 8;
        std::vector<NkThread> readers;

        for (nk_uint32 i = 0; i < kNumReaders; ++i) {
            readers.emplace_back([&rwlock, &activeReaders, &maxConcurrent]() {
                NkReadLock lock(rwlock);
                nk_uint32 current = activeReaders.fetch_add(1) + 1;
                maxConcurrent = std::max(maxConcurrent.load(), current);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                activeReaders.fetch_sub(1);
            });
        }

        for (auto& t : readers) t.Join();

        // Tous les readers ont pu s'exécuter concurrentiellement
        EXPECT_EQ(maxConcurrent.load(), kNumReaders);
    }

    TEST(NKThreadingModule, EventManualResetPersistence)
    {
        NkEvent event(true);  // ManualReset
        event.Reset();

        std::atomic<nk_uint32> signaledCount{0};
        std::vector<NkThread> waiters;

        for (int i = 0; i < 4; ++i) {
            waiters.emplace_back([&event, &signaledCount]() {
                if (event.Wait(1000)) {
                    signaledCount.fetch_add(1);
                }
            });
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        event.Set();  // ManualReset = persistant

        for (auto& t : waiters) t.Join();

        EXPECT_EQ(signaledCount.load(), 4u);  // Tous ont vu le signal persistant
    }

    // ---------------------------------------------------------------------
    // Exemple 10 : Bonnes pratiques récapitulatives
    // ---------------------------------------------------------------------
    // ✅ Checklist avant de soumettre du code utilisant NKThreading :

    // 1. Synchronisation de base
    // □ Utiliser NkScopedLock plutôt que Lock()/Unlock() manuels
    // □ Choisir NkSpinLock uniquement pour sections <100 cycles
    // □ Utiliser WaitUntil avec prédicat pour éviter les spurious wakeups

    // 2. Synchronisation de phase
    // □ NkBarrier pour phases itératives réutilisables
    // □ NkLatch pour completion unique de N tâches
    // □ NkEvent pour signaux booléens persistants ou consommables

    // 3. Accès concurrent
    // □ NkReadLock pour lectures (partagé avec autres readers)
    // □ NkWriteLock pour écritures (exclusif, exclut tous)
    // □ Éviter les conversions read→write sur le même lock

    // 4. Parallélisme
    // □ Choisir un grainSize adapté dans ParallelFor (100-1000 typiquement)
    // □ Vérifier que la fonction parallélisée est thread-safe
    // □ Utiliser Join() avant d'accéder aux résultats partagés

    // 5. Gestion des threads
    // □ Toujours Join() ou Detach() avant destruction d'un NkThread
    // □ Ne pas appeler Join() depuis le thread lui-même (deadlock)
    // □ Utiliser NkThreadLocal pour données thread-specific

    // ❌ Anti-patterns à éviter absolument :
    // □ Lock nested sans mutex réentrant = deadlock garanti
    // □ ParallelFor avec grainSize=1 sur grande boucle = overhead massif
    // □ Thread-local pour communication inter-threads = bug garanti
    // □ Oublier de libérer un lock sur exception = deadlock potentiel
    // □ Wait() infini sans garantie de signal = hang potentiel

    // ---------------------------------------------------------------------
    // Exemple 11 : Debugging et instrumentation en mode développement
    // ---------------------------------------------------------------------
    #ifdef NK_DEBUG
    #include <NKThreading/NKThreading.h>
    #include <NKCore/NkLogger.h>

    // Wrapper instrumenté pour logging des opérations de synchronisation
    class DebugLatch : public NkLatch {
    public:
        explicit DebugLatch(nk_uint32 initialCount, const nk_char* name = nullptr)
            : NkLatch(initialCount), mName(name ? name : "Unnamed") {}

        void CountDown(nk_uint32 count = 1) noexcept
        {
            NK_LOG_DEBUG("Latch[{}]: CountDown({}) called, remaining={}",
                        mName, count, GetCount());
            NkLatch::CountDown(count);
        }

        nk_bool Wait(nk_int32 timeoutMs = -1) noexcept
        {
            auto tid = NkThread::GetCurrentThreadId();
            NK_LOG_DEBUG("Latch[{}]: Thread {} calling Wait(timeout={}ms)",
                        mName, tid, timeoutMs);
            bool result = NkLatch::Wait(timeoutMs);
            NK_LOG_DEBUG("Latch[{}]: Thread {} Wait() returned {}",
                        mName, tid, result ? "true" : "false");
            return result;
        }

    private:
        const nk_char* mName;
    };

    // Usage conditionnel :
    #ifdef NK_DEBUG
        using ProductionLatch = DebugLatch;
    #else
        using ProductionLatch = NkLatch;
    #endif
    #endif  // NK_DEBUG

    // ---------------------------------------------------------------------
    // Exemple 12 : Référence rapide des types principaux
    // ---------------------------------------------------------------------
    /\*
    🔹 PRIMITIVES DE BASE
    ├─ NkMutex              : exclusion mutuelle simple
    ├─ NkRecursiveMutex     : mutex réentrant (usage spécifique)
    └─ NkSpinLock           : spin-lock haute performance

    🔹 SYNCHRONISATION AVANCÉE
    ├─ NkConditionVariable  : attente sur prédicat
    ├─ NkSemaphore          : compteur de ressources
    └─ NkSharedMutex        : reader-writer lock (alias)

    🔹 SYNCHRONISATION DE PHASE
    ├─ NkBarrier            : synchronisation de phase itérative (réutilisable)
    ├─ NkLatch              : completion de N tâches (usage unique)
    └─ NkEvent              : signal booléen (ManualReset/AutoReset)

    🔹 ACCÈS CONCURRENT
    ├─ NkReaderWriterLock   : verrou lecteur-rédacteur principal
    ├─ NkReadLock           : guard RAII pour lecture (partagé)
    └─ NkWriteLock          : guard RAII pour écriture (exclusif)

    🔹 GESTION DE THREADS
    ├─ NkThread             : wrapper thread natif
    ├─ NkThreadLocal<T>     : stockage isolé par thread
    └─ NkThreadPool         : pool avec work-stealing

    🔹 PATTERNS ASYNC
    ├─ NkPromise<T>         : producteur de résultat
    ├─ NkFuture<T>          : consommateur de résultat
    └─ NkScopedLock<T>      : guard RAII générique

    🔹 UTILITAIRES
    ├─ Yield()              : céder le CPU au scheduler
    ├─ Pause()              : hint d'optimisation pour spin
    └─ GetHardwareConcurrency() : détection du nombre de cores

    🔹 CONSTANTES
    ├─ kVersionMajor/Minor/Patch : version du module
    ├─ kVersionEncoded      : version encodée 0xMMmmpp
    └─ GetVersionString()   : version lisible "M.m.p"
    *\/

    // ---------------------------------------------------------------------
    // Exemple 13 : Guide de choix entre primitives de synchronisation
    // ---------------------------------------------------------------------
    /\*
    ┌─────────────────────────────────────────────────────────────┐
    │ QUELLE PRIMITIVE CHOISIR ?                                   │
    ├─────────────────────────────────────────────────────────────┤
    │                                                             │
    │ Problème                          → Solution recommandée   │
    │ ─────────────────────────────────────────────────────     │
    │                                                             │
    │ Protéger une variable partagée   → NkMutex + NkScopedLock  │
    │ Section critique <100 cycles     → NkSpinLock              │
    │ API imbriquée avec re-lock       → NkRecursiveMutex        │
    │ Attendre sur condition complexe  → NkConditionVariable     │
    │ Pool de N ressources             → NkSemaphore             │
    │ Lectures fréquentes, écritures   → NkReaderWriterLock      │
    │   rares                                                    │
    │                                                             │
    │ Synchroniser N threads sur       → NkBarrier (réutilisable)│
    │   un point de phase              → NkLatch (usage unique)  │
    │ Attendre completion de tâches    → NkLatch                 │
    │ Signal booléen persistant        → NkEvent (ManualReset)   │
    │ Signal booléen consommable       → NkEvent (AutoReset)     │
    │                                                             │
    │ Thread natif avec API C++        → NkThread                │
    │ Données isolées par thread       → NkThreadLocal<T>        │
    │ Parallélisme automatique         → NkThreadPool            │
    │ Résultat asynchrone              → NkFuture/NkPromise      │
    │                                                             │
    └─────────────────────────────────────────────────────────────┘

    Règles empiriques :
    • Commencer simple : NkMutex + NkScopedLock couvre 80% des cas
    • Optimiser seulement si profiling montre un goulot
    • Documenter clairement les invariants de thread-safety
    • Tester avec ThreadSanitizer en mode debug
    *\/
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================