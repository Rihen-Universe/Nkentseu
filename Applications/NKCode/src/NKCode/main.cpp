// =============================================================================
// main.cpp — Point d'entree de NKCode (IDE type VSCode, base sur Jenga).
// Coquille = NKEditorKit (sur NKGui). Panneaux : Explorateur (fichiers reels),
// Editeur (onglets + saisie), Sortie (jenga build). Commandes : Construire /
// Executer (jenga) + Enregistrer. Le visuel/Blueprint/UIBuilder viendront ensuite.
// =============================================================================
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEditorKit/NkEditorKit.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKCode/Shell/Panels.h"
#include "NKCode/Shell/Toolbar.h"
#include "NKCode/Shell/Dialogs.h"
#include "NKCode/Shell/NkMenuBar.h" // barre de menus principale (spec Banani, remplace les menus shell)
#include "NKCode/Shell/ScaffoldPanels.h"
#include "NKCode/Shell/NkAiPanel.h"
#include "NKCode/Shell/NkHome.h"
#include "NKCode/Shell/NkAppFonts.h"
#include "NKCode/Shell/NkAppIcons.h"
#include "NKCode/Shell/NkAppCommands.h"
#include "NKCode/Project/NkLogSink.h"
#include "NKImage/NKImage.h"
#include "NKPlatform/NkEnv.h" // env::GetEnvVar (variables d'environnement maison)
#include "NKCode/Editor/NkSyntaxLangs.h" // coloration data-driven (CSS, JS, Lua…)

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


int nkmain(const NkEntryState &state) {
	(void)state;

	nkcode::InstallLogSink(); // capture les logs NKLogger -> panneau OUTPUT

	nkcode::NkSynInitDefaultLangs(); // coloration data-driven (CSS, JS, Lua, Rust…)
	nkcode::NkLoadFallbackFonts();	 // polices de repli (broad/CJK/emoji)

	auto shell = memory::NkMakeUnique<NkEditorShell>();
	NkEditorShellConfig cfg;
	cfg.title = "NKCode - IDE (Jenga)";
	cfg.width = 1440; // grande fenetre centree, REDIMENSIONNABLE (pas maximisee de force)
	cfg.height = 900;
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
	static nkcode::OutputPanel output(&g_state);
	static nkcode::TerminalPanel terminal;
	shell->AddPanel(&explorer);
	shell->AddPanel(&outline);
	shell->AddPanel(&editor);
	shell->AddPanel(&output);
	shell->AddPanel(&terminal);

	// Maquettes des interfaces (interface.md) : structure visuelle d'abord, rendu
	// fonctionnel ensuite (roadmap #2-#20). Fermees par defaut -> menu Affichage.
	using nkcode::ScaffoldPanel;
	namespace sc = nkcode::scaffold;
	static nkcode::SearchPanel pSearch(&g_state); // Recherche workspace FONCTIONNELLE (remplace la maquette #7)
	static ScaffoldPanel pProblem("Problemes", NkEditorDockSide::NK_BOTTOM, "Maquette - roadmap #8", sc::kProblems, 1);
	static ScaffoldPanel pGit("Controle de version", NkEditorDockSide::NK_LEFT, "Maquette - roadmap #9", sc::kGit, 3);
	static ScaffoldPanel pDebug("Debogueur", NkEditorDockSide::NK_LEFT, "Maquette - roadmap #10", sc::kDebug, 2);
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
	g_menuBar.exePath = (state.args.Size() > 0) ? state.args[0] : NkString(); // « Nouvelle fenetre »
	shell->SetMenuBar(&nkcode::MainMenuBarThunk, &g_menuBar);
	shell->SetOverlay(&nkcode::OverlayThunk, &g_dialogs);	// dialogues modaux (creation/enregistrement)

	// ── Ecran d'accueil (Home) : nouvelle UI + logos/icones rasterises en texture ──
	g_home.st = &g_state;
	g_home.dlg = &g_dialogs;
	g_home.exePath = (state.args.Size() > 0) ? state.args[0] : NkString(); // pour "Ouvrir dans une nouvelle fenetre"
	if (state.args.Size() > 0) {
		nkcode::NkOpenWsState::ExeDir() =
			NkPath(state.args[0].CStr()).GetParent().ToString(); // pour trouver le Jenga embarque (tools/jenga)
		// Jenga IN-PROCESS (Phase 12) : memorise les chemins tools/python-embed +
		// tools/jenga-src (init de l'interpreteur paresseuse, au premier build).
		nkcode::NkEmbeddedJenga::Configure(nkcode::NkOpenWsState::ExeDir());
	}
	// Argument : un dossier de workspace -> ouvre directement (cas "nouvelle fenetre").
	bool g_openedArg = false;
	for (usize ai = 1; ai < state.args.Size(); ++ai) {
		const char *a = state.args[ai].CStr();
		if (a && a[0] && a[0] != '-') {
			g_dialogs.DoLoad(NkPath(a));
			g_openedArg = true;
			break;
		}
	}
	// Parametre General « Au demarrage = Dernier workspace » (openStartup==1) :
	// ouvre directement le workspace recent le plus recent qui existe encore sur disque.
	if (!g_openedArg && nkcode::StrEq(nkcode::NkOpenWsState::ReadNkSetting("openStartup").CStr(), "1")) {
		for (usize i = 0; i < g_state.recents.Size(); ++i) {
			const char *rp = g_state.recents[i].CStr();
			if (rp && rp[0] && (NkDirectory::Exists(rp) || NkFile::Exists(rp))) {
				g_dialogs.DoLoad(NkPath(rp));
				break;
			}
		}
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
