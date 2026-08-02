#pragma once
// =============================================================================
// NkModelerWidgets.h — les trois briques interactives du produit.
//
// TROIS PRIMITIVES COUVRENT LA QUASI-TOTALITE DE L'INTERFACE :
//   1. le CHAMP NUMERIQUE qu'on modifie EN GLISSANT (Blender, Unreal, Maya) ;
//   2. la LISTE DEROULANTE generique ;
//   3. la SAISIE DE TEXTE EN PLACE (renommer un objet, un dossier, une scene).
//
// Les ecrire UNE fois plutot qu'a chaque usage n'est pas qu'une economie : c'est
// la seule facon d'obtenir que tous les champs se comportent PAREIL. Un champ
// qui repond differemment d'un autre se lit comme un bug, meme si chacun pris
// isolement est correct.
// =============================================================================

#include "NK3DModeler/Shell/NkModelerUI.h"
#include "NK3DModeler/Shell/NkModelerInput.h"
// Le champ de saisie UNIVERSEL de l'editeur : curseur placable, selection,
// copier / couper / coller. Rihen le veut dans TOUS les champs de
// l'application, pour qu'aucun ne se comporte differemment d'un autre.
#include "NKEditorKit/NkEditorTextField.h"
#include "NKMath/NkColor.h" // NkColorF::ToHSV / FromHSV : la reference du moteur

#include <cstdio>

namespace nkentseu {
	namespace nk3d {

		// ── LE CONTEXTE NKGUI DE LA FRAME ───────────────────────────────────────
		// Les widgets partages en ont besoin pour la saisie universelle, et ils
		// sont appeles depuis des dizaines d'endroits : le faire descendre en
		// parametre partout aurait touche presque toutes les signatures. Il est
		// donc POSE UNE FOIS par frame, au debut, et lu ici.
		inline nkgui::NkGuiContext *&NkUiCtx() {
			static nkgui::NkGuiContext *c = nullptr;
			return c;
		}

		// ── ETAT PARTAGE DES WIDGETS ────────────────────────────────────────────
		// Un seul champ peut etre en cours de glissement ou de saisie a la fois :
		// l'etat est donc GLOBAL a la session et non par widget. Le stocker dans
		// chaque widget obligerait a les faire vivre entre les frames, ce qui est
		// contraire au principe d'interface immediate.
		struct NkWidgetState {
				// Glissement d'un champ numerique.
				char dragKey[48] = {};
				float32 dragStartX = 0.f;
				float32 dragStartValue = 0.f;
				bool dragging = false;

				// Saisie de texte en place.
				char editKey[48] = {};
				char editBuf[64] = {};
				uint32 editLen = 0;
				bool editing = false;

				// Liste deroulante ouverte. Cle vide = aucune.
				char openCombo[48] = {};

				static void Copy(char *dst, const char *src, uint32 cap = 47) {
					uint32 i = 0;
					for (; src && src[i] && i < cap; ++i)
						dst[i] = src[i];
					dst[i] = 0;
				}
				static bool Eq(const char *a, const char *b) {
					if (!a || !b)
						return false;
					while (*a && *b) {
						if (*a != *b)
							return false;
						++a;
						++b;
					}
					return *a == *b;
				}

				void BeginEdit(const char *key, const char *text) {
					Copy(editKey, key);
					Copy(editBuf, text, 63);
					editLen = 0;
					while (editBuf[editLen])
						editLen++;
					editing = true;
				}
				void EndEdit() {
					editing = false;
					editKey[0] = 0;
				}
				bool IsEditing(const char *key) const {
					return editing && Eq(editKey, key);
				}
				bool ComboOpen(const char *key) const {
					return Eq(openCombo, key);
				}
				void ToggleCombo(const char *key) {
					if (ComboOpen(key))
						openCombo[0] = 0;
					else
						Copy(openCombo, key);
				}
				void CloseCombo() {
					openCombo[0] = 0;
				}
		};

		// Un panneau deroulant qui sort de la fenetre est un panneau dont les
		// dernieres entrees sont INATTEIGNABLES. On le fait donc remonter ou glisser
		// pour qu'il tienne, plutot que de le laisser deborder : la liste est courte,
		// il y a toujours la place quelque part.
		inline NkRect NkFitPopup(const NkRect &anchor, float32 w, float32 h) {
			const float32 W = NkPopupBoundsW(), H = NkPopupBoundsH();
			const float32 m = S(4.f);
			float32 x = anchor.x, y = anchor.y + anchor.h + 2.f;
			// Sous l'ancre si possible, AU-DESSUS sinon : recouvrir le bouton qu'on
			// vient de cliquer ferait perdre de vue ce qu'on est en train de regler.
			if (y + h > H - m)
				y = anchor.y - h - 2.f;
			if (y < m)
				y = m;
			if (x + w > W - m)
				x = W - m - w;
			if (x < m)
				x = m;
			return {x, y, w, h};
		}

		// ── 1. CHAMP NUMERIQUE A GLISSEMENT ─────────────────────────────────────
		// Cliquer-glisser HORIZONTALEMENT change la valeur, comme dans Blender,
		// Unreal et Maya. C'est le geste le plus utilise d'un modeleur : bien plus
		// souvent qu'on ne tape un nombre, on veut « un peu plus, un peu moins » en
		// regardant le resultat.
		//
		// LA SENSIBILITE DEPEND DES MODIFICATEURS, et c'est indispensable : sans
		// cela, un meme geste sert soit au reglage grossier soit au fin, jamais aux
		// deux. Maj = precis (x0,1), Ctrl = par pas (aimante). C'est la convention
		// des trois logiciels cites.
		//
		// Renvoie true si la valeur a change cette frame.
		inline bool DragFloat(NkModelerPainter &p, NkHitRegistry &hit, NkWidgetState &ws,
							  const nkgui::NkGuiInput &in, const char *key, const NkRect &r,
							  float32 &value, float32 step, NkRole accent, const char *fmt = "%.2f") {
			// ── SAISIE AU CLAVIER (double-clic), demande de Rihen ───────────────
			// Le champ est AUSSI un champ de texte : double-clic, on tape la
			// valeur, Entree ou clic ailleurs valide, Echap annule. Le glissement
			// reste le geste principal ; la saisie sert aux valeurs exactes.
			if (ws.IsEditing(key)) {
				p.Outline(r, NkRole::AccentUi, NkRole::InputBg, 3.f);
				// MEME SAISIE QUE PARTOUT : curseur placable, selection, copier /
				// couper / coller. Un champ numerique se tape et se corrige comme
				// n'importe quel champ (Rihen : « tous les champs de saisie de
				// l'application, numerique ou caractere ou mixte »).
				hit.Add(key, r);
				if (nkgui::NkGuiContext *gc = NkUiCtx()) {
					editorkit::NkOverlayTextField(*gc, gc->dl, p.FontPtr(), r, ws.editBuf, 31,
												  true);
					// La VIRGULE tapee devient un point : c'est elle qu'on a sous le
					// doigt sur un clavier francais, et atof ne lit que le point.
					uint32 nl = 0;
					for (; ws.editBuf[nl]; ++nl)
						if (ws.editBuf[nl] == ',')
							ws.editBuf[nl] = '.';
					ws.editLen = nl;
				} else {
					p.Clip(r);
					p.TextV(r.x + S(4.f), r.y, r.h, ws.editBuf);
					const float32 cw = p.TextW(ws.editBuf);
					p.Fill({r.x + S(5.f) + cw, r.y + S(3.f), 1.f, r.h - S(6.f)}, NkRole::Text);
					p.Unclip();
				}
				bool commit = false, finish = false;
				if (hit.AnyClick() && !hit.IsHovered(key)) {
					commit = ws.editLen > 0;
					finish = true;
				}
				if (!NkUiCtx()) {
					for (int32 i = 0; i < in.charCount; ++i) {
						const uint32 c = in.chars[i];
						if (((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' ||
							 c == ',') &&
							ws.editLen < 30u) {
							ws.editBuf[ws.editLen++] = (c == ',') ? '.' : (char)c;
							ws.editBuf[ws.editLen] = 0;
						}
					}
					if (in.KeyPressed(nkgui::NkGuiKey::Backspace) && ws.editLen > 0)
						ws.editBuf[--ws.editLen] = 0;
				}
				if (in.KeyPressed(nkgui::NkGuiKey::Enter)) {
					commit = ws.editLen > 0;
					finish = true;
				} else if (in.KeyPressed(nkgui::NkGuiKey::Escape)) {
					finish = true; // annule : la valeur n'est jamais touchee
				}
				bool typed = false;
				if (finish) {
					if (commit) {
						const float32 nv = (float32)atof(ws.editBuf);
						typed = (nv != value);
						value = nv;
					}
					ws.EndEdit();
				}
				return typed;
			}

			const bool over = hit.Add(key, r);
			const bool active = ws.dragging && NkWidgetState::Eq(ws.dragKey, key);

			// Le curseur annonce le geste AVANT le clic : sans lui, rien ne dit
			// qu'un champ se glisse, et l'utilisateur le decouvre par accident.
			if (over || active)
				hit.WantCursor(NkCursorWant::ResizeWE);

			// Le DOUBLE-CLIC ouvre la saisie -- teste AVANT le drag : son premier
			// clic a arme un glissement qu'on desarme ici.
			if (over && in.mouseDoubleClicked[0]) {
				ws.dragging = false;
				ws.dragKey[0] = 0;
				char init[32];
				snprintf(init, sizeof(init), fmt, (double)value);
				ws.BeginEdit(key, init);
				return false;
			}

			if (hit.Clicked(key)) {
				ws.dragging = true;
				NkWidgetState::Copy(ws.dragKey, key);
				ws.dragStartX = hit.Mouse().x;
				ws.dragStartValue = value;
			}
			bool changed = false;
			if (active) {
				if (!hit.MouseDown()) {
					ws.dragging = false;
					ws.dragKey[0] = 0;
				} else {
					float32 s = step;
					if (in.shiftDown)
						s *= 0.1f; // reglage fin
					const float32 dx = hit.Mouse().x - ws.dragStartX;
					float32 v = ws.dragStartValue + dx * s;
					if (in.ctrlDown && s > 0.f) {
						// Aimantation sur un pas ROND, calculee depuis zero et non
						// depuis la valeur de depart : sinon on aimante sur une grille
						// decalee, et deux champs voisins ne retombent pas sur les
						// memes valeurs.
						const float32 g = step * 10.f;
						v = (float32)(int32)(v / g + (v < 0.f ? -0.5f : 0.5f)) * g;
					}
					if (v != value) {
						value = v;
						changed = true;
					}
				}
			}

			p.Outline(r, (over || active) ? NkRole::AccentUi : NkRole::Border, NkRole::InputBg, 3.f);
			p.Fill({r.x + 1.f, r.y + 1.f, S(3.f), r.h - 2.f}, accent);
			char txt[32];
			snprintf(txt, sizeof(txt), fmt, (double)value);
			// Valeur calee a GAUCHE, CLIPPEE au champ (Rihen) : le bloc peut alors
			// grandir sans que le nombre saute d'un bord a l'autre, et ce qui
			// deborde se tronque a DROITE -- l'entier et le signe, qui portent le
			// sens, restent toujours lisibles.
			p.Clip(r);
			p.TextV(r.x + S(7.f), r.y, r.h, txt);
			p.Unclip();
			return changed;
		}

		// ── 2. LISTE DEROULANTE GENERIQUE ───────────────────────────────────────
		// Dessine le bouton ; la LISTE elle-meme est peinte plus tard par
		// DrawComboPopup, apres tout le reste. Separer les deux est ce qui permet a
		// la liste de recouvrir les panneaux : le registre donne la priorite a la
		// derniere zone declaree, donc l'ordre de peinture EST l'ordre de priorite.
		struct NkComboPending {
				bool active = false;
				NkRect anchor{};
				const char *const *items = nullptr;
				const NkIcon *icons = nullptr;
				int32 count = 0;
				int32 *selected = nullptr;
				char key[48] = {};
		};

		inline void Combo(NkModelerPainter &p, NkHitRegistry &hit, NkWidgetState &ws, const char *key,
						  const NkRect &r, const char *const *items, const NkIcon *icons, int32 count,
						  int32 &selected, NkComboPending &pending, bool enabled = true,
						  bool showChevron = true, bool showFrame = true,
						  NkIcon faceIcon = NkIcon::Count) {
			const bool over = enabled && hit.Add(key, r);
			const bool open = ws.ComboOpen(key);
			const NkRole fg = enabled ? NkRole::Text : NkRole::TextMuted;
			// Le CADRE est optionnel : dans un groupe colle, c'est le groupe qui en
			// porte un seul. Sans cette option, trois cadres imbriques dans un
			// quatrieme donneraient une barre en damier.
			if (showFrame)
				p.Outline(r, (over || open) ? NkRole::AccentUi : NkRole::Border,
						  enabled ? NkRole::InputBg : NkRole::PanelHeader, 3.f);
			else if (over || open)
				p.Fill(r, open ? NkRole::AccentUi : NkRole::PanelHeader, 3.f);
			// SANS LIBELLE NI CHEVRON, le combo se reduit a son icone. C'est ce qu'il
			// faut dans une barre d'outils dense : le repere et la vitesse de camera
			// sont des reglages qu'on consulte rarement, leur donner la largeur d'un
			// libelle prendrait la place de commandes qu'on utilise a chaque geste.
			// L'etat reste lisible -- c'est l'icone elle-meme qui change.
			// Sans CHEVRON ni cadre, le combo se reduit a son icone centree : c'est
			// ce qu'il faut pour un reglage rare dans une barre dense.
			const bool iconOnly = !showChevron && !showFrame;
			float32 tx = r.x + (iconOnly ? (r.w - S(14.f)) * 0.5f : S(8.f));
			if (icons) {
				// OUVERT = fond accent -> l'icone passe en TextOnAccent. Elle etait
				// en AccentUi : bleu sur bleu, la valeur courante disparaissait le
				// temps du choix (bug signale par Rihen).
				p.IconV(tx, r.y, r.h, faceIcon != NkIcon::Count ? faceIcon : icons[selected],
						open ? NkRole::TextOnAccent : (enabled ? NkRole::Text : NkRole::TextMuted), 14.f);
				tx += S(19.f);
			}
			if (!iconOnly)
				p.TextV(tx, r.y, r.h, items[selected], fg);
			// Le chevron TOURNE quand la liste est ouverte : c'est ce qui distingue
			// « je peux ouvrir » de « c'est ouvert », sans avoir a regarder ailleurs.
			if (showChevron)
				p.IconV(r.x + r.w - S(18.f), r.y, r.h, open ? NkIcon::ChevronUp : NkIcon::ChevronDown,
						fg, 11.f);
			// MARQUEUR DE COMBO : un point blanc en bas a droite du bouton. C'est
			// ce qui distingue d'un coup d'oeil une LISTE d'un simple bouton --
			// sans lui, il faut cliquer pour le decouvrir (demande de Rihen).
			p.Fill({r.x + r.w - S(6.f), r.y + r.h - S(6.f), S(3.f), S(3.f)},
				   open ? NkRole::TextOnAccent : NkRole::Text);

			if (enabled && hit.Clicked(key))
				ws.ToggleCombo(key);
			if (ws.ComboOpen(key)) {
				pending.active = true;
				pending.anchor = r;
				pending.items = items;
				pending.icons = icons;
				pending.count = count;
				pending.selected = &selected;
				NkWidgetState::Copy(pending.key, key);
			}
		}

		// Peint la liste ouverte. A appeler APRES tout le reste.
		// `blockRect` (optionnel) recoit l'emprise de la liste ouverte : les
		// panneaux, peints AVANT elle, s'en servent a la frame suivante pour
		// ignorer les clics qui tombent dessus. Sans cela, choisir une entree
		// atteignait AUSSI le widget situe dessous -- meme famille de defaut que
		// les menus, et Rihen veut le meme traitement pour tous ces panneaux.
		inline void DrawComboPopup(NkModelerPainter &p, NkHitRegistry &hit, NkWidgetState &ws,
								   NkComboPending &pending, NkRect *blockRect = nullptr) {
			if (!pending.active)
				return;
			const NkRect &a = pending.anchor;
			const float32 itemH = S(24.f);
			// LARGEUR CALCULEE SUR LE CONTENU REEL, jamais sur l'ancre. Un combo
			// reduit a son icone fait 28 px : caler la liste dessus ecrasait le
			// libelle contre la coche, au point qu'on ne voyait plus quelle entree
			// etait la courante -- le defaut signale par Rihen.
			//
			// On RESERVE explicitement chaque zone : marge, icone, libelle, puis la
			// coche avec son propre espace. Additionner « un peu de marge » au juge
			// est precisement ce qui produit des chevauchements des qu'un libelle
			// s'allonge ou qu'une traduction arrive.
			const float32 padL = S(10.f);
			const float32 iconW = pending.icons ? S(19.f) : 0.f;
			const float32 checkW = S(26.f);
			const float32 padR = S(10.f);
			float32 labelW = 0.f;
			for (int32 i = 0; i < pending.count; ++i) {
				const float32 t = p.TextW(pending.items[i]);
				if (t > labelW)
					labelW = t;
			}
			float32 w = padL + iconW + labelW + checkW + padR;
			if (w < a.w)
				w = a.w;
			const NkRect box = NkFitPopup(a, w, itemH * (float32)pending.count + S(6.f));
			p.Fill({box.x + 2.f, box.y + 2.f, box.w, box.h}, NkRole::WindowBg, 4.f); // ombre
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 4.f);
			hit.Add("combo.panel", box);
			// L'EMPRISE de la liste, pour que les panneaux du dessous l'ignorent.
			if (blockRect)
				*blockRect = box;

			char k[64];
			for (int32 i = 0; i < pending.count; ++i) {
				const NkRect ir{box.x + 2.f, box.y + S(3.f) + (float32)i * itemH, box.w - 4.f, itemH};
				snprintf(k, sizeof(k), "%s.i%d", pending.key, i);
				const bool over = hit.Add(k, ir);
				const bool cur = (i == *pending.selected);
				if (over)
					p.Fill(ir, NkRole::AccentUi, 3.f);
				float32 tx = ir.x + padL;
				if (pending.icons) {
					p.IconV(tx, ir.y, itemH, pending.icons[i],
							over ? NkRole::TextOnAccent : NkRole::Text, 13.f);
					tx += iconW;
				}
				p.TextV(tx, ir.y, itemH, pending.items[i], over ? NkRole::TextOnAccent : NkRole::Text);
				// La valeur COURANTE porte une coche : sans elle, on ne sait pas ce
				// qu'on est en train de remplacer.
				// La coche est peinte dans l'espace qui lui est RESERVE : sans
				// reservation elle chevauche le libelle des qu'il s'allonge.
				if (cur)
					p.IconV(ir.x + ir.w - checkW + S(4.f), ir.y, itemH, NkIcon::Check,
							over ? NkRole::TextOnAccent : NkRole::AccentUi, 13.f);
				if (hit.Clicked(k)) {
					*pending.selected = i;
					ws.CloseCombo();
				}
			}

			// Un clic HORS de la liste la referme. Teste apres toutes les entrees :
			// sinon un clic sur une entree refermerait avant d'etre traite.
			if (hit.AnyClick() && !hit.IsHovered("combo.panel")) {
				bool onItem = false;
				for (int32 i = 0; i < pending.count && !onItem; ++i) {
					snprintf(k, sizeof(k), "%s.i%d", pending.key, i);
					onItem = hit.IsHovered(k);
				}
				if (!onItem && !hit.IsHovered(pending.key))
					ws.CloseCombo();
			}
			pending.active = false;
		}

		// ── 2bis. LISTE A CASES A COCHER ────────────────────────────────────────
		// Certaines listes ne sont PAS un choix unique. « Affichage » en est une :
		// on veut couramment la grille ET le repere d axes, sans les normales. Une
		// liste a choix unique obligerait a la rouvrir trois fois pour trois
		// reglages independants -- c est ce que UI_SPEC 9bis annoncait et que le
		// premier jet n avait pas respecte.
		//
		// L etat tient dans un MASQUE DE BITS : un booleen par entree eparpille l
		// etat, et le passer a une fonction generique demanderait un tableau de
		// pointeurs. Un entier se copie, se compare et se serialise d un bloc.
		struct NkCheckPending {
				bool active = false;
				NkRect anchor{};
				const char *const *items = nullptr;
				const NkIcon *icons = nullptr;
				int32 count = 0;
				uint32 *mask = nullptr;
				char key[48] = {};
		};

		inline void CheckCombo(NkModelerPainter &p, NkHitRegistry &hit, NkWidgetState &ws,
							   const char *key, const NkRect &r, const char *const *items,
							   const NkIcon *icons, int32 count, uint32 &mask, NkIcon buttonIcon,
							   NkCheckPending &pending) {
			const bool over = hit.Add(key, r);
			const bool open = ws.ComboOpen(key);
			if (over || open)
				p.Fill(r, open ? NkRole::AccentUi : NkRole::PanelHeader, 3.f);
			// L icone du bouton est FIXE : contrairement a un choix unique, il n y a
			// pas « une » valeur courante a representer.
			p.IconV(r.x + (r.w - S(14.f)) * 0.5f, r.y, r.h, buttonIcon,
					open ? NkRole::TextOnAccent : NkRole::Text, 14.f);
			// Marqueur de combo : les listes a cases en sont aussi.
			p.Fill({r.x + r.w - S(6.f), r.y + r.h - S(6.f), S(3.f), S(3.f)},
				   open ? NkRole::TextOnAccent : NkRole::Text);
			if (hit.Clicked(key))
				ws.ToggleCombo(key);
			if (ws.ComboOpen(key)) {
				pending.active = true;
				pending.anchor = r;
				pending.items = items;
				pending.icons = icons;
				pending.count = count;
				pending.mask = &mask;
				NkWidgetState::Copy(pending.key, key);
			}
		}

		inline void DrawCheckPopup(NkModelerPainter &p, NkHitRegistry &hit, NkWidgetState &ws,
								   NkCheckPending &pending) {
			if (!pending.active)
				return;
			const NkRect &a = pending.anchor;
			const float32 itemH = S(24.f);
			const float32 padL = S(10.f), boxW = S(22.f);
			const float32 iconW = pending.icons ? S(19.f) : 0.f;
			const float32 padR = S(14.f);
			float32 labelW = 0.f;
			for (int32 i = 0; i < pending.count; ++i) {
				const float32 t = p.TextW(pending.items[i]);
				if (t > labelW)
					labelW = t;
			}
			const float32 w = padL + boxW + iconW + labelW + padR;
			const NkRect box = NkFitPopup(a, w, itemH * (float32)pending.count + S(6.f));
			p.Fill({box.x + 2.f, box.y + 2.f, box.w, box.h}, NkRole::WindowBg, 4.f);
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 4.f);
			hit.Add("check.panel", box);

			char k[64];
			for (int32 i = 0; i < pending.count; ++i) {
				const NkRect ir{box.x + 2.f, box.y + S(3.f) + (float32)i * itemH, box.w - 4.f, itemH};
				snprintf(k, sizeof(k), "%s.c%d", pending.key, i);
				const bool over = hit.Add(k, ir);
				const bool on = ((*pending.mask) & (1u << (uint32)i)) != 0u;
				if (over)
					p.Fill(ir, NkRole::AccentUi, 3.f);
				// La CASE est dessinee meme decochee : une case qui n apparait que
				// cochee ne dit pas qu on peut la cocher.
				const NkRect cb{ir.x + padL, ir.y + (itemH - S(14.f)) * 0.5f, S(14.f), S(14.f)};
				if (on) {
					p.Fill(cb, over ? NkRole::TextOnAccent : NkRole::AccentUi, 2.f);
					p.IconV(cb.x + 1.f, cb.y - S(1.f), S(16.f), NkIcon::Check,
							over ? NkRole::AccentUi : NkRole::TextOnAccent, 12.f);
				} else {
					p.Outline(cb, over ? NkRole::TextOnAccent : NkRole::Border, NkRole::InputBg, 2.f);
				}
				float32 tx = ir.x + padL + boxW;
				if (pending.icons) {
					p.IconV(tx, ir.y, itemH, pending.icons[i],
							over ? NkRole::TextOnAccent : NkRole::Text, 13.f);
					tx += iconW;
				}
				p.TextV(tx, ir.y, itemH, pending.items[i], over ? NkRole::TextOnAccent : NkRole::Text);
				// LA LISTE NE SE REFERME PAS au clic : on coche plusieurs cases
				// d affilee. La refermer a chaque case obligerait a la rouvrir autant
				// de fois, ce qui annulerait tout l interet des cases.
				if (hit.Clicked(k))
					*pending.mask ^= (1u << (uint32)i);
			}

			if (hit.AnyClick() && !hit.IsHovered("check.panel")) {
				bool inside = false;
				for (int32 i = 0; i < pending.count && !inside; ++i) {
					snprintf(k, sizeof(k), "%s.c%d", pending.key, i);
					inside = hit.IsHovered(k);
				}
				if (!inside && !hit.IsHovered(pending.key))
					ws.CloseCombo();
			}
			pending.active = false;
		}

		// ── 3. SAISIE DE TEXTE EN PLACE ─────────────────────────────────────────
		// Double-clic pour renommer, Entree pour valider, Echap pour annuler.
		// La valeur n'est ecrite QU'A LA VALIDATION : editer directement la source
		// laisserait un nom a moitie tape si l'utilisateur abandonne.
		//
		// Renvoie true si le nom vient d'etre valide (out contient le nouveau).
		inline bool EditableText(NkModelerPainter &p, NkHitRegistry &hit, NkWidgetState &ws,
								 const nkgui::NkGuiInput &in, const char *key, const NkRect &r,
								 const char *text, NkRole role, char *out, uint32 outCap) {
			const bool editing = ws.IsEditing(key);
			if (!editing) {
				const bool over = hit.Add(key, r);
				p.TextV(r.x, r.y, r.h, text, role);
				// DOUBLE-CLIC et non simple clic : un simple clic doit selectionner
				// l'objet, pas ouvrir son nom. Confondre les deux rendrait la
				// selection impossible sans declencher un renommage.
				if (over && in.mouseDoubleClicked[0])
					ws.BeginEdit(key, text);
				return false;
			}

			// ── SAISIE : LE CHAMP UNIVERSEL DE NKEDITORKIT ──────────────────────
			// Curseur placable au clic, SELECTION a la souris et au clavier,
			// copier / couper / coller, double-clic pour tout prendre : c'est la
			// brique partagee de l'editeur (Rihen la veut PARTOUT, pour que tous
			// les champs de l'application se comportent pareil). Notre saisie
			// maison ne savait qu'ajouter et effacer en fin de ligne.
			p.Outline(r, NkRole::AccentUi, NkRole::InputBg, 2.f);
			if (nkgui::NkGuiContext *gc = NkUiCtx()) {
				editorkit::NkOverlayTextField(*gc, gc->dl, p.FontPtr(), r, ws.editBuf,
											  (int32)(outCap < 63u ? outCap : 63u), true);
				uint32 nlen = 0;
				while (ws.editBuf[nlen])
					++nlen;
				ws.editLen = nlen;
			} else {
				// Repli (contexte non fourni) : affichage simple, sans edition riche.
				p.TextV(r.x + S(4.f), r.y, r.h, ws.editBuf);
				const float32 cw = p.TextW(ws.editBuf);
				p.Fill({r.x + S(5.f) + cw, r.y + S(3.f), 1.f, r.h - S(6.f)}, NkRole::Text);
			}

			bool done = false;
			// CLIQUER AILLEURS VALIDE. C'est ce que fait tout editeur : on tape un
			// nom, on va cliquer autre chose, et le nom est pris. Exiger Entree
			// obligeait a un geste supplementaire que personne n'attend -- et pire,
			// abandonner sans Entree perdait la saisie en silence.
			//
			// La zone du champ a ete declaree ci-dessus (hit.Add) : elle n'est donc
			// survolee que si le clic tombe DANS le champ. Un clic hors du champ ne
			// la survole pas, et c'est exactement le signal de perte de focus.
			hit.Add(key, r);
			if (hit.AnyClick() && !hit.IsHovered(key)) {
				if (ws.editLen > 0) {
					NkWidgetState::Copy(out, ws.editBuf, outCap - 1u);
					done = true;
				}
				ws.EndEdit();
				return done;
			}
			// Caracteres et effacement : SEULEMENT en repli. Quand le champ
			// universel est en place, c'est lui qui les traite (et il gere en plus
			// la selection, le collage et le curseur), les refaire ici doublerait
			// chaque frappe.
			if (!NkUiCtx()) {
				for (int32 i = 0; i < in.charCount; ++i) {
					const uint32 c = in.chars[i];
					if (c >= 32u && c < 127u && ws.editLen + 1u < outCap && ws.editLen < 62u) {
						ws.editBuf[ws.editLen++] = (char)c;
						ws.editBuf[ws.editLen] = 0;
					}
				}
				if (in.KeyPressed(nkgui::NkGuiKey::Backspace) && ws.editLen > 0)
					ws.editBuf[--ws.editLen] = 0;
			}
			if (in.KeyPressed(nkgui::NkGuiKey::Enter)) {
				// Un nom VIDE est refuse : il rendrait la ligne inidentifiable dans
				// la liste. On annule plutot que d'accepter l'inutilisable.
				if (ws.editLen > 0) {
					NkWidgetState::Copy(out, ws.editBuf, outCap - 1u);
					done = true;
				}
				ws.EndEdit();
			} else if (in.KeyPressed(nkgui::NkGuiKey::Escape)) {
				ws.EndEdit(); // ANNULE : la source n'a jamais ete touchee
			}
			return done;
		}


		// ── VRAI PICKER DE COULEUR ──────────────────────────────────────────────
		// Transpose du ColorPicker4 de NKGui (NkGuiWidgets.cpp) sur le painter du
		// modeleur : carre SATURATION/VALEUR (blanc->teinte, noir par-dessus) +
		// barre de TEINTE en six segments. La teinte est MEMORISEE pendant le
		// geste : au noir ou au blanc elle serait perdue par la conversion.
		// Les conversions viennent de NKMath (NkColorF::ToHSV/FromHSV, teinte en
		// degres et s/v en [0,1]) : c'est la reference du moteur, inutile d'en
		// entretenir une seconde ici (Rihen).
		inline void NkRgbToHsv(const float32 *rgb, float32 *hsv) {
			const auto h = math::NkColorF(rgb[0], rgb[1], rgb[2], 1.f).ToHSV();
			hsv[0] = h.x;
			hsv[1] = h.y;
			hsv[2] = h.z;
		}
		inline void NkHsvToRgb(const float32 *hsv, float32 *rgb) {
			const math::NkColorF c = math::NkColorF::FromHSV(hsv[0], hsv[1], hsv[2], 1.f);
			rgb[0] = c.r;
			rgb[1] = c.g;
			rgb[2] = c.b;
		}
		// ── ROUE CHROMATIQUE ───────────────────────────────────────────────────
		// La TEINTE est l'angle, la SATURATION le rayon. On pave le disque de
		// secteurs : chacun est un triangle blanc au centre, teinte au bord --
		// le degrade des sommets fait tout le travail, sans texture.
		// hsv est la source de verite (la teinte survit au noir et au blanc) ;
		// rgb en est la traduction, mise a jour a chaque geste.
		inline bool NkColorWheel(NkModelerPainter &p, NkHitRegistry &hit, char *dragKey,
								 uint32 dragKeyCap, const char *key, float32 cx, float32 cy,
								 float32 radius, float32 *hsv, float32 *rgb) {
			const NkRect box{cx - radius, cy - radius, radius * 2.f, radius * 2.f};
			hit.Add(key, box);
			const int32 kSeg = 48;
			const float32 kTwoPi = 6.28318531f;
			for (int32 i = 0; i < kSeg; ++i) {
				const float32 a0 = kTwoPi * (float32)i / (float32)kSeg;
				const float32 a1 = kTwoPi * (float32)(i + 1) / (float32)kSeg;
				float32 c0[3], c1[3];
				const float32 h0[3] = {a0 * 180.f / 3.14159265f, 1.f, hsv[2]};
				const float32 h1[3] = {a1 * 180.f / 3.14159265f, 1.f, hsv[2]};
				NkHsvToRgb(h0, c0);
				NkHsvToRgb(h1, c1);
				const uint8 vw = (uint8)(hsv[2] * 255.f); // le centre suit la valeur
				const NkColor cc{vw, vw, vw, 255};
				const NkColor ce0{(uint8)(c0[0] * 255.f), (uint8)(c0[1] * 255.f),
								  (uint8)(c0[2] * 255.f), 255};
				const NkColor ce1{(uint8)(c1[0] * 255.f), (uint8)(c1[1] * 255.f),
								  (uint8)(c1[2] * 255.f), 255};
				p.TriColor({cx, cy}, {cx + cosf(a0) * radius, cy + sinf(a0) * radius},
						   {cx + cosf(a1) * radius, cy + sinf(a1) * radius}, cc, ce0, ce1);
			}
			bool changed = false;
			if (hit.MouseDown() &&
				(strcmp(dragKey, key) == 0 || (!dragKey[0] && hit.IsHovered(key)))) {
				snprintf(dragKey, dragKeyCap, "%s", key);
				const float32 dx = hit.Mouse().x - cx, dy = hit.Mouse().y - cy;
				const float32 d = sqrtf(dx * dx + dy * dy);
				float32 ang = atan2f(dy, dx) * 180.f / 3.14159265f;
				if (ang < 0.f)
					ang += 360.f;
				hsv[0] = ang;
				hsv[1] = d / radius > 1.f ? 1.f : d / radius; // hors du disque : sature
				NkHsvToRgb(hsv, rgb);
				changed = true;
			}
			// Le CURSEUR : un anneau clair double d'un anneau sombre, lisible aussi
			// bien sur le blanc du centre que sur les teintes saturees du bord.
			{
				const float32 a = hsv[0] * 3.14159265f / 180.f;
				const float32 px2 = cx + cosf(a) * hsv[1] * radius;
				const float32 py2 = cy + sinf(a) * hsv[1] * radius;
				p.Disc(px2, py2, 5.f, NkRole::Text);
				p.Disc(px2, py2, 3.5f, NkRole::PanelBg);
			}
			return changed;
		}

		inline bool NkColorPickerSV(NkModelerPainter &p, NkHitRegistry &hit, char *dragKey,
									uint32 dragKeyCap, const char *key, const NkRect &r,
									float32 *rgb) {
			const float32 barW = S(14.f), gap = S(8.f);
			const NkRect sq{r.x, r.y, r.w - barW - gap, r.h};
			const NkRect hb{r.x + r.w - barW, r.y, barW, r.h};
			char kSq[40], kHu[40];
			snprintf(kSq, sizeof(kSq), "%s.sv", key);
			snprintf(kHu, sizeof(kHu), "%s.h", key);
			hit.Add(kSq, sq);
			hit.Add(kHu, hb);
			const bool dragSq = (strcmp(dragKey, kSq) == 0);
			const bool dragHu = (strcmp(dragKey, kHu) == 0);
			// La teinte survit au geste : convertie a chaque image depuis le RGB,
			// elle retomberait a zero au noir/blanc (c'est ce que memorise aussi le
			// picker de NKGui, par id).
			static float32 sHsv[3] = {0.f, 0.f, 0.f};
			static char sFor[40] = {0};
			if (!dragSq && !dragHu) {
				// Resynchroniser DEPUIS le RGB seulement s'il a change de
				// l'EXTERIEUR (autre widget, prereglage) : au noir/blanc la
				// conversion perd la teinte, et la refaire chaque image faisait
				// sauter le curseur de teinte apres chaque relachement.
				float32 back[3];
				NkHsvToRgb(sHsv, back);
				const bool same = strcmp(sFor, key) == 0 &&
								  fabsf(back[0] - rgb[0]) < 0.004f &&
								  fabsf(back[1] - rgb[1]) < 0.004f &&
								  fabsf(back[2] - rgb[2]) < 0.004f;
				if (!same) {
					NkRgbToHsv(rgb, sHsv);
					snprintf(sFor, sizeof(sFor), "%s", key);
				}
			}
			bool changed = false;
			if (hit.MouseDown()) {
				if (!dragKey[0]) {
					if (hit.IsHovered(kSq))
						snprintf(dragKey, dragKeyCap, "%s", kSq);
					else if (hit.IsHovered(kHu))
						snprintf(dragKey, dragKeyCap, "%s", kHu);
				}
				if (strcmp(dragKey, kSq) == 0) {
					float32 t = (hit.Mouse().x - sq.x) / sq.w;
					float32 u = (hit.Mouse().y - sq.y) / sq.h;
					t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
					u = u < 0.f ? 0.f : (u > 1.f ? 1.f : u);
					sHsv[1] = t;
					sHsv[2] = 1.f - u;
					NkHsvToRgb(sHsv, rgb);
					changed = true;
				} else if (strcmp(dragKey, kHu) == 0) {
					float32 u = (hit.Mouse().y - hb.y) / hb.h;
					u = u < 0.f ? 0.f : (u > 1.f ? 1.f : u);
					sHsv[0] = u * 359.9f;
					NkHsvToRgb(sHsv, rgb);
					changed = true;
				}
			}
			// ── Dessin ──────────────────────────────────────────────────────
			float32 hueRgb[3];
			const float32 hueHsv[3] = {sHsv[0], 1.f, 1.f};
			NkHsvToRgb(hueHsv, hueRgb);
			const NkColor cHue{(uint8)(hueRgb[0] * 255.f), (uint8)(hueRgb[1] * 255.f),
							   (uint8)(hueRgb[2] * 255.f), 255};
			const NkColor cW{255, 255, 255, 255};
			const NkColor cT{0, 0, 0, 0};
			const NkColor cB{0, 0, 0, 255};
			p.RectMultiColor(sq, cW, cHue, cHue, cW); // blanc -> teinte
			p.RectMultiColor(sq, cT, cT, cB, cB);	  // noir par-dessus, vers le bas
			p.OutlineSharp(sq, NkRole::Border);
			// Barre de teinte : six segments de l'arc-en-ciel.
			static const uint8 kHue[7][3] = {{255, 0, 0},   {255, 255, 0}, {0, 255, 0}, {0, 255, 255},
											 {0, 0, 255},   {255, 0, 255}, {255, 0, 0}};
			for (int32 i = 0; i < 6; ++i) {
				const NkColor c0{kHue[i][0], kHue[i][1], kHue[i][2], 255};
				const NkColor c1{kHue[i + 1][0], kHue[i + 1][1], kHue[i + 1][2], 255};
				const NkRect seg{hb.x, hb.y + hb.h * (float32)i / 6.f, hb.w, hb.h / 6.f + 1.f};
				p.RectMultiColor(seg, c0, c0, c1, c1);
			}
			p.OutlineSharp(hb, NkRole::Border);
			// Curseurs : croix sur le carre, trait sur la barre.
			{
				const float32 cx = sq.x + sHsv[1] * sq.w;
				const float32 cy = sq.y + (1.f - sHsv[2]) * sq.h;
				p.Ring(cx, cy, S(5.f), NkRole::Text, NkRole::Border);
				const float32 hy = hb.y + (sHsv[0] / 359.9f) * hb.h;
				p.Fill({hb.x - S(2.f), hy - S(1.f), hb.w + S(4.f), S(3.f)}, NkRole::Text);
			}
			return changed;
		}

	} // namespace nk3d
} // namespace nkentseu
