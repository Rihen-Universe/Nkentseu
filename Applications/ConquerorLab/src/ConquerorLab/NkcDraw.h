#pragma once
// =============================================================================
// NkcDraw — les quelques primitives qui manquent a NkGuiDrawList pour dessiner
// un plateau : polygone plein, contour, anneau, et trois formes de texte.
//
// NkGuiDrawList offre AddRectFilled / AddRect / AddLine / AddTriangleFilled /
// AddCircleFilled / AddText. Il n'y a NI polyligne, NI cercle en contour : on
// les compose ici, une fois, plutot que dans chaque panneau.
//
// Les fonctions de texte encapsulent les deux formules a ne jamais retaper de
// tete (cours NKGui §3.7) :
//     aligne en haut  : baseline.y = y + font->Ascent()
//     centre dans r   : baseline.y = r.y + (r.h - LineHeight()) * 0.5 + Ascent()
// et la garde `!ctx.font || !ctx.font->Valid()` qui doit preceder tout AddText.
// =============================================================================

#include "NKGui/NKGui.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu {
	namespace conqueror {

		using nkgui::NkColor;
		using nkgui::NkGuiContext;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		// ---------------------------------------------------------------------
		// Texte
		// ---------------------------------------------------------------------
		inline float32 NkcTextW(NkGuiContext &ctx, const char *s) noexcept {
			if (!ctx.font || !ctx.font->Valid() || !s) return 0.f;
			return ctx.font->MeasureWidth(s);
		}

		inline float32 NkcLineH(NkGuiContext &ctx) noexcept {
			return (ctx.font && ctx.font->Valid()) ? ctx.font->LineHeight() : 16.f;
		}

		/// Texte au coin haut-gauche.
		inline void NkcText(NkGuiContext &ctx, float32 x, float32 y, const char *s,
							const NkColor &col, float32 maxWidth = -1.f) noexcept {
			if (!ctx.font || !ctx.font->Valid() || !s || !*s) return;
			ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(), {x, y + ctx.font->Ascent()}, s, col,
							 maxWidth);
		}

		/// Texte centre horizontalement ET verticalement dans `r`.
		inline void NkcTextCenter(NkGuiContext &ctx, const NkRect &r, const char *s,
								  const NkColor &col) noexcept {
			if (!ctx.font || !ctx.font->Valid() || !s || !*s) return;
			const float32 tw = ctx.font->MeasureWidth(s);
			const float32 bx = r.x + (r.w - tw) * 0.5f;
			const float32 by = r.y + (r.h - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent();
			ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(), {bx, by}, s, col, r.w);
		}

		/// Texte aligne a droite dans `r`, centre verticalement.
		inline void NkcTextRight(NkGuiContext &ctx, const NkRect &r, const char *s,
								 const NkColor &col) noexcept {
			if (!ctx.font || !ctx.font->Valid() || !s || !*s) return;
			const float32 tw = ctx.font->MeasureWidth(s);
			const float32 bx = r.x + r.w - tw;
			const float32 by = r.y + (r.h - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent();
			ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(), {bx, by}, s, col, r.w);
		}

		// ---------------------------------------------------------------------
		// Polygones
		// ---------------------------------------------------------------------
		/// Polygone CONVEXE plein, en eventail depuis le sommet 0. Suffisant pour
		/// des hexagones et des carres reguliers.
		inline void NkcPolyFilled(nkgui::NkGuiDrawList &dl, const NkVec2 *pts, int32 n,
								  const NkColor &col) noexcept {
			if (!pts || n < 3) return;
			for (int32 i = 1; i + 1 < n; ++i) dl.AddTriangleFilled(pts[0], pts[i], pts[i + 1], col);
		}

		inline void NkcPolyOutline(nkgui::NkGuiDrawList &dl, const NkVec2 *pts, int32 n,
								   const NkColor &col, float32 thickness = 1.f) noexcept {
			if (!pts || n < 2) return;
			for (int32 i = 0; i < n; ++i) dl.AddLine(pts[i], pts[(i + 1) % n], col, thickness);
		}

		/// Anneau (cercle en contour). `segs = 0` : resolution deduite du rayon —
		/// 12 segments suffisent a 8 px, 48 sont necessaires a 80 px.
		inline void NkcRing(nkgui::NkGuiDrawList &dl, const NkVec2 &c, float32 radius,
							const NkColor &col, float32 thickness = 2.f, int32 segs = 0) noexcept {
			if (radius <= 0.5f) return;
			if (segs <= 0) {
				segs = static_cast<int32>(radius * 0.8f);
				if (segs < 12) segs = 12;
				if (segs > 48) segs = 48;
			}
			// cos/sin incrementaux : un seul appel trigonometrique par anneau.
			const float32 step = 6.2831853f / static_cast<float32>(segs);
			const float32 ca   = math::NkCos(step);
			const float32 sa   = math::NkSin(step);
			float32		  dx   = radius;
			float32		  dy   = 0.f;
			NkVec2		  prev = {c.x + dx, c.y + dy};
			for (int32 i = 0; i < segs; ++i) {
				const float32 nx = dx * ca - dy * sa;
				const float32 ny = dx * sa + dy * ca;
				dx				 = nx;
				dy				 = ny;
				const NkVec2 cur = {c.x + dx, c.y + dy};
				dl.AddLine(prev, cur, col, thickness);
				prev = cur;
			}
		}

		/// Polygone en contour, retreci vers son centre — sert aux surbrillances
		/// posees a l'interieur d'une cellule sans mordre sur la voisine.
		inline void NkcPolyOutlineInset(nkgui::NkGuiDrawList &dl, const NkVec2 *pts, int32 n,
										const NkVec2 &center, float32 inset, const NkColor &col,
										float32 thickness = 2.f) noexcept {
			if (!pts || n < 3 || n > 8) return;
			NkVec2 tmp[8];
			for (int32 i = 0; i < n; ++i) {
				tmp[i].x = center.x + (pts[i].x - center.x) * inset;
				tmp[i].y = center.y + (pts[i].y - center.y) * inset;
			}
			NkcPolyOutline(dl, tmp, n, col, thickness);
		}

		// ---------------------------------------------------------------------
		/// Zone de dessin REELLEMENT visible au curseur de layout. Sans cette
		/// intersection, `AvailHeight()` vaut ~1e9 dans une zone defilable et la
		/// barre de defilement part a un million de pixels (cours NKGui §3.9).
		inline NkRect NkcVisibleRect(NkGuiContext &ctx) noexcept {
			const NkRect clip = ctx.DL().CurrentClip();
			NkRect		 r	  = {ctx.layout.cursor.x, ctx.layout.cursor.y, ctx.ContentWidth(),
								 ctx.AvailHeight()};
			if (r.x < clip.x) { r.w -= (clip.x - r.x); r.x = clip.x; }
			if (r.y < clip.y) { r.h -= (clip.y - r.y); r.y = clip.y; }
			if (r.x + r.w > clip.x + clip.w) r.w = clip.x + clip.w - r.x;
			if (r.y + r.h > clip.y + clip.h) r.h = clip.y + clip.h - r.y;
			if (r.w < 0.f) r.w = 0.f;
			if (r.h < 0.f) r.h = 0.f;
			return r;
		}

	} // namespace conqueror
} // namespace nkentseu
