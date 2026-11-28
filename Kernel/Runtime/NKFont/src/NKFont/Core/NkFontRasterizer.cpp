// -----------------------------------------------------------------------------
// FICHIER: NKFont/Core/NkFontRasterizer.cpp
// DESCRIPTION: Rastériseur scanline pour contours de glyphes.
//              Implémente un algorithme de remplissage par scanline avec deux
//              buffers flottants (contribution locale et déversement "fill").
//              Aucune dépendance externe — utilise les types nkft_.
//
// ─────────────────────────────────────────────────────────────────────────────
// ALGORITHME DE RASTÉRISATION PAR SCANLINE — VERSION À DEUX BUFFERS
// ─────────────────────────────────────────────────────────────────────────────
//
// Le rastériseur utilise DEUX buffers flottants par scanline :
//
//   scanline[W]   : contribution "locale" de l'arête dans chaque pixel
//                   (aire du trapèze dans ce pixel, signée)
//
//   scanline2[W+1]: contribution "fill" = déversement vers la droite
//                   (hauteur de l'arête au-dessus de ce pixel)
//
// Intégration finale :
//   float sum = 0;
//   for (i = 0; i < W; ++i) {
//       sum += scanline2[i];          // accumule le déversement "fill"
//       k = scanline[i] + sum;        // local + tout ce qui est à gauche
//       k = fabs(k) * 255 + 0.5f;
//       m = (int) k;
//       if (m > 255) m = 255;
//       result[i] = (unsigned char) m;
//   }
//
// L'arête active stocke :
//   fx  = position X au DÉBUT (top) de la scanline courante (à y+0.0)
//   fdx = delta X par scanline complète
//   fdy = 1/fdx si fdx != 0, sinon 0
//   sy  = y0 de l'arête (start)
//   ey  = y1 de l'arête (end)
//
// Avance en fin de scanline :
//   z->fx += z->fdx;   (fx passe de y → y+1, prêt pour la prochaine scanline)
//
// Contribution d'une arête à une scanline :
//   La fonction NkFillActiveEdgesNew() calcule exactement quels pixels
//   sont touchés par le trapèze formé par l'arête entre y_top et y_bottom.
//   Elle écrit dans scanline[x] la contribution "locale" et dans
//   scanline2[x+1] le déversement "fill" (tout ce qui est à droite).
//
// PROPRIÉTÉ CLÉ :
//   sum(scanline[0..W-1] + scanline_fill déversement) = 0 pour un contour fermé.
//   Cela garantit que l'accumulateur revient à 0 → aucun artefact dans les zones vides.
// -----------------------------------------------------------------------------

#include "NkFontParser.h"
#include <math.h>
#include <string.h>

namespace nkentseu {
	namespace nkfont {

		// ============================================================
		// CONSTANTES
		// ============================================================

		static constexpr nkft_int32 NK_RE_MAX_EDGES = 8192;
		static constexpr nkft_int32 NK_RE_MAX_ACTIVE = 1024;
		static constexpr nkft_int32 NK_RE_SCAN_W = 4096; // largeur maximale de bitmap supportée
		// scanline2 a besoin de W+1 slots (déversement possible sur le slot W)
		static constexpr nkft_int32 NK_RE_SCAN2_W = NK_RE_SCAN_W + 1;

		// ============================================================
		// STRUCTURES (représentation des arêtes)
		// ============================================================

		// Arête brute (avant activation)
		struct NkREdge {
				nkft_float32 x0, y0, x1, y1;
				nkft_int32 invert; // 1 si la direction est vers le haut dans l'espace original
		};

		// Arête active pendant le parcours des scanlines
		struct NkRActive {
				struct NkRActive *next;
				nkft_float32 fx;		// position X au TOP de la scanline courante (à y+0.0)
				nkft_float32 fdx;		// delta X par scanline entière
				nkft_float32 fdy;		// 1/fdx si fdx != 0, sinon 0
				nkft_float32 direction; // +1.0 ou -1.0 selon le sens de l'arête
				nkft_float32 sy;		// y0 de l'arête (start)
				nkft_float32 ey;		// y1 de l'arête (end)
		};

		// ============================================================
		// ÉTAT GLOBAL (buffers réutilisables)
		// ============================================================

		static NkREdge sREdges[NK_RE_MAX_EDGES];
		static nkft_int32 sRNum = 0;

		// (27/11/2026, scripts-23) Le calage vertical du glyphe en cours (NkMakeGlyphBitmapCale) ; nul : aucun, le
		// contour d'avant au bit pres (NkBuildEdges garde alors son expression d'origine).
		static const NkCalageVertical *sRCalage = nullptr;

		// (27/11/2026, scripts-23) LES HAMPES d'un glyphe (NkCalageVertical::hampes) : des noeuds en x, en unites (x de
		// la face -> x cale, tombant sur une colonne entiere) ; calcules sur la forme du glyphe (NkPreparerHampes).
		struct NkCalageHampes {
				static constexpr nkft_int32 NK_HAMPES_MAX = 48;
				nkft_int32 n = 0;
				nkft_float32 src[NK_HAMPES_MAX] = {};
				nkft_float32 dst[NK_HAMPES_MAX] = {};
		};
		static const NkCalageHampes *sRHampes = nullptr;
		// Les barres du glyphe en cours : son calage vertical PROPRE (les zones et ses barres) ; nul : sRCalage.
		static const NkCalageHampes *sRBarres = nullptr;

		/// Entre deux noeuds : lineaire ; au-dela des bouts : la pente 1 (decale) ; sans noeud : v.
		static nkft_float32 NkInterpNoeuds(nkft_int32 n, const nkft_float32 *src, const nkft_float32 *dst, nkft_float32 v) {
			if (n <= 0) {
				return v;
			}
			if (v <= src[0]) {
				return dst[0] + (v - src[0]);
			}
			if (v >= src[n - 1]) {
				return dst[n - 1] + (v - src[n - 1]);
			}
			nkft_int32 i = 0;
			while (i + 1 < n && src[i + 1] <= v) {
				++i;
			}
			const nkft_float32 t = (v - src[i]) / (src[i + 1] - src[i]);
			return dst[i] + t * (dst[i + 1] - dst[i]);
		}

		static nkft_float32 NkCalerX(const NkCalageHampes &h, nkft_float32 x) {
			return NkInterpNoeuds(h.n, h.src, h.dst, x);
		}

		/// Le y cale du glyphe en cours (sRCalage non nul) : ses barres s'il en a, sinon le calage de la face.
		static nkft_float32 NkYCale(nkft_float32 y) {
			return sRBarres ? NkInterpNoeuds(sRBarres->n, sRBarres->src, sRBarres->dst, y)
							: NkInterpNoeuds(sRCalage->n, sRCalage->src, sRCalage->dst, y);
		}

		// Pool d'arêtes actives (évite les allocations dynamiques)
		static NkRActive sRPool[NK_RE_MAX_ACTIVE];
		static nkft_int32 sRPoolTop = 0;

		// Deux buffers de scanline
		static nkft_float32 sScanline[NK_RE_SCAN_W];   // contribution locale
		static nkft_float32 sScanline2[NK_RE_SCAN2_W]; // déversement "fill"

		// ============================================================
		// GESTION DU POOL D'ARÊTES ACTIVES
		// ============================================================

		static NkRActive *NkPoolAlloc() {
			if (sRPoolTop >= NK_RE_MAX_ACTIVE) {
				return nullptr;
			}
			return &sRPool[sRPoolTop++];
		}

		static void NkPoolReset() {
			sRPoolTop = 0;
		}

		// ============================================================
		// AJOUT D'UNE ARÊTE (segment de ligne)
		// ============================================================

		static void NkPushEdge(nkft_float32 x0, nkft_float32 y0, nkft_float32 x1, nkft_float32 y1, nkft_int32 invert) {
			// Ignorer les arêtes de hauteur nulle
			if (y0 == y1) {
				return;
			}
			if (sRNum >= NK_RE_MAX_EDGES) {
				return;
			}
			NkREdge &e = sREdges[sRNum++];

			// Normaliser pour que y0 < y1 (top en premier)
			if (y0 < y1) {
				e.x0 = x0;
				e.y0 = y0;
				e.x1 = x1;
				e.y1 = y1;
				e.invert = invert;
			} else {
				e.x0 = x1;
				e.y0 = y1;
				e.x1 = x0;
				e.y1 = y0;
				e.invert = !invert;
			}
		}

		// ============================================================
		// APLATISSEMENT DES COURBES QUADRATIQUES (Bézier)
		// ============================================================

		static void NkFlattenQuad(nkft_float32 x0, nkft_float32 y0, nkft_float32 x1, nkft_float32 y1, nkft_float32 x2,
								  nkft_float32 y2, nkft_float32 objspace_flatness_sq, nkft_int32 n) {
			// Calcul du milieu de la courbe vs la corde
			nkft_float32 mx = (x0 + 2 * x1 + x2) / 4;
			nkft_float32 my = (y0 + 2 * y1 + y2) / 4;
			nkft_float32 dx = (x0 + x2) / 2 - mx;
			nkft_float32 dy = (y0 + y2) / 2 - my;

			// Sécurité : ne pas subdiviser trop profondément
			if (n > 16) {
				NkPushEdge(x0, y0, x2, y2, 1);
				return;
			}

			// Si l'écart dépasse la tolérance, subdiviser
			if (dx * dx + dy * dy > objspace_flatness_sq) {
				nkft_float32 m01x = (x0 + x1) * 0.5f;
				nkft_float32 m01y = (y0 + y1) * 0.5f;
				nkft_float32 m12x = (x1 + x2) * 0.5f;
				nkft_float32 m12y = (y1 + y2) * 0.5f;
				nkft_float32 mmx = (m01x + m12x) * 0.5f;
				nkft_float32 mmy = (m01y + m12y) * 0.5f;

				NkFlattenQuad(x0, y0, m01x, m01y, mmx, mmy, objspace_flatness_sq, n + 1);
				NkFlattenQuad(mmx, mmy, m12x, m12y, x2, y2, objspace_flatness_sq, n + 1);
			} else {
				NkPushEdge(x0, y0, x2, y2, 1);
			}
		}

		// ============================================================
		// APLATISSEMENT DES COURBES CUBIQUES (Bézier)
		// ============================================================

		static void NkFlattenCubic(nkft_float32 x0, nkft_float32 y0, nkft_float32 x1, nkft_float32 y1, nkft_float32 x2,
								   nkft_float32 y2, nkft_float32 x3, nkft_float32 y3, nkft_float32 objspace_flatness_sq,
								   nkft_int32 n) {
			nkft_float32 dx0 = x1 - x0;
			nkft_float32 dy0 = y1 - y0;
			nkft_float32 dx1 = x2 - x1;
			nkft_float32 dy1 = y2 - y1;
			nkft_float32 dx2 = x3 - x2;
			nkft_float32 dy2 = y3 - y2;
			nkft_float32 dx = x3 - x0;
			nkft_float32 dy = y3 - y0;

			nkft_float32 ll = (nkft_float32)(sqrtf(dx0 * dx0 + dy0 * dy0) + sqrtf(dx1 * dx1 + dy1 * dy1) +
											 sqrtf(dx2 * dx2 + dy2 * dy2));
			nkft_float32 sl = sqrtf(dx * dx + dy * dy);
			nkft_float32 flat_sq = ll * ll - sl * sl;

			if (n > 16) {
				NkPushEdge(x0, y0, x3, y3, 1);
				return;
			}

			if (flat_sq > objspace_flatness_sq) {
				nkft_float32 x01 = (x0 + x1) * 0.5f;
				nkft_float32 y01 = (y0 + y1) * 0.5f;
				nkft_float32 x12 = (x1 + x2) * 0.5f;
				nkft_float32 y12 = (y1 + y2) * 0.5f;
				nkft_float32 x23 = (x2 + x3) * 0.5f;
				nkft_float32 y23 = (y2 + y3) * 0.5f;
				nkft_float32 xa = (x01 + x12) * 0.5f;
				nkft_float32 ya = (y01 + y12) * 0.5f;
				nkft_float32 xb = (x12 + x23) * 0.5f;
				nkft_float32 yb = (y12 + y23) * 0.5f;
				nkft_float32 mx = (xa + xb) * 0.5f;
				nkft_float32 my = (ya + yb) * 0.5f;

				NkFlattenCubic(x0, y0, x01, y01, xa, ya, mx, my, objspace_flatness_sq, n + 1);
				NkFlattenCubic(mx, my, xb, yb, x23, y23, x3, y3, objspace_flatness_sq, n + 1);
			} else {
				NkPushEdge(x0, y0, x3, y3, 1);
			}
		}

		// ============================================================
		// CONSTRUCTION DES ARÊTES À PARTIR D'UN VERTEX BUFFER
		// ============================================================

		static void NkBuildEdges(const NkFontVertexBuffer *vb, nkft_float32 sx, nkft_float32 sy, nkft_float32 ox,
								 nkft_float32 oy) {
			sRNum = 0;
			nkft_float32 px = 0.f;
			nkft_float32 py = 0.f;

			// Tolérance d'aplatissement en pixels (valeur empirique)
			static constexpr nkft_float32 flatness = 0.35f;
			const nkft_float32 flat_sq = flatness * flatness;

			for (nkft_uint32 i = 0; i < vb->count; ++i) {
				const NkFontVertex &v = vb->verts[i];

				// Application de l'échelle et du flip Y (TrueType Y-up → écran Y-down)
				// (scripts-23) le x et le y cales, en flottant (sans calage : l'expression d'origine).
				nkft_float32 vx = sRHampes ? NkCalerX(*sRHampes, (nkft_float32)v.x) * sx + ox : v.x * sx + ox;
				nkft_float32 vy = sRCalage ? -NkYCale((nkft_float32)v.y) * sy + oy : -v.y * sy + oy;

				switch (v.type) {
					case NK_FONT_VERTEX_MOVE:
						px = vx;
						py = vy;
						break;

					case NK_FONT_VERTEX_LINE:
						if (py != vy) {
							NkPushEdge(px, py, vx, vy, 1);
						}
						px = vx;
						py = vy;
						break;

					case NK_FONT_VERTEX_CURVE: {
						nkft_float32 cx = sRHampes ? NkCalerX(*sRHampes, (nkft_float32)v.cx) * sx + ox : v.cx * sx + ox;
						nkft_float32 cy = sRCalage ? -NkYCale((nkft_float32)v.cy) * sy + oy : -v.cy * sy + oy;
						NkFlattenQuad(px, py, cx, cy, vx, vy, flat_sq, 0);
						px = vx;
						py = vy;
						break;
					}

					case NK_FONT_VERTEX_CUBIC: {
						nkft_float32 c0x = sRHampes ? NkCalerX(*sRHampes, (nkft_float32)v.cx) * sx + ox : v.cx * sx + ox;
						nkft_float32 c0y = sRCalage ? -NkYCale((nkft_float32)v.cy) * sy + oy : -v.cy * sy + oy;
						nkft_float32 c1x = sRHampes ? NkCalerX(*sRHampes, (nkft_float32)v.cx1) * sx + ox : v.cx1 * sx + ox;
						nkft_float32 c1y = sRCalage ? -NkYCale((nkft_float32)v.cy1) * sy + oy : -v.cy1 * sy + oy;
						NkFlattenCubic(px, py, c0x, c0y, c1x, c1y, vx, vy, flat_sq, 0);
						px = vx;
						py = vy;
						break;
					}
				}
			}
		}

		// ============================================================
		// TRI DES ARÊTES PAR y0 CROISSANT (INSERTION SORT)
		// ============================================================

		static void NkSortEdges() {
			for (nkft_int32 i = 1; i < sRNum; ++i) {
				NkREdge t = sREdges[i];
				nkft_int32 j = i - 1;
				while (j >= 0 && sREdges[j].y0 > t.y0) {
					sREdges[j + 1] = sREdges[j];
					--j;
				}
				sREdges[j + 1] = t;
			}
		}

		// ============================================================
		// TRAITEMENT D'UN SEGMENT D'ARÊTE CLIPPÉ À UN PIXEL
		// ============================================================
		// Calcule la contribution d'un segment d'arête qui reste dans le pixel [x, x+1].
		// Écrit dans scanline[x] (contribution locale).
		// ============================================================

		static void NkHandleClippedEdge(nkft_float32 *scanline, nkft_int32 x, NkRActive *e, nkft_float32 x0,
										nkft_float32 y0, nkft_float32 x1, nkft_float32 y1) {
			if (y0 == y1) {
				return;
			}

			// Clip vertical aux bornes sy..ey de l'arête
			if (y0 > e->ey) {
				return;
			}
			if (y1 < e->sy) {
				return;
			}
			if (y0 < e->sy) {
				x0 += (x1 - x0) * (e->sy - y0) / (y1 - y0);
				y0 = e->sy;
			}
			if (y1 > e->ey) {
				x1 += (x1 - x0) * (e->ey - y1) / (y1 - y0);
				y1 = e->ey;
			}

			// Contribution au pixel x : coverage = direction * (y1 - y0) * (1 - avg_x_in_pixel)
			if (x0 <= x && x1 <= x) {
				scanline[x] += e->direction * (y1 - y0);
			} else if (x0 >= x + 1 && x1 >= x + 1) {
				// entièrement à droite du pixel → aucune contribution locale
			} else {
				// Partiellement dans le pixel
				scanline[x] += e->direction * (y1 - y0) * (1.f - ((x0 - x) + (x1 - x)) / 2.f);
			}
		}

		// ============================================================
		// FONCTIONS DE CALCUL D'AIRES (TRAPÈZES / TRIANGLES)
		// ============================================================

		static inline nkft_float32 NkSizedTrapArea(nkft_float32 height, nkft_float32 top_w, nkft_float32 bot_w) {
			return (top_w + bot_w) / 2.f * height;
		}

		static inline nkft_float32 NkPosTrapArea(nkft_float32 height, nkft_float32 tx0, nkft_float32 tx1,
												 nkft_float32 bx0, nkft_float32 bx1) {
			return NkSizedTrapArea(height, tx1 - tx0, bx1 - bx0);
		}

		static inline nkft_float32 NkSizedTriArea(nkft_float32 height, nkft_float32 width) {
			return height * width / 2.f;
		}

		// ============================================================
		// REMPLISSAGE DES ARÊTES ACTIVES (VERSION À DEUX BUFFERS)
		// ============================================================
		// Pour chaque arête active, calcule sa contribution à la scanline Y.
		// Utilise DEUX buffers :
		//   scanline[x]       += contribution locale (aire dans le pixel x)
		//   scanline_fill[x]  += hauteur de l'arête au-dessus (déversement vers droite)
		// ============================================================

		static void NkFillActiveEdgesNew(nkft_float32 *scanline, nkft_float32 *scanline_fill, nkft_int32 len,
										 NkRActive *e, nkft_float32 y_top) {
			nkft_float32 y_bottom = y_top + 1.f;

			while (e) {
				if (e->fdx == 0.f) {
					// Arête verticale parfaite
					nkft_float32 x0 = e->fx;
					if (x0 < len) {
						if (x0 >= 0.f) {
							NkHandleClippedEdge(scanline, (nkft_int32)x0, e, x0, y_top, x0, y_bottom);
							NkHandleClippedEdge(scanline_fill - 1, (nkft_int32)x0 + 1, e, x0, y_top, x0, y_bottom);
						} else {
							NkHandleClippedEdge(scanline_fill - 1, 0, e, x0, y_top, x0, y_bottom);
						}
					}
				} else {
					// Arête oblique
					nkft_float32 x0 = e->fx;
					nkft_float32 dx = e->fdx;
					nkft_float32 xb = x0 + dx;
					nkft_float32 x_top;
					nkft_float32 x_bottom;
					nkft_float32 sy0;
					nkft_float32 sy1;
					nkft_float32 dy = e->fdy;

					if (e->sy > y_top) {
						x_top = x0 + dx * (e->sy - y_top);
						sy0 = e->sy;
					} else {
						x_top = x0;
						sy0 = y_top;
					}
					if (e->ey < y_bottom) {
						x_bottom = x0 + dx * (e->ey - y_top);
						sy1 = e->ey;
					} else {
						x_bottom = xb;
						sy1 = y_bottom;
					}

					if (x_top >= 0.f && x_bottom >= 0.f && x_top < len && x_bottom < len) {
						// Cas rapide : l'arête est entièrement dans le bitmap
						if ((nkft_int32)x_top == (nkft_int32)x_bottom) {
							// Traverse un seul pixel en X
							nkft_float32 height;
							nkft_int32 x = (nkft_int32)x_top;
							height = (sy1 - sy0) * e->direction;
							scanline[x] += NkPosTrapArea(height, x_top, x + 1.f, x_bottom, x + 1.f);
							scanline_fill[x] += height;
						} else {
							// Traverse plusieurs pixels en X
							nkft_int32 x;
							nkft_int32 x1;
							nkft_int32 x2;
							nkft_float32 y_crossing;
							nkft_float32 y_final;
							nkft_float32 step;
							nkft_float32 sign;
							nkft_float32 area;

							if (x_top > x_bottom) {
								// Inverser pour aller de gauche à droite
								nkft_float32 t;
								sy0 = y_bottom - (sy0 - y_top);
								sy1 = y_bottom - (sy1 - y_top);
								t = sy0;
								sy0 = sy1;
								sy1 = t;
								t = x_bottom;
								x_bottom = x_top;
								x_top = t;
								dx = -dx;
								dy = -dy;
								t = x0;
								x0 = xb;
								xb = t;
							}

							x1 = (nkft_int32)x_top;
							x2 = (nkft_int32)x_bottom;

							// Intersection avec la colonne x1+1
							y_crossing = y_top + dy * (x1 + 1 - x0);
							// Intersection avec la colonne x2
							y_final = y_top + dy * (x2 - x0);

							if (y_crossing > y_bottom) {
								y_crossing = y_bottom;
							}

							sign = e->direction;
							area = sign * (y_crossing - sy0);
							scanline[x1] += NkSizedTriArea(area, x1 + 1 - x_top);

							if (y_final > y_bottom) {
								y_final = y_bottom;
								dy = (y_final - y_crossing) / (x2 - (x1 + 1));
							}

							step = sign * dy * 1.f;
							for (x = x1 + 1; x < x2; ++x) {
								scanline[x] += area + step / 2.f;
								area += step;
							}

							scanline[x2] += area + sign * NkPosTrapArea(sy1 - y_final, (nkft_float32)x2, x2 + 1.f,
																		x_bottom, x2 + 1.f);
							scanline_fill[x2] += sign * (sy1 - sy0);
						}
					} else {
						// Chement lent : clipping par force brute (arête sort du bitmap)
						for (nkft_int32 x = 0; x < len; ++x) {
							nkft_float32 y0_ = y_top;
							nkft_float32 x1_ = (nkft_float32)x;
							nkft_float32 x2_ = (nkft_float32)(x + 1);
							nkft_float32 x3_ = xb;
							nkft_float32 y3_ = y_bottom;
							nkft_float32 y1_ = (x - x0) / dx + y_top;
							nkft_float32 y2_ = (x + 1 - x0) / dx + y_top;

							if (x0 < x1_ && x3_ > x2_) {
								NkHandleClippedEdge(scanline, x, e, x0, y0_, x1_, y1_);
								NkHandleClippedEdge(scanline, x, e, x1_, y1_, x2_, y2_);
								NkHandleClippedEdge(scanline, x, e, x2_, y2_, x3_, y3_);
							} else if (x3_ < x1_ && x0 > x2_) {
								NkHandleClippedEdge(scanline, x, e, x0, y0_, x2_, y2_);
								NkHandleClippedEdge(scanline, x, e, x2_, y2_, x1_, y1_);
								NkHandleClippedEdge(scanline, x, e, x1_, y1_, x3_, y3_);
							} else if (x0 < x1_ && x3_ > x1_) {
								NkHandleClippedEdge(scanline, x, e, x0, y0_, x1_, y1_);
								NkHandleClippedEdge(scanline, x, e, x1_, y1_, x3_, y3_);
							} else if (x3_ < x1_ && x0 > x1_) {
								NkHandleClippedEdge(scanline, x, e, x0, y0_, x1_, y1_);
								NkHandleClippedEdge(scanline, x, e, x1_, y1_, x3_, y3_);
							} else if (x0 < x2_ && x3_ > x2_) {
								NkHandleClippedEdge(scanline, x, e, x0, y0_, x2_, y2_);
								NkHandleClippedEdge(scanline, x, e, x2_, y2_, x3_, y3_);
							} else if (x3_ < x2_ && x0 > x2_) {
								NkHandleClippedEdge(scanline, x, e, x0, y0_, x2_, y2_);
								NkHandleClippedEdge(scanline, x, e, x2_, y2_, x3_, y3_);
							} else {
								NkHandleClippedEdge(scanline, x, e, x0, y0_, x3_, y3_);
							}
						}
					}
				}
				e = e->next;
			}
		}

		// ============================================================
		// RASTÉRISATION PRINCIPALE (PARCOURS DES SCANLINES)
		// ============================================================

		static void NkDoRasterize(nkft_uint8 *out, nkft_int32 W, nkft_int32 H, nkft_int32 stride, nkft_int32 off_x,
								  nkft_int32 off_y) {
			NkPoolReset();

			NkRActive *active = nullptr;
			nkft_int32 j = 0;  // ligne de sortie courante
			nkft_int32 ei = 0; // index dans sREdges[]

			// Sentinelle : la dernière arête du tableau
			sREdges[sRNum].y0 = (nkft_float32)(off_y + H) + 1.f;

			while (j < H) {
				nkft_float32 scan_y_top = (nkft_float32)(off_y + j) + 0.0f;
				nkft_float32 scan_y_bottom = (nkft_float32)(off_y + j) + 1.0f;

				// Nettoyage des deux buffers de scanline
				memset(sScanline, 0, (nkft_size)W * sizeof(nkft_float32));
				memset(sScanline2, 0, (nkft_size)(W + 1) * sizeof(nkft_float32));

				// Suppression des arêtes terminées avant le top de cette scanline
				{
					NkRActive **step = &active;
					while (*step) {
						NkRActive *z = *step;
						if (z->ey <= scan_y_top) {
							*step = z->next;
							z->direction = 0; // marque comme libre
						} else {
							step = &((*step)->next);
						}
					}
				}

				// Activation des arêtes qui commencent avant le bas de la scanline
				while (sREdges[ei].y0 <= scan_y_bottom) {
					if (sREdges[ei].y0 != sREdges[ei].y1) {
						NkRActive *z = NkPoolAlloc();
						if (z) {
							nkft_float32 dxdy = (sREdges[ei].x1 - sREdges[ei].x0) / (sREdges[ei].y1 - sREdges[ei].y0);
							z->fdx = dxdy;
							z->fdy = (dxdy != 0.f) ? (1.f / dxdy) : 0.f;
							// fx = position X au TOP de la scanline (scan_y_top)
							z->fx = sREdges[ei].x0 + dxdy * (scan_y_top - sREdges[ei].y0);
							z->fx -= (nkft_float32)off_x; // décalage bitmap
							z->direction = sREdges[ei].invert ? 1.f : -1.f;
							z->sy = sREdges[ei].y0;
							z->ey = sREdges[ei].y1;

							// Sécurité flottante : garantir ey >= scan_y_top
							if (j == 0 && off_y != 0 && z->ey < scan_y_top) {
								z->ey = scan_y_top;
							}
							z->next = active;
							active = z;
						}
					}
					++ei;
				}

				// Calcul des contributions de toutes les arêtes actives
				if (active) {
					NkFillActiveEdgesNew(sScanline, sScanline2 + 1, W, active, scan_y_top);
				}

				// Intégration finale pour produire les pixels de la scanline
				{
					nkft_float32 sum = 0.f;
					nkft_uint8 *row = out + j * stride;
					for (nkft_int32 i = 0; i < W; ++i) {
						nkft_float32 k;
						nkft_int32 m;
						sum += sScanline2[i];	// accumule le déversement "fill"
						k = sScanline[i] + sum; // contribution locale + tout ce qui est à gauche
						k = (k < 0.f ? -k : k) * 255.f + 0.5f;
						m = (nkft_int32)k;
						if (m > 255) {
							m = 255;
						}
						row[i] = (nkft_uint8)m;
					}
				}

				// Avance toutes les arêtes actives d'une scanline (fx passe de y → y+1)
				{
					NkRActive **step = &active;
					while (*step) {
						NkRActive *z = *step;
						z->fx += z->fdx;
						step = &((*step)->next);
					}
				}

				++j;
			}
		}

		// ============================================================
		// API PUBLIQUE : GÉNÉRATION D'UN BITMAP DE GLYPHE
		// ============================================================

		void NkMakeGlyphBitmapExact(const NkFontFaceInfo *info, nkft_uint8 *output, nkft_int32 outW, nkft_int32 outH,
									nkft_int32 outStride, nkft_float32 scaleX, nkft_float32 scaleY, nkft_float32 shiftX,
									nkft_float32 shiftY, NkGlyphId glyph) {
			if (!info || !output || outW <= 0 || outH <= 0) {
				return;
			}

			// Effacement du bitmap de sortie
			for (nkft_int32 r = 0; r < outH; ++r) {
				memset(output + r * outStride, 0, (nkft_size)outW);
			}

			// Récupération des vertices du contour du glyphe
			NkFontVertexBuffer vb;
			if (!NkGetGlyphShape(info, glyph, &vb) || vb.count == 0) {
				return;
			}

			// Calcul de la bounding box en pixels (pour connaître l'offset)
			nkft_int32 bx0, by0, bx1, by1;
			NkGetGlyphBitmapBox(info, glyph, scaleX, scaleY, shiftX, shiftY, &bx0, &by0, &bx1, &by1);

			// Construction des arêtes avec l'offset pour que le glyphe
			// commence à (0,0) dans le bitmap de sortie
			NkBuildEdges(&vb, scaleX, scaleY, shiftX - (nkft_float32)bx0, shiftY - (nkft_float32)by0);

			if (sRNum == 0) {
				return;
			}

			// Tri des arêtes par y0 croissant
			NkSortEdges();

			// Ajout d'une sentinelle à la fin (NkDoRasterize l'utilise)
			if (sRNum < NK_RE_MAX_EDGES) {
				sREdges[sRNum].y0 = 1e10f;
				sREdges[sRNum].y1 = 1e10f;
			}

			// Rastérisation finale
			NkDoRasterize(output, outW, outH, outStride, 0, 0);
		}

		// Alias de la fonction principale
		void NkMakeGlyphBitmap(const NkFontFaceInfo *info, nkft_uint8 *output, nkft_int32 outW, nkft_int32 outH,
							   nkft_int32 outStride, nkft_float32 scaleX, nkft_float32 scaleY, nkft_float32 shiftX,
							   nkft_float32 shiftY, NkGlyphId glyph) {
			NkMakeGlyphBitmapExact(info, output, outW, outH, outStride, scaleX, scaleY, shiftX, shiftY, glyph);
		}

		// ============================================================
		// (27/11/2026, scripts-23) LE CALAGE VERTICAL
		// ============================================================
		// Ce que fait le hinting de Windows, sur l'axe vertical seulement : les hauteurs qui portent l'oeil (la ligne
		// de base, la hauteur d'x, les capitales, les montantes, les descendantes) tombent sur des rangees ENTIERES.
		// Sans lui, le haut d'un « n » a 13 px tombe a 6,8 px de la ligne de base : une rangee a 80 % de gris au lieu
		// d'une rangee pleine, sur toutes les minuscules ; c'est le flou que le banc de netteté mesurait (juge2).
		// Les depassements des rondes (« o » monte un peu au-dessus de « x ») s'arrondissent A PART : nuls aux
		// petites tailles, le haut d'un « o » se cale alors sur celui d'un « x ».

		bool NkPreparerCalageVertical(const NkFontFaceInfo *info, nkft_float32 sy, NkCalageVertical *out) {
			if (!info || !out || !(sy > 0.f)) {
				return false;
			}
			out->n = 0;
			out->hauteurX = 0.f;
			// La borne haute (ou basse) de la lettre `cp`, en unites ; false : la face ne l'a pas.
			const auto borne = [info](NkFontCodepoint cp, bool haut, nkft_float32 *v) -> bool {
				const NkGlyphId g = NkFindGlyphIndex(info, cp);
				nkft_int32 x0 = 0, y0 = 0, x1 = 0, y1 = 0;
				if (g == 0 || !NkGetGlyphBox(info, g, &x0, &y0, &x1, &y1)) {
					return false;
				}
				*v = static_cast<nkft_float32>(haut ? y1 : y0);
				return true;
			};
			// Le noeud (y, rangee entiere `px`) ; `px` en texels, ramene en unites.
			const auto noeud = [out, sy](nkft_float32 y, nkft_float32 px) {
				if (out->n < NkCalageVertical::NK_CALAGE_MAX) {
					out->src[out->n] = y;
					out->dst[out->n] = px / sy;
					++out->n;
				}
			};
			const auto rangee = [sy](nkft_float32 y) -> nkft_float32 { return floorf(y * sy + 0.5f); };
			// Une zone : sa hauteur principale posee sur la rangee `p`, et le depassement de sa ronde (arrondi a part).
			const auto zone = [&](nkft_float32 principal, bool aRonde, nkft_float32 ronde, nkft_float32 p) {
				noeud(principal, p);
				if (aRonde && ronde != principal) {
					noeud(ronde, p + floorf((ronde - principal) * sy + 0.5f));
				}
			};
			nkft_float32 v = 0.f, r = 0.f;
			bool lettres = false;
			// La ligne de base, et le bas de « o ».
			zone(0.f, borne(0x6Fu, false, &r) && r < 0.f, r, 0.f);
			// La hauteur d'x, et le haut de « o ». AUX PETITES TAILLES (moins de 7 texels d'x), elle s'arrondit vers le
			// HAUT des qu'elle depasse sa rangee d'un cinquieme (ce que font les hinters, « x-height increase » de
			// ttfautohint) : a 7 px, 3,2 texels d'x font 4 rangees au lieu de 3, et un « a », un « e », un « o » ont
			// la place de rester distincts.
			nkft_float32 rangeeX = -1.f;
			if (borne(0x78u, true, &v)) {
				lettres = true;
				out->hauteurX = v;
				const nkft_float32 t = v * sy;
				rangeeX = t < 7.f ? floorf(t + 0.8f) : rangee(v);
				zone(v, borne(0x6Fu, true, &r) && r > v, r, rangeeX);
			}
			// Au-dessus de la hauteur d'x, une rangee de plus au moins (une capitale ne se confond pas avec une
			// minuscule quand la hauteur d'x monte).
			const auto auDessusX = [&](nkft_float32 p) -> nkft_float32 { return (rangeeX >= 0.f && p < rangeeX + 1.f) ? rangeeX + 1.f : p; };
			// Les capitales, et le haut de « O ».
			if (borne(0x48u, true, &v)) {
				lettres = true;
				zone(v, borne(0x4Fu, true, &r) && r > v, r, auDessusX(rangee(v)));
			}
			// Les montantes (« d ») et les descendantes (« p »).
			if (borne(0x64u, true, &v)) {
				lettres = true;
				zone(v, false, 0.f, auDessusX(rangee(v)));
			}
			if (borne(0x70u, false, &v) && v < 0.f) {
				lettres = true;
				zone(v, false, 0.f, rangee(v));
			}
			if (!lettres) {
				out->n = 0;
				return false;
			}
			// Du plus bas au plus haut (tri par insertion) ; un noeud a moins d'une demi-unite d'un autre s'en va ;
			// les y cales ne redescendent jamais (le contour reste dans son ordre).
			for (nkft_int32 i = 1; i < out->n; ++i) {
				const nkft_float32 s = out->src[i], d = out->dst[i];
				nkft_int32 j = i - 1;
				while (j >= 0 && out->src[j] > s) {
					out->src[j + 1] = out->src[j];
					out->dst[j + 1] = out->dst[j];
					--j;
				}
				out->src[j + 1] = s;
				out->dst[j + 1] = d;
			}
			nkft_int32 k = 0;
			for (nkft_int32 i = 0; i < out->n; ++i) {
				if (k > 0 && out->src[i] - out->src[k - 1] < 0.5f) {
					continue;
				}
				out->src[k] = out->src[i];
				out->dst[k] = (k > 0 && out->dst[i] < out->dst[k - 1]) ? out->dst[k - 1] : out->dst[i];
				++k;
			}
			out->n = k;
			return k > 0;
		}

		nkft_float32 NkCalerY(const NkCalageVertical &c, nkft_float32 y) {
			return NkInterpNoeuds(c.n, c.src, c.dst, y);
		}

		// ── Les hampes (scripts-23) ──
		// Ce que fait le hinting de Windows sur l'axe horizontal : un fût vertical (le « l », les jambages du « n », le
		// flanc d'un « o ») large de 1,3 px tombe sur DEUX colonnes grises ; cale, il devient UNE colonne pleine. Les
		// bords des fûts se lisent sur la forme : un segment presque vertical assez long, ou un point du contour ou la
		// tangente est verticale (le flanc d'une ronde). Le sens de parcours dit le cote de l'encre (le bord gauche
		// d'un fût monte dans une face TrueType, descend dans une face PostScript : l'aire signee le dit). Un bord
		// gauche s'apparie au bord droit le plus proche a sa droite qui le chevauche en hauteur ; la paire devient un
		// fût de largeur ENTIERE (au moins une colonne), centre au plus pres de sa place. Le reste du glyphe suit,
		// lineaire entre les fûts ; l'avance ne change pas.
		//
		// Les BARRES (le meme travail sur l'autre axe) : la barre d'un « e », d'un « t », le haut et le bas d'une
		// ronde ; une barre dont un bord tombe deja sur une zone calee (la ligne de base, la hauteur d'x) s'y accroche
		// et prend son epaisseur entiere de l'autre cote.
		struct NkFut {
				nkft_float32 a, b; // les deux bords (a < b), en unites, sur l'axe du calage
		};

		/// Les fûts de `vb` sur l'axe `axe` (0 : bords en x, les fûts verticaux ; 1 : bords en y, les barres), a
		/// l'echelle `echelle` (texels par unite) ; rend leur nombre (au plus `maxFuts`).
		static nkft_int32 NkLireFuts(const NkFontVertexBuffer &vb, nkft_int32 axe, nkft_float32 echelle, nkft_float32 hauteurX,
									 NkFut *futs, nkft_int32 maxFuts) {
			const nkft_float32 sx = echelle; // (le nom d'origine : l'echelle de l'axe des bords)
			if (!(sx > 0.f) || !(hauteurX > 0.f) || vb.count < 2) {
				return 0;
			}
			// La position (sur l'axe) et l'etendue (l'autre axe) d'un point.
			const auto U = [axe](nkft_float32 x, nkft_float32 y) -> nkft_float32 { return axe == 0 ? x : y; };
			const auto L = [axe](nkft_float32 x, nkft_float32 y) -> nkft_float32 { return axe == 0 ? y : x; };
			// 1. L'orientation : l'aire signee (y vers le haut ; positive = sens trigonometrique = PostScript).
			nkft_float32 aire = 0.f;
			{
				nkft_float32 px = 0.f, py = 0.f;
				for (nkft_uint32 i = 0; i < vb.count; ++i) {
					const NkFontVertex &v = vb.verts[i];
					if (v.type != NK_FONT_VERTEX_MOVE) {
						aire += px * (nkft_float32)v.y - (nkft_float32)v.x * py;
					}
					px = (nkft_float32)v.x;
					py = (nkft_float32)v.y;
				}
			}
			// Le sens (sur l'etendue) d'un bord « bas » d'encre (l'encre du cote des positions croissantes) : en x, un
			// bord gauche monte dans une face TrueType ; en y (les axes echanges renversent l'orientation), un bord
			// bas va vers les x decroissants.
			const nkft_int32 sensGauche = (aire > 0.f ? -1 : 1) * (axe == 0 ? 1 : -1);
			// 2. Les bords.
			struct Bord {
					nkft_float32 x, y0, y1;
					nkft_int32 sens;
			};
			static constexpr nkft_int32 kBords = 96;
			Bord bords[kBords];
			nkft_int32 nb = 0;
			const nkft_float32 minLong = (0.2f * hauteurX > 1.5f / sx) ? 0.2f * hauteurX : 1.5f / sx;
			const nkft_float32 quartPx = 0.25f / sx;
			const auto ajouter = [&](nkft_float32 x, nkft_float32 ya, nkft_float32 yb, nkft_int32 sens) {
				const nkft_float32 y0 = ya < yb ? ya : yb, y1 = ya < yb ? yb : ya;
				for (nkft_int32 k = 0; k < nb; ++k) { // le meme bord vu deux fois (le segment, puis sa jonction)
					Bord &b = bords[k];
					if (b.sens == sens && (b.x - x < quartPx && x - b.x < quartPx) && y0 <= b.y1 && y1 >= b.y0) {
						b.y0 = b.y0 < y0 ? b.y0 : y0;
						b.y1 = b.y1 > y1 ? b.y1 : y1;
						return;
					}
				}
				if (nb < kBords) {
					bords[nb++] = Bord{x, y0, y1, sens};
				}
			};
			nkft_uint32 debut = 0; // l'indice du MOVE du contour en cours
			for (nkft_uint32 i = 0; i < vb.count; ++i) {
				const NkFontVertex &v = vb.verts[i];
				if (v.type == NK_FONT_VERTEX_MOVE) {
					debut = i;
					continue;
				}
				const NkFontVertex &p = vb.verts[i - 1]; // i > debut : le point d'avant existe
				// (« x » : la position sur l'axe ; « y » : l'etendue, sur l'autre axe)
				const nkft_float32 vx = U((nkft_float32)v.x, (nkft_float32)v.y), vy = L((nkft_float32)v.x, (nkft_float32)v.y);
				const nkft_float32 px = U((nkft_float32)p.x, (nkft_float32)p.y), py = L((nkft_float32)p.x, (nkft_float32)p.y);
				// (a) un segment droit presque perpendiculaire a l'axe, assez long
				if (v.type == NK_FONT_VERTEX_LINE) {
					const nkft_float32 dx = vx - px, dy = vy - py;
					const nkft_float32 ady = dy < 0.f ? -dy : dy, adx = dx < 0.f ? -dx : dx;
					if (ady >= minLong && adx * 8.f <= ady) {
						ajouter(0.5f * (px + vx), py, vy, dy > 0.f ? 1 : -1);
					}
				}
				// (b) le point v, ou la tangente est verticale (le flanc d'une ronde) : la poignee d'entree et celle
				// de sortie de part et d'autre de v en hauteur, a peu pres a son aplomb.
				nkft_uint32 s = i + 1;
				if (s >= vb.count || vb.verts[s].type == NK_FONT_VERTEX_MOVE) {
					s = debut + 1; // le contour se referme : la sortie est son premier segment
				}
				if (s >= vb.count || s == i) {
					continue;
				}
				const NkFontVertex &w = vb.verts[s];
				if (v.type != NK_FONT_VERTEX_CURVE && w.type != NK_FONT_VERTEX_CURVE) {
					continue; // deux droites : (a) s'en charge
				}
				const nkft_float32 ax = v.type == NK_FONT_VERTEX_CURVE ? U((nkft_float32)v.cx, (nkft_float32)v.cy) : px;
				const nkft_float32 ay = v.type == NK_FONT_VERTEX_CURVE ? L((nkft_float32)v.cx, (nkft_float32)v.cy) : py;
				const nkft_float32 bx = w.type == NK_FONT_VERTEX_CURVE ? U((nkft_float32)w.cx, (nkft_float32)w.cy) : U((nkft_float32)w.x, (nkft_float32)w.y);
				const nkft_float32 by = w.type == NK_FONT_VERTEX_CURVE ? L((nkft_float32)w.cx, (nkft_float32)w.cy) : L((nkft_float32)w.x, (nkft_float32)w.y);
				const nkft_float32 dax = ax - vx, day = ay - vy, dbx = bx - vx, dby = by - vy;
				const bool aplombA = (dax < 0.f ? -dax : dax) * 4.f <= (day < 0.f ? -day : day);
				const bool aplombB = (dbx < 0.f ? -dbx : dbx) * 4.f <= (dby < 0.f ? -dby : dby);
				const nkft_float32 etendue = by - ay;
				if (aplombA && aplombB && day * dby < 0.f && (etendue < 0.f ? -etendue : etendue) >= 0.5f * minLong) {
					ajouter(vx, ay, by, etendue > 0.f ? 1 : -1);
				}
			}
			// 3. Les fûts : un bord gauche, le bord droit le plus proche a sa droite qui le chevauche en hauteur.
			const nkft_float32 maxLarge = 0.45f * hauteurX;
			nkft_int32 n = 0;
			for (nkft_int32 a = 0; a < nb; ++a) {
				const Bord &g = bords[a];
				if (g.sens != sensGauche) {
					continue;
				}
				nkft_int32 meilleur = -1;
				for (nkft_int32 b = 0; b < nb; ++b) {
					const Bord &d = bords[b];
					if (d.sens == sensGauche || d.x <= g.x || d.x - g.x > maxLarge) {
						continue;
					}
					const nkft_float32 haut = g.y1 < d.y1 ? g.y1 : d.y1, bas = g.y0 > d.y0 ? g.y0 : d.y0;
					const nkft_float32 lg = g.y1 - g.y0, ld = d.y1 - d.y0;
					if (haut - bas < 0.3f * (lg < ld ? lg : ld)) {
						continue;
					}
					if (meilleur < 0 || d.x < bords[meilleur].x) {
						meilleur = b;
					}
				}
				if (meilleur < 0 || n >= maxFuts) {
					continue;
				}
				futs[n].a = g.x;
				futs[n].b = bords[meilleur].x;
				++n;
			}
			return n;
		}

		/// Des noeuds (src, dst) en vrac -> `h`, du plus bas au plus haut : un noeud a moins d'un quart de texel d'un
		/// autre s'en va ; un noeud qui reculerait, ou qui ecraserait sous un texel un ecart qui en faisait au moins
		/// un, s'en va aussi (les premiers venus a egalite gardent la place : tri stable).
		static void NkRangerNoeuds(nkft_float32 *src, nkft_float32 *dst, nkft_int32 n, nkft_float32 echelle, NkCalageHampes *h) {
			h->n = 0;
			const nkft_float32 sx = echelle;
			const nkft_float32 quartPx = 0.25f / sx;
			for (nkft_int32 i = 1; i < n; ++i) {
				const nkft_float32 s = src[i], d = dst[i];
				nkft_int32 j = i - 1;
				while (j >= 0 && src[j] > s) {
					src[j + 1] = src[j];
					dst[j + 1] = dst[j];
					--j;
				}
				src[j + 1] = s;
				dst[j + 1] = d;
			}
			const nkft_float32 unPx = 1.f / sx;
			for (nkft_int32 i = 0; i < n && h->n < NkCalageHampes::NK_HAMPES_MAX; ++i) {
				if (h->n > 0) {
					const nkft_float32 ecartAvant = src[i] - h->src[h->n - 1];
					const nkft_float32 ecartApres = dst[i] - h->dst[h->n - 1];
					if (ecartAvant < quartPx || ecartApres < 0.f || (ecartAvant >= unPx && ecartApres < unPx * 0.999f)) {
						continue;
					}
				}
				h->src[h->n] = src[i];
				h->dst[h->n] = dst[i];
				++h->n;
			}
		}

		/// Les HAMPES d'un glyphe : ses fûts verticaux, chacun d'une largeur entiere (au moins une colonne), centre au
		/// plus pres de sa place.
		static void NkPreparerHampes(const NkFontVertexBuffer &vb, nkft_float32 sx, nkft_float32 hauteurX, NkCalageHampes *h) {
			h->n = 0;
			NkFut futs[NkCalageHampes::NK_HAMPES_MAX / 2];
			const nkft_int32 nf = NkLireFuts(vb, 0, sx, hauteurX, futs, NkCalageHampes::NK_HAMPES_MAX / 2);
			nkft_float32 src[NkCalageHampes::NK_HAMPES_MAX], dst[NkCalageHampes::NK_HAMPES_MAX];
			nkft_int32 n = 0;
			for (nkft_int32 k = 0; k < nf; ++k) {
				const nkft_float32 l = (futs[k].b - futs[k].a) * sx;
				const nkft_float32 W = l < 1.f ? 1.f : floorf(l + 0.5f);
				const nkft_float32 gauche = floorf(futs[k].a * sx + 0.5f * (l - W) + 0.5f);
				src[n] = futs[k].a;
				dst[n] = gauche / sx;
				src[n + 1] = futs[k].b;
				dst[n + 1] = (gauche + W) / sx;
				n += 2;
			}
			if (n > 0) {
				NkRangerNoeuds(src, dst, n, sx, h);
			}
		}

		/// Les BARRES d'un glyphe, sur le calage vertical `c` (ses zones restent des noeuds, en premier) : chaque
		/// barre d'une epaisseur entiere (au moins une rangee) ; un bord deja sur une rangee entiere (une zone) reste,
		/// l'autre s'en deduit ; sinon, centree au plus pres. Rend dans `h` le calage vertical PROPRE au glyphe.
		static void NkPreparerBarres(const NkFontVertexBuffer &vb, nkft_float32 sy, const NkCalageVertical &c, NkCalageHampes *h) {
			h->n = 0;
			NkFut futs[NkCalageHampes::NK_HAMPES_MAX / 2];
			const nkft_int32 nf = NkLireFuts(vb, 1, sy, c.hauteurX, futs, NkCalageHampes::NK_HAMPES_MAX / 2 - NkCalageVertical::NK_CALAGE_MAX / 2);
			if (nf <= 0) {
				return;
			}
			nkft_float32 src[NkCalageHampes::NK_HAMPES_MAX], dst[NkCalageHampes::NK_HAMPES_MAX];
			nkft_int32 n = 0;
			for (nkft_int32 i = 0; i < c.n && n < NkCalageHampes::NK_HAMPES_MAX; ++i) {
				src[n] = c.src[i];
				dst[n] = c.dst[i];
				++n;
			}
			// Sur une rangee entiere, a l'arrondi du flottant pres.
			const auto entiere = [](nkft_float32 v) -> bool {
				const nkft_float32 e = v - floorf(v + 0.5f);
				return e < 0.01f && e > -0.01f;
			};
			for (nkft_int32 k = 0; k < nf && n + 2 <= NkCalageHampes::NK_HAMPES_MAX; ++k) {
				const nkft_float32 ga = NkCalerY(c, futs[k].a) * sy, gb = NkCalerY(c, futs[k].b) * sy; // en texels
				const nkft_float32 l = (futs[k].b - futs[k].a) * sy;
				const nkft_float32 W = l < 1.f ? 1.f : floorf(l + 0.5f);
				nkft_float32 bas;
				if (entiere(ga)) {
					bas = floorf(ga + 0.5f);
				} else if (entiere(gb)) {
					bas = floorf(gb + 0.5f) - W;
				} else {
					bas = floorf(0.5f * (ga + gb) - 0.5f * W + 0.5f);
				}
				src[n] = futs[k].a;
				dst[n] = bas / sy;
				src[n + 1] = futs[k].b;
				dst[n + 1] = (bas + W) / sy;
				n += 2;
			}
			NkRangerNoeuds(src, dst, n, sy, h);
		}

		/// La boite du glyphe CALE (`c` : le calage vertical ; `h` : ses hampes, nul ou vide : x d'origine).
		static void NkBoiteCalee(const NkFontFaceInfo *info, NkGlyphId glyph, nkft_float32 sx, nkft_float32 sy,
								 nkft_float32 shx, nkft_float32 shy, nkft_int32 *ix0, nkft_int32 *iy0, nkft_int32 *ix1,
								 nkft_int32 *iy1, const NkCalageVertical *c, const NkCalageHampes *h,
								 const NkCalageHampes *barres = nullptr) {
			NkGetGlyphBitmapBox(info, glyph, sx, sy, shx, shy, ix0, iy0, ix1, iy1);
			if (!c) {
				return;
			}
			nkft_int32 gx0 = 0, gy0 = 0, gx1 = 0, gy1 = 0;
			if (!NkGetGlyphBox(info, glyph, &gx0, &gy0, &gx1, &gy1)) {
				return;
			}
			// Les bords CALES (chaque calage est monotone : le contour cale reste entre eux). La tolerance : un bord
			// cale tombe sur une rangee (une colonne) entiere, l'arrondi du flottant ne doit pas en ajouter une vide.
			if (barres && barres->n > 0) {
				if (iy0) {
					*iy0 = (nkft_int32)floorf(-NkCalerX(*barres, (nkft_float32)gy1) * sy + shy + 1e-3f);
				}
				if (iy1) {
					*iy1 = (nkft_int32)ceilf(-NkCalerX(*barres, (nkft_float32)gy0) * sy + shy - 1e-3f);
				}
			} else if (c->n > 0) {
				if (iy0) {
					*iy0 = (nkft_int32)floorf(-NkCalerY(*c, (nkft_float32)gy1) * sy + shy + 1e-3f);
				}
				if (iy1) {
					*iy1 = (nkft_int32)ceilf(-NkCalerY(*c, (nkft_float32)gy0) * sy + shy - 1e-3f);
				}
			}
			if (h && h->n > 0) {
				if (ix0) {
					*ix0 = (nkft_int32)floorf(NkCalerX(*h, (nkft_float32)gx0) * sx + shx + 1e-3f);
				}
				if (ix1) {
					*ix1 = (nkft_int32)ceilf(NkCalerX(*h, (nkft_float32)gx1) * sx + shx - 1e-3f);
				}
			}
		}

		void NkGetGlyphBitmapBoxCale(const NkFontFaceInfo *info, NkGlyphId glyph, nkft_float32 sx, nkft_float32 sy,
									 nkft_float32 shx, nkft_float32 shy, nkft_int32 *ix0, nkft_int32 *iy0,
									 nkft_int32 *ix1, nkft_int32 *iy1, const NkCalageVertical *c) {
			if (c && c->hampes) {
				// Les hampes se lisent sur la forme : la boite les suit (la meme forme, les memes fûts qu'a la
				// rasterisation : NkMakeGlyphBitmapCale).
				NkFontVertexBuffer vb;
				NkCalageHampes h, barres;
				if (NkGetGlyphShape(info, glyph, &vb) && vb.count > 0) {
					NkPreparerHampes(vb, sx, c->hauteurX, &h);
					NkPreparerBarres(vb, sy, *c, &barres);
				}
				NkBoiteCalee(info, glyph, sx, sy, shx, shy, ix0, iy0, ix1, iy1, c, &h, &barres);
				return;
			}
			NkBoiteCalee(info, glyph, sx, sy, shx, shy, ix0, iy0, ix1, iy1, c, nullptr);
		}

		void NkMakeGlyphBitmapCale(const NkFontFaceInfo *info, nkft_uint8 *output, nkft_int32 outW, nkft_int32 outH,
								   nkft_int32 outStride, nkft_float32 scaleX, nkft_float32 scaleY, nkft_float32 shiftX,
								   nkft_float32 shiftY, NkGlyphId glyph, const NkCalageVertical *c) {
			if (!c) {
				NkMakeGlyphBitmapExact(info, output, outW, outH, outStride, scaleX, scaleY, shiftX, shiftY, glyph);
				return;
			}
			if (!info || !output || outW <= 0 || outH <= 0) {
				return;
			}
			for (nkft_int32 r = 0; r < outH; ++r) {
				memset(output + r * outStride, 0, (nkft_size)outW);
			}
			NkFontVertexBuffer vb;
			if (!NkGetGlyphShape(info, glyph, &vb) || vb.count == 0) {
				return;
			}
			NkCalageHampes h, barres;
			if (c->hampes) {
				NkPreparerHampes(vb, scaleX, c->hauteurX, &h);
				NkPreparerBarres(vb, scaleY, *c, &barres);
			}
			nkft_int32 bx0 = 0, by0 = 0, bx1 = 0, by1 = 0;
			NkBoiteCalee(info, glyph, scaleX, scaleY, shiftX, shiftY, &bx0, &by0, &bx1, &by1, c, &h, &barres);
			sRCalage = c;
			sRHampes = h.n > 0 ? &h : nullptr;
			sRBarres = barres.n > 0 ? &barres : nullptr;
			NkBuildEdges(&vb, scaleX, scaleY, shiftX - (nkft_float32)bx0, shiftY - (nkft_float32)by0);
			sRCalage = nullptr;
			sRHampes = nullptr;
			sRBarres = nullptr;
			if (sRNum == 0) {
				return;
			}
			NkSortEdges();
			if (sRNum < NK_RE_MAX_EDGES) {
				sREdges[sRNum].y0 = 1e10f;
				sREdges[sRNum].y1 = 1e10f;
			}
			NkDoRasterize(output, outW, outH, outStride, 0, 0);
		}

	} // namespace nkfont
} // namespace nkentseu

/*
// -----------------------------------------------------------------------------
// FICHIER: NKFont/Core/NkFontRasterizer.cpp
// DESCRIPTION: Rasteriseur scanline pour les contours de glyphes TrueType/OpenType.
//              Implémente l'algorithme classique à deux buffers (local + fill).
// -----------------------------------------------------------------------------

#include "NkFontParser.h"
#include <math.h>
#include <string.h>

namespace nkentseu {
	namespace nkfont {

		// ============================================================
		// CONSTANTES
		// ============================================================

		static constexpr nkft_int32 NK_RE_MAX_EDGES  = 8192;  // Nombre maximal d'arêtes
		static constexpr nkft_int32 NK_RE_MAX_ACTIVE = 1024;  // Arêtes actives maximales
		static constexpr nkft_int32 NK_RE_SCAN_W     = 4096;  // Largeur max de bitmap
		static constexpr nkft_int32 NK_RE_SCAN2_W    = NK_RE_SCAN_W + 1; // scanline2 a besoin de +1

		// ============================================================
		// STRUCTURES (arêtes et arêtes actives)
		// ============================================================

		struct NkREdge {
			nkft_float32 x0, y0, x1, y1;
			nkft_int32   invert; // 1 = direction montante dans l'espace original
		};

		struct NkRActive {
			struct NkRActive* next;
			nkft_float32 fx;        // Position X au top de la scanline courante
			nkft_float32 fdx;       // Delta X par scanline entière
			nkft_float32 fdy;       // 1/fdx si fdx != 0, sinon 0
			nkft_float32 direction; // +1.0 ou -1.0
			nkft_float32 sy;        // Y de début de l'arête
			nkft_float32 ey;        // Y de fin de l'arête
		};

		// ============================================================
		// ÉTAT GLOBAL (pool d'arêtes et buffers de scanline)
		// ============================================================

		static NkREdge   sREdges[NK_RE_MAX_EDGES];
		static nkft_int32 sRNum = 0;

		static NkRActive  sRPool[NK_RE_MAX_ACTIVE];
		static nkft_int32 sRPoolTop = 0;

		static nkft_float32 sScanline [NK_RE_SCAN_W ];   // Contribution locale
		static nkft_float32 sScanline2[NK_RE_SCAN2_W];   // Déversement (fill)

		// ============================================================
		// Gestion du pool d'arêtes actives
		// ============================================================

		static NkRActive* pool_alloc() {
			if (sRPoolTop >= NK_RE_MAX_ACTIVE) return nullptr;
			return &sRPool[sRPoolTop++];
		}

		static void pool_reset() {
			sRPoolTop = 0;
		}

		// ============================================================
		// Ajout d'une arête (normalisée avec y0 < y1)
		// ============================================================

		static void PushEdge(nkft_float32 x0, nkft_float32 y0,
							nkft_float32 x1, nkft_float32 y1,
							nkft_int32 invert) {
			if (y0 == y1) return;
			if (sRNum >= NK_RE_MAX_EDGES) return;
			NkREdge& e = sREdges[sRNum++];
			if (y0 < y1) {
				e.x0 = x0; e.y0 = y0; e.x1 = x1; e.y1 = y1; e.invert = invert;
			} else {
				e.x0 = x1; e.y0 = y1; e.x1 = x0; e.y1 = y0; e.invert = !invert;
			}
		}

		// ============================================================
		// Aplatissement de courbe quadratique (Bézier)
		// ============================================================

		static void FlattenQuad(nkft_float32 x0, nkft_float32 y0,
								nkft_float32 x1, nkft_float32 y1,
								nkft_float32 x2, nkft_float32 y2,
								nkft_float32 objspace_flatness_sq,
								nkft_int32 n) {
			nkft_float32 mx = (x0 + 2*x1 + x2) / 4;
			nkft_float32 my = (y0 + 2*y1 + y2) / 4;
			nkft_float32 dx = (x0 + x2) / 2 - mx;
			nkft_float32 dy = (y0 + y2) / 2 - my;
			if (n > 16) {
				PushEdge(x0, y0, x2, y2, 1);
				return;
			}
			if (dx*dx + dy*dy > objspace_flatness_sq) {
				nkft_float32 m01x = (x0+x1)*0.5f, m01y = (y0+y1)*0.5f;
				nkft_float32 m12x = (x1+x2)*0.5f, m12y = (y1+y2)*0.5f;
				nkft_float32 mmx  = (m01x+m12x)*0.5f, mmy = (m01y+m12y)*0.5f;
				FlattenQuad(x0, y0, m01x, m01y, mmx, mmy, objspace_flatness_sq, n+1);
				FlattenQuad(mmx, mmy, m12x, m12y, x2, y2, objspace_flatness_sq, n+1);
			} else {
				PushEdge(x0, y0, x2, y2, 1);
			}
		}

		// ============================================================
		// Aplatissement de courbe cubique (Bézier)
		// ============================================================

		static void FlattenCubic(nkft_float32 x0, nkft_float32 y0,
								nkft_float32 x1, nkft_float32 y1,
								nkft_float32 x2, nkft_float32 y2,
								nkft_float32 x3, nkft_float32 y3,
								nkft_float32 objspace_flatness_sq,
								nkft_int32 n) {
			nkft_float32 dx0 = x1-x0, dy0 = y1-y0;
			nkft_float32 dx1 = x2-x1, dy1 = y2-y1;
			nkft_float32 dx2 = x3-x2, dy2 = y3-y2;
			nkft_float32 dx  = x3-x0, dy  = y3-y0;
			nkft_float32 ll = (nkft_float32)(sqrtf(dx0*dx0+dy0*dy0) +
											sqrtf(dx1*dx1+dy1*dy1) +
											sqrtf(dx2*dx2+dy2*dy2));
			nkft_float32 sl = sqrtf(dx*dx+dy*dy);
			nkft_float32 flat_sq = ll*ll - sl*sl;
			if (n > 16) {
				PushEdge(x0, y0, x3, y3, 1);
				return;
			}
			if (flat_sq > objspace_flatness_sq) {
				nkft_float32 x01=(x0+x1)*0.5f, y01=(y0+y1)*0.5f;
				nkft_float32 x12=(x1+x2)*0.5f, y12=(y1+y2)*0.5f;
				nkft_float32 x23=(x2+x3)*0.5f, y23=(y2+y3)*0.5f;
				nkft_float32 xa=(x01+x12)*0.5f, ya=(y01+y12)*0.5f;
				nkft_float32 xb=(x12+x23)*0.5f, yb=(y12+y23)*0.5f;
				nkft_float32 mx=(xa+xb)*0.5f,   my=(ya+yb)*0.5f;
				FlattenCubic(x0,y0, x01,y01, xa,ya, mx,my, objspace_flatness_sq, n+1);
				FlattenCubic(mx,my, xb,yb, x23,y23, x3,y3, objspace_flatness_sq, n+1);
			} else {
				PushEdge(x0, y0, x3, y3, 1);
			}
		}

		// ============================================================
		// Construction des arêtes à partir des vertices du contour
		// ============================================================

		static void BuildEdges(const NkFontVertexBuffer* vb,
							nkft_float32 sx, nkft_float32 sy,
							nkft_float32 ox, nkft_float32 oy) {
			sRNum = 0;
			nkft_float32 px = 0.f, py = 0.f;
			const nkft_float32 flatness = 0.35f;
			const nkft_float32 flat_sq = flatness * flatness;

			for (nkft_uint32 i = 0; i < vb->count; ++i) {
				const NkFontVertex& v = vb->verts[i];
				nkft_float32 vx =  v.x * sx + ox;
				nkft_float32 vy = -v.y * sy + oy;   // Flip Y (TrueType → écran)

				switch (v.type) {
				case NK_FONT_VERTEX_MOVE:
					px = vx; py = vy;
					break;
				case NK_FONT_VERTEX_LINE:
					if (py != vy) PushEdge(px, py, vx, vy, 1);
					px = vx; py = vy;
					break;
				case NK_FONT_VERTEX_CURVE: {
					nkft_float32 cx =  v.cx * sx + ox;
					nkft_float32 cy = -v.cy * sy + oy;
					FlattenQuad(px, py, cx, cy, vx, vy, flat_sq, 0);
					px = vx; py = vy;
					break;
				}
				case NK_FONT_VERTEX_CUBIC: {
					nkft_float32 c0x =  v.cx  * sx + ox, c0y = -v.cy  * sy + oy;
					nkft_float32 c1x =  v.cx1 * sx + ox, c1y = -v.cy1 * sy + oy;
					FlattenCubic(px, py, c0x, c0y, c1x, c1y, vx, vy, flat_sq, 0);
					px = vx; py = vy;
					break;
				}
				}
			}
		}

		// ============================================================
		// Comparateur pour tri des arêtes par y0
		// ============================================================

		static int CompareEdge(const void* a, const void* b) {
			const NkREdge* ea = (const NkREdge*)a;
			const NkREdge* eb = (const NkREdge*)b;
			if (ea->y0 < eb->y0) return -1;
			if (ea->y0 > eb->y0) return  1;
			return 0;
		}

		static void SortEdges() {
			for (nkft_int32 i = 1; i < sRNum; ++i) {
				NkREdge t = sREdges[i];
				nkft_int32 j = i - 1;
				while (j >= 0 && sREdges[j].y0 > t.y0) {
					sREdges[j+1] = sREdges[j];
					--j;
				}
				sREdges[j+1] = t;
			}
		}

		// ============================================================
		// Gestion d'un segment d'arête dans un pixel donné
		// ============================================================

		static void HandleClippedEdge(nkft_float32* scanline, nkft_int32 x,
									NkRActive* e,
									nkft_float32 x0, nkft_float32 y0,
									nkft_float32 x1, nkft_float32 y1) {
			if (y0 == y1) return;
			if (y0 > e->ey) return;
			if (y1 < e->sy) return;
			if (y0 < e->sy) {
				x0 += (x1-x0) * (e->sy - y0) / (y1-y0);
				y0 = e->sy;
			}
			if (y1 > e->ey) {
				x1 += (x1-x0) * (e->ey - y1) / (y1-y0);
				y1 = e->ey;
			}
			if (x0 <= x && x1 <= x)
				scanline[x] += e->direction * (y1-y0);
			else if (x0 >= x+1 && x1 >= x+1)
				; // entièrement à droite du pixel
			else {
				// partiellement dans le pixel
				scanline[x] += e->direction * (y1-y0) * (1.f - ((x0-x)+(x1-x))/2.f);
			}
		}

		// ============================================================
		// Aires de trapèzes / triangles (helpers)
		// ============================================================

		static inline nkft_float32 SizedTrapArea(nkft_float32 height,
												nkft_float32 top_w,
												nkft_float32 bot_w) {
			return (top_w + bot_w) / 2.f * height;
		}

		static inline nkft_float32 PosTrapArea(nkft_float32 height,
												nkft_float32 tx0, nkft_float32 tx1,
												nkft_float32 bx0, nkft_float32 bx1) {
			return SizedTrapArea(height, tx1-tx0, bx1-bx0);
		}

		static inline nkft_float32 SizedTriArea(nkft_float32 height, nkft_float32 width) {
			return height * width / 2.f;
		}

		// ============================================================
		// Remplissage des arêtes actives pour une scanline (deux buffers)
		// ============================================================

		static void FillActiveEdgesNew(nkft_float32* scanline,
									nkft_float32* scanline_fill,
									nkft_int32 len,
									NkRActive* e,
									nkft_float32 y_top) {
			nkft_float32 y_bottom = y_top + 1.f;
			while (e) {
				if (e->fdx == 0.f) {
					// Arête verticale
					nkft_float32 x0 = e->fx;
					if (x0 < len) {
						if (x0 >= 0.f) {
							HandleClippedEdge(scanline, (nkft_int32)x0,    e, x0, y_top, x0, y_bottom);
							HandleClippedEdge(scanline_fill-1, (nkft_int32)x0+1, e, x0, y_top, x0, y_bottom);
						} else {
							HandleClippedEdge(scanline_fill-1, 0, e, x0, y_top, x0, y_bottom);
						}
					}
				} else {
					// Arête oblique
					nkft_float32 x0 = e->fx;
					nkft_float32 dx = e->fdx;
					nkft_float32 xb = x0 + dx;
					nkft_float32 x_top, x_bottom, sy0, sy1;
					nkft_float32 dy = e->fdy;

					if (e->sy > y_top) {
						x_top = x0 + dx * (e->sy - y_top);
						sy0 = e->sy;
					} else {
						x_top = x0;
						sy0 = y_top;
					}
					if (e->ey < y_bottom) {
						x_bottom = x0 + dx * (e->ey - y_top);
						sy1 = e->ey;
					} else {
						x_bottom = xb;
						sy1 = y_bottom;
					}

					if (x_top >= 0.f && x_bottom >= 0.f && x_top < len && x_bottom < len) {
						// Cas entièrement dans le bitmap
						if ((nkft_int32)x_top == (nkft_int32)x_bottom) {
							nkft_float32 height;
							nkft_int32 x = (nkft_int32)x_top;
							height = (sy1 - sy0) * e->direction;
							scanline[x]      += PosTrapArea(height, x_top, x+1.f, x_bottom, x+1.f);
							scanline_fill[x] += height;
						} else {
							nkft_int32 x, x1, x2;
							nkft_float32 y_crossing, y_final, step, sign, area;
							if (x_top > x_bottom) {
								nkft_float32 t;
								sy0 = y_bottom - (sy0 - y_top);
								sy1 = y_bottom - (sy1 - y_top);
								t = sy0; sy0 = sy1; sy1 = t;
								t = x_bottom; x_bottom = x_top; x_top = t;
								dx = -dx; dy = -dy;
								t = x0; x0 = xb; xb = t;
							}
							x1 = (nkft_int32)x_top;
							x2 = (nkft_int32)x_bottom;
							y_crossing = y_top + dy * (x1+1 - x0);
							y_final    = y_top + dy * (x2 - x0);
							if (y_crossing > y_bottom) y_crossing = y_bottom;
							sign = e->direction;
							area = sign * (y_crossing - sy0);
							scanline[x1] += SizedTriArea(area, x1+1 - x_top);
							if (y_final > y_bottom) {
								y_final = y_bottom;
								dy = (y_final - y_crossing) / (x2 - (x1+1));
							}
							step = sign * dy * 1.f;
							for (x = x1+1; x < x2; ++x) {
								scanline[x] += area + step/2.f;
								area += step;
							}
							scanline[x2] += area + sign * PosTrapArea(sy1 - y_final,
													(nkft_float32)x2, x2+1.f,
													x_bottom, x2+1.f);
							scanline_fill[x2] += sign * (sy1 - sy0);
						}
					} else {
						// Chemin lent : clipping par pixel (arête sort du bitmap)
						for (nkft_int32 x = 0; x < len; ++x) {
							nkft_float32 y0_ = y_top;
							nkft_float32 x1_ = (nkft_float32)x;
							nkft_float32 x2_ = (nkft_float32)(x+1);
							nkft_float32 x3_ = xb;
							nkft_float32 y3_ = y_bottom;
							nkft_float32 y1_ = (x - x0) / dx + y_top;
							nkft_float32 y2_ = (x+1 - x0) / dx + y_top;
							if (x0 < x1_ && x3_ > x2_) {
								HandleClippedEdge(scanline,x,e, x0,y0_, x1_,y1_);
								HandleClippedEdge(scanline,x,e, x1_,y1_, x2_,y2_);
								HandleClippedEdge(scanline,x,e, x2_,y2_, x3_,y3_);
							} else if (x3_ < x1_ && x0 > x2_) {
								HandleClippedEdge(scanline,x,e, x0,y0_, x2_,y2_);
								HandleClippedEdge(scanline,x,e, x2_,y2_, x1_,y1_);
								HandleClippedEdge(scanline,x,e, x1_,y1_, x3_,y3_);
							} else if (x0 < x1_ && x3_ > x1_) {
								HandleClippedEdge(scanline,x,e, x0,y0_, x1_,y1_);
								HandleClippedEdge(scanline,x,e, x1_,y1_, x3_,y3_);
							} else if (x3_ < x1_ && x0 > x1_) {
								HandleClippedEdge(scanline,x,e, x0,y0_, x1_,y1_);
								HandleClippedEdge(scanline,x,e, x1_,y1_, x3_,y3_);
							} else if (x0 < x2_ && x3_ > x2_) {
								HandleClippedEdge(scanline,x,e, x0,y0_, x2_,y2_);
								HandleClippedEdge(scanline,x,e, x2_,y2_, x3_,y3_);
							} else if (x3_ < x2_ && x0 > x2_) {
								HandleClippedEdge(scanline,x,e, x0,y0_, x2_,y2_);
								HandleClippedEdge(scanline,x,e, x2_,y2_, x3_,y3_);
							} else {
								HandleClippedEdge(scanline,x,e, x0,y0_, x3_,y3_);
							}
						}
					}
				}
				e = e->next;
			}
		}

		// ============================================================
		// Rastérisation principale (deux buffers + intégration)
		// ============================================================

		static void DoRasterize(nkft_uint8* out, nkft_int32 W, nkft_int32 H,
								nkft_int32 stride, nkft_int32 off_x, nkft_int32 off_y) {
			pool_reset();
			NkRActive* active = nullptr;
			nkft_int32 j = 0;
			nkft_int32 ei = 0;
			sREdges[sRNum].y0 = (nkft_float32)(off_y + H) + 1.f; // sentinelle

			while (j < H) {
				nkft_float32 scan_y_top    = (nkft_float32)(off_y + j) + 0.0f;
				nkft_float32 scan_y_bottom = (nkft_float32)(off_y + j) + 1.0f;

				// Nettoyage des buffers
				memset(sScanline,  0, (nkft_size)W        * sizeof(nkft_float32));
				memset(sScanline2, 0, (nkft_size)(W+1)    * sizeof(nkft_float32));

				// Suppression des arêtes terminées
				{
					NkRActive** step = &active;
					while (*step) {
						NkRActive* z = *step;
						if (z->ey <= scan_y_top) {
							*step = z->next;
							z->direction = 0;
						} else {
							step = &((*step)->next);
						}
					}
				}

				// Activation des nouvelles arêtes
				while (sREdges[ei].y0 <= scan_y_bottom) {
					if (sREdges[ei].y0 != sREdges[ei].y1) {
						NkRActive* z = pool_alloc();
						if (z) {
							nkft_float32 dxdy = (sREdges[ei].x1 - sREdges[ei].x0) /
												(sREdges[ei].y1 - sREdges[ei].y0);
							z->fdx = dxdy;
							z->fdy = (dxdy != 0.f) ? (1.f / dxdy) : 0.f;
							z->fx  = sREdges[ei].x0 + dxdy * (scan_y_top - sREdges[ei].y0);
							z->fx -= (nkft_float32)off_x;
							z->direction = sREdges[ei].invert ? 1.f : -1.f;
							z->sy = sREdges[ei].y0;
							z->ey = sREdges[ei].y1;
							if (j == 0 && off_y != 0 && z->ey < scan_y_top) z->ey = scan_y_top;
							z->next = active;
							active  = z;
						}
					}
					++ei;
				}

				// Calcul des contributions
				if (active)
					FillActiveEdgesNew(sScanline, sScanline2 + 1, W, active, scan_y_top);

				// Intégration (local + fill) → pixels
				{
					nkft_float32 sum = 0.f;
					nkft_uint8* row = out + j * stride;
					for (nkft_int32 i = 0; i < W; ++i) {
						nkft_float32 k;
						nkft_int32 m;
						sum += sScanline2[i];
						k = sScanline[i] + sum;
						k = (k < 0.f ? -k : k) * 255.f + 0.5f;
						m = (nkft_int32)k;
						if (m > 255) m = 255;
						row[i] = (nkft_uint8)m;
					}
				}

				// Avance des arêtes actives (fx += fdx)
				{
					NkRActive** step = &active;
					while (*step) {
						NkRActive* z = *step;
						z->fx += z->fdx;
						step = &((*step)->next);
					}
				}

				++j;
			}
		}

		// ============================================================
		// API PUBLIQUE
		// ============================================================

		void MakeGlyphBitmapExact(const NkFontFaceInfo* info,
								nkft_uint8*  output,
								nkft_int32   outW,    nkft_int32   outH,    nkft_int32   outStride,
								nkft_float32 scaleX,  nkft_float32 scaleY,
								nkft_float32 shiftX,  nkft_float32 shiftY,
								NkGlyphId    glyph) {
			if (!info || !output || outW <= 0 || outH <= 0) return;

			for (nkft_int32 r = 0; r < outH; ++r)
				memset(output + r * outStride, 0, (nkft_size)outW);

			NkFontVertexBuffer vb;
			if (!GetGlyphShape(info, glyph, &vb) || vb.count == 0) return;

			nkft_int32 bx0, by0, bx1, by1;
			GetGlyphBitmapBox(info, glyph, scaleX, scaleY, shiftX, shiftY,
							&bx0, &by0, &bx1, &by1);

			BuildEdges(&vb, scaleX, scaleY,
					shiftX - (nkft_float32)bx0,
					shiftY - (nkft_float32)by0);

			if (sRNum == 0) return;

			SortEdges();

			if (sRNum < NK_RE_MAX_EDGES) {
				sREdges[sRNum].y0 = 1e10f;
				sREdges[sRNum].y1 = 1e10f;
			}

			DoRasterize(output, outW, outH, outStride, 0, 0);
		}

		void MakeGlyphBitmap(const NkFontFaceInfo* info,
							nkft_uint8*  output,
							nkft_int32   outW,    nkft_int32   outH,    nkft_int32   outStride,
							nkft_float32 scaleX,  nkft_float32 scaleY,
							nkft_float32 shiftX,  nkft_float32 shiftY,
							NkGlyphId    glyph) {
			MakeGlyphBitmapExact(info, output, outW, outH, outStride,
								scaleX, scaleY, shiftX, shiftY, glyph);
		}

	} // namespace nkfont
} // namespace nkentseu
*/