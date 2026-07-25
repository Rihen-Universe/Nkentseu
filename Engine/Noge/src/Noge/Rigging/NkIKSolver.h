#pragma once
// =============================================================================
// Nkentseu/Rigging/NkIKSolver.h
// =============================================================================
// [RÉVISION 2026-07-23] : NkIKSolver est désormais une fine COUCHE D'ADAPTATION
// au-dessus de renderer::NkIKSystem (Kernel/Runtime/NKRenderer/src/NKRenderer/
// Tools/IK/NkIKSystem.h) et non plus une réimplémentation indépendante de
// FABRIK / CCD / Two-Bone / Spline IK. Raison : NkIKSystem existe DÉJÀ en
// production — solveurs testés et utilisés par NkAnima (Applications/Sandbox/
// src/Demo/DemoIK.cpp, DemoIKChar.cpp, DemoAnimIK.cpp) — Noge ne réimplémente
// plus ces algorithmes ; il traduit ses structures ECS (ecs::NkSkeleton) vers/
// depuis les structures NKRenderer (NkIKRig / NkIKChainDesc / matrices MONDE)
// et délègue la résolution numérique.
//
// DESIGN ACTUEL :
//   • Stockage : `renderer::NkIKSystem mSystem`, un seul système IK par
//     NkIKSolver. Les rigs y sont indexés par un id opaque dérivé de l'adresse
//     du ecs::NkSkeleton (un ecs::NkSkeleton n'a pas d'identité stable côté
//     NKRenderer — seule sa position mémoire sert de clé pour ce pont).
//   • Chain / TwoBoneChain / SplineChain (API publique conservée pour rester
//     compatible avec NkFootIKSystem etc.) sont convertis en NkIKChainDesc +
//     enum renderer::NkIKSolver puis résolus par NkIKSystem::SolveRig — voir
//     NkIKSolver.cpp pour le détail du marshalling (BuildWorldPose / write-back).
//   • BuildWorldPose fait un FK minimal (local -> monde, bones[i].parent, os
//     supposés stockés PARENT AVANT ENFANT) depuis ecs::NkSkeleton::bones[i].
//     {localPosition,localRotation,localScale}. C'est du MARSHALLING de
//     données (pas de l'IK) : NkIKRig::SetWorldPose exige des matrices MONDE.
//   • Write-back : seuls les os de la CHAÎNE résolue reçoivent une nouvelle
//     transform LOCALE (déduite du monde résolu / monde du parent) ; les os
//     hors chaîne (et leurs descendants) ne sont PAS re-FK — limitation
//     connue et assumée (voir NkIKSolver.cpp). `skinMatrices` est en revanche
//     recalculé pour TOUT le squelette (monde résolu × inverseBindPose), donc
//     le rendu skinné reste cohérent même sans ce re-FK.
//   • SolveSpline délègue à renderer::NkIKSolver::NK_SPLINE. Ce chemin est
//     ENCORE UN STUB côté NkIKSystem.cpp (SolveChain_Spline ne fait rien,
//     commentaire "stub" à la source) : ce n'est PAS une régression introduite
//     ici — l'appel est réellement délégué, il ne fait juste rien tant que
//     l'amont NKRenderer n'implémente pas ce solveur. Zéro réimplémentation
//     locale de substitution.
//   • TwoBoneChain::stretchable / maxStretch n'ont pas d'équivalent dans
//     NkIKSystem::SolveChain_TwoBone (qui clampe toujours la distance au
//     reach, sans facteur d'étirement configurable) : ignorés côté adaptateur,
//     pas réimplémentés localement.
//   • Les contraintes par-os (Chain::constraints, NkIKConstraint) ne sont PAS
//     transmises au backend : renderer::NkIKSystem::ApplyConstraint() est lui
//     aussi un stub (aucun effet, cf. NkIKSystem.cpp) — les convertir serait
//     du travail mort tant que l'amont ne les consomme pas.
//
// USAGE (inchangé pour les appelants existants — NkFootIKSystem etc.) :
//   NkIKSolver solver;
//   NkIKSolver::Chain chain;
//   chain.boneIndices = {0, 1, 2, 3};  // root → effecteur
//   chain.targetPosition = {5, 2, 0};
//   chain.weight = 1.f;
//   solver.SolveFABRIK(skeleton, chain);   // délègue à NkIKSystem::NK_FABRIK
//
//   NkIKSolver::TwoBoneChain tb;
//   tb.rootBone = 5; tb.midBone = 6; tb.tipBone = 7;
//   tb.target = handPosition; tb.poleTarget = elbowHint; tb.usePole = true;
//   solver.SolveTwoBone(skeleton, tb);     // délègue à NkIKSystem::NK_TWO_BONE
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKECS/System/NkSystem.h" // ecs::NkSystem / NkSystemDesc (NkConstraintSystem ci-dessous)
#include "NKECS/World/NkWorld.h"	// ecs::NkWorld (Execute/ResolveConstraint)
#include "NKMath/NKMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "Noge/ECS/Components/Animation/NkAnimation.h"
#include "NKRenderer/Tools/IK/NkIKSystem.h"

namespace nkentseu {

	using namespace math;

	// =========================================================================
	// NkIKConstraint — contrainte d'angle sur les os, utilisée par
	// NkConstraintSystem (composant ECS générique CopyRotation/LimitRotation/…
	// ci-dessous). SANS RAPPORT avec le solveur IK : conservée telle quelle,
	// hors-scope de cette passe (voir note NkConstraintSystem plus bas).
	// =========================================================================
	struct NkIKConstraint {
			bool enabled = false;
			float32 minAngleX = -180.f; ///< Limite rotation X (degrés)
			float32 maxAngleX = 180.f;
			float32 minAngleY = -180.f;
			float32 maxAngleY = 180.f;
			float32 minAngleZ = -180.f;
			float32 maxAngleZ = 180.f;
	};

	// =========================================================================
	// NkIKSolver — adaptateur ECS Noge -> renderer::NkIKSystem
	// =========================================================================
	class NkIKSolver {
		public:
			NkIKSolver() noexcept;
			~NkIKSolver() noexcept = default;

			// Contient un renderer::NkIKSystem (rigs alloués via NKMemory) :
			// ni copiable, ni déplaçable — une seule instance par utilisateur
			// (ex. un membre NkIKSolver par système ECS, comme NkFootIKSystem).
			NkIKSolver(const NkIKSolver &) = delete;
			NkIKSolver &operator=(const NkIKSolver &) = delete;
			NkIKSolver(NkIKSolver &&) = delete;
			NkIKSolver &operator=(NkIKSolver &&) = delete;

			// ── Chaîne générique (FABRIK / CCD) ──────────────────────────
			struct Chain {
					NkVector<uint32> boneIndices; ///< Root → tip (indices dans NkSkeleton)
					NkVec3f targetPosition = {};
					NkQuatf targetRotation = NkQuatf::Identity();
					float32 weight = 1.f; ///< Influence IK [0..1]
					uint32 maxIterations = 10;
					float32 tolerance = 0.0001f;		  ///< Distance de convergence
					bool constrainTip = false;			  ///< Orienter le tip vers target
					NkVector<NkIKConstraint> constraints; ///< NON transmis au backend (stub amont, voir tête de fichier)
			};

			// ── Chaîne bipartite (TwoBone — analytique) ──────────────────
			struct TwoBoneChain {
					uint32 rootBone = 0;	 ///< Épaule / cuisse
					uint32 midBone = 0;		 ///< Coude / genou
					uint32 tipBone = 0;		 ///< Poignet / cheville
					NkVec3f target = {};	 ///< Position cible de l'effecteur
					NkVec3f poleTarget = {}; ///< Hint de direction du joint intermédiaire
					bool usePole = false;
					float32 weight = 1.f;
					bool stretchable = false;  ///< NON PORTÉ (voir tête de fichier)
					float32 maxStretch = 1.5f; ///< NON PORTÉ (voir tête de fichier)
			};

			// ── Chaîne spline ─────────────────────────────────────────────
			struct SplineChain {
					NkVector<uint32> boneIndices;	///< Os à déformer le long de la courbe
					NkVector<NkVec3f> splinePoints; ///< NON PORTÉS (voir tête de fichier — stub amont)
					float32 weight = 1.f;
					bool useYUp = true; ///< NON PORTÉ (stub amont)
			};

			// ── Solveurs — DÉLÈGUENT à renderer::NkIKSystem ────────────────

			/**
			 * @brief FABRIK — délègue à renderer::NkIKSolver::NK_FABRIK.
			 * @return Plafond d'itérations transmis (chain.maxIterations) :
			 *         NkIKSystem ne remonte pas le nombre d'itérations RÉELLEMENT
			 *         effectuées (early-exit interne sur la tolérance) — ce n'est
			 *         donc pas une mesure exacte, juste la borne configurée.
			 */
			uint32 SolveFABRIK(ecs::NkSkeleton &skeleton, const Chain &chain) noexcept;

			/**
			 * @brief CCD — délègue à renderer::NkIKSolver::NK_CCD. Même remarque
			 * sur la valeur de retour que SolveFABRIK.
			 */
			uint32 SolveCCD(ecs::NkSkeleton &skeleton, const Chain &chain) noexcept;

			/**
			 * @brief TwoBone — délègue à renderer::NkIKSolver::NK_TWO_BONE
			 * (solution analytique exacte, loi des cosinus + pôle).
			 */
			void SolveTwoBone(ecs::NkSkeleton &skeleton, const TwoBoneChain &chain) noexcept;

			/**
			 * @brief Spline IK — délègue à renderer::NkIKSolver::NK_SPLINE.
			 * NOTE : ce chemin est un STUB côté NKRenderer (SolveChain_Spline ne
			 * fait rien) — l'appel est réellement délégué, le squelette restera
			 * simplement inchangé tant que l'amont n'est pas implémenté.
			 */
			void SolveSpline(ecs::NkSkeleton &skeleton, const SplineChain &chain) noexcept;

			// ── Utilitaires (géométrie pure, pas de l'IK — pas de délégation
			//    nécessaire : simple somme de distances sur la pose MONDE) ────

			/**
			 * @brief Calcule la longueur totale d'une chaîne d'os (pose monde
			 * courante, via le même FK que celui utilisé pour nourrir NkIKSystem).
			 */
			[[nodiscard]] float32 ChainLength(const ecs::NkSkeleton &skeleton,
											  const NkVector<uint32> &boneIndices) const noexcept;

			/**
			 * @brief Vérifie si le target est atteignable depuis la chaîne.
			 */
			[[nodiscard]] bool IsReachable(const ecs::NkSkeleton &skeleton, const NkVector<uint32> &boneIndices,
										   const NkVec3f &target) const noexcept;

		private:
			renderer::NkIKSystem mSystem;

			[[nodiscard]] static uint64 SkeletonKey(const ecs::NkSkeleton &sk) noexcept;
			renderer::NkIKRig *GetOrCreateRig(const ecs::NkSkeleton &sk) noexcept;

			// FK local -> monde (marshalling ECS -> NKRenderer, voir tête de fichier).
			static void BuildWorldPose(const ecs::NkSkeleton &sk, NkVector<NkMat4f> &outWorld) noexcept;

			// Write-back monde résolu -> local (uniquement les os de `boneIdx`) +
			// recalcul de skinMatrices pour tout le squelette.
			static void WriteBackChain(ecs::NkSkeleton &sk, const NkVector<uint32> &boneIdx,
									   const NkMat4f *worldAfter, uint32 worldAfterCount) noexcept;

			// Coeur partagé par SolveFABRIK/SolveCCD/SolveTwoBone : construit la
			// pose monde, (re)crée la chaîne côté NkIKRig, appelle SolveRig,
			// écrit le résultat. `chainTag` sert de clé de nommage de chaîne
			// (root/tip) pour retrouver/mettre à jour la même NkIKChainDesc d'un
			// appel à l'autre au lieu d'en empiler une nouvelle à chaque frame.
			uint32 SolveByDesc(ecs::NkSkeleton &skeleton, const NkVector<uint32> &boneIndices,
							   const NkVec3f &targetPos, const NkQuatf &targetRot, bool matchRotation,
							   const NkVec3f &poleVector, bool usePole, float32 weight, uint32 maxIterations,
							   float32 tolerance, renderer::NkIKSolver solverKind) noexcept;
	};

	// =========================================================================
	// NkConstraintSystem — composant ECS + système de contraintes
	// -----------------------------------------------------------------------
	// [NOTE 2026-07-23] Hors-scope de la passe d'adaptation IK/Locomotion (voir
	// Engine/Noge/ROADMAP.md, item G1.4) : laissé tel quel (spec déclarée, 0
	// .cpp). Pas de régression — confirmé par grep, NkConstraintSystem n'était
	// déjà instancié NULLE PART dans le moteur avant cette révision (dead code
	// pré-existant, indépendant de NkIKSolver).
	// =========================================================================

	enum class NkConstraintType : uint8 {
		CopyLocation,  ///< Copie la position d'une entité cible
		CopyRotation,  ///< Copie la rotation d'une entité cible
		CopyScale,	   ///< Copie l'échelle d'une entité cible
		CopyTransform, ///< Copie toute la transform d'une entité cible
		LookAt,		   ///< Oriente vers une entité cible
		TrackTo,	   ///< Track-To (maintient un axe vers la cible)
		StretchTo,	   ///< Étire l'os vers la cible
		ClampTo,	   ///< Contraindre la position sur une courbe
		LimitLocation, ///< Limites de position min/max
		LimitRotation, ///< Limites de rotation min/max
		LimitScale,	   ///< Limites d'échelle min/max
		Floor,		   ///< Empêche de passer sous un plan
		ChildOf,	   ///< Parentage inverse avec correction de bind
	};

	struct NkConstraint {
			NkConstraintType type = NkConstraintType::LookAt;
			ecs::NkEntityId target = ecs::NkEntityId::Invalid();
			float32 influence = 1.f; ///< Poids [0..1]
			bool enabled = true;

			// Configuration par type
			struct {
					bool useX = true, useY = true, useZ = true;
					bool invert = false;
					float32 min[3] = {-1e9f, -1e9f, -1e9f};
					float32 max[3] = {1e9f, 1e9f, 1e9f};
					uint8 trackAxis = 1; ///< 0=X, 1=Y, 2=Z, 3=-X, 4=-Y, 5=-Z
					uint8 upAxis = 1;
			} cfg;
	};
	NK_COMPONENT(NkConstraint)

	/**
	 * @brief Système ECS qui résout toutes les contraintes actives.
	 * Exécuté dans PostUpdate après NkTransformSystem.
	 */
	class NkConstraintSystem final : public ecs::NkSystem {
		public:
			[[nodiscard]] ecs::NkSystemDesc Describe() const override {
				return ecs::NkSystemDesc{}
					.Reads<ecs::NkTransform>()
					.Writes<NkConstraint>()
					.InGroup(ecs::NkSystemGroup::PostUpdate)
					.WithPriority(500.f)
					.Named("NkConstraintSystem");
			}

			void Execute(ecs::NkWorld &world, float32 dt) noexcept override;

		private:
			void ResolveConstraint(ecs::NkWorld &world, ecs::NkEntityId entity, NkConstraint &c) noexcept;
	};

} // namespace nkentseu
