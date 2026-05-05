//
// NkBarrier.h
// =============================================================================
// Description :
//   Primitive de synchronisation de type barrière pour coordonner N threads
//   sur un point de rendez-vous commun. Tous les threads appelant Wait()
//   bloquent jusqu'à ce que le nombre configuré de threads ait atteint la barrière.
//
// Caractéristiques :
//   - Synchronisation de phase : tous les threads progressent ensemble
//   - Réutilisable : Reset() ou nouvelle phase automatique après completion
//   - Thread-safe : protection par mutex + condition variable natives
//   - Retour d'information : le dernier thread arrive reçoit true pour actions post-sync
//   - Garantie noexcept sur toutes les opérations publiques
//
// Algorithmes implémentés :
//   - Wait() : comptage des arrivées + notification via condition variable
//   - Gestion des phases : mCurrentPhase incrémente pour distinguer les itérations
//   - Reset() : réinitialisation forcée pour réutilisation immédiate
//
// Types disponibles :
//   NkBarrier : barrière réutilisable pour synchronisation de N threads
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_THREADING_BARRIER_H__
#define __NKENTSEU_THREADING_BARRIER_H__

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKCore/NkTypes.h"
    #include "NKCore/Assert/NkAssert.h"
    #include "NKThreading/NkMutex.h"
    #include "NKThreading/NkConditionVariable.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : NAMESPACE PRINCIPAL
    // -------------------------------------------------------------------------
    namespace nkentseu {

        // =====================================================================
        // SOUS-NAMESPACE : threading
        // =====================================================================
        namespace threading {

            // =================================================================
            // CLASSE : NkBarrier
            // =====================================================================
            // Barrière de synchronisation pour coordonner l'avancement de N threads.
            //
            // Représentation interne :
            //   - mCount : nombre de threads ayant atteint la barrière dans la phase courante
            //   - mCurrentPhase : identifiant de phase pour distinguer les itérations successives
            //   - mMutex + mCondVar : primitives de synchronisation natives
            //
            // Invariant de production :
            //   - 0 <= mCount <= mNumThreads à tout instant
            //   - mCurrentPhase incrémente monotone à chaque completion de barrière
            //   - Après Wait() retour, tous les threads ont atteint le point de sync
            //
            // Modèle d'utilisation typique :
            //   // Phase 1 : tous les threads préparent des données
            //   PrepareData();
            //   barrier.Wait();  // Attendre que tous aient fini la préparation
            //
            //   // Phase 2 : tous les threads consomment les données préparées
            //   ConsumeData();
            //   barrier.Wait();  // Synchroniser avant la phase suivante
            //
            // Cas d'usage :
            //   - Simulation parallèle avec étapes synchronisées (time-stepping)
            //   - Pipeline de traitement avec barrières entre les stages
            //   - Tests parallèles nécessitant un démarrage simultané
            //   - Algorithmes BSP (Bulk Synchronous Parallel)
            // =================================================================
            class NKENTSEU_THREADING_CLASS_EXPORT NkBarrier {
            public:

                // -------------------------------------------------------------
                // CONSTRUCTEURS ET RÈGLES DE COPIE
                // -------------------------------------------------------------

                /// @brief Constructeur : initialise la barrière pour N threads.
                /// @param numThreads Nombre de threads à synchroniser (doit être > 0).
                /// @note Garantie noexcept : aucune allocation ni exception possible.
                /// @note NKENTSEU_ASSERT vérifie numThreads > 0 en mode debug.
                /// @note La barrière est immédiatement utilisable après construction.
                explicit NkBarrier(nk_uint32 numThreads) noexcept;

                /// @brief Constructeur de copie supprimé : barrière non-copiable.
                /// @note Une barrière représente un point de sync partagé unique.
                NkBarrier(const NkBarrier&) = delete;

                /// @brief Opérateur d'affectation supprimé : barrière non-affectable.
                NkBarrier& operator=(const NkBarrier&) = delete;

                // -------------------------------------------------------------
                // API DE SYNCHRONISATION
                // -------------------------------------------------------------

                /// @brief Attend que tous les threads atteignent la barrière.
                /// @return true pour le dernier thread arrivé, false pour les autres.
                /// @note Le thread qui reçoit true peut effectuer des actions post-sync
                ///       (ex: agrégation de résultats, lancement de la phase suivante).
                /// @note Thread-safe : peut être appelé simultanément par tous les threads.
                /// @note Bloquant : le thread est suspendu jusqu'à completion de la phase.
                /// @warning Ne pas appeler avec un nombre de threads différent de celui
                ///          configuré au constructeur = deadlock garanti.
                [[nodiscard]] nk_bool Wait() noexcept;

                /// @brief Réinitialise la barrière pour une nouvelle itération.
                /// @note Force la completion de la phase courante même si mCount < mNumThreads.
                /// @note Notifie tous les threads en attente pour qu'ils reprennent l'exécution.
                /// @note Utile pour aborter une synchronisation ou forcer l'avancement.
                /// @warning Peut causer des threads à reprendre avec des données incomplètes :
                ///          utiliser uniquement quand la cohérence des données est garantie.
                void Reset() noexcept;

            private:

                // -------------------------------------------------------------
                // MEMBRES PRIVÉS (état interne de la barrière)
                // -------------------------------------------------------------

                nk_uint32 mNumThreads;           ///< Nombre de threads à synchroniser (fixe)
                nk_uint32 mCurrentPhase;         ///< Identifiant de phase courante (incrémente à chaque completion)
                nk_uint32 mCount;                ///< Nombre de threads ayant atteint la barrière dans la phase courante
                mutable NkMutex mMutex;          ///< Mutex pour protéger l'accès concurrent aux membres
                mutable NkConditionVariable mCondVar;  ///< Condition variable pour l'attente efficace des threads

            };

        }
    }

#endif

// =============================================================================
// EXEMPLES D'UTILISATION (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Synchronisation de phase simple (pattern classique)
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkBarrier.h>
    using namespace nkentseu::threading;

    void ParallelSimulation()
    {
        constexpr nk_uint32 kNumThreads = 4;
        constexpr nk_uint32 kNumSteps = 100;

        NkBarrier barrier(kNumThreads);
        std::vector<NkThread> workers;

        // Lancer les threads workers
        for (nk_uint32 i = 0; i < kNumThreads; ++i) {
            workers.emplace_back([&barrier, i]() {
                for (nk_uint32 step = 0; step < kNumSteps; ++step) {
                    // Phase 1 : calcul local
                    ComputeLocalData(i, step);

                    // Synchroniser avant d'échanger les données de bordure
                    barrier.Wait();

                    // Phase 2 : échange des données de bordure avec voisins
                    ExchangeBoundaryData(i, step);

                    // Synchroniser avant la prochaine itération
                    barrier.Wait();
                }
            });
        }

        // Attendre que tous les threads terminent
        for (auto& t : workers) {
            t.Join();
        }
    }

    // ---------------------------------------------------------------------
    // Exemple 2 : Utilisation du retour "dernier thread" pour agrégation
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkBarrier.h>
    #include <atomic>

    void ParallelReduction()
    {
        constexpr nk_uint32 kNumThreads = 8;
        std::atomic<double> globalSum{0.0};
        std::vector<double> localSums(kNumThreads, 0.0);

        NkBarrier barrier(kNumThreads);
        std::vector<NkThread> workers;

        for (nk_uint32 i = 0; i < kNumThreads; ++i) {
            workers.emplace_back([&barrier, &localSums, &globalSum, i]() {
                // Calcul local
                localSums[i] = ComputePartialSum(i);

                // Synchroniser avant l'agrégation
                bool isLast = barrier.Wait();

                // Le dernier thread agrège les résultats
                if (isLast) {
                    double total = 0.0;
                    for (double partial : localSums) {
                        total += partial;
                    }
                    globalSum.store(total);
                }

                // Synchroniser après l'agrégation
                barrier.Wait();

                // Tous les threads peuvent maintenant utiliser globalSum
                UseResult(globalSum.load());
            });
        }

        for (auto& t : workers) {
            t.Join();
        }
    }

    // ---------------------------------------------------------------------
    // Exemple 3 : Démarrage simultané de threads (barrière de lancement)
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkBarrier.h>

    void SynchronizedStart()
    {
        constexpr nk_uint32 kNumWorkers = 4;
        NkBarrier startBarrier(kNumWorkers + 1);  // +1 pour le thread principal

        std::vector<NkThread> workers;

        // Lancer les workers qui attendent le signal de départ
        for (nk_uint32 i = 0; i < kNumWorkers; ++i) {
            workers.emplace_back([&startBarrier, i]() {
                startBarrier.Wait();  // Attendre le "GO" du thread principal
                RunWorkerTask(i);     // Démarrer simultanément
            });
        }

        // Préparations dans le thread principal...
        InitializeSharedResources();

        // Donner le signal de départ : tous les threads repartent ensemble
        startBarrier.Wait();  // Dernier à arriver = thread principal

        // Optionnel : attendre la fin des workers
        for (auto& t : workers) {
            t.Join();
        }
    }

    // ---------------------------------------------------------------------
    // Exemple 4 : Réinitialisation forcée avec Reset()
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkBarrier.h>

    void AdaptiveSynchronization()
    {
        NkBarrier barrier(4);
        std::atomic<bool> shouldAbort{false};

        std::vector<NkThread> workers;
        for (int i = 0; i < 4; ++i) {
            workers.emplace_back([&barrier, &shouldAbort, i]() {
                while (!shouldAbort) {
                    if (!DoWorkStep(i)) {
                        // Un thread détecte une condition d'arrêt
                        shouldAbort = true;
                        barrier.Reset();  // Forcer la sortie de tous les threads
                        return;
                    }
                    barrier.Wait();  // Sync normale
                }
            });
        }

        for (auto& t : workers) {
            t.Join();
        }
    }

    // ---------------------------------------------------------------------
    // Exemple 5 : Pipeline parallèle avec barrières entre les stages
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkBarrier.h>

    template<typename Data>
    class ParallelPipeline {
    public:
        ParallelPipeline(nk_uint32 numStages, nk_uint32 threadsPerStage)
            : mNumStages(numStages)
        {
            // Une barrière par transition entre stages
            for (nk_uint32 i = 0; i < numStages - 1; ++i) {
                mBarriers.emplace_back(threadsPerStage);
            }
        }

        void RunStage(nk_uint32 stageId, std::function<void(Data&)> processor)
        {
            Data data;
            InitializeData(data);

            for (nk_uint32 iteration = 0; iteration < mNumIterations; ++iteration) {
                // Traitement du stage courant
                processor(data);

                // Synchroniser avant de passer au stage suivant
                if (stageId < mNumStages - 1) {
                    mBarriers[stageId].Wait();
                }
            }
        }

    private:
        nk_uint32 mNumStages;
        nk_uint32 mNumIterations = 100;  // Exemple
        std::vector<NkBarrier> mBarriers;
    };

    // ---------------------------------------------------------------------
    // Exemple 6 : Bonnes pratiques et anti-patterns
    // ---------------------------------------------------------------------
    // ✅ BON : Toujours appeler Wait() le même nombre de fois par thread
    void GoodBarrierUsage(NkBarrier& barrier)
    {
        // Phase 1
        DoPhase1();
        barrier.Wait();  // ✅ Tous les threads appellent Wait() ici

        // Phase 2
        DoPhase2();
        barrier.Wait();  // ✅ Encore une fois, tous appellent Wait()
    }

    // ❌ MAUVAIS : Appeler Wait() conditionnellement (deadlock potentiel)
    void BadBarrierUsage(NkBarrier& barrier, bool condition)
    {
        DoWork();
        if (condition) {
            barrier.Wait();  // ❌ Si condition=false pour un thread = deadlock !
        }
    }

    // ✅ BON : Utiliser le retour isLast pour actions post-sync coordonnées
    void CoordinatedPostSync(NkBarrier& barrier)
    {
        DoPreSyncWork();

        bool isLast = barrier.Wait();

        if (isLast) {
            // Seul le dernier thread exécute cette action
            AggregateResults();
        }

        // Tous les threads reprennent ici après la sync
        DoPostSyncWork();
    }

    // ✅ BON : Reset() uniquement quand la cohérence des données est garantie
    void SafeResetUsage(NkBarrier& barrier, std::atomic<bool>& emergency)
    {
        if (emergency.load()) {
            // Tous les threads peuvent sortir proprement
            barrier.Reset();  // ✅ Safe car emergency est visible par tous
            return;
        }
        barrier.Wait();  // Sync normale sinon
    }

    // ❌ MAUVAIS : Reset() arbitraire pouvant corrompre l'état partagé
    void UnsafeResetUsage(NkBarrier& barrier, std::vector<int>& sharedData)
    {
        // Thread 1 :
        sharedData[0] = 42;  // ❌ Écriture partielle
        barrier.Reset();     // ❌ Thread 2 peut lire sharedData[0] incomplet !

        // Thread 2 :
        barrier.Wait();      // Reprend après Reset()
        int val = sharedData[0];  // ❌ Valeur potentiellement incohérente
    }

    // ---------------------------------------------------------------------
    // Exemple 7 : Intégration avec NkThreadPool pour parallélisme dynamique
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkBarrier.h>
    #include <NKThreading/NkThreadPool.h>

    void BarrierWithThreadPool()
    {
        NkThreadPool pool(8);
        NkBarrier barrier(8);  // Synchroniser les 8 workers

        std::atomic<nk_uint32> iteration{0};
        constexpr nk_uint32 kMaxIterations = 100;

        // Soumettre une tâche par worker
        for (nk_uint32 i = 0; i < 8; ++i) {
            pool.Enqueue([&barrier, &iteration, i]() {
                while (iteration.load() < kMaxIterations) {
                    // Travail parallèle
                    ProcessChunk(i);

                    // Synchroniser avant d'incrémenter l'itération globale
                    bool isLast = barrier.Wait();

                    // Le dernier thread incrémente le compteur global
                    if (isLast) {
                        iteration.fetch_add(1);
                    }

                    // Synchroniser après l'incrément pour que tous voient la nouvelle valeur
                    barrier.Wait();
                }
            });
        }

        pool.Join();  // Attendre la fin de toutes les tâches
    }

    // ---------------------------------------------------------------------
    // Exemple 8 : Testing unitaire de NkBarrier
    // ---------------------------------------------------------------------
    #include <gtest/gtest.h>
    #include <NKThreading/Synchronization/NkBarrier.h>
    #include <NKThreading/NkThread.h>
    #include <atomic>
    #include <vector>

    TEST(NkBarrierTest, BasicSynchronization)
    {
        constexpr nk_uint32 kNumThreads = 4;
        NkBarrier barrier(kNumThreads);
        std::atomic<nk_uint32> reached{0};
        std::atomic<bool> lastThreadCalled{false};

        std::vector<NkThread> workers;
        for (nk_uint32 i = 0; i < kNumThreads; ++i) {
            workers.emplace_back([&barrier, &reached, &lastThreadCalled]() {
                reached.fetch_add(1);
                bool isLast = barrier.Wait();
                if (isLast) {
                    lastThreadCalled.store(true);
                }
            });
        }

        for (auto& t : workers) {
            t.Join();
        }

        // Tous les threads ont atteint la barrière
        EXPECT_EQ(reached.load(), kNumThreads);
        // Exactement un thread a reçu true
        EXPECT_TRUE(lastThreadCalled.load());
    }

    TEST(NkBarrierTest, MultiplePhases)
    {
        constexpr nk_uint32 kNumThreads = 3;
        constexpr nk_uint32 kNumPhases = 5;
        NkBarrier barrier(kNumThreads);
        std::atomic<nk_uint32> phaseCounters[kNumPhases] = {};

        std::vector<NkThread> workers;
        for (nk_uint32 t = 0; t < kNumThreads; ++t) {
            workers.emplace_back([&barrier, &phaseCounters, t]() {
                for (nk_uint32 phase = 0; phase < kNumPhases; ++phase) {
                    // Simuler un travail de phase
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));

                    bool isLast = barrier.Wait();
                    if (isLast) {
                        phaseCounters[phase].fetch_add(1);
                    }

                    // Deuxième sync par phase
                    barrier.Wait();
                }
            });
        }

        for (auto& t : workers) {
            t.Join();
        }

        // Chaque phase a eu exactement un "dernier thread"
        for (nk_uint32 phase = 0; phase < kNumPhases; ++phase) {
            EXPECT_EQ(phaseCounters[phase].load(), 1u);
        }
    }

    TEST(NkBarrierTest, ResetForcesCompletion)
    {
        constexpr nk_uint32 kNumThreads = 4;
        NkBarrier barrier(kNumThreads);
        std::atomic<nk_uint32> completed{0};

        // Lancer 3 threads qui attendent normalement
        std::vector<NkThread> workers;
        for (nk_uint32 i = 0; i < 3; ++i) {
            workers.emplace_back([&barrier, &completed]() {
                barrier.Wait();  // Va bloquer car 4ème thread manquant
                completed.fetch_add(1);
            });
        }

        // Petite pause pour laisser les 3 threads atteindre Wait()
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Reset() force la completion même sans le 4ème thread
        barrier.Reset();

        // Attendre que les 3 threads reprennent
        for (auto& t : workers) {
            t.Join();
        }

        // Tous les 3 threads ont pu continuer grâce à Reset()
        EXPECT_EQ(completed.load(), 3u);
    }

    // ---------------------------------------------------------------------
    // Exemple 9 : Configuration CMake pour NkBarrier
    // ---------------------------------------------------------------------
    # CMakeLists.txt - Intégration de NkBarrier

    # NkBarrier dépend de NkMutex et NkConditionVariable
    target_link_libraries(your_target PRIVATE
        NKThreading::NKThreading  # Inclut toutes les dépendances transitives
    )

    # Option pour activer les assertions de debug dans NkBarrier
    option(NKBARRIER_ENABLE_ASSERTS "Enable NKENTSEU_ASSERT in NkBarrier" ON)
    if(NKBARRIER_ENABLE_ASSERTS)
        target_compile_definitions(your_target PRIVATE NKENTSEU_ENABLE_ASSERTS)
    endif()

    // ---------------------------------------------------------------------
    // Exemple 10 : Debugging et instrumentation de barrière
    // ---------------------------------------------------------------------
    #ifdef NK_DEBUG
    #include <NKThreading/Synchronization/NkBarrier.h>
    #include <NKCore/NkLogger.h>

    // Wrapper instrumenté pour logging des synchronisations
    class DebugBarrier : public NkBarrier {
    public:
        explicit DebugBarrier(nk_uint32 numThreads, const nk_char* name = nullptr)
            : NkBarrier(numThreads), mName(name ? name : "Unnamed") {}

        nk_bool Wait() noexcept
        {
            auto tid = NkThread::GetCurrentThreadId();
            NK_LOG_DEBUG("Barrier[{}]: Thread {} entering Wait()", mName, tid);

            bool isLast = NkBarrier::Wait();

            NK_LOG_DEBUG("Barrier[{}]: Thread {} exiting Wait() (isLast={})",
                        mName, tid, isLast ? "true" : "false");
            return isLast;
        }

    private:
        const nk_char* mName;
    };

    // Usage en mode debug :
    // DebugBarrier barrier(4, "SimulationSync");
    #endif
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================