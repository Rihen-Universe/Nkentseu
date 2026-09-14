#pragma once
// -----------------------------------------------------------------------------
// @File    NkPanneauxSonde.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// =============================================================================
// NkPanneauxSonde.h — MESURE de la DISPOSITION de Nogee (`--panneaux-sonde`).
//
// CE QU'ELLE MESURE, ET POURQUOI ELLE NE SAIT PAS LA REPONSE
//   Elle imprime la geometrie REELLE de la frame : les cotes des trois bandes et
//   le rectangle de chaque panneau, en pixels ET en fraction de la fenetre. Elle
//   ne compare a RIEN et ne rend aucun verdict : la comparaison est faite par
//   `scratchpad/agent-panneaux/verdict_disposition.py`, qui lit les attendus
//   DANS `NK3DModeler/Shell/NkModelerUI.h` (NkLayout::Compute) a chaque
//   execution. Un attendu recopie ici se perimerait le jour ou Rodolf changerait
//   une fraction du modeleur, et crierait rouge sur un montage correct.
//
// TOUT EST DERIVE, RIEN N'EST RECOPIE
//   titleH   = ui.titleBarH                    (pose par le shell a chaque frame)
//   corps    = ui.dockSpaceRect                (le rect passe a DockSpace)
//   toolbarH = corps.y - titleH                <- et non « 34 », le nombre demande
//   footerH  = H - (corps.y + corps.h)
//   rect d'un panneau = ui.dockNodes[PanelDockNode(titre)].rect
//   C'est ce qui permet de distinguer « la bande est demandee » de « la bande
//   occupe des pixels » — la difference exacte qui a fait passer le lot
//   precedent pour livre alors qu'une cote sur deux ne servait a rien.
//
// LE ZERO SE PROUVE AVANT TOUT LE RESTE
//   Les compteurs `menus_emis` / `entrees_emises` / `outils_emis` sont poses par
//   l'application quand elle dessine ses barres. Ils sont relevés une premiere
//   fois SANS aucune barre : ils doivent valoir 0. Un compteur qui rend autre
//   chose que 0 sans cible ne mesure pas ce qu'on croit.
//
// GARDES
//   * Aucune injection de souris ni de clavier. La sonde LIT, elle ne pilote rien.
//   * Elle ne photographie rien. Elle imprime des nombres.
//   * Quand elle est active, la fenetre porte un titre qui dit qu'elle n'est pas
//     le produit, et elle se ferme d'elle-meme apres sa mesure.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKEditorKit/NkEditorContext.h"
#include "NKGui/Core/NkGuiContext.h"
#include <cstdio>

namespace nkentseu {
	namespace editorkit {
		class NkEditorShell;
	}
	namespace noge {

		/// Titre impose a la fenetre quand la sonde tourne. Une fenetre d'agent
		/// s'est deja fait photographier a la place du produit : le titre est le
		/// seul endroit qu'on regarde avant de croire une capture.
		inline const char *NkPanneauxSondeTitre() noexcept {
			return "*** SONDE DE MESURE - CETTE FENETRE N'EST PAS LE PRODUIT ***";
		}

		/// Etat de la sonde. Les trois compteurs sont poses par les barres de
		/// l'application (voir `NkPanneauxCompteurs`), pas par la sonde.
		struct NkPanneauxSondeEtat {
				bool active = false;
				int32 frame = 0;
				bool reported = false;
				/// Frame de la mesure. Assez tard pour que le dock ait ete bootstrappe
				/// ET que les polices soient chargees ; assez tot pour ne pas faire
				/// attendre. `BootstrapDocking` agit a la premiere frame de dessin.
				int32 frameMesure = 60;
				/// `--panneaux-sonde-redim` : apres la premiere mesure, la fenetre
				/// CHANGE DE TAILLE et on remesure. C'est le NEGATIF de la
				/// disposition : une zone posee en pixels garderait sa taille et sa
				/// FRACTION bougerait ; une zone en fraction garde sa fraction et
				/// ses pixels bougent. Les deux mesures se lisent ensemble, jamais
				/// separement.
				///
				/// Le redimensionnement passe par `NkEditorShell::Resize`, c'est-a-dire
				/// la fonction que le geste de la souris appelle -- aucune injection
				/// d'entree, la garde tient.
				bool redim = false;
				/// `--panneaux-pose` : apres la mesure, la fenetre RESTE ouverte le
				/// temps d'etre photographiee -- la meme image que les nombres qui
				/// viennent d'etre imprimes, ce qui est la seule facon de calibrer
				/// l'instrument de capture sur un cas dont on connait la reponse.
				/// ⚠️ GARDE-FOU : elle se referme SEULE au bout de `frameFinPose`.
				/// Une fenetre d'agent laissee ouverte sur le bureau de Rodolf est
				/// exactement ce qu'on s'interdit.
				bool pose = false;
				int32 frameFinPose = 0;
				int32 phase = 0;	  ///< 0 = avant, 1 = apres le redimensionnement
				int32 frameApres = 0; ///< image ou la seconde mesure sera prise
		};

		/// Compteurs EMIS PAR L'APPLICATION pendant qu'elle dessine ses barres.
		/// Remis a zero au debut de chaque frame par la barre elle-meme : un
		/// compteur cumulatif sur 60 frames dirait 60 fois trop.
		struct NkPanneauxCompteurs {
				int32 menus = 0;	 ///< entrees de la barre de menus (Fichier, Edition...)
				int32 entrees = 0;	 ///< entrees DECLAREES (separateurs exclus), menu ferme compris
				int32 entreesDessinees = 0; ///< entrees reellement peintes cette image
				int32 outils = 0;	 ///< elements de la barre d'outils
				float32 largeurMenus = 0.f; ///< px consommes par la barre de menus (preuve de dessin)
				float32 largeurOutils = 0.f;
		};

		/// Facteurs du redimensionnement d'essai. Volontairement NON ronds :
		/// 1600 x 0,75 = 1200 et 900 x 0,78 = 702 ne partagent aucun diviseur
		/// commode avec les fractions mesurees, donc une coincidence numerique
		/// ne peut pas faire passer le negatif pour vert.
		inline constexpr float32 kRedimW = 0.75f;
		inline constexpr float32 kRedimH = 0.78f;

		inline NkPanneauxSondeEtat &NkPanneauxSonde() noexcept {
			static NkPanneauxSondeEtat s;
			return s;
		}

		inline NkPanneauxCompteurs &NkPanneauxCpt() noexcept {
			static NkPanneauxCompteurs c;
			return c;
		}

		/// Imprime une ligne par panneau + une ligne par bande. Appelee depuis
		/// l'overlay du shell, donc APRES `DockSpace` et `DrawPanels` : les rects
		/// des noeuds sont ceux de la frame courante, pas ceux d'hier.
		///
		/// `titres` : les panneaux a mesurer, dans l'ordre ou l'application les a
		/// enregistres. La liste est passee par l'appelant plutot que devinee :
		/// le shell n'expose pas ses titres, et une liste ecrite ici en double
		/// finirait par diverger de celle des `AddPanel`.
		void NkPanneauxSondeMesurer(editorkit::NkEditorFrameContext &ec,
									editorkit::NkEditorShell *shell, const char *const *titres,
									int32 nTitres) noexcept;

	} // namespace noge
} // namespace nkentseu
