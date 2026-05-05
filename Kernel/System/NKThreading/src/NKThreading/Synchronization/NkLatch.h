//
// NkLatch.h
// =============================================================================
// Description :
//   Primitive de synchronisation de type CountDownLatch permettant à N threads
//   de signaler leur completion et à d'autres threads d'attendre que tous
//   aient terminé. Idéal pour la coordination d'initialisation ou de phase.
//
// Caractéristiques :
//   - Compteur décrémentation thread-safe avec notification automatique à zéro
//   - Support timeout : Wait(ms) avec horloge monotone pour précision
//   - Usage unique : contrairement à NkBarrier, ne peut être réinitialisé
//   - Décrémentation batch : CountDown(n) pour signaler plusieurs unités d'un coup
//   - Garantie noexcept sur toutes les opérations publiques
//
// Algorithmes implémentés :
//   - CountDown : décrémentation atomique + notification si compteur atteint zéro
//   - Wait : attente bloquante avec gestion des spurious wakeups via boucle while
//   - IsReady/GetCount : requêtes thread-safe de l'état courant
//
// Types disponibles :
//   NkLatch : latch à usage unique pour synchronisation de completion
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_THREADING_LATCH_H__
#define __NKENTSEU_THREADING_LATCH_H__

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
            // CLASSE : NkLatch
            // =====================================================================
            // Latch de comptage à usage unique pour synchronisation de completion.
            //
            // Représentation interne :
            //   - mCount : compteur restant à décrémenter avant completion
            //   - mMutex + mCondVar : primitives natives pour synchronisation thread-safe
            //
            // Invariant de production :
            //   - mCount >= 0 à tout instant (jamais négatif)
            //   - Une fois mCount == 0, il reste à zéro (latch irréversible)
            //   - Tous les Wait() retournent immédiatement une fois le compteur à zéro
            //
            // Différence vs NkBarrier :
            //   • NkLatch : usage unique, compteur décrémente, pas de réinitialisation
            //   • NkBarrier : réutilisable, synchronisation de phase, gestion des itérations
            //
            // Cas d'usage typiques :
            //   - Attendre que N tâches d'initialisation soient terminées avant de démarrer
            //   - Synchroniser le début d'un traitement après collecte de données parallèles
            //   - Coordonner la fin d'un batch de travaux avant agrégation des résultats
            //   - Pattern "start gun" : plusieurs threads attendent un signal de départ unique
            // =================================================================
            class NKENTSEU_THREADING_CLASS_EXPORT NkLatch {
            public:

                // -------------------------------------------------------------
                // CONSTRUCTEURS ET RÈGLES DE COPIE
                // -------------------------------------------------------------

                /// @brief Constructeur : initialise le latch avec un compteur initial.
                /// @param initialCount Nombre de signaux à recevoir avant completion (doit être > 0).
                /// @note Garantie noexcept : aucune allocation ni exception possible.
                /// @note NKENTSEU_ASSERT vérifie initialCount > 0 en mode debug.
                /// @note Le latch est immédiatement utilisable après construction.
                explicit NkLatch(nk_uint32 initialCount) noexcept;

                /// @brief Constructeur de copie supprimé : latch non-copiable.
                /// @note Un latch représente un point de synchronisation partagé unique.
                NkLatch(const NkLatch&) = delete;

                /// @brief Opérateur d'affectation supprimé : latch non-affectable.
                NkLatch& operator=(const NkLatch&) = delete;

                // -------------------------------------------------------------
                // API DE DÉCRÉMENTATION (CountDown)
                // -------------------------------------------------------------

                /// @brief Décrémente le compteur du latch d'une ou plusieurs unités.
                /// @param count Nombre d'unités à décrémenter (par défaut : 1).
                /// @note Si count >= mCount, le compteur est mis à zéro directement.
                /// @note Si le compteur atteint zéro, tous les threads en attente sont réveillés.
                /// @note Thread-safe : peut être appelé simultanément depuis plusieurs threads.
                /// @note Non-bloquant : retourne immédiatement après mise à jour.
                /// @warning Ne pas appeler avec count > mCount restant = comportement défini
                ///          (compteur clampé à zéro) mais potentiellement indicateur d'un bug logique.
                void CountDown(nk_uint32 count = 1) noexcept;

                // -------------------------------------------------------------
                // API D'ATTENTE (Wait/IsReady)
                // -------------------------------------------------------------

                /// @brief Attend que le compteur atteigne zéro.
                /// @param timeoutMs Durée d'attente maximale en millisecondes.
                ///        -1 = attente infinie (défaut), 0 = retour immédiat (polling).
                /// @return true si le compteur a atteint zéro, false en cas de timeout.
                /// @note Thread-safe : peut être appelé depuis n'importe quel thread.
                /// @note Bloquant : suspend le thread jusqu'à completion ou timeout.
                /// @note Une fois le compteur à zéro, tous les Wait() futurs retournent immédiatement.
                [[nodiscard]] nk_bool Wait(nk_int32 timeoutMs = -1) noexcept;

                /// @brief Vérifie si le latch est prêt (compteur à zéro).
                /// @return true si mCount == 0, false sinon.
                /// @note Thread-safe : lecture protégée par mutex interne.
                /// @note Non-bloquant : retourne immédiatement dans tous les cas.
                /// @note La valeur peut changer immédiatement après le retour (concurrence).
                [[nodiscard]] nk_bool IsReady() const noexcept;

                /// @brief Récupère la valeur courante du compteur.
                /// @return Nombre d'unités restantes avant completion.
                /// @note Thread-safe : lecture protégée par mutex interne.
                /// @note Utile pour le monitoring et le debugging, pas pour la logique de sync.
                [[nodiscard]] nk_uint32 GetCount() const noexcept;

            private:

                // -------------------------------------------------------------
                // MEMBRES PRIVÉS (état interne du latch)
                // -------------------------------------------------------------

                nk_uint32 mCount;                    ///< Compteur restant à décrémenter avant completion
                mutable NkMutex mMutex;              ///< Mutex pour protéger l'accès concurrent aux membres
                mutable NkConditionVariable mCondVar;///< Condition variable pour l'attente efficace des threads

            };

        }
    }

#endif

// =============================================================================
// EXEMPLES D'UTILISATION (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Synchronisation d'initialisation de services
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkLatch.h>
    using namespace nkentseu::threading;

    void ParallelInitialization()
    {
        constexpr nk_uint32 kNumServices = 4;
        NkLatch initLatch(kNumServices);  // Attendre 4 initialisations

        std::vector<NkThread> workers;

        // Lancer l'initialisation parallèle des services
        for (nk_uint32 i = 0; i < kNumServices; ++i) {
            workers.emplace_back([&initLatch, i]() {
                InitializeService(i);  // Travail d'initialisation
                initLatch.CountDown(); // Signaler la completion
            });
        }

        // Attendre que tous les services soient initialisés
        initLatch.Wait();  // Bloque jusqu'à ce que CountDown() soit appelé 4 fois

        // Tous les services sont prêts : démarrer l'application
        StartApplication();

        // Attendre que les threads d'initialisation terminent proprement
        for (auto& t : workers) {
            t.Join();
        }
    }

    // ---------------------------------------------------------------------
    // Exemple 2 : Pattern "start gun" pour démarrage synchronisé
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkLatch.h>

    void SynchronizedStart()
    {
        NkLatch startLatch(1);  // Un signal de départ

        std::vector<NkThread> workers;
        for (int i = 0; i < 4; ++i) {
            workers.emplace_back([&startLatch, i]() {
                startLatch.Wait();  // Attendre le signal de départ
                RunWorker(i);       // Démarrer simultanément
            });
        }

        // Préparations dans le thread principal...
        PrepareSharedResources();

        // Donner le signal de départ : tous les workers repartent ensemble
        startLatch.CountDown();  // Décrémente de 1 → compteur à zéro → réveil de tous

        for (auto& t : workers) {
            t.Join();
        }
    }

    // ---------------------------------------------------------------------
    // Exemple 3 : Agrégation de résultats après traitement parallèle
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkLatch.h>
    #include <vector>
    #include <atomic>

    void ParallelAggregation()
    {
        constexpr nk_uint32 kNumChunks = 8;
        std::vector<int> partialResults(kNumChunks, 0);
        NkLatch completionLatch(kNumChunks);

        std::vector<NkThread> workers;
        for (nk_uint32 i = 0; i < kNumChunks; ++i) {
            workers.emplace_back([&partialResults, &completionLatch, i]() {
                partialResults[i] = ProcessChunk(i);  // Traitement indépendant
                completionLatch.CountDown();           // Signaler la completion
            });
        }

        // Attendre que tous les chunks soient traités
        completionLatch.Wait();

        // Agréger les résultats partiels (tous sont maintenant disponibles)
        int total = 0;
        for (int partial : partialResults) {
            total += partial;
        }

        UseResult(total);

        for (auto& t : workers) {
            t.Join();
        }
    }

    // ---------------------------------------------------------------------
    // Exemple 4 : Décrémentation batch avec CountDown(n)
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkLatch.h>

    void BatchCompletion()
    {
        // Latch pour 100 sous-tâches
        NkLatch latch(100);

        // Thread qui traite par lots de 10
        NkThread batchProcessor([&latch]() {
            for (int batch = 0; batch < 10; ++batch) {
                ProcessBatch(batch);  // Traite 10 sous-tâches
                latch.CountDown(10);  // Signale 10 completions d'un coup
            }
        });

        // Thread qui attend la completion totale
        NkThread waiter([&latch]() {
            latch.Wait();  // Attend que les 100 sous-tâches soient signalées
            OnAllComplete();
        });

        batchProcessor.Join();
        waiter.Join();
    }

    // ---------------------------------------------------------------------
    // Exemple 5 : Timeout pour éviter les blocages infinis
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkLatch.h>
    #include <NKCore/NkLogger.h>

    bool WaitForCompletion(NkLatch& latch, nk_uint32 timeoutMs)
    {
        if (latch.Wait(timeoutMs)) {
            return true;  // Completion dans le délai
        } else {
            NK_LOG_WARNING("Latch timeout after {}ms - {} units remaining",
                          timeoutMs, latch.GetCount());
            return false;  // Timeout : gérer l'erreur ou fallback
        }
    }

    // Usage :
    // NkLatch initLatch(5);
    // if (!WaitForCompletion(initLatch, 10000)) {
    //     FallbackInitialization();
    // }

    // ---------------------------------------------------------------------
    // Exemple 6 : Monitoring avec GetCount() et IsReady()
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkLatch.h>
    #include <NKCore/NkLogger.h>

    void MonitorLatch(NkLatch& latch, const char* operationName)
    {
        while (!latch.IsReady()) {
            nk_uint32 remaining = latch.GetCount();
            NK_LOG_INFO("{}: {} units remaining", operationName, remaining);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        NK_LOG_INFO("{}: completed", operationName);
    }

    // ---------------------------------------------------------------------
    // Exemple 7 : Combinaison Latch + ThreadPool pour tâches dynamiques
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkLatch.h>
    #include <NKThreading/NkThreadPool.h>

    void DynamicTaskCompletion()
    {
        NkThreadPool pool(8);
        NkLatch taskLatch(0);  // Compteur initial à zéro

        std::atomic<nk_uint32> taskCounter{0};
        constexpr nk_uint32 kMaxTasks = 100;

        // Soumettre des tâches dynamiquement
        for (nk_uint32 i = 0; i < kMaxTasks; ++i) {
            taskCounter.fetch_add(1);
            pool.Enqueue([&taskLatch, &taskCounter, i]() {
                ProcessTask(i);
                taskLatch.CountDown();  // Signaler la completion
            });
        }

        // Attendre que toutes les tâches soumises soient terminées
        // Note : le compteur initial était 0, donc on doit le régler à kMaxTasks
        // Correction : initialiser avec le nombre attendu
        // Mieux : utiliser un pattern différent si le nombre n'est pas connu à l'avance

        // Pattern corrigé :
        NkLatch knownLatch(kMaxTasks);  // Connaître le nombre à l'avance
        // ... soumettre les tâches avec knownLatch.CountDown() ...
        knownLatch.Wait();  // Attendre la completion connue
    }

    // ---------------------------------------------------------------------
    // Exemple 8 : Bonnes pratiques et anti-patterns
    // ---------------------------------------------------------------------
    // ✅ BON : Initialiser avec le nombre exact de signaux attendus
    void GoodLatchInit()
    {
        constexpr nk_uint32 kExpectedSignals = 5;
        NkLatch latch(kExpectedSignals);  // ✅ Clair et explicite
        // ... utiliser latch ...
    }

    // ❌ MAUVAIS : Initialiser avec 0 ou valeur incorrecte
    void BadLatchInit()
    {
        NkLatch latch(0);  // ❌ Wait() retourne immédiatement, pas de synchronisation !
        // Ou pire :
        NkLatch wrongLatch(10);  // Mais seulement 5 CountDown() seront appelés = deadlock !
    }

    // ✅ BON : Utiliser IsReady() pour polling non-bloquant
    void GoodPolling(NkLatch& latch)
    {
        while (!latch.IsReady()) {
            DoBackgroundWork();  // Travail utile pendant l'attente
            NkThread::Yield();   // Céder le CPU
        }
        // Latch prêt : continuer
    }

    // ❌ MAUVAIS : Busy-wait sans Yield() (waste CPU)
    void BadPolling(NkLatch& latch)
    {
        while (!latch.IsReady()) {
            // ❌ Boucle active sans Yield() = consommation CPU inutile !
        }
    }

    // ✅ BON : Timeout pour éviter les hangs sur CountDown() manquant
    bool SafeWait(NkLatch& latch, nk_uint32 maxWaitMs)
    {
        return latch.Wait(maxWaitMs);  // Retourne false en cas de timeout
    }

    // ❌ MAUVAIS : Wait() infini sans garantie que tous les CountDown() seront appelés
    void RiskyWait(NkLatch& latch)
    {
        latch.Wait();  // ❌ Peut bloquer indéfiniment si un thread oublie CountDown() !
    }

    // ✅ BON : Documenter clairement qui doit appeler CountDown()
    /\**
     * @brief Traite un élément et signale sa completion au latch
     * @param latch Latch partagé pour synchronisation (doit être initialisé avec le bon compte)
     * @note Appelant responsable : doit appeler CountDown() exactement une fois par élément
     *\/
    void ProcessAndSignal(NkLatch& latch, const Item& item)
    {
        ProcessItem(item);
        latch.CountDown();  // ✅ Signal explicite de completion
    }

    // ---------------------------------------------------------------------
    // Exemple 9 : Testing unitaire de NkLatch
    // ---------------------------------------------------------------------
    #include <gtest/gtest.h>
    #include <NKThreading/Synchronization/NkLatch.h>
    #include <NKThreading/NkThread.h>
    #include <atomic>
    #include <vector>

    TEST(NkLatchTest, BasicCountDownAndWait)
    {
        constexpr nk_uint32 kCount = 4;
        nkentseu::threading::NkLatch latch(kCount);
        std::atomic<nk_uint32> signaled{0};

        // Lancer des threads qui appellent CountDown()
        std::vector<NkThread> signalers;
        for (nk_uint32 i = 0; i < kCount; ++i) {
            signalers.emplace_back([&latch, &signaled]() {
                signaled.fetch_add(1);
                latch.CountDown();
            });
        }

        // Thread qui attend la completion
        NkThread waiter([&latch]() {
            latch.Wait();  // Doit retourner quand les 4 CountDown() sont appelés
        });

        for (auto& t : signalers) t.Join();
        waiter.Join();

        EXPECT_EQ(signaled.load(), kCount);
        EXPECT_TRUE(latch.IsReady());
        EXPECT_EQ(latch.GetCount(), 0u);
    }

    TEST(NkLatchTest, BatchCountDown)
    {
        nkentseu::threading::NkLatch latch(10);

        // Décrémenter par lots
        latch.CountDown(3);  // Reste 7
        EXPECT_EQ(latch.GetCount(), 7u);

        latch.CountDown(5);  // Reste 2
        EXPECT_EQ(latch.GetCount(), 2u);

        latch.CountDown(3);  // Dépasse → clampé à 0
        EXPECT_EQ(latch.GetCount(), 0u);
        EXPECT_TRUE(latch.IsReady());
    }

    TEST(NkLatchTest, TimeoutBehavior)
    {
        nkentseu::threading::NkLatch latch(1);  // Jamais de CountDown()

        // Wait avec timeout court
        auto start = std::chrono::steady_clock::now();
        bool result = latch.Wait(50);  // 50ms timeout
        auto end = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        EXPECT_FALSE(result);  // Doit retourner false (timeout)
        EXPECT_GE(elapsed, 45);  // Environ 50ms (marge de tolérance)
        EXPECT_LE(elapsed, 100);  // Ne doit pas attendre beaucoup plus
    }

    TEST(NkLatchTest, MultipleWaiters)
    {
        constexpr nk_uint32 kCount = 3;
        nkentseu::threading::NkLatch latch(kCount);
        std::atomic<nk_uint32> waitersReturned{0};

        // Plusieurs threads qui attendent le même latch
        std::vector<NkThread> waiters;
        for (int i = 0; i < 5; ++i) {
            waiters.emplace_back([&latch, &waitersReturned]() {
                latch.Wait();  // Tous doivent retourner quand CountDown() x3 est appelé
                waitersReturned.fetch_add(1);
            });
        }

        // Threads qui signalent
        std::vector<NkThread> signalers;
        for (nk_uint32 i = 0; i < kCount; ++i) {
            signalers.emplace_back([&latch]() {
                latch.CountDown();
            });
        }

        for (auto& t : signalers) t.Join();
        for (auto& t : waiters) t.Join();

        // Tous les waiters doivent avoir retourné
        EXPECT_EQ(waitersReturned.load(), 5u);
        EXPECT_TRUE(latch.IsReady());
    }

    // ---------------------------------------------------------------------
    // Exemple 10 : Configuration CMake pour NkLatch
    // ---------------------------------------------------------------------
    # CMakeLists.txt - Intégration de NkLatch

    # NkLatch dépend de NkMutex et NkConditionVariable
    target_link_libraries(your_target PRIVATE
        NKThreading::NKThreading  # Inclut toutes les dépendances transitives
    )

    # Option pour activer les assertions de debug dans NkLatch
    option(NKLATCH_ENABLE_ASSERTS "Enable NKENTSEU_ASSERT in NkLatch" ON)
    if(NKLATCH_ENABLE_ASSERTS)
        target_compile_definitions(your_target PRIVATE NKENTSEU_ENABLE_ASSERTS)
    endif()

    // ---------------------------------------------------------------------
    // Exemple 11 : Debugging et instrumentation de NkLatch
    // ---------------------------------------------------------------------
    #ifdef NK_DEBUG
    #include <NKThreading/Synchronization/NkLatch.h>
    #include <NKCore/NkLogger.h>

    // Wrapper instrumenté pour logging des opérations sur latch
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

    // Usage en mode debug :
    // DebugLatch initLatch(4, "ServiceInitialization");
    #endif

    // ---------------------------------------------------------------------
    // Exemple 12 : Comparaison Latch vs Barrier vs Event
    // ---------------------------------------------------------------------
    /\*
    ┌─────────────────────────────────────────────────────────────┐
    │ COMPARAISON : NkLatch vs NkBarrier vs NkEvent              │
    ├─────────────────────────────────────────────────────────────┤
    │                                                             │
    │  NkLatch                     NkBarrier              NkEvent │
    │  ─────────                   ─────────          ─────────   │
    │                                                             │
    │  Compteur décrémente        Compteur de phase      Booléen │
    │  Usage unique               Réutilisable          Manuel/Auto │
    │  Wait() retourne quand      Wait() synchronise    Set()/Wait() │
    │  compteur == 0              N threads             Signal persistant │
    │                                                             │
    │  Cas d'usage :             Cas d'usage :         Cas d'usage : │
    │  • Completion de tâches    • Synchronisation    • Signal de shutdown │
    │  • Démarrage synchronisé     de phase itérative • Notification un-à-un │
    │  • Agrégation de résultats • Algorithmes BSP    • Flag d'initialisation │
    │                                                             │
    │  Règle de choix :                                          │
    │  • Besoin de compter des completions → NkLatch             │
    │  • Besoin de synchroniser des phases → NkBarrier           │
    │  • Besoin d'un signal booléen → NkEvent                    │
    └─────────────────────────────────────────────────────────────┘
    *\/
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================