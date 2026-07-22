#pragma once
// =============================================================================
// NkMenuBar.h — BARRE DE MENUS PRINCIPALE de NKCode (spec Banani, 11 menus :
//   Fichier/Édition/Affichage/Aller/Exécuter/Déboguer/Git/IA/Outils/Fenêtre/
//   Aide). REMPLACE les menus par defaut du shell (via NkEditorShell::
//   SetMenuBar) — l'ancien systeme (menus en dur + SetFileMenu) reste dans le
//   moteur uniquement comme repli pour d'autres apps.
//
//   Regle d'HONNETETE : un item n'est CLIQUABLE que si son backend existe
//   reellement (cable ici sur NkCodeState/NkCodeDialogs/NkEditorShell) ;
//   sinon il est AFFICHE GRISE (enabled=false) — jamais un clic qui ne fait
//   rien. Les raccourcis affiches sont UNIQUEMENT ceux qui fonctionnent deja.
//   i18n : libelles via NkT (cles mb.*) ; les items grises pas encore
//   traduits gardent le libelle de la spec (passe 2 completera).
// =============================================================================
#include "NKCode/Shell/Dialogs.h"  // NkCodeDialogs (Open*, SaveActiveNative, ShowStart)
#include "NKCode/Shell/Panels.h"   // SideLeftGroup/OpenSideExclusive (Recherche)
#include "NKCode/Shell/NkI18n.h"   // NkT
#include "NKWindow/Core/NkLauncher.h" // OpenURL (Aide)

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu;
		using namespace nkentseu::nkgui;
		using namespace nkentseu::editorkit;

		// Pointeurs partages poses par main.cpp avant SetMenuBar.
		struct NkMenuBarCtx {
				NkCodeDialogs *dlg = nullptr;
				NkEditorShell *shell = nullptr;
		};

		inline void DrawMainMenuBar(NkEditorFrameContext &ec, NkMenuBarCtx *mb) {
			if (!mb || !mb->dlg || !mb->shell)
				return;
			auto &ctx = ec.Ui();
			NkCodeDialogs *d = mb->dlg;
			NkEditorShell *sh = mb->shell;
			NkCodeState *s = d->st;
			const bool hasWs = s && s->HasWorkspace();
			const bool hasFile = s && s->HasActive();
			NkCodeDoc *doc = hasFile ? &s->files[s->active].doc : nullptr;

			// ── FICHIER ──────────────────────────────────────────────────────
			if (BeginMenu(ctx, NkT("mb.file"))) {
				if (MenuItem(ctx, NkT("mb.file.startscreen")))
					d->ShowStart();
				Separator(ctx);
				if (MenuItem(ctx, NkT("mb.file.newfile"), "Ctrl+N") && s)
					s->NewFile();
				MenuItem(ctx, NkT("mb.file.newfolder"), nullptr, false); // (a venir : dossier via explorateur)
				if (MenuItem(ctx, NkT("mb.file.newproject"), nullptr, hasWs))
					d->Open(NkCodeDialogs::NewProject);
				if (MenuItem(ctx, NkT("mb.file.newworkspace")))
					d->Open(NkCodeDialogs::NewWorkspace);
				Separator(ctx);
				MenuItem(ctx, NkT("mb.file.openfile"), nullptr, false); // (a venir : dialogue fichier natif)
				if (MenuItem(ctx, NkT("mb.file.openfolder")))
					d->OpenFolderDialog();
				if (MenuItem(ctx, NkT("mb.file.openws")))
					d->OpenWorkspaceDialog();
				MenuItem(ctx, NkT("mb.file.recent"), nullptr, false); // (a venir : liste recents)
				Separator(ctx);
				if (MenuItem(ctx, NkT("mb.file.save"), "Ctrl+S", hasFile) && s) {
					if (s->ActiveHasPath())
						s->SaveActive();
					else
						d->SaveActiveNative();
				}
				if (MenuItem(ctx, NkT("mb.file.saveas"), "Ctrl+Shift+S", hasFile))
					d->SaveActiveNative();
				if (MenuItem(ctx, NkT("mb.file.saveall"), nullptr, hasFile) && s)
					s->SaveAll();
				MenuItem(ctx, NkT("mb.file.autosave"), nullptr, false); // (a venir)
				MenuItem(ctx, NkT("mb.file.revert"), nullptr, false);	// (a venir)
				Separator(ctx);
				MenuItem(ctx, NkT("mb.file.share"), nullptr, false);  // (a venir : Live Collab/Gist)
				MenuItem(ctx, NkT("mb.file.export"), nullptr, false); // (a venir : ZIP/tarball)
				Separator(ctx);
				if (MenuItem(ctx, NkT("mb.file.prefs")))
					sh->OpenPreferences(0);
				if (MenuItem(ctx, NkT("mb.file.quit"), "Ctrl+Q"))
					sh->RequestClose();
				EndMenu(ctx);
			}

			// ── ÉDITION ──────────────────────────────────────────────────────
			if (BeginMenu(ctx, NkT("mb.edit"))) {
				if (MenuItem(ctx, NkT("mb.edit.undo"), "Ctrl+Z", hasFile) && doc)
					doc->Undo();
				if (MenuItem(ctx, NkT("mb.edit.redo"), "Ctrl+Shift+Z", hasFile) && doc)
					doc->Redo();
				Separator(ctx);
				const bool hasSel = doc && doc->HasSel();
				if (MenuItem(ctx, NkT("mb.edit.cut"), "Ctrl+X", hasSel) && doc) {
					ctx.SetClipboard(doc->GetSelectedText().CStr());
					doc->Checkpoint(3);
					doc->EraseSelection();
				}
				if (MenuItem(ctx, NkT("mb.edit.copy"), "Ctrl+C", hasSel) && doc)
					ctx.SetClipboard(doc->GetSelectedText().CStr());
				if (MenuItem(ctx, NkT("mb.edit.paste"), "Ctrl+V", hasFile) && doc) {
					const NkString clip = ctx.GetClipboard();
					if (!clip.Empty()) {
						doc->Checkpoint(3);
						doc->InsertText(clip.CStr());
					}
				}
				if (MenuItem(ctx, NkT("mb.edit.selectall"), "Ctrl+A", hasFile) && doc) {
					doc->selLine = 0;
					doc->selCol = 0;
					doc->curLine = doc->LineCount() - 1;
					doc->curCol = doc->LineLen(doc->curLine);
				}
				Separator(ctx);
				if (MenuItem(ctx, NkT("mb.edit.find"), "Ctrl+F", hasFile) && doc) {
					doc->findOpen = true;
					doc->findBarActive = true;
				}
				if (MenuItem(ctx, NkT("mb.edit.replace"), "Ctrl+H", hasFile) && doc) {
					doc->findOpen = true;
					doc->findBarActive = true;
				}
				if (MenuItem(ctx, NkT("mb.edit.findfiles"), "Ctrl+Shift+F", hasWs) && s) {
					s->wsFocusField = 1;
					int32 gN = 0;
					const char *const *g = SideLeftGroup(gN);
					OpenSideExclusive(sh, g, gN, "Recherche");
					s->wsFocusReq = true;
				}
				if (MenuItem(ctx, NkT("mb.edit.replfiles"), "Ctrl+Shift+H", hasWs) && s) {
					s->wsFocusField = 2;
					int32 gN = 0;
					const char *const *g = SideLeftGroup(gN);
					OpenSideExclusive(sh, g, gN, "Recherche");
					s->wsFocusReq = true;
				}
				Separator(ctx);
				MenuItem(ctx, NkT("mb.edit.cursorabove"), nullptr, false); // (a venir : menu -> multi-curseur)
				MenuItem(ctx, NkT("mb.edit.cursorbelow"), nullptr, false);
				MenuItem(ctx, NkT("mb.edit.linecomment"), nullptr, false);	// (a venir depuis le menu)
				MenuItem(ctx, NkT("mb.edit.blockcomment"), nullptr, false);
				Separator(ctx);
				if (MenuItem(ctx, NkT("mb.edit.foldall"), "Ctrl+K Ctrl+0", hasFile) && doc)
					doc->FoldAll();
				if (MenuItem(ctx, NkT("mb.edit.unfoldall"), "Ctrl+K Ctrl+J", hasFile) && doc)
					doc->UnfoldAll();
				EndMenu(ctx);
			}

			// ── AFFICHAGE ────────────────────────────────────────────────────
			if (BeginMenu(ctx, NkT("mb.view"))) {
				if (MenuItem(ctx, NkT("mb.view.palette"), "Ctrl+P"))
					sh->OpenCommandPalette();
				if (BeginMenu(ctx, NkT("mb.view.openview"))) {
					sh->DrawPanelsMenuItems();
					EndMenu(ctx);
				}
				if (BeginMenu(ctx, NkT("mb.view.appearance"))) {
					if (MenuItem(ctx, NkT("mb.view.theme")))
						sh->OpenPreferences(1);
					if (MenuItem(ctx, NkT("mb.view.fonts")))
						sh->OpenPreferences(0);
					if (MenuItem(ctx, NkT("mb.view.langs")))
						sh->OpenPreferences(2);
					EndMenu(ctx);
				}
				Separator(ctx);
				if (MenuItem(ctx, NkT("mb.view.minimap")))
					NkCodeMinimapOn() = !NkCodeMinimapOn();
				if (MenuItem(ctx, NkT("mb.view.wordwrap"), "Alt+Z", hasFile) && doc)
					doc->wrapOn = !doc->wrapOn;
				MenuItem(ctx, "Breadcrumbs", nullptr, false);		 // (a venir)
				MenuItem(ctx, "Whitespace", nullptr, false);		 // (a venir)
				MenuItem(ctx, "Indentation Guides", nullptr, false); // (a venir)
				MenuItem(ctx, NkT("mb.view.zen"), nullptr, false);	 // (a venir)
				Separator(ctx);
				if (MenuItem(ctx, NkT("mb.view.resetlayout")))
					sh->ResetLayout();
				EndMenu(ctx);
			}

			// ── ALLER ────────────────────────────────────────────────────────
			if (BeginMenu(ctx, NkT("mb.go"))) {
				MenuItem(ctx, NkT("mb.go.gotofile"), nullptr, false); // (a venir : quick open)
				MenuItem(ctx, NkT("mb.go.gotodef"), nullptr, false);  // (Ctrl+clic fonctionne dans l'editeur)
				MenuItem(ctx, NkT("mb.go.gotorefs"), nullptr, false);
				MenuItem(ctx, NkT("mb.go.gotoline"), nullptr, false);
				Separator(ctx);
				MenuItem(ctx, NkT("mb.go.nextproblem"), "F8", false); // (F8 fonctionne dans l'editeur)
				MenuItem(ctx, NkT("mb.go.prevproblem"), "Shift+F8", false);
				EndMenu(ctx);
			}

			// ── EXÉCUTER ─────────────────────────────────────────────────────
			if (BeginMenu(ctx, NkT("mb.run"))) {
				if (MenuItem(ctx, NkT("mb.run.run"), nullptr, hasWs) && s)
					s->DoRun();
				MenuItem(ctx, NkT("mb.run.debug"), nullptr, false); // (a venir : debogueur)
				MenuItem(ctx, NkT("mb.run.stop"), nullptr, false);	 // (a venir : kill du run)
				Separator(ctx);
				if (MenuItem(ctx, NkT("mb.run.buildtask"), nullptr, hasWs) && s)
					s->DoBuildAction("build");
				if (MenuItem(ctx, NkT("mb.run.rebuild"), nullptr, hasWs) && s)
					s->DoBuildAction("rebuild");
				if (MenuItem(ctx, NkT("mb.run.clean"), nullptr, hasWs) && s)
					s->DoClean();
				if (MenuItem(ctx, NkT("mb.run.tests"), nullptr, hasWs) && s)
					s->DoTest();
				Separator(ctx);
				MenuItem(ctx, NkT("mb.run.breakpoint"), "F9", false); // (gouttiere de l'editeur)
				MenuItem(ctx, "Hot Reload", nullptr, false);		   // (a venir)
				EndMenu(ctx);
			}

			// ── DÉBOGUER (backend a venir : tout grise, honnete) ─────────────
			if (BeginMenu(ctx, NkT("mb.debug"))) {
				MenuItem(ctx, "Attach to Process", nullptr, false);
				MenuItem(ctx, "Configurations", nullptr, false);
				MenuItem(ctx, "Debug Console", nullptr, false);
				MenuItem(ctx, "Call Stack", nullptr, false);
				MenuItem(ctx, "Variables", nullptr, false);
				MenuItem(ctx, "Watch", nullptr, false);
				MenuItem(ctx, "Breakpoints", nullptr, false);
				MenuItem(ctx, "Threads", nullptr, false);
				MenuItem(ctx, "GPU Debugger", nullptr, false);
				EndMenu(ctx);
			}

			// ── GIT (integration native a venir ; le terminal marche deja) ──
			if (BeginMenu(ctx, NkT("mb.git"))) {
				if (MenuItem(ctx, NkT("mb.git.opentermin"), nullptr, hasWs) && s) {
					s->termOpenCmd = "git status";
					s->termOpenKind = -1;
					s->termOpenAt = s->root.ToString();
					sh->FocusPanel("TERMINAL");
				}
				Separator(ctx);
				MenuItem(ctx, "Commit", nullptr, false);
				MenuItem(ctx, "Push", nullptr, false);
				MenuItem(ctx, "Pull", nullptr, false);
				MenuItem(ctx, "Fetch", nullptr, false);
				MenuItem(ctx, "Branches", nullptr, false);
				MenuItem(ctx, "Stash", nullptr, false);
				MenuItem(ctx, "History / Blame / Diff", nullptr, false);
				MenuItem(ctx, "Pull Request", nullptr, false);
				EndMenu(ctx);
			}

			// ── IA ───────────────────────────────────────────────────────────
			if (BeginMenu(ctx, NkT("mb.ai"))) {
				if (MenuItem(ctx, NkT("mb.ai.toggle"), "Ctrl+Shift+A")) {
					int32 gN = 0;
					const char *const *g = SideRightGroup(gN);
					ToggleSideExclusive(sh, g, gN, "Assistant IA");
				}
				if (MenuItem(ctx, NkT("mb.ai.chat")))
					sh->FocusPanel("Assistant IA");
				if (MenuItem(ctx, NkT("mb.ai.terminal"), nullptr, hasWs) && s) {
					s->termOpenCmd = "claude";
					s->termOpenKind = -1;
					s->termOpenAt = s->root.ToString();
					sh->FocusPanel("TERMINAL");
				}
				Separator(ctx);
				MenuItem(ctx, "Explain Selection", nullptr, false); // (a venir : pont selection -> chat)
				MenuItem(ctx, "Fix Selection", nullptr, false);
				MenuItem(ctx, "Generate Tests", nullptr, false);
				MenuItem(ctx, "Code Review", nullptr, false);
				MenuItem(ctx, "Generate Commit Message", nullptr, false);
				MenuItem(ctx, "Training Custom Model", nullptr, false);
				EndMenu(ctx);
			}

			// ── OUTILS ───────────────────────────────────────────────────────
			if (BeginMenu(ctx, NkT("mb.tools"))) {
				MenuItem(ctx, "Extensions", nullptr, false); // (Phase 10)
				MenuItem(ctx, "Package Manager", nullptr, false);
				MenuItem(ctx, NkT("mb.tools.keybindings"), nullptr, false);
				Separator(ctx);
				MenuItem(ctx, "Profiler", nullptr, false);
				MenuItem(ctx, "Memory Analyzer", nullptr, false);
				MenuItem(ctx, "Regex Tester", nullptr, false);
				MenuItem(ctx, "Diff Tool", nullptr, false);
				MenuItem(ctx, "Checksum", nullptr, false);
				EndMenu(ctx);
			}

			// ── FENÊTRE ──────────────────────────────────────────────────────
			if (BeginMenu(ctx, NkT("mb.window"))) {
				if (MenuItem(ctx, NkT("mb.window.togglepanel")))
					sh->FocusPanel("Sortie");
				if (MenuItem(ctx, NkT("mb.view.resetlayout")))
					sh->ResetLayout();
				Separator(ctx);
				MenuItem(ctx, "New Window", nullptr, false); // (a venir : multi-fenetres)
				MenuItem(ctx, "Split Editor", nullptr, false);
				MenuItem(ctx, "Layouts", nullptr, false);
				EndMenu(ctx);
			}

			// ── AIDE ─────────────────────────────────────────────────────────
			if (BeginMenu(ctx, NkT("mb.help"))) {
				if (MenuItem(ctx, NkT("mb.help.docs"), "F1"))
					NkLauncher::OpenURL("https://github.com/Rihen-Universe/Nkentseu/wiki");
				if (MenuItem(ctx, NkT("mb.help.releasenotes")))
					NkLauncher::OpenURL("https://github.com/Rihen-Universe/NKCode-Beta/releases");
				if (MenuItem(ctx, NkT("mb.help.reportissue")))
					NkLauncher::OpenURL("https://github.com/Rihen-Universe/NKCode-Beta/issues");
				Separator(ctx);
				MenuItem(ctx, NkT("mb.help.updates"), nullptr, false); // (Phase 13)
				MenuItem(ctx, "Interactive Tutorial", nullptr, false);
				if (MenuItem(ctx, NkT("mb.help.about")) && s)
					s->status = NkString("NKCode 0.1.0-beta \xE2\x80\x94 Rihen \xC2\xB7 rihen.universe@gmail.com");
				EndMenu(ctx);
			}
		}

		// Thunk pour NkEditorShell::SetMenuBar.
		inline void MainMenuBarThunk(NkEditorFrameContext &ec, void *user) {
			DrawMainMenuBar(ec, static_cast<NkMenuBarCtx *>(user));
		}

	} // namespace nkcode
} // namespace nkentseu
