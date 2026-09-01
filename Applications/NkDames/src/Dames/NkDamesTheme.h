// =============================================================================
// NkDamesTheme.h — LA source des couleurs et des proportions
//
// A QUOI SERT CE FICHIER
//   Un seul endroit ou vivent les couleurs. Une valeur ecrite en dur dans le
//   dessin est une valeur qu'on ne retrouvera pas le jour ou le theme change —
//   et il en reste toujours une.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - une couleur           -> ici
//   - une proportion PARTAGEE par plusieurs dessins (rayon, epaisseur) -> ici
//   - une proportion propre a UN dessin -> a cote de ce dessin, dans NkDamesEcran
// =============================================================================
#pragma once

#include "NKGui/Core/NkGuiTypes.h"

namespace nkentseu {
	namespace jeux {
		namespace dames {

			using nkgui::NkColor;

			// --- Fond et chrome -------------------------------------------
			const NkColor kFond(18, 20, 28);
			const NkColor kPanneau(30, 34, 46);
			const NkColor kPanneauActif(52, 58, 74);
			const NkColor kBord(70, 78, 96);
			const NkColor kVoile(8, 10, 16, 205);

			// --- Damier ---------------------------------------------------
			const NkColor kCaseClaire(222, 202, 170);
			const NkColor kCaseSombre(101, 67, 45);
			const NkColor kCadre(52, 38, 28);

			// --- Pieces ---------------------------------------------------
			const NkColor kPionBlanc(242, 240, 236);
			const NkColor kPionBlancOmbre(176, 172, 164);
			const NkColor kPionBlancTrait(200, 196, 188);
			const NkColor kPionNoir(38, 40, 48);
			const NkColor kPionNoirOmbre(14, 15, 20);
			const NkColor kPionNoirTrait(66, 70, 82);
			const NkColor kOr(240, 190, 70); ///< la couronne d'une dame

			// --- Interaction ----------------------------------------------
			const NkColor kSelection(90, 200, 255);
			const NkColor kDestination(90, 220, 140);

			// --- Texte ----------------------------------------------------
			const NkColor kTexte(236, 238, 245);
			const NkColor kTexteFaible(150, 158, 176);

			// --- Proportions partagees ------------------------------------
			/// Rayon d'un pion, en fraction de cellule. Au-dela de 0,42 les
			/// pions se touchent visuellement sur deux cases voisines.
			const float32 kRayonPion = 0.38f;
			/// Epaisseur d'un trait d'interface, en fraction de cellule.
			const float32 kTraitFin = 0.05f;
			const float32 kTraitEpais = 0.07f;

		} // namespace dames
	} // namespace jeux
} // namespace nkentseu
