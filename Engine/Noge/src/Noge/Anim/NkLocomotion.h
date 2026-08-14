#pragma once
// =============================================================================
// Nkentseu/Anim3D/NkLocomotion.h
// =============================================================================
// Système de locomotion avancé pour personnages bipèdes et quadrupèdes.
//
// COMPOSANTS :
//   NkFootIK       — correction IK des pieds sur terrain irrégulier
//   NkLocomotion   — paramètres de mouvement (vitesse, direction, stance)
//   NkMotionMatch  — Motion Matching (sélection de frames par ML)
//   NkCrowdAgent   — agent de foule avec navigation + animation procédurale
//
// FOOT IK (Inverse Kinematics de Pied) :
//   Problème : les animations sont créées sur terrain plat.
//   Solution : NkFootIK corrige la position des pieds en temps réel
//   en projetant les pieds sur la géométrie du terrain (raycast).
//
//   Pipeline :
//   1. Raycast depuis chaque pied vers le bas (NkPhysicsSystem)
//   2. Si le pied est au-dessus du sol → baisser le pied (IK)
//   3. Rotation de la cheville selon la normale du sol
//   4. Ajuster la hanche pour compenser la jambe raccourcie
//
// MOTION MATCHING :
//   Au lieu d'une state machine d'animation, Motion Matching cherche
//   dans une base de données de frames d'animation la frame qui
//   correspond le mieux à l'état courant du personnage (position, vélocité,
//   trajectoire désirée). C'est la technique d'Ubisoft (For Honor, etc.)
//
// [RÉVISION 2026-07-23] — Pont animation (Engine/Noge/ROADMAP.md, item G1.4) :
//   • NkFootIKSystem délègue désormais la résolution jambe (hanche→genou→
//     pied) à NkIKSolver::SolveTwoBone (lui-même adaptateur fin sur
//     renderer::NkIKSystem::NK_TWO_BONE, voir Rigging/NkIKSolver.h) — zéro
//     réimplémentation FABRIK/TwoBone locale. Le raycast sol délègue à
//     physics::NkPhysicsWorld::Raycast (NKPhysics, déjà réel) quand un monde
//     physique est fourni (SetPhysicsWorld) ; sinon un plan de sol plat
//     (fallback CPU-only honnête, PAS une réimplémentation de raycast contre
//     un mesh de terrain) sert de repli pour les démos sans monde physique.
//   • NkLocomotionSystem possède un anim::NkBlendTree1D (ConfigureBlend)
//     qui mélange RÉELLEMENT deux clips (ex. Walk/Run) via
//     NkBlendTree1D::SetParameter/Update — zéro réimplémentation de blend.
//     Limitation assumée : le blend écrit une pose NON hiérarchique dans
//     ecs::NkSkeleton (chaque os = sa propre transform monde, DecomposeTRS
//     direct) ; cohérent pour un squelette PLAT (bones[i].parent == -1, cf.
//     démo Applications/NkLocomotionDemo) mais pas pour un squelette
//     hiérarchique complet (aucun re-FK des enfants) — voir NkLocomotion.cpp.
//   • NkMotionMatchSystem / NkCrowdSystem (Motion Matching, foule) restent
//     hors-scope de cette passe : specs déclarées, 0 .cpp, mais NON
//     instanciées ailleurs dans le moteur (confirmé par grep) — donc pas de
//     régression. Motion Matching et crowd steering sont des chantiers à part
//     entière, indépendants de la délégation IK/blend demandée par G1.4.
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKECS/System/NkSystem.h"
#include "NKECS/World/NkWorld.h"
#include "NKMath/NKMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "Noge/ECS/Components/Animation/NkAnimation.h"
#include "Noge/Rigging/NkIKSolver.h"
#include "NKAnimation/NkAnimation.h"
#include "NKPhysics/NkPhysicsWorld.h"
#include "NKCollision/NkColTypes.h"

namespace nkentseu {
	using namespace math;
	using namespace ecs;

	// =========================================================================
	// NkFootContact — contact du pied avec le sol
	// =========================================================================
	struct NkFootContact {
			NkVec3f groundPos = {};			  ///< Position de contact au sol
			NkVec3f groundNormal = {0, 1, 0}; ///< Normale du sol au contact
			float32 groundDist = 0.f;		  ///< Distance pied → sol
			bool isGrounded = false;
			float32 contactWeight = 0.f; ///< Poids du contact [0..1] (lissé)
	};

	// =========================================================================
	// NkFootIK — correction IK des pieds sur terrain irrégulier
	// =========================================================================
	struct NkFootIK {
			// ── Os du squelette ──────────────────────────────────────────────
			uint32 hipBoneIdx = 0;	 ///< Hanche
			uint32 leftThighIdx = 1; ///< Cuisse gauche
			uint32 leftCalfIdx = 2;	 ///< Mollet gauche
			uint32 leftFootIdx = 3;	 ///< Pied gauche
			uint32 leftToeIdx = 4;	 ///< Orteil gauche
			uint32 rightThighIdx = 5;
			uint32 rightCalfIdx = 6;
			uint32 rightFootIdx = 7;
			uint32 rightToeIdx = 8;

			// ── Paramètres ───────────────────────────────────────────────────
			float32 footHeight = 0.04f;		///< Hauteur du pied au-dessus du sol (m)
			float32 ikWeight = 1.0f;		///< Influence globale [0..1]
			float32 hipCompensation = 0.5f; ///< Compensation de la hanche [0..1]
			float32 maxFootAngle = 45.f;	///< Angle max d'inclinaison du pied (°)
			float32 rayLength = 1.5f;		///< Longueur du raycast vers le bas (m)
			float32 blendSpeed = 10.f;		///< Vitesse de lissage du contact

			// ── Layers de physique pour le raycast ────────────────────────────
			uint32 raycastLayerMask = 0xFFFFFFFF; ///< Layers touchant le terrain

			// ── État (calculé par NkFootIKSystem) ─────────────────────────────
			NkFootContact leftFoot;
			NkFootContact rightFoot;
			float32 hipOffset = 0.f; ///< Décalage vertical de la hanche

			bool enabled = true;
	};
	NK_COMPONENT(NkFootIK)

	// =========================================================================
	// NkLocomotion — état de mouvement du personnage
	// =========================================================================
	struct NkLocomotion {
			// ── Input de mouvement ───────────────────────────────────────────
			NkVec3f desiredVelocity = {};  ///< Vélocité désirée (depuis input)
			float32 desiredSpeed = 0.f;	   ///< Vitesse désirée
			float32 desiredTurnRate = 0.f; ///< Vitesse de rotation (rad/s)
			NkVec3f trajectoryDir[4] = {}; ///< Points de trajectoire prédite

			// ── État courant ─────────────────────────────────────────────────
			NkVec3f velocity = {}; ///< Vélocité actuelle (lissée)
			float32 speed = 0.f;
			NkVec3f facing = {0, 0, 1};
			bool isGrounded = true;
			bool isSprinting = false;
			bool isCrouching = false;
			bool isAiming = false;

			// ── Stance ────────────────────────────────────────────────────────
			enum class Stance : uint8 { Stand, Crouch, Prone, Swim, Climb };
			Stance stance = Stance::Stand;

			// ── Paramètres ───────────────────────────────────────────────────
			float32 walkSpeed = 1.5f; ///< m/s
			float32 runSpeed = 5.5f;
			float32 sprintSpeed = 10.f;
			float32 accelerationTime = 0.2f; ///< 0→max speed en X secondes
			float32 decelerationTime = 0.1f;
			float32 turnSpeed = 360.f; ///< °/s

			// ── Locomotion banks (variation selon vitesse/direction) ──────────
			struct SpeedEntry {
					float32 speed;
					nk_uint64 animClipHandle; ///< Clip pour cette vitesse
			};

			static constexpr uint32 kMaxSpeedEntries = 8u;
			SpeedEntry speedEntries[kMaxSpeedEntries] = {};
			uint32 speedEntryCount = 0;

			bool AddSpeedEntry(float32 spd, nk_uint64 clipHandle) noexcept {
				if (speedEntryCount >= kMaxSpeedEntries)
					return false;
				speedEntries[speedEntryCount++] = {spd, clipHandle};
				return true;
			}
	};
	NK_COMPONENT(NkLocomotion)

	// =========================================================================
	// NkMotionDatabase — base de données pour Motion Matching
	// =========================================================================
	/**
	 * @struct NkMotionFeature
	 * @brief Vecteur de features qui décrit un instant dans la base de données.
	 *
	 * Le Motion Matching compare ce vecteur au vecteur de query courante
	 * (position, vélocité, trajectoire désirée) via une distance euclidienne.
	 */
	struct NkMotionFeature {
			// Position et vélocité des os clés (pieds, mains, hanche)
			NkVec3f leftFootPos = {};
			NkVec3f leftFootVel = {};
			NkVec3f rightFootPos = {};
			NkVec3f rightFootVel = {};
			NkVec3f hipPos = {};
			NkVec3f hipVel = {};

			// Trajectoire prédite (3 points dans le futur)
			NkVec3f trajPos0 = {}; ///< Position dans 0.2s
			NkVec3f trajPos1 = {}; ///< Position dans 0.4s
			NkVec3f trajPos2 = {}; ///< Position dans 0.6s
			NkVec3f trajDir0 = {}; ///< Direction dans 0.2s
			NkVec3f trajDir1 = {};
			NkVec3f trajDir2 = {};

			// Meta
			uint32 clipIndex = 0;	///< Index du clip source
			float32 clipTime = 0.f; ///< Temps dans le clip
			bool isTransition = false;

			/**
			 * @brief Distance entre deux features (pour la recherche).
			 * Coefficients de pondération configurables via NkMotionDatabase.
			 */
			[[nodiscard]] float32 Distance(const NkMotionFeature &other,
										   const float32 *weights = nullptr) const noexcept;
	};

	struct NkMotionDatabase {
			NkVector<NkMotionFeature> features; ///< Tous les features de la DB

			// Poids des features pour la recherche
			float32 weightFootPos = 1.0f;
			float32 weightFootVel = 0.5f;
			float32 weightHipVel = 0.7f;
			float32 weightTrajPos = 1.0f;
			float32 weightTrajDir = 0.5f;

			/**
			 * @brief Recherche la frame la plus proche du query.
			 * Utilise une K-D tree ou brute force selon la taille.
			 * @return Index dans features du meilleur match.
			 */
			[[nodiscard]] uint32 FindBestMatch(const NkMotionFeature &query, uint32 excludeStart = 0,
											   uint32 excludeEnd = 0) const noexcept;

			/**
			 * @brief Construit la base de données depuis une liste de clips.
			 */
			void BuildFromClips(const NkVector<nk_uint64> &clipHandles, float32 fps = 30.f) noexcept;

			bool SaveToFile(const char *path) const noexcept;
			bool LoadFromFile(const char *path) noexcept;
	};

	struct NkMotionMatch {
			NkMotionDatabase database;
			float32 queryRate = 10.f;	  ///< Fréquence de recherche (Hz)
			float32 blendDuration = 0.2f; ///< Durée du blend entre poses
			bool enabled = true;

			uint32 currentFeatureIdx = 0; ///< Index courant dans la DB
			float32 currentClipTime = 0.f;
			float32 queryTimer = 0.f;
			float32 blendTimer = 0.f;
			uint32 prevFeatureIdx = 0;
	};
	NK_COMPONENT(NkMotionMatch)

	// =========================================================================
	// NkCrowdAgent — agent de simulation de foule
	// =========================================================================
	struct NkCrowdAgent {
			// ── Navigation ────────────────────────────────────────────────────
			NkVec3f destination = {};
			NkVec3f currentVelocity = {};
			float32 maxSpeed = 1.5f;
			float32 radius = 0.3f; ///< Rayon de l'agent (m)
			float32 height = 1.8f; ///< Hauteur (m)

			// ── Séparation (steering behaviors) ──────────────────────────────
			float32 separationRadius = 0.8f;
			float32 separationForce = 2.f;
			float32 alignmentRadius = 2.f;
			float32 alignmentForce = 1.f;
			float32 cohesionRadius = 3.f;
			float32 cohesionForce = 0.5f;

			// ── Animation ────────────────────────────────────────────────────
			NkEntityId animatorEntity = NkEntityId::Invalid();
			float32 animSpeedMult = 1.f;

			// ── LOD (Level of Detail) de la simulation ────────────────────────
			enum class LOD : uint8 {
				Full,	///< Sim + Anim + Physique
				Medium, ///< Sim + Anim simplifiée
				Low,	///< Sim seulement (anim billboards)
				Culled	///< Hors caméra — tick rare
			};
			LOD currentLOD = LOD::Full;

			// ── État interne ─────────────────────────────────────────────────
			NkVec3f steeringForce = {};
			bool hasPath = false;
			bool reached = false;
			uint32 pathNodeIdx = 0;
	};
	NK_COMPONENT(NkCrowdAgent)

	// =========================================================================
	// Systèmes ECS de locomotion
	// =========================================================================

	// NkFootIKSystem : la jambe (hanche→genou→pied) est résolue par
	// NkIKSolver::SolveTwoBone (adaptateur -> renderer::NkIKSystem::NK_TWO_BONE,
	// voir Rigging/NkIKSolver.h) — zéro FABRIK/TwoBone réimplémenté ici. Le
	// raycast sol délègue à physics::NkPhysicsWorld::Raycast (NKPhysics réel)
	// quand SetPhysicsWorld() a été appelé ; sinon repli plan-plat honnête
	// (voir RaycastGround dans NkLocomotion.cpp).
	class NkFootIKSystem final : public NkSystem {
		public:
			[[nodiscard]] NkSystemDesc Describe() const override {
				return NkSystemDesc{}
					.Reads<NkTransform>()
					.Writes<NkFootIK>()
					.Writes<NkSkeleton>()
					.InGroup(NkSystemGroup::PostUpdate)
					.WithPriority(350.f)
					.Named("NkFootIKSystem");
			}

			void Execute(NkWorld &world, float32 dt) noexcept override;

			// Monde physique à interroger pour le raycast sol (NON possédé —
			// typiquement &NkPhysicsSystem::World()). Laissé nul = repli plan-
			// plat CPU-only (voir tête de fichier + NkLocomotion.cpp).
			void SetPhysicsWorld(physics::NkPhysicsWorld *w) noexcept {
				mPhysWorld = w;
			}

			// Hauteur du plan de sol utilisé UNIQUEMENT quand aucun monde
			// physique n'est fourni (démo / tests CPU-only sans NKPhysics).
			void SetFallbackGroundY(float32 y) noexcept {
				mFallbackGroundY = y;
			}

		private:
			NkIKSolver mSolver;
			physics::NkPhysicsWorld *mPhysWorld = nullptr; // non possédé
			float32 mFallbackGroundY = 0.f;

			bool RaycastGround(NkWorld &world, const NkVec3f &from, float32 len, uint32 mask,
							   NkFootContact &out) const noexcept;
	};

	// NkLocomotionSystem : le blend Walk/Run est un anim::NkBlendTree1D
	// RÉEL (ConfigureBlend fournit les clips, SetParameter/Update font le
	// mélange bone-local ou legacy matriciel selon le clip -- voir
	// NKAnimation/NkAnimation.h) — zéro blend
	// réimplémenté ici. Limitation assumée : la pose résultante est écrite
	// dans ecs::NkSkeleton comme transform MONDE indépendante par os
	// (DecomposeTRS direct, pas de re-FK) -- correct pour un squelette PLAT
	// (bones[i].parent == -1), voir NkLocomotion.cpp.
	class NkLocomotionSystem final : public NkSystem {
		public:
			[[nodiscard]] NkSystemDesc Describe() const override {
				return NkSystemDesc{}
					.Writes<NkLocomotion>()
					.Writes<NkTransform>()
					.Writes<NkAnimator>()
					.Writes<NkSkeleton>()
					.InGroup(NkSystemGroup::Update)
					.WithPriority(200.f)
					.Named("NkLocomotionSystem");
			}

			void Execute(NkWorld &world, float32 dt) noexcept override;

			// Configure le blend-space 1D Walk(pos=0)/Run(pos=1). Les clips ne
			// sont PAS possédés (durée de vie gérée par l'appelant, comme pour
			// NkBlendTree1D::AddClip côté NKRenderer).
			void ConfigureBlend(const anim::NkAnimationClip *walk, const anim::NkAnimationClip *run) noexcept;

			[[nodiscard]] const anim::NkAnimationState &GetBlendedState() const noexcept {
				return mBlend.GetState();
			}

			[[nodiscard]] const anim::NkBlendTree1D &GetBlendTree() const noexcept {
				return mBlend;
			}

		private:
			anim::NkBlendTree1D mBlend; // partagé par le système -- suffisant pour une démo
											 // mono-personnage ; un jeu multi-personnages voudrait
											 // une instance par entité (table NkHashMap<NkEntityId,...>).
	};

	class NkMotionMatchSystem final : public NkSystem {
		public:
			[[nodiscard]] NkSystemDesc Describe() const override {
				return NkSystemDesc{}
					.Writes<NkMotionMatch>()
					.Reads<NkLocomotion>()
					.Writes<NkSkeleton>()
					.InGroup(NkSystemGroup::PostUpdate)
					.WithPriority(300.f)
					.Named("NkMotionMatchSystem");
			}

			void Execute(NkWorld &world, float32 dt) noexcept override;

		private:
			NkMotionFeature BuildQuery(const NkLocomotion &loco, const NkSkeleton &sk) const noexcept;
	};

	class NkCrowdSystem final : public NkSystem {
		public:
			[[nodiscard]] NkSystemDesc Describe() const override {
				return NkSystemDesc{}
					.Writes<NkCrowdAgent>()
					.Writes<NkTransform>()
					.InGroup(NkSystemGroup::Update)
					.WithPriority(150.f)
					.Named("NkCrowdSystem");
			}

			void Execute(NkWorld &world, float32 dt) noexcept override;

		private:
			void UpdateSteering(NkCrowdAgent &agent, NkWorld &world, const NkTransform &tf, float32 dt) noexcept;
			void UpdateLOD(NkCrowdAgent &agent, NkWorld &world, const NkTransform &tf) noexcept;
	};

} // namespace nkentseu
