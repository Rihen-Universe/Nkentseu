#pragma once
// -----------------------------------------------------------------------------
// @File    NkEditorScrollbar.h
// @Brief   Scrollbar STANDARD reutilisable (vertical + horizontal). C'est le
//          scrollbar de l'editeur de code EXTRAIT TEL QUEL (memes couleurs, memes
//          fleches, memes tailles) -> une seule barre pour TOUTE l'UI Nkentseu.
//          NE PAS "ameliorer" le rendu : il doit rester identique a l'editeur.
//          Engine-native (ctx/dl/theme).
// @Author  Rihen
// @License Proprietary - Free to use and modify
// -----------------------------------------------------------------------------
#include "NKGui/NKGui.h"

namespace nkentseu {
	namespace editorkit {

		using namespace nkentseu;
		using namespace nkentseu::nkgui;

		// Epaisseur STANDARD d'une scrollbar = celle de l'editeur (14 px, ctx).
		inline float32 NkScrollbarWidth() { return 14.f; }

		// ── Couleurs scrollbar (par defaut : EXACTEMENT celles de l'editeur, theme-aware) ──
		struct NkScrollbarColors {
				NkColor track, thumb, thumbHover, arrowHover;
		};

		inline NkScrollbarColors NkScrollbarThemeColors(const NkGuiTheme &th) {
			const bool light = ((int32)th.bgPrimary.r + th.bgPrimary.g + th.bgPrimary.b) > 384;
			NkScrollbarColors c;
			c.track = light ? NkColor{0, 0, 0, 20} : NkColor{255, 255, 255, 16};
			c.thumb = light ? NkColor{168, 176, 185, 255} : NkColor{80, 88, 98, 255};
			c.thumbHover = light ? NkColor{130, 138, 148, 255} : NkColor{120, 130, 142, 255};
			c.arrowHover = {33, 39, 48, 255};
			return c;
		}

		// ── PERSONNALISATION UTILISATEUR (optionnelle) : si `custom` est vrai, ces
		//    couleurs REMPLACENT celles du theme (l'utilisateur peut alterer l'apparence,
		//    theme compris). Par defaut inactive -> le scrollbar suit le theme Dark/Light. ──
		struct NkScrollbarSkin {
				bool custom = false;
				NkScrollbarColors colors;
		};

		inline NkScrollbarSkin &NkScrollbarUserSkin() {
			static NkScrollbarSkin s;
			return s;
		}

		// Couleurs effectives : skin utilisateur si actif, sinon le theme.
		inline NkScrollbarColors NkScrollbarActiveColors(const NkGuiTheme &th) {
			const NkScrollbarSkin &sk = NkScrollbarUserSkin();
			return sk.custom ? sk.colors : NkScrollbarThemeColors(th);
		}

		namespace detail {
			// Bouton fleche (dir : 0 haut, 1 bas, 2 gauche, 3 droite). Retourne MAINTENU.
			// IDENTIQUE a l'editeur : fond survol {33,39,48}, triangle a=3.2, couleur pouce.
			inline bool NkSbArrow(NkGuiContext &ctx, NkGuiDrawList &dl, const NkRect &r, int32 dir,
								  const NkScrollbarColors &c) {
				const NkVec2 m = ctx.input.mousePos;
				const bool h = NkGuiRectContains(r, m);
				if (h)
					dl.AddRectFilled(r, c.arrowHover);
				const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f, a = 3.2f;
				const NkColor ac = h ? c.thumbHover : c.thumb;
				if (dir == 0)
					dl.AddTriangleFilled({cx, cy - a}, {cx - a, cy + a}, {cx + a, cy + a}, ac);
				else if (dir == 1)
					dl.AddTriangleFilled({cx - a, cy - a}, {cx + a, cy - a}, {cx, cy + a}, ac);
				else if (dir == 2)
					dl.AddTriangleFilled({cx - a, cy}, {cx + a, cy - a}, {cx + a, cy + a}, ac);
				else
					dl.AddTriangleFilled({cx - a, cy - a}, {cx + a, cy}, {cx - a, cy + a}, ac);
				return h && ctx.input.mouseDown[0];
			}
		} // namespace detail

		// ── Barre VERTICALE. `track` = gouttiere COMPLETE (fleches incluses). `scroll`
		//    borne dans [0, contentLen-viewLen]. `id` = identifiant unique (drag). ──
		inline bool NkVScrollbar(NkGuiContext &ctx, NkGuiDrawList &dl, const NkRect &track, float32 &scroll,
								 float32 contentLen, float32 viewLen, uint32 id, float32 lineStep = 0.f,
								 bool arrows = true) {
			const NkScrollbarColors c = NkScrollbarActiveColors(ctx.theme); // theme OU skin utilisateur
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
				float32 th = inner.h * (viewLen / contentLen);
				if (th < 24.f)
					th = 24.f;
				if (th > inner.h)
					th = inner.h;
				const float32 ty = inner.y + (scroll / maxScroll) * (inner.h - th);
				const NkRect thumb = {inner.x + 3.f, ty, sbW - 6.f, th};
				if (ctx.input.mouseClicked[0] && NkGuiRectContains(inner, m))
					ctx.activeId = id;
				const bool act = (ctx.activeId == id);
				if (act && ctx.input.mouseDown[0] && inner.h - th > 0.f) {
					const float32 t = (m.y - inner.y - th * 0.5f) / (inner.h - th);
					scroll = (t < 0.f ? 0.f : t > 1.f ? 1.f : t) * maxScroll;
				}
				dl.AddRectFilled(thumb, (act || NkGuiRectContains(inner, m)) ? c.thumbHover : c.thumb, 3.f);
			}
			if (ctx.activeId == id && !ctx.input.mouseDown[0])
				ctx.activeId = 0; // relache le drag
			if (scroll < 0.f)
				scroll = 0.f;
			if (scroll > maxScroll)
				scroll = maxScroll;
			return scroll != before;
		}

		// ── Barre HORIZONTALE. `track` = gouttiere COMPLETE (fleches incluses). ──
		inline bool NkHScrollbar(NkGuiContext &ctx, NkGuiDrawList &dl, const NkRect &track, float32 &scroll,
								 float32 contentLen, float32 viewLen, uint32 id, float32 step = 18.f,
								 bool arrows = true) {
			const NkScrollbarColors c = NkScrollbarActiveColors(ctx.theme); // theme OU skin utilisateur
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
				float32 tw = inner.w * (viewLen / contentLen);
				if (tw < 24.f)
					tw = 24.f;
				if (tw > inner.w)
					tw = inner.w;
				const float32 tx = inner.x + (scroll / maxScroll) * (inner.w - tw);
				const NkRect thumb = {tx, inner.y + 3.f, tw, sbW - 6.f};
				if (ctx.input.mouseClicked[0] && NkGuiRectContains(inner, m))
					ctx.activeId = id;
				const bool act = (ctx.activeId == id);
				if (act && ctx.input.mouseDown[0] && inner.w - tw > 0.f) {
					const float32 t = (m.x - inner.x - tw * 0.5f) / (inner.w - tw);
					scroll = (t < 0.f ? 0.f : t > 1.f ? 1.f : t) * maxScroll;
				}
				dl.AddRectFilled(thumb, (act || NkGuiRectContains(inner, m)) ? c.thumbHover : c.thumb, 3.f);
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
