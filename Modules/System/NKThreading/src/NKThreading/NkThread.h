//
// NkThread.h
// =============================================================================
// Description :
//   Wrapper C++ multiplateforme autour des threads natifs du système
//   d'exploitation. Fournit une API unifiée pour la création et la gestion
//   de threads avec support des fonctionnalités avancées (nom, affinité, priorité).
//
// Caractéristiques :
//   - Abstraction native : CreateThread/Win32 (Windows), pthread (POSIX)
//   - Sémantique de déplacement pour transfert de propriété de thread
//   - Gestion RAII : Detach() automatique au destructeur si thread joinable
//   - Fonctionnalités avancées : SetName, SetAffinity, SetPriority
//   - Garantie noexcept sur toutes les opérations publiques
//
// Algorithmes implémentés :
//   - ThreadEntry : point d'entrée statique avec transfert de contexte via heap
//   - MoveFrom : transfert atomique des handles natifs entre instances
//   - GetID/GetCurrentThreadId : extraction portable d'identifiant thread
//   - SetName : conversion UTF-8 → plateforme native (WideChar/pthread_setname_np)
//
// Types disponibles :
//   NkThread : wrapper de thread natif avec API C++ moderne
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_THREADING_THREAD_H__
#define __NKENTSEU_THREADING_THREAD_H__

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKCore/NkTraits.h"
    #include "NKCore/NkTypes.h"
    #include "NKPlatform/NkPlatformDetect.h"
    #include "NKContainers/Functional/NkFunction.h"
    #include "NKThreading/NkThreadingApi.h"

    // Inclusions système conditionnelles pour les primitives de threading natif
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        #ifndef WIN32_LEAN_AND_MEAN
            #define WIN32_LEAN_AND_MEAN
        #endif
        #include <windows.h>
    #else
        #include <stdint.h>
        #include <pthread.h>
        #include <sched.h>
        #include <string.h>
        #if defined(NKENTSEU_PLATFORM_LINUX)
            #include <sys/syscall.h>
            #include <sys/types.h>
        #endif
        #include <unistd.h>
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
            // CLASSE : NkThread
            // =====================================================================
            // Wrapper C++ pour thread natif du système d'exploitation.
            //
            // Représentation interne :
            //   - Windows : HANDLE + DWORD (thread ID) via CreateThread
            //   - POSIX   : pthread_t + flag joinable via pthread_create
            //
            // Invariant de production :
            //   - Un thread est soit joinable, soit détaché, soit non-initialisé
            //   - Après Join() ou Detach(), le thread n'est plus joinable
            //   - Le destructeur appelle Detach() si le thread est encore joinable
            //
            // Sémantique de propriété :
            //   - Non-copiable : un thread OS ne peut être dupliqué
            //   - Déplaçable : transfert de propriété via move semantics
            //   - RAII : nettoyage automatique des ressources au destructeur
            //
            // Fonctionnalités avancées :
            //   - SetName : nommage du thread pour debugging/profiling
            //   - SetAffinity : pinning sur un core CPU spécifique
            //   - SetPriority : ajustement de la priorité de scheduling
            //   - GetCurrentCore : requête du core d'exécution courant
            // =================================================================
            class NKENTSEU_THREADING_CLASS_EXPORT NkThread {
            public:

                // -------------------------------------------------------------
                // TYPE ALIAS ET MÉTA-INFORMATIONS
                // -------------------------------------------------------------

                /// @brief Type de fonction exécutable par le thread.
                /// @note Basé sur NkFunction<void()> pour flexibilité maximale.
                using ThreadFunc = NkFunction<void(void* userData)>;

                /// @brief Type d'identifiant unique de thread.
                /// @note nk_uint64 pour compatibilité multiplateforme.
                using ThreadId = nk_uint64;

                // -------------------------------------------------------------
                // CONSTRUCTEURS ET DESTRUCTEUR
                // -------------------------------------------------------------

                /// @brief Constructeur par défaut : thread non-initialisé.
                /// @note Garantie noexcept : aucune allocation ni exception.
                /// @note Le thread n'est pas joinable : Joinable() retourne false.
                NkThread() noexcept;

                /// @brief Constructeur avec fonction : crée et démarre le thread.
                /// @tparam Func Type callable compatible avec void().
                /// @param func Fonction à exécuter dans le nouveau thread.
                /// @note Garantie noexcept : les échecs de création sont gérés en interne.
                /// @note Le thread démarre immédiatement après construction.
                template<typename Func,
                typename = traits::NkEnableIf_t<!traits::NkIsSame_v<traits::NkDecay_t<Func>, NkThread>>>
                explicit NkThread(Func&& func) noexcept : NkThread() {
                    Start(ThreadFunc(traits::NkForward<Func>(func)));
                }


                /// @brief Destructeur : détache automatiquement si thread joinable.
                /// @note Garantie noexcept : nettoyage toujours sûr.
                /// @note Évite les fuites de ressources thread en cas d'oubli de Join/Detach.
                /// @warning Ne pas détruire un NkThread pendant que le thread OS est encore en exécution
                ///          si la fonction du thread accède à des données locales du destructeur.
                ~NkThread();

                // -------------------------------------------------------------
                // SÉMANTIQUE DE DÉPLACEMENT (MOVE-ONLY)
                // -------------------------------------------------------------

                /// @brief Constructeur de déplacement : transfère la propriété du thread.
                /// @param other Instance source dont la propriété est transférée.
                /// @note L'instance source devient non-initialisée après le déplacement.
                /// @note Le thread OS continue son exécution : seul le handle C++ est transféré.
                NkThread(NkThread&& other) noexcept {
                    MoveFrom(other);
                }

                /// @brief Opérateur d'affectation par déplacement.
                /// @param other Instance source dont la propriété est transférée.
                /// @return Référence vers cette instance après transfert.
                /// @note Détache le thread courant si joinable avant le transfert.
                NkThread& operator=(NkThread&& other) noexcept{
                    if (this != &other) {
                        if (Joinable()) {
                            Detach();
                        }
                        MoveFrom(other);
                    }
                    return *this;
                }

                // -------------------------------------------------------------
                // RÈGLES DE COPIE (INTERDITES)
                // -------------------------------------------------------------

                /// @brief Constructeur de copie supprimé : thread non-copiable.
                /// @note Un thread OS représente une ressource système unique.
                NkThread(const NkThread&) = delete;

                /// @brief Opérateur d'affectation supprimé : thread non-affectable.
                NkThread& operator=(const NkThread&) = delete;

                // -------------------------------------------------------------
                // API DE CONTRÔLE DU THREAD (Join/Detach)
                // -------------------------------------------------------------

                /// @brief Attend la fin d'exécution du thread (bloquant).
                /// @note Bloque le thread appelant jusqu'à ce que le thread cible termine.
                /// @note Après Join(), le thread n'est plus joinable : Joinable() retourne false.
                /// @note Libère les ressources système associées au thread (handle pthread/Win32).
                /// @warning Ne pas appeler Join() sur un thread déjà détaché ou non-initialisé.
                /// @warning Ne pas appeler Join() depuis le thread lui-même = deadlock.
                void Join() noexcept;

                /// @brief Détache le thread : exécution indépendante jusqu'à completion.
                /// @note Le thread continue son exécution même après destruction du NkThread.
                /// @note Les ressources système sont libérées automatiquement à la fin du thread.
                /// @note Après Detach(), le thread n'est plus joinable : Join() n'est plus possible.
                /// @warning Ne pas accéder aux données locales du thread créateur après Detach().
                void Detach() noexcept;

                /// @brief Vérifie si le thread est joinable (peut être joint).
                /// @return true si le thread a été créé et ni Join() ni Detach() n'a été appelé.
                /// @note Thread-safe : peut être appelé depuis n'importe quel thread.
                /// @note Non-bloquant : retourne immédiatement dans tous les cas.
                [[nodiscard]] nk_bool Joinable() const noexcept;

                // -------------------------------------------------------------
                // API D'IDENTIFICATION (Thread ID)
                // -------------------------------------------------------------

                /// @brief Récupère l'identifiant unique du thread.
                /// @return ThreadId opaque représentant le thread OS sous-jacent.
                /// @note Retourne 0 si le thread n'est pas joinable (non-initialisé/détaché).
                /// @note L'identifiant est stable pendant toute la vie du thread OS.
                /// @note Peut être comparé avec GetCurrentThreadId() pour l'auto-détection.
                [[nodiscard]] ThreadId GetID() const noexcept;

                /// @brief Récupère l'identifiant du thread courant.
                /// @return ThreadId du thread qui appelle cette méthode.
                /// @note Fonction statique : peut être appelée sans instance NkThread.
                /// @note Utile pour le logging, le debugging, et l'auto-détection.
                [[nodiscard]] static ThreadId GetCurrentThreadId() noexcept;

                // -------------------------------------------------------------
                // API DE CONFIGURATION AVANCÉE
                // -------------------------------------------------------------

                /// @brief Définit un nom lisible pour le thread (debugging/profiling).
                /// @param name Chaîne UTF-8 représentant le nom du thread.
                /// @note Windows : utilise SetThreadDescription() (Kernel32.dll, Windows 10+).
                /// @note Linux  : utilise pthread_setname_np() (limite 16 caractères).
                /// @note macOS : utilise pthread_setname_np() uniquement sur le thread courant.
                /// @note Ignoré silencieusement si name est null/vide ou plateforme non-supportée.
                /// @warning Sur Linux, le nom est tronqué à 15 caractères + null terminator.
                void SetName(const nk_char* name) noexcept;

                /// @brief Définit l'affinité CPU : pinning sur un core spécifique.
                /// @param cpuCore Index du core CPU (0 = premier core, 1 = deuxième, etc.).
                /// @note Windows : utilise SetThreadAffinityMask() avec bitmask.
                /// @note Linux  : utilise pthread_setaffinity_np() avec cpu_set_t.
                /// @note Ignoré silencieusement si cpuCore est hors limites ou plateforme non-supportée.
                /// @warning Le pinning peut dégrader les performances si mal utilisé (load imbalance).
                /// @warning Sur les systèmes avec CPU hotplug, le core spécifié peut devenir indisponible.
                void SetAffinity(nk_uint32 cpuCore) noexcept;

                /// @brief Définit la priorité de scheduling du thread.
                /// @param priority Valeur dans [-2, +2] : -2=lowest, 0=normal, +2=highest.
                /// @note Windows : mappe vers THREAD_PRIORITY_* constants natifs.
                /// @note POSIX  : mappe vers [minPriority, maxPriority] via interpolation linéaire.
                /// @note Ignoré silencieusement si le thread n'est pas joinable ou plateforme non-supportée.
                /// @warning Les priorités élevées peuvent affamer d'autres threads : utiliser avec précaution.
                /// @warning Sur Linux, peut nécessiter des privilèges CAP_SYS_NICE pour les priorités temps-réel.
                void SetPriority(nk_int32 priority) noexcept;

                /// @brief Récupère l'index du core CPU sur lequel le thread courant s'exécute.
                /// @return Index du core (0-based), ou 0 en cas d'erreur/plattform non-supportée.
                /// @note Fonction membre const : peut être appelée sur n'importe quelle instance.
                /// @note Retourne le core du thread appelant, pas nécessairement celui de *this.
                /// @note Utile pour le profiling, le debugging, et l'optimisation d'affinité.
                [[nodiscard]] nk_uint32 GetCurrentCore() const noexcept;

            private:

                // -------------------------------------------------------------
                // STRUCTURE INTERNE : ThreadStartData
                // -------------------------------------------------------------
                // Données de contexte transférées au nouveau thread via heap.
                // Allouée dans Start(), libérée dans ThreadEntry() après transfert.
                // =================================================================
                struct ThreadStartData {
                    /// @brief Constructeur : déplace la fonction dans le contexte.
                    /// @param function Fonction à exécuter dans le nouveau thread.
                    explicit ThreadStartData(ThreadFunc&& function) noexcept;

                    ThreadFunc Function;  ///< Fonction à exécuter (transférée par move)
                };

                // -------------------------------------------------------------
                // MÉTHODES PRIVÉES (implémentation interne)
                // -------------------------------------------------------------
                
            public:
                /// @brief Démarre l'exécution du thread avec la fonction spécifiée.
                /// @param function Fonction à exécuter dans le nouveau thread.
                /// @note Détache le thread courant si déjà joinable avant de créer le nouveau.
                /// @note Alloue ThreadStartData sur le heap pour transfert inter-thread.
                /// @note En cas d'échec de création, libère startData et marque non-joinable.
                void Start(ThreadFunc&& function) noexcept;

            private:
                /// @brief Transfère les membres natifs depuis une autre instance.
                /// @param other Instance source dont les handles sont transférés.
                /// @note Met à jour les membres natifs et invalide l'instance source.
                /// @note Utilisé par le constructeur de déplacement et operator=.
                void MoveFrom(NkThread& other) noexcept;

                /// @brief Point d'entrée statique pour le thread OS.
                /// @param userData Pointeur vers ThreadStartData allouée sur le heap.
                /// @note Extrait la fonction, libère startData, exécute la fonction.
                /// @note Retourne 0/nullptr selon la plateforme (convention OS).
                #if defined(NKENTSEU_PLATFORM_WINDOWS)
                    static DWORD WINAPI ThreadEntry(LPVOID userData);
                #else
                    static void* ThreadEntry(void* userData);
                #endif

                // -------------------------------------------------------------
                // MEMBRES PRIVÉS (données internes)
                // -------------------------------------------------------------

                #if defined(NKENTSEU_PLATFORM_WINDOWS)
                    HANDLE mHandle;      ///< Handle Win32 du thread (nullptr si non-initialisé)
                    DWORD mThreadId;     ///< Identifiant numérique du thread (0 si non-initialisé)
                #else
                    pthread_t mThread;   ///< Handle pthread du thread
                    nk_bool mJoinable;   ///< Flag indiquant si le thread peut être joint
                #endif
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
        // Constructeur template : création et démarrage du thread
        // ---------------------------------------------------------------------
        // Déduit le type de fonction via forwarding, l'enveloppe dans ThreadFunc,
        // puis délègue au constructeur par défaut + Start().
        //
        // Garantie noexcept :
        //   - NkFunction construction peut échouer silencieusement (état null)
        //   - Start() gère les échecs de création de thread en interne
        //   - Aucune exception ne peut être propagée vers l'appelant
        // ---------------------------------------------------------------------

    }
}

// =============================================================================
// EXEMPLES D'UTILISATION (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Création basique de thread avec lambda
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThread.h>
    using namespace nkentseu::threading;

    void SimpleThreadExample()
    {
        // Création et démarrage immédiat d'un thread
        NkThread worker([]() {
            for (int i = 0; i < 10; ++i) {
                DoWork(i);
            }
        });

        // Attendre la fin du thread avant de continuer
        worker.Join();

        // Le thread est terminé : ressources libérées
    }

    // ---------------------------------------------------------------------
    // Exemple 2 : Transfert de données via capture de lambda
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThread.h>

    void DataProcessingExample()
    {
        std::vector<int> data = LoadData();
        int result = 0;

        // Capture par référence : attention à la durée de vie !
        NkThread processor([&data, &result]() {
            result = ComputeSum(data);  // data et result doivent rester valides
        });

        processor.Join();  // Attendre avant d'utiliser result
        LogResult(result);
    }

    // ---------------------------------------------------------------------
    // Exemple 3 : Transfert de propriété avec move semantics
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThread.h>

    NkThread CreateBackgroundTask()
    {
        // Retourner un thread par valeur : move semantics
        return NkThread([]() {
            RunBackgroundService();
        });
    }

    void ManagerFunction()
    {
        // Le thread est déplacé dans 'task'
        NkThread task = CreateBackgroundTask();

        // ... faire autre chose ...

        task.Join();  // Attendre la completion
    }

    // ---------------------------------------------------------------------
    // Exemple 4 : Nommage des threads pour debugging/profiling
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThread.h>

    void NamedThreadExample()
    {
        NkThread worker([]() {
            ProcessItems();
        });

        // Nommer le thread pour identification dans les outils de profiling
        worker.SetName("ItemProcessor-Worker");

        // Sur Windows : visible dans Visual Studio Debugger, PerfView
        // Sur Linux  : visible dans 'top -H', 'htop', /proc/[pid]/task/[tid]/comm
        // Sur macOS : visible dans Instruments, Activity Monitor

        worker.Join();
    }

    // ---------------------------------------------------------------------
    // Exemple 5 : Affinité CPU pour optimisation cache/performance
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThread.h>

    void PinnedThreadExample()
    {
        // Créer un thread et le pinner sur le core 2
        NkThread compute([]() {
            RunComputeIntensiveTask();
        });

        // Pinning sur core spécifique : réduit les migrations de cache
        compute.SetAffinity(2);

        // Attention : ne pas pinner trop de threads sur le même core !
        // Utiliser pour :
        //   - Threads temps-réel avec contraintes de latence
        //   - Workloads compute-intensive avec localité de données
        //   - Isolation de tâches critiques du reste du système

        compute.Join();
    }

    // ---------------------------------------------------------------------
    // Exemple 6 : Priorité de scheduling pour tâches critiques
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThread.h>

    void PriorityThreadExample()
    {
        // Thread avec priorité élevée pour traitement temps-réel
        NkThread realtime([]() {
            while (running) {
                ProcessRealtimeData();
            }
        });

        // Priorité +2 = highest : exécuté en priorité par le scheduler
        realtime.SetPriority(2);

        // ⚠️ Attention : priorités élevées peuvent affamer d'autres threads
        // ⚠️ Sur Linux : peut nécessiter CAP_SYS_NICE ou exécution root
        // ⚠️ Toujours tester l'impact sur le système global

        // Pour arrêter proprement :
        running = false;
        realtime.Join();
    }

    // ---------------------------------------------------------------------
    // Exemple 7 : Auto-détection et logging avec GetCurrentThreadId
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThread.h>
    #include <NKCore/NkLogger.h>

    void LoggingThreadExample()
    {
        NkThread logger([]() {
            // Récupérer l'ID du thread courant pour le logging
            auto tid = NkThread::GetCurrentThreadId();
            NK_LOG_INFO("Logger thread started [ID={}]", tid);

            while (logging) {
                FlushLogBuffer();
                // ...
            }

            NK_LOG_INFO("Logger thread stopped [ID={}]", tid);
        });

        logger.SetName("LogFlusher");
        logger.Join();
    }

    // ---------------------------------------------------------------------
    // Exemple 8 : Pattern worker pool avec multiple threads
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThread.h>
    #include <vector>

    class SimpleWorkerPool {
    public:
        explicit SimpleWorkerPool(nk_uint32 threadCount)
        {
            for (nk_uint32 i = 0; i < threadCount; ++i) {
                mWorkers.emplace_back([this, i]() {
                    WorkerLoop(i);
                });
                mWorkers.back().SetName(NkFormat("Worker-{}", i).CStr());
            }
        }

        ~SimpleWorkerPool()
        {
            mShutdown = true;
            for (auto& worker : mWorkers) {
                if (worker.Joinable()) {
                    worker.Join();
                }
            }
        }

    private:
        void WorkerLoop(nk_uint32 workerId)
        {
            while (!mShutdown) {
                if (auto task = TryDequeueTask()) {
                    task->Execute();
                } else {
                    // Pas de tâche : attendre ou yield
                    NkThread::Yield();  // Hypothétique
                }
            }
        }

        std::vector<NkThread> mWorkers;
        volatile nk_bool mShutdown = false;
    };

    // ---------------------------------------------------------------------
    // Exemple 9 : Gestion d'erreur et fallback en cas d'échec de création
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThread.h>
    #include <NKCore/NkLogger.h>

    bool TrySpawnWorker(std::function<void()> task, NkThread& outThread)
    {
        // Construction noexcept : les échecs sont gérés en interne
        outThread = NkThread(std::move(task));

        // Vérifier si le thread a été créé avec succès
        if (!outThread.Joinable()) {
            NK_LOG_ERROR("Failed to create worker thread");
            return false;
        }

        // Optionnel : configurer le thread après création
        outThread.SetPriority(1);  // Above normal
        outThread.SetName("FallbackWorker");

        return true;
    }

    // ---------------------------------------------------------------------
    // Exemple 10 : Bonnes pratiques et anti-patterns
    // ---------------------------------------------------------------------
    // ✅ BON : Toujours Join() ou Detach() avant destruction
    void SafeThreadUsage()
    {
        NkThread t([]() { DoWork(); });
        t.Join();  // ✅ Attendre explicitement
    }  // Destructeur : thread déjà joint, rien à faire

    // ❌ MAUVAIS : Laisser le destructeur Detach() implicitement
    void RiskyThreadUsage()
    {
        NkThread t([]() {
            // Accès à des données locales du scope parent = UB !
            ExternalData* data = GetLocalData();  // ❌ data peut être détruite !
            data->Process();
        });
        // Destructeur appelle Detach() : thread continue avec données potentiellement invalides
    }

    // ✅ BON : Capturer par valeur ou utiliser des données thread-local
    void SafeCaptureExample()
    {
        int localValue = 42;
        NkThread t([value = localValue]() {  // Capture par copie
            ProcessValue(value);  // ✅ value est copié, safe
        });
        t.Join();
    }

    // ✅ BON : Utiliser Joinable() pour vérifier avant Join/Detach
    void ConditionalJoin(NkThread& t)
    {
        if (t.Joinable()) {
            t.Join();  // ✅ Safe : thread valide et joinable
        }
        // Sinon : thread déjà joint/détaché ou non-initialisé
    }

    // ❌ MAUVAIS : Appeler Join() depuis le thread lui-même (deadlock)
    void DeadlockExample()
    {
        NkThread t([](NkThread* self) {  // Hypothétique : passage de self
            self->Join();  // ❌ Deadlock : thread s'attend lui-même !
        });
        // Ne jamais faire ça !
    }

    // ✅ BON : Pattern RAII pour gestion automatique des threads
    class ScopedThread {
        NkThread mThread;
    public:
        template<typename Func>
        explicit ScopedThread(Func&& f) : mThread(std::forward<Func>(f)) {}
        ~ScopedThread() { if (mThread.Joinable()) mThread.Join(); }
        // Non-copiable, non-déplaçable pour simplifier
        ScopedThread(const ScopedThread&) = delete;
        ScopedThread& operator=(const ScopedThread&) = delete;
    };

    // Usage :
    void AutoJoinExample()
    {
        ScopedThread t([]() { DoWork(); });
        // Join() appelé automatiquement à la sortie de portée
    }

    // ---------------------------------------------------------------------
    // Exemple 11 : Intégration avec système de logging thread-safe
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThread.h>
    #include <NKCore/NkLogger.h>

    class ThreadSafeLogger {
    public:
        void LogAsync(const nk_char* message)
        {
            // Créer un thread éphémère pour logging asynchrone
            NkThread logger([msg = NkString(message)]() mutable {
                NK_LOG_INFO("[Async] {}", msg.CStr());
            });
            logger.SetName("AsyncLogger");
            logger.Detach();  // Logging en fire-and-forget
        }
    };

    // ---------------------------------------------------------------------
    // Exemple 12 : Configuration CMake pour threading natif
    // ---------------------------------------------------------------------
    # CMakeLists.txt - Configuration pour NkThread

    # Liaison avec les bibliothèques système requises pour pthread
    if(UNIX AND NOT APPLE)
        target_link_libraries(your_target PRIVATE pthread)
    endif()

    # Options de compilation pour optimisation threading
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(your_target PRIVATE
            -pthread              # Requis pour pthread sur GCC/Clang
            -ftls-model=global-dynamic  # Optimisation thread-local storage
        )
    endif()

    # Définir les macros de plateforme si non détectées automatiquement
    if(WIN32)
        target_compile_definitions(your_target PRIVATE
            NKENTSEU_PLATFORM_WINDOWS
            WIN32_LEAN_AND_MEAN
        )
    elseif(UNIX)
        target_compile_definitions(your_target PRIVATE
            NKENTSEU_PLATFORM_POSIX
        )
        if(LINUX)
            target_compile_definitions(your_target PRIVATE
                NKENTSEU_PLATFORM_LINUX
            )
        elseif(APPLE)
            target_compile_definitions(your_target PRIVATE
                NKENTSEU_PLATFORM_MACOS
            )
        endif()
    endif()

    // ---------------------------------------------------------------------
    // Exemple 13 : Testing unitaire avec NkThread
    // ---------------------------------------------------------------------
    #include <gtest/gtest.h>
    #include <NKThreading/NkThread.h>
    #include <atomic>

    TEST(NkThreadTest, BasicCreationAndJoin)
    {
        std::atomic<bool> executed{false};

        {
            nkentseu::threading::NkThread t([&executed]() {
                executed.store(true);
            });

            EXPECT_TRUE(t.Joinable());
            t.Join();
            EXPECT_FALSE(t.Joinable());
        }

        EXPECT_TRUE(executed.load());
    }

    TEST(NkThreadTest, MoveSemantics)
    {
        std::atomic<int> counter{0};

        nkentseu::threading::NkThread t1([&counter]() {
            counter.fetch_add(1);
        });

        // Déplacement vers t2
        nkentseu::threading::NkThread t2(std::move(t1));

        EXPECT_FALSE(t1.Joinable());  // t1 vidé après move
        EXPECT_TRUE(t2.Joinable());   // t2 détient le thread

        t2.Join();
        EXPECT_EQ(counter.load(), 1);
    }

    TEST(NkThreadTest, ThreadIdUniqueness)
    {
        using ThreadId = nkentseu::threading::NkThread::ThreadId;

        ThreadId mainId = nkentseu::threading::NkThread::GetCurrentThreadId();
        ThreadId childId = 0;

        nkentseu::threading::NkThread t([&childId]() {
            childId = nkentseu::threading::NkThread::GetCurrentThreadId();
        });

        t.Join();

        // Les IDs doivent être différents (thread principal vs worker)
        EXPECT_NE(mainId, 0u);
        EXPECT_NE(childId, 0u);
        EXPECT_NE(mainId, childId);
    }

    TEST(NkThreadTest, DetachPreventsJoin)
    {
        std::atomic<bool> executed{false};

        {
            nkentseu::threading::NkThread t([&executed]() {
                executed.store(true);
            });

            EXPECT_TRUE(t.Joinable());
            t.Detach();
            EXPECT_FALSE(t.Joinable());  // Plus joinable après detach
        }  // Destructeur : rien à faire car déjà détaché

        // Attendre un peu pour laisser le thread détaché terminer
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        EXPECT_TRUE(executed.load());
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================