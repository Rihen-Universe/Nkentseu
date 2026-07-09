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
#include "NKCode/Shell/ScaffoldPanels.h"
#include "NKCode/Shell/NkAiPanel.h"
#include "NKCode/Shell/NkHome.h"
#include "NKCode/Project/NkLogSink.h"
#include "NKImage/NKImage.h"

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

// ── Commandes (NkEditorCommandFn = void(*)(void*)) ──
static void CmdBuild(void *) {
	g_state.DoBuildAction("build");
}

static void CmdRun(void *) {
	g_state.DoRun();
}

static void CmdSave(void *user) {
	if (user)
		static_cast<nkcode::NkCodeState *>(user)->SaveActive();
}

static void CmdFormat(void *) { // Formater le document actif (C/C++)
	if (g_state.active >= 0 && g_state.active < static_cast<int32>(g_state.files.Size()))
		g_state.files[g_state.active].doc.FormatCpp();
}

static void CmdQuit(void *user) {
	if (user)
		static_cast<NkEditorShell *>(user)->RequestClose();
}

static void CmdResetLayout(void *u) {
	if (u)
		static_cast<NkEditorShell *>(u)->ResetLayout();
}

// ── Zoom PAR ONGLET : le shell route Ctrl+molette / Ctrl+± / Ctrl+0 ici. On ajuste la
//    taille PROPRE de l'onglet actif (0 = taille globale). L'atlas suit via RequestCodeSize.
struct ZoomCtx {
		nkcode::NkCodeState *st;
		NkEditorShell *shell;
};

static ZoomCtx g_zoomCtx;

static void ZoomHandler(void *u, nkentseu::float32 delta, bool reset) {
	auto *z = static_cast<ZoomCtx *>(u);
	if (!z || !z->st || !z->st->HasActive())
		return;
	auto &f = z->st->files[z->st->active];
	if (reset) {
		f.codeZoom = 0.f;
		return;
	}
	nkentseu::float32 s = (f.codeZoom > 0.f ? f.codeZoom : z->shell->CodeFontSize()) + delta;
	if (s < 8.f)
		s = 8.f;
	if (s > 40.f)
		s = 40.f;
	f.codeZoom = s;
}

// Barre d'outils Visual Studio (config/plateforme + Demarrer) -> delegue a NKCode.
static void ToolbarThunk(NkEditorFrameContext &ec, void *u) {
	nkcode::DrawCodeToolbar(ec, static_cast<nkcode::NkCodeState *>(u));
}

// Dialogues modaux (creation/enregistrement) + items du menu Fichier.
static nkcode::NkCodeDialogs g_dialogs;

static void FileMenuThunk(NkEditorFrameContext &ec, void *u) {
	nkcode::DrawFileMenu(ec, static_cast<nkcode::NkCodeDialogs *>(u));
}

static void OverlayThunk(NkEditorFrameContext &ec, void *u) {
	auto *d = static_cast<nkcode::NkCodeDialogs *>(u);
	// Demande d'« Enregistrer sous » (bouton +, ré-enregistrer un fichier supprimé) : ouvre le dialogue.
	if (d->st && d->st->reqSaveAs) {
		d->st->reqSaveAs = false;
		d->SaveActiveNative();
	}
	nkcode::DrawOverlay(ec, d);
}

// Ecran d'accueil (Home) — nouvelle UI propre (design Banani).
static nkcode::NkHomeState g_home;

static void StartScreenThunk(NkEditorFrameContext &ec, void *u) {
	nkcode::DrawHome(ec, static_cast<nkcode::NkHomeState *>(u));
}

// Pose appFullScreen/appModal CHAQUE FRAME (barre de menus, inconditionnel).
// + synchronise le thème de l'ÉDITORKIT sur le thème NKCode : le chrome de l'éditeur
//   (barre de titre, activity bar, onglets, dock, status bar, panneaux) suit Dark/Light.
static void AppFlagsThunk(NkEditorFrameContext &ec, void *u) {
	nkcode::DrawAppFlags(ec, static_cast<nkcode::NkCodeDialogs *>(u));
	if (!g_home.settings.loaded)
		g_home.settings.Load();
	nkcode::NkApplyEditorTheme(ec.Ui(), g_home.settings.theme, g_home.settings.accent);
}

int nkmain(const NkEntryState &state) {
	(void)state;

	nkcode::InstallLogSink(); // capture les logs NKLogger -> panneau OUTPUT

	// Polices de REPLI externes depuis le dossier data/fonts de NKCode (charge au
	// runtime, pas embarque) : tout glyphe absent d'Inter/DejaVu y est cherche.
	// Roles : broad (large couverture), cjk (ideogrammes, opt-in), emoji.
	{
		auto fileOk = [](const char *p) {
			std::FILE *f = std::fopen(p, "rb");
			if (f) {
				std::fclose(f);
				return true;
			}
			return false;
		};
		static const char *dirs[] = {"Applications/NKCode/data/fonts/", "data/fonts/", "NKCode/data/fonts/", ""};
		auto find = [&](const char *const *names, char *out, std::size_t cap) {
			out[0] = '\0';
			for (const char *const *np = names; *np; ++np)
				for (const char *const *dp = dirs;; ++dp) {
					std::snprintf(out, cap, "%s%s", *dp, *np);
					if (fileOk(out))
						return;
					if (!**dp)
						break;
				}
			out[0] = '\0';
		};
		static char broad[600], cjk[600], emoji[600];
		const char *broadN[] = {"NotoSans-Regular.ttf", nullptr};
		const char *cjkN[] = {"NotoSansSC-Regular.ttf", "NotoSansSC.ttf", "NotoSansCJKsc-Regular.otf", nullptr};
		const char *emojiN[] = {"NotoEmoji-Regular.ttf", nullptr};
		find(broadN, broad, sizeof(broad));
		find(cjkN, cjk, sizeof(cjk));
		find(emojiN, emoji, sizeof(emoji));
		nkgui::NkSetFallbackFontPaths(broad[0] ? broad : nullptr, cjk[0] ? cjk : nullptr, emoji[0] ? emoji : nullptr);
	}

	auto shell = memory::NkMakeUnique<NkEditorShell>();
	NkEditorShellConfig cfg;
	cfg.title = "NKCode - IDE (Jenga)";
	cfg.width = 1440; // grande fenetre centree, REDIMENSIONNABLE (pas maximisee de force)
	cfg.height = 900;
	if (!shell || !shell->Init(cfg))
		return -1;

	// Ouvre un fichier au demarrage (demo) : le README de NKCode.
	g_state.OpenPath(g_state.root / "README.md");

	static nkcode::ExplorerPanel explorer(&g_state);
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
	static ScaffoldPanel pSearch("Recherche", NkEditorDockSide::NK_LEFT, "Maquette - roadmap #7", sc::kSearch, 1);
	static ScaffoldPanel pProblem("Problemes", NkEditorDockSide::NK_BOTTOM, "Maquette - roadmap #8", sc::kProblems, 1);
	static ScaffoldPanel pGit("Controle de version", NkEditorDockSide::NK_LEFT, "Maquette - roadmap #9", sc::kGit, 3);
	static ScaffoldPanel pDebug("Debogueur", NkEditorDockSide::NK_LEFT, "Maquette - roadmap #10", sc::kDebug, 2);
	static ScaffoldPanel pBuild("Build & Taches", NkEditorDockSide::NK_BOTTOM, "Maquette - roadmap #14", sc::kBuild, 1);
	static ScaffoldPanel pProf("Profiler", NkEditorDockSide::NK_BOTTOM, "Maquette - roadmap #19", sc::kProfiler, 1);
	static nkcode::AiPanel aiPanel(&g_state); // Assistant IA FONCTIONNEL (remplace la maquette)
	static ScaffoldPanel pEngine("Moteur", NkEditorDockSide::NK_RIGHT, "Maquette - roadmap #17", sc::kEngine, 1);
	static ScaffoldPanel pExt("Extensions", NkEditorDockSide::NK_LEFT, "Maquette - roadmap #12", sc::kExtensions, 1);
	shell->AddPanel(&pSearch);
	shell->AddPanel(&pProblem);
	shell->AddPanel(&pGit);
	shell->AddPanel(&pDebug);
	shell->AddPanel(&pBuild);
	shell->AddPanel(&pProf);
	shell->AddPanel(&aiPanel);
	shell->AddPanel(&pEngine);
	shell->AddPanel(&pExt);

	shell->SetToolbar(&ToolbarThunk, &g_state); // barre d'outils Visual Studio
	g_zoomCtx = {&g_state, shell.Get()};
	shell->SetZoomHandler(&ZoomHandler, &g_zoomCtx); // zoom Ctrl+molette/±/0 -> onglet actif
	terminal.mShell = shell.Get();					 // police propre du terminal (non zoomee)
	g_dialogs.st = &g_state;
	g_dialogs.shell = shell.Get();
	g_state.LoadRecents();							// workspaces recents (ecran de demarrage)
	shell->SetAppMenu(&AppFlagsThunk, &g_dialogs);	// pose appFullScreen/appModal chaque frame
	shell->SetFileMenu(&FileMenuThunk, &g_dialogs); // items du menu Fichier (Nouveau/Enregistrer/Deploiement)
	shell->SetOverlay(&OverlayThunk, &g_dialogs);	// dialogues modaux (creation/enregistrement)

	// ── Ecran d'accueil (Home) : nouvelle UI + logos/icones rasterises en texture ──
	g_home.st = &g_state;
	g_home.dlg = &g_dialogs;
	g_home.exePath = (state.args.Size() > 0) ? state.args[0] : NkString(); // pour "Ouvrir dans une nouvelle fenetre"
	if (state.args.Size() > 0)
		nkcode::NkOpenWsState::ExeDir() =
			NkPath(state.args[0].CStr()).GetParent().ToString(); // pour trouver le Jenga embarque (tools/jenga)
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
	{
		// Charge une texture NETTE : PNG en priorite (repli SVG), puis REDIMENSIONNE
		// a ~ la taille d'affichage (tw x th) au filtre bilineaire. Sans mipmaps, une
		// texture bien plus grande que l'affichage est sous-echantillonnee (flou) ;
		// on l'amene donc proche de sa taille a l'ecran. `base` = nom sans extension.
		// box=true : letterbox dans un carre tw x th (icones -> taille uniforme).
		// box=false : upload a la taille ajustee fw x fh (wordmark -> ratio tight, sans bandes).
		auto upload = [&](NkImage &img, int32 tw, int32 th, int32 *outW, int32 *outH, bool box) -> uint32 {
			const int32 sw = img.Width(), sh = img.Height();
			if (sw <= 0 || sh <= 0)
				return 0;
			// Fit en PRESERVANT L'ASPECT (anti-deformation) dans tw x th.
			const float32 ar = (float32)sw / (float32)sh, tar = (float32)tw / (float32)th;
			int32 fw, fh;
			if (ar >= tar) {
				fw = tw;
				fh = (int32)((float32)tw / ar + 0.5f);
			} else {
				fh = th;
				fw = (int32)((float32)th * ar + 0.5f);
			}
			if (fw < 1)
				fw = 1;
			if (fh < 1)
				fh = 1;
			// Downscale PROGRESSIF par demi-pas : un bilinéaire direct 128->~35 ne prend que
			// 2x2 texels et CRÉNÈLE le line-art (pas de mipmaps). En halvant (128->64->35),
			// chaque étape moyenne 2x2 -> approxime un filtre surface -> icônes NETTES.
			NkImage *fitted;
			if (sw == fw && sh == fh) {
				fitted = &img;
			} else {
				NkImage *inter = nullptr;
				const NkImage *src = &img;
				int32 cw = sw, chh = sh;
				while (cw >= fw * 2 && chh >= fh * 2) {
					NkImage *half = src->Resize(cw / 2, chh / 2);
					if (!half)
						break;
					if (inter)
						inter->Free();
					inter = half;
					src = half;
					cw = half->Width();
					chh = half->Height();
				}
				fitted = src->Resize(fw, fh);
				if (inter)
					inter->Free();
				if (!fitted)
					fitted = &img;
			}
			uint32 id = 0;
			if (!box || (fw == tw && fh == th)) { // remplit la cible (ou pas de letterbox) -> direct
				id = shell->UploadRGBA(fitted->Pixels(), fw, fh);
				if (outW)
					*outW = fw;
				if (outH)
					*outH = fh;
			} else { // LETTERBOX dans un carre transparent
				NkImage *canvas = NkImage::Create((uint32)tw, (uint32)th, 4, 0u);
				if (canvas && canvas->IsValid()) {
					canvas->Blit(*fitted, (tw - fw) / 2, (th - fh) / 2);
					id = shell->UploadRGBA(canvas->Pixels(), tw, th);
					if (outW)
						*outW = tw;
					if (outH)
						*outH = th;
					canvas->Free();
				} else {
					id = shell->UploadRGBA(fitted->Pixels(), fw, fh);
					if (outW)
						*outW = fw;
					if (outH)
						*outH = fh;
				}
			}
			if (fitted != &img)
				fitted->Free();
			return id;
		};
		// Rogne les marges TRANSPARENTES (bounding box alpha) -> le glyphe remplit son
		// bitmap. Sans ca, une icone 128x128 avec grande marge interne parait plus PETITE
		// qu'une icone qui remplit son bitmap (ex. Ouvrir 40x32) -> tailles inegales.
		auto trimAlpha = [](NkImage &src) -> NkImage * {
			if (!src.IsValid() || src.Channels() < 4)
				return nullptr;
			const int32 w = src.Width(), h = src.Height(), ch = src.Channels();
			const uint8 *px = src.Pixels();
			if (!px)
				return nullptr;
			const usize stride = (usize)w * ch;
			int32 minX = w, minY = h, maxX = -1, maxY = -1;
			for (int32 yy = 0; yy < h; ++yy) {
				const uint8 *row = px + (usize)yy * stride;
				for (int32 xx = 0; xx < w; ++xx)
					if (row[(usize)xx * ch + 3] > 10) {
						if (xx < minX)
							minX = xx;
						if (xx > maxX)
							maxX = xx;
						if (yy < minY)
							minY = yy;
						if (yy > maxY)
							maxY = yy;
					}
			}
			if (maxX < minX || maxY < minY)
				return nullptr; // tout transparent
			if (minX == 0 && minY == 0 && maxX == w - 1 && maxY == h - 1)
				return nullptr; // deja bord-a-bord
			return src.Crop(minX, minY, maxX - minX + 1, maxY - minY + 1);
		};
		// Dossier d'OVERRIDE utilisateur (icones personnalisees, data-driven) : deposer un
		// PNG dans <ovrDir>icon/<Nom>.png remplace l'icone livree, sans recompiler.
		char ovrDir[512] = {};
		{
			const char *ad = std::getenv("APPDATA");
			const char *hm = std::getenv("HOME");
			if (ad && *ad)
				std::snprintf(ovrDir, sizeof(ovrDir), "%s/NKCode/", ad);
			else if (hm && *hm)
				std::snprintf(ovrDir, sizeof(ovrDir), "%s/.config/nkcode/", hm);
			else
				std::snprintf(ovrDir, sizeof(ovrDir), "Applications/NKCode/data/textures/");
		}
		auto loadTex = [&](const char *base, int32 tw, int32 th, int32 *outW = nullptr, int32 *outH = nullptr,
						   bool trim = true, bool box = true) -> uint32 {
			const char *dirs[] = {ovrDir, "Applications/NKCode/data/textures/", "data/textures/",
								  "NKCode/data/textures/", ""};
			char path[512];
			auto put = [&](NkImage &img) -> uint32 { // rogne (option) puis upload
				NkImage *t = trim ? trimAlpha(img) : nullptr;
				uint32 id = upload(t ? *t : img, tw, th, outW, outH, box);
				if (t)
					t->Free();
				return id;
			};
			for (const char *const *d = dirs;; ++d) {
				std::snprintf(path, sizeof(path), "%s%s.png", *d, base);
				{
					std::FILE *fp = std::fopen(path, "rb");
					if (fp) {
						std::fclose(fp);
						NkImage img;
						if (img.LoadFromFile(path) && img.IsValid())
							return put(img);
					}
				}
				std::snprintf(path, sizeof(path), "%s%s.svg", *d, base);
				{
					std::FILE *fp = std::fopen(path, "rb");
					if (fp) {
						std::fclose(fp);
						NkImage *im =
							NkSVGCodec::DecodeFromFile(path, tw * 2, th * 2); // rasterise large puis reduit = net
						if (im && im->IsValid()) {
							uint32 id = put(*im);
							im->Free();
							return id;
						}
						if (im)
							im->Free();
					}
				}
				if (!**d)
					break;
			}
			return 0;
		};
		// Logos. Le wordmark COMPLET contient "nkcode" + sous-titre "INTELLIGENT IDE" :
		// illisible/flou s'il est reduit a la taille minuscule de la barre de titre. On
		// pre-redimensionne donc le wordmark cote CPU (filtre qualite) a ~ sa taille
		// d'affichage sidebar -> NET (sans mipmaps, uploader 512px puis laisser le GPU
		// sous-echantillonner produit du flou). La barre de titre utilise l'ICONE (nette)
		// + "nkcode" en police vectorielle (toujours net), conforme a la maquette.
		g_home.logoIcon = loadTex("logo/nkcode_icon", 48, 48); // icone barre de titre (elle seule, sans texte)
		// Wordmark en 2 versions PRETES : nkcode_white (fond sombre) + nkcode_dark (fond clair / theme Light).
		g_home.logoWord =
			loadTex("logo/nkcode_white", 360, 90, &g_home.wordW, &g_home.wordH, /*trim*/ true, /*box*/ false);
		g_home.logoWordDark =
			loadTex("logo/nkcode_dark", 360, 90, &g_home.wordWD, &g_home.wordHD, /*trim*/ true, /*box*/ false);
		shell->SetTitleLogo(g_home.logoIcon, 1.0f); // aspect>0 => icone carree SEULE (pas de texte "nkcode")
		// Icones : uploadees a la taille d'AFFICHAGE (DPI-aware) -> NET. Sans mipmaps,
		// uploader plus grand que l'ecran puis laisser le GPU reduire = flou. On
		// redimensionne donc la source 128px directement a ~ sa taille ecran (CPU, filtre
		// qualite). Les icones s'affichent ~13-22 px logiques -> upload ~26 px * DPI.
		const float32 dpi = shell->DpiScale();
		int32 IS = (int32)(32.f * dpi + 0.5f);
		if (IS < 24)
			IS = 24; // source 128px -> downscale progressif net
		nkcode::NkIcons &ic = g_home.icons;
		ic.accueil = loadTex("icon/Accueil", IS, IS);
		ic.ouvrir = loadTex("icon/Ouvrir", IS, IS);
		ic.ouvrirDossier = loadTex("icon/OuvrirUnDossier", IS, IS);
		ic.nouveau = loadTex("icon/Nouveau", IS, IS);
		ic.cloner = loadTex("icon/Cloner", IS, IS);
		ic.toolchains = loadTex("icon/Toolchains", IS, IS);
		ic.platforms = loadTex("icon/Platforms", IS, IS);
		ic.gear = loadTex("icon/Gear", IS, IS);
		ic.exemple = loadTex("icon/Exemple", IS, IS);
		ic.star = loadTex("icon/Star", IS, IS);
		ic.search = loadTex("icon/Search", IS, IS);
		ic.workspace = loadTex("logo/workspace", IS, IS); // workspace.png est dans logo/
		// Navigateur « Ouvrir un Workspace »
		ic.back = loadTex("icon/LeftArrow", IS, IS);
		ic.forward = loadTex("icon/RightArrow", IS, IS);
		ic.up = loadTex("icon/UpArrow", IS, IS);
		ic.downArrow = loadTex("icon/DownArrow", IS, IS);
		ic.bureau = loadTex("icon/Bureau", IS, IS);
		ic.disque = loadTex("icon/Disque", IS, IS);
		ic.jenga = loadTex("icon/Jenga", IS, IS);
		ic.valide = loadTex("icon/Valide", IS, IS);
		ic.horloge = loadTex("icon/Horloge", IS, IS);
		ic.fichier = loadTex("icon/Fichier", IS, IS);
		ic.sort = loadTex("icon/Sort", IS, IS);
		// Wizard projet : types + actions + validation
		ic.kConsole = loadTex("icon/consoleapp", IS, IS);
		ic.kWindowed = loadTex("icon/windowedapp", IS, IS);
		ic.kStatic = loadTex("icon/staticlib", IS, IS);
		ic.kShared = loadTex("icon/sharedlib", IS, IS);
		ic.kTest = loadTex("icon/test", IS, IS);
		ic.kConfig = loadTex("icon/config", IS, IS);
		ic.valideSimple = loadTex("icon/ValideSimple", IS, IS);
		ic.editer = loadTex("icon/editer", IS, IS);
		ic.dependance = loadTex("icon/Dependance", IS, IS);
		ic.creeProjet = loadTex("icon/CreeProjet", IS, IS);
		ic.fileCode = loadTex("icon/FileCode", IS, IS);
		ic.plus = loadTex("icon/Plus", IS, IS);
		ic.corbeille = loadTex("icon/Corbeil", IS, IS);
		ic.lock = loadTex("icon/Lock", IS, IS);
		ic.clonerTel = loadTex("icon/ClonerTelecharger", IS, IS);
		ic.github = loadTex("icon/Github", IS, IS);
		ic.oeilOuvert = loadTex("icon/OeilOuvert", IS, IS);
		ic.oeilFermer = loadTex("icon/OeilFermer", IS, IS);
		ic.rondI = loadTex("icon/RondI", IS, IS);
		// ── Vue principale IDE : vraies icones (Lucide -> assets reels) ──
		ic.hammer = loadTex("icon/Mateau", IS, IS);			 // build
		ic.bug = loadTex("icon/cocsinelle", IS, IS);		 // debug
		ic.sparkles = loadTex("icon/EtoileEtoile", IS, IS);	 // IA
		ic.zap = loadTex("icon/Eclaire", IS, IS);			 // moteur
		ic.chart = loadTex("icon/gRAPH", IS, IS);			 // profiler
		ic.puzzle = loadTex("icon/stack", IS, IS);			 // extensions
		ic.eraser = loadTex("icon/Gomme", IS, IS);			 // nettoyer
		ic.rebuild = loadTex("icon/circletwoarrow", IS, IS); // rebuild
		ic.play = loadTex("icon/Play", IS, IS);				 // executer
		ic.monitor = loadTex("icon/Ordinateur", IS, IS);	 // plateforme
		ic.flask = loadTex("icon/test", IS, IS);			 // tests
		ic.layers = loadTex("icon/Piles", IS, IS);			 // solution
		ic.pkg = loadTex("icon/stack", IS, IS);				 // projet
		ic.globe = loadTex("icon/Globe", IS, IS);			 // web
		ic.pause = loadTex("icon/Pause", IS, IS);
		ic.stop = loadTex("icon/Stop", IS, IS);
		ic.gitPush = loadTex("icon/Push", IS, IS);
		ic.gitPull = loadTex("icon/Pull", IS, IS);
		ic.split = loadTex("icon/Split", IS, IS);
		ic.folderOpen = loadTex("icon/FolderOpen", IS, IS);
		ic.fileText = loadTex("icon/FileText", IS, IS);
		ic.fileCode2 = loadTex("icon/FileCode2", IS, IS);
		ic.filePlus = loadTex("icon/FilePlus", IS, IS);
		ic.code = loadTex("icon/Code", IS, IS);
		ic.compare = loadTex("icon/Compare", IS, IS);
		ic.blame = loadTex("icon/Blame", IS, IS);
		ic.exit = loadTex("icon/Exit", IS, IS);
		ic.tags = loadTex("icon/Tags", IS, IS);
		ic.cloud = loadTex("icon/Cloud", IS, IS);
		ic.docker = loadTex("icon/docker", IS, IS);
		ic.linux = loadTex("icon/Linux", IS, IS);
		g_state.icons = &g_home.icons; // rend les icones accessibles aux panneaux/toolbar (via l'etat)
		// toggle liste/grille : pas d'asset adapte (`<>` et `↕` ne conviennent pas) -> dessine.

		// ── Registre d'extensions DATA-DRIVEN (icons.cfg) : .ext -> icone ──
		{
			NkVector<NkString> stemName;
			NkVector<uint32> stemTex; // cache stem -> texture (evite re-upload)
			auto loadStem = [&](const NkString &stem) -> uint32 {
				for (usize i = 0; i < stemName.Size(); ++i)
					if (stemName[i] == stem)
						return stemTex[i];
				const NkString base = NkString("icon/") + stem.CStr();
				const uint32 t = loadTex(base.CStr(), IS, IS);
				stemName.PushBack(stem);
				stemTex.PushBack(t);
				return t;
			};
			auto trim = [](NkString s) -> NkString {
				int32 a = 0, b = (int32)s.Length();
				const char *d = s.CStr();
				while (a < b && (d[a] == ' ' || d[a] == '\t'))
					++a;
				while (b > a && (d[b - 1] == ' ' || d[b - 1] == '\t' || d[b - 1] == '\r'))
					--b;
				return s.SubStr(a, b - a);
			};
			auto applyManifest = [&](const NkString &text) {
				const char *p = text.CStr();
				NkString line;
				auto flush = [&]() {
					NkString l = trim(line);
					if (!l.Empty() && l.CStr()[0] != '#') {
						int32 eq = -1;
						const char *d = l.CStr();
						for (int32 k = 0; d[k]; ++k)
							if (d[k] == '=') {
								eq = k;
								break;
							}
						if (eq > 0) {
							const NkString key = trim(l.SubStr(0, eq));
							const NkString val = trim(l.SubStr(eq + 1, l.Length() - eq - 1));
							if (!key.Empty() && key.CStr()[0] == '.' && !val.Empty())
								ic.SetExt(key.CStr(), loadStem(val));
						}
					}
					line.Clear();
				};
				for (;; ++p) {
					if (*p == '\n' || *p == '\0') {
						flush();
						if (*p == '\0')
							break;
					} else
						line += *p;
				}
			};
			// 1) defauts integres (au cas ou aucun manifeste n'est present) ;
			applyManifest(NkString(".cpp=Cpp\n.cc=Cpp\n.h=Header\n.hpp=Header\n.c=C\n.py=Python\n.rs=Rust\n.zig=Zig\n."
								   "jenga=Jenga\n.md=Markdown\n.txt=Texte\n.json=Json\n.png=Image\n.jpg=Image\n.zip="
								   "Archive\n.exe=Binaire\n.dll=Binaire\n"));
			// 2) manifeste livre ; 3) override utilisateur (applique en dernier -> gagne).
			for (const char *mp : {"Applications/NKCode/data/icons.cfg", "data/icons.cfg"})
				if (NkFile::Exists(mp)) {
					applyManifest(NkFile::ReadAllText(NkPath(mp)));
					break;
				}
			{
				const NkString uman = NkString(ovrDir) + "icons.cfg";
				if (NkFile::Exists(uman.CStr()))
					applyManifest(NkFile::ReadAllText(NkPath(uman.CStr())));
			}
		}
	}
	shell->SetStartScreen(&StartScreenThunk, &g_home); // ecran de demarrage plein cadre (Home)
	// Fenetre large/centree mais NON maximisee -> redimensionnable par les bords des
	// le lancement (une fenetre maximisee n'est pas redimensionnable). L'utilisateur
	// peut maximiser via le bouton de la barre de titre ; l'etat projet le surchargera.
	// g_dialogs.showStart est vrai par defaut -> l'ecran de demarrage s'affiche au lancement.

	shell->RegisterCommand("Projet: Construire (jenga build)", &CmdBuild, nullptr, "Ctrl+B");
	shell->RegisterCommand("Projet: Demarrer (jenga run)", &CmdRun, nullptr, "Ctrl+R");
	shell->RegisterCommand("Fichier: Enregistrer", &CmdSave, &g_state, "Ctrl+S");
	shell->RegisterCommand("Edition: Formater le document", &CmdFormat, nullptr,
						   "Ctrl+Shift+I"); // Ctrl+L libéré pour « sélectionner la ligne » (éditeur)
	shell->RegisterCommand("Disposition: Reinitialiser", &CmdResetLayout, shell.Get());
	shell->RegisterCommand("Application: Quitter", &CmdQuit, shell.Get(), "Ctrl+Q");

	const int rc = shell->Run();
	// Sauvegarde l'etat d'interface du projet courant (maximise + panneaux ouverts).
	if (!g_dialogs.showStart && g_state.HasWorkspace())
		shell->SaveUiState(g_state.UiConfigPath().CStr());
	return rc;
}
