// =============================================================================
// ConquerorLab — point d'entree.
//
// Assemble : catalogue de modules -> session de partie -> coquille d'editeur
// avec six panneaux. Tout le pipeline (fenetre, docking, rendu, polices) vient
// de NKEditorKit ; ce fichier ne fait que cabler.
//
// TROIS POINTS QUI ONT COUTE, ET QU'ON NE REDECOUVRIRA PAS
//
//   1. LE SHELL EST ALLOUE SUR LE TAS. NkEditorShell porte le contexte NKGui
//      complet (pool de 32 draw-lists de fenetres, arbre de dock, caches de
//      polices) : plus d'un megaoctet. Sur la pile, c'est un debordement au
//      demarrage, sans message utile.
//
//   2. LE THEME S'ECRIT APRES Init. Le shell pose son propre theme puis
//      RECHARGE le theme utilisateur sauvegarde a la fin de son Init : ecrire
//      avant serait ecrase sans bruit.
//
//   3. LA PARTIE AVANCE DANS L'OVERLAY, PAS DANS UN PANNEAU. Si on faisait
//      tourner la boucle de jeu dans OnUI du plateau, fermer ce panneau
//      arreterait la partie — et la campagne de mesure avec elle. Le hook
//      overlay du shell est appele a chaque frame, quel que soit l'etat des
//      panneaux.
// =============================================================================

#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEditorKit/NkEditorKit.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKFileSystem/NkPath.h"
#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"

#include "ConquerorLab/NkcLayout.h"
#include "ConquerorLab/NkcModuleHost.h"
#include "ConquerorLab/NkcSession.h"
#include "ConquerorLab/NkcLabTheme.h"
#include "ConquerorLab/NkcBoardPanel.h"
#include "ConquerorLab/NkcRulesPanel.h"
#include "ConquerorLab/NkcPlayersPanel.h"
#include "ConquerorLab/NkcModulesPanel.h"
#include "ConquerorLab/NkcJournalPanel.h"
#include "ConquerorLab/NkcMetricsPanel.h"
#include "ConquerorLab/NkcOutputPanel.h"

using namespace nkentseu;
using namespace nkentseu::editorkit;
using namespace nkentseu::conqueror;

NKENTSEU_DEFINE_APP_DATA(([]() {
	NkAppData d{};
	d.appName	 = "ConquerorLab";
	d.appVersion = "0.1.0";
	return d;
})());

namespace {

	// -------------------------------------------------------------------------
	/// Racine du depot. Trouvee par un MARQUEUR, pas par un nombre de « .. » :
	/// le binaire ne vit pas au meme niveau selon la configuration et la
	/// plateforme, et compter les niveaux casse silencieusement des qu'un
	/// dossier change.
	NkString FindRepoRoot() noexcept {
		// DEUX marqueurs : l'arborescence de developpement, et celle du kit livre.
		// Le kit ne recopie plus la disposition du depot — voir NkcLayout.h.
		static const char *kMarkers[2] = {
			"/Applications/ConquerorLab/include/Conqueror/ConquerorRulesABI.h",
			"/include/Conqueror/ConquerorRulesABI.h"
		};

		NkString bases[2];
		bases[0] = NkPath::GetExecutableDirectory().ToString();
		bases[1] = NkPath::GetCurrentDirectory().ToString();

		for (int32 b = 0; b < 2; ++b) {
			NkString cur = bases[b];
			for (int32 up = 0; up < 8 && !cur.Empty(); ++up) {
				for (int32 m = 0; m < 2; ++m) {
					NkString probe = cur;
					probe += kMarkers[m];
					if (NkFile::Exists(probe.CStr())) return cur;
				}
				const usize slash = cur.RFind('/');
				const usize back  = cur.RFind('\\');
				usize		cut	  = slash;
				if (back != NkString::npos && (slash == NkString::npos || back > slash)) cut = back;
				if (cut == NkString::npos) break;
				cur = cur.SubStr(0, cut);
			}
		}
		// Introuvable (build deploye, Android, Web) : les modules internes
		// suffisent, et le dossier de depot sera simplement vide.
		return NkPath::GetExecutableDirectory().ToString();
	}

	// -------------------------------------------------------------------------
	/// Etat applicatif. Statique : sa duree de vie doit couvrir celle du shell,
	/// qui NE POSSEDE PAS les panneaux.
	struct LabApp {
			NkcLayout		layout;
			NkcModuleLog	modLog;
			NkcModuleHost host;
			NkcSession	  session;

			NkcBoardPanel	board{&session};
			NkcRulesPanel	rules{&session};
			NkcPlayersPanel players{&session};
			NkcModulesPanel modules{&session};
			NkcJournalPanel journal{&session};
			NkcMetricsPanel metrics{&session};
			NkcOutputPanel  output{&modLog};

			NkEditorShell *shell   = nullptr;
			int32		   lastW   = 0;
			bool		   compact = false;
			bool		   adapted = false;
	};

	LabApp *gApp = nullptr;

	// -------------------------------------------------------------------------
	/// Ecrans etroits (telephone, fenetre reduite) : les panneaux lateraux se
	/// replient et le plateau garde tout l'ecran (HANDOFF §2.5). On ne bascule
	/// qu'au FRANCHISSEMENT du seuil, sinon on empecherait l'utilisateur de
	/// rouvrir un panneau a la main.
	void AdaptLayout(LabApp &app, int32 width) noexcept {
		const bool wantCompact = width > 0 && width < 900;
		if (app.adapted && wantCompact == app.compact) return;
		app.adapted = true;
		app.compact = wantCompact;

		static const char *kSides[] = {"Regles", "Joueurs", "Modules", "Journal", "Metriques"};
		for (const char *t : kSides) {
			if (wantCompact) app.shell->ClosePanel(t);
			else			 app.shell->FocusPanel(t);
		}
		if (!wantCompact) app.shell->FocusPanel("Plateau");
	}

	// -------------------------------------------------------------------------
	/// Hook overlay : appele a CHAQUE frame par le shell, panneaux ouverts ou
	/// non. C'est ici que la partie avance et que le catalogue est scrute.
	void TickApp(NkEditorFrameContext &ec, void *user) {
		LabApp *app = static_cast<LabApp *>(user);
		if (!app) return;

		const int32 w = ec.Ui().viewW;
		if (w != app->lastW) {
			app->lastW = w;
			AdaptLayout(*app, w);
		}

		app->session.PollModules(ec.dt);
		app->session.Tick(ec.dt);

		char footer[192];
		std::snprintf(footer, sizeof(footer), "%s   |   %u coups   |   empreinte %llu",
					  app->session.Thinking() ? "reflexion..." : "pret",
					  static_cast<unsigned>(app->session.Journal().Size()),
					  static_cast<unsigned long long>(app->session.Hash()));
		app->shell->SetFooter("ConquerorLab — atelier de test de Conqueror", footer);
	}

	// ---- commandes (palette Ctrl+P + menu applicatif) -----------------------
	void CmdNewGame(void *) { if (gApp) gApp->session.NewGame(); }
	void CmdTogglePlay(void *) { if (gApp) gApp->session.SetAutoPlay(!gApp->session.AutoPlay()); }
	void CmdStep(void *) { if (gApp) gApp->session.StepOnce(); }
	void CmdRescan(void *) { if (gApp) gApp->session.ScanModules(); }
	void CmdLive(void *) { if (gApp) gApp->session.LiveView(); }
	void CmdResetLayout(void *u) { if (u) static_cast<NkEditorShell *>(u)->ResetLayout(); }
	void CmdQuit(void *u) { if (u) static_cast<NkEditorShell *>(u)->RequestClose(); }

	void AppMenu(NkEditorFrameContext &ec, void *) {
		using namespace nkentseu::nkgui;
		NkGuiContext &ctx = ec.Ui();
		if (!BeginMenu(ctx, "Conqueror")) return;
		if (MenuItem(ctx, "Nouvelle partie", "Ctrl+N")) CmdNewGame(nullptr);
		if (MenuItem(ctx, gApp && gApp->session.AutoPlay() ? "Pause" : "Lecture", "Espace"))
			CmdTogglePlay(nullptr);
		if (MenuItem(ctx, "Un coup", "Ctrl+.")) CmdStep(nullptr);
		Separator(ctx);
		if (MenuItem(ctx, "Revenir a la position vivante")) CmdLive(nullptr);
		if (MenuItem(ctx, "Rechercher et compiler les modules", "F5")) CmdRescan(nullptr);
		EndMenu(ctx);
	}

} // namespace

int nkmain(const NkEntryState &state) {
	(void)state;

	// Le shell est enorme : sur le TAS, jamais sur la pile.
	auto shell = memory::NkMakeUnique<NkEditorShell>();
	if (!shell) return -1;

	NkEditorShellConfig cfg;
	cfg.title  = "ConquerorLab — atelier de test de Conqueror";
	cfg.width  = 1440;
	cfg.height = 900;
	if (!shell->Init(cfg)) return -1;

	// GitHub Dark Pro — APRES Init (cf. en-tete, point 2).
	ApplyGitHubDarkPro(shell->Ui());

	// PAS DE BARRES D'ACTIVITE. Elles servent a basculer entre des « vues » dans
	// un IDE ; l'atelier n'en a aucune, et les garder lui donnait le chrome de
	// NKCode sans en avoir le metier. Le dock reprend toute la largeur.
	shell->SetActivityBars(false, false);

	// NOS PANNEAUX OUVRENT DES COMBOS (grille, pilote, palier). Par defaut la
	// coquille neutralise l'input du corps des qu'un popup NKGui est sous la
	// souris — protection destinee aux menus de la barre de titre. Mais un combo
	// de panneau est dessine DANS le corps : il s'ouvrait, puis n'acceptait plus
	// aucun clic et refusait de se refermer.
	//
	// En echange, tous nos hit-tests « bruts » (plateau, listes) sont gardes par
	// `ctx.popupDepth == 0` : un clic destine a un popup ne traverse pas.
	shell->SetMaskBodyOnPopup(false);

	static LabApp app;
	gApp	  = &app;
	app.shell = shell.Get();

	// TROIS DOSSIERS, TROIS NATURES DE CONTRIBUTION — c'est tout ce qu'un
	// stagiaire a besoin de savoir :
	//   rules/  un .cpp qui implemente NkcRulesVTable  -> compile et charge
	//   ai/     un .cpp qui implemente NkcAIVTable     -> compile et charge
	//   boards/ un .json au format LoadBoardJson       -> lu tel quel, sans build
	app.layout.Init(FindRepoRoot());
	const NkString boardsDir = app.layout.WorkDir("boards");
	// Bibliotheque de grilles livree, recopiee au premier lancement dans le
	// dossier de travail. Sans elle, la liste des grilles n'avait qu'une entree
	// et il n'y avait litteralement rien a choisir.
	//
	// Elle ne vit pas au meme endroit dans les deux dispositions : c'est
	// NkcLayout qui sait, pas ce fichier.
	const NkString boardsSeed = app.layout.SeedBoardsDir();
	logger.Infof("[lab] racine (%s) : %s", app.layout.KindName(),
				 app.layout.Root().CStr());

	app.host.Init(app.layout, &app.modLog);
	app.session.Init(&app.host, boardsDir, boardsSeed);

	shell->AddPanel(&app.board);
	shell->AddPanel(&app.rules);
	shell->AddPanel(&app.players);
	shell->AddPanel(&app.modules);
	shell->AddPanel(&app.journal);
	shell->AddPanel(&app.metrics);
	shell->AddPanel(&app.output);

	shell->SetOverlay(&TickApp, &app);
	shell->SetAppMenu(&AppMenu, &app);

	shell->RegisterCommand("Partie : Nouvelle", &CmdNewGame, nullptr, "Ctrl+N");
	shell->RegisterCommand("Partie : Lecture / Pause", &CmdTogglePlay, nullptr, "Espace");
	shell->RegisterCommand("Partie : Un coup", &CmdStep, nullptr, "Ctrl+.");
	shell->RegisterCommand("Partie : Position vivante", &CmdLive, nullptr);
	shell->RegisterCommand("Modules : Rechercher et compiler", &CmdRescan, nullptr, "F5");
	shell->RegisterCommand("Disposition : Reinitialiser", &CmdResetLayout, shell.Get());
	shell->RegisterCommand("Application : Quitter", &CmdQuit, shell.Get(), "Ctrl+Q");

	const int rc = shell->Run();

	// Ordre de destruction impose : la session detient un thread de reflexion et
	// des instances de modules ; les DLL doivent etre fermees APRES.
	app.session.Shutdown();
	app.host.Shutdown();
	gApp = nullptr;
	return rc;
}
