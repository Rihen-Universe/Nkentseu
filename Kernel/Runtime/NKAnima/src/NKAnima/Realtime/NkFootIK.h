#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkFootIK.h — L'IK DES PIEDS SUR SOL IRREGULIER, en temps reel.
//
// L'animation a ete faite sur un sol plat (a `groundRefY`) ; le monde ne l'est
// pas. A chaque image :
//   1. sous chaque pied (cheville et orteil), un LANCER DE RAYON de l'hote donne la
//      hauteur du sol et sa normale ; l'ecart au sol de reference est le DECALAGE
//      du pied (positif sur une marche, negatif dans un creux) ;
//   2. le BASSIN descend (ou monte) du plus petit decalage des pieds en appui :
//      sans lui, la jambe tendue du pied le plus bas ne l'atteint pas ; et assez
//      pour que chaque jambe en appui reste a PORTEE de sa cible ;
//   3. chaque jambe est resolue par une IK A DEUX OS (hanche, genou, cheville)
//      vers la cheville animee + son decalage ; le genou garde le plan de la pose
//      animee (sinon : la direction des orteils) ;
//   4. le pied reprend son orientation animee, puis s'incline sur la normale du
//      sol, au prorata de son POIDS D'APPUI (un pied leve ne s'incline pas).
//
// ⚠️ Les decalages et le bassin sont LISSES par un ressort critique dont l'etat
// est garde ici (valeur + vitesse) -- jamais en repartant de la pose courante,
// qui a deja bouge a l'image d'avant (un poids applique chaque image depuis la
// position courante devient un TAUX, memoire du 26/09).
// =============================================================================

#include "NKAnima/Realtime/NkRtGround.h"
#include "NKAnima/Realtime/NkRtRig.h"

namespace nkentseu {
	namespace anim {
		namespace rt {

			struct NkFootIKLeg {
					int32 hip = -1, knee = -1, ankle = -1, toe = -1;
			};

			struct NkFootIKSettings {
					float32 maxStepUp = 0.45f;	   ///< marche la plus haute cherchee (m)
					float32 maxStepDown = 0.45f;   ///< creux le plus profond cherche (m)
					float32 maxPelvisDrop = 0.45f; ///< le bassin descend au plus de... (m)
					float32 maxPelvisRise = 0.45f;
					float32 pelvisSmoothTime = 0.06f; ///< s (0 : immediat)
					float32 footSmoothTime = 0.04f;	  ///< s (0 : immediat)
					float32 plantLow = 0.04f;		  ///< levee de la cheville en dessous : appui plein
					float32 plantHigh = 0.20f;		  ///< au-dessus : pied leve (poids 0)
					float32 maxFootTilt = 0.6f;		  ///< inclinaison maximale du pied (rad)
					float32 weight = 1.f;			  ///< dosage global (0 : l'animation)
			};

			struct NkFootIKLegState {
					bool grounded = false;
					NkVec3f groundPoint = {0.f, 0.f, 0.f};
					NkVec3f groundNormal = {0.f, 1.f, 0.f};
					float32 offset = 0.f;	   ///< decalage lisse applique (m)
					float32 plant = 0.f;	   ///< poids d'appui (0..1)
					NkVec3f target = {0.f, 0.f, 0.f};
					float32 reachError = 0.f;  ///< distance cheville-cible apres IK (m)
			};

			class NkFootIK {
				public:
					/// Le squelette, sa pose de repos monde, et les jambes trouvees par leurs
					/// NOMS (foot/ankle, puis le parent = genou, son parent = hanche, le
					/// premier enfant = orteil). `pelvis` : -1 = le parent de la 1re hanche.
					/// Rend le nombre de jambes trouvees.
					uint32 Setup(const NkRtRig &rig, const math::NkMat4f *bindWorld, int32 pelvis = -1);
					/// Une jambe donnee a la main (rig sans noms). Apres Setup.
					bool AddLeg(const NkFootIKLeg &leg);
					/// Les lissages repartent de zero et la prochaine image se pose d'un coup
					/// (teleportation, changement de sol).
					void Reset();
					/// Les lissages repartent de zero, SANS saut : la prochaine image glisse
					/// depuis l'animation (apres un relevement).
					void Relax();

					/// Une image. `world` : la pose entrante (monde), corrigee en place.
					/// `footSource` : la pose ou lire les pieds animes (nul = `world`) -- un
					/// equilibre qui a deja deplace le bassin passe la pose d'AVANT lui, pour
					/// que les pieds restent plantes ou l'animation les met.
					void Apply(const NkRtRig &rig, const math::NkMat4f *local, math::NkMat4f *world,
							   const math::NkMat4f *footSource, float32 groundRefY, const NkRtGround &ground, float32 dt);

					NkFootIKSettings settings;

					[[nodiscard]] uint32 LegCount() const noexcept {
						return (uint32)mLegs.Size();
					}
					[[nodiscard]] const NkFootIKLegState &LegState(uint32 i) const {
						return mLegs[i].st;
					}
					[[nodiscard]] const NkFootIKLeg &LegJoints(uint32 i) const {
						return mLegs[i].j;
					}
					[[nodiscard]] float32 PelvisOffset() const noexcept {
						return mPelvis;
					}
					[[nodiscard]] int32 Pelvis() const noexcept {
						return mPelvisJoint;
					}
					/// La hauteur de cheville au repos, au-dessus du sol du repos.
					[[nodiscard]] float32 RestAnkleHeight(uint32 i) const {
						return mLegs[i].restAnkle;
					}

				private:
					struct Leg {
							NkFootIKLeg j;
							float32 restAnkle = 0.08f;
							float32 reach = 0.8f; ///< hanche-genou + genou-cheville (repos)
							float32 off = 0.f, offVel = 0.f;
							NkFootIKLegState st;
					};
					const NkRtRig *mRig = nullptr;
					NkVector<math::NkMat4f> mBind;
					float32 mBindFloor = 0.f;
					NkVector<Leg> mLegs;
					int32 mPelvisJoint = -1;
					float32 mPelvis = 0.f, mPelvisVel = 0.f;
					bool mPrimed = false;
			};

			/// L'IK a deux os, sur des matrices MONDE : la chaine `a` (hanche, epaule),
			/// `b` (genou, coude), `c` (cheville, poignet) est tournee pour que `c`
			/// atteigne `target`, le coude/genou du cote de `poleDir`. Les sous-arbres
			/// sont rafraichis depuis `local`. Rend la distance restante a la cible.
			float32 NkRtSolveTwoBone(const NkRtRig &rig, const math::NkMat4f *local, math::NkMat4f *world, uint32 a,
									 uint32 b, uint32 c, const NkVec3f &target, const NkVec3f &poleDir);

			/// Le ressort critique (lissage sans depassement), etat externe.
			void NkRtSmoothDamp(float32 &value, float32 &velocity, float32 target, float32 smoothTime, float32 dt);

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
