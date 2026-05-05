//
// NkThreadLocal.h
// =============================================================================
// Description :
//   Wrapper générique pour valeurs thread-local avec valeur initiale optionnelle.
//   Fournit un accès thread-safe à des données sans synchronisation explicite.
//
// Caractéristiques :
//   - Stockage thread_local natif : zero overhead de synchronisation
//   - Valeur initiale optionnelle : configurable par instance
//   - Sémantique de pointeur : opérateurs -> et * pour accès intuitif
//   - Support move semantics pour types non-copiables
//   - Garantie noexcept sur toutes les opérations publiques
//
// Algorithmes implémentés :
//   - Get() : retourne la référence thread-local avec initialisation lazy
//   - Set() : assignation avec support copy/move pour flexibilité
//   - Opérateurs : délégation vers Get() pour syntaxe pointer-like
//
// Types disponibles :
//   NkThreadLocal<T> : wrapper thread-local générique pour type T
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_THREADING_THREADLOCAL_H__
#define __NKENTSEU_THREADING_THREADLOCAL_H__

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKCore/NkTypes.h"
    #include "NKCore/NkTraits.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : NAMESPACE PRINCIPAL
    // -------------------------------------------------------------------------
    namespace nkentseu {

        // =====================================================================
        // SOUS-NAMESPACE : threading
        // =====================================================================
        namespace threading {

            // =================================================================
            // CLASSE TEMPLATE : NkThreadLocal<T>
            // =================================================================
            // Wrapper pour valeur stockée en mémoire thread-local.
            //
            // Représentation interne :
            //   - thread_local T : stockage natif par thread (C++11)
            //   - mInitialValue + mHasInitialValue : configuration d'initialisation
            //   - Initialisation lazy : la valeur n'est créée qu'au premier Get()
            //
            // Invariant de production :
            //   - Chaque thread voit sa propre copie indépendante de la valeur
            //   - Aucune synchronisation requise : accès lock-free par design
            //   - La valeur initiale est copiée/déplacée une fois par thread au premier accès
            //
            // Avantages vs autres approches :
            //   - vs pthread_key_t : syntaxe C++ moderne, RAII, type-safe
            //   - vs std::map<thread_id, T> : zero lookup overhead, cache-friendly
            //   - vs mutex-protected global : lock-free, pas de contention
            //
            // Cas d'usage typiques :
            //   - Buffers temporaires réutilisables par thread (évite realloc)
            //   - Compteurs de statistiques par thread (agrégation finale)
            //   - Contexte d'exécution thread-specific (logging, tracing)
            //   - Cache de résultats intermédiaires (mémoization thread-local)
            //
            // Limitations :
            //   - La valeur n'est accessible que depuis le thread qui l'a créée
            //   - Pas de mécanisme de cleanup automatique à la fin du thread
            //     (géré par le runtime C++ via destructeur thread_local)
            //   - L'ordre de destruction des thread_locals est non-spécifié
            // =================================================================
            template<typename T>
            class NkThreadLocal {
                public:

                    // -------------------------------------------------------------
                    // CONSTRUCTEURS
                    // -------------------------------------------------------------

                    /// @brief Constructeur par défaut : pas de valeur initiale.
                    /// @note La valeur sera default-constructée au premier Get() dans chaque thread.
                    /// @note T doit être default-constructible si aucune valeur initiale n'est fournie.
                    NkThreadLocal() = default;

                    /// @brief Constructeur avec valeur initiale (copie).
                    /// @param initialValue Valeur à utiliser pour l'initialisation thread-local.
                    /// @note La valeur est copiée dans mInitialValue pour réutilisation par thread.
                    /// @note Chaque thread recevra sa propre copie de initialValue au premier accès.
                    /// @tparam T doit être copy-constructible.
                    explicit NkThreadLocal(const T& initialValue);

                    /// @brief Constructeur avec valeur initiale (déplacement).
                    /// @param initialValue Valeur à déplacer pour l'initialisation thread-local.
                    /// @note Plus efficace que la copie pour les types coûteux ou move-only.
                    /// @note Le paramètre est déplacé dans mInitialValue via traits::NkMove.
                    /// @tparam T doit être move-constructible.
                    explicit NkThreadLocal(T&& initialValue);

                    // -------------------------------------------------------------
                    // ACCÈS À LA VALEUR THREAD-LOCAL
                    // -------------------------------------------------------------

                    /// @brief Récupère la référence vers la valeur thread-local.
                    /// @return Référence vers T spécifique au thread appelant.
                    /// @note Initialisation lazy : la valeur est créée au premier appel dans chaque thread.
                    /// @note Si une valeur initiale a été fournie, elle est utilisée ; sinon default-construct.
                    /// @note Thread-safe par design : thread_local garantit l'isolation.
                    /// @note Retourne toujours la même référence pour un thread donné après initialisation.
                    [[nodiscard]] T& Get() const noexcept;

                    /// @brief Définit la valeur thread-local (copie).
                    /// @param value Nouvelle valeur à assigner (copiée dans le storage thread-local).
                    /// @note Équivalent à Get() = value mais avec sémantique explicite.
                    /// @note La modification n'affecte que le thread appelant.
                    void Set(const T& value) noexcept;

                    /// @brief Définit la valeur thread-local (déplacement).
                    /// @param value Nouvelle valeur à assigner (déplacée dans le storage thread-local).
                    /// @note Plus efficace que Set(const T&) pour les types coûteux.
                    /// @note La modification n'affecte que le thread appelant.
                    void Set(T&& value) noexcept;

                    // -------------------------------------------------------------
                    // OPÉRATEURS DE TYPE POINTERR (syntaxe intuitive)
                    // -------------------------------------------------------------

                    /// @brief Opérateur de déréférencement par flèche (mutable).
                    /// @return Pointeur vers la valeur thread-local pour accès membre.
                    /// @note Permet la syntaxe : threadLocal->memberFunction().
                    /// @note Délègue vers Get() puis prend l'adresse.
                    T* operator->() noexcept;

                    /// @brief Opérateur de déréférencement par flèche (const).
                    /// @return Pointeur const vers la valeur thread-local.
                    /// @note Permet la syntaxe const-correcte : constThreadLocal->memberFunction().
                    const T* operator->() const noexcept;

                    /// @brief Opérateur de déréférencement par astérisque (mutable).
                    /// @return Référence vers la valeur thread-local.
                    /// @note Permet la syntaxe : (*threadLocal).member ou *threadLocal = newValue.
                    T& operator*() noexcept;

                    /// @brief Opérateur de déréférencement par astérisque (const).
                    /// @return Référence const vers la valeur thread-local.
                    /// @note Permet l'accès en lecture seule via *constThreadLocal.
                    const T& operator*() const noexcept;

                private:

                    // -------------------------------------------------------------
                    // MEMBRES PRIVÉS (configuration d'initialisation)
                    // -------------------------------------------------------------

                    /// @brief Valeur initiale à utiliser pour le premier Get() dans chaque thread.
                    /// @note Stockée pour être copiée/déplacée lors de l'initialisation lazy.
                    T mInitialValue;

                    /// @brief Flag indiquant si une valeur initiale a été fournie.
                    /// @note Si false : la valeur sera default-constructée au premier accès.
                    /// @note Si true : mInitialValue sera utilisé pour l'initialisation.
                    nk_bool mHasInitialValue;
            };

        }
    }

    // -------------------------------------------------------------------------
    // SECTION 3 : NAMESPACE LEGACY (compatibilité rétroactive)
    // -------------------------------------------------------------------------
    // Ces alias dans nkentseu::entseu sont conservés uniquement pour la
    // rétrocompatibilité avec le code existant. Tout nouveau code doit
    // utiliser directement le namespace nkentseu::threading.
    // =====================================================================
    namespace nkentseu {

        // -----------------------------------------------------------------
        // ALIAS LEGACY : NkThreadLocal<T>
        // -----------------------------------------------------------------
        /// @brief Alias de compatibilité legacy pour NkThreadLocal<T>.
        /// @ingroup LegacyCompatibility
        /// @deprecated Utiliser nkentseu::threading::NkThreadLocal<T> directement.
        /// @tparam T Type de la valeur stockée en thread-local.
        /// @note Résolu à la compilation : zero runtime overhead.
        /// @note La spécialisation template est également redirigée correctement.
        template<typename T>
        using NkThreadLocal = ::nkentseu::threading::NkThreadLocal<T>;

    }

#endif

// =============================================================================
// IMPLÉMENTATIONS TEMPLATE (dans le header car templates)
// =============================================================================

namespace nkentseu {
    namespace threading {

        // ---------------------------------------------------------------------
        // Constructeur : initialisation par copie
        // ---------------------------------------------------------------------
        // Copie la valeur fournie dans mInitialValue et marque le flag.
        // Chaque thread recevra sa propre copie lors du premier Get().
        // ---------------------------------------------------------------------
        template<typename T>
        inline NkThreadLocal<T>::NkThreadLocal(const T& initialValue)
            : mInitialValue(initialValue)
            , mHasInitialValue(true)
        {
            // Copie de la valeur initiale pour réutilisation thread-local
            // mHasInitialValue = true indique d'utiliser mInitialValue
        }

        // ---------------------------------------------------------------------
        // Constructeur : initialisation par déplacement
        // ---------------------------------------------------------------------
        // Déplace la valeur fournie dans mInitialValue via traits::NkMove.
        // Plus efficace pour les types coûteux ou move-only.
        // ---------------------------------------------------------------------
        template<typename T>
        inline NkThreadLocal<T>::NkThreadLocal(T&& initialValue)
            : mInitialValue(traits::NkMove(initialValue))
            , mHasInitialValue(true)
        {
            // Déplacement de la valeur initiale pour éviter une copie inutile
            // Particulièrement utile pour std::string, std::vector, etc.
        }

        // ---------------------------------------------------------------------
        // Méthode : Get (accès à la valeur thread-local)
        // ---------------------------------------------------------------------
        // Retourne une référence vers la valeur stockée en thread_local.
        //
        // Comportement d'initialisation lazy :
        //   - Au premier appel dans un thread : la variable thread_local est
        //     initialisée avec mInitialValue (si fourni) ou default-constructée
        //   - Aux appels suivants : retourne directement la référence existante
        //
        // Garantie thread-safety :
        //   - thread_local est géré par le runtime C++ : isolation garantie
        //   - Aucune synchronisation explicite requise : lock-free par design
        //   - L'initialisation de thread_local est thread-safe en C++11+
        //
        // Note sur la sémantique const :
        //   - La méthode est marquée const car elle ne modifie pas l'état
        //     observable de l'instance NkThreadLocal (mInitialValue, etc.)
        //   - Mais elle retourne une référence mutable vers la valeur thread-local
        //   - C'est intentionnel : permet Get().member = newValue même sur const NkThreadLocal
        // ---------------------------------------------------------------------
        template<typename T>
        inline T& NkThreadLocal<T>::Get() const noexcept
        {
            if (mHasInitialValue) {
                thread_local T value = mInitialValue;
                return value;
            }
            thread_local T value{};
            return value;
        }

        // ---------------------------------------------------------------------
        // Méthode : Set (assignation par copie)
        // ---------------------------------------------------------------------
        // Assigne une nouvelle valeur à la variable thread-local via copie.
        // Équivalent sémantique à Get() = value mais avec nom explicite.
        // ---------------------------------------------------------------------
        template<typename T>
        inline void NkThreadLocal<T>::Set(const T& value) noexcept
        {
            Get() = value;
        }

        // ---------------------------------------------------------------------
        // Méthode : Set (assignation par déplacement)
        // ---------------------------------------------------------------------
        // Assigne une nouvelle valeur à la variable thread-local via déplacement.
        // Plus efficace que Set(const T&) pour les types coûteux.
        // ---------------------------------------------------------------------
        template<typename T>
        inline void NkThreadLocal<T>::Set(T&& value) noexcept
        {
            Get() = traits::NkMove(value);
        }

        // ---------------------------------------------------------------------
        // Opérateur : operator-> (mutable)
        // ---------------------------------------------------------------------
        // Permet l'accès aux membres de T via syntaxe pointer-like.
        // Exemple : threadLocal->size() au lieu de threadLocal.Get().size()
        // ---------------------------------------------------------------------
        template<typename T>
        inline T* NkThreadLocal<T>::operator->() noexcept
        {
            return &Get();
        }

        // ---------------------------------------------------------------------
        // Opérateur : operator-> (const)
        // ---------------------------------------------------------------------
        // Version const-correct de operator-> pour accès en lecture seule.
        // ---------------------------------------------------------------------
        template<typename T>
        inline const T* NkThreadLocal<T>::operator->() const noexcept
        {
            return &Get();
        }

        // ---------------------------------------------------------------------
        // Opérateur : operator* (mutable)
        // ---------------------------------------------------------------------
        // Permet la déréférenciation explicite via syntaxe *threadLocal.
        // Utile pour les opérations qui nécessitent une référence explicite.
        // ---------------------------------------------------------------------
        template<typename T>
        inline T& NkThreadLocal<T>::operator*() noexcept
        {
            return Get();
        }

        // ---------------------------------------------------------------------
        // Opérateur : operator* (const)
        // ---------------------------------------------------------------------
        // Version const-correct de operator* pour accès en lecture seule.
        // ---------------------------------------------------------------------
        template<typename T>
        inline const T& NkThreadLocal<T>::operator*() const noexcept
        {
            return Get();
        }

    }
}

// =============================================================================
// EXEMPLES D'UTILISATION (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Usage basique avec type primitif
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThreadLocal.h>
    using namespace nkentseu::threading;

    // Déclaration globale : partagée par tous les threads mais valeur isolée
    NkThreadLocal<int> threadCounter;

    void IncrementCounter()
    {
        // Chaque thread a sa propre copie de threadCounter
        ++(*threadCounter);  // Syntaxe via operator*
        // ou : threadCounter.Get()++
        // ou : *threadCounter += 1
    }

    // ---------------------------------------------------------------------
    // Exemple 2 : Valeur initiale personnalisée
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThreadLocal.h>

    // Chaque thread commence avec counter = 100
    NkThreadLocal<int> threadCounter(100);

    void ProcessWithOffset()
    {
        int current = *threadCounter;  // 100 au premier appel dans ce thread
        *threadCounter = current + 1;  // Incrémentation thread-local
    }

    // ---------------------------------------------------------------------
    // Exemple 3 : Types complexes avec initialisation move-only
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThreadLocal.h>
    #include <string>

    // Buffer thread-local réutilisable : évite les allocations répétées
    NkThreadLocal<std::string> threadBuffer(std::string(4096, '\0'));

    void FormatMessage(const char* format, ...)
    {
        // Réutilise le même buffer dans ce thread
        std::string& buffer = threadBuffer.Get();
        buffer.clear();
        // ... formatage dans buffer ...
    }

    // ---------------------------------------------------------------------
    // Exemple 4 : Syntaxe pointer-like avec operator->
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThreadLocal.h>
    #include <vector>

    NkThreadLocal<std::vector<int>> threadWorkQueue;

    void QueueWork(int item)
    {
        // Syntaxe intuitive : threadWorkQueue->push_back(item)
        threadWorkQueue->push_back(item);

        // Équivalent à : threadWorkQueue.Get().push_back(item)
        // Mais plus lisible et familier pour les utilisateurs de pointeurs
    }

    void ProcessQueue()
    {
        // Accès aux méthodes const via operator-> const
        const auto& queue = *threadWorkQueue;  // ou threadWorkQueue.Get()
        for (int item : queue) {
            ProcessItem(item);
        }
        threadWorkQueue->clear();  // Réinitialisation thread-local
    }

    // ---------------------------------------------------------------------
    // Exemple 5 : Pattern de statistiques par thread (agrégation finale)
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThreadLocal.h>
    #include <atomic>
    #include <vector>

    struct ThreadStats {
        uint64_t processed = 0;
        uint64_t errors = 0;
        double totalTime = 0.0;
    };

    // Stats thread-local : chaque thread maintient ses propres compteurs
    NkThreadLocal<ThreadStats> threadStats;

    void WorkerThread()
    {
        for (auto& task : GetTasks()) {
            try {
                ProcessTask(task);
                threadStats->processed++;  // Incrémentation thread-local
            } catch (...) {
                threadStats->errors++;
            }
        }
    }

    // Agrégation des stats de tous les threads (à appeler après join)
    ThreadStats AggregateStats(const std::vector<NkThread*>& threads)
    {
        ThreadStats total{};
        // Note : nécessite un mécanisme pour accéder aux stats des autres threads
        // (par exemple, un registre global de pointeurs vers ThreadStats)
        return total;
    }

    // ---------------------------------------------------------------------
    // Exemple 6 : Contexte de logging thread-local
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThreadLocal.h>
    #include <string>

    struct LogContext {
        std::string requestId;
        std::string userId;
        int severity = 0;
    };

    NkThreadLocal<LogContext> currentLogContext;

    void SetLogContext(const std::string& requestId, const std::string& userId)
    {
        currentLogContext->requestId = requestId;  // Thread-local assignment
        currentLogContext->userId = userId;
    }

    void LogMessage(int severity, const char* message)
    {
        const auto& ctx = *currentLogContext;  // Lecture thread-local
        if (severity >= ctx.severity) {
            printf("[req=%s user=%s] %s\n",
                   ctx.requestId.c_str(),
                   ctx.userId.c_str(),
                   message);
        }
    }

    // ---------------------------------------------------------------------
    // Exemple 7 : Cache de résultats intermédiaires (mémoization thread-local)
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThreadLocal.h>
    #include <unordered_map>

    using CacheType = std::unordered_map<std::string, int>;

    // Cache thread-local : évite la contention d'un cache global
    NkThreadLocal<CacheType> threadCache;

    int ExpensiveLookup(const std::string& key)
    {
        auto& cache = *threadCache;  // Accès thread-local

        auto it = cache.find(key);
        if (it != cache.end()) {
            return it->second;  // Cache hit : retour rapide
        }

        // Cache miss : calcul coûteux
        int result = PerformExpensiveComputation(key);
        cache[key] = result;  // Stockage thread-local uniquement
        return result;
    }

    // ---------------------------------------------------------------------
    // Exemple 8 : Comparaison avec alternatives (mutex, std::map, etc.)
    // ---------------------------------------------------------------------
    // ❌ APPROCHE NAÏVE : mutex-protected global (contention élevée)
    std::mutex globalMutex;
    std::unordered_map<ThreadId, int> globalCounter;

    void BadIncrement(ThreadId tid)
    {
        std::lock_guard<std::mutex> lock(globalMutex);  // ❌ Contention !
        globalCounter[tid]++;
    }

    // ✅ APPROCHE OPTIMISÉE : thread-local (zero contention)
    NkThreadLocal<int> threadCounter;

    void GoodIncrement()
    {
        ++(*threadCounter);  // ✅ Lock-free, isolation garantie
    }

    // ---------------------------------------------------------------------
    // Exemple 9 : Bonnes pratiques et anti-patterns
    // ---------------------------------------------------------------------
    // ✅ BON : Utiliser thread-local pour données thread-specific
    void GoodUsage()
    {
        static NkThreadLocal<std::string> buffer;
        buffer->clear();  // Réinitialisation thread-local
        // ... utilisation ...
    }

    // ❌ MAUVAIS : Attendre de la visibilité entre threads via thread-local
    void BadCrossThreadUsage()
    {
        static NkThreadLocal<int> value;
        *value = 42;  // ❌ Cette modification n'est visible QUE dans ce thread !

        // Un autre thread verra sa propre copie (non-initialisée ou avec sa propre valeur)
        // Ne jamais utiliser thread-local pour la communication inter-threads !
    }

    // ✅ BON : Initialiser avec une valeur significative pour éviter les surprises
    NkThreadLocal<int> safeCounter(0);  // ✅ Clair : commence à 0

    // ❌ MAUVAIS : Compter sur default-construction implicite pour les types complexes
    struct Complex { std::vector<int> data; };
    NkThreadLocal<Complex> riskyLocal;  // ❌ data sera vide, mais pas explicite

    // ✅ MIEUX : Initialisation explicite même pour default-constructible
    NkThreadLocal<Complex> safeLocal(Complex{});  // ✅ Intention claire

    // ✅ BON : Libérer les ressources si nécessaire (destruction thread-local)
    // Note : les destructeurs thread_local sont appelés automatiquement
    // à la fin du thread, mais l'ordre est non-spécifié entre différents
    // thread_locals. Éviter les dépendances circulaires.

    // ---------------------------------------------------------------------
    // Exemple 10 : Intégration avec système de pooling d'objets
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThreadLocal.h>
    #include <memory>

    template<typename T>
    class ThreadLocalObjectPool {
    public:
        // Récupère un objet du pool thread-local (ou en crée un nouveau)
        std::unique_ptr<T> Acquire()
        {
            auto& pool = *mPool;  // thread_local std::vector<std::unique_ptr<T>>
            if (!pool.empty()) {
                auto obj = std::move(pool.back());
                pool.pop_back();
                return obj;
            }
            return std::make_unique<T>();  // Fallback : nouvelle allocation
        }

        // Retourne un objet au pool thread-local pour réutilisation
        void Release(std::unique_ptr<T> obj)
        {
            if (obj) {
                obj->Reset();  // Optionnel : réinitialiser l'état
                mPool->push_back(std::move(obj));
            }
        }

    private:
        NkThreadLocal<std::vector<std::unique_ptr<T>>> mPool;
    };

    // Usage :
    // ThreadLocalObjectPool<DatabaseConnection> connPool;
    // auto conn = connPool.Acquire();
    // ... utilisation ...
    // connPool.Release(std::move(conn));

    // ---------------------------------------------------------------------
    // Exemple 11 : Migration depuis l'ancien namespace legacy
    // ---------------------------------------------------------------------
    // AVANT (code utilisant l'alias legacy) :
    // #include <NKThreading/NkThreadLocal.h>
    // using namespace nkentseu::entseu;
    // NkThreadLocal<int> legacyLocal;

    // APRÈS (code moderne recommandé) :
    // #include <NKThreading/NkThreadLocal.h>
    // using namespace nkentseu::threading;
    // NkThreadLocal<int> modernLocal;

    // Les deux fonctionnent grâce à l'alias de compatibilité,
    // mais le nouveau code doit utiliser nkentseu::threading directement.

    // ---------------------------------------------------------------------
    // Exemple 12 : Testing de thread-local behavior
    // ---------------------------------------------------------------------
    #include <gtest/gtest.h>
    #include <NKThreading/NkThreadLocal.h>
    #include <thread>
    #include <vector>

    TEST(NkThreadLocalTest, IsolationBetweenThreads)
    {
        nkentseu::threading::NkThreadLocal<int> localValue(10);

        std::vector<std::thread> workers;
        std::atomic<int> completed{0};

        // Lance plusieurs threads qui modifient leur copie locale
        for (int i = 0; i < 4; ++i) {
            workers.emplace_back([i, &localValue, &completed]() {
                // Chaque thread voit sa propre copie
                int initial = *localValue;  // 10 pour tous
                *localValue = initial + i;  // Modification thread-local
                completed.fetch_add(1);
            });
        }

        for (auto& t : workers) {
            t.join();
        }

        // Le thread principal voit toujours sa propre valeur (non modifiée)
        EXPECT_EQ(*localValue, 10);  // Valeur initiale, pas affectée par les workers
        EXPECT_EQ(completed.load(), 4);
    }

    TEST(NkThreadLocalTest, PointerLikeOperators)
    {
        struct TestStruct {
            int value = 0;
            void Increment() { ++value; }
            int GetValue() const { return value; }
        };

        nkentseu::threading::NkThreadLocal<TestStruct> localStruct;

        // Test operator->
        localStruct->Increment();
        EXPECT_EQ(localStruct->GetValue(), 1);

        // Test operator*
        (*localStruct).value = 42;
        EXPECT_EQ(localStruct->GetValue(), 42);

        // Test const access
        const auto& constLocal = localStruct;
        EXPECT_EQ(constLocal->GetValue(), 42);
        EXPECT_EQ((*constLocal).value, 42);
    }

    // ---------------------------------------------------------------------
    // Exemple 13 : Configuration CMake pour thread-local storage
    // ---------------------------------------------------------------------
    # CMakeLists.txt - Options pour thread-local

    # thread_local est supporté en C++11+, mais certains compilateurs
    # anciens peuvent nécessiter des flags spécifiques

    if(CMAKE_CXX_STANDARD LESS 11)
        message(FATAL_ERROR "NkThreadLocal requires C++11 or higher for thread_local")
    endif()

    # GCC/Clang : s'assurer que -ftls-model est approprié
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # global-dynamic : bon compromis performance/flexibilité
        target_compile_options(your_target PRIVATE -ftls-model=global-dynamic)
    endif()

    # MSVC : thread_local est supporté nativement depuis VS2015
    if(MSVC AND MSVC_VERSION LESS 1900)
        message(WARNING "NkThreadLocal may have limited support on MSVC < 2015")
    endif()

    // ---------------------------------------------------------------------
    // Exemple 14 : Debugging et instrumentation des thread-locals
    // ---------------------------------------------------------------------
    #include <NKThreading/NkThreadLocal.h>
    #include <NKCore/NkLogger.h>

    // Wrapper instrumenté pour debugging des accès thread-local
    template<typename T>
    class DebugThreadLocal : public NkThreadLocal<T> {
    public:
        using NkThreadLocal<T>::NkThreadLocal;  // Hériter des constructeurs

        T& Get() const noexcept
        {
            auto tid = NkThread::GetCurrentThreadId();  // Hypothétique
            NK_LOG_DEBUG("ThreadLocal<{}>::Get() called from thread {}",
                        typeid(T).name(), tid);
            return NkThreadLocal<T>::Get();
        }
    };

    // Usage en mode debug uniquement :
    #ifdef NK_DEBUG
        using DebugCounter = DebugThreadLocal<int>;
    #else
        using DebugCounter = NkThreadLocal<int>;
    #endif
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================