#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkBlockCompress.h — COMPRESSION PAR BLOCS, maison
//
// ── POURQUOI CE FICHIER EXISTE, ET CE QU'IL VAUT ─────────────────────────────
// Mesure du 2026-09-05 sur `renderdemo --demo=20`, vrai GPU : un actif texture
// BRUT (pixels + mipmaps) supprime bien les 1 965 ms de decodage PNG… et les
// remplace par 1 929 ms de LECTURE DISQUE, parce qu'il lit 538 Mo la ou les PNG
// en font 58. Gain net : zero. **La compression par blocs n'est donc pas un
// palier optionnel du format d'actif : c'est ce qui le rend payant.**
//
// BC1 code 16 pixels dans 8 octets — un demi-octet par pixel, soit **8 fois
// moins** qu'un RGBA8. Les 538 Mo tombent a 67 Mo, sous les 58 Mo de PNG : la
// lecture devient plus courte ET le decodage reste supprime. La VRAM suit.
//
// ⚠️ CE QUE CET ENCODEUR EST, ET CE QU'IL N'EST PAS.
// C'est un encodeur BC1 **par boite englobante avec retrait** — la methode
// classique et courte. Il n'est PAS de qualite de production : un encodeur serieux
// (ISPC Texture Compressor, NVTT) essaie plusieurs axes, plusieurs partitions et
// gagne 1 a 3 dB de PSNR. Il est ici pour **prouver la chaine de bout en bout**
// avec du code de la maison, et il est mesure comme tel (voir le banc : PSNR
// cite, pas suppose). BC7 est **nomme, non fait** : il a huit modes de
// partitionnement et c'est un lot a lui seul.
//
// ⚠️ BC1 EST AVEC PERTE ET N'A PAS D'ALPHA. Il ne convient pas a une carte de
// normales (les artefacts de bloc s'y voient comme des facettes) ni a une texture
// a canal alpha utile. Le four ne l'applique donc jamais tout seul : il faut le
// demander. C'est dit dans `NkTextureOven`.
//
// Reference du format : Microsoft, « BC1 Format » (Direct3D 11 Block Compression),
// et la specification `EXT_texture_compression_s3tc` d'OpenGL — les deux decrivent
// la meme disposition : deux couleurs RGB565 puis 16 indices de 2 bits.
// =============================================================================

#ifndef NKENTSEU_IMAGE_CORE_NKBLOCKCOMPRESS_H
#define NKENTSEU_IMAGE_CORE_NKBLOCKCOMPRESS_H

#include "NKContainers/Sequential/NkVector.h"
#include "NKCore/NkTypes.h"

#include <cstring>

namespace nkentseu {

	class NkBlockCompress {
		public:
			// Un bloc BC1 = 8 octets. Une image WxH en occupe
			// ceil(W/4) * ceil(H/4) * 8.
			static constexpr nk_uint32 kOctetsParBlocBC1 = 8u;

			[[nodiscard]] static nk_uint64 TailleBC1(nk_uint32 w, nk_uint32 h) noexcept {
				const nk_uint64 bx = (w + 3u) / 4u;
				const nk_uint64 by = (h + 3u) / 4u;
				return bx * by * kOctetsParBlocBC1;
			}

			// ── ENCODAGE ─────────────────────────────────────────────────────
			// `rgba` : W x H pixels, 4 octets par pixel, lignes serrees.
			// `out`  : redimensionne a `TailleBC1(w, h)`.
			//
			// L'alpha est IGNORE (BC1 sans alpha). Les blocs de bord d'une image
			// dont les dimensions ne sont pas multiples de 4 sont completes en
			// repliant le dernier pixel — pas en noir, qui tirerait la moyenne du
			// bloc et se verrait sur le bord.
			static void EncoderBC1(const nk_uint8 *rgba, nk_uint32 w, nk_uint32 h, NkVector<nk_uint8> &out) noexcept {
				const nk_uint32 bx = (w + 3u) / 4u;
				const nk_uint32 by = (h + 3u) / 4u;
				out.Clear();
				out.Resize(nk_size(TailleBC1(w, h)));

				for (nk_uint32 by_ = 0; by_ < by; ++by_) {
					for (nk_uint32 bx_ = 0; bx_ < bx; ++bx_) {
						nk_uint8 bloc[16 * 3]; // RGB seulement
						for (nk_uint32 j = 0; j < 4u; ++j) {
							const nk_uint32 y = Borne(by_ * 4u + j, h);
							for (nk_uint32 i = 0; i < 4u; ++i) {
								const nk_uint32 x = Borne(bx_ * 4u + i, w);
								const nk_uint8 *p = rgba + (nk_size(y) * w + x) * 4u;
								nk_uint8 *d = bloc + (j * 4u + i) * 3u;
								d[0] = p[0];
								d[1] = p[1];
								d[2] = p[2];
							}
						}
						EncoderUnBloc(bloc, out.Data() + nk_size(by_ * bx + bx_) * kOctetsParBlocBC1);
					}
				}
			}

			// ── DECODAGE ─────────────────────────────────────────────────────
			// Reproduit EXACTEMENT ce que le materiel fera : c'est cette image-la
			// qu'il faut comparer a l'originale pour juger la perte, pas une
			// approximation. `out` : W x H x 4, alpha a 255.
			static void DecoderBC1(const nk_uint8 *blocs, nk_uint32 w, nk_uint32 h,
								   NkVector<nk_uint8> &out) noexcept {
				const nk_uint32 bx = (w + 3u) / 4u;
				const nk_uint32 by = (h + 3u) / 4u;
				out.Clear();
				out.Resize(nk_size(w) * nk_size(h) * 4u);

				for (nk_uint32 by_ = 0; by_ < by; ++by_) {
					for (nk_uint32 bx_ = 0; bx_ < bx; ++bx_) {
						const nk_uint8 *b = blocs + nk_size(by_ * bx + bx_) * kOctetsParBlocBC1;
						const nk_uint16 c0 = nk_uint16(nk_uint16(b[0]) | (nk_uint16(b[1]) << 8));
						const nk_uint16 c1 = nk_uint16(nk_uint16(b[2]) | (nk_uint16(b[3]) << 8));
						nk_uint8 pal[4][3];
						ConstruirePalette(c0, c1, pal);
						const nk_uint32 idx = nk_uint32(b[4]) | (nk_uint32(b[5]) << 8) | (nk_uint32(b[6]) << 16) |
											  (nk_uint32(b[7]) << 24);
						for (nk_uint32 j = 0; j < 4u; ++j) {
							const nk_uint32 y = by_ * 4u + j;
							if (y >= h)
								break;
							for (nk_uint32 i = 0; i < 4u; ++i) {
								const nk_uint32 x = bx_ * 4u + i;
								if (x >= w)
									break;
								const nk_uint32 k = (idx >> ((j * 4u + i) * 2u)) & 3u;
								nk_uint8 *d = out.Data() + (nk_size(y) * w + x) * 4u;
								d[0] = pal[k][0];
								d[1] = pal[k][1];
								d[2] = pal[k][2];
								d[3] = 255u;
							}
						}
					}
				}
			}

			// ── MESURE DE LA PERTE ───────────────────────────────────────────
			// PSNR = 10 * log10(MAX^2 / EQM), MAX = 255, EQM = erreur quadratique
			// moyenne sur les canaux R, G et B (l'alpha est hors sujet en BC1).
			// C'est la metrique standard de l'imagerie ; on la CITE plutot que de
			// dire « ça se ressemble ». Rend 99.0 quand les images sont
			// identiques (EQM nulle : le logarithme n'existe pas).
			[[nodiscard]] static float64 PSNR(const nk_uint8 *a, const nk_uint8 *b, nk_uint32 w,
											  nk_uint32 h) noexcept {
				float64 somme = 0.0;
				nk_uint64 n = 0;
				for (nk_size i = 0; i < nk_size(w) * nk_size(h); ++i) {
					for (nk_uint32 k = 0; k < 3u; ++k) {
						const float64 d = float64(a[i * 4u + k]) - float64(b[i * 4u + k]);
						somme += d * d;
						++n;
					}
				}
				if (n == 0)
					return 0.0;
				const float64 eqm = somme / float64(n);
				if (eqm <= 0.0)
					return 99.0;
				return 10.0 * Log10(255.0 * 255.0 / eqm);
			}

			// Ecart maximal sur un canal, en niveaux (0..255). Le PSNR est une
			// moyenne : il peut rester bon alors qu'un pixel est tres faux. Les
			// deux se lisent ensemble.
			[[nodiscard]] static nk_uint32 EcartMax(const nk_uint8 *a, const nk_uint8 *b, nk_uint32 w,
													nk_uint32 h) noexcept {
				nk_uint32 pire = 0;
				for (nk_size i = 0; i < nk_size(w) * nk_size(h); ++i) {
					for (nk_uint32 k = 0; k < 3u; ++k) {
						const nk_uint32 x = nk_uint32(a[i * 4u + k]);
						const nk_uint32 y = nk_uint32(b[i * 4u + k]);
						const nk_uint32 d = (x > y) ? (x - y) : (y - x);
						if (d > pire)
							pire = d;
					}
				}
				return pire;
			}

		private:
			static nk_uint32 Borne(nk_uint32 v, nk_uint32 max) noexcept {
				return v < max ? v : (max > 0u ? max - 1u : 0u);
			}

			// Logarithme decimal sans <cmath> : serie autour de ln, suffisante
			// pour un rapport de mesure (5 chiffres significatifs).
			static float64 Log10(float64 x) noexcept {
				if (x <= 0.0)
					return 0.0;
				// x = m * 2^e avec m dans [1, 2)
				int e = 0;
				while (x >= 2.0) {
					x *= 0.5;
					++e;
				}
				while (x < 1.0) {
					x *= 2.0;
					--e;
				}
				// ln(m) par atanh : ln(m) = 2 * atanh((m-1)/(m+1))
				const float64 z = (x - 1.0) / (x + 1.0);
				const float64 z2 = z * z;
				float64 terme = z, somme = z;
				for (int k = 3; k <= 21; k += 2) {
					terme *= z2;
					somme += terme / float64(k);
				}
				const float64 ln = 2.0 * somme + float64(e) * 0.6931471805599453; // ln 2
				return ln * 0.4342944819032518;									  // 1 / ln 10
			}

			static nk_uint16 Vers565(nk_uint32 r, nk_uint32 g, nk_uint32 b) noexcept {
				return nk_uint16(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
			}

			// Dequantification EXACTE du materiel : les bits de poids fort sont
			// recopies dans les bits de poids faible (5->8 et 6->8). Une simple
			// multiplication par 8 donnerait 248 au lieu de 255 pour le blanc, et
			// tout le banc mesurerait cette erreur-la au lieu de la compression.
			static void De565(nk_uint16 c, nk_uint8 *rgb) noexcept {
				const nk_uint32 r = (c >> 11) & 31u;
				const nk_uint32 g = (c >> 5) & 63u;
				const nk_uint32 b = c & 31u;
				rgb[0] = nk_uint8((r << 3) | (r >> 2));
				rgb[1] = nk_uint8((g << 2) | (g >> 4));
				rgb[2] = nk_uint8((b << 3) | (b >> 2));
			}

			static void ConstruirePalette(nk_uint16 c0, nk_uint16 c1, nk_uint8 pal[4][3]) noexcept {
				De565(c0, pal[0]);
				De565(c1, pal[1]);
				if (c0 > c1) {
					// Quatre couleurs : deux interpolees aux tiers.
					for (int k = 0; k < 3; ++k) {
						pal[2][k] = nk_uint8((2u * pal[0][k] + pal[1][k] + 1u) / 3u);
						pal[3][k] = nk_uint8((pal[0][k] + 2u * pal[1][k] + 1u) / 3u);
					}
				} else {
					// Trois couleurs + noir transparent. Cet encodeur ne produit
					// jamais ce mode (il force c0 > c1), mais le DECODEUR doit le
					// connaitre : il sert a relire un bloc venu d'ailleurs.
					for (int k = 0; k < 3; ++k) {
						pal[2][k] = nk_uint8((pal[0][k] + pal[1][k] + 1u) / 2u);
						pal[3][k] = 0u;
					}
				}
			}

			// Un bloc : boite englobante, retrait, quantification, puis choix du
			// plus proche dans la palette REELLE (celle que le materiel calculera).
			static void EncoderUnBloc(const nk_uint8 *rgb16, nk_uint8 *sortie) noexcept {
				nk_uint32 mn[3] = {255u, 255u, 255u};
				nk_uint32 mx[3] = {0u, 0u, 0u};
				for (int p = 0; p < 16; ++p) {
					for (int k = 0; k < 3; ++k) {
						const nk_uint32 v = rgb16[p * 3 + k];
						if (v < mn[k])
							mn[k] = v;
						if (v > mx[k])
							mx[k] = v;
					}
				}

				// Retrait de 1/16 de l'etendue : les extremes d'un bloc sont des
				// valeurs isolees ; les garder comme bornes gaspille les deux
				// couleurs interpolees. C'est le « inset » classique.
				for (int k = 0; k < 3; ++k) {
					const nk_uint32 etendue = (mx[k] - mn[k]) >> 4;
					mn[k] = (mn[k] + etendue <= 255u) ? mn[k] + etendue : 255u;
					mx[k] = (mx[k] >= etendue) ? mx[k] - etendue : 0u;
					if (mn[k] > mx[k]) {
						const nk_uint32 t = mn[k];
						mn[k] = mx[k];
						mx[k] = t;
					}
				}

				nk_uint16 c0 = Vers565(mx[0], mx[1], mx[2]);
				nk_uint16 c1 = Vers565(mn[0], mn[1], mn[2]);
				if (c0 < c1) {
					const nk_uint16 t = c0;
					c0 = c1;
					c1 = t;
				}
				if (c0 == c1) {
					// Bloc uni : il faut quand meme c0 > c1 pour rester en mode
					// quatre couleurs (sinon l'indice 3 devient du noir
					// transparent, et un bloc uni deviendrait troue).
					if (c1 > 0u)
						--c1;
					else
						c0 = 1u;
				}

				nk_uint8 pal[4][3];
				ConstruirePalette(c0, c1, pal);

				nk_uint32 indices = 0u;
				for (int p = 0; p < 16; ++p) {
					const nk_uint8 *px = rgb16 + p * 3;
					nk_uint32 meilleur = 0u;
					nk_uint32 meilleureDist = 0xFFFFFFFFu;
					for (nk_uint32 k = 0; k < 4u; ++k) {
						const int dr = int(px[0]) - int(pal[k][0]);
						const int dg = int(px[1]) - int(pal[k][1]);
						const int db = int(px[2]) - int(pal[k][2]);
						// Distance ponderee par la sensibilite de l'oeil (les
						// memes poids que la luminance BT.601, en entiers) : a
						// erreur egale, une derive dans le vert se voit plus.
						const nk_uint32 d = nk_uint32(2 * dr * dr + 4 * dg * dg + db * db);
						if (d < meilleureDist) {
							meilleureDist = d;
							meilleur = k;
						}
					}
					indices |= (meilleur & 3u) << (nk_uint32(p) * 2u);
				}

				sortie[0] = nk_uint8(c0 & 0xFFu);
				sortie[1] = nk_uint8((c0 >> 8) & 0xFFu);
				sortie[2] = nk_uint8(c1 & 0xFFu);
				sortie[3] = nk_uint8((c1 >> 8) & 0xFFu);
				sortie[4] = nk_uint8(indices & 0xFFu);
				sortie[5] = nk_uint8((indices >> 8) & 0xFFu);
				sortie[6] = nk_uint8((indices >> 16) & 0xFFu);
				sortie[7] = nk_uint8((indices >> 24) & 0xFFu);
			}
	};

} // namespace nkentseu

#endif // NKENTSEU_IMAGE_CORE_NKBLOCKCOMPRESS_H
