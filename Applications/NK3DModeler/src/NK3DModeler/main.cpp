// -----------------------------------------------------------------------------
// @File    main.cpp
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// =============================================================================
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
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
#include "NK3DModeler/Shell/NkModelerToast.h" // le resultat d'une action, DIT A L'ECRAN
#include "NKContainers/String/Encoding/NkBase64.h" // les messages du moteur, lisibles dans l'app
#include "NK3DModeler/Shell/NkModelerHierarchy.h" // hierarchie + menus de scene
#include "NK3DModeler/Shell/NkModelerViewport.h"  // vue 3D et ses surcouches
#include "NK3DModeler/Shell/NkModelerProperties.h" // panneau de proprietes
#include "NK3DModeler/Shell/NkModelerBrowser.h" // navigateur de contenu
#include "NK3DModeler/Shell/NkModelerImport.h"  // import de fichiers 3D (bouton Importer)
// La dette du 18/08 : le peintre de NK3DModeler vu comme un NkComponentPaint,
// et le premier composant du kit rendu par lui (NK_KIT_TREE=1).
#include "NK3DModeler/Shell/NkModelerComponentPaint.h"
#include "NKEditorKit/Components/NkTreeViewModel.h"
#include "NKEditorKit/Components/NkContentBrowserModel.h"
#include "NK3DModeler/Genia/NkGeniaImport.h"     // GENIA : image -> generateur externe -> import (bouton Generer)
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
			// ⚠️ LE REPLI BitBlt EST INTERDIT SOUS SONDE, et ce n'est pas une
			//    precaution de style. `PrintWindow(PW_RENDERFULLCONTENT)` demande a
			//    LA FENETRE de se redessiner : il ne peut rendre que SON contenu,
			//    meme recouverte. `BitBlt(..., CAPTUREBLT)` lit l'ECRAN a
			//    l'emplacement de la fenetre : si quoi que ce soit passe par dessus,
			//    l'image contient l'ecran de quelqu'un d'autre. Une sonde n'a pas le
			//    droit de produire cette image-la, meme par accident.
			//    HORS sonde le repli reste : pour un utilisateur, une capture
			//    degradee vaut mieux qu'une capture absente.
			if (!ok && !std::getenv("NK_TOAST_PROBE"))
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
			// ⚠️ LE REPLI BitBlt EST INTERDIT SOUS SONDE, et ce n'est pas une
			//    precaution de style. `PrintWindow(PW_RENDERFULLCONTENT)` demande a
			//    LA FENETRE de se redessiner : il ne peut rendre que SON contenu,
			//    meme recouverte. `BitBlt(..., CAPTUREBLT)` lit l'ECRAN a
			//    l'emplacement de la fenetre : si quoi que ce soit passe par dessus,
			//    l'image contient l'ecran de quelqu'un d'autre. Une sonde n'a pas le
			//    droit de produire cette image-la, meme par accident.
			//    HORS sonde le repli reste : pour un utilisateur, une capture
			//    degradee vaut mieux qu'une capture absente.
			if (!ok && !std::getenv("NK_TOAST_PROBE"))
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
		// G/R/S sont les MODALES depuis le 2026-08-28, dans les DEUX modes -- plus
		// la selection d'outil. La table le disait deja pour l'objet ("Deplacer") ;
		// elle ne le disait pas du tout pour l'edition, ou les memes touches
		// faisaient la meme chose. Un contexte manquant, c'est un menu qui n'affiche
		// pas un raccourci qui existe.
		t.Bind("objet.deplacer", "Deplacer", NkKey::NK_G, 0, NK_SCTX_OBJECT);
		t.Bind("objet.tourner", "Tourner", NkKey::NK_R, 0, NK_SCTX_OBJECT);
		t.Bind("objet.echelle", "Redimensionner", NkKey::NK_S, 0, NK_SCTX_OBJECT);
		t.Bind("edit.deplacer", "Deplacer", NkKey::NK_G, 0, NK_SCTX_EDIT);
		t.Bind("edit.tourner", "Tourner", NkKey::NK_R, 0, NK_SCTX_EDIT);
		t.Bind("edit.echelle", "Redimensionner", NkKey::NK_S, 0, NK_SCTX_EDIT);
		// Le SELECTEUR D'OUTIL, la ou Blender le met : une touche a base d'espace,
		// jamais une lettre nue.
		t.Bind("app.selecteur_outil", "Selecteur d'outil", NkKey::NK_SPACE, 0, NK_SCTX_GLOBAL);
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
		// ── LIAISONS QUI EXISTAIENT DANS LE VISEUR SANS ETRE DECLAREES ICI ──
		// Elles fonctionnaient toutes ; la table ne les connaissait pas, donc
		// AUCUN menu ne pouvait afficher leur raccourci. Verifiees une par une
		// dans NkDemo3D.cpp avant d'etre ecrites : une table qui ment est pire
		// qu'une table vide, puisqu'on la croit.
		t.Bind("edit.loop_cut", "Loop cut", NkKey::NK_R, NK_SC_CTRL, NK_SCTX_EDIT);
		t.Bind("edit.supprimer", "Supprimer", NkKey::NK_X, 0, NK_SCTX_EDIT);
		t.Bind("edit.dissoudre", "Dissoudre", NkKey::NK_X, NK_SC_CTRL, NK_SCTX_EDIT);
		t.Bind("edit.creer_face", "Creer une face", NkKey::NK_F, 0, NK_SCTX_EDIT);
		t.Bind("edit.spheriser", "Spheriser (to sphere)", NkKey::NK_S,
			   (uint8)(NK_SC_SHIFT | NK_SC_ALT), NK_SCTX_EDIT);
		// ECART DOCUMENTE, deja motive dans NkDemo3D.cpp : Blender met Alt+S, mais
		// Alt+S efface deja l'echelle du gizmo chez nous. On ne « corrige » donc
		// PAS cette divergence -- elle a sa raison ecrite, ce qui est la regle.
		t.Bind("edit.gonfler", "Gonfler / retrecir (shrink-fatten)", NkKey::NK_S,
			   (uint8)(NK_SC_CTRL | NK_SC_ALT), NK_SCTX_EDIT);
		// ⚠ SPIN, SEPARER LES ARETES et BISECT n'apparaissent PAS ici, et c'est
		// VOULU : depuis le 2026-08-28 ils n'ont plus de raccourci, exactement
		// comme chez Blender (mesh.spin, mesh.edge_split et mesh.bisect n'en ont
		// aucun dans le keymap par defaut -- verifie a la source). La table dit
		// donc la verite : pas d'entree = pas de touche, et le menu n'affiche
		// aucun raccourci a cote d'eux.

		// ── DEUX FAMILLES DE TOUCHES QUI EXISTENT ET QUE LA TABLE IGNORAIT ──
		// Trouvees en verifiant une contradiction, pas en cherchant a completer :
		// le panneau Proprietes annonce « K -- couteau » alors que la table declare
		// `Bisect` SANS touche. L'une des deux ment. Mesure, en ouvrant le fichier :
		// `NkDemo3D.cpp:5741` traite bien `NkKey::NK_K` et arme le couteau. C'est
		// donc la TABLE qui etait incomplete, et le panneau qui disait vrai.
		//
		// ⚠ POURQUOI ELLE L'ETAIT, ET CE QUE CA APPREND. Le commentaire qui justifie
		// l'absence s'appuie sur Blender, ou `mesh.bisect` n'a pas de touche par
		// defaut -- verification exacte, mais portant sur LE MAUVAIS OBJET. Notre
		// viseur n'est pas Blender : c'est lui qu'il fallait ouvrir. Un raccourci qui
		// fonctionne et qu'aucun menu n'annonce est precisement la maladie que ce
		// fichier soigne. Spin et « separer les aretes », eux, n'ont vraiment aucune
		// touche (verifie de la meme facon : ils ne s'atteignent que par le pilote
		// d'agent `NK_VP_ACTION`) -- ils restent donc sans entree, et le menu
		// continue de n'afficher aucun raccourci a cote d'eux.
		t.Bind("edit.bisect", "Couper (bisect)", NkKey::NK_K, 0, NK_SCTX_EDIT);
		// LES SOUS-MODES. Ils sont traites dans `NkDemo3D.cpp:5356-5377`, garde par
		// l'etat REEL du viseur : 1/2/3 posent le mode seul, Maj+1/2/3 combinent.
		// Sans ces trois entrees, la pastille de sous-mode ne pouvait porter aucune
		// infobulle honnete -- et Rodolf avait raison de ne pas savoir si ces
		// raccourcis existaient : rien dans l'application ne le disait.
		t.Bind("edit.sous_mode_sommet", "Sous-mode Sommets", NkKey::NK_NUM1, 0, NK_SCTX_EDIT);
		t.Bind("edit.sous_mode_arete", "Sous-mode Aretes", NkKey::NK_NUM2, 0, NK_SCTX_EDIT);
		t.Bind("edit.sous_mode_face", "Sous-mode Faces", NkKey::NK_NUM3, 0, NK_SCTX_EDIT);

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

	// ── LE TREE_VIEW DU KIT DANS LE PANNEAU DE GAUCHE (NK_KIT_TREE=1) ───────
	// Premiere consommation reelle de l'adaptateur `NkModelerComponentPaint` —
	// la dette du 18/08. Le modele est PERSISTANT (l'ouverture, la selection et
	// le defilement sont ecrits dedans par le composant, cles par identite
	// nk_uint64) ; ses NOEUDS sont rebatis a chaque image depuis la hierarchie
	// vivante, en ordre PREFIXE (la seule precondition du composant — l'ordre
	// des indices hote ne la garantit pas, un empty parent peut avoir un indice
	// superieur a ses enfants, d'ou le parcours en profondeur explicite).
	void PaintKitTree(NkModelerPainter &p, const NkRect &r, NkModelerState &st,
					  const nkgui::NkGuiInput &in) {
		static NkTreeViewModel m; // etat durable : toggled / active / chosen / scroll
		m.nodes.Clear();
		// ⚠ LE PLAFOND SE LIT SUR LA FONCTION, PAS SUR SON COMMENTAIRE. L'en-tete
		// de l'hote annonce « 96 (plafond, empties compris) » ; la constante vaut
		// 160 (kNkvpMaxNodes, NkDemo3D.cpp:150). Des tableaux dimensionnes sur le
		// commentaire ont ECRASE LA PILE — plantage a une adresse folle, quelques
		// images plus tard, sans lien visible avec la cause. Un nombre dans un
		// commentaire est une mesure non datee : ici il a menti, et le crash
		// n'accusait pas le menteur. Dimensionnement DYNAMIQUE, borne verifiee.
		const int32 total = demo::Demo3DHostNodeCount();
		static NkVector<int32> pos, pile;
		pos.Resize((uint32)total);
		pile.Resize((uint32)total);
		for (int32 i = 0; i < total; ++i)
			pos[i] = -1;
		int32 sp = 0;
		for (int32 n = total - 1; n >= 0; --n) {
			if (NkHierNodeSkip(n) || demo::Demo3DHostNodeDeleted(n))
				continue;
			if (demo::Demo3DHostNodeParent(n) < 0)
				pile[sp++] = n;
		}
		char nom[48];
		while (sp > 0) {
			const int32 n = pile[--sp];
			NkTreeNode t;
			t.id = (nk_uint64)(n + 1); // 0 est reserve par le contrat du composant
			const int32 par = demo::Demo3DHostNodeParent(n);
			t.parent = (par >= 0 && par < total) ? pos[par] : -1;
			NkHierNodeName(st, n, nom, sizeof(nom));
			t.label = NkString(nom);
			t.icon = NkIconHandle(n >= 90 ? NkIcon::Globe : NkIcon::Mesh);
			t.kindRole = (uint16)NkRole::TextMuted;
			t.userTag = (uint32)n;
			pos[n] = (int32)m.nodes.Size();
			m.nodes.PushBack(t);
			for (int32 c = total - 1; c >= 0; --c) {
				if (NkHierNodeSkip(c) || demo::Demo3DHostNodeDeleted(c))
					continue;
				if (demo::Demo3DHostNodeParent(c) == n && sp < total)
					pile[sp++] = c;
			}
		}

		NkTreeViewStyle s;
		// L'INSTANCE : la ou un reglage differe du defaut de la declaration. Les
		// filets d'indentation sont ETEINTS par defaut (`indent_guides` = 0 dans
		// la declaration) — c'est un reglage, pas une constante, et c'est ici que
		// l'application le pose. Sans instance, pas de guide, et la preuve n.2 de
		// NK3D-120 n'aurait rien a montrer.
		static NkComponentInstance inst;
		static bool instInit = false;
		if (!instInit) {
			instInit = true;
			inst.Bind(NkTreeViewDecl());
			inst.SetParam("indent_guides", 1.f);
		}
		s.values = &inst;
		s.panelBg = (uint16)NkRole::PanelBg;
		s.headerBg = (uint16)NkRole::PanelHeader;
		s.border = (uint16)NkRole::Border;
		s.text = (uint16)NkRole::Text;
		s.textMuted = (uint16)NkRole::TextMuted;
		s.rowHover = (uint16)NkRole::InputBg;
		s.activeMark = (uint16)NkRole::AccentUi;
		s.activeText = (uint16)NkRole::TextOnAccent;
		s.chosenMark = (uint16)NkRole::PanelHeader;
		// ⚠ GUIDE DELIBEREMENT DISTINCT DE Border POUR LA CAPTURE DE PREUVE : un
		// guide ambre a cote d'une bordure grise tranche a l'oeil — c'est la
		// verification n.2 de NK3D-120 (l'adaptateur route le ROLE, il ne code
		// rien en dur). Le role definitif est un reglage produit, pas le mien.
		s.guide = (uint16)NkRole::AccentSel;
		s.dropMark = (uint16)NkRole::AccentUi;
		s.iconTint = (uint16)NkRole::TextMuted;
		s.dimTint = (uint16)NkRole::TextMuted;
		s.icons.chevronClosed = NkIconHandle(NkIcon::ChevronRight);
		s.icons.chevronOpen = NkIconHandle(NkIcon::ChevronDown);
		s.icons.eyeOpen = NkIconHandle(NkIcon::Eye);
		s.icons.eyeClosed = NkIconHandle(NkIcon::EyeClosed);
		s.icons.lockOpen = NkIconHandle(NkIcon::Unlock);
		s.icons.lockClosed = NkIconHandle(NkIcon::Lock);

		NkComponentInput ci;
		ci.surfaceScale = gUiScale; // les composants multiplient leurs metriques par elle
		ci.mouseX = in.mousePos.x;
		ci.mouseY = in.mousePos.y;
		ci.wheel = in.wheel;
		ci.mouseDown = in.mouseDown[0];
		ci.mousePressed = in.mouseClicked[0];
		ci.mouseReleased = in.mouseReleased[0];
		ci.doubleClick = in.mouseDoubleClicked[0];
		ci.rightPressed = in.mouseClicked[1];
		ci.ctrl = in.ctrlDown;
		ci.shift = in.shiftDown;

		NkModelerComponentPaint paint(p);
		const NkPaintRect rect{r.x, r.y, r.w, r.h};
		NkTreeViewHooks hooks; // aucun crochet pour la preuve : le composant gere
		(void)NkDrawTreeView(paint, ci, rect, m, s, hooks);
		if (std::getenv("NK_KIT_TRACE") != nullptr) {
			static uint32 sTick = 0;
			if (++sTick % 60u == 1u) {
				std::printf("[kit-tree] modele : %u noeud(s)\n", (uint32)m.nodes.Size());
				for (uint32 i = 0; i < (uint32)m.nodes.Size(); ++i)
					std::printf("[kit-tree]   %2u id=%u parent=%d %s\n", i,
								(uint32)m.nodes[i].id, (int)m.nodes[i].parent, m.nodes[i].label.CStr());
				std::fflush(stdout);
			}
		}
	}

	// ── LE CONTENT_BROWSER DU KIT (NK_KIT_BROWSER=1) ────────────────────────
	// Second composant sur le MEME adaptateur. Il exerce ce que l'arbre ne peut
	// pas : le pied de carte appelle `Text(..., NkTextAlign::Center)` deux fois
	// — c'est la preuve n.1 de NK3D-120, l'alignement etant le seul vrai
	// travail de l'adaptateur. Donnees : les cartes reelles du projet ouvert.
	void PaintKitBrowser(NkModelerPainter &p, const NkRect &r, NkModelerState &st,
						 const nkgui::NkGuiInput &in) {
		static NkContentBrowserModel m;
		// VIGNETTE ADAPTEE AU PANNEAU : le defaut de la declaration vise un
		// navigateur plein ecran ; dans le bandeau bas de NK3DModeler, une carte au
		// defaut depasse le clip et son PIED (les deux libelles) disparait — on
		// croirait l'alignement casse alors que c'est la carte qui deborde.
		m.thumbSize = 56.f;
		m.entries.Clear();
		// ⚠️ LE `&& i < 32` A DISPARU, ET C'EST LE POINT (2026-09-14).
		//    Ce site lisait encore les dix tableaux paralleles que le refactor
		//    du 05/09 a remplaces par un vecteur (NkModelerInput.h:144-156). La
		//    fusion dans `transit` a garde l'appelant ancien et la structure
		//    neuve : refus de compilation. Le migrer en RECOPIANT la borne 32
		//    aurait remis EN SILENCE le plafond que le refactor existait pour
		//    supprimer -- le projet de Rodolf en avait exactement 32.
		for (int32 i = 0; i < st.BrowserCount(); ++i) {
			const NkBrowserCard &c = st.Card(i);
			if (c.kind == 255)
				continue; // carte supprimee
			NkAssetEntry e;
			e.name = NkString(c.name);
			e.isFolder = (c.kind == 1);
			// Legende du CONSOMMATEUR (NkModelerUI.h) : 0 graphe · 1 dossier ·
			// 2 materiau · 3 texture · 4 dataset IA · 5 scene · 6 model.
			static const char *const kKind[7] = {"Graphe", "Dossier", "Materiau",
												 "Texture", "Dataset", "Scene", "Model"};
			e.kindLabel = (c.kind < 7) ? kKind[c.kind] : "";
			static const NkRole kKindRole[7] = {NkRole::AccentUi, NkRole::TextMuted,
												NkRole::TypeMat, NkRole::TypeTex,
												NkRole::AccentUi, NkRole::TypeAnim,
												NkRole::TypeMesh};
			e.kindRole = (uint16)((c.kind < 7) ? kKindRole[c.kind] : NkRole::TextMuted);
			e.userTag = (uint32)i;
			m.entries.PushBack(e);
		}

		NkContentBrowserStyle s;
		s.panelBg = (uint16)NkRole::PanelBg;
		s.headerBg = (uint16)NkRole::PanelHeader;
		s.border = (uint16)NkRole::Border;
		s.text = (uint16)NkRole::Text;
		s.textMuted = (uint16)NkRole::TextMuted;
		s.cardBg = (uint16)NkRole::InputBg;
		s.cardFooterBg = (uint16)NkRole::PanelHeader;
		s.activeMark = (uint16)NkRole::AccentUi;
		s.chosenMark = (uint16)NkRole::PanelHeader;
		s.folderTint = (uint16)NkRole::AccentSel;

		NkComponentInput ci;
		ci.surfaceScale = gUiScale;
		ci.mouseX = in.mousePos.x;
		ci.mouseY = in.mousePos.y;
		ci.wheel = in.wheel;
		ci.mouseDown = in.mouseDown[0];
		ci.mousePressed = in.mouseClicked[0];
		ci.mouseReleased = in.mouseReleased[0];
		ci.doubleClick = in.mouseDoubleClicked[0];
		ci.rightPressed = in.mouseClicked[1];
		ci.ctrl = in.ctrlDown;
		ci.shift = in.shiftDown;

		NkModelerComponentPaint paint(p);
		const NkPaintRect rect{r.x, r.y, r.w, r.h};
		NkContentBrowserHooks hooks;
		(void)NkDrawContentBrowser(paint, ci, rect, m, s, hooks);
	}

} // namespace

int nkmain(const NkEntryState &entry) {
	// ── SONDE DU FORMAT DE GEOMETRIE, AVANT TOUT LE RESTE ───────────────────
	// `NK3DModeler.exe --sonde-geo [dossier]` eprouve NkModelerGeom.h et SORT :
	// aucune fenetre, aucun device, aucun GPU pris. C'est ce qui permet de la
	// lancer pendant qu'une autre application tient la carte -- et de la lancer
	// sans un seul clic. Le verdict part dans `sonde_geo.txt` du dossier donne
	// (defaut : le dossier courant), parce qu'une application fenetree n'a pas
	// de console ou ecrire.
	for (usize a = 0; a < entry.args.Size(); ++a) {
		if (!(entry.args[a] == NkString("--sonde-geo")))
			continue;
		const NkString dir = (a + 1u < entry.args.Size()) ? entry.args[a + 1u] : NkString(".");
		return (int)nk3d::NkGeoSonde(dir);
	}

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
	// NK_THEME="<nom>" : choisir le theme au lancement. Sert aux captures de
	// controle -- un grisage ou un contraste ne se verifie QUE a l'oeil, et le
	// verifier dans un seul theme ne dit rien de l'autre.
	if (const char *thEnv = std::getenv("NK_THEME"))
		if (*thEnv) {
			// On DIT si le nom a ete accepte : un theme demande et silencieusement
			// ignore produirait deux captures identiques qu'on prendrait pour la
			// preuve de deux themes.
			const bool ok = themes.SetCurrent(thEnv);
			std::printf("[theme] NK_THEME=%s -> %s (courant : %s)\n", thEnv,
						ok ? "accepte" : "INCONNU, ignore", themes.Current().Name().CStr());
		}

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

	// ── QUEL DORSAL GRAPHIQUE ? ──
	//
	// Directive de Rodolf du 18/08 : toute application doit laisser choisir son
	// dorsal, avec le MEME vocabulaire partout. `NkDemoCommon.h` du modeleur
	// portait bien un `ParseBackend` avec ses cinq mots-cles -- et pas UN SEUL
	// appelant dans tout `Applications/NK3DModeler/src`. Declare, jamais honore.
	//
	// ⚠️ ON N'UTILISE PAS CE `ParseBackend`-LA. Le vocabulaire canonique vit dans
	// NKEditorKit (`NkEditorGfxApiFromName` / `...Name` / `...Choices` /
	// `...Supported`), justement pour qu'il n'y ait pas un dialecte par
	// application -- ce que la directive interdit nommement. Reutiliser la copie
	// locale aurait fait un vocabulaire de plus (`-bvk`, `sw`) la ou le kit dit
	// `vulkan` et `software`.
	//
	// Deux entrees, l'option l'emporte sur l'environnement :
	//   --backend=<nom>   ou   --backend <nom>
	//   NK_GFX_BACKEND=<nom>
	// (`NK_GFX_BACKEND` etait promue par un commentaire de `NkCGXDetect.h` et LUE
	//  PAR PERSONNE ; elle a desormais un lecteur.)
	//
	// ⚠️ UN MOT INCONNU EST REFUSE EN LE NOMMANT, et le programme SORT. Il ne
	// retombe pas en silence sur le defaut : c'est la regle 3 de la directive, et
	// c'est ce qui a coute une journee -- on croit tester Vulkan et on teste
	// OpenGL. `NkEditorGfxApiFromName` NE TOUCHE PAS sa sortie quand il refuse,
	// donc un refus ne peut pas laisser une valeur a moitie ecrite.
	NkEditorGfxApi gfxApi = NkEditorGfxApi::Auto;
	{
		NkString demande;
		const char *provenance = "defaut";
		if (const char *e = env::GetEnvVar("NK_GFX_BACKEND")) {
			if (*e) {
				demande = NkString(e);
				provenance = "NK_GFX_BACKEND";
			}
		}
		for (usize ia = 0; ia < entry.args.Size(); ++ia) {
			const NkString &arg = entry.args[ia];
			if (arg.StartsWith("--backend=")) {
				demande = NkString(arg.CStr() + 10);
				provenance = "--backend=";
			} else if (arg == NkString("--backend") && ia + 1u < entry.args.Size()) {
				demande = entry.args[ia + 1u];
				provenance = "--backend";
			}
		}
		if (!demande.Empty()) {
			if (!NkEditorGfxApiFromName(demande.CStr(), gfxApi)) {
				printf("[nk3d] dorsal graphique inconnu : %s (donne par %s)\n",
					   demande.CStr(), provenance);
				printf("[nk3d] valeurs acceptees : %s\n", NkEditorGfxApiChoices());
				printf("[nk3d] REFUS -- on ne retombe pas en silence sur le defaut.\n");
				return 2;
			}
			const char *raison = nullptr;
			if (!NkEditorGfxApiSupported(gfxApi, &raison)) {
				printf("[nk3d] dorsal %s indisponible : %s\n", NkEditorGfxApiName(gfxApi),
					   (raison && *raison) ? raison : "non porte sur cette plateforme");
				printf("[nk3d] valeurs acceptees : %s\n", NkEditorGfxApiChoices());
				printf("[nk3d] REFUS -- on ne remplace pas en silence.\n");
				return 2;
			}
		}
		// LE CHOIX SE JOURNALISE AVANT TOUTE CREATION DE CONTEXTE (regle 2).
		printf("[nk3d] dorsal graphique : demande %s (%s) -> retenu %s\n",
			   demande.Empty() ? "auto" : demande.CStr(), provenance,
			   NkEditorGfxApiName(gfxApi));
		nk3d::NkDorsalRetenu() = NkEditorGfxApiName(gfxApi);
	}


	// ── FENETRE ─────────────────────────────────────────────────────────────
	// SANS CADRE OS : la maquette porte ses propres boutons de fenetre dans la
	// barre de menus. Garder le cadre natif donnerait deux barres de titre.
	NkWindowConfig wc;
	// ⚠ UN SEUL MARQUEUR DE SONDE, DEUX DECLENCHEURS. Transit et ce chantier
	// ont ecrit le meme garde-fou le meme jour, sans se voir : `NK_TOAST_PROBE`
	// d'un cote, `NK_SONDE` de l'autre, pour la raison exacte -- une capture de
	// sonde prise pour une capture du produit. On n'en garde qu'un, qui repond
	// aux deux variables : deux mecanismes pour un role divergeraient au premier
	// changement de texte.
	// `sonde` sert AUSSI a la barre DESSINEE (PaintMenuBarI) -- c'est elle que
	// Rodolf voit, le titre OS ne se montrant qu'en barre des taches.
	const bool sonde =
		(std::getenv("NK_SONDE") != nullptr) || (std::getenv("NK_TOAST_PROBE") != nullptr);
	// LE DORSAL DANS LE TITRE (apport de transit) : la barre des taches et les
	// outils systeme le montrent, meme si la fenetre est sans cadre.
	wc.title = sonde ? NkString("*** SONDE DE MESURE - CETTE FENETRE N'EST PAS LE PRODUIT *** ") +
						   NkString(NkEditorGfxApiName(gfxApi))
					 : NkString("NK3DModeler ") + NkString(NkEditorGfxApiName(gfxApi));
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
	if (!renderer.Init(window, gfxApi)) {
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
	//   NK_AGENT_SCENE=<n> : a la trame n, QUITTE L'ACCUEIL SANS OUVRIR DE
	//     PROJET -- le viseur paraît alors avec sa scene par defaut (le portage
	//     de --demo=2), et le ciel passe en Rayleigh+Mie.
	//
	//     ⚠️ POURQUOI CE LEVIER EXISTE. Mesurer le chemin du viseur demande un
	//     viseur, donc un onglet ouvert. Les deux seules facons d'en avoir
	//     etaient d'ouvrir un projet RECENT (donc un projet de Rodolf, qu'il
	//     faudrait sauvegarder et restituer) ou de creer un projet neuf (qui
	//     ECRIT un dossier et un .nk3dm sur le disque). Aucune des deux n'est
	//     acceptable pour une mesure. Celui-ci n'ecrit RIEN : ni fichier, ni
	//     recents, ni sauvegarde. Une scene en memoire, et c'est tout.
	//
	//     ⚠️ ET IL NE CHANGE RIEN QUAND LA VARIABLE EST ABSENTE. Sans elle,
	//     `agentSceneFrame` reste a -1 et le bloc ne s'execute jamais : le
	//     comportement de l'application est celui d'avant, a l'octet pres.
	//
	//     Le ciel est mis en Rayleigh+Mie parce que c'est le modele dont le
	//     degrade est le plus franc (56 % d'amplitude mesuree au banc) : une
	//     inversion s'y lit sans ambiguite, la ou un ciel a faible contraste
	//     laisserait le doute.
	int32 agentShotFrame = -1, agentExitFrame = -1, agentOpenRecent = -1;
	int32 agentSceneFrame = -1;
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
		if (const char *v = std::getenv("NK_AGENT_SCENE"))
			agentSceneFrame = (int32)std::atoi(v);
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
		// Les messages a l'ecran vieillissent en SECONDES, pas en images.
		nk3d::NkToastTick(dt);

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
					printf("[nk3d-drag] browserCount=%d\n", st.BrowserCount());
					for (int32 b = 0; b < st.BrowserCount(); ++b)
						printf("[nk3d-drag] brow=%d kind=%d parent=%d name=\"%s\"\n", b,
							   st.Card(b).kind, st.Card(b).parent, st.Card(b).name);
					printf("[nk3d-drag] browAskIdx=%d browAskDest=%d folder=%d\n",
						   st.browAskIdx, st.browAskDest, st.browserFolder);
					fflush(stdout);
				}
			}
		}
		// ── NK_AGENT_CLICK / NK_AGENT_RCLICK : injecter un CLIC ponctuel ───
		// `NK_AGENT_DRAG` ne pilote que le bouton GAUCHE et decrit un GLISSEMENT :
		// il ne pouvait donc NI ouvrir un menu contextuel (clic droit) NI choisir
		// une entree dedans. Un menu ne se prouvait pas tout seul -- il fallait
		// deranger quelqu'un pour qu'il clique a notre place.
		// UN INSTRUMENT QUI MANQUE COUTE A CHAQUE FOIS QU'IL MANQUE.
		//   NK_AGENT_CLICK="f,x,y"  : clic GAUCHE a la frame f, aux pixels (x,y)
		//   NK_AGENT_RCLICK="f,x,y" : clic DROIT, meme forme
		// Les deux se combinent : ouvrir le menu au clic droit, puis choisir une
		// entree au clic gauche quelques frames plus tard.
		// Chronologie sur TROIS images, comme une vraie main : f = survol seul (le
		// survol precede le clic d'au moins une image, cf. hotIdPrev), f+1 = appui,
		// f+2 = relachement. C'est BeginFrame qui en tire "vient d'etre clique".
		{
			static float32 sClk[2][3] = {{-1.f, 0.f, 0.f}, {-1.f, 0.f, 0.f}};
			static bool sClkInit = false;
			if (!sClkInit) {
				sClkInit = true;
				for (int32 b = 0; b < 2; ++b) {
					const char *v = std::getenv(b == 0 ? "NK_AGENT_CLICK" : "NK_AGENT_RCLICK");
					if (!v)
						continue;
					float32 f[3] = {-1.f, 0.f, 0.f};
					const char *q = v;
					for (int32 k = 0; k < 3 && *q; ++k) {
						f[k] = (float32)atof(q);
						while (*q && *q != ',')
							++q;
						if (*q == ',')
							++q;
					}
					sClk[b][0] = f[0];
					sClk[b][1] = f[1];
					sClk[b][2] = f[2];
				}
			}
			for (int32 b = 0; b < 2; ++b) {
				if (sClk[b][0] < 0.f)
					continue;
				const int32 k = agentFrame + 1 - (int32)sClk[b][0];
				if (k < 0 || k > 2)
					continue;
				ui.input.mousePos = {sClk[b][1], sClk[b][2]};
				ui.input.mouseDown[b] = (k == 1);
				std::printf("[nk3d-clic] bouton=%s k=%d pos=(%.0f,%.0f) enfonce=%d\n",
							b == 0 ? "GAUCHE" : "DROIT", k, ui.input.mousePos.x, ui.input.mousePos.y,
							ui.input.mouseDown[b] ? 1 : 0);
				std::fflush(stdout);
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
		// ── UN MENU DE LA BARRE PRINCIPALE RESERVE LA SAISIE ────────────────
		// 🔴 Rodolf, 2026-09-05 : « les menus du menu principal laissent traverser
		// les evenements ». Meme famille que deux defauts deja clos ailleurs -- la
		// molette sous un menu contextuel, et les modales qui laissaient passer
		// souris ET clavier. Ici les trois passaient :
		//   * la MOLETTE et les TOUCHES, parce que le code qui les lit ne passe pas
		//     toujours par le registre (`WheelIn`, la vue 3D lisent `ui.input`) ;
		//   * les CLICS des panneaux, parce que le menu deroule ne declarait PAS
		//     son emprise -- les six menus contextuels de la hierarchie le font
		//     depuis le 14 aout, celui de la barre principale n'avait jamais ete
		//     rattache. Une lecon ecrite a cote d'un chemin ne couvre pas le
		//     chemin voisin.
		// Un menu deroule EST une surcouche modale au sens de ce mecanisme : on
		// rejoint donc celui qui existe, on n'en ajoute pas un troisieme. L'entree
		// reelle est RENDUE avant de peindre les surcouches, plus bas -- le menu,
		// lui, doit repondre, et `NkMenuBarClics` redeclare la barre a ce
		// moment-la pour qu'on puisse passer d'un menu a l'autre.
		// NK_MENU_OPEN=<i> : deroule le menu i AU DEMARRAGE, en posant `st.openMenu`
		// -- le MEME champ que le clic sur la barre pose. Aucune injection d'entree
		// souris ni clavier : on arme l'etat, on ne conduit pas la machine.
		{
			static int32 sMenuIdx = -2;
			if (sMenuIdx == -2) {
				const char *v = std::getenv("NK_MENU_OPEN");
				sMenuIdx = v ? (int32)std::atoi(v) : -1;
			}
			if (sMenuIdx >= 0 && agentFrame >= 12)
				st.openMenu = sMenuIdx; // maintenu : un menu qu'on rouvre chaque
										// image se comporte comme un menu ouvert
		}
		const bool menuDeroule = st.openMenu >= 0;
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
		// CE QUE LA SONDE ③ LIT, et c'est le point : ce drapeau est pose DANS la
		// branche qui vide reellement l'entree, jamais recalcule a cote. Une sonde
		// qui relirait `menuDeroule` mesurerait l'INTENTION ; celle-ci mesure le
		// CHEMIN -- retirez `menuDeroule` de la condition et elle rougit.
		bool saisieVidee = false;
		if (modalOpen || menuDeroule || sourisSurJournal) {
			saisieVidee = true;
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
		// ── SONDE ③ : CE QU'UN MENU DEROULE RESERVE VRAIMENT ────────────────
		// Elle lit les valeurs de LA BOUCLE, apres le basculement de l'emprise --
		// pas des valeurs qu'elle poserait elle-meme (un banc qui pose l'etat
		// qu'il mesure prouve la regle, pas le chemin).
		// PERIMETRE, DIT ICI : elle prouve que la saisie est RESERVEE et que
		// l'emprise COUVRE les panneaux. Elle ne fabrique ni molette ni touche
		// (aucune injection d'entree n'est permise) : qu'un vrai coup de molette
		// soit avale reste a verifier d'un geste humain.
		{
			static int32 sProbeFrame = -2;
			if (sProbeFrame == -2) {
				const char *v = std::getenv("NK_MENU_PROBE");
				sProbeFrame = v ? (int32)std::atoi(v) : -1;
			}
			if (sProbeFrame > 0 && agentFrame == sProbeFrame) {
				const NkRect &e = st.uiBlockCur;
				auto couvre = [&](const NkRect &r) {
					if (!st.uiBlockCurOn || r.w <= 0.f || r.h <= 0.f)
						return 0;
					return (e.x < r.x + r.w && e.x + e.w > r.x && e.y < r.y + r.h &&
							e.y + e.h > r.y)
							   ? 1
							   : 0;
				};
				std::printf("[sonde3] menu=%d reserve=%d emprise=%d (%.0f, %.0f, %.0f, %.0f) "
							"couvre_vue=%d couvre_hier=%d couvre_navig=%d\n",
							st.openMenu, saisieVidee ? 1 : 0, st.uiBlockCurOn ? 1 : 0, e.x, e.y,
							e.w, e.h, couvre(st.viewRect), couvre(st.hierRect),
							couvre(st.browserRect));
				std::printf("[sonde3] entree vue par les panneaux : molette=%.2f touches=%d "
							"clics=%d caracteres=%d\n",
							(double)ui.input.wheel,
							ui.input.keyDown[0] || ui.input.keyInit[0] ? 1 : 0,
							(ui.input.mouseClicked[0] || ui.input.mouseClicked[1] ||
							 ui.input.mouseClicked[2])
								? 1
								: 0,
							(int)ui.input.charCount);
			}
		}
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
		// ── NK_EDIT_DIAG=1 : LES DEUX MODES SONT-ILS D'ACCORD ? ─────────────
		// `st.mode` est l'etat du SHELL ; `Demo3DHostInEditMode()` celui du
		// VISEUR. Ils peuvent diverger indefiniment : `Demo3DHostSetMode` n'impose
		// rien, il arme une bascule qui ECHOUE si aucun objet n'est selectionne.
		// Rien, nulle part, ne mesurait cet ecart -- on ne pouvait donc pas
		// distinguer « le mode edition ne marche pas » de « le mode edition n'a
		// jamais commence ». Une ligne toutes les 30 images, pas une par image.
		{
			static const bool trDiag = (std::getenv("NK_EDIT_DIAG") != nullptr);
			if (trDiag && (agentFrame % 30) == 0) {
				const int32 refus = demo::Demo3DHostEditRefusedFrames();
				// COMBIEN D'OBJETS DE L'UTILISATEUR VIVENT, et quelle geometrie porte
				// le maillage courant. Les deux repondent a « ajouter en edition
				// cree-t-il un objet a part ou entre-t-il dans le maillage ? » -- et
				// il faut les DEUX : un seul des deux compteurs laisserait la
				// question ouverte.
				// ⚠ `Demo3DHostObjectCount` ne convient pas : il rend une CONSTANTE
				// (les objets de demonstration), pas les nœuds de l'utilisateur.
				int32 vivants = 0;
				const int32 nTot = demo::Demo3DHostNodeCount();
				for (int32 q = 0; q < nTot; ++q)
					if (demo::Demo3DHostUserKind(q) != 0 && !demo::Demo3DHostNodeDeleted(q))
						++vivants;
				uint32 gv = 0, ge = 0, gf = 0, gt = 0;
				(void)demo::Demo3DHostStats(&gv, &ge, &gf, &gt);
				// L'AFFICHAGE SUIT-IL LA CAGE, et l'aimantation est-elle armee ?
				// Les deux repondent a des questions de Rodolf qu'aucun banc ne
				// pouvait poser : « le deplacement ne se voit pas en temps reel »
				// et « le snap marche-t-il en edition ? ».
				bool un11 = false, mods = false, snapOn = false, snapAbs = false;
				uint32 dvc = 0, rvc = 0;
				float32 snapPas = 0.f, piv[3] = {0.f, 0.f, 0.f};
				const bool dispOk =
					demo::Demo3DHostEditDisplayInfo(&un11, &dvc, &rvc, &mods);
				const bool snapOk = demo::Demo3DHostEditSnapInfo(&snapOn, &snapPas, &snapAbs, piv);
				// LA POSITION D'UN SOMMET, LUE PENDANT LE GESTE. C'est la seule
				// facon de distinguer « l'operation n'est pas appliquee » de
				// « elle est appliquee mais rien ne la repeint ». Le sommet 0
				// suffit : on mesure une VARIATION, pas une valeur absolue.
				float32 vl[3] = {0.f, 0.f, 0.f}, vw[3] = {0.f, 0.f, 0.f};
				// -1 : le PREMIER SOMMET SELECTIONNE, pas le sommet 0 -- lire un sommet
				// que le geste ne concerne pas ferait conclure a tort que rien ne bouge.
				const bool vOk = demo::Demo3DHostEditVertPos(-1, vl, vw);
				// LA VALEUR DE L'OPERATION MODALE, a cote de la position du sommet.
				// C'est ce couple qui separe les DEUX causes possibles du « rien ne
				// bouge » : la modale ne calcule pas (valeur figee), ou elle calcule
				// et n'applique pas (valeur qui monte, sommet immobile).
				int32 mop2 = 0, mseg2 = 1;
				const char *mn2 = nullptr, *mlv2 = nullptr, *mls2 = nullptr;
				float32 mval2 = 0.f;
				const bool mOk2 = demo::Demo3DHostModalInfo(&mop2, &mn2, &mlv2, &mval2, &mls2, &mseg2);
				std::printf("[nk3d-diag] f=%4d shell.mode=%d(edit=%d) viseur.edit=%d "
							"refus=%d masque=%d selection=%d modale=%d "
							"noeuds=%d v=%u a=%u f=%u\n",
							(int)agentFrame, (int)st.mode, (st.mode != NkMode::Object) ? 1 : 0,
							demo::Demo3DHostInEditMode() ? 1 : 0, (int)refus,
							(int)demo::Demo3DHostEditSelMask(),
							(int)demo::Demo3DHostEditSelCount(),
							demo::Demo3DHostModalActive() ? 1 : 0,
							(int)vivants, gv, ge, gf);
				if (dispOk || snapOk) {
					std::printf("[nk3d-vue ] f=%4d affiche1pour1=%d (disp=%u cage=%u mods=%d) "
								"| snap actif=%d pas=%.3f absolu=%d pivot=(%.3f %.3f %.3f)\n",
								(int)agentFrame, un11 ? 1 : 0, dvc, rvc, mods ? 1 : 0,
								snapOn ? 1 : 0, (double)snapPas, snapAbs ? 1 : 0,
								(double)piv[0], (double)piv[1], (double)piv[2]);
				}
				if (vOk) {
					std::printf("[nk3d-vert] f=%4d sommet0 local=(%.4f %.4f %.4f) "
								"monde=(%.4f %.4f %.4f)\n",
								(int)agentFrame, (double)vl[0], (double)vl[1], (double)vl[2],
								(double)vw[0], (double)vw[1], (double)vw[2]);
				}
				// LE CRITERE QUI MANQUAIT : combien de lignes du panneau ont ete
				// coupees faute de largeur. Il vaut 0 quand tout tient. C'est une
				// image qui a trouve la premiere troncature ; celui-ci est la pour
				// que la prochaine se MESURE.
				std::printf("[nk3d-txt ] f=%4d lignes tronquees = %d\n",
							(int)agentFrame, (int)nk3d::NkPropTronquees());
				if (mOk2) {
					std::printf("[nk3d-mod ] f=%4d op=%d (%s) %s=%.4f %s=%d\n",
								(int)agentFrame, (int)mop2, mn2 ? mn2 : "?", mlv2 ? mlv2 : "?",
								(double)mval2, mls2 ? mls2 : "(pas de segments)", (int)mseg2);
				}
				std::fflush(stdout);
			}
		}
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

		// NK_ADD_NODE2=<kind>[,sub[,frame]] : UN SECOND AJOUT, par le MEME chemin
		// que NK_ADD_NODE (`Demo3DHostAddNode`, celui du menu Ajouter).
		// POURQUOI IL FAUT UN SECOND. `NK_ADD_NODE` ne tire qu'une fois (son
		// `sAddDone` est unique), et il tire AVANT l'entree en edition. La question
		// de Rodolf -- « ajouter un element EN MODE EDITION, sous-maillage ou objet
		// a part ? » -- porte precisement sur un ajout qui arrive APRES. Sans ce
		// second levier, elle n'est mesurable par AUCUN banc : il faudrait cliquer.
		{
			static bool sAdd2Done = false;
			if (const char *an = std::getenv("NK_ADD_NODE2")) {
				int32 v[3] = {2, 0, 100};
				int32 k = 0;
				for (const char *p2 = an; k < 3 && *p2;) {
					v[k++] = (int32)std::atoi(p2);
					while (*p2 && *p2 != ',')
						++p2;
					if (*p2 == ',')
						++p2;
				}
				if (!sAdd2Done && agentFrame >= v[2]) {
					sAdd2Done = true;
					// On NOTE l'etat AVANT : « combien d'objets, quelle geometrie ».
					// Une mesure prise seulement APRES ne dirait pas de combien ca a
					// change, et c'est la variation qui repond a la question.
					int32 av = 0;
					const int32 nT2 = demo::Demo3DHostNodeCount();
					for (int32 q = 0; q < nT2; ++q)
						if (demo::Demo3DHostUserKind(q) != 0 && !demo::Demo3DHostNodeDeleted(q))
							++av;
					uint32 v0 = 0, e0 = 0, f0 = 0, t0 = 0;
					(void)demo::Demo3DHostStats(&v0, &e0, &f0, &t0);
					const int32 nd = demo::Demo3DHostAddNode(v[0], v[1]);
					int32 ap = 0;
					for (int32 q = 0; q < nT2; ++q)
						if (demo::Demo3DHostUserKind(q) != 0 && !demo::Demo3DHostNodeDeleted(q))
							++ap;
					uint32 v1 = 0, e1 = 0, f1 = 0, t1 = 0;
					(void)demo::Demo3DHostStats(&v1, &e1, &f1, &t1);
					std::printf("[nk3d-add2] frame=%d edition=%d -> noeud %d | noeuds %d->%d "
								"| maillage v %u->%u  a %u->%u  f %u->%u\n",
								(int)agentFrame, demo::Demo3DHostInEditMode() ? 1 : 0, (int)nd,
								(int)av, (int)ap, v0, v1, e0, e1, f0, f1);
					std::fflush(stdout);
				}
			} else {
				sAdd2Done = true;
			}
		}

		// NK_FOLD_OPEN="<cmd>[,<cmd>...]" : DEPLIE des blocs d'operation au demarrage.
		// ⚠ CE N'EST PAS UN CHEMIN D'ARMEMENT D'UN ETAT DU PRODUIT, c'est une
		// commande d'INSPECTION : elle pose le meme etat que le clic sur le
		// chevron, par la meme porte (`NkFoldTable::Poser`). Sans elle, le contenu
		// d'un bloc n'est verifiable a l'image par personne -- puisqu'ils naissent
		// TOUS plies, ce qui est precisement ce que Rodolf a demande.
		{
			static bool sFoldDone = false;
			if (const char *fo = std::getenv("NK_FOLD_OPEN")) {
				if (!sFoldDone) {
					sFoldDone = true;
					for (const char *q = fo; *q;) {
						char kb[48];
						snprintf(kb, sizeof(kb), "prop.g.op.%d", (int)std::atoi(q));
						st.grpFold.Poser(kb, false); // false = DEPLIE
						while (*q && *q != ',')
							++q;
						if (*q == ',')
							++q;
					}
				}
			} else {
				sFoldDone = true;
			}
		}

		// NK_UNDO_TEST=<frame> : LE NEGATIF CAPITAL, en un seul lancement.
		// A la frame f     : releve l'empreinte APRES l'operation
		// A la frame f+20  : demande l'annulation, par la porte du clavier
		// A la frame f+40  : releve -- doit egaler l'empreinte D'AVANT l'operation
		// A la frame f+60  : demande le retablissement
		// A la frame f+80  : releve -- doit egaler l'empreinte D'APRES
		// L'empreinte d'AVANT est prise des l'entree en edition, avant toute op.
		//
		// ⚠ « Identique au bit » et « memes compteurs » ne sont pas la meme chose.
		// Ce matin un journal affirmait « comparaison bit a bit = IDENTIQUE » --
		// pour le gizmo -- pendant que le maillage restait deplace. L'empreinte
		// hache les BITS des positions, la selection et la topologie.
		{
			static int32 sUndoF = -1;
			static uint64 sAvant = 0, sApres = 0, sAvantGeo = 0, sAvantPos = 0, sAvantSel = 0, sAvantTopo = 0;
			static bool sAvantPris = false;
			if (const char *ut = std::getenv("NK_UNDO_TEST")) {
				if (sUndoF < 0)
					sUndoF = (int32)std::atoi(ut);
				uint64 emp = 0, geo = 0, pos = 0, sel = 0, topo = 0;
				uint32 nv = 0, nf = 0;
				const bool ok = demo::Demo3DHostEditFingerprint(&emp, &nv, &nf, &geo, &pos, &sel, &topo);
				// ⚠ L'ETAT DE REFERENCE SE PREND QUAND IL EST STABLE, pas a la
				// premiere image d'edition. Mesure du 14/09 : l'empreinte change
				// encore entre l'entree en edition et la frame 70 -- la selection
				// n'est pas normalisee tout de suite. Prise trop tot, la reference
				// n'est l'etat d'AUCUN moment, et le negatif accuse l'annulation
				// d'un ecart qu'elle n'a pas produit. C'est ce qu'il a fait, et j'ai
				// publie la conclusion fausse avant de la mesurer.
				// La stabilite se constate : deux releves consecutifs identiques.
				static uint64 sPrec = 0;
				static bool sPrecPris = false;
				const bool stable = sPrecPris && (emp == sPrec);
				sPrec = emp;
				sPrecPris = ok;
				if (ok && !sAvantPris && stable) {
					// LA PREMIERE IMAGE OU L'EDITION EST ACTIVE, et non un numero de
					// frame choisi : mon premier essai prenait l'empreinte a la
					// frame 66, APRES que le pilote ait deja applique l'operation.
					// L'etat  AVANT  etait donc l'etat d'apres, et le negatif
					// comparait une chose a elle-meme. Cinquieme fois aujourd'hui
					// qu'un instrument mal place accuse le produit.
					// L'etat de REFERENCE : en edition, avant toute operation.
					sAvantPris = true;
					sAvant = emp;
					sAvantGeo = geo;
					sAvantPos = pos;
					sAvantSel = sel;
					sAvantTopo = topo;
					std::printf("[nk3d-undo] AVANT  op (etat STABLE, f=%d) : empreinte=%016llx v=%u f=%u\n",
								(int)agentFrame, (unsigned long long)sAvant, nv, nf);
					std::fflush(stdout);
				}
				// QUAND L'ETAT SE STABILISE-T-IL ? Un releve periodique, sans lequel
				// on ne peut pas distinguer « l'annulation a change quelque chose »
				// de « l'etat n'etait pas encore stable quand je l'ai photographie ».
				// C'est la faute que ce releve vient de me faire attraper.
				if (ok && (agentFrame % 10) == 0 && agentFrame <= sUndoF)
					std::printf("[nk3d-stab] f=%4d empreinte=%016llx\n",
								(int)agentFrame, (unsigned long long)emp);
				if (ok && agentFrame == sUndoF) {
					sApres = emp;
					std::printf("[nk3d-undo] APRES  op : empreinte=%016llx v=%u f=%u  (%s)\n",
								(unsigned long long)sApres, nv, nf,
								(sApres == sAvant) ? "INCHANGE -- l'op n'a rien fait ?" : "modifie");
					std::fflush(stdout);
				}
				if (agentFrame == sUndoF + 20) {
					const bool d = demo::Demo3DHostEditUndoAsk();
					std::printf("[nk3d-undo] annulation demandee : %s\n", d ? "oui" : "REFUSEE");
					std::fflush(stdout);
				}
				if (ok && agentFrame == sUndoF + 40) {
					std::printf("[nk3d-undo] APRES undo : empreinte=%016llx geo=%016llx v=%u f=%u  -> %s (geo %s)\n",
								(unsigned long long)emp, (unsigned long long)geo, nv, nf,
								(emp == sAvant) ? "IDENTIQUE AU BIT a l'etat d'avant"
												: "DIFFERENT de l'etat d'avant",
								(geo == sAvantGeo) ? "identique" : "DIFFERENTE");
					std::printf("[nk3d-undo]   positions %s | selection %s | topologie %s\n",
								(pos == sAvantPos) ? "identiques" : "DIFFERENTES",
								(sel == sAvantSel) ? "identique" : "DIFFERENTE",
								(topo == sAvantTopo) ? "identique" : "DIFFERENTE");
					std::fflush(stdout);
				}
				if (agentFrame == sUndoF + 60) {
					const bool d = demo::Demo3DHostEditRedoAsk();
					std::printf("[nk3d-undo] retablissement demande : %s\n", d ? "oui" : "REFUSE");
					std::fflush(stdout);
				}
				if (ok && agentFrame == sUndoF + 80) {
					std::printf("[nk3d-undo] APRES redo : empreinte=%016llx v=%u f=%u  -> %s\n",
								(unsigned long long)emp, nv, nf,
								(emp == sApres) ? "IDENTIQUE AU BIT a l'etat d'apres"
												: "DIFFERENT de l'etat d'apres");
					std::fflush(stdout);
				}
			}
		}

		// NK_OP_PARAM="index,valeur[,frame]" : pose un REGLAGE PERSISTANT d'operation
		// par la MEME porte que le champ du panneau (`Demo3DHostOpParamSet`), donc
		// avec le meme clamp. Sert a prouver ce que le canal exige : « changer la
		// propriete change la GEOMETRIE » -- sans quoi un champ affiche n'est qu'un
		// decor mieux habille.
		{
			static bool sOpPDone = false;
			if (const char *op = std::getenv("NK_OP_PARAM")) {
				float32 v[3] = {0.f, 0.f, 90.f};
				int32 k = 0;
				for (const char *q = op; k < 3 && *q;) {
					v[k++] = (float32)std::atof(q);
					while (*q && *q != ',')
						++q;
					if (*q == ',')
						++q;
				}
				if (!sOpPDone && agentFrame >= (int32)v[2]) {
					sOpPDone = true;
					const int32 idx = (int32)v[0];
					float32 avant = 0.f, apres = 0.f;
					(void)demo::Demo3DHostOpParamGet(idx, &avant);
					const bool ok = demo::Demo3DHostOpParamSet(idx, v[1]);
					(void)demo::Demo3DHostOpParamGet(idx, &apres);
					const char *lib = nullptr;
					int32 cmd = -1, typ = 0;
					(void)demo::Demo3DHostOpParamInfo(idx, &cmd, &lib, &typ, nullptr, nullptr);
					std::printf("[nk3d-opp ] param %d (%s, cmd=%d) : %.3f -> %.3f (pose=%d)\n",
								(int)idx, lib ? lib : "?", (int)cmd, (double)avant, (double)apres,
								ok ? 1 : 0);
					std::fflush(stdout);
				}
			} else {
				sOpPDone = true;
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

		// NK_EDIT_PICK="x,y[,frame][,shift][,alt]" : UN CLIC D'ELEMENT A DES
		// COORDONNEES ECRITES, en pixels de la VUE. Il passe par la MEME porte que
		// le clic de la souris (`Demo3DHostEditPickAt` arme, la vue consomme au
		// meme endroit) -- aucun evenement souris n'est fabrique, aucune position
		// de curseur n'est ecrite.
		// ⚠️ POURQUOI CE CROCHET EXISTE : le pick d'element etait le SEUL geste du
		// mode Edition qu'aucune porte ne pouvait declencher. Le mode, les
		// operations, la selection par indices en avaient une ; designer un sommet
		// a un endroit donne, non. Sans lui, « les trois modes designent trois
		// choses differentes au meme clic » ne pouvait pas se mesurer du tout.
		// La ligne imprimee est le quadruplet de Blender : sommet actif, arete
		// active (deux sommets), face active, et le compte de la selection.
		{
			static bool sPickDone = false;
			if (const char *pk = std::getenv("NK_EDIT_PICK")) {
				float32 px = 0.f, py = 0.f;
				int32 fr = 40;
				int32 shf = 0, alt = 0;
				{
					const char *c = pk;
					px = (float32)std::atof(c);
					auto suivant = [](const char *&p) -> bool {
						while (*p && *p != ',')
							++p;
						if (*p != ',')
							return false;
						++p;
						return true;
					};
					if (suivant(c)) {
						py = (float32)std::atof(c);
						if (suivant(c)) {
							fr = (int32)std::atoi(c);
							if (suivant(c)) {
								shf = std::atoi(c);
								if (suivant(c))
									alt = std::atoi(c);
							}
						}
					}
				}
				if (!sPickDone && agentFrame >= fr && demo::Demo3DHostInEditMode()) {
					sPickDone = true;
					// UNE COORDONNEE ENTRE 0 ET 1 EST UNE FRACTION DE LA VUE, et c'est
					// dit : la taille de la vue depend de la fenetre et des panneaux, et
					// ecrire « 512 » dans un banc qui tournera ailleurs, c'est ecrire un
					// point qui tombera a cote. Au-dessus de 1, c'est un pixel.
					uint32 vw = 0, vh = 0;
					demo::Demo3DHostViewSize(&vw, &vh);
					if (px > 0.f && px <= 1.f)
						px *= (float32)vw;
					if (py > 0.f && py <= 1.f)
						py *= (float32)vh;
					const bool arme = demo::Demo3DHostEditPickAt(px, py, shf != 0, alt != 0);
					std::printf("[nk3d] NK_EDIT_PICK (%.1f,%.1f) vue=%ux%u mode=%d arme=%d\n",
								(double)px, (double)py, vw, vh,
								(int)demo::Demo3DHostEditSelMask(), arme ? 1 : 0);
				}
				// Le RESULTAT se lit une frame APRES l'armement : le pick est
				// consomme dans la vue, pas ici. Lire tout de suite rendrait
				// l'etat d'AVANT le clic -- la faute deja payee sur les modes.
				static int32 sPickLu = -1;
				if (sPickDone && sPickLu < 0 && agentFrame >= fr + 2) {
					sPickLu = agentFrame;
					int32 v = -1, ea = -1, eb = -1, f = -1;
					const bool ok = demo::Demo3DHostEditActive(&v, &ea, &eb, &f);
					std::printf("[nk3d] PICK RESULTAT mode=%d : sommet=%d arete=(%d,%d) face=%d "
								"selection=%d (lu=%d)\n",
								(int)demo::Demo3DHostEditSelMask(), (int)v, (int)ea, (int)eb, (int)f,
								(int)demo::Demo3DHostEditSelCount(), ok ? 1 : 0);
				}
			} else {
				sPickDone = true;
			}
		}

		// NK_EDIT_REPORT="<frame>[,<frame2>]" : l'etat du mode Edition a UNE ou DEUX
		// frames donnees -- comptes reels, selection, masque, ET disponibilite de
		// l'annulation. Deux frames parce qu'une operation se juge par un AVANT et
		// un APRES, et qu'un seul relevé ne dit jamais ce qui a change.
		// ⚠️ `annuler`/`refaire` sont dans la meme ligne que les comptes A DESSEIN :
		// un mode edition sans annulation est un piege, pas un outil -- et la seule
		// facon de le savoir est de le lire au meme moment que le reste.
		{
			static bool sRep1 = false, sRep2 = false;
			if (const char *rp = std::getenv("NK_EDIT_REPORT")) {
				int32 f1 = 50, f2 = -1;
				f1 = (int32)std::atoi(rp);
				{
					const char *c = rp;
					while (*c && *c != ',')
						++c;
					if (*c == ',')
						f2 = (int32)std::atoi(c + 1);
				}
				auto ecrire = [&](int32 quand) {
					uint32 vv = 0, ve = 0, vf = 0, vt = 0;
					const bool ok = demo::Demo3DHostStats(&vv, &ve, &vf, &vt);
					std::printf("[nk3d] EDIT RAPPORT frame=%d : v=%u e=%u f=%u t=%u | selection=%d "
								"| masque=%d | annuler=%d refaire=%d (lu=%d)\n",
								(int)quand, vv, ve, vf, vt, (int)demo::Demo3DHostEditSelCount(),
								(int)demo::Demo3DHostEditSelMask(),
								demo::Demo3DHostEditCanUndo() ? 1 : 0,
								demo::Demo3DHostEditCanRedo() ? 1 : 0, ok ? 1 : 0);
				};
				if (!sRep1 && f1 > 0 && agentFrame >= f1) {
					sRep1 = true;
					ecrire(agentFrame);
				}
				if (!sRep2 && f2 > 0 && agentFrame >= f2) {
					sRep2 = true;
					ecrire(agentFrame);
				}
			} else {
				sRep1 = sRep2 = true;
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

		// NK_SET_PARENT="enfant,parent[,frame]" : PARENTE deux noeuds par la
		// facade (Demo3DHostSetNodeParent, le meme chemin que Ctrl+P et le depot).
		// Sans lui, une hierarchie IMBRIQUEE ne se produit qu'a la souris — donc
		// les guides d'indentation d'un arbre ne se prouvaient pas en headless.
		// Un jeu de donnees incapable de produire le cas rend toute mesure verte.
		{
			static int32 sParFrame = -2, sParChild = -1, sParParent = -1;
			if (sParFrame == -2) {
				sParFrame = -1;
				if (const char *v = std::getenv("NK_SET_PARENT")) {
					int32 f[3] = {-1, -1, 70};
					const char *q = v;
					for (int32 k = 0; k < 3 && *q; ++k) {
						f[k] = (int32)std::atoi(q);
						while (*q && *q != ',')
							++q;
						if (*q == ',')
							++q;
					}
					sParChild = f[0];
					sParParent = f[1];
					sParFrame = f[2];
				}
			}
			if (sParFrame > 0 && agentFrame == sParFrame && sParChild >= 0) {
				const bool ok = demo::Demo3DHostSetNodeParent(sParChild, sParParent);
				std::printf("[nk3d] NK_SET_PARENT %d -> parent %d : %s\n", (int)sParChild,
							(int)sParParent, ok ? "fait" : "REFUSE");
				std::fflush(stdout);
				sParFrame = -1;
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
				// ⚠ CES TROIS CAS N'ONT JAMAIS RIEN FAIT, ET C'EST LE CORRECTIF.
				// Ils posaient `st.subMode`, un MIROIR que la boucle REECRIT a chaque
				// image depuis le viseur (« Sous-mode : refleter le masque reel »,
				// plus haut dans ce fichier). La valeur ecrite ici etait donc ecrasee
				// a l'image suivante : un raccourci qui ecrit dans un miroir n'est pas
				// un raccourci.
				//
				// C'ETAIT UN SECOND CHEMIN vers le meme etat -- le motif que TAB a
				// deja paye dans `NkDemo3D.cpp` (« TAB N'EST PLUS TRAITE ICI, ET
				// C'EST LE CORRECTIF »). Le viseur tient deja 1/2/3, garde par son
				// etat REEL, et lui seul sait faire Maj+1/2/3 = COMBINER.
				//
				// ⚠ ET ON NE LES REBRANCHE SURTOUT PAS vers `Demo3DHostSetEditSelMask` :
				// les deux rappels recoivent la MEME touche. Sur Maj+1, le viseur ferait
				// son XOR et ce cas-ci ecraserait par le bit seul -- la combinaison
				// serait perdue, et le defaut n'apparaitrait QUE modificateur enfonce.
				// On retire le doublon, on ne le repare pas.
				case NkVpAction::SubModeVertex:
				case NkVpAction::SubModeEdge:
				case NkVpAction::SubModeFace:
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
		// Sous NK_SONDE, le nom du projet cede la place a l'avertissement : c'est
		// le texte CENTRAL de la barre, donc celui qu'une capture d'ecran montre.
		PaintMenuBarI(p, lay.menu,
					  sonde ? "*** SONDE DE MESURE - CETTE FENETRE N'EST PAS LE PRODUIT ***"
							: (proj.open && !proj.name.Empty() ? proj.name.CStr() : "Aucun projet"),
					  st, hit);
		PaintTabsI(p, lay.tabs, st, hit, ws, ui.input);
		PaintToolbar(p, lay.tool, st, hit, ws, combo);
		// Un panneau MASQUE n'est pas peint en taille nulle : il n'est pas peint du
		// tout. Le peindre dans un rectangle de 14 px declarerait ses zones cliquables
		// les unes sur les autres et un clic sur la poignee tomberait sur la premiere
		// ligne de la liste.
		if (st.showLeft) {
			// ── NK_KIT_TREE=1 : LE TREE_VIEW DU KIT, RENDU PAR L'ADAPTATEUR ─────
			// Premiere consommation reelle de `NkModelerComponentPaint` (la dette du
			// 18/08). DERRIERE UN LEVIER, pas en remplacement : le panneau historique
			// reste le defaut tant que Rodolf n'a pas tranche l'adoption. Le levier
			// sert la PREUVE — trois verifications pre-enregistrees (NK3D-120) :
			// guide d'indentation couleur `guide` (pas border), curseur de renommage
			// couleur `text`, et le centrage se prouve sur le navigateur.
			static const bool kitTree = (std::getenv("NK_KIT_TREE") != nullptr);
			if (kitTree)
				PaintKitTree(p, lay.left, st, ui.input);
			else
				PaintHierarchy(p, lay.left, st, hit, ws, ui.input, &ui);
		}
		// Le contexte GUI est passe pour le MENU CONTEXTUEL du maillage : le
		// composant du kit dessine sur la couche overlay et gere lui-meme
		// l'occlusion, ce qu'un peintre seul ne sait pas faire.
		PaintViewport(p, lay.view, st, hit, ws, ui.input, combo, checks, shortcuts, &ui);
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
				// LA TABLE DESCEND JUSQU'AU PANNEAU. Sans elle, ses deux listes de
				// raccourcis etaient des chaines recopiees a la main -- ce que
				// `NkModelerMeshMenu.h` interdit par ecrit : « une chaine recopiee
				// peut mentir sans que rien ne le signale ».
				PaintPropertiesUnified(p, rightR, st, hit, ws, ui.input, combo, &ui, &shortcuts);
			}
		}
		if (st.showBrowser) {
			// NK_KIT_BROWSER=1 : le content_browser du kit via l'adaptateur — la
			// preuve du CENTRAGE (pied de carte). Le panneau historique reste le
			// defaut.
			static const bool kitBrowser = (std::getenv("NK_KIT_BROWSER") != nullptr);
			if (kitBrowser)
				PaintKitBrowser(p, lay.browser, st, ui.input);
			else
				PaintBrowser(p, lay.browser, st, hit, ws, ui.input, &ui, &combo);
		}
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
		if (modalOpen || menuDeroule || sourisSurJournal) {
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
			// LE SELECTEUR DE LA MAISON par defaut (celui de NkUIDesign, dans
			// NKEditorKit) : rail a sections, vignettes, fil d'Ariane, filtres
			// NOMMES, tri. Demande de Rodolf, 2026-09-05.
			//
			// ⚠️ UNE EXCEPTION, MESUREE ET NOMMEE, pas une hesitation :
			// `NkDrawSelecteur` n'appelle que `PickerTitle()` et
			// `PickerConfirmLabel()`. Il n'appelle NI `PickerExtraHeight`, NI
			// `PickerBottomReserve`, NI `PickerConfirmEnabled`, NI
			// `PickerClearExtraFocus` -- les quatre points par lesquels
			// `NkModelerPicker` greffe l'assistant « Nouveau materiau » (le champ
			// de nom et le combo de type). Y basculer ce mode-la ferait DISPARAITRE
			// l'assistant en silence : un refactor se juge sur ce qu'il ne change
			// pas. Le mode materiau garde donc l'ANCIEN dessin jusqu'a ce que le
			// selecteur du kit porte une region supplementaire. Ce n'est PAS un
			// quatrieme selecteur : ce sont les deux qui existent deja, et le neuf
			// devient le defaut. Note dans la ROADMAP.
			if (st.picker.matNewMode)
				editorkit::NkDrawFilePicker(ui, st.picker, editorkit::NkFilePickerStyle{});
			else
				(void)editorkit::NkDrawSelecteur(ui, st.picker, theme);
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
					// pose ICI, avant l'ecriture -- c'est `Card(i).parent` qui
					// decide du chemin du `.nkmat` (NkAsRelFor).
					const int32 dossier = nk3d::NkAsFolderFromAbs(
						st, st.projectRoot, st.picker.pickerResultPath);
					for (int32 b3 = 0; b3 < st.BrowserCount(); ++b3)
						if (st.Card(b3).kind == 2 && st.Card(b3).mat == ni + 1) {
							st.Card(b3).parent = dossier;
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
			// ⚠️ PAR LA PORTE DE LISTE, meme pour un seul chemin (Rodolf,
			// 06/09 : « dans le chargeur on doit pouvoir avoir la possibilite
			// de charger plusieurs fichiers »). Le selecteur du kit ne rend
			// encore qu'UN `pickerResultPath` -- la selection multiple y est le
			// chantier d'un autre agent -- mais l'appelant est deja ecrit pour
			// une liste : le jour ou le kit la livre, c'est CETTE ligne qui
			// change, et rien en dessous.
			if (st.pickerAction == 2 && st.picker.pickerResultPath[0]) {
				const char *un[1] = {st.picker.pickerResultPath};
				(void)nk3d::NkImportFiles(st, un, 1);
			}
			// 3 = GENERER UN OBJET DEPUIS UNE IMAGE (bouton « Generer », GENIA).
			// Le generateur est un PROCESSUS EXTERNE derriere NkIGenerateur ; le
			// glTF qu'il ecrit passe par LA MEME chaine que l'import (ci-dessus).
			// Aucune logique de generation ici : on enchaine, c'est tout.
			if (st.pickerAction == 3 && st.picker.pickerResultPath[0])
				(void)nk3d::NkGeniaImporterImage(st, st.picker.pickerResultPath);
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

		// ── LE RESULTAT DES ACTIONS, EN DERNIER ─────────────────────────────
		// APRES les panneaux, APRES les modales, APRES le selecteur : une
		// incrustation se peint en dernier, sinon elle existe sans se voir --
		// et c'est precisement le defaut que ces messages reparent. Couche 200 :
		// au-dessus meme des modales (100), pour que la croix reste cliquable
		// quand un dialogue est ouvert.
		{
			NkHitRegistry::LayerScope toastLayer(hit, 200);
			(void)nk3d::NkToastPaint(hit, (float32)W, (float32)H, lay.status.h);
		}

		// ── SONDE DU PEINTRE (`NK3D_SONDE_PEINTRE=1`) — canal onglets, (o1) ──
		// ⚠️ EN DERNIER, ET C'EST UNE CORRECTION MESUREE. Posee d'abord dans le
		//    rectangle du viseur, elle n'apparaissait PAS sur la capture : l'image
		//    de la vue 3D est composee par-dessus. Ses trois boites etaient
		//    dessinees et recouvertes -- le journal disait « DESSINEE » et le
		//    pixel disait non. Meme lecon que le reste de ce chantier : ce qui est
		//    emis n'est pas ce qui est vu.
		{
			static const bool kSondePeintre = (std::getenv("NK3D_SONDE_PEINTRE") != nullptr);
			if (kSondePeintre)
				PaintSondePeintre(p, {0.f, lay.tool.y + lay.tool.h, (float32)W, (float32)H});
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
					for (int32 b = 0; b < st.BrowserCount(); ++b) {
						if (st.Card(b).kind != 2 || st.Card(b).mat <= 0)
							continue;
						char nm[64];
						float32 alb[3];
						float32 rg = 0.f, mt = 0.f;
						if (demo::Demo3DHostProjMatInfo(st.Card(b).mat - 1, nm,
														(uint32)sizeof(nm), alb, &rg, &mt))
							NkWidgetState::Copy(st.Card(b).name, nm, 31u);
					}
					nk3d::NkMarkDirty(st);
					// ON REECRIT LE DISQUE TOUT DE SUITE (Rihen, 13 aout). Renommer
					// en memoire ne suffisait pas : les deux fichiers restaient
					// « Materiau.nkmat » dans leurs dossiers respectifs, et le doublon
					// revenait a la reouverture. `NkProjectWriteAssets` ecrit le
					// fichier sous son NOUVEAU nom puis efface l'ancien -- il connait
					// le chemin precedent par `Card(i).file`, justement pour ne pas
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
		// NK_AGENT_MATPREV=<n> : a partir de la trame n, DEMANDE l'apercu du
		// premier materiau, sans passer par le panneau -- et sauve sa cible en
		// PNG. C'est le pendant de NK_AGENT_SCENE pour les materiaux.
		//
		// ⚠️ POURQUOI IL EXISTE. L'apercu de materiau est une cible hors ecran
		// comme le viseur, et son AFFICHAGE passe par le meme Image() a UV
		// {0,0}->{1,1}, celui dont on a MESURE qu'il sortait retourne sur OpenGL.
		// Mais aucun temoin ne pouvait le juger : l'apercu n'est rendu que si le
		// panneau a depose un slot, et on ne pilote pas l'interface. Sans ce
		// levier son orientation restait INCONNUE, et je refusais de la deduire.
		//
		// ⚠️ IL NE CHANGE RIEN QUAND LA VARIABLE EST ABSENTE : sans elle, le slot
		// reste celui du panneau, a l'octet pres.
		{
			static int32 sMatPrev = -2;
			static bool sMatPrevDemande = false;
			if (sMatPrev == -2) {
				const char *v = std::getenv("NK_AGENT_MATPREV");
				sMatPrev = v ? (int32)std::atoi(v) : -1;
			}
			if (sMatPrev > 0 && agentFrame >= sMatPrev && demo::Demo3DHostReady()) {
				// Le premier emplacement OCCUPE : Info rend faux sur un emplacement
				// libre, et il n'existe pas d'accesseur « combien ».
				int32 slot = -1;
				for (int32 i = 0; i < 64 && slot < 0; ++i) {
					char nom[64];
					float32 alb[3], rg = 0.f, mt = 0.f;
					if (demo::Demo3DHostProjMatInfo(i, nom, (uint32)sizeof(nom), alb, &rg, &mt))
						slot = i;
				}
				if (slot < 0)
					slot = demo::Demo3DHostProjMatCreate();
				if (slot >= 0) {
					st.matPrevSlot = slot; // CE QUE LE PANNEAU AURAIT DEPOSE
					st.matPrevW = 260;
					st.matPrevH = 150;
					// La prise se fait sur DEUX trames -- rendu puis relecture -- donc
					// on demande une trame APRES avoir pose le slot.
					if (!sMatPrevDemande && agentFrame >= sMatPrev + 3) {
						sMatPrevDemande = true;
						char mp[256];
						if (NkNextCapturePath("matprev", mp, (int32)sizeof(mp))) {
							demo::Demo3DHostMatThumbRequest(slot, mp);
							std::printf("[nk3d] NK_AGENT_MATPREV : materiau %d, apercu -> %s\n",
										slot, mp);
						}
					}
				}
			}
		}
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
		// ── LES DEUX BORNES DE NOEUDS DOIVENT S'ACCORDER, ET LE DIRE ────────
		// `kNkvpMaxNodes` (hote) et `kMaxNodeNames` (etat) sont declarees dans
		// deux fichiers que rien ne relie. Tant qu'elles etaient egales par
		// hasard, personne ne le savait ; le jour ou l'une est relevee sans
		// l'autre, les noeuds au-dela perdent leur NOM a l'enregistrement --
		// en silence, dans deux boucles de serialisation. Le desaccord se DIT
		// donc, une fois, au premier tour ou l'hote est pret.
		{
			static bool sBornesDites = false;
			if (!sBornesDites && demo::Demo3DHostReady()) {
				sBornesDites = true;
				const int32 hote = demo::Demo3DHostNodeCount();
				if (hote > (int32)nk3d::NkModelerState::kMaxNodeNames)
					nkentseu::NkLog::Instance().Warnf(
						"[nk3d] BORNES DESACCORDEES : l'hote porte %d noeuds, l'etat ne nomme que "
						"%d. Les noeuds %d..%d perdront leur nom a l'enregistrement. Corriger "
						"NkModelerState::kMaxNodeNames (NkModelerInput.h).",
						hote, (int32)nk3d::NkModelerState::kMaxNodeNames,
						(int32)nk3d::NkModelerState::kMaxNodeNames, hote - 1);
				else
					nkentseu::NkLog::Instance().Infof(
						"[nk3d] MESURE bornes : noeuds hote=%d, noms=%d, plafond d'import = "
						"emplacements utilisateur libres.",
						hote, (int32)nk3d::NkModelerState::kMaxNodeNames);
			}
		}
		if (agentOpenRecent >= 0 && agentFrame >= 3 && demo::Demo3DHostReady()) {
			st.projRecent = agentOpenRecent;
			st.projPending = 7;
			agentOpenRecent = -1;
		}
		// NK_PROJECT=<chemin .nk3dm> : ouvre CE projet, et le CREE s'il n'existe
		// pas encore. Pose a cote de NK_OPEN_RECENT et pour la meme raison,
		// mais par le CHEMIN : mesurer un aller-retour de persistance demande un
		// projet a soi, et passer par la liste des recents obligerait a ecrire
		// dans le fichier de recents de quelqu'un d'autre pour s'y ranger.
		// Meme attente que ci-dessus : l'hote 3D doit etre ne, c'est lui qui
		// porte les noeuds que la restitution recree.
		{
			static bool sProjDone = false;
			if (!sProjDone && agentFrame >= 3 && demo::Demo3DHostReady() &&
				st.projPending == 0) {
				if (const char *v = std::getenv("NK_PROJECT")) {
					sProjDone = true;
					if (*v) {
						std::snprintf(st.projOpenPath, sizeof(st.projOpenPath), "%s", v);
						st.projPending = 9;
					}
				} else
					sProjDone = true;
			}
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
		// NK_AGENT_SCENE : on quitte l'accueil, rien de plus. Le viseur naît a son
		// premier PAINT et porte sa scene par defaut ; aucun projet n'est cree.
		// Le ciel n'est pose QUE lorsque l'hote 3D est pret -- le poser avant
		// serait un reglage ecrit dans le vide, et le banc a deja paye ce defaut.
		if (agentSceneFrame > 0 && agentFrame >= agentSceneFrame) {
			static bool sSceneArmee = false;
			static bool sCielPose = false;
			if (!sSceneArmee) {
				sSceneArmee = true;
				st.welcome = false;
				std::printf("[nk3d] NK_AGENT_SCENE : accueil quitte, aucun projet ouvert\n");
			}
			if (!sCielPose && demo::Demo3DHostReady()) {
				sCielPose = true;
				demo::Demo3DHostSetSkyVisible(true);
				// NK_AGENT_SKY=<m> : choisit le modele. Defaut 2 (Rayleigh+Mie).
				// ⚠️ IL SERT A DERIVER LA REGION DU VISEUR, PAS A VARIER POUR VARIER :
				// deux courses avec deux modeles donnent, PAR DIFFERENCE, l'ensemble
				// des pixels que le ciel occupe -- l'interface, elle, ne bouge pas.
				// Sans ca il faudrait poser un rectangle a l'oeil sur la capture.
				{
					// NK_AGENT_SKY=-1 ETEINT le ciel. C'est ce qui permet de DERIVER la
					// region du viseur proprement : la difference entre ciel allume et
					// ciel eteint couvre TOUT le ciel, la ou la difference entre deux
					// MODELES ne couvre qu'une bande -- j'ai mesure une bande de 96
					// lignes en croyant tenir le viseur, et le verdict d'orientation qui
					// en sortait ne valait rien.
					const char *sm = std::getenv("NK_AGENT_SKY");
					const int32 mdl = (sm && sm[0]) ? (int32)std::atoi(sm) : 2;
					if (mdl < 0) {
						demo::Demo3DHostSetSkyVisible(false);
					} else {
						demo::Demo3DHostSetSkyModel(mdl);
					}
				}
				std::printf("[nk3d] NK_AGENT_SCENE : ciel Rayleigh+Mie pose (hote pret)\n");
			}
		}
		// ── SONDE NK_TOAST_PROBE ────────────────────────────────────────────
		// Les trois verdicts, un de chaque, poses juste avant le declic. Pas au
		// demarrage : un succes ne dure que six secondes et serait deja mort.
		// Aucune injection d'entree -- on appelle la MEME fonction que le code
		// produit (`NkToastPush`), donc la meme table de couleurs.
		// ⚠️ REPOSEES A CHAQUE IMAGE, ET NON UNE FOIS A UNE IMAGE NOMMEE. La
		//    premiere version testait `agentFrame == agentShotFrame - 2` : elle a
		//    donne une capture VIDE une fois sur deux, et j'ai failli conclure
		//    « le theme clair ne peint pas ses pastilles ». Un indice d'image
		//    n'est pas un rendez-vous fiable, et une pastille a duree de vie
		//    (6 s / 12 s) peut mourir entre la pose et le declic. Reposees a
		//    chaque image, la sonde ne depend plus d'aucun timing -- et le
		//    compteur imprime dit ce qu'il y avait DANS LA PILE au declic, pour
		//    qu'une capture vide se lise comme une capture vide et non comme un
		//    verdict sur le theme.
		if (std::getenv("NK_TOAST_PROBE") && agentShotFrame > 0 &&
			agentFrame >= agentShotFrame - 2) {
			NkToasts().count = 0;
			NkToastPush(NkToastKind::Succes, "SONDE : succes");
			NkToastPush(NkToastKind::Partiel, "SONDE : avertissement");
			NkToastPush(NkToastKind::Refus, "SONDE : refus");
			if (agentFrame == agentShotFrame)
				std::printf("[sonde/toast] au declic : %d pastille(s) dans la pile\n",
							(int)NkToasts().count);
		}
		if (agentShotFrame > 0 && agentFrame == agentShotFrame)
			st.capturePending = 2; // « tutoriel » : toute la fenetre
		// NK_AGENT_POST="tonemap,bloom,ssao,fxaa" : eteint des passes de
		// post-traitement, une par une, sur la cible du viseur.
		//
		// ⚠️ POURQUOI CE LEVIER. Le BANC rend juste sur les quatre dorsaux, en
		// absolu ; le MODELEUR sort inverse sur DirectX. Meme dorsal, meme
		// generateur, meme chemin de soumission, meme camera. Trois differences
		// de chemin ont ete mesurees INNOCENTES : la surtaille de rendu, le fait
		// de rendre hors ecran, et l'ouverture d'un document. Il en reste une, et
		// le candidat est une PASSE PLEIN ECRAN : elle echantillonne la cible de
		// scene et la reecrit -- la famille exacte du defaut deja trouve deux fois
		// chez des consommateurs qu'on n'avait pas comptes, mais A L'INTERIEUR du
		// graphe, la ou aucun temoin ne regarde.
		//
		// ⚠️ UNE PAR UNE, JAMAIS PAR MOITIES : une comparaison qui change deux
		// variables ne prouve rien. La liste vide (NK_AGENT_POST="") n'eteint
		// rien ; « tout » les eteint toutes, et ne sert qu'a savoir si la FAMILLE
		// est en cause avant de chercher LAQUELLE.
		{
			static bool sPostFait = false;
			if (!sPostFait && demo::Demo3DHostReady()) {
				if (const char *pv = std::getenv("NK_AGENT_POST")) {
					sPostFait = true;
					const bool tout = std::strstr(pv, "tout") != nullptr;
					const bool tm = !(tout || std::strstr(pv, "tonemap"));
					const bool bl = !(tout || std::strstr(pv, "bloom"));
					const bool ao = !(tout || std::strstr(pv, "ssao"));
					const bool fx = !(tout || std::strstr(pv, "fxaa"));
					const bool ok = demo::Demo3DHostSetPost(tm, bl, ao, fx);
					std::printf("[nk3d] NK_AGENT_POST=%s : tonemap=%d bloom=%d ssao=%d"
								" fxaa=%d -> %s\n", pv, tm ? 1 : 0, bl ? 1 : 0, ao ? 1 : 0,
								fx ? 1 : 0, ok ? "applique" : "REFUSE");
				}
			}
		}
		// NK_AGENT_VUE=<n> : a la trame n, sauve LE CONTENU DE LA CIBLE HORS
		// ECRAN par Demo3DHostCaptureView -- la MEME cible que le GUI
		// echantillonne pour afficher le viseur, sans interface par-dessus.
		//
		// ⚠️ POURQUOI PAS st.capturePending. Le mode 1 preferait
		// Demo3DHostRenderOutputAs, qui REFAIT un rendu aux reglages de sortie :
		// on mesurerait un second chemin au lieu de la cible affichee. Ici on
		// veut FIGER la derniere image rendue, telle qu'elle est stockee.
		//
		// ⚠️ ET CE QUE LE FICHIER CONTIENT N'EST PAS LE CONTENU BRUT :
		// NkOffscreenTarget::ReadbackPixels retourne les lignes POUR OPENGL
		// SEULEMENT (origine framebuffer en bas-gauche). Qui compare ces PNG
		// entre dorsaux doit DEFAIRE ce retournement sur OpenGL, sinon il mesure
		// la convention de relecture au lieu du contenu ecrit.
		{
			static int32 sVueFrame = -2;
			if (sVueFrame == -2) {
				const char *v = std::getenv("NK_AGENT_VUE");
				sVueFrame = v ? (int32)std::atoi(v) : -1;
			}
			if (sVueFrame > 0 && agentFrame == sVueFrame && demo::Demo3DHostReady()) {
				char vuePath[256];
				if (NkNextCapturePath("cible", vuePath, (int32)sizeof(vuePath))) {
					const bool ok = demo::Demo3DHostCaptureView(vuePath);
					std::printf("[nk3d] NK_AGENT_VUE : contenu de la cible -> %s : %s\n",
								vuePath, ok ? "ecrit" : "ECHEC");
				}
			}
		}
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
		// NK_PICKER_IMPORT=<n> : a la frame n, OUVRE le selecteur d'import par le
		// MEME point de passage que le bouton « Importer » du navigateur
		// (`nk3d::NkPickerOuvrirImport`) -- filtres nommes compris. Aucune
		// injection d'entree : on arme l'etat que le clic arme, rien de plus.
		// PERIMETRE, dit ici : il prouve que le selecteur S'OUVRE et CE QU'IL
		// dessine ; il ne prouve pas que le BOUTON y mene -- ça reste un clic
		// humain a faire.
		{
			static int32 sPickFrame = -2;
			if (sPickFrame == -2) {
				const char *v = std::getenv("NK_PICKER_IMPORT");
				sPickFrame = v ? (int32)std::atoi(v) : -1;
			}
			if (sPickFrame > 0 && agentFrame == sPickFrame) {
				nk3d::NkPickerOuvrirImport(st);
				st.pickerAction = 2;
			}
		}
		// NK_GENIA_IMAGE=<chemin> : la generation + import par le MEME chemin que
		// la confirmation du picker « Generer » (nk3d::NkGeniaImporterImage) --
		// pour rejouer sans main. Appliquee UNE fois, hote pret, frame 10, comme
		// NK_IMPORT_FILE. PERIMETRE, dit ici : couvre generateur -> glTF ->
		// charge -> decoupe -> creation -> archivage ; ne couvre NI le bouton
		// NI le picker -- une relecture a la main reste necessaire pour eux.
		// NK_GENIA_PROJET=<dossier parent> : CREE un projet JETABLE
		// `<parent>/GeniaTemoin/` a la frame 3, hote pret, par LE MEME appel que
		// la boite « Nouveau projet » (NkProjectCreate, NkModelerWelcome.h cas 6),
		// puis rejoue `opened()` SANS `rec.Touch` : un temoin ne s'inscrit pas dans
		// les recents de Rodolf, et n'ecrit jamais dans un de ses projets --
		// c'est pourquoi NK_OPEN_RECENT n'est pas utilise ici. L'import refuse
		// sans projet (NkImportCreate) : sans ce crochet, NK_GENIA_IMAGE ne
		// mesurerait que ce refus.
		{
			static bool sGeniaProjDone = false;
			if (!sGeniaProjDone && agentFrame >= 3 && demo::Demo3DHostReady()) {
				sGeniaProjDone = true;
				if (const char *v = std::getenv("NK_GENIA_PROJET")) {
					NkString errP;
					const bool dejaOuvert = proj.open;
					const bool okP = !dejaOuvert && nk3d::NkProjectCreate(v, "GeniaTemoin", proj, &errP);
					if (okP) {
						st.welcome = false;
						st.newProjOpen = false;
						st.projError[0] = 0;
						nk3d::NkClearDirty(st);
					}
					nkentseu::NkLog::Instance().Infof("[genia] MESURE projet jetable : '%s/GeniaTemoin' -> %s%s", v,
													  okP ? "cree" : "REFUSE : ",
													  okP ? "" : (dejaOuvert ? "un projet est deja ouvert" : errP.CStr()));
				}
			}
		}
		{
			static bool sAgentGeniaDone = false;
			if (!sAgentGeniaDone && agentFrame >= 10 && demo::Demo3DHostReady()) {
				sAgentGeniaDone = true;
				if (const char *v = std::getenv("NK_GENIA_IMAGE")) {
					const int32 avant = st.BrowserCount();
					const bool ok = nk3d::NkGeniaImporterImage(st, v);
					nkentseu::NkLog::Instance().Infof("[genia] MESURE crochet : '%s' -> %s, %d carte(s) nee(s)", v,
													  ok ? "importe" : "REFUSE", st.BrowserCount() - avant);
				}
			}
		}
		{
			static bool sAgentImportDone = false;
			if (!sAgentImportDone && agentFrame >= 10 && demo::Demo3DHostReady()) {
				sAgentImportDone = true;
				// NK_IMPORT_FILE : UN chemin, ou PLUSIEURS separes par `;`.
				// C'est le temoin de la porte de liste (Rodolf, 06/09, point ⑤)
				// et celui des PLAFONDS (point ④) : `NK_IMPORT_REPEAT=<n>`
				// rejoue la meme liste n fois, jusqu'a ce que la chaine dise
				// non -- et le journal dit alors OU elle a dit non.
				if (const char *v = std::getenv("NK_IMPORT_FILE")) {
					char buf[NkModelerState::kMaxOsDrop * 512];
					snprintf(buf, sizeof(buf), "%s", v);
					const char *ptrs[NkModelerState::kMaxOsDrop];
					int32 n = 0;
					char *p = buf;
					while (*p && n < (int32)NkModelerState::kMaxOsDrop) {
						ptrs[n++] = p;
						while (*p && *p != ';')
							++p;
						if (*p == ';')
							*p++ = 0;
					}
					int32 tours = 1;
					if (const char *r = std::getenv("NK_IMPORT_REPEAT")) {
						tours = (int32)std::atoi(r);
						if (tours < 1)
							tours = 1;
					}
					for (int32 t = 0; t < tours; ++t) {
						const int32 avant = st.BrowserCount();
						const int32 ok = nk3d::NkImportFiles(st, ptrs, n);
						nkentseu::NkLog::Instance().Infof(
							"[import] TEMOIN plafond : tour %d/%d -> %d fichier(s) aboutis, "
							"cartes %d -> %d",
							t + 1, tours, ok, avant, st.BrowserCount());
						if (ok == 0)
							break; // la chaine a dit non : le refus est deja nomme
					}
				}
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
						if (ci >= 0 && ci < st.BrowserCount()) {
							st.dropIdx = ci;
							st.dropKind = st.Card(ci).kind;
							st.dropSrcNode = st.Card(ci).srcNode;
							st.dropMat = st.Card(ci).mat;
							snprintf(st.dropName, sizeof(st.dropName), "%s",
									 st.Card(ci).name);
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
								"[nk3d] MESURE jeton : {0} cartes\n", st.BrowserCount());
							for (int32 bi = 0; bi < st.BrowserCount(); ++bi)
								nkentseu::NkLog::Instance().Info(
									"[nk3d]   carte {0} « {1} » nature={2} srcNode={3} "
									"mat={4} parent={5}\n",
									bi, st.Card(bi).name, (int32)st.Card(bi).kind,
									st.Card(bi).srcNode, st.Card(bi).mat,
									st.Card(bi).parent);
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
					if (carte >= 0 && carte < st.BrowserCount()) {
						st.dropIdx = carte;
						st.dropKind = st.Card(carte).kind;
						st.dropSrcNode = st.Card(carte).srcNode;
						st.dropMat = st.Card(carte).mat;
						snprintf(st.dropName, sizeof(st.dropName), "%s", st.Card(carte).name);
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
				// SONDE : destination EXPLICITE, hors du dossier de sortie de Rodolf --
				// une image de sonde rangee parmi ses rendus finirait par etre prise
				// pour l'un d'eux.
				const char *probeOut = std::getenv("NK_TOAST_PROBE");
				const bool okPath2 =
					(probeOut && probeOut[0] && probeOut[0] != '1')
						? (snprintf(capPath, sizeof(capPath), "%s", probeOut) > 0)
						: demo::Demo3DHostReady()
							? demo::Demo3DHostOutNextPath(capPath, (int32)sizeof(capPath), 2)
							: NkNextCapturePath("tutoriel", capPath, (int32)sizeof(capPath));
				if (okPath2) {
					const bool okCap = NkCaptureWholeWindow(window, capPath);
					// MEME RESOLUTION DE SORTIE que le rendu : la fenetre est
					// photographiee a sa taille -- c'est sa nature -- puis
					// ramenee au format demande. Sans cela, « tutoriel » etait
					// le seul des trois a ignorer les reglages (Rihen).
					// SONDE : PAS DE REDIMENSIONNEMENT. Un bicubique melange les pixels
					// voisins -- la couleur mesuree ne serait plus celle qui a ete
					// peinte, mais une moyenne. On mesure l'image telle qu'elle sort.
					if (okCap && !std::getenv("NK_TOAST_PROBE") && demo::Demo3DHostReady()) {
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
				for (int32 b = 0; b < st.BrowserCount(); ++b)
					if (st.Card(b).kind == 2 && st.Card(b).mat == mDirty + 1) {
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
					// NK_AGENT_MATPREV : on ECRIT ces pixels, faute de quoi
					// l'orientation de la cible d'apercu reste inconnue.
					// ⚠️ Demo3DHostMatThumbRequest prend un `cheminPng` et le JETTE
					// -- « (void)cheminPng; la vignette ne va plus dans un fichier
					// voisin » -- alors que sa documentation annonce l'inverse.
					// C'est le SEUL endroit ou ces pixels existent cote application.
					if (std::getenv("NK_AGENT_MATPREV")) {
						char mpp[256];
						if (NkNextCapturePath("matprev", mpp, (int32)sizeof(mpp))) {
							NkImage vig;
							if (vig.Create((uint32)cote, (uint32)cote, math::NkColor(0, 0, 0, 255), 4)) {
								memcpy(vig.Pixels(), frais, (size_t)cote * (size_t)cote * 4u);
								const bool okv = vig.Save(mpp);
								std::printf("[nk3d] NK_AGENT_MATPREV : vignette %d, %dx%d -> %s : %s\n",
											i, cote, cote, mpp, okv ? "ecrite" : "ECHEC");
							}
						}
					}
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
