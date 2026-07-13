// =============================================================================
// DemoAnim.cpp — NkAnima M1 : pipeline d'animation (clip + .nkanim + player)
// -----------------------------------------------------------------------------
// Prouve le ROUND-TRIP complet du système d'animation :
//   1. charge CesiumMan (glTF skinné)
//   2. BAKE son animation en NkAnimationClip éditable (keyframes par os)
//   3. SAUVE le clip en binaire .nkanim
//   4. RECHARGE depuis le .nkanim (clip neuf)  <-- preuve du format binaire
//   5. JOUE le clip rechargé via NkAnimationPlayer (Update + GetState)
//   6. applique boneMatrices -> SubmitSkinned -> le mesh s'anime
//
// Tout le système (clip/track/player/blend) est ENGINE-level -> réutilisable par
// les jeux (Noge) ET par l'app NkAnima. Le .nkanim binaire = compact + chargement
// rapide (pas de parsing JSON).
//   renderdemo --demo=16        (cf remap main.cpp)   NK_SKIN_MODEL=<chemin>
// =============================================================================
#include "DemoCommon.h"
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKRenderer/Mesh/NkGLTFMaterialBridge.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/Animation/NkAnimationSystem.h"
#include "NKRenderer/Tools/Animation/NkAnimationEditor.h"
#include "NKLogger/NkLog.h"
#include <cmath>
#include <cstdlib>

namespace nkentseu {
	namespace demo {

		using namespace nkentseu::renderer;

		struct DemoAnimState {
				NkGLTFMeshData *gltf = nullptr;
				bool loaded = false, skinned = false;
				NkMeshHandle mesh;
				NkAABB bounds;
				NkVec3f center = {0, 0, 0};
				float32 radius = 4.f;
				NkGLTFMaterialSet matSet;
				NkMatInstHandle skinMat;
				NkVector<NkMatInstHandle> matSlots;

				NkAnimationClip clip; // clip RECHARGÉ depuis .nkanim
				NkAnimationPlayer player;
				bool roundTripOK = false;
				NkString nkanimPath;
				uint32 jointCount = 0, frameCount = 0;

				// ── Blend tree 1D (si le modele a >= 2 animations, ex. Fox) ──────
				// Chaque animation glTF est bakee en clip skeletalLocal, posee a
				// la position i sur l'axe ; le parametre balaye l'axe -> on VOIT
				// les melanges (Survey<->Walk<->Run) avec phases synchronisees.
				bool blendMode = false;
				NkVector<NkAnimationClip *> blendClips;
				NkBlendTree1D blendTree;

				// ── Morph targets sur mesh SKINNE (morph applique AVANT skinning) ──
				NkVector<NkVertexSkinned> morphScratch;
				NkVector<float32> morphWeights;
		};

		static NkString PickModel() {
			const char *env = getenv("NK_SKIN_MODEL");
			if (env && env[0])
				return NkString(env);
			return NkString("Resources/Models/CesiumMan/CesiumMan.glb");
		}

		bool DemoAnim_Init(DemoCtx &ctx) {
			auto *st = new DemoAnimState();
			ctx.userData = st;

			auto *meshSys = ctx.renderer->GetMeshSystem();
			if (!meshSys) {
				logger.Errorf("[DemoAnim] MeshSystem manquant\n");
				return true;
			}

			NkString path = PickModel();
			st->gltf = new NkGLTFMeshData();
			if (!LoadGLTF(path, *st->gltf)) {
				logger.Errorf("[DemoAnim] echec glTF : %s\n", path.CStr());
				return true;
			}
			NkGLTFMeshData &data = *st->gltf;
			st->skinned = data.isSkinned && !data.skinnedVertices.Empty();
			st->bounds = data.bounds;

			// ── Mesh GPU skinné + matériaux (calque DemoSkin) ─────────────────────
			NkMeshDesc d;
			d.layout = st->skinned ? renderer::NkVertexLayout::Skinned() : renderer::NkVertexLayout::Default3D();
			// Morph targets : le VBO doit etre re-uploadable chaque frame.
			d.dynamic = data.hasMorphs;
			d.vertices = st->skinned ? (const void *)data.skinnedVertices.Data() : (const void *)data.vertices.Data();
			d.vertexCount = st->skinned ? (uint32)data.skinnedVertices.Size() : (uint32)data.vertices.Size();
			d.indices = data.indices.Data();
			d.indexCount = (uint32)data.indices.Size();
			d.subMeshes = data.subMeshes;
			d.bounds = data.bounds;
			d.debugName = data.debugName;
			st->mesh = meshSys->Create(d);
			st->loaded = st->mesh.IsValid();

			if (auto *matSys = ctx.renderer->GetMaterials()) {
				if (auto *texLib = ctx.renderer->GetTextures()) {
					BuildGLTFMaterials(data, matSys, texLib, st->matSet);
					int32 matIdx = (!data.subMeshMaterial.Empty()) ? data.subMeshMaterial[0] : -1;
					st->skinMat = st->matSet.InstanceForMaterial(matIdx);
					if (!st->skinMat.IsValid())
						for (uint32 mi = 0; mi < (uint32)st->matSet.instances.Size(); ++mi)
							if (st->matSet.instances[mi].IsValid()) {
								st->skinMat = st->matSet.instances[mi];
								break;
							}
					const uint32 nSubs = meshSys->GetSubMeshCount(st->mesh);
					st->matSlots.Resize(nSubs);
					for (uint32 si = 0; si < nSubs; ++si) {
						int32 mIdx = (si < (uint32)data.subMeshMaterial.Size()) ? data.subMeshMaterial[si] : -1;
						NkMatInstHandle h = st->matSet.InstanceForMaterial(mIdx);
						st->matSlots[si] = h.IsValid() ? h : st->skinMat;
					}
				}
			}

			// ── M1 round-trip : bake -> save .nkanim -> reload -> play ────────────
			if (st->skinned) {
				int32 animIdx = data.animations.Empty() ? -1 : 0;
				NkAnimationClip baked;
				if (baked.BakeFromGLTF(data, animIdx, 30.f)) {
					st->jointCount = (uint32)baked.boneTracks.Size();
					st->frameCount = (st->jointCount > 0) ? baked.boneTracks[0].KeyCount() : 0;
					st->nkanimPath = NkString("Build/Bin/Debug-Windows/renderdemo/cesiumman_walk.nkanim");
					bool saved = baked.SaveBinary(st->nkanimPath);
					bool loaded = saved && st->clip.LoadBinary(st->nkanimPath);
					st->roundTripOK =
						loaded && st->clip.boneTracks.Size() == baked.boneTracks.Size() &&
						(st->clip.boneTracks.Empty() || st->clip.boneTracks[0].KeyCount() == st->frameCount);
					logger.Info(
						"[DemoAnim] round-trip : bake={0} save={1} reload={2} match={3} (joints={4} frames={5})\n", 1,
						saved ? 1 : 0, loaded ? 1 : 0, st->roundTripOK ? 1 : 0, st->jointCount, st->frameCount);
					if (loaded) {
						st->player.SetClip(&st->clip);
						st->player.Play(NkPlayMode::NK_LOOP, 1.f);
					}
				}

				// ── Blend tree 1D si le modele a PLUSIEURS animations (ex. Fox :
				// Survey/Walk/Run). Bake chaque animation en clip skeletalLocal
				// et le pose a la position i (0, 1, 2, ...).
				if (data.animations.Size() >= 2) {
					for (uint32 ai = 0; ai < (uint32)data.animations.Size(); ++ai) {
						auto *c = memory::NkGetDefaultAllocator().New<NkAnimationClip>();
						if (c->BakeFromGLTF(data, (int32)ai, 30.f)) {
							c->name = data.animations[ai].name;
							st->blendClips.PushBack(c);
							st->blendTree.AddClip(c, (float32)(st->blendClips.Size() - 1));
						} else {
							memory::NkGetDefaultAllocator().Delete(c);
						}
					}
					st->blendMode = st->blendClips.Size() >= 2;
					if (st->blendMode)
						logger.Info("[DemoAnim] BLEND TREE 1D : {0} clips sur l'axe 0..{1}\n",
									(uint32)st->blendClips.Size(), (uint32)st->blendClips.Size() - 1);

					// Self-test state machine (NK_ANIM_SMTEST) : 2 etats + transitions
					// pilotees par un float, verifie declenchement + fin de fondu.
					if (getenv("NK_ANIM_SMTEST") && st->blendClips.Size() >= 2) {
						NkAnimStateMachine sm;
						int32 sIdle = sm.AddState(NkString("idle"), st->blendClips[0]);
						int32 sWalk = sm.AddState(NkString("walk"), st->blendClips[1]);
						sm.AddTransition(sIdle, sWalk, NkString("speed"),
										 NkAnimStateMachine::NkCondKind::FLOAT_GREATER, 0.5f, 0.2f);
						sm.AddTransition(sWalk, sIdle, NkString("speed"),
										 NkAnimStateMachine::NkCondKind::FLOAT_LESS, 0.2f, 0.2f);
						// Evenements de transition : compte les debuts et fins.
						int32 evStart = 0;
						int32 evEnd = 0;
						sm.SetTransitionCallback([&evStart, &evEnd](const NkString &, const NkString &, bool fin) {
							if (fin)
								evEnd++;
							else
								evStart++;
						});
						sm.Update(0.016f);
						const bool ok1 = sm.GetCurrentState() == sIdle;
						sm.SetFloat(NkString("speed"), 1.f);
						for (int i = 0; i < 20; i++)
							sm.Update(0.016f); // 0.32 s > fadeDur 0.2 s
						const bool ok2 = sm.GetCurrentState() == sWalk && !sm.GetState().boneMatrices.Empty();
						sm.SetFloat(NkString("speed"), 0.f);
						for (int i = 0; i < 20; i++)
							sm.Update(0.016f);
						const bool ok3 = sm.GetCurrentState() == sIdle;
						const bool okEv = (evStart == 2 && evEnd == 2);
						logger.Info("[DemoAnim] SM self-test : start_idle={0} vers_walk={1} retour_idle={2} "
									"events(start={3} end={4} ok={5})\n",
									ok1 ? 1 : 0, ok2 ? 1 : 0, ok3 ? 1 : 0, evStart, evEnd, okEv ? 1 : 0);

						// Self-test blend 2D : 3 clips a 3 points, parametre au
						// barycentre -> pose melangee non vide ; hit exact -> pur.
						NkBlendTree2D bt2;
						bt2.AddClip(st->blendClips[0], {0.f, 0.f});
						bt2.AddClip(st->blendClips[1], {1.f, 0.f});
						bt2.AddClip(st->blendClips.Size() >= 3 ? st->blendClips[2] : st->blendClips[1], {0.f, 1.f});
						bt2.SetParameter({0.4f, 0.3f});
						bt2.Update(0.016f);
						const bool ok2d1 = !bt2.GetState().boneMatrices.Empty() && !bt2.GetLocalPose().Empty();
						bt2.SetParameter({0.f, 0.f}); // hit exact clip 0
						bt2.Update(0.016f);
						const bool ok2d2 = !bt2.GetState().boneMatrices.Empty();
						logger.Info("[DemoAnim] BLEND2D self-test : mix={0} exact={1}\n", ok2d1 ? 1 : 0,
									ok2d2 ? 1 : 0);
					}
				}
			}

			// ── Cadre caméra sur le mesh skinné (pose initiale) ───────────────────
			if (st->skinned && st->clip.boneTracks.Size() > 0) {
				st->player.Update(0.f);
				const NkVector<NkMat4f> &bones = st->player.GetState().boneMatrices;
				if (!bones.Empty() && !data.skinnedVertices.Empty()) {
					NkAABB sk = NkAABB::Empty();
					for (uint32 vi = 0; vi < (uint32)data.skinnedVertices.Size(); ++vi) {
						const NkVertexSkinned &sv = data.skinnedVertices[vi];
						NkMat4f m;
						for (int e = 0; e < 16; e++)
							m.data[e] = 0.f;
						float32 wsum = 0.f;
						for (int b = 0; b < 4; b++) {
							int j = (int)(sv.boneIdx[b] + 0.5f);
							float32 w = sv.boneWeight[b];
							if (j >= 0 && j < (int)bones.Size() && w > 0.f) {
								for (int e = 0; e < 16; e++)
									m.data[e] += w * bones[(uint32)j].data[e];
								wsum += w;
							}
						}
						if (wsum < 1e-4f)
							m = NkMat4f::Identity();
						NkVec4f wp = m * NkVec4f{sv.pos.x, sv.pos.y, sv.pos.z, 1.f};
						sk.Expand(NkVec3f{wp.x, wp.y, wp.z});
					}
					st->bounds = sk;
				}
			}
			NkVec3f mn = st->bounds.min, mx = st->bounds.max;
			st->center = {(mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f};
			NkVec3f ext = {(mx.x - mn.x) * 0.5f, (mx.y - mn.y) * 0.5f, (mx.z - mn.z) * 0.5f};
			float32 diag = sqrtf(ext.x * ext.x + ext.y * ext.y + ext.z * ext.z);
			st->radius = (diag > 1e-4f) ? diag * 2.2f : 4.f;
			if (st->radius < 1.5f)
				st->radius = 1.5f;

			// Test round-trip math (NK_ANIM_RTTEST) : isole le bug slerp.
			if (getenv("NK_ANIM_RTTEST") && st->clip.boneTracks.Size() > 1) {
				const auto &tr = st->clip.boneTracks[1];
				if (tr.KeyCount() > 0) {
					NkMat4f M = tr.GetKey(tr.KeyCount() / 2).value;
					NkVec3f tt, ss;
					NkMat4f rr;
					M.DecomposeTRS(tt, rr, ss);
					NkQuatf q(rr);
					NkMat4f rmat = q.ToMat4();
					NkMat4f recomp = NkMat4f::Translate(tt) * rmat * NkMat4f::Scale(ss);
					float32 errQuatMat = 0.f, errRecomp = 0.f;
					for (int i = 0; i < 16; i++) {
						errQuatMat = NkMax(errQuatMat, fabsf(rmat.data[i] - rr.data[i]));
						errRecomp = NkMax(errRecomp, fabsf(recomp.data[i] - M.data[i]));
					}
					logger.Info("[RTTEST] scale=({0},{1},{2})  quat->mat vs rot err={3}  decompose+recompose err={4}\n",
								ss.x, ss.y, ss.z, errQuatMat, errRecomp);
					// Scan TOUS les os : cherche réflexion (det<0) ou scale anormal qui
					// ferait échouer DecomposeTRS+quat (suppose rotation propre det=+1).
					float32 worstScale = 1.f, worstDet = 1.f;
					uint32 worstBone = 0;
					float32 worstQ = 1.f;
					for (uint32 bi = 0; bi < (uint32)st->clip.boneTracks.Size(); ++bi) {
						const auto &trb = st->clip.boneTracks[bi];
						if (trb.KeyCount() == 0)
							continue;
						NkMat4f Mm = trb.GetKey(trb.KeyCount() / 2).value;
						NkVec3f tm, sm;
						NkMat4f rm;
						Mm.DecomposeTRS(tm, rm, sm);
						// det de la rotation 3x3 (col0·(col1×col2))
						NkVec3f c0{rm.m00, rm.m01, rm.m02}, c1{rm.m10, rm.m11, rm.m12}, c2{rm.m20, rm.m21, rm.m22};
						float32 det = c0.x * (c1.y * c2.z - c1.z * c2.y) - c1.x * (c0.y * c2.z - c0.z * c2.y) +
									  c2.x * (c0.y * c1.z - c0.z * c1.y);
						NkQuatf qq2(rm);
						float32 ql2 = sqrtf(qq2.x * qq2.x + qq2.y * qq2.y + qq2.z * qq2.z + qq2.w * qq2.w);
						float32 maxs = NkMax(NkMax(fabsf(sm.x), fabsf(sm.y)), fabsf(sm.z));
						if (maxs > worstScale || det < worstDet || fabsf(ql2 - 1.f) > fabsf(worstQ - 1.f)) {
							if (maxs > worstScale)
								worstScale = maxs;
							if (det < worstDet)
								worstDet = det;
							if (fabsf(ql2 - 1.f) > fabsf(worstQ - 1.f))
								worstQ = ql2;
							worstBone = bi;
						}
					}
					logger.Info("[RTTEST2] scan {0} os : worstScale={1} worstDet={2} worstQuatLen={3} (bone {4})\n",
								(uint32)st->clip.boneTracks.Size(), worstScale, worstDet, worstQ, worstBone);
				}
			}

			// Self-test de l'éditeur de timeline (NK_ANIM_EDITTEST), SANS UI : insert/
			// delete/move + undo/redo sur le clip, puis restaure (doit revenir à l'initial).
			if (getenv("NK_ANIM_EDITTEST") && st->clip.boneTracks.Size() > 0) {
				NkAnimationEditor ed;
				ed.SetClip(&st->clip);
				uint32 n0 = ed.PoseKeyCount();
				float32 tIns = 0.5123f;
				ed.SetCursor(tIns);
				NkVector<NkMat4f> pose;
				pose.Resize(st->clip.boneTracks.Size());
				for (uint32 b = 0; b < (uint32)st->clip.boneTracks.Size(); ++b)
					pose[b] = st->clip.boneTracks[b].Evaluate(tIns);
				ed.InsertPoseKey(pose);
				uint32 n1 = ed.PoseKeyCount();
				ed.DeletePoseKeyAt(ed.GetCursor());
				uint32 n2 = ed.PoseKeyCount();
				ed.Undo();
				uint32 n3 = ed.PoseKeyCount(); // re-insert
				ed.Undo();
				uint32 n4 = ed.PoseKeyCount(); // annule insert
				ed.Redo();
				uint32 n5 = ed.PoseKeyCount(); // refait insert
				// test move : déplace la 1re clé puis annule
				NkVector<float32> tk = ed.GetPoseKeyTimes();
				float32 t1m = tk.Size() > 1 ? tk[1] : 0.f;
				ed.MovePoseKey(t1m, t1m + 0.013f);
				int32 movedOk = (ed.GetClip()->boneTracks[0].FindKeyAtTime(t1m + 0.013f, 1e-4f) >= 0) ? 1 : 0;
				ed.Undo();
				while (ed.CanUndo())
					ed.Undo(); // restaure le clip
				uint32 nF = ed.PoseKeyCount();
				logger.Info("[EDITTEST] keys init={0} +ins={1} +del={2} undoDel={3} undoIns={4} redoIns={5} | move "
							"ok={6} | restored={7} (==init:{8})\n",
							n0, n1, n2, n3, n4, n5, movedOk, nF, (nF == n0) ? 1 : 0);
			}

			logger.Info("[DemoAnim] Init '{0}' : skinned={1} mesh={2} roundTrip={3}\n", path.CStr(),
						st->skinned ? 1 : 0, st->loaded ? 1 : 0, st->roundTripOK ? 1 : 0);
			return true;
		}

		void DemoAnim_Frame(DemoCtx &ctx, float32 dt) {
			auto *st = (DemoAnimState *)ctx.userData;
			if (!st)
				return;
			if (!ctx.renderer->BeginFrame())
				return;
			auto *r3d = ctx.renderer->GetRender3D();
			if (!r3d) {
				ctx.renderer->Present();
				ctx.renderer->EndFrame();
				return;
			}

			// ── Avance le player (clip rechargé du .nkanim) ───────────────────────
			st->player.Update(dt);

			// ── Morphs sur mesh SKINNE : deltas appliques AVANT le skinning ──────
			// (le shader skin fait ensuite le LBS normalement — bones preserves).
			if (st->gltf && st->gltf->hasMorphs && st->skinned && st->mesh.IsValid()) {
				const int32 mAnim = st->gltf->animations.Empty() ? -1 : 0;
				if (EvaluateGLTFMorphWeights(*st->gltf, mAnim, ctx.totalTime, st->morphWeights) &&
					ApplyGLTFMorphCPUSkinned(*st->gltf, st->morphWeights.Data(), (uint32)st->morphWeights.Size(),
											 st->morphScratch)) {
					if (auto *meshSys = ctx.renderer->GetMeshSystem())
						meshSys->UpdateVertices(st->mesh, st->morphScratch.Data(), (uint32)st->morphScratch.Size());
				}
			}

			// ── Blend tree : balaye le parametre 0..N-1 (triangle 8 s) ────────────
			// On VOIT le fondu continu entre les clips (ex. Fox Survey->Walk->Run
			// puis retour), phases synchronisees par le blend tree.
			if (st->blendMode) {
				const float32 nspan = (float32)(st->blendClips.Size() - 1);
				const float32 cyc = fmodf(ctx.totalTime * 0.25f, 2.f); // 0..2
				const float32 tri = (cyc < 1.f) ? cyc : (2.f - cyc);   // triangle 0..1..0
				st->blendTree.SetParameter(tri * nspan);
				st->blendTree.Update(dt);
			}

			// ── Caméra orbitale ───────────────────────────────────────────────────
			NkCamera3DData camData;
			const float32 a = ctx.totalTime * 0.4f;
			camData.position = {st->center.x + sinf(a) * st->radius, st->center.y + st->radius * 0.12f,
								st->center.z + cosf(a) * st->radius};
			camData.target = st->center;
			camData.up = {0, 1, 0};
			camData.fovY = 50.f;
			camData.aspect = (float32)ctx.width / (float32)NkMax(1u, ctx.height);
			camData.nearPlane = 0.02f;
			camData.farPlane = NkMax(50.f, st->radius * 8.f);
			NkCamera3D cam(camData);

			NkSceneContext sctx;
			sctx.camera = cam;
			sctx.time = ctx.totalTime;
			NkLightDesc sun;
			sun.type = NkLightType::NK_DIRECTIONAL;
			sun.direction = {-0.4f, -0.8f, -0.5f};
			sun.color = {1.f, 0.97f, 0.92f};
			sun.intensity = 2.2f;
			sun.castShadow = true;
			sctx.lights.PushBack(sun);
			NkLightDesc fill;
			fill.type = NkLightType::NK_DIRECTIONAL;
			fill.direction = {0.5f, -0.3f, 0.6f};
			fill.color = {0.6f, 0.7f, 0.9f};
			fill.intensity = 0.8f;
			sctx.lights.PushBack(fill);
			sctx.ambientIntensity = 0.45f;
			r3d->BeginScene(sctx);

			// Sol
			if (auto *meshSys = ctx.renderer->GetMeshSystem()) {
				float32 floorY = st->bounds.min.y - (st->bounds.max.y - st->bounds.min.y) * 0.005f;
				float32 s = NkMax(2.f, st->radius);
				NkDrawCall3D dc;
				dc.mesh = meshSys->GetPlane();
				dc.transform = NkMat4f::Translate({st->center.x, floorY, st->center.z}) * NkMat4f::Scale({s, 1.f, s});
				dc.aabb = {{st->center.x - s, floorY - 0.01f, st->center.z - s},
						   {st->center.x + s, floorY + 0.01f, st->center.z + s}};
				dc.tint = {0.18f, 0.18f, 0.22f};
				dc.roughness = 0.95f;
				r3d->Submit(dc);
			}

			// ── Mesh skinné : pose du BLEND TREE si actif, sinon du player ───────
			bool submitted = false;
			if (st->loaded && st->skinned && (st->blendMode || st->clip.boneTracks.Size() > 0)) {
				const NkVector<NkMat4f> &bones =
					st->blendMode ? st->blendTree.GetState().boneMatrices : st->player.GetState().boneMatrices;
				if (!bones.Empty()) {
					NkDrawCallSkinned dc;
					dc.mesh = st->mesh;
					dc.transform = NkMat4f::Identity();
					dc.boneMatrices = bones;
					dc.material = st->skinMat;
					dc.materialSlots = st->matSlots;
					dc.tint = st->skinMat.IsValid() ? NkVec3f{1, 1, 1} : NkVec3f{0.85f, 0.55f, 0.45f};
					dc.aabb = st->bounds;
					dc.castShadow = true;
					r3d->SubmitSkinned(dc);
					submitted = true;
				}
			}

			if (auto *overlay = ctx.renderer->GetOverlay()) {
				overlay->BeginOverlay(ctx.renderer->GetCmd(), ctx.width, ctx.height);
				overlay->DrawText({20.f, 35.f}, "DemoAnim (NkAnima M1)  |  API : %s", NkGraphicsApiName(ctx.api));
				overlay->DrawText({20.f, 55.f}, "glTF -> bake -> .nkanim -> RELOAD -> player -> skin");
				overlay->DrawText({20.f, 75.f}, "round-trip:%d  joints:%u  frames:%u  t=%.2fs", st->roundTripOK ? 1 : 0,
								  st->jointCount, st->frameCount, st->player.GetTime());
				overlay->DrawText({20.f, 95.f}, "submitted:%d  FPS:%.0f", submitted ? 1 : 0,
								  dt > 1e-5f ? 1.f / dt : 0.f);
				if (st->blendMode) {
					const uint32 nc = (uint32)st->blendClips.Size();
					const float32 p = st->blendTree.GetParameter();
					const uint32 lo = (uint32)p < nc ? (uint32)p : nc - 1;
					const uint32 hi = lo + 1 < nc ? lo + 1 : lo;
					overlay->DrawText({20.f, 115.f}, "BLEND TREE 1D : param=%.2f  [%s <-> %s]", p,
									  st->blendClips[lo]->name.CStr(), st->blendClips[hi]->name.CStr());
				}
				overlay->EndOverlay();
			}

			ctx.renderer->Present();
			ctx.renderer->EndFrame();
		}

		void DemoAnim_Shutdown(DemoCtx &ctx) {
			auto *st = (DemoAnimState *)ctx.userData;
			if (st) {
				for (auto *c : st->blendClips)
					if (c)
						memory::NkGetDefaultAllocator().Delete(c);
				st->blendClips.Clear();
				delete st->gltf;
				delete st;
			}
			ctx.userData = nullptr;
		}

	} // namespace demo
} // namespace nkentseu
