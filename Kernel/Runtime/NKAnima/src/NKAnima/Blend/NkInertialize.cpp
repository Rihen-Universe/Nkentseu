// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Blend/NkInertialize.cpp : la transition par inertie (voir NkInertialize.h).
// =============================================================================
#include "NKAnima/Blend/NkInertialize.h"
#include "NKAnima/Realtime/NkRtMath.h"
#include "NKAnima/Realtime/NkRtTwist.h"

#include <cmath>

namespace nkentseu {
	namespace anim {

		namespace {
			uint32 gInertializeMutations = 0u;
			bool Mute(uint32 bit) {
				return (gInertializeMutations & bit) != 0u;
			}
			rt::NkRtQuat VersRt(const NkQuatf &q) {
				return rt::Q(q.x, q.y, q.z, q.w);
			}
			NkQuatf DepuisRt(const rt::NkRtQuat &q) {
				return NkQuatf(q.x, q.y, q.z, q.w);
			}
		} // namespace

		void NkInertializeSetMutations(uint32 bits) noexcept {
			gInertializeMutations = bits;
		}
		uint32 NkInertializeMutations() noexcept {
			return gInertializeMutations;
		}

		// ── La courbe ────────────────────────────────────────────────────────────
		void NkInertialCurve::Set(float32 x0_, float32 v0_, float32 duration) noexcept {
			x0 = x0_;
			v0 = v0_;
			T = duration > 1e-6f ? duration : 1e-6f;
			a = b = c = a0 = 0.f;
			if (x0 <= 1e-9f) {
				x0 = v0 = 0.f;
				return;
			}
			// Un ecart qui grandit : pris a vitesse nulle. Une vitesse qui ferait passer
			// l'ecart sous zero avant T : T raccourci (aucun rebond).
			if (v0 > 0.f) {
				v0 = 0.f;
			}
			if (v0 < 0.f && !Mute(NK_IN_MUT_NO_SHORTEN)) { // MUTATION inertia_no_shorten
				const float32 tMax = -5.f * x0 / v0;
				T = tMax < T ? tMax : T;
			}
			const float32 T2 = T * T, T3 = T2 * T, T4 = T3 * T, T5 = T4 * T;
			a0 = (-8.f * v0 * T - 20.f * x0) / T2;
			a = -(a0 * T2 + 6.f * v0 * T + 12.f * x0) / (2.f * T5);
			b = (3.f * a0 * T2 + 16.f * v0 * T + 30.f * x0) / (2.f * T4);
			c = -(3.f * a0 * T2 + 12.f * v0 * T + 20.f * x0) / (2.f * T3);
		}

		float32 NkInertialCurve::Eval(float32 t) const noexcept {
			if (x0 == 0.f || t >= T) {
				return 0.f;
			}
			if (t <= 0.f) {
				return x0;
			}
			// MUTATION inertia_linear : la droite (la vitesse de depart est perdue).
			if (Mute(NK_IN_MUT_LINEAR)) {
				return x0 * (1.f - t / T);
			}
			return ((((a * t + b) * t + c) * t + 0.5f * a0) * t + v0) * t + x0;
		}

		// ── La transition ────────────────────────────────────────────────────────
		void NkInertialBlend::Start(const NkAnimPose &prev, const NkAnimPose &prevPrev, float32 dt, const NkAnimPose &target, float32 duration) {
			mDuration = duration > 1e-6f ? duration : 1e-6f;
			mArmed = true;
			mUnite.Set(1.f, 0.f, mDuration);
			const uint32 n = (uint32)(prev.bones.Size() < target.bones.Size() ? prev.bones.Size() : target.bones.Size());
			const bool avecVitesse = dt > 1e-6f && prevPrev.bones.Size() >= n;
			const bool rien = Mute(NK_IN_MUT_NO_OFFSET); // MUTATION inertia_no_offset
			mOs.Resize(n);
			for (uint32 j = 0; j < n; ++j) {
				Os &o = mOs[j];
				o = Os();
				if (rien) {
					continue;
				}
				const NkBoneTRS &p = prev.bones[j];
				const NkBoneTRS &cible = target.bones[j];
				// Position.
				const NkVec3f e = rt::Sub(p.t, cible.t);
				const float32 x0 = rt::Length(e);
				o.dir = x0 > 1e-9f ? rt::Scale(e, 1.f / x0) : rt::V3(0.f, 0.f, 0.f);
				float32 v0 = 0.f;
				if (avecVitesse && x0 > 1e-9f) {
					v0 = (x0 - rt::Dot(rt::Sub(prevPrev.bones[j].t, cible.t), o.dir)) / dt;
				}
				o.pos.Set(x0, v0, mDuration);
				// Rotation : l'ecart prev * cible^-1, par le chemin court.
				const rt::NkRtQuat qc = rt::QConj(VersRt(cible.r));
				const NkVec3f rv = rt::QToRotVec(rt::QMul(VersRt(p.r), qc));
				const float32 r0 = rt::Length(rv);
				o.axe = r0 > 1e-9f ? rt::Scale(rv, 1.f / r0) : rt::V3(0.f, 1.f, 0.f);
				float32 w0 = 0.f;
				if (avecVitesse && r0 > 1e-9f) {
					w0 = (r0 - rt::NkRtTwistAngle(rt::QMul(VersRt(prevPrev.bones[j].r), qc), o.axe)) / dt;
				}
				o.rot.Set(r0, w0, mDuration);
				// Echelle.
				o.ds = rt::Sub(p.s, cible.s);
			}
			const uint32 nm = (uint32)(prev.morphs.Size() < target.morphs.Size() ? prev.morphs.Size() : target.morphs.Size());
			mMorphs.Resize(nm);
			for (uint32 i = 0; i < nm; ++i) {
				mMorphs[i] = rien ? 0.f : prev.morphs[i] - target.morphs[i];
			}
		}

		void NkInertialBlend::Apply(NkAnimPose &pose, float32 elapsed) const {
			if (!Active(elapsed)) {
				return;
			}
			const float32 u = mUnite.Eval(elapsed);
			const uint32 n = (uint32)(pose.bones.Size() < mOs.Size() ? pose.bones.Size() : mOs.Size());
			for (uint32 j = 0; j < n; ++j) {
				const Os &o = mOs[j];
				NkBoneTRS &b = pose.bones[j];
				const float32 x = o.pos.Eval(elapsed);
				if (x != 0.f) {
					b.t = rt::Madd(b.t, o.dir, x);
				}
				const float32 r = o.rot.Eval(elapsed);
				if (r != 0.f) {
					b.r = DepuisRt(rt::QNormalize(rt::QMul(rt::QFromAxisAngle(o.axe, r), VersRt(b.r))));
				}
				if (u != 0.f) {
					b.s = rt::Madd(b.s, o.ds, u);
				}
			}
			const uint32 nm = (uint32)(pose.morphs.Size() < mMorphs.Size() ? pose.morphs.Size() : mMorphs.Size());
			for (uint32 i = 0; i < nm; ++i) {
				pose.morphs[i] += mMorphs[i] * u;
			}
		}

	} // namespace anim
} // namespace nkentseu
