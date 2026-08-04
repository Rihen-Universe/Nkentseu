#pragma once
// =============================================================================
// NkAppCommands.h — Commandes de la palette, raccourcis globaux et THUNKS de
// l'application (activity bar, toolbar, menus, overlay, zoom, exclusivité des
// sidebars, thème). Extrait de main.cpp (modularisation) : main ne fait plus
// que l'orchestration.
// =============================================================================
#include "NKCode/Shell/Panels.h"
#include "NKCode/Shell/Toolbar.h"
#include "NKCode/Shell/Dialogs.h"
#include "NKCode/Shell/NkHome.h"

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu::editorkit;

// ── Commandes (NkEditorCommandFn = void(*)(void*)) — user = NkCodeState* ──
inline void CmdBuild(void *u) {
	if (u)
		static_cast<NkCodeState *>(u)->DoBuildAction("build");
}

inline void CmdRun(void *u) {
	if (u)
		static_cast<NkCodeState *>(u)->DoRun();
}

inline void CmdSave(void *user) {
	if (user)
		static_cast<NkCodeState *>(user)->SaveActive();
}

// « Enregistrer sous » — user = NkCodeDialogs* (pas juste l'etat : ouvre LE
// PICKER, cf. Dialogs.h::OpenSaveAs). Le menu affichait deja le raccourci
// "Ctrl+Shift+S" en LIBELLE mais rien ne l'enregistrait reellement aupres du
// shell -> la touche ne faisait rien (issue beta #5).
inline void CmdSaveAs(void *user) {
	if (user)
		static_cast<nkcode::NkCodeDialogs *>(user)->OpenSaveAs();
}

inline void CmdFormat(void *u) { // Formater le document actif (C/C++)
	auto *st = static_cast<NkCodeState *>(u);
	if (st && st->active >= 0 && st->active < static_cast<int32>(st->files.Size()))
		st->files[st->active].doc.FormatCpp();
}

// ── Activity bar -> SIDEBARS EXCLUSIVES (facon VSCode) : 0..6 = vues gauche,
//    100..102 = IA (panneau droit), 999 = Preferences. ──
inline void ActivityThunk(void *user, int32 idx) {
	auto *sh = static_cast<editorkit::NkEditorShell *>(user);
	int32 gN = 0;
	if (idx >= 0 && idx < 7) {
		static const char *kByIdx[7] = {"Explorateur", "Recherche", "Controle de version", "Debogueur", "Live Collab",
										"Extensions",  "Profiler"};
		const char *const *g = nkcode::SideLeftGroup(gN);
		nkcode::ToggleSideExclusive(sh, g, gN, kByIdx[idx]);
	} else if (idx >= 100 && idx <= 103) {
		static const char *kAi[4] = {"Claude Code", "Codex", "Assistant IA", "NkAI"};
		const char *const *g = nkcode::SideRightGroup(gN);
		nkcode::ToggleSideExclusive(sh, g, gN, kAi[idx - 100]);
	} else if (idx == 999)
		sh->OpenPreferences();
}

// ── Ctrl+Shift+A : bascule le panneau « Assistant IA » (Section 6.4 du spec Banani).
//    Réutilise EXACTEMENT le chemin de l'icône activity bar idx==102 (ActivityThunk) —
//    même groupe exclusif (SideRightGroup), même ToggleSideExclusive. ──
inline void CmdToggleAiPanel(void *user) {
	auto *sh = static_cast<editorkit::NkEditorShell *>(user);
	int32 gN = 0;
	const char *const *g = nkcode::SideRightGroup(gN);
	nkcode::ToggleSideExclusive(sh, g, gN, "Assistant IA");
}

inline void CmdToggleTabRows(void *) { // Affichage: onglets multi-rangees (option VS)
	nkcode::NkCodeTabRowsOn() = !nkcode::NkCodeTabRowsOn();
}

inline void CmdToggleMinimap(void *) { // Affichage: minimap on/off (aussi Ctrl+Maj+AntiSlash dans l editeur)
	nkcode::NkCodeMinimapOn() = !nkcode::NkCodeMinimapOn();
}

inline void CmdQuit(void *user) {
	if (user)
		static_cast<NkEditorShell *>(user)->RequestQuit(/*windowClose=*/false); // Ctrl+Q : quitter (confirmation possible)
}

inline void CmdResetLayout(void *u) {
	if (u)
		static_cast<NkEditorShell *>(u)->ResetLayout();
}

// ── Zoom PAR ONGLET : le shell route Ctrl+molette / Ctrl+± / Ctrl+0 ici. On ajuste la
//    taille PROPRE de l'onglet actif (0 = taille globale). L'atlas suit via RequestCodeSize.
struct ZoomCtx {
		nkcode::NkCodeState *st;
		NkEditorShell *shell;
};

inline ZoomCtx &NkZoomCtx() {
	static ZoomCtx z;
	return z;
}

inline void ZoomHandler(void *u, nkentseu::float32 delta, bool reset) {
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
inline void ToolbarThunk(NkEditorFrameContext &ec, void *u) {
	nkcode::DrawCodeToolbar(ec, static_cast<nkcode::NkCodeState *>(u));
}


inline void FileMenuThunk(NkEditorFrameContext &ec, void *u) {
	nkcode::DrawFileMenu(ec, static_cast<nkcode::NkCodeDialogs *>(u));
}

// ── Fenetre modale PREFERENCES : heberge le panneau de parametres COMPLET du
//    launcher (NkSettingsPanel — General/Chemins/Jenga/Theme/Git/Comptes/
//    Raccourcis) au-dessus de l'editeur. Parite launcher demandee par Rihen. ──
inline void DrawPrefsModal(NkEditorFrameContext &ec, nkcode::NkCodeDialogs *d) {
	nkcode::NkHomeState *H = d->home;
	if (!H)
		return;
	nkcode::NkUi u = nkcode::NkUi::From(ec, /*overlay=*/true);
	if (!u.Valid())
		return;
	NkGuiContext &ctx = *u.ctx;
	const float32 W = (float32)ctx.viewW, Vh = (float32)ctx.viewH;
	// 88% du viewport, borne : assez grand pour le sous-sidebar categories + pages.
	float32 pw = W * 0.88f, ph = Vh * 0.88f;
	if (pw > u.s(1080))
		pw = u.s(1080);
	if (ph > u.s(760))
		ph = u.s(760);
	const NkRect box = {(W - pw) * 0.5f, (Vh - ph) * 0.5f, pw, ph};
	// Routeur d'occlusion : la modale COUVRE tout (couche 100) ; ses propres
	// hit-tests passent grace au scope.
	ctx.PushOcclusion(box, 100);
	NkGuiContext::NkInputLayerScope _layer(ctx, 100);
	// MODALITE GLOBALE (meme mecanisme que NkModalDraw) : popupDepth > 0 est
	// LE signal verifie par les points d'interaction des panneaux NKCode —
	// sans lui les clics TRAVERSENT vers l'editeur/l'explorateur derriere.
	// popupRects[0] doit pointer la modale, sinon Update() (debut de frame)
	// retombe a 0 au premier clic et un panneau traite avant nous reagit.
	if (ctx.popupDepth == 0)
		ctx.popupDepth = 1;
	ctx.popupRects[0] = box;
	ctx.popupAnchor = box;
	u.Rect({0.f, 0.f, W, Vh}, NkColor{0, 0, 0, 150}); // backdrop
	// Clic hors fenetre = fermer (sauf si le picker des settings est ouvert).
	if (u.click && !u.Hit(box) && !d->pickerOpen) {
		d->showPrefs = false;
		ctx.popupDepth = 0; // libere la modalite
		return;
	}
	u.Panel(box, nkcode::NkCol::background, nkcode::NkCol::border, nkcode::NkR::lg);
	int32 navDummy = -1; // navOut inutilise par NkSettingsPanel ((void)navOut)
	if (nkcode::NkSettingsPanel(u, box, &H->settings, H->st, d, ec.dt, H->icons, &navDummy) == 1) {
		d->showPrefs = false; // Echap (champ/combo deja defocalises) = fermer
		ctx.popupDepth = 0;
		return;
	}
	ctx.appModal = true; // neutralise raccourcis/edition derriere la modale
}

// ── Fenetre modale NOUVEAU WORKSPACE : heberge le wizard COMPLET du launcher
//    (NkNewWsPanel — templates, configs, OS, archs, toolchain, projets, git).
//    A la creation, DoLoad est intercepte (wsAddAsRoot) : le workspace est
//    AJOUTE comme racine de l'explorateur, le courant reste charge. ──
inline void DrawNewWsModal(NkEditorFrameContext &ec, nkcode::NkCodeDialogs *d) {
	nkcode::NkHomeState *H = d->home;
	if (!H)
		return;
	nkcode::NkUi u = nkcode::NkUi::From(ec, /*overlay=*/true);
	if (!u.Valid())
		return;
	NkGuiContext &ctx = *u.ctx;
	const float32 W = (float32)ctx.viewW, Vh = (float32)ctx.viewH;
	float32 pw = W * 0.9f, ph = Vh * 0.9f;
	if (pw > u.s(1150))
		pw = u.s(1150);
	if (ph > u.s(800))
		ph = u.s(800);
	const NkRect box = {(W - pw) * 0.5f, (Vh - ph) * 0.5f, pw, ph};
	ctx.PushOcclusion(box, 100);
	NkGuiContext::NkInputLayerScope _layer(ctx, 100);
	// MODALITE GLOBALE (cf. DrawPrefsModal) : popupDepth + popupRects[0].
	if (ctx.popupDepth == 0)
		ctx.popupDepth = 1;
	ctx.popupRects[0] = box;
	ctx.popupAnchor = box;
	u.Rect({0.f, 0.f, W, Vh}, NkColor{0, 0, 0, 150}); // backdrop
	if (u.click && !u.Hit(box) && !d->pickerOpen) {	  // clic dehors = annuler
		d->showNewWs = false;
		d->wsAddAsRoot = false;
		ctx.popupDepth = 0;
		return;
	}
	u.Panel(box, nkcode::NkCol::background, nkcode::NkCol::border, nkcode::NkR::lg);
	if (nkcode::NkNewWsPanel(u, box, &H->nw, H->st, d, ec.dt, H->icons) == 1) {
		d->showNewWs = false; // Annuler du wizard
		d->wsAddAsRoot = false;
		ctx.popupDepth = 0;
		return;
	}
	if (!d->wsAddAsRoot) {
		// DoLoad intercepte -> creation TERMINEE (workspace ajoute en racine).
		d->showNewWs = false;
		ctx.popupDepth = 0;
		return;
	}
	ctx.appModal = true;
}

// ── Fenetre DEDIEE d'aide : Raccourcis clavier (1) / A propos (2). Contenu
//    statique reel (les raccourcis listes = ceux effectivement cables). ──
inline void DrawHelpModal(NkEditorFrameContext &ec, nkcode::NkCodeDialogs *d) {
	nkcode::NkUi u = nkcode::NkUi::From(ec, /*overlay=*/true);
	if (!u.Valid())
		return;
	NkGuiContext &ctx = *u.ctx;
	const float32 W = (float32)ctx.viewW, Vh = (float32)ctx.viewH;
	const bool about = (d->showHelp == 2);
	float32 pw = u.s(about ? 520.f : 680.f), ph = Vh * 0.85f;
	if (about && ph > u.s(360))
		ph = u.s(360);
	if (!about && ph > u.s(700))
		ph = u.s(700);
	const NkRect box = {(W - pw) * 0.5f, (Vh - ph) * 0.5f, pw, ph};
	ctx.PushOcclusion(box, 100);
	NkGuiContext::NkInputLayerScope _layer(ctx, 100);
	// MODALITE GLOBALE (cf. DrawPrefsModal) : popupDepth + popupRects[0] —
	// sans eux, les clics TRAVERSENT vers les panneaux derriere la fenetre.
	if (ctx.popupDepth == 0)
		ctx.popupDepth = 1;
	ctx.popupRects[0] = box;
	ctx.popupAnchor = box;
	u.Rect({0.f, 0.f, W, Vh}, NkColor{0, 0, 0, 150});
	if ((u.click && !u.Hit(box)) || ctx.input.KeyPressed(NkGuiKey::Escape)) {
		d->showHelp = 0;
		d->helpScroll = 0.f;
		ctx.popupDepth = 0;
		return;
	}
	u.Panel(box, nkcode::NkCol::background, nkcode::NkCol::border, nkcode::NkR::lg);
	// Titre + lisere accent + bouton fermer.
	const float32 tbH = u.s(44.f);
	u.Rect({box.x, box.y, box.w, u.s(3.f)}, nkcode::NkCol::primary, nkcode::NkR::lg);
	u.TextV(box.x + u.s(20), box.y, tbH, about ? NkT("mb.help.about") : NkT("mb.help.shortcuts"),
			nkcode::NkCol::foreground);
	{
		const NkRect xr = {box.x + box.w - u.s(36), box.y + u.s(8), u.s(28), u.s(28)};
		if (u.Button(xr, "x", NkColor{0, 0, 0, 0}, nkcode::NkColHover(nkcode::NkCol::sidebar),
					 nkcode::NkCol::foreground, u.s(6))) {
			d->showHelp = 0;
			d->helpScroll = 0.f;
			ctx.popupDepth = 0;
			return;
		}
	}
	const NkRect body = {box.x + u.s(24), box.y + tbH + u.s(6), box.w - u.s(48), box.h - tbH - u.s(30)};
	if (about) {
		// ── A PROPOS : identite produit/editeur, contact. ──
		const NkString titleLine = NkString("NKCode ") + nkcode::NkCodeVersion(); // source unique (NkUi.h)
		const char *L[] = {titleLine.CStr(),
						   "",
						   "IDE natif de l'ecosysteme Nkentseu (NKGui).",
						   "Builds pilotes par Jenga (C, C++, ASM, Rust, Zig).",
						   "",
						   "Editeur : Rihen",
						   "Contact : rihen.universe@gmail.com",
						   "Depot beta : github.com/Rihen-Universe/NKCode-Beta"};
		float32 y = body.y + u.s(6);
		for (int32 i = 0; i < 8; ++i) {
			u.Text(body.x, y, L[i], i == 0 ? nkcode::NkCol::foreground : nkcode::NkCol::sidebarFg);
			y += u.Lh() + u.s(8);
		}
		return;
	}
	// ── RACCOURCIS : (touche, action) — la liste reflete les cablages REELS. ──
	struct Sc {
			const char *k, *a;
	};
	static const Sc SC[] = {{"Ctrl+N / Ctrl+S / Ctrl+Shift+S", "Nouveau fichier / Enregistrer / Enregistrer sous"},
							{"Ctrl+Q", "Quitter"},
							{"Ctrl+P", "Palette de commandes"},
							{"Ctrl+Shift+A", "Panneau Assistant IA"},
							{"", ""},
							{"Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y", "Annuler / Retablir"},
							{"Ctrl+X / Ctrl+C / Ctrl+V / Ctrl+A", "Couper / Copier / Coller / Tout selectionner"},
							{"Ctrl+D", "Dupliquer la ligne ou la selection"},
							{"Ctrl+L", "Selectionner la ligne (etend au repete)"},
							{"Ctrl+Shift+D", "Occurrence suivante (multi-curseur)"},
							{"Ctrl+Shift+L", "Toutes les occurrences (multi-curseur)"},
							{"Ctrl+Shift+K", "Supprimer la ligne"},
							{"Alt+\xE2\x86\x91 / Alt+\xE2\x86\x93", "Deplacer la ligne"},
							{"Shift+Alt+\xE2\x86\x91 / Shift+Alt+\xE2\x86\x93", "Dupliquer la ligne (haut/bas)"},
							{"Ctrl+Entree / Ctrl+Shift+Entree", "Inserer une ligne dessous / dessus"},
							{"Ctrl+/ et Ctrl+Shift+/", "Commentaire ligne / bloc (selon langage)"},
							{"Ctrl+] / Ctrl+[", "Indenter / Desindenter"},
							{"", ""},
							{"Ctrl+F / Ctrl+H", "Rechercher / Remplacer (fichier)"},
							{"F3 / Shift+F3", "Occurrence suivante / precedente"},
							{"Ctrl+Shift+F / Ctrl+Shift+H", "Rechercher / Remplacer (workspace)"},
							{"Ctrl+G", "Aller a la ligne"},
							{"F2", "Renommer l'identifiant"},
							{"F8 / Shift+F8", "Probleme suivant / precedent"},
							{"", ""},
							{"F9", "Point d'arret (gouttiere)"},
							{"F5", "Deboguer (jenga gdb, points d'arret transmis)"},
							{"Alt+Z", "Retour a la ligne (word wrap)"},
							{"Ctrl+molette / Ctrl+\xC2\xB1 / Ctrl+0", "Zoom du code (par onglet)"},
							{"Ctrl+K Ctrl+0 / Ctrl+K Ctrl+J", "Replier / Deplier tout"},
							{"F1", "Documentation"}};
	const int32 N = (int32)(sizeof(SC) / sizeof(SC[0]));
	const float32 rowH = u.Lh() + u.s(10);
	const float32 contentH = N * rowH;
	if (u.Hit(body) && ctx.input.wheel != 0.f) {
		d->helpScroll -= ctx.input.wheel * rowH;
		ctx.input.wheel = 0.f;
	}
	const float32 maxS = contentH - body.h > 0.f ? contentH - body.h : 0.f;
	if (d->helpScroll < 0.f)
		d->helpScroll = 0.f;
	if (d->helpScroll > maxS)
		d->helpScroll = maxS;
	u.dl->PushClipRect(body, true);
	float32 y = body.y - d->helpScroll;
	const float32 keyW = u.s(280.f);
	for (int32 i = 0; i < N; ++i) {
		if (y + rowH >= body.y && y <= body.y + body.h && SC[i].k[0]) {
			u.Text(body.x, y + u.s(4), SC[i].k, nkcode::NkCol::primary);
			u.Text(body.x + keyW, y + u.s(4), SC[i].a, nkcode::NkCol::foreground);
		}
		y += SC[i].k[0] ? rowH : u.s(10.f);
	}
	u.dl->PopClipRect();
	if (maxS > 0.f) { // barre de defilement fine
		const float32 th = body.h * (body.h / contentH);
		const float32 ty = body.y + (body.h - th) * (d->helpScroll / maxS);
		u.Rect({body.x + body.w + u.s(10), ty, u.s(4), th}, nkcode::NkCol::border, u.s(2));
	}
	ctx.appModal = true;
}

inline void OverlayThunk(NkEditorFrameContext &ec, void *u) {
	auto *d = static_cast<nkcode::NkCodeDialogs *>(u);
	// Modales AVANT DrawOverlay : le picker (ouvert par un champ des
	// settings/du wizard) doit se dessiner AU-DESSUS de la modale.
	if (d->showPrefs)
		DrawPrefsModal(ec, d);
	if (d->showNewWs)
		DrawNewWsModal(ec, d);
	if (d->showHelp)
		DrawHelpModal(ec, d);
	// Demande d'« Enregistrer sous » (bouton +, ré-enregistrer un fichier supprimé) : ouvre le dialogue.
	if (d->st && d->st->reqSaveAs) {
		d->st->reqSaveAs = false;
		d->SaveActiveNative();
	}
	// « Ajouter un dossier au workspace » (explorateur) : réutilise LE picker de
	// l'app (Dialogs.h, mode PK_PickFolder — dossier quelconque). Un seul picker.
	if (d->st && d->st->reqPickFolder) {
		d->st->reqPickFolder = false;
		d->OpenPicker(nkcode::NkCodeDialogs::PK_PickFolder, d->st->root.ToString().CStr());
	}
	nkcode::DrawOverlay(ec, d);
}


inline void StartScreenThunk(NkEditorFrameContext &ec, void *u) {
	nkcode::DrawHome(ec, static_cast<nkcode::NkHomeState *>(u));
}

// Pose appFullScreen/appModal CHAQUE FRAME (barre de menus, inconditionnel).
// + synchronise le thème de l'ÉDITORKIT sur le thème NKCode : le chrome de l'éditeur
//   (barre de titre, activity bar, onglets, dock, status bar, panneaux) suit Dark/Light.
// Garantit « au plus UN panneau du groupe ouvert PAR FEUILLE » (une disposition
// restaurée peut en rouvrir plusieurs au même endroit) : le premier reste, les
// surnuméraires sont fermés ET détachés. Un panneau déplacé dans une AUTRE feuille
// (indépendant) n'est pas touché.
inline void EnforceExclusiveSides(editorkit::NkEditorShell *sh) {
	if (!sh)
		return;
	for (int32 side = 0; side < 2; ++side) {
		int32 n = 0;
		const char *const *g = side == 0 ? nkcode::SideLeftGroup(n) : nkcode::SideRightGroup(n);
		for (int32 i = 0; i < n; ++i) {
			if (!sh->IsPanelOpen(g[i]))
				continue;
			const int32 node = sh->PanelDockNode(g[i]);
			if (node < 0)
				continue;
			for (int32 j = i + 1; j < n; ++j)
				if (sh->IsPanelOpen(g[j]) && sh->PanelDockNode(g[j]) == node) {
					sh->ClosePanel(g[j]);
					sh->DetachPanel(g[j]);
				}
		}
	}
}

// Icônes marquées des activity bars = l'état RÉEL des panneaux (l'index cliqué ne
// suffit pas : une disposition restaurée ouvre des panneaux sans passer par un clic).
inline void SyncActivityMarkers(editorkit::NkEditorShell *sh) {
	if (!sh)
		return;
	static const char *kLeft[7] = {"Explorateur", "Recherche",	"Controle de version", "Debogueur",
								   "Live Collab", "Extensions", "Profiler"};
	static const char *kAi[4] = {"Claude Code", "Codex", "Assistant IA", "NkAI"};
	int32 left = -1, right = -1;
	for (int32 i = 0; i < 7 && left < 0; ++i)
		if (sh->IsPanelOpen(kLeft[i]))
			left = i;
	for (int32 i = 0; i < 4 && right < 0; ++i)
		if (sh->IsPanelOpen(kAi[i]))
			right = 100 + i;
	sh->SetActivityActive(left, right);
}

inline void AppFlagsThunk(NkEditorFrameContext &ec, void *u) { // user = NkHomeState*
	auto *home = static_cast<NkHomeState *>(u);
	if (!home || !home->dlg)
		return;
	nkcode::DrawAppFlags(ec, home->dlg);
		// Entree dans la barre « Recherche rapide » (toolbar) -> ouvre le panneau
		// Recherche, y recopie la requete (wsPrefill deja pose par la toolbar) et
		// LANCE la recherche projet. Le champ du panneau prend le focus (Ctrl+Maj+F-like).
		if (home->dlg->st && home->dlg->st->reqSearch) {
			auto *st = home->dlg->st;
			st->reqSearch = false;
			if (home->dlg->shell)
				home->dlg->shell->FocusPanel("Recherche");
			// Si aucune requete fournie (cas clic sans texte), reprend la selection editeur.
			if (st->wsPrefill.Empty() && st->HasActive() && st->files[st->active].doc.HasSel())
				st->wsPrefill = st->files[st->active].doc.GetSelectedText();
			st->wsFocusField = 1;
			st->wsFocusReq = true;
			if (!st->wsPrefill.Empty())
				st->StartWsFind(st->wsPrefill, false, false); // resultats immediats
		}
	{ // audio : arret auto quand on quitte l'onglet (option) — par onglet actif
		auto *st2 = home->dlg->st;
		const NkString ap = (st2 && st2->HasActive()) ? st2->files[st2->active].path.ToString() : NkString();
		nkcode::NkAudioStopInactive(ap.CStr());
	}
	EnforceExclusiveSides(home->dlg->shell); // sidebars exclusives
	SyncActivityMarkers(home->dlg->shell);   // marqueurs = état réel
	// Drop OS non consommé (zone morte) : purge après quelques frames.
	if (home->dlg->st && home->dlg->st->osDropTtl > 0 && --home->dlg->st->osDropTtl == 0)
		home->dlg->st->osDropPaths.Clear();
	if (!home->settings.loaded)
		home->settings.Load();
	nkcode::NkApplyEditorTheme(ec.Ui(), home->settings.theme, home->settings.accent);
}

	} // namespace nkcode
} // namespace nkentseu
