#pragma once
// =============================================================================
// NkRagdoll.h — Assemblage d'un corps articulé depuis une HIÉRARCHIE D'OS. [M9]
// GÉNÉRIQUE (aucune morphologie figée) : on décrit des os (parent, pose, forme,
// joint vers le parent) et le ragdoll crée les NkRigidBody + NkJoint correspondants
// dans le monde, avec self-collision désactivée (les os d'un même ragdoll ne se
// percutent pas). Humanoïde, quadrupède, créature, mécanique = juste des listes
// d'os différentes.
// =============================================================================
#include "NKPhysics/NkPhysicsWorld.h"

namespace nkentseu {
    namespace physics {

        // Description d'un os (un corps rigide + son joint vers le parent).
        struct NkBoneDef {
            int32             parent = -1;             // index de l'os parent (-1 = racine)
            NkVec3f           position{};              // pose du corps (centre de masse)
            NkQuatf           orientation{};
            collision::NkShape shape;                  // forme de collision (capsule/box…)
            NkPhysicsMaterial material{};
            uint32            flags = NK_BODY_NONE;
            // Joint reliant cet os à son parent (ignoré si racine) :
            NkJointType       jointType  = NkJointType::BALL;
            NkVec3f           jointPivot{};            // pivot monde (articulation)
            NkVec3f           jointAxis{ 0,0,1 };      // REVOLUTE : axe de charnière
            bool              limitEnabled = false;
            float32           lowerAngle = 0.f, upperAngle = 0.f;
        };

        class NkRagdoll {
            public:
                // Construit le ragdoll dans `world`. `group` = bit de layer dédié : les os
                // partagent ce bit et l'excluent de leur masque -> AUCUNE self-collision,
                // mais collision normale avec le reste du monde.
                void Build(NkPhysicsWorld& world, const NkBoneDef* bones, uint32 count, uint32 group = 0x2u) {
                    mBodies.Clear(); mJoints.Clear();
                    for (uint32 i = 0; i < count; ++i) {
                        const NkBoneDef& bd = bones[i];
                        NkBodyDef def; def.type = NkBodyType::DYNAMIC;
                        def.position = bd.position; def.orientation = bd.orientation;
                        def.material = bd.material; def.flags = bd.flags;
                        def.layer = group; def.mask = ~group;       // pas de self-collision
                        mBodies.PushBack(world.CreateBody(def, bd.shape));
                        mJoints.PushBack(NK_INVALID_JOINT);
                    }
                    for (uint32 i = 0; i < count; ++i) {
                        const NkBoneDef& bd = bones[i];
                        if (bd.parent < 0 || (uint32)bd.parent >= count) continue;
                        const NkBodyId p = mBodies[(uint32)bd.parent], c = mBodies[i];
                        NkJointId jid = NK_INVALID_JOINT;
                        switch (bd.jointType) {
                            case NkJointType::REVOLUTE:
                                jid = world.CreateRevoluteJoint(p, c, bd.jointPivot, bd.jointAxis);
                                if (bd.limitEnabled) world.SetRevoluteLimit(jid, bd.lowerAngle, bd.upperAngle);
                                break;
                            case NkJointType::WELD:     jid = world.CreateWeldJoint(p, c, bd.jointPivot); break;
                            case NkJointType::DISTANCE: jid = world.CreateDistanceJoint(p, c, bd.jointPivot, bd.jointPivot); break;
                            case NkJointType::BALL:
                            default:                    jid = world.CreateBallJoint(p, c, bd.jointPivot); break;
                        }
                        mJoints[i] = jid;
                    }
                }

                uint32    Count() const noexcept { return (uint32)mBodies.Size(); }
                NkBodyId  Body(uint32 i) const noexcept { return i < (uint32)mBodies.Size() ? mBodies[i] : NK_INVALID_BODY; }
                NkJointId Joint(uint32 i) const noexcept { return i < (uint32)mJoints.Size() ? mJoints[i] : NK_INVALID_JOINT; }

                // ── Boîte à outils de COUPLAGE (NkAnima ; tout corps articulé) ──
                // Lit la pose physique courante : position (COM) + orientation par os.
                // -> NkAnima s'en sert pour PILOTER LE SKIN (ragdoll passif drive le mesh).
                void ReadPose(const NkPhysicsWorld& w, NkVector<NkVec3f>& outPos, NkVector<NkQuatf>& outRot) const {
                    outPos.Clear(); outRot.Clear();
                    for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i) {
                        const NkRigidBody* b = w.GetBody(mBodies[i]);
                        outPos.PushBack(b ? b->position : NkVec3f{});
                        outRot.PushBack(b ? b->orientation : NkQuatf{});
                    }
                }
                // Active le RAGDOLL ACTIF : moteurs PD sur tous les joints revolute.
                void SetActive(NkPhysicsWorld& w, float32 kp = 25.f, float32 maxTorque = 500.f) {
                    for (uint32 i = 0; i < (uint32)mJoints.Size(); ++i)
                        if (mJoints[i] != NK_INVALID_JOINT) w.SetRevoluteMotor(mJoints[i], 0.f, kp, maxTorque);
                }
                // Pilote vers une pose : angle cible par os (depuis l'animation). NkAnima -> physique.
                void SetPoseTargets(NkPhysicsWorld& w, const float32* targetAngles, uint32 n) {
                    for (uint32 i = 0; i < (uint32)mJoints.Size() && i < n; ++i)
                        if (mJoints[i] != NK_INVALID_JOINT) w.SetRevoluteMotor(mJoints[i], targetAngles[i], 25.f, 500.f);
                }

            private:
                NkVector<NkBodyId>  mBodies;
                NkVector<NkJointId> mJoints;   // joint vers le parent (NK_INVALID_JOINT pour la racine)
        };

    } // namespace physics
} // namespace nkentseu
