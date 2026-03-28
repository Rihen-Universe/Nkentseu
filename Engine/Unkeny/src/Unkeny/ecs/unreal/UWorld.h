#pragma once
// =============================================================================
// UWorld.h  —  Style Unreal Engine
// Le monde gère tous les acteurs, le tick global, et les sous-systèmes.
// UWorldSubsystem = l'équivalent des "systèmes globaux" dans l'architecture UE.
// =============================================================================
#include "AActor.h"
#include <vector>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <string>

namespace UE {

// ---------------------------------------------------------------------------
// UWorldSubsystem  —  système global attaché au monde
// Simule UWorldSubsystem de UE5 : logique globale, itère les acteurs.
// ---------------------------------------------------------------------------
class UWorldSubsystem : public UObject {
    DECLARE_CLASS(UWorldSubsystem)
    friend class UWorld;
public:
    UCLASS()

    virtual void Initialize() {}
    virtual void Deinitialize() {}
    virtual void Tick(float DeltaTime) { (void)DeltaTime; }
    virtual bool ShouldCreateSubsystem() const { return true; }

    UWorld* GetWorld() const { return mWorld; }

private:
    UWorld* mWorld = nullptr;
};

// ---------------------------------------------------------------------------
// FActorSpawnParameters  —  paramètres pour SpawnActor<T>
// Équivaut à FActorSpawnParameters dans UE
// ---------------------------------------------------------------------------
struct FActorSpawnParameters {
    FName                     Name;
    AActor*                   Owner    = nullptr;
    USceneComponent::FVector  Location = {};
    USceneComponent::FRotator Rotation = {};
    bool                      bDeferBeginPlay = false;
};

// ---------------------------------------------------------------------------
// UWorld
// ---------------------------------------------------------------------------
class UWorld : public UObject {
    DECLARE_CLASS(UWorld)
public:
    UCLASS()

    UWorld() { SetName(FName("World")); }

    // ── SpawnActor<T> ──────────────────────────────────────────────────────
    // Équivaut exactement à UWorld::SpawnActor<T>() dans UE5
    template<typename T>
    T* SpawnActor(const FActorSpawnParameters& params = {}) {
        static_assert(std::is_base_of_v<AActor, T>, "T must derive from AActor");

        auto actor = std::make_unique<T>();
        T* ptr = actor.get();
        ptr->mWorld = this;

        // Appliquer les paramètres de spawn
        if (!params.Name.value.empty()) ptr->SetName(params.Name);
        else ptr->SetName(FName(std::string(ptr->GetClass()) + "_" +
                                std::to_string(mActors.size())));

        ptr->SetActorLocation(params.Location);

        // Cycle de vie
        ptr->PreInitializeComponents();
        ptr->InitializeComponents();

        mActors.push_back(std::move(actor));

        if (!params.bDeferBeginPlay) {
            ptr->mHasBegunPlay = true;
            ptr->BeginPlay();
        }

        printf("[UWorld] SpawnActor: '%s' (%s) à (%.1f, %.1f, %.1f)\n",
               ptr->GetName().c_str(), ptr->GetClass(),
               params.Location.X, params.Location.Y, params.Location.Z);

        return ptr;
    }

    // ── DestroyActor ──────────────────────────────────────────────────────
    void DestroyActor(AActor* actor) {
        if (actor) actor->Destroy();
    }

    // ── Tick global ───────────────────────────────────────────────────────
    void Tick(float DeltaTime) {
        const size_t n = mActors.size(); // snapshot avant spawn potentiel

        // 1. Tick les sous-systèmes
        for (auto& [key, sub] : mSubsystems)
            sub->Tick(DeltaTime);

        // 2. Tick les acteurs
        for (size_t i = 0; i < n; ++i)
            mActors[i]->Tick(DeltaTime);

        // 3. Nettoyage différé
        mActors.erase(
            std::remove_if(mActors.begin(), mActors.end(),
                [](const std::unique_ptr<AActor>& a){
                    if (!a->IsPendingDestroy()) return false;
                    a->EndPlay(EEndPlayReason::Destroyed);
                    return true;
                }),
            mActors.end());
    }

    // ── Sous-systèmes ─────────────────────────────────────────────────────

    // RegisterSubsystem<T> : crée et enregistre un sous-système
    template<typename T>
    T* RegisterSubsystem() {
        static_assert(std::is_base_of_v<UWorldSubsystem, T>,
                      "T must derive from UWorldSubsystem");
        auto sub = std::make_unique<T>();
        T* ptr   = sub.get();
        ptr->mWorld = this;
        ptr->SetName(FName(std::string(ptr->GetClass())));
        if (ptr->ShouldCreateSubsystem()) {
            ptr->Initialize();
            mSubsystems[std::type_index(typeid(T))] = std::move(sub);
            printf("[UWorld] Subsystem enregistré: %s\n", ptr->GetClass());
        }
        return ptr;
    }

    // GetSubsystem<T>
    template<typename T>
    T* GetSubsystem() const {
        auto it = mSubsystems.find(std::type_index(typeid(T)));
        return it != mSubsystems.end()
               ? static_cast<T*>(it->second.get())
               : nullptr;
    }

    // ── Recherche d'acteurs ───────────────────────────────────────────────

    // Équivaut à TActorIterator<T> dans UE
    template<typename T>
    std::vector<T*> GetAllActorsOfClass() const {
        std::vector<T*> result;
        for (auto& a : mActors)
            if (T* t = dynamic_cast<T*>(a.get())) result.push_back(t);
        return result;
    }

    AActor* FindActorByName(const FName& name) const {
        for (auto& a : mActors)
            if (a->GetName() == name) return a.get();
        return nullptr;
    }

    std::vector<AActor*> GetActorsByTag(const FName& tag) const {
        std::vector<AActor*> result;
        for (auto& a : mActors)
            if (a->ActorHasTag(tag)) result.push_back(a.get());
        return result;
    }

    // ── Stats ─────────────────────────────────────────────────────────────
    size_t GetActorCount()     const { return mActors.size(); }
    size_t GetSubsystemCount() const { return mSubsystems.size(); }

    void Deinitialize() {
        for (auto& [key, sub] : mSubsystems)
            sub->Deinitialize();
    }

private:
    std::vector<std::unique_ptr<AActor>> mActors;
    std::unordered_map<std::type_index, std::unique_ptr<UWorldSubsystem>> mSubsystems;
};

} // namespace UE
