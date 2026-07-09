#pragma once
// =============================================================================
// Noge/ECS/Systems/NkParticleSystem.h — pont ECS -> VFX (NKRenderer NkVFXSystem)
// =============================================================================
// Modèle NkRenderSystem : EMPRUNTE le renderer (Init), et pour chaque entité
// (NkParticleEmitter + NkTransform) crée l'émetteur VFX correspondant, le
// positionne depuis le transform et gère son activation. L'Update/Render des
// particules est piloté par le pipeline de NKRenderer (sous-système VFX).
// =============================================================================

#include "NKECS/System/NkSystem.h"
#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Rendering/NkParticleEmitter.h"
#include "NKRenderer/NkRenderer.h"

namespace nkentseu {

	class NkParticleSystem final : public ecs::NkSystem {
		public:
			NkParticleSystem() noexcept = default;

			void Init(renderer::NkRenderer *r) noexcept {
				mRenderer = r;
			}

			[[nodiscard]] ecs::NkSystemDesc Describe() const override {
				return ecs::NkSystemDesc{}
					.Writes<ecs::NkParticleEmitter>()
					.Reads<ecs::NkTransform>()
					.InGroup(ecs::NkSystemGroup::PostUpdate)
					.Sequential()
					.Named("NkParticleSystem");
			}

			void Execute(ecs::NkWorld &world, float32 dt) noexcept override;

		private:
			renderer::NkRenderer *mRenderer = nullptr; // EMPRUNTÉ à NkApplication
	};

} // namespace nkentseu
