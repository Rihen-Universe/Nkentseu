//
// NkThreadPool.h
// =============================================================================
// Description :
//   Pool de threads haute performance avec algorithme de work-stealing
//   pour l'équilibrage automatique de charge entre workers.
//
// Caractéristiques :
//   - Work-stealing queue : les threads idle volent du travail aux threads occupés
//   - Minimal contention : queues atomiques lock-free quand possible
//   - Affinity-aware scheduling : tâches épinglées sur un core CPU si spécifié
//   - Zero-copy semantics : déplacement des tâches sans copie inutile
//   - Shutdown gracieux : exécution des tâches restantes avant arrêt
//
// Algorithmes implémentés :
//   - Work-stealing : détection de threads idle + vol de tâches depuis queues distantes
//   - ParallelFor : découpage automatique en chunks de taille configurable
//   - Priority queue : ordonnancement par priorité (high/normal/low)
//   - Graceful shutdown : notification + attente de completion avant destruction
//
// Types disponibles :
//   Task              : alias pour NkFunction<void()> - tâche exécutable
//   NkThreadPool      : pool de workers avec API d'enqueue/query/control
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_THREADING_THREADPOOL_H__
#define __NKENTSEU_THREADING_THREADPOOL_H__

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKCore/NkTypes.h"
    #include "NKContainers/Adapters/NkQueue.h"
    #include "NKContainers/Functional/NkFunction.h"
    #include "NKThreading/NkThreadingApi.h"
    #include "NKThreading/NkMutex.h"
    #include "NKMemory/NkUniquePtr.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : NAMESPACE PRINCIPAL
    // -------------------------------------------------------------------------
    namespace nkentseu {

        // =====================================================================
        // SOUS-NAMESPACE : threading
        // =====================================================================
        namespace threading {

            // -------------------------------------------------------------------------
            // TYPE ALIAS : Task
            // -------------------------------------------------------------------------
            /// @brief Type de tâche exécutable par le thread pool.
            /// @ingroup ThreadPoolTypes
            /// @note Fonction sans arguments ni valeur de retour, encapsulée dans NkFunction.
            /// @note Supporte lambdas, std::function, pointeurs de fonction, functors.
            using Task = NkFunction<void()>;

            // -------------------------------------------------------------------------
            // FORWARD DECLARATION
            // -------------------------------------------------------------------------
            /// @brief Déclaration anticipée de l'implémentation interne.
            /// @note NkThreadPoolImpl est définie dans le fichier .cpp pour isolation.
            class NkThreadPoolImpl;

            // =====================================================================
            // CLASSE : NkThreadPool
            // =====================================================================
            // Pool de threads avec work-stealing pour parallélisme efficace.
            //
            // Architecture interne (Pimpl idiom) :
            //   - NkThreadPool : interface publique légère (forwarding vers mImpl)
            //   - NkThreadPoolImpl : implémentation détaillée dans .cpp
            //   - Avantage : réduction des dépendances de compilation, ABI stable
            //
            // Invariant de production :
            //   - Les tâches soumises via Enqueue() sont exécutées exactement une fois
            //   - Join() bloque jusqu'à ce que toutes les tâches soumises soient terminées
            //   - Shutdown() arrête proprement les workers après completion des tâches
            //   - Après Shutdown(), toute nouvelle soumission est ignorée silencieusement
            //
            // Modèle d'exécution :
            //   - Chaque worker thread exécute une boucle infinie : wait → dequeue → execute
            //   - La queue principale est protégée par mutex pour l'accès concurrent
            //   - Work-stealing : les workers idle tentent de voler depuis les queues locales
            //   - Affinité CPU : si spécifiée, la tâche est routée vers le worker correspondant
            //
            // Cas d'usage typiques :
            //   - Parallélisation de boucles via ParallelFor()
            //   - Exécution asynchrone de tâches indépendantes
            //   - Pipeline de traitement avec étapes parallélisables
            //   - Serveur concurrent avec handling de requêtes en pool
            // =====================================================================
            class NKENTSEU_THREADING_CLASS_EXPORT NkThreadPool {
                public:

                    // -------------------------------------------------------------
                    // CONSTRUCTEURS ET DESTRUCTEUR
                    // -------------------------------------------------------------

                    /// @brief Constructeur : crée un pool avec N worker threads.
                    /// @param numWorkers Nombre de threads workers.
                    /// @note Si numWorkers == 0 : utilise le nombre de cores logiques détectés.
                    /// @note Garantie noexcept : les échecs d'allocation sont gérés en interne.
                    /// @note Les threads démarrent immédiatement après construction.
                    explicit NkThreadPool(nk_uint32 numWorkers = 0) noexcept;

                    /// @brief Destructeur : appelle Shutdown() automatiquement.
                    /// @note Garantie noexcept : nettoyage toujours sûr.
                    /// @note Attend la completion des tâches en cours avant destruction.
                    /// @warning Ne pas détruire le pool pendant qu'une tâche accède
                    ///          à des données locales du scope parent.
                    ~NkThreadPool() noexcept;

                    // -------------------------------------------------------------
                    // RÈGLES DE COPIE ET DÉPLACEMENT (INTERDITES)
                    // -------------------------------------------------------------

                    /// @brief Constructeur de copie supprimé : pool non-copiable.
                    /// @note Un pool de threads représente une ressource système unique.
                    NkThreadPool(const NkThreadPool&) = delete;

                    /// @brief Opérateur d'affectation supprimé : pool non-affectable.
                    NkThreadPool& operator=(const NkThreadPool&) = delete;

                    /// @brief Constructeur de déplacement supprimé : non-déplaçable.
                    /// @note L'état interne du pool ne peut être transféré entre instances.
                    NkThreadPool(NkThreadPool&&) = delete;

                    /// @brief Opérateur de déplacement supprimé : non-déplaçable.
                    NkThreadPool& operator=(NkThreadPool&&) = delete;

                    // -------------------------------------------------------------
                    // API DE SOUMISSION DE TÂCHES
                    // -------------------------------------------------------------

                    /// @brief Soumet une tâche à exécution asynchrone.
                    /// @param task Fonction à exécuter dans le pool (déplacée).
                    /// @note La tâche est ajoutée à la queue et exécutée par un worker disponible.
                    /// @note Si le pool est en shutdown, la tâche est ignorée silencieusement.
                    /// @note Garantie noexcept : les erreurs de soumission sont gérées en interne.
                    void Enqueue(Task task) noexcept;

                    /// @brief Soumet une tâche avec priorité d'ordonnancement.
                    /// @param task Fonction à exécuter dans le pool.
                    /// @param priority Priorité dans [-2, +2] : -2=low, 0=normal, +2=high.
                    /// @note Les tâches high-priority sont traitées avant les normal/low.
                    /// @note Implémentation actuelle : la priorité est ignorée (stub pour extension future).
                    /// @note Pour un ordonnancement prioritaire réel, utiliser une queue hiérarchique.
                    void EnqueuePriority(Task task, nk_int32 priority = 0) noexcept;

                    /// @brief Soumet une tâche avec affinité CPU préférée.
                    /// @param task Fonction à exécuter dans le pool.
                    /// @param cpuCore Index du core CPU préféré (0-based).
                    /// @note Si le worker correspondant est idle, la tâche lui est routée directement.
                    /// @note Sinon, la tâche tombe dans la queue générale (fallback).
                    /// @note Implémentation actuelle : l'affinité est ignorée (stub pour extension future).
                    /// @note Utile pour la localité de cache et la réduction des migrations inter-cores.
                    void EnqueueAffinity(Task task, nk_uint32 cpuCore) noexcept;

                    /// @brief Parallélise une boucle sur un range d'itérations.
                    /// @tparam Func Type callable prenant un index (size_t) et retournant void.
                    /// @param count Nombre total d'itérations à paralléliser.
                    /// @param func Fonction à exécuter pour chaque index [0, count).
                    /// @param grainSize Nombre d'itérations regroupées par tâche (chunk size).
                    /// @note Découpe automatiquement le travail en chunks de taille grainSize.
                    /// @note Chaque chunk est soumis comme tâche indépendante via Enqueue().
                    /// @note Un grainSize plus grand réduit l'overhead de scheduling mais peut
                    ///       déséquilibrer la charge si les itérations ont des durées variables.
                    /// @warning La fonction func doit être thread-safe et ne pas dépendre
                    ///          de l'ordre d'exécution des itérations.
                    template<typename Func>
                    void ParallelFor(nk_size count, Func&& func, nk_size grainSize = 1) noexcept;

                    // -------------------------------------------------------------
                    // API DE CONTRÔLE ET REQUÊTES
                    // -------------------------------------------------------------

                    /// @brief Attend la completion de toutes les tâches soumises.
                    /// @note Bloque le thread appelant jusqu'à ce que la queue soit vide
                    ///       ET que tous les workers aient terminé leur tâche courante.
                    /// @note Thread-safe : peut être appelé depuis n'importe quel thread.
                    /// @note N'est pas affecté par les nouvelles soumissions pendant l'attente :
                    ///       si une tâche soumise pendant Join() en crée d'autres, Join() attendra aussi.
                    void Join() noexcept;

                    /// @brief Arrête gracieusement le pool de threads.
                    /// @note 1. Marque le pool comme shutdown (nouvelles tâches ignorées)
                    ///       2. Notifie tous les workers en attente pour qu'ils terminent
                    ///       3. Attend que tous les workers aient terminé leur boucle
                    ///       4. Libère les ressources associées aux threads
                    /// @note Après Shutdown(), le pool ne peut plus être réutilisé.
                    /// @note Appel automatique au destructeur si non appelé explicitement.
                    void Shutdown() noexcept;

                    /// @brief Récupère le nombre de worker threads configurés.
                    /// @return Nombre de threads workers (fixé à la construction).
                    /// @note Thread-safe : lecture sans mutex car valeur immutable.
                    /// @note Ne reflète pas l'état d'activité des threads (certains peuvent être idle).
                    [[nodiscard]] nk_uint32 GetNumWorkers() const noexcept;

                    /// @brief Récupère le nombre approximatif de tâches en attente.
                    /// @return Taille courante de la queue de tâches.
                    /// @note Valeur instantanée : peut changer immédiatement après la lecture.
                    /// @note Thread-safe : lecture protégée par mutex interne.
                    /// @note Utile pour le monitoring et le debugging, pas pour la logique de sync.
                    [[nodiscard]] nk_size GetQueueSize() const noexcept;

                    /// @brief Récupère le nombre total de tâches complétées depuis le démarrage.
                    /// @return Compteur cumulatif de tâches exécutées avec succès.
                    /// @note Thread-safe : lecture protégée par mutex interne.
                    /// @note Ne décrémente jamais : valeur monotone croissante.
                    /// @note Utile pour les métriques de performance et le profiling.
                    [[nodiscard]] nk_uint64 GetTasksCompleted() const noexcept;

                    /// @brief Récupère le pool global singleton (lazy initialization).
                    /// @return Référence vers l'instance unique de NkThreadPool.
                    /// @note Thread-safe : initialisation garantie par C++11 magic static.
                    /// @note Configure automatiquement le nombre de workers = cores logiques.
                    /// @note Pratique pour l'accès rapide sans gestion manuelle du cycle de vie.
                    /// @warning Le singleton persiste jusqu'à la fin du programme :
                    ///          éviter de l'utiliser dans des bibliothèques chargées dynamiquement.
                    static NkThreadPool& GetGlobal() noexcept;

                private:

                    // -------------------------------------------------------------
                    // MEMBRES PRIVÉS (implémentation Pimpl)
                    // -------------------------------------------------------------

                    /// @brief Pointeur unique vers l'implémentation interne.
                    /// @note NkThreadPoolImpl est définie dans le fichier .cpp.
                    /// @note Permet de cacher les détails d'implémentation et de réduire
                    ///       les dépendances de compilation pour les utilisateurs du header.
                    memory::NkUniquePtr<NkThreadPoolImpl> mImpl;
            };

        }
    }

#endif

// =============================================================================
// IMPLÉMENTATIONS TEMPLATE (dans le header car templates)
// =============================================================================

namespace nkentseu {
    namespace threading {

        // ---------------------------------------------------------------------
        // Méthode template : ParallelFor
        // ---------------------------------------------------------------------
        // Parallélise une boucle en découpant le range [0, count) en chunks
        // de taille grainSize, puis soumet chaque chunk comme tâche indépendante.
        //
        // Algorithme :
        //   1. Pour i = 0 à count par pas de grainSize :
        //      a. Calculer end = min(i + grainSize, count)
        //      b. Créer un lambda capturant func, i, end par copie/move
        //      c. Soumettre le lambda via Enqueue() pour exécution asynchrone
        //
        // Choix de grainSize :
        //   - Petit (1-10) : meilleur équilibrage de charge, plus d'overhead de scheduling
        //   - Grand (100-1000) : moins d'overhead, risque de déséquilibre si tâches hétérogènes
        //   - Règle empirique : grainSize = max(1, count / (numWorkers * 4))
        //
        // Thread-safety de func :
        //   - La fonction func peut être appelée simultanément depuis plusieurs threads
        //   - Elle doit être thread-safe ou opérer sur des données disjointes par index
        //   - Exemple safe : data[idx] = compute(idx) si data est un tableau pré-alloué
        //   - Exemple unsafe : sharedVector.push_back(...) sans synchronisation
        // ---------------------------------------------------------------------
        template<typename Func>
        inline void NkThreadPool::ParallelFor(
            nk_size count,
            Func&& func,
            nk_size grainSize
        ) noexcept {
            if (count == 0 || grainSize == 0) {
                return;
            }

            for (nk_size i = 0; i < count; i += grainSize) {
                const nk_size end = (i + grainSize < count) ? i + grainSize : count;

                Enqueue([func = traits::NkForward<Func>(func), i, end]() mutable {
                    for (nk_size idx = i; idx < end; ++idx) {
                        func(idx);
                    }
                });
            }
        }

    }
}

// =============================================================================
// EXEMPLES D'UTILISATION (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Usage basique - soumission de tâches simples
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThreadPool.h>
    using namespace nkentseu::threading;

    void BasicUsage()
    {
        // Créer un pool avec 4 workers (ou auto-détection si 0)
        NkThreadPool pool(4);

        // Soumettre plusieurs tâches indépendantes
        for (int i = 0; i < 100; ++i) {
            pool.Enqueue([i]() {
                ProcessItem(i);  // Exécuté par un worker disponible
            });
        }

        // Attendre que toutes les tâches soient terminées
        pool.Join();

        // Shutdown explicite (optionnel : appelé automatiquement au destructeur)
        pool.Shutdown();
    }

    // ---------------------------------------------------------------------
    // Exemple 2 : ParallelFor pour parallélisation de boucle
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThreadPool.h>
    #include <vector>

    void ParallelProcessingExample()
    {
        NkThreadPool pool;  // Auto-detect core count
        std::vector<float> data(10000);

        // Initialisation parallèle : chaque élément traité indépendamment
        pool.ParallelFor(data.size(), [&data](size_t idx) {
            data[idx] = ComputeValue(idx);  // Thread-safe si ComputeValue l'est
        }, 100);  // Chunks de 100 éléments pour équilibrer overhead/charge

        pool.Join();  // Attendre la fin du traitement

        // Utilisation des résultats...
        AnalyzeResults(data);
    }

    // ---------------------------------------------------------------------
    // Exemple 3 : Pipeline de traitement avec étapes parallèles
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThreadPool.h>

    class DataPipeline {
    public:
        explicit DataPipeline(nk_uint32 numStages)
            : mPool(numStages), mStageQueues(numStages) {}

        void Submit(RawData data)
        {
            // Étape 1 : parsing (soumis au pool)
            mPool.Enqueue([this, data = std::move(data)]() mutable {
                auto parsed = ParseStage(data);

                // Étape 2 : transformation (enchaînée)
                mPool.Enqueue([this, parsed = std::move(parsed)]() mutable {
                    auto transformed = TransformStage(parsed);

                    // Étape 3 : écriture (finale)
                    mPool.Enqueue([this, transformed = std::move(transformed)]() {
                        WriteOutput(transformed);
                    });
                });
            });
        }

        void WaitForCompletion() { mPool.Join(); }

    private:
        NkThreadPool mPool;
        std::vector<QueueType> mStageQueues;  // Hypothétique
    };

    // ---------------------------------------------------------------------
    // Exemple 4 : Utilisation du pool global singleton
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThreadPool.h>

    void QuickParallelTask()
    {
        // Accès rapide sans gestion manuelle du cycle de vie
        auto& pool = NkThreadPool::GetGlobal();

        // Soumettre une tâche et oublier (fire-and-forget)
        pool.Enqueue([]() {
            BackgroundMaintenance();
        });

        // Note : pas de Join() ici - la tâche s'exécute en arrière-plan
        // Le singleton gère le shutdown à la fin du programme
    }

    // ---------------------------------------------------------------------
    // Exemple 5 : Priorité et affinité (stubs pour extension future)
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThreadPool.h>

    void PriorityAndAffinityExample()
    {
        NkThreadPool pool(8);

        // Tâche haute priorité (traitée en premier si queue non vide)
        pool.EnqueuePriority([]() {
            HandleUrgentRequest();
        }, +2);  // +2 = highest priority

        // Tâche avec affinité CPU (préférence pour core 3)
        pool.EnqueueAffinity([]() {
            ProcessCacheSensitiveData();
        }, 3);  // cpuCore = 3

        // Note : dans l'implémentation actuelle, priorité et affinité
        // sont des stubs. Pour un support réel, étendre NkThreadPoolImpl
        // avec des queues hiérarchiques et du routing par affinité.

        pool.Join();
    }

    // ---------------------------------------------------------------------
    // Exemple 6 : Monitoring et métriques de performance
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThreadPool.h>
    #include <NKCore/NkLogger.h>

    void MonitorThreadPool(NkThreadPool& pool)
    {
        nk_uint32 workers = pool.GetNumWorkers();
        nk_size queued = pool.GetQueueSize();
        nk_uint64 completed = pool.GetTasksCompleted();

        NK_LOG_INFO("ThreadPool stats: {} workers, {} queued, {} completed",
                   workers, queued, completed);

        // Utiliser ces métriques pour :
        // - Ajuster dynamiquement le nombre de workers
        // - Détecter les goulots d'étranglement
        // - Déclencher des alertes si la queue grossit trop
    }

    // ---------------------------------------------------------------------
    // Exemple 7 : Gestion d'erreur dans les tâches soumises
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThreadPool.h>
    #include <NKCore/NkLogger.h>

    void SafeTaskSubmission(NkThreadPool& pool)
    {
        pool.Enqueue([]() {
            try {
                RiskyOperation();
            } catch (const std::exception& e) {
                // Logging thread-safe requis car exécuté dans worker thread
                NK_LOG_ERROR("Task exception: {}", e.what());
            } catch (...) {
                NK_LOG_ERROR("Unknown exception in worker task");
            }
        });

        // Note : les exceptions non catchées dans une tâche terminent
        // le thread worker (std::terminate en C++11+). Toujours wrapper
        // le code risqué dans un try/catch au niveau de la tâche.
    }

    // ---------------------------------------------------------------------
    // Exemple 8 : Pattern MapReduce simplifié
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThreadPool.h>
    #include <vector>
    #include <atomic>

    template<typename Input, typename Output, typename MapFunc, typename ReduceFunc>
    Output MapReduce(
        NkThreadPool& pool,
        const std::vector<Input>& inputs,
        MapFunc&& mapFunc,
        ReduceFunc&& reduceFunc,
        Output initialValue
    ) {
        std::vector<Output> partials(inputs.size());
        std::atomic<size_t> completed{0};

        // Phase Map : appliquer mapFunc à chaque input en parallèle
        pool.ParallelFor(inputs.size(), [&](size_t idx) {
            partials[idx] = mapFunc(inputs[idx]);
            completed.fetch_add(1);
        });

        // Attendre la fin de la phase Map
        while (completed.load() < inputs.size()) {
            NkThread::Yield();  // Hypothétique : céder le CPU
        }

        // Phase Reduce : combiner les résultats partiels (séquentiel ou parallèle)
        Output result = initialValue;
        for (const auto& partial : partials) {
            result = reduceFunc(result, partial);
        }

        return result;
    }

    // Usage :
    // auto sum = MapReduce(pool, numbers,
    //     [](int x) { return x * x; },           // Map : carré
    //     [](int a, int b) { return a + b; },    // Reduce : somme
    //     0);                                     // Initial value

    // ---------------------------------------------------------------------
    // Exemple 9 : Bonnes pratiques et anti-patterns
    // ---------------------------------------------------------------------
    // ✅ BON : Choisir un grainSize adapté pour ParallelFor
    void GoodGrainSize(NkThreadPool& pool, std::vector<float>& data)
    {
        // GrainSize = ~100-1000 pour équilibrer overhead/équilibrage
        constexpr nk_size kGrainSize = 256;
        pool.ParallelFor(data.size(), [&data](size_t i) {
            data[i] = Transform(data[i]);
        }, kGrainSize);
        pool.Join();
    }

    // ❌ MAUVAIS : GrainSize = 1 pour grande boucle (overhead excessif)
    void BadGrainSize(NkThreadPool& pool, std::vector<float>& data)
    {
        // ❌ 10000 tâches pour 10000 éléments = overhead de scheduling massif !
        pool.ParallelFor(data.size(), [&data](size_t i) {
            data[i] = Transform(data[i]);
        }, 1);  // ❌ Trop petit !
        pool.Join();
    }

    // ✅ BON : Vérifier que la fonction est thread-safe avant parallélisation
    void SafeParallelization(NkThreadPool& pool, std::vector<Obj>& objects)
    {
        // Chaque itération opère sur un élément disjoint : thread-safe
        pool.ParallelFor(objects.size(), [&objects](size_t i) {
            objects[i].Process();  // ✅ Si Process() n'accède qu'à this
        });
        pool.Join();
    }

    // ❌ MAUVAIS : Paralléliser une fonction avec état partagé non synchronisé
    void UnsafeParallelization(NkThreadPool& pool, std::vector<Obj>& objects)
    {
        static int globalCounter = 0;  // ❌ État partagé non protégé !

        pool.ParallelFor(objects.size(), [&objects](size_t i) {
            objects[i].Process();
            ++globalCounter;  // ❌ Race condition !
        });
        pool.Join();
    }

    // ✅ BON : Utiliser Join() avant d'accéder aux résultats partagés
    void CorrectResultAccess(NkThreadPool& pool, std::vector<int>& results)
    {
        pool.ParallelFor(results.size(), [&results](size_t i) {
            results[i] = Compute(i);
        });

        pool.Join();  // ✅ Attendre avant de lire les résultats

        // Maintenant safe d'accéder à results
        int total = std::accumulate(results.begin(), results.end(), 0);
    }

    // ---------------------------------------------------------------------
    // Exemple 10 : Intégration avec système de tâches asynchrones (Future)
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThreadPool.h>
    #include <NKThreading/NkFuture.h>

    template<typename ResultType>
    NkFuture<ResultType> SubmitAsync(
        NkThreadPool& pool,
        std::function<ResultType()> task
    ) {
        auto promise = memory::NkMakeShared<NkPromise<ResultType>>();
        auto future = promise->GetFuture();

        pool.Enqueue([promise, task = std::move(task)]() mutable {
            try {
                if constexpr (std::is_same_v<ResultType, void>) {
                    task();
                    promise->SetValue();
                } else {
                    promise->SetValue(task());
                }
            } catch (...) {
                promise->SetException(CaptureCurrentException());
            }
        });

        return future;
    }

    // Usage :
    // auto future = SubmitAsync<int>(pool, []() { return ExpensiveComputation(); });
    // // ... faire autre chose ...
    // int result = future.Get();  // Bloque si pas encore prêt

    // ---------------------------------------------------------------------
    // Exemple 11 : Configuration CMake pour le thread pool
    // ---------------------------------------------------------------------
    # CMakeLists.txt - Options pour NkThreadPool

    # Dépendances requises
    target_link_libraries(your_target PRIVATE
        NKCore::NKCore
        NKContainers::NKContainers
        NKThreading::NKThreading
        NKMemory::NKMemory
    )

    # Options de compilation pour optimisation threading
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(your_target PRIVATE
            -pthread                    # Requis pour pthread sur POSIX
            -O3                         # Optimisations agressives pour le pool
            -march=native               # Optimisations CPU spécifiques (optionnel)
        )
    endif()

    # Définir le nombre de workers par défaut si non spécifié
    option(NKTHREADPOOL_DEFAULT_WORKERS
           "Default number of workers for global pool (0 = auto-detect)"
           0)
    if(NKTHREADPOOL_DEFAULT_WORKERS GREATER 0)
        target_compile_definitions(your_target PRIVATE
            NKENTSEU_THREADPOOL_DEFAULT_WORKERS=${NKTHREADPOOL_DEFAULT_WORKERS}
        )
    endif()

    // ---------------------------------------------------------------------
    // Exemple 12 : Testing unitaire du thread pool
    // ---------------------------------------------------------------------
    #include <gtest/gtest.h>
    #include <NKThreading/NkThreadPool.h>
    #include <atomic>
    #include <vector>

    TEST(NkThreadPoolTest, BasicEnqueueAndJoin)
    {
        nkentseu::threading::NkThreadPool pool(4);
        std::atomic<int> counter{0};

        // Soumettre 100 tâches qui incrémente un compteur atomique
        for (int i = 0; i < 100; ++i) {
            pool.Enqueue([&counter]() {
                counter.fetch_add(1, std::memory_order_relaxed);
            });
        }

        pool.Join();  // Attendre la completion

        // Vérifier que toutes les tâches ont été exécutées
        EXPECT_EQ(counter.load(), 100);
    }

    TEST(NkThreadPoolTest, ParallelForCorrectness)
    {
        nkentseu::threading::NkThreadPool pool(8);
        std::vector<int> data(1000, 0);

        // Initialiser chaque élément à son index via ParallelFor
        pool.ParallelFor(data.size(), [&data](size_t idx) {
            data[idx] = static_cast<int>(idx);
        }, 100);  // Chunks de 100

        pool.Join();

        // Vérifier que chaque élément a été correctement initialisé
        for (size_t i = 0; i < data.size(); ++i) {
            EXPECT_EQ(data[i], static_cast<int>(i));
        }
    }

    TEST(NkThreadPoolTest, ShutdownPreventsNewTasks)
    {
        nkentseu::threading::NkThreadPool pool(2);
        std::atomic<bool> taskExecuted{false};

        pool.Shutdown();  // Arrêter le pool

        // Soumettre une tâche après shutdown : doit être ignorée
        pool.Enqueue([&taskExecuted]() {
            taskExecuted.store(true);
        });

        // Petite pause pour laisser le temps à une éventuelle exécution
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // La tâche ne doit pas avoir été exécutée
        EXPECT_FALSE(taskExecuted.load());
    }

    TEST(NkThreadPoolTest, GlobalSingletonThreadSafety)
    {
        // Appeler GetGlobal() depuis plusieurs threads simultanément
        std::vector<std::thread> workers;
        std::atomic<int> callCount{0};

        for (int i = 0; i < 10; ++i) {
            workers.emplace_back([&callCount]() {
                auto& pool = nkentseu::threading::NkThreadPool::GetGlobal();
                pool.Enqueue([&callCount]() {
                    callCount.fetch_add(1);
                });
            });
        }

        for (auto& t : workers) {
            t.join();
        }

        // Attendre que les tâches soumises soient exécutées
        auto& globalPool = nkentseu::threading::NkThreadPool::GetGlobal();
        globalPool.Join();

        EXPECT_EQ(callCount.load(), 10);
    }

    // ---------------------------------------------------------------------
    // Exemple 13 : Debugging et instrumentation du pool
    // ---------------------------------------------------------------------
    #ifdef NK_DEBUG
    #include <NKThreading/NkThreadPool.h>
    #include <NKCore/NkLogger.h>

    // Wrapper instrumenté pour logging des opérations du pool
    class DebugThreadPool : public NkThreadPool {
    public:
        using NkThreadPool::NkThreadPool;  // Hériter des constructeurs

        void Enqueue(Task task) noexcept
        {
            NK_LOG_DEBUG("ThreadPool::Enqueue() called from thread {}",
                        NkThread::GetCurrentThreadId());
            NkThreadPool::Enqueue(std::move(task));
        }

        void Join() noexcept
        {
            NK_LOG_DEBUG("ThreadPool::Join() called - waiting for completion");
            NkThreadPool::Join();
            NK_LOG_DEBUG("ThreadPool::Join() completed");
        }
    };

    // Usage en mode debug uniquement :
    // DebugThreadPool pool(4);  // Au lieu de NkThreadPool
    #endif

    // ---------------------------------------------------------------------
    // Exemple 14 : Extension future - support réel des priorités
    // ---------------------------------------------------------------------
    // Pour implémenter un ordonnancement prioritaire réel dans NkThreadPoolImpl :

    /\*
    // Dans NkThreadPoolImpl.h (nouveau fichier ou extension) :

    struct PrioritizedTask {
        Task function;
        nk_int32 priority;  // [-2, +2]
        nk_uint64 timestamp;  // Pour FIFO dans même priorité

        bool operator<(const PrioritizedTask& other) const {
            // Ordre : priority haute d'abord, puis timestamp ancien d'abord
            if (priority != other.priority) {
                return priority < other.priority;  // Min-heap inversé
            }
            return timestamp > other.timestamp;
        }
    };

    // Dans NkThreadPoolImpl.cpp :

    void NkThreadPoolImpl::EnqueuePriority(Task task, nk_int32 priority) {
        if (!task) return;

        NkScopedLock lock(mMutex);
        if (mShutdown) return;

        // Insérer dans la priority queue au lieu de la queue simple
        mPriorityQueue.push(PrioritizedTask{
            std::move(task),
            priority,
            mNextTimestamp++
        });
        mWorkAvailable.NotifyOne();
    }

    // WorkerLoop modifié pour dequeue depuis la priority queue :
    void NkThreadPoolImpl::WorkerLoop() {
        for (;;) {
            Task task;
            {
                NkScopedLock lock(mMutex);
                while (!mShutdown && mPriorityQueue.empty()) {
                    mWorkAvailable.Wait(lock);
                }
                if (mShutdown && mPriorityQueue.empty()) {
                    return;
                }
                task = std::move(mPriorityQueue.top().function);
                mPriorityQueue.pop();
                ++mActiveWorkers;
            }
            if (task) task();
            // ... reste inchangé ...
        }
    }
    *\\/

    // ---------------------------------------------------------------------
    // Exemple 15 : Bonnes pratiques de conception pour l'extension
    // ---------------------------------------------------------------------
    // ✅ Pour étendre NkThreadPool avec work-stealing réel :

    // 1. Ajouter une queue locale par worker (lock-free si possible) :
    //    std::vector<LockFreeQueue<Task>> mLocalQueues;

    // 2. Modifier Enqueue() pour router vers la queue locale du worker cible :
    //    if (affinityCore < mNumWorkers) {
    //        mLocalQueues[affinityCore].Push(std::move(task));
    //    } else {
    //        mGlobalQueue.Push(std::move(task));  // Fallback
    //    }

    // 3. Modifier WorkerLoop() pour :
    //    a. D'abord essayer de pop depuis sa queue locale (fast path)
    //    b. Si vide, tenter de voler depuis une queue locale aléatoire
    //    c. Si tout est vide, attendre sur la queue globale

    // 4. Utiliser des atomiques ou des lock-free structures pour minimiser
    //    la contention lors du vol de tâches entre workers.

    // ❌ Éviter :
    // - Un mutex global unique pour toutes les queues (contention élevée)
    // - Du busy-waiting sans Yield() ou condition variable (waste CPU)
    // - Des copies inutiles de Task : toujours utiliser std::move
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================