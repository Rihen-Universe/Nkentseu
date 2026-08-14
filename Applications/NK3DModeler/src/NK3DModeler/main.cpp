// =============================================================================
// main.cpp — Point d'entree de NK3DModeler.
//
// L'INTERFACE EST PEINTE DIRECTEMENT, sans passer par NkEditorShell.
//   Le shell de NKEditorKit apporte sa PROPRE chrome — barre de menus, barre
//   d'etat, docking, palette — pensee pour un IDE. Elle est excellente pour
//   NKCode et elle empeche de coller a une maquette au pixel pres : on passerait
//   son temps a lutter contre une disposition qu'on ne controle pas.
//   NK3DModeler doit ressembler EXACTEMENT a l'ecran A valide par Rihen, donc on
//   ouvre la fenetre, on prend la draw list, et on peint.
//
// CE QUI EST DEJA VRAI ET N'EST PAS DE LA MAQUETTE
//   * pas une seule couleur en dur dans le rendu : tout passe par les roles du
//     theme, y compris les axes, les types d'assets et les six roles propres au
//     produit. C'est ce qui fera fonctionner le theme clair sans y retoucher ;
//   * les raccourcis affiches sont LUS dans NkShortcutTable, jamais recopies :
//     rebinder une touche changera l'affichage tout seul.
//
// CE QUI RESTE A FAIRE, par iterations successives comme convenu : les
//   interactions (survol, clic, redimensionnement des zones), puis la vue 3D
//   reelle, puis la pile de modificateurs pilotee par NkModifierParams.
// =============================================================================

#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEvent/NkEvent.h"
#include "NKGui/NkEditorRHIRenderer.h" // Integrations/NKGui
#include "NK3DModeler/Viewport/NkViewport3D.h"
#include "NK3DModeler/Viewport/NkDemo3DHost.h" // PORTAGE INTEGRAL de --demo=2
#include "NKGui/Core/NkGuiContext.h"
#include "NKLogger/NkLog.h"
#include "NKGui/Core/NkGuiFont.h"
#include "NKTime/NkClock.h"
#include "NKPlatform/NkEnv.h"

#include "NK3DModeler/Shell/NkModelerTheme.h"
#include "NK3DModeler/Shell/NkModelerScreens.h"
#include "NK3DModeler/Shell/NkModelerChrome.h" // separateurs, dialogues, barre d etat
#include "NK3DModeler/Shell/NkModelerJournal.h"
#include "NKContainers/String/Encoding/NkBase64.h" // les messages du moteur, lisibles dans l'app
#include "NK3DModeler/Shell/NkModelerHierarchy.h" // hierarchie + menus de scene
#include "NK3DModeler/Shell/NkModelerViewport.h"  // vue 3D et ses surcouches
#include "NK3DModeler/Shell/NkModelerProperties.h" // panneau de proprietes
#include "NK3DModeler/Shell/NkModelerBrowser.h" // navigateur de projet
#include "NK3DModeler/Shell/NkModelerMenus.h"   // menus deroulants
// ECRAN D'ACCUEIL + socle PROJET (.nk3dm) : l'accueil est peint tant qu'aucun
// projet n'est ouvert, et il porte l'execution differee des actions projet.
#include "NK3DModeler/Shell/NkModelerWelcome.h"
#include "NKEvent/NkMouseEvent.h"
// Captures (« Capturer la vue » / « Tutoriel ») : dossier + numerotation +
// photographie de la fenetre entiere.
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKImage/NKImage.h"
#if defined(NKENTSEU_PLATFORM_WINDOWS)
#include <windows.h>
#endif

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::editorkit;
using namespace nkentseu::nk3d;

namespace {

	// ── CAPTURES ────────────────────────────────────────────────────────────
	// Premier chemin LIBRE captures/<prefixe>_NNN.png : une numerotation simple
	// et lisible, sans horloge -- l'ordre des fichiers EST l'ordre des prises.
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
	// « Tutoriel » : TOUTE la fenetre, interface comprise. PrintWindow avec
	// PW_RENDERFULLCONTENT (2) demande a l'OS l'image COMPOSEE (le rendu D3D
	// inclus) ; repli BitBlt si l'OS refuse. BGRA -> RGBA puis PNG via NkImage.
	// La capture de fenetre rend ses PIXELS ici, dans `out` : l'enregistrement
	// video en a besoin image par image, et sauver un PNG pour le relire aurait
	// ete absurde. La version qui ecrit un fichier s'appuie dessus -- une seule
	// facon de photographier la fenetre, donc un seul comportement a corriger.
	// TAILLE REELLE de la fenetre a l'ecran, cadre compris -- exactement celle
	// que produira la capture. Ouvrir un fichier video demande de connaitre
	// cette taille AVANT la premiere image : la deviner de la surface de rendu
	// donnerait un fichier qui ne correspond a rien.
	bool NkCaptureWholeWindowSize(NkWindow &win, uint32 *outW, uint32 *outH) {
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
		if (outW)
			*outW = (uint32)w;
		if (outH)
			*outH = (uint32)h;
		return true;
	}

	bool NkCaptureWholeWindowToImage(NkWindow &win, NkImage &out, int32 *outW, int32 *outH) {
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
		if (outW)
			*outW = w;
		if (outH)
			*outH = h;
		HDC hdcWin = GetWindowDC(hwnd);
		HDC hdcMem = CreateCompatibleDC(hdcWin);
		BITMAPINFO bi{};
		bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bi.bmiHeader.biWidth = w;
		bi.bmiHeader.biHeight = -h; // negatif = origine en HAUT (ordre des lignes PNG)
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
				ok = out.Create((uint32)w, (uint32)h, math::NkColor(0, 0, 0, 255), 4);
				if (ok) {
					const uint8 *src = (const uint8 *)bits;
					uint8 *dst = out.Pixels();
					for (int32 i = 0; i < w * h; ++i) { // BGRA -> RGBA, alpha opaque
						dst[i * 4 + 0] = src[i * 4 + 2];
						dst[i * 4 + 1] = src[i * 4 + 1];
						dst[i * 4 + 2] = src[i * 4 + 0];
						dst[i * 4 + 3] = 255;
					}
				}
			}
			SelectObject(hdcMem, old);
			DeleteObject(hbmp);
		}
		DeleteDC(hdcMem);
		ReleaseDC(hwnd, hdcWin);
		return ok;
	}

	bool NkCaptureWholeWindow(NkWindow &win, const char *path) {
		NkImage img;
		if (!NkCaptureWholeWindowToImage(win, img, nullptr, nullptr))
			return false;
		return img.Save(path);
	}

	bool NkCaptureWholeWindowLegacy(NkWindow &win, const char *path) {
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
		bi.bmiHeader.biHeight = -h; // negatif = origine en HAUT (ordre des lignes PNG)
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
#endif

	// Les raccourcis de la modelisation. UNE SEULE table : les menus de la vue,
	// le menu contextuel, la palette de recherche et le panneau T la liront tous.
	// Une liste ecrite deux fois finit toujours par diverger, et c'est
	// l'utilisateur qui le decouvre.
	void FillShortcuts(NkShortcutTable &t) {
		t.Bind("objet.deplacer", "Deplacer", NkKey::NK_G, 0, NK_SCTX_OBJECT);
		t.Bind("objet.tourner", "Tourner", NkKey::NK_R, 0, NK_SCTX_OBJECT);
		t.Bind("objet.echelle", "Redimensionner", NkKey::NK_S, 0, NK_SCTX_OBJECT);
		t.Bind("objet.dupliquer", "Dupliquer", NkKey::NK_D, NK_SC_SHIFT, NK_SCTX_OBJECT);
		t.Bind("objet.supprimer", "Supprimer", NkKey::NK_X, 0, NK_SCTX_OBJECT);
		t.Bind("objet.mode_edition", "Mode edition", NkKey::NK_TAB, 0, NK_SCTX_OBJECT);

		t.Bind("edit.extruder", "Extruder la region", NkKey::NK_E, 0, NK_SCTX_EDIT);
		t.Bind("edit.inserer", "Inserer une face", NkKey::NK_I, 0, NK_SCTX_EDIT);
		t.Bind("edit.biseauter", "Biseauter", NkKey::NK_B, NK_SC_CTRL, NK_SCTX_EDIT);
		t.Bind("edit.fusionner", "Fusionner", NkKey::NK_M, 0, NK_SCTX_EDIT);
		t.Bind("edit.subdiviser", "Subdiviser", NkKey::NK_W, 0, NK_SCTX_EDIT);
		t.Bind("edit.mode_objet", "Mode objet", NkKey::NK_TAB, 0, NK_SCTX_EDIT);

		t.Bind("app.palette", "Rechercher une commande", NkKey::NK_F3, 0, NK_SCTX_GLOBAL);
		t.Bind("app.panneau_outils", "Panneau d'outils", NkKey::NK_T, 0, NK_SCTX_GLOBAL);
		t.Bind("app.annuler", "Annuler", NkKey::NK_Z, NK_SC_CTRL, NK_SCTX_GLOBAL);
		t.Bind("app.refaire", "Refaire", NkKey::NK_Y, NK_SC_CTRL, NK_SCTX_GLOBAL);
		// ENREGISTRER porte sur le FICHIER ACTIF, « tout » sur le projet entier
		// (Rihen : « pourquoi Ctrl+S sur un onglet actif enregistre tous les
		// onglets ? »). Depuis qu'un asset est un fichier, Ctrl+S doit se
		// comporter comme partout ailleurs : il enregistre ce qu'on regarde.
		t.Bind("app.enregistrer", "Enregistrer", NkKey::NK_S, NK_SC_CTRL, NK_SCTX_GLOBAL);
		t.Bind("app.enregistrer_tout", "Enregistrer tout", NkKey::NK_S,
			   NK_SC_CTRL | NK_SC_SHIFT, NK_SCTX_GLOBAL);
	}

} // namespace

int nkmain(const NkEntryState &entry) {
	(void)entry;

	// ── THEMES ──────────────────────────────────────────────────────────────
	NkModelerRoles roles;
	roles.Register();

	NkThemeLibrary themes;
	NkString userThemes;
	if (const char *appdata = env::GetEnvVar("APPDATA")) {
		if (*appdata) {
			userThemes = NkString(appdata);
			userThemes.Append("/NK3DModeler/themes");
		}
	}
	const uint32 fromDisk = LoadThemes(themes, roles, "data/themes", userThemes.CStr());
	themes.SetCurrent("Sombre");

	NkThemeIssue issue{};
	if (const uint32 bad = themes.Current().Validate(&issue)) {
		printf("[theme] %u paire(s) sous le seuil : %s sur %s = %.2f (exige %.1f)\n", bad,
			   NkRoleName(issue.fg), NkRoleName(issue.bg), (double)issue.ratio, (double)issue.required);
	}

	// ── RACCOURCIS ──────────────────────────────────────────────────────────
	NkShortcutTable shortcuts;
	FillShortcuts(shortcuts);
	if (const uint32 c = shortcuts.ConflictCount())
		printf("[raccourcis] %u conflit(s).\n", c);
	printf("[nk3d] %u themes (%u depuis le disque), %u raccourcis.\n", themes.Count(), fromDisk,
		   shortcuts.Count());

	// ── JOURNAL : BRANCHE AVANT TOUT LE RESTE ───────────────────────────────
	// Un puits de plus sur le logger du moteur, qui garde les dernieres lignes
	// en memoire pour le panneau. Installe ICI, le plus tot possible : ce qui
	// est ecrit avant n'existera que dans la console et le fichier, or c'est
	// justement au demarrage -- creation du device, des cibles, chargement des
	// icones -- que se disent les choses qu'on cherche ensuite.
	nk3d::NkJournalInstall();

	// ── FENETRE ─────────────────────────────────────────────────────────────
	// SANS CADRE OS : la maquette porte ses propres boutons de fenetre dans la
	// barre de menus. Garder le cadre natif donnerait deux barres de titre.
	NkWindowConfig wc;
	wc.title = "NK3DModeler";
	wc.width = 1600;
	wc.height = 900;
	wc.minWidth = 1100;
	wc.minHeight = 700;
	wc.centered = true;
	wc.resizable = true;
	wc.frame = false;

	NkWindow window;
	if (!window.Create(wc)) {
		printf("[nk3d] impossible de creer la fenetre.\n");
		return 1;
	}

	// RENDU SUR NKRHI, PAS SUR NKCANVAS. Meme interface NkIEditorRenderer, donc
	// l'interface 2D ne change pas d'une ligne ; ce qui change, c'est que ce
	// renderer-ci EXPOSE SON DEVICE. C'est la condition pour que la vue 3D
	// (NKRenderer) rende dans une cible hors ecran sur le MEME device et que sa
	// texture soit simplement posee dans la draw-list -- au lieu d'ouvrir une
	// seconde pile GPU dans la meme fenetre, ce que le depot interdit
	// explicitement (« une fenetre = une pile »).
	// La scene 3D rend dans sa cible hors ecran sur le command buffer de
	// l'editeur, AVANT que la passe backbuffer ne s'ouvre : on ne peut pas
	// imbriquer une passe de rendu dans une autre. Puis on publie sa texture
	// aupres du backend, pour que la draw-list n'ait plus qu'a la poser.
	// CE QUE LE PANNEAU VEUT VOIR, depose pour le crochet pre-UI. Ces trois
	// valeurs traversent le fichier parce que `preUI3D` est une lambda sans
	// capture (elle est convertie en pointeur de fonction par SetPreUI) : elle
	// ne peut donc rien lire de la boucle. Elles sont reecrites a chaque frame
	// depuis `st`, juste avant le rendu.
	static int32 gPrevSlot = -1, gPrevW = 260, gPrevH = 150;
	static auto preUI3D = [](NkICommandBuffer *cmd, void *user) {
		auto *r = static_cast<nkgui::NkEditorRHIRenderer *>(user);
		// PORTAGE INTEGRAL de --demo=2 : la vue 3D est desormais la demo de
		// renderdemo, portee telle quelle (NkDemo3D.cpp). L'ancienne vue
		// (NkViewport3D) reste compilee mais DORMANTE — on ne lui donne plus
		// de device, donc chacun de ses appels est un no-op sans danger.
		demo::Demo3DHostFrame(cmd);
		// L'APERCU DE MATERIAU rend ici lui aussi : c'est le seul moment ou le
		// command buffer est ouvert ET la passe backbuffer pas encore commencee.
		// Le slot et la taille voulus sont deposes par le panneau dans l'etat --
		// un panneau ne rend rien, il decrit ce qu'il veut voir.
		// APPELEE A CHAQUE FRAME, meme sans materiau affiche (slot = -1) : elle
		// ne fait pas que rendre le grand apercu, elle surveille aussi les
		// reglages et prend les vignettes en attente. Gardee derriere « un
		// materiau est ouvert », rien de tout cela ne tournait hors du panneau --
		// et la vignette semblait attendre l'enregistrement alors qu'elle
		// attendait qu'on revienne dans le materiau (Rihen, 14 aout).
		demo::Demo3DHostMatPreviewFrame(cmd, gPrevSlot, gPrevW, gPrevH);
		demo::Demo3DHostRegisterInto(&r->GetBackend());
	};

	nkgui::NkEditorRHIRenderer renderer;
	if (!renderer.Init(window, NkEditorGfxApi::Auto)) {
		printf("[nk3d] impossible d'initialiser le rendu.\n");
		return 1;
	}

	// ── LA TAILLE DE REFERENCE EST CELLE DU RENDU, PAS CELLE DE LA FENETRE ──
	// C'ETAIT LA CAUSE DU FLOU. J'initialisais l'interface avec la taille
	// DEMANDEE (1600x900) alors que la fenetre reelle fait 1616x939 : la liste de
	// dessin etait projetee dans un repere qui ne correspondait pas au tampon de
	// rendu, et toute l'image se retrouvait reechantillonnee -- texte et icones
	// compris. D'ou le flou uniforme et la fatigue visuelle.
	//
	// On interroge desormais le RENDU et non la fenetre : lui seul sait la taille
	// de son tampon. La fenetre peut compter ses bordures, l'OS peut ajuster, et
	// les deux chiffres divergent sans prevenir.
	// Le device de l'interface EST celui de la vue 3D. C'est toute la raison
	// d'avoir quitte NKCanvas : deux piles GPU dans une fenetre, le depot
	// l'interdit, et une relecture CPU par image serait hors de question.
	// L'ancienne vue ne recoit VOLONTAIREMENT plus le device : c'est ce qui la
	// rend dormante (son Init3D echoue proprement et chaque facade se tait).
	demo::Demo3DHostSetDevice(renderer.GetDevice());
	renderer.SetPreUI(preUI3D, &renderer);

	math::NkVec2u real0 = renderer.Size();
	if (real0.x == 0 || real0.y == 0)
		real0 = window.GetSize(); // repli : mieux vaut une taille approchee qu'une taille nulle
	printf("[nk3d] fenetre %ux%u, tampon de rendu %ux%u\n", window.GetSize().x, window.GetSize().y,
		   real0.x, real0.y);

	nkgui::NkGuiContext ui;
	ui.Init((int32)real0.x, (int32)real0.y);
	// ── PRESSE-PAPIERS OS ───────────────────────────────────────────────────
	// Le contexte GUI delegue le presse-papiers a l'app, et personne ne le
	// cablait ici : Ctrl+C/Ctrl+V dans les champs lisaient un presse-papiers
	// VIDE (constate par Rihen sur les chemins de texture). Meme cablage que
	// NkEditorShell : la fenetre OS fait foi.
	ui.clipboardUser = &window;
	ui.clipboardGetFn = [](void *u, NkString &out) {
		out = static_cast<NkWindow *>(u)->GetClipboardText();
	};
	ui.clipboardSetFn = [](void *u, const char *t) {
		static_cast<NkWindow *>(u)->SetClipboardText(t);
	};

	// ── ECHELLE D'INTERFACE ─────────────────────────────────────────────────
	// Sans elle, sur un ecran a 125 % ou 150 %, Windows ETIRE l'image de la
	// fenetre : tout devient mou. C'est la premiere cause du flou signale.
	float32 uiScale = window.GetDpiScale();
	if (uiScale < 0.5f || uiScale > 4.f)
		uiScale = 1.f; // valeur aberrante : on prefere une interface petite a une interface cassee

	// ── DENSITE D'INTERFACE ─────────────────────────────────────────────────
	// Facteur de LISIBILITE, distinct du DPI, et il a une raison technique.
	//
	// Dans NkGuiDrawList::AddText, le curseur avance de `g->advance`, qui est
	// FRACTIONNAIRE. Seul le PREMIER glyphe d'une chaine tombe donc sur un pixel
	// entier ; les suivants derivent, et chacun echantillonne l'atlas ENTRE deux
	// texels. C'est structurel, et partage avec NKCode -- on ne le corrige pas
	// depuis ici.
	//
	// Mais l'erreur est un demi-texel CONSTANT : a 13 px de corps elle represente
	// 4 % de la hauteur d'un caractere, a 15 px seulement 3,3 %. Grossir le texte
	// ne supprime pas le defaut, il en DIVISE l'effet -- et c'est pour cela que
	// le shell de NKCode charge Inter a 16 px et non a 13. Rihen a donc vu juste
	// en soupconnant l'echelle.
	//
	// 1,15 est un compromis : assez pour que le texte se pose, pas au point de
	// faire perdre deux lignes a chaque panneau. Ce sera le curseur « densite »
	// des reglages.
	const float32 kUiZoom = 1.15f;
	const float32 total = uiScale * kUiZoom;
	ApplyUiScale(total);
	printf("[nk3d] echelle : DPI %.2f x densite %.2f = %.2f\n", (double)uiScale, (double)kUiZoom,
		   (double)total);

	nkgui::NkGuiFont font;
	// La taille de corps est ARRONDIE : une police demandee a 16,25 px produit
	// des metriques fractionnaires, donc des lignes de base entre deux pixels.
	const float32 fontPx = (float32)(int32)(13.f * total + 0.5f);
	if (!font.LoadEmbedded(NkEmbeddedFontId::Inter, fontPx)) {
		printf("[nk3d] police introuvable.\n");
		return 1;
	}
	printf("[nk3d] police Inter a %.0f px.\n", (double)fontPx);
	ui.font = &font;
	// L'atlas de glyphes doit etre televerse AVANT la premiere frame, sinon le
	// texte sort en rectangles vides -- symptome classique et deroutant.
	renderer.UploadFontGray8(font.TexId(), font.pixels, font.atlasW, font.atlasH);

	// ── ICONES ──────────────────────────────────────────────────────────────
	// Apres la police : leurs identifiants de texture partent APRES celui de
	// l'atlas de glyphes, sinon la premiere icone ecraserait la police.
	NkModelerIcons icons;
	icons.Load(renderer, font.TexId() + 16u, (int32)(16.f * total + 0.5f));
	printf("[nk3d] %u icones chargees.\n", icons.LoadedCount());

	// VIGNETTES DES MATCAPS (ids 4300+) : la bibliotheque genere chaque
	// boule en pixels, et on l'uploade comme n'importe quelle icone. C'est
	// l'APERCU REEL du selecteur -- un pictogramme generique ne dit pas
	// quelle matiere on choisit.
	{
		// 128 px, pas 32 : le grand apercu du panneau fait 72 px, et AGRANDIR
		// pixelise (constate par Rihen) -- on genere plus grand que le plus
		// grand usage ; le retrecissement, lui, reste lisse.
		static uint8 ball[128 * 128 * 4];
		const int32 nMc = demo::Demo3DHostMatcapCount();
		for (int32 i = 0; i < nMc; ++i) {
			demo::Demo3DHostMatcapBall(i, ball, 128);
			renderer.UploadImageRGBA(4300u + (uint32)i, ball, 128, 128);
		}
		printf("[nk3d] %d vignettes de matcap (128 px).\n", nMc);
	}

	// ── ETAT DE SESSION ─────────────────────────────────────────────────────
	NkModelerState st;
	NkHitRegistry hit;
	NkWidgetState ws;
	NkComboPending combo;
	NkCheckPending checks;
	static const char *const kScenes[] = {"Scene_01", "Scene_02"};

	// ── PROJET ET PROJETS RECENTS ───────────────────────────────────────────
	// Aucun projet n'est ouvert au lancement : l'ecran d'accueil est donc
	// affiche, et il l'est tant que `st.welcome` reste vrai. Les recents sont
	// lus depuis ~/.nk3dmodeler_recent.cfg (meme patron que l'IDE frere NKCode).
	nk3d::NkProjectState proj;
	nk3d::NkRecentList recents;
	// Surveillance du dossier du projet. Vit AUSSI LONGTEMPS que la boucle : son
	// fil est arrete par Stop(), et le laisser mourir avant lui laisserait un fil
	// pointant sur un objet detruit.
	nk3d::NkProjectWatch projWatch;
	// IMAGE DE LA VERSION (façon Blender, decision de Rihen) : une oeuvre
	// realisee avec le logiciel, creditee, livree dans data/splash/. Chargee
	// paresseusement a la premiere frame d'accueil.
	nk3d::NkSplashArt splashArt;
	recents.Load();
	printf("[nk3d] %d projet(s) recent(s).\n", (int)recents.items.Size());

	// ── CROCHETS D'AGENT (verification headless, autorisee par Rihen le
	// 9 aout : « tester avec des scripts ou lancer directement l'application,
	// faire des captures et analyser ») ────────────────────────────────────
	//   NK_OPEN_RECENT=<i> : ouvre le i-eme projet recent au demarrage, par le
	//     MEME point de passage que le double-clic de l'accueil (action 7 de
	//     NkProjectHandlePending) — aucun second chemin d'ouverture.
	//   NK_AGENT_SHOT=<n>  : a la frame n, declenche la capture « tutoriel »
	//     (toute la fenetre, PNG numerote) — celle des boutons du bas.
	//   NK_AGENT_EXIT=<n>  : a la frame n, quitte proprement (st.running).
	// Ces crochets ne font qu'ARMER des etats que l'interface arme deja : si
	// personne ne les pose, rien ne change.
	int32 agentShotFrame = -1, agentExitFrame = -1, agentOpenRecent = -1;
	{
		if (const char *v = std::getenv("NK_OPEN_RECENT")) {
			const int32 idx = (int32)std::atoi(v);
			if (idx >= 0 && (usize)idx < recents.items.Size()) {
				// PAS tout de suite : ouvrir a la frame 0 fige l'application —
				// la restitution appelle l'hote 3D, qui ne nait qu'au premier
				// PAINT du viewport. On attend donc qu'il soit pret (boucle),
				// comme le fait de facto un clic humain sur l'accueil.
				agentOpenRecent = idx;
			} else {
				printf("[nk3d] NK_OPEN_RECENT=%d hors bornes (%d recents)\n", idx,
					   (int)recents.items.Size());
			}
		}
		if (const char *v = std::getenv("NK_AGENT_SHOT"))
			agentShotFrame = (int32)std::atoi(v);
		if (const char *v = std::getenv("NK_AGENT_EXIT"))
			agentExitFrame = (int32)std::atoi(v);
	}
	int32 agentFrame = 0;

	// ── ENTREE SOURIS ───────────────────────────────────────────────────────
	// NKGui calcule les TRANSITIONS (clic, relachement, double-clic) dans
	// BeginFrame a partir de l'etat BRUT que l'application pose ici. On se
	// contente donc de reporter les evenements ; c'est BeginFrame qui en tire
	// « vient d'etre clique ».
	{
		auto &ev = NkEvents();
		ev.AddEventCallback<NkMouseMoveEvent>([&ui](NkMouseMoveEvent *e) {
			ui.input.mousePos = {(float32)e->GetX(), (float32)e->GetY()};
		});
		ev.AddEventCallback<NkMouseButtonPressEvent>([&ui](NkMouseButtonPressEvent *e) {
			const NkMouseButton b = e->GetButton();
			if (b == NkMouseButton::NK_MB_LEFT)
				ui.input.mouseDown[0] = true;
			else if (b == NkMouseButton::NK_MB_RIGHT)
				ui.input.mouseDown[1] = true;
			else if (b == NkMouseButton::NK_MB_MIDDLE)
				ui.input.mouseDown[2] = true;
			ui.input.ctrlDown = e->GetModifiers().ctrl;
			ui.input.shiftDown = e->GetModifiers().shift;
			ui.input.altDown = e->GetModifiers().alt;
		});
		ev.AddEventCallback<NkMouseButtonReleaseEvent>([&ui](NkMouseButtonReleaseEvent *e) {
			const NkMouseButton b = e->GetButton();
			if (b == NkMouseButton::NK_MB_LEFT)
				ui.input.mouseDown[0] = false;
			else if (b == NkMouseButton::NK_MB_RIGHT)
				ui.input.mouseDown[1] = false;
			else if (b == NkMouseButton::NK_MB_MIDDLE)
				ui.input.mouseDown[2] = false;
		});
		// La molette s'ACCUMULE : plusieurs crans peuvent arriver dans la meme
		// frame, et n'en garder qu'un rendrait le defilement saccade.
		// ── CLAVIER ─────────────────────────────────────────────────────────
		// Une quinzaine de fonctions de la vue 3D etaient ecrites mais DORMANTES :
		// aucun appelant. Le clavier est leur premier chemin d'acces -- les menus
		// et la palette suivront, alimentes par la meme table de raccourcis.
		//
		// Le clavier de Blender, parce que c'est celui que connaissent les gens qui
		// modelisent. Les touches ne sont PAS ecrites en dur ailleurs : cette table
		// est le seul endroit ou l'on decide « quelle touche fait quoi ».
		//
		// Les evenements arrivent HORS de la frame, donc on ne touche pas au
		// maillage ici : on pose une intention, consommee dans la boucle. Modifier
		// la geometrie depuis un callback reentrerait dans une image en cours de
		// peinture, avec des tampons a moitie ecrits.
		// ── SAISIE DE TEXTE ─────────────────────────────────────────────────
		// RIEN DE TOUT CECI N'ETAIT BRANCHE. NkGuiInput expose PushChar et SetKey,
		// mais l'application ne les appelait jamais : les champs de saisie ne
		// recevaient donc aucun caractere, aucune touche Entree, aucun Echap. D'ou
		// « impossible de renommer » ET « impossible de fermer l'editeur » -- ce
		// n'etait pas deux bugs mais un seul, en amont de tout le reste.
		ev.AddEventCallback<NkTextInputEvent>([&ui](NkTextInputEvent *e) {
			ui.input.PushChar(e->GetCodepoint());
		});
		// Les touches d'EDITION ont leur propre table dans NKGui, distincte des
		// raccourcis de l'application : c'est ce qui permet a Entree de valider un
		// nom sans declencher aussi une commande.
		ev.AddEventCallback<NkKeyPressEvent>([&ui](NkKeyPressEvent *e) {
			// ── COPIER / COUPER / COLLER / TOUT SELECTIONNER ────────────────
			// La MEME mecanique que NkEditorShell (NKCode, regle de Rihen) :
			// le callback leve les drapeaux, les champs de saisie les lisent,
			// la fenetre OS porte le presse-papiers. Personne ne les levait
			// ici : Ctrl+V ne faisait RIEN dans les champs, chemins compris.
			{
				const auto m0 = e->GetModifiers();
				ui.input.ctrlDown = m0.ctrl;
				ui.input.shiftDown = m0.shift;
				ui.input.altDown = m0.alt;
				if (m0.ctrl) {
					const NkKey k0 = e->GetKey();
					if (k0 == NkKey::NK_C)
						ui.input.wantCopy = true;
					else if (k0 == NkKey::NK_X)
						ui.input.wantCut = true;
					else if (k0 == NkKey::NK_V)
						ui.input.wantPaste = true;
					else if (k0 == NkKey::NK_A)
						ui.input.wantSelectAll = true;
				}
			}
			switch (e->GetKey()) {
				case NkKey::NK_ENTER:
				case NkKey::NK_NUMPAD_ENTER:
					ui.input.SetKey(nkgui::NkGuiKey::Enter, true);
					break;
				case NkKey::NK_ESCAPE:
					ui.input.SetKey(nkgui::NkGuiKey::Escape, true);
					break;
				case NkKey::NK_BACK:
					ui.input.SetKey(nkgui::NkGuiKey::Backspace, true);
					break;
				case NkKey::NK_DELETE:
					ui.input.SetKey(nkgui::NkGuiKey::Delete, true);
					break;
				case NkKey::NK_LEFT:
					ui.input.SetKey(nkgui::NkGuiKey::Left, true);
					break;
				case NkKey::NK_RIGHT:
					ui.input.SetKey(nkgui::NkGuiKey::Right, true);
					break;
				// Raccourcis de scene (les CARACTERES n'arrivent pas toujours
				// hors saisie : on mappe les TOUCHES, constate par Rihen).
				case NkKey::NK_D:
					ui.input.SetKey(nkgui::NkGuiKey::D, true);
					break;
				case NkKey::NK_X:
					ui.input.SetKey(nkgui::NkGuiKey::X, true);
					break;
				case NkKey::NK_P:
					ui.input.SetKey(nkgui::NkGuiKey::P, true);
					break;
				case NkKey::NK_C:
					ui.input.SetKey(nkgui::NkGuiKey::C, true);
					break;
				case NkKey::NK_V:
					ui.input.SetKey(nkgui::NkGuiKey::V, true);
					break;
				default:
					break;
			}
		});
		ev.AddEventCallback<NkKeyReleaseEvent>([&ui](NkKeyReleaseEvent *e) {
			switch (e->GetKey()) {
				case NkKey::NK_ENTER:
				case NkKey::NK_NUMPAD_ENTER:
					ui.input.SetKey(nkgui::NkGuiKey::Enter, false);
					break;
				case NkKey::NK_ESCAPE:
					ui.input.SetKey(nkgui::NkGuiKey::Escape, false);
					break;
				case NkKey::NK_BACK:
					ui.input.SetKey(nkgui::NkGuiKey::Backspace, false);
					break;
				case NkKey::NK_DELETE:
					ui.input.SetKey(nkgui::NkGuiKey::Delete, false);
					break;
				case NkKey::NK_LEFT:
					ui.input.SetKey(nkgui::NkGuiKey::Left, false);
					break;
				case NkKey::NK_RIGHT:
					ui.input.SetKey(nkgui::NkGuiKey::Right, false);
					break;
				case NkKey::NK_D:
					ui.input.SetKey(nkgui::NkGuiKey::D, false);
					break;
				case NkKey::NK_X:
					ui.input.SetKey(nkgui::NkGuiKey::X, false);
					break;
				case NkKey::NK_P:
					ui.input.SetKey(nkgui::NkGuiKey::P, false);
					break;
				case NkKey::NK_C:
					ui.input.SetKey(nkgui::NkGuiKey::C, false);
					break;
				case NkKey::NK_V:
					ui.input.SetKey(nkgui::NkGuiKey::V, false);
					break;
				default:
					break;
			}
		});

		ev.AddEventCallback<NkKeyPressEvent>([&st](NkKeyPressEvent *e) {
			const NkKey k = e->GetKey();
			const auto mods = e->GetModifiers();
			const bool ctrl = mods.ctrl, shift = mods.shift, alt = mods.alt;
			// La saisie d'un nom en cours capte TOUT : taper « e » dans un champ ne
			// doit pas extruder. C'est le premier reflexe a avoir des qu'un
			// raccourci d'une seule lettre existe.
			// L'ECRAN D'ACCUEIL capte de la meme facon : aucun raccourci de scene
			// ne doit agir sur un document qu'on n'a pas encore ouvert.
			if (st.editingText || st.welcome)
				return;
			auto want = [&st](NkVpAction a) { st.pendingAction = a; };

			switch (k) {
				// ── Modes ───────────────────────────────────────────────────
				case NkKey::NK_TAB:
					want(NkVpAction::ToggleEdit);
					break;
				case NkKey::NK_NUM1:
					want(NkVpAction::SubModeVertex);
					break;
				case NkKey::NK_NUM2:
					want(NkVpAction::SubModeEdge);
					break;
				case NkKey::NK_NUM3:
					want(NkVpAction::SubModeFace);
					break;
				// ── Selection ───────────────────────────────────────────────
				case NkKey::NK_A:
					want(alt ? NkVpAction::SelectNone : NkVpAction::SelectAll);
					break;
				// ── Outils de transformation ────────────────────────────────
				// G / R / S ARMENT UNE TRANSFORMATION, ils ne changent pas d'outil.
				// C'est le geste de Blender : la touche saisit l'objet, la souris le
				// pilote, X / Y / Z contraignent, le clic confirme et Echap annule.
				// Le choisir plutot que « selectionner l'outil » n'est pas un detail :
				// il n'y a aucune poignee a viser, donc rien a rater.
				case NkKey::NK_G:
					want(NkVpAction::ModalMove);
					break;
				case NkKey::NK_R:
					want(ctrl ? NkVpAction::LoopCut : NkVpAction::ModalRotate);
					break;
				case NkKey::NK_S:
					// Ctrl+S ENREGISTRE -- le reflexe universel passe AVANT le
					// raccourci local (constate par Rihen : Ctrl+S armait
					// l'echelle au lieu de sauver). S seul arme l'echelle, comme
					// chez Blender. Meme motif que R : ctrl ? LoopCut : Rotate.
					//
					// Ctrl+S = le FICHIER ACTIF ; Ctrl+Maj+S = TOUT le projet.
					if (ctrl)
						st.projPending = shift ? 8 : 3;
					else
						want(NkVpAction::ModalScale);
					break;
				// ── Operations ──────────────────────────────────────────────
				case NkKey::NK_E:
					want(shift ? NkVpAction::ExtrudeIndividual : NkVpAction::Extrude);
					break;
				case NkKey::NK_X:
					// Pendant une modale, X contraint a l'axe ; sinon il supprime.
					// L'intention posee est « axe X », et le dispatch la reinterprete
					// en suppression s'il n'y a pas de modale en cours.
					want(ctrl ? NkVpAction::Dissolve : NkVpAction::ModalAxisX);
					break;
				case NkKey::NK_M:
					want(NkVpAction::Merge);
					break;
				case NkKey::NK_F:
					want(NkVpAction::MakeFace);
					break;
				case NkKey::NK_W:
					want(NkVpAction::Subdivide);
					break;
				case NkKey::NK_I:
					want(NkVpAction::Inset);
					break;
				case NkKey::NK_B:
					// Ctrl+B biseaute, B seul arme la selection RECTANGLE -- c'est le
					// clavier de Blender, ou B veut dire « box select ».
					want(ctrl ? (shift ? NkVpAction::BevelVertex : NkVpAction::BevelEdge)
							  : NkVpAction::ZoneRect);
					break;
				case NkKey::NK_C:
					// C arme la selection CERCLE (peinture) ; la molette en regle le
					// rayon pendant le geste.
					want(NkVpAction::ZoneCircle);
					break;
				// ── Annulation ──────────────────────────────────────────────
				case NkKey::NK_Z:
					if (ctrl)
						want(shift ? NkVpAction::Redo : NkVpAction::Undo);
					else if (alt)
						want(NkVpAction::ToggleXray);
					else
						want(NkVpAction::ModalAxisZ); // contrainte, si une modale court
					break;
				// ── Contraintes d'axe et fin de transformation ──────────────
				// Ces touches N'ONT DE SENS QUE pendant une modale ; hors modale,
				// elles retombent sur leur role habituel (X = supprimer). C'est le
				// dispatch qui tranche, pas le callback : lui ne connait pas l'etat
				// de la vue.
				case NkKey::NK_Y:
					want(ctrl ? NkVpAction::Redo : NkVpAction::ModalAxisY);
					break;
				case NkKey::NK_ESCAPE:
					want(NkVpAction::ModalCancel);
					break;
				case NkKey::NK_ENTER:
					want(NkVpAction::ModalConfirm);
					break;

				// ── Vues du pave numerique ──────────────────────────────────
				// Ctrl donne la vue OPPOSEE, comme chez Blender : c'est deux fois
				// moins de touches a retenir pour six vues.
				case NkKey::NK_NUMPAD_1:
					want(ctrl ? NkVpAction::ViewBack : NkVpAction::ViewFront);
					break;
				case NkKey::NK_NUMPAD_3:
					want(ctrl ? NkVpAction::ViewLeft : NkVpAction::ViewRight);
					break;
				case NkKey::NK_NUMPAD_7:
					want(ctrl ? NkVpAction::ViewBottom : NkVpAction::ViewTop);
					break;
				case NkKey::NK_NUMPAD_5:
					want(NkVpAction::ToggleOrtho);
					break;
				case NkKey::NK_NUMPAD_DOT:
					want(NkVpAction::FrameAll);
					break;
				case NkKey::NK_HOME:
					want(NkVpAction::FrameAll);
					break;
				default:
					break;
			}
		});

		ev.AddEventCallback<NkMouseDoubleClickEvent>([&ui](NkMouseDoubleClickEvent *e) {
			const NkMouseButton b = e->GetButton();
			ui.input.SetDoubleClick(b == NkMouseButton::NK_MB_LEFT
										? 0
										: (b == NkMouseButton::NK_MB_RIGHT ? 1 : 2));
		});

		ev.AddEventCallback<NkMouseWheelVerticalEvent>(
			[&ui](NkMouseWheelVerticalEvent *e) { ui.input.wheel += (float32)e->GetDeltaY(); });
		// La croix de l'OS ne TUE plus l'application : elle DEMANDE la fermeture,
	// que la peinture arbitre (document modifie ? prise ou encodage video en
	// cours ?) -- exactement comme la croix dessinee et le menu Quitter.
	ev.AddEventCallback<NkWindowCloseEvent>([&st](NkWindowCloseEvent *) { st.wantClose = true; });
	}

	NkClock clock;
	uint32 lastW = real0.x, lastH = real0.y;

	// ── BOUCLE ──────────────────────────────────────────────────────────────
	while (st.running && window.IsOpen()) {
		while (NkEvent *ev = NkEvents().PollEvent()) {
			(void)ev;
		}

		// ── FENETRE MINIMISEE : ON NE FAIT RIEN DU TOUT ─────────────────────
		// Une fenetre reduite n'a plus de surface. Continuer a rendre dessus --
		// et surtout a reconstruire la swapchain et le graphe de rendu -- tuait
		// l'application (Rihen). On rend la main a l'OS et on repart au debut de
		// la boucle : les evenements continuent d'etre depiles, donc la fenetre
		// se restaure normalement.
		// C'est la SURFACE qu'il faut interroger, pas GetSize() : une fenetre
		// reduite garde sa taille logique -- Windows la conserve pour la
		// restauration -- alors que sa surface de rendu tombe a zero. Tester
		// GetSize() ne detectait donc jamais la minimisation, et l'application
		// continuait a rendre puis mourait.
		{
			// L'ETAT MINIMISE se demande a l'OS (IsIconic), PAS a la taille :
			// une fenetre reduite garde un rect de placeholder (~160x28 sous
			// Windows), jamais nul -- la garde par taille ne declenchait pas,
			// ce rect partait en ResizeSwapchain, une cible divisee (bloom /32)
			// tombait a zero et CreateTexture2D echouait : mort a la
			// restauration (defaut 4.3, reproduit par messages systeme).
			const NkSurfaceDesc surf0 = window.GetSurfaceDesc();
			if (window.IsMinimized() || surf0.width == 0 || surf0.height == 0) {
				NkClock::SleepMilliseconds(8);
				continue;
			}
		}
		// On previent le rendu du changement, PUIS on relit SA taille : c'est elle
		// qui sert a projeter, pas celle qu'on vient de lui donner.
		const math::NkVec2u winSz = window.GetSize();
		if (winSz.x > 0 && winSz.y > 0 && (winSz.x != lastW || winSz.y != lastH)) {
			renderer.OnResize(winSz.x, winSz.y);
			const math::NkVec2u rs = renderer.Size();
			lastW = rs.x > 0 ? rs.x : winSz.x;
			lastH = rs.y > 0 ? rs.y : winSz.y;
		}

		float32 dt = clock.Tick().delta;
		if (dt <= 0.f || dt > 0.1f)
			dt = 1.f / 60.f;

		// LA TAILLE DE VUE SUIT LA FENETRE. `NkGuiContext::Init` la pose une fois
		// et ne la revoit jamais : apres un redimensionnement, tout composant qui
		// s'appuie sur `viewW/viewH` (les modales de NKEditorKit, qui s'y centrent
		// et y etendent leur voile) travaille sur les dimensions du DEMARRAGE --
		// voile tronque, dialogue decentre (Rihen, 12 aout).
		ui.viewW = (int32)lastW;
		ui.viewH = (int32)lastH;
		ui.BeginFrame(dt);
		// Le registre est reinitialise APRES BeginFrame : il lit les transitions
		// que celui-ci vient de calculer.
		// Memorise AVANT le Begin (qui reinitialise le registre) : ce que la
		// souris survolait a l'image precedente. Le pilotage du gizmo en a besoin
		// pour distinguer « clic sur la scene » de « clic sur un widget pose
		// par-dessus la scene ».
		// ... et JAMAIS quand la souris est sur une SURCOUCHE BLOQUANTE (badge
		// vue camera, listes posees sur la vue) : sans ce garde, leurs clics
		// TRAVERSAIENT jusqu'a la scene -- selection/deselection fantomes
		// (constate par Rihen ; meme patron que NKCode, via SetBlock).
		const bool overSceneLastFrame = hit.IsHovered("view.nav") && !hit.BlockedAtMouse();
		// ── UNE MODALE SUSPEND L'APPLICATION, POUR DE BON ───────────────────
		// Les couches du registre suffisent aux widgets qui passent par lui,
		// mais beaucoup de code -- la vue 3D, les glissements, les menus
		// contextuels -- lit l'input DIRECTEMENT. Tant qu'une modale est
		// ouverte, on prive donc les panneaux de tout evenement a la source :
		// c'est le seul endroit ou l'etancheite vaut partout a la fois. L'input
		// reel est rendu juste avant de peindre les surcouches, qui, elles,
		// doivent repondre.
		const nkgui::NkGuiInput inputReel = ui.input;
		// L'ECRAN D'ACCUEIL EST UNE MODALE, et c'est ce qui le rend etanche sans
		// demonter la boucle : il recouvre l'application, donc l'application ne
		// doit plus recevoir un seul evenement. Le mecanisme existait deja pour
		// le picker de couleur -- on ne lui en ajoute pas un second.
		// Le selecteur de fichiers de NKEditorKit est une modale de plein droit :
		// il rejoint donc CE mecanisme plutot que d'en amener un second (ce que
		// j'avais fait -- un SetBlock a part -- et qui l'empechait de repondre).
		const bool modalOpen = (st.colorOpen[0] != 0) || st.welcome ||
							   st.picker.pickerOpen || st.matAddOpen;
		if (modalOpen) {
			for (int32 b = 0; b < 3; ++b) {
				ui.input.mouseDown[b] = false;
				ui.input.mouseClicked[b] = false;
				ui.input.mouseReleased[b] = false;
				ui.input.mouseDoubleClicked[b] = false;
			}
			ui.input.wheel = 0.f;
			ui.input.wheelH = 0.f;
			ui.input.charCount = 0;
			for (int32 k = 0; k < nkgui::NkGuiInput::KeyCount; ++k) {
				ui.input.keyDown[k] = false;
				ui.input.keyInit[k] = false;
			}
		}
		hit.Begin(ui.input);
		// L'emprise des surfaces flottantes de la frame precedente devient celle
		// que TOUT LE MONDE consulte cette frame -- registre et code direct.
		hit.FlipOcclusions();
		// LE CONTEXTE DE LA FRAME, pose une fois : les widgets partages (la
		// saisie universelle de NKEditorKit) le lisent ici au lieu de le
		// recevoir en parametre dans des dizaines de signatures.
		NkUiCtx() = &ui;
		// L'emprise des menus de la frame PRECEDENTE devient la garde de
		// celle-ci : les panneaux sont peints avant les menus, ils ne peuvent
		// pas connaitre leur emprise autrement.
		st.UiBlockFlip();
		// L'ANCIENNE GARDE (SetBlock) EST RETIREE : le routeur d'occlusion la
		// remplace entierement, et faire cohabiter deux mecanismes etait
		// precisement le defaut -- la garde bloquait les menus qu'elle etait
		// censee proteger, si bien que « Creer » refusait ses propres clics.
		// La garde du clavier suit l'etat REEL des widgets : tant qu'un champ est
		// en cours de saisie, aucune touche ne doit atteindre les raccourcis.
		st.editingText = ws.editing;
		// Relu CHAQUE frame et non seulement apres notre bouton : l'utilisateur peut
		// maximiser par double-clic sur la barre, par raccourci Windows ou en glissant
		// la fenetre en haut de l'ecran. L'icone doit suivre dans tous les cas.
		st.maximized = window.IsMaximized();

		const float32 W = (float32)lastW, H = (float32)lastH;
		NkLayout lay;
		lay.Compute(W, H, st.leftFrac, st.rightFrac, st.browserFrac, st.propsFrac, st.showLeft,
					st.showRight, st.showBrowser);

		// Bornes des panneaux deroulants pour cette image.
		NkPopupBoundsW() = W;
		NkPopupBoundsH() = H;

		// ── L'INTERFACE PILOTE LA VUE 3D ────────────────────────────────────
		// Tout descend ici, AVANT la peinture : la barre de la vue est lue a
		// l'image N et appliquee a l'image N. L'inverse -- appliquer apres avoir
		// peint -- ferait toujours voir l'etat precedent, ce qui donne une
		// interface qui « repond en retard » sans qu'on sache pourquoi.
		nk3d::Viewport3DSetShading(st.shading, st.solidLight);
		nk3d::Viewport3DSetOverlays(st.overlayMask);
		nk3d::Viewport3DResize((uint32)lay.view.w, (uint32)lay.view.h);
		// La demo portee recoit la taille de la vue, son origine (traduction
		// souris fenetre -> vue), le survol (ses raccourcis n'ecoutent que la
		// vue survolee, comme Blender) et la garde de saisie de texte.
		// AUCUNE pastille de proprietes active : le panneau se REPLIE sur sa
		// colonne de pastilles et la VUE recupere la place.
		if (st.showRight && !st.AnyPropOpen()) {
			const float32 tabW = S(28.f);
			const float32 give = lay.propsR.w - tabW;
			if (give > 0.f) {
				lay.view.w += give;
				lay.propsR.x += give;
				lay.propsR.w = tabW;
				lay.detailsR.x += give;
				lay.detailsR.w = tabW;
			}
		}
		// La vue REELLE vit SOUS la barre d'espaces : taille et origine de la
		// souris doivent viser la meme zone que l'image, sinon le picking
		// decale d'une hauteur de barre.
		{
			NkRect viewImg = lay.view;
			if (st.wsBarOpen) {
				viewImg.y += S(24.f);
				viewImg.h -= S(24.f);
			}
			demo::Demo3DHostResize((uint32)viewImg.w, (uint32)viewImg.h);
			// PENDANT L'ACCUEIL, LA SCENE EST SOURDE. La vue 3D lit l'input
			// DIRECTEMENT (elle ne passe pas par le registre de zones) : sans ce
			// garde, cliquer une carte de projet recent selectionnerait aussi un
			// objet derriere l'ecran, et taper un nom de projet extruderait un
			// maillage. Meme raisonnement que `st.editingText`.
			demo::Demo3DHostSetView(viewImg.x, viewImg.y, overSceneLastFrame && !st.welcome,
									!st.editingText && !st.welcome);
		}

		// ── SYNCHRONISATION UI <-> DEMO PORTEE ──────────────────────────────
		// POUSSER quand l'interface a change depuis l'image precedente, TIRER
		// sinon : les raccourcis de la demo (Z, virgule, pave numerique,
		// Shift+TAB...) restent maitres et l'interface les REFLETE, au lieu de
		// les ecraser chaque image. L'etat « derniere valeur vue » vit ici.
		if (demo::Demo3DHostReady()) {
			static struct {
					int32 shading = -1, solidLight = -1, projection = -1, orientation = -1,
						  camSpeed = -1, gizmoOp = -1;
					uint32 overlay = 0xFFFFFFFFu;
					NkTool tool = (NkTool)255;
					bool snapGrid = false, snapAngle = false, snapScale = false;
					bool first = true;
			} sy;

			// Ombrage (les 6 modes reels : l'index de la liste EST le mode).
			if (!sy.first && st.shading != sy.shading)
				demo::Demo3DHostSetShading(st.shading);
			else
				st.shading = demo::Demo3DHostShading();
			sy.shading = st.shading;

			// Source de couleur des modes non eclaires (touche B de la demo).
			if (!sy.first && st.solidLight != sy.solidLight)
				demo::Demo3DHostSetUnlitColor(st.solidLight);
			else
				st.solidLight = demo::Demo3DHostUnlitColor();
			sy.solidLight = st.solidLight;

			// Projection : 0 perspective, 1 orthogonale, 2..7 vues d'axe. Une vue
			// d'axe est une ACTION (elle pose la camera) ; l'etat durable, c'est
			// ortho/perspective.
			if (!sy.first && st.projection != sy.projection) {
				if (st.projection == 0)
					demo::Demo3DHostSetOrtho(false);
				else if (st.projection == 1)
					demo::Demo3DHostSetOrtho(true);
				else {
					// Dessus/Dessous, Avant/Arriere, Gauche/Droite.
					static const int32 kWhich[6] = {2, 2, 0, 0, 1, 1};
					static const bool kOpp[6] = {false, true, false, true, true, false};
					demo::Demo3DHostAxisView(kWhich[st.projection - 2], kOpp[st.projection - 2]);
				}
			} else if (!demo::Demo3DHostIsOrtho()) {
				st.projection = 0;
			} else if (st.projection == 0) {
				st.projection = 1;
			}
			sy.projection = st.projection;
			st.lastProjection = st.projection;

			// Orientation du gizmo (monde / local / normale).
			if (!sy.first && st.orientation != sy.orientation)
				demo::Demo3DHostSetOrientation(st.orientation);
			else
				st.orientation = demo::Demo3DHostOrientation();
			sy.orientation = st.orientation;

			// Outils. Deplacer/Rotation/Echelle/Multigizmo = les 4 modes du gizmo
			// de la demo ; Selection et Curseur sont des outils du shell qui
			// s'appuient sur ses mecanismes (zones, curseur 3D).
			const int32 opNow = demo::Demo3DHostGizmoOp();
			if (!sy.first && st.tool != sy.tool) {
				if ((int32)st.tool >= (int32)NkTool::Move)
					demo::Demo3DHostSetGizmoOp((int32)st.tool - (int32)NkTool::Move);
			} else if (opNow != sy.gizmoOp && (int32)st.tool >= (int32)NkTool::Move) {
				// G/R/S/C presses dans la vue : l'outil de la barre suit.
				st.tool = (NkTool)((int32)NkTool::Move + opNow);
			}
			sy.gizmoOp = demo::Demo3DHostGizmoOp();
			sy.tool = st.tool;
			demo::Demo3DHostSetCursorTool(st.tool == NkTool::Cursor);
			demo::Demo3DHostSetZoneTool(st.tool == NkTool::Select ? st.selShape : -1);
			demo::Demo3DHostSetGizmoHidden(st.tool == NkTool::Select || st.tool == NkTool::Cursor);

			// Vitesse de camera : 1x / 2x / 4x / 8x.
			if (st.camSpeed != sy.camSpeed) {
				demo::Demo3DHostSetCamSpeed((float32)(1 << st.camSpeed));
				sy.camSpeed = st.camSpeed;
			}

			// Aimantation : les pas sont FIXES (0,5 / 15 deg / 0,1) et l'etat du
			// gizmo est GLOBAL -> la bascule appliquee est celle du mode courant.
			{
				const bool changed = st.snapGrid != sy.snapGrid || st.snapAngle != sy.snapAngle ||
									 st.snapScale != sy.snapScale;
				bool *cur = &st.snapGrid;
				if (opNow == 1)
					cur = &st.snapAngle;
				else if (opNow == 2)
					cur = &st.snapScale;
				if (!sy.first && changed)
					demo::Demo3DHostSetSnap(*cur, 0.5f, 15.f, 0.1f);
				else
					*cur = demo::Demo3DHostSnapEnabled(); // Shift+TAB dans la vue
				sy.snapGrid = st.snapGrid;
				sy.snapAngle = st.snapAngle;
				sy.snapScale = st.snapScale;
			}

			// ── LES NOMS DE CAMERA DESCENDENT VERS L'HOTE ───────────────────
			// C'est lui qui ecrit les fichiers de sortie, et il ne connait que
			// des numeros de noeud : sans ce depot, une miniature sortait en
			// « cam2 » au lieu de « Camera.002 ». Ce depot vivait dans le
			// panneau Output -- il fallait donc l'avoir ouvert au moins une fois
			// pour que les noms soient justes, ce qui est une condition qu'on ne
			// devine pas. Il se fait desormais dans la synchronisation
			// generale : les noms sont a jour quoi qu'on ait ouvert, et
			// renommer une camera renomme les prochains fichiers.
			{
				int32 camN[16];
				const int32 nC = demo::Demo3DHostSceneCameras(camN, 16);
				for (int32 c = 0; c < nC; ++c) {
					char cn[32] = {};
					nk3d::NkHierNodeName(st, camN[c], cn, sizeof(cn));
					demo::Demo3DHostSetNodeLabel(camN[c], cn);
				}
			}

			// Surimpressions : grille et ses traits (F1..F4), lisere, HUD, et le
			// CURSEUR 3D (bit 64) -- un repere de travail qu'on doit pouvoir
			// eteindre sans renoncer a l'outil qui le place (Rihen).
			if (!sy.first && st.overlayMask != sy.overlay) {
				demo::Demo3DHostSetGridFlags((st.overlayMask & 1u) != 0u, (st.overlayMask & 2u) != 0u,
											 (st.overlayMask & 4u) != 0u, (st.overlayMask & 8u) != 0u);
				demo::Demo3DHostSetOutline((st.overlayMask & 16u) != 0u);
				demo::Demo3DHostSetHud((st.overlayMask & 32u) != 0u);
				demo::Demo3DHostSetCursorShown((st.overlayMask & 64u) != 0u);
			} else {
				bool g0, g1, g2, g3;
				demo::Demo3DHostGridFlags(&g0, &g1, &g2, &g3);
				st.overlayMask = (g0 ? 1u : 0u) | (g1 ? 2u : 0u) | (g2 ? 4u : 0u) | (g3 ? 8u : 0u) |
								 (demo::Demo3DHostOutline() ? 16u : 0u) |
								 (demo::Demo3DHostHud() ? 32u : 0u) |
								 (demo::Demo3DHostCursorShown() ? 64u : 0u);
			}
			sy.overlay = st.overlayMask;

			// Sous-mode : refleter le masque reel (le bouton pousse lui-meme).
			{
				const int32 m2 = demo::Demo3DHostEditSelMask();
				st.subMode = (m2 & 1) ? NkSubMode::Vertex : ((m2 & 2) ? NkSubMode::Edge : NkSubMode::Face);
			}

			// Premiere image : tout TIRER, ne rien pousser -- la demo est la
			// source de verite a l'ouverture. Et le HUD suit le masque du shell
			// (off par defaut : il chevauchait la barre d'outils).
			if (sy.first) {
				sy.first = false;
				demo::Demo3DHostSetHud((st.overlayMask & 32u) != 0u);
				demo::Demo3DHostSetOutline((st.overlayMask & 16u) != 0u);
				sy.overlay = 0xFFFFFFFFu; // re-tirer au prochain tour
			}
		}
		nk3d::Viewport3DSetEditMode(st.mode != NkMode::Object);
		// Le sous-mode de la vue devient le masque de selection. Un seul bit ici :
		// les trois boutons sont exclusifs. Les combiner (Maj+1/2/3 chez Blender)
		// viendra avec les raccourcis clavier.
		nk3d::Viewport3DSetSelectMask(1u << (uint32)st.subMode);
		// Outil -> mode du gizmo. « Selection » et « Curseur » n'en ont pas : on
		// laisse alors le gizmo sur le deplacement, mais il ne prendra pas le clic
		// puisque l'arbitrage donne la priorite au maillage.
		{
			int32 gm = 0;
			if (st.tool == NkTool::Rotate)
				gm = 1;
			else if (st.tool == NkTool::Scale)
				gm = 2;
			nk3d::Viewport3DSetGizmoMode(gm);
			// L'outil SELECTION ne transforme rien : afficher ses poignees ferait
			// croire le contraire, et elles captureraient les clics de selection.
			nk3d::Viewport3DSetGizmoVisible(st.tool != NkTool::Select && st.tool != NkTool::Cursor);
		}
		nk3d::Viewport3DSetGizmoOrientation(st.orientation);
		// AIMANTATION : les pas sont FIXES (0,5 unite, 15 degres, 0,1 -- ceux de
		// Demo3D) et ce sont les BASCULES qui decident si elle agit. Le gizmo n'a
		// qu'un interrupteur global : on lui donne celui de la bascule du MODE
		// COURANT -- aimanter les angles sans aimanter les positions reste ainsi
		// possible, puisqu'on ne tourne et ne deplace jamais dans le meme geste.
		// L'ancienne version passait 0 comme pas quand une bascule etait eteinte ;
		// or le gizmo IGNORE un pas nul (garde v > 0) et gardait l'ancien : la
		// valeur appliquee n'etait jamais celle qu'on croyait.
		{
			bool snapOn = st.snapGrid;
			if (st.tool == NkTool::Rotate)
				snapOn = st.snapAngle;
			else if (st.tool == NkTool::Scale)
				snapOn = st.snapScale;
			nk3d::Viewport3DSetSnap(snapOn, 0.5f, 15.f, 0.1f);
		}
		// PROJECTION : entierement geree par la SYNC de la demo portee, plus haut.
		// L'ancien bloc RELISAIT l'etat de la vue DORMANTE (Viewport3DIsOrtho,
		// toujours faux) et remettait le combo a « Perspective » une image apres
		// chaque passage en ortho -- c'est le bug « l'ortho s'active et se
		// desactive en quelques millisecondes » constate par Rihen.

		// ── ENTREE DU GIZMO ─────────────────────────────────────────────────
		// Les deplacements sont recalcules ICI, a partir de la position precedente.
		// Ne surtout pas lire un « delta » fourni par la couche d'evenements : il
		// reste fige a sa derniere valeur quand la souris s'arrete, et le gizmo
		// derive tout seul. Le probleme a deja ete rencontre dans Demo3D.
		{
			const float32 mxv = ui.input.mousePos.x - lay.view.x;
			const float32 myv = ui.input.mousePos.y - lay.view.y;
			// LA VUE N'A PAS LA SOURIS SOUS UNE MODALE. `inView` ne jugeait que la
			// geometrie : le clic droit de la vue passait donc a travers le panneau
			// pose au-dessus d'elle, menu contextuel compris (Rihen, 12 aout).
			const bool inView = !st.ModalOpen() && (mxv >= 0.f && myv >= 0.f &&
													mxv < lay.view.w && myv < lay.view.h);
			// LE GESTE APPARTIENT A LA ZONE OU IL A COMMENCE. Sans ce verrou, tirer
			// un champ de transformation dont le trajet traverse la vue declenchait
			// un press pour le gizmo 3D -- qui pickait dans le vide et DESELECTIONNAIT
			// l'objet qu'on etait en train de regler. De meme, un clic sur les boules
			// du gizmo de navigation (peintes PAR-DESSUS la vue) ne doit pas devenir
			// un pick 3D : on exige que le survol appartienne bien a la scene.
			if (ui.input.mouseDown[0] && !st.gizWasMouseDown)
				st.gizGestureInView = inView && overSceneLastFrame;
			if (!ui.input.mouseDown[0])
				st.gizGestureInView = false;
			st.gizWasMouseDown = ui.input.mouseDown[0];
			const bool down = ui.input.mouseDown[0] && st.gizGestureInView;
			nk3d::Viewport3DSetGizmoInput(mxv, myv, mxv - st.gizLastX, myv - st.gizLastY,
										  down && !st.gizWasDown, down, ui.input.shiftDown,
										  ui.input.ctrlDown);
			st.gizLastX = mxv;
			st.gizLastY = myv;
			st.gizWasDown = down;
		}

		// ── CONSOMMATION DE L'INTENTION CLAVIER ─────────────────────────────
		// Ici, et pas dans le callback : on est entre deux images, le maillage
		// n'est pas en cours de lecture par le rendu, et une modification
		// topologique peut donc se faire sans risque.
		// ── PILOTAGE DE LA TRANSFORMATION MODALE ────────────────────────────
		// Elle est mise a jour AVANT le dispatch : la souris a bouge depuis la
		// derniere image, et l'objet doit avoir suivi quand le panneau Proprietes
		// se peindra. C'est ce qui donne la mise a jour en TEMPS REEL, dans la vue
		// comme dans les champs.
		// ── UN GESTE DE GIZMO VIENT-IL DE SE TERMINER ? ─────────────────────
		// Le front DESCENDANT (ca glissait, ca ne glisse plus) = un commit :
		// deplacement, rotation, echelle -- au gizmo objet, d'edition, de
		// lumiere ou d'empty. C'est le pendant du NkMarkDirty de la modale
		// ci-dessous : sans lui, bouger un cube A LA SOURIS n'allumait pas la
		// pastille « non enregistre » (constate par Rihen), et la protection a
		// la fermeture ne protegeait rien.
		{
			static bool sGizmoWasDragging = false;
			const bool gizNow = demo::Demo3DHostAnyGizmoDragging();
			if (sGizmoWasDragging && !gizNow)
				NkMarkDirty(st);
			sGizmoWasDragging = gizNow;
		}
		{
			const float32 mxv = ui.input.mousePos.x - lay.view.x;
			const float32 myv = ui.input.mousePos.y - lay.view.y;
			if (nk3d::Viewport3DModalKind() != nk3d::kVpXformNone) {
				nk3d::Viewport3DModalUpdate(mxv, myv);
				NkMarkDirty(st);
				// Le clic gauche CONFIRME, le clic droit ANNULE -- et la modale
				// consomme le clic, sinon il tomberait ensuite sur la selection.
				if (ui.input.mouseClicked[0])
					nk3d::Viewport3DModalConfirm();
				else if (ui.input.mouseClicked[1])
					nk3d::Viewport3DModalCancel();
			}
		}

		if (st.pendingAction != NkVpAction::None) {
			const NkVpAction a = st.pendingAction;
			st.pendingAction = NkVpAction::None;
			const bool edit = (st.mode != NkMode::Object);
			const bool inModal = (nk3d::Viewport3DModalKind() != nk3d::kVpXformNone);
			const float32 mxv = ui.input.mousePos.x - lay.view.x;
			const float32 myv = ui.input.mousePos.y - lay.view.y;
			switch (a) {
				case NkVpAction::ToggleEdit:
					st.mode = edit ? NkMode::Object : NkMode::Edit;
					break;
				case NkVpAction::SubModeVertex:
					st.subMode = NkSubMode::Vertex;
					break;
				case NkVpAction::SubModeEdge:
					st.subMode = NkSubMode::Edge;
					break;
				case NkVpAction::SubModeFace:
					st.subMode = NkSubMode::Face;
					break;
				case NkVpAction::SelectAll:
					nk3d::Viewport3DSelectAll(true);
					break;
				case NkVpAction::SelectNone:
					nk3d::Viewport3DSelectAll(false);
					break;
				case NkVpAction::ToolMove:
					st.tool = NkTool::Move;
					break;
				case NkVpAction::ToolRotate:
					st.tool = NkTool::Rotate;
					break;
				case NkVpAction::ToolScale:
					st.tool = NkTool::Scale;
					break;
				// ── Modales ─────────────────────────────────────────────────
				case NkVpAction::ModalMove:
					nk3d::Viewport3DBeginModal(nk3d::kVpXformMove, mxv, myv);
					break;
				case NkVpAction::ModalRotate:
					nk3d::Viewport3DBeginModal(nk3d::kVpXformRotate, mxv, myv);
					break;
				case NkVpAction::ModalScale:
					nk3d::Viewport3DBeginModal(nk3d::kVpXformScale, mxv, myv);
					break;
				case NkVpAction::ModalAxisX:
					// HORS MODALE, X garde son role de suppression : une touche ne
					// doit pas devenir muette parce qu'un autre mode existe.
					if (inModal) {
						nk3d::Viewport3DModalAxis(0);
					} else if (edit) {
						if (nk3d::Viewport3DDeleteSelection())
							NkMarkDirty(st);
					} else {
						const int32 act = nk3d::Viewport3DActiveObject();
						if (act >= 0) {
							nk3d::Viewport3DDeleteObject(act);
							NkMarkDirty(st);
						}
					}
					break;
				case NkVpAction::ModalAxisY:
					if (inModal)
						nk3d::Viewport3DModalAxis(1);
					break;
				case NkVpAction::ModalAxisZ:
					if (inModal)
						nk3d::Viewport3DModalAxis(2);
					break;
				case NkVpAction::ModalConfirm:
					if (inModal)
						nk3d::Viewport3DModalConfirm();
					break;
				case NkVpAction::ModalCancel:
					// Echap annule ce qui est en cours, dans l'ordre de priorite :
					// une modale d'abord, un outil de zone ensuite. Sans cet ordre,
					// armer un rectangle puis appuyer Echap annulerait la mauvaise
					// chose.
					if (inModal)
						nk3d::Viewport3DModalCancel();
					else if (st.zoneTool >= 0) {
						st.zoneTool = -1;
						st.zoneActive = false;
					}
					break;
				case NkVpAction::ZoneRect:
					st.zoneTool = (st.zoneTool == 0) ? -1 : 0;
					st.zoneActive = false;
					break;
				case NkVpAction::ZoneCircle:
					st.zoneTool = (st.zoneTool == 2) ? -1 : 2;
					st.zoneActive = false;
					break;
				case NkVpAction::ToggleXray:
					st.xray = !st.xray;
					nk3d::Viewport3DSetXray(st.xray);
					break;
				// Les operations n'ont de sens QU'EN EDITION. Les laisser passer en
				// mode objet donnerait des commandes sans effet, donc un journal
				// d'annulation qui se remplit de riens.
				case NkVpAction::Extrude:
					if (edit && nk3d::Viewport3DExtrude(false))
						NkMarkDirty(st);
					break;
				case NkVpAction::ExtrudeIndividual:
					if (edit && nk3d::Viewport3DExtrude(true))
						NkMarkDirty(st);
					break;
				case NkVpAction::Delete:
					// Supprime CE QUE le mode designe : les faces selectionnees en
					// edition, l'objet actif en mode objet.
					if (edit) {
						if (nk3d::Viewport3DDeleteSelection())
							NkMarkDirty(st);
					} else {
						const int32 act = nk3d::Viewport3DActiveObject();
						if (act >= 0) {
							nk3d::Viewport3DDeleteObject(act);
							NkMarkDirty(st);
						}
					}
					break;
				case NkVpAction::Dissolve:
					if (edit && nk3d::Viewport3DDissolve())
						NkMarkDirty(st);
					break;
				case NkVpAction::Merge:
					if (edit && nk3d::Viewport3DMerge(0)) // 0 = au centre
						NkMarkDirty(st);
					break;
				case NkVpAction::MakeFace:
					if (edit && nk3d::Viewport3DMakeFace())
						NkMarkDirty(st);
					break;
				case NkVpAction::Subdivide:
					if (edit && nk3d::Viewport3DSubdivide(1))
						NkMarkDirty(st);
					break;
				case NkVpAction::LoopCut:
					if (edit && nk3d::Viewport3DLoopCut(1))
						NkMarkDirty(st);
					break;
				case NkVpAction::Inset:
					// Epaisseur AUTOMATIQUE, proportionnelle a l'objet : une valeur
					// fixe donne un inset invisible sur un grand modele et un inset
					// qui traverse tout sur un petit.
					if (edit && nk3d::Viewport3DInset(0.1f, 0.f))
						NkMarkDirty(st);
					break;
				case NkVpAction::BevelEdge:
					if (edit && nk3d::Viewport3DBevel(0.1f, 2, false))
						NkMarkDirty(st);
					break;
				case NkVpAction::BevelVertex:
					if (edit && nk3d::Viewport3DBevel(0.1f, 2, true))
						NkMarkDirty(st);
					break;
				case NkVpAction::Undo:
					nk3d::Viewport3DUndo();
					break;
				case NkVpAction::Redo:
					nk3d::Viewport3DRedo();
					break;
				// ── Vues ────────────────────────────────────────────────────
				case NkVpAction::ViewFront:
					nk3d::Viewport3DAxisView(0, false);
					break;
				case NkVpAction::ViewBack:
					nk3d::Viewport3DAxisView(0, true);
					break;
				case NkVpAction::ViewRight:
					nk3d::Viewport3DAxisView(1, false);
					break;
				case NkVpAction::ViewLeft:
					nk3d::Viewport3DAxisView(1, true);
					break;
				case NkVpAction::ViewTop:
					nk3d::Viewport3DAxisView(2, false);
					break;
				case NkVpAction::ViewBottom:
					nk3d::Viewport3DAxisView(2, true);
					break;
				case NkVpAction::ToggleOrtho:
					st.projection = (st.projection == 1) ? 0 : 1;
					break;
				case NkVpAction::FrameAll:
					nk3d::Viewport3DFrameAll();
					break;
				default:
					break;
			}
		}

		const NkTheme &theme = themes.Current();
		NkModelerPainter p(ui.dl, font, theme, roles, icons);
		// Le peintre de la couche OVERLAY : meme theme, meme jeu d'icones, mais il
		// ecrit dans la liste soumise EN DERNIER. C'est lui qui peint les surfaces
		// modales, pour qu'elles restent au-dessus des composants du kit.
		NkModelerPainter pOverlay(ui.dlOverlay, font, theme, roles, icons);
		nk3d::NkOvPainter() = &pOverlay;

		// Fond general : il se voit dans les interstices entre panneaux, et c'est
		// ce qui donne la profondeur a trois niveaux de UI_SPEC 10bis.1.
		p.Fill({0.f, 0.f, W, H}, NkRole::WindowBg);

		// ── LE JOURNAL INTERDIT SON RECTANGLE AUX PANNEAUX ──────────────────
		// Il est peint EN DERNIER, mais un panneau decide de ses clics AU MOMENT
		// ou il se peint -- donc avant que le journal ait declare quoi que ce
		// soit. Le navigateur ouvrait ainsi son menu contextuel a travers lui
		// (Rihen, 13 aout). `SetBlock` est le mecanisme prevu pour exactement
		// cela : une surcouche annonce son emprise A L'AVANCE, et le registre
		// refuse tout clic qui y tombe. Il est LEVE juste avant de peindre le
		// journal, comme pour les autres surcouches.
		const NkRect jRect =
			nk3d::NkJournalRect({0.f, 0.f, (float32)W, (float32)H - lay.status.h});
		// L'INPUT EST VIDE A LA SOURCE quand la souris est sur le journal.
		// `SetBlock` ne suffit pas : il ne protege que ce qui passe par le
		// registre, or le navigateur teste `in.mouseClicked[1]` DIRECTEMENT pour
		// ouvrir son menu contextuel -- son clic droit traversait donc le journal
		// (Rihen, 14 aout : « son clic droit est toujours capte par le navigateur,
		// donc je conclus que ses evenements aussi »). C'est le meme remede que
		// pour les modales : vider l'input est le seul endroit qui vaut partout a
		// la fois, y compris pour le code qui ne connait pas le registre.
		const nkgui::NkGuiInput inputAvantJournal = ui.input;
		const bool sourisSurJournal =
			st.journalOpen && nkgui::NkGuiRectContains(jRect, ui.input.mousePos);
		if (st.journalOpen)
			hit.SetBlock(jRect, true);
		if (sourisSurJournal) {
			for (int32 b = 0; b < 3; ++b) {
				ui.input.mouseDown[b] = false;
				ui.input.mouseClicked[b] = false;
				ui.input.mouseReleased[b] = false;
			}
			ui.input.wheel = 0.f;
		}

		// LE NOM DU PROJET, PAS UN LIBELLE FIGE. « MonProjet » etait un exemple de
		// maquette ; la barre dit desormais ce qui est reellement ouvert.
		PaintMenuBarI(p, lay.menu,
					  proj.open && !proj.name.Empty() ? proj.name.CStr() : "Aucun projet", st,
					  hit);
		PaintTabsI(p, lay.tabs, st, hit, ws, ui.input);
		PaintToolbar(p, lay.tool, st, hit, ws, combo);
		// Un panneau MASQUE n'est pas peint en taille nulle : il n'est pas peint du
		// tout. Le peindre dans un rectangle de 14 px declarerait ses zones cliquables
		// les unes sur les autres et un clic sur la poignee tomberait sur la premiere
		// ligne de la liste.
		if (st.showLeft)
			PaintHierarchy(p, lay.left, st, hit, ws, ui.input, &ui);
		PaintViewport(p, lay.view, st, hit, ws, ui.input, combo, checks, shortcuts);
		if (st.showRight) {
			// PANNEAU DROIT UNIQUE (demande de Rihen) : Objet / Scene / Outil.
			// Proprietes et Details disaient deux fois la meme chose ; leurs
			// deux rectangles sont reunis en un seul.
			{
				NkRect rightR = lay.propsR;
				rightR.h = (lay.detailsR.y + lay.detailsR.h) - lay.propsR.y;
				// Le contexte passe AU PANNEAU : sa scrollbar est celle de
				// NKEditorKit (la meme que l'editeur de code), qui dessine
				// directement dans le contexte.
				PaintPropertiesUnified(p, rightR, st, hit, ws, ui.input, combo, &ui);
			}
		}
		if (st.showBrowser)
			PaintBrowser(p, lay.browser, st, hit, ws, ui.input, &ui, &combo);
		PaintStatus(p, hit, lay.status, st);
		// LE JOURNAL S'ANCRE SUR LA FENETRE ENTIERE, pas sur une zone de la mise
		// en page : il recouvre ce qui se trouve dessous, comme un tiroir. Peint
		// APRES la barre d'etat, dont il sort. Le blocage pose plus haut est
		// LEVE ici : il protegeait les panneaux de ses clics, il ne doit pas
		// l'empecher de recevoir les siens.
		if (st.journalOpen)
			hit.SetBlock({}, false);
		// L'input REEL revient pour le journal : il lui etait retire, pas perdu.
		if (sourisSurJournal) {
			ui.input = inputAvantJournal;
			hit.Rearm(ui.input);
		}
		PaintJournal(p, hit, st, ui.input, {0.f, 0.f, (float32)W, (float32)H - lay.status.h});

		// Poignees de reouverture, a la place exacte qu'occupait le panneau.
		PaintPanelHandle(p, lay.handleLeft, hit, "handle.left", st.showLeft, NkIcon::ChevronRight);
		PaintPanelHandle(p, lay.handleRight, hit, "handle.right", st.showRight, NkIcon::ChevronLeft);
		PaintPanelHandle(p, lay.handleBrowser, hit, "handle.browser", st.showBrowser,
						 NkIcon::ChevronUp);

		// L'ORDRE DE CES TROIS APPELS EST SIGNIFIANT. Les separateurs doivent
		// recevoir le clic avant les panneaux qu'ils bordent ; le menu deroule
		// recouvre tout ; la boite de confirmation recouvre le menu. Le registre
		// donnant la priorite a la DERNIERE zone declaree, l'ordre de peinture EST
		// l'ordre de priorite -- il n'y a rien d'autre a synchroniser.
		// La liste deroulee est peinte AVANT les separateurs et le menu : elle doit
		// les recouvrir, et le registre donne la priorite a la derniere zone.
		PaintSplitters(p, lay, W, H, st, hit);
		// ── LES SURCOUCHES MONTENT DE COUCHE ────────────────────────────────
		// Menus, sous-menus et listes deroulees vivent sur la couche 50, les
		// fenetres modales sur la couche 100. Le registre donne le survol a la
		// couche la plus HAUTE : tout ce qui est peint dessous devient aveugle
		// sous leur emprise, sans qu'aucun panneau ait a s'en garder lui-meme.
		// LES SURCOUCHES, ELLES, REPONDENT : on leur rend l'input reel qu'on
		// avait retire aux panneaux. Le registre est re-arme sans etre vide --
		// les zones deja declarees restent, seuls les evenements reviennent.
		if (modalOpen) {
			ui.input = inputReel;
			hit.Rearm(ui.input);
		}
		{
			NkHitRegistry::LayerScope menuLayer(hit, 50);
			// ── LES SURCOUCHES ECHAPPENT AU BLOCAGE DES PANNEAUX ────────────
			// Un panneau arme SetBlock pour que la liste ouverte d'un combo ne
			// laisse pas ses clics le traverser. Mais menus, listes et boites
			// sont peints ICI, APRES lui : le blocage les neutralisait a leur
			// tour, si bien que la liste s'ouvrait sans qu'on puisse rien y
			// choisir (constate par Rihen sur le format de sortie). Le blocage
			// protege ce qui est DESSOUS, jamais ce qui est au-dessus -- on le
			// leve donc en entrant dans la couche des surcouches.
			hit.SetBlock({}, false);
			// Menus et dialogues de scene (menu contextuel de la hierarchie ET
			// de la vue 3D, du navigateur, confirmation de suppression).
			PaintSceneMenus(p, {0.f, 0.f, (float32)W, (float32)H}, lay.view, st, hit, ws,
							ui.input);
			PaintMatcapPopup(p, hit, st);
			{
				NkRect comboBox{};
				DrawComboPopup(p, hit, ws, combo, &comboBox);
				st.UiBlockAdd(comboBox);
			}
			DrawCheckPopup(p, hit, ws, checks);
			PaintModifierMenu(p, st, hit, ws, W, H);
			PaintAddObjectMenu(p, st, hit, ws, W, H);
			PaintOpenMenu(p, lay.menu, st, hit, shortcuts);
		}
		// L'emprise que les menus viennent de declarer devient, a la frame
		// SUIVANTE, ce qui les rend etanches : les panneaux peints sous eux la
		// consultent sans rien savoir d'eux. Une seule union suffit -- c'est
		// deja ce qu'accumule UiBlockAdd.
		if (st.uiBlockAccOn)
			hit.PushOcclusion(st.uiBlockAcc, 50);
		{
			// MODALES : elles suspendent tout le reste, menus compris.
			NkHitRegistry::LayerScope modalLayer(hit, 100);
			// L'ACCUEIL EN PREMIER dans la couche : il recouvre l'application,
			// mais les boites de fermeture doivent pouvoir se poser DESSUS --
			// le registre donne la priorite a la derniere zone declaree.
			nk3d::PaintWelcome(p, W, H, st, hit, ws, ui.input, recents, splashArt);
			PaintCloseDialog(p, W, H, st, hit);
			PaintCloseRecDialog(p, W, H, st, hit);
			PaintEncodeDoneDialog(p, W, H, st, hit);
			PaintColorPicker(p, hit, ws, ui.input, st, (float32)W, (float32)H);
			// La modale « Ajouter un materiau » : ICI, avec les surcouches, jamais
			// dans le panneau de proprietes. C'est ce qui la rend etanche -- l'input
			// vient d'etre rendu aux surcouches, et la vue 3D, elle, n'a rien recu.
			nk3d::PaintMatAddModal(st, hit, ws, ui.input, combo, &ui);
		}

		// ── SELECTEUR DE FICHIERS (NKEditorKit, celui de NKCode) ────────────
		// Peint ICI, hors de toute couche de panneaux : il flotte donc sur
		// TOUTE la fenetre, comme Rihen l'a demande. Le composant ne fait que
		// DECIDER (il depose un resultat) ; c'est l'application qui agit.
		if (st.picker.pickerOpen) {
			// COUCHE 100 : le registre donne le survol a la couche la plus haute,
			// donc tout ce qui est peint dessous -- menus contextuels compris --
			// devient aveugle sous son emprise. C'est ce qui empeche le clic droit
			// de la vue 3D de repondre a travers lui (Rihen, 12 aout).
			NkHitRegistry::LayerScope modalLayer(hit, 100);
			(void)hit.Add("picker.modal", {0.f, 0.f, (float32)W, (float32)H});
			editorkit::NkDrawFilePicker(ui, st.picker, editorkit::NkFilePickerStyle{});
		}
		if (st.picker.pickerConfirmed) {
			st.picker.pickerConfirmed = false;
			if (st.pickerAction == 1 && st.picker.pickerResultName[0]) {
				const int32 ni = demo::Demo3DHostProjMatCreate();
				if (ni >= 0) {
					// Le nom saisi n'est pas pose tel quel : s'il est deja porte
					// ailleurs dans le projet, il devient « X.001 » (Rihen : renommer
					// plutot que refuser). L'utilisateur voit tout de suite le nom
					// retenu, au lieu d'un bouton eteint sans explication.
					char nomLibre[80];
					nk3d::NkMatUniqueName(st.picker.pickerResultName, ni, nomLibre,
										  (uint32)sizeof(nomLibre));
					demo::Demo3DHostProjMatSetName(ni, nomLibre);
					// ── SON TYPE, CHOISI AVANT LA CREATION ──────────────────
					// Pose AVANT l'ecriture disque : le `.nkmat` serialise le
					// champ `type` (NkProjectWriteAssets), et un type applique
					// apres coup n'aurait vecu qu'en memoire -- exactement la
					// faute qui a coute la matinee (cf. « agir a la source »).
					demo::Demo3DHostProjMatSetType(ni, st.picker.MatNewTypeValue());
					// LE MATERIAU NAISSANT SE LIE A L'OBJET ACTIF. Le meme repli
					// que partout ailleurs : `Demo3DHostActiveObject` ne connait
					// que les objets du MOTEUR et rend -1 pour les autres (vides,
					// lumieres, cameras), pour lesquels l'application tient
					// `activeEmpty`. Sans ce repli, le materiau etait bien cree
					// mais n'apparaissait dans la liste d'aucun objet — « ca ne
					// s'ajoute pas directement a la liste des materiaux de l'objet
					// selectionne » (Rihen, 13 aout).
					const int32 an = demo::Demo3DHostActiveObject() >= 0
										 ? demo::Demo3DHostActiveObject()
										 : st.activeEmpty;
					// CREER, C'EST VOULOIR S'EN SERVIR : le materiau devient le
					// materiau ACTIF de l'objet, pas une ligne de plus dans sa
					// liste — « ca l'ajoute a l'objet actif mais ne le lie pas
					// comme materiau par defaut » (Rihen, 13 aout). `ProjMatAssign`
					// fait les deux (il associe aussi), la ou `NodeMatAdd` se garde
					// justement de toucher a un actif deja choisi : ajouter n'est
					// pas assigner, et c'est bien d'assigner qu'il s'agit ici.
					// Le bouton « Ajouter » de la modale, lui, garde l'ajout seul.
					if (an >= 0)
						demo::Demo3DHostProjMatAssign(an, ni);
					nk3d::NkMarkDirty(st);
					// ── ET ON L'ECRIT SUR LE DISQUE ─────────────────────────
					// Il n'existait qu'en MEMOIRE : aucun `.nkmat` n'etait ecrit,
					// aucune carte creee. L'utilisateur choisissait un dossier et un
					// nom, et ne trouvait rien — « la creation d'un nouveau materiau
					// a echoue » (Rihen, 13 aout). La carte d'abord (c'est elle qui
					// porte le chemin du fichier), l'ecriture ensuite.
					nk3d::NkBrowserSyncMats(st);
					// ── ET DANS LE DOSSIER CHOISI ───────────────────────────
					// `NkBrowserSyncMats` cree les cartes manquantes A LA RACINE :
					// il repare un lien, il ne peut pas deviner ou l'utilisateur
					// voulait ranger. Le dossier retenu dans le selecteur est donc
					// pose ICI, avant l'ecriture -- c'est `browserParent` qui
					// decide du chemin du `.nkmat` (NkAsRelFor).
					const int32 dossier = nk3d::NkAsFolderFromAbs(
						st, st.projectRoot, st.picker.pickerResultPath);
					for (int32 b3 = 0; b3 < st.browserCount; ++b3)
						if (st.browserKind[b3] == 2 && st.browserMat[b3] == ni + 1) {
							st.browserParent[b3] = dossier;
							break;
						}
					NkString errNew;
					if (!nk3d::NkProjectWriteAssets(proj.root, st, &errNew, -1))
						nkentseu::NkLog::Instance().Info(
							"[materiaux] creation : ecriture impossible : {0}", errNew.CStr());
				} else {
					nkentseu::NkLog::Instance().Info(
						"[materiaux] creation impossible : plus d'emplacement libre");
				}
			}
			st.pickerAction = 0;
			st.matNewPending = false;
			// Le mode « nouveau materiau » du selecteur se desarme TOUT SEUL,
			// dans `PickerCancel` : c'est sa porte de sortie unique, Echap
			// comprise. Le desarmer aussi ici ne ferait que dupliquer la regle.
		}
		if (st.picker.pickerCancelled) {
			st.picker.pickerCancelled = false;
			st.pickerAction = 0;
			st.matNewPending = false;
		}
		// SELECTEUR FERME = ACTION CADUQUE. La touche Echap referme le selecteur
		// sans passer par « Annuler » : elle ne posait donc ni confirmation ni
		// annulation, et `pickerAction` restait a 1. Le selecteur suivant --
		// ouvert pour tout autre chose -- aurait vu sa confirmation interpretee
		// comme « creer un materiau ». Une intention doit mourir avec la fenetre
		// qui l'a fait naitre.
		if (!st.picker.pickerOpen && st.pickerAction != 0) {
			st.pickerAction = 0;
			st.matNewPending = false;
		}

		ui.EndFrame();

		// ── ACTIONS PROJET ──────────────────────────────────────────────────
		// APRES la frame, jamais pendant : les selecteurs de fichiers de l'OS
		// entrent dans une boucle modale et reentreraient dans la peinture.
		// Puis les vignettes de couverture, rechargees SEULEMENT quand la
		// liste des recents a change (drapeau `texDirty`).
		nk3d::NkProjectHandlePending(st, proj, recents);

		// ── LE DOSSIER DU PROJET EST SURVEILLE ──────────────────────────────
		// Un fichier ajoute ou efface a la main doit se voir dans le navigateur
		// (Rihen). Le surveillant a SON PROPRE FIL : il ne pose qu'un drapeau, et
		// la reconciliation se fait ICI, sur le fil principal, entre deux frames.
		// Toucher l'etat du modeleur depuis l'autre fil produirait des corruptions
		// impossibles a reproduire.
		// LA RACINE DESCEND DANS L'ETAT, une fois par frame. C'est le point de
		// passage unique ou projet et etat se cotoient : sans elle, un panneau
		// qui veut ecrire un fichier ne le peut pas — il ne voit que l'etat.
		// Meme geste que NKCode, dont l'etat porte sa propre `root` (Rihen,
		// 12 aout : « rends ce dossier accessible »).
		st.projectRoot = proj.open ? proj.root : NkString();
		// ── LES DOUBLONS DE NOMS SONT CORRIGES A L'OUVERTURE ────────────────
		// La regle « deux materiaux ne portent jamais le meme nom » est neuve
		// (Rihen, 13 aout) : les projets d'avant en ont -- il y avait deux
		// « Materiau » dans celui de test. On les renomme une fois, au chargement,
		// plutot que de laisser l'utilisateur les demeler a la main. Detecte par le
		// CHANGEMENT de fichier ouvert, donc une seule fois par projet.
		{
			static NkString sDernierProjet;
			if (proj.open && proj.file != sDernierProjet) {
				sDernierProjet = proj.file;
				const int32 renommes = nk3d::NkMatFixDuplicates();
				if (renommes > 0) {
					// LE RENOMMAGE DOIT SURVIVRE A LA FERMETURE. Il ne portait que sur
					// l'emplacement EN MEMOIRE : la carte du navigateur et le fichier
					// .nkmat gardaient l'ancien nom, et le doublon revenait a la
					// reouverture (constate par Rihen, 13 aout). On aligne les cartes,
					// puis on marque le projet modifie pour que l'enregistrement porte.
					nk3d::NkBrowserSyncMats(st);
					for (int32 b = 0; b < st.browserCount; ++b) {
						if (st.browserKind[b] != 2 || st.browserMat[b] <= 0)
							continue;
						char nm[64];
						float32 alb[3];
						float32 rg = 0.f, mt = 0.f;
						if (demo::Demo3DHostProjMatInfo(st.browserMat[b] - 1, nm,
														(uint32)sizeof(nm), alb, &rg, &mt))
							NkWidgetState::Copy(st.browserNames[b], nm, 31u);
					}
					nk3d::NkMarkDirty(st);
					// ON REECRIT LE DISQUE TOUT DE SUITE (Rihen, 13 aout). Renommer
					// en memoire ne suffisait pas : les deux fichiers restaient
					// « Materiau.nkmat » dans leurs dossiers respectifs, et le doublon
					// revenait a la reouverture. `NkProjectWriteAssets` ecrit le
					// fichier sous son NOUVEAU nom puis efface l'ancien -- il connait
					// le chemin precedent par `browserFile`, justement pour ne pas
					// laisser d'orphelins qu'on prendrait plus tard pour du travail
					// perdu.
					NkString errRen;
					if (!nk3d::NkProjectWriteAssets(proj.root, st, &errRen, -1))
						nkentseu::NkLog::Instance().Info(
							"[materiaux] reecriture disque impossible : {0}", errRen.CStr());
					nkentseu::NkLog::Instance().Info(
						"[materiaux] {0} nom(s) en double corrige(s) a l'ouverture", renommes);
				}
			} else if (!proj.open) {
				sDernierProjet.Clear();
			}
		}
		// Le dossier courant du navigateur, en chemin DISQUE : c'est la que les
		// selecteurs doivent s'ouvrir. `NkAsFolderPath` ne rend qu'un relatif, et
		// n'est visible QUE d'ici (NkModelerAssets.h est inclus apres les ecrans).
		// Vide si le dossier n'a pas encore d'existence sur le disque -- l'appelant
		// se replie alors sur la racine plutot que d'ouvrir un arbre vide.
		st.browserFolderAbs = NkString();
		if (proj.open) {
			const NkString rel = nk3d::NkAsFolderPath(st, st.browserFolder);
			const NkString abs = rel.Empty() ? proj.root : nk3d::NkScToAbs(proj.root, rel.CStr());
			if (NkDirectory::Exists(abs.CStr()))
				st.browserFolderAbs = abs;
		}
		if (proj.open)
			projWatch.Watch(proj.root);
		else
			projWatch.Stop();
		if (projWatch.signaled) {
			projWatch.signaled = false;
			// Nos PROPRES ecritures reveillent le surveillant elles aussi : le
			// balayage ne trouve alors aucune difference et ne fait rien. C'est
			// voulu -- distinguer nos ecritures des autres demanderait une
			// comptabilite qui se desynchroniserait au premier oubli.
			if (nk3d::NkProjectRescan(proj.root, st) > 0)
				nk3d::NkMarkTreeDirty(st);
		}
		nk3d::NkWelcomeUploadCovers(renderer, recents);
		// L'IMAGE DE VERSION : chargee une seule fois, et seulement quand
		// l'accueil est visible -- inutile de decoder un PNG que personne ne
		// verra si l'application ouvre directement un projet.
		if (st.welcome)
			nk3d::NkSplashLoad(renderer, splashArt);

		// ── CURSEUR ─────────────────────────────────────────────────────────
		// Repose CHAQUE frame : sur Windows le systeme le remet a la fleche des
		// que la souris traverse une zone qui ne le redemande pas.
		switch (hit.Cursor()) {
			case NkCursorWant::ResizeWE:
				window.SetCursor(NkWindow::NkCursorType::ResizeWE);
				break;
			case NkCursorWant::ResizeNS:
				window.SetCursor(NkWindow::NkCursorType::ResizeNS);
				break;
			case NkCursorWant::Hand:
				window.SetCursor(NkWindow::NkCursorType::Hand);
				break;
			default:
				window.SetCursor(NkWindow::NkCursorType::Arrow);
				break;
		}

		// CE QUE LE PANNEAU A DEMANDE, transmis au crochet pre-UI juste avant
		// qu'il ne s'execute : `BeginFrame` appelle preUI3D, qui rendra l'apercu
		// du materiau dans la meme frame device.
		gPrevSlot = st.matPrevSlot;
		gPrevW = st.matPrevW > 0 ? st.matPrevW : 260;
		gPrevH = st.matPrevH > 0 ? st.matPrevH : 150;
		renderer.BeginFrame();
		renderer.SubmitDrawList(ui.dl, lastW, lastH);
		// LA COUCHE OVERLAY EST SOUMISE APRES, donc rendue PAR-DESSUS. Sans cette
		// ligne, tout composant de NKEditorKit (selecteur de fichiers, modale, menu
		// contextuel) dessine dans le vide : ces composants ecrivent dans
		// `ctx.dlOverlay`, que le modeleur ne soumettait pas -- « le bouton Nouveau
		// ne fait pas apparaitre le selecteur » (Rihen, 12 aout).
		renderer.SubmitDrawList(ui.dlOverlay, lastW, lastH);
		renderer.EndFrame();

		// ── ENREGISTREMENT DU TUTORIEL : LA FENETRE ENTIERE ─────────────────
		// APRES EndFrame : c'est le seul moment ou la fenetre affiche l'image
		// complete de cette frame. Avant, on photographierait la precedente.
		// On ne photographie QUE si la cadence l'attend -- une capture d'ecran
		// coute cher, la prendre pour la jeter ensuite serait absurde.
#if defined(NKENTSEU_PLATFORM_WINDOWS)
		// Demande venue de l'interface : elle sait CE QU'ON VEUT, la boucle
		// seule sait a QUELLE TAILLE la fenetre est reellement affichee -- et
		// cette taille est indispensable pour ouvrir le fichier video.
		if (st.tutoRecPending) {
			const int32 rq = st.tutoRecPending;
			st.tutoRecPending = 0;
			if (rq == 1) {
				uint32 fw = 0, fh = 0;
				NkCaptureWholeWindowSize(window, &fw, &fh);
				if (fw > 0 && fh > 0)
					demo::Demo3DHostRecTutoStart((int32)fw, (int32)fh);
			} else
				demo::Demo3DHostRecTutoStop(rq == 2);
		}
		// LE CURSEUR N'EST PAS DANS LA CAPTURE : PrintWindow rend le contenu de
		// la fenetre, pas le pointeur du systeme. Une video de tutoriel sans
		// curseur montre des menus qui s'ouvrent tout seuls -- on le dessine
		// donc, avec la TRACE de ses dernieres positions : c'est le mouvement
		// qui s'explique, pas la position instantanee (demande de Rihen).
		// La trace se nourrit A CHAQUE IMAGE, pas seulement quand la cadence
		// reclame une capture : echantillonnee a 2 i/s, elle sauterait d'un
		// bout de l'ecran a l'autre au lieu de dessiner un geste.
		static nk3d::NkCursorTrail sTutoCursor;
		if (demo::Demo3DHostRecTutoActive()) {
			static float64 sTutoLastNs = 0.0;
			const float64 nowNs = (float64)::nkentseu::NkChrono::Now().nanoseconds;
			float32 dtT = sTutoLastNs > 0.0 ? (float32)((nowNs - sTutoLastNs) / 1.0e9) : (1.f / 60.f);
			sTutoLastNs = nowNs;
			if (dtT <= 0.f || dtT > 0.25f)
				dtT = 1.f / 60.f;
			POINT cur{};
			RECT wr{};
			const NkSurfaceDesc sdC = window.GetSurfaceDesc();
			const bool okCur = sdC.hwnd && GetCursorPos(&cur) && GetWindowRect(sdC.hwnd, &wr);
			if (okCur)
				sTutoCursor.Push((int32)(cur.x - wr.left), (int32)(cur.y - wr.top));
			if (demo::Demo3DHostRecTutoWants(dtT)) {
				NkImage shot;
				int32 sw = 0, sh = 0;
				if (NkCaptureWholeWindowToImage(window, shot, &sw, &sh) && shot.Pixels()) {
					if (okCur && demo::Demo3DHostOutCursor()) {
						// L'echelle suit la taille de la fenetre : un curseur de
						// 16 px dans une video 4K serait un point invisible.
						float32 sc = (float32)sw / 1600.f;
						if (sc < 1.f)
							sc = 1.f;
						if (sc > 3.f)
							sc = 3.f;
						nk3d::NkDrawCursorTrail((uint8 *)shot.Pixels(), sw, sh, sTutoCursor, sc);
					}
					demo::Demo3DHostRecTutoPush((const uint8 *)shot.Pixels(), sw, sh);
				}
			}
		} else
			sTutoCursor.Clear(); // une prise neuve ne herite pas du geste precedent
#endif

		// ── CROCHETS D'AGENT : capture et sortie a la frame demandee ────────
		// (cf. leur declaration pres de recents.Load() — ils ne font qu'armer
		// ce que les boutons arment deja.)
		++agentFrame;
		if (agentOpenRecent >= 0 && agentFrame >= 3 && demo::Demo3DHostReady()) {
			st.projRecent = agentOpenRecent;
			st.projPending = 7;
			agentOpenRecent = -1;
		}
		if (agentShotFrame > 0 && agentFrame == agentShotFrame)
			st.capturePending = 2; // « tutoriel » : toute la fenetre
		// NK_AGENT_SAVE=<n> : « Enregistrer tout » (action 8) a la frame n —
		// le MEME chemin que Ctrl+Maj+S. Pour le test d'aller-retour de la
		// persistance : enregistrer, relancer, re-enregistrer, comparer.
		{
			static int32 sAgentSaveFrame = -2;
			if (sAgentSaveFrame == -2) {
				const char *v = std::getenv("NK_AGENT_SAVE");
				sAgentSaveFrame = v ? (int32)std::atoi(v) : -1;
			}
			if (sAgentSaveFrame > 0 && agentFrame == sAgentSaveFrame && st.projPending == 0)
				st.projPending = 8;
		}
		if (agentExitFrame > 0 && agentFrame >= agentExitFrame)
			st.running = false;
		// NK_SHADOW_QUALITY=<0..4> / NK_SHADOW_SOFT=<f> : appliques UNE fois,
		// des que l'hote 3D existe — par le MEME setter que le panneau. Le
		// combo du panneau est aussi aligne, sinon il repousserait son propre
		// etat par-dessus a la frame suivante.
		{
			static bool sAgentShadowDone = false;
			if (!sAgentShadowDone && demo::Demo3DHostReady()) {
				sAgentShadowDone = true;
				const char *q = std::getenv("NK_SHADOW_QUALITY");
				const char *sf = std::getenv("NK_SHADOW_SOFT");
				if (q || sf) {
					float32 nb = 0.f, sb = 0.f, so = 0.f;
					int32 qq = 1;
					if (demo::Demo3DHostShadowCfg(&nb, &sb, &so, &qq)) {
						if (q)
							qq = (int32)std::atoi(q);
						if (sf)
							so = (float32)std::atof(sf);
						demo::Demo3DHostSetShadowCfg(nb, sb, so, qq);
						st.shadowQual = qq;
					}
				}
			}
		}
		// NK_LIGHT_ATT=<0|1> : loi d'attenuation de TOUTES les lumieres, par le
		// setter du panneau (no-op sur les noeuds non-lumiere) — pour l'A/B
		// heritee vs physique face a Blender.
		{
			static bool sAgentAttDone = false;
			if (!sAgentAttDone && agentFrame >= 10 && demo::Demo3DHostReady()) {
				sAgentAttDone = true;
				if (const char *v = std::getenv("NK_LIGHT_ATT")) {
					// "mode[,watts]" : en physique, l'intensite devient des watts —
					// on peut donc poser « comme Blender » (ex. 1,1000).
					int32 mode = 0;
					float32 watts = -1.f;
					std::sscanf(v, "%d,%f", &mode, &watts);
					for (int32 n = 0; n < 1024; ++n) {
						demo::Demo3DHostSetLightAttMode(n, mode);
						if (watts > 0.f) {
							float32 c3[3];
							float32 i3 = 0.f;
							if (demo::Demo3DHostUserLightParams(n, c3, &i3))
								demo::Demo3DHostSetUserLightParams(n, c3, watts);
						}
					}
				}
				// NK_SHADOW_LINEAR=<0|1> : profondeur d'ombre lineaire de TOUTES
				// les lumieres (no-op hors omni cote rendu) — pour l'A/B projete
				// vs lineaire par captures, sans passer par le panneau.
				if (const char *v2 = std::getenv("NK_SHADOW_LINEAR")) {
					const bool lin = std::atoi(v2) != 0;
					for (int32 n = 0; n < 1024; ++n)
						demo::Demo3DHostSetLightShadowLinear(n, lin);
				}
			}
		}
		// NK_SSAO="0|1[,rayon[,intensite]]" : l'occlusion ambiante par le MEME
		// setter que le panneau — pour l'A/B d'agent du bouton Actif.
		{
			static bool sAgentSSAODone = false;
			if (!sAgentSSAODone && agentFrame >= 10 && demo::Demo3DHostReady()) {
				sAgentSSAODone = true;
				if (const char *v = std::getenv("NK_SSAO")) {
					int32 on = 0;
					float32 rad = 0.5f, inten = 1.f;
					std::sscanf(v, "%d,%f,%f", &on, &rad, &inten);
					demo::Demo3DHostSetSSAO(on != 0, rad, inten);
				}
			}
		}
		// NK_MAT_SURFACE="cc,ccRough,sss" : physique de surface du materiau par
		// defaut, par le MEME setter que le panneau. Applique une fois, APRES
		// l'eventuelle ouverture de projet (frame 10) : la relecture d'un
		// .nkmat repasserait par-dessus.
		{
			static bool sAgentMatDone = false;
			if (!sAgentMatDone && agentFrame >= 10 && demo::Demo3DHostReady()) {
				sAgentMatDone = true;
				if (const char *v = std::getenv("NK_MAT_SURFACE")) {
					float32 cc = 0.f, ccR = 0.f, sss = 0.f;
					std::sscanf(v, "%f,%f,%f", &cc, &ccR, &sss);
					// TOUS les emplacements utilises : l'agent ne sait pas lequel
					// porte le cube de la scene, et un reglage de test n'a pas a
					// le deviner.
					const int32 mx = demo::Demo3DHostProjMatMax();
					for (int32 m = 0; m < mx; ++m)
						demo::Demo3DHostProjMatSetSurface(m, cc, ccR, sss);
				}
			}
		}

		// ── CAPTURES, une fois l'image envoyee ──────────────────────────────
		// « Capturer la vue » fige la cible hors ecran de la vue 3D (la scene
		// seule, sans interface) ; « Tutoriel » photographie TOUTE la fenetre
		// via l'OS. PNG numerotes dans captures/ du projet (regle de Rihen).
		if (st.capturePending) {
			const int32 capMode = st.capturePending;
			st.capturePending = 0;
			char capPath[256];
			if (capMode == 1) {
				// « CAPTURER LA VUE » FAIT LE MEME TRAVAIL QUE « RENDRE »
				// (Rihen) : meme resolution, meme source, meme echelle, memes
				// incrustations, memes types de rendu -- seul le nom du fichier
				// change. Il figeait auparavant l'ecran tel quel, ce qui donnait
				// deux verites pour un seul acte : une image a la taille de la
				// fenetre a cote d'une image aux reglages de sortie.
				// Repli sur l'ancienne capture si la vue 3D n'est pas prete.
				if (demo::Demo3DHostReady())
					demo::Demo3DHostRenderOutputAs(1); // trace son resultat au journal
				else if (NkNextCapturePath("vue", capPath, (int32)sizeof(capPath)))
					demo::Demo3DHostCaptureView(capPath);
			} else {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
				// TUTORIEL SUIT LA MEME DESTINATION (Rihen) : la seule
				// difference tient a CE QU'ON PHOTOGRAPHIE -- toute la fenetre,
				// interface comprise, au lieu de la seule scene. Le dossier, le
				// nom et la numerotation sont ceux de la sortie : une seule
				// destination configuree dans l'application, une seule
				// convention.
				const bool okPath2 =
					demo::Demo3DHostReady()
						? demo::Demo3DHostOutNextPath(capPath, (int32)sizeof(capPath), 2)
						: NkNextCapturePath("tutoriel", capPath, (int32)sizeof(capPath));
				if (okPath2) {
					const bool okCap = NkCaptureWholeWindow(window, capPath);
					// MEME RESOLUTION DE SORTIE que le rendu : la fenetre est
					// photographiee a sa taille -- c'est sa nature -- puis
					// ramenee au format demande. Sans cela, « tutoriel » etait
					// le seul des trois a ignorer les reglages (Rihen).
					if (okCap && demo::Demo3DHostReady()) {
						int32 ew = 0, eh = 0;
						demo::Demo3DHostOutEffectiveSize(&ew, &eh);
						NkImage shot;
						if (ew > 0 && eh > 0 && shot.Load(capPath) &&
							(shot.Width() != ew || shot.Height() != eh)) {
							// Resize RETOURNE une nouvelle image (il ne modifie
							// pas l'objet) : l'ancien appel etait un no-op muet.
							NkImage *rs = shot.Resize((int32)ew, (int32)eh,
													  NkResizeFilter::NK_BICUBIC);
							if (rs) {
								rs->Save(capPath);
								rs->Free();
							}
						}
					}
					std::printf("[NK3DModeler] Capture tutoriel -> %s : %s\n", capPath,
								okCap ? "ecrite" : "ECHEC");
				}
#else
				std::printf("[NK3DModeler] Capture tutoriel : pas encore portee sur cette plateforme\n");
#endif
			}
		}

		// ── UNE VIGNETTE FRAICHEMENT ENCODEE REJOINT SON FICHIER ────────────
		// Elle est encodee une a deux frames APRES le geste qui l'a demandee ;
		// si ce geste etait l'enregistrement, le .nkmat est deja ecrit et ne la
		// contient pas. On reecrit alors ce seul materiau -- sinon le fichier
		// garderait la vignette de l'etat precedent jusqu'a la sauvegarde
		// suivante (constate le 14 aout : albedo rouge, vignette verte).
		if (proj.open && !proj.root.Empty()) {
			const int32 mDirty = demo::Demo3DHostMatThumbTakeDirty();
			if (mDirty >= 0) {
				for (int32 b = 0; b < st.browserCount; ++b)
					if (st.browserKind[b] == 2 && st.browserMat[b] == mDirty + 1) {
						NkString errV;
						// SUSPENDU pendant l'ecriture : c'est une vignette qui l'a
						// declenchee ; en redemander une relancerait la meme chaine
						// sans fin.
						demo::Demo3DHostMatThumbSuspend(true);
						const bool okV = nk3d::NkProjectWriteAssets(proj.root, st, &errV, b);
						demo::Demo3DHostMatThumbSuspend(false);
						if (!okV)
							nkentseu::NkLog::Instance().Info(
								"[apercu] vignette : reecriture impossible : {0}", errV.CStr());
						break;
					}
			}
		}

		// ── APERCUS DES MATERIAUX DU PROJET (ids 4400+) ─────────────────────
		// Meme mecanique que les vignettes de matcap : l'hote rend la vignette
		// en pixels quand elle est PERIMEE (parametres ou forme changes), et on
		// l'uploade comme n'importe quelle image d'interface.
		//
		// 256 px et non 128 : l'apercu suit desormais la LARGEUR du panneau de
		// proprietes (Rihen, 13 aout) et depasse largement les 104 px d'avant des
		// que le panneau est elargi. Retrecir une image reste propre, l'agrandir
		// non -- a 128 la sphere devenait molle des qu'on tirait la poignee.
		//
		// RECTANGULAIRE, et rendu a la taille EXACTE d'affichage. La largeur
		// vient du panneau (`st.matPrevW`), qui seul la connait ; la hauteur est
		// fixe, et c'est elle qui dimensionne l'objet -- elargir le panneau
		// etend le damier sans grossir la sphere. Rendre au 1:1 evite a la fois
		// l'etirement et le flou d'un agrandissement.
		//
		// CARREES, et c'est desormais leur seul usage : les CARTES du navigateur.
		// Le grand apercu du panneau ne passe plus par ici -- il est rendu par le
		// moteur (kNkMatPreviewTexId). Les avoir faites rectangulaires pour lui a
		// aussitot etire les cartes, qui sont carrees : « on a comme des
		// etirements sur les miniatures et ca deforme les spheres » (Rihen,
		// 13 aout). Une vignette doit avoir le format de l'endroit ou elle est
		// posee, et ces deux endroits n'ont pas le meme.
		{
			static const int32 kCarte = 128;
			static uint8 sMatBall[kCarte * kCarte * 4];
			// ── LA VIGNETTE CAPTUREE PASSE AVANT LE RENDU ANALYTIQUE ────────
			// Si le materiau porte une vignette -- une capture du VRAI rendu,
			// prise a son enregistrement -- c'est elle qui fait foi : elle seule
			// montre le verre comme du verre. Le rendu analytique reste le repli
			// pour un materiau jamais enregistre, qui n'a donc pas encore d'image.
			static NkString sVigVue[64];
			for (int32 i = 0; i < 64; ++i) {
				// UNE VIGNETTE FRAICHE PASSE AVANT TOUT : rendue il y a une frame
				// parce qu'un reglage a change, elle n'attend pas l'enregistrement.
				const uint8 *frais = nullptr;
				int32 cote = 0;
				if (demo::Demo3DHostMatThumbTakePixels(i, &frais, &cote) && frais &&
					cote > 0) {
					renderer.UploadImageRGBA(4400u + (uint32)i, frais, cote, cote);
					// LE TEMOIN PREND LE BASE64 COURANT, il ne se vide PAS. Le vider
					// -- ce que je faisais -- redemandait le decodage a la frame
					// suivante, et l'ancienne image enregistree ecrasait aussitot
					// celle qu'on venait de rendre : la vignette semblait ne jamais
					// suivre (Rihen, 14 aout, capture a l'appui -- sphere verte dans
					// l'apercu, bleue sur la carte).
					// En le posant, on declare l'affichage A JOUR : le base64 ne sera
					// redecode que s'il CHANGE, c'est-a-dire au prochain
					// enregistrement.
					const char *cur = demo::Demo3DHostProjMatThumb(i);
					sVigVue[i] = (cur && *cur) ? cur : "";
					continue;
				}
				const char *b64 = demo::Demo3DHostProjMatThumb(i);
				if (b64 && *b64) {
					// Ne decoder QUE si elle a change : decoder un PNG par materiau
					// et par frame couterait bien plus que tout le navigateur.
					if (sVigVue[i] != b64) {
						sVigVue[i] = b64;
						NkVector<uint8> png;
						png.Resize(((usize)sVigVue[i].Size() * 3u) / 4u + 4u);
						usize taille = png.Size();
						if (encoding::base64::NkDecode(sVigVue[i].CStr(), png.Data(), &taille)) {
							NkImage im;
							if (im.LoadFromMemory(png.Data(), taille) && im.IsValid())
								renderer.UploadImageRGBA(4400u + (uint32)i, im.Pixels(),
														 (int32)im.Width(), (int32)im.Height());
						}
					}
					continue; // pas de rendu analytique : la capture fait foi
				}
				sVigVue[i].Clear();
				if (demo::Demo3DHostProjMatPreviewTake(i, sMatBall, (uint32)kCarte,
													   (uint32)kCarte))
					renderer.UploadImageRGBA(4400u + (uint32)i, sMatBall, kCarte, kCarte);
			}
		}

		// ── MINIATURES DES SCENES (ids 4500+) ───────────────────────────────
		// Chargees du PNG « Apercus/<nom>.png » du projet quand elles sont
		// A (RE)CHARGER — a l'ouverture du projet (DocAlloc les met a 0) et
		// apres chaque enregistrement (NkAsSceneThumbCapture remet a 0). Un
		// PNG absent est note une fois pour toutes (2) : pas de tentative de
		// lecture disque a chaque image.
		if (proj.open) {
			for (int32 d2 = 0; d2 < nk3d::NkModelerState::kMaxDocs; ++d2) {
				if (!st.docUsed[d2] || st.docTransient[d2] || st.docThumb[d2] != 0)
					continue;
				st.docThumb[d2] = 2; // absente, sauf preuve du contraire
				nk3d::NkString rel("Apercus/");
				rel += nk3d::NkAsSafeName(st.docName[d2]);
				rel += ".png";
				NkImage im;
				if (!im.Load(nk3d::NkScToAbs(proj.root, rel.CStr()).CStr()))
					continue;
				if (im.Width() > 0 && im.Height() > 0) {
					renderer.UploadImageRGBA(4500u + (uint32)d2, im.Pixels(),
											 im.Width(), im.Height());
					st.docThumbW[d2] = (uint16)im.Width();
					st.docThumbH[d2] = (uint16)im.Height();
					st.docThumb[d2] = 1;
				}
			}
		}

		// ── ACTIONS DE FENETRE, HORS FRAME ──────────────────────────────────
		// BeginDragMove et Maximize entrent dans une boucle modale de l'OS : les
		// appeler pendant la peinture reentrerait dans la frame. On les consomme
		// donc ICI, une fois l'image envoyee.
		if (st.wantMinimize) {
			st.wantMinimize = false;
			window.Minimize();
		}
		// MODE DAEMON : la fenetre se cache, le processus continue d'encoder.
		if (st.wantHideWindow) {
			st.wantHideWindow = false;
			window.SetVisible(false);
		}
		// ── FIN D'ENCODAGE : notifier, ou fermer si c'etait la consigne ─────
		// Front descendant par prise : quand une passe finale se termine, soit
		// on previent (dialogue « Video terminee », demande de Rihen), soit --
		// si la fermeture attendait l'encodage -- on eteint l'application une
		// fois TOUT le travail video fini.
		{
			static bool sPrevEncV = false, sPrevEncT = false;
			const bool encV = demo::Demo3DHostRecEncoding();
			const bool encT = demo::Demo3DHostRecTutoEncoding();
			const char *donePath = nullptr;
			if (sPrevEncV && !encV)
				donePath = demo::Demo3DHostRecPath();
			if (sPrevEncT && !encT)
				donePath = demo::Demo3DHostRecTutoPath();
			sPrevEncV = encV;
			sPrevEncT = encT;
			if (donePath) {
				if (st.closeAfterEncode) {
					const bool stillBusy = encV || encT || demo::Demo3DHostRecActive() ||
										   demo::Demo3DHostRecTutoActive();
					if (!stillBusy)
						st.running = false;
				} else {
					uint32 i = 0;
					for (; donePath[i] && i + 1 < sizeof(st.encodeDonePath); ++i)
						st.encodeDonePath[i] = donePath[i];
					st.encodeDonePath[i] = 0;
					st.encodeDone = true;
				}
			}
		}
		if (st.wantMaxRestore) {
			st.wantMaxRestore = false;
			if (window.IsMaximized())
				window.Restore();
			else
				window.Maximize();
			st.maximized = window.IsMaximized();
		}
		if (st.wantDragMove) {
			st.wantDragMove = false;
			// TIRER UNE FENETRE MAXIMISEE LA RESTAURE, puis la deplace. C'est le
			// comportement de toutes les fenetres du systeme, et le refuser -- ce que
			// faisait la version precedente -- donne une barre de titre morte une fois
			// sur deux sans rien qui l'explique.
			//
			// On replace la fenetre restauree SOUS LE CURSEUR avant de rendre la main
			// a l'OS : sans cela elle saute en haut a gauche et la suite du geste
			// l'emmene ailleurs que la ou on croyait l'avoir attrapee. On conserve la
			// fraction horizontale du point saisi -- attraper la barre a droite doit
			// laisser le curseur a droite de la fenetre restauree.
			if (window.IsMaximized()) {
				const math::NkVec2 m = ui.input.mousePos;
				window.Restore();
				const math::NkVec2u sz = window.GetSize();
				const int32 nx = (int32)(m.x - (float32)sz.x * st.dragFracX);
				const int32 ny = (int32)(m.y - 12.f); // le curseur reste dans la barre
				window.SetPosition(nx < 0 ? 0 : nx, ny < 0 ? 0 : ny);
				st.maximized = false;
			}
			window.BeginDragMove();
		}
	}

	demo::Demo3DHostShutdown();
	nk3d::Viewport3DShutdown();
	renderer.Shutdown();
	ui.Shutdown();
	return 0;
}
