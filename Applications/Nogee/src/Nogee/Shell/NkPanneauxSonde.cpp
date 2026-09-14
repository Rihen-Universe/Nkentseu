// -----------------------------------------------------------------------------
// @File    NkPanneauxSonde.cpp
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// Mesure de la disposition — voir l'en-tete pour ce qu'elle mesure et ce
// qu'elle refuse de faire (elle ne compare a rien, elle ne pilote rien).
// -----------------------------------------------------------------------------
#include "Nogee/Shell/NkPanneauxSonde.h"
// ── LA CAPTURE DE SA PROPRE FENETRE (canal onglets, 2026-09-14) ─────────────
// Nogee rend par NKRHI ; `NkIEditorRenderer::CaptureNext` n'est implemente que
// par le dorsal NKCanvas/DX11 et rend faux ici. La seule facon de rendre compte
// de ce que Nogee AFFICHE est donc de demander a la fenetre de se dessiner.
#include "NKImage/NkImage.h"
#if defined(NKENTSEU_PLATFORM_WINDOWS)
#include <windows.h>
#endif
#include "NKEditorKit/NkEditorShell.h"

namespace nkentseu {
	namespace noge {

		namespace {
			/// Photographie la fenetre `hwnd` dans un PNG. Rend faux et n'ecrit
			/// RIEN si l'OS refuse -- cf. l'en-tete : pas d'image plutot qu'une
			/// image de trop.
			bool NkPhotographierSaFenetre(void *hwndOpaque, const char *chemin) {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
				HWND hwnd = (HWND)hwndOpaque;
				if (!hwnd)
					return false;
				RECT rc{};
				if (!GetWindowRect(hwnd, &rc))
					return false;
				const int w = rc.right - rc.left, h = rc.bottom - rc.top;
				if (w <= 0 || h <= 0)
					return false;
				HDC hdcWin = GetWindowDC(hwnd);
				HDC hdcMem = CreateCompatibleDC(hdcWin);
				BITMAPINFO bi{};
				bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
				bi.bmiHeader.biWidth = w;
				bi.bmiHeader.biHeight = -h; // negatif = origine en HAUT
				bi.bmiHeader.biPlanes = 1;
				bi.bmiHeader.biBitCount = 32;
				bi.bmiHeader.biCompression = BI_RGB;
				void *bits = nullptr;
				HBITMAP hbmp = CreateDIBSection(hdcMem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
				bool ok = false;
				if (hbmp) {
					HGDIOBJ vieux = SelectObject(hdcMem, hbmp);
					// ⚠️ PAS DE REPLI `BitBlt` : lui lirait l'ECRAN.
					ok = PrintWindow(hwnd, hdcMem, 2 /*PW_RENDERFULLCONTENT*/) != 0;
					if (ok && bits) {
						NkImage img;
						ok = img.Create((uint32)w, (uint32)h, math::NkColor(0, 0, 0, 255), 4);
						if (ok) {
							const uint8 *src = (const uint8 *)bits;
							uint8 *dst = img.Pixels();
							for (int i = 0; i < w * h; ++i) { // BGRA -> RGBA
								dst[i * 4 + 0] = src[i * 4 + 2];
								dst[i * 4 + 1] = src[i * 4 + 1];
								dst[i * 4 + 2] = src[i * 4 + 0];
								dst[i * 4 + 3] = 255;
							}
							ok = img.Save(chemin);
						}
					}
					SelectObject(hdcMem, vieux);
					DeleteObject(hbmp);
				}
				DeleteDC(hdcMem);
				ReleaseDC(hwnd, hdcWin);
				return ok;
#else
				(void)hwndOpaque;
				(void)chemin;
				return false;
#endif
			}
		} // namespace

		using namespace nkentseu::editorkit;

		void NkPanneauxSondeMesurer(NkEditorFrameContext &ec, NkEditorShell *shell,
									const char *const *titres, int32 nTitres) noexcept {
			NkPanneauxSondeEtat &s = NkPanneauxSonde();
			if (!s.active || !shell)
				return;
			// En pose, on a deja rendu son verdict mais la fenetre vit encore :
			// on compte les images jusqu'au garde-fou, puis on ferme.
			if (s.reported) {
				if (s.pose && ++s.frame >= s.frameFinPose) {
					std::printf("[PANNEAUX] fin de pose : fenetre FERMEE par la sonde\n");
					std::fflush(stdout);
					shell->RequestClose();
				}
				return;
			}
			nkgui::NkGuiContext &ui = ec.Ui();
			++s.frame;
			// Phase 0 : la mesure de reference. Phase 1 : la meme, apres que la
			// fenetre a change de taille — on laisse passer des images pour que le
			// dock ait recalcule, sinon on mesurerait la geometrie d'avant.
			const int32 cible = (s.phase == 0) ? s.frameMesure : s.frameApres;
			if (s.frame < cible)
				return;

			const float32 W = static_cast<float32>(ui.viewW);
			const float32 H = static_cast<float32>(ui.viewH);
			if (W < 2.f || H < 2.f) {
				// Une fenetre de taille nulle (reduite) rendrait des fractions
				// infinies. On attend plutot que de publier un nombre faux.
				return;
			}

			// ── LES TROIS BANDES, DERIVEES DU CORPS ─────────────────────────
			// `dockSpaceRect` est le rectangle que le shell a reellement donne au
			// dock cette frame. Tout ce qui n'est pas dedans est du chrome : c'est
			// la seule facon de mesurer une bande dont la hauteur est un calcul
			// interne au shell, et non un nombre que l'application a demande.
			const nkgui::NkRect corps = ui.dockSpaceRect;
			const float32 titleH = ui.titleBarH;
			// ⚠️ DEUX BANDES VIVENT DESORMAIS ENTRE LE TITRE ET LE CORPS, et les
			//    confondre serait une mesure qui ment. `corps.y - titleH` rendait
			//    « outils » tant que la barre d'outils etait seule ; depuis que la
			//    bande d'onglets partagee existe (canal onglets, o1), la meme
			//    soustraction rend 28 + 34 = 62 et l'imprimerait sous le nom
			//    « outils ». La hauteur des onglets est donc LUE a sa source -- la
			//    metrique `band_h` du composant partage -- et retranchee.
			//    ⚠️ ET ELLE EST LUE, PAS RECOPIEE : le jour ou Rodolf change
			//       `band_h`, cette sonde suit sans qu'on y touche.
			const float32 tabsH =
				shell->TabStripResult().tabs.Empty() && !shell->TabStripResult().usedW
					? 0.f
					: ui.S(editorkit::NkTabStripDecl().Metric("band_h"));
			const float32 toolbarH = corps.y - titleH - tabsH;
			const float32 footerH = H - (corps.y + corps.h);
			const float32 railG = corps.x;
			const float32 railD = W - (corps.x + corps.w);

			std::printf("[PANNEAUX] --- MESURE frame=%d phase=%d (%s) ---\n", s.frame, s.phase,
						s.phase == 0 ? "taille d'origine" : "APRES redimensionnement");
			std::printf("[PANNEAUX] fenetre W=%.2f H=%.2f  echelle=%.4f  hauteur_ligne=%.2f\n",
						static_cast<double>(W), static_cast<double>(H),
						static_cast<double>(ui.scale), static_cast<double>(ui.ItemHeight()));
			std::printf("[PANNEAUX] corps x=%.2f y=%.2f w=%.2f h=%.2f\n",
						static_cast<double>(corps.x), static_cast<double>(corps.y),
						static_cast<double>(corps.w), static_cast<double>(corps.h));
			std::printf("[PANNEAUX] bande titre=%.2f onglets=%.2f outils=%.2f etat=%.2f  rail_g=%.2f rail_d=%.2f\n",
						static_cast<double>(titleH), static_cast<double>(tabsH),
						static_cast<double>(toolbarH),
						static_cast<double>(footerH), static_cast<double>(railG),
						static_cast<double>(railD));

			// ── LA PHOTO, AU MEME INSTANT QUE LES NOMBRES ───────────────────
			// `NOGEE_SONDE_PNG=<chemin>` seulement : sans la variable, rien ne
			// change. Nogee rend par NKRHI, donc `CaptureNext` du kit rend faux
			// ici -- c'est la fenetre elle-meme qui se dessine (`PrintWindow`,
			// jamais l'ecran).
			if (const char *png = std::getenv("NOGEE_SONDE_PNG")) {
				if (*png && s.phase == 0) {
					const NkSurfaceDesc sd = shell->Window().GetSurfaceDesc();
					const bool ok = NkPhotographierSaFenetre((void *)sd.hwnd, png);
					// ⚠️ ON N'AFFIRME PAS QUE LA PHOTO EXISTE : on journalise le
					//    retour, et l'appelant verifiera le FICHIER.
					std::printf("[PANNEAUX] capture \"%s\" -> %s\n",
								png, ok ? "ECRITE" : "REFUSEE par l'OS");
			}
			}

			// ── LES COMPTEURS, RELEVES APRES QUE LES BARRES ONT DESSINE ─────
			const NkPanneauxCompteurs &c = NkPanneauxCpt();
			std::printf("[PANNEAUX] menus=%d entrees=%d dessinees=%d largeur_menus=%.2f\n", c.menus,
						c.entrees, c.entreesDessinees, static_cast<double>(c.largeurMenus));
			std::printf("[PANNEAUX] outils=%d largeur_outils=%.2f\n", c.outils,
						static_cast<double>(c.largeurOutils));

			// ── LE RECTANGLE DE CHAQUE PANNEAU ──────────────────────────────
			// Un panneau qui n'est ancre nulle part rend node=-1 : on l'imprime tel
			// quel plutot que de le taire. Un panneau absent du tableau est une
			// information, pas un trou a combler par un zero.
			for (int32 i = 0; i < nTitres; ++i) {
				const char *t = titres[i];
				const int32 node = shell->PanelDockNode(t);
				if (node < 0 || node >= static_cast<int32>(ui.dockNodes.Size())) {
					std::printf("[PANNEAUX] panneau \"%s\" node=%d NON ANCRE\n", t, node);
					continue;
				}
				const nkgui::NkRect &r = ui.dockNodes[node].rect;
				std::printf("[PANNEAUX] panneau \"%s\" node=%d px=(%.2f,%.2f,%.2f,%.2f) "
							"frac=(%.4f,%.4f,%.4f,%.4f) onglets=%d\n",
							t, node, static_cast<double>(r.x), static_cast<double>(r.y),
							static_cast<double>(r.w), static_cast<double>(r.h),
							static_cast<double>(r.x / W), static_cast<double>(r.y / H),
							static_cast<double>(r.w / W), static_cast<double>(r.h / H),
							ui.dockNodes[node].winCount);
			}

			std::printf("[PANNEAUX] --- FIN DE MESURE phase=%d ---\n", s.phase);
			std::fflush(stdout);

			// ── LE NEGATIF : on change la taille, et on remesure ─────────────
			if (s.redim && s.phase == 0) {
				const uint32 nW = static_cast<uint32>(W * kRedimW);
				const uint32 nH = static_cast<uint32>(H * kRedimH);
				std::printf("[PANNEAUX] REDIMENSIONNEMENT : %ux%u -> %ux%u (via Resize, "
							"aucune injection d'entree)\n",
							static_cast<unsigned>(W), static_cast<unsigned>(H),
							static_cast<unsigned>(nW), static_cast<unsigned>(nH));
				std::fflush(stdout);
				shell->Resize(nW, nH);
				s.phase = 1;
				s.frameApres = s.frame + 30; // laisser le dock recalculer
				return;
			}

			s.reported = true;
			if (s.pose) {
				// ~4000 images : quelques secondes, assez pour une photo, trop peu
				// pour qu'une fenetre oubliee traine.
				s.frameFinPose = s.frame + 4000;
				std::printf("[PANNEAUX] POSE : la fenetre reste ouverte jusqu'a l'image %d\n",
							s.frameFinPose);
				std::fflush(stdout);
				return;
			}
			shell->RequestClose();
		}

	} // namespace noge
} // namespace nkentseu
