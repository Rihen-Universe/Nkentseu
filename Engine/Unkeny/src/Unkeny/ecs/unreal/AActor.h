#pragma once
// =============================================================================
// AActor.h  —  Style Unreal Engine
// Classe de base de tout objet placé dans le monde.
// Gère : composants, cycle de vie, spawn/destroy, tags, overlaps.
// =============================================================================
#include "UActorComponent.h"
#include <vector>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <cassert>
#include <algorithm>

namespace UE {

class UWorld;   // forward

// ---------------------------------------------------------------------------
// AActor
// ---------------------------------------------------------------------------
class AActor : public UObject {
    DECLARE_CLASS(AActor)
    friend class UWorld;

public:
    UCLASS()

    // ── Construction ───────────────────────────────────────────────────────
    AActor() {
        // RootComponent obligatoire (comme dans UE)
        mRootComponent = AddComponent<USceneComponent>();
        mRootComponent->SetName(FName("RootComponent"));
    }

    virtual ~AActor() = default;

    // ── Cycle de vie (appelé par UWorld) ───────────────────────────────────
    // PreInitializeComponents → InitializeComponents → BeginPlay → Tick → EndPlay

    virtual void PreInitializeComponents() {}

    void InitializeComponents() {
        for (auto& [key, comp] : mComponents)
            comp->InitializeComponent();
    }

    virtual void BeginPlay() {
        for (auto& [key, comp] : mComponents)
            if (comp->IsActive()) comp->BeginPlay();
    }

    // Tick : appelé par UWorld. Sous-classes overrident Tick().
    virtual void Tick(float DeltaTime) {
        if (!bCanEverTick || !mTickEnabled) return;
        // Tick les composants
        for (auto& [key, comp] : mComponents)
            if (comp->IsActive() && comp->bCanEverTick)
                comp->TickComponent(DeltaTime);
    }

    virtual void EndPlay(EEndPlayReason reason) {
        for (auto& [key, comp] : mComponents)
            comp->EndPlay(reason);
    }

    // ── Gestion des composants ─────────────────────────────────────────────

    // AddComponent<T> — crée, attache, et retourne un composant
    // Équivaut à CreateDefaultSubobject<T> dans le constructeur
    // ou NewObject<T> + RegisterComponent() en runtime
    template<typename T, typename... Args>
    T* AddComponent(Args&&... args) {
        static_assert(std::is_base_of_v<UActorComponent, T>,
                      "T must derive from UActorComponent");

        auto comp = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = comp.get();
        ptr->mOwner = this;

        // Nom auto si non fourni
        if (ptr->GetName().value.empty())
            ptr->SetName(FName(std::string(ptr->GetClass()) + "_" +
                               std::to_string(mComponents.size())));

        // Attache les SceneComponent au RootComponent par défaut
        if (auto* sc = dynamic_cast<USceneComponent*>(ptr)) {
            if (mRootComponent && sc != mRootComponent)
                sc->AttachTo(mRootComponent);
        }

        auto key = std::type_index(typeid(T));
        mComponents[key] = std::move(comp);
        mComponentOrder.push_back(ptr);

        // Si déjà en jeu : initialise immédiatement
        if (mHasBegunPlay) {
            ptr->InitializeComponent();
            if (ptr->bAutoActivate) ptr->BeginPlay();
        }

        return ptr;
    }

    // GetComponentByClass<T> — récupère le premier composant du type T
    template<typename T>
    T* GetComponentByClass() const {
        auto it = mComponents.find(std::type_index(typeid(T)));
        if (it == mComponents.end()) return nullptr;
        return static_cast<T*>(it->second.get());
    }

    // FindComponentByClass<T> — cherche aussi dans les sous-types (dynamic_cast)
    template<typename T>
    T* FindComponentByClass() const {
        for (auto& [key, comp] : mComponents)
            if (T* c = dynamic_cast<T*>(comp.get())) return c;
        return nullptr;
    }

    // GetAllComponentsOfClass<T>
    template<typename T>
    std::vector<T*> GetAllComponentsOfClass() const {
        std::vector<T*> result;
        for (auto& [key, comp] : mComponents)
            if (T* c = dynamic_cast<T*>(comp.get())) result.push_back(c);
        return result;
    }

    // DestroyComponent
    template<typename T>
    void DestroyComponent() {
        auto key = std::type_index(typeid(T));
        auto it  = mComponents.find(key);
        if (it == mComponents.end()) return;
        it->second->EndPlay(EEndPlayReason::Destroyed);
        mComponentOrder.erase(
            std::remove(mComponentOrder.begin(), mComponentOrder.end(),
                        it->second.get()),
            mComponentOrder.end());
        mComponents.erase(it);
    }

    // ── RootComponent ─────────────────────────────────────────────────────
    USceneComponent* GetRootComponent() const { return mRootComponent; }

    // Raccourcis vers la transformation du RootComponent
    USceneComponent::FVector GetActorLocation() const {
        return mRootComponent ? mRootComponent->GetWorldLocation()
                              : USceneComponent::FVector{};
    }
    void SetActorLocation(const USceneComponent::FVector& loc) {
        if (mRootComponent) mRootComponent->SetWorldLocation(loc);
    }

    // ── Tags (comme ActorHasTag() dans UE) ───────────────────────────────
    std::vector<FName> Tags;
    bool ActorHasTag(const FName& tag) const {
        for (auto& t : Tags) if (t == tag) return true;
        return false;
    }

    // ── Destruction ───────────────────────────────────────────────────────
    void Destroy() { mPendingDestroy = true; }
    bool IsPendingDestroy() const { return mPendingDestroy; }

    // ── Tick ──────────────────────────────────────────────────────────────
    UPROPERTY() bool bCanEverTick = true;
    void SetActorTickEnabled(bool b) { mTickEnabled = b; }

    // ── UWorld ────────────────────────────────────────────────────────────
    UWorld* GetWorld() const { return mWorld; }

    // ── Nombre de composants ──────────────────────────────────────────────
    size_t GetComponentCount() const { return mComponents.size(); }

protected:
    USceneComponent* mRootComponent = nullptr;

private:
    std::unordered_map<std::type_index, std::unique_ptr<UActorComponent>> mComponents;
    std::vector<UActorComponent*> mComponentOrder;
    UWorld* mWorld          = nullptr;
    bool    mHasBegunPlay   = false;
    bool    mPendingDestroy = false;
    bool    mTickEnabled    = true;
};

} // namespace UE
