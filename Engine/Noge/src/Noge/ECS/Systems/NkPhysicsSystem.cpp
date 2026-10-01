// =============================================================================
// Noge/ECS/Systems/NkPhysicsSystem.cpp — pont ECS -> NKPhysics (rigid-body 3D)
// =============================================================================
#include "NkPhysicsSystem.h"

namespace nkentseu {

	using namespace ecs;
	using namespace physics;
	using namespace collision;
	using namespace math;

	// -------------------------------------------------------------------------
	// NkCollider3D -> forme de collision EN REPÈRE MONDE.
	//
	// `col.center` est un offset LOCAL au corps ; `CreateBody` attend la forme
	// DÉJÀ PLACÉE. On compose donc : monde = position + rotation * offset, et la
	// boîte prend l'orientation de l'entité. (Voir l'en-tête : le décalage a été
	// mesuré, il valait exactement la hauteur de départ du corps.)
	// -------------------------------------------------------------------------
	NkShape NkPhysicsSystem::MakeShape(const NkCollider3D &col, const NkTransform &tf) noexcept {
		const NkQuatf q = tf.localRotation;
		const NkVec3f c = tf.localPosition + q * col.center;
		switch (col.shape) {
			case NkCollider3DShape::Sphere:
				return NkShape::Sphere(c, col.sphereRadius);

			case NkCollider3DShape::Capsule: {
				const float32 h = col.capsuleHeight * 0.5f; // demi-hauteur du segment
				const NkVec3f axisL = (col.capsuleDir == NkCapsuleDirection3D::X)  ? NkVec3f{h, 0.f, 0.f}
									 : (col.capsuleDir == NkCapsuleDirection3D::Z) ? NkVec3f{0.f, 0.f, h}
																				   : NkVec3f{0.f, h, 0.f};
				// L'axe TOURNE avec l'entité : une capsule couchée sur un corps
				// incliné doit être couchée dans le MONDE, pas dans son repère.
				const NkVec3f axis = q * axisL;
				return NkShape::Capsule3D(c - axis, c + axis, col.capsuleRadius);
			}

			case NkCollider3DShape::Box:
			default: {
				// boxSize = dimensions PLEINES -> demi-extents pour la forme.
				NkShape s = NkShape::Box3D(c, col.boxSize * 0.5f);
				// Sans ceci la boîte reste alignée sur les axes du monde : un corps
				// incliné aurait une forme droite, et la pente n'existerait pas.
				s.orientation = q;
				return s;
			}
		}
	}

	// -------------------------------------------------------------------------
	// Crée le corps physique d'une entité depuis ses composants.
	// -------------------------------------------------------------------------
	NkBodyId NkPhysicsSystem::CreateBodyFor(const NkRigidbody3D &rb, const NkCollider3D &col,
											const NkTransform &tf) noexcept {
		NkBodyDef def;
		def.type = ConvertBodyType(rb.bodyType);
		def.position = tf.localPosition;
		def.orientation = tf.localRotation;
		def.linearVelocity = rb.velocity;
		def.angularVelocity = rb.angularVelocity;
		def.gravityScale = rb.gravityScale;
		def.linearDamping = rb.drag;
		def.angularDamping = rb.angularDrag;

		// Matériau : friction/restitution du rigidbody (sinon celles du collider).
		def.material.staticFriction = rb.friction;
		def.material.dynamicFriction = rb.friction;
		def.material.restitution = rb.restitution;

		// Filtrage de collision (layer/mask). layer ECS (index) -> bit.
		def.layer = (col.layer < 32u) ? (1u << col.layer) : 0x1u;
		def.mask = col.layerMask;

		// Rotation gelee sur les TROIS axes -> le drapeau du socle (inertie
		// inverse nulle, NkComputeMassProps). Un gel partiel n'est pas tenu.
		if (rb.freezeRotX && rb.freezeRotY && rb.freezeRotZ) {
			def.flags |= NK_BODY_FIXED_ROT;
		}

		++mBodiesCreated;
		return mWorld.CreateBody(def, MakeShape(col, tf));
	}

	// -------------------------------------------------------------------------
	// Ce que le JEU a ecrit dans le composant depuis le dernier pas.
	//
	// Le pont recopie `body.linearVelocity` dans `rb.velocity` apres chaque pas :
	// tant que personne n'y touche, les deux sont egaux au bit pres. S'ils
	// different, c'est le jeu (AddImpulse, SetVelocity, Stop, ou une ecriture
	// directe) : sa valeur passe au corps. Meme regle pour l'angulaire. Les
	// forces et couples accumules passent et sont remis a zero (l'integrateur
	// vide ceux du corps a chaque pas).
	// -------------------------------------------------------------------------
	void NkPhysicsSystem::PushGameplayToBody(NkRigidbody3D &rb, NkRigidBody &body) noexcept {
		bool touche = false;
		const NkVec3f dv = rb.velocity - body.linearVelocity;
		if (dv.x * dv.x + dv.y * dv.y + dv.z * dv.z > 1e-12f) {
			body.linearVelocity = rb.velocity;
			touche = true;
		}
		const NkVec3f dw = rb.angularVelocity - body.angularVelocity;
		if (dw.x * dw.x + dw.y * dw.y + dw.z * dw.z > 1e-12f) {
			body.angularVelocity = rb.angularVelocity;
			touche = true;
		}
		if (rb.force.x != 0.f || rb.force.y != 0.f || rb.force.z != 0.f) {
			body.ApplyForce(rb.force);
			rb.force = {};
			touche = true;
		}
		if (rb.torque.x != 0.f || rb.torque.y != 0.f || rb.torque.z != 0.f) {
			body.torque = body.torque + rb.torque;
			rb.torque = {};
			touche = true;
		}
		// Un corps endormi est hors du solveur : le jeu qui le pousse le reveille.
		if (touche) {
			body.flags &= ~static_cast<uint32>(NK_BODY_SLEEPING);
			body.sleepTimer = 0.f;
		}
	}

	// -------------------------------------------------------------------------
	// ReleaseBodies — rend les corps d'une scene avant qu'on la detruise.
	// -------------------------------------------------------------------------
	uint32 NkPhysicsSystem::ReleaseBodies(ecs::NkWorld &world) noexcept {
		uint32 n = 0;
		world.Query<NkCollider3D>().ForEach([&](NkEntityId, NkCollider3D &col) {
			if (col.physicsBodyId != 0) {
				mWorld.DestroyBody(static_cast<NkBodyId>(col.physicsBodyId));
				col.physicsBodyId = 0;
				++n;
			}
		});
		return n;
	}

	// -------------------------------------------------------------------------
	// Execute — pas fixe : crée les corps, avance la simulation, synchronise.
	// -------------------------------------------------------------------------
	void NkPhysicsSystem::Execute(ecs::NkWorld &world, float32 fixedDt) noexcept {
		// 1. Création des corps manquants + push des cinématiques (transform -> corps).
		world.Query<NkRigidbody3D, NkCollider3D, NkTransform>().ForEach(
			[&](NkEntityId, NkRigidbody3D &rb, NkCollider3D &col, NkTransform &tf) {
				if (!col.enabled)
					return;

				if (col.physicsBodyId == 0) {
					col.physicsBodyId = static_cast<uint64>(CreateBodyFor(rb, col, tf));
					return;
				}

				// Corps cinématique : piloté par le transform -> on pousse sa pose.
				if (rb.bodyType == ecs::NkBodyType::Kinematic) {
					if (NkRigidBody *body = mWorld.GetBody(static_cast<NkBodyId>(col.physicsBodyId))) {
						body->position = tf.localPosition;
						body->orientation = tf.localRotation;
					}
				} else if (rb.bodyType == ecs::NkBodyType::Dynamic) {
					// Corps dynamique : ce que le jeu a ecrit dans le composant.
					if (NkRigidBody *body = mWorld.GetBody(static_cast<NkBodyId>(col.physicsBodyId))) {
						PushGameplayToBody(rb, *body);
					}
				}
			});

		// 2. Avance la simulation (collision + dynamique) d'un pas fixe.
		mWorld.Step(fixedDt);

		// 3. Synchronisation corps -> transform pour les corps DYNAMIQUES.
		world.Query<NkRigidbody3D, NkCollider3D, NkTransform>().ForEach(
			[&](NkEntityId, NkRigidbody3D &rb, const NkCollider3D &col, NkTransform &tf) {
				if (col.physicsBodyId == 0 || rb.bodyType != ecs::NkBodyType::Dynamic)
					return;

				const NkRigidBody *body = mWorld.GetBody(static_cast<NkBodyId>(col.physicsBodyId));
				if (!body)
					return;

				tf.localPosition = body->position;
				tf.localRotation = body->orientation;
				tf.worldDirty = true;
				rb.velocity = body->linearVelocity;
				rb.angularVelocity = body->angularVelocity;
			});
	}

} // namespace nkentseu
