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
// LA DISPOSITION — celle d'UE5 (2026-09-29)
//   gauche  : Hierarchie (la scene, ses entites, « + Entite »)
//             Placer des acteurs (le catalogue de simulation d'Unkeny)
//             Outils & appareil
//   centre  : Viseur
//   droite  : Inspecteur (les COMPOSANTS de l'entite, « Ajouter un composant »)
//             Monde (les proprietes de la scene : gravite, temperature...)
//   Chaque geste appelle une action de NkEditeurActions.h : les panneaux
//   n'ont pas de logique a eux, et les actions s'eprouvent sans fenetre.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - un nouveau panneau -> une classe ici + un enregistrement dans NkEditeurApp
//   - un reglage partage -> NkEditeurModele, jamais un membre de panneau
// =============================================================================
#pragma once

#include "Editeur/NkEditeurActions.h"
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

				/// Recadre la vue sur l'ensemble de la scene (a la prochaine trame si le
				/// viseur n'a pas encore sa taille).
				void CadrerSurTout() noexcept;

			private:
				void Souris(NkEditorFrameContext &ec, const nkgui::NkRect &aire);

				NkEditeurModele &mM;
				NkVec2f mPrecMonde{0.f, 0.f}; ///< point precedent du couteau et du pinceau
				bool mGeste = false;		  ///< saisie, coupe ou pinceau en cours
				/// Cadrer demande une taille de viseur : a l'Init, le panneau n'en a
				/// pas encore (1 x 1), et le cadrage donnait un zoom de fourmi. On
				/// cadre donc a la PREMIERE trame ou le viseur a sa vraie taille.
				bool mCadrageEnAttente = true;
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

		/// « Placer des acteurs » : le catalogue d'Unkeny/Simulation, par rubrique.
		/// Un clic arme l'outil Poser ; le clic suivant dans le viseur pose.
		class NkPanneauActeurs : public NkEditorPanel {
			public:
				explicit NkPanneauActeurs(NkEditeurModele &m) noexcept
					: NkEditorPanel("Placer des acteurs", NkEditorDockSide::NK_LEFT), mM(m) {
				}
				void OnUI(NkEditorFrameContext &ec) override;

			private:
				NkEditeurModele &mM;
		};

		class NkPanneauInspecteur : public NkEditorPanel {
			public:
				explicit NkPanneauInspecteur(NkEditeurModele &m) noexcept
					: NkEditorPanel("Inspecteur", NkEditorDockSide::NK_RIGHT), mM(m) {
				}
				void OnUI(NkEditorFrameContext &ec) override;

			private:
				void Transform(NkEditorFrameContext &ec, ecs::NkEntityId id);
				void Sprite(NkEditorFrameContext &ec, ecs::NkEntityId id);
				void Collisionneur(NkEditorFrameContext &ec, ecs::NkEntityId id);
				void Corps(NkEditorFrameContext &ec, ecs::NkEntityId id);
				void CorpsMou(NkEditorFrameContext &ec, ecs::NkEntityId id);
				void Source(NkEditorFrameContext &ec, ecs::NkEntityId id);
				void Animation(NkEditorFrameContext &ec, ecs::NkEntityId id);
				void Ajouter(NkEditorFrameContext &ec, ecs::NkEntityId id);
				/// L'en-tete d'une section : son nom, et « Retirer ». Rend false si
				/// la section vient d'etre retiree (ne plus rien lire du composant).
				bool EnTete(NkEditorFrameContext &ec, ecs::NkEntityId id, NkComposantEditeur c, bool &ouvert);

				NkEditeurModele &mM;
				ecs::NkEntityId mNomDe;	 ///< l'entite dont `mNom` est le tampon
				char mNom[32] = {};
				bool mListeAjout = false; ///< la liste « Ajouter un composant » est depliee
		};

		/// « Monde » : les proprietes de la SCENE, pas d'une entite.
		class NkPanneauMonde : public NkEditorPanel {
			public:
				explicit NkPanneauMonde(NkEditeurModele &m) noexcept
					: NkEditorPanel("Monde", NkEditorDockSide::NK_RIGHT), mM(m) {
				}
				void OnUI(NkEditorFrameContext &ec) override;

			private:
				NkEditeurModele &mM;
		};

		class NkPanneauOutils : public NkEditorPanel {
			public:
				explicit NkPanneauOutils(NkEditeurModele &m) noexcept
					: NkEditorPanel("Outils & appareil", NkEditorDockSide::NK_LEFT), mM(m) {
				}
				void OnUI(NkEditorFrameContext &ec) override;

			private:
				NkEditeurModele &mM;
		};

		/// La barre d'outils du shell (SetToolbar) : Jouer / Pause / Pas / Arreter,
		/// Nouveau / Ouvrir / Enregistrer. `user` = le NkEditeurModele.
		void NkBarreOutilsEditeur(NkEditorFrameContext &ec, void *user);

	} // namespace editeur
} // namespace nkentseu
