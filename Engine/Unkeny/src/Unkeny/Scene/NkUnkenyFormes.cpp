// -----------------------------------------------------------------------------
// FICHIER: Unkeny/Scene/NkUnkenyFormes.cpp
// DESCRIPTION: La geometrie des formes 2D et leur collisionneur assorti
//              (voir NkUnkenyFormes.h). Aucun dessin ici.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Unkeny/Scene/NkUnkenyFormes.h"

#include "NKContainers/Sequential/NkVector.h"

#include <cstddef>

namespace nkentseu {
	namespace unkeny {

		namespace {
			constexpr float32 PI = 3.14159265358979f;
			/// Les segments d'une courbe pleine (cercle, ellipse) : assez pour
			/// qu'un cercle de 400 px reste rond a l'oeil.
			constexpr uint32 SEGMENTS_COURBE = 64u;

			float32 Max(float32 a, float32 b) noexcept {
				return a > b ? a : b;
			}
			float32 Min(float32 a, float32 b) noexcept {
				return a < b ? a : b;
			}
			float32 Abs(float32 a) noexcept {
				return a < 0.f ? -a : a;
			}

			/// Ajoute un point s'il reste de la place.
			void Ajouter(NkVec2f *s, uint32 cap, uint32 &n, const NkVec2f &p) noexcept {
				if (n < cap) {
					s[n++] = p;
				}
			}

			/// Les points par defaut d'une ligne (un trait horizontal) et d'un
			/// polygone libre (une maison, CONCAVE expres : un toit en V renverse).
			uint32 PointsOuDefaut(const NkRenduForme2D &f, const NkVec2f *&pts, NkVec2f *tampon) noexcept {
				const uint32 n = f.nbPoints < NK_FORME_POINTS_MAX ? f.nbPoints : NK_FORME_POINTS_MAX;
				if (f.genre == NkGenreForme2D::NK_LIGNE) {
					if (n >= 2u) {
						pts = f.points;
						return n;
					}
					tampon[0] = NkVec2f(-f.taille.x * 0.5f, 0.f);
					tampon[1] = NkVec2f(f.taille.x * 0.5f, 0.f);
					pts = tampon;
					return 2u;
				}
				if (n >= 3u) {
					pts = f.points;
					return n;
				}
				const float32 hx = f.taille.x * 0.5f, hy = f.taille.y * 0.5f;
				tampon[0] = NkVec2f(-hx, -hy);
				tampon[1] = NkVec2f(hx, -hy);
				tampon[2] = NkVec2f(hx, hy);
				tampon[3] = NkVec2f(-hx, hy);
				pts = tampon;
				return 4u;
			}

			/// Le contour d'une ellipse de demi-axes (rx, ry), `n` segments.
			uint32 Ellipse(float32 rx, float32 ry, uint32 n, NkVec2f *s, uint32 cap) noexcept {
				uint32 k = 0;
				for (uint32 i = 0; i < n; ++i) {
					const float32 a = 2.f * PI * static_cast<float32>(i) / static_cast<float32>(n);
					Ajouter(s, cap, k, NkVec2f(rx * math::NkCos(a), ry * math::NkSin(a)));
				}
				return k;
			}

			/// Le contour d'une capsule inscrite dans (w, h) : le long du grand axe.
			uint32 Capsule(float32 w, float32 h, uint32 parDemi, NkVec2f *s, uint32 cap) noexcept {
				uint32 k = 0;
				const bool couchee = w >= h;
				const float32 r = (couchee ? h : w) * 0.5f;
				const float32 a = Max(0.f, (couchee ? w : h) * 0.5f - r);
				for (uint32 i = 0; i <= parDemi; ++i) {
					const float32 t = -PI * 0.5f + PI * static_cast<float32>(i) / static_cast<float32>(parDemi);
					const NkVec2f p(a + r * math::NkCos(t), r * math::NkSin(t));
					Ajouter(s, cap, k, couchee ? p : NkVec2f(-p.y, p.x));
				}
				for (uint32 i = 0; i <= parDemi; ++i) {
					const float32 t = PI * 0.5f + PI * static_cast<float32>(i) / static_cast<float32>(parDemi);
					const NkVec2f p(-a + r * math::NkCos(t), r * math::NkSin(t));
					Ajouter(s, cap, k, couchee ? p : NkVec2f(-p.y, p.x));
				}
				return k;
			}

			/// Arrondit les coins d'un contour ferme : chaque coin devient un arc
			/// TANGENT aux deux aretes (le meme geste que l'arrondi d'un rectangle
			/// de NKGui, generalise). Le rayon est borne, coin par coin, a ce que
			/// les deux aretes peuvent porter ; un coin plat est garde tel quel.
			/// Juste aussi pour un coin RENTRANT (celui d'une etoile) : l'arc est
			/// alors de l'autre cote, un conge.
			uint32 Arrondir(const NkVec2f *p, uint32 n, float32 rayon, NkVec2f *s, uint32 cap) noexcept {
				uint32 k = 0;
				if (n < 3u || rayon <= 0.f) {
					for (uint32 i = 0; i < n; ++i) {
						Ajouter(s, cap, k, p[i]);
					}
					return k;
				}
				// Moins de segments par coin quand il y a beaucoup de coins.
				uint32 parCoin = cap / (n + 1u);
				parCoin = parCoin > 8u ? 8u : (parCoin < 2u ? 2u : parCoin);
				for (uint32 i = 0; i < n; ++i) {
					const NkVec2f &P = p[i];
					const NkVec2f &A = p[(i + n - 1u) % n];
					const NkVec2f &B = p[(i + 1u) % n];
					NkVec2f u(A.x - P.x, A.y - P.y), v(B.x - P.x, B.y - P.y);
					const float32 lu = math::NkSqrt(u.x * u.x + u.y * u.y);
					const float32 lv = math::NkSqrt(v.x * v.x + v.y * v.y);
					if (lu < 1.0e-6f || lv < 1.0e-6f) {
						Ajouter(s, cap, k, P);
						continue;
					}
					u = NkVec2f(u.x / lu, u.y / lu);
					v = NkVec2f(v.x / lv, v.y / lv);
					const float32 cosT = math::NkClamp(u.x * v.x + u.y * v.y, -1.f, 1.f);
					const float32 theta = math::NkAcos(cosT);
					const float32 demi = theta * 0.5f;
					if (demi < 1.0e-3f || PI - theta < 1.0e-3f) {
						Ajouter(s, cap, k, P); // coin plat ou en epingle : garde
						continue;
					}
					const float32 tanD = math::NkTan(demi);
					float32 d = rayon / tanD; // distance du coin aux points de tangence
					const float32 dMax = 0.5f * Min(lu, lv);
					d = d > dMax ? dMax : d;
					const float32 r = d * tanD;
					NkVec2f bis(u.x + v.x, u.y + v.y);
					const float32 lb = math::NkSqrt(bis.x * bis.x + bis.y * bis.y);
					if (lb < 1.0e-6f) {
						Ajouter(s, cap, k, P);
						continue;
					}
					bis = NkVec2f(bis.x / lb, bis.y / lb);
					const float32 h = r / math::NkSin(demi);
					const NkVec2f C(P.x + bis.x * h, P.y + bis.y * h);
					const NkVec2f T1(P.x + u.x * d, P.y + u.y * d);
					const NkVec2f T2(P.x + v.x * d, P.y + v.y * d);
					const float32 a1 = math::NkAtan2(T1.y - C.y, T1.x - C.x);
					float32 da = math::NkAtan2(T2.y - C.y, T2.x - C.x) - a1;
					while (da > PI) {
						da -= 2.f * PI;
					}
					while (da < -PI) {
						da += 2.f * PI;
					}
					for (uint32 j = 0; j <= parCoin; ++j) {
						const float32 a = a1 + da * static_cast<float32>(j) / static_cast<float32>(parCoin);
						Ajouter(s, cap, k, NkVec2f(C.x + r * math::NkCos(a), C.y + r * math::NkSin(a)));
					}
				}
				return k;
			}

			/// Le contour d'un TRAIT epais : les deux bords d'une ligne brisee,
			/// a +/- e/2 de chaque cote (jointures en onglet borne). C'est ce que
			/// la chaine du decor suit pour une ligne epaisse : une balle se pose
			/// SUR le trait, pas en son milieu.
			uint32 BordsDuTrait(const NkVec2f *p, uint32 n, float32 e, NkVec2f *s, uint32 cap) noexcept {
				uint32 k = 0;
				if (n < 2u) {
					return 0u;
				}
				const float32 h = e * 0.5f;
				auto Normale = [&](uint32 i) {
					// La normale moyenne des segments qui touchent le point i.
					NkVec2f nm(0.f, 0.f);
					for (int32 c = -1; c <= 0; ++c) {
						const int32 a = static_cast<int32>(i) + c;
						if (a < 0 || a + 1 >= static_cast<int32>(n)) {
							continue;
						}
						const NkVec2f d(p[a + 1].x - p[a].x, p[a + 1].y - p[a].y);
						const float32 l = math::NkSqrt(d.x * d.x + d.y * d.y);
						if (l > 1.0e-6f) {
							nm = NkVec2f(nm.x - d.y / l, nm.y + d.x / l);
						}
					}
					const float32 l = math::NkSqrt(nm.x * nm.x + nm.y * nm.y);
					return l > 1.0e-6f ? NkVec2f(nm.x / l, nm.y / l) : NkVec2f(0.f, 1.f);
				};
				for (uint32 i = 0; i < n; ++i) {
					const NkVec2f nm = Normale(i);
					Ajouter(s, cap, k, NkVec2f(p[i].x - nm.x * h, p[i].y - nm.y * h));
				}
				for (uint32 i = n; i-- > 0u;) {
					const NkVec2f nm = Normale(i);
					Ajouter(s, cap, k, NkVec2f(p[i].x + nm.x * h, p[i].y + nm.y * h));
				}
				return k;
			}

			bool PresqueRond(const NkRenduForme2D &f) noexcept {
				return Abs(f.taille.x - f.taille.y) <= 1.0e-3f * Max(f.taille.x, f.taille.y);
			}

			/// Un contour quelconque devient le collisionneur d'un corps qui
			/// bouge : son enveloppe convexe, 8 sommets au plus.
			void EnveloppeDynamique(const NkVec2f *pts, uint32 n, NkCollisionneur2D &c) noexcept {
				NkVec2f env[NK_FORME_CONTOUR_MAX];
				uint32 m = NkEnveloppeConvexe2D(pts, n, env, NK_FORME_CONTOUR_MAX);
				m = NkReduireConvexe2D(env, m, NK_POLYGONE_CONVEXE_MAX);
				c.forme = NkForme2D::NK_POLYGONE;
				c.nbSommets = static_cast<uint8>(m);
				c.boucle = true;
				for (uint32 i = 0; i < m; ++i) {
					c.sommets[i] = env[i];
				}
			}

			/// Le contour EXACT du decor : une chaine fermee (32 points au plus ;
			/// au-dela, on en prend un sur deux, puis un sur trois...).
			void ChaineFermee(const NkVec2f *pts, uint32 n, NkCollisionneur2D &c) noexcept {
				uint32 pas = 1u;
				while ((n + pas - 1u) / pas > NK_COLLISION_SOMMETS_MAX) {
					++pas;
				}
				uint32 m = 0;
				for (uint32 i = 0; i < n && m < NK_COLLISION_SOMMETS_MAX; i += pas) {
					c.sommets[m++] = pts[i];
				}
				c.forme = NkForme2D::NK_CHAINE;
				c.nbSommets = static_cast<uint8>(m);
				c.boucle = true;
			}

			void Polygone(const NkVec2f *pts, uint32 n, NkCollisionneur2D &c) noexcept {
				c.forme = NkForme2D::NK_POLYGONE;
				c.nbSommets = static_cast<uint8>(n);
				c.boucle = true;
				// Toujours dans le sens trigonometrique : la masse d'un polygone
				// parcouru a l'envers serait NEGATIVE (NkMassePolygone2D).
				const bool inverse = NkAireSignee2D(pts, n) < 0.f;
				for (uint32 i = 0; i < n; ++i) {
					c.sommets[i] = pts[inverse ? n - 1u - i : i];
				}
			}

			const NkChampSauve kChamps[] = {
				NK_UNKENY_CHAMP(NkRenduForme2D, genre, NkTypeChamp::NK_U8),
				NK_UNKENY_CHAMP(NkRenduForme2D, taille, NkTypeChamp::NK_VEC2),
				NK_UNKENY_CHAMP(NkRenduForme2D, remplissage, NkTypeChamp::NK_U32),
				NK_UNKENY_CHAMP(NkRenduForme2D, rempli, NkTypeChamp::NK_BOOL),
				NK_UNKENY_CHAMP(NkRenduForme2D, couleurContour, NkTypeChamp::NK_U32),
				NK_UNKENY_CHAMP(NkRenduForme2D, epaisseurContour, NkTypeChamp::NK_F32),
				NK_UNKENY_CHAMP(NkRenduForme2D, arrondi, NkTypeChamp::NK_F32),
				NK_UNKENY_CHAMP(NkRenduForme2D, opacite, NkTypeChamp::NK_F32),
				NK_UNKENY_CHAMP(NkRenduForme2D, cotes, NkTypeChamp::NK_U8),
				NK_UNKENY_CHAMP(NkRenduForme2D, branches, NkTypeChamp::NK_U8),
				NK_UNKENY_CHAMP(NkRenduForme2D, rayonInterieur, NkTypeChamp::NK_F32),
				NK_UNKENY_CHAMP(NkRenduForme2D, epaisseur, NkTypeChamp::NK_F32),
				NK_UNKENY_CHAMP(NkRenduForme2D, couche, NkTypeChamp::NK_I32),
				NK_UNKENY_CHAMP(NkRenduForme2D, visible, NkTypeChamp::NK_BOOL),
				NK_UNKENY_CHAMP(NkRenduForme2D, collisionSuit, NkTypeChamp::NK_BOOL),
				NK_UNKENY_CHAMP(NkRenduForme2D, nbPoints, NkTypeChamp::NK_U8),
				// Un TABLEAU de NkVec2f : `nombre` = 32, ecrit « x y x y ... ».
				NkChampSauve{"points", NkTypeChamp::NK_VEC2, static_cast<uint32>(offsetof(NkRenduForme2D, points)),
							 static_cast<uint32>(sizeof(NkVec2f)), NK_FORME_POINTS_MAX, 0u, false},
			};
		} // namespace

		// =====================================================================
		const char *NkNomGenreForme2D(NkGenreForme2D g) noexcept {
			switch (g) {
				case NkGenreForme2D::NK_RECTANGLE: return "Rectangle";
				case NkGenreForme2D::NK_CERCLE: return "Cercle";
				case NkGenreForme2D::NK_ELLIPSE: return "Ellipse";
				case NkGenreForme2D::NK_TRIANGLE: return "Triangle";
				case NkGenreForme2D::NK_ETOILE: return "Étoile";
				case NkGenreForme2D::NK_POLYGONE_REGULIER: return "Polygone régulier";
				case NkGenreForme2D::NK_CAPSULE: return "Capsule";
				case NkGenreForme2D::NK_LIGNE: return "Ligne / chaîne";
				case NkGenreForme2D::NK_POLYGONE_LIBRE: return "Polygone libre";
				default: return "Forme";
			}
		}

		NkRenduForme2D NkFormeParDefaut(NkGenreForme2D g) noexcept {
			NkRenduForme2D f;
			f.genre = g;
			switch (g) {
				case NkGenreForme2D::NK_RECTANGLE:
					f.taille = NkVec2f(1.6f, 1.f);
					f.remplissage = 0x4C9BE8FFu;
					break;
				case NkGenreForme2D::NK_CERCLE:
					f.remplissage = 0xE8784CFFu;
					break;
				case NkGenreForme2D::NK_ELLIPSE:
					f.taille = NkVec2f(1.6f, 0.9f);
					f.remplissage = 0xB06CE0FFu;
					break;
				case NkGenreForme2D::NK_TRIANGLE:
					f.remplissage = 0x5CC46BFFu;
					break;
				case NkGenreForme2D::NK_ETOILE:
					f.taille = NkVec2f(1.2f, 1.2f);
					f.remplissage = 0xF2C53DFFu;
					break;
				case NkGenreForme2D::NK_POLYGONE_REGULIER:
					f.remplissage = 0x3DC2C2FFu;
					f.cotes = 6;
					break;
				case NkGenreForme2D::NK_CAPSULE:
					f.taille = NkVec2f(1.6f, 0.7f);
					f.remplissage = 0xE85C8AFFu;
					break;
				case NkGenreForme2D::NK_LIGNE:
					f.taille = NkVec2f(2.f, 0.5f);
					f.remplissage = 0xDDE3EEFFu;
					f.epaisseur = 0.12f;
					f.nbPoints = 4;
					f.points[0] = NkVec2f(-1.f, -0.25f);
					f.points[1] = NkVec2f(-0.35f, 0.25f);
					f.points[2] = NkVec2f(0.35f, -0.25f);
					f.points[3] = NkVec2f(1.f, 0.25f);
					break;
				case NkGenreForme2D::NK_POLYGONE_LIBRE:
					// Une fleche : CONCAVE, ce qu'aucun autre genre ne donne.
					f.taille = NkVec2f(1.6f, 1.f);
					f.remplissage = 0x8FA4C2FFu;
					f.nbPoints = 7;
					f.points[0] = NkVec2f(-0.8f, -0.2f);
					f.points[1] = NkVec2f(0.2f, -0.2f);
					f.points[2] = NkVec2f(0.2f, -0.5f);
					f.points[3] = NkVec2f(0.8f, 0.f);
					f.points[4] = NkVec2f(0.2f, 0.5f);
					f.points[5] = NkVec2f(0.2f, 0.2f);
					f.points[6] = NkVec2f(-0.8f, 0.2f);
					break;
				default:
					break;
			}
			return f;
		}

		// =====================================================================
		uint32 NkSommetsForme2D(const NkRenduForme2D &f, NkVec2f *s, uint32 cap, bool &ferme) noexcept {
			ferme = true;
			uint32 k = 0;
			const float32 hx = f.taille.x * 0.5f, hy = f.taille.y * 0.5f;
			switch (f.genre) {
				case NkGenreForme2D::NK_RECTANGLE:
					Ajouter(s, cap, k, NkVec2f(-hx, -hy));
					Ajouter(s, cap, k, NkVec2f(hx, -hy));
					Ajouter(s, cap, k, NkVec2f(hx, hy));
					Ajouter(s, cap, k, NkVec2f(-hx, hy));
					return k;
				case NkGenreForme2D::NK_CERCLE: {
					const float32 r = Min(hx, hy);
					return Ellipse(r, r, SEGMENTS_COURBE, s, cap);
				}
				case NkGenreForme2D::NK_ELLIPSE:
					return Ellipse(hx, hy, SEGMENTS_COURBE, s, cap);
				case NkGenreForme2D::NK_TRIANGLE:
					// Isocele, CENTRE SUR SON CENTROIDE : le solveur prend l'origine
					// du corps pour centre de masse (NkMassePolygone2D), un triangle
					// centre sur sa boite tournerait autour d'un point faux.
					Ajouter(s, cap, k, NkVec2f(-hx, -f.taille.y / 3.f));
					Ajouter(s, cap, k, NkVec2f(hx, -f.taille.y / 3.f));
					Ajouter(s, cap, k, NkVec2f(0.f, f.taille.y * 2.f / 3.f));
					return k;
				case NkGenreForme2D::NK_ETOILE: {
					const uint32 b = f.branches < 3u ? 3u : (f.branches > 16u ? 16u : f.branches);
					const float32 ri = math::NkClamp(f.rayonInterieur, 0.05f, 1.f);
					for (uint32 i = 0; i < 2u * b; ++i) {
						const float32 a = PI * 0.5f + PI * static_cast<float32>(i) / static_cast<float32>(b);
						const float32 r = (i % 2u) == 0u ? 1.f : ri;
						Ajouter(s, cap, k, NkVec2f(hx * r * math::NkCos(a), hy * r * math::NkSin(a)));
					}
					return k;
				}
				case NkGenreForme2D::NK_POLYGONE_REGULIER: {
					const uint32 n = f.cotes < 3u ? 3u : (f.cotes > 32u ? 32u : f.cotes);
					// Le premier sommet a -90° + 180°/n : le cote du BAS est
					// horizontal (un hexagone pose a plat, un carre droit).
					const float32 a0 = -PI * 0.5f + PI / static_cast<float32>(n);
					for (uint32 i = 0; i < n; ++i) {
						const float32 a = a0 + 2.f * PI * static_cast<float32>(i) / static_cast<float32>(n);
						Ajouter(s, cap, k, NkVec2f(hx * math::NkCos(a), hy * math::NkSin(a)));
					}
					return k;
				}
				case NkGenreForme2D::NK_CAPSULE:
					return Capsule(f.taille.x, f.taille.y, SEGMENTS_COURBE / 4u, s, cap);
				case NkGenreForme2D::NK_LIGNE:
				case NkGenreForme2D::NK_POLYGONE_LIBRE: {
					NkVec2f tampon[4];
					const NkVec2f *pts = nullptr;
					const uint32 n = PointsOuDefaut(f, pts, tampon);
					ferme = f.genre == NkGenreForme2D::NK_POLYGONE_LIBRE;
					for (uint32 i = 0; i < n; ++i) {
						Ajouter(s, cap, k, pts[i]);
					}
					return k;
				}
				default:
					return 0u;
			}
		}

		uint32 NkContourForme2D(const NkRenduForme2D &f, NkVec2f *s, uint32 cap, bool &ferme) noexcept {
			NkVec2f coins[NK_FORME_CONTOUR_MAX];
			const uint32 n = NkSommetsForme2D(f, coins, NK_FORME_CONTOUR_MAX, ferme);
			const bool aCoins = f.genre == NkGenreForme2D::NK_RECTANGLE || f.genre == NkGenreForme2D::NK_TRIANGLE ||
								f.genre == NkGenreForme2D::NK_ETOILE || f.genre == NkGenreForme2D::NK_POLYGONE_REGULIER ||
								f.genre == NkGenreForme2D::NK_POLYGONE_LIBRE;
			if (aCoins && ferme && f.arrondi > 0.f) {
				return Arrondir(coins, n, f.arrondi, s, cap);
			}
			uint32 k = 0;
			for (uint32 i = 0; i < n; ++i) {
				Ajouter(s, cap, k, coins[i]);
			}
			return k;
		}

		bool NkFormePeutEtreConcave(const NkRenduForme2D &f) noexcept {
			return f.genre == NkGenreForme2D::NK_ETOILE || f.genre == NkGenreForme2D::NK_POLYGONE_LIBRE;
		}

		float32 NkDistanceRenduForme2D(const NkTransform2D &t, const NkRenduForme2D &f, const NkVec2f &p) noexcept {
			// Le point dans le repere de l'entite (rotation et ECHELLE inverses).
			const float32 co = math::NkCos(-t.rotation), si = math::NkSin(-t.rotation);
			const float32 gx = p.x - t.position.x, gy = p.y - t.position.y;
			const float32 ex = Abs(t.echelle.x) > 1.0e-6f ? t.echelle.x : 1.0e-6f;
			const float32 ey = Abs(t.echelle.y) > 1.0e-6f ? t.echelle.y : 1.0e-6f;
			const NkVec2f l((gx * co - gy * si) / ex, (gx * si + gy * co) / ey);
			NkVec2f pts[NK_FORME_CONTOUR_MAX];
			bool ferme = true;
			const uint32 n = NkContourForme2D(f, pts, NK_FORME_CONTOUR_MAX, ferme);
			const float32 d = NkDistanceContour2D(pts, n, ferme, l);
			// La distance revient en metres (echelle moyenne) ; une ligne a son
			// epaisseur, un contour la moitie du sien vers l'exterieur.
			const float32 e = (Abs(ex) + Abs(ey)) * 0.5f;
			const float32 bord = ferme ? f.epaisseurContour * 0.5f : Max(f.epaisseur, f.epaisseurContour) * 0.5f;
			return d * e - bord;
		}

		NkVec2f NkDemiBoiteForme2D(const NkRenduForme2D &f) noexcept {
			NkVec2f pts[NK_FORME_CONTOUR_MAX];
			bool ferme = true;
			const uint32 n = NkContourForme2D(f, pts, NK_FORME_CONTOUR_MAX, ferme);
			float32 mx = 0.f, my = 0.f;
			for (uint32 i = 0; i < n; ++i) {
				mx = Max(mx, Abs(pts[i].x));
				my = Max(my, Abs(pts[i].y));
			}
			const float32 bord = Max(f.epaisseurContour, ferme ? 0.f : f.epaisseur) * 0.5f;
			return NkVec2f(mx + bord, my + bord);
		}

		// =====================================================================
		bool NkCollisionneurDepuisForme(const NkRenduForme2D &f, bool dynamique, NkCollisionneur2D &c) noexcept {
			c.decalage = NkVec2f(0.f, 0.f);
			c.rotation = 0.f;
			c.nbSommets = 0;
			c.boucle = false;
			const float32 hx = Abs(f.taille.x) * 0.5f, hy = Abs(f.taille.y) * 0.5f;
			NkVec2f pts[NK_FORME_CONTOUR_MAX];
			bool ferme = true;
			switch (f.genre) {
				case NkGenreForme2D::NK_RECTANGLE:
					c.forme = NkForme2D::NK_BOITE;
					c.demiTaille = NkVec2f(Max(hx, 0.01f), Max(hy, 0.01f));
					return true;
				case NkGenreForme2D::NK_CERCLE:
					c.forme = NkForme2D::NK_CERCLE;
					c.rayon = Max(Min(hx, hy), 0.01f);
					return true;
				case NkGenreForme2D::NK_CAPSULE: {
					const bool couchee = f.taille.x >= f.taille.y;
					c.forme = NkForme2D::NK_CAPSULE;
					c.rayon = Max((couchee ? hy : hx), 0.01f);
					c.demiTaille = NkVec2f(Max(0.f, (couchee ? hx : hy) - c.rayon), 0.f);
					c.rotation = couchee ? 0.f : PI * 0.5f;
					return true;
				}
				case NkGenreForme2D::NK_ELLIPSE: {
					if (PresqueRond(f)) {
						c.forme = NkForme2D::NK_CERCLE;
						c.rayon = Max(hx, 0.01f);
						return true;
					}
					// Huit sommets pour un corps qui bouge, vingt-quatre pour le decor.
					const uint32 n = Ellipse(hx, hy, dynamique ? NK_POLYGONE_CONVEXE_MAX : 24u, pts, NK_FORME_CONTOUR_MAX);
					if (dynamique) {
						Polygone(pts, n, c);
					} else {
						ChaineFermee(pts, n, c);
					}
					return true;
				}
				case NkGenreForme2D::NK_LIGNE: {
					const uint32 n = NkSommetsForme2D(f, pts, NK_FORME_CONTOUR_MAX, ferme);
					if (n == 2u) {
						// UN segment : une capsule exacte, ses bouts ronds compris.
						const NkVec2f d(pts[1].x - pts[0].x, pts[1].y - pts[0].y);
						c.forme = NkForme2D::NK_CAPSULE;
						c.decalage = NkVec2f((pts[0].x + pts[1].x) * 0.5f, (pts[0].y + pts[1].y) * 0.5f);
						c.rotation = math::NkAtan2(d.y, d.x);
						c.demiTaille = NkVec2f(math::NkSqrt(d.x * d.x + d.y * d.y) * 0.5f, 0.f);
						c.rayon = Max(f.epaisseur * 0.5f, 0.005f);
						return true;
					}
					// Une ligne brisee EPAISSE : les deux bords du trait.
					NkVec2f bords[2u * NK_FORME_POINTS_MAX];
					const uint32 m = BordsDuTrait(pts, n, Max(f.epaisseur, 0.01f), bords, 2u * NK_FORME_POINTS_MAX);
					if (dynamique) {
						EnveloppeDynamique(bords, m, c);
					} else {
						ChaineFermee(bords, m, c);
					}
					return m >= 3u;
				}
				default: {
					// Triangle, etoile, polygone regulier, polygone libre : leurs coins.
					const uint32 n = NkSommetsForme2D(f, pts, NK_FORME_CONTOUR_MAX, ferme);
					if (n < 3u) {
						return false;
					}
					if (n <= NK_POLYGONE_CONVEXE_MAX && NkEstConvexe2D(pts, n)) {
						Polygone(pts, n, c); // exact
						return true;
					}
					if (dynamique) {
						// Le cercle, plus juste que 8 sommets, pour un polygone regulier
						// de plus de 8 cotes inscrit dans un carre.
						if (f.genre == NkGenreForme2D::NK_POLYGONE_REGULIER && PresqueRond(f)) {
							const float32 n2 = static_cast<float32>(n);
							c.forme = NkForme2D::NK_CERCLE;
							c.rayon = hx * (1.f + math::NkCos(PI / n2)) * 0.5f; // entre apotheme et rayon
							return true;
						}
						EnveloppeDynamique(pts, n, c);
					} else {
						ChaineFermee(pts, n, c);
					}
					return true;
				}
			}
		}

		bool NkCollisionneurDepuisSprite(const NkSprite2D &s, const uint8 *rgba, int32 w, int32 h, NkCollisionneur2D &c,
										 uint8 seuil) noexcept {
			if (rgba == nullptr || w <= 0 || h <= 0) {
				return false;
			}
			// La region UV du sprite, en pixels de l'image.
			const int32 x0 = static_cast<int32>(math::NkClamp(Min(s.uv0.x, s.uv1.x), 0.f, 1.f) * static_cast<float32>(w));
			const int32 x1 = static_cast<int32>(math::NkClamp(Max(s.uv0.x, s.uv1.x), 0.f, 1.f) * static_cast<float32>(w));
			const int32 y0 = static_cast<int32>(math::NkClamp(Min(s.uv0.y, s.uv1.y), 0.f, 1.f) * static_cast<float32>(h));
			const int32 y1 = static_cast<int32>(math::NkClamp(Max(s.uv0.y, s.uv1.y), 0.f, 1.f) * static_cast<float32>(h));
			if (x1 <= x0 || y1 <= y0) {
				return false;
			}
			// Par ligne, le premier et le dernier pixel OPAQUE (leurs coins) : c'est
			// tout ce qu'il faut a l'enveloppe convexe. Une grande image est lue
			// une ligne sur `pas` (au plus ~256 lignes).
			const int32 pas = (y1 - y0) > 256 ? (y1 - y0) / 256 : 1;
			NkVector<NkVec2f> candidats;
			int32 opaques = 0, lus = 0;
			for (int32 y = y0; y < y1; y += pas) {
				int32 g = -1, d = -1;
				for (int32 x = x0; x < x1; ++x) {
					++lus;
					if (rgba[(static_cast<usize>(y) * static_cast<usize>(w) + static_cast<usize>(x)) * 4u + 3u] >= seuil) {
						++opaques;
						if (g < 0) {
							g = x;
						}
						d = x;
					}
				}
				if (g < 0) {
					continue;
				}
				const float32 yh = static_cast<float32>(y), yb = static_cast<float32>(y + pas < y1 ? y + pas : y1);
				candidats.PushBack(NkVec2f(static_cast<float32>(g), yh));
				candidats.PushBack(NkVec2f(static_cast<float32>(g), yb));
				candidats.PushBack(NkVec2f(static_cast<float32>(d + 1), yh));
				candidats.PushBack(NkVec2f(static_cast<float32>(d + 1), yb));
			}
			if (candidats.Empty()) {
				return false; // image entierement transparente
			}
			// Des pixels de l'image vers le repere du sprite (NkDessinerScene : la
			// ligne du HAUT de l'image est en haut, y monte en monde).
			const float32 W = s.taille.x, H = s.taille.y;
			const float32 ox = -s.pivot.x * W, oy = -s.pivot.y * H;
			const float32 dx = static_cast<float32>(x1 - x0), dy = static_cast<float32>(y1 - y0);
			for (uint32 i = 0; i < candidats.Size(); ++i) {
				const NkVec2f q = candidats[i];
				candidats[i] = NkVec2f(ox + W * (q.x - static_cast<float32>(x0)) / dx, oy + H * (1.f - (q.y - static_cast<float32>(y0)) / dy));
			}
			c.decalage = NkVec2f(0.f, 0.f);
			c.rotation = 0.f;
			c.boucle = false;
			c.nbSommets = 0;
			// Sans transparence : la boite du sprite, exacte.
			if (opaques == lus) {
				c.forme = NkForme2D::NK_BOITE;
				c.demiTaille = NkVec2f(Abs(W) * 0.5f, Abs(H) * 0.5f);
				c.decalage = NkVec2f(ox + W * 0.5f, oy + H * 0.5f);
				return true;
			}
			NkVec2f env[NK_FORME_CONTOUR_MAX];
			NkVector<NkVec2f> tous = candidats;
			uint32 m = NkEnveloppeConvexe2D(tous.Data(), static_cast<uint32>(tous.Size()), env, NK_FORME_CONTOUR_MAX);
			m = NkReduireConvexe2D(env, m, NK_POLYGONE_CONVEXE_MAX);
			if (m < 3u) {
				return false;
			}
			Polygone(env, m, c);
			return true;
		}

		uint32 NkContourCollisionneur2D(const NkTransform2D &t, const NkCollisionneur2D &c, NkVec2f *s, uint32 cap,
										bool &ferme, uint32 segs) noexcept {
			ferme = true;
			uint32 k = 0;
			segs = segs < 8u ? 8u : segs;
			switch (c.forme) {
				case NkForme2D::NK_CERCLE:
					for (uint32 i = 0; i < segs; ++i) {
						const float32 a = 2.f * PI * static_cast<float32>(i) / static_cast<float32>(segs);
						Ajouter(s, cap, k, NkPointCollision2D(t, c, NkVec2f(c.rayon * math::NkCos(a), c.rayon * math::NkSin(a))));
					}
					return k;
				case NkForme2D::NK_CAPSULE: {
					// Demi-cercle droit (-90 -> +90), puis gauche (+90 -> +270).
					const uint32 demi = segs / 2u;
					for (int32 cote = 0; cote < 2; ++cote) {
						const float32 cx = cote == 0 ? c.demiTaille.x : -c.demiTaille.x;
						const float32 a0 = cote == 0 ? -PI * 0.5f : PI * 0.5f;
						for (uint32 i = 0; i <= demi; ++i) {
							const float32 a = a0 + PI * static_cast<float32>(i) / static_cast<float32>(demi);
							Ajouter(s, cap, k, NkPointCollision2D(t, c, NkVec2f(cx + c.rayon * math::NkCos(a), c.rayon * math::NkSin(a))));
						}
					}
					return k;
				}
				case NkForme2D::NK_POLYGONE:
				case NkForme2D::NK_CHAINE: {
					ferme = c.forme == NkForme2D::NK_POLYGONE || c.boucle;
					const uint32 n = NkNbSommetsCollision2D(c);
					for (uint32 i = 0; i < n; ++i) {
						Ajouter(s, cap, k, NkPointCollision2D(t, c, c.sommets[i]));
					}
					return k;
				}
				default: {
					const float32 hx = c.demiTaille.x, hy = c.demiTaille.y;
					Ajouter(s, cap, k, NkPointCollision2D(t, c, NkVec2f(-hx, -hy)));
					Ajouter(s, cap, k, NkPointCollision2D(t, c, NkVec2f(hx, -hy)));
					Ajouter(s, cap, k, NkPointCollision2D(t, c, NkVec2f(hx, hy)));
					Ajouter(s, cap, k, NkPointCollision2D(t, c, NkVec2f(-hx, hy)));
					return k;
				}
			}
		}

		// =====================================================================
		float32 NkAireSignee2D(const NkVec2f *p, uint32 n) noexcept {
			float32 a = 0.f;
			for (uint32 i = 0; i < n; ++i) {
				const NkVec2f &u = p[i];
				const NkVec2f &v = p[(i + 1u) % n];
				a += u.x * v.y - v.x * u.y;
			}
			return a * 0.5f;
		}

		bool NkEstConvexe2D(const NkVec2f *p, uint32 n) noexcept {
			if (n < 3u) {
				return false;
			}
			int32 signe = 0;
			for (uint32 i = 0; i < n; ++i) {
				const NkVec2f &a = p[i];
				const NkVec2f &b = p[(i + 1u) % n];
				const NkVec2f &c = p[(i + 2u) % n];
				const float32 z = (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
				if (Abs(z) < 1.0e-9f) {
					continue;
				}
				const int32 s = z > 0.f ? 1 : -1;
				if (signe == 0) {
					signe = s;
				} else if (s != signe) {
					return false;
				}
			}
			return signe != 0;
		}

		uint32 NkEnveloppeConvexe2D(const NkVec2f *pts, uint32 n, NkVec2f *sortie, uint32 cap) noexcept {
			if (n == 0u || cap == 0u) {
				return 0u;
			}
			// Tri par x puis y (insertion : quelques centaines de points au plus).
			NkVector<NkVec2f> p;
			p.Resize(n);
			for (uint32 i = 0; i < n; ++i) {
				p[i] = pts[i];
			}
			for (uint32 i = 1; i < n; ++i) {
				const NkVec2f cle = p[i];
				int32 j = static_cast<int32>(i) - 1;
				while (j >= 0 && (p[static_cast<uint32>(j)].x > cle.x ||
								  (p[static_cast<uint32>(j)].x == cle.x && p[static_cast<uint32>(j)].y > cle.y))) {
					p[static_cast<uint32>(j + 1)] = p[static_cast<uint32>(j)];
					--j;
				}
				p[static_cast<uint32>(j + 1)] = cle;
			}
			auto Croix = [](const NkVec2f &o, const NkVec2f &a, const NkVec2f &b) {
				return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
			};
			NkVector<NkVec2f> h;
			h.Resize(2u * n + 1u);
			uint32 k = 0;
			for (uint32 i = 0; i < n; ++i) { // bas
				while (k >= 2u && Croix(h[k - 2u], h[k - 1u], p[i]) <= 0.f) {
					--k;
				}
				h[k++] = p[i];
			}
			const uint32 bas = k + 1u;
			for (uint32 i = n - 1u; i-- > 0u;) { // haut
				while (k >= bas && Croix(h[k - 2u], h[k - 1u], p[i]) <= 0.f) {
					--k;
				}
				h[k++] = p[i];
			}
			const uint32 m = k > 1u ? k - 1u : k; // le dernier repete le premier
			uint32 r = 0;
			for (uint32 i = 0; i < m && r < cap; ++i) {
				sortie[r++] = h[i];
			}
			return r;
		}

		uint32 NkReduireConvexe2D(NkVec2f *p, uint32 n, uint32 max) noexcept {
			while (n > max && n > 3u) {
				uint32 moins = 0;
				float32 aireMin = 1.0e30f;
				for (uint32 i = 0; i < n; ++i) {
					const NkVec2f &a = p[(i + n - 1u) % n];
					const NkVec2f &b = p[i];
					const NkVec2f &c = p[(i + 1u) % n];
					const float32 aire = Abs((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x));
					if (aire < aireMin) {
						aireMin = aire;
						moins = i;
					}
				}
				for (uint32 i = moins; i + 1u < n; ++i) {
					p[i] = p[i + 1u];
				}
				--n;
			}
			return n;
		}

		const NkChampSauve *NkChampsRenduForme2D(uint32 &nombre) noexcept {
			nombre = static_cast<uint32>(sizeof(kChamps) / sizeof(kChamps[0]));
			return kChamps;
		}

	} // namespace unkeny
} // namespace nkentseu
