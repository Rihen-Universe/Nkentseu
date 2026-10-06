#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkLiveBalance.h — L'EQUILIBRE EN TEMPS REEL : le centre de masse
// ramene au-dessus des appuis.
//
// A chaque image : le centre de masse de la pose (NkPoseMass : une masse par
// joint, anthropometrique si les os sont nommes), projete au sol, est compare au
// POLYGONE D'APPUI (l'enveloppe convexe des points d'appui que donne l'hote ou
// l'IK des pieds). S'il en sort, ou s'approche du bord a moins de la marge voulue,
// le BASSIN glisse horizontalement vers l'interieur -- juste assez -- et l'IK des
// pieds, appelee apres, garde les pieds plantes. Le glissement est lisse (ressort
// critique, etat garde ici) et borne.
//
// Ce que le bassin deplace : tout le corps sauf ce que l'IK replante. La part du
// centre de masse qui suit le bassin est donc < 1 ; elle est estimee depuis les
// masses des jambes declarees (SetPinnedLegs), et le glissement la compense.
//
// Le polygone est calcule sans allocation (32 points au plus).
// =============================================================================

#include "NKAnima/Physics/NkPoseMass.h"
#include "NKAnima/Realtime/NkRtMath.h"
#include "NKAnima/Realtime/NkRtRig.h"

namespace nkentseu {
	namespace anim {
		namespace rt {

			struct NkLiveBalanceSettings {
					float32 targetMargin = 0.03f; ///< marge voulue a l'interieur du polygone (m)
					float32 maxShift = 0.15f;	  ///< glissement maximal du bassin (m)
					float32 smoothTime = 0.12f;	  ///< s (0 : immediat)
					float32 weight = 1.f;		  ///< dosage (0 : l'animation)
			};

			class NkLiveBalance {
				public:
					static constexpr uint32 kMaxSupport = 32u;

					/// Les masses (depuis les noms du squelette) et le joint deplace.
					bool Setup(const NkRtRig &rig, int32 pelvis);
					/// Les jambes que l'IK replantera (cheville, genou) : la part du centre
					/// de masse qui ne suit pas le bassin.
					void SetPinnedLegs(const NkRtRig &rig, const uint32 *ankles, const uint32 *knees, uint32 count);
					void Reset();
					/// Le glissement repart de zero, sans saut (apres un relevement).
					void Relax();

					/// Une image : `support` = les points d'appui (monde). Deplace le bassin
					/// dans `world` (et rafraichit). Rend la marge PREDITE apres correction.
					float32 Apply(const NkRtRig &rig, const math::NkMat4f *local, math::NkMat4f *world,
								  const NkVec3f *support, uint32 count, float32 dt);

					NkLiveBalanceSettings settings;

					/// Marge signee (m) du point `p` (projete au sol) dans le polygone des
					/// points `pts` : > 0 dedans. Un seul point ou un segment : -distance.
					static float32 SignedMargin(const NkVec3f &p, const NkVec3f *pts, uint32 count);

					[[nodiscard]] NkVec3f Com() const noexcept {
						return mCom;
					}
					[[nodiscard]] NkVec3f Shift() const noexcept {
						return rt::V3(mShiftX, 0.f, mShiftZ);
					}
					[[nodiscard]] float32 MarginBefore() const noexcept {
						return mMarginBefore;
					}
					[[nodiscard]] float32 FollowFraction() const noexcept {
						return mFollow;
					}
					NkPoseMass &Mass() noexcept {
						return mMass;
					}
					[[nodiscard]] const NkPoseMass &Mass() const noexcept {
						return mMass;
					}

				private:
					NkPoseMass mMass;
					int32 mPelvis = -1;
					float32 mFollow = 1.f;
					float32 mShiftX = 0.f, mShiftZ = 0.f, mVelX = 0.f, mVelZ = 0.f;
					NkVec3f mCom = {0.f, 0.f, 0.f};
					float32 mMarginBefore = 0.f;
					bool mPrimed = false;
			};

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
