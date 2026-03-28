// =============================================================================
// main_unity.cpp  —  Style Unity : exemple d'utilisation complet
// Compile : g++ -std=c++17 main_unity.cpp -o unity_demo
// =============================================================================
#include "Scene.h"
#include "Components.h"
#include <cstdio>
#include <cstring>

using namespace Unity;

// =============================================================================
// EXEMPLE 1 : Composant personnalisé — IA d'un ennemi simple
// Montre comment créer ses propres composants en dérivant de Component.
// =============================================================================
class EnemyAI : public Component {
public:
    float detectionRadius = 8.f;
    float attackDamage    = 15.f;
    float attackCooldown  = 1.5f;  // secondes entre deux attaques

    enum class State { Idle, Chase, Attack };
    State state = State::Idle;

    GameObject* player = nullptr;

    void Awake() override {
        printf("[EnemyAI] Awake sur '%s'\n", GetGameObject()->name.c_str());
        mHealth = GetComponent<Health>();  // récupère Health au démarrage
    }

    void Start() override {
        printf("[EnemyAI] Start — prêt à chasser\n");
    }

    void Update(float dt) override {
        if (!player || !mHealth || !mHealth->IsAlive()) return;

        // Distance vers le joueur
        Vec3  myPos     = GetComponent<Transform>()->WorldPosition();
        Vec3  playerPos = player->GetTransform()->WorldPosition();
        float dist      = (playerPos - myPos).Length();

        // Machine à états
        switch (state) {
            case State::Idle:
                if (dist < detectionRadius) {
                    state = State::Chase;
                    printf("[EnemyAI] %s détecte le joueur !\n",
                           GetGameObject()->name.c_str());
                }
                break;

            case State::Chase:
                if (dist > detectionRadius * 1.5f) {
                    state = State::Idle;
                    printf("[EnemyAI] %s perd le joueur de vue\n",
                           GetGameObject()->name.c_str());
                } else if (dist < 1.5f) {
                    state = State::Attack;
                } else {
                    // Avancer vers le joueur
                    Vec3 dir = (playerPos - myPos).Normalized();
                    GetComponent<Transform>()->Translate(dir * (3.f * dt));
                }
                break;

            case State::Attack:
                if (dist > 2.f) {
                    state = State::Chase;
                } else {
                    mAttackTimer += dt;
                    if (mAttackTimer >= attackCooldown) {
                        mAttackTimer = 0.f;
                        // Attaque le Health du joueur
                        if (Health* ph = player->GetComponent<Health>()) {
                            ph->TakeDamage(attackDamage);
                            printf("[EnemyAI] %s attaque le joueur pour %.0f dégâts (PV: %.0f)\n",
                                   GetGameObject()->name.c_str(),
                                   attackDamage, ph->GetHp());
                        }
                    }
                }
                break;
        }
    }

    void OnDestroy() override {
        printf("[EnemyAI] OnDestroy sur '%s'\n", GetGameObject()->name.c_str());
    }

private:
    Health* mHealth      = nullptr;
    float   mAttackTimer = 0.f;
};

// =============================================================================
// EXEMPLE 2 : Composant Spawner — crée des ennemis périodiquement
// Montre comment un composant peut interagir avec la Scene.
// =============================================================================
class EnemySpawner : public Component {
public:
    float       spawnInterval = 3.f;
    Scene*      scene         = nullptr;
    GameObject* player        = nullptr;
    int         maxEnemies    = 3;
    int         spawnCount    = 0;

    void Update(float dt) override {
        mTimer += dt;
        if (mTimer < spawnInterval) return;
        mTimer = 0.f;

        if (spawnCount >= maxEnemies) return;
        if (!scene || !player)       return;

        // Crée un ennemi
        std::string ename = "Enemy_" + std::to_string(++spawnCount);
        GameObject* enemy = scene->CreateGameObject(ename);
        enemy->tag        = "Enemy";

        // Positionne aléatoirement autour du joueur
        float angle = spawnCount * 2.09f; // ~120° entre chaque spawn
        enemy->GetTransform()->position = {
            std::cos(angle) * 10.f, 0.f, std::sin(angle) * 10.f
        };

        // Ajoute les composants
        auto* hp  = enemy->AddComponent<Health>();
        hp->maxHp = 50.f;
        hp->onDeath = [ename](GameObject* go){
            printf("[Mort] %s est mort !\n", ename.c_str());
            go->Destroy();  // destruction différée
        };

        auto* ai   = enemy->AddComponent<EnemyAI>();
        ai->player = player;

        enemy->AddComponent<Renderer>("sphere", "enemy_mat");

        printf("[Spawner] Spawned '%s' à (%.1f, 0, %.1f)\n",
               ename.c_str(),
               enemy->GetTransform()->position.x,
               enemy->GetTransform()->position.z);
    }

private:
    float mTimer = 0.f;
};

// =============================================================================
// PROGRAMME PRINCIPAL
// Simule plusieurs frames d'une boucle de jeu.
// =============================================================================
int main() {
    printf("============================================================\n");
    printf("         DEMO  —  Style ECS GameObject (Unity)\n");
    printf("============================================================\n\n");

    // ── Création de la scène ─────────────────────────────────────────────
    Scene scene("Level_01");

    // ── Joueur ─────────────────────────────────────────────────────────────
    GameObject* player = scene.CreateGameObject("Player");
    player->tag        = "Player";
    player->GetTransform()->position = {0, 0, 0};

    // Ajoute des composants au joueur
    auto* playerHealth = player->AddComponent<Health>();
    playerHealth->maxHp   = 100.f;
    playerHealth->onDeath = [](GameObject* go){
        printf("\n>>> LE JOUEUR EST MORT <<<\n\n");
    };

    player->AddComponent<Renderer>("player_mesh", "player_mat");

    auto* rb = player->AddComponent<Rigidbody>();
    rb->useGravity  = false;
    rb->isKinematic = true;  // le joueur est contrôlé par code

    // ── Spawner dans la scène ──────────────────────────────────────────────
    GameObject* spawnerGO = scene.CreateGameObject("EnemySpawner");
    auto* spawner         = spawnerGO->AddComponent<EnemySpawner>();
    spawner->scene        = &scene;
    spawner->player       = player;
    spawner->spawnInterval = 2.f;
    spawner->maxEnemies    = 3;

    // ── Objet décoratif : tourne et oscille ───────────────────────────────
    GameObject* crystal = scene.CreateGameObject("Crystal");
    crystal->GetTransform()->position = {5, 2, 3};
    crystal->AddComponent<Renderer>("crystal_mesh", "crystal_mat");
    crystal->AddComponent<RotateAround>()->degreesPerSecond = 60.f;
    auto* osc = crystal->AddComponent<Oscillate>();
    osc->amplitude = 0.5f;
    osc->frequency = 0.5f;

    // ── Timer : désactive le cristal après 8 secondes ─────────────────────
    auto* crystalTimer     = crystal->AddComponent<Timer>();
    crystalTimer->duration = 8.f;
    crystalTimer->loop     = false;
    crystalTimer->onExpire = [crystal](){
        printf("[Timer] Crystal désactivé après 8s\n");
        crystal->SetActive(false);
    };

    // ── Hiérarchie parent/enfant ──────────────────────────────────────────
    GameObject* weapon = scene.CreateGameObject("PlayerWeapon");
    weapon->GetTransform()->position = {0.5f, 0.8f, 0.3f};  // offset local
    weapon->SetParent(player);  // le weapon suit le joueur
    weapon->AddComponent<Renderer>("sword_mesh", "sword_mat");

    printf("\n[Scene] %zu objets créés, démarrage de la simulation...\n\n",
           scene.ObjectCount());

    // ── Boucle de simulation (simule 10 secondes à 30fps) ─────────────────
    const float dt       = 1.f / 30.f;   // ~33ms par frame
    const int   frames   = 300;           // 10 secondes
    float       totalTime = 0.f;

    for (int f = 0; f < frames; ++f) {
        totalTime += dt;

        // Le joueur se déplace légèrement chaque frame (simulation input)
        float angle = totalTime * 0.5f;
        player->GetTransform()->position = {
            std::cos(angle) * 2.f, 0.f, std::sin(angle) * 2.f
        };

        // Update de la scène : Start() → Update() → LateUpdate() → cleanup
        scene.Update(dt);

        // Log périodique
        if (f % 60 == 0) {
            printf("\n--- Frame %d (t=%.1fs) | Objets scène: %zu ---\n",
                   f, totalTime, scene.ObjectCount());
            printf("  Joueur PV: %.0f/%.0f | Position: (%.1f, %.1f, %.1f)\n",
                   playerHealth->GetHp(), playerHealth->maxHp,
                   player->GetTransform()->position.x,
                   player->GetTransform()->position.y,
                   player->GetTransform()->position.z);

            // Arme suit bien le joueur (position monde)
            Vec3 wPos = weapon->GetTransform()->WorldPosition();
            printf("  Weapon (monde): (%.1f, %.1f, %.1f)\n",
                   wPos.x, wPos.y, wPos.z);
        }

        // Simulation: le joueur reçoit des dégâts directs après 7s
        if (f == 210) {
            printf("\n[Simulation] Boss inflige 80 dégâts au joueur !\n");
            playerHealth->TakeDamage(80.f);
        }
    }

    // ── Fin : afficher les composants trouvés dans la scène ───────────────
    printf("\n--- Fin de simulation ---\n");
    auto renderers = scene.FindAllComponents<Renderer>();
    printf("Renderers dans la scène: %zu\n", renderers.size());

    printf("\n[Demo terminée]\n");
    return 0;
}
