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

#include <cstdio>

namespace nkentseu {
	namespace nk3d {

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
			const bool over = hit.Add(key, r);
			const bool active = ws.dragging && NkWidgetState::Eq(ws.dragKey, key);

			// Le curseur annonce le geste AVANT le clic : sans lui, rien ne dit
			// qu'un champ se glisse, et l'utilisateur le decouvre par accident.
			if (over || active)
				hit.WantCursor(NkCursorWant::ResizeWE);

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
			// Valeur calee a DROITE : les nombres se comparent par leur unite, et
			// trois colonnes calees a gauche donnent des virgules en escalier.
			const float32 tw = p.TextW(txt);
			p.TextV(r.x + r.w - tw - S(8.f), r.y, r.h, txt);
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
						  bool showChevron = true, bool showFrame = true) {
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
				p.IconV(tx, r.y, r.h, icons[selected],
						open ? NkRole::AccentUi : (enabled ? NkRole::Text : NkRole::TextMuted), 14.f);
				tx += S(19.f);
			}
			if (!iconOnly)
				p.TextV(tx, r.y, r.h, items[selected], fg);
			// Le chevron TOURNE quand la liste est ouverte : c'est ce qui distingue
			// « je peux ouvrir » de « c'est ouvert », sans avoir a regarder ailleurs.
			if (showChevron)
				p.IconV(r.x + r.w - S(18.f), r.y, r.h, open ? NkIcon::ChevronUp : NkIcon::ChevronDown,
						fg, 11.f);

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
		inline void DrawComboPopup(NkModelerPainter &p, NkHitRegistry &hit, NkWidgetState &ws,
								   NkComboPending &pending) {
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
			const NkRect box{a.x, a.y + a.h + 2.f, w, itemH * (float32)pending.count + S(6.f)};
			p.Fill({box.x + 2.f, box.y + 2.f, box.w, box.h}, NkRole::WindowBg, 4.f); // ombre
			p.Outline(box, NkRole::Border, NkRole::PanelHeader, 4.f);
			hit.Add("combo.panel", box);

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
			const NkRect box{a.x, a.y + a.h + 2.f, w, itemH * (float32)pending.count + S(6.f)};
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

			p.Outline(r, NkRole::AccentUi, NkRole::InputBg, 2.f);
			p.TextV(r.x + S(4.f), r.y, r.h, ws.editBuf);
			// Curseur de saisie : un trait plein, pas clignotant -- le clignotement
			// demanderait une horloge et n'apporte rien tant qu'un seul champ
			// s'edite a la fois.
			const float32 cw = p.TextW(ws.editBuf);
			p.Fill({r.x + S(5.f) + cw, r.y + S(3.f), 1.f, r.h - S(6.f)}, NkRole::Text);

			bool done = false;
			// Caracteres saisis. NKGui les met en file dans la frame ; on les
			// consomme ici. On filtre a l'ASCII imprimable tant que le rendu de
			// texte n'accepte pas l'UTF-8 en saisie -- laisser passer un accent
			// afficherait un caractere de remplacement, ce qui est pire que de le
			// refuser.
			for (int32 i = 0; i < in.charCount; ++i) {
				const uint32 c = in.chars[i];
				if (c >= 32u && c < 127u && ws.editLen + 1u < outCap && ws.editLen < 62u) {
					ws.editBuf[ws.editLen++] = (char)c;
					ws.editBuf[ws.editLen] = 0;
				}
			}
			if (in.KeyPressed(nkgui::NkGuiKey::Backspace) && ws.editLen > 0)
				ws.editBuf[--ws.editLen] = 0;
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

	} // namespace nk3d
} // namespace nkentseu
