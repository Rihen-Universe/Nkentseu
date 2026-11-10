// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/PathTracer/NkMatieresProcedurales.cpp
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// Les generateurs de matieres (voir l'en-tete). Aucune bibliotheque standard :
// le bruit, l'arrondi et l'encodage sRGB sont ecrits ici.

#include "NKRenderer/Tools/PathTracer/NkMatieresProcedurales.h"
#include "NKImage/Core/NkImage.h"
#include "NKFileSystem/NkFile.h"
#include "NKCore/Text/NkSnprintf.h"
#include "NKCore/Text/NkSortie.h"
#include "NKMath/NkFunctions.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace renderer {

		namespace {
			// La version des cartes : la monter regenere tout (le nom de fichier la porte).
			constexpr int32 kVersionCartes = 3; // (10/11) 2 : tissu, velours, placage ; 3 : la couture du bois

			// ── CONTRE-EPREUVE DU BANC : un bruit qui NE se replie PAS ──────────
			// Vrai : le reseau de germes n'est plus pris modulo sa periode, la carte
			// a une couture a son bord. Le banc doit la voir (NkMatieresBanc).
			bool sNonPeriodique = false;

			const NkMatiereParams kTable[kMatClasses] = {
				// nom          tuile  rug    metal relief cartes couleurPropre
				{"enduit", 1.5f, 0.82f, 0.f, 0.6f, true, false},
				{"beton", 2.0f, 0.85f, 0.f, 0.8f, true, false},
				{"pierre", 1.4f, 0.90f, 0.f, 1.6f, true, false},
				{"bois", 1.0f, 0.55f, 0.f, 1.0f, true, false},
				{"carrelage", 1.2f, 0.22f, 0.f, 1.0f, true, false},
				{"metal", 0.5f, 0.32f, 1.f, 0.3f, true, false},
				{"feuillage", 0.6f, 0.60f, 0.f, 1.2f, true, false},
				{"herbe", 4.0f, 0.95f, 0.f, 1.2f, true, true},
				{"ceramique", 1.0f, 0.10f, 0.f, 0.1f, true, false},
				{"cuir", 0.3f, 0.50f, 0.f, 0.8f, true, false},
				{"peinture", 1.5f, 0.55f, 0.f, 0.3f, true, false},
				{"verre", 1.0f, 0.02f, 0.f, 0.0f, false, false},
				{"emissif", 1.0f, 0.50f, 0.f, 0.0f, false, false},
				// (10/11/2026) tuiles : 12 cm de trame (96 fils), 15 cm de velours, 2,40 m de placage
				{"tissu", 0.12f, 0.92f, 0.f, 0.9f, true, false},
				{"velours", 0.15f, 0.95f, 0.f, 0.25f, true, false},
				{"placage", 2.4f, 0.45f, 0.f, 0.35f, true, false},
			};

			inline int32 Plancher(float32 x) {
				const int32 i = (int32)x;
				return (x < (float32)i) ? i - 1 : i;
			}
			inline float32 Frac(float32 x) {
				return x - (float32)Plancher(x);
			}
			inline int32 Mod(int32 a, int32 p) {
				if (sNonPeriodique)
					return a; // la mutation : pas de repli
				const int32 r = a % p;
				return r < 0 ? r + p : r;
			}
			inline float32 Borne(float32 x, float32 a, float32 b) {
				return x < a ? a : (x > b ? b : x);
			}
			inline float32 Lisse(float32 t) {
				return t * t * t * (t * (t * 6.f - 15.f) + 10.f);
			}
			inline float32 Marche(float32 a, float32 b, float32 x) {
				const float32 t = Borne((x - a) / (b - a), 0.f, 1.f);
				return t * t * (3.f - 2.f * t);
			}
			inline float32 Melange(float32 a, float32 b, float32 t) {
				return a + (b - a) * t;
			}
			inline uint32 Hache(int32 x, int32 y, uint32 graine) {
				uint32 h = (uint32)x * 374761393u + (uint32)y * 668265263u + graine * 2246822519u;
				h = (h ^ (h >> 13)) * 1274126177u;
				return h ^ (h >> 16);
			}
			inline float32 Alea(int32 x, int32 y, uint32 g) {
				return (float32)(Hache(x, y, g) & 0xFFFFFFu) / 16777216.f;
			}

			// Bruit de valeurs PERIODIQUE : reseau px x py sur le carre unite, donc la
			// carte (u, v dans [0,1)) se repete sans couture.
			float32 Valeur(float32 u, float32 v, int32 px, int32 py, uint32 g) {
				const float32 x = u * (float32)px, y = v * (float32)py;
				const int32 x0 = Plancher(x), y0 = Plancher(y);
				const float32 fx = Lisse(x - (float32)x0), fy = Lisse(y - (float32)y0);
				const int32 a = Mod(x0, px), b = Mod(x0 + 1, px), c = Mod(y0, py), d = Mod(y0 + 1, py);
				const float32 v00 = Alea(a, c, g), v10 = Alea(b, c, g), v01 = Alea(a, d, g), v11 = Alea(b, d, g);
				return Melange(Melange(v00, v10, fx), Melange(v01, v11, fx), fy);
			}

			// Somme d'octaves (fbm), ramenee dans [0,1].
			float32 Fbm(float32 u, float32 v, int32 px, int32 py, int32 octaves, uint32 g) {
				float32 s = 0.f, amp = 0.5f, tot = 0.f;
				for (int32 o = 0; o < octaves; ++o) {
					s += amp * Valeur(u, v, px << o, py << o, g + (uint32)o * 101u);
					tot += amp;
					amp *= 0.5f;
				}
				return s / tot;
			}

			// Bruit cellulaire PERIODIQUE (Worley) : un germe par case, F1 et F2 en
			// unites de case, et l'identite de la case la plus proche.
			struct Cellule {
					float32 f1, f2;
					int32 idX, idY;
			};
			Cellule Worley(float32 u, float32 v, int32 px, int32 py, uint32 g) {
				const float32 x = u * (float32)px, y = v * (float32)py;
				const int32 cx = Plancher(x), cy = Plancher(y);
				Cellule c{1e9f, 1e9f, 0, 0};
				for (int32 j = -1; j <= 1; ++j)
					for (int32 i = -1; i <= 1; ++i) {
						const int32 gx = cx + i, gy = cy + j;
						const int32 wx = Mod(gx, px), wy = Mod(gy, py);
						const float32 sx = (float32)gx + 0.15f + 0.7f * Alea(wx, wy, g);
						const float32 sy = (float32)gy + 0.15f + 0.7f * Alea(wx, wy, g + 7u);
						const float32 dx = sx - x, dy = sy - y;
						const float32 d = math::NkSqrt(dx * dx + dy * dy);
						if (d < c.f1) {
							c.f2 = c.f1;
							c.f1 = d;
							c.idX = wx;
							c.idY = wy;
						} else if (d < c.f2)
							c.f2 = d;
					}
				return c;
			}

			struct Point {
					float32 r, g, b; // couleur (lineaire)
					float32 h;		 // hauteur 0..1
					float32 rug;	 // rugosite
					float32 met;	 // metal
					float32 occ;	 // occlusion de cavite
			};

			void Gris(Point &p, float32 a, float32 tr, float32 tg, float32 tb) {
				p.r = a * tr;
				p.g = a * tg;
				p.b = a * tb;
			}

			// UN point de la carte d'une classe. Le banc et l'ecriture passent par
			// ici : ce qui est mesure est ce qui est ecrit.
			Point Echantillon(int32 classe, float32 u, float32 v) {
				Point p{0.9f, 0.9f, 0.9f, 0.5f, 0.5f, 0.f, 1.f};
				const NkMatiereParams &M = NkMatiereDe(classe);
				p.rug = M.rugosite;
				p.met = M.metal;
				switch (classe) {
					case kMatEnduit: {
						const float32 n1 = Fbm(u, v, 8, 8, 5, 11u), n2 = Fbm(u, v, 64, 64, 3, 23u);
						const float32 a = 0.90f + 0.16f * (Fbm(u, v, 3, 3, 3, 37u) - 0.5f);
						Gris(p, a, 1.f, 0.99f, 0.97f);
						p.h = 0.6f * n1 + 0.4f * n2;
						p.rug = 0.82f + 0.2f * (n2 - 0.5f);
						break;
					}
					case kMatBeton: {
						const float32 n = Fbm(u, v, 16, 16, 4, 41u), gros = Fbm(u, v, 3, 3, 4, 43u);
						const Cellule c = Worley(u, v, 24, 24, 47u);
						const float32 pore = (Alea(c.idX, c.idY, 53u) < 0.5f) ? (1.f - Marche(0.06f, 0.16f, c.f1)) : 0.f;
						const float32 a = 0.82f + 0.36f * (gros - 0.5f) + 0.12f * (n - 0.5f) - 0.25f * pore;
						Gris(p, a, 1.f, 0.98f, 0.95f);
						p.h = 0.5f * n + 0.5f - 0.6f * pore;
						p.rug = 0.85f + 0.16f * (n - 0.5f);
						p.occ = 1.f - 0.4f * pore;
						break;
					}
					case kMatPierre: {
						const Cellule c = Worley(u, v, 5, 4, 61u);
						const float32 e = c.f2 - c.f1;
						const float32 joint = Marche(0.03f, 0.10f, e); // 0 dans le joint, 1 dans la pierre
						const float32 t = 0.75f + 0.40f * Alea(c.idX, c.idY, 67u);
						const float32 teinte = (Alea(c.idX, c.idY, 71u) - 0.5f) * 0.10f;
						const float32 detail = Fbm(u, v, 24, 24, 4, 73u);
						const float32 a = t * (0.85f + 0.30f * (detail - 0.5f));
						const float32 ar = Melange(0.42f, a * (1.f + teinte), joint);
						const float32 ag = Melange(0.42f, a, joint);
						const float32 ab = Melange(0.40f, a * (1.f - teinte), joint);
						p.r = ar;
						p.g = ag;
						p.b = ab;
						p.h = joint * (0.6f + 0.25f * detail + 0.15f * (1.f - Borne(c.f1, 0.f, 1.f)));
						p.rug = Melange(0.95f, 0.88f, joint);
						p.occ = 0.7f + 0.3f * joint;
						break;
					}
					case kMatBois: {
						// Le FIL suit v (vertical sur un mur, grace a la projection du chargeur).
						const float32 ul = u * 5.f;	 // cinq lames de 20 cm par tuile de 1 m
						const int32 lame = Plancher(ul);
						const float32 pu = ul - (float32)lame;
						// (10/11) le decalage des abouts reste loin du bord de la tuile (5 % a 95 %) : un about
						// coupe par le bord (lame 2 : decalage 0,4 %) mettait la rampe du joint SUR la ligne
						// mesuree -- le banc y lisait une couture (v 14,3) qui n'en etait pas une
						const float32 decal = 0.05f + 0.9f * Alea(Mod(lame, 5), 0, 83u);
						const float32 pv = Frac(v * 2.f + decal); // lames de 50 cm, abouts decales
						const int32 troncon = Plancher(v * 2.f + decal);
						// (10/11) modulo DEUX : la tuile porte deux rangs de lames (v x 2) ; modulo trois, la lame
						// coupee par le bord de la tuile changeait de nuance d'un cote a l'autre (couture v 19,2
						// au banc des matieres)
						const float32 tl = 0.84f + 0.28f * Alea(Mod(lame, 5), Mod(troncon, 2), 89u);
						const float32 fil = Fbm(u, v, 160, 4, 3, 97u);
						const float32 cerne =
							0.5f + 0.5f * math::NkSin(6.2831853f * (pu * 3.f + 2.2f * Fbm(u, v, 10, 2, 3, 101u)));
						const float32 a = tl * (0.80f + 0.16f * (fil - 0.5f) - 0.14f * cerne * cerne);
						Gris(p, a, 1.f, 0.97f, 0.93f);
						const float32 jointLong = 1.f - Marche(0.0f, 0.015f, pu) * Marche(1.0f, 0.985f, pu);
						const float32 jointBout = 1.f - Marche(0.0f, 0.006f, pv) * Marche(1.0f, 0.994f, pv);
						const float32 joint = jointLong > jointBout ? jointLong : jointBout;
						p.h = 0.65f + 0.15f * fil - 0.08f * cerne - 0.55f * joint;
						p.rug = 0.55f + 0.2f * (fil - 0.5f) + 0.2f * joint;
						p.occ = 1.f - 0.5f * joint;
						p.r *= 1.f - 0.45f * joint;
						p.g *= 1.f - 0.45f * joint;
						p.b *= 1.f - 0.45f * joint;
						break;
					}
					case kMatCarrelage: {
						// 2 x 2 carreaux de 60 cm par tuile de 1,20 m ; joints de 4 mm.
						const float32 tu = Frac(u * 2.f), tv = Frac(v * 2.f);
						float32 bord = tu < 1.f - tu ? tu : 1.f - tu;
						const float32 bv = tv < 1.f - tv ? tv : 1.f - tv;
						bord = (bord < bv ? bord : bv) * 0.6f; // en metres
						const float32 carreau = Marche(0.0018f, 0.0032f, bord);
						const int32 ix = Mod(Plancher(u * 2.f), 2), iy = Mod(Plancher(v * 2.f), 2);
						const float32 t = 0.96f + 0.08f * Alea(ix, iy, 107u);
						const float32 veine = Fbm(u, v, 4, 4, 5, 109u);
						const float32 a = Melange(0.50f, t * (0.95f + 0.10f * (veine - 0.5f)), carreau);
						Gris(p, a, 1.f, 0.99f, 0.97f);
						p.h = Melange(0.25f, 0.9f, carreau);
						p.rug = Melange(0.80f, 0.20f + 0.06f * (veine - 0.5f), carreau);
						p.occ = Melange(0.7f, 1.f, carreau);
						break;
					}
					case kMatMetal: {
						const float32 s = Fbm(u, v, 2, 256, 3, 113u);
						Gris(p, 0.90f + 0.16f * (s - 0.5f), 1.f, 1.f, 1.f);
						p.h = 0.5f + 0.2f * (s - 0.5f);
						p.rug = 0.32f + 0.2f * (s - 0.5f);
						p.met = 1.f;
						break;
					}
					case kMatFeuillage: {
						const Cellule c = Worley(u, v, 12, 12, 127u);
						const float32 feuille = 1.f - Borne(c.f1, 0.f, 1.f);
						const float32 t = 0.65f + 0.5f * Alea(c.idX, c.idY, 131u);
						const float32 jaune = Alea(c.idX, c.idY, 137u) * 0.2f;
						const float32 bordF = Marche(0.02f, 0.08f, c.f2 - c.f1);
						p.r = t * (1.f + jaune) * (0.55f + 0.45f * bordF);
						p.g = t * (0.55f + 0.45f * bordF);
						p.b = t * 0.85f * (0.55f + 0.45f * bordF);
						p.h = feuille * bordF;
						p.rug = 0.6f;
						p.occ = 0.6f + 0.4f * bordF;
						break;
					}
					case kMatHerbe: {
						const float32 gros = Fbm(u, v, 4, 4, 4, 149u), moyen = Fbm(u, v, 16, 16, 4, 151u);
						const float32 brins = Valeur(u, v, 256, 96, 157u);
						float32 r = Melange(0.050f, 0.150f, moyen), g = Melange(0.105f, 0.235f, moyen),
								b = Melange(0.025f, 0.055f, moyen);
						const float32 sec = Marche(0.60f, 0.80f, gros) * 0.6f;
						r = Melange(r, 0.30f, sec);
						g = Melange(g, 0.27f, sec);
						b = Melange(b, 0.12f, sec);
						const float32 terre = Marche(0.35f, 0.20f, gros) * 0.7f;
						r = Melange(r, 0.16f, terre);
						g = Melange(g, 0.12f, terre);
						b = Melange(b, 0.08f, terre);
						const float32 k = 0.75f + 0.5f * brins;
						p.r = r * k;
						p.g = g * k;
						p.b = b * k;
						p.h = 0.5f * brins + 0.5f * moyen;
						p.rug = 0.95f;
						p.occ = 0.75f + 0.25f * brins;
						break;
					}
					case kMatCeramique: {
						Gris(p, 0.98f - 0.04f * Fbm(u, v, 8, 8, 3, 163u), 1.f, 1.f, 0.99f);
						p.h = 0.5f;
						p.rug = 0.10f;
						break;
					}
					case kMatCuir: {
						const Cellule c = Worley(u, v, 40, 40, 167u);
						Gris(p, 0.85f + 0.15f * Borne(c.f1, 0.f, 1.f), 1.f, 1.f, 1.f);
						p.h = Borne(c.f1, 0.f, 1.f);
						p.rug = 0.50f;
						break;
					}
					case kMatTissu: {
						// TOILE (armure unie) : 96 fils de chaine (le long de v) et 96 de trame (le long
						// de u) par tuile ; au croisement (i, j), la chaine passe dessus si i + j est pair.
						// Chaque fil a un profil rond, une epaisseur qui varie le long de lui-meme (les
						// FLAMMES du lin, la chenille) et sa propre nuance ; un duvet fin par-dessus.
						const int32 N = 96;
						const float32 x = u * (float32)N, y = v * (float32)N;
						const int32 ix = Plancher(x), iy = Plancher(y);
						const float32 fx = x - (float32)ix, fy = y - (float32)iy;
						const bool chaineDessus = ((Mod(ix, N) + Mod(iy, N)) & 1) == 0;
						// les flammes : l'epaisseur d'un fil varie lentement le long de lui
						const float32 flC = 0.75f + 0.5f * Valeur((float32)Mod(ix, N) / (float32)N, v, N, 6, 191u);
						const float32 flT = 0.75f + 0.5f * Valeur(u, (float32)Mod(iy, N) / (float32)N, 6, N, 193u);
						const float32 sC = math::NkSin(3.14159265f * Borne(0.5f + (fx - 0.5f) / Borne(flC, 0.5f, 1.2f), 0.f, 1.f));
						const float32 sT = math::NkSin(3.14159265f * Borne(0.5f + (fy - 0.5f) / Borne(flT, 0.5f, 1.2f), 0.f, 1.f));
						// l'ondulation : le fil du dessus bombe au milieu du croisement
						const float32 bombe = math::NkSin(3.14159265f * (chaineDessus ? fy : fx));
						const float32 hC = math::NkSqrt(Borne(sC, 0.f, 1.f)) * (chaineDessus ? 0.7f + 0.3f * bombe : 0.45f);
						const float32 hT = math::NkSqrt(Borne(sT, 0.f, 1.f)) * (chaineDessus ? 0.45f : 0.7f + 0.3f * bombe);
						const bool chaine = hC >= hT;
						const float32 h = chaine ? hC : hT;
						const float32 nuance = 0.92f + 0.12f * (chaine ? Alea(Mod(ix, N), 0, 197u) : Alea(0, Mod(iy, N), 199u));
						const float32 duvet = Fbm(u, v, 256, 256, 2, 211u);
						const float32 a = nuance * (0.80f + 0.20f * h) * (0.94f + 0.12f * (duvet - 0.5f));
						Gris(p, a, 1.f, 1.f, 1.f);
						p.h = 0.85f * h + 0.15f * duvet;
						p.rug = 0.90f + 0.08f * (duvet - 0.5f);
						p.occ = 0.62f + 0.38f * h;
						break;
					}
					case kMatVelours: {
						// VELOURS : un poil tres fin (bruit a haute frequence) et la MOIRE d'ecrasement --
						// des plages ou le poil est couche autrement, plus claires ou plus sombres. Le
						// relief est faible : c'est le lustre (sheen) du traceur et du temps reel qui fait
						// le velours, la carte ne porte que ses variations.
						const float32 poil = Fbm(u, v, 192, 192, 2, 223u);
						const float32 moire = Fbm(u, v, 3, 3, 4, 227u);
						const float32 a = (0.86f + 0.22f * (moire - 0.5f)) * (0.95f + 0.10f * (poil - 0.5f));
						Gris(p, a, 1.f, 1.f, 1.f);
						p.h = 0.5f + 0.25f * (moire - 0.5f) + 0.15f * (poil - 0.5f);
						p.rug = 0.95f;
						p.occ = 0.92f + 0.08f * poil;
						break;
					}
					case kMatPlacage: {
						// PLACAGE : une tuile de 2,40 m (un grand panneau entier), douze LES de 20 cm
						// ASSORTIS EN LIVRE (un le sur deux est le miroir de son voisin, comme les feuilles
						// tranchees d'une meme bille) ; le FIL suit v sur toute la tuile, sans lame ni about :
						// plus de damier. Joints de les a peine visibles ; pores en traits le long du fil.
						const int32 nLes = 12;
						const float32 ul = u * (float32)nLes;
						const int32 le = Plancher(ul);
						float32 pu = ul - (float32)le;
						const int32 paire = Mod(le, nLes) / 2;
						if (Mod(le, 2) == 1)
							pu = 1.f - pu; // le miroir du le voisin
						// la bille : meme figure pour les deux les d'une paire, nuance par paire
						const float32 uu = ((float32)paire + pu) / (float32)(nLes / 2);
						const float32 onde = Fbm(uu, v, 3, 2, 3, 233u); // le fil ondule doucement
						const float32 fil = 0.5f + 0.5f * math::NkSin(6.2831853f * (pu * 28.f + 3.f * onde));
						const float32 cerne = 0.5f + 0.5f * math::NkSin(6.2831853f * (pu * 7.f + 1.5f * onde + 0.3f * Fbm(uu, v, 4, 8, 3, 239u)));
						const float32 pores = Valeur(uu, v, 900, 12, 241u);
						const float32 nuance = 0.95f + 0.10f * Alea(paire, 0, 251u);
						const float32 joint = 1.f - Marche(0.0f, 0.004f, pu) * Marche(1.0f, 0.996f, pu);
						const float32 a = nuance * (0.86f - 0.10f * cerne * cerne - 0.05f * fil - 0.06f * Marche(0.75f, 0.95f, pores)) *
										  (1.f - 0.12f * joint);
						Gris(p, a, 1.f, 0.97f, 0.93f);
						p.h = 0.6f + 0.06f * fil - 0.08f * Marche(0.75f, 0.95f, pores) - 0.1f * joint;
						p.rug = 0.42f + 0.10f * (fil - 0.5f) + 0.15f * Marche(0.75f, 0.95f, pores);
						p.occ = 1.f - 0.2f * joint;
						break;
					}
					case kMatPeinture:
					default: {
						Gris(p, 0.96f + 0.08f * (Fbm(u, v, 6, 6, 3, 173u) - 0.5f), 1.f, 1.f, 1.f);
						p.h = 0.3f * Fbm(u, v, 32, 32, 3, 179u);
						p.rug = 0.55f;
						break;
					}
				}
				p.r = Borne(p.r, 0.f, 1.f);
				p.g = Borne(p.g, 0.f, 1.f);
				p.b = Borne(p.b, 0.f, 1.f);
				p.h = Borne(p.h, 0.f, 1.f);
				p.rug = Borne(p.rug, 0.02f, 1.f);
				p.met = Borne(p.met, 0.f, 1.f);
				p.occ = Borne(p.occ, 0.f, 1.f);
				return p;
			}

			uint8 Octet(float32 x) {
				const float32 y = Borne(x, 0.f, 1.f) * 255.f + 0.5f;
				return (uint8)(int32)y;
			}
			// Encodage sRGB (IEC 61966-2-1).
			float32 VersSRGB(float32 c) {
				if (c <= 0.0031308f)
					return 12.92f * c;
				return 1.055f * (float32)math::NkPow((float64)c, 1.0 / 2.4) - 0.055f;
			}

			// Calcule toute la carte (points), taille x taille.
			void Calculer(int32 classe, int32 taille, Point *out) {
				for (int32 y = 0; y < taille; ++y) {
					const float32 v = ((float32)y + 0.5f) / (float32)taille;
					for (int32 x = 0; x < taille; ++x) {
						const float32 u = ((float32)x + 0.5f) / (float32)taille;
						out[y * taille + x] = Echantillon(classe, u, v);
					}
				}
			}
		} // namespace

		const NkMatiereParams &NkMatiereDe(int32 classe) {
			if (classe < 0 || classe >= kMatClasses)
				return kTable[kMatEnduit];
			return kTable[classe];
		}

		void NkMatiereChemin(int32 classe, const char *dossier, int32 canal, char *out, int32 cap) {
			static const char *kCanal[3] = {"couleur", "normale", "orm"};
			const int32 c = canal < 0 ? 0 : (canal > 2 ? 2 : canal);
			NkSnprintf(out, (usize)cap, "%s/%s_%s_v%d.png", dossier, NkMatiereDe(classe).nom, kCanal[c],
					   (int)kVersionCartes);
		}

		bool NkMatiereGenerer(int32 classe, const char *dossier, int32 taille, float32 *moyenneLin3) {
			const NkMatiereParams &M = NkMatiereDe(classe);
			if (moyenneLin3) {
				moyenneLin3[0] = 1.f;
				moyenneLin3[1] = 1.f;
				moyenneLin3[2] = 1.f;
			}
			if (!M.cartes)
				return true;
			if (taille < 16)
				taille = 16;
			// (10/11) le placage couvre 2,40 m : deux fois plus de pixels pour que son fil reste net
			if (classe == kMatPlacage && taille < 2048)
				taille *= 2;
			char ch[3][512];
			for (int32 k = 0; k < 3; ++k)
				NkMatiereChemin(classe, dossier, k, ch[k], 512);
			// DEJA ECRITES (meme version) : seule la moyenne se recalcule, sur 256 x 256
			// (la carte est periodique et stationnaire : la moyenne ne depend pas de la taille
			// au-dela de quelques periodes ; 33 s de generation evites a chaque lancement).
			if (NkFile::Exists(ch[0]) && NkFile::Exists(ch[1]) && NkFile::Exists(ch[2])) {
				if (moyenneLin3) {
					const int32 t = 256;
					NkVector<Point> p;
					p.Resize((usize)(t * t));
					Calculer(classe, t, p.Data());
					float64 s[3] = {0.0, 0.0, 0.0};
					for (int32 i = 0; i < t * t; ++i) {
						s[0] += p[i].r;
						s[1] += p[i].g;
						s[2] += p[i].b;
					}
					for (int32 a = 0; a < 3; ++a)
						moyenneLin3[a] = (float32)(s[a] / (float64)(t * t));
				}
				return true;
			}
			NkImage coul = NkImage::Create((uint32)taille, (uint32)taille, 4, 0u);
			NkImage norm = NkImage::Create((uint32)taille, (uint32)taille, 4, 0u);
			NkImage orm = NkImage::Create((uint32)taille, (uint32)taille, 4, 0u);
			if (!coul.IsValid() || !norm.IsValid() || !orm.IsValid())
				return false;
			const int32 n = taille * taille;
			NkVector<Point> pts;
			pts.Resize((usize)n);
			Calculer(classe, taille, pts.Data());
			float64 som[3] = {0.0, 0.0, 0.0};
			// La pente : la hauteur 0..1 vaut `relief` x 2 mm de relief sur une tuile ;
			// convertie en pente par pixel (tuile en metres / taille).
			const float32 pixelM = M.tuileM / (float32)taille;
			const float32 hauteurM = 0.002f * M.relief;
			for (int32 y = 0; y < taille; ++y) {
				uint8 *lc = coul.RowPtr(y);
				uint8 *ln = norm.RowPtr(y);
				uint8 *lo = orm.RowPtr(y);
				for (int32 x = 0; x < taille; ++x) {
					const Point &p = pts[y * taille + x];
					som[0] += p.r;
					som[1] += p.g;
					som[2] += p.b;
					lc[x * 4 + 0] = Octet(VersSRGB(p.r));
					lc[x * 4 + 1] = Octet(VersSRGB(p.g));
					lc[x * 4 + 2] = Octet(VersSRGB(p.b));
					lc[x * 4 + 3] = 255;
					// differences centrees, repliees (la carte est periodique)
					const int32 xg = (x + taille - 1) % taille, xd = (x + 1) % taille;
					const int32 yh = (y + taille - 1) % taille, yb = (y + 1) % taille;
					const float32 dhdu = (pts[y * taille + xd].h - pts[y * taille + xg].h) * hauteurM / (2.f * pixelM);
					const float32 dhdv = (pts[yb * taille + x].h - pts[yh * taille + x].h) * hauteurM / (2.f * pixelM);
					float32 nx = -dhdu, ny = -dhdv, nz = 1.f;
					const float32 l = math::NkSqrt(nx * nx + ny * ny + nz * nz);
					nx /= l;
					ny /= l;
					nz /= l;
					ln[x * 4 + 0] = Octet(nx * 0.5f + 0.5f);
					ln[x * 4 + 1] = Octet(ny * 0.5f + 0.5f);
					ln[x * 4 + 2] = Octet(nz * 0.5f + 0.5f);
					ln[x * 4 + 3] = 255;
					lo[x * 4 + 0] = Octet(p.occ);
					lo[x * 4 + 1] = Octet(p.rug);
					lo[x * 4 + 2] = Octet(p.met);
					lo[x * 4 + 3] = 255;
				}
			}
			if (moyenneLin3) {
				moyenneLin3[0] = (float32)(som[0] / (float64)n);
				moyenneLin3[1] = (float32)(som[1] / (float64)n);
				moyenneLin3[2] = (float32)(som[2] / (float64)n);
			}
			const bool ok = coul.SavePNG(ch[0]) && norm.SavePNG(ch[1]) && orm.SavePNG(ch[2]);
			return ok;
		}

		int32 NkMatieresBanc(bool mutation) {
			sNonPeriodique = mutation;
			// (09/11, 2e version) A LA TAILLE ECRITE (1024) : a 128 pixels, le joint de 4 mm du
			// carrelage tombait entre deux centres de pixel et le banc le disait « plat » --
			// il mesurait une autre carte que celle qu'on ecrit.
			const int32 T = 1024;
			NkVector<Point> pts;
			pts.Resize((usize)(T * T));
			int32 echecs = 0;
			NkPrintfSortie("[matieres] BANC%s : %d classes, cartes %dx%d\n", mutation ? " (MUTATION : bruit non periodique)" : "",
						   (int)kMatClasses, (int)T, (int)T);
			for (int32 c = 0; c < kMatClasses; ++c) {
				const NkMatiereParams &M = NkMatiereDe(c);
				if (!M.cartes)
					continue;
				Calculer(c, T, pts.Data());
				// (1) PERIODICITE : l'ecart moyen a travers le bord (colonne T-1 -> 0, ligne
				// T-1 -> 0) compare a l'ecart moyen entre deux voisins interieurs DANS LE MEME
				// SENS (une carte anisotrope -- le metal brosse -- varie vite dans un sens et
				// lentement dans l'autre). Signal : hauteur + luminance. Une carte qui se
				// repete sans couture a un rapport proche de 1.
				auto sig = [&](int32 x, int32 y) -> float64 {
					const Point &p = pts[y * T + x];
					return (float64)p.h + 0.2126 * p.r + 0.7152 * p.g + 0.0722 * p.b;
				};
				float64 bordU = 0.0, bordV = 0.0, dedU = 0.0, dedV = 0.0;
				int32 nd = 0;
				for (int32 i = 0; i < T; ++i) {
					bordU += math::NkFabs(sig(T - 1, i) - sig(0, i));
					bordV += math::NkFabs(sig(i, T - 1) - sig(i, 0));
					for (int32 x = 1; x < T; ++x) {
						dedU += math::NkFabs(sig(x, i) - sig(x - 1, i));
						dedV += math::NkFabs(sig(i, x) - sig(i, x - 1));
						++nd;
					}
				}
				bordU /= (float64)T;
				bordV /= (float64)T;
				dedU /= (float64)nd;
				dedV /= (float64)nd;
				const float64 rU = bordU / (dedU + 1e-6), rV = bordV / (dedV + 1e-6);
				// (2) moyenne et ecart-type de la couleur et de la hauteur
				float64 m = 0.0, m2 = 0.0, mh = 0.0, mh2 = 0.0;
				for (int32 i = 0; i < T * T; ++i) {
					const float64 l = 0.2126 * pts[i].r + 0.7152 * pts[i].g + 0.0722 * pts[i].b;
					m += l;
					m2 += l * l;
					mh += pts[i].h;
					mh2 += (float64)pts[i].h * pts[i].h;
				}
				m /= (float64)(T * T);
				m2 /= (float64)(T * T);
				mh /= (float64)(T * T);
				mh2 /= (float64)(T * T);
				const float64 sd = math::NkSqrt(m2 - m * m > 0.0 ? m2 - m * m : 0.0);
				const float64 sdh = math::NkSqrt(mh2 - mh * mh > 0.0 ? mh2 - mh * mh : 0.0);
				// Criteres ECRITS AVANT la mesure : couture <= 2,5 fois l'ecart interieur ;
				// relief present (ecart-type de hauteur > 0,01) ; modulation de couleur
				// dans [0,3 ; 1] (herbe : couleur propre, dans [0,04 ; 0,4]).
				const bool couture = rU <= 2.5 && rV <= 2.5;
				// le relief n'est exige que des classes qui en ont (ceramique polie : plate par nature)
				const bool relief = M.relief < 0.5f || sdh > 0.01;
				const bool moy = M.couleurPropre ? (m >= 0.04 && m <= 0.4) : (m >= 0.3 && m <= 1.0);
				const bool ok = couture && relief && moy;
				if (!ok)
					++echecs;
				NkPrintfSortie("[matieres]   %-10s couture u %.2f v %.2f | lum moy %.3f ecart %.3f | hauteur ecart %.3f : %s%s%s%s\n",
							   M.nom, rU, rV, m, sd, sdh, ok ? "VERT" : "ROUGE", couture ? "" : " (couture)",
							   relief ? "" : " (plat)", moy ? "" : " (moyenne)");
			}
			sNonPeriodique = false;
			NkPrintfSortie("[matieres] BANC : %d echec(s)\n", (int)echecs);
			NkFflush(NkFlux::Sortie);
			return echecs;
		}

	} // namespace renderer
} // namespace nkentseu
