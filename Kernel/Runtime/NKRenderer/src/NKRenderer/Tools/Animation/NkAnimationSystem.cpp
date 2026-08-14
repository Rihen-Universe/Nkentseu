// =============================================================================
// NKRenderer/Tools/Animation/NkAnimationSystem.cpp — voir le .h
// -----------------------------------------------------------------------------
// Facade de rendu seule depuis le 2026-08-14. Le modele est dans NKAnimation.
// =============================================================================
#include "NkAnimationSystem.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKMemory/NkAllocator.h"
#include "NKLogger/NkLog.h"
#include <cmath>
#include <cstring>

namespace nkentseu {
	namespace renderer {

		// =========================================================================
		// NkAnimationSystem
		// =========================================================================
		NkAnimationSystem::~NkAnimationSystem() {
			Shutdown();
		}

		bool NkAnimationSystem::Init(NkIDevice *device, NkRender3D *r3d) {
			mDevice = device;
			mR3D = r3d;
			NkComputePipelineDesc pd;
			pd.debugName = "MorphTargets";
			mMorphCompute = mDevice->CreateComputePipeline(pd);
			return true;
		}

		void NkAnimationSystem::Shutdown() {
			for (auto *p : mPlayers)
				memory::NkGetDefaultAllocator().Delete(p);
			mPlayers.Clear();
			for (auto *c : mClips)
				memory::NkGetDefaultAllocator().Delete(c);
			mClips.Clear();
		}

		void NkAnimationSystem::Update(float32 dt) {
			for (auto *p : mPlayers)
				if (p)
					p->Update(dt);
		}

		// ── Clips ─────────────────────────────────────────────────────────────────
		anim::NkAnimationClip *NkAnimationSystem::CreateClip(const NkString &name) {
			auto *c = memory::NkGetDefaultAllocator().New<anim::NkAnimationClip>();
			c->name = name;
			mClips.PushBack(c);
			return c;
		}

		anim::NkAnimationClip *NkAnimationSystem::FindClip(const NkString &name) const {
			for (auto *c : mClips)
				if (c && c->name == name)
					return c;
			return nullptr;
		}

		void NkAnimationSystem::DestroyClip(anim::NkAnimationClip *&c) {
			for (uint32 i = 0; i < (uint32)mClips.Size(); i++) {
				if (mClips[i] == c) {
					memory::NkGetDefaultAllocator().Delete(c);
					mClips.RemoveAt(i);
					break;
				}
			}
			c = nullptr;
		}

		// ── Players ───────────────────────────────────────────────────────────────
		anim::NkAnimationPlayer *NkAnimationSystem::CreatePlayer(const NkString &name) {
			auto *p = memory::NkGetDefaultAllocator().New<anim::NkAnimationPlayer>();
			p->name = name;
			mPlayers.PushBack(p);
			return p;
		}

		anim::NkAnimationPlayer *NkAnimationSystem::FindPlayer(const NkString &name) const {
			for (auto *p : mPlayers)
				if (p && p->name == name)
					return p;
			return nullptr;
		}

		void NkAnimationSystem::DestroyPlayer(anim::NkAnimationPlayer *&p) {
			for (uint32 i = 0; i < (uint32)mPlayers.Size(); i++) {
				if (mPlayers[i] == p) {
					memory::NkGetDefaultAllocator().Delete(p);
					mPlayers.RemoveAt(i);
					break;
				}
			}
			p = nullptr;
		}

		// ── Application au renderer ───────────────────────────────────────────────
		void NkAnimationSystem::ApplySkinnedMesh(NkMeshHandle mesh, NkMatInstHandle mat, const NkMat4f &baseWorld,
												 const anim::NkAnimationPlayer &player) {
			if (!mR3D)
				return;
			const auto &s = player.GetState();
			NkDrawCallSkinned dc;
			dc.mesh = mesh;
			dc.material = mat;
			dc.transform = baseWorld * s.transform;
			dc.boneMatrices = s.boneMatrices;
			dc.tint = {s.albedo.x, s.albedo.y, s.albedo.z};
			dc.alpha = s.opacity;
			mR3D->SubmitSkinned(dc);
		}

		void NkAnimationSystem::ApplyOnionSkin(NkMeshHandle mesh, NkMatInstHandle mat, const NkMat4f &baseWorld,
											   const anim::NkAnimationPlayer &player, const int32 *offsets, uint32 ghostCount,
											   NkVec4f pastCol, NkVec4f futureCol) {
			if (!mR3D || !player.GetClip())
				return;
			const anim::NkAnimationClip *clip = player.GetClip();
			mTmpBones.Resize(clip->boneCount);

			for (uint32 g = 0; g < ghostCount; g++) {
				float32 t = player.GetTime() + (float32)offsets[g] / fmaxf(clip->fps, 1.f);
				// Évaluer les bones à ce temps
				for (uint32 b = 0; b < clip->boneCount; b++) {
					if (b < (uint32)clip->boneTracks.Size() && !clip->boneTracks[b].Empty())
						mTmpBones[b] = clip->boneTracks[b].Evaluate(t);
					else
						mTmpBones[b] = NkMat4f::Identity();
				}
				NkVec4f col = (offsets[g] < 0) ? pastCol : futureCol;
				float32 dist = (float32)abs(offsets[g]) / (float32)(ghostCount / 2 + 1);
				col.w *= (1.f - dist * 0.5f);

				NkDrawCallSkinned dc;
				dc.mesh = mesh;
				dc.material = mat;
				dc.transform = baseWorld;
				dc.boneMatrices = mTmpBones;
				dc.tint = {col.x, col.y, col.z};
				dc.alpha = col.w;
				mR3D->SubmitSkinnedTinted(dc, {col.x, col.y, col.z}, col.w);
			}
			// Frame courant par-dessus
			ApplySkinnedMesh(mesh, mat, baseWorld, player);
		}

		NkMat4f NkAnimationSystem::ApplyTransform(const anim::NkAnimationPlayer &player, const NkMat4f &base) {
			return base * player.GetState().transform;
		}

		void NkAnimationSystem::ApplyMaterial(NkMaterialInstance *inst, const anim::NkAnimationPlayer &player) {
			if (!inst)
				return;
			const auto &s = player.GetState();
			inst->SetAlbedo({s.albedo.x, s.albedo.y, s.albedo.z}, s.albedo.w)
				->SetEmissive({s.emissive.x, s.emissive.y, s.emissive.z}, s.emissiveStrength)
				->SetMetallic(s.metallic)
				->SetRoughness(s.roughness);
		}

		void NkAnimationSystem::ApplyCamera(NkCamera3D &cam, const anim::NkAnimationPlayer &player) {
			const auto &s = player.GetState();
			cam.SetPosition(s.camPos);
			cam.SetTarget(s.camTarget);
			cam.SetFOV(s.camFOV);
		}

		void NkAnimationSystem::ApplyLight(NkLightDesc &light, const anim::NkAnimationPlayer &player) {
			const auto &s = player.GetState();
			light.intensity = s.lightIntensity;
			light.color = s.lightColor;
			light.position = s.lightPos;
			light.range = s.lightRange;
		}

		void NkAnimationSystem::ApplyPostProcess(NkPostConfig &pp, const anim::NkAnimationPlayer &player) {
			const auto &s = player.GetState();
			pp.exposure = s.ppExposure;
			pp.saturation = s.ppSaturation;
			pp.contrast = s.ppContrast;
			pp.bloomStrength = s.ppBloom;
			pp.dofFocusDist = s.ppDOFFocus;
			pp.vignetteIntens = s.ppVignette;
		}

		NkVec4f NkAnimationSystem::GetAnimatedSpriteUV(const anim::NkAnimationPlayer &player) {
			return player.GetState().spriteUV;
		}

		void NkAnimationSystem::DrawSkeleton(const anim::NkAnimationPlayer &player, const NkMat4f &baseWorld,
											 const int32 *parentIdx, NkVec4f boneColor, float32 boneSize) {
			if (!mR3D)
				return;
			const auto &s = player.GetState();
			uint32 n = (uint32)s.boneMatrices.Size();
			for (uint32 i = 0; i < n; i++) {
				NkMat4f bw = baseWorld * s.boneMatrices[i];
				NkVec3f pos = {bw[3][0], bw[3][1], bw[3][2]};
				mR3D->DrawDebugSphere(pos, boneSize, boneColor);
				if (parentIdx && parentIdx[i] >= 0) {
					int32 pi = parentIdx[i];
					NkMat4f pw = baseWorld * s.boneMatrices[pi];
					NkVec3f ppos = {pw[3][0], pw[3][1], pw[3][2]};
					mR3D->DrawDebugLine(pos, ppos, boneColor);
				}
				mR3D->DrawDebugAxes(bw, boneSize * 2.f);
			}
		}

		NkMeshHandle NkAnimationSystem::ApplyMorphTargets(NkMeshHandle base, const NkMeshHandle *targets,
														  const float32 *weights, uint32 count, bool useGPU) {
			// Cette API handle-par-target est INADAPTEE aux morph targets reels
			// (glTF = DELTAS par vertex, pas des meshes complets). Le VRAI chemin
			// livre (2026-07-13) est dans NkGLTFLoader :
			//   EvaluateGLTFMorphWeights(data, anim, t, w)  — poids animes (canaux WEIGHTS)
			//   ApplyGLTFMorphCPU(data, w, n, outVerts)     — base + somme(w_i * delta_i)
			//   puis NkMeshSystem::UpdateVertices(mesh cree dynamic=true).
			// Reference d'integration : DemoGLTF.cpp (asset MorphTest). GPU compute = futur.
			(void)targets;
			(void)weights;
			(void)count;
			(void)useGPU;
			return base;
		}

	} // namespace renderer
} // namespace nkentseu
