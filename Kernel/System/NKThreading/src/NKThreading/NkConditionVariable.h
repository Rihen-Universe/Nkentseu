//
// NkConditionVariable.h
// =============================================================================
// Description :
//   Primitive de synchronisation permettant aux threads d'attendre des
//   conditions spécifiques avant de poursuivre leur exécution.
//
// Caractéristiques :
//   - Implémentation native : CONDITION_VARIABLE (Windows), pthread_cond_t (POSIX)
//   - Support des attentes avec timeout relatif et absolu
//   - Pattern predicate-based waiting pour éviter les spurious wakeups
//   - Intégration transparente avec NkMutex et NkScopedLockMutex
//   - Garantie noexcept sur toutes les opérations publiques
//
// Algorithmes implémentés :
//   - Wait/WaitFor : attente bloquante avec libération temporaire du mutex
//   - WaitUntil (deadline) : attente jusqu'à un timestamp monotone absolu
//   - WaitUntil (predicate) : boucle d'attente avec évaluation de prédicat
//   - NotifyOne/NotifyAll : réveil sélectif ou broadcast des threads en attente
//
// Types disponibles :
//   NkConditionVariable : primitive de condition multiplateforme
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_THREADING_CONDITIONVARIABLE_H__
#define __NKENTSEU_THREADING_CONDITIONVARIABLE_H__

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKCore/NkTypes.h"
    #include "NKPlatform/NkPlatformDetect.h"
    #include "NKThreading/NkMutex.h"
    #include "NKThreading/NkThreadingApi.h"

    #if !defined(NKENTSEU_PLATFORM_WINDOWS)
        #include <pthread.h>
        #include <time.h>
    #endif

    // -------------------------------------------------------------------------
    // SECTION 2 : NAMESPACE PRINCIPAL
    // -------------------------------------------------------------------------
    namespace nkentseu {

        // =====================================================================
        // SOUS-NAMESPACE : threading
        // =====================================================================
        namespace threading {

            // =================================================================
            // CLASSE : NkConditionVariable
            // =================================================================
            // Variable de condition pour la synchronisation entre threads.
            //
            // Représentation interne :
            //   - Windows : CONDITION_VARIABLE (légère, intégrée aux SRWLock)
            //   - POSIX   : pthread_cond_t avec gestion des timeouts absolus
            //
            // Invariant de production :
            //   - Toujours utilisée avec un NkMutex/NkScopedLockMutex associé
            //   - Le mutex doit être verrouillé avant tout appel à Wait/WaitFor
            //   - Le mutex est automatiquement relâché pendant l'attente
            //
            // Avantages vs polling manuel :
            //   - Attente bloquante efficace (pas de consommation CPU)
            //   - Réveil immédiat par NotifyOne/NotifyAll
            //   - Gestion native des timeouts par le kernel OS
            //
            // Pattern d'usage recommandé :
            //   while (!condition) { condVar.Wait(lock); }
            //   (toujours vérifier la condition après réveil - spurious wakeups)
            // =================================================================
            class NKENTSEU_THREADING_CLASS_EXPORT NkConditionVariable {
            public:

                // -------------------------------------------------------------
                // CONSTRUCTEURS ET DESTRUCTEUR
                // -------------------------------------------------------------

                /// @brief Constructeur par défaut : initialise la variable de condition.
                /// @note Garantie noexcept : l'initialisation ne peut pas échouer.
                /// @note Windows : InitializeConditionVariable() est infaillible.
                /// @note POSIX  : pthread_cond_init() échec marqué via mInitialized.
                NkConditionVariable() noexcept;

                /// @brief Destructeur : libère les ressources système.
                /// @note Garantie noexcept : nettoyage toujours sûr.
                /// @note Windows : CONDITION_VARIABLE ne nécessite pas de destruction.
                /// @note POSIX  : pthread_cond_destroy() appelé seulement si initialisé.
                ~NkConditionVariable() noexcept;

                // -------------------------------------------------------------
                // RÈGLES DE COPIE ET DÉPLACEMENT (INTERDITES)
                // -------------------------------------------------------------

                /// @brief Constructeur de copie supprimé : ressource non-copiable.
                NkConditionVariable(const NkConditionVariable&) = delete;

                /// @brief Opérateur d'affectation supprimé : ressource non-affectable.
                NkConditionVariable& operator=(const NkConditionVariable&) = delete;

                // -------------------------------------------------------------
                // API D'ATTENTE BASIQUE (sans timeout)
                // -------------------------------------------------------------

                /// @brief Attend une notification (bloquant, sans timeout).
                /// @param lock Guard RAII sur le mutex associé (sera relâché temporairement).
                /// @note Le mutex est automatiquement relâché pendant l'attente.
                /// @note Le mutex est ré-acquis avant le retour de cette méthode.
                /// @warning Le lock DOIT être détenu par le thread appelant.
                /// @warning Risque de spurious wakeup : toujours vérifier la condition.
                void Wait(NkScopedLockMutex& lock) noexcept;

                // -------------------------------------------------------------
                // API D'ATTENTE AVEC TIMEOUT RELATIF
                // -------------------------------------------------------------

                /// @brief Attend avec timeout relatif en millisecondes.
                /// @param lock Guard RAII sur le mutex associé.
                /// @param milliseconds Durée d'attente maximale.
                /// @return true si réveillé par notification, false si timeout/erreur.
                /// @note Timeout relatif : compté depuis l'appel de la méthode.
                /// @note Précision dépend du scheduler OS (non garantie milliseconde).
                [[nodiscard]] nk_bool WaitFor(NkScopedLockMutex& lock, nk_uint32 milliseconds) noexcept;

                // -------------------------------------------------------------
                // API D'ATTENTE AVEC PRÉDICAT (pattern recommandé)
                // -------------------------------------------------------------

                /// @brief Attend jusqu'à ce que le prédicat retourne true.
                /// @param lock Guard RAII sur le mutex associé.
                /// @param predicate Fonction/callable retournant bool.
                /// @note Boucle interne : Wait() appelé tant que predicate() == false.
                /// @note Protège contre les spurious wakeups du système.
                /// @tparam Predicate Type callable évaluant la condition d'arrêt.
                template<typename Predicate>
                void WaitUntil(NkScopedLockMutex& lock, Predicate&& predicate) noexcept;

                // -------------------------------------------------------------
                // API D'ATTENTE AVEC DEADLINE ABSOLUE
                // -------------------------------------------------------------

                /// @brief Attend jusqu'à une échéance monotonic absolue en ms.
                /// @param lock Guard RAII sur le mutex associé.
                /// @param deadlineMs Timestamp absolu (GetMonotonicTimeMs() + délai).
                /// @return true si réveillé avant deadline, false si timeout.
                /// @note Deadline absolue : utile pour les timeouts cumulatifs.
                /// @note Utilise GetMonotonicTimeMs() pour éviter les sauts d'horloge.
                [[nodiscard]] nk_bool WaitUntil(NkScopedLockMutex& lock, nk_uint64 deadlineMs) noexcept;

                /// @brief Attend jusqu'à deadline OU prédicat vrai.
                /// @param lock Guard RAII sur le mutex associé.
                /// @param deadlineMs Timestamp absolu d'échéance.
                /// @param predicate Fonction/callable retournant bool.
                /// @return true si predicate() devient true avant deadline, false sinon.
                /// @note Combinaison optimale : timeout de sécurité + condition logique.
                /// @tparam Predicate Type callable évaluant la condition d'arrêt.
                template<typename Predicate>
                [[nodiscard]] nk_bool WaitUntil(
                    NkScopedLockMutex& lock,
                    nk_uint64 deadlineMs,
                    Predicate&& predicate
                ) noexcept;

                // -------------------------------------------------------------
                // API DE NOTIFICATION (réveil des threads en attente)
                // -------------------------------------------------------------

                /// @brief Notifie un thread en attente (si présent).
                /// @note Sélection arbitraire parmi les threads bloqués sur Wait().
                /// @note Plus efficace que NotifyAll() quand un seul thread peut progresser.
                /// @note Aucun effet si aucun thread n'est en attente.
                void NotifyOne() noexcept;

                /// @brief Notifie tous les threads en attente.
                /// @note Tous les threads bloqués sur Wait() sont réveillés.
                /// @note Utile pour les conditions de broadcast (shutdown, reconfiguration).
                /// @note Peut causer du thundering herd : utiliser avec discernement.
                void NotifyAll() noexcept;

                // -------------------------------------------------------------
                // UTILITAIRES STATIQUES (horloge monotone)
                // -------------------------------------------------------------

                /// @brief Récupère le temps monotone courant en millisecondes.
                /// @return Timestamp en ms depuis le boot système (monotone, non-décrémentable).
                /// @note Utilisé pour les deadlines absolues dans WaitUntil().
                /// @note Windows : GetTickCount64() - wrap après ~584 jours.
                /// @note POSIX  : clock_gettime(CLOCK_MONOTONIC) - précision nanoseconde.
                static nk_uint64 GetMonotonicTimeMs() noexcept;

            private:

                // -------------------------------------------------------------
                // MÉTHODES UTILITAIRES INTERNES (POSIX uniquement)
                // -------------------------------------------------------------

                #if !defined(NKENTSEU_PLATFORM_WINDOWS)
                    /// @brief Convertit un timeout relatif en timespec absolu.
                    /// @param milliseconds Durée relative en millisecondes.
                    /// @return timespec absolu pour pthread_cond_timedwait().
                    /// @note Gère le débordement de tv_nsec vers tv_sec.
                    static timespec MakeAbsoluteTimeout(nk_uint32 milliseconds) noexcept;
                #endif

                // -------------------------------------------------------------
                // MEMBRES PRIVÉS (données internes)
                // -------------------------------------------------------------

                #if defined(NKENTSEU_PLATFORM_WINDOWS)
                    CONDITION_VARIABLE mCondVar;  ///< Handle natif Windows
                #else
                    pthread_cond_t mCondVar;      ///< Handle natif POSIX
                #endif

                nk_bool mInitialized;  ///< Indicateur d'initialisation réussie

                // -------------------------------------------------------------
                // CLASSES AMIES (accès privilégié pour interopérabilité)
                // -------------------------------------------------------------

                /// @brief NkMutex nécessite un accès pour l'intégration native.
                friend class NkMutex;
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
        // Template : WaitUntil avec prédicat (pattern recommandé)
        // ---------------------------------------------------------------------
        // Boucle d'attente qui évalue un prédicat après chaque réveil.
        // Protège contre les spurious wakeups et permet des conditions complexes.
        //
        // Pattern d'usage typique :
        //   condVar.WaitUntil(lock, []() { return !queue.empty(); });
        // ---------------------------------------------------------------------
        template<typename Predicate>
        inline void NkConditionVariable::WaitUntil(
            NkScopedLockMutex& lock,
            Predicate&& predicate
        ) noexcept {
            while (!predicate()) {
                Wait(lock);
            }
        }

        // ---------------------------------------------------------------------
        // Template : WaitUntil avec deadline ET prédicat
        // ---------------------------------------------------------------------
        // Combinaison optimale : timeout de sécurité + condition logique.
        // Retourne false si la deadline expire avant que le prédicat ne soit vrai.
        //
        // Pattern d'usage typique :
        //   auto deadline = NkConditionVariable::GetMonotonicTimeMs() + 5000;
        //   if (!condVar.WaitUntil(lock, deadline, []() { return dataReady; })) {
        //       // Timeout : gérer l'échec
        //   }
        // ---------------------------------------------------------------------
        template<typename Predicate>
        inline nk_bool NkConditionVariable::WaitUntil(
            NkScopedLockMutex& lock,
            nk_uint64 deadlineMs,
            Predicate&& predicate
        ) noexcept {
            while (!predicate()) {
                if (!WaitUntil(lock, deadlineMs)) {
                    return false;
                }
            }
            return true;
        }

    }
}

// =============================================================================
// EXEMPLES D'UTILISATION (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Pattern producteur-consommateur basique
    // ---------------------------------------------------------------------
    #include <NKThreading/NkMutex.h>
    #include <NKThreading/NkConditionVariable.h>
    #include <queue>

    using namespace nkentseu::threading;

    NkMutex queueMutex;
    NkConditionVariable queueCond;
    std::queue<int> workQueue;
    bool shutdown = false;

    void Producer(int item)
    {
        {
            NkScopedLockMutex lock(queueMutex);
            workQueue.push(item);
        }
        queueCond.NotifyOne();  // Réveille un consommateur
    }

    void Consumer()
    {
        while (true) {
            int item;
            {
                NkScopedLockMutex lock(queueMutex);
                queueCond.WaitUntil(lock, []() {
                    return !workQueue.empty() || shutdown;
                });

                if (shutdown && workQueue.empty()) {
                    return;  // Exit propre
                }

                item = workQueue.front();
                workQueue.pop();
            }
            ProcessItem(item);  // Traitement hors section critique
        }
    }

    // ---------------------------------------------------------------------
    // Exemple 2 : Timeout avec fallback sur opération alternative
    // ---------------------------------------------------------------------
    #include <NKThreading/NkMutex.h>
    #include <NKThreading/NkConditionVariable.h>

    NkMutex resourceMutex;
    NkConditionVariable resourceCond;
    bool resourceAvailable = false;

    bool TryAcquireResourceWithFallback(nk_uint32 timeoutMs)
    {
        NkScopedLockMutex lock(resourceMutex);

        // Attend la disponibilité OU le timeout
        bool acquired = resourceCond.WaitFor(lock, timeoutMs);

        if (acquired && resourceAvailable) {
            resourceAvailable = false;
            return true;  // Ressource acquise
        }

        // Fallback : opération alternative sans la ressource
        PerformAlternativeOperation();
        return false;
    }

    // ---------------------------------------------------------------------
    // Exemple 3 : Broadcast pour reconfiguration globale
    // ---------------------------------------------------------------------
    #include <NKThreading/NkConditionVariable.h>

    NkMutex configMutex;
    NkConditionVariable configCond;
    Config currentConfig;
    bool configChanged = false;

    void UpdateGlobalConfig(const Config& newConfig)
    {
        {
            NkScopedLockMutex lock(configMutex);
            currentConfig = newConfig;
            configChanged = true;
        }
        // NotifyAll : tous les threads doivent recharger la config
        configCond.NotifyAll();
    }

    void WorkerThread()
    {
        while (running) {
            Config localConfig;
            {
                NkScopedLockMutex lock(configMutex);
                configCond.WaitUntil(lock, []() { return configChanged; });
                localConfig = currentConfig;
                configChanged = false;
            }
            ProcessWithConfig(localConfig);
        }
    }

    // ---------------------------------------------------------------------
    // Exemple 4 : Deadline absolue pour timeout cumulatif
    // ---------------------------------------------------------------------
    #include <NKThreading/NkConditionVariable.h>

    bool WaitForMultipleConditions(
        NkConditionVariable& cond,
        NkScopedLockMutex& lock,
        nk_uint32 totalTimeoutMs
    ) {
        // Deadline absolue pour l'opération globale
        const nk_uint64 deadline = NkConditionVariable::GetMonotonicTimeMs()
                                 + static_cast<nk_uint64>(totalTimeoutMs);

        // Attente condition A
        if (!cond.WaitUntil(lock, deadline, []() { return conditionA; })) {
            return false;  // Timeout global
        }

        // Attente condition B (avec le temps restant)
        if (!cond.WaitUntil(lock, deadline, []() { return conditionB; })) {
            return false;  // Timeout global
        }

        return true;  // Les deux conditions satisfaites dans le délai
    }

    // ---------------------------------------------------------------------
    // Exemple 5 : Pattern "latch" pour synchronisation de phase
    // ---------------------------------------------------------------------
    class PhaseBarrier {
    public:
        explicit PhaseBarrier(nk_uint32 threadCount)
            : mCount(threadCount), mGeneration(0) {}

        void AwaitPhase(nk_uint32 expectedGeneration)
        {
            NkScopedLockMutex lock(mMutex);
            mCond.WaitUntil(lock, [this, expectedGeneration]() {
                return mGeneration != expectedGeneration;
            });
        }

        void AdvancePhase()
        {
            NkScopedLockMutex lock(mMutex);
            ++mGeneration;
            mCond.NotifyAll();  // Réveille tous les threads pour la phase suivante
        }

    private:
        NkMutex mMutex;
        NkConditionVariable mCond;
        nk_uint32 mCount;
        nk_uint32 mGeneration;
    };

    // ---------------------------------------------------------------------
    // Exemple 6 : Bonnes pratiques et anti-patterns
    // ---------------------------------------------------------------------
    // ✅ BON : Toujours vérifier la condition après Wait()
    void GoodWait(NkConditionVariable& cond, NkScopedLockMutex& lock, bool& ready)
    {
        while (!ready) {  // Boucle pour spurious wakeups
            cond.Wait(lock);
        }
    }

    // ❌ MAUVAIS : if au lieu de while (risque de spurious wakeup)
    void BadWait(NkConditionVariable& cond, NkScopedLockMutex& lock, bool& ready)
    {
        if (!ready) {  // ❌ Peut retourner avec ready==false !
            cond.Wait(lock);
        }
    }

    // ✅ BON : Utiliser WaitUntil avec prédicat (plus sûr et lisible)
    void BetterWait(NkConditionVariable& cond, NkScopedLockMutex& lock, bool& ready)
    {
        cond.WaitUntil(lock, [&ready]() { return ready; });
    }

    // ✅ BON : NotifyOne quand un seul thread peut progresser
    void SignalSingleConsumer(NkConditionVariable& cond)
    {
        cond.NotifyOne();  // Plus efficace que NotifyAll
    }

    // ✅ BON : NotifyAll pour broadcast global
    void SignalAllWorkers(NkConditionVariable& cond)
    {
        cond.NotifyAll();  // Tous les threads doivent réagir
    }

    // ❌ MAUVAIS : Appeler Notify sans mutex détenu (race condition)
    void UnsafeNotify(NkConditionVariable& cond, bool& flag)
    {
        flag = true;
        cond.NotifyOne();  // ❌ Thread peut manquer la notification !
    }

    // ✅ BON : Modifier la condition ET notifier avec mutex détenu
    void SafeNotify(NkConditionVariable& cond, NkMutex& mutex, bool& flag)
    {
        NkScopedLockMutex lock(mutex);
        flag = true;
        cond.NotifyOne();  // ✅ Notification garantie visible
    }

    // ---------------------------------------------------------------------
    // Exemple 7 : Intégration avec système de tâches asynchrones
    // ---------------------------------------------------------------------
    class TaskScheduler {
    public:
        void Submit(Task* task)
        {
            {
                NkScopedLockMutex lock(mQueueMutex);
                mPendingTasks.push(task);
            }
            mTaskAvailable.NotifyOne();
        }

        void Shutdown()
        {
            {
                NkScopedLockMutex lock(mQueueMutex);
                mShutdown = true;
            }
            mTaskAvailable.NotifyAll();  // Réveille tous les workers pour exit
        }

    private:
        Task* TryDequeue()
        {
            NkScopedLockMutex lock(mQueueMutex);
            mTaskAvailable.WaitUntil(lock, [this]() {
                return !mPendingTasks.empty() || mShutdown;
            });

            if (mShutdown && mPendingTasks.empty()) {
                return nullptr;
            }

            Task* task = mPendingTasks.front();
            mPendingTasks.pop();
            return task;
        }

        NkMutex mQueueMutex;
        NkConditionVariable mTaskAvailable;
        std::queue<Task*> mPendingTasks;
        bool mShutdown;
    };
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================
