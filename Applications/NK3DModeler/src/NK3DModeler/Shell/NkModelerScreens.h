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

			// Logo hexagonal. Dessine en losange faute de primitive polygonale ici :
			// la forme exacte viendra avec l'icone reelle, extraite comme les autres.
			const float32 logo = 22.f;
			const float32 ly = r.y + (r.h - logo) * 0.5f;
			p.Fill({12.f, ly, logo, logo}, NkRole::AccentUi, 4.f);
			p.TextV(12.f + 4.f, ly, logo, "NK", NkRole::TextOnAccent);

			float32 x = 12.f + logo + 14.f;
			static const char *const kMenus[] = {"Fichier", "Edition", "Fenetre", "Outils",
												 "Selection", "Objet", "Aide"};
			for (int32 i = 0; i < 7; ++i) {
				p.TextV(x, r.y, r.h, kMenus[i]);
				x += p.TextW(kMenus[i]) + 20.f;
			}

			// Nom du projet CENTRE sur la fenetre, pas apres les menus : il doit
			// rester au meme endroit quelle que soit la langue de l'interface.
			const float32 nw = p.TextW(projectName);
			p.TextV(r.w * 0.5f - nw * 0.5f, r.y, r.h, projectName, NkRole::TextMuted);

			// Boutons de fenetre. Sans bordure OS (fenetre sans cadre), c'est nous qui
			// les portons -- comme dans la maquette.
			const float32 bw = 30.f, bh = 22.f;
			const float32 by = r.y + (r.h - bh) * 0.5f;
			float32 bx = r.w - kPad - bw;
			p.Fill({bx, by, bw, bh}, NkColor{231, 76, 60, 255}, 3.f);
			p.TextV(bx + bw * 0.5f - 4.f, by, bh, "X", NkRole::TextOnAccent);
			bx -= bw + 6.f;
			p.Fill({bx, by, bw, bh}, NkRole::PanelBg, 3.f);
			p.TextV(bx + bw * 0.5f - 4.f, by, bh, "[]");
			bx -= bw + 6.f;
			p.Fill({bx, by, bw, bh}, NkRole::PanelBg, 3.f);
			p.TextV(bx + bw * 0.5f - 3.f, by, bh, "-");
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

			// LE COMMUTATEUR DE MODE — la seule chose que je garde de la barre que
			// Banani avait ajoutee, et la meilleure idee de cette maquette : chez
			// Blender ce mode n'est visible que dans un menu deroulant, ce qui est la
			// premiere source de confusion pour un debutant.
			const float32 segH = 24.f, segY = r.y + (r.h - segH) * 0.5f;
			static const char *const kModes[] = {"Objet", "Edition"};
			for (int32 i = 0; i < 2; ++i) {
				const float32 w = p.TextW(kModes[i]) + 24.f;
				const bool on = (i == 1) == editMode;
				p.Fill({x, segY, w, segH}, on ? NkRole::AccentUi : NkRole::PanelBg, i == 0 ? 3.f : 0.f);
				p.TextV(x + 12.f, segY, segH, kModes[i], on ? NkRole::TextOnAccent : NkRole::Text);
				x += w;
			}
			x += 16.f;
			static const char *const kBtns[] = {"Ajouter", "Modificateur"};
			for (int32 i = 0; i < 2; ++i) {
				p.TextV(x, r.y, r.h, kBtns[i]);
				x += p.TextW(kBtns[i]) + 20.f;
			}
		}

		// ── EN-TETE DE PANNEAU ──────────────────────────────────────────────────
		// Onglet + croix, comme la maquette. Rendu ici une seule fois : quatre
		// panneaux le partagent, et il n'y a donc qu'un endroit a corriger.
		inline float32 PaintPanelTab(NkModelerPainter &p, const NkRect &r, const char *title) {
			const float32 h = 26.f;
			p.Fill({r.x, r.y, r.w, h}, NkRole::PanelHeader);
			p.TextV(r.x + kPad, r.y, h, title);
			p.TextV(r.x + r.w - 18.f, r.y, h, "x", NkRole::TextMuted);
			p.HLine(r.x, r.y + h - 1.f, r.w);
			return r.y + h;
		}

		// Champ de recherche.
		inline float32 PaintSearch(NkModelerPainter &p, const NkRect &r, float32 y) {
			const float32 h = 22.f;
			p.Fill({r.x + 6.f, y + 4.f, r.w - 12.f, h}, NkRole::InputBg, 2.f);
			p.TextV(r.x + 14.f, y + 4.f, h, "Rechercher...", NkRole::TextMuted);
			return y + h + 8.f;
		}

		// ── HIERARCHIE (gauche) ─────────────────────────────────────────────────
		inline void PaintHierarchy(NkModelerPainter &p, const NkRect &r, int32 selectedRow) {
			p.Fill(r, NkRole::PanelBg);
			p.VLine(r.x + r.w - 1.f, r.y, r.h);
			float32 y = PaintPanelTab(p, r, "Hierarchie");
			y = PaintSearch(p, r, y);

			// En-tete de colonnes.
			p.Fill({r.x, y, r.w, kRowH}, NkRole::WindowBg);
			p.TextV(r.x + 30.f, y, kRowH, "Nom", NkRole::Text);
			p.TextV(r.x + r.w - 60.f, y, kRowH, "Type", NkRole::TextMuted);
			p.HLine(r.x, y + kRowH - 1.f, r.w);
			y += kRowH;

			struct Row {
					int32 depth;
					const char *name;
					const char *type;
			};
			static const Row kRows[] = {
				{0, "Scene", ""},		 {1, "Cube", "Maillage"},  {1, "Sphere", "Maillage"},
				{1, "Groupe", "Dossier"}, {2, "Roue", "Maillage"}, {2, "Axe", "Maillage"},
			};
			for (int32 i = 0; i < 6; ++i) {
				const bool sel = (i == selectedRow);
				if (sel)
					p.Fill({r.x, y, r.w, kRowH}, NkRole::AccentUi);
				const float32 tx = r.x + 8.f + (float32)kRows[i].depth * 14.f;
				p.TextV(tx, y, kRowH, kRows[i].depth == 0 ? "v" : " ", NkRole::TextMuted);
				p.TextV(tx + 14.f, y, kRowH, kRows[i].name, sel ? NkRole::TextOnAccent : NkRole::Text);
				if (*kRows[i].type)
					p.TextV(r.x + r.w - 60.f, y, kRowH, kRows[i].type,
							sel ? NkRole::TextOnAccent : NkRole::TextMuted);
				y += kRowH;
			}

			// Pied : le compte. Il vaut mieux qu'un panneau vide dise combien il
			// contient plutot que de laisser l'utilisateur se demander s'il a charge.
			const float32 fy = r.y + r.h - kRowH;
			p.Fill({r.x, fy, r.w, kRowH}, NkRole::WindowBg);
			p.HLine(r.x, fy, r.w);
			p.TextV(r.x + kPad, fy, kRowH, "6 objets (1 selectionne)", NkRole::TextMuted);
		}

		// ── VUE 3D (centre) ─────────────────────────────────────────────────────
		inline void PaintViewport(NkModelerPainter &p, const NkRect &r, bool editMode,
								  const NkShortcutTable &sc) {
			// Degrade haut/bas : deux roles distincts, donc pilotables par theme.
			p.Fill(r, NkRole::ViewportTop);
			p.Fill({r.x, r.y + r.h * 0.5f, r.w, r.h * 0.5f}, NkRole::ViewportBottom);

			// Sol quadrille en perspective, purement indicatif tant que la vue reelle
			// n'est pas branchee -- mais il donne l'echelle et le sens de la scene.
			const float32 hy = r.y + r.h * 0.55f;
			for (int32 i = 0; i < 7; ++i) {
				const float32 t = (float32)i / 6.f;
				const float32 yy = hy + (r.h - (hy - r.y)) * t * t * 0.75f;
				p.Fill({r.x, yy, r.w, 1.f}, NkRole::GridLine);
			}
			for (int32 i = -3; i <= 3; ++i) {
				const float32 x0 = r.x + r.w * 0.5f;
				const float32 x1 = x0 + (float32)i * r.w * 0.16f;
				p.Fill({x1 < x0 ? x1 : x0, hy, 1.f + (x1 > x0 ? x1 - x0 : x0 - x1), 1.f}, NkRole::GridLine);
			}

			// ── BARRE FLOTTANTE : les menus de commandes (UI_SPEC 9bis, chemin 1).
			// Leur contenu CHANGE avec le mode -- c'est ce qui rend le mode lisible
			// sans avoir a le chercher.
			const float32 barH = 28.f, barY = r.y + 10.f;
			float32 bx = r.x + 10.f;
			static const char *const kObj[] = {"Ajouter", "Objet", "Selection"};
			static const char *const kEdit[] = {"Ajouter", "Maillage", "Sommet", "Arete", "Face"};
			const char *const *menus = editMode ? kEdit : kObj;
			const int32 nMenus = editMode ? 5 : 3;
			float32 gw = 0.f;
			for (int32 i = 0; i < nMenus; ++i)
				gw += p.TextW(menus[i]) + 26.f;
			p.Fill({bx, barY, gw, barH}, NkRole::PanelBg, 6.f);
			for (int32 i = 0; i < nMenus; ++i) {
				p.TextV(bx + 10.f, barY, barH, menus[i]);
				bx += p.TextW(menus[i]) + 26.f;
			}

			// Groupe de vue, cale a DROITE.
			static const char *const kView[] = {"Perspective", "Eclaire", "Affichage"};
			float32 vw = 0.f;
			for (int32 i = 0; i < 3; ++i)
				vw += p.TextW(kView[i]) + 26.f;
			float32 vx = r.x + r.w - 10.f - vw;
			p.Fill({vx, barY, vw, barH}, NkRole::PanelBg, 6.f);
			for (int32 i = 0; i < 3; ++i) {
				p.TextV(vx + 10.f, barY, barH, kView[i]);
				vx += p.TextW(kView[i]) + 26.f;
			}

			// ── REPERE D'AXES, en bas a gauche. Les couleurs viennent du THEME : en
			// clair ce sont d'autres valeurs, assombries pour rester lisibles.
			const float32 ax = r.x + 42.f, ay = r.y + r.h - 46.f, len = 22.f;
			p.Fill({ax, ay, len, 2.f}, NkRole::AxisX);
			p.Text(ax + len + 4.f, ay - 7.f, "X", NkRole::AxisX);
			p.Fill({ax, ay - len, 2.f, len}, NkRole::AxisY);
			p.Text(ax - 3.f, ay - len - 16.f, "Y", NkRole::AxisY);
			p.Fill({ax - len, ay + 8.f, len, 2.f}, NkRole::AxisZ);
			p.Text(ax - len - 12.f, ay + 2.f, "Z", NkRole::AxisZ);

			// ── PANNEAU DE DERNIERE OPERATION (chemin 4). En bas a GAUCHE, flottant
			// au-dessus de la scene et non encastre dans un bord : il appartient a la
			// vue, pas au cadre.
			if (editMode) {
				const float32 pw = 210.f, ph = 4.f * kRowH + 8.f;
				const float32 px = r.x + 12.f, py = r.y + r.h - ph - 70.f;
				p.Fill({px, py, pw, ph}, NkRole::PanelHeader, 4.f);
				p.TextV(px + kPad, py, kRowH, "v Extruder la region");
				float32 ry = py + kRowH;
				static const char *const kL[] = {"Distance", "Decalage"};
				static const char *const kV[] = {"0,25", "0,00"};
				for (int32 i = 0; i < 2; ++i) {
					p.TextV(px + kPad, ry, kRowH, kL[i], NkRole::TextMuted);
					p.Fill({px + 110.f, ry + 3.f, 80.f, 16.f}, NkRole::InputBg, 2.f);
					p.TextV(px + 116.f, ry, kRowH, kV[i]);
					ry += kRowH;
				}
				p.Fill({px + kPad, ry + 5.f, 12.f, 12.f}, NkRole::AccentUi, 2.f);
				p.TextV(px + kPad + 18.f, ry, kRowH, "Decalage pair", NkRole::TextMuted);
			}

			// Le raccourci de l'operation courante, lu dans la TABLE et jamais ecrit
			// en dur : c'est ce qui garantit que l'affichage suit une reliaison faite
			// par l'utilisateur. FormatFor prend la CLE DE COMMANDE, pas un index --
			// la cle est stable, l'index bouge des qu'on ajoute une liaison.
			{
				const char *cmd = editMode ? "edit.extruder" : "objet.deplacer";
				char keys[32];
				if (sc.FormatFor(cmd, keys, sizeof(keys))) {
					char hint[96];
					snprintf(hint, sizeof(hint), "%s   %s", editMode ? "Extruder" : "Deplacer", keys);
					p.TextV(r.x + 12.f, r.y + r.h - 26.f, 20.f, hint, NkRole::TextMuted);
				}
			}
		}

		// ── LIGNE DE PROPRIETE A TROIS CHAMPS ───────────────────────────────────
		// Le liseré de couleur d'axe est sur le SEUL BORD GAUCHE du champ, le fond
		// restant neutre. Teinter le fond entier rendrait les trois champs criards
		// et illisibles -- c'est explicitement un critere de refus de maquette.
		inline void PaintVec3Row(NkModelerPainter &p, const NkRect &r, float32 y, const char *label,
								 const char *v0, const char *v1, const char *v2) {
			p.Fill({r.x, y, kLabelW, kRowH}, NkRole::LabelCol);
			p.TextV(r.x + kPad, y, kRowH, label);
			p.VLine(r.x + kLabelW, y, kRowH);
			const NkRole axes[3] = {NkRole::AxisX, NkRole::AxisY, NkRole::AxisZ};
			const char *vals[3] = {v0, v1, v2};
			const float32 fw = (r.w - kLabelW - 34.f) / 3.f;
			float32 x = r.x + kLabelW + 5.f;
			for (int32 i = 0; i < 3; ++i) {
				p.Fill({x, y + 2.f, fw - 4.f, kRowH - 4.f}, NkRole::InputBg, 2.f);
				p.Fill({x, y + 2.f, 2.f, kRowH - 4.f}, axes[i]); // le liseré, bord gauche
				p.TextV(x + 7.f, y, kRowH, vals[i]);
				x += fw;
			}
			p.HLine(r.x, y + kRowH - 1.f, r.w);
		}

		// ── PROPRIETES (droite, haut) ───────────────────────────────────────────
		inline void PaintProperties(NkModelerPainter &p, const NkRect &r) {
			p.Fill(r, NkRole::PanelBg);
			p.VLine(r.x, r.y, r.h);
			float32 y = PaintPanelTab(p, r, "Proprietes");
			y = PaintSearch(p, r, y);

			// Pastilles de filtre. Arrondies a fond, la derniere est ACTIVE.
			static const char *const kPills[] = {"General", "Objet", "Rendu", "Physique", "Tout"};
			float32 x = r.x + 6.f;
			for (int32 i = 0; i < 5; ++i) {
				const float32 w = p.TextW(kPills[i]) + 18.f;
				const bool on = (i == 4);
				if (on)
					p.Fill({x, y, w, 20.f}, NkRole::AccentUi, 10.f);
				p.TextV(x + 9.f, y, 20.f, kPills[i], on ? NkRole::TextOnAccent : NkRole::TextMuted);
				x += w + 4.f;
			}
			y += 26.f;
			p.HLine(r.x, y, r.w);
			y += 1.f;

			p.Fill({r.x, y, r.w, kRowH}, NkRole::PanelHeader);
			p.TextV(r.x + kPad, y, kRowH, "v Transformation");
			y += kRowH;
			PaintVec3Row(p, r, y, "Position", "0,00", "0,00", "0,00");
			y += kRowH;
			PaintVec3Row(p, r, y, "Rotation", "0,00", "0,00", "0,00");
			y += kRowH;
			PaintVec3Row(p, r, y, "Echelle", "1,00", "1,00", "1,00");
		}

		// ── DETAILS + PILE DE MODIFICATEURS (droite, bas) ───────────────────────
		inline void PaintDetails(NkModelerPainter &p, const NkRect &r) {
			p.Fill(r, NkRole::PanelBg);
			p.VLine(r.x, r.y, r.h);
			p.HLine(r.x, r.y, r.w);
			float32 y = PaintPanelTab(p, r, "Details (Cube)");

			p.Fill({r.x, y, r.w, kRowH}, NkRole::PanelHeader);
			p.TextV(r.x + kPad, y, kRowH, "v Maillage");
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

			// ── LA PILE. Banani l'avait faite en menu deroulant ; c'est faux, et le
			// dire ici plutot que dans un commentaire de commit evite qu'on y revienne.
			// L'ORDRE EST SIGNIFIANT : un miroir place apres une subdivision ne donne
			// pas le meme maillage qu'avant. D'ou les fleches de reordonnancement, que
			// jamais un menu deroulant ne pourra offrir.
			p.Fill({r.x, y, r.w, kRowH}, NkRole::PanelHeader);
			p.TextV(r.x + kPad, y, kRowH, "v Modificateurs");
			{
				const char *add = "+ Ajouter";
				const float32 w = p.TextW(add) + 16.f;
				p.Fill({r.x + r.w - w - 6.f, y + 3.f, w, kRowH - 6.f}, NkRole::AccentUi, 3.f);
				p.TextV(r.x + r.w - w - 6.f + 8.f, y, kRowH, add, NkRole::TextOnAccent);
			}
			y += kRowH;

			struct Mod {
					const char *name;
					bool on;
					const char *p1;
					const char *v1;
			};
			static const Mod kMods[] = {
				{"Subdivision", true, "Niveaux", "2"},
				{"Miroir", true, "Axe", "X"},
				{"Tableau", false, "Compte", "3"},
			};
			for (int32 i = 0; i < 3; ++i) {
				// La pastille d'etat prend un ROLE PROPRE AU PRODUIT : actif =
				// l'accent d'interface (c'est un etat de l'interface, pas une
				// selection 3D), inactif = le texte attenue.
				const uint16 role = kMods[i].on ? p.Roles().modifierOn : p.Roles().modifierOff;
				p.Fill({r.x + 6.f, y + 5.f, 12.f, 12.f}, p.C(role), 2.f);
				p.TextV(r.x + 24.f, y, kRowH, kMods[i].name,
						kMods[i].on ? NkRole::Text : NkRole::TextMuted);
				p.TextV(r.x + r.w - 92.f, y, kRowH, "^  v", NkRole::TextMuted);
				p.TextV(r.x + r.w - 56.f, y, kRowH, "appl.", NkRole::TextMuted);
				p.TextV(r.x + r.w - 18.f, y, kRowH, "x", NkRole::TextMuted);
				y += kRowH;
				// Parametre. Le petit losange marque « animable » -- il porte lui aussi
				// un role propre, parce qu'un parametre marque pour animation doit se
				// reconnaitre au premier coup d'oeil, dans tous les themes.
				p.Fill({r.x + 24.f, y, kLabelW - 24.f, kRowH}, NkRole::LabelCol);
				p.TextV(r.x + 30.f, y, kRowH, kMods[i].p1, NkRole::TextMuted);
				p.Fill({r.x + kLabelW + 5.f, y + 3.f, 60.f, kRowH - 6.f}, NkRole::InputBg, 2.f);
				p.TextV(r.x + kLabelW + 12.f, y, kRowH, kMods[i].v1);
				p.Fill({r.x + kLabelW + 72.f, y + 8.f, 7.f, 7.f}, p.C(p.Roles().animatable), 2.f);
				p.HLine(r.x, y + kRowH - 1.f, r.w);
				y += kRowH;
			}

			p.Fill({r.x, y, r.w, kRowH}, NkRole::PanelHeader);
			p.TextV(r.x + kPad, y, kRowH, "> Materiau");
		}

		// ── NAVIGATEUR DE PROJET (bas) ──────────────────────────────────────────
		inline void PaintBrowser(NkModelerPainter &p, const NkRect &r) {
			p.Fill(r, NkRole::PanelBg);
			p.HLine(r.x, r.y, r.w);

			const float32 topH = 30.f;
			p.Fill({r.x, r.y, r.w, topH}, NkRole::PanelHeader);
			p.TextV(r.x + kPad, r.y, topH, "Navigateur de projet");
			float32 x = r.x + p.TextW("Navigateur de projet") + 28.f;
			static const char *const kBtns[] = {"+ Ajouter", "Importer", "Tout enregistrer"};
			for (int32 i = 0; i < 3; ++i) {
				p.TextV(x, r.y, topH, kBtns[i], NkRole::TextMuted);
				x += p.TextW(kBtns[i]) + 20.f;
			}
			p.TextV(r.x + r.w - 220.f, r.y, topH, "Tout > Contenu > Perso", NkRole::TextMuted);
			p.HLine(r.x, r.y + topH - 1.f, r.w);

			// Arbre de dossiers. Ils structurent le PROJET et non le disque : ils
			// seront enregistres DANS le fichier de projet, comme demande.
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
				p.TextV(r.x + 8.f + (i ? 12.f : 0.f), dy, kRowH, kDirs[i],
						on ? NkRole::TextOnAccent : NkRole::Text);
				dy += kRowH;
			}

			// Filtres par TYPE. Chaque pastille porte la couleur de son type, prise
			// dans le theme : le navigateur et le reste de l'application parlent ainsi
			// la meme langue de couleur.
			const float32 ax = r.x + treeW + 10.f;
			float32 fx = ax + 170.f;
			p.Fill({ax, ty + 6.f, 150.f, 20.f}, NkRole::InputBg, 2.f);
			p.TextV(ax + 8.f, ty + 6.f, 20.f, "Rechercher...", NkRole::TextMuted);
			static const char *const kTypes[] = {"Maillage", "Animation", "Materiau", "Texture"};
			const NkRole kTypeRoles[] = {NkRole::TypeMesh, NkRole::TypeAnim, NkRole::TypeMat,
										 NkRole::TypeTex};
			for (int32 i = 0; i < 4; ++i) {
				const float32 w = p.TextW(kTypes[i]) + 26.f;
				p.Fill({fx + 6.f, ty + 12.f, 7.f, 7.f}, kTypeRoles[i], 4.f);
				p.TextV(fx + 18.f, ty + 6.f, 20.f, kTypes[i], NkRole::TextMuted);
				fx += w + 6.f;
			}
			p.HLine(ax - 10.f, ty + 32.f, r.w - treeW);

			// Vignettes. La bande de couleur en bas de chaque vignette redit le type :
			// on reconnait un materiau d'un maillage sans lire l'etiquette.
			struct Asset {
					const char *name;
					NkRole role;
			};
			static const Asset kAssets[] = {
				{"Cube", NkRole::TypeMesh}, {"Tete", NkRole::TypeMesh}, {"Bois", NkRole::TypeMat},
				{"Marche", NkRole::TypeMesh}, {"Roche", NkRole::TypeMesh},
			};
			float32 tx = ax;
			const float32 tw = 88.f, thh = 66.f, tyy = ty + 42.f;
			for (int32 i = 0; i < 5; ++i) {
				p.Fill({tx, tyy, tw, thh}, NkRole::PanelHeader, 2.f);
				p.Fill({tx, tyy + thh - 3.f, tw, 3.f}, kAssets[i].role);
				const float32 nw = p.TextW(kAssets[i].name);
				p.Text(tx + tw * 0.5f - nw * 0.5f, tyy + thh + 4.f, kAssets[i].name);
				tx += tw + 10.f;
			}
			p.TextV(r.x + r.w - 90.f, r.y + r.h - kRowH, kRowH, "5 elements", NkRole::TextMuted);
		}

		// ── BARRE D'ETAT ────────────────────────────────────────────────────────
		inline void PaintStatus(NkModelerPainter &p, const NkRect &r, const char *stats) {
			p.Fill(r, NkRole::PanelHeader);
			p.HLine(r.x, r.y, r.w);
			p.TextV(r.x + kPad, r.y, r.h, "Tiroir", NkRole::TextMuted);
			p.TextV(r.x + kPad + 54.f, r.y, r.h, "Journal", NkRole::TextMuted);
			p.Fill({r.x + 130.f, r.y + (r.h - 20.f) * 0.5f, 230.f, 20.f}, NkRole::InputBg, 2.f);
			p.TextV(r.x + 138.f, r.y, r.h, "Entrer une commande", NkRole::TextMuted);
			const float32 w = p.TextW(stats);
			p.TextV(r.x + r.w - w - kPad, r.y, r.h, stats, NkRole::TextMuted);
		}

	} // namespace nk3d
} // namespace nkentseu
