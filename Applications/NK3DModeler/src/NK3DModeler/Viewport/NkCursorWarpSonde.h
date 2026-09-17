#pragma once
// -----------------------------------------------------------------------------
// @File    Applications/NK3DModeler/src/NK3DModeler/Viewport/NkCursorWarpSonde.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
//
// NkCursorWarpSonde.h — SONDE DE `NkWindow::SetMousePositionClient` (b5, etape 3).
//
//   NK3DModeler.exe --sonde-warp     -> ecrit sonde_warp.txt et SORT. Code 0 = vert.
//
// ══ CE QUE CETTE SONDE NE FERA JAMAIS, ET C'EST LA CONTRAINTE QUI LA DESSINE ══
//   ELLE NE DEPLACE PAS LE CURSEUR. Deplacer le curseur sur la machine de Rodolf
//   EST une injection de souris, et c'est interdit sans exception. Elle n'appelle
//   donc `SetMousePositionClient` qu'avec des cibles qui doivent etre REFUSEES,
//   et elle verifie par un instrument independant (`GetCursorPos`) que rien n'a
//   bouge. Le chemin d'ACCEPTATION n'est pas mesure ici : le mesurer serait le
//   geste interdit. Dit franchement, plutot que maquille.
//
// ══ CE QU'ELLE PROUVE, ET POURQUOI C'EST LE POINT DUR ══════════════════════
//   La question n'est pas « `SetCursorPos` marche-t-il » -- c'est l'API de
//   Windows. La question est : **la conversion client -> ecran est-elle la
//   bonne** ? La derivation naive, celle qu'on ecrit quand on est presse, est
//   « coin de la fenetre (`GetWindowRect`) + position client ». Elle est JUSTE
//   sur une fenetre SANS CADRE -- et NK3DModeler est sans cadre, donc elle
//   aurait passe tous les essais faits sur lui -- et FAUSSE des qu'un cadre
//   existe, de la hauteur de la barre de titre.
//
//   La sonde ouvre donc une fenetre AVEC CADRE, marquee *** SONDE DE MESURE ***,
//   et montre que les deux derivations DIFFERENT. C'est le seul montage qui
//   distingue ce qu'il pretend distinguer : sur une fenetre sans cadre, les deux
//   sont egales et le critere ne pourrait pas echouer.
//
// ══ LE ZERO D'ABORD ════════════════════════════════════════════════════════
//   Avant de demander un refus, la sonde releve la position du curseur DEUX fois
//   sans rien faire, et exige qu'elle soit stable. Sans ce releve, « le curseur
//   n'a pas bouge » ne prouverait rien : il pourrait bouger tout seul (l'usager
//   a la main dessus) et le critere serait un tirage au sort.
// =============================================================================

#include "NKWindow/Core/NkWindow.h"
#include <cstdio>
#include <cstdlib>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace nkentseu {
	namespace nk3d {

		inline int NkCursorWarpSonde(const char *dir) {
			char rap[8192];
			int len = 0;
			int rouges = 0;
			auto dire = [&](const char *nom, bool vert, const char *detail) {
				len += snprintf(rap + len, sizeof(rap) - (size_t)len, "%s  %-44s  %s\n",
								vert ? "VERT " : "ROUGE", nom, detail ? detail : "");
				if (!vert)
					++rouges;
			};
			char d[512];

#if !defined(_WIN32)
			dire("(plateforme)", false,
				 "cette sonde n'a ete ecrite que pour Win32 -- ailleurs, le refus nomme de "
				 "SetMousePositionClient est la seule chose a verifier");
#else
			// ── LA FENETRE D'EPREUVE : AVEC CADRE, ET MARQUEE ─────────────────
			// AVEC CADRE a dessein : c'est la seule forme ou la derivation naive
			// et la vraie conversion different.
			NkWindowConfig wc;
			wc.title = "*** SONDE DE MESURE - CETTE FENETRE N'EST PAS LE PRODUIT *** conversion curseur";
			wc.width = 420;
			wc.height = 260;
			wc.centered = true;
			wc.resizable = false;
			// MUTATION DU MONTAGE : NK_WARP_SANS_CADRE=1 retire le cadre. Le critere
			// (1) DOIT alors rougir, puisque les deux derivations du coin client
			// deviennent egales. C'est la seule facon de montrer que (1) sait
			// echouer -- un critere qui ne peut pas rougir ne teste rien.
			// ⚠ LE CRITERE (3), LUI, N'EST PAS MUTABLE ET JE LE DIS : sa mutation
			// consisterait a retirer la borne, donc a ACCEPTER la cible, donc a
			// DEPLACER le curseur. C'est l'injection interdite. Il est protege
			// autrement : par (4), qui relit la position avec un instrument
			// independant du booleen rendu.
			const bool sansCadre = (std::getenv("NK_WARP_SANS_CADRE") != nullptr);
			wc.frame = !sansCadre;
			NkWindow f;
			if (!f.Create(wc)) {
				dire("(fenetre d'epreuve)", false, "impossible d'ouvrir la fenetre de sonde");
			} else {
				HWND h = nullptr;
				// On retrouve le HWND par le titre : la sonde n'a pas acces aux
				// donnees privees de la fenetre, et c'est tres bien ainsi -- elle
				// mesure ce qu'un tiers peut voir.
				h = ::FindWindowA(nullptr, wc.title.CStr());

				// ── (z) LE ZERO : la position du curseur est-elle STABLE ? ─────
				POINT c0{}, c1{};
				const bool lu0 = (::GetCursorPos(&c0) != 0);
				const bool lu1 = (::GetCursorPos(&c1) != 0);
				const bool stable = lu0 && lu1 && c0.x == c1.x && c0.y == c1.y;
				snprintf(d, sizeof(d), "deux releves : (%d, %d) et (%d, %d)", (int)c0.x, (int)c0.y,
						 (int)c1.x, (int)c1.y);
				dire("(z) ZERO : le curseur est immobile avant l'essai", stable, d);

				if (!h) {
					dire("(fenetre d'epreuve)", false, "HWND introuvable par le titre");
				} else {
					RECT wr{}, cr{};
					::GetWindowRect(h, &wr);
					::GetClientRect(h, &cr);
					POINT org{0, 0};
					::ClientToScreen(h, &org);

					// ── (1) LES DEUX DERIVATIONS DIFFERENT ────────────────────
					// Si elles etaient egales, ce critere ne distinguerait rien.
					const int dx = (int)(org.x - wr.left), dy = (int)(org.y - wr.top);
					snprintf(d, sizeof(d),
							 "cadre (%d, %d) · client->ecran (%d, %d) · ecart (%d, %d) ; la "
							 "derivation naive se tromperait de cet ecart",
							 (int)wr.left, (int)wr.top, (int)org.x, (int)org.y, dx, dy);
					dire(sansCadre ? "(1) MUTATION sans cadre : ce critere DOIT rougir"
						          : "(1) le coin du CADRE n'est pas celui du CLIENT",
					     (dx != 0 || dy != 0), d);

					// ── (2) LA TAILLE CLIENT EST PLUS PETITE QUE LA FENETRE ───
					const int lw = (int)(wr.right - wr.left), lh = (int)(wr.bottom - wr.top);
					const int cw = (int)(cr.right - cr.left), ch = (int)(cr.bottom - cr.top);
					snprintf(d, sizeof(d), "fenetre %dx%d · client %dx%d", lw, lh, cw, ch);
					dire("(2) la zone client est strictement plus petite", (cw < lw || ch < lh), d);

					// ── (3) REFUS NOMME : cible HORS de la zone client ────────
					// ⚠ ON NE DEMANDE QUE DES CIBLES REFUSABLES. Une cible valide
					//   deplacerait le curseur de Rodolf : c'est l'injection
					//   interdite, et aucun resultat ne la justifierait.
					const bool r1 = f.SetMousePositionClient(-1000, -1000);
					const bool r2 = f.SetMousePositionClient(cw + 10000, ch + 10000);
					snprintf(d, sizeof(d), "(-1000, -1000) -> %s · (%d, %d) -> %s", r1 ? "VRAI" : "faux",
							 cw + 10000, ch + 10000, r2 ? "VRAI" : "faux");
					dire("(3) une cible hors zone client est REFUSEE", (!r1 && !r2), d);

					// ── (4) ET LE CURSEUR N'A PAS BOUGE ───────────────────────
					// Deux instruments : le booleen rendu par la methode, et une
					// lecture INDEPENDANTE de la position. Un refus qui rendrait
					// faux tout en ayant bouge le curseur passerait le (3).
					POINT c2{};
					const bool lu2 = (::GetCursorPos(&c2) != 0);
					const bool immobile = lu2 && c2.x == c1.x && c2.y == c1.y;
					snprintf(d, sizeof(d), "avant (%d, %d) · apres (%d, %d)", (int)c1.x, (int)c1.y,
							 (int)c2.x, (int)c2.y);
					dire("(4) apres les refus, le curseur est au meme pixel", immobile, d);
				}
				f.Close();
			}
#endif

			char tete[1100];
			snprintf(tete, sizeof(tete),
					 "SONDE (b5 etape 3) — NkWindow::SetMousePositionClient\n"
					 "⚠ CETTE SONDE NE DEPLACE PAS LE CURSEUR : le deplacer serait une injection\n"
					 "  de souris. Elle ne demande que des cibles qui doivent etre REFUSEES, et\n"
					 "  verifie par une lecture independante que rien n'a bouge. Le chemin\n"
					 "  d'ACCEPTATION n'est donc PAS mesure ici -- le mesurer serait le geste\n"
					 "  interdit. C'est une limite, pas un oubli.\n"
					 "La fenetre d'epreuve a un CADRE a dessein : sans cadre, les deux\n"
					 "derivations du coin client sont EGALES et le critere (1) ne pourrait pas\n"
					 "echouer.\n"
					 "----------------------------------------------------------------------------\n");
			char pied[160];
			snprintf(pied, sizeof(pied),
					 "----------------------------------------------------------------------------\n"
					 "%s (%d rouge(s))\n",
					 rouges == 0 ? "TOUT VERT" : "ECHEC", rouges);
			char chemin[1024];
			snprintf(chemin, sizeof(chemin), "%s/sonde_warp.txt", (dir && dir[0]) ? dir : ".");
			if (FILE *out = std::fopen(chemin, "wb")) {
				std::fputs(tete, out);
				std::fputs(rap, out);
				std::fputs(pied, out);
				std::fclose(out);
			}
			std::printf("%s%s%s", tete, rap, pied);
			std::fflush(stdout);
			// Sous mutation, ZERO rouge est un ECHEC DE LA SONDE : le montage n'a pas
			// change ce qu'il devait changer, donc le critere ne teste rien.
			if (std::getenv("NK_WARP_SANS_CADRE"))
				return rouges > 0 ? 1 : 2;
			return rouges == 0 ? 0 : 1;
		}

	} // namespace nk3d
} // namespace nkentseu
