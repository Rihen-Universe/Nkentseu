#pragma once
// =============================================================================
// Noge/ECS/Systems/NkPhysicsSystem.h — pont ECS -> NKPhysics (rigid-body 3D)
// =============================================================================
// Modèle identique à NkRenderSystem (pont ECS -> NKRenderer) :
//   - POSSÈDE un physics::NkPhysicsWorld (NKCollision + NKPhysics).
//   - Pour chaque entité (NkRigidbody3D + NkCollider3D + NkTransform) sans corps,
//     crée un NkRigidBody (NkBodyDef + NkShape) et stocke l'id (collider.physicsBodyId).
//   - Avance la simulation à pas fixe (groupe FixedUpdate), puis SYNCHRONISE :
//       • dynamique : corps -> NkTransform (+ vélocités vers le composant)
//       • cinématique : NkTransform -> corps (avant le step)
//
// LE GAMEPLAY PARLE AU CORPS PAR LE COMPOSANT (2026-09-30) :
//   NkRigidbody3D::AddForce / AddImpulse / AddTorque / SetVelocity / Stop
//   existaient et n'avaient AUCUN effet : le pont ecrasait `velocity` apres
//   chaque pas et ignorait `force` / `torque`. Desormais, avant le pas :
//       • une `velocity` (ou `angularVelocity`) qui n'est plus celle que le pont
//         a recopiee du corps au pas d'avant a ete ecrite par le jeu : elle
//         PASSE au corps (le jeu gagne) ;
//       • `force` et `torque` accumules passent au corps, puis sont remis a 0 ;
//       • le corps touche est reveille.
//   freezeRotX && freezeRotY && freezeRotZ -> NK_BODY_FIXED_ROT (inertie
//   inverse nulle : le corps glisse sans basculer). Un gel PARTIEL n'est pas
//   tenu (il le serait sur des axes locaux, pas monde) ; `freezePos*` et
//   `mass` ne sont pas tenus non plus (la masse vient de la densite du
//   materiau) -- voir Engine/Noge/ETAT_2026-09-30.md.
// =============================================================================

#include "NKECS/System/NkSystem.h"
#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Physics/NkPhysics.h" // NkRigidbody3D, NkCollider3D, NkBodyType
#include "NKPhysics/NkPhysicsWorld.h"
#include "NKPhysics/NkRigidBody.h"
#include "NKCollision/NkColShapes.h"

namespace nkentseu {

	using namespace math;

	class NkPhysicsSystem final : public ecs::NkSystem {
		public:
			NkPhysicsSystem() noexcept = default;

			[[nodiscard]] ecs::NkSystemDesc Describe() const override {
				return ecs::NkSystemDesc{}
					.Writes<ecs::NkTransform>()
					.Writes<ecs::NkRigidbody3D>()
					.Writes<ecs::NkCollider3D>()
					.InGroup(ecs::NkSystemGroup::FixedUpdate)
					.Sequential()
					.Named("NkPhysicsSystem");
			}

			void Execute(ecs::NkWorld &world, float32 fixedDt) noexcept override;

			// Accès direct au monde physique (raycast, gravité, requêtes...).
			[[nodiscard]] physics::NkPhysicsWorld &World() noexcept {
				return mWorld;
			}

			[[nodiscard]] const physics::NkPhysicsWorld &World() const noexcept {
				return mWorld;
			}

			/// Rend au monde physique TOUS les corps que portent les
			/// collisionneurs de `world` et remet leur `physicsBodyId` a 0 (le
			/// prochain pas les recree). A appeler AVANT de detruire les entites
			/// d'une scene : sinon leurs corps restent dans le monde physique,
			/// sans entite, et continuent de heurter ce qui arrive ensuite.
			/// Rend le nombre de corps rendus.
			uint32 ReleaseBodies(ecs::NkWorld &world) noexcept;

			/// Nombre de corps crees depuis le debut (diagnostic, bancs).
			[[nodiscard]] uint32 BodiesCreated() const noexcept {
				return mBodiesCreated;
			}

		private:
			// Crée le corps physique d'une entité depuis ses composants.
			physics::NkBodyId CreateBodyFor(const ecs::NkRigidbody3D &rb, const ecs::NkCollider3D &col,
											const ecs::NkTransform &tf) noexcept;

			// NkCollider3D -> forme de collision (box/sphère/capsule), EN REPÈRE
			// MONDE : déjà placée à la pose de l'entité.
			//
			// ⚠️ LE TRANSFORM EST UN PARAMÈTRE, ET C'EST TOUT LE POINT.
			//    `CreateBody` attend une forme DÉJÀ PLACÉE à `def.position` : il
			//    en déduit la forme de repos par l'inverse de la pose. Une forme
			//    centrée sur `col.center` seul (un offset LOCAL) donnait une forme
			//    de repos décalée de -position, et la forme de collision restait
			//    collée à l'origine du monde pendant que le corps tombait.
			//
			//    Mesuré le 2026-09-26 (`NkDemoContact --mesure`) : corps à 6,995 m
			//    et sa forme à 0,995 m — un écart constant, exactement la hauteur
			//    de départ. La simulation, elle, était JUSTE : c'est la forme qui
			//    s'était posée sur le sol, six mètres sous son propre corps.
			//
			//    Même contrat que `NkUnkenyScene::AjouterCorps`, qui passait déjà
			//    `t->position` : ce pont-ci était le seul à ne pas le faire.
			[[nodiscard]] static collision::NkShape MakeShape(const ecs::NkCollider3D &col,
														  const ecs::NkTransform &tf) noexcept;

			// Enum Noge -> enum NKPhysics.
			[[nodiscard]] static physics::NkBodyType ConvertBodyType(ecs::NkBodyType t) noexcept {
				switch (t) {
					case ecs::NkBodyType::Static:
						return physics::NkBodyType::STATIC;
					case ecs::NkBodyType::Kinematic:
						return physics::NkBodyType::KINEMATIC;
					default:
						return physics::NkBodyType::DYNAMIC;
				}
			}

			// Le jeu a-t-il ecrit dans le composant depuis le dernier pas ? Pousse
			// vitesses / forces vers le corps (voir l'en-tete).
			static void PushGameplayToBody(ecs::NkRigidbody3D &rb, physics::NkRigidBody &body) noexcept;

			physics::NkPhysicsWorld mWorld; // monde physique POSSÉDÉ (gravité par défaut)
			uint32 mBodiesCreated = 0;
	};

} // namespace nkentseu
