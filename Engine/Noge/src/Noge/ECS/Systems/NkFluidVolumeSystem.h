#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Noge/ECS/Systems/NkFluidVolumeSystem.h — pont ECS -> registre de fluides
// (renderer::NkFluidVolumeStore), 2026-09-06.
//
// Même partage que `NkParticleSystem` : pour chaque entité portant
// (`NkFluidVolume` + `NkTransform`), il CRÉE le volume, le POSITIONNE depuis le
// transform, l'active ou l'éteint, et le DÉTRUIT quand l'entité disparaît. Il ne
// fait PAS avancer la simulation — `NkVFXSystem::Update` est le seul appelant de
// `StepAll`, une fois par image.
//
// ⚠️ IL PREND LE REGISTRE, PAS LE RENDERER, et ce n'est pas un détail de style.
// `NkParticleSystem` prend un `renderer::NkRenderer*` : aucun banc ne peut donc
// l'exercer sans device, et personne ne l'a jamais fait tourner sans fenêtre. Le
// registre de fluides est CPU pur ; en le prenant lui, ce pont s'éprouve
// headless — c'est ce que fait `NkFluidEcsProbe`. L'application le câble en une
// ligne : `Init(&renderer->GetVFX()->FluidVolumes())`.
// =============================================================================
#include "NKECS/System/NkSystem.h"
#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Rendering/NkFluidVolume.h"
#include "NKRenderer/Tools/VFX/NkFluidVolumeStore.h"

namespace nkentseu {

	class NkFluidVolumeSystem final : public ecs::NkSystem {
		public:
			NkFluidVolumeSystem() noexcept = default;

			// Le registre est EMPRUNTÉ : il appartient au NkVFXSystem, qui le fait
			// avancer. Il doit survivre à ce système.
			void Init(renderer::NkFluidVolumeStore *store) noexcept {
				mStore = store;
			}

			[[nodiscard]] ecs::NkSystemDesc Describe() const override {
				return ecs::NkSystemDesc{}
					.Writes<ecs::NkFluidVolume>()
					.Reads<ecs::NkTransform>()
					.InGroup(ecs::NkSystemGroup::PostUpdate)
					.Sequential()
					.Named("NkFluidVolumeSystem");
			}

			void Execute(ecs::NkWorld &world, float32 dt) noexcept override;

			// ── Témoins ────────────────────────────────────────────────────────
			// Comptés APRÈS le geste, jamais avant : un compteur d'intentions ne dirait
			// pas si le volume a été créé. `mOrphelins` compte les volumes du registre
			// qu'aucune entité ne réclame plus et que ce système a détruits.
			uint32 CreatedTotal() const noexcept {
				return mCreated;
			}
			uint32 FollowedLastFrame() const noexcept {
				return mFollowed;
			}
			uint32 ReapedTotal() const noexcept {
				return mReaped;
			}

		private:
			renderer::NkFluidVolumeStore *mStore = nullptr; // EMPRUNTÉ
			// Les poignées que CE système a créées. On ne ramasse jamais un volume
			// venu d'ailleurs : un registre est partagé, et une liste d'exclusion ne
			// protège que ce qu'on a pensé à y mettre.
			NkVector<uint64> mOwned;
			uint32 mCreated = 0;
			uint32 mFollowed = 0;
			uint32 mReaped = 0;
	};

} // namespace nkentseu
