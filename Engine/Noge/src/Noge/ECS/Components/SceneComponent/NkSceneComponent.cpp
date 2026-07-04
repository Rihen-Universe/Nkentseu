// =============================================================================
// FICHIER: NKECS/Components/SceneComponent/NkSceneComponent.cpp
// DESCRIPTION: Implémentations utilitaires pour NkSceneComponent.
// =============================================================================
#include "NkSceneComponent.h"
#include "NKECS/World/NkWorld.h"
#include <cstring>

namespace nkentseu {
    namespace ecs {

        // AttachSceneComponent / DetachSceneComponent : definis INLINE dans
        // NkSceneComponent.h (source unique). Pas de redefinition out-of-line ici
        // (evite la redefinition ODR ; les versions OOP AttachTo()/Detach()
        // n'existent pas sur NkSceneComponent).

        // ============================================================================
        // Helpers Sockets (Calcul Matriciel)
        // ============================================================================

        NkMat4 GetSocketWorldMatrix(const NkWorld& world, NkEntityId entityId, const char* socketName) noexcept {
            const auto* scene = world.Get<NkSceneComponent>(entityId);
            const auto* transform = world.Get<NkTransform>(entityId);

            if (!scene || !transform || !scene->enabled) {
                return NkMat4::Identity();
            }

            const NkSocket* socket = scene->FindSocket(socketName);
            if (!socket) {
                return transform->worldMatrix;
            }

            // worldSocket = worldParent * localSocket
            return transform->worldMatrix * socket->GetLocalMatrix();
        }

        NkVec3 GetSocketWorldPosition(const NkWorld& world, NkEntityId entityId, const char* socketName) noexcept {
            const NkMat4 mat = GetSocketWorldMatrix(world, entityId, socketName);
            return {mat.data[12], mat.data[13], mat.data[14]};
        }

        NkQuat GetSocketWorldRotation(const NkWorld& world, NkEntityId entityId, const char* socketName) noexcept {
            const NkMat4 mat = GetSocketWorldMatrix(world, entityId, socketName);
            // Extraction quaternion depuis matrice 4x4 (algorithme standard)
            float32 trace = mat.data[0] + mat.data[5] + mat.data[10];
            NkQuat q;

            if (trace > 0.f) {
                float32 s = 0.5f / NkSqrt(trace + 1.f);
                q.w = 0.25f / s;
                q.x = (mat.data[9] - mat.data[6]) * s;
                q.y = (mat.data[2] - mat.data[8]) * s;
                q.z = (mat.data[4] - mat.data[1]) * s;
            } else {
                if (mat.data[0] > mat.data[5] && mat.data[0] > mat.data[10]) {
                    float32 s = 2.f * NkSqrt(1.f + mat.data[0] - mat.data[5] - mat.data[10]);
                    q.x = 0.25f * s;
                    q.y = (mat.data[4] + mat.data[1]) / s;
                    q.z = (mat.data[2] + mat.data[8]) / s;
                    q.w = (mat.data[9] - mat.data[6]) / s;
                } else if (mat.data[5] > mat.data[10]) {
                    float32 s = 2.f * NkSqrt(1.f + mat.data[5] - mat.data[0] - mat.data[10]);
                    q.x = (mat.data[4] + mat.data[1]) / s;
                    q.y = 0.25f * s;
                    q.z = (mat.data[9] + mat.data[6]) / s;
                    q.w = (mat.data[2] - mat.data[8]) / s;
                } else {
                    float32 s = 2.f * NkSqrt(1.f + mat.data[10] - mat.data[0] - mat.data[5]);
                    q.x = (mat.data[2] + mat.data[8]) / s;
                    q.y = (mat.data[9] + mat.data[6]) / s;
                    q.z = 0.25f * s;
                    q.w = (mat.data[4] - mat.data[1]) / s;
                }
            }
            return q;
        }

    } // namespace ecs
} // namespace nkentseu

// =============================================================================
// EXEMPLES D'UTILISATION DE NKSCENECOMPONENT.CPP
// =============================================================================

/*
// -----------------------------------------------------------------------------
// Exemple 1 : Attacher un enfant via AttachSceneComponent
// -----------------------------------------------------------------------------
void Exemple_Attach(NkWorld& world) {
    // Créer parent et enfant
    auto parent = world.CreateEntity();
    auto child = world.CreateEntity();

    // Ajouter les composants requis
    world.Add<NkTransform>(parent);
    world.Add<NkTransform>(child);
    world.Add<NkSceneComponent>(parent);
    world.Add<NkSceneComponent>(child);

    // Attacher l'enfant au parent
    AttachSceneComponent(world, child, parent);

    // Le système NkSceneTransformSystem calculera automatiquement
    // la transformation world de l'enfant lors de la prochaine frame.
}

// -----------------------------------------------------------------------------
// Exemple 2 : Utiliser un socket pour attacher une arme
// -----------------------------------------------------------------------------
void Exemple_SocketWeapon(NkWorld& world, NkEntityId player) {
    auto* scene = world.Get<NkSceneComponent>(player);
    if (scene) {
        // Ajouter un socket "RightHand"
        scene->AddSocket("RightHand", {0.3f, -0.2f, 0.1f}, NkQuat::Identity(), NkVec3::One());

        // Créer l'arme
        auto weapon = world.CreateEntity();
        world.Add<NkTransform>(weapon);
        world.Add<NkSceneComponent>(weapon);

        // Attacher l'arme au socket
        auto* weaponScene = world.Get<NkSceneComponent>(weapon);
        if (weaponScene) {
            weaponScene->AttachTo(player);
            // Optionnel : offset local via SetLocalPosition
            weaponScene->SetLocalPosition({0.3f, -0.2f, 0.1f});
        }
    }
}

// -----------------------------------------------------------------------------
// Exemple 3 : Obtenir la position world d'un socket
// -----------------------------------------------------------------------------
void Exemple_GetSocketPosition(NkWorld& world, NkEntityId entity) {
    NkVec3 muzzlePos = GetSocketWorldPosition(world, entity, "Muzzle");
    printf("Muzzle world position: (%.2f, %.2f, %.2f)
",
           muzzlePos.x, muzzlePos.y, muzzlePos.z);

    // Utiliser cette position pour spawn un projectile
    auto projectile = world.CreateEntity();
    world.Add<NkTransform>(projectile);
    auto* t = world.Get<NkTransform>(projectile);
    if (t) {
        t->position = muzzlePos;
        t->dirty = true;
    }
}

// -----------------------------------------------------------------------------
// Exemple 4 : Détacher proprement un composant
// -----------------------------------------------------------------------------
void Exemple_Detach(NkWorld& world, NkEntityId child) {
    // Détacher l'enfant de son parent actuel
    DetachSceneComponent(world, child);

    // Optionnel : réattacher à la racine de la scène
    // AttachSceneComponent(world, child, sceneRootId);
}

// -----------------------------------------------------------------------------
// Exemple 5 : Intégration avec NkGameObject
// -----------------------------------------------------------------------------
void Exemple_WithGameObject(NkWorld& world) {
    auto player = world.CreateGameObject("Player");
    auto* root = player.GetRootComponent();

    if (root) {
        // Ajouter des sockets via l'API OOP
        root->AddSocket("Head", {0, 1.6f, 0}, NkQuat::Identity(), NkVec3::One());
        root->AddSocket("RightHand", {0.3f, 1.2f, 0.2f}, NkQuat::Identity(), NkVec3::One());

        // Attacher une arme via ECS helper
        auto sword = world.CreateGameObject("Sword");
        AttachSceneComponent(world, sword.Id(), player.Id());
    }
}
*/