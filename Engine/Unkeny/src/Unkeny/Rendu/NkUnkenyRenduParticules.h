// =============================================================================
// NkUnkenyRenduParticules.h — dessiner les corps mous et les fluides d'une scene
//
// UN MATERIAU, UNE FACON DE SE MONTRER
//   ballon    eventail plein depuis le centre + reflet speculaire
//   gelee     cellules de la grille ombrees par leur COMPRESSION
//   tissu     cellules a rayures ; le VERSO plus sombre quand il se replie ;
//             une cellule dont un lien a rompu n'est plus dessinee (la dechirure)
//   fluides   halo translucide sous un coeur opaque : les particules se fondent
//             en une nappe ; l'eau blanchit quand elle va vite ; la PEAU d'un
//             blob brille, pas son coeur
//   sable     chaque grain a sa teinte (un tas uniforme ne se lit pas)
//   atomes    liaisons + spheres ; ils ROUGEOIENT quand ils s'agitent (additif)
//   corde     polyligne epaisse, lest metallique ; le pont, en planches
//
// Lit la scene, ne la modifie JAMAIS : le rendu peut sauter une trame ou tourner
// deux fois sans que la simulation change.
// =============================================================================
#pragma once

#include "NKGui/Core/NkGuiDrawList.h"
#include "Unkeny/Scene/NkUnkenyScene.h"

namespace nkentseu {
	namespace unkeny {

		enum class NkModeRenduParticules : uint8 {
			NK_ECLAIRE = 0, ///< le rendu "joli"
			NK_FILAIRE,		///< tous les liens et toutes les particules
			NK_CONTRAINTES, ///< liens colores par leur allongement (bleu comprime, rouge etire)
			NK_VITESSE,		///< particules colorees par leur vitesse
			NK_COUNT
		};
		const char *NkModeRenduParticulesNom(NkModeRenduParticules m) noexcept;

		struct NkOptionsRenduParticules {
				NkModeRenduParticules mode = NkModeRenduParticules::NK_ECLAIRE;
				bool liens = false;		 ///< liens par-dessus le rendu eclaire
				bool particules = false; ///< contours des particules par-dessus
				bool vitesses = false;	 ///< vecteurs vitesse
				bool epingles = true;	 ///< les particules fixees dans l'espace
		};

		/// Dessine tous les corps mous VISIBLES de la scene, avec la couleur de
		/// leur NkCorpsMou2D. Ne fait rien si la scene n'a pas de particules.
		void NkDessinerCorpsMous(nkgui::NkGuiDrawList &dl, NkScene &scene, const NkOptionsRenduParticules &o);

	} // namespace unkeny
} // namespace nkentseu
