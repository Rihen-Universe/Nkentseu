// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Motion/NkRootMotion.cpp : le mouvement de la racine d'un clip (voir NkRootMotion.h).
// =============================================================================
#include "NKAnima/Motion/NkRootMotion.h"
#include "NKAnima/Realtime/NkRtMath.h"
#include "NKAnima/Realtime/NkRtTwist.h"

#include <cmath>

namespace nkentseu {
	namespace anim {

		namespace {
			uint32 gRootMotionMutations = 0u;
			bool Mute(uint32 bit) {
				return (gRootMotionMutations & bit) != 0u;
			}

			/// L'origine et l'orientation de l'os racine a un instant.
			struct Racine {
					NkVec3f o = {0.f, 0.f, 0.f};
					rt::NkRtQuat q;
			};

			/// La transformation de l'os depuis une cle : locale (une racine : son monde),
			/// ou matrice de peau d'un clip non local (peau x repos = monde).
			NkMat4f CleMonde(const NkAnimationClip &clip, uint32 bone, const NkMat4f &m) {
				if (!clip.skeletalLocal && bone < (uint32)clip.jointInverseBind.Size()) {
					return m * clip.jointInverseBind[bone].Inverse();
				}
				return m;
			}
			Racine DepuisMat(const NkMat4f &m) {
				Racine r;
				r.o = rt::Origin(m);
				r.q = rt::QFromMat(m);
				return r;
			}
			rt::NkRtQuat Nlerp(const rt::NkRtQuat &a, const rt::NkRtQuat &b, float32 u) {
				const float32 d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
				const float32 sg = d < 0.f ? -1.f : 1.f;
				return rt::QNormalize(rt::Q(a.x + (b.x * sg - a.x) * u, a.y + (b.y * sg - a.y) * u, a.z + (b.z * sg - a.z) * u,
											a.w + (b.w * sg - a.w) * u));
			}

			/// L'os racine a `t` : origine lineaire, orientation NLERP entre deux cles, la
			/// courbe de la cle comprise (le meme geste que l'echantillonnage des poses,
			/// Blend/NkAnimMix.cpp EchantillonOs).
			Racine Echantillon(const NkAnimationClip &clip, uint32 bone, float32 t) {
				const NkAnimationTrack<NkMat4f> &tr = clip.boneTracks[bone];
				const uint32 n = tr.KeyCount();
				if (n == 1 || t <= tr.GetKey(0).time) {
					return DepuisMat(CleMonde(clip, bone, tr.GetKey(0).value));
				}
				if (t >= tr.GetKey(n - 1).time) {
					return DepuisMat(CleMonde(clip, bone, tr.GetKey(n - 1).value));
				}
				uint32 hi = 1;
				while (hi < n && tr.GetKey(hi).time <= t) {
					++hi;
				}
				const NkKeyframe<NkMat4f> &ka = tr.GetKey(hi - 1);
				const NkKeyframe<NkMat4f> &kb = tr.GetKey(hi);
				const Racine a = DepuisMat(CleMonde(clip, bone, ka.value));
				if (ka.interp == NkInterpMode::NK_STEP) {
					return a;
				}
				const Racine b = DepuisMat(CleMonde(clip, bone, kb.value));
				const float32 d = kb.time - ka.time;
				float32 u = d > 1e-6f ? (t - ka.time) / d : 0.f;
				if (ka.interp != NkInterpMode::NK_LINEAR && ka.interp != NkInterpMode::NK_CUBIC) {
					u = NkAnimationTrack<float32>::Ease(u, ka.interp);
				}
				Racine r;
				r.o = rt::Lerp(a.o, b.o, u);
				r.q = Nlerp(a.q, b.q, u);
				return r;
			}

			/// Le lacet de 0 a t, CUMULE de cle en cle (chaque pas est petit, donc jamais
			/// replie) : un demi-tour fait pi, un tour 2 pi.  MUTATION rootmotion_yaw_wrapped.
			float32 LacetCumule(const NkAnimationClip &clip, uint32 bone, float32 t, const NkVec3f &up) {
				const Racine r0 = Echantillon(clip, bone, 0.f);
				const Racine rt_ = Echantillon(clip, bone, t);
				if (Mute(NK_RM_MUT_YAW_WRAPPED)) {
					return rt::NkRtTwistAngle(rt::QMul(rt_.q, rt::QConj(r0.q)), up);
				}
				const NkAnimationTrack<NkMat4f> &tr = clip.boneTracks[bone];
				float32 acc = 0.f;
				rt::NkRtQuat prev = r0.q;
				for (uint32 i = 0; i < tr.KeyCount(); ++i) {
					const float32 ti = tr.GetKey(i).time;
					if (ti <= 0.f || ti >= t) {
						continue;
					}
					const rt::NkRtQuat qi = DepuisMat(CleMonde(clip, bone, tr.GetKey(i).value)).q;
					acc += rt::NkRtTwistAngle(rt::QMul(qi, rt::QConj(prev)), up);
					prev = qi;
				}
				return acc + rt::NkRtTwistAngle(rt::QMul(rt_.q, rt::QConj(prev)), up);
			}

			float32 Duree(const NkAnimationClip &clip, uint32 bone) {
				if (clip.duration > 1e-6f) {
					return clip.duration;
				}
				const NkAnimationTrack<NkMat4f> &tr = clip.boneTracks[bone];
				return tr.KeyCount() > 0 ? tr.GetKey(tr.KeyCount() - 1).time : 0.f;
			}

			NkVec3f Haut(const NkRootMotionSettings &s) {
				return rt::Normalize(s.up, rt::V3(0.f, 1.f, 0.f));
			}

			/// D^count par carres successifs (les puissances d'un meme deplacement commutent).
			NkRootMotionDelta Puissance(const NkRootMotionDelta &d, uint32 count, const NkVec3f &up) {
				NkRootMotionDelta res, base = d;
				while (count > 0u) {
					if ((count & 1u) != 0u) {
						res = NkRootMotionCompose(res, base, up);
					}
					base = NkRootMotionCompose(base, base, up);
					count >>= 1u;
				}
				return res;
			}

			/// M(t0)^-1 o M(t1), deux instants du MEME cycle.  MUTATION rootmotion_world_frame.
			NkRootMotionDelta Segment(const NkAnimationClip &clip, float32 t0, float32 t1, const NkRootMotionSettings &s) {
				NkRootMotionDelta m0, m1;
				NkRootMotionAt(clip, t0, s, m0);
				NkRootMotionAt(clip, t1, s, m1);
				if (Mute(NK_RM_MUT_WORLD_FRAME)) {
					NkRootMotionDelta d;
					d.translation = rt::Sub(m1.translation, m0.translation);
					d.yaw = m1.yaw - m0.yaw;
					return d;
				}
				const NkVec3f up = Haut(s);
				return NkRootMotionCompose(NkRootMotionInverse(m0, up), m1, up);
			}
		} // namespace

		void NkRootMotionSetMutations(uint32 bits) noexcept {
			gRootMotionMutations = bits;
		}
		uint32 NkRootMotionMutations() noexcept {
			return gRootMotionMutations;
		}

		int32 NkRootMotionBone(const NkAnimationClip &clip, const NkRootMotionSettings &settings) {
			int32 b = settings.bone;
			if (b < 0) {
				b = 0;
				for (uint32 j = 0; j < (uint32)clip.jointParent.Size(); ++j) {
					if (clip.jointParent[j] < 0) {
						b = (int32)j;
						break;
					}
				}
			}
			if ((uint32)b >= (uint32)clip.boneTracks.Size() || clip.boneTracks[(uint32)b].Empty()) {
				return -1;
			}
			return b;
		}

		NkRootMotionDelta NkRootMotionCompose(const NkRootMotionDelta &a, const NkRootMotionDelta &b, const NkVec3f &up) {
			NkRootMotionDelta r;
			r.translation = rt::Add(a.translation, rt::QRotate(rt::QFromAxisAngle(up, a.yaw), b.translation));
			r.yaw = a.yaw + b.yaw;
			return r;
		}

		NkRootMotionDelta NkRootMotionInverse(const NkRootMotionDelta &d, const NkVec3f &up) {
			NkRootMotionDelta r;
			r.yaw = -d.yaw;
			r.translation = rt::Scale(rt::QRotate(rt::QFromAxisAngle(up, -d.yaw), d.translation), -1.f);
			return r;
		}

		void NkRootMotionApply(const NkRootMotionDelta &d, const NkVec3f &up, NkVec3f &position, float32 &yaw) {
			position = rt::Add(position, rt::QRotate(rt::QFromAxisAngle(up, yaw), d.translation));
			yaw += d.yaw;
		}

		bool NkRootMotionAt(const NkAnimationClip &clip, float32 t, const NkRootMotionSettings &settings, NkRootMotionDelta &out) {
			out = NkRootMotionDelta();
			const int32 b = NkRootMotionBone(clip, settings);
			if (b < 0) {
				return false;
			}
			const uint32 bone = (uint32)b;
			const float32 dur = Duree(clip, bone);
			t = t < 0.f ? 0.f : (t > dur ? dur : t);
			const NkVec3f up = Haut(settings);
			const Racine r0 = Echantillon(clip, bone, 0.f);
			const Racine r = Echantillon(clip, bone, t);
			const NkVec3f d = rt::Sub(r.o, r0.o);
			const NkVec3f v = rt::Scale(up, rt::Dot(d, up));
			NkVec3f h = rt::V3(0.f, 0.f, 0.f);
			if (settings.horizontal) {
				h = rt::Add(h, rt::Sub(d, v));
			}
			if (settings.vertical) {
				h = rt::Add(h, v);
			}
			const float32 y = settings.yaw ? LacetCumule(clip, bone, t, up) : 0.f;
			// tau = c + h - R(y) c : la rotation autour de la verticale de la racine au depart.
			out.translation = rt::Sub(rt::Add(r0.o, h), rt::QRotate(rt::QFromAxisAngle(up, y), r0.o));
			out.yaw = y;
			return true;
		}

		NkRootMotionDelta NkRootMotionBetween(const NkAnimationClip &clip, float32 elapsed0, float32 elapsed1, const NkRootMotionSettings &settings) {
			const int32 b = NkRootMotionBone(clip, settings);
			if (b < 0) {
				return NkRootMotionDelta();
			}
			const NkVec3f up = Haut(settings);
			if (elapsed1 < elapsed0) {
				return NkRootMotionInverse(NkRootMotionBetween(clip, elapsed1, elapsed0, settings), up);
			}
			const float32 dur = Duree(clip, (uint32)b);
			if (dur <= 1e-6f) {
				return NkRootMotionDelta();
			}
			if (!clip.loop) {
				return Segment(clip, elapsed0, elapsed1, settings); // bornes par NkRootMotionAt
			}
			const float32 c0 = std::floor(elapsed0 / dur), c1 = std::floor(elapsed1 / dur);
			float32 t0 = elapsed0 - c0 * dur, t1 = elapsed1 - c1 * dur;
			t0 = t0 < 0.f ? 0.f : (t0 > dur ? dur : t0);
			t1 = t1 < 0.f ? 0.f : (t1 > dur ? dur : t1);
			if (c0 == c1 || Mute(NK_RM_MUT_NO_WRAP)) {
				return Segment(clip, t0, t1, settings);
			}
			// La fin du cycle de depart, les cycles entiers, le debut du cycle d'arrivee.
			NkRootMotionDelta cycle;
			NkRootMotionAt(clip, dur, settings, cycle);
			NkRootMotionDelta d = Segment(clip, t0, dur, settings);
			const float32 entiers = c1 - c0 - 1.f;
			if (entiers > 0.f) {
				d = NkRootMotionCompose(d, Puissance(cycle, (uint32)entiers, up), up);
			}
			NkRootMotionDelta debut;
			NkRootMotionAt(clip, t1, settings, debut);
			return NkRootMotionCompose(d, debut, up);
		}

		bool NkRootMotionStrip(const NkAnimationClip &clip, float32 t, const NkRootMotionSettings &settings, NkMat4f &rootTransform) {
			NkRootMotionDelta m;
			if (!NkRootMotionAt(clip, t, settings, m)) {
				return false;
			}
			if (Mute(NK_RM_MUT_STRIP_OFF)) {
				return true;
			}
			const NkVec3f up = Haut(settings);
			const NkRootMotionDelta inv = NkRootMotionInverse(m, up);
			rt::RotateAbout(rootTransform, rt::QFromAxisAngle(up, inv.yaw), rt::V3(0.f, 0.f, 0.f));
			rt::SetOrigin(rootTransform, rt::Add(rt::Origin(rootTransform), inv.translation));
			return true;
		}

	} // namespace anim
} // namespace nkentseu
