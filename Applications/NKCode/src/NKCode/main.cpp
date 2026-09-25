// =============================================================================
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// main.cpp — Point d'entree de NKCode (IDE type VSCode, base sur Jenga).
// Coquille = NKEditorKit (sur NKGui). Panneaux : Explorateur (fichiers reels),
// Editeur (onglets + saisie), Sortie (jenga build). Commandes : Construire /
// Executer (jenga) + Enregistrer. Le visuel/Blueprint/UIBuilder viendront ensuite.
// =============================================================================
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEditorKit/NkEditorKit.h"
// ⚠️ L'umbrella ne tire PAS l'implementation NKCanvas, deliberement : le kit
// serait alors lie a NKCanvas chez TOUS ses consommateurs, y compris ceux qui
// rendent en NKRHI. C'est a l'application de choisir son backend et de
// l'inclure. Voir NkEditorShell::Init (2026-09-01).
#include "NKEditorKit/NkEditorCanvasRenderer.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKCode/Shell/Panels.h"
#include "NKCode/Shell/Toolbar.h"
#include "NKCode/Shell/Dialogs.h"
#include "NKCode/Shell/NkMenuBar.h" // barre de menus principale (spec Banani, remplace les menus shell)
#include "NKCode/Shell/ScaffoldPanels.h"
#include "NKCode/Shell/NkProblemsPanel.h"
#include "NKCode/Shell/NkGitPanel.h"
#include "NKCode/Shell/NkDebugPanel.h"
#include "NKCode/Shell/NkAiPanel.h"
#include "NKEditorKit/NkEditorScriptEvenements.h" // (Q9) NK_EVENEMENTS
#include "NKCode/Shell/NkHome.h"
#include "NKCode/Shell/NkAppFonts.h"
#include "NKCode/Shell/NkAppIcons.h"
#include "NKCode/Shell/NkAppCommands.h"
#include "NKCode/Shell/NkOpenWindows.h" // registre des fenetres ouvertes (restauration au lancement)
#include "NKCode/Project/NkLogSink.h"
#include "NKImage/NKImage.h"
#include "NKPlatform/NkEnv.h" // env::GetEnvVar (variables d'environnement maison)
#include "NKCode/Editor/NkSyntaxLangs.h" // coloration data-driven (CSS, JS, Lua…)
#include "NKEditorKit/NkAiPanneauImage.h" // NK_AI_IMAGE : le panneau IA rendu par l'application

#include <cstdio>
#include <cstddef>

using namespace nkentseu;
using namespace nkentseu::editorkit;

NKENTSEU_DEFINE_APP_DATA(([]() {
	NkAppData d{};
	d.appName = "NKCode";
	d.appVersion = "0.1.0";
	return d;
})());

// Etat global de l'IDE (duree de vie = appli).
static nkcode::NkCodeState g_state;

// Dialogues modaux (creation/enregistrement) + items du menu Fichier.
static nkcode::NkCodeDialogs g_dialogs;

// Ecran d'accueil (Home) — nouvelle UI propre (design Banani).
static nkcode::NkHomeState g_home;

// Mises a jour in-app (ROADMAP Phase 13) : verification GitHub Releases,
// notification, puis telechargement de l'installeur Inno qui met a jour EN
// PLACE et relance NKCode.
static nkcode::NkUpdateState g_update;
// Mise a jour de JENGA seul : 1 Mo, aucune reinstallation de NKCode.
// Separee de g_update parce que les deux produits n'ont ni le meme
// rythme de publication ni le meme cout de mise a jour.
static nkcode::NkJengaUpdateState g_jenga_update;



// ═══════════════════════════════════════════════════════════════════════════
//  LES CROCHETS DE PREUVE DU PANNEAU IA (21/09) — aucune injection d'entree
// ═══════════════════════════════════════════════════════════════════════════
//  NK_AI_PANNEAU=<assistant>,<image> : ouvre le panneau de cet assistant par la
//      MEME porte que l'icone de la barre d'activite (OpenSideExclusive).
//  NK_AI_IMAGE=<chemin>,<image>      : l'image du panneau, rendue par NKCode --
//      sa liste d'affichage COMPLETE, rasterisee sans GPU, decoupee au rectangle
//      que le panneau a PUBLIE. Jamais une capture de l'ecran.
//  NK_AGENT_EXIT=<image>             : ferme CETTE instance (RequestQuit).
//  Ils ne sont poses que si l'une des variables existe : un lancement normal ne
//  change pas d'un octet.
static nkcode::AiPanel *gPanneauxIA[4] = {nullptr, nullptr, nullptr, nullptr};

static nkentseu::int32 NkLireImage(const char *v, char *texte, nkentseu::usize cap, nkentseu::int32 defaut) {
	const char *virg = nullptr;
	for (const char *c = v; *c; ++c)
		if (*c == ',')
			virg = c;
	nkentseu::usize n = 0;
	for (const char *c = v; *c && (!virg || c < virg) && n + 1u < cap; ++c)
		texte[n++] = *c;
	texte[n] = 0;
	return virg ? (nkentseu::int32)std::atoi(virg + 1) : defaut;
}

static void NkCrochetsPanneauIA(nkentseu::nkgui::NkGuiContext &ui, nkentseu::int32 W, nkentseu::int32 H, void *u) {
	using namespace nkentseu;
	auto *sh = static_cast<NkEditorShell *>(u);
	static int32 sImage = 0;
	static int32 sOuvre = -2, sPhoto = -2, sSortie = -2;
	static char sQui[64] = {0}, sChemin[256] = {0};
	if (sOuvre == -2) {
		const char *a = std::getenv("NK_AI_PANNEAU");
		sOuvre = a ? NkLireImage(a, sQui, sizeof(sQui), 30) : -1;
		const char *b = std::getenv("NK_AI_IMAGE");
		sPhoto = b ? NkLireImage(b, sChemin, sizeof(sChemin), 120) : -1;
		const char *c = std::getenv("NK_AGENT_EXIT");
		sSortie = c ? (int32)std::atoi(c) : -1;
	}
	++sImage;
	{
		// (Q9) NK_EVENEMENTS : rejoue par les rappels de la coquille
		static editorkit::NkEditorScriptEvenements sScript;
		sScript.Tick();
	}
	if (const char *v = std::getenv("NK_AI_ETAT"))
		for (const char *c = v; *c;) {
			if (std::atoi(c) == sImage)
				for (int32 k = 0; k < 4; ++k)
					if (gPanneauxIA[k] && gPanneauxIA[k]->IsOpen()) {
						gPanneauxIA[k]->Kit().TracerEtat(ui, "nkcode", sImage);
						break;
					}
			while (*c && *c != ',')
				++c;
			if (*c == ',')
				++c;
		}
	if (sImage == sOuvre && sh) {
		int32 gN = 0;
		const char *const *g = nkcode::SideRightGroup(gN);
		nkcode::OpenSideExclusive(sh, g, gN, sQui);
		printf("[nkcode] AI PANNEAU image=%d : « %s » ouvert\n", (int)sImage, sQui);
		fflush(stdout);
	}
	if (sImage == sPhoto) {
		editorkit::NkPaintRect r{};
		const char *qui = "";
		for (int32 k = 0; k < 4; ++k)
			if (gPanneauxIA[k] && gPanneauxIA[k]->IsOpen() && gPanneauxIA[k]->Kit().rect.w > 0.f) {
				r = gPanneauxIA[k]->Kit().rect;
				qui = gPanneauxIA[k]->Title();
				break;
			}
		const nkgui::NkGuiFont *polices[3] = {ui.font, sh ? sh->TermCodeFont() : nullptr,
											  nkcode::AiPanel::PoliceCorpsIA().Valid() ? &nkcode::AiPanel::PoliceCorpsIA()
																					   : nullptr};
		const nkgui::NkGuiDrawList *listes[2] = {&ui.dl, &ui.dlOverlay};
		char c1[300], c2[300];
		snprintf(c1, sizeof(c1), "%s.png", sChemin);
		snprintf(c2, sizeof(c2), "%s_fenetre.png", sChemin);
		const uint32 fond = sh ? sh->KitTheme().Get(editorkit::NkRole::WindowBg) : 0x010409FFu;
		const editorkit::NkAiImageResultat r1 =
			editorkit::NkAiEcrireImageListes(listes, 2, W, H, r.x, r.y, r.w, r.h, polices, 3, fond, c1);
		const editorkit::NkAiImageResultat r2 = editorkit::NkAiEcrireImageListes(
			listes, 2, W, H, 0.f, 0.f, (float32)W, (float32)H, polices, 3, fond, c2);
		printf("[nkcode] AI IMAGE image=%d panneau « %s » (%.0f,%.0f,%.0f,%.0f) : %s | %s\n", (int)sImage, qui,
			   (double)r.x, (double)r.y, (double)r.w, (double)r.h, r1.ok ? r1.message : "ECHEC",
			   r2.ok ? r2.message : "ECHEC");
		fflush(stdout);
	}
	// NK_AI_MENU=<modeles|modes|fournisseurs|plus>,<image> (Q5) : ouvre un menu du
	// kit comme son clic -- l'etat qu'un clic ecrirait, aucune entree injectee.
	{
		static int32 sMenuQuand = -2;
		static char sMenu[32] = {0};
		if (sMenuQuand == -2) {
			const char *m = std::getenv("NK_AI_MENU");
			sMenuQuand = m ? NkLireImage(m, sMenu, sizeof(sMenu), 800) : -1;
		}
		if (sImage == sMenuQuand)
			for (int32 k = 0; k < 4; ++k)
				if (gPanneauxIA[k] && gPanneauxIA[k]->IsOpen()) {
					editorkit::NkAiPanneau &kit = gPanneauxIA[k]->KitModifiable();
					const editorkit::NkAiMenu mm = std::strcmp(sMenu, "modeles") == 0 ? editorkit::NkAiMenu::Modeles
												   : std::strcmp(sMenu, "modes") == 0 ? editorkit::NkAiMenu::Modes
												   : std::strcmp(sMenu, "plus") == 0  ? editorkit::NkAiMenu::Plus
																					  : editorkit::NkAiMenu::Fournisseurs;
					kit.OuvrirMenu(mm, kit.Actif());
					printf("[nkcode] AI MENU image=%d : %s\n", (int)sImage, sMenu);
					fflush(stdout);
					break;
				}
	}
	if (sImage == sSortie && sh) {
		// LA SONDE SE FERME ELLE-MEME, par la porte que la confirmation emprunte :
		// `RequestQuit` seul est VETOE par la question « quitter ? » (voulue pour
		// la main). Cette fenetre est la notre -- aucune autre n'est touchee.
		g_state.quitConfirmed = true;
		g_state.ConfirmQuit();
	}
}

int nkmain(const NkEntryState &state) {
	(void)state;

	// ── Dossier de l'EXECUTABLE, calcule EN PREMIER ──────────────────────────
	// Demande a l'OS (GetModuleFileNameW / /proc/self/exe / _NSGetExecutablePath),
	// PAS deduit de argv[0] : argv[0] n'a pas de dossier quand l'exe est lance
	// via le PATH, et est relatif quand il est lance depuis un autre dossier.
	// Doit etre pose AVANT le chargement des polices/icones (qui cherchent
	// aussi a cote de l'exe) et avant NkEmbeddedJenga::Configure.
	{
		NkString exeDir = NkPath::GetExecutableDirectory().ToString();
		if (exeDir.Empty() && state.args.Size() > 0)
			exeDir = NkPath(state.args[0].CStr()).GetParent().ToString(); // repli
		nkcode::NkOpenWsState::ExeDir() = exeDir;
	}

	nkcode::InstallLogSink(); // capture les logs NKLogger -> panneau OUTPUT

	nkcode::NkSynInitDefaultLangs(); // coloration data-driven (CSS, JS, Lua, Rust…)
	nkcode::NkLoadFallbackFonts();	 // polices de repli (broad/CJK/emoji)

	auto shell = memory::NkMakeUnique<NkEditorShell>();
	NkEditorShellConfig cfg;
	// ⚠️ UNE FENETRE DE SONDE SE DENONCE (21/09) : `NK_SONDE` -- le meme marqueur
	//    que NK3DModeler. Sans lui, pas un caractere ne change.
	cfg.title = std::getenv("NK_SONDE") ? "*** SONDE DE MESURE - CETTE FENETRE N'EST PAS LE PRODUIT *** NKCode"
										: "NKCode - IDE (Jenga)";
	cfg.width = 1440; // grande fenetre centree, REDIMENSIONNABLE (pas maximisee de force)
	cfg.height = 900;
	// ── BACKEND DE RENDU, INJECTE ────────────────────────────────────────
	// Le kit n'en cree plus par defaut depuis le 2026-09-01 : un defaut dans
	// son .cpp etait une dependance de LIEN pour tout le monde. `static` parce
	// que le shell NE POSSEDE PAS ce pointeur -- l'objet doit lui survivre.
	static NkEditorCanvasRenderer canvasRenderer;
	cfg.renderer = &canvasRenderer;
	if (!shell || !shell->Init(cfg))
		return -1;

	// Géométrie du LAUNCHER : mémorisée globalement (taille/pos/moniteur). Par
	// DÉFAUT (aucun état sauvegardé) -> maximisé. Un workspace ouvert écrasera
	// ensuite cette géométrie via son ui.cfg (état par workspace).
	const nkentseu::NkString g_launcherGeom =
		(NkPath(nkcode::NkOpenWsState::Home().CStr()) / ".nkcode" / "window.cfg").ToString();
	if (!shell->LoadWindowGeom(g_launcherGeom.CStr()))
		shell->MaximizeWindow();

	// Ouvre un fichier au demarrage (demo) : le README de NKCode.
	g_state.OpenPath(g_state.root / "README.md");

	static nkcode::ExplorerPanel explorer(&g_state, shell.Get());
	static nkcode::OutlinePanel outline(&g_state);
	static nkcode::EditorPanel editor(&g_state, shell.Get());
	static nkcode::OutputPanel output(&g_state, shell.Get());
	static nkcode::TerminalPanel terminal;
	// Panneau d'EXECUTION : meme moteur de terminal, mais reserve aux programmes
	// lances par « Demarrer » — pour ne pas les melanger aux shells que
	// l'utilisateur garde ouverts. Ne cree jamais de shell tout seul.
	static nkcode::TerminalPanel runTerm("EXECUTION", /*runMode=*/true);
	shell->AddPanel(&explorer);
	shell->AddPanel(&outline);
	shell->AddPanel(&editor);
	shell->AddPanel(&output);
	shell->AddPanel(&terminal);
	shell->AddPanel(&runTerm);

	// Maquettes des interfaces (interface.md) : structure visuelle d'abord, rendu
	// fonctionnel ensuite (roadmap #2-#20). Fermees par defaut -> menu Affichage.
	using nkcode::ScaffoldPanel;
	namespace sc = nkcode::scaffold;
	static nkcode::SearchPanel pSearch(&g_state); // Recherche workspace FONCTIONNELLE (remplace la maquette #7)
	// Panneau Problemes REEL (roadmap #8) : diagnostics du dernier build, avec
	// saut fichier:ligne. Remplace la maquette sc::kProblems, desormais inutilisee.
	static nkcode::NkProblemsPanel pProblem(&g_state);
	// Panneau Git REEL (roadmap #9) : branche, fichiers modifies, indexation,
	// validation et historique — tout vient de commandes git reelles.
	static nkcode::NkGitPanel pGit(&g_state);
	// Panneau Debogueur : points d'arret REELS. Variables/pile/threads restent
	// absents faute de session GDB pilotee — le panneau le dit lui-meme.
	static nkcode::NkDebugPanel pDebug(&g_state);
	static ScaffoldPanel pBuild("Build & Taches", NkEditorDockSide::NK_BOTTOM, "Maquette - roadmap #14", sc::kBuild, 1);
	static ScaffoldPanel pDbgConsole("Console de debogage", NkEditorDockSide::NK_BOTTOM, "Maquette", sc::kDbgConsole, 1);
	static ScaffoldPanel pTests("Tests", NkEditorDockSide::NK_BOTTOM, "Maquette", sc::kTests, 1);
	static ScaffoldPanel pPorts("Ports", NkEditorDockSide::NK_BOTTOM, "Maquette", sc::kPorts, 1);
	static ScaffoldPanel pProf("Profiler", NkEditorDockSide::NK_LEFT, "Maquette - roadmap #19", sc::kProfiler, 1);
	static ScaffoldPanel pCollab("Live Collab", NkEditorDockSide::NK_LEFT, "Maquette - collaboration", sc::kCollab, 2);
	// 4 IA, MEME interface (AiPanel) mais backend + proprietes propres a chacune :
	//   0 Assistant (general), 1 Claude Code, 2 Codex, 3 NkAI (maison Rihen).
	static nkcode::AiPanel aiPanel(&g_state, shell.Get(), 0, "Assistant IA");
	static nkcode::AiPanel claudePanel(&g_state, shell.Get(), 1, "Claude Code");
	static nkcode::AiPanel codexPanel(&g_state, shell.Get(), 2, "Codex");
	static nkcode::AiPanel nkaiPanel(&g_state, shell.Get(), 3, "NkAI");
	gPanneauxIA[0] = &aiPanel;
	gPanneauxIA[1] = &claudePanel;
	gPanneauxIA[2] = &codexPanel;
	gPanneauxIA[3] = &nkaiPanel;
	if (std::getenv("NK_AI_IMAGE") || std::getenv("NK_AI_PANNEAU") || std::getenv("NK_AGENT_EXIT"))
		shell->SetApresImage(&NkCrochetsPanneauIA, shell.Get());
	static ScaffoldPanel pEngine("Moteur", NkEditorDockSide::NK_RIGHT, "Maquette - roadmap #17", sc::kEngine, 1);
	static ScaffoldPanel pExt("Extensions", NkEditorDockSide::NK_LEFT, "Maquette - roadmap #12", sc::kExtensions, 1);
	shell->AddPanel(&pSearch);
	shell->AddPanel(&pProblem);
	shell->AddPanel(&pGit);
	shell->AddPanel(&pDebug);
	shell->AddPanel(&pBuild);
	shell->AddPanel(&pDbgConsole);
	shell->AddPanel(&pTests);
	shell->AddPanel(&pPorts);
	shell->AddPanel(&pProf);
	shell->AddPanel(&aiPanel);
	shell->AddPanel(&claudePanel);
	shell->AddPanel(&codexPanel);
	shell->AddPanel(&nkaiPanel);
	shell->AddPanel(&pCollab);
	shell->AddPanel(&pEngine);
	shell->AddPanel(&pExt);

	shell->SetActivityHandler(&nkcode::ActivityThunk, shell.Get()); // sidebars exclusives (activity bar)
	// Drop de fichiers depuis l'OS -> état partagé (consommé par le panneau visé).
	shell->SetDropFilesHandler(
		+[](void *u, const NkVector<NkString> &paths, nkentseu::int32 x, nkentseu::int32 y) {
			auto *st = static_cast<nkcode::NkCodeState *>(u);
			st->osDropPaths = paths;
			st->osDropX = x;
			st->osDropY = y;
			st->osDropTtl = 3; // purge AppFlagsThunk si aucune cible ne consomme
		},
		&g_state);
	shell->SetToolbar(&nkcode::ToolbarThunk, &g_state);				// barre d'outils Visual Studio
	nkcode::NkZoomCtx() = {&g_state, shell.Get()};
	shell->SetZoomHandler(&nkcode::ZoomHandler, &nkcode::NkZoomCtx()); // zoom Ctrl+molette/±/0 -> onglet actif
	terminal.mShell = shell.Get();					 // police propre du terminal (non zoomee)
	terminal.mState = &g_state;						 // terminal demarre dans la racine du workspace
	runTerm.mShell = shell.Get();
	runTerm.mState = &g_state;
	g_dialogs.st = &g_state;
	g_dialogs.shell = shell.Get();
	g_dialogs.home = &g_home; // modale Preferences = panneau settings COMPLET du launcher
	g_state.LoadRecents();							// workspaces recents (ecran de demarrage)
	shell->SetAppMenu(&nkcode::AppFlagsThunk, &g_home); // user = NkHomeState	// pose appFullScreen/appModal chaque frame
	// Barre de menus COMPLETE (11 menus, spec Banani) : remplace les menus par
	// defaut du shell ET l'ancien SetFileMenu (items Fichier absorbes dedans).
	static nkcode::NkMenuBarCtx g_menuBar;
	g_menuBar.dlg = &g_dialogs;
	g_menuBar.shell = shell.Get();
	g_menuBar.home = &g_home; // « Nouveau Workspace » -> wizard complet du launcher (nav==2)
	g_menuBar.upd = &g_update; // menu Aide > Rechercher les mises a jour (Phase 13)
	g_menuBar.jup = &g_jenga_update; // menu Aide > Mettre a jour Jenga (1 Mo, sans reinstallation)
	// (g_menuBar.exePath est pose plus bas, avec le chemin COMPLET de l'exe.)
	shell->SetMenuBar(&nkcode::MainMenuBarThunk, &g_menuBar);
	shell->SetOverlay(&nkcode::OverlayThunk, &g_dialogs);	// dialogues modaux (creation/enregistrement)

	// ── Ecran d'accueil (Home) : nouvelle UI + logos/icones rasterises en texture ──
	g_home.st = &g_state;
	g_home.dlg = &g_dialogs;
	// ExeDir() a ete pose en tete de main (chemin fiable donne par l'OS).
	// Sans lui, NkEmbeddedJenga::Configure ne trouvait pas tools/python-embed
	// -> gProdTools=false -> mode embarque DESACTIVE -> les boutons Construire/
	// Executer retombaient sur un `jenga` du PATH absent chez un testeur sans
	// Python : c'est la cause des retours beta #9/#10 (« le Jenga inclus n'est
	// pas fonctionnel », « pas utilisable avec les boutons dedies »).
	{
		const NkString &exeDir = nkcode::NkOpenWsState::ExeDir();
		// « Ouvrir dans une nouvelle fenetre » : chemin COMPLET de l'exe (argv[0]
		// pouvait etre un simple nom -> relance impossible depuis un autre CWD).
		g_home.exePath = exeDir.Empty() ? ((state.args.Size() > 0) ? state.args[0] : NkString())
										: (exeDir + "/NKCode.exe");
		g_menuBar.exePath = g_home.exePath; // « Nouvelle fenetre » du menu Fenetre
		// Jenga IN-PROCESS (Phase 12) : memorise les chemins tools/python-embed +
		// tools/jenga-src (init de l'interpreteur paresseuse, au premier build).
		nkcode::NkEmbeddedJenga::Configure(exeDir);
		// ── DIAGNOSTIC de demarrage (panneau Sortie) ──
		// Les retours beta #9/#10 disaient « le Jenga inclus n'est pas fonctionnel »
		// sans qu'on puisse savoir POURQUOI a distance. Cette ligne rend l'etat
		// verifiable d'un coup d'oeil (et exploitable dans un rapport de bug) :
		// mode embarque actif, ou desactive avec la raison exacte.
		{
			const bool prod = nkcode::NkEmbeddedJenga::HasProdTools();
			const bool avail = nkcode::NkEmbeddedJenga::Available();
			g_state.output.PushBack(NkString("[nkcode] dossier de l'executable : ") +
									(exeDir.Empty() ? "(inconnu !)" : exeDir.CStr()));
			if (prod && avail)
				g_state.output.PushBack(
					NkString("[nkcode] Jenga EMBARQUE actif (aucun Python systeme requis)"));
			else if (!prod)
				g_state.output.PushBack(NkString("[nkcode] Jenga embarque INACTIF : tools/python-embed "
												 "ou tools/jenga-src/Jenga absent a cote de l'executable "
												 "-> repli sur le `jenga` du PATH"));
			else
				g_state.output.PushBack(NkString("[nkcode] Jenga embarque INACTIF : runtime present mais "
												 "non exploitable -> repli sur le `jenga` du PATH"));
		}
	}
	// Argument : un dossier de workspace -> ouvre directement (cas "nouvelle fenetre").
	// C'est AUSSI le chemin d'entree de l'explorateur de fichiers : le menu
	// contextuel « Ouvrir avec NKCode » lance simplement `NKCode.exe <dossier>`.
	bool g_openedArg = false;
	NkString g_openedPath; // chemin REELLEMENT ouvert -> inscrit dans le registre
	for (usize ai = 1; ai < state.args.Size(); ++ai) {
		const char *a = state.args[ai].CStr();
		if (a && a[0] && a[0] != '-') {
			g_dialogs.DoLoad(NkPath(a));
			g_openedArg = true;
			g_openedPath = a;
			break;
		}
	}
	const NkString g_startupMode = nkcode::NkOpenWsState::ReadNkSetting("openStartup");
	const NkString g_regHome = nkcode::NkOpenWsState::Home();
	// « Au demarrage = Toutes les fenetres precedentes » (openStartup==2).
	// Une fenetre = un processus : on reprend le premier workspace ICI et on
	// relance les autres, exactement comme le fait « Ouvrir dans une nouvelle
	// fenetre ». NkOpenWindowsTakeStale ne rend que les entrees dont le PID est
	// mort — une fenetre encore ouverte n'est donc jamais dupliquee.
	if (!g_openedArg && nkcode::StrEq(g_startupMode.CStr(), "2")) {
		NkVector<NkString> prev = nkcode::NkOpenWindowsTakeStale(g_regHome);
		if (!prev.Empty()) {
			g_dialogs.DoLoad(NkPath(prev[0].CStr()));
			g_openedArg = true;
			g_openedPath = prev[0];
			for (usize i = 1; i < prev.Size(); ++i)
				nkcode::NkHomeOpenNewWindow(g_home.exePath, prev[i]);
		}
	}
	// Parametre General « Au demarrage = Dernier workspace » (openStartup==1) :
	// ouvre directement le workspace recent le plus recent qui existe encore sur disque.
	if (!g_openedArg && nkcode::StrEq(g_startupMode.CStr(), "1")) {
		for (usize i = 0; i < g_state.recents.Size(); ++i) {
			const char *rp = g_state.recents[i].CStr();
			if (rp && rp[0] && (NkDirectory::Exists(rp) || NkFile::Exists(rp))) {
				g_dialogs.DoLoad(NkPath(rp));
				g_openedPath = rp;
				break;
			}
		}
	}
	// ── Registre des fenetres ouvertes (restauration au lancement suivant) ──
	// Inscription inconditionnelle : meme si l'option est desactivee aujourd'hui,
	// le registre doit refleter la realite, sinon l'activer plus tard ne
	// restaurerait rien de la session en cours.
	nkcode::NkOpenWindowsRegister(g_regHome, g_openedPath);
	{
		// Le rappel recoit le dossier personnel ; il doit survivre a main().
		static NkString s_regHome = g_regHome;
		shell->SetOnWindowClosed(
			[](void *user) -> bool {
				// On VETOE et on demande confirmation — TOUJOURS, pas seulement quand
				// des fichiers sont modifies : fermer l'IDE d'un clic sur la croix,
				// sans un mot, est trop facile a faire par accident. L'overlay
				// (Dialogs::DrawQuitConfirm) pose la question puis appelle
				// ConfirmQuit(), qui repasse ici avec quitConfirmed leve.
				if (!g_state.quitConfirmed) {
					g_state.quitRequested = true;
					return false;
				}
				return true;
			},
			&s_regHome);
		// Fermeture REELLE une fois l'utilisateur decide dans la confirmation :
		// desinscrit la fenetre (fermeture explicite -> pas de restauration au
		// prochain lancement) puis arrete la boucle.
		static NkEditorShell *s_shell = shell.Get();
		g_state.quitFnUser = &s_regHome;
		g_state.quitFn = [](void *user) {
			// Fermer CETTE fenetre (croix dessinee, « Fermer la fenetre ») = geste
			// explicite : elle ne doit PAS revenir au prochain lancement. Quitter
			// l'application (Quitter, Ctrl+Q) la laisse inscrite, pour reprendre ou
			// l'on en etait. La distinction vient de RequestQuit(windowClose).
			if (s_shell && s_shell->QuitIsWindowClose())
				nkcode::NkOpenWindowsUnregister(*static_cast<NkString *>(user));
			if (s_shell)
				s_shell->RequestClose();
		};
	}
	// Logos + icônes (table unique) + manifeste + activity bars — Shell/NkAppIcons.h
	nkcode::NkLoadAppIcons(shell.Get(), g_home, g_state);
	shell->SetStartScreen(&nkcode::StartScreenThunk, &g_home); // ecran de demarrage plein cadre (Home)
	// Fenetre large/centree mais NON maximisee -> redimensionnable par les bords des
	// le lancement (une fenetre maximisee n'est pas redimensionnable). L'utilisateur
	// peut maximiser via le bouton de la barre de titre ; l'etat projet le surchargera.
	// g_dialogs.showStart est vrai par defaut -> l'ecran de demarrage s'affiche au lancement.

	shell->RegisterCommand("Projet: Construire (jenga build)", &nkcode::CmdBuild, &g_state, "Ctrl+B");
	shell->RegisterCommand("Projet: Demarrer (jenga run)", &nkcode::CmdRun, &g_state, "Ctrl+R");
	shell->RegisterCommand("Fichier: Enregistrer", &nkcode::CmdSave, &g_state, "Ctrl+S");
	shell->RegisterCommand("Fichier: Enregistrer sous...", &nkcode::CmdSaveAs, &g_dialogs, "Ctrl+Shift+S");
	shell->RegisterCommand("Edition: Formater le document", &nkcode::CmdFormat, &g_state,
						   "Ctrl+Shift+I"); // Ctrl+L libéré pour « sélectionner la ligne » (éditeur)
	shell->RegisterCommand("Disposition: Reinitialiser", &nkcode::CmdResetLayout, shell.Get());
	shell->RegisterCommand("Affichage: Minimap (afficher/masquer)", &nkcode::CmdToggleMinimap, nullptr);
	shell->RegisterCommand("Affichage: Onglets multi-rangees", &nkcode::CmdToggleTabRows, nullptr);
	shell->RegisterCommand("Application: Quitter", &nkcode::CmdQuit, shell.Get(), "Ctrl+Q");
	shell->RegisterCommand("IA: Basculer le panneau Assistant", &nkcode::CmdToggleAiPanel, shell.Get(),
						   "Ctrl+Shift+A");

	const int rc = shell->Run();
	// Sauvegarde l'etat d'interface : par WORKSPACE si un projet est ouvert (dock +
	// geometrie fenetre + moniteur), sinon la geometrie GLOBALE du launcher.
	if (!g_dialogs.showStart && g_state.HasWorkspace())
		shell->SaveUiState(g_state.UiConfigPath().CStr());
	else
		shell->SaveWindowGeom(g_launcherGeom.CStr());
	// Jenga IN-PROCESS : join du worker + Py_FinalizeEx sur SON thread (no-op si
	// l'interpreteur n'a jamais ete initialise).
	nkcode::NkEmbeddedJenga::Shutdown();
	return rc;
}
