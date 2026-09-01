// =============================================================================
// NkUnkenyTheme.h — les jetons de couleur, en UN seul endroit
//
// A QUOI SERT CE FICHIER
//   Une couleur ecrite en dur dans un dessin est une couleur qu'on ne retrouvera
//   pas le jour ou le theme change — et il en reste toujours une.
//
// ⚠️ CE N'EST PAS UNE PALETTE IMPOSEE. Un jeu passe SON theme aux fonctions de
//   dessin ; celui-ci n'est qu'un defaut raisonnable, pour qu'un ecran soit
//   lisible avant qu'on ait choisi quoi que ce soit. Imposer une palette au
//   moteur ferait que tous les jeux se ressemblent, et surtout qu'aucun ne
//   puisse suivre une maquette.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - un jeton utile a TOUTES les applications -> ici
//   - une couleur propre a un jeu              -> chez ce jeu
// =============================================================================
#pragma once

#include "NKGui/Core/NkGuiTypes.h"

namespace nkentseu {
	namespace unkeny {

		using nkgui::NkColor;

		struct NkTheme {
				NkColor fond{18, 20, 28};
				NkColor panneau{30, 34, 46};
				NkColor panneauActif{52, 58, 74};
				NkColor bord{70, 78, 96};
				NkColor voile{8, 10, 16, 205};

				NkColor texte{236, 238, 245};
				NkColor texteFaible{150, 158, 176};

				NkColor accent{90, 200, 255};
				NkColor succes{90, 220, 140};
				NkColor alerte{235, 80, 80};
				NkColor or_{240, 190, 70};

				/// Arrondi des panneaux et boutons, en FRACTION de leur hauteur.
				/// Une valeur en pixels serait fausse des que la taille change —
				/// et elle change a chaque rotation.
				float32 arrondi = 0.28f;
				float32 epaisseurBord = 1.5f;
		};

		/// Le theme par defaut. `NkTheme` etant une simple structure, un jeu en
		/// prend une copie et modifie ce qu'il veut.
		inline const NkTheme &NkThemeDefaut() noexcept {
			static const NkTheme theme;
			return theme;
		}

	} // namespace unkeny
} // namespace nkentseu
