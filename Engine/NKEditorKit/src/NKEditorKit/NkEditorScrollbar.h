#pragma once
// -----------------------------------------------------------------------------
// @File    NkEditorScrollbar.h
// @Brief   Scrollbar GENERAL reutilisable (vertical + horizontal), calque sur
//          celui de l'editeur de code : gouttiere toujours visible (theme-aware),
//          fleches aux extremites, pouce draggable, clic-piste-pour-se-positionner.
//          UN SEUL style/comportement partout (editeur, dropdowns, picker, listes).
//          Engine-native (ctx/dl/theme) -> a utiliser dans toute l'UI Nkentseu.
// @Author  Rihen
// @License Proprietary - Free to use and modify
// -----------------------------------------------------------------------------
#include "NKGui/NKGui.h"

namespace nkentseu {
	namespace editorkit {

		using namespace nkentseu;
		using namespace nkentseu::nkgui;

		// Largeur/epaisseur CANONIQUE d'une scrollbar (identique a l'editeur = 14 px logiques).
		// Toutes les scrollbars de l'UI doivent reserver CETTE largeur -> aspect uniforme.
		inline float32 NkScrollbarWidth(float32 scale) { return 14.f * scale; }

		// ── Couleurs scrollbar deduites du theme (clair/sombre) ──
		struct NkScrollbarColors {
				NkColor track, thumb, thumbHover, arrow, arrowHover;
		};

		inline NkScrollbarColors NkScrollbarThemeColors(const NkGuiTheme &th) {
			const bool light = ((int32)th.bgPrimary.r + th.bgPrimary.g + th.bgPrimary.b) > 384;
			NkScrollbarColors c;
			// Gouttiere VISIBLE sur n'importe quel fond (channel semi-opaque theme-aware).
			c.track = light ? NkColor{0, 0, 0, 34} : NkColor{255, 255, 255, 26};
			c.thumb = light ? NkColor{150, 158, 168, 255} : NkColor{104, 114, 128, 255};
			c.thumbHover = light ? NkColor{112, 120, 130, 255} : NkColor{140, 152, 166, 255};
			// Fleches NETTEMENT contrastees (pas la meme couleur que le pouce -> bien visibles).
			c.arrow = light ? NkColor{90, 98, 108, 255} : NkColor{170, 180, 194, 255};
			c.arrowHover = light ? NkColor{0, 0, 0, 45} : NkColor{255, 255, 255, 45};
			return c;
		}

		namespace detail {
			// Bouton fleche (dir : 0 haut, 1 bas, 2 gauche, 3 droite). Retourne MAINTENU.
			// La taille de la fleche SCALE avec le bouton (sbW) -> visible en haute densite.
			inline bool NkSbArrow(NkGuiContext &ctx, NkGuiDrawList &dl, const NkRect &r, int32 dir,
								  const NkScrollbarColors &c) {
				const NkVec2 m = ctx.input.mousePos;
				const bool h = NkGuiRectContains(r, m);
				if (h)
					dl.AddRectFilled(r, c.arrowHover);
				const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
				const float32 a = (r.w < r.h ? r.w : r.h) * 0.32f; // demi-taille de la fleche (scalee, bien visible)
				const NkColor ac = h ? c.thumbHover : c.arrow;
				if (dir == 0)
					dl.AddTriangleFilled({cx, cy - a}, {cx - a, cy + a}, {cx + a, cy + a}, ac);
				else if (dir == 1)
					dl.AddTriangleFilled({cx - a, cy - a}, {cx + a, cy - a}, {cx, cy + a}, ac);
				else if (dir == 2)
					dl.AddTriangleFilled({cx - a, cy}, {cx + a, cy - a}, {cx + a, cy + a}, ac);
				else
					dl.AddTriangleFilled({cx - a, cy - a}, {cx + a, cy}, {cx - a, cy + a}, ac);
				return h && ctx.input.mouseClicked[0];
			}
		} // namespace detail

		// ── Barre de defilement VERTICALE. `track` = gouttiere COMPLETE (fleches incluses).
		//    `scroll` borne dans [0, contentLen-viewLen]. `id` = identifiant unique (drag).
		//    `arrows` = affiche/active les fleches. Retourne true si `scroll` a change. ──
		inline bool NkVScrollbar(NkGuiContext &ctx, NkGuiDrawList &dl, const NkRect &track, float32 &scroll,
								 float32 contentLen, float32 viewLen, uint32 id, float32 lineStep = 0.f,
								 bool arrows = true) {
			const NkScrollbarColors c = NkScrollbarThemeColors(ctx.theme);
			const float32 sbW = track.w;
			dl.AddRectFilled(track, c.track); // gouttiere toujours visible
			const float32 step = lineStep > 0.f ? lineStep : sbW * 1.4f;
			const float32 before = scroll;
			const float32 maxScroll = contentLen - viewLen > 0.f ? contentLen - viewLen : 0.f;
			const NkVec2 m = ctx.input.mousePos;
			NkRect inner = track;
			if (arrows) {
				const NkRect upB = {track.x, track.y, sbW, sbW};
				const NkRect dnB = {track.x, track.y + track.h - sbW, sbW, sbW};
				inner = {track.x, track.y + sbW, sbW, track.h - 2.f * sbW};
				if (detail::NkSbArrow(ctx, dl, upB, 0, c))
					scroll -= step;
				if (detail::NkSbArrow(ctx, dl, dnB, 1, c))
					scroll += step;
			}
			if (maxScroll > 0.f && inner.h > 8.f) {
				const float32 pad = sbW * 0.21f;			   // inset/arrondi scale avec l'epaisseur
				float32 th = inner.h * (viewLen / contentLen);
				if (th < sbW * 1.7f)
					th = sbW * 1.7f;
				if (th > inner.h)
					th = inner.h;
				const float32 ty = inner.y + (scroll / maxScroll) * (inner.h - th);
				const NkRect thumb = {inner.x + pad, ty, sbW - 2.f * pad, th};
				if (ctx.input.mouseClicked[0] && NkGuiRectContains(inner, m))
					ctx.activeId = id;
				const bool act = (ctx.activeId == id);
				if (act && ctx.input.mouseDown[0] && inner.h - th > 0.f) {
					const float32 t = (m.y - inner.y - th * 0.5f) / (inner.h - th);
					scroll = (t < 0.f ? 0.f : t > 1.f ? 1.f : t) * maxScroll;
				}
				dl.AddRectFilled(thumb, (act || NkGuiRectContains(inner, m)) ? c.thumbHover : c.thumb, pad);
			}
			if (ctx.activeId == id && !ctx.input.mouseDown[0])
				ctx.activeId = 0; // relache le drag
			if (scroll < 0.f)
				scroll = 0.f;
			if (scroll > maxScroll)
				scroll = maxScroll;
			return scroll != before;
		}

		// ── Barre de defilement HORIZONTALE. `track` = gouttiere COMPLETE (fleches incluses). ──
		inline bool NkHScrollbar(NkGuiContext &ctx, NkGuiDrawList &dl, const NkRect &track, float32 &scroll,
								 float32 contentLen, float32 viewLen, uint32 id, float32 step = 18.f,
								 bool arrows = true) {
			const NkScrollbarColors c = NkScrollbarThemeColors(ctx.theme);
			const float32 sbW = track.h;
			dl.AddRectFilled(track, c.track);
			const float32 before = scroll;
			const float32 maxScroll = contentLen - viewLen > 0.f ? contentLen - viewLen : 0.f;
			const NkVec2 m = ctx.input.mousePos;
			NkRect inner = track;
			if (arrows) {
				const NkRect lfB = {track.x, track.y, sbW, sbW};
				const NkRect rtB = {track.x + track.w - sbW, track.y, sbW, sbW};
				inner = {track.x + sbW, track.y, track.w - 2.f * sbW, sbW};
				if (detail::NkSbArrow(ctx, dl, lfB, 2, c))
					scroll -= step;
				if (detail::NkSbArrow(ctx, dl, rtB, 3, c))
					scroll += step;
			}
			if (maxScroll > 0.f && inner.w > 8.f) {
				const float32 pad = sbW * 0.21f;
				float32 tw = inner.w * (viewLen / contentLen);
				if (tw < sbW * 1.7f)
					tw = sbW * 1.7f;
				if (tw > inner.w)
					tw = inner.w;
				const float32 tx = inner.x + (scroll / maxScroll) * (inner.w - tw);
				const NkRect thumb = {tx, inner.y + pad, tw, sbW - 2.f * pad};
				if (ctx.input.mouseClicked[0] && NkGuiRectContains(inner, m))
					ctx.activeId = id;
				const bool act = (ctx.activeId == id);
				if (act && ctx.input.mouseDown[0] && inner.w - tw > 0.f) {
					const float32 t = (m.x - inner.x - tw * 0.5f) / (inner.w - tw);
					scroll = (t < 0.f ? 0.f : t > 1.f ? 1.f : t) * maxScroll;
				}
				dl.AddRectFilled(thumb, (act || NkGuiRectContains(inner, m)) ? c.thumbHover : c.thumb, pad);
			}
			if (ctx.activeId == id && !ctx.input.mouseDown[0])
				ctx.activeId = 0;
			if (scroll < 0.f)
				scroll = 0.f;
			if (scroll > maxScroll)
				scroll = maxScroll;
			return scroll != before;
		}

	} // namespace editorkit
} // namespace nkentseu
