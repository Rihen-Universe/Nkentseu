// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Blend/NkPoseMirror.cpp — le miroir d'une pose (voir NkPoseMirror.h).
// =============================================================================
#include "NKAnima/Blend/NkPoseMirror.h"
#include "NKAnima/Realtime/NkRtRig.h" // les mutations des bancs

namespace nkentseu {
	namespace anim {

		NkQuatf NkMirrorRotation(const NkQuatf &q, const NkVec3f &normal) noexcept {
			// MUTATION mirror_off : la rotation est rendue telle quelle (la main gauche
			// recoit la rotation de la droite, sans miroir).
			if (rt::NkRtMutated(rt::NK_RT_MUT_MIRROR_OFF)) {
				return q;
			}
			const float32 d = 2.f * (q.x * normal.x + q.y * normal.y + q.z * normal.z);
			return NkQuatf(-q.x + d * normal.x, -q.y + d * normal.y, -q.z + d * normal.z, q.w);
		}

		NkBoneTRS NkMirrorBone(const NkBoneTRS &b, const NkVec3f &normal) noexcept {
			NkBoneTRS r = b;
			r.r = NkMirrorRotation(b.r, normal);
			if (!rt::NkRtMutated(rt::NK_RT_MUT_MIRROR_OFF)) {
				const float32 d = 2.f * (b.t.x * normal.x + b.t.y * normal.y + b.t.z * normal.z);
				r.t = {b.t.x - d * normal.x, b.t.y - d * normal.y, b.t.z - d * normal.z};
			}
			return r;
		}

		void NkMirrorPose(const NkAnimPose &in, const NkVector<int32> &pairs, const NkVec3f &normal, NkAnimPose &out) {
			// `in` et `out` peuvent etre la meme pose : les os sont lus d'une copie.
			const NkVector<NkBoneTRS> source = in.bones;
			if (&out != &in) {
				out = in;
			}
			const usize n = source.Size();
			for (usize j = 0; j < n; ++j) {
				const int32 p = j < pairs.Size() ? pairs[j] : -1;
				out.bones[j] = (p >= 0 && (usize)p < n) ? NkMirrorBone(source[(usize)p], normal) : source[j];
			}
		}

	} // namespace anim
} // namespace nkentseu
