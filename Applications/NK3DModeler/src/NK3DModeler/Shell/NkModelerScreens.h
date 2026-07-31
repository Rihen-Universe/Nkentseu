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
#include "NKEditorKit/NkShortcutTable.h"

#include <cstdio>

namespace nkentseu {
	namespace nk3d {

		struct NkModelerState;

		// Hauteur d'une ligne de liste ou de propriete. Reprise de la maquette :
		// 22 px. En dessous le texte touche les bords, au-dessus la densite chute et
		// il faut faire defiler pour rien.
		static const float32 kRowH = 22.f;
		static const float32 kLabelW = 78.f;  ///< colonne de libelles des proprietes
		static const float32 kPad = 8.f;
		// MARGE INTERNE des panneaux de droite. Le contenu ne doit pas toucher les
		// bords : colle au trait de separation, une ligne de propriete se lit comme
		// la continuation du panneau voisin. Applique en RETRECISSANT le rectangle de
		// travail, une fois, plutot qu'en decalant chaque appel -- sinon il suffit
		// d'en oublier un pour que l'alignement casse.
		static const float32 kInset = 10.f;

		// Retrecit un rectangle de la marge interne, sans toucher au haut ni au bas :
		// l'en-tete d'onglet et les fonds pleins doivent, eux, aller bord a bord.
		inline NkRect Inset(const NkRect &r) {
			return {r.x + kInset, r.y, r.w - kInset * 2.f, r.h};
		}

		// ── BARRE DE MENUS ──────────────────────────────────────────────────────
		inline void PaintMenuBar(NkModelerPainter &p, const NkRect &r, const char *projectName) {
			p.Fill(r, NkRole::PanelHeader);
			p.HLine(r.x, r.y + r.h - 1.f, r.w);

			const float32 logo = 24.f;
			const float32 ly = r.y + (r.h - logo) * 0.5f;
			p.Fill({10.f, ly, logo, logo}, NkRole::AccentUi, 4.f);
			const float32 nkW = p.TextW("NK");
			p.TextV(10.f + (logo - nkW) * 0.5f, ly, logo, "NK", NkRole::TextOnAccent);

			float32 x = 10.f + logo + 16.f;
			static const char *const kMenus[] = {"Fichier", "Edition", "Fenetre", "Outils",
												 "Selection", "Objet", "Aide"};
			for (int32 i = 0; i < 7; ++i) {
				p.TextV(x, r.y, r.h, kMenus[i]);
				x += p.TextW(kMenus[i]) + 22.f;
			}

			// Nom du projet CENTRE sur la FENETRE, pas apres les menus : il doit rester
			// au meme endroit quelle que soit la langue, et les libelles traduits
			// changent de largeur de 30 % d'une langue a l'autre.
			const float32 nw = p.TextW(projectName);
			p.TextV(r.w * 0.5f - nw * 0.5f, r.y, r.h, projectName, NkRole::TextMuted);

			// Boutons de fenetre. La fenetre etant SANS CADRE OS, c'est nous qui les
			// portons ; seule la fermeture est coloree, comme dans la maquette.
			const float32 bw = 30.f, bh = 22.f;
			const float32 by = r.y + (r.h - bh) * 0.5f;
			const NkIcon kWin[3] = {NkIcon::WinMin, NkIcon::WinMax, NkIcon::WinClose};
			for (int32 i = 0; i < 3; ++i) {
				const float32 bx = r.w - kPad - (float32)(3 - i) * (bw + 6.f);
				const bool close = (i == 2);
				if (close) {
					p.Fill({bx, by, bw, bh}, NkColor{231, 76, 60, 255}, 3.f);
				} else {
					// BORDURE demandee : sans elle, les deux boutons neutres se
					// fondent dans la barre et on ne devine leur zone cliquable qu'au
					// survol -- alors que la fermeture, coloree, se voit d'emblee.
					p.Outline({bx, by, bw, bh}, NkRole::Border, NkRole::PanelHeader, 3.f);
				}
				p.IconV(bx + (bw - 13.f) * 0.5f, by, bh, kWin[i],
						close ? NkRole::TextOnAccent : NkRole::Text, 13.f);
			}
		}

		// ── ONGLETS DE DOCUMENT ─────────────────────────────────────────────────
		inline void PaintTabs(NkModelerPainter &p, const NkRect &r, const char *const *scenes, int32 n,
							  int32 active) {
			p.Fill(r, NkRole::PanelBg);
			p.HLine(r.x, r.y + r.h - 1.f, r.w);
			float32 x = 10.f;
			const float32 h = r.h - 2.f;
			for (int32 i = 0; i < n; ++i) {
				const float32 w = p.TextW(scenes[i]) + 34.f;
				// L'onglet ACTIF prend la couleur de l'EN-TETE, pas un accent : c'est
				// une continuite de surface avec ce qui est en dessous, pas une
				// selection. L'accent bleu est reserve aux etats d'interface.
				p.Fill({x, r.y + 2.f, w, h}, i == active ? NkRole::PanelHeader : NkRole::InputBg, 3.f);
				p.TextV(x + 10.f, r.y, r.h, scenes[i], i == active ? NkRole::Text : NkRole::TextMuted);
				p.TextV(x + w - 16.f, r.y, r.h, "x", NkRole::TextMuted);
				x += w + 3.f;
			}
			p.TextV(x + 8.f, r.y, r.h, "+", NkRole::TextMuted);
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
		inline void PaintToolbar(NkModelerPainter &p, const NkRect &r, int32 mode) {
			p.Fill(r, NkRole::PanelHeader);
			p.HLine(r.x, r.y + r.h - 1.f, r.w);
			float32 x = kPad;

			p.IconV(x, r.y, r.h, NkIcon::Save, NkRole::Text, 14.f);
			p.TextV(x + 19.f, r.y, r.h, "Enregistrer");
			x += 19.f + p.TextW("Enregistrer") + 14.f;
			p.VLine(x, r.y + 7.f, r.h - 14.f);
			x += 10.f;

			// Le deroulant de mode.
			static const char *const kModes[] = {"Objet", "Edition", "Sculpt 2.5D", "Sculpt",
												 "Texturing"};
			const NkIcon kModeIc[5] = {NkIcon::Mesh, NkIcon::Edit, NkIcon::Layers, NkIcon::Ruler,
									   NkIcon::Overlay};
			const int32 m = (mode >= 0 && mode < 5) ? mode : 0;
			const float32 cbW = 150.f, cbH = 22.f, cbY = r.y + (r.h - cbH) * 0.5f;
			p.Outline({x, cbY, cbW, cbH}, NkRole::Border, NkRole::InputBg, 3.f);
			p.IconV(x + 7.f, cbY, cbH, kModeIc[m], NkRole::AccentUi, 13.f);
			p.TextV(x + 26.f, cbY, cbH, kModes[m]);
			p.IconV(x + cbW - 18.f, cbY, cbH, NkIcon::ChevronDown, NkRole::Text, 11.f);
			x += cbW + 14.f;
			p.VLine(x - 7.f, r.y + 7.f, r.h - 14.f);

			// Sous-mode de selection : actif SEULEMENT hors mode objet, et grise
			// sinon plutot que masque -- une commande qui disparait laisse croire
			// qu'elle n'existe pas.
			const bool selUsable = (m != 0);
			const NkRole selFg = selUsable ? NkRole::Text : NkRole::TextMuted;
			p.IconV(x, r.y, r.h, NkIcon::Cursor, selFg, 14.f);
			p.TextV(x + 19.f, r.y, r.h, "Mode de selection", selFg);
			x += 19.f + p.TextW("Mode de selection") + 6.f;
			p.IconV(x, r.y, r.h, NkIcon::ChevronDown, NkRole::Text, 11.f);
			x += 22.f;

			p.IconV(x, r.y, r.h, NkIcon::Add, NkRole::Text, 14.f);
			p.TextV(x + 19.f, r.y, r.h, "Ajouter");
			x += 19.f + p.TextW("Ajouter") + 16.f;
			p.IconV(x, r.y, r.h, NkIcon::Layers, NkRole::Text, 14.f);
			p.TextV(x + 19.f, r.y, r.h, "Modificateur");

			// Reglages cale a DROITE : action de session, pas de modelisation. La
			// distance visuelle dit cette difference de nature.
			const float32 sw = 19.f + p.TextW("Reglages");
			p.IconV(r.w - kPad - sw, r.y, r.h, NkIcon::Gear, NkRole::Text, 14.f);
			p.TextV(r.w - kPad - sw + 19.f, r.y, r.h, "Reglages");
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
		// DEUX COLONNES D'ETAT a droite, demandees par Rihen et presentes chez
		// Blender comme chez Unreal :
		//   * l'OEIL — visible dans la vue de travail ;
		//   * le CADENAS — objet verrouille, donc non selectionnable.
		// Elles sont a l'EXTREME DROITE et non collees au nom : ce sont des etats,
		// pas des attributs du nom, et les aligner en colonne permet de les lire
		// d'un coup d'oeil sur toute la liste.
		inline void PaintHierarchy(NkModelerPainter &p, const NkRect &r, int32 selectedRow,
								   float32 scroll) {
			p.Fill(r, NkRole::PanelBg);
			p.VLine(r.x + r.w - 1.f, r.y, r.h);
			float32 y = PaintPanelTab(p, r, "Hierarchie");
			y = PaintSearch(p, r, y);

			const float32 colEye = r.x + r.w - 74.f;
			const float32 colLock = r.x + r.w - 52.f;
			const float32 colType = r.x + r.w - 132.f;

			p.Fill({r.x, y, r.w, kRowH}, NkRole::WindowBg);
			p.TextV(r.x + 34.f, y, kRowH, "Nom");
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
					bool visible;
					bool locked;
			};
			static const Row kRows[] = {
				{0, "Scene", "", NkIcon::Globe, true, false, true, false},
				{1, "Cube", "Maillage", NkIcon::Mesh, false, false, true, false},
				{1, "Sphere", "Maillage", NkIcon::Circle, false, false, true, true},
				{1, "Groupe", "Dossier", NkIcon::Folder, true, true, true, false},
				{2, "Roue", "Maillage", NkIcon::Mesh, false, false, false, false},
				{2, "Axe", "Maillage", NkIcon::Mesh, false, false, true, false},
			};
			const int32 nRows = 6;
			const float32 listTop = y;
			const float32 listH = r.y + r.h - kRowH - listTop;
			y -= scroll;
			for (int32 i = 0; i < nRows; ++i) {
				const bool sel = (i == selectedRow);
				if (y >= listTop - kRowH && y < listTop + listH) {
					if (sel)
						p.Fill({r.x, y, r.w, kRowH}, NkRole::AccentUi);
					const NkRole fg = sel ? NkRole::TextOnAccent : NkRole::Text;
					const NkRole dim = sel ? NkRole::TextOnAccent : NkRole::TextMuted;
					float32 tx = r.x + 6.f + (float32)kRows[i].depth * 14.f;
					if (kRows[i].expandable)
						p.IconV(tx, y, kRowH, i == 0 ? NkIcon::ChevronDown : NkIcon::ChevronRight, dim, 12.f);
					tx += 14.f;
					// Le DOSSIER prend sa couleur propre, meme selectionne : c'est un
					// contenant, et c'est ce qui le distingue au premier coup d'oeil.
					if (kRows[i].folder)
						p.IconV(tx, y, kRowH, NkIcon::Folder, NkRole::TypeFolder, 13.f);
					else
						p.IconV(tx, y, kRowH, kRows[i].icon, sel ? NkRole::TextOnAccent : NkRole::Text, 13.f);
					p.TextV(tx + 18.f, y, kRowH, kRows[i].name, fg);
					if (*kRows[i].type)
						p.TextV(colType, y, kRowH, kRows[i].type, dim);
					// Etats. L'oeil FERME et le cadenas OUVERT restent dessines, en
					// attenue : une colonne qui se vide donne une liste en dents de scie
					// et on ne sait plus si l'etat est faux ou l'objet sans etat.
					p.IconV(colEye, y, kRowH, kRows[i].visible ? NkIcon::Eye : NkIcon::EyeClosed,
							kRows[i].visible ? fg : dim, 13.f);
					p.IconV(colLock, y, kRowH, kRows[i].locked ? NkIcon::Lock : NkIcon::Unlock,
							kRows[i].locked ? fg : dim, 13.f);
				}
				y += kRowH;
			}
			p.VScroll({r.x, listTop, r.w, listH}, (float32)nRows * kRowH, scroll);

			const float32 fy = r.y + r.h - kRowH;
			p.Fill({r.x, fy, r.w, kRowH}, NkRole::WindowBg);
			p.HLine(r.x, fy, r.w);
			p.TextV(r.x + kPad, fy, kRowH, "6 objets (1 selectionne)", NkRole::TextMuted);
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
		inline void PaintViewport(NkModelerPainter &p, const NkRect &r, bool editMode,
								  const NkShortcutTable &sc) {
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
			static const char *const kView[] = {"Perspective", "Eclaire", "Affichage"};
			float32 gw = 30.f;
			for (int32 i = 0; i < 3; ++i)
				gw += p.TextW(kView[i]) + 30.f;
			float32 bx = r.x + 10.f;
			p.Fill({bx, barY, gw, barH}, NkRole::PanelBg, 5.f);
			p.IconV(bx + 8.f, barY, barH, NkIcon::Layers, NkRole::Text, 13.f);
			bx += 30.f;
			for (int32 i = 0; i < 3; ++i) {
				p.TextV(bx, barY, barH, kView[i]);
				bx += p.TextW(kView[i]) + 6.f;
				p.IconV(bx, barY, barH, NkIcon::ChevronDown, NkRole::Text, 11.f);
				bx += 24.f;
			}

			// ── BARRE FLOTTANTE DROITE : sous-modes (en edition) + outils + aimantation.
			const float32 btn = 24.f;
			const int32 nSub = editMode ? 3 : 0;
			static const char *const kSnaps[] = {"0,5", "15 deg", "0,25"};
			float32 tw = 12.f + (float32)(nSub + 5) * (btn + 2.f) + 12.f;
			for (int32 i = 0; i < 3; ++i)
				tw += p.TextW(kSnaps[i]) + 12.f;
			float32 tx = r.x + r.w - 10.f - tw;
			p.Fill({tx, barY, tw, barH}, NkRole::PanelBg, 5.f);
			tx += 5.f;
			if (editMode) {
				// Sommet / arete / face. Le sous-mode ACTIF prend l'accent d'interface,
				// pas l'ambre : c'est un etat de l'outil, pas une selection dans la scene.
				const NkIcon kSub[3] = {NkIcon::Dot, NkIcon::Ruler, NkIcon::Square};
				for (int32 i = 0; i < 3; ++i) {
					const bool on = (i == 2);
					if (on)
						p.Fill({tx, barY + 2.f, btn, barH - 4.f}, NkRole::AccentUi, 3.f);
					p.IconV(tx + (btn - 14.f) * 0.5f, barY, barH, kSub[i],
							on ? NkRole::TextOnAccent : NkRole::Text, 14.f);
					tx += btn + 2.f;
				}
				p.VLine(tx + 1.f, barY + 6.f, barH - 12.f);
				tx += 5.f;
			}
			const NkIcon kTools[5] = {NkIcon::Cursor, NkIcon::Move, NkIcon::Rotate, NkIcon::Scale,
									  NkIcon::Globe};
			for (int32 i = 0; i < 5; ++i) {
				const bool on = (i == 1); // deplacement actif, comme dans la maquette
				if (on)
					p.Fill({tx, barY + 2.f, btn, barH - 4.f}, NkRole::AccentUi, 3.f);
				p.IconV(tx + (btn - 14.f) * 0.5f, barY, barH, kTools[i],
						on ? NkRole::TextOnAccent : NkRole::Text, 14.f);
				tx += btn + 2.f;
			}
			p.VLine(tx + 1.f, barY + 6.f, barH - 12.f);
			tx += 7.f;
			for (int32 i = 0; i < 3; ++i) {
				p.TextV(tx, barY, barH, kSnaps[i], NkRole::TextMuted);
				tx += p.TextW(kSnaps[i]) + 12.f;
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

			// ── OUTILS SELECTION ET CURSEUR, colonne de GAUCHE.
			// Ce sont les deux premiers outils de la barre T de Blender, et ils ne
			// sont pas de meme nature que les boutons de navigation : ceux-la
			// deplacent la VUE, ceux-ci changent ce que fait le CLIC. D'ou deux
			// groupes distincts, et non une seule colonne fourre-tout.
			// Un seul peut etre actif a la fois -- d'ou l'accent sur un seul.
			{
				const float32 d = 26.f;
				const float32 tx2 = r.x + 14.f;
				const float32 ty2 = barY + barH + 10.f;
				const NkIcon kT[2] = {NkIcon::Cursor, NkIcon::Gizmo};
				const int32 active = 0; // outil de selection
				for (int32 i = 0; i < 2; ++i) {
					const float32 by2 = ty2 + (float32)i * (d + 5.f);
					const bool on = (i == active);
					p.Fill({tx2, by2, d, d}, on ? NkRole::AccentUi : NkRole::PanelBg, 4.f);
					p.IconV(tx2 + (d - 14.f) * 0.5f, by2, d, kT[i],
							on ? NkRole::TextOnAccent : NkRole::Text, 14.f);
				}
			}

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
		inline void PaintProperties(NkModelerPainter &p, const NkRect &full, float32 scroll) {
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
			p.VScroll({full.x, listTop, full.w, full.y + full.h - listTop}, y - listTop + scroll, scroll);
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
		inline void PaintDetails(NkModelerPainter &p, const NkRect &full, float32 scroll) {
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
			p.VScroll({full.x, listTop, full.w, full.y + full.h - listTop}, y - listTop + scroll,
					  scroll);
		}

		// ── NAVIGATEUR DE PROJET (bas) ──────────────────────────────────────────
		inline void PaintBrowser(NkModelerPainter &p, const NkRect &r, float32 treeScroll,
								 float32 assetScroll) {
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
			// L'APERCU DIT LE TYPE, et pas seulement la bande de couleur. Un
			// utilisateur reconnait une forme avant de lire une etiquette ou de
			// decoder un code couleur -- et le code couleur seul echoue des qu'il y a
			// du daltonisme ou un ecran mal calibre.
			//   maillage  -> cube en fil de fer
			//   materiau  -> sphere (la boule de rendu, convention universelle)
			//   texture   -> damier (ce qu'on voit dans tous les logiciels)
			//   animation -> courbe avec ses cles
			enum class Preview : uint8 { Mesh = 0, Material, Texture, Animation };
			struct Asset {
					const char *name;
					NkRole role;
					Preview kind;
			};
			static const Asset kAssets[] = {
				{"Cube", NkRole::TypeMesh, Preview::Mesh},
				{"Tete", NkRole::TypeMesh, Preview::Mesh},
				{"Bois", NkRole::TypeMat, Preview::Material},
				{"Ecorce", NkRole::TypeTex, Preview::Texture},
				{"Marche", NkRole::TypeAnim, Preview::Animation},
			};
			float32 tx = ax;
			const float32 tw = 88.f, thh = 62.f, tyy = ty + 42.f;
			for (int32 i = 0; i < 5; ++i) {
				p.Fill({tx, tyy, tw, thh}, NkRole::PanelHeader, 2.f);
				// Apercu : une sphere pour un materiau, un cube en fil de fer sinon --
				// une vignette vide ne dirait rien de ce qu'elle contient.
				const float32 cx = tx + tw * 0.5f, cy = tyy + thh * 0.5f;
				switch (kAssets[i].kind) {
					case Preview::Material:
						// Boule de rendu : disque plein, avec un reflet clair en haut a
						// gauche. Sans le reflet, le disque se lit comme une pastille.
						p.Disc(cx, cy, 17.f, kAssets[i].role);
						p.Disc(cx - 6.f, cy - 6.f, 4.f, NkRole::Text);
						break;
					case Preview::Texture: {
						// Damier : ce qu'affichent tous les logiciels pour une texture.
						const float32 c = 7.f;
						for (int32 gx = 0; gx < 4; ++gx)
							for (int32 gy = 0; gy < 4; ++gy)
								if (((gx + gy) & 1) == 0)
									p.Fill({cx - c * 2.f + (float32)gx * c, cy - c * 2.f + (float32)gy * c,
											c, c},
										   kAssets[i].role);
						p.OutlineSharp({cx - c * 2.f, cy - c * 2.f, c * 4.f, c * 4.f}, NkRole::Border);
						break;
					}
					case Preview::Animation: {
						// Courbe avec ses cles : une animation est une valeur qui varie,
						// et c'est cette variation qu'il faut montrer.
						const float32 w2 = 17.f, h2 = 11.f;
						p.Line(cx - w2, cy + h2, cx - w2 * 0.3f, cy - h2 * 0.6f, kAssets[i].role, 2.f);
						p.Line(cx - w2 * 0.3f, cy - h2 * 0.6f, cx + w2 * 0.4f, cy + h2 * 0.3f,
							   kAssets[i].role, 2.f);
						p.Line(cx + w2 * 0.4f, cy + h2 * 0.3f, cx + w2, cy - h2, kAssets[i].role, 2.f);
						p.Disc(cx - w2 * 0.3f, cy - h2 * 0.6f, 3.f, NkRole::Text);
						p.Disc(cx + w2 * 0.4f, cy + h2 * 0.3f, 3.f, NkRole::Text);
						break;
					}
					default: {
						// Cube en fil de fer : la face avant, plus les trois aretes de
						// profondeur. Un carre plein ne dirait pas « volume ».
						const float32 hw = 14.f, hh = 12.f, dp = 7.f;
						p.Fill({cx - hw, cy - hh + dp, hw * 2.f, hh * 2.f - dp}, NkRole::InputBg);
						p.OutlineSharp({cx - hw, cy - hh + dp, hw * 2.f, hh * 2.f - dp},
									   NkRole::TypeMesh);
						p.Line(cx - hw, cy - hh + dp, cx - hw + dp, cy - hh, NkRole::TypeMesh);
						p.Line(cx - hw + dp, cy - hh, cx + hw + dp, cy - hh, NkRole::TypeMesh);
						p.Line(cx + hw, cy - hh + dp, cx + hw + dp, cy - hh, NkRole::TypeMesh);
						p.Line(cx + hw + dp, cy - hh, cx + hw + dp, cy + hh - dp, NkRole::TypeMesh);
						p.Line(cx + hw, cy + hh, cx + hw + dp, cy + hh - dp, NkRole::TypeMesh);
						break;
					}
				}
				p.Fill({tx, tyy + thh - 3.f, tw, 3.f}, kAssets[i].role);
				const float32 nw = p.TextW(kAssets[i].name);
				p.Text(tx + tw * 0.5f - nw * 0.5f, tyy + thh + 5.f, kAssets[i].name);
				tx += tw + 10.f;
			}
			p.VScroll({r.x, ty, treeW, th}, 5.f * kRowH + 8.f, treeScroll);
			p.VScroll({ax - 10.f, ty + 33.f, r.w - treeW - 10.f, th - 33.f}, thh + 26.f, assetScroll);
			const float32 ew = p.TextW("5 elements");
			p.TextV(r.x + r.w - ew - kPad, r.y + r.h - kRowH, kRowH, "5 elements", NkRole::TextMuted);
			(void)ih;
		}

		// ── BARRE D'ETAT ────────────────────────────────────────────────────────
		inline void PaintStatus(NkModelerPainter &p, const NkRect &r, const char *stats) {
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
