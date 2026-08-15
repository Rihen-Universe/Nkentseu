// =============================================================================
// NKRenderer/Tools/Animation/NkPoseDebugDraw.cpp  —  viz debug M3 (voir .h).
// =============================================================================
#include "NKRenderer/Tools/Animation/NkPoseDebugDraw.h"
#include "NKAnimPhysics/NkBalance.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"

namespace nkentseu {
	namespace renderer {

		using math::NkVec3f;
		using math::NkVec4f;

		void NkPoseDebugDraw::Draw(NkRender3D &r3d, const NkVec3f *jointWorld, int32 count, const animphys::NkPoseMass &mass,
								   const NkVec3f *supportPts, int32 supportCount, const NkVec3f &groundNormal,
								   const NkPoseDebugVizOptions &opt, const NkVec3f &comVelocity) {
			if (jointWorld == nullptr || count <= 0)
				return;

			// Normale de sol unitaire.
			NkVec3f n = groundNormal;
			const float32 nl = math::NkSqrt(n.x * n.x + n.y * n.y + n.z * n.z);
			if (nl > 1e-6f) {
				n.x /= nl;
				n.y /= nl;
				n.z /= nl;
			} else {
				n = NkVec3f{0.f, 1.f, 0.f};
			}

			// COM + état d'équilibre.
			const NkVec3f com = mass.ComputeCOMFromPositions(jointWorld, count);
			animphys::NkBalanceResult bal;
			bool haveBal = false;
			if (supportPts != nullptr && supportCount > 0) {
				bal = animphys::NkBalance::EvaluateStatic(com, supportPts, supportCount, n);
				haveBal = true;
			}

			const NkVec4f green{0.15f, 1.0f, 0.25f, 1.0f};
			const NkVec4f red{1.0f, 0.20f, 0.20f, 1.0f};
			const NkVec4f yellow{1.0f, 0.85f, 0.15f, 1.0f};
			const NkVec4f white{0.9f, 0.9f, 0.9f, 1.0f};
			const NkVec4f comColor = (haveBal && !bal.balanced) ? red : green;

			// --- Polygone de support : arêtes (boucle fermée sur les coins fournis) + coins. ---
			if (opt.drawSupport && supportPts != nullptr && supportCount > 0) {
				for (int32 i = 0; i < supportCount; ++i) {
					const NkVec3f a = supportPts[i];
					const NkVec3f b = supportPts[(i + 1) % supportCount];
					r3d.DrawDebugLine(a, b, yellow, 0.f, true); // overlay = visible à travers la géométrie
					r3d.DrawDebugSphere(a, opt.supportRadius, yellow);
				}
			}

			// --- Projection du COM sur le sol (fil d'aplomb + cercle). ---
			if (opt.drawProjection && supportPts != nullptr && supportCount > 0) {
				// Point du plan = centroïde des appuis ; proj = com - dot(com - p0, n) * n.
				NkVec3f p0{0.f, 0.f, 0.f};
				for (int32 i = 0; i < supportCount; ++i) {
					p0.x += supportPts[i].x;
					p0.y += supportPts[i].y;
					p0.z += supportPts[i].z;
				}
				const float32 inv = 1.0f / static_cast<float32>(supportCount);
				p0.x *= inv;
				p0.y *= inv;
				p0.z *= inv;
				const float32 d = (com.x - p0.x) * n.x + (com.y - p0.y) * n.y + (com.z - p0.z) * n.z;
				const NkVec3f proj{com.x - d * n.x, com.y - d * n.y, com.z - d * n.z};
				r3d.DrawDebugLine(com, proj, comColor, 0.f, true);
				r3d.DrawDebugCircle(proj, opt.comRadius * 1.5f, n, comColor);
			}

			// --- Centre de masse. ---
			if (opt.drawCOM)
				r3d.DrawDebugSphere(com, opt.comRadius, comColor);

			// --- Direction de bascule (option). ---
			if (opt.drawTipArrow) {
				const NkVec3f v = comVelocity;
				const float32 vl = math::NkSqrt(v.x * v.x + v.y * v.y + v.z * v.z);
				if (vl > 1e-4f) {
					// composante horizontale de la vitesse, mise à l'échelle pour la lisibilité.
					const float32 dh = v.x * n.x + v.y * n.y + v.z * n.z;
					NkVec3f h{v.x - dh * n.x, v.y - dh * n.y, v.z - dh * n.z};
					const float32 hl = math::NkSqrt(h.x * h.x + h.y * h.y + h.z * h.z);
					if (hl > 1e-4f) {
						const float32 s = 0.3f / hl;
						const NkVec3f to{com.x + h.x * s, com.y + h.y * s, com.z + h.z * s};
						r3d.DrawDebugArrow(com, to, white);
					}
				}
			}
		}

	} // namespace renderer
} // namespace nkentseu
