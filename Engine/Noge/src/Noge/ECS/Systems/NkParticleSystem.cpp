// =============================================================================
// Noge/ECS/Systems/NkParticleSystem.cpp — pont ECS -> VFX de NKRenderer
// =============================================================================
#include "NkParticleSystem.h"
#include "NKRenderer/Tools/VFX/NkVFXSystem.h"

namespace nkentseu {

	using namespace ecs;
	using namespace renderer;
	using namespace math;

	void NkParticleSystem::Execute(ecs::NkWorld &world, float32 dt) noexcept {
		(void)dt; // l'animation des particules est faite par le pipeline NKRenderer
		if (!mRenderer || !mRenderer->IsValid())
			return;
		NkVFXSystem *vfx = mRenderer->GetVFX();
		if (!vfx)
			return;

		world.Query<NkParticleEmitter, NkTransform>().ForEach(
			[&](NkEntityId, NkParticleEmitter &pe, const NkTransform &tf) {
				const NkVec3f pos = tf.GetWorldPosition();

				// Recréation si la configuration a changé.
				if (pe.dirty && pe.emitterId != 0) {
					NkEmitterId old{pe.emitterId};
					vfx->DestroyEmitter(old);
					pe.emitterId = 0;
					pe.dirty = false;
				}

				// Création paresseuse du premier émetteur.
				if (pe.emitterId == 0) {
					if (!pe.enabled)
						return;
					pe.desc.position = pos;
					NkEmitterId id = vfx->CreateEmitter(pe.desc);
					pe.emitterId = id.id;
					return;
				}

				// Suivi du transform + activation.
				NkEmitterId id{pe.emitterId};
				vfx->SetEmitterPos(id, pos);
				vfx->SetEmitterEnabled(id, pe.enabled);
			});
	}

} // namespace nkentseu
