#pragma once
// -----------------------------------------------------------------------------
// @File    NkModelerProperties.h
// @Brief   LE PANNEAU DE PROPRIETES (droite) : widgets de reglage (ligne de
//          transformation, selecteur de couleur, groupes repliables) puis les
//          pastilles elles-memes -- objet, materiau, lumiere, camera, monde,
//          rendu, outil, modificateur.
//
//          ATTENTION : `PaintPropertiesUnified` fait a elle seule ~6 400 lignes.
//          Ce fichier ISOLE le domaine ; l'eclatement de cette fonction en
//          sous-fonctions par pastille est le chantier SUIVANT. Il ne peut pas
//          se faire par simple deplacement : les sections partagent des
//          variables locales (position courante, defilement, largeurs), qu'il
//          faudra passer explicitement.
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
#include "NK3DModeler/Shell/NkModelerViewport.h"
#include "NK3DModeler/Shell/NkModelerFileDialog.h"
#include "NK3DModeler/Viewport/NkViewport3D.h"
#include "NK3DModeler/Viewport/NkDemo3DHost.h"
#include "NK3DModeler/Viewport/NkOutCompose.h"

namespace nkentseu {
	namespace nk3d {

		// Declarations anticipees : ces fonctions vivent encore dans
		// NkModelerScreens.h, inclus APRES ce fichier par main.cpp.
		bool NkProjectWriteAssets(const NkString &root, NkModelerState &st, NkString *err,
								  int32 onlyCard);
		void NkBrowserSyncMats(NkModelerState &st);

		inline void PaintTransformRow(NkModelerPainter &p, NkHitRegistry &hit, NkWidgetState &ws,
									  const nkgui::NkGuiInput &in, const NkRect &r, float32 y,
									  const char *label, float32 *v, float32 step, const char *keyBase,
									  NkIcon icon1, NkIcon icon2, const char *fmt = "%.2f",
									  NkIcon icon3 = NkIcon::None, bool icon3On = false,
									  float32 labelW = kLabelW) {
			const float32 rowH = kRowH + S(6.f);
			p.Fill({r.x, y, labelW, rowH}, NkRole::LabelCol);
			p.Clip({r.x, y, labelW, rowH});
			p.TextV(r.x + kPad, y, rowH, label);
			p.Unclip();

			const float32 sq = rowH - S(6.f); // colonne carree : cote = hauteur du champ
			const float32 gap = S(4.f);
			// TROIS cases : cadenas, reinitialiser, PROPORTIONNEL. La troisieme
			// reste vide quand la ligne n'en veut pas -- l'alignement prime.
			const float32 iconsW = sq * 3.f + gap * 2.f;
			const float32 avail = r.w - labelW - S(10.f) - iconsW - gap;
			float32 fw = (avail - gap * 2.f) / 3.f;
			if (fw < S(40.f))
				fw = S(40.f);

			const NkRole axes[3] = {NkRole::AxisX, NkRole::AxisY, NkRole::AxisZ};
			float32 x = r.x + labelW + S(5.f);
			char key[48];
			// Les champs gardent leur LARGEUR MINIMALE, mais le bloc est CLIPPE
			// avant la colonne des icones : panneau etroit, le champ Z passait
			// SOUS le cadenas (constate par Rihen). Tronque vaut mieux que
			// superpose -- et la colonne d'icones reste toujours cliquable.
			const float32 fieldsEnd = r.x + r.w - S(5.f) - iconsW - gap;
			p.Clip({x, y, fieldsEnd - x, rowH});
			for (int32 i = 0; i < 3; ++i) {
				snprintf(key, sizeof(key), "%s.%d", keyBase, i);
				DragFloat(p, hit, ws, in, key, {x, y + S(3.f), fw, rowH - S(6.f)}, v[i], step, axes[i], fmt);
				x += fw + gap;
			}
			p.Unclip();

			// Les deux carres. Une icone absente laisse sa case VIDE plutot que de
			// decaler la suivante -- l'alignement des trois lignes prime.
			float32 ix = r.x + r.w - S(5.f) - iconsW;
			const NkIcon icons[3] = {icon1, icon2, icon3};
			for (int32 i = 0; i < 3; ++i) {
				if (icons[i] == NkIcon::None) {
					ix += sq + gap;
					continue;
				}
				const NkRect br{ix, y + S(3.f), sq, sq};
				snprintf(key, sizeof(key), "%s.ic%d", keyBase, i);
				const bool over = hit.Add(key, br);
				const bool accent = (i == 2 && icon3On);
				if (accent)
					p.Fill(br, NkRole::AccentUi, 3.f);
				else
					p.Outline(br, over ? NkRole::AccentUi : NkRole::Border, NkRole::PanelBg, 3.f);
				p.IconV(br.x + (sq - S(13.f)) * 0.5f, br.y, sq, icons[i],
						accent ? NkRole::TextOnAccent : NkRole::Text, 13.f);
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

		// ── LIGNE DE COULEUR : LA NUANCE OUVRE LE VRAI PICKER ───────────────────
		// Une couleur se choisit A L'OEIL, pas en tapant trois nombres. La ligne
		// montre donc une NUANCE cliquable ; le clic deplie le carre
		// saturation/valeur et la barre de teinte, avec les trois champs R/V/B
		// dessous pour la valeur exacte. Une seule nuance reste ouverte a la fois
		// (st.colorOpen), comme les pastilles du panneau.
		// Renvoie la hauteur consommee ; met *changed a vrai si la couleur bouge.
		inline float32 PaintColorRow(NkModelerPainter &p, NkHitRegistry &hit, NkWidgetState &ws,
									 const nkgui::NkGuiInput &in, NkModelerState &st,
									 const NkRect &r, float32 y, const char *label,
									 const char *keyBase, float32 *rgb, bool *changed) {
			const auto sat01 = [](float32 v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); };
			const float32 y0 = y;
			const float32 swH = kRowH - S(6.f);
			const float32 labW = S(110.f);
			p.TextV(r.x, y, kRowH, label, NkRole::TextMuted);
			const NkRect sw{r.x + labW, y + S(3.f), r.w - labW, swH};
			char key[48];
			snprintf(key, sizeof(key), "%s.sw", keyBase);
			const bool over = hit.Add(key, sw);
			const NkColor cur{(uint8)(sat01(rgb[0]) * 255.f), (uint8)(sat01(rgb[1]) * 255.f),
							  (uint8)(sat01(rgb[2]) * 255.f), 255};
			p.Fill(sw, cur, 3.f);
			p.OutlineSharp(sw, over ? NkRole::AccentUi : NkRole::Border);
			// LE PICKER S'OUVRE EN FENETRE MODALE (Rihen), peinte par-dessus tout
			// en fin de frame. Tant qu'elle est ouverte pour CE champ, la couleur
			// qu'elle porte est recopiee ici : l'apercu est immediat dans la scene.
			const bool open = strcmp(st.colorOpen, keyBase) == 0;
			if (hit.Clicked(key)) {
				if (open) {
					st.colorOpen[0] = 0;
				} else {
					snprintf(st.colorOpen, sizeof(st.colorOpen), "%s", keyBase);
					for (int32 c = 0; c < 3; ++c)
						st.colorOrig[c] = st.colorCur[c] = rgb[c];
					st.colorAlphaOrig = st.colorAlpha;
					st.colorHsvValid = false;
					st.colorModalPlaced = false;
				}
			} else if (open) {
				for (int32 c = 0; c < 3; ++c)
					if (rgb[c] != st.colorCur[c]) {
						rgb[c] = st.colorCur[c];
						*changed = true;
					}
			}
			if (open)
				p.OutlineSharp(sw, NkRole::AccentUi); // ce champ est celui qu'on regle
			return y + kRowH - y0;
		}

		// ── LA FENETRE MODALE DU PICKER ────────────────────────────────────────
		// Peinte en TOUT DERNIER, comme les menus : une surface modale qui doit
		// repondre a ses propres clics et voiler le reste. Deplacable par sa barre
		// de titre, comme les dialogues de NKEditorKit.
		inline void PaintColorPicker(NkModelerPainter &p, NkHitRegistry &hit, NkWidgetState &ws,
									 const nkgui::NkGuiInput &in, NkModelerState &st, float32 W,
									 float32 H) {
			if (!st.colorOpen[0])
				return;
			// FERMETURE DIFFEREE D'UNE IMAGE : le champ lit `colorCur` AVANT que
			// cette fenetre ne soit peinte. Vider la cle des le clic sur Annuler
			// laisserait donc l'objet avec la couleur de l'apercu -- l'annulation
			// n'annulerait rien. On rend d'abord la couleur d'origine, on ferme
			// a l'image suivante, quand le champ l'a reprise.
			if (st.colorClosing) {
				st.colorClosing = false;
				st.colorOpen[0] = 0;
				return;
			}
			const auto sat01 = [](float32 v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); };
			const float32 dw = S(260.f), titleH = S(26.f);
			// roue + deux rangees d'onglets + quatre canaux + hexa + boutons
			const float32 dh = titleH + (dw - S(56.f)) + S(62.f) + kRowH * 5.f + S(52.f);
			if (!st.colorModalPlaced) {
				st.colorModalX = (W - dw) * 0.5f;
				st.colorModalY = (H - dh) * 0.35f;
				st.colorModalPlaced = true;
			}
			// Le VOILE dit que le reste est suspendu -- et absorbe les clics.
			p.Fill({0.f, 0.f, W, H}, NkColor{0, 0, 0, 110});
			hit.Add("colmod.veil", {0.f, 0.f, W, H});
			NkRect box{st.colorModalX, st.colorModalY, dw, dh};
			// Glissement par la barre de titre, traite AVANT de figer la boite :
			// sinon l'affichage traine d'une image derriere la souris.
			const NkRect tb{box.x, box.y, box.w, titleH};
			if (hit.MouseDown() && !st.colorModalDrag && NkHitRegistry::Contains(tb, hit.Mouse())) {
				st.colorModalDrag = true;
				st.colorDragDX = hit.Mouse().x - box.x;
				st.colorDragDY = hit.Mouse().y - box.y;
			}
			if (st.colorModalDrag) {
				if (!hit.MouseDown()) {
					st.colorModalDrag = false;
				} else {
					st.colorModalX = hit.Mouse().x - st.colorDragDX;
					st.colorModalY = hit.Mouse().y - st.colorDragDY;
					if (st.colorModalX < 0.f)
						st.colorModalX = 0.f;
					if (st.colorModalY < 0.f)
						st.colorModalY = 0.f;
					if (st.colorModalX + dw > W)
						st.colorModalX = W - dw;
					if (st.colorModalY + dh > H)
						st.colorModalY = H - dh;
					box.x = st.colorModalX;
					box.y = st.colorModalY;
				}
			}
			p.Fill({box.x + 3.f, box.y + 4.f, box.w, box.h}, NkColor{0, 0, 0, 110}, 4.f);
			p.Outline(box, NkRole::AccentUi, NkRole::PanelBg, 4.f);
			p.Fill({box.x, box.y, box.w, titleH}, NkRole::PanelHeader, 4.f);
			p.TextV(box.x + S(10.f), box.y, titleH, "Couleur");
			hit.Add("colmod.title", tb);
			float32 y = box.y + titleH + S(8.f);
			// ── ROUE CHROMATIQUE + BARRE DE VALEUR ──────────────────────────
			// La teinte tourne, la saturation s'eloigne du centre, la valeur se
			// regle a part : la disposition retenue par Rihen.
			const float32 wheelD = box.w - S(56.f);
			const float32 rad = wheelD * 0.5f;
			const float32 cxw = box.x + S(12.f) + rad, cyw = y + rad;
			// La TEINTE est memorisee : au noir comme au blanc, la reconvertir
			// depuis le RVB la perdrait et le curseur sauterait.
			if (!st.colorHsvValid) {
				NkRgbToHsv(st.colorCur, st.colorHsv);
				st.colorHsvValid = true;
			} else {
				float32 back[3];
				NkHsvToRgb(st.colorHsv, back);
				if (fabsf(back[0] - st.colorCur[0]) > 0.004f ||
					fabsf(back[1] - st.colorCur[1]) > 0.004f ||
					fabsf(back[2] - st.colorCur[2]) > 0.004f)
					NkRgbToHsv(st.colorCur, st.colorHsv); // change de l'exterieur
			}
			NkColorWheel(p, hit, st.propDragKey, sizeof(st.propDragKey), "colmod.wheel", cxw, cyw,
						 rad, st.colorHsv, st.colorCur);
			{
				// Barre de VALEUR, de la teinte pleine au noir.
				const NkRect vb{box.x + box.w - S(32.f), y, S(18.f), wheelD};
				hit.Add("colmod.val", vb);
				float32 pure[3];
				const float32 pureHsv[3] = {st.colorHsv[0], st.colorHsv[1], 1.f};
				NkHsvToRgb(pureHsv, pure);
				const NkColor cTop{(uint8)(pure[0] * 255.f), (uint8)(pure[1] * 255.f),
								   (uint8)(pure[2] * 255.f), 255};
				const NkColor cBot{0, 0, 0, 255};
				p.RectMultiColor(vb, cTop, cTop, cBot, cBot);
				p.OutlineSharp(vb, NkRole::Border);
				if (hit.MouseDown() && (strcmp(st.propDragKey, "colmod.val") == 0 ||
										(!st.propDragKey[0] && hit.IsHovered("colmod.val")))) {
					snprintf(st.propDragKey, sizeof(st.propDragKey), "colmod.val");
					st.colorHsv[2] = sat01(1.f - (hit.Mouse().y - vb.y) / vb.h);
					NkHsvToRgb(st.colorHsv, st.colorCur);
				}
				const float32 my = vb.y + (1.f - st.colorHsv[2]) * vb.h;
				p.Fill({vb.x - 2.f, my - 1.5f, vb.w + 4.f, 3.f}, NkRole::Text, 1.f);
			}
			y += wheelD + S(10.f);
			// ── ESPACE : LINEAIRE / PERCEPTUEL ──────────────────────────────
			// Le perceptuel corrige le gamma : il ne change QUE l'affichage des
			// nombres, jamais la couleur envoyee au rendu.
			{
				static const char *const kSp[2] = {"Lineaire", "Perceptuel"};
				const float32 hw = (box.w - S(20.f)) * 0.5f;
				char sk[24];
				for (int32 s2 = 0; s2 < 2; ++s2) {
					const NkRect br{box.x + S(10.f) + (float32)s2 * hw, y, hw, S(22.f)};
					snprintf(sk, sizeof(sk), "colmod.sp%d", s2);
					const bool on = (st.colorSpace == s2);
					hit.Add(sk, br);
					p.Fill(br, on ? NkRole::AccentUi : NkRole::InputBg, 3.f);
					p.TextV(br.x + (hw - p.TextW(kSp[s2])) * 0.5f, y, S(22.f), kSp[s2],
							on ? NkRole::TextOnAccent : NkRole::Text);
					if (hit.Clicked(sk))
						st.colorSpace = s2;
				}
				y += S(26.f);
			}
			// ── RVB / TSV ───────────────────────────────────────────────────
			{
				static const char *const kTb[2] = {"RVB", "TSV"};
				const float32 hw = (box.w - S(20.f)) * 0.5f;
				char tk[24];
				for (int32 t2 = 0; t2 < 2; ++t2) {
					const NkRect br{box.x + S(10.f) + (float32)t2 * hw, y, hw, S(22.f)};
					snprintf(tk, sizeof(tk), "colmod.tb%d", t2);
					const bool on = (st.colorTab == t2);
					hit.Add(tk, br);
					p.Fill(br, on ? NkRole::AccentUi : NkRole::InputBg, 3.f);
					p.TextV(br.x + (hw - p.TextW(kTb[t2])) * 0.5f, y, S(22.f), kTb[t2],
							on ? NkRole::TextOnAccent : NkRole::Text);
					if (hit.Clicked(tk))
						st.colorTab = t2;
				}
				y += S(26.f);
			}
			// ── LES CANAUX, PUIS L'ALPHA, PUIS L'HEXADECIMAL ────────────────
			{
				static const char *const kRgb[3] = {"Rouge", "Vert", "Bleu"};
				static const char *const kHsvN[3] = {"Teinte", "Saturation", "Valeur"};
				const float32 lblW = S(78.f);
				char k[32];
				for (int32 c = 0; c < 3; ++c) {
					p.TextV(box.x + S(10.f), y, kRowH, st.colorTab == 0 ? kRgb[c] : kHsvN[c],
							NkRole::TextMuted);
					snprintf(k, sizeof(k), "colmod.c%d", c);
					const NkRect fr{box.x + S(10.f) + lblW, y + S(2.f), box.w - S(20.f) - lblW,
									kRowH - S(4.f)};
					if (st.colorTab == 0) {
						const float32 g = st.colorSpace == 1 ? 1.f / 2.2f : 1.f;
						float32 v = powf(sat01(st.colorCur[c]), g);
						if (DragFloat(p, hit, ws, in, k, fr, v, 0.005f, NkRole::AccentUi, "%.3f")) {
							st.colorCur[c] = sat01(powf(sat01(v), 1.f / g));
							st.colorHsvValid = false;
						}
					} else {
						float32 v = c == 0 ? st.colorHsv[0] / 360.f : st.colorHsv[c];
						if (DragFloat(p, hit, ws, in, k, fr, v, 0.005f, NkRole::AccentUi, "%.3f")) {
							st.colorHsv[c] = c == 0 ? sat01(v) * 359.9f : sat01(v);
							NkHsvToRgb(st.colorHsv, st.colorCur);
						}
					}
					y += kRowH;
				}
				p.TextV(box.x + S(10.f), y, kRowH, "Alpha", NkRole::TextMuted);
				float32 al = st.colorAlpha;
				if (DragFloat(p, hit, ws, in, "colmod.a",
							  {box.x + S(10.f) + lblW, y + S(2.f), box.w - S(20.f) - lblW,
							   kRowH - S(4.f)},
							  al, 0.005f, NkRole::AccentUi, "%.3f"))
					st.colorAlpha = sat01(al);
				y += kRowH + S(4.f);
				// L'HEXADECIMAL s'echange en sRGB : c'est la notation que tout le
				// monde copie-colle, elle doit donner la meme couleur qu'ailleurs.
				p.TextV(box.x + S(10.f), y, kRowH, "Hex", NkRole::TextMuted);
				const float32 gH = 1.f / 2.2f;
				if (!ws.IsEditing("colmod.hex"))
					snprintf(st.colorHex, sizeof(st.colorHex), "#%02X%02X%02X%02X",
							 (uint32)(powf(sat01(st.colorCur[0]), gH) * 255.f + 0.5f),
							 (uint32)(powf(sat01(st.colorCur[1]), gH) * 255.f + 0.5f),
							 (uint32)(powf(sat01(st.colorCur[2]), gH) * 255.f + 0.5f),
							 (uint32)(sat01(st.colorAlpha) * 255.f + 0.5f));
				if (EditableText(p, hit, ws, in, "colmod.hex",
								 {box.x + S(10.f) + lblW, y + S(2.f), box.w - S(20.f) - lblW,
								  kRowH - S(4.f)},
								 st.colorHex, NkRole::Text, st.colorHex, sizeof(st.colorHex))) {
					uint32 r2 = 0, g2 = 0, b2 = 0, a2 = 255;
					const char *s3 = st.colorHex;
					while (*s3 == '#' || *s3 == ' ')
						++s3;
					const int32 n3 = (int32)strlen(s3);
					if ((n3 == 6 || n3 == 8) && sscanf(s3, "%2x%2x%2x", &r2, &g2, &b2) == 3) {
						if (n3 == 8)
							sscanf(s3 + 6, "%2x", &a2);
						st.colorCur[0] = powf((float32)r2 / 255.f, 2.2f);
						st.colorCur[1] = powf((float32)g2 / 255.f, 2.2f);
						st.colorCur[2] = powf((float32)b2 / 255.f, 2.2f);
						st.colorAlpha = (float32)a2 / 255.f;
						st.colorHsvValid = false;
					}
				}
				y += kRowH + S(6.f);
			}
			const float32 bw = (box.w - S(30.f)) * 0.5f;
			const NkRect bOk{box.x + S(10.f), y, bw, S(24.f)};
			const NkRect bCa{box.x + S(20.f) + bw, y, bw, S(24.f)};
			hit.Add("colmod.ok", bOk);
			p.Fill(bOk, NkRole::AccentUi, 3.f);
			p.TextV(bOk.x + (bOk.w - p.TextW("Valider")) * 0.5f, y, S(24.f), "Valider",
					NkRole::TextOnAccent);
			hit.Add("colmod.cancel", bCa);
			p.Outline(bCa, NkRole::Border, NkRole::InputBg, 3.f);
			p.TextV(bCa.x + (bCa.w - p.TextW("Annuler")) * 0.5f, y, S(24.f), "Annuler");
			const bool esc = in.KeyPressed(nkgui::NkGuiKey::Escape);
			if (hit.Clicked("colmod.cancel") || esc) {
				// ANNULER REND LA COULEUR DE DEPART, pour de bon : l'apercu en
				// direct l'avait deja appliquee a l'objet.
				for (int32 c = 0; c < 3; ++c)
					st.colorCur[c] = st.colorOrig[c];
				st.colorAlpha = st.colorAlphaOrig;
				st.colorHsvValid = false;
				st.colorClosing = true;
			} else if (hit.Clicked("colmod.ok") || in.KeyPressed(nkgui::NkGuiKey::Enter)) {
				st.colorOpen[0] = 0;
			}
			// LA MODALE DECLARE SON EMPRISE, VOILE COMPRIS : plus rien de la
			// couche inferieure n'est atteignable tant qu'elle est ouverte, que
			// le clic passe par le registre ou qu'il soit teste a la main.
			hit.PushOcclusion({0.f, 0.f, W, H}, 100);
			st.UiBlockAdd(box);
		}

		// ── GROUPE DE TRANSFORMATION, AU FORMAT DE LA MAQUETTE ──────────────────
		// Dessin choisi par Rihen (Banani) : le titre sur SA ligne, puis une rangee
		// de trois cellules « barre d'axe coloree + champ », et enfin les trois
		// commandes cadenas / reinitialiser / proportionnel.
		//
		// L'axe ne s'ecrit pas : il se LIT A LA COULEUR de sa barre. Ces trois
		// couleurs sont celles de la maquette (plus claires que celles de la vue
		// 3D) -- c'est ce que Rihen a valide a l'ecran.
		inline float32 NkXformGroupH() {
			return kRowH + S(28.f);
		}

		// ── GROUPE DE PROPRIETES REPLIABLE (bandeau + chevron) ──────────────────
		// Les elements de nature differente se rangent par GROUPE (Rihen) :
		// Transformation, Dimensions, Relations, Materiaux... Le bandeau porte le
		// chevron ; replier un groupe cache SON contenu, pas celui des voisins.
		// Renvoie true si le groupe est DEPLIE (donc s'il faut peindre son
		// contenu). `bit` identifie le groupe dans st.grpFold.
		// MARGE du contenu dans le panneau : il ne colle ni au bord gauche ni a la
		// gouttiere de droite (Rihen) -- un texte qui touche le bord se lit mal et
		// donne l'impression que le panneau est trop petit.
		inline float32 NkPropInset() {
			return S(10.f);
		}
		// Le BLOC qui entoure le contenu d'un groupe : il dit ou le groupe commence
		// et ou il finit, ce qu'un simple bandeau ne suffit pas a montrer quand
		// plusieurs groupes se suivent (Rihen).
		// L'ESPACE ENTRE DEUX GROUPES. Il vaut AUSSI pour un groupe replie (Rihen) :
		// c'est justement quand les bandeaux se suivent qu'ils ont besoin de
		// respirer, sinon ils forment une colonne indistincte.
		inline float32 NkPropGroupGap() {
			return S(8.f);
		}
		// MARGE INTERNE d'un groupe : son contenu ne touche ni le cadre a gauche ni
		// a droite, et respire aussi en haut et en bas (Rihen). Sans elle, les
		// champs semblaient colles aux bords du bloc.
		inline float32 NkGroupPad() {
			return S(8.f);
		}
		// Le rectangle de TRAVAIL a l'interieur d'un groupe.
		inline NkRect NkGroupInner(const NkRect &r) {
			return {r.x + NkGroupPad(), 0.f, r.w - 2.f * NkGroupPad(), 0.f};
		}
		inline void PaintGroupBlock(NkModelerPainter &p, const NkRect &r, float32 yTop,
									float32 yBottom) {
			if (yBottom <= yTop)
				return;
			// CONTOUR SEUL. `Outline` REPEINT le fond : peint apres le contenu, il
			// effacait les champs du groupe -- ils s'affichaient vides (constate
			// par Rihen). Le cadre se trace donc sans remplissage.
			p.OutlineSharp({r.x, yTop, r.w, yBottom - yTop}, NkRole::Border);
		}
		// ── UNE SECTION-LISTE DU PANNEAU MODELE ────────────────────────────────
		// Dessin de la maquette : chaque element porte son marqueur, son nom
		// EDITABLE, sa valeur, puis la rangee de quatre commandes
		// (assigner / deselectionner / ajouter / retirer) ; la section se termine
		// par « + Ajouter ».
		//
		// AUCUNE DONNEE INVENTEE : la liste part vide. Ces natures n'existent pas
		// encore dans le moteur, alors on montre honnetement qu'il n'y a rien, et
		// on garde ce que l'utilisateur cree (regle de Rihen sur les references).
		inline void PaintListSection(NkModelerPainter &p, NkHitRegistry &hit, NkWidgetState &ws,
									 const nkgui::NkGuiInput &in, NkModelerState &st,
									 const NkRect &r, float32 &y, const char *keyBase,
									 uint8 kind, int32 owner, NkIcon mark, NkRole markRole,
									 const char *newName, const char *emptyNote,
									 bool withValue) {
			const NkRect inR = NkGroupInner(r);
			char key[48];
			int32 shown = 0;
			for (int32 i = 0; i < st.listCount; ++i) {
				if (st.listItems[i].kind != kind || st.listItems[i].owner != owner)
					continue;
				++shown;
				p.IconV(inR.x, y, kRowH, mark, markRole, 10.f);
				snprintf(key, sizeof(key), "%s.n%d", keyBase, i);
				const float32 metaW = withValue ? S(46.f) : 0.f;
				EditableText(p, hit, ws, in, key,
							 {inR.x + S(16.f), y, inR.w - S(16.f) - metaW, kRowH},
							 st.listItems[i].name, NkRole::Text, st.listItems[i].name, 20u);
				if (withValue) {
					char mv[16];
					snprintf(mv, sizeof(mv), "%.0f%%", (double)(st.listItems[i].value * 100.f));
					p.TextV(inR.x + inR.w - metaW, y, kRowH, mv, NkRole::TextMuted);
				}
				y += kRowH;
				// Les quatre commandes de la maquette, a parts egales.
				{
					static const NkIcon kAct[4] = {NkIcon::SquareCheck, NkIcon::Square,
												   NkIcon::PlusCircle, NkIcon::MinusCircle};
					const float32 gap = S(3.f);
					const float32 bw = (inR.w - gap * 3.f) * 0.25f;
					const float32 bh = S(18.f);
					float32 bx = inR.x;
					for (int32 a = 0; a < 4; ++a) {
						snprintf(key, sizeof(key), "%s.a%d.%d", keyBase, i, a);
						const NkRect br{bx, y, bw, bh};
						const bool ov = hit.Add(key, br);
						p.Outline(br, ov ? NkRole::AccentUi : NkRole::Border,
								  NkRole::PanelHeader, 3.f);
						p.IconV(br.x + (bw - S(10.f)) * 0.5f, br.y, bh, kAct[a],
								NkRole::TextMuted, 10.f);
						// RETIRER est la seule action que nous savons deja tenir :
						// les trois autres attendent le modele de donnees.
						if (a == 3 && hit.Clicked(key)) {
							for (int32 k = i; k + 1 < st.listCount; ++k)
								st.listItems[k] = st.listItems[k + 1];
							--st.listCount;
						}
						bx += bw + gap;
					}
					y += bh + S(4.f);
				}
			}
			if (shown == 0) {
				p.TextV(inR.x, y, kRowH, emptyNote, NkRole::TextMuted);
				y += kRowH;
			}
			snprintf(key, sizeof(key), "%s.add", keyBase);
			{
				const NkRect br{inR.x, y, inR.w, S(20.f)};
				const bool ov = hit.Add(key, br);
				p.Outline(br, ov ? NkRole::AccentUi : NkRole::Border, NkRole::PanelHeader, 3.f);
				const float32 tw = p.TextW("Ajouter");
				p.IconV(br.x + (br.w - tw) * 0.5f - S(14.f), br.y, br.h, NkIcon::Add,
						NkRole::TextMuted, 10.f);
				p.TextV(br.x + (br.w - tw) * 0.5f, br.y - S(1.f), br.h, "Ajouter",
						NkRole::TextMuted);
				if (hit.Clicked(key) && st.listCount < 64 && owner >= 0) {
					NkModelerState::ListItem &it = st.listItems[st.listCount++];
					it.kind = kind;
					it.owner = owner;
					it.value = 1.f;
					it.on = true;
					snprintf(it.name, sizeof(it.name), "%s_%02d", newName, shown + 1);
				}
				y += br.h;
			}
		}
		inline bool PaintPropGroup(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
								   const NkRect &r, float32 &y, const char *key,
								   const char *title, uint32 bit) {
			const NkRect hr{r.x, y, r.w, kRowH};
			const bool over = hit.Add(key, hr);
			p.Fill(hr, NkRole::PanelHeader);
			if (over)
				p.Fill({hr.x, hr.y + hr.h - S(2.f), hr.w, S(2.f)}, NkRole::AccentUi);
			const bool folded = (st.grpFold & bit) != 0u;
			p.IconV(r.x + S(4.f), y, kRowH,
					folded ? NkIcon::ChevronRight : NkIcon::ChevronDown, NkRole::Text, 11.f);
			p.TextV(r.x + S(20.f), y, kRowH, title);
			// ── LE MENU DU GROUPE, POUR TOUS LES GROUPES (Rihen) ────────────
			// Il est rendu ICI, dans la brique commune : aucun groupe n'a a le
			// reecrire, et un groupe ajoute demain l'aura sans y penser. Sa
			// facture est celle d'Unity : un petit bouton a droite du bandeau
			// qui deroule copier / coller / reinitialiser.
			{
				char mk[52];
				snprintf(mk, sizeof(mk), "%s.menu", key);
				const NkRect mb{hr.x + hr.w - S(22.f), y + S(3.f), S(18.f), kRowH - S(6.f)};
				const bool ovM = hit.Add(mk, mb);
				const bool openM = (strcmp(st.grpMenuKey, key) == 0);
				if (openM)
					p.Fill(mb, NkRole::AccentUi, 3.f);
				else if (ovM)
					p.Fill(mb, NkRole::InputBg, 3.f);
				p.IconV(mb.x + S(3.f), mb.y, mb.h, NkIcon::Menu,
						openM ? NkRole::TextOnAccent : NkRole::TextMuted, 11.f);
				if (hit.Clicked(mk)) {
					if (openM) {
						st.grpMenuKey[0] = 0;
					} else {
						NkWidgetState::Copy(st.grpMenuKey, key, 39u);
						NkWidgetState::Copy(st.grpMenuTitle, title, 39u);
						st.grpMenuAnchor = mb;
					}
				}
				// Le CHEVRON ne doit pas plier quand on visait le menu : la zone
				// du menu est declaree APRES, elle gagne donc le survol.
				if (hit.Clicked(key) && !ovM)
					st.grpFold ^= bit;
			}
			y += kRowH;
			return !folded;
		}
		// ── LE GROUPE RECLAME-T-IL UNE ACTION ? ─────────────────────────────
		// Teste ET CONSOMME : appelee au tour de peinture du groupe, elle rend
		// vrai une seule fois. `act` : 1 copier, 2 coller, 3 reinitialiser.
		inline bool NkGrpWants(NkModelerState &st, const char *key, int32 act) {
			if (st.grpAction != act || strcmp(st.grpActionKey, key) != 0)
				return false;
			st.grpAction = 0; // consommee
			return true;
		}
		// Le presse-papiers porte-t-il des valeurs de CE groupe ?
		inline bool NkGrpCanPaste(const NkModelerState &st, const char *key) {
			return st.grpClipHas && strcmp(st.grpClipKey, key) == 0;
		}
		inline void NkGrpCopyF(NkModelerState &st, const char *key, const float32 *v, int32 n) {
			NkWidgetState::Copy(st.grpClipKey, key, 39u);
			for (int32 i = 0; i < n && i < 16; ++i)
				st.grpClipF[i] = v[i];
			st.grpClipHas = true;
		}
		// ── LE MENU OUVERT D'UN GROUPE, peint EN SURCOUCHE ──────────────────
		// Appele une fois, tout a la fin du panneau : les entrees repondent
		// alors sans que le contenu du dessous ne vole leurs clics. Les actions
		// qui n'ont pas encore de sens sont GRISEES et le disent -- jamais une
		// entree qui fait semblant d'agir.
		inline void PaintPropGroupMenu(NkModelerPainter &p, NkHitRegistry &hit,
									   NkModelerState &st, const nkgui::NkGuiInput &in) {
			if (!st.grpMenuKey[0])
				return;
			// ── SURCOUCHE : COUCHE 50, comme tous les menus ─────────────────
			// Le registre donne le survol a la couche la PLUS HAUTE. Peint sur
			// la couche du panneau, ce menu ne gagnait pas ses propres clics :
			// les entrees ne repondaient pas, donc rien n'etait copie (« coller
			// reste grise ») et le menu ne se refermait jamais (Rihen). C'est le
			// meme dispositif que les menus de scene et les listes deroulees.
			NkHitRegistry::LayerScope menuLayer(hit, 50);
			static const char *const kIt[3] = {"Copier les proprietes",
											   "Coller les proprietes", "Reinitialiser"};
			const float32 rowH2 = S(22.f);
			float32 wI = 0.f;
			for (int32 i = 0; i < 3; ++i)
				if (p.TextW(kIt[i]) > wI)
					wI = p.TextW(kIt[i]);
			const float32 pw = wI + S(26.f), ph = rowH2 * 3.f + S(6.f);
			const NkRect &a = st.grpMenuAnchor;
			NkRect pr{a.x + a.w - pw, a.y + a.h + S(2.f), pw, ph};
			if (pr.x < S(4.f))
				pr.x = S(4.f);
			hit.Add("prop.grpmenu", pr);
			p.Fill(pr, NkRole::PanelBg, 4.f);
			p.OutlineSharp(pr, NkRole::Border);
			float32 yy = pr.y + S(3.f);
			// COLLER n'a de sens que depuis un groupe de MEME nature : coller
			// une Transformation dans un Brouillard ne veut rien dire.
			const bool canPaste = NkGrpCanPaste(st, st.grpMenuKey);
			char ik[32];
			for (int32 i = 0; i < 3; ++i) {
				snprintf(ik, sizeof(ik), "prop.grpmenu.%d", i);
				const NkRect ir{pr.x + S(3.f), yy, pr.w - S(6.f), rowH2};
				const bool en = (i == 0) || (i == 1 && canPaste) || i == 2;
				const bool ov = hit.Add(ik, ir);
				if (ov && en)
					p.Fill(ir, NkRole::PanelHeader, 3.f);
				p.TextV(ir.x + S(8.f), yy, rowH2, kIt[i],
						en ? NkRole::Text : NkRole::TextMuted);
				if (en && hit.Clicked(ik)) {
					if (i == 0)
						NkWidgetState::Copy(st.grpClipKey, st.grpMenuKey, 39u);
					// L'INTENTION est posee ici ; c'est la categorie
					// proprietaire du groupe qui l'execute, elle seule sait ce
					// que « copier » veut dire pour ses champs.
					NkWidgetState::Copy(st.grpActionKey, st.grpMenuKey, 39u);
					st.grpAction = i + 1;
					st.grpMenuKey[0] = 0;
				}
				yy += rowH2;
			}
			// SURCOUCHE BLOQUANTE, par le mecanisme DEJA en place pour les menus
			// de la hierarchie (repris de NKCode) : l'emprise memorisee d'une
			// frame sur l'autre, que les panneaux consultent avant d'accepter un
			// clic. SetBlock ne suffisait pas ici -- ce panneau est peint APRES
			// le contenu, qui avait deja tranche ses propres clics.
			st.UiBlockAdd(pr);
			// LE CLIC D'OUVERTURE NE DOIT PAS REFERMER. Le bandeau est peint
			// AVANT ce menu : dans la meme image, le clic qui vient de poser
			// l'ouverture tombait ici comme un « clic exterieur » et refermait
			// aussitot (constate par Rihen ; meme piege que le panneau matcap).
			// L'emprise du BOUTON d'ancrage est donc exclue.
			if (hit.AnyClick() && !NkHitRegistry::Contains(pr, in.mousePos) &&
				!NkHitRegistry::Contains(st.grpMenuAnchor, in.mousePos))
				st.grpMenuKey[0] = 0;
		}
		// Renvoie la HAUTEUR REELLE consommee : un groupe SANS titre (Dimensions,
		// qui n'a qu'une ligne) ne reserve pas la ligne du titre -- elle laissait
		// un grand vide en haut du bloc (constate par Rihen).
		// `neutral` est la valeur de REMISE A ZERO, donnee explicitement par
		// l'appelant : 0 pour une position, une rotation ou un pivot, 1 pour une
		// echelle. La deduire du pas etait faux -- position et echelle ont le meme
		// pas, et la position revenait donc a 1 (constate par Rihen).
		inline float32 PaintXformGroup(NkModelerPainter &p, NkHitRegistry &hit,
									   NkWidgetState &ws, const nkgui::NkGuiInput &in,
									   const NkRect &r, float32 y, const char *title,
									   float32 *v, float32 step, const char *keyBase,
									   bool &locked, bool &prop, const char *fmt = "%.2f",
									   float32 neutral = 0.f) {
			const bool hasTitle = title && title[0];
			if (hasTitle)
				p.TextV(r.x, y, kRowH, title);
			const float32 ry = hasTitle ? y + kRowH : y;
			const float32 rowH = S(22.f);
			// Boutons carres, a la MEME hauteur que les champs : ils suivent leur
			// taille (Rihen). Champs plus etroits -> boutons plus petits.
			const float32 btn = rowH;
			const float32 gap = S(3.f);
			const float32 iconsW = btn * 3.f + gap * 2.f;
			// LARGEUR PROPORTIONNELLE : les trois cellules se partagent tout
			// l'espace laisse par les commandes, et grandissent ou retrecissent
			// avec le panneau (Rihen). Seul un plancher les protege d'un panneau
			// trop etroit -- au-dela, c'est le clip du champ qui tronque.
			float32 cell = (r.w - iconsW - gap * 3.f) / 3.f;
			if (cell < S(30.f))
				cell = S(30.f);
			// LA COULEUR D'AXE EST DEJA DANS LE CHAMP (liseré gauche du champ) : en
			// remettre une a l'exterieur la disait deux fois (Rihen).
			static const NkRole kAxisRole[3] = {NkRole::AxisX, NkRole::AxisY, NkRole::AxisZ};
			char key[56];
			float32 x = r.x;
			for (int32 i = 0; i < 3; ++i) {
				snprintf(key, sizeof(key), "%s.%d", keyBase, i);
				DragFloat(p, hit, ws, in, key, {x, ry, cell, rowH}, v[i], step,
						  kAxisRole[i], fmt);
				x += cell + gap;
			}
			// Les trois commandes, dans l'ordre de la maquette : cadenas, remise a
			// zero, proportionnel. Elles s'ALLUMENT quand elles sont actives -- une
			// icone qui ne dit pas son etat oblige a essayer pour savoir.
			const NkIcon ics[3] = {locked ? NkIcon::Lock : NkIcon::Unlock, NkIcon::Refresh,
								   NkIcon::Link2};
			x = r.x + r.w - iconsW;
			for (int32 i = 0; i < 3; ++i) {
				snprintf(key, sizeof(key), "%s.ic%d", keyBase, i);
				const NkRect br{x, ry, btn, rowH};
				const bool over = hit.Add(key, br);
				const bool on = (i == 0 && locked) || (i == 2 && prop);
				p.Outline(br, on ? NkRole::AccentUi
								 : (over ? NkRole::AccentUi : NkRole::Border),
						  on ? NkRole::AccentUi : NkRole::PanelHeader, 3.f);
				p.IconV(br.x + (btn - S(11.f)) * 0.5f, br.y, rowH, ics[i],
						on ? NkRole::TextOnAccent : NkRole::TextMuted, 11.f);
				if (hit.Clicked(key)) {
					if (i == 0)
						locked = !locked;
					else if (i == 1)
						v[0] = v[1] = v[2] = neutral; // valeur de repos du groupe
					else
						prop = !prop;
				}
				x += btn + gap;
			}
			// HAUTEUR NETTE, sans marge de fin : l'espacement entre deux lignes
			// appartient a l'appelant, sinon un groupe d'UNE ligne (Dimensions)
			// herite d'un vide en bas que rien ne justifie (Rihen).
			return (hasTitle ? kRowH : 0.f) + rowH;
		}

		// â”€â”€ EN-TETE DE SECTION REPLIABLE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// La fleche REPLIE VRAIMENT la section. Une fleche qui ne fait rien est pire
		// qu'une absence de fleche : elle promet une commande et ne la tient pas.
		inline bool SectionHeader(NkModelerPainter &p, NkHitRegistry &hit, const NkRect &r, float32 y,
								  const char *key, const char *title, bool &open) {
			const NkRect hr{r.x, y, r.w, kRowH};
			const bool over = hit.Add(key, hr);
			// L'EN-TETE a TOUJOURS son fond propre, distinct du fond des valeurs
			// (demande de Rihen) ; le survol s'annonce par un lisere accent.
			p.Fill(hr, NkRole::PanelHeader);
			if (over)
				p.Fill({hr.x, hr.y + hr.h - S(2.f), hr.w, S(2.f)}, NkRole::AccentUi);
			p.IconV(r.x + S(6.f), y, kRowH, open ? NkIcon::ChevronDown : NkIcon::ChevronRight,
					NkRole::Text, 11.f);
			p.TextV(r.x + S(22.f), y, kRowH, title);
			if (hit.Clicked(key))
				open = !open;
			return open;
		}

		// â”€â”€ PANNEAU DROIT UNIQUE : OBJET / SCENE / OUTIL â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		// Trois SOUS-BLOCS repliables, chacun son DEFILEMENT : le contenu d'une
		// section peut etre tres long sans pousser les autres hors de l'ecran.
		// La hauteur se partage entre les sections OUVERTES ; la hauteur de
		// contenu est mesuree a l'image precedente (stable des la deuxieme).
		/// L'ETAT DU DIALOGUE D'EMPLACEMENT. En statique de fichier et non dans
		/// NkModelerState : NkModelerFileDialog.h inclut deja cet etat, l'y mettre
		/// ferait un cycle d'inclusion. Un seul dialogue a la fois, donc un seul
		/// etat suffit.
		inline NkFileDialogState &NkMatFileDlg() {
			static NkFileDialogState d;
			return d;
		}

		// ── BOUTON DE PROPRIETE ─────────────────────────────────────────────────
		// Le bouton pleine largeur des panneaux de reglage. Fonction LIBRE : les
		// pastilles extraites de PaintPropertiesUnified en ont besoin autant que
		// le corps principal, et une lambda ne se partage pas.
		inline bool NkPropButton(NkModelerPainter &p, NkHitRegistry &hit, const char *key,
								 float32 yB, const char *label, float32 x, float32 w) {
			const NkRect br{x, yB + S(2.f), w, kRowH - S(4.f)};
			const bool over = hit.Add(key, br);
			p.Outline(br, over ? NkRole::AccentUi : NkRole::Border, NkRole::PanelHeader, 3.f);
			const float32 tw = p.TextW(label);
			p.TextV(br.x + (br.w - tw) * 0.5f, yB, kRowH, label);
			return hit.Clicked(key);
		}

		// ── PASTILLE « SCENE » ──────────────────────────────────────────────────
		// Vue, echelle exacte (cisaillement) et unites de mesure. PREMIERE pastille
		// sortie de PaintPropertiesUnified : le corps de cette fonction faisait
		// ~6 400 lignes, toutes pastilles empilees. Chacune ne partageait avec les
		// autres qu'un petit contexte -- le rect du panneau, celui de la ligne
		// courante, et la position verticale, qui AVANCE (d'ou le `yy` par
		// reference). C'est ce contexte, et lui seul, qui passe en parametres.
		inline void PaintPropScene(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
								   NkWidgetState &ws, const nkgui::NkGuiInput &in,
								   NkComboPending &combo, nkgui::NkGuiContext *guiCtx,
								   const NkRect &r, const NkRect &rr, float32 &yy) {
			auto Button = [&](const char *k2, float32 yB, const char *label, float32 x,
							  float32 w) -> bool {
				return NkPropButton(p, hit, k2, yB, label, x, w);
			};
			(void)Button;
			// Tampons de TRAVAIL, pas un etat partage : la fonction d'origine en
			// gardait un pour toute sa longueur, chaque pastille extraite a le sien.
			char key[64];
			char buf[160];
			(void)buf;
					// ── LA SCENE, EN GROUPES REPLIABLES (Rihen) : meme facture
					// que Transformation, Dimensions ou Sol -- une categorie qui
					// deroule quinze champs a la file ne se lit pas.
					NkRect rowR = rr;
					rowR.x = r.x + NkPropInset();
					rowR.w = rr.w - 2.f * NkPropInset();
					const bool grpView = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.view",
														"Vue", 256u);
					const float32 grpViewTop = yy;
					if (grpView) {
						yy += NkGroupPad();
						// MARGES INTERNES : le contenu de ce groupe est ecrit en
						// « r.x + kPad » et « rr.w » ; on SUBSTITUE ces deux rects
						// par ceux du dedans du cadre, comme le groupe Camera --
						// sans quoi les champs collent au bord (Rihen).
						const NkRect iV = NkGroupInner(rowR);
						const NkRect r{iV.x - kPad, rowR.y, iV.w + 2.f * kPad, rowR.h};
						const NkRect rr = r;
						// Gabarit de champ DERIVE des rects substitues : calcule
						// avant eux, il gardait la largeur du panneau entier et
						// les champs debordaient du cadre (Rihen).
						NkRect fr{r.x + S(120.f), yy + S(3.f), rr.w - S(128.f), kRowH - S(4.f)};
					{
						p.TextV(r.x + kPad, yy, kRowH, "Projection", NkRole::TextMuted);
						const bool o = demo::Demo3DHostIsOrtho();
						const float32 bw = (rr.w - S(132.f)) * 0.5f;
						const NkRect b1{r.x + S(120.f), yy + S(2.f), bw, kRowH - S(4.f)};
						hit.Add("props.persp", b1);
						if (!o)
							p.Fill(b1, NkRole::AccentUi, 3.f);
						else
							p.Outline(b1, NkRole::Border, NkRole::PanelHeader, 3.f);
						p.TextV(b1.x + S(8.f), yy, kRowH, "Persp.",
								!o ? NkRole::TextOnAccent : NkRole::Text);
						if (hit.Clicked("props.persp")) {
							demo::Demo3DHostSetOrtho(false);
							NkMarkDirty(st);
							st.projection = 0;
							st.lastProjection = 0;
						}
						const NkRect b2{r.x + S(120.f) + bw + S(4.f), yy + S(2.f), bw, kRowH - S(4.f)};
						hit.Add("props.ortho", b2);
						if (o)
							p.Fill(b2, NkRole::AccentUi, 3.f);
						else
							p.Outline(b2, NkRole::Border, NkRole::PanelHeader, 3.f);
						p.TextV(b2.x + S(8.f), yy, kRowH, "Ortho.",
								o ? NkRole::TextOnAccent : NkRole::Text);
						if (hit.Clicked("props.ortho")) {
							demo::Demo3DHostSetOrtho(true);
							NkMarkDirty(st);
							st.projection = 1;
							st.lastProjection = 1;
						}
						yy += kRowH;
					}
					fr.y = yy + S(3.f);
					{
						p.TextV(r.x + kPad, yy, kRowH, "Taille ortho", NkRole::TextMuted);
						float32 os = demo::Demo3DHostOrthoScale();
						if (DragFloat(p, hit, ws, in, "props.oscale", fr, os, 0.005f,
									  NkRole::AccentUi, "%.2f")) {
							demo::Demo3DHostSetOrthoScale(os);
							NkMarkDirty(st);
						}
						yy += kRowH;
					}
					{
						p.TextV(r.x + kPad, yy, kRowH, "Distance (0=auto)", NkRole::TextMuted);
						NkRect fr2 = fr;
						fr2.y = yy + S(3.f);
						float32 fv = demo::Demo3DHostViewFar();
						if (DragFloat(p, hit, ws, in, "props.far", fr2, fv, 5.f, NkRole::AccentUi,
									  "%.0f")) {
							demo::Demo3DHostSetViewFar(fv < 20.f ? 0.f : fv);
							NkMarkDirty(st);
						}
						yy += kRowH;
					}
					{
						p.TextV(r.x + kPad, yy, kRowH, "Etendue grille", NkRole::TextMuted);
						NkRect fr2 = fr;
						fr2.y = yy + S(3.f);
						float32 ge = (float32)demo::Demo3DHostGridExtent();
						if (DragFloat(p, hit, ws, in, "props.grid", fr2, ge, 1.f, NkRole::AccentUi,
									  "%.0f")) {
							demo::Demo3DHostSetGridExtent((int32)(ge + 0.5f));
							NkMarkDirty(st);
						}
						yy += kRowH;
					}
					// ── ECHELLE EXACTE (cisaillement) ────────────────────────
					// Coupee : l'echelle est projetee sur les axes de l'objet, il
					// ne se deforme jamais de travers (choix d'Unreal, notre
					// defaut). Active : un scale en repere GLOBAL sur un objet
					// TOURNE le cisaille vraiment -- un carre devient un losange,
					// comme dans un logiciel qui garde une matrice complete.
					{
						const bool shOn = demo::Demo3DHostShearScale();
						const NkRect cb{r.x + kPad, yy + S(5.f), S(12.f), S(12.f)};
						hit.Add("props.shear", cb);
						p.Outline(cb, shOn ? NkRole::AccentUi : NkRole::Border,
								  shOn ? NkRole::AccentUi : NkRole::InputBg, 2.f);
						p.TextV(cb.x + S(18.f), yy, kRowH, "Echelle exacte (cisaillement)",
								NkRole::TextMuted);
						if (hit.Clicked("props.shear")) {
							demo::Demo3DHostSetShearScale(!shOn);
							NkMarkDirty(st);
						}
						yy += kRowH;
					}
						yy += NkGroupPad();
						PaintGroupBlock(p, rowR, grpViewTop, yy);
					}
					yy += NkPropGroupGap();
					// ── GROUPE « UNITES » ────────────────────────────────────
					const bool grpUnit = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.unit",
														"Unites", 512u);
					const float32 grpUnitTop = yy;
					if (grpUnit) {
						yy += NkGroupPad();
						// Memes marges internes que le groupe « Vue » ci-dessus.
						const NkRect iU = NkGroupInner(rowR);
						const NkRect r{iU.x - kPad, rowR.y, iU.w + 2.f * kPad, rowR.h};
						const NkRect rr = r;
						NkRect fr{r.x + S(120.f), yy + S(3.f), rr.w - S(128.f), kRowH - S(4.f)};
					{
						// ── UNITES DE MESURE DE LA SCENE (Rihen) ─────────────────
						// Declaratif pour l'instant : les champs restent en unites
						// scene ; la conversion d'affichage viendra avec le format
						// projet.
						p.TextV(r.x + kPad, yy, kRowH, "Unites", NkRole::TextMuted);
						static const char *const kUSys[3] = {"Metrique", "Imperial", "Aucun"};
						Combo(p, hit, ws, "props.usys",
							  {r.x + S(120.f), yy + S(2.f), rr.w - S(128.f), kRowH - S(4.f)},
							  kUSys, nullptr, 3, st.unitSystem, combo);
						yy += kRowH;
						if (st.unitSystem != 2) {
							p.TextV(r.x + kPad, yy, kRowH, "Longueur", NkRole::TextMuted);
							static const char *const kUMet[3] = {"Metres", "Centimetres",
																 "Millimetres"};
							static const char *const kUImp[2] = {"Pieds", "Pouces"};
							const NkRect ur{r.x + S(120.f), yy + S(2.f), rr.w - S(128.f),
											kRowH - S(4.f)};
							if (st.unitSystem == 0) {
								Combo(p, hit, ws, "props.ulen", ur, kUMet, nullptr, 3,
									  st.unitLength, combo);
							} else {
								if (st.unitLength > 1)
									st.unitLength = 0;
								Combo(p, hit, ws, "props.ulen", ur, kUImp, nullptr, 2,
									  st.unitLength, combo);
							}
							yy += kRowH;
						}
						p.TextV(r.x + kPad, yy, kRowH, "Echelle d'unite", NkRole::TextMuted);
						NkRect fru = fr;
						fru.y = yy + S(3.f);
						DragFloat(p, hit, ws, in, "props.uscale", fru, st.unitScale, 0.01f,
								  NkRole::AccentUi, "%.2f");
						if (st.unitScale < 0.001f)
							st.unitScale = 0.001f;
						yy += kRowH;
							// CHAQUE SCENE A SES VALEURS (Rihen). Ici, une vraie
							// interface : un COMBO choisit la scene destinataire (ou
							// toutes), un BOUTON fait la copie.
							{
								p.TextV(r.x + kPad, yy, kRowH, "Copier vers",
										NkRole::TextMuted);
								// Le combo liste « Toutes les scenes » puis chaque
								// scene du PROJET -- pas seulement celles qui ont un
								// onglet ouvert : porter ses unites vers une scene
								// fermee est justement ce qu'on veut pouvoir faire.
								static const char *sTgts[NkModelerState::kMaxDocs + 1];
								static int32 sTgtDoc[NkModelerState::kMaxDocs + 1];
								sTgts[0] = "Toutes les scenes";
								int32 nTgt = 1;
								for (int32 d7 = 0; d7 < NkModelerState::kMaxDocs; ++d7) {
									if (!st.docUsed[d7] || st.docTransient[d7])
										continue;
									sTgtDoc[nTgt] = d7;
									sTgts[nTgt] = st.docName[d7];
									++nTgt;
								}
								if (st.propCopyTarget >= nTgt)
									st.propCopyTarget = 0;
								Combo(p, hit, ws, "props.utgt",
									  {r.x + S(120.f), yy + S(2.f), rr.w - S(128.f),
									   kRowH - S(4.f)},
									  sTgts, nullptr, nTgt, st.propCopyTarget, combo);
								yy += kRowH;
								// (Le bouton « Copier les proprietes » a disparu :
								// l'action vit maintenant dans le MENU du bandeau,
								// commun a tous les groupes -- Rihen. Le combo
								// ci-dessus reste : il dit VERS QUELLE scene.)
								// COPIER, demande par le menu du groupe : ce groupe
								// sait ce que ca veut dire -- porter ses unites vers
								// la scene choisie, ou vers toutes.
								if (NkGrpWants(st, "prop.g.unit", 1)) {
									// « Toutes » parcourt les documents ; sinon le seul
									// designe par le combo (sTgtDoc, rempli juste au-dessus).
									for (int32 k7 = 1; k7 < nTgt; ++k7) {
										if (st.propCopyTarget > 0 && k7 != st.propCopyTarget)
											continue;
										const int32 d8 = sTgtDoc[k7];
										st.docUnitSys[d8] = st.unitSystem;
										st.docUnitLen[d8] = st.unitLength;
										st.docUnitScale[d8] = st.unitScale;
									}
									// ... et dans le presse-papiers, pour un collage
									// vers une autre scene plus tard.
									const float32 u3[3] = {(float32)st.unitSystem,
														   (float32)st.unitLength,
														   st.unitScale};
									NkGrpCopyF(st, "prop.g.unit", u3, 3);
								}
								if (NkGrpWants(st, "prop.g.unit", 2) &&
									NkGrpCanPaste(st, "prop.g.unit")) {
									st.unitSystem = (int32)(st.grpClipF[0] + 0.5f);
									st.unitLength = (int32)(st.grpClipF[1] + 0.5f);
									st.unitScale = st.grpClipF[2];
								}
								if (NkGrpWants(st, "prop.g.unit", 3)) {
									st.unitSystem = 0; // metrique
									st.unitLength = 0; // metres
									st.unitScale = 1.f;
								}
							}
					}
					{
						// MATCAP : un COMBO avec l'APERCU de la boule choisie ; le clic
						// ouvre le panneau par categories, ancre ICI.
						p.TextV(r.x + kPad, yy, kRowH, "Matcap", NkRole::TextMuted);
						const int32 mc = demo::Demo3DHostMatcap();
						const NkRect br{r.x + S(120.f), yy + S(2.f), rr.w - S(128.f), kRowH - S(4.f)};
						const bool overM = hit.Add("props.matcap", br);
						p.Outline(br, (overM || st.matcapOpen) ? NkRole::AccentUi : NkRole::Border,
								  NkRole::InputBg, 3.f);
						p.Image(4300u + (uint32)mc,
								{br.x + S(3.f), br.y + S(2.f), br.h - S(4.f), br.h - S(4.f)});
						p.TextV(br.x + br.h + S(4.f), yy, kRowH, demo::Demo3DHostMatcapName(mc));
						// Marqueur blanc de combo, comme partout.
						p.Fill({br.x + br.w - S(6.f), br.y + br.h - S(6.f), S(3.f), S(3.f)},
							   NkRole::Text);
						if (hit.Clicked("props.matcap")) {
							st.matcapOpen = !st.matcapOpen;
							st.matcapAnchor = br;
						}
						yy += kRowH;
						// L'APERCU, EN GRAND : une vignette de 22 px dit « il y a une
						// boule », pas « quelle matiere c'est ». Le carre reprend la
						// meme texture, en 72 px.
						p.Image(4300u + (uint32)mc, {r.x + S(120.f), yy + S(2.f), S(72.f), S(72.f)});
						yy += S(78.f);
					}
					{
						// â”€â”€ FOND PAR TYPE : un COMBO, et les proprietes du type choisi
						// juste en dessous (Rihen). Seule la COULEUR UNIE agit
						// aujourd'hui ; degrade, texture, HDRI et ciel montrent leurs
						// proprietes en annoncant le chantier moteur -- certains ne
						// seront visibles qu'en mode Rendu/Materiaux.
						p.TextV(r.x + kPad, yy, kRowH, "Fond", NkRole::TextMuted);
						static const char *const kBgTypes[5] = {"Couleur unie", "Degrade", "Texture",
																"HDRI", "Ciel"};
						Combo(p, hit, ws, "props.bgtype", {r.x + S(120.f), yy + S(2.f), rr.w - S(128.f),
														   kRowH - S(4.f)},
							  kBgTypes, nullptr, 5, st.bgType, combo);
						yy += kRowH;
						if (st.bgType == 0) {
							p.TextV(r.x + kPad, yy, kRowH, "Couleur", NkRole::TextMuted);
							float32 bx = r.x + S(120.f);
							for (int32 i3 = 0; i3 < 6; ++i3) {
								snprintf(key, sizeof(key), "props.bg.%d", i3);
								const NkRect br{bx, yy + S(4.f), S(18.f), kRowH - S(8.f)};
								hit.Add(key, br);
								float32 c[3];
								NkBgColorOf(st, i3, c);
								p.Fill(br, NkColor{(uint8)(c[0] * 255.f), (uint8)(c[1] * 255.f),
												   (uint8)(c[2] * 255.f), 255},
									   2.f);
								if (st.bgChoice == i3)
									p.OutlineSharp(br, NkRole::AccentUi);
								if (hit.Clicked(key))
									st.bgChoice = i3;
								bx += S(22.f);
							}
						yy += kRowH;
						// LUMINOSITE : plus sombre ou plus claire, quelle que soit
						// la couleur choisie (Rihen).
						p.TextV(r.x + kPad, yy, kRowH, "Luminosite", NkRole::TextMuted);
						if (DragFloat(p, hit, ws, in, "props.bglum",
									  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
									   kRowH - S(4.f)},
									  st.bgBrightness, 0.005f, NkRole::AccentUi, "%.2f")) {
							if (st.bgBrightness < 0.1f)
								st.bgBrightness = 0.1f;
							if (st.bgBrightness > 2.f)
								st.bgBrightness = 2.f;
						}
						yy += kRowH;
						if (st.bgChoice == 5) {
							// LE VRAI PICKER (carre SV + barre de teinte, transpose du
							// ColorPicker4 de NKGui) ; les champs R/V/B restent dessous
							// pour les valeurs exactes. Le fond suit EN DIRECT.
							{
								const NkRect pk{r.x + S(16.f), yy + S(2.f), rr.w - S(28.f), S(120.f)};
								NkColorPickerSV(p, hit, st.propDragKey, sizeof(st.propDragKey),
												"props.bgpick", pk, st.bgCustom);
								yy += S(126.f);
							}
							static const char *const kCh[3] = {"Rouge", "Vert", "Bleu"};
							for (int32 c2 = 0; c2 < 3; ++c2) {
								p.TextV(r.x + kPad + S(8.f), yy, kRowH, kCh[c2], NkRole::TextMuted);
								snprintf(key, sizeof(key), "props.bgc.%d", c2);
								float32 v2 = st.bgCustom[c2];
								if (DragFloat(p, hit, ws, in, key,
											 {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
											  kRowH - S(4.f)},
											 v2, 0.005f, NkRole::AccentUi, "%.2f")) {
									if (v2 < 0.f)
										v2 = 0.f;
									if (v2 > 1.f)
										v2 = 1.f;
									st.bgCustom[c2] = v2;
								}
								yy += kRowH;
							}
						}
						} else if (st.bgType == 1) {
							p.TextV(r.x + kPad, yy, kRowH, "Haut / horizon / bas : trois",
									NkRole::TextMuted);
							yy += kRowH - S(6.f);
							p.TextV(r.x + kPad, yy, kRowH, "couleurs -- fond moteur, a venir.",
									NkRole::TextMuted);
							yy += kRowH;
						} else if (st.bgType == 2 || st.bgType == 3) {
							p.TextV(r.x + kPad, yy, kRowH,
									st.bgType == 2 ? "Fichier image : (aucun)" : "Fichier .hdr : (aucun)",
									NkRole::TextMuted);
							yy += kRowH - S(6.f);
							p.TextV(r.x + kPad, yy, kRowH, "Visible en mode Rendu -- a venir.",
									NkRole::TextMuted);
							yy += kRowH;
						} else {
							p.TextV(r.x + kPad, yy, kRowH, "Ciel procedural (mode Rendu) --",
									NkRole::TextMuted);
							yy += kRowH - S(6.f);
							p.TextV(r.x + kPad, yy, kRowH, "a venir avec le fond moteur.",
									NkRole::TextMuted);
							yy += kRowH;
						}
					}
						yy += NkGroupPad();
						PaintGroupBlock(p, rowR, grpUnitTop, yy);
					}
					yy += NkPropGroupGap();
		}

		// ── PASTILLE « RENDU / MONDE » ──────────────────────────────────────────
		// Ombres globales, ambiance et ciel (modele, etoiles, lunes, nuages),
		// brouillard et sol infini, occlusion ambiante, exposition et bloom.
		// Tout ce qui vaut pour la SCENE ENTIERE plutot que pour un objet.
		inline void PaintPropWorld(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
								   NkWidgetState &ws, const nkgui::NkGuiInput &in,
								   NkComboPending &combo, nkgui::NkGuiContext *guiCtx,
								   const NkRect &r, const NkRect &rr, float32 &yy) {
			auto Button = [&](const char *k2, float32 yB, const char *label, float32 x,
							  float32 w) -> bool {
				return NkPropButton(p, hit, k2, yB, label, x, w);
			};
			(void)Button;
			char key[64];
			char buf[160];
			(void)buf;
					// ── RENDU : LES OMBRES ──────────────────────────────────────
					// Elles sont GLOBALES au rendu -- une ombre douce l'est pour
					// toute la scene -- donc leur place est ici, et non sur chaque
					// lumiere, qui ne garde que son interrupteur d'ombre (c'est
					// aussi le partage de Blender entre Rendu et Lumiere).
					NkRect rowR = rr;
					rowR.x = r.x + NkPropInset();
					rowR.w = rr.w - 2.f * NkPropInset();
					// ── ECLAIRAGE D'AMBIANCE ────────────────────────────────────
					// Ce que la scene recoit de son ENVIRONNEMENT, sans aucune
					// source. A zero, un objet hors de toute lumiere est noir --
					// c'est ce qu'on attend d'un rendu, et ce que fait Blender.
					{
						NkRect aR = rr;
						aR.x = r.x + NkPropInset();
						aR.w = rr.w - 2.f * NkPropInset();
						const float32 aTop = yy;
						if (PaintPropGroup(p, hit, st, aR, yy, "prop.g.amb", "Ambiance", 2u)) {
							const NkRect iA = NkGroupInner(aR);
							yy += NkGroupPad();
							p.TextV(iA.x, yy, kRowH, "Intensite", NkRole::TextMuted);
							float32 amb = demo::Demo3DHostAmbient();
							const float32 amb0 = amb;
							DragFloat(p, hit, ws, in, "prop.amb",
									  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
									   kRowH - S(6.f)},
									  amb, 0.005f, NkRole::AccentUi, "%.3f");
							if (amb != amb0) {
								demo::Demo3DHostSetAmbient(amb);
								NkMarkDirty(st);
							}
							yy += kRowH;
							// LA TEINTE de l'ambiance, avec le meme picker que
							// partout ailleurs : blanc = neutre.
							{
								float32 ac[3];
								demo::Demo3DHostAmbientColor(ac);
								bool acCh = false;
								yy += PaintColorRow(p, hit, ws, in, st, iA, yy, "Couleur",
													"prop.ambcol", ac, &acCh);
								if (acCh) {
									demo::Demo3DHostSetAmbientColor(ac);
									NkMarkDirty(st);
								}
							}
						// ── D'OU VIENT L'AMBIANCE ? ─────────────────────────
							// Couleur unie : un aplat, comme le monde par defaut de
							// Blender. Ciel procedural : trois couleurs dont le
							// moteur deduit l'eclairage -- il est donc DIRECTIONNEL,
							// c'est lui qui eclairait trois faces plus que les
							// autres. Image HDRI : une photo 360 qui apporte la
							// lumiere ET les reflets d'un lieu reel.
							{
								static const char *const kSrc[3] = {"Couleur unie",
																	"Ciel procedural",
																	"Image HDRI"};
								p.TextV(iA.x, yy, kRowH, "Source", NkRole::TextMuted);
								// MEMOIRE DU POUSSE, pas de capture dans la frame :
								// DrawComboPopup ecrit st.envSource en FIN d'image,
								// donc « capturer avant / comparer apres » ne voit
								// jamais le changement (meme panne que le modele de
								// ciel). Le moteur ne stocke qu'un booleen pour trois
								// choix : impossible de relire la verite, on memorise
								// donc ce qu'on a reellement pousse.
								static int32 pushedSrc = -1;
								if (pushedSrc < 0)
									pushedSrc = st.envSource;
								Combo(p, hit, ws, "prop.amb.src",
									  {iA.x + S(110.f), yy + S(2.f), iA.w - S(110.f),
									   kRowH - S(4.f)},
									  kSrc, nullptr, 3, st.envSource, combo);
								if (st.envSource != pushedSrc) {
									pushedSrc = st.envSource;
									demo::Demo3DHostSetAmbientUseEnv(st.envSource != 0);
									NkMarkDirty(st);
								}
								yy += kRowH;
							}
							// ── LE CIEL SE VOIT-IL ? ────────────────────────────
							// Reglage SEPARE de la source ci-dessus, et il doit le
							// rester : « d'ou vient la lumiere » et « qu'est-ce
							// qu'on voit derriere » sont deux questions distinctes.
							// On eclaire souvent une scene avec un HDRI sans
							// l'afficher en fond, et on affiche parfois un ciel qui
							// ne pilote pas l'ambiance.
							// Le moteur savait deja le faire (NkRender3D::
							// SetSkyboxEnabled, shader Skybox compile au demarrage) ;
							// simplement, AUCUNE ligne de l'application ne le lui
							// demandait -- le ciel ne pouvait donc jamais apparaitre.
							{
								bool skyOn = demo::Demo3DHostSkyVisible();
								const NkRect cb{iA.x, yy + S(5.f), S(12.f), S(12.f)};
								hit.Add("prop.amb.skyvis", cb);
								p.Outline(cb, skyOn ? NkRole::AccentUi : NkRole::Border,
										  skyOn ? NkRole::AccentUi : NkRole::InputBg, 2.f);
								p.TextV(cb.x + S(18.f), yy, kRowH, "Afficher le ciel",
										NkRole::TextMuted);
								if (hit.Clicked("prop.amb.skyvis")) {
									demo::Demo3DHostSetSkyVisible(!skyOn);
									NkMarkDirty(st);
								}
								yy += kRowH;
								// SA LUMINOSITE, encore un reglage a part. Le shader
								// peignait le ciel en le multipliant par l'intensite
								// d'ambiance (0,05) : il sortait quasi noir et
								// paraissait absent alors qu'il etait bien genere.
								// Visible seulement quand le ciel l'est : un curseur
								// sans effet observable n'apprend rien.
								if (skyOn) {
									float32 si = demo::Demo3DHostSkyIntensity();
									const float32 si0 = si;
									p.TextV(iA.x, yy, kRowH, "Luminosite", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "prop.amb.skyint",
											  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
											   kRowH - S(6.f)},
											  si, 0.02f, NkRole::AccentUi, "%.2f");
									if (si != si0) {
										demo::Demo3DHostSetSkyIntensity(si);
										NkMarkDirty(st);
									}
									yy += kRowH;
								}
								// REINITIALISER L'AMBIANCE seule : intensite,
								// teinte et luminosite du ciel. Ni le modele de
								// ciel ni les nuages ne bougent -- ils ont leurs
								// propres boutons.
								{
									const NkRect rra{iA.x, yy + S(2.f), iA.w, kRowH - S(4.f)};
									hit.Add("prop.amb.reset", rra);
									p.Outline(rra, NkRole::Border, NkRole::InputBg, 3.f);
									p.TextV(rra.x + (rra.w - p.TextW("Ambiance par defaut")) * 0.5f,
											yy, kRowH, "Ambiance par defaut", NkRole::TextMuted);
									if (hit.Clicked("prop.amb.reset"))
										demo::Demo3DHostResetAmbient();
									yy += kRowH;
								}
							}
							if (st.envSource == 1) {
								// ── QUEL MODELE DE CIEL ? ───────────────────
								// Degrade : trois couleurs, stylise, previsible.
								// Physique : la couleur DECOULE de la position du
								// soleil et de la turbidite de l'air -- le bleu du
								// zenith, le blanchiment vers l'horizon et les
								// teintes du couchant sortent du modele, on ne les
								// regle pas.
								{
									static const char *const kSkyM[6] = {
										"Degrade", "Physique (Preetham)",
										"Atmosphere (Rayleigh + Mie)",
										"Hosek-Wilkie (mesure)",
										"Prague (mesure, couchants)",
										"Soleil alien (temperature)"};
									// LA VALEUR VIT DANS L'ETAT, jamais en local : le
									// combo retient un POINTEUR dessus et n'ecrit
									// qu'a la frame suivante. Avec une locale, le
									// choix se perdait en silence et le modele
									// restait bloque sur « Degrade ».
									//
									// ET ON NE COMPARE PAS A UNE VALEUR CAPTUREE
									// DANS LA FRAME. DrawComboPopup ecrit
									// *selected en FIN d'image, apres les panneaux :
									// a la frame suivante, un `const int32 v0 = st.x`
									// pris avant l'appel vaut DEJA la nouvelle
									// valeur, et `st.x != v0` est toujours faux. Le
									// poussage vers le moteur n'a alors jamais lieu.
									// On memorise donc CE QU'ON A REELLEMENT POUSSE.
									static int32 pushedModel = 0;
									p.TextV(iA.x, yy, kRowH, "Modele", NkRole::TextMuted);
									Combo(p, hit, ws, "prop.sky.model",
										  {iA.x + S(110.f), yy + S(2.f), iA.w - S(110.f),
										   kRowH - S(4.f)},
										  kSkyM, nullptr, 6, st.skyModel, combo);
									if (st.skyModel != pushedModel) {
										pushedModel = st.skyModel;
										demo::Demo3DHostSetSkyModel(st.skyModel);
										NkMarkDirty(st);
									}
									yy += kRowH;
								}
								const int32 skyModel = st.skyModel;
								bool ch = false;
								float32 top[3], hor[3], gnd[3];
								demo::Demo3DHostEnvSky(top, hor, gnd);
								// ── SOLEIL ALIEN : la temperature de l'etoile ──
								// C'est LE reglage du modele : le monde entier
								// change de teinte avec elle, pas seulement le
								// disque. Effet visible en continu (modele cuit
								// avec rafraichissement auto) ; l'eclairage
								// attend « Regenerer », comme partout.
								if (skyModel == 5) {
									float32 tk = demo::Demo3DHostSkyAlienTemp();
									const float32 tk0 = tk;
									p.TextV(iA.x, yy, kRowH, "Etoile (K)", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "prop.sky.alientemp",
											  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
											   kRowH - S(6.f)},
											  tk, 25.f, NkRole::AccentUi, "%.0f");
									if (tk != tk0) {
										demo::Demo3DHostSetSkyAlienTemp(tk);
										NkMarkDirty(st);
									}
									yy += kRowH;
								}
								if (skyModel == 0) {
									// LES TROIS COULEURS DU CIEL, modifiables.
									yy += PaintColorRow(p, hit, ws, in, st, iA, yy, "Zenith",
														"prop.sky.top", top, &ch);
									yy += PaintColorRow(p, hit, ws, in, st, iA, yy, "Horizon",
														"prop.sky.hor", hor, &ch);
								} else {
									// ── CIEL PHYSIQUE ──────────────────────
									// LE SOLEIL SE DONNE EN ELEVATION / AZIMUT,
									// pas en vecteur : c'est ainsi qu'on pense un
									// soleil, et c'est ce qui permet de viser un
									// couchant sans calculer de composantes.
									float32 sd[3], turb = 2.5f, si = 1.f;
									bool disc = true;
									demo::Demo3DHostSkySun(sd, &turb, &disc, &si);
									// dir = propagation ; le vecteur VERS le soleil
									// est son oppose.
									const float32 tx = -sd[0], ty = -sd[1], tz = -sd[2];
									const float32 tl = sqrtf(tx * tx + ty * ty + tz * tz);
									float32 elev = (tl > 1e-6f) ? asinf(ty / tl) * 57.2957795f : 45.f;
									float32 azim = atan2f(tx, tz) * 57.2957795f;
									const float32 e0 = elev, a0 = azim, t0 = turb, i0 = si;
									const bool d0 = disc;
									// ── QUEL SOLEIL LE CIEL SUIT-IL ? ──────
									// Une scene peut en porter PLUSIEURS : on
									// choisit, on ne devine pas. La source est
									// designee par son NOEUD, donc supprimer une
									// autre lampe ne fait pas changer de soleil.
									// « Manuel » garde la main sur elevation et
									// azimut.
									// TOUT CE QUE LE COMBO RETIENT DOIT SURVIVRE A LA
									// FRAME. NkComboPending garde un pointeur sur la
									// valeur ET sur le TABLEAU D'ITEMS ; la liste
									// n'est peinte qu'apres tout le reste, par
									// DrawComboPopup. Un tableau d'items LOCAL est
									// donc detruit avant d'etre lu -- ce qui ne se
									// perd pas en silence comme pour la valeur, mais
									// fait planter net a l'ouverture de la liste.
									// Les noms sont recalcules a chaque image (une
									// lumiere peut etre renommee), le STOCKAGE, lui,
									// est permanent.
									static int32 sunNodes[16];
									static char sunLbl[17][48];
									static const char *sunItems[17];
									int32 raw[16];
									const int32 rawCount = demo::Demo3DHostSunNodes(raw, 16);
									// ON NE PROPOSE QUE LES SOLEILS QUE L'UTILISATEUR
									// VOIT. Le moteur porte aussi les lumieres de la
									// demo (noeuds 86..89), qui n'apparaissent pas
									// dans la hierarchie : les lister ici faisait
									// surgir un soleil dont Rihen n'a jamais entendu
									// parler. On applique donc EXACTEMENT le meme
									// filtre que l'arbre de scene.
									int32 sunCount = 0;
									for (int32 i = 0; i < rawCount; ++i) {
										if (NkHierNodeSkip(raw[i]))
											continue;
										sunNodes[sunCount++] = raw[i];
									}
									snprintf(sunLbl[0], sizeof(sunLbl[0]), "Manuel");
									sunItems[0] = sunLbl[0];
									for (int32 i = 0; i < sunCount; ++i) {
										NkHierNodeName(st, sunNodes[i], sunLbl[i + 1],
													   (uint32)sizeof(sunLbl[0]));
										sunItems[i + 1] = sunLbl[i + 1];
									}
									const int32 curSun = demo::Demo3DHostSkySunSource();
									static int32 pushedSun = 0;
									// L'HOTE A PU LACHER LA SOURCE tout seul (le
									// soleil suivi a ete supprime). On remet alors le
									// rang a « Manuel » -- mais SEULEMENT si aucun
									// choix n'est en attente, sinon on effacerait ce
									// que le combo vient d'ecrire.
									if (curSun < 0 && st.skySunSel > 0 &&
										pushedSun == st.skySunSel) {
										st.skySunSel = 0;
										pushedSun = 0;
									}
									p.TextV(iA.x, yy, kRowH, "Suit", NkRole::TextMuted);
									Combo(p, hit, ws, "prop.sky.sunsrc",
										  {iA.x + S(110.f), yy + S(2.f), iA.w - S(110.f),
										   kRowH - S(4.f)},
										  sunItems, nullptr, sunCount + 1, st.skySunSel, combo);
									// Meme regle que pour le modele : on compare a CE
									// QU'ON A POUSSE, pas a une valeur capturee dans
									// la frame. Et on BORNE avant d'indexer : entre le
									// clic et son traitement, une lumiere a pu
									// disparaitre et raccourcir la liste.
									if (st.skySunSel != pushedSun) {
										if (st.skySunSel < 0 || st.skySunSel > sunCount)
											st.skySunSel = 0;
										pushedSun = st.skySunSel;
										demo::Demo3DHostSetSkySunSource(
											st.skySunSel <= 0 ? -1 : sunNodes[st.skySunSel - 1]);
										NkMarkDirty(st);
									}
									yy += kRowH;
									// LIE : elevation et azimut sont IMPOSES par la
									// lumiere. On les AFFICHE quand meme, en lecture
									// seule -- un etat qui se propage doit se voir
									// sous sa forme effective, pas rester un champ
									// modifiable dont personne ne tient compte.
									if (curSun >= 0) {
										char sb[64];
										snprintf(sb, sizeof(sb), "%.1f°  /  %.1f°", (double)elev,
												 (double)azim);
										p.TextV(iA.x, yy, kRowH, "Elev. / Azimut",
												NkRole::TextMuted);
										p.TextV(iA.x + S(110.f), yy, kRowH, sb, NkRole::Text);
										yy += kRowH;
									} else {
										p.TextV(iA.x, yy, kRowH, "Elevation", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.sky.elev",
												  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
												   kRowH - S(6.f)},
												  elev, 0.5f, NkRole::AccentUi, "%.1f°");
										yy += kRowH;
										p.TextV(iA.x, yy, kRowH, "Azimut", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.sky.azim",
												  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
												   kRowH - S(6.f)},
												  azim, 1.f, NkRole::AccentUi, "%.1f°");
										yy += kRowH;
									}
									// Turbidite : 1 = air de haute montagne,
									// 2-3 = ciel clair, 6-10 = atmosphere chargee.
									p.TextV(iA.x, yy, kRowH, "Turbidite", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "prop.sky.turb",
											  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
											   kRowH - S(6.f)},
											  turb, 0.05f, NkRole::AccentUi, "%.2f");
									yy += kRowH;
									p.TextV(iA.x, yy, kRowH, "Puissance", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "prop.sky.sunint",
											  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
											   kRowH - S(6.f)},
											  si, 0.02f, NkRole::AccentUi, "%.2f");
									yy += kRowH;
									{
										float32 sc[3];
										demo::Demo3DHostSkySunColor(sc);
										bool scCh = false;
										yy += PaintColorRow(p, hit, ws, in, st, iA, yy, "Couleur",
															"prop.sky.suncol", sc, &scCh);
										if (scCh) {
											demo::Demo3DHostSetSkySunColor(sc);
											NkMarkDirty(st);
										}
									}
									{
										const NkRect cb{iA.x, yy + S(5.f), S(12.f), S(12.f)};
										hit.Add("prop.sky.disc", cb);
										p.Outline(cb, disc ? NkRole::AccentUi : NkRole::Border,
												  disc ? NkRole::AccentUi : NkRole::InputBg, 2.f);
										p.TextV(cb.x + S(18.f), yy, kRowH, "Disque solaire",
												NkRole::TextMuted);
										if (hit.Clicked("prop.sky.disc"))
											disc = !disc;
										yy += kRowH;
									}
									// LE SOLEIL DU CIEL ECLAIRE-T-IL LA SCENE ?
									// Propose UNIQUEMENT en mode Manuel : quand le
									// ciel suit une lumiere, celle-ci eclaire deja,
									// et en ajouter une seconde doublerait
									// l'eclairement sans que rien ne l'explique.
									// C'est ce qui donne au soleil manuel TOUS les
									// effets d'une directionnelle -- ombres portees
									// comprises -- au lieu d'un simple decor.
									if (curSun < 0) {
										bool lightsOn = demo::Demo3DHostSkySunLightsScene();
										const NkRect cb{iA.x, yy + S(5.f), S(12.f), S(12.f)};
										hit.Add("prop.sky.sunlight", cb);
										p.Outline(cb,
												  lightsOn ? NkRole::AccentUi : NkRole::Border,
												  lightsOn ? NkRole::AccentUi : NkRole::InputBg,
												  2.f);
										p.TextV(cb.x + S(18.f), yy, kRowH, "Eclaire la scene",
												NkRole::TextMuted);
										if (hit.Clicked("prop.sky.sunlight")) {
											demo::Demo3DHostSetSkySunLightsScene(!lightsOn);
											NkMarkDirty(st);
										}
										yy += kRowH;
									}
									if (elev != e0 || azim != a0 || turb != t0 || si != i0 ||
										disc != d0) {
										const float32 er = elev * 0.0174532925f;
										const float32 ar = azim * 0.0174532925f;
										const float32 nd[3] = {-cosf(er) * sinf(ar), -sinf(er),
															   -cosf(er) * cosf(ar)};
										demo::Demo3DHostSetSkySun(nd, turb, disc, si);
										NkMarkDirty(st);
									}
								}
								// LE SOL sert aux DEUX modeles : le ciel physique
								// n'est pas defini sous l'horizon, on y pose cette
								// couleur.
								yy += PaintColorRow(p, hit, ws, in, st, iA, yy, "Sol",
													"prop.sky.gnd", gnd, &ch);
								if (ch) {
									demo::Demo3DHostSetEnvSky(top, hor, gnd);
									NkMarkDirty(st);
								}
								// ── ETOILES ─────────────────────────────────
								// Elles s'effacent SEULES quand le ciel
								// s'eclaire : leur visibilite est l'inverse de
								// la luminosite locale. Un cycle jour/nuit les
								// fera donc apparaitre et disparaitre sans
								// qu'on les pilote -- comme les teintes du
								// couchant decoulent du modele physique.
								// Effet immediat : aucune regeneration.
								{
									float32 si2 = 0.f, sd2 = 200.f;
									demo::Demo3DHostSkyStars(&si2, &sd2);
									const float32 a0 = si2, b0 = sd2;
									p.TextV(iA.x, yy, kRowH, "Etoiles", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "prop.sky.stars",
											  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
											   kRowH - S(6.f)},
											  si2, 0.02f, NkRole::AccentUi, "%.2f");
									yy += kRowH;
									if (si2 > 0.001f) {
										p.TextV(iA.x, yy, kRowH, "Densite", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.sky.stard",
												  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
												   kRowH - S(6.f)},
												  sd2, 2.f, NkRole::AccentUi, "%.0f");
										yy += kRowH;
									}
									if (si2 != a0 || sd2 != b0) {
										demo::Demo3DHostSetSkyStars(si2, sd2);
										NkMarkDirty(st);
									}
									// MOUVEMENT : rotation de la voute et etoiles
									// filantes. Propose seulement si les etoiles
									// sont allumees — faire tourner un ciel vide
									// ou y lancer des filantes invisibles n'a
									// aucun sens.
									if (si2 > 0.001f) {
										float32 rot = 0.f, sho = 0.f;
										demo::Demo3DHostSkyStarMotion(&rot, &sho);
										const float32 r0 = rot, s0s = sho;
										p.TextV(iA.x, yy, kRowH, "Rotation", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.sky.starrot",
												  {iA.x + S(110.f), yy + S(3.f),
												   iA.w - S(110.f), kRowH - S(6.f)},
												  rot, 0.002f, NkRole::AccentUi, "%.3f");
										yy += kRowH;
										p.TextV(iA.x, yy, kRowH, "Filantes / min",
												NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.sky.shoot",
												  {iA.x + S(110.f), yy + S(3.f),
												   iA.w - S(110.f), kRowH - S(6.f)},
												  sho, 0.2f, NkRole::AccentUi, "%.1f");
										yy += kRowH;
										if (rot != r0 || sho != s0s) {
											demo::Demo3DHostSetSkyStarMotion(rot, sho);
											NkMarkDirty(st);
										}
									}
								}
								// ── LUNES ───────────────────────────────────
								// PLUSIEURS sont possibles : c'est un nombre, pas
								// un interrupteur. Leur PHASE n'est pas reglable
								// -- elle se deduit du soleil, donc le croissant
								// change tout seul quand il descend.
								{
									int32 mc = demo::Demo3DHostSkyMoonCount();
									const int32 mc0 = mc;
									p.TextV(iA.x, yy, kRowH, "Lunes", NkRole::TextMuted);
									const float32 gm = S(3.f);
									const float32 bwm = (iA.w - S(110.f) - gm * 2.f) / 3.f;
									float32 bxm = iA.x + S(110.f);
									char km[24];
									for (int32 t = 0; t < 3; ++t) {
										snprintf(km, sizeof(km), "prop.sky.moonn%d", t);
										const NkRect br{bxm, yy + S(2.f), bwm, kRowH - S(4.f)};
										const bool on = (t == mc);
										hit.Add(km, br);
										p.Outline(br, on ? NkRole::AccentUi : NkRole::Border,
												  on ? NkRole::AccentUi : NkRole::InputBg, 3.f);
										char lb[8];
										snprintf(lb, sizeof(lb), "%d", t);
										p.TextV(br.x + (br.w - p.TextW(lb)) * 0.5f, yy, kRowH, lb,
												on ? NkRole::TextOnAccent : NkRole::TextMuted);
										if (hit.Clicked(km))
											mc = t;
										bxm += bwm + gm;
									}
									yy += kRowH;
									if (mc != mc0) {
										demo::Demo3DHostSetSkyMoonCount(mc);
										NkMarkDirty(st);
									}
									for (int32 m = 0; m < mc; ++m) {
										float32 me = 0.f, ma = 0.f, ms = 0.f, mb = 0.f, mcol[3];
										demo::Demo3DHostSkyMoon(m, &me, &ma, &ms, &mb, mcol);
										const float32 e0m = me, a0m = ma, s0m = ms, b0m = mb;
										bool colCh2 = false;
										char kk[28];
										char lbl[24];
										snprintf(lbl, sizeof(lbl), "Lune %d", m + 1);
										p.TextV(iA.x, yy, kRowH, lbl, NkRole::Text);
										yy += kRowH;
										snprintf(kk, sizeof(kk), "prop.sky.mel%d", m);
										p.TextV(iA.x, yy, kRowH, "Elevation", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, kk,
												  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
												   kRowH - S(6.f)},
												  me, 0.5f, NkRole::AccentUi, "%.1f°");
										yy += kRowH;
										snprintf(kk, sizeof(kk), "prop.sky.maz%d", m);
										p.TextV(iA.x, yy, kRowH, "Azimut", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, kk,
												  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
												   kRowH - S(6.f)},
												  ma, 1.f, NkRole::AccentUi, "%.1f°");
										yy += kRowH;
										snprintf(kk, sizeof(kk), "prop.sky.msz%d", m);
										p.TextV(iA.x, yy, kRowH, "Taille", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, kk,
												  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
												   kRowH - S(6.f)},
												  ms, 0.001f, NkRole::AccentUi, "%.3f");
										yy += kRowH;
										snprintf(kk, sizeof(kk), "prop.sky.mbr%d", m);
										p.TextV(iA.x, yy, kRowH, "Luminosite", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, kk,
												  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
												   kRowH - S(6.f)},
												  mb, 0.02f, NkRole::AccentUi, "%.2f");
										yy += kRowH;
										snprintf(kk, sizeof(kk), "prop.sky.mcl%d", m);
										yy += PaintColorRow(p, hit, ws, in, st, iA, yy, "Couleur",
															kk, mcol, &colCh2);
										if (me != e0m || ma != a0m || ms != s0m || mb != b0m || colCh2) {
											demo::Demo3DHostSetSkyMoon(m, me, ma, ms, mb, mcol);
											NkMarkDirty(st);
										}
										// PHASE : deduite du soleil par defaut, donc
										// toujours coherente avec l'eclairage. La
										// forcer est un choix de MISE EN SCENE --
										// legitime pour un plan de film, et declare
										// explicitement plutot que subi.
										{
											bool mph = false;
											float32 mpv = 0.25f;
											demo::Demo3DHostSkyMoonPhase(m, &mph, &mpv);
											const bool h0 = mph;
											const float32 v0m = mpv;
											char kp[28];
											snprintf(kp, sizeof(kp), "prop.sky.mpm%d", m);
											const NkRect cb2{iA.x, yy + S(5.f), S(12.f), S(12.f)};
											hit.Add(kp, cb2);
											p.Outline(cb2, mph ? NkRole::AccentUi : NkRole::Border,
													  mph ? NkRole::AccentUi : NkRole::InputBg, 2.f);
											p.TextV(cb2.x + S(18.f), yy, kRowH, "Phase forcee",
													NkRole::TextMuted);
											if (hit.Clicked(kp))
												mph = !mph;
											yy += kRowH;
											if (mph) {
												snprintf(kp, sizeof(kp), "prop.sky.mpv%d", m);
												p.TextV(iA.x, yy, kRowH, "Phase",
														NkRole::TextMuted);
												DragFloat(p, hit, ws, in, kp,
														  {iA.x + S(110.f), yy + S(3.f),
														   iA.w - S(110.f), kRowH - S(6.f)},
														  mpv, 0.01f, NkRole::AccentUi, "%.2f");
												yy += kRowH;
											}
											if (mph != h0 || mpv != v0m) {
												demo::Demo3DHostSetSkyMoonPhase(m, mph, mpv);
												NkMarkDirty(st);
											}
										}
									}
								}
								// ── NUAGES ──────────────────────────────────
								// Couche INDEPENDANTE du modele : elle se pose
								// aussi bien sur un degrade que sur un ciel
								// physique. La « couverture » est un SEUIL, pas
								// une opacite : a 0 il n'y a rien, et les nuages
								// naissent puis grossissent quand on monte -- au
								// lieu d'un voile uniforme qui se contenterait de
								// foncer.
								{
									bool cOn = false;
									float32 cCov = 0.5f, cDen = 1.f, cScl = 2.f, cCol[3];
									demo::Demo3DHostSkyClouds(&cOn, &cCov, &cDen, &cScl, cCol);
									const bool o0 = cOn;
									const float32 v0 = cCov, w0 = cDen, s0c = cScl;
									bool colCh = false;
									const NkRect cb{iA.x, yy + S(5.f), S(12.f), S(12.f)};
									hit.Add("prop.sky.clouds", cb);
									p.Outline(cb, cOn ? NkRole::AccentUi : NkRole::Border,
											  cOn ? NkRole::AccentUi : NkRole::InputBg, 2.f);
									p.TextV(cb.x + S(18.f), yy, kRowH, "Nuages", NkRole::TextMuted);
									if (hit.Clicked("prop.sky.clouds"))
										cOn = !cOn;
									yy += kRowH;
									if (cOn) {
										p.TextV(iA.x, yy, kRowH, "Couverture", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.sky.cov",
												  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
												   kRowH - S(6.f)},
												  cCov, 0.01f, NkRole::AccentUi, "%.2f");
										yy += kRowH;
										p.TextV(iA.x, yy, kRowH, "Densite", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.sky.cden",
												  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
												   kRowH - S(6.f)},
												  cDen, 0.01f, NkRole::AccentUi, "%.2f");
										yy += kRowH;
										p.TextV(iA.x, yy, kRowH, "Echelle", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.sky.cscl",
												  {iA.x + S(110.f), yy + S(3.f), iA.w - S(110.f),
												   kRowH - S(6.f)},
												  cScl, 0.05f, NkRole::AccentUi, "%.2f");
										yy += kRowH;
										// LA VITESSE : les nuages DEFILENT. Elle
										// n'agit que sur le ciel evalue en temps
										// reel, donc son effet est immediat -- elle
										// ne demande aucune regeneration.
										{
											float32 cs = demo::Demo3DHostSkyCloudSpeed();
											const float32 cs0 = cs;
											p.TextV(iA.x, yy, kRowH, "Vitesse", NkRole::TextMuted);
											DragFloat(p, hit, ws, in, "prop.sky.cspd",
													  {iA.x + S(110.f), yy + S(3.f),
													   iA.w - S(110.f), kRowH - S(6.f)},
													  cs, 0.002f, NkRole::AccentUi, "%.3f");
											if (cs != cs0) {
												demo::Demo3DHostSetSkyCloudSpeed(cs);
												NkMarkDirty(st);
											}
											yy += kRowH;
										}
										yy += PaintColorRow(p, hit, ws, in, st, iA, yy, "Couleur",
															"prop.sky.ccol", cCol, &colCh);
									}
									if (cOn != o0 || cCov != v0 || cDen != w0 || cScl != s0c || colCh) {
										demo::Demo3DHostSetSkyClouds(cOn, cCov, cDen, cScl, cCol);
										NkMarkDirty(st);
									}
									if (cOn) {
										// AMBIANCES : des reglages tout faits. Le bouton de
										// l'ambiance EN PLACE est plein (accent) -- des
										// qu'un curseur est retouche, plus aucun ne
										// s'allume, et c'est le bon message : on n'est
										// plus sur un preset.
										static const char *const kAmb[3] = {"Defaut", "Pluie",
																			"Desert"};
										static const char *const kAmbKey[3] = {
											"prop.sky.creset", "prop.sky.cpluie",
											"prop.sky.cdesert"};
										const int32 ambCur = demo::Demo3DHostCloudPreset();
										const float32 gpA = S(4.f);
										const float32 bwA = (iA.w - gpA * 2.f) / 3.f;
										for (int32 a = 0; a < 3; ++a) {
											const NkRect ra{iA.x + (bwA + gpA) * a, yy + S(2.f),
															bwA, kRowH - S(4.f)};
											hit.Add(kAmbKey[a], ra);
											const bool actA = ambCur == a;
											if (actA)
												p.Fill(ra, NkRole::AccentUi, 3.f);
											else
												p.Outline(ra, NkRole::Border, NkRole::InputBg,
														  3.f);
											const char *la = kAmb[a];
											float32 twA = p.TextW(la);
											if (twA <= ra.w - S(2.f))
												p.TextV(ra.x + (ra.w - twA) * 0.5f, yy, kRowH,
														la,
														actA ? NkRole::TextOnAccent
															 : NkRole::TextMuted);
											if (hit.Clicked(kAmbKey[a]))
												demo::Demo3DHostApplyCloudPreset(a);
										}
										yy += kRowH;
									}
								}
								// REGENERER est un calcul CPU (convolutions) : il se
								// demande, il ne se declenche pas a chaque image ni
								// sous le curseur qu'on tire.
								//
								// LE BOUTON DIT S'IL Y A QUELQUE CHOSE A REGENERER.
								// Sans ce retour, on tire un curseur, l'image ne
								// bouge pas, et rien ne dit que c'est normal : le
								// reglage passe pour « sans effet » -- exactement le
								// genre de doute qu'on vient de payer cher ailleurs.
								{
									const bool dirty = demo::Demo3DHostSkyNeedsApply();
									const float32 gp = S(4.f);
									const float32 bw = (iA.w - gp) * 0.62f;
									const NkRect br{iA.x, yy + S(2.f), bw, kRowH - S(4.f)};
									hit.Add("prop.sky.apply", br);
									p.Fill(br, dirty ? NkRole::AccentUi : NkRole::PanelHeader, 3.f);
									const char *lbl = dirty ? "Regenerer le ciel *"
															: "Regenerer le ciel";
									p.TextV(br.x + (br.w - p.TextW(lbl)) * 0.5f, yy, kRowH, lbl,
											dirty ? NkRole::TextOnAccent : NkRole::TextMuted);
									if (hit.Clicked("prop.sky.apply"))
										demo::Demo3DHostApplySky();
									// REINITIALISER LE CIEL, sans toucher a
									// l'ambiance ni aux nuages : trois portees
									// separees, pour ne pas perdre l'un en voulant
									// retrouver l'autre.
									const NkRect rr2{iA.x + bw + gp, yy + S(2.f),
													 iA.w - bw - gp, kRowH - S(4.f)};
									hit.Add("prop.sky.reset", rr2);
									p.Outline(rr2, NkRole::Border, NkRole::InputBg, 3.f);
									p.TextV(rr2.x + (rr2.w - p.TextW("Defaut")) * 0.5f, yy, kRowH,
											"Defaut", NkRole::TextMuted);
									if (hit.Clicked("prop.sky.reset")) {
										demo::Demo3DHostResetSky();
										// L'etat d'interface doit suivre la remise a
										// zero : sinon le combo continuerait
										// d'afficher « Physique » alors que le
										// moteur est revenu au degrade.
										st.skyModel = 0;
										st.skySunSel = 0;
									}
									yy += kRowH;
								}
							} else if (st.envSource == 2) {
								p.TextV(iA.x, yy, kRowH, "Fichier", NkRole::TextMuted);
								if (!ws.IsEditing("prop.hdr.path") && !st.hdrPath[0])
									snprintf(st.hdrPath, sizeof(st.hdrPath), "%s",
											 demo::Demo3DHostHdrPath());
								EditableText(p, hit, ws, in, "prop.hdr.path",
											 {iA.x + S(70.f), yy + S(2.f), iA.w - S(70.f),
											  kRowH - S(4.f)},
											 st.hdrPath[0] ? st.hdrPath : "Resources/HDRI/....hdr",
											 st.hdrPath[0] ? NkRole::Text : NkRole::TextMuted,
											 st.hdrPath, sizeof(st.hdrPath));
								yy += kRowH;
								const NkRect br{iA.x, yy + S(2.f), iA.w, kRowH - S(4.f)};
								hit.Add("prop.hdr.load", br);
								p.Fill(br, NkRole::AccentUi, 3.f);
								p.TextV(br.x + (br.w - p.TextW("Charger l'image")) * 0.5f, yy,
										kRowH, "Charger l'image", NkRole::TextOnAccent);
								if (hit.Clicked("prop.hdr.load"))
									st.hdrOk = demo::Demo3DHostLoadHdr(st.hdrPath) ? 1 : -1;
								yy += kRowH;
								if (st.hdrOk != 0) {
									p.TextV(iA.x, yy, kRowH,
											st.hdrOk > 0 ? "Image chargee"
														 : "Echec : fichier introuvable ou format"
														   " non equirectangulaire",
											st.hdrOk > 0 ? NkRole::TextMuted : NkRole::AxisX);
									yy += kRowH;
								}
							}
							yy += NkGroupPad();
							PaintGroupBlock(p, aR, aTop, yy);
						}
						yy += NkPropGroupGap();
					}
					// ── BROUILLARD ──────────────────────────────────────────────
					{
						// ── SOL INFINI (option, Rihen) : un vrai plan recepteur
						// d'ombres, distinct de la grille -- couleur, hauteur,
						// rugosite. Le plan suit la camera cote hote.
						{
							NkRect gR = rr;
							gR.x = r.x + NkPropInset();
							gR.w = rr.w - 2.f * NkPropInset();
							const float32 gTop = yy;
							if (PaintPropGroup(p, hit, st, gR, yy, "prop.g.floor", "Sol", 4u)) {
								const NkRect iG = NkGroupInner(gR);
								const float32 gvX = iG.x + S(110.f);
								const float32 gvW = iG.w - S(110.f);
								yy += NkGroupPad();
								bool gOn = false;
								float32 gCol[3], gY = 0.f, gRg = 0.9f, gTl = 1.f, gMt = 0.f;
								int32 gPat = 0;
								demo::Demo3DHostFloor(&gOn, gCol, &gY, &gRg, &gPat, &gTl, &gMt);
								const bool g0 = gOn;
								const float32 gy0 = gY, gr0 = gRg, gt0 = gTl, gm0 = gMt;
								const int32 gp0 = gPat;
								bool gcCh = false;
								{
									const NkRect cb{iG.x, yy + S(5.f), S(12.f), S(12.f)};
									hit.Add("prop.floor.on", cb);
									p.Outline(cb, gOn ? NkRole::AccentUi : NkRole::Border,
											  gOn ? NkRole::AccentUi : NkRole::InputBg, 2.f);
									p.TextV(cb.x + S(18.f), yy, kRowH, "Actif",
											NkRole::TextMuted);
									if (hit.Clicked("prop.floor.on"))
										gOn = !gOn;
									yy += kRowH;
								}
								yy += PaintColorRow(p, hit, ws, in, st, iG, yy, "Couleur",
													"prop.floorcol", gCol, &gcCh);
								p.TextV(iG.x, yy, kRowH, "Hauteur", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.floor.y",
										  {gvX, yy + S(3.f), gvW, kRowH - S(6.f)}, gY, 0.01f,
										  NkRole::AccentUi, "%.2f m");
								yy += kRowH;
								p.TextV(iG.x, yy, kRowH, "Rugosite", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.floor.rg",
										  {gvX, yy + S(3.f), gvW, kRowH - S(6.f)}, gRg, 0.005f,
										  NkRole::AccentUi, "%.2f");
								yy += kRowH;
								// METALLIQUE : 0 = dielectrique (la couleur diffuse,
								// reflets blancs), 1 = metal (plus de diffusion, la
								// couleur TEINTE les reflets).
								p.TextV(iG.x, yy, kRowH, "Metallique", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.floor.mt",
										  {gvX, yy + S(3.f), gvW, kRowH - S(6.f)}, gMt, 0.005f,
										  NkRole::AccentUi, "%.2f");
								yy += kRowH;
								// ── MOTIF (Rihen, references Unreal) : uni, damier,
								// ou carreaux a joints -- et la taille du carreau.
								{
									static const char *const kPat[3] = {"Uni", "Damier",
																		"Carreaux"};
									p.TextV(iG.x, yy, kRowH, "Motif", NkRole::TextMuted);
									const float32 pw3 = gvW / 3.f - S(3.f);
									for (int32 m3 = 0; m3 < 3; ++m3) {
										const NkRect mb{gvX + (pw3 + S(4.f)) * (float32)m3,
														yy + S(2.f), pw3, kRowH - S(4.f)};
										char mk[24];
										snprintf(mk, sizeof(mk), "prop.floor.p%d", m3);
										hit.Add(mk, mb);
										const bool onM = gPat == m3;
										p.Fill(mb, onM ? NkRole::AccentUi : NkRole::PanelHeader,
											   3.f);
										// LIBELLE CLAMPE (Rihen : retreci, le texte
										// debordait du bouton et chevauchait le
										// voisin) : trop etroit -> l'initiale ;
										// encore trop -> rien, le bouton reste lisible
										// par sa couleur.
										static const char *const kPatS[3] = {"U", "D", "C"};
										const char *lbl3 = kPat[m3];
										float32 tw3 = p.TextW(lbl3);
										if (tw3 > mb.w - S(6.f)) {
											lbl3 = kPatS[m3];
											tw3 = p.TextW(lbl3);
										}
										if (tw3 <= mb.w - S(2.f))
											p.TextV(mb.x + (mb.w - tw3) * 0.5f, yy, kRowH, lbl3,
													onM ? NkRole::TextOnAccent
														: NkRole::TextMuted);
										if (hit.Clicked(mk))
											gPat = m3;
									}
									yy += kRowH;
								}
								if (gPat > 0) {
									p.TextV(iG.x, yy, kRowH, "Carreau", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "prop.floor.tl",
											  {gvX, yy + S(3.f), gvW, kRowH - S(6.f)}, gTl,
											  0.01f, NkRole::AccentUi, "%.2f m");
									yy += kRowH;
								}
								if (gOn != g0 || gY != gy0 || gRg != gr0 || gcCh ||
									gPat != gp0 || gTl != gt0 || gMt != gm0) {
									demo::Demo3DHostSetFloor(gOn, gCol, gY, gRg, gPat, gTl, gMt);
									NkMarkDirty(st);
								}
								yy += NkGroupPad();
								PaintGroupBlock(p, gR, gTop, yy);
							}
							yy += NkPropGroupGap();
						}
						NkRect fR = rr;
						fR.x = r.x + NkPropInset();
						fR.w = rr.w - 2.f * NkPropInset();
						const float32 fTop = yy;
						if (PaintPropGroup(p, hit, st, fR, yy, "prop.g.fog", "Brouillard", 4u)) {
							const NkRect iF = NkGroupInner(fR);
							const float32 fvX = iF.x + S(110.f);
							const float32 fvW = iF.w - S(110.f);
							yy += NkGroupPad();
							bool fOn = false;
							float32 fCol[3], fDen = 0.f, fSta = 0.f, fEnd = 0.f;
							int32 fMode = 0;
							demo::Demo3DHostFog(&fOn, fCol, &fDen, &fSta, &fEnd, &fMode);
							const bool o0 = fOn;
							const float32 d0 = fDen, s0 = fSta, e0 = fEnd;
							const int32 m0 = fMode;
							{
								const NkRect cb{iF.x, yy + S(5.f), S(12.f), S(12.f)};
								hit.Add("prop.fog.on", cb);
								p.Outline(cb, fOn ? NkRole::AccentUi : NkRole::Border,
										  fOn ? NkRole::AccentUi : NkRole::InputBg, 2.f);
								p.TextV(cb.x + S(18.f), yy, kRowH, "Actif", NkRole::TextMuted);
								if (hit.Clicked("prop.fog.on"))
									fOn = !fOn;
								yy += kRowH;
							}
							static const char *const kFm[2] = {"Lineaire (debut / fin)",
															   "Exponentiel (densite)"};
							p.TextV(iF.x, yy, kRowH, "Loi", NkRole::TextMuted);
							Combo(p, hit, ws, "prop.fog.mode",
								  {fvX, yy + S(2.f), fvW, kRowH - S(4.f)}, kFm, nullptr, 2,
								  st.fogMode, combo);
							fMode = st.fogMode;
							yy += kRowH;
							bool fcCh = false;
							yy += PaintColorRow(p, hit, ws, in, st, iF, yy, "Couleur",
												"prop.fogcol", fCol, &fcCh);
							if (fMode == 1) {
								p.TextV(iF.x, yy, kRowH, "Densite", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.fog.den",
										  {fvX, yy + S(3.f), fvW, kRowH - S(6.f)}, fDen, 0.002f,
										  NkRole::AccentUi, "%.3f");
								yy += kRowH;
							} else {
								p.TextV(iF.x, yy, kRowH, "Debut", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.fog.sta",
										  {fvX, yy + S(3.f), fvW, kRowH - S(6.f)}, fSta, 0.25f,
										  NkRole::AccentUi, "%.2f m");
								yy += kRowH;
								p.TextV(iF.x, yy, kRowH, "Fin", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.fog.end",
										  {fvX, yy + S(3.f), fvW, kRowH - S(6.f)}, fEnd, 0.5f,
										  NkRole::AccentUi, "%.2f m");
								yy += kRowH;
							}
							// ── NAPPE AU SOL, SOUFFLEE COMME UNE FUMEE (Rihen) ──
							// Epaisseur a zero : le brouillard ne depend que de la
							// distance, comme avant -- les autres champs n'ont
							// alors aucun effet et ne s'affichent pas.
							{
								float32 fgB = 0.f, fgT = 0.f, fgW = 0.f;
								bool fgC = true;
								demo::Demo3DHostFogGround(&fgB, &fgT, &fgW, &fgC);
								const float32 b0g = fgB, t0g = fgT, w0g = fgW;
								const bool c0g = fgC;
								p.TextV(iF.x, yy, kRowH, "Epaisseur sol", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.fog.thk",
										  {fvX, yy + S(3.f), fvW, kRowH - S(6.f)}, fgT, 0.1f,
										  NkRole::AccentUi, "%.2f m");
								yy += kRowH;
								if (fgT > 0.001f) {
									p.TextV(iF.x, yy, kRowH, "Altitude", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "prop.fog.base",
											  {fvX, yy + S(3.f), fvW, kRowH - S(6.f)}, fgB,
											  0.1f, NkRole::AccentUi, "%.2f m");
									yy += kRowH;
									p.TextV(iF.x, yy, kRowH, "Souffle", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "prop.fog.wind",
											  {fvX, yy + S(3.f), fvW, kRowH - S(6.f)}, fgW,
											  0.01f, NkRole::AccentUi, "%.2f");
									yy += kRowH;
									// LIER AU VENT DES NUAGES : la nappe derive au
									// meme pas que la couche nuageuse -- c'est ce
									// qui fait respirer le sol et le ciel ensemble.
									const NkRect cbW{iF.x, yy + S(5.f), S(12.f), S(12.f)};
									hit.Add("prop.fog.wcl", cbW);
									p.Outline(cbW, fgC ? NkRole::AccentUi : NkRole::Border,
											  fgC ? NkRole::AccentUi : NkRole::InputBg, 2.f);
									p.TextV(cbW.x + S(18.f), yy, kRowH,
											"Suivre le vent des nuages", NkRole::TextMuted);
									if (hit.Clicked("prop.fog.wcl"))
										fgC = !fgC;
									yy += kRowH;
								}
								if (fgB != b0g || fgT != t0g || fgW != w0g || fgC != c0g) {
									demo::Demo3DHostSetFogGround(fgB, fgT, fgW, fgC);
									NkMarkDirty(st);
								}
							}
							if (fOn != o0 || fDen != d0 || fSta != s0 || fEnd != e0 ||
								fMode != m0 || fcCh) {
								demo::Demo3DHostSetFog(fOn, fCol, fDen, fSta, fEnd, fMode);
								NkMarkDirty(st);
							}
							yy += NkGroupPad();
							PaintGroupBlock(p, fR, fTop, yy);
						}
						yy += NkPropGroupGap();
					}
					// ── OCCLUSION AMBIANTE (SSAO) ───────────────────────────────
					// Rihen (9 aout) : le depot sombre au pied des objets « ne
					// donne rien de bon pour une application » -> ETEINTE par
					// defaut, mais REGLABLE ici (rayon monde en metres,
					// intensite). Aucun etat local : la config du renderer fait
					// foi (Demo3DHostSSAO la lit, Set la pousse via SetPostConfig
					// qui reconstruit le graphe a l'aplomb de la frame suivante).
					{
						const bool grpAO = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.ssao",
														  "Occlusion ambiante", 1u);
						const float32 grpAOTop = yy;
						if (grpAO) {
							const NkRect iA = NkGroupInner(rowR);
							const float32 avX = iA.x + S(110.f);
							const float32 avW = iA.w - S(110.f);
							yy += NkGroupPad();
							bool aOn = false;
							float32 aRad = 0.5f, aInt = 1.f;
							demo::Demo3DHostSSAO(&aOn, &aRad, &aInt);
							const bool a0 = aOn;
							const float32 ar0 = aRad, ai0 = aInt;
							{
								const NkRect cb{iA.x, yy + S(5.f), S(12.f), S(12.f)};
								hit.Add("prop.ssao.on", cb);
								p.Outline(cb, aOn ? NkRole::AccentUi : NkRole::Border,
										  aOn ? NkRole::AccentUi : NkRole::InputBg, 2.f);
								p.TextV(cb.x + S(18.f), yy, kRowH, "Actif", NkRole::TextMuted);
								if (hit.Clicked("prop.ssao.on"))
									aOn = !aOn;
								yy += kRowH;
							}
							if (aOn) {
								p.TextV(iA.x, yy, kRowH, "Rayon", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.ssao.rad",
										  {avX, yy + S(3.f), avW, kRowH - S(6.f)}, aRad, 0.02f,
										  NkRole::AccentUi, "%.2f m");
								yy += kRowH;
								p.TextV(iA.x, yy, kRowH, "Intensite", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.ssao.int",
										  {avX, yy + S(3.f), avW, kRowH - S(6.f)}, aInt, 0.02f,
										  NkRole::AccentUi, "%.2f");
								yy += kRowH;
							}
							if (aOn != a0 || aRad != ar0 || aInt != ai0) {
								demo::Demo3DHostSetSSAO(aOn, aRad, aInt);
								NkMarkDirty(st);
							}
							yy += NkGroupPad();
							PaintGroupBlock(p, rowR, grpAOTop, yy);
						}
						yy += NkPropGroupGap();
					}
					// ── EXPOSITION & BLOOM (2026-08-09) ─────────────────────────
					// Reglages presents dans le moteur depuis le debut, jamais
					// proposes : un spot surpuissant faisait un halo geant sans
					// qu'on puisse ni baisser l'exposition ni relever le seuil.
					{
						const bool grpFx = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.postfx",
														  "Exposition et bloom", 1u);
						const float32 grpFxTop = yy;
						if (grpFx) {
							const NkRect iF2 = NkGroupInner(rowR);
							const float32 fvX2 = iF2.x + S(110.f);
							const float32 fvW2 = iF2.w - S(110.f);
							yy += NkGroupPad();
							float32 fxE = 1.f, fxT = 0.85f, fxS = 1.5f;
							bool fxB = true;
							demo::Demo3DHostPostFx(&fxE, &fxB, &fxT, &fxS);
							const float32 e0 = fxE, t0 = fxT, s0 = fxS;
							const bool b0 = fxB;
							p.TextV(iF2.x, yy, kRowH, "Exposition", NkRole::TextMuted);
							DragFloat(p, hit, ws, in, "prop.fx.exp",
									  {fvX2, yy + S(3.f), fvW2, kRowH - S(6.f)}, fxE, 0.01f,
									  NkRole::AccentUi, "%.2f");
							yy += kRowH;
							{
								const NkRect cb{iF2.x, yy + S(5.f), S(12.f), S(12.f)};
								hit.Add("prop.fx.bloom", cb);
								p.Outline(cb, fxB ? NkRole::AccentUi : NkRole::Border,
										  fxB ? NkRole::AccentUi : NkRole::InputBg, 2.f);
								p.TextV(cb.x + S(18.f), yy, kRowH, "Bloom", NkRole::TextMuted);
								if (hit.Clicked("prop.fx.bloom"))
									fxB = !fxB;
								yy += kRowH;
							}
							if (fxB) {
								p.TextV(iF2.x, yy, kRowH, "Seuil", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.fx.thr",
										  {fvX2, yy + S(3.f), fvW2, kRowH - S(6.f)}, fxT,
										  0.01f, NkRole::AccentUi, "%.2f");
								yy += kRowH;
								p.TextV(iF2.x, yy, kRowH, "Intensite", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.fx.str",
										  {fvX2, yy + S(3.f), fvW2, kRowH - S(6.f)}, fxS,
										  0.01f, NkRole::AccentUi, "%.2f");
								yy += kRowH;
							}
							if (fxE != e0 || fxB != b0 || fxT != t0 || fxS != s0) {
								demo::Demo3DHostSetPostFx(fxE, fxB, fxT, fxS);
								NkMarkDirty(st);
							}
							yy += NkGroupPad();
							PaintGroupBlock(p, rowR, grpFxTop, yy);
						}
						yy += NkPropGroupGap();
					}
					const bool grpSh = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.shadow",
													  "Ombres", 1u);
					const float32 grpShTop = yy;
					if (grpSh) {
						const NkRect iR = NkGroupInner(rowR);
						const float32 svX = iR.x + S(110.f);
						const float32 svW = iR.w - S(110.f);
						yy += NkGroupPad();
						float32 nb = 0.f, sb = 0.f, sf = 0.f;
						int32 q = 1;
						if (demo::Demo3DHostShadowCfg(&nb, &sb, &sf, &q)) {
							const float32 nb0 = nb, sb0 = sb, sf0 = sf;
							// La qualite vit dans l'etat, pas ici : la liste
							// deroulee ecrit a la frame SUIVANTE, par un pointeur
							// qui designerait alors une variable locale morte --
							// le choix se perdait en silence (Rihen : « le
							// combobox ne fonctionne pas »).
							if (st.shadowQual < 0)
								st.shadowQual = q;
							// VERITE-MOTEUR : q vient d'etre lu du moteur, l'etat
							// vient du combo (ecrit en fin de frame). Des qu'ils
							// divergent, on pousse -- « capturer avant / comparer
							// apres » dans la meme frame ne detectait jamais rien.
							const int32 qEng = q;
							// CINQ crans, comme l'enum moteur : avec 4, « Penombre
								// (PCSS) » etait l'index 3, c'est-a-dire POISSON --
								// le vrai PCSS etait inatteignable.
								// PCSS reel depuis le 9 aout (echantillonnage brut de
								// l'atlas au binding 12) : ombre NETTE au contact,
								// penombre qui grandit avec la distance au bloqueur.
								// En mode Penombre, « Douceur » = taille de la source.
								static const char *const kQ[5] = {"Aucune", "Douce (PCF 3)",
															  "Douce (PCF 5)",
															  "Poisson (grain doux)",
															  "Penombre (PCSS)"};
							p.TextV(iR.x, yy, kRowH, "Qualite", NkRole::TextMuted);
							Combo(p, hit, ws, "prop.sh.q",
								  {svX, yy + S(2.f), svW, kRowH - S(4.f)}, kQ, nullptr, 5,
								  st.shadowQual, combo);
							q = st.shadowQual;
							yy += kRowH;
							// ── STATIQUE OU DYNAMIQUE (Rihen) ───────────────────
							// Statique : l'ombre est calculee une fois puis gardee
							// telle quelle -- c'est gratuit, mais elle ne suit plus
							// rien. Dynamique : elle se refait des que la lumiere ou
							// la scene bouge. Un modeleur veut le second ; le
							// premier sert aux decors qui ne bougent plus.
							{
								static const char *const kDyn[2] = {"Statique (calcul unique)",
																	"Dynamique (suit la scene)"};
								p.TextV(iR.x, yy, kRowH, "Mise a jour", NkRole::TextMuted);
								// VERITE-MOTEUR, comme la qualite ci-dessus : le
								// combo ecrit l'etat en fin de frame, on pousse des
								// que l'etat diverge de ce que dit le moteur.
								const int32 dEng = demo::Demo3DHostShadowDynamic() ? 1 : 0;
								Combo(p, hit, ws, "prop.sh.dyn",
									  {svX, yy + S(2.f), svW, kRowH - S(4.f)}, kDyn, nullptr, 2,
									  st.shadowDynamic, combo);
								if (st.shadowDynamic != dEng) {
									demo::Demo3DHostSetShadowDynamic(st.shadowDynamic != 0);
									NkMarkDirty(st);
								}
								yy += kRowH;
								// EN STATIQUE, l'ombre est figee par choix -- mais on
								// doit pouvoir la refaire QUAND ON LE DECIDE (Rihen :
								// « pour static il faut un bouton pour recalculer »).
								// Une passe complete, puis le cache refige.
								// BLEU des qu'un recalcul serait UTILE (lumiere ou
								// geometrie modifiee depuis le gel), normal sinon --
								// le meme langage que « Regenerer le ciel * ».
								if (st.shadowDynamic == 0) {
									const bool stale = demo::Demo3DHostShadowRecalcPending();
									const NkRect rb{svX, yy + S(2.f), svW, kRowH - S(4.f)};
									hit.Add("prop.sh.recalc", rb);
									p.Fill(rb, stale ? NkRole::AccentUi : NkRole::PanelHeader, 3.f);
									const char *rlbl = stale ? "Recalculer l'ombre *"
															 : "Recalculer l'ombre";
									p.TextV(rb.x + (rb.w - p.TextW(rlbl)) * 0.5f, yy, kRowH, rlbl,
											stale ? NkRole::TextOnAccent : NkRole::TextMuted);
									if (hit.Clicked("prop.sh.recalc"))
										demo::Demo3DHostShadowRecalc();
									yy += kRowH;
								}
							}
							p.TextV(iR.x, yy, kRowH, "Douceur", NkRole::TextMuted);
							DragFloat(p, hit, ws, in, "prop.sh.soft",
									  {svX, yy + S(3.f), svW, kRowH - S(6.f)}, sf, 0.0005f,
									  NkRole::AccentUi, "%.4f");
							yy += kRowH;
							// LE BIAIS NORMAL est le reglage qui empeche un objet de
							// projeter son ombre SUR LUI-MEME : c'est lui qu'on
							// augmente quand on voit ces bandes sombres a sa surface.
							// En TEXELS de la shadow map (1.5 par defaut) : le pas
							// suit cette echelle -- l'ancien 0.005 datait des metres
							// et demandait cent crans pour produire un effet.
							p.TextV(iR.x, yy, kRowH, "Biais normal", NkRole::TextMuted);
							DragFloat(p, hit, ws, in, "prop.sh.nb",
									  {svX, yy + S(3.f), svW, kRowH - S(6.f)}, nb, 0.05f,
									  NkRole::AccentUi, "%.2f");
							yy += kRowH;
							p.TextV(iR.x, yy, kRowH, "Biais de pente", NkRole::TextMuted);
							DragFloat(p, hit, ws, in, "prop.sh.sb",
									  {svX, yy + S(3.f), svW, kRowH - S(6.f)}, sb, 0.0002f,
									  NkRole::AccentUi, "%.4f");
							yy += kRowH;
							if (nb != nb0 || sb != sb0 || sf != sf0 || q != qEng) {
								demo::Demo3DHostSetShadowCfg(nb, sb, sf, q);
								NkMarkDirty(st);
							}
						} else {
							p.TextV(iR.x, yy, kRowH, "Ombres indisponibles",
									NkRole::TextMuted);
							yy += kRowH;
						}
						yy += NkGroupPad();
						PaintGroupBlock(p, rowR, grpShTop, yy);
					}
					yy += NkPropGroupGap();
		}







		// ── PASTILLE « MATERIAU » ───────────────────────────────────────────────
		// La liste des materiaux DE L'OBJET (jamais celle du projet), sa colonne de
		// boutons, la modale d'ajout, et les reglages de surface du materiau actif.
		// C'est cette pastille qui porte la modale a refaire sur NkEditorModal.
		inline void PaintPropMaterial(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
									  NkWidgetState &ws, const nkgui::NkGuiInput &in,
									  NkComboPending &combo, nkgui::NkGuiContext *guiCtx,
									  const NkRect &r, const NkRect &rr, float32 &yy) {
			auto Button = [&](const char *k2, float32 yB, const char *label, float32 x,
							  float32 w) -> bool {
				return NkPropButton(p, hit, k2, yB, label, x, w);
			};
			(void)Button;
			char key[64];
			char buf[160];
			(void)buf;
					// ── MATERIAU, FACTURE BLENDER (capture de Rihen) ────────────
					// Une LISTE d'emplacements avec sa colonne + / - / menu et sa
					// poignee de hauteur ; dessous, la barre du NAVIGATEUR (choisir
					// un materiau existant, le renommer, le delier) ; puis les
					// PROPRIETES du materiau selectionne. La pile de groupes
					// repliables ne tenait pas quand les materiaux se comptaient en
					// dizaines -- une liste tient dans un coin d'ecran.
					// La sauvegarde .nkasset (NkMaterialLibrary) et l'editeur nodal
					// s'appuieront sur ce meme registre.
					const int32 actN = st.activeEmpty >= 0 ? st.activeEmpty
														   : demo::Demo3DHostActiveObject();
					const int32 curOf = actN >= 0 ? demo::Demo3DHostProjMatOf(actN) : -1;
					// LE REGISTRE N'EST JAMAIS VIDE ICI : le materiau de base est
					// cree des l'affichage (regle de Rihen -- un maillage ne vit pas
					// sans materiau, la liste non plus).
					demo::Demo3DHostProjMatEnsureDefault();
					// Table des materiaux vivants : la liste travaille sur des
					// RANGS, le registre sur des indices -- les deux ne coincident
					// pas des qu'un materiau est supprime au milieu.
					static int32 sMatIdx[64];
					static char sMatNm[64][32];
					int32 nMats = 0;
					// ── CETTE LISTE EST CELLE DE L'OBJET ────────────────────
					// Modele fixe avec Rihen le 12 aout : la pastille montre les
					// materiaux ASSOCIES A L'OBJET ACTIF, pas le projet entier
					// (celui-ci se consulte dans le navigateur de projet). C'est
					// ce qui donne son sens au « retirer » : il sort le materiau
					// de CET objet, sans rien detruire, et le « + » l'y remet.
					// Sans objet actif, on retombe sur le projet — il faut bien
					// pouvoir regarder et editer un materiau sans rien
					// selectionner.
					// JAMAIS le projet entier (Rihen, 12 aout) : cette liste est
					// celle de l'OBJET SELECTIONNE, un point c'est tout. Le projet
					// se consulte dans le navigateur, et s'invite ici uniquement
					// par le « + », le temps de choisir. Sans objet selectionne,
					// la liste est donc VIDE — la pastille elle-meme disparait
					// dans ce cas, et le panneau cede la place a une autre section.
					const int32 nOb = (actN >= 0) ? demo::Demo3DHostNodeMatCount(actN) : 0;
					for (int32 k = 0; k < nOb && nMats < 64; ++k) {
						const int32 mi = demo::Demo3DHostNodeMatAt(actN, k);
						if (mi < 0)
							continue;
						if (!demo::Demo3DHostProjMatInfo(mi, sMatNm[nMats], 32u, nullptr,
														 nullptr, nullptr))
							continue;
						sMatIdx[nMats] = mi;
						++nMats;
					}
					if (st.projMatSel >= nMats)
						st.projMatSel = nMats > 0 ? nMats - 1 : 0;
					const int32 selMat = nMats > 0 ? sMatIdx[st.projMatSel] : -1;

					NkRect rowR = rr;
					rowR.x = r.x + NkPropInset();
					rowR.w = rr.w - 2.f * NkPropInset();
					// ── LA LISTE + SA COLONNE DE BOUTONS ───────────────────────
					{
						const float32 colW = S(22.f);
						// Au moins la hauteur de la colonne de boutons (6 boutons) : en
						// dessous, epingle et fleches chevauchaient la ligne du nom
						// (constate par Rihen).
						const float32 lstH =
							st.projMatListH < S(146.f) ? S(146.f) : st.projMatListH;
						const NkRect lst{rowR.x, yy, rowR.w - colW - S(4.f), lstH};
						p.Fill(lst, NkRole::InputBg, 3.f);
						p.OutlineSharp(lst, NkRole::Border);
						p.Clip(lst);
						hit.PushClip(lst);
						const float32 lineH = S(20.f);
						float32 ly = lst.y + S(2.f) - st.projMatScroll;
						char lk[32];
						for (int32 i = 0; i < nMats; ++i) {
							const NkRect lr{lst.x + S(2.f), ly, lst.w - S(4.f), lineH};
							if (ly + lineH > lst.y && ly < lst.y + lst.h) {
								snprintf(lk, sizeof(lk), "props.pm.l.%d", i);
								const bool ovL = hit.Add(lk, lr);
								const bool selL = (i == st.projMatSel);
								if (selL)
									p.Fill(lr, NkRole::AccentUi, 2.f);
								else if (ovL)
									p.Fill(lr, NkRole::PanelHeader, 2.f);
								// Pastille de la COULEUR REELLE, comme la capture.
								float32 alb[3];
								demo::Demo3DHostProjMatInfo(sMatIdx[i], nullptr, 0u, alb,
															nullptr, nullptr);
								const NkColor cw{(uint8)(alb[0] * 255.f),
												 (uint8)(alb[1] * 255.f),
												 (uint8)(alb[2] * 255.f), 255};
								p.Fill({lr.x + S(4.f), lr.y + S(4.f), S(12.f), S(12.f)}, cw,
									   6.f);
								p.TextV(lr.x + S(22.f), lr.y, lineH, sMatNm[i],
										selL ? NkRole::TextOnAccent : NkRole::Text);
								// L'objet ACTIF porte-t-il ce materiau ?
								if (curOf == sMatIdx[i])
									p.IconV(lr.x + lr.w - S(16.f), lr.y, lineH, NkIcon::Check,
											selL ? NkRole::TextOnAccent : NkRole::AccentUi,
											11.f);
								// LA COCHE EST UN BOUTON : cliquer la zone droite ASSIGNE le
								// materiau a l'objet actif — avant, seul le mode edition savait
								// assigner et la coche semblait mentir (retour de Rihen).
								{
									char lkC[32];
									snprintf(lkC, sizeof(lkC), "props.pm.chk.%d", i);
									const NkRect ckR{lr.x + lr.w - S(20.f), lr.y, S(20.f), lineH};
									const bool ovC = hit.Add(lkC, ckR);
									if (ovC && actN >= 0 && curOf != sMatIdx[i])
										p.IconV(lr.x + lr.w - S(16.f), lr.y, lineH, NkIcon::Check,
												NkRole::TextMuted, 11.f);
									if (actN >= 0 && hit.Clicked(lkC)) {
										demo::Demo3DHostProjMatAssign(actN, sMatIdx[i]);
										st.projMatSel = i;
										NkMarkDirty(st);
									}
								}
								// CLIQUER UNE LIGNE = SELECTIONNER **ET** ASSIGNER a l'objet
								// actif : editer un materiau que l'objet ne porte pas etait le
								// piege n1 (captures de Rihen, 11 aout — il reglait l'opacite
								// d'un materiau que le cube ne portait pas). La coche reste le
								// temoin ; sans objet actif, le clic ne fait que selectionner.
								if (hit.Clicked(lk)) {
									// SELECTIONNER N'EST PAS ACTIVER (Rihen, 12 aout).
									// La liste ne contient que des materiaux DEJA
									// portes par l'objet : cliquer choisit celui qu'on
									// EDITE dans le panneau ci-dessous. C'est la COCHE
									// a droite qui decide lequel le rendu applique.
									st.projMatSel = i;
								}
							}
							ly += lineH;
						}
						hit.PopClip();
						p.Unclip();
						// DEFILEMENT : la liste garde sa hauteur, son contenu glisse.
						const float32 contH = (float32)nMats * lineH + S(4.f);
						if (hit.IsHovered("props.pm.list") && in.wheel != 0.f)
							st.projMatScroll -= in.wheel * lineH;
						const float32 maxSc = contH > lstH ? contH - lstH : 0.f;
						if (st.projMatScroll > maxSc)
							st.projMatScroll = maxSc;
						if (st.projMatScroll < 0.f)
							st.projMatScroll = 0.f;
						hit.Add("props.pm.list", lst);
						// COLONNE + / - / MENU, a droite de la liste (la capture).
						const float32 bx = lst.x + lst.w + S(4.f);
						{
							const NkRect ab{bx, lst.y, colW, S(20.f)};
							// LE « + » EST UN BOUTON SIMPLE. Il bascule le menu
							// d'ajout, qui se deroule SOUS LA LISTE et sur toute sa
							// largeur — pas dans cette colonne de 20 pixels, ou son
							// champ debordait sur les boutons voisins et masquait le
							// « - » (Rihen, 12 aout : « arrete de faire que les
							// boutons se chevauchent a droite de la liste »).
							const bool ovA = hit.Add("props.pm.add", ab);
							p.Outline(ab, ovA ? NkRole::AccentUi : NkRole::Border,
									  NkRole::PanelHeader, 3.f);
							p.IconV(ab.x + S(5.f), ab.y, ab.h, NkIcon::Add, NkRole::Text, 11.f);
							if (hit.Clicked("props.pm.add")) {
								// Le clic qui OUVRE est encore actif quand la modale est
								// peinte plus bas dans la MEME frame : sans garde, elle
								// l'attrape et se referme aussitot — « apparait et
								// disparait directement » (Rihen, 12 aout). Cette garde
								// vit maintenant DES DEUX COTES (drapeau applicatif et
								// cadre du kit) : `NkMatAddSetOpen` les arme ensemble.
								NkMatAddSetOpen(st, !st.matAddOpen);
							}
						}
						{
							// ── « - » : RETIRER DE CET OBJET ────────────────────
							// Il delie, il ne supprime pas : le materiau continue
							// d'exister dans le projet et sur les autres objets qui
							// le portent (regle de Rihen). Le « + » le remettra sans
							// qu'on ait a en recreer un.
							const NkRect rb{bx, lst.y + S(21.f), colW, S(20.f)};
							// LE DERNIER SE RETIRE AUSSI (Rihen, 13 aout). La garde a
							// ete levee dans le moteur, mais elle SUBSISTAIT ICI : le
							// bouton restait eteint sur le dernier materiau, si bien
							// que la regle semblait ne pas s'appliquer. Une regle
							// changee doit l'etre AUX DEUX BOUTS -- le moteur qui
							// autorise et l'interface qui propose.
							// Un objet sans materiau est rendu en magenta.
							const bool en = (actN >= 0) && (selMat >= 0) &&
											(demo::Demo3DHostNodeMatCount(actN) >= 1);
							const bool ovR = hit.Add("props.pm.del", rb);
							p.Outline(rb, (ovR && en) ? NkRole::AccentUi : NkRole::Border,
									  NkRole::PanelHeader, 3.f);
							// L'icone reste LISIBLE meme inactive : en TextMuted sur
							// ce fond sombre elle disparaissait, et le bouton semblait
							// ABSENT alors qu'il etait seulement desactive. Un bouton
							// grise dit « pas maintenant » ; un bouton invisible dit
							// « ca n'existe pas », et c'est faux.
							p.IconV(rb.x + S(5.f), rb.y, rb.h, NkIcon::MinusCircle,
									en ? NkRole::Text : NkRole::Border, 11.f);
							if (en && hit.Clicked("props.pm.del")) {
								if (demo::Demo3DHostNodeMatRemove(actN, selMat)) {
									if (st.projMatSel > 0)
										--st.projMatSel;
									NkMarkDirty(st);
								}
							}
						}
						// BOUTON « NOUVEAU » RETIRE (Rihen, 12 aout : « retire le bouton
						// nouveau, on va gerer ca apres »). La creation depuis la
						// pastille reviendra avec sa boite de dialogue — nom + dossier,
						// le dossier COURANT par defaut. Ce qui est acquis et reste :
						// st.projectRoot descend dans l'etat, donc un panneau PEUT
						// desormais ecrire un .nkmat sans attendre une sauvegarde.
						// L'echange d'emplacements est REEL (fichiers, cartes, melanges
						// suivent) : les cartes du navigateur sont recalees ici-meme.
						auto swapWithBrowser = [&](int32 sa, int32 sb) {
							if (!demo::Demo3DHostProjMatSwap(sa, sb))
								return false;
							for (int32 c2 = 0; c2 < st.browserCount; ++c2) {
								if (st.browserMat[c2] == sa + 1)
									st.browserMat[c2] = sb + 1;
								else if (st.browserMat[c2] == sb + 1)
									st.browserMat[c2] = sa + 1;
							}
							NkMarkDirty(st);
							return true;
						};
						{
							// EPINGLE : materiau PAR DEFAUT des nouveaux maillages, et il
							// REMONTE en tete de liste (demande de Rihen, 11 aout).
							const NkRect db{bx, lst.y + S(71.f), colW, S(20.f)};
							const bool isDef = demo::Demo3DHostProjMatDefault() == selMat;
							const bool ovD = hit.Add("props.pm.def", db);
							if (isDef)
								p.Fill(db, NkRole::AccentUi, 3.f);
							else
								p.Outline(db, ovD ? NkRole::AccentUi : NkRole::Border,
										  NkRole::PanelHeader, 3.f);
							// PAS UNE EPINGLE (Rihen, 12 aout) : l epingle dit « garder
							// sous la main », pas « celui-ci est le principal ». La
							// coche carree porte deja ce sens dans le projet — son
							// role documente est ASSIGNER.
							p.IconV(db.x + S(5.f), db.y, db.h, NkIcon::SquareCheck,
									isDef ? NkRole::TextOnAccent : NkRole::Text, 11.f);
							if (hit.Clicked("props.pm.def") && selMat >= 0) {
								// ── DEUX NOTIONS QUI SE RESSEMBLENT ─────────────
								// `SetDefault` designe le materiau par defaut du
								// PROJET -- celui que recevront les NOUVEAUX objets.
								// Il ne touche pas a l'objet selectionne : le bouton
								// semblait donc sans effet (Rihen, 13 aout : « ce
								// dernier n'est pas applique au model »).
								// C'est `Assign` qui pose le materiau ACTIF de CET
								// objet, celui qui est rendu -- et qui l'ajoute a sa
								// liste s'il n'y etait pas.
								const int32 nodeSel = demo::Demo3DHostActiveObject() >= 0
														  ? demo::Demo3DHostActiveObject()
														  : st.activeEmpty;
								if (nodeSel >= 0)
									demo::Demo3DHostProjMatAssign(nodeSel, selMat);
								demo::Demo3DHostProjMatSetDefault(selMat);
								NkMarkDirty(st);
								for (int32 rk = st.projMatSel; rk > 0; --rk)
									swapWithBrowser(sMatIdx[rk], sMatIdx[rk - 1]);
								st.projMatSel = 0;
							}
						}
						{
							// FLECHES : organisation de la liste (demande de Rihen).
							const NkRect ub{bx, lst.y + S(96.f), colW, S(20.f)};
							const bool enU = st.projMatSel > 0;
							const bool ovU = hit.Add("props.pm.up", ub);
							p.Outline(ub, (ovU && enU) ? NkRole::AccentUi : NkRole::Border,
									  NkRole::PanelHeader, 3.f);
							p.IconV(ub.x + S(5.f), ub.y, ub.h, NkIcon::ArrowUp,
									enU ? NkRole::Text : NkRole::TextMuted, 11.f);
							if (enU && hit.Clicked("props.pm.up") &&
								swapWithBrowser(sMatIdx[st.projMatSel], sMatIdx[st.projMatSel - 1]))
								--st.projMatSel;
							const NkRect db2{bx, lst.y + S(121.f), colW, S(20.f)};
							const bool enD = st.projMatSel + 1 < nMats;
							const bool ovD2 = hit.Add("props.pm.dn", db2);
							p.Outline(db2, (ovD2 && enD) ? NkRole::AccentUi : NkRole::Border,
									  NkRole::PanelHeader, 3.f);
							p.IconV(db2.x + S(5.f), db2.y, db2.h, NkIcon::ArrowDown,
									enD ? NkRole::Text : NkRole::TextMuted, 11.f);
							if (enD && hit.Clicked("props.pm.dn") &&
								swapWithBrowser(sMatIdx[st.projMatSel], sMatIdx[st.projMatSel + 1]))
								++st.projMatSel;
						}
						{
							// LE MENU du groupe, comme partout : copier / coller /
							// reinitialiser, dans la brique commune.
							const NkRect mb{bx, lst.y + S(46.f), colW, S(20.f)};
							const bool ovM = hit.Add("props.pm.menu", mb);
							const bool opM = (strcmp(st.grpMenuKey, "prop.g.mat") == 0);
							if (opM)
								p.Fill(mb, NkRole::AccentUi, 3.f);
							else
								p.Outline(mb, ovM ? NkRole::AccentUi : NkRole::Border,
										  NkRole::PanelHeader, 3.f);
							p.IconV(mb.x + S(5.f), mb.y, mb.h, NkIcon::ChevronDown,
									opM ? NkRole::TextOnAccent : NkRole::Text, 11.f);
							if (hit.Clicked("props.pm.menu")) {
								if (opM) {
									st.grpMenuKey[0] = 0;
		
								} else {
									NkWidgetState::Copy(st.grpMenuKey, "prop.g.mat", 39u);
									NkWidgetState::Copy(st.grpMenuTitle, "Materiau", 39u);
									st.grpMenuAnchor = mb;
								}
							}
						}
						yy += lstH;
						// Le panneau d'ajout est une MODALE (Rihen, 12 aout) : elle
						// est peinte plus bas, par-dessus tout le panneau, pas ici
						// dans le flux. Voir « MODALE D'AJOUT » en fin de section.

						// POIGNEE DE HAUTEUR, sous la liste (les points de la
						// capture) : la liste s'agrandit quand les materiaux se
						// multiplient.
						{
							const NkRect gh{lst.x, yy, lst.w, S(6.f)};
							const bool ovG = hit.Add("props.pm.grip", gh);
							const bool mineG = (strcmp(st.propDragKey, "props.pm.grip") == 0);
							if (ovG || mineG)
								hit.WantCursor(NkCursorWant::ResizeNS);
							p.Fill({gh.x + gh.w * 0.5f - S(9.f), gh.y + S(2.f), S(18.f), S(2.f)},
								   (ovG || mineG) ? NkRole::AccentUi : NkRole::Border);
							if (hit.MouseDown() && (ovG || mineG)) {
								if (!st.propDragKey[0] && ovG)
									NkWidgetState::Copy(st.propDragKey, "props.pm.grip", 39u);
								if (mineG || !st.propDragKey[0]) {
									float32 nh = hit.Mouse().y - lst.y;
									st.projMatListH = nh < S(48.f) ? S(48.f)
																   : (nh > S(320.f) ? S(320.f) : nh);
								}
							}
							yy += S(8.f);
						}
					}
					// ── LA BARRE DU NAVIGATEUR (sous la liste, comme la capture)
					// Icone + combo des materiaux + nom editable + delier.
					if (selMat >= 0) {
						char mnm[32];
						float32 alb[3], rgh = 0.85f, mtl = 0.f;
						demo::Demo3DHostProjMatInfo(selMat, mnm, sizeof(mnm), alb, &rgh, &mtl);
						{
							const NkRect br{rowR.x, yy + S(2.f), rowR.w, kRowH - S(4.f)};
							p.Outline(br, NkRole::Border, NkRole::InputBg, 3.f);
							// ── LE DEROULANT EST UNE ICONE, PAS UN LIBELLE ─────
							// Avec son nom affiche, il empietait sur le champ du
							// NOM juste a cote : on ne distinguait plus les deux
							// (constate par Rihen). Chez Blender l'icone EST le
							// navigateur -- elle ouvre la liste, le champ voisin
							// porte le nom. Le mode « icone seule » du combo se
							// demande en refusant a la fois cadre et chevron.
							static const char *sNavPtr[64];
							static NkIcon sNavIc[64];
							for (int32 i = 0; i < nMats; ++i) {
								sNavPtr[i] = sMatNm[i];
								sNavIc[i] = NkIcon::Material;
							}
							int32 navSel = st.projMatSel;
							Combo(p, hit, ws, "props.pm.nav",
								  {br.x + S(2.f), br.y + S(1.f), S(26.f), br.h - S(2.f)},
								  sNavPtr, sNavIc, nMats, navSel, combo, true, false, false);
							if (navSel != st.projMatSel)
								st.projMatSel = navSel;
							// Un TRAIT separe les deux commandes : l'oeil voit
							// « ouvrir la liste » puis « le nom », pas un bloc.
							p.VLine(br.x + S(30.f), br.y + S(3.f), br.h - S(6.f));
							// NOM editable (double-clic), clippe a son cadre.
							static char sNavName[32] = {};
							const NkRect nmR{br.x + S(35.f), br.y, br.w - S(63.f), br.h};
							p.Clip(nmR);
							if (EditableText(p, hit, ws, in, "props.pm.rename",
											 {nmR.x + S(2.f), nmR.y - S(2.f), nmR.w, kRowH},
											 mnm, NkRole::Text, sNavName, 31u))
								demo::Demo3DHostProjMatSetName(selMat, sNavName);
							p.Unclip();
							// DELIER : l'objet actif n'a plus ce materiau -- il en
							// reprend un, jamais aucun.
							const NkRect xb{br.x + br.w - S(24.f), br.y + S(2.f), S(20.f),
											br.h - S(4.f)};
							const bool ovX = hit.Add("props.pm.unlink", xb);
							const bool enX = (actN >= 0 && curOf == selMat);
							p.Outline(xb, (ovX && enX) ? NkRole::AccentUi : NkRole::Border,
									  NkRole::PanelHeader, 2.f);
							p.IconV(xb.x + S(4.f), xb.y, xb.h, NkIcon::WinClose,
									enX ? NkRole::Text : NkRole::TextMuted, 10.f);
							if (enX && hit.Clicked("props.pm.unlink"))
								demo::Demo3DHostProjMatAssign(actN, -1);
							yy += kRowH;
						}
						// ASSIGNER : geste du mode EDITION (regle de Rihen).
						if (demo::Demo3DHostInEditMode() && actN >= 0) {
							if (Button("props.pm.asg", yy, "Assigner", rowR.x, rowR.w))
								demo::Demo3DHostProjMatAssign(actN, selMat);
							yy += kRowH;
						}
						yy += NkPropGroupGap();
						// ── LES PROPRIETES DU MATERIAU SELECTIONNE ─────────────
						const bool grpMt = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.matp",
														  "Surface", 8192u);
						const float32 grpMtTop = yy;
						if (grpMt) {
							const NkRect iR = NkGroupInner(rowR);
							yy += NkGroupPad();
							// APERCU + RANGEE DES FORMES, sous l'image (Rihen,
							// 13 aout). Elles etaient empilees en colonne a droite :
							// a sept, la colonne depassait la hauteur de l'apercu.
							{
								const float32 side = S(104.f);
								const float32 btn = S(20.f);
								const float32 pvX = iR.x + (iR.w - side) * 0.5f;
								p.Image(4400u + (uint32)selMat, {pvX, yy, side, side});
								p.OutlineSharp({pvX, yy, side, side}, NkRole::Border);
								// SEPT FORMES, dans l'ordre de Blender. Chacune a son
								// propre dessin : `Liquid`, `Hair`, `Cloth` et `Monkey`
								// ont ete crees pour elles, plutot que d'emprunter
								// `Metaball` et `CurveBezier` -- qui designent
								// Ball/Ellipsoide/Metaball et Bezier/Cercle/NURBS/Chemin
								// dans le menu Ajouter. Un dessin partage entre deux
								// sujets ment aux deux.
								static const NkIcon kShpIc[7] = {
									NkIcon::Plane3D, NkIcon::SphereUV, NkIcon::Cube3D,
									NkIcon::Hair,	 NkIcon::Monkey,   NkIcon::Cloth,
									NkIcon::Liquid};
								// L'ORDRE D'AFFICHAGE N'EST PAS L'ORDRE DES VALEURS.
								// `prevShape` est serialise (« apercu ») dans les .nkmat
								// et les scenes : 0..4 gardent leur sens, tissu et tete
								// s'ajoutent en 5 et 6. Cette table fait la traduction,
								// et c'est elle qu'on remanie si l'ordre change encore.
								static const int32 kShpVal[7] = {0, 1, 2, 4, 6, 5, 3};
								const int32 shpCur = demo::Demo3DHostProjMatPrevShape(selMat);
								const float32 rangeeW = 7.f * btn + 6.f * S(1.f);
								float32 bx = iR.x + (iR.w - rangeeW) * 0.5f;
								const float32 byy = yy + side + S(4.f);
								for (int32 s7 = 0; s7 < 7; ++s7) {
									const int32 val = kShpVal[s7];
									snprintf(key, sizeof(key), "props.pm.s%d", val);
									const NkRect sb{bx, byy, btn, btn};
									hit.Add(key, sb);
									if (shpCur == val)
										p.Fill(sb, NkRole::AccentUi, 3.f);
									else
										p.Outline(sb, NkRole::Border, NkRole::InputBg, 3.f);
									p.IconV(sb.x + S(4.f), byy, btn, kShpIc[s7],
											shpCur == val ? NkRole::TextOnAccent
														  : NkRole::TextMuted,
											12.f);
									if (hit.Clicked(key))
										demo::Demo3DHostProjMatSetPrevShape(selMat, val);
									bx += btn + S(1.f);
								}
								yy += side + S(4.f) + btn + NkGroupPad();
							}
							const float32 a0 = alb[0], a1 = alb[1], a2 = alb[2];
							const float32 r0 = rgh, m0 = mtl;
							// ── TYPE DE MATERIAU (11 aout — « tout ce qui est public,
							// possibilite de choisir un type ») : le combo bascule le
							// GABARIT moteur ; stockage statique resynchronise (Loi).
							{
								// LE CATALOGUE VIENT DE NkModelerMatTypes.h — il est
								// PARTAGE avec la creation de materiau, qui fait choisir
								// le type avant d'exister (Rihen, 13 aout). Il vivait ici
								// en `static` ; deux copies auraient diverge au premier
								// type ajoute, et la liste n'aurait plus propose les
								// memes choix selon l'endroit d'ou on l'ouvre.
								const int32 tCur = demo::Demo3DHostProjMatType(selMat);
								const int32 tIdx = NkMatTypeIndexOf(tCur);
								static int32 sTySel = 0;
								static int32 sTyFor = -1;
								if (sTyFor == selMat && sTySel != tIdx) {
									const int32 pick = sTySel < 0 ? 0 : sTySel % kNkMatTypeCount;
									if (!kNkMatTypeOk[pick]) {
										sTySel = tIdx; // type pas encore valide : on reste
									} else {
										demo::Demo3DHostProjMatSetType(selMat, kNkMatTypeVal[pick]);
										NkMarkDirty(st);
									}
								} else {
									sTySel = tIdx;
								}
								sTyFor = selMat;
								p.TextV(iR.x, yy, kRowH, "Type", NkRole::TextMuted);
								Combo(p, hit, ws, "props.pm.type",
								      {iR.x + S(110.f), yy + S(2.f), iR.w - S(110.f), kRowH - S(4.f)},
								      kNkMatTypeNames, nullptr, kNkMatTypeCount, sTySel, combo, true,
								      true, true, NkIcon::Count, kNkMatTypeOff);
								yy += kRowH;
							}
							// L'INTERFACE SUIT LE TYPE (Rihen : « les proprietes du nouveau
							// type REMPLACENT les anciennes ») : les rangees PBR ne se
							// montrent que pour la famille realiste ; Toon a les siennes ;
							// Sans eclairage n'a que sa couleur.
							const int32 tFam9 = demo::Demo3DHostProjMatType(selMat);
							const bool famPBR =
								!(tFam9 == 20 || tFam9 == 21 || tFam9 == 22 || tFam9 == 60 ||
								  tFam9 == 11);
							// L'Emissif (11) sort de la famille PBR : Rihen — « emissive et
							// pbr ont les memes proprietes, c'est pas normal ». Il n'a QUE
							// couleur, canal couleur, canal emissif, emission, intensite.
							const bool famEmis = (tFam9 == 11);
							// LE VERRE A SES PROPRES REGLAGES : couleur, rugosite (le
							// flou du reflet), opacite et indice. Il n'a ni vernis, ni
							// metallique, ni anisotropie, ni sheen, ni diffusion — son
							// shader ne les lit pas. Les afficher trompait doublement,
							// puisque le champ `vernis` PORTE l'indice cote GPU : les
							// deux lignes montraient la meme valeur (capture de Rihen,
							// 12 aout 10h29).
							const bool famVerre = (tFam9 == 5);
							bool colCh = false;
							yy += PaintColorRow(p, hit, ws, in, st, iR, yy, "Couleur",
												"props.pm.col", alb, &colCh);
							// ── LES QUATRE CANAUX DE TEXTURE ────────────────────
							// Couleur, Normale, ORM, Emissif : le moteur les porte
							// depuis toujours, seule la couleur etait reglable ici.
							// UNE boucle pour les quatre -- quatre blocs recopies
							// auraient diverge au premier correctif.
							// Chaque canal a SON tampon de saisie statique : un
							// tampon partage ferait sauter le texte d'un champ a
							// l'autre pendant la frappe (NkComboPending a deja
							// coute cette lecon).
							{
								static char sPmTex[5][260] = {};
								static const char *const kPmKeys[5] = {
									"props.pm.tex0", "props.pm.tex1", "props.pm.tex2",
									"props.pm.tex3", "props.pm.tex4"};
								const int32 nCh = demo::Demo3DHostMatChanCount();
								for (int32 ch = 0; ch < nCh && ch < 5; ++ch) {
									// Hors famille PBR, seule la COULEUR a un sens ici —
									// sauf l'Emissif, qui garde aussi SON canal (3).
									if (!famPBR && ch != 0 && !(famEmis && ch == 3))
										continue;
									p.TextV(iR.x, yy, kRowH, demo::Demo3DHostMatChanName(ch),
											NkRole::TextMuted);
									const NkRect txR{iR.x + S(110.f), yy + S(2.f),
													 iR.w - S(110.f) - S(22.f), kRowH - S(4.f)};
									p.Outline(txR, NkRole::Border, NkRole::InputBg, 3.f);
									const char *curTx = demo::Demo3DHostProjMatMap(selMat, ch);
									p.Clip(txR);
									const bool txApply = EditableText(
										p, hit, ws, in, kPmKeys[ch],
										{txR.x + S(4.f), yy, txR.w - S(8.f), kRowH},
										curTx[0] ? curTx : "aucune",
										curTx[0] ? NkRole::Text : NkRole::TextMuted,
										sPmTex[ch], 259u);
									p.Unclip();
									if (txApply)
										demo::Demo3DHostProjMatSetMap(selMat, ch, sPmTex[ch]);
									// RETIRER : n'apparait que si le canal porte
									// quelque chose -- un bouton qui n'a rien a
									// enlever n'a rien a faire la.
									if (curTx[0]) {
										char rk[24];
										snprintf(rk, sizeof(rk), "props.pm.texx%d", ch);
										const NkRect xr{iR.x + iR.w - S(20.f), yy + S(3.f),
														S(18.f), kRowH - S(6.f)};
										const bool ovx = hit.Add(rk, xr);
										p.Outline(xr, NkRole::Border,
												  ovx ? NkRole::PanelBg : NkRole::PanelHeader,
												  3.f);
										p.IconV(xr.x + S(3.f), yy, kRowH, NkIcon::Trash,
												NkRole::TextMuted, 11.f);
										if (hit.Clicked(rk))
											demo::Demo3DHostProjMatSetMap(selMat, ch, "-");
									}
									yy += kRowH;
								}
								// INTENSITES : elles ne s'affichent QUE si leur
								// texture existe -- un curseur sans effet est pire
								// qu'un curseur absent (regle du projet).
								float32 nrmS = 1.f, emiS = 1.f;
								demo::Demo3DHostProjMatChanStrength(selMat, &nrmS, &emiS);
								const float32 nrm0 = nrmS, emi0 = emiS;
								if (famPBR && demo::Demo3DHostProjMatMap(selMat, 1)[0]) {
									p.TextV(iR.x, yy, kRowH, "Relief", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "props.pm.nrms",
											  {iR.x + S(110.f), yy + S(3.f), iR.w - S(110.f),
											   kRowH - S(6.f)},
											  nrmS, 0.01f, NkRole::AccentUi, "%.2f");
									yy += kRowH;
								}
								// PARALLAX : meme regle que le relief — le curseur
								// n'apparait qu'avec sa carte de hauteur (canal 4).
								if (famPBR && demo::Demo3DHostProjMatMap(selMat, 4)[0]) {
									float32 par = demo::Demo3DHostProjMatParallax(selMat);
									const float32 par0 = par;
									p.TextV(iR.x, yy, kRowH, "Parallax", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "props.pm.parx",
											  {iR.x + S(110.f), yy + S(3.f), iR.w - S(110.f),
											   kRowH - S(6.f)},
											  par, 0.001f, NkRole::AccentUi, "%.3f");
									yy += kRowH;
									if (par != par0) {
										demo::Demo3DHostProjMatSetParallax(selMat, par);
										NkMarkDirty(st);
									}
								}
								if (famPBR || famEmis) {
									// L'EMISSIF a une teinte ET une intensite, et la
									// teinte vaut MEME SANS texture : une surface peut
									// emettre une couleur unie.
									float32 emiC[3] = {0.f, 0.f, 0.f};
									demo::Demo3DHostProjMatEmissive(selMat, emiC);
									const float32 e0 = emiC[0], e1 = emiC[1], e2 = emiC[2];
									bool emiCh = false;
									yy += PaintColorRow(p, hit, ws, in, st, iR, yy, "Emission",
														"props.pm.emi", emiC, &emiCh);
									p.TextV(iR.x, yy, kRowH, "Intensite", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "props.pm.emis",
											  {iR.x + S(110.f), yy + S(3.f), iR.w - S(110.f),
											   kRowH - S(6.f)},
											  emiS, 0.02f, NkRole::AccentUi, "%.2f");
									yy += kRowH;
									if (nrmS != nrm0 || emiS != emi0)
										demo::Demo3DHostProjMatSetChanStrength(selMat, nrmS, emiS);
									if (emiCh || emiC[0] != e0 || emiC[1] != e1 || emiC[2] != e2)
										demo::Demo3DHostProjMatSetEmissive(selMat, emiC);
								}
							}
							if (famPBR) {
								p.TextV(iR.x, yy, kRowH, "Rugosite", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "props.pm.rgh",
										  {iR.x + S(110.f), yy + S(3.f), iR.w - S(110.f),
										   kRowH - S(6.f)},
										  rgh, 0.005f, NkRole::AccentUi, "%.2f");
								yy += kRowH;
								if (!famVerre) {
									p.TextV(iR.x, yy, kRowH, "Metallique", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "props.pm.mtl",
											  {iR.x + S(110.f), yy + S(3.f), iR.w - S(110.f),
											   kRowH - S(6.f)},
											  mtl, 0.005f, NkRole::AccentUi, "%.2f");
									yy += kRowH;
								}
							}
							// ── LES REGLAGES PBR RESTES SANS CURSEUR (11 aout) ──────────
							{
								float32 xAl = 1.f, xAn = 0.f, xSh = 0.f;
								demo::Demo3DHostProjMatPBRExtra(selMat, &xAl, &xAn, &xSh);
								const float32 xa0 = xAl, xn0 = xAn, xs0 = xSh;
								// L'OPACITE EST UNIVERSELLE : elle decrit l'objet, pas un
								// modele d'ombrage — Blender l'expose sur tous ses shaders.
								// Reservee a la famille PBR, elle RESTAIT POURTANT ACTIVE
								// apres un passage en Toon : le cube gardait la transparence
								// reglee en PBR sans plus aucun curseur pour la reprendre
								// (capture de Rihen, 12 aout 07:12). Elle sort donc du bloc.
								p.TextV(iR.x, yy, kRowH, "Opacite", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "props.pm.opa",
								          {iR.x + S(110.f), yy + S(3.f), iR.w - S(110.f), kRowH - S(6.f)},
								          xAl, 0.005f, NkRole::AccentUi, "%.2f");
								yy += kRowH;
								if (xAl != xa0) {
									demo::Demo3DHostProjMatSetPBRExtra(selMat, xAl, xAn, xSh);
									NkMarkDirty(st);
								}
								// ── VERRE : L'INDICE DE REFRACTION ──────────────────
								// Le Fresnel a une valeur PHYSIQUE par defaut (n=1.5,
								// le verre a vitre) ; ce curseur permet a l'artiste de
								// l'outrepasser — 1.0 aucun reflet, 2.4 diamant, au-dela
								// non physique mais assume (Rihen : « les artistes
								// pourront faire des choses waou »). Le champ `vernis` du
								// materiau porte l'indice pour ce type, comme Unreal
								// reaffecte ses entrees selon le modele d'ombrage.
								if (tFam9 == 5) {
									float32 gcc = 0.f, gccR = 0.f, gsss = 0.f;
									demo::Demo3DHostProjMatSurface(selMat, &gcc, &gccR, &gsss);
									// LA COCHE COMMANDE, LE CURSEUR SE SOUVIENT (Rihen) :
									// decochee, le shader prend la valeur PHYSIQUE (1.5) et
									// le curseur reste inerte en affichant la DERNIERE
									// valeur reglee. La memoire vit dans le signe : gcc
									// NEGATIF = « desactive, mais je garde |gcc| ». Aucun
									// champ supplementaire a propager jusqu'a l'UBO, au
									// .nkmat et aux facades — le materiau ne connait que
									// l'indice effectif.
									const bool iorOn = gcc > 1.f;
									float32 ior = gcc > 1.f ? gcc : (gcc < -1.f ? -gcc : 1.5f);
									const float32 ior0 = ior;
									// La coche.
									const NkRect ckI{iR.x, yy + S(4.f), S(16.f), S(16.f)};
									const bool ovI = hit.Add("props.pm.iorx", ckI);
									p.Outline(ckI, NkRole::Border,
											  ovI ? NkRole::PanelBg : NkRole::InputBg, 3.f);
									if (iorOn)
										p.IconV(ckI.x + S(2.f), yy, kRowH, NkIcon::Check,
												NkRole::AccentUi, 11.f);
									if (hit.Clicked("props.pm.iorx")) {
										// Bascule SANS perdre la valeur : on ne fait que
										// changer son signe.
										demo::Demo3DHostProjMatSetSurface(
											selMat, iorOn ? -ior : ior, gccR, gsss);
										NkMarkDirty(st);
									}
									p.TextV(iR.x + S(22.f), yy, kRowH, "Indice (Fresnel)",
											iorOn ? NkRole::Text : NkRole::TextMuted);
									if (iorOn) {
										DragFloat(p, hit, ws, in, "props.pm.ior",
										          {iR.x + S(130.f), yy + S(3.f), iR.w - S(130.f),
										           kRowH - S(6.f)},
										          ior, 0.01f, NkRole::AccentUi, "%.2f");
									} else {
										// Inerte : la derniere valeur reste LISIBLE, ce qui
										// evite de la retrouver a l'aveugle en reactivant.
										char iorTxt[24];
										snprintf(iorTxt, sizeof(iorTxt), "%.2f (auto)", ior);
										p.TextV(iR.x + S(130.f), yy, kRowH, iorTxt,
												NkRole::TextMuted);
									}
									yy += kRowH;
									// ── LA MEME GRANDEUR, DEUX VOCABULAIRES ──────────
									// Blender fait saisir l'INDICE (tabule pour chaque
									// matiere), Unreal la REFLECTANCE (son « Specular »).
									// Les deux sont strictement equivalents :
									//     F0 = ((n-1)/(n+1))^2   et   n = (1+VF0)/(1-VF0)
									// Rihen : « je veux les deux ». On n'en STOCKE qu'une
									// (l'indice) et on derive l'autre a l'affichage : deux
									// valeurs stockees finiraient par diverger.
									{
										const float32 r0 = (ior - 1.f) / (ior + 1.f);
										float32 f0 = r0 * r0;
										const float32 f00 = f0;
										p.TextV(iR.x + S(22.f), yy, kRowH, "Reflectance F0",
												iorOn ? NkRole::Text : NkRole::TextMuted);
										if (iorOn) {
											DragFloat(p, hit, ws, in, "props.pm.f0",
											          {iR.x + S(130.f), yy + S(3.f),
											           iR.w - S(130.f), kRowH - S(6.f)},
											          f0, 0.002f, NkRole::AccentUi, "%.3f");
											if (f0 != f00) {
												// F0 -> n. Borne a 0.99 : a 1 la formule
												// diverge (n infini), et une reflectance de
												// 1 n'existe pas — un miroir parfait non
												// plus.
												if (f0 < 0.f)
													f0 = 0.f;
												if (f0 > 0.99f)
													f0 = 0.99f;
												const float32 sq = (float32)sqrt((double)f0);
												ior = (1.f + sq) / (1.f - sq);
												demo::Demo3DHostProjMatSetSurface(selMat, ior,
												                                  gccR, gsss);
												NkMarkDirty(st);
											}
										} else {
											char f0Txt[24];
											snprintf(f0Txt, sizeof(f0Txt), "%.3f (auto)", f0);
											p.TextV(iR.x + S(130.f), yy, kRowH, f0Txt,
													NkRole::TextMuted);
										}
										yy += kRowH;
									}
									if (iorOn && ior != ior0) {
										// SEULE CONTRAINTE : n > 0. Le facteur de Fresnel,
										// lui, est borne [0,1] par conservation d'energie —
										// et la formule F0 = ((n-1)/(n+1))^2 le garantit
										// d'elle-meme pour tout n > 0. Plafonner l'indice
										// serait donc arbitraire : le silicium vaut ~3.9,
										// le germanium 4 a 5 dans l'infrarouge, et les
										// metamateriaux descendent sous 1.
										if (ior < 0.01f)
											ior = 0.01f;
										demo::Demo3DHostProjMatSetSurface(selMat, ior, gccR, gsss);
										NkMarkDirty(st);
									}
								}
								// OMBRE DE L'OBJET TRANSPARENT — n'apparait qu'une
								// fois l'objet reellement transparent : un choix
								// sans effet est pire qu'un choix absent (regle du
								// projet). « Coloree » viendra avec son tampon
								// d'ombre en couleur, elle reste donc grisee.
								if (xAl < 0.999f) {
									static const char *const kShTypes[4] = {
										"Pleine", "Proportionnelle", "Aucune",
										"Coloree (bientot)"};
									static const bool kShOff[4] = {false, false, false, true};
									const int32 shCur = demo::Demo3DHostProjMatShadowMode(selMat);
									static int32 sShSel = 1;
									static int32 sShFor = -1;
									if (sShFor == selMat && sShSel != shCur) {
										if (sShSel >= 0 && sShSel <= 2) {
											demo::Demo3DHostProjMatSetShadowMode(selMat, sShSel);
											NkMarkDirty(st);
										} else {
											sShSel = shCur; // « Coloree » : on reste
										}
									} else {
										sShSel = shCur;
									}
									sShFor = selMat;
									p.TextV(iR.x, yy, kRowH, "Ombre", NkRole::TextMuted);
									Combo(p, hit, ws, "props.pm.shm",
									      {iR.x + S(110.f), yy + S(2.f), iR.w - S(110.f),
									       kRowH - S(4.f)},
									      kShTypes, nullptr, 4, sShSel, combo, true, true, true,
									      NkIcon::Count, kShOff);
									yy += kRowH;
								}
							}
							if (famPBR) {
								float32 xAl = 1.f, xAn = 0.f, xSh = 0.f;
								demo::Demo3DHostProjMatPBRExtra(selMat, &xAl, &xAn, &xSh);
								const float32 xa0 = xAl, xn0 = xAn, xs0 = xSh;
								if (!famVerre) {
								p.TextV(iR.x, yy, kRowH, "Anisotropie", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "props.pm.ani",
								          {iR.x + S(110.f), yy + S(3.f), iR.w - S(110.f), kRowH - S(6.f)},
								          xAn, 0.005f, NkRole::AccentUi, "%.2f");
								yy += kRowH;
								p.TextV(iR.x, yy, kRowH, "Sheen", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "props.pm.shn",
								          {iR.x + S(110.f), yy + S(3.f), iR.w - S(110.f), kRowH - S(6.f)},
								          xSh, 0.005f, NkRole::AccentUi, "%.2f");
								yy += kRowH;
								}
								if (xAl != xa0 || xAn != xn0 || xSh != xs0) {
									demo::Demo3DHostProjMatSetPBRExtra(selMat, xAl, xAn, xSh);
									NkMarkDirty(st);
								}
							}
							// ── FAMILLE TOON : ses reglages n'apparaissent QUE pour elle ──
							{
								const int32 tNow = demo::Demo3DHostProjMatType(selMat);
								if (tNow == 20 || tNow == 21 || tNow == 22) {
									float32 tv[14];
									demo::Demo3DHostProjMatToon(selMat, tv);
									float32 t0[14];
									for (int32 k4 = 0; k4 < 14; ++k4)
										t0[k4] = tv[k4];
									p.TextV(iR.x, yy, kRowH, "Seuil d'ombre", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "props.pm.tth",
									          {iR.x + S(110.f), yy + S(3.f), iR.w - S(110.f), kRowH - S(6.f)},
									          tv[0], 0.005f, NkRole::AccentUi, "%.2f");
									yy += kRowH;
									p.TextV(iR.x, yy, kRowH, "Adoucissement", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "props.pm.tsm",
									          {iR.x + S(110.f), yy + S(3.f), iR.w - S(110.f), kRowH - S(6.f)},
									          tv[1], 0.005f, NkRole::AccentUi, "%.2f");
									yy += kRowH;
									bool tc1 = false, tc2 = false, tc3 = false;
									yy += PaintColorRow(p, hit, ws, in, st, iR, yy, "Ombre toon",
									                    "props.pm.tsc", &tv[2], &tc1);
									p.TextV(iR.x, yy, kRowH, "Contour", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "props.pm.tow",
									          {iR.x + S(110.f), yy + S(3.f), iR.w - S(110.f), kRowH - S(6.f)},
									          tv[5], 0.02f, NkRole::AccentUi, "%.1f");
									yy += kRowH;
									yy += PaintColorRow(p, hit, ws, in, st, iR, yy, "Contour couleur",
									                    "props.pm.toc", &tv[6], &tc2);
									p.TextV(iR.x, yy, kRowH, "Lisere", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "props.pm.tri",
									          {iR.x + S(110.f), yy + S(3.f), iR.w - S(110.f), kRowH - S(6.f)},
									          tv[9], 0.005f, NkRole::AccentUi, "%.2f");
									yy += kRowH;
									yy += PaintColorRow(p, hit, ws, in, st, iR, yy, "Lisere couleur",
									                    "props.pm.trc", &tv[10], &tc3);
									p.TextV(iR.x, yy, kRowH, "Durete speculaire", NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "props.pm.tsh",
									          {iR.x + S(110.f), yy + S(3.f), iR.w - S(110.f), kRowH - S(6.f)},
									          tv[13], 0.2f, NkRole::AccentUi, "%.0f");
									yy += kRowH;
									bool tDiff = tc1 || tc2 || tc3;
									for (int32 k4 = 0; k4 < 14 && !tDiff; ++k4)
										if (tv[k4] != t0[k4])
											tDiff = true;
									if (tDiff) {
										demo::Demo3DHostProjMatSetToon(selMat, tv);
										NkMarkDirty(st);
									}
								}
							}
							// ── PHYSIQUE DE SURFACE (passation §5, « gain le moins
							// cher ») : le shader calcule vernis et diffusion depuis
							// longtemps, seuls ces curseurs manquaient. La rugosite du
							// vernis n'apparait QUE si le vernis existe — un curseur
							// sans effet est pire qu'un curseur absent (regle du
							// projet). La couleur de diffusion suit l'albedo.
							if (famPBR && !famVerre) {
								float32 cc = 0.f, ccR = 0.f, sss = 0.f;
								demo::Demo3DHostProjMatSurface(selMat, &cc, &ccR, &sss);
								const float32 cc0 = cc, ccR0 = ccR, sss0 = sss;
								p.TextV(iR.x, yy, kRowH, "Vernis", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "props.pm.cc",
										  {iR.x + S(110.f), yy + S(3.f), iR.w - S(110.f),
										   kRowH - S(6.f)},
										  cc, 0.005f, NkRole::AccentUi, "%.2f");
								yy += kRowH;
								if (cc > 0.f) {
									p.TextV(iR.x, yy, kRowH, "Vernis rugosite",
											NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "props.pm.ccr",
											  {iR.x + S(110.f), yy + S(3.f),
											   iR.w - S(110.f), kRowH - S(6.f)},
											  ccR, 0.005f, NkRole::AccentUi, "%.2f");
									yy += kRowH;
								}
								p.TextV(iR.x, yy, kRowH, "Diffusion", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "props.pm.sss",
										  {iR.x + S(110.f), yy + S(3.f), iR.w - S(110.f),
										   kRowH - S(6.f)},
										  sss, 0.005f, NkRole::AccentUi, "%.2f");
								yy += kRowH;
								if (cc != cc0 || ccR != ccR0 || sss != sss0)
									demo::Demo3DHostProjMatSetSurface(selMat, cc, ccR, sss);
							}
							// ── MELANGE (etape 1 — Rihen : « mixer, operations, comme
							// Blender et Unreal ») ─────────────────────────────────────
							// B + source du masque + facteur. Les combos ECRIVENT EN FIN
							// DE FRAME par pointeur : selections en stockage STATIQUE
							// resynchronise (lecon de la « Loi »), libelles en tampons
							// statiques qui survivent a la frame.
							if (famPBR) {
								static char sMxNames[65][32];
								static const char *sMxPtr[65];
								static int32 sMxSlot[65];
								snprintf(sMxNames[0], 32, "%s", "Aucun");
								sMxPtr[0] = sMxNames[0];
								sMxSlot[0] = -1;
								int32 nMx = 1;
								for (int32 mI = 0; mI < 64 && nMx < 65; ++mI) {
									if (mI == selMat)
										continue;
									char nm3[64];
									if (!demo::Demo3DHostProjMatInfo(mI, nm3, sizeof(nm3), nullptr,
									                                 nullptr, nullptr))
										continue;
									snprintf(sMxNames[nMx], 32, "%s", nm3);
									sMxPtr[nMx] = sMxNames[nMx];
									sMxSlot[nMx] = mI;
									++nMx;
								}
								const int32 curB = demo::Demo3DHostProjMatMixWith(selMat);
								const int32 curSrc = demo::Demo3DHostProjMatMixSource(selMat);
								float32 fac = demo::Demo3DHostProjMatMixFactor(selMat);
								int32 curIdx = 0;
								for (int32 k2 = 1; k2 < nMx; ++k2)
									if (sMxSlot[k2] == curB)
										curIdx = k2;
								static int32 sMxSel = 0;
								static int32 sMxSrcSel = 0;
								static int32 sMxFor = -1;
								if (sMxFor == selMat && sMxSel != curIdx) {
									const int32 slotB = (sMxSel >= 0 && sMxSel < nMx) ? sMxSlot[sMxSel] : -1;
									demo::Demo3DHostProjMatSetMix(selMat, slotB, curSrc, fac);
									NkMarkDirty(st);
								} else {
									sMxSel = curIdx;
								}
								if (sMxFor == selMat && sMxSrcSel != curSrc) {
									demo::Demo3DHostProjMatSetMix(selMat, demo::Demo3DHostProjMatMixWith(selMat),
									                              sMxSrcSel, fac);
									NkMarkDirty(st);
								} else {
									sMxSrcSel = curSrc;
								}
								sMxFor = selMat;
								p.TextV(iR.x, yy, kRowH, "Melanger avec", NkRole::TextMuted);
								Combo(p, hit, ws, "props.pm.mixb",
								      {iR.x + S(110.f), yy + S(2.f), iR.w - S(110.f), kRowH - S(4.f)},
								      sMxPtr, nullptr, nMx, sMxSel, combo);
								yy += kRowH;
								if (demo::Demo3DHostProjMatMixWith(selMat) >= 0) {
									static const char *const kMxSrc[7] = {
									    "Facteur", "Sommets R", "Sommets V", "Sommets B",
									    "Sommets A", "Degrade UV X", "Degrade UV Y"};
									p.TextV(iR.x, yy, kRowH, "Masque", NkRole::TextMuted);
									Combo(p, hit, ws, "props.pm.mixs",
									      {iR.x + S(110.f), yy + S(2.f), iR.w - S(110.f), kRowH - S(4.f)},
									      kMxSrc, nullptr, 7, sMxSrcSel, combo);
									yy += kRowH;
									if (sMxSrcSel == 0) {
										const float32 f0 = fac;
										p.TextV(iR.x, yy, kRowH, "Facteur", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "props.pm.mixf",
										          {iR.x + S(110.f), yy + S(3.f), iR.w - S(110.f), kRowH - S(6.f)},
										          fac, 0.005f, NkRole::AccentUi, "%.2f");
										yy += kRowH;
										if (fac != f0) {
											demo::Demo3DHostProjMatSetMix(selMat,
											                              demo::Demo3DHostProjMatMixWith(selMat),
											                              sMxSrcSel, fac);
											NkMarkDirty(st);
										}
									}
								}
							}
							if (colCh || alb[0] != a0 || alb[1] != a1 || alb[2] != a2 ||
								rgh != r0 || mtl != m0)
								demo::Demo3DHostProjMatSetParams(selMat, alb, rgh, mtl);
							yy += NkGroupPad();
							PaintGroupBlock(p, rowR, grpMtTop, yy);
						}
						yy += NkPropGroupGap();
						// LES ACTIONS DU MENU, pour ce materiau.
						if (NkGrpWants(st, "prop.g.mat", 1)) {
							const float32 v5[5] = {alb[0], alb[1], alb[2], rgh, mtl};
							NkGrpCopyF(st, "prop.g.mat", v5, 5);
						}
						if (NkGrpWants(st, "prop.g.mat", 2) &&
							NkGrpCanPaste(st, "prop.g.mat")) {
							const float32 a5[3] = {st.grpClipF[0], st.grpClipF[1],
												   st.grpClipF[2]};
							demo::Demo3DHostProjMatSetParams(selMat, a5, st.grpClipF[3],
															 st.grpClipF[4]);
						}
						if (NkGrpWants(st, "prop.g.mat", 3)) {
							const float32 g5[3] = {0.7f, 0.7f, 0.7f};
							demo::Demo3DHostProjMatSetParams(selMat, g5, 0.85f, 0.f);
							// Un materiau neuf n'a ni vernis ni diffusion.
							demo::Demo3DHostProjMatSetSurface(selMat, 0.f, 0.f, 0.f);
							// REINITIALISER retire les QUATRE canaux : n'en
							// oublier qu'un laisserait un relief ou un emissif
							// invisible dans un materiau cense etre neuf.
							const int32 nCh2 = demo::Demo3DHostMatChanCount();
							for (int32 ch = 0; ch < nCh2; ++ch)
								demo::Demo3DHostProjMatSetMap(selMat, ch, "-");
							const float32 e0[3] = {0.f, 0.f, 0.f};
							demo::Demo3DHostProjMatSetEmissive(selMat, e0);
							demo::Demo3DHostProjMatSetChanStrength(selMat, 1.f, 1.f);
						}
					}
		}

		// ── PASTILLE « SORTIE » ─────────────────────────────────────────────────
		// Ce qui SORT de la scene : sources (vue 3D ou cameras), destination,
		// types de rendu, aides, video et incrustations, puis le declenchement du
		// rendu lui-meme.
		inline void PaintPropOutput(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
									NkWidgetState &ws, const nkgui::NkGuiInput &in,
									NkComboPending &combo, nkgui::NkGuiContext *guiCtx,
									const NkRect &r, const NkRect &rr, float32 &yy) {
			auto Button = [&](const char *k2, float32 yB, const char *label, float32 x,
							  float32 w) -> bool {
				return NkPropButton(p, hit, k2, yB, label, x, w);
			};
			(void)Button;
			char key[64];
			char buf[160];
			(void)buf;
					// ══ OUTPUT : CE QUI SORT DE LA SCENE (Rihen) ═══════════════
					// Une cible PRINCIPALE et, posees dessus, jusqu'a huit
					// INCRUSTATIONS de formes libres. La resolution ne depend
					// PAS de la taille de la fenetre : sans cela le champ ne
					// serait qu'une decoration.
					NkRect rowR = rr;
					rowR.x = r.x + NkPropInset();
					rowR.w = rr.w - 2.f * NkPropInset();
					// ── Sources disponibles : la vue 3D, puis chaque camera.
					// Construites une fois, elles servent la principale ET les
					// incrustations -- deux listes divergeraient.
					// ── LES LISTES DE COMBO DOIVENT SURVIVRE A LA FRAME ─────
					// NkComboPending garde les pointeurs `items` ET `selected`
					// pour peindre la liste deroulante PLUS TARD, par-dessus le
					// reste. Passer un tableau local revenait donc a lui confier
					// des adresses de pile deja mortes au moment ou elle peint
					// et ou elle ecrit le choix : le combo s'ouvrait, mais
					// choisir ne changeait rien (Rihen : « pourquoi on ne peut
					// pas choisir la source ? »). Tous les combos du projet
					// passent des tableaux statiques et un etat persistant ;
					// ceux-ci s'y conforment.
					int32 camNodes[16];
					const int32 nCam = demo::Demo3DHostSceneCameras(camNodes, 16);
					static char srcBuf[18][40];
					static const char *srcNames[18];
					// Plus d'entree « camera active » (Rihen) : elle est deja celle
					// qu'on voit, et une source qui se deplace toute seule rendait
					// imprevisible ce qu'on s'appretait a produire.
					snprintf(srcBuf[0], sizeof(srcBuf[0]), "Vue 3D");
					srcNames[0] = srcBuf[0];
					for (int32 c5 = 0; c5 < nCam && c5 < 16; ++c5) {
						// NkHierNodeName, PAS Demo3DHostObjectName : les NOEUDS
						// et les OBJETS sont deux espaces d'indices distincts.
						// Passer un numero de noeud a la fonction des objets
						// nommait un tout autre element de la scene -- une
						// camera s'annoncait « mur gi » (constate par Rihen).
						char cn[32] = {};
						NkHierNodeName(st, camNodes[c5], cn, sizeof(cn));
						// (Le depot du nom vers l'hote a ete deplace dans la
						// synchronisation generale de main.cpp : ici, il aurait
						// fallu avoir ouvert ce panneau au moins une fois pour
						// que les fichiers portent le bon nom -- une condition
						// qu'on ne devine pas.)
						snprintf(srcBuf[c5 + 1], sizeof(srcBuf[0]), "%s",
								 cn[0] ? cn : "Camera");
						srcNames[c5 + 1] = srcBuf[c5 + 1];
					}
					const int32 nSrc = nCam + 1;
					// Noeud <-> rang dans la liste. La liste bouge quand on
					// ajoute une camera ; le noeud, lui, ne bouge pas -- c'est
					// donc LUI qu'on memorise, et le rang se recalcule.
					// Rang 0 = vue 3D (-1), ensuite les cameras nommees. C'est le
					// NOEUD qu'on memorise, jamais le rang : ajouter une camera
					// decale la liste.
					auto srcIndexOf = [&](int32 node) {
						if (node < 0)
							return 0;
						for (int32 c5 = 0; c5 < nCam && c5 < 16; ++c5)
							if (camNodes[c5] == node)
								return c5 + 1;
						return 0;
					};
					auto srcNodeOf = [&](int32 idx) {
						return (idx <= 0 || idx - 1 >= nCam) ? -1 : camNodes[idx - 1];
					};
					// La source principale, lue AVANT de batir la liste des
					// miniatures : c'est elle qu'on en retire.
					int32 oSrcCur = -1;
					demo::Demo3DHostOutMain(&oSrcCur, nullptr, nullptr, nullptr, nullptr,
											nullptr);
					// ── LISTE DES MINIATURES : SANS LA SOURCE PRINCIPALE ────
					// Une vue n'est pas a la fois principale et miniature
					// (Rihen), donc la principale ne doit meme pas etre
					// PROPOSABLE ici -- seul un echange, ou le fait qu'elle
					// cesse d'etre principale, l'y ramene. Une entree qu'on ne
					// peut pas choisir n'a rien a faire dans une liste.
					static char insBuf[17][40];
					static const char *insNames[17];
					int32 insNodes[17];
					int32 nIns = 0;
					if (oSrcCur != -1) {
						snprintf(insBuf[nIns], sizeof(insBuf[0]), "Vue 3D");
						insNames[nIns] = insBuf[nIns];
						insNodes[nIns++] = -1;
					}
					for (int32 c5 = 0; c5 < nCam && c5 < 16; ++c5) {
						if (camNodes[c5] == oSrcCur)
							continue;
						char cn[32] = {};
						NkHierNodeName(st, camNodes[c5], cn, sizeof(cn));
						snprintf(insBuf[nIns], sizeof(insBuf[0]), "%s", cn[0] ? cn : "Camera");
						insNames[nIns] = insBuf[nIns];
						insNodes[nIns++] = camNodes[c5];
					}
					auto insIndexOf = [&](int32 node) {
						for (int32 i5 = 0; i5 < nIns; ++i5)
							if (insNodes[i5] == node)
								return i5;
						return 0;
					};
					auto insNodeOf = [&](int32 idx) {
						return (idx >= 0 && idx < nIns) ? insNodes[idx] : -1;
					};

					int32 oSrc = -1, oW = 1920, oH = 1080, oScale = 100, oFmt = 0;
					bool oTrans = false;
					demo::Demo3DHostOutMain(&oSrc, &oW, &oH, &oScale, &oFmt, &oTrans);
					const int32 oSrc0 = oSrc, oW0 = oW, oH0 = oH, oScale0 = oScale;
					const int32 oFmt0 = oFmt;
					const bool oTrans0 = oTrans;

					// ── GROUPE « SORTIE PRINCIPALE » ────────────────────────
					const bool gOut = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.out",
													 "Sortie principale", 8192u);
					const float32 gOutTop = yy;
					if (!gOut) {
						yy += NkPropGroupGap();
		
					} else {
						yy += NkGroupPad();
						const NkRect iO = NkGroupInner(rowR);
						const NkRect r{iO.x - kPad, rowR.y, iO.w + 2.f * kPad, rowR.h};
						const NkRect rr = r;
						const float32 fx = r.x + S(104.f);
						const float32 fw = rr.w - S(112.f);
						p.TextV(r.x + kPad, yy, kRowH, "Source", NkRole::TextMuted);
						{
							// La selection vit dans un STATIC : la liste
							// deroulante y ecrit apres la fin de ce bloc (voir
							// la note sur NkComboPending plus haut).
							// IL FAUT DISTINGUER DEUX CHANGEMENTS. Comparer le
							// static a la verite moteur ne suffit pas : quand
							// c'est le MOTEUR qui a bouge -- un echange
							// principale/miniature, le pave 0 -- l'ecart se lit
							// comme un choix de l'utilisateur, et on reapplique
							// l'ancienne valeur. L'echange etait ainsi annule a
							// l'image suivante (constate par Rihen). On memorise
							// donc la derniere valeur VUE du moteur : s'il a
							// change, il gagne ; sinon seul le combo parle.
							static int32 sSrcSel = 0, sSrcSeen = -999;
							const int32 cur = srcIndexOf(oSrc);
							if (cur != sSrcSeen) {
								sSrcSel = cur;
								sSrcSeen = cur;
							} else if (sSrcSel != cur && sSrcSel >= 0 && sSrcSel < nSrc) {
								oSrc = srcNodeOf(sSrcSel);
								sSrcSeen = sSrcSel;
							}
							Combo(p, hit, ws, "out.src", {fx, yy + S(2.f), fw, kRowH - S(4.f)},
								  srcNames, nullptr, nSrc, sSrcSel, combo);
						}
						yy += kRowH;
						// ── FORMAT : UNE LISTE, DONT UNE ENTREE « LIBRE » ───────
						// Huit boutons de preset prenaient deux rangees et
						// laissaient croire qu'on pouvait a la fois choisir un
						// format ET taper autre chose (Rihen). Une liste dit
						// l'etat sans ambiguite : sur un format nomme, les deux
						// champs sont GRISES et affichent ce qu'il impose ; sur
						// « Libre », ils s'editent.
						struct OPre {
								const char *n;
								int32 w, h;
						};
						static const OPre kPre[9] = {
							{"Libre", 0, 0},		  {"HD 1280 x 720", 1280, 720},
							{"Full HD 1920 x 1080", 1920, 1080},
							{"2K 2560 x 1440", 2560, 1440},
							{"4K 3840 x 2160", 3840, 2160},
							{"Carre 1080", 1080, 1080},
							{"Vertical 1080 x 1920", 1080, 1920},
							{"Cinema 2048 x 858", 2048, 858},
							{"Web 1200 x 630", 1200, 630}};
						static const char *preNames[9];
						for (int32 q = 0; q < 9; ++q)
							preNames[q] = kPre[q].n;
						// LE FORMAT NOMME SE DEDUIT de la resolution -- c'est elle
						// la verite, et taper 1920x1080 affiche « Full HD » tout
						// seul. « LIBRE » NE SE DEDUIT DE RIEN : il a sa propre
						// memoire cote moteur, sans quoi le choisir sur une
						// resolution qui vaut un format connu etait annule a
						// l'image suivante (Rihen).
						bool freeSz = demo::Demo3DHostOutFreeSize();
						int32 match = 0;
						for (int32 q = 1; q < 9; ++q)
							if (oW == kPre[q].w && oH == kPre[q].h) {
								match = q;
								break;
							}
						// Une resolution qui ne correspond a aucun format EST
						// libre : l'etat suit, il ne peut pas dire autre chose.
						if (match == 0)
							freeSz = true;
						int32 preCur = freeSz ? 0 : match;
						p.TextV(r.x + kPad, yy, kRowH, "Format", NkRole::TextMuted);
						{
							static int32 sPreSel = 0, sPreSeen = -999;
							if (preCur != sPreSeen) {
								sPreSel = preCur;
								sPreSeen = preCur;
							} else if (sPreSel != preCur) {
								if (sPreSel <= 0) {
									freeSz = true; // la resolution ne bouge pas
								} else if (sPreSel < 9) {
									freeSz = false;
									oW = kPre[sPreSel].w;
									oH = kPre[sPreSel].h;
								}
								sPreSeen = sPreSel;
								preCur = sPreSel <= 0 ? 0 : sPreSel;
							}
							Combo(p, hit, ws, "out.pre", {fx, yy + S(2.f), fw, kRowH - S(4.f)},
								  preNames, nullptr, 9, sPreSel, combo);
						}
						if (freeSz != demo::Demo3DHostOutFreeSize()) {
							demo::Demo3DHostSetOutFreeSize(freeSz);
							NkMarkDirty(st);
						}
						yy += kRowH;
						// RESOLUTION : deux champs cote a cote, comme Blender.
						// Il n'existe pas de DragInt : le glissement se fait en
						// reel puis s'arrondit -- une seule mecanique de champ
						// dans toute l'interface, donc un seul comportement a
						// apprendre.
						p.TextV(r.x + kPad, yy, kRowH, "Resolution",
								preCur == 0 ? NkRole::TextMuted : NkRole::TextMuted);
						{
							const float32 half = (fw - S(6.f)) * 0.5f;
							if (preCur == 0) {
								float32 fwv = (float32)oW, fhv = (float32)oH;
								DragFloat(p, hit, ws, in, "out.rw",
										  {fx, yy + S(3.f), half, kRowH - S(6.f)}, fwv, 1.f,
										  NkRole::AxisX, "%.0f");
								DragFloat(p, hit, ws, in, "out.rh",
										  {fx + half + S(6.f), yy + S(3.f), half, kRowH - S(6.f)},
										  fhv, 1.f, NkRole::AxisY, "%.0f");
								oW = (int32)(fwv + 0.5f);
								oH = (int32)(fhv + 0.5f);
							} else {
								// GRISES : le format nomme impose ces valeurs.
								// On les MONTRE quand meme -- un champ vide ne
								// dirait pas ce qui va sortir.
								char rb[24];
								const NkRect b1{fx, yy + S(3.f), half, kRowH - S(6.f)};
								const NkRect b2{fx + half + S(6.f), yy + S(3.f), half,
												kRowH - S(6.f)};
								p.Outline(b1, NkRole::Border, NkRole::PanelBg, 3.f);
								p.Outline(b2, NkRole::Border, NkRole::PanelBg, 3.f);
								snprintf(rb, sizeof(rb), "%d", (int)oW);
								p.TextV(b1.x + S(6.f), b1.y, b1.h, rb, NkRole::TextMuted);
								snprintf(rb, sizeof(rb), "%d", (int)oH);
								p.TextV(b2.x + S(6.f), b2.y, b2.h, rb, NkRole::TextMuted);
							}
						}
						yy += kRowH;
						// (Les huit boutons de preset ont ete remplaces par la
						// liste « Format » ci-dessus, dont chaque entree porte
						// sa taille reelle -- Rihen.)
						p.TextV(r.x + kPad, yy, kRowH, "Echelle", NkRole::TextMuted);
						{
							float32 fsc = (float32)oScale;
							DragFloat(p, hit, ws, in, "out.scl",
									  {fx, yy + S(3.f), fw, kRowH - S(6.f)}, fsc, 1.f,
									  NkRole::AccentUi, "%.0f %%");
							oScale = (int32)(fsc + 0.5f);
						}
						yy += kRowH;
						// CE QUI SERA REELLEMENT PRODUIT, en toutes lettres : la
						// resolution seule ne le dit pas des que l'echelle n'est
						// plus a 100 %.
						{
							int32 ew = 0, eh = 0;
							demo::Demo3DHostOutEffectiveSize(&ew, &eh);
							char eb[64];
							snprintf(eb, sizeof(eb), "Image produite : %d x %d px", ew, eh);
							p.TextV(r.x + kPad + S(8.f), yy, kRowH, eb, NkRole::TextMuted);
							yy += kRowH;
						}
						yy += NkGroupPad();
						PaintGroupBlock(p, rowR, gOutTop, yy);
						yy += NkPropGroupGap();
					}
					// (Le commit de ces valeurs a ete deplace APRES le groupe
					// Destination : le FORMAT et le FOND TRANSPARENT s'y editent,
					// et un commit place ici les testait AVANT qu'ils ne changent
					// -- la case « Fond transparent » ne se cochait donc jamais,
					// et le format choisi n'atteignait pas le moteur. Un commit
					// doit venir apres TOUTES les ecritures de ses variables.)

					// ── GROUPE « DESTINATION » ──────────────────────────────
					const bool gDst = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.outdst",
													 "Destination", 16384u);
					const float32 gDstTop = yy;
					if (!gDst) {
						yy += NkPropGroupGap();
					} else {
						yy += NkGroupPad();
						const NkRect iD = NkGroupInner(rowR);
						const NkRect r{iD.x - kPad, rowR.y, iD.w + 2.f * kPad, rowR.h};
						const NkRect rr = r;
						const float32 fx = r.x + S(104.f);
						const float32 fw = rr.w - S(112.f);
						p.TextV(r.x + kPad, yy, kRowH, "Dossier", NkRole::TextMuted);
						{
							char dbuf[260] = {};
							if (EditableText(p, hit, ws, in, "out.dir",
											 {fx, yy + S(2.f), fw, kRowH - S(4.f)},
											 demo::Demo3DHostOutDir(), NkRole::Text, dbuf,
											 sizeof(dbuf))) {
								demo::Demo3DHostSetOutDir(dbuf);
								NkMarkDirty(st);
							}
						}
						yy += kRowH;
						p.TextV(r.x + kPad, yy, kRowH, "Nom (rendu)", NkRole::TextMuted);
						{
							char nbuf[64] = {};
							if (EditableText(p, hit, ws, in, "out.name",
											 {fx, yy + S(2.f), fw, kRowH - S(4.f)},
											 demo::Demo3DHostOutName(), NkRole::Text, nbuf,
											 sizeof(nbuf))) {
								demo::Demo3DHostSetOutName(nbuf);
								NkMarkDirty(st);
							}
						}
						yy += kRowH;
						// LES TROIS NOMS ENSEMBLE (Rihen) : rendu, capture de la
						// vue et tutoriel partagent TOUTES les proprietes de
						// sortie -- dossier, format, qualite -- et ne different
						// que par leur nom de base. Les separer dans un groupe a
						// part laissait croire a des reglages independants.
						p.TextV(r.x + kPad, yy, kRowH, "Nom (vue)", NkRole::TextMuted);
						{
							char cb1[64] = {};
							if (EditableText(p, hit, ws, in, "out.capv",
											 {fx, yy + S(2.f), fw, kRowH - S(4.f)},
											 demo::Demo3DHostCaptureName(1), NkRole::Text, cb1,
											 sizeof(cb1))) {
								demo::Demo3DHostSetCaptureName(1, cb1);
								NkMarkDirty(st);
							}
						}
						yy += kRowH;
						p.TextV(r.x + kPad, yy, kRowH, "Nom (tutoriel)", NkRole::TextMuted);
						{
							char cb2[64] = {};
							if (EditableText(p, hit, ws, in, "out.capt",
											 {fx, yy + S(2.f), fw, kRowH - S(4.f)},
											 demo::Demo3DHostCaptureName(2), NkRole::Text, cb2,
											 sizeof(cb2))) {
								demo::Demo3DHostSetCaptureName(2, cb2);
								NkMarkDirty(st);
							}
						}
						yy += kRowH;
						// FORMAT : ceux que le moteur d'images sait REELLEMENT
						// ecrire. WebP et SVG y sont declares mais annonces
						// « non implemente » -- les proposer aurait produit des
						// fichiers vides.
						{
							const int32 nF = demo::Demo3DHostOutFormatCount();
							static char fmtBuf[12][20];
							static const char *fmtNames[12];
							for (int32 f5 = 0; f5 < nF && f5 < 12; ++f5) {
								snprintf(fmtBuf[f5], sizeof(fmtBuf[0]), "%s",
										 demo::Demo3DHostOutFormatName(f5));
								fmtNames[f5] = fmtBuf[f5];
							}
							p.TextV(r.x + kPad, yy, kRowH, "Format", NkRole::TextMuted);
							{
								static int32 sFmtSel = 0;
								if (sFmtSel != oFmt && sFmtSel >= 0 && sFmtSel < nF)
									oFmt = sFmtSel;
								else
									sFmtSel = oFmt;
								Combo(p, hit, ws, "out.fmt",
									  {fx, yy + S(2.f), fw, kRowH - S(4.f)}, fmtNames, nullptr,
									  nF < 12 ? nF : 12, sFmtSel, combo);
							}
							yy += kRowH;
							// L'EXTENSION EN CLAIR : c'est elle qui decide de
							// l'encodeur, autant la montrer.
							{
								char eb2[48];
								snprintf(eb2, sizeof(eb2), "Extension : .%s",
										 demo::Demo3DHostOutFormatExt(oFmt));
								p.TextV(r.x + kPad + S(8.f), yy, kRowH, eb2, NkRole::TextMuted);
								yy += kRowH;
							}
							// FOND TRANSPARENT : seulement si le format porte
							// l'alpha. Le proposer pour un JPEG donnerait un fond
							// NOIR sans le dire -- une case qui ment est pire
							// qu'une case absente.
							if (demo::Demo3DHostOutFormatAlpha(oFmt)) {
								const NkRect cb{r.x + kPad, yy + S(4.f), kRowH - S(8.f),
												kRowH - S(8.f)};
								const bool ovt = hit.Add("out.transp", cb);
								if (oTrans)
									p.Fill(cb, NkRole::AccentUi, 3.f);
								else
									p.Outline(cb, NkRole::Border,
											  ovt ? NkRole::PanelHeader : NkRole::PanelBg, 3.f);
								if (oTrans)
									p.IconV(cb.x + S(1.f), yy, kRowH, NkIcon::Check,
											NkRole::TextOnAccent, 11.f);
								p.Clip({cb.x + cb.w + S(8.f), yy, rr.w - S(60.f), kRowH});
								p.TextV(cb.x + cb.w + S(8.f), yy, kRowH, "Fond transparent");
								p.Unclip();
								if (hit.Clicked("out.transp"))
									oTrans = !oTrans;
								yy += kRowH;
								if (oTrans) {
									yy += p.TextWrap(r.x + kPad + S(24.f), yy,
													 rr.w - 2.f * kPad - S(24.f),
													 "Le ciel et le fond sont coupes : seule la "
													 "scene est ecrite.",
													 NkRole::TextMuted);
									yy += S(4.f);
								}
							}
							// QUALITE : seulement pour un format qui perd de
							// l'information. L'afficher pour le PNG ferait
							// croire qu'elle y change quelque chose.
							if (demo::Demo3DHostOutFormatLossy(oFmt)) {
								p.TextV(r.x + kPad, yy, kRowH, "Qualite", NkRole::TextMuted);
								float32 q5 = (float32)demo::Demo3DHostOutQuality();
								if (DragFloat(p, hit, ws, in, "out.qual",
											  {fx, yy + S(3.f), fw, kRowH - S(6.f)}, q5, 1.f,
											  NkRole::AccentUi, "%.0f")) {
									demo::Demo3DHostSetOutQuality((int32)(q5 + 0.5f));
									NkMarkDirty(st);
								}
								yy += kRowH;
							}
						}
						{
							char lb[300];
							const char *lp = demo::Demo3DHostOutLastPath();
							if (lp && lp[0]) {
								snprintf(lb, sizeof(lb), "Dernier : %s%s", lp,
										 demo::Demo3DHostOutLastOk() ? "" : "  (ECHEC)");
								p.Clip({r.x + kPad, yy, rr.w - 2.f * kPad, kRowH});
								p.TextV(r.x + kPad + S(8.f), yy, kRowH, lb, NkRole::TextMuted);
								p.Unclip();
								yy += kRowH;
							}
						}
						yy += NkGroupPad();
						PaintGroupBlock(p, rowR, gDstTop, yy);
						yy += NkPropGroupGap();
					}
					// COMMIT APRES TOUTES LES ECRITURES : source, resolution et
					// echelle viennent de « Sortie principale », format, qualite
					// et fond transparent de « Destination ». Les commiter entre
					// les deux groupes ne voyait que la moitie des changements.
					if (oSrc != oSrc0 || oW != oW0 || oH != oH0 || oScale != oScale0 ||
						oFmt != oFmt0 || oTrans != oTrans0) {
						demo::Demo3DHostSetOutMain(oSrc, oW, oH, oScale, oFmt, oTrans);
						NkMarkDirty(st);
					}

					// ── GROUPE « TYPES DE RENDU » (Rihen) ───────────────────
					// Chaque type coche produit SON image, incrustations
					// comprises, et le fichier porte son suffixe. Rien de coche
					// = le mode courant de la vue, et lui seul : cocher ne doit
					// pas etre un prealable pour rendre ce qu'on a sous les
					// yeux.
					const bool gMod = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.outmod",
													 "Types de rendu", 65536u);
					const float32 gModTop = yy;
					if (!gMod) {
						yy += NkPropGroupGap();
					} else {
						yy += NkGroupPad();
						const NkRect iM = NkGroupInner(rowR);
						const NkRect r{iM.x - kPad, rowR.y, iM.w + 2.f * kPad, rowR.h};
						const NkRect rr = r;
						const int32 nM = demo::Demo3DHostOutModeCount();
						int32 mask = demo::Demo3DHostOutModes();
						const int32 mask0 = mask;
						for (int32 m5 = 0; m5 < nM; ++m5) {
							char mk[24];
							snprintf(mk, sizeof(mk), "out.mode.%d", m5);
							const bool on5 = (mask & (1 << m5)) != 0;
							const NkRect cb{r.x + kPad, yy + S(4.f), kRowH - S(8.f),
											kRowH - S(8.f)};
							const bool ovm = hit.Add(mk, cb);
							if (on5)
								p.Fill(cb, NkRole::AccentUi, 3.f);
							else
								p.Outline(cb, NkRole::Border,
										  ovm ? NkRole::PanelHeader : NkRole::PanelBg, 3.f);
							if (on5)
								p.IconV(cb.x + S(1.f), yy, kRowH, NkIcon::Check,
										NkRole::TextOnAccent, 11.f);
							p.TextV(cb.x + cb.w + S(8.f), yy, kRowH,
									demo::Demo3DHostOutModeName(m5));
							if (hit.Clicked(mk))
								mask ^= (1 << m5);
							yy += kRowH;
						}
						if (mask != mask0) {
							demo::Demo3DHostSetOutModes(mask);
							NkMarkDirty(st);
						}
						{
							int32 nOnM = 0;
							for (int32 m5 = 0; m5 < nM; ++m5)
								if (mask & (1 << m5))
									++nOnM;
							char mb[96];
							if (nOnM == 0)
								snprintf(mb, sizeof(mb),
										 "Aucun coche : le mode courant de la vue, une image.");
							else
								snprintf(mb, sizeof(mb), "%d image(s), une par type coche.",
										 (int)nOnM);
							yy += S(3.f);
							yy += p.TextWrap(r.x + kPad, yy, rr.w - 2.f * kPad, mb,
											 NkRole::TextMuted);
							yy += S(6.f);
						}
						yy += NkGroupPad();
						PaintGroupBlock(p, rowR, gModTop, yy);
						yy += NkPropGroupGap();
					}

					// ── GROUPE « AIDES DANS LE RENDU » (Rihen) ──────────────
					// Coupees par defaut : une lumiere n'existe dans une image
					// que par son effet, pas par son symbole. Mais on veut
					// parfois montrer justement la grille ou le cadre d'une
					// camera -- une planche pedagogique, une explication. Le
					// masquage est donc une OPTION.
					// « Tutoriel » n'y est pas soumis : il photographie la
					// fenetre telle qu'elle est, c'est tout son propos.
					const bool gAid = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.outaid",
													 "Aides dans le rendu", 524288u);
					const float32 gAidTop = yy;
					if (!gAid) {
						yy += NkPropGroupGap();
					} else {
						yy += NkGroupPad();
						const NkRect iA = NkGroupInner(rowR);
						const NkRect r{iA.x - kPad, rowR.y, iA.w + 2.f * kPad, rowR.h};
						const NkRect rr = r;
						const int32 nA = demo::Demo3DHostOutAidCount();
						int32 aids = demo::Demo3DHostOutAids();
						const int32 aids0 = aids;
						for (int32 a5 = 0; a5 < nA; ++a5) {
							char ak[24];
							snprintf(ak, sizeof(ak), "out.aid.%d", a5);
							const bool on6 = (aids & (1 << a5)) != 0;
							const NkRect cb{r.x + kPad, yy + S(4.f), kRowH - S(8.f),
											kRowH - S(8.f)};
							const bool ova = hit.Add(ak, cb);
							if (on6)
								p.Fill(cb, NkRole::AccentUi, 3.f);
							else
								p.Outline(cb, NkRole::Border,
										  ova ? NkRole::PanelHeader : NkRole::PanelBg, 3.f);
							if (on6)
								p.IconV(cb.x + S(1.f), yy, kRowH, NkIcon::Check,
										NkRole::TextOnAccent, 11.f);
							p.TextV(cb.x + cb.w + S(8.f), yy, kRowH,
									demo::Demo3DHostOutAidName(a5));
							if (hit.Clicked(ak))
								aids ^= (1 << a5);
							yy += kRowH;
						}
						if (aids != aids0) {
							demo::Demo3DHostSetOutAids(aids);
							NkMarkDirty(st);
						}
						// BLOC DE TEXTE QUI VA A LA LIGNE : il epouse la largeur du
						// groupe au lieu de deborder, et sa hauteur est celle qu'il
						// occupe reellement -- retrecir le panneau ajoute une ligne
						// au lieu de tronquer la phrase.
						yy += S(4.f);
						yy += p.TextWrap(r.x + kPad, yy, rr.w - 2.f * kPad,
										 "Decochee, l'aide est coupee au rendu. Cochee, elle "
										 "laisse passer ce que l'affichage montre.",
										 NkRole::TextMuted);
						yy += S(4.f);
						yy += NkGroupPad();
						PaintGroupBlock(p, rowR, gAidTop, yy);
						yy += NkPropGroupGap();
					}

					// ── GROUPE « VIDEO » ────────────────────────────────────
					// La configuration se REGLE et se conserve des maintenant ;
					// le rendu viendra plus tard (decision de Rihen). Rien ici
					// ne pretend l'executer : pas de bouton « Rendre la video »
					// qui ne rendrait rien -- une commande factice est ce que
					// le principe « fonctionnalites a la naissance » interdit.
					const bool gVid = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.outvid",
													 "Video", 131072u);
					const float32 gVidTop = yy;
					if (!gVid) {
						yy += NkPropGroupGap();
					} else {
						yy += NkGroupPad();
						const NkRect iV = NkGroupInner(rowR);
						const NkRect r{iV.x - kPad, rowR.y, iV.w + 2.f * kPad, rowR.h};
						const NkRect rr = r;
						const float32 fx3 = r.x + S(104.f);
						const float32 fw3 = rr.w - S(112.f);
						bool vOn = false;
						int32 vFps = 25, vA = 1, vB = 250, vCod = 0;
						demo::Demo3DHostOutVideo(&vOn, &vFps, &vA, &vB, &vCod);
						const bool vOn0 = vOn;
						const int32 f0v = vFps, a0v = vA, b0v = vB, c0v = vCod;
						p.TextV(r.x + kPad, yy, kRowH, "Images / s", NkRole::TextMuted);
						{
							float32 ff = (float32)vFps;
							DragFloat(p, hit, ws, in, "out.fps",
									  {fx3, yy + S(3.f), fw3, kRowH - S(6.f)}, ff, 0.5f,
									  NkRole::AccentUi, "%.0f");
							vFps = (int32)(ff + 0.5f);
						}
						yy += kRowH;
						p.TextV(r.x + kPad, yy, kRowH, "Plage", NkRole::TextMuted);
						{
							const float32 half = (fw3 - S(6.f)) * 0.5f;
							float32 fa = (float32)vA, fb = (float32)vB;
							DragFloat(p, hit, ws, in, "out.fa",
									  {fx3, yy + S(3.f), half, kRowH - S(6.f)}, fa, 1.f,
									  NkRole::AxisX, "%.0f");
							DragFloat(p, hit, ws, in, "out.fb",
									  {fx3 + half + S(6.f), yy + S(3.f), half, kRowH - S(6.f)},
									  fb, 1.f, NkRole::AxisY, "%.0f");
							vA = (int32)(fa + 0.5f);
							vB = (int32)(fb + 0.5f);
						}
						yy += kRowH;
						// ── CONTENEUR PUIS CODEC (Rihen, comme Blender) ─────
						// Une seule liste melangeait les deux et cachait que le
						// meme MJPEG sert dans AVI et dans MOV, et que l'AVI sait
						// aussi ecrire du non compresse. Le conteneur dit le
						// FICHIER, le codec dit COMMENT les images y entrent.
						p.TextV(r.x + kPad, yy, kRowH, "Conteneur", NkRole::TextMuted);
						{
							static const char *sCont[8] = {};
							static int32 sContN = 0;
							if (sContN == 0) {
								sContN = demo::Demo3DHostOutVidContCount();
								if (sContN > 8)
									sContN = 8;
								for (int32 c = 0; c < sContN; ++c)
									sCont[c] = demo::Demo3DHostOutVidContName(c);
							}
							static int32 sContSel = 0;
							if (sContSel != vCod && sContSel >= 0 && sContSel < sContN)
								vCod = sContSel;
							else
								sContSel = vCod;
							Combo(p, hit, ws, "out.cont",
								  {fx3, yy + S(2.f), fw3, kRowH - S(4.f)}, sCont, nullptr,
								  sContN, sContSel, combo);
							yy += kRowH;
						}
						// LE CODEC DEPEND DU CONTENEUR : proposer H.264 sous AVI
						// promettrait un fichier que NKMedia ne sait pas ecrire.
						// La liste se reconstruit donc a chaque changement.
						{
							p.TextV(r.x + kPad, yy, kRowH, "Codec", NkRole::TextMuted);
							static const char *sCod[6] = {};
							static int32 sCodN = 0, sCodFor = -1;
							if (sCodFor != vCod) {
								sCodFor = vCod;
								sCodN = demo::Demo3DHostOutVidCodCount(vCod);
								if (sCodN > 6)
									sCodN = 6;
								for (int32 k = 0; k < sCodN; ++k)
									sCod[k] = demo::Demo3DHostOutVidCodName(vCod, k);
							}
							const int32 cod0 = demo::Demo3DHostOutVidCod();
							static int32 sCodSel = 0;
							if (sCodSel != cod0 && sCodSel >= 0 && sCodSel < sCodN) {
								demo::Demo3DHostSetOutVidCod(sCodSel);
								NkMarkDirty(st);
							} else {
								sCodSel = cod0;
							}
							Combo(p, hit, ws, "out.codec",
								  {fx3, yy + S(2.f), fw3, kRowH - S(4.f)}, sCod, nullptr, sCodN,
								  sCodSel, combo);
							yy += kRowH;
						}
						// QUALITE PROPRE A LA VIDEO (Rihen) : elle etait partagee
						// avec l'image fixe, si bien que soigner un rendu JPEG
						// alourdissait toutes les prises -- deux usages, deux
						// reglages. Sans objet pour la suite d'images PNG, qui est
						// sans perte : on ne montre pas un levier sans effet.
						if (vCod != 0) {
							p.TextV(r.x + kPad, yy, kRowH, "Qualite video", NkRole::TextMuted);
							float32 vq = (float32)demo::Demo3DHostOutVideoQuality();
							const float32 vq0 = vq;
							DragFloat(p, hit, ws, in, "out.vq",
									  {fx3, yy + S(3.f), fw3, kRowH - S(6.f)}, vq, 0.5f,
									  NkRole::AccentUi, "%.0f");
							if (vq < 1.f)
								vq = 1.f;
							if (vq > 100.f)
								vq = 100.f;
							if ((int32)(vq + 0.5f) != (int32)(vq0 + 0.5f)) {
								demo::Demo3DHostSetOutVideoQuality((int32)(vq + 0.5f));
								NkMarkDirty(st);
							}
							yy += kRowH;
							yy += p.TextWrap(
								r.x + kPad, yy, rr.w - 2.f * kPad,
								vCod == 4
									? "Convertie en QP H.264 (borne 12-48). Le MP4 est encode "
									  "APRES la prise : on filme en images, l'encodeur travaille "
									  "ensuite -- sans quoi la video sortait onze fois trop "
									  "rapide."
									: "Compression image par image : chaque image du fichier est "
									  "independante des autres.",
								NkRole::TextMuted);
							yy += S(6.f);
						}
						// ── LE CURSEUR DANS LA VIDEO DE TUTORIEL (Rihen) ────
						// La capture de fenetre de l'OS ne contient PAS le
						// pointeur : sans ce dessin, la video montre des menus
						// qui s'ouvrent tout seuls.
						{
							const bool cu = demo::Demo3DHostOutCursor();
							const NkRect cb{r.x + kPad, yy + S(4.f), kRowH - S(8.f),
											kRowH - S(8.f)};
							const bool ovc = hit.Add("out.cursor", cb);
							if (cu)
								p.Fill(cb, NkRole::AccentUi, 3.f);
							else
								p.Outline(cb, NkRole::Border,
										  ovc ? NkRole::PanelHeader : NkRole::PanelBg, 3.f);
							if (cu)
								p.IconV(cb.x + S(1.f), yy, kRowH, NkIcon::Check,
										NkRole::TextOnAccent, 11.f);
							p.Clip({cb.x + cb.w + S(8.f), yy, rr.w - S(60.f), kRowH});
							p.TextV(cb.x + cb.w + S(8.f), yy, kRowH,
									"Curseur et sa trace (tutoriel)");
							p.Unclip();
							if (hit.Clicked("out.cursor")) {
								demo::Demo3DHostSetOutCursor(!cu);
								NkMarkDirty(st);
							}
							yy += kRowH;
							yy += p.TextWrap(r.x + kPad + S(24.f), yy,
											 rr.w - 2.f * kPad - S(24.f),
											 "Ne concerne que la video de la fenetre entiere : "
											 "une image fixe n'a pas de trajectoire a montrer.",
											 NkRole::TextMuted);
							yy += S(6.f);
						}
						// ── CONSERVER LES IMAGES QOI (Rihen) ────────────────
						// Decoche par defaut : le dossier nom_qoi_numero
						// s'efface une fois la video construite. Coche, il
						// reste -- source de montage sans perte.
						{
							const bool kq = demo::Demo3DHostOutKeepQoi();
							const NkRect cb{r.x + kPad, yy + S(4.f), kRowH - S(8.f),
											kRowH - S(8.f)};
							const bool ovq = hit.Add("out.keepqoi", cb);
							if (kq)
								p.Fill(cb, NkRole::AccentUi, 3.f);
							else
								p.Outline(cb, NkRole::Border,
										  ovq ? NkRole::PanelHeader : NkRole::PanelBg, 3.f);
							if (kq)
								p.IconV(cb.x + S(1.f), yy, kRowH, NkIcon::Check,
										NkRole::TextOnAccent, 11.f);
							p.Clip({cb.x + cb.w + S(8.f), yy, rr.w - S(60.f), kRowH});
							p.TextV(cb.x + cb.w + S(8.f), yy, kRowH,
									"Conserver les images QOI");
							p.Unclip();
							if (hit.Clicked("out.keepqoi")) {
								demo::Demo3DHostSetOutKeepQoi(!kq);
								NkMarkDirty(st);
							}
							yy += kRowH;
							yy += p.TextWrap(r.x + kPad + S(24.f), yy,
											 rr.w - 2.f * kPad - S(24.f),
											 "La prise filme en images QOI sans perte, la video "
											 "se construit a l'arret. Cochee, le dossier "
											 "nom_qoi_numero reste apres l'encodage.",
											 NkRole::TextMuted);
							yy += S(6.f);
						}
						// ── ENREGISTRER LA SESSION (Rihen) ──────────────────
						// On filme ce qui SE PASSE dans la vue. Pas de plage
						// d'images : sans timeline, elle produirait la meme
						// image repetee -- le rendu d'animation viendra pour
						// les captures quand la timeline existera.
						{
							const bool rec = demo::Demo3DHostRecActive();
							const float32 bw3 = (rr.w - 2.f * kPad - S(6.f)) * 0.5f;
							if (!rec) {
								// EN SATURATION : le choix appartient a
								// l'utilisateur (Rihen). Reglable seulement hors
								// prise -- la file est dimensionnee au demarrage.
								{
									const bool gr = demo::Demo3DHostRecGrow();
									const NkRect cb{r.x + kPad, yy + S(4.f), kRowH - S(8.f),
													kRowH - S(8.f)};
									const bool ovg = hit.Add("out.rec.grow", cb);
									if (gr)
										p.Fill(cb, NkRole::AccentUi, 3.f);
									else
										p.Outline(cb, NkRole::Border,
												  ovg ? NkRole::PanelHeader : NkRole::PanelBg,
												  3.f);
									if (gr)
										p.IconV(cb.x + S(1.f), yy, kRowH, NkIcon::Check,
												NkRole::TextOnAccent, 11.f);
									p.Clip({cb.x + cb.w + S(8.f), yy, rr.w - S(60.f), kRowH});
									p.TextV(cb.x + cb.w + S(8.f), yy, kRowH,
											"Ne perdre aucune image");
									p.Unclip();
									if (hit.Clicked("out.rec.grow")) {
										demo::Demo3DHostSetRecGrow(!gr);
										NkMarkDirty(st);
									}
									yy += kRowH;
									yy += p.TextWrap(r.x + kPad + S(24.f), yy,
													 rr.w - 2.f * kPad - S(24.f),
													 gr ? "L'application attend l'encodeur quand "
														  "il prend du retard."
														: "Les images en trop sont sautees : "
														  "l'application reste fluide.",
													 NkRole::TextMuted);
									yy += S(6.f);
								}
								if (Button("out.rec.go", yy, "Enregistrer la vue",
										   r.x + kPad, rr.w - 2.f * kPad))
									demo::Demo3DHostRecStart();
								yy += kRowH;
							} else {
								// PAUSE / REPRENDRE : on suspend sans fermer le
								// fichier -- le montage n'aura aucune trace du
								// temps arrete (Rihen).
								const bool pz = demo::Demo3DHostRecPaused();
								if (Button("out.rec.pause", yy,
										   pz ? "Reprendre" : "Pause", r.x + kPad, bw3))
									demo::Demo3DHostRecPause(!pz);
								if (Button("out.rec.stop", yy, "Arreter et garder",
										   r.x + kPad + bw3 + S(6.f), bw3))
									demo::Demo3DHostRecStop(true);
								yy += kRowH;
								// ABANDONNER : ferme ET efface. Sans lui, une
								// prise ratee laisserait un fichier a supprimer
								// a la main, et on hesiterait a enregistrer.
								if (Button("out.rec.drop", yy, "Abandonner (rien n'est garde)",
										   r.x + kPad, rr.w - 2.f * kPad))
									demo::Demo3DHostRecStop(false);
								yy += kRowH;
								const int32 nf = demo::Demo3DHostRecFrames();
								const int32 nd = demo::Demo3DHostRecDropped();
								const float32 secs =
									(vFps > 0) ? (float32)nf / (float32)vFps : 0.f;
								char rb2[128];
								if (nd > 0)
									snprintf(rb2, sizeof(rb2),
											 "%d images -- %.1f s  (%d sautee(s))", (int)nf,
											 (double)secs, (int)nd);
								else
									snprintf(rb2, sizeof(rb2), "%d images -- %.1f s%s", (int)nf,
											 (double)secs, pz ? "  [en pause]" : "");
								p.TextV(r.x + kPad + S(8.f), yy, kRowH, rb2,
										pz ? NkRole::TextMuted : NkRole::AccentUi);
								yy += kRowH;
								p.Clip({r.x + kPad, yy, rr.w - 2.f * kPad, kRowH});
								p.TextV(r.x + kPad + S(8.f), yy, kRowH,
										demo::Demo3DHostRecPath(), NkRole::TextMuted);
								p.Unclip();
								yy += kRowH;
							}
						}
						// ── CADENCE DE CAPTURE (Rihen) ──────────────────────
						// Une image fixe coute trois passes ; une video ne peut
						// pas se le permettre, et c'est la LECTURE des pixels qui
						// coute -- elle synchronise le processeur sur la carte.
						// Chacun de ces leviers a sa contrepartie, d'ou des
						// cases plutot qu'un comportement impose.
						yy += S(4.f);
						{
							const int32 nF2 = demo::Demo3DHostOutFastCount();
							int32 fm = demo::Demo3DHostOutFastMask();
							const int32 fm0 = fm;
							for (int32 a6 = 0; a6 < nF2; ++a6) {
								char fk2[24];
								snprintf(fk2, sizeof(fk2), "out.fast.%d", a6);
								const bool on7 = (fm & (1 << a6)) != 0;
								const NkRect cb{r.x + kPad, yy + S(4.f), kRowH - S(8.f),
												kRowH - S(8.f)};
								const bool ovf = hit.Add(fk2, cb);
								if (on7)
									p.Fill(cb, NkRole::AccentUi, 3.f);
								else
									p.Outline(cb, NkRole::Border,
											  ovf ? NkRole::PanelHeader : NkRole::PanelBg, 3.f);
								if (on7)
									p.IconV(cb.x + S(1.f), yy, kRowH, NkIcon::Check,
											NkRole::TextOnAccent, 11.f);
								p.Clip({cb.x + cb.w + S(8.f), yy, rr.w - S(60.f), kRowH});
								p.TextV(cb.x + cb.w + S(8.f), yy, kRowH,
										demo::Demo3DHostOutFastName(a6));
								p.Unclip();
								if (hit.Clicked(fk2))
									fm ^= (1 << a6);
								yy += kRowH;
							}
							if (fm != fm0) {
								demo::Demo3DHostSetOutFastMask(fm);
								NkMarkDirty(st);
							}
						}
						yy += S(3.f);
						yy += p.TextWrap(r.x + kPad, yy, rr.w - 2.f * kPad,
										 "L'enregistrement filme la vue a sa resolution, sans "
										 "redimensionner sa cible : une seule lecture par image. "
										 "La plage ci-dessus servira au rendu d'animation, quand "
										 "la timeline existera.",
										 NkRole::TextMuted);
						yy += S(6.f);
						if (vOn != vOn0 || vFps != f0v || vA != a0v || vB != b0v || vCod != c0v) {
							demo::Demo3DHostSetOutVideo(vOn, vFps, vA, vB, vCod);
							NkMarkDirty(st);
						}
						yy += NkGroupPad();
						PaintGroupBlock(p, rowR, gVidTop, yy);
						yy += NkPropGroupGap();
					}

					// ── GROUPE « INCRUSTATIONS » ────────────────────────────
					// Les cibles secondaires posees SUR la principale. Position
					// et taille sont des FRACTIONS : changer la resolution ne
					// deplace donc aucune incrustation.
					const bool gIns = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.outins",
													 "Incrustations", 32768u);
					const float32 gInsTop = yy;
					if (!gIns) {
						yy += NkPropGroupGap();
					} else {
						yy += NkGroupPad();
						const NkRect iI = NkGroupInner(rowR);
						const NkRect r{iI.x - kPad, rowR.y, iI.w + 2.f * kPad, rowR.h};
						const NkRect rr = r;
						const int32 maxIns = demo::Demo3DHostOutInsetMax();
						int32 nUsed = 0;
						for (int32 k5 = 0; k5 < maxIns; ++k5)
							if (demo::Demo3DHostOutInset(k5, nullptr, nullptr, nullptr, nullptr,
														 nullptr, nullptr, nullptr))
								++nUsed;
						if (nUsed == 0) {
							yy += S(3.f);
							yy += p.TextWrap(r.x + kPad, yy, rr.w - 2.f * kPad,
											 "Aucune incrustation : l'image sort telle quelle.",
											 NkRole::TextMuted);
							yy += S(6.f);
						}
						// Formes, nommees par le moteur : une seule liste, donc
						// aucun risque de dire « Cercle » et d'en rendre un autre.
						const int32 nShape = demo::Demo3DHostOutInsetShapeCount();
						static char shpBuf[8][24];
						static const char *shpNames[8];
						// Une selection PAR incrustation, persistante : la liste
						// deroulante y ecrit apres la fin de la boucle. Et une
						// memoire de ce qu'on a VU du moteur, pour ne pas
						// prendre un echange principale/miniature pour un choix
						// de l'utilisateur -- voir la note du combo de source.
						static int32 sInsSrcSel[8] = {};
						static int32 sInsSrcSeen[8] = {-999, -999, -999, -999,
													   -999, -999, -999, -999};
						static int32 sInsShpSel[8] = {};
						static int32 sInsShpSeen[8] = {-999, -999, -999, -999,
													   -999, -999, -999, -999};
						for (int32 s5 = 0; s5 < nShape && s5 < 8; ++s5) {
							snprintf(shpBuf[s5], sizeof(shpBuf[0]), "%s",
									 demo::Demo3DHostOutInsetShapeName(s5));
							shpNames[s5] = shpBuf[s5];
						}
						for (int32 k5 = 0; k5 < maxIns; ++k5) {
							int32 iSrc = -1, iShape = 0;
							float32 iXY[2] = {0.f, 0.f}, iSz[2] = {0.25f, 0.25f}, iBrd = 2.f,
									iCol[3] = {1.f, 1.f, 1.f}, iOpa = 1.f;
							if (!demo::Demo3DHostOutInset(k5, &iSrc, &iShape, iXY, iSz, &iBrd,
														  iCol, &iOpa))
								continue;
							const int32 s0 = iSrc, sh0 = iShape;
							const float32 x0 = iXY[0], y0 = iXY[1], sz0 = iSz[0], sz1 = iSz[1],
										  b0 = iBrd, o0 = iOpa;
							char key2[32], lbl[40];
							snprintf(lbl, sizeof(lbl), "Incrustation %d", (int)(k5 + 1));
							// En-tete de l'incrustation + sa suppression.
							{
								const NkRect hr{r.x + kPad, yy, rr.w - 2.f * kPad, kRowH};
								p.Fill(hr, NkRole::PanelHeader, 3.f);
								p.TextV(hr.x + S(8.f), yy, kRowH, lbl);
								snprintf(key2, sizeof(key2), "out.ins.del.%d", k5);
								const NkRect db{hr.x + hr.w - S(24.f), yy + S(3.f), S(20.f),
												kRowH - S(6.f)};
								const bool ovd = hit.Add(key2, db);
								HoverFill(p, db, ovd, 3.f);
								p.IconV(db.x + S(3.f), yy, kRowH, NkIcon::Trash,
										ovd ? NkRole::AxisX : NkRole::TextMuted, 12.f);
								if (hit.Clicked(key2)) {
									demo::Demo3DHostOutInsetDelete(k5);
									yy += kRowH;
									continue;
								}
								yy += kRowH;
							}
							const float32 fx2 = r.x + S(104.f);
							const float32 fw2 = rr.w - S(112.f);
							p.TextV(r.x + kPad + S(8.f), yy, kRowH, "Source", NkRole::TextMuted);
							snprintf(key2, sizeof(key2), "out.ins.src.%d", k5);
							if (k5 < 8) {
								// Le MOTEUR gagne quand c'est lui qui a bouge --
								// un echange principale/miniature, par exemple --
								// sinon l'ecart se lisait comme un choix de
								// l'utilisateur et l'echange etait annule a
								// l'image suivante (Rihen). Voir la note du combo
								// de source principale. La liste, elle, exclut la
								// source principale.
								const int32 cur2 = insIndexOf(iSrc);
								if (cur2 != sInsSrcSeen[k5]) {
									sInsSrcSel[k5] = cur2;
									sInsSrcSeen[k5] = cur2;
								} else if (sInsSrcSel[k5] != cur2 && sInsSrcSel[k5] >= 0 &&
										   sInsSrcSel[k5] < nIns) {
									iSrc = insNodeOf(sInsSrcSel[k5]);
									sInsSrcSeen[k5] = sInsSrcSel[k5];
								}
								Combo(p, hit, ws, key2, {fx2, yy + S(2.f), fw2, kRowH - S(4.f)},
									  insNames, nullptr, nIns, sInsSrcSel[k5], combo);
							}
							yy += kRowH;
							p.TextV(r.x + kPad + S(8.f), yy, kRowH, "Forme", NkRole::TextMuted);
							snprintf(key2, sizeof(key2), "out.ins.shp.%d", k5);
							if (k5 < 8) {
								if (iShape != sInsShpSeen[k5]) {
									sInsShpSel[k5] = iShape;
									sInsShpSeen[k5] = iShape;
								} else if (sInsShpSel[k5] != iShape && sInsShpSel[k5] >= 0 &&
										   sInsShpSel[k5] < nShape) {
									iShape = sInsShpSel[k5];
									sInsShpSeen[k5] = sInsShpSel[k5];
								}
								Combo(p, hit, ws, key2, {fx2, yy + S(2.f), fw2, kRowH - S(4.f)},
									  shpNames, nullptr, nShape < 8 ? nShape : 8, sInsShpSel[k5],
									  combo);
							}
							yy += kRowH;
							p.TextV(r.x + kPad + S(8.f), yy, kRowH, "Position", NkRole::TextMuted);
							{
								const float32 half = (fw2 - S(6.f)) * 0.5f;
								snprintf(key2, sizeof(key2), "out.ins.x.%d", k5);
								DragFloat(p, hit, ws, in, key2,
										  {fx2, yy + S(3.f), half, kRowH - S(6.f)}, iXY[0], 0.002f,
										  NkRole::AxisX, "%.2f");
								snprintf(key2, sizeof(key2), "out.ins.y.%d", k5);
								DragFloat(p, hit, ws, in, key2,
										  {fx2 + half + S(6.f), yy + S(3.f), half, kRowH - S(6.f)},
										  iXY[1], 0.002f, NkRole::AxisY, "%.2f");
							}
							yy += kRowH;
							// DIMENSIONS PROPRES A LA FORME (Rihen) : un carre a un
							// COTE, un cercle un DIAMETRE, les autres une largeur
							// et une hauteur. Afficher deux champs pour un cercle
							// reviendrait a en montrer un qui ne sert a rien.
							{
								const int32 nDim = nk3d::NkInsetDimCount(iShape);
								p.TextV(r.x + kPad + S(8.f), yy, kRowH,
										nk3d::NkInsetDimName(iShape, 0), NkRole::TextMuted);
								if (nDim == 1) {
									snprintf(key2, sizeof(key2), "out.ins.sz.%d", k5);
									DragFloat(p, hit, ws, in, key2,
											  {fx2, yy + S(3.f), fw2, kRowH - S(6.f)}, iSz[0],
											  0.002f, NkRole::AccentUi, "%.2f");
									iSz[1] = iSz[0];
									yy += kRowH;
								} else {
									snprintf(key2, sizeof(key2), "out.ins.sz.%d", k5);
									DragFloat(p, hit, ws, in, key2,
											  {fx2, yy + S(3.f), fw2, kRowH - S(6.f)}, iSz[0],
											  0.002f, NkRole::AxisX, "%.2f");
									yy += kRowH;
									p.TextV(r.x + kPad + S(8.f), yy, kRowH,
											nk3d::NkInsetDimName(iShape, 1), NkRole::TextMuted);
									snprintf(key2, sizeof(key2), "out.ins.szh.%d", k5);
									DragFloat(p, hit, ws, in, key2,
											  {fx2, yy + S(3.f), fw2, kRowH - S(6.f)}, iSz[1],
											  0.002f, NkRole::AxisY, "%.2f");
									yy += kRowH;
								}
							}
							p.TextV(r.x + kPad + S(8.f), yy, kRowH, "Lisere", NkRole::TextMuted);
							snprintf(key2, sizeof(key2), "out.ins.br.%d", k5);
							DragFloat(p, hit, ws, in, key2,
									  {fx2, yy + S(3.f), fw2, kRowH - S(6.f)}, iBrd, 0.1f,
									  NkRole::AccentUi, "%.0f px");
							yy += kRowH;
							// COULEUR DU LISERE : la nuance cliquable du projet, qui
							// ouvre le vrai selecteur. Une pastille qui se contente
							// d'afficher une couleur sans pouvoir la changer serait
							// une commande factice -- exactement ce que le principe
							// « fonctionnalites a la naissance » interdit.
							{
								snprintf(key2, sizeof(key2), "out.ins.bc.%d", k5);
								bool cch = false;
								const NkRect crow{r.x + kPad + S(8.f), yy,
												  rr.w - 2.f * kPad - S(8.f), kRowH};
								yy += PaintColorRow(p, hit, ws, in, st, crow, yy, "Couleur",
													key2, iCol, &cch);
								if (cch) {
									demo::Demo3DHostSetOutInset(k5, iSrc, iShape, iXY, iSz, iBrd,
																iCol, iOpa);
									NkMarkDirty(st);
								}
							}
							p.TextV(r.x + kPad + S(8.f), yy, kRowH, "Opacite", NkRole::TextMuted);
							snprintf(key2, sizeof(key2), "out.ins.op.%d", k5);
							DragFloat(p, hit, ws, in, key2,
									  {fx2, yy + S(3.f), fw2, kRowH - S(6.f)}, iOpa, 0.005f,
									  NkRole::AccentUi, "%.2f");
							yy += kRowH;
							// FICHIER PROPRE : la miniature est AUSSI ecrite
							// seule, telle quelle -- sans masque de forme ni
							// lisere, qui appartiennent a la composition.
							{
								snprintf(key2, sizeof(key2), "out.ins.own.%d", k5);
								const bool own = demo::Demo3DHostOutInsetOwnFile(k5);
								const NkRect cb{r.x + kPad + S(8.f), yy + S(4.f), kRowH - S(8.f),
												kRowH - S(8.f)};
								const bool ovo = hit.Add(key2, cb);
								if (own)
									p.Fill(cb, NkRole::AccentUi, 3.f);
								else
									p.Outline(cb, NkRole::Border,
											  ovo ? NkRole::PanelHeader : NkRole::PanelBg, 3.f);
								if (own)
									p.IconV(cb.x + S(1.f), yy, kRowH, NkIcon::Check,
											NkRole::TextOnAccent, 11.f);
								p.Clip({cb.x + cb.w + S(8.f), yy, rr.w - S(60.f), kRowH});
								p.TextV(cb.x + cb.w + S(8.f), yy, kRowH, "Aussi en fichier propre");
								p.Unclip();
								if (hit.Clicked(key2)) {
									demo::Demo3DHostSetOutInsetOwnFile(k5, !own);
									NkMarkDirty(st);
								}
								yy += kRowH;
								// SOUS-OPTION : elle n'existe que si le fichier
								// propre est demande (Rihen). Une case qui ne
								// gouverne rien n'a pas a etre montree.
								if (own) {
									snprintf(key2, sizeof(key2), "out.ins.shp2.%d", k5);
									const bool shp = demo::Demo3DHostOutInsetOwnShaped(k5);
									const NkRect cb2{r.x + kPad + S(26.f), yy + S(4.f),
													 kRowH - S(8.f), kRowH - S(8.f)};
									const bool ovs = hit.Add(key2, cb2);
									if (shp)
										p.Fill(cb2, NkRole::AccentUi, 3.f);
									else
										p.Outline(cb2, NkRole::Border,
												  ovs ? NkRole::PanelHeader : NkRole::PanelBg,
												  3.f);
									if (shp)
										p.IconV(cb2.x + S(1.f), yy, kRowH, NkIcon::Check,
												NkRole::TextOnAccent, 11.f);
									p.Clip({cb2.x + cb2.w + S(8.f), yy, rr.w - S(80.f), kRowH});
									p.TextV(cb2.x + cb2.w + S(8.f), yy, kRowH,
											"en gardant la forme", NkRole::TextMuted);
									p.Unclip();
									if (hit.Clicked(key2)) {
										demo::Demo3DHostSetOutInsetOwnShaped(k5, !shp);
										NkMarkDirty(st);
									}
									yy += kRowH;
								}
							}
							yy += S(4.f);
							if (iSrc != s0 || iShape != sh0 || iXY[0] != x0 || iXY[1] != y0 ||
								iSz[0] != sz0 || iSz[1] != sz1 || iBrd != b0 || iOpa != o0) {
								demo::Demo3DHostSetOutInset(k5, iSrc, iShape, iXY, iSz, iBrd, iCol,
															iOpa);
								NkMarkDirty(st);
							}
						}
						if (nUsed < maxIns) {
							if (Button("out.ins.add", yy, "Ajouter une incrustation",
									   r.x + kPad, rr.w - 2.f * kPad))
								demo::Demo3DHostOutInsetAdd();
							yy += kRowH;
						} else {
							char fb[64];
							snprintf(fb, sizeof(fb), "Maximum atteint (%d incrustations).",
									 (int)maxIns);
							yy += S(3.f);
							yy += p.TextWrap(r.x + kPad, yy, rr.w - 2.f * kPad, fb,
											 NkRole::TextMuted);
							yy += S(6.f);
						}
						yy += NkGroupPad();
						PaintGroupBlock(p, rowR, gInsTop, yy);
						yy += NkPropGroupGap();
					}

					// ── LE RENDU ────────────────────────────────────────────
					// Hors des groupes : c'est l'ACTE, pas un reglage. Il
					// s'etale sur plusieurs images, donc le bouton dit ce qui
					// se passe au lieu de paraitre sans effet.
					{
						const bool busy = demo::Demo3DHostOutBusy();
						const NkRect br{rowR.x + kPad, yy + S(2.f), rowR.w - 2.f * kPad,
										kRowH + S(4.f)};
						const bool ovr = hit.Add("out.render", br);
						p.Fill(br, busy ? NkRole::PanelHeader : NkRole::AccentUi, 4.f);
						if (ovr && !busy)
							p.OutlineSharp(br, NkRole::Text);
						const char *bt = busy ? "Rendu en cours..." : "Rendre l'image";
						const float32 tw3 = p.TextW(bt);
						p.TextV(br.x + (br.w - tw3) * 0.5f, yy + S(2.f), kRowH + S(4.f), bt,
								busy ? NkRole::TextMuted : NkRole::TextOnAccent);
						if (!busy && hit.Clicked("out.render"))
							demo::Demo3DHostRenderOutput();
						yy += kRowH + S(10.f);
					}
		}

		// ── PASTILLE « OBJET » ──────────────────────────────────────────────────
		// La plus fournie : nom, transformation, pivot, dimensions, relations
		// (parente, enfants), materiaux lies, et les reglages propres au type --
		// lumiere, camera, empty. C'est ce qu'on regle le plus souvent, donc ce
		// qui merite le plus d'etre lisible.
		inline void PaintPropObject(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
									NkWidgetState &ws, const nkgui::NkGuiInput &in,
									NkComboPending &combo, nkgui::NkGuiContext *guiCtx,
									const NkRect &r, const NkRect &rr, float32 &yy) {
			auto Button = [&](const char *k2, float32 yB, const char *label, float32 x,
							  float32 w) -> bool {
				return NkPropButton(p, hit, k2, yB, label, x, w);
			};
			(void)Button;
			char key[64];
			char buf[160];
			(void)buf;
					// ── LA PIPETTE A-T-ELLE DESIGNE QUELQU'UN ? ──────────────────
					// Elle se resout ICI, avant tout le reste : cliquer une cible
					// CHANGE LA SELECTION, donc l'objet que le panneau affiche. Se
					// fier a l'objet courant arrivait toujours trop tard -- le
					// sujet etait deja remplace par la cible (constate par Rihen :
					// « le picker se confond avec le clic de selection »).
					if (st.relPick != 0 && st.relPickFor >= 0 && st.activeEmpty >= 0 &&
						st.activeEmpty != st.relPickPrev &&
						st.activeEmpty != st.relPickFor) {
						const int32 subj = st.relPickFor;
						const int32 tgt = st.activeEmpty;
						if (st.relPick == 1 && !NkHierIsDescendant(tgt, subj)) {
							demo::Demo3DHostSetNodeParent(subj, tgt);
							NkMarkDirty(st);
						} else if (st.relPick == 2 && !NkHierIsDescendant(subj, tgt)) {
							demo::Demo3DHostSetNodeParent(tgt, subj);
							NkMarkDirty(st);
						}
						// On REVIENT sur l'objet qu'on editait : sinon le panneau
						// reste sur la cible et l'on perd le fil de son reglage.
						demo::Demo3DHostDeselectAll();
						demo::Demo3DHostSelectEmptyNode(subj);
						st.activeEmpty = subj;
						st.relPick = 0;
						st.relPickFor = -1;
					}
					// â”€â”€ L'OBJET : nom + TRANSFORMATION COMPLETE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
					const int32 li = demo::Demo3DHostSelectedLight();
					const int32 act = demo::Demo3DHostActiveObject();
					if (st.activeEmpty >= 0) {
						// ── UN EMPTY : nom + transformation. Pas de rendu propre --
						// sa transformation n'agit QUE par ses enfants (le detecteur
						// de parente la repercute au sous-arbre).
						const int32 en = st.activeEmpty;
						NkHierNodeName(st, en, buf, sizeof(buf));
						// LE NOM EST UN CHAMP, pas une etiquette (Rihen) : on doit
						// pouvoir renommer ici, avant meme de toucher a la
						// transformation, sans passer par la hierarchie.
						{
							// L'icone dit la NATURE de l'objet, comme dans la
							// hierarchie : model, maillage, lumiere, camera, empty.
							// Elle est POUSSEE vers la droite (Rihen) : elle s'aligne
							// sur la marge du contenu, et le champ demarre apres elle.
							const float32 icoX = r.x + NkPropInset();
							p.IconV(icoX, yy, kRowH, NkNodeIcon(en), NkRole::Text, 13.f);
							const NkRect nmR{icoX + S(22.f), yy + S(2.f),
											 (r.x + rr.w - NkPropInset()) - (icoX + S(22.f)),
											 kRowH - S(4.f)};
							p.Outline(nmR, NkRole::Border, NkRole::InputBg, 3.f);
							if (en >= 0 && en < 176)
								EditableText(p, hit, ws, in, "props.name",
											 {nmR.x + S(4.f), yy, nmR.w - S(8.f), kRowH},
											 buf, NkRole::Text, st.customNames[en], 24u);
							yy += kRowH;
							// RESPIRATION entre le nom et le premier groupe
							// (Transformation) : colles, ils se lisaient comme un
							// seul bloc (Rihen).
							yy += NkPropGroupGap();
						}
						static int32 sELast = -1;
						static float32 sE[9] = {};
						float32 ep[3], er2[3], es2[3];
						if (demo::Demo3DHostEmptyTransform(en, ep, er2, es2)) {
							const bool holdE = ws.dragging || ws.editing;
							if (!holdE || en != sELast) {
								for (int32 a = 0; a < 3; ++a) {
									st.pos[a] = ep[a];
									st.rot[a] = er2[a];
									st.scl[a] = es2[a];
									sE[a] = ep[a];
									sE[3 + a] = er2[a];
									sE[6 + a] = es2[a];
								}
								sELast = en;
							}
							// LE CONTENU EST CENTRE dans le panneau, avec la meme
							// marge a gauche et a droite (Rihen).
							NkRect rowR = rr;
							rowR.x = r.x + NkPropInset();
							rowR.w = rr.w - 2.f * NkPropInset();
							// GROUPE « Transformation » : bandeau repliable, puis
							// Position / Rotation / Echelle / Pivot (Rihen, Banani).
							const bool grpXf = PaintPropGroup(p, hit, st, rowR, yy,
															  "prop.g.xform",
															  "Transformation", 1u);
							const float32 grpXfTop = yy;
							// Le contenu travaille EN RETRAIT du cadre (Rihen).
							const NkRect inR = NkGroupInner(rowR);
							// ── LES ACTIONS DU MENU DE CE GROUPE ───────────────
							// Elles agissent AVANT la peinture des champs, pour que
							// ceux-ci montrent deja le resultat. COPIER prend les
							// neuf valeurs, COLLER les repose sur l'objet courant
							// (c'est le geste demande par Rihen : porter la
							// transformation d'un objet sur un autre), et
							// REINITIALISER rend l'objet a l'origine, sans
							// rotation, a l'echelle 1.
							{
								const float32 cur9[9] = {st.pos[0], st.pos[1], st.pos[2],
														 st.rot[0], st.rot[1], st.rot[2],
														 st.scl[0], st.scl[1], st.scl[2]};
								if (NkGrpWants(st, "prop.g.xform", 1))
									NkGrpCopyF(st, "prop.g.xform", cur9, 9);
								if (NkGrpWants(st, "prop.g.xform", 2) &&
									NkGrpCanPaste(st, "prop.g.xform")) {
									for (int32 a = 0; a < 3; ++a) {
										st.pos[a] = st.grpClipF[a];
										st.rot[a] = st.grpClipF[3 + a];
										st.scl[a] = st.grpClipF[6 + a];
									}
									demo::Demo3DHostSetEmptyTransform(en, st.pos, st.rot, st.scl);
									NkMarkDirty(st);
								}
								if (NkGrpWants(st, "prop.g.xform", 3)) {
									for (int32 a = 0; a < 3; ++a) {
										st.pos[a] = 0.f;
										st.rot[a] = 0.f;
										st.scl[a] = 1.f;
									}
									demo::Demo3DHostSetEmptyTransform(en, st.pos, st.rot, st.scl);
									NkMarkDirty(st);
								}
							}
							if (grpXf) {
								yy += NkGroupPad(); // respiration en haut du bloc
								yy += PaintXformGroup(p, hit, ws, in, inR, yy, "Position",
												st.pos, 0.01f, "prop.epos", st.lockPos,
												st.propPos, "%.2f m", 0.f);
								yy += NkGroupPad(); // entre deux lignes du groupe
								yy += PaintXformGroup(p, hit, ws, in, inR, yy, "Rotation",
												st.rot, 0.5f, "prop.erot", st.lockRot,
												st.propRot, "%.1f\xC2\xB0", 0.f);
								yy += NkGroupPad();
								yy += PaintXformGroup(p, hit, ws, in, inR, yy, "Echelle",
												st.scl, 0.01f, "prop.escl", st.lockScl,
												st.propScale, "%.2f", 1.f);
							}
							// ── PIVOT (origine) ────────────────────────────────
							// Blender ne le laisse bouger qu'en mode Edition ; on
							// l'offre AUSSI en mode Objet (Rihen : « on ne sait
							// jamais »), avec son cadenas pour le figer quand on
							// ne veut pas y toucher -- comme les autres lignes.
							// Le deplacer NE DEPLACE PAS la matiere : les enfants
							// reculent d'autant, seul le point de rotation et de
							// mise a l'echelle change.
							if (grpXf) {
								float32 piv[3];
								if (demo::Demo3DHostNodeOrigin(act, piv)) {
									const float32 piv0[3] = {piv[0], piv[1], piv[2]};
									yy += NkGroupPad();
									yy += PaintXformGroup(p, hit, ws, in, inR, yy, "Pivot", piv,
													0.01f, "prop.epiv", st.lockPiv,
													st.propPiv, "%.2f m", 0.f);
									bool pivCh = false;
									for (int32 a = 0; a < 3; ++a)
										if (piv[a] != piv0[a])
											pivCh = true;
									if (pivCh && !st.lockPiv) {
										demo::Demo3DHostSetNodeOrigin(act, piv);
										NkMarkDirty(st);
									}
									float32 ctr[3];
									if (demo::Demo3DHostMeshesCenter(act, ctr)) {
										yy += NkGroupPad();
										if (Button("props.pivctr", yy,
												   "Pivot au centre des maillages", inR.x,
												   inR.w) &&
											!st.lockPiv) {
											demo::Demo3DHostSetNodeOrigin(act, ctr);
											NkMarkDirty(st);
										}
										yy += kRowH - S(4.f);
									}
								}
							}
							// Le BLOC qui entoure le groupe : peint APRES son contenu,
							// puisqu'il faut connaitre ou celui-ci s'arrete. Il se
							// referme sur une marge egale a celle du haut.
							if (grpXf) {
								yy += NkGroupPad();
								PaintGroupBlock(p, rowR, grpXfTop, yy);
							}
							// L'espace suit le groupe REPLIE OU NON (Rihen).
							yy += NkPropGroupGap();
							// Proportionnel et verrous : memes regles que l'objet.
							auto PropagateE = [](float32 *vals, const float32 *base, bool on) {
								if (!on)
									return;
								int32 ch = -1;
								for (int32 a = 0; a < 3; ++a)
									if (vals[a] != base[a])
										ch = a;
								if (ch < 0)
									return;
								if (fabsf(base[ch]) > 1e-6f) {
									const float32 ratio = vals[ch] / base[ch];
									for (int32 a = 0; a < 3; ++a)
										if (a != ch)
											vals[a] = base[a] * ratio;
								} else {
									const float32 d = vals[ch] - base[ch];
									for (int32 a = 0; a < 3; ++a)
										if (a != ch)
											vals[a] = base[a] + d;
								}
							};
							PropagateE(st.pos, sE, st.propPos);
							PropagateE(st.rot, sE + 3, st.propRot);
							PropagateE(st.scl, sE + 6, st.propScale);
							for (int32 a = 0; a < 3; ++a) {
								if (st.lockPos)
									st.pos[a] = sE[a];
								if (st.lockRot)
									st.rot[a] = sE[3 + a];
								if (st.lockScl)
									st.scl[a] = sE[6 + a];
							}
							// Un MAILLAGE UTILISATEUR a TOUTES les proprietes d'un
							// mesh (Rihen) : DIMENSIONS ici, materiau plus bas.
							const int32 ukE = demo::Demo3DHostUserKind(en);
							if (ukE >= 1 && ukE <= 3) {
								float32 dimE[3];
								demo::Demo3DHostNodeBaseSize(en, dimE);
								const float32 dimE0[3] = {dimE[0], dimE[1], dimE[2]};
								// GROUPE « Dimensions » : sa propre bande repliable.
								const bool grpDim = PaintPropGroup(p, hit, st, rowR, yy,
																   "prop.g.dim", "Dimensions",
																   2u);
								const float32 grpDimTop = yy;
								// ACTIONS DU MENU : copier / coller les trois cotes,
								// reinitialiser aux dimensions de la nature (un
								// tableau vide, que Demo3DHostNodeBaseSize rendra).
								if (NkGrpWants(st, "prop.g.dim", 1))
									NkGrpCopyF(st, "prop.g.dim", dimE, 3);
								if (NkGrpWants(st, "prop.g.dim", 2) &&
									NkGrpCanPaste(st, "prop.g.dim")) {
									for (int32 a = 0; a < 3; ++a)
										dimE[a] = st.grpClipF[a];
									demo::Demo3DHostSetNodeBaseSize(en, dimE);
									NkMarkDirty(st);
								}
								if (grpDim) {
									yy += NkGroupPad();
									yy += PaintXformGroup(p, hit, ws, in, NkGroupInner(rowR), yy, "",
													dimE, 0.01f, "prop.edim", st.lockDim,
													st.propDim, "%.2f m", 1.f);
									yy += NkGroupPad();
									PaintGroupBlock(p, rowR, grpDimTop, yy);
								}
								yy += NkPropGroupGap(); // meme replie (Rihen)
					// PROPORTIONNEL : l'axe touche impose son RAPPORT aux autres ;
					// sinon chaque dimension ne bouge QUE son axe (Rihen).
					if (st.propDim) {
						int32 chD = -1;
						for (int32 a = 0; a < 3; ++a)
							if (dimE[a] != dimE0[a])
								chD = a;
						if (chD >= 0 && fabsf(dimE0[chD]) > 1e-6f) {
							const float32 rd = dimE[chD] / dimE0[chD];
							for (int32 a = 0; a < 3; ++a)
								if (a != chD)
									dimE[a] = dimE0[a] * rd;
						}
					}
					if (st.lockDim)
						for (int32 a = 0; a < 3; ++a)
							dimE[a] = dimE0[a];
					bool dimChE = false;
					for (int32 a = 0; a < 3; ++a)
						if (dimE[a] != dimE0[a])
							dimChE = true;
					if (dimChE) {
						demo::Demo3DHostSetNodeBaseSize(en, dimE);
						NkMarkDirty(st);
					}
							}
							// ── GROUPE « RELATIONS » ────────────────────────────
							// La maquette montre Parent et Enfant ; Blender y ajoute
							// ce qui rend la parente COMPREHENSIBLE : ce que le
							// parent transmet, et le nombre d'enfants portes. Sans
							// cela on voit un lien sans savoir ce qu'il fait.
							{
								const bool grpRel = PaintPropGroup(p, hit, st, rowR, yy,
																   "prop.g.rel", "Relations",
																   4u);
								const float32 grpRelTop = yy;
								if (grpRel) {
									const NkRect iR = NkGroupInner(rowR);
									yy += NkGroupPad();
									// (La pipette se resout PLUS HAUT, au niveau du panneau :
									// designer une cible change la selection, donc l'objet
									// affiche -- la resoudre ici arrivait trop tard.)
									const float32 valX = iR.x + S(96.f);
									const float32 valW = iR.w - S(96.f);
									// PARENT : une LISTE, pas une etiquette (Rihen) -- on
									// CHANGE de parent depuis elle, « Aucun » detachant
									// l'objet. Les candidats excluent l'objet lui-meme
									// et sa descendance : s'y rattacher ferait un cycle.
									p.TextV(iR.x, yy, kRowH, "Parent", NkRole::TextMuted);
									{
										static char sParName[24][24];
										static const char *sParPtr[24];
										static int32 sParNode[24];
										static int32 sParOwner = -1;
										static int32 sParSel = 0;
										// LE NOEUD DEJA APPLIQUE. La liste est peinte APRES
										// le panneau : le choix de l'utilisateur n'arrive
										// donc qu'a la frame suivante. Comparer a l'etat
										// « avant l'appel » ne voyait jamais rien, et
										// recaler l'indice sur le parent reel effacait le
										// choix avant qu'on le lise -- le piege des listes
										// differees, deja paye une fois.
										static int32 sParApplied = -2;
										int32 np = 0;
										snprintf(sParName[0], sizeof(sParName[0]), "Aucun");
										sParPtr[0] = sParName[0];
										sParNode[0] = -1;
										np = 1;
										const int32 pa = demo::Demo3DHostNodeParent(en);
										int32 curIdx = 0;
										const int32 ncP = demo::Demo3DHostNodeCount();
										for (int32 c9 = 0; c9 < ncP && np < 24; ++c9) {
											if (c9 == en || NkHierNodeSkip(c9))
												continue;
											if (NkHierIsDescendant(c9, en))
												continue; // interdit : cycle
											NkHierNodeName(st, c9, sParName[np],
														   sizeof(sParName[0]));
											sParPtr[np] = sParName[np];
											sParNode[np] = c9;
											if (c9 == pa)
												curIdx = np;
											++np;
										}
										if (sParOwner != en) {
											sParOwner = en;
											sParSel = curIdx;
											sParApplied = pa;
										}
										if (sParSel < 0 || sParSel >= np)
											sParSel = curIdx;
										// DEUX SENS, sans se marcher dessus : si la liste
										// designe autre chose que ce qu'on a applique,
										// c'est l'utilisateur qui a choisi -> on reparente.
										// Sinon, si la parente reelle a change AILLEURS
										// (hierarchie, glisser-deposer), la liste s'y
										// recale.
										if (sParNode[sParSel] != sParApplied) {
											demo::Demo3DHostSetNodeParent(en, sParNode[sParSel]);
											NkMarkDirty(st);
											sParApplied = sParNode[sParSel];
										} else if (pa != sParApplied) {
											sParSel = curIdx;
											sParApplied = pa;
										}
										// PIPETTE : designer le parent dans la vue plutot que
										// de le chercher dans la liste (Rihen, Blender).
										const bool pkP = (st.relPick == 1 && st.relPickFor == en);
										const NkRect vb{valX, yy + S(3.f), valW - S(24.f),
														kRowH - S(6.f)};
										Combo(p, hit, ws, "prop.rel.par", vb, sParPtr, nullptr, np,
											  sParSel, combo);
										{
											const NkRect eb{vb.x + vb.w + S(4.f), vb.y, S(20.f),
															vb.h};
											const bool ovE = hit.Add("prop.rel.parpick", eb);
											p.Outline(eb,
													  (pkP || ovE) ? NkRole::AccentUi
																   : NkRole::Border,
													  pkP ? NkRole::AccentUi : NkRole::PanelHeader,
													  3.f);
											p.IconV(eb.x + (eb.w - S(11.f)) * 0.5f, eb.y, eb.h,
													NkIcon::Pipette,
													pkP ? NkRole::TextOnAccent : NkRole::TextMuted,
													11.f);
											if (hit.Clicked("prop.rel.parpick")) {
												st.relPick = pkP ? 0 : 1;
												st.relPickFor = en;
												st.relPickPrev = st.activeEmpty;
											}
										}
									}
									yy += kRowH;
									// ENFANTS : la LISTE, pas un compte (Rihen). Un nombre
									// dit qu'il y en a ; la liste dit LESQUELS -- et la
									// choisir, c'est y aller.
									{
										static char sKidName[16][24];
										static const char *sKidPtr[16];
										static int32 sKidNode[16];
										static int32 sKidOwner = -1;
										static int32 sKidSel = 0;
										int32 nk = 0;
										const int32 ncR = demo::Demo3DHostNodeCount();
										for (int32 c9 = 0; c9 < ncR && nk < 16; ++c9)
											if (!NkHierNodeSkip(c9) &&
												demo::Demo3DHostNodeParent(c9) == en) {
												NkHierNodeName(st, c9, sKidName[nk],
															   sizeof(sKidName[0]));
												sKidPtr[nk] = sKidName[nk];
												sKidNode[nk] = c9;
												++nk;
											}
										// LE COMPTE dans le libelle, LA LISTE a cote (Rihen) :
										// on sait d'un coup d'oeil combien il y en a, et
										// lesquels si on ouvre.
										char klab[24];
										snprintf(klab, sizeof(klab), "Enfants (%d)", nk);
										p.TextV(iR.x, yy, kRowH, klab, NkRole::TextMuted);
										const float32 delW = nk ? S(24.f) : 0.f;
										const NkRect vb{valX, yy + S(3.f), valW - delW,
														kRowH - S(6.f)};
										if (nk == 0) {
											p.Outline(vb, NkRole::Border, NkRole::InputBg, 2.f);
											p.TextV(vb.x + S(6.f), yy, kRowH, "Aucun",
													NkRole::TextMuted);
										} else {
											// Changer d'objet remet la liste sur son
											// premier enfant : garder l'ancien indice
											// designerait un enfant qui n'est plus la.
											if (sKidOwner != en) {
												sKidOwner = en;
												sKidSel = 0;
											}
											if (sKidSel < 0 || sKidSel >= nk)
												sKidSel = nk - 1;
											Combo(p, hit, ws, "prop.rel.kids", vb, sKidPtr,
												  nullptr, nk, sKidSel, combo);
											// RETIRER l'enfant choisi : detacher se fait
											// depuis la ou on le designe (Rihen).
											const NkRect db{vb.x + vb.w + S(4.f), vb.y,
															S(20.f), vb.h};
											const bool ovD = hit.Add("prop.rel.kdel", db);
											p.Outline(db, ovD ? NkRole::AccentUi : NkRole::Border,
													  NkRole::PanelHeader, 3.f);
											p.IconV(db.x + (db.w - S(11.f)) * 0.5f, db.y, db.h,
													NkIcon::MinusCircle, NkRole::TextMuted, 11.f);
											if (hit.Clicked("prop.rel.kdel") && sKidSel < nk) {
												demo::Demo3DHostSetNodeParent(sKidNode[sKidSel], -1);
												NkMarkDirty(st);
											}
										}
									}
									yy += kRowH;
									// AJOUTER UN ENFANT depuis ce panneau (Rihen) : la
									// liste propose les objets LIBRES du document -- ceux
									// qui n'ont pas encore de parent et qui ne sont pas
									// dans la descendance de celui-ci.
									{
										static char sAddName[24][24];
										static const char *sAddPtr[24];
										static int32 sAddNode[24];
										static int32 sAddSel = 0;
										// « Aucun » EN TETE ET PAR DEFAUT (Rihen) : tant qu'on
										// n'a designe personne, le bouton n'ajoute rien --
										// sinon un clic distrait rattache le premier venu.
										int32 na = 0;
										snprintf(sAddName[0], sizeof(sAddName[0]), "Aucun");
										sAddPtr[0] = sAddName[0];
										sAddNode[0] = -1;
										na = 1;
										const int32 ncA = demo::Demo3DHostNodeCount();
										for (int32 c9 = 0; c9 < ncA && na < 24; ++c9) {
											if (c9 == en || NkHierNodeSkip(c9))
												continue;
											if (demo::Demo3DHostNodeParent(c9) == en)
												continue; // deja enfant
											if (NkHierIsDescendant(en, c9))
												continue; // interdit : cycle
											NkHierNodeName(st, c9, sAddName[na],
														   sizeof(sAddName[0]));
											sAddPtr[na] = sAddName[na];
											sAddNode[na] = c9;
											++na;
										}
										// PAS DE LIBELLE « Ajouter » (Rihen) : le libelle
										// « Enfants » couvre deja la liste ET l'ajout. Cette
										// seconde ligne s'aligne simplement sous la
										// premiere, ce qui la rattache visiblement a elle.
										{
											if (sAddSel < 0 || sAddSel >= na)
												sAddSel = 0;
											// La liste, PUIS la pipette, PUIS le plus : on
											// designe l'objet du regard ou du doigt, et on
											// valide toujours par le meme bouton (Rihen).
											const bool pkC = (st.relPick == 2 && st.relPickFor == en);
											const NkRect ab{valX, yy + S(3.f), valW - S(48.f),
															kRowH - S(6.f)};
											Combo(p, hit, ws, "prop.rel.addk", ab, sAddPtr,
												  nullptr, na, sAddSel, combo);
											{
												const NkRect eb{ab.x + ab.w + S(4.f), ab.y, S(20.f),
																ab.h};
												const bool ovE = hit.Add("prop.rel.addpick", eb);
												p.Outline(eb,
														  (pkC || ovE) ? NkRole::AccentUi
																	   : NkRole::Border,
														  pkC ? NkRole::AccentUi
															  : NkRole::PanelHeader,
														  3.f);
												p.IconV(eb.x + (eb.w - S(11.f)) * 0.5f, eb.y, eb.h,
														NkIcon::Pipette,
														pkC ? NkRole::TextOnAccent
															: NkRole::TextMuted,
														11.f);
												if (hit.Clicked("prop.rel.addpick")) {
													st.relPick = pkC ? 0 : 2;
													st.relPickFor = en;
													st.relPickPrev = st.activeEmpty;
												}
											}
											// Le bouton ne fait rien tant que le choix est
											// « Aucun » : il s'affiche eteint pour le dire,
											// et BLANC des qu'il peut agir.
											const bool canAdd =
												sAddSel > 0 && sAddSel < na && sAddNode[sAddSel] >= 0;
											const NkRect pb{ab.x + ab.w + S(28.f), ab.y, S(20.f),
															ab.h};
											const bool ovP = hit.Add("prop.rel.addb", pb);
											p.Outline(pb,
													  (canAdd && ovP) ? NkRole::AccentUi
																	  : NkRole::Border,
													  NkRole::PanelHeader, 3.f);
											p.IconV(pb.x + (pb.w - S(11.f)) * 0.5f, pb.y, pb.h,
													NkIcon::PlusCircle,
													canAdd ? NkRole::Text : NkRole::TextMuted,
													11.f);
											if (canAdd && hit.Clicked("prop.rel.addb")) {
												demo::Demo3DHostSetNodeParent(sAddNode[sAddSel],
																			  en);
												NkMarkDirty(st);
												sAddSel = 0; // on revient a « Aucun »
											}
										}
										yy += kRowH;
									}
									// TOUS LES ENFANTS D'UN COUP (Rihen) : detacher un a
									// un devient vite penible des qu'ils sont nombreux.
									if (demo::Demo3DHostNodeHasChildren(en)) {
										if (Button("prop.rel.kallout", yy,
												   "Detacher tous les enfants", iR.x, iR.w)) {
											const int32 ncD = demo::Demo3DHostNodeCount();
											for (int32 c9 = 0; c9 < ncD; ++c9)
												if (!NkHierNodeSkip(c9) &&
													demo::Demo3DHostNodeParent(c9) == en) {
													demo::Demo3DHostSetNodeParent(c9, -1);
													NkMarkDirty(st);
												}
										}
										yy += kRowH;
									}
									// CE QUE LE PARENT TRANSMET (apport de Blender) :
									// position, rotation, echelle, chacune coupable.
									// La ligne travaille dans le rectangle INTERIEUR du
									// groupe, sinon ses boutons sortaient du cadre.
									if (demo::Demo3DHostNodeHasChildren(en))
										NkXmitRow(p, hit, iR, iR, yy, en);
									yy += NkGroupPad();
									PaintGroupBlock(p, rowR, grpRelTop, yy);
								}
								yy += NkPropGroupGap();
							}
							bool diffE = false;
							for (int32 a = 0; a < 3; ++a)
								if (st.pos[a] != sE[a] || st.rot[a] != sE[3 + a] ||
									st.scl[a] != sE[6 + a])
									diffE = true;
							if (diffE) {
								demo::Demo3DHostSetEmptyTransform(en, st.pos, st.rot, st.scl);
								NkMarkDirty(st);
								for (int32 a = 0; a < 3; ++a) {
									sE[a] = st.pos[a];
									sE[3 + a] = st.rot[a];
									sE[6 + a] = st.scl[a];
								}
							}
							// ── GROUPE « MATERIAUX » ────────────────────────────
							// Structure de la maquette et de Blender : l'EMPLACEMENT
							// (son nom, sa pastille de couleur, ses commandes), puis
							// la SURFACE avec ses parametres.
							//
							// On n'expose QUE ce que notre moteur rend vraiment :
							// couleur de base, metallique, rugosite. Le reste du
							// Principled BSDF (emission, occlusion, vernis, diffusion,
							// duvet, anisotropie) existe dans NkPBRParams mais n'a pas
							// encore de surcharge par objet -- l'afficher sans effet
							// serait mentir sur ce que l'outil sait faire.
							// (Le groupe « Materiaux » a DEMENAGE dans la pastille
							// MATERIAU du panneau : bibliotheque du projet, apercus
							// et assignation -- regle de Rihen, un seul endroit.)
							// ── GROUPE « LUMIERE » ──────────────────────────────
							// Structure de Blender (captures de Rihen) : le TYPE en
							// tete, puis couleur et puissance, puis ce qui appartient
							// au type choisi -- portee, cones du spot, taille de
							// l'area -- et enfin l'ombre.
							//
							// Notre moteur n'a ni temperature de couleur, ni
							// exposition, ni normalisation, ni reglages d'influence
							// par canal : ces lignes de Blender ne sont donc PAS
							// reprises, plutot que d'etre affichees sans effet.
							const bool grpLit =
								(ukE == 5) ? PaintPropGroup(p, hit, st, rowR, yy, "prop.g.lit",
															"Lumiere", 16u)
										   : false;
							const float32 grpLitTop = yy;
							if (ukE == 5 && grpLit) {
								const NkRect iR = NkGroupInner(rowR);
								const float32 lvX = iR.x + S(96.f);
								const float32 lvW = iR.w - S(96.f);
								yy += NkGroupPad();
								// LE TYPE, en quatre boutons comme Blender : on voit
								// d'un coup lequel est actif et on en change sans
								// ouvrir de liste.
								{
									static const char *const kLT[4] = {"Soleil", "Point",
																	   "Spot", "Area"};
									static const NkIcon kLI[4] = {NkIcon::Sun, NkIcon::Light,
																  NkIcon::Light, NkIcon::Square};
									const int32 cur = demo::Demo3DHostUserSub(en) & 3;
									const float32 g5 = S(3.f);
									const float32 bw5 = (iR.w - g5 * 3.f) * 0.25f;
									float32 bx5 = iR.x;
									char k5[32];
									for (int32 t5 = 0; t5 < 4; ++t5) {
										snprintf(k5, sizeof(k5), "prop.lit.t%d", t5);
										const NkRect br5{bx5, yy + S(2.f), bw5, kRowH - S(4.f)};
										const bool ov5 = hit.Add(k5, br5);
										const bool on5 = (t5 == cur);
										p.Outline(br5,
												  (on5 || ov5) ? NkRole::AccentUi
															   : NkRole::Border,
												  on5 ? NkRole::AccentUi : NkRole::PanelHeader,
												  3.f);
										p.IconV(br5.x + S(4.f), br5.y, br5.h, kLI[t5],
												on5 ? NkRole::TextOnAccent : NkRole::TextMuted,
												11.f);
										p.TextV(br5.x + S(20.f), yy, kRowH, kLT[t5],
												on5 ? NkRole::TextOnAccent : NkRole::Text);
										if (hit.Clicked(k5) && !on5) {
											demo::Demo3DHostSetUserSub(en, t5);
											NkMarkDirty(st);
										}
										bx5 += bw5 + g5;
									}
									yy += kRowH + S(2.f);
								}
								float32 ulc[3], uli = 1.f;
								if (demo::Demo3DHostUserLightParams(en, ulc, &uli)) {
									bool ulch = false;
									const float32 ulc0[3] = {ulc[0], ulc[1], ulc[2]};
									// LA COULEUR SE CHOISIT A L'OEIL : la nuance ouvre le
									// vrai picker (carre saturation/valeur + teinte), les
									// champs chiffres restent dessous pour la precision.
									yy += PaintColorRow(p, hit, ws, in, st, iR, yy, "Couleur",
														"prop.ulcol", ulc, &ulch);
									yy += NkGroupPad();
									p.TextV(iR.x, yy, kRowH, "Puissance", NkRole::TextMuted);
									ulch |= DragFloat(p, hit, ws, in, "prop.ulint",
													  {lvX, yy + S(3.f), lvW, kRowH - S(6.f)},
													  uli, 0.05f, NkRole::AccentUi, "%.2f");
									yy += kRowH;
									// TEMPERATURE ET EXPOSITION (Rihen) : elles vivent
									// desormais DANS LE MOTEUR. 0 K = temperature
									// desactivee, 0 stop = exposition neutre : tant
									// qu'on n'y touche pas, rien ne change.
									{
										float32 tK = 0.f, ex = 0.f;
										if (demo::Demo3DHostLightTempExp(en, &tK, &ex)) {
											const float32 tK0 = tK, ex0 = ex;
											p.TextV(iR.x, yy, kRowH, "Temperature",
													NkRole::TextMuted);
											DragFloat(p, hit, ws, in, "prop.ultemp",
													  {lvX, yy + S(3.f), lvW, kRowH - S(6.f)},
													  tK, 25.f, NkRole::AccentUi, "%.0f K");
											yy += kRowH;
											p.TextV(iR.x, yy, kRowH, "Exposition",
													NkRole::TextMuted);
											DragFloat(p, hit, ws, in, "prop.ulexp",
													  {lvX, yy + S(3.f), lvW, kRowH - S(6.f)},
													  ex, 0.05f, NkRole::AccentUi, "%.2f");
											yy += kRowH;
											if (tK != tK0 || ex != ex0) {
												demo::Demo3DHostSetLightTempExp(en, tK, ex);
												NkMarkDirty(st);
											}
										}
									}
									if (ulch || ulc[0] != ulc0[0] || ulc[1] != ulc0[1] ||
										ulc[2] != ulc0[2]) {
										demo::Demo3DHostSetUserLightParams(en, ulc, uli);
										NkMarkDirty(st);
									}
								}
								{
									// PROPRIETES NATIVES par type : portee, cones du spot, dimensions
									// de l'area, ombres -- visibles ICI et a la creation (Rihen).
									float32 rgL, inL, outL, awL, ahL;
									bool shL = true;
									int32 tyL = -1;
									if (demo::Demo3DHostLightEx(en, &rgL, &inL, &outL, &awL, &ahL, &shL,
																&tyL)) {
										const float32 v0[5] = {rgL, inL, outL, awL, ahL};
										const bool s0 = shL;
										auto LRow = [&](const char *lbl, const char *k4, float32 &val,
														float32 stp, const char *fm) {
											// Les lignes travaillent dans le rectangle
											// INTERIEUR du groupe, comme partout ailleurs.
											p.TextV(iR.x, yy, kRowH, lbl, NkRole::TextMuted);
											DragFloat(p, hit, ws, in, k4,
													  {lvX, yy + S(3.f), lvW, kRowH - S(4.f)},
													  val, stp, NkRole::AccentUi, fm);
											yy += kRowH;
										};
										if (tyL != 0)
											LRow("Portee", "prop.ulex.rg", rgL, 0.05f, "%.2f");
										// LOI D'ATTENUATION (Rihen, 9 aout) : au choix PAR
										// lumiere, comme Unreal. « Physique » = 1/d^2 fenetre —
										// la portee ne fait que couper, l'intensite devient
										// comparable a Blender. « Heritee » = l'ancienne loi,
										// defaut des scenes existantes. Pas pour la
										// directionnelle : elle n'a pas d'attenuation.
										if (tyL != 0) {
											// LE POPUP D'UN COMBO ECRIT EN FIN DE FRAME, PAR
											// POINTEUR (pending.selected). Une LOCALE de pile
											// est morte a ce moment-la : le choix partait dans
											// de la pile invalide et la « Loi » semblait
											// verrouillee (constate par Rihen). Stockage
											// STATIQUE, re-synchronise du hote — et la POUSSEE
											// du choix se fait AVANT la resynchro, sinon elle
											// l'ecraserait.
											static int32 sAttSel = 0;
											static int32 sAttFor = -1;
											const int32 am0 = demo::Demo3DHostLightAttMode(en);
											if (sAttFor == en && sAttSel != am0) {
												demo::Demo3DHostSetLightAttMode(en, sAttSel);
												NkMarkDirty(st);
											} else {
												sAttSel = am0;
											}
											sAttFor = en;
											static const char *const kAtt[2] = {
												"Heritee (portee = niveau)",
												"Physique (1/d2, portee = coupure)"};
											p.TextV(iR.x, yy, kRowH, "Loi", NkRole::TextMuted);
											Combo(p, hit, ws, "prop.ulex.att",
												  {lvX, yy + S(2.f), lvW, kRowH - S(4.f)}, kAtt,
												  nullptr, 2, sAttSel, combo);
											yy += kRowH;
										}
										if (tyL == 2) {
											LRow("Cone interne", "prop.ulex.ci", inL, 0.2f, "%.1f");
											LRow("Cone externe", "prop.ulex.co", outL, 0.2f, "%.1f");
										}
										if (tyL == 3) {
											LRow("Largeur", "prop.ulex.aw", awL, 0.01f, "%.2f");
											LRow("Hauteur", "prop.ulex.ah", ahL, 0.01f, "%.2f");
										}
										{
											const NkRect cb2{iR.x, yy + S(5.f), S(12.f), S(12.f)};
											hit.Add("prop.ulex.sh", cb2);
											p.Outline(cb2, shL ? NkRole::AccentUi : NkRole::Border,
													  shL ? NkRole::AccentUi : NkRole::InputBg, 2.f);
											p.TextV(cb2.x + S(18.f), yy, kRowH, "Ombres portees",
													NkRole::TextMuted);
											if (hit.Clicked("prop.ulex.sh"))
												shL = !shL;
											yy += kRowH;
										}
										// PROFONDEUR LINEAIRE (omni seulement, Rihen — option
										// LearnOpenGL) : l'atlas recoit dist/portee au lieu de
										// la profondeur projetee -> biais constant, coutures de
										// faces effacees. Cable DIRECTEMENT a l'hote, comme la
										// Loi : c'est un choix, pas un reglage de portee.
										if (tyL == 1) {
											const bool ln0 = demo::Demo3DHostLightShadowLinear(en);
											bool lnL = ln0;
											const NkRect cb3{iR.x, yy + S(5.f), S(12.f), S(12.f)};
											hit.Add("prop.ulex.shlin", cb3);
											p.Outline(cb3, lnL ? NkRole::AccentUi : NkRole::Border,
													  lnL ? NkRole::AccentUi : NkRole::InputBg, 2.f);
											p.TextV(cb3.x + S(18.f), yy, kRowH,
													"Profondeur lineaire", NkRole::TextMuted);
											if (hit.Clicked("prop.ulex.shlin"))
												lnL = !lnL;
											yy += kRowH;
											if (lnL != ln0) {
												demo::Demo3DHostSetLightShadowLinear(en, lnL);
												NkMarkDirty(st);
											}
										}
										if (rgL != v0[0] || inL != v0[1] || outL != v0[2] || awL != v0[3] ||
											ahL != v0[4] || shL != s0) {
											demo::Demo3DHostSetLightEx(en, rgL, inL, outL, awL, ahL, shL);
											NkMarkDirty(st);
										}
									}
								}
								// COULEUR / TEXTURE / MIX (Rihen) : par defaut couleur pure ;
								// Texture = cookie de l'atlas (couleur forcee au blanc) ; Mix =
								// texture x couleur libre. Deux textures melangees : avec le
								// chargement de fichiers ; le nodal NKGraphe ensuite.
								{
									const int32 ck0 = demo::Demo3DHostLightCookie(en);
									const bool whiteC = [&] {
										float32 c4[3];
										float32 i4 = 1.f;
										return demo::Demo3DHostUserLightParams(en, c4, &i4) &&
											   c4[0] > 0.99f && c4[1] > 0.99f && c4[2] > 0.99f;
									}();
									const int32 derivedM = ck0 < 0 ? 0 : (whiteC ? 1 : 2);
									if (st.lightSrcNode != en) {
										st.lightSrcNode = en;
										st.lightSrcUi = derivedM;
									}
									static const char *const kCMix[3] = {"Couleur", "Texture", "Mix"};
									// DANS le groupe : cette ligne se calait encore sur le
									// panneau entier et debordait donc du cadre (Rihen).
									p.TextV(iR.x, yy, kRowH, "Source", NkRole::TextMuted);
									Combo(p, hit, ws, "prop.ulex.mode",
										  {lvX, yy + S(2.f), lvW, kRowH - S(4.f)},
										  kCMix, nullptr, 3, st.lightSrcUi, combo);
									yy += kRowH;
									if (st.lightSrcUi != derivedM) {
										if (st.lightSrcUi == 0) {
											demo::Demo3DHostSetLightCookie(en, -1);
											NkMarkDirty(st);
										} else {
											demo::Demo3DHostSetLightCookie(en, ck0 < 0 ? 0 : ck0);
											NkMarkDirty(st);
											if (st.lightSrcUi == 1) {
												const float32 wc[3] = {1.f, 1.f, 1.f};
												float32 c5[3];
								float32 i5 = 1.f;
								demo::Demo3DHostUserLightParams(en, c5, &i5);
								demo::Demo3DHostSetUserLightParams(en, wc, i5);
								NkMarkDirty(st);
											}
										}
									}
									if (st.lightSrcUi > 0) {
										float32 slot = (float32)(ck0 < 0 ? 0 : ck0);
										p.TextV(iR.x, yy, kRowH, "Texture (atlas)", NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.ulex.slot",
												  {lvX, yy + S(3.f), lvW, kRowH - S(4.f)},
												  slot, 0.05f, NkRole::AccentUi, "%.0f");
										yy += kRowH;
										if ((int32)(slot + 0.5f) != ck0) {
											demo::Demo3DHostSetLightCookie(en, (int32)(slot + 0.5f));
											NkMarkDirty(st);
										}
									}
								}
								yy += NkGroupPad();
								PaintGroupBlock(p, rowR, grpLitTop, yy);
							}
							if (ukE == 5)
								yy += NkPropGroupGap();
							{
								// CAMERA : ses proprietes propres (declaratives tant que
								// le rendu au travers de la camera n'est pas branche).
								float32 cf = 50.f, cnr = 0.1f, cfr = 100.f;
								if (demo::Demo3DHostCameraParams(en, &cf, &cnr, &cfr)) {
									// UN GROUPE A PART ENTIERE, repliable, comme
									// Transformation et Relations (Rihen) -- plus une
									// simple etiquette suivie de rangees flottantes.
									const bool grpCam = PaintPropGroup(p, hit, st, rowR, yy,
																	   "prop.g.cam", "Camera", 3u);
									const float32 grpCamTop = yy;
									if (grpCam) {
									yy += NkGroupPad();
									// MARGES DU GROUPE : les rangees ci-dessous sont
									// ecrites en coordonnees de PANNEAU (r.x + kPad,
									// rr.w - 128...). On SUBSTITUE localement r et rr
									// par des rects derives de l'INTERIEUR du groupe :
									// memes formules, et les labels tombent a iCam.x,
									// les champs s'arretent a iCam.x + iCam.w -- les
									// marges des autres groupes, sans reecrire chaque
									// rangee (les rangees flottantes debordaient du
									// cadre, constate par Rihen).
									const NkRect iCam = NkGroupInner(rowR);
									const NkRect r{iCam.x - kPad, iCam.y, iCam.w, iCam.h};
									const NkRect rr{iCam.x, iCam.y, iCam.w + kPad + S(8.f),
													iCam.h};
									const float32 c0[3] = {cf, cnr, cfr};
									// TYPE (Rihen) : perspective ou orthographique. En
									// ortho, l'ECHELLE Y du noeud regle la demi-hauteur
									// du cadre (regle consignee).
									const bool isO = demo::Demo3DHostCamOrtho(en);
									{
										p.TextV(r.x + kPad, yy, kRowH, "Type", NkRole::TextMuted);
										const float32 bw2 = (rr.w - S(128.f)) * 0.5f - S(2.f);
										const NkRect bp{r.x + S(120.f), yy + S(2.f), bw2,
														kRowH - S(4.f)};
										const NkRect bo{bp.x + bw2 + S(4.f), yy + S(2.f), bw2,
														kRowH - S(4.f)};
										hit.Add("prop.cam.persp", bp);
										hit.Add("prop.cam.ortho", bo);
										p.Fill(bp, !isO ? NkRole::AccentUi : NkRole::PanelHeader,
											   3.f);
										p.Fill(bo, isO ? NkRole::AccentUi : NkRole::PanelHeader,
											   3.f);
										p.TextV(bp.x + (bp.w - p.TextW("Perspective")) * 0.5f, yy,
												kRowH, "Perspective",
												!isO ? NkRole::TextOnAccent : NkRole::TextMuted);
										p.TextV(bo.x + (bo.w - p.TextW("Ortho")) * 0.5f, yy, kRowH,
												"Ortho",
												isO ? NkRole::TextOnAccent : NkRole::TextMuted);
										if (hit.Clicked("prop.cam.persp")) {
											demo::Demo3DHostSetCamOrtho(en, false);
											NkMarkDirty(st);
										}
										if (hit.Clicked("prop.cam.ortho")) {
											demo::Demo3DHostSetCamOrtho(en, true);
											NkMarkDirty(st);
										}
										yy += kRowH;
									}
									// PROPRIETES PAR TYPE (Rihen) : la focale n'a pas de
									// sens en ortho (rayons paralleles) -- elle cede la
									// place a l'ECHELLE ORTHO (demi-hauteur du cadre =
									// echelle du noeud). Clips et passe-partout restent
									// communs.
									if (isO) {
										float32 osc = demo::Demo3DHostCamOrthoScale(en);
										const float32 os0 = osc;
										p.TextV(r.x + kPad, yy, kRowH, "Echelle ortho",
												NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.cortho",
												  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
												   kRowH - S(4.f)},
												  osc, 0.02f, NkRole::AccentUi, "%.2f");
										if (osc != os0) {
											demo::Demo3DHostSetCamOrthoScale(en, osc);
											NkMarkDirty(st);
										}
										yy += kRowH;
									} else {
									// UNITE DE FOCALE (parite Blender) : degres ou
									// millimetres -- meme grandeur, la conversion passe
									// par le CAPTEUR. La focale ANGULAIRE reste la seule
									// verite cote hote.
									const bool inMM = demo::Demo3DHostCamLensMM(en);
									{
										p.TextV(r.x + kPad, yy, kRowH, "Unite focale",
												NkRole::TextMuted);
										const float32 bw3 = (rr.w - S(128.f)) * 0.5f - S(2.f);
										const NkRect bd{r.x + S(120.f), yy + S(2.f), bw3,
														kRowH - S(4.f)};
										const NkRect bm{bd.x + bw3 + S(4.f), yy + S(2.f), bw3,
														kRowH - S(4.f)};
										hit.Add("prop.cam.udeg", bd);
										hit.Add("prop.cam.umm", bm);
										p.Fill(bd, !inMM ? NkRole::AccentUi : NkRole::PanelHeader,
											   3.f);
										p.Fill(bm, inMM ? NkRole::AccentUi : NkRole::PanelHeader,
											   3.f);
										p.TextV(bd.x + (bd.w - p.TextW("Degres")) * 0.5f, yy,
												kRowH, "Degres",
												!inMM ? NkRole::TextOnAccent : NkRole::TextMuted);
										p.TextV(bm.x + (bm.w - p.TextW("mm")) * 0.5f, yy, kRowH,
												"mm",
												inMM ? NkRole::TextOnAccent : NkRole::TextMuted);
										if (hit.Clicked("prop.cam.udeg")) {
											demo::Demo3DHostSetCamLensMM(en, false);
											NkMarkDirty(st);
										}
										if (hit.Clicked("prop.cam.umm")) {
											demo::Demo3DHostSetCamLensMM(en, true);
											NkMarkDirty(st);
										}
										yy += kRowH;
									}
									if (inMM) {
										float32 mmv = demo::Demo3DHostCamFocalMM(en);
										const float32 mm0 = mmv;
										p.TextV(r.x + kPad, yy, kRowH, "Focale (mm)",
												NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.cfmm",
												  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
												   kRowH - S(4.f)},
												  mmv, 0.5f, NkRole::AccentUi, "%.1f");
										yy += kRowH;
										if (mmv != mm0) {
											demo::Demo3DHostSetCamFocalMM(en, mmv);
											NkMarkDirty(st);
										}
										float32 sen = demo::Demo3DHostCamSensor(en);
										const float32 se0 = sen;
										p.TextV(r.x + kPad, yy, kRowH, "Capteur (mm)",
												NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.csen",
												  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
												   kRowH - S(4.f)},
												  sen, 0.2f, NkRole::AccentUi, "%.0f");
										yy += kRowH;
										if (sen != se0) {
											demo::Demo3DHostSetCamSensor(en, sen);
											NkMarkDirty(st);
										}
									} else {
									p.TextV(r.x + kPad, yy, kRowH, "Focale (deg)",
											NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "prop.cfov",
											  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
											   kRowH - S(4.f)},
											  cf, 0.2f, NkRole::AccentUi, "%.0f");
									yy += kRowH;
									}
									}
									p.TextV(r.x + kPad, yy, kRowH, "Clip debut",
											NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "prop.cnear",
											  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
											   kRowH - S(4.f)},
											  cnr, 0.01f, NkRole::AccentUi, "%.2f");
									yy += kRowH;
									p.TextV(r.x + kPad, yy, kRowH, "Clip fin",
											NkRole::TextMuted);
									DragFloat(p, hit, ws, in, "prop.cfar",
											  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
											   kRowH - S(4.f)},
											  cfr, 0.5f, NkRole::AccentUi, "%.0f");
									yy += kRowH;
									if (cf != c0[0] || cnr != c0[1] || cfr != c0[2]) {
										demo::Demo3DHostSetCameraParams(en, cf, cnr, cfr);
										NkMarkDirty(st);
									}
									// ── PASSE-PARTOUT (Rihen) : couleur + opacite
									// du voile hors cadre quand on regarde par
									// cette camera. Noir a 60 % par defaut.
									{
										float32 ppc[4];
										demo::Demo3DHostCamPasse(en, ppc);
										const float32 pa0 = ppc[3];
										bool ppCh = false;
										const NkRect iC{r.x + kPad, 0.f, rr.w - kPad * 2.f, 0.f};
										yy += PaintColorRow(p, hit, ws, in, st, iC, yy,
															"Passe-partout", "prop.campp", ppc,
															&ppCh);
										p.TextV(r.x + kPad, yy, kRowH, "Opacite voile",
												NkRole::TextMuted);
										DragFloat(p, hit, ws, in, "prop.camppa",
												  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
												   kRowH - S(4.f)},
												  ppc[3], 0.01f, NkRole::AccentUi, "%.2f");
										yy += kRowH;
										if (ppc[3] < 0.f)
											ppc[3] = 0.f;
										if (ppc[3] > 1.f)
											ppc[3] = 1.f;
										if (ppCh || ppc[3] != pa0) {
											demo::Demo3DHostSetCamPasse(en, ppc);
											NkMarkDirty(st);
										}
									}
									// ── GUIDES DE COMPOSITION + ZONES SURES (Rihen,
									// parite Blender) : traces dans le cadre en vue
									// camera. Bits : 1 tiers, 2 centre, 4 diagonales,
									// 8 nombre d'or, 16 zones sures.
									{
										const int32 gd = demo::Demo3DHostCamGuides(en);
										static const char *const kGd[5] = {
											"Tiers", "Centre", "Diagonales", "Nombre d'or",
											"Zones sures"};
										for (int32 g5 = 0; g5 < 5; ++g5) {
											const int32 bit = 1 << g5;
											const NkRect cbG{r.x + kPad, yy + S(5.f), S(12.f),
															 S(12.f)};
											char gk[24];
											snprintf(gk, sizeof(gk), "prop.cam.gd%d", g5);
											hit.Add(gk, cbG);
											const bool onG = (gd & bit) != 0;
											p.Outline(cbG, onG ? NkRole::AccentUi : NkRole::Border,
													  onG ? NkRole::AccentUi : NkRole::InputBg,
													  2.f);
											p.TextV(cbG.x + S(18.f), yy, kRowH, kGd[g5],
													NkRole::TextMuted);
											if (hit.Clicked(gk)) {
												demo::Demo3DHostSetCamGuides(en, gd ^ bit);
												NkMarkDirty(st);
											}
											yy += kRowH;
										}
									}
									yy += NkGroupPad();
									PaintGroupBlock(p, rowR, grpCamTop, yy);
									} // fin du groupe Camera
									yy += NkPropGroupGap();
								}
							}
							// (« Transmettre » a rejoint le groupe RELATIONS : le
							// laisser aussi ici l'affichait DEUX FOIS -- constate par
							// Rihen sur sa capture.)
						}
					} else if (li >= 0) {
						// UNE LUMIERE A SES PROPRIETES comme un maillage (Rihen) --
						// les vides et cameras suivront avec le modele objet.
						demo::Demo3DHostLightName(li, buf, sizeof(buf));
						p.IconV(r.x + kPad, yy, kRowH, NkIcon::Light, NkRole::Text, 13.f);
						p.TextV(r.x + kPad + S(18.f), yy, kRowH, buf);
						yy += kRowH;
						float32 lpos[3];
						demo::Demo3DHostLightPosition(li, lpos);
						NkRect rowR = rr;
						rowR.x = r.x + kPad; // meme marge des deux cotes
						rowR.w = rr.w - 2.f * kPad;
						PaintTransformRow(p, hit, ws, in, rowR, yy, "Position", lpos, 0.01f,
										  "prop.lpos", NkIcon::None, NkIcon::None);
						static float32 sLPull[3] = {};
						static int32 sLLast = -1;
						if (sLLast != li || !(ws.dragging || ws.editing)) {
							// tirer : la lumiere peut bouger par son widget dans la vue
						}
						if (lpos[0] != sLPull[0] || lpos[1] != sLPull[1] || lpos[2] != sLPull[2]) {
							if (sLLast == li) {
								demo::Demo3DHostSetLightPosition(li, lpos);
								NkMarkDirty(st);
							}
							sLPull[0] = lpos[0];
							sLPull[1] = lpos[1];
							sLPull[2] = lpos[2];
							sLLast = li;
						}
						yy += Vec3RowH();
						// ROTATION (direction du faisceau) : soleil, projecteur,
						// surfacique -- une ponctuelle n'a pas d'orientation.
						if (demo::Demo3DHostLightType(li) != 1) {
							static float32 sLE[4][3];
							static bool sLEInit[4] = {};
							float32 ld[3];
							demo::Demo3DHostLightDir(li, ld);
							if (li >= 0 && li < 4 && !sLEInit[li]) {
								// angles retrouves depuis la direction (rx elevation,
								// rz azimut ; ry libre, sans effet visuel)
								float32 dz = ld[2] < -1.f ? -1.f : (ld[2] > 1.f ? 1.f : ld[2]);
								sLE[li][0] = asinf(-dz) * 57.29578f;
								sLE[li][1] = 0.f;
								sLE[li][2] = atan2f(ld[0], -ld[1]) * 57.29578f;
								sLEInit[li] = true;
							}
							const float32 le0[3] = {sLE[li][0], sLE[li][1], sLE[li][2]};
							PaintTransformRow(p, hit, ws, in, rowR, yy, "Rotation",
											  sLE[li], 0.5f, "prop.lrot", NkIcon::None,
											  NkIcon::None, "%.1f");
							yy += Vec3RowH();
							if (sLE[li][0] != le0[0] || sLE[li][1] != le0[1] ||
								sLE[li][2] != le0[2]) {
								const float32 k2r = 0.017453292f;
								const float32 cxr = cosf(sLE[li][0] * k2r);
								const float32 sxr = sinf(sLE[li][0] * k2r);
								const float32 czr = cosf(sLE[li][2] * k2r);
								const float32 szr = sinf(sLE[li][2] * k2r);
								// direction = Rz(azimut) * Rx(elevation) . (0,-1,0)
								const float32 nd[3] = {cxr * szr, -cxr * czr, -sxr};
								demo::Demo3DHostSetLightDir(li, nd);
								NkMarkDirty(st);
							}
						}
						float32 lcol[3], lint = 1.f;
						demo::Demo3DHostLightParams(li, lcol, &lint);
						bool lch = false;
						p.TextV(r.x + kPad, yy, kRowH, "Intensite", NkRole::TextMuted);
						lch |= DragFloat(p, hit, ws, in, "prop.lint",
										 {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f), kRowH - S(4.f)},
										 lint, 0.05f, NkRole::AccentUi, "%.2f");
						yy += kRowH;
						{
							// LA NUANCE, pas trois nombres : le picker se deplie sous
							// elle (Rihen). La lumiere suit en direct, le bloc qui suit
							// detecte le changement comme avant.
							NkRect cR = rowR;
							cR.x = r.x + kPad;
							cR.w = rr.w - S(16.f);
							bool cCh = false;
							yy += PaintColorRow(p, hit, ws, in, st, cR, yy, "Couleur",
												"prop.lcol", lcol, &cCh);
							lch |= cCh;
						}
						static float32 sLC[4] = {-1.f, 0.f, 0.f, 0.f};
						if (lch || lcol[0] != sLC[1] || lcol[1] != sLC[2] || lcol[2] != sLC[3]) {
							if ((int32)sLC[0] == li) {
								for (int32 a = 0; a < 3; ++a)
									if (lcol[a] < 0.f)
										lcol[a] = 0.f;
								demo::Demo3DHostSetLightParams(li, lcol, lint < 0.f ? 0.f : lint);
								NkMarkDirty(st);
								// PROPAGER coche : memes reglages pour les lumieres
								// DESCENDANTES de celle-ci (propriete commune).
								if (st.matPropagate)
									for (int32 l2 = 0; l2 < demo::Demo3DHostLightCount(); ++l2)
										if (l2 != li && NkHierIsDescendant(86 + l2, 86 + li)) {
											demo::Demo3DHostSetLightParams(l2, lcol,
																		   lint < 0.f ? 0.f : lint);
											NkMarkDirty(st);
										}
							}
							sLC[0] = (float32)li;
							sLC[1] = lcol[0];
							sLC[2] = lcol[1];
							sLC[3] = lcol[2];
						}
						{
							// PROPRIETES NATIVES par type : portee, cones du spot, dimensions
							// de l'area, ombres -- visibles ICI et a la creation (Rihen).
							float32 rgL, inL, outL, awL, ahL;
							bool shL = true;
							int32 tyL = -1;
							if (demo::Demo3DHostLightEx(86 + li, &rgL, &inL, &outL, &awL, &ahL, &shL,
														&tyL)) {
								const float32 v0[5] = {rgL, inL, outL, awL, ahL};
								const bool s0 = shL;
								auto LRow = [&](const char *lbl, const char *k4, float32 &val,
												float32 stp, const char *fm) {
									p.TextV(r.x + kPad, yy, kRowH, lbl, NkRole::TextMuted);
									DragFloat(p, hit, ws, in, k4,
											  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f), kRowH - S(4.f)},
											  val, stp, NkRole::AccentUi, fm);
									yy += kRowH;
								};
								if (tyL != 0)
									LRow("Portee", "prop.lex.rg", rgL, 0.05f, "%.2f");
								if (tyL == 2) {
									LRow("Cone interne", "prop.lex.ci", inL, 0.2f, "%.1f");
									LRow("Cone externe", "prop.lex.co", outL, 0.2f, "%.1f");
								}
								if (tyL == 3) {
									LRow("Largeur", "prop.lex.aw", awL, 0.01f, "%.2f");
									LRow("Hauteur", "prop.lex.ah", ahL, 0.01f, "%.2f");
								}
								{
									const NkRect cb2{r.x + kPad, yy + S(5.f), S(12.f), S(12.f)};
									hit.Add("prop.lex.sh", cb2);
									p.Outline(cb2, shL ? NkRole::AccentUi : NkRole::Border,
											  shL ? NkRole::AccentUi : NkRole::InputBg, 2.f);
									p.TextV(cb2.x + S(18.f), yy, kRowH, "Ombres portees",
											NkRole::TextMuted);
									if (hit.Clicked("prop.lex.sh"))
										shL = !shL;
									yy += kRowH;
								}
								if (rgL != v0[0] || inL != v0[1] || outL != v0[2] || awL != v0[3] ||
									ahL != v0[4] || shL != s0) {
									demo::Demo3DHostSetLightEx(86 + li, rgL, inL, outL, awL, ahL, shL);
									NkMarkDirty(st);
								}
							}
						}
						// COULEUR / TEXTURE / MIX (Rihen) : par defaut couleur pure ;
						// Texture = cookie de l'atlas (couleur forcee au blanc) ; Mix =
						// texture x couleur libre. Deux textures melangees : avec le
						// chargement de fichiers ; le nodal NKGraphe ensuite.
						{
							const int32 ck0 = demo::Demo3DHostLightCookie(86 + li);
							const bool whiteC = lcol[0] > 0.99f && lcol[1] > 0.99f && lcol[2] > 0.99f;
							const int32 derivedM = ck0 < 0 ? 0 : (whiteC ? 1 : 2);
							if (st.lightSrcNode != 86 + li) {
								st.lightSrcNode = 86 + li;
								st.lightSrcUi = derivedM;
							}
							static const char *const kCMix[3] = {"Couleur", "Texture", "Mix"};
							p.TextV(r.x + kPad, yy, kRowH, "Source", NkRole::TextMuted);
							Combo(p, hit, ws, "prop.lex.mode",
								  {r.x + S(120.f), yy + S(2.f), rr.w - S(128.f), kRowH - S(4.f)},
								  kCMix, nullptr, 3, st.lightSrcUi, combo);
							yy += kRowH;
							if (st.lightSrcUi != derivedM) {
								if (st.lightSrcUi == 0) {
									demo::Demo3DHostSetLightCookie(86 + li, -1);
									NkMarkDirty(st);
								} else {
									demo::Demo3DHostSetLightCookie(86 + li, ck0 < 0 ? 0 : ck0);
									NkMarkDirty(st);
									if (st.lightSrcUi == 1) {
										const float32 wc[3] = {1.f, 1.f, 1.f};
										demo::Demo3DHostSetLightParams(li, wc, lint < 0.f ? 0.f : lint);
										NkMarkDirty(st);
									}
								}
							}
							if (st.lightSrcUi > 0) {
								float32 slot = (float32)(ck0 < 0 ? 0 : ck0);
								p.TextV(r.x + kPad, yy, kRowH, "Texture (atlas)", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "prop.lex.slot",
										  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f), kRowH - S(4.f)},
										  slot, 0.05f, NkRole::AccentUi, "%.0f");
								yy += kRowH;
								if ((int32)(slot + 0.5f) != ck0) {
									demo::Demo3DHostSetLightCookie(86 + li, (int32)(slot + 0.5f));
									NkMarkDirty(st);
								}
							}
						}
						if (demo::Demo3DHostNodeHasChildren(86 + li)) {
							NkPropagateCheck(p, hit, r, yy, "prop.lprop", st.matPropagate);
							yy += kRowH;
							NkXmitRow(p, hit, r, rr, yy, 86 + li);
						}
					} else if (act >= 0 && demo::Demo3DHostObjectSelected(act)) {
						demo::Demo3DHostObjectName(act, buf, sizeof(buf));
						p.IconV(r.x + kPad, yy, kRowH, NkIcon::Mesh, NkRole::Text, 13.f);
						p.TextV(r.x + kPad + S(18.f), yy, kRowH, buf);
						yy += kRowH;

						// SYNC : TIRER quand on ne glisse pas (le gizmo et la vue
						// restent maitres), POUSSER au changement des champs. Un axe
						// VERROUILLE revient a la valeur tiree ; l'echelle
						// PROPORTIONNELLE propage le rapport de l'axe touche.
						static int32 sLastAct = -1;
						static float32 sPull[9] = {};
						const bool holding = ws.dragging || ws.editing;
						float32 tp[3], tr2[3], ts2[3];
						if (demo::Demo3DHostObjectTransform(act, tp, tr2, ts2)) {
							if (!holding || act != sLastAct) {
								for (int32 a = 0; a < 3; ++a) {
									st.pos[a] = tp[a];
									st.rot[a] = tr2[a];
									st.scl[a] = ts2[a];
									sPull[a] = tp[a];
									sPull[3 + a] = tr2[a];
									sPull[6 + a] = ts2[a];
								}
								sLastAct = act;
							}
							// CENTRE dans le panneau : meme marge a gauche et a droite
							// (kPad, celle des autres panneaux).
							NkRect rowR = rr;
							rowR.x = r.x + kPad;
							rowR.w = rr.w - 2.f * kPad;
							PaintTransformRow(p, hit, ws, in, rowR, yy, "Position", st.pos, 0.01f,
											  "prop.pos", st.lockPos ? NkIcon::Lock : NkIcon::Unlock,
											  NkIcon::Refresh, "%.2f", NkIcon::Link2, st.propPos);
							if (hit.Clicked("prop.pos.ic0"))
								st.lockPos = !st.lockPos;
							if (hit.Clicked("prop.pos.ic2"))
								st.propPos = !st.propPos;
							yy += Vec3RowH();
							PaintTransformRow(p, hit, ws, in, rowR, yy, "Rotation", st.rot, 0.5f,
											  "prop.rot", st.lockRot ? NkIcon::Lock : NkIcon::Unlock,
											  NkIcon::Refresh, "%.1f", NkIcon::Link2, st.propRot);
							if (hit.Clicked("prop.rot.ic0"))
								st.lockRot = !st.lockRot;
							if (hit.Clicked("prop.rot.ic2"))
								st.propRot = !st.propRot;
							yy += Vec3RowH();
							PaintTransformRow(p, hit, ws, in, rowR, yy, "Echelle", st.scl, 0.01f,
											  "prop.scl", st.lockScl ? NkIcon::Lock : NkIcon::Unlock,
											  NkIcon::Refresh, "%.2f", NkIcon::Link2, st.propScale);
							if (hit.Clicked("prop.scl.ic0"))
								st.lockScl = !st.lockScl;
							if (hit.Clicked("prop.scl.ic2"))
								st.propScale = !st.propScale;
							yy += Vec3RowH();
							// DIMENSIONS (unites monde) : echelle x taille de base de
							// la nature ; les EDITER ajuste l'echelle, comme Blender.
							{
								// DIMENSIONS = taille LOCALE, DECOUPLEE de l'echelle
								// (Rihen) : l'editer redimensionne la geometrie au rendu,
								// l'echelle n'en sait rien -- et reciproquement.
								float32 dim[3];
								demo::Demo3DHostNodeBaseSize(act, dim);
								const float32 dim0[3] = {dim[0], dim[1], dim[2]};
								PaintTransformRow(p, hit, ws, in, rowR, yy, "Dimensions", dim,
									  0.01f, "prop.dim",
									  st.lockDim ? NkIcon::Lock : NkIcon::Unlock,
									  NkIcon::Refresh, "%.2f", NkIcon::Link2, st.propDim);
					if (hit.Clicked("prop.dim.ic0"))
						st.lockDim = !st.lockDim;
					if (hit.Clicked("prop.dim.ic2"))
						st.propDim = !st.propDim;
					yy += Vec3RowH();
					// PROPORTIONNEL : l'axe touche impose son RAPPORT aux autres ;
					// sinon chaque dimension ne bouge QUE son axe (Rihen).
					if (st.propDim) {
						int32 chD = -1;
						for (int32 a = 0; a < 3; ++a)
							if (dim[a] != dim0[a])
								chD = a;
						if (chD >= 0 && fabsf(dim0[chD]) > 1e-6f) {
							const float32 rd = dim[chD] / dim0[chD];
							for (int32 a = 0; a < 3; ++a)
								if (a != chD)
									dim[a] = dim0[a] * rd;
						}
					}
					if (st.lockDim)
						for (int32 a = 0; a < 3; ++a)
							dim[a] = dim0[a];
					bool dimCh = false;
					for (int32 a = 0; a < 3; ++a)
						if (dim[a] != dim0[a])
							dimCh = true;
					if (dimCh) {
						demo::Demo3DHostSetNodeBaseSize(act, dim);
						NkMarkDirty(st);
					}
							}

							// PROPORTIONNEL (par ligne, 3e icone) : l'axe touche propage
							// son RAPPORT aux autres -- delta simple quand la base est
							// nulle, un rapport n'y aurait pas de sens.
							auto Propagate = [](float32 *vals, const float32 *base, bool on) {
								if (!on)
									return;
								int32 ch = -1;
								for (int32 a = 0; a < 3; ++a)
									if (vals[a] != base[a])
										ch = a;
								if (ch < 0)
									return;
								if (fabsf(base[ch]) > 1e-6f) {
									const float32 ratio = vals[ch] / base[ch];
									for (int32 a = 0; a < 3; ++a)
										if (a != ch)
											vals[a] = base[a] * ratio;
								} else {
									const float32 d = vals[ch] - base[ch];
									for (int32 a = 0; a < 3; ++a)
										if (a != ch)
											vals[a] = base[a] + d;
								}
							};
							Propagate(st.pos, sPull, st.propPos);
							Propagate(st.rot, sPull + 3, st.propRot);
							Propagate(st.scl, sPull + 6, st.propScale);
							// Verrous : la ligne revient a l'etat tire.
							for (int32 a = 0; a < 3; ++a) {
								if (st.lockPos)
									st.pos[a] = sPull[a];
								if (st.lockRot)
									st.rot[a] = sPull[3 + a];
								if (st.lockScl)
									st.scl[a] = sPull[6 + a];
							}
							bool diff = false;
							for (int32 a = 0; a < 3; ++a)
								if (fabsf(st.pos[a] - sPull[a]) > 1e-5f ||
									fabsf(st.rot[a] - sPull[3 + a]) > 1e-5f ||
									fabsf(st.scl[a] - sPull[6 + a]) > 1e-5f)
									diff = true;
							if (diff) {
								// La MEME modification pour TOUTE la selection : le delta
								// tape ici se propage aux autres objets selectionnes.
								float32 dP[3], dR[3], rS[3];
								for (int32 a = 0; a < 3; ++a) {
									dP[a] = st.pos[a] - sPull[a];
									dR[a] = st.rot[a] - sPull[3 + a];
									rS[a] = (sPull[6 + a] > 1e-6f) ? st.scl[a] / sPull[6 + a] : 1.f;
								}
								demo::Demo3DHostSetObjectTransform(act, st.pos, st.rot, st.scl);
								NkMarkDirty(st);
								demo::Demo3DHostApplyDeltaToSelection(dP, dR, rS, act);
								for (int32 a = 0; a < 3; ++a) {
									sPull[a] = st.pos[a];
									sPull[3 + a] = st.rot[a];
									sPull[6 + a] = st.scl[a];
								}
							}
						}
						if (demo::Demo3DHostNodeHasChildren(act))
							NkXmitRow(p, hit, r, rr, yy, act);
						// (Le PIVOT a rejoint le groupe Transformation, avec son cadenas
						// et son bouton de reinitialisation comme les autres lignes.)
						// (Le bloc « Materiau » a DEMENAGE dans la pastille MATERIAU
						// du panneau : bibliotheque du projet, apercus et
						// assignation -- regle de Rihen, un seul endroit.)
						int32 nSel = 0;
						const int32 nO = demo::Demo3DHostObjectCount();
						for (int32 i3 = 0; i3 < nO; ++i3)
							if (demo::Demo3DHostObjectSelected(i3))
								++nSel;
						snprintf(buf, sizeof(buf), "%d objet(s) selectionne(s)", nSel);
						p.TextV(r.x + kPad, yy, kRowH, buf, NkRole::TextMuted);
						yy += kRowH;
						if (Button("props.desel", yy, "Tout deselectionner", r.x + kPad,
								   rr.w - 2.f * kPad))
							demo::Demo3DHostDeselectAll();
						yy += kRowH;
					} else {
						p.TextV(r.x + kPad, yy, kRowH, "Aucun objet selectionne", NkRole::TextMuted);
						yy += kRowH;
					}
		}

		// ── PASTILLE « MODE » ───────────────────────────────────────────────────
		// Unique a chaque mode (Objet, Edition, Sculpture...). Ses fonctions
		// arrivent PROGRESSIVEMENT par categories -- regle de Rihen : un onglet
		// nait avec ses outils, pas vide.
		inline void PaintPropMode(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
									NkWidgetState &ws, const nkgui::NkGuiInput &in,
									NkComboPending &combo, nkgui::NkGuiContext *guiCtx,
									const NkRect &r, const NkRect &rr, float32 &yy) {
			auto Button = [&](const char *k2, float32 yB, const char *label, float32 x,
							  float32 w) -> bool {
				return NkPropButton(p, hit, k2, yB, label, x, w);
			};
			(void)Button;
			char key[64];
			char buf[160];
			(void)buf;
					// ── LA PASTILLE DU MODE : unique a chaque mode, ses
					// fonctions arrivent PROGRESSIVEMENT par categories (regle
					// de Rihen). Aujourd'hui : l'EDITION porte ses premieres
					// categories ; les autres modes annoncent honnetement ce
					// qui vient -- aucune commande factice.
					// (Indice 7 depuis qu'Output occupe le 6 : la pastille du
					// mode reste TOUJOURS la derniere de la colonne.)
					const int32 m5 = (int32)st.mode;
					NkRect rowR = rr;
					rowR.x = r.x + NkPropInset();
					rowR.w = rr.w - 2.f * NkPropInset();
					if (m5 == 1) {
						// ── EDITION ─────────────────────────────────────────
						const bool gSel = PaintPropGroup(p, hit, st, rowR, yy,
														 "prop.g.edsel", "Selection",
														 0x1000u);
						const float32 gSelTop = yy;
						if (gSel) {
							const NkRect iR = NkGroupInner(rowR);
							yy += NkGroupPad();
							const int32 m2 = demo::Demo3DHostEditSelMask();
							snprintf(buf, sizeof(buf), "Sous-mode : %s%s%s",
									 (m2 & 1) ? "Sommets " : "", (m2 & 2) ? "Aretes " : "",
									 (m2 & 4) ? "Faces" : "");
							p.TextV(iR.x, yy, kRowH, buf, NkRole::TextMuted);
							yy += kRowH;
							p.TextV(iR.x, yy, kRowH, "1 / 2 / 3 pour changer",
									NkRole::TextMuted);
							yy += kRowH + NkGroupPad();
							PaintGroupBlock(p, rowR, gSelTop, yy);
						}
						yy += NkPropGroupGap();
						const bool gTools = PaintPropGroup(p, hit, st, rowR, yy,
														   "prop.g.edtools", "Outils",
														   0x2000u);
						const float32 gToolsTop = yy;
						if (gTools) {
							const NkRect iR = NkGroupInner(rowR);
							yy += NkGroupPad();
							static const char *const kEdT[5] = {
								"E  --  extruder", "I  --  inserer une face",
								"Ctrl+B  --  biseauter", "Ctrl+R  --  boucle de coupe",
								"K  --  couteau   W  --  subdiviser"};
							for (int32 t6 = 0; t6 < 5; ++t6) {
								p.TextV(iR.x, yy, kRowH, kEdT[t6], NkRole::TextMuted);
								yy += kRowH;
							}
							yy += NkGroupPad();
							PaintGroupBlock(p, rowR, gToolsTop, yy);
						}
						yy += NkPropGroupGap();
						const bool gGeo = PaintPropGroup(p, hit, st, rowR, yy,
														 "prop.g.edgeo", "Geometrie",
														 0x4000u);
						const float32 gGeoTop = yy;
						if (gGeo) {
							const NkRect iR = NkGroupInner(rowR);
							yy += NkGroupPad();
							p.TextV(iR.x, yy, kRowH,
									"Fusion, separation, symetrie -- a venir.",
									NkRole::TextMuted);
							yy += kRowH + NkGroupPad();
							PaintGroupBlock(p, rowR, gGeoTop, yy);
						}
					} else {
						static const char *const kSoon[5] = {
							"Sculpture 2.5D : brosses de relief -- a venir.",
							"Sculpture : brosses volumiques -- a venir.",
							"Texturing : peinture et calques -- a venir.",
							"Patron : depliage UV (unwrapping) -- a venir.",
							"Texture painting : peinture sur texture -- a venir."};
						if (m5 >= 2 && m5 <= 6) {
							// Blocs qui vont a la ligne : ces phrases depassaient
							// la largeur du panneau des qu'on le retrecissait.
							yy += S(3.f);
							yy += p.TextWrap(r.x + kPad, yy, rowR.w - 2.f * kPad, kSoon[m5 - 2],
											 NkRole::TextMuted);
							yy += S(4.f);
							yy += p.TextWrap(r.x + kPad, yy, rowR.w - 2.f * kPad,
											 "Ses categories apparaitront ici, dans cette "
											 "pastille.",
											 NkRole::TextMuted);
							yy += S(6.f);
						}
					}
		}

		// ── PASTILLE « OUTIL » ──────────────────────────────────────────────────
		// Les reglages de l'outil ACTIF : ce que fait le clic, dans quel repere,
		// avec quelle aimantation, et l'edition proportionnelle.
		inline void PaintPropTool(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
									NkWidgetState &ws, const nkgui::NkGuiInput &in,
									NkComboPending &combo, nkgui::NkGuiContext *guiCtx,
									const NkRect &r, const NkRect &rr, float32 &yy) {
			auto Button = [&](const char *k2, float32 yB, const char *label, float32 x,
							  float32 w) -> bool {
				return NkPropButton(p, hit, k2, yB, label, x, w);
			};
			(void)Button;
			char key[64];
			char buf[160];
			(void)buf;
					// ── OUTIL (pastille dediee, regle de Rihen) ─────────────────
					// Les reglages de l'outil ACTIF : ce que fait le clic, dans
					// quel repere, avec quelle aimantation. Ils etaient heberges
					// par Modificateur -- un provisoire annonce comme tel dans le
					// code -- et retrouvent ici leur place, comme l'onglet
					// « Tool » de Blender.
					NkRect rowR = rr;
					rowR.x = r.x + NkPropInset();
					rowR.w = rr.w - 2.f * NkPropInset();
					// ── GROUPE « OUTIL ACTIF » : les reglages de l'outil courant.
					const bool grpTool = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.tool",
														"Outil actif", 2048u);
					const float32 grpToolTop = yy;
					if (!grpTool) {
						yy += NkPropGroupGap();
		
					} else {
						yy += NkGroupPad();
						// Memes marges internes que les groupes de la Scene.
						const NkRect iT = NkGroupInner(rowR);
						const NkRect r{iT.x - kPad, rowR.y, iT.w + 2.f * kPad, rowR.h};
						const NkRect rr = r;
					// (« Ajouter un modificateur » a rejoint la pastille
					// MODIFICATEUR : il etait parti avec l'outil lors du
					// decoupage, alors qu'il n'a rien a y faire -- Rihen.)
					// ── L'OUTIL -- et PLUSIEURS quand plusieurs coexistent : l'outil
					// de transformation garde son bloc, le mode EDITION empile le
					// sien dessous (deplacer + extruder arrivent ensemble, chacun
					// doit rester lisible). ─────────────────────────────────────────
					static const char *const kToolNames[6] = {"Selection",  "Curseur 3D", "Deplacer",
															  "Rotation",	"Echelle",	  "Multigizmo"};
					p.TextV(r.x + kPad, yy, kRowH, kToolNames[(int32)st.tool]);
					yy += kRowH;
					if (st.tool == NkTool::Select) {
						int32 nS2 = 0;
						const char *const *shapes = NkSelShapeItems(nS2);
						float32 bx = r.x + kPad;
						for (int32 i3 = 0; i3 < nS2; ++i3) {
							snprintf(key, sizeof(key), "props.shape.%d", i3);
							float32 wq = (rr.w - 2.f * kPad - 8.f) / 3.f;
							if (wq < S(30.f))
								wq = S(30.f);
							const NkRect br{bx, yy + S(2.f), wq, kRowH - S(4.f)};
							hit.Add(key, br);
							if (st.selShape == i3)
								p.Fill(br, NkRole::AccentUi, 3.f);
							else
								p.Outline(br, NkRole::Border, NkRole::PanelHeader, 3.f);
							p.Clip(br);
							p.TextV(br.x + S(6.f), yy, kRowH, shapes[i3],
									st.selShape == i3 ? NkRole::TextOnAccent : NkRole::Text);
							p.Unclip();
							if (hit.Clicked(key))
								st.selShape = i3;
							bx += wq + 4.f;
						}
						yy += kRowH;
					} else if (st.tool == NkTool::Cursor) {
						p.TextV(r.x + kPad, yy, kRowH, "Clic gauche : poser le curseur 3D",
								NkRole::TextMuted);
						yy += kRowH;
						if (Button("props.cur0", yy, "Remettre a l'origine", r.x + kPad,
								   rr.w - 2.f * kPad))
							demo::Demo3DHostResetCursor();
						yy += kRowH;
						// CURSEUR <-> SELECTION, en mode objet comme en edition.
						if (Button("props.cur1", yy, "Curseur -> selection", r.x + kPad,
								   rr.w - 2.f * kPad))
							demo::Demo3DHostCursorToSelection();
						yy += kRowH;
						if (Button("props.cur2", yy, "Selection -> curseur", r.x + kPad,
								   rr.w - 2.f * kPad))
							demo::Demo3DHostSelectionToCursor();
						yy += kRowH;
					} else {
						// Orientation : libelles CLIPPES a leur bouton -- en retrecissant
						// le panneau ils debordaient (Deplacer, Rotation, Echelle et le
						// cumule partagent ce bloc).
						// ORIENTATION EN COMBO (Rihen) : depuis qu'elles sont SEPT
						// (Monde, Local, Normale, Gimbal, Vue, Curseur, Parent),
						// une rangee de boutons calculee pour trois debordait du
						// groupe. Une liste tient dans n'importe quelle largeur et
						// nomme les entrees en toutes lettres.
						p.TextV(r.x + kPad, yy, kRowH, "Orientation", NkRole::TextMuted);
						int32 nOr = 0;
						const char *const *orients = NkOrientItems(nOr);
						Combo(p, hit, ws, "props.orient",
							  {r.x + S(96.f), yy + S(2.f), rr.w - S(104.f), kRowH - S(4.f)},
							  orients, NkOrientIcons(), nOr, st.orientation, combo);
						yy += kRowH;
						// AIMANTATION : la bascule ET ses PAS, modifiables ici.
						const bool snapOn = demo::Demo3DHostSnapEnabled();
						if (Button("props.snap", yy,
								   snapOn ? "Aimantation : active" : "Aimantation : coupee",
								   r.x + kPad, rr.w - 2.f * kPad))
							demo::Demo3DHostSetSnap(!snapOn, st.snapStepT, st.snapStepR,
													st.snapStepS);
						yy += kRowH;
						{
							static const char *const kSn[3] = {"Pas deplacement", "Pas angle",
															   "Pas echelle"};
							float32 *sv[3] = {&st.snapStepT, &st.snapStepR, &st.snapStepS};
							static const char *const kFm[3] = {"%.2f", "%.0f", "%.2f"};
							bool snCh = false;
							for (int32 i3 = 0; i3 < 3; ++i3) {
								p.TextV(r.x + kPad + S(8.f), yy, kRowH, kSn[i3], NkRole::TextMuted);
								snprintf(key, sizeof(key), "props.snap.%d", i3);
								snCh |= DragFloat(p, hit, ws, in, key,
												  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
												   kRowH - S(4.f)},
												  *sv[i3], i3 == 1 ? 0.5f : 0.01f,
												  NkRole::AccentUi, kFm[i3]);
								yy += kRowH;
							}
							if (snCh) {
								if (st.snapStepT < 0.01f)
									st.snapStepT = 0.01f;
								if (st.snapStepR < 1.f)
									st.snapStepR = 1.f;
								if (st.snapStepS < 0.01f)
									st.snapStepS = 0.01f;
								demo::Demo3DHostSetSnap(snapOn, st.snapStepT, st.snapStepR,
														st.snapStepS);
							}
						}
						// ── EDITION PROPORTIONNELLE : bascule, rayon, attenuation.
						// Les memes reglages que le chevron de la barre -- une seule
						// verite, lue au moteur -- pour qui travaille au panneau.
						{
							bool peOn = false;
							float32 peR = 1.f;
							int32 peF = 0;
							demo::Demo3DHostPropEdit(&peOn, &peR, &peF);
							const float32 r0 = peR;
							const int32 f0 = peF;
							if (Button("props.pe", yy,
									   peOn ? "Edition proportionnelle : active"
											: "Edition proportionnelle : coupee",
									   r.x + kPad, rr.w - 2.f * kPad))
								demo::Demo3DHostSetPropEdit(!peOn, peR, peF);
							yy += kRowH;
							if (peOn) {
								p.TextV(r.x + kPad + S(8.f), yy, kRowH, "Rayon", NkRole::TextMuted);
								DragFloat(p, hit, ws, in, "props.pe.r",
										  {r.x + S(120.f), yy + S(3.f), rr.w - S(128.f),
										   kRowH - S(4.f)},
										  peR, 0.02f, NkRole::AccentUi, "%.2f m");
								yy += kRowH;
								p.TextV(r.x + kPad + S(8.f), yy, kRowH, "Attenuation",
										NkRole::TextMuted);
								static const char *const kFal[8] = {
									"Lisse", "Sphere",	 "Racine",	 "Carre inverse",
									"Net",	 "Lineaire", "Constant", "Aleatoire"};
								Combo(p, hit, ws, "props.pe.f",
									  {r.x + S(120.f), yy + S(2.f), rr.w - S(128.f), kRowH - S(4.f)},
									  kFal, nullptr, 8, peF, combo);
								yy += kRowH;
								if (peR < 0.01f)
									peR = 0.01f;
								if (peR != r0 || peF != f0)
									demo::Demo3DHostSetPropEdit(peOn, peR, peF);
							}
						}
						if (st.tool == NkTool::Move || st.tool == NkTool::MultiGizmo) {
							if (Button("props.clr0", yy, "Remettre la translation", r.x + kPad,
									   rr.w - 2.f * kPad))
								demo::Demo3DHostClearXform(0);
							yy += kRowH;
						}
						if (st.tool == NkTool::Rotate || st.tool == NkTool::MultiGizmo) {
							if (Button("props.clr1", yy, "Remettre la rotation", r.x + kPad,
									   rr.w - 2.f * kPad))
								demo::Demo3DHostClearXform(1);
							yy += kRowH;
						}
						if (st.tool == NkTool::Scale || st.tool == NkTool::MultiGizmo) {
							if (Button("props.clr2", yy, "Remettre l'echelle", r.x + kPad,
									   rr.w - 2.f * kPad))
								demo::Demo3DHostClearXform(2);
							yy += kRowH;
						}
					}
						yy += NkGroupPad();
						PaintGroupBlock(p, rowR, grpToolTop, yy);
						yy += NkPropGroupGap();
					}
					// ── GROUPE « OUTIL D'EDITION », en mode Edit seulement.
					if (demo::Demo3DHostInEditMode()) {
						const bool grpEd = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.edt",
														  "Outil d'edition", 4096u);
						const float32 grpEdTop = yy;
						if (grpEd) {
						yy += NkGroupPad();
						// Memes marges internes que les autres groupes.
						const NkRect iE = NkGroupInner(rowR);
						const NkRect r{iE.x - kPad, rowR.y, iE.w + 2.f * kPad, rowR.h};
						const NkRect rr = r;
						const int32 m2 = demo::Demo3DHostEditSelMask();
						snprintf(buf, sizeof(buf), "Sous-mode : %s%s%s", (m2 & 1) ? "Sommets " : "",
								 (m2 & 2) ? "Aretes " : "", (m2 & 4) ? "Faces" : "");
						p.TextV(r.x + kPad, yy, kRowH, buf, NkRole::TextMuted);
						yy += kRowH;
						p.TextV(r.x + kPad, yy, kRowH, "E extruder   I inserer   Ctrl+B biseauter",
								NkRole::TextMuted);
						yy += kRowH;
						p.TextV(r.x + kPad, yy, kRowH, "Ctrl+R boucle   W subdiviser   K couteau",
								NkRole::TextMuted);
						yy += kRowH;
						yy += NkGroupPad();
						PaintGroupBlock(p, rowR, grpEdTop, yy);
						}
						yy += NkPropGroupGap();
					}
		}

		// ── PASTILLE « MODIFICATEUR » ───────────────────────────────────────────
		// Categorie encore A DEFINIR avec Rihen : elle porte pour l'instant la
		// pile de modificateurs de l'objet.
		inline void PaintPropModifier(NkModelerPainter &p, NkHitRegistry &hit, NkModelerState &st,
									  NkWidgetState &ws, const nkgui::NkGuiInput &in,
									  NkComboPending &combo, nkgui::NkGuiContext *guiCtx,
									  const NkRect &r, const NkRect &rr, float32 &yy) {
			auto Button = [&](const char *k2, float32 yB, const char *label, float32 x,
							  float32 w) -> bool {
				return NkPropButton(p, hit, k2, yB, label, x, w);
			};
			(void)Button;
			char key[64];
			char buf[160];
			(void)buf;
					// ── MODIFICATEUR (pastille « sliders-horizontal ») ──────────
					// La categorie reste A DEFINIR avec Rihen. En attendant, elle
					// HEBERGE les reglages de l'outil actif : ils etaient sur une
					// pastille supprimee par la maquette, et les perdre en silence
					// serait une regression. Ils demenageront a la refonte.
					// EN GROUPES REPLIABLES (Rihen), comme les autres categories.
					NkRect rowR = rr;
					rowR.x = r.x + NkPropInset();
					rowR.w = rr.w - 2.f * NkPropInset();
					const bool grpMod = PaintPropGroup(p, hit, st, rowR, yy, "prop.g.mod",
													   "Modificateurs", 1024u);
					const float32 grpModTop = yy;
					if (grpMod) {
						yy += NkGroupPad();
						const NkRect iM = NkGroupInner(rowR);
						// L'AJOUT vit ICI : le deroulant de la barre d'outils est
						// retire (regle de Rihen, la pastille suffit). Meme menu a
						// deux niveaux, ancre a ce bouton.
						{
							const NkRect mb{iM.x, yy + S(2.f), iM.w, kRowH - S(4.f)};
							const bool ovM = hit.Add("props.modadd", mb);
							const bool opM = ws.ComboOpen("tb.mod");
							p.Outline(mb, (ovM || opM) ? NkRole::AccentUi : NkRole::Border,
									  opM ? NkRole::AccentUi : NkRole::InputBg, 3.f);
							const char *lbAdd = "Ajouter un modificateur";
							float32 twA = p.TextW(lbAdd);
							if (twA > mb.w - S(6.f)) {
								lbAdd = "Ajouter";
								twA = p.TextW(lbAdd);
							}
							p.TextV(mb.x + (mb.w - twA) * 0.5f, yy, kRowH, lbAdd,
									opM ? NkRole::TextOnAccent : NkRole::Text);
							if (hit.Clicked("props.modadd")) {
								ws.ToggleCombo("tb.mod");
								st.modAnchor = mb;
							}
							yy += kRowH;
						}
						p.TextV(iM.x, yy, kRowH, "Aucun modificateur pose.",
								NkRole::TextMuted);
						yy += kRowH;
						yy += NkGroupPad();
						PaintGroupBlock(p, rowR, grpModTop, yy);
					}
					yy += NkPropGroupGap();
		}

		// ── MODALE « AJOUTER UN MATERIAU » ──────────────────────────────────────
		// PEINTE DEPUIS main.cpp, avec les SURCOUCHES -- pas dans le panneau de
		// proprietes. La raison est l'etancheite : main.cpp vide l'input avant de
		// peindre les panneaux (une modale ouverte ne doit rien laisser passer) et
		// ne le rend qu'aux surcouches. Peinte parmi les panneaux, cette modale ne
		// recevait donc rien, tandis que la vue 3D -- qui lit l'input DIRECTEMENT --
		// recevait tout : cliquer sa barre de titre deselectionnait l'objet
		// (Rihen, 13 aout).
		inline void PaintMatAddModal(NkModelerState &st, NkHitRegistry &hit, NkWidgetState &ws,
									 const nkgui::NkGuiInput &in, NkComboPending &combo,
									 nkgui::NkGuiContext *guiCtx) {
			(void)combo;
			(void)guiCtx;
			if (st.matAddOpen) {
				// ── LE CADRE VIENT DU KIT (NKEditorKit), PLUS DE CODE MAISON ────
				// Voile, boite, barre de titre designee, deplacement, croix de
				// fermeture, modalite globale et occlusion : tout cela vivait ici,
				// ecrit a la main, et m'a coute quatre correctifs rates le 12 aout.
				// `NkModalFrameDraw` le fournit -- et il empile SANS cacher la
				// modale du dessous, le voile n'etant pose qu'une fois par pile
				// (Rihen, 13 aout).
				NkHitRegistry::LayerScope matModalLayer(hit, 100);
				const int32 actM = demo::Demo3DHostActiveObject() >= 0
									   ? demo::Demo3DHostActiveObject()
									   : st.activeEmpty;
				st.matAddModal.open = true;
				nkgui::NkGuiContext *gcM = NkUiCtx();
				NkModelerPainter *poM = NkOvPainter();
				if (!gcM || !poM) {
					NkMatAddSetOpen(st, false);
					return;
				}
				const float32 dw = S(300.f), dh = S(200.f);
				// LE SELECTEUR S'EMPILE PAR-DESSUS, il ne remplace pas : tant qu'il
				// est ouvert, cette modale reste VISIBLE mais inerte. « Nouveau » ne
				// la referme donc plus -- on revient dessus en annulant (Rihen,
				// 13 aout : « pourquoi quand on clique sur Nouveau ca ferme le
				// dialogue du bouton + ? »).
				const bool sousLeSelecteur = st.picker.pickerOpen;
				editorkit::NkModalFrame fr =
					editorkit::NkModalFrameDraw(*gcM, st.matAddModal, "Ajouter un materiau",
												dw, dh, editorkit::NkModalStyle{},
												sousLeSelecteur);
				if (!fr.visible || fr.closeAsked) {
					NkMatAddSetOpen(st, false);
					return;
				}
				// LE CONTENU SE PEINT SUR LA COUCHE OVERLAY, au-dessus du cadre :
				// le peintre ordinaire ecrit dans `dl`, soumise AVANT `dlOverlay`,
				// donc son trace passerait DERRIERE la boite. Ce `p` local masque
				// volontairement le parametre pour toute la duree du bloc.
				// L'EMPRISE DE LA BOITE, DECLAREE DANS LE REGISTRE DU MODELEUR.
				// Le cadre est dessine par le kit, qui a SON propre systeme
				// d'entrees : sa barre de titre n'existe pas pour `hit`. Sans cette
				// zone, un clic sur l'entete ne rencontrait aucune surface de couche
				// 100 et repartait vers ce qui se trouve dessous -- « ce qui laisse
				// traverser les evenements, c'est l'entete » (Rihen, 13 aout).
				// Declaree AVANT le contenu : les widgets poses ensuite la couvrent.
				(void)hit.Add("props.pm.modalbox", fr.box);

				NkModelerPainter &p = *poM;
				// LA BOITE, pas la zone de contenu : le contenu de cette modale
				// place deja ses elements sous une hauteur de titre (`dr.y + kRowH`).
				// Lui donner `fr.content`, qui commence DEJA sous le titre, l'aurait
				// decale d'une ligne vers le bas.
				const NkRect dr = fr.box;

				// DEUX BOUTONS COTE A COTE.
				const float32 bw2 = (dr.w - S(18.f)) * 0.5f;
				const NkRect nb{dr.x + S(6.f), dr.y + kRowH + S(4.f), bw2, S(24.f)};
				const NkRect adb{nb.x + bw2 + S(6.f), nb.y, bw2, S(24.f)};
				const bool ovN = hit.Add("props.pm.newp", nb);
				p.Outline(nb, ovN ? NkRole::AccentUi : NkRole::Border, NkRole::PanelHeader,
						  3.f);
				p.IconV(nb.x + S(5.f), nb.y, nb.h, NkIcon::PlusCircle, NkRole::Text, 11.f);
				p.TextV(nb.x + S(22.f), nb.y, nb.h, "Nouveau", NkRole::Text);
				// « NOUVEAU » OUVRE LE DIALOGUE D'EMPLACEMENT (Rihen, 12 aout) :
				// on ne cree plus a l'aveugle dans le dossier courant sous un nom
				// serialise — l'utilisateur choisit ou et comment.
				if (hit.Clicked("props.pm.newp") && actM >= 0) {
					NkFileDialogDesc fd;
					fd.title = "Nouveau materiau";
					fd.forcedExt = "nkmat";	 // l'extension ne se tape pas
					fd.suggested = "Materiau";
					fd.okLabel = "Creer";
					(void)fd;
					// Le selecteur de NKEditorKit, celui de NKCode : il flotte sur TOUTE
					// la fenetre (il est peint depuis main.cpp, hors des panneaux) et se
					// confine tout seul a la racine du projet.
					// On demarre dans le dossier COURANT du navigateur (Rihen), et on
					// se replie sur la racine tant qu'il n'existe pas sur le disque.
					const NkString &dep =
						st.browserFolderAbs.Empty() ? st.projectRoot : st.browserFolderAbs;
					st.picker.OpenPickerBase(editorkit::NkFilePickerState::PK_SaveFile,
											 dep.CStr(), nullptr, 0, st.projectRoot.CStr());
					editorkit::NkFilePickerState::CopyTo(st.picker.pickerSaveName, "Materiau",
														 (int32)sizeof(st.picker.pickerSaveName));
					// LE TYPE SE CHOISIT AVANT LA CREATION (Rihen, 13 aout) : le
					// selecteur ajoute sa rangee de types sous le nom. Arme APRES
					// l'ouverture -- `OpenPickerBase` reinitialise l'etat de base et
					// n'a aucune raison de connaitre le mode creation de materiau.
					st.picker.MatNewBegin();
					st.pickerAction = 1;	   // 1 = creer un materiau
					st.matNewPending = true;
					// LA MODALE RESTE OUVERTE, SOUS LE SELECTEUR. Elle passe simplement
					// en inerte (cf. plus haut) : on la retrouve intacte en annulant,
					// au lieu de repartir du bouton « + ».
				}
				// « Ajouter » ne s'allume qu'une fois une ligne CHOISIE.
				const bool okAdd = (st.matAddSel >= 0);
				const bool ovAd = hit.Add("props.pm.addp", adb);
				p.Outline(adb, (ovAd && okAdd) ? NkRole::AccentUi : NkRole::Border,
						  NkRole::PanelHeader, 3.f);
				p.IconV(adb.x + S(5.f), adb.y, adb.h, NkIcon::Add,
						okAdd ? NkRole::Text : NkRole::Border, 11.f);
				p.TextV(adb.x + S(22.f), adb.y, adb.h, "Ajouter",
						okAdd ? NkRole::Text : NkRole::TextMuted);
				if (okAdd && hit.Clicked("props.pm.addp") && actM >= 0) {
					(void)demo::Demo3DHostNodeMatAdd(actM, st.matAddSel);
					NkMatAddSetOpen(st, false);
					NkMarkDirty(st);
				}

				// LA LISTE des materiaux du projet ABSENTS de cet objet.
				const NkRect lb{dr.x + S(6.f), nb.y + nb.h + S(6.f), dr.w - S(12.f),
								dr.y + dr.h - (nb.y + nb.h) - S(12.f)};
				p.Fill(lb, NkRole::InputBg, 3.f);
				p.OutlineSharp(lb, NkRole::Border);
				p.Clip(lb);
				hit.PushClip(lb);
				const float32 aLineH = S(20.f);
				float32 ay = lb.y + S(2.f) - st.matAddScrollY;
				int32 nCand = 0;
				float32 wMax = 0.f;
				for (int32 mi = 0; mi < 64; ++mi) {
					char nm4[64];
					if (!demo::Demo3DHostProjMatInfo(mi, nm4, sizeof(nm4), nullptr, nullptr,
													 nullptr))
						continue;
					bool deja = false;
					const int32 nOb2 = (actM >= 0) ? demo::Demo3DHostNodeMatCount(actM) : 0;
					for (int32 k = 0; k < nOb2 && !deja; ++k)
						deja = (demo::Demo3DHostNodeMatAt(actM, k) == mi);
					if (deja)
						continue;
					++nCand;
					usize lenN = 0;
					while (nm4[lenN])
						++lenN;
					const float32 wN = (float32)lenN * S(7.f) + S(28.f);
					if (wN > wMax)
						wMax = wN;
					if (ay + aLineH >= lb.y && ay <= lb.y + lb.h) {
						char ck[28];
						snprintf(ck, sizeof(ck), "props.pm.cand.%d", mi);
						const NkRect cr{lb.x, ay, lb.w, aLineH};
						const bool ovC = hit.Add(ck, cr);
						if (st.matAddSel == mi)
							p.Fill(cr, NkRole::AccentUi, 3.f);
						else if (ovC)
							p.Fill(cr, NkRole::PanelHeader, 3.f);
						p.TextV(cr.x + S(6.f) - st.matAddScrollX, ay, aLineH, nm4,
								NkRole::Text);
						// Selectionner ALLUME « Ajouter » ; c'est lui qui valide.
						if (hit.Clicked(ck))
							st.matAddSel = mi;
					}
					ay += aLineH;
				}
				if (nCand == 0)
					p.TextV(lb.x + S(6.f), lb.y + S(2.f), aLineH,
							"aucun autre materiau dans le projet", NkRole::TextMuted);
				hit.PopClip();
				p.Unclip();

				// LES DEUX BARRES : verticale si la liste deborde, horizontale si
				// un nom est plus large que la colonne.
				hit.Add("props.pm.candlist", lb);
				const float32 contH = (float32)nCand * aLineH + S(4.f);
				if (hit.IsHovered("props.pm.candlist") && in.wheel != 0.f)
					st.matAddScrollY -= in.wheel * aLineH;
				const float32 maxY = contH > lb.h ? contH - lb.h : 0.f;
				if (st.matAddScrollY > maxY)
					st.matAddScrollY = maxY;
				if (st.matAddScrollY < 0.f)
					st.matAddScrollY = 0.f;
				const float32 maxX = wMax > lb.w ? wMax - lb.w : 0.f;
				if (st.matAddScrollX > maxX)
					st.matAddScrollX = maxX;
				if (st.matAddScrollX < 0.f)
					st.matAddScrollX = 0.f;
				if (maxY > 0.f)
					p.Fill({lb.x + lb.w - S(4.f), lb.y + (st.matAddScrollY / contH) * lb.h,
							S(3.f), lb.h * (lb.h / contH)},
						   NkRole::TextMuted, 2.f);
				if (maxX > 0.f)
					p.Fill({lb.x + (st.matAddScrollX / wMax) * lb.w, lb.y + lb.h - S(4.f),
							lb.w * (lb.w / wMax), S(3.f)},
						   NkRole::TextMuted, 2.f);

				// PLUS DE CADRE REPASSE NI DE VOILE JUGE EN DERNIER : le kit dessine
				// le cadre au-dessus (couche overlay) et traite lui-meme le clic
				// « a cote », Echap et la croix -- avec sa propre garde contre le
				// clic d'ouverture. Ces trois rustines maison etaient les cicatrices
				// des correctifs du 12 aout ; elles n'ont plus d'objet.
				st.matAddJustOpened = false;
			}
		}

		inline void PaintPropertiesUnified(NkModelerPainter &p, const NkRect &rFull,
										   NkModelerState &st, NkHitRegistry &hit, NkWidgetState &ws,
										   const nkgui::NkGuiInput &in, NkComboPending &combo,
										   nkgui::NkGuiContext *guiCtx = nullptr) {
			p.Fill(rFull, NkRole::PanelBg);
			p.VLine(rFull.x, rFull.y, rFull.h);
			// ── LA LISTE OUVERTE D'UN COMBO BLOQUE CE PANNEAU ───────────────
			// Elle est peinte APRES lui, par-dessus ses widgets : sans cette
			// garde, cliquer une entree atteignait AUSSI le widget situe
			// dessous, et le choix etait aussitot ecrase -- « je n'arrive pas a
			// choisir un format », « ca ne doit pas laisser traverser les
			// evenements » (Rihen). L'emprise est celle memorisee a l'image
			// precedente (patron UiBlockAdd/UiBlocks, deja employe par la
			// hierarchie et le navigateur) ; ce panneau lui manquait, alors
			// qu'il est celui qui porte le plus de listes.
			if (st.UiBlocks(hit.Mouse().x, hit.Mouse().y))
				hit.SetBlock({hit.Mouse().x - 1.f, hit.Mouse().y - 1.f, 2.f, 2.f}, true);
			// Editeurs sans design defini : pas de proprietes (Rihen).
			{
				const uint8 tkP = st.sceneTabKind[st.activeTab];
				if (tkP != 0 && tkP != 7) {
					p.TextV(rFull.x + S(12.f), rFull.y + S(6.f), kRowH,
							"Indisponible pour cet editeur", NkRole::TextMuted);
					return;
				}
			}
			// PLUS DE CROIX : les PASTILLES font l'affichage/masquage -- aucune
			// active, et le panneau n'est plus que sa colonne de pastilles.
			const bool collapsed = !st.AnyPropOpen();
			float32 y;
			if (collapsed) {
				y = rFull.y + S(6.f);
			} else {
				p.Fill({rFull.x, rFull.y, rFull.w, kRowH}, NkRole::PanelHeader);
				// L'EN-TETE PORTE LA CATEGORIE entre parentheses (Rihen) : une
				// seule pastille etant active, un second en-tete « Modele » sous
				// celui-ci repetait l'information et volait une ligne au contenu.
				{
					// SEPT sections de base depuis qu'Output existe : cette table
					// etait restee a six, et l'indice 6 -- Output -- tombait dans
					// la branche « pastille du mode ». L'en-tete n'affichait donc
					// aucun nom pour Output (constate par Rihen). La pastille du
					// MODE est desormais la 8e, comme partout ailleurs.
					static const char *const kHdrNames[7] = {"Modele",	 "Rendu",	 "Scene",
															 "Modificateur", "Materiau", "Outil",
															 "Output"};
					static const char *const kHdrMode[6] = {
						"Edition",	 "Sculpture 2.5D", "Sculpture",
						"Texturing", "Patron",		   "Texture painting"};
					int32 actSec = -1;
					for (int32 i9 = 0; i9 < 8; ++i9)
						if (st.propOpen[i9]) {
							actSec = i9;
							break;
						}
					char hd[64];
					if (actSec == 7 && (int32)st.mode >= 1 && (int32)st.mode <= 6)
						snprintf(hd, sizeof(hd), "Proprietes (%s)",
								 kHdrMode[(int32)st.mode - 1]);
					else if (actSec >= 0 && actSec < 7)
						snprintf(hd, sizeof(hd), "Proprietes (%s)", kHdrNames[actSec]);
					else
						snprintf(hd, sizeof(hd), "Proprietes");
					p.TextV(rFull.x + kPad, rFull.y, kRowH, hd);
					// FLECHE DE REPLI (Rihen : une fleche, pas une croix — et elle
					// manquait sur CET en-tete : PaintPanelTab n'est pas utilise ici).
					{
						const NkRect cr9{rFull.x + rFull.w - S(24.f), rFull.y + S(3.f),
										 S(20.f), kRowH - S(6.f)};
						const bool ov9 = hit.Add("prop.fold", cr9);
						if (ov9)
							p.Fill(cr9, NkRole::AccentUi, 2.f);
						p.IconV(cr9.x + S(4.f), rFull.y, kRowH, NkIcon::ChevronRight,
								ov9 ? NkRole::TextOnAccent : NkRole::Text, 11.f);
						if (hit.Clicked("prop.fold"))
							st.showRight = false;
					}
				}
				p.HLine(rFull.x, rFull.y + kRowH - 1.f, rFull.w);
				y = rFull.y + kRowH;
			}
			// LA COLONNE DE PASTILLES (idee de Rihen, facon Blender) reserve le
			// bord droit ; tout le reste du panneau travaille dans r, ampute
			// d'autant. Entre les deux, la SCROLLBAR de NKEditorKit -- celle de
			// l'editeur de code -- est TOUJOURS visible (Rihen) : une barre qui
			// n'apparait qu'au besoin fait sauter la mise en page et laisse
			// douter qu'il y ait quelque chose plus bas.
			const float32 kSbW = editorkit::NkScrollbarWidth();
			NkRect r = rFull;
			r.w -= S(26.f) + kSbW;
			char key[40], buf[96];

			// Etait une lambda locale : elle ne capturait que le peintre et le
			// registre, donc elle n'avait pas a etre prisonniere de cette fonction.
			// Devenue libre (NkPropButton), elle sert aussi aux pastilles extraites.
			// L'alias garde intacts ses dizaines d'appels existants.
			auto Button = [&](const char *k2, float32 yB, const char *label, float32 x,
							  float32 w) -> bool {
				return NkPropButton(p, hit, k2, yB, label, x, w);
			};

			static float32 sContentH[8] = {200.f, 260.f, 200.f, 200.f,
										   200.f, 200.f, 200.f, 200.f};
			// ── LA TABLE DES CATEGORIES ─────────────────────────────────────
			// EN AJOUTER UNE = une entree ici (titre + icone de pastille) + son
			// contenu dans le switch plus bas. Pastilles, pliage, defilement et
			// hauteurs suivent la table sans autre code.
			struct NkPropSec {
					const char *title;
					NkIcon icon;
			};
			// LES CATEGORIES de la maquette (Rihen, dessin Banani), avec leurs
			// icones : Modele (box), Rendu (sun), Scene (layers), Modificateur
			// (sliders-horizontal), Materiau (sphere). UNE SEULE est active a la
			// fois.
			// OUTIL a sa PROPRE pastille (Rihen) : les reglages de l'outil actif
			// etaient HEBERGES par Modificateur depuis qu'une pastille de la
			// maquette avait disparu -- un hebergement annonce comme provisoire
			// dans le code. Ils rejoignent leur place, comme l'onglet « Tool »
			// de Blender ; Modificateur ne garde que les modificateurs.
			// OUTPUT (Rihen) : ce qui SORT de la scene -- resolution, source,
			// destination, et les incrustations posees sur l'image principale.
			// Rendu dit COMMENT la scene est eclairee, Output dit CE QU'ON EN
			// PRODUIT : deux questions distinctes, deux pastilles.
			// Elle est ajoutee EN FIN de liste, pas apres Rendu ou sa place
			// serait plus logique : les indices de section sont memorises dans
			// st.propOpen et cables en dur ailleurs (kSelOnly, la pastille du
			// mode) -- les decaler casserait ces regles en silence.
			static const NkPropSec kSecsBase[7] = {{"Modele", NkIcon::Cube3D},
												   {"Rendu", NkIcon::Sun},
												   {"Scene", NkIcon::Layers},
												   {"Modificateur", NkIcon::SlidersH},
												   {"Materiau", NkIcon::Material},
												   {"Outil", NkIcon::Move},
												   {"Output", NkIcon::Camera}};
			// ── LA PASTILLE DU MODE (regle de Rihen) : chaque mode hors Objet a
			// SA pastille, unique a lui, qui n'existe QUE dans ce mode -- ses
			// fonctions s'y rempliront progressivement, par categories. Une
			// seule entree (indice 5) dont le visage suit le mode courant.
			static const NkPropSec kModeSecs[6] = {{"Edition", NkIcon::Edit},
												   {"Sculpture 2.5D", NkIcon::Layers},
												   {"Sculpture", NkIcon::Ruler},
												   {"Texturing", NkIcon::Overlay},
												   {"Patron", NkIcon::ViewUV},
												   {"Texture painting", NkIcon::Picker}};
			const int32 modeIdx5 = (int32)st.mode; // 0 = Objet
			NkPropSec kSecs[8];
			for (int32 i2 = 0; i2 < 7; ++i2)
				kSecs[i2] = kSecsBase[i2];
			const int32 kNSec = modeIdx5 > 0 ? 8 : 7;
			if (modeIdx5 > 0)
				kSecs[7] = kModeSecs[modeIdx5 - 1]; // la pastille du MODE, en dernier
			// ENTRER dans un mode ACTIVE sa pastille -- mais SEULEMENT si le
			// panneau etait deja ouvert : ferme, il le reste (Rihen -- «
			// mettre sa pastille mais pas ouvrir le panneau s'il etait ferme »).
			// En SORTIR : la pastille disparait, et si elle etait l'active la
			// main revient a Modele (qui suit sa propre regle de selection).
			{
				static int32 sLastMode5 = -1;
				if (sLastMode5 != modeIdx5) {
					if (sLastMode5 >= 0) {
						bool anyOpen5 = false;
						for (int32 j2 = 0; j2 < 8; ++j2)
							anyOpen5 = anyOpen5 || st.propOpen[j2];
						if (modeIdx5 > 0) {
							if (anyOpen5) {
								for (int32 j2 = 0; j2 < 8; ++j2)
									st.propOpen[j2] = false;
								st.propOpen[7] = true;
							}
						} else if (st.propOpen[7]) {
							st.propOpen[7] = false;
							st.propOpen[0] = true;
						}
					}
					sLastMode5 = modeIdx5;
				}
			}
			if (modeIdx5 == 0 && st.propOpen[7]) {
				st.propOpen[7] = false;
				st.propSecH[7] = 0.f;
				st.propScroll3[7] = 0.f;
			}
			// LA PASTILLE MODELE N'EXISTE QUE POUR UNE SELECTION (regle de
			// Rihen) : sans objet actif elle disparait de la colonne, et si
			// elle etait l'active le panneau se replie -- un panneau Modele
			// sans modele n'aurait rien d'honnete a montrer.
			const bool hasSel5 = st.activeEmpty >= 0 ||
								 demo::Demo3DHostActiveObject() >= 0 ||
								 demo::Demo3DHostSelectedLight() >= 0;
			// LA MATIERE EST UNE AFFAIRE DE SURFACE : une lumiere n'en porte
			// pas (Rihen, 11 aout — « pourquoi une lumiere a un material ? »).
			// Sa propre couleur/texture d'emission vit dans SON panneau.
			// On RETIRE donc la pastille pour une lumiere, sans rien changer
			// aux autres cas : exiger gizmo.ActiveIndex() >= 0 l'avait fait
			// disparaitre aussi pour un cube selectionne depuis la hierarchie
			// (constate par Rihen dans la foulee).
			const bool hasObj5 = hasSel5 && demo::Demo3DHostSelectedLight() < 0;
			// MEME REGLE POUR MATERIAU ET MODIFICATEUR (Rihen) : sans objet, il
			// n'y a ni matiere a assigner ni modificateur a poser -- leurs
			// pastilles disparaissent comme celle de Modele, et si l'une etait
			// active le panneau se replie.
			// PLUS AUCUN AUTOMATISME (regle de Rihen, 11 aout) : ouvert, on
			// ferme A LA MAIN ; ferme, on ouvre A LA MAIN ; selectionner un
			// objet n'ouvre rien. Les sections Modele/Modificateur montrent
			// « Aucune selection » quand il n'y a personne — honnete et stable.
			// UNE SECTION ORPHELINE CEDE LA PLACE (Rihen, 12 aout : « le mieux
			// est de basculer sur une autre pastille presente »). Modele (0),
			// Modificateur (3) et Materiau (4) perdent leur pastille sans
			// selection ; les laisser ouvertes affichait un panneau dont
			// l'onglet n'existait plus. On bascule alors sur la premiere
			// section TOUJOURS disponible — jamais sur une autre orpheline.
			// Ce n'est pas un automatisme d'ouverture (regle du 11 aout) : le
			// panneau reste ouvert, seul son CONTENU change.
			for (int32 i2 = 0; i2 < kNSec; ++i2) {
				const bool orphelin = ((i2 == 0 || i2 == 3) && !hasSel5) ||
									  (i2 == 4 && !hasObj5);
				if (!orphelin || !st.propOpen[i2])
					continue;
				st.propOpen[i2] = false;
				bool reste = false;
				for (int32 k2 = 0; k2 < kNSec && !reste; ++k2)
					reste = st.propOpen[k2];
				if (reste)
					continue; // une autre section tient deja l'affiche
				for (int32 k2 = 0; k2 < kNSec; ++k2) {
					const bool orph2 = ((k2 == 0 || k2 == 3) && !hasSel5) ||
									   (k2 == 4 && !hasObj5);
					if (!orph2) {
						st.propOpen[k2] = true;
						st.propFold[k2] = false;
						break;
					}
				}
			}
			int32 nOpen = 0, nUnfold = 0;
			for (int32 i2 = 0; i2 < kNSec; ++i2)
				if (st.propOpen[i2]) {
					++nOpen;
					if (!st.propFold[i2])
						++nUnfold;
				}
			// Les en-tetes affiches (sections actives) sont deduits ; la place
			// restante se partage entre les sections DEPLIEES.
			const float32 availH = (r.y + r.h) - y - (float32)(nOpen > 0 ? nOpen : 1) * kRowH;
			// La gouttiere du scrollbar est DEJA retranchee de r (voir plus haut) :
			// en retirer une seconde fois laissait une large bande morte a droite
			// du contenu (constate par Rihen).
			const NkRect rr{r.x, 0.f, r.w, 0.f};
			// DEFILEMENT GLOBAL : la pile entiere glisse de propScroll ; le
			// contenu est mesure au fil de la peinture et la barre du bord la
			// pilote (celles des sections sont inserees).
			const float32 stackTop = y;
			p.Clip({r.x, stackTop, r.w, (r.y + r.h) - stackTop});
			float32 secY = y - st.propScroll;

			bool anyWheel = false;
			for (int32 sec = 0; sec < kNSec; ++sec) {
				// PASTILLE DECOCHEE = SECTION RETIREE de la liste (Rihen) : ni
				// contenu NI en-tete -- la colonne de pastilles est le seul moyen
				// de la faire revenir.
				if (!st.propOpen[sec])
					continue;
				snprintf(key, sizeof(key), "props.sec.%d", sec);
				// Le CHEVRON plie/deplie ; il ne retire jamais la section de la
				// liste (ca, c'est la pastille). Et AUCUN clic d'en-tete ne
				// compte pendant un glissement : relacher la POIGNEE sur
				// l'en-tete voisin basculait le chevron (constate par Rihen).
				// PLUS D'EN-TETE DE CATEGORIE ICI : il repetait le titre du panneau,
				// qui porte deja la categorie entre parentheses (Rihen). Le contenu
				// commence donc directement par ses propres sections, comme sur la
				// maquette. La section reste depliee -- son pliage se ferait au
				// niveau de chaque bloc interne.
				st.propFold[sec] = false;
				// REPARTITION EN DEUX PASSES : les sections plus petites que leur
				// part rendent l'espace, redistribue aux plus grandes -- une
				// section n'a de defilement local que quand l'ESPACE TOTAL manque
				// (une petite section en dessous ne doit pas figer la part des
				// autres, constate par Rihen).
				float32 boxH;
				{
					float32 want[8];
					bool alloc[8] = {};
					float32 given[8] = {};
					// La hauteur CHOISIE a la poignee est INTOUCHABLE : allouee
					// telle quelle, meme si le total deborde -- c'est alors le
					// DEFILEMENT GLOBAL qui prend le relais. La plafonner a la
					// part egale (version precedente) rendait la poignee inerte
					// et eteignait la barre generale (constate par Rihen). Les
					// deux passes ne repartissent que les sections AUTO.
					float32 remaining = availH;
					int32 hungry = 0;
					for (int32 j = 0; j < kNSec; ++j) {
						if (!st.propOpen[j] || st.propFold[j])
							continue;
						if (st.propSecH[j] > 0.f) {
							given[j] = st.propSecH[j];
							alloc[j] = true;
							remaining -= given[j];
						} else {
							want[j] = sContentH[j];
							++hungry;
						}
					}
					if (remaining < 0.f)
						remaining = 0.f;
					for (int32 pass = 0; pass < 3 && hungry > 0; ++pass) {
						const float32 sh = remaining / (float32)hungry;
						bool moved = false;
						for (int32 j = 0; j < kNSec; ++j) {
							if (alloc[j] || !st.propOpen[j] || st.propFold[j])
								continue;
							if (want[j] <= sh) {
								given[j] = want[j];
								alloc[j] = true;
								remaining -= want[j];
								--hungry;
								moved = true;
							}
						}
						if (!moved)
							break;
					}
					const float32 shFinal = hungry > 0 ? remaining / (float32)hungry : 0.f;
					for (int32 j = 0; j < kNSec; ++j)
						if (!alloc[j])
							given[j] = shFinal;
					boxH = given[sec];
				}
				if (boxH < kRowH)
					boxH = kRowH;
				// UNE SEULE SECTION A LA FOIS : elle prend TOUTE la hauteur, et
				// c'est la barre generale -- celle de NKEditorKit, toujours
				// visible -- qui la fait defiler. Lui donner une part et une barre
				// a elle revenait a decouper un seul contenu en deux gestes de
				// defilement (Rihen).
				boxH = (r.y + r.h) - secY;
				if (boxH < kRowH)
					boxH = kRowH;
				st.propScroll3[sec] = 0.f;
				// PAS de plafond calcule sur la position DEFILEE : il creait une
				// retroaction (descendre allongeait la derniere section, donc la
				// pile, donc le defilement...) -- c'etait le CLIGNOTEMENT de la
				// barre generale constate par Rihen. L'exces de hauteur est
				// l'affaire du defilement de page, pas d'un plafond.
				const NkRect box{r.x, secY, r.w, boxH};
				snprintf(key, sizeof(key), "props.body.%d", sec);
				hit.Add(key, box);
				p.Clip(box);
				hit.PushClip(box); // les zones suivent le dessin : rien d'invisible
				// UNE RESPIRATION EN HAUT DE CHAQUE SECTION (Rihen) : Modele,
				// Rendu, Scene et Modificateur. Le premier groupe collait au
				// bandeau du panneau, ce qui donnait l'impression qu'il en faisait
				// partie. Pose ICI, au point de depart commun aux quatre : une
				// marge ajoutee section par section aurait fini par diverger.
				float32 yy = secY - st.propScroll3[sec] + S(8.f);

				// MATERIAU : sa pastille exige un OBJET (une lumiere n'a pas de
				// surface). Elle disparaissait donc a la deselection, mais le
				// PANNEAU restait affiche — une section dont l'onglet n'existe
				// plus (Rihen, 12 aout). Il le DIT desormais, comme Modele et
				// Modificateur. Pas de fermeture automatique : le panneau
				// n'obeit qu'a la main, regle posee le 11 aout.
				const bool secOrphelin =
					((sec == 0 || sec == 3) && !hasSel5) || (sec == 4 && !hasObj5);
				if (secOrphelin) {
					p.TextV(r.x + NkPropInset(), yy, kRowH, "Aucune selection",
							NkRole::TextMuted);
					yy += kRowH;
				} else if (sec == 0) {
					PaintPropObject(p, hit, st, ws, in, combo, guiCtx, r, rr, yy);
				} else if (sec == 1) {
					PaintPropWorld(p, hit, st, ws, in, combo, guiCtx, r, rr, yy);
				} else if (sec == 2) {
					PaintPropScene(p, hit, st, ws, in, combo, guiCtx, r, rr, yy);
				} else if (sec == 3) {
					PaintPropModifier(p, hit, st, ws, in, combo, guiCtx, r, rr, yy);
				} else if (sec == 5) {
					PaintPropTool(p, hit, st, ws, in, combo, guiCtx, r, rr, yy);
				} else if (sec == 4) {
					PaintPropMaterial(p, hit, st, ws, in, combo, guiCtx, r, rr, yy);
				} else if (sec == 6) {
					PaintPropOutput(p, hit, st, ws, in, combo, guiCtx, r, rr, yy);
				} else if (sec == 7) {
					PaintPropMode(p, hit, st, ws, in, combo, guiCtx, r, rr, yy);
				}

				// La hauteur du contenu sert desormais a la SEULE barre generale :
				// la molette s'y applique donc directement, sans defilement local.
				sContentH[sec] = yy - secY + S(4.f);
				hit.PopClip();
				p.Unclip();
				// PLUS DE BARRE PAR SECTION (Rihen) : une seule pastille est active,
				// donc une seule section occupe le panneau -- c'est la barre
				// GENERALE qui doit la faire defiler. Une seconde barre a
				// l'interieur decoupait le defilement en deux gestes pour un seul
				// contenu.
				// La pile avance de ce que le contenu occupe REELLEMENT : c'est lui
				// que la barre generale doit pouvoir parcourir, pas la hauteur du
				// cadre (qui vaut maintenant tout le panneau).
				secY += sContentH[sec];
				// ── POIGNEE DE HAUTEUR : agrandir/retrecir CETTE section ────
				// Le geste appartient a la poignee ou il a commence (propDragKey),
				// comme la barre de defilement.
				// La poignee n'existe que si une AUTRE section OUVERTE suit : au
				// bas de la DERNIERE elle flottait en lisere fantome sur le vide
				// (capture de Rihen) -- la derniere remplit toujours l'espace.
				bool hasNextOpen = false;
				for (int32 j2 = sec + 1; j2 < kNSec; ++j2)
					if (st.propOpen[j2] && !st.propFold[j2])
						hasNextOpen = true;
				if (!hasNextOpen)
					st.propSecH[sec] = 0.f;
				if (hasNextOpen) {
					snprintf(key, sizeof(key), "props.div.%d", sec);
					// Entierement DANS le bas de la boite : elle mordait sur
					// l'en-tete suivant, et viser l'un declenchait l'autre.
					const NkRect dv{r.x, secY - S(6.f), r.w - S(14.f), S(6.f)};
					const bool overD = hit.Add(key, dv);
					const bool mineD = (strcmp(st.propDragKey, key) == 0);
					if (overD || mineD) {
						hit.WantCursor(NkCursorWant::ResizeNS);
						p.Fill({r.x, secY - S(2.f), r.w, S(3.f)}, NkRole::AccentUi);
					}
					if (hit.MouseDown() && (overD || mineD)) {
						if (!st.propDragKey[0] && overD)
							snprintf(st.propDragKey, sizeof(st.propDragKey), "%s", key);
						if (strcmp(st.propDragKey, key) == 0) {
							float32 nh = hit.Mouse().y - box.y;
							if (nh < kRowH * 2.f)
								nh = kRowH * 2.f;
							st.propSecH[sec] = nh;
						}
					}
				}
			}
			p.Unclip();
			{
				const float32 stackH = (secY + st.propScroll) - stackTop;
				const float32 viewH = (r.y + r.h) - stackTop;
				const float32 maxOff = stackH > viewH ? stackH - viewH : 0.f;
				if (st.propScroll > maxOff)
					st.propScroll = maxOff;
				// Molette de PAGE : quand aucune section ne l'a consommee (souris
				// sur un en-tete, une poignee, ou du vide), c'est la pile entiere
				// qui defile.
				if (!anyWheel)
					hit.WheelIn({r.x, stackTop, r.w, viewH}, st.propScroll, stackH, viewH);
				// LA SCROLLBAR STANDARD de NKEditorKit -- la meme que l'editeur de
				// code (Rihen). Elle occupe sa gouttiere entre le contenu et la
				// colonne de pastilles, et reste VISIBLE meme quand tout tient a
				// l'ecran : une barre qui va et vient fait sauter la mise en page.
				// AUCUNE pastille active = pas de contenu, donc PAS DE BARRE : le
				// panneau se reduit a sa colonne de pastilles (Rihen). Une
				// gouttiere seule, sans rien a faire defiler, n'annonce rien.
				if (guiCtx && !collapsed) {
					const NkRect sbTrack{r.x + r.w, stackTop, kSbW, viewH};
					editorkit::NkVScrollbar(*guiCtx, guiCtx->dl, sbTrack, st.propScroll,
											stackH > viewH ? stackH : viewH + 1.f, viewH,
											0x4E4B5000u, kRowH);
				} else {
					NkScrollDrag(p, hit, st, "props.outer", {r.x, stackTop, r.w, viewH},
								 stackH, st.propScroll);
				}
			}
			// ── LES PASTILLES : une par section, a droite. BLEUE = active.
			// UNE SEULE A LA FOIS (regle de Rihen) : le panneau montre les
			// proprietes de LA categorie choisie, et rien d'autre. Les ouvrir
			// ensemble revenait a empiler des blocs sans rapport et a rogner la
			// place de chacun -- illisible des que les categories se comptent en
			// dizaines. Recliquer la pastille active replie le panneau.
			{
				// La colonne commence APRES la gouttiere de la scrollbar : posee au
				// meme x, elle recouvrait la barre (constate par Rihen). Le trait
				// separateur se place de meme, entre la barre et les pastilles.
				const float32 tabX = r.x + r.w + kSbW;
				p.VLine(tabX, stackTop, (rFull.y + rFull.h) - stackTop);
				float32 ty = stackTop + S(4.f);
				for (int32 i2 = 0; i2 < kNSec; ++i2) {
					// Les pastilles LIEES A L'OBJET (Modele, Modificateur) se retirent
					// sans selection (Rihen, 11 aout). Le panneau, lui, n'obeit
					// qu'a la main.
					if ((i2 == 0 || i2 == 3) && !hasSel5)
						continue;
					// MATERIAU : exige un OBJET. Une lumiere selectionnee ne fait
					// plus apparaitre la matiere — elle n'a pas de surface.
					if (i2 == 4 && !hasObj5)
						continue;
					char tk[24];
					snprintf(tk, sizeof(tk), "props.tab.%d", i2);
					const NkRect tb{tabX + S(3.f), ty, S(20.f), S(24.f)};
					const bool on = st.propOpen[i2];
					const bool overT = hit.Add(tk, tb);
					if (on)
						p.Fill(tb, NkRole::AccentUi, 3.f);
					else
						HoverFill(p, tb, overT, 3.f);
					p.IconV(tb.x + (tb.w - S(14.f)) * 0.5f, tb.y, tb.h, kSecs[i2].icon,
							on ? NkRole::TextOnAccent : NkRole::TextMuted, 14.f);
					if (hit.Clicked(tk)) {
						// EXCLUSIVE : choisir une categorie eteint les autres, et
						// recliquer l'active replie le panneau. Une section fermee
						// oublie son agrandissement et son defilement -- ils ne
						// doivent plus peser sur la mise en page.
						for (int32 j2 = 0; j2 < kNSec; ++j2) {
							if (j2 == i2)
								continue;
							if (st.propOpen[j2]) {
								st.propOpen[j2] = false;
								st.propSecH[j2] = 0.f;
								st.propScroll3[j2] = 0.f;
							}
						}
						st.propOpen[i2] = !on;
						if (on) {
							st.propSecH[i2] = 0.f;
							st.propScroll3[i2] = 0.f;
						}
					}
					ty += S(28.f);
				}
			}
			// ── MODALE D'AJOUT DE MATERIAU ──────────────────────────────────
			// Peinte AVANT le menu de groupe mais apres tout le reste : les
			// zones declarees en dernier gagnent le survol, c'est ce qui la rend
			// MODALE (Rihen, 12 aout). Un voile plein ecran capte tout ce qui
			// passerait a cote — « ce panneau ne doit pas laisser traverser les
			// evenements » — et cliquer dedans la referme.

			// ── LE DIALOGUE D'EMPLACEMENT, PAR-DESSUS TOUT ─────────────────
			// Peint apres la modale d'ajout : c'est lui qui prend la main quand
			// les deux existent, et les zones declarees en dernier gagnent.
			{
				NkFileDialogState &fdlg = NkMatFileDlg();
				if (fdlg.open) {
					NkFileDialogDesc fd;
					fd.title = "Nouveau materiau";
					fd.forcedExt = "nkmat";
					fd.okLabel = "Creer";
					fd.uniqueKind = 2u;
					if (NkFileDialogPaint(p, hit, ws, in, st, fdlg, fd, rFull) &&
						st.matNewPending) {
						// L'APPELANT AGIT — le dialogue n'a fait que decider.
						const int32 ni = demo::Demo3DHostProjMatCreate();
						if (ni >= 0) {
							char nomLibre2[80];
							NkMatUniqueName(fdlg.resultName, ni, nomLibre2,
											(uint32)sizeof(nomLibre2));
							demo::Demo3DHostProjMatSetName(ni, nomLibre2);
							const int32 an = demo::Demo3DHostActiveObject() >= 0
												 ? demo::Demo3DHostActiveObject()
												 : st.activeEmpty;
							if (an >= 0)
								(void)demo::Demo3DHostNodeMatAdd(an, ni);
							// Sa carte, DANS LE DOSSIER CHOISI, puis son fichier.
							nk3d::NkBrowserSyncMats(st);
							for (int32 b3 = 0; b3 < st.browserCount; ++b3)
								if (st.browserKind[b3] == 2 && st.browserMat[b3] == ni + 1) {
									st.browserParent[b3] = fdlg.resultFolder;
									snprintf(st.browserNames[b3], sizeof(st.browserNames[0]),
											 "%s", fdlg.resultName);
									if (!st.projectRoot.Empty()) {
										NkString err3;
										(void)nk3d::NkProjectWriteAssets(st.projectRoot, st,
																		 &err3, b3);
									}
									break;
								}
							NkMarkDirty(st);
						}
						st.matNewPending = false;
					}
				}
			}

			// LE MENU DE GROUPE, EN DERNIER : peint par-dessus tout le panneau,
			// il repond donc a ses propres clics (les zones declarees en
			// dernier gagnent le survol).
			PaintPropGroupMenu(p, hit, st, in);
			if (!hit.MouseDown())
				st.propDragKey[0] = 0; // fin de glissement : la barre lache le geste
		}

		// â”€â”€ PROPRIETES (droite, haut) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
		inline void PaintProperties(NkModelerPainter &p, const NkRect &full, NkModelerState &st,
									NkHitRegistry &hit, NkWidgetState &ws, const nkgui::NkGuiInput &in) {
			p.Fill(full, NkRole::PanelBg);
			p.VLine(full.x, full.y, full.h);
			float32 y = PaintPanelTab(p, full, "Proprietes", &hit, &st.showRight, "prop.close");
			const NkRect r = Inset(full);
			y = PaintSearch(p, r, y, hit, ws, in, "props.search", st.searchProps);

			// LES CINQ PASTILLES Â« General / Objet / Rendu / Physique / Tout Â» ONT
			// ETE RETIREES. Rihen a demande a quoi elles servaient : a rien. Elles
			// venaient de la maquette et annonÃ§aient quatre familles de reglages
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
			// l'en-tete Â« Proprietes Â» et deborde sur Details en dessous.
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
					// â”€â”€ EDITION PROPORTIONNELLE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
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

		// â”€â”€ DETAILS (droite, bas) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
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
			// Â« Maillage Â» remonte par-dessus l'onglet Â« Details Â» des le premier cran
			// de molette, et les sections du bas debordent sur le navigateur. C'etait
			// visible et c'est corrige ici plutot qu'en bornant le defilement : borner
			// ne changerait rien, le debordement vient du DESSIN.
			const NkRect clipR{full.x, listTop, full.w, full.y + full.h - listTop};
			p.Clip(clipR);
			y -= scroll;

			// â”€â”€ MAILLAGE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			if (DetailHeader(p, hit, r, y, st, NkDetailMesh, "Maillage")) {
				// LES ARETES MANQUAIENT. Sur un maillage a demi-aretes elles ne sont
				// pas une curiosite : c'est la seule des trois quantites qui trahit un
				// maillage non-manifold (une arete portee par trois faces) et c'est
				// aussi le sous-mode d'edition du milieu. En afficher deux sur trois
				// laissait croire que le compte d'aretes n'existait pas.
				static const char *const kL[] = {"Sommets", "Aretes", "Faces", "Triangles"};
				// PLUS DE VALEURS EN DUR. Elles viennent du NkEditMesh lui-meme : un
				// panneau qui affiche Â« 8 sommets Â» quoi qu'il arrive est pire qu'un
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

			// â”€â”€ MODIFICATEURS : LA PILE REELLE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			// Ce qui etait peint ici -- Â« Selectionner un modificateur Â» -- etait une
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
							NkMarkDirty(st);
						}
						bx -= 22.f;
					}
					y += kRowH;

					// â”€â”€ Parametres, decrits par le modificateur lui-meme â”€â”€â”€â”€â”€
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

			// â”€â”€ MATERIAUX : PLUSIEURS PAR MODELE â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
			// Rihen le rappelle et c'est structurant : un modele n'a pas UN materiau,
			// il a une LISTE D'EMPLACEMENTS. Chaque face du maillage porte l'indice de
			// l'emplacement qui la peint ; l'ensemble des faces qui partagent un indice
			// forme un SOUS-MAILLAGE. C'est exactement le modele de Blender, et c'est
			// aussi ce qu'attend le rendu : un tampon de dessin par emplacement.
			//
			// Consequence sur le format du maillage : `NkEditMesh` doit porter un
			// `materialSlot` PAR FACE, pas une couleur par objet. Consequence sur
			// l'edition : selectionner des faces (ou des sommets, dont on deduit les
			// faces) puis Â« Assigner Â» ecrit cet indice -- ce qui CREE le sous-maillage
			// sans qu'aucune geometrie soit dupliquee ni separee.
			//
			// Ce panneau n'est pour l'instant qu'une facade : les emplacements sont en
			// dur et Â« Assigner Â» n'ecrit rien. Le cablage vient avec la vue 3D, quand
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
					// TOUT est grise tant qu'aucun materiau n'existe : Â« Assigner Â»
					// demande en plus une selection de faces, donc le mode edition.
					const bool off = true;
					const NkRole fg = off ? NkRole::TextMuted : NkRole::Text;
					p.Outline(br, NkRole::Border, NkRole::InputBg, 2.f);
					p.IconV(br.x + 5.f, br.y, br.h, kB[i].ic, fg, 12.f);
					p.TextV(br.x + 21.f, br.y, br.h, kB[i].label, fg);
				}
				y += kRowH + 4.f;
			}

			// â”€â”€ SOUS-MAILLAGES â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
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

	} // namespace nk3d
} // namespace nkentseu
