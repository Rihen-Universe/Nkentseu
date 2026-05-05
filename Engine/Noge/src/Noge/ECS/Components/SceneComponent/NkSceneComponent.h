// =============================================================================
// FICHIER: NKECS/Components/SceneComponent/NkSceneComponent.h
// DESCRIPTION: Composant hiérarchique style Unreal (Phase P1 - Mise à jour)
// =============================================================================
#pragma once
#include "../../NkECSDefines.h"
#include "../../Core/NkTypeRegistry.h"
#include "Nkentseu/ECS/Components/Core/NkTransform.h"
#include <cstring>


namespace nkentseu {
    namespace ecs {

        // Forward declarations
        class NkGameObject;
        struct NkSceneComponent;

        // ============================================================================
        // NkParent — Référence à l'entité parente dans la hiérarchie
        // ============================================================================
        struct NkParent {
            NkEntityId entity = NkEntityId::Invalid();
        };
        NK_COMPONENT(NkParent)

        // ============================================================================
        // NkChildren — Liste des entités enfants (max 64 pour cache-friendly)
        // ============================================================================
        struct NkChildren {
            static constexpr uint32 kMaxChildren = 64u;
            NkEntityId children[kMaxChildren] = {};
            uint32     count = 0;

            bool Add(NkEntityId child) noexcept {
                if (count >= kMaxChildren) return false;
                children[count++] = child;
                return true;
            }

            void Remove(NkEntityId child) noexcept {
                for (uint32 i = 0; i < count; ++i) {
                    if (children[i] == child) {
                        children[i] = children[--count];
                        return;
                    }
                }
            }

            bool Has(NkEntityId child) const noexcept {
                for (uint32 i = 0; i < count; ++i) {
                    if (children[i] == child) return true;
                }
                return false;
            }
        };
        NK_COMPONENT(NkChildren)

        // ============================================================================
        // NkSceneNode — Métadonnées de nœud de scène (optionnel, pour éditeur)
        // ============================================================================
        struct NkSceneNode {
            char   name[64] = {};
            bool   active   = true;
            bool   visible  = true;
            uint8  layer    = 0;

            NkSceneNode() noexcept = default;
            explicit NkSceneNode(const char* n) noexcept {
                NkStrNCpy(name, n, 63);
            }
        };
        NK_COMPONENT(NkSceneNode)

        // ============================================================================
        // NkSocket — Point d'attache nommé sur un SceneComponent
        // ============================================================================
        struct NkSocket {
            static constexpr uint32 kMaxNameLen = 64u;
            char name[kMaxNameLen] = {};
            NkVec3 localPosition = NkVec3::Zero();
            NkQuat localRotation = NkQuat::Identity();
            NkVec3 localScale = NkVec3::One();

            NkSocket() noexcept = default;
            NkSocket(const char* n, const NkVec3& pos, const NkQuat& rot, const NkVec3& scale) noexcept {
                NkStrNCpy(name, n, kMaxNameLen - 1);
                localPosition = pos;
                localRotation = rot;
                localScale = scale;
            }

            [[nodiscard]] NkMat4 GetLocalMatrix() const noexcept {
                return NkMat4::TRS(localPosition, localRotation, localScale);
            }
        };

        // ============================================================================
        // NkSceneComponent — Base hiérarchique style Unreal USceneComponent
        // ============================================================================
        // Ce composant gère :
        //   - La transformation locale (position/rotation/scale relative au parent)
        //   - Le dirty flag pour propagation batchée
        //   - Les sockets pour l'attachement d'objets
        //
        // Note : La hiérarchie parent/enfant est gérée via les composants ECS
        // NkParent et NkChildren, pas stockée ici pour éviter la duplication.
        // Les données world sont calculées par NkSceneTransformSystem.
        // ============================================================================
        struct NkSceneComponent {
            static constexpr uint32 kMaxSockets = 16u;

            // ── Transform locale (relative au parent) ─────────────────────────────
            NkVec3 localPosition = NkVec3::Zero();
            NkQuat localRotation = NkQuat::Identity();
            NkVec3 localScale = NkVec3::One();

            // ── Flags de mise à jour ──────────────────────────────────────────────
            bool dirty = true;           // true = world transform doit être recalculé
            bool hierarchyDirty = true;  // true = enfants doivent être recalculés

            // ── Sockets (points d'attache nommés) ─────────────────────────────────
            NkSocket sockets[kMaxSockets] = {};
            uint32 socketCount = 0;

            // ── Métadonnées ───────────────────────────────────────────────────────
            char componentName[64] = "SceneComponent";
            bool enabled = true;  // false = ignoré par les systèmes de rendu/physique

            // ── Constructeurs ─────────────────────────────────────────────────────
            NkSceneComponent() noexcept = default;
            explicit NkSceneComponent(const char* name) noexcept {
                NkStrNCpy(componentName, name, 63);
            }

            // ── API de transformation locale ──────────────────────────────────────
            void SetLocalPosition(const NkVec3& pos) noexcept {
                localPosition = pos;
                MarkDirty();
            }

            void SetLocalRotation(const NkQuat& rot) noexcept {
                localRotation = rot;
                MarkDirty();
            }

            void SetLocalScale(const NkVec3& scale) noexcept {
                localScale = scale;
                MarkDirty();
            }

            void SetLocalTransform(const NkVec3& pos, const NkQuat& rot, const NkVec3& scale) noexcept {
                localPosition = pos;
                localRotation = rot;
                localScale = scale;
                MarkDirty();
            }

            // ── Dirty flag management ─────────────────────────────────────────────
            void MarkDirty() noexcept {
                dirty = true;
                hierarchyDirty = true;
            }

            void MarkClean() noexcept {
                dirty = false;
            }

            [[nodiscard]] bool IsDirty() const noexcept {
                return dirty;
            }

            [[nodiscard]] bool IsHierarchyDirty() const noexcept {
                return hierarchyDirty;
            }

            // ── Calcul de matrice locale (TRS) ────────────────────────────────────
            [[nodiscard]] NkMat4 GetLocalMatrix() const noexcept {
                return NkMat4::TRS(localPosition, localRotation, localScale);
            }

            // ── Gestion des sockets ───────────────────────────────────────────────
            bool AddSocket(const char* name, const NkVec3& pos, const NkQuat& rot, const NkVec3& scale) noexcept {
                if (socketCount >= kMaxSockets) return false;
                sockets[socketCount++] = NkSocket(name, pos, rot, scale);
                return true;
            }

            bool AddSocket(const char* name) noexcept {
                return AddSocket(name, NkVec3::Zero(), NkQuat::Identity(), NkVec3::One());
            }

            [[nodiscard]] const NkSocket* FindSocket(const char* name) const noexcept {
                for (uint32 i = 0; i < socketCount; ++i) {
                    if (NkStrCmp(sockets[i].name, name) == 0) return &sockets[i];
                }
                return nullptr;
            }

            [[nodiscard]] bool HasSocket(const char* name) const noexcept {
                return FindSocket(name) != nullptr;
            }

            // ── Helpers 2D/3D ─────────────────────────────────────────────────────
            void SetLocalPosition2D(float32 x, float32 y) noexcept {
                SetLocalPosition({x, y, localPosition.z});
            }

            void SetLocalRotation2D(float32 angleDeg) noexcept {
                const float32 rad = angleDeg * (3.14159265f / 180.f);
                SetLocalRotation(NkQuat::FromAxisAngle({0, 0, 1}, rad));
            }
        };
        NK_COMPONENT(NkSceneComponent)

        // ============================================================================
        // Helpers Hiérarchie — Fonctions utilitaires pour attacher/détacher
        // ============================================================================
        // Ces fonctions assurent la cohérence entre NkSceneComponent et les
        // composants ECS NkParent/NkChildren.

        inline void AttachSceneComponent(NkWorld& world, NkEntityId childId, NkEntityId parentId) noexcept {
            if (!world.IsAlive(childId) || !world.IsAlive(parentId)) return;

            // Mettre à jour NkParent sur l'enfant
            if (auto* pComp = world.Get<NkParent>(childId)) {
                pComp->entity = parentId;
            } else {
                world.Add<NkParent>(childId, NkParent{parentId});
            }

            // Ajouter l'enfant à la liste NkChildren du parent
            auto* children = world.Get<NkChildren>(parentId);
            if (!children) {
                children = &world.Add<NkChildren>(parentId, NkChildren{});
            }
            if (!children->Has(childId)) {
                children->Add(childId);
            }

            // Marquer les transforms comme dirty pour recalcul
            if (auto* t = world.Get<NkTransform>(childId)) t->MarkDirty();
            if (auto* s = world.Get<NkSceneComponent>(childId)) s->MarkDirty();
        }

        inline void DetachSceneComponent(NkWorld& world, NkEntityId childId) noexcept {
            if (!world.IsAlive(childId)) return;

            // Retirer de la liste des enfants du parent actuel
            if (const auto* pComp = world.Get<NkParent>(childId)) {
                if (pComp->entity.IsValid()) {
                    if (auto* children = world.Get<NkChildren>(pComp->entity)) {
                        children->Remove(childId);
                    }
                }
            }
            world.Remove<NkParent>(childId);

            // Marquer comme dirty
            if (auto* t = world.Get<NkTransform>(childId)) t->MarkDirty();
            if (auto* s = world.Get<NkSceneComponent>(childId)) s->MarkDirty();
        }

        // ============================================================================
        // NkSceneTransformSystem — Propagation batchée des world transforms
        // ============================================================================
        // Ce système calcule les matrices world pour tous les NkSceneComponent
        // en une seule passe itérative (DFS avec stack explicite).
        // Il respecte l'ordre hiérarchique : parents avant enfants.
        // ============================================================================
        class NkSceneTransformSystem final : public NkSystem {
            public:
                NkSystemDesc Describe() const override {
                    return NkSystemDesc{}
                        .Reads<NkSceneComponent>()
                        .Reads<NkParent>()
                        .Reads<NkChildren>()
                        .Reads<NkTransform>()
                        .Writes<NkTransform>()
                        .InGroup(NkSystemGroup::PreUpdate)
                        .WithPriority(-50.f)
                        .Named("NkSceneTransformSystem");
                }

                void Execute(NkWorld& world, float32) override {
                    // Étape 1 : Mettre à jour les racines (entités sans NkParent ou parent invalide)
                    world.Query<NkSceneComponent, NkTransform>()
                        .Without<NkParent>()
                        .ForEach([&](NkEntityId id, NkSceneComponent& scene, NkTransform& transform) {
                            if (scene.dirty) {
                                UpdateEntity(world, id, scene, transform, NkMat4::Identity());
                            }
                        });

                    // Étape 2 : Propagation itérative aux enfants via BFS/DFS
                    PropagateToChildren(world);
                }

            private:
                // Met à jour le world transform d'une entité depuis son parent
                void UpdateEntity(NkWorld& world, NkEntityId id,
                                NkSceneComponent& scene, NkTransform& transform,
                                const NkMat4& parentWorld) noexcept {
                    // Calcul : world = parentWorld * local
                    const NkMat4 localMat = scene.GetLocalMatrix();
                    const NkMat4 worldMat = parentWorld * localMat;

                    // Mise à jour du NkTransform ECS (source de vérité pour le rendu)
                    transform.worldMatrix = worldMat;
                    transform.localMatrix = localMat;
                    transform.dirty = false;

                    // Extraction position world pour accès rapide
                    transform.position = {worldMat.m[12], worldMat.m[13], worldMat.m[14]};

                    scene.MarkClean();
                }

                // Propagation BFS aux enfants (évite la récursion)
                void PropagateToChildren(NkWorld& world) noexcept {
                    struct StackEntry {
                        NkEntityId id;
                        NkMat4 parentWorld;
                    };

                    NkVector<StackEntry> stack;
                    uint32 stackSize = 0;

                    // Initialisation : pousser toutes les racines déjà mises à jour
                    world.Query<NkSceneComponent, NkTransform, NkChildren>()
                        .ForEach([&](NkEntityId id, const NkSceneComponent& scene,
                                   const NkTransform& transform, const NkChildren& children) {
                            if (!scene.dirty && children.count > 0) {
                                if (stackSize < stack.size()) {
                                    stack[stackSize++] = {id, transform.worldMatrix};
                                }
                            }
                        });

                    // Traitement DFS itératif
                    while (stackSize > 0) {
                        const StackEntry entry = stack[--stackSize];
                        NkEntityId parentId = entry.id;
                        const NkMat4& parentWorld = entry.parentWorld;

                        // Parcours des enfants directs via NkChildren
                        if (const auto* children = world.Get<NkChildren>(parentId)) {
                            for (uint32 i = 0; i < children->count; ++i) {
                                NkEntityId childId = children->children[i];

                                // Vérifier si l'enfant a un SceneComponent et un Transform
                                auto* childScene = world.Get<NkSceneComponent>(childId);
                                auto* childTransform = world.Get<NkTransform>(childId);

                                if (childScene && childTransform && childScene->enabled) {
                                    // Mettre à jour l'enfant
                                    UpdateEntity(world, childId, *childScene, *childTransform, parentWorld);

                                    // Pousser l'enfant dans la stack s'il a des enfants
                                    if (childScene->hierarchyDirty) {
                                        if (const auto* childChildren = world.Get<NkChildren>(childId)) {
                                            if (childChildren->count > 0 && stackSize < stack.size()) {
                                                stack[stackSize++] = {childId, childTransform->worldMatrix};
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
        };

    } // namespace ecs
} // namespace nkentseu

// =============================================================================
// EXEMPLES D'UTILISATION DE NKSCENECOMPONENT.H
// =============================================================================

/*
// -----------------------------------------------------------------------------
// Exemple 1 : Création et manipulation d'un SceneComponent
// -----------------------------------------------------------------------------
void Exemple_SceneComponent(NkWorld& world) {
    // Créer une entité avec les composants requis
    NkEntityId id = world.CreateEntity();
    world.Add<NkTransform>(id);
    world.Add<NkSceneComponent>(id, NkSceneComponent("MyComponent"));

    // Accéder et modifier le SceneComponent
    auto* scene = world.Get<NkSceneComponent>(id);
    if (scene) {
        scene->SetLocalPosition({10.0f, 0.0f, 5.0f});
        scene->SetLocalRotation(NkQuat::FromAxisAngle({0, 1, 0}, 45.f * 3.14159f / 180.f));
        scene->SetLocalScale({2.0f, 2.0f, 2.0f});
    }

    // Le système NkSceneTransformSystem calculera automatiquement
    // la matrice world lors de la prochaine exécution.
}

// -----------------------------------------------------------------------------
// Exemple 2 : Gestion des sockets (points d'attache nommés)
// -----------------------------------------------------------------------------
void Exemple_Sockets(NkWorld& world) {
    NkEntityId weaponHolder = world.CreateEntity();
    world.Add<NkTransform>(weaponHolder);
    world.Add<NkSceneComponent>(weaponHolder, NkSceneComponent("WeaponSocket"));

    auto* socketComp = world.Get<NkSceneComponent>(weaponHolder);
    if (socketComp) {
        // Ajouter un socket "Muzzle" pour attacher une arme
        socketComp->AddSocket("Muzzle", {0.5f, 0.0f, 1.0f}, NkQuat::Identity(), NkVec3::One());

        // Vérifier l'existence d'un socket
        if (socketComp->HasSocket("Muzzle")) {
            const NkSocket* muzzle = socketComp->FindSocket("Muzzle");
            if (muzzle) {
                printf("Socket Muzzle trouvé à (%.2f, %.2f, %.2f)
",
                       muzzle->localPosition.x,
                       muzzle->localPosition.y,
                       muzzle->localPosition.z);
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Exemple 3 : Attachement hiérarchique via NkParent/NkChildren
// -----------------------------------------------------------------------------
void Exemple_Hierarchie(NkWorld& world) {
    // Créer le parent
    NkEntityId parent = world.CreateEntity();
    world.Add<NkTransform>(parent);
    world.Add<NkSceneComponent>(parent, NkSceneComponent("Parent"));

    // Créer l'enfant
    NkEntityId child = world.CreateEntity();
    world.Add<NkTransform>(child);
    world.Add<NkSceneComponent>(child, NkSceneComponent("Child"));

    // Attacher l'enfant au parent via les composants ECS
    AttachSceneComponent(world, child, parent);

    // Le système NkSceneTransformSystem propagera automatiquement
    // les transformations : worldChild = worldParent * localChild
}

// -----------------------------------------------------------------------------
// Exemple 4 : Utilisation avec NkGameObject (couche OOP)
// -----------------------------------------------------------------------------
void Exemple_WithGameObject(NkWorld& world) {
    // Créer un GameObject via la factory
    auto go = world.CreateGameObject("Player");

    // Accéder au SceneComponent via le GameObject
    auto* scene = go.GetComponent<NkSceneComponent>();
    if (scene) {
        scene->SetLocalPosition({0.0f, 1.8f, 0.0f}); // Position debout
        scene->AddSocket("Head", {0.0f, 1.6f, 0.0f}, NkQuat::Identity(), NkVec3::One());
    }

    // Attacher un enfant via l'API GameObject
    auto weapon = world.CreateGameObject("Sword");
    AttachSceneComponent(world, weapon.Id(), go.Id());  // Attache via NkParent/NkChildren ECS

    // Le système calculera automatiquement la position world de l'épée
    // en fonction de la position du joueur + offset local
}

// -----------------------------------------------------------------------------
// Exemple 5 : Dirty flags et propagation optimisée
// -----------------------------------------------------------------------------
void Exemple_DirtyPropagation(NkWorld& world) {
    // Créer une hiérarchie : Root -> Child -> GrandChild
    NkEntityId root = world.CreateEntity();
    NkEntityId child = world.CreateEntity();
    NkEntityId grandChild = world.CreateEntity();

    for (auto id : {root, child, grandChild}) {
        world.Add<NkTransform>(id);
        world.Add<NkSceneComponent>(id);
    }

    // Établir la hiérarchie ECS
    AttachSceneComponent(world, child, root);
    AttachSceneComponent(world, grandChild, child);

    // Modifier uniquement la racine
    auto* rootScene = world.Get<NkSceneComponent>(root);
    if (rootScene) {
        rootScene->SetLocalPosition({10.0f, 0.0f, 0.0f});
        // MarkDirty() est appelé automatiquement par SetLocalPosition
    }

    // Seul root est marqué dirty initialement.
    // Le système NkSceneTransformSystem propagera le recalcul
    // aux enfants uniquement si nécessaire (grâce à hierarchyDirty).
}

// -----------------------------------------------------------------------------
// Exemple 6 : Intégration avec le Scheduler
// -----------------------------------------------------------------------------
void Exemple_SchedulerIntegration(NkScheduler& scheduler, NkWorld& world) {
    // Ajouter le système de transformation hiérarchique
    // Il s'exécutera dans PreUpdate avec priorité haute
    scheduler.AddSystem<NkSceneTransformSystem>();

    // Dans votre boucle de jeu :
    // scheduler.Run(world, dt);  // Appelle automatiquement Execute()
    // Les worldMatrix seront à jour avant les systèmes de rendu/physique
}
*/