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

inline void CmdToggleTabRows(void *) { // Affichage: onglets multi-rangees (option VS)
	nkcode::NkCodeTabRowsOn() = !nkcode::NkCodeTabRowsOn();
}

inline void CmdToggleMinimap(void *) { // Affichage: minimap on/off (aussi Ctrl+Maj+AntiSlash dans l editeur)
	nkcode::NkCodeMinimapOn() = !nkcode::NkCodeMinimapOn();
}

inline void CmdQuit(void *user) {
	if (user)
		static_cast<NkEditorShell *>(user)->RequestClose();
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

inline void OverlayThunk(NkEditorFrameContext &ec, void *u) {
	auto *d = static_cast<nkcode::NkCodeDialogs *>(u);
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
