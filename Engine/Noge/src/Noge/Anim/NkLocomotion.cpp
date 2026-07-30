// =============================================================================
// Noge/Anim/NkLocomotion.cpp — pont ECS -> NkIKSolver (jambes) + NkBlendTree1D
// -----------------------------------------------------------------------------
// Voir NkLocomotion.h pour le design. Ce fichier ne réimplémente ni IK ni
// blend : NkFootIKSystem délègue à NkIKSolver::SolveTwoBone (lui-même un
// adaptateur sur renderer::NkIKSystem, voir Rigging/NkIKSolver.cpp) et
// NkLocomotionSystem délègue à renderer::NkBlendTree1D::SetParameter/Update.
// NkMotionMatchSystem / NkCrowdSystem restent hors-scope (voir tête de
// fichier NkLocomotion.h) -- volontairement NON définis ici (dead code non
// instancié ailleurs, confirmé par grep, donc aucune régression de lien).
// =============================================================================
#include "NkLocomotion.h"

namespace nkentseu {

	// -------------------------------------------------------------------------
	// NkFootIKSystem
	// -------------------------------------------------------------------------
	bool NkFootIKSystem::RaycastGround(NkWorld & /*world*/, const NkVec3f &from, float32 len, uint32 mask,
										NkFootContact &out) const noexcept {
		if (mPhysWorld) {
			collision::NkRay3D ray;
			ray.origin = from;
			ray.dir = {0.f, -1.f, 0.f};
			ray.maxT = len;
			physics::NkBodyId hitBody{};
			collision::NkRayHit3D hit;
			if (mPhysWorld->Raycast(ray, hitBody, hit, mask)) {
				out.isGrounded = true;
				out.groundPos = hit.point;
				out.groundNormal = hit.normal;
				out.groundDist = from.y - hit.point.y;
				return true;
			}
			out.isGrounded = false;
			return false;
		}

		// Repli CPU-only (aucun monde physique fourni) : plan de sol plat à
		// mFallbackGroundY -- PAS un raycast contre un mesh de terrain, juste
		// un plan analytique pour les démos/tests sans NKPhysics. Documenté en
		// tête de fichier NkLocomotion.h.
		(void)mask;
		if (from.y - mFallbackGroundY > len) {
			out.isGrounded = false;
			return false;
		}
		out.isGrounded = true;
		out.groundPos = {from.x, mFallbackGroundY, from.z};
		out.groundNormal = {0.f, 1.f, 0.f};
		out.groundDist = from.y - mFallbackGroundY;
		return true;
	}

	void NkFootIKSystem::Execute(NkWorld &world, float32 dt) noexcept {
		world.Query<NkFootIK, NkSkeleton, NkTransform>().ForEach(
			[&](NkEntityId, NkFootIK &foot, NkSkeleton &sk, const NkTransform &) {
				if (!foot.enabled)
					return;

				auto solveLeg = [&](uint32 thighIdx, uint32 calfIdx, uint32 footIdx, NkFootContact &contact) {
					if (thighIdx >= sk.boneCount || calfIdx >= sk.boneCount || footIdx >= sk.boneCount)
						return;

					// Position MONDE courante du pied. Suppose un squelette PLAT
					// (bones[i].parent == -1) comme le reste de ce pont -- voir
					// tête de fichier NkLocomotion.h.
					const NkVec3f footPos = sk.bones[footIdx].localPosition;

					NkFootContact raw;
					const NkVec3f rayFrom = {footPos.x, footPos.y + foot.rayLength * 0.5f, footPos.z};
					const bool grounded = RaycastGround(world, rayFrom, foot.rayLength, foot.raycastLayerMask, raw);

					const float32 targetWeight = grounded ? 1.f : 0.f;
					contact.contactWeight += (targetWeight - contact.contactWeight) * NkClamp(foot.blendSpeed * dt, 0.f, 1.f);
					contact.isGrounded = grounded;
					if (grounded) {
						contact.groundPos = raw.groundPos;
						contact.groundNormal = raw.groundNormal;
						contact.groundDist = raw.groundDist;
					}

					const float32 blend = foot.ikWeight * contact.contactWeight;
					if (blend <= 1e-4f)
						return;

					NkVec3f corrected = footPos;
					corrected.y = contact.groundPos.y + foot.footHeight;
					const NkVec3f target = {
						footPos.x + (corrected.x - footPos.x) * blend,
						footPos.y + (corrected.y - footPos.y) * blend,
						footPos.z + (corrected.z - footPos.z) * blend,
					};

					// Délégation réelle -- voir Rigging/NkIKSolver.h/.cpp.
					NkIKSolver::TwoBoneChain chain;
					chain.rootBone = thighIdx;
					chain.midBone = calfIdx;
					chain.tipBone = footIdx;
					chain.target = target;
					chain.poleTarget = {0.f, 0.f, 1.f}; // genou vers l'avant (convention démo)
					chain.usePole = true;
					chain.weight = blend;
					mSolver.SolveTwoBone(sk, chain);
				};

				solveLeg(foot.leftThighIdx, foot.leftCalfIdx, foot.leftFootIdx, foot.leftFoot);
				solveLeg(foot.rightThighIdx, foot.rightCalfIdx, foot.rightFootIdx, foot.rightFoot);

				// Compensation de hanche (étape 4 du pipeline documenté en tête
				// de fichier) : léger abaissement selon la correction la plus
				// forte (pied le plus bas).
				if (foot.hipBoneIdx < sk.boneCount) {
					const float32 dL = foot.leftFoot.isGrounded ? (foot.leftFoot.groundPos.y + foot.footHeight) : 0.f;
					const float32 dR = foot.rightFoot.isGrounded ? (foot.rightFoot.groundPos.y + foot.footHeight) : 0.f;
					foot.hipOffset = NkMin(dL, dR) * foot.hipCompensation;
					sk.bones[foot.hipBoneIdx].localPosition.y += foot.hipOffset;
				}
			});
	}

	// -------------------------------------------------------------------------
	// NkLocomotionSystem
	// -------------------------------------------------------------------------
	void NkLocomotionSystem::ConfigureBlend(const renderer::NkAnimationClip *walk,
											const renderer::NkAnimationClip *run) noexcept {
		if (walk)
			mBlend.AddClip(walk, 0.f);
		if (run)
			mBlend.AddClip(run, 1.f);
	}

	void NkLocomotionSystem::Execute(NkWorld &world, float32 dt) noexcept {
		// 1) Gameplay pur (intégration vitesse/cap) : rien à déléguer, ce n'est
		// ni de l'IK ni du blend.
		world.Query<NkLocomotion, NkAnimator>().ForEach([&](NkEntityId, NkLocomotion &loco, NkAnimator &animator) {
			const float32 rate = 1.f / NkMax(loco.accelerationTime, 1e-4f);
			loco.speed += (loco.desiredSpeed - loco.speed) * NkClamp(rate * dt, 0.f, 1.f);
			loco.velocity = loco.facing * loco.speed;

			// 2) Paramètre de blend Walk(0)/Run(1) -- délégation réelle au
			// renderer::NkBlendTree1D (voir NkAnimationSystem.h/.cpp).
			const float32 span = NkMax(loco.runSpeed - loco.walkSpeed, 1e-3f);
			const float32 param = NkClamp((loco.speed - loco.walkSpeed) / span, 0.f, 1.f);
			mBlend.SetParameter(param);
			mBlend.Update(dt);

			animator.SetFloat("speed", loco.speed);
		});

		// 3) Publie la pose du blend tree dans les squelettes de la scène.
		// Démo mono-blend (voir commentaire sur mBlend dans NkLocomotion.h) :
		// tous les NkSkeleton présents reçoivent la même pose -- suffisant
		// pour le jalon (un personnage). Squelette PLAT attendu (parent==-1) :
		// DecomposeTRS direct, pas de re-FK hiérarchique (voir tête de fichier).
		const NkVector<NkMat4f> &blended = mBlend.GetState().boneMatrices;
		if (blended.Empty())
			return;
		world.Query<NkSkeleton>().ForEach([&](NkEntityId, NkSkeleton &sk) {
			const uint32 count = NkMin(sk.boneCount, (uint32)blended.Size());
			for (uint32 i = 0; i < count; ++i) {
				NkVec3f t, s;
				NkMat4f rot;
				NkMat4f m = blended[i];
				m.DecomposeTRS(t, rot, s);
				sk.bones[i].localPosition = t;
				sk.bones[i].localRotation = NkQuatf(rot);
				sk.bones[i].localScale = s;
				sk.skinMatrices[i] = blended[i]; // squelette plat -- monde == skin ici
			}
		});
	}

} // namespace nkentseu
