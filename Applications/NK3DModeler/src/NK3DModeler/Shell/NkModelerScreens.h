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
				const float32 bx = r.w - kPad - (float32)(3 - i) * (bw + 4.f);
				const bool close = (i == 2);
				if (close)
					p.Fill({bx, by, bw, bh}, NkColor{231, 76, 60, 255}, 3.f);
				p.IconV(bx + (bw - p.IconSize()) * 0.5f, by, bh, kWin[i],
						close ? NkRole::TextOnAccent : NkRole::TextMuted);
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

		// ── BARRE D'OUTILS (amincie) ────────────────────────────────────────────
		inline void PaintToolbar(NkModelerPainter &p, const NkRect &r, bool editMode) {
			p.Fill(r, NkRole::PanelHeader);
			p.HLine(r.x, r.y + r.h - 1.f, r.w);
			float32 x = kPad;
			const float32 ih = p.IconSize();

			auto btn = [&](NkIcon ic, const char *label) {
				p.IconV(x, r.y, r.h, ic, NkRole::Text);
				p.TextV(x + ih + 5.f, r.y, r.h, label);
				x += ih + 5.f + p.TextW(label) + 18.f;
			};
			btn(NkIcon::Save, "Enregistrer");
			p.VLine(x - 9.f, r.y + 7.f, r.h - 14.f);

			// LE COMMUTATEUR DE MODE — la meilleure idee de la maquette Banani, et la
			// seule chose que je garde de la barre qu'elle avait ajoutee : chez Blender
			// ce mode n'est visible que dans un menu deroulant, ce qui est la premiere
			// source de confusion pour un debutant.
			const float32 segH = 22.f, segY = r.y + (r.h - segH) * 0.5f;
			static const char *const kModes[] = {"Objet", "Edition"};
			const NkIcon kModeIc[2] = {NkIcon::Mesh, NkIcon::Edit};
			for (int32 i = 0; i < 2; ++i) {
				const float32 w = ih + 8.f + p.TextW(kModes[i]) + 18.f;
				const bool on = ((i == 1) == editMode);
				if (on)
					p.Fill({x, segY, w, segH}, NkRole::AccentUi, 3.f);
				p.IconV(x + 7.f, segY, segH, kModeIc[i], on ? NkRole::TextOnAccent : NkRole::Text);
				p.TextV(x + 7.f + ih + 5.f, segY, segH, kModes[i],
						on ? NkRole::TextOnAccent : NkRole::Text);
				x += w + 4.f;
			}
			x += 10.f;
			p.VLine(x - 9.f, r.y + 7.f, r.h - 14.f);

			p.IconV(x, r.y, r.h, NkIcon::Cursor);
			p.TextV(x + ih + 5.f, r.y, r.h, "Mode de selection");
			x += ih + 5.f + p.TextW("Mode de selection") + 6.f;
			p.IconV(x, r.y, r.h, NkIcon::ChevronDown, NkRole::TextMuted, 12.f);
			x += 22.f;
			btn(NkIcon::Add, "Ajouter");
			btn(NkIcon::Layers, "Modificateur");

			// Reglages calE A DROITE : c'est une action de session, pas une action de
			// modelisation. La distance visuelle dit la difference de nature.
			const float32 sw = ih + 5.f + p.TextW("Reglages");
			p.IconV(r.w - kPad - sw, r.y, r.h, NkIcon::Gear);
			p.TextV(r.w - kPad - sw + ih + 5.f, r.y, r.h, "Reglages");
		}

		// ── EN-TETE DE PANNEAU ──────────────────────────────────────────────────
		// Onglet + croix, comme la maquette. Rendu ici une seule fois : quatre
		// panneaux le partagent, et il n'y a donc qu'un endroit a corriger.
		inline float32 PaintPanelTab(NkModelerPainter &p, const NkRect &r, const char *title) {
			const float32 h = 26.f;
			p.Fill({r.x, r.y, r.w, h}, NkRole::PanelHeader);
			p.TextV(r.x + kPad, r.y, h, title);
			p.IconV(r.x + r.w - 20.f, r.y, h, NkIcon::WinClose, NkRole::TextMuted, 11.f);
			p.HLine(r.x, r.y + h - 1.f, r.w);
			return r.y + h;
		}

		// Champ de recherche, avec sa loupe. Sans l'icone, un champ vide au texte
		// grise se confond avec une etiquette desactivee.
		inline float32 PaintSearch(NkModelerPainter &p, const NkRect &r, float32 y) {
			const float32 h = 22.f;
			p.Fill({r.x + 6.f, y + 4.f, r.w - 12.f, h}, NkRole::InputBg, 2.f);
			p.IconV(r.x + 12.f, y + 4.f, h, NkIcon::Search, NkRole::TextMuted, 12.f);
			p.TextV(r.x + 30.f, y + 4.f, h, "Rechercher", NkRole::TextMuted);
			return y + h + 8.f;
		}

		// ── HIERARCHIE (gauche) ─────────────────────────────────────────────────
		inline void PaintHierarchy(NkModelerPainter &p, const NkRect &r, int32 selectedRow) {
			p.Fill(r, NkRole::PanelBg);
			p.VLine(r.x + r.w - 1.f, r.y, r.h);
			float32 y = PaintPanelTab(p, r, "Hierarchie");
			y = PaintSearch(p, r, y);

			p.Fill({r.x, y, r.w, kRowH}, NkRole::WindowBg);
			p.TextV(r.x + 34.f, y, kRowH, "Nom");
			p.TextV(r.x + r.w - 62.f, y, kRowH, "Type", NkRole::TextMuted);
			p.HLine(r.x, y + kRowH - 1.f, r.w);
			y += kRowH;

			struct Row {
					int32 depth;
					const char *name;
					const char *type;
					NkIcon icon;
					bool expandable;
			};
			static const Row kRows[] = {
				{0, "Scene", "", NkIcon::Globe, true},
				{1, "Cube", "Maillage", NkIcon::Mesh, false},
				{1, "Sphere", "Maillage", NkIcon::Circle, false},
				{1, "Groupe", "Dossier", NkIcon::Folder, true},
				{2, "Roue", "Maillage", NkIcon::Mesh, false},
				{2, "Axe", "Maillage", NkIcon::Mesh, false},
			};
			for (int32 i = 0; i < 6; ++i) {
				const bool sel = (i == selectedRow);
				if (sel)
					p.Fill({r.x, y, r.w, kRowH}, NkRole::AccentUi);
				const NkRole fg = sel ? NkRole::TextOnAccent : NkRole::Text;
				const NkRole dim = sel ? NkRole::TextOnAccent : NkRole::TextMuted;
				float32 tx = r.x + 6.f + (float32)kRows[i].depth * 14.f;
				// Le chevron n'est dessine QUE si la ligne a des enfants. En mettre un
				// partout ferait croire que toutes les lignes se deplient.
				if (kRows[i].expandable)
					p.IconV(tx, y, kRowH, i == 0 ? NkIcon::ChevronDown : NkIcon::ChevronRight, dim, 12.f);
				tx += 14.f;
				p.IconV(tx, y, kRowH, kRows[i].icon, dim, 13.f);
				p.TextV(tx + 18.f, y, kRowH, kRows[i].name, fg);
				if (*kRows[i].type)
					p.TextV(r.x + r.w - 62.f, y, kRowH, kRows[i].type, dim);
				y += kRowH;
			}

			const float32 fy = r.y + r.h - kRowH;
			p.Fill({r.x, fy, r.w, kRowH}, NkRole::WindowBg);
			p.HLine(r.x, fy, r.w);
			p.TextV(r.x + kPad, fy, kRowH, "6 objets (1 selectionne)", NkRole::TextMuted);
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
				p.IconV(bx, barY, barH, NkIcon::ChevronDown, NkRole::TextMuted, 11.f);
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

			// ── REPERE D'AXES. Couleurs prises dans le THEME : en clair ce sont
			// d'autres valeurs, assombries pour rester lisibles sur fond pale.
			const float32 ax = r.x + 44.f, ay = r.y + r.h - 46.f, len = 26.f;
			p.Line(ax, ay, ax + len, ay - 7.f, NkRole::AxisX, 2.f);
			p.Text(ax + len + 4.f, ay - 16.f, "X", NkRole::AxisX);
			p.Line(ax, ay, ax, ay - len, NkRole::AxisY, 2.f);
			p.Text(ax - 4.f, ay - len - 16.f, "Y", NkRole::AxisY);
			p.Line(ax, ay, ax - len * 0.7f, ay + len * 0.5f, NkRole::AxisZ, 2.f);
			p.Text(ax - len * 0.7f - 12.f, ay + len * 0.5f - 4.f, "Z", NkRole::AxisZ);

			// ── PANNEAU DE DERNIERE OPERATION. Il FLOTTE au-dessus de la scene et n'est
			// pas encastre dans un bord : il appartient a la vue, pas au cadre.
			if (editMode) {
				const float32 pw = 214.f, ph = 4.f * kRowH + 6.f;
				const float32 px = r.x + 12.f, py = r.y + r.h - ph - 80.f;
				p.Fill({px, py, pw, ph}, NkRole::PanelHeader, 4.f);
				p.IconV(px + 6.f, py, kRowH, NkIcon::ChevronDown, NkRole::TextMuted, 11.f);
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
		inline void PaintProperties(NkModelerPainter &p, const NkRect &r) {
			p.Fill(r, NkRole::PanelBg);
			p.VLine(r.x, r.y, r.h);
			float32 y = PaintPanelTab(p, r, "Proprietes");
			y = PaintSearch(p, r, y);

			// Pastilles de filtre : CONTOUREES, sauf l'active qui est pleine. Toutes
			// pleines, on ne verrait plus laquelle est choisie.
			static const char *const kPills[] = {"General", "Objet", "Rendu", "Physique", "Tout"};
			float32 x = r.x + 6.f;
			for (int32 i = 0; i < 5; ++i) {
				const float32 w = p.TextW(kPills[i]) + 18.f;
				const bool on = (i == 4);
				if (on)
					p.Fill({x, y, w, 20.f}, NkRole::AccentUi, 10.f);
				else
					p.Outline({x, y, w, 20.f}, NkRole::Border, 10.f);
				p.TextV(x + 9.f, y, 20.f, kPills[i], on ? NkRole::TextOnAccent : NkRole::TextMuted);
				x += w + 4.f;
			}
			y += 26.f;
			p.HLine(r.x, y, r.w);
			y += 1.f;

			p.Fill({r.x, y, r.w, kRowH}, NkRole::PanelBg);
			p.IconV(r.x + 6.f, y, kRowH, NkIcon::ChevronDown, NkRole::TextMuted, 11.f);
			p.TextV(r.x + 22.f, y, kRowH, "Transformation");
			y += kRowH;
			PaintVec3Row(p, r, y, "Position", "0,00", "0,00", "0,00", NkIcon::Refresh);
			y += kRowH;
			PaintVec3Row(p, r, y, "Rotation", "0,00", "0,00", "0,00", NkIcon::None);
			y += kRowH;
			PaintVec3Row(p, r, y, "Echelle", "1,00", "1,00", "1,00", NkIcon::Lock);
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
		inline void PaintDetails(NkModelerPainter &p, const NkRect &r) {
			p.Fill(r, NkRole::PanelBg);
			p.VLine(r.x, r.y, r.h);
			p.HLine(r.x, r.y, r.w);
			float32 y = PaintPanelTab(p, r, "Details (Cube)");

			p.IconV(r.x + 6.f, y, kRowH, NkIcon::ChevronDown, NkRole::TextMuted, 11.f);
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

			p.IconV(r.x + 6.f, y, kRowH, NkIcon::ChevronDown, NkRole::TextMuted, 11.f);
			p.TextV(r.x + 22.f, y, kRowH, "Modificateurs");
			y += kRowH;
			// Le deroulant. Bordure + chevron a droite, comme tout selecteur.
			p.Fill({r.x + 8.f, y + 2.f, r.w - 16.f, kRowH - 4.f}, NkRole::InputBg, 2.f);
			p.Outline({r.x + 8.f, y + 2.f, r.w - 16.f, kRowH - 4.f}, NkRole::Border);
			p.TextV(r.x + 16.f, y, kRowH, "Selectionner un modificateur", NkRole::TextMuted);
			p.IconV(r.x + r.w - 26.f, y, kRowH, NkIcon::ChevronDown, NkRole::TextMuted, 11.f);
			y += kRowH + 4.f;

			p.IconV(r.x + 6.f, y, kRowH, NkIcon::ChevronRight, NkRole::TextMuted, 11.f);
			p.TextV(r.x + 22.f, y, kRowH, "Materiau");
		}

		// ── NAVIGATEUR DE PROJET (bas) ──────────────────────────────────────────
		inline void PaintBrowser(NkModelerPainter &p, const NkRect &r) {
			p.Fill(r, NkRole::PanelBg);
			p.HLine(r.x, r.y, r.w);

			const float32 topH = 28.f;
			const float32 ih = p.IconSize();
			p.Fill({r.x, r.y, r.w, topH}, NkRole::PanelHeader);
			p.TextV(r.x + kPad, r.y, topH, "Navigateur de projet");
			float32 x = r.x + kPad + p.TextW("Navigateur de projet") + 10.f;
			p.IconV(x, r.y, topH, NkIcon::WinClose, NkRole::TextMuted, 11.f);
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
			p.IconV(x, r.y, topH, NkIcon::ArrowLeft, NkRole::TextMuted, 13.f);
			p.IconV(x + 22.f, r.y, topH, NkIcon::ArrowRight, NkRole::TextMuted, 13.f);
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
			float32 dy = ty + 4.f;
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
			p.IconV(ax + 6.f, ty + 6.f, 20.f, NkIcon::Search, NkRole::TextMuted, 12.f);
			p.TextV(ax + 24.f, ty + 6.f, 20.f, "Rechercher", NkRole::TextMuted);
			float32 fx = ax + 168.f;
			static const char *const kTypes[] = {"Maillage", "Animation", "Materiau", "Texture"};
			const NkRole kTypeRoles[] = {NkRole::TypeMesh, NkRole::TypeAnim, NkRole::TypeMat,
										 NkRole::TypeTex};
			for (int32 i = 0; i < 4; ++i) {
				const float32 w = p.TextW(kTypes[i]) + 30.f;
				p.Outline({fx, ty + 6.f, w, 20.f}, NkRole::Border, 10.f);
				p.Fill({fx + 8.f, ty + 13.f, 7.f, 7.f}, kTypeRoles[i], 4.f);
				p.TextV(fx + 20.f, ty + 6.f, 20.f, kTypes[i], NkRole::TextMuted);
				fx += w + 6.f;
			}
			p.HLine(ax - 10.f, ty + 32.f, r.w - treeW);

			// Vignettes. La bande de couleur SOUS la vignette redit le type : on
			// reconnait un materiau d'un maillage sans lire l'etiquette.
			struct Asset {
					const char *name;
					NkRole role;
					bool sphere;
			};
			static const Asset kAssets[] = {
				{"Cube", NkRole::TypeMesh, false}, {"Tete", NkRole::TypeMesh, false},
				{"Bois", NkRole::TypeMat, true},   {"Marche", NkRole::TypeMesh, false},
				{"Roche", NkRole::TypeMesh, false},
			};
			float32 tx = ax;
			const float32 tw = 88.f, thh = 62.f, tyy = ty + 42.f;
			for (int32 i = 0; i < 5; ++i) {
				p.Fill({tx, tyy, tw, thh}, NkRole::PanelHeader, 2.f);
				// Apercu : une sphere pour un materiau, un cube en fil de fer sinon --
				// une vignette vide ne dirait rien de ce qu'elle contient.
				const float32 cx = tx + tw * 0.5f, cy = tyy + thh * 0.5f;
				if (kAssets[i].sphere) {
					p.Disc(cx, cy, 17.f, kAssets[i].role);
				} else {
					const float32 hw = 15.f, hh = 13.f, dp = 7.f;
					p.Fill({cx - hw, cy - hh + dp, hw * 2.f, hh * 2.f - dp}, NkRole::InputBg);
					p.Line(cx - hw, cy - hh + dp, cx - hw + dp, cy - hh, NkRole::TextMuted);
					p.Line(cx - hw + dp, cy - hh, cx + hw + dp, cy - hh, NkRole::TextMuted);
					p.Line(cx + hw, cy - hh + dp, cx + hw + dp, cy - hh, NkRole::TextMuted);
					p.Line(cx + hw + dp, cy - hh, cx + hw + dp, cy + hh - dp, NkRole::TextMuted);
					p.Line(cx + hw, cy + hh, cx + hw + dp, cy + hh - dp, NkRole::TextMuted);
				}
				p.Fill({tx, tyy + thh - 3.f, tw, 3.f}, kAssets[i].role);
				const float32 nw = p.TextW(kAssets[i].name);
				p.Text(tx + tw * 0.5f - nw * 0.5f, tyy + thh + 5.f, kAssets[i].name);
				tx += tw + 10.f;
			}
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
			p.IconV(x + 6.f, r.y, r.h, NkIcon::Terminal, NkRole::TextMuted, 12.f);
			p.TextV(x + 24.f, r.y, r.h, "Entrer une commande", NkRole::TextMuted);
			const float32 w = p.TextW(stats);
			p.TextV(r.x + r.w - w - kPad, r.y, r.h, stats, NkRole::TextMuted);
		}

	} // namespace nk3d
} // namespace nkentseu
