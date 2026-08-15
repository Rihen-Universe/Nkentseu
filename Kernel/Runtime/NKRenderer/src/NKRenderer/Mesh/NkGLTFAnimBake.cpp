// =============================================================================
// NKRenderer/Mesh/NkGLTFAnimBake.cpp — voir NkGLTFAnimBake.h
// -----------------------------------------------------------------------------
// Corps deplace tel quel depuis NkAnimationSystem.cpp le 2026-08-14 (methode
// NkAnimationClip::BakeFromGLTF). Seule transformation : les membres du clip,
// tous publics, sont desormais adresses via `out.` au lieu de `this->`.
// =============================================================================
#include "NKRenderer/Mesh/NkGLTFAnimBake.h"
#include "NKLogger/NkLog.h"
#include <cmath>
#include <cstring>

namespace nkentseu {
	namespace renderer {

		bool BakeClipFromGLTF(const NkGLTFMeshData &data, int32 animIdx, float32 fps, anim::NkAnimationClip &out) {
			if (!data.isSkinned || data.skinJoints.Empty()) {
				logger.Errorf("[NkGLTFAnimBake] BakeClipFromGLTF : modele non skinne\n");
				return false;
			}
			if (fps < 1.f)
				fps = 30.f;
			// Duree de l'animation (sinon 1 frame de bind pose).
			float32 dur = 0.f;
			if (animIdx >= 0 && animIdx < (int32)data.animations.Size())
				dur = data.animations[(uint32)animIdx].duration;
			const uint32 jc = (uint32)data.skinJoints.Size();

			out.boneTracks.Clear();
			out.ResizeBones(jc);
			for (uint32 b = 0; b < jc; ++b)
				out.boneTracks[b].name = NkFormat("joint_{0}", b);

			// ── Squelette : hiérarchie (parentJoint) + inverseBind + bindGlobal ──
			// bindGlobal[j] = inverse(inverseBind[j]) (= global bind cohérent avec le skin,
			// PAS EvaluateGLTFWorldJoints dont le global diffère). Sert à récupérer le
			// global animé : global = skin × bindGlobal, puis local = inv(global[parent])×global.
			NkVector<NkMat4f> bindGlobalUnused;
			EvaluateGLTFWorldJoints(data, -1, 0.f, bindGlobalUnused, out.jointParent);
			out.jointInverseBind = data.inverseBind;
			NkVector<NkMat4f> bindGlobal;
			bindGlobal.Resize(jc);
			for (uint32 j = 0; j < jc; ++j)
				bindGlobal[j] =
					(j < (uint32)data.inverseBind.Size()) ? data.inverseBind[j].Inverse() : NkMat4f::Identity();
			// topo (parent avant enfant)
			out.jointTopo.Clear();
			{
				NkVector<bool> placed;
				placed.Resize(jc);
				for (uint32 j = 0; j < jc; ++j)
					placed[j] = false;
				uint32 done = 0, g = 0;
				while (done < jc && g++ < jc + 2) {
					for (uint32 j = 0; j < jc; ++j) {
						if (placed[j])
							continue;
						int32 p = (j < (uint32)out.jointParent.Size()) ? out.jointParent[j] : -1;
						if (p < 0 || placed[(uint32)p]) {
							out.jointTopo.PushBack(j);
							placed[j] = true;
							++done;
						}
					}
				}
				for (uint32 j = 0; j < jc; ++j)
					if (!placed[j])
						out.jointTopo.PushBack(j);
			}
			out.skeletalLocal = true;

			// Echantillonne la pose en MATRICES BONE-LOCAL (relatives au parent joint).
			const uint32 frameCount = (dur > 1e-4f) ? (uint32)(dur * fps) + 1u : 1u;
			NkVector<NkMat4f> pose, global;
			global.Resize(jc);
			for (uint32 f = 0; f < frameCount; ++f) {
				float32 t = (frameCount > 1) ? (float32)f / fps : 0.f;
				if (t > dur)
					t = dur;
				if (!EvaluateGLTFPose(data, animIdx, t, pose))
					break; // skinning
				for (uint32 j = 0; j < jc && j < (uint32)pose.Size(); ++j)
					global[j] = pose[j] * bindGlobal[j];
				for (uint32 j = 0; j < jc; ++j) {
					int32 p = (j < (uint32)out.jointParent.Size()) ? out.jointParent[j] : -1;
					NkMat4f local = (p >= 0) ? (global[(uint32)p].Inverse() * global[j]) : global[j];
					out.boneTracks[j].AddKey(t, local, anim::NkInterpMode::NK_LINEAR);
				}
			}
			out.fps = fps;
			out.duration = dur;
			out.loop = true;
			out.RecalcDuration();
			logger.Info("[NkGLTFAnimBake] BakeClipFromGLTF : {0} os, {1} frames @ {2}fps, dur={3}s (LOCAL)\n", jc, frameCount,
						fps, out.duration);
			return true;
		}
	} // namespace renderer
} // namespace nkentseu
