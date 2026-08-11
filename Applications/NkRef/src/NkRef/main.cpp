// ============================================================================
// main.cpp — NkRef, Étape 1 : des IMAGES sur la planche.
// ----------------------------------------------------------------------------
// GLUE fenêtre + boucle + entrées + rendu. Le modèle vit dans NkRefBoard.h
// (pur), la caméra dans NkRefView.h (pure) : c'est eux qu'on garde.
//
// Ce que fait l'Étape 1 (et rien de plus) :
//   - glisser-déposer des fichiers image → NkSprite posé au point de dépôt ;
//   - Ctrl+V : colle l'image du presse-papiers (GetClipboardImage, chantier
//     NKWindow de cette branche) sous le curseur ;
//   - clic = sélection (Ctrl = ajouter), glisser = déplacer la sélection,
//     glisser sur le vide = rectangle de sélection ;
//   - poignées d'échelle aux coins (ancre = coin opposé, échelle UNIFORME),
//     poignée de rotation au-dessus (Maj = crans de 15°) ;
//   - X / Y = miroir, Suppr = supprimer, PgUp/PgDn ou Ctrl+molette = ordre Z ;
//   - pan (clic milieu, Espace+glisser), zoom molette centré curseur, grille
//     adaptative, Début = origine — l'Étape 0 inchangée.
//
// Rendu en ESPACE ÉCRAN (la vue transforme elle-même, pas de SetView) : les
// sprites reçoivent position/échelle/rotation en pixels — un seul chemin de
// transformation, vérifiable au pixel sur les captures.
//
// L'ordre du tableau d'items EST l'ordre de profondeur ; la glue tient un
// tableau de NkTexture* ALIGNÉ SUR LES INDEX — toute opération qui réordonne
// ou supprime applique le même mouvement aux deux (cf. NkRefBoard.h).
// ============================================================================

#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEvent/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkDropEvent.h"
#include "NKLogger/NkLog.h"
#include "NKTime/NkTime.h"

#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkGraphicsApi.h"
#include "NKCanvas/Renderer/Resources/NkTexture.h"
#include "NKCanvas/Renderer/Resources/NkSprite.h" // contient aussi NkText
#include "NKCanvas/Renderer/Resources/NkFont.h"

// Captures agent : dossier + numérotation + photographie de la fenêtre.
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKImage/NKImage.h"
#if defined(NKENTSEU_PLATFORM_WINDOWS)
#include <windows.h>
#endif

// Le panneau de propriétés escamotable : NKGui (immediate mode) + son pont
// canvas header-only. NKGui.h inclut NkX11Clean en tête (macros Xlib) — le
// laisser APRÈS les includes fenêtre/canvas, jamais avant un X11 « sale ».
#include "NKGui/NKGui.h"
#include "NKCanvas/UI/NkGuiCanvasBackend.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKTime/NkClock.h"

#include "NkRef/NkRefView.h"
#include "NkRef/NkRefBoard.h"

#include <cstdio>  // snprintf (chemins, titre — glue, OK)
#include <cstdlib> // getenv/atoi (crochets d'agent, comme le modeleur)
#include <cstring> // strchr/strncpy (parsing des crochets)

using namespace nkentseu;
using namespace nkentseu::renderer;

namespace {

	// ── CAPTURES ────────────────────────────────────────────────────────────
	// Premier chemin LIBRE captures/<prefixe>_NNN.png : numérotation simple,
	// sans horloge — l'ordre des fichiers EST l'ordre des prises (même
	// convention que le modeleur).
	bool NkNextCapturePath(const char *prefix, char *out, int32 cap) {
		NkDirectory::CreateRecursive("captures");
		for (int32 i = 1; i < 1000; ++i) {
			std::snprintf(out, (size_t)cap, "captures/%s_%03d.png", prefix, (int)i);
			if (!NkFile::Exists(out))
				return true;
		}
		return false;
	}

#if defined(NKENTSEU_PLATFORM_WINDOWS)
	// Photographie de TOUTE la fenêtre par l'OS (PrintWindow) : indépendante du
	// backend graphique — NkRenderWindow::Capture ne lit que DX11 et nous
	// tournons en OpenGL. Recopié du modeleur (le chemin qui a fait ses preuves).
	bool NkCaptureWholeWindow(NkWindow &win, const char *path) {
		const NkSurfaceDesc sd = win.GetSurfaceDesc();
		HWND hwnd = sd.hwnd;
		if (!hwnd)
			return false;
		RECT rc{};
		if (!GetWindowRect(hwnd, &rc))
			return false;
		const int32 w = rc.right - rc.left, h = rc.bottom - rc.top;
		if (w <= 0 || h <= 0)
			return false;
		HDC hdcWin = GetWindowDC(hwnd);
		HDC hdcMem = CreateCompatibleDC(hdcWin);
		BITMAPINFO bi{};
		bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bi.bmiHeader.biWidth = w;
		bi.bmiHeader.biHeight = -h; // négatif = origine en HAUT (ordre des lignes PNG)
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biCompression = BI_RGB;
		void *bits = nullptr;
		HBITMAP hbmp = CreateDIBSection(hdcMem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
		bool ok = false;
		if (hbmp) {
			HGDIOBJ old = SelectObject(hdcMem, hbmp);
			ok = PrintWindow(hwnd, hdcMem, 2 /*PW_RENDERFULLCONTENT*/) != 0;
			if (!ok)
				ok = BitBlt(hdcMem, 0, 0, w, h, hdcWin, 0, 0, SRCCOPY | CAPTUREBLT) != 0;
			if (ok && bits) {
				NkImage img;
				ok = img.Create((uint32)w, (uint32)h, math::NkColor(0, 0, 0, 255), 4);
				if (ok) {
					const uint8 *src = (const uint8 *)bits;
					uint8 *dst = img.Pixels();
					for (int32 i = 0; i < w * h; ++i) { // BGRA -> RGBA, alpha opaque
						dst[i * 4 + 0] = src[i * 4 + 2];
						dst[i * 4 + 1] = src[i * 4 + 1];
						dst[i * 4 + 2] = src[i * 4 + 0];
						dst[i * 4 + 3] = 255;
					}
					ok = img.Save(path);
				}
			}
			SelectObject(hdcMem, old);
			DeleteObject(hbmp);
		}
		DeleteDC(hdcMem);
		ReleaseDC(hwnd, hdcWin);
		return ok;
	}
#else
	bool NkCaptureWholeWindow(NkWindow &, const char *) {
		std::printf("[NkRef] capture : pas encore portee sur cette plateforme\n");
		return false;
	}
#endif

	// ── CROCHETS D'AGENT ────────────────────────────────────────────────────
	// Même doctrine que le modeleur : les crochets n'ARMENT que ce que
	// l'interface arme déjà — DROP/CLICK/MOVE appellent les MÊMES fonctions
	// que le glisser-déposer et la souris. Si personne ne les pose, rien ne
	// change.
	//   NK_AGENT_SHOT="n[,n...]"                  captures aux frames n…
	//   NK_AGENT_EXIT=n                           sortie propre à la frame >= n
	//   NK_AGENT_PAN="n:dx:dy"                    pan de (dx,dy) pixels
	//   NK_AGENT_ZOOM="n:facteur:px:py"           zoom au pixel (px,py)
	//   NK_AGENT_DROP="n:px:py:chemin[;...]"      dépose une image au pixel
	//   NK_AGENT_CLICK="n:px:py[;...]"            clic gauche de sélection
	//   NK_AGENT_MOVE="n:dx:dy"                   déplace la sélection (pixels)
	struct NkAgentHooks {
			static constexpr int32 kMax = 8;
			int32 shotFrames[kMax] = {};
			int32 shotCount = 0;
			int32 exitFrame = -1;
			int32 panFrame = -1;
			float32 panDx = 0.0f, panDy = 0.0f;
			int32 zoomFrame = -1;
			float32 zoomFactor = 1.0f, zoomPx = 0.0f, zoomPy = 0.0f;

			struct Drop {
					int32 frame = -1;
					float32 px = 0.0f, py = 0.0f;
					char path[260] = {};
			};
			Drop drops[kMax];
			int32 dropCount = 0;

			struct Click {
					int32 frame = -1;
					float32 px = 0.0f, py = 0.0f;
			};
			Click clicks[kMax];
			int32 clickCount = 0;

			int32 moveFrame = -1;
			float32 moveDx = 0.0f, moveDy = 0.0f;
			int32 packFrame = -1;  ///< NK_AGENT_PACK=n : Pack à la frame n
			int32 panelFrame = -1; ///< NK_AGENT_PANEL=n : ouvre le tiroir à la frame n
			int32 themeFrame = -1; ///< NK_AGENT_THEME="n:0|1" : bascule le thème (1=sombre)
			bool themeDark = true;

			void Read() {
				if (const char *v = std::getenv("NK_AGENT_SHOT")) {
					const char *p = v;
					while (*p && shotCount < kMax) {
						shotFrames[shotCount++] = (int32)std::atoi(p);
						while (*p && *p != ',')
							++p;
						if (*p == ',')
							++p;
					}
				}
				if (const char *v = std::getenv("NK_AGENT_EXIT"))
					exitFrame = (int32)std::atoi(v);
				if (const char *v = std::getenv("NK_AGENT_PAN")) {
					int f = 0;
					float dx = 0, dy = 0;
					if (std::sscanf(v, "%d:%f:%f", &f, &dx, &dy) == 3) {
						panFrame = (int32)f;
						panDx = (float32)dx;
						panDy = (float32)dy;
					}
				}
				if (const char *v = std::getenv("NK_AGENT_ZOOM")) {
					int f = 0;
					float fac = 1, px = 0, py = 0;
					if (std::sscanf(v, "%d:%f:%f:%f", &f, &fac, &px, &py) == 4) {
						zoomFrame = (int32)f;
						zoomFactor = (float32)fac;
						zoomPx = (float32)px;
						zoomPy = (float32)py;
					}
				}
				// Le CHEMIN peut contenir des ':' (D:\...) : on coupe NOUS-MÊMES
				// après le 3e deux-points, sscanf s'y tromperait.
				if (const char *v = std::getenv("NK_AGENT_DROP")) {
					const char *tok = v;
					while (tok && *tok && dropCount < kMax) {
						const char *end = std::strchr(tok, ';');
						Drop &d = drops[dropCount];
						int f = 0;
						float px = 0, py = 0;
						const char *c1 = std::strchr(tok, ':');
						const char *c2 = c1 ? std::strchr(c1 + 1, ':') : nullptr;
						const char *c3 = c2 ? std::strchr(c2 + 1, ':') : nullptr;
						if (c3 && std::sscanf(tok, "%d:%f:%f", &f, &px, &py) == 3) {
							d.frame = (int32)f;
							d.px = (float32)px;
							d.py = (float32)py;
							const char *p0 = c3 + 1;
							const usize len = end ? (usize)(end - p0) : std::strlen(p0);
							const usize n = len < sizeof(d.path) - 1 ? len : sizeof(d.path) - 1;
							std::memcpy(d.path, p0, n);
							d.path[n] = '\0';
							++dropCount;
						}
						tok = end ? end + 1 : nullptr;
					}
				}
				if (const char *v = std::getenv("NK_AGENT_CLICK")) {
					const char *tok = v;
					while (tok && *tok && clickCount < kMax) {
						const char *end = std::strchr(tok, ';');
						int f = 0;
						float px = 0, py = 0;
						if (std::sscanf(tok, "%d:%f:%f", &f, &px, &py) == 3) {
							clicks[clickCount].frame = (int32)f;
							clicks[clickCount].px = (float32)px;
							clicks[clickCount].py = (float32)py;
							++clickCount;
						}
						tok = end ? end + 1 : nullptr;
					}
				}
				if (const char *v = std::getenv("NK_AGENT_MOVE")) {
					int f = 0;
					float dx = 0, dy = 0;
					if (std::sscanf(v, "%d:%f:%f", &f, &dx, &dy) == 3) {
						moveFrame = (int32)f;
						moveDx = (float32)dx;
						moveDy = (float32)dy;
					}
				}
				if (const char *v = std::getenv("NK_AGENT_PACK"))
					packFrame = (int32)std::atoi(v);
				if (const char *v = std::getenv("NK_AGENT_PANEL"))
					panelFrame = (int32)std::atoi(v);
				if (const char *v = std::getenv("NK_AGENT_THEME")) {
					int f = 0, dark = 1;
					if (std::sscanf(v, "%d:%d", &f, &dark) == 2) {
						themeFrame = (int32)f;
						themeDark = dark != 0;
					}
				}
			}
	};

	// Ce que la souris est en train de faire. Un seul mode à la fois : c'est
	// ce qui rend le canevas prévisible (pas de drag+zoom+rect simultanés).
	enum class NkMode { Idle, Pan, DragItems, ScaleCorner, Rotate, RectSelect, Draw };

	// ── Thèmes (demande Rihen : un sombre ET un clair) ──────────────────────
	// Toutes les couleurs de l'app (canevas, grille, en-tête, sélection) dans
	// UNE struct — deux presets. Le panneau NKGui a sa bascule au même moment.
	// L'orange #F79A28 et le pétrole #0A555F de la charte tiennent les deux rôles.
	struct NkRefColors {
			NkColor2D clear, gridMinor, gridMajor, axis;
			NkColor2D barBg, barLine, barText, barGlyph, barHover, barClose;
			NkColor2D tabBg, tabGlyph;
			NkColor2D selected, active, rectFill, rectRim;
	};

	NkRefColors MakeDarkColors() {
		NkRefColors c;
		c.clear = {20, 22, 25, 255};
		c.gridMinor = {255, 255, 255, 10};
		c.gridMajor = {255, 255, 255, 26};
		c.axis = {247, 154, 40, 70};
		c.barBg = {14, 15, 17, 236};
		c.barLine = {255, 255, 255, 22};
		c.barText = {205, 210, 216, 255};
		c.barGlyph = {215, 220, 226, 255};
		c.barHover = {255, 255, 255, 26};
		c.barClose = {200, 48, 44, 230};
		c.tabBg = {10, 85, 95, 235};
		c.tabGlyph = {240, 244, 247, 255};
		c.selected = {46, 140, 153, 255};
		c.active = {247, 154, 40, 255};
		c.rectFill = {46, 140, 153, 30};
		c.rectRim = {46, 140, 153, 180};
		return c;
	}

	NkRefColors MakeLightColors() {
		NkRefColors c;
		c.clear = {236, 238, 240, 255};
		c.gridMinor = {0, 0, 0, 12};
		c.gridMajor = {0, 0, 0, 30};
		c.axis = {235, 130, 12, 110};
		c.barBg = {246, 247, 249, 242};
		c.barLine = {0, 0, 0, 30};
		c.barText = {32, 38, 44, 255};
		c.barGlyph = {40, 48, 56, 255};
		c.barHover = {0, 0, 0, 20};
		c.barClose = {210, 55, 50, 235};
		c.tabBg = {10, 85, 95, 235}; // le pétrole reste lisible sur clair
		c.tabGlyph = {240, 244, 247, 255};
		c.selected = {12, 105, 118, 255};
		c.active = {225, 122, 8, 255};
		c.rectFill = {12, 105, 118, 26};
		c.rectRim = {12, 105, 118, 170};
		return c;
	}

	// Le thème NKGui assorti (le panneau doit suivre le canevas).
	void ApplyGuiTheme(nkgui::NkGuiContext &gui, bool dark) {
		if (dark) {
			gui.theme.bgPrimary = {16, 18, 22, 255};
			gui.theme.panel = {21, 25, 30, 246};
			gui.theme.header = {10, 60, 68, 255};
			gui.theme.button = {12, 72, 82, 255};
			gui.theme.buttonHover = {10, 85, 95, 255};
			gui.theme.buttonActive = {247, 154, 40, 255};
			gui.theme.border = {10, 85, 95, 200};
			gui.theme.text = {228, 233, 236, 255};
			gui.theme.textDisabled = {130, 136, 142, 255};
			gui.theme.selection = {10, 85, 95, 235};
			gui.theme.accent = {247, 154, 40, 255};
			gui.theme.track = {13, 16, 20, 255};
		} else {
			gui.theme.bgPrimary = {240, 242, 244, 255};
			gui.theme.panel = {248, 249, 251, 248};
			gui.theme.header = {205, 226, 230, 255}; // pétrole pâle
			gui.theme.button = {222, 226, 230, 255};
			gui.theme.buttonHover = {186, 214, 219, 255};
			gui.theme.buttonActive = {247, 154, 40, 255};
			gui.theme.border = {150, 168, 172, 220};
			gui.theme.text = {28, 33, 38, 255};
			gui.theme.textDisabled = {130, 138, 144, 255};
			gui.theme.selection = {166, 205, 211, 235};
			gui.theme.accent = {225, 122, 8, 255};
			gui.theme.track = {214, 218, 222, 255};
		}
		gui.theme.rounding = 6.0f;
	}

} // namespace

NKENTSEU_DEFINE_APP_DATA(([]() {
	NkAppData d{};
	d.appName = "NkRef";
	d.appVersion = "0.1.0";
	return d;
})());

int nkmain(const NkEntryState &state) {
	(void)state;

	NkWindowConfig cfg;
	cfg.title = "NkRef";
	cfg.width = 1280;
	cfg.height = 800;
	cfg.centered = true;
	cfg.resizable = true;
	cfg.dropEnabled = true; // glisser-déposer OLE complet (Enter/Leave/File)
	// SANS BORDURE, comme PureRef : le canevas jusqu'au pixel. NKWindow garde
	// les styles snap/min/max sous le capot (WM_NCCALCSIZE) ; notre en-tête
	// escamotable et les bords de redimensionnement font le reste.
	cfg.frame = false;
	NkWindow window(cfg);
	if (!window.IsOpen()) {
		logger.Error("[NkRef] creation fenetre echouee");
		return -1;
	}

	NkContextDesc desc;
	desc.api = NkGraphicsApi::NK_GFX_API_OPENGL; // seul backend Renderer2D validé à ce jour
	NkRenderWindow target(window, desc);
	if (!target.IsValid()) {
		logger.Error("[NkRef] cible de rendu invalide");
		return -2;
	}

	// Police de l'en-tête custom (la fenêtre n'a plus de barre de titre OS —
	// zoom, compte d'images et avis d'échec se lisent ICI). NkFont existe dans
	// deux namespaces → on qualifie (piège documenté par ConquerorProto).
	renderer::NkFont uiFont;
	// Police EMBARQUÉE (config optimale du module NKFont — netteté demandée par
	// Rihen), repli fichier si les données embarquées manquent du build.
	bool hasFont = uiFont.LoadEmbedded(*target.GetRenderer(), NkEmbeddedFontId::DroidSans);
	if (!hasFont)
		hasFont = uiFont.LoadFromFile(*target.GetRenderer(), "Resources/Fonts/Karla-Regular.ttf");
	std::printf("[NkRef] police en-tete : %s\n", hasFont ? "chargee" : "INTROUVABLE (lancer depuis la racine)");

	nkref::NkRefView view;
	nkref::NkRefBoard board;
	// Textures GPU, INDEX ALIGNÉ sur board.items (cf. en-tête du fichier).
	NkVector<NkTexture *> textures;
	auto &alloc = memory::NkGetDefaultAllocator();

	// ── NKGui : le panneau de propriétés escamotable ────────────────────────
	// Contexte sur le TAS (piège ConquerorLab : un contexte GUI complet sur la
	// pile = débordement muet au démarrage). Le NkGuiFont doit SURVIVRE à la
	// boucle : ctx.font est un pointeur non possédé.
	auto guiCtxPtr = memory::NkMakeUnique<nkgui::NkGuiContext>();
	nkgui::NkGuiContext &gui = *guiCtxPtr;
	gui.Init((int32)cfg.width, (int32)cfg.height);
	// Charte Rihen (pétrole/orange) déclinée en SOMBRE (défaut) et CLAIR —
	// bascule dans le panneau (demande Rihen).
	bool darkTheme = true;
	NkRefColors th = MakeDarkColors();
	ApplyGuiTheme(gui, darkTheme);
	nkgui::SetCurrentContext(&gui);
	renderer::NkGuiCanvasBackend guiBackend;
	const bool hasGui = guiBackend.Init(target.GetRenderer());
	auto guiFontPtr = memory::NkMakeUnique<nkgui::NkGuiFont>();
	bool hasGuiFont = guiFontPtr->LoadFromFile("Resources/Fonts/Karla-Regular.ttf", 17.0f);
	if (!hasGuiFont)
		hasGuiFont = guiFontPtr->LoadEmbedded(NkEmbeddedFontId::ProggyClean, 16.0f);
	if (hasGui && hasGuiFont && guiFontPtr->Valid()) {
		gui.font = guiFontPtr.Get();
		guiBackend.UploadFontGray8(guiFontPtr->TexId(), guiFontPtr->pixels, guiFontPtr->atlasW, guiFontPtr->atlasH);
	}
	bool panelOpen = false;			 // le tiroir ne gâche l'espace QUE s'il est ouvert
	constexpr float32 kPanelW = 300.0f; // largeur du tiroir (les sliders NKGui ont un label à droite)
	// Le conflit « glisser le vide » (question de Rihen) tranché par un RÉGLAGE :
	// coché (défaut, geste PureRef) = déplacer la FENÊTRE, et le rectangle passe
	// par Ctrl+glisser ; décoché = glisser trace le rectangle directement.
	bool dragEmptyMovesWindow = true;
	// Le crayon : n'importe quelle marque colorée sur la planche (demande
	// Rihen). Épaisseur choisie en PIXELS, convertie en monde à la pose du
	// trait : le trait zoome ensuite avec la planche, comme une image.
	bool penMode = false;
	uint8 penR = 247, penG = 154, penB = 40; // orange Rihen par défaut
	float32 penWidthPx = 4.0f;
	NkClock clock;

	NkAgentHooks agent;
	agent.Read();
	int32 agentFrame = 0;

	auto &events = NkEvents();
	math::NkVec2u lastSize = target.GetSize();

	NkMode mode = NkMode::Idle;
	math::NkVec2f mousePix{640.0f, 400.0f}; // dernière position curseur (client)
	math::NkVec2f rectStartPix{0.0f, 0.0f}; // départ du rectangle de sélection
	bool rectAdditive = false;
	int32 grabCorner = -1;					 // coin d'échelle saisi (0..3)
	math::NkVec2f grabAnchorWorld{0, 0};	 // coin OPPOSÉ (ancre d'échelle)
	math::NkVec2f grabCenterWorld{0, 0};	 // centre de l'item à la prise
	float32 grabScale = 1.0f;				 // échelle à la prise
	float32 grabDist = 1.0f;				 // distance ancre→souris à la prise
	float32 grabRotDeg = 0.0f;				 // rotation à la prise
	float32 grabAngleDeg = 0.0f;			 // angle centre→souris à la prise
	bool homeWasDown = false;
	int32 titleCooldown = 0;
	// Échecs de chargement VISIBLES : sans console, un fichier illisible était
	// un échec muet (retour Rihen : « 8 déposés, 6 pris en compte » — les deux
	// autres étaient des formats non décodés). Compté, affiché dans le titre.
	int32 loadFailCount = 0;
	int32 loadFailTicks = 0;
	char windowTitle[192] = "NkRef"; // partagé : SetTitle (barre des tâches) + en-tête custom
	bool running = true;

	// ── Les ACTIONS, partagées entre la souris et les crochets d'agent ──────
	// (un seul chemin : le crochet appelle exactement ce que l'événement appelle)

	// Charge une image et la pose centrée sur `world`. Retourne l'index (-1 si échec).
	auto addImageFromFile = [&](const char *path, const math::NkVec2f &world) -> int32 {
		NkImage img;
		if (!img.Load(path)) {
			logger.Warn("[NkRef] image illisible : %s", path);
			++loadFailCount;
			loadFailTicks = 400; // ~4 s d'affichage dans le titre
			return -1;
		}
		// PLAFOND D'IMPORT : une photo smartphone (24-48 Mpx) en texture brute
		// pèse 100-200 Mo de VRAM — quelques-unes suffisent à mettre le GPU à
		// genoux (machine qui s'éteint sous pic GPU : contrainte documentée du
		// dépôt ; crash rapporté par Rihen au-delà de 6 images). PureRef fait
		// pareil (réduction d'affichage). RIEN n'est perdu : l'Étape 2
		// embarquera les OCTETS SOURCE dans le .nkref, pas la texture.
		constexpr int32 kMaxSide = 4096;
		NkImage *resized = nullptr;
		if (img.Width() > kMaxSide || img.Height() > kMaxSide) {
			const int32 big = img.Width() > img.Height() ? img.Width() : img.Height();
			const float32 s = (float32)kMaxSide / (float32)big;
			// Resize RETOURNE une NOUVELLE image (il ne modifie pas l'objet —
			// piège documenté par le modeleur).
			resized = img.Resize((int32)((float32)img.Width() * s), (int32)((float32)img.Height() * s),
								 NkResizeFilter::NK_BICUBIC);
			if (resized)
				logger.Info("[NkRef] image reduite %dx%d -> %dx%d : %s", img.Width(), img.Height(),
							resized->Width(), resized->Height(), path);
		}
		const NkImage &srcImg = resized ? *resized : img;
		NkTexture *tex = alloc.New<NkTexture>();
		const bool okTex = tex && tex->LoadFromImage(*target.GetRenderer(), srcImg);
		const uint32 w = (uint32)srcImg.Width(), h = (uint32)srcImg.Height();
		if (resized)
			resized->Free(); // libère pixels + wrapper (pattern Alloc/Free)
		if (!okTex) {
			logger.Warn("[NkRef] upload GPU echoue : %s", path);
			if (tex)
				alloc.Delete(tex);
			++loadFailCount;
			loadFailTicks = 400;
			return -1;
		}
		board.AddItem(world, w, h, NkString(path));
		textures.PushBack(tex);
		logger.Info("[NkRef] image posee : %s (%ux%u)", path, w, h);
		return (int32)board.items.Size() - 1;
	};

	// Colle l'image du presse-papiers centrée sur `world`.
	auto pasteClipboard = [&](const math::NkVec2f &world) {
		NkClipboardImage clip;
		if (!window.GetClipboardImage(clip)) {
			logger.Info("[NkRef] presse-papiers : pas d'image");
			return;
		}
		NkImage img;
		if (!img.Create(clip.width, clip.height, math::NkColor(0, 0, 0, 0), 4))
			return;
		uint8 *dst = img.Pixels();
		const uint8 *src = clip.pixels.Data();
		const usize total = (usize)clip.width * clip.height * 4u;
		for (usize i = 0; i < total; ++i)
			dst[i] = src[i];
		NkTexture *tex = alloc.New<NkTexture>();
		if (!tex || !tex->LoadFromImage(*target.GetRenderer(), img)) {
			if (tex)
				alloc.Delete(tex);
			return;
		}
		board.AddItem(world, clip.width, clip.height, NkString());
		textures.PushBack(tex);
		logger.Info("[NkRef] collage presse-papiers : %ux%u", clip.width, clip.height);
	};

	// Clic gauche de sélection au pixel (px,py) — la partie « choisir » du
	// press ; le démarrage du drag reste dans le gestionnaire souris.
	auto selectAtPixel = [&](float32 px, float32 py, bool additive) -> int32 {
		const math::NkVec2u sz = target.GetSize();
		const math::NkVec2f w = view.PixelToWorld({px, py}, {(float32)sz.x, (float32)sz.y});
		const int32 hit = board.HitTest(w);
		if (hit >= 0) {
			// Re-cliquer un item DÉJÀ sélectionné sans Ctrl ne désélectionne
			// pas les autres : c'est ce qui permet de déplacer un groupe.
			if (!board.items[(usize)hit].selected)
				board.Select(hit, additive);
			else
				board.active = hit;
		} else if (!additive) {
			board.ClearSelection();
		}
		return hit;
	};

	auto moveSelectionByPixels = [&](float32 dx, float32 dy) {
		board.MoveSelected({dx / view.zoom, dy / view.zoom});
	};

	auto removeSelected = [&]() {
		NkVector<nkref::int32> removed;
		board.RemoveSelected(removed);
		for (int32 k = (int32)removed.Size() - 1; k >= 0; --k) {
			const usize idx = (usize)removed[(usize)k];
			alloc.Delete(textures[idx]);
			textures.RemoveAt(idx);
		}
	};

	auto reorderActive = [&](bool up) {
		const int32 old = board.active;
		const int32 other = board.RaiseActive(up);
		if (other >= 0 && old >= 0) {
			NkTexture *tmp = textures[(usize)old];
			textures[(usize)old] = textures[(usize)other];
			textures[(usize)other] = tmp;
		}
	};

	// ── Fenêtre sans bordure : en-tête escamotable + bords de resize ────────
	// L'en-tête (30 px) n'apparaît que quand le curseur s'approche du haut —
	// le reste du temps, le canevas EST la fenêtre (philosophie PureRef).
	constexpr float32 kBarH = 30.0f;   // hauteur de l'en-tête
	constexpr float32 kBtnW = 40.0f;   // largeur d'un bouton (− □ ×)
	constexpr float32 kEdge = 6.0f;	   // épaisseur des bords de redimensionnement
	constexpr float32 kBarShow = 40.0f; // zone d'approche qui révèle l'en-tête

	// 0 = rien, 1 = zone de drag, 2 = réduire, 3 = agrandir, 4 = fermer.
	auto barHit = [&](float32 px, float32 py, const math::NkVec2f &vp) -> int32 {
		if (py > kBarH)
			return 0;
		if (px >= vp.x - kBtnW)
			return 4;
		if (px >= vp.x - 2.0f * kBtnW)
			return 3;
		if (px >= vp.x - 3.0f * kBtnW)
			return 2;
		return 1;
	};

	// Bord de resize sous le curseur. -1 si aucun (ou fenêtre maximisée : les
	// bords d'une fenêtre maximisée appartiennent aux écrans voisins).
	auto edgeHit = [&](float32 px, float32 py, const math::NkVec2f &vp) -> int32 {
		if (window.IsMaximized())
			return -1;
		const bool l = px <= kEdge, r = px >= vp.x - kEdge;
		const bool t = py <= kEdge, b = py >= vp.y - kEdge;
		if (t && l)
			return (int32)NkWindow::NkResizeEdge::TopLeft;
		if (t && r)
			return (int32)NkWindow::NkResizeEdge::TopRight;
		if (b && l)
			return (int32)NkWindow::NkResizeEdge::BottomLeft;
		if (b && r)
			return (int32)NkWindow::NkResizeEdge::BottomRight;
		if (l)
			return (int32)NkWindow::NkResizeEdge::Left;
		if (r)
			return (int32)NkWindow::NkResizeEdge::Right;
		if (t)
			return (int32)NkWindow::NkResizeEdge::Top;
		if (b)
			return (int32)NkWindow::NkResizeEdge::Bottom;
		return -1;
	};

	// L'onglet du tiroir de propriétés : petit chevron centré verticalement.
	// Panneau FERMÉ : collé au bord droit. Panneau OUVERT : collé au bord
	// GAUCHE DU PANNEAU — le laisser sous le panneau le rendait invisible et
	// donc « le panneau ne se referme pas » (retour Rihen).
	auto tabX = [&](const math::NkVec2f &vp) -> float32 {
		return panelOpen ? (vp.x - kPanelW - 20.0f) : (vp.x - 22.0f);
	};
	auto tabHit = [&](float32 px, float32 py, const math::NkVec2f &vp) -> bool {
		const float32 tx = tabX(vp), ty = vp.y * 0.5f - 32.0f;
		return px >= tx && px <= tx + 18.0f && py >= ty && py <= ty + 64.0f;
	};

	// Le curseur est-il au-dessus de l'INTERFACE (tiroir ouvert ou onglet) ?
	// Si oui, la souris appartient à NKGui — pan/zoom/sélection du canevas ne
	// doivent PAS se déclencher dessous (piège d'occlusion documenté par NKGui :
	// pas de WantCaptureMouse global, c'est l'app qui route).
	auto overUi = [&](float32 px, float32 py, const math::NkVec2f &vp) -> bool {
		if (panelOpen && px >= vp.x - kPanelW && py >= kBarH)
			return true;
		return tabHit(px, py, vp);
	};

	// Position pixel de la poignée de rotation de l'item actif (au-dessus du
	// milieu du bord haut, à 26 px écran — constant quel que soit le zoom).
	auto rotationHandlePix = [&](const nkref::NkRefItem &it, const math::NkVec2f &vp) -> math::NkVec2f {
		math::NkVec2f c[4];
		nkref::NkRefBoard::Corners(it, c);
		const math::NkVec2f topMid = view.WorldToPixel({(c[0].x + c[1].x) * 0.5f, (c[0].y + c[1].y) * 0.5f}, vp);
		const math::NkVec2f center = view.WorldToPixel(it.pos, vp);
		math::NkVec2f dir{topMid.x - center.x, topMid.y - center.y};
		const float32 len = math::NkSqrt(dir.x * dir.x + dir.y * dir.y);
		if (len > 0.001f) {
			dir.x /= len;
			dir.y /= len;
		} else {
			dir = {0.0f, -1.0f};
		}
		return {topMid.x + dir.x * 26.0f, topMid.y + dir.y * 26.0f};
	};

	while (running && window.IsOpen()) {
		const float32 dt = clock.Tick().delta;
		const bool spaceDown = events.GetInputState().keyboard.IsKeyPressed(NkKey::NK_SPACE);
		const math::NkVec2u szNow = target.GetSize();
		const math::NkVec2f vp{(float32)szNow.x, (float32)szNow.y};

		// ── Événements ──────────────────────────────────────────────────────
		while (NkEvent *ev = events.PollEvent()) {
			if (ev->Is<NkWindowCloseEvent>()) {
				running = false;
				break;
			}

			if (auto *mw = ev->As<NkMouseWheelVerticalEvent>()) {
				const float32 wx = (float32)mw->GetX(), wy = (float32)mw->GetY();
				if (overUi(wx, wy, vp)) {
					gui.input.wheel += (float32)mw->GetDeltaY(); // défilement du panneau
				} else if (mw->GetModifiers().ctrl) {
					// Ctrl+molette = ordre de profondeur de l'item actif.
					reorderActive(mw->GetDeltaY() > 0.0);
				} else {
					const float32 factor = math::NkPow(1.15f, (float32)mw->GetDeltaY());
					view.ZoomAtPixel(factor, {wx, wy}, vp);
				}
			}

			if (auto *mb = ev->As<NkMouseButtonPressEvent>()) {
				const float32 px = (float32)mb->GetX(), py = (float32)mb->GetY();
				mousePix = {px, py};
				// NKGui reçoit TOUJOURS l'état souris (il ne réagit que survolé).
				gui.input.mousePos = {px, py};
				if (mb->IsLeft())
					gui.input.mouseDown[0] = true;
				if (mb->IsRight())
					gui.input.mouseDown[1] = true;
				if (mb->IsMiddle())
					gui.input.mouseDown[2] = true;
				gui.input.ctrlDown = mb->GetModifiers().ctrl;
				gui.input.shiftDown = mb->GetModifiers().shift;
				if (mb->IsLeft() && tabHit(px, py, vp)) {
					panelOpen = !panelOpen; // l'onglet du tiroir a priorité sur tout
				} else if (mb->IsLeft() && overUi(px, py, vp)) {
					// Le clic appartient au panneau : rien côté canevas.
				} else if (mb->IsMiddle() || (mb->IsLeft() && spaceDown)) {
					mode = NkMode::Pan;
				} else if (mb->IsLeft() && edgeHit(px, py, vp) >= 0) {
					// Bord de la fenêtre sans bordure : hand-off natif du resize
					// (l'OS prend la main jusqu'au relâchement — snap compris).
					window.BeginResize((NkWindow::NkResizeEdge)edgeHit(px, py, vp));
				} else if (mb->IsLeft() && barHit(px, py, vp) != 0) {
					// L'en-tête escamotable : boutons − □ ×, sinon déplacement natif.
					const int32 bh = barHit(px, py, vp);
					if (bh == 4)
						running = false;
					else if (bh == 3)
						window.Maximize(); // bascule agrandir/restaurer (Win32)
					else if (bh == 2)
						window.Minimize();
					else
						window.BeginDragMove();
				} else if (mb->IsLeft() && penMode) {
					// Mode crayon : le glisser TRACE (pas de sélection, pas de
					// déplacement de fenêtre — on ressort par D ou le panneau).
					const math::NkVec2f w = view.PixelToWorld({px, py}, vp);
					board.BeginStroke(penR, penG, penB, penWidthPx / view.zoom, w);
					mode = NkMode::Draw;
				} else if (mb->IsLeft()) {
					const bool ctrl = mb->GetModifiers().ctrl;
					// 1) Les poignées de l'item actif ont priorité sur tout.
					bool onHandle = false;
					if (board.active >= 0 && board.active < (int32)board.items.Size()) {
						const nkref::NkRefItem &it = board.items[(usize)board.active];
						math::NkVec2f c[4];
						nkref::NkRefBoard::Corners(it, c);
						for (int32 k = 0; k < 4 && !onHandle; ++k) {
							const math::NkVec2f hp = view.WorldToPixel(c[k], vp);
							if (math::NkAbs(hp.x - px) <= 7.0f && math::NkAbs(hp.y - py) <= 7.0f) {
								mode = NkMode::ScaleCorner;
								grabCorner = k;
								grabAnchorWorld = c[(k + 2) % 4]; // le coin OPPOSÉ ne bouge pas
								grabCenterWorld = it.pos;
								grabScale = it.scale;
								const math::NkVec2f mw2 = view.PixelToWorld({px, py}, vp);
								const float32 ddx = mw2.x - grabAnchorWorld.x, ddy = mw2.y - grabAnchorWorld.y;
								grabDist = math::NkSqrt(ddx * ddx + ddy * ddy);
								if (grabDist < 0.0001f)
									grabDist = 0.0001f;
								onHandle = true;
							}
						}
						if (!onHandle) {
							const math::NkVec2f rh = rotationHandlePix(it, vp);
							if (math::NkAbs(rh.x - px) <= 8.0f && math::NkAbs(rh.y - py) <= 8.0f) {
								mode = NkMode::Rotate;
								grabRotDeg = it.rotationDeg;
								const math::NkVec2f mw2 = view.PixelToWorld({px, py}, vp);
								grabAngleDeg = math::NkAtan2(mw2.y - it.pos.y, mw2.x - it.pos.x) *
											   (180.0f / math::NK_PI_F);
								onHandle = true;
							}
						}
					}
					// 2) Sinon : item → drag ; VIDE → selon le réglage : déplacer
					//    la FENÊTRE (geste PureRef, Ctrl+glisser = rectangle) ou
					//    tracer directement le rectangle de sélection.
					if (!onHandle) {
						const int32 hit = selectAtPixel(px, py, ctrl);
						if (hit >= 0) {
							mode = NkMode::DragItems;
						} else if (ctrl || !dragEmptyMovesWindow) {
							mode = NkMode::RectSelect;
							rectStartPix = {px, py};
							rectAdditive = ctrl;
						} else {
							window.BeginDragMove(); // la sélection a déjà été vidée
						}
					}
				}
			}

			if (auto *dc = ev->As<NkMouseDoubleClickEvent>()) {
				// Double-clic dans la zone de drag de l'en-tête = agrandir/restaurer,
				// le geste universel des barres de titre.
				if (dc->IsLeft() && barHit((float32)dc->GetX(), (float32)dc->GetY(), vp) == 1)
					window.Maximize();
			}

			if (auto *mr = ev->As<NkMouseButtonReleaseEvent>()) {
				if (mr->IsLeft())
					gui.input.mouseDown[0] = false;
				if (mr->IsRight())
					gui.input.mouseDown[1] = false;
				if (mr->IsMiddle())
					gui.input.mouseDown[2] = false;
				if (mode == NkMode::RectSelect && mr->IsLeft()) {
					const math::NkVec2f a = view.PixelToWorld(rectStartPix, vp);
					const math::NkVec2f b = view.PixelToWorld(mousePix, vp);
					board.SelectInRect(a, b, rectAdditive);
				}
				if (mr->IsMiddle() || mr->IsLeft())
					mode = NkMode::Idle;
			}

			if (auto *mm = ev->As<NkMouseMoveEvent>()) {
				const float32 px = (float32)mm->GetX(), py = (float32)mm->GetY();
				mousePix = {px, py};
				gui.input.mousePos = {px, py};
				const float32 dx = (float32)mm->GetDeltaX(), dy = (float32)mm->GetDeltaY();
				switch (mode) {
					case NkMode::Pan:
						view.PanByPixels(dx, dy);
						break;
					case NkMode::Draw:
						// 2 px écran de pas minimum : assez fin pour une écriture,
						// assez grossier pour ne pas pondre 10000 points.
						board.AppendStrokePoint(view.PixelToWorld({px, py}, vp), 2.0f / view.zoom);
						break;
					case NkMode::DragItems:
						moveSelectionByPixels(dx, dy);
						break;
					case NkMode::ScaleCorner:
						if (board.active >= 0 && board.active < (int32)board.items.Size()) {
							nkref::NkRefItem &it = board.items[(usize)board.active];
							const math::NkVec2f mw2 = view.PixelToWorld({px, py}, vp);
							const float32 ddx = mw2.x - grabAnchorWorld.x, ddy = mw2.y - grabAnchorWorld.y;
							const float32 dist = math::NkSqrt(ddx * ddx + ddy * ddy);
							float32 s = grabScale * (dist / grabDist);
							if (s < 0.01f)
								s = 0.01f;
							if (s > 100.0f)
								s = 100.0f;
							// L'ancre (coin opposé) reste FIXE : le centre suit
							// linéairement le rapport d'échelle.
							const float32 r = s / grabScale;
							it.scale = s;
							it.pos = {grabAnchorWorld.x + (grabCenterWorld.x - grabAnchorWorld.x) * r,
									  grabAnchorWorld.y + (grabCenterWorld.y - grabAnchorWorld.y) * r};
						}
						break;
					case NkMode::Rotate:
						if (board.active >= 0 && board.active < (int32)board.items.Size()) {
							nkref::NkRefItem &it = board.items[(usize)board.active];
							const math::NkVec2f mw2 = view.PixelToWorld({px, py}, vp);
							const float32 a = math::NkAtan2(mw2.y - it.pos.y, mw2.x - it.pos.x) *
											  (180.0f / math::NK_PI_F);
							float32 rot = grabRotDeg + (a - grabAngleDeg);
							if (mm->GetModifiers().shift) { // Maj = crans de 15°
								rot = (float32)((int32)(rot / 15.0f + (rot >= 0 ? 0.5f : -0.5f))) * 15.0f;
							}
							it.rotationDeg = rot;
						}
						break;
					default:
						break;
				}
			}

			if (auto *kp = ev->As<NkKeyPressEvent>()) {
				const NkKey k = kp->GetKey();
				const bool ctrl = kp->GetModifiers().ctrl;
				if (k == NkKey::NK_DELETE) {
					removeSelected();
				} else if (k == NkKey::NK_X && board.HasSelection()) {
					board.MirrorSelected(true);
				} else if (k == NkKey::NK_Y && board.HasSelection()) {
					board.MirrorSelected(false);
				} else if (k == NkKey::NK_PAGE_UP) {
					reorderActive(true);
				} else if (k == NkKey::NK_PAGE_DOWN) {
					reorderActive(false);
				} else if (k == NkKey::NK_V && ctrl) {
					pasteClipboard(view.PixelToWorld(mousePix, vp));
				} else if (k == NkKey::NK_D && !ctrl) {
					penMode = !penMode; // bascule crayon (aussi dans le panneau)
				} else if (k == NkKey::NK_P && ctrl) {
					// « Pack » : rangement compact (sélection ≥ 2, sinon tout).
					board.Pack(16.0f);
				} else if (k == NkKey::NK_T) {
					// Toujours-devant — l'API « fenêtre discrète » de NKWindow.
					// Une bascule clavier en attendant le menu clic droit (Étape 3/4).
					window.SetAlwaysOnTop(!window.IsAlwaysOnTop());
					titleCooldown = 0; // reflète l'état dans le titre tout de suite
				} else if (k >= NkKey::NK_NUM1 && k <= NkKey::NK_NUM0) {
					// Opacité de fenêtre : 1..9 = 10..90 %, 0 = opaque (comme les
					// presets PureRef). Enum contigu NUM1..NUM9 puis NUM0.
					const int32 n = (int32)k - (int32)NkKey::NK_NUM1 + 1; // 1..10
					window.SetOpacity(n >= 10 ? 1.0f : (float32)n * 0.1f);
					titleCooldown = 0;
				}
			}

			if (auto *df = ev->As<NkDropFileEvent>()) {
				// Chaque fichier au point de dépôt, en cascade de 24 px pour
				// que plusieurs fichiers ne s'empilent pas exactement.
				const math::NkVec2f base = view.PixelToWorld(
					{(float32)df->data.x, (float32)df->data.y}, vp);
				for (uint32 i = 0; i < df->data.Count(); ++i) {
					const float32 off = (float32)i * 24.0f / view.zoom;
					addImageFromFile(df->data.paths[i].CStr(), {base.x + off, base.y + off});
				}
			}
		}
		if (!running || !window.IsOpen())
			break;

		// Début (Home) = revenir à l'origine (front montant seulement).
		const bool homeDown = events.GetInputState().keyboard.IsKeyPressed(NkKey::NK_HOME);
		if (homeDown && !homeWasDown)
			view.Reset();
		homeWasDown = homeDown;

		// ── Resize (garde-fou WM_SIZE au démarrage, piège documenté) ────────
		const math::NkVec2u sz = target.GetSize();
		if (sz.x != lastSize.x || sz.y != lastSize.y) {
			if (lastSize.x != 0 && sz.x > 0 && sz.y > 0)
				target.OnResize(sz.x, sz.y);
			lastSize = sz;
		}

		// ── Crochets d'agent : mêmes fonctions que la souris ────────────────
		if (agent.panFrame > 0 && agentFrame == agent.panFrame)
			view.PanByPixels(agent.panDx, agent.panDy);
		if (agent.zoomFrame > 0 && agentFrame == agent.zoomFrame)
			view.ZoomAtPixel(agent.zoomFactor, {agent.zoomPx, agent.zoomPy}, vp);
		for (int32 i = 0; i < agent.dropCount; ++i) {
			if (agent.drops[i].frame > 0 && agentFrame == agent.drops[i].frame)
				addImageFromFile(agent.drops[i].path,
								 view.PixelToWorld({agent.drops[i].px, agent.drops[i].py}, vp));
		}
		for (int32 i = 0; i < agent.clickCount; ++i) {
			if (agent.clicks[i].frame > 0 && agentFrame == agent.clicks[i].frame)
				selectAtPixel(agent.clicks[i].px, agent.clicks[i].py, false);
		}
		if (agent.moveFrame > 0 && agentFrame == agent.moveFrame)
			moveSelectionByPixels(agent.moveDx, agent.moveDy);
		if (agent.packFrame > 0 && agentFrame == agent.packFrame)
			board.Pack(16.0f);
		if (agent.panelFrame > 0 && agentFrame == agent.panelFrame)
			panelOpen = true; // même variable que le clic sur l'onglet
		if (agent.themeFrame > 0 && agentFrame == agent.themeFrame) {
			darkTheme = agent.themeDark; // même chemin que la case du panneau
			th = darkTheme ? MakeDarkColors() : MakeLightColors();
			ApplyGuiTheme(gui, darkTheme);
		}

		// ── NKGui : le tiroir de propriétés (logique seulement, rendu au Submit) ──
		gui.viewW = (int32)sz.x;
		gui.viewH = (int32)sz.y;
		gui.BeginFrame(dt);
		if (panelOpen && hasGui) {
			const nkgui::NkRect pr{vp.x - kPanelW, kBarH, kPanelW, vp.y - kBarH};
			if (nkgui::BeginPanel(gui, "Proprietes", pr)) {
				// La bascule sombre/clair, en tête : elle rethème TOUT
				// (canevas, grille, en-tête, panneau) d'un coup.
				if (nkgui::Checkbox(gui, "Theme sombre", darkTheme)) {
					th = darkTheme ? MakeDarkColors() : MakeLightColors();
					ApplyGuiTheme(gui, darkTheme);
				}
				nkgui::Separator(gui);
				nkgui::Text(gui, "Fenetre");
				bool onTop = window.IsAlwaysOnTop();
				if (nkgui::Checkbox(gui, "Toujours devant (T)", onTop))
					window.SetAlwaysOnTop(onTop);
				float32 wop = window.GetOpacity();
				if (nkgui::SliderFloat(gui, "Opacite fenetre", wop, 0.2f, 1.0f))
					window.SetOpacity(wop);
				nkgui::Checkbox(gui, "Glisser le fond = fenetre", dragEmptyMovesWindow);
				nkgui::Separator(gui);
				nkgui::Text(gui, "Planche");
				{
					// Le COMPTE des fichiers ouverts, demandé par Rihen — ici, et
					// toujours dans l'en-tête escamotable.
					int32 selCount = 0;
					for (usize i = 0; i < board.items.Size(); ++i)
						if (board.items[i].selected)
							++selCount;
					char line[96];
					std::snprintf(line, sizeof(line), "%d image%s - %d selectionnee%s - %d trait%s",
								  (int)board.items.Size(), board.items.Size() > 1 ? "s" : "", (int)selCount,
								  selCount > 1 ? "s" : "", (int)board.strokes.Size(),
								  board.strokes.Size() > 1 ? "s" : "");
					nkgui::Text(gui, line);
				}
				if (nkgui::Button(gui, "Pack (Ctrl+P)"))
					board.Pack(16.0f);
				if (nkgui::Button(gui, "Origine (Debut)"))
					view.Reset();
				nkgui::Separator(gui);
				nkgui::Text(gui, "Crayon");
				nkgui::Checkbox(gui, "Mode crayon (D)", penMode);
				{
					// Palette : la couleur ACTIVE est marquée [x]. Des pastilles
					// colorées viendront quand NKGui saura teinter un bouton.
					struct PenColor {
							const char *name;
							uint8 r, g, b;
					};
					static const PenColor kPen[] = {
						{"Orange", 247, 154, 40}, {"Petrole", 22, 140, 155}, {"Rouge", 225, 60, 50},
						{"Vert", 80, 190, 100},	  {"Bleu", 70, 130, 230},	 {"Blanc", 240, 242, 245},
						{"Noir", 15, 16, 18},
					};
					for (int32 c = 0; c < (int32)(sizeof(kPen) / sizeof(kPen[0])); ++c) {
						const bool active = penR == kPen[c].r && penG == kPen[c].g && penB == kPen[c].b;
						char lbl[24];
						std::snprintf(lbl, sizeof(lbl), "%s%s", active ? "[x] " : "", kPen[c].name);
						if (nkgui::Button(gui, lbl)) {
							penR = kPen[c].r;
							penG = kPen[c].g;
							penB = kPen[c].b;
							penMode = true; // choisir une couleur = vouloir dessiner
						}
					}
				}
				nkgui::SliderFloat(gui, "Epaisseur (px)", penWidthPx, 1.0f, 24.0f);
				if (nkgui::Button(gui, "Annuler le dernier trait"))
					board.UndoStroke();
				if (nkgui::Button(gui, "Effacer tous les traits"))
					board.ClearStrokes();
				nkgui::Separator(gui);
				if (board.active >= 0 && board.active < (int32)board.items.Size()) {
					nkref::NkRefItem &it = board.items[(usize)board.active];
					nkgui::Text(gui, "Image active");
					nkgui::SliderFloat(gui, "Opacite", it.opacity, 0.05f, 1.0f);
					nkgui::Checkbox(gui, "Miroir X (X)", it.mirrorX);
					nkgui::Checkbox(gui, "Miroir Y (Y)", it.mirrorY);
					if (nkgui::Button(gui, "Supprimer (Suppr)"))
						removeSelected();
				} else {
					nkgui::Text(gui, "(aucune image active)");
				}
				if (nkgui::CollapsingHeader(gui, "Gestes")) {
					nkgui::Text(gui, "Molette : zoom au curseur");
					nkgui::Text(gui, "Milieu / Espace+glisser : pan");
					nkgui::Text(gui, "Ctrl+glisser : rectangle");
					nkgui::Text(gui, "Coins : echelle - Rond : rotation");
					nkgui::Text(gui, "PgUp/PgDn : ordre - Ctrl+V : coller");
				}
				nkgui::EndPanel(gui);
			}
		}
		gui.EndFrame();

		// ── Rendu : fond + grille + images + sélection, en espace écran ─────
		target.Clear(th.clear);
		NkRenderer2D &r = target.GetRenderer2D();


		const float32 spacing = view.GridSpacing(32.0f);
		const math::NkVec2f wMin = view.PixelToWorld({0.0f, 0.0f}, vp);
		const math::NkVec2f wMax = view.PixelToWorld(vp, vp);
		const NkColor2D minor = th.gridMinor;
		const NkColor2D major = th.gridMajor;
		const int64 ix0 = (int64)math::NkFloor(wMin.x / spacing);
		const int64 ix1 = (int64)math::NkCeil(wMax.x / spacing);
		for (int64 i = ix0; i <= ix1; ++i) {
			const float32 xPix = view.WorldToPixel({(float32)i * spacing, 0.0f}, vp).x;
			r.DrawLine({xPix, 0.0f}, {xPix, vp.y}, (i % 8) == 0 ? major : minor, 1.0f);
		}
		const int64 iy0 = (int64)math::NkFloor(wMin.y / spacing);
		const int64 iy1 = (int64)math::NkCeil(wMax.y / spacing);
		for (int64 i = iy0; i <= iy1; ++i) {
			const float32 yPix = view.WorldToPixel({0.0f, (float32)i * spacing}, vp).y;
			r.DrawLine({0.0f, yPix}, {vp.x, yPix}, (i % 8) == 0 ? major : minor, 1.0f);
		}
		const math::NkVec2f origin = view.WorldToPixel({0.0f, 0.0f}, vp);
		const NkColor2D axis = th.axis;
		if (origin.x >= 0.0f && origin.x <= vp.x)
			r.DrawLine({origin.x, 0.0f}, {origin.x, vp.y}, axis, 1.0f);
		if (origin.y >= 0.0f && origin.y <= vp.y)
			r.DrawLine({0.0f, origin.y}, {vp.x, origin.y}, axis, 1.0f);

		// Les images, du fond vers le dessus (l'ordre du tableau). CULLING :
		// une planche de dizaines de photos ne doit coûter que ce qui est
		// VISIBLE (le GPU de cette machine n'aime pas les pics — et dessiner
		// hors écran ne sert à rien).
		for (usize i = 0; i < board.items.Size(); ++i) {
			const nkref::NkRefItem &it = board.items[i];
			NkTexture *tex = textures[i];
			if (!tex || !tex->IsValid())
				continue;
			{
				math::NkVec2f c[4];
				nkref::NkRefBoard::Corners(it, c);
				float32 minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
				for (int32 k = 0; k < 4; ++k) {
					const math::NkVec2f p = view.WorldToPixel(c[k], vp);
					if (p.x < minX)
						minX = p.x;
					if (p.x > maxX)
						maxX = p.x;
					if (p.y < minY)
						minY = p.y;
					if (p.y > maxY)
						maxY = p.y;
				}
				if (maxX < 0.0f || minX > vp.x || maxY < 0.0f || minY > vp.y)
					continue; // entièrement hors écran
			}
			NkSprite sp(*tex);
			sp.SetOrigin({(float32)it.texW * 0.5f, (float32)it.texH * 0.5f});
			sp.SetPosition(view.WorldToPixel(it.pos, vp));
			sp.SetRotation(it.rotationDeg);
			sp.SetScale({it.scale * view.zoom, it.scale * view.zoom});
			sp.SetFlipX(it.mirrorX);
			sp.SetFlipY(it.mirrorY);
			// Opacité PAR IMAGE (propriété PureRef) : modulée par la couleur.
			sp.SetColor(NkColor2D{255, 255, 255, (uint8)(it.opacity * 255.0f + 0.5f)});
			// NkSprite hérite de NkIDrawable2D ET NkDrawable → on lève
			// l'ambiguïté de Draw() en ciblant le chemin NkDrawable.
			target.Draw(static_cast<const NkDrawable &>(sp));
		}

		// Les traits de crayon, par-dessus les images (annotations). Culling
		// AABB par trait, comme les images.
		for (usize si = 0; si < board.strokes.Size(); ++si) {
			const nkref::NkRefStroke &st = board.strokes[si];
			if (st.points.Empty())
				continue;
			float32 minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
			for (usize p = 0; p < st.points.Size(); ++p) {
				if (st.points[p].x < minX)
					minX = st.points[p].x;
				if (st.points[p].x > maxX)
					maxX = st.points[p].x;
				if (st.points[p].y < minY)
					minY = st.points[p].y;
				if (st.points[p].y > maxY)
					maxY = st.points[p].y;
			}
			const math::NkVec2f pMin = view.WorldToPixel({minX, minY}, vp);
			const math::NkVec2f pMax = view.WorldToPixel({maxX, maxY}, vp);
			const float32 wpx = st.widthWorld * view.zoom;
			if (pMax.x + wpx < 0.0f || pMin.x - wpx > vp.x || pMax.y + wpx < 0.0f || pMin.y - wpx > vp.y)
				continue;
			const NkColor2D col{st.r, st.g, st.b, st.a};
			if (st.points.Size() == 1) { // simple point = pastille
				r.DrawFilledCircle(view.WorldToPixel(st.points[0], vp), wpx * 0.5f + 1.0f, col, 12);
			} else {
				for (usize p = 0; p + 1 < st.points.Size(); ++p)
					r.DrawLine(view.WorldToPixel(st.points[p], vp), view.WorldToPixel(st.points[p + 1], vp), col,
							   wpx < 1.0f ? 1.0f : wpx);
			}
		}

		// Contours de sélection + poignées (par-dessus les images).
		const NkColor2D selCol = th.selected;  // sélectionné
		const NkColor2D activeCol = th.active; // actif (porte les poignées)
		for (usize i = 0; i < board.items.Size(); ++i) {
			const nkref::NkRefItem &it = board.items[i];
			if (!it.selected)
				continue;
			math::NkVec2f c[4];
			nkref::NkRefBoard::Corners(it, c);
			math::NkVec2f p[4];
			for (int32 k = 0; k < 4; ++k)
				p[k] = view.WorldToPixel(c[k], vp);
			const NkColor2D col = ((int32)i == board.active) ? activeCol : selCol;
			for (int32 k = 0; k < 4; ++k)
				r.DrawLine(p[k], p[(k + 1) % 4], col, 1.5f);
			if ((int32)i == board.active) {
				for (int32 k = 0; k < 4; ++k)
					r.DrawFilledRect({p[k].x - 4.0f, p[k].y - 4.0f, 8.0f, 8.0f}, activeCol);
				const math::NkVec2f topMid{(p[0].x + p[1].x) * 0.5f, (p[0].y + p[1].y) * 0.5f};
				const math::NkVec2f rh = rotationHandlePix(it, vp);
				r.DrawLine(topMid, rh, activeCol, 1.5f);
				r.DrawCircle(rh, 5.0f, activeCol, 24);
			}
		}

		// Rectangle de sélection en cours.
		if (mode == NkMode::RectSelect) {
			const float32 x0 = rectStartPix.x < mousePix.x ? rectStartPix.x : mousePix.x;
			const float32 y0 = rectStartPix.y < mousePix.y ? rectStartPix.y : mousePix.y;
			const float32 w = math::NkAbs(mousePix.x - rectStartPix.x);
			const float32 h = math::NkAbs(mousePix.y - rectStartPix.y);
			const NkColor2D rimCol = th.rectRim;
			r.DrawFilledRect({x0, y0, w, h}, th.rectFill);
			r.DrawLine({x0, y0}, {x0 + w, y0}, rimCol, 1.0f);
			r.DrawLine({x0 + w, y0}, {x0 + w, y0 + h}, rimCol, 1.0f);
			r.DrawLine({x0 + w, y0 + h}, {x0, y0 + h}, rimCol, 1.0f);
			r.DrawLine({x0, y0 + h}, {x0, y0}, rimCol, 1.0f);
		}

		// ── L'en-tête escamotable (par-dessus tout) ─────────────────────────
		// Révélé quand le curseur s'approche du haut, ou pendant qu'un avis
		// d'échec est actif (il faut bien le LIRE quelque part).
		const bool barVisible = mousePix.y <= kBarShow || loadFailTicks > 0;
		if (barVisible) {
			r.DrawFilledRect({0.0f, 0.0f, vp.x, kBarH}, th.barBg);
			r.DrawLine({0.0f, kBarH}, {vp.x, kBarH}, th.barLine, 1.0f);
			// La pastille de marque (petit carré orange), à défaut de logo.
			r.DrawFilledRect({10.0f, 10.0f, 10.0f, 10.0f}, NkColor2D{247, 154, 40, 255});
			if (hasFont) {
				NkText txt(uiFont, windowTitle, 15);
				txt.SetFillColor(th.barText);
				// La position d'un NkText est sa LIGNE DE BASE : à y=6 le texte
				// montait HORS fenêtre (invisible, vécu) — on vise le bas de la barre.
				txt.SetPosition({30.0f, 21.0f});
				target.Draw(static_cast<const NkDrawable &>(txt));
			}
			// Boutons − □ × : survol surligné, glyphes en primitives.
			const int32 hov = barHit(mousePix.x, mousePix.y, vp);
			const float32 bx2 = vp.x - 3.0f * kBtnW, bx1 = vp.x - 2.0f * kBtnW, bx0 = vp.x - kBtnW;
			if (hov == 2)
				r.DrawFilledRect({bx2, 0.0f, kBtnW, kBarH}, th.barHover);
			if (hov == 3)
				r.DrawFilledRect({bx1, 0.0f, kBtnW, kBarH}, th.barHover);
			if (hov == 4)
				r.DrawFilledRect({bx0, 0.0f, kBtnW, kBarH}, th.barClose);
			const NkColor2D glyph = th.barGlyph;
			const float32 cy = kBarH * 0.5f;
			// − (réduire)
			r.DrawLine({bx2 + kBtnW * 0.5f - 5.0f, cy}, {bx2 + kBtnW * 0.5f + 5.0f, cy}, glyph, 1.5f);
			// □ (agrandir/restaurer)
			{
				const float32 cx = bx1 + kBtnW * 0.5f;
				r.DrawLine({cx - 5.0f, cy - 5.0f}, {cx + 5.0f, cy - 5.0f}, glyph, 1.5f);
				r.DrawLine({cx + 5.0f, cy - 5.0f}, {cx + 5.0f, cy + 5.0f}, glyph, 1.5f);
				r.DrawLine({cx + 5.0f, cy + 5.0f}, {cx - 5.0f, cy + 5.0f}, glyph, 1.5f);
				r.DrawLine({cx - 5.0f, cy + 5.0f}, {cx - 5.0f, cy - 5.0f}, glyph, 1.5f);
			}
			// × (fermer)
			{
				const float32 cx = bx0 + kBtnW * 0.5f;
				r.DrawLine({cx - 5.0f, cy - 5.0f}, {cx + 5.0f, cy + 5.0f}, glyph, 1.5f);
				r.DrawLine({cx - 5.0f, cy + 5.0f}, {cx + 5.0f, cy - 5.0f}, glyph, 1.5f);
			}
		}

		// Le panneau NKGui par-dessus le canevas (DEUX Submit, jamais un
		// seul : dlOverlay porte popups/combos — piège documenté du pont).
		if (hasGui) {
			guiBackend.Submit(gui.dl, (uint32)sz.x, (uint32)sz.y);
			guiBackend.Submit(gui.dlOverlay, (uint32)sz.x, (uint32)sz.y);
		}

		// L'onglet du tiroir — APRÈS les Submit : il doit rester visible
		// au-dessus du panneau ouvert (sinon impossible de refermer, vécu).
		{
			const float32 tx = tabX(vp), ty = vp.y * 0.5f - 32.0f;
			r.DrawFilledRect({tx, ty, 18.0f, 64.0f}, th.tabBg); // pétrole Rihen
			const NkColor2D ch = th.tabGlyph;
			const float32 cy2 = ty + 32.0f;
			if (panelOpen) { // chevron vers la droite = refermer le tiroir
				r.DrawLine({tx + 6.0f, cy2 - 6.0f}, {tx + 12.0f, cy2}, ch, 1.5f);
				r.DrawLine({tx + 12.0f, cy2}, {tx + 6.0f, cy2 + 6.0f}, ch, 1.5f);
			} else { // chevron vers la gauche = ouvrir
				r.DrawLine({tx + 12.0f, cy2 - 6.0f}, {tx + 6.0f, cy2}, ch, 1.5f);
				r.DrawLine({tx + 6.0f, cy2}, {tx + 12.0f, cy2 + 6.0f}, ch, 1.5f);
			}
		}

		// Curseur : flèches de redimensionnement sur les bords de la fenêtre
		// sans bordure (persistant : à rappeler chaque frame, cf. NkWindow.h).
		{
			NkWindow::NkCursorType cur = NkWindow::NkCursorType::Arrow;
			switch (edgeHit(mousePix.x, mousePix.y, vp)) {
				case (int32)NkWindow::NkResizeEdge::Left:
				case (int32)NkWindow::NkResizeEdge::Right:
					cur = NkWindow::NkCursorType::ResizeWE;
					break;
				case (int32)NkWindow::NkResizeEdge::Top:
				case (int32)NkWindow::NkResizeEdge::Bottom:
					cur = NkWindow::NkCursorType::ResizeNS;
					break;
				case (int32)NkWindow::NkResizeEdge::TopLeft:
				case (int32)NkWindow::NkResizeEdge::BottomRight:
					cur = NkWindow::NkCursorType::ResizeNWSE;
					break;
				case (int32)NkWindow::NkResizeEdge::TopRight:
				case (int32)NkWindow::NkResizeEdge::BottomLeft:
					cur = NkWindow::NkCursorType::ResizeNESW;
					break;
				default:
					break;
			}
			window.SetCursor(cur);
		}

		target.Display();

		if (loadFailTicks > 0 && --loadFailTicks == 0) {
			loadFailCount = 0; // l'avis expire, le compteur repart
			titleCooldown = 0;
		}
		if (--titleCooldown <= 0) {
			char extra[96] = "";
			// L'état « fenêtre discrète » se lit dans le titre (pas encore de menu).
			if (window.IsAlwaysOnTop() && window.GetOpacity() < 1.0f)
				std::snprintf(extra, sizeof(extra), " - devant - opacite %d%%",
							  (int)(window.GetOpacity() * 100.0f + 0.5f));
			else if (window.IsAlwaysOnTop())
				std::snprintf(extra, sizeof(extra), " - devant");
			else if (window.GetOpacity() < 1.0f)
				std::snprintf(extra, sizeof(extra), " - opacite %d%%",
							  (int)(window.GetOpacity() * 100.0f + 0.5f));
			char fails[64] = "";
			if (loadFailCount > 0 && loadFailTicks > 0)
				std::snprintf(fails, sizeof(fails), " - %d fichier%s illisible%s (format ?)", (int)loadFailCount,
							  loadFailCount > 1 ? "s" : "", loadFailCount > 1 ? "s" : "");
			char t2[96];
			std::snprintf(t2, sizeof(t2), "NkRef - %d%% - %d image%s", (int)(view.zoom * 100.0f + 0.5f),
						  (int)board.items.Size(), board.items.Size() > 1 ? "s" : "");
			std::snprintf(windowTitle, sizeof(windowTitle), "%s%s%s", t2, extra, fails);
			window.SetTitle(windowTitle); // barre des tâches / Alt+Tab
			titleCooldown = 15;
		}

		// ── Crochets d'agent : capture et sortie à la frame demandée ────────
		++agentFrame;
		for (int32 s = 0; s < agent.shotCount; ++s) {
			if (agent.shotFrames[s] > 0 && agentFrame == agent.shotFrames[s]) {
				char capPath[256];
				if (NkNextCapturePath("nkref", capPath, (int32)sizeof(capPath))) {
					const bool okCap = NkCaptureWholeWindow(window, capPath);
					std::printf("[NkRef] capture -> %s : %s\n", capPath, okCap ? "ecrite" : "ECHEC");
				}
			}
		}
		if (agent.exitFrame > 0 && agentFrame >= agent.exitFrame)
			running = false; // >= : même si une frame saute, on sort toujours

		NkChrono::Sleep(8.0);
	}

	// Libération des textures AVANT la mort du contexte GL (target vit encore).
	for (usize i = 0; i < textures.Size(); ++i)
		alloc.Delete(textures[i]);
	textures.Clear();

	// PAS de window.Close() explicite ici : `target` (déclaré APRÈS `window`)
	// doit mourir AVANT la fenêtre — glXDestroyContext sur un Display X11 déjà
	// fermé segfaultait sous WSLg (backtrace _XSend/XInitExtension, vécu le
	// 2026-08-11). L'ordre de destruction C++ inverse fait exactement ça, et
	// ~NkWindow appelle Close() lui-même.
	return 0;
}
