// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// FICHIER: NKFont/Core/NkFontContour.cpp
// DESCRIPTION: Le contour d'un glyphe PAR DISTANCE (NkFontConfig::contourPx).
//
// ORIGINE (19/11/2026, Rodolf sur les captures du Ludo NKORA, au zoom) : le contour fonce des libelles (« Jouer »,
// « Reprendre ») avait des bords en marches alors que le texte plein etait lisse. Il etait fait de seize copies du
// glyphe decalees en cercle, chacune calee au pixel et composee « par-dessus » : la couverture de copies voisines
// se cumulait (1 - (1 - a)^k) et le bord anticrenele devenait un bord dur, a marches.
//
// ICI : le glyphe DILATE de r pixels ecran, calcule une fois dans l'atlas. Pour chaque texel,
//   alpha = max(couverture du glyphe, clamp(r + 0,5 - d, 0, 1)),
// d = distance (pixels ecran) du centre du texel au trace VECTORIEL du glyphe (courbes aplaties a 0,05 px pres). La
// rampe d'un pixel est la couverture d'un bord droit par un pixel : le bord du contour est anticrenele et rond, a
// toute epaisseur, en UN quad par glyphe. Les segments horizontaux comptent (le rasteriseur, lui, les ignore : ils
// ne changent pas le remplissage, mais ils portent la distance au-dessus d'une barre de « T »).
// -----------------------------------------------------------------------------
#include "NkFontParser.h"
#include "NKMath/NkFunctions.h"
#include "NKMemory/NkAllocator.h"

namespace nkentseu {
	namespace nkfont {

		namespace {
			static constexpr nkft_float32 TRES_LOIN = 1.0e30f;
			static constexpr nkft_float32 ECART_COURBE = 0.05f; ///< pixels : ecart maximal d'une courbe a ses cordes
			static constexpr nkft_int32 PAS_COURBE_MAX = 64;

			nkft_float32 Mini(nkft_float32 a, nkft_float32 b) {
				return a < b ? a : b;
			}
			nkft_float32 Maxi(nkft_float32 a, nkft_float32 b) {
				return a > b ? a : b;
			}

			/// Le carre de la plus petite distance (pixels ecran) de chaque texel aux segments vus.
			struct Distances {
					nkft_float32 *d2 = nullptr;
					nkft_int32 w = 0, h = 0;
					nkft_float32 pasX = 1.f, pasY = 1.f; ///< pixels ecran par texel
					nkft_float32 portee = 1.f;			 ///< pixels ecran : au-dela, la rampe vaut 0

					/// Segment en coordonnees de texel.
					void Segment(nkft_float32 ax, nkft_float32 ay, nkft_float32 bx, nkft_float32 by) {
						const nkft_float32 AX = ax * pasX, AY = ay * pasY;
						const nkft_float32 ex = bx * pasX - AX, ey = by * pasY - AY;
						const nkft_float32 l2 = ex * ex + ey * ey;
						const nkft_float32 mx = portee / pasX, my = portee / pasY;
						nkft_int32 i0 = static_cast<nkft_int32>(math::NkFloor(Mini(ax, bx) - mx));
						nkft_int32 i1 = static_cast<nkft_int32>(math::NkCeil(Maxi(ax, bx) + mx));
						nkft_int32 j0 = static_cast<nkft_int32>(math::NkFloor(Mini(ay, by) - my));
						nkft_int32 j1 = static_cast<nkft_int32>(math::NkCeil(Maxi(ay, by) + my));
						i0 = i0 < 0 ? 0 : i0;
						j0 = j0 < 0 ? 0 : j0;
						i1 = i1 > w - 1 ? w - 1 : i1;
						j1 = j1 > h - 1 ? h - 1 : j1;
						for (nkft_int32 j = j0; j <= j1; ++j) {
							const nkft_float32 py = (static_cast<nkft_float32>(j) + 0.5f) * pasY - AY;
							nkft_float32 *ligne = d2 + j * w;
							for (nkft_int32 i = i0; i <= i1; ++i) {
								const nkft_float32 px = (static_cast<nkft_float32>(i) + 0.5f) * pasX - AX;
								nkft_float32 t = l2 > 0.f ? (px * ex + py * ey) / l2 : 0.f;
								t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
								const nkft_float32 dx = px - t * ex, dy = py - t * ey;
								const nkft_float32 q = dx * dx + dy * dy;
								if (q < ligne[i]) {
									ligne[i] = q;
								}
							}
						}
					}

					/// Nombre de cordes pour qu'une courbe de derivee seconde bornee par `courbure` (pixels) s'en ecarte
					/// d'au plus ECART_COURBE : e = courbure / (8 n^2).
					static nkft_int32 Cordes(nkft_float32 courbure) {
						const nkft_float32 n = math::NkCeil(math::NkSqrt(courbure / (8.f * ECART_COURBE)));
						const nkft_int32 k = static_cast<nkft_int32>(n);
						return k < 1 ? 1 : (k > PAS_COURBE_MAX ? PAS_COURBE_MAX : k);
					}

					nkft_float32 LongueurEcran(nkft_float32 x, nkft_float32 y) const {
						return math::NkSqrt(x * pasX * x * pasX + y * pasY * y * pasY);
					}

					void Quadratique(nkft_float32 x0, nkft_float32 y0, nkft_float32 cx, nkft_float32 cy, nkft_float32 x1,
									 nkft_float32 y1) {
						// B'' = 2 (P0 - 2 P1 + P2)
						const nkft_int32 n = Cordes(2.f * LongueurEcran(x0 - 2.f * cx + x1, y0 - 2.f * cy + y1));
						nkft_float32 px = x0, py = y0;
						for (nkft_int32 k = 1; k <= n; ++k) {
							const nkft_float32 t = static_cast<nkft_float32>(k) / static_cast<nkft_float32>(n), u = 1.f - t;
							const nkft_float32 qx = u * u * x0 + 2.f * u * t * cx + t * t * x1;
							const nkft_float32 qy = u * u * y0 + 2.f * u * t * cy + t * t * y1;
							Segment(px, py, qx, qy);
							px = qx;
							py = qy;
						}
					}

					void Cubique(nkft_float32 x0, nkft_float32 y0, nkft_float32 c0x, nkft_float32 c0y, nkft_float32 c1x,
								 nkft_float32 c1y, nkft_float32 x1, nkft_float32 y1) {
						// |B''| <= 6 max(|P0 - 2 P1 + P2|, |P1 - 2 P2 + P3|)
						const nkft_float32 a = LongueurEcran(x0 - 2.f * c0x + c1x, y0 - 2.f * c0y + c1y);
						const nkft_float32 b = LongueurEcran(c0x - 2.f * c1x + x1, c0y - 2.f * c1y + y1);
						const nkft_int32 n = Cordes(6.f * Maxi(a, b));
						nkft_float32 px = x0, py = y0;
						for (nkft_int32 k = 1; k <= n; ++k) {
							const nkft_float32 t = static_cast<nkft_float32>(k) / static_cast<nkft_float32>(n), u = 1.f - t;
							const nkft_float32 qx = u * u * u * x0 + 3.f * u * u * t * c0x + 3.f * u * t * t * c1x + t * t * t * x1;
							const nkft_float32 qy = u * u * u * y0 + 3.f * u * u * t * c0y + 3.f * u * t * t * c1y + t * t * t * y1;
							Segment(px, py, qx, qy);
							px = qx;
							py = qy;
						}
					}
			};
		} // namespace

		bool NkDilaterGlyphe(const NkFontFaceInfo *info, NkGlyphId glyph, nkft_float32 sx, nkft_float32 sy, nkft_float32 ox,
							 nkft_float32 oy, nkft_uint8 *buf, nkft_int32 w, nkft_int32 h, nkft_int32 stride, nkft_float32 pasX,
							 nkft_float32 pasY, nkft_float32 rayon) {
			if (!info || !buf || w <= 0 || h <= 0 || stride < w || rayon <= 0.f || pasX <= 0.f || pasY <= 0.f) {
				return false;
			}
			NkFontVertexBuffer *vb = static_cast<NkFontVertexBuffer *>(memory::NkAlloc(sizeof(NkFontVertexBuffer)));
			nkft_float32 *d2 = static_cast<nkft_float32 *>(memory::NkAlloc(static_cast<nk_size>(w) * static_cast<nk_size>(h) * sizeof(nkft_float32)));
			if (!vb || !d2) {
				if (vb) {
					memory::NkFree(vb);
				}
				if (d2) {
					memory::NkFree(d2);
				}
				return false;
			}
			vb->count = 0;
			const nkft_int32 n = w * h;
			for (nkft_int32 i = 0; i < n; ++i) {
				d2[i] = TRES_LOIN;
			}
			Distances dist;
			dist.d2 = d2;
			dist.w = w;
			dist.h = h;
			dist.pasX = pasX;
			dist.pasY = pasY;
			dist.portee = rayon + 1.f;

			if (NkGetGlyphShape(info, glyph, vb)) {
				nkft_float32 px = 0.f, py = 0.f, debutX = 0.f, debutY = 0.f;
				bool ouvert = false;
				for (nkft_uint32 k = 0; k < vb->count; ++k) {
					const NkFontVertex &v = vb->verts[k];
					const nkft_float32 vx = static_cast<nkft_float32>(v.x) * sx + ox;
					const nkft_float32 vy = -static_cast<nkft_float32>(v.y) * sy + oy;
					switch (v.type) {
						case NK_FONT_VERTEX_MOVE:
							if (ouvert && (px != debutX || py != debutY)) {
								dist.Segment(px, py, debutX, debutY); // un contour se ferme toujours
							}
							debutX = vx;
							debutY = vy;
							ouvert = true;
							break;
						case NK_FONT_VERTEX_LINE:
							dist.Segment(px, py, vx, vy);
							break;
						case NK_FONT_VERTEX_CURVE:
							dist.Quadratique(px, py, static_cast<nkft_float32>(v.cx) * sx + ox, -static_cast<nkft_float32>(v.cy) * sy + oy,
											 vx, vy);
							break;
						case NK_FONT_VERTEX_CUBIC:
							dist.Cubique(px, py, static_cast<nkft_float32>(v.cx) * sx + ox, -static_cast<nkft_float32>(v.cy) * sy + oy,
										 static_cast<nkft_float32>(v.cx1) * sx + ox, -static_cast<nkft_float32>(v.cy1) * sy + oy, vx, vy);
							break;
						default:
							break;
					}
					px = vx;
					py = vy;
				}
				if (ouvert && (px != debutX || py != debutY)) {
					dist.Segment(px, py, debutX, debutY);
				}
			}

			for (nkft_int32 j = 0; j < h; ++j) {
				nkft_uint8 *ligne = buf + j * stride;
				for (nkft_int32 i = 0; i < w; ++i) {
					const nkft_float32 q = d2[j * w + i];
					nkft_float32 a = q < TRES_LOIN ? rayon + 0.5f - math::NkSqrt(q) : 0.f;
					a = a < 0.f ? 0.f : (a > 1.f ? 1.f : a);
					const nkft_float32 c = static_cast<nkft_float32>(ligne[i]) / 255.f;
					const nkft_float32 m = c > a ? c : a;
					ligne[i] = static_cast<nkft_uint8>(m * 255.f + 0.5f);
				}
			}
			memory::NkFree(d2);
			memory::NkFree(vb);
			return true;
		}

		void NkFiltrerSousPixel(nkft_uint8 *buf, nkft_int32 w, nkft_int32 h, nkft_int32 stride, nkft_int32 facteur) {
			if (!buf || w <= 0 || h <= 0 || facteur < 2) {
				return;
			}
			// Boite de `facteur` texels (un pixel ecran) : out[i] = moyenne de in[i - facteur + 1 .. i]. De droite a
			// gauche, en place : out[i] ne lit que des in[k <= i], pas encore reecrits.
			for (nkft_int32 j = 0; j < h; ++j) {
				nkft_uint8 *ligne = buf + j * stride;
				for (nkft_int32 i = w - 1; i >= 0; --i) {
					nkft_int32 somme = 0;
					for (nkft_int32 k = 0; k < facteur; ++k) {
						const nkft_int32 x = i - k;
						somme += x >= 0 ? static_cast<nkft_int32>(ligne[x]) : 0;
					}
					ligne[i] = static_cast<nkft_uint8>((somme + facteur / 2) / facteur);
				}
			}
		}

	} // namespace nkfont
} // namespace nkentseu
