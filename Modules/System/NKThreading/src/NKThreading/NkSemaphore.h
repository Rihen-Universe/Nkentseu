//
// NkSemaphore.h
// =============================================================================
// Description :
//   Primitive de synchronisation de type sémaphore compteur pour la coordination
//   entre threads. Permet de contrôler l'accès concurrent à un pool de ressources.
//
// Caractéristiques :
//   - Sémaphore compteur avec limite supérieure configurable (maxCount)
//   - Opérations Acquire/TryAcquire/Release thread-safe
//   - Support timeout via TryAcquireFor() avec horloge monotone
//   - Implémentation portable basée sur NkMutex/NkConditionVariable
//   - Garantie noexcept sur toutes les opérations publiques
//
// Algorithmes implémentés :
//   - Acquire : attente bloquante avec boucle while sur condition variable
//   - TryAcquireFor : deadline absolue + boucle d'attente avec timeout
//   - Release : incrémentation atomique + notification des waiters
//   - Protection contre overflow : vérification mCount + count <= mMaxCount
//
// Types disponibles :
//   NkSemaphore : sémaphore compteur multiplateforme
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_THREADING_SEMAPHORE_H__
#define __NKENTSEU_THREADING_SEMAPHORE_H__

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKCore/NkTypes.h"
    #include "NKThreading/NkMutex.h"
    #include "NKThreading/NkConditionVariable.h"
    #include "NKThreading/NkThreadingApi.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : NAMESPACE PRINCIPAL
    // -------------------------------------------------------------------------
    namespace nkentseu {

        // =====================================================================
        // SOUS-NAMESPACE : threading
        // =====================================================================
        namespace threading {

            // =================================================================
            // CLASSE : NkSemaphore
            // =================================================================
            // Sémaphore compteur pour la synchronisation entre threads.
            //
            // Représentation interne :
            //   - Compteur atomique protégé par NkMutex
            //   - NkConditionVariable pour l'attente efficace des threads bloqués
            //   - Limite supérieure (mMaxCount) pour éviter l'overflow du compteur
            //
            // Invariant de production :
            //   - 0 <= mCount <= mMaxCount à tout instant
            //   - Release() échoue si l'ajout dépasserait mMaxCount
            //   - Acquire() bloque tant que mCount == 0
            //
            // Cas d'usage typiques :
            //   - Pool de ressources limitées (connexions DB, buffers, workers)
            //   - Contrôle de concurrence (max N threads dans une section)
            //   - Synchronisation producteur-consommateur avec capacité bornée
            //
            // Différence vs NkMutex :
            //   - NkMutex : exclusion mutuelle binaire (1 thread à la fois)
            //   - NkSemaphore : accès concurrent limité à N threads (compteur)
            // =================================================================
            class NKENTSEU_THREADING_CLASS_EXPORT NkSemaphore {
            public:

                // -------------------------------------------------------------
                // CONSTRUCTEURS ET DESTRUCTEUR
                // -------------------------------------------------------------

                /// @brief Constructeur : initialise le sémaphore avec un compteur initial.
                /// @param initialCount Valeur initiale du compteur (ressources disponibles).
                /// @param maxCount Valeur maximale du compteur (capacité totale).
                /// @note Si maxCount < initialCount, maxCount est ajusté à initialCount.
                /// @note Garantie noexcept : aucune allocation ni exception possible.
                /// @note initialCount = 0 : sémaphore initialement indisponible (barrière).
                /// @note maxCount = 0xFFFFFFFF : pas de limite pratique (sémaphore "infini").
                explicit NkSemaphore(
                    nk_uint32 initialCount = 0u,
                    nk_uint32 maxCount = 0xFFFFFFFFu
                ) noexcept;

                /// @brief Destructeur : libère les ressources internes.
                /// @note Garantie noexcept : destruction toujours sûre.
                /// @note Les threads en attente sur Acquire() peuvent rester bloqués ;
                ///       il est de la responsabilité de l'appelant de les réveiller avant.
                ~NkSemaphore() = default;

                // -------------------------------------------------------------
                // RÈGLES DE COPIE ET DÉPLACEMENT (INTERDITES)
                // -------------------------------------------------------------

                /// @brief Constructeur de copie supprimé : sémaphore non-copiable.
                /// @note Un sémaphore représente une ressource système partagée unique.
                NkSemaphore(const NkSemaphore&) = delete;

                /// @brief Opérateur d'affectation supprimé : sémaphore non-affectable.
                NkSemaphore& operator=(const NkSemaphore&) = delete;

                // -------------------------------------------------------------
                // API D'ACQUISITION (consommation de ressources)
                // -------------------------------------------------------------

                /// @brief Acquiert une unité du sémaphore (bloquant).
                /// @note Bloque indéfiniment si mCount == 0 jusqu'à notification.
                /// @note Décrémente mCount de 1 après acquisition réussie.
                /// @warning Peut bloquer indéfiniment si personne n'appelle Release().
                /// @warning Appel récursif depuis le même thread = deadlock potentiel
                ///          si le thread détient déjà le mutex interne (rare mais possible).
                void Acquire() noexcept;

                /// @brief Tente d'acquérir sans blocage.
                /// @return true si acquis (mCount > 0), false sinon.
                /// @note Retourne immédiatement dans tous les cas (non-bloquant).
                /// @note Utile pour les algorithmes lock-free avec fallback.
                [[nodiscard]] nk_bool TryAcquire() noexcept;

                /// @brief Tente d'acquérir avec timeout maximal.
                /// @param milliseconds Durée d'attente maximale en millisecondes.
                /// @return true si acquis avant timeout, false sinon.
                /// @note Timeout relatif : compté depuis l'appel de la méthode.
                /// @note Précision dépend du scheduler OS (non garantie milliseconde).
                [[nodiscard]] nk_bool TryAcquireFor(nk_uint32 milliseconds) noexcept;

                // -------------------------------------------------------------
                // API DE LIBÉRATION (production de ressources)
                // -------------------------------------------------------------

                /// @brief Libère une ou plusieurs unités du sémaphore.
                /// @param count Nombre d'unités à libérer (par défaut : 1).
                /// @return true si libération réussie, false si overflow détecté.
                /// @note Incrémente mCount de 'count' et notifie 'count' waiters.
                /// @note Échoue si mCount + count > mMaxCount (protection overflow).
                /// @note NotifyOne() appelé 'count' fois pour réveiller les waiters.
                [[nodiscard]] nk_bool Release(nk_uint32 count = 1u) noexcept;

                // -------------------------------------------------------------
                // REQUÊTES D'ÉTAT (const, thread-safe)
                // -------------------------------------------------------------

                /// @brief Récupère la valeur courante du compteur.
                /// @return Nombre d'unités actuellement disponibles.
                /// @note Thread-safe : lecture protégée par mutex interne.
                /// @note Valeur potentiellement obsolète dès le retour (concurrence).
                /// @note Utile pour le debugging et les métriques, pas pour la logique.
                [[nodiscard]] nk_uint32 GetCount() const noexcept;

                /// @brief Récupère la capacité maximale du sémaphore.
                /// @return Valeur de mMaxCount fixée à la construction.
                /// @note Immutable après construction : pas de mutex requis.
                /// @note Utile pour valider les appels à Release() côté client.
                [[nodiscard]] nk_uint32 GetMaxCount() const noexcept;

            private:

                // -------------------------------------------------------------
                // MEMBRES PRIVÉS (données internes)
                // -------------------------------------------------------------

                nk_uint32 mCount;              ///< Compteur courant : ressources disponibles
                nk_uint32 mMaxCount;           ///< Capacité maximale : limite supérieure du compteur
                mutable NkMutex mMutex;        ///< Mutex pour protéger l'accès concurrent à mCount
                NkConditionVariable mCondVar;  ///< Condition pour l'attente efficace des acquireurs

            };

        }
    }

#endif

// =============================================================================
// EXEMPLES D'UTILISATION (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Pool de ressources limitées (pattern classique)
    // ---------------------------------------------------------------------
    #include <NKThreading/NkSemaphore.h>
    #include <NKThreading/NkMutex.h>
    using namespace nkentseu::threading;

    // Sémaphore pour gérer un pool de 5 connexions database
    NkSemaphore dbPoolSemaphore(5);  // 5 ressources disponibles au départ
    NkMutex dbPoolMutex;
    std::vector<DatabaseConnection*> dbPool;

    DatabaseConnection* AcquireConnection()
    {
        // Attendre qu'une connexion soit disponible (bloquant)
        dbPoolSemaphore.Acquire();

        // Accéder au pool protégé par mutex
        NkScopedLock lock(dbPoolMutex);
        DatabaseConnection* conn = dbPool.back();
        dbPool.pop_back();
        return conn;
    }

    void ReleaseConnection(DatabaseConnection* conn)
    {
        {
            NkScopedLock lock(dbPoolMutex);
            dbPool.push_back(conn);
        }
        // Libérer une unité du sémaphore après avoir rendu la connexion
        dbPoolSemaphore.Release();
    }

    // ---------------------------------------------------------------------
    // Exemple 2 : Contrôle de concurrence (max N threads dans une section)
    // ---------------------------------------------------------------------
    #include <NKThreading/NkSemaphore.h>

    // Limiter à 4 threads simultanés dans une section critique coûteuse
    NkSemaphore concurrencyLimit(4);

    void ExpensiveOperation(int threadId)
    {
        // Attendre une "place" dans la section critique
        if (!concurrencyLimit.TryAcquireFor(5000)) {  // Timeout 5s
            LogWarning("Thread {} timeout waiting for slot", threadId);
            return;
        }

        // Section critique : max 4 threads simultanés ici
        PerformExpensiveWork();

        // Libérer la place pour un autre thread
        concurrencyLimit.Release();
    }

    // ---------------------------------------------------------------------
    // Exemple 3 : Synchronisation producteur-consommateur avec capacité
    // ---------------------------------------------------------------------
    #include <NKThreading/NkSemaphore.h>
    #include <NKThreading/NkMutex.h>
    #include <queue>

    template<typename T>
    class BoundedQueue {
    public:
        explicit BoundedQueue(size_t capacity)
            : mSemaphore(capacity), mEmptySemaphore(0)
            , mCapacity(capacity) {}

        bool Enqueue(const T& item, nk_uint32 timeoutMs = 0)
        {
            // Attendre qu'il y ait de la place dans le buffer borné
            if (timeoutMs > 0) {
                if (!mSemaphore.TryAcquireFor(timeoutMs)) {
                    return false;  // Timeout : buffer plein
                }
            } else {
                mSemaphore.Acquire();  // Bloquant indéfiniment
            }

            {
                NkScopedLock lock(mMutex);
                mQueue.push(item);
            }
            mEmptySemaphore.Release();  // Notifier qu'un item est disponible
            return true;
        }

        bool Dequeue(T& outItem, nk_uint32 timeoutMs = 0)
        {
            // Attendre qu'un item soit disponible
            if (timeoutMs > 0) {
                if (!mEmptySemaphore.TryAcquireFor(timeoutMs)) {
                    return false;  // Timeout : buffer vide
                }
            } else {
                mEmptySemaphore.Acquire();
            }

            {
                NkScopedLock lock(mMutex);
                outItem = std::move(mQueue.front());
                mQueue.pop();
            }
            mSemaphore.Release();  // Libérer une place dans le buffer
            return true;
        }

    private:
        NkSemaphore mSemaphore;       // Places disponibles dans le buffer
        NkSemaphore mEmptySemaphore;  // Items disponibles dans le buffer
        NkMutex mMutex;
        std::queue<T> mQueue;
        size_t mCapacity;
    };

    // ---------------------------------------------------------------------
    // Exemple 4 : Barrière de synchronisation implicite (initialCount = 0)
    // ---------------------------------------------------------------------
    #include <NKThreading/NkSemaphore.h>

    // Sémaphore utilisé comme barrière : threads attendent un signal
    NkSemaphore startSignal(0);  // Initialement indisponible

    void WorkerThread(int id)
    {
        // Attendre le signal de démarrage
        startSignal.Acquire();  // Bloque jusqu'à Release() depuis le main

        // Tous les threads démarrent simultanément après le signal
        DoWork(id);
    }

    void StartAllWorkers(std::vector<std::thread>& workers)
    {
        // Lancer tous les threads (ils se bloquent sur Acquire())
        for (auto& t : workers) {
            t.start();
        }

        // Libérer le signal : tous les threads repartent simultanément
        startSignal.Release(workers.size());
    }

    // ---------------------------------------------------------------------
    // Exemple 5 : TryAcquire pour fallback non-bloquant
    // ---------------------------------------------------------------------
    #include <NKThreading/NkSemaphore.h>

    NkSemaphore rateLimit(10);  // Max 10 requêtes simultanées

    bool TryProcessRequest(Request& req)
    {
        // Tenter d'acquérir sans bloquer
        if (!rateLimit.TryAcquire()) {
            // Resource busy : mettre en file d'attente ou rejeter
            QueueForLater(req);
            return false;
        }

        // Traitement avec garantie de limite de concurrence
        ProcessRequest(req);
        rateLimit.Release();
        return true;
    }

    // ---------------------------------------------------------------------
    // Exemple 6 : Gestion d'erreur sur Release (overflow protection)
    // ---------------------------------------------------------------------
    #include <NKThreading/NkSemaphore.h>

    NkSemaphore limitedSem(0, 10);  // Max 10 unités

    void SafeRelease(NkSemaphore& sem, nk_uint32 count)
    {
        if (!sem.Release(count)) {
            // Échec : overflow détecté (mCount + count > mMaxCount)
            LogError("Semaphore overflow: cannot release {} units", count);
            // Stratégie de récupération : libérer partiellement ou ignorer
        }
    }

    // ---------------------------------------------------------------------
    // Exemple 7 : Requêtes d'état pour monitoring/debugging
    // ---------------------------------------------------------------------
    #include <NKThreading/NkSemaphore.h>

    void MonitorSemaphore(const NkSemaphore& sem, const char* name)
    {
        nk_uint32 available = sem.GetCount();
        nk_uint32 maxCap = sem.GetMaxCount();
        nk_uint32 inUse = maxCap - available;

        LogInfo("{}: {}/{} units in use", name, inUse, maxCap);

        // Note : ces valeurs sont instantanées et peuvent changer
        // immédiatement après la lecture à cause de la concurrence
    }

    // ---------------------------------------------------------------------
    // Exemple 8 : Pattern "token bucket" pour rate limiting
    // ---------------------------------------------------------------------
    #include <NKThreading/NkSemaphore.h>

    class TokenBucket {
    public:
        TokenBucket(nk_uint32 capacity, nk_uint32 refillRate)
            : mSemaphore(capacity), mCapacity(capacity)
            , mRefillRate(refillRate) {}

        bool TryConsume(nk_uint32 tokens = 1)
        {
            return mSemaphore.TryAcquire(tokens);
        }

        void Refill()
        {
            // Ajouter des tokens jusqu'à la capacité max
            nk_uint32 current = mSemaphore.GetCount();
            nk_uint32 toAdd = mCapacity - current;
            if (toAdd > 0) {
                mSemaphore.Release(toAdd);
            }
        }

    private:
        NkSemaphore mSemaphore;
        nk_uint32 mCapacity;
        nk_uint32 mRefillRate;  // Tokens par seconde (à gérer via timer externe)
    };

    // ---------------------------------------------------------------------
    // Exemple 9 : Bonnes pratiques et anti-patterns
    // ---------------------------------------------------------------------
    // ✅ BON : Toujours libérer dans le même thread (ou via RAII wrapper)
    class ScopedSemaphoreAcquire {
        NkSemaphore& mSem;
        nk_bool mAcquired;
    public:
        explicit ScopedSemaphoreAcquire(NkSemaphore& sem)
            : mSem(sem), mAcquired(sem.TryAcquire()) {}
        ~ScopedSemaphoreAcquire() { if (mAcquired) mSem.Release(); }
        [[nodiscard]] nk_bool Acquired() const { return mAcquired; }
    };

    // Usage :
    void SafeUsage(NkSemaphore& sem)
    {
        ScopedSemaphoreAcquire guard(sem);
        if (!guard.Acquired()) {
            return;  // Échec : pas de Release() à faire
        }
        DoWork();  // Release() automatique à la sortie
    }

    // ❌ MAUVAIS : Oublier de libérer après acquisition
    void LeakUsage(NkSemaphore& sem)
    {
        sem.Acquire();
        if (errorCondition) {
            return;  // ❌ Fuite : sem jamais libéré !
        }
        sem.Release();  // Peut ne jamais être atteint
    }

    // ✅ BON : Vérifier le retour de Release() en production
    void CheckedRelease(NkSemaphore& sem, nk_uint32 count)
    {
        if (!sem.Release(count)) {
            // Gérer l'erreur : logging, fallback, ou assertion en debug
            NK_ASSERT(false && "Semaphore overflow in Release()");
        }
    }

    // ❌ MAUVAIS : Appeler Release() sans avoir acquis (déséquilibre)
    void UnbalancedUsage(NkSemaphore& sem)
    {
        // ... code sans Acquire() ...
        sem.Release();  // ❌ Peut faire dépasser mMaxCount ou corrompre l'état
    }

    // ---------------------------------------------------------------------
    // Exemple 10 : Intégration avec système de tâches asynchrones
    // ---------------------------------------------------------------------
    #include <NKThreading/NkSemaphore.h>
    #include <NKThreading/NkThreadPool.h>

    class TaskLimiter {
    public:
        explicit TaskLimiter(nk_uint32 maxConcurrent)
            : mSemaphore(maxConcurrent) {}

        template<typename Func>
        void SubmitLimited(NkThreadPool& pool, Func&& task)
        {
            // Wrapper qui acquiert/libère automatiquement
            auto wrapped = [this, f = std::forward<Func>(task)]() mutable {
                // Acquire est déjà fait avant soumission au pool
                try {
                    f();
                } catch (...) {
                    // Gérer les exceptions sans fuir le Release()
                }
                mSemaphore.Release();  // Toujours libérer à la fin
            };

            // Acquérir avant de soumettre : bloque si limite atteinte
            mSemaphore.Acquire();
            pool.Enqueue(std::move(wrapped));
        }

    private:
        NkSemaphore mSemaphore;
    };

    // Usage :
    // TaskLimiter limiter(8);  // Max 8 tâches simultanées
    // limiter.SubmitLimited(pool, []() { ExpensiveTask(); });
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================