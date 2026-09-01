// =============================================================================
// NkEditeurPanneaux.h — les panneaux de l'editeur, en NkEditorPanel
//
// ⚠️ CE FICHIER A ETE REECRIT LE 2026-09-01, ET LA RAISON VAUT D'ETRE LUE
//   Sa premiere version dessinait a la main une barre d'outils, une liste de
//   hierarchie, un inspecteur et une barre d'etat, dans des rectangles calcules
//   par un `NkDispoEditeur` maison. NKEditorKit porte tout cela -- coquille
//   ancrable, panneaux, palette de commandes, barres d'activite -- et la regle
//   du depot est explicite : on cherche dans le kit AVANT d'ecrire un element
//   d'interface. Je ne l'avais pas fait.
//
//   Ce que la reecriture rend, en plus de supprimer le doublon :
//     - l'ANCRAGE : on deplace, on ferme, on rouvre, la disposition se sauve ;
//     - la PALETTE DE COMMANDES (Ctrl+Maj+P) et le menu « Affichage » ;
//     - le HIT-TEST OCCULTE (`ctx.InputHits`) -- ma version testait un simple
//       « le point est-il dans le rectangle », donc un clic tombe sur un panneau
//       flottant AU-DESSUS du viseur atteignait quand meme la scene.
//
// COMMENT UN PANNEAU MARCHE ICI
//   Il derive de `NkEditorPanel`, il implemente `OnUI(ec)`, et le shell l'appelle
//   quand il est ouvert. Il ne connait AUCUN autre panneau : tout ce qui est
//   partage passe par `NkEditeurModele` (cf. NkEditeurModele.h).
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - un nouveau panneau -> une classe ici + un enregistrement dans NkEditeurApp
//   - un reglage partage -> NkEditeurModele, jamais un membre de panneau
// =============================================================================
#pragma once

#include "Editeur/NkEditeurModele.h"

#include "NKEditorKit/NkEditorContext.h"
#include "NKEditorKit/NkEditorPanel.h"

namespace nkentseu {
	namespace editeur {

		using editorkit::NkEditorDockSide;
		using editorkit::NkEditorFrameContext;
		using editorkit::NkEditorPanel;

		// =====================================================================
		// Le viseur — la scene, et le seul panneau qui prend la souris
		// =====================================================================
		class NkPanneauViseur : public NkEditorPanel {
			public:
				explicit NkPanneauViseur(NkEditeurModele &m) noexcept
					: NkEditorPanel("Viseur", NkEditorDockSide::NK_CENTER), mM(m) {
				}

				void OnUI(NkEditorFrameContext &ec) override;

				/// Recadre la vue sur l'ensemble de la scene.
				void CadrerSurTout() noexcept;

			private:
				void PoserEntite(const NkVec2f &monde);
				void Souris(NkEditorFrameContext &ec, const nkgui::NkRect &aire);

				NkEditeurModele &mM;
		};

		// =====================================================================
		// La hierarchie — la liste des entites, et la selection
		// =====================================================================
		class NkPanneauHierarchie : public NkEditorPanel {
			public:
				explicit NkPanneauHierarchie(NkEditeurModele &m) noexcept
					: NkEditorPanel("Hierarchie", NkEditorDockSide::NK_LEFT), mM(m) {
				}

				void OnUI(NkEditorFrameContext &ec) override;

			private:
				NkEditeurModele &mM;
		};

		// =====================================================================
		// L'inspecteur — les proprietes de la selection
		// =====================================================================
		class NkPanneauInspecteur : public NkEditorPanel {
			public:
				explicit NkPanneauInspecteur(NkEditeurModele &m) noexcept
					: NkEditorPanel("Inspecteur", NkEditorDockSide::NK_RIGHT), mM(m) {
				}

				void OnUI(NkEditorFrameContext &ec) override;

			private:
				NkEditeurModele &mM;
		};

		// =====================================================================
		// Outils & appareil — ce qui pilote le viseur sans etre dedans
		//
		// ⚠️ L'APPAREIL SIMULE N'EST PAS UN GADGET : c'est ce qui permet de voir
		//    un debordement mobile AVANT de deployer. La regle du depot le dit --
		//    « un bouton qui deborde sous l'indicateur de geste est
		//    INATTEIGNABLE » -- et le defaut se decouvre sinon sur le telephone,
		//    quand il coute le plus cher.
		// =====================================================================
		class NkPanneauOutils : public NkEditorPanel {
			public:
				explicit NkPanneauOutils(NkEditeurModele &m) noexcept
					: NkEditorPanel("Outils & appareil", NkEditorDockSide::NK_LEFT), mM(m) {
				}

				void OnUI(NkEditorFrameContext &ec) override;

			private:
				NkEditeurModele &mM;
		};

	} // namespace editeur
} // namespace nkentseu
