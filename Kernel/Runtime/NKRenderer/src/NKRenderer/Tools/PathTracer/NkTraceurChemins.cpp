// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/PathTracer/NkTraceurChemins.cpp
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// Le traceur de chemins (voir l'en-tete). Aucune bibliotheque standard.

#include "NKRenderer/Tools/PathTracer/NkTraceurChemins.h"
#include "NKRenderer/Tools/PathTracer/NkMatieresProcedurales.h"
#include "NKImage/Core/NkImage.h"
#include "NKThreading/NkThreadPool.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKCore/Text/NkSnprintf.h"
#include "NKCore/Text/NkSortie.h"
#include "NKMath/NkFunctions.h"
#include "NKTime/NkChrono.h"
#include "NKPlatform/NkEnv.h"

namespace nkentseu {
	namespace renderer {

		namespace {
			constexpr float32 kPi = 3.14159265358979f;

			struct V3 {
					float32 x, y, z;
			};
			inline V3 Mk(float32 a, float32 b, float32 c) {
				return V3{a, b, c};
			}
			inline V3 operator+(V3 a, V3 b) {
				return V3{a.x + b.x, a.y + b.y, a.z + b.z};
			}
			inline V3 operator-(V3 a, V3 b) {
				return V3{a.x - b.x, a.y - b.y, a.z - b.z};
			}
			inline V3 operator*(V3 a, float32 s) {
				return V3{a.x * s, a.y * s, a.z * s};
			}
			inline V3 Mul(V3 a, V3 b) {
				return V3{a.x * b.x, a.y * b.y, a.z * b.z};
			}
			inline float32 Dot(V3 a, V3 b) {
				return a.x * b.x + a.y * b.y + a.z * b.z;
			}
			inline V3 Cross(V3 a, V3 b) {
				return V3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
			}
			inline float32 Racine(float32 x) {
				return math::NkSqrt(x > 0.f ? x : 0.f);
			}
			inline V3 Norm(V3 a) {
				const float32 l = Racine(Dot(a, a));
				return l > 1e-20f ? a * (1.f / l) : V3{0.f, 1.f, 0.f};
			}
			inline float32 Lum(V3 c) {
				return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
			}
			inline float32 Min(float32 a, float32 b) {
				return a < b ? a : b;
			}
			inline float32 Max(float32 a, float32 b) {
				return a > b ? a : b;
			}
			inline float32 Abs(float32 a) {
				return a < 0.f ? -a : a;
			}
			inline void Base(V3 n, V3 &t, V3 &b) {
				const V3 a = Abs(n.y) < 0.9f ? V3{0.f, 1.f, 0.f} : V3{1.f, 0.f, 0.f};
				t = Norm(Cross(a, n));
				b = Cross(n, t);
			}

			// PCG32 (O'Neill 2014)
			struct Rng {
					uint64 s;
					uint32 Next() {
						const uint64 old = s;
						s = old * 6364136223846793005ULL + 1442695040888963407ULL;
						const uint32 x = (uint32)(((old >> 18u) ^ old) >> 27u);
						const uint32 r = (uint32)(old >> 59u);
						return (x >> r) | (x << ((0u - r) & 31u));
					}
					float32 F() {
						return (float32)(Next() >> 8) * (1.f / 16777216.f);
					}
			};
			inline uint64 Melange(uint64 x) {
				x ^= x >> 33;
				x *= 0xff51afd7ed558ccdULL;
				x ^= x >> 33;
				x *= 0xc4ceb9fe1a85ec53ULL;
				x ^= x >> 33;
				return x;
			}

			struct Tri {
					V3 v0, e1, e2;
					uint32 i0, i1, i2;
					int32 mat;
			};
			struct Noeud {
					float32 mn[3], mx[3];
					int32 g; // feuille : premier triangle ; sinon : enfant gauche (droit = g + 1)
					int32 nb; // > 0 : feuille
			};
			struct Carte {
					int32 w = 0, h = 0;
					NkVector<uint8> rgb;
					float32 moy[3] = {1.f, 1.f, 1.f};
			};

			struct Scene {
					bool prete = false;
					NkScArchviz s;
					NkVector<Tri> tris;
					NkVector<Noeud> noeuds;
					NkVector<uint32> ordre;
					Carte cartes[kMatClasses];
					float32 lin[256];
					NkVector<float32> cdfSources;
					float32 puissanceTotale = 0.f;
					bool mutationBanc = false; // contre-epreuve du banc : Lambert sans 1/pi
			};
			Scene &G() {
				static Scene g;
				return g;
			}
			uint64 gNonFinis = 0; // echantillons ecartes (NaN, infini) depuis le dernier rendu
			// compteurs de traversee (par fil) : noeuds visites, triangles testes
			thread_local uint64 tNoeuds = 0, tTris = 0;
			uint64 gNoeuds = 0, gTris = 0;

			// ── BVH (SAH a casiers) ─────────────────────────────────────────────
			struct Boite {
					float32 mn[3] = {1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f, -1e30f, -1e30f};
					void Ajoute(const float32 *p) {
						for (int32 a = 0; a < 3; ++a) {
							mn[a] = Min(mn[a], p[a]);
							mx[a] = Max(mx[a], p[a]);
						}
					}
					void Ajoute(const Boite &b) {
						for (int32 a = 0; a < 3; ++a) {
							mn[a] = Min(mn[a], b.mn[a]);
							mx[a] = Max(mx[a], b.mx[a]);
						}
					}
					float32 Aire() const {
						const float32 d0 = mx[0] - mn[0], d1 = mx[1] - mn[1], d2 = mx[2] - mn[2];
						if (d0 < 0.f)
							return 0.f;
						return 2.f * (d0 * d1 + d1 * d2 + d2 * d0);
					}
			};
			void BoiteTri(const Tri &t, Boite &b) {
				const float32 a[3] = {t.v0.x, t.v0.y, t.v0.z};
				const float32 p1[3] = {t.v0.x + t.e1.x, t.v0.y + t.e1.y, t.v0.z + t.e1.z};
				const float32 p2[3] = {t.v0.x + t.e2.x, t.v0.y + t.e2.y, t.v0.z + t.e2.z};
				b.Ajoute(a);
				b.Ajoute(p1);
				b.Ajoute(p2);
			}
			float32 Centre(const Tri &t, int32 a) {
				const float32 v0 = a == 0 ? t.v0.x : (a == 1 ? t.v0.y : t.v0.z);
				const float32 e1 = a == 0 ? t.e1.x : (a == 1 ? t.e1.y : t.e1.z);
				const float32 e2 = a == 0 ? t.e2.x : (a == 1 ? t.e2.y : t.e2.z);
				return v0 + (e1 + e2) * (1.f / 3.f);
			}
			// remplit le noeud `ni` (deja alloue) ; les deux enfants sont alloues COTE A COTE
			// avant de descendre (la traversee lit le droit en g + 1)
			void Construire(Scene &g, int32 ni, int32 debut, int32 fin, int32 prof) {
				Boite b, bc;
				for (int32 i = debut; i < fin; ++i) {
					BoiteTri(g.tris[g.ordre[(usize)i]], b);
					float32 c[3];
					for (int32 a = 0; a < 3; ++a)
						c[a] = Centre(g.tris[g.ordre[(usize)i]], a);
					bc.Ajoute(c);
				}
				Noeud nd;
				for (int32 a = 0; a < 3; ++a) {
					nd.mn[a] = b.mn[a];
					nd.mx[a] = b.mx[a];
				}
				const int32 n = fin - debut;
				if (n <= 4 || prof > 60) {
					nd.g = debut;
					nd.nb = n;
					g.noeuds[(usize)ni] = nd;
					return;
				}
				// la meilleure coupe : 3 axes x 12 casiers, cout = aire x nombre
				const int32 kC = 12;
				float32 meilleur = 1e30f;
				int32 axe = -1, coupe = 0;
				for (int32 a = 0; a < 3; ++a) {
					const float32 lo = bc.mn[a], hi = bc.mx[a];
					if (hi - lo < 1e-6f)
						continue;
					Boite bb[kC];
					int32 cnt[kC] = {0};
					for (int32 i = debut; i < fin; ++i) {
						const Tri &t = g.tris[g.ordre[(usize)i]];
						int32 k = (int32)((Centre(t, a) - lo) / (hi - lo) * (float32)kC);
						k = k < 0 ? 0 : (k >= kC ? kC - 1 : k);
						++cnt[k];
						BoiteTri(t, bb[k]);
					}
					for (int32 c = 1; c < kC; ++c) {
						Boite l, r;
						int32 nl = 0, nr = 0;
						for (int32 k = 0; k < c; ++k) {
							if (cnt[k]) {
								l.Ajoute(bb[k]);
								nl += cnt[k];
							}
						}
						for (int32 k = c; k < kC; ++k) {
							if (cnt[k]) {
								r.Ajoute(bb[k]);
								nr += cnt[k];
							}
						}
						if (nl == 0 || nr == 0)
							continue;
						const float32 cout = l.Aire() * (float32)nl + r.Aire() * (float32)nr;
						if (cout < meilleur) {
							meilleur = cout;
							axe = a;
							coupe = c;
						}
					}
				}
				int32 milieu = (debut + fin) / 2;
				if (axe >= 0 && meilleur < b.Aire() * (float32)n) {
					const float32 lo = bc.mn[axe], hi = bc.mx[axe];
					int32 i = debut, j = fin - 1;
					while (i <= j) {
						int32 k = (int32)((Centre(g.tris[g.ordre[(usize)i]], axe) - lo) / (hi - lo) * (float32)kC);
						k = k < 0 ? 0 : (k >= kC ? kC - 1 : k);
						if (k < coupe)
							++i;
						else {
							const uint32 tmp = g.ordre[(usize)i];
							g.ordre[(usize)i] = g.ordre[(usize)j];
							g.ordre[(usize)j] = tmp;
							--j;
						}
					}
					milieu = i;
					if (milieu == debut || milieu == fin)
						milieu = (debut + fin) / 2;
				} else if (n <= 12) {
					nd.g = debut;
					nd.nb = n;
					g.noeuds[(usize)ni] = nd;
					return;
				}
				const int32 gauche = (int32)g.noeuds.Size();
				g.noeuds.PushBack(Noeud{});
				g.noeuds.PushBack(Noeud{});
				nd.g = gauche;
				nd.nb = 0;
				g.noeuds[(usize)ni] = nd;
				Construire(g, gauche, debut, milieu, prof + 1);
				Construire(g, gauche + 1, milieu, fin, prof + 1);
			}

			struct Impact {
					float32 t = 1e30f, b1 = 0.f, b2 = 0.f;
					int32 tri = -1;
			};
			inline bool BoiteRayon(const Noeud &n, V3 o, V3 inv, float32 tmax, float32 &tEntree) {
				float32 t0 = (n.mn[0] - o.x) * inv.x, t1 = (n.mx[0] - o.x) * inv.x;
				float32 tn = Min(t0, t1), tf = Max(t0, t1);
				t0 = (n.mn[1] - o.y) * inv.y;
				t1 = (n.mx[1] - o.y) * inv.y;
				tn = Max(tn, Min(t0, t1));
				tf = Min(tf, Max(t0, t1));
				t0 = (n.mn[2] - o.z) * inv.z;
				t1 = (n.mx[2] - o.z) * inv.z;
				tn = Max(tn, Min(t0, t1));
				tf = Min(tf, Max(t0, t1));
				tEntree = tn;
				return tf >= Max(tn, 0.f) && tn < tmax;
			}
			inline bool MT(const Tri &t, V3 o, V3 d, float32 tmin, float32 tmax, float32 &tt, float32 &u, float32 &v) {
				const V3 p = Cross(d, t.e2);
				const float32 det = Dot(t.e1, p);
				if (Abs(det) < 1e-14f)
					return false;
				const float32 inv = 1.f / det;
				const V3 s = o - t.v0;
				u = Dot(s, p) * inv;
				if (u < 0.f || u > 1.f)
					return false;
				const V3 q = Cross(s, t.e1);
				v = Dot(d, q) * inv;
				if (v < 0.f || u + v > 1.f)
					return false;
				tt = Dot(t.e2, q) * inv;
				return tt > tmin && tt < tmax;
			}
			V3 Inv(V3 d) {
				return V3{1.f / (Abs(d.x) > 1e-12f ? d.x : 1e-12f), 1.f / (Abs(d.y) > 1e-12f ? d.y : 1e-12f),
						  1.f / (Abs(d.z) > 1e-12f ? d.z : 1e-12f)};
			}
			bool Tracer(const Scene &g, V3 o, V3 d, float32 tmax, Impact &h) {
				if (g.noeuds.Empty())
					return false;
				const V3 inv = Inv(d);
				// pointeurs nus : operator[] de NkVector garde une assertion (la traversee en paie des millions)
				const Noeud *NO = g.noeuds.Data();
				const uint32 *OR = g.ordre.Data();
				const Tri *TR = g.tris.Data();
				int32 pile[96];
				int32 np = 0;
				pile[np++] = 0;
				h.t = tmax;
				h.tri = -1;
				while (np > 0) {
					const Noeud &n = NO[pile[--np]];
					++tNoeuds;
					float32 te;
					if (!BoiteRayon(n, o, inv, h.t, te))
						continue;
					if (n.nb > 0) {
						for (int32 i = 0; i < n.nb; ++i) {
							const uint32 ti = OR[n.g + i];
							float32 tt, u, v;
							++tTris;
							if (MT(TR[ti], o, d, 1e-4f, h.t, tt, u, v)) {
								h.t = tt;
								h.b1 = u;
								h.b2 = v;
								h.tri = (int32)ti;
							}
						}
					} else if (np < 94) {
						float32 ta, tb;
						const bool ha = BoiteRayon(NO[n.g], o, inv, h.t, ta);
						const bool hb = BoiteRayon(NO[n.g + 1], o, inv, h.t, tb);
						if (ha && hb) {
							if (ta < tb) {
								pile[np++] = n.g + 1;
								pile[np++] = n.g;
							} else {
								pile[np++] = n.g;
								pile[np++] = n.g + 1;
							}
						} else if (ha)
							pile[np++] = n.g;
						else if (hb)
							pile[np++] = n.g + 1;
					}
				}
				return h.tri >= 0;
			}
			// Transmission le long d'un segment : 0 si un opaque le coupe ; le verre (mince)
			// laisse passer (1 - F normal) x teinte.
			V3 Transmission(const Scene &g, V3 o, V3 d, float32 tmax) {
				V3 tr{1.f, 1.f, 1.f};
				const V3 inv = Inv(d);
				const Noeud *NO = g.noeuds.Data();
				const uint32 *OR = g.ordre.Data();
				const Tri *TR = g.tris.Data();
				const NkScMateriau *MA = g.s.mats.Data();
				int32 pile[96];
				int32 np = 0;
				pile[np++] = 0;
				while (np > 0) {
					const Noeud &n = NO[pile[--np]];
					++tNoeuds;
					float32 te;
					if (!BoiteRayon(n, o, inv, tmax, te))
						continue;
					if (n.nb > 0) {
						for (int32 i = 0; i < n.nb; ++i) {
							const Tri &t = TR[OR[n.g + i]];
							float32 tt, u, v;
							++tTris;
							if (!MT(t, o, d, 1e-4f, tmax, tt, u, v))
								continue;
							const NkScMateriau &m = MA[t.mat];
							if (m.transmission > 0.5f) {
								tr = Mul(tr, Mk(m.couleur[0], m.couleur[1], m.couleur[2]) * 0.96f);
								if (Lum(tr) < 1e-3f)
									return V3{0.f, 0.f, 0.f};
							} else
								return V3{0.f, 0.f, 0.f};
						}
					} else if (np < 94) {
						pile[np++] = n.g;
						pile[np++] = n.g + 1;
					}
				}
				return tr;
			}

			// ── matieres ──────────────────────────────────────────────────────────
			struct Surface {
					V3 p, ng, ns, albedo, emis;
					float32 rug, metal;
					bool verre;
					int32 mat;
			};
			void Surf(const Scene &g, const Impact &h, V3 o, V3 d, bool sansCartes, Surface &S) {
				const Tri &t = g.tris.Data()[h.tri];
				const float32 w0 = 1.f - h.b1 - h.b2;
				S.p = o + d * h.t;
				const float32 *N = g.s.N.Data();
				V3 ns = Mk(N[t.i0 * 3] * w0 + N[t.i1 * 3] * h.b1 + N[t.i2 * 3] * h.b2,
						   N[t.i0 * 3 + 1] * w0 + N[t.i1 * 3 + 1] * h.b1 + N[t.i2 * 3 + 1] * h.b2,
						   N[t.i0 * 3 + 2] * w0 + N[t.i1 * 3 + 2] * h.b1 + N[t.i2 * 3 + 2] * h.b2);
				ns = Norm(ns);
				V3 ng = Norm(Cross(t.e1, t.e2));
				if (Dot(ng, ns) < 0.f)
					ng = ng * -1.f;
				const NkScMateriau &m = g.s.mats.Data()[t.mat];
				S.mat = t.mat;
				S.verre = m.transmission > 0.5f;
				S.rug = Max(m.rugosite, 0.03f);
				S.metal = m.metal;
				S.albedo = Mk(m.couleur[0], m.couleur[1], m.couleur[2]);
				S.emis = S.albedo * m.emission;
				if (!sansCartes && m.classe >= 0 && m.classe < kMatClasses && g.cartes[m.classe].w > 0) {
					const Carte &c = g.cartes[m.classe];
					const float32 *UV = g.s.UV.Data();
					const float32 u = (UV[t.i0 * 2] * w0 + UV[t.i1 * 2] * h.b1 + UV[t.i2 * 2] * h.b2) / m.tuile;
					const float32 v = (UV[t.i0 * 2 + 1] * w0 + UV[t.i1 * 2 + 1] * h.b1 + UV[t.i2 * 2 + 1] * h.b2) / m.tuile;
					float32 fu = u - (float32)(int32)u, fv = v - (float32)(int32)v;
					if (fu < 0.f)
						fu += 1.f;
					if (fv < 0.f)
						fv += 1.f;
					int32 x = (int32)(fu * (float32)c.w), y = (int32)((1.f - fv) * (float32)c.h);
					x = x < 0 ? 0 : (x >= c.w ? c.w - 1 : x);
					y = y < 0 ? 0 : (y >= c.h ? c.h - 1 : y);
					const uint8 *px = c.rgb.Data() + ((usize)y * (usize)c.w + (usize)x) * 3;
					if (NkMatiereDe(m.classe).couleurPropre) {
						// carte a COULEUR PROPRE (herbe) : ses teintes, a la luminance de la matiere. La
						// diviser canal par canal virait la terre au mauve (lu sur `tc1`).
						const float32 k = Lum(S.albedo) / Max(0.02f, Lum(Mk(c.moy[0], c.moy[1], c.moy[2])));
						S.albedo = Mk(Min(0.98f, g.lin[px[0]] * k), Min(0.98f, g.lin[px[1]] * k), Min(0.98f, g.lin[px[2]] * k));
					} else
						S.albedo = Mk(Min(0.98f, S.albedo.x * g.lin[px[0]] / c.moy[0]), Min(0.98f, S.albedo.y * g.lin[px[1]] / c.moy[1]),
									  Min(0.98f, S.albedo.z * g.lin[px[2]] / c.moy[2]));
				}
				// deux faces pour l'opaque : la normale regarde le rayon
				if (!S.verre && Dot(ng, d) > 0.f) {
					ng = ng * -1.f;
					ns = ns * -1.f;
				}
				if (!S.verre && Dot(ns, d) > 0.f)
					ns = ns - ng * (Dot(ns, ng) * 2.f); // normale de rendu tournee vers le rayon
				S.ng = ng;
				S.ns = Norm(ns);
			}

			// GGX (Trowbridge-Reitz), Smith separable, Schlick
			inline float32 DGgx(float32 nh, float32 a2) {
				const float32 d = nh * nh * (a2 - 1.f) + 1.f;
				return a2 / (kPi * d * d + 1e-12f);
			}
			inline float32 G1(float32 nx, float32 a2) {
				return 2.f * nx / (nx + Racine(a2 + (1.f - a2) * nx * nx) + 1e-12f);
			}
			inline V3 Schlick(V3 f0, float32 c) {
				const float32 k = (1.f - c);
				const float32 k5 = k * k * k * k * k;
				return f0 + (V3{1.f, 1.f, 1.f} - f0) * k5;
			}
			// BSDF opaque : valeur f x cos(wi)
			V3 EvalBsdf(const Scene &g, const Surface &S, V3 wo, V3 wi) {
				const float32 nl = Dot(S.ns, wi), nv = Dot(S.ns, wo);
				if (nl <= 0.f || nv <= 0.f || Dot(S.ng, wi) <= 0.f)
					return V3{0.f, 0.f, 0.f};
				const V3 f0 = Schlick(V3{0.04f, 0.04f, 0.04f}, 1.f) * (1.f - S.metal) + S.albedo * S.metal;
				const V3 h = Norm(wo + wi);
				const float32 a2 = (S.rug * S.rug) * (S.rug * S.rug);
				const V3 F = Schlick(f0, Max(0.f, Dot(wo, h)));
				const float32 spec = DGgx(Max(0.f, Dot(S.ns, h)), a2) * G1(nl, a2) * G1(nv, a2) / (4.f * nl * nv + 1e-8f);
				const float32 invPi = g.mutationBanc ? 1.f : 1.f / kPi;
				const V3 kd = Mul(S.albedo, V3{1.f, 1.f, 1.f} - F) * ((1.f - S.metal) * invPi);
				return (kd + F * spec) * nl;
			}
			float32 ProbaSpec(const Surface &S) {
				return S.metal > 0.5f ? 1.f : (S.rug < 0.3f ? 0.4f : 0.15f);
			}
			float32 PdfBsdf(const Surface &S, V3 wo, V3 wi) {
				const float32 nl = Dot(S.ns, wi);
				if (nl <= 0.f)
					return 0.f;
				const V3 h = Norm(wo + wi);
				const float32 a2 = (S.rug * S.rug) * (S.rug * S.rug);
				const float32 nh = Max(0.f, Dot(S.ns, h));
				const float32 ps = ProbaSpec(S);
				const float32 pSpec = DGgx(nh, a2) * nh / (4.f * Max(1e-6f, Dot(wo, h)));
				return ps * pSpec + (1.f - ps) * nl / kPi;
			}
			V3 TirerBsdf(const Surface &S, V3 wo, Rng &r) {
				V3 t, b;
				Base(S.ns, t, b);
				const float32 u1 = r.F(), u2 = r.F();
				if (r.F() < ProbaSpec(S)) {
					const float32 a2 = (S.rug * S.rug) * (S.rug * S.rug);
					const float32 ct = Racine((1.f - u1) / (1.f + (a2 - 1.f) * u1));
					const float32 st = Racine(1.f - ct * ct);
					const float32 ph = 2.f * kPi * u2;
					const V3 h = t * (st * math::NkCos(ph)) + b * (st * math::NkSin(ph)) + S.ns * ct;
					return h * (2.f * Dot(wo, h)) - wo;
				}
				const float32 rr = Racine(u1), ph = 2.f * kPi * u2;
				return Norm(t * (rr * math::NkCos(ph)) + b * (rr * math::NkSin(ph)) + S.ns * Racine(1.f - u1));
			}

			V3 Ciel(const Scene &g, V3 d) {
				const NkScArchviz &s = g.s;
				const V3 hz = Mk(s.cielHorizon[0], s.cielHorizon[1], s.cielHorizon[2]);
				const V3 ze = Mk(s.cielZenith[0], s.cielZenith[1], s.cielZenith[2]);
				// sous l'horizon : la teinte d'horizon (le terrain cache ces directions dans la
				// maison). Une premiere version y mettait la MOITIE : le four blanc l'a vu (0,589
				// au lieu de 0,8 ; le verre 0,73 au lieu de 1).
				if (d.y < 0.f)
					return hz * s.cielIntensite;
				const float32 k = Racine(d.y);
				return (hz * (1.f - k) + ze * k) * s.cielIntensite;
			}

			struct Ctx {
					const NkTcReglages *r;
					V3 soleil, soleilE;
					float32 cosSoleil;
					uint64 rayons;
					uint64 nonFinis;
			};

			// NEE : soleil + une source (RIS) ; renvoie la contribution (sans le debit)
			V3 Directe(const Scene &g, Ctx &c, const Surface &S, V3 wo, Rng &r) {
				V3 L{0.f, 0.f, 0.f};
				const V3 o = S.p + S.ng * 2e-3f;
				if (!c.r->sansSoleil && Lum(c.soleilE) > 0.f && Dot(S.ng, c.soleil) > 0.f) {
					V3 t, b;
					Base(c.soleil, t, b);
					const float32 ct = 1.f - r.F() * (1.f - c.cosSoleil), st = Racine(1.f - ct * ct), ph = 2.f * kPi * r.F();
					const V3 wi = Norm(t * (st * math::NkCos(ph)) + b * (st * math::NkSin(ph)) + c.soleil * ct);
					const V3 f = EvalBsdf(g, S, wo, wi);
					if (Lum(f) > 0.f) {
						++c.rayons;
						const V3 tr = Transmission(g, o, wi, 1e6f);
						L = L + Mul(Mul(f, c.soleilE), tr);
					}
				}
				const usize ns = g.s.sources.Size();
				const NkScSource *SRC = g.s.sources.Data();
				const float32 *CDF = g.cdfSources.Data();
				if (!c.r->sansSources && ns > 0 && g.puissanceTotale > 0.f) {
					// RIS : 8 candidates tirees selon la puissance
					const int32 M = 8;
					int32 cand[M];
					float32 w[M], somme = 0.f;
					for (int32 k = 0; k < M; ++k) {
						const float32 u = r.F() * g.puissanceTotale;
						usize lo = 0, hi = ns - 1;
						while (lo < hi) {
							const usize mi = (lo + hi) / 2;
							if (CDF[mi] < u)
								lo = mi + 1;
							else
								hi = mi;
						}
						cand[k] = (int32)lo;
						const NkScSource &q = SRC[lo];
						const V3 dv = Mk(q.p[0], q.p[1], q.p[2]) - S.p;
						const float32 d2 = Max(Dot(dv, dv), q.rayon * q.rayon + 1e-4f);
						const V3 wi = dv * (1.f / Racine(d2));
						float32 ph = q.intensite * Max(0.f, Dot(S.ns, wi)) / d2;
						if (q.type == 1) {
							const float32 ca = -Dot(wi, Mk(q.dir[0], q.dir[1], q.dir[2]));
							ph *= Max(0.f, Min(1.f, (ca - q.cosExt) / Max(1e-3f, (1.f - q.cosExt) * 0.15f)));
						}
						const float32 p = q.puissance / g.puissanceTotale;
						w[k] = p > 0.f ? ph / p : 0.f;
						somme += w[k];
					}
					if (somme > 0.f) {
						float32 u = r.F() * somme;
						int32 k = 0;
						while (k < M - 1 && u >= w[k]) {
							u -= w[k];
							++k;
						}
						const NkScSource &q = SRC[cand[k]];
						// un point de la sphere de la source (ombres douces)
						const float32 z = 1.f - 2.f * r.F(), ph2 = 2.f * kPi * r.F(), rz = Racine(1.f - z * z);
						const V3 pl = Mk(q.p[0], q.p[1], q.p[2]) + Mk(rz * math::NkCos(ph2), z, rz * math::NkSin(ph2)) * q.rayon;
						const V3 dv = pl - S.p;
						const float32 d2 = Max(Dot(dv, dv), 1e-4f);
						const float32 dl = Racine(d2);
						const V3 wi = dv * (1.f / dl);
						float32 I = q.intensite;
						if (q.type == 1) {
							const float32 ca = -Dot(wi, Mk(q.dir[0], q.dir[1], q.dir[2]));
							I *= Max(0.f, Min(1.f, (ca - q.cosExt) / Max(1e-3f, (1.f - q.cosExt) * 0.15f)));
						}
						const V3 f = EvalBsdf(g, S, wo, wi);
						if (I > 0.f && Lum(f) > 0.f) {
							++c.rayons;
							const V3 tr = Transmission(g, o, wi, dl - 2e-3f);
							// estimateur RIS : f L / p_chap x (somme / M)
							const V3 cen = Mk(q.p[0], q.p[1], q.p[2]) - S.p;
							const float32 dc2 = Max(Dot(cen, cen), q.rayon * q.rayon + 1e-4f);
							float32 phy = q.intensite * Max(0.f, Dot(S.ns, cen * (1.f / Racine(dc2)))) / dc2;
							if (q.type == 1) {
								const float32 ca = -Dot(cen * (1.f / Racine(dc2)), Mk(q.dir[0], q.dir[1], q.dir[2]));
								phy *= Max(0.f, Min(1.f, (ca - q.cosExt) / Max(1e-3f, (1.f - q.cosExt) * 0.15f)));
							}
							if (phy > 0.f) {
								const float32 W = (somme / (float32)M) / phy;
								const V3 Li = Mk(q.couleur[0], q.couleur[1], q.couleur[2]) * (I / d2);
								L = L + Mul(Mul(f, Li), tr) * W;
							}
						}
					}
				}
				return L;
			}

			// UN chemin ; aov : albedo et normale du premier impact diffus, profondeur
			V3 Chemin(const Scene &g, Ctx &c, V3 o, V3 d, Rng &r, V3 &aAlb, V3 &aNrm, float32 &aZ) {
				V3 L{0.f, 0.f, 0.f}, beta{1.f, 1.f, 1.f};
				bool speculaire = true, aov = false;
				const int32 maxRebonds = c.r->unRebond ? 1 : c.r->rebonds;
				for (int32 rebond = 0; rebond <= maxRebonds; ++rebond) {
					Impact h;
					++c.rayons;
					if (!Tracer(g, o, d, 1e6f, h)) {
						if (!c.r->sansCiel)
							L = L + Mul(beta, Ciel(g, d));
						if (speculaire && !c.r->sansSoleil && Dot(d, c.soleil) >= c.cosSoleil)
							L = L + Mul(beta, c.soleilE * (1.f / (2.f * kPi * (1.f - c.cosSoleil))));
						if (!aov) {
							aAlb = V3{1.f, 1.f, 1.f};
							aNrm = d * -1.f;
							aZ = 1e4f;
							aov = true;
						}
						break;
					}
					Surface S;
					Surf(g, h, o, d, c.r->sansCartes, S);
					if (speculaire && Lum(S.emis) > 0.f)
						L = L + Mul(beta, S.emis);
					if (S.verre) {
						// dielectrique lisse n = 1,5 : Fresnel exact, reflexion ou refraction
						const bool entre = Dot(d, S.ng) < 0.f;
						const V3 n = entre ? S.ng : S.ng * -1.f;
						const float32 eta = entre ? 1.f / 1.5f : 1.5f;
						const float32 ci = -Dot(d, n);
						const float32 s2 = eta * eta * (1.f - ci * ci);
						float32 F = 1.f;
						V3 dt{0.f, 0.f, 0.f};
						if (s2 < 1.f) {
							const float32 ct = Racine(1.f - s2);
							const float32 rs = (eta * ci - ct) / (eta * ci + ct), rp = (ci - eta * ct) / (ci + eta * ct);
							F = 0.5f * (rs * rs + rp * rp);
							dt = d * eta + n * (eta * ci - ct);
						}
						if (r.F() < F) {
							d = d + n * (2.f * ci);
							o = S.p + n * 2e-3f;
						} else {
							d = Norm(dt);
							o = S.p - n * 2e-3f;
							beta = Mul(beta, S.albedo);
						}
						speculaire = true;
						continue;
					}
					if (!aov) {
						aAlb = S.albedo;
						aNrm = S.ns;
						aZ = h.t;
						aov = true;
					}
					const V3 wo = d * -1.f;
					V3 Ld = Directe(g, c, S, wo, r);
					if (rebond > 0) {
						const float32 l = Lum(Mul(beta, Ld));
						if (l > c.r->ecretage)
							Ld = Ld * (c.r->ecretage / l);
					}
					L = L + Mul(beta, Ld);
					if (rebond == maxRebonds)
						break;
					const V3 wi = TirerBsdf(S, wo, r);
					const float32 pdf = PdfBsdf(S, wo, wi);
					if (pdf <= 1e-8f)
						break;
					const V3 f = EvalBsdf(g, S, wo, wi);
					if (Lum(f) <= 0.f)
						break;
					beta = Mul(beta, f * (1.f / pdf));
					speculaire = S.rug < 0.08f;
					if (rebond >= 3) {
						const float32 q = Min(0.95f, Max(beta.x, Max(beta.y, beta.z)));
						if (r.F() > q)
							break;
						beta = beta * (1.f / q);
					}
					o = S.p + S.ng * 2e-3f;
					d = wi;
				}
				return L;
			}

			float32 Aces(float32 x) {
				const float32 a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
				const float32 y = (x * (a * x + b)) / (x * (c * x + d) + e);
				return y < 0.f ? 0.f : (y > 1.f ? 1.f : y);
			}
			uint8 Srgb8(float32 c) {
				const float32 s = c <= 0.0031308f ? 12.92f * c : 1.055f * (float32)math::NkPow((float64)c, 1.0 / 2.4) - 0.055f;
				return (uint8)(int32)(Min(1.f, Max(0.f, s)) * 255.f + 0.5f);
			}
			bool EcrirePng(const char *chemin, const NkVector<float32> &rgb, int32 L, int32 H, float32 expo) {
				NkImage im = NkImage::Create((uint32)L, (uint32)H, 4, 0u);
				if (!im.IsValid())
					return false;
				for (int32 y = 0; y < H; ++y) {
					uint8 *ln = im.RowPtr(y);
					for (int32 x = 0; x < L; ++x) {
						const usize i = ((usize)y * (usize)L + (usize)x) * 3;
						ln[x * 4 + 0] = Srgb8(Aces(rgb[i] * expo));
						ln[x * 4 + 1] = Srgb8(Aces(rgb[i + 1] * expo));
						ln[x * 4 + 2] = Srgb8(Aces(rgb[i + 2] * expo));
						ln[x * 4 + 3] = 255;
					}
				}
				return im.SavePNG(chemin);
			}

			// LE DEBRUITEUR : ondelette a trous a arretes preservees (Dammertz et al. 2010), sur
			// l'eclairement demodule, guide par normale, profondeur et variance de luminance.
			void Debruiter(NkVector<float32> &rgb, const NkVector<float32> &alb, const NkVector<float32> &nrm,
						   const NkVector<float32> &z, const NkVector<float32> &var, int32 L, int32 H) {
				const usize n = (usize)L * (usize)H;
				NkVector<float32> il, il2, va, va2;
				il.Resize(n * 3);
				il2.Resize(n * 3);
				va.Resize(n);
				va2.Resize(n);
				for (usize i = 0; i < n; ++i) {
					for (int32 k = 0; k < 3; ++k)
						il[i * 3 + (usize)k] = rgb[i * 3 + (usize)k] / Max(alb[i * 3 + (usize)k], 0.02f);
					va[i] = var[i];
				}
				const float32 h5[5] = {1.f / 16.f, 1.f / 4.f, 3.f / 8.f, 1.f / 4.f, 1.f / 16.f};
				threading::NkThreadPool &pool = threading::NkThreadPool::GetGlobal();
				for (int32 it = 0; it < 5; ++it) {
					const int32 pas = 1 << it;
					auto ligne = [&](nk_size yy) {
						const int32 y = (int32)yy;
						for (int32 x = 0; x < L; ++x) {
							const usize p = (usize)y * (usize)L + (usize)x;
							const V3 cp = Mk(il[p * 3], il[p * 3 + 1], il[p * 3 + 2]);
							const float32 lp = Lum(cp);
							const V3 np = Mk(nrm[p * 3], nrm[p * 3 + 1], nrm[p * 3 + 2]);
							const float32 zp = z[p];
							const float32 sl = 4.f * Racine(Max(va[p], 0.f)) + 1e-4f;
							V3 s{0.f, 0.f, 0.f};
							float32 ws = 0.f, vs = 0.f;
							for (int32 j = -2; j <= 2; ++j) {
								const int32 yq = y + j * pas;
								if (yq < 0 || yq >= H)
									continue;
								for (int32 i = -2; i <= 2; ++i) {
									const int32 xq = x + i * pas;
									if (xq < 0 || xq >= L)
										continue;
									const usize q = (usize)yq * (usize)L + (usize)xq;
									const V3 cq = Mk(il[q * 3], il[q * 3 + 1], il[q * 3 + 2]);
									const V3 nq = Mk(nrm[q * 3], nrm[q * 3 + 1], nrm[q * 3 + 2]);
									const float32 dn = Max(0.f, Dot(np, nq));
									float32 wn = dn * dn;
									wn = wn * wn;
									wn = wn * wn;
									wn = wn * wn; // dn^16
									const float32 wz = math::NkExp(-Abs(zp - z[q]) / Max(1e-3f, 0.03f * zp * (float32)pas));
									const float32 wl = math::NkExp(-Abs(lp - Lum(cq)) / sl);
									const float32 w = h5[i + 2] * h5[j + 2] * wn * wz * wl;
									s = s + cq * w;
									ws += w;
									vs += w * w * va[q];
								}
							}
							const float32 inv = ws > 1e-12f ? 1.f / ws : 0.f;
							il2[p * 3] = ws > 1e-12f ? s.x * inv : cp.x;
							il2[p * 3 + 1] = ws > 1e-12f ? s.y * inv : cp.y;
							il2[p * 3 + 2] = ws > 1e-12f ? s.z * inv : cp.z;
							va2[p] = ws > 1e-12f ? vs * inv * inv : va[p];
						}
					};
					pool.ParallelFor((nk_size)H, ligne, (nk_size)4);
					pool.Join();
					for (usize i = 0; i < n * 3; ++i)
						il[i] = il2[i];
					for (usize i = 0; i < n; ++i)
						va[i] = va2[i];
				}
				for (usize i = 0; i < n; ++i)
					for (int32 k = 0; k < 3; ++k)
						rgb[i * 3 + (usize)k] = il[i * 3 + (usize)k] * Max(alb[i * 3 + (usize)k], 0.02f);
			}

			// Rendu dans des tampons (sert au banc et a NkTcRendre)
			struct Tampons {
					NkVector<float32> somme, alb, nrm, z, lum2;
					int32 spp = 0;
			};
			void Passe(const Scene &g, const NkTcCamera &cam, const NkTcReglages &r, Tampons &tb, int32 sppPasse,
					   int32 graine, uint64 &rayons) {
				const int32 L = cam.L, H = cam.H;
				const V3 oeil = Mk(cam.oeil[0], cam.oeil[1], cam.oeil[2]);
				const V3 fw = Norm(Mk(cam.cible[0], cam.cible[1], cam.cible[2]) - oeil);
				const V3 rt = Norm(Cross(fw, V3{0.f, 1.f, 0.f}));
				const V3 up = Cross(rt, fw);
				const float32 th = 18.f / Max(1.f, cam.focaleMM); // tan du demi-angle vertical
				const float32 asp = (float32)L / (float32)H;
				const float32 az = g.s.soleilDir[1];
				(void)az;
				threading::NkThreadPool &pool = threading::NkThreadPool::GetGlobal();
				NkVector<uint64> rl, rn, rno, rtr;
				rl.Resize((usize)H);
				rn.Resize((usize)H);
				rno.Resize((usize)H);
				rtr.Resize((usize)H);
				auto ligne = [&](nk_size yy) {
					const int32 y = (int32)yy;
					Ctx c;
					c.r = &r;
					c.soleil = Mk(g.s.soleilDir[0], g.s.soleilDir[1], g.s.soleilDir[2]);
					c.soleilE = Mk(g.s.soleilCouleur[0], g.s.soleilCouleur[1], g.s.soleilCouleur[2]) * g.s.soleilE;
					c.cosSoleil = 0.99998918f; // demi-angle 0,2665 deg
					c.rayons = 0;
					c.nonFinis = 0;
					tNoeuds = 0;
					tTris = 0;
					for (int32 x = 0; x < L; ++x) {
						const usize p = (usize)y * (usize)L + (usize)x;
						Rng rng{Melange(((uint64)p << 20) ^ ((uint64)graine * 0x9E3779B97F4A7C15ULL) ^ 0x1234567ULL)};
						for (int32 sm = 0; sm < sppPasse; ++sm) {
							const float32 px = ((float32)x + rng.F()) / (float32)L * 2.f - 1.f;
							const float32 py = 1.f - ((float32)y + rng.F()) / (float32)H * 2.f;
							const V3 d = Norm(fw + rt * (px * th * asp) + up * (py * th));
							V3 a{1.f, 1.f, 1.f}, nn{0.f, 1.f, 0.f};
							float32 zz = 1e4f;
							V3 Lc = Chemin(g, c, oeil, d, rng, a, nn, zz);
							// un echantillon non fini (NaN, infini) est ecarte et compte : sinon il noircit
							// le pixel apres le rendu des tons (points noirs lus sur `tc1`)
							if (!(Lc.x == Lc.x && Lc.y == Lc.y && Lc.z == Lc.z) || Lc.x > 1e20f || Lc.y > 1e20f ||
								Lc.z > 1e20f) {
								Lc = V3{0.f, 0.f, 0.f};
								++c.nonFinis;
							}
							tb.somme[p * 3] += Lc.x;
							tb.somme[p * 3 + 1] += Lc.y;
							tb.somme[p * 3 + 2] += Lc.z;
							tb.alb[p * 3] += a.x;
							tb.alb[p * 3 + 1] += a.y;
							tb.alb[p * 3 + 2] += a.z;
							tb.nrm[p * 3] += nn.x;
							tb.nrm[p * 3 + 1] += nn.y;
							tb.nrm[p * 3 + 2] += nn.z;
							tb.z[p] += zz;
							const float32 l = Lum(Lc);
							tb.lum2[p] += l * l;
						}
					}
					rl[(usize)y] = c.rayons;
					rn[(usize)y] = c.nonFinis;
					rno[(usize)y] = tNoeuds;
					rtr[(usize)y] = tTris;
				};
				// NK_TRACEUR_SERIE=1 : un seul fil (mesure : le gain du parallele, et la cadence d'UN fil)
				static const char *sSerie = env::GetEnvVar("NK_TRACEUR_SERIE");
				if (sSerie && sSerie[0] == '1') {
					for (int32 y = 0; y < H; ++y)
						ligne((nk_size)y);
				} else {
					pool.ParallelFor((nk_size)H, ligne, (nk_size)1);
					pool.Join();
				}
				for (usize i = 0; i < rl.Size(); ++i) {
					rayons += rl[i];
					gNonFinis += rn[i];
					gNoeuds += rno[i];
					gTris += rtr[i];
				}
				tb.spp += sppPasse;
			}
			void InitTampons(Tampons &tb, int32 L, int32 H) {
				const usize n = (usize)L * (usize)H;
				tb.somme.Resize(n * 3);
				tb.alb.Resize(n * 3);
				tb.nrm.Resize(n * 3);
				tb.z.Resize(n);
				tb.lum2.Resize(n);
				for (usize i = 0; i < n * 3; ++i) {
					tb.somme[i] = 0.f;
					tb.alb[i] = 0.f;
					tb.nrm[i] = 0.f;
				}
				for (usize i = 0; i < n; ++i) {
					tb.z[i] = 0.f;
					tb.lum2[i] = 0.f;
				}
				tb.spp = 0;
			}
			// moyenne ; variance de la moyenne (luminance)
			void Moyennes(const Tampons &tb, int32 L, int32 H, NkVector<float32> &rgb, NkVector<float32> &alb,
						  NkVector<float32> &nrm, NkVector<float32> &z, NkVector<float32> &var) {
				const usize n = (usize)L * (usize)H;
				const float32 inv = 1.f / (float32)(tb.spp > 0 ? tb.spp : 1);
				rgb.Resize(n * 3);
				alb.Resize(n * 3);
				nrm.Resize(n * 3);
				z.Resize(n);
				var.Resize(n);
				for (usize i = 0; i < n; ++i) {
					for (int32 k = 0; k < 3; ++k) {
						rgb[i * 3 + (usize)k] = tb.somme[i * 3 + (usize)k] * inv;
						alb[i * 3 + (usize)k] = tb.alb[i * 3 + (usize)k] * inv;
					}
					const V3 nn = Norm(Mk(tb.nrm[i * 3], tb.nrm[i * 3 + 1], tb.nrm[i * 3 + 2]));
					nrm[i * 3] = nn.x;
					nrm[i * 3 + 1] = nn.y;
					nrm[i * 3 + 2] = nn.z;
					z[i] = tb.z[i] * inv;
					const float32 m = Lum(Mk(rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]));
					var[i] = Max(0.f, tb.lum2[i] * inv - m * m) * inv;
				}
			}

			bool PreparerInterne(Scene &g, const char *dossierCartes) {
				g.prete = false;
				g.tris.Clear();
				g.noeuds.Clear();
				g.ordre.Clear();
				for (int32 i = 0; i < 256; ++i) {
					const float32 c = (float32)i / 255.f;
					g.lin[i] = c <= 0.04045f ? c / 12.92f : (float32)math::NkPow((float64)((c + 0.055f) / 1.055f), 2.4);
				}
				const usize nt = g.s.tri.Size() / 3;
				g.tris.Resize(nt);
				g.ordre.Resize(nt);
				const float32 *P = g.s.P.Data();
				for (usize i = 0; i < nt; ++i) {
					Tri t;
					t.i0 = g.s.tri[i * 3];
					t.i1 = g.s.tri[i * 3 + 1];
					t.i2 = g.s.tri[i * 3 + 2];
					t.v0 = Mk(P[t.i0 * 3], P[t.i0 * 3 + 1], P[t.i0 * 3 + 2]);
					t.e1 = Mk(P[t.i1 * 3], P[t.i1 * 3 + 1], P[t.i1 * 3 + 2]) - t.v0;
					t.e2 = Mk(P[t.i2 * 3], P[t.i2 * 3 + 1], P[t.i2 * 3 + 2]) - t.v0;
					t.mat = g.s.triMat[i];
					g.tris[i] = t;
					g.ordre[i] = (uint32)i;
				}
				if (nt > 0)
				{
					g.noeuds.PushBack(Noeud{});
					Construire(g, 0, 0, (int32)nt, 0);
				}
				// cartes de couleur (classes employees)
				for (int32 c = 0; c < kMatClasses; ++c) {
					g.cartes[c].w = 0;
					bool sert = false;
					for (usize m = 0; m < g.s.mats.Size(); ++m)
						if (g.s.mats[m].classe == c)
							sert = true;
					if (!sert || !dossierCartes || !NkMatiereDe(c).cartes)
						continue;
					char ch[600];
					NkMatiereChemin(c, dossierCartes, 0, ch, 600);
					NkImage im;
					if (!im.Load(ch, 4) || im.Width() <= 0)
						continue;
					Carte &k = g.cartes[c];
					k.w = im.Width();
					k.h = im.Height();
					k.rgb.Resize((usize)k.w * (usize)k.h * 3);
					float64 s[3] = {0.0, 0.0, 0.0};
					for (int32 y = 0; y < k.h; ++y) {
						const uint8 *ln = im.RowPtr(y);
						for (int32 x = 0; x < k.w; ++x)
							for (int32 a = 0; a < 3; ++a) {
								const uint8 v = ln[x * 4 + a];
								k.rgb[((usize)y * (usize)k.w + (usize)x) * 3 + (usize)a] = v;
								s[a] += g.lin[v];
							}
					}
					for (int32 a = 0; a < 3; ++a)
						k.moy[a] = Max(0.02f, (float32)(s[a] / ((float64)k.w * (float64)k.h)));
				}
				// sources : repartition selon la puissance
				g.cdfSources.Resize(g.s.sources.Size());
				g.puissanceTotale = 0.f;
				for (usize i = 0; i < g.s.sources.Size(); ++i) {
					g.puissanceTotale += Max(0.f, g.s.sources[i].puissance);
					g.cdfSources[i] = g.puissanceTotale;
				}
				g.prete = nt > 0;
				return g.prete;
			}
		} // namespace

		bool NkTcPreparer(const NkScArchviz &s, const char *dossierCartes, NkTcStats *stats) {
			const NkElapsedTime t0 = NkChrono::Now();
			Scene &g = G();
			g.s = s;
			const bool ok = PreparerInterne(g, dossierCartes);
			if (stats) {
				stats->msPreparation = (NkChrono::Now() - t0).ToMilliseconds();
				stats->fils = (int32)threading::NkThreadPool::GetGlobal().GetNumWorkers();
			}
			NkPrintfSortie("[traceur] PREPARE : %u triangles, %u noeuds de BVH, %u sources, %.0f ms\n", (unsigned)g.tris.Size(),
						   (unsigned)g.noeuds.Size(), (unsigned)g.s.sources.Size(), (NkChrono::Now() - t0).ToMilliseconds());
			return ok;
		}

		static bool Finaliser(const NkTcCamera &cam, const NkTcReglages &r, const char *png, const Tampons &tb, float64 msRendu,
							  uint64 rayons, const char *materiel, NkTcStats *stats);

		bool NkTcRendre(const NkTcCamera &cam, const NkTcReglages &r, const char *png, NkTcStats *stats) {
			Scene &g = G();
			if (!g.prete)
				return false;
			Tampons tb;
			InitTampons(tb, cam.L, cam.H);
			const NkElapsedTime t0 = NkChrono::Now();
			uint64 rayons = 0;
			int32 passe = 0;
			gNonFinis = 0;
			gNoeuds = 0;
			gTris = 0;
			while (tb.spp < r.spp) {
				const int32 n = (r.spp - tb.spp) < 4 ? (r.spp - tb.spp) : 4;
				Passe(g, cam, r, tb, n, passe++, rayons);
				if (r.secondesMax > 0.f && (NkChrono::Now() - t0).ToMilliseconds() > (float64)r.secondesMax * 1000.0)
					break;
			}
			const float64 msRendu = (NkChrono::Now() - t0).ToMilliseconds();
			return Finaliser(cam, r, png, tb, msRendu, rayons, "processeur", stats);
		}

		// LA FIN COMMUNE au processeur et a la carte graphique : moyennes, exposition, image brute,
		// debruiteur, image finale, compte rendu.
		static bool Finaliser(const NkTcCamera &cam, const NkTcReglages &r, const char *png, const Tampons &tb, float64 msRendu,
							  uint64 rayons, const char *materiel, NkTcStats *stats) {
			Scene &g = G();
			NkVector<float32> rgb, alb, nrm, z, var;
			Moyennes(tb, cam.L, cam.H, rgb, alb, nrm, z, var);
			// l'exposition
			float32 expo = 1000.f / (1.2f * (float32)math::NkPow(2.0, (float64)g.s.expositionEV));
			if (r.expoAuto) {
				float64 sl = 0.0;
				const usize n = (usize)cam.L * (usize)cam.H;
				for (usize i = 0; i < n; ++i)
					sl += math::NkLog((float64)Lum(Mk(rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2])) + 1e-4);
				expo = 0.18f / (float32)math::NkExp(sl / (float64)n);
			}
			char brut[600];
			{
				int32 n = 0;
				while (png[n])
					++n;
				int32 k = n > 4 ? n - 4 : n;
				NkSnprintf(brut, sizeof(brut), "%.*s_brut.png", (int)k, png);
			}
			bool ok = EcrirePng(brut, rgb, cam.L, cam.H, expo);
			const NkElapsedTime t1 = NkChrono::Now();
			if (r.debruiter)
				Debruiter(rgb, alb, nrm, z, var, cam.L, cam.H);
			const float64 msDeb = (NkChrono::Now() - t1).ToMilliseconds();
			ok = EcrirePng(png, rgb, cam.L, cam.H, expo) && ok;
			if (stats) {
				stats->msRendu = msRendu;
				stats->msDebruitage = msDeb;
				stats->sppFaits = tb.spp;
				stats->rayons = rayons;
				stats->fils = (int32)threading::NkThreadPool::GetGlobal().GetNumWorkers();
			}
			NkPrintfSortie("[traceur] RENDU (%s) %s : %dx%d, %d ech./pixel, %.1f s de rendu + %.1f s de debruitage, %.1f Mrayons/s, %d fils, "
						   "exposition x%.3f (%s), %llu echantillon(s) non fini(s) ecarte(s) ; par rayon : %.1f noeuds, %.1f triangles\n",
						   materiel, png, (int)cam.L, (int)cam.H, (int)tb.spp, msRendu / 1000.0, msDeb / 1000.0,
						   msRendu > 0.0 ? (float64)rayons / msRendu / 1000.0 : 0.0,
						   (int)threading::NkThreadPool::GetGlobal().GetNumWorkers(), (double)expo,
						   r.expoAuto ? "auto" : "EV de la scene", (unsigned long long)gNonFinis,
						   rayons ? (float64)gNoeuds / (float64)rayons : 0.0, rayons ? (float64)gTris / (float64)rayons : 0.0);
			NkFflush(NkFlux::Sortie);
			return ok;
		}

		// =====================================================================
		//  POUR LA CARTE GRAPHIQUE : la scene preparee, a plat (NkTraceurGPU.cpp)
		// =====================================================================
		bool NkTcEmpaqueter(NkTcPaquet &p) {
			Scene &g = G();
			if (!g.prete)
				return false;
			p.scene.Clear();
			p.cartes.Clear();
			auto pousse = [&](float32 a, float32 b, float32 c, float32 d) {
				p.scene.PushBack(a);
				p.scene.PushBack(b);
				p.scene.PushBack(c);
				p.scene.PushBack(d);
			};
			// noeuds : (min, g), (max, nb) ; les indices tiennent dans un flottant (< 2^24)
			p.offNoeuds = 0;
			for (usize i = 0; i < g.noeuds.Size(); ++i) {
				const Noeud &n = g.noeuds[i];
				pousse(n.mn[0], n.mn[1], n.mn[2], (float32)n.g);
				pousse(n.mx[0], n.mx[1], n.mx[2], (float32)n.nb);
			}
			// triangles DANS L'ORDRE DES FEUILLES : (v0, mat), (e1, i0), (e2, i1), (i2, -, -, -)
			p.offTris = (int32)(p.scene.Size() / 4);
			for (usize i = 0; i < g.ordre.Size(); ++i) {
				const Tri &t = g.tris[g.ordre[i]];
				pousse(t.v0.x, t.v0.y, t.v0.z, (float32)t.mat);
				pousse(t.e1.x, t.e1.y, t.e1.z, (float32)t.i0);
				pousse(t.e2.x, t.e2.y, t.e2.z, (float32)t.i1);
				pousse((float32)t.i2, 0.f, 0.f, 0.f);
			}
			// sommets : (normale, u), (v, -, -, -)
			p.offSommets = (int32)(p.scene.Size() / 4);
			const usize nv = g.s.N.Size() / 3;
			for (usize i = 0; i < nv; ++i) {
				pousse(g.s.N[i * 3], g.s.N[i * 3 + 1], g.s.N[i * 3 + 2], g.s.UV[i * 2]);
				pousse(g.s.UV[i * 2 + 1], 0.f, 0.f, 0.f);
			}
			// matieres : (couleur, rugosite), (metal, transmission, emission, classe), (tuile, couleur propre, -, -),
			// (moyenne de la carte, -)
			p.offMats = (int32)(p.scene.Size() / 4);
			for (usize i = 0; i < g.s.mats.Size(); ++i) {
				const NkScMateriau &m = g.s.mats[i];
				const bool carte = m.classe >= 0 && m.classe < kMatClasses && g.cartes[m.classe].w > 0;
				pousse(m.couleur[0], m.couleur[1], m.couleur[2], m.rugosite);
				pousse(m.metal, m.transmission, m.emission, carte ? (float32)m.classe : -1.f);
				pousse(m.tuile, carte && NkMatiereDe(m.classe).couleurPropre ? 1.f : 0.f, 0.f, 0.f);
				if (carte)
					pousse(g.cartes[m.classe].moy[0], g.cartes[m.classe].moy[1], g.cartes[m.classe].moy[2], 0.f);
				else
					pousse(1.f, 1.f, 1.f, 0.f);
			}
			// sources : (p, type), (axe, cos), (couleur, intensite), (rayon, puissance, repartition cumulee, -)
			p.offSources = (int32)(p.scene.Size() / 4);
			p.nSources = (int32)g.s.sources.Size();
			for (usize i = 0; i < g.s.sources.Size(); ++i) {
				const NkScSource &q = g.s.sources[i];
				pousse(q.p[0], q.p[1], q.p[2], (float32)q.type);
				pousse(q.dir[0], q.dir[1], q.dir[2], q.cosExt);
				pousse(q.couleur[0], q.couleur[1], q.couleur[2], q.intensite);
				pousse(q.rayon, q.puissance, g.cdfSources[i], 0.f);
			}
			p.puissanceTotale = g.puissanceTotale;
			// cartes : RGBA8 empaquete (r en poids faible)
			for (int32 c = 0; c < kMatClasses; ++c) {
				p.carteOff[c] = -1;
				p.carteW[c] = 0;
				p.carteH[c] = 0;
				const Carte &k = g.cartes[c];
				if (k.w <= 0)
					continue;
				p.carteOff[c] = (int32)p.cartes.Size();
				p.carteW[c] = k.w;
				p.carteH[c] = k.h;
				const usize n = (usize)k.w * (usize)k.h;
				for (usize i = 0; i < n; ++i)
					p.cartes.PushBack((uint32)k.rgb[i * 3] | ((uint32)k.rgb[i * 3 + 1] << 8) | ((uint32)k.rgb[i * 3 + 2] << 16));
			}
			if (p.cartes.Empty())
				p.cartes.PushBack(0u);
			// l'ambiance
			for (int32 a = 0; a < 3; ++a) {
				p.soleilDir[a] = g.s.soleilDir[a];
				p.soleilE[a] = g.s.soleilCouleur[a] * g.s.soleilE;
				p.zenith[a] = g.s.cielZenith[a];
				p.horizon[a] = g.s.cielHorizon[a];
			}
			p.cielIntensite = g.s.cielIntensite;
			return true;
		}

		bool NkTcFinaliserSommes(const NkTcCamera &cam, const NkTcReglages &r, const char *png, const float32 *o4, int32 spp,
								 float64 msRendu, uint64 rayons, NkTcStats *stats) {
			if (!G().prete || !o4)
				return false;
			const usize n = (usize)cam.L * (usize)cam.H;
			Tampons tb;
			InitTampons(tb, cam.L, cam.H);
			for (usize i = 0; i < n; ++i) {
				for (int32 k = 0; k < 3; ++k) {
					tb.somme[i * 3 + (usize)k] = o4[i * 4 + (usize)k];
					tb.alb[i * 3 + (usize)k] = o4[(n + i) * 4 + (usize)k];
					tb.nrm[i * 3 + (usize)k] = o4[(2 * n + i) * 4 + (usize)k];
				}
				tb.z[i] = o4[(3 * n + i) * 4];
				tb.lum2[i] = o4[(3 * n + i) * 4 + 1];
			}
			tb.spp = spp;
			gNonFinis = 0;
			gNoeuds = 0;
			gTris = 0;
			return Finaliser(cam, r, png, tb, msRendu, rayons, "carte graphique", stats);
		}

		// =====================================================================
		//  LE BANC DE JUSTESSE (sans scene externe)
		// =====================================================================
		namespace {
			// sphere (icosaedre subdivise) de rayon R au centre c, normales sortantes
			void AjouterSphere(NkScArchviz &s, const float32 *c, float32 R, int32 mat, int32 subdiv) {
				const float32 t = (1.f + math::NkSqrt(5.f)) * 0.5f;
				NkVector<V3> v;
				const float32 base[12][3] = {{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0}, {0, -1, t}, {0, 1, t},
											 {0, -1, -t}, {0, 1, -t}, {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}};
				for (int32 i = 0; i < 12; ++i)
					v.PushBack(Norm(Mk(base[i][0], base[i][1], base[i][2])));
				NkVector<int32> f;
				const int32 F[20][3] = {{0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11}, {1, 5, 9}, {5, 11, 4},
										{11, 10, 2}, {10, 7, 6}, {7, 1, 8}, {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8},
										{3, 8, 9}, {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}};
				for (int32 i = 0; i < 20; ++i)
					for (int32 k = 0; k < 3; ++k)
						f.PushBack(F[i][k]);
				for (int32 sd = 0; sd < subdiv; ++sd) {
					NkVector<int32> f2;
					for (usize i = 0; i < f.Size(); i += 3) {
						const int32 a = f[i], b = f[i + 1], cc = f[i + 2];
						const int32 ab = (int32)v.Size();
						v.PushBack(Norm((v[(usize)a] + v[(usize)b]) * 0.5f));
						const int32 bc = (int32)v.Size();
						v.PushBack(Norm((v[(usize)b] + v[(usize)cc]) * 0.5f));
						const int32 ca = (int32)v.Size();
						v.PushBack(Norm((v[(usize)cc] + v[(usize)a]) * 0.5f));
						const int32 q[12] = {a, ab, ca, b, bc, ab, cc, ca, bc, ab, bc, ca};
						for (int32 k = 0; k < 12; ++k)
							f2.PushBack(q[k]);
					}
					f = f2;
				}
				for (usize i = 0; i < f.Size(); i += 3) {
					const uint32 b0 = (uint32)(s.P.Size() / 3);
					for (int32 k = 0; k < 3; ++k) {
						const V3 n = v[(usize)f[i + (usize)k]];
						s.P.PushBack(c[0] + n.x * R);
						s.P.PushBack(c[1] + n.y * R);
						s.P.PushBack(c[2] + n.z * R);
						s.N.PushBack(n.x);
						s.N.PushBack(n.y);
						s.N.PushBack(n.z);
						s.UV.PushBack(0.f);
						s.UV.PushBack(0.f);
					}
					s.tri.PushBack(b0);
					s.tri.PushBack(b0 + 1u);
					s.tri.PushBack(b0 + 2u);
					s.triMat.PushBack(mat);
				}
			}
			// moyenne de la radiance (non tonalisee) sur les pixels du centre (carre de cote 1/5)
			float32 MoyenneCentre(const Tampons &tb, int32 L, int32 H) {
				float64 s = 0.0;
				int32 n = 0;
				for (int32 y = H * 2 / 5; y < H * 3 / 5; ++y)
					for (int32 x = L * 2 / 5; x < L * 3 / 5; ++x) {
						const usize p = (usize)y * (usize)L + (usize)x;
						s += Lum(Mk(tb.somme[p * 3], tb.somme[p * 3 + 1], tb.somme[p * 3 + 2])) / (float32)tb.spp;
						++n;
					}
				return (float32)(s / (float64)(n > 0 ? n : 1));
			}
			float32 Furnace(bool verre, bool mutation, int32 spp) {
				Scene &g = G();
				NkScArchviz s;
				NkScMateriau m;
				m.couleur[0] = m.couleur[1] = m.couleur[2] = 0.8f;
				m.rugosite = 1.f;
				if (verre) {
					m.couleur[0] = m.couleur[1] = m.couleur[2] = 1.f;
					m.transmission = 1.f;
				}
				s.mats.PushBack(m);
				const float32 c[3] = {0.f, 0.f, 0.f};
				AjouterSphere(s, c, 1.f, 0, 3);
				s.cielZenith[0] = s.cielZenith[1] = s.cielZenith[2] = 1.f;
				s.cielHorizon[0] = s.cielHorizon[1] = s.cielHorizon[2] = 1.f;
				s.cielIntensite = 1.f;
				s.soleilE = 0.f;
				g.s = s;
				g.mutationBanc = mutation;
				PreparerInterne(g, nullptr);
				NkTcCamera cam;
				cam.oeil[0] = 0.f;
				cam.oeil[1] = 0.f;
				cam.oeil[2] = 4.f;
				cam.cible[0] = cam.cible[1] = cam.cible[2] = 0.f;
				cam.focaleMM = 50.f;
				cam.L = cam.H = 64;
				NkTcReglages r;
				r.ecretage = 1e9f;
				r.rebonds = 32;
				Tampons tb;
				InitTampons(tb, cam.L, cam.H);
				uint64 ry = 0;
				Passe(g, cam, r, tb, spp, 7, ry);
				g.mutationBanc = false;
				return MoyenneCentre(tb, cam.L, cam.H);
			}
			float32 PointSurPlan(int32 spp) {
				// plan lambertien (albedo 0,5) a y = 0, source ponctuelle P = 1 a h = 2 m : E = P/(4 pi h^2)
				Scene &g = G();
				NkScArchviz s;
				NkScMateriau m;
				m.couleur[0] = m.couleur[1] = m.couleur[2] = 0.5f;
				m.rugosite = 1.f;
				s.mats.PushBack(m);
				const float32 h = 50.f;
				const float32 c4[4][3] = {{-h, 0.f, -h}, {h, 0.f, -h}, {h, 0.f, h}, {-h, 0.f, h}};
				const int32 ord[6] = {0, 2, 1, 0, 3, 2};
				for (int32 t2 = 0; t2 < 2; ++t2) {
					const uint32 b0 = (uint32)(s.P.Size() / 3);
					for (int32 j = 0; j < 3; ++j) {
						for (int32 a = 0; a < 3; ++a)
							s.P.PushBack(c4[ord[t2 * 3 + j]][a]);
						s.N.PushBack(0.f);
						s.N.PushBack(1.f);
						s.N.PushBack(0.f);
						s.UV.PushBack(0.f);
						s.UV.PushBack(0.f);
					}
					s.tri.PushBack(b0);
					s.tri.PushBack(b0 + 1u);
					s.tri.PushBack(b0 + 2u);
					s.triMat.PushBack(0);
				}
				NkScSource q;
				q.p[0] = 0.f;
				q.p[1] = 2.f;
				q.p[2] = 0.f;
				q.rayon = 0.001f;
				q.puissance = 1.f;
				q.intensite = 1.f / (4.f * kPi);
				s.sources.PushBack(q);
				s.cielIntensite = 0.f;
				s.soleilE = 0.f;
				g.s = s;
				PreparerInterne(g, nullptr);
				NkTcCamera cam;
				cam.oeil[0] = 0.f;
				cam.oeil[1] = 1.f;
				cam.oeil[2] = 0.001f;
				cam.cible[0] = 0.f;
				cam.cible[1] = 0.f;
				cam.cible[2] = 0.f;
				cam.focaleMM = 400.f; // tout petit champ : le pixel central voit le pied de la source
				cam.L = cam.H = 16;
				NkTcReglages r;
				r.rebonds = 1;
				r.ecretage = 1e9f;
				Tampons tb;
				InitTampons(tb, cam.L, cam.H);
				uint64 ry = 0;
				Passe(g, cam, r, tb, spp, 3, ry);
				return MoyenneCentre(tb, cam.L, cam.H);
			}
		} // namespace

		int32 NkTcBanc() {
			int32 echecs = 0;
			// (1) FOUR BLANC : sphere lambertienne d'albedo 0,8 sous un ciel uniforme de radiance 1 -> 0,8
			const float32 f1 = Furnace(false, false, 64);
			// tolerance 4 % : la couche speculaire dielectrique (F0 = 0,04) prend et rend quelques pourcents
			const bool ok1 = Abs(f1 - 0.8f) < 0.032f;
			// contre-epreuve : Lambert sans 1/pi -> 0,8 pi
			const float32 f1m = Furnace(false, true, 16);
			const bool ok1m = Abs(f1m - 0.8f) > 0.5f;
			// (2) FOUR BLANC DU VERRE : sphere de verre (sans absorption) -> 1
			const float32 f2 = Furnace(true, false, 256);
			const bool ok2 = Abs(f2 - 1.f) < 0.03f;
			// (3) SOURCE PONCTUELLE : E = P/(4 pi h^2) = 1/(16 pi) ; L = albedo E / pi
			const float32 f3 = PointSurPlan(64);
			const float32 att3 = 0.5f * (1.f / (16.f * kPi)) / kPi;
			const bool ok3 = Abs(f3 / att3 - 1.f) < 0.05f; // 5 % : meme couche speculaire
			if (!ok1)
				++echecs;
			if (!ok1m)
				++echecs;
			if (!ok2)
				++echecs;
			if (!ok3)
				++echecs;
			NkPrintfSortie("[traceur] BANC four blanc lambertien : %.4f (attendu 0,8 a 4 %%) %s\n", (double)f1, ok1 ? "VERT" : "ROUGE");
			NkPrintfSortie("[traceur] BANC contre-epreuve (Lambert sans 1/pi) : %.4f (doit s'ecarter de 0,8) %s\n", (double)f1m,
						   ok1m ? "VERT (le banc voit la faute)" : "ROUGE (le banc ne voit rien)");
			NkPrintfSortie("[traceur] BANC four blanc du verre : %.4f (attendu 1 a 3 %%) %s\n", (double)f2, ok2 ? "VERT" : "ROUGE");
			NkPrintfSortie("[traceur] BANC source ponctuelle sur plan : %.6f (attendu %.6f a 5 %%) %s\n", (double)f3, (double)att3,
						   ok3 ? "VERT" : "ROUGE");
			NkPrintfSortie("[traceur] BANC : %d echec(s)\n", (int)echecs);
			NkFflush(NkFlux::Sortie);
			G().prete = false;
			return echecs;
		}

	} // namespace renderer
} // namespace nkentseu
