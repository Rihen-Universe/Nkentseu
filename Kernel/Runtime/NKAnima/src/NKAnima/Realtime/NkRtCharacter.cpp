// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkRtCharacter.cpp — la physique d'animation d'un personnage (voir
// l'en-tete pour l'ordre d'une image et ses raisons).
// =============================================================================
#include "NKAnima/Realtime/NkRtCharacter.h"

namespace nkentseu {
	namespace anim {
		namespace rt {

			bool NkRtCharacter::Setup(const int32 *parents, const math::NkMat4f *bindLocal, const char *const *names,
									  uint32 count) {
				if (!rig.Build(parents, count, names) || bindLocal == nullptr)
					return false;
				const uint32 n = rig.Count();
				mBindWorld.Resize(n);
				mLocal.Resize(n);
				mAnim.Resize(n);
				mOut.Resize(n);
				mPrev.Resize(n);
				rig.LocalToWorld(bindLocal, mBindWorld.Data());
				for (uint32 j = 0; j < n; ++j) {
					mLocal[j] = bindLocal[j];
					mAnim[j] = mBindWorld[j];
					mOut[j] = mBindWorld[j];
					mPrev[j] = mBindWorld[j];
				}
				(void)footIK.Setup(rig, mBindWorld.Data());
				(void)balance.Setup(rig, footIK.Pelvis());
				NkVector<uint32> chevilles, genoux;
				for (uint32 i = 0; i < footIK.LegCount(); ++i) {
					chevilles.PushBack((uint32)footIK.LegJoints(i).ankle);
					genoux.PushBack((uint32)footIK.LegJoints(i).knee);
				}
				balance.SetPinnedLegs(rig, chevilles.Data(), genoux.Data(), (uint32)chevilles.Size());
				(void)springs.Setup(rig, mBindWorld.Data());
				static const char *const kMots[] = {"ponytail", "hair", "tail", "spring", "jiggle", "cape", "antenna",
													"meche", "queue", "ressort"};
				NkSpringParams pr;
				(void)springs.AddChainsByName(kMots, (uint32)(sizeof(kMots) / sizeof(kMots[0])), pr);
				(void)ragdoll.Setup(rig, mBindWorld.Data(), &balance.Mass());
				mHasPrev = false;
				mPhaseAvant = NkRagdollPhase::Animated;
				return true;
			}

			void NkRtCharacter::Reset() {
				footIK.Reset();
				balance.Reset();
				springs.Reset(mOut.Data());
				mHasPrev = false;
			}

			void NkRtCharacter::Hit(uint32 joint, const NkVec3f &dv) {
				if (joint >= rig.Count())
					return;
				ragdoll.Trigger(mOut.Data(), mHasPrev ? mPrev.Data() : nullptr, mLastDt, joint, dv);
			}

			void NkRtCharacter::Update(const math::NkMat4f *animLocal, float32 groundRefY, const NkRtGround &ground,
									   float32 dt) {
				const uint32 n = rig.Count();
				if (n == 0u || animLocal == nullptr || dt <= 0.f)
					return;
				for (uint32 j = 0; j < n; ++j)
					mPrev[j] = mOut[j];
				// 1. L'animation, recalee par le dernier relevement.
				const bool recale = ragdoll.RelocationActive();
				for (uint32 j = 0; j < n; ++j) {
					mLocal[j] = animLocal[j];
					if (recale && rig.Parent(j) < 0)
						mLocal[j] = ragdoll.Relocation() * animLocal[j];
				}
				rig.LocalToWorld(mLocal.Data(), mAnim.Data());
				for (uint32 j = 0; j < n; ++j)
					mOut[j] = mAnim[j];

				const NkRagdollPhase phase = ragdoll.Phase();
				const bool physique = phase == NkRagdollPhase::Falling || phase == NkRagdollPhase::Ragdoll;
				mSupportCount = 0;
				if (!physique) {
					// 2. Les appuis : les pieds ANIMES en appui, chacun un petit rectangle.
					const NkFootIKSettings &fs = footIK.settings;
					for (uint32 i = 0; i < footIK.LegCount() && mSupportCount + 4u <= NkLiveBalance::kMaxSupport; ++i) {
						const NkFootIKLeg &l = footIK.LegJoints(i);
						const NkVec3f a = Origin(mAnim[(uint32)l.ankle]);
						const float32 levee = (a.y - groundRefY) - footIK.RestAnkleHeight(i);
						const float32 appui = 1.f - SmoothStep(fs.plantLow, fs.plantHigh, levee);
						if (appui < 0.5f)
							continue;
						NkVec3f bout = Madd(a, V3(0.f, 0.f, 1.f), 0.12f);
						if (l.toe >= 0)
							bout = Origin(mAnim[(uint32)l.toe]);
						const NkVec3f av = Normalize(V3(bout.x - a.x, 0.f, bout.z - a.z), V3(0.f, 0.f, 1.f));
						const NkVec3f lat = Scale(V3(-av.z, 0.f, av.x), supportHalfWidth);
						const NkVec3f talon = Madd(a, av, -0.03f);
						mSupport[mSupportCount++] = Add(talon, lat);
						mSupport[mSupportCount++] = Sub(talon, lat);
						mSupport[mSupportCount++] = Add(bout, lat);
						mSupport[mSupportCount++] = Sub(bout, lat);
					}
					if (useBalance && phase == NkRagdollPhase::Animated)
						(void)balance.Apply(rig, mLocal.Data(), mOut.Data(), mSupport, mSupportCount, dt);
					// 3. Les pieds sur le sol reel, la ou l'animation les met.
					if (useFootIK)
						footIK.Apply(rig, mLocal.Data(), mOut.Data(), mAnim.Data(), groundRefY, ground, dt);
				}
				// 4. Le ragdoll et le relevement.
				ragdoll.Update(rig, mOut.Data(), dt, ground);
				const NkRagdollPhase apres = ragdoll.Phase();
				if (apres != mPhaseAvant && apres == NkRagdollPhase::GettingUp) {
					footIK.Relax();
					balance.Relax();
				}
				mPhaseAvant = apres;
				// 5. Le secondaire, sur la pose finale.
				if (useSprings)
					springs.Apply(rig, mLocal.Data(), mOut.Data(), dt, &ground);
				mLastDt = dt;
				mHasPrev = true;
			}

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
