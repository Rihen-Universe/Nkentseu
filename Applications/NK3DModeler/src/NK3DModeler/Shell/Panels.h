#pragma once
// =============================================================================
// Panels.h — les panneaux de NK3DModeler, aux places de l'ecran A (Banani).
//
// SQUELETTE ASSUME. Chaque panneau pose sa STRUCTURE — en-tetes, colonnes,
// lignes representatives — et rien de fonctionnel. C'est deliberé : la
// disposition et la grammaire visuelle se valident a l'oeil AVANT qu'on branche
// une vue 3D ou une pile de modificateurs. Brancher d'abord et disposer ensuite
// obligerait a tout redecouper une fois le contenu en place.
//
// ADJACENCES IMPOSEES (UI_SPEC 2, decidees par Rihen) :
//   hierarchie a GAUCHE · proprietes AU-DESSUS des details a DROITE ·
//   navigateur de projet EN BAS sur toute la largeur · vue 3D au CENTRE.
//
// DEUX ECARTS PAR RAPPORT A LA MAQUETTE, assumes et expliques ici :
//   1. La barre d'outils principale ajoutee par Banani est GARDEE mais AMINCIE.
//      Son commutateur Objet/Edition est une bonne idee -- il rend visible un
//      etat qui, chez Blender, n'existe que dans un menu deroulant. Le reste de
//      ses boutons fait doublon avec les menus de la vue : retire.
//   2. Les modificateurs y sont un MENU DEROULANT. C'est faux : la demande est
//      une PILE, qu'on empile, reordonne, active et applique. Le deroulant
//      garde sa place, mais comme bouton « ajouter », pas comme liste.
// =============================================================================

#include "NKEditorKit/NkEditorKit.h"
#include "NKEditorKit/NkShortcutTable.h"
#include "NKEditorKit/NkTheme.h"

namespace nkentseu {
	namespace nk3d {

		using namespace nkentseu::editorkit;

		// Etat partage par les panneaux. Un seul objet passe par reference : les
		// panneaux ne se connaissent pas entre eux, ils lisent le meme etat.
		struct NkModelerState {
				NkThemeLibrary themes;
				NkShortcutTable shortcuts;
				bool editMode = false; ///< false = objet, true = edition
				int32 selectedObject = 0;
		};

		// ── HIERARCHIE (gauche) ─────────────────────────────────────────────────
		class HierarchyPanel : public NkEditorPanel {
			public:
				explicit HierarchyPanel(NkModelerState &st) noexcept
					: NkEditorPanel("Hierarchie", NkEditorDockSide::NK_LEFT), mSt(st) {}

				void OnUI(NkEditorFrameContext &ec) override {
					ec.Text("Rechercher...");
					ec.Separator();
					ec.Text("Nom                          Type");
					ec.Separator();
					// Arborescence de demonstration : elle montre les DEUX choses que
					// le panneau doit savoir faire -- l'imbrication et la colonne de
					// type -- sans qu'aucune scene ne soit chargee.
					static const char *const kRows[] = {
						"v Scene",
						"    Cube                   Maillage",
						"    Sphere                 Maillage",
						"  v Groupe                 Dossier",
						"      Roue                 Maillage",
						"      Axe                  Maillage",
					};
					for (int32 i = 0; i < 6; ++i)
						ec.Text(kRows[i]);
					ec.Separator();
					ec.Text("6 objets (1 selectionne)");
				}

			private:
				NkModelerState &mSt;
		};

		// ── VUE 3D (centre) ─────────────────────────────────────────────────────
		class ViewportPanel : public NkEditorPanel {
			public:
				explicit ViewportPanel(NkModelerState &st) noexcept
					: NkEditorPanel("Vue 3D", NkEditorDockSide::NK_CENTER), mSt(st) {}

				void OnUI(NkEditorFrameContext &ec) override {
					// LES MENUS DE COMMANDES, premier des quatre chemins d'acces de
					// UI_SPEC 9bis. Ils changent avec le mode : c'est la seule chose
					// qui distingue vraiment le mode objet du mode edition ici.
					if (mSt.editMode)
						ec.Text("[Ajouter v] [Maillage v] [Sommet v] [Arete v] [Face v]");
					else
						ec.Text("[Ajouter v] [Objet v] [Selection v]");
					ec.Text("[Perspective v] [Eclaire v] [Affichage v]     "
							"( ) (+) (o) (#) (@)   0,5  15deg  0,25");
					ec.Separator();
					ec.Text("");
					ec.Text("            (vue 3D — a brancher sur NKRenderer)");
					ec.Text("");
					ec.Separator();
					// LE PANNEAU DE DERNIERE OPERATION, quatrieme chemin d'acces. Il
					// n'apparait qu'apres une operation : ici on le montre en mode
					// edition pour valider qu'il tient dans le coin sans gener.
					if (mSt.editMode) {
						ec.Text("v Extruder la region");
						ec.Text("   Distance      0,25");
						ec.Text("   Decalage      0,00");
						ec.Text("   [x] Decalage pair");
					}
				}

			private:
				NkModelerState &mSt;
		};

		// ── PROPRIETES (droite, en haut) ────────────────────────────────────────
		class PropertiesPanel : public NkEditorPanel {
			public:
				explicit PropertiesPanel(NkModelerState &st) noexcept
					: NkEditorPanel("Proprietes", NkEditorDockSide::NK_RIGHT), mSt(st) {}

				void OnUI(NkEditorFrameContext &ec) override {
					ec.Text("Rechercher...");
					ec.Text("(General) (Objet) (Rendu) (Physique) [Tout]");
					ec.Separator();
					ec.Text("v Transformation");
					// Trois champs par ligne, un liseré de couleur d'AXE a gauche de
					// chacun. La couleur vient du theme (roles AxisX/Y/Z) et non d'une
					// constante : en theme clair ce sont d'autres valeurs.
					ec.Text("  Position    | 0,00 | 0,00 | 0,00      (reinit)");
					ec.Text("  Rotation    | 0,00 | 0,00 | 0,00");
					ec.Text("  Echelle     | 1,00 | 1,00 | 1,00      (verrou)");
				}

			private:
				NkModelerState &mSt;
		};

		// ── DETAILS (droite, en bas) — c'est ici que vit la PILE ────────────────
		class DetailsPanel : public NkEditorPanel {
			public:
				explicit DetailsPanel(NkModelerState &st) noexcept
					: NkEditorPanel("Details", NkEditorDockSide::NK_RIGHT), mSt(st) {}

				void OnUI(NkEditorFrameContext &ec) override {
					ec.Text("Details (Cube)");
					ec.Separator();
					ec.Text("v Maillage");
					ec.Text("   Sommets   8");
					ec.Text("   Faces     6");
					ec.Separator();
					// LA PILE DE MODIFICATEURS, et non un menu deroulant. L'ordre est
					// signifiant (un miroir apres une subdivision ne donne pas le meme
					// resultat qu'avant), d'ou les fleches de reordonnancement. Chaque
					// entree porte ses quatre actions : actif, monter/descendre,
					// appliquer, retirer.
					ec.Text("v Modificateurs            [+ Ajouter v]");
					ec.Text("   [x] Subdivision      ^ v  [appliquer] [x]");
					ec.Text("        Niveaux    2");
					ec.Text("        Rendu      3");
					ec.Text("   [x] Miroir           ^ v  [appliquer] [x]");
					ec.Text("        Axe        X");
					ec.Text("   [ ] Tableau          ^ v  [appliquer] [x]   (desactive)");
					ec.Separator();
					ec.Text("> Materiau");
				}

			private:
				NkModelerState &mSt;
		};

		// ── NAVIGATEUR DE PROJET (bas, toute la largeur) ────────────────────────
		class ProjectBrowserPanel : public NkEditorPanel {
			public:
				explicit ProjectBrowserPanel(NkModelerState &st) noexcept
					: NkEditorPanel("Navigateur de projet", NkEditorDockSide::NK_BOTTOM), mSt(st) {}

				void OnUI(NkEditorFrameContext &ec) override {
					ec.Text("[+ Ajouter] [Importer] [Tout enregistrer]    "
							"Tout > Contenu > Perso");
					ec.Separator();
					// Les dossiers structurent le PROJET, pas le disque : ils seront
					// enregistres DANS le fichier de projet (demande de Rihen).
					ec.Text("v MonProjet        | Rechercher...  "
							"(Maillage) (Animation) (Materiau) (Texture)");
					ec.Text("    Maillages      |");
					ec.Text("    Animations     |  [Cube]  [Tete]  [Bois]  [Marche]  [Roche]");
					ec.Text("    Materiaux      |");
					ec.Text("    Textures       |                             5 elements");
				}

			private:
				NkModelerState &mSt;
		};

		// ── PANNEAU T (gauche de la vue) ────────────────────────────────────────
		// Demande de Rihen. Ferme par defaut en mode objet -- un debutant doit voir
		// la scene, pas trois panneaux -- ouvert d'office en sculpt, ou le mode
		// serait inutilisable sans lui.
		class ToolPanel : public NkEditorPanel {
			public:
				explicit ToolPanel(NkModelerState &st) noexcept
					: NkEditorPanel("Outils", NkEditorDockSide::NK_LEFT), mSt(st) {
					SetOpen(false);
				}

				void OnUI(NkEditorFrameContext &ec) override {
					// La barre flottante de la vue porte l'outil ACTIF ; ce panneau
					// porte la LISTE et les REGLAGES. Les deux ne font pas doublon --
					// une barre horizontale ne peut pas tenir une liste de brosses.
					static const char *const kObj[] = {"Selection", "Curseur", "Deplacer",
													   "Tourner",   "Redimen.", "Transformer"};
					static const char *const kEdit[] = {"Extruder", "Biseauter", "Inserer",
													    "Decouper", "Boucle",	 "Lisser"};
					for (int32 i = 0; i < 6; ++i)
						ec.Text(kObj[i]);
					if (mSt.editMode) {
						ec.Separator();
						for (int32 i = 0; i < 6; ++i)
							ec.Text(kEdit[i]);
						ec.Separator();
						ec.Text("v Options d'extrusion");
						ec.Text("   Distance      0,25");
						ec.Text("   [x] Decalage pair");
					}
				}

			private:
				NkModelerState &mSt;
		};

	} // namespace nk3d
} // namespace nkentseu
