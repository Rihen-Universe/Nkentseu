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

		// 5. Envoi GPU : BeginScene -> Submit* -> Flush
		r3d->BeginScene(mSceneCtx);
		for (const auto &dc : mOpaqueCalls) {
			r3d->Submit(dc);
		}
		r3d->Flush(mCmd);
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

				const float32 aspect = (cam.aspect > 0.f) ? cam.aspect : (16.f / 9.f);
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
				NkMeshHandle meshHandle{mesh.meshHandle};
				if (!meshHandle.IsValid() && !mesh.meshPath.Empty()) {
					meshHandle = meshSys->Import(mesh.meshPath);
					const_cast<NkMeshComponent &>(mesh).meshHandle = meshHandle.id;
				}
				if (!meshHandle.IsValid())
					return;

				// Frustum culling (AABB monde vs viewProj).
				NkAABB worldAABB = meshSys->GetBounds(meshHandle);
				const NkVec3f wpos = tf.GetWorldPosition();
				worldAABB.min = worldAABB.min + wpos;
				worldAABB.max = worldAABB.max + wpos;
				if (!IsVisible(worldAABB))
					return;

				// Matériau GPU (instance) depuis le slot du composant.
				NkMatInstHandle matHandle;
				if (mat.slotCount > 0) {
					const uint32 slot = (mesh.subMeshIndex < mat.slotCount) ? mesh.subMeshIndex : 0;
					matHandle.id = mat.slots[slot].materialHandle;
				}

				NkDrawCall3D dc;
				dc.mesh = meshHandle;
				dc.material = matHandle;
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
