#pragma once
// =============================================================================
// NkMenuBar.h — BARRE DE MENUS PRINCIPALE de NKCode (spec Banani, 11 menus).
//   REMPLACE les menus par defaut du shell (NkEditorShell::SetMenuBar).
//
//   Regle d'HONNETETE : un item n'est CLIQUABLE que si son backend existe
//   reellement ; sinon AFFICHE GRISE — jamais un clic muet. Les commandes
//   externes (git, checksum, mises a jour) passent par le TERMINAL integre :
//   visibles, reelles, transparentes.
// =============================================================================
#include "NKCode/Shell/Dialogs.h"	  // NkCodeDialogs (Open*, SaveActiveNative, ShowStart, DoLoad)
#include "NKCode/Shell/Panels.h"	  // SideLeftGroup/OpenSideExclusive (Recherche)
#include "NKCode/Shell/NkHome.h"	  // NkHomeOpenNewWindow (Nouvelle fenetre)
#include "NKCode/Shell/NkI18n.h"	  // NkT
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
				NkHomeState *home = nullptr; // wizard « Nouveau Workspace » du launcher (nav==2)
				NkString exePath;			 // pour « Nouvelle fenetre » (NkHomeOpenNewWindow)
				// « Ouvrir un fichier »/« Aller au fichier » via le PICKER MAISON :
				// le picker (PK_File) remplit ce buffer a la confirmation (asynchrone) ;
				// DrawMainMenuBar le poll chaque frame et ouvre le fichier.
				char openFileBuf[512] = {};
		};

		namespace menubar_detail {

			// Mot (identifiant) sous le caret — pour Aller a la definition/references.
			inline NkString WordAtCaret(NkCodeDoc &d) {
				if (d.curLine < 0 || d.curLine >= d.LineCount())
					return NkString();
				const NkCodeLine &L = d.lines[d.curLine];
				int32 a = d.curCol, b = d.curCol;
				const int32 n = static_cast<int32>(L.Size());
				if (a > n)
					a = b = n;
				while (a > 0 && NkCodeDoc::IsWChar(L[a - 1]))
					--a;
				while (b < n && NkCodeDoc::IsWChar(L[b]))
					++b;
				NkString w;
				for (int32 i = a; i < b; ++i)
					w += L[i];
				return w;
			}

			// (le commentaire de ligne passe par doc->ToggleComment(CommentPrefix(lang)),
			//  sensible au langage — meme chemin que Ctrl+/ dans l'editeur)

			// Reindentation simple par profondeur d'accolades (Format Document) —
			// 4 espaces par niveau, lignes vides laissees vides.
			inline void ReindentDocument(NkCodeDoc &d) {
				d.Checkpoint(3);
				const NkString txt = d.GetText();
				NkString out;
				int32 depth = 0;
				const char *p = txt.CStr();
				const char *ls = p;
				for (;; ++p) {
					if (*p == '\n' || !*p) {
						const char *q = ls;
						while (q < p && (*q == ' ' || *q == '\t'))
							++q;
						int32 openC = 0, closeC = 0;
						for (const char *r = q; r < p; ++r) {
							if (*r == '{')
								++openC;
							else if (*r == '}')
								++closeC;
						}
						int32 lineDepth = depth;
						if (q < p && *q == '}')
							lineDepth = depth - 1 > 0 ? depth - 1 : 0; // ligne fermante desindentee
						if (q < p) {
							for (int32 i = 0; i < lineDepth * 4; ++i)
								out += ' ';
							for (const char *r = q; r < p; ++r)
								out += *r;
						}
						depth += openC - closeC;
						if (depth < 0)
							depth = 0;
						if (!*p)
							break;
						out += "\n";
						ls = p + 1;
					}
				}
				d.SetText(out.CStr());
			}

			// Probleme suivant/precedent (meme logique que F8 dans l'editeur).
			inline void GotoProblem(NkCodeDoc &d, bool fwd) {
				if (d.diags.Empty())
					return;
				int32 bestL = -1, bestC = 0;
				for (usize i = 0; i < d.diags.Size(); ++i) {
					const int32 l = d.diags[i].line, c = d.diags[i].col < 0 ? 0 : d.diags[i].col;
					const bool after = (l > d.curLine) || (l == d.curLine && c > d.curCol);
					const bool before = (l < d.curLine) || (l == d.curLine && c < d.curCol);
					if (fwd ? after : before) {
						if (bestL < 0 || (fwd ? (l < bestL || (l == bestL && c < bestC))
											   : (l > bestL || (l == bestL && c > bestC)))) {
							bestL = l;
							bestC = c;
						}
					}
				}
				if (bestL < 0) { // boucle (wrap) sur le premier/dernier
					bestL = d.diags[0].line;
					bestC = d.diags[0].col < 0 ? 0 : d.diags[0].col;
					for (usize i = 1; i < d.diags.Size(); ++i) {
						const int32 l = d.diags[i].line, c = d.diags[i].col < 0 ? 0 : d.diags[i].col;
						if (fwd ? (l < bestL || (l == bestL && c < bestC)) : (l > bestL || (l == bestL && c > bestC))) {
							bestL = l;
							bestC = c;
						}
					}
				}
				d.curLine = bestL;
				d.curCol = bestC;
				d.selLine = bestL;
				d.selCol = bestC;
				d.wantReveal = true;
			}

			// Ouvre le terminal integre avec une commande executee a la racine.
			inline void TermCmd(NkCodeState *s, NkEditorShell *sh, const char *cmd) {
				if (!s || !sh)
					return;
				s->termOpenCmd = cmd;
				s->termOpenKind = -1;
				s->termOpenAt = s->HasWorkspace() ? s->root.ToString() : NkString(".");
				sh->FocusPanel("TERMINAL");
			}

			// Menu IA -> chat : joint la selection (ou le fichier courant) au prompt.
			inline void AiAsk(NkCodeState *s, NkEditorShell *sh, const char *ask, bool wantSelection) {
				if (!s || !sh)
					return;
				NkString p(ask);
				if (wantSelection && s->HasActive()) {
					NkCodeDoc &d = s->files[s->active].doc;
					const NkString sel = d.HasSel() ? d.GetSelectedText() : NkString();
					if (!sel.Empty()) {
						p += "\n```\n";
						p += sel;
						p += "\n```";
					} else {
						p += "\n[Fichier : ";
						p += s->files[s->active].path.ToString();
						p += "]";
					}
				}
				s->aiPrompt = p;
				s->aiSend = true;
				sh->FocusPanel("Assistant IA");
			}

		} // namespace menubar_detail

		inline void DrawMainMenuBar(NkEditorFrameContext &ec, NkMenuBarCtx *mb) {
			using namespace menubar_detail;
			if (!mb || !mb->dlg || !mb->shell)
				return;
			auto &ctx = ec.Ui();
			NkCodeDialogs *d = mb->dlg;
			NkEditorShell *sh = mb->shell;
			NkCodeState *s = d->st;
			const bool hasWs = s && s->HasWorkspace();
			const bool hasFile = s && s->HasActive();
			NkCodeDoc *doc = hasFile ? &s->files[s->active].doc : nullptr;

			// Resultat ASYNCHRONE du picker « Ouvrir un fichier » (PK_File remplit le
			// buffer a la confirmation, une frame plus tard) -> ouverture ici.
			if (mb->openFileBuf[0] && s) {
				s->OpenPath(NkPath(mb->openFileBuf));
				mb->openFileBuf[0] = 0;
			}

			// ── FICHIER ──────────────────────────────────────────────────────
			if (BeginMenu(ctx, NkT("mb.file"))) {
				if (MenuItem(ctx, NkT("mb.file.startscreen")))
					d->ShowStart();
				Separator(ctx);
				if (MenuItem(ctx, NkT("mb.file.newfile"), "Ctrl+N", hasWs) && s) {
					// Onglet vide + PICKER MAISON en mode enregistrer : le doc etant
					// vide, l'ASSISTANT complet de creation s'affiche (emplacement,
					// nom, type de fichier, proprietes/scaffolding C++) — exactement
					// la fenetre modale de creation demandee, pas un onglet muet.
					s->NewFile();
					d->SaveActiveNative();
				}
				if (MenuItem(ctx, NkT("mb.file.newfolder"), nullptr, hasWs) && s)
					// Picker maison : navigation libre + bouton « nouveau dossier »
					// integre (PickerCreateFolder) -> l'utilisateur choisit l'emplacement
					// ET cree le dossier dans le meme dialogue modal.
					d->OpenPicker(NkCodeDialogs::PK_NewFolder, s->root.ToString().CStr());
				if (MenuItem(ctx, NkT("mb.file.newproject"), nullptr, hasWs))
					d->Open(NkCodeDialogs::NewProject); // dialogue riche (nom/template/langage)
				if (MenuItem(ctx, NkT("mb.file.newworkspace"))) {
					// Modale DEDIEE = wizard COMPLET du launcher ; le workspace cree
					// est AJOUTE comme racine de l'explorateur (courant reste charge).
					d->showNewWs = true;
					d->wsAddAsRoot = true;
				}
				Separator(ctx);
				if (MenuItem(ctx, NkT("mb.file.openfile")) && s) {
					// PICKER MAISON (pas le dialogue systeme), navigation DISQUE ENTIER
					// (aucun confinement au workspace) — consigne explicite de Rihen.
					mb->openFileBuf[0] = 0;
					d->OpenPicker(NkCodeDialogs::PK_File, hasWs ? s->root.ToString().CStr() : nullptr, mb->openFileBuf,
								  (int32)sizeof(mb->openFileBuf));
				}
				if (MenuItem(ctx, NkT("mb.file.openfolder")))
					d->OpenFolderDialog();
				if (MenuItem(ctx, NkT("mb.file.openws")))
					d->OpenWorkspaceDialog();
				if (s && !s->recents.Empty() && BeginMenu(ctx, NkT("mb.file.recent"))) {
					for (usize i = 0; i < s->recents.Size() && i < 10; ++i) {
						const char *nm = i < s->recentNames.Size() ? s->recentNames[i].CStr()
																	: s->recents[i].CStr();
						if (MenuItem(ctx, nm))
							d->DoLoad(NkPath(s->recents[i].CStr()).GetParent());
					}
					EndMenu(ctx);
				}
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
				if (s && BeginMenu(ctx, NkT("mb.file.autosave"))) {
					const bool off = s->autoSave == 0;
					if (MenuItem(ctx, (NkString(off ? "\xE2\x9C\x93 " : "   ") + NkT("mb.autosave.off")).CStr()))
						s->autoSave = 0;
					if (MenuItem(ctx, (NkString(!off ? "\xE2\x9C\x93 " : "   ") + NkT("mb.autosave.delay")).CStr()))
						s->autoSave = 1;
					EndMenu(ctx);
				}
				if (MenuItem(ctx, NkT("mb.file.revert"), nullptr, hasFile && s->ActiveHasPath()) && doc && s) {
					const NkString disk = NkFile::ReadAllText(s->files[s->active].path);
					doc->Checkpoint(3); // Ctrl+Z peut revenir a l'etat d'avant le revert
					doc->SetText(disk.CStr());
					doc->dirty = false;
					s->status = NkString("Fichier retabli depuis le disque");
				}
				Separator(ctx);
				if (MenuItem(ctx, NkT("mb.file.reveal"), nullptr, hasWs) && s) {
					const NkString target = hasFile ? s->files[s->active].path.ToString() : s->root.ToString();
#if defined(_WIN32)
					NkCodeShellRun((NkString("explorer /select,\"") + target.CStr() + "\"").CStr());
#else
					NkCodeShellRun((NkString("xdg-open \"") + NkPath(target).GetParent().ToString().CStr() + "\"").CStr());
#endif
				}
				if (MenuItem(ctx, NkT("mb.file.export"), nullptr, hasWs) && s)
					// Picker maison : choisir le dossier de DESTINATION du zip ; la
					// confirmation lance tar dans le terminal integre (RoutePickerResult).
					d->OpenPicker(NkCodeDialogs::PK_ExportZip, s->root.ToString().CStr());
				Separator(ctx);
				if (MenuItem(ctx, NkT("mb.file.prefs")))
					d->showPrefs = true; // modale PREFERENCES complete (panneau launcher)
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
				if (BeginMenu(ctx, NkT("mb.edit.pastespecial"))) {
					// Editeur = texte brut par nature : "coller comme texte brut" est le
					// MEME collage (sans reindentation relative on retire les \t -> espaces
					// deja gere par InsertText) — expose pour la parite VSCode.
					if (MenuItem(ctx, NkT("mb.edit.pasteplain"), nullptr, hasFile) && doc) {
						const NkString clip = ctx.GetClipboard();
						if (!clip.Empty()) {
							doc->Checkpoint(3);
							doc->InsertText(clip.CStr());
						}
					}
					EndMenu(ctx);
				}
				if (MenuItem(ctx, NkT("mb.edit.selectall"), "Ctrl+A", hasFile) && doc) {
					doc->selLine = 0;
					doc->selCol = 0;
					doc->curLine = doc->LineCount() - 1;
					doc->curCol = doc->LineLen(doc->curLine);
				}
				Separator(ctx);
				if (MenuItem(ctx, NkT("mb.edit.find"), "Ctrl+F", hasFile) && doc) {
					doc->findReplace = false; // barre SANS le champ Remplacer (= Ctrl+F)
					doc->findOpen = true;
					doc->findBarActive = true;
					doc->FindRecompute();
				}
				if (MenuItem(ctx, NkT("mb.edit.findnext"), "F3", hasFile) && doc)
					doc->FindNext(true);
				if (MenuItem(ctx, NkT("mb.edit.findprev"), "Shift+F3", hasFile) && doc)
					doc->FindNext(false);
				if (MenuItem(ctx, NkT("mb.edit.replace"), "Ctrl+H", hasFile) && doc) {
					doc->findReplace = true; // mode REMPLACEMENT (= Ctrl+H), champ en plus
					doc->findOpen = true;
					doc->findBarActive = true;
					doc->FindRecompute();
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
				// ── Ligne : MEMES actions que les raccourcis natifs de l'editeur ──
				if (BeginMenu(ctx, NkT("mb.edit.line"))) {
					if (MenuItem(ctx, NkT("mb.edit.dupline"), "Ctrl+D", hasFile) && doc)
						doc->DuplicateSelOrLine();
					if (MenuItem(ctx, NkT("mb.edit.movelineup"), "Alt+\xE2\x86\x91", hasFile) && doc) {
						doc->Checkpoint(3);
						doc->MoveLines(true);
					}
					if (MenuItem(ctx, NkT("mb.edit.movelinedown"), "Alt+\xE2\x86\x93", hasFile) && doc) {
						doc->Checkpoint(3);
						doc->MoveLines(false);
					}
					if (MenuItem(ctx, NkT("mb.edit.delline"), "Ctrl+Shift+K", hasFile) && doc) {
						doc->Checkpoint(3);
						doc->DeleteLines();
					}
					if (MenuItem(ctx, NkT("mb.edit.insertbelow"), "Ctrl+Entree", hasFile) && doc) {
						doc->Checkpoint(3);
						doc->InsertLineBelow();
					}
					if (MenuItem(ctx, NkT("mb.edit.insertabove"), "Ctrl+Shift+Entree", hasFile) && doc) {
						doc->Checkpoint(3);
						doc->InsertLineAbove();
					}
					if (MenuItem(ctx, NkT("mb.edit.selectline"), "Ctrl+L", hasFile) && doc)
						doc->SelectCurrentLine();
					EndMenu(ctx);
				}
				// ── Multi-curseur ──
				if (BeginMenu(ctx, NkT("mb.edit.multicursor"))) {
					if (MenuItem(ctx, NkT("mb.edit.cursorabove"), nullptr, hasFile) && doc && doc->curLine > 0) {
						doc->extraCarets.PushBack({doc->selLine, doc->selCol, doc->curLine, doc->curCol});
						doc->curLine -= 1;
						doc->selLine = doc->curLine;
						doc->selCol = doc->curCol;
					}
					if (MenuItem(ctx, NkT("mb.edit.cursorbelow"), nullptr, hasFile) && doc &&
						doc->curLine + 1 < doc->LineCount()) {
						doc->extraCarets.PushBack({doc->selLine, doc->selCol, doc->curLine, doc->curCol});
						doc->curLine += 1;
						doc->selLine = doc->curLine;
						doc->selCol = doc->curCol;
					}
					if (MenuItem(ctx, NkT("mb.edit.nextoccur"), "Ctrl+Shift+D", hasFile) && doc)
						doc->SelectWordOrAddNext();
					if (MenuItem(ctx, NkT("mb.edit.alloccur"), "Ctrl+Shift+L", hasFile) && doc)
						doc->SelectAllOccurrences();
					EndMenu(ctx);
				}
				Separator(ctx);
				// ── Commentaires / indentation — sensibles au LANGAGE du fichier actif,
				//    exactement comme Ctrl+/ et Ctrl+Shift+/ dans l'editeur. ──
				{
					const NkLang mlang =
						hasFile ? NkLangFromExt(s->files[s->active].path.GetExtension().CStr()) : NkLang::None;
					if (MenuItem(ctx, NkT("mb.edit.linecomment"), "Ctrl+/", hasFile) && doc) {
						doc->Checkpoint(3);
						doc->ToggleComment(CommentPrefix(mlang));
					}
					if (MenuItem(ctx, NkT("mb.edit.blockcomment"), "Ctrl+Shift+/", hasFile) && doc) {
						doc->Checkpoint(3);
						if (mlang == NkLang::C || mlang == NkLang::NKSL)
							doc->BlockComment("/* ", " */");
						else
							doc->ToggleComment(CommentPrefix(mlang));
					}
				}
				if (MenuItem(ctx, NkT("mb.edit.indent"), "Ctrl+]", hasFile) && doc) {
					doc->Checkpoint(3);
					doc->IndentSelection(false);
				}
				if (MenuItem(ctx, NkT("mb.edit.unindent"), "Ctrl+[", hasFile) && doc) {
					doc->Checkpoint(3);
					doc->IndentSelection(true);
				}
				if (MenuItem(ctx, NkT("mb.edit.format"), nullptr, hasFile) && doc)
					ReindentDocument(*doc);
				Separator(ctx);
				if (MenuItem(ctx, NkT("mb.edit.foldall"), "Ctrl+K Ctrl+0", hasFile) && doc)
					doc->FoldAll();
				if (MenuItem(ctx, NkT("mb.edit.unfoldall"), "Ctrl+K Ctrl+J", hasFile) && doc)
					doc->UnfoldAll();
				if (BeginMenu(ctx, NkT("mb.edit.snippets"))) {
					if (MenuItem(ctx, "main()", nullptr, hasFile) && doc)
						doc->InsertText("int main(int argc, char **argv) {\n    return 0;\n}\n");
					if (MenuItem(ctx, "class", nullptr, hasFile) && doc)
						doc->InsertText("class MaClasse {\npublic:\n    MaClasse() = default;\n};\n");
					if (MenuItem(ctx, "#pragma once", nullptr, hasFile) && doc)
						doc->InsertText("#pragma once\n");
					if (MenuItem(ctx, "for (i)", nullptr, hasFile) && doc)
						doc->InsertText("for (int i = 0; i < n; ++i) {\n}\n");
					EndMenu(ctx);
				}
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
					// -> modale PREFERENCES, ouverte sur la bonne categorie.
					if (MenuItem(ctx, NkT("mb.view.theme"))) {
						d->showPrefs = true;
						if (mb->home)
							mb->home->settings.cat = 3; // Theme
					}
					if (MenuItem(ctx, NkT("mb.view.fonts"))) {
						d->showPrefs = true;
						if (mb->home)
							mb->home->settings.cat = 0; // General (echelle/police UI)
					}
					if (MenuItem(ctx, NkT("mb.view.langs"))) {
						d->showPrefs = true;
						if (mb->home)
							mb->home->settings.cat = 0; // General (langue)
					}
					EndMenu(ctx);
				}
				Separator(ctx);
				if (MenuItem(ctx, (NkString(NkCodeMinimapOn() ? "\xE2\x9C\x93 " : "   ") + NkT("mb.view.minimap")).CStr()))
					NkCodeMinimapOn() = !NkCodeMinimapOn();
				if (MenuItem(ctx, (NkString(NkCodeTabRowsOn() ? "\xE2\x9C\x93 " : "   ") + NkT("mb.view.tabrows")).CStr()))
					NkCodeTabRowsOn() = !NkCodeTabRowsOn();
				if (s && MenuItem(ctx, (NkString(s->showBreadcrumb ? "\xE2\x9C\x93 " : "   ") + NkT("mb.view.breadcrumbs")).CStr()))
					s->showBreadcrumb = !s->showBreadcrumb;
				if (MenuItem(ctx,
							 (NkString(doc && doc->wrapOn ? "\xE2\x9C\x93 " : "   ") + NkT("mb.view.wordwrap")).CStr(),
							 "Alt+Z", hasFile) &&
					doc)
					doc->wrapOn = !doc->wrapOn;
				Separator(ctx);
				// ── Zoom du CODE, par onglet — memes bornes que Ctrl+molette / Ctrl+± ──
				if (MenuItem(ctx, NkT("mb.view.zoomin"), "Ctrl++", hasFile) && s) {
					auto &f = s->files[s->active];
					float32 z = (f.codeZoom > 0.f ? f.codeZoom : sh->CodeFontSize()) + 1.f;
					f.codeZoom = z > 40.f ? 40.f : z;
				}
				if (MenuItem(ctx, NkT("mb.view.zoomout"), "Ctrl+-", hasFile) && s) {
					auto &f = s->files[s->active];
					float32 z = (f.codeZoom > 0.f ? f.codeZoom : sh->CodeFontSize()) - 1.f;
					f.codeZoom = z < 8.f ? 8.f : z;
				}
				if (MenuItem(ctx, NkT("mb.view.zoomreset"), "Ctrl+0", hasFile) && s)
					s->files[s->active].codeZoom = 0.f; // 0 = taille globale
				Separator(ctx);
				if (MenuItem(ctx, NkT("mb.view.resetlayout"))) {
					// Reinitialisation COMPLETE : re-bootstrap du dock ET suppression de
					// l'etat d'UI sauvegarde du projet (sinon l'ancienne disposition se
					// reapplique au prochain chargement du workspace).
					sh->ResetLayout();
					if (s && s->HasWorkspace()) {
						NkFile::Delete(NkPath(s->UiConfigPath().CStr()));
						s->status = NkString("Disposition reinitialisee (defaut)");
					}
				}
				EndMenu(ctx);
			}

			// ── ALLER ────────────────────────────────────────────────────────
			if (BeginMenu(ctx, NkT("mb.go"))) {
				if (MenuItem(ctx, NkT("mb.go.gotofile")) && s) {
					// meme picker maison que « Ouvrir un fichier » (disque entier).
					mb->openFileBuf[0] = 0;
					d->OpenPicker(NkCodeDialogs::PK_File, hasWs ? s->root.ToString().CStr() : nullptr, mb->openFileBuf,
								  (int32)sizeof(mb->openFileBuf));
				}
				Separator(ctx);
				const NkString word = doc ? WordAtCaret(*doc) : NkString();
				const bool hasWord = hasWs && !word.Empty();
				if (MenuItem(ctx, NkT("mb.go.gotodef"), nullptr, hasWord) && s)
					s->StartGotoDef(word, s->FlagsForFile(s->files[s->active].path.ToString()));
				if (MenuItem(ctx, NkT("mb.go.gotorefs"), nullptr, hasWord) && s)
					s->StartRefs(word, s->FlagsForFile(s->files[s->active].path.ToString()));
				Separator(ctx);
				const bool hasDiags = doc && !doc->diags.Empty();
				if (MenuItem(ctx, NkT("mb.go.nextproblem"), "F8", hasDiags) && doc)
					GotoProblem(*doc, true);
				if (MenuItem(ctx, NkT("mb.go.prevproblem"), "Shift+F8", hasDiags) && doc)
					GotoProblem(*doc, false);
				EndMenu(ctx);
			}

			// ── EXÉCUTER ─────────────────────────────────────────────────────
			if (BeginMenu(ctx, NkT("mb.run"))) {
				if (MenuItem(ctx, NkT("mb.run.run"), nullptr, hasWs) && s)
					s->DoRun();
				MenuItem(ctx, NkT("mb.run.debug"), nullptr, false); // (a venir : debogueur, chantier dedie)
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
				if (MenuItem(ctx, NkT("mb.run.breakpoint"), "F9", hasFile) && doc)
					doc->ToggleBreakpoint(doc->curLine);
				EndMenu(ctx);
			}

			// ── DÉBOGUER (backend = chantier dedie ; honnete : tout grise) ───
			if (BeginMenu(ctx, NkT("mb.debug"))) {
				MenuItem(ctx, "Attach to Process", nullptr, false);
				MenuItem(ctx, "Configurations", nullptr, false);
				MenuItem(ctx, "Call Stack / Variables / Watch", nullptr, false);
				MenuItem(ctx, "GPU Debugger", nullptr, false);
				EndMenu(ctx);
			}

			// ── GIT (via le terminal integre : commandes REELLES et visibles) ─
			if (BeginMenu(ctx, NkT("mb.git"))) {
				if (MenuItem(ctx, "Status", nullptr, hasWs))
					TermCmd(s, sh, "git status");
				if (MenuItem(ctx, "Init", nullptr, hasWs))
					TermCmd(s, sh, "git init");
				Separator(ctx);
				if (MenuItem(ctx, "Push", nullptr, hasWs))
					TermCmd(s, sh, "git push");
				if (MenuItem(ctx, "Pull", nullptr, hasWs))
					TermCmd(s, sh, "git pull");
				if (MenuItem(ctx, "Fetch", nullptr, hasWs))
					TermCmd(s, sh, "git fetch --all");
				if (MenuItem(ctx, "Sync (pull + push)", nullptr, hasWs))
					TermCmd(s, sh, "git pull && git push");
				if (MenuItem(ctx, "Stage All", nullptr, hasWs))
					TermCmd(s, sh, "git add -A && git status --short");
				Separator(ctx);
				if (MenuItem(ctx, "Branches", nullptr, hasWs))
					TermCmd(s, sh, "git branch -a -v");
				if (MenuItem(ctx, "Stash", nullptr, hasWs))
					TermCmd(s, sh, "git stash");
				if (MenuItem(ctx, "Apply Stash", nullptr, hasWs))
					TermCmd(s, sh, "git stash pop");
				Separator(ctx);
				if (MenuItem(ctx, "History", nullptr, hasWs))
					TermCmd(s, sh, "git log --oneline --graph -30");
				if (MenuItem(ctx, "Diff", nullptr, hasWs))
					TermCmd(s, sh, "git diff");
				if (MenuItem(ctx, "Blame (fichier actif)", nullptr, hasFile) && s)
					TermCmd(s, sh, (NkString("git blame -- \"") + s->files[s->active].path.ToString().CStr() + "\"").CStr());
				Separator(ctx);
				MenuItem(ctx, "Commit (UI dediee a venir)", nullptr, false);
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
				if (MenuItem(ctx, NkT("mb.ai.terminal"), nullptr, hasWs))
					TermCmd(s, sh, "claude");
				Separator(ctx);
				const bool aiSel = hasFile;
				if (MenuItem(ctx, NkT("mb.ai.explain"), nullptr, aiSel))
					AiAsk(s, sh, "Explique ce code :", true);
				if (MenuItem(ctx, NkT("mb.ai.fix"), nullptr, aiSel))
					AiAsk(s, sh, "Corrige les problemes de ce code (explique chaque correction) :", true);
				if (MenuItem(ctx, NkT("mb.ai.optimize"), nullptr, aiSel))
					AiAsk(s, sh, "Optimise ce code (perfs/lisibilite), en justifiant :", true);
				if (MenuItem(ctx, NkT("mb.ai.tests"), nullptr, aiSel))
					AiAsk(s, sh, "Genere des tests unitaires pour ce code :", true);
				if (MenuItem(ctx, NkT("mb.ai.doc"), nullptr, aiSel))
					AiAsk(s, sh, "Genere la documentation (commentaires) de ce code :", true);
				if (MenuItem(ctx, NkT("mb.ai.review"), nullptr, aiSel))
					AiAsk(s, sh, "Fais une revue de code detaillee (bugs, style, securite) :", true);
				if (MenuItem(ctx, NkT("mb.ai.commitmsg"), nullptr, hasWs))
					AiAsk(s, sh, "Regarde `git diff` et `git status` puis propose un message de commit clair.", false);
				Separator(ctx);
				MenuItem(ctx, "Training Custom Model", nullptr, false); // (a venir : NkAI)
				EndMenu(ctx);
			}

			// ── OUTILS ───────────────────────────────────────────────────────
			if (BeginMenu(ctx, NkT("mb.tools"))) {
				if (MenuItem(ctx, NkT("mb.view.theme"))) {
					d->showPrefs = true;
					if (mb->home)
						mb->home->settings.cat = 3; // Theme
				}
				if (MenuItem(ctx, NkT("mb.tools.checksum"), nullptr, hasFile) && s)
					TermCmd(s, sh,
							(NkString("certutil -hashfile \"") + s->files[s->active].path.ToString().CStr() +
							 "\" SHA256").CStr());
				if (MenuItem(ctx, NkT("mb.tools.jsonviewer"), nullptr, hasFile) && s) {
					OpenFile &f = s->files[s->active];
					if (NkFindSub(f.path.ToString().CStr(), ".json"))
						f.mdPreview = !f.mdPreview;
					else
						s->status = NkString("Ouvrez un fichier .json pour la vue arbre");
				}
				Separator(ctx);
				MenuItem(ctx, "Extensions", nullptr, false); // (Phase 10)
				MenuItem(ctx, "Package Manager", nullptr, false);
				MenuItem(ctx, "Profiler / Memory / Network", nullptr, false);
				EndMenu(ctx);
			}

			// ── FENÊTRE ──────────────────────────────────────────────────────
			if (BeginMenu(ctx, NkT("mb.window"))) {
				if (MenuItem(ctx, NkT("mb.window.newwindow"), nullptr, !mb->exePath.Empty()))
					NkHomeOpenNewWindow(mb->exePath, hasWs ? s->root.ToString() : NkString());
				if (MenuItem(ctx, NkT("mb.window.closewindow")))
					sh->RequestClose();
				Separator(ctx);
				if (MenuItem(ctx, NkT("mb.window.closeall"), nullptr, hasFile) && s) {
					int32 kept = 0;
					for (int32 i = static_cast<int32>(s->files.Size()) - 1; i >= 0; --i) {
						if (s->files[i].doc.dirty)
							++kept; // jamais de perte silencieuse : les modifies restent
						else
							s->CloseFile(i);
					}
					if (kept)
						s->status = NkPrintf("%d fichier(s) modifie(s) conserves (enregistrez d'abord)", kept);
				}
				if (MenuItem(ctx, NkT("mb.window.closefolder"), nullptr, hasWs))
					d->ShowStart();
				if (s && !s->files.Empty() && BeginMenu(ctx, NkT("mb.window.editorn"))) {
					for (usize i = 0; i < s->files.Size() && i < 9; ++i) {
						if (MenuItem(ctx, NkPrintf("%d  %s", (int32)i + 1, s->files[i].Name().CStr()).CStr()))
							s->active = static_cast<int32>(i);
					}
					EndMenu(ctx);
				}
				Separator(ctx);
				if (hasWs && MenuItem(ctx, NkT("mb.window.savelayout")) && s)
					sh->SaveUiState((s->root.ToString() + "/.nkcode/layout_manual.cfg").CStr());
				if (hasWs && MenuItem(ctx, NkT("mb.window.loadlayout")) && s)
					sh->LoadUiState((s->root.ToString() + "/.nkcode/layout_manual.cfg").CStr());
				if (MenuItem(ctx, NkT("mb.view.resetlayout")))
					sh->ResetLayout();
				EndMenu(ctx);
			}

			// ── AIDE ─────────────────────────────────────────────────────────
			if (BeginMenu(ctx, NkT("mb.help"))) {
				if (MenuItem(ctx, NkT("mb.help.docs"), "F1"))
					NkLauncher::OpenURL("https://github.com/Rihen-Universe/Nkentseu/wiki");
				if (MenuItem(ctx, NkT("mb.help.shortcuts")))
					NkLauncher::OpenURL("https://github.com/Rihen-Universe/Nkentseu/wiki");
				if (MenuItem(ctx, NkT("mb.help.forum")))
					NkLauncher::OpenURL("https://github.com/Rihen-Universe/NKCode-Beta");
				if (MenuItem(ctx, NkT("mb.help.releasenotes")))
					NkLauncher::OpenURL("https://github.com/Rihen-Universe/NKCode-Beta/releases");
				if (MenuItem(ctx, NkT("mb.help.reportissue")))
					NkLauncher::OpenURL("https://github.com/Rihen-Universe/NKCode-Beta/issues");
				Separator(ctx);
				if (MenuItem(ctx, NkT("mb.help.updates"), nullptr, true) && s)
					TermCmd(s, sh,
							"curl -s https://api.github.com/repos/Rihen-Universe/NKCode-Beta/releases/latest | findstr /i \"tag_name name browser_download_url\"");
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
