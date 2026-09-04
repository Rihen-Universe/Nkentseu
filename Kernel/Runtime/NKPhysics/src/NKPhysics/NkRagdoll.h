#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
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

		// ── UNE SEULE DEFINITION DE SQUELETTE (2026-09-04) ─────────────────────
		// Il y avait ici un troisieme `NkBoneDef` (parent + corps rigide + joint) :
		// un second squelette qui REDISAIT `parent`, et c'est exactement ce qui
		// diverge un jour. Rodolf : « j'espere que ce n'est pas un chantier
		// duplique » -- c'en etait un, a cet endroit precis.
		// Desormais le ragdoll se construit depuis une VUE du squelette du moteur
		// (les parents et la pose de repos MONDE, en tableaux bruts : NKPhysics
		// n'inclut pas NKAnima, et la vue n'est pas une copie) plus une table
		// d'ATTRIBUTS PHYSIQUES par os. Un os = le squelette + ses attributs.
		struct NkSkeletonView {
				const int32 *parent = nullptr;	   // index du parent (-1 = racine)
				const NkVec3f *restPos = nullptr;  // pose de repos MONDE du joint
				const NkQuatf *restRot = nullptr;  // orientation de repos MONDE (nullptr = identite)
				uint32 count = 0;
		};
		// Ce qu'un os a de PHYSIQUE, et rien de topologique.
		struct NkRagdollBoneAttr {
				// DYNAMIC par defaut ; STATIC/KINEMATIC pour EPINGLER un os (une racine
				// tenue). C'est CreateBody qui en deduit la masse inverse : changer le
				// type apres coup laisse invMass = 1 et le solveur pousse un mur.
				NkBodyType type = NkBodyType::DYNAMIC;
				NkVec3f comOffset{};		  // centre de masse, relatif au joint (repere monde au repos)
				collision::NkShape shape;	  // forme de collision (capsule/box…), repere MONDE au repos
				NkPhysicsMaterial material{};
				uint32 flags = NK_BODY_NONE;
				// Joint reliant cet os a son parent (ignore si racine) :
				NkJointType jointType = NkJointType::BALL;
				NkVec3f jointPivot{};		// pivot monde (articulation)
				NkVec3f jointAxis{0, 0, 1}; // REVOLUTE : axe de charniere
				bool limitEnabled = false;
				float32 lowerAngle = 0.f, upperAngle = 0.f;
		};

		class NkRagdoll {
			public:
				// Construit le ragdoll dans `world`. `group` = bit de layer dédié : les os
				// partagent ce bit et l'excluent de leur masque -> AUCUNE self-collision,
				// mais collision normale avec le reste du monde.
				void Build(NkPhysicsWorld &world, const NkSkeletonView &skel, const NkRagdollBoneAttr *attrs,
						   uint32 group = 0x2u) {
					mBodies.Clear();
					mJoints.Clear();
					const uint32 count = skel.count;
					for (uint32 i = 0; i < count; ++i) {
						const NkRagdollBoneAttr &bd = attrs[i];
						NkBodyDef def;
						def.type = bd.type;
						def.position = skel.restPos[i] + bd.comOffset; // la pose vient du SQUELETTE
						def.orientation = skel.restRot ? skel.restRot[i] : NkQuatf::Identity();
						def.material = bd.material;
						def.flags = bd.flags;
						def.layer = group;
						def.mask = ~group; // pas de self-collision
						mBodies.PushBack(world.CreateBody(def, bd.shape));
						mJoints.PushBack(NK_INVALID_JOINT);
					}
					for (uint32 i = 0; i < count; ++i) {
						const NkRagdollBoneAttr &bd = attrs[i];
						const int32 parent = skel.parent[i]; // la topologie vient du SQUELETTE
						if (parent < 0 || (uint32)parent >= count)
							continue;
						const NkBodyId p = mBodies[(uint32)parent], c = mBodies[i];
						NkJointId jid = NK_INVALID_JOINT;
						switch (bd.jointType) {
							case NkJointType::REVOLUTE:
								jid = world.CreateRevoluteJoint(p, c, bd.jointPivot, bd.jointAxis);
								if (bd.limitEnabled)
									world.SetRevoluteLimit(jid, bd.lowerAngle, bd.upperAngle);
								break;
							case NkJointType::WELD:
								jid = world.CreateWeldJoint(p, c, bd.jointPivot);
								break;
							case NkJointType::DISTANCE:
								jid = world.CreateDistanceJoint(p, c, bd.jointPivot, bd.jointPivot);
								break;
							case NkJointType::BALL:
							default:
								jid = world.CreateBallJoint(p, c, bd.jointPivot);
								break;
						}
						mJoints[i] = jid;
					}
				}

				uint32 Count() const noexcept {
					return (uint32)mBodies.Size();
				}

				NkBodyId Body(uint32 i) const noexcept {
					return i < (uint32)mBodies.Size() ? mBodies[i] : NK_INVALID_BODY;
				}

				NkJointId Joint(uint32 i) const noexcept {
					return i < (uint32)mJoints.Size() ? mJoints[i] : NK_INVALID_JOINT;
				}

				// ── Boîte à outils de COUPLAGE (NkAnima ; tout corps articulé) ──
				// Lit la pose physique courante : position (COM) + orientation par os.
				// -> NkAnima s'en sert pour PILOTER LE SKIN (ragdoll passif drive le mesh).
				void ReadPose(const NkPhysicsWorld &w, NkVector<NkVec3f> &outPos, NkVector<NkQuatf> &outRot) const {
					outPos.Clear();
					outRot.Clear();
					for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i) {
						const NkRigidBody *b = w.GetBody(mBodies[i]);
						outPos.PushBack(b ? b->position : NkVec3f{});
						outRot.PushBack(b ? b->orientation : NkQuatf{});
					}
				}

				// Active le RAGDOLL ACTIF : moteurs PD sur tous les joints revolute.
				void SetActive(NkPhysicsWorld &w, float32 kp = 25.f, float32 maxTorque = 500.f) {
					for (uint32 i = 0; i < (uint32)mJoints.Size(); ++i)
						if (mJoints[i] != NK_INVALID_JOINT)
							w.SetRevoluteMotor(mJoints[i], 0.f, kp, maxTorque);
				}

				// Pilote vers une pose : angle cible par os (depuis l'animation). NkAnima -> physique.
				void SetPoseTargets(NkPhysicsWorld &w, const float32 *targetAngles, uint32 n) {
					for (uint32 i = 0; i < (uint32)mJoints.Size() && i < n; ++i)
						if (mJoints[i] != NK_INVALID_JOINT)
							w.SetRevoluteMotor(mJoints[i], targetAngles[i], 25.f, 500.f);
				}

			private:
				NkVector<NkBodyId> mBodies;
				NkVector<NkJointId> mJoints; // joint vers le parent (NK_INVALID_JOINT pour la racine)
		};

	} // namespace physics
} // namespace nkentseu
