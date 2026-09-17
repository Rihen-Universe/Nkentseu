#pragma once
// -----------------------------------------------------------------------------
// @File    Applications/NK3DModeler/src/NK3DModeler/Viewport/NkCursorWrapSonde.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
//
// NkCursorWrapSonde.h — LA SONDE DE (b5). AUCUNE FENETRE, AUCUN GPU, AUCUNE
// INJECTION D'ENTREE. Elle rejoue des SUITES DE POSITIONS ecrites a l'avance
// (canal `modeleur-blender.questions.md`, R16) a travers `NkCursorWrapStep` —
// LA MEME fonction que le produit appelle, pas une copie.
//
//   NK3DModeler.exe --sonde-wrap [dossier]
//       -> ecrit `sonde_wrap.txt` et SORT. Code 0 si tout est vert.
//   NK_WRAP_NOFIX=1 NK3DModeler.exe --sonde-wrap [dossier]
//       -> LA MUTATION : la correction est retiree, le reste est intact.
//          La sonde DOIT alors rendre 1. Si elle reste verte, elle ne teste rien.
//
// ⚠ Le zero se prouve EN PREMIER (cas z) : une suite entierement interieure ne
//   doit demander AUCUN rebouclage. Sans ce cas, un compteur qui vaut 1 plus bas
//   ne prouverait pas qu'il compte ce qu'on croit.
// =============================================================================

#include "NK3DModeler/Viewport/NkCursorWrap.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace nk3d {

		struct NkWrapSondeRap {
			char txt[16384];
			int len = 0;
			int rouges = 0;
			void Dire(const char *nom, bool vert, const char *detail) {
				len += snprintf(txt + len, (size_t)(sizeof(txt) - (size_t)len), "%s  %-46s  %s\n",
								vert ? "VERT " : "ROUGE", nom, detail ? detail : "");
				if (!vert)
					++rouges;
			}
		};

		// Une image d'une suite : la position OBSERVEE (px vue) sur les deux axes,
		// et le delta corrige que l'attendu prescrit.
		struct NkWrapPas {
			float x, y;		// position observee
			float adx, ady; // delta corrige ATTENDU
		};

		inline bool NkWrapProche(float a, float b) {
			const float d = a - b;
			return (d < 0.f ? -d : d) < 1e-3f;
		}

		// Rejoue une suite. `depX/depY` est la position de depart (image 0, aucun
		// delta juge). Rend vrai si tous les deltas corriges tombent sur l'attendu
		// et si le nombre de rebouclages demandes vaut `warpsAttendus`.
		// `sommeX` recoit la somme des deltas corriges en X : c'est elle qui montre
		// le saut quand la correction est retiree.
		inline bool NkWrapRejoue(bool disabled, float depX, float depY, float W, float H,
								 const NkWrapPas *pas, int n, unsigned warpsAttendus, float *sommeX,
								 float *sommeY, float *premWarpX, float *premWarpY, char *trace,
								 int traceMax) {
			NkCursorWrapState st;
			st.disabled = disabled;
			float prevX = depX, prevY = depY;
			float sx = 0.f, sy = 0.f;
			float pwx = -1.f, pwy = -1.f;
			bool ok = true;
			int tl = 0;
			for (int i = 0; i < n; ++i) {
				const float rdx = pas[i].x - prevX;
				const float rdy = pas[i].y - prevY;
				NkCursorWrapOut out;
				NkCursorWrapStep(st, pas[i].x, pas[i].y, rdx, rdy, W, H, out);
				if (out.warp && pwx < 0.f) {
					pwx = out.warpX;
					pwy = out.warpY;
				}
				sx += out.dx;
				sy += out.dy;
				if (!disabled && (!NkWrapProche(out.dx, pas[i].adx) || !NkWrapProche(out.dy, pas[i].ady)))
					ok = false;
				if (trace && tl < traceMax - 96)
					tl += snprintf(trace + tl, (size_t)(traceMax - tl),
								   "      img %d : pos(%.0f,%.0f) brut(%+.0f,%+.0f) -> corrige(%+.0f,%+.0f)%s\n",
								   i + 1, (double)pas[i].x, (double)pas[i].y, (double)rdx, (double)rdy,
								   (double)out.dx, (double)out.dy, out.warp ? "  [WARP]" : "");
				prevX = pas[i].x;
				prevY = pas[i].y;
			}
			if (st.x.wraps + st.y.wraps != warpsAttendus)
				ok = false;
			if (sommeX)
				*sommeX = sx;
			if (sommeY)
				*sommeY = sy;
			if (premWarpX)
				*premWarpX = pwx;
			if (premWarpY)
				*premWarpY = pwy;
			return ok;
		}

		inline int NkCursorWrapSonde(const char *dir) {
			const bool nofix = (std::getenv("NK_WRAP_NOFIX") != nullptr);
			NkWrapSondeRap R;
			R.txt[0] = 0;
			char d[512];
			char tr[4096];

			const float W = 800.f, H = 600.f; // ⚠ DIFFERENTS : un critere qui compare
											  // deux axes egaux ne peut pas distinguer
											  // un axe branche sur l'etendue de l'autre.

			// ── (z) LE ZERO D'ABORD ────────────────────────────────────────────
			// Suite entierement interieure : AUCUN rebouclage. Si ce cas passe au
			// vert avec un warp, tous les comptages plus bas sont sans valeur.
			{
				const NkWrapPas p[] = {{410.f, 310.f, 10.f, 10.f}, {420.f, 320.f, 10.f, 10.f}};
				float sx = 0.f, sy = 0.f;
				const bool ok = NkWrapRejoue(nofix, 400.f, 300.f, W, H, p, 2, 0u, &sx, &sy, nullptr,
											 nullptr, tr, (int)sizeof(tr));
				snprintf(d, sizeof(d), "0 warp, somme X %+.0f (attendu +20), somme Y %+.0f (+20)",
						 (double)sx, (double)sy);
				R.Dire("(z) ZERO : suite interieure, aucun rebouclage",
					   ok && NkWrapProche(sx, 20.f) && NkWrapProche(sy, 20.f), d);
			}

			// ── (a) BORD DROIT, SANS LATENCE ───────────────────────────────────
			{
				const NkWrapPas p[] = {{805.f, 300.f, 10.f, 0.f}, {7.f, 300.f, 2.f, 0.f}};
				float sx = 0.f, sy = 0.f, wx = -1.f, wy = -1.f;
				tr[0] = 0;
				const bool ok = NkWrapRejoue(nofix, 795.f, 300.f, W, H, p, 2, 1u, &sx, &sy, &wx, &wy,
											 tr, (int)sizeof(tr));
				const bool vert = nofix ? false : (ok && NkWrapProche(sx, 12.f) && NkWrapProche(wx, 5.f));
				snprintf(d, sizeof(d), "somme X %+.0f (attendu +12 ; sans correction -788), warp vers x=%.0f (attendu 5)",
						 (double)sx, (double)wx);
				R.Dire("(a) bord droit : +10 puis -798 -> +2", vert, d);
				R.len += snprintf(R.txt + R.len, sizeof(R.txt) - (size_t)R.len, "%s", tr);
			}

			// ── (b) LATENCE : la position pompee reste perimee une image ───────
			{
				const NkWrapPas p[] = {
					{805.f, 300.f, 10.f, 0.f}, {805.f, 300.f, 0.f, 0.f}, {8.f, 300.f, 3.f, 0.f}};
				float sx = 0.f, sy = 0.f;
				tr[0] = 0;
				const bool ok = NkWrapRejoue(nofix, 795.f, 300.f, W, H, p, 3, 1u, &sx, &sy, nullptr,
											 nullptr, tr, (int)sizeof(tr));
				const bool vert = nofix ? false : (ok && NkWrapProche(sx, 13.f));
				snprintf(d, sizeof(d), "somme X %+.0f (attendu +13 ; sans correction -787)", (double)sx);
				R.Dire("(b) latence : le report est MAINTENU une image", vert, d);
				R.len += snprintf(R.txt + R.len, sizeof(R.txt) - (size_t)R.len, "%s", tr);
			}

			// ── (c) BORD GAUCHE ────────────────────────────────────────────────
			{
				const NkWrapPas p[] = {{-6.f, 300.f, -11.f, 0.f}, {792.f, 300.f, -2.f, 0.f}};
				float sx = 0.f, sy = 0.f, wx = -1.f, wy = -1.f;
				tr[0] = 0;
				const bool ok = NkWrapRejoue(nofix, 5.f, 300.f, W, H, p, 2, 1u, &sx, &sy, &wx, &wy, tr,
											 (int)sizeof(tr));
				const bool vert = nofix ? false : (ok && NkWrapProche(sx, -13.f) && NkWrapProche(wx, 794.f));
				snprintf(d, sizeof(d), "somme X %+.0f (attendu -13 ; sans correction +787), warp vers x=%.0f (attendu 794)",
						 (double)sx, (double)wx);
				R.Dire("(c) bord gauche : -11 puis +798 -> -2", vert, d);
				R.len += snprintf(R.txt + R.len, sizeof(R.txt) - (size_t)R.len, "%s", tr);
			}

			// ── (d) L'AXE Y A SA PROPRE ETENDUE ────────────────────────────────
			// H = 600, PAS 800. Un axe Y branche par megarde sur l'etendue X
			// poserait un report de -800 au lieu de -600 et rendrait +202 au lieu
			// de +2. Ce cas est le seul qui puisse le voir.
			{
				const NkWrapPas p[] = {{403.f, 605.f, 3.f, 10.f}, {406.f, 7.f, 3.f, 2.f}};
				float sx = 0.f, sy = 0.f, wx = -1.f, wy = -1.f;
				tr[0] = 0;
				const bool ok = NkWrapRejoue(nofix, 400.f, 595.f, W, H, p, 2, 1u, &sx, &sy, &wx, &wy,
											 tr, (int)sizeof(tr));
				const bool vert =
					nofix ? false : (ok && NkWrapProche(sy, 12.f) && NkWrapProche(sx, 6.f) && NkWrapProche(wy, 5.f));
				snprintf(d, sizeof(d),
						 "somme Y %+.0f (attendu +12 ; H=600 et non 800 -> +202 si les axes sont confondus), somme X %+.0f (+6)",
						 (double)sy, (double)sx);
				R.Dire("(d) Y reboucle sur H, X ne reboucle pas", vert, d);
				R.len += snprintf(R.txt + R.len, sizeof(R.txt) - (size_t)R.len, "%s", tr);
			}

			// ── (e) CONDITION DE RETRAIT ───────────────────────────────────────
			// Le replacement n'arrive JAMAIS. Au bout de 30 images le report est
			// abandonne, et un nouveau rebouclage redevient possible. Sans cette
			// borne, l'attente survivrait indefiniment.
			// ⚠ Ce cas ne depend pas de la correction : il reste vert sous la
			//    mutation, et c'est normal — il protege l'attente, pas le calcul.
			{
				NkCursorWrapState st;
				st.disabled = nofix;
				NkCursorWrapOut out;
				// image 1 : sortie par la droite, report pose
				NkCursorWrapStep(st, 805.f, 300.f, 10.f, 0.f, W, H, out);
				const bool pose = st.x.pending && out.warp;
				// 31 images sans rien voir arriver (deltas nuls)
				for (int i = 0; i < 31; ++i)
					NkCursorWrapStep(st, 805.f, 300.f, 0.f, 0.f, W, H, out);
				const bool lache = !st.x.pending;
				// un nouveau rebouclage doit redevenir possible
				NkCursorWrapStep(st, 806.f, 300.f, 1.f, 0.f, W, H, out);
				const bool reprend = out.warp && st.x.wraps == 2u;
				snprintf(d, sizeof(d), "pose=%d lache apres 30 images=%d reprend=%d", (int)pose,
						 (int)lache, (int)reprend);
				R.Dire("(e) report jamais consomme : abandonne, puis reprend",
					   pose && lache && reprend, d);
			}

			// ── (f) VUE DEGENEREE ──────────────────────────────────────────────
			// Panneau replie ou vue pas encore dimensionnee : aucun bord utile,
			// donc aucun rebouclage et aucun delta touche.
			{
				NkCursorWrapState st;
				st.disabled = nofix;
				NkCursorWrapOut out;
				NkCursorWrapStep(st, 805.f, 300.f, 10.f, 4.f, 0.f, 0.f, out);
				const bool ok = !out.warp && NkWrapProche(out.dx, 10.f) && NkWrapProche(out.dy, 4.f) &&
								st.x.wraps == 0u;
				snprintf(d, sizeof(d), "W=H=0 -> warp=%d dx=%+.0f dy=%+.0f", (int)out.warp,
						 (double)out.dx, (double)out.dy);
				R.Dire("(f) vue degeneree : aucun rebouclage, deltas intacts", ok, d);
			}

			// ── (g) LA SORTIE DE MODALE OUBLIE TOUT ────────────────────────────
			// Un report garde d'une operation a la suivante s'appliquerait au
			// premier grand geste de celle-ci.
			{
				NkCursorWrapState st;
				st.disabled = nofix;
				NkCursorWrapOut out;
				NkCursorWrapStep(st, 805.f, 300.f, 10.f, 0.f, W, H, out);
				const bool avant = st.x.pending;
				NkCursorWrapReset(st);
				const bool apres = st.x.pending;
				NkCursorWrapStep(st, 7.f, 300.f, -798.f, 0.f, W, H, out);
				const bool intact = NkWrapProche(out.dx, -798.f);
				snprintf(d, sizeof(d), "attente avant=%d apres reset=%d, delta suivant %+.0f (intact)",
						 (int)avant, (int)apres, (double)out.dx);
				R.Dire("(g) reset de fin de modale : l'attente ne survit pas",
					   avant && !apres && intact, d);
			}

			char tete[900];
			snprintf(tete, sizeof(tete),
					 "SONDE (b5) — REBOUCLAGE DU CURSEUR, NkCursorWrap.h\n"
					 "Regime : ARITHMETIQUE PURE. Aucune fenetre, aucun GPU, aucune injection\n"
					 "d'entree : la sonde rejoue des SUITES DE POSITIONS ecrites a l'avance\n"
					 "(canal modeleur-blender, R16) a travers LA MEME fonction que le produit.\n"
					 "Vue d'epreuve : W=800  H=600 (differents a dessein).\n"
					 "MUTATION NK_WRAP_NOFIX : %s\n"
					 "-------------------------------------------------------------------------\n",
					 nofix ? "ACTIVE — la correction est retiree, les cas a..d DOIVENT rougir"
						   : "inactive");
			char pied[256];
			const bool attenduNofix = nofix; // sous mutation, on EXIGE des rouges
			snprintf(pied, sizeof(pied),
					 "-------------------------------------------------------------------------\n"
					 "%s (%d rouge(s))\n",
					 R.rouges == 0 ? "TOUT VERT" : "ECHEC", R.rouges);

			char chemin[1024];
			snprintf(chemin, sizeof(chemin), "%s/sonde_wrap.txt", (dir && dir[0]) ? dir : ".");
			if (FILE *f = std::fopen(chemin, "wb")) {
				std::fputs(tete, f);
				std::fputs(R.txt, f);
				std::fputs(pied, f);
				std::fclose(f);
			}
			std::printf("%s%s%s", tete, R.txt, pied);
			std::fflush(stdout);
			// Sous mutation, ZERO rouge est un ECHEC DE LA SONDE : la mutation a
			// survecu, donc les criteres ne testent rien.
			if (attenduNofix)
				return (R.rouges > 0) ? 1 : 2;
			return (R.rouges == 0) ? 0 : 1;
		}

	} // namespace nk3d
} // namespace nkentseu
