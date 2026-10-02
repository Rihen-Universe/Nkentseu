// -----------------------------------------------------------------------------
// FICHIER: Unkeny/Maillage/NkUnkenyMaillage.cpp
// DESCRIPTION: La geometrie du maillage 2D (voir NkUnkenyMaillage.h) : lecture,
//              fabrication (contour de l'alpha, Delaunay), edition, parties.
//              Aucun dessin, aucune physique ici.
//
// LES OUTILS, ET CE QUI LES PORTE DEJA
//   - le contour d'une forme 2D      -> NkContourForme2D (Scene/NkUnkenyFormes.h)
//   - l'interieur d'un contour       -> NkDistanceContour2D (NkUnkenyComposants.h)
//   - l'aire signee, l'enveloppe     -> NkAireSignee2D, NkEnveloppeConvexe2D
//   - le decoupage en oreilles       -> NkEarcutVers (NKMath, sans allocation)
//   Ce qui manquait au depot, et qui est donc ecrit ICI : le contour de l'ALPHA
//   d'une image (suivi de fissure sur la grille des pixels), la simplification
//   de Ramer-Douglas-Peucker, et la triangulation de DELAUNAY (Bowyer-Watson)
//   avec points interieurs -- NkEarcut ne pose aucun point dedans, et un
//   maillage sans point interieur ne se deforme pas (il ne fait que plier
//   le long de ses grands triangles).
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Unkeny/Maillage/NkUnkenyMaillage.h"

#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NkEarcut.h"
#include "Unkeny/Scene/NkUnkenyFormes.h"

#include <cstddef>
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace unkeny {

		namespace {
			constexpr float32 EPS_SOUDURE = 1.0e-5f; ///< deux sommets plus proches sont « a la meme place »

			float32 Min(float32 a, float32 b) noexcept {
				return a < b ? a : b;
			}
			float32 Max(float32 a, float32 b) noexcept {
				return a > b ? a : b;
			}
			float32 Abs(float32 a) noexcept {
				return a < 0.f ? -a : a;
			}
			float32 Croix(const NkVec2f &a, const NkVec2f &b, const NkVec2f &c) noexcept {
				return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
			}
			float32 Dist2(const NkVec2f &a, const NkVec2f &b) noexcept {
				const float32 dx = a.x - b.x, dy = a.y - b.y;
				return dx * dx + dy * dy;
			}
			bool MemePlace(const NkVec2f &a, const NkVec2f &b) noexcept {
				return Abs(a.x - b.x) <= EPS_SOUDURE && Abs(a.y - b.y) <= EPS_SOUDURE;
			}

			/// Les coordonnees barycentriques de `p` dans (a, b, c) ; fausses si degenere.
			bool Barycentre(const NkVec2f &a, const NkVec2f &b, const NkVec2f &c, const NkVec2f &p, float32 &l0, float32 &l1,
							float32 &l2) noexcept {
				const float32 d = Croix(a, b, c);
				if (Abs(d) < 1.0e-12f) {
					return false;
				}
				l0 = Croix(b, c, p) / d;
				l1 = Croix(c, a, p) / d;
				l2 = 1.f - l0 - l1;
				return true;
			}

			uint32 Melanger(uint32 a, uint32 b, uint32 c, float32 l0, float32 l1, float32 l2) noexcept {
				uint32 r = 0u;
				for (int32 k = 0; k < 4; ++k) {
					const int32 s = 24 - 8 * k;
					const float32 v = static_cast<float32>((a >> s) & 0xFFu) * l0 + static_cast<float32>((b >> s) & 0xFFu) * l1 +
									  static_cast<float32>((c >> s) & 0xFFu) * l2;
					const int32 q = static_cast<int32>(v + 0.5f);
					r |= static_cast<uint32>(q < 0 ? 0 : (q > 255 ? 255 : q)) << s;
				}
				return r;
			}

			/// Un sommet « libre » : sans os, poids nuls.
			void SansOs(NkMaillage2D &m, uint32 s) noexcept {
				for (uint32 k = 0; k < NK_MAILLAGE2D_OS; ++k) {
					m.os[s * NK_MAILLAGE2D_OS + k] = NK_MAILLAGE2D_SANS_OS;
					m.poids[s * NK_MAILLAGE2D_OS + k] = 0u;
				}
			}

			/// Copie les attributs du sommet `src` de `a` vers `dst` de `b`.
			void CopierSommet(NkMaillage2D &b, uint32 dst, const NkMaillage2D &a, uint32 src) noexcept {
				b.positions[dst] = a.positions[src];
				b.uvs[dst] = a.uvs[src];
				b.couleurs[dst] = a.couleurs[src];
				b.partieSommet[dst] = a.partieSommet[src];
				for (uint32 k = 0; k < NK_MAILLAGE2D_OS; ++k) {
					b.os[dst * NK_MAILLAGE2D_OS + k] = a.os[src * NK_MAILLAGE2D_OS + k];
					b.poids[dst * NK_MAILLAGE2D_OS + k] = a.poids[src * NK_MAILLAGE2D_OS + k];
				}
			}

			NkVec2f Pos(const NkMaillage2D &m, uint32 t, uint32 k) noexcept {
				return m.positions[m.triangles[t * 3u + k]];
			}

			/// UV et couleur d'un point, lus dans le triangle `t` (prolonges dehors).
			void Interpoler(const NkMaillage2D &m, uint32 t, const NkVec2f &p, NkVec2f &uv, uint32 &col) noexcept {
				const uint32 i0 = m.triangles[t * 3u], i1 = m.triangles[t * 3u + 1u], i2 = m.triangles[t * 3u + 2u];
				float32 l0 = 1.f / 3.f, l1 = 1.f / 3.f, l2 = 1.f / 3.f;
				Barycentre(m.positions[i0], m.positions[i1], m.positions[i2], p, l0, l1, l2);
				uv = m.uvs[i0] * l0 + m.uvs[i1] * l1 + m.uvs[i2] * l2;
				// La couleur ne se PROLONGE pas : dehors, les poids sont bornes a 0..1.
				const float32 c0 = Max(l0, 0.f), c1 = Max(l1, 0.f), c2 = Max(l2, 0.f);
				const float32 s = c0 + c1 + c2 > 1.0e-6f ? c0 + c1 + c2 : 1.f;
				col = Melanger(m.couleurs[i0], m.couleurs[i1], m.couleurs[i2], c0 / s, c1 / s, c2 / s);
			}

			/// Le triangle le plus proche de `p` (dedans : distance nulle).
			int32 TrianglePlusProche(const NkMaillage2D &m, const NkVec2f &p) noexcept {
				int32 meilleur = -1;
				float32 dMin = 1.0e30f;
				for (uint32 t = 0; t < m.nbTriangles; ++t) {
					const NkVec2f a = Pos(m, t, 0), b = Pos(m, t, 1), c = Pos(m, t, 2);
					float32 d = Min(NkDistanceSegment2D(p, a, b), Min(NkDistanceSegment2D(p, b, c), NkDistanceSegment2D(p, c, a)));
					if (Croix(a, b, p) >= 0.f && Croix(b, c, p) >= 0.f && Croix(c, a, p) >= 0.f) {
						d = 0.f;
					}
					if (d < dMin) {
						dMin = d;
						meilleur = static_cast<int32>(t);
					}
				}
				return meilleur;
			}

			// =================================================================
			// LA SOUDURE : un indice CANONIQUE par place (le premier sommet a y etre)
			// =================================================================
			void Canoniques(const NkMaillage2D &m, uint8 *canon) noexcept {
				for (uint32 i = 0; i < m.nbSommets; ++i) {
					canon[i] = static_cast<uint8>(i);
					for (uint32 j = 0; j < i; ++j) {
						if (canon[j] == j && MemePlace(m.positions[i], m.positions[j])) {
							canon[i] = static_cast<uint8>(j);
							break;
						}
					}
				}
			}

			// =================================================================
			// DELAUNAY (Bowyer-Watson). En double : les cercles circonscrits de
			// points presque alignes sont la ou un float32 se trompe.
			// =================================================================
			struct TriD {
					int32 a, b, c;
					float64 cx, cy, r2;
			};

			bool Cercle(const float64 *x, const float64 *y, int32 a, int32 b, int32 c, TriD &t) noexcept {
				const float64 ax = x[a], ay = y[a], bx = x[b], by = y[b], cx = x[c], cy = y[c];
				const float64 d = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
				if (d > -1.0e-18 && d < 1.0e-18) {
					return false;
				}
				const float64 a2 = ax * ax + ay * ay, b2 = bx * bx + by * by, c2 = cx * cx + cy * cy;
				t.cx = (a2 * (by - cy) + b2 * (cy - ay) + c2 * (ay - by)) / d;
				t.cy = (a2 * (cx - bx) + b2 * (ax - cx) + c2 * (bx - ax)) / d;
				t.r2 = (ax - t.cx) * (ax - t.cx) + (ay - t.cy) * (ay - t.cy);
				return true;
			}

			/// Triangule `n` points DISTINCTS ; rend les triangles (sens trigonometrique)
			/// dans `sortie` (3 indices par triangle).
			void Delaunay(const NkVec2f *p, uint32 n, NkVector<uint32> &sortie) noexcept {
				sortie.Clear();
				if (n < 3u) {
					return;
				}
				// Centre et echelle : les calculs se font pres de l'origine, a l'unite.
				float32 mnx = p[0].x, mny = p[0].y, mxx = p[0].x, mxy = p[0].y;
				for (uint32 i = 1; i < n; ++i) {
					mnx = Min(mnx, p[i].x);
					mny = Min(mny, p[i].y);
					mxx = Max(mxx, p[i].x);
					mxy = Max(mxy, p[i].y);
				}
				const float64 ox = (mnx + mxx) * 0.5, oy = (mny + mxy) * 0.5;
				const float64 ech = Max(Max(mxx - mnx, mxy - mny), 1.0e-6f);
				NkVector<float64> x, y;
				x.Resize(n + 3u);
				y.Resize(n + 3u);
				for (uint32 i = 0; i < n; ++i) {
					x[i] = (p[i].x - ox) / ech;
					y[i] = (p[i].y - oy) / ech;
				}
				// Le SUPER-TRIANGLE (trigonometrique), tres loin.
				x[n] = -40.0;
				y[n] = -30.0;
				x[n + 1] = 40.0;
				y[n + 1] = -30.0;
				x[n + 2] = 0.0;
				y[n + 2] = 50.0;
				NkVector<TriD> tris;
				TriD super{static_cast<int32>(n), static_cast<int32>(n + 1u), static_cast<int32>(n + 2u), 0.0, 0.0, 0.0};
				Cercle(x.Data(), y.Data(), super.a, super.b, super.c, super);
				tris.PushBack(super);
				NkVector<int32> bords; // paires (u, v)
				NkVector<TriD> garde;
				for (uint32 i = 0; i < n; ++i) {
					const float64 px = x[i], py = y[i];
					bords.Clear();
					garde.Clear();
					for (uint32 k = 0; k < tris.Size(); ++k) {
						const TriD &t = tris[k];
						const float64 dx = px - t.cx, dy = py - t.cy;
						if (dx * dx + dy * dy < t.r2 * (1.0 + 1.0e-10)) {
							// Mauvais : ses aretes vont au bord de la cavite (une arete vue
							// deux fois est interieure, elle s'annule).
							const int32 e[6] = {t.a, t.b, t.b, t.c, t.c, t.a};
							for (int32 q = 0; q < 3; ++q) {
								const int32 u = e[q * 2], v = e[q * 2 + 1];
								bool vue = false;
								for (uint32 r = 0; r < bords.Size(); r += 2u) {
									if (bords[r] == v && bords[r + 1u] == u) {
										bords.Erase(bords.Begin() + r, bords.Begin() + r + 2u);
										vue = true;
										break;
									}
								}
								if (!vue) {
									bords.PushBack(u);
									bords.PushBack(v);
								}
							}
						} else {
							garde.PushBack(t);
						}
					}
					tris = garde;
					for (uint32 r = 0; r < bords.Size(); r += 2u) {
						TriD t{bords[r], bords[r + 1u], static_cast<int32>(i), 0.0, 0.0, 0.0};
						if (Cercle(x.Data(), y.Data(), t.a, t.b, t.c, t)) {
							tris.PushBack(t);
						}
					}
				}
				for (uint32 k = 0; k < tris.Size(); ++k) {
					const TriD &t = tris[k];
					if (t.a >= static_cast<int32>(n) || t.b >= static_cast<int32>(n) || t.c >= static_cast<int32>(n)) {
						continue;
					}
					uint32 a = static_cast<uint32>(t.a), b = static_cast<uint32>(t.b), c = static_cast<uint32>(t.c);
					const float32 aire = Croix(p[a], p[b], p[c]);
					if (Abs(aire) < 1.0e-10f) {
						continue; // plat
					}
					if (aire < 0.f) {
						const uint32 s = b;
						b = c;
						c = s;
					}
					sortie.PushBack(a);
					sortie.PushBack(b);
					sortie.PushBack(c);
				}
			}

			/// Le cercle de (a, b, c) contient-il d (strictement) ? a, b, c trigonometriques.
			bool DansCercle(const NkVec2f &a, const NkVec2f &b, const NkVec2f &c, const NkVec2f &d) noexcept {
				const float64 adx = a.x - d.x, ady = a.y - d.y;
				const float64 bdx = b.x - d.x, bdy = b.y - d.y;
				const float64 cdx = c.x - d.x, cdy = c.y - d.y;
				const float64 det = (adx * adx + ady * ady) * (bdx * cdy - cdx * bdy) - (bdx * bdx + bdy * bdy) * (adx * cdy - cdx * ady) +
									(cdx * cdx + cdy * cdy) * (adx * bdy - bdx * ady);
				return det > 1.0e-14;
			}

			/// LAWSON : retourne les aretes interieures qui ne sont pas de Delaunay,
			/// ENTRE TRIANGLES D'UNE MEME PARTIE (une couture ne se retourne pas).
			void Ameliorer(NkMaillage2D &m) noexcept {
				for (uint32 tour = 0; tour < 4u * NK_MAILLAGE2D_TRIANGLES_MAX; ++tour) {
					bool change = false;
					for (uint32 t1 = 0; t1 < m.nbTriangles && !change; ++t1) {
						for (uint32 e = 0; e < 3u && !change; ++e) {
							const uint8 a = m.triangles[t1 * 3u + e], b = m.triangles[t1 * 3u + (e + 1u) % 3u];
							const uint8 o1 = m.triangles[t1 * 3u + (e + 2u) % 3u];
							for (uint32 t2 = t1 + 1u; t2 < m.nbTriangles; ++t2) {
								if (m.partieTriangle[t2] != m.partieTriangle[t1]) {
									continue;
								}
								for (uint32 f = 0; f < 3u; ++f) {
									if (m.triangles[t2 * 3u + f] != b || m.triangles[t2 * 3u + (f + 1u) % 3u] != a) {
										continue;
									}
									const uint8 o2 = m.triangles[t2 * 3u + (f + 2u) % 3u];
									const NkVec2f &pa = m.positions[a], &pb = m.positions[b];
									const NkVec2f &p1 = m.positions[o1], &p2 = m.positions[o2];
									// Le quadrilatere doit etre CONVEXE, sinon le retournement plie.
									if (!DansCercle(pa, pb, p1, p2) || Croix(p1, pa, p2) <= 1.0e-10f || Croix(p2, pb, p1) <= 1.0e-10f) {
										break;
									}
									m.triangles[t1 * 3u] = o1;
									m.triangles[t1 * 3u + 1u] = a;
									m.triangles[t1 * 3u + 2u] = o2;
									m.triangles[t2 * 3u] = o2;
									m.triangles[t2 * 3u + 1u] = b;
									m.triangles[t2 * 3u + 2u] = o1;
									change = true;
									break;
								}
								if (change) {
									break;
								}
							}
						}
					}
					if (!change) {
						return;
					}
				}
			}

			// =================================================================
			// LES CONTOURS
			// =================================================================
			/// Ramer-Douglas-Peucker sur une boucle FERMEE (garde le point 0 et le plus eloigne).
			void Simplifier(NkVector<NkVec2f> &pts, float32 tolerance) noexcept {
				const uint32 n = static_cast<uint32>(pts.Size());
				if (n < 4u || tolerance <= 0.f) {
					return;
				}
				// Le point le plus loin du premier : la boucle devient deux chemins.
				uint32 loin = 0;
				float32 dLoin = -1.f;
				for (uint32 i = 1; i < n; ++i) {
					const float32 d = Dist2(pts[0], pts[i]);
					if (d > dLoin) {
						dLoin = d;
						loin = i;
					}
				}
				NkVector<uint8> garder;
				garder.Resize(n);
				for (uint32 i = 0; i < n; ++i) {
					garder[i] = 0u;
				}
				garder[0] = 1u;
				garder[loin] = 1u;
				// Une pile de chemins [debut, fin] (indices modulo n).
				NkVector<uint32> pile;
				pile.PushBack(0u);
				pile.PushBack(loin);
				pile.PushBack(loin);
				pile.PushBack(n);
				while (!pile.Empty()) {
					const uint32 fin = pile.Back();
					pile.PopBack();
					const uint32 debut = pile.Back();
					pile.PopBack();
					if (fin <= debut + 1u) {
						continue;
					}
					const NkVec2f a = pts[debut % n], b = pts[fin % n];
					float32 dMax = -1.f;
					uint32 iMax = debut;
					for (uint32 i = debut + 1u; i < fin; ++i) {
						const float32 d = NkDistanceSegment2D(pts[i % n], a, b);
						if (d > dMax) {
							dMax = d;
							iMax = i;
						}
					}
					if (dMax > tolerance) {
						garder[iMax % n] = 1u;
						pile.PushBack(debut);
						pile.PushBack(iMax);
						pile.PushBack(iMax);
						pile.PushBack(fin);
					}
				}
				NkVector<NkVec2f> r;
				for (uint32 i = 0; i < n; ++i) {
					if (garder[i] != 0u) {
						r.PushBack(pts[i]);
					}
				}
				if (r.Size() >= 3u) {
					pts = r;
				}
			}

			/// Reechantillonne une boucle : aucun cote plus long que `h`, aucun point a
			/// moins de 0,35 h du precedent.
			void Reechantillonner(const NkVector<NkVec2f> &in, float32 h, NkVector<NkVec2f> &out) noexcept {
				out.Clear();
				const uint32 n = static_cast<uint32>(in.Size());
				for (uint32 i = 0; i < n; ++i) {
					const NkVec2f a = in[i], b = in[(i + 1u) % n];
					const float32 l = math::NkSqrt(Dist2(a, b));
					const int32 k = h > 0.f ? static_cast<int32>(l / h + 0.999f) : 1;
					for (int32 s = 0; s < (k < 1 ? 1 : k); ++s) {
						const float32 t = static_cast<float32>(s) / static_cast<float32>(k < 1 ? 1 : k);
						out.PushBack(a + (b - a) * t);
					}
				}
				// Les points trop serres (les escaliers d'un contour de pixels).
				NkVector<NkVec2f> r;
				for (uint32 i = 0; i < out.Size(); ++i) {
					if (r.Empty() || Dist2(r.Back(), out[i]) > (0.35f * h) * (0.35f * h)) {
						r.PushBack(out[i]);
					}
				}
				while (r.Size() > 3u && Dist2(r.Back(), r[0]) <= (0.35f * h) * (0.35f * h)) {
					r.PopBack();
				}
				out = r;
			}

			/// Boucles de BORD d'un ensemble de triangles (aretes vues une seule fois),
			/// sur les indices `idx` (canoniques ou non). Rend la plus grande boucle
			/// (aire positive), en indices.
			bool PlusGrandeBoucle(const NkMaillage2D &m, const uint8 *idx, int32 partie, NkVector<uint8> &boucle) noexcept {
				boucle.Clear();
				// Les aretes orientees de bord : (u -> v) dont (v -> u) n'existe pas.
				NkVector<uint8> du, dv;
				for (uint32 t = 0; t < m.nbTriangles; ++t) {
					if (partie >= 0 && m.partieTriangle[t] != static_cast<uint8>(partie)) {
						continue;
					}
					for (uint32 e = 0; e < 3u; ++e) {
						const uint8 u = idx[m.triangles[t * 3u + e]], v = idx[m.triangles[t * 3u + (e + 1u) % 3u]];
						bool inverse = false;
						for (uint32 k = 0; k < du.Size(); ++k) {
							if (du[k] == v && dv[k] == u) {
								du.Erase(du.Begin() + k);
								dv.Erase(dv.Begin() + k);
								inverse = true;
								break;
							}
						}
						if (!inverse) {
							du.PushBack(u);
							dv.PushBack(v);
						}
					}
				}
				NkVector<uint8> pris;
				pris.Resize(du.Size());
				for (uint32 k = 0; k < pris.Size(); ++k) {
					pris[k] = 0u;
				}
				float32 meilleure = 0.f;
				for (uint32 k0 = 0; k0 < du.Size(); ++k0) {
					if (pris[k0] != 0u) {
						continue;
					}
					NkVector<uint8> b;
					uint32 k = k0;
					for (uint32 garde = 0; garde <= du.Size(); ++garde) {
						pris[k] = 1u;
						b.PushBack(du[k]);
						const uint8 suivant = dv[k];
						if (suivant == du[k0]) {
							break;
						}
						uint32 trouve = 0xFFFFFFFFu;
						for (uint32 q = 0; q < du.Size(); ++q) {
							if (pris[q] == 0u && du[q] == suivant) {
								trouve = q;
								break;
							}
						}
						if (trouve == 0xFFFFFFFFu) {
							break;
						}
						k = trouve;
					}
					if (b.Size() < 3u) {
						continue;
					}
					float32 aire = 0.f;
					for (uint32 i = 0; i < b.Size(); ++i) {
						const NkVec2f &p = m.positions[b[i]], &q = m.positions[b[(i + 1u) % b.Size()]];
						aire += p.x * q.y - q.x * p.y;
					}
					if (aire * 0.5f > meilleure) {
						meilleure = aire * 0.5f;
						boucle = b;
					}
				}
				return boucle.Size() >= 3u;
			}

			/// Les noeuds et indices d'un decoupage en oreilles de 128 points.
			struct NkOreilles {
					detail::NkEarcutNode<float32> noeuds[NK_MAILLAGE2D_SOMMETS_MAX];
					uint32 idx[(NK_MAILLAGE2D_SOMMETS_MAX - 2u) * 3u];
			};

			/// Le coeur de la fabrication : un contour (trigonometrique, distinct) ->
			/// sommets + triangles de la partie 0. Les UV sont laisses a l'appelant.
			bool Remplir(NkMaillage2D &m, NkVector<NkVec2f> contour, const NkOptionsMaillage2D &o) noexcept {
				if (contour.Size() < 3u) {
					return false;
				}
				if (NkAireSignee2D(contour.Data(), static_cast<uint32>(contour.Size())) < 0.f) {
					for (uint32 i = 0, j = static_cast<uint32>(contour.Size()) - 1u; i < j; ++i, --j) {
						const NkVec2f s = contour[i];
						contour[i] = contour[j];
						contour[j] = s;
					}
				}
				float32 mnx = contour[0].x, mny = contour[0].y, mxx = mnx, mxy = mny;
				for (uint32 i = 1; i < contour.Size(); ++i) {
					mnx = Min(mnx, contour[i].x);
					mny = Min(mny, contour[i].y);
					mxx = Max(mxx, contour[i].x);
					mxy = Max(mxy, contour[i].y);
				}
				const float32 grand = Max(mxx - mnx, mxy - mny);
				if (grand <= 1.0e-6f) {
					return false;
				}
				const int32 densite = o.densite < 1 ? 1 : (o.densite > 24 ? 24 : o.densite);
				float32 h = grand / static_cast<float32>(densite);
				NkVector<NkVec2f> bord, pts;
				// La densite demandee, ou la plus proche qui tient dans 128 sommets et
				// 240 triangles : on espace jusqu'a ce que ca tienne.
				for (int32 essai = 0; essai < 40; ++essai, h *= 1.15f) {
					Reechantillonner(contour, h, bord);
					if (bord.Size() < 3u) {
						continue;
					}
					pts = bord;
					if (o.interieur) {
						// Une grille en QUINCONCE (des triangles presque equilateraux).
						const float32 dy = h * 0.8660254f;
						int32 rang = 0;
						for (float32 y = mny + dy * 0.5f; y < mxy; y += dy, ++rang) {
							for (float32 x = mnx + ((rang & 1) ? h : h * 0.5f); x < mxx; x += h) {
								const NkVec2f q(x, y);
								if (NkDistanceContour2D(bord.Data(), static_cast<uint32>(bord.Size()), true, q) < -0.45f * h) {
									pts.PushBack(q);
								}
							}
						}
					}
					if (pts.Size() <= NK_MAILLAGE2D_SOMMETS_MAX && pts.Size() * 2u <= NK_MAILLAGE2D_TRIANGLES_MAX + bord.Size()) {
						break;
					}
				}
				if (pts.Size() < 3u || pts.Size() > NK_MAILLAGE2D_SOMMETS_MAX) {
					return false;
				}
				NkVector<uint32> tri;
				Delaunay(pts.Data(), static_cast<uint32>(pts.Size()), tri);
				// Seuls les triangles DEDANS (leur centre dans le contour).
				NkVector<uint32> garde;
				float32 aireGardee = 0.f;
				for (uint32 k = 0; k + 2u < tri.Size(); k += 3u) {
					const NkVec2f a = pts[tri[k]], b = pts[tri[k + 1u]], c = pts[tri[k + 2u]];
					const NkVec2f g = (a + b + c) * (1.f / 3.f);
					if (NkDistanceContour2D(bord.Data(), static_cast<uint32>(bord.Size()), true, g) < 0.f) {
						garde.PushBack(tri[k]);
						garde.PushBack(tri[k + 1u]);
						garde.PushBack(tri[k + 2u]);
						aireGardee += Croix(a, b, c) * 0.5f;
					}
				}
				// ⚠️ DELAUNAY NE RESPECTE PAS FORCEMENT LE CONTOUR (une concavite
				// tres fine peut etre enjambee). Si l'aire couverte s'ecarte de celle
				// du contour, on retombe sur le decoupage en OREILLES du contour seul :
				// moins souple, mais exact.
				const float32 aireContour = NkAireSignee2D(bord.Data(), static_cast<uint32>(bord.Size()));
				if (Abs(aireGardee - aireContour) > 0.02f * Abs(aireContour) || garde.Size() / 3u > NK_MAILLAGE2D_TRIANGLES_MAX) {
					pts = bord;
					garde.Clear();
					NkOreilles *ore = memory::NkGetDefaultAllocator().New<NkOreilles>();
					if (ore == nullptr) {
						return false;
					}
					const uint32 nb = NkEarcutVers<float32>(pts.Data(), static_cast<uint32>(pts.Size()), ore->noeuds,
															NK_MAILLAGE2D_SOMMETS_MAX, ore->idx, (NK_MAILLAGE2D_SOMMETS_MAX - 2u) * 3u);
					for (uint32 k = 0; k < nb * 3u; k += 3u) {
						uint32 a = ore->idx[k], b = ore->idx[k + 1u], c = ore->idx[k + 2u];
						if (Croix(pts[a], pts[b], pts[c]) < 0.f) {
							const uint32 s = b;
							b = c;
							c = s;
						}
						garde.PushBack(a);
						garde.PushBack(b);
						garde.PushBack(c);
					}
					memory::NkGetDefaultAllocator().Delete(ore);
				}
				if (garde.Size() < 3u || garde.Size() / 3u > NK_MAILLAGE2D_TRIANGLES_MAX) {
					return false;
				}
				NkMaillageVider2D(m);
				m.nbSommets = static_cast<uint8>(pts.Size());
				for (uint32 i = 0; i < pts.Size(); ++i) {
					m.positions[i] = pts[i];
					m.couleurs[i] = 0xFFFFFFFFu;
					m.partieSommet[i] = 0u;
					SansOs(m, i);
				}
				m.nbTriangles = static_cast<uint8>(garde.Size() / 3u);
				for (uint32 k = 0; k < garde.Size(); ++k) {
					m.triangles[k] = static_cast<uint8>(garde[k]);
				}
				m.nbParties = 1u;
				std::strcpy(m.parties[0].nom, "Corps");
				Ameliorer(m);
				NkMaillageRefaireCoutures2D(m); // les sommets orphelins (points sans triangle) partent
				return m.nbTriangles > 0u;
			}

			/// UV : la boite des positions -> 0..1 (v vers le bas, comme une image).
			void UVBoite(NkMaillage2D &m) noexcept {
				if (m.nbSommets == 0u) {
					return;
				}
				float32 mnx = m.positions[0].x, mny = m.positions[0].y, mxx = mnx, mxy = mny;
				for (uint32 i = 1; i < m.nbSommets; ++i) {
					mnx = Min(mnx, m.positions[i].x);
					mny = Min(mny, m.positions[i].y);
					mxx = Max(mxx, m.positions[i].x);
					mxy = Max(mxy, m.positions[i].y);
				}
				const float32 w = Max(mxx - mnx, 1.0e-6f), h = Max(mxy - mny, 1.0e-6f);
				for (uint32 i = 0; i < m.nbSommets; ++i) {
					m.uvs[i] = NkVec2f((m.positions[i].x - mnx) / w, 1.f - (m.positions[i].y - mny) / h);
				}
			}

			/// Le contour de l'ALPHA d'une region d'image, en CELLULES (coins de
			/// pixels), par suivi de fissure sur la plus grande ile (8-connexe).
			/// `cellules` : la region reduite (un octet par cellule, 1 = opaque).
			bool ContourAlpha(const NkVector<uint8> &cellules, int32 cw, int32 ch, NkVector<NkVec2f> &sortie) noexcept {
				sortie.Clear();
				// 1. Les iles, 8-connexes : la plus grande gagne (une poussiere de
				//    pixels ne fait pas le contour d'un personnage).
				NkVector<int32> ile;
				ile.Resize(static_cast<uint32>(cw * ch));
				for (uint32 i = 0; i < ile.Size(); ++i) {
					ile[i] = -1;
				}
				int32 meilleure = -1, tailleMax = 0, numero = 0;
				NkVector<int32> pile;
				for (int32 y = 0; y < ch; ++y) {
					for (int32 x = 0; x < cw; ++x) {
						const int32 i0 = y * cw + x;
						if (cellules[static_cast<uint32>(i0)] == 0u || ile[static_cast<uint32>(i0)] >= 0) {
							continue;
						}
						int32 taille = 0;
						pile.Clear();
						pile.PushBack(i0);
						ile[static_cast<uint32>(i0)] = numero;
						while (!pile.Empty()) {
							const int32 i = pile.Back();
							pile.PopBack();
							++taille;
							const int32 px = i % cw, py = i / cw;
							for (int32 dy = -1; dy <= 1; ++dy) {
								for (int32 dx = -1; dx <= 1; ++dx) {
									const int32 qx = px + dx, qy = py + dy;
									if (qx < 0 || qy < 0 || qx >= cw || qy >= ch) {
										continue;
									}
									const int32 q = qy * cw + qx;
									if (cellules[static_cast<uint32>(q)] != 0u && ile[static_cast<uint32>(q)] < 0) {
										ile[static_cast<uint32>(q)] = numero;
										pile.PushBack(q);
									}
								}
							}
						}
						if (taille > tailleMax) {
							tailleMax = taille;
							meilleure = numero;
						}
						++numero;
					}
				}
				if (meilleure < 0) {
					return false;
				}
				auto Dedans = [&](int32 x, int32 y) {
					return x >= 0 && y >= 0 && x < cw && y < ch && ile[static_cast<uint32>(y * cw + x)] == meilleure;
				};
				// 2. Le depart : la premiere cellule de l'ile (de haut en bas, de gauche a
				//    droite) ; son bord du HAUT, parcouru vers la droite (dedans a droite,
				//    en coordonnees d'image ou y descend).
				int32 sx = -1, sy = -1;
				for (int32 y = 0; y < ch && sx < 0; ++y) {
					for (int32 x = 0; x < cw; ++x) {
						if (Dedans(x, y)) {
							sx = x;
							sy = y;
							break;
						}
					}
				}
				int32 cx = sx, cy = sy, dx = 1, dy = 0;
				const int32 limite = 4 * (cw + 1) * (ch + 1);
				for (int32 pas = 0; pas < limite; ++pas) {
					// Un sommet a chaque changement de direction.
					cx += dx;
					cy += dy;
					// Les deux cellules DEVANT le coin : a gauche (A) et a droite (B).
					const int32 lx = dy, ly = -dx;	 // la gauche de (dx, dy), y vers le bas
					const int32 rx = -dy, ry = dx; // la droite
					auto Cellule = [&](int32 ox, int32 oy) {
						// Le centre (c + 0.5 d + 0.5 o), arrondi vers le bas.
						const float32 fx = static_cast<float32>(cx) + 0.5f * static_cast<float32>(dx) + 0.5f * static_cast<float32>(ox);
						const float32 fy = static_cast<float32>(cy) + 0.5f * static_cast<float32>(dy) + 0.5f * static_cast<float32>(oy);
						return Dedans(static_cast<int32>(fx < 0.f ? fx - 1.f : fx), static_cast<int32>(fy < 0.f ? fy - 1.f : fy));
					};
					const bool a = Cellule(lx, ly), b = Cellule(rx, ry);
					int32 ndx = dx, ndy = dy;
					if (a) {
						ndx = lx;
						ndy = ly; // a gauche
					} else if (!b) {
						ndx = rx;
						ndy = ry; // a droite
					}
					if (ndx != dx || ndy != dy) {
						sortie.PushBack(NkVec2f(static_cast<float32>(cx), static_cast<float32>(cy)));
					}
					dx = ndx;
					dy = ndy;
					if (cx == sx && cy == sy && dx == 1 && dy == 0) {
						break;
					}
				}
				// Le coin de depart est un changement de direction (on y arrive par le
				// haut ou la gauche) : il est deja dans la liste s'il le faut.
				return sortie.Size() >= 3u;
			}
		} // namespace

		// =====================================================================
		// LECTURE
		// =====================================================================
		uint32 NkCouleurPartie2D(const NkMaillage2D &m, uint32 k) noexcept {
			static const uint32 kPalette[NK_MAILLAGE2D_PARTIES_MAX] = {0x4C9BE8FFu, 0xF08C3CFFu, 0x5CC46BFFu, 0xD65CC9FFu,
																	   0xE8D24CFFu, 0x4CD6D0FFu, 0xE8505EFFu, 0x9B7CE8FFu};
			if (k < NK_MAILLAGE2D_PARTIES_MAX && m.parties[k].couleur != 0u) {
				return m.parties[k].couleur;
			}
			return kPalette[k % NK_MAILLAGE2D_PARTIES_MAX];
		}

		float32 NkAireTriangle2D(const NkMaillage2D &m, uint32 t) noexcept {
			if (t >= m.nbTriangles) {
				return 0.f;
			}
			return 0.5f * Croix(Pos(m, t, 0), Pos(m, t, 1), Pos(m, t, 2));
		}

		uint32 NkTrianglesRetournes2D(const NkMaillage2D &m) noexcept {
			uint32 n = 0;
			for (uint32 t = 0; t < m.nbTriangles; ++t) {
				n += NkAireTriangle2D(m, t) <= 1.0e-9f ? 1u : 0u;
			}
			return n;
		}

		bool NkMaillageCoherent2D(const NkMaillage2D &m, const char **pourquoi) noexcept {
			auto Faux = [pourquoi](const char *r) {
				if (pourquoi != nullptr) {
					*pourquoi = r;
				}
				return false;
			};
			if (m.nbSommets > NK_MAILLAGE2D_SOMMETS_MAX || m.nbTriangles > NK_MAILLAGE2D_TRIANGLES_MAX ||
				m.nbParties > NK_MAILLAGE2D_PARTIES_MAX || m.nbLiens > NK_MAILLAGE2D_LIENS_MAX) {
				return Faux("compte hors capacite");
			}
			if (m.nbTriangles > 0u && m.nbParties == 0u) {
				return Faux("des triangles sans partie");
			}
			for (uint32 k = 0; k < m.nbTriangles * 3u; ++k) {
				if (m.triangles[k] >= m.nbSommets) {
					return Faux("un indice de triangle hors des sommets");
				}
			}
			for (uint32 t = 0; t < m.nbTriangles; ++t) {
				if (m.partieTriangle[t] >= m.nbParties) {
					return Faux("un triangle dans une partie inexistante");
				}
				for (uint32 k = 0; k < 3u; ++k) {
					if (m.partieSommet[m.triangles[t * 3u + k]] != m.partieTriangle[t]) {
						return Faux("un sommet partage par deux parties (couture non dedoublee)");
					}
				}
			}
			for (uint32 l = 0; l < m.nbLiens; ++l) {
				if (m.liens[l].a >= m.nbParties || m.liens[l].b >= m.nbParties || m.liens[l].a == m.liens[l].b) {
					return Faux("un lien vers une partie inexistante");
				}
			}
			return true;
		}

		NkVec2f NkDemiBoiteMaillage2D(const NkMaillage2D &m) noexcept {
			NkVec2f d(0.f, 0.f);
			for (uint32 i = 0; i < m.nbSommets; ++i) {
				d.x = Max(d.x, Abs(m.positions[i].x));
				d.y = Max(d.y, Abs(m.positions[i].y));
			}
			return d;
		}

		int32 NkTriangleSous2D(const NkMaillage2D &m, const NkVec2f &p) noexcept {
			for (uint32 t = 0; t < m.nbTriangles; ++t) {
				const NkVec2f a = Pos(m, t, 0), b = Pos(m, t, 1), c = Pos(m, t, 2);
				if (Croix(a, b, p) >= 0.f && Croix(b, c, p) >= 0.f && Croix(c, a, p) >= 0.f) {
					return static_cast<int32>(t);
				}
			}
			return -1;
		}

		float32 NkDistanceMaillageLocal2D(const NkMaillage2D &m, const NkVec2f &p) noexcept {
			float32 d = 1.0e9f;
			for (uint32 t = 0; t < m.nbTriangles; ++t) {
				for (uint32 e = 0; e < 3u; ++e) {
					d = Min(d, NkDistanceSegment2D(p, Pos(m, t, e), Pos(m, t, (e + 1u) % 3u)));
				}
			}
			return NkTriangleSous2D(m, p) >= 0 ? -d : d;
		}

		NkVec2f NkCentrePartie2D(const NkMaillage2D &m, uint32 k) noexcept {
			NkVec2f c(0.f, 0.f);
			float32 aire = 0.f;
			for (uint32 t = 0; t < m.nbTriangles; ++t) {
				if (m.partieTriangle[t] != k) {
					continue;
				}
				const float32 a = Max(NkAireTriangle2D(m, t), 0.f);
				c = c + (Pos(m, t, 0) + Pos(m, t, 1) + Pos(m, t, 2)) * (a / 3.f);
				aire += a;
			}
			return aire > 1.0e-12f ? c * (1.f / aire) : c;
		}

		uint32 NkContourPartie2D(const NkMaillage2D &m, uint32 k, NkVec2f *sortie, uint32 capacite, uint8 *indices) noexcept {
			uint8 idx[NK_MAILLAGE2D_SOMMETS_MAX];
			for (uint32 i = 0; i < NK_MAILLAGE2D_SOMMETS_MAX; ++i) {
				idx[i] = static_cast<uint8>(i);
			}
			NkVector<uint8> b;
			if (!PlusGrandeBoucle(m, idx, static_cast<int32>(k), b)) {
				return 0u;
			}
			uint32 n = 0;
			for (uint32 i = 0; i < b.Size() && n < capacite; ++i, ++n) {
				sortie[n] = m.positions[b[i]];
				if (indices != nullptr) {
					indices[n] = b[i];
				}
			}
			return n;
		}

		uint32 NkContourMaillage2D(const NkMaillage2D &m, NkVec2f *sortie, uint32 capacite) noexcept {
			uint8 canon[NK_MAILLAGE2D_SOMMETS_MAX];
			Canoniques(m, canon);
			NkVector<uint8> b;
			if (!PlusGrandeBoucle(m, canon, -1, b)) {
				return 0u;
			}
			uint32 n = 0;
			for (uint32 i = 0; i < b.Size() && n < capacite; ++i) {
				sortie[n++] = m.positions[b[i]];
			}
			return n;
		}

		uint32 NkDoublesSommet2D(const NkMaillage2D &m, uint32 s, uint8 *sortie, uint32 capacite) noexcept {
			uint32 n = 0;
			if (s >= m.nbSommets) {
				return 0u;
			}
			for (uint32 i = 0; i < m.nbSommets && n < capacite; ++i) {
				if (MemePlace(m.positions[i], m.positions[s])) {
					sortie[n++] = static_cast<uint8>(i);
				}
			}
			return n;
		}

		// =====================================================================
		// FABRICATION
		// =====================================================================
		void NkMaillageVider2D(NkMaillage2D &m) noexcept {
			const uint32 tex = m.texId, teinte = m.teinte;
			const int32 couche = m.couche;
			const bool visible = m.visible, touchent = m.partiesSeTouchent;
			char source[sizeof(m.source)];
			std::memcpy(source, m.source, sizeof(source));
			m = NkMaillage2D();
			m.texId = tex;
			m.teinte = teinte;
			m.couche = couche;
			m.visible = visible;
			m.partiesSeTouchent = touchent;
			std::memcpy(m.source, source, sizeof(source));
			NkMaillageSansOs2D(m);
		}

		bool NkMaillageRectangle2D(NkMaillage2D &m, const NkVec2f &taille, const NkVec2f &pivot, int32 nx, int32 ny) noexcept {
			nx = nx < 1 ? 1 : nx;
			ny = ny < 1 ? 1 : ny;
			while ((nx + 1) * (ny + 1) > static_cast<int32>(NK_MAILLAGE2D_SOMMETS_MAX) ||
				   2 * nx * ny > static_cast<int32>(NK_MAILLAGE2D_TRIANGLES_MAX)) {
				if (nx >= ny) {
					--nx;
				} else {
					--ny;
				}
			}
			NkMaillageVider2D(m);
			const float32 ox = -pivot.x * taille.x, oy = -pivot.y * taille.y;
			for (int32 j = 0; j <= ny; ++j) {
				for (int32 i = 0; i <= nx; ++i) {
					const uint32 s = static_cast<uint32>(j * (nx + 1) + i);
					const float32 u = static_cast<float32>(i) / static_cast<float32>(nx);
					const float32 v = static_cast<float32>(j) / static_cast<float32>(ny);
					m.positions[s] = NkVec2f(ox + u * taille.x, oy + v * taille.y);
					m.uvs[s] = NkVec2f(u, 1.f - v);
					m.couleurs[s] = 0xFFFFFFFFu;
					m.partieSommet[s] = 0u;
				}
			}
			m.nbSommets = static_cast<uint8>((nx + 1) * (ny + 1));
			uint32 t = 0;
			for (int32 j = 0; j < ny; ++j) {
				for (int32 i = 0; i < nx; ++i) {
					const uint8 a = static_cast<uint8>(j * (nx + 1) + i), b = static_cast<uint8>(a + 1u);
					const uint8 c = static_cast<uint8>(a + nx + 1), d = static_cast<uint8>(c + 1u);
					const uint8 tri[6] = {a, b, d, a, d, c};
					for (uint32 k = 0; k < 6u; ++k) {
						m.triangles[t * 3u + k] = tri[k];
					}
					m.partieTriangle[t] = 0u;
					m.partieTriangle[t + 1u] = 0u;
					t += 2u;
				}
			}
			m.nbTriangles = static_cast<uint8>(t);
			m.nbParties = 1u;
			std::strcpy(m.parties[0].nom, "Corps");
			return true;
		}

		bool NkMaillageDepuisContour2D(NkMaillage2D &m, const NkVec2f *contour, uint32 n, const NkOptionsMaillage2D &o) noexcept {
			if (contour == nullptr || n < 3u) {
				return false;
			}
			NkVector<NkVec2f> c;
			for (uint32 i = 0; i < n; ++i) {
				if (c.Empty() || !MemePlace(c.Back(), contour[i])) {
					c.PushBack(contour[i]);
				}
			}
			if (!Remplir(m, c, o)) {
				return false;
			}
			UVBoite(m);
			return true;
		}

		void NkMaillageUVDepuisSprite2D(NkMaillage2D &m, const NkSprite2D &s) noexcept {
			const float32 W = Abs(s.taille.x) > 1.0e-6f ? s.taille.x : 1.f;
			const float32 H = Abs(s.taille.y) > 1.0e-6f ? s.taille.y : 1.f;
			const float32 ox = -s.pivot.x * W, oy = -s.pivot.y * H;
			for (uint32 i = 0; i < m.nbSommets; ++i) {
				// L'inverse du dessin d'un sprite (NkDessinerScene) : (uv0.x, uv0.y) est
				// le coin HAUT-gauche, v descend comme les lignes de l'image.
				const float32 fx = (m.positions[i].x - ox) / W;
				const float32 fy = 1.f - (m.positions[i].y - oy) / H;
				m.uvs[i] = NkVec2f(s.uv0.x + fx * (s.uv1.x - s.uv0.x), s.uv0.y + fy * (s.uv1.y - s.uv0.y));
			}
		}

		bool NkMaillageDepuisSprite2D(NkMaillage2D &m, const NkSprite2D &s, const uint8 *rgba, int32 w, int32 h,
									  const NkOptionsMaillage2D &o) noexcept {
			const uint32 tex = s.texId;
			const float32 W = s.taille.x, H = s.taille.y;
			const float32 ox = -s.pivot.x * W, oy = -s.pivot.y * H;
			NkVector<NkVec2f> contour;
			if (rgba != nullptr && w > 0 && h > 0) {
				// La region UV du sprite, en pixels de l'image.
				const int32 x0 = static_cast<int32>(math::NkClamp(Min(s.uv0.x, s.uv1.x), 0.f, 1.f) * static_cast<float32>(w));
				const int32 x1 = static_cast<int32>(math::NkClamp(Max(s.uv0.x, s.uv1.x), 0.f, 1.f) * static_cast<float32>(w));
				const int32 y0 = static_cast<int32>(math::NkClamp(Min(s.uv0.y, s.uv1.y), 0.f, 1.f) * static_cast<float32>(h));
				const int32 y1 = static_cast<int32>(math::NkClamp(Max(s.uv0.y, s.uv1.y), 0.f, 1.f) * static_cast<float32>(h));
				if (x1 <= x0 || y1 <= y0) {
					return false;
				}
				// Une grande image est lue par CELLULES de f x f pixels (au plus ~192
				// de cote) ; une cellule est opaque si UN de ses pixels l'est (le
				// contour englobe, il ne rogne jamais l'image).
				const int32 rw = x1 - x0, rh = y1 - y0;
				const int32 f = (rw > rh ? rw : rh) > 192 ? ((rw > rh ? rw : rh) + 191) / 192 : 1;
				const int32 cw = (rw + f - 1) / f, ch = (rh + f - 1) / f;
				NkVector<uint8> cellules;
				cellules.Resize(static_cast<uint32>(cw * ch));
				int32 opaques = 0;
				for (int32 cy = 0; cy < ch; ++cy) {
					for (int32 cx = 0; cx < cw; ++cx) {
						uint8 v = 0u;
						for (int32 y = y0 + cy * f; y < y0 + (cy + 1) * f && y < y1 && v == 0u; ++y) {
							for (int32 x = x0 + cx * f; x < x0 + (cx + 1) * f && x < x1; ++x) {
								if (rgba[(static_cast<usize>(y) * static_cast<usize>(w) + static_cast<usize>(x)) * 4u + 3u] >= o.seuilAlpha) {
									v = 1u;
									break;
								}
							}
						}
						cellules[static_cast<uint32>(cy * cw + cx)] = v;
						opaques += v;
					}
				}
				if (opaques == 0) {
					return false; // rien d'opaque : pas de maillage a faire
				}
				NkVector<NkVec2f> brut;
				if (!ContourAlpha(cellules, cw, ch, brut)) {
					return false;
				}
				Simplifier(brut, o.tolerancePx / static_cast<float32>(f));
				// Cellules -> pixels -> repere du sprite (y monte en monde).
				for (uint32 i = 0; i < brut.Size(); ++i) {
					const float32 px = Min(static_cast<float32>(brut[i].x * static_cast<float32>(f)), static_cast<float32>(rw));
					const float32 py = Min(static_cast<float32>(brut[i].y * static_cast<float32>(f)), static_cast<float32>(rh));
					contour.PushBack(NkVec2f(ox + W * px / static_cast<float32>(rw), oy + H * (1.f - py / static_cast<float32>(rh))));
				}
			} else {
				contour.PushBack(NkVec2f(ox, oy));
				contour.PushBack(NkVec2f(ox + W, oy));
				contour.PushBack(NkVec2f(ox + W, oy + H));
				contour.PushBack(NkVec2f(ox, oy + H));
			}
			if (!Remplir(m, contour, o)) {
				return false;
			}
			m.texId = tex;
			for (uint32 i = 0; i < m.nbSommets; ++i) {
				m.couleurs[i] = s.couleur;
			}
			m.couche = s.couche;
			m.visible = true;
			NkMaillageUVDepuisSprite2D(m, s);
			return true;
		}

		bool NkMaillageDepuisForme2D(NkMaillage2D &m, const NkRenduForme2D &f, const NkOptionsMaillage2D &o) noexcept {
			NkVec2f pts[NK_FORME_CONTOUR_MAX];
			bool ferme = true;
			const uint32 n = NkContourForme2D(f, pts, NK_FORME_CONTOUR_MAX, ferme);
			if (!ferme || n < 3u) {
				return false; // une ligne n'a pas d'interieur
			}
			if (!NkMaillageDepuisContour2D(m, pts, n, o)) {
				return false;
			}
			// La couleur de remplissage, opacite comprise (0xRRGGBBAA).
			const float32 op = math::NkClamp(f.opacite, 0.f, 1.f);
			const uint32 a = static_cast<uint32>(static_cast<float32>(f.remplissage & 0xFFu) * op + 0.5f);
			const uint32 col = (f.remplissage & 0xFFFFFF00u) | (a > 255u ? 255u : a);
			for (uint32 i = 0; i < m.nbSommets; ++i) {
				m.couleurs[i] = col;
			}
			m.texId = 0u;
			m.couche = f.couche;
			return true;
		}

		// =====================================================================
		// EDITION
		// =====================================================================
		void NkMaillageSansOs2D(NkMaillage2D &m) noexcept {
			for (uint32 s = 0; s < NK_MAILLAGE2D_SOMMETS_MAX; ++s) {
				SansOs(m, s);
			}
		}

		void NkMaillageMettreAEchelle2D(NkMaillage2D &m, const NkVec2f &f) noexcept {
			for (uint32 i = 0; i < m.nbSommets; ++i) {
				m.positions[i] = NkVec2f(m.positions[i].x * f.x, m.positions[i].y * f.y);
			}
			// Un miroir (un facteur negatif) retourne les triangles : on les remet
			// dans le sens trigonometrique.
			if (f.x * f.y < 0.f) {
				for (uint32 t = 0; t < m.nbTriangles; ++t) {
					const uint8 s = m.triangles[t * 3u + 1u];
					m.triangles[t * 3u + 1u] = m.triangles[t * 3u + 2u];
					m.triangles[t * 3u + 2u] = s;
				}
			}
		}

		void NkMaillageRefaireCoutures2D(NkMaillage2D &m) noexcept {
			uint8 canon[NK_MAILLAGE2D_SOMMETS_MAX];
			Canoniques(m, canon);
			// Les paires (place, partie) utilisees par les triangles.
			bool utilise[NK_MAILLAGE2D_SOMMETS_MAX][NK_MAILLAGE2D_PARTIES_MAX] = {};
			for (uint32 t = 0; t < m.nbTriangles; ++t) {
				for (uint32 k = 0; k < 3u; ++k) {
					const uint8 p = m.partieTriangle[t] < NK_MAILLAGE2D_PARTIES_MAX ? m.partieTriangle[t] : 0u;
					utilise[canon[m.triangles[t * 3u + k]]][p] = true;
				}
			}
			// Le compte d'apres : refuse s'il depasse (rien ne change alors).
			uint32 total = 0;
			for (uint32 i = 0; i < m.nbSommets; ++i) {
				if (canon[i] != i) {
					continue;
				}
				uint32 parts = 0;
				for (uint32 p = 0; p < NK_MAILLAGE2D_PARTIES_MAX; ++p) {
					parts += utilise[i][p] ? 1u : 0u;
				}
				total += m.nbTriangles == 0u ? 1u : parts;
			}
			if (total > NK_MAILLAGE2D_SOMMETS_MAX) {
				return;
			}
			NkMaillage2D *pn = memory::NkGetDefaultAllocator().New<NkMaillage2D>();
			if (pn == nullptr) {
				return;
			}
			NkMaillage2D &n = *pn;
			n = m;
			uint8 nouveau[NK_MAILLAGE2D_SOMMETS_MAX][NK_MAILLAGE2D_PARTIES_MAX];
			std::memset(nouveau, 0xFF, sizeof(nouveau));
			uint32 c = 0;
			for (uint32 i = 0; i < m.nbSommets; ++i) {
				if (canon[i] != i) {
					continue;
				}
				if (m.nbTriangles == 0u) {
					// Pas encore de triangle : les sommets restent tous (ils attendent).
					CopierSommet(n, c, m, i);
					n.partieSommet[c] = 0u;
					nouveau[i][0] = static_cast<uint8>(c++);
					continue;
				}
				for (uint32 p = 0; p < NK_MAILLAGE2D_PARTIES_MAX; ++p) {
					if (!utilise[i][p]) {
						continue;
					}
					// Les attributs du double qui etait DEJA dans cette partie, s'il existe.
					uint32 src = i;
					for (uint32 j = i; j < m.nbSommets; ++j) {
						if (canon[j] == i && m.partieSommet[j] == p) {
							src = j;
							break;
						}
					}
					CopierSommet(n, c, m, src);
					n.partieSommet[c] = static_cast<uint8>(p);
					nouveau[i][p] = static_cast<uint8>(c++);
				}
			}
			for (uint32 s = c; s < NK_MAILLAGE2D_SOMMETS_MAX; ++s) {
				n.positions[s] = NkVec2f(0.f, 0.f);
				n.uvs[s] = NkVec2f(0.f, 0.f);
				n.couleurs[s] = 0u;
				n.partieSommet[s] = 0u;
				SansOs(n, s);
			}
			n.nbSommets = static_cast<uint8>(c);
			for (uint32 t = 0; t < m.nbTriangles; ++t) {
				const uint8 p = m.partieTriangle[t] < NK_MAILLAGE2D_PARTIES_MAX ? m.partieTriangle[t] : 0u;
				for (uint32 k = 0; k < 3u; ++k) {
					n.triangles[t * 3u + k] = nouveau[canon[m.triangles[t * 3u + k]]][p];
				}
			}
			m = n;
			memory::NkGetDefaultAllocator().Delete(pn);
		}

		bool NkMaillageTrianguler2D(NkMaillage2D &m, bool garderForme) noexcept {
			if (m.nbSommets < 3u) {
				return false;
			}
			NkMaillage2D *pa = memory::NkGetDefaultAllocator().New<NkMaillage2D>();
			if (pa == nullptr) {
				return false;
			}
			NkMaillage2D &ancien = *pa;
			ancien = m;
			// Les places DISTINCTES (une couture n'est qu'un point pour Delaunay).
			uint8 canon[NK_MAILLAGE2D_SOMMETS_MAX];
			Canoniques(m, canon);
			NkVector<NkVec2f> pts;
			NkVector<uint8> deQui; // point -> sommet canonique
			for (uint32 i = 0; i < m.nbSommets; ++i) {
				if (canon[i] == i) {
					pts.PushBack(m.positions[i]);
					deQui.PushBack(static_cast<uint8>(i));
				}
			}
			NkVector<uint32> tri;
			Delaunay(pts.Data(), static_cast<uint32>(pts.Size()), tri);
			const bool forme = garderForme && ancien.nbTriangles > 0u;
			uint32 t = 0;
			for (uint32 k = 0; k + 2u < tri.Size() && t < NK_MAILLAGE2D_TRIANGLES_MAX; k += 3u) {
				const NkVec2f g = (pts[tri[k]] + pts[tri[k + 1u]] + pts[tri[k + 2u]]) * (1.f / 3.f);
				int32 dans = forme ? NkTriangleSous2D(ancien, g) : -1;
				if (forme && dans < 0) {
					continue;
				}
				m.triangles[t * 3u] = deQui[tri[k]];
				m.triangles[t * 3u + 1u] = deQui[tri[k + 1u]];
				m.triangles[t * 3u + 2u] = deQui[tri[k + 2u]];
				m.partieTriangle[t] = dans >= 0 ? ancien.partieTriangle[static_cast<uint32>(dans)] : 0u;
				++t;
			}
			memory::NkGetDefaultAllocator().Delete(pa);
			if (t == 0u) {
				return false;
			}
			m.nbTriangles = static_cast<uint8>(t);
			if (m.nbParties == 0u) {
				m.nbParties = 1u;
				std::strcpy(m.parties[0].nom, "Corps");
			}
			Ameliorer(m);
			NkMaillageRefaireCoutures2D(m);
			return true;
		}

		bool NkMaillageRefaire2D(NkMaillage2D &m, const NkOptionsMaillage2D &o) noexcept {
			if (m.nbTriangles == 0u) {
				return false;
			}
			NkVec2f contour[NK_MAILLAGE2D_SOMMETS_MAX];
			const uint32 n = NkContourMaillage2D(m, contour, NK_MAILLAGE2D_SOMMETS_MAX);
			if (n < 3u) {
				return false;
			}
			NkMaillage2D *pa = memory::NkGetDefaultAllocator().New<NkMaillage2D>();
			if (pa == nullptr) {
				return false;
			}
			NkMaillage2D &ancien = *pa;
			ancien = m;
			NkVector<NkVec2f> c;
			for (uint32 i = 0; i < n; ++i) {
				c.PushBack(contour[i]);
			}
			const bool ok = Remplir(m, c, o);
			if (ok) {
				// La texture reste a sa place : UV et couleur LUS dans l'ancien maillage.
				for (uint32 i = 0; i < m.nbSommets; ++i) {
					const int32 t = TrianglePlusProche(ancien, m.positions[i]);
					if (t >= 0) {
						Interpoler(ancien, static_cast<uint32>(t), m.positions[i], m.uvs[i], m.couleurs[i]);
					}
				}
			} else {
				m = ancien;
			}
			memory::NkGetDefaultAllocator().Delete(pa);
			return ok;
		}

		int32 NkMaillageAjouterSommet2D(NkMaillage2D &m, const NkVec2f &p) noexcept {
			if (m.nbSommets >= NK_MAILLAGE2D_SOMMETS_MAX) {
				return -1;
			}
			for (uint32 i = 0; i < m.nbSommets; ++i) {
				if (MemePlace(m.positions[i], p)) {
					return -1; // deja un sommet ici
				}
			}
			const uint32 s = m.nbSommets;
			m.positions[s] = p;
			m.couleurs[s] = m.nbSommets > 0u ? m.couleurs[0] : 0xFFFFFFFFu;
			m.uvs[s] = NkVec2f(0.f, 0.f);
			m.partieSommet[s] = 0u;
			SansOs(m, s);
			if (m.nbTriangles == 0u) {
				m.nbSommets = static_cast<uint8>(s + 1u);
				// Trois sommets : le premier triangle (puis chaque nouveau s'y relie).
				if (m.nbSommets >= 3u) {
					NkMaillageTrianguler2D(m, false);
					if (m.nbTriangles > 0u) {
						UVBoite(m);
					}
				}
				return static_cast<int32>(s);
			}
			if (m.nbTriangles + 2u > NK_MAILLAGE2D_TRIANGLES_MAX) {
				return -1;
			}
			const int32 dans = NkTriangleSous2D(m, p);
			const int32 proche = TrianglePlusProche(m, p);
			if (proche < 0) {
				return -1;
			}
			Interpoler(m, static_cast<uint32>(dans >= 0 ? dans : proche), p, m.uvs[s], m.couleurs[s]);
			m.nbSommets = static_cast<uint8>(s + 1u);
			if (dans >= 0) {
				// Sur une arete du triangle ? On coupe l'arete (et son voisin).
				const uint32 t = static_cast<uint32>(dans);
				for (uint32 e = 0; e < 3u; ++e) {
					const uint8 a = m.triangles[t * 3u + e], b = m.triangles[t * 3u + (e + 1u) % 3u];
					if (NkDistanceSegment2D(p, m.positions[a], m.positions[b]) < 1.0e-4f) {
						m.nbSommets = static_cast<uint8>(s); // CouperArete pose le sien
						return NkMaillageCouperArete2D(m, a, b);
					}
				}
				// Dedans : le triangle se coupe en trois.
				const uint8 a = m.triangles[t * 3u], b = m.triangles[t * 3u + 1u], c = m.triangles[t * 3u + 2u];
				const uint8 part = m.partieTriangle[t];
				m.partieSommet[s] = part;
				const uint8 ns = static_cast<uint8>(s);
				m.triangles[t * 3u + 2u] = ns; // (a, b, s)
				const uint32 t1 = m.nbTriangles, t2 = m.nbTriangles + 1u;
				m.triangles[t1 * 3u] = b;
				m.triangles[t1 * 3u + 1u] = c;
				m.triangles[t1 * 3u + 2u] = ns;
				m.triangles[t2 * 3u] = c;
				m.triangles[t2 * 3u + 1u] = a;
				m.triangles[t2 * 3u + 2u] = ns;
				m.partieTriangle[t1] = part;
				m.partieTriangle[t2] = part;
				m.nbTriangles = static_cast<uint8>(m.nbTriangles + 2u);
			} else {
				// Dehors : relie a l'arete de BORD la plus proche (le bord du maillage
				// entier, coutures soudees). Le triangle prend la partie de son voisin.
				uint8 canon[NK_MAILLAGE2D_SOMMETS_MAX];
				Canoniques(m, canon);
				float32 dMin = 1.0e30f;
				int32 tMin = -1;
				uint32 eMin = 0;
				for (uint32 t = 0; t < m.nbTriangles; ++t) {
					for (uint32 e = 0; e < 3u; ++e) {
						const uint8 a = m.triangles[t * 3u + e], b = m.triangles[t * 3u + (e + 1u) % 3u];
						// De bord : aucune arete (b -> a) ailleurs, coutures comprises.
						bool bord = true;
						for (uint32 u = 0; u < m.nbTriangles && bord; ++u) {
							for (uint32 f = 0; f < 3u; ++f) {
								if (canon[m.triangles[u * 3u + f]] == canon[b] && canon[m.triangles[u * 3u + (f + 1u) % 3u]] == canon[a]) {
									bord = false;
									break;
								}
							}
						}
						// Et le point doit etre DEVANT elle (a droite : dehors).
						if (!bord || Croix(m.positions[a], m.positions[b], p) >= 0.f) {
							continue;
						}
						const float32 d = NkDistanceSegment2D(p, m.positions[a], m.positions[b]);
						if (d < dMin) {
							dMin = d;
							tMin = static_cast<int32>(t);
							eMin = e;
						}
					}
				}
				if (tMin < 0) {
					m.nbSommets = static_cast<uint8>(s);
					return -1;
				}
				const uint32 t = static_cast<uint32>(tMin);
				const uint8 a = m.triangles[t * 3u + eMin], b = m.triangles[t * 3u + (eMin + 1u) % 3u];
				const uint32 tn = m.nbTriangles;
				m.triangles[tn * 3u] = b;
				m.triangles[tn * 3u + 1u] = a;
				m.triangles[tn * 3u + 2u] = static_cast<uint8>(s);
				m.partieTriangle[tn] = m.partieTriangle[t];
				m.partieSommet[s] = m.partieTriangle[t];
				m.nbTriangles = static_cast<uint8>(tn + 1u);
			}
			Ameliorer(m);
			NkMaillageRefaireCoutures2D(m);
			// L'indice a pu bouger (coutures) : le sommet a cette place.
			for (uint32 i = 0; i < m.nbSommets; ++i) {
				if (MemePlace(m.positions[i], p)) {
					return static_cast<int32>(i);
				}
			}
			return -1;
		}

		int32 NkMaillageCouperArete2D(NkMaillage2D &m, uint32 a, uint32 b) noexcept {
			if (a >= m.nbSommets || b >= m.nbSommets || a == b) {
				return -1;
			}
			const NkVec2f pa = m.positions[a], pb = m.positions[b];
			const NkVec2f milieu = (pa + pb) * 0.5f;
			// Chaque triangle (de chaque partie) qui porte l'arete, coutures comprises.
			uint32 touches = 0;
			for (uint32 t = 0; t < m.nbTriangles; ++t) {
				for (uint32 e = 0; e < 3u; ++e) {
					const NkVec2f &u = m.positions[m.triangles[t * 3u + e]], &v = m.positions[m.triangles[t * 3u + (e + 1u) % 3u]];
					if ((MemePlace(u, pa) && MemePlace(v, pb)) || (MemePlace(u, pb) && MemePlace(v, pa))) {
						++touches;
					}
				}
			}
			if (touches == 0u || m.nbSommets >= NK_MAILLAGE2D_SOMMETS_MAX || m.nbTriangles + touches > NK_MAILLAGE2D_TRIANGLES_MAX) {
				return -1;
			}
			const uint32 s = m.nbSommets;
			m.positions[s] = milieu;
			m.uvs[s] = (m.uvs[a] + m.uvs[b]) * 0.5f;
			m.couleurs[s] = Melanger(m.couleurs[a], m.couleurs[b], 0u, 0.5f, 0.5f, 0.f);
			m.partieSommet[s] = m.partieSommet[a];
			SansOs(m, s);
			m.nbSommets = static_cast<uint8>(s + 1u);
			const uint32 nbAvant = m.nbTriangles;
			for (uint32 t = 0; t < nbAvant; ++t) {
				for (uint32 e = 0; e < 3u; ++e) {
					const uint8 u = m.triangles[t * 3u + e], v = m.triangles[t * 3u + (e + 1u) % 3u];
					const uint8 w = m.triangles[t * 3u + (e + 2u) % 3u];
					const bool porte = (MemePlace(m.positions[u], pa) && MemePlace(m.positions[v], pb)) ||
									   (MemePlace(m.positions[u], pb) && MemePlace(m.positions[v], pa));
					if (!porte) {
						continue;
					}
					// (u, v, w) -> (u, s, w) et (s, v, w) : le meme sens.
					m.triangles[t * 3u] = u;
					m.triangles[t * 3u + 1u] = static_cast<uint8>(s);
					m.triangles[t * 3u + 2u] = w;
					const uint32 tn = m.nbTriangles;
					m.triangles[tn * 3u] = static_cast<uint8>(s);
					m.triangles[tn * 3u + 1u] = v;
					m.triangles[tn * 3u + 2u] = w;
					m.partieTriangle[tn] = m.partieTriangle[t];
					m.nbTriangles = static_cast<uint8>(tn + 1u);
					break;
				}
			}
			NkMaillageRefaireCoutures2D(m);
			for (uint32 i = 0; i < m.nbSommets; ++i) {
				if (MemePlace(m.positions[i], milieu)) {
					return static_cast<int32>(i);
				}
			}
			return -1;
		}

		uint32 NkMaillageRetirerSommets2D(NkMaillage2D &m, const uint8 *choisis) noexcept {
			if (choisis == nullptr) {
				return 0u;
			}
			NkMaillage2D *pa = memory::NkGetDefaultAllocator().New<NkMaillage2D>();
			if (pa == nullptr) {
				return 0u;
			}
			NkMaillage2D &ancien = *pa;
			ancien = m;
			uint8 nouveau[NK_MAILLAGE2D_SOMMETS_MAX];
			uint32 c = 0, retires = 0;
			for (uint32 i = 0; i < m.nbSommets; ++i) {
				if (choisis[i] != 0u) {
					nouveau[i] = 0xFFu;
					++retires;
					continue;
				}
				CopierSommet(m, c, ancien, i);
				nouveau[i] = static_cast<uint8>(c++);
			}
			if (retires == 0u) {
				memory::NkGetDefaultAllocator().Delete(pa);
				return 0u;
			}
			m.nbSommets = static_cast<uint8>(c);
			// Les triangles intacts restent ; les autres partent, le trou se refait
			// par Delaunay DANS l'ancien maillage (garderForme : un sommet interieur
			// retire laisse la matiere pleine ; un sommet de bord la rogne).
			m.nbTriangles = 0u;
			for (uint32 t = 0; t < ancien.nbTriangles; ++t) {
				const uint8 a = nouveau[ancien.triangles[t * 3u]], b = nouveau[ancien.triangles[t * 3u + 1u]],
							d = nouveau[ancien.triangles[t * 3u + 2u]];
				if (a == 0xFFu || b == 0xFFu || d == 0xFFu) {
					continue;
				}
				const uint32 n = m.nbTriangles;
				m.triangles[n * 3u] = a;
				m.triangles[n * 3u + 1u] = b;
				m.triangles[n * 3u + 2u] = d;
				m.partieTriangle[n] = ancien.partieTriangle[t];
				m.nbTriangles = static_cast<uint8>(n + 1u);
			}
			if (m.nbSommets >= 3u && ancien.nbTriangles > 0u) {
				// Les sommets RESTANTS, triangules par Delaunay DANS LA FORME D'AVANT
				// (`ancien` : ses positions et ses triangles d'origine). Un sommet
				// interieur retire laisse la matiere pleine ; un sommet de bord la rogne.
				uint8 canon[NK_MAILLAGE2D_SOMMETS_MAX];
				Canoniques(m, canon);
				NkVector<NkVec2f> pts;
				NkVector<uint8> deQui;
				for (uint32 i = 0; i < m.nbSommets; ++i) {
					if (canon[i] == i) {
						pts.PushBack(m.positions[i]);
						deQui.PushBack(static_cast<uint8>(i));
					}
				}
				NkVector<uint32> tri;
				Delaunay(pts.Data(), static_cast<uint32>(pts.Size()), tri);
				uint32 t = 0;
				for (uint32 k = 0; k + 2u < tri.Size() && t < NK_MAILLAGE2D_TRIANGLES_MAX; k += 3u) {
					const NkVec2f g = (pts[tri[k]] + pts[tri[k + 1u]] + pts[tri[k + 2u]]) * (1.f / 3.f);
					const int32 dans = NkTriangleSous2D(ancien, g);
					if (dans < 0) {
						continue;
					}
					m.triangles[t * 3u] = deQui[tri[k]];
					m.triangles[t * 3u + 1u] = deQui[tri[k + 1u]];
					m.triangles[t * 3u + 2u] = deQui[tri[k + 2u]];
					m.partieTriangle[t] = ancien.partieTriangle[static_cast<uint32>(dans)];
					++t;
				}
				m.nbTriangles = static_cast<uint8>(t);
				Ameliorer(m);
			}
			memory::NkGetDefaultAllocator().Delete(pa);
			NkMaillageRefaireCoutures2D(m);
			return retires;
		}

		int32 NkMaillageFairePartie2D(NkMaillage2D &m, const uint8 *choisis, const char *nom) noexcept {
			if (choisis == nullptr || m.nbTriangles == 0u) {
				return -1;
			}
			if (m.nbParties == 0u) {
				m.nbParties = 1u;
				std::strcpy(m.parties[0].nom, "Corps");
			}
			if (m.nbParties >= NK_MAILLAGE2D_PARTIES_MAX) {
				return -1;
			}
			// Un sommet choisi choisit ses doubles (la couture est UN point a l'ecran).
			uint8 canon[NK_MAILLAGE2D_SOMMETS_MAX];
			Canoniques(m, canon);
			bool place[NK_MAILLAGE2D_SOMMETS_MAX] = {};
			for (uint32 i = 0; i < m.nbSommets; ++i) {
				if (choisis[i] != 0u) {
					place[canon[i]] = true;
				}
			}
			const uint8 k = m.nbParties;
			uint32 pris = 0, restent = 0;
			NkMaillage2D *ps = memory::NkGetDefaultAllocator().New<NkMaillage2D>();
			if (ps == nullptr) {
				return -1;
			}
			*ps = m;
			for (uint32 t = 0; t < m.nbTriangles; ++t) {
				if (place[canon[m.triangles[t * 3u]]] && place[canon[m.triangles[t * 3u + 1u]]] && place[canon[m.triangles[t * 3u + 2u]]]) {
					m.partieTriangle[t] = k;
					++pris;
				}
			}
			for (uint32 t = 0; t < m.nbTriangles; ++t) {
				restent += m.partieTriangle[t] != k ? 1u : 0u;
			}
			// Rien de pris, ou TOUT pris (une partie vide resterait) : refus.
			if (pris == 0u || restent == 0u) {
				m = *ps;
				memory::NkGetDefaultAllocator().Delete(ps);
				return -1;
			}
			NkPartieMaillage2D p;
			if (nom != nullptr && nom[0] != '\0') {
				std::strncpy(p.nom, nom, sizeof(p.nom) - 1u);
			} else {
				std::snprintf(p.nom, sizeof(p.nom), "Partie %u", static_cast<unsigned>(k));
			}
			p.ordre = static_cast<int32>(k);
			m.parties[k] = p;
			m.nbParties = static_cast<uint8>(k + 1u);
			// Une partie VIDEE par ce geste (tous ses triangles pris) part.
			NkMaillageRefaireCoutures2D(m);
			if (!NkMaillageCoherent2D(m)) {
				// La couture n'a pas tenu (plus de place pour les doubles) : rien ne change.
				m = *ps;
				memory::NkGetDefaultAllocator().Delete(ps);
				return -1;
			}
			memory::NkGetDefaultAllocator().Delete(ps);
			for (uint32 q = 1; q < m.nbParties; ++q) {
				bool vide = true;
				for (uint32 t = 0; t < m.nbTriangles && vide; ++t) {
					vide = m.partieTriangle[t] != q;
				}
				if (vide && q != k) {
					NkMaillageRetirerPartie2D(m, q);
					return static_cast<int32>(m.nbParties) - 1;
				}
			}
			return static_cast<int32>(k);
		}

		bool NkMaillageRetirerPartie2D(NkMaillage2D &m, uint32 k) noexcept {
			if (k == 0u || k >= m.nbParties) {
				return false;
			}
			for (uint32 t = 0; t < m.nbTriangles; ++t) {
				if (m.partieTriangle[t] == k) {
					m.partieTriangle[t] = 0u;
				} else if (m.partieTriangle[t] > k) {
					m.partieTriangle[t] = static_cast<uint8>(m.partieTriangle[t] - 1u);
				}
			}
			for (uint32 i = 0; i < m.nbSommets; ++i) {
				if (m.partieSommet[i] == k) {
					m.partieSommet[i] = 0u;
				} else if (m.partieSommet[i] > k) {
					m.partieSommet[i] = static_cast<uint8>(m.partieSommet[i] - 1u);
				}
			}
			for (uint32 q = k; q + 1u < m.nbParties; ++q) {
				m.parties[q] = m.parties[q + 1u];
			}
			m.parties[m.nbParties - 1u] = NkPartieMaillage2D();
			m.nbParties = static_cast<uint8>(m.nbParties - 1u);
			// Les liens : ceux de la partie partent, les autres suivent la renumerotation.
			uint32 w = 0;
			for (uint32 l = 0; l < m.nbLiens; ++l) {
				NkLienParties2D x = m.liens[l];
				if (x.a == k || x.b == k) {
					continue;
				}
				x.a = static_cast<uint8>(x.a > k ? x.a - 1u : x.a);
				x.b = static_cast<uint8>(x.b > k ? x.b - 1u : x.b);
				m.liens[w++] = x;
			}
			for (uint32 l = w; l < m.nbLiens; ++l) {
				m.liens[l] = NkLienParties2D();
			}
			m.nbLiens = static_cast<uint8>(w);
			NkMaillageRefaireCoutures2D(m);
			return true;
		}

		bool NkPartiesVoisines2D(const NkMaillage2D &m, uint32 a, uint32 b) noexcept {
			for (uint32 i = 0; i < m.nbSommets; ++i) {
				if (m.partieSommet[i] != a) {
					continue;
				}
				for (uint32 j = 0; j < m.nbSommets; ++j) {
					if (m.partieSommet[j] == b && MemePlace(m.positions[i], m.positions[j])) {
						return true;
					}
				}
			}
			return false;
		}

		int32 NkMaillageAjouterLien2D(NkMaillage2D &m, uint32 a, uint32 b, NkGenreLienParties2D genre) noexcept {
			if (a == b || a >= m.nbParties || b >= m.nbParties || m.nbLiens >= NK_MAILLAGE2D_LIENS_MAX) {
				return -1;
			}
			for (uint32 l = 0; l < m.nbLiens; ++l) {
				if ((m.liens[l].a == a && m.liens[l].b == b) || (m.liens[l].a == b && m.liens[l].b == a)) {
					m.liens[l].genre = static_cast<uint8>(genre); // deja relies : le genre change
					return static_cast<int32>(l);
				}
			}
			NkLienParties2D x;
			x.a = static_cast<uint8>(a);
			x.b = static_cast<uint8>(b);
			x.genre = static_cast<uint8>(genre);
			m.liens[m.nbLiens] = x;
			m.nbLiens = static_cast<uint8>(m.nbLiens + 1u);
			return static_cast<int32>(m.nbLiens) - 1;
		}

		bool NkMaillageRetirerLien2D(NkMaillage2D &m, uint32 k) noexcept {
			if (k >= m.nbLiens) {
				return false;
			}
			for (uint32 l = k; l + 1u < m.nbLiens; ++l) {
				m.liens[l] = m.liens[l + 1u];
			}
			m.liens[m.nbLiens - 1u] = NkLienParties2D();
			m.nbLiens = static_cast<uint8>(m.nbLiens - 1u);
			return true;
		}

		// =====================================================================
		// LA DESCRIPTION (sauvegarde, photo, prefab)
		// =====================================================================
		namespace {
			/// Un TABLEAU de scalaires : un champ par tableau, « a b c ... » au fichier.
#define NK_MAILLAGE_LISTE(champ, genre)                                                                                \
	NkChampSauve {                                                                                                     \
		#champ, genre, static_cast<uint32>(offsetof(NkMaillage2D, champ)),                                            \
			static_cast<uint32>(sizeof(static_cast<NkMaillage2D *>(nullptr)->champ[0])),                              \
			static_cast<uint32>(sizeof(static_cast<NkMaillage2D *>(nullptr)->champ) /                                 \
								sizeof(static_cast<NkMaillage2D *>(nullptr)->champ[0])),                              \
			0u, false                                                                                                  \
	}
#define NK_MAILLAGE_PARTIE(champ, genre)                                                                               \
	NK_UNKENY_CHAMP_TABLEAU(NkMaillage2D, parties, NkPartieMaillage2D, champ, genre, "parties." #champ)
#define NK_MAILLAGE_LIEN(champ, genre) NK_UNKENY_CHAMP_TABLEAU(NkMaillage2D, liens, NkLienParties2D, champ, genre, "liens." #champ)

			const NkChampSauve kChampsMaillage[] = {
				NK_UNKENY_CHAMP(NkMaillage2D, texId, NkTypeChamp::NK_TEXTURE),
				NK_UNKENY_CHAMP(NkMaillage2D, teinte, NkTypeChamp::NK_U32),
				NK_UNKENY_CHAMP(NkMaillage2D, couche, NkTypeChamp::NK_I32),
				NK_UNKENY_CHAMP(NkMaillage2D, visible, NkTypeChamp::NK_BOOL),
				NK_UNKENY_CHAMP(NkMaillage2D, nbSommets, NkTypeChamp::NK_U8),
				NK_UNKENY_CHAMP(NkMaillage2D, nbTriangles, NkTypeChamp::NK_U8),
				NK_UNKENY_CHAMP(NkMaillage2D, nbParties, NkTypeChamp::NK_U8),
				NK_UNKENY_CHAMP(NkMaillage2D, nbLiens, NkTypeChamp::NK_U8),
				NK_UNKENY_CHAMP(NkMaillage2D, partiesSeTouchent, NkTypeChamp::NK_BOOL),
				NK_UNKENY_CHAMP(NkMaillage2D, source, NkTypeChamp::NK_TEXTE),
				NK_MAILLAGE_LISTE(positions, NkTypeChamp::NK_VEC2),
				NK_MAILLAGE_LISTE(uvs, NkTypeChamp::NK_VEC2),
				NK_MAILLAGE_LISTE(couleurs, NkTypeChamp::NK_U32),
				NK_MAILLAGE_LISTE(partieSommet, NkTypeChamp::NK_U8),
				NK_MAILLAGE_LISTE(os, NkTypeChamp::NK_U8),
				NK_MAILLAGE_LISTE(poids, NkTypeChamp::NK_U16),
				NK_MAILLAGE_LISTE(triangles, NkTypeChamp::NK_U8),
				NK_MAILLAGE_LISTE(partieTriangle, NkTypeChamp::NK_U8),
				NK_MAILLAGE_PARTIE(nom, NkTypeChamp::NK_TEXTE),
				NK_MAILLAGE_PARTIE(couleur, NkTypeChamp::NK_U32),
				NK_MAILLAGE_PARTIE(ordre, NkTypeChamp::NK_I32),
				NK_MAILLAGE_PARTIE(physique, NkTypeChamp::NK_U8),
				NK_MAILLAGE_PARTIE(typeCorps, NkTypeChamp::NK_U8),
				NK_MAILLAGE_PARTIE(collision, NkTypeChamp::NK_BOOL),
				NK_MAILLAGE_PARTIE(masse, NkTypeChamp::NK_F32),
				NK_MAILLAGE_PARTIE(friction, NkTypeChamp::NK_F32),
				NK_MAILLAGE_PARTIE(rebond, NkTypeChamp::NK_F32),
				NK_MAILLAGE_PARTIE(raideur, NkTypeChamp::NK_F32),
				NK_MAILLAGE_PARTIE(forme, NkTypeChamp::NK_F32),
				NK_MAILLAGE_LIEN(a, NkTypeChamp::NK_U8),
				NK_MAILLAGE_LIEN(b, NkTypeChamp::NK_U8),
				NK_MAILLAGE_LIEN(genre, NkTypeChamp::NK_U8),
				NK_MAILLAGE_LIEN(raideur, NkTypeChamp::NK_F32),
				NK_MAILLAGE_LIEN(amortissement, NkTypeChamp::NK_F32),
				NK_MAILLAGE_LIEN(rupture, NkTypeChamp::NK_F32),
			};
#undef NK_MAILLAGE_LISTE
#undef NK_MAILLAGE_PARTIE
#undef NK_MAILLAGE_LIEN
		} // namespace

		const NkChampSauve *NkChampsMaillage2D(uint32 &nombre) noexcept {
			nombre = static_cast<uint32>(sizeof(kChampsMaillage) / sizeof(kChampsMaillage[0]));
			return kChampsMaillage;
		}

	} // namespace unkeny
} // namespace nkentseu
