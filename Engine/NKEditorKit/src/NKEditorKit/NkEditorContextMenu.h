#pragma once
// -----------------------------------------------------------------------------
// @File    NkEditorContextMenu.h
// @Brief   Menu contextuel (clic droit) REUTILISABLE : liste d'items scrollable
//          (V/H), sous-menus (fleche), theme, occlusion d'input ("modal leger" :
//          consomme le clic quand la souris est dedans). Engine-native (ctx/dl/
//          font/theme) -> partageable par tous les editeurs Nkentseu.
// @Author  Rihen
// @License Proprietary - Free to use and modify
// -----------------------------------------------------------------------------
#include "NKGui/NKGui.h"
#include "NKEditorKit/NkEditorScrollbar.h" // scrollbar standard

namespace nkentseu {
	namespace editorkit {

		using namespace nkentseu;
		using namespace nkentseu::nkgui;

// ── Menu contextuel (clic droit) Copier/Couper/Coller — reutilise par
		//    l'editeur et le terminal. Dessine sur la couche overlay (au-dessus). ──
		struct NkCtxMenu {
				bool open = false;
				NkVec2 pos{0.f, 0.f};
				float32 sx = 0.f, sy = 0.f;			// defilement (reset a la fermeture)
				NkRect rect = {0.f, 0.f, 0.f, 0.f}; // boite de la frame courante (garde d'input)
		};

		// Retourne l'index de l'item clique (et ferme le menu), -1 sinon. Se ferme au
		// clic exterieur ou sur Echap. `enabled[i]` grise les items non applicables.
		inline int32 NkCtxMenuDraw(NkGuiContext &ctx, NkCtxMenu &mn, const char *const *items, const bool *enabled,
								   int32 count, int32 *hoveredOut = nullptr, const bool *hasSub = nullptr) {
			if (!mn.open)
				return -1;
			NkGuiDrawList &dl = ctx.dlOverlay;
			const float32 lh = (ctx.font && ctx.font->Valid()) ? ctx.font->LineHeight() : 16.f;
			const float32 rowH = lh + 8.f, pad = 10.f, sbT = NkScrollbarWidth(); // scrollbar standard (14)
			// Taille IDEALE (plus long item) puis bornes : 60 % de la largeur, 50 % de la hauteur ;
			// au-dela -> defilement V/H (molette = V, Maj+molette ou molette H = H, barres draggables).
			float32 wIdeal = 168.f;
			if (ctx.font && ctx.font->Valid())
				for (int32 i = 0; i < count; ++i) {
					const float32 tw = ctx.font->MeasureWidth(items[i]) + pad * 2.f + 10.f +
									   ((hasSub && hasSub[i]) ? 16.f : 0.f); // place de la flèche ▸
					if (tw > wIdeal)
						wIdeal = tw;
				}
			const float32 wCap = static_cast<float32>(ctx.viewW) * 0.6f;
			const float32 hCap = static_cast<float32>(ctx.viewH) * 0.5f;
			const float32 contentH = count * rowH;
			const bool hasH = wIdeal > wCap;
			const float32 w = hasH ? wCap : wIdeal;
			const bool hasV = contentH + 8.f > hCap;
			const float32 h = (hasV ? hCap : contentH + 8.f) + (hasH ? sbT : 0.f);
			NkRect box = {mn.pos.x, mn.pos.y, w, h};
			if (box.x + box.w > static_cast<float32>(ctx.viewW))
				box.x = static_cast<float32>(ctx.viewW) - box.w;
			if (box.y + box.h > static_cast<float32>(ctx.viewH))
				box.y = static_cast<float32>(ctx.viewH) - box.h;
			if (box.x < 0.f)
				box.x = 0.f;
			if (box.y < 0.f)
				box.y = 0.f;
			const NkRect inner = {box.x, box.y, box.w - (hasV ? sbT : 0.f), box.h - (hasH ? sbT : 0.f)};
			const NkVec2 m = ctx.input.mousePos;
			const bool inBox = m.x >= box.x && m.x < box.x + box.w && m.y >= box.y && m.y < box.y + box.h;
			// Molette CONSOMMEE au-dessus du menu (sinon l'editeur en dessous defile aussi).
			const float32 maxSy = contentH + 8.f - inner.h > 0.f ? contentH + 8.f - inner.h : 0.f;
			const float32 maxSx = wIdeal - inner.w > 0.f ? wIdeal - inner.w : 0.f;
			if (inBox && ctx.input.wheel != 0.f) {
				if (ctx.input.shiftDown)
					mn.sx -= ctx.input.wheel * 32.f;
				else
					mn.sy -= ctx.input.wheel * rowH * 2.f;
				ctx.input.wheel = 0.f;
			}
			if (inBox && ctx.input.wheelH != 0.f) {
				mn.sx -= ctx.input.wheelH * 32.f;
				ctx.input.wheelH = 0.f;
			}
			if (mn.sy < 0.f)
				mn.sy = 0.f;
			if (mn.sy > maxSy)
				mn.sy = maxSy;
			if (mn.sx < 0.f)
				mn.sx = 0.f;
			if (mn.sx > maxSx)
				mn.sx = maxSx;
			mn.rect = box; // exposee : les zones DERRIERE ignorent la souris quand elle est ici
			// Couleurs du THÈME (dark ET light) — plus de valeurs en dur qui juraient
			// en thème clair (fond sombre + texte clair sur UI claire).
			dl.AddRectFilled(box, ctx.theme.panel, 6.f);
			dl.AddRect(box, ctx.theme.border, 1.f);
			int32 clicked = -1;
			dl.PushClipRect(inner, true);
			float32 y = box.y + 4.f - mn.sy;
			for (int32 i = 0; i < count; ++i) {
				const NkRect r = {box.x + 3.f, y, inner.w - 6.f, rowH};
				if (y + rowH >= inner.y && y <= inner.y + inner.h) { // row visible
					const bool hov = m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h && m.y >= inner.y &&
									 m.y < inner.y + inner.h;
					if (hov && hoveredOut)
						*hoveredOut = i; // sous-menus : l'appelant sait quel item est survolé
					if (hov && enabled[i]) {
						NkColor selBg = ctx.theme.selection;
						selBg.a = 110;
						dl.AddRectFilled(r, selBg, 4.f);
					}
					if (ctx.font && ctx.font->Valid())
						dl.AddText(ctx.font->Face(), ctx.font->TexId(),
								   {r.x + pad - mn.sx, y + (rowH - lh) * 0.5f + ctx.font->Ascent()}, items[i],
								   enabled[i] ? ctx.theme.text : ctx.theme.textDisabled);
					if (hasSub && hasSub[i]) { // indicateur de SOUS-MENU : petite flèche ▸ à droite
						const float32 ax = r.x + r.w - 11.f, ay = y + rowH * 0.5f;
						dl.AddTriangleFilled({ax - 3.f, ay - 4.f}, {ax - 3.f, ay + 4.f}, {ax + 3.f, ay},
											 enabled[i] ? ctx.theme.text : ctx.theme.textDisabled);
					}
					if (hov && enabled[i] && ctx.input.mouseClicked[0])
						clicked = i;
				}
				y += rowH;
			}
			dl.PopClipRect();
			// Barres de defilement (temoins + clic/glisser pour se positionner).
			if (hasV) {
				const NkRect tr = {box.x + box.w - sbT, inner.y, sbT, inner.h};
				NkVScrollbar(ctx, dl, tr, mn.sy, contentH + 8.f, inner.h, 0xC71E0001u, rowH);
			}
			if (hasH) {
				const NkRect tr = {box.x, box.y + box.h - sbT, inner.w, sbT};
				NkHScrollbar(ctx, dl, tr, mn.sx, wIdeal, inner.w, 0xC71E0002u, 32.f);
			}
			if (clicked >= 0) {
				mn.open = false;
				mn.sx = 0.f;
				mn.sy = 0.f;
			} else if ((ctx.input.mouseClicked[0] || ctx.input.mouseClicked[1]) && !inBox) {
				mn.open = false;
				mn.sx = 0.f;
				mn.sy = 0.f;
			}
			if (ctx.input.KeyPressed(NkGuiKey::Escape)) {
				mn.open = false;
				mn.sx = 0.f;
				mn.sy = 0.f;
			}
			if (inBox) { // MODAL leger : rien derriere ne doit voir ce clic
				ctx.input.mouseClicked[0] = false;
				ctx.input.mouseClicked[1] = false;
			}
			return clicked;
		}

	} // namespace editorkit
} // namespace nkentseu
