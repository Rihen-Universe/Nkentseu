//
// NkReaderWriterLock.h
// =============================================================================
// Description :
//   Verrou lecteur-rédacteur optimisé pour les scénarios avec forte proportion
//   de lectures. Permet à plusieurs lecteurs d'accéder simultanément aux
//   données, tandis que les écritures obtiennent un accès exclusif.
//
// Caractéristiques :
//   - Lectures concurrentes : multiples NkReadLock peuvent être détenus simultanément
//   - Écritures exclusives : un seul NkWriteLock à la fois, exclut tous les lecteurs
//   - Prévention de starvation : les writers en attente bloquent les nouveaux readers
//   - Guards RAII : NkReadLock et NkWriteLock pour gestion automatique des verrous
//   - Garantie noexcept sur toutes les opérations publiques
//
// Algorithmes implémentés :
//   - LockRead : acquisition partagée si aucun writer actif ou en attente
//   - LockWrite : acquisition exclusive après attente que tous les readers terminent
//   - TryLock* : tentatives non-bloquantes pour fallback rapide
//   - Notification : réveil sélectif des waiters selon le type de verrou libéré
//
// Types disponibles :
//   NkReaderWriterLock : verrou lecteur-rédacteur principal
//   NkReadLock         : guard RAII pour verrouillage en lecture
//   NkWriteLock        : guard RAII pour verrouillage en écriture
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_THREADING_READERWRITERLOCK_H__
#define __NKENTSEU_THREADING_READERWRITERLOCK_H__

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
            // CLASSE : NkReaderWriterLock
            // =====================================================================
            // Verrou lecteur-rédacteur pour accès concurrent optimisé.
            //
            // Représentation interne :
            //   - mReaders : nombre de lecteurs actuellement détenteurs du verrou
            //   - mWriters : nombre d'écrivains actifs (0 ou 1 par invariant)
            //   - mWritersWaiting : nombre d'écrivains en attente (pour prévention de starvation)
            //   - mMutex : mutex interne pour protéger l'accès aux compteurs
            //   - mReadCondVar / mWriteCondVar : conditions variables pour attente sélective
            //
            // Invariants de production :
            //   - Si mWriters == 1, alors mReaders == 0 et mWritersWaiting == 0 (exclusivité)
            //   - Si mWritersWaiting > 0, les nouveaux LockRead() bloquent (prévention starvation)
            //   - mReaders peut être > 1 uniquement si mWriters == 0 et mWritersWaiting == 0
            //
            // Politique d'ordonnancement :
            //   • Préférence aux writers en attente : évite la starvation des écritures
            //   • Lectures concurrentes maximales : tous les readers actifs peuvent lire ensemble
            //   • Notification sélective : NotifyOne() pour writers, NotifyAll() pour readers
            //
            // Cas d'usage typiques :
            //   - Cache en mémoire avec lectures fréquentes et mises à jour rares
            //   - Configuration partagée : lecture par nombreux threads, écriture par admin
            //   - Structures de données immuables avec mise à jour par copy-on-write
            //   - Index de recherche : lectures parallèles, rebuild périodique exclusif
            // =================================================================
            class NKENTSEU_THREADING_CLASS_EXPORT NkReaderWriterLock {
            public:

                // -------------------------------------------------------------
                // CONSTRUCTEURS ET RÈGLES DE COPIE
                // -------------------------------------------------------------

                /// @brief Constructeur par défaut : initialise les compteurs à zéro.
                /// @note Garantie noexcept : aucune allocation ni exception possible.
                /// @note Le verrou est immédiatement utilisable après construction.
                NkReaderWriterLock() noexcept;

                /// @brief Constructeur de copie supprimé : verrou non-copiable.
                /// @note Un verrou représente une ressource de synchronisation unique.
                NkReaderWriterLock(const NkReaderWriterLock&) = delete;

                /// @brief Opérateur d'affectation supprimé : verrou non-affectable.
                NkReaderWriterLock& operator=(const NkReaderWriterLock&) = delete;

                // -------------------------------------------------------------
                // API LECTURE (shared access)
                // -------------------------------------------------------------

                /// @brief Acquiert le verrou en mode lecture (partagé).
                /// @note Plusieurs threads peuvent détenir le verrou en lecture simultanément.
                /// @note Bloque si un writer est actif OU si des writers sont en attente.
                /// @note Thread-safe : peut être appelé depuis n'importe quel thread.
                /// @warning Ne pas appeler LockRead() depuis un thread qui détient déjà un lock
                ///          sur ce verrou (sauf si NkRecursiveMutex est utilisé en interne).
                void LockRead() noexcept;

                /// @brief Tente d'acquérir le verrou en lecture sans blocage.
                /// @return true si acquis, false si un writer est actif ou en attente.
                /// @note Retourne immédiatement dans tous les cas (non-bloquant).
                /// @note Utile pour les algorithmes avec fallback rapide.
                [[nodiscard]] nk_bool TryLockRead() noexcept;

                /// @brief Libère le verrou en mode lecture.
                /// @note Décrémente le compteur de readers.
                /// @note Si ce thread était le dernier reader, notifie un writer en attente.
                /// @warning Doit être appelé par le thread qui a acquis le lock en lecture.
                /// @warning Appel sans LockRead() préalable = comportement indéfini.
                void UnlockRead() noexcept;

                // -------------------------------------------------------------
                // API ÉCRITURE (exclusive access)
                // -------------------------------------------------------------

                /// @brief Acquiert le verrou en mode écriture (exclusif).
                /// @note Seul un thread peut détenir le verrou en écriture à la fois.
                /// @note Exclut tous les readers et autres writers pendant la détention.
                /// @note Bloque jusqu'à ce que tous les readers actifs aient terminé.
                /// @note Thread-safe : peut être appelé depuis n'importe quel thread.
                /// @warning Ne pas appeler LockWrite() depuis un thread qui détient déjà un lock.
                void LockWrite() noexcept;

                /// @brief Tente d'acquérir le verrou en écriture sans blocage.
                /// @return true si acquis, false si des readers/writers sont actifs ou en attente.
                /// @note Retourne immédiatement dans tous les cas (non-bloquant).
                /// @note Utile pour les mises à jour opportunistes avec fallback.
                [[nodiscard]] nk_bool TryLockWrite() noexcept;

                /// @brief Libère le verrou en mode écriture.
                /// @note Décrémente le compteur de writers.
                /// @note Notifie tous les readers en attente, puis un writer si présent.
                /// @warning Doit être appelé par le thread qui a acquis le lock en écriture.
                /// @warning Appel sans LockWrite() préalable = comportement indéfini.
                void UnlockWrite() noexcept;

            private:

                // -------------------------------------------------------------
                // MEMBRES PRIVÉS (état interne du verrou)
                // -------------------------------------------------------------

                nk_uint32 mReaders;              ///< Nombre de lecteurs actuellement détenteurs du verrou
                nk_uint32 mWriters;              ///< Nombre d'écrivains actifs (0 ou 1 par invariant)
                nk_uint32 mWritersWaiting;       ///< Nombre d'écrivains en attente (pour prévention de starvation)
                mutable NkMutex mMutex;          ///< Mutex pour protéger l'accès concurrent aux compteurs
                mutable NkConditionVariable mReadCondVar;  ///< Condition variable pour l'attente des readers
                mutable NkConditionVariable mWriteCondVar; ///< Condition variable pour l'attente des writers

            };

            // =================================================================
            // CLASSE : NkReadLock (RAII guard for read access)
            // =====================================================================
            // Guard RAII pour verrouillage en lecture sur NkReaderWriterLock.
            //
            // Pattern Resource Acquisition Is Initialization :
            //   - Acquisition du read lock dans le constructeur (immédiate)
            //   - Libération automatique dans le destructeur (garantie même en cas d'exception)
            //   - Non-copiable : un lock ne peut être dupliqué (ressource unique)
            //
            // Avantages :
            //   - Prévention systématique des oublis d'UnlockRead()
            //   - Code plus lisible : la portée du lock est visuellement délimitée
            //   - Sécurité exceptionnelle : rollback automatique sur throw
            // =================================================================
            class NKENTSEU_THREADING_CLASS_EXPORT NkReadLock {
            public:

                // -------------------------------------------------------------
                // CONSTRUCTEURS ET DESTRUCTEUR
                // -------------------------------------------------------------

                /// @brief Constructeur : acquiert immédiatement le read lock.
                /// @param lock Référence vers le NkReaderWriterLock à protéger.
                /// @note Garantie noexcept : LockRead() ne lève pas d'exception.
                /// @note Le verrou en lecture est détenu dès la fin de cette ligne.
                explicit NkReadLock(NkReaderWriterLock& lock) noexcept;

                /// @brief Destructeur : libère automatiquement le read lock.
                /// @note Garantie noexcept : UnlockRead() est toujours sûr.
                /// @note Appel automatique à la sortie de portée (même sur exception).
                ~NkReadLock() noexcept;

                // -------------------------------------------------------------
                // RÈGLES DE COPIE (INTERDITES)
                // -------------------------------------------------------------

                /// @brief Constructeur de copie supprimé : lock non-copiable.
                NkReadLock(const NkReadLock&) = delete;

                /// @brief Opérateur d'affectation supprimé : lock non-affectable.
                NkReadLock& operator=(const NkReadLock&) = delete;

            private:

                // -------------------------------------------------------------
                // MEMBRES PRIVÉS
                // -------------------------------------------------------------

                /// @brief Référence vers le verrou géré.
                NkReaderWriterLock& mLock;
            };

            // =================================================================
            // CLASSE : NkWriteLock (RAII guard for write access)
            // =====================================================================
            // Guard RAII pour verrouillage en écriture sur NkReaderWriterLock.
            //
            // Pattern Resource Acquisition Is Initialization :
            //   - Acquisition du write lock dans le constructeur (immédiate)
            //   - Libération automatique dans le destructeur (garantie même en cas d'exception)
            //   - Non-copiable : un lock ne peut être dupliqué (ressource unique)
            //
            // Différence vs NkReadLock :
            //   - NkWriteLock : accès exclusif, exclut tous les autres threads
            //   - NkReadLock  : accès partagé, compatible avec d'autres readers
            //   - Choisir en fonction du type d'accès requis (lecture vs modification)
            // =================================================================
            class NKENTSEU_THREADING_CLASS_EXPORT NkWriteLock {
            public:

                // -------------------------------------------------------------
                // CONSTRUCTEURS ET DESTRUCTEUR
                // -------------------------------------------------------------

                /// @brief Constructeur : acquiert immédiatement le write lock.
                /// @param lock Référence vers le NkReaderWriterLock à protéger.
                /// @note Garantie noexcept : LockWrite() ne lève pas d'exception.
                /// @note Le verrou en écriture est détenu dès la fin de cette ligne.
                explicit NkWriteLock(NkReaderWriterLock& lock) noexcept;

                /// @brief Destructeur : libère automatiquement le write lock.
                /// @note Garantie noexcept : UnlockWrite() est toujours sûr.
                /// @note Appel automatique à la sortie de portée (même sur exception).
                ~NkWriteLock() noexcept;

                // -------------------------------------------------------------
                // RÈGLES DE COPIE (INTERDITES)
                // -------------------------------------------------------------

                /// @brief Constructeur de copie supprimé : lock non-copiable.
                NkWriteLock(const NkWriteLock&) = delete;

                /// @brief Opérateur d'affectation supprimé : lock non-affectable.
                NkWriteLock& operator=(const NkWriteLock&) = delete;

            private:

                // -------------------------------------------------------------
                // MEMBRES PRIVÉS
                // -------------------------------------------------------------

                /// @brief Référence vers le verrou géré.
                NkReaderWriterLock& mLock;
            };

        }
    }

#endif

// =============================================================================
// EXEMPLES D'UTILISATION (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Cache thread-safe avec lectures fréquentes
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkReaderWriterLock.h>
    #include <unordered_map>
    using namespace nkentseu::threading;

    class ThreadSafeCache {
    public:
        Optional<Value> Get(const Key& key) const
        {
            NkReadLock readLock(mRwLock);  // Multiple readers OK
            auto it = mCache.find(key);
            return (it != mCache.end()) ? Optional<Value>(it->second) : nullopt;
        }

        void Insert(const Key& key, const Value& value)
        {
            NkWriteLock writeLock(mRwLock);  // Exclusive access
            mCache[key] = value;
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
        mutable NkReaderWriterLock mRwLock;  // Protège l'accès à mCache
        mutable std::unordered_map<Key, Value> mCache;
    };

    // ---------------------------------------------------------------------
    // Exemple 2 : Configuration partagée avec mise à jour rare
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkReaderWriterLock.h>

    class SharedConfig {
    public:
        std::string GetSetting(const std::string& key) const
        {
            NkReadLock lock(mRwLock);  // Lectures concurrentes
            auto it = mSettings.find(key);
            return (it != mSettings.end()) ? it->second : "";
        }

        void UpdateSetting(const std::string& key, const std::string& value)
        {
            NkWriteLock lock(mRwLock);  // Écriture exclusive
            mSettings[key] = value;
            // Notification aux listeners si nécessaire
        }

        void BulkUpdate(const std::unordered_map<std::string, std::string>& updates)
        {
            NkWriteLock lock(mRwLock);  // Verrouillage unique pour tout le batch
            for (const auto& [key, value] : updates) {
                mSettings[key] = value;
            }
        }

    private:
        mutable NkReaderWriterLock mRwLock;
        std::unordered_map<std::string, std::string> mSettings;
    };

    // ---------------------------------------------------------------------
    // Exemple 3 : TryLock pour fallback non-bloquant
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkReaderWriterLock.h>

    bool TryReadWithFallback(NkReaderWriterLock& rwlock, std::function<void()> reader)
    {
        if (rwlock.TryLockRead()) {
            // Lecture acquise : exécuter le reader
            reader();
            rwlock.UnlockRead();
            return true;
        } else {
            // Fallback : lecture avec données potentiellement obsolètes
            ReadWithStaleData();
            return false;
        }
    }

    bool TryUpdateWithFallback(NkReaderWriterLock& rwlock, std::function<bool()> updater)
    {
        if (rwlock.TryLockWrite()) {
            // Écriture acquise : tenter la mise à jour
            bool success = updater();
            rwlock.UnlockWrite();
            return success;
        } else {
            // Fallback : reporter la mise à jour à plus tard
            QueueForLaterUpdate(updater);
            return false;
        }
    }

    // ---------------------------------------------------------------------
    // Exemple 4 : Pattern copy-on-write avec reader-writer lock
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkReaderWriterLock.h>
    #include <memory>

    template<typename Data>
    class CopyOnWriteContainer {
    public:
        // Lecture : accès partagé à la donnée courante
        std::shared_ptr<const Data> Read() const
        {
            NkReadLock lock(mRwLock);
            return mData;  // Copie du shared_ptr (atomique, léger)
        }

        // Écriture : création d'une nouvelle copie, puis swap atomique
        void Update(std::function<Data(const Data&)> transformer)
        {
            NkWriteLock lock(mRwLock);
            // Créer une nouvelle copie modifiée
            auto newData = std::make_shared<Data>(transformer(*mData));
            // Swap atomique : les nouveaux readers verront la nouvelle version
            mData = std::move(newData);
        }

    private:
        mutable NkReaderWriterLock mRwLock;
        std::shared_ptr<Data> mData;  // Pointeur vers la donnée immuable courante
    };

    // Usage :
    // CopyOnWriteContainer<std::vector<int>> container;
    // auto snapshot = container.Read();  // Lecture thread-safe
    // container.Update([](const auto& data) {
    //     auto copy = data;
    //     copy.push_back(42);  // Modification sur copie
    //     return copy;
    // });  // Mise à jour atomique

    // ---------------------------------------------------------------------
    // Exemple 5 : Prévention de starvation des writers
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkReaderWriterLock.h>

    // NkReaderWriterLock implémente une prévention de starvation :
    // - Quand un writer appelle LockWrite(), il incrémente mWritersWaiting
    // - Les nouveaux LockRead() bloquent si mWritersWaiting > 0
    // - Cela garantit que les writers ne sont pas indéfiniment repoussés
    //   par un flux continu de readers

    void StarvationPreventionDemo()
    {
        NkReaderWriterLock rwlock;
        std::atomic<bool> writerStarted{false};

        // Thread reader continu (simule un flux de lectures)
        NkThread continuousReader([&rwlock]() {
            while (true) {
                NkReadLock lock(rwlock);
                // Lecture rapide
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                // Avec prévention de starvation : quand un writer attend,
                // ce reader terminera et les nouveaux readers bloqueront,
                // permettant au writer d'acquérir le lock
            }
        });

        // Thread writer (arrive après un délai)
        NkThread writer([&rwlock, &writerStarted]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            writerStarted.store(true);

            NkWriteLock lock(rwlock);  // Va acquérir le lock après les readers actifs
            // Écriture exclusive
            PerformCriticalUpdate();
        });

        // Attendre un peu puis arrêter
        std::this_thread::sleep_for(std::chrono::seconds(1));
        // ... cleanup ...
    }

    // ---------------------------------------------------------------------
    // Exemple 6 : Bonnes pratiques et anti-patterns
    // ---------------------------------------------------------------------
    // ✅ BON : Utiliser NkReadLock pour les lectures, NkWriteLock pour les écritures
    void GoodLockChoice(NkReaderWriterLock& rwlock, const Data& data, bool isWrite)
    {
        if (isWrite) {
            NkWriteLock lock(rwlock);  // ✅ Exclusif pour modification
            ModifyData(data);
        } else {
            NkReadLock lock(rwlock);   // ✅ Partagé pour lecture
            ReadData(data);
        }
    }

    // ❌ MAUVAIS : Utiliser NkWriteLock pour une simple lecture (exclusion inutile)
    void BadLockChoice(NkReaderWriterLock& rwlock, const Data& data)
    {
        NkWriteLock lock(rwlock);  // ❌ Bloque tous les autres threads inutilement !
        ReadData(data);            // Lecture seule : NkReadLock suffirait
    }

    // ✅ BON : Minimiser la durée de détention des locks
    void EfficientLocking(NkReaderWriterLock& rwlock)
    {
        // Préparer les données hors section critique
        auto prepared = PrepareData();

        {
            NkWriteLock lock(rwlock);  // Scope minimal pour write lock
            ApplyUpdate(prepared);     // Opération rapide protégée
        }  // Libération immédiate

        ContinueWithUnprotectedWork();  // Hors section critique
    }

    // ❌ MAUVAIS : Garder un write lock pendant un traitement long
    void InefficientLocking(NkReaderWriterLock& rwlock)
    {
        NkWriteLock lock(rwlock);  // ❌ Lock acquis trop tôt
        auto data = ExpensiveComputation();  // Lock maintenu inutilement !
        ApplyUpdate(data);
    }

    // ✅ BON : Utiliser TryLock pour éviter les blocages dans les UI threads
    bool NonBlockingRead(NkReaderWriterLock& rwlock, std::function<void()> onAcquired)
    {
        if (rwlock.TryLockRead()) {
            onAcquired();
            rwlock.UnlockRead();
            return true;
        }
        return false;  // Fallback : afficher "chargement..." ou données en cache
    }

    // ❌ MAUVAIS : Appeler Unlock sans avoir acquis le lock (UB)
    void RiskyUnlock(NkReaderWriterLock& rwlock)
    {
        if (someCondition) {
            return;  // ❌ Retour sans LockRead/Write → Unlock suivant = UB !
        }
        rwlock.LockRead();
        DoWork();
        rwlock.UnlockRead();
    }

    // ---------------------------------------------------------------------
    // Exemple 7 : Intégration avec NkThreadPool pour lectures parallèles
    // ---------------------------------------------------------------------
    #include <NKThreading/Synchronization/NkReaderWriterLock.h>
    #include <NKThreading/NkThreadPool.h>

    void ParallelReads(NkReaderWriterLock& rwlock, NkThreadPool& pool,
                      const std::vector<Key>& keys, std::vector<Value>& results)
    {
        // Soumettre des tâches de lecture en parallèle
        pool.ParallelFor(keys.size(), [&](size_t i) {
            NkReadLock lock(rwlock);  // Multiple readers OK : parallélisme maximal
            results[i] = LookupInSharedData(keys[i]);
        }, 100);  // Chunks de 100 pour équilibrer overhead/charge

        pool.Join();  // Attendre que toutes les lectures soient terminées
    }

    // ---------------------------------------------------------------------
    // Exemple 8 : Testing unitaire de NkReaderWriterLock
    // ---------------------------------------------------------------------
    #include <gtest/gtest.h>
    #include <NKThreading/Synchronization/NkReaderWriterLock.h>
    #include <NKThreading/NkThread.h>
    #include <atomic>
    #include <vector>

    TEST(NkReaderWriterLockTest, MultipleConcurrentReaders)
    {
        nkentseu::threading::NkReaderWriterLock rwlock;
        std::atomic<nk_uint32> activeReaders{0};
        std::atomic<nk_uint32> maxConcurrent{0};

        constexpr nk_uint32 kNumReaders = 8;
        std::vector<NkThread> readers;

        for (nk_uint32 i = 0; i < kNumReaders; ++i) {
            readers.emplace_back([&rwlock, &activeReaders, &maxConcurrent]() {
                NkReadLock lock(rwlock);
                nk_uint32 current = activeReaders.fetch_add(1) + 1;
                maxConcurrent = std::max(maxConcurrent.load(), current);

                // Simuler une lecture
                std::this_thread::sleep_for(std::chrono::milliseconds(10));

                activeReaders.fetch_sub(1);
            });
        }

        for (auto& t : readers) {
            t.Join();
        }

        // Tous les readers ont pu s'exécuter concurrentiellement
        EXPECT_EQ(maxConcurrent.load(), kNumReaders);
    }

    TEST(NkReaderWriterLockTest, WriterExclusivity)
    {
        nkentseu::threading::NkReaderWriterLock rwlock;
        std::atomic<bool> writerActive{false};
        std::atomic<nk_uint32> readerDuringWrite{0};

        // Thread writer
        NkThread writer([&rwlock, &writerActive, &readerDuringWrite]() {
            NkWriteLock lock(rwlock);
            writerActive.store(true);

            // Pendant que le writer détient le lock, lancer un reader
            NkThread concurrentReader([&rwlock, &readerDuringWrite]() {
                if (rwlock.TryLockRead()) {
                    readerDuringWrite.fetch_add(1);  // ❌ Ne devrait pas arriver
                    rwlock.UnlockRead();
                }
            });
            concurrentReader.Join();

            writerActive.store(false);
        });

        writer.Join();

        // Aucun reader n'a pu acquérir le lock pendant l'écriture
        EXPECT_EQ(readerDuringWrite.load(), 0u);
    }

    TEST(NkReaderWriterLockTest, WriterStarvationPrevention)
    {
        nkentseu::threading::NkReaderWriterLock rwlock;
        std::atomic<bool> writerAcquired{false};

        // Thread reader continu
        NkThread continuousReader([&rwlock]() {
            for (int i = 0; i < 100; ++i) {
                NkReadLock lock(rwlock);
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });

        // Thread writer qui arrive après un délai
        NkThread writer([&rwlock, &writerAcquired]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            NkWriteLock lock(rwlock);  // Doit acquérir malgré le flux de readers
            writerAcquired.store(true);
        });

        writer.Join();
        continuousReader.Join();

        // Le writer a pu acquérir le lock (pas de starvation)
        EXPECT_TRUE(writerAcquired.load());
    }

    // ---------------------------------------------------------------------
    // Exemple 9 : Configuration CMake pour NkReaderWriterLock
    // ---------------------------------------------------------------------
    # CMakeLists.txt - Intégration de NkReaderWriterLock

    # NkReaderWriterLock dépend de NkMutex et NkConditionVariable
    target_link_libraries(your_target PRIVATE
        NKThreading::NKThreading  # Inclut toutes les dépendances transitives
    )

    # Option pour activer les assertions de debug
    option(NKRWLOCK_ENABLE_ASSERTS "Enable NKENTSEU_ASSERT in NkReaderWriterLock" ON)
    if(NKRWLOCK_ENABLE_ASSERTS)
        target_compile_definitions(your_target PRIVATE NKENTSEU_ENABLE_ASSERTS)
    endif()

    // ---------------------------------------------------------------------
    // Exemple 10 : Debugging et instrumentation
    // ---------------------------------------------------------------------
    #ifdef NK_DEBUG
    #include <NKThreading/Synchronization/NkReaderWriterLock.h>
    #include <NKCore/NkLogger.h>

    // Wrapper instrumenté pour logging des acquisitions de verrous
    class DebugReaderWriterLock : public NkReaderWriterLock {
    public:
        explicit DebugReaderWriterLock(const nk_char* name = nullptr)
            : mName(name ? name : "Unnamed") {}

        void LockRead() noexcept
        {
            auto tid = NkThread::GetCurrentThreadId();
            NK_LOG_DEBUG("RWLock[{}]: Thread {} acquiring READ lock", mName, tid);
            NkReaderWriterLock::LockRead();
            NK_LOG_DEBUG("RWLock[{}]: Thread {} acquired READ lock", mName, tid);
        }

        void LockWrite() noexcept
        {
            auto tid = NkThread::GetCurrentThreadId();
            NK_LOG_DEBUG("RWLock[{}]: Thread {} acquiring WRITE lock", mName, tid);
            NkReaderWriterLock::LockWrite();
            NK_LOG_DEBUG("RWLock[{}]: Thread {} acquired WRITE lock", mName, tid);
        }

    private:
        const nk_char* mName;
    };

    // Guards instrumentés
    class DebugReadLock : public NkReadLock {
    public:
        explicit DebugReadLock(DebugReaderWriterLock& lock) noexcept
            : NkReadLock(lock) {}
        // Logging géré par le lock lui-même
    };

    class DebugWriteLock : public NkWriteLock {
    public:
        explicit DebugWriteLock(DebugReaderWriterLock& lock) noexcept
            : NkWriteLock(lock) {}
    };

    // Usage en mode debug :
    // DebugReaderWriterLock rwlock("ConfigLock");
    // {
    //     DebugReadLock readLock(rwlock);
    //     ReadConfig();
    // }
    #endif

    // ---------------------------------------------------------------------
    // Exemple 11 : Comparaison avec NkSharedMutex (alias)
    // ---------------------------------------------------------------------
    /\*
    NkReaderWriterLock et NkSharedMutex sont fonctionnellement équivalents :
    - NkSharedMutex est un alias vers NkReaderWriterLock pour compatibilité std::shared_mutex
    - NkReadLock  est un alias vers NkReadLock (identique)
    - NkWriteLock est un alias vers NkWriteLock (identique)

    Quand utiliser quel nom :
    • NkReaderWriterLock : quand on veut être explicite sur le comportement reader/writer
    • NkSharedMutex      : quand on migre depuis std::shared_mutex ou on veut une API familière

    Exemple de migration :
    // AVANT (STL) :
    std::shared_mutex mutex;
    std::shared_lock<std::shared_mutex> readLock(mutex);
    std::unique_lock<std::shared_mutex> writeLock(mutex);

    // APRÈS (NKThreading) :
    nkentseu::threading::NkSharedMutex mutex;  // ou NkReaderWriterLock
    nkentseu::threading::NkSharedLock readLock(mutex);   // ou NkReadLock
    nkentseu::threading::NkUniqueLock writeLock(mutex);  // ou NkWriteLock

    Les deux styles fonctionnent grâce aux alias dans NkSharedMutex.h.
    *\/

    // ---------------------------------------------------------------------
    // Exemple 12 : Bonnes pratiques de conception pour les structures partagées
    // ---------------------------------------------------------------------
    /\**
     * Checklist pour utiliser NkReaderWriterLock efficacement :
     *
     * ✅ Préférer les lectures aux écritures quand possible
     *    - Structurer les données pour maximiser les accès en lecture seule
     *    - Utiliser copy-on-write pour éviter les locks longs en écriture
     *
     * ✅ Minimiser la durée de détention des write locks
     *    - Préparer les données hors section critique
     *    - Appliquer les modifications en une opération atomique rapide
     *
     * ✅ Utiliser TryLock pour les chemins non-critiques
     *    - Éviter les blocages dans les UI threads ou les boucles temps-réel
     *    - Fallback gracieux si le lock n'est pas disponible immédiatement
     *
     * ✅ Documenter clairement qui peut lire/écrire
     *    - Annotations : @thread_safe_read, @requires_write_lock
     *    - Assertions en debug : vérifier que le bon type de lock est détenu
     *
     * ❌ Éviter les conversions read→write sur le même lock
     *    - Risque de deadlock si implémenté naïvement
     *    - Solution : relâcher le read lock, acquérir le write lock, re-vérifier
     *
     * ❌ Ne pas garder un lock pendant des I/O ou opérations bloquantes
     *    - Bloque inutilement tous les autres threads
     *    - Solution : copier les données nécessaires, relâcher le lock, puis faire l'I/O
     *\/
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================