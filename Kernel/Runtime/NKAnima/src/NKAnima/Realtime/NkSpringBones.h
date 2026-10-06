#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkSpringBones.h — LE MOUVEMENT SECONDAIRE A RESSORTS (os « suiveurs »).
//
// Une queue de cheval, une queue, une antenne, une sangle : des chaines d'os qui
// SUIVENT l'animation avec du retard, du depassement et du poids, au lieu de la
// recopier. Chaque os d'une chaine porte une particule au bout (sa « pointe ») :
//
//   la CIBLE        = ou la pointe serait si l'os gardait sa pose animee, sous son
//                     parent DEJA corrige (la chaine se plie de proche en proche) ;
//   le RESSORT      = raideur k vers la cible, amortissement c sur la vitesse
//                     RELATIVE a la cible (suivre n'est pas freine), trainee d sur
//                     la vitesse absolue (l'air), gravite g ;
//   les LIMITES     = longueur de l'os gardee exactement, angle maximal entre l'os
//                     simule et l'os anime, spheres de collision (la tete, le dos).
//
// ⚠️ L'INTEGRATION EST IMPLICITE (Euler retrograde), en sous-pas fixes :
//     v1 = (v0 + h (k (cible - x0) + c v_cible + g)) / (1 + h (c + d) + h^2 k)
// Elle est inconditionnellement stable : une raideur de 5 000 a 15 images par
// seconde converge au lieu d'exploser. C'est le critere « le ressort converge sans
// exploser » du banc ; la mutation NK_RT_MUT_SPRING_EXPLICIT (Euler explicite, un
// seul pas par image) le fait rougir.
//
// Temps reel : cout par image = (os des chaines) x (sous-pas) + le rafraichissement
// du sous-arbre de chaque os tourne. Aucune allocation apres Setup.
// =============================================================================

#include "NKAnima/Realtime/NkRtGround.h"
#include "NKAnima/Realtime/NkRtMath.h"
#include "NKAnima/Realtime/NkRtRig.h"

namespace nkentseu {
	namespace anim {
		namespace rt {

			struct NkSpringParams {
					float32 stiffness = 90.f; ///< k (1/s^2) : le rappel vers la pose animee
					float32 damping = 9.f;	  ///< c (1/s)   : sur la vitesse RELATIVE a la cible
					float32 drag = 0.6f;	  ///< d (1/s)   : sur la vitesse absolue (l'air)
					float32 gravity = 1.f;	  ///< fraction de la gravite appliquee
					float32 maxAngle = 1.3f;  ///< ecart maximal a l'os anime (radians)
			};

			struct NkSpringCollider {
					int32 joint = -1;					  ///< le joint qui porte la sphere
					NkVec3f offset = {0.f, 0.f, 0.f};	  ///< dans le repere de ce joint
					float32 radius = 0.1f;
			};

			class NkSpringBones {
				public:
					/// Ajoute une chaine : `joints` du plus proche du corps au bout. Le
					/// parent du premier est l'ANCRE (il suit l'animation). Rend l'indice
					/// de la chaine, ou -1. A appeler apres Setup.
					int32 AddChain(const uint32 *joints, uint32 count, const NkSpringParams &params, float32 weight = 1.f);
					/// Les chaines dont le nom des os contient l'un des mots (« ponytail »,
					/// « hair », « tail », « spring », « jiggle »...) : chaque suite d'os
					/// ainsi nommes, du parent a l'enfant, devient une chaine.
					uint32 AddChainsByName(const char *const *words, uint32 wordCount, const NkSpringParams &params);

					/// Le squelette et sa pose de repos MONDE (les longueurs, les pointes).
					bool Setup(const NkRtRig &rig, const math::NkMat4f *bindWorld);
					/// Les particules sur la pose donnee, vitesses nulles (debut, teleportation).
					void Reset(const math::NkMat4f *world);
					/// Une image : corrige `world` (pose animee entrante, deja en monde) en
					/// place. `local` = la pose animee locale (pour rafraichir les sous-arbres).
					/// `ground` (facultatif) : les pointes ne traversent pas le sol de l'hote.
					void Apply(const NkRtRig &rig, const math::NkMat4f *local, math::NkMat4f *world, float32 dt,
							   const NkRtGround *ground = nullptr);

					void AddCollider(const NkSpringCollider &c) {
						mColliders.PushBack(c);
					}
					void ClearColliders() {
						mColliders.Clear();
					}

					NkVec3f gravity = {0.f, -9.81f, 0.f};
					float32 maxSubstep = 1.f / 120.f;	 ///< le pas d'integration maximal
					float32 teleportDistance = 1.0f; ///< un saut de l'ancre plus grand : Reset
					float32 groundRadius = 0.02f;	 ///< distance gardee au sol par les pointes (m)

					[[nodiscard]] uint32 ChainCount() const noexcept {
						return (uint32)mChains.Size();
					}
					/// La chaine `i` : son reglage (modifiable) et son dosage (0 : l'animation).
					NkSpringParams &Params(uint32 i) {
						return mChains[i].params;
					}
					float32 &Weight(uint32 i) {
						return mChains[i].weight;
					}
					/// L'ecart angulaire (radians) entre l'os simule et l'os anime, le plus
					/// grand de la derniere image -- ce que le banc et les demos lisent.
					[[nodiscard]] float32 LastMaxDeviation() const noexcept {
						return mLastMaxDev;
					}
					/// La pointe simulee du `k`-ieme os de la chaine `i` (monde).
					[[nodiscard]] NkVec3f Tip(uint32 i, uint32 k) const;
					/// La plus grande vitesse de pointe de la derniere image (m/s).
					[[nodiscard]] float32 LastMaxSpeed() const noexcept {
						return mLastMaxSpeed;
					}

				private:
					struct Bone {
							uint32 joint = 0;
							NkVec3f tipLocal = {0.f, 0.f, 0.f}; ///< la pointe dans le repere du joint
							float32 length = 0.f;
							NkVec3f p = {0.f, 0.f, 0.f}, v = {0.f, 0.f, 0.f};
							NkVec3f prevTarget = {0.f, 0.f, 0.f}, prevAnchor = {0.f, 0.f, 0.f};
					};
					struct Chain {
							NkVector<Bone> bones;
							NkSpringParams params;
							float32 weight = 1.f;
							bool primed = false;
					};
					const NkRtRig *mRig = nullptr;
					NkVector<math::NkMat4f> mBind; ///< la pose de repos monde (longueurs, pointes)
					NkVector<Chain> mChains;
					NkVector<NkSpringCollider> mColliders;
					float32 mLastMaxDev = 0.f;
					float32 mLastMaxSpeed = 0.f;
					bool mReady = false;
			};

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
