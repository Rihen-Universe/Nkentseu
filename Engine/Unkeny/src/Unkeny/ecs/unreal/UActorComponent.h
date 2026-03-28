#pragma once
// =============================================================================
// UActorComponent.h  —  Style Unreal Engine
// Classe de base de tous les composants attachables à un AActor.
// Cycle de vie : InitializeComponent → BeginPlay → TickComponent → EndPlay
// =============================================================================
#include "UObject.h"

namespace UE {

class AActor;   // forward

// Raisons de fin de jeu (simule EEndPlayReason)
enum class EEndPlayReason { Destroyed, LevelTransition, EndPlayInEditor };

// ---------------------------------------------------------------------------
// UActorComponent
// ---------------------------------------------------------------------------
class UActorComponent : public UObject {
    DECLARE_CLASS(UActorComponent)
    friend class AActor;

public:
    UCLASS()

    // ── Cycle de vie ───────────────────────────────────────────────────────
    // InitializeComponent : avant BeginPlay, une seule fois
    virtual void InitializeComponent() {}

    // BeginPlay : quand l'acteur entre dans le monde
    virtual void BeginPlay() {}

    // TickComponent : appelé chaque frame si bCanEverTick == true
    virtual void TickComponent(float DeltaTime) { (void)DeltaTime; }

    // EndPlay : quand l'acteur est retiré du monde
    virtual void EndPlay(EEndPlayReason reason) { (void)reason; }

    // ── Configuration ─────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere)
    bool bCanEverTick = true;

    UPROPERTY(EditAnywhere)
    bool bAutoActivate = true;

    // ── Activation ────────────────────────────────────────────────────────
    bool IsActive()   const { return mActive; }
    void Activate()         { mActive = true;  OnActivated();   }
    void Deactivate()       { mActive = false; OnDeactivated(); }

    virtual void OnActivated()   {}
    virtual void OnDeactivated() {}

    // ── Accès à l'acteur propriétaire ─────────────────────────────────────
    AActor* GetOwner() const { return mOwner; }

    // ── Utilitaire : cast vers un sous-type (simule Cast<T>()) ────────────
    template<typename T>
    T* As() { return dynamic_cast<T*>(this); }

private:
    AActor* mOwner  = nullptr;
    bool    mActive = true;
};

// ---------------------------------------------------------------------------
// USceneComponent  —  composant avec transformation spatiale
// En vrai UE : ajoute AttachToComponent, socket, etc.
// ---------------------------------------------------------------------------
class USceneComponent : public UActorComponent {
    DECLARE_CLASS(USceneComponent)
public:
    UCLASS()

    // Transformation locale
    struct FVector { float X=0,Y=0,Z=0; };
    struct FRotator { float Pitch=0,Yaw=0,Roll=0; };

    UPROPERTY(EditAnywhere) FVector  RelativeLocation;
    UPROPERTY(EditAnywhere) FRotator RelativeRotation;
    UPROPERTY(EditAnywhere) FVector  RelativeScale{1,1,1};

    // Parent dans la hiérarchie de composants
    USceneComponent* GetAttachParent() const { return mAttachParent; }

    void AttachTo(USceneComponent* parent) {
        mAttachParent = parent;
        if (parent) parent->mChildren.push_back(this);
    }

    // Position monde simplifiée (sans rotation pour la clarté)
    FVector GetWorldLocation() const {
        if (!mAttachParent) return RelativeLocation;
        FVector p = mAttachParent->GetWorldLocation();
        return {p.X + RelativeLocation.X,
                p.Y + RelativeLocation.Y,
                p.Z + RelativeLocation.Z};
    }

    void SetWorldLocation(const FVector& loc) {
        if (!mAttachParent) { RelativeLocation = loc; return; }
        FVector p = mAttachParent->GetWorldLocation();
        RelativeLocation = {loc.X-p.X, loc.Y-p.Y, loc.Z-p.Z};
    }

    const std::vector<USceneComponent*>& GetChildren() const { return mChildren; }

private:
    USceneComponent*              mAttachParent = nullptr;
    std::vector<USceneComponent*> mChildren;
};

} // namespace UE
