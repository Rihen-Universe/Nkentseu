// =============================================================================
// main_unreal.cpp  —  Style Unreal Engine : exemple d'utilisation complet
// Compile : g++ -std=c++17 main_unreal.cpp -o unreal_demo
// =============================================================================
#include "ActorComponents.h"
#include <cstdio>
#include <cmath>

using namespace UE;
using FVector  = USceneComponent::FVector;
using FRotator = USceneComponent::FRotator;

// =============================================================================
// ACTEURS PERSONNALISÉS
// Montre comment créer ses propres AActor en overridant BeginPlay/Tick
// =============================================================================

// ---------------------------------------------------------------------------
// APlayerCharacter  —  personnage joueur avec mouvement et santé
// ---------------------------------------------------------------------------
class APlayerCharacter : public AActor {
    DECLARE_CLASS(APlayerCharacter)
public:
    UCLASS()

    // Composants déclarés comme membres (style constructeur UE)
    UStaticMeshComponent*  MeshComp      = nullptr;
    UMovementComponent*    MovementComp  = nullptr;
    UHealthComponent*      HealthComp    = nullptr;

    APlayerCharacter() {
        // Création des composants dans le constructeur (comme UE)
        // Équivaut à CreateDefaultSubobject<T>(TEXT("..."))
        MeshComp     = AddComponent<UStaticMeshComponent>("player_mesh", "player_mat");
        MovementComp = AddComponent<UMovementComponent>();
        HealthComp   = AddComponent<UHealthComponent>();

        // Configuration
        MovementComp->MaxSpeed     = 500.f;
        MovementComp->bApplyGravity = false;   // vue du dessus pour simplifier
        HealthComp->MaxHealth      = 150.f;
        HealthComp->bCanRespawn    = true;

        bCanEverTick = true;
    }

    void BeginPlay() override {
        AActor::BeginPlay();  // appelle BeginPlay() sur tous les composants
        printf("[APlayerCharacter] BeginPlay — %s\n", GetName().c_str());

        // Bind des callbacks (équivaut à BindUFunction/BindDynamic en UE)
        HealthComp->OnDeath.AddLambda([this](AActor* killer){
            (void)killer;
            printf("[APlayerCharacter] GAME OVER — %s est mort !\n",
                   GetName().c_str());
            bCanEverTick = false;
        });

        HealthComp->OnHealthChanged.AddLambda([](float old, float cur){
            printf("[APlayerCharacter] HP: %.0f → %.0f\n", old, cur);
        });
    }

    void Tick(float DeltaTime) override {
        AActor::Tick(DeltaTime);  // tick les composants

        // Simulation d'input clavier (mouvement circulaire)
        mTime += DeltaTime;
        float speed = 300.f;
        FVector input = {
            std::cos(mTime) * speed,
            std::sin(mTime) * speed,
            0.f
        };
        MovementComp->AddInputVector(VNorm(input));
    }

    void EndPlay(EEndPlayReason reason) override {
        printf("[APlayerCharacter] EndPlay (%d)\n", (int)reason);
        AActor::EndPlay(reason);
    }

private:
    float mTime = 0.f;
};

// ---------------------------------------------------------------------------
// AEnemyCharacter  —  ennemi avec IA, mesh, santé
// ---------------------------------------------------------------------------
class AEnemyCharacter : public AActor {
    DECLARE_CLASS(AEnemyCharacter)
public:
    UCLASS()

    UStaticMeshComponent*  MeshComp       = nullptr;
    UHealthComponent*      HealthComp     = nullptr;
    UMovementComponent*    MovementComp   = nullptr;
    UAIPerceptionComponent* PerceptionComp = nullptr;

    AActor* Target = nullptr;   // cible assignée manuellement ou via perception

    UPROPERTY(EditAnywhere) float AttackDamage  = 20.f;
    UPROPERTY(EditAnywhere) float AttackRange   = 150.f;
    UPROPERTY(EditAnywhere) float ChaseSpeed    = 280.f;

    AEnemyCharacter() {
        MeshComp       = AddComponent<UStaticMeshComponent>("enemy_mesh", "enemy_mat");
        HealthComp     = AddComponent<UHealthComponent>();
        MovementComp   = AddComponent<UMovementComponent>();
        PerceptionComp = AddComponent<UAIPerceptionComponent>();

        HealthComp->MaxHealth = 80.f;
        MovementComp->MaxSpeed = ChaseSpeed;
        MovementComp->bApplyGravity = false;
        PerceptionComp->SightRadius = 700.f;
        PerceptionComp->TargetTag   = "Player";

        bCanEverTick = true;
    }

    void BeginPlay() override {
        AActor::BeginPlay();
        printf("[AEnemyCharacter] BeginPlay — %s\n", GetName().c_str());

        // Réagir à la perception
        PerceptionComp->OnTargetPerceptionUpdated.AddLambda(
            [this](AActor* actor, bool detected){
                if (detected) {
                    Target = actor;
                    printf("[AEnemyCharacter] %s détecte %s !\n",
                           GetName().c_str(), actor->GetName().c_str());
                } else {
                    printf("[AEnemyCharacter] %s perd %s de vue\n",
                           GetName().c_str(), actor->GetName().c_str());
                    Target = nullptr;
                }
            });

        HealthComp->OnDeath.AddLambda([this](AActor*){
            printf("[AEnemyCharacter] %s est éliminé !\n", GetName().c_str());
        });
    }

    void Tick(float DeltaTime) override {
        AActor::Tick(DeltaTime);

        if (!Target || !HealthComp->IsAlive()) return;

        FVector myPos  = GetActorLocation();
        FVector tgtPos = Target->GetActorLocation();
        FVector dir    = VNorm(VSub(tgtPos, myPos));
        float   dist   = VLen(VSub(tgtPos, myPos));

        if (dist > AttackRange) {
            // Chasse
            MovementComp->AddInputVector(dir);
        } else {
            // Attaque
            mAttackTimer += DeltaTime;
            if (mAttackTimer >= mAttackCooldown) {
                mAttackTimer = 0.f;
                if (auto* hp = Target->FindComponentByClass<UHealthComponent>()) {
                    hp->TakeDamage(AttackDamage, this);
                }
            }
        }
    }

private:
    float mAttackTimer    = 0.f;
    float mAttackCooldown = 1.2f;
};

// ---------------------------------------------------------------------------
// ACrystalPickup  —  objet ramassable qui tourne
// ---------------------------------------------------------------------------
class ACrystalPickup : public AActor {
    DECLARE_CLASS(ACrystalPickup)
public:
    UCLASS()

    UStaticMeshComponent* MeshComp = nullptr;

    ACrystalPickup() {
        MeshComp = AddComponent<UStaticMeshComponent>("crystal", "crystal_mat");
        bCanEverTick = true;
    }

    void Tick(float dt) override {
        AActor::Tick(dt);
        // Rotation continue
        mRotY += 90.f * dt;
        if (auto* root = dynamic_cast<USceneComponent*>(MeshComp))
            root->RelativeRotation.Yaw = mRotY;
        // Oscillation verticale
        mTime += dt;
        FVector loc = GetActorLocation();
        SetActorLocation({loc.X, loc.Y, mBaseZ + std::sin(mTime*2.f)*0.3f});
    }

    void BeginPlay() override {
        AActor::BeginPlay();
        mBaseZ = GetActorLocation().Z;
    }

private:
    float mRotY  = 0.f;
    float mTime  = 0.f;
    float mBaseZ = 0.f;
};

// =============================================================================
// SOUS-SYSTÈMES GLOBAUX
// Équivalent de UWorldSubsystem dans UE — contient de la logique globale
// =============================================================================

// ---------------------------------------------------------------------------
// UEnemySpawnSubsystem  —  spawne des ennemis périodiquement
// ---------------------------------------------------------------------------
class UEnemySpawnSubsystem : public UWorldSubsystem {
    DECLARE_CLASS(UEnemySpawnSubsystem)
public:
    UCLASS()

    int   MaxEnemies     = 4;
    float SpawnInterval  = 3.f;
    int   SpawnCount     = 0;

    void Initialize() override {
        printf("[UEnemySpawnSubsystem] Initialisé\n");
    }

    void Tick(float dt) override {
        mTimer += dt;
        if (mTimer < SpawnInterval) return;
        mTimer = 0.f;

        if (SpawnCount >= MaxEnemies) return;

        // Trouver le joueur
        auto players = GetWorld()->GetActorsByTag(FName("Player"));
        if (players.empty()) return;

        // Spawn un ennemi
        float angle = SpawnCount * 1.571f;  // 90° entre chaque
        FActorSpawnParameters params;
        params.Location = {std::cos(angle)*12.f, std::sin(angle)*12.f, 0.f};

        AEnemyCharacter* enemy =
            GetWorld()->SpawnActor<AEnemyCharacter>(params);
        enemy->Tags.push_back(FName("Enemy"));
        ++SpawnCount;
    }

    void Deinitialize() override {
        printf("[UEnemySpawnSubsystem] Déinitialisé, ennemis spawnés: %d\n",
               SpawnCount);
    }

private:
    float mTimer = 0.f;
};

// ---------------------------------------------------------------------------
// UScoreSubsystem  —  comptabilise les kills
// ---------------------------------------------------------------------------
class UScoreSubsystem : public UWorldSubsystem {
    DECLARE_CLASS(UScoreSubsystem)
public:
    UCLASS()

    void AddKill(int points = 100) {
        mScore += points;
        mKills++;
        printf("[UScoreSubsystem] Kill ! +%d pts → Score total: %d\n",
               points, mScore);
    }

    int GetScore() const { return mScore; }
    int GetKills() const { return mKills; }

    void Initialize() override {
        printf("[UScoreSubsystem] Score initialisé\n");
    }

private:
    int mScore = 0;
    int mKills = 0;
};

// =============================================================================
// PROGRAMME PRINCIPAL
// =============================================================================
int main() {
    printf("============================================================\n");
    printf("      DEMO  —  Style ECS Actor/Component (Unreal)\n");
    printf("============================================================\n\n");

    // ── Créer le monde ────────────────────────────────────────────────────
    UWorld world;
    world.SetName(FName("GameLevel_01"));

    // ── Enregistrer les sous-systèmes ─────────────────────────────────────
    auto* spawnSys = world.RegisterSubsystem<UEnemySpawnSubsystem>();
    spawnSys->MaxEnemies    = 3;
    spawnSys->SpawnInterval = 2.f;

    auto* scoreSys = world.RegisterSubsystem<UScoreSubsystem>();
    printf("\n");

    // ── Spawner le joueur ─────────────────────────────────────────────────
    FActorSpawnParameters playerParams;
    playerParams.Name     = FName("PlayerCharacter_0");
    playerParams.Location = {0.f, 0.f, 0.f};

    APlayerCharacter* player = world.SpawnActor<APlayerCharacter>(playerParams);
    player->Tags.push_back(FName("Player"));

    // ── Spawner des cristaux décoratifs ───────────────────────────────────
    {
        FActorSpawnParameters cp;
        cp.Location = {5.f, 3.f, 1.f};
        world.SpawnActor<ACrystalPickup>(cp);
    }
    {
        FActorSpawnParameters cp;
        cp.Location = {-4.f, 6.f, 1.f};
        world.SpawnActor<ACrystalPickup>(cp);
    }

    printf("\n[World] Acteurs: %zu | Subsystems: %zu\n\n",
           world.GetActorCount(), world.GetSubsystemCount());

    // ── Connecter les callbacks cross-acteurs ─────────────────────────────
    // On connecte après le spawn pour avoir accès à tous les acteurs.
    // En UE vrai on utiliserait des delegates Blueprint-assignables.
    // (ici fait dans Tick de chaque ennemi via FindComponentByClass)

    // Quand un ennemi meurt → score
    // On binde ça dans le spawn subsystem plutôt (démo simplifiée ici)

    // ── Boucle de simulation ──────────────────────────────────────────────
    const float dt    = 1.f / 30.f;
    const int   total = 240;   // 8 secondes
    float       time  = 0.f;

    for (int f = 0; f < total; ++f) {
        time += dt;
        world.Tick(dt);

        if (f % 60 == 0) {
            printf("\n--- Frame %d (t=%.1fs) | Acteurs: %zu ---\n",
                   f, time, world.GetActorCount());

            FVector pos = player->GetActorLocation();
            printf("  Joueur — PV: %.0f/%.0f | Pos: (%.1f, %.1f, %.1f)\n",
                   player->HealthComp->GetCurrentHealth(),
                   player->HealthComp->MaxHealth,
                   pos.X, pos.Y, pos.Z);

            // Tous les ennemis
            auto enemies = world.GetAllActorsOfClass<AEnemyCharacter>();
            for (auto* e : enemies) {
                FVector ep = e->GetActorLocation();
                printf("  Ennemi '%s' — PV: %.0f | Pos: (%.1f, %.1f)\n",
                       e->GetName().c_str(),
                       e->HealthComp->GetCurrentHealth(),
                       ep.X, ep.Y);
            }
        }

        // Simulation : un boss inflige 120 dégâts après 5 secondes
        if (f == 150) {
            printf("\n[Simulation] Attaque de boss : 120 dégâts !\n");
            player->HealthComp->TakeDamage(120.f, nullptr);
        }

        // Simulation : tuer le premier ennemi trouvé après 4 secondes
        if (f == 120) {
            auto enemies = world.GetAllActorsOfClass<AEnemyCharacter>();
            if (!enemies.empty()) {
                printf("\n[Simulation] Joueur tue '%s' !\n",
                       enemies[0]->GetName().c_str());
                enemies[0]->HealthComp->TakeDamage(200.f, player);
                scoreSys->AddKill(150);
            }
        }
    }

    // ── Résultats finaux ──────────────────────────────────────────────────
    printf("\n--- Résultats finaux ---\n");
    printf("Score final: %d | Kills: %d\n",
           scoreSys->GetScore(), scoreSys->GetKills());
    printf("Acteurs restants: %zu\n", world.GetActorCount());

    // Déinitialisation propre
    world.Deinitialize();
    printf("\n[Demo terminée]\n");
    return 0;
}
