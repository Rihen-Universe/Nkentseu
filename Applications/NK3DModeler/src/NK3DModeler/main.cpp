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
#include "NK3DModeler/Shell/NkModelerBrowser.h" // navigateur de contenu
#include "NK3DModeler/Shell/NkModelerImport.h"  // import de fichiers 3D (bouton Importer)
#include "NK3DModeler/Shell/NkModelerMenus.h"   // menus deroulants
// ECRAN D'ACCUEIL + socle PROJET (.nk3dm) : l'accueil est peint tant qu'aucun
// projet n'est ouvert, et il porte l'execution differee des actions projet.
#include "NK3DModeler/Shell/NkModelerWelcome.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkDropEvent.h" // NkDropFileEvent : fichiers laches depuis l'explorateur
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
		// LE CHOIX DU PARTAGE (decision de Rodolf, 16 aout). Shift+D partage la
		// geometrie -- c'est le DEFAUT, et le geste courant. Ctrl+Shift+D en fait
		// une copie INDEPENDANTE, pour retoucher l'un sans l'autre. Le defaut
		// garde le raccourci le plus court parce que c'est lui qu'on fait cent
		// fois (array, decor, foule) ; l'exception paie un modificateur de plus.
		t.Bind("objet.dupliquer_independant", "Dupliquer (copie independante)",
			   NkKey::NK_D, (uint8)(NK_SC_SHIFT | NK_SC_CTRL), NK_SCTX_OBJECT);
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
	// LACHER DE FICHIERS DEPUIS LE SYSTEME : la fenetre s'inscrit comme cible
	// OLE (NkWin32DropTarget) et NkDropFileEvent arrive dans la file. Sans ce
	// drapeau, l'explorateur montre le curseur « interdit » et rien n'arrive --
	// c'etait l'ecoute qui manquait (contrat d'import, point 2).
	wc.dropEnabled = true;

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
		// FICHIERS LACHES DEPUIS L'EXPLORATEUR : memes coordonnees client que
		// la souris (ScreenToClient cote Win32). On RANGE, la boucle route une
		// fois les rects de la frame connus (NkOsDropRoute) -- l'evenement
		// arrive avant la mise en page.
		ev.AddEventCallback<NkDropFileEvent>([&st](NkDropFileEvent *e) {
			st.osDropCount = 0;
			for (usize i = 0; i < e->data.paths.Size() &&
							  st.osDropCount < nk3d::NkModelerState::kMaxOsDrop; ++i)
				snprintf(st.osDropPaths[st.osDropCount++], sizeof(st.osDropPaths[0]), "%s",
						 e->data.paths[i].CStr());
			st.osDropX = (float32)e->data.x;
			st.osDropY = (float32)e->data.y;
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
		// ── TEMOIN DU GLISSER-DEPOSER (crochets d'agent, 2026-08-18) ────────
		// NK_HIER_ROWS=<n>  : a la frame n, la hierarchie imprime ses lignes.
		// NK_AGENT_DRAG="f,x0,y0,x1,y1" (et NK_AGENT_DRAG2, une seconde course
		//   dans le meme lancement) : SURVOLE (x0,y0) a la frame f, PRESSE a
		//   f+1, glisse en 8 frames vers (x1,y1), RELACHE a f+10 -- pose l'etat
		//   souris BRUT que BeginFrame lit, exactement comme les evenements de
		//   la fenetre ; tout le reste (seuil, fantome, cibles, livraison) est
		//   le vrai code. A f+14 : rapport `[nk3d-drag] node=.. parent=..` de
		//   tous les noeuds vivants + compte du navigateur.
		{
			static int32 sRowsFrame = -2, sDragFrame[2] = {-2, -2};
			static float32 sDx0[2], sDy0[2], sDx1[2], sDy1[2];
			if (sRowsFrame == -2) {
				const char *v = std::getenv("NK_HIER_ROWS");
				sRowsFrame = v ? (int32)std::atoi(v) : -1;
			}
			for (int32 c = 0; c < 2; ++c) {
				if (sDragFrame[c] != -2)
					continue;
				sDragFrame[c] = -1;
				if (const char *v = std::getenv(c == 0 ? "NK_AGENT_DRAG" : "NK_AGENT_DRAG2")) {
					float32 f[5] = {0.f, 0.f, 0.f, 0.f, 0.f};
					const char *q = v;
					for (int32 k = 0; k < 5 && *q; ++k) {
						f[k] = (float32)atof(q);
						while (*q && *q != ',')
							++q;
						if (*q == ',')
							++q;
					}
					sDragFrame[c] = (int32)f[0];
					sDx0[c] = f[1];
					sDy0[c] = f[2];
					sDx1[c] = f[3];
					sDy1[c] = f[4];
				}
			}
			if (sRowsFrame > 0 && agentFrame + 1 == sRowsFrame) {
				st.hierTraceRows = true;
				st.browTraceCards = true;
			}
			for (int32 c = 0; c < 2; ++c) {
				if (sDragFrame[c] <= 0)
					continue;
				const int32 k = agentFrame + 1 - sDragFrame[c]; // frame relative
				// k=0 : SURVOL sans appui (comme une vraie main : le survol precede
				// le clic d'au moins une frame -- hotIdPrev) ; k=1 : appui ; k=2..9
				// glissement ; k=10 : relachement sur place.
				if (k >= 0 && k <= 10) {
					const float32 t = k <= 2 ? 0.f : (k >= 9 ? 1.f : (float32)(k - 2) / 7.f);
					ui.input.mousePos = {sDx0[c] + (sDx1[c] - sDx0[c]) * t,
										 sDy0[c] + (sDy1[c] - sDy0[c]) * t};
					ui.input.mouseDown[0] = (k >= 1 && k <= 9);
					printf("[nk3d-drag] c=%d k=%d pos=(%.0f,%.0f) down=%d dragActive=%d type=%s\n", c,
						   k, ui.input.mousePos.x, ui.input.mousePos.y,
						   ui.input.mouseDown[0] ? 1 : 0, ui.dragActive ? 1 : 0, ui.dragType);
				}
				if (k == 14) {
					const int32 nn = demo::Demo3DHostNodeCount();
					for (int32 n = 0; n < nn; ++n) {
						if (nk3d::NkHierNodeSkip(n))
							continue;
						char nm[48];
						nk3d::NkHierNodeName(st, n, nm, sizeof(nm));
						printf("[nk3d-drag] node=%d name=\"%s\" parent=%d sel=%d\n", n, nm,
							   demo::Demo3DHostNodeParent(n),
							   n >= 90 ? (demo::Demo3DHostEmptyNodeSelected(n) ? 1 : 0)
									   : (demo::Demo3DHostObjectSelected(n) ? 1 : 0));
					}
					printf("[nk3d-drag] browserCount=%d\n", st.browserCount);
					for (int32 b = 0; b < st.browserCount; ++b)
						printf("[nk3d-drag] brow=%d kind=%d parent=%d name=\"%s\"\n", b,
							   st.browserKind[b], st.browserParent[b], st.browserNames[b]);
					printf("[nk3d-drag] browAskIdx=%d browAskDest=%d folder=%d\n",
						   st.browAskIdx, st.browAskDest, st.browserFolder);
					fflush(stdout);
				}
			}
		}
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
		// ── LE JOURNAL SUSPEND CE QU'IL RECOUVRE, AU MEME ENDROIT ───────────
		// Il n'est pas modal -- le reste de l'application doit rester utilisable
		// -- mais SOUS LUI plus rien ne doit repondre. J'avais vide l'input plus
		// bas, juste avant de peindre les panneaux : trop tard. `hit.Begin` a
		// deja recopie l'input dans le registre a cet instant, et le navigateur
		// interroge le REGISTRE (`hit.RightClicked`) autant que l'input. Son menu
		// contextuel s'ouvrait donc encore a travers le journal (Rihen, 14 aout,
		// trois fois de suite).
		// Le vidage doit precede `hit.Begin`, comme celui des modales -- c'est le
		// seul endroit ou l'etancheite vaut a la fois pour le registre et pour le
		// code qui lit l'input directement.
		const NkRect jRectSuspend =
			nk3d::NkJournalRect({0.f, 0.f, (float32)lastW, (float32)lastH - S(26.f)});
		const bool sourisSurJournal =
			st.journalOpen && nkgui::NkGuiRectContains(jRectSuspend, ui.input.mousePos);
		if (modalOpen || sourisSurJournal) {
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
		// L'appel dormant prenait DEUX parametres ; la facade vivante les separe
		// en deux reglages distincts (mode d'affichage, couleur du mode solide).
		demo::Demo3DHostSetShading(st.shading);
		demo::Demo3DHostSetUnlitColor(st.solidLight);
		// ⚠️ APPEL RETIRE, ET IL N'Y A RIEN A PORTER : `st.overlayMask` est DEJA
		// route vers la vue vivante plus bas (SetGridFlags / SetOutline / SetHud /
		// SetCursorShown). Cette ligne l'envoyait EN PLUS a la vue morte.
		// 🔴 ET LES DEUX COTES NE LISENT PAS LES MEMES BITS. Vivant : 1 grille,
		// 2 mineures, 4 majeures, 8 axes, 16 contour, 32 HUD, 64 curseur. Mort
		// (NkViewport3D.cpp:264) : 1 grille, 2 axes, 4 contour, 8 gizmos,
		// 16 normales, 32 stats, 64 fil de fer. Le bit 4 veut dire « majeures »
		// d'un cote et « contour » de l'autre, le bit 16 « contour » puis
		// « normales ». DEUX VOCABULAIRES POUR LE MEME ENTIER : tant que le second
		// lecteur etait mort, personne ne pouvait le voir.
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
			// ── MESURE : LES DEUX SOURCES DE POSITION SOURIS ────────────────
			// NK_MOUSE_TRACE=1. Le CLIC lit `NkInput.MouseX()` dans la vue 3D ;
			// le LACHER du navigateur lit `hit.Mouse()`, alimente par
			// `NkMouseMoveEvent`. Les deux soustraient ensuite la MEME origine
			// (`viewImg.x/y`). Si les deux sources divergent, le lacher vise un
			// autre pixel que le clic au meme endroit de l'ecran -- et un pick
			// qui ne touche rien explique a lui seul « le vide », « la mauvaise
			// position », « pas d'enfant » et « le materiau ne fait rien ».
			// Trace TEMPORAIRE : elle sort une ligne par deplacement.
			{
				static int32 mtOn = -1;
				if (mtOn < 0)
					mtOn = (std::getenv("NK_MOUSE_TRACE") != nullptr) ? 1 : 0;
				if (mtOn == 1) {
					static float32 lastX = -1e9f, lastY = -1e9f;
					const float32 sx = ui.input.mousePos.x, sy = ui.input.mousePos.y;
					const float32 ix = (float32)nkentseu::NkInput.MouseX();
					const float32 iy = (float32)nkentseu::NkInput.MouseY();
					if (sx != lastX || sy != lastY) {
						lastX = sx;
						lastY = sy;
						nkentseu::NkLog::Instance().Info(
							"[nk3d] MESURE souris : shell=({0}, {1}) input=({2}, {3}) "
							"ecart=({4}, {5}) origine=({6}, {7}) vueShell=({8}, {9}) "
							"vueInput=({10}, {11}) viewRect=({12}, {13}, {14}, {15})\n",
							sx, sy, ix, iy, sx - ix, sy - iy, viewImg.x, viewImg.y,
							sx - viewImg.x, sy - viewImg.y, ix - viewImg.x, iy - viewImg.y,
							st.viewRect.x, st.viewRect.y, st.viewRect.w, st.viewRect.h);
					}
				}
			}
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
		// LE MODE, ET NON UN BOOLEEN. Cette ligne faisait `st.mode != Object` :
		// elle repliait SEPT modes en DEUX etats *et* les envoyait a la vue
		// DORMANTE. Deux fautes distinctes -- corriger la seule destination
		// aurait laisse Sculpture et Sculpture 2.5D indiscernables a l'arrivee,
		// alors qu'elles n'ont pas les memes exigences de topologie.
		demo::Demo3DHostSetMode((int32)st.mode);
		// Le sous-mode de la vue devient le masque de selection. Un seul bit ici :
		// les trois boutons sont exclusifs. Les combiner (Maj+1/2/3 chez Blender)
		// viendra avec les raccourcis clavier.
		demo::Demo3DHostSetSelectMask(1u << (uint32)st.subMode);
		// Outil -> mode du gizmo. « Selection » et « Curseur » n'en ont pas : on
		// laisse alors le gizmo sur le deplacement, mais il ne prendra pas le clic
		// puisque l'arbitrage donne la priorite au maillage.
		{
			int32 gm = 0;
			if (st.tool == NkTool::Rotate)
				gm = 1;
			else if (st.tool == NkTool::Scale)
				gm = 2;
			demo::Demo3DHostSetGizmoOp(gm);
			// L'outil SELECTION ne transforme rien : afficher ses poignees ferait
			// croire le contraire, et elles captureraient les clics de selection.
			// SENS INVERSE : la facade vivante parle en « cache », la morte en
		// « visible ». Repointer sans nier aurait montre le gizmo exactement
		// quand il faut le cacher -- et l'erreur se serait vue comme un gizmo
		// qui clignote au changement d'outil, pas comme un appel inverse.
		demo::Demo3DHostSetGizmoHidden(!(st.tool != NkTool::Select && st.tool != NkTool::Cursor));
		}
		demo::Demo3DHostSetOrientation(st.orientation);
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
			demo::Demo3DHostSetSnap(snapOn, 0.5f, 15.f, 0.1f);
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
			// ⚠️ APPEL RETIRE, ET IL N'Y A RIEN A PORTER : DEUX CANAUX DISJOINTS.
			// Il remplissait `g.gin`, l'entree du gizmo de la vue MORTE, depuis
			// `ui.input` (l'etat souris de NKGui). Le viseur VIVANT ne lit pas ce
			// canal : il construit son propre `gin` depuis `NkInput`, le singleton
			// plateforme (`NkDemo3D.cpp:8222` et `:9582`). Ce fait est deja ecrit
			// dans le viseur a propos de NK_AGENT_DRAG : « deux canaux disjoints ».
			// Les suivis ci-dessous (gizLastX/Y, gizWasDown) restent : ils servent
			// au shell lui-meme pour savoir si un geste a commence DANS le viseur.
			(void)mxv;
			(void)myv;
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

		// NK_STATS_TRACE=<frame> : imprime les DEUX sources de compteurs, dans la
		// MEME execution -- la vue morte que le panneau interrogeait, et la vue
		// vivante qu'il interrogera. Rouge et vert cote a cote, sans deux binaires.
		// ⚠️ LU TARD DANS L'IMAGE, et c'est deliberé : un COMPTEUR change PENDANT la
		// frame. Le lire avant la synchronisation du maillage rendrait des zeros
		// indiscernables d'un chemin mort -- je l'ai deja paye sur les modes.
		{
			static bool sStatsDone = false;
			if (const char *sv = std::getenv("NK_STATS_TRACE")) {
				const int32 fr = (int32)std::atoi(sv);
				if (!sStatsDone && agentFrame >= (fr > 0 ? fr : 150)) {
					sStatsDone = true;
					uint32 mv = 0, me = 0, mf = 0, mt = 0;
					nk3d::Viewport3DStats(mv, me, mf, mt);
					uint32 vv = 0, ve = 0, vf = 0, vt = 0;
					const bool ok = demo::Demo3DHostStats(&vv, &ve, &vf, &vt);
					std::printf("[nk3d] STATS vue MORTE   : v=%u e=%u f=%u t=%u\n",
								mv, me, mf, mt);
					std::printf("[nk3d] STATS vue VIVANTE : v=%u e=%u f=%u t=%u (ok=%d)\n",
								vv, ve, vf, vt, ok ? 1 : 0);
				}
			} else {
				sStatsDone = true;
			}
		}

		// NK_EDIT_MODE=<1>[,frame] : le MODE vient du shell, la CIBLE du viseur.
		// Le crochet cote viseur choisit l'objet a editer ; c'est ici que le mode
		// est POSE, par la meme porte que l'onglet et que TAB. Sans cela, le
		// viseur entrait en edition et le shell -- toujours en Objet -- l'en
		// faisait ressortir a l'image suivante.
		{
			static bool sEditModeDone = false;
			if (const char *em = std::getenv("NK_EDIT_MODE")) {
				int32 fr = 0;
				const char *c = em;
				while (*c && *c != ',')
					++c;
				if (*c == ',')
					fr = (int32)std::atoi(c + 1);
				if (!sEditModeDone && agentFrame >= fr && em[0] && em[0] != '0') {
					sEditModeDone = true;
					st.mode = NkMode::Edit;
				}
			} else {
				sEditModeDone = true;
			}
		}

		{
			static bool sMarkDone = false;
			if (const char *em2 = std::getenv("NK_EDGE_MARK")) {
				const int32 fr = std::atoi(em2) > 1 ? (int32)std::atoi(em2) : 100;
				if (!sMarkDone && agentFrame >= fr) {
					sMarkDone = true;
					(void)demo::Demo3DHostMarkAllEdges();
				}
			} else {
				sMarkDone = true;
			}
		}

		// NK_UI_MODE=<n>[,frame] : pose le MODE DE L'INTERFACE (valeur de NkMode),
		// par le meme chemin que l'onglet -- on ecrit `st.mode`, et la ligne qui
		// transmet au viseur fait le reste.
		// ⚠️ CE QUE CE TEMOIN DOIT MONTRER : que l'information n'est plus PERDUE.
		// Deux modes non-Objet DIFFERENTS (Sculpture=3, Texturing=4) doivent donner
		// deux etats distincts a l'arrivee. Avec l'ancien `st.mode != Object`, ils
		// rendaient tous deux `true` : un temoin qui n'aurait compare qu'Objet a
		// Edition serait passe au vert AVANT comme APRES, sans rien prouver.
		{
			static bool sUiModeDone = false;
			if (const char *um = std::getenv("NK_UI_MODE")) {
				int32 fr = 60;
				const char *c = um;
				while (*c && *c != ',')
					++c;
				if (*c == ',')
					fr = (int32)std::atoi(c + 1);
				if (!sUiModeDone && agentFrame >= fr) {
					sUiModeDone = true;
					st.mode = (NkMode)std::atoi(um);
				}
				// ⚠️ LIRE PLUS TARD, ET NON DANS LA MEME IMAGE. La ligne qui transmet
				// le mode au viseur tourne PLUS TOT dans la frame : relire aussitot
				// apres avoir pose `st.mode` rendait toujours la valeur PRECEDENTE, et
				// les six modes semblaient tous arriver a zero. L instrument lisait
				// AVANT que la chose n arrive : le defaut etait dans la MESURE, pas
				// dans le chemin mesure.
				static bool sUiModeLu = false;
				if (sUiModeDone && !sUiModeLu && agentFrame >= fr + 5) {
					sUiModeLu = true;
					std::printf("[nk3d] NK_UI_MODE shell=%d -> viseur=%d\n", (int)st.mode,
								(int)demo::Demo3DHostMode());
				}
			} else {
				sUiModeDone = true;
			}
		}

		// NK_ADD_NODE=<kind>[,sub[,frame]] : CREE UN OBJET dans la scene, par le
		// meme chemin que le menu « Ajouter » (Demo3DHostAddNode). kind 1..3
		// generent un vrai maillage ; l'objet nait au curseur 3D.
		//
		// POURQUOI CE LEVIER EXISTE (27/08). Le projet de capture ouvert par
		// NK_OPEN_RECENT=0 est VIDE : la barre d'etat affiche « 0 objet(s), 0
		// selectionne(s) ». Aucune capture ne pouvait donc montrer un contour de
		// selection, une carte de relief ni un materiau pose sur une face -- et,
		// pire, TOUTES les captures sortaient identiques au bit pres. Deux images
		// identiques « prouvaient » alors qu'un reglage etait mort, alors qu'elles
		// disaient seulement que rien n'avait jamais ete dessine.
		// UN INSTRUMENT INERTE CONFIRME LE DEFAUT QU'ON LUI SOUMET, QUEL QU'IL SOIT.
		// Sans de quoi PEUPLER la scene, les leviers NK_SEL_AT et NK_OUTLINE_THICK
		// existaient deja mais ne pouvaient rien prouver.
		{
			static bool sAddDone = false;
			if (const char *an = std::getenv("NK_ADD_NODE")) {
				int32 v[3] = {2, 0, 40};
				int32 k = 0;
				for (const char *p = an; k < 3 && *p;) {
					v[k++] = (int32)std::atoi(p);
					while (*p && *p != ',')
						++p;
					if (*p == ',')
						++p;
				}
				if (!sAddDone && agentFrame >= v[2]) {
					sAddDone = true;
					const int32 nd = demo::Demo3DHostAddNode(v[0], v[1]);
					// LE MENU SELECTIONNE IMMEDIATEMENT ce qu'il cree (« noeud
					// utilisateur nomme d'apres l'entree, selectionne
					// immediatement »). Ce crochet ne le faisait pas, et
					// l'objet naissait donc dans un etat que le produit ne
					// produit jamais -- de quoi rendre muette toute mesure qui
					// suppose une selection.
					if (nd >= 0)
						demo::Demo3DHostSelectEmptyNode(nd);
					std::printf("[nk3d] NK_ADD_NODE kind=%d sub=%d frame=%d -> noeud %d\n",
								(int)v[0], (int)v[1], (int)agentFrame, (int)nd);
				}
			} else {
				sAddDone = true;
			}
		}

		// NK_MOD_STACK="<t1>+<t2>+...[,frame]" : EMPILE des modificateurs par la
		// facade, c'est-a-dire par le MEME chemin que le panneau.
		// ⚠️ IL EN FAUT DEUX, PAS UN. Un modificateur seul prouverait qu'il
		// s'applique, pas que la PILE est respectee : l'ordre compte, et deux
		// modificateurs inverses ne donnent pas le meme maillage. Le temoin compare
		// donc « Mirror puis Array » a « Array puis Mirror ».
		{
			static bool sModDone = false;
			if (const char *ms = std::getenv("NK_MOD_STACK")) {
				int32 fr = 120;
				const char *v = ms;
				while (*v && *v != ',')
					++v;
				if (*v == ',')
					fr = (int32)std::atoi(v + 1);
				if (!sModDone && agentFrame >= fr) {
					sModDone = true;
					for (const char *p = ms; *p && *p != ',';) {
						const int32 t = (int32)std::atoi(p);
						const int32 i = demo::Demo3DHostModAdd(t);
						std::printf("[nk3d] NK_MOD_STACK ajoute type=%d -> index=%d (pile=%u)\n",
									(int)t, (int)i, (unsigned)demo::Demo3DHostModCount());
						while (*p && *p != '+' && *p != ',')
							++p;
						if (*p == '+')
							++p;
						else
							break;
					}
				}
			} else {
				sModDone = true;
			}
		}

		if (agentFrame > 60)
			demo::Demo3DHostXformTrace();
		// NK_NODES_TRACE=<frame> : l'inventaire se lit A LA FRAME DEMANDEE.
		// Il etait fige a 61 : toute mesure d'un geste declenche plus tard lisait
		// donc l'etat D'AVANT, et rendait des compteurs inchanges indiscernables
		// d'un geste sans effet. C'est la meme faute que sur les modes, au meme
		// endroit : l'instrument lisait avant que la chose n'arrive.
		{
			const char *nt = std::getenv("NK_NODES_TRACE");
			const int32 frNt = (nt && std::atoi(nt) > 1) ? (int32)std::atoi(nt) : 61;
			if (agentFrame >= frNt)
				demo::Demo3DHostNodesTrace();
		}

		// NK_VP_ACTION=<nom>[,frame] : declenche une ACTION DU SHELL (NkVpAction),
		// par le MEME chemin que le clavier et que les futurs boutons.
		// ATTENTION, C EST TOUTE LA DIFFERENCE QUE CE TEMOIN MESURE : les crochets
		// NK_EDIT_* parlent DIRECTEMENT a NkDemo3D et fonctionnent ; le chemin du
		// shell, lui, passe par Viewport3D* et ne fait rien, parce que cette vue
		// n a pas de device. Un temoin qui emprunterait NK_EDIT_* serait vert des
		// aujourd hui et ne mesurerait rien.
		// Le mode EDITION du shell est force ici : c est ce que fait l action
		// ToggleEdit, et l enchainer demanderait une sequence de crochets pour un
		// gain nul.
		{
			static bool sVpActDone = false;
			if (const char *vpa = std::getenv("NK_VP_ACTION")) {
				int32 fr = 140;
				const char *cm = vpa;
				while (*cm && *cm != ',')
					++cm;
				if (*cm == ',')
					fr = (int32)std::atoi(cm + 1);
				if (!sVpActDone && agentFrame >= fr) {
					sVpActDone = true;
					auto est = [&](const char *n) -> bool {
						const char *a = vpa;
						const char *b = n;
						while (*b) {
							char x = *a++, y = *b++;
							if (x >= 'A' && x <= 'Z')
								x = (char)(x - 'A' + 'a');
							if (x != y)
								return false;
						}
						return (*a == 0 || *a == ',');
					};
					// ⚠️ CE CROCHET NE FORCE PLUS LE MODE. Il posait `st.mode = Edit`
					// avant toute action : tout temoin mesurait donc l'action ET le
					// changement de mode, d'ou l'obligation d'un controle a nom inconnu
					// pour les separer. Pire, il rendait INTESTABLE toute action de mode
					// OBJET -- la suppression d'objet partait toujours dans la branche
					// edition. Le mode se pose desormais explicitement (NK_EDIT_MODE ou
					// NK_UI_MODE), par la porte unique.
					// On pose l ACTION, on n appelle pas la facade : le temoin doit
					// emprunter le chemin du BOUTON, pas un raccourci qui serait vert
					// meme si le bouton restait mort.
					if (est("togglexray"))
						st.pendingAction = NkVpAction::ToggleXray;
					else if (est("frameall"))
						st.pendingAction = NkVpAction::FrameAll;
					else if (est("viewfront"))
						st.pendingAction = NkVpAction::ViewFront;
					else if (est("viewtop"))
						st.pendingAction = NkVpAction::ViewTop;
					else if (est("viewright"))
						st.pendingAction = NkVpAction::ViewRight;
					else if (est("selectall"))
						st.pendingAction = NkVpAction::SelectAll;
					else if (est("selectnone"))
						st.pendingAction = NkVpAction::SelectNone;
					else if (est("submodeedge"))
						st.pendingAction = NkVpAction::SubModeEdge;
					else if (est("submodeface"))
						st.pendingAction = NkVpAction::SubModeFace;
					else if (est("undo"))
						st.pendingAction = NkVpAction::Undo;
					else if (est("redo"))
						st.pendingAction = NkVpAction::Redo;
					else if (est("subdivide"))
						st.pendingAction = NkVpAction::Subdivide;
					else if (est("extrude"))
						st.pendingAction = NkVpAction::Extrude;
					else if (est("inset"))
						st.pendingAction = NkVpAction::Inset;
					else if (est("bevel"))
						st.pendingAction = NkVpAction::BevelEdge;
					else if (est("delete"))
						st.pendingAction = NkVpAction::Delete;
					else
						puts("[nk3d] NK_VP_ACTION : nom inconnu, aucune action posee");
				}
			}
		}

		// NK_VP_ACTION2=<nom>[,frame] : une SECONDE action du shell, plus tard.
		// Necessaire pour le temoin d ANNULER, qui demande TROIS etats : avant,
		// apres l operation, apres l annulation. Un seul crochet ne pouvait pas
		// enchainer deux gestes, et un temoin d annulation sans operation prealable
		// ne mesure rien -- la pile serait vide et Annuler aurait raison de ne rien
		// faire. Meme chemin que le premier : on pose l ACTION, pas la facade.
		{
			static bool sVpAct2Done = false;
			if (const char *vpa = std::getenv("NK_VP_ACTION2")) {
				int32 fr = 160;
				const char *cm = vpa;
				while (*cm && *cm != ',')
					++cm;
				if (*cm == ',')
					fr = (int32)std::atoi(cm + 1);
				if (!sVpAct2Done && agentFrame >= fr) {
					sVpAct2Done = true;
					auto est = [&](const char *n) -> bool {
						const char *a = vpa;
						const char *b = n;
						while (*b) {
							char x = *a++, y = *b++;
							if (x >= 'A' && x <= 'Z')
								x = (char)(x - 'A' + 'a');
							if (x != y)
								return false;
						}
						return (*a == 0 || *a == ',');
					};
					// ⚠️ CE CROCHET NE FORCE PLUS LE MODE. Il posait `st.mode = Edit`
					// avant toute action : tout temoin mesurait donc l'action ET le
					// changement de mode, d'ou l'obligation d'un controle a nom inconnu
					// pour les separer. Pire, il rendait INTESTABLE toute action de mode
					// OBJET -- la suppression d'objet partait toujours dans la branche
					// edition. Le mode se pose desormais explicitement (NK_EDIT_MODE ou
					// NK_UI_MODE), par la porte unique.
					// On pose l ACTION, on n appelle pas la facade : le temoin doit
					// emprunter le chemin du BOUTON, pas un raccourci qui serait vert
					// meme si le bouton restait mort.
					if (est("togglexray"))
						st.pendingAction = NkVpAction::ToggleXray;
					else if (est("frameall"))
						st.pendingAction = NkVpAction::FrameAll;
					else if (est("viewfront"))
						st.pendingAction = NkVpAction::ViewFront;
					else if (est("viewtop"))
						st.pendingAction = NkVpAction::ViewTop;
					else if (est("viewright"))
						st.pendingAction = NkVpAction::ViewRight;
					else if (est("selectall"))
						st.pendingAction = NkVpAction::SelectAll;
					else if (est("selectnone"))
						st.pendingAction = NkVpAction::SelectNone;
					else if (est("submodeedge"))
						st.pendingAction = NkVpAction::SubModeEdge;
					else if (est("submodeface"))
						st.pendingAction = NkVpAction::SubModeFace;
					else if (est("undo"))
						st.pendingAction = NkVpAction::Undo;
					else if (est("redo"))
						st.pendingAction = NkVpAction::Redo;
					else if (est("subdivide"))
						st.pendingAction = NkVpAction::Subdivide;
					else if (est("extrude"))
						st.pendingAction = NkVpAction::Extrude;
					else if (est("inset"))
						st.pendingAction = NkVpAction::Inset;
					else if (est("bevel"))
						st.pendingAction = NkVpAction::BevelEdge;
					else if (est("delete"))
						st.pendingAction = NkVpAction::Delete;
					else
						puts("[nk3d] NK_VP_ACTION2 : nom inconnu, aucune action posee");
				}
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
					demo::Demo3DHostSelectAll(true);
					break;
				case NkVpAction::SelectNone:
					demo::Demo3DHostSelectAll(false);
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
						if (demo::Demo3DHostEditDelete())
							NkMarkDirty(st);
					} else {
						// SUPPRESSION EN MODE OBJET. Elle visait l'objet actif de la vue
						// MORTE : la touche Suppr ne supprimait donc rien hors edition.
						// `withChildren = true` parce que la specification des modes le
						// dit : en mode objet, un clic prend le model ENTIER, tous ses
						// sous-mesh avec -- le supprimer sans eux laisserait des orphelins
						// invisibles occupant des emplacements.
						const int32 noeud = demo::Demo3DHostSelectedEmptyNode();
						if (noeud >= 0) {
							demo::Demo3DHostDeleteNode(noeud, true);
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
					// ON BASCULE DEPUIS LA VALEUR REELLE, pas depuis l'ombre : si les
					// deux divergeaient, partir de l'ombre demanderait DEUX appuis pour
					// repartir -- le defaut classique de l'etat duplique.
					st.xray = !demo::Demo3DHostXray();
					demo::Demo3DHostSetXray(st.xray);
					break;
				// Les operations n'ont de sens QU'EN EDITION. Les laisser passer en
				// mode objet donnerait des commandes sans effet, donc un journal
				// d'annulation qui se remplit de riens.
				case NkVpAction::Extrude:
					if (edit && demo::Demo3DHostEditExtrude(false))
						NkMarkDirty(st);
					break;
				case NkVpAction::ExtrudeIndividual:
					if (edit && demo::Demo3DHostEditExtrude(true))
						NkMarkDirty(st);
					break;
				case NkVpAction::Delete:
					// Supprime CE QUE le mode designe : les faces selectionnees en
					// edition, l'objet actif en mode objet.
					if (edit) {
						if (demo::Demo3DHostEditDelete())
							NkMarkDirty(st);
					} else {
						// SUPPRESSION EN MODE OBJET. Elle visait l'objet actif de la vue
						// MORTE : la touche Suppr ne supprimait donc rien hors edition.
						// `withChildren = true` parce que la specification des modes le
						// dit : en mode objet, un clic prend le model ENTIER, tous ses
						// sous-mesh avec -- le supprimer sans eux laisserait des orphelins
						// invisibles occupant des emplacements.
						const int32 noeud = demo::Demo3DHostSelectedEmptyNode();
						if (noeud >= 0) {
							demo::Demo3DHostDeleteNode(noeud, true);
							NkMarkDirty(st);
						}
					}
					break;
				case NkVpAction::Dissolve:
					if (edit && demo::Demo3DHostEditDissolve())
						NkMarkDirty(st);
					break;
				case NkVpAction::Merge:
					if (edit && demo::Demo3DHostEditMerge()) // 0 = au centre
						NkMarkDirty(st);
					break;
				case NkVpAction::MakeFace:
					if (edit && demo::Demo3DHostEditMakeFace())
						NkMarkDirty(st);
					break;
				case NkVpAction::Subdivide:
					if (edit && demo::Demo3DHostEditSubdivide())
						NkMarkDirty(st);
					break;
				case NkVpAction::LoopCut:
					if (edit && demo::Demo3DHostEditLoopCut())
						NkMarkDirty(st);
					break;
				case NkVpAction::Inset:
					// Epaisseur AUTOMATIQUE, proportionnelle a l'objet : une valeur
					// fixe donne un inset invisible sur un grand modele et un inset
					// qui traverse tout sur un petit.
					if (edit && demo::Demo3DHostEditInset())
						NkMarkDirty(st);
					break;
				case NkVpAction::BevelEdge:
					if (edit && demo::Demo3DHostEditBevel(false))
						NkMarkDirty(st);
					break;
				case NkVpAction::BevelVertex:
					if (edit && demo::Demo3DHostEditBevel(true))
						NkMarkDirty(st);
					break;
				// LE BOUTON PARLAIT A LA MAUVAISE PILE. `Viewport3DUndo` manipule
				// l historique de la vue DORMANTE (g.history), que rien n alimente ;
				// les operations commitent dans celui de la vue VIVANTE. D ou un
				// Annuler qui marchait au clavier et restait mort a la souris.
				case NkVpAction::Undo:
					if (edit && demo::Demo3DHostEditUndo())
						NkMarkDirty(st);
					break;
				case NkVpAction::Redo:
					if (edit && demo::Demo3DHostEditRedo())
						NkMarkDirty(st);
					break;
				// ── Vues ────────────────────────────────────────────────────
				case NkVpAction::ViewFront:
					demo::Demo3DHostAxisView(0, false);
					break;
				case NkVpAction::ViewBack:
					demo::Demo3DHostAxisView(0, true);
					break;
				case NkVpAction::ViewRight:
					demo::Demo3DHostAxisView(1, false);
					break;
				case NkVpAction::ViewLeft:
					demo::Demo3DHostAxisView(1, true);
					break;
				case NkVpAction::ViewTop:
					demo::Demo3DHostAxisView(2, false);
					break;
				case NkVpAction::ViewBottom:
					demo::Demo3DHostAxisView(2, true);
					break;
				case NkVpAction::ToggleOrtho:
					st.projection = (st.projection == 1) ? 0 : 1;
					break;
				case NkVpAction::FrameAll:
					demo::Demo3DHostFrameAll();
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
		// L'etancheite du journal est posee PLUS HAUT, avec celle des modales :
		// elle doit preceder `hit.Begin`. Ne reste ici que le blocage du
		// registre, utile aux widgets qui, eux, passent par lui.
		if (st.journalOpen)
			hit.SetBlock(jRect, true);

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
	if (modalOpen || sourisSurJournal) {
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
			// 2 = IMPORTER UN FICHIER 3D (bouton « Importer » du navigateur de
			// contenu). Chaine complete depuis le 17/08 : chargement par le
			// chargeur du format, decoupage par nom de sous-mesh, puis CREATION
			// (un maillage DIRECT par model d'une tranche, racine + maillages
			// sinon ; positions monde), ARCHIVAGE EN PLACE (rien dans la scene)
			// + carte navigateur + ECRITURE du `.nkmesh` par model, tout de
			// suite -- « un import ECRIT » (contrat de Rodolf du 17/08 soir,
			// NkModelerImport.h). Le bouton = import seul.
			if (st.pickerAction == 2 && st.picker.pickerResultPath[0])
				nk3d::NkImportFile(st, st.picker.pickerResultPath);
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
		// ── NK_SEL_NODES="frame,n1,n2,n3..." : SELECTION MULTIPLE DE NOEUDS ──
		// Crochet d'agent pose pour le defaut n.3 de Rodolf (18/08). Il n'existait
		// aucun moyen de scripter une selection multiple de MODELS : NK_GIZMO_MULTI
		// ne pilote que `st->gizmo` (les objets de demo, indices < kNumObj), alors
		// que tous les models et maillages importes vivent dans `emptyGizmo`
		// (noeuds >= kNkvpFirstEmpty). Le levier existant ne pouvait donc pas
		// atteindre le regime ou le defaut se produit.
		//
		// ⚠️ IL N'INVENTE AUCUN CHEMIN : il appelle EXACTEMENT les deux fonctions
		// que la ligne de hierarchie appelle sur un clic
		// (NkModelerHierarchy.h:1349-1352) -- `SelectEmptyNode` pour la premiere,
		// `ToggleEmptyNode` pour les suivantes, soit le Ctrl+clic reel. Mesurer une
		// reconstruction au lieu de la chose est la 4e facon dont un controle se
		// trompe ; ici la selection passe par le code de production.
		//
		// LA FRAME EST OBLIGATOIRE (dette connue : « les leviers d'agent ne disent
		// pas QUAND ») : appliquer au premier passage selectionnerait dans une scene
		// que NK_OPEN_RECENT n'a pas encore ouverte. Et l'application est UNIQUE --
		// rejouer `Toggle` a chaque frame ferait clignoter la selection.
		{
			static int32 sSelFrame = -2;
			static int32 sSelNodes[16];
			static int32 sSelCount = 0;
			if (sSelFrame == -2) {
				sSelFrame = -1;
				if (const char *v = std::getenv("NK_SEL_NODES")) {
					const char *q = v;
					sSelFrame = atoi(q);
					while (*q && *q != ',')
						++q;
					if (*q == ',')
						++q;
					while (*q && sSelCount < 16) {
						sSelNodes[sSelCount++] = atoi(q);
						while (*q && *q != ',')
							++q;
						if (*q == ',')
							++q;
					}
				}
			}
			if (sSelFrame > 0 && agentFrame == sSelFrame && demo::Demo3DHostReady()) {
				for (int32 s = 0; s < sSelCount; ++s) {
					if (s == 0)
						demo::Demo3DHostSelectEmptyNode(sSelNodes[s]);
					else
						demo::Demo3DHostToggleEmptyNode(sSelNodes[s]);
					printf("[nk3d-sel] demande noeud=%d selectionne=%d\n", sSelNodes[s],
						   demo::Demo3DHostEmptyNodeSelected(sSelNodes[s]) ? 1 : 0);
				}
				fflush(stdout);
				sSelFrame = -1; // une seule fois
			}
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
				// NK_MAT_TYPE=<valeur moteur> : le TYPE de tous les materiaux du
				// projet, par le MEME setter que le combo du panneau.
				// ⚠ POSE AVANT NK_MAT_SURFACE, ET CE N'EST PAS UN DETAIL : changer
				// le type REINITIALISE les parametres (c'est tout l'objet de
				// NKMatTypeResetTest). L'ordre inverse effacerait le reglage qu'on
				// vient de demander, et la mesure porterait sur les defauts du type.
				// Valeurs moteur : 0 PBR, 5 verre, 6 tissu, 7 carrosserie,
				// 11 emissif, 60 sans eclairage (cf. kNkMatTypeVal).
				if (const char *v = std::getenv("NK_MAT_TYPE")) {
					const int32 t = (int32)std::atoi(v);
					const int32 mx = demo::Demo3DHostProjMatMax();
					for (int32 m = 0; m < mx; ++m)
						demo::Demo3DHostProjMatSetType(m, t);
				}
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
		// NK_IMPORT_FILE=<chemin> : l'import par le MEME chemin que la
		// confirmation du picker (nk3d::NkImportFile, plus haut) -- pour
		// rejouer un import sans main, avant/apres correctif. Applique UNE
		// fois, hote pret, apres l'eventuelle ouverture de projet (frame 10),
		// comme les autres crochets de mesure. PERIMETRE, dit ici : couvre
		// charge -> decoupe -> creation -> archivage ; ne couvre NI le bouton
		// Importer NI le picker -- une relecture a la main reste necessaire
		// pour eux.
		{
			static bool sAgentImportDone = false;
			if (!sAgentImportDone && agentFrame >= 10 && demo::Demo3DHostReady()) {
				sAgentImportDone = true;
				if (const char *v = std::getenv("NK_IMPORT_FILE"))
					nk3d::NkImportFile(st, v);
				// NK_OS_DROP="x,y,<chemin>" : FABRIQUE le lacher OS a ces pixels
				// de fenetre, exactement comme NkDropFileEvent le range -- seul
				// le trajet depuis l'explorateur est simule ; le routage par
				// zone, l'import, le pick et l'instanciation sont les vrais.
				if (const char *v = std::getenv("NK_OS_DROP")) {
					float32 dx = 0.f, dy = 0.f;
					const char *q = v;
					dx = (float32)atof(q);
					while (*q && *q != ',')
						++q;
					if (*q == ',')
						++q;
					dy = (float32)atof(q);
					while (*q && *q != ',')
						++q;
					if (*q == ',')
						++q;
					if (*q) {
						st.osDropCount = 1;
						snprintf(st.osDropPaths[0], sizeof(st.osDropPaths[0]), "%s", q);
						st.osDropX = dx;
						st.osDropY = dy;
					}
				}
			}
		}
		// ── LACHER VENU DU SYSTEME : ROUTAGE PAR ZONE, PUIS REPONSE DU PICK ──
		// Les rects de la frame sont poses (hierRect, viewRect, browserRect) :
		// on peut dire OU le fichier a ete lache. Vue 3D -> import + pick
		// differe ; hierarchie -> import + instanciation aux coordonnees du
		// fichier ; navigateur -> import seul ; ailleurs -> refus nomme.
		if (demo::Demo3DHostReady()) {
			nk3d::NkOsDropRoute(st);
			nk3d::NkOsDropPickTake(st);
		}

		// ── LACHER DU NAVIGATEUR SUR LA VUE 3D : LA REPONSE DU PICK ARRIVE ──
		// Le jeton a ete fige au relachement (cf. NkModelerBrowser.h) ; il ne
		// reste qu'a lire OU l'utilisateur a lache et a appliquer. Rien n'est
		// relu dans le navigateur ici : tout ce que le geste utilise voyage
		// dans le jeton.
		//
		// LA TABLE, telle que Rodolf l'a specifiee :
		//   materiau  · vide -> rien             · objet -> assigne
		//   model     · vide -> AJOUTE A CETTE POSITION · objet -> menu
		//   texture   · vide -> rien             · objet -> refus NOMME
		//   scene/dossier/autre · vide -> rien   · objet -> refus NOMME
		//
		// AUCUNE nature ne reste muette sur un objet : un refus silencieux est
		// indistinguable d'un glisser-deposer casse (regle du depot, vague 27).
		//
		// ── MESURE : NK_DROP_TOKEN="carte,x,y[,frame]" ──────────────────────
		// FIGE LE JETON D'UNE VRAIE CARTE du navigateur, exactement comme le
		// relachement le fait, puis demande le pick a ces pixels de FENETRE.
		// Seul le TRAJET de la souris est fabrique : la nature, le noeud source
		// et l'emplacement de materiau sont lus dans le navigateur, pas
		// inventes. Sans lui, la suite du geste -- assignation, position, menu
		// -- n'est exercable que par une main, et les trois symptomes de Rodolf
		// vivent tous APRES le pick, pas dedans.
		//
		// DEUX FENTES (NK_DROP_TOKEN et NK_DROP_TOKEN2), meme convention que
		// NK_AGENT_DRAG/DRAG2 : poser DEUX models dans la scene en UN lancement.
		// Une seule fente obligeait a enregistrer entre deux lancements pour
		// obtenir deux instances -- donc a modifier le projet pour pouvoir le
		// mesurer, ce qui change l'objet mesure. Le defaut n.3 de Rodolf (« il
		// n'y a que le premier qui se deplace ») ne s'exerce qu'a partir de DEUX.
		{
			static bool dtDone[2] = {false, false};
			static int32 dtFrame = 0;
			++dtFrame;
			for (int32 dc = 0; dc < 2; ++dc) {
			if (!dtDone[dc]) {
				const char *dt = std::getenv(dc == 0 ? "NK_DROP_TOKEN" : "NK_DROP_TOKEN2");
				if (!dt) {
					dtDone[dc] = true;
				} else {
					float32 dv[4] = {0.f, 0.f, 0.f, 8.f};
					int32 dk = 0;
					for (const char *dp = dt; dk < 4 && *dp;) {
						dv[dk++] = (float32)atof(dp);
						while (*dp && *dp != ',')
							++dp;
						if (*dp == ',')
							++dp;
					}
					if (dtFrame >= (int32)dv[3]) {
						dtDone[dc] = true;
						const int32 ci = (int32)dv[0];
						if (ci >= 0 && ci < st.browserCount) {
							st.dropIdx = ci;
							st.dropKind = st.browserKind[ci];
							st.dropSrcNode = st.browserSrcNode[ci];
							st.dropMat = st.browserMat[ci];
							snprintf(st.dropName, sizeof(st.dropName), "%s",
									 st.browserNames[ci]);
							st.dropMenuTarget = -1;
							demo::Demo3DHostPickRequest(dv[1], dv[2]);
							nkentseu::NkLog::Instance().Info(
								"[nk3d] MESURE jeton : carte {0} « {1} » nature={2} "
								"srcNode={3} mat={4} · lacher demande a ({5}, {6}) "
								"fenetre\n",
								ci, st.dropName, (int32)st.dropKind, st.dropSrcNode,
								st.dropMat, dv[1], dv[2]);
						} else {
							// Carte hors bornes = DEMANDE D'INVENTAIRE. Sans lui, il
							// faut une execution par indice pour savoir quelle carte
							// porte quel numero, et le numero change avec le tri.
							nkentseu::NkLog::Instance().Info(
								"[nk3d] MESURE jeton : {0} cartes\n", st.browserCount);
							for (int32 bi = 0; bi < st.browserCount; ++bi)
								nkentseu::NkLog::Instance().Info(
									"[nk3d]   carte {0} « {1} » nature={2} srcNode={3} "
									"mat={4} parent={5}\n",
									bi, st.browserNames[bi], (int32)st.browserKind[bi],
									st.browserSrcNode[bi], st.browserMat[bi],
									st.browserParent[bi]);
						}
					}
				}
			}
			}
		}
		if (st.dropIdx >= 0 && st.dropMenuTarget < 0) {
			int32 dropNode = -3;
			float32 dropW[3] = {0.f, 0.f, 0.f};
			if (demo::Demo3DHostPickTake(&dropNode, dropW)) {
				const bool vide = (dropNode == -1);
				// MESURE : ce que la reponse du pick vaut AVANT tout traitement.
				nkentseu::NkLog::Instance().Info(
					"[nk3d] MESURE lacher : nature={0} noeud={1} monde=({2}, {3}, {4}) "
					"srcNode={5} mat={6}\n",
					(int32)st.dropKind, dropNode, dropW[0], dropW[1], dropW[2],
					st.dropSrcNode, st.dropMat);
				// -2 = hors du viseur. La zone de lacher EST le viseur, donc ce
				// cas ne devrait pas arriver : il est journalise plutot que
				// traite, parce que c'est un bogue et pas un cas d'usage.
				if (dropNode == -2) {
					nkentseu::NkLog::Instance().Warn(
						"[nk3d] lacher resolu HORS du viseur : la zone de lacher du "
						"shell et le viseur de l'hote ont diverge\n");
					st.dropIdx = -1;
				} else if (st.dropKind == 2) { // MATERIAU
					if (vide) {
						st.dropIdx = -1; // lache dans le vide : rien, et c'est voulu
					} else {
						if (st.dropMat > 0) {
							const int32 avant = demo::Demo3DHostProjMatOf(dropNode);
							// LES NUMEROS NE SUFFISENT PAS : "demande=5 apres=5" dit que
							// l'assignation ecrit ce qu'on lui DEMANDE, pas que 5 soit
							// l'emplacement de la carte SAISIE. Deux causes, un symptome :
							// la carte designe un autre emplacement, ou elle designe le bon
							// et sa VIGNETTE est perimee. Le NOM et l'albedo les separent.
							char nomSlot[64] = {0};
							float32 alb3[3] = {0.f, 0.f, 0.f};
							demo::Demo3DHostProjMatInfo(st.dropMat - 1, nomSlot,
								(uint32)sizeof(nomSlot), alb3, nullptr, nullptr);
							// UN MODEL NE SE PEINT PAS : SA MATIERE EST CHEZ SES ENFANTS.
							//
							// Le rendu saute les conteneurs -- NkDemo3D.cpp : `if (nkvpIsModel[un])
							// continue; // conteneur : sa geometrie vit dans ses maillages` -- et le
							// pick fait de meme. Assigner au conteneur REUSSIT donc sans rien
							// changer a l'ecran. Mesure : noeud=107, demande=4, apres=4, et aucun
							// effet visible. Rihen : « aucun changement de plus pour ces model, je
							// ne peux meme pas modifier leur material visible depuis la scene ».
							//
							// C'est sa specification du 17/08 appliquee : en mode objet, un clic
							// prend le model AVEC tous ses sous-mesh. Le materiau lache sur un model
							// va donc a ce qui SE VOIT -- ses maillages -- et le conteneur garde
							// l'entree dans SA liste : c'est lui qu'on selectionne, et c'est lui qui
							// portera le choix quand le mode edition existera.
							if (demo::Demo3DHostNodeIsModel(dropNode)) {
								int32 posesSurEnfants = 0;
								for (int32 ce = 0; ce < 160; ++ce) {
									if (demo::Demo3DHostNodeParent(ce) != dropNode)
										continue;
									demo::Demo3DHostProjMatAssign(ce, st.dropMat - 1);
									++posesSurEnfants;
									// MESURE : ce que l'ENFANT porte APRES la pose. Le conteneur
									// ne se voit pas -- mesurer SON materiau ne dit rien de ce qui
									// est a l'ecran. Seul l'enfant repond de la couleur rendue.
									nkentseu::NkLog::Instance().Info(
										"[nk3d]   MESURE enfant peint : noeud={0} mesh={1} "
										"materiau={2}\n",
										ce, demo::Demo3DHostNodeIsMesh(ce) ? 1 : 0,
										demo::Demo3DHostProjMatOf(ce));
								}
								// La liste du conteneur suit. S'il ne portait AUCUN materiau,
								// HostNodeMatAdd le promeut aussi en actif -- sans effet a
								// l'ecran, le conteneur n'etant pas rendu, mais c'est ce que le
								// panneau lira quand on selectionnera le model.
								demo::Demo3DHostNodeMatAdd(dropNode, st.dropMat - 1);
								// UN MODEL SANS MAILLAGE NE DOIT PAS SE TAIRE : sinon le geste
								// parait avoir marche alors que rien n'a ete peint.
								if (posesSurEnfants == 0)
									snprintf(st.hierNote, sizeof(st.hierNote),
									         "%s n'a aucun maillage a peindre", st.dropName);
							} else {
								demo::Demo3DHostProjMatAssign(dropNode, st.dropMat - 1);
							}
							// MESURE : l'assignation a-t-elle PRIS ? « aucun effet »
							// peut vouloir dire « rien ne s'est ecrit » ou « le
							// materiau pose ressemble a celui d'avant ».
							nkentseu::NkLog::Instance().Info(
								"[nk3d] MESURE materiau : noeud={0} avant={1} "
								"demande={2} apres={3} carte='{4}' emplacement='{5}' "
								"albedo=({6}, {7}, {8})\n",
								dropNode, avant, st.dropMat - 1,
								demo::Demo3DHostProjMatOf(dropNode), st.dropName, nomSlot,
								alb3[0], alb3[1], alb3[2]);
						} else
							snprintf(st.hierNote, sizeof(st.hierNote),
									 "« %s » n'a pas encore d'emplacement de materiau",
									 st.dropName);
						st.dropIdx = -1;
					}
				} else if (st.dropKind == 6) { // MODEL / MESH
					if (vide) {
						// AJOUTE A CETTE POSITION -- pas a l'origine. C'est tout
						// l'objet du point du monde rendu par le pick : sans lui,
						// dix lachers a dix endroits empilaient dix modeles au
						// meme point, et le geste n'avait plus de sens.
						const int32 nn = NkDropSpawnModel(st);
						if (nn >= 0) {
							const float32 rot[3] = {0.f, 0.f, 0.f};
							const float32 scl[3] = {1.f, 1.f, 1.f};
							demo::Demo3DHostSetModelTransform(nn, dropW, rot, scl);
							demo::Demo3DHostSelectEmptyNode(nn);
							// MESURE : ce que le noeud vaut APRES la pose. Si la
							// position relue differe de celle demandee, ce n'est
							// pas le pick qui ment, c'est la pose.
							float32 gp[3] = {0.f, 0.f, 0.f}, gr[3] = {0.f, 0.f, 0.f},
									gs[3] = {0.f, 0.f, 0.f};
							const bool got =
								demo::Demo3DHostEmptyTransform(nn, gp, gr, gs);
							nkentseu::NkLog::Instance().Info(
								"[nk3d] MESURE pose : noeud={0} demande=({1}, {2}, {3}) "
								"relu={4} ({5}, {6}, {7}) model={8}\n",
								nn, dropW[0], dropW[1], dropW[2], got ? 1 : 0, gp[0],
								gp[1], gp[2],
								demo::Demo3DHostNodeIsModel(nn) ? 1 : 0);
							// COMBIEN DE MATERIAUX, ET SUR QUI ? Rihen : "quand je porte un
							// model du navigateur vers la scene, je ne peux pas modifier son
							// materiau". Le panneau lit NodeMatCount(noeud ACTIF) -- et le
							// noeud actif est le CONTENANT. Si sa matiere vit chez ses
							// enfants, il compte zero materiau et le panneau n'a rien a
							// montrer. On mesure les deux niveaux avant de conclure : un
							// contenant a zero et des enfants a un, ce n'est pas le meme
							// defaut qu'un contenant a zero et des enfants a zero.
							{
								int32 matEnf = 0, nbEnf = 0;
								for (int32 ce = 0; ce < 160; ++ce) {
									if (demo::Demo3DHostNodeParent(ce) != nn)
										continue;
									++nbEnf;
									matEnf += demo::Demo3DHostNodeMatCount(ce);
								}
								nkentseu::NkLog::Instance().Info(
									"[nk3d] MESURE materiaux du model : noeud={0} sesMateriaux={1} "
									"enfants={2} materiauxDesEnfants={3} actif={4}\n",
									nn, demo::Demo3DHostNodeMatCount(nn), nbEnf, matEnf,
									demo::Demo3DHostProjMatOf(nn));
							}
							// MESURE : ET SES ENFANTS ? Un model est un CONTENANT --
							// le pick lui-meme le dit (« un model se prend par sa
							// matiere »). Poser la transformation du contenant ne
							// prouve rien si sa matiere reste ou elle etait.
							for (int32 ci2 = 0; ci2 < 160; ++ci2) {
								if (demo::Demo3DHostNodeParent(ci2) != nn)
									continue;
								float32 cp[3] = {0.f, 0.f, 0.f}, cr[3] = {0.f, 0.f, 0.f},
										cs[3] = {0.f, 0.f, 0.f};
								const bool cg =
									demo::Demo3DHostEmptyTransform(ci2, cp, cr, cs);
								nkentseu::NkLog::Instance().Info(
									"[nk3d]   MESURE enfant : noeud={0} mesh={1} "
									"relu={2} ({3}, {4}, {5})\n",
									ci2, demo::Demo3DHostNodeIsMesh(ci2) ? 1 : 0,
									cg ? 1 : 0, cp[0], cp[1], cp[2]);
							}
							// Et la SOURCE, pour comparer : c'est d'elle qu'on a
							// copie, donc c'est elle le point de reference.
							{
								const int32 sn = st.dropSrcNode - 1;
								float32 sp[3] = {0.f, 0.f, 0.f}, sr[3] = {0.f, 0.f, 0.f},
										ss[3] = {0.f, 0.f, 0.f};
								const bool sg =
									demo::Demo3DHostEmptyTransform(sn, sp, sr, ss);
								nkentseu::NkLog::Instance().Info(
									"[nk3d]   MESURE source : noeud={0} relu={1} ({2}, "
									"{3}, {4})\n",
									sn, sg ? 1 : 0, sp[0], sp[1], sp[2]);
								for (int32 ci3 = 0; ci3 < 160; ++ci3) {
									if (demo::Demo3DHostNodeParent(ci3) != sn)
										continue;
									float32 dp[3] = {0.f, 0.f, 0.f},
											dr[3] = {0.f, 0.f, 0.f},
											ds[3] = {0.f, 0.f, 0.f};
									const bool dg =
										demo::Demo3DHostEmptyTransform(ci3, dp, dr, ds);
									nkentseu::NkLog::Instance().Info(
										"[nk3d]   MESURE enfant source : noeud={0} "
										"relu={1} ({2}, {3}, {4})\n",
										ci3, dg ? 1 : 0, dp[0], dp[1], dp[2]);
								}
							}
						} else {
							nkentseu::NkLog::Instance().Warn(
								"[nk3d] MESURE pose : AUCUN noeud cree (srcNode={0})\n",
								st.dropSrcNode);
						}
						st.dropIdx = -1;
					} else {
						// SUR UN OBJET : le choix revient a l'utilisateur, par un
						// menu. On MEMORISE la cible et le point ; le jeton reste
						// en vol jusqu'a ce que le menu tranche -- ou soit
						// abandonne, ce qui est la troisieme issue et pas un
						// « enfant par defaut ».
						st.dropMenuTarget = dropNode;
						st.dropWorld[0] = dropW[0];
						st.dropWorld[1] = dropW[1];
						st.dropWorld[2] = dropW[2];
						st.dropMenuX = ui.input.mousePos.x;
						st.dropMenuY = ui.input.mousePos.y;
					}
				} else { // TEXTURE, SCENE, DOSSIER, GRAPHE, DATASET...
					if (!vide)
						NkDropRefuse(st, st.dropKind);
					st.dropIdx = -1;
				}
			}
		}

		// ---- LA CARTE SUIVANTE DU GESTE MULTIPLE ----
		//
		// Le jeton vient de se liberer et la file n'est pas vide : on y remet
		// la carte suivante, qui reprend le chemin au debut -- pick, nature,
		// refus ou application. Une carte par frame, jamais deux : le pick a
		// besoin d'une frame pour repondre, et vouloir tout appliquer d'un
		// coup demanderait un second chemin sans pick.
		//
		// LE POINT DE LACHER EST CELUI QUI A ETE FIGE, pas la position
		// courante de la souris. Entre la premiere carte et la dixieme, le
		// curseur a bouge et la camera peut avoir tourne ; les dix objets
		// doivent atterrir la ou l'utilisateur a lache.
		//
		// Un menu ouvert SUSPEND la file : tant que l'utilisateur n'a pas
		// repondu "enfant ou independant", la carte suivante attend. Sinon
		// dix menus se superposeraient et il repondrait au dernier en croyant
		// repondre au premier.
		if (st.dropIdx < 0 && st.dropMenuTarget < 0 && st.dropQueueCount > 0) {
					const int32 carte = st.dropQueue[0];
					for (int32 k = 1; k < st.dropQueueCount; ++k)
						st.dropQueue[k - 1] = st.dropQueue[k];
					--st.dropQueueCount;
					if (carte >= 0 && carte < st.browserCount) {
						st.dropIdx = carte;
						st.dropKind = st.browserKind[carte];
						st.dropSrcNode = st.browserSrcNode[carte];
						st.dropMat = st.browserMat[carte];
						snprintf(st.dropName, sizeof(st.dropName), "%s", st.browserNames[carte]);
						demo::Demo3DHostPickRequest(st.dropQueueX, st.dropQueueY);
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
							NkImage rs = shot.Resize((int32)ew, (int32)eh,
													 NkResizeFilter::NK_BICUBIC);
							if (rs.IsValid())
								rs.Save(capPath);
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
