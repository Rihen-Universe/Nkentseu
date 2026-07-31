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
		inline const char *const *NkAddItems(int32 &n) {
			static const char *const k[] = {"Cube",	   "Sphere", "Cylindre", "Cone",
											"Plan",	   "Tore",	 "Texte",	 "Lumiere",
											"Camera",  "Vide"};
			n = 10;
			return k;
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
		struct NkModEntry {
				const char *label;
				NkIcon icon;
		};
		struct NkModCategory {
				const char *name;
				NkIcon icon;
				const NkModEntry *items;
				int32 count;
		};

		inline const NkModCategory *NkModifierCategories(int32 &n) {
			static const NkModEntry kGen[] = {
				{"Miroir", NkIcon::Ruler},		 {"Tableau", NkIcon::SnapGrid},
				{"Subdivision", NkIcon::Layers}, {"Biseau", NkIcon::Scale},
				{"Solidifier", NkIcon::Mesh}, {"Vissage", NkIcon::Rotate},
				{"Booleen", NkIcon::Overlay},	 {"Enveloppe", NkIcon::Mesh},
				{"Remailler", NkIcon::SnapGrid},
			};
			static const NkModEntry kDef[] = {
				{"Deformer", NkIcon::Move},		{"Courbe", NkIcon::Ruler},
				{"Simple", NkIcon::Scale},		{"Lisser", NkIcon::Circle},
			};
			static const NkModEntry kClean[] = {
				{"Decimer", NkIcon::Trash},		 {"Fondre", NkIcon::Refresh},
				{"Ombrage doux", NkIcon::Light},
			};
			static const NkModCategory kCats[] = {
				{"Generer", NkIcon::Add, kGen, 9},
				{"Deformer", NkIcon::Move, kDef, 4},
				{"Nettoyer", NkIcon::Trash, kClean, 3},
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
				int32 n = 0;
				const char *const *items = NkAddItems(n);
				// L'icone reste le « + » pour toutes les entrees : ce qui compte est
				// l'ACTION (ajouter), pas le dernier element ajoute.
				static NkIcon kAddIc[16];
				for (int32 i = 0; i < n && i < 16; ++i)
					kAddIc[i] = NkIcon::Add;
				Combo(p, hit, ws, "tb.add", {x, cbY, S(118.f), cbH}, items, kAddIc, n, st.addKind, combo);
				x += S(126.f);
			}
			// MODIFICATEUR : liste a DEUX NIVEAUX. Le bouton montre le modificateur
			// courant avec SON icone ; le clic ouvre les categories, et chaque
			// categorie ouvre ses entrees.
			{
				const NkModEntry *cur = NkModifierAt(st.modKind);
				const NkRect mr{x, cbY, S(136.f), cbH};
				const bool over = hit.Add("tb.mod", mr);
				const bool open = ws.ComboOpen("tb.mod");
				p.Outline(mr, (over || open) ? NkRole::AccentUi : NkRole::Border, NkRole::InputBg, 3.f);
				p.IconV(x + S(8.f), cbY, cbH, cur->icon, NkRole::AccentUi, 13.f);
				p.TextV(x + S(27.f), cbY, cbH, cur->label);
				p.IconV(x + mr.w - S(18.f), cbY, cbH, open ? NkIcon::ChevronUp : NkIcon::ChevronDown,
						NkRole::Text, 11.f);
				if (hit.Clicked("tb.mod"))
					ws.ToggleCombo("tb.mod");
				st.modAnchor = mr; // memorise pour la liste, peinte apres tout le reste
				x += S(144.f);
			}

			btn("tb.add", NkIcon::Add, "Ajouter");
			btn("tb.mod", NkIcon::Layers, "Modificateur");

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
		inline float32 PaintSearch(NkModelerPainter &p, const NkRect &r, float32 y) {
			const float32 h = 22.f;
			p.Fill({r.x + 6.f, y + 4.f, r.w - 12.f, h}, NkRole::InputBg, 2.f);
			p.IconV(r.x + 12.f, y + 4.f, h, NkIcon::Search, NkRole::Text, 12.f);
			p.TextV(r.x + 30.f, y + 4.f, h, "Rechercher", NkRole::TextMuted);
			return y + h + 8.f;
		}

		// ── HIERARCHIE (gauche) ─────────────────────────────────────────────────
		// Arbre REPLIABLE, noms MODIFIABLES, deux colonnes d'etat (oeil, cadenas), et
		// un clic dans le VIDE qui deselectionne.
		//
		// Ce dernier point compte plus qu'il n'en a l'air : sans lui, une fois un
		// objet selectionne on ne peut plus revenir a « rien de selectionne » sans
		// passer par un menu. Or « rien » est un etat legitime -- c'est celui ou les
		// commandes de scene s'appliquent.
		inline void PaintHierarchy(NkModelerPainter &p, const NkRect &r, NkModelerState &st,
								   NkHitRegistry &hit, NkWidgetState &ws, const nkgui::NkGuiInput &in) {
			p.Fill(r, NkRole::PanelBg);
			p.VLine(r.x + r.w - 1.f, r.y, r.h);
			float32 y = PaintPanelTab(p, r, "Hierarchie", &hit, &st.showLeft, "hier.close");
			y = PaintSearch(p, r, y);

			const float32 colEye = r.x + r.w - S(74.f);
			const float32 colLock = r.x + r.w - S(52.f);
			const float32 colType = r.x + r.w - S(132.f);

			p.Fill({r.x, y, r.w, kRowH}, NkRole::WindowBg);
			p.TextV(r.x + S(34.f), y, kRowH, "Nom");
			p.TextV(colType, y, kRowH, "Type", NkRole::TextMuted);
			p.HLine(r.x, y + kRowH - 1.f, r.w);
			y += kRowH;

			struct Row {
					int32 depth;
					int32 parent; ///< -1 = racine ; sert au repli
					const char *type;
					NkIcon icon;
					bool folder;
			};
			// `parent` remplace un simple niveau d'indentation : replier un groupe doit
			// cacher SES descendants, et l'indentation seule ne dit pas qui appartient
			// a qui des qu'il y a deux groupes voisins.
			static const Row kRows[] = {
				{0, -1, "", NkIcon::Globe, false},		 {1, 0, "Maillage", NkIcon::Mesh, false},
				{1, 0, "Maillage", NkIcon::Circle, false}, {1, 0, "Dossier", NkIcon::Folder, true},
				{2, 3, "Maillage", NkIcon::Mesh, false},   {2, 3, "Maillage", NkIcon::Mesh, false},
			};
			const int32 nRows = 6;
			const float32 listTop = y;
			const float32 listH = r.y + r.h - kRowH - listTop;
			const NkRect listR{r.x, listTop, r.w, listH};
			// La zone de LISTE est declaree en PREMIER : les lignes, declarees ensuite,
			// la recouvrent. Ce qui reste attribue a la liste, c'est donc exactement le
			// VIDE -- et c'est la qu'un clic deselectionne.
			hit.Add("hier.list", listR);

			// Le test de visibilite ci-dessous ecarte les lignes hors champ, mais la
			// ligne a cheval sur le bord reste peinte en entier : c'est elle qui
			// depasse sur le pied de panneau. La decoupe s'en charge.
			p.Clip(listR);
			int32 visibleCount = 0;
			float32 yy = y - st.scrollHier;
			char key[40];
			for (int32 i = 0; i < nRows; ++i) {
				// Cache si un ANCETRE est replie.
				bool hidden = false;
				for (int32 par = kRows[i].parent; par >= 0; par = kRows[par].parent)
					if (!st.folderOpen[par]) {
						hidden = true;
						break;
					}
				if (hidden)
					continue;
				visibleCount++;

				const NkRect rowR{r.x, yy, r.w, kRowH};
				if (yy >= listTop - kRowH && yy < listTop + listH) {
					snprintf(key, sizeof(key), "hier.row.%d", i);
					const bool over = hit.Add(key, rowR);
					const bool sel = (i == st.selectedObject);
					if (sel)
						p.Fill(rowR, NkRole::AccentUi);
					else
						HoverFill(p, rowR, over, 0.f);
					const NkRole fg = sel ? NkRole::TextOnAccent : NkRole::Text;
					const NkRole dim = sel ? NkRole::TextOnAccent : NkRole::TextMuted;
					float32 tx = r.x + S(6.f) + (float32)kRows[i].depth * S(14.f);

					// CHEVRON : il replie vraiment. Il n'apparait que si la ligne a des
					// enfants -- en mettre un partout ferait croire que tout se deplie.
					bool hasChild = false;
					for (int32 k = 0; k < nRows && !hasChild; ++k)
						hasChild = (kRows[k].parent == i);
					if (hasChild) {
						const NkRect cr{tx - S(2.f), yy, S(16.f), kRowH};
						snprintf(key, sizeof(key), "hier.chev.%d", i);
						hit.Add(key, cr);
						p.IconV(tx, yy, kRowH,
								st.folderOpen[i] ? NkIcon::ChevronDown : NkIcon::ChevronRight, dim, 12.f);
						if (hit.Clicked(key))
							st.folderOpen[i] = !st.folderOpen[i];
					}
					tx += S(16.f);

					// Le DOSSIER garde sa couleur propre meme selectionne : c'est un
					// contenant, et c'est ce qui le distingue au premier coup d'oeil.
					p.IconV(tx, yy, kRowH, kRows[i].icon,
							kRows[i].folder ? NkRole::TypeFolder : fg, 13.f);

					// NOM MODIFIABLE au double-clic.
					snprintf(key, sizeof(key), "hier.name.%d", i);
					EditableText(p, hit, ws, in, key,
								 {tx + S(18.f), yy, colType - tx - S(24.f), kRowH}, st.objectNames[i], fg,
								 st.objectNames[i], 32u);
					if (*kRows[i].type)
						p.TextV(colType, yy, kRowH, kRows[i].type, dim);

					snprintf(key, sizeof(key), "hier.eye.%d", i);
					const NkRect eyeR{colEye - S(3.f), yy, S(20.f), kRowH};
					HoverFill(p, eyeR, hit.Add(key, eyeR) && !sel, 2.f);
					p.IconV(colEye, yy, kRowH, st.visible[i] ? NkIcon::Eye : NkIcon::EyeClosed,
							st.visible[i] ? fg : dim, 13.f);
					if (hit.Clicked(key))
						st.visible[i] = !st.visible[i];

					snprintf(key, sizeof(key), "hier.lock.%d", i);
					const NkRect lockR{colLock - S(3.f), yy, S(20.f), kRowH};
					HoverFill(p, lockR, hit.Add(key, lockR) && !sel, 2.f);
					p.IconV(colLock, yy, kRowH, st.locked[i] ? NkIcon::Lock : NkIcon::Unlock,
							st.locked[i] ? fg : dim, 13.f);
					if (hit.Clicked(key))
						st.locked[i] = !st.locked[i];

					snprintf(key, sizeof(key), "hier.row.%d", i);
					if (hit.Clicked(key) && !st.locked[i])
						st.selectedObject = i;
				}
				yy += kRowH;
			}

			p.Unclip();

			// UN CLIC DANS LE VIDE DESELECTIONNE. La zone « hier.list » n'est survolee
			// que la ou aucune ligne ne l'a recouverte, donc ce test designe exactement
			// le vide -- sans qu'on ait a calculer ou finit la derniere ligne.
			if (hit.Clicked("hier.list"))
				st.selectedObject = -1;

			hit.Wheel("hier.list", st.scrollHier, (float32)visibleCount * kRowH, listH);
			p.VScroll(listR, (float32)visibleCount * kRowH, st.scrollHier);
			// Barre HORIZONTALE : les noms longs debordent des que le panneau est
			// retreci, et sans elle la fin du nom est simplement perdue.
			hit.Wheel("hier.list", st.scrollHierH, r.w + S(120.f), r.w);
			p.HScroll(listR, r.w + S(120.f), st.scrollHierH);

			const float32 fy = r.y + r.h - kRowH;
			p.Fill({r.x, fy, r.w, kRowH}, NkRole::WindowBg);
			p.HLine(r.x, fy, r.w);
			char foot[72];
			snprintf(foot, sizeof(foot), "%d objets (%s)", visibleCount,
					 st.selectedObject >= 0 ? "1 selectionne" : "aucune selection");
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
		inline void PaintNavGizmo(NkModelerPainter &p, float32 cx, float32 cy, float32 radius) {
			// Projection isometrique fixe, reprise de l'orientation de Blender :
			// Z vers le haut, Y vers la droite, X vers l'avant-bas.
			struct Axis {
					float32 dx, dy, depth; ///< depth : + = vers l'observateur
					NkRole role;
					const char *label;
					bool positive;
			};
			const Axis kAxes[6] = {
				{0.42f, 0.86f, 0.6f, NkRole::AxisX, "X", true},
				{-0.42f, -0.86f, -0.6f, NkRole::AxisX, "", false},
				{0.92f, -0.30f, 0.2f, NkRole::AxisY, "Y", true},
				{-0.92f, 0.30f, -0.2f, NkRole::AxisY, "", false},
				{-0.18f, -0.96f, 0.4f, NkRole::AxisZ, "Z", true},
				{0.18f, 0.96f, -0.4f, NkRole::AxisZ, "", false},
			};
			const float32 ball = radius * 0.30f;

			// Passe 1 : ce qui est DERRIERE (profondeur negative).
			for (int32 pass = 0; pass < 2; ++pass) {
				for (int32 i = 0; i < 6; ++i) {
					const bool front = kAxes[i].depth > 0.f;
					if ((pass == 0) == front)
						continue;
					const float32 ex = cx + kAxes[i].dx * (radius - ball);
					const float32 ey = cy + kAxes[i].dy * (radius - ball);
					if (kAxes[i].positive)
						p.Line(cx, cy, ex, ey, kAxes[i].role, 2.f);
					if (kAxes[i].positive) {
						p.Disc(ex, ey, ball, kAxes[i].role);
						const float32 lw = p.TextW(kAxes[i].label);
						p.Text(ex - lw * 0.5f, ey - p.LineH() * 0.5f, kAxes[i].label,
							   NkRole::TextOnAccent);
					} else {
						// Creuse : le trou reprend le fond de la VUE, pas celui d'un
						// panneau -- le gizmo flotte au-dessus de la scene.
						p.Ring(ex, ey, ball, kAxes[i].role, NkRole::ViewportTop);
					}
				}
			}
		}

		// ── COLONNE DE BOUTONS DE VUE ───────────────────────────────────────────
		// Zoom, deplacement lateral, camera, bascule orthographique/perspective.
		// VERTICALE et sous le gizmo, comme chez Blender : ce sont des commandes de
		// NAVIGATION, pas d'edition, et les tenir a l'ecart des outils evite de
		// changer d'outil en croyant deplacer la vue.
		inline void PaintViewButtons(NkModelerPainter &p, float32 x, float32 y) {
			const NkIcon kBtns[4] = {NkIcon::Zoom, NkIcon::Pan, NkIcon::Camera, NkIcon::Ortho};
			const float32 d = 26.f;
			for (int32 i = 0; i < 4; ++i) {
				const float32 by = y + (float32)i * (d + 5.f);
				p.Disc(x + d * 0.5f, by + d * 0.5f, d * 0.5f, NkRole::PanelBg);
				p.IconV(x + (d - 14.f) * 0.5f, by, d, kBtns[i], NkRole::Text, 14.f);
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
						if (hit.ShiftDown())
							nk3d::Viewport3DPan(dx, dy);
						else
							nk3d::Viewport3DOrbit(dx * 0.008f, -dy * 0.008f);
					}
					st.navDragging = true;
				} else {
					st.navDragging = false;
				}
				st.navLastX = m.x;
				st.navLastY = m.y;
				if (overView && hit.WheelDelta() != 0.f)
					nk3d::Viewport3DZoom(hit.WheelDelta());
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
			PaintViewButtons(p, r.x + 14.f, navY);
			PaintNavGizmo(p, r.x + 12.f + gz, r.y + r.h - 22.f - gz, gz);

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
			y = PaintSearch(p, r, y);

			static const char *const kPills[] = {"General", "Objet", "Rendu", "Physique", "Tout"};
			float32 x = r.x;
			char key[32];
			for (int32 i = 0; i < 5; ++i) {
				const float32 w = p.TextW(kPills[i]) + S(18.f);
				const NkRect pr{x, y, w, S(20.f)};
				snprintf(key, sizeof(key), "prop.pill.%d", i);
				const bool over = hit.Add(key, pr);
				const bool on = (i == st.activeFilter);
				if (on)
					p.Fill(pr, NkRole::AccentUi, 10.f);
				else
					p.Outline(pr, over ? NkRole::AccentUi : NkRole::Border, NkRole::PanelBg, 10.f);
				p.TextV(x + S(9.f), y, S(20.f), kPills[i], on ? NkRole::TextOnAccent : NkRole::TextMuted);
				if (hit.Clicked(key))
					st.activeFilter = i;
				x += w + S(4.f);
			}
			y += S(26.f);
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
				PaintTransformRow(p, hit, ws, in, r, y, "Position", st.pos, 0.01f, "prop.pos",
								  NkIcon::Refresh, NkIcon::Lock);
				y += Vec3RowH();
				PaintTransformRow(p, hit, ws, in, r, y, "Rotation", st.rot, 0.5f, "prop.rot",
								  NkIcon::Refresh, NkIcon::None, "%.1f");
				y += Vec3RowH();
				PaintTransformRow(p, hit, ws, in, r, y, "Echelle", st.scl, 0.01f, "prop.scl",
								  NkIcon::Refresh, NkIcon::Lock, "%.3f");
				y += Vec3RowH();
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
								 NkHitRegistry &hit) {
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
				static const char *const kV[] = {"8", "12", "6", "12"};
				for (int32 i = 0; i < 4; ++i) {
					p.Fill({r.x, y, kLabelW, kRowH}, NkRole::LabelCol);
					p.TextV(r.x + kPad, y, kRowH, kL[i]);
					p.TextV(r.x + kLabelW + kPad, y, kRowH, kV[i], NkRole::TextMuted);
					p.HLine(r.x, y + kRowH - 1.f, r.w);
					y += kRowH;
				}
			}

			// ── MODIFICATEURS ───────────────────────────────────────────────────
			if (DetailHeader(p, hit, r, y, st, NkDetailModifiers, "Modificateurs")) {
				p.Outline({r.x + 8.f, y + 2.f, r.w - 16.f, kRowH - 4.f}, NkRole::Border,
						  NkRole::InputBg, 2.f);
				p.TextV(r.x + 16.f, y, kRowH, "Selectionner un modificateur", NkRole::TextMuted);
				p.IconV(r.x + r.w - 26.f, y, kRowH, NkIcon::ChevronDown, NkRole::Text, 11.f);
				y += kRowH + 4.f;
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
				struct Slot {
						const char *name;
						NkRole tint;
						int32 faces; ///< nombre de faces portant cet emplacement
				};
				static const Slot kSlots[] = {
					{"M_Bois", NkRole::TypeMat, 4},
					{"M_Metal", NkRole::TypeMesh, 2},
				};
				char key[40];
				for (int32 i = 0; i < 2; ++i) {
					const NkRect sr{r.x, y, r.w, kRowH};
					snprintf(key, sizeof(key), "det.slot.%d", i);
					const bool over = hit.Add(key, sr);
					const bool sel = (st.materialSlot == i);
					if (sel)
						p.Fill(sr, NkRole::AccentUi);
					else
						HoverFill(p, sr, over, 0.f);
					const NkRole fg = sel ? NkRole::TextOnAccent : NkRole::Text;
					// La PASTILLE de couleur reste teintee meme sur ligne selectionnee :
					// c'est elle qui relie l'emplacement a ce qu'on voit dans la vue.
					p.Fill({r.x + 8.f, y + 5.f, 12.f, kRowH - 10.f}, kSlots[i].tint, 2.f);
					p.TextV(r.x + 26.f, y, kRowH, kSlots[i].name, fg);
					char cnt[32];
					snprintf(cnt, sizeof(cnt), "%d faces", kSlots[i].faces);
					p.TextV(r.x + r.w - 8.f - p.TextW(cnt), y, kRowH, cnt,
							sel ? NkRole::TextOnAccent : NkRole::TextMuted);
					p.HLine(r.x, y + kRowH - 1.f, r.w);
					if (hit.Clicked(key))
						st.materialSlot = i;
					y += kRowH;
				}
				// Ajouter / retirer un emplacement, puis assigner la selection courante.
				// « Assigner » est GRISE hors mode edition : hors edition il n'y a pas de
				// faces selectionnees, donc rien a assigner -- et un bouton qui repond
				// sans rien faire est pire qu'un bouton grise.
				const float32 bw = (r.w - 24.f) / 3.f;
				struct Btn {
						const char *label;
						NkIcon ic;
				};
				static const Btn kB[] = {
					{"Ajouter", NkIcon::Add}, {"Retirer", NkIcon::Trash}, {"Assigner", NkIcon::Check}};
				for (int32 i = 0; i < 3; ++i) {
					const NkRect br{r.x + 8.f + (float32)i * bw, y + 3.f, bw - 4.f, kRowH - 6.f};
					const bool off = (i == 2 && st.mode != NkMode::Edit);
					snprintf(key, sizeof(key), "det.mat.%d", i);
					const bool over = !off && hit.Add(key, br);
					p.Outline(br, NkRole::Border, over ? NkRole::PanelHeader : NkRole::InputBg, 2.f);
					const NkRole fg = off ? NkRole::TextMuted : NkRole::Text;
					p.IconV(br.x + 5.f, br.y, br.h, kB[i].ic, fg, 12.f);
					p.TextV(br.x + 21.f, br.y, br.h, kB[i].label, fg);
				}
				y += kRowH + 4.f;
			}

			// ── SOUS-MAILLAGES ──────────────────────────────────────────────────
			// La contrepartie du point precedent : ce que les emplacements DECOUPENT.
			// Un sous-maillage n'est pas un objet separe -- c'est un groupe de faces du
			// meme maillage. Le selectionner ici doit selectionner ses faces dans la
			// vue, ce qui donne le chemin inverse de « selectionner puis assigner ».
			if (DetailHeader(p, hit, r, y, st, NkDetailSubMesh, "Sous-maillages")) {
				static const char *const kSub[] = {"Corps (M_Bois)", "Ferrures (M_Metal)"};
				char key[40];
				for (int32 i = 0; i < 2; ++i) {
					const NkRect sr{r.x, y, r.w, kRowH};
					snprintf(key, sizeof(key), "det.sub.%d", i);
					const bool over = hit.Add(key, sr);
					const bool sel = (st.materialSlot == i);
					if (sel)
						p.Fill(sr, NkRole::AccentUi);
					else
						HoverFill(p, sr, over, 0.f);
					const NkRole fg = sel ? NkRole::TextOnAccent : NkRole::Text;
					p.IconV(r.x + 8.f, y, kRowH, NkIcon::Mesh, fg, 12.f);
					p.TextV(r.x + 26.f, y, kRowH, kSub[i], fg);
					p.HLine(r.x, y + kRowH - 1.f, r.w);
					if (hit.Clicked(key))
						st.materialSlot = i;
					y += kRowH;
				}
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
			static const B kBtns[] = {
				{NkIcon::Add, "Ajouter"}, {NkIcon::Import, "Importer"}, {NkIcon::Save, "Tout enregistrer"}};
			for (int32 i = 0; i < 3; ++i) {
				p.IconV(x, r.y, topH, kBtns[i].ic, NkRole::Text, 13.f);
				p.TextV(x + 18.f, r.y, topH, kBtns[i].label);
				x += 18.f + p.TextW(kBtns[i].label) + 16.f;
			}
			p.VLine(x, r.y + 6.f, topH - 12.f);
			x += 10.f;
			p.IconV(x, r.y, topH, NkIcon::ArrowLeft, NkRole::Text, 13.f);
			p.IconV(x + 22.f, r.y, topH, NkIcon::ArrowRight, NkRole::Text, 13.f);
			p.TextV(x + 50.f, r.y, topH, "Tout > Contenu > Perso", NkRole::TextMuted);
			p.HLine(r.x, r.y + topH - 1.f, r.w);

			// Arbre de dossiers. Ils structurent le PROJET et non le disque : ils seront
			// enregistres DANS le fichier de projet, comme demande.
			const float32 treeW = r.w * 0.18f;
			const float32 ty = r.y + topH;
			const float32 th = r.h - topH;
			// Tout le corps du navigateur est DECOUPE sous sa barre de titre : arbre et
			// cartes sont dessines a partir d'une position defilee, donc negative des le
			// premier cran, et sans decoupe la premiere rangee de cartes se peint sur la
			// barre « Ajouter / Importer ».
			p.Clip({r.x, ty, r.w, th});
			p.Fill({r.x, ty, treeW, th}, NkRole::WindowBg);
			p.VLine(r.x + treeW, ty, th);
			// LES DOSSIERS SONT ORANGE et portent un CHEVRON qui replie leur contenu,
			// exactement comme dans la hierarchie. Deux listes qui montrent la meme
			// chose -- une arborescence -- doivent se manipuler pareil, sinon
			// l'utilisateur reapprend a chaque panneau.
			//
			// La couleur du dossier est conservee MEME SELECTIONNE : c'est ce qui le
			// distingue d'un asset au premier coup d'oeil, et la selection se lit deja
			// au fond de la ligne.
			float32 dy = ty + S(4.f) - treeScroll;
			char fkey[40];
			for (int32 i = 0; i < 5; ++i) {
				const bool child = (i > 0);
				if (child && !st.folderOpen[0])
					continue; // un enfant disparait si la racine est repliee
				const bool on = (i == st.selectedFolder);
				const NkRect rowR{r.x, dy, treeW, kRowH};
				snprintf(fkey, sizeof(fkey), "brow.dir.%d", i);
				const bool over = hit.Add(fkey, rowR);
				if (on)
					p.Fill(rowR, NkRole::AccentUi);
				else
					HoverFill(p, rowR, over, 0.f);
				float32 fx = r.x + S(6.f) + (child ? S(14.f) : 0.f);
				if (!child) {
					const NkRect cr{fx - S(2.f), dy, S(16.f), kRowH};
					snprintf(fkey, sizeof(fkey), "brow.chev.%d", i);
					hit.Add(fkey, cr);
					p.IconV(fx, dy, kRowH,
							st.folderOpen[0] ? NkIcon::ChevronDown : NkIcon::ChevronRight,
							on ? NkRole::TextOnAccent : NkRole::TextMuted, 12.f);
					if (hit.Clicked(fkey))
						st.folderOpen[0] = !st.folderOpen[0];
				}
				fx += S(16.f);
				p.IconV(fx, dy, kRowH,
						(!child && st.folderOpen[0]) ? NkIcon::FolderOpen : NkIcon::Folder,
						NkRole::TypeFolder, 13.f);
				snprintf(fkey, sizeof(fkey), "brow.name.%d", i);
				EditableText(p, hit, ws, in, fkey, {fx + S(18.f), dy, treeW - fx - S(24.f), kRowH},
							 st.folderNames[i], on ? NkRole::TextOnAccent : NkRole::Text,
							 st.folderNames[i], 32u);
				snprintf(fkey, sizeof(fkey), "brow.dir.%d", i);
				if (hit.Clicked(fkey))
					st.selectedFolder = i;
				dy += kRowH;
			}

			// Filtres par TYPE. Chaque pastille porte la couleur de son type, prise dans
			// le theme : le navigateur et le reste de l'application parlent ainsi la meme
			// langue de couleur.
			const float32 ax = r.x + treeW + 10.f;
			p.Fill({ax, ty + 6.f, 150.f, 20.f}, NkRole::InputBg, 2.f);
			p.IconV(ax + 6.f, ty + 6.f, 20.f, NkIcon::Search, NkRole::Text, 12.f);
			p.TextV(ax + 24.f, ty + 6.f, 20.f, "Rechercher", NkRole::TextMuted);
			float32 fx = ax + 168.f;
			static const char *const kTypes[] = {"Maillage", "Animation", "Materiau", "Texture"};
			const NkRole kTypeRoles[] = {NkRole::TypeMesh, NkRole::TypeAnim, NkRole::TypeMat,
										 NkRole::TypeTex};
			for (int32 i = 0; i < 4; ++i) {
				const float32 w = p.TextW(kTypes[i]) + 30.f;
				p.Outline({fx, ty + 6.f, w, 20.f}, NkRole::Border, NkRole::PanelBg, 10.f);
				p.Fill({fx + 8.f, ty + 13.f, 7.f, 7.f}, kTypeRoles[i], 4.f);
				p.TextV(fx + 20.f, ty + 6.f, 20.f, kTypes[i], NkRole::TextMuted);
				fx += w + 6.f;
			}
			p.HLine(ax - 10.f, ty + 32.f, r.w - treeW);

			// Vignettes. La bande de couleur SOUS la vignette redit le type : on
			// reconnait un materiau d'un maillage sans lire l'etiquette.
			// ── CARTES D'ASSETS, CALQUEES SUR LE NAVIGATEUR D'UNREAL ────────────
			// Structure d'une carte, de haut en bas :
			//   1. l'apercu, sur un DAMIER (il dit « ce fond est vide », pas « ce fond
			//      est gris ») ;
			//   2. une BANDE DE COULEUR fine, qui ouvre le pied de carte ;
			//   3. le NOM ;
			//   4. le TYPE, en petit et attenue.
			//
			// Mon dessin precedent mettait la bande SOUS l'apercu et le nom DEHORS : le
			// nom flottait entre deux cartes et on ne savait plus a laquelle il
			// appartenait des que les libelles etaient longs. Chez Unreal tout est
			// DANS la carte, et c'est ce qui rend la grille lisible.
			//
			// L'APERCU DIT LE TYPE, pas seulement la couleur : on reconnait une forme
			// avant de lire une etiquette, et un code couleur seul echoue des qu'il y a
			// du daltonisme ou un ecran mal calibre.
			enum class Preview : uint8 { Mesh = 0, Material, Texture, Animation, Blueprint };
			struct Asset {
					const char *name;
					const char *type;
					NkRole role;
					Preview kind;
					bool selected;
			};
			static const Asset kAssets[] = {
				{"SM_Cube", "Maillage", NkRole::TypeMesh, Preview::Mesh, true},
				{"SM_Tete", "Maillage", NkRole::TypeMesh, Preview::Mesh, false},
				{"M_Bois", "Materiau", NkRole::TypeMat, Preview::Material, false},
				{"T_Ecorce", "Texture", NkRole::TypeTex, Preview::Texture, false},
				{"A_Marche", "Animation", NkRole::TypeAnim, Preview::Animation, false},
			};
			float32 tx = ax;
			// CARTES PLUS HAUTES, comme dans Unreal. L'apercu est ce qu'on regarde et
			// un carre l'ecrase : un format legerement portrait laisse voir la
			// silhouette d'un objet debout -- personnage, arbre, colonne -- qui est le
			// cas le plus courant. Cela donne aussi au pied la place de deux lignes
			// sans rogner l'image.
			const float32 tw = 96.f;   // largeur de carte
			const float32 pvH = 96.f;  // hauteur d'apercu : PLUS GRANDE
			const float32 barH2 = 3.f; // bande de type
			const float32 footH = 34.f; // pied : nom + type
			const float32 tyy = ty + 42.f - assetScroll;
			for (int32 i = 0; i < 5; ++i) {
				const float32 cardH = pvH + barH2 + footH;
				// OMBRE PORTEE legere : elle detache la carte du fond du panneau et
				// donne le relief d'Unreal. Deux ou trois pixels suffisent -- une ombre
				// marquee ferait flotter les cartes et fatiguerait sur une grille dense.
				p.Fill({tx + 2.f, tyy + 3.f, tw, cardH}, NkColor{0, 0, 0, 90}, 3.f);
				// Carte selectionnee : contour a l'accent d'interface. Chez Unreal c'est
				// un liseré, pas un fond plein -- un fond plein ecraserait l'apercu, qui
				// est justement ce qu'on regarde.
				if (kAssets[i].selected)
					p.Fill({tx - 2.f, tyy - 2.f, tw + 4.f, cardH + 4.f}, NkRole::AccentUi, 3.f);

				// 1. DAMIER de fond, comme Unreal : il dit que l'apercu est detoure.
				const float32 c = 8.f;
				p.Fill({tx, tyy, tw, pvH}, NkRole::InputBg);
				for (int32 gx = 0; gx * c < tw; ++gx)
					for (int32 gy = 0; gy * c < pvH; ++gy)
						if (((gx + gy) & 1) == 0) {
							const float32 cw = ((float32)(gx + 1) * c > tw) ? tw - (float32)gx * c : c;
							const float32 ch = ((float32)(gy + 1) * c > pvH) ? pvH - (float32)gy * c : c;
							p.Fill({tx + (float32)gx * c, tyy + (float32)gy * c, cw, ch}, NkRole::WindowBg);
						}

				const float32 cx = tx + tw * 0.5f, cy = tyy + pvH * 0.5f;
				switch (kAssets[i].kind) {
					case Preview::Material:
						// Boule de rendu, avec son reflet : sans le reflet, le disque se
						// lit comme une pastille de couleur.
						p.Disc(cx, cy, 20.f, kAssets[i].role);
						p.Disc(cx - 7.f, cy - 7.f, 5.f, NkRole::Text);
						break;
					case Preview::Texture: {
						// Un carre de texture : plein, avec un damier plus fin dedans.
						const float32 q = 6.f, half = q * 3.f;
						for (int32 gx = 0; gx < 6; ++gx)
							for (int32 gy = 0; gy < 6; ++gy)
								p.Fill({cx - half + (float32)gx * q, cy - half + (float32)gy * q, q, q},
									   ((gx + gy) & 1) ? kAssets[i].role : NkRole::PanelHeader);
						break;
					}
					case Preview::Animation: {
						const float32 w2 = 20.f, h2 = 13.f;
						p.Line(cx - w2, cy + h2, cx - w2 * 0.3f, cy - h2 * 0.7f, kAssets[i].role, 2.f);
						p.Line(cx - w2 * 0.3f, cy - h2 * 0.7f, cx + w2 * 0.4f, cy + h2 * 0.3f,
							   kAssets[i].role, 2.f);
						p.Line(cx + w2 * 0.4f, cy + h2 * 0.3f, cx + w2, cy - h2, kAssets[i].role, 2.f);
						p.Disc(cx - w2 * 0.3f, cy - h2 * 0.7f, 3.f, NkRole::Text);
						p.Disc(cx + w2 * 0.4f, cy + h2 * 0.3f, 3.f, NkRole::Text);
						break;
					}
					default: {
						// Cube en volume : face avant pleine, dessus et cote en biais. Un
						// carre plein ne dirait pas « volume ».
						const float32 hw = 16.f, hh = 14.f, dp = 8.f;
						p.Fill({cx - hw, cy - hh + dp, hw * 2.f, hh * 2.f - dp}, NkRole::PanelHeader);
						p.OutlineSharp({cx - hw, cy - hh + dp, hw * 2.f, hh * 2.f - dp}, kAssets[i].role);
						p.Line(cx - hw, cy - hh + dp, cx - hw + dp, cy - hh, kAssets[i].role);
						p.Line(cx - hw + dp, cy - hh, cx + hw + dp, cy - hh, kAssets[i].role);
						p.Line(cx + hw, cy - hh + dp, cx + hw + dp, cy - hh, kAssets[i].role);
						p.Line(cx + hw + dp, cy - hh, cx + hw + dp, cy + hh - dp, kAssets[i].role);
						p.Line(cx + hw, cy + hh, cx + hw + dp, cy + hh - dp, kAssets[i].role);
						break;
					}
				}

				// 2. BANDE DE TYPE, puis 3. le NOM et 4. le TYPE, tous DANS la carte.
				p.Fill({tx, tyy + pvH, tw, barH2}, kAssets[i].role);
				p.Fill({tx, tyy + pvH + barH2, tw, footH}, NkRole::PanelHeader);
				// LE TEXTE RESTE DANS LE CADRE, en hauteur comme en largeur. Il etait
				// pose a des decalages fixes qui ignoraient la hauteur de ligne et la
				// largeur disponible : a la premiere police plus grande ou au premier
				// nom long, il debordait par le bas et par la droite. On CENTRE le bloc
				// de deux lignes dans le pied, et on TRONQUE a la largeur utile.
				{
					const float32 fy = tyy + pvH + barH2;
					const float32 lh = p.LineH();
					const float32 pad = 6.f;
					const float32 maxW = tw - pad * 2.f;
					float32 fyy = fy + (footH - (lh * 2.f + 2.f)) * 0.5f;
					p.TextClipped(tx + pad, fyy, maxW, kAssets[i].name, NkRole::Text);
					fyy += lh + 2.f;
					p.TextClipped(tx + pad, fyy, maxW, kAssets[i].type, NkRole::TextMuted);
				}
				tx += tw + 10.f;
			}
			p.Unclip();
			const NkRect treeArea{r.x, ty, treeW, th};
			const NkRect assetArea{ax - S(10.f), ty + S(33.f), r.w - treeW - S(10.f), th - S(33.f)};
			hit.Add("brow.tree", treeArea);
			hit.Wheel("brow.tree", st.scrollTree, 5.f * kRowH + S(8.f), treeArea.h);
			hit.Add("brow.assets", assetArea);
			// La hauteur de contenu etait figee a 125 px, valeur de l'ancienne carte.
			// Les cartes font maintenant 133 px : le defilement s'arretait avant le bas
			// et la derniere rangee restait inaccessible.
			const float32 assetContentH = 42.f + pvH + barH2 + footH + 10.f;
			hit.Wheel("brow.assets", st.scrollAssets, assetContentH, assetArea.h);
			p.VScroll(treeArea, 5.f * kRowH + S(8.f), treeScroll);
			p.VScroll(assetArea, assetContentH, assetScroll);
			const float32 ew = p.TextW("5 elements");
			p.TextV(r.x + r.w - ew - kPad, r.y + r.h - kRowH, kRowH, "5 elements", NkRole::TextMuted);
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
						if (cur)
							p.IconV(er.x + er.w - S(20.f), er.y, itemH, NkIcon::Check,
									o2 ? NkRole::TextOnAccent : NkRole::AccentUi, 12.f);
						if (hit.Clicked(key)) {
							st.modKind = flatBase + i;
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
