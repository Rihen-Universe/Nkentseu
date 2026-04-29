//
// NkEvent.h
// =============================================================================
// Description :
//   Primitive de synchronisation de style Win32 permettant de signaler
//   un événement entre threads. Supporte les modes ManualReset et AutoReset
//   pour une flexibilité d'utilisation maximale.
//
// Caractéristiques :
//   - Deux modes : ManualReset (signal persistant) ou AutoReset (signal consommé)
//   - Timeout supporté : Wait(ms) avec horloge monotone pour précision
//   - Pulse() : signal éphémère pour réveiller les waiters actuels uniquement
//   - Thread-safe : toutes les opérations protégées par mutex + condition variable
//   - Garantie noexcept sur toutes les opérations publiques
//
// Algorithmes implémentés :
//   - Set/Reset : modification atomique de l'état signalé avec notification
//   - Wait : attente avec gestion des spurious wakeups via génération de pulse
//   - Pulse : signal temporaire qui ne réveille que les threads déjà en attente
//
// Types disponibles :
//   NkEvent : événement de synchronisation multiplateforme
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_THREADING_EVENT_H__
#define __NKENTSEU_THREADING_EVENT_H__

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKCore/NkTypes.h"
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
            // CLASSE : NkEvent
            // =====================================================================
            // Événement de synchronisation inspiré du modèle Win32.
            //
            // Représentation interne :
            //   - mSignaled : état booléen indiquant si l'événement est signalé
            //   - mManualReset : mode de fonctionnement (persistant vs consommé)
            //   - mWaiters : compteur de threads actuellement en attente
            //   - mPulseGeneration : compteur pour distinguer les signaux Pulse() successifs
            //   - mMutex + mCondVar : primitives natives pour la synchronisation
            //
            // Modes de fonctionnement :
            //   • ManualReset (true) :
            //     - Set() signale l'événement de manière persistante
            //     - Tous les Wait() actuels et futurs retournent immédiatement jusqu'à Reset()
            //     - Utile pour les flags de shutdown, d'initialisation, etc.
            //
            //   • AutoReset (false, défaut) :
            //     - Set() signale l'événement mais le reset automatiquement après un Wait()
            //     - Seul un thread en attente est réveillé par Set() (NotifyOne)
            //     - Utile pour la signalisation un-à-un ou le handoff de ressources
            //
            // Cas d'usage typiques :
            //   - Signal de shutdown gracieux entre threads
            //   - Notification de completion d'une tâche asynchrone
            //   - Coordination producteur-consommateur avec signal explicite
            //   - Démarrage/arrêt contrôlé de sous-systèmes
            // =================================================================
            class NKENTSEU_THREADING_CLASS_EXPORT NkEvent {
            public:

                // -------------------------------------------------------------
                // CONSTRUCTEURS ET RÈGLES DE COPIE
                // -------------------------------------------------------------

                /// @brief Constructeur : initialise l'événement avec mode et état initial.
                /// @param manualReset true = mode ManualReset, false = mode AutoReset (défaut).
                /// @param initialState true = événement initialement signalé, false = non-signalé.
                /// @note Garantie noexcept : aucune allocation ni exception possible.
                /// @note En mode AutoReset, un Wait() consommera le signal initial si initialState=true.
                explicit NkEvent(nk_bool manualReset = false, nk_bool initialState = false) noexcept;

                /// @brief Constructeur de copie supprimé : événement non-copiable.
                /// @note Un événement représente un état de signalisation partagé unique.
                NkEvent(const NkEvent&) = delete;

                /// @brief Opérateur d'affectation supprimé : événement non-affectable.
                NkEvent& operator=(const NkEvent&) = delete;

                // -------------------------------------------------------------
                // API DE SIGNALISATION (Set/Reset/Pulse)
                // -------------------------------------------------------------

                /// @brief Signale l'événement : réveille les threads en attente.
                /// @note Mode ManualReset : le signal persiste jusqu'à un Reset() explicite.
                /// @note Mode AutoReset : le signal est consommé par le premier Wait() qui retourne.
                /// @note Thread-safe : peut être appelé depuis n'importe quel thread.
                /// @note Non-bloquant : retourne immédiatement après la notification.
                void Set() noexcept;

                /// @brief Remet l'événement à l'état non-signalé.
                /// @note En mode ManualReset : annule l'effet d'un Set() précédent.
                /// @note En mode AutoReset : généralement inutile car le reset est automatique.
                /// @note Thread-safe : peut être appelé depuis n'importe quel thread.
                /// @note Non-bloquant : retourne immédiatement.
                void Reset() noexcept;

                /// @brief Pulse : signale puis reset immédiatement l'événement.
                /// @note Réveille uniquement les threads déjà en attente au moment de l'appel.
                /// @note Les Wait() appelés après Pulse() ne seront pas affectés (signal éphémère).
                /// @note Utile pour les notifications "fire-and-forget" sans persistance.
                /// @note Thread-safe : peut être appelé depuis n'importe quel thread.
                void Pulse() noexcept;

                // -------------------------------------------------------------
                // API D'ATTENTE (Wait/IsSignaled)
                // -------------------------------------------------------------

                /// @brief Attend que l'événement soit signalé.
                /// @param timeoutMs Durée d'attente maximale en millisecondes.
                ///        -1 = attente infinie (défaut), 0 = retour immédiat (polling).
                /// @return true si l'événement était signalé, false en cas de timeout.
                /// @note Mode AutoReset : consomme le signal (reset automatique après retour true).
                /// @note Mode ManualReset : ne consomme pas le signal (d'autres Wait() retournent aussi true).
                /// @note Thread-safe : peut être appelé depuis n'importe quel thread.
                /// @note Bloquant : suspend le thread jusqu'à signal ou timeout.
                [[nodiscard]] nk_bool Wait(nk_int32 timeoutMs = -1) noexcept;

                /// @brief Vérifie l'état courant de l'événement sans attendre.
                /// @return true si l'événement est signalé, false sinon.
                /// @note Non-bloquant : retourne immédiatement dans tous les cas.
                /// @note La valeur peut changer immédiatement après le retour (concurrence).
                /// @note Utile pour le polling ou le debugging, pas pour la synchronisation critique.
                [[nodiscard]] nk_bool IsSignaled() const noexcept;

            private:

                // -------------------------------------------------------------
                // MEMBRES PRIVÉS (état interne de l'événement)
                // -------------------------------------------------------------

                nk_bool mManualReset;              ///< Mode de fonctionnement : true=ManualReset, false=AutoReset
                nk_bool mSignaled;                 ///< État courant : true=signalé, false=non-signalé
                nk_uint32 mWaiters;                ///< Nombre de threads actuellement en attente sur cet événement
                nk_uint64 mPulseGeneration;        ///< Compteur de génération pour distinguer les Pulse() successifs
                mutable NkMutex mMutex;            ///< Mutex pour protéger l'accès concurrent aux membres
                mutable NkConditionVariable mCondVar;  ///< Condition variable pour l'attente efficace

            };

        }
    }

#endif

// =============================================================================
// EXEMPLES D'UTILISATION (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Signal de shutdown gracieux (ManualReset)
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkEvent.h>
    using namespace nkentseu::threading;

    void GracefulShutdown()
    {
        // Événement persistant : tous les threads voient le signal de shutdown
        NkEvent shutdownEvent(true);  // ManualReset

        std::vector<NkThread> workers;
        for (int i = 0; i < 4; ++i) {
            workers.emplace_back([&shutdownEvent, i]() {
                while (!shutdownEvent.IsSignaled()) {
                    DoWork(i);
                    // Vérifier périodiquement le signal de shutdown
                }
                Cleanup(i);  // Nettoyage avant sortie
            });
        }

        // ... application en cours ...

        // Signaler l'arrêt à tous les threads
        shutdownEvent.Set();  // ManualReset : persiste pour tous les Wait() futurs

        // Attendre que tous les threads terminent leur cleanup
        for (auto& t : workers) {
            t.Join();
        }
    }

    // ---------------------------------------------------------------------
    // Exemple 2 : Handoff de ressource entre threads (AutoReset)
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkEvent.h>

    void ResourceHandoff()
    {
        // Événement consommable : un seul thread est réveillé par Set()
        NkEvent dataReady(false);  // AutoReset (défaut)
        std::vector<int> sharedBuffer;

        // Thread producteur
        NkThread producer([&dataReady, &sharedBuffer]() {
            sharedBuffer = GenerateData();
            dataReady.Set();  // Signale que les données sont prêtes
            // Le signal est consommé par le premier consommateur
        });

        // Thread consommateur
        NkThread consumer([&dataReady, &sharedBuffer]() {
            dataReady.Wait();  // Attend le signal (bloque si pas encore prêt)
            // Le signal est consommé : AutoReset a fait Reset() automatiquement
            ProcessData(sharedBuffer);
        });

        producer.Join();
        consumer.Join();
    }

    // ---------------------------------------------------------------------
    // Exemple 3 : Timeout pour éviter les blocages infinis
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkEvent.h>
    #include <NKCore/NkLogger.h>

    bool WaitForInitialization(NkEvent& initEvent, nk_uint32 timeoutMs)
    {
        if (initEvent.Wait(timeoutMs)) {
            return true;  // Initialisation complétée dans le délai
        } else {
            NK_LOG_WARNING("Initialization timeout after {}ms", timeoutMs);
            return false;  // Timeout : gérer l'erreur ou fallback
        }
    }

    // Usage :
    // NkEvent initComplete;
    // if (!WaitForInitialization(initComplete, 5000)) {
    //     FallbackInitialization();
    // }

    // ---------------------------------------------------------------------
    // Exemple 4 : Pulse() pour notification éphémère
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkEvent.h>

    void EphemeralNotification()
    {
        NkEvent updateEvent(false);  // AutoReset

        // Threads qui pollent pour des mises à jour
        std::vector<NkThread> listeners;
        for (int i = 0; i < 3; ++i) {
            listeners.emplace_back([&updateEvent, i]() {
                // Wait avec timeout court pour polling
                if (updateEvent.Wait(100)) {
                    // Signal reçu : rafraîchir les données
                    RefreshData(i);
                }
                // Si timeout : continuer sans rafraîchissement
            });
        }

        // Thread émetteur : envoie une notification ponctuelle
        NkThread emitter([&updateEvent]() {
            if (DataHasChanged()) {
                updateEvent.Pulse();  // Réveille uniquement les listeners déjà en Wait()
                // Les listeners qui appellent Wait() après Pulse() ne seront pas affectés
            }
        });

        for (auto& t : listeners) t.Join();
        emitter.Join();
    }

    // ---------------------------------------------------------------------
    // Exemple 5 : Pattern "start barrier" avec événement
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkEvent.h>

    void SynchronizedStartWithEvent()
    {
        NkEvent startSignal(true);  // ManualReset, initialement non-signalé
        startSignal.Reset();        // S'assurer qu'il est non-signalé au départ

        std::vector<NkThread> workers;
        for (int i = 0; i < 4; ++i) {
            workers.emplace_back([&startSignal, i]() {
                startSignal.Wait();  // Attendre le signal de démarrage
                RunWorker(i);        // Démarrer simultanément
            });
        }

        // Préparations...
        PrepareSharedResources();

        // Donner le signal de départ : tous les workers repartent ensemble
        startSignal.Set();  // ManualReset : persiste pour tous les Wait()

        for (auto& t : workers) t.Join();
    }

    // ---------------------------------------------------------------------
    // Exemple 6 : Combinaison Event + ConditionVariable pour patterns avancés
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkEvent.h>
    #include <NKThreading/NkConditionVariable.h>

    class AdvancedSynchronizer {
    public:
        void WaitForCondition(std::function<bool()> predicate, nk_uint32 timeoutMs)
        {
            // Utiliser Event pour le signal global + ConditionVariable pour le prédicat
            if (mGlobalEvent.Wait(0)) {  // Polling non-bloquant
                mCondVar.WaitUntil(mLock, timeoutMs, predicate);
            }
        }

        void SignalGlobalChange()
        {
            mGlobalEvent.Pulse();  // Notifier les waiters qu'un changement est possible
            mCondVar.NotifyAll();  // Réveiller pour ré-évaluation du prédicat
        }

    private:
        NkEvent mGlobalEvent{false};  // AutoReset pour notifications éphémères
        NkConditionVariable mCondVar;
        NkMutex mLock;
    };

    // ---------------------------------------------------------------------
    // Exemple 7 : Bonnes pratiques et anti-patterns
    // ---------------------------------------------------------------------
    // ✅ BON : Utiliser ManualReset pour les flags persistants (shutdown, init)
    void GoodPersistentFlag()
    {
        NkEvent initialized(true);  // ManualReset pour persistance
        initialized.Reset();        // Commencer non-signalé

        // Thread worker :
        while (!initialized.IsSignaled()) {
            // Attendre l'initialisation
        }
        // Une fois signalé, tous les threads voient l'état persistant

        // Thread initialiseur :
        DoInitialization();
        initialized.Set();  // Signal persistant pour tous les waiters actuels et futurs
    }

    // ❌ MAUVAIS : Utiliser AutoReset pour un flag global (signal consommé)
    void BadPersistentFlag()
    {
        NkEvent initialized(false);  // AutoReset (défaut)

        // Thread 1 :
        initialized.Wait();  // Consomme le signal
        // Thread 2 : initialized.Wait() bloquera même si Set() a été appelé !

        // Thread initialiseur :
        initialized.Set();  // ❌ Réveille un seul thread, pas tous !
    }

    // ✅ BON : Utiliser IsSignaled() pour polling non-bloquant
    void GoodPolling(NkEvent& event)
    {
        while (!event.IsSignaled()) {
            DoBackgroundWork();  // Travail utile pendant l'attente
            NkThread::Yield();   // Céder le CPU pour ne pas brûler les cycles
        }
        // Événement signalé : continuer
    }

    // ❌ MAUVAIS : Busy-wait sans Yield() (waste CPU)
    void BadPolling(NkEvent& event)
    {
        while (!event.IsSignaled()) {
            // ❌ Boucle active sans Yield() = consommation CPU inutile !
        }
    }

    // ✅ BON : Timeout pour éviter les hangs infinis
    bool SafeWait(NkEvent& event, nk_uint32 maxWaitMs)
    {
        return event.Wait(maxWaitMs);  // Retourne false en cas de timeout
    }

    // ❌ MAUVAIS : Wait() infini sans garantie de signal
    void RiskyWait(NkEvent& event)
    {
        event.Wait();  // ❌ Peut bloquer indéfiniment si personne n'appelle Set() !
    }

    // ---------------------------------------------------------------------
    // Exemple 8 : Intégration avec NkThreadPool pour tâches asynchrones
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkEvent.h>
    #include <NKThreading/NkThreadPool.h>

    class AsyncTaskManager {
    public:
        void SubmitAsync(std::function<void()> task)
        {
            mPool.Enqueue([this, task = std::move(task)]() {
                try {
                    task();
                    mCompletionEvent.Set();  // Signaler la completion
                } catch (...) {
                    mCompletionEvent.Set();  // Signaler même en cas d'erreur
                }
            });
        }

        bool WaitForCompletion(nk_uint32 timeoutMs)
        {
            return mCompletionEvent.Wait(timeoutMs);
        }

        void Reset()
        {
            mCompletionEvent.Reset();  // Préparer pour la prochaine tâche
        }

    private:
        NkThreadPool mPool{4};
        NkEvent mCompletionEvent{true};  // ManualReset pour persistance du signal
    };

    // Usage :
    // AsyncTaskManager manager;
    // manager.SubmitAsync([]() { ExpensiveOperation(); });
    // if (manager.WaitForCompletion(5000)) {
    //     // Tâche complétée
    // } else {
    //     // Timeout : gérer l'erreur
    // }
    // manager.Reset();  // Préparer pour la prochaine soumission

    // ---------------------------------------------------------------------
    // Exemple 9 : Testing unitaire de NkEvent
    // ---------------------------------------------------------------------
    #include <gtest/gtest.h>
    #include <NKThreading/Synchronization/NkEvent.h>
    #include <NKThreading/NkThread.h>
    #include <atomic>
    #include <vector>

    TEST(NkEventTest, ManualResetPersistence)
    {
        nkentseu::threading::NkEvent event(true);  // ManualReset
        event.Reset();  // Commencer non-signalé

        std::atomic<nk_uint32> signaledCount{0};

        // Lancer plusieurs threads qui attendent l'événement
        std::vector<NkThread> waiters;
        for (int i = 0; i < 4; ++i) {
            waiters.emplace_back([&event, &signaledCount]() {
                if (event.Wait(1000)) {  // Timeout généreux
                    signaledCount.fetch_add(1);
                }
            });
        }

        // Petite pause pour laisser les threads entrer en Wait()
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Signaler l'événement : ManualReset = persistant
        event.Set();

        // Attendre que tous les threads terminent
        for (auto& t : waiters) {
            t.Join();
        }

        // Tous les threads ont vu le signal persistant
        EXPECT_EQ(signaledCount.load(), 4u);

        // Un nouveau Wait() retourne immédiatement car le signal persiste
        EXPECT_TRUE(event.Wait(0));  // Timeout 0 = polling
    }

    TEST(NkEventTest, AutoResetConsumption)
    {
        nkentseu::threading::NkEvent event(false);  // AutoReset
        std::atomic<nk_uint32> wokenCount{0};

        // Lancer plusieurs threads qui attendent
        std::vector<NkThread> waiters;
        for (int i = 0; i < 4; ++i) {
            waiters.emplace_back([&event, &wokenCount]() {
                if (event.Wait(1000)) {
                    wokenCount.fetch_add(1);
                }
            });
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Signaler : AutoReset = un seul thread réveillé
        event.Set();

        for (auto& t : waiters) {
            t.Join();
        }

        // Exactement un thread a été réveillé (les autres ont timeout)
        EXPECT_EQ(wokenCount.load(), 1u);
    }

    TEST(NkEventTest, PulseEphemeralBehavior)
    {
        nkentseu::threading::NkEvent event(false);  // AutoReset
        std::atomic<nk_uint32> earlyWoken{0};
        std::atomic<nk_uint32> lateWoken{0};

        // Threads "early" : en attente avant Pulse()
        std::vector<NkThread> early;
        for (int i = 0; i < 2; ++i) {
            early.emplace_back([&event, &earlyWoken]() {
                if (event.Wait(50)) {  // Timeout court
                    earlyWoken.fetch_add(1);
                }
            });
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        // Pulse : réveille uniquement les threads déjà en Wait()
        event.Pulse();

        // Threads "late" : commencent à attendre après Pulse()
        std::vector<NkThread> late;
        for (int i = 0; i < 2; ++i) {
            late.emplace_back([&event, &lateWoken]() {
                if (event.Wait(50)) {
                    lateWoken.fetch_add(1);
                }
            });
        }

        for (auto& t : early) t.Join();
        for (auto& t : late) t.Join();

        // Seul un thread "early" a été réveillé par Pulse() (AutoReset)
        EXPECT_EQ(earlyWoken.load(), 1u);
        // Les threads "late" n'ont pas vu le signal éphémère
        EXPECT_EQ(lateWoken.load(), 0u);
    }

    TEST(NkEventTest, TimeoutBehavior)
    {
        nkentseu::threading::NkEvent event(false);  // AutoReset, non-signalé

        // Wait avec timeout court sur événement non-signalé
        auto start = std::chrono::steady_clock::now();
        bool result = event.Wait(50);  // 50ms timeout
        auto end = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        // Doit retourner false (timeout) après environ 50ms
        EXPECT_FALSE(result);
        EXPECT_GE(elapsed, 45);  // Marge de tolérance pour le scheduler
        EXPECT_LE(elapsed, 100);  // Ne doit pas attendre beaucoup plus que demandé
    }

    // ---------------------------------------------------------------------
    // Exemple 10 : Configuration CMake pour NkEvent
    // ---------------------------------------------------------------------
    # CMakeLists.txt - Intégration de NkEvent

    # NkEvent dépend de NkMutex et NkConditionVariable
    target_link_libraries(your_target PRIVATE
        NKThreading::NKThreading  # Inclut toutes les dépendances transitives
    )

    # Option pour activer le logging debug dans NkEvent
    option(NKEVENT_ENABLE_DEBUG_LOGGING "Enable debug logging in NkEvent" OFF)
    if(NKEVENT_ENABLE_DEBUG_LOGGING)
        target_compile_definitions(your_target PRIVATE NKENTSEU_THREADING_DEBUG)
    endif()

    // ---------------------------------------------------------------------
    // Exemple 11 : Debugging et instrumentation de NkEvent
    // ---------------------------------------------------------------------
    #ifdef NK_DEBUG
    #include <NKThreading/Synchronization/NkEvent.h>
    #include <NKCore/NkLogger.h>

    // Wrapper instrumenté pour logging des opérations sur événement
    class DebugEvent : public NkEvent {
    public:
        explicit DebugEvent(nk_bool manualReset = false,
                           nk_bool initialState = false,
                           const nk_char* name = nullptr)
            : NkEvent(manualReset, initialState), mName(name ? name : "Unnamed") {}

        void Set() noexcept
        {
            NK_LOG_DEBUG("Event[{}]: Set() called", mName);
            NkEvent::Set();
        }

        void Reset() noexcept
        {
            NK_LOG_DEBUG("Event[{}]: Reset() called", mName);
            NkEvent::Reset();
        }

        nk_bool Wait(nk_int32 timeoutMs = -1) noexcept
        {
            auto tid = NkThread::GetCurrentThreadId();
            NK_LOG_DEBUG("Event[{}]: Thread {} calling Wait(timeout={}ms)",
                        mName, tid, timeoutMs);

            bool result = NkEvent::Wait(timeoutMs);

            NK_LOG_DEBUG("Event[{}]: Thread {} Wait() returned {}",
                        mName, tid, result ? "true" : "false");
            return result;
        }

    private:
        const nk_char* mName;
    };

    // Usage en mode debug :
    // DebugEvent initEvent(true, false, "InitializationComplete");
    #endif

    // ---------------------------------------------------------------------
    // Exemple 12 : Référence rapide : ManualReset vs AutoReset
    // ---------------------------------------------------------------------
    /\*
    ┌─────────────────────────────────────────────────────────────┐
    │ COMPARAISON : ManualReset vs AutoReset                      │
    ├─────────────────────────────────────────────────────────────┤
    │                                                             │
    │  ManualReset (true)          AutoReset (false, défaut)     │
    │  ─────────────────           ──────────────────────        │
    │                                                             │
    │  Set() :                       Set() :                      │
    │  • Signal persiste             • Signal consommé par        │
    │  • Tous les Wait() retournent    premier Wait() qui retourne│
    │    true jusqu'à Reset()        • NotifyOne() : un seul      │
    │  • NotifyAll() : tous les        thread réveillé            │
    │    waiters réveillés           • Wait() suivant bloquera    │
    │                                                             │
    │  Reset() :                     Reset() :                    │
    • Nécessaire pour annuler       • Généralement inutile        │
      le signal persistant            (auto-reset après Set())    │
    │                                                             │
    │  Cas d'usage :               Cas d'usage :                 │
    │  • Flags de shutdown         • Handoff de ressource        │
    │  • Signal d'initialisation   • Notification un-à-un        │
    │  • Démarrage synchronisé     • Producteur-consommateur     │
    │                                                             │
    └─────────────────────────────────────────────────────────────┘

    Règle empirique :
    • Si plusieurs threads doivent voir le même signal → ManualReset
    • Si un seul thread doit consommer le signal → AutoReset
    • En cas de doute → commencer par AutoReset (plus sûr)
    *\/
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================