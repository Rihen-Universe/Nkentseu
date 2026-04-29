//
// NkThreadPool.cpp
// =============================================================================
// Description :
//   Implémentation interne de NkThreadPool via pattern Pimpl.
//   Ce fichier contient la définition de NkThreadPoolImpl et les
//   méthodes non-template de l'interface publique.
//
// Architecture :
//   - NkThreadPool (header) : interface légère, forwarding vers mImpl
//   - NkThreadPoolImpl (cpp) : implémentation détaillée, isolation des dépendances
//   - Avantage : compilation incrémentale rapide, ABI stable pour les binaires
//
// Note sur le work-stealing :
//   L'implémentation actuelle utilise une queue globale protégée par mutex.
//   Pour un work-stealing réel avec queues locales par worker, étendre
//   NkThreadPoolImpl avec une structure de queues hiérarchiques et un
//   algorithme de vol de tâches entre workers idle.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

// =====================================================================
// Inclusion de l'en-tête de déclaration
// =====================================================================
#include "NKThreading/NkThreadPool.h"

// =====================================================================
// Inclusions internes pour l'implémentation
// =====================================================================
// Ces en-têtes ne sont nécessaires que pour l'implémentation interne.
// Leur exclusion du header public réduit les dépendances de compilation.
#include "NKCore/NkTraits.h"
#include "NKPlatform/NkCPUFeatures.h"
#include "NKThreading/NkConditionVariable.h"
#include "NKThreading/NkThread.h"
#include "NKContainers/Sequential/NkVector.h"

// =====================================================================
// Namespace : nkentseu::threading
// =====================================================================
namespace nkentseu {
    namespace threading {

        // =================================================================
        // CLASSE INTERNE : NkThreadPoolImpl
        // =================================================================
        // Implémentation détaillée du pool de threads, cachée derrière
        // le pointeur unique mImpl de l'interface publique NkThreadPool.
        //
        // Composants internes :
        //   - mWorkers : vecteur de threads workers exécutant WorkerLoop()
        //   - mQueue   : queue FIFO de tâches en attente d'exécution
        //   - mMutex   : protection de l'accès concurrent à mQueue et flags
        //   - mWorkAvailable : condition variable pour notifier les workers idle
        //   - mIdle    : condition variable pour notifier Join() quand queue vide
        //
        // Invariants :
        //   - mShutdown == true ⇒ aucune nouvelle tâche n'est acceptée
        //   - mActiveWorkers compte les workers actuellement en train d'exécuter
        //   - mTasksCompleted est incrémenté atomiquement après chaque exécution
        // =================================================================
        class NkThreadPoolImpl {
        public:

            // -------------------------------------------------------------
            // CONSTRUCTEUR / DESTRUCTEUR
            // -------------------------------------------------------------

            /// @brief Constructeur : initialise et démarre les worker threads.
            /// @param numWorkers Nombre de threads à créer (0 = auto-détection).
            explicit NkThreadPoolImpl(nk_uint32 numWorkers);

            /// @brief Destructeur : appelle Shutdown() pour arrêt gracieux.
            ~NkThreadPoolImpl();

            // -------------------------------------------------------------
            // API PUBLIQUE (appelée via forwarding depuis NkThreadPool)
            // -------------------------------------------------------------

            void Enqueue(Task task);
            void Join();
            void Shutdown();
            [[nodiscard]] nk_uint32 GetNumWorkers() const;
            [[nodiscard]] nk_size GetQueueSize() const;
            [[nodiscard]] nk_uint64 GetTasksCompleted() const;

        private:

            // -------------------------------------------------------------
            // BOUCLE PRINCIPALE DES WORKERS
            // -------------------------------------------------------------

            /// @brief Boucle d'exécution infinie pour chaque worker thread.
            /// @note Algorithme : wait → dequeue → execute → notify → repeat
            /// @note Gère gracieusement le shutdown via flag mShutdown.
            void WorkerLoop(void* userData);

            // -------------------------------------------------------------
            // MEMBRES PRIVÉS (état interne du pool)
            // -------------------------------------------------------------

            nk_uint32 mNumWorkers;                    ///< Nombre de threads workers configurés
            NkVector<NkThread> mWorkers;              ///< Collection des threads workers actifs
            NkQueue<Task> mQueue;                     ///< Queue FIFO des tâches en attente
            nk_bool mShutdown;                        ///< Flag d'arrêt gracieux
            nk_uint64 mTasksCompleted;                ///< Compteur cumulatif de tâches exécutées
            nk_uint32 mActiveWorkers;                 ///< Nombre de workers actuellement occupés
            mutable NkMutex mMutex;                   ///< Mutex pour protéger l'accès concurrent
            NkConditionVariable mWorkAvailable;       ///< Notification : nouvelle tâche disponible
            NkConditionVariable mIdle;                ///< Notification : pool idle (pour Join)
        };

        // =================================================================
        // IMPLÉMENTATION : NkThreadPoolImpl
        // =================================================================

        // -----------------------------------------------------------------
        // Constructeur : création et démarrage des workers
        // -----------------------------------------------------------------
        // Algorithme :
        //   1. Déterminer le nombre de workers : paramètre ou auto-détection CPU
        //   2. Réserver la capacité du vecteur mWorkers pour éviter realloc
        //   3. Pour chaque worker : créer un NkThread avec WorkerLoop comme entry point
        //   4. Les threads démarrent immédiatement et entrent en attente sur mWorkAvailable
        //
        // Gestion d'erreur :
        //   - Si numWorkers == 0 après auto-détection, fallback à 1 worker minimum
        //   - Les échecs de création de thread sont gérés par NkThread (noexcept)
        //   - Aucun cleanup spécial requis : les threads non-startés sont noop
        // -----------------------------------------------------------------
        NkThreadPoolImpl::NkThreadPoolImpl(nk_uint32 numWorkers)
            : mNumWorkers(numWorkers > 0
                              ? numWorkers
                              : static_cast<nk_uint32>(platform::NkGetLogicalCoreCount()))
            , mWorkers()
            , mQueue()
            , mShutdown(false)
            , mTasksCompleted(0u)
            , mActiveWorkers(0u)
            , mMutex()
            , mWorkAvailable()
            , mIdle()
        {
            // Fallback de sécurité : au moins 1 worker même si détection échoue
            if (mNumWorkers == 0u) {
                mNumWorkers = 1u;
            }

            // Pré-allocation pour éviter les reallocs pendant la création des threads
            mWorkers.Reserve(mNumWorkers);

            // Création et démarrage des worker threads
            for (nk_uint32 i = 0u; i < mNumWorkers; ++i) {
                // NkThread worker([this]() { WorkerLoop(); });
                // mWorkers.PushBack(traits::NkMove(worker));
                mWorkers.EmplaceBack([this, i](void* userData) { WorkerLoop(userData); });
            }
        }

        // -----------------------------------------------------------------
        // Destructeur : arrêt gracieux via Shutdown()
        // -----------------------------------------------------------------
        // Le destructeur délègue à Shutdown() pour garantir :
        //   - L'exécution des tâches restantes avant destruction
        //   - La libération propre des ressources thread
        //   - L'absence de fuites de ressources système
        //
        // Note : Shutdown() est idempotent : peut être appelé multiple fois
        // sans effet secondaire, ce qui le rend safe dans un destructeur.
        // -----------------------------------------------------------------
        NkThreadPoolImpl::~NkThreadPoolImpl()
        {
            Shutdown();
        }

        // -----------------------------------------------------------------
        // Méthode : Enqueue (soumission de tâche)
        // -----------------------------------------------------------------
        // Algorithme :
        //   1. Vérifier que la tâche est valide (non-null)
        //   2. Acquérir le mutex pour protéger l'accès à mQueue
        //   3. Si shutdown : ignorer silencieusement la tâche
        //   4. Sinon : déplacer la tâche dans la queue et notifier un worker
        //
        // Thread-safety :
        //   - Mutex protège mQueue et mShutdown contre les accès concurrents
        //   - NotifyOne() réveille un worker idle pour traiter la nouvelle tâche
        //   - Si tous les workers sont occupés, la tâche attend dans la queue
        //
        // Performance :
        //   - Fast path : mutex acquis rapidement, push O(1) dans queue
        //   - Slow path : si queue pleine (rare), allocation dynamique de la queue
        // -----------------------------------------------------------------
        void NkThreadPoolImpl::Enqueue(Task task)
        {
            if (!task) {
                return;
            }

            NkScopedLockMutex lock(mMutex);
            if (mShutdown) {
                return;
            }
            mQueue.Push(traits::NkMove(task));
            mWorkAvailable.NotifyOne();
        }

        // -----------------------------------------------------------------
        // Méthode : Join (attente de completion)
        // -----------------------------------------------------------------
        // Algorithme :
        //   1. Acquérir le mutex pour lecture protégée de l'état du pool
        //   2. Boucler tant que : queue non vide OU workers actifs > 0
        //   3. À chaque itération : attendre sur mIdle (relâche mutex temporairement)
        //   4. Au réveil : re-vérifier les conditions de sortie (spurious wakeup safe)
        //
        // Gestion des spurious wakeups :
        //   - while (!condition) au lieu de if : robuste aux réveils intempestifs
        //   - Requis par la sémantique des condition variables POSIX/Windows
        //
        // Postconditions :
        //   - À la sortie de Join() : mQueue.empty() && mActiveWorkers == 0
        //   - Toutes les tâches soumises avant l'appel ont été exécutées
        // -----------------------------------------------------------------
        void NkThreadPoolImpl::Join()
        {
            NkScopedLockMutex lock(mMutex);
            while (!mQueue.Empty() || mActiveWorkers > 0u) {
                mIdle.Wait(lock);
            }
        }

        // -----------------------------------------------------------------
        // Méthode : Shutdown (arrêt gracieux)
        // -----------------------------------------------------------------
        // Algorithme en deux phases :
        //
        // Phase 1 : Notification (avec mutex)
        //   1. Acquérir le mutex pour mise à jour atomique de mShutdown
        //   2. Si déjà shutdown : retour immédiat (idempotence)
        //   3. Marquer mShutdown = true pour rejeter les nouvelles soumissions
        //   4. NotifyAll() sur mWorkAvailable pour réveiller tous les workers idle
        //
        // Phase 2 : Attente (sans mutex, pour éviter deadlock)
        //   5. Pour chaque worker : si joinable, appeler Join() pour attendre sa fin
        //   6. Après tous les Join() : clear() le vecteur pour libérer les ressources
        //
        // Sécurité :
        //   - Le mutex est relâché avant les Join() pour éviter de bloquer les workers
        //   - Chaque worker vérifie mShutdown && queue.empty() avant de terminer
        //   - Aucune tâche n'est perdue : celles en queue sont exécutées avant shutdown
        // -----------------------------------------------------------------
        void NkThreadPoolImpl::Shutdown()
        {
            // Phase 1 : marquer shutdown et notifier les workers
            {
                NkScopedLockMutex lock(mMutex);
                if (mShutdown) {
                    return;  // Déjà shutdown : idempotence
                }
                mShutdown = true;
                mWorkAvailable.NotifyAll();  // Réveiller tous les workers pour qu'ils terminent
            }

            // Phase 2 : attendre la fin de chaque worker thread
            for (nk_size i = 0u; i < mWorkers.Size(); ++i) {
                if (mWorkers[i].Joinable()) {
                    mWorkers[i].Join();
                }
            }

            // Cleanup : libérer le vecteur de threads
            mWorkers.Clear();
        }

        // -----------------------------------------------------------------
        // Méthode : GetNumWorkers (requête de configuration)
        // -----------------------------------------------------------------
        // Retourne le nombre de workers configuré à la construction.
        //
        // Thread-safety :
        //   - Lecture de mNumWorkers : immutable après construction, pas de mutex requis
        //   - Safe à appeler depuis n'importe quel thread, à tout moment
        //
        // Usage :
        //   - Monitoring : afficher la configuration du pool dans les logs
        //   - Debugging : vérifier que l'auto-détection CPU a fonctionné
        //   - Métriques : calculer le throughput par worker (tasks/sec/worker)
        // -----------------------------------------------------------------
        nk_uint32 NkThreadPoolImpl::GetNumWorkers() const
        {
            return mNumWorkers;
        }

        // -----------------------------------------------------------------
        // Méthode : GetQueueSize (requête d'état)
        // -----------------------------------------------------------------
        // Retourne le nombre de tâches actuellement en attente dans la queue.
        //
        // Thread-safety :
        //   - Lecture protégée par mutex : pas de data race sur mQueue
        //   - Mais la valeur peut changer immédiatement après le retour (concurrence)
        //   - Ne pas utiliser pour prendre des décisions de synchronisation
        //
        // Usage :
        //   - Monitoring : détecter si la queue grossit anormalement (goulot)
        //   - Debugging : vérifier que les tâches sont bien consommées
        //   - Métriques : calculer le temps moyen d'attente dans la queue
        // -----------------------------------------------------------------
        nk_size NkThreadPoolImpl::GetQueueSize() const
        {
            NkScopedLockMutex lock(mMutex);
            return mQueue.Size();
        }

        // -----------------------------------------------------------------
        // Méthode : GetTasksCompleted (requête de métrique)
        // -----------------------------------------------------------------
        // Retourne le compteur cumulatif de tâches exécutées avec succès.
        //
        // Thread-safety :
        //   - Lecture protégée par mutex : cohérence avec les incréments dans WorkerLoop
        //   - Valeur monotone croissante : ne décrémente jamais
        //
        // Usage :
        //   - Métriques de performance : throughput = completed / elapsed_time
        //   - Debugging : vérifier que toutes les tâches soumises ont été exécutées
        //   - Logging : rapporter le nombre de tâches traitées dans les rapports
        // -----------------------------------------------------------------
        nk_uint64 NkThreadPoolImpl::GetTasksCompleted() const
        {
            NkScopedLockMutex lock(mMutex);
            return mTasksCompleted;
        }

        // -----------------------------------------------------------------
        // Méthode privée : WorkerLoop (boucle d'exécution des workers)
        // -----------------------------------------------------------------
        // Boucle infinie exécutée par chaque thread worker :
        //
        // Algorithme principal :
        //   for (;;) {
        //       1. Attendre une tâche (wait sur condition variable si queue vide)
        //       2. Si shutdown ET queue vide : sortir de la boucle (termination)
        //       3. Déqueue une tâche et incrémenter mActiveWorkers
        //       4. Exécuter la tâche (hors section critique pour parallélisme)
        //       5. Incrémenter mTasksCompleted et décrémenter mActiveWorkers
        //       6. Si queue vide ET aucun worker actif : notifier Join() via mIdle
        //   }
        //
        // Gestion du shutdown :
        //   - La condition de sortie est : mShutdown && mQueue.Empty()
        //   - Cela garantit que toutes les tâches soumises avant shutdown sont exécutées
        //   - Les nouvelles soumissions après shutdown sont ignorées (voir Enqueue)
        //
        // Performance :
        //   - La tâche est exécutée hors de la section critique (après release du mutex)
        //   - Cela permet à d'autres workers de dequeue pendant l'exécution
        //   - Minimise le temps de détention du mutex : uniquement pour dequeue/notify
        // -----------------------------------------------------------------
        void NkThreadPoolImpl::WorkerLoop(void*)
        {
            for (;;) {
                Task task;

                // Section critique : accès protégé à la queue et aux flags
                {
                    NkScopedLockMutex lock(mMutex);

                    // Attendre qu'une tâche soit disponible ou que shutdown soit signalé
                    while (!mShutdown && mQueue.Empty()) {
                        mWorkAvailable.Wait(lock);
                    }

                    // Condition de terminaison : shutdown + queue vide
                    if (mShutdown && mQueue.Empty()) {
                        return;
                    }

                    // Déqueue une tâche et marquer ce worker comme actif
                    task = traits::NkMove(mQueue.Front());
                    mQueue.Pop();
                    ++mActiveWorkers;
                }

                // Exécution de la tâche : hors section critique pour parallélisme
                if (task) {
                    task();
                }

                // Mise à jour des métriques et notification si pool devient idle
                {
                    NkScopedLockMutex lock(mMutex);
                    ++mTasksCompleted;
                    if (mActiveWorkers > 0u) {
                        --mActiveWorkers;
                    }
                    if (mQueue.Empty() && mActiveWorkers == 0u) {
                        mIdle.NotifyAll();  // Notifier Join() que le pool est idle
                    }
                }
            }
        }

        // =================================================================
        // IMPLÉMENTATION : NkThreadPool (interface publique - forwarding)
        // =================================================================
        // Ces méthodes sont des wrappers minces qui délèguent à mImpl.
        // Le pattern Pimpl permet de cacher les détails d'implémentation
        // et de réduire les dépendances de compilation pour les utilisateurs.

        // -----------------------------------------------------------------
        // Constructeur : délégation vers NkThreadPoolImpl
        // -----------------------------------------------------------------
        NkThreadPool::NkThreadPool(nk_uint32 numWorkers) noexcept
            : mImpl(memory::NkMakeUnique<NkThreadPoolImpl>(numWorkers))
        {
            // Construction de l'implémentation via unique_ptr
            // Les threads workers démarrent dans le constructeur de NkThreadPoolImpl
        }

        // -----------------------------------------------------------------
        // Destructeur : délégation vers ~NkThreadPoolImpl (via unique_ptr)
        // -----------------------------------------------------------------
        NkThreadPool::~NkThreadPool() noexcept = default;
        // La destruction de mImpl appelle automatiquement ~NkThreadPoolImpl
        // qui appelle Shutdown() pour arrêt gracieux

        // -----------------------------------------------------------------
        // Méthodes publiques : forwarding vers mImpl
        // -----------------------------------------------------------------
        void NkThreadPool::Enqueue(Task task) noexcept
        {
            mImpl->Enqueue(traits::NkMove(task));
        }

        void NkThreadPool::EnqueuePriority(Task task, nk_int32 priority) noexcept
        {
            // Stub pour extension future : actuellement ignore la priorité
            // Pour un support réel, étendre NkThreadPoolImpl avec priority queue
            Enqueue(traits::NkMove(task));
            (void)priority;  // Supprimer warning unused parameter
        }

        void NkThreadPool::EnqueueAffinity(Task task, nk_uint32 cpuCore) noexcept
        {
            // Stub pour extension future : actuellement ignore l'affinité
            // Pour un support réel, étendre NkThreadPoolImpl avec routing par core
            Enqueue(traits::NkMove(task));
            (void)cpuCore;  // Supprimer warning unused parameter
        }

        void NkThreadPool::Join() noexcept
        {
            mImpl->Join();
        }

        void NkThreadPool::Shutdown() noexcept
        {
            mImpl->Shutdown();
        }

        nk_uint32 NkThreadPool::GetNumWorkers() const noexcept
        {
            return mImpl->GetNumWorkers();
        }

        nk_size NkThreadPool::GetQueueSize() const noexcept
        {
            return mImpl->GetQueueSize();
        }

        nk_uint64 NkThreadPool::GetTasksCompleted() const noexcept
        {
            return mImpl->GetTasksCompleted();
        }

        // -----------------------------------------------------------------
        // Méthode statique : GetGlobal (singleton lazy)
        // -----------------------------------------------------------------
        NkThreadPool& NkThreadPool::GetGlobal() noexcept
        {
            // Magic static : initialisation thread-safe garantie par C++11
            // L'instance est créée au premier appel et détruite à la fin du programme
            static NkThreadPool pool;
            return pool;
        }

    }
}