#pragma once
// =============================================================================
// NkModelerScreens.h — les zones de l'ecran A, peintes une par une.
//
// Une fonction par zone, dans l'ordre ou elles apparaissent a l'ecran. Chacune
// recoit son rectangle et ne peint QUE dedans : c'est ce qui permettra de les
// deplacer, replier ou remplacer une par une sans toucher aux autres.
//
// Les valeurs affichees sont des EXEMPLES cales sur la maquette (Cube, Sphere,
// 8 sommets, 6 faces...). Elles seront remplacees par la scene reelle zone par
// zone -- c'est la marche a suivre convenue avec Rihen : construire, puis mettre
// a jour en developpant.
// =============================================================================

#include "NK3DModeler/Viewport/NkViewport3D.h"
#include "NK3DModeler/Shell/NkModelerUI.h"
#include "NK3DModeler/Shell/NkModelerInput.h"
#include "NK3DModeler/Shell/NkModelerWidgets.h"
#include "NKEditorKit/NkShortcutTable.h"

#include <cstdio>

namespace nkentseu {
	namespace nk3d {

		// Hauteur d'une ligne de liste ou de propriete. Reprise de la maquette :
		// 22 px. En dessous le texte touche les bords, au-dessus la densite chute et
		// il faut faire defiler pour rien.
		// Non const : elles sont multipliees par l'echelle au demarrage (cf. S()).
		inline float32 kRowH = 22.f;
		inline float32 kLabelW = 78.f;  ///< colonne de libelles des proprietes
		inline float32 kPad = 8.f;
		// MARGE INTERNE des panneaux de droite. Le contenu ne doit pas toucher les
		// bords : colle au trait de separation, une ligne de propriete se lit comme
		// la continuation du panneau voisin. Applique en RETRECISSANT le rectangle de
		// travail, une fois, plutot qu'en decalant chaque appel -- sinon il suffit
		// d'en oublier un pour que l'alignement casse.
		inline float32 kInset = 10.f;

		// Retrecit un rectangle de la marge interne, sans toucher au haut ni au bas :
		// l'en-tete d'onglet et les fonds pleins doivent, eux, aller bord a bord.
		// Applique l'echelle d'interface a toutes les constantes de disposition.
		// Appelee UNE FOIS au demarrage, apres avoir lu le DPI de la fenetre.
		inline void ApplyUiScale(float32 scale) {
			gUiScale = scale;
			kRowH = Px(22.f * scale);
			kLabelW = Px(78.f * scale);
			kPad = Px(8.f * scale);
			kInset = Px(10.f * scale);
		}

		inline NkRect Inset(const NkRect &r) {
			return {r.x + kInset, r.y, r.w - kInset * 2.f, r.h};
		}

		// ── CONTENU DES LISTES DE LA VUE ────────────────────────────────────────
		// En TABLES, comme les menus : ajouter une entree ne demande pas de toucher
		// au rendu, et la meme table servira la palette de recherche.
		inline const char *const *NkProjectionItems(int32 &n) {
			// Les six vues orthographiques sont les MEMES que celles du gizmo de
			// navigation : un seul etat, deux facons d'y arriver.
			// ORTHOGONALE est une PROJECTION a part entiere, distincte des six vues
			// d'axe : on peut regarder de biais en orthographique. La confondre avec
			// « Dessus » -- l'erreur de ma premiere liste -- interdisait justement ce
			// cas, qui est celui du blocking et du travail de proportions.
			static const char *const k[] = {"Perspective", "Orthogonale", "Dessus", "Dessous",
											"Avant",	   "Arriere",	  "Gauche", "Droite"};
			n = 8;
			return k;
		}
		inline const NkIcon *NkProjectionIcons() {
			// La CAMERA pour la perspective (on voit depuis un point), la GRILLE pour
			// les vues orthographiques (aucun point de fuite). Deux dessins distincts,
			// sinon rien ne dit laquelle est active.
			// UNE ICONE PAR ENTREE. Six vues d'axe portant le meme dessin
			// obligeraient a lire le libelle pour les distinguer -- ce qui annule
			// l'interet d'une liste a icones, et rend le bouton de la barre muet
			// des qu'on sort de la perspective.
			//   perspective -> camera        orthogonale -> grille
			//   dessus / dessous -> fleches verticales
			//   avant / arriere  -> fleches horizontales
			//   gauche / droite  -> fleches simples
			static const NkIcon k[] = {NkIcon::Persp,	  NkIcon::OrthoView, NkIcon::ArrowUp,
									   NkIcon::ArrowDown, NkIcon::ViewFront,  NkIcon::ViewBack,
									   NkIcon::ArrowLeft, NkIcon::ArrowRight};
			return k;
		}
		// ── MODES D'OMBRAGE ─────────────────────────────────────────────────────
		// QUATRE modes disponibles PARTOUT -- solide, materiau, rendu, fil de fer --
		// et on entre en mode EDITION depuis n'importe lequel : le mode d'edition et
		// le mode d'ombrage sont deux axes INDEPENDANTS. C'est ce que Rihen a precise,
		// et c'est ce que fait Blender : on peut editer un maillage en vue rendue.
		//
		// LE MATCAP est le CINQUIEME, et lui n'existe QU'EN EDITION. Sa raison d'etre
		// est justement de montrer la FORME sans texture ni eclairage de scene :
		// c'est ce qui permet de lire les bosses et les creux pendant qu'on modele.
		// L'offrir en mode objet n'aurait pas de sens -- on y regarde le resultat,
		// pas la surface.
		// QUATRE modes d ombrage, disponibles PARTOUT, et on entre en edition depuis
		// n importe lequel : mode d edition et mode d ombrage sont deux axes
		// INDEPENDANTS. Le matcap n en fait plus partie -- c est un reglage du mode
		// solide, cf. NkSolidLightItems.
		inline const char *const *NkShadingItems(int32 &n) {
			static const char *const k[] = {"Solide", "Materiau", "Rendu", "Fil de fer"};
			n = 4;
			return k;
		}
		inline const NkIcon *NkShadingIcons() {
			// QUATRE DESSINS DISTINCTS. Solide et Materiau partageaient le meme disque
			// plein : impossible de savoir lequel etait actif en regardant le bouton,
			// ce qui vide de son sens une barre a icones seules.
			static const NkIcon k[] = {NkIcon::Circle,	  // solide : sphere nue
									   NkIcon::Material,  // materiau : sphere coloree
									   NkIcon::Light,	  // rendu : eclairage de scene
									   NkIcon::Wireframe, // fil de fer : quadrillage
									   NkIcon::Matcap};	  // matcap : degrade capture
			return k;
		}

		// ── FORMES DE SELECTION ─────────────────────────────────────────────────
		// C'est le sous-menu qu'annonce le petit point du bouton de selection.
		// Rectangle par defaut : c'est le geste le plus simple et le plus previsible.
		// Le cercle sert a « peindre » une selection en balayant, le lasso a cerner
		// une forme irreguliere -- trois besoins reels que le rectangle seul ne
		// couvre pas.
		inline const char *const *NkSelShapeItems(int32 &n) {
			static const char *const k[] = {"Rectangle", "Cercle", "Lasso"};
			n = 3;
			return k;
		}
		inline const NkIcon *NkSelShapeIcons() {
			static const NkIcon k[] = {NkIcon::SelRect, NkIcon::SelCircle, NkIcon::SelLasso};
			return k;
		}

		// Les matcaps disponibles. Ils ne s'affichent que si l'ombrage Matcap est
		// choisi : proposer un choix de matcap alors qu'aucun n'est utilise ferait
		// croire a un reglage sans effet.
		inline const char *const *NkMatcapItems(int32 &n) {
			static const char *const k[] = {"Argile", "Metal", "Perle", "Rouge mat", "Bleu doux",
											"Vert atelier", "Peau", "Ceramique"};
			n = 8;
			return k;
		}

		// ── MENU DE VUE (l icone a gauche de Perspective) ───────────────────────
		// Il porte ce qui ne merite pas un bouton permanent : disposition des vues,
		// plein ecran, cameras enregistrees, reinitialisation.
		inline const char *const *NkViewMenuItems(int32 &n) {
			static const char *const k[] = {"Vue unique",	 "Deux vues",		"Quatre vues",
											"Plein ecran",	 "Camera enregistree", "Reinitialiser la vue"};
			n = 6;
			return k;
		}
		inline const NkIcon *NkViewMenuIcons() {
			static const NkIcon k[] = {NkIcon::Square,  NkIcon::Drawer, NkIcon::SnapGrid,
									   NkIcon::WinMax,  NkIcon::Camera, NkIcon::Refresh};
			return k;
		}
		// ── AFFICHAGE : DES CASES, PAS UN CHOIX UNIQUE ──────────────────────────
		// Rihen a raison, et UI_SPEC 9bis le disait deja : on veut couramment la
		// grille ET le repere d axes SANS les normales. Mon premier jet proposait
		// trois presets exclusifs (« tout / grille seule / rien »), ce qui obligeait
		// a choisir la combinaison la moins mauvaise au lieu de composer la sienne.
		//
		// Chaque entree est un bit du masque `st.overlayMask`.
		inline const char *const *NkOverlayItems(int32 &n) {
			static const char *const k[] = {
				"Grille",		   // le sol quadrille
				"Repere d axes",   // le trepied rouge / vert / bleu
				"Contours",		   // silhouette des objets selectionnes
				"Gizmos",		   // poignees de transformation
				"Normales",		   // direction des faces, en mode edition
				"Statistiques",	   // sommets / aretes / faces en surimpression
				"Filaire",		   // cage par-dessus la surface
				"Origines",		   // point d origine de chaque objet
			};
			n = 8;
			return k;
		}
		inline const NkIcon *NkOverlayIcons() {
			static const NkIcon k[] = {NkIcon::SnapGrid, NkIcon::Gizmo,	  NkIcon::Square,
									   NkIcon::Move,	NkIcon::Ruler,	  NkIcon::Journal,
									   NkIcon::Wireframe, NkIcon::Dot};
			return k;
		}

		// ── MATCAP : UN REGLAGE DU MODE SOLIDE ──────────────────────────────────
		// Rihen l a corrige a juste titre. J en avais fait un cinquieme mode
		// d ombrage ; c est faux. Le matcap N EST PAS un mode : c est la maniere
		// dont le mode SOLIDE eclaire la surface. Blender fait exactement cela --
		// « Solid » ouvre un panneau ou l on choisit entre eclairage studio, matcap
		// et couleur plate.
		// Consequence pratique : le selecteur de matcap n apparait que si l ombrage
		// est SOLIDE, et il reste disponible en mode objet comme en edition.
		inline const char *const *NkSolidLightItems(int32 &n) {
			static const char *const k[] = {"Studio", "Matcap", "Plat"};
			n = 3;
			return k;
		}
		inline const NkIcon *NkSolidLightIcons() {
			static const NkIcon k[] = {NkIcon::Light, NkIcon::Matcap, NkIcon::Circle};
			return k;
		}
		inline const char *const *NkOrientItems(int32 &n) {
			static const char *const k[] = {"Monde", "Local", "Normale", "Vue"};
			n = 4;
			return k;
		}
		inline const NkIcon *NkOrientIcons() {
			static const NkIcon k[] = {NkIcon::Globe, NkIcon::Mesh, NkIcon::Ruler, NkIcon::Camera};
			return k;
		}
		inline const char *const *NkCamSpeedItems(int32 &n) {
			// Vitesse de deplacement de la camera, comme le « 4 » d'Unreal. Sans elle,
			// une scene de 200 metres se parcourt au pas et une piece se traverse d'un
			// coup -- c'est le meme reglage qui rend les deux supportables.
			static const char *const k[] = {"Vitesse 1", "Vitesse 2", "Vitesse 4", "Vitesse 8"};
			n = 4;
			return k;
		}
		inline const char *const *NkSelectModeItems(int32 &n) {
			static const char *const k[] = {"Sommet", "Arete", "Face"};
			n = 3;
			return k;
		}
		inline const NkIcon *NkSelectModeIcons() {
			static const NkIcon k[] = {NkIcon::Dot, NkIcon::Ruler, NkIcon::Square};
			return k;
		}
		// ── AJOUTER, PAR CATEGORIES -- comme Blender ────────────────────────────
		// « Ajouter » n'est pas une liste de cubes : c'est un menu de CREATION, et
		// chaque categorie repond a une question differente -- une geometrie a
		// modeler, une source de lumiere, un point de vue, un repere d'assemblage.
		// Chaque entree porte le couple (type, primitive) attendu par
		// Viewport3DAddObject : le menu ne connait AUCUNE logique de creation.
		struct NkAddEntry {
				const char *label;
				NkIcon icon;
				int32 type;
				int32 prim;
		};
		struct NkAddCategory {
				const char *name;
				NkIcon icon;
				const NkAddEntry *items;
				int32 count;
		};
		inline const NkAddCategory *NkAddCategories(int32 &n) {
			static const NkAddEntry kMesh[] = {
				{"Cube", NkIcon::Mesh, nk3d::kVpObjMesh, nk3d::kVpPrimCube},
				{"Plan", NkIcon::Square, nk3d::kVpObjMesh, nk3d::kVpPrimPlane},
				{"Sphere", NkIcon::Circle, nk3d::kVpObjMesh, nk3d::kVpPrimSphere},
				{"Cylindre", NkIcon::Mesh, nk3d::kVpObjMesh, nk3d::kVpPrimCylinder},
				{"Cone", NkIcon::Mesh, nk3d::kVpObjMesh, nk3d::kVpPrimCone},
				{"Tore", NkIcon::Circle, nk3d::kVpObjMesh, nk3d::kVpPrimTorus},
			};
			static const NkAddEntry kLight[] = {
				{"Point", NkIcon::Light, nk3d::kVpObjLightPoint, 0},
				{"Soleil", NkIcon::Light, nk3d::kVpObjLightSun, 0},
				{"Spot", NkIcon::Light, nk3d::kVpObjLightSpot, 0},
			};
			static const NkAddEntry kCam[] = {
				{"Camera", NkIcon::Camera, nk3d::kVpObjCamera, 0},
			};
			static const NkAddEntry kEmpty[] = {
				{"Repere vide", NkIcon::Gizmo, nk3d::kVpObjEmpty, 0},
			};
			// IMAGE DE REFERENCE : un plan texture qu'on aligne sur une vue pour
			// modeler par-dessus. C'est l'outil de base du blocking -- on part
			// presque toujours d'un croquis de face et d'un croquis de profil. Elle
			// n'est PAS un maillage ordinaire : elle ne doit ni recevoir d'ombre ni
			// entrer dans le rendu, mais elle a une transformation, donc elle vit
			// dans la meme table d'objets.
			static const NkAddEntry kRef[] = {
				{"Image de reference", NkIcon::Journal, nk3d::kVpObjReference, 0},
			};
			static const NkAddCategory kCats[] = {
				{"Maillage", NkIcon::Mesh, kMesh, 6},
				{"Lumiere", NkIcon::Light, kLight, 3},
				{"Camera", NkIcon::Camera, kCam, 1},
				{"Reference", NkIcon::Journal, kRef, 1},
				{"Vide", NkIcon::Gizmo, kEmpty, 1},
			};
			n = 5;
			return kCats;
		}
		// ── MODIFICATEURS, CLASSES PAR CATEGORIE ────────────────────────────────
		// Seize modificateurs dans une liste plate obligent a la parcourir en entier
		// pour trouver le bon, et rien n'indique lesquels font des choses comparables.
		// Les trois categories repondent chacune a une question differente :
		//   GENERER  — ajoute de la geometrie qui n'existait pas ;
		//   DEFORMER — garde la meme geometrie et la deplace ;
		//   NETTOYER — en retire ou la reorganise.
		// C'est le classement de Blender, et il tient parce qu'il repose sur ce que le
		// modificateur FAIT au maillage, pas sur son nom.
		//
		// La liste reprend NkModifierType (NKRenderer). Elle sera GENEREE depuis
		// l'enumeration au branchement : la recopier a la main garantit qu'elle
		// divergera le jour ou un modificateur sera ajoute.
		// Chaque entree porte le TYPE attendu par le moteur (cf. NkModifierType).
		// L'interface classe par usage -- generer / deformer / nettoyer -- et cet
		// ordre n'a aucune raison de coincider avec celui de l'enumeration : le
		// type explicite evite tout calcul d'indice, donc toute divergence le jour
		// ou une categorie gagne une entree.
		struct NkModEntry {
				const char *label;
				NkIcon icon;
				int32 type; ///< NkModifierType du moteur
		};
		struct NkModCategory {
				const char *name;
				NkIcon icon;
				const NkModEntry *items;
				int32 count;
		};

		inline const NkModCategory *NkModifierCategories(int32 &n) {
			// LES DIX-SEPT MODIFICATEURS DU MOTEUR, aucun de plus. La liste
			// precedente annoncait « Booleen », « Enveloppe », « Remailler » et
			// « Courbe » qui n'existent pas : un menu qui propose ce qui n'existe
			// pas est pire qu'un menu court.
			static const NkModEntry kGen[] = {
				{"Miroir", NkIcon::Ruler, 0},		{"Reseau", NkIcon::SnapGrid, 1},
				{"Subdivision", NkIcon::Layers, 2}, {"Solidifier", NkIcon::Mesh, 3},
				{"Biseau", NkIcon::Scale, 6},		{"Vissage", NkIcon::Rotate, 7},
				{"Construction", NkIcon::Add, 10},	{"Masque", NkIcon::EyeClosed, 11},
			};
			static const NkModEntry kDef[] = {
				{"Projeter", NkIcon::Circle, 12},	 {"Deformation simple", NkIcon::Move, 13},
				{"Lisser", NkIcon::Circle, 14},		 {"Onde", NkIcon::Ruler, 15},
			};
			static const NkModEntry kClean[] = {
				{"Trianguler", NkIcon::Mesh, 4},	  {"Souder", NkIcon::Refresh, 5},
				{"Separer les aretes", NkIcon::Ruler, 8}, {"Decimer", NkIcon::Trash, 9},
				{"Ombrage par angle", NkIcon::Light, 16},
			};
			static const NkModCategory kCats[] = {
				{"Generer", NkIcon::Add, kGen, 8},
				{"Deformer", NkIcon::Move, kDef, 4},
				{"Nettoyer", NkIcon::Trash, kClean, 5},
			};
			n = 3;
			return kCats;
		}

		// Libelle du modificateur choisi, tous categories confondues.
		inline const NkModEntry *NkModifierAt(int32 flat) {
			int32 nc = 0;
			const NkModCategory *cats = NkModifierCategories(nc);
			int32 seen = 0;
			for (int32 c = 0; c < nc; ++c) {
				if (flat < seen + cats[c].count)
					return &cats[c].items[flat - seen];
				seen += cats[c].count;
			}
			return &cats[0].items[0];
		}


		// Fond de survol. UN SEUL endroit, pour que tous les elements survolables
		// reagissent pareil : un survol qui change d'aspect d'un bouton a l'autre se
		// lit comme un defaut d'affichage, pas comme une intention.
		inline void HoverFill(NkModelerPainter &p, const NkRect &r, bool on, float32 rounding = 3.f) {
			if (on)
				p.Fill(r, NkRole::PanelBg, rounding);
		}

		// ── BARRE DE MENUS ──────────────────────────────────────────────────────
		inline void PaintMenuBarI(NkModelerPainter &p, const NkRect &r, const char *projectName,
								  NkModelerState &st, NkHitRegistry &hit) {
			p.Fill(r, NkRole::PanelHeader);
			p.HLine(r.x, r.y + r.h - 1.f, r.w);
			// Poignee de deplacement : toute la barre, declaree AVANT les menus et
			// les boutons pour qu'ils la recouvrent.
			hit.Add("win.drag", r);

			const float32 logo = S(22.f);
			const float32 ly = r.y + (r.h - logo) * 0.5f;
			p.Fill({S(10.f), ly, logo, logo}, NkRole::AccentUi, 4.f);
			const float32 nkW = p.TextW("NK");
			p.TextV(S(10.f) + (logo - nkW) * 0.5f, ly, logo, "NK", NkRole::TextOnAccent);

			float32 x = S(10.f) + logo + S(16.f);
			static const char *const kMenus[] = {"Fichier", "Edition", "Fenetre", "Outils",
												 "Selection", "Objet", "Aide"};
			static const char *const kMenuKeys[] = {"menu.0", "menu.1", "menu.2", "menu.3",
													"menu.4", "menu.5", "menu.6"};
			for (int32 i = 0; i < 7; ++i) {
				const float32 w = p.TextW(kMenus[i]) + S(18.f);
				const NkRect mr{x - S(9.f), r.y + S(6.f), w, r.h - S(12.f)};
				const bool over = hit.Add(kMenuKeys[i], mr);
				// Un menu OUVERT reste marque meme si la souris est partie : sinon on ne
				// saurait plus quel menu a produit la liste affichee.
				if (st.openMenu == i)
					p.Fill(mr, NkRole::AccentUi, 3.f);
				else
					HoverFill(p, mr, over);
				p.TextV(x, r.y, r.h, kMenus[i], st.openMenu == i ? NkRole::TextOnAccent : NkRole::Text);
				if (hit.Clicked(kMenuKeys[i]))
					st.openMenu = (st.openMenu == i) ? -1 : i; // deuxieme clic = referme
				x += w;
			}

			const float32 nw = p.TextW(projectName);
			p.TextV(r.w * 0.5f - nw * 0.5f, r.y, r.h, projectName, NkRole::TextMuted);

			const float32 bw = S(30.f), bh = S(22.f);
			const float32 by = r.y + (r.h - bh) * 0.5f;
			// L'ICONE DU BOUTON CENTRAL SUIT L'ETAT DE LA FENETRE : carre quand elle
			// peut etre agrandie, deux carres superposes quand elle peut etre
			// restauree. Garder le meme dessin dans les deux cas obligerait a se
			// souvenir de ce qu'on a fait pour savoir ce que le bouton va faire.
			const NkIcon kWin[3] = {NkIcon::WinMin, st.maximized ? NkIcon::WinRestore : NkIcon::WinMax,
									NkIcon::WinClose};
			static const char *const kWinKeys[3] = {"win.min", "win.max", "win.close"};
			for (int32 i = 0; i < 3; ++i) {
				const float32 bx = r.w - kPad - (float32)(3 - i) * (bw + S(6.f));
				const NkRect br{bx, by, bw, bh};
				const bool over = hit.Add(kWinKeys[i], br);
				const bool close = (i == 2);
				if (close)
					p.Fill(br, over ? NkColor{240, 100, 85, 255} : NkColor{231, 76, 60, 255}, 3.f);
				else
					p.Outline(br, NkRole::Border, over ? NkRole::PanelBg : NkRole::PanelHeader, 3.f);
				p.IconV(bx + (bw - S(13.f)) * 0.5f, by, bh, kWin[i],
						close ? NkRole::TextOnAccent : NkRole::Text, 13.f);
			}
			if (hit.Clicked("win.min"))
				st.wantMinimize = true;
			if (hit.Clicked("win.max"))
				st.wantMaxRestore = true;
			// La FERMETURE passe par la confirmation si le document est modifie.
			// Fermer directement ferait perdre le travail sur un clic mal place, et
			// c'est le genre d'accident qu'on ne pardonne pas a un logiciel.
			if (hit.Clicked("win.close")) {
				if (st.dirty)
					st.askClose = true;
				else
					st.running = false;
			}

			// ── DEPLACEMENT DE LA FENETRE ───────────────────────────────────────
			// La barre de titre est la poignee, mais seulement la ou elle est VIDE :
			// declarer la zone en PREMIER laisse les menus et les boutons, declares
			// ensuite, la recouvrir. C'est la regle « la derniere zone gagne » qui
			// fait le tri, sans qu'on ait a lister les exceptions.
			//
			// Le drapeau est pose ici et consomme par la boucle : BeginDragMove BLOQUE
			// (boucle modale de l'OS) et le rappeler pendant la peinture reentrerait
			// dans la frame.
			// On retient OU l'on a saisi la barre, en fraction de largeur : c'est ce
			// qui permet de replacer la fenetre sous le curseur si le glissement part
			// d'un etat maximise (cf. main.cpp).
			if (hit.Clicked("win.drag")) {
				st.wantDragMove = true;
				const float32 w = r.w > 1.f ? r.w : 1.f;
				float32 f = (hit.Mouse().x - r.x) / w;
				st.dragFracX = f < 0.f ? 0.f : (f > 1.f ? 1.f : f);
			}
		}

		// ── ONGLETS DE DOCUMENT ─────────────────────────────────────────────────
		// ── ONGLETS DE SCENE ────────────────────────────────────────────────────
		// UNE SEULE scene a l'ouverture. Demarrer sur deux onglets vides ferait croire
		// que l'un d'eux contient quelque chose, et obligerait a en fermer un avant
		// meme d'avoir commence. Le nom se modifie au DOUBLE-clic, le + en ajoute une.
		inline void PaintTabsI(NkModelerPainter &p, const NkRect &r, NkModelerState &st,
							   NkHitRegistry &hit, NkWidgetState &ws, const nkgui::NkGuiInput &in) {
			p.Fill(r, NkRole::PanelBg);
			p.HLine(r.x, r.y + r.h - 1.f, r.w);
			float32 x = S(10.f);
			const float32 h = r.h - 2.f;
			char key[32];
			for (int32 i = 0; i < st.sceneCount; ++i) {
				const float32 tw = p.TextW(st.sceneNames[i]) + S(44.f);
				const NkRect tr{x, r.y + 2.f, tw, h};
				snprintf(key, sizeof(key), "tab.%d", i);
				const bool over = hit.Add(key, tr);
				const bool on = (i == st.activeTab);
				p.Fill(tr, on ? NkRole::PanelHeader : (over ? NkRole::PanelBg : NkRole::InputBg), 3.f);
				snprintf(key, sizeof(key), "tab.name.%d", i);
				EditableText(p, hit, ws, in, key, {x + S(10.f), r.y, tw - S(32.f), r.h}, st.sceneNames[i],
							 on ? NkRole::Text : NkRole::TextMuted, st.sceneNames[i], 32u);
				// La CROIX n'apparait que s'il reste plus d'une scene : fermer la derniere
				// laisserait l'application sans document, etat qu'aucune autre partie de
				// l'interface ne sait representer.
				if (st.sceneCount > 1) {
					snprintf(key, sizeof(key), "tab.close.%d", i);
					const NkRect cr{x + tw - S(24.f), r.y + 2.f, S(20.f), h};
					HoverFill(p, cr, hit.Add(key, cr), 2.f);
					p.IconV(x + tw - S(20.f), r.y, r.h, NkIcon::WinClose, NkRole::TextMuted, 10.f);
					if (hit.Clicked(key)) {
						for (int32 k = i; k + 1 < st.sceneCount; ++k)
							NkWidgetState::Copy(st.sceneNames[k], st.sceneNames[k + 1], 31u);
						st.sceneCount--;
						if (st.activeTab >= st.sceneCount)
							st.activeTab = st.sceneCount - 1;
						break; // la liste a change : on ne continue pas a la parcourir
					}
				}
				snprintf(key, sizeof(key), "tab.%d", i);
				if (hit.Clicked(key))
					st.activeTab = i;
				x += tw + 3.f;
			}
			const NkRect ar{x + S(4.f), r.y + 2.f, S(24.f), h};
			HoverFill(p, ar, hit.Add("tab.add", ar));
			p.IconV(x + S(8.f), r.y, r.h, NkIcon::Add, NkRole::Text, 12.f);
			if (hit.Clicked("tab.add") && st.sceneCount < 8) {
				// Le nom par defaut est NUMEROTE : deux scenes nommees « Scene » seraient
				// indistinguables dans la barre.
				snprintf(st.sceneNames[st.sceneCount], 32, "Scene_%d", st.sceneCount + 1);
				st.activeTab = st.sceneCount;
				st.sceneCount++;
			}
		}

		// ── BARRE D'OUTILS ──────────────────────────────────────────────────────
		// LE MODE EST UN DEROULANT, PLUS UN COMMUTATEUR A DEUX ETATS. Rihen a
		// remarque que « Objet » et « Edition » faisaient doublon avec « Mode de
		// selection » -- et il a raison sur le fond : ce ne sont pas deux boutons,
		// c'est UNE liste de modes, qui va s'allonger (sculpt 2.5D, sculpt reel,
		// texturing, riggging...). Deux boutons cotes a cote auraient cesse de tenir
		// au troisieme mode.
		// « Mode de selection » reste a cote, et ne fait PAS doublon : il porte le
		// sous-mode sommet / arete / face, qui n'a de sens qu'EN edition.
		inline void PaintToolbar(NkModelerPainter &p, const NkRect &r, NkModelerState &st,
								 NkHitRegistry &hit, NkWidgetState &ws, NkComboPending &combo) {
			p.Fill(r, NkRole::PanelHeader);
			p.HLine(r.x, r.y + r.h - 1.f, r.w);
			float32 x = kPad;
			const float32 ih = S(14.f);

			// Bouton icone + libelle, avec sa zone sensible. Une seule fonction pour
			// que tous reagissent pareil.
			auto btn = [&](const char *key, NkIcon ic, const char *label) -> bool {
				const float32 w = ih + S(5.f) + p.TextW(label) + S(14.f);
				const NkRect br{x - S(7.f), r.y + S(5.f), w, r.h - S(10.f)};
				const bool over = hit.Add(key, br);
				HoverFill(p, br, over);
				p.IconV(x, r.y, r.h, ic, NkRole::Text, 14.f);
				p.TextV(x + ih + S(5.f), r.y, r.h, label);
				x += w;
				return hit.Clicked(key);
			};

			btn("tb.save", NkIcon::Save, "Enregistrer");
			p.VLine(x - S(4.f), r.y + S(7.f), r.h - S(14.f));
			x += S(6.f);

			// ── DEROULANT DE MODE ───────────────────────────────────────────────
			// Ce ne sont pas deux boutons mais UNE liste, qui va s'allonger. Deux
			// boutons auraient cesse de tenir au troisieme mode.
			static const NkIcon kModeIc[5] = {NkIcon::Mesh, NkIcon::Edit, NkIcon::Layers,
											  NkIcon::Ruler, NkIcon::Overlay};
			static const char *const kModeNames[5] = {"Objet", "Edition", "Sculpt 2.5D", "Sculpt",
													  "Texturing"};
			const float32 cbH = S(22.f), cbY = r.y + (r.h - cbH) * 0.5f;
			{
				int32 sel = (int32)st.mode;
				Combo(p, hit, ws, "tb.mode", {x, cbY, S(140.f), cbH}, kModeNames, kModeIc, 5, sel, combo);
				st.mode = (NkMode)sel;
			}
			x += S(148.f);
			p.VLine(x - S(7.f), r.y + S(7.f), r.h - S(14.f));

			// LE « MODE DE SELECTION » A ETE RETIRE D ICI. Rihen a demande a quoi
			// servait le « Face » a cote d « Objet » : c etait le sous-mode
			// sommet/arete/face, exactement la meme chose que les trois boutons de la
			// barre de la vue. Un doublon que j avais introduit sans le voir.
			// Il reste dans la VUE, ou il est a sa place : c est la qu on selectionne,
			// et c est la qu on doit pouvoir changer de sous-mode sans traverser
			// l ecran.

			// AJOUTER et MODIFICATEUR sont des LISTES, pas des boutons : ils ouvrent un
			// choix. Un bouton simple laisserait croire a une action immediate.
			{
				// « Ajouter » OUVRE LE MENU PAR CATEGORIES, il ne retient pas de
				// « primitive courante » : ajouter est une action, pas un reglage.
				const NkRect ar{x, cbY, S(96.f), cbH};
				const bool over = hit.Add("tb.addmenu", ar);
				const bool open = ws.ComboOpen("tb.addmenu");
				// ENCADRE comme tous les deroulants de la barre : sans cadre il se
				// lisait comme une etiquette, pas comme une commande.
				p.Outline(ar, (over || open) ? NkRole::AccentUi : NkRole::Border,
						  open ? NkRole::AccentUi : NkRole::InputBg, 3.f);
				const NkRole fg = open ? NkRole::TextOnAccent : NkRole::Text;
				p.IconV(ar.x + S(8.f), cbY, cbH, NkIcon::Add, fg, 13.f);
				p.TextV(ar.x + S(26.f), cbY, cbH, "Ajouter", fg);
				p.IconV(ar.x + ar.w - S(16.f), cbY, cbH,
						open ? NkIcon::ChevronUp : NkIcon::ChevronDown, fg, 11.f);
				if (hit.Clicked("tb.addmenu"))
					ws.ToggleCombo("tb.addmenu");
				st.addAnchor = ar; // le menu est peint apres tout le reste
				x += S(104.f);
			}
			// MODIFICATEUR : liste a DEUX NIVEAUX. Le bouton montre le modificateur
			// courant avec SON icone ; le clic ouvre les categories, et chaque
			// categorie ouvre ses entrees.
			{
				// LE BOUTON DIT « MODIFICATEUR », pas le nom du dernier choisi.
				// Afficher « Miroir » laisse croire a un reglage en cours alors que
				// c'est une commande d'AJOUT -- et le modificateur ajoute, lui, vit
				// dans le panneau Details.
				const NkRect mr{x, cbY, S(136.f), cbH};
				const bool over = hit.Add("tb.mod", mr);
				const bool open = ws.ComboOpen("tb.mod");
				p.Outline(mr, (over || open) ? NkRole::AccentUi : NkRole::Border, NkRole::InputBg, 3.f);
				p.IconV(x + S(8.f), cbY, cbH, NkIcon::Layers, NkRole::AccentUi, 13.f);
				p.TextV(x + S(27.f), cbY, cbH, "Modificateur");
				p.IconV(x + mr.w - S(18.f), cbY, cbH, open ? NkIcon::ChevronUp : NkIcon::ChevronDown,
						NkRole::Text, 11.f);
				if (hit.Clicked("tb.mod"))
					ws.ToggleCombo("tb.mod");
				st.modAnchor = mr; // memorise pour la liste, peinte apres tout le reste
				x += S(144.f);
			}

			// « Ajouter » et « Modificateur » etaient ecrits DEUX FOIS : une fois en
			// deroulant (ci-dessus, celui qui marche) et une fois en bouton plat ici.
			// Le doublon est retire -- deux commandes identiques cote a cote font
			// douter qu'elles fassent la meme chose.

			// Reglages cale a DROITE : action de session et non de modelisation, la
			// distance visuelle dit cette difference de nature.
			{
				const float32 sw = ih + S(5.f) + p.TextW("Reglages");
				const NkRect br{r.w - kPad - sw - S(7.f), r.y + S(5.f), sw + S(14.f), r.h - S(10.f)};
				HoverFill(p, br, hit.Add("tb.settings", br));
				p.IconV(r.w - kPad - sw, r.y, r.h, NkIcon::Gear, NkRole::Text, 14.f);
				p.TextV(r.w - kPad - sw + ih + S(5.f), r.y, r.h, "Reglages");
			}
		}

		// ── EN-TETE DE PANNEAU ──────────────────────────────────────────────────
		// Onglet + croix, comme la maquette. Rendu ici une seule fois : quatre
		// panneaux le partagent, et il n'y a donc qu'un endroit a corriger.
		// La croix de l'en-tete REFERME LE PANNEAU. Elle etait dessinee mais morte,
		// et c'est justement la contrepartie de la largeur minimale : puisqu'on ne
		// peut plus reduire un panneau jusqu'a le faire disparaitre, il faut un geste
		// franc pour le fermer -- et une poignee visible pour le rouvrir.
		//
		// `show` peut etre nul : certains en-tetes (l'onglet d'un panneau qui ne se
		// ferme pas) gardent la croix desactivee plutot que de la faire disparaitre,
		// pour que la rangee d'en-tetes reste alignee.
		inline float32 PaintPanelTab(NkModelerPainter &p, const NkRect &r, const char *title,
									 NkHitRegistry *hit = nullptr, bool *show = nullptr,
									 const char *key = nullptr) {
			const float32 h = 26.f;
			p.Fill({r.x, r.y, r.w, h}, NkRole::PanelHeader);
			p.TextV(r.x + kPad, r.y, h, title);
			const NkRect cr{r.x + r.w - 24.f, r.y + 3.f, 20.f, h - 6.f};
			bool over = false;
			if (hit && show && key) {
				over = hit->Add(key, cr);
				if (over)
					p.Fill(cr, NkRole::AccentUi, 2.f);
				if (hit->Clicked(key))
					*show = false;
			}
			p.IconV(r.x + r.w - 20.f, r.y, h, NkIcon::WinClose,
					over ? NkRole::TextOnAccent : NkRole::Text, 11.f);
			p.HLine(r.x, r.y + h - 1.f, r.w);
			return r.y + h;
		}

		// ── POIGNEE DE REOUVERTURE ──────────────────────────────────────────────
		// Un panneau ferme doit laisser une trace : sans elle, l'utilisateur qui a
		// clique la croix n'a plus aucun moyen de deviner comment revenir en arriere,
		// et le menu n'est pas un moyen de DEVINER -- c'est un moyen de retrouver ce
		// qu'on sait deja exister.
		// Le chevron pointe vers l'endroit ou le panneau reapparaitra.
		inline void PaintPanelHandle(NkModelerPainter &p, const NkRect &r, NkHitRegistry &hit,
									 const char *key, bool &show, NkIcon icon) {
			if (r.w <= 0.f || r.h <= 0.f)
				return;
			const bool over = hit.Add(key, r);
			p.Fill(r, over ? NkRole::AccentUi : NkRole::PanelHeader);
			if (r.w > r.h) {
				p.HLine(r.x, r.y, r.w);
				p.IconV(r.x + r.w * 0.5f - 7.f, r.y, r.h, icon,
						over ? NkRole::TextOnAccent : NkRole::TextMuted, 12.f);
			} else {
				p.VLine(r.x + (r.x < 4.f ? r.w - 1.f : 0.f), r.y, r.h);
				p.IconV(r.x + r.w * 0.5f - 7.f, r.y + r.h * 0.5f - 13.f, 26.f, icon,
						over ? NkRole::TextOnAccent : NkRole::TextMuted, 12.f);
			}
			if (over)
				hit.WantCursor(NkCursorWant::Hand);
			if (hit.Clicked(key))
				show = true;
		}

		// Champ de recherche, avec sa loupe. Sans l'icone, un champ vide au texte
		// grise se confond avec une etiquette desactivee.
		// CHAMP DE RECHERCHE REEL : bordure, saisie, effacement, et un filtre que
		// l'appelant applique. L'ancien etait un dessin -- une boite grise avec le
		// mot « Rechercher » -- qui ne recevait aucun clic et ne filtrait rien.
		inline float32 PaintSearch(NkModelerPainter &p, const NkRect &r, float32 y,
								   NkHitRegistry &hit, NkWidgetState &ws,
								   const nkgui::NkGuiInput &in, const char *key, char *buf) {
			const float32 h = 22.f;
			const NkRect fr{r.x + 6.f, y + 4.f, r.w - 12.f, h};
			// BORDURE : sans elle, le champ se confond avec le fond du panneau et
			// rien ne dit qu'on peut y ecrire.
			const bool editing = ws.IsEditing(key);
			p.Outline(fr, editing ? NkRole::AccentUi : NkRole::Border, NkRole::InputBg, 3.f);
			p.IconV(fr.x + 6.f, fr.y, h, NkIcon::Search, NkRole::TextMuted, 12.f);
			const NkRect tr{fr.x + 24.f, fr.y, fr.w - 48.f, h};
			if (editing) {
				p.TextV(tr.x, tr.y, h, ws.editBuf);
				const float32 cw = p.TextW(ws.editBuf);
				p.Fill({tr.x + cw + 1.f, tr.y + 3.f, 1.f, h - 6.f}, NkRole::Text);
				for (int32 i = 0; i < in.charCount; ++i) {
					const uint32 c = in.chars[i];
					if (c >= 32u && c < 127u && ws.editLen < 30u) {
						ws.editBuf[ws.editLen++] = (char)c;
						ws.editBuf[ws.editLen] = 0;
					}
				}
				if (in.KeyPressed(nkgui::NkGuiKey::Backspace) && ws.editLen > 0)
					ws.editBuf[--ws.editLen] = 0;
				// La recherche s'applique A CHAQUE FRAPPE : attendre Entree pour
				// filtrer une liste n'aurait aucun sens, on cherche justement en
				// voyant le resultat se reduire.
				NkWidgetState::Copy(buf, ws.editBuf, 31u);
				hit.Add(key, fr);
				if (in.KeyPressed(nkgui::NkGuiKey::Enter)
					|| in.KeyPressed(nkgui::NkGuiKey::Escape)
					|| (hit.AnyClick() && !hit.IsHovered(key)))
					ws.EndEdit();
			} else {
				const bool over = hit.Add(key, fr);
				if (over)
					hit.WantCursor(NkCursorWant::Hand);
				if (*buf)
					p.TextV(tr.x, tr.y, h, buf, NkRole::Text);
				else
					p.TextV(tr.x, tr.y, h, "Rechercher", NkRole::TextMuted);
				// SIMPLE clic, pas double : un champ de recherche s'ouvre au premier
				// clic. Le double-clic est reserve au renommage, ou il protege d'un
				// renommage accidentel -- ici il n'y a rien a proteger.
				if (hit.Clicked(key))
					ws.BeginEdit(key, buf);
			}
			// Croix d'effacement, seulement s'il y a quelque chose a effacer.
			if (*buf) {
				char ck[48];
				snprintf(ck, sizeof(ck), "%s.clear", key);
				const NkRect cr{fr.x + fr.w - 22.f, fr.y + 3.f, 18.f, h - 6.f};
				HoverFill(p, cr, hit.Add(ck, cr), 2.f);
				p.IconV(cr.x + 3.f, cr.y, cr.h, NkIcon::WinClose, NkRole::TextMuted, 10.f);
				if (hit.Clicked(ck)) {
					buf[0] = 0;
					if (editing)
						ws.EndEdit();
				}
			}
			return y + h + 8.f;
		}

		// Filtre insensible a la casse : « cu » trouve « Cube ». Un filtre sensible
		// obligerait a connaitre la casse exacte de ce qu'on cherche, ce qui est
		// exactement ce qu'on ne sait pas quand on cherche.
		inline bool NkNameMatches(const char *name, const char *filter) {
			if (!filter || !*filter)
				return true;
			if (!name)
				return false;
			for (const char *a = name; *a; ++a) {
				const char *x = a;
				const char *y = filter;
				while (*x && *y) {
					char cx = *x, cy = *y;
					if (cx >= 'A' && cx <= 'Z')
						cx = (char)(cx - 'A' + 'a');
					if (cy >= 'A' && cy <= 'Z')
						cy = (char)(cy - 'A' + 'a');
					if (cx != cy)
						break;
					++x;
					++y;
				}
				if (!*y)
					return true;
			}
			return false;
		}

		// ── HIERARCHIE (gauche) ─────────────────────────────────────────────────
		// Arbre REPLIABLE, noms MODIFIABLES, deux colonnes d'etat (oeil, cadenas), et
		// un clic dans le VIDE qui deselectionne.
		//
		// Ce dernier point compte plus qu'il n'en a l'air : sans lui, une fois un
		// objet selectionne on ne peut plus revenir a « rien de selectionne » sans
		// passer par un menu. Or « rien » est un etat legitime -- c'est celui ou les
		// commandes de scene s'appliquent.
		// La hierarchie liste LA SCENE, plus des lignes inventees. Chaque ligne est
		// un slot du viewport : les indices sont stables (tableau a trous), donc les
		// cles de zones cliquables restent valides d'une image a l'autre.
		inline NkIcon NkObjectIcon(int32 type) {
			switch (type) {
				case nk3d::kVpObjLightPoint:
				case nk3d::kVpObjLightSun:
				case nk3d::kVpObjLightSpot:
					return NkIcon::Light;
				case nk3d::kVpObjCamera:
					return NkIcon::Camera;
				case nk3d::kVpObjEmpty:
					return NkIcon::Gizmo;
				default:
					return NkIcon::Mesh;
			}
		}

		inline const char *NkObjectTypeName(int32 type) {
			switch (type) {
				case nk3d::kVpObjLightPoint:
				case nk3d::kVpObjLightSun:
				case nk3d::kVpObjLightSpot:
					return "Lumiere";
				case nk3d::kVpObjCamera:
					return "Camera";
				case nk3d::kVpObjEmpty:
					return "Repere";
				default:
					return "Maillage";
			}
		}

		inline void PaintHierarchy(NkModelerPainter &p, const NkRect &r, NkModelerState &st,
								   NkHitRegistry &hit, NkWidgetState &ws, const nkgui::NkGuiInput &in) {
			p.Fill(r, NkRole::PanelBg);
			p.VLine(r.x + r.w - 1.f, r.y, r.h);
			float32 y = PaintPanelTab(p, r, "Hierarchie", &hit, &st.showLeft, "hier.close");
			y = PaintSearch(p, r, y, hit, ws, in, "hier.search", st.searchHier);

			const float32 colEye = r.x + r.w - S(74.f);
			const float32 colLock = r.x + r.w - S(52.f);
			const float32 colType = r.x + r.w - S(152.f);

			p.Fill({r.x, y, r.w, kRowH}, NkRole::WindowBg);
			p.TextV(r.x + S(34.f), y, kRowH, "Nom");
			p.TextV(colType, y, kRowH, "Type", NkRole::TextMuted);
			p.HLine(r.x, y + kRowH - 1.f, r.w);
			y += kRowH;

			const float32 listTop = y;
			const float32 listH = r.y + r.h - kRowH - listTop;
			const NkRect listR{r.x, listTop, r.w, listH};
			// La zone de LISTE est declaree en PREMIER : les lignes la recouvrent.
			// Ce qui reste attribue a la liste est exactement le VIDE -- et c'est la
			// qu'un clic deselectionne.
			hit.Add("hier.list", listR);
			p.Clip(listR);

			char key[40];
			float32 yy = y - st.scrollHier;
			int32 visibleCount = 0;

			// Racine : la scene, renommable. Elle ne se selectionne pas -- c'est un
			// contenant, pas un objet.
			{
				const NkRect rowR{r.x, yy, r.w, kRowH};
				hit.Add("hier.scene", rowR);
				p.IconV(r.x + S(6.f), yy, kRowH, NkIcon::Globe, NkRole::Text, 13.f);
				EditableText(p, hit, ws, in, "hier.scene.name",
							 {r.x + S(24.f), yy, colType - r.x - S(30.f), kRowH}, st.sceneNames[0],
							 NkRole::Text, st.sceneNames[0], 32u);
				yy += kRowH;
				++visibleCount;
			}

			const int32 nSlots = nk3d::Viewport3DObjectCount();
			int32 aliveCount = 0, selCount = 0;
			for (int32 i = 0; i < nSlots; ++i) {
				if (!nk3d::Viewport3DObjectAlive(i))
					continue;
				++aliveCount;
				// LE FILTRE ECARTE LA LIGNE, il ne la grise pas : une liste filtree
				// qui garde ses lignes barrees n'est pas plus courte, donc elle ne
				// sert a rien.
				if (!NkNameMatches(nk3d::Viewport3DObjectName(i), st.searchHier))
					continue;
				const bool sel = nk3d::Viewport3DObjectSelected(i);
				if (sel)
					++selCount;
				++visibleCount;
				const NkRect rowR{r.x, yy, r.w, kRowH};
				if (yy >= listTop - kRowH && yy < listTop + listH) {
					snprintf(key, sizeof(key), "hier.row.%d", i);
					const bool over = hit.Add(key, rowR);
					if (sel)
						p.Fill(rowR, NkRole::AccentUi);
					else
						HoverFill(p, rowR, over, 0.f);
					const NkRole fg = sel ? NkRole::TextOnAccent : NkRole::Text;
					const NkRole dim = sel ? NkRole::TextOnAccent : NkRole::TextMuted;
					const int32 type = nk3d::Viewport3DObjectType(i);
					float32 tx = r.x + S(20.f);
					p.IconV(tx, yy, kRowH, NkObjectIcon(type), fg, 13.f);

					// NOM MODIFIABLE. Le tampon de saisie doit SURVIVRE d'une image a
					// l'autre : ma version le recopiait depuis le viewport a chaque
					// image, donc chaque caractere tape etait efface aussitot -- il
					// etait litteralement impossible de renommer quoi que ce soit.
					// On garde donc un tampon persistant par ligne, resynchronise
					// tant qu'on n'edite PAS cette ligne-la.
					snprintf(key, sizeof(key), "hier.name.%d", i);
					const int32 slot = i % 32;
					if (!ws.IsEditing(key))
						snprintf(st.objectNames[slot], 32, "%s", nk3d::Viewport3DObjectName(i));
					const NkRect nameR{tx + S(18.f), yy, colType - tx - S(46.f), kRowH};
					EditableText(p, hit, ws, in, key, nameR, st.objectNames[slot], fg,
								 st.objectNames[slot], 32u);
					if (strcmp(st.objectNames[slot], nk3d::Viewport3DObjectName(i)) != 0)
						nk3d::Viewport3DRenameObject(i, st.objectNames[slot]);
					p.TextV(colType, yy, kRowH, NkObjectTypeName(type), dim);

					// SUPPRESSION depuis la hierarchie. Elle n'existait nulle part :
					// la touche X ne servait qu'en mode objet, et rien ne le disait.
					snprintf(key, sizeof(key), "hier.del.%d", i);
					const NkRect delR{colType - S(22.f), yy, S(20.f), kRowH};
					HoverFill(p, delR, hit.Add(key, delR) && !sel, 2.f);
					p.IconV(colType - S(19.f), yy, kRowH, NkIcon::Trash, dim, 12.f);
					if (hit.Clicked(key)) {
						nk3d::Viewport3DDeleteObject(i);
						continue; // la ligne n'existe plus : ne pas la finir
					}

					// L'oeil pilote la VISIBILITE DE SCENE, pas un drapeau d'interface.
					const bool vis = nk3d::Viewport3DObjectVisible(i);
					snprintf(key, sizeof(key), "hier.eye.%d", i);
					const NkRect eyeR{colEye - S(3.f), yy, S(20.f), kRowH};
					HoverFill(p, eyeR, hit.Add(key, eyeR) && !sel, 2.f);
					p.IconV(colEye, yy, kRowH, vis ? NkIcon::Eye : NkIcon::EyeClosed, vis ? fg : dim,
							13.f);
					if (hit.Clicked(key))
						nk3d::Viewport3DSetObjectVisible(i, !vis);

					// CADENAS : il interdit la selection ET la modification. Il avait
					// disparu quand la hierarchie est passee sur la scene reelle --
					// c'etait une colonne de la maquette, il fallait la rebrancher sur
					// un vrai etat d'objet, pas la supprimer.
					const bool lok = nk3d::Viewport3DObjectLocked(i);
					snprintf(key, sizeof(key), "hier.lock.%d", i);
					const NkRect lockR{colLock - S(3.f), yy, S(20.f), kRowH};
					HoverFill(p, lockR, hit.Add(key, lockR) && !sel, 2.f);
					p.IconV(colLock, yy, kRowH, lok ? NkIcon::Lock : NkIcon::Unlock,
							lok ? fg : dim, 13.f);
					if (hit.Clicked(key))
						nk3d::Viewport3DSetObjectLocked(i, !lok);

					snprintf(key, sizeof(key), "hier.row.%d", i);
					// Un objet VERROUILLE ne se selectionne pas, meme depuis la
					// hierarchie : sinon le cadenas ne promettrait rien.
					if (hit.Clicked(key) && !lok)
						nk3d::Viewport3DSelectObject(i, hit.ShiftDown());
				}
				yy += kRowH;
			}

			// Scene VIDE : le dire, et dire quoi faire -- SOUS la racine, pas dessus.
			// Le texte etait pose a une hauteur fixe qui chevauchait la ligne de la
			// scene.
			if (aliveCount == 0)
				p.TextV(r.x + S(24.f), yy, kRowH, "Vide -- Ajouter pour creer un objet",
						NkRole::TextMuted);

			p.Unclip();

			// UN CLIC DANS LE VIDE DESELECTIONNE.
			if (hit.Clicked("hier.list"))
				nk3d::Viewport3DDeselectAllObjects();

			hit.Wheel("hier.list", st.scrollHier, (float32)visibleCount * kRowH, listH);
			p.VScroll(listR, (float32)visibleCount * kRowH, st.scrollHier);

			const float32 fy = r.y + r.h - kRowH;
			p.Fill({r.x, fy, r.w, kRowH}, NkRole::WindowBg);
			p.HLine(r.x, fy, r.w);
			char foot[72];
			snprintf(foot, sizeof(foot), "%d objet(s), %d selectionne(s)", aliveCount, selCount);
			p.TextV(r.x + kPad, fy, kRowH, foot, NkRole::TextMuted);
		}

		// ── GIZMO DE NAVIGATION, FACON BLENDER ──────────────────────────────────
		// Six boules reliees au centre, une par DEMI-AXE. Les positives sont PLEINES
		// et portent leur lettre ; les negatives sont CREUSES et muettes.
		//
		// C'est cette dissymetrie qui fait tout le travail : elle dit d'un coup d'oeil
		// de quel cote on regarde. Un simple trepied a trois branches, comme celui que
		// j'avais dessine, ne distingue pas +X de -X -- on ne sait donc jamais si la
		// scene est vue de face ou de dos.
		//
		// Les boules du FOND sont dessinees en PREMIER : sans cet ordre, un demi-axe
		// qui s'eloigne passerait par-dessus celui qui s'approche, et la profondeur
		// serait inversee.
		inline void PaintNavGizmo(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
								  float32 cx, float32 cy, float32 radius) {
			// IL TOURNE AVEC LA VUE, comme celui de Blender. La version precedente
			// avait une projection isometrique FIGEE : elle affichait toujours la
			// meme orientation, donc elle ne disait rien de ce qu'on regardait --
			// c'etait un decor. On projette maintenant les axes du MONDE avec le
			// meme repere que la scene : X, Y et Z se placent exactement la ou ils
			// se trouvent dans l'image.
			float32 rgt[3], upv[3], fwd[3];
			nk3d::Viewport3DCameraAxes(rgt, upv, fwd);

			struct Half {
					float32 wx, wy, wz;	 ///< direction MONDE du demi-axe
					NkRole role;
					const char *label;
					bool positive;
			};
			static const Half kHalf[6] = {
				{1.f, 0.f, 0.f, NkRole::AxisX, "X", true},
				{-1.f, 0.f, 0.f, NkRole::AxisX, "", false},
				{0.f, 1.f, 0.f, NkRole::AxisY, "Y", true},
				{0.f, -1.f, 0.f, NkRole::AxisY, "", false},
				{0.f, 0.f, 1.f, NkRole::AxisZ, "Z", true},
				{0.f, 0.f, -1.f, NkRole::AxisZ, "", false},
			};
			// Chaque demi-axe amene sa vue : cliquer +X regarde DEPUIS +X.
			// Correspondance avec Viewport3DAxisView : 0 face (-Z monde), 1 droite
			// (+X), 2 dessus (+Y).
			struct View {
					int32 which;
					bool opposite;
			};
			static const View kViews[6] = {
				{1, false}, {1, true},	// +X droite / -X gauche
				{2, false}, {2, true},	// +Y dessus / -Y dessous
				{0, true},	{0, false}, // +Z arriere / -Z face
			};

			const float32 ball = radius * 0.28f;
			struct Proj {
					float32 sx, sy, depth;
			};
			Proj pr[6];
			for (int32 i = 0; i < 6; ++i) {
				const float32 wx = kHalf[i].wx, wy = kHalf[i].wy, wz = kHalf[i].wz;
				// Projection sur le repere camera : droite -> +x ecran, haut -> -y
				// ecran (l'ecran descend), avant -> profondeur.
				pr[i].sx = wx * rgt[0] + wy * rgt[1] + wz * rgt[2];
				pr[i].sy = -(wx * upv[0] + wy * upv[1] + wz * upv[2]);
				// Profondeur : positif = vers l'observateur, donc l'OPPOSE de l'axe
				// « avant » de la camera, qui pointe vers la scene.
				pr[i].depth = -(wx * fwd[0] + wy * fwd[1] + wz * fwd[2]);
			}

			// Ordre de PROFONDEUR reel : ce qui est derriere se peint d'abord. Un
			// tri complet plutot qu'un tableau fige -- l'orientation change, donc
			// l'ordre aussi.
			int32 order[6] = {0, 1, 2, 3, 4, 5};
			for (int32 a = 0; a < 6; ++a)
				for (int32 b = a + 1; b < 6; ++b)
					if (pr[order[b]].depth < pr[order[a]].depth) {
						const int32 t = order[a];
						order[a] = order[b];
						order[b] = t;
					}

			// ROTATION LIBRE : tirer le CORPS du gizmo fait tourner la vue. Les
			// boules restent des raccourcis vers les six vues d'axe, mais elles ne
			// suffisent pas -- Blender permet aussi de le faire pivoter a la main,
			// et c'est souvent le geste le plus rapide pour se replacer.
			// La zone du corps est declaree AVANT les boules : elles la recouvrent,
			// donc viser une boule reste un clic sur la boule.
			{
				const NkRect body{cx - radius, cy - radius, radius * 2.f, radius * 2.f};
				const bool overBody = hit.Add("nav.body", body);
				if (overBody)
					hit.WantCursor(NkCursorWant::Hand);
				if (hit.Clicked("nav.body")) {
					st.navDragMode = 2;
					st.navDragLastX = hit.Mouse().x;
					st.navDragLastY = hit.Mouse().y;
				}
			}

			char key[24];
			for (int32 k = 0; k < 6; ++k) {
				const int32 i = order[k];
				const float32 ex = cx + pr[i].sx * (radius - ball);
				const float32 ey = cy + pr[i].sy * (radius - ball);
				snprintf(key, sizeof(key), "nav.axis.%d", i);
				const NkRect br{ex - ball, ey - ball, ball * 2.f, ball * 2.f};
				const bool over = hit.Add(key, br);
				// La tige ne part que des demi-axes POSITIFS : six tiges feraient une
				// etoile illisible.
				if (kHalf[i].positive)
					p.Line(cx, cy, ex, ey, kHalf[i].role, 2.f);
				if (kHalf[i].positive) {
					p.Disc(ex, ey, over ? ball + 2.f : ball, kHalf[i].role);
					const float32 lw = p.TextW(kHalf[i].label);
					p.Text(ex - lw * 0.5f, ey - p.LineH() * 0.5f, kHalf[i].label,
						   NkRole::TextOnAccent);
				} else {
					// Creuse : le trou reprend le fond de la VUE -- le gizmo flotte
					// au-dessus de la scene.
					p.Ring(ex, ey, over ? ball + 2.f : ball, kHalf[i].role, NkRole::ViewportTop);
				}
				if (over)
					hit.WantCursor(NkCursorWant::Hand);
				if (hit.Clicked(key))
					nk3d::Viewport3DAxisView(kViews[i].which, kViews[i].opposite);
			}
		}

		// ── COLONNE DE BOUTONS DE VUE ───────────────────────────────────────────
		// Zoom, deplacement lateral, camera, bascule orthographique/perspective.
		// VERTICALE et sous le gizmo, comme chez Blender : ce sont des commandes de
		// NAVIGATION, pas d'edition, et les tenir a l'ecart des outils evite de
		// changer d'outil en croyant deplacer la vue.
		inline void PaintViewButtons(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
									 float32 x, float32 y) {
			// QUATRE COMMANDES DE NAVIGATION, et elles agissent vraiment :
			//   Zoom      -> recadre sur la scene (c'est le « zoom pour tout voir ») ;
			//   Deplacer  -> remet la cible a l'origine sans changer l'angle ;
			//   Camera    -> bascule la vue en camera... plus tard : pour l'instant
			//                elle recadre aussi, et le bouton est GRISE plutot que de
			//                mentir ;
			//   Ortho     -> bascule perspective / orthographique.
			struct VB {
					NkIcon ic;
					const char *key;
					const char *tip;
					bool enabled;
			};
			const VB kBtns[4] = {
				{NkIcon::Zoom, "view.frame", "Cadrer la scene", true},
				{NkIcon::Pan, "view.center", "Recentrer", true},
				{NkIcon::Camera, "view.cam", "Vue camera (a venir)", false},
				{NkIcon::Ortho, "view.ortho", "Perspective / orthographique", true},
			};
			const float32 d = 26.f;
			// Le RECADRAGE reste accessible : double-clic sur la loupe. Le
			// glissement regle le zoom, le double-clic cadre tout -- deux besoins
			// differents sur le meme bouton, comme dans la plupart des editeurs.
			if (hit.DoubleClicked("view.frame"))
				nk3d::Viewport3DFrameAll();
			for (int32 i = 0; i < 4; ++i) {
				const NkRect br{x, y + (float32)i * (d + 6.f), d, d};
				const bool over = kBtns[i].enabled && hit.Add(kBtns[i].key, br);
				const bool on = (i == 3) && nk3d::Viewport3DIsOrtho();
				if (on)
					p.Fill(br, NkRole::AccentUi, 4.f);
				else
					p.Outline(br, over ? NkRole::AccentUi : NkRole::Border, NkRole::PanelHeader, 4.f);
				p.IconV(br.x + (d - 14.f) * 0.5f, br.y, d, kBtns[i].ic,
						!kBtns[i].enabled ? NkRole::TextMuted
										  : (on ? NkRole::TextOnAccent : NkRole::Text),
						14.f);
				if (over)
					hit.WantCursor(NkCursorWant::Hand);
				if (kBtns[i].enabled && hit.Clicked(kBtns[i].key)) {
					if (i == 0 || i == 1) {
						// LOUPE ET MAIN SONT DES GLISSEMENTS, pas des clics : on
						// attrape le bouton et on tire. C'est ce que fait Blender, et
						// c'est le seul acces a la navigation sur un portable sans
						// molette ni bouton du milieu. Un clic simple ne pourrait
						// exprimer ni la quantite ni la direction.
						st.navDragMode = i;
						st.navDragLastX = hit.Mouse().x;
						st.navDragLastY = hit.Mouse().y;
					} else if (i == 3) {
						const bool o = !nk3d::Viewport3DIsOrtho();
						nk3d::Viewport3DSetOrtho(o);
						st.projection = o ? 1 : 0;
						st.lastProjection = st.projection;
					}
				}
			}
		}

		// ── VUE 3D (centre) ─────────────────────────────────────────────────────
		inline void PaintViewport(NkModelerPainter &p, const NkRect &r, NkModelerState &st,
								  NkHitRegistry &hit, NkWidgetState &ws, NkComboPending &combo,
								  NkCheckPending &checks, const NkShortcutTable &sc) {
			const bool editMode = (st.mode != NkMode::Object);

			// ── LA VUE 3D REELLE ────────────────────────────────────────────────
			// Ce qui etait peint ici jusqu'a present -- un sol en fuyantes dessine a
			// la main -- etait un DECOR. Il donnait l'impression d'une perspective
			// sans caméra, sans profondeur et sans le moindre objet : rien de ce
			// qu'on y voyait ne pouvait etre selectionne, tourne ni modifie.
			//
			// A la place, la scene est rendue par NKRenderer dans une cible hors
			// ecran, sur le meme device et le meme command buffer que l'interface,
			// et on pose ici SA TEXTURE. Aucune relecture CPU, aucune seconde pile
			// GPU : l'image ne quitte jamais la carte.
			p.Fill(r, NkRole::ViewportTop); // fond, visible tant que la 3D n'est pas prete
			if (nk3d::Viewport3DReady()) {
				p.Image(nk3d::kViewportTexId, r);
			} else if (const char *e = nk3d::Viewport3DError()) {
				// UN ECHEC SE DIT. Un viewport reste noir ne distingue pas « la carte
				// a refuse la cible » de « la scene est vide », et on cherche le
				// probleme du mauvais cote pendant une heure.
				char msg[128];
				snprintf(msg, sizeof(msg), "Vue 3D indisponible : %s", e);
				p.TextV(r.x + S(16.f), r.y, r.h, msg, NkRole::TextMuted);
			}

			// ── NAVIGATION ──────────────────────────────────────────────────────
			// Convention Blender, celle que connaissent les gens qui utiliseront ca :
			// bouton du MILIEU pour orbiter, MILIEU + Maj pour deplacer, molette pour
			// zoomer. La zone est declaree AVANT les barres flottantes : elles sont
			// peintes ensuite, donc elles gagnent le survol la ou elles se trouvent,
			// et un clic sur une liste ne fait pas pivoter la camera.
			{
				const bool overView = hit.Add("view.nav", r);
				const math::NkVec2 m = hit.Mouse();
				if (overView && hit.MiddleDown()) {
					const float32 dx = m.x - st.navLastX, dy = m.y - st.navLastY;
					if (st.navDragging) {
						// DELTAS BRUTS, en pixels. J'appliquais un facteur 0,008 avant
						// de les passer : or NkOrbitCameraController3D applique DEJA sa
						// sensibilite (mRotateSpeed) a l'interieur. Le tour etait donc
						// cent fois trop faible -- la scene ne bougeait pas. Demo3D
						// passe mdx/mdy bruts, exactement comme ici.
						if (hit.ShiftDown())
							nk3d::Viewport3DPan(dx, dy);
						else
							nk3d::Viewport3DOrbit(dx, dy);
					}
					st.navDragging = true;
				} else {
					st.navDragging = false;
				}
				st.navLastX = m.x;
				st.navLastY = m.y;
				if (overView && hit.WheelDelta() != 0.f) {
					// Molette seule = zoom ; Maj = deplacement vertical ; Ctrl =
					// horizontal. C'est le jeu de raccourcis de Blender, et il evite
					// d'avoir a lacher la souris pour recadrer.
					if (hit.ShiftDown())
						nk3d::Viewport3DPanSteps(0.f, hit.WheelDelta());
					else if (hit.CtrlDown())
						nk3d::Viewport3DPanSteps(hit.WheelDelta(), 0.f);
					else
						nk3d::Viewport3DZoom(hit.WheelDelta());
				}

				// ── GLISSEMENT DE NAVIGATION EN COURS ───────────────────────────
			// Il se poursuit MEME SI la souris quitte le bouton : c'est le bouton
			// ENFONCE qui commande, pas la position. Sans cela, un geste un peu
			// rapide lacherait la navigation en pleine course.
			if (st.navDragMode >= 0) {
				if (!hit.MouseDown()) {
					st.navDragMode = -1;
				} else {
					const math::NkVec2 mm = hit.Mouse();
					const float32 ndx = mm.x - st.navDragLastX;
					const float32 ndy = mm.y - st.navDragLastY;
					if (st.navDragMode == 2)
						nk3d::Viewport3DOrbitFree(ndx, ndy);
					else
						nk3d::Viewport3DNavDrag(st.navDragMode, ndx, ndy);
					st.navDragLastX = mm.x;
					st.navDragLastY = mm.y;
					hit.WantCursor(st.navDragMode == 1 ? NkCursorWant::Hand
													   : NkCursorWant::ResizeNS);
				}
			}

			// ── OUTILS DE SELECTION PAR ZONE ────────────────────────────
			// Rectangle (B), lasso (Ctrl + glisser) et cercle (C, molette =
			// rayon). Portes de Demo3D : le coeur de la selection etait deja
			// ecrit dans le viewport, il ne lui manquait que ce pilote et son
			// trace -- un outil qu'on ne voit pas est un outil qu'on croit casse.
			{
				const bool ctrlLasso = overView && hit.CtrlDown() && hit.MouseDown()
									   && st.zoneTool < 0 && !st.zoneActive;
				if (ctrlLasso) {
					st.zoneTool = 1; // lasso : arme par le geste lui-meme
					st.zoneActive = true;
					st.lassoCount = 0;
				}
				if (st.zoneTool >= 0) {
					hit.WantCursor(NkCursorWant::Hand);
					const math::NkVec2 mz = hit.Mouse();
					if (st.zoneTool == 2) {
						// CERCLE : il peint tant que le bouton est enfonce, et la
						// molette regle son rayon. Pas de « depart » a memoriser.
						st.zoneX1 = mz.x;
						st.zoneY1 = mz.y;
						if (overView && hit.WheelDelta() != 0.f) {
							st.zoneRadius += hit.WheelDelta() * 6.f;
							if (st.zoneRadius < 8.f)
								st.zoneRadius = 8.f;
							if (st.zoneRadius > 400.f)
								st.zoneRadius = 400.f;
						}
						if (overView && hit.MouseDown())
							nk3d::Viewport3DSelectCircle(mz.x - r.x, mz.y - r.y, st.zoneRadius,
														 hit.ShiftDown() ? 1 : 0);
						p.Ring(mz.x, mz.y, st.zoneRadius, NkRole::AccentUi,
							   NkRole::ViewportTop);
					} else if (hit.MouseDown()) {
						if (!st.zoneActive) {
							st.zoneActive = true;
							st.zoneX0 = mz.x;
							st.zoneY0 = mz.y;
							st.lassoCount = 0;
						}
						st.zoneX1 = mz.x;
						st.zoneY1 = mz.y;
						if (st.zoneTool == 1
							&& st.lassoCount < NkModelerState::kMaxLasso) {
							// Un point tous les quelques pixels : suivre chaque
							// image donnerait des milliers de points pour un
							// contour que trente decrivent aussi bien.
							const int32 n = st.lassoCount;
							if (n == 0
								|| (fabsf(st.lasso[(n - 1) * 2] - mz.x) > 4.f
									|| fabsf(st.lasso[(n - 1) * 2 + 1] - mz.y) > 4.f)) {
								st.lasso[n * 2] = mz.x;
								st.lasso[n * 2 + 1] = mz.y;
								st.lassoCount = n + 1;
							}
						}
					} else if (st.zoneActive) {
						// RELACHEMENT : on applique, puis l'outil se desarme --
						// comme Blender, ou B ne sert qu'a un rectangle.
						const int32 mode = hit.ShiftDown() ? 1 : (hit.CtrlDown() ? 2 : 0);
						if (st.zoneTool == 0) {
							nk3d::Viewport3DSelectRect(st.zoneX0 - r.x, st.zoneY0 - r.y,
													   st.zoneX1 - r.x, st.zoneY1 - r.y, mode);
						} else if (st.zoneTool == 1 && st.lassoCount >= 3) {
							float32 pts[NkModelerState::kMaxLasso * 2];
							for (int32 k = 0; k < st.lassoCount; ++k) {
								pts[k * 2] = st.lasso[k * 2] - r.x;
								pts[k * 2 + 1] = st.lasso[k * 2 + 1] - r.y;
							}
							nk3d::Viewport3DSelectLasso(pts, (uint32)st.lassoCount, mode);
						}
						st.zoneActive = false;
						st.zoneTool = -1;
						st.lassoCount = 0;
					}

					// TRACE de la zone. Sans lui on selectionne a l'aveugle.
					if (st.zoneActive && st.zoneTool == 0) {
						const float32 x0 = st.zoneX0 < st.zoneX1 ? st.zoneX0 : st.zoneX1;
						const float32 y0 = st.zoneY0 < st.zoneY1 ? st.zoneY0 : st.zoneY1;
						const float32 x1 = st.zoneX0 < st.zoneX1 ? st.zoneX1 : st.zoneX0;
						const float32 y1 = st.zoneY0 < st.zoneY1 ? st.zoneY1 : st.zoneY0;
						p.OutlineSharp({x0, y0, x1 - x0, y1 - y0}, NkRole::AccentUi);
					} else if (st.zoneActive && st.zoneTool == 1) {
						for (int32 k = 1; k < st.lassoCount; ++k)
							p.Line(st.lasso[(k - 1) * 2], st.lasso[(k - 1) * 2 + 1],
								   st.lasso[k * 2], st.lasso[k * 2 + 1], NkRole::AccentUi, 1.5f);
					}
				}
			}

			// CURSEUR 3D : Maj + clic DROIT le pose sous la souris, comme
			// Blender. Le clic droit seul reste libre pour le menu contextuel.
			if (overView && hit.RightClicked("view.nav") && hit.ShiftDown())
				nk3d::Viewport3DPlaceCursor(m.x - r.x, m.y - r.y);

			// CLIC GAUCHE = SELECTION, mais seulement si le gizmo ne l'a pas pris.
				// L'ordre compte : les rubans de rotation couvrent une large zone et
				// avaleraient tous les clics si on ne les interrogeait pas d'abord.
				// Les coordonnees sont RELATIVES a la vue -- la cible hors ecran a sa
				// propre origine.
				if (overView && st.zoneTool < 0 && hit.Clicked("view.nav")
				&& !nk3d::Viewport3DGizmoDragging()) {
					nk3d::Viewport3DPick(m.x - r.x, m.y - r.y, hit.ShiftDown(), hit.CtrlDown());
					st.dirty = true;
				}
			}

			// ── BARRE FLOTTANTE GAUCHE : ce qu'on REGARDE ───────────────────────
			// Trois listes deroulantes, chacune avec son icone d'etat. Les menus de
			// commandes vivent dans la barre d'outils principale : les dupliquer ici
			// donnerait deux endroits a tenir a jour pour une seule liste.
			const float32 barH = S(26.f), barY = r.y + S(10.f);
			{
				int32 nP = 0, nS = 0, nO = 0;
				const char *const *proj = NkProjectionItems(nP);
				const bool edit = (st.mode != NkMode::Object);
				const char *const *shad = NkShadingItems(nS);
				const char *const *ovl = NkOverlayItems(nO);
				// ── GROUPE DE VUE : ICONES SEULES ───────────────────────────────
				// Le groupe faisait pres de 500 px parce que chaque liste etait
				// dimensionnee sur son libelle LE PLUS LONG -- « Sans surimpression »
				// imposait sa largeur en permanence, y compris quand « Grille seule »
				// etait choisi.
				//
				// Dimensionner sur le libelle COURANT aurait fait respirer la barre a
				// chaque changement, et deplace les boutons voisins sous le curseur.
				// On garde donc les ICONES SEULES : c'est ce que fait Blender dans son
				// en-tete de vue, et l'icone porte deja l'etat -- camera contre grille
				// pour la projection, ampoule contre disque pour l'ombrage, oeil ouvert
				// contre ferme pour les surimpressions.
				//
				// Le libelle n'est pas perdu : il reste dans la liste deroulee, avec la
				// coche sur la valeur courante. On passe de ~500 px a ~120.
				const float32 ib = S(28.f);
				const int32 nBtn = 4 + (st.shading == 0 ? (st.solidLight == 1 ? 2 : 1) : 0);
				const float32 groupW = S(6.f) + (float32)nBtn * (ib + 2.f);
				float32 bx = r.x + S(10.f);
				p.Fill({bx, barY, groupW, barH}, NkRole::PanelBg, 5.f);
				bx += S(3.f);

				// L'icone a gauche porte SON contenu : disposition des vues, plein ecran,
				// camera enregistree, reinitialisation.
				int32 nV = 0;
				const char *const *vitems = NkViewMenuItems(nV);
				Combo(p, hit, ws, "vp.menu", {bx, barY + 2.f, ib, barH - 4.f}, vitems, NkViewMenuIcons(),
					  nV, st.viewLayout, combo, true, false, false);
				bx += ib + 2.f;
				p.VLine(bx - 1.f, barY + S(5.f), barH - S(10.f));

				Combo(p, hit, ws, "vp.proj", {bx, barY + 2.f, ib, barH - 4.f}, proj, NkProjectionIcons(),
					  nP, st.projection, combo, true, false, false);
				bx += ib + 2.f;
				Combo(p, hit, ws, "vp.shade", {bx, barY + 2.f, ib, barH - 4.f}, shad, NkShadingIcons(), nS,
					  st.shading, combo, true, false, false);
				bx += ib + 2.f;
				// ── FOND DE LA VUE ──────────────────────────────────────────
				// Des PREREGLAGES, pas une roue chromatique : cinq fonds couvrent
				// les usages reels (sombre, noir pur pour les silhouettes, gris
				// neutre pour juger les couleurs, clair pour les captures, bleu
				// nuit). Un selecteur libre viendra avec l'editeur de theme.
				{
					static const char *const kBg[] = {"Fond sombre", "Fond noir", "Fond gris",
													  "Fond clair", "Fond bleu nuit"};
					static NkIcon kBgIc[5];
					for (int32 i = 0; i < 5; ++i)
						kBgIc[i] = NkIcon::Circle;
					Combo(p, hit, ws, "vp.bg", {bx, barY + 2.f, ib, barH - 4.f}, kBg, kBgIc, 5,
						  st.bgChoice, combo, true, false, false);
					bx += ib + 2.f;
					static const float32 kBgCol[5][3] = {{0.05f, 0.05f, 0.07f},
														 {0.01f, 0.01f, 0.012f},
														 {0.24f, 0.24f, 0.25f},
														 {0.62f, 0.63f, 0.65f},
														 {0.05f, 0.07f, 0.13f}};
					nk3d::Viewport3DSetBackground(kBgCol[st.bgChoice][0], kBgCol[st.bgChoice][1],
												  kBgCol[st.bgChoice][2]);
				}

				// AFFICHAGE : liste a CASES, pas a choix unique. On veut la grille ET le
				// repere sans les normales -- trois presets exclusifs obligeaient a
				// choisir la combinaison la moins mauvaise au lieu de composer.
				CheckCombo(p, hit, ws, "vp.ovl", {bx, barY + 2.f, ib, barH - 4.f}, ovl,
						   NkOverlayIcons(), nO, st.overlayMask, NkIcon::Eye, checks);
				bx += ib + 2.f;

				// ECLAIRAGE DU MODE SOLIDE : studio, matcap ou plat. Il n apparait que
				// si l ombrage est SOLIDE -- c est un reglage DE ce mode, pas un mode.
				// Et il reste disponible en mode objet comme en edition : on veut lire
				// une forme au matcap avant meme d entrer en edition.
				if (st.shading == 0) {
					int32 nL = 0;
					const char *const *lights = NkSolidLightItems(nL);
					Combo(p, hit, ws, "vp.solidlight", {bx, barY + 2.f, ib, barH - 4.f}, lights,
						  NkSolidLightIcons(), nL, st.solidLight, combo, true, false, false);
					bx += ib + 2.f;
					// Le CHOIX du matcap ne s ouvre que si le matcap est l eclairage
					// retenu : proposer huit matcaps sous un eclairage studio laisserait
					// croire a un reglage sans effet.
					if (st.solidLight == 1) {
						int32 nM = 0;
						const char *const *mats = NkMatcapItems(nM);
						static NkIcon kMatIc[8];
						for (int32 i = 0; i < 8; ++i)
							kMatIc[i] = NkIcon::Matcap;
						Combo(p, hit, ws, "vp.matcap", {bx, barY + 2.f, ib, barH - 4.f}, mats, kMatIc,
							  nM, st.matcap, combo, true, false, false);
					}
				}
			}

			// ── BARRE FLOTTANTE DROITE : ce qu'on FAIT ──────────────────────────
			// TROIS GROUPES SEPARES PAR DU VIDE, et c'est ce que Rihen demandait :
			//   1. les sous-modes de selection (mode edition seulement) ;
			//   2. les outils -- « que fait mon clic ? », un seul actif ;
			//   3. les reglages : repere, vitesse de camera, aimantations.
			// Un seul bloc continu obligeait a compter les boutons pour retrouver le
			// sien. L'espace fait le tri sans qu'on ait a lire.
			const float32 btn = S(24.f);
			const float32 grp = S(10.f); // vide entre deux groupes
			struct Snap {
					NkIcon icon;
					const char *value;
			};
			static const Snap kSnaps[3] = {
				{NkIcon::SnapGrid, "10"},
				{NkIcon::SnapAngle, "10 deg"},
				{NkIcon::SnapScale, "0,25"},
			};

			// Largeurs, calculees d'abord pour caler le tout a droite.
			const bool editMode2 = (st.mode != NkMode::Object);
			const float32 wSub = editMode2 ? (S(8.f) + 3.f * (btn + 2.f)) : 0.f;
			const float32 wTools = S(8.f) + 5.f * (btn + 2.f);
			float32 wSet = S(8.f) + 2.f * (btn + 2.f);
			for (int32 i = 0; i < 3; ++i)
				wSet += btn + p.TextW(kSnaps[i].value) + S(12.f);
			float32 tx = r.x + r.w - S(10.f) - wSet;

			// Groupe 3 : reglages (a droite).
			{
				p.Fill({tx, barY, wSet, barH}, NkRole::PanelBg, 5.f);
				float32 cx = tx + S(4.f);
				int32 nOr = 0, nCam = 0;
				const char *const *orients = NkOrientItems(nOr);
				const char *const *cams = NkCamSpeedItems(nCam);
				// GLOBE et CAMERA : des listes SANS chevron ni libelle, reduites a leur
				// icone. Ce sont des reglages qu'on consulte rarement ; leur donner la
				// largeur d'un libelle prendrait la place de commandes utilisees a
				// chaque geste. L'etat reste lisible : c'est l'icone qui change.
				Combo(p, hit, ws, "vp.orient", {cx, barY + 1.f, btn, barH - 2.f}, orients, NkOrientIcons(),
					  nOr, st.orientation, combo, true, false, false);
				cx += btn + 2.f;
				static const NkIcon kCamIc[4] = {NkIcon::Camera, NkIcon::Camera, NkIcon::Camera,
												 NkIcon::Camera};
				Combo(p, hit, ws, "vp.cam", {cx, barY + 1.f, btn, barH - 2.f}, cams, kCamIc, nCam,
					  st.camSpeed, combo, true, false, false);
				cx += btn + S(8.f);
				p.VLine(cx - S(4.f), barY + S(6.f), barH - S(12.f));

				bool *const flags[3] = {&st.snapGrid, &st.snapAngle, &st.snapScale};
				static const char *const kKeys[3] = {"vp.snap.0", "vp.snap.1", "vp.snap.2"};
				for (int32 i = 0; i < 3; ++i) {
					const NkRect br{cx, barY + 2.f, btn, barH - 4.f};
					const bool over = hit.Add(kKeys[i], br);
					const bool on = *flags[i];
					if (on)
						p.Fill(br, NkRole::AccentUi, 3.f);
					else
						HoverFill(p, br, over);
					if (hit.Clicked(kKeys[i]))
						*flags[i] = !on;
					p.IconV(cx + (btn - S(13.f)) * 0.5f, barY, barH, kSnaps[i].icon,
							on ? NkRole::TextOnAccent : NkRole::TextMuted, 13.f);
					cx += btn + 3.f;
					// La valeur reste AFFICHEE quand l'aimantation est coupee, mais
					// attenuee : on veut savoir sur quel pas on retombera en la rallumant.
					p.TextV(cx, barY, barH, kSnaps[i].value, on ? NkRole::Text : NkRole::TextMuted);
					cx += p.TextW(kSnaps[i].value) + S(9.f);
				}
			}

			// Groupe 2 : outils.
			tx -= grp + wTools;
			{
				p.Fill({tx, barY, wTools, barH}, NkRole::PanelBg, 5.f);
				float32 cx = tx + S(4.f);
				const NkIcon kTools[5] = {NkIcon::Cursor, NkIcon::Gizmo, NkIcon::Move, NkIcon::Rotate,
										  NkIcon::Scale};
				static const char *const kKeys[5] = {"vp.t.0", "vp.t.1", "vp.t.2", "vp.t.3", "vp.t.4"};
				for (int32 i = 0; i < 5; ++i) {
					const NkRect br{cx, barY + 2.f, btn, barH - 4.f};
					const bool on = ((int32)st.tool == i);
					if (i == 0) {
						// LE BOUTON DE SELECTION EST UNE LISTE DE FORMES : rectangle,
						// cercle, lasso. C'est ce qu'annoncait deja le petit point ; il
						// ouvre desormais vraiment un choix.
						// Son icone suit la FORME choisie et non un dessin fixe : sinon
						// rien ne dirait, une fois le lasso choisi, qu'on est en lasso.
						int32 nS2 = 0;
						const char *const *shapes = NkSelShapeItems(nS2);
						if (on)
							p.Fill(br, NkRole::AccentUi, 3.f);
						Combo(p, hit, ws, "vp.selshape", br, shapes, NkSelShapeIcons(), nS2,
							  st.selShape, combo, true, false, false);
						if (hit.Clicked("vp.selshape"))
							st.tool = NkTool::Select; // choisir une forme active l'outil
						p.Fill({cx + btn - S(6.f), barY + barH - S(9.f), S(3.f), S(3.f)},
							   on ? NkRole::TextOnAccent : NkRole::Text);
						cx += btn + 2.f;
						continue;
					}
					const bool over = hit.Add(kKeys[i], br);
					if (on)
						p.Fill(br, NkRole::AccentUi, 3.f);
					else
						HoverFill(p, br, over);
					if (hit.Clicked(kKeys[i]))
						st.tool = (NkTool)i;
					p.IconV(cx + (btn - S(14.f)) * 0.5f, barY, barH, kTools[i],
							on ? NkRole::TextOnAccent : NkRole::Text, 14.f);
					cx += btn + 2.f;
				}
			}

			// Groupe 1 : sous-modes, en mode edition seulement.
			if (editMode2) {
				tx -= grp + wSub;
				p.Fill({tx, barY, wSub, barH}, NkRole::PanelBg, 5.f);
				float32 cx = tx + S(4.f);
				const NkIcon kSub[3] = {NkIcon::Dot, NkIcon::Ruler, NkIcon::Square};
				static const char *const kKeys[3] = {"vp.sub.0", "vp.sub.1", "vp.sub.2"};
				for (int32 i = 0; i < 3; ++i) {
					const NkRect br{cx, barY + 2.f, btn, barH - 4.f};
					const bool over = hit.Add(kKeys[i], br);
					const bool on = ((int32)st.subMode == i);
					if (on)
						p.Fill(br, NkRole::AccentUi, 3.f);
					else
						HoverFill(p, br, over);
					if (hit.Clicked(kKeys[i]))
						st.subMode = (NkSubMode)i;
					p.IconV(cx + (btn - S(14.f)) * 0.5f, barY, barH, kSub[i],
							on ? NkRole::TextOnAccent : NkRole::Text, 14.f);
					cx += btn + 2.f;
				}
			}

			// ── GIZMO DE NAVIGATION + BOUTONS DE VUE, en bas a gauche.
			// Chez Blender ils sont en haut a droite ; ici la place y est prise par
			// les outils, et Rihen a demande de garder le coin bas-gauche.
			// Les boutons de navigation sont AU-DESSUS du gizmo, comme chez Blender :
			// on descend du plus abstrait (l'orientation) vers le plus direct (zoom,
			// deplacement). Les mettre dessous les collait au bord de la fenetre.
			const float32 gz = 34.f;
			const float32 navH = 4.f * 26.f + 3.f * 5.f;
			const float32 navY = r.y + r.h - 22.f - gz * 2.f - 14.f - navH;
			PaintViewButtons(p, hit, st, r.x + 14.f, navY);
			PaintNavGizmo(p, hit, st, r.x + 12.f + gz, r.y + r.h - 22.f - gz, gz);

			// ── PANNEAU DE DERNIERE OPERATION. Il FLOTTE au-dessus de la scene et n'est
			// pas encastre dans un bord : il appartient a la vue, pas au cadre.
			if (editMode) {
				const float32 pw = 214.f, ph = 4.f * kRowH + 6.f;
				const float32 px = r.x + 12.f, py = r.y + r.h - ph - 80.f;
				p.Fill({px, py, pw, ph}, NkRole::PanelHeader, 4.f);
				p.IconV(px + 6.f, py, kRowH, NkIcon::ChevronDown, NkRole::Text, 11.f);
				p.TextV(px + 22.f, py, kRowH, "Extruder la region");
				float32 ry = py + kRowH;
				static const char *const kL[] = {"Distance", "Decalage"};
				static const char *const kV[] = {"0,25", "0,00"};
				for (int32 i = 0; i < 2; ++i) {
					p.TextV(px + kPad, ry, kRowH, kL[i], NkRole::TextMuted);
					p.Fill({px + 112.f, ry + 3.f, 92.f, 16.f}, NkRole::InputBg, 2.f);
					p.TextV(px + 118.f, ry, kRowH, kV[i]);
					ry += kRowH;
				}
				p.Fill({px + kPad, ry + 5.f, 12.f, 12.f}, NkRole::AccentUi, 2.f);
				p.TextV(px + kPad + 18.f, ry, kRowH, "Decalage pair", NkRole::TextMuted);
			}

			// Le raccourci de l'operation courante est LU dans la table via sa CLE DE
			// COMMANDE, jamais recopie : rebinder la touche changera cet affichage tout
			// seul. La cle est stable, l'index ne l'est pas.
			{
				const char *cmd = editMode ? "edit.extruder" : "objet.deplacer";
				char keys[32];
				if (sc.FormatFor(cmd, keys, sizeof(keys))) {
					char hint[96];
					snprintf(hint, sizeof(hint), "%s   %s", editMode ? "Extruder" : "Deplacer", keys);
					p.TextV(r.x + 12.f, r.y + r.h - 24.f, 20.f, hint, NkRole::TextMuted);
				}
			}
		}

		// ── LIGNE DE TRANSFORMATION ─────────────────────────────────────────────
		// TROIS CADRES DE LARGEUR IDENTIQUE, puis DEUX COLONNES CARREES pour les
		// icones. La largeur egale n'est pas cosmetique : trois champs de tailles
		// differentes se lisent comme trois choses differentes, alors que X, Y et Z
		// sont la meme grandeur sur trois axes.
		//
		// Les deux colonnes d'icones sont CARREES et RESERVEES meme quand la ligne
		// n'a qu'une icone : sans reservation, les champs de « Rotation » seraient
		// plus larges que ceux de « Position », et les trois lignes ne s'aligneraient
		// plus verticalement.
		//
		// LES VALEURS SE MODIFIENT EN GLISSANT, comme dans Blender et Unreal. C'est le
		// geste le plus utilise d'un modeleur : bien plus souvent qu'on ne tape un
		// nombre, on veut « un peu plus, un peu moins » en regardant le resultat.
		inline void PaintTransformRow(NkModelerPainter &p, NkHitRegistry &hit, NkWidgetState &ws,
									  const nkgui::NkGuiInput &in, const NkRect &r, float32 y,
									  const char *label, float32 *v, float32 step, const char *keyBase,
									  NkIcon icon1, NkIcon icon2, const char *fmt = "%.2f") {
			const float32 rowH = kRowH + S(6.f);
			p.Fill({r.x, y, kLabelW, rowH}, NkRole::LabelCol);
			p.TextV(r.x + kPad, y, rowH, label);

			const float32 sq = rowH - S(6.f); // colonne carree : cote = hauteur du champ
			const float32 gap = S(4.f);
			const float32 iconsW = sq * 2.f + gap;
			const float32 avail = r.w - kLabelW - S(10.f) - iconsW - gap;
			float32 fw = (avail - gap * 2.f) / 3.f;
			if (fw < S(40.f))
				fw = S(40.f);

			const NkRole axes[3] = {NkRole::AxisX, NkRole::AxisY, NkRole::AxisZ};
			float32 x = r.x + kLabelW + S(5.f);
			char key[48];
			for (int32 i = 0; i < 3; ++i) {
				snprintf(key, sizeof(key), "%s.%d", keyBase, i);
				DragFloat(p, hit, ws, in, key, {x, y + S(3.f), fw, rowH - S(6.f)}, v[i], step, axes[i], fmt);
				x += fw + gap;
			}

			// Les deux carres. Une icone absente laisse sa case VIDE plutot que de
			// decaler la suivante -- l'alignement des trois lignes prime.
			float32 ix = r.x + r.w - S(5.f) - iconsW;
			const NkIcon icons[2] = {icon1, icon2};
			for (int32 i = 0; i < 2; ++i) {
				if (icons[i] == NkIcon::None) {
					ix += sq + gap;
					continue;
				}
				const NkRect br{ix, y + S(3.f), sq, sq};
				snprintf(key, sizeof(key), "%s.ic%d", keyBase, i);
				const bool over = hit.Add(key, br);
				p.Outline(br, over ? NkRole::AccentUi : NkRole::Border, NkRole::PanelBg, 3.f);
				p.IconV(br.x + (sq - S(13.f)) * 0.5f, br.y, sq, icons[i], NkRole::Text, 13.f);
				// Reinitialiser remet la valeur NEUTRE de la grandeur : zero pour une
				// position ou une rotation, UN pour une echelle -- remettre une echelle
				// a zero ferait disparaitre l'objet.
				if (hit.Clicked(key) && icons[i] == NkIcon::Refresh) {
					const float32 neutral = (step > 0.05f) ? 0.f : 1.f;
					v[0] = v[1] = v[2] = neutral;
				}
				ix += sq + gap;
			}
			p.HLine(r.x, y + rowH - 1.f, r.w);
		}

		inline float32 Vec3RowH() {
			return kRowH + S(6.f);
		}

		// ── EN-TETE DE SECTION REPLIABLE ────────────────────────────────────────
		// La fleche REPLIE VRAIMENT la section. Une fleche qui ne fait rien est pire
		// qu'une absence de fleche : elle promet une commande et ne la tient pas.
		inline bool SectionHeader(NkModelerPainter &p, NkHitRegistry &hit, const NkRect &r, float32 y,
								  const char *key, const char *title, bool &open) {
			const NkRect hr{r.x, y, r.w, kRowH};
			const bool over = hit.Add(key, hr);
			if (over)
				p.Fill(hr, NkRole::PanelHeader);
			p.IconV(r.x + S(6.f), y, kRowH, open ? NkIcon::ChevronDown : NkIcon::ChevronRight,
					NkRole::Text, 11.f);
			p.TextV(r.x + S(22.f), y, kRowH, title);
			if (hit.Clicked(key))
				open = !open;
			return open;
		}

		// ── PROPRIETES (droite, haut) ───────────────────────────────────────────
		inline void PaintProperties(NkModelerPainter &p, const NkRect &full, NkModelerState &st,
									NkHitRegistry &hit, NkWidgetState &ws, const nkgui::NkGuiInput &in) {
			p.Fill(full, NkRole::PanelBg);
			p.VLine(full.x, full.y, full.h);
			float32 y = PaintPanelTab(p, full, "Proprietes", &hit, &st.showRight, "prop.close");
			const NkRect r = Inset(full);
			y = PaintSearch(p, r, y, hit, ws, in, "props.search", st.searchProps);

			// LES CINQ PASTILLES « General / Objet / Rendu / Physique / Tout » ONT
			// ETE RETIREES. Rihen a demande a quoi elles servaient : a rien. Elles
			// venaient de la maquette et annonçaient quatre familles de reglages
			// dont trois n'existent pas -- il n'y a ni materiau d'objet, ni physique.
			// Une barre de filtres qui ne filtre rien apprend a ne plus lire les
			// filtres, y compris ceux qui marcheront un jour.
			//
			// Elles reviendront quand il y aura vraiment plusieurs familles a
			// separer, et elles porteront alors le meme mecanisme que les sections
			// de Details : un bit par famille, teste a la peinture.
			p.HLine(full.x, y, full.w);
			y += 1.f;

			const float32 listTop = y;
			// Meme decoupe que Details : sinon la section Transformation remonte sur
			// l'en-tete « Proprietes » et deborde sur Details en dessous.
			const NkRect clipR{full.x, listTop, full.w, full.y + full.h - listTop};
			p.Clip(clipR);
			y -= st.scrollProps;

			if (SectionHeader(p, hit, r, y, "prop.sec.transform", "Transformation", st.showTransform)) {
				y += kRowH;
				// LES CHAMPS SONT CEUX DE L'OBJET ACTIF, dans les deux sens : on lit
				// sa transformation avant de peindre (le gizmo a pu la changer), on la
				// reecrit apres (les champs ont pu etre edites). Sans objet, la
				// section le dit au lieu d'afficher des zeros editables qui ne
				// commandent rien.
				const int32 act = nk3d::Viewport3DActiveObject();
				if (act >= 0 && nk3d::Viewport3DObjectAlive(act)) {
					nk3d::Viewport3DGetObjectTransform(act, st.pos, st.rot, st.scl);
					PaintTransformRow(p, hit, ws, in, r, y, "Position", st.pos, 0.01f, "prop.pos",
									  NkIcon::Refresh, NkIcon::Lock);
					y += Vec3RowH();
					PaintTransformRow(p, hit, ws, in, r, y, "Rotation", st.rot, 0.5f, "prop.rot",
									  NkIcon::Refresh, NkIcon::None, "%.1f");
					y += Vec3RowH();
					PaintTransformRow(p, hit, ws, in, r, y, "Echelle", st.scl, 0.01f, "prop.scl",
									  NkIcon::Refresh, NkIcon::Lock, "%.3f");
					y += Vec3RowH();
					nk3d::Viewport3DSetObjectTransform(act, st.pos, st.rot, st.scl);
					// ── EDITION PROPORTIONNELLE ─────────────────────────────
					// Les sommets voisins suivent en s'attenuant, dans un rayon
					// donne. C'est l'outil qui distingue une deformation organique
					// d'un deplacement de sommets : sans lui, bouger un point d'un
					// visage laisse un pic.
					{
						p.Fill({r.x, y, kLabelW, kRowH}, NkRole::LabelCol);
						p.TextV(r.x + kPad, y, kRowH, "Proportionnel");
						const NkRect cb{r.x + kLabelW + 8.f, y + 5.f, 14.f, 14.f};
						const bool over = hit.Add("prop.prop", {r.x + kLabelW, y, 30.f, kRowH});
						if (st.proportional) {
							p.Fill(cb, NkRole::AccentUi, 2.f);
							p.IconV(cb.x + 1.f, cb.y - 1.f, 16.f, NkIcon::Check,
									NkRole::TextOnAccent, 12.f);
						} else {
							p.Outline(cb, over ? NkRole::AccentUi : NkRole::Border,
									  NkRole::InputBg, 2.f);
						}
						if (hit.Clicked("prop.prop"))
							st.proportional = !st.proportional;
						// Le RAYON n'apparait que si l'option est active : un reglage
						// visible mais sans effet fait douter de tout le panneau.
						if (st.proportional) {
							const NkRect rr{r.x + kLabelW + 34.f, y + 2.f, r.w - kLabelW - 42.f,
											kRowH - 4.f};
							DragFloat(p, hit, ws, in, "prop.proprad", rr, st.proportionalRadius,
									  0.01f, NkRole::AccentUi, "%.2f");
							if (st.proportionalRadius < 0.01f)
								st.proportionalRadius = 0.01f;
						}
						p.HLine(r.x, y + kRowH - 1.f, r.w);
						y += kRowH;
					}
				} else {
					p.TextV(r.x + kPad, y, kRowH, "Aucun objet selectionne", NkRole::TextMuted);
					y += kRowH;
				}
			} else {
				y += kRowH;
			}

			p.Unclip();
			const NkRect area = clipR;
			hit.Add("props.list", area);
			hit.Wheel("props.list", st.scrollProps, y - listTop + st.scrollProps, area.h);
			p.VScroll(area, y - listTop + st.scrollProps, st.scrollProps);
		}

		// ── DETAILS (droite, bas) ───────────────────────────────────────────────
		// LE DEROULANT DE MODIFICATEURS EST CELUI DE LA MAQUETTE, et il est ici
		// VOLONTAIREMENT tel quel. J'avais dessine une PILE (activation,
		// reordonnancement, application, retrait) parce que c'est ce qu'il faut
		// fonctionnellement : l'ordre est signifiant, un miroir apres une subdivision
		// ne donne pas le meme maillage qu'avant. Mais la consigne est de coller a la
		// maquette d'abord et de mettre a jour en developpant -- la pile reviendra
		// quand les modificateurs seront reellement branches sur NkModifierParams,
		// avec l'accord de Rihen sur son dessin.
		// Sections repliables du panneau Details. L'index sert de bit dans
		// `st.detailOpen` : un booleen par section eparpillerait l'etat et
		// obligerait a passer un tableau de pointeurs a chaque appel.
		enum NkDetailSection : uint32 {
			NkDetailMesh = 0,
			NkDetailModifiers = 1,
			NkDetailMaterial = 2,
			NkDetailSubMesh = 3,
		};

		// En-tete de section repliable. Retourne l'etat OUVERT/FERME apres le clic,
		// pour que l'appelant saute le corps sans le recalculer.
		inline bool DetailHeader(NkModelerPainter &p, NkHitRegistry &hit, const NkRect &r, float32 &y,
								 NkModelerState &st, uint32 bit, const char *label) {
			const NkRect hr{r.x, y, r.w, kRowH};
			char key[40];
			snprintf(key, sizeof(key), "det.sec.%u", bit);
			const bool over = hit.Add(key, hr);
			HoverFill(p, hr, over, 0.f);
			const bool open = (st.detailOpen & (1u << bit)) != 0u;
			p.IconV(r.x + 6.f, y, kRowH, open ? NkIcon::ChevronDown : NkIcon::ChevronRight,
					NkRole::Text, 11.f);
			p.TextV(r.x + 22.f, y, kRowH, label);
			y += kRowH;
			// LE CHEVRON REPLIE VRAIMENT. Il etait dessine mais mort : un chevron qui
			// ne fait rien apprend a l'utilisateur a ne plus essayer de cliquer, et
			// c'est une lecon qu'il applique ensuite aux chevrons qui, eux, marchent.
			// Toute la ligne est cliquable, pas seulement la fleche -- viser 11 px de
			// haut n'a aucun interet quand la ligne entiere est sans ambiguite.
			if (hit.Clicked(key))
				st.detailOpen ^= (1u << bit);
			return open;
		}

		inline void PaintDetails(NkModelerPainter &p, const NkRect &full, NkModelerState &st,
								 NkHitRegistry &hit, NkWidgetState &ws, const nkgui::NkGuiInput &in) {
			const float32 scroll = st.scrollDetails;
			p.Fill(full, NkRole::PanelBg);
			p.VLine(full.x, full.y, full.h);
			p.HLine(full.x, full.y, full.w);
			// Proprietes et Details partagent UNE colonne : les fermer ferme donc la
			// colonne entiere, et la poignee de droite les ramene toutes les deux. Les
			// separer demanderait deux poignees pour un gain nul -- on ne travaille pas
			// avec les proprietes sans les details.
			float32 y = PaintPanelTab(p, full, "Details (Cube)", &hit, &st.showRight, "det.close");
			const NkRect r = Inset(full);
			const float32 listTop = y;
			// LE CONTENU EST DECOUPE au rectangle qui reste sous l'en-tete. Sans cela
			// « Maillage » remonte par-dessus l'onglet « Details » des le premier cran
			// de molette, et les sections du bas debordent sur le navigateur. C'etait
			// visible et c'est corrige ici plutot qu'en bornant le defilement : borner
			// ne changerait rien, le debordement vient du DESSIN.
			const NkRect clipR{full.x, listTop, full.w, full.y + full.h - listTop};
			p.Clip(clipR);
			y -= scroll;

			// ── MAILLAGE ────────────────────────────────────────────────────────
			if (DetailHeader(p, hit, r, y, st, NkDetailMesh, "Maillage")) {
				// LES ARETES MANQUAIENT. Sur un maillage a demi-aretes elles ne sont
				// pas une curiosite : c'est la seule des trois quantites qui trahit un
				// maillage non-manifold (une arete portee par trois faces) et c'est
				// aussi le sous-mode d'edition du milieu. En afficher deux sur trois
				// laissait croire que le compte d'aretes n'existait pas.
				static const char *const kL[] = {"Sommets", "Aretes", "Faces", "Triangles"};
				// PLUS DE VALEURS EN DUR. Elles viennent du NkEditMesh lui-meme : un
				// panneau qui affiche « 8 sommets » quoi qu'il arrive est pire qu'un
				// panneau vide, parce qu'on le croit.
				uint32 nv = 0, ne = 0, nf = 0, nt = 0;
				nk3d::Viewport3DStats(nv, ne, nf, nt);
				char vbuf[4][24];
				snprintf(vbuf[0], sizeof(vbuf[0]), "%u", nv);
				snprintf(vbuf[1], sizeof(vbuf[1]), "%u", ne);
				snprintf(vbuf[2], sizeof(vbuf[2]), "%u", nf);
				snprintf(vbuf[3], sizeof(vbuf[3]), "%u", nt);
				const char *kV[4] = {vbuf[0], vbuf[1], vbuf[2], vbuf[3]};
				for (int32 i = 0; i < 4; ++i) {
					p.Fill({r.x, y, kLabelW, kRowH}, NkRole::LabelCol);
					p.TextV(r.x + kPad, y, kRowH, kL[i]);
					p.TextV(r.x + kLabelW + kPad, y, kRowH, kV[i], NkRole::TextMuted);
					p.HLine(r.x, y + kRowH - 1.f, r.w);
					y += kRowH;
				}
			}

			// ── MODIFICATEURS : LA PILE REELLE ──────────────────────────────────
			// Ce qui etait peint ici -- « Selectionner un modificateur » -- etait une
			// facade. La pile vit sur l'objet actif, elle est NON DESTRUCTIVE (la
			// cage editee reste la base) et l'ORDRE Y EST SIGNIFIANT : miroir puis
			// reseau ne donne pas la meme chose que reseau puis miroir. D'ou les
			// fleches de reordonnancement, qui ne sont pas un confort.
			//
			// L'affichage est GENERIQUE : chaque modificateur publie la liste de ses
			// parametres (libelle, type, bornes) et on la parcourt. Recopier a la
			// main dix-sept jeux de reglages aurait diverge au premier ajout moteur.
			if (DetailHeader(p, hit, r, y, st, NkDetailModifiers, "Modificateurs")) {
				const uint32 nMods = nk3d::Viewport3DModifierCount();
				if (nMods == 0u) {
					p.TextV(r.x + kPad, y, kRowH, "Aucun -- barre d'outils > Modificateur",
							NkRole::TextMuted);
					y += kRowH;
				}
				char mkey[48];
				for (uint32 m = 0; m < nMods; ++m) {
					const int32 type = nk3d::Viewport3DModifierTypeAt(m);
					const bool on = nk3d::Viewport3DModifierEnabled(m);
					const NkRect hr{r.x, y, r.w, kRowH};
					p.Fill(hr, NkRole::PanelHeader);

					// L'OEIL desactive sans supprimer : comparer avec et sans est le
					// geste le plus courant d'une pile.
					snprintf(mkey, sizeof(mkey), "det.mod.eye.%u", m);
					const NkRect er{r.x + 4.f, y + 3.f, 20.f, kRowH - 6.f};
					HoverFill(p, er, hit.Add(mkey, er), 2.f);
					p.IconV(r.x + 6.f, y, kRowH, on ? NkIcon::Eye : NkIcon::EyeClosed,
							on ? NkRole::Text : NkRole::TextMuted, 12.f);
					if (hit.Clicked(mkey))
						nk3d::Viewport3DSetModifierEnabled(m, !on);

					p.TextV(r.x + 26.f, y, kRowH, nk3d::Viewport3DModifierTypeName(type),
							on ? NkRole::Text : NkRole::TextMuted);

					// Monter / descendre / appliquer / retirer, cales a droite.
					float32 bx = r.x + r.w - 22.f;
					struct MB {
							NkIcon ic;
							int32 act; ///< 0 retirer, 1 appliquer, 2 descendre, 3 monter
					};
					static const MB kMB[4] = {{NkIcon::Trash, 0},
											  {NkIcon::Check, 1},
											  {NkIcon::ChevronDown, 2},
											  {NkIcon::ChevronUp, 3}};
					for (int32 b = 0; b < 4; ++b) {
						snprintf(mkey, sizeof(mkey), "det.mod.b%d.%u", b, m);
						const NkRect br{bx - 2.f, y + 3.f, 20.f, kRowH - 6.f};
						HoverFill(p, br, hit.Add(mkey, br), 2.f);
						p.IconV(bx, y, kRowH, kMB[b].ic, NkRole::TextMuted, 11.f);
						if (hit.Clicked(mkey)) {
							switch (kMB[b].act) {
								case 0:
									nk3d::Viewport3DRemoveModifier(m);
									break;
								case 1:
									// DESTRUCTIF : cuit le modificateur dans le maillage
									// et le retire de la pile. C'est tout son objet, et
									// Ctrl+Z le defait (un instantane est pris).
									nk3d::Viewport3DApplyModifier(m);
									break;
								case 2:
									nk3d::Viewport3DMoveModifier(m, false);
									break;
								default:
									nk3d::Viewport3DMoveModifier(m, true);
									break;
							}
							st.dirty = true;
						}
						bx -= 22.f;
					}
					y += kRowH;

					// ── Parametres, decrits par le modificateur lui-meme ─────
					const uint32 nP = nk3d::Viewport3DModifierParamCount(m);
					for (uint32 pi = 0; pi < nP; ++pi) {
						const char *plabel = "";
						int32 ptype = 2;
						float32 pmin = 0.f, pmax = 0.f;
						if (!nk3d::Viewport3DModifierParamInfo(m, pi, &plabel, &ptype, &pmin, &pmax))
							continue;
						p.Fill({r.x, y, kLabelW, kRowH}, NkRole::LabelCol);
						p.TextV(r.x + kPad + 8.f, y, kRowH, plabel);
						float32 v = nk3d::Viewport3DGetModifierParam(m, pi);
						snprintf(mkey, sizeof(mkey), "det.mod.p%u.%u", m, pi);
						const NkRect fr{r.x + kLabelW + 4.f, y + 2.f, r.w - kLabelW - 12.f,
										kRowH - 4.f};
						if (ptype == 0) {
							// BOOLEEN : une case, pas un champ numerique.
							const NkRect cb{fr.x + 2.f, fr.y + 2.f, 14.f, 14.f};
							const bool over2 = hit.Add(mkey, {fr.x, fr.y, 22.f, fr.h});
							if (v != 0.f) {
								p.Fill(cb, NkRole::AccentUi, 2.f);
								p.IconV(cb.x + 1.f, cb.y - 1.f, 16.f, NkIcon::Check,
										NkRole::TextOnAccent, 12.f);
							} else {
								p.Outline(cb, over2 ? NkRole::AccentUi : NkRole::Border,
										  NkRole::InputBg, 2.f);
							}
							if (hit.Clicked(mkey))
								nk3d::Viewport3DSetModifierParam(m, pi, v != 0.f ? 0.f : 1.f);
						} else {
								// Les ENTIERS se tirent par pas de 1, les flottants par
							// centiemes : un pas unique rendrait les uns inatteignables
							// et les autres inutilisables.
							const float32 step = (ptype == 1) ? 1.f : 0.01f;
							const char *fmt = (ptype == 1) ? "%.0f" : "%.3f";
							float32 nv = v;
							DragFloat(p, hit, ws, in, mkey, fr, nv, step, NkRole::AccentUi, fmt);
							if (nv != v) {
								if (pmin != pmax) { // bornes publiees : on les respecte
									if (nv < pmin)
										nv = pmin;
									if (nv > pmax)
										nv = pmax;
								}
								nk3d::Viewport3DSetModifierParam(m, pi, nv);
							}
						}
						p.HLine(r.x, y + kRowH - 1.f, r.w);
						y += kRowH;
					}
					y += 4.f;
				}
			}

			// ── MATERIAUX : PLUSIEURS PAR MODELE ────────────────────────────────
			// Rihen le rappelle et c'est structurant : un modele n'a pas UN materiau,
			// il a une LISTE D'EMPLACEMENTS. Chaque face du maillage porte l'indice de
			// l'emplacement qui la peint ; l'ensemble des faces qui partagent un indice
			// forme un SOUS-MAILLAGE. C'est exactement le modele de Blender, et c'est
			// aussi ce qu'attend le rendu : un tampon de dessin par emplacement.
			//
			// Consequence sur le format du maillage : `NkEditMesh` doit porter un
			// `materialSlot` PAR FACE, pas une couleur par objet. Consequence sur
			// l'edition : selectionner des faces (ou des sommets, dont on deduit les
			// faces) puis « Assigner » ecrit cet indice -- ce qui CREE le sous-maillage
			// sans qu'aucune geometrie soit dupliquee ni separee.
			//
			// Ce panneau n'est pour l'instant qu'une facade : les emplacements sont en
			// dur et « Assigner » n'ecrit rien. Le cablage vient avec la vue 3D, quand
			// il y aura une vraie selection a assigner.
			if (DetailHeader(p, hit, r, y, st, NkDetailMaterial, "Materiaux")) {
				// LES DEUX MATERIAUX ET LES DEUX SOUS-MAILLAGES QUI S'AFFICHAIENT ICI
				// ETAIENT SIMULES. Rihen a demande d'ou ils sortaient : de nulle part,
				// c'etait une maquette. Un panneau qui invente son contenu est pire
				// qu'un panneau vide -- on lui fait confiance.
				//
				// Le modele reste celui annonce : un modele porte une LISTE
				// d'emplacements, chaque face du maillage porte l'indice de celui qui
				// la peint, et l'ensemble des faces d'un meme indice forme un
				// sous-maillage. Les emplacements viendront des materiaux crees dans
				// le navigateur de projet ; tant qu'aucun n'existe, il n'y a rien a
				// montrer et on le dit.
				p.TextV(r.x + kPad, y, kRowH, "Aucun emplacement", NkRole::TextMuted);
				y += kRowH;
				const float32 bw = (r.w - 24.f) / 3.f;
				struct Btn {
						const char *label;
						NkIcon ic;
				};
				static const Btn kB[] = {
					{"Ajouter", NkIcon::Add}, {"Retirer", NkIcon::Trash}, {"Assigner", NkIcon::Check}};
				for (int32 i = 0; i < 3; ++i) {
					const NkRect br{r.x + 8.f + (float32)i * bw, y + 3.f, bw - 4.f, kRowH - 6.f};
					// TOUT est grise tant qu'aucun materiau n'existe : « Assigner »
					// demande en plus une selection de faces, donc le mode edition.
					const bool off = true;
					const NkRole fg = off ? NkRole::TextMuted : NkRole::Text;
					p.Outline(br, NkRole::Border, NkRole::InputBg, 2.f);
					p.IconV(br.x + 5.f, br.y, br.h, kB[i].ic, fg, 12.f);
					p.TextV(br.x + 21.f, br.y, br.h, kB[i].label, fg);
				}
				y += kRowH + 4.f;
			}

			// ── SOUS-MAILLAGES ──────────────────────────────────────────────────
			// Ce que les emplacements DECOUPENT. Un sous-maillage n'est pas un objet
			// separe : c'est un groupe de faces du meme maillage. Il n'y en a donc
			// aucun tant qu'aucun emplacement n'a ete assigne.
			if (DetailHeader(p, hit, r, y, st, NkDetailSubMesh, "Sous-maillages")) {
				p.TextV(r.x + kPad, y, kRowH, "Aucun", NkRole::TextMuted);
				y += kRowH;
			}

			p.Unclip();
			const NkRect area = clipR;
			hit.Add("det.list", area);
			hit.Wheel("det.list", st.scrollDetails, y - listTop + scroll, area.h);
			p.VScroll(area, y - listTop + scroll, scroll);
		}

		// ── NAVIGATEUR DE PROJET (bas) ──────────────────────────────────────────
		inline void PaintBrowser(NkModelerPainter &p, const NkRect &r, NkModelerState &st,
								 NkHitRegistry &hit, NkWidgetState &ws, const nkgui::NkGuiInput &in) {
			const float32 treeScroll = st.scrollTree;
			const float32 assetScroll = st.scrollAssets;
			p.Fill(r, NkRole::PanelBg);
			p.HLine(r.x, r.y, r.w);

			const float32 topH = 28.f;
			const float32 ih = p.IconSize();
			p.Fill({r.x, r.y, r.w, topH}, NkRole::PanelHeader);
			p.TextV(r.x + kPad, r.y, topH, "Navigateur de projet");
			float32 x = r.x + kPad + p.TextW("Navigateur de projet") + 10.f;
			{
				// Meme croix que les panneaux lateraux, meme effet : elle referme, et la
				// poignee du bas ramene le navigateur.
				const NkRect cr{x - 3.f, r.y + 4.f, 20.f, topH - 8.f};
				const bool over = hit.Add("brw.close", cr);
				if (over)
					p.Fill(cr, NkRole::AccentUi, 2.f);
				p.IconV(x, r.y, topH, NkIcon::WinClose, over ? NkRole::TextOnAccent : NkRole::Text,
						11.f);
				if (hit.Clicked("brw.close"))
					st.showBrowser = false;
			}
			x += 22.f;
			p.VLine(x, r.y + 6.f, topH - 12.f);
			x += 10.f;
			struct B {
					NkIcon ic;
					const char *label;
			};
			// TROIS BOUTONS DE CREATION DIRECTS, pas un menu : creer un dossier, un
			// materiau ou une texture sont les trois gestes fondateurs du navigateur,
			// et les cacher derriere un deroulant ajouterait un clic a chacun.
			// LE BLUEPRINT manquait. C'est le document de modelisation lui-meme --
			// une piece a modeler, avec sa geometrie, ses materiaux et ses reglages.
			// Materiau et texture sont des RESSOURCES qu'il consomme ; le blueprint
			// est ce qu'on ouvre pour travailler, donc il vient en premier.
			static const B kBtns[] = {{NkIcon::Mesh, "+ Blueprint"},
									  {NkIcon::Folder, "+ Dossier"},
									  {NkIcon::Circle, "+ Materiau"},
									  {NkIcon::Journal, "+ Texture"},
									  {NkIcon::Import, "Importer"}};
			for (int32 i = 0; i < 5; ++i) {
				const float32 bw = 18.f + p.TextW(kBtns[i].label) + 10.f;
				char bkey[24];
				snprintf(bkey, sizeof(bkey), "brw.new.%d", i);
				const NkRect br{x - 4.f, r.y + 3.f, bw, topH - 6.f};
				HoverFill(p, br, hit.Add(bkey, br), 2.f);
				static const NkRole kBtnRole[5] = {NkRole::TypeMesh, NkRole::TypeFolder,
												   NkRole::TypeMat, NkRole::TypeTex,
												   NkRole::Text};
				p.IconV(x, r.y, topH, kBtns[i].ic, kBtnRole[i], 13.f);
				p.TextV(x + 18.f, r.y, topH, kBtns[i].label);
				x += bw + 8.f;
				// La creation ecrit dans l'etat ; le nom par defaut est numerote et
				// se renomme au double-clic, comme partout.
				if (i < 4 && hit.Clicked(bkey) && st.browserCount < NkModelerState::kMaxBrowser) {
					const int32 k = st.browserCount++;
					// 0 blueprint, 1 dossier, 2 materiau, 3 texture -- l'ordre des
					// boutons EST celui des genres, il n'y a rien a traduire.
					st.browserKind[k] = (uint8)i;
					st.browserParent[k] = st.browserFolder;
					static const char *const kBase[] = {"BP", "Dossier", "Materiau", "Texture"};
					snprintf(st.browserNames[k], 32, "%s_%02d", kBase[i], k + 1);
				}
			}
			p.VLine(x, r.y + 6.f, topH - 12.f);
			x += 10.f;
			p.IconV(x, r.y, topH, NkIcon::ArrowLeft, NkRole::Text, 13.f);
			p.IconV(x + 22.f, r.y, topH, NkIcon::ArrowRight, NkRole::Text, 13.f);
			p.TextV(x + 50.f, r.y, topH, "Tout > Contenu > Perso", NkRole::TextMuted);
			p.HLine(r.x, r.y + topH - 1.f, r.w);

			// Arbre de dossiers : LES DOSSIERS CREES PAR L'UTILISATEUR, plus une
			// racine fixe. Ils structurent le PROJET et non le disque : ils seront
			// enregistres DANS le fichier de projet.
			const float32 treeW = r.w * 0.18f;
			const float32 ty = r.y + topH;
			const float32 th = r.h - topH;
			p.Clip({r.x, ty, r.w, th});
			p.Fill({r.x, ty, treeW, th}, NkRole::WindowBg);
			p.VLine(r.x + treeW, ty, th);
			int32 folderCount = 0;
			{
				float32 dy = ty + S(4.f) - treeScroll;
				char fkey[40];
				// Racine « Contenu ».
				{
					const NkRect rowR{r.x, dy, treeW, kRowH};
					const bool over = hit.Add("brow.root", rowR);
					const bool on = (st.browserFolder < 0);
					if (on)
						p.Fill(rowR, NkRole::AccentUi);
					else
						HoverFill(p, rowR, over, 0.f);
					p.IconV(r.x + S(6.f), dy, kRowH, NkIcon::Drawer,
							on ? NkRole::TextOnAccent : NkRole::Text, 13.f);
					p.TextV(r.x + S(24.f), dy, kRowH, "Contenu",
							on ? NkRole::TextOnAccent : NkRole::Text);
					if (hit.Clicked("brow.root"))
						st.browserFolder = -1;
					dy += kRowH;
				}
				for (int32 i = 0; i < st.browserCount; ++i) {
					if (st.browserKind[i] != 1)
						continue; // seuls les DOSSIERS vivent dans l'arbre
					++folderCount;
					const bool on = (st.browserFolder == i);
					const NkRect rowR{r.x, dy, treeW, kRowH};
					snprintf(fkey, sizeof(fkey), "brow.dir.%d", i);
					const bool over = hit.Add(fkey, rowR);
					if (on)
						p.Fill(rowR, NkRole::AccentUi);
					else
						HoverFill(p, rowR, over, 0.f);
					// LE DOSSIER RESTE ORANGE meme selectionne : c'est ce qui le
					// distingue d'un asset au premier coup d'oeil.
					p.IconV(r.x + S(18.f), dy, kRowH, on ? NkIcon::FolderOpen : NkIcon::Folder,
							NkRole::TypeFolder, 13.f);
					snprintf(fkey, sizeof(fkey), "brow.dirname.%d", i);
					EditableText(p, hit, ws, in, fkey,
								 {r.x + S(36.f), dy, treeW - S(42.f), kRowH}, st.browserNames[i],
								 on ? NkRole::TextOnAccent : NkRole::Text, st.browserNames[i], 32u);
					snprintf(fkey, sizeof(fkey), "brow.dir.%d", i);
					if (hit.Clicked(fkey))
						st.browserFolder = i;
					dy += kRowH;
				}
				if (folderCount == 0)
					p.TextV(r.x + S(8.f), dy, kRowH, "Aucun dossier", NkRole::TextMuted);
			}

			// ── CARTES : le contenu du dossier courant ──────────────────────────
			// Plus AUCUNE donnee simulee : chaque carte est un materiau ou une
			// texture cree par l'utilisateur, rattache au dossier ouvert. Le style
			// (ombre, damier, bande de type, pied a deux lignes) est celui valide
			// avec Rihen sur les cartes precedentes.
			const float32 ax = r.x + treeW + S(14.f);
			const float32 tw = 96.f;
			const float32 pvH = 96.f;
			const float32 barH2 = 3.f;
			const float32 footH = 34.f;
			const float32 cardH = pvH + barH2 + footH;
			float32 tx = ax;
			float32 tyy = ty + S(12.f) - assetScroll;
			int32 shown = 0;
			char akey[40];
			const float32 wrapW = r.x + r.w - S(16.f);
			for (int32 i = 0; i < st.browserCount; ++i) {
				if (st.browserKind[i] == 1 || st.browserParent[i] != st.browserFolder)
					continue; // les dossiers sont dans l'arbre, pas dans la grille
				++shown;
				if (tx + tw > wrapW) { // retour a la ligne
					tx = ax;
					tyy += cardH + S(14.f);
				}
				const uint8 kind = st.browserKind[i]; // 0 blueprint, 2 materiau, 3 texture
				const NkRole role = (kind == 0)   ? NkRole::TypeMesh
									: (kind == 2) ? NkRole::TypeMat
												  : NkRole::TypeTex;
				const char *kindName = (kind == 0)   ? "Blueprint"
									   : (kind == 2) ? "Materiau"
													 : "Texture";

				// Ombre portee legere, comme Unreal.
				p.Fill({tx + 2.f, tyy + 3.f, tw, cardH}, NkColor{0, 0, 0, 90}, 3.f);
				snprintf(akey, sizeof(akey), "brow.card.%d", i);
				const bool selCard = (st.selectedAsset == i);
				hit.Add(akey, {tx, tyy, tw, cardH});
				if (selCard)
					p.Fill({tx - 2.f, tyy - 2.f, tw + 4.f, cardH + 4.f}, NkRole::AccentUi, 3.f);

				// Damier de fond : il dit « ce fond est vide ».
				const float32 c = 8.f;
				p.Fill({tx, tyy, tw, pvH}, NkRole::InputBg);
				for (int32 gx = 0; gx * c < tw; ++gx)
					for (int32 gy = 0; gy * c < pvH; ++gy)
						if (((gx + gy) & 1) == 0) {
							const float32 cw = ((float32)(gx + 1) * c > tw) ? tw - (float32)gx * c : c;
							const float32 ch =
								((float32)(gy + 1) * c > pvH) ? pvH - (float32)gy * c : c;
							p.Fill({tx + (float32)gx * c, tyy + (float32)gy * c, cw, ch},
								   NkRole::WindowBg);
						}
				const float32 cx = tx + tw * 0.5f, cy = tyy + pvH * 0.5f;
				if (kind == 0) {
					// BLUEPRINT : un cube en volume -- c'est une piece a modeler.
					const float32 hw = 18.f, hh = 16.f, dp = 9.f;
					p.Fill({cx - hw, cy - hh + dp, hw * 2.f, hh * 2.f - dp}, NkRole::PanelHeader);
					p.OutlineSharp({cx - hw, cy - hh + dp, hw * 2.f, hh * 2.f - dp}, role);
					p.Line(cx - hw, cy - hh + dp, cx - hw + dp, cy - hh, role);
					p.Line(cx - hw + dp, cy - hh, cx + hw + dp, cy - hh, role);
					p.Line(cx + hw, cy - hh + dp, cx + hw + dp, cy - hh, role);
					p.Line(cx + hw + dp, cy - hh, cx + hw + dp, cy + hh - dp, role);
					p.Line(cx + hw, cy + hh, cx + hw + dp, cy + hh - dp, role);
				} else if (kind == 2) {
					// Boule de rendu avec reflet : sans reflet, le disque se lit
					// comme une pastille de couleur.
					p.Disc(cx, cy, 22.f, role);
					p.Disc(cx - 8.f, cy - 8.f, 5.f, NkRole::Text);
				} else {
					const float32 q = 6.f, half = q * 3.f;
					for (int32 gx = 0; gx < 6; ++gx)
						for (int32 gy = 0; gy < 6; ++gy)
							p.Fill({cx - half + (float32)gx * q, cy - half + (float32)gy * q, q, q},
								   ((gx + gy) & 1) ? role : NkRole::PanelHeader);
				}

				// Bande de type, puis nom et type DANS la carte, tronques.
				p.Fill({tx, tyy + pvH, tw, barH2}, role);
				p.Fill({tx, tyy + pvH + barH2, tw, footH}, NkRole::PanelHeader);
				{
					const float32 fy = tyy + pvH + barH2;
					const float32 lh = p.LineH();
					const float32 pad = 6.f;
					float32 fyy = fy + (footH - (lh * 2.f + 2.f)) * 0.5f;
					snprintf(akey, sizeof(akey), "brow.cardname.%d", i);
					EditableText(p, hit, ws, in, akey, {tx + pad, fyy, tw - pad * 2.f, lh + 2.f},
								 st.browserNames[i], NkRole::Text, st.browserNames[i], 32u);
					fyy += lh + 2.f;
					p.TextClipped(tx + pad, fyy, tw - pad * 2.f, kindName, NkRole::TextMuted);
				}
				snprintf(akey, sizeof(akey), "brow.card.%d", i);
				if (hit.Clicked(akey))
					st.selectedAsset = i;
				tx += tw + S(12.f);
			}
			if (shown == 0)
				p.TextV(ax, ty + S(12.f), kRowH,
						"Vide -- creez un dossier, un materiau ou une texture", NkRole::TextMuted);

			p.Unclip();
			const NkRect treeArea{r.x, ty, treeW, th};
			const NkRect assetArea{ax - S(10.f), ty + S(33.f), r.w - treeW - S(10.f), th - S(33.f)};
			hit.Add("brow.tree", treeArea);
			hit.Wheel("brow.tree", st.scrollTree, 5.f * kRowH + S(8.f), treeArea.h);
			hit.Add("brow.assets", assetArea);
			// La hauteur de contenu etait figee a 125 px, valeur de l'ancienne carte.
			// Les cartes font maintenant 133 px : le defilement s'arretait avant le bas
			// et la derniere rangee restait inaccessible.
			const float32 assetContentH = (tyy + cardH + S(14.f)) - ty + assetScroll;
			hit.Wheel("brow.assets", st.scrollAssets, assetContentH, assetArea.h);
			p.VScroll(treeArea, 5.f * kRowH + S(8.f), treeScroll);
			p.VScroll(assetArea, assetContentH, assetScroll);
			char cnt[32];
			snprintf(cnt, sizeof(cnt), "%d element(s)", shown);
			const float32 ew = p.TextW(cnt);
			p.TextV(r.x + r.w - ew - kPad, r.y + r.h - kRowH, kRowH, cnt, NkRole::TextMuted);
			(void)ih;
		}

		// ── CONTENU DES MENUS ───────────────────────────────────────────────────
		// Une TABLE plutot que du code : ajouter une entree ne demande pas de
		// toucher au rendu, et la meme table servira la palette de recherche et le
		// menu contextuel -- une liste ecrite deux fois finit par diverger.
		//
		// `command` est la CLE STABLE de NkShortcutTable quand l'entree en a une :
		// c'est elle qui permet d'afficher le raccourci sans le recopier. Vide pour
		// les entrees qui n'en ont pas encore.
		struct NkMenuItem {
				const char *label;   ///< nullptr = separateur
				const char *command; ///< cle NkShortcutTable, ou ""
				bool submenu;		 ///< ouvre un sous-menu
		};
		struct NkMenuDef {
				const char *title;
				const NkMenuItem *items;
				int32 count;
		};

		inline const NkMenuDef *NkMenus(int32 &outCount) {
			static const NkMenuItem kFile[] = {
				{"Nouveau", "", false},		{"Ouvrir...", "", false}, {"Ouvrir recent", "", true},
				{nullptr, "", false},		{"Enregistrer", "", false}, {"Enregistrer sous...", "", false},
				{nullptr, "", false},		{"Importer", "", true},   {"Exporter", "", true},
				{nullptr, "", false},		{"Quitter", "", false},
			};
			static const NkMenuItem kEdit[] = {
				{"Annuler", "app.annuler", false}, {"Refaire", "app.refaire", false},
				{nullptr, "", false},			   {"Dupliquer", "objet.dupliquer", false},
				{"Supprimer", "objet.supprimer", false}, {nullptr, "", false},
				{"Preferences...", "", false},
			};
			static const NkMenuItem kWindow[] = {
				{"Hierarchie", "", false},		{"Proprietes", "", false}, {"Details", "", false},
				{"Navigateur de projet", "", false}, {nullptr, "", false},
				{"Panneau d'outils", "app.panneau_outils", false}, {nullptr, "", false},
				{"Plein ecran", "", false},
			};
			static const NkMenuItem kTools[] = {
				{"Rechercher une commande", "app.palette", false}, {nullptr, "", false},
				{"Retopologier", "", false},   {"Decimer...", "", false},
				{nullptr, "", false},		   {"Extensions", "", true},
			};
			static const NkMenuItem kSelect[] = {
				{"Tout selectionner", "", false}, {"Tout deselectionner", "", false},
				{"Inverser", "", false},		  {nullptr, "", false},
				{"Par type", "", true},
			};
			static const NkMenuItem kObject[] = {
				{"Deplacer", "objet.deplacer", false}, {"Tourner", "objet.tourner", false},
				{"Redimensionner", "objet.echelle", false}, {nullptr, "", false},
				{"Ajouter un modificateur", "", true}, {"Appliquer tout", "", false},
			};
			static const NkMenuItem kHelp[] = {
				{"Documentation", "", false}, {"Raccourcis clavier", "", false},
				{nullptr, "", false},		  {"A propos", "", false},
			};
			static const NkMenuDef kMenus[] = {
				{"Fichier", kFile, 11},	  {"Edition", kEdit, 7},	{"Fenetre", kWindow, 8},
				{"Outils", kTools, 6},	  {"Selection", kSelect, 5}, {"Objet", kObject, 6},
				{"Aide", kHelp, 4},
			};
			outCount = 7;
			return kMenus;
		}

		// ── DEROULEMENT D'UN MENU ───────────────────────────────────────────────
		// Peint APRES tout le reste : un menu doit recouvrir les panneaux, et le
		// registre de zones donne la priorite a ce qui est declare en dernier.
		inline void PaintOpenMenu(NkModelerPainter &p, const NkRect &bar, NkModelerState &st,
								  NkHitRegistry &hit, const NkShortcutTable &sc) {
			if (st.openMenu < 0)
				return;
			int32 nMenus = 0;
			const NkMenuDef *menus = NkMenus(nMenus);
			if (st.openMenu >= nMenus)
				return;
			const NkMenuDef &m = menus[st.openMenu];

			// Position : sous l'entree de barre correspondante. On recalcule les
			// largeurs comme la barre pour retomber exactement dessous -- une
			// constante recopiee se decalerait a la premiere traduction.
			float32 x = S(10.f) + S(24.f) + S(16.f);
			for (int32 i = 0; i < st.openMenu; ++i)
				x += p.TextW(menus[i].title) + S(18.f);
			x -= S(9.f);

			const float32 itemH = S(24.f), sepH = S(7.f);
			float32 w = S(150.f);
			char keys[32];
			for (int32 i = 0; i < m.count; ++i) {
				if (!m.items[i].label)
					continue;
				float32 need = p.TextW(m.items[i].label) + S(70.f);
				if (m.items[i].command && *m.items[i].command
					&& sc.FormatFor(m.items[i].command, keys, sizeof(keys)))
					need += p.TextW(keys);
				if (need > w)
					w = need;
			}
			float32 h = S(8.f);
			for (int32 i = 0; i < m.count; ++i)
				h += m.items[i].label ? itemH : sepH;

			const NkRect box{x, bar.y + bar.h, w, h};
			p.Fill({box.x + 2.f, box.y + 2.f, box.w, box.h}, NkRole::WindowBg, 4.f); // ombre portee
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 4.f);
			hit.Add("menu.panel", box); // avale les clics qui tombent dans le menu

			float32 y = box.y + S(4.f);
			for (int32 i = 0; i < m.count; ++i) {
				if (!m.items[i].label) {
					// Separateur : il ne s'etend PAS bord a bord, il est retrait de la
					// marge du texte -- sinon il coupe le menu en deux blocs qui ont
					// l'air sans rapport.
					p.HLine(box.x + S(8.f), y + sepH * 0.5f, box.w - S(16.f));
					y += sepH;
					continue;
				}
				const NkRect ir{box.x + 2.f, y, box.w - 4.f, itemH};
				snprintf(keys, sizeof(keys), "menu.item.%d", i);
				const bool over = hit.Add(keys, ir);
				if (over)
					p.Fill(ir, NkRole::AccentUi, 3.f);
				p.TextV(ir.x + S(12.f), y, itemH, m.items[i].label,
						over ? NkRole::TextOnAccent : NkRole::Text);
				// Le RACCOURCI est lu dans la table, jamais recopie : rebinder une
				// touche met l'affichage a jour tout seul.
				if (m.items[i].command && *m.items[i].command
					&& sc.FormatFor(m.items[i].command, keys, sizeof(keys))) {
					const float32 kw = p.TextW(keys);
					p.TextV(ir.x + ir.w - kw - S(12.f), y, itemH, keys,
							over ? NkRole::TextOnAccent : NkRole::TextMuted);
				}
				if (m.items[i].submenu)
					p.IconV(ir.x + ir.w - S(18.f), y, itemH, NkIcon::ChevronRight,
							over ? NkRole::TextOnAccent : NkRole::TextMuted, 11.f);
				snprintf(keys, sizeof(keys), "menu.item.%d", i);
				if (hit.Clicked(keys) && !m.items[i].submenu) {
					// Une entree SANS sous-menu referme le menu. Une entree AVEC en
					// ouvrirait un second -- non ecrit tant qu'aucune n'a de contenu
					// reel, plutot qu'un panneau vide qui ferait croire a un bug.
					if (st.openMenu == 0 && i == 10) // Fichier > Quitter
						st.askClose = true;
					st.openMenu = -1;
				}
				y += itemH;
			}

			// UN CLIC AILLEURS REFERME. Teste en dernier, apres que toutes les zones
			// du menu ont ete declarees : sans cet ordre, le clic sur une entree
			// refermerait aussi le menu avant d'etre traite.
			if (hit.AnyClick() && !hit.IsHovered("menu.panel")) {
				bool onItem = false;
				for (int32 i = 0; i < m.count && !onItem; ++i) {
					snprintf(keys, sizeof(keys), "menu.item.%d", i);
					onItem = hit.IsHovered(keys);
				}
				bool onBar = false;
				for (int32 i = 0; i < nMenus && !onBar; ++i) {
					snprintf(keys, sizeof(keys), "menu.%d", i);
					onBar = hit.IsHovered(keys);
				}
				if (!onItem && !onBar)
					st.openMenu = -1;
			}
		}

		// ── LISTE DES MODIFICATEURS, A DEUX NIVEAUX ─────────────────────────────
		// Peinte APRES tout le reste : elle doit recouvrir les panneaux, et le
		// registre donne la priorite a la derniere zone declaree.
		//
		// LA CATEGORIE S'OUVRE AU SURVOL et non au clic. Un clic serait un geste de
		// plus pour atteindre une entree qui, elle, en demande deja un -- et rien ne
		// justifie de valider le fait de « regarder » une categorie.
		inline void PaintModifierMenu(NkModelerPainter &p, NkModelerState &st, NkHitRegistry &hit,
									  NkWidgetState &ws, float32 W, float32 H) {
			if (!ws.ComboOpen("tb.mod"))
				return;
			int32 nc = 0;
			const NkModCategory *cats = NkModifierCategories(nc);
			const NkRect &a = st.modAnchor;
			const float32 itemH = S(24.f);
			const float32 catW = S(150.f);
			// LE MENU RESTE DANS LA FENETRE. Il etait pose sous son ancre sans aucune
			// verification : avec neuf categories et un sous-menu de huit entrees, le
			// bas du panneau sortait par le bas de l'ecran et les dernieres entrees
			// etaient inatteignables. On remonte le panneau plutot que de le faire
			// defiler -- un menu qui defile demande deux gestes la ou un seul suffit.
			float32 boxY = a.y + a.h + 2.f;
			const float32 boxH = itemH * (float32)nc + S(6.f);
			if (boxY + boxH > H - S(4.f))
				boxY = H - S(4.f) - boxH;
			if (boxY < S(4.f))
				boxY = S(4.f);
			float32 boxX = a.x;
			if (boxX + catW > W - S(4.f))
				boxX = W - S(4.f) - catW;
			if (boxX < S(4.f))
				boxX = S(4.f);
			const NkRect box{boxX, boxY, catW, boxH};
			p.Fill({box.x + 2.f, box.y + 2.f, box.w, box.h}, NkRole::WindowBg, 4.f);
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 4.f);
			hit.Add("mod.panel", box);

			char key[32];
			int32 flatBase = 0;
			for (int32 c = 0; c < nc; ++c) {
				const NkRect ir{box.x + 2.f, box.y + S(3.f) + (float32)c * itemH, box.w - 4.f, itemH};
				snprintf(key, sizeof(key), "mod.cat.%d", c);
				const bool over = hit.Add(key, ir);
				if (over)
					st.modOpenCat = c; // survol = ouverture, sans clic
				const bool active = (st.modOpenCat == c);
				if (active)
					p.Fill(ir, NkRole::AccentUi, 3.f);
				p.IconV(ir.x + S(10.f), ir.y, itemH, cats[c].icon,
						active ? NkRole::TextOnAccent : NkRole::Text, 13.f);
				p.TextV(ir.x + S(29.f), ir.y, itemH, cats[c].name,
						active ? NkRole::TextOnAccent : NkRole::Text);
				p.IconV(ir.x + ir.w - S(18.f), ir.y, itemH, NkIcon::ChevronRight,
						active ? NkRole::TextOnAccent : NkRole::TextMuted, 11.f);

				if (active) {
					// Le sous-menu s'ouvre A DROITE, aligne sur SA categorie : aligne sur
					// le haut du panneau, il faudrait chercher a quelle categorie il se
					// rapporte des qu'on en survole une du bas.
					const float32 subW = S(170.f);
					const float32 subH = itemH * (float32)cats[c].count + S(6.f);
					// Le sous-menu subit la meme contrainte, et bascule A GAUCHE du
					// panneau s'il n'y a plus la place a droite.
					float32 subX = box.x + box.w + 3.f;
					if (subX + subW > W - S(4.f))
						subX = box.x - subW - 3.f;
					if (subX < S(4.f))
						subX = S(4.f);
					float32 subY = ir.y - S(3.f);
					if (subY + subH > H - S(4.f))
						subY = H - S(4.f) - subH;
					if (subY < S(4.f))
						subY = S(4.f);
					const NkRect sub{subX, subY, subW, subH};
					p.Fill({sub.x + 2.f, sub.y + 2.f, sub.w, sub.h}, NkRole::WindowBg, 4.f);
					p.Outline(sub, NkRole::Border, NkRole::PanelHeader, 4.f);
					hit.Add("mod.sub", sub);
					for (int32 i = 0; i < cats[c].count; ++i) {
						const NkRect er{sub.x + 2.f, sub.y + S(3.f) + (float32)i * itemH, sub.w - 4.f,
										itemH};
						snprintf(key, sizeof(key), "mod.item.%d.%d", c, i);
						const bool o2 = hit.Add(key, er);
						const bool cur = (st.modKind == flatBase + i);
						if (o2)
							p.Fill(er, NkRole::AccentUi, 3.f);
						p.IconV(er.x + S(10.f), er.y, itemH, cats[c].items[i].icon,
								o2 ? NkRole::TextOnAccent : NkRole::Text, 13.f);
						p.TextV(er.x + S(29.f), er.y, itemH, cats[c].items[i].label,
								o2 ? NkRole::TextOnAccent : NkRole::Text);
						// AUCUNE COCHE ICI. Une coche dit « ceci est l'option retenue » ;
						// or ce menu ne retient rien, il AJOUTE. Le modificateur ajoute
						// se lit dans le panneau Details, la ou il vit.
						(void)cur;
						if (hit.Clicked(key)) {
							st.modKind = flatBase + i;
							// Chaque entree porte SON type moteur : le menu ne calcule
							// aucun indice, donc rien ne diverge quand une categorie
							// gagne une entree.
							nk3d::Viewport3DAddModifier(cats[c].items[i].type);
							st.dirty = true;
							ws.CloseCombo();
						}
					}
				}
				flatBase += cats[c].count;
			}

			// Un clic HORS des deux panneaux referme. Teste apres toutes les zones :
			// sinon un clic sur une entree refermerait avant d'etre traite.
			if (hit.AnyClick() && !hit.IsHovered("mod.panel") && !hit.IsHovered("mod.sub")
				&& !hit.IsHovered("tb.mod")) {
				bool inside = false;
				for (int32 c = 0; c < nc && !inside; ++c) {
					snprintf(key, sizeof(key), "mod.cat.%d", c);
					inside = hit.IsHovered(key);
					for (int32 i = 0; i < cats[c].count && !inside; ++i) {
						snprintf(key, sizeof(key), "mod.item.%d.%d", c, i);
						inside = hit.IsHovered(key);
					}
				}
				if (!inside)
					ws.CloseCombo();
			}
		}

		// ── MENU « AJOUTER », DEUX NIVEAUX ──────────────────────────────────────
		// Meme mecanique que le menu des modificateurs : survol d'une categorie =
		// ouverture de son sous-menu, clic sur une entree = creation + fermeture.
		inline void PaintAddObjectMenu(NkModelerPainter &p, NkModelerState &st, NkHitRegistry &hit,
									   NkWidgetState &ws, float32 W, float32 H) {
			if (!ws.ComboOpen("tb.addmenu"))
				return;
			int32 nc = 0;
			const NkAddCategory *cats = NkAddCategories(nc);
			const NkRect &a = st.addAnchor;
			const float32 itemH = S(24.f);
			const float32 catW = S(140.f);
			float32 boxY = a.y + a.h + 2.f;
			const float32 boxH = itemH * (float32)nc + S(6.f);
			if (boxY + boxH > H - S(4.f))
				boxY = H - S(4.f) - boxH;
			float32 boxX = a.x;
			if (boxX + catW > W - S(4.f))
				boxX = W - S(4.f) - catW;
			const NkRect box{boxX, boxY, catW, boxH};
			p.Fill({box.x + 2.f, box.y + 2.f, box.w, box.h}, NkRole::WindowBg, 4.f);
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 4.f);
			hit.Add("addm.panel", box);

			char key[32];
			for (int32 c = 0; c < nc; ++c) {
				const NkRect ir{box.x + 2.f, box.y + S(3.f) + (float32)c * itemH, box.w - 4.f, itemH};
				snprintf(key, sizeof(key), "addm.cat.%d", c);
				const bool over = hit.Add(key, ir);
				if (over)
					st.addOpenCat = c; // survol = ouverture, sans clic
				const bool active = (st.addOpenCat == c);
				if (active)
					p.Fill(ir, NkRole::AccentUi, 3.f);
				p.IconV(ir.x + S(10.f), ir.y, itemH, cats[c].icon,
						active ? NkRole::TextOnAccent : NkRole::Text, 13.f);
				p.TextV(ir.x + S(29.f), ir.y, itemH, cats[c].name,
						active ? NkRole::TextOnAccent : NkRole::Text);
				p.IconV(ir.x + ir.w - S(18.f), ir.y, itemH, NkIcon::ChevronRight,
						active ? NkRole::TextOnAccent : NkRole::TextMuted, 11.f);

				if (active) {
					const float32 subW = S(150.f);
					const float32 subH = itemH * (float32)cats[c].count + S(6.f);
					float32 subX = box.x + box.w + 3.f;
					if (subX + subW > W - S(4.f))
						subX = box.x - subW - 3.f;
					float32 subY = ir.y - S(3.f);
					if (subY + subH > H - S(4.f))
						subY = H - S(4.f) - subH;
					const NkRect sub{subX, subY, subW, subH};
					p.Fill({sub.x + 2.f, sub.y + 2.f, sub.w, sub.h}, NkRole::WindowBg, 4.f);
					p.Outline(sub, NkRole::Border, NkRole::PanelHeader, 4.f);
					hit.Add("addm.sub", sub);
					for (int32 i = 0; i < cats[c].count; ++i) {
						const NkRect er{sub.x + 2.f, sub.y + S(3.f) + (float32)i * itemH,
										sub.w - 4.f, itemH};
						snprintf(key, sizeof(key), "addm.item.%d.%d", c, i);
						const bool o2 = hit.Add(key, er);
						if (o2)
							p.Fill(er, NkRole::AccentUi, 3.f);
						p.IconV(er.x + S(10.f), er.y, itemH, cats[c].items[i].icon,
								o2 ? NkRole::TextOnAccent : NkRole::Text, 13.f);
						p.TextV(er.x + S(29.f), er.y, itemH, cats[c].items[i].label,
								o2 ? NkRole::TextOnAccent : NkRole::Text);
						if (hit.Clicked(key)) {
							nk3d::Viewport3DAddObject(cats[c].items[i].type, cats[c].items[i].prim);
							st.dirty = true;
							ws.CloseCombo();
						}
					}
				}
			}

			// Un clic HORS des deux panneaux referme.
			if (hit.AnyClick() && !hit.IsHovered("addm.panel") && !hit.IsHovered("addm.sub") &&
				!hit.IsHovered("tb.addmenu")) {
				bool inside = false;
				for (int32 c = 0; c < nc && !inside; ++c) {
					snprintf(key, sizeof(key), "addm.cat.%d", c);
					inside = hit.IsHovered(key);
					for (int32 i = 0; i < cats[c].count && !inside; ++i) {
						snprintf(key, sizeof(key), "addm.item.%d.%d", c, i);
						inside = hit.IsHovered(key);
					}
				}
				if (!inside)
					ws.CloseCombo();
			}
		}

		// ── SEPARATEURS GLISSABLES ──────────────────────────────────────────────
		// Ils modifient des FRACTIONS et non des pixels : la disposition se retrouve
		// identique a la reouverture quelle que soit la taille de fenetre.
		//
		// La zone sensible est PLUS LARGE que le trait dessine (6 px contre 1) :
		// viser un trait d'un pixel est un exercice d'adresse, pas une interaction.
		// C'est ce que font tous les gestionnaires de fenetres.
		inline void PaintSplitters(NkModelerPainter &p, const NkLayout &lay, float32 W, float32 H,
								   NkModelerState &st, NkHitRegistry &hit) {
			const float32 grab = S(6.f);
			struct Sp {
					const char *key;
					NkRect zone;
					bool vertical; ///< true = trait vertical, on glisse en X
					float32 *frac;
					float32 span; ///< dimension de reference pour convertir px -> fraction
					float32 sign; ///< sens : +1 si la fraction croit avec la position
			};
			const Sp sps[4] = {
				{"split.left", {lay.left.x + lay.left.w - grab * 0.5f, lay.left.y, grab, lay.left.h},
				 true, &st.leftFrac, W, 1.f},
				{"split.right", {lay.right.x - grab * 0.5f, lay.right.y, grab, lay.right.h}, true,
				 &st.rightFrac, W, -1.f},
				{"split.browser", {0.f, lay.browser.y - grab * 0.5f, W, grab}, false, &st.browserFrac, H,
				 -1.f},
				{"split.props",
				 {lay.propsR.x, lay.propsR.y + lay.propsR.h - grab * 0.5f, lay.propsR.w, grab}, false,
				 &st.propsFrac, lay.right.h, 1.f},
			};

			// Un separateur borde un panneau : si le panneau est masque, il n'y a rien
			// a redimensionner. Le laisser actif donnerait un trait invisible qui
			// change le curseur et modifie une fraction qu'on ne voit pas.
			const bool alive[4] = {st.showLeft, st.showRight, st.showBrowser, st.showRight};
			for (int32 i = 0; i < 4; ++i) {
				if (!alive[i])
					continue;
				const bool over = hit.Add(sps[i].key, sps[i].zone);
				const bool active = (st.dragSplitter == i);
				if (over || active) {
					hit.WantCursor(sps[i].vertical ? NkCursorWant::ResizeWE : NkCursorWant::ResizeNS);
					// Le trait s'ECLAIRE au survol : sans retour visuel, on ne sait pas
					// qu'on a attrape le bon endroit avant d'avoir deja tire.
					p.Fill(sps[i].zone, active ? NkRole::AccentUi : NkRole::Border);
				}
				if (hit.Clicked(sps[i].key)) {
					st.dragSplitter = i;
					st.dragStart = sps[i].vertical ? hit.Mouse().x : hit.Mouse().y;
					st.dragStartFrac = *sps[i].frac;
				}
			}

			// Le glissement se poursuit MEME SI la souris quitte la zone : c'est le
			// bouton enfonce qui commande, pas la position. Sans cela, un glissement
			// rapide lacherait le separateur en pleine course.
			if (st.dragSplitter >= 0) {
				if (!hit.MouseDown()) {
					st.dragSplitter = -1;
				} else {
					const Sp &sp = sps[st.dragSplitter];
					const float32 now = sp.vertical ? hit.Mouse().x : hit.Mouse().y;
					float32 f = st.dragStartFrac + sp.sign * (now - st.dragStart) / sp.span;
					if (f < 0.08f)
						f = 0.08f;
					if (f > 0.60f)
						f = 0.60f;
					*sp.frac = f;
				}
			}
		}

		// ── CONFIRMATION DE FERMETURE ───────────────────────────────────────────
		// N'apparait QUE si le document a des modifications non enregistrees.
		// Demander confirmation pour un document propre serait une friction inutile,
		// et l'utilisateur finirait par valider sans lire -- ce qui rend la question
		// dangereuse le jour ou elle compte vraiment.
		inline void PaintCloseDialog(NkModelerPainter &p, float32 W, float32 H, NkModelerState &st,
									 NkHitRegistry &hit) {
			if (!st.askClose)
				return;
			p.Fill({0.f, 0.f, W, H}, NkColor{0, 0, 0, 150}); // voile
			hit.Add("dlg.veil", {0.f, 0.f, W, H});

			const float32 bw = S(420.f), bh = S(150.f);
			const NkRect box{(W - bw) * 0.5f, (H - bh) * 0.5f, bw, bh};
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 6.f);
			hit.Add("dlg.box", box);
			p.TextV(box.x + S(20.f), box.y + S(14.f), S(24.f), "Quitter NK3DModeler ?");
			p.TextV(box.x + S(20.f), box.y + S(44.f), S(22.f),
					"La scene a des modifications non enregistrees.", NkRole::TextMuted);

			struct B {
					const char *key;
					const char *label;
					bool primary;
			};
			// L'ordre compte : l'action la plus SURE est a droite, sous la main, et
			// c'est elle qui porte l'accent. « Quitter sans enregistrer » reste
			// atteignable mais ne se propose pas.
			const B kBtns[3] = {{"dlg.cancel", "Annuler", false},
								{"dlg.discard", "Quitter sans enregistrer", false},
								{"dlg.save", "Enregistrer et quitter", true}};
			float32 bx = box.x + box.w - S(14.f);
			for (int32 i = 2; i >= 0; --i) {
				const float32 w = p.TextW(kBtns[i].label) + S(24.f);
				const NkRect br{bx - w, box.y + bh - S(44.f), w, S(28.f)};
				const bool over = hit.Add(kBtns[i].key, br);
				if (kBtns[i].primary)
					p.Fill(br, over ? NkRole::AccentSel : NkRole::AccentUi, 4.f);
				else
					p.Outline(br, NkRole::Border, over ? NkRole::PanelBg : NkRole::PanelHeader, 4.f);
				const float32 lw = p.TextW(kBtns[i].label);
				p.TextV(br.x + (w - lw) * 0.5f, br.y, br.h, kBtns[i].label,
						kBtns[i].primary ? NkRole::TextOnAccent : NkRole::Text);
				bx -= w + S(10.f);
			}

			if (hit.Clicked("dlg.cancel"))
				st.askClose = false;
			if (hit.Clicked("dlg.discard"))
				st.running = false;
			if (hit.Clicked("dlg.save")) {
				// L'enregistrement reel viendra avec le format de projet. On marque le
				// document propre pour que le chemin soit deja juste.
				st.dirty = false;
				st.running = false;
			}
		}

		// ── BARRE D'ETAT ────────────────────────────────────────────────────────
		inline void PaintStatus(NkModelerPainter &p, const NkRect &r, const NkModelerState &st) {
			char stats[128];
			if (st.mode == NkMode::Object)
				snprintf(stats, sizeof(stats), "Objets 6 - selectionne : %s - 60 ips",
						 st.selectedObject == 1 ? "Cube" : "-");
			else
				snprintf(stats, sizeof(stats), "Sommets 8 - Aretes 12 - Faces 6 - %s - 60 ips",
						 NkModeName(st.mode));
			p.Fill(r, NkRole::PanelHeader);
			p.HLine(r.x, r.y, r.w);
			float32 x = r.x + kPad;
			struct B {
					NkIcon ic;
					const char *label;
			};
			static const B kBtns[] = {{NkIcon::Drawer, "Tiroir"}, {NkIcon::Journal, "Journal"}};
			for (int32 i = 0; i < 2; ++i) {
				p.IconV(x, r.y, r.h, kBtns[i].ic, NkRole::Text, 13.f);
				p.TextV(x + 18.f, r.y, r.h, kBtns[i].label);
				x += 18.f + p.TextW(kBtns[i].label) + 16.f;
			}
			p.VLine(x, r.y + 6.f, r.h - 12.f);
			x += 10.f;
			p.Fill({x, r.y + (r.h - 20.f) * 0.5f, 240.f, 20.f}, NkRole::InputBg, 2.f);
			p.IconV(x + 6.f, r.y, r.h, NkIcon::Terminal, NkRole::Text, 12.f);
			p.TextV(x + 24.f, r.y, r.h, "Entrer une commande", NkRole::TextMuted);
			const float32 w = p.TextW(stats);
			p.TextV(r.x + r.w - w - kPad, r.y, r.h, stats, NkRole::TextMuted);
		}

	} // namespace nk3d
} // namespace nkentseu
