#pragma once
// =============================================================================
// NkRagdollBridge.h — Pont RAGDOLL <-> SQUELETTE skinné (NkAnima). [couplage M*]
// Construit un NkRagdoll (NKPhysics) depuis la hiérarchie de joints d'un perso
// glTF (bindGlobal[] + jointParent[]) et synchronise la pose PHYSIQUE vers les
// matrices monde par joint (worldEdit[]) -> le mesh skinné suit le ragdoll.
//
//   Passif  : Step(dt) puis SyncToSkeleton(worldEdit) -> le perso s'effondre/réagit.
//   (Actif/pose-driven : variante revolute + SetPoseTargets, étape suivante.)
//
// Générique (toute morphologie : humanoïde, animal, créature) — rien de figé.
// =============================================================================
#include "NKPhysics/NKPhysics.h"
#include "NKMath/NkMat.h"

namespace nkanima {

    using nkentseu::int32;
    using nkentseu::uint32;
    using nkentseu::float32;
    using NkMat4f = nkentseu::math::NkMat4f;
    using NkQuatf = nkentseu::math::NkQuatf;
    using NkVec3f = nkentseu::math::NkVec3f;
    namespace physics   = nkentseu::physics;
    namespace collision = nkentseu::collision;

    class NkRagdollBridge {
        public:
            // Construit le ragdoll : 1 corps par joint (capsule joint->parent), joints
            // ball au pivot de chaque joint ; la racine (parent<0) est KINEMATIC (ancre).
            void Build(physics::NkPhysicsWorld& world,
                       const nkentseu::NkVector<NkMat4f>& bindGlobal,
                       const nkentseu::NkVector<int32>& jointParent,
                       float32 boneRadius = 0.04f) {
                mWorld = &world; mBodies.Clear(); mJoints.Clear();
                const uint32 n = (uint32)bindGlobal.Size();
                for (uint32 j = 0; j < n; ++j) {
                    const NkMat4f& M = bindGlobal[j];
                    const NkVec3f pos{ M.m30, M.m31, M.m32 };
                    physics::NkBodyDef def;
                    def.position = pos; def.orientation = NkQuatf(M);
                    def.layer = 0x2u; def.mask = ~0x2u;                 // pas de self-collision
                    const int32 p = (j < (uint32)jointParent.Size()) ? jointParent[j] : -1;
                    collision::NkShape shape;
                    if (p >= 0 && (uint32)p < n) {
                        const NkMat4f& MP = bindGlobal[(uint32)p];
                        shape = collision::NkShape::Capsule3D(pos, NkVec3f{ MP.m30, MP.m31, MP.m32 }, boneRadius);
                    } else {
                        def.type = physics::NkBodyType::KINEMATIC;       // racine ancrée
                        shape = collision::NkShape::Sphere(pos, boneRadius * 1.5f);
                    }
                    mBodies.PushBack(world.CreateBody(def, shape));
                }
                mJoints.Resize(n);
                for (uint32 j = 0; j < n; ++j) mJoints[j] = physics::NK_INVALID_JOINT;
                for (uint32 j = 0; j < n; ++j) {
                    const int32 p = (j < (uint32)jointParent.Size()) ? jointParent[j] : -1;
                    if (p < 0 || (uint32)p >= n) continue;
                    const NkMat4f& M = bindGlobal[j];
                    mJoints[j] = world.CreateBallJoint(mBodies[(uint32)p], mBodies[j], NkVec3f{ M.m30, M.m31, M.m32 });
                }
            }

            // Physique -> squelette : worldEdit[j] = pose du corps j (le skin suivra).
            void SyncToSkeleton(nkentseu::NkVector<NkMat4f>& worldEdit) const {
                if (!mWorld) return;
                const uint32 n = (uint32)mBodies.Size();
                for (uint32 j = 0; j < n && j < (uint32)worldEdit.Size(); ++j) {
                    const physics::NkRigidBody* b = mWorld->GetBody(mBodies[j]);
                    if (!b) continue;
                    NkMat4f m = (NkMat4f)b->orientation;
                    m.m30 = b->position.x; m.m31 = b->position.y; m.m32 = b->position.z;
                    worldEdit[j] = m;
                }
            }

            // Pilote la racine (KINEMATIC) -> permet de déplacer le ragdoll à la main / à l'anim.
            void SetRootTarget(const NkVec3f& pos) { if (mWorld && mBodies.Size() > 0) if (auto* b = mWorld->GetBody(mBodies[0])) b->position = pos; }

            bool   Built() const noexcept { return mWorld != nullptr && mBodies.Size() > 0; }
            uint32 Count() const noexcept { return (uint32)mBodies.Size(); }
            void   Clear() { mBodies.Clear(); mJoints.Clear(); mWorld = nullptr; }

        private:
            physics::NkPhysicsWorld*           mWorld = nullptr;
            nkentseu::NkVector<physics::NkBodyId>  mBodies;
            nkentseu::NkVector<physics::NkJointId> mJoints;
    };

} // namespace nkanima
