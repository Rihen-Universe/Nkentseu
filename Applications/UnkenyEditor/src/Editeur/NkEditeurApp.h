//
// NkEditeurApp.h
// =============================================================================
// Description :
//   L'assemblage de l'editeur d'Unkeny : la fenetre (NkCanvasGuiApp), le
//   modele (la scene), l'interface (NkEditeurInterface.h) et l'entree.
//
// Caracteristiques :
//   - Bati sur renderer::NkCanvasGuiApp (NKCanvas) : fenetre, NKGui, polices,
//     et le cycle de vie mobile que NkEditorShell n'avait pas.
//   - L'entree NKGui est REMPLIE ICI, dans OnEvent, avec les correspondances de
//     NkEditorShell::HookEvents / MapEditKey. Sans shell, personne d'autre ne
//     le fait : pas une touche n'atteindrait un champ de saisie.
//   - `--selftest` repond AVANT toute fenetre (OnCommandLine).
//
// ⚠️ REECRIT LE 2026-09-29 : DE `NkEditorShell` VERS `NkCanvasGuiApp`
//   Le 2026-09-01, l'editeur avait quitte NkCanvasApp pour NkEditorShell, pour
//   son ancrage et sa palette. Le prix s'est mesure a l'ecran : toute
//   application batie sur le shell heritait du chrome de NKCode (barres
//   d'activite gauche et droite, onglets lateraux, indicateur de zoom), et la
//   disposition d'UE5 ne pouvait pas s'y dessiner. NK3DModeler avait deja fait
//   le choix inverse (docs/UI_SPEC.md §0) : on peint la disposition, et on ne
//   prend dans NKEditorKit que des PIECES (theme, arbre, navigateur de
//   contenu, champ de saisie). C'est ce que fait desormais l'editeur.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - une commande      -> NkActionEditeur + NkEditeurExecuter (Chrome.cpp)
//   - un panneau        -> une fonction NkEditeurDessiner*, appelee dans OnDraw
//   - un etat partage   -> NkEditeurModele.h (scene) ou NkEditeurInterface.h
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURAPP_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURAPP_H__

#include "Editeur/NkEditeurEntrees.h"
#include "Editeur/NkEditeurInterface.h"
#include "Editeur/NkEditeurModele.h"
#include "Editeur/NkEditeurSouris.h"
#include "Livraison/NkEditeurFenetreConstruire.h"

#include "NKCanvas/App/NkCanvasGuiApp.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkOptional.h"
#include "NKEditorKit/NkTheme.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKMemory/NKMemory.h"

namespace nkentseu {
	namespace editeur {

		class NkEditeurApp : public renderer::NkCanvasGuiApp {
			public:
				NkEditeurApp();

			protected:
				/// `--profil=N`, `--paysage`, `--simuler`, `--selftest`.
				///
				/// ⚠️ `--profil=` et `--paysage` existent pour qu'une capture
				/// d'ecran soit REPRODUCTIBLE (avec `--capture=` de la coquille) :
				/// un reglage qu'on ne peut atteindre qu'a la souris ne se verifie
				/// jamais en automatique.
				NkOptional<int> OnCommandLine(const NkVector<NkString> &args) override;
				bool OnGuiInit() override;
				void OnTick(float32 deltaTime) override;
				void OnDraw(nkgui::NkGuiDrawList &dl) override;
				bool OnEvent(const NkEvent &event) override;
				/// La croix de la fenetre, Alt+F4 : on ne ferme pas une scene
				/// modifiee sans demander (la boite de NkEditeurDessinerConfirmation).
				bool OnCloseRequested() override;
				/// ~13 px : un editeur dense, pas un jeu lu a bout de bras.
				float32 TaillePoliceCorps(const renderer::NkLayoutInfo &lay) const noexcept override;

			private:
				/// Touche OS -> touche NKGui (enfoncee / relachee).
				void MapperTouche(NkKey touche, bool enfoncee) noexcept;
				/// Les raccourcis globaux, APRES le dessin : un champ de saisie
				/// focalise les a vus passer et les garde pour lui.
				void Raccourcis(NkEditeurCadre &cadre);
				/// Les demandes de la barre de titre (reduire, agrandir, deplacer,
				/// redimensionner), consommees HORS du dessin : voir NkEditeurInterface.h.
				void AppliquerDemandesFenetre();

				// ⚠️ SUR LE TAS, ET CE N'EST PAS UN DETAIL : `Run<T>` construit
				// l'application SUR LA PILE, et le modele contient une scene, un
				// monde ECS et un monde physique. Ici l'objet reste petit.
				memory::NkUniquePtr<NkEditeurModele> mModele;
				memory::NkUniquePtr<NkEditeurInterface> mUi;
				/// « Jouer » : qui recoit le clavier, la manette et le doigt (29/09).
				memory::NkUniquePtr<NkEditeurEntrees> mEntrees;
				/// La fenetre « Construire » et la construction en cours (U5).
				memory::NkUniquePtr<NkEditeurConstruction> mConstruction;
				editorkit::NkTheme mTheme;
				NkPaletteEditeur mPalette;
				float32 mDernierDt = 1.f / 60.f;
				float32 mTempsIps = 0.f; ///< temps ecoule depuis le dernier releve d'ips
				int32 mTramesIps = 0;	 ///< trames comptees depuis ce releve
				NkString mSelectionDepart; ///< --selection= : l'entite choisie au demarrage
				NkString mSceneDepart;	   ///< --scene= : la scene ouverte au demarrage, au lieu de la scene neuve
				bool mNiveauGelee = false; ///< --niveau=gelee : le niveau du jalon Gelee au demarrage (essayer Espace a la main)
				NkString mCacherDepart;	   ///< --cacher= : les entites dont l'oeil est ferme au demarrage
				NkString mVerrouDepart;	   ///< --verrouiller= : celles dont le cadenas est ferme
				/// Les boutons de la souris, sans clic perdu entre deux trames.
				NkEditeurSouris mBoutons;
				bool mExempleNuit = false; ///< --exemple=nuit : la nuit au feu de camp (NkEditeurLumiere.h)
				bool mEclairageEteint = false; ///< --eclairage=off : la scene de depart, eclairage eteint
		};

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURAPP_H__
