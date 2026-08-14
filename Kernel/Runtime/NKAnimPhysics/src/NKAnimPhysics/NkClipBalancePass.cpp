// =============================================================================
// NKAnimPhysics/NkClipBalancePass.cpp — M3.6 : pont anim (voir .h).
// =============================================================================
#include "NKAnimPhysics/NkClipBalancePass.h"
#include "NKAnimPhysics/NkPoseBalancer.h"
#include "NKAnimPhysics/NkBalance.h"

namespace nkentseu {
	namespace animphys {

		using math::NkVec3f;

		int32 NkClipBalancePass::Correct(const NkVec3f *posesIn, int32 frameCount, int32 jointCount,
										const NkPoseMass &mass, const bool *plantedMask, const NkVec3f *supportPts,
										int32 supportCount, float32 strength, float32 smoothMaxDeltaPerFrame,
										NkVector<NkVec3f> &posesOut, const NkVec3f &groundNormal) {
			if (posesIn == nullptr || frameCount <= 0 || jointCount <= 0)
				return 0;
			posesOut.Resize(static_cast<uint64>(frameCount) * static_cast<uint64>(jointCount));

			NkBalanceSmoother smoother;
			NkVector<NkVec3f> tmp;
			tmp.Resize(static_cast<uint64>(jointCount));
			int32 balancedFrames = 0;

			for (int32 f = 0; f < frameCount; ++f) {
				const NkVec3f *frameIn = posesIn + static_cast<uint64>(f) * static_cast<uint64>(jointCount);

				// 1) Décalage-cible de cette frame (ce que le correcteur pieds-plantés VOUDRAIT appliquer).
				for (int32 j = 0; j < jointCount; ++j)
					tmp[static_cast<uint64>(j)] = frameIn[j];
				NkBalanceCorrection corr;
				if (plantedMask != nullptr) {
					corr = NkPoseBalancer::BalanceByUpperShift(tmp.Data(), jointCount, mass, plantedMask, supportPts,
															   supportCount, strength, 6, groundNormal);
				} else {
					corr = NkPoseBalancer::BalanceByShift(tmp.Data(), jointCount, mass, supportPts, supportCount,
														  strength, groundNormal);
				}
				const NkVec3f targetShift = corr.shift;

				// 2) Lissage temporel du décalage (anti-à-coups entre frames).
				const NkVec3f applied = smoother.Smooth(targetShift, smoothMaxDeltaPerFrame);

				// 3) Écrit la frame corrigée = frame d'origine + décalage lissé sur les joints NON plantés.
				NkVec3f *frameOut = posesOut.Data() + static_cast<uint64>(f) * static_cast<uint64>(jointCount);
				for (int32 j = 0; j < jointCount; ++j) {
					NkVec3f p = frameIn[j];
					const bool planted = (plantedMask != nullptr) && plantedMask[j];
					if (!planted) {
						p.x += applied.x;
						p.y += applied.y;
						p.z += applied.z;
					}
					frameOut[j] = p;
				}

				// 4) Comptage : cette frame corrigée est-elle équilibrée ?
				if (supportPts != nullptr && supportCount > 0) {
					const NkVec3f com = mass.ComputeCOMFromPositions(frameOut, jointCount);
					const NkBalanceResult bal = NkBalance::EvaluateStatic(com, supportPts, supportCount, groundNormal);
					if (bal.balanced)
						++balancedFrames;
				}
			}
			return balancedFrames;
		}

		bool NkClipBalancePass::SelfTest() {
			bool ok = true;

			// Clip : le personnage se penche de plus en plus (le haut du corps dérive en +x),
			// pieds plantés à l'origine. Sans correction, les dernières frames basculent.
			const int32 jointCount = 7;
			const int32 frameCount = 6;
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

			NkVector<NkVec3f> clip;
			clip.Resize(static_cast<uint64>(frameCount) * static_cast<uint64>(jointCount));
			for (int32 f = 0; f < frameCount; ++f) {
				const float32 lean = static_cast<float32>(f) * 0.2f; // 0, .2, .4, .6, .8, 1.0
				NkVec3f *fr = clip.Data() + static_cast<uint64>(f) * static_cast<uint64>(jointCount);
				fr[0] = NkVec3f{lean, 1.6f, 0.0f};
				fr[1] = NkVec3f{lean, 1.0f, 0.0f};
				fr[2] = NkVec3f{lean, 0.5f, 0.0f};
				fr[3] = NkVec3f{-0.15f, 0.0f, -0.10f};
				fr[4] = NkVec3f{0.15f, 0.0f, -0.10f};
				fr[5] = NkVec3f{0.15f, 0.0f, 0.10f};
				fr[6] = NkVec3f{-0.15f, 0.0f, 0.10f};
			}

			// Compte les frames équilibrées AVANT correction.
			int32 balBefore = 0;
			for (int32 f = 0; f < frameCount; ++f) {
				const NkVec3f *fr = clip.Data() + static_cast<uint64>(f) * static_cast<uint64>(jointCount);
				const NkVec3f com = mass.ComputeCOMFromPositions(fr, jointCount);
				if (NkBalance::EvaluateStatic(com, support.Data(), sc).balanced)
					++balBefore;
			}

			// (a) Correction sans lissage (grande borne) : toutes les frames doivent devenir équilibrées.
			NkVector<NkVec3f> out;
			const int32 balAfter = NkClipBalancePass::Correct(clip.Data(), frameCount, jointCount, mass, planted,
															 support.Data(), sc, 1.0f, 1000.0f, out);
			if (balAfter <= balBefore)
				ok = false;
			if (balAfter != frameCount)
				ok = false; // sans lissage, chaque frame est ramenée en équilibre

			// Les pieds ne doivent jamais avoir bougé.
			for (int32 f = 0; f < frameCount; ++f) {
				const NkVec3f *fi = clip.Data() + static_cast<uint64>(f) * static_cast<uint64>(jointCount);
				const NkVec3f *fo = out.Data() + static_cast<uint64>(f) * static_cast<uint64>(jointCount);
				for (int32 k = 3; k < 7; ++k) {
					const float32 dx = fi[k].x - fo[k].x;
					const float32 dy = fi[k].y - fo[k].y;
					const float32 dz = fi[k].z - fo[k].z;
					if (math::NkSqrt(dx * dx + dy * dy + dz * dz) > 1e-4f)
						ok = false;
				}
			}

			// (b) Correction avec lissage serré : le décalage appliqué varie peu entre frames consécutives.
			NkVector<NkVec3f> outS;
			NkClipBalancePass::Correct(clip.Data(), frameCount, jointCount, mass, planted, support.Data(), sc, 1.0f,
									  0.05f, outS);
			// décalage appliqué à la frame f = (out[f].joint0 - clip[f].joint0) sur x (joint mobile).
			float32 prevShift = 0.f;
			bool first = true;
			for (int32 f = 0; f < frameCount; ++f) {
				const NkVec3f *fi = clip.Data() + static_cast<uint64>(f) * static_cast<uint64>(jointCount);
				const NkVec3f *fo = outS.Data() + static_cast<uint64>(f) * static_cast<uint64>(jointCount);
				const float32 shift = fo[0].x - fi[0].x;
				if (!first) {
					if (math::NkSqrt((shift - prevShift) * (shift - prevShift)) > 0.05f + 1e-4f)
						ok = false; // variation bornée par le lissage
				}
				prevShift = shift;
				first = false;
			}

			return ok;
		}

	} // namespace animphys
} // namespace nkentseu
