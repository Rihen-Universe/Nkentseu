// =============================================================================
// NkEditeurApp.h — l'editeur d'Unkeny
//
// A QUOI SERT CETTE APPLICATION
//   Voir et modifier une scene Unkeny : poser des entites, les deplacer, lire
//   et changer leurs composants, lancer la physique, et VERIFIER la mise en
//   page sur les zones sures des appareils qu'on n'a pas sous la main.
//
// ⚠️ POURQUOI ELLE VIT DANS Applications/ ET NON DANS Engine/
//   Un moteur ne contient pas son outil. L'editeur CONSOMME Unkeny ; l'inverse
//   ferait qu'un jeu embarquerait l'editeur, et que le moteur ne puisse plus
//   etre construit sans lui.
//
// ⚠️ ET POURQUOI ELLE EXISTE DES MAINTENANT
//   Elle est le PREMIER consommateur d'Unkeny. Un moteur sans consommateur ne
//   se prouve pas : ce depot a deja quatre systemes ecrits sans usage
//   (NKReflection longtemps, l'interpreteur blueprint, NkEditorInspector,
//   NKGraph), et chacun a dormi jusqu'a ce qu'un consommateur le reveille.
//   L'editeur exerce la scene, les composants, la vue, le rendu, les tuiles et
//   la physique — donc un defaut d'Unkeny se voit ICI, tout de suite.
//
// L'ORGANISATION
//   NkEditeurAppareils.h   profils d'appareils, zones sures simulees
//   NkEditeurViseur.{h,cpp} le viseur : grille, scene, selection, deplacement
//   NkEditeurPanneaux.{h,cpp} hierarchie, inspecteur, barre d'outils
//   NkEditeurApp.{h,cpp}   l'assemblage et les entrees
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - un panneau        -> NkEditeurPanneaux
//   - un outil de viseur-> NkEditeurViseur, plus un mode dans NkOutil
//   - une capacite du MOTEUR -> Unkeny, jamais ici
// =============================================================================
#pragma once

#include "Editeur/NkEditeurAppareils.h"
#include "NKCanvas/App/NkCanvasGuiApp.h"
#include "Unkeny/Unkeny.h"

namespace nkentseu {
	namespace editeur {

		using namespace nkentseu::unkeny;

		/// Ce que fait un clic dans le viseur. Un seul outil actif a la fois :
		/// deux outils qui repondent au meme clic se disputent l'evenement, et
		/// celui qui gagne depend de l'ordre du code.
		enum class NkOutil : uint8 {
			NK_SELECTION = 0, ///< choisir et deplacer une entite
			NK_POSER,		  ///< poser une entite neuve
			NK_PEINDRE,		  ///< peindre des tuiles
			NK_EFFACER		  ///< retirer une tuile ou une entite
		};

		/// La disposition de l'editeur. Recalculee a chaque changement de taille.
		struct NkDispoEditeur {
				nkgui::NkRect barre{0.f, 0.f, 0.f, 0.f};	  ///< outils, en haut
				nkgui::NkRect hierarchie{0.f, 0.f, 0.f, 0.f}; ///< liste, a gauche
				nkgui::NkRect viseur{0.f, 0.f, 0.f, 0.f};	  ///< la scene, au centre
				nkgui::NkRect inspecteur{0.f, 0.f, 0.f, 0.f}; ///< proprietes, a droite
				nkgui::NkRect etat{0.f, 0.f, 0.f, 0.f};		  ///< pied de page, mesures

				/// L'aire d'APPAREIL SIMULE, a l'interieur du viseur. Elle a le
				/// rapport largeur/hauteur du profil choisi : c'est elle qu'on
				/// regarde pour juger une mise en page mobile.
				nkgui::NkRect appareil{0.f, 0.f, 0.f, 0.f};

				void Calculer(const renderer::NkLayoutInfo &info, const NkProfilAppareil &profil) noexcept;
		};

		class NkEditeurApp : public renderer::NkCanvasGuiApp {
			public:
				NkEditeurApp();

			protected:
				NkOptional<int> OnCommandLine(const NkVector<NkString> &args) override;
				bool OnGuiInit() override;
				void OnLayout(const renderer::NkLayoutInfo &info) override;
				bool OnPointer(const NkPointer &p) override;
				bool OnKeyPress(const NkKeyPressEvent &event) override;
				void OnTick(float32 deltaTime) override;
				void OnDraw(nkgui::NkGuiDrawList &dl) override;

			private:
				void ConstruireSceneExemple();
				void PoserEntite(const NkVec2f &monde);
				void SupprimerSelection();
				void CadrerSurTout();

				NkScene mScene;
				NkCarteTuiles mCarte;
				NkDispoEditeur mDispo;
				NkTheme mTheme;

				NkOutil mOutil = NkOutil::NK_SELECTION;
				int32 mProfil = 0;
				bool mPaysage = false;
				bool mSimuler = false; ///< la physique tourne-t-elle ?
				bool mVoirCollisionneurs = true;
				bool mVoirGrille = true;

				ecs::NkEntityId mSelection;
				bool mADesSelection = false;

				/// Deplacement en cours : on retient le decalage entre le point
				/// saisi et le centre de l'entite. Sans lui, l'entite saute pour
				/// se centrer sous le curseur des le premier pixel de glissement.
				bool mDeplace = false;
				NkVec2f mDecalageSaisie{0.f, 0.f};

				/// Panoramique du viseur au bouton droit ou a deux doigts.
				bool mPanoramique = false;
				NkVec2f mDernierPointeur{0.f, 0.f};

				NkStatsRendu mStats;
				uint32 mGraine = 20260901u;
		};

	} // namespace editeur
} // namespace nkentseu
