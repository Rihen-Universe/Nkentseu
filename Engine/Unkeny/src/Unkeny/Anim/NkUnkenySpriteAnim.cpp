// =============================================================================
// NkUnkenySpriteAnim.cpp — le systeme d'animation par images
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Unkeny/Anim/NkUnkenySpriteAnim.h"

#include "NKECS/World/NkWorld.h"
#include "Unkeny/Scene/NkUnkenyActif.h"
#include "Unkeny/Scene/NkUnkenyComposants.h"

namespace nkentseu {
	namespace unkeny {

		void NkAvancerAnimations(ecs::NkWorld &monde, float32 dt) {
			monde.Query<NkAnimSprite2D>().ForEach([&](ecs::NkEntityId id, NkAnimSprite2D &a) {
				a.evenementAtteint = false;
				// Une entite ETEINTE (NkUnkenyActif.h) ne s'anime pas.
				if (a.nbClips == 0 || !NkEntiteActive(monde, id)) {
					return;
				}
				const uint16 avant = a.ImageCourante();
				if (!a.enPause && !a.termine) {
					a.temps += dt;
				}
				const NkClipSprite &c = a.clips[a.clipCourant < a.nbClips ? a.clipCourant : 0];
				if (c.mode == NkModeLecture::NK_UNE_FOIS && c.imagesParSeconde > 0.f &&
					a.temps * c.imagesParSeconde >= static_cast<float32>(c.nombre)) {
					a.termine = true;
				}
				const uint16 apres = a.ImageCourante();
				if (c.imageEvent != 0xFFFFu && apres == c.premiere + c.imageEvent && (apres != avant || a.temps <= dt)) {
					a.evenementAtteint = true;
				}
				if (NkSprite2D *s = monde.Get<NkSprite2D>(id)) {
					a.RegionUV(s->uv0, s->uv1);
				}
			});
		}

	} // namespace unkeny
} // namespace nkentseu
