#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkRtCharacter.h — LA PHYSIQUE D'ANIMATION D'UN PERSONNAGE, en une
// classe : ce que Noge, NKScena et PV3DE appellent une fois par image.
//
//   entree : la pose de l'ANIMATION (locale ; la racine porte deja la place du
//            personnage dans le monde), le sol de l'hote, dt ;
//   sortie : la pose FINALE (monde), prete pour la peau (monde x inverseBind).
//
// L'ordre d'une image, et pourquoi :
//   1. FK de l'animation (recalee si un relevement a deplace le personnage) ;
//   2. EQUILIBRE : le bassin glisse pour garder le centre de masse sur les appuis
//      (les pieds animes en appui) ;
//   3. IK DES PIEDS : le bassin descend au sol le plus bas, les jambes posent les
//      pieds sur le sol reel -- la ou l'ANIMATION les met (pas apres le glissement
//      de l'equilibre : c'est ce qui les garde plantes) ;
//   4. RAGDOLL : apres un choc, la physique prend la main (fondu), puis rend la
//      main a l'animation de relevement ; pendant ce temps, 2 et 3 dorment ;
//   5. SECONDAIRE : les ressorts en DERNIER -- ils suivent la pose finale, donc
//      la chute aussi.
//
// Aucune allocation apres Setup. Le cout par image est mesure par le banc
// (tests/test_temps_reel.cpp, critere « cout »).
// =============================================================================

#include "NKAnima/Realtime/NkFootIK.h"
#include "NKAnima/Realtime/NkLiveBalance.h"
#include "NKAnima/Realtime/NkRagdollBlend.h"
#include "NKAnima/Realtime/NkSpringBones.h"

namespace nkentseu {
	namespace anim {
		namespace rt {

			class NkRtCharacter {
				public:
					/// Le squelette (parents, repos LOCAL, noms). Les jambes, le bassin et
					/// les chaines secondaires se trouvent par les noms (foot/ankle ;
					/// ponytail, hair, tail, spring, jiggle, cape...). Faux si le squelette
					/// est invalide.
					bool Setup(const int32 *parents, const math::NkMat4f *bindLocal, const char *const *names, uint32 count);

					/// Une image. `animLocal` : la pose de l'animation (Count() matrices).
					/// `groundRefY` : le sol sur lequel l'animation a ete faite (monde).
					void Update(const math::NkMat4f *animLocal, float32 groundRefY, const NkRtGround &ground, float32 dt);
					/// Remet a zero les lissages et les ressorts (teleportation, nouvelle scene).
					void Reset();

					/// LE CHOC : un changement de vitesse sur un joint -> ragdoll.
					void Hit(uint32 joint, const NkVec3f &deltaVelocity);

					/// La pose finale (monde), et celle de l'animation seule (monde).
					[[nodiscard]] const math::NkMat4f *World() const noexcept {
						return mOut.Data();
					}
					[[nodiscard]] const math::NkMat4f *AnimWorld() const noexcept {
						return mAnim.Data();
					}
					[[nodiscard]] uint32 Count() const noexcept {
						return rig.Count();
					}
					[[nodiscard]] const math::NkMat4f *BindWorld() const noexcept {
						return mBindWorld.Data();
					}

					NkRtRig rig;
					NkLiveBalance balance;
					NkFootIK footIK;
					NkRagdollBlend ragdoll;
					NkSpringBones springs;
					bool useBalance = true;
					bool useFootIK = true;
					bool useSprings = true;
					float32 supportHalfWidth = 0.045f; ///< demi-largeur d'un pied (m)

					/// Les points d'appui de la derniere image (pour l'affichage).
					[[nodiscard]] const NkVec3f *Support(uint32 &count) const noexcept {
						count = mSupportCount;
						return mSupport;
					}

				private:
					NkVector<math::NkMat4f> mBindWorld, mLocal, mAnim, mOut, mPrev;
					NkVec3f mSupport[NkLiveBalance::kMaxSupport];
					uint32 mSupportCount = 0;
					float32 mLastDt = 1.f / 60.f;
					bool mHasPrev = false;
					NkRagdollPhase mPhaseAvant = NkRagdollPhase::Animated;
			};

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
