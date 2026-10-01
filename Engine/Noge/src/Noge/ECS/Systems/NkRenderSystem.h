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
//   4. NkRender3D::BeginScene + Submit ; le Flush est celui de la passe
//      Geometry du render graph, au Present() (2026-09-30, voir le .cpp).
//
// SCHEDULER :
//   InGroup(NkSystemGroup::Render) — après tous les autres groupes
//   ⚠️ NkEngineLayer ne le met PLUS dans son ordonnanceur (2026-09-30) : le
//      groupe Render y tournait dans OnUpdate, AVANT BeginFrame. La couche
//      l'execute dans OnRender, entre BeginFrame et Present.
//
// SANS FICHIER (2026-09-30) :
//   - NkMeshComponent::meshPath = "primitive:cube" (sphere, plane, quad,
//     cylinder, cone, capsule, icosphere) : primitive du NkMeshSystem ;
//   - NkMaterialComponent::SetColor(slot, couleur, metallic, rugosite) : la
//     couleur passe par le canal PAR DRAW CALL (tint / metallic / roughness),
//     celui que lit le nuanceur PBR ; le materiau lie reste l'instance de
//     repli du renderer.
// =============================================================================

#include "NKECS/System/NkSystem.h"
#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Rendering/NkRenderComponents.h"
#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkSceneContext.h"		  // NkSceneContext (membre par valeur)
#include "NKRenderer/Tools/Render3D/NkRender3D.h" // NkRender3D complet (SetWireframe inline)
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NKMath.h"

namespace nkentseu {

	using namespace math;

	class NkRenderSystem final : public ecs::NkSystem {
		public:
			NkRenderSystem() noexcept = default;
			~NkRenderSystem() noexcept override = default;

			// ── Initialisation (avant NkScheduler::Init) ──────────────────
			void Init(renderer::NkRenderer *renderer, NkICommandBuffer *cmd) noexcept {
				mRenderer = renderer;
				mCmd = cmd;
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

			void Execute(ecs::NkWorld &world, float32 dt) noexcept override;

			// ── Configuration ─────────────────────────────────────────────
			void SetCommandBuffer(NkICommandBuffer *cmd) noexcept {
				mCmd = cmd;
			}

			void SetAmbientIntensity(float32 v) noexcept {
				mAmbientIntensity = v;
			}

			void SetEnvMap(nk_uint64 handle) noexcept {
				mEnvMapHandle = handle;
			}

			void SetWireframe(bool v) noexcept {
				if (mRenderer && mRenderer->GetRender3D())
					mRenderer->GetRender3D()->SetWireframe(v);
			}

			// Modes de rendu façon Unreal/Blender (Solid / Wireframe / Unlit /
			// Rendered...). Le viewMode est propagé au NkSceneContext chaque frame.
			void SetViewMode(renderer::NkViewMode mode) noexcept {
				mViewMode = mode;
			}

			[[nodiscard]] renderer::NkViewMode GetViewMode() const noexcept {
				return mViewMode;
			}

			[[nodiscard]] const renderer::NkRendererStats &GetStats() const noexcept {
				return mRenderer ? mRenderer->GetStats() : mEmptyStats;
			}

			/// Flush dans Execute (l'ancien comportement). Par defaut NON : le
			/// Flush appartient a la passe Geometry du render graph (Present).
			void SetManualFlush(bool v) noexcept {
				mManualFlush = v;
			}

			/// Draw calls soumis par le dernier Execute (apres culling).
			[[nodiscard]] nk_uint32 GetLastSubmittedCount() const noexcept {
				return mLastSubmitted;
			}

			/// La camera retenue au dernier Execute (priorite maximale).
			[[nodiscard]] ecs::NkEntityId GetActiveCamera() const noexcept {
				return mActiveCameraId;
			}

			/// Sa matrice vue-projection (celle du culling), pour projeter un point.
			[[nodiscard]] const NkMat4f &GetViewProjection() const noexcept {
				return mViewProjMatrix;
			}

		private:
			void UpdateActiveCamera(ecs::NkWorld &world) noexcept;
			void CollectLights(ecs::NkWorld &world) noexcept;
			void SubmitMeshes(ecs::NkWorld &world) noexcept;

			[[nodiscard]] bool IsVisible(const renderer::NkAABB &aabb) const noexcept;

			// « primitive:<nom> » -> maillage built-in (cube, sphere, plane...).
			[[nodiscard]] static renderer::NkMeshHandle ResolvePrimitive(renderer::NkMeshSystem &meshSys,
																		 const char *name) noexcept;
			// Boite locale -> boite monde par la matrice ENTIERE (echelle et rotation comprises).
			[[nodiscard]] static renderer::NkAABB TransformAABB(const renderer::NkAABB &local,
																const NkMat4f &world) noexcept;

			[[nodiscard]] static renderer::NkLightType ConvertLightType(ecs::NkLightType t) noexcept {
				switch (t) {
					case ecs::NkLightType::Directional:
						return renderer::NkLightType::NK_DIRECTIONAL;
					case ecs::NkLightType::Point:
						return renderer::NkLightType::NK_POINT;
					case ecs::NkLightType::Spot:
						return renderer::NkLightType::NK_SPOT;
					case ecs::NkLightType::Area:
						return renderer::NkLightType::NK_AREA;
					// NkLightType::Ambient (ECS) -> pas de type GPU dédié : géré via
					// NkSceneContext::ambientIntensity (lumière ambiante globale).
					default:
						return renderer::NkLightType::NK_DIRECTIONAL;
				}
			}

			renderer::NkRenderer *mRenderer = nullptr;
			NkICommandBuffer *mCmd = nullptr;
			nk_uint64 mEnvMapHandle = 0;
			float32 mAmbientIntensity = 0.2f;

			renderer::NkSceneContext mSceneCtx;
			NkVector<renderer::NkDrawCall3D> mOpaqueCalls;
			renderer::NkViewMode mViewMode = renderer::NkViewMode::NK_SOLID;

			ecs::NkEntityId mActiveCameraId = ecs::NkEntityId::Invalid();
			NkMat4f mViewProjMatrix = NkMat4f::Identity();

			renderer::NkRendererStats mEmptyStats{};

			bool mManualFlush = false;			  ///< ancien comportement : Flush dans Execute (voir le .cpp)
			nk_uint32 mLastSubmitted = 0;		  ///< draw calls soumis par le dernier Execute

		public:
			// Les prefixes de maillage sans fichier.
			static constexpr const char *kPrimitivePrefix = "primitive:";
			static constexpr nk_uint32 kPrimitivePrefixLen = 10;
	};

} // namespace nkentseu