// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/Realiste/NkRenduRealisteBanc.cpp
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// (13/11/2026) LE BANC SANS FENETRE ET SANS NKCraft du rendu realiste du moteur : la preuve que l'API sert a une
// autre application (Rodolf, 13/11 : Noge, NKScena, NKAnima...). Une scene programmee -- un sol de 20 m, une boite
// d'un metre --, trois ambiances (jour, couchant, nuit) posees par NkRrPoserAmbianceTraceur, trois images au traceur
// sur le processeur. Aucune fenetre, aucune carte graphique.
// -----------------------------------------------------------------------------

#include "NKRenderer/Tools/Realiste/NkRenduRealisteMoteur.h"
#include "NKRenderer/Tools/PathTracer/NkTraceurChemins.h"
#include "NKImage/Core/NkImage.h"
#include "NKCore/Text/NkSnprintf.h"
#include "NKCore/Text/NkSortie.h"

namespace nkentseu {
	namespace renderer {

		namespace {
			// un quadrilatere (deux triangles) de normale n
			void Quad(NkScArchviz &s, const float32 *a, const float32 *b, const float32 *c, const float32 *d, const float32 *n,
					  uint32 mat) {
				const uint32 base = (uint32)(s.P.Size() / 3u);
				const float32 *p[4] = {a, b, c, d};
				for (int32 k = 0; k < 4; ++k) {
					for (int32 q = 0; q < 3; ++q) {
						s.P.PushBack(p[k][q]);
						s.N.PushBack(n[q]);
					}
					s.UV.PushBack(0.f);
					s.UV.PushBack(0.f);
				}
				const uint32 t[6] = {0, 1, 2, 0, 2, 3};
				for (int32 k = 0; k < 6; ++k)
					s.tri.PushBack(base + t[k]);
				s.triMat.PushBack(mat);
				s.triMat.PushBack(mat);
			}

			// la scene : un sol gris de 20 m et une boite claire d'un metre, un ciel en degrade
			void Scene(NkScArchviz &s) {
				NkScMateriau sol, boite;
				sol.couleur[0] = sol.couleur[1] = sol.couleur[2] = 0.5f;
				sol.rugosite = 0.9f;
				boite.couleur[0] = boite.couleur[1] = boite.couleur[2] = 0.75f;
				boite.rugosite = 0.8f;
				s.mats.PushBack(sol);
				s.mats.PushBack(boite);
				{
					const float32 a[3] = {-10.f, 0.f, -10.f}, b[3] = {-10.f, 0.f, 10.f}, c[3] = {10.f, 0.f, 10.f}, d[3] = {10.f, 0.f, -10.f};
					const float32 n[3] = {0.f, 1.f, 0.f};
					Quad(s, a, b, c, d, n, 0);
				}
				// la boite [-0,5 ; 0,5] x [0 ; 1] x [-0,5 ; 0,5] : six faces, normales sortantes
				const float32 x0 = -0.5f, x1 = 0.5f, y0 = 0.f, y1 = 1.f, z0 = -0.5f, z1 = 0.5f;
				const float32 v[8][3] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
										 {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
				const int32 f[6][4] = {{4, 5, 6, 7}, {1, 0, 3, 2}, {5, 1, 2, 6}, {0, 4, 7, 3}, {7, 6, 2, 3}, {0, 1, 5, 4}};
				const float32 nf[6][3] = {{0.f, 0.f, 1.f}, {0.f, 0.f, -1.f}, {1.f, 0.f, 0.f}, {-1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, -1.f, 0.f}};
				for (int32 k = 0; k < 6; ++k)
					Quad(s, v[f[k][0]], v[f[k][1]], v[f[k][2]], v[f[k][3]], nf[k], 1);
				// le soleil par defaut (l'ambiance le remplace) et un ciel en degrade
				s.soleilDir[0] = 0.4f;
				s.soleilDir[1] = 0.8f;
				s.soleilDir[2] = 0.45f;
				s.soleilE = 60.f;
				for (int32 q = 0; q < 3; ++q) {
					s.soleilCouleur[q] = 1.f;
					s.mn[q] = q == 1 ? 0.f : -10.f;
					s.mx[q] = q == 1 ? 1.f : 10.f;
				}
				const float32 zen[3] = {0.25f, 0.4f, 0.8f}, hor[3] = {0.7f, 0.78f, 0.9f};
				for (int32 q = 0; q < 3; ++q) {
					s.cielZenith[q] = zen[q];
					s.cielHorizon[q] = hor[q];
				}
				s.cielIntensite = 1.f;
			}

			// la moyenne r, g, b (0..1) d'une image PNG ; faux si elle ne se lit pas
			bool Moyenne(const char *png, float64 *m3) {
				NkImage im;
				if (!im.Load(png, 3) || !im.IsValid() || im.Channels() < 3)
					return false;
				const int32 W = im.Width(), H = im.Height(), C = im.Channels();
				float64 s[3] = {0.0, 0.0, 0.0};
				for (int32 y = 0; y < H; ++y) {
					const uint8 *p = im.RowPtr(y);
					for (int32 x = 0; x < W; ++x)
						for (int32 q = 0; q < 3; ++q)
							s[q] += (float64)p[(usize)x * (usize)C + (usize)q] / 255.0;
				}
				const float64 n = (float64)W * (float64)H;
				for (int32 q = 0; q < 3; ++q)
					m3[q] = n > 0.0 ? s[q] / n : 0.0;
				return n > 0.0;
			}
		} // namespace

		int32 NkRrBancSansFenetre(const char *dossier) {
			int32 echecs = 0;
			auto verdict = [&](bool ok, const char *quoi, float64 mesure, float64 attendu) {
				NkPrintfSortie("[rendu-moteur-banc] %-56s mesure %.4f attendu %.4f : %s\n", quoi, mesure, attendu, ok ? "VERT" : "ROUGE");
				if (!ok)
					++echecs;
			};
			const char *noms[3] = {"jour", "couchant", "nuit"};
			float64 moy[3][3] = {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}};
			bool lues[3] = {false, false, false};
			for (int32 k = 0; k < 3; ++k) {
				const int32 ia = NkAmbianceIndex(noms[k]);
				if (ia < 0) {
					verdict(false, "ambiance connue du moteur", 0.0, 1.0);
					continue;
				}
				NkScArchviz s;
				Scene(s);
				NkTcReglages r;
				r.spp = 16;
				r.debruiter = true;
				const NkRrEffetAmbiance fx = NkRrEffetDeLAmbiance(NkAmbianceDe(ia));
				NkRrPoserAmbianceTraceur(fx, s, r);
				r.spp = 16; // le minimum de la nuit (512) n'a pas d'objet ici : aucune lampe, l'image est noire
				NkTcStats st;
				if (!NkTcPreparer(s, nullptr, &st)) {
					verdict(false, "scene preparee par le traceur", 0.0, 1.0);
					continue;
				}
				NkTcCamera cam;
				// la face -x de la boite, celle que le soleil de l'apres-midi et du couchant (azimuts 250 et 285, l'ouest)
				// eclaire : la couleur du soleil s'y lit
				const float32 oeil[3] = {-3.5f, 0.8f, 1.2f}, cible[3] = {0.f, 0.45f, 0.f};
				for (int32 q = 0; q < 3; ++q) {
					cam.oeil[q] = oeil[q];
					cam.cible[q] = cible[q];
				}
				cam.focaleMM = 40.f;
				cam.L = 160;
				cam.H = 90;
				char png[512];
				NkSnprintf(png, sizeof(png), "%s/banc_moteur_%s.png", dossier ? dossier : ".", noms[k]);
				const bool ok = NkTcRendre(cam, r, png, &st) && Moyenne(png, moy[k]);
				lues[k] = ok;
				NkPrintfSortie("[rendu-moteur-banc] %s : %s ; moyenne r %.4f g %.4f b %.4f\n", noms[k], ok ? png : "IMAGE ABSENTE",
							   moy[k][0], moy[k][1], moy[k][2]);
			}
			auto lum = [](const float64 *m) {
				return 0.2126 * m[0] + 0.7152 * m[1] + 0.0722 * m[2];
			};
			verdict(lues[0] && lues[1] && lues[2], "trois images rendues et relues", (lues[0] && lues[1] && lues[2]) ? 1.0 : 0.0, 1.0);
			if (lues[0] && lues[1] && lues[2]) {
				const float64 lj = lum(moy[0]), ln = lum(moy[2]);
				const float64 rbJ = moy[0][0] / (moy[0][2] > 1e-6 ? moy[0][2] : 1e-6);
				const float64 rbC = moy[1][0] / (moy[1][2] > 1e-6 ? moy[1][2] : 1e-6);
				verdict(lj > 0.10, "jour : image eclairee (luminance moyenne)", lj, 0.10);
				verdict(rbC > rbJ * 1.15, "couchant plus chaud que le jour (R/B, rapport)", rbC / (rbJ > 1e-6 ? rbJ : 1e-6), 1.15);
				verdict(ln < lj * 0.25, "nuit : sans soleil ni ciel (luminance / jour)", ln / (lj > 1e-6 ? lj : 1e-6), 0.25);
			}
			NkPrintfSortie("[rendu-moteur-banc] BANC DU RENDU REALISTE DU MOTEUR (sans fenetre, sans NKCraft) : %s (%d echec(s))\n",
						   echecs == 0 ? "VERT" : "ROUGE", (int)echecs);
			return echecs;
		}

	} // namespace renderer
} // namespace nkentseu
