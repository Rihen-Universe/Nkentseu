// =============================================================================
// Noge/ECS/Systems/NkRenderSystem.cpp — pont ECS -> NKRenderer (3D)
// =============================================================================
// Adapté à l'API NKRenderer actuelle :
//   - NkRenderer::GetRender3D() (pointeur)
//   - NkRender3D : BeginScene(NkSceneContext) -> Submit(NkDrawCall3D) -> Flush(cmd)
//   - NkSceneContext : camera (NkCamera3D), lights (NkVector<NkLightDesc>),
//     ambientIntensity, viewMode (modes Solid/Wireframe/Unlit/... façon Unreal)
//   - NkMeshSystem : Import(path) + GetBounds(handle)
// =============================================================================
#include "NkRenderSystem.h"
#include "Noge/ECS/Components/Core/NkTag.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h" // type complet (Submit/Flush)
#include "NKLogger/NkLog.h"

namespace nkentseu {

	using namespace ecs;
	using namespace renderer;
	using namespace math;

	// =========================================================================
	// Execute — point d'entrée frame
	// =========================================================================
	void NkRenderSystem::Execute(NkWorld &world, float32 dt) noexcept {
		if (!mRenderer || !mCmd || !mRenderer->IsValid())
			return;
		NkRender3D *r3d = mRenderer->GetRender3D();
		if (!r3d)
			return;

		// Reset de l'état de frame
		mSceneCtx.lights.Clear();
		mOpaqueCalls.Clear();

		// 1. Caméra active -> matrices view/proj
		UpdateActiveCamera(world);
		// 2. Lumières actives -> NkSceneContext
		CollectLights(world);
		// 3. Meshes visibles -> draw calls
		SubmitMeshes(world);

		// 4. Paramètres globaux de scène
		mSceneCtx.ambientIntensity = mAmbientIntensity;
		mSceneCtx.deltaTime = dt;
		mSceneCtx.time += dt;
		mSceneCtx.viewMode = mViewMode; // mode de rendu (Solid/Wireframe/...)

		// 5. Envoi : BeginScene -> Submit*.
		// ⚠️ PAS DE Flush ICI (2026-09-30). Le Flush appartient a la passe
		//    « Geometry » du render graph, qui l'appelle DANS sa passe au
		//    Present() (NkRendererImpl.cpp, `geom.Execute(... mRender3D->Flush
		//    (cmd))`). L'appeler ici le jouait hors de toute passe et REMETTAIT
		//    `mInScene` a faux : la passe Geometry, ensuite, ne trouvait plus
		//    rien a dessiner. C'est le patron de Demo3D (Sandbox), qui dessine :
		//    BeginScene + Submit entre BeginFrame et Present, rien d'autre.
		//    `SetManualFlush(true)` rend l'ancien comportement a qui le demande.
		r3d->BeginScene(mSceneCtx);
		for (const auto &dc : mOpaqueCalls) {
			r3d->Submit(dc);
		}
		if (mManualFlush) {
			r3d->Flush(mCmd);
		}
		mLastSubmitted = static_cast<nk_uint32>(mOpaqueCalls.Size());
	}

	// =========================================================================
	// UpdateActiveCamera — sélectionne la caméra de priorité max
	// =========================================================================
	void NkRenderSystem::UpdateActiveCamera(NkWorld &world) noexcept {
		mActiveCameraId = NkEntityId::Invalid();
		nk_int32 maxPriority = -99999;

		world.Query<NkCameraComponent, NkTransform>().ForEach(
			[&](NkEntityId id, NkCameraComponent &cam, const NkTransform &tf) {
				if (world.Has<NkInactive>(id))
					return;
				if (cam.priority <= maxPriority)
					return;

				maxPriority = cam.priority;
				mActiveCameraId = id;

				const NkVec3f pos = tf.GetWorldPosition();
				const NkVec3f fwd = tf.GetWorldForward();
				const NkVec3f up = tf.GetWorldUp();

				cam.viewMatrix = NkMat4f::LookAt(pos, pos + fwd, up);

				// aspect <= 0 : celui de la SORTIE du renderer (la fenetre), repli 16/9.
				float32 aspect = 16.f / 9.f;
				if (cam.aspect > 0.f) {
					aspect = cam.aspect;
				} else if (mRenderer && mRenderer->GetWidth() > 0 && mRenderer->GetHeight() > 0) {
					aspect = static_cast<float32>(mRenderer->GetWidth()) / static_cast<float32>(mRenderer->GetHeight());
				}
				if (cam.projection == NkCameraProjection::Perspective) {
					// Perspective prend un NkAngle (le ctor NkAngle prend des degrés).
					cam.projMatrix = NkMat4f::Perspective(math::NkAngle(cam.fovDeg), aspect, cam.nearClip, cam.farClip);
				} else {
					const float32 h = cam.orthoSize; // demi-hauteur
					const float32 w = h * aspect;	 // demi-largeur
					cam.projMatrix = NkMat4f::Orthogonal(2.f * w, 2.f * h, cam.nearClip, cam.farClip);
				}

				cam.viewProjMatrix = cam.projMatrix * cam.viewMatrix;
				mViewProjMatrix = cam.viewProjMatrix;

				// Alimente la caméra du renderer (classe NkCamera3D : on lui passe
				// les params haut-niveau via SetData, elle calcule view/proj).
				NkCamera3DData cd;
				cd.position = pos;
				cd.target = pos + fwd;
				cd.up = up;
				cd.fovY = cam.fovDeg;
				cd.aspect = aspect;
				cd.nearPlane = cam.nearClip;
				cd.farPlane = cam.farClip;
				cd.ortho = (cam.projection != NkCameraProjection::Perspective);
				cd.orthoSize = cam.orthoSize;
				mSceneCtx.camera.SetData(cd);
			});
	}

	// =========================================================================
	// CollectLights — Query NkLightComponent -> NkSceneContext.lights
	// =========================================================================
	void NkRenderSystem::CollectLights(NkWorld &world) noexcept {
		world.Query<NkTransform, NkLightComponent>().ForEach(
			[&](NkEntityId id, const NkTransform &tf, const NkLightComponent &lc) {
				if (world.Has<NkInactive>(id))
					return;
				// La lumière ambiante (ECS) est gérée via ambientIntensity, pas un light GPU.
				if (lc.type == ecs::NkLightType::Ambient)
					return;

				NkLightDesc desc;
				desc.type = ConvertLightType(lc.type);
				desc.color = {lc.color.r, lc.color.g, lc.color.b};
				desc.intensity = lc.intensity;
				desc.range = lc.range;
				desc.innerAngle = lc.innerAngle;
				desc.outerAngle = lc.outerAngle;
				desc.castShadow = lc.castShadow;
				desc.position = tf.GetWorldPosition();
				desc.direction = tf.GetWorldForward();
				mSceneCtx.lights.PushBack(desc);
			});
	}

	// =========================================================================
	// SubmitMeshes — Query mesh+material+transform -> draw calls
	// =========================================================================
	void NkRenderSystem::SubmitMeshes(NkWorld &world) noexcept {
		NkMeshSystem *meshSys = mRenderer->GetMeshSystem();
		if (!meshSys)
			return;

		// ── Meshes statiques (mesh + material) ──────────────────────────────
		world.Query<NkTransform, NkMeshComponent, NkMaterialComponent>().ForEach(
			[&](NkEntityId id, const NkTransform &tf, const NkMeshComponent &mesh, const NkMaterialComponent &mat) {
				if (world.Has<NkInactive>(id))
					return;
				if (!mesh.visible)
					return;

				// Résolution / lazy-load du mesh GPU.
				// « primitive:<nom> » (2026-09-30) : une primitive du NkMeshSystem
				// (cube, sphere, plan...), SANS fichier -- et un nom qui se
				// sauvegarde, contrairement au handle.
				NkMeshHandle meshHandle{mesh.meshHandle};
				if (!meshHandle.IsValid() && !mesh.meshPath.Empty()) {
					meshHandle = mesh.meshPath.StartsWith(kPrimitivePrefix)
									 ? ResolvePrimitive(*meshSys, mesh.meshPath.CStr() + kPrimitivePrefixLen)
									 : meshSys->Import(mesh.meshPath);
					const_cast<NkMeshComponent &>(mesh).meshHandle = meshHandle.id;
				}
				if (!meshHandle.IsValid())
					return;

				// Frustum culling (AABB monde vs viewProj).
				// ⚠️ LA BOITE PASSE PAR LA MATRICE MONDE ENTIERE (2026-09-30), pas
				//    par la seule position : un sol de 20 m (cube unite a l'echelle
				//    20) gardait une boite d'un metre, et disparaissait des que son
				//    centre sortait du champ.
				const NkAABB worldAABB = TransformAABB(meshSys->GetBounds(meshHandle), tf.worldMatrix);
				if (!IsVisible(worldAABB))
					return;

				// Matériau GPU (instance) depuis le slot du composant.
				NkMatInstHandle matHandle;
				const NkMaterialSlot *slotLu = nullptr;
				if (mat.slotCount > 0) {
					const uint32 slot = (mesh.subMeshIndex < mat.slotCount) ? mesh.subMeshIndex : 0;
					slotLu = &mat.slots[slot];
					matHandle.id = slotLu->materialHandle;
				}

				NkDrawCall3D dc;
				dc.mesh = meshHandle;
				dc.material = matHandle;
				// La COULEUR du slot (NkMaterialComponent::SetColor) passe par le
				// canal PAR DRAW CALL : c'est lui que le nuanceur PBR lit pour
				// l'albedo, le metallic et la rugosite (pbr.frag.nksl, « albedo (via
				// uObj.tint), metallic et roughness RESTENT sur uObj »), une instance
				// de materiau n'y changerait rien. Mesure du 30/09 : sans ces trois
				// lignes, le cube « rouge » de NogeDemo sortait BLANC a la capture.
				if (slotLu != nullptr) {
					dc.tint = NkVec3f{slotLu->albedo.r, slotLu->albedo.g, slotLu->albedo.b};
					dc.alpha = slotLu->albedo.a;
					dc.metallic = slotLu->metallic;
					dc.roughness = slotLu->roughness;
				}
				dc.transform = tf.worldMatrix;
				dc.castShadow = mesh.castShadow;
				dc.aabb = worldAABB;
				mOpaqueCalls.PushBack(dc);
			});

		// ── Meshes skinnés ──────────────────────────────────────────────────
		world.Query<NkTransform, NkSkinnedMeshComponent>().ForEach(
			[&](NkEntityId id, const NkTransform &tf, const NkSkinnedMeshComponent &smesh) {
				if (world.Has<NkInactive>(id))
					return;
				if (!smesh.visible)
					return;

				NkMeshHandle meshHandle{smesh.meshHandle};
				if (!meshHandle.IsValid() && !smesh.meshPath.Empty()) {
					meshHandle = meshSys->Import(smesh.meshPath);
					const_cast<NkSkinnedMeshComponent &>(smesh).meshHandle = meshHandle.id;
				}
				if (!meshHandle.IsValid())
					return;

				NkDrawCall3D dc;
				dc.mesh = meshHandle;
				dc.transform = tf.worldMatrix;
				dc.castShadow = smesh.castShadow;
				mOpaqueCalls.PushBack(dc);
			});
	}

	// =========================================================================
	// ResolvePrimitive — « primitive:cube » -> maillage built-in du NkMeshSystem
	// =========================================================================
	NkMeshHandle NkRenderSystem::ResolvePrimitive(NkMeshSystem &meshSys, const char *name) noexcept {
		if (name == nullptr)
			return NkMeshHandle{};
		const NkString n(name);
		if (n == "cube")
			return meshSys.GetCube();
		if (n == "sphere")
			return meshSys.GetSphere();
		if (n == "icosphere")
			return meshSys.GetIcosphere();
		if (n == "plane")
			return meshSys.GetPlane();
		if (n == "quad")
			return meshSys.GetQuad();
		if (n == "cylinder")
			return meshSys.GetCylinder();
		if (n == "cone")
			return meshSys.GetCone();
		if (n == "capsule")
			return meshSys.GetCapsule();
		logger.Warnf("[NkRenderSystem] primitive inconnue : '%s' (cube, sphere, icosphere, plane, quad, "
					 "cylinder, cone, capsule)\n",
					 name);
		return NkMeshHandle{};
	}

	// =========================================================================
	// TransformAABB — les 8 coins de la boite locale, par la matrice monde
	// =========================================================================
	NkAABB NkRenderSystem::TransformAABB(const NkAABB &local, const NkMat4f &m) noexcept {
		NkAABB out;
		for (int32 i = 0; i < 8; ++i) {
			const float32 x = (i & 1) ? local.max.x : local.min.x;
			const float32 y = (i & 2) ? local.max.y : local.min.y;
			const float32 z = (i & 4) ? local.max.z : local.min.z;
			// Colonnes : m[col][ligne] (la translation est m[3][0..2]).
			const NkVec3f p{m[0][0] * x + m[1][0] * y + m[2][0] * z + m[3][0],
							m[0][1] * x + m[1][1] * y + m[2][1] * z + m[3][1],
							m[0][2] * x + m[1][2] * y + m[2][2] * z + m[3][2]};
			if (i == 0) {
				out.min = p;
				out.max = p;
			} else {
				out.min = NkVec3f{NkMin(out.min.x, p.x), NkMin(out.min.y, p.y), NkMin(out.min.z, p.z)};
				out.max = NkVec3f{NkMax(out.max.x, p.x), NkMax(out.max.y, p.y), NkMax(out.max.z, p.z)};
			}
		}
		return out;
	}

	// =========================================================================
	// IsVisible — frustum culling (Gribb-Hartmann, AABB vs viewProj)
	// =========================================================================
	bool NkRenderSystem::IsVisible(const NkAABB &aabb) const noexcept {
		const NkMat4f &m = mViewProjMatrix;

		struct Plane {
				float32 a, b, c, d;
		};

		const Plane planes[6] = {
			{m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0], m[3][3] + m[3][0]},
			{m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0], m[3][3] - m[3][0]},
			{m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1], m[3][3] + m[3][1]},
			{m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1], m[3][3] - m[3][1]},
			{m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2], m[3][3] + m[3][2]},
			{m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2], m[3][3] - m[3][2]},
		};
		for (auto &p : planes) {
			const float32 px = (p.a > 0) ? aabb.max.x : aabb.min.x;
			const float32 py = (p.b > 0) ? aabb.max.y : aabb.min.y;
			const float32 pz = (p.c > 0) ? aabb.max.z : aabb.min.z;
			if (p.a * px + p.b * py + p.c * pz + p.d < 0.f)
				return false;
		}
		return true;
	}

} // namespace nkentseu
