#pragma once
// -----------------------------------------------------------------------------
// @File    NkEditorCombo.h
// @Brief   Combo/menu deroulant REUTILISABLE : bouton compact (icone seule OU
//          icone + libelle court + chevron) qui ouvre un menu de selection.
//          Le menu s'ouvre EN BAS ou EN HAUT de l'ancre selon l'espace libre
//          dans `bounds` (ex. le rect du panneau) — jamais coupe hors-ecran.
//          Independant de toute application (raw NkGuiContext/NkGuiDrawList/
//          NkGuiFont), meme esprit que NkOverlayTextField.h.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NKGui/NKGui.h"

namespace nkentseu {
	namespace editorkit {

		using namespace nkentseu;
		using namespace nkentseu::nkgui;

		// Dessine un glyphe vectoriel dans un cercle de rayon `rad` centre en `c` (meme
		// signature que les icones maison existantes : engrenage, thermometre...).
		using NkComboIconFn = void (*)(NkGuiDrawList &dl, const NkVec2 &c, float32 rad, const NkColor &col);

		// ── Bouton combo compact. `label` == nullptr => bouton ICONE SEULE (carre, sans
		// chevron, comme les boutons [+] / [crayon] d'une barre d'outils). `label` != nullptr
		// => pilule icone+texte+chevron (ex. "⚡ Auto"). `iconTex` (texture) est prioritaire
		// sur `iconFn` (glyphe vectoriel dessine) si les deux sont fournis.
		// `comboId` identifie CE combo dans le groupe partageant `openId` (0 = aucun ouvert).
		// `anchorOut` recoit le rect du bouton (a repasser tel quel a NkComboMenu).
		// Retourne la largeur du bouton (pour enchainer horizontalement, comme un layout). ──
		inline float32 NkComboButton(NkGuiContext &ctx, NkGuiDrawList &dl, const NkGuiFont *font, float32 x,
									 float32 cy, uint32 iconTex, NkComboIconFn iconFn, const char *label,
									 int32 comboId, int32 &openId, NkRect &anchorOut, const NkColor &bg,
									 const NkColor &bgHover, const NkColor &fg) noexcept {
			const float32 h = ctx.ItemHeight();
			const bool hasIcon = iconTex != 0 || iconFn != nullptr;
			const bool hasLabel = label && label[0];
			const float32 iw = hasIcon ? h * 0.5f : 0.f;
			const float32 tw = (hasLabel && font && font->Valid()) ? font->MeasureWidth(label) : 0.f;
			const float32 chevW = hasLabel ? ctx.S(14.f) : 0.f;
			const float32 w = hasLabel ? (ctx.S(10.f) + iw + (hasIcon ? ctx.S(5.f) : 0.f) + tw + ctx.S(6.f) + chevW +
										 ctx.S(4.f))
										: h; // icone seule = bouton carre
			const NkVec2 mp = ctx.input.mousePos;
			const NkRect r = {x, cy - h * 0.5f, w, h};
			const bool hov = NkGuiRectContains(r, mp);
			const bool open = (openId == comboId);
			dl.AddRectFilled(r, (hov || open) ? bgHover : bg, ctx.theme.rounding);
			float32 tx = r.x + (hasLabel ? ctx.S(10.f) : (w - iw) * 0.5f);
			if (hasIcon) {
				if (iconTex)
					dl.AddImage(iconTex, {tx, cy - iw * 0.5f, iw, iw}, {0.f, 0.f}, {1.f, 1.f}, fg);
				else
					iconFn(dl, {tx + iw * 0.5f, cy}, iw * 0.5f, fg);
			}
			if (hasLabel && font && font->Valid()) {
				tx += hasIcon ? iw + ctx.S(5.f) : 0.f;
				dl.AddText(font->Face(), font->TexId(), {tx, cy - font->LineHeight() * 0.5f + font->Ascent()}, label,
						   fg);
				const NkRect chev = {r.x + w - chevW - ctx.S(4.f), cy - ctx.S(3.f), ctx.S(10.f), ctx.S(7.f)};
				dl.AddTriangleFilled({chev.x, chev.y}, {chev.x + ctx.S(10.f), chev.y},
									 {chev.x + ctx.S(5.f), chev.y + ctx.S(6.f)}, fg);
			}
			anchorOut = r;
			if (hov && ctx.input.mouseClicked[0]) {
				openId = open ? 0 : comboId;
				ctx.input.mouseClicked[0] = false;
			}
			return w;
		}

		// ── Menu deroulant (options texte, selection simple) ancre sous `anchor` — ou
		// AU-DESSUS si l'espace manque en dessous dans `bounds` (jamais coupe hors-ecran).
		// Ferme au clic exterieur ou a la selection (openId remis a 0). ──
		inline void NkComboMenu(NkGuiContext &ctx, NkGuiDrawList &dl, const NkGuiFont *font, const NkRect &anchor,
								const NkRect &bounds, const char *const *opts, int32 n, int32 &sel, int32 &openId,
								const NkColor &accent, const NkColor &panelBg, const NkColor &border,
								const NkColor &text, const NkColor &textHi, const NkColor &rowHover) noexcept {
			const float32 rowH = ctx.ItemHeight() + ctx.S(4.f);
			float32 w = anchor.w;
			for (int32 i = 0; i < n; ++i) {
				const float32 tw = (font && font->Valid()) ? font->MeasureWidth(opts[i]) : 0.f;
				if (tw + ctx.S(24.f) > w)
					w = tw + ctx.S(24.f);
			}
			const float32 menuH = n * rowH + ctx.S(8.f);
			const float32 spaceBelow = (bounds.y + bounds.h) - (anchor.y + anchor.h);
			const float32 spaceAbove = anchor.y - bounds.y;
			// Ouvre en HAUT si l'espace manque en dessous ET qu'il y a plus de place au-dessus.
			const bool openUp = spaceBelow < menuH + ctx.S(4.f) && spaceAbove > spaceBelow;
			NkRect menu = {anchor.x, openUp ? (anchor.y - menuH - ctx.S(4.f)) : (anchor.y + anchor.h + ctx.S(4.f)), w,
						   menuH};
			if (menu.x < bounds.x + ctx.S(4.f))
				menu.x = bounds.x + ctx.S(4.f);
			if (menu.x + menu.w > bounds.x + bounds.w - ctx.S(4.f))
				menu.x = bounds.x + bounds.w - ctx.S(4.f) - menu.w;
			if (menu.y < bounds.y)
				menu.y = bounds.y;
			if (menu.y + menu.h > bounds.y + bounds.h)
				menu.y = bounds.y + bounds.h - menu.h;
			dl.AddRectFilled(menu, panelBg, ctx.S(6.f));
			dl.AddRect(menu, border, 1.f);
			const NkVec2 mp = ctx.input.mousePos;
			float32 y = menu.y + ctx.S(4.f);
			for (int32 i = 0; i < n; ++i) {
				const NkRect row = {menu.x + ctx.S(4.f), y, w - ctx.S(8.f), rowH};
				const bool hov = NkGuiRectContains(row, mp);
				const bool s = (i == sel);
				if (hov || s)
					dl.AddRectFilled(row, s ? accent : rowHover, ctx.S(4.f));
				if (font && font->Valid())
					dl.AddText(font->Face(), font->TexId(),
							   {row.x + ctx.S(8.f), row.y + (rowH - font->LineHeight()) * 0.5f + font->Ascent()},
							   opts[i], (s || hov) ? textHi : text);
				if (hov && ctx.input.mouseClicked[0]) {
					sel = i;
					openId = 0;
					ctx.input.mouseClicked[0] = false;
				}
				y += rowH;
			}
			if (ctx.input.mouseClicked[0] && !NkGuiRectContains(menu, mp) && !NkGuiRectContains(anchor, mp))
				openId = 0;
		}

	} // namespace editorkit
} // namespace nkentseu
