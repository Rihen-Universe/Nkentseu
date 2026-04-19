#pragma once
// =============================================================================
// Nkentseu/Renderer/NkRenderSystem.h
// =============================================================================
// Système ECS de rendu — bridge entre NkWorld et NKRenderer.
//
// PIPELINE PAR FRAME :
//   1. UpdateActiveCamera()  — sélectionne la caméra de priorité max, calcule
//                              viewMatrix/projMatrix/viewProjMatrix
//   2. CollectLights()       — Query NkLightComponent → NkSceneContext3D.lights
//                              Conversion ecs::NkLightType → renderer::NkLightType
//   3. SubmitMeshes()        — Query NkMeshComponent+NkMaterialComponent+NkTransform
//                              Résolution handles GPU (NkResourceManager)
//                              Frustum culling (viewProj × AABB)
//                              NkRender3D::Submit(drawCall)
//   4. NkRender3D::EndScene(cmd) → flush GPU
//
// SCHEDULER :
//   InGroup(NkSystemGroup::Render) — après tous les autres groupes
// =============================================================================

#include "NKECS/System/NkSystem.h"
#include "NKECS/World/NkWorld.h"
#include "Nkentseu/ECS/Components/Core/NkTransform.h"
#include "Nkentseu/ECS/Components/Rendering/NkRenderComponents.h"
#include "NKRenderer/src/NkRenderer.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NkMath.h"

namespace nkentseu {

    using namespace math;

    class NkRenderSystem final : public ecs::NkSystem {
        public:
            NkRenderSystem() noexcept = default;
            ~NkRenderSystem() noexcept override = default;

            // ── Initialisation (avant NkScheduler::Init) ──────────────────
            void Init(renderer::NkRenderer* renderer,
                      NkICommandBuffer*     cmd) noexcept {
                mRenderer = renderer;
                mCmd      = cmd;
            }

            // ── Descriptor ECS ────────────────────────────────────────────
            [[nodiscard]] ecs::NkSystemDesc Describe() const override {
                return ecs::NkSystemDesc{}
                    .Reads<ecs::NkTransform>()
                    .Reads<ecs::NkMeshComponent>()
                    .Reads<ecs::NkMaterialComponent>()
                    .Reads<ecs::NkLightComponent>()
                    .Writes<ecs::NkCameraComponent>()
                    .InGroup(ecs::NkSystemGroup::Render)
                    .Sequential()
                    .Named("NkRenderSystem");
            }

            void Execute(ecs::NkWorld& world, float32 dt) noexcept override;

            // ── Configuration ─────────────────────────────────────────────
            void SetCommandBuffer(NkICommandBuffer* cmd) noexcept { mCmd = cmd; }
            void SetAmbientIntensity(float32 v)          noexcept { mAmbientIntensity = v; }
            void SetEnvMap(nk_uint64 handle)             noexcept { mEnvMapHandle = handle; }
            void SetWireframe(bool v)                    noexcept {
                if (mRenderer) mRenderer->Renderer3D().SetWireframe(v);
            }

            [[nodiscard]] const renderer::NkRendererStats& GetStats() const noexcept {
                return mRenderer ? mRenderer->GetStats() : mEmptyStats;
            }

        private:
            void UpdateActiveCamera(ecs::NkWorld& world) noexcept;
            void CollectLights(ecs::NkWorld& world) noexcept;
            void SubmitMeshes(ecs::NkWorld& world) noexcept;

            [[nodiscard]] bool IsVisible(const renderer::NkAABB& aabb) const noexcept;

            [[nodiscard]] static renderer::NkLightType
            ConvertLightType(ecs::NkLightType t) noexcept {
                switch (t) {
                    case ecs::NkLightType::Directional: return renderer::NkLightType::NK_DIRECTIONAL;
                    case ecs::NkLightType::Point:       return renderer::NkLightType::NK_POINT;
                    case ecs::NkLightType::Spot:        return renderer::NkLightType::NK_SPOT;
                    case ecs::NkLightType::Area:        return renderer::NkLightType::NK_AREA;
                    case ecs::NkLightType::Ambient:     return renderer::NkLightType::NK_AMBIENT;
                    default:                            return renderer::NkLightType::NK_DIRECTIONAL;
                }
            }

            renderer::NkRenderer*             mRenderer         = nullptr;
            NkICommandBuffer*                 mCmd              = nullptr;
            nk_uint64                         mEnvMapHandle     = 0;
            float32                           mAmbientIntensity = 0.2f;

            renderer::NkSceneContext3D        mSceneCtx;
            NkVector<renderer::NkDrawCall3D>  mOpaqueCalls;

            ecs::NkEntityId mActiveCameraId  = ecs::NkEntityId::Invalid();
            NkMat4f         mViewProjMatrix   = NkMat4f::Identity();

            renderer::NkRendererStats mEmptyStats{};
    };

} // namespace nkentseu