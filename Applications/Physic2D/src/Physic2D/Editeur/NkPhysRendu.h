// =============================================================================
// NkPhysRendu.h — dessiner le monde dans la vue de l'editeur
//
// Lit le monde, ne le modifie JAMAIS (const partout) : le rendu peut sauter
// une trame, tourner deux fois, ou pas du tout (--selftest) sans que la
// simulation en soit changee.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#pragma once

#include "NKGui/Core/NkGuiDrawList.h"
#include "Physic2D/Physique/NkPhysMonde.h"

namespace nkentseu {
	namespace physic2d {

		/// Camera orthographique 2D. Le monde a Y vers le HAUT, l'ecran vers le BAS.
		struct NkCamera2D {
				NkV2 centre = NkV2(0.f, 600.f);
				float32 zoom = 0.5f; ///< pixels par centimetre
				nkgui::NkRect vue = {0.f, 0.f, 800.f, 600.f};

				nkgui::NkVec2 Ecran(const NkV2 &w) const noexcept {
					return nkgui::NkVec2{vue.x + vue.w * 0.5f + (w.x - centre.x) * zoom,
										 vue.y + vue.h * 0.5f - (w.y - centre.y) * zoom};
				}
				NkV2 Monde(float32 sx, float32 sy) const noexcept {
					return NkV2(centre.x + (sx - (vue.x + vue.w * 0.5f)) / zoom, centre.y - (sy - (vue.y + vue.h * 0.5f)) / zoom);
				}
		};

		enum class NkModeVue : uint8 {
			NK_ECLAIRE = 0, ///< le rendu "joli"
			NK_FILAIRE,		///< tous les liens et toutes les particules
			NK_CONTRAINTES, ///< liens colores par leur allongement
			NK_VITESSE,		///< particules colorees par leur vitesse
			NK_COUNT
		};
		const char *NkModeVueNom(NkModeVue m) noexcept;

		struct NkOptionsVue {
				NkModeVue mode = NkModeVue::NK_ECLAIRE;
				bool grille = true;
				bool liens = false;
				bool particules = false;
				bool vitesses = false;
				bool epingles = true;
				uint32 selectionId = 0;
				float32 temps = 0.f;
		};

		void NkDessinerMonde(nkgui::NkGuiDrawList &dl, const NkMonde &m, const NkCamera2D &cam,
							 const NkOptionsVue &o) noexcept;

		/// Le pas de grille retenu (cm) : le plus fin qui reste lisible.
		float32 NkPasGrille(float32 zoom) noexcept;

	} // namespace physic2d
} // namespace nkentseu
