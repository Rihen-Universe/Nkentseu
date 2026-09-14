// -----------------------------------------------------------------------------
// @File    NkPanneauxSonde.cpp
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// Mesure de la disposition — voir l'en-tete pour ce qu'elle mesure et ce
// qu'elle refuse de faire (elle ne compare a rien, elle ne pilote rien).
// -----------------------------------------------------------------------------
#include "Nogee/Shell/NkPanneauxSonde.h"
#include "NKEditorKit/NkEditorShell.h"

namespace nkentseu {
	namespace noge {

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
			const float32 toolbarH = corps.y - titleH;
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
			std::printf("[PANNEAUX] bande titre=%.2f outils=%.2f etat=%.2f  rail_g=%.2f rail_d=%.2f\n",
						static_cast<double>(titleH), static_cast<double>(toolbarH),
						static_cast<double>(footerH), static_cast<double>(railG),
						static_cast<double>(railD));

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
