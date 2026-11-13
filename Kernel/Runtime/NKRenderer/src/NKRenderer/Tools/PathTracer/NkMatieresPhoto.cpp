// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/PathTracer/NkMatieresPhoto.cpp
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// Les cartes image CC0 cuites pour une matiere decrite : voir NkMatieresPhoto.h.

#include "NKRenderer/Tools/PathTracer/NkMatieresPhoto.h"
#include "NKImage/Core/NkImage.h"
#include "NKFileSystem/NkFile.h"
#include "NKCore/Text/NkSnprintf.h"
#include "NKCore/Text/NkSortie.h"
#include "NKMath/NkFunctions.h"
#include "NKPlatform/NkEnv.h"

namespace nkentseu {
	namespace renderer {

		namespace {
			// sRGB <-> lineaire : 256 entrees a la lecture, 4096 a l'ecriture (pas d'appel a pow par texel)
			struct Tables {
					float32 lin[256];
					uint8 srgb[4096];
					Tables() {
						for (int32 i = 0; i < 256; ++i) {
							const float64 c = (float64)i / 255.0;
							lin[i] = (float32)(c <= 0.04045 ? c / 12.92 : math::NkPow((c + 0.055) / 1.055, 2.4));
						}
						for (int32 i = 0; i < 4096; ++i) {
							const float64 l = (float64)i / 4095.0;
							const float64 s = l <= 0.0031308 ? l * 12.92 : 1.055 * math::NkPow(l, 1.0 / 2.4) - 0.055;
							const int32 v = (int32)(s * 255.0 + 0.5);
							srgb[i] = (uint8)(v < 0 ? 0 : (v > 255 ? 255 : v));
						}
					}
			};
			const Tables &Tab() {
				static Tables t;
				return t;
			}
			uint8 VersSrgb(float32 l) {
				int32 i = (int32)(l * 4095.f + 0.5f);
				i = i < 0 ? 0 : (i > 4095 ? 4095 : i);
				return Tab().srgb[i];
			}
			uint8 Octet(float32 v01) {
				const int32 v = (int32)(v01 * 255.f + 0.5f);
				return (uint8)(v < 0 ? 0 : (v > 255 ? 255 : v));
			}

			// « polyhaven/parquet_chene » -> « polyhaven_parquet_chene »
			void NomPlat(const char *nom, char *out, int32 cap) {
				int32 i = 0;
				for (; nom[i] && i < cap - 1; ++i)
					out[i] = (nom[i] == '/' || nom[i] == '\\' || nom[i] == ':') ? '_' : nom[i];
				out[i] = 0;
			}

			// une image source, en RGBA, ramenee a `taille` x `taille` (moyenne par cellule : sans crenelage)
			bool Charger(const char *racine, const char *nom, const char *fichier, int32 taille, NkImage &out) {
				char ch[700];
				NkSnprintf(ch, sizeof(ch), "%s/%s/%s", racine, nom, fichier);
				NkImage im;
				if (!im.Load(ch, 4) || im.Width() <= 0 || im.Height() <= 0 || im.Channels() != 4) {
					NkPrintfSortie("[photo] image ILLISIBLE : %s\n", ch);
					return false;
				}
				out = im.Resize(taille, taille, NkResizeFilter::NK_AREA);
				return out.Width() == taille && out.Height() == taille && out.Channels() == 4;
			}

			// les metres d'une repetition (echelle.txt : « 2.250 », la virgule aussi)
			float32 LireEchelle(const char *racine, const char *nom) {
				char ch[700];
				NkSnprintf(ch, sizeof(ch), "%s/%s/echelle.txt", racine, nom);
				if (!NkFile::Exists(ch))
					return 1.f;
				const NkString t = NkFile::ReadAllText(ch);
				const char *s = t.CStr();
				float64 v = 0.0, f = 0.0;
				bool vu = false;
				for (; s && *s; ++s) {
					if (*s >= '0' && *s <= '9') {
						vu = true;
						if (f > 0.0) {
							v += (float64)(*s - '0') * f;
							f *= 0.1;
						} else
							v = v * 10.0 + (float64)(*s - '0');
					} else if ((*s == '.' || *s == ',') && f == 0.0)
						f = 0.1;
					else if (vu)
						break;
				}
				return (vu && v > 0.01) ? (float32)v : 1.f;
			}

			// LA MEDIANE du canal `c` (octets) d'une image RGBA : un histogramme de 256 cases
			int32 Mediane(const NkImage &im, int32 c) {
				uint32 hist[256] = {0};
				const int32 w = im.Width(), h = im.Height();
				for (int32 y = 0; y < h; ++y) {
					const uint8 *l = im.RowPtr(y);
					for (int32 x = 0; x < w; ++x)
						++hist[l[x * 4 + c]];
				}
				const uint64 moitie = ((uint64)w * (uint64)h + 1u) / 2u;
				uint64 cumul = 0;
				for (int32 v = 0; v < 256; ++v) {
					cumul += hist[v];
					if (cumul >= moitie)
						return v;
				}
				return 255;
			}

			// le rapport a la mediane, a la puissance `contraste` (1 : la photo telle quelle ; 0,5 : sa variation
			// adoucie de moitie en log -- un mur fraichement peint est plus uni que la photo d'un enduit use)
			float32 Rapport(float32 v, float32 med, float32 contraste) {
				const float32 r = v / med;
				return contraste == 1.f ? r : (float32)math::NkPow((float64)(r > 1e-6f ? r : 1e-6f), (float64)contraste);
			}

			// LA COULEUR CUITE : couleur x (texel / mediane)^contraste (lineaire), bornee a 1 ; rend la mediane de la photo
			void CuireCouleur(const NkImage &src, const float32 *couleur, float32 contraste, NkImage &dst, float32 *med) {
				const int32 w = src.Width(), h = src.Height();
				uint8 table[3][256]; // un octet source -> un octet cuit, par canal (pas de puissance par texel)
				for (int32 a = 0; a < 3; ++a) {
					med[a] = Tab().lin[Mediane(src, a)];
					med[a] = med[a] > 1e-3f ? med[a] : 1e-3f;
					for (int32 i = 0; i < 256; ++i) {
						const float32 v = couleur[a] * Rapport(Tab().lin[i], med[a], contraste);
						table[a][i] = VersSrgb(v < 1.f ? v : 1.f);
					}
				}
				dst = NkImage::Create((uint32)w, (uint32)h, 4, 0u);
				for (int32 y = 0; y < h; ++y) {
					const uint8 *l = src.RowPtr(y);
					uint8 *o = dst.RowPtr(y);
					for (int32 x = 0; x < w; ++x) {
						for (int32 a = 0; a < 3; ++a)
							o[x * 4 + a] = table[a][l[x * 4 + a]];
						o[x * 4 + 3] = 255;
					}
				}
			}

			// L'ORM CUIT : (1, rugosite x (r / mediane de r)^contraste, 0) ; rend la mediane de r
			void CuireOrm(const NkImage &src, float32 rugosite, float32 contraste, NkImage &dst, float32 &med) {
				const int32 w = src.Width(), h = src.Height();
				med = (float32)Mediane(src, 0) / 255.f;
				med = med > 1e-3f ? med : 1e-3f;
				float32 table[256];
				for (int32 i = 0; i < 256; ++i)
					table[i] = rugosite * Rapport((float32)i / 255.f, med, contraste);
				dst = NkImage::Create((uint32)w, (uint32)h, 4, 0u);
				for (int32 y = 0; y < h; ++y) {
					const uint8 *l = src.RowPtr(y);
					uint8 *o = dst.RowPtr(y);
					for (int32 x = 0; x < w; ++x) {
						const float32 r = table[l[x * 4]];
						o[x * 4] = 255;
						o[x * 4 + 1] = Octet(r < 1.f ? r : 1.f);
						o[x * 4 + 2] = 0;
						o[x * 4 + 3] = 255;
					}
				}
			}

			// LA NORMALE CUITE : le vert inverse (OpenGL -> la bitangente du moteur, vers le bas de l'image),
			// renormalisee apres la reduction (la moyenne de vecteurs unitaires est plus courte)
			void CuireNormale(const NkImage &src, bool inverserVert, NkImage &dst) {
				const int32 w = src.Width(), h = src.Height();
				dst = NkImage::Create((uint32)w, (uint32)h, 4, 0u);
				for (int32 y = 0; y < h; ++y) {
					const uint8 *l = src.RowPtr(y);
					uint8 *o = dst.RowPtr(y);
					for (int32 x = 0; x < w; ++x) {
						float32 nx = (float32)l[x * 4] / 127.5f - 1.f;
						float32 ny = (float32)l[x * 4 + 1] / 127.5f - 1.f;
						float32 nz = (float32)l[x * 4 + 2] / 127.5f - 1.f;
						if (inverserVert)
							ny = -ny;
						nz = nz > 0.05f ? nz : 0.05f;
						const float32 n = math::NkSqrt(nx * nx + ny * ny + nz * nz);
						o[x * 4] = Octet((nx / n) * 0.5f + 0.5f);
						o[x * 4 + 1] = Octet((ny / n) * 0.5f + 0.5f);
						o[x * 4 + 2] = Octet((nz / n) * 0.5f + 0.5f);
						o[x * 4 + 3] = 255;
					}
				}
			}
		} // namespace

		bool NkPhotoPreparer(const char *racine, const char *nom, const float32 *couleur, float32 rugosite, float32 contraste,
							 const char *cache, int32 taille, NkPhotoJeu &out) {
			out = NkPhotoJeu();
			if (!racine || !racine[0] || !nom || !nom[0] || !cache || !cache[0] || !couleur)
				return false;
			taille = taille < 16 ? 16 : (taille > 4096 ? 4096 : taille);
			char plat[200];
			NomPlat(nom, plat, sizeof(plat));
			const int32 cr = (int32)Octet(couleur[0]), cg = (int32)Octet(couleur[1]), cb = (int32)Octet(couleur[2]);
			int32 rr = (int32)(rugosite * 1000.f + 0.5f);
			rr = rr < 0 ? 0 : (rr > 1000 ? 1000 : rr);
			// le contraste au centieme (1 : rien dans le nom, les cartes d'avant servent encore)
			int32 kk = (int32)(contraste * 100.f + 0.5f);
			kk = kk < 0 ? 0 : (kk > 300 ? 300 : kk);
			char suf[16] = {0};
			if (kk != 100)
				NkSnprintf(suf, sizeof(suf), "_k%03d", (int)kk);
			// v2 (13/11) : cuites a la MEDIANE (les v1, a la moyenne, restent dans le cache sans servir)
			NkSnprintf(out.couleur, sizeof(out.couleur), "%s/photo_%s_%d_c%02x%02x%02x%s_couleur_v2.png", cache, plat, (int)taille, cr, cg,
					   cb, suf);
			NkSnprintf(out.orm, sizeof(out.orm), "%s/photo_%s_%d_r%03d%s_orm_v2.png", cache, plat, (int)taille, (int)rr, suf);
			// NK_PHOTO_MUTATION=1 : la contre-epreuve des bancs de rendu (le vert n'est plus inverse ; une carte a part)
			const bool mutation = env::GetEnvVar("NK_PHOTO_MUTATION") != nullptr;
			NkSnprintf(out.normale, sizeof(out.normale), "%s/photo_%s_%d_normale%s_v1.png", cache, plat, (int)taille,
					   mutation ? "_mutation" : "");
			out.echelle = LireEchelle(racine, nom);
			// la couleur et la rugosite cuites a la valeur QUANTIFIEE du nom (deux matieres au meme nom : la meme carte)
			const float32 cq[3] = {(float32)cr / 255.f, (float32)cg / 255.f, (float32)cb / 255.f};
			const float32 rq = (float32)rr / 1000.f;
			const float32 kq = (float32)kk / 100.f;
			if (!NkFile::Exists(out.couleur)) {
				NkImage src, dst;
				if (!Charger(racine, nom, "couleur_2k.jpg", taille, src))
					return false;
				CuireCouleur(src, cq, kq, dst, out.medLin);
				if (!dst.SavePNG(out.couleur)) {
					NkPrintfSortie("[photo] ecriture IMPOSSIBLE : %s\n", out.couleur);
					return false;
				}
			}
			if (!NkFile::Exists(out.orm)) {
				NkImage src, dst;
				if (!Charger(racine, nom, "rugosite_2k.jpg", taille, src))
					return false;
				CuireOrm(src, rq, kq, dst, out.medRug);
				if (!dst.SavePNG(out.orm)) {
					NkPrintfSortie("[photo] ecriture IMPOSSIBLE : %s\n", out.orm);
					return false;
				}
			}
			if (!NkFile::Exists(out.normale)) {
				NkImage src, dst;
				if (!Charger(racine, nom, "normale_2k.jpg", taille, src))
					return false;
				CuireNormale(src, !mutation, dst);
				if (!dst.SavePNG(out.normale)) {
					NkPrintfSortie("[photo] ecriture IMPOSSIBLE : %s\n", out.normale);
					return false;
				}
			}
			out.ok = true;
			return true;
		}

		int32 NkPhotoBanc(const char *racine, const char *noms, const char *cache) {
			const bool mutation = env::GetEnvVar("NK_PHOTO_MUTATION") != nullptr;
			int32 echecs = 0, vus = 0, ydecides = 0;
			const char *s = noms ? noms : "";
			while (*s) {
				char nom[200];
				int32 n = 0;
				while (*s && *s != ',' && n < 199)
					nom[n++] = *s++;
				nom[n] = 0;
				if (*s == ',')
					++s;
				if (n == 0)
					continue;
				++vus;
				// 1. les moyennes des cartes cuites : la couleur et la rugosite DECRITES
				const float32 c[3] = {0.50f, 0.40f, 0.30f};
				const float32 rug = 0.5f;
				NkPhotoJeu j;
				if (!NkPhotoPreparer(racine, nom, c, rug, 1.f, cache, 1024, j)) {
					NkPrintfSortie("[photo] BANC %s : cuisson IMPOSSIBLE -> ROUGE\n", nom);
					++echecs;
					continue;
				}
				// (les medianes des cartes cuites ; a 8 bits, un pas sRGB vaut ~0,004 en lineaire vers 0,5)
				NkImage a, o;
				float32 ec = 1.f, er = 1.f;
				if (a.Load(j.couleur, 4) && o.Load(j.orm, 4) && a.Channels() == 4 && o.Channels() == 4) {
					ec = 0.f;
					for (int32 k = 0; k < 3; ++k) {
						const float32 d = Tab().lin[Mediane(a, k)] - c[k];
						ec = math::NkMax(ec, d < 0.f ? -d : d);
					}
					er = (float32)Mediane(o, 1) / 255.f - rug;
				}
				const bool moyOk = ec < 0.01f && (er < 0.f ? -er : er) < 0.01f;
				// 2. LE SENS DE LA NORMALE contre le deplacement : x = +u (vers la droite de l'image), y = +v
				// (vers le BAS : la ligne 0 est v = 0) ; une pente qui monte vers +u penche la normale vers -u
				NkImage hs, ns, nc;
				float64 sxy[2] = {0.0, 0.0}, snn[2] = {0.0, 0.0}, sgg[2] = {0.0, 0.0};
				const int32 t = 512;
				if (Charger(racine, nom, "deplacement_2k.jpg", t, hs) && Charger(racine, nom, "normale_2k.jpg", t, ns)) {
					CuireNormale(ns, !mutation, nc);
					for (int32 y = 1; y < t - 1; ++y) {
						const uint8 *h0 = hs.RowPtr(y - 1), *h1 = hs.RowPtr(y), *h2 = hs.RowPtr(y + 1);
						const uint8 *l = nc.RowPtr(y);
						for (int32 x = 1; x < t - 1; ++x) {
							const float64 gx = -((float64)h1[(x + 1) * 4] - (float64)h1[(x - 1) * 4]);
							const float64 gy = -((float64)h2[x * 4] - (float64)h0[x * 4]);
							const float64 nx = (float64)l[x * 4] / 127.5 - 1.0, ny = (float64)l[x * 4 + 1] / 127.5 - 1.0;
							sxy[0] += nx * gx;
							sxy[1] += ny * gy;
							snn[0] += nx * nx;
							snn[1] += ny * ny;
							sgg[0] += gx * gx;
							sgg[1] += gy * gy;
						}
					}
				}
				float64 cor[2];
				for (int32 k = 0; k < 2; ++k)
					cor[k] = (snn[k] > 0.0 && sgg[k] > 0.0) ? sxy[k] / math::NkSqrt(snn[k] * sgg[k]) : 0.0;
				// un axe dont le deplacement ne dit rien (|correlation| < 0,1) ne juge pas ; un axe qui juge doit etre positif
				const bool decX = cor[0] >= 0.1 || cor[0] <= -0.1, decY = cor[1] >= 0.1 || cor[1] <= -0.1;
				const bool sensOk = (!decX || cor[0] > 0.0) && (!decY || cor[1] > 0.0);
				if (decY && cor[1] > 0.0)
					++ydecides;
				const bool vert = moyOk && sensOk;
				if (!vert)
					++echecs;
				NkPrintfSortie("[photo] BANC %-30s echelle %.3f m ; mediane cuite - decrite : couleur %.4f, rugosite %.4f ; "
							   "sens de la normale (correlation avec la pente du deplacement) x %.3f%s y %.3f%s%s -> %s\n",
							   nom, (double)j.echelle, (double)ec, (double)er, cor[0], decX ? "" : " (indecidable)", cor[1],
							   decY ? "" : " (indecidable)", mutation ? " (MUTATION : vert non inverse)" : "", vert ? "VERT" : "ROUGE");
			}
			const bool assez = ydecides >= 10;
			NkPrintfSortie("[photo] BANC DES PHOTOS : %d photo(s), %d echec(s), %d au sens vertical decide et juste (10 exigees) -> %s\n",
						   (int)vus, (int)echecs, (int)ydecides, (echecs == 0 && assez) ? "VERT" : "ROUGE");
			return echecs + (assez ? 0 : 1);
		}

	} // namespace renderer
} // namespace nkentseu
