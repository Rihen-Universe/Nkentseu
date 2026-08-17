// =============================================================================
// AnimBridge.cpp — implémentation du pont. SEUL TU à inclure NKRenderer (anim).
// =============================================================================
#include "NKRenderer/Mesh/NkGLTFAnimBake.h"
#include "AnimBridge.h"
#include "NKAnimation/NkAnimation.h"
#include "NKAnimation/NkAnimationEditor.h"
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKRenderer/Mesh/NkFBXLoader.h" // routage .fbx (chantier FBX, 2026-08-17)
// ── Viewport 3D : moteur de rendu complet (TU isolé) ────────────────────────
#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkRendererConfig.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Core/NkSceneContext.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Mesh/NkGLTFMaterialBridge.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Core/NkDeviceInitInfo.h"
#include "NKRHI/Core/NkGraphicsApi.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRenderer/Materials/NkMaterialCollection.h"		 // Upload() per-frame
#include "NKRenderer/Tools/PostProcess/NkPostProcessStack.h" // Execute() tonemap ACES
#include "NKRenderer/Core/NkRenderGraph.h"					 // Execute() pipeline complet (option A)
#include "NKGui/NkGuiRHIBackend.h"							 // RegisterTexture (Integrations/NKGui)
#include "NkAnimaEditor/NkRagdollBridge.h"					 // couplage ragdoll <-> squelette (NKPhysics)
#include "NKAnimPhysics/NkPoseMass.h"						 // M3.1 : distribution de masse + COM
#include "NKAnimPhysics/NkBalance.h"						 // M3.2 : équilibre, polygone de support
#include "NKAnimPhysics/NkContactDetector.h"				 // M3.3 : détection des appuis au sol
#include "NKLogger/NkLog.h"
#include <cstdlib>
#include <cmath>

namespace nkanima {

	using namespace nkentseu::renderer;
	// Substrat d animation extrait de NKRenderer le 2026-08-14 : les types de
	// clip/lecture/edition viennent desormais de nkentseu::anim.
	namespace anim = nkentseu::anim;
	using nkentseu::math::NkMat4f;
	using nkentseu::math::NkQuatf;
	using nkentseu::math::NkVec4f;
	// Types NKRHI (namespace nkentseu, pas ::renderer)
	using nkentseu::NkDeviceFactory;
	using nkentseu::NkDeviceInitInfo;
	using nkentseu::NkGPUFormat;
	using nkentseu::NkGraphicsApi;
	using nkentseu::NkGraphicsApiName;
	using nkentseu::NkICommandBuffer;
	using nkentseu::NkIDevice;
	using nkentseu::NkSwapchainFormat;

	namespace {
		struct Doc {
				NkGLTFMeshData gltf;
				anim::NkAnimationClip clip;
				anim::NkAnimationPlayer player;
				anim::NkAnimationEditor editor;
				bool loaded = false;
				bool playing = true;
				// édition de pose (§2 Pose Mode)
				NkVector<NkMat4f> bindGlobal; // inverse(inverseBind) par joint
				NkVector<uint32> topo;		  // ordre topo (parent avant enfant)
				NkVector<NkMat4f> worldEdit;  // pose de TRAVAIL (matrices monde par joint)
				bool editMode = false;

				// ── Debug COM (M3.1/M3.2/M3.3) : tampons réutilisés, zéro alloc/frame ──
				bool showCom = false;
				nkentseu::animphys::NkPoseMass poseMass;
				NkVector<NkVec3f> comPos;	  // positions monde des joints (scratch)
				NkVector<int32> comParent;	  // ignoré, exigé par AnimGetSkeleton
				int32 comRegime = 0;		  // 0 = uniforme (pas de noms), 1 = anthropométrique
				int32 comJointCount = 0;	  // compte au dernier calibrage (re-synchro si change)
				NkVector<uint32> comFeet;	  // indices des joints d'appui (foot/ankle/toe)
				NkVector<NkVec3f> comFeetPos; // scratch : positions monde des appuis
				NkVector<NkVec3f> comSupport; // scratch : points de support détectés
				int32 comVerdict = -1;		  // -1 = indéterminé, 0 = déséquilibré, 1 = équilibré

				// ── Viewport 3D (lazy-init, device PARTAGÉ avec l'éditeur) ───────
				bool tried3d = false, ok3d = false, meshLoaded3d = false;
				NkIDevice *sharedDev = nullptr; // device de l'éditeur (NkEditorRHIRenderer)
				NkIDevice *dev3d = nullptr;		// = sharedDev (alias d'usage)
				NkTexHandle toneTex{};			// sortie tonemappée LDR (post-process)
				int32 viewModeSel = 0;			// 0=Solide(unlit) 1=Rendu(lit) 2=Filaire
				NkRenderer *r3 = nullptr;
				NkOffscreenTarget *rt = nullptr;
				NkMeshHandle mesh;
				NkGLTFMaterialSet matSet;
				NkMatInstHandle skinMat;
				NkVector<NkMatInstHandle> matSlots;
				NkVector<NkMat4f> invBind; // inverseBind par joint
				NkVector<NkMat4f> skin3d;  // scratch : matrices de skinning
				NkVector<uint8> pixels;	   // readback RGBA8
				uint32 rtW = 0, rtH = 0;
				NkVec3f center3d = {0, 0, 0};
				float32 radius3d = 2.f;
				float32 camYaw = 0.6f, camPitch = 0.12f, camZoom = 1.f;

				// ── Ragdoll physique (couplage NKPhysics) ───────────────────────
				nkentseu::physics::NkPhysicsWorld physWorld{nkentseu::physics::NkPhysicsConfig{{0.f, -9.81f, 0.f}}};
				NkRagdollBridge ragdoll;
				bool physicsOn = false;
		};

		Doc g;

		inline float32 fmin(float32 a, float32 b) {
			return a < b ? a : b;
		}

		inline float32 fmax(float32 a, float32 b) {
			return a > b ? a : b;
		}

		void BuildSkeletonAux() {
			uint32 jc = (uint32)g.clip.jointInverseBind.Size();
			g.bindGlobal.Resize(jc);
			for (uint32 j = 0; j < jc; ++j)
				g.bindGlobal[j] = g.clip.jointInverseBind[j].Inverse();
			g.topo.Clear();
			NkVector<bool> placed;
			placed.Resize(jc);
			for (uint32 j = 0; j < jc; ++j)
				placed[j] = false;
			uint32 done = 0, gg = 0;
			while (done < jc && gg++ < jc + 2) {
				for (uint32 j = 0; j < jc; ++j) {
					if (placed[j])
						continue;
					int32 p = g.clip.jointParent[j];
					if (p < 0 || placed[(uint32)p]) {
						g.topo.PushBack(j);
						placed[j] = true;
						++done;
					}
				}
			}
			for (uint32 j = 0; j < jc; ++j)
				if (!placed[j])
					g.topo.PushBack(j);
		}
	} // namespace

	bool AnimInit(const char *modelPath) {
		// Routage par extension (insensible a la casse — meme critere que
		// NkMeshSystem::Import) : le chemin FBX porte desormais squelette,
		// skinning et animations (chantier « FBX operationnel », 2026-08-17).
		// Tout autre suffixe garde le chemin glTF historique.
		nkentseu::NkString lower{modelPath};
		for (uint32 i = 0; i < (uint32)lower.Size(); ++i) {
			char c = lower[i];
			if (c >= 'A' && c <= 'Z')
				lower[i] = (char)(c + 32);
		}
		const bool ok = lower.EndsWith(".fbx") ? LoadFBX(modelPath, g.gltf) : LoadGLTF(modelPath, g.gltf);
		if (ok && g.gltf.isSkinned) {
			int32 ai = g.gltf.animations.Empty() ? -1 : 0;
			if (BakeClipFromGLTF(g.gltf, ai, 30.f, g.clip)) {
				g.player.SetClip(&g.clip);
				g.player.Play(anim::NkPlayMode::NK_LOOP, 1.f);
				g.editor.SetClip(&g.clip);
				g.editor.SetSnap(1.f / g.clip.fps);
				g.loaded = true;
				g.comJointCount = 0; // force le recalibrage du COM : un autre modèle
									 // au MÊME nombre de joints garderait sinon la
									 // masse (et les pieds) de l'ancien, en silence.
				if (getenv("NK_SHOW_COM"))
					g.showCom = true; // même famille que NK_POSE_TEST/NK_ANIM_VIEW :
									  // allume le COM au lancement (captures
									  // témoins reproductibles, sans clic).
				BuildSkeletonAux();
				logger.Info("[AnimBridge] '{0}' : {1} os, dur={2}s, {3} cles\n", modelPath,
							(uint32)g.clip.boneTracks.Size(), g.clip.duration, g.editor.PoseKeyCount());
				// Self-test IK-drag (NK_POSE_TEST) : tire un os feuille et vérifie qu'il
				// se rapproche de la cible.
				if (getenv("NK_POSE_TEST")) {
					g.player.Update(0.f);
					AnimBeginPoseEdit();
					uint32 jc = (uint32)g.worldEdit.Size();
					NkVector<bool> hasChild;
					hasChild.Resize(jc);
					for (uint32 j = 0; j < jc; ++j)
						hasChild[j] = false;
					for (uint32 j = 0; j < jc; ++j) {
						int32 p = g.clip.jointParent[j];
						if (p >= 0)
							hasChild[(uint32)p] = true;
					}
					int32 leaf = -1;
					for (uint32 j = 0; j < jc; ++j)
						if (!hasChild[j]) {
							leaf = (int32)j;
							break;
						}
					if (leaf >= 0) {
						NkMat4f &m = g.worldEdit[(uint32)leaf];
						float32 ox = m.position.x, oy = m.position.y, oz = m.position.z;
						float32 tx = ox + 0.3f, ty = oy + 0.2f, tz = oz;
						float32 d0 = sqrtf((ox - tx) * (ox - tx) + (oy - ty) * (oy - ty));
						AnimDragJoint(leaf, tx, ty, tz);
						NkMat4f &m2 = g.worldEdit[(uint32)leaf];
						float32 d1 = sqrtf((m2.position.x - tx) * (m2.position.x - tx) +
										   (m2.position.y - ty) * (m2.position.y - ty));
						uint32 before = g.editor.PoseKeyCount();
						AnimCommitPoseKey();
						logger.Info(
							"[POSE_TEST] leaf={0} dist cible avant={1} apres={2} (rapproche:{3}) | cles {4}->{5}\n",
							leaf, d0, d1, (d1 < d0) ? 1 : 0, before, g.editor.PoseKeyCount());
					}
					// Test FK : pivoter un joint AVEC enfant -> l'enfant doit bouger,
					// le parent (et donc le joint pivoté) doit rester en place.
					int32 mid = -1;
					for (uint32 j = 0; j < jc; ++j)
						if (hasChild[j]) {
							mid = (int32)j;
							break;
						}
					if (mid >= 0) {
						int32 ch = -1;
						for (uint32 j = 0; j < jc; ++j)
							if (g.clip.jointParent[j] == mid) {
								ch = (int32)j;
								break;
							}
						if (ch >= 0) {
							NkMat4f &mc0 = g.worldEdit[(uint32)ch];
							float32 cx0 = mc0.position.x, cy0 = mc0.position.y;
							NkMat4f &mp0 = g.worldEdit[(uint32)mid];
							float32 px0 = mp0.position.x, py0 = mp0.position.y;
							AnimRotateJoint(mid, 0.5f);
							NkMat4f &mc1 = g.worldEdit[(uint32)ch];
							float32 cmove = sqrtf((mc1.position.x - cx0) * (mc1.position.x - cx0) +
												  (mc1.position.y - cy0) * (mc1.position.y - cy0));
							NkMat4f &mp1 = g.worldEdit[(uint32)mid];
							float32 pmove = sqrtf((mp1.position.x - px0) * (mp1.position.x - px0) +
												  (mp1.position.y - py0) * (mp1.position.y - py0));
							logger.Info("[FK_TEST] joint={0} enfant={1} deplacement_enfant={2} deplacement_pivot={3} "
										"(ok:{4})\n",
										mid, ch, cmove, pmove, (cmove > 1e-4f && pmove < 1e-4f) ? 1 : 0);
							// Test translation : joint ET enfant se déplacent du même delta.
							float32 jx0 = g.worldEdit[(uint32)mid].position.x, cx1 = g.worldEdit[(uint32)ch].position.x;
							AnimTranslateJoint(mid, 0.1f, 0.f, 0.f);
							float32 djoint = g.worldEdit[(uint32)mid].position.x - jx0;
							float32 dchild = g.worldEdit[(uint32)ch].position.x - cx1;
							logger.Info("[TR_TEST] joint={0} d_joint={1} d_enfant={2} (ok:{3})\n", mid, djoint, dchild,
										(djoint > 0.09f && djoint < 0.11f && dchild > 0.09f && dchild < 0.11f) ? 1 : 0);
						}
					}
					AnimEndPoseEdit();
				}
			}
		}
		if (!g.loaded)
			logger.Errorf("[AnimBridge] echec '%s'\n", modelPath);
		return g.loaded;
	}

	bool AnimLoaded() {
		return g.loaded;
	}

	void AnimUpdate(float32 dt) {
		if (g.physicsOn) { // ragdoll : la physique pilote la pose (worldEdit)
			const float32 h = (dt > 0.f && dt < 0.1f) ? dt : (1.f / 60.f);
			g.physWorld.Step(h);
			g.ragdoll.SyncToSkeleton(g.worldEdit);
			return;
		}
		if (g.loaded && g.playing) {
			g.player.Update(dt);
			g.editor.SetCursor(g.player.GetTime());
		}
	}

	// ── Ragdoll physique : couplage NKPhysics <-> squelette skinné ───────────
	void AnimSetPhysics(bool on) {
		if (!g.loaded) {
			g.physicsOn = false;
			return;
		}
		if (on) {
			if (!g.editMode)
				AnimBeginPoseEdit(); // capture la pose courante -> worldEdit (editMode)
			g.physWorld = nkentseu::physics::NkPhysicsWorld(nkentseu::physics::NkPhysicsConfig{{0.f, -9.81f, 0.f}});
			g.ragdoll.Build(g.physWorld, g.bindGlobal, g.clip.jointParent);
			g.physicsOn = true;
			logger.Info("[AnimBridge] ragdoll physique ON ({0} os)\n", g.ragdoll.Count());
		} else {
			g.physicsOn = false;
			g.ragdoll.Clear();
		}
	}

	bool AnimPhysicsEnabled() {
		return g.physicsOn;
	}

	bool AnimIsPlaying() {
		return g.playing;
	}

	void AnimSetPlaying(bool p) {
		g.playing = p;
		if (p)
			g.player.Play(anim::NkPlayMode::NK_LOOP, 1.f);
		else
			g.player.Pause();
	}

	float32 AnimCursor() {
		return g.editor.GetCursor();
	}

	void AnimSetCursor(float32 t) {
		g.editor.SetCursor(t);
	}

	void AnimSeek(float32 t) {
		g.playing = false;
		g.player.Pause();
		g.player.SeekTo(t);
		g.editor.SetCursor(t);
	}

	float32 AnimDuration() {
		return g.editor.Duration();
	}

	float32 AnimFps() {
		return g.editor.Fps();
	}

	uint32 AnimKeyCount() {
		return g.editor.PoseKeyCount();
	}

	void AnimGetKeyTimes(NkVector<float32> &out) {
		out = g.editor.GetPoseKeyTimes();
	}

	bool AnimIsSelected(float32 t) {
		return g.editor.IsSelected(t);
	}

	void AnimSelectKey(float32 t) {
		g.editor.SelectKey(t, false);
	}

	void AnimClearSelection() {
		g.editor.ClearSelection();
	}

	void AnimDeleteSelected() {
		g.editor.DeleteSelected();
	}

	void AnimMoveKey(float32 a, float32 b) {
		g.editor.MovePoseKey(a, b);
	}

	void AnimUndo() {
		g.editor.Undo();
	}

	void AnimRedo() {
		g.editor.Redo();
	}

	void AnimInsertKeyAtCursor() {
		if (!g.loaded)
			return;
		float32 t = g.editor.GetCursor();
		uint32 nb = (uint32)g.clip.boneTracks.Size();
		NkVector<NkMat4f> pose;
		pose.Resize(nb);
		for (uint32 b = 0; b < nb; ++b)
			pose[b] = g.clip.boneTracks[b].Evaluate(t);
		g.editor.InsertPoseKey(pose);
	}

	uint32 AnimJointCount() {
		return (uint32)g.clip.jointInverseBind.Size();
	}

	void AnimGetSkeleton(NkVector<NkVec3f> &outPos, NkVector<int32> &outParent) {
		outPos.Clear();
		outParent.Clear();
		if (!g.loaded)
			return;
		uint32 jc = (uint32)g.clip.jointInverseBind.Size();
		outPos.Resize(jc);
		outParent.Resize(jc);
		const auto &skin = g.player.GetState().boneMatrices;
		for (uint32 j = 0; j < jc; ++j) {
			NkMat4f gl;
			if (g.editMode && j < (uint32)g.worldEdit.Size()) // pose de travail éditée
				gl = g.worldEdit[j];
			else // pose du player : skin*bindGlobal
				gl = (j < (uint32)skin.Size()) ? (skin[j] * g.bindGlobal[j]) : NkMat4f::Identity();
			outPos[j] = {gl.position.x, gl.position.y, gl.position.z};
			outParent[j] = (j < (uint32)g.clip.jointParent.Size()) ? g.clip.jointParent[j] : -1;
		}
	}

	// ── Édition de pose ──────────────────────────────────────────────────────────
	void AnimBeginPoseEdit() {
		if (!g.loaded)
			return;
		const auto &skin = g.player.GetState().boneMatrices;
		uint32 jc = (uint32)g.bindGlobal.Size();
		g.worldEdit.Resize(jc);
		for (uint32 j = 0; j < jc; ++j)
			g.worldEdit[j] = (j < (uint32)skin.Size()) ? (skin[j] * g.bindGlobal[j]) : NkMat4f::Identity();
		g.editMode = true;
		g.playing = false;
		g.player.Pause();
	}

	bool AnimInPoseEdit() {
		return g.editMode;
	}

	void AnimEndPoseEdit() {
		g.editMode = false;
	}

	void AnimDragJoint(int32 jsel, float32 wx, float32 wy, float32 wz) {
		if (!g.editMode || jsel < 0 || jsel >= (int32)g.worldEdit.Size())
			return;
		uint32 jc = (uint32)g.worldEdit.Size();
		// Chaîne IK : jusqu'à 3 joints (… parent, jsel) racine->effecteur.
		NkVector<int32> up;
		{
			int32 c = jsel;
			uint32 n = 0;
			while (c >= 0 && n < 3) {
				up.PushBack(c);
				c = g.clip.jointParent[(uint32)c];
				++n;
			}
		}
		NkVector<int32> chain;
		for (uint32 i = up.Size(); i > 0; --i)
			chain.PushBack(up[i - 1]);
		uint32 n = (uint32)chain.Size();
		if (n < 2)
			return;

		NkVector<NkMat4f> baseLocal;
		baseLocal.Resize(jc);
		for (uint32 k = 0; k < jc; ++k) {
			int32 p = g.clip.jointParent[k];
			baseLocal[k] = (p >= 0) ? (g.worldEdit[(uint32)p].Inverse() * g.worldEdit[k]) : g.worldEdit[k];
		}

		// FABRIK (positions).
		NkVector<NkVec3f> pos;
		pos.Resize(n);
		for (uint32 i = 0; i < n; ++i) {
			const NkMat4f &m = g.worldEdit[(uint32)chain[i]];
			pos[i] = {m.position.x, m.position.y, m.position.z};
		}
		NkVector<float32> len;
		len.Resize(n);
		for (uint32 i = 1; i < n; ++i) {
			NkVec3f d = {pos[i].x - pos[i - 1].x, pos[i].y - pos[i - 1].y, pos[i].z - pos[i - 1].z};
			len[i] = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
		}
		NkVec3f root = pos[0], target = {wx, wy, wz};
		for (int it = 0; it < 16; ++it) {
			pos[n - 1] = target;
			for (int32 i = (int32)n - 2; i >= 0; --i) {
				NkVec3f d = {pos[(uint32)i].x - pos[(uint32)i + 1].x, pos[(uint32)i].y - pos[(uint32)i + 1].y,
							 pos[(uint32)i].z - pos[(uint32)i + 1].z};
				float32 dl = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
				if (dl > 1e-7f) {
					d.x /= dl;
					d.y /= dl;
					d.z /= dl;
				}
				float32 L = len[(uint32)i + 1];
				pos[(uint32)i] = {pos[(uint32)i + 1].x + d.x * L, pos[(uint32)i + 1].y + d.y * L,
								  pos[(uint32)i + 1].z + d.z * L};
			}
			pos[0] = root;
			for (uint32 i = 0; i < n - 1; ++i) {
				NkVec3f d = {pos[i + 1].x - pos[i].x, pos[i + 1].y - pos[i].y, pos[i + 1].z - pos[i].z};
				float32 dl = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
				if (dl > 1e-7f) {
					d.x /= dl;
					d.y /= dl;
					d.z /= dl;
				}
				float32 L = len[i + 1];
				pos[i + 1] = {pos[i].x + d.x * L, pos[i].y + d.y * L, pos[i].z + d.z * L};
			}
		}
		// aim-FK : chaîne racine->tip pivote vers les positions FABRIK.
		auto rotP = [](const NkMat4f &m) {
			NkMat4f r = m;
			r.m30 = 0;
			r.m31 = 0;
			r.m32 = 0;
			r.m33 = 1;
			return r;
		};
		NkVector<NkMat4f> out;
		out.Resize(jc);
		for (uint32 k = 0; k < jc; ++k)
			out[k] = g.worldEdit[k];
		for (uint32 i = 0; i + 1 < n; ++i) {
			uint32 a = (uint32)chain[i], c = (uint32)chain[i + 1];
			NkMat4f childLocal = g.worldEdit[a].Inverse() * g.worldEdit[c];
			NkMat4f Ga = out[a];
			NkVec3f jp = {Ga.position.x, Ga.position.y, Ga.position.z};
			NkMat4f cur = Ga * childLocal;
			NkVec3f cp = {cur.position.x, cur.position.y, cur.position.z};
			NkVec3f np = pos[i + 1];
			NkVec3f va = {cp.x - jp.x, cp.y - jp.y, cp.z - jp.z}, vb = {np.x - jp.x, np.y - jp.y, np.z - jp.z};
			float32 al = sqrtf(va.x * va.x + va.y * va.y + va.z * va.z),
					bl = sqrtf(vb.x * vb.x + vb.y * vb.y + vb.z * vb.z);
			if (al > 1e-6f && bl > 1e-6f) {
				va.x /= al;
				va.y /= al;
				va.z /= al;
				vb.x /= bl;
				vb.y /= bl;
				vb.z /= bl;
				NkQuatf Rw(va, vb);
				out[a] = NkMat4f::Translate(jp) * (Rw.ToMat4() * rotP(Ga));
			}
			out[c] = out[a] * childLocal;
		}
		NkVector<bool> inChain;
		inChain.Resize(jc);
		for (uint32 k = 0; k < jc; ++k)
			inChain[k] = false;
		for (uint32 i = 0; i < n; ++i)
			inChain[(uint32)chain[i]] = true;
		for (uint32 oi = 0; oi < (uint32)g.topo.Size(); ++oi) {
			uint32 k = g.topo[oi];
			if (inChain[k])
				continue;
			int32 p = g.clip.jointParent[k];
			out[k] = (p >= 0) ? (out[(uint32)p] * baseLocal[k]) : out[k];
		}
		g.worldEdit = out;
	}

	void AnimRotateJoint(int32 jsel, float32 deltaRadians) {
		if (!g.editMode || jsel < 0 || jsel >= (int32)g.worldEdit.Size())
			return;
		if (deltaRadians > -1e-6f && deltaRadians < 1e-6f)
			return;
		uint32 jc = (uint32)g.worldEdit.Size();
		// local de chaque joint relativement à la pose de travail courante
		NkVector<NkMat4f> baseLocal;
		baseLocal.Resize(jc);
		for (uint32 k = 0; k < jc; ++k) {
			int32 p = g.clip.jointParent[k];
			baseLocal[k] = (p >= 0) ? (g.worldEdit[(uint32)p].Inverse() * g.worldEdit[k]) : g.worldEdit[k];
		}
		// rotation monde autour de Z (from-to, même construction que l'aim-FK),
		// pivot = position du joint sélectionné.
		NkMat4f &M = g.worldEdit[(uint32)jsel];
		NkVec3f jp = {M.position.x, M.position.y, M.position.z};
		NkVec3f va = {1.f, 0.f, 0.f}, vb = {cosf(deltaRadians), sinf(deltaRadians), 0.f};
		NkQuatf Rz(va, vb);
		auto rotP = [](const NkMat4f &m) {
			NkMat4f r = m;
			r.m30 = 0;
			r.m31 = 0;
			r.m32 = 0;
			r.m33 = 1;
			return r;
		};
		NkVector<NkMat4f> out;
		out.Resize(jc);
		for (uint32 k = 0; k < jc; ++k)
			out[k] = g.worldEdit[k];
		out[(uint32)jsel] = NkMat4f::Translate(jp) * (Rz.ToMat4() * rotP(M));
		// propage : enfants recalculés depuis le parent (ordre topo). Les joints hors
		// sous-arbre de jsel retrouvent leur monde inchangé (parent non bougé).
		for (uint32 oi = 0; oi < (uint32)g.topo.Size(); ++oi) {
			uint32 k = g.topo[oi];
			if (k == (uint32)jsel)
				continue;
			int32 p = g.clip.jointParent[k];
			if (p < 0)
				continue;
			out[k] = out[(uint32)p] * baseLocal[k];
		}
		g.worldEdit = out;
	}

	void AnimTranslateJoint(int32 jsel, float32 dx, float32 dy, float32 dz) {
		if (!g.editMode || jsel < 0 || jsel >= (int32)g.worldEdit.Size())
			return;
		if (dx > -1e-7f && dx < 1e-7f && dy > -1e-7f && dy < 1e-7f && dz > -1e-7f && dz < 1e-7f)
			return;
		uint32 jc = (uint32)g.worldEdit.Size();
		NkVector<NkMat4f> baseLocal;
		baseLocal.Resize(jc);
		for (uint32 k = 0; k < jc; ++k) {
			int32 p = g.clip.jointParent[k];
			baseLocal[k] = (p >= 0) ? (g.worldEdit[(uint32)p].Inverse() * g.worldEdit[k]) : g.worldEdit[k];
		}
		NkVector<NkMat4f> out;
		out.Resize(jc);
		for (uint32 k = 0; k < jc; ++k)
			out[k] = g.worldEdit[k];
		out[(uint32)jsel].position.x += dx;
		out[(uint32)jsel].position.y += dy;
		out[(uint32)jsel].position.z += dz;
		// propage : le sous-arbre suit (offset local conservé), le reste inchangé.
		for (uint32 oi = 0; oi < (uint32)g.topo.Size(); ++oi) {
			uint32 k = g.topo[oi];
			if (k == (uint32)jsel)
				continue;
			int32 p = g.clip.jointParent[k];
			if (p < 0)
				continue;
			out[k] = out[(uint32)p] * baseLocal[k];
		}
		g.worldEdit = out;
	}

	void AnimJointWorldPos(int32 j, float32 &x, float32 &y, float32 &z) {
		x = y = z = 0.f;
		if (j < 0 || j >= (int32)g.worldEdit.Size() || !g.editMode)
			return;
		const NkMat4f &m = g.worldEdit[(uint32)j];
		x = m.position.x;
		y = m.position.y;
		z = m.position.z;
	}

	void AnimCommitPoseKey() {
		if (!g.editMode || !g.loaded)
			return;
		uint32 jc = (uint32)g.worldEdit.Size();
		NkVector<NkMat4f> local;
		local.Resize(jc);
		for (uint32 j = 0; j < jc; ++j) {
			int32 p = g.clip.jointParent[j];
			local[j] = (p >= 0) ? (g.worldEdit[(uint32)p].Inverse() * g.worldEdit[j]) : g.worldEdit[j];
		}
		g.editor.InsertPoseKey(local);
	}

	// ── Viewport 3D embarqué (device PARTAGÉ avec l'éditeur) ───────────────────────
	void Anim3DSetSharedDevice(void *device) {
		g.sharedDev = (NkIDevice *)device;
	}

	void Anim3DSetViewMode(NkAnimViewMode m) {
		g.viewModeSel = (int32)m;
	}

	NkAnimViewMode Anim3DViewMode() {
		return (NkAnimViewMode)g.viewModeSel;
	}

	namespace {
		bool Init3D() {
			if (g.tried3d)
				return g.ok3d;
			g.tried3d = true;
			if (!g.loaded || !g.gltf.isSkinned || g.gltf.skinnedVertices.Empty())
				return false;
			if (!g.sharedDev || !g.sharedDev->IsValid()) {
				logger.Errorf("[Anim3D] device partage absent (Anim3DSetSharedDevice non appele)\n");
				return false;
			}
			g.dev3d = g.sharedDev; // on PARTAGE le device de l'éditeur (pas de 2e device)

			// NkRenderer sur le device partagé. On NE pilote PAS sa frame (BeginFrame/
			// Present) : l'éditeur possède la frame device. On l'utilise seulement pour
			// ses sous-systèmes (render3d/mesh/material/offscreen) en pilotant le cmd
			// de l'éditeur. ResetFrame()+materials Upload() sont rejoués à la main.
			NkRendererConfig cfg = NkRendererConfig::ForGame(g.dev3d->GetApi(), 1280, 720);
			cfg.Enable(NK_SS_OFFSCREEN);
			cfg.Enable(NK_SS_POST_PROCESS); // tonemap ACES (HDR -> LDR perceptuel)
			cfg.shadow.cascadeCount = 1;
			cfg.postProcess.toneMapping = true;
			cfg.postProcess.aces = true;
			cfg.postProcess.gamma = 2.2f;
			cfg.postProcess.bloom = false;
			cfg.postProcess.ssao = false; // halo magenta sur le skin (cf mémoire)
			cfg.voxelAOEnabled = false;
			// IBL ambient : environnement procédural neutre (couleurs par défaut) +
			// force d'ambient élevée -> le perso est éclairé sur TOUTES ses faces (pas
			// seulement celles face aux directionnelles). Clé de la visibilité.
			cfg.ibl.useHDR = false;
			cfg.ibl.iblStrength = 1.2f;
			g.r3 = NkRenderer::Create(g.dev3d, cfg);
			if (!g.r3) {
				logger.Errorf("[Anim3D] renderer echec\n");
				return false;
			}

			NkOffscreenDesc od;
			od.width = 1280;
			od.height = 720;
			// Cible FINALE du render graph (sortie tonemappée LDR). Le graph crée son
			// propre HDR transient pour la géométrie ; cet offscreen reçoit le résultat
			// final (après ombres + éclairage + IBL + tonemap). Format = swapchain.
			od.hdr = false;
			od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
			od.hasDepth = true;
			od.readable = true;
			od.readback = false;
			od.name = "AnimViewport";
			g.rt = g.r3->CreateOffscreen(od);
			if (!g.rt || !g.rt->IsValid()) {
				logger.Errorf("[Anim3D] offscreen echec\n");
				return false;
			}
			g.rtW = 1280;
			g.rtH = 720;
			// OPTION A : redirige la sortie du render graph (normalement la swapchain)
			// vers cet offscreen -> le PIPELINE COMPLET (shadow/lighting/IBL/tonemap)
			// rend dans le viewport, échantillonnable par l'UI.
			if (auto *texLib = g.r3->GetTextures())
				g.r3->SetFinalColorTarget(texLib->GetRHIHandle(g.rt->GetColorResult()));

			// Mesh skinné + matériaux glTF (calque DemoIKChar).
			auto *meshSys = g.r3->GetMeshSystem();
			NkMeshDesc md;
			md.layout = NkVertexLayout::Skinned();
			md.vertices = g.gltf.skinnedVertices.Data();
			md.vertexCount = (uint32)g.gltf.skinnedVertices.Size();
			md.indices = g.gltf.indices.Data();
			md.indexCount = (uint32)g.gltf.indices.Size();
			md.subMeshes = g.gltf.subMeshes;
			md.bounds = g.gltf.bounds;
			md.debugName = g.gltf.debugName;
			g.mesh = meshSys->Create(md);
			g.meshLoaded3d = g.mesh.IsValid();
			if (g.meshLoaded3d) {
				auto *matSys = g.r3->GetMaterials();
				auto *texLib = g.r3->GetTextures();
				BuildGLTFMaterials(g.gltf, matSys, texLib, g.matSet);
				int32 m0 = (!g.gltf.subMeshMaterial.Empty()) ? g.gltf.subMeshMaterial[0] : -1;
				g.skinMat = g.matSet.InstanceForMaterial(m0);
				if (!g.skinMat.IsValid())
					for (uint32 mi = 0; mi < (uint32)g.matSet.instances.Size(); ++mi)
						if (g.matSet.instances[mi].IsValid()) {
							g.skinMat = g.matSet.instances[mi];
							break;
						}
				uint32 nSubs = meshSys->GetSubMeshCount(g.mesh);
				g.matSlots.Resize(nSubs);
				for (uint32 si = 0; si < nSubs; ++si) {
					int32 mIdx = (si < (uint32)g.gltf.subMeshMaterial.Size()) ? g.gltf.subMeshMaterial[si] : -1;
					NkMatInstHandle h = g.matSet.InstanceForMaterial(mIdx);
					g.matSlots[si] = h.IsValid() ? h : g.skinMat;
				}
			}

			uint32 jc = (uint32)g.clip.jointInverseBind.Size();
			g.invBind.Resize(jc);
			g.skin3d.Resize(jc);
			for (uint32 j = 0; j < jc; ++j)
				g.invBind[j] = g.clip.jointInverseBind[j];

			// Cadrage caméra sur les SOMMETS SKINNÉS POSÉS réels (calque DemoAnim) —
			// PAS sur bindGlobal (espace/échelle différents du mesh rendu -> caméra mal
			// cadrée -> seule une partie du mesh visible). On pose à t=0 et on calcule
			// l'AABB du mesh déformé (blend des matrices d'os par sommet).
			g.player.Update(0.f);
			const NkVector<NkMat4f> &bones0 = g.player.GetState().boneMatrices;
			float32 mnx = 1e9f, mny = 1e9f, mnz = 1e9f, mxx = -1e9f, mxy = -1e9f, mxz = -1e9f;
			const auto &svs = g.gltf.skinnedVertices;
			for (uint32 vi = 0; vi < (uint32)svs.Size(); ++vi) {
				const auto &v = svs[vi];
				NkMat4f m;
				for (int e = 0; e < 16; e++)
					m.data[e] = 0.f;
				float32 wsum = 0.f;
				for (int b = 0; b < 4; b++) {
					int j = (int)(v.boneIdx[b] + 0.5f);
					float32 w = v.boneWeight[b];
					if (j >= 0 && j < (int)bones0.Size() && w > 0.f) {
						for (int e = 0; e < 16; e++)
							m.data[e] += w * bones0[(uint32)j].data[e];
						wsum += w;
					}
				}
				if (wsum < 1e-4f)
					m = NkMat4f::Identity();
				NkVec4f wp = m * NkVec4f{v.pos.x, v.pos.y, v.pos.z, 1.f};
				mnx = fmin(mnx, wp.x);
				mny = fmin(mny, wp.y);
				mnz = fmin(mnz, wp.z);
				mxx = fmax(mxx, wp.x);
				mxy = fmax(mxy, wp.y);
				mxz = fmax(mxz, wp.z);
			}
			g.center3d = {(mnx + mxx) * 0.5f, (mny + mxy) * 0.5f, (mnz + mxz) * 0.5f};
			float32 ex = (mxx - mnx) * 0.5f, ey = (mxy - mny) * 0.5f, ez = (mxz - mnz) * 0.5f;
			g.radius3d = fmax(0.7f, sqrtf(ex * ex + ey * ey + ez * ez)); // demi-diagonale ; dist = r*2.2 (cf render)

			if (const char *vm = getenv("NK_ANIM_VIEW"))
				g.viewModeSel = atoi(vm); // 0/1/2 mode initial
			g.ok3d = true;
			logger.Info("[Anim3D] pret : API={0} mesh={1} joints={2} centre=({3},{4},{5}) r={6}\n",
						NkGraphicsApiName(g.dev3d->GetApi()), g.meshLoaded3d ? 1 : 0, jc, g.center3d.x, g.center3d.y,
						g.center3d.z, g.radius3d);
			return true;
		}
	} // namespace

	bool Anim3DReady() {
		return g.ok3d;
	}

	void Anim3DOrbit(float32 dYaw, float32 dPitch, float32 dZoom) {
		g.camYaw += dYaw;
		g.camPitch += dPitch;
		if (g.camPitch > 1.45f)
			g.camPitch = 1.45f;
		if (g.camPitch < -1.45f)
			g.camPitch = -1.45f;
		g.camZoom *= (1.f + dZoom);
		if (g.camZoom < 0.3f)
			g.camZoom = 0.3f;
		if (g.camZoom > 4.f)
			g.camZoom = 4.f;
	}

	// Le nom d'un joint évoque-t-il un pied ? Mêmes mots-clés que la famille
	// « foot » de MassWeightForName (NkPoseMass.cpp), même insensibilité à la
	// casse — HasKw y est interne, on réécrit l'équivalent localement.
	static bool NomEvoquePied(const nkentseu::NkString &nm) {
		static const char *kws[] = {"foot", "ankle", "toe"};
		const char *hay = nm.Data();
		const nkentseu::int64 n = (nkentseu::int64)nm.Size();
		for (int32 k = 0; k < 3; ++k) {
			const char *kw = kws[k];
			nkentseu::int64 klen = 0;
			while (kw[klen] != '\0')
				++klen;
			for (nkentseu::int64 i = 0; i + klen <= n; ++i) {
				nkentseu::int64 j = 0;
				while (j < klen) {
					char c = hay[i + j];
					if (c >= 'A' && c <= 'Z')
						c = (char)(c - 'A' + 'a');
					if (c != kw[j])
						break;
					++j;
				}
				if (j == klen)
					return true;
			}
		}
		return false;
	}

	// Rend la pose courante dans l'offscreen via le cmd FOURNI (celui de l'éditeur).
	// On rejoue le setup per-frame essentiel de NkRenderer::BeginFrame (ResetFrame +
	// upload matériaux) sans piloter la frame device (que l'éditeur possède).
	void Anim3DRenderOffscreen(void *cmdv) {
		if (!Init3D())
			return;
		NkICommandBuffer *cmd = (NkICommandBuffer *)cmdv;
		auto *r3d = g.r3->GetRender3D();
		if (!cmd || !r3d)
			return;

		uint32 jc = (uint32)g.invBind.Size();
		if (g.editMode && (uint32)g.worldEdit.Size() == jc) {
			for (uint32 j = 0; j < jc; ++j)
				g.skin3d[j] = g.worldEdit[j] * g.invBind[j];
		} else {
			const auto &bm = g.player.GetState().boneMatrices;
			for (uint32 j = 0; j < jc; ++j)
				g.skin3d[j] = (j < (uint32)bm.Size()) ? bm[j] : NkMat4f::Identity();
		}

		// Setup per-frame (cf NkRendererImpl::BeginFrame) — UNE fois par frame.
		r3d->ResetFrame();
		if (auto *mc = g.r3->GetMaterialCollection())
			mc->Upload();

		float32 cp = cosf(g.camPitch), sp = sinf(g.camPitch);
		float32 dist = g.radius3d * 2.2f * g.camZoom;
		NkCamera3DData cd;
		cd.position = {g.center3d.x + sinf(g.camYaw) * cp * dist, g.center3d.y + sp * dist,
					   g.center3d.z + cosf(g.camYaw) * cp * dist};
		cd.target = g.center3d;
		cd.up = {0, 1, 0};
		cd.fovY = 45.f;
		cd.aspect = (float32)g.rtW / (float32)(g.rtH > 0 ? g.rtH : 1);
		cd.nearPlane = 0.02f;
		cd.farPlane = fmax(50.f, g.radius3d * 12.f);
		NkCamera3D cam(cd);

		// Mode d'affichage : Solide=NK_UNLIT (flat albedo, toujours visible),
		// Rendu=NK_SOLID (PBR éclairé), Filaire=NK_WIREFRAME.
		const bool wire = (g.viewModeSel == 2);
		r3d->SetWireframe(wire);

		NkSceneContext sctx;
		sctx.camera = cam;
		sctx.time = g.editor.GetCursor();
		sctx.viewMode = wire ? NkViewMode::NK_WIREFRAME
							 : (g.viewModeSel == 1 ? NkViewMode::NK_SOLID	// Rendu = PBR
												   : NkViewMode::NK_UNLIT); // Solide = albedo flat
		// Éclairage : clé + remplissage doux (l'IBL ambient fait le reste). Avec le
		// pipeline complet + tonemap ACES, des intensités modérées suffisent.
		NkLightDesc sun;
		sun.type = NkLightType::NK_DIRECTIONAL;
		sun.direction = {-0.4f, -0.8f, -0.5f};
		sun.color = {1.f, 0.97f, 0.92f};
		sun.intensity = 3.0f;
		sun.castShadow = true;
		sctx.lights.PushBack(sun);
		NkLightDesc fill;
		fill.type = NkLightType::NK_DIRECTIONAL;
		fill.direction = {0.5f, -0.3f, 0.6f};
		fill.color = {0.6f, 0.7f, 0.9f};
		fill.intensity = 1.0f;
		fill.castShadow = false;
		sctx.lights.PushBack(fill);
		sctx.iblIntensity = 1.0f;
		sctx.ambientIntensity = 0.4f;
		r3d->BeginScene(sctx);

		if (auto *meshSys = g.r3->GetMeshSystem()) { // sol
			float32 floorY = g.center3d.y - g.radius3d * 0.5f;
			float32 s = fmax(2.f, g.radius3d);
			NkDrawCall3D dc;
			dc.mesh = meshSys->GetPlane();
			dc.transform = NkMat4f::Translate({g.center3d.x, floorY, g.center3d.z}) * NkMat4f::Scale({s, 1.f, s});
			dc.aabb = {{g.center3d.x - s, floorY - 0.01f, g.center3d.z - s},
					   {g.center3d.x + s, floorY + 0.01f, g.center3d.z + s}};
			dc.tint = {0.13f, 0.14f, 0.17f};
			dc.roughness = 0.95f;
			dc.castShadow = false;
			r3d->Submit(dc);
		}
		if (g.meshLoaded3d) { // mesh skinné
			NkDrawCallSkinned dc;
			dc.mesh = g.mesh;
			dc.transform = NkMat4f::Identity();
			dc.boneMatrices = g.skin3d;
			dc.material = g.skinMat;
			dc.materialSlots = g.matSlots;
			dc.tint = g.skinMat.IsValid() ? NkVec3f{1, 1, 1} : NkVec3f{0.85f, 0.55f, 0.45f};
			dc.aabb = NkAABB{{g.center3d.x - g.radius3d, g.center3d.y - g.radius3d, g.center3d.z - g.radius3d},
							 {g.center3d.x + g.radius3d, g.center3d.y + g.radius3d, g.center3d.z + g.radius3d}};
			dc.castShadow = true;
			r3d->SubmitSkinned(dc);
		}

		// ── Debug M3.1/M3.2/M3.3 : COM + appuis + verdict d'équilibre ─────────
		// AnimGetSkeleton fournit les positions MONDE dans LES DEUX modes (pose de
		// travail éditée OU pose du player) et extrait la translation par le membre
		// nommé `gl.position` — aucune indexation d'octets, donc indépendant de la
		// convention de stockage des matrices.
		if (g.showCom) {
			AnimGetSkeleton(g.comPos, g.comParent);
			const int32 n = (int32)g.comPos.Size();
			if (n > 0) {
				// Calibrage : au premier passage et à chaque changement de compte.
				// Régime ANTHROPOMÉTRIQUE si le clip porte les noms de joints
				// (glTF "name", bakés depuis 2026-08-17), UNIFORME sinon — et le
				// libellé à l'écran suit le régime réel (AnimCOMRegimeLabel).
				if (g.comJointCount != n) {
					g.comJointCount = n;
					const bool hasNames = ((int32)g.clip.jointNames.Size() == n);
					if (hasNames) {
						g.poseMass.SetAnthropometric(g.clip.jointNames);
						g.comRegime = 1;
					} else {
						g.poseMass.SetUniform(n);
						g.comRegime = 0;
					}
					// Appuis : joints dont le nom évoque un pied — mêmes mots-clés
					// que la famille "foot" de MassWeightForName (NkPoseMass).
					g.comFeet.Clear();
					if (hasNames)
						for (int32 j = 0; j < n; ++j)
							if (NomEvoquePied(g.clip.jointNames[(uint32)j]))
								g.comFeet.PushBack((uint32)j);
				}
				const NkVec3f com = g.poseMass.ComputeCOMFromPositions(g.comPos.Data(), n);

				// Verdict d'équilibre — UNIQUEMENT si des appuis sont détectables.
				// Sol de l'éditeur : le même plan que celui dessiné plus haut.
				g.comVerdict = -1;
				g.comSupport.Clear();
				if (!g.comFeet.Empty()) {
					const float32 floorY = g.center3d.y - g.radius3d * 0.5f;
					const NkVec3f planePoint{g.center3d.x, floorY, g.center3d.z};
					const NkVec3f planeNormal{0.f, 1.f, 0.f};
					const float32 threshold = 0.04f * (g.radius3d > 0.f ? g.radius3d : 1.f);
					g.comFeetPos.Clear();
					for (uint32 k = 0; k < (uint32)g.comFeet.Size(); ++k)
						g.comFeetPos.PushBack(g.comPos[g.comFeet[k]]);
					const int32 nc = nkentseu::animphys::NkContactDetector::DetectSupportPoints(
						g.comFeetPos.Data(), (int32)g.comFeetPos.Size(), planePoint, planeNormal, threshold,
						g.comSupport);
					if (nc > 0) {
						const auto bal = nkentseu::animphys::NkBalance::EvaluateStatic(
							com, g.comSupport.Data(), (int32)g.comSupport.Size(), planeNormal);
						g.comVerdict = bal.balanced ? 1 : 0;
					}
				}

				// Couleur : verte/rouge SEULEMENT sur verdict fondé (appuis réels au
				// sol) ; NEUTRE sinon — une sphère colorée ressemble à un verdict,
				// elle n'a le droit d'en être un que s'il a été calculé.
				const NkVec4f col = (g.comVerdict == 1)	  ? NkVec4f{0.15f, 1.0f, 0.25f, 1.f}
									: (g.comVerdict == 0) ? NkVec4f{1.0f, 0.20f, 0.20f, 1.f}
														  : NkVec4f{0.92f, 0.92f, 0.92f, 1.f};
				r3d->DrawDebugSphere(com, 0.05f, col);
				// Polygone de support (jaune) quand il existe : arêtes + coins.
				const int32 sc = (int32)g.comSupport.Size();
				for (int32 i = 0; i < sc; ++i) {
					const NkVec3f a = g.comSupport[(uint32)i];
					const NkVec3f b = g.comSupport[(uint32)((i + 1) % sc)];
					if (sc > 1)
						r3d->DrawDebugLine(a, b, NkVec4f{1.0f, 0.85f, 0.15f, 1.f}, 0.f, true);
					r3d->DrawDebugSphere(a, 0.02f, NkVec4f{1.0f, 0.85f, 0.15f, 1.f});
				}
			}
		}

		// OPTION A : exécute le PIPELINE COMPLET (graph) sur le cmd de l'éditeur. Le
		// graph fait shadow -> geometry (HDR) -> SSAO/bloom -> tonemap, et écrit le
		// résultat final dans NOTRE offscreen (redirigé via SetFinalColorTarget). Il
		// gère ses propres render passes + le clear. PAS de BeginCapture/Flush manuels.
		g.toneTex = {};
		if (auto *graph = g.r3->GetRenderGraph())
			graph->Execute(cmd);
		// PAS de BeginFrame/EndFrame/Present : l'éditeur possède la frame device.
	}

	void Anim3DRegisterInto(void *guiBackend, uint32 texId) {
		if (!g.ok3d || !g.rt || !guiBackend)
			return;
		auto *b = (nkentseu::nkgui::NkGuiRHIBackend *)guiBackend;
		auto *texLib = g.r3->GetTextures();
		if (!texLib)
			return;
		// Affiche la sortie TONEMAPPÉE (LDR) si dispo, sinon l'HDR brut.
		NkTexHandle src = g.toneTex.IsValid() ? g.toneTex : g.rt->GetColorResult();
		// Pont NKRenderer (NkTexHandle) -> NKRHI (NkTextureHandle).
		b->RegisterTexture(texId, texLib->GetRHIHandle(src));
	}

	// ── Debug COM (M3.1) ─────────────────────────────────────────────────────
	void AnimSetShowCOM(bool on) {
		g.showCom = on;
	}

	bool AnimShowCOM() {
		return g.showCom;
	}

	const char *AnimCOMRegimeLabel() {
		// Le régime est nommé à l'écran parce qu'il est INDISCERNABLE autrement :
		// un COM uniforme tombe vers le milieu du torse et ressemble à un COM
		// anthropométrique. Depuis le 2026-08-17 les noms de joints arrivent du
		// glTF (NkGLTFNode.name -> clip.jointNames) : le libellé suit le régime
		// RÉEL, et le verdict n'est affiché que s'il a été calculé sur des appuis.
		if (g.comRegime == 1) {
			if (g.comVerdict == 1)
				return "COM anthropometrique — EQUILIBRE (COM au-dessus des appuis)";
			if (g.comVerdict == 0)
				return "COM anthropometrique — DESEQUILIBRE (COM hors du polygone d appuis)";
			return "COM anthropometrique — equilibre indetermine : aucun appui au sol";
		}
		return "COM uniforme (approximatif) — equilibre indetermine : aucun appui detecte";
	}

} // namespace nkanima
