// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkGuiDrawList.cpp — primitives de dessin NKGui (Phase 2).
// =============================================================================
#include "NKGui/Core/NkGuiDrawList.h"
#include "NKFont/NkFont.h"
#include "NKFont/Core/NkFontSizeCache.h" // NkFontScaleRenderer : quads de glyphe a l'echelle
#include <cmath>

namespace nkentseu {
	namespace nkgui {

		void NkGuiDrawList::Reset() noexcept {
			vtx.Clear();
			idx.Clear();
			cmds.Clear();
			clipDepth = 0;
			blendDepth = 0; // 2026-09-04
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
			const NkGuiBlend blend = CurrentBlend(); // 2026-09-04 : un mode de melange par commande
			bool need = (cmds.Size() == 0);
			if (!need) {
				const NkGuiDrawCmd &b = cmds.Back();
				need = (b.texId != texId) || b.clipRect.x != clip.x || b.clipRect.y != clip.y ||
					   b.clipRect.w != clip.w || b.clipRect.h != clip.h || b.blend != blend;
			}
			if (need) {
				NkGuiDrawCmd c;
				c.texId = texId;
				c.clipRect = clip;
				c.blend = blend;
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

		// ── LA FINESSE DES COURBES : UNE SEULE ECRITURE ─────────────────────
		// 🔴 LE MEME CALCUL VIVAIT EN TROIS EXEMPLAIRES (cercle, ellipse, arc) et
		//    un QUATRIEME site l'ignorait carrement : le rectangle arrondi rempli
		//    portait `const int32 seg = 4;` en dur. Resultat mesure par Rodolf :
		//    *« les arrondis laissent des cassures, ce n'est pas lisse »* -- le
		//    FOND etait facette a 4 segments par coin pendant que les contours en
		//    prenaient au moins 12. Avec ou sans bordure, puisque le fond est
		//    anguleux de naissance.
		//
		// ⚠️ ET LE REMEDE N'EST PAS D'ECRIRE 12 A LA PLACE DE 4 : la finesse doit
		//    SUIVRE LE RAYON (un coin de 2 px n'a pas besoin de douze segments,
		//    un coin de 40 en veut plus). On appelle donc LA FONCTION, on
		//    n'invente pas un second nombre. *Une constante ecrite a la main a
		//    cote d'une fonction qui sait calculer la bonne valeur est une
		//    divergence qui attend son jour.*
		static inline int32 NkGuiCercleSegs(float32 radius) noexcept {
			int32 segs = static_cast<int32>(8.f * radius / 4.f) + 8;
			if (segs < 12)
				segs = 12;
			else if (segs > 128)
				segs = 128;
			return segs;
		}

		// Segments d'un QUART de cercle : la meme regle, divisee par quatre puis
		// bornee 3..16. Deterministe : c'est ce qui rend la geometrie verifiable
		// au banc.
		static inline int32 NkGuiArcSegs(float32 radius) noexcept {
			int32 n = NkGuiCercleSegs(radius) / 4;
			if (n < 3)
				n = 3;
			else if (n > 16)
				n = 16;
			return n;
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
			// La finesse SUIT LE RAYON -- voir NkGuiCercleSegs pour le pourquoi.
			const int32 seg = NkGuiArcSegs(rad);
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

		void NkGuiDrawList::AddImagePolygon(uint32 texId, const NkVec2 *pts, const NkVec2 *uvs, int32 n,
											 const NkColor &tint) noexcept {
			// Polygone CONVEXE texturé (uv par sommet) — l'image d'un remplissage rognée
			// par un contour arrondi, ou tournée. Même éventail qu'AddConvexPolyFilled,
			// même chemin que le texte et AddImage (TexturedTriangles) : le backend
			// résout déjà `texId`. Un uv (0,0) sur TOUS les sommets serait pris pour
			// « couleur unie » par le vertex : c'est le contrat des sommets, pas le nôtre.
			if (!pts || !uvs || n < 3 || texId == 0u)
				return;
			const uint32 c = NkGuiPackColor(tint);
			const uint32 i0 = Vtx(pts[0], uvs[0], c);
			uint32 prev = Vtx(pts[1], uvs[1], c);
			for (int32 i = 2; i < n; ++i) {
				const uint32 cur = Vtx(pts[i], uvs[i], c);
				Tri(i0, prev, cur, texId);
				prev = cur;
			}
		}

		void NkGuiDrawList::AddMesh(uint32 texId, const NkVec2 *pts, const NkVec2 *uvs, const NkColor *cols, int32 nVtx,
									const uint32 *indices, int32 nIdx) noexcept {
			// Un maillage INDEXE (le maillage 2D d'Unkeny) : les sommets une seule
			// fois, les triangles par indices. Meme chemin que AddImagePolygon
			// (TexturedTriangles si texId), mais la couleur est PAR SOMMET : une
			// teinte ou un fondu qui suit la deformation.
			if (!pts || !indices || nVtx < 3 || nIdx < 3)
				return;
			const uint32 base = static_cast<uint32>(vtx.Size());
			const NkVec2 zero{0.f, 0.f};
			const uint32 blanc = NkGuiPackColor(NkColor{255, 255, 255, 255});
			for (int32 i = 0; i < nVtx; ++i) {
				Vtx(pts[i], uvs ? uvs[i] : zero, cols ? NkGuiPackColor(cols[i]) : blanc);
			}
			for (int32 k = 0; k + 2 < nIdx; k += 3) {
				const uint32 a = indices[k], b = indices[k + 1], c = indices[k + 2];
				if (a >= static_cast<uint32>(nVtx) || b >= static_cast<uint32>(nVtx) || c >= static_cast<uint32>(nVtx))
					continue;
				Tri(base + a, base + b, base + c, texId);
			}
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
			// OBLIQUE : le trait lisse (frange) ; horizontal/vertical : le quad net.
			if (traitsLisses && std::fabs(b.x - a.x) > 1.0e-3f && std::fabs(b.y - a.y) > 1.0e-3f) {
				const NkVec2 deux[2] = {a, b};
				AddPolylineLisse(deux, 2, col, thickness, false, false);
				return;
			}
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
			if (segs <= 0)
				segs = NkGuiCercleSegs(r);
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
			// Un seul segment OBLIQUE : toute la ligne passe au trait lisse (raccords
			// et frange continus ; des quads separes se chevaucheraient aux sommets).
			if (traitsLisses) {
				for (int32 i = 0; i < last; ++i) {
					const NkVec2 &p = pts[i], &q = pts[(i + 1) % n];
					if (std::fabs(q.x - p.x) > 1.0e-3f && std::fabs(q.y - p.y) > 1.0e-3f) {
						AddPolylineLisse(pts, n, col, thickness, closed, false);
						return;
					}
				}
			}
			for (int32 i = 0; i < last; ++i)
				AddLine(pts[i], pts[(i + 1) % n], col, thickness);
		}

		// ── LES TRAITS LISSES (2026-10-02) ─────────────────────────────────────
		// Chaque point du chemin donne QUATRE sommets sur sa normale (onglet borne) :
		// bord+ (alpha 0), coeur+ (alpha plein), coeur- (alpha plein), bord- (alpha
		// 0). Entre deux points : six triangles (frange+, coeur, frange-). Le coeur
		// mesure t - 1 px, la frange 1 px de chaque cote : le poids visuel reste t.
		void NkGuiDrawList::AddPolylineLisse(const NkVec2 *pts, int32 n, const NkColor &col, float32 thickness,
											 bool closed, bool boutsRonds) noexcept {
			if (!pts || n < 2 || col.a == 0)
				return;
			if (!traitsLisses) {
				// Le chemin d'AVANT (contre-epreuve) : un quad net par segment.
				const int32 last = closed ? n : n - 1;
				for (int32 i = 0; i < last; ++i)
					AddLine(pts[i], pts[(i + 1) % n], col, thickness);
				return;
			}
			const float32 t = thickness * thickScale;
			if (t <= 0.f)
				return;
			NkColor plein = col;
			float32 coeur = t * 0.5f - 0.5f;
			if (t < 1.f) { // plus fin qu'un pixel : coeur nul, l'alpha porte l'epaisseur
				plein.a = static_cast<uint8>(static_cast<float32>(col.a) * t + 0.5f);
				coeur = 0.f;
			}
			const float32 bord = coeur + 1.f;
			NkColor vide = col;
			vide.a = 0;
			const uint32 cp = NkGuiPackColor(plein), cv = NkGuiPackColor(vide);
			const NkVec2 uv{0.f, 0.f};

			// Les points distincts (un doublon n'a pas de direction).
			NkVector<NkVec2> p;
			p.Reserve(static_cast<usize>(n));
			for (int32 i = 0; i < n; ++i) {
				if (p.Empty()) {
					p.PushBack(pts[i]);
					continue;
				}
				const NkVec2 &d = p.Back();
				if ((pts[i].x - d.x) * (pts[i].x - d.x) + (pts[i].y - d.y) * (pts[i].y - d.y) > 1.0e-8f)
					p.PushBack(pts[i]);
			}
			if (closed && p.Size() > 2u) {
				const NkVec2 &f = p[0], &d = p.Back();
				if ((f.x - d.x) * (f.x - d.x) + (f.y - d.y) * (f.y - d.y) <= 1.0e-8f)
					p.PopBack();
			}
			const int32 m = static_cast<int32>(p.Size());
			if (m < 2)
				return;
			if (m < 3)
				closed = false;
			const int32 nbSeg = closed ? m : m - 1;
			// Les normales des segments, puis celles des sommets (onglet borne a 2x).
			NkVector<NkVec2> ns;
			ns.Resize(static_cast<usize>(nbSeg));
			for (int32 i = 0; i < nbSeg; ++i) {
				const NkVec2 &a = p[static_cast<uint32>(i)], &b = p[static_cast<uint32>((i + 1) % m)];
				const float32 dx = b.x - a.x, dy = b.y - a.y;
				const float32 l = std::sqrt(dx * dx + dy * dy);
				ns[static_cast<uint32>(i)] = NkVec2{-dy / l, dx / l};
			}
			NkVector<NkVec2> nv;
			nv.Resize(static_cast<usize>(m));
			for (int32 i = 0; i < m; ++i) {
				if (!closed && (i == 0 || i == m - 1)) {
					nv[static_cast<uint32>(i)] = ns[static_cast<uint32>(i == 0 ? 0 : nbSeg - 1)];
					continue;
				}
				const NkVec2 &na = ns[static_cast<uint32>((i - 1 + nbSeg) % nbSeg)], &nb = ns[static_cast<uint32>(i % nbSeg)];
				NkVec2 dm{(na.x + nb.x) * 0.5f, (na.y + nb.y) * 0.5f};
				const float32 d2 = dm.x * dm.x + dm.y * dm.y;
				if (d2 > 1.0e-6f) {
					float32 k = 1.f / d2;
					if (k > 4.f)
						k = 4.f;
					dm.x *= k;
					dm.y *= k;
				} else {
					dm = nb; // demi-tour : la normale du segment suivant
				}
				nv[static_cast<uint32>(i)] = dm;
			}
			// Les quatre sommets de chaque point.
			const uint32 base = static_cast<uint32>(vtx.Size());
			for (int32 i = 0; i < m; ++i) {
				const NkVec2 &q = p[static_cast<uint32>(i)], &nn = nv[static_cast<uint32>(i)];
				Vtx({q.x + nn.x * bord, q.y + nn.y * bord}, uv, cv);
				Vtx({q.x + nn.x * coeur, q.y + nn.y * coeur}, uv, cp);
				Vtx({q.x - nn.x * coeur, q.y - nn.y * coeur}, uv, cp);
				Vtx({q.x - nn.x * bord, q.y - nn.y * bord}, uv, cv);
			}
			for (int32 i = 0; i < nbSeg; ++i) {
				const uint32 a = base + static_cast<uint32>(i) * 4u, b = base + static_cast<uint32>((i + 1) % m) * 4u;
				Tri(a + 0u, a + 1u, b + 1u, 0u); // frange +
				Tri(a + 0u, b + 1u, b + 0u, 0u);
				Tri(a + 1u, a + 2u, b + 2u, 0u); // coeur
				Tri(a + 1u, b + 2u, b + 1u, 0u);
				Tri(a + 2u, a + 3u, b + 3u, 0u); // frange -
				Tri(a + 2u, b + 3u, b + 2u, 0u);
			}
			// Les BOUTS RONDS : un demi-disque (coeur plein + anneau de frange)
			// d'un cote a l'autre de la normale, en passant par la direction SORTANTE.
			if (boutsRonds && !closed) {
				const float32 PI = 3.14159265358979f;
				int32 k = static_cast<int32>(bord * 2.f) + 4;
				k = k > 16 ? 16 : k;
				for (int32 bout = 0; bout < 2; ++bout) {
					const int32 i = bout == 0 ? 0 : m - 1;
					const NkVec2 q = p[static_cast<uint32>(i)];
					const NkVec2 nn = ns[static_cast<uint32>(bout == 0 ? 0 : nbSeg - 1)];
					// La direction du segment est (nn.y, -nn.x) ; sortante : son
					// oppose au debut, elle-meme a la fin.
					const float32 sg = bout == 0 ? -1.f : 1.f;
					const NkVec2 sort{nn.y * sg, -nn.x * sg};
					const uint32 centre = Vtx(q, uv, cp);
					uint32 pc = 0, po = 0;
					for (int32 j = 0; j <= k; ++j) {
						const float32 an = PI * static_cast<float32>(j) / static_cast<float32>(k);
						const float32 cs = std::cos(an), sn = std::sin(an);
						const NkVec2 v{nn.x * cs + sort.x * sn, nn.y * cs + sort.y * sn};
						const uint32 vc = Vtx({q.x + v.x * coeur, q.y + v.y * coeur}, uv, cp);
						const uint32 vo = Vtx({q.x + v.x * bord, q.y + v.y * bord}, uv, cv);
						if (j > 0) {
							Tri(centre, pc, vc, 0u);
							Tri(pc, po, vo, 0u);
							Tri(pc, vo, vc, 0u);
						}
						pc = vc;
						po = vo;
					}
				}
			}
		}

		NkVec2 NkGuiDrawList::PointBezier(const NkVec2 &p0, const NkVec2 &c0, const NkVec2 &c1, const NkVec2 &p1,
										  float32 t) noexcept {
			const float32 u = 1.f - t;
			const float32 w0 = u * u * u, w1 = 3.f * u * u * t, w2 = 3.f * u * t * t, w3 = t * t * t;
			return NkVec2{w0 * p0.x + w1 * c0.x + w2 * c1.x + w3 * p1.x, w0 * p0.y + w1 * c0.y + w2 * c1.y + w3 * p1.y};
		}

		int32 NkGuiDrawList::SegmentsBezier(const NkVec2 &p0, const NkVec2 &c0, const NkVec2 &c1,
											const NkVec2 &p1) noexcept {
			auto d = [](const NkVec2 &a, const NkVec2 &b) {
				return std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
			};
			const float32 l = d(p0, c0) + d(c0, c1) + d(c1, p1);
			const int32 s = static_cast<int32>(l / 4.f) + 1;
			return s < 8 ? 8 : (s > 256 ? 256 : s);
		}

		void NkGuiDrawList::AddBezierCubic(const NkVec2 &p0, const NkVec2 &c0, const NkVec2 &c1, const NkVec2 &p1,
										   const NkColor &col, float32 thickness, int32 segs, bool boutsRonds) noexcept {
			if (segs <= 0)
				segs = SegmentsBezier(p0, c0, c1, p1);
			if (segs > 256)
				segs = 256;
			NkVec2 pts[257];
			for (int32 i = 0; i <= segs; ++i)
				pts[i] = PointBezier(p0, c0, c1, p1, static_cast<float32>(i) / static_cast<float32>(segs));
			AddPolylineLisse(pts, segs + 1, col, thickness, false, boutsRonds);
		}

		// Le polygone convexe LISSE : le polygone rentre d'un demi-pixel (plein),
		// puis une frange jusqu'au polygone sorti d'un demi-pixel (alpha 0) -- le
		// bord tombe ou il etait, adouci sur un pixel (le remplissage AA d'ImGui).
		void NkGuiDrawList::AddConvexPolyLisse(const NkVec2 *pts, int32 n, const NkColor &col) noexcept {
			if (!pts || n < 3)
				return;
			if (!traitsLisses) {
				AddConvexPolyFilled(pts, n, col); // le chemin d'avant (contre-epreuve)
				return;
			}
			// L'orientation : la normale SORTANTE de (a -> b) est sg * (dy, -dx).
			float32 aire = 0.f;
			for (int32 i = 0; i < n; ++i) {
				const NkVec2 &p = pts[i], &q = pts[(i + 1) % n];
				aire += p.x * q.y - q.x * p.y;
			}
			if (std::fabs(aire) < 1.0e-6f)
				return;
			const float32 sg = aire > 0.f ? 1.f : -1.f;
			NkVec2 ne[64];
			NkVec2 nvx[64];
			if (n > 64)
				n = 64;
			for (int32 i = 0; i < n; ++i) {
				const NkVec2 &p = pts[i], &q = pts[(i + 1) % n];
				const float32 dx = q.x - p.x, dy = q.y - p.y;
				const float32 l = std::sqrt(dx * dx + dy * dy);
				ne[i] = l > 1.0e-6f ? NkVec2{sg * dy / l, -sg * dx / l} : NkVec2{0.f, 0.f};
			}
			for (int32 i = 0; i < n; ++i) {
				const NkVec2 &na = ne[(i - 1 + n) % n], &nb = ne[i];
				NkVec2 dm{(na.x + nb.x) * 0.5f, (na.y + nb.y) * 0.5f};
				const float32 d2 = dm.x * dm.x + dm.y * dm.y;
				if (d2 > 1.0e-6f) {
					float32 k = 1.f / d2;
					if (k > 4.f)
						k = 4.f;
					dm.x *= k;
					dm.y *= k;
				}
				nvx[i] = dm;
			}
			NkColor vide = col;
			vide.a = 0;
			const uint32 cp = NkGuiPackColor(col), cv = NkGuiPackColor(vide);
			const NkVec2 uv{0.f, 0.f};
			const uint32 base = static_cast<uint32>(vtx.Size());
			for (int32 i = 0; i < n; ++i) {
				Vtx({pts[i].x - nvx[i].x * 0.5f, pts[i].y - nvx[i].y * 0.5f}, uv, cp); // dedans
				Vtx({pts[i].x + nvx[i].x * 0.5f, pts[i].y + nvx[i].y * 0.5f}, uv, cv); // dehors
			}
			for (int32 i = 2; i < n; ++i)
				Tri(base, base + static_cast<uint32>(i - 1) * 2u, base + static_cast<uint32>(i) * 2u, 0u);
			for (int32 i = 0; i < n; ++i) {
				const uint32 a = base + static_cast<uint32>(i) * 2u, b = base + static_cast<uint32>((i + 1) % n) * 2u;
				Tri(a, a + 1u, b + 1u, 0u);
				Tri(a, b + 1u, b, 0u);
			}
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
			// LA PORTE A ANGLE NUL : la boucle de glyphes vit dans `AddTextTourne`, une
			// seule fois. Une seconde boucle aurait diverge a la premiere retouche.
			AddTextTourne(face, texId, baseline, text, col, 0.f, baseline, maxWidth, skew, textEnd);
		}

		void NkGuiDrawList::AddTextTourne(const NkFont *face, uint32 texId, const NkVec2 &baseline,
										  const char *text, const NkColor &col, float32 angleDeg,
										  const NkVec2 &pivot, float32 maxWidth, float32 skew,
										  const char *textEnd) noexcept {
			// LA PORTE TOURNEE : une rotation autour d'un pivot est une affine.
			if (angleDeg == 0.f) {
				AddTextTransforme(face, texId, baseline, text, col, 1.f, 0.f, 0.f, 1.f, 0.f, 0.f,
								  maxWidth, skew, textEnd);
				return;
			}
			const float32 rad = angleDeg * 0.017453292519943295f;
			const float32 sn = std::sin(rad), cs = std::cos(rad);
			// p' = P + R (p - P)  ->  e = Px - (a Px + c Py), f = Py - (b Px + d Py)
			const float32 ma = cs, mb = sn, mc = -sn, md = cs;
			const float32 me = pivot.x - (ma * pivot.x + mc * pivot.y);
			const float32 mf = pivot.y - (mb * pivot.x + md * pivot.y);
			AddTextTransforme(face, texId, baseline, text, col, ma, mb, mc, md, me, mf, maxWidth,
							  skew, textEnd);
		}

		void NkGuiDrawList::AddTextTransforme(const NkFont *face, uint32 texId, const NkVec2 &baseline,
											  const char *text, const NkColor &col, float32 ta, float32 tb,
											  float32 tc, float32 td, float32 te, float32 tf, float32 maxWidth,
											  float32 skew, const char *textEnd) noexcept {
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
			// sin/cos UNE fois, et seulement s'il y a un angle : la porte `AddText`
			// ne paie aucune trigonometrie. Le calage au pixel s'est fait AVANT, sur
			// la ligne droite -- un texte tourne ne se cale pas au pixel.
			// L'AFFINE, appliquee aux quatre sommets de chaque quad. A l'identite,
			// `tourne` rend son entree telle quelle : les appelants d'avant ne paient rien.
			const bool tourneVraiment =
				!(ta == 1.f && tb == 0.f && tc == 0.f && td == 1.f && te == 0.f && tf == 0.f);
			auto tourne = [&](NkVec2 v) -> NkVec2 {
				if (!tourneVraiment)
					return v;
				return NkVec2{ta * v.x + tc * v.y + te, tb * v.x + td * v.y + tf};
			};

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
					// LES QUATRE SOMMETS DU QUAD, TOURNES AUTOUR DU PIVOT -- comme un
					// contour. A angle nul, `tourne` rend son entree telle quelle.
					const uint32 i0 = Vtx(tourne({x0 + sTop, y0}), {g->u0, g->v0}, c);
					const uint32 i1 = Vtx(tourne({x1 + sTop, y0}), {g->u1, g->v0}, c);
					const uint32 i2 = Vtx(tourne({x1 + sBot, y1}), {g->u1, g->v1}, c);
					const uint32 i3 = Vtx(tourne({x0 + sBot, y1}), {g->u0, g->v1}, c);
					Tri(i0, i1, i2, texId);
					Tri(i0, i2, i3, texId);
				}
				x += g->advanceX;
			}
		}

		void NkGuiDrawList::AddTextScaled(const NkFont *face, uint32 texId, const NkVec2 &baseline,
										  const char *text, const NkColor &col, float32 scale,
										  float32 maxWidth) noexcept {
			// Texte à l'ÉCHELLE (2026-08-31, « le texte doit suivre le zoom ») :
			// la GÉOMÉTRIE du glyphe scalé vivait déjà dans la couche du dessous
			// (`NkFontScaleRenderer::GetScaledGlyphQuad`, NKFont) — ici on ne fait
			// que l'émettre en quads. Les UV ne bougent pas : c'est l'atlas
			// existant, agrandi/réduit par le filtre GPU. Net en réduction, doux
			// en agrandissement — le SDF (NkFontSdf, futur) est nommé dans NKFont.
			if (scale <= 0.f)
				return;
			// À l'échelle ~1, le chemin historique reste le bon : il a le
			// pixel-snap qui évite le flou. On ne duplique pas sa qualité ici.
			if (scale > 0.999f && scale < 1.001f) {
				AddText(face, texId, baseline, text, col, maxWidth);
				return;
			}
			if (!face || !text || !*text || texId == 0u)
				return;
			const uint32 c = NkGuiPackColor(col);
			const char *p = text;
			const char *end = text;
			while (*end)
				++end;
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
					// ⚠️ PAS de pixel-snap ici : arrondir des positions scalées
					// ferait « respirer » l'interlettrage à chaque cran de zoom.
					float32 x0 = 0.f, y0 = 0.f, x1 = 0.f, y1 = 0.f;
					NkFontScaleRenderer::GetScaledGlyphQuad(g, x, y, scale, &x0, &y0, &x1, &y1,
															nullptr);
					if (x1 > xEnd)
						break; // troncature simple, comme AddText
					const uint32 i0 = Vtx({x0, y0}, {g->u0, g->v0}, c);
					const uint32 i1 = Vtx({x1, y0}, {g->u1, g->v0}, c);
					const uint32 i2 = Vtx({x1, y1}, {g->u1, g->v1}, c);
					const uint32 i3 = Vtx({x0, y1}, {g->u0, g->v1}, c);
					Tri(i0, i1, i2, texId);
					Tri(i0, i2, i3, texId);
				}
				// L'avance se scale AUSSI pour les glyphes invisibles (espaces).
				x += g->advanceX * scale;
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

		void NkGuiDrawList::AddEllipseFilled(const NkVec2 &center, float32 rx, float32 ry, const NkColor &col,
											  int32 segs) noexcept {
			if (rx <= 0.f || ry <= 0.f)
				return;
			if (segs <= 0)
				segs = NkGuiCercleSegs(rx > ry ? rx : ry);
			const uint32 cc = NkGuiPackColor(col);
			const NkVec2 uv{0.f, 0.f};
			const uint32 ic = Vtx(center, uv, cc);
			const float32 kTau = 6.28318530718f;
			uint32 prev = Vtx({center.x + rx, center.y}, uv, cc);
			for (int32 s = 1; s <= segs; ++s) {
				const float32 ang = kTau * static_cast<float32>(s) / static_cast<float32>(segs);
				const uint32 cur =
					Vtx({center.x + std::cos(ang) * rx, center.y + std::sin(ang) * ry}, uv, cc);
				Tri(ic, prev, cur, 0u);
				prev = cur;
			}
		}

		void NkGuiDrawList::AddCircleFilled(const NkVec2 &center, float32 r, const NkColor &col, int32 segs) noexcept {
			if (r <= 0.f)
				return;
			if (segs <= 0)
				segs = NkGuiCercleSegs(r);
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
