// =============================================================================
// @File    main.cpp — NkChromeProbe
// @Brief   SONDE DE MESURE (b1) : `NkEditorShell::SetHeaderLayout` honore-t-il
//          ses PIXELS, ou les passe-t-il par le facteur d'echelle ?
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
//
// ⚠️ CETTE APPLICATION N'EST PAS UN PRODUIT. Sa fenetre le dit dans son titre.
//    Elle ne sert qu'a une mesure : poser une hauteur d'en-tete, capturer le
//    backbuffer, et laisser un script compter les lignes de la bande.
//
// POURQUOI ELLE NE DESSINE RIEN
//   Le seul pixel qui compte est le FOND de la barre de titre, peint par le
//   shell lui-meme (`DrawTitleBar` : `dl.AddRectFilled(bar, mUI.theme.header)`).
//   La sonde force donc `theme.header` a une couleur FRANCHE et met tout le
//   reste du theme a une autre couleur franche : la frontiere ne depend alors
//   d'aucun reglage, d'aucune police, d'aucun contenu.
//
// LE ZERO DU COMPTEUR SE PROUVE AVEC LA MEME SONDE
//   `NKPROBE_CIBLE=0` peint `theme.header` de la MEME couleur que le reste :
//   le compteur de lignes-cibles doit alors rendre 0. Ce n'est pas une image
//   vide inventee pour faire plaisir au compteur — c'est la sonde entiere, la
//   cible en moins.
//
// REGLAGES (tous par variables d'environnement, pour ne construire qu'une fois)
//   NKPROBE_TITLEH=<px>   hauteur passee a SetHeaderLayout ; 0 = calcul historique
//   NKPROBE_FONT=<px>     taille de police d'interface forcee ; 0 = defaut
//   NKPROBE_CIBLE=0|1     1 = header en couleur cible ; 0 = header comme le fond
//   NKPROBE_OUT=<chemin>  fichier de capture (.png)
//   NKPROBE_FRAME=<n>     frame a capturer (defaut 6) ; on ferme a n+3
// =============================================================================

#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEditorKit/NkEditorKit.h"
#include "NKEditorKit/NkEditorCanvasRenderer.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKLogger/NkLog.h"
#include <cstdlib>
#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::editorkit;

NKENTSEU_DEFINE_APP_DATA(([]() {
	NkAppData d{};
	d.appName = "NkChromeProbe";
	d.appVersion = "0.1.0";
	return d;
})());

namespace {

	float32 EnvF(const char *nom, float32 defaut) {
		const char *v = std::getenv(nom);
		if (!v || !*v)
			return defaut;
		return (float32)std::atof(v);
	}

	int32 EnvI(const char *nom, int32 defaut) {
		const char *v = std::getenv(nom);
		if (!v || !*v)
			return defaut;
		return (int32)std::atoi(v);
	}

	const char *EnvS(const char *nom, const char *defaut) {
		const char *v = std::getenv(nom);
		return (v && *v) ? v : defaut;
	}

	// ── LA BARRE DE MENUS EST REMPLACEE PAR RIEN ─────────────────────────────
	// Le defaut du shell dessine Fichier/Affichage/Fenetre. Ce texte tomberait
	// DANS la bande cible et ferait baisser son taux de remplissage. On pose
	// donc un hook vide : c'est le point d'extension prevu (SetMenuBar
	// « REMPLACE ENTIEREMENT les menus par defaut »), pas un contournement.
	void MenuBarVide(NkEditorFrameContext &, void *) {
	}

	// Panneau minimal : il ne peint rien, il COMPTE les images. C'est le seul
	// endroit appele une fois par image sans rien dessiner dans le corps.
	class PanneauCadence final : public NkEditorPanel {
		public:
			PanneauCadence() : NkEditorPanel("Sonde", NkEditorDockSide::NK_CENTER) {
			}

			NkEditorShell *shell = nullptr;
			int32 frame = 0;
			int32 frameCapture = 6;
			const char *sortie = "probe.png";
			bool armee = false;

			void OnUI(NkEditorFrameContext &) override {
				++frame;
				if (!shell)
					return;
				if (frame == frameCapture && !armee) {
					// ⚠️ LA CONDITION D'ESSAI SE MESURE, ELLE NE SE SUPPOSE PAS.
					//    Un banc a deja mesure une pente de 10 degres sur un sol
					//    plat, avec des chiffres coherents entre eux. On imprime
					//    donc l'echelle REELLE au moment de la capture, et la
					//    hauteur que le contexte porte.
					nkgui::NkGuiContext &ui = shell->Ui();
					std::printf("[SONDE] etat a la capture : scale=%.4f  DpiScale=%.4f  "
								"ItemHeight=%.2f  titleBarH=%.2f  viewW=%d viewH=%d\n",
								ui.scale, shell->DpiScale(), ui.ItemHeight(), ui.titleBarH,
								ui.viewW, ui.viewH);
					NkIEditorRenderer *r = shell->Renderer();
					// ⚠️ ON N'AFFIRME PAS QUE LA CAPTURE A EU LIEU : `CaptureNext`
					//    rend faux par defaut (« un backend sans readback ne promet
					//    rien qu'il ne tiendra pas »). On journalise SON retour.
					const bool ok = r ? r->CaptureNext(sortie) : false;
					armee = ok;
					std::printf("[SONDE] CaptureNext(\"%s\") -> %s\n", sortie, ok ? "ARMEE" : "REFUSEE");
					std::fflush(stdout);
					if (!ok)
						std::fprintf(stderr, "[SONDE] le backend a REFUSE la capture\n");
				}
				if (frame >= frameCapture + 3)
					shell->RequestClose();
			}
	};

} // namespace

int nkmain(const NkEntryState &state) {
	(void)state;

	const float32 titleH = EnvF("NKPROBE_TITLEH", 28.f);
	const float32 fontPx = EnvF("NKPROBE_FONT", 0.f);
	const int32 cible = EnvI("NKPROBE_CIBLE", 1);
	const char *sortie = EnvS("NKPROBE_OUT", "probe.png");
	const int32 frameCap = EnvI("NKPROBE_FRAME", 6);

	std::printf("[SONDE] titleH=%.1f font=%.1f cible=%d out=%s frame=%d\n", titleH, fontPx, cible,
				sortie, frameCap);
	std::fflush(stdout);

	auto shell = memory::NkMakeUnique<NkEditorShell>();
	static NkEditorCanvasRenderer canvasRenderer;

	NkEditorShellConfig cfg;
	// ⚠️ LE TITRE EST UNE GARDE, PAS UNE DECORATION.
	cfg.title = "*** SONDE DE MESURE - CETTE FENETRE N'EST PAS LE PRODUIT ***";
	cfg.width = 1280;
	cfg.height = 720;
	// ── LE DORSAL N'EST PAS UN GOUT ICI, C'EST UNE CONTRAINTE MESUREE ────────
	// `NkRenderWindow::Capture` (NkRenderWindowCapture.cpp:97-107) ne relit le
	// backbuffer QUE sur DX11 : « autres backends (GL/DX12/Vulkan/Metal/
	// Software) : a venir ». En OpenGL, la capture s'arme et n'ecrit rien --
	// mesure faite, six fichiers absents. La sonde tourne donc en DX11 par
	// defaut, et `NKPROBE_API` permet de refaire la mesure ailleurs le jour ou
	// le readback existera.
	const char *api = EnvS("NKPROBE_API", "dx11");
	cfg.graphicsApi = (api[0] == 'g') ? NkEditorGfxApi::OpenGL : NkEditorGfxApi::DX11;
	cfg.renderer = &canvasRenderer;
	if (!shell || !shell->Init(cfg)) {
		std::fprintf(stderr, "[SONDE] Init du shell ECHOUE\n");
		return -1;
	}

	// ── LA CIBLE : UNE SEULE COULEUR, ET ELLE N'EXISTE NULLE PART AILLEURS ───
	// On ne repeint PAS tout le theme : on pose `header` — le seul jeton que
	// `DrawTitleBar` utilise pour le fond de la bande — a un ROUGE PUR exact
	// (255,0,0). Aucun jeton du theme par defaut ne vaut cette valeur (verifie
	// dans NkGuiContext.h : le plus proche, `danger`, vaut 232,106,106). Le
	// compteur teste donc l'EGALITE EXACTE, et aucun anticrenelage de texte ne
	// peut fabriquer un faux positif.
	//
	// `NKPROBE_CIBLE=0` laisse `header` a sa valeur de theme : le compteur doit
	// alors rendre ZERO. C'est la preuve du zero, faite par la sonde elle-meme,
	// la cible en moins — pas par une image vide fabriquee pour l'occasion.
	if (cible != 0) {
		nkgui::NkGuiTheme t = shell->Ui().theme;
		t.header = nkgui::NkColor{255, 0, 0, 255};
		shell->Ui().theme = t;
	}

	// ── L'ECHELLE : LE SEUL LEVIER QUI REND LE TEST DECISIF ──────────────────
	// Le bureau de Rodolf est a 96 dpi (echelle 1,00) et je n'y touche pas.
	// A l'echelle 1, `S(28)` vaut 28 : une mesure a 28 ne prouverait donc RIEN
	// sur le passage par `S()`. On force donc l'echelle du contexte NKGui.
	//
	// ⚠️ ET CE LEVIER EST LUI-MEME UNE MESURE : `SetUiScale` n'est appele par
	//    AUCUNE application du kit (verifie : seul NKGuiDemo l'appelle). Le
	//    shell tourne donc aujourd'hui a `scale = 1.f` sur tout ecran.
	const float32 echelle = EnvF("NKPROBE_SCALE", 0.f);
	if (echelle > 0.f)
		nkgui::SetUiScale(shell->Ui(), echelle);

	if (fontPx > 0.f)
		shell->ForceUiFontSize(fontPx);

	// La hauteur MESUREE. 0 = on laisse le calcul historique (le temoin qui doit
	// bouger avec la police) ; > 0 = la valeur imposee (celle qui ne doit pas).
	if (titleH > 0.f)
		shell->SetHeaderLayout(titleH, 0.f, 0.f);

	// Ni barre d'outils ni barre d'etat : une seule bande a mesurer.
	shell->SetMenuBar(&MenuBarVide, nullptr);
	shell->SetStatusBarVisible(false);
	shell->SetActivityBars(false, false);
	shell->SetFooterZoomIndicator(false);

	static PanneauCadence cadence;
	cadence.shell = shell.Get();
	cadence.frameCapture = frameCap;
	cadence.sortie = sortie;
	shell->AddPanel(&cadence);

	const int rc = shell->Run();

	// ⚠️ « ARMEE » N'EST PAS « ECRITE ». `NkEditorCanvasRenderer::EndFrame`
	//    appelle `mTarget->Capture(...)` SANS regarder son retour ; un dorsal
	//    sans readback echoue alors en silence. On verifie donc le FICHIER,
	//    seule preuve qui ne depende d'aucune promesse.
	long taille = -1;
	if (FILE *f = std::fopen(sortie, "rb")) {
		std::fseek(f, 0, SEEK_END);
		taille = std::ftell(f);
		std::fclose(f);
	}
	std::printf("[SONDE] fin, code=%d, images=%d, capture=%s (%ld octets)\n", rc, cadence.frame,
				taille >= 0 ? "ECRITE" : "ABSENTE", taille);
	std::fflush(stdout);
	if (taille < 0)
		std::fprintf(stderr, "[SONDE] ECHEC : aucun fichier de capture a '%s'\n", sortie);
	return rc;
}
