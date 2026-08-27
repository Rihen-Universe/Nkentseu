#pragma once
// -----------------------------------------------------------------------------
// @File    NkModelerMenus.h
// @Brief   MENUS DEROULANTS de la barre de menu (Fichier, Edition, ...), menu des
//          modificateurs et menu d'ajout d'objet, tous peints APRES les panneaux.
//
//          Extrait de NkModelerScreens.h pendant la refonte d'interface --
//          « subdiviser les gros fichiers » (Rihen, 13 aout 2026).
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NK3DModeler/Shell/NkModelerUI.h"
#include "NK3DModeler/Shell/NkModelerInput.h"
#include "NK3DModeler/Shell/NkModelerWidgets.h"
#include "NK3DModeler/Shell/NkModelerTables.h"
#include "NK3DModeler/Shell/NkModelerCommon.h"
#include "NK3DModeler/Viewport/NkDemo3DHost.h"
#include "NKEditorKit/NkShortcutTable.h"

namespace nkentseu {
	namespace nk3d {


		// â”€â”€ DEROULEMENT D'UN MENU â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
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
					//
					// MENU FICHIER : les indices sont ceux de kFile, plus haut. Ils
					// sont ecrits ICI et nulle part ailleurs ; inserer une entree
					// dans la table oblige a relire ce bloc -- ce qui est voulu, un
					// decalage silencieux serait bien pire.
					//   0 Nouveau · 1 Ouvrir... · 2 Ouvrir recent (sous-menu, non
					//   ecrit) · 4 Enregistrer · 5 Enregistrer tout · 6 Enregistrer
					//   sous... · 11 Quitter
					// Les demandes partent en DIFFERE (`projPending`) : le selecteur
					// de fichiers de l'OS ouvre une boucle modale, l'appeler pendant
					// la peinture reentrerait dans la frame en cours.
					if (st.openMenu == 0) {
						if (i == 0)
							st.projPending = 1;
						else if (i == 1)
							st.projPending = 2;
						else if (i == 4)
							st.projPending = 3;
						else if (i == 5)
							st.projPending = 8;
						else if (i == 6)
							st.projPending = 4;
						else if (i == 11)
							NkRequestClose(st);
					}
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

		// â”€â”€ LISTE DES MODIFICATEURS, A DEUX NIVEAUX â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Peinte APRES tout le reste : elle doit recouvrir les panneaux, et le
		// registre donne la priorite a la derniere zone declaree.
		//
		// LA CATEGORIE S'OUVRE AU SURVOL et non au clic. Un clic serait un geste de
		// plus pour atteindre une entree qui, elle, en demande deja un -- et rien ne
		// justifie de valider le fait de Â« regarder Â» une categorie.
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
						// AUCUNE COCHE ICI. Une coche dit Â« ceci est l'option retenue Â» ;
						// or ce menu ne retient rien, il AJOUTE. Le modificateur ajoute
						// se lit dans le panneau Details, la ou il vit.
						(void)cur;
						if (hit.Clicked(key)) {
							st.modKind = flatBase + i;
							// Chaque entree porte SON type moteur : le menu ne calcule
							// aucun indice, donc rien ne diverge quand une categorie
							// gagne une entree.
							demo::Demo3DHostModAdd(cats[c].items[i].type);
							NkMarkDirty(st);
							ws.CloseCombo();
						}
					}
				}
				flatBase += cats[c].count;
			}

			// Un clic HORS des deux panneaux referme. Teste apres toutes les zones :
			// sinon un clic sur une entree refermerait avant d'etre traite.
			if (hit.AnyClick() && !hit.IsHovered("mod.panel") && !hit.IsHovered("mod.sub")
				&& !hit.IsHovered("tb.mod") && !hit.IsHovered("props.modadd")) {
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

		// â”€â”€ MENU Â« AJOUTER Â», DEUX NIVEAUX â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
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
			// Ce menu recouvre souvent la hierarchie : sans declarer son emprise,
			// le clic sur une entree atteignait AUSSI la ligne du dessous (c'est
			// ainsi que « Ajouter un enfant » verrouillait le parent).
			st.UiBlockAdd(box);

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
					st.UiBlockAdd(sub);
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
							// CREATION dans l'HOTE : noeud utilisateur nomme d'apres
							// l'entree (« Cube.001 »), selectionne immediatement.
							const int32 nn = demo::Demo3DHostAddNode(cats[c].items[i].type,
																	 cats[c].items[i].prim);
							if (nn >= 0) {
								NkHierComposeName(st, cats[c].items[i].label, nn);
								const int32 t8 = cats[c].items[i].type;
								const int32 root8 = NkModelRootOf(st);
								if (root8 >= 0) {
									// DANS UN MODEL, la regle du model PRIME sur le
									// parent demande : un maillage devient un MESH du
									// model, frere des autres sous sa racine. Lumieres,
									// cameras et empties restent des aides COSMETIQUES.
									//
									// Cette regle etait auparavant dans la branche
									// « sinon » : passer par « Ajouter un enfant »
									// posait donc le parent SANS marquer le maillage --
									// il ne revenait alors pas dans la scene et
									// n'entrait pas dans le lisere du model (Rihen).
									if (t8 != 4 && t8 != 5) {
										demo::Demo3DHostSetNodeParent(nn, root8);
										demo::Demo3DHostSetNodeIsMesh(nn, true);
									}
								} else if (st.addParentNode >= 0) {
									// hors model : clic droit sur un OBJET, il devient
									// le parent du nouveau (Rihen)
									demo::Demo3DHostSetNodeParent(nn, st.addParentNode);
								}
								demo::Demo3DHostSelectEmptyNode(nn);
								// pour TOUT element du menu, sans distinction (Rihen)
								st.addAdjustNode = nn;
							}
							NkMarkDirty(st);
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

	} // namespace nk3d
} // namespace nkentseu
