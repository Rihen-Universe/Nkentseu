//
// NkFuture.h
// =============================================================================
// Description :
//   Implémentation du pattern Future/Promise pour la programmation asynchrone.
//   Permet de découpler la production d'un résultat de sa consommation,
//   avec support natif de l'attente bloquante, du timeout et des exceptions.
//
// Caractéristiques :
//   - Pattern producer/consumer thread-safe via NkMutex/NkConditionVariable
//   - Support des types arbitraires via template (y compris void)
//   - Sémantique de déplacement pour transfert efficace des résultats
//   - Gestion des exceptions via handle opaque (NkExceptionHandle)
//   - Partage d'état via NkSharedPtr pour copie sécurisée des futures
//
// Algorithmes implémentés :
//   - Wait/WaitFor : attente bloquante avec condition variable native
//   - SetValue/SetException : notification atomique aux consommateurs
//   - Get : accès au résultat avec vérification de disponibilité
//   - IsReady : requête non-bloquante de l'état de complétion
//
// Types disponibles :
//   NkFuture<T>      : handle de lecture pour résultat asynchrone de type T
//   NkPromise<T>     : handle d'écriture pour produire le résultat de type T
//   NkFuture<void>   : spécialisation pour opérations sans valeur de retour
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_THREADING_FUTURE_H__
#define __NKENTSEU_THREADING_FUTURE_H__

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKCore/NkTypes.h"
    #include "NKMemory/NkSharedPtr.h"
    #include "NKThreading/NkThreadingApi.h"
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

            // -------------------------------------------------------------------------
            // TYPE ALIAS : Handle d'exception opaque
            // -------------------------------------------------------------------------
            // Représente une exception capturée de manière type-erased.
            // Permet de transporter des exceptions entre threads sans dépendance
            // à <exception> ou <std::exception_ptr> pour la portabilité.
            using NkExceptionHandle = void*;

            // -------------------------------------------------------------------------
            // FORWARD DECLARATION
            // -------------------------------------------------------------------------
            // NkPromise doit être déclaré avant NkFuture pour l'amitié template.
            template<typename T>
            class NkPromise;

            // =====================================================================
            // CLASSE TEMPLATE : NkFuture<T>
            // =====================================================================
            // Handle de lecture pour un résultat asynchrone de type T.
            //
            // Représentation interne :
            //   - État partagé (State) géré par NkSharedPtr pour référence counting
            //   - Mutex pour protéger l'accès concurrent au résultat
            //   - ConditionVariable pour l'attente efficace sans polling
            //
            // Invariant de production :
            //   - Un futur ne peut être "satisfait" qu'une seule fois (SetValue/SetException)
            //   - Après satisfaction, l'état est immutable et visible par tous les lecteurs
            //   - La copie d'un NkFuture partage le même état sous-jacent (shallow copy)
            //
            // Sémantique d'accès :
            //   - Get() : bloque si nécessaire, retourne const T& (pas de copie)
            //   - IsReady() : requête non-bloquante, thread-safe
            //   - Wait()/WaitFor() : attente bloquante avec/sans timeout
            //
            // Thread-safety :
            //   - Lecture concurrente : multiple readers OK après IsReady()==true
            //   - Écriture : seul le NkPromise associé peut appeler SetValue/SetException
            //   - Mixte : Get() peut être appelé pendant que Set*() est en cours (bloquant)
            // =====================================================================
            template<typename T = void>
            class NKENTSEU_THREADING_CLASS_EXPORT NkFuture {
                public:

                    // -------------------------------------------------------------
                    // TYPE ALIAS ET MÉTA-INFORMATIONS
                    // -------------------------------------------------------------

                    /// @brief Type de la valeur contenue dans ce futur.
                    using ValueType = T;

                    // -------------------------------------------------------------
                    // CONSTRUCTEURS ET OPÉRATEURS SPÉCIAUX
                    // -------------------------------------------------------------

                    /// @brief Constructeur par défaut : futur vide (non-associé).
                    /// @note Un futur vide retourne false à IsReady() et bloque indéfiniment sur Get().
                    NkFuture() noexcept = default;

                    /// @brief Constructeur de copie : partage l'état sous-jacent.
                    /// @note Les deux futures pointent vers le même résultat (shallow copy).
                    NkFuture(const NkFuture&) = default;

                    /// @brief Opérateur d'affectation par copie : partage l'état.
                    NkFuture& operator=(const NkFuture&) = default;

                    /// @brief Constructeur de déplacement : transfère la propriété.
                    /// @note L'instance source devient vide après le déplacement.
                    NkFuture(NkFuture&&) noexcept = default;

                    /// @brief Opérateur d'affectation par déplacement.
                    NkFuture& operator=(NkFuture&&) noexcept = default;

                    /// @brief Destructeur : libère la référence sur l'état partagé.
                    /// @note N'affecte pas les autres copies du même futur.
                    ~NkFuture() = default;

                    // -------------------------------------------------------------
                    // API D'ACCÈS AU RÉSULTAT
                    // -------------------------------------------------------------

                    /// @brief Récupère le résultat (attente bloquante si nécessaire).
                    /// @return Référence constante vers la valeur stockée.
                    /// @note Bloque indéfiniment si le résultat n'est pas encore prêt.
                    /// @note Retourne const T& : aucune copie de la valeur n'est effectuée.
                    /// @warning Comportement indéfini si le futur est vide (mState == nullptr).
                    /// @warning Si une exception a été définie via SetException(), le comportement
                    ///          dépend de l'implémentation du consumer (à documenter par l'utilisateur).
                    [[nodiscard]] const T& Get() const noexcept;

                    /// @brief Vérifie si le résultat est disponible sans bloquer.
                    /// @return true si SetValue() ou SetException() a été appelé, false sinon.
                    /// @note Thread-safe : peut être appelé depuis n'importe quel thread.
                    /// @note Non-bloquant : retourne immédiatement dans tous les cas.
                    [[nodiscard]] nk_bool IsReady() const noexcept;

                    /// @brief Attend que le résultat soit prêt (sans timeout).
                    /// @note Bloque le thread appelant jusqu'à notification ou erreur.
                    /// @note Utilise NkConditionVariable pour une attente efficace (pas de polling).
                    /// @warning Peut bloquer indéfiniment si personne n'appelle SetValue/SetException.
                    void Wait() const noexcept;

                    /// @brief Attend que le résultat soit prêt avec timeout maximal.
                    /// @param milliseconds Durée d'attente maximale en millisecondes.
                    /// @return true si le résultat est devenu prêt, false en cas de timeout.
                    /// @note Timeout relatif : compté depuis l'appel de la méthode.
                    /// @note Précision dépend du scheduler OS (non garantie milliseconde exacte).
                    [[nodiscard]] nk_bool WaitFor(nk_uint32 milliseconds) const noexcept;

                private:

                    // -------------------------------------------------------------
                    // CLASSE INTERNE : State (partagée entre Future et Promise)
                    // -------------------------------------------------------------
                    // Contient le résultat, l'état de complétion et les primitives
                    // de synchronisation. Partagée via NkSharedPtr pour référence counting.
                    class State {
                    public:

                        /// @brief Constructeur : initialise les primitives de sync.
                        State() noexcept = default;

                        // ---------------------------------------------------------
                        // MEMBRES DONNÉES (protégés par mMutex)
                        // ---------------------------------------------------------

                        mutable NkMutex mMutex;              ///< Mutex pour protéger l'accès concurrent
                        mutable NkConditionVariable mCond;   ///< Condition pour l'attente efficace
                        T mValue;                            ///< Valeur stockée (si T != void)
                        NkExceptionHandle mException;        ///< Handle d'exception (nullptr si succès)
                        nk_bool mReady;                      ///< Flag de complétion : true si satisfait

                    private:
                        // Amis pour accès direct à l'état interne
                        friend class NkFuture<T>;
                        friend class NkPromise<T>;
                    };

                    // -------------------------------------------------------------
                    // MEMBRES PRIVÉS DE NkFuture
                    // -------------------------------------------------------------

                    /// @brief Pointeur partagé vers l'état interne.
                    /// @note nullptr si le futur est "vide" (non-associé à un promise).
                    memory::NkSharedPtr<State> mState;

                    // -------------------------------------------------------------
                    // CLASSES AMIES
                    // -------------------------------------------------------------

                    /// @brief NkPromise<T> peut créer et initialiser l'état partagé.
                    friend class NkPromise<T>;
            };

            // =====================================================================
            // CLASSE TEMPLATE : NkPromise<T>
            // =====================================================================
            // Handle d'écriture pour produire un résultat asynchrone de type T.
            //
            // Responsabilités :
            //   - Créer l'état partagé (State) lors de la construction
            //   - Fournir un NkFuture<T> associé via GetFuture()
            //   - Notifier les consommateurs via SetValue() ou SetException()
            //
            // Invariant de production :
            //   - SetValue/SetException ne peut être appelé qu'une seule fois
            //   - Appels multiples = comportement indéfini (à éviter par l'utilisateur)
            //   - Après notification, l'état devient immutable et visible par tous
            //
            // Sémantique de transfert :
            //   - NkPromise est move-only : la responsabilité de satisfaction
            //     est transférée, pas copiée, pour éviter les conflits
            //   - Le NkFuture associé reste valide même après déplacement du promise
            //
            // Thread-safety :
            //   - SetValue/SetException : thread-safe, notifie atomiquement les waiters
            //   - GetFuture : peut être appelé avant ou après SetValue (retourne le même futur)
            // =====================================================================
            template<typename T>
            class NKENTSEU_THREADING_CLASS_EXPORT NkPromise {
                public:

                    // -------------------------------------------------------------
                    // CONSTRUCTEURS ET OPÉRATEURS SPÉCIAUX
                    // -------------------------------------------------------------

                    /// @brief Constructeur : crée l'état partagé associé.
                    /// @note Alloue un State via NkSharedPtr (allocation unique).
                    NkPromise() noexcept;

                    /// @brief Constructeur de copie supprimé : promise non-copiable.
                    /// @note La responsabilité de satisfaction ne peut être dupliquée.
                    NkPromise(const NkPromise&) = delete;

                    /// @brief Opérateur d'affectation supprimé : promise non-affectable.
                    NkPromise& operator=(const NkPromise&) = delete;

                    /// @brief Constructeur de déplacement : transfère la responsabilité.
                    /// @note L'instance source devient vide (ne peut plus satisfaire).
                    NkPromise(NkPromise&&) noexcept = default;

                    /// @brief Opérateur d'affectation par déplacement.
                    NkPromise& operator=(NkPromise&&) noexcept = default;

                    /// @brief Destructeur : libère la référence sur l'état partagé.
                    /// @note N'affecte pas les NkFuture déjà distribués.
                    ~NkPromise() = default;

                    // -------------------------------------------------------------
                    // API DE PRODUCTION DU RÉSULTAT
                    // -------------------------------------------------------------

                    /// @brief Récupère le futur associé à ce promise.
                    /// @return NkFuture<T> partageant le même état interne.
                    /// @note Peut être appelé multiple fois : retourne toujours le même futur.
                    /// @note Le futur reste valide même après déplacement du promise.
                    [[nodiscard]] NkFuture<T> GetFuture() const noexcept;

                    /// @brief Définit la valeur du résultat (copie).
                    /// @param value Valeur à stocker (copiée dans l'état partagé).
                    /// @note Notifie atomiquement tous les threads en attente via mCond.
                    /// @warning Ne doit être appelé qu'une seule fois (UB si appelé plusieurs fois).
                    void SetValue(const T& value) noexcept;

                    /// @brief Définit la valeur du résultat (déplacement).
                    /// @param value Valeur à stocker (déplacée dans l'état partagé).
                    /// @note Plus efficace que SetValue(const T&) pour les types coûteux.
                    /// @warning Ne doit être appelé qu'une seule fois (UB si appelé plusieurs fois).
                    void SetValue(T&& value) noexcept;

                    /// @brief Définit une exception comme résultat.
                    /// @param ex Handle opaque représentant l'exception capturée.
                    /// @note Permet de propager des erreurs entre threads sans throw/catch.
                    /// @warning Le consumer doit interpréter le handle via sa propre logique.
                    void SetException(NkExceptionHandle ex) noexcept;

                private:

                    // -------------------------------------------------------------
                    // MEMBRES PRIVÉS
                    // -------------------------------------------------------------

                    /// @brief Pointeur partagé vers l'état interne (créé à la construction).
                    memory::NkSharedPtr<typename NkFuture<T>::State> mState;
            };

            // =====================================================================
            // SPÉCIALISATION : NkFuture<void>
            // =====================================================================
            // Spécialisation pour les opérations asynchrones sans valeur de retour.
            //
            // Différences vs NkFuture<T> :
            //   - Pas de méthode Get() (pas de valeur à récupérer)
            //   - L'état interne ne contient pas de membre mValue
            //   - IsReady/Wait/WaitFor fonctionnent identiquement
            //
            // Cas d'usage typiques :
            //   - Synchronisation de fin de tâche (barrière implicite)
            //   - Signal de completion pour pipeline de traitement
            //   - Coordination de shutdown entre threads
            // =====================================================================
            template<>
            class NKENTSEU_THREADING_CLASS_EXPORT NkFuture<void> {
                public:

                    // -------------------------------------------------------------
                    // CONSTRUCTEURS ET OPÉRATEURS SPÉCIAUX
                    // -------------------------------------------------------------

                    /// @brief Constructeur par défaut : futur vide.
                    NkFuture() noexcept = default;

                    // -------------------------------------------------------------
                    // API D'ATTENTE (pas de Get() car pas de valeur)
                    // -------------------------------------------------------------

                    /// @brief Vérifie si l'opération est terminée.
                    /// @return true si SetVoid()/SetException() a été appelé.
                    [[nodiscard]] nk_bool IsReady() const noexcept;

                    /// @brief Attend la completion (sans timeout).
                    void Wait() const noexcept;

                    /// @brief Attend avec timeout maximal.
                    /// @param milliseconds Durée d'attente en millisecondes.
                    /// @return true si complété, false en cas de timeout.
                    [[nodiscard]] nk_bool WaitFor(nk_uint32 milliseconds) const noexcept;

                private:

                    // -------------------------------------------------------------
                    // CLASSE INTERNE : State (sans valeur)
                    // -------------------------------------------------------------
                    class State {
                    public:
                        State() noexcept = default;

                        mutable NkMutex mMutex;
                        mutable NkConditionVariable mCond;
                        NkExceptionHandle mException;
                        nk_bool mReady;

                    private:
                        friend class NkFuture<void>;
                        friend class NkPromise<void>;
                    };

                    // -------------------------------------------------------------
                    // MEMBRES PRIVÉS
                    // -------------------------------------------------------------
                    memory::NkSharedPtr<State> mState;

                    // -------------------------------------------------------------
                    // CLASSES AMIES
                    // -------------------------------------------------------------
                    friend class NkPromise<void>;
            };

        }
    }

#endif

// =============================================================================
// IMPLÉMENTATIONS TEMPLATE (dans le header car templates)
// =============================================================================

namespace nkentseu {
    namespace threading {

        // =====================================================================
        // IMPLÉMENTATION : NkFuture<T> (méthodes non-inline)
        // =====================================================================

        // ---------------------------------------------------------------------
        // Méthode : Get
        // ---------------------------------------------------------------------
        // Attend si nécessaire puis retourne une référence constante vers
        // la valeur stockée. Aucune copie n'est effectuée.
        //
        // Préconditions :
        //   - mState != nullptr (futur non-vide)
        //   - L'appelant accepte de bloquer si !IsReady()
        //
        // Postconditions :
        //   - Retourne const T& valide jusqu'à destruction du futur
        //   - Thread-safe : lecture concurrente OK après mReady == true
        // ---------------------------------------------------------------------
        template<typename T>
        inline const T& NkFuture<T>::Get() const noexcept
        {
            Wait();
            return mState->mValue;
        }

        // ---------------------------------------------------------------------
        // Méthode : IsReady
        // ---------------------------------------------------------------------
        // Requiert le mutex pour lire mReady de manière thread-safe.
        // Retourne immédiatement sans bloquer.
        // ---------------------------------------------------------------------
        template<typename T>
        inline nk_bool NkFuture<T>::IsReady() const noexcept
        {
            if (!mState) {
                return false;
            }
            NkScopedLock lock(mState->mMutex);
            return mState->mReady;
        }

        // ---------------------------------------------------------------------
        // Méthode : Wait
        // ---------------------------------------------------------------------
        // Bloque via NkConditionVariable jusqu'à ce que mReady == true.
        // Utilise WaitUntil avec prédicat pour gérer les spurious wakeups.
        // ---------------------------------------------------------------------
        template<typename T>
        inline void NkFuture<T>::Wait() const noexcept
        {
            if (!mState) {
                return;
            }
            NkScopedLock lock(mState->mMutex);
            mState->mCond.WaitUntil(lock, [this]() {
                return mState->mReady;
            });
        }

        // ---------------------------------------------------------------------
        // Méthode : WaitFor
        // ---------------------------------------------------------------------
        // Combine deadline absolue et prédicat pour un timeout robuste.
        // Retourne false si le délai expire avant que mReady ne devienne true.
        // ---------------------------------------------------------------------
        template<typename T>
        inline nk_bool NkFuture<T>::WaitFor(nk_uint32 milliseconds) const noexcept
        {
            if (!mState) {
                return false;
            }
            const nk_uint64 deadline = NkConditionVariable::GetMonotonicTimeMs()
                                     + static_cast<nk_uint64>(milliseconds);
            NkScopedLock lock(mState->mMutex);
            return mState->mCond.WaitUntil(lock, deadline, [this]() {
                return mState->mReady;
            });
        }

        // =====================================================================
        // IMPLÉMENTATION : NkPromise<T>
        // =====================================================================

        // ---------------------------------------------------------------------
        // Constructeur : création de l'état partagé
        // ---------------------------------------------------------------------
        template<typename T>
        inline NkPromise<T>::NkPromise() noexcept
            : mState(memory::MakeSharedPtr<typename NkFuture<T>::State>())
        {
            // Initialisation des membres de State via constructeur par défaut :
            // mReady = false, mException = nullptr, mValue default-construct
        }

        // ---------------------------------------------------------------------
        // Méthode : GetFuture
        // ---------------------------------------------------------------------
        // Retourne un NkFuture partageant le même pointeur d'état.
        // La copie du NkSharedPtr incrémente le compteur de références.
        // ---------------------------------------------------------------------
        template<typename T>
        inline NkFuture<T> NkPromise<T>::GetFuture() const noexcept
        {
            NkFuture<T> future;
            future.mState = mState;
            return future;
        }

        // ---------------------------------------------------------------------
        // Méthode : SetValue (copie)
        // ---------------------------------------------------------------------
        // Stocke la valeur par copie, marque l'état comme prêt, notifie les waiters.
        // Toutes les opérations sont protégées par le mutex pour atomicité.
        // ---------------------------------------------------------------------
        template<typename T>
        inline void NkPromise<T>::SetValue(const T& value) noexcept
        {
            if (!mState) {
                return;
            }
            NkScopedLock lock(mState->mMutex);
            if (mState->mReady) {
                return;  // Déjà satisfait : ignorer pour éviter UB
            }
            mState->mValue = value;
            mState->mReady = true;
            mState->mCond.NotifyAll();
        }

        // ---------------------------------------------------------------------
        // Méthode : SetValue (déplacement)
        // ---------------------------------------------------------------------
        // Version optimisée pour les types move-only ou coûteux à copier.
        // Utilise std::move sémantique via transfert direct.
        // ---------------------------------------------------------------------
        template<typename T>
        inline void NkPromise<T>::SetValue(T&& value) noexcept
        {
            if (!mState) {
                return;
            }
            NkScopedLock lock(mState->mMutex);
            if (mState->mReady) {
                return;
            }
            mState->mValue = static_cast<T&&>(value);
            mState->mReady = true;
            mState->mCond.NotifyAll();
        }

        // ---------------------------------------------------------------------
        // Méthode : SetException
        // ---------------------------------------------------------------------
        // Stocke un handle d'exception opaque et notifie les consommateurs.
        // Le consumer doit interpréter le handle via sa propre logique métier.
        // ---------------------------------------------------------------------
        template<typename T>
        inline void NkPromise<T>::SetException(NkExceptionHandle ex) noexcept
        {
            if (!mState) {
                return;
            }
            NkScopedLock lock(mState->mMutex);
            if (mState->mReady) {
                return;
            }
            mState->mException = ex;
            mState->mReady = true;
            mState->mCond.NotifyAll();
        }

        // =====================================================================
        // IMPLÉMENTATION : NkFuture<void> (spécialisation)
        // =====================================================================

        // ---------------------------------------------------------------------
        // Méthode : IsReady (void specialization)
        // ---------------------------------------------------------------------
        inline nk_bool NkFuture<void>::IsReady() const noexcept
        {
            if (!mState) {
                return false;
            }
            NkScopedLock lock(mState->mMutex);
            return mState->mReady;
        }

        // ---------------------------------------------------------------------
        // Méthode : Wait (void specialization)
        // ---------------------------------------------------------------------
        inline void NkFuture<void>::Wait() const noexcept
        {
            if (!mState) {
                return;
            }
            NkScopedLock lock(mState->mMutex);
            mState->mCond.WaitUntil(lock, [this]() {
                return mState->mReady;
            });
        }

        // ---------------------------------------------------------------------
        // Méthode : WaitFor (void specialization)
        // ---------------------------------------------------------------------
        inline nk_bool NkFuture<void>::WaitFor(nk_uint32 milliseconds) const noexcept
        {
            if (!mState) {
                return false;
            }
            const nk_uint64 deadline = NkConditionVariable::GetMonotonicTimeMs()
                                     + static_cast<nk_uint64>(milliseconds);
            NkScopedLock lock(mState->mMutex);
            return mState->mCond.WaitUntil(lock, deadline, [this]() {
                return mState->mReady;
            });
        }

    }
}

// =============================================================================
// EXEMPLES D'UTILISATION (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Pattern async basique avec retour de valeur
    // ---------------------------------------------------------------------
    #include <NKThreading/NkFuture.h>
    #include <NKThreading/NkThreadPool.h>
    using namespace nkentseu::threading;

    // Création d'un promise/future pair
    NkPromise<int> promise;
    NkFuture<int> future = promise.GetFuture();

    // Thread producteur : calcule et définit le résultat
    std::thread producer([&promise]() {
        int result = ExpensiveComputation();
        promise.SetValue(result);  // Notifie les consommateurs
    });

    // Thread consommateur : attend et récupère le résultat
    std::thread consumer([&future]() {
        // Option 1 : attente bloquante
        int value = future.Get();

        // Option 2 : attente avec timeout
        // if (future.WaitFor(5000)) {
        //     int value = future.Get();
        // } else {
        //     HandleTimeout();
        // }
    });

    producer.join();
    consumer.join();

    // ---------------------------------------------------------------------
    // Exemple 2 : Intégration avec NkThreadPool (cas d'usage principal)
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThreadPool.h>

    NkThreadPool pool(4);  // Pool avec 4 threads workers

    // Soumission d'une tâche avec futur
    NkFuture<std::string> future = pool.Enqueue([]() -> std::string {
        // Travail exécuté sur un thread du pool
        return FetchDataFromNetwork();
    });

    // Le thread submitter peut faire autre chose...
    DoOtherWork();

    // ...puis récupérer le résultat quand nécessaire
    std::string data = future.Get();  // Bloque si pas encore prêt
    ProcessData(data);

    // ---------------------------------------------------------------------
    // Exemple 3 : Gestion d'exceptions entre threads
    // ---------------------------------------------------------------------
    #include <NKThreading/NkFuture.h>

    NkPromise<void> promise;
    NkFuture<void> future = promise.GetFuture();

    // Thread worker : capture et propage une erreur
    std::thread worker([&promise]() {
        try {
            RiskyOperation();
            promise.SetValue();  // Succès : pas de valeur pour void
        } catch (...) {
            // Capture l'exception de manière opaque
            promise.SetException(CaptureCurrentException());
        }
    });

    // Thread principal : attend et vérifie le statut
    future.Wait();
    if (/\* check exception handle *\/) {
        HandleError();
    }

    // ---------------------------------------------------------------------
    // Exemple 4 : Synchronisation de fin de tâche (void future)
    // ---------------------------------------------------------------------
    #include <NKThreading/NkFuture.h>

    // Signal de completion pour une tâche de background
    NkPromise<void> doneSignal;
    NkFuture<void> doneFuture = doneSignal.GetFuture();

    // Thread background
    std::thread background([&doneSignal]() {
        PerformBackgroundTask();
        doneSignal.SetValue();  // Notifie la completion
    });

    // Thread principal : attend la fin sans valeur de retour
    doneFuture.Wait();
    // La tâche est terminée, on peut continuer
    CleanupAfterBackground();

    // ---------------------------------------------------------------------
    // Exemple 5 : Multiple consumers sur le même futur
    // ---------------------------------------------------------------------
    #include <NKThreading/NkFuture.h>

    NkPromise<int> promise;
    NkFuture<int> future = promise.GetFuture();

    // Plusieurs threads peuvent lire le même futur
    std::thread reader1([&future]() {
        int val = future.Get();  // Bloque jusqu'à disponibilité
        UseValue(val);
    });

    std::thread reader2([&future]() {
        // IsReady permet du polling non-bloquant si nécessaire
        while (!future.IsReady()) {
            DoLightWork();
        }
        int val = future.Get();  // Ne bloque pas ici
        UseValue(val);
    });

    // Producteur : satisfait le futur une seule fois
    promise.SetValue(42);  // Tous les readers sont notifiés

    reader1.join();
    reader2.join();

    // ---------------------------------------------------------------------
    // Exemple 6 : Chaînage de futures (pattern avancé)
    // ---------------------------------------------------------------------
    #include <NKThreading/NkFuture.h>

    // Fonction utilitaire pour chaîner des opérations async
    template<typename T, typename Func>
    NkFuture<decltype(Func{}(T{}))> Then(
        NkFuture<T> input,
        Func&& continuation
    ) {
        using ResultType = decltype(Func{}(T{}));
        NkPromise<ResultType> promise;
        NkFuture<ResultType> output = promise.GetFuture();

        // Thread dédié pour exécuter la continuation
        std::thread([input = std::move(input),
                     promise = std::move(promise),
                     continuation = std::forward<Func>(continuation)]() mutable {
            input.Wait();  // Attend le résultat d'entrée
            try {
                ResultType result = continuation(input.Get());
                promise.SetValue(std::move(result));
            } catch (...) {
                promise.SetException(CaptureCurrentException());
            }
        }).detach();

        return output;
    }

    // Usage :
    // auto finalFuture = Then(
    //     pool.Enqueue(FetchUser),
    //     [](User u) { return ProcessUser(u); }
    // );
    // auto result = finalFuture.Get();

    // ---------------------------------------------------------------------
    // Exemple 7 : Timeout avec fallback
    // ---------------------------------------------------------------------
    #include <NKThreading/NkFuture.h>

    bool TryGetWithFallback(
        NkFuture<int>& future,
        int defaultValue,
        nk_uint32 timeoutMs,
        int& outValue
    ) {
        if (future.WaitFor(timeoutMs)) {
            outValue = future.Get();
            return true;  // Succès dans le délai
        }
        outValue = defaultValue;
        return false;  // Timeout : fallback utilisé
    }

    // Usage :
    // int result;
    // if (TryGetWithFallback(future, -1, 1000, result)) {
    //     UseResult(result);
    // } else {
    //     UseFallback(-1);
    // }

    // ---------------------------------------------------------------------
    // Exemple 8 : Bonnes pratiques et anti-patterns
    // ---------------------------------------------------------------------
    // ✅ BON : Toujours vérifier IsReady() avant Get() si non-bloquant requis
    void NonBlockingCheck(NkFuture<int>& future)
    {
        if (future.IsReady()) {
            int val = future.Get();  // Ne bloquera pas
            Process(val);
        }
    }

    // ❌ MAUVAIS : Appeler Get() sans savoir si c'est acceptable de bloquer
    void RiskyGet(NkFuture<int>& future)
    {
        int val = future.Get();  // Peut bloquer indéfiniment !
    }

    // ✅ BON : Utiliser WaitFor() avec timeout pour éviter les hangs
    bool SafeGet(NkFuture<int>& future, int& out, nk_uint32 timeout)
    {
        if (future.WaitFor(timeout)) {
            out = future.Get();
            return true;
        }
        return false;  // Gérer le timeout proprement
    }

    // ✅ BON : Déplacer les promises pour transférer la responsabilité
    void TransferResponsibility(NkPromise<int>&& promise)
    {
        // 'promise' est maintenant responsable de la satisfaction
        // L'appelant ne peut plus l'utiliser (move semantics)
        std::thread([p = std::move(promise)]() mutable {
            p.SetValue(ComputeResult());
        }).detach();
    }

    // ❌ MAUVAIS : Copier un promise (interdit par design)
    // NkPromise<int> p1;
    // NkPromise<int> p2 = p1;  // ❌ Compilation error : copy deleted

    // ✅ BON : Partager un future entre multiple consumers
    void BroadcastResult(NkFuture<int> future)
    {
        // Le futur peut être copié : tous partagent le même état
        std::thread([f = future]() { Consumer1(f.Get()); }).detach();
        std::thread([f = future]() { Consumer2(f.Get()); }).detach();
        // Note : Get() peut être appelé multiple fois après IsReady()
    }

    // ---------------------------------------------------------------------
    // Exemple 9 : Intégration avec système de tâches hiérarchique
    // ---------------------------------------------------------------------
    class AsyncTask {
    public:
        virtual ~AsyncTask() = default;
        virtual void Execute() = 0;
    };

    class TaskRunner {
    public:
        template<typename Func>
        NkFuture<decltype(Func{}())> Submit(Func&& func)
        {
            using ResultType = decltype(func());
            NkPromise<ResultType> promise;
            auto future = promise.GetFuture();

            mPool.Enqueue([func = std::forward<Func>(func),
                          promise = std::move(promise)]() mutable {
                try {
                    if constexpr (std::is_same_v<ResultType, void>) {
                        func();
                        promise.SetValue();
                    } else {
                        promise.SetValue(func());
                    }
                } catch (...) {
                    promise.SetException(CaptureCurrentException());
                }
            });

            return future;
        }

    private:
        NkThreadPool mPool;
    };

    // Usage :
    // TaskRunner runner;
    // auto future = runner.Submit([]() { return HeavyComputation(); });
    // auto result = future.Get();
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================