#pragma once
// =============================================================================
// ActorComponents.h  —  Style Unreal Engine
// Composants gameplay concrets : Health, StaticMesh, Movement, Projectile.
// Chacun démontre le pattern UActorComponent avec son cycle de vie complet.
// =============================================================================
#include "UWorld.h"
#include <cmath>
#include <string>
#include <algorithm>

namespace UE {

using FVector  = USceneComponent::FVector;
using FRotator = USceneComponent::FRotator;

// ── Helpers math ──────────────────────────────────────────────────────────
inline float VLen(FVector v){ return std::sqrt(v.X*v.X+v.Y*v.Y+v.Z*v.Z); }
inline FVector VNorm(FVector v){
    float l=VLen(v); return l>1e-6f?FVector{v.X/l,v.Y/l,v.Z/l}:FVector{};
}
inline FVector VSub(FVector a,FVector b){ return{a.X-b.X,a.Y-b.Y,a.Z-b.Z}; }
inline FVector VAdd(FVector a,FVector b){ return{a.X+b.X,a.Y+b.Y,a.Z+b.Z}; }
inline FVector VMul(FVector v,float s)  { return{v.X*s,v.Y*s,v.Z*s}; }

// ---------------------------------------------------------------------------
// UHealthComponent
// ---------------------------------------------------------------------------
class UHealthComponent : public UActorComponent {
    DECLARE_CLASS(UHealthComponent)
public:
    UCLASS()

    UPROPERTY(EditAnywhere) float MaxHealth    = 100.f;
    UPROPERTY(EditAnywhere) bool  bCanRespawn  = false;

    // Delegates (simule DECLARE_DYNAMIC_MULTICAST_DELEGATE)
    TMulticastDelegate<float, float>  OnHealthChanged; // (oldHp, newHp)
    TMulticastDelegate<AActor*>       OnDeath;         // (killer)
    TDelegate<float, AActor*>         OnTakeDamage;    // (amount, instigator)

    void InitializeComponent() override {
        mCurrentHealth = MaxHealth;
        printf("[UHealthComponent] InitializeComponent — HP: %.0f/%.0f\n",
               mCurrentHealth, MaxHealth);
    }

    void BeginPlay() override {
        printf("[UHealthComponent] BeginPlay sur '%s'\n",
               GetOwner()->GetName().c_str());
    }

    // Équivaut à UGameplayStatics::ApplyDamage() → TakeDamage()
    void TakeDamage(float amount, AActor* instigator = nullptr) {
        if (!IsAlive()) return;
        if (OnTakeDamage.IsBound()) OnTakeDamage.Execute(amount, instigator);

        float oldHp = mCurrentHealth;
        mCurrentHealth = std::max(0.f, mCurrentHealth - amount);
        OnHealthChanged.Broadcast(oldHp, mCurrentHealth);

        printf("[UHealthComponent] '%s' subit %.0f dégâts → PV: %.0f/%.0f\n",
               GetOwner()->GetName().c_str(), amount, mCurrentHealth, MaxHealth);

        if (mCurrentHealth <= 0.f) {
            OnDeath.Broadcast(instigator);
            printf("[UHealthComponent] '%s' est mort !\n",
                   GetOwner()->GetName().c_str());
            GetOwner()->Destroy();
        }
    }

    void Heal(float amount) {
        float old = mCurrentHealth;
        mCurrentHealth = std::min(mCurrentHealth + amount, MaxHealth);
        OnHealthChanged.Broadcast(old, mCurrentHealth);
    }

    float GetCurrentHealth() const { return mCurrentHealth; }
    float GetHealthPercent()  const { return mCurrentHealth / MaxHealth; }
    bool  IsAlive()           const { return mCurrentHealth > 0.f; }

private:
    UPROPERTY() float mCurrentHealth = 100.f;
};

// ---------------------------------------------------------------------------
// UStaticMeshComponent  —  représente une géométrie rendue
// ---------------------------------------------------------------------------
class UStaticMeshComponent : public USceneComponent {
    DECLARE_CLASS(UStaticMeshComponent)
public:
    UCLASS()

    UPROPERTY(EditAnywhere) std::string MeshAsset;
    UPROPERTY(EditAnywhere) std::string MaterialAsset;
    UPROPERTY(EditAnywhere) bool        CastShadow   = true;
    UPROPERTY(EditAnywhere) bool        bVisible     = true;

    UStaticMeshComponent() = default;
    UStaticMeshComponent(std::string mesh, std::string mat)
        : MeshAsset(std::move(mesh)), MaterialAsset(std::move(mat)) {}

    void InitializeComponent() override {
        printf("[UStaticMeshComponent] Load mesh='%s' mat='%s'\n",
               MeshAsset.c_str(), MaterialAsset.c_str());
    }

    void EndPlay(EEndPlayReason) override {
        printf("[UStaticMeshComponent] Unload mesh='%s'\n", MeshAsset.c_str());
    }
};

// ---------------------------------------------------------------------------
// UMovementComponent  —  déplacement physique simplifié
// ---------------------------------------------------------------------------
class UMovementComponent : public UActorComponent {
    DECLARE_CLASS(UMovementComponent)
public:
    UCLASS()

    UPROPERTY(EditAnywhere) float MaxSpeed       = 600.f;   // cm/s (unités UE)
    UPROPERTY(EditAnywhere) float Acceleration   = 2000.f;
    UPROPERTY(EditAnywhere) float Deceleration   = 4000.f;
    UPROPERTY(EditAnywhere) float GravityScale   = 1.f;
    UPROPERTY(EditAnywhere) bool  bApplyGravity  = true;

    FVector Velocity;

    void AddInputVector(FVector dir) {
        mInputVector = VAdd(mInputVector, dir);
    }

    void Launch(FVector launchVelocity) {
        Velocity = launchVelocity;
    }

    void TickComponent(float dt) override {
        if (!IsActive()) return;

        // Gravité
        if (bApplyGravity) Velocity.Z -= 980.f * GravityScale * dt;

        // Accélération selon l'input
        float inputLen = VLen(mInputVector);
        if (inputLen > 1e-4f) {
            FVector dir   = VNorm(mInputVector);
            Velocity.X += dir.X * Acceleration * dt;
            Velocity.Y += dir.Y * Acceleration * dt;
            // Clamp vitesse horizontale
            float hSpeed = std::sqrt(Velocity.X*Velocity.X + Velocity.Y*Velocity.Y);
            if (hSpeed > MaxSpeed) {
                float scale = MaxSpeed / hSpeed;
                Velocity.X *= scale;
                Velocity.Y *= scale;
            }
        } else {
            // Décélération
            float hSpeed = std::sqrt(Velocity.X*Velocity.X + Velocity.Y*Velocity.Y);
            if (hSpeed > 0.f) {
                float decel = std::min(Deceleration * dt, hSpeed);
                float scale = (hSpeed - decel) / hSpeed;
                Velocity.X *= scale;
                Velocity.Y *= scale;
            }
        }

        // Déplace l'acteur
        AActor* owner = GetOwner();
        if (owner) {
            FVector loc = owner->GetActorLocation();
            owner->SetActorLocation({
                loc.X + Velocity.X * dt,
                loc.Y + Velocity.Y * dt,
                loc.Z + Velocity.Z * dt
            });
        }

        mInputVector = {};  // reset input
    }

    FVector GetVelocity() const { return Velocity; }

private:
    FVector mInputVector;
};

// ---------------------------------------------------------------------------
// UAIPerceptionComponent  —  détection d'acteurs dans un rayon
// (simule UAIPerceptionComponent de l'AI Module UE)
// ---------------------------------------------------------------------------
class UAIPerceptionComponent : public UActorComponent {
    DECLARE_CLASS(UAIPerceptionComponent)
public:
    UCLASS()

    UPROPERTY(EditAnywhere) float SightRadius      = 800.f;
    UPROPERTY(EditAnywhere) float HearingRadius    = 400.f;
    UPROPERTY(EditAnywhere) std::string TargetTag  = "Player";

    // Delegate : quand un acteur est perçu
    TMulticastDelegate<AActor*, bool> OnTargetPerceptionUpdated; // (actor, isDetected)

    void TickComponent(float dt) override {
        (void)dt;
        if (!GetOwner() || !GetOwner()->GetWorld()) return;

        auto targets = GetOwner()->GetWorld()->GetActorsByTag(FName(TargetTag));
        FVector myPos = GetOwner()->GetActorLocation();

        for (AActor* target : targets) {
            FVector tPos = target->GetActorLocation();
            float dist   = VLen(VSub(tPos, myPos));
            bool seen    = dist < SightRadius;

            if (seen != mWasSeen) {
                mWasSeen = seen;
                OnTargetPerceptionUpdated.Broadcast(target, seen);
            }
        }
    }

private:
    bool mWasSeen = false;
};

} // namespace UE
