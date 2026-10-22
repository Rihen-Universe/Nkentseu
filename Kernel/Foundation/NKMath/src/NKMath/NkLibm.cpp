// =============================================================================
// NKMath/NkLibm.cpp
// Voir NkLibm.h. Aucune inclusion de <math.h> ni de <cmath>.
//
// Les noyaux de sin et cos, la réduction moyenne modulo pi/2, exp et log
// suivent fdlibm (par musl). Avis de fdlibm, recopié comme il le demande :
//
//   Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
//   Developed at SunSoft, a Sun Microsystems, Inc. business.
//   Permission to use, copy, modify, and distribute this software is freely
//   granted, provided that this notice is preserved.
//
// La réduction des très grands angles (Payne et Hanek) et les fonctions
// exactes sont écrites ici.
//
// Le compilateur NE DOIT PAS réordonner les calculs flottants (pas de
// -ffast-math) : les additions « (t - r) - w » portent les bits perdus.
//
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// DATE : 2026-10-22
// =============================================================================

#include "pch.h"
#include "NKMath/NkLibm.h"

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h> // fourni par le compilateur : l'instruction sqrtsd
#define NK_LIBM_SSE2 1
#endif

namespace nkentseu {
	namespace math {
		namespace libm {

			namespace {

				union Mot {
						float64 f;
						uint64 u;
				};

				inline uint64 Bits(float64 x) noexcept {
					Mot m;
					m.f = x;
					return m.u;
				}

				inline float64 DeBits(uint64 u) noexcept {
					Mot m;
					m.u = u;
					return m.f;
				}

				inline uint32 Haut(float64 x) noexcept {
					return (uint32)(Bits(x) >> 32);
				}

				// 2^k, pour k dans [-1022, 1023].
				inline float64 Puissance2(int32 k) noexcept {
					return DeBits((uint64)(k + 1023) << 52);
				}

				// ── Les constantes de fdlibm ───────────────────────────────
				const float64 kInvPio2 = 6.36619772367581382433e-01; // 2/pi
				const float64 kPio2_1 = 1.57079632673412561417e+00;	 // 33 premiers bits de pi/2
				const float64 kPio2_1t = 6.07710050650619224932e-11; // pi/2 - kPio2_1
				const float64 kPio2_2 = 6.07710050630396597660e-11;	 // 33 bits suivants
				const float64 kPio2_2t = 2.02226624879595063154e-21;
				const float64 kPio2_3 = 2.02226624871116645580e-21; // 33 bits suivants
				const float64 kPio2_3t = 8.47842766036889956997e-32;

				const float64 kS1 = -1.66666666666666324348e-01;
				const float64 kS2 = 8.33333333332248946124e-03;
				const float64 kS3 = -1.98412698298579493134e-04;
				const float64 kS4 = 2.75573137070700676789e-06;
				const float64 kS5 = -2.50507602534068634195e-08;
#if defined(NK_LIBM_CONTRE_EPREUVE) && NK_LIBM_CONTRE_EPREUVE == 1
				// Contre-épreuve du banc (NKMath/bancs/banc_libm.cpp) : le dernier
				// terme du polynôme du sinus manque, le banc doit le voir. Jamais
				// défini dans le produit.
				const float64 kS6 = 0.0;
#else
				const float64 kS6 = 1.58969099521155010221e-10;
#endif

				const float64 kC1 = 4.16666666666666019037e-02;
				const float64 kC2 = -1.38888888888741095749e-03;
				const float64 kC3 = 2.48015872894767294178e-05;
				const float64 kC4 = -2.75573143513906633035e-07;
				const float64 kC5 = 2.08757232129817482790e-09;
				const float64 kC6 = -1.13596475577881948265e-11;

#if defined(NK_LIBM_CONTRE_EPREUVE) && NK_LIBM_CONTRE_EPREUVE == 2
// Contre-épreuve : UN bit de 2/pi faux (le 10e mot), les très grands angles doivent le montrer.
#define NK_LIBM_MOT9 (0x3991D639835339F4ULL ^ (1ull << 40))
#else
#define NK_LIBM_MOT9 0x3991D639835339F4ULL
#endif
				// 1 472 bits de 2/pi après la virgule, CALCULÉS en entiers exacts
				// (formule de Machin, pi = 16 atan(1/5) - 4 atan(1/239)), pas recopiés.
				const uint64 kDeuxSurPi[23] = {
					0xA2F9836E4E441529ULL, 0xFC2757D1F534DDC0ULL, 0xDB6295993C439041ULL, 0xFE5163ABDEBBC561ULL,
					0xB7246E3A424DD2E0ULL, 0x06492EEA09D1921CULL, 0xFE1DEB1CB129A73EULL, 0xE88235F52EBB4484ULL,
					0xE99C7026B45F7E41ULL, NK_LIBM_MOT9, 0x9C845F8BBDF9283BULL, 0x1FF897FFDE05980FULL,
					0xEF2F118B5A0A6D1FULL, 0x6D367ECF27CB09B7ULL, 0x4F463F669E5FEA2DULL, 0x7527BAC7EBE5F17BULL,
					0x3D0739F78A5292EAULL, 0x6BFB5FB11F8D5D08ULL, 0x56033046FC7B6BABULL, 0xF0CFBC209AF4361DULL,
					0xA9E391615EE61B08ULL, 0x6599855F14A06840ULL, 0x8DFFD8804D732731ULL,
				};

				// ── Les noyaux de fdlibm, sur [-pi/4, pi/4] ; y : la partie basse ──
				float64 NoyauSin(float64 x, float64 y, int32 iy) noexcept {
					const float64 z = x * x;
					const float64 w = z * z;
					const float64 r = kS2 + z * (kS3 + z * kS4) + z * w * (kS5 + z * kS6);
					const float64 v = z * x;
					if (iy == 0)
						return x + v * (kS1 + z * r);
					return x - ((z * (0.5 * y - v * r) - y) - v * kS1);
				}

				float64 NoyauCos(float64 x, float64 y) noexcept {
					const float64 z = x * x;
					float64 w = z * z;
					const float64 r = z * (kC1 + z * (kC2 + z * kC3)) + w * w * (kC4 + z * (kC5 + z * kC6));
					const float64 hz = 0.5 * z;
					w = 1.0 - hz;
					return w + (((1.0 - w) - hz) + (z * r - x * y));
				}

				// Le noyau de la tangente (fdlibm k_tan par musl), sur |x| <= pi/4 ;
				// impair != 0 : rend -1/tan. Remplace le quotient sin/cos (22/10 : 2 ULP
				// sur 60 points de la référence exacte, contre 1 pour la libm).
				const float64 kT[13] = {
					3.33333333333334091986e-01,	 1.33333333333201242699e-01,  5.39682539762260521377e-02,
					2.18694882948595424599e-02,	 8.86323982359930005737e-03,  3.59207910759131235356e-03,
					1.45620945432529025516e-03,	 5.88041240820264096874e-04,  2.46463134818469906812e-04,
					7.81794442939557092300e-05,	 7.14072491382608190305e-05,  -1.85586374855275456654e-05,
					2.59073051863633712884e-05,
				};
				const float64 kPio4 = 7.85398163397448278999e-01;
				const float64 kPio4lo = 3.06161699786838301793e-17;

				float64 SansMotBas(float64 x) noexcept {
					return DeBits(Bits(x) & 0xFFFFFFFF00000000ull);
				}

				float64 NoyauTan(float64 x, float64 y, int32 impair) noexcept {
					const uint32 hx = Haut(x);
					const bool grand = (hx & 0x7FFFFFFFu) >= 0x3FE59428u; // |x| >= 0,6744
					const bool negatif = (hx >> 31) != 0;
					if (grand) {
						if (negatif) {
							x = -x;
							y = -y;
						}
						x = (kPio4 - x) + (kPio4lo - y);
						y = 0.0;
					}
					const float64 z = x * x;
					float64 w = z * z;
					float64 r = kT[1] + w * (kT[3] + w * (kT[5] + w * (kT[7] + w * (kT[9] + w * kT[11]))));
					float64 v = z * (kT[2] + w * (kT[4] + w * (kT[6] + w * (kT[8] + w * (kT[10] + w * kT[12])))));
					float64 s = z * x;
					r = y + z * (s * (r + v) + y) + s * kT[0];
					w = x + r;
					if (grand) {
						s = 1.0 - 2.0 * (float64)impair;
						v = s - 2.0 * (x + (r - w * w / (w + s)));
						return negatif ? -v : v;
					}
					if (!impair)
						return w;
					// -1/(x + r) aurait jusqu'à 2 ULP d'erreur : calcul soigné.
					const float64 w0 = SansMotBas(w);
					v = r - (w0 - x); // w0 + v = r + x
					const float64 a = -1.0 / w;
					const float64 a0 = SansMotBas(a);
					return a0 + a * (1.0 + a0 * w0 + a0 * v);
				}

				// ── Produit exact de deux flottants (Dekker), sans fma ──────
				void ProduitExact(float64 a, float64 b, float64 &p, float64 &e) noexcept {
					const float64 c = 134217729.0; // 2^27 + 1
					p = a * b;
					float64 t = c * a;
					const float64 ah = t - (t - a);
					const float64 al = a - ah;
					t = c * b;
					const float64 bh = t - (t - b);
					const float64 bl = b - bh;
					e = ((ah * bh - p) + ah * bl + al * bh) + al * bl;
				}

				// ── 64 x 64 -> 128 bits, sans entier de 128 bits ────────────
				void Mul64(uint64 a, uint64 b, uint64 &hi, uint64 &lo) noexcept {
#if defined(__SIZEOF_INT128__)
					// Une seule instruction (mul) là où le compilateur a des entiers de 128 bits.
					const unsigned __int128 p = (unsigned __int128)a * b;
					hi = (uint64)(p >> 64);
					lo = (uint64)p;
#else
					const uint64 a0 = a & 0xFFFFFFFFull, a1 = a >> 32;
					const uint64 b0 = b & 0xFFFFFFFFull, b1 = b >> 32;
					const uint64 p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
					const uint64 milieu = (p00 >> 32) + (p01 & 0xFFFFFFFFull) + (p10 & 0xFFFFFFFFull);
					lo = (milieu << 32) | (p00 & 0xFFFFFFFFull);
					hi = p11 + (p01 >> 32) + (p10 >> 32) + (milieu >> 32);
#endif
				}

				// Zéros de tête d'un mot non nul.
				inline int32 Clz64(uint64 v) noexcept {
#if defined(_MSC_VER) && !defined(__clang__)
					unsigned long i;
					_BitScanReverse64(&i, v);
					return 63 - (int32)i;
#else
					return __builtin_clzll(v);
#endif
				}

				// Les 64 bits de 2/pi qui commencent au bit `debut` (1 = le premier
				// après la virgule). Les bits d'indice <= 0 valent zéro.
				uint64 Mot64(int32 debut) noexcept {
					const int32 p = debut - 1;
					if (p <= -64)
						return 0;
					if (p < 0)
						return kDeuxSurPi[0] >> (-p);
					const int32 i = p >> 6, d = p & 63;
					if (i >= 23)
						return 0;
					uint64 m = kDeuxSurPi[i] << d;
					if (d != 0 && i + 1 < 23)
						m |= kDeuxSurPi[i + 1] >> (64 - d);
					return m;
				}

				// Payne et Hanek : |x| >= 2^20 * pi/2, fini. Rend n (le quadrant)
				// et y = x - n*pi/2 en double-double, dans [-pi/4, pi/4].
				int32 ReduireGrand(float64 x, float64 *y) noexcept {
					const uint64 u = Bits(x);
					const bool negatif = (u >> 63) != 0;
					const int32 expo = (int32)((u >> 52) & 0x7FF);
					const uint64 m = (u & 0x000FFFFFFFFFFFFFull) | 0x0010000000000000ull; // x = m * 2^e
					const int32 e = expo - 1075;

					// Fenêtre de 192 bits de 2/pi : le bit de tête pèse 2^1 une
					// fois multiplié par 2^e (les poids >= 4 sont des tours entiers).
					const int32 s0 = e - 1;
					const uint64 w0 = Mot64(s0), w1 = Mot64(s0 + 64), w2 = Mot64(s0 + 128);

					// P = m * W, 245 bits ; la valeur cherchée est P / 2^190 modulo 4.
					uint64 h0, l0, h1, l1, h2, l2;
					Mul64(m, w0, h0, l0);
					Mul64(m, w1, h1, l1);
					Mul64(m, w2, h2, l2);
					(void)h0;
					const uint64 limb0 = l2;
					const uint64 limb1 = h2 + l1;
					const uint64 c1 = limb1 < h2 ? 1u : 0u;
					uint64 limb2 = h1 + l0;
					const uint64 c2a = limb2 < h1 ? 1u : 0u;
					(void)c2a; // la retenue vers les bits >= 192 ne compte pas (modulo 4)
					limb2 += c1;

					int32 n = (int32)((limb2 >> 62) & 3);
					uint64 fh = (limb2 << 2) | (limb1 >> 62); // la fraction, 128 bits
					uint64 fl = (limb1 << 2) | (limb0 >> 62);

					// Au plus près : une fraction >= 1/2 passe au quadrant suivant.
					bool fractionNegative = false;
					if ((fh >> 63) != 0) {
						n = (n + 1) & 3;
						fractionNegative = true;
						fl = ~fl + 1;
						fh = ~fh + (fl == 0 ? 1u : 0u);
					}

					float64 a = 0.0, b = 0.0;
					if (fh != 0 || fl != 0) {
						// Normaliser : le bit de tête en position 127.
						const int32 lz = fh != 0 ? Clz64(fh) : 64 + Clz64(fl);
						if (lz >= 64) {
							fh = fl << (lz - 64);
							fl = 0;
						} else if (lz > 0) {
							fh = (fh << lz) | (fl >> (64 - lz));
							fl <<= lz;
						}
						// |f| = (fh * 2^64 + fl) * 2^(-128 - lz)
						a = (float64)(fh >> 11) * Puissance2(-53 - lz);
						b = ((float64)(fh & 0x7FFu) * 18446744073709551616.0 + (float64)fl) * Puissance2(-128 - lz);
					}

					// (a + b) * pi/2, en double-double.
					const float64 kPio2h = 1.5707963267948966192e+00;
					const float64 kPio2l = 6.1232339957367660359e-17;
					float64 p, err;
					ProduitExact(a, kPio2h, p, err);
					float64 bas = err + (a * kPio2l + b * kPio2h);
					const float64 s = p + bas;
					bas = bas - (s - p);
					float64 y0 = s, y1 = bas;
					if (fractionNegative) {
						y0 = -y0;
						y1 = -y1;
					}
					if (negatif) {
						y0 = -y0;
						y1 = -y1;
						n = (4 - n) & 3;
					}
					y[0] = y0;
					y[1] = y1;
					return n;
				}

				// x - n*pi/2 en double-double (y[0] + y[1]), |y| <= pi/4 environ.
				// Rend n. |x| > pi/4, fini.
				int32 Reduire(float64 x, float64 *y) noexcept {
					const uint32 ix = Haut(x) & 0x7FFFFFFFu;
					if (ix >= 0x413921FBu) // |x| >= 2^20 * pi/2
						return ReduireGrand(x, y);

					// La réduction moyenne de fdlibm : trois tours de 33 bits de pi/2.
					// n = l'entier le plus proche de x * 2/pi (|n| < 2^20).
					// Sans branchement sur le signe (mesuré : un « q >= 0 ? 0,5 : -0,5 »
					// se trompait une fois sur deux sur des angles au hasard). 1,5 * 2^52 :
					// l'addition arrondit à l'entier, la soustraction rend la valeur
					// (aucun -ffast-math, voir l'en-tête).
					const float64 kVersEntier = 6755399441055744.0;
					const float64 fn = (x * kInvPio2 + kVersEntier) - kVersEntier;
					const int32 n = (int32)fn;
					float64 r = x - fn * kPio2_1; // exact : kPio2_1 a 33 bits, |fn| < 2^20
					float64 w = fn * kPio2_1t;
					y[0] = r - w;
					const int32 ex = (int32)(ix >> 20);
					int32 ey = (int32)((Haut(y[0]) >> 20) & 0x7FF);
					if (ex - ey > 16) { // annulation : second tour, 118 bits
						float64 t = r;
						w = fn * kPio2_2;
						r = t - w;
						w = fn * kPio2_2t - ((t - r) - w);
						y[0] = r - w;
						ey = (int32)((Haut(y[0]) >> 20) & 0x7FF);
						if (ex - ey > 49) { // troisième tour, 151 bits : tous les cas
							t = r;
							w = fn * kPio2_3;
							r = t - w;
							w = fn * kPio2_3t - ((t - r) - w);
							y[0] = r - w;
						}
					}
					y[1] = (r - y[0]) - w;
					return n;
				}

				// ── exp et log : les constantes de fdlibm ──────────────────
				const float64 kLn2Hi = 6.93147180369123816490e-01;
				const float64 kLn2Lo = 1.90821492927058770002e-10;
				const float64 kInvLn2 = 1.44269504088896338700e+00;
				const float64 kP1 = 1.66666666666666019037e-01;
				const float64 kP2 = -2.77777777770155933842e-03;
				const float64 kP3 = 6.61375632143793436117e-05;
				const float64 kP4 = -1.65339022054652515390e-06;
				const float64 kP5 = 4.13813679705723846039e-08;
				const float64 kLg1 = 6.666666666666735130e-01;
				const float64 kLg2 = 3.999999999940941908e-01;
				const float64 kLg3 = 2.857142874366239149e-01;
				const float64 kLg4 = 2.222219843214978396e-01;
				const float64 kLg5 = 1.818357216161805012e-01;
				const float64 kLg6 = 1.531383769920937332e-01;
				const float64 kLg7 = 1.479819860511658591e-01;

			} // namespace

			// ── 1. Exactes : Fabs, Copysign, Trunc, Floor, Ceil et Round sont
			// EN LIGNE dans NkLibm.h (22/10 : hors ligne, l'appel coûtait le
			// double de la libm, dont le compilateur intègre ces fonctions).
			float64 Sqrt(float64 x) noexcept {
#if defined(NK_LIBM_SSE2)
				return _mm_cvtsd_f64(_mm_sqrt_sd(_mm_set_sd(x), _mm_set_sd(x)));
#else
				return __builtin_sqrt(x);
#endif
			}

			float32 Sqrt(float32 x) noexcept {
#if defined(NK_LIBM_SSE2)
				return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(x)));
#else
				return __builtin_sqrtf(x);
#endif
			}

			// ── 2. Trigonométrie ───────────────────────────────────────────
			float64 Sin(float64 x) noexcept {
				const uint32 ix = Haut(x) & 0x7FFFFFFFu;
				if (ix <= 0x3FE921FBu) { // |x| <= pi/4
					if (ix < 0x3E500000u) // |x| < 2^-26 : sin(x) = x
						return x;
					return NoyauSin(x, 0.0, 0);
				}
				if (ix >= 0x7FF00000u) // infini ou NaN
					return x - x;
				// Les deux noyaux, puis un choix sans branchement : sur des angles
				// au hasard, un « switch » sur le quadrant se trompe trois fois sur
				// quatre (mesuré : plus lent que de calculer les deux).
				float64 y[2];
				const int32 n = Reduire(x, y);
				const float64 s = NoyauSin(y[0], y[1], 1);
				const float64 c = NoyauCos(y[0], y[1]);
				const float64 r = (n & 1) ? c : s;
				return (n & 2) ? -r : r;
			}

			float64 Cos(float64 x) noexcept {
				const uint32 ix = Haut(x) & 0x7FFFFFFFu;
				if (ix <= 0x3FE921FBu) {
					if (ix < 0x3E46A09Eu) // |x| < 2^-27 * sqrt(2) : cos(x) = 1
						return 1.0;
					return NoyauCos(x, 0.0);
				}
				if (ix >= 0x7FF00000u)
					return x - x;
				float64 y[2];
				const int32 n = Reduire(x, y);
				const float64 s = NoyauSin(y[0], y[1], 1);
				const float64 c = NoyauCos(y[0], y[1]);
				const float64 r = (n & 1) ? s : c;
				return ((n + 1) & 2) ? -r : r; // quadrants 1 et 2 : négatif
			}

			float64 Tan(float64 x) noexcept {
				const uint32 ix = Haut(x) & 0x7FFFFFFFu;
				if (ix <= 0x3FE921FBu) {
					if (ix < 0x3E400000u) // |x| < 2^-27 : tan(x) = x
						return x;
					return NoyauTan(x, 0.0, 0);
				}
				if (ix >= 0x7FF00000u)
					return x - x;
				float64 y[2];
				const int32 n = Reduire(x, y);
				return NoyauTan(y[0], y[1], n & 1);
			}

			// ── Les versions float32 : des noyaux de degré moindre, calculés en
			// double (fdlibm k_sinf / k_cosf par musl), et une réduction courte.
			namespace {
				const float64 kSf1 = -0x15555554cbac77.0p-55; // -0,166666666416265235595
				const float64 kSf2 = 0x111110896efbb2.0p-59;  //  0,0083333293858894631756
				const float64 kSf3 = -0x1a00f9e2cae774.0p-65; // -0,000198393348360966317347
				const float64 kSf4 = 0x16cd878c3b46a7.0p-71;  //  0,0000027183114939898219064
				const float64 kCf0 = -0x1ffffffd0c5e81.0p-54; // -0,499999997251031003120
				const float64 kCf1 = 0x155553e1053a42.0p-57;  //  0,0416666233237390631894
				const float64 kCf2 = -0x16c087e80f1e27.0p-62; // -0,00138867637746099294692
				const float64 kCf3 = 0x199342e0ee5069.0p-68;  //  0,0000243904487962774090654
				const float64 kPio2f_1 = 1.57079631090164184570e+00;  // 25 premiers bits de pi/2
				const float64 kPio2f_1t = 1.58932547735281966916e-08; // pi/2 - kPio2f_1

				union MotF {
						float32 f;
						uint32 u;
				};
				inline uint32 BitsF(float32 x) noexcept {
					MotF m;
					m.f = x;
					return m.u;
				}
				inline float32 DeBitsF(uint32 u) noexcept {
					MotF m;
					m.u = u;
					return m.f;
				}

				inline float64 NoyauSinf(float64 x) noexcept {
					const float64 z = x * x;
					const float64 w = z * z;
					const float64 r = kSf3 + z * kSf4;
					const float64 s = z * x;
					return (x + s * (kSf1 + z * kSf2)) + s * w * r;
				}

				inline float64 NoyauCosf(float64 x) noexcept {
					const float64 z = x * x;
					const float64 w = z * z;
					const float64 r = kCf2 + z * kCf3;
					return ((1.0 + z * kCf0) + w * kCf1) + (w * z) * r;
				}

				// y = x - n*pi/2 (en double, assez pour un résultat float) ; rend n.
				inline int32 ReduireF(float64 x, uint32 ix, float64 &y) noexcept {
					if (ix < 0x4DC90FDBu) { // |x| < 2^28 * pi/2
						const float64 kVersEntier = 6755399441055744.0;
						const float64 fn = (x * kInvPio2 + kVersEntier) - kVersEntier;
						y = x - fn * kPio2f_1 - fn * kPio2f_1t;
						return (int32)fn;
					}
					float64 yy[2];
					const int32 n = Reduire(x, yy);
					y = yy[0];
					return n;
				}
			} // namespace

			float32 Sin(float32 xf) noexcept {
				const uint32 ix = BitsF(xf) & 0x7FFFFFFFu;
				if (ix <= 0x3F490FDAu) { // |x| <= pi/4
					if (ix < 0x39800000u) // |x| < 2^-12 : sin(x) = x
						return xf;
					return (float32)NoyauSinf((float64)xf);
				}
				if (ix >= 0x7F800000u)
					return xf - xf;
				float64 y;
				const int32 n = ReduireF((float64)xf, ix, y);
				const float64 s = NoyauSinf(y), c = NoyauCosf(y);
				const float64 r = (n & 1) ? c : s;
				return (float32)((n & 2) ? -r : r);
			}

			float32 Cos(float32 xf) noexcept {
				const uint32 ix = BitsF(xf) & 0x7FFFFFFFu;
				if (ix <= 0x3F490FDAu) {
					if (ix < 0x39800000u) // |x| < 2^-12 : cos(x) = 1
						return 1.0f;
					return (float32)NoyauCosf((float64)xf);
				}
				if (ix >= 0x7F800000u)
					return xf - xf;
				float64 y;
				const int32 n = ReduireF((float64)xf, ix, y);
				const float64 s = NoyauSinf(y), c = NoyauCosf(y);
				const float64 r = (n & 1) ? s : c;
				return (float32)(((n + 1) & 2) ? -r : r);
			}

			float32 Tan(float32 x) noexcept {
				return (float32)Tan((float64)x);
			}

			// ── 3. Exponentielle et logarithme ─────────────────────────────
			float64 Scalbn(float64 x, int32 n) noexcept {
				if (n > 1023) {
					x *= 0x1p1023;
					n -= 1023;
					if (n > 1023) {
						x *= 0x1p1023;
						n -= 1023;
						if (n > 1023)
							n = 1023;
					}
				} else if (n < -1022) {
					// 0x1p-1022 * 0x1p53 : on remonte d'abord de 53 pour ne pas
					// arrondir deux fois dans les sous-normaux.
					x *= 0x1p-1022 * 0x1p53;
					n += 1022 - 53;
					if (n < -1022) {
						x *= 0x1p-1022 * 0x1p53;
						n += 1022 - 53;
						if (n < -1022)
							n = -1022;
					}
				}
				return x * Puissance2(n);
			}

			float64 Exp(float64 x) noexcept {
				uint32 hx = Haut(x);
				const int32 signe = (int32)(hx >> 31);
				hx &= 0x7FFFFFFFu;

				if (hx >= 0x4086232Bu) { // |x| >= 708,39...
					if (x != x)			 // NaN
						return x;
					if (x > 709.782712893383973096) // débordement (infini si x l'est)
						return x * 0x1p1023;
					if (x < -745.13321910194110842) // sous le plus petit sous-normal
						return 0.0;
				}

				float64 hi, lo;
				int32 k;
				if (hx > 0x3FD62E42u) { // |x| > ln2 / 2
					if (hx >= 0x3FF0A2B2u) {
						// Entier le plus proche sans branchement sur le signe (voir Reduire).
						const float64 kVersEntier = 6755399441055744.0;
						k = (int32)((kInvLn2 * x + kVersEntier) - kVersEntier);
					} else {
						k = 1 - signe - signe;
					}
					hi = x - k * kLn2Hi; // exact
					lo = k * kLn2Lo;
					x = hi - lo;
				} else if (hx > 0x3E300000u) { // |x| > 2^-28
					k = 0;
					hi = x;
					lo = 0.0;
				} else {
					return 1.0 + x;
				}

				const float64 xx = x * x;
				const float64 c = x - xx * (kP1 + xx * (kP2 + xx * (kP3 + xx * (kP4 + xx * kP5))));
				const float64 y = 1.0 + (x * c / (2.0 - c) - lo + hi);
				if (k >= -1021 && k <= 1023) // le cas courant : 2^k est un flottant normal
					return y * Puissance2(k);
				return Scalbn(y, k);
			}

			float64 Log(float64 x) noexcept {
				uint64 u = Bits(x);
				uint32 hx = (uint32)(u >> 32);
				int32 k = 0;
				if (hx < 0x00100000u || (hx >> 31) != 0) {
					if ((u << 1) == 0) // log(+-0) = -infini
						return -1.0 / (x * x);
					if ((hx >> 31) != 0) // log(négatif) = NaN
						return (x - x) / 0.0;
					// sous-normal : on remonte de 54
					k -= 54;
					x *= 0x1p54;
					u = Bits(x);
					hx = (uint32)(u >> 32);
				} else if (hx >= 0x7FF00000u) { // infini ou NaN
					return x;
				} else if (hx == 0x3FF00000u && (u << 32) == 0) {
					return 0.0;
				}

				// x = 2^k * (1 + f), avec 1 + f dans [sqrt(2)/2, sqrt(2)]
				hx += 0x3FF00000u - 0x3FE6A09Eu;
				k += (int32)(hx >> 20) - 0x3FF;
				hx = (hx & 0x000FFFFFu) + 0x3FE6A09Eu;
				u = ((uint64)hx << 32) | (u & 0xFFFFFFFFull);
				x = DeBits(u);

				const float64 f = x - 1.0;
				const float64 hfsq = 0.5 * f * f;
				const float64 s = f / (2.0 + f);
				const float64 z = s * s;
				const float64 w = z * z;
				const float64 t1 = w * (kLg2 + w * (kLg4 + w * kLg6));
				const float64 t2 = z * (kLg1 + w * (kLg3 + w * (kLg5 + w * kLg7)));
				const float64 R = t2 + t1;
				const float64 dk = (float64)k;
				return s * (hfsq + R) + dk * kLn2Lo - hfsq + f + dk * kLn2Hi;
			}

			// ── expf et logf : par table, sans division ─────────────────────
			// Tables CALCULÉES en précision étendue (module decimal de Python, 40
			// chiffres) puis arrondies au double le plus proche ; rien de recopié.
			namespace {
				// 2^(j/32), j = 0..31, en bits de double.
				const uint64 kExp2_32[32] = {
					0x3FF0000000000000ULL, 0x3FF059B0D3158574ULL, 0x3FF0B5586CF9890FULL, 0x3FF11301D0125B51ULL,
					0x3FF172B83C7D517BULL, 0x3FF1D4873168B9AAULL, 0x3FF2387A6E756238ULL, 0x3FF29E9DF51FDEE1ULL,
					0x3FF306FE0A31B715ULL, 0x3FF371A7373AA9CBULL, 0x3FF3DEA64C123422ULL, 0x3FF44E086061892DULL,
					0x3FF4BFDAD5362A27ULL, 0x3FF5342B569D4F82ULL, 0x3FF5AB07DD485429ULL, 0x3FF6247EB03A5585ULL,
					0x3FF6A09E667F3BCDULL, 0x3FF71F75E8EC5F74ULL, 0x3FF7A11473EB0187ULL, 0x3FF82589994CCE13ULL,
					0x3FF8ACE5422AA0DBULL, 0x3FF93737B0CDC5E5ULL, 0x3FF9C49182A3F090ULL, 0x3FFA5503B23E255DULL,
					0x3FFAE89F995AD3ADULL, 0x3FFB7F76F2FB5E47ULL, 0x3FFC199BDD85529CULL, 0x3FFCB720DCEF9069ULL,
					0x3FFD5818DCFBA487ULL, 0x3FFDFC97337B9B5FULL, 0x3FFEA4AFA2A490DAULL, 0x3FFF50765B6E4540ULL,
				};

				// [1, 2) en 16 intervalles : invc = 1/c (c au milieu), logc = -ln(invc)
				// calculé sur l'invc ARRONDI, si bien que ln(m) = logc + ln(m * invc).
				struct LogfEntree {
						float64 invc, logc;
				};
				const LogfEntree kLogf[16] = {
					{0.9696969696969697, 0.03077165866675366}, {0.9142857142857143, 0.08961215868968717},
					{0.8648648648648649, 0.14518200984449783}, {0.8205128205128205, 0.19782574332991992},
					{0.7804878048780488, 0.2478361639045812},  {0.7441860465116279, 0.2954642128938359},
					{0.7111111111111111, 0.3409265869705932},  {0.6808510638297872, 0.38441169891033206},
					{0.6530612244897959, 0.42608439531090014}, {0.6274509803921569, 0.46608972992459924},
					{0.6037735849056604, 0.5045560107523953},  {0.5818181818181818, 0.5415972824327444},
					{0.5614035087719298, 0.5773153650348236},  {0.5423728813559322, 0.6118015411059929},
					{0.5245901639344263, 0.6451379613735847},  {0.5079365079365079, 0.6773988235918061},
				};
				const float64 kLn2 = 0.69314718055994530942;
			} // namespace

			float32 Exp(float32 xf) noexcept {
				const float64 x = (float64)xf;
				if (!(x < 89.0)) // grand ou NaN
					return (x != x) ? xf : (float32)(x * 1e300); // infini
				if (x < -104.0) // sous le plus petit sous-normal float
					return 0.0f;
				// x = (k / 32) ln2 + r ln2 / 32, |r| <= 1/2 ; 2^(k/32) = 2^(k>>5) * 2^((k&31)/32)
				const float64 kVersEntier = 6755399441055744.0;
				const float64 z = x * (32.0 / kLn2);
				const float64 kd = (z + kVersEntier) - kVersEntier;
				const int64 ki = (int64)kd;
				const float64 r = z - kd;
				const float64 s = DeBits(kExp2_32[ki & 31] + ((uint64)(ki >> 5) << 52));
				// 2^(r/32) = e^u, u = r ln2 / 32, |u| <= 0,0109 : Taylor de degré 3
				// (erreur 6e-10, vingt fois sous le demi-ULP d'un float)
				const float64 u = r * (kLn2 / 32.0);
				const float64 p = 1.0 + u * (1.0 + u * (0.5 + u * (1.0 / 6.0)));
				return (float32)(s * p);
			}

			float32 Log(float32 xf) noexcept {
				uint32 u = BitsF(xf);
				int32 ajuste = 0;
				if (u - 0x00800000u >= 0x7F800000u - 0x00800000u) { // hors des normaux positifs
					if ((u << 1) == 0)									 // log(+-0) = -infini
						return -1.0f / (xf * xf);
					if (u == 0x7F800000u) // +infini
						return xf;
					if ((u >> 31) != 0 || u > 0x7F800000u) // négatif ou NaN
						return (xf - xf) / (xf - xf);
					xf *= 8388608.0f; // sous-normal : on remonte de 2^23
					u = BitsF(xf);
					ajuste = -23;
				}
				// Près de 1, le résultat est petit : l'erreur ABSOLUE de la table (1e-10)
				// devient une grosse erreur RELATIVE (le banc l'a vu : 2 ULP en 1,00096).
				// On calcule alors ln(1 + f) directement, f = x - 1 exact :
				// 2 atanh(s), s = f / (2 + f), |s| < 0,033, erreur relative < 1e-16.
				if (u - 0x3F700000u < 0x3F880000u - 0x3F700000u) { // x dans [0,9375 ; 1,0625)
					const float64 f = (float64)xf - 1.0;
					const float64 s = f / (2.0 + f);
					const float64 s2 = s * s;
					return (float32)(2.0 * s *
									 (1.0 + s2 * ((1.0 / 3.0) + s2 * (0.2 + s2 * ((1.0 / 7.0) + s2 * (1.0 / 9.0))))));
				}
				const int32 k = (int32)(u >> 23) - 127 + ajuste;
				const LogfEntree &e = kLogf[(u >> 19) & 15];
				const float64 m = (float64)DeBitsF((u & 0x007FFFFFu) | 0x3F800000u); // [1, 2)
				const float64 r = m * e.invc - 1.0;									  // |r| < 1/31
				const float64 r2 = r * r;
				// ln(1 + r), degré 5 (erreur < 2e-10)
				const float64 p = r - 0.5 * r2 + r * r2 * ((1.0 / 3.0) - 0.25 * r + 0.2 * r2);
				return (float32)((float64)k * kLn2 + e.logc + p);
			}

		} // namespace libm
	} // namespace math
} // namespace nkentseu
