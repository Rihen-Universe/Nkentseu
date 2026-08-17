// =============================================================================
// UI/NkoungDraw.h
// Primitives de dessin de Nkoung au-dessus de NkGuiDrawList (NKGui).
//
// Portage NKUI -> NKGui (campagne de retrait NKUI, 2026-08). NkGuiDrawList n'a
// ni contour de rectangle ARRONDI (AddRect est carré, tracé à l'intérieur), ni
// contour de cercle : on les émule ici par des anneaux de quads
// (AddTriangleFilled), trait CENTRÉ sur le bord comme le faisait NKUI
// (PathStroke). Le texte passe par AddText(face, texId, BASELINE) : les
// helpers prennent un Y de HAUT et ajoutent l'ascent de la police.
// =============================================================================
#pragma once

#ifndef NKOUNG_DRAW_H
#define NKOUNG_DRAW_H

#include "NKMath/NKMath.h"
#include "NKGui/NKGui.h"

namespace nkoung {
	namespace draw {

		using nkentseu::float32;
		using nkentseu::int32;
		using nkentseu::uint32;
		using nkentseu::math::NkColor;
		using nkentseu::math::NkFloatRect;
		using nkentseu::math::NkVec2f;
		using nkentseu::nkgui::NkGuiDrawList;
		using nkentseu::nkgui::NkGuiFont;

		// Même règle de segments automatique que NkGuiDrawList::AddCircleFilled.
		inline int32 AutoSegs(float32 r) noexcept {
			int32 segs = static_cast<int32>(8.f * r / 4.f) + 8;
			if (segs < 12)
				segs = 12;
			else if (segs > 128)
				segs = 128;
			return segs;
		}

		// Anneau (contour de cercle) : rayon extérieur r + th/2, intérieur r - th/2.
		inline void CircleOutline(NkGuiDrawList &dl, NkVec2f c, float32 r, const NkColor &col, float32 th,
								  int32 segs = 0) noexcept {
			if (r <= 0.f || th <= 0.f)
				return;
			if (segs <= 0)
				segs = AutoSegs(r);
			const float32 ro = r + th * 0.5f;
			float32 ri = r - th * 0.5f;
			if (ri < 0.f)
				ri = 0.f;
			const float32 kTau = 6.28318530718f;
			float32 px = 1.f, py = 0.f;
			for (int32 s = 1; s <= segs; ++s) {
				const float32 ang = kTau * static_cast<float32>(s) / static_cast<float32>(segs);
				const float32 cx = nkentseu::math::NkCos(ang), cy = nkentseu::math::NkSin(ang);
				const NkVec2f o0{c.x + px * ro, c.y + py * ro}, o1{c.x + cx * ro, c.y + cy * ro};
				const NkVec2f i1{c.x + cx * ri, c.y + cy * ri}, i0{c.x + px * ri, c.y + py * ri};
				dl.AddTriangleFilled(o0, o1, i1, col);
				dl.AddTriangleFilled(o0, i1, i0, col);
				px = cx;
				py = cy;
			}
		}

		// Contour de rectangle, arrondi ou non, trait centré sur le bord (NKUI).
		inline void RectOutline(NkGuiDrawList &dl, const NkFloatRect &r, const NkColor &col, float32 th,
								float32 round = 0.f) noexcept {
			if (r.w <= 0.f || r.h <= 0.f || th <= 0.f)
				return;
			const float32 half = th * 0.5f;
			if (round <= 0.f) {
				// 4 quads entre le rect élargi de th/2 et le rect rétréci de th/2.
				const float32 x0 = r.x - half, y0 = r.y - half, x1 = r.x + r.w + half, y1 = r.y + r.h + half;
				const float32 ix0 = r.x + half, iy0 = r.y + half, ix1 = r.x + r.w - half, iy1 = r.y + r.h - half;
				dl.AddRectFilled(NkFloatRect{x0, y0, x1 - x0, iy0 - y0}, col);	 // haut
				dl.AddRectFilled(NkFloatRect{x0, iy1, x1 - x0, y1 - iy1}, col);	 // bas
				dl.AddRectFilled(NkFloatRect{x0, iy0, ix0 - x0, iy1 - iy0}, col); // gauche
				dl.AddRectFilled(NkFloatRect{ix1, iy0, x1 - ix1, iy1 - iy0}, col); // droite
				return;
			}
			float32 rr = round;
			const float32 rmax = (r.w < r.h ? r.w : r.h) * 0.5f;
			if (rr > rmax)
				rr = rmax;
			const float32 ro = rr + half;
			float32 ri = rr - half;
			if (ri < 0.f)
				ri = 0.f;
			// Segments par coin (quart de cercle) : un quart de la règle du cercle.
			int32 n = AutoSegs(rr) / 4;
			if (n < 3)
				n = 3;
			else if (n > 16)
				n = 16;
			// Centres des 4 coins + angle de départ (sens horaire, y vers le bas) :
			// TL pi -> 3pi/2, TR 3pi/2 -> 2pi, BR 0 -> pi/2, BL pi/2 -> pi.
			const float32 kPi = 3.14159265359f;
			const NkVec2f centers[4] = {{r.x + rr, r.y + rr},
										{r.x + r.w - rr, r.y + rr},
										{r.x + r.w - rr, r.y + r.h - rr},
										{r.x + rr, r.y + r.h - rr}};
			const float32 a0s[4] = {kPi, 1.5f * kPi, 0.f, 0.5f * kPi};
			// 4 * (n + 1) points, n <= 16 -> 68 max.
			NkVec2f outer[68], inner[68];
			int32 count = 0;
			for (int32 k = 0; k < 4; ++k) {
				for (int32 i = 0; i <= n; ++i) {
					const float32 ang = a0s[k] + 0.5f * kPi * static_cast<float32>(i) / static_cast<float32>(n);
					const float32 cx = nkentseu::math::NkCos(ang), cy = nkentseu::math::NkSin(ang);
					outer[count] = {centers[k].x + cx * ro, centers[k].y + cy * ro};
					inner[count] = {centers[k].x + cx * ri, centers[k].y + cy * ri};
					++count;
				}
			}
			for (int32 i = 0; i < count; ++i) {
				const int32 j = (i + 1) % count;
				dl.AddTriangleFilled(outer[i], outer[j], inner[j], col);
				dl.AddTriangleFilled(outer[i], inner[j], inner[i], col);
			}
		}

		// Texte : Y de HAUT -> baseline = topY + Ascent().
		inline void Text(NkGuiDrawList &dl, NkGuiFont *f, float32 x, float32 topY, const char *s, const NkColor &c,
						 float32 maxW = -1.f) noexcept {
			if (f && f->Face() && s)
				dl.AddText(f->Face(), f->TexId(), NkVec2f{x, topY + f->Ascent()}, s, c, maxW);
		}

		inline float32 TextW(NkGuiFont *f, const char *s) noexcept {
			return (f && s) ? f->MeasureWidth(s) : 0.f;
		}

		inline float32 LineH(NkGuiFont *f, float32 fallback = 18.f) noexcept {
			return (f && f->Face()) ? f->LineHeight() : fallback;
		}

		inline void TextCentered(NkGuiDrawList &dl, NkGuiFont *f, float32 x, float32 w, float32 topY, const char *s,
								 const NkColor &c) noexcept {
			if (f && s)
				Text(dl, f, x + (w - f->MeasureWidth(s)) * 0.5f, topY, s, c);
		}

		// Image plein cadre (uv 0..1), teinte blanche par défaut.
		inline void Image(NkGuiDrawList &dl, uint32 texId, const NkFloatRect &r,
						  const NkColor &tint = NkColor{255, 255, 255, 255}) noexcept {
			if (texId)
				dl.AddImage(texId, r, NkVec2f{0.f, 0.f}, NkVec2f{1.f, 1.f}, tint);
		}

	} // namespace draw
} // namespace nkoung

#endif // NKOUNG_DRAW_H
