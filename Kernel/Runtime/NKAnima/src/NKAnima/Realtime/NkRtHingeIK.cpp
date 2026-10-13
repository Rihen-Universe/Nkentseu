// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkRtHingeIK.cpp — l'IK a deux os a charniere (voir NkRtHingeIK.h).
// =============================================================================
#include "NKAnima/Realtime/NkRtHingeIK.h"

namespace nkentseu {
	namespace anim {
		namespace rt {

			float32 NkRtSolveTwoBoneHinge(const NkRtRig &rig, const math::NkMat4f *local, math::NkMat4f *world, uint32 a, uint32 b,
										  uint32 c, const NkVec3f &target, const NkVec3f &poleDir, float32 reach) {
				const NkVec3f S = Origin(world[a]), E = Origin(world[b]), W = Origin(world[c]);
				const float32 l1 = Distance(S, E), l2 = Distance(E, W);
				if (l1 < 1e-6f || l2 < 1e-6f) {
					return Distance(W, target);
				}
				// La place du milieu et du bout : la loi des cosinus, le milieu du cote du pole.
				const NkVec3f versCible = Sub(target, S);
				float32 d = Length(versCible);
				const NkVec3f dir = d > 1e-6f ? Scale(versCible, 1.f / d) : V3(0.f, -1.f, 0.f);
				d = Clamp(d, std::fabs(l1 - l2) + 1e-3f * (l1 + l2), (l1 + l2) * reach);
				const NkVec3f bout = Madd(S, dir, d);
				const float32 cosA = Clamp((l1 * l1 + d * d - l2 * l2) / (2.f * l1 * d), -1.f, 1.f);
				const float32 sinA = std::sqrt(1.f - cosA * cosA);
				NkVec3f perp = Sub(poleDir, Scale(dir, Dot(poleDir, dir)));
				if (Dot(perp, perp) < 1e-10f) {
					perp = Cross(dir, V3(1.f, 0.f, 0.f));
				}
				perp = Normalize(perp, AnyPerpendicular(dir));
				const NkVec3f milieu = Add(S, Add(Scale(dir, l1 * cosA), Scale(perp, l1 * sinA)));
				// 1. L'os du haut : la direction du milieu.
				RotateAbout(world[a], QFromTo(Sub(E, S), Sub(milieu, S)), S);
				rig.RefreshDescendants(a, local, world);
				// 2. Son roulis : le bout dans le plan (haut, milieu, cible). L'os du milieu
				// reste une charniere.  MUTATION twobone_no_roll : pas de roulis.
				if (!NkRtMutated(NK_RT_MUT_TWOBONE_NO_ROLL)) {
					const NkVec3f e1 = Origin(world[b]), w1 = Origin(world[c]);
					const NkVec3f u = Normalize(Sub(e1, S), dir);
					NkVec3f f = Sub(w1, e1), g = Sub(bout, e1);
					f = Sub(f, Scale(u, Dot(u, f)));
					g = Sub(g, Scale(u, Dot(u, g)));
					if (Dot(f, f) > 1e-8f * l1 * l1 && Dot(g, g) > 1e-8f * l1 * l1) {
						const float32 angle = std::atan2(Dot(u, Cross(f, g)), Dot(f, g));
						RotateAbout(world[a], QFromAxisAngle(u, angle), S);
						rig.RefreshDescendants(a, local, world);
					}
				}
				// 3. L'os du milieu plie.
				const NkVec3f e2 = Origin(world[b]), w2 = Origin(world[c]);
				RotateAbout(world[b], QFromTo(Sub(w2, e2), Sub(bout, e2)), e2);
				rig.RefreshDescendants(b, local, world);
				return Distance(Origin(world[c]), target);
			}

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
