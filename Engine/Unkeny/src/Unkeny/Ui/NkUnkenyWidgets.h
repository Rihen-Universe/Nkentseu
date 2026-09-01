// =============================================================================
// NkUnkenyWidgets.h — boutons et bascules, dessin ET test de zone
//
// A QUOI SERT CE FICHIER
//   Les elements d'interface qui se sont repetes dans les trois jeux ecrits
//   avant Unkeny : un bouton, une bascule a pastille, un panneau.
//
// ⚠️ POURQUOI LE DESSIN ET LE TEST SONT SEPARES
//   `NkBouton` DESSINE ; `NkDansRect` TESTE. Les reunir en un
//   `if (Bouton(...)) {...}` a la mode immediate serait plus court a ecrire et
//   ferait perdre le controle de l'ORDRE : dans les trois jeux, la bascule de
//   siege doit etre testee AVANT le plateau, et le retour au menu AVANT la
//   bascule. Un widget qui teste au moment ou il dessine impose l'ordre du
//   dessin a l'ordre des priorites d'entree — et ce sont deux ordres
//   differents.
//   NKGui offre deja le mode immediat pour qui le veut ; Unkeny offre l'autre.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - un element utile a plusieurs jeux -> ici
//   - un element propre a un jeu        -> chez ce jeu
// =============================================================================
#pragma once

#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiFont.h"
#include "Unkeny/Ui/NkUnkenyGeometrie.h"
#include "Unkeny/Ui/NkUnkenyTheme.h"

namespace nkentseu {
	namespace unkeny {

		using nkgui::NkGuiDrawList;
		using nkgui::NkGuiFont;

		/// Un panneau : fond arrondi + bord. La brique de tout le reste.
		void NkPanneau(NkGuiDrawList &dl, const NkRect &box, const NkTheme &th, bool actif = false);

		/// Un bouton avec son libelle centre. Il DESSINE ; le clic se teste par
		/// NkDansRect, au moment que l'application choisit.
		void NkBouton(NkGuiDrawList &dl, const NkRect &box, NkGuiFont *police, const char *libelle, const NkTheme &th,
					  bool actif = false);

		/// Un bouton portant une PASTILLE de couleur a gauche et un libelle a
		/// droite. C'est la forme qui dit deux choses d'un coup d'oeil : de qui
		/// il s'agit, et dans quel etat il est.
		/// `souligne` entoure le bouton d'un liseré d'accent — pour marquer celui
		/// dont c'est le tour.
		void NkBoutonPastille(NkGuiDrawList &dl, const NkRect &box, NkGuiFont *police, const char *libelle,
							  const NkColor &pastille, const NkTheme &th, bool actif, bool souligne);

		/// Trois barres horizontales : le retour au menu. Dessinees plutot
		/// qu'ecrites — un glyphe demanderait un atlas d'icones qu'un jeu n'a pas
		/// forcement.
		void NkBoutonMenu(NkGuiDrawList &dl, const NkRect &box, const NkTheme &th);

		/// Un voile plein ecran plus un panneau centre : la forme d'une fin de
		/// partie ou d'une confirmation.
		/// Rend le rectangle du panneau, pour que l'appelant y pose son texte.
		NkRect NkVoileEtPanneau(NkGuiDrawList &dl, const NkRect &ecran, const NkTheme &th, float32 largeur = 0.8f,
								float32 hauteur = 0.24f);

	} // namespace unkeny
} // namespace nkentseu
