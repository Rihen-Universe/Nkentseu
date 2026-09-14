// =============================================================================
// main.cpp — NkAnimaEditor : éditeur d'animation (timeline) sur NKEditorKit.
// L'app ne touche QUE l'Editor Kit + AnimBridge (pas NKRenderer directement, pour
// éviter le conflit de types NKRenderer/NKCanvas). L'anim vit dans AnimBridge.cpp.
// =============================================================================
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEditorKit/NkEditorKit.h"
#include "NKMemory/NkUniquePtr.h"
#include "AnimBridge.h"
#include "Panels.h"
#include "NkEditorRHIRenderer.h" // UI sur NKRHI/NKRenderer (pas NKCanvas)
#include <cstdio>  // traces de la MESURE (canal chrome) : couleur et geometrie reelles
#include <cstdlib> // getenv : negatif et choix de theme, dans le MEME binaire

using namespace nkentseu;
using namespace nkentseu::editorkit;

NKENTSEU_DEFINE_APP_DATA(([]() {
	NkAppData d{};
	d.appName = "NkAnimaEditor";
	d.appVersion = "0.1.0";
	return d;
})());

static void CmdUndo(void *) {
	nkanima::AnimUndo();
}

static void CmdRedo(void *) {
	nkanima::AnimRedo();
}

static void CmdInsert(void *) {
	nkanima::AnimInsertKeyAtCursor();
}

static void CmdQuit(void *u) {
	if (u)
		static_cast<NkEditorShell *>(u)->RequestClose();
}

// Hook pré-UI : rend le viewport 3D dans l'offscreen (device partagé) AVANT la passe
// UI, puis publie sa texture au backend NKGui pour AddImage. user = NkEditorRHIRenderer*.
static void PreUI3D(NkICommandBuffer *cmd, void *user) {
	auto *r = static_cast<nkanima::NkEditorRHIRenderer *>(user);
	nkanima::Anim3DRenderOffscreen(cmd);
	nkanima::Anim3DRegisterInto(&r->GetBackend(), nkanima::ANIM_VIEWPORT_TEXID);
}

int nkmain(const NkEntryState &state) {
	// Backend graphique depuis les args (-bgl défaut, -bvk, -bdx11, -bdx12, -bsw).
	// Tout argument SANS tiret est le chemin du modèle à charger (défaut CesiumMan)
	// — c'est ce qui permet de pointer un autre rig (XBot Mixamo…) sans recompiler.
	NkEditorGfxApi gfx = NkEditorGfxApi::OpenGL;
	const char *modelPath = "Resources/Models/CesiumMan/CesiumMan.glb";
	// ⚠️ ON SAUTE args[0] : C'EST L'IDENTITE DU PROGRAMME, PAS UN ARGUMENT.
	//    Sans ce saut, la clause « tout argument sans tiret est un chemin de
	//    modele » ci-dessous capturait le CHEMIN DE L'EXECUTABLE, et l'editeur
	//    tentait de charger son propre .exe comme fichier glTF. Le defaut
	//    CesiumMan n'etait donc JAMAIS atteint, et le viewport 3D restait vide
	//    a chaque lancement -- avec pour seule trace un [WRN] NkGLTFLoader et un
	//    [ERR] AnimBridge, qu'on pouvait prendre pour « pas encore implante ».
	//
	//    Mesure du 2026-09-14 : 21 sites du depot sautent ce premier element a la
	//    main, 7 y sont immunises parce qu'ils ne comparent qu'a des litteraux
	//    exacts, et CELUI-CI etait le seul a se tromper. `NkAudioPlayer` a le
	//    meme besoin -- un chemin de fichier libre -- et part de 1 lui aussi
	//    (main.cpp:105).
	const NkVector<NkString> &args = state.GetArgs();
	for (usize i = 1; i < args.Size(); ++i) {
		const NkString &a = args[i];
		if (a == "-bvk" || a == "--backend=vulkan")
			gfx = NkEditorGfxApi::Vulkan;
		else if (a == "-bdx11" || a == "--backend=dx11")
			gfx = NkEditorGfxApi::DX11;
		else if (a == "-bdx12" || a == "--backend=dx12")
			gfx = NkEditorGfxApi::DX12;
		else if (a == "-bsw" || a == "--backend=sw")
			gfx = NkEditorGfxApi::Software;
		else if (a == "-bgl" || a == "--backend=opengl")
			gfx = NkEditorGfxApi::OpenGL;
		else if (!a.Empty() && a.Data()[0] != '-')
			modelPath = a.CStr(); // les args vivent dans state : durée de vie OK
	}

	auto shell = memory::NkMakeUnique<NkEditorShell>();
	// Backend de rendu NKRHI/NKRenderer injecte (PAS NKCanvas) : l'UI NKGui et le
	// viewport 3D partageront ce device. Doit survivre au shell.
	static nkanima::NkEditorRHIRenderer rhi;
	NkEditorShellConfig cfg;
	cfg.title = "NkAnimaEditor — timeline (NkAnima M1.c)";
	cfg.width = 1280;
	cfg.height = 720;
	cfg.graphicsApi = gfx; // choisi par -b<backend> (OpenGL par défaut)
	cfg.renderer = &rhi;
	if (!shell || !shell->Init(cfg))
		return -1;

	// ── Thème « Dark Pro » épuré (cf interface.md §1 : fond #1a1a1a, accent cyan) ──
	{
		nkgui::NkGuiTheme t;
		t.bgPrimary = {26, 26, 28, 255}; // #1a1a1c quasi-noir
		t.panel = {30, 31, 34, 255};
		t.header = {34, 35, 39, 255};
		t.button = {40, 42, 47, 255};
		t.buttonHover = {52, 56, 64, 255};
		t.buttonActive = {0, 168, 208, 255}; // cyan profond
		t.border = {46, 48, 55, 255};
		t.text = {222, 226, 232, 255};
		t.textDisabled = {110, 114, 124, 255};
		t.selection = {0, 150, 190, 200};
		t.accent = {0, 212, 255, 255}; // #00d4ff cyan
		t.track = {24, 25, 28, 255};
		t.tabBar = {20, 20, 22, 255};
		t.tab = {28, 29, 32, 255};
		t.tabHover = {40, 42, 47, 255};
		t.tabActive = {34, 36, 41, 255};
		t.rounding = 4.f;
		t.framePadX = 12.f;
		t.framePadY = 7.f;
		shell->Ui().theme = t;
	}

	// ═══════════════════════════════════════════════════════════════════════
	//  L'HABILLAGE DE LA COQUILLE — ON APPELLE, ON NE REDESSINE PAS
	// ═══════════════════════════════════════════════════════════════════════
	//  Demande de Rodolf : NkAnimaEditor doit porter « exactement la meme
	//  interface » que NK3DModeler. Mesure prealable (canal chrome, lot 1) :
	//  cette application montait la coquille NUE -- AddPanel x2,
	//  RegisterCommand x4, et AUCUN des ~30 points d'extension de chrome.
	//
	//  LA COTE VIENT DE LA SOURCE, PAS D'UNE AUTRE APPLICATION :
	//  `NkModelerUI.h:113`, `NkLayout::Compute` -> `menuH = S(30.f)`
	//  (et `toolH = S(34.f)` en ligne 119).
	//
	//  ⚠️ LE SECOND PARAMETRE EST INERTE ICI, ET IL FAUT LE DIRE. La coquille
	//     ne reserve la bande d'outils que si l'application pose un
	//     `SetToolbar` : `toolbarH = (mToolbarFn && !fullScreen) ? bandH : 0.f`
	//     (NkEditorShell.cpp:831). NkAnimaEditor n'en pose pas. On passe donc
	//     34 pour rester fidele a la maquette le jour ou une barre d'outils
	//     arrivera -- mais aujourd'hui ce 34 ne peint rien, et l'ecrire sans le
	//     dire serait « declarer sans livrer ».
	//
	//  ⚠️ `NKANIMA_SANS_HABILLAGE=1` saute les deux appels : c'est LE NEGATIF,
	//     et il vit dans le MEME binaire. Comparer deux constructions aurait
	//     compare deux binaires differant peut-etre par autre chose.
	const bool sansHabillage = std::getenv("NKANIMA_SANS_HABILLAGE") != nullptr;
	if (!sansHabillage)
		shell->SetHeaderLayout(30.f, 34.f, 0.f);

	// ── LE THEME : DEUX CANDIDATS, ET C'EST RODOLF QUI TRANCHE ──────────────
	//  Le bloc ci-dessus pose un theme EN DUR (accent cyan #00d4ff, cf.
	//  interface.md §1). Il n'est PAS supprime : un theme en dur qu'on retire
	//  sans le dire, c'est une apparence qui change sans que personne sache
	//  pourquoi.
	//
	//  `ApplyTheme` est le point de synchronisation des deux objets theme
	//  (roles editeur -> jetons de dessin). NK3DModeler part de
	//  `NkTheme::Dark()` (NkModelerTheme.h:111) ; « exactement la meme
	//  interface » exige donc la meme source. Applique APRES le bloc en dur, il
	//  le remplace.
	//
	//  `NKANIMA_THEME=dur` rend le comportement d'avant, pour que les deux
	//  soient comparables cote a cote.
	{
		const char *th = std::getenv("NKANIMA_THEME");
		const bool garderDur = th && (th[0] == 'd' || th[0] == 'D');
		const bool clair = th && (th[0] == 'l' || th[0] == 'L');
		const nkgui::NkColor av = shell->Ui().theme.header;
		if (!sansHabillage && !garderDur)
			shell->ApplyTheme(clair ? NkTheme::Light() : NkTheme::Dark());
		const nkgui::NkColor ap = shell->Ui().theme.header;
		std::printf("[CHROME] theme=%s  header avant=(%d,%d,%d)  apres=(%d,%d,%d)  "
					"framePadY=%.1f\n",
					sansHabillage ? "SANS-HABILLAGE" : (garderDur ? "en dur" : (clair ? "Light" : "Dark")),
					(int)av.r, (int)av.g, (int)av.b, (int)ap.r, (int)ap.g, (int)ap.b,
					(double)shell->Ui().theme.framePadY);
		std::printf("[CHROME] ItemHeight=%.2f  scale=%.4f\n", (double)shell->Ui().ItemHeight(),
					(double)shell->Ui().scale);
		std::fflush(stdout);
	}

	nkanima::AnimInit(modelPath);

	// Viewport 3D : partage le device NKRHI de l'éditeur + rend en début de frame.
	nkanima::Anim3DSetSharedDevice(rhi.GetDevice());
	rhi.SetPreUI(&PreUI3D, &rhi);

	static nkanima::PreviewPanel preview;
	static nkanima::TimelinePanel timeline;
	shell->AddPanel(&preview);
	shell->AddPanel(&timeline);

	shell->RegisterCommand("Edition: Inserer cle", &CmdInsert, nullptr, "I");
	shell->RegisterCommand("Edition: Annuler", &CmdUndo, nullptr, "Ctrl+Z");
	shell->RegisterCommand("Edition: Refaire", &CmdRedo, nullptr, "Ctrl+Y");
	shell->RegisterCommand("Application: Quitter", &CmdQuit, shell.Get(), "Ctrl+Q");

	return shell->Run();
}
