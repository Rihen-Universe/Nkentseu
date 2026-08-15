// =============================================================================
// NKAnimPhysics/NkAutoPose.cpp  —  M3.5 : auto-posing (voir .h).
// =============================================================================
#include "NKAnimPhysics/NkAutoPose.h"
#include "NKAnimPhysics/NkPoseBalancer.h"
#include "NKAnimPhysics/NkBalance.h"

namespace nkentseu {
	namespace animphys {

		using math::NkVec3f;

		void NkAutoPose::BlendBalanced(const NkVec3f *poseA, const NkVec3f *poseB, int32 count, float32 t,
									   const NkPoseMass &mass, const bool *plantedMask, const NkVec3f *supportPts,
									   int32 supportCount, float32 balanceStrength, NkVector<NkVec3f> &out,
									   const NkVec3f &groundNormal) {
			if (poseA == nullptr || poseB == nullptr || count <= 0)
				return;
			const float32 u = math::NkClamp(t, 0.0f, 1.0f);

			// 1) Interpolation linéaire des positions monde.
			out.Resize(static_cast<uint64>(count));
			for (int32 i = 0; i < count; ++i) {
				const NkVec3f a = poseA[i];
				const NkVec3f b = poseB[i];
				out[static_cast<uint64>(i)] = NkVec3f{a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u,
													  a.z + (b.z - a.z) * u};
			}

			// 2) Correction d'équilibre de l'entre-deux (M3.4).
			if (supportPts != nullptr && supportCount > 0 && balanceStrength > 0.0f) {
				if (plantedMask != nullptr) {
					NkPoseBalancer::BalanceByUpperShift(out.Data(), count, mass, plantedMask, supportPts,
														supportCount, balanceStrength, 6, groundNormal);
				} else {
					NkPoseBalancer::BalanceByShift(out.Data(), count, mass, supportPts, supportCount,
												   balanceStrength, groundNormal);
				}
			}
		}

		bool NkAutoPose::SelfTest() {
			bool ok = true;

			// Pose A = debout équilibrée (COM ~ x=0). Pose B = penchée (COM ~ x=1, hors support).
			// À t=0.7, le lerp brut a le COM hors support (déséquilibré) → BlendBalanced doit rééquilibrer.
			NkVector<NkVec3f> A, B;
			// haut du corps + 4 coins de pieds.
			A.PushBack(NkVec3f{0.0f, 1.6f, 0.0f});
			A.PushBack(NkVec3f{0.0f, 1.0f, 0.0f});
			A.PushBack(NkVec3f{0.0f, 0.5f, 0.0f});
			A.PushBack(NkVec3f{-0.15f, 0.0f, -0.10f});
			A.PushBack(NkVec3f{0.15f, 0.0f, -0.10f});
			A.PushBack(NkVec3f{0.15f, 0.0f, 0.10f});
			A.PushBack(NkVec3f{-0.15f, 0.0f, 0.10f});
			// B : haut du corps décalé en x=1 (les pieds restent au même endroit).
			B = A;
			B[0] = NkVec3f{1.0f, 1.6f, 0.0f};
			B[1] = NkVec3f{1.0f, 1.0f, 0.0f};
			B[2] = NkVec3f{1.0f, 0.5f, 0.0f};
			const int32 n = static_cast<int32>(A.Size());

			NkPoseMass mass;
			mass.jointMass.Clear();
			mass.jointMass.PushBack(5.0f);
			mass.jointMass.PushBack(30.0f);
			mass.jointMass.PushBack(15.0f);
			mass.jointMass.PushBack(0.5f);
			mass.jointMass.PushBack(0.5f);
			mass.jointMass.PushBack(0.5f);
			mass.jointMass.PushBack(0.5f);

			bool planted[7] = {false, false, false, true, true, true, true};

			NkVector<NkVec3f> support;
			support.PushBack(NkVec3f{-0.15f, 0.0f, -0.10f});
			support.PushBack(NkVec3f{0.15f, 0.0f, -0.10f});
			support.PushBack(NkVec3f{0.15f, 0.0f, 0.10f});
			support.PushBack(NkVec3f{-0.15f, 0.0f, 0.10f});
			const int32 sc = static_cast<int32>(support.Size());

			const float32 t = 0.7f;

			// (a) lerp BRUT (strength 0) → doit être déséquilibré à t=0.7.
			NkVector<NkVec3f> raw;
			NkAutoPose::BlendBalanced(A.Data(), B.Data(), n, t, mass, planted, support.Data(), sc, 0.0f, raw);
			const NkVec3f comRaw = mass.ComputeCOMFromPositions(raw.Data(), n);
			const NkBalanceResult balRaw = NkBalance::EvaluateStatic(comRaw, support.Data(), sc);
			if (balRaw.balanced)
				ok = false; // le lerp brut penché DOIT être hors équilibre

			// (b) BlendBalanced (strength 1, pieds plantés) → équilibré, pieds inchangés.
			NkVector<NkVec3f> bal;
			NkAutoPose::BlendBalanced(A.Data(), B.Data(), n, t, mass, planted, support.Data(), sc, 1.0f, bal);
			const NkVec3f comBal = mass.ComputeCOMFromPositions(bal.Data(), n);
			const NkBalanceResult balOk = NkBalance::EvaluateStatic(comBal, support.Data(), sc);
			if (!balOk.balanced)
				ok = false;
			// Les pieds (3..6) doivent être identiques au lerp brut (non déplacés).
			for (int32 k = 3; k < 7; ++k) {
				const NkVec3f a = raw[static_cast<uint64>(k)];
				const NkVec3f b = bal[static_cast<uint64>(k)];
				const float32 dx = a.x - b.x;
				const float32 dy = a.y - b.y;
				const float32 dz = a.z - b.z;
				if (math::NkSqrt(dx * dx + dy * dy + dz * dz) > 1e-4f)
					ok = false;
			}

			// (c) bornes : t=0 rend A, t=1 rend B (lerp exact, strength 0).
			NkVector<NkVec3f> p0, p1;
			NkAutoPose::BlendBalanced(A.Data(), B.Data(), n, 0.0f, mass, planted, support.Data(), sc, 0.0f, p0);
			NkAutoPose::BlendBalanced(A.Data(), B.Data(), n, 1.0f, mass, planted, support.Data(), sc, 0.0f, p1);
			for (int32 i = 0; i < n; ++i) {
				const NkVec3f da{p0[static_cast<uint64>(i)].x - A[static_cast<uint64>(i)].x,
								 p0[static_cast<uint64>(i)].y - A[static_cast<uint64>(i)].y,
								 p0[static_cast<uint64>(i)].z - A[static_cast<uint64>(i)].z};
				const NkVec3f db{p1[static_cast<uint64>(i)].x - B[static_cast<uint64>(i)].x,
								 p1[static_cast<uint64>(i)].y - B[static_cast<uint64>(i)].y,
								 p1[static_cast<uint64>(i)].z - B[static_cast<uint64>(i)].z};
				if (math::NkSqrt(da.x * da.x + da.y * da.y + da.z * da.z) > 1e-5f)
					ok = false;
				if (math::NkSqrt(db.x * db.x + db.y * db.y + db.z * db.z) > 1e-5f)
					ok = false;
			}

			return ok;
		}

	} // namespace animphys
} // namespace nkentseu
