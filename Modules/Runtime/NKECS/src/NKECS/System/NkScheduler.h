#pragma once
// =============================================================================
// System/NkScheduler.h
// Ordonnancement des systèmes ECS avec parallélisme basé sur un DAG.
//
// Algorithme :
//   1. Collecte les NkSystemDesc de tous les systèmes
//   2. Construit un DAG : arête A→B si A doit s'exécuter avant B
//      (dépendances explicites After<> + conflits d'écriture implicites)
//   3. Tri topologique → ordre d'exécution
//   4. Groupes parallèles : systèmes sans dépendances mutuelles dans le même
//      groupe s'exécutent en parallèle via le job system
//
// Modèle de concurrence :
//   - Thread pool simple (std::thread)
//   - Un system peut lire en parallèle d'un autre system s'il n'y a pas de write
//   - Les structural changes (Add/Remove) sont toujours différés
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NkSystem.h"
#include "NKECS/World/NkWorld.h"
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <algorithm>
#include <typeindex>

namespace nkentseu { namespace ecs {

// -----------------------------------------------------------------------------
// NkJobPool — thread pool minimaliste
// -----------------------------------------------------------------------------

class NkJobPool {
public:
    explicit NkJobPool(uint32 threadCount = 0) noexcept {
        const uint32 n = threadCount > 0
            ? threadCount
            : std::max(1u, static_cast<uint32>(std::thread::hardware_concurrency()) - 1u);

        mThreads.reserve(n);
        for (uint32 i = 0; i < n; ++i)
            mThreads.emplace_back([this] { WorkerLoop(); });
    }

    ~NkJobPool() noexcept {
        {
            std::unique_lock lock(mMutex);
            mStopping = true;
        }
        mCv.notify_all();
        for (auto& t : mThreads) t.join();
    }

    // Soumet un job. Retourne immédiatement.
    void Submit(std::function<void()> job) noexcept {
        {
            std::unique_lock lock(mMutex);
            mJobs.push_back(std::move(job));
            ++mPending;
        }
        mCv.notify_one();
    }

    // Attend que tous les jobs soumis soient terminés
    void WaitAll() noexcept {
        std::unique_lock lock(mDoneMutex);
        mDoneCv.wait(lock, [this] { return mPending.load() == 0; });
    }

    [[nodiscard]] uint32 ThreadCount() const noexcept {
        return static_cast<uint32>(mThreads.size());
    }

private:
    void WorkerLoop() noexcept {
        while (true) {
            std::function<void()> job;
            {
                std::unique_lock lock(mMutex);
                mCv.wait(lock, [this] { return !mJobs.empty() || mStopping; });
                if (mStopping && mJobs.empty()) return;
                job = std::move(mJobs.back());
                mJobs.pop_back();
            }
            job();
            if (--mPending == 0) mDoneCv.notify_all();
        }
    }

    std::vector<std::thread>        mThreads;
    std::vector<std::function<void()>> mJobs;
    std::mutex                      mMutex;
    std::condition_variable         mCv;
    std::mutex                      mDoneMutex;
    std::condition_variable         mDoneCv;
    std::atomic<int32>              mPending { 0 };
    bool                            mStopping = false;
};

// -----------------------------------------------------------------------------
// NkScheduler — gère l'enregistrement et l'exécution des systèmes
// -----------------------------------------------------------------------------

class NkScheduler {
public:
    explicit NkScheduler(uint32 jobThreads = 0) noexcept
        : mJobPool(jobThreads) {}

    // ── Enregistrement ────────────────────────────────────────────────────────

    // Enregistre un système typé (instanciation automatique)
    template<typename T, typename... Args>
    T& AddSystem(Args&&... args) noexcept {
        static_assert(std::is_base_of_v<NkSystem, T>,
                      "T must derive from NkSystem");
        auto sys = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *sys;
        RegisterSystem(std::move(sys));
        return ref;
    }

    // Enregistre un système lambda (prototypage rapide)
    NkLambdaSystem& AddLambda(NkSystemDesc desc,
                               std::function<void(NkWorld&, float32)> fn) noexcept {
        auto sys = std::make_unique<NkLambdaSystem>(std::move(desc), std::move(fn));
        NkLambdaSystem& ref = *sys;
        RegisterSystem(std::move(sys));
        return ref;
    }

    // Active/désactive un système par type
    template<typename T>
    void SetEnabled(bool enabled) noexcept {
        const auto tid = std::type_index(typeid(T));
        for (auto& entry : mSystems)
            if (entry.typeId == tid) { entry.system->SetEnabled(enabled); break; }
    }

    // ── Initialisation ────────────────────────────────────────────────────────

    // Appelle OnCreate sur tous les systèmes + construit le DAG
    void Init(NkWorld& world) noexcept {
        for (auto& entry : mSystems)
            if (entry.system->IsEnabled())
                entry.system->OnCreate(world);
        RebuildDAG();
        mInitialized = true;
    }

    // ── Exécution par frame ───────────────────────────────────────────────────

    void Run(NkWorld& world, float32 dt) noexcept {
        if (!mInitialized) Init(world);

        // 1. Drain des events en attente
        world.DrainEvents();

        // 2. Exécute chaque groupe dans l'ordre
        RunGroup(world, dt, NkSystemGroup::PreUpdate);
        RunGroup(world, dt, NkSystemGroup::Update);
        RunGroup(world, dt, NkSystemGroup::PostUpdate);
        RunGroup(world, dt, NkSystemGroup::Render);

        // 3. Flush des changements structurels différés
        world.FlushDeferred();

        // 4. Drain des events émis pendant la frame
        world.DrainEvents();
    }

    // ── FixedUpdate (appelé séparément par la boucle de jeu) ─────────────────

    void RunFixed(NkWorld& world, float32 fixedDt) noexcept {
        if (!mInitialized) Init(world);
        RunGroup(world, fixedDt, NkSystemGroup::FixedUpdate);
        world.FlushDeferred();
    }

    // ── Statistiques ─────────────────────────────────────────────────────────

    [[nodiscard]] uint32 SystemCount() const noexcept {
        return static_cast<uint32>(mSystems.size());
    }

    // Retourne le nom du système (pour debugger)
    [[nodiscard]] const char* GetSystemName(uint32 index) const noexcept {
        if (index >= mSystems.size()) return "<invalid>";
        return mSystems[index].desc.name;
    }

private:
    // ── Entrée interne ────────────────────────────────────────────────────────

    struct SystemEntry {
        std::unique_ptr<NkSystem> system;
        NkSystemDesc              desc;
        std::type_index           typeId;
        std::vector<uint32>       dependsOn; // indices dans mSystems
        uint32                    index = 0;
    };

    // ── Enregistrement interne ────────────────────────────────────────────────

    void RegisterSystem(std::unique_ptr<NkSystem> sys) noexcept {
        SystemEntry entry;
        entry.desc    = sys->Describe();
        entry.typeId  = std::type_index(typeid(*sys));
        entry.index   = static_cast<uint32>(mSystems.size());
        entry.system  = std::move(sys);
        mSystems.push_back(std::move(entry));
        if (mInitialized) RebuildDAG();
    }

    // ── Construction du DAG ───────────────────────────────────────────────────

    void RebuildDAG() noexcept {
        // 1. Réinitialise les dépendances
        for (auto& e : mSystems) e.dependsOn.clear();

        // 2. Dépendances explicites After<>
        for (uint32 i = 0; i < mSystems.size(); ++i) {
            for (const auto& afterType : mSystems[i].desc.afterTypes) {
                for (uint32 j = 0; j < mSystems.size(); ++j) {
                    if (i != j && mSystems[j].typeId == afterType)
                        mSystems[i].dependsOn.push_back(j);
                }
            }
        }

        // 3. Conflits d'écriture implicites (dans le même groupe)
        for (uint32 i = 0; i < mSystems.size(); ++i) {
            for (uint32 j = 0; j < i; ++j) {
                if (mSystems[i].desc.group != mSystems[j].desc.group) continue;
                if (mSystems[i].desc.ConflictsWith(mSystems[j].desc)) {
                    // j s'exécute avant i si j a une priorité plus basse
                    if (mSystems[j].desc.priority <= mSystems[i].desc.priority)
                        mSystems[i].dependsOn.push_back(j);
                    else
                        mSystems[j].dependsOn.push_back(i);
                }
            }
        }
    }

    // ── Exécution d'un groupe ─────────────────────────────────────────────────

    void RunGroup(NkWorld& world, float32 dt, NkSystemGroup group) noexcept {
        // Collecte les systèmes du groupe, triés par priorité + dépendances
        std::vector<uint32> toRun;
        for (uint32 i = 0; i < mSystems.size(); ++i) {
            SystemEntry& e = mSystems[i];
            if (e.desc.group == group && e.system->IsEnabled())
                toRun.push_back(i);
        }
        if (toRun.empty()) return;

        // Tri topologique (Kahn's algorithm)
        const std::vector<uint32> sorted = TopoSort(toRun);

        // Exécution par vagues parallèles
        // Chaque vague = ensemble de systèmes sans dépendances entre eux
        std::vector<bool> done(mSystems.size(), false);

        uint32 remaining = static_cast<uint32>(sorted.size());
        while (remaining > 0) {
            // Collecte les systèmes prêts (toutes leurs dépendances sont done)
            std::vector<uint32> wave;
            for (uint32 idx : sorted) {
                if (done[idx]) continue;
                bool ready = true;
                for (uint32 dep : mSystems[idx].dependsOn) {
                    if (std::find(sorted.begin(), sorted.end(), dep) != sorted.end()
                        && !done[dep]) { ready = false; break; }
                }
                if (ready) wave.push_back(idx);
            }

            if (wave.empty()) break; // cycle de dépendances → abort

            if (wave.size() == 1 || !mSystems[wave[0]].desc.runInParallel) {
                // Exécution séquentielle
                for (uint32 idx : wave) {
                    mSystems[idx].system->Execute(world, dt);
                    done[idx] = true;
                    --remaining;
                }
            } else {
                // Exécution parallèle des systèmes de cette vague
                std::atomic<uint32> waveCount { static_cast<uint32>(wave.size()) };
                for (uint32 idx : wave) {
                    mJobPool.Submit([&, idx] {
                        mSystems[idx].system->Execute(world, dt);
                    });
                }
                mJobPool.WaitAll();
                for (uint32 idx : wave) {
                    done[idx] = true;
                    --remaining;
                }
            }
        }
    }

    // Tri topologique (retourne les indices dans l'ordre d'exécution)
    std::vector<uint32> TopoSort(const std::vector<uint32>& subset) const noexcept {
        // Calcule le in-degree pour chaque nœud du sous-ensemble
        std::vector<uint32> inDegree(mSystems.size(), 0);
        for (uint32 idx : subset)
            for (uint32 dep : mSystems[idx].dependsOn)
                if (std::find(subset.begin(), subset.end(), dep) != subset.end())
                    ++inDegree[idx];

        std::vector<uint32> queue, result;
        for (uint32 idx : subset)
            if (inDegree[idx] == 0) queue.push_back(idx);

        // Trie par priorité dans la queue
        auto sortByPriority = [&] {
            std::sort(queue.begin(), queue.end(), [&](uint32 a, uint32 b) {
                return mSystems[a].desc.priority < mSystems[b].desc.priority;
            });
        };
        sortByPriority();

        while (!queue.empty()) {
            const uint32 cur = queue.front();
            queue.erase(queue.begin());
            result.push_back(cur);

            for (uint32 idx : subset) {
                for (uint32 dep : mSystems[idx].dependsOn) {
                    if (dep == cur && --inDegree[idx] == 0)
                        queue.push_back(idx);
                }
            }
            sortByPriority();
        }

        return result;
    }

    // ── Données ───────────────────────────────────────────────────────────────

    std::vector<SystemEntry> mSystems;
    NkJobPool                mJobPool;
    bool                     mInitialized = false;
};

}} // namespace nkentseu::ecs
