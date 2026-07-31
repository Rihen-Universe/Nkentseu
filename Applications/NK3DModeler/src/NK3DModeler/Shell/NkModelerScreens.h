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

#include "NK3DModeler/Shell/NkModelerUI.h"
#include "NK3DModeler/Shell/NkModelerInput.h"
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
			st.wantDragMove = hit.Clicked("win.drag");
		}

		// ── ONGLETS DE DOCUMENT ─────────────────────────────────────────────────
		inline void PaintTabsI(NkModelerPainter &p, const NkRect &r, const char *const *scenes, int32 n,
							   NkModelerState &st, NkHitRegistry &hit) {
			p.Fill(r, NkRole::PanelBg);
			p.HLine(r.x, r.y + r.h - 1.f, r.w);
			float32 x = S(10.f);
			const float32 h = r.h - 2.f;
			char key[24];
			for (int32 i = 0; i < n; ++i) {
				const float32 w = p.TextW(scenes[i]) + S(34.f);
				const NkRect tr{x, r.y + 2.f, w, h};
				snprintf(key, sizeof(key), "tab.%d", i);
				const bool over = hit.Add(key, tr);
				const bool on = (i == st.activeTab);
				// L'onglet ACTIF prend la couleur de l'EN-TETE, pas un accent : c'est une
				// continuite de surface avec ce qui est dessous, pas une selection.
				p.Fill(tr, on ? NkRole::PanelHeader : (over ? NkRole::PanelBg : NkRole::InputBg), 3.f);
				p.TextV(x + S(10.f), r.y, r.h, scenes[i], on ? NkRole::Text : NkRole::TextMuted);
				p.IconV(x + w - S(18.f), r.y, r.h, NkIcon::WinClose, NkRole::TextMuted, 10.f);
				if (hit.Clicked(key))
					st.activeTab = i;
				x += w + 3.f;
			}
			const NkRect ar{x + S(4.f), r.y + 2.f, S(24.f), h};
			HoverFill(p, ar, hit.Add("tab.add", ar));
			p.IconV(x + S(8.f), r.y, r.h, NkIcon::Add, NkRole::Text, 12.f);
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
								 NkHitRegistry &hit) {
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
			// Rihen avait raison : ce ne sont pas deux boutons mais UNE liste, qui va
			// s'allonger. Deux boutons auraient cesse de tenir au troisieme mode.
			const NkIcon kModeIc[5] = {NkIcon::Mesh, NkIcon::Edit, NkIcon::Layers, NkIcon::Ruler,
									   NkIcon::Overlay};
			const float32 cbW = S(150.f), cbH = S(22.f), cbY = r.y + (r.h - cbH) * 0.5f;
			const NkRect cb{x, cbY, cbW, cbH};
			const bool cbOver = hit.Add("tb.mode", cb);
			p.Outline(cb, cbOver ? NkRole::AccentUi : NkRole::Border, NkRole::InputBg, 3.f);
			p.IconV(x + S(7.f), cbY, cbH, kModeIc[(uint8)st.mode], NkRole::AccentUi, 13.f);
			p.TextV(x + S(26.f), cbY, cbH, NkModeName(st.mode));
			p.IconV(x + cbW - S(18.f), cbY, cbH, NkIcon::ChevronDown, NkRole::Text, 11.f);
			if (hit.Clicked("tb.mode")) {
				// Tant que le menu deroulant n'est pas ecrit, le clic FAIT DEFILER les
				// modes. C'est provisoire et assume : mieux vaut un bouton qui repond
				// qu'un bouton mort en attendant la liste.
				st.mode = (NkMode)(((uint8)st.mode + 1u) % (uint8)NkMode::Count);
			}
			// La liste deroulee, si elle est ouverte.
			if (st.openMenu == 100) {
				const float32 itemH = S(24.f);
				const NkRect list{x, cbY + cbH + 2.f, cbW, itemH * (float32)NkMode::Count};
				p.Fill(list, NkRole::PanelHeader, 3.f);
				char key[24];
				for (uint8 i = 0; i < (uint8)NkMode::Count; ++i) {
					const NkRect ir{list.x, list.y + (float32)i * itemH, list.w, itemH};
					snprintf(key, sizeof(key), "tb.mode.%u", (uint32)i);
					const bool over = hit.Add(key, ir);
					if (over)
						p.Fill(ir, NkRole::AccentUi);
					p.IconV(ir.x + S(7.f), ir.y, itemH, kModeIc[i], over ? NkRole::TextOnAccent : NkRole::Text,
							13.f);
					p.TextV(ir.x + S(26.f), ir.y, itemH, NkModeName((NkMode)i),
							over ? NkRole::TextOnAccent : NkRole::Text);
					if (hit.Clicked(key)) {
						st.mode = (NkMode)i;
						st.openMenu = -1;
					}
				}
			}
			x += cbW + S(12.f);
			p.VLine(x - S(7.f), r.y + S(7.f), r.h - S(14.f));

			// Sous-mode de selection : GRISE en mode objet plutot que masque -- une
			// commande qui disparait laisse croire qu'elle n'existe pas.
			const bool selUsable = (st.mode != NkMode::Object);
			const NkRole selFg = selUsable ? NkRole::Text : NkRole::TextMuted;
			{
				const float32 w = ih + S(5.f) + p.TextW("Mode de selection") + S(28.f);
				const NkRect br{x - S(7.f), r.y + S(5.f), w, r.h - S(10.f)};
				if (selUsable)
					HoverFill(p, br, hit.Add("tb.selmode", br));
				p.IconV(x, r.y, r.h, NkIcon::Cursor, selFg, 14.f);
				p.TextV(x + ih + S(5.f), r.y, r.h, "Mode de selection", selFg);
				p.IconV(x + w - S(24.f), r.y, r.h, NkIcon::ChevronDown, selFg, 11.f);
				x += w;
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
		inline float32 PaintPanelTab(NkModelerPainter &p, const NkRect &r, const char *title) {
			const float32 h = 26.f;
			p.Fill({r.x, r.y, r.w, h}, NkRole::PanelHeader);
			p.TextV(r.x + kPad, r.y, h, title);
			p.IconV(r.x + r.w - 20.f, r.y, h, NkIcon::WinClose, NkRole::Text, 11.f);
			p.HLine(r.x, r.y + h - 1.f, r.w);
			return r.y + h;
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
		// DEUX COLONNES D'ETAT a droite : l'OEIL (visible dans la vue) et le CADENAS
		// (objet verrouille, donc non selectionnable). Elles sont alignees en colonne
		// a l'extreme droite -- ce sont des etats, pas des attributs du nom, et
		// l'alignement permet de les lire d'un coup sur toute la liste.
		inline void PaintHierarchy(NkModelerPainter &p, const NkRect &r, NkModelerState &st,
								   NkHitRegistry &hit) {
			p.Fill(r, NkRole::PanelBg);
			p.VLine(r.x + r.w - 1.f, r.y, r.h);
			float32 y = PaintPanelTab(p, r, "Hierarchie");
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
					const char *name;
					const char *type;
					NkIcon icon;
					bool expandable;
					bool folder;
			};
			static const Row kRows[] = {
				{0, "Scene", "", NkIcon::Globe, true, false},
				{1, "Cube", "Maillage", NkIcon::Mesh, false, false},
				{1, "Sphere", "Maillage", NkIcon::Circle, false, false},
				{1, "Groupe", "Dossier", NkIcon::Folder, true, true},
				{2, "Roue", "Maillage", NkIcon::Mesh, false, false},
				{2, "Axe", "Maillage", NkIcon::Mesh, false, false},
			};
			const int32 nRows = 6;
			const float32 listTop = y;
			const float32 listH = r.y + r.h - kRowH - listTop;
			const NkRect listR{r.x, listTop, r.w, listH};
			hit.Add("hier.list", listR);
			hit.Wheel("hier.list", st.scrollHier, (float32)nRows * kRowH, listH);

			y -= st.scrollHier;
			char key[32];
			for (int32 i = 0; i < nRows; ++i) {
				const NkRect rowR{r.x, y, r.w, kRowH};
				if (y >= listTop - kRowH && y < listTop + listH) {
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
					if (kRows[i].expandable)
						p.IconV(tx, y, kRowH, i == 0 ? NkIcon::ChevronDown : NkIcon::ChevronRight, dim, 12.f);
					tx += S(14.f);
					if (kRows[i].folder)
						p.IconV(tx, y, kRowH, NkIcon::Folder, NkRole::TypeFolder, 13.f);
					else
						p.IconV(tx, y, kRowH, kRows[i].icon, fg, 13.f);
					p.TextV(tx + S(18.f), y, kRowH, kRows[i].name, fg);
					if (*kRows[i].type)
						p.TextV(colType, y, kRowH, kRows[i].type, dim);

					// OEIL et CADENAS : zones sensibles PROPRES, declarees APRES la
					// ligne pour qu'elles la recouvrent. Cliquer l'oeil ne doit pas
					// selectionner l'objet -- ce sont deux intentions differentes.
					snprintf(key, sizeof(key), "hier.eye.%d", i);
					const NkRect eyeR{colEye - S(3.f), y, S(20.f), kRowH};
					const bool eOver = hit.Add(key, eyeR);
					HoverFill(p, eyeR, eOver && !sel, 2.f);
					p.IconV(colEye, y, kRowH, st.visible[i] ? NkIcon::Eye : NkIcon::EyeClosed,
							st.visible[i] ? fg : dim, 13.f);
					if (hit.Clicked(key))
						st.visible[i] = !st.visible[i];

					snprintf(key, sizeof(key), "hier.lock.%d", i);
					const NkRect lockR{colLock - S(3.f), y, S(20.f), kRowH};
					const bool lOver = hit.Add(key, lockR);
					HoverFill(p, lockR, lOver && !sel, 2.f);
					p.IconV(colLock, y, kRowH, st.locked[i] ? NkIcon::Lock : NkIcon::Unlock,
							st.locked[i] ? fg : dim, 13.f);
					if (hit.Clicked(key))
						st.locked[i] = !st.locked[i];

					// La selection n'est prise QU'APRES : si l'oeil ou le cadenas a
					// recu le clic, c'est lui qui est survole, pas la ligne.
					snprintf(key, sizeof(key), "hier.row.%d", i);
					if (hit.Clicked(key) && !st.locked[i])
						st.selectedObject = i;
				}
				y += kRowH;
			}
			p.VScroll(listR, (float32)nRows * kRowH, st.scrollHier);

			const float32 fy = r.y + r.h - kRowH;
			p.Fill({r.x, fy, r.w, kRowH}, NkRole::WindowBg);
			p.HLine(r.x, fy, r.w);
			char foot[64];
			snprintf(foot, sizeof(foot), "%d objets (1 selectionne)", nRows);
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
								  NkHitRegistry &hit, const NkShortcutTable &sc) {
			const bool editMode = (st.mode != NkMode::Object);
			p.Fill(r, NkRole::ViewportTop);

			// SOL EN PERSPECTIVE. Les fuyantes convergent vers un point de fuite, et les
			// lignes d'horizon se RESSERRENT vers lui. Ma premiere version tracait des
			// horizontales equidistantes : aucune profondeur, ca ressemblait a du papier
			// reglé. Et elle dessinait les fuyantes en RECTANGLES horizontaux, d'ou les
			// barres etranges de la capture.
			const float32 vpx = r.x + r.w * 0.5f;
			const float32 vpy = r.y + r.h * 0.42f;
			for (int32 i = 1; i <= 9; ++i) {
				const float32 t = (float32)i / 9.f;
				const float32 yy = vpy + (r.y + r.h - vpy) * t * t;
				const float32 half = r.w * 0.55f * t;
				float32 x0 = vpx - half, x1 = vpx + half;
				if (x0 < r.x)
					x0 = r.x;
				if (x1 > r.x + r.w)
					x1 = r.x + r.w;
				p.Fill({x0, yy, x1 - x0, 1.f}, NkRole::GridLine);
			}
			for (int32 i = -6; i <= 6; ++i) {
				const float32 xEnd = vpx + (float32)i * r.w * 0.19f;
				p.Line(vpx, vpy, xEnd, r.y + r.h, NkRole::GridLine);
			}

			// ── BARRE FLOTTANTE GAUCHE : les options de VUE.
			// Les menus de commandes (Ajouter/Objet/Selection) vivent dans la barre
			// d'outils principale, comme dans la maquette. Les dupliquer ici donnerait
			// deux endroits a tenir a jour pour une seule liste.
			const float32 barH = 26.f, barY = r.y + 10.f;
			// Chaque entree porte SON icone, comme chez Unreal : la camera pour la
			// projection, l'ampoule pour l'eclairage, l'oeil pour l'affichage. Trois
			// libelles nus se lisent comme une phrase et non comme trois reglages.
			static const char *const kView[] = {"Perspective", "Eclaire", "Affichage"};
			const NkIcon kViewIc[3] = {NkIcon::Camera, NkIcon::Light, NkIcon::Eye};
			float32 gw = 34.f;
			for (int32 i = 0; i < 3; ++i)
				gw += 18.f + p.TextW(kView[i]) + 26.f;
			float32 bx = r.x + 10.f;
			p.Fill({bx, barY, gw, barH}, NkRole::PanelBg, 5.f);
			p.IconV(bx + 9.f, barY, barH, NkIcon::Menu, NkRole::Text, 13.f);
			bx += 32.f;
			for (int32 i = 0; i < 3; ++i) {
				p.IconV(bx, barY, barH, kViewIc[i], NkRole::Text, 13.f);
				p.TextV(bx + 18.f, barY, barH, kView[i]);
				bx += 18.f + p.TextW(kView[i]) + 5.f;
				p.IconV(bx, barY, barH, NkIcon::ChevronDown, NkRole::Text, 11.f);
				bx += 21.f;
			}

			// ── BARRE FLOTTANTE DROITE, calquee sur celle d'Unreal.
			// Rihen a demande que SELECTION et CURSEUR soient colles aux outils de
			// transformation plutot que relegues dans une colonne a part : ils
			// repondent tous a la meme question -- « que fait mon clic ? » -- et un
			// seul est actif a la fois. Les separer forcait un aller-retour du regard
			// entre deux coins de l'ecran pour un choix unique.
			//
			// L'AIMANTATION EST PAR TRANSFORMATION, comme chez Unreal : une bascule et
			// une valeur pour la grille, une pour l'angle, une pour l'echelle. Un
			// interrupteur global obligerait a le couper pour tourner librement alors
			// qu'on veut garder la grille en deplacement.
			const float32 btn = 24.f;
			const int32 nSub = editMode ? 3 : 0;
			// L'ETAT vit dans NkModelerState, pas dans la table : la table decrit
			// l'apparence, l'etat appartient a la session et doit survivre a la frame.
			struct Snap {
					NkIcon icon;
					const char *value;
			};
			static const Snap kSnaps[3] = {
				{NkIcon::Ortho, "10"},		// grille : pas de deplacement
				{NkIcon::Rotate, "10 deg"}, // angle
				{NkIcon::Scale, "0,25"},	// echelle
			};
			float32 tw = 12.f + (float32)(nSub + 7) * (btn + 2.f) + 16.f;
			for (int32 i = 0; i < 3; ++i)
				tw += btn + p.TextW(kSnaps[i].value) + S(12.f);
			float32 tx = r.x + r.w - 10.f - tw;
			p.Fill({tx, barY, tw, barH}, NkRole::PanelBg, 5.f);
			tx += 5.f;

			if (editMode) {
				// Sous-modes sommet / arete / face. L'ACTIF prend l'accent d'interface,
				// pas l'ambre : c'est un etat de l'outil, pas une selection dans la scene.
				const NkIcon kSub[3] = {NkIcon::Dot, NkIcon::Ruler, NkIcon::Square};
				static const char *const kSubKeys[3] = {"vp.sub.0", "vp.sub.1", "vp.sub.2"};
				for (int32 i = 0; i < 3; ++i) {
					const NkRect br{tx, barY + 2.f, btn, barH - 4.f};
					const bool over = hit.Add(kSubKeys[i], br);
					const bool on = ((int32)st.subMode == i);
					if (on)
						p.Fill(br, NkRole::AccentUi, 3.f);
					else
						HoverFill(p, br, over);
					p.IconV(tx + (btn - S(14.f)) * 0.5f, barY, barH, kSub[i],
							on ? NkRole::TextOnAccent : NkRole::Text, 14.f);
					if (hit.Clicked(kSubKeys[i]))
						st.subMode = (NkSubMode)i;
					tx += btn + 2.f;
				}
				p.VLine(tx + 1.f, barY + 6.f, barH - 12.f);
				tx += 5.f;
			}

			// Selection, curseur, puis les trois transformations et le repere.
			const NkIcon kTools[7] = {NkIcon::Cursor, NkIcon::Gizmo, NkIcon::Move,
									  NkIcon::Rotate, NkIcon::Scale, NkIcon::Globe, NkIcon::Camera};
			static const char *const kToolKeys[7] = {"vp.t.0", "vp.t.1", "vp.t.2", "vp.t.3",
													"vp.t.4", "vp.t.5", "vp.t.6"};
			for (int32 i = 0; i < 7; ++i) {
				const NkRect br{tx, barY + 2.f, btn, barH - 4.f};
				const bool over = hit.Add(kToolKeys[i], br);
				// Les cinq premiers sont des OUTILS (un seul actif) ; les deux
				// derniers, repere et camera, sont des reglages a part.
				const bool on = (i < 5) && ((int32)st.tool == i);
				if (on)
					p.Fill(br, NkRole::AccentUi, 3.f);
				else
					HoverFill(p, br, over);
				if (i < 5 && hit.Clicked(kToolKeys[i]))
					st.tool = (NkTool)i;
				p.IconV(tx + (btn - S(14.f)) * 0.5f, barY, barH, kTools[i],
						on ? NkRole::TextOnAccent : NkRole::Text, 14.f);
				// LE POINT en bas a droite du bouton de SELECTION : il annonce un
				// sous-menu (rectangle, cercle, lasso). Sans lui, rien ne dit que le
				// bouton cache un choix -- c'est la convention d'Unreal et de Blender.
				if (i == 0)
					p.Fill({tx + btn - 6.f, barY + barH - 8.f, 3.f, 3.f},
						   on ? NkRole::TextOnAccent : NkRole::Text);
				tx += btn + 2.f;
			}
			p.VLine(tx + 1.f, barY + 6.f, barH - 12.f);
			tx += 7.f;

			// Les trois aimantations : bascule (icone) + valeur, cote a cote.
			bool *const snapFlags[3] = {&st.snapGrid, &st.snapAngle, &st.snapScale};
			static const char *const kSnapKeys[3] = {"vp.snap.0", "vp.snap.1", "vp.snap.2"};
			for (int32 i = 0; i < 3; ++i) {
				const NkRect br{tx, barY + 2.f, btn, barH - 4.f};
				const bool over = hit.Add(kSnapKeys[i], br);
				const bool on = *snapFlags[i];
				if (on)
					p.Fill(br, NkRole::AccentUi, 3.f);
				else
					HoverFill(p, br, over);
				if (hit.Clicked(kSnapKeys[i]))
					*snapFlags[i] = !on;
				p.IconV(tx + (btn - S(13.f)) * 0.5f, barY, barH, kSnaps[i].icon,
						on ? NkRole::TextOnAccent : NkRole::TextMuted, 13.f);
				tx += btn + 3.f;
				// La valeur est ATTENUEE quand l'aimantation est coupee : elle reste
				// lisible (on veut savoir sur quel pas on retombera) sans faire croire
				// qu'elle agit.
				// La valeur reste AFFICHEE quand l'aimantation est coupee, mais
				// attenuee : on veut savoir sur quel pas on retombera en la
				// rallumant, la masquer obligerait a l'activer pour la lire.
				p.TextV(tx, barY, barH, kSnaps[i].value, on ? NkRole::Text : NkRole::TextMuted);
				tx += p.TextW(kSnaps[i].value) + 9.f;
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

		// ── LIGNE DE PROPRIETE A TROIS CHAMPS ───────────────────────────────────
		// Le lisere de couleur d'axe est sur le SEUL BORD GAUCHE du champ, le fond
		// restant neutre : teinter le fond entier rend les trois champs criards et
		// illisibles, c'est un critere de refus ecrit dans le brief.
		//
		// Les champs sont ETROITS et de largeur FIXE, ils ne remplissent pas la
		// colonne. Ma premiere version les etirait sur toute la largeur : les valeurs
		// se retrouvaient perdues au milieu de rectangles vides, et il ne restait
		// aucune place pour l'icone de reinitialisation a droite.
		inline void PaintVec3Row(NkModelerPainter &p, const NkRect &r, float32 y, const char *label,
								 const char *v0, const char *v1, const char *v2, NkIcon trailing) {
			p.Fill({r.x, y, kLabelW, kRowH}, NkRole::LabelCol);
			p.TextV(r.x + kPad, y, kRowH, label);
			const NkRole axes[3] = {NkRole::AxisX, NkRole::AxisY, NkRole::AxisZ};
			const char *vals[3] = {v0, v1, v2};
			const float32 fw = 52.f;
			float32 x = r.x + kLabelW + 4.f;
			for (int32 i = 0; i < 3; ++i) {
				p.Fill({x, y + 2.f, fw, kRowH - 4.f}, NkRole::InputBg, 2.f);
				p.Fill({x, y + 2.f, 2.f, kRowH - 4.f}, axes[i]); // le lisere, bord gauche
				p.TextV(x + 8.f, y, kRowH, vals[i]);
				x += fw + 3.f;
			}
			if (trailing != NkIcon::None)
				p.IconV(r.x + r.w - 20.f, y, kRowH, trailing, NkRole::TextMuted, 12.f);
			p.HLine(r.x, y + kRowH - 1.f, r.w);
		}

		// ── PROPRIETES (droite, haut) ───────────────────────────────────────────
		inline void PaintProperties(NkModelerPainter &p, const NkRect &full, NkModelerState &st,
									NkHitRegistry &hit) {
			const float32 scroll = st.scrollProps;
			p.Fill(full, NkRole::PanelBg);
			p.VLine(full.x, full.y, full.h);
			float32 y = PaintPanelTab(p, full, "Proprietes");
			// A partir d'ici tout travaille DANS la marge.
			const NkRect r = Inset(full);
			y = PaintSearch(p, r, y);

			// Pastilles de filtre : CONTOUREES, sauf l'active qui est pleine. Toutes
			// pleines, on ne verrait plus laquelle est choisie.
			static const char *const kPills[] = {"General", "Objet", "Rendu", "Physique", "Tout"};
			float32 x = r.x + 6.f;
			for (int32 i = 0; i < 5; ++i) {
				const float32 w = p.TextW(kPills[i]) + 18.f;
				const bool on = (i == 4);
				// GELULE, pas carre : le rayon vaut la MOITIE de la hauteur. Mon
				// premier essai passait 10 a un contour qui ne savait pas arrondir,
				// d'ou les pastilles carrees de la capture.
				if (on)
					p.Fill({x, y, w, 20.f}, NkRole::AccentUi, 10.f);
				else
					p.Outline({x, y, w, 20.f}, NkRole::Border, NkRole::PanelBg, 10.f);
				p.TextV(x + 9.f, y, 20.f, kPills[i], on ? NkRole::TextOnAccent : NkRole::TextMuted);
				x += w + 4.f;
			}
			y += 26.f;
			p.HLine(r.x, y, r.w);
			y += 1.f;

			const float32 listTop = y;
			y -= scroll;
			p.Fill({r.x, y, r.w, kRowH}, NkRole::PanelBg);
			p.IconV(r.x + 6.f, y, kRowH, NkIcon::ChevronDown, NkRole::Text, 11.f);
			p.TextV(r.x + 22.f, y, kRowH, "Transformation");
			y += kRowH;
			PaintVec3Row(p, r, y, "Position", "0,00", "0,00", "0,00", NkIcon::Refresh);
			y += kRowH;
			PaintVec3Row(p, r, y, "Rotation", "0,00", "0,00", "0,00", NkIcon::None);
			y += kRowH;
			PaintVec3Row(p, r, y, "Echelle", "1,00", "1,00", "1,00", NkIcon::Lock);
			y += kRowH;
			// L'ascenseur mesure le contenu REEL depuis le haut de la zone de liste :
			// une hauteur devinee ferait glisser le curseur a cote de la matiere.
			const NkRect area{full.x, listTop, full.w, full.y + full.h - listTop};
			hit.Add("props.list", area);
			hit.Wheel("props.list", st.scrollProps, y - listTop + scroll, area.h);
			p.VScroll(area, y - listTop + scroll, scroll);
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
		inline void PaintDetails(NkModelerPainter &p, const NkRect &full, NkModelerState &st,
								 NkHitRegistry &hit) {
			const float32 scroll = st.scrollDetails;
			p.Fill(full, NkRole::PanelBg);
			p.VLine(full.x, full.y, full.h);
			p.HLine(full.x, full.y, full.w);
			float32 y = PaintPanelTab(p, full, "Details (Cube)");
			const NkRect r = Inset(full);
			const float32 listTop = y;
			y -= scroll;

			p.IconV(r.x + 6.f, y, kRowH, NkIcon::ChevronDown, NkRole::Text, 11.f);
			p.TextV(r.x + 22.f, y, kRowH, "Maillage");
			y += kRowH;
			static const char *const kL[] = {"Sommets", "Faces"};
			static const char *const kV[] = {"8", "6"};
			for (int32 i = 0; i < 2; ++i) {
				p.Fill({r.x, y, kLabelW, kRowH}, NkRole::LabelCol);
				p.TextV(r.x + kPad, y, kRowH, kL[i]);
				p.TextV(r.x + kLabelW + kPad, y, kRowH, kV[i], NkRole::TextMuted);
				p.HLine(r.x, y + kRowH - 1.f, r.w);
				y += kRowH;
			}

			p.IconV(r.x + 6.f, y, kRowH, NkIcon::ChevronDown, NkRole::Text, 11.f);
			p.TextV(r.x + 22.f, y, kRowH, "Modificateurs");
			y += kRowH;
			// Le deroulant. Bordure + chevron a droite, comme tout selecteur.
			p.Outline({r.x + 8.f, y + 2.f, r.w - 16.f, kRowH - 4.f}, NkRole::Border, NkRole::InputBg, 2.f);
			p.TextV(r.x + 16.f, y, kRowH, "Selectionner un modificateur", NkRole::TextMuted);
			p.IconV(r.x + r.w - 26.f, y, kRowH, NkIcon::ChevronDown, NkRole::Text, 11.f);
			y += kRowH + 4.f;

			p.IconV(r.x + 6.f, y, kRowH, NkIcon::ChevronRight, NkRole::Text, 11.f);
			p.TextV(r.x + 22.f, y, kRowH, "Materiau");
			y += kRowH;
			const NkRect area{full.x, listTop, full.w, full.y + full.h - listTop};
			hit.Add("det.list", area);
			hit.Wheel("det.list", st.scrollDetails, y - listTop + scroll, area.h);
			p.VScroll(area, y - listTop + scroll, scroll);
		}

		// ── NAVIGATEUR DE PROJET (bas) ──────────────────────────────────────────
		inline void PaintBrowser(NkModelerPainter &p, const NkRect &r, NkModelerState &st,
								 NkHitRegistry &hit) {
			const float32 treeScroll = st.scrollTree;
			const float32 assetScroll = st.scrollAssets;
			p.Fill(r, NkRole::PanelBg);
			p.HLine(r.x, r.y, r.w);

			const float32 topH = 28.f;
			const float32 ih = p.IconSize();
			p.Fill({r.x, r.y, r.w, topH}, NkRole::PanelHeader);
			p.TextV(r.x + kPad, r.y, topH, "Navigateur de projet");
			float32 x = r.x + kPad + p.TextW("Navigateur de projet") + 10.f;
			p.IconV(x, r.y, topH, NkIcon::WinClose, NkRole::Text, 11.f);
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
			p.Fill({r.x, ty, treeW, th}, NkRole::WindowBg);
			p.VLine(r.x + treeW, ty, th);
			static const char *const kDirs[] = {"MonProjet", "Maillages", "Animations", "Materiaux",
												"Textures"};
			float32 dy = ty + 4.f - treeScroll;
			for (int32 i = 0; i < 5; ++i) {
				const bool on = (i == 1);
				if (on)
					p.Fill({r.x, dy, treeW, kRowH}, NkRole::AccentUi);
				const float32 fx = r.x + 8.f + (i ? 12.f : 0.f);
				p.IconV(fx, dy, kRowH, on ? NkIcon::FolderOpen : NkIcon::Folder,
						on ? NkRole::TextOnAccent : NkRole::TextMuted, 13.f);
				p.TextV(fx + 18.f, dy, kRowH, kDirs[i], on ? NkRole::TextOnAccent : NkRole::Text);
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
			const float32 tw = 94.f;		  // largeur de carte
			const float32 pvH = 66.f;		  // hauteur d'apercu
			const float32 barH2 = 3.f;		  // bande de type
			const float32 footH = 30.f;		  // pied : nom + type
			const float32 tyy = ty + 42.f - assetScroll;
			for (int32 i = 0; i < 5; ++i) {
				const float32 cardH = pvH + barH2 + footH;
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
				p.Text(tx + 5.f, tyy + pvH + barH2 + 4.f, kAssets[i].name);
				p.Text(tx + 5.f, tyy + pvH + barH2 + 17.f, kAssets[i].type, NkRole::TextMuted);
				tx += tw + 10.f;
			}
			const NkRect treeArea{r.x, ty, treeW, th};
			const NkRect assetArea{ax - S(10.f), ty + S(33.f), r.w - treeW - S(10.f), th - S(33.f)};
			hit.Add("brow.tree", treeArea);
			hit.Wheel("brow.tree", st.scrollTree, 5.f * kRowH + S(8.f), treeArea.h);
			hit.Add("brow.assets", assetArea);
			hit.Wheel("brow.assets", st.scrollAssets, S(125.f), assetArea.h);
			p.VScroll(treeArea, 5.f * kRowH + S(8.f), treeScroll);
			p.VScroll(assetArea, S(125.f), assetScroll);
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

			for (int32 i = 0; i < 4; ++i) {
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
