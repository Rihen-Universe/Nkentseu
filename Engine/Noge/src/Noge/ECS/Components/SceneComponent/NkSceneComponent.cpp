// =============================================================================
// FICHIER: NKECS/Components/SceneComponent/NkSceneComponent.cpp
// DESCRIPTION: Implémentations utilitaires pour NkSceneComponent.
// =============================================================================
#include "NkSceneComponent.h"
#include "../../World/NkWorld.h"
#include <cstring>

namespace nkentseu {
    namespace ecs {

        // ============================================================================
        // Helpers Hiérarchie (Niveau SceneComponent ↔ ECS)
        // ============================================================================

        void AttachSceneComponent(NkWorld& world, NkEntityId childId, NkEntityId parentId) noexcept {
            if (!world.IsAlive(childId) || !world.IsAlive(parentId)) {
                return;
            }

            // Mise à jour du composant OOP de l'enfant
            auto* childScene = world.Get<NkSceneComponent>(childId);
            if (childScene) {
                childScene->AttachTo(parentId);
            }

            // Mise à jour des composants ECS
            if (auto* pComp = world.Get<NkParent>(childId)) {
                pComp->entity = parentId;
            } else {
                world.Add<NkParent>(childId, NkParent{parentId});
            }

            auto* children = world.Get<NkChildren>(parentId);
            if (!children) {
                children = &world.Add<NkChildren>(parentId, NkChildren{});
            }
            if (!children->Has(childId)) {
                children->Add(childId);
            }

            // Marquer comme dirty pour le calcul des matrices
            if (auto* t = world.Get<NkTransform>(childId)) {
                t->dirty = true;
            }
        }

        void DetachSceneComponent(NkWorld& world, NkEntityId childId) noexcept {
            if (!world.IsAlive(childId)) {
                return;
            }

            // Mise à jour du composant OOP
            auto* childScene = world.Get<NkSceneComponent>(childId);
            if (childScene) {
                childScene->Detach();
            }

            // Nettoyage ECS (Retrait de la liste enfants du parent)
            if (const auto* pComp = world.Get<NkParent>(childId)) {
                if (pComp->entity.IsValid()) {
                    if (auto* children = world.Get<NkChildren>(pComp->entity)) {
                        children->Remove(childId);
                    }
                }
            }
            world.Remove<NkParent>(childId);

            // Marquer comme dirty
            if (auto* t = world.Get<NkTransform>(childId)) {
                t->dirty = true;
            }
        }

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
            return {mat.m[12], mat.m[13], mat.m[14]};
        }

        NkQuat GetSocketWorldRotation(const NkWorld& world, NkEntityId entityId, const char* socketName) noexcept {
            const NkMat4 mat = GetSocketWorldMatrix(world, entityId, socketName);
            // Extraction quaternion depuis matrice 4x4 (algorithme standard)
            float32 trace = mat.m[0] + mat.m[5] + mat.m[10];
            NkQuat q;

            if (trace > 0.f) {
                float32 s = 0.5f / NkSqrt(trace + 1.f);
                q.w = 0.25f / s;
                q.x = (mat.m[9] - mat.m[6]) * s;
                q.y = (mat.m[2] - mat.m[8]) * s;
                q.z = (mat.m[4] - mat.m[1]) * s;
            } else {
                if (mat.m[0] > mat.m[5] && mat.m[0] > mat.m[10]) {
                    float32 s = 2.f * NkSqrt(1.f + mat.m[0] - mat.m[5] - mat.m[10]);
                    q.x = 0.25f * s;
                    q.y = (mat.m[4] + mat.m[1]) / s;
                    q.z = (mat.m[2] + mat.m[8]) / s;
                    q.w = (mat.m[9] - mat.m[6]) / s;
                } else if (mat.m[5] > mat.m[10]) {
                    float32 s = 2.f * NkSqrt(1.f + mat.m[5] - mat.m[0] - mat.m[10]);
                    q.x = (mat.m[4] + mat.m[1]) / s;
                    q.y = 0.25f * s;
                    q.z = (mat.m[9] + mat.m[6]) / s;
                    q.w = (mat.m[2] - mat.m[8]) / s;
                } else {
                    float32 s = 2.f * NkSqrt(1.f + mat.m[10] - mat.m[0] - mat.m[5]);
                    q.x = (mat.m[2] + mat.m[8]) / s;
                    q.y = (mat.m[9] + mat.m[6]) / s;
                    q.z = 0.25f * s;
                    q.w = (mat.m[4] - mat.m[1]) / s;
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