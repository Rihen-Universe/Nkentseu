// =============================================================================
// NkGuiDrawList.cpp — primitives de dessin NKGui (Phase 2).
// =============================================================================
#include "NKGui/Core/NkGuiDrawList.h"
#include "NKFont/NkFont.h"
#include <cmath>

namespace nkentseu {
	namespace nkgui {

		void NkGuiDrawList::Reset() noexcept {
			vtx.Clear();
			idx.Clear();
			cmds.Clear();
			clipDepth = 0;
		}

		void NkGuiDrawList::Append(const NkGuiDrawList &o) noexcept {
			if (o.idx.Size() == 0u)
				return;
			const uint32 vbase = static_cast<uint32>(vtx.Size());
			const uint32 ibase = static_cast<uint32>(idx.Size());
			for (uint32 i = 0; i < o.vtx.Size(); ++i)
				vtx.PushBack(o.vtx[i]);
			for (uint32 i = 0; i < o.idx.Size(); ++i)
				idx.PushBack(o.idx[i] + vbase);
			for (uint32 i = 0; i < o.cmds.Size(); ++i) {
				NkGuiDrawCmd c = o.cmds[i];
				c.idxOffset += ibase;
				cmds.PushBack(c);
			}
		}

		NkRect NkGuiDrawList::CurrentClip() const noexcept {
			return clipDepth > 0 ? clipStack[clipDepth - 1] : NkRect{0.f, 0.f, 1.0e9f, 1.0e9f};
		}

		void NkGuiDrawList::PushClipRect(const NkRect &r, bool intersect) noexcept {
			NkRect c = r;
			if (intersect && clipDepth > 0) {
				const NkRect &p = clipStack[clipDepth - 1];
				const float32 x1 = (c.x > p.x) ? c.x : p.x;
				const float32 y1 = (c.y > p.y) ? c.y : p.y;
				const float32 x2 = ((c.x + c.w) < (p.x + p.w)) ? (c.x + c.w) : (p.x + p.w);
				const float32 y2 = ((c.y + c.h) < (p.y + p.h)) ? (c.y + c.h) : (p.y + p.h);
				c = {x1, y1, x2 > x1 ? x2 - x1 : 0.f, y2 > y1 ? y2 - y1 : 0.f};
			}
			if (clipDepth < 32)
				clipStack[clipDepth++] = c;
		}

		void NkGuiDrawList::PopClipRect() noexcept {
			if (clipDepth > 0)
				--clipDepth;
		}

		NkGuiDrawCmd &NkGuiDrawList::CurCmd(uint32 texId) noexcept {
			const NkRect clip = CurrentClip();
			bool need = (cmds.Size() == 0);
			if (!need) {
				const NkGuiDrawCmd &b = cmds.Back();
				need = (b.texId != texId) || b.clipRect.x != clip.x || b.clipRect.y != clip.y ||
					   b.clipRect.w != clip.w || b.clipRect.h != clip.h;
			}
			if (need) {
				NkGuiDrawCmd c;
				c.texId = texId;
				c.clipRect = clip;
				c.idxOffset = static_cast<uint32>(idx.Size());
				c.idxCount = 0;
				c.type = texId ? NkGuiDrawCmdType::TexturedTriangles : NkGuiDrawCmdType::Triangles;
				cmds.PushBack(c);
			}
			return cmds.Back();
		}

		uint32 NkGuiDrawList::Vtx(const NkVec2 &p, const NkVec2 &uv, uint32 col) noexcept {
			NkGuiVertex v;
			v.pos = p;
			v.uv = uv;
			v.col = col;
			vtx.PushBack(v);
			return static_cast<uint32>(vtx.Size()) - 1u;
		}

		void NkGuiDrawList::Tri(uint32 a, uint32 b, uint32 c, uint32 texId) noexcept {
			NkGuiDrawCmd &cmd = CurCmd(texId);
			idx.PushBack(a);
			idx.PushBack(b);
			idx.PushBack(c);
			cmd.idxCount += 3u;
		}

		void NkGuiDrawList::AddRectFilled(const NkRect &r, const NkColor &col, float32 rounding) noexcept {
			if (r.w <= 0.f || r.h <= 0.f)
				return;
			const uint32 c = NkGuiPackColor(col);
			const NkVec2 uv{0.f, 0.f};

			float32 rad = rounding;
			const float32 maxr = (r.w < r.h ? r.w : r.h) * 0.5f;
			if (rad > maxr)
				rad = maxr;
			if (rad < 0.5f) { // coins droits
				const uint32 i0 = Vtx({r.x, r.y}, uv, c);
				const uint32 i1 = Vtx({r.x + r.w, r.y}, uv, c);
				const uint32 i2 = Vtx({r.x + r.w, r.y + r.h}, uv, c);
				const uint32 i3 = Vtx({r.x, r.y + r.h}, uv, c);
				Tri(i0, i1, i2, 0u);
				Tri(i0, i2, i3, 0u);
				return;
			}

			// Rectangle ARRONDI : 4 arcs de coin tries en eventail depuis le centre.
			const float32 x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h;
			const NkVec2 cc[4] = {
				{x0 + rad, y0 + rad}, // haut-gauche
				{x1 - rad, y0 + rad}, // haut-droite
				{x1 - rad, y1 - rad}, // bas-droite
				{x0 + rad, y1 - rad}, // bas-gauche
			};
			const float32 PI = 3.14159265358979f;
			const float32 a0[4] = {PI, 1.5f * PI, 0.f, 0.5f * PI}; // sens horaire (y vers le bas)
			const int32 seg = 4;								   // segments par coin
			const uint32 ic = Vtx({(x0 + x1) * 0.5f, (y0 + y1) * 0.5f}, uv, c);
			uint32 prev = 0, first = 0;
			bool has = false;
			for (int32 k = 0; k < 4; ++k) {
				for (int32 s = 0; s <= seg; ++s) {
					const float32 a = a0[k] + (0.5f * PI) * (static_cast<float32>(s) / static_cast<float32>(seg));
					const uint32 v = Vtx({cc[k].x + std::cos(a) * rad, cc[k].y + std::sin(a) * rad}, uv, c);
					if (has)
						Tri(ic, prev, v, 0u);
					else
						first = v;
					prev = v;
					has = true;
				}
			}
			if (has)
				Tri(ic, prev, first, 0u); // ferme l'anneau
		}

		void NkGuiDrawList::AddTriangleMultiColor(const NkVec2 &a, const NkVec2 &b, const NkVec2 &c, const NkColor &ca,
												  const NkColor &cb, const NkColor &cc) noexcept {
			// Triangle à couleurs de sommet (dégradé barycentrique) — roue de teinte +
			// triangle SV du color picker circulaire.
			const NkVec2 uv{0.f, 0.f};
			const uint32 i0 = Vtx(a, uv, NkGuiPackColor(ca));
			const uint32 i1 = Vtx(b, uv, NkGuiPackColor(cb));
			const uint32 i2 = Vtx(c, uv, NkGuiPackColor(cc));
			Tri(i0, i1, i2, 0u);
		}

		void NkGuiDrawList::AddImage(uint32 texId, const NkRect &r, const NkVec2 &uv0, const NkVec2 &uv1,
									 const NkColor &tint) noexcept {
			// Quad TEXTURÉ (texId backend) — image/icône. La couleur de sommet `tint`
			// multiplie l'échantillon (blanc = image telle quelle). Même chemin que le
			// texte (TexturedTriangles), donc le backend résout déjà `texId`.
			if (r.w <= 0.f || r.h <= 0.f || texId == 0u)
				return;
			const uint32 c = NkGuiPackColor(tint);
			const uint32 i0 = Vtx({r.x, r.y}, {uv0.x, uv0.y}, c);
			const uint32 i1 = Vtx({r.x + r.w, r.y}, {uv1.x, uv0.y}, c);
			const uint32 i2 = Vtx({r.x + r.w, r.y + r.h}, {uv1.x, uv1.y}, c);
			const uint32 i3 = Vtx({r.x, r.y + r.h}, {uv0.x, uv1.y}, c);
			Tri(i0, i1, i2, texId);
			Tri(i0, i2, i3, texId);
		}

		void NkGuiDrawList::AddRectFilledMultiColor(const NkRect &r, const NkColor &tl, const NkColor &tr,
													const NkColor &br, const NkColor &bl) noexcept {
			// Quad à couleurs de coin (dégradé bilinéaire) — base du sélecteur de
			// couleur (carré SV, barre de teinte, barre alpha). uv=(0,0) → couleur unie.
			if (r.w <= 0.f || r.h <= 0.f)
				return;
			const NkVec2 uv{0.f, 0.f};
			const uint32 i0 = Vtx({r.x, r.y}, uv, NkGuiPackColor(tl));
			const uint32 i1 = Vtx({r.x + r.w, r.y}, uv, NkGuiPackColor(tr));
			const uint32 i2 = Vtx({r.x + r.w, r.y + r.h}, uv, NkGuiPackColor(br));
			const uint32 i3 = Vtx({r.x, r.y + r.h}, uv, NkGuiPackColor(bl));
			Tri(i0, i1, i2, 0u);
			Tri(i0, i2, i3, 0u);
		}

		void NkGuiDrawList::AddLine(const NkVec2 &a, const NkVec2 &b, const NkColor &col, float32 thickness) noexcept {
			thickness *= thickScale; // épaisseur en px écran (DPI)
			float32 dx = b.x - a.x, dy = b.y - a.y;
			const float32 len = std::sqrt(dx * dx + dy * dy);
			if (len < 1.0e-4f)
				return;
			const float32 nx = -dy / len * thickness * 0.5f;
			const float32 ny = dx / len * thickness * 0.5f;
			const uint32 c = NkGuiPackColor(col);
			const NkVec2 uv{0.f, 0.f};
			const uint32 i0 = Vtx({a.x + nx, a.y + ny}, uv, c);
			const uint32 i1 = Vtx({b.x + nx, b.y + ny}, uv, c);
			const uint32 i2 = Vtx({b.x - nx, b.y - ny}, uv, c);
			const uint32 i3 = Vtx({a.x - nx, a.y - ny}, uv, c);
			Tri(i0, i1, i2, 0u);
			Tri(i0, i2, i3, 0u);
		}

		// Segments d'un quart de cercle pour un rayon donne — MEME regle que
		// AddCircleFilled (8*r/4 + 8, borne 12..128), divisee par 4 puis bornee
		// 3..16. Deterministe : c'est ce qui rend la geometrie verifiable au banc.
		static inline int32 NkGuiArcSegs(float32 radius) noexcept {
			int32 segs = static_cast<int32>(8.f * radius / 4.f) + 8;
			if (segs < 12)
				segs = 12;
			else if (segs > 128)
				segs = 128;
			int32 n = segs / 4;
			if (n < 3)
				n = 3;
			else if (n > 16)
				n = 16;
			return n;
		}

		void NkGuiDrawList::AddRect(const NkRect &r, const NkColor &col, float32 thickness,
									float32 rounding) noexcept {
			const float32 t = thickness * thickScale; // épaisseur en px écran (DPI)
			if (r.w <= 0.f || r.h <= 0.f || t <= 0.f)
				return;

			if (rounding < 0.5f) {
				// Bordure DROITE = 4 rectangles pleins tracés STRICTEMENT à l'intérieur
				// du rect. (AddLine centrerait l'épaisseur sur l'arête → la moitié
				// extérieure sort du rect et est rognée par le clip — scissor exclusif à
				// droite/bas → bord droit invisible/aminci.) Chemin HISTORIQUE, inchangé :
				// chaque bord tient dans [r.x, r.x+r.w] etc.
				AddRectFilled({r.x, r.y, r.w, t}, col);			  // haut
				AddRectFilled({r.x, r.y + r.h - t, r.w, t}, col); // bas
				AddRectFilled({r.x, r.y, t, r.h}, col);			  // gauche
				AddRectFilled({r.x + r.w - t, r.y, t, r.h}, col); // droite
				return;
			}

			// Bordure ARRONDIE : anneau entre le contour de `r` (rayon `ro`) et celui
			// du rect rentre de `t` (rayon `ri`). Meme convention « strictement a
			// l'interieur » que le cas droit — le bord exterieur EST celui de `r`.
			const float32 x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h;
			float32 ro = rounding;
			const float32 romax = (r.w < r.h ? r.w : r.h) * 0.5f;
			if (ro > romax)
				ro = romax;
			// Rect INTERIEUR (rentre de t). S'il est degenere, l'anneau couvre tout :
			// on retombe sur le disque plein arrondi, ce qui est le resultat juste.
			const float32 iw = r.w - 2.f * t, ih = r.h - 2.f * t;
			if (iw <= 0.f || ih <= 0.f) {
				AddRectFilled(r, col, ro);
				return;
			}
			float32 ri = ro - t;
			if (ri < 0.f)
				ri = 0.f;
			const float32 rimax = (iw < ih ? iw : ih) * 0.5f;
			if (ri > rimax)
				ri = rimax;

			const int32 n = NkGuiArcSegs(ro);
			const float32 PI = 3.14159265358979f;
			// Centres d'arc, sens horaire (y vers le bas) : HG, HD, BD, BG.
			const NkVec2 co[4] = {{x0 + ro, y0 + ro}, {x1 - ro, y0 + ro}, {x1 - ro, y1 - ro}, {x0 + ro, y1 - ro}};
			const NkVec2 ci[4] = {{x0 + t + ri, y0 + t + ri},
								  {x1 - t - ri, y0 + t + ri},
								  {x1 - t - ri, y1 - t - ri},
								  {x0 + t + ri, y1 - t - ri}};
			const float32 a0[4] = {PI, 1.5f * PI, 0.f, 0.5f * PI};

			const uint32 c = NkGuiPackColor(col);
			const NkVec2 uv{0.f, 0.f};
			uint32 pO = 0, pI = 0, fO = 0, fI = 0;
			bool has = false;
			for (int32 k = 0; k < 4; ++k) {
				for (int32 i = 0; i <= n; ++i) {
					const float32 a = a0[k] + (0.5f * PI) * (static_cast<float32>(i) / static_cast<float32>(n));
					const float32 cs = std::cos(a), sn = std::sin(a);
					const uint32 vO = Vtx({co[k].x + cs * ro, co[k].y + sn * ro}, uv, c);
					const uint32 vI = Vtx({ci[k].x + cs * ri, ci[k].y + sn * ri}, uv, c);
					if (has) {
						Tri(pO, vO, vI, 0u);
						Tri(pO, vI, pI, 0u);
					} else {
						fO = vO;
						fI = vI;
					}
					pO = vO;
					pI = vI;
					has = true;
				}
			}
			if (has) { // ferme l'anneau
				Tri(pO, fO, fI, 0u);
				Tri(pO, fI, pI, 0u);
			}
		}

		void NkGuiDrawList::AddCircle(const NkVec2 &center, float32 r, const NkColor &col, float32 thickness,
									  int32 segs) noexcept {
			const float32 th = thickness * thickScale;
			if (r <= 0.f || th <= 0.f)
				return;
			if (segs <= 0) {
				segs = static_cast<int32>(8.f * r / 4.f) + 8;
				if (segs < 12)
					segs = 12;
				else if (segs > 128)
					segs = 128;
			}
			// `r` = ligne MEDIANE (convention des emulations remplacees).
			const float32 ro = r + th * 0.5f;
			float32 ri = r - th * 0.5f;
			if (ri < 0.f)
				ri = 0.f;
			const uint32 c = NkGuiPackColor(col);
			const NkVec2 uv{0.f, 0.f};
			const float32 kTau = 6.28318530718f;
			uint32 pO = Vtx({center.x + ro, center.y}, uv, c);
			uint32 pI = Vtx({center.x + ri, center.y}, uv, c);
			const uint32 fO = pO, fI = pI;
			for (int32 s = 1; s < segs; ++s) {
				const float32 a = kTau * static_cast<float32>(s) / static_cast<float32>(segs);
				const float32 cs = std::cos(a), sn = std::sin(a);
				const uint32 vO = Vtx({center.x + cs * ro, center.y + sn * ro}, uv, c);
				const uint32 vI = Vtx({center.x + cs * ri, center.y + sn * ri}, uv, c);
				Tri(pO, vO, vI, 0u);
				Tri(pO, vI, pI, 0u);
				pO = vO;
				pI = vI;
			}
			Tri(pO, fO, fI, 0u); // ferme l'anneau
			Tri(pO, fI, pI, 0u);
		}

		void NkGuiDrawList::AddConvexPolyFilled(const NkVec2 *pts, int32 n, const NkColor &col) noexcept {
			if (!pts || n < 3)
				return;
			const uint32 c = NkGuiPackColor(col);
			const NkVec2 uv{0.f, 0.f};
			const uint32 i0 = Vtx(pts[0], uv, c);
			uint32 prev = Vtx(pts[1], uv, c);
			for (int32 i = 2; i < n; ++i) {
				const uint32 cur = Vtx(pts[i], uv, c);
				Tri(i0, prev, cur, 0u);
				prev = cur;
			}
		}

		void NkGuiDrawList::AddPolyline(const NkVec2 *pts, int32 n, const NkColor &col, float32 thickness,
										bool closed) noexcept {
			if (!pts || n < 2)
				return;
			// Un quad par segment, sans raccord d'onglet (miter) — meme modele que
			// AddLine, dont c'est la generalisation. Aux angles vifs les deux quads
			// laissent une encoche ; c'est assume (les emulations remplacees faisaient
			// deja exactement cela).
			const int32 last = closed ? n : n - 1;
			for (int32 i = 0; i < last; ++i)
				AddLine(pts[i], pts[(i + 1) % n], col, thickness);
		}

		void NkGuiDrawList::AddTriangleFilled(const NkVec2 &a, const NkVec2 &b, const NkVec2 &c,
											  const NkColor &col) noexcept {
			const uint32 cc = NkGuiPackColor(col);
			const NkVec2 uv{0.f, 0.f};
			const uint32 i0 = Vtx(a, uv, cc);
			const uint32 i1 = Vtx(b, uv, cc);
			const uint32 i2 = Vtx(c, uv, cc);
			Tri(i0, i1, i2, 0u);
		}

		// ── ALIGNEMENT D'UN GLYPHE SUR LA GRILLE DE PIXELS ──────────────────────
		// Le curseur de texte accumule des avances FRACTIONNAIRES. Sans arrondi,
		// seul le PREMIER glyphe d'une chaîne tombe sur un pixel entier ; tous les
		// suivants dérivent et échantillonnent l'atlas ENTRE deux texels, ce qui
		// rend le texte uniformément flou. Le défaut est d'autant plus marqué que le
		// corps est petit : l'erreur vaut un demi-texel CONSTANT, soit 4 % de la
		// hauteur d'un caractère à 13 px et 3,3 % à 15 px.
		static inline float32 NkGuiPixelSnap(float32 v) noexcept {
			return (float32)(int32)(v < 0.f ? v - 0.5f : v + 0.5f);
		}

		void NkGuiDrawList::AddText(const NkFont *face, uint32 texId, const NkVec2 &baseline, const char *text,
									const NkColor &col, float32 maxWidth, float32 skew,
									const char *textEnd) noexcept {
			if (!face || !text || !*text || texId == 0u)
				return;
			const uint32 c = NkGuiPackColor(col);
			const char *p = text;
			// `textEnd` borne la partie dessinee (convention ##id) ; nullptr =
			// jusqu'au NUL, comportement historique.
			const char *end = textEnd;
			if (!end) {
				end = text;
				while (*end)
					++end; // fin de chaîne (UTF-8)
			}
			if (p >= end)
				return; // partie affichee vide (libelle entierement ##id)
			const float32 xEnd = (maxWidth >= 0.f) ? baseline.x + maxWidth : 1.0e30f;
			float32 x = baseline.x;
			const float32 y = baseline.y;

			while (p < end) {
				const NkFontCodepoint cp = NkFontDecodeUTF8(&p, end);
				if (cp == 0u)
					break;
				const NkFontGlyph *g = face->FindGlyph(cp);
				if (!g)
					continue;
				if (g->visible) {
					// On arrondit la POSITION du quad, jamais l'AVANCE : `x` continue
					// d'accumuler la valeur exacte, donc la largeur totale de la chaîne
					// est inchangée et MeasureWidth reste d'accord avec le rendu. Seul
					// l'espacement entre deux glyphes varie de moins d'un pixel, ce qui
					// ne se voit pas — alors que le flou, lui, se voyait.
					//
					// La LARGEUR est reportée telle quelle : arrondir les deux bords
					// séparément étirerait le glyphe d'un pixel et le rééchantillonnerait,
					// ce qu'on cherche précisément à éviter.
					const float32 x0 = NkGuiPixelSnap(x + g->x0), y0 = NkGuiPixelSnap(y + g->y0);
					const float32 x1 = x0 + (g->x1 - g->x0), y1 = y0 + (g->y1 - g->y0);
					if (x1 > xEnd)
						break; // troncature simple
					// Italique factice : décale chaque sommet selon sa hauteur au-dessus
					// de la ligne de base (haut penché à droite, descendantes à gauche).
					const float32 sTop = skew != 0.f ? skew * (y - y0) : 0.f;
					const float32 sBot = skew != 0.f ? skew * (y - y1) : 0.f;
					const uint32 i0 = Vtx({x0 + sTop, y0}, {g->u0, g->v0}, c);
					const uint32 i1 = Vtx({x1 + sTop, y0}, {g->u1, g->v0}, c);
					const uint32 i2 = Vtx({x1 + sBot, y1}, {g->u1, g->v1}, c);
					const uint32 i3 = Vtx({x0 + sBot, y1}, {g->u0, g->v1}, c);
					Tri(i0, i1, i2, texId);
					Tri(i0, i2, i3, texId);
				}
				x += g->advanceX;
			}
		}

		void NkGuiDrawList::AddTextRange(const NkFont *face, uint32 texId, const NkVec2 &baseline, const char *begin,
										 const char *end, const NkColor &col) noexcept {
			if (!face || !begin || begin >= end || texId == 0u)
				return;
			const uint32 c = NkGuiPackColor(col);
			const char *p = begin;
			float32 x = baseline.x;
			const float32 y = baseline.y;
			while (p < end) {
				const NkFontCodepoint cp = NkFontDecodeUTF8(&p, end);
				if (cp == 0u)
					break;
				const NkFontGlyph *g = face->FindGlyph(cp);
				if (!g)
					continue;
				if (g->visible) {
					// Même alignement que dans AddText : sans lui, ce chemin resterait
					// flou alors que l'autre serait net — et le défaut serait d'autant
					// plus déroutant qu'il ne toucherait qu'une partie du texte.
					const float32 x0 = NkGuiPixelSnap(x + g->x0), y0 = NkGuiPixelSnap(y + g->y0);
					const float32 x1 = x0 + (g->x1 - g->x0), y1 = y0 + (g->y1 - g->y0);
					const uint32 i0 = Vtx({x0, y0}, {g->u0, g->v0}, c);
					const uint32 i1 = Vtx({x1, y0}, {g->u1, g->v0}, c);
					const uint32 i2 = Vtx({x1, y1}, {g->u1, g->v1}, c);
					const uint32 i3 = Vtx({x0, y1}, {g->u0, g->v1}, c);
					Tri(i0, i1, i2, texId);
					Tri(i0, i2, i3, texId);
				}
				x += g->advanceX;
			}
		}

		void NkGuiDrawList::AddCircleFilled(const NkVec2 &center, float32 r, const NkColor &col, int32 segs) noexcept {
			if (r <= 0.f)
				return;
			if (segs <= 0) {
				segs = static_cast<int32>(8.f * r / 4.f) + 8;
				if (segs < 12)
					segs = 12;
				else if (segs > 128)
					segs = 128;
			}
			const uint32 cc = NkGuiPackColor(col);
			const NkVec2 uv{0.f, 0.f};
			const uint32 ic = Vtx(center, uv, cc);
			const float32 kTau = 6.28318530718f;
			uint32 prev = Vtx({center.x + r, center.y}, uv, cc);
			for (int32 s = 1; s <= segs; ++s) {
				const float32 ang = kTau * static_cast<float32>(s) / static_cast<float32>(segs);
				const uint32 cur = Vtx({center.x + std::cos(ang) * r, center.y + std::sin(ang) * r}, uv, cc);
				Tri(ic, prev, cur, 0u);
				prev = cur;
			}
		}

	} // namespace nkgui
} // namespace nkentseu
