#pragma once
// -----------------------------------------------------------------------------
// @File    NogeeChrome.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// =============================================================================
// NogeeChrome.h — LA BARRE DE MENUS, LA BARRE D'OUTILS ET LA BARRE D'ETAT de
// Nogee, posees sur les trois points d'accrochage du shell.
//
// POURQUOI CE FICHIER N'EST PAS UN PEINTRE
//   NK3DModeler peint ses trois barres lui-meme (`PaintMenuBarI`,
//   `PaintToolbar`, `PaintStatus`) parce qu'il n'utilise PAS `NkEditorShell` :
//   il ouvre la fenetre et prend la draw list. Nogee, lui, passe par le shell,
//   et le shell offre exactement les trois prises qu'il faut :
//     `SetMenuBar`    (NkEditorShell.h:444) -- remplace entierement les menus par defaut
//     `SetToolbar`    (NkEditorShell.h:462) -- ET c'est ce qui fait exister la bande
//     `SetFooter`     (NkEditorShell.h:643) -- le texte des deux cotes de la barre d'etat
//   Recopier le peintre du modeleur aurait tire `NkModelerPainter`,
//   `NkModelerTheme`, `NkModelerIcons`, `NkHitRegistry` et `NkModelerState` —
//   cinq dependances propres a une autre application, pour trois barres.
//   Ici : on APPELLE.
//
// ⚠️ `SetToolbar` NE DECORE PAS, IL RESERVE.
//   `toolbarH = (mToolbarFn && !fullScreen) ? bandH : 0.f` (NkEditorShell.cpp:831).
//   Tant que ce hook est nul, la hauteur demandee par `SetHeaderLayout` n'occupe
//   AUCUN pixel. C'est mesure, pas suppose : la sonde `--panneaux-sonde` rendait
//   `outils=0.00` alors que `SetHeaderLayout(30, 34, 0)` etait deja appele.
//
// ⚠️ LES LIBELLES PORTENT LEURS ACCENTS.
//   La tolerance « francais sans accents » du CLAUDE.md parent ne couvre que
//   `echanges/` et les messages de commit ; l'interface d'un produit
//   francophone porte ses accents (meme regle que NKUIDesign, dont la cause de
//   l'absence avait ete mesuree : la source, jamais la police). Les libelles du
//   modeleur sont ecrits sans accents ; je ne touche pas a NK3DModeler, mais je
//   ne reproduis pas sa dette ici.
//
// ⚠️ UNE ENTREE SANS ACTION SE GRISE, ELLE NE DISPARAIT PAS.
//   Deux regles ecrites se rencontrent ici. `PRINCIPES_CONCEPTION.private.md` :
//   une entree d'interface ne nait que quand ses outils existent. NKUIDesign
//   (§5bis.1) : une entree qui ne peut rien produire se grise plutot que de
//   disparaitre — une barre dont le contenu varie apprend a l'utilisateur une
//   carte qui se deforme sous ses pieds. On garde donc la STRUCTURE du modeleur
//   (sept menus, les memes familles), et chaque entree dont l'outil n'existe pas
//   encore chez Nogee est posee GRISEE. Rien n'est simule : une entree grisee ne
//   promet rien, elle situe.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKEditorKit/NkEditorShell.h"
#include "NKGui/Widgets/NkGuiWidgets.h"
#include "Nogee/Editor/CommandHistory.h"
#include "Nogee/Editor/NkSelectionManager.h"
#include "Nogee/Editor/ProjectManager.h"
#include "Nogee/Shell/NkPanneauxSonde.h"
#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Scene/NkSceneGraph.h"
#include "Noge/ECS/Components/Core/NkCoreComponents.h"
#include <cstdio>

namespace nkentseu {
	namespace noge {

		using namespace nkentseu::editorkit;

		/// Ce que les trois barres ont besoin de connaitre. Un seul objet, pose
		/// une fois par le montage : trois `void*` distincts finiraient par ne
		/// plus designer le meme etat.
		struct NogeeChromeCtx {
				NkEditorShell *shell = nullptr;
				ProjectManager *projet = nullptr;
				CommandHistory *histo = nullptr;
				NkSelectionManager *selection = nullptr;
				ecs::NkWorld *monde = nullptr;
				ecs::NkSceneGraph *scene = nullptr;
		};

		namespace chrome {

			/// Declaree avant la barre de menus, qui l'appelle (cf. le bloc a la
			/// fin de `BarreDeMenus`). Definie plus bas, avec les deux autres.
			inline void MettreAJourBarreDEtat(NogeeChromeCtx &c) noexcept;

			/// Selection de TOUS les noeuds de scene. Ecrit ici parce que
			/// `NkSelectionManager` ne connait pas le monde : il gere une liste
			/// d'identifiants, il ne sait pas les enumerer. C'est la meme requete
			/// que le montage utilise deja pour la selection initiale.
			inline void ToutSelectionner(NogeeChromeCtx &c) noexcept {
				if (!c.monde || !c.selection)
					return;
				c.selection->Clear();
				c.monde->Query<const ecs::NkSceneNode>().ForEach(
					[&](ecs::NkEntityId id, const ecs::NkSceneNode &n) {
						if (n.name[0] != '\0')
							c.selection->SelectAdd(id);
					});
			}

			// ── LES FRACTIONS DU MODELEUR ────────────────────────────────────
			// NK3DModeler donne a ses zones une fraction de la FENETRE :
			// `fLeft = 0.16f`, `fRight = 0.29f`, `fBrowser = 0.22f`
			// (`NkLayout::Compute`, NkModelerUI.h). Le dock de NKGui, lui, ne
			// connait que des ratios de SPLIT : chaque panneau de bord enveloppe la
			// racine avec `ratio = 0.22` en dur (NkGuiWidgets.cpp:3578), et les
			// ratios se composent -- 0,22 d'un parent qui vaut deja 0,78 de la
			// fenetre ne fait pas 0,22 de la fenetre.
			//
			// On ne recopie donc PAS un ratio : on resout, a l'execution, celui qui
			// donne la fraction voulue compte tenu du rectangle REEL du split
			// parent. C'est la meme mecanique que `NkEditorShell::SetRegionMode`
			// (l.2204), qui ajuste deja `split.ratio` par le noeud d'un panneau
			// nomme : la porte existe, elle n'est simplement pas publique.
			//
			// ⚠️ CES TROIS NOMBRES SONT DES PARAMETRES DE PRODUIT, PAS DES ATTENDUS
			//    DE MESURE. Le banc `verdict_disposition.py` les relit dans
			//    `NkModelerUI.h` a chaque execution et compare : le jour ou Rodolf
			//    change une fraction du modeleur, c'est le banc qui le dira, pas un
			//    commentaire qui vieillit en silence.
			inline constexpr float32 kFracGauche = 0.16f;  ///< NkLayout::Compute, fLeft
			inline constexpr float32 kFracDroite = 0.29f;  ///< NkLayout::Compute, fRight
			inline constexpr float32 kFracBas = 0.22f;	   ///< NkLayout::Compute, fBrowser

			/// Pose le ratio du split PARENT de la feuille qui porte `titre`, pour
			/// que ce panneau occupe `ciblePx` dans la direction demandee.
			/// `largeur = true` : on regle une LARGEUR (split gauche|droite).
			/// Rend faux si la geometrie n'est pas encore connue -- et ne devine
			/// rien dans ce cas : un ratio calcule sur un rectangle nul serait un
			/// nombre parfaitement coherent avec lui-meme et faux.
			inline bool PoserFraction(NkEditorShell *shell, const char *titre, float32 ciblePx,
									  bool largeur) noexcept {
				if (!shell || ciblePx <= 0.f)
					return false;
				const int32 feuille = shell->PanelDockNode(titre);
				auto &noeuds = shell->Ui().dockNodes;
				if (feuille < 0 || feuille >= static_cast<int32>(noeuds.Size()))
					return false;
				const int32 parent = noeuds[feuille].parent;
				if (parent < 0 || parent >= static_cast<int32>(noeuds.Size()))
					return false;
				nkgui::NkGuiDockNode &split = noeuds[parent];
				if (split.kind != 1)
					return false;
				// `vertical` = split gauche|droite (NkGuiTypes.h:355). Un panneau du
				// bas a un parent HORIZONTAL : refuser ici evite de regler une
				// hauteur en croyant regler une largeur.
				if (split.vertical != largeur)
					return false;
				const float32 taille = largeur ? split.rect.w : split.rect.h;
				if (taille < 2.f)
					return false; // rect pas encore calcule : on repassera
				float32 f = ciblePx / taille;
				if (f < 0.05f)
					f = 0.05f;
				if (f > 0.95f)
					f = 0.95f;
				split.ratio = (split.child0 == feuille) ? f : (1.f - f);
				return true;
			}

			/// Applique les trois fractions. Appelee sur les PREMIERES images, pas
			/// une seule fois : changer le ratio d'un split ne met a jour le
			/// rectangle de ses enfants qu'a l'image suivante, et les trois splits
			/// sont imbriques. On va donc du plus EXTERNE au plus INTERNE (bas,
			/// puis droite, puis gauche -- l'inverse de l'ordre d'ancrage), et on
			/// repasse jusqu'a ce que la geometrie se soit propagee.
			///
			/// Ensuite on s'arrete : continuer empecherait Rodolf de deplacer un
			/// separateur a la souris, ce qui serait remplacer un defaut par un pire.
			inline void AjusterFractions(NkEditorShell *shell, int32 &passesRestantes) noexcept {
				if (!shell || passesRestantes <= 0)
					return;
				nkgui::NkGuiContext &ui = shell->Ui();
				const float32 W = static_cast<float32>(ui.viewW);
				const float32 H = static_cast<float32>(ui.viewH);
				if (W < 2.f || H < 2.f)
					return;
				const bool bas = PoserFraction(shell, "Content Browser", kFracBas * H, false);
				const bool droite = PoserFraction(shell, "Details", kFracDroite * W, true);
				const bool gauche = PoserFraction(shell, "World Outliner", kFracGauche * W, true);
				if (bas && droite && gauche)
					--passesRestantes;
			}

			// ── LA BARRE DE MENUS ────────────────────────────────────────────
			// Structure reprise de `NkMenus()` (NK3DModeler/Shell/NkModelerBrowser.h) :
			// Fichier, Edition, Fenetre, Outils, Selection, Objet, Aide — dans cet
			// ordre, qui est celui de la barre du modeleur.
			//
			// Le shell appelle cette fonction ENTRE `BeginMenuBar` et `EndMenuBar`
			// (NkEditorShell.cpp:2356) : on ne pose donc que les menus.
			inline void BarreDeMenus(NkEditorFrameContext &ec, void *user) noexcept {
				NogeeChromeCtx &c = *static_cast<NogeeChromeCtx *>(user);
				nkgui::NkGuiContext &ui = ec.Ui();
				using namespace nkentseu::nkgui;

				// ── DEUX COMPTEURS, ET ILS NE DISENT PAS LA MEME CHOSE ───────
				// Une entree de menu n'est DESSINEE que si son menu est deroule :
				// `BeginMenu` rend faux quand le menu est ferme, et le corps ne
				// s'execute pas. Compter les entrees dessinees rendrait donc 0 sur
				// une barre au repos — et personne ne peut cliquer dans une mesure
				// automatisee (aucune injection de souris, c'est une garde, pas une
				// preference).
				//   `entrees`          = DECLAREES : ce que la barre porte, compte
				//                        que le menu soit ouvert ou non.
				//   `entreesDessinees` = ce qui a reellement ete peint cette image.
				// Les deux sont imprimes. Un seul des deux aurait menti : l'un en
				// disant zero, l'autre en laissant croire que tout est a l'ecran.
				// Remis a zero A CHAQUE image : un compteur cumulatif dirait
				// soixante fois trop a la soixantieme.
				NkPanneauxCompteurs &cpt = NkPanneauxCpt();
				cpt.menus = 0;
				cpt.entrees = 0;
				cpt.entreesDessinees = 0;
				const float32 xDepart = ui.menuBarX;

				// Un TITRE de la barre. Compte toujours : le titre est peint meme
				// quand le menu est ferme, c'est meme tout ce qu'on voit au repos.
				auto menu = [&](const char *titre) -> bool {
					++cpt.menus;
					return BeginMenu(ui, titre);
				};

				// Une ENTREE. Declaree toujours, dessinee seulement si son menu est
				// ouvert. Le comptage vit ici et nulle part ailleurs : une entree
				// ajoutee plus tard ne peut pas « oublier » de se compter, il lui
				// manquerait juste son action.
				auto item = [&](bool ouvert, const char *label, const char *raccourci,
								bool actif) -> bool {
					++cpt.entrees;
					if (!ouvert)
						return false;
					++cpt.entreesDessinees;
					return MenuItem(ui, label, raccourci, actif);
				};

				// Un SOUS-MENU. Il compte pour une entree de son parent (c'est une
				// ligne de la liste), et son propre contenu ne se declare que s'il
				// est lui-meme ouvert. Le court-circuit protege NKGui : `BeginMenu`
				// ne doit pas etre appele hors du menu parent.
				auto sousMenu = [&](bool parentOuvert, const char *titre) -> bool {
					++cpt.entrees;
					if (!parentOuvert)
						return false;
					++cpt.entreesDessinees;
					return BeginMenu(ui, titre);
				};

				// ── Fichier ──────────────────────────────────────────────────
				{
					const bool m = menu("Fichier");
					item(m, "Nouveau", "", false);
					item(m, "Ouvrir…", "", false);
					if (sousMenu(m, "Ouvrir récent")) {
						item(true, "(aucun projet récent)", nullptr, false);
						EndMenu(ui);
					}
					if (m)
						Separator(ui);
					// `Save(nullptr)` ecrit dans le chemin deja connu. Grise tant
					// qu'aucun projet n'est ouvert : enregistrer un projet qui
					// n'existe pas creerait un fichier que personne n'a demande.
					const bool projetOuvert = c.projet && c.projet->IsOpen();
					if (item(m, "Enregistrer", "Ctrl+S", projetOuvert))
						c.projet->Save();
					if (item(m, "Enregistrer tout", "", projetOuvert))
						c.projet->Save();
					item(m, "Enregistrer sous…", "", false);
					if (m)
						Separator(ui);
					if (sousMenu(m, "Importer")) {
						item(true, "(glisser un fichier dans la vue)", nullptr, false);
						EndMenu(ui);
					}
					if (sousMenu(m, "Exporter")) {
						item(true, "(aucun exportateur branché)", nullptr, false);
						EndMenu(ui);
					}
					if (m)
						Separator(ui);
					if (item(m, "Quitter", "Alt+F4", true))
						c.shell->RequestClose();
					if (m)
						EndMenu(ui);
				}

				// ── Édition ──────────────────────────────────────────────────
				{
					const bool m = menu("Édition");
					const bool peutAnnuler = c.histo && c.histo->CanUndo();
					const bool peutRefaire = c.histo && c.histo->CanRedo();
					if (item(m, "Annuler", "Ctrl+Z", peutAnnuler))
						c.histo->Undo();
					if (item(m, "Rétablir", "Ctrl+Y", peutRefaire))
						c.histo->Redo();
					if (m)
						Separator(ui);
					item(m, "Dupliquer", "Ctrl+D", false);
					item(m, "Supprimer", "Suppr", false);
					if (m)
						Separator(ui);
					if (item(m, "Préférences…", "", true))
						c.shell->OpenPreferences(0);
					if (m)
						EndMenu(ui);
				}

				// ── Fenêtre ──────────────────────────────────────────────────
				{
					const bool m = menu("Fenêtre");
					// ⚠️ ON N'ECRIT PAS LA LISTE DES PANNEAUX. `DrawPanelsMenuItems`
					//    (NkEditorShell.h:451) la construit depuis les panneaux
					//    REELLEMENT enregistres, et ouvrir une entree ANCRE le
					//    panneau a son cote. Une liste recopiee ici mentirait le
					//    jour ou un panneau serait ajoute ou retire — c'est
					//    exactement la faute qui a fait disparaitre « Quitter » du
					//    menu Fichier du modeleur (son propre commentaire le dit).
					//
					//    Et c'est pourquoi ces entrees-la ne sont PAS comptees : le
					//    nombre appartient au shell, pas a ce fichier. Le mettre ici
					//    serait la meme recopie, deguisee en compteur. La sonde
					//    imprime a cote le nombre de panneaux enregistres.
					if (m)
						c.shell->DrawPanelsMenuItems();
					if (m)
						Separator(ui);
					if (item(m, "Réinitialiser la disposition", "", true))
						c.shell->ResetLayout();
					if (m)
						Separator(ui);
					item(m, "Plein écran", "F11", false);
					if (m)
						EndMenu(ui);
				}

				// ── Outils ───────────────────────────────────────────────────
				{
					const bool m = menu("Outils");
					if (item(m, "Rechercher une commande", "Ctrl+P", true))
						c.shell->OpenCommandPalette();
					if (m)
						Separator(ui);
					item(m, "Jouer la scène", "", false);
					item(m, "Arrêter", "", false);
					if (m)
						Separator(ui);
					if (sousMenu(m, "Extensions")) {
						item(true, "(aucune extension chargée)", nullptr, false);
						EndMenu(ui);
					}
					if (m)
						EndMenu(ui);
				}

				// ── Sélection ────────────────────────────────────────────────
				{
					const bool m = menu("Sélection");
					const bool aSelection = c.selection && c.selection->HasSelection();
					if (item(m, "Tout sélectionner", "Ctrl+A", c.monde != nullptr))
						ToutSelectionner(c);
					if (item(m, "Tout désélectionner", "", aSelection))
						c.selection->Clear();
					item(m, "Inverser", "", false);
					if (m)
						Separator(ui);
					if (sousMenu(m, "Par type")) {
						item(true, "(aucun filtre de type)", nullptr, false);
						EndMenu(ui);
					}
					if (m)
						EndMenu(ui);
				}

				// ── Objet ────────────────────────────────────────────────────
				{
					const bool m = menu("Objet");
					item(m, "Déplacer", "W", false);
					item(m, "Tourner", "E", false);
					item(m, "Redimensionner", "R", false);
					if (m)
						Separator(ui);
					item(m, "Ajouter un composant", "", false);
					item(m, "Appliquer tout", "", false);
					if (m)
						EndMenu(ui);
				}

				// ── Aide ─────────────────────────────────────────────────────
				{
					const bool m = menu("Aide");
					item(m, "Documentation", "", false);
					item(m, "Raccourcis clavier", "", false);
					if (m)
						Separator(ui);
					item(m, "À propos", "", false);
					if (m)
						EndMenu(ui);
				}

				// LARGEUR REELLEMENT CONSOMMEE. `ctx.menuBarX` avance a chaque titre
				// de `BeginMenu` de `MeasureWidth(label) + 20` (NKGui,
				// NkGuiWidgets.cpp:5129-5130). C'est donc une mesure prise SUR le
				// dessin, pas une somme d'intentions : une barre qui n'aurait rien
				// peint rendrait zero. C'est elle qui porte le NEGATIF de ce lot —
				// une barre sans aucun menu ne consomme aucune largeur, et la bande
				// ne grandit pas pour autant.
				cpt.largeurMenus = ui.menuBarX - xDepart;

				// ⚠️ LA BARRE D'ETAT SE MET A JOUR ICI, ET CE N'EST PAS UN HASARD
				//    D'EMPLACEMENT. `SetFooter` POSE un texte ; c'est le shell qui
				//    dessine la bande, plus tard dans la meme image
				//    (`DrawStatusBar`, NkEditorShell.cpp:1024, apres la barre de
				//    titre l.882). Le shell n'offre aucun hook « debut d'image » :
				//    l'overlay est pris par les sondes, et la barre d'outils peut
				//    etre debranchee. La barre de menus, elle, est appelee a chaque
				//    image sans condition. Poser le pied d'ici garantit qu'il ne se
				//    fige jamais sur l'etat d'une image passee.
				MettreAJourBarreDEtat(c);
			}

			// ── LA BARRE D'OUTILS ────────────────────────────────────────────
			// Trois elements, comme le modeleur : Enregistrer, Ajouter, et
			// Reglages cale a droite. Ce sont les siens, pas une invention : sa
			// `PaintToolbar` n'en dessine pas d'autres (les deroulants « Mode » et
			// « Modificateur » en ont ete retires, ses commentaires le disent).
			//
			// Poser ce hook a un effet que la decoration n'a pas : c'est LUI qui
			// fait exister la bande de 34 px demandee par `SetHeaderLayout`.
			inline void BarreDOutils(NkEditorFrameContext &ec, void *user) noexcept {
				NogeeChromeCtx &c = *static_cast<NogeeChromeCtx *>(user);
				nkgui::NkGuiContext &ui = ec.Ui();
				using namespace nkentseu::nkgui;

				NkPanneauxCompteurs &cpt = NkPanneauxCpt();
				cpt.outils = 0;
				// Le shell vient de poser `cursor` ET `maxX` au meme point
				// (NkEditorShell.cpp:1904-1908) : le zero du temoin est donc connu,
				// pas suppose.
				const float32 xDepart = ui.layout.cursor.x;

				const bool projetOuvert = c.projet && c.projet->IsOpen();
				const bool modifie = c.projet && c.projet->IsModified();

				// « Enregistrer » est AUSSI un signal chez le modeleur : tant qu'il
				// reste du travail non enregistre, il s'affiche en accent. Ici, le
				// shell ne donne pas de bouton accentue ; l'etat passe donc par le
				// libelle, qui DIT ce qu'il y a a enregistrer au lieu de le laisser
				// deviner. Rien a enregistrer = bouton inerte, comme chez lui.
				++cpt.outils;
				ui.BeginDisabled(!(projetOuvert && modifie));
				if (Button(ui, modifie ? "Enregistrer *" : "Enregistrer") && projetOuvert)
					c.projet->Save();
				ui.EndDisabled();
				ui.SameLine();

				++cpt.outils;
				ui.BeginDisabled(c.scene == nullptr);
				if (Button(ui, "Ajouter") && c.scene) {
					// Une entite reelle, nommee, posee a la racine : l'Outliner la
					// montre a la frame suivante. C'est le seul outil de cette
					// barre qui produit quelque chose aujourd'hui, et il produit
					// vraiment — un bouton qui ne fait rien est pire qu'un bouton
					// grise, parce qu'il ne le dit pas.
					static int32 nAjouts = 0;
					char nom[48];
					std::snprintf(nom, sizeof(nom), "Entite_%d", ++nAjouts);
					const ecs::NkEntityId id = c.scene->SpawnNode(nom);
					if (c.monde) {
						c.monde->Add<ecs::NkName>(id, ecs::NkName(nom));
						c.monde->Add<ecs::NkTransform>(id);
					}
					if (c.selection)
						c.selection->Select(id);
				}
				ui.EndDisabled();
				ui.SameLine();

				++cpt.outils;
				if (Button(ui, "Réglages"))
					c.shell->OpenPreferences(0);

				// ⚠️ `maxX`, PAS `cursor.x`. Premiere version de ce temoin : la
				//    difference des curseurs. Elle rendait 0,00 avec trois boutons
				//    bel et bien dessines — et c'est le temoin qui avait tort, pas
				//    la barre. Un widget qui termine sa ligne REMET `cursor.x` a
				//    `lineStartX` (NkGuiContext.cpp:275) ; le dernier bouton n'etant
				//    pas suivi d'un `SameLine`, le curseur etait revenu a son point
				//    de depart. `maxX` retient l'extremite droite reellement
				//    atteinte (l.260-261) : c'est la seule des deux qui temoigne.
				cpt.largeurOutils = ui.layout.maxX - xDepart;
			}

			// ── LA BARRE D'ETAT ──────────────────────────────────────────────
			// Le modeleur y met, a droite, un etat de la scene : « Objets 6 —
			// selectionne : Cube — 60 ips — dorsal : ... ». Chez lui ces nombres
			// sont ECRITS EN DUR (son propre commentaire le dit : « Objets 6 est
			// ECRIT EN DUR dans PaintStatus »). Ici ils sont COMPTES : le nombre
			// d'entites vient du monde, la selection du gestionnaire de selection.
			// Reproduire la chaine sans reproduire le mensonge.
			//
			// `SetFooter` pose un texte, il ne dessine pas : la bande est celle du
			// shell. Appele une fois par frame depuis le montage.
			inline void MettreAJourBarreDEtat(NogeeChromeCtx &c) noexcept {
				if (!c.shell)
					return;
				int32 nEntites = 0;
				if (c.monde)
					c.monde->Query<const ecs::NkSceneNode>().ForEach(
						[&](ecs::NkEntityId, const ecs::NkSceneNode &n) {
							if (n.name[0] != '\0')
								++nEntites;
						});
				const nk_uint32 nSel = c.selection ? c.selection->Count() : 0u;

				char gauche[96];
				const bool modifie = c.projet && c.projet->IsModified();
				std::snprintf(gauche, sizeof(gauche), "%s%s",
							  (c.projet && c.projet->IsOpen()) ? "Projet ouvert" : "Aucun projet",
							  modifie ? " — modifié" : "");

				char droite[128];
				std::snprintf(droite, sizeof(droite), "Entités %d — sélection : %u",
							  nEntites, static_cast<unsigned>(nSel));
				c.shell->SetFooter(gauche, droite);
			}

		} // namespace chrome
	} // namespace noge
} // namespace nkentseu
