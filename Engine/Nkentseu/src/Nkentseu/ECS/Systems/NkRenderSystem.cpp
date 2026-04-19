// =============================================================================
// Nkentseu/Renderer/NkRenderSystem.cpp
// =============================================================================
#include "NkRenderSystem.h"
#include "Nkentseu/ECS/Components/Core/NkTag.h"

namespace nkentseu {

    using namespace ecs;
    using namespace renderer;
    using namespace math;

    // =========================================================================
    // Execute — point d'entrée frame
    // =========================================================================
    void NkRenderSystem::Execute(NkWorld& world, float32 dt) noexcept {
        if (!mRenderer || !mCmd || !mRenderer->IsValid()) return;

        // Reset frame state
        mSceneCtx.lights.Clear();
        mOpaqueCalls.Clear();

        // 1. Caméra active → matrices view/proj
        UpdateActiveCamera(world);

        // 2. Lumières actives → NkSceneContext3D
        CollectLights(world);

        // 3. Meshes visibles → draw calls
        SubmitMeshes(world);

        // 4. Envoi GPU
        mSceneCtx.envMap          = NkTextureHandle{ mEnvMapHandle };
        mSceneCtx.ambientIntensity = mAmbientIntensity;
        mSceneCtx.deltaTime        = dt;
        mSceneCtx.time            += dt;

        auto& r3d = mRenderer->Renderer3D();
        r3d.BeginScene(mSceneCtx);

        for (const auto& dc : mOpaqueCalls) {
            r3d.Submit(dc);
        }

        r3d.EndScene(mCmd);
    }

    // =========================================================================
    // UpdateActiveCamera
    // =========================================================================
    void NkRenderSystem::UpdateActiveCamera(NkWorld& world) noexcept {
        mActiveCameraId = NkEntityId::Invalid();
        nk_int32 maxPriority = -99999;

        world.Query<NkCameraComponent, NkTransform>()
            .ForEach([&](NkEntityId id,
                         NkCameraComponent& cam,
                         const NkTransform& tf) {
                // Skip inactive entities
                if (world.Has<NkInactive>(id)) return;
                if (cam.priority <= maxPriority) return;

                maxPriority     = cam.priority;
                mActiveCameraId = id;

                // ── Calcul des matrices ──────────────────────────────────
                const NkVec3f pos = tf.GetWorldPosition();
                const NkVec3f fwd = tf.GetWorldForward();
                const NkVec3f up  = tf.GetWorldUp();

                cam.viewMatrix = NkMat4f::LookAt(pos, pos + fwd, up);

                const float32 aspect = (cam.aspect > 0.f) ? cam.aspect : (16.f / 9.f);

                if (cam.projection == NkCameraProjection::Perspective) {
                    cam.projMatrix = NkMat4f::Perspective(
                        cam.fovDeg * (3.14159265f / 180.f),
                        aspect,
                        cam.nearClip,
                        cam.farClip);
                } else {
                    const float32 h = cam.orthoSize;
                    const float32 w = h * aspect;
                    cam.projMatrix = NkMat4f::Ortho(-w, w, -h, h,
                                                    cam.nearClip, cam.farClip);
                }

                cam.viewProjMatrix = cam.projMatrix * cam.viewMatrix;
                mViewProjMatrix    = cam.viewProjMatrix;

                // ── Alimente le contexte de scène ────────────────────────
                mSceneCtx.camera.viewMatrix = cam.viewMatrix;
                mSceneCtx.camera.projMatrix = cam.projMatrix;
                mSceneCtx.camera.fovRad     = cam.fovDeg * (3.14159265f / 180.f);
                mSceneCtx.camera.nearPlane  = cam.nearClip;
                mSceneCtx.camera.farPlane   = cam.farClip;
                mSceneCtx.cameraPos         = pos;
            });
    }

    // =========================================================================
    // CollectLights
    // =========================================================================
    void NkRenderSystem::CollectLights(NkWorld& world) noexcept {
        world.Query<NkTransform, NkLightComponent>()
            .ForEach([&](NkEntityId id,
                         const NkTransform& tf,
                         const NkLightComponent& lc) {
                if (world.Has<NkInactive>(id)) return;

                NkLightDesc desc;
                desc.type      = ConvertLightType(lc.type);
                desc.color     = NkColorF(lc.color.r, lc.color.g, lc.color.b, lc.color.a);
                desc.intensity = lc.intensity;
                desc.range     = lc.range;
                desc.innerAngle = lc.innerAngle;
                desc.outerAngle = lc.outerAngle;
                desc.castShadow = lc.castShadow;
                desc.position   = tf.GetWorldPosition();
                desc.direction  = tf.GetWorldForward();
                desc.enabled    = true;

                // Lumière directionnelle principale → sunLight
                if (lc.type == NkLightType::Directional && !mSceneCtx.hasSunLight) {
                    mSceneCtx.sunLight    = desc;
                    mSceneCtx.hasSunLight = true;
                } else {
                    mSceneCtx.lights.PushBack(desc);
                }
            });
    }

    // =========================================================================
    // SubmitMeshes
    // =========================================================================
    void NkRenderSystem::SubmitMeshes(NkWorld& world) noexcept {
        if (!mRenderer->IsValid()) return;

        auto& resources = mRenderer->Resources();

        world.Query<NkTransform, NkMeshComponent, NkMaterialComponent>()
            .ForEach([&](NkEntityId id,
                         const NkTransform& tf,
                         const NkMeshComponent& mesh,
                         const NkMaterialComponent& mat) {
                if (world.Has<NkInactive>(id)) return;
                if (!mesh.visible) return;

                // ── Résolution du mesh GPU ──────────────────────────────
                NkMeshHandle meshHandle{ mesh.meshHandle };
                if (!meshHandle.IsValid() && !mesh.meshPath.IsEmpty()) {
                    // Lazy load depuis le chemin
                    meshHandle = resources.LoadMesh(mesh.meshPath.CStr());
                    // Mise à jour du composant (cast away const pour cache)
                    const_cast<NkMeshComponent&>(mesh).meshHandle = meshHandle.id;
                }
                if (!meshHandle.IsValid()) return;

                // ── Frustum culling ─────────────────────────────────────
                const NkAABB& aabb = resources.GetMeshAABB(meshHandle);
                // Transforme l'AABB en world space (approximation rapide)
                NkAABB worldAABB = aabb;
                const NkVec3f wpos = tf.GetWorldPosition();
                worldAABB.min = worldAABB.min + wpos;
                worldAABB.max = worldAABB.max + wpos;
                if (!IsVisible(worldAABB)) return;

                // ── Résolution du matériau GPU ──────────────────────────
                NkMaterialInstHandle matHandle;
                if (mat.slotCount > 0) {
                    matHandle.id = mat.slots[mesh.subMeshIndex < mat.slotCount
                                             ? mesh.subMeshIndex : 0].materialHandle;
                }
                // Si pas de matériau configuré, utiliser un matériau par défaut
                // (NkResourceManager fournit des ressources par défaut)

                // ── Soumission du draw call ─────────────────────────────
                NkDrawCall3D dc;
                dc.mesh       = meshHandle;
                dc.material   = matHandle;
                dc.transform  = tf.worldMatrix;
                dc.castShadow = mesh.castShadow;
                dc.aabb       = worldAABB;
                dc.visible    = true;

                mOpaqueCalls.PushBack(dc);
            });

        // ── Meshes skinnés ──────────────────────────────────────────────
        world.Query<NkTransform, NkSkinnedMeshComponent>()
            .ForEach([&](NkEntityId id,
                         const NkTransform& tf,
                         const NkSkinnedMeshComponent& smesh) {
                if (world.Has<NkInactive>(id)) return;
                if (!smesh.visible) return;

                NkMeshHandle meshHandle{ smesh.meshHandle };
                if (!meshHandle.IsValid() && !smesh.meshPath.IsEmpty()) {
                    meshHandle = resources.LoadMesh(smesh.meshPath.CStr());
                    const_cast<NkSkinnedMeshComponent&>(smesh).meshHandle = meshHandle.id;
                }
                if (!meshHandle.IsValid()) return;

                // Pour les meshes skinnés, on soumet le mesh complet
                // Le skinning est géré via SSBO bones dans NkRender3D
                NkDrawCall3D dc;
                dc.mesh       = meshHandle;
                dc.transform  = tf.worldMatrix;
                dc.castShadow = smesh.castShadow;
                dc.visible    = true;
                mOpaqueCalls.PushBack(dc);
            });
    }

    // =========================================================================
    // IsVisible — Frustum culling (clip space AABB test)
    // =========================================================================
    bool NkRenderSystem::IsVisible(const NkAABB& aabb) const noexcept {
        // Test rapide : les 8 coins de l'AABB contre les 6 plans du frustum
        // Extrait les plans depuis mViewProjMatrix (Gribb-Hartmann)
        const NkMat4f& m = mViewProjMatrix;

        // Plans en row-major : left, right, bottom, top, near, far
        struct Plane { float32 a, b, c, d; };
        const Plane planes[6] = {
            { m[0][3]+m[0][0], m[1][3]+m[1][0], m[2][3]+m[2][0], m[3][3]+m[3][0] },
            { m[0][3]-m[0][0], m[1][3]-m[1][0], m[2][3]-m[2][0], m[3][3]-m[3][0] },
            { m[0][3]+m[0][1], m[1][3]+m[1][1], m[2][3]+m[2][1], m[3][3]+m[3][1] },
            { m[0][3]-m[0][1], m[1][3]-m[1][1], m[2][3]-m[2][1], m[3][3]-m[3][1] },
            { m[0][3]+m[0][2], m[1][3]+m[1][2], m[2][3]+m[2][2], m[3][3]+m[3][2] },
            { m[0][3]-m[0][2], m[1][3]-m[1][2], m[2][3]-m[2][2], m[3][3]-m[3][2] },
        };

        for (auto& p : planes) {
            // Teste le coin le plus positif (optimistic culling)
            const float32 px = (p.a > 0) ? aabb.max.x : aabb.min.x;
            const float32 py = (p.b > 0) ? aabb.max.y : aabb.min.y;
            const float32 pz = (p.c > 0) ? aabb.max.z : aabb.min.z;
            if (p.a * px + p.b * py + p.c * pz + p.d < 0.f) return false;
        }
        return true;
    }

} // namespace nkentseu