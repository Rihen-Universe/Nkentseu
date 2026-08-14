// =============================================================================
// NKAnimPhysics/NkPoseBalancer.cpp  —  M3.4 : optimiseur de pose.
// =============================================================================
#include "NKAnimPhysics/NkPoseBalancer.h"
#include "NKAnimPhysics/NkBalance.h"

namespace nkentseu {
	namespace renderer {

		using math::NkVec3f;

		namespace {

			// Retire de `v` sa composante le long de `n` (n supposé ~unitaire) → v horizontal.
			NkVec3f RemoveAlong(const NkVec3f &v, const NkVec3f &n) {
				const float32 d = v.x * n.x + v.y * n.y + v.z * n.z;
				return NkVec3f{v.x - d * n.x, v.y - d * n.y, v.z - d * n.z};
			}

			// Centroïde (moyenne) d'un nuage de points.
			NkVec3f Centroid(const NkVec3f *pts, int32 count) {
				NkVec3f c{0.f, 0.f, 0.f};
				if (count <= 0)
					return c;
				for (int32 i = 0; i < count; ++i) {
					c.x += pts[i].x;
					c.y += pts[i].y;
					c.z += pts[i].z;
				}
				const float32 inv = 1.0f / static_cast<float32>(count);
				return NkVec3f{c.x * inv, c.y * inv, c.z * inv};
			}

			// Extrait les positions monde (colonne translation) — ici l'entrée EST déjà des positions.
			// (Le balancer opère sur des positions monde : simple à composer avec l'IK / le clip.)

		} // namespace

		NkBalanceCorrection NkPoseBalancer::BalanceByShift(NkVec3f *jointWorld, int32 count, const NkPoseMass &mass,
														   const NkVec3f *supportPts, int32 supportCount,
														   float32 strength, const NkVec3f &groundNormal) {
			NkBalanceCorrection out;
			if (jointWorld == nullptr || count <= 0 || supportPts == nullptr || supportCount <= 0)
				return out;

			// Normale de sol unitaire (garde-fou).
			NkVec3f n = groundNormal;
			const float32 nlen = math::NkSqrt(n.x * n.x + n.y * n.y + n.z * n.z);
			if (nlen > 1e-6f) {
				n.x /= nlen;
				n.y /= nlen;
				n.z /= nlen;
			} else {
				n = NkVec3f{0.f, 1.f, 0.f};
			}

			// État AVANT correction.
			const NkVec3f comBefore = mass.ComputeCOMFromPositions(jointWorld, count);
			const NkBalanceResult balBefore = NkBalance::EvaluateStatic(comBefore, supportPts, supportCount, n);
			out.wasBalanced = balBefore.balanced;
			out.marginBefore = balBefore.margin;

			// Cible = centroïde des appuis (toujours à l'intérieur d'un polygone convexe).
			// Correction horizontale pondérée par `strength` (0 = pose intacte, 1 = COM sur le centroïde).
			const NkVec3f target = Centroid(supportPts, supportCount);
			const float32 s = math::NkClamp(strength, 0.0f, 1.0f);
			NkVec3f delta{target.x - comBefore.x, target.y - comBefore.y, target.z - comBefore.z};
			delta = RemoveAlong(delta, n); // horizontal seulement (on ne « soulève » pas le corps)
			const NkVec3f shift{delta.x * s, delta.y * s, delta.z * s};

			// Applique le décalage à toute la pose (V1 : corps rigide horizontal).
			for (int32 i = 0; i < count; ++i) {
				jointWorld[i].x += shift.x;
				jointWorld[i].y += shift.y;
				jointWorld[i].z += shift.z;
			}
			out.shift = shift;

			// État APRÈS correction.
			const NkVec3f comAfter = mass.ComputeCOMFromPositions(jointWorld, count);
			const NkBalanceResult balAfter = NkBalance::EvaluateStatic(comAfter, supportPts, supportCount, n);
			out.nowBalanced = balAfter.balanced;
			out.marginAfter = balAfter.margin;
			return out;
		}

		NkBalanceCorrection NkPoseBalancer::BalanceByUpperShift(NkVec3f *jointWorld, int32 count, const NkPoseMass &mass,
															   const bool *plantedMask, const NkVec3f *supportPts,
															   int32 supportCount, float32 strength, int32 maxIters,
															   const NkVec3f &groundNormal) {
			NkBalanceCorrection out;
			if (jointWorld == nullptr || count <= 0 || plantedMask == nullptr || supportPts == nullptr ||
				supportCount <= 0)
				return out;

			// Normale de sol unitaire.
			NkVec3f n = groundNormal;
			const float32 nlen = math::NkSqrt(n.x * n.x + n.y * n.y + n.z * n.z);
			if (nlen > 1e-6f) {
				n.x /= nlen;
				n.y /= nlen;
				n.z /= nlen;
			} else {
				n = NkVec3f{0.f, 1.f, 0.f};
			}

			// Masse mobile (joints NON plantés) : c'est elle qui déplace le COM.
			float32 upperMass = 0.f;
			const int32 nm = static_cast<int32>(mass.jointMass.Size());
			for (int32 i = 0; i < count && i < nm; ++i) {
				if (!plantedMask[i])
					upperMass += mass.jointMass[static_cast<uint64>(i)];
			}
			const float32 totalMass = mass.TotalMass();

			// État AVANT.
			const NkVec3f com0 = mass.ComputeCOMFromPositions(jointWorld, count);
			const NkBalanceResult balBefore = NkBalance::EvaluateStatic(com0, supportPts, supportCount, n);
			out.wasBalanced = balBefore.balanced;
			out.marginBefore = balBefore.margin;

			if (upperMass < 1e-5f || totalMass < 1e-5f) {
				// Rien de mobile → on ne peut pas corriger sans bouger les appuis.
				out.nowBalanced = balBefore.balanced;
				out.marginAfter = balBefore.margin;
				return out;
			}

			// COM voulu = com0 déplacé de `strength` vers le centroïde des appuis (horizontal).
			const NkVec3f target = Centroid(supportPts, supportCount);
			const float32 s = math::NkClamp(strength, 0.0f, 1.0f);
			NkVec3f toTarget{target.x - com0.x, target.y - com0.y, target.z - com0.z};
			toTarget = RemoveAlong(toTarget, n);
			const NkVec3f desired{com0.x + toTarget.x * s, com0.y + toTarget.y * s, com0.z + toTarget.z * s};

			// Itère : déplacer la masse mobile de S déplace le COM de (upperMass/totalMass)*S.
			// Donc pour combler `remaining`, S = remaining * totalMass/upperMass. Quasi-linéaire → converge vite.
			const float32 gain = totalMass / upperMass;
			NkVec3f applied{0.f, 0.f, 0.f};
			for (int32 iter = 0; iter < maxIters; ++iter) {
				const NkVec3f com = mass.ComputeCOMFromPositions(jointWorld, count);
				NkVec3f remaining{desired.x - com.x, desired.y - com.y, desired.z - com.z};
				remaining = RemoveAlong(remaining, n);
				const float32 rlen = math::NkSqrt(remaining.x * remaining.x + remaining.y * remaining.y +
												  remaining.z * remaining.z);
				if (rlen < 1e-5f)
					break;
				const NkVec3f step{remaining.x * gain, remaining.y * gain, remaining.z * gain};
				for (int32 j = 0; j < count; ++j) {
					if (plantedMask[j])
						continue;
					jointWorld[j].x += step.x;
					jointWorld[j].y += step.y;
					jointWorld[j].z += step.z;
				}
				applied.x += step.x;
				applied.y += step.y;
				applied.z += step.z;
			}
			out.shift = applied;

			// État APRÈS.
			const NkVec3f comAfter = mass.ComputeCOMFromPositions(jointWorld, count);
			const NkBalanceResult balAfter = NkBalance::EvaluateStatic(comAfter, supportPts, supportCount, n);
			out.nowBalanced = balAfter.balanced;
			out.marginAfter = balAfter.margin;
			return out;
		}

		void NkBalanceSmoother::Reset() {
			mHas = false;
			mPrev = NkVec3f{0.f, 0.f, 0.f};
		}

		math::NkVec3f NkBalanceSmoother::Smooth(const NkVec3f &target, float32 maxDeltaPerFrame) {
			if (!mHas) {
				mPrev = target;
				mHas = true;
				return mPrev;
			}
			NkVec3f d{target.x - mPrev.x, target.y - mPrev.y, target.z - mPrev.z};
			const float32 len = math::NkSqrt(d.x * d.x + d.y * d.y + d.z * d.z);
			const float32 cap = maxDeltaPerFrame < 0.f ? 0.f : maxDeltaPerFrame;
			if (len > cap && len > 1e-6f) {
				const float32 k = cap / len;
				d.x *= k;
				d.y *= k;
				d.z *= k;
			}
			mPrev = NkVec3f{mPrev.x + d.x, mPrev.y + d.y, mPrev.z + d.z};
			return mPrev;
		}

		bool NkPoseBalancer::SelfTest() {
			bool ok = true;

			// --- Test 1 : pose déséquilibrée (COM au-dessus, décalé en X) ramenée à l'équilibre. ---
			// Un corps simple : quelques joints hauts (masse) + 4 coins de pieds (support, masse ~0).
			{
				NkVector<NkVec3f> joints;
				// tronc/tête décalés à x = 1.0 (hors du support) : ils portent la masse.
				joints.PushBack(NkVec3f{1.0f, 1.6f, 0.0f}); // tête
				joints.PushBack(NkVec3f{1.0f, 1.0f, 0.0f}); // tronc
				joints.PushBack(NkVec3f{1.0f, 0.5f, 0.0f}); // bassin
				// 4 coins de pieds autour de l'origine (le polygone de support est centré sur x=0).
				joints.PushBack(NkVec3f{-0.15f, 0.0f, -0.10f});
				joints.PushBack(NkVec3f{0.15f, 0.0f, -0.10f});
				joints.PushBack(NkVec3f{0.15f, 0.0f, 0.10f});
				joints.PushBack(NkVec3f{-0.15f, 0.0f, 0.10f});
				const int32 n = static_cast<int32>(joints.Size());

				NkPoseMass mass;
				mass.jointMass.Clear();
				mass.jointMass.PushBack(5.0f);	// tête
				mass.jointMass.PushBack(30.0f); // tronc
				mass.jointMass.PushBack(15.0f); // bassin
				mass.jointMass.PushBack(0.01f);
				mass.jointMass.PushBack(0.01f);
				mass.jointMass.PushBack(0.01f);
				mass.jointMass.PushBack(0.01f);

				// Points de support = les 4 coins de pieds.
				NkVector<NkVec3f> support;
				support.PushBack(NkVec3f{-0.15f, 0.0f, -0.10f});
				support.PushBack(NkVec3f{0.15f, 0.0f, -0.10f});
				support.PushBack(NkVec3f{0.15f, 0.0f, 0.10f});
				support.PushBack(NkVec3f{-0.15f, 0.0f, 0.10f});

				// Avant : le COM est vers x≈0.87 → hors support.
				const NkVec3f com0 = mass.ComputeCOMFromPositions(joints.Data(), n);
				const NkBalanceResult b0 = NkBalance::EvaluateStatic(com0, support.Data(),
																	 static_cast<int32>(support.Size()));
				if (b0.balanced)
					ok = false; // doit être déséquilibré au départ

				// strength = 1 : doit ramener l'équilibre.
				NkVector<NkVec3f> j1 = joints;
				const NkBalanceCorrection c1 = NkPoseBalancer::BalanceByShift(
					j1.Data(), n, mass, support.Data(), static_cast<int32>(support.Size()), 1.0f);
				if (!c1.wasBalanced && !c1.nowBalanced)
					ok = false; // la correction pleine doit équilibrer
				if (c1.marginAfter <= c1.marginBefore)
					ok = false; // la marge doit s'améliorer

				// strength = 0 : pose inchangée (toujours déséquilibrée).
				NkVector<NkVec3f> j0 = joints;
				const NkBalanceCorrection c0 = NkPoseBalancer::BalanceByShift(
					j0.Data(), n, mass, support.Data(), static_cast<int32>(support.Size()), 0.0f);
				if (c0.nowBalanced)
					ok = false; // strength 0 ne doit RIEN corriger
				const float32 shiftLen0 = math::NkSqrt(c0.shift.x * c0.shift.x + c0.shift.y * c0.shift.y +
													  c0.shift.z * c0.shift.z);
				if (shiftLen0 > 1e-5f)
					ok = false;

				// strength = 0.5 : correction partielle (marge intermédiaire, < strength 1).
				NkVector<NkVec3f> jh = joints;
				const NkBalanceCorrection ch = NkPoseBalancer::BalanceByShift(
					jh.Data(), n, mass, support.Data(), static_cast<int32>(support.Size()), 0.5f);
				if (ch.marginAfter <= c0.marginAfter)
					ok = false; // mieux que 0
				if (ch.marginAfter >= c1.marginAfter + 1e-4f)
					ok = false; // mais pas mieux que 1
			}

			// --- Test 2 : pose DÉJÀ équilibrée → correction quasi nulle, reste équilibrée. ---
			{
				NkVector<NkVec3f> joints;
				joints.PushBack(NkVec3f{0.0f, 1.6f, 0.0f});
				joints.PushBack(NkVec3f{0.0f, 1.0f, 0.0f});
				joints.PushBack(NkVec3f{-0.15f, 0.0f, -0.10f});
				joints.PushBack(NkVec3f{0.15f, 0.0f, -0.10f});
				joints.PushBack(NkVec3f{0.15f, 0.0f, 0.10f});
				joints.PushBack(NkVec3f{-0.15f, 0.0f, 0.10f});
				const int32 n = static_cast<int32>(joints.Size());

				NkPoseMass mass;
				mass.SetUniform(n);

				NkVector<NkVec3f> support;
				support.PushBack(NkVec3f{-0.15f, 0.0f, -0.10f});
				support.PushBack(NkVec3f{0.15f, 0.0f, -0.10f});
				support.PushBack(NkVec3f{0.15f, 0.0f, 0.10f});
				support.PushBack(NkVec3f{-0.15f, 0.0f, 0.10f});

				NkVector<NkVec3f> j = joints;
				const NkBalanceCorrection c = NkPoseBalancer::BalanceByShift(
					j.Data(), n, mass, support.Data(), static_cast<int32>(support.Size()), 1.0f);
				if (!c.nowBalanced)
					ok = false;
			}

			// --- Test 3 : V2 correction PIEDS PLANTÉS — le haut du corps bouge, les appuis PAS. ---
			{
				NkVector<NkVec3f> joints;
				joints.PushBack(NkVec3f{1.0f, 1.6f, 0.0f}); // tête (penché en x=1)
				joints.PushBack(NkVec3f{1.0f, 1.0f, 0.0f}); // tronc
				joints.PushBack(NkVec3f{1.0f, 0.5f, 0.0f}); // bassin
				joints.PushBack(NkVec3f{-0.15f, 0.0f, -0.10f}); // pied (planté)
				joints.PushBack(NkVec3f{0.15f, 0.0f, -0.10f});
				joints.PushBack(NkVec3f{0.15f, 0.0f, 0.10f});
				joints.PushBack(NkVec3f{-0.15f, 0.0f, 0.10f});
				const int32 n = static_cast<int32>(joints.Size());

				NkPoseMass mass;
				mass.jointMass.Clear();
				mass.jointMass.PushBack(5.0f);
				mass.jointMass.PushBack(30.0f);
				mass.jointMass.PushBack(15.0f);
				mass.jointMass.PushBack(0.5f);
				mass.jointMass.PushBack(0.5f);
				mass.jointMass.PushBack(0.5f);
				mass.jointMass.PushBack(0.5f);

				// Les 4 derniers joints (pieds) sont plantés.
				bool planted[7] = {false, false, false, true, true, true, true};

				NkVector<NkVec3f> support;
				support.PushBack(NkVec3f{-0.15f, 0.0f, -0.10f});
				support.PushBack(NkVec3f{0.15f, 0.0f, -0.10f});
				support.PushBack(NkVec3f{0.15f, 0.0f, 0.10f});
				support.PushBack(NkVec3f{-0.15f, 0.0f, 0.10f});

				NkVector<NkVec3f> j = joints;
				const NkBalanceCorrection c = NkPoseBalancer::BalanceByUpperShift(
					j.Data(), n, mass, planted, support.Data(), static_cast<int32>(support.Size()), 1.0f);
				if (!c.nowBalanced)
					ok = false; // doit équilibrer
				if (c.marginAfter <= c.marginBefore)
					ok = false;
				// Les pieds (joints 3..6) NE doivent PAS avoir bougé.
				for (int32 k = 3; k < 7; ++k) {
					const NkVec3f a = joints[static_cast<uint64>(k)];
					const NkVec3f b = j[static_cast<uint64>(k)];
					const float32 dx = a.x - b.x;
					const float32 dy = a.y - b.y;
					const float32 dz = a.z - b.z;
					if (math::NkSqrt(dx * dx + dy * dy + dz * dz) > 1e-4f)
						ok = false;
				}
				// Le haut du corps (joint 0) DOIT avoir bougé.
				{
					const NkVec3f a = joints[0];
					const NkVec3f b = j[0];
					const float32 dx = a.x - b.x;
					if (math::NkSqrt(dx * dx) < 1e-3f)
						ok = false;
				}
			}

			// --- Test 4 : lissage multi-frame — la variation par frame est bornée. ---
			{
				NkBalanceSmoother sm;
				const NkVec3f target{1.0f, 0.0f, 0.0f};
				const NkVec3f f0 = sm.Smooth(target, 0.1f); // 1re frame : prend la cible telle quelle
				if (math::NkSqrt((f0.x - 1.0f) * (f0.x - 1.0f)) > 1e-5f)
					ok = false;
				// Nouvelle cible éloignée : la sortie ne doit se déplacer que de <= 0.1 par frame.
				const NkVec3f target2{5.0f, 0.0f, 0.0f};
				const NkVec3f f1 = sm.Smooth(target2, 0.1f);
				const float32 d = math::NkSqrt((f1.x - f0.x) * (f1.x - f0.x));
				if (d > 0.1f + 1e-5f)
					ok = false;
				if (d < 0.05f)
					ok = false; // doit quand même avancer
			}

			return ok;
		}

	} // namespace renderer
} // namespace nkentseu
