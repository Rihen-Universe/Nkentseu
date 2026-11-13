// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/PathTracer/NkFinitionImage.cpp
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// La finition de l'image du traceur : voir NkFinitionImage.h.

#include "NKRenderer/Tools/PathTracer/NkFinitionImage.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKCore/Text/NkSortie.h"
#include "NKMath/NkFunctions.h"
#include "NKPlatform/NkEnv.h"

namespace nkentseu {
	namespace renderer {

		namespace {
			bool Mutation() {
				static const bool m = env::GetEnvVar("NK_FINITION_MUTATION") != nullptr;
				return m;
			}
			float32 Lum3(const float32 *c) {
				return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];
			}
			int32 Borne(int32 v, int32 a, int32 b) {
				return v < a ? a : (v > b ? b : v);
			}

			// une moyenne glissante (fenetre 2r + 1, bords prolonges), en x puis en y, sur 3 canaux
			void Boite(NkVector<float32> &a, NkVector<float32> &t, int32 L, int32 H, int32 r) {
				const float32 inv = 1.f / (float32)(2 * r + 1);
				t.Resize(a.Size());
				for (int32 y = 0; y < H; ++y)
					for (int32 c = 0; c < 3; ++c) {
						const float32 *l = a.Data() + (usize)y * (usize)L * 3u + (usize)c;
						float32 *o = t.Data() + (usize)y * (usize)L * 3u + (usize)c;
						float32 s = 0.f;
						for (int32 k = -r; k <= r; ++k)
							s += l[(usize)Borne(k, 0, L - 1) * 3u];
						for (int32 x = 0; x < L; ++x) {
							o[(usize)x * 3u] = s * inv;
							s += l[(usize)Borne(x + r + 1, 0, L - 1) * 3u] - l[(usize)Borne(x - r, 0, L - 1) * 3u];
						}
					}
				for (int32 x = 0; x < L; ++x)
					for (int32 c = 0; c < 3; ++c) {
						const usize pas = (usize)L * 3u;
						const float32 *l = t.Data() + (usize)x * 3u + (usize)c;
						float32 *o = a.Data() + (usize)x * 3u + (usize)c;
						float32 s = 0.f;
						for (int32 k = -r; k <= r; ++k)
							s += l[(usize)Borne(k, 0, H - 1) * pas];
						for (int32 y = 0; y < H; ++y) {
							o[(usize)y * pas] = s * inv;
							s += l[(usize)Borne(y + r + 1, 0, H - 1) * pas] - l[(usize)Borne(y - r, 0, H - 1) * pas];
						}
					}
			}
			// une gaussienne d'ecart type `sigma` pixels : trois moyennes glissantes (Wells 1986)
			void Gauss(NkVector<float32> &a, NkVector<float32> &t, int32 L, int32 H, float32 sigma) {
				const float32 w = math::NkSqrt(4.f * sigma * sigma + 1.f);
				int32 r = (int32)((w - 1.f) * 0.5f + 0.5f);
				r = r < 1 ? 1 : r;
				for (int32 k = 0; k < 3; ++k)
					Boite(a, t, L, H, r);
			}

			// la part au-dessus du seuil (seuil doux : la meme teinte, la luminance en exces)
			void Exces(const float32 *rgb, usize n, float32 seuil, NkVector<float32> &br) {
				br.Resize(n * 3u);
				for (usize i = 0; i < n; ++i) {
					const float32 l = Lum3(rgb + i * 3u);
					const float32 k = l > seuil ? (l - seuil) / l : 0.f;
					for (int32 c = 0; c < 3; ++c)
						br[i * 3u + (usize)c] = rgb[i * 3u + (usize)c] * k;
				}
			}

			// 1. LE HALO : trois gaussiennes (r, 2r, 4r ; 0,5 0,3 0,2), energie conservee
			void Halo(const NkFinition &f, float32 *rgb, int32 L, int32 H) {
				const usize n = (usize)L * (usize)H;
				NkVector<float32> br, g, t, acc;
				Exces(rgb, n, f.bloomSeuil, br);
				acc.Resize(n * 3u);
				for (usize i = 0; i < n * 3u; ++i)
					acc[i] = 0.f;
				const float32 r0 = math::NkMax(0.5f, f.bloomRayon * (float32)H);
				const float32 sig[3] = {r0, 2.f * r0, 4.f * r0}, pds[3] = {0.5f, 0.3f, 0.2f};
				for (int32 k = 0; k < 3; ++k) {
					g = br;
					Gauss(g, t, L, H, sig[k]);
					for (usize i = 0; i < n * 3u; ++i)
						acc[i] += pds[k] * g[i];
				}
				const float32 b = math::NkMin(1.f, f.bloom);
				const bool mut = Mutation();
				for (usize i = 0; i < n * 3u; ++i)
					rgb[i] += b * (acc[i] - (mut ? 0.f : br[i]));
			}

			// 2. LES ETOILES : N/2 directions, decroissance exponentielle, calculees au quart de la taille (le reflet
			// est doux) puis remontees en bilineaire ; energie conservee (noyau normalise, moyennes de cellules)
			void Etoiles(const NkFinition &f, float32 *rgb, int32 L, int32 H) {
				const usize n = (usize)L * (usize)H;
				NkVector<float32> br;
				Exces(rgb, n, f.bloomSeuil, br);
				const int32 q = 4, l4 = (L + q - 1) / q, h4 = (H + q - 1) / q;
				NkVector<float32> pt, st;
				pt.Resize((usize)l4 * (usize)h4 * 3u);
				st.Resize(pt.Size());
				for (usize i = 0; i < pt.Size(); ++i)
					pt[i] = st[i] = 0.f;
				for (int32 y = 0; y < H; ++y)
					for (int32 x = 0; x < L; ++x)
						for (int32 c = 0; c < 3; ++c)
							pt[((usize)(y / q) * (usize)l4 + (usize)(x / q)) * 3u + (usize)c] += br[((usize)y * (usize)L + (usize)x) * 3u + (usize)c];
				// pt : SOMMES par cellule (l'energie) ; le noyau les repartit
				int32 nb = f.branches < 4 ? 4 : (f.branches > 16 ? 16 : f.branches);
				nb = (nb / 2) * 2;
				const int32 dirs = nb / 2;
				const float32 tau = math::NkMax(1.f, 0.03f * (float32)h4);
				const int32 T = (int32)(4.f * tau);
				float32 z = 0.f;
				for (int32 t = 1; t <= T; ++t)
					z += 2.f * math::NkExp(-(float32)t / tau);
				z *= (float32)dirs;
				for (int32 j = 0; j < dirs; ++j) {
					const float32 a = 3.14159265f * (float32)j / (float32)dirs;
					const float32 dx = math::NkCos(a), dy = math::NkSin(a);
					for (int32 y = 0; y < h4; ++y)
						for (int32 x = 0; x < l4; ++x) {
							const float32 *s = pt.Data() + ((usize)y * (usize)l4 + (usize)x) * 3u;
							if (s[0] + s[1] + s[2] <= 0.f)
								continue;
							// diffusion (scatter) : chaque cellule envoie sa lumiere le long des branches
							for (int32 t = 1; t <= T; ++t) {
								const float32 w = math::NkExp(-(float32)t / tau) / z;
								for (int32 sg = -1; sg <= 1; sg += 2) {
									const int32 xx = x + (int32)math::NkFloor((float32)(sg * t) * dx + 0.5f);
									const int32 yy = y + (int32)math::NkFloor((float32)(sg * t) * dy + 0.5f);
									if (xx < 0 || yy < 0 || xx >= l4 || yy >= h4)
										continue;
									float32 *o = st.Data() + ((usize)yy * (usize)l4 + (usize)xx) * 3u;
									for (int32 c = 0; c < 3; ++c)
										o[c] += w * s[c];
								}
							}
						}
				}
				// remontee : la cellule (somme) redevient une densite par pixel, en bilineaire
				const float32 e = math::NkMin(1.f, f.eblouissement);
				const float32 parPixel = 1.f / (float32)(q * q);
				const bool mut = Mutation();
				for (int32 y = 0; y < H; ++y) {
					const float32 fy = ((float32)y + 0.5f) / (float32)q - 0.5f;
					const int32 y0 = Borne((int32)math::NkFloor(fy), 0, h4 - 1), y1 = Borne(y0 + 1, 0, h4 - 1);
					const float32 ty = math::NkMin(1.f, math::NkMax(0.f, fy - (float32)y0));
					for (int32 x = 0; x < L; ++x) {
						const float32 fx = ((float32)x + 0.5f) / (float32)q - 0.5f;
						const int32 x0 = Borne((int32)math::NkFloor(fx), 0, l4 - 1), x1 = Borne(x0 + 1, 0, l4 - 1);
						const float32 tx = math::NkMin(1.f, math::NkMax(0.f, fx - (float32)x0));
						const usize i = ((usize)y * (usize)L + (usize)x) * 3u;
						for (int32 c = 0; c < 3; ++c) {
							const float32 v00 = st[((usize)y0 * (usize)l4 + (usize)x0) * 3u + (usize)c];
							const float32 v10 = st[((usize)y0 * (usize)l4 + (usize)x1) * 3u + (usize)c];
							const float32 v01 = st[((usize)y1 * (usize)l4 + (usize)x0) * 3u + (usize)c];
							const float32 v11 = st[((usize)y1 * (usize)l4 + (usize)x1) * 3u + (usize)c];
							const float32 v = ((v00 * (1.f - tx) + v10 * tx) * (1.f - ty) + (v01 * (1.f - tx) + v11 * tx) * ty) * parPixel;
							rgb[i + (usize)c] += e * (v - (mut ? 0.f : br[i + (usize)c]));
						}
					}
				}
			}

			// 3. L'ABERRATION LATERALE : le rouge lu plus pres du centre (il sort plus loin), le bleu plus loin
			void Aberration(const NkFinition &f, float32 *rgb, int32 L, int32 H) {
				const usize n = (usize)L * (usize)H;
				NkVector<float32> src;
				src.Resize(n * 3u);
				for (usize i = 0; i < n * 3u; ++i)
					src[i] = rgb[i];
				const float32 cx = 0.5f * (float32)L, cy = 0.5f * (float32)H;
				const float32 rc = math::NkSqrt(cx * cx + cy * cy);
				const float32 k = (Mutation() ? -1.f : 1.f) * f.aberration / math::NkMax(1.f, rc);
				const float32 echelle[2] = {1.f - k, 1.f + k}; // rouge, bleu
				const int32 canal[2] = {0, 2};
				for (int32 j = 0; j < 2; ++j)
					for (int32 y = 0; y < H; ++y)
						for (int32 x = 0; x < L; ++x) {
							const float32 sx = cx + ((float32)x + 0.5f - cx) * echelle[j] - 0.5f;
							const float32 sy = cy + ((float32)y + 0.5f - cy) * echelle[j] - 0.5f;
							const int32 x0 = (int32)math::NkFloor(sx), y0 = (int32)math::NkFloor(sy);
							const float32 tx = sx - (float32)x0, ty = sy - (float32)y0;
							const int32 xa = Borne(x0, 0, L - 1), xb = Borne(x0 + 1, 0, L - 1);
							const int32 ya = Borne(y0, 0, H - 1), yb = Borne(y0 + 1, 0, H - 1);
							const usize c = (usize)canal[j];
							const float32 v = (src[((usize)ya * (usize)L + (usize)xa) * 3u + c] * (1.f - tx) +
											   src[((usize)ya * (usize)L + (usize)xb) * 3u + c] * tx) *
												  (1.f - ty) +
											  (src[((usize)yb * (usize)L + (usize)xa) * 3u + c] * (1.f - tx) +
											   src[((usize)yb * (usize)L + (usize)xb) * 3u + c] * tx) *
												  ty;
							rgb[((usize)y * (usize)L + (usize)x) * 3u + c] = v;
						}
			}

			// 4. LA VIGNETTE : cos^4 de l'angle de chaque pixel a l'axe, dosee
			void Vignette(const NkFinition &f, float32 *rgb, int32 L, int32 H, float32 tanDemiV) {
				const float32 asp = (float32)L / (float32)H;
				const float32 v = math::NkMin(1.f, f.vignette);
				const bool mut = Mutation();
				for (int32 y = 0; y < H; ++y)
					for (int32 x = 0; x < L; ++x) {
						const float32 px = (((float32)x + 0.5f) / (float32)L * 2.f - 1.f) * asp * tanDemiV;
						const float32 py = (1.f - ((float32)y + 0.5f) / (float32)H * 2.f) * tanDemiV;
						const float32 c2 = 1.f / (1.f + px * px + py * py); // cos^2
						const float32 loi = mut ? c2 : c2 * c2;
						const float32 k = 1.f - v + v * loi;
						for (int32 c = 0; c < 3; ++c)
							rgb[((usize)y * (usize)L + (usize)x) * 3u + (usize)c] *= k;
					}
			}

			// 5. L'ETALONNAGE : contraste (log, autour de 0,18), saturation, chaleur
			void Etalonnage(const NkFinition &f, float32 *rgb, usize n) {
				for (usize i = 0; i < n; ++i) {
					float32 *c = rgb + i * 3u;
					const float32 l = Lum3(c);
					float32 l2 = l;
					if (f.contraste != 1.f && l > 1e-8f)
						l2 = 0.18f * (float32)math::NkPow((float64)(l / 0.18f), (float64)f.contraste);
					const float32 k = l > 1e-8f ? l2 / l : 0.f;
					for (int32 a = 0; a < 3; ++a) {
						float32 v = l2 + (c[a] * k - l2) * f.saturation;
						if (a == 0)
							v *= 1.f + 0.1f * f.chaleur;
						if (a == 2)
							v *= 1.f - 0.1f * f.chaleur;
						c[a] = v > 0.f ? v : 0.f;
					}
				}
			}
		} // namespace

		void NkFinitionAppliquer(const NkFinition &f, float32 *rgb, int32 L, int32 H, float32 tanDemiV) {
			if (!rgb || L <= 0 || H <= 0 || !f.Actif())
				return;
			if (f.bloom > 0.f)
				Halo(f, rgb, L, H);
			if (f.eblouissement > 0.f)
				Etoiles(f, rgb, L, H);
			if (f.aberration > 0.f)
				Aberration(f, rgb, L, H);
			if (f.vignette > 0.f)
				Vignette(f, rgb, L, H, tanDemiV);
			if (f.contraste != 1.f || f.saturation != 1.f || f.chaleur != 0.f)
				Etalonnage(f, rgb, (usize)L * (usize)H);
		}

		int32 NkFinitionBanc() {
			int32 echecs = 0;
			auto juge = [&](bool ok, const char *quoi, float64 a, float64 b) {
				NkPrintfSortie("[finition] BANC %-58s %.5f / %.5f -> %s\n", quoi, a, b, ok ? "VERT" : "ROUGE");
				if (!ok)
					++echecs;
			};
			auto somme = [](const NkVector<float32> &v) {
				float64 s = 0.0;
				for (usize i = 0; i < v.Size(); ++i)
					s += (float64)v[i];
				return s;
			};
			const int32 L = 256, H = 256;
			NkVector<float32> img;
			auto source = [&]() {
				img.Resize((usize)L * (usize)H * 3u);
				for (usize i = 0; i < img.Size(); ++i)
					img[i] = 0.f;
				for (int32 y = 127; y <= 129; ++y)
					for (int32 x = 127; x <= 129; ++x)
						for (int32 c = 0; c < 3; ++c)
							img[((usize)y * (usize)L + (usize)x) * 3u + (usize)c] = 100.f;
			};
			auto lumAt = [&](int32 x, int32 y) {
				return (float64)Lum3(img.Data() + ((usize)y * (usize)L + (usize)x) * 3u);
			};
			// 0. neutre : l'identite au bit
			{
				source();
				NkVector<float32> avant = img;
				NkFinition f;
				NkFinitionAppliquer(f, img.Data(), L, H, 0.5f);
				bool pareil = true;
				for (usize i = 0; i < img.Size(); ++i)
					pareil = pareil && img[i] == avant[i];
				juge(pareil && !f.Actif(), "neutre : image inchangee au bit", 0.0, 0.0);
			}
			// 1. le halo : energie conservee, profil decroissant
			{
				source();
				const float64 s0 = somme(img);
				NkFinition f;
				f.bloom = 0.3f;
				f.bloomRayon = 0.02f;
				NkFinitionAppliquer(f, img.Data(), L, H, 0.5f);
				const float64 s1 = somme(img);
				juge(math::NkFabs((float32)((s1 - s0) / s0)) < 1e-3f, "halo : energie conservee (somme apres / avant)", s1, s0);
				const float64 a = lumAt(136, 128), b = lumAt(144, 128), c = lumAt(160, 128);
				juge(a > b && b > c && c > 0.0, "halo : profil decroissant a 8, 16, 32 px (8 puis 32)", a, c);
			}
			// 2. les etoiles a 6 branches : energie conservee, lumiere sur les axes (0 deg) et pas a 30 deg
			{
				source();
				const float64 s0 = somme(img);
				NkFinition f;
				f.eblouissement = 0.3f;
				f.branches = 6;
				NkFinitionAppliquer(f, img.Data(), L, H, 0.5f);
				const float64 s1 = somme(img);
				juge(math::NkFabs((float32)((s1 - s0) / s0)) < 1e-2f, "etoiles : energie conservee (somme apres / avant)", s1, s0);
				const float64 axe = lumAt(128 + 24, 128);
				const float64 hors = lumAt(128 + 21, 128 - 12); // 30 deg, meme distance
				juge(axe > 4.0 * hors && axe > 0.0, "etoiles : sur l'axe a 24 px contre 30 deg hors axe (x4)", axe, hors);
			}
			// 3. la vignette : cos^4 de l'angle du coin
			{
				img.Resize((usize)L * (usize)H * 3u);
				for (usize i = 0; i < img.Size(); ++i)
					img[i] = 1.f;
				NkFinition f;
				f.vignette = 1.f;
				const float32 t = 0.5f;
				NkFinitionAppliquer(f, img.Data(), L, H, t);
				// le coin : centre du pixel (0, 0) ; l'angle depuis la geometrie, independamment de la formule
				const float64 ux = (0.5 / L * 2.0 - 1.0) * t, uy = (1.0 - 0.5 / H * 2.0) * t;
				const float64 th = math::NkAtan((float32)math::NkSqrt((float32)(ux * ux + uy * uy)));
				const float64 cs = math::NkCos((float32)th);
				const float64 attendu = cs * cs * cs * cs;
				juge(math::NkFabs((float32)(img[0] - attendu)) < 1e-3f, "vignette : coin = cos^4 de son angle", img[0], attendu);
			}
			// 4. l'aberration : une ligne verticale a droite, rouge dehors, bleu dedans
			{
				const int32 L2 = 400, H2 = 200;
				img.Resize((usize)L2 * (usize)H2 * 3u);
				for (usize i = 0; i < img.Size(); ++i)
					img[i] = 0.f;
				for (int32 y = 0; y < H2; ++y)
					for (int32 c = 0; c < 3; ++c)
						img[((usize)y * (usize)L2 + 350u) * 3u + (usize)c] = 1.f;
				NkFinition f;
				f.aberration = 2.f;
				NkFinitionAppliquer(f, img.Data(), L2, H2, 0.5f);
				float64 cr = 0.0, sr = 0.0, cb = 0.0, sb = 0.0;
				for (int32 x = 330; x < 370; ++x) {
					const usize i = ((usize)100 * (usize)L2 + (usize)x) * 3u;
					cr += (float64)x * img[i];
					sr += img[i];
					cb += (float64)x * img[i + 2u];
					sb += img[i + 2u];
				}
				const float64 ecart = (sr > 0.0 && sb > 0.0) ? cr / sr - cb / sb : 0.0;
				const float64 rc = math::NkSqrt(200.f * 200.f + 100.f * 100.f);
				const float64 attendu = 2.0 * 2.0 * (350.5 - 200.0) / rc; // rouge + delta, bleu - delta
				juge(math::NkFabs((float32)(ecart - attendu)) < 0.25f, "aberration : rouge - bleu au rang central (px)", ecart, attendu);
			}
			// 5. l'etalonnage : le gris 0,18 ne bouge pas, 0,36 suit la puissance, saturation nulle = gris
			{
				img.Resize(9u);
				const float32 v[9] = {0.18f, 0.18f, 0.18f, 0.36f, 0.36f, 0.36f, 0.5f, 0.2f, 0.1f};
				for (usize i = 0; i < 9u; ++i)
					img[i] = v[i];
				NkFinition f;
				f.contraste = 1.5f;
				NkFinitionAppliquer(f, img.Data(), 3, 1, 0.5f);
				juge(math::NkFabs(img[0] - 0.18f) < 1e-5f, "etalonnage : le gris 0,18 fixe sous contraste 1,5", img[0], 0.18);
				const float64 att = 0.18 * math::NkPow(2.0, 1.5);
				juge(math::NkFabs((float32)(img[3] - att)) < 1e-4f, "etalonnage : 0,36 -> 0,18 x 2^1,5", img[3], att);
				for (usize i = 0; i < 9u; ++i)
					img[i] = v[i];
				NkFinition g;
				g.saturation = 0.f;
				NkFinitionAppliquer(g, img.Data(), 3, 1, 0.5f);
				juge(math::NkFabs(img[6] - img[7]) < 1e-6f && math::NkFabs(img[7] - img[8]) < 1e-6f, "etalonnage : saturation 0 -> R = G = B",
					 img[6], img[8]);
			}
			NkPrintfSortie("[finition] BANC DE LA FINITION : %d echec(s)%s -> %s\n", (int)echecs,
						   Mutation() ? " (MUTATION : halo ajoute, vignette cos^2, aberration inversee)" : "", echecs == 0 ? "VERT" : "ROUGE");
			return echecs;
		}

	} // namespace renderer
} // namespace nkentseu
