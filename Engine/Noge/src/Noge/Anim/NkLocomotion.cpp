// =============================================================================
// Noge/Anim/NkLocomotion.cpp — pont ECS -> NkIKSolver (jambes) + NkBlendTree1D
// -----------------------------------------------------------------------------
// Voir NkLocomotion.h pour le design. Ce fichier ne réimplémente ni IK ni
// blend : NkFootIKSystem délègue à NkIKSolver::SolveTwoBone (lui-même un
// adaptateur sur renderer::NkIKSystem, voir Rigging/NkIKSolver.cpp) et
// NkLocomotionSystem délègue à anim::NkBlendTree1D::SetParameter/Update.
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

				// La correction verticale REELLEMENT appliquee a chaque pied, en
				// metres. C'est un ECART, et c'est ce que la hanche doit suivre.
				float32 corrGauche = 0.f, corrDroite = 0.f;

				auto solveLeg = [&](uint32 thighIdx, uint32 calfIdx, uint32 footIdx, NkFootContact &contact,
										   float32 &corrOut, float32 plante) {
					if (thighIdx >= sk.BoneCount() || calfIdx >= sk.BoneCount() || footIdx >= sk.BoneCount())
						return;

					// ── Position MONDE courante du pied ──────────────────────────
					// ⚠️ CORRIGE LE 2026-09-26. Ce code lisait
					//    `Pose(footIdx).localPosition` COMME une position monde --
					//    juste pour un squelette PLAT (parent == -1), FAUX pour tout
					//    vrai personnage. Mesure sur CesiumMan.glb (18 os sur 19 ont
					//    un parent) : pose locale y = 0,0014 contre pose monde
					//    y = 0,0835. Le rayon partait donc du mauvais endroit, et la
					//    cible etait calculee depuis une position qui n'existe pas.
					//
					// ⚠️ LA COMPOSITION EXISTAIT DEJA : `NkIKSolver::BuildWorldPose`,
					//    qui suit l'ordre topologique de la definition. Elle etait
					//    seulement `private`. On l'a ouverte plutot que de la
					//    reecrire ici : le meme calcul a deux sites finit par
					//    diverger.
					//
					// ⚠️ ET LA COMPOSITION NE SE FAIT QU'UNE FOIS PAR LECTURE.
					//    On compose ICI pour LIRE ; `SolveTwoBone` recomposera pour
					//    RESOUDRE, depuis les memes poses LOCALES, qu'on ne touche
					//    pas entre les deux. Rien n'est compose a partir d'un
					//    resultat deja compose -- « une derivation en double, pas
					//    une compensation » a deja coute ici.
					//
					//    Recompose a CHAQUE jambe, et non une fois pour les deux :
					//    la premiere jambe resolue a modifie des poses locales, donc
					//    une pose monde calculee avant elle serait perimee pour la
					//    seconde. Deux compositions de ~20 os par image : le prix de
					//    la justesse est ici negligeable, et il est mesurable.
					NkVector<NkMat4f> mondeAvant;
					NkIKSolver::BuildWorldPose(sk, mondeAvant);
					if ((uint32)mondeAvant.Size() <= footIdx)
						return; // squelette incoherent : on ne devine pas une position
					const NkMat4f &mF = mondeAvant[(NkVector<NkMat4f>::SizeType)footIdx];
					const NkVec3f footPos = {mF[3][0], mF[3][1], mF[3][2]};

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

					// ⚠️ LE POIDS DE PLANTE MULTIPLIE LE MELANGE, sans seuil : a 0 le
					//    pied est LIBRE et rien ne le tire ; a 0,5 il est tire de
					//    moitie ; a 1 il epouse le sol. Un seuil interne rendrait ce
					//    reglage binaire sans que rien ne le dise -- le banc mesure
					//    donc explicitement le point milieu.
					const float32 planteSure = plante < 0.f ? 0.f : (plante > 1.f ? 1.f : plante);
					const float32 blend = foot.ikWeight * contact.contactWeight * planteSure;
					if (blend <= 1e-4f)
						return;

					// ── LA BASE : la pose AVANT notre propre correction ──────────
					// ⚠️ C'est elle, et non `footPos`, qui doit servir de depart au
					//    melange. `footPos` porte deja ce que nous avons ajoute a
					//    l'image precedente : melanger depuis lui compose le poids avec
					//    lui-meme, et tout poids non nul finit au sol en soixante
					//    images. Mesure avant correctif : poids 0,5 donnait 0,5500,
					//    EXACTEMENT comme poids 1,0, au lieu de 0,7740.
					//
					// ⚠️ ON NE DEFAIT NOTRE DELTA QUE SI PERSONNE N'A REECRIT LA POSE.
					//    Une animation qui repose le squelette a chaque image efface
					//    deja notre correction ; la retirer une seconde fois
					//    abaisserait le pied indefiniment. Le test est direct : la
					//    pose courante est-elle celle que nous avions VISEE ?
					NkVec3f base = footPos;
					if (contact.hasHistory) {
						const NkVec3f d = footPos - contact.lastTarget;
						if (d.x * d.x + d.y * d.y + d.z * d.z < 1e-6f)
							base.y = footPos.y - contact.appliedY;
					}

					NkVec3f corrected = base;
					corrected.y = contact.groundPos.y + foot.footHeight;
					// ⚠️ CE QUE LE POIDS DE PLANTE PEUT ET NE PEUT PAS FAIRE (2026-09-26)
					//
					//    A 0, il fonctionne exactement : `blend` vaut 0, la cible EST la
					//    position courante, rien ne bouge. Mesure : un pied leve a
					//    0,52 m du sol y reste, a 0,0000 m pres. C'est le comportement
					//    que Rodolf demandait et il est acquis.
					//
					// ⚠️ A UNE VALEUR INTERMEDIAIRE, IL NE PEUT PAS FONCTIONNER ICI, et
					//    ce n'est pas un reglage a corriger. Mesure : poids 0,5 place le
					//    pied a 0,5500 -- EXACTEMENT comme poids 1,0, au lieu de 0,7740.
					//    Cause : cette ligne melange depuis `footPos`, la position
					//    COURANTE, qui a deja bouge a l'image precedente.
					//    *Tout melange applique a chaque image depuis la position
					//    courante converge : un poids devient un taux.*
					//    Deplacer le melange vers `chain.weight` ne change rien -- teste,
					//    mesure, et retire : le solveur part lui aussi de la pose
					//    courante.
					//
					//    Pour qu'un poids reste un poids, il faut une REFERENCE STABLE :
					//    la pose d'animation AVANT correction, que ce systeme ne conserve
					//    pas. La conserver est du NEUF -> decision de Rodolf.
					//    Le banc le dit en `[DIT]`, il ne rougit pas : un poids binaire
					//    0/1 suffit a ce qui etait demande.
					// Le melange part de la BASE : c'est ce qui fait de `blend` un
					// POIDS et non un taux.
					const NkVec3f target = {
						base.x + (corrected.x - base.x) * blend,
						base.y + (corrected.y - base.y) * blend,
						base.z + (corrected.z - base.z) * blend,
					};
					// Ce que nous ajoutons, et la cible visee : de quoi nous defaire a
					// l'image suivante, et de quoi savoir si on nous a reecrit.
					contact.appliedY = target.y - base.y;
					contact.lastTarget = target;
					contact.hasHistory = true;
					// L'ECART vertical demande a ce pied, depuis la base.
					corrOut = contact.appliedY;

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

				solveLeg(foot.leftThighIdx, foot.leftCalfIdx, foot.leftFootIdx, foot.leftFoot,
						 corrGauche, foot.leftPlant);
				solveLeg(foot.rightThighIdx, foot.rightCalfIdx, foot.rightFootIdx, foot.rightFoot,
						 corrDroite, foot.rightPlant);

				// ── Compensation de hanche (étape 4 du pipeline) ─────────────────
				// Intention : léger abaissement selon la correction la plus forte
				// (le pied qui a dû descendre le plus).
				//
				// ⚠️ DEUX DÉFAUTS MESURÉS ET CORRIGÉS LE 2026-09-26, sur un sol à
				//    1,475 m (`NkDemoPiedsSurLaPente --mesure`) :
				//
				//    (1) `dL`/`dR` valaient `groundPos.y + footHeight` — une
				//        ALTITUDE de sol, pas un écart. `hipOffset` sortait à
				//        +0,7476 m pour un personnage qui n'avait rien à corriger.
				//        Ce sont désormais les ÉCARTS réellement appliqués aux pieds.
				//
				//    (2) `+=` s'ajoutait à chaque image sans défaire la précédente :
				//        la hanche passait de 2,74 m à 24,43 m en 29 images, soit
				//        +21,68 m de dérive. On applique maintenant la DIFFÉRENCE
				//        avec l'offset de l'image précédente, que le composant
				//        portait déjà sans que personne s'en serve pour ça.
				//
				// ⚠️ ET POURQUOI ÇA N'AVAIT JAMAIS ÉTÉ VU : le repli de
				//    `RaycastGround` met le sol à y = 0, et zéro fois la
				//    compensation vaut zéro. **Le défaut dormait dans la valeur
				//    nulle d'un repli.** Il ne se réveille que sur du relief — donc
				//    seulement depuis que `SetPhysicsWorld` est branché.
				if (foot.hipBoneIdx < sk.BoneCount()) {
					const float32 dL = foot.leftFoot.isGrounded ? corrGauche : 0.f;
					const float32 dR = foot.rightFoot.isGrounded ? corrDroite : 0.f;
					const float32 vise = NkMin(dL, dR) * foot.hipCompensation;
					sk.Pose(foot.hipBoneIdx).localPosition.y += (vise - foot.hipOffset);
					foot.hipOffset = vise; // l'état appliqué, pour pouvoir le défaire
				}
			});
	}

	// -------------------------------------------------------------------------
	// NkLocomotionSystem
	// -------------------------------------------------------------------------
	void NkLocomotionSystem::ConfigureBlend(const anim::NkAnimationClip *walk,
											const anim::NkAnimationClip *run) noexcept {
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
			// anim::NkBlendTree1D (voir NkAnimationSystem.h/.cpp).
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
			const uint32 count = NkMin(sk.BoneCount(), (uint32)blended.Size());
			for (uint32 i = 0; i < count; ++i) {
				NkVec3f t, s;
				NkMat4f rot;
				NkMat4f m = blended[i];
				m.DecomposeTRS(t, rot, s);
				sk.Pose(i).localPosition = t;
				sk.Pose(i).localRotation = NkQuatf(rot);
				sk.Pose(i).localScale = s;
				sk.skinMatrices[i] = blended[i]; // squelette plat -- monde == skin ici
			}
		});
	}

} // namespace nkentseu
