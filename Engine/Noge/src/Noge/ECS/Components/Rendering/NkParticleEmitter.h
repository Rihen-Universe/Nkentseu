#pragma once
// =============================================================================
// Noge/ECS/Components/Rendering/NkParticleEmitter.h — composant émetteur VFX
// =============================================================================
// Pont vers le sous-système VFX de NKRenderer (renderer::NkVFXSystem). Décrit un
// émetteur de particules (feu/fumée/eau/étincelles...). Le NkParticleSystem (ECS)
// crée l'émetteur GPU/CPU correspondant et le positionne depuis le NkTransform.
// Presets façon Unreal/Unity : NkParticleEmitter::Fire()/Smoke()/Water()/Sparks().
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKECS/Core/NkTypeRegistry.h"
#include "NKRenderer/Tools/VFX/NkVFXSystem.h" // renderer::NkEmitterDesc

namespace nkentseu {
	namespace ecs {

		struct NkParticleEmitter {
				renderer::NkEmitterDesc desc; // configuration de l'émetteur
				uint64 emitterId = 0;		  // handle runtime (renderer::NkEmitterId.id)
				bool enabled = true;
				bool dirty = false; // desc modifiée -> recréer

				// ── Presets prêts à l'emploi ──────────────────────────────────────
				[[nodiscard]] static NkParticleEmitter Fire() noexcept {
					NkParticleEmitter e;
					auto &d = e.desc;
					d.shape = renderer::NkEmitterShape::CONE;
					d.coneAngle = 18.f;
					d.ratePerSec = 80.f;
					d.lifeMin = 0.4f;
					d.lifeMax = 1.0f;
					d.speedMin = 1.2f;
					d.speedMax = 3.0f;
					d.sizeStart = 0.35f;
					d.sizeEnd = 0.02f;
					d.colorStart = {1.0f, 0.6f, 0.1f, 1.f}; // orange vif
					d.colorEnd = {0.8f, 0.1f, 0.0f, 0.f};	// rouge -> transparent
					d.gravity = {0.f, 1.2f, 0.f};			// chaleur montante
					d.velocityDir = {0.f, 1.f, 0.f};
					d.velocityRand = 0.5f;
					d.blend = renderer::NkBlendMode::NK_ADDITIVE;
					return e;
				}

				[[nodiscard]] static NkParticleEmitter Smoke() noexcept {
					NkParticleEmitter e;
					auto &d = e.desc;
					d.shape = renderer::NkEmitterShape::CONE;
					d.coneAngle = 22.f;
					d.ratePerSec = 25.f;
					d.lifeMin = 1.5f;
					d.lifeMax = 3.5f;
					d.speedMin = 0.4f;
					d.speedMax = 1.2f;
					d.sizeStart = 0.3f;
					d.sizeEnd = 1.2f; // s'étale
					d.colorStart = {0.3f, 0.3f, 0.3f, 0.6f};
					d.colorEnd = {0.1f, 0.1f, 0.1f, 0.f};
					d.gravity = {0.f, 0.5f, 0.f};
					d.velocityDir = {0.f, 1.f, 0.f};
					d.velocityRand = 0.6f;
					d.blend = renderer::NkBlendMode::NK_ALPHA;
					return e;
				}

				[[nodiscard]] static NkParticleEmitter Water() noexcept {
					NkParticleEmitter e;
					auto &d = e.desc;
					d.shape = renderer::NkEmitterShape::CONE;
					d.coneAngle = 12.f;
					d.ratePerSec = 120.f;
					d.lifeMin = 0.6f;
					d.lifeMax = 1.4f;
					d.speedMin = 3.0f;
					d.speedMax = 6.0f;
					d.sizeStart = 0.08f;
					d.sizeEnd = 0.04f;
					d.colorStart = {0.6f, 0.8f, 1.0f, 0.9f};
					d.colorEnd = {0.4f, 0.6f, 0.9f, 0.f};
					d.gravity = {0.f, -9.8f, 0.f}; // retombe
					d.velocityDir = {0.f, 1.f, 0.f};
					d.velocityRand = 0.3f;
					d.blend = renderer::NkBlendMode::NK_ALPHA;
					return e;
				}

				[[nodiscard]] static NkParticleEmitter Sparks() noexcept {
					NkParticleEmitter e;
					auto &d = e.desc;
					d.shape = renderer::NkEmitterShape::POINT;
					d.ratePerSec = 0.f;
					d.burstCount = 40.f; // burst à la demande
					d.lifeMin = 0.3f;
					d.lifeMax = 0.8f;
					d.speedMin = 4.0f;
					d.speedMax = 9.0f;
					d.sizeStart = 0.05f;
					d.sizeEnd = 0.f;
					d.colorStart = {1.0f, 0.9f, 0.4f, 1.f};
					d.colorEnd = {1.0f, 0.4f, 0.1f, 0.f};
					d.gravity = {0.f, -9.8f, 0.f};
					d.velocityDir = {0.f, 1.f, 0.f};
					d.velocityRand = 1.f;
					d.blend = renderer::NkBlendMode::NK_ADDITIVE;
					return e;
				}
		};
		NK_COMPONENT(NkParticleEmitter)

	} // namespace ecs
} // namespace nkentseu
