#pragma once
// =============================================================================
// NkExplorer.h — EXPLORATEUR de projet (roadmap #3, maquette Banani).
//   Tranche 1 : arbre custom (icônes du dossier data/textures/icon via le
//   registre ForFile, chevrons, sélection, survol), badges Git colorés
//   (git status --porcelain, ASYNCHRONE via NkProcess), toolbar d'en-tête
//   (rafraîchir, tout replier, fichiers exclus, filtre), filtre inline.
//   L'arbre est mis en CACHE (rows) : le disque n'est relu qu'à l'ouverture
//   d'un dossier, au rafraîchissement ou à la mise à jour du statut Git.
// =============================================================================
#include "NKEditorKit/NkEditorKit.h"
#include "NKCode/Project/NkCodeState.h"
#include "NKCode/Project/NkProcess.h"
#include "NKCode/Shell/NkI18n.h"
#include "NKCode/Shell/NkUi.h" // NkIcons (registre extension -> texture)
#include <cstdio>

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu;
		using namespace nkentseu::editorkit;
		using namespace nkentseu::nkgui;

		class ExplorerPanel : public NkEditorPanel {
			public:
				explicit ExplorerPanel(NkCodeState *s)
					: NkEditorPanel("Explorateur", NkEditorDockSide::NK_LEFT), mS(s) {
				}

				void OnUI(NkEditorFrameContext &ec) override {
					auto &ctx = ec.Ui();
					++mTick;
					TickGit();
					// Changement de workspace -> reset complet (expansion, filtre, git).
					if (!SameStr(mRootStr.CStr(), mS->root.ToString().CStr())) {
						mRootStr = mS->root.ToString();
						mExpanded.Clear();
						mFilter[0] = 0;
						mFilterOn = false;
						mGitPath.Clear();
						mGitCode.Clear();
						mGitNext = 0;
						mRowsDirty = true;
					}
					if (mRowsDirty)
						BuildRows();
					DrawHeader(ctx);
					if (mFilterOn)
						DrawFilterBar(ctx);
					DrawRows(ctx);
				}

			private:
				// ── Ligne APLATIE de l'arbre (cache reconstruit à la demande) ──
				struct Row {
						NkString path;	///< chemin complet
						NkString name;	///< nom affiché
						int32 depth = 0;
						bool dir = false;
						bool open = false;
						bool root = false;
						char git = 0; ///< 0 = aucun, sinon M/A/U/D/C/R
				};

				static bool SameStr(const char *a, const char *b) {
					if (!a || !b)
						return a == b;
					while (*a && *a == *b) {
						++a;
						++b;
					}
					return *a == *b;
				}

				static bool ContainsI(const char *hay, const char *needle) {
					if (!needle || !*needle)
						return true;
					auto low = [](char c) { return (c >= 'A' && c <= 'Z') ? char(c + 32) : c; };
					for (const char *h = hay; *h; ++h) {
						const char *a = h, *b = needle;
						while (*a && *b && low(*a) == low(*b)) {
							++a;
							++b;
						}
						if (!*b)
							return true;
					}
					return false;
				}

				// Dossiers exclus de l'arbre tant que l'œil est fermé (artefacts/outils).
				static bool IsExcluded(const char *name) {
					return SameStr(name, ".git") || SameStr(name, "Build") || SameStr(name, ".nkcode") ||
						   SameStr(name, ".vs") || SameStr(name, ".cache");
				}

				// ── Statut Git ASYNCHRONE : git status --porcelain toutes les ~5 s. ──
				void TickGit() {
					if (mGit.Running())
						return;
					if (mGit.Done()) { // parse le résultat accumulé
						NkVector<NkString> lines;
						mGit.Drain(lines);
						mGitPath.Clear();
						mGitCode.Clear();
						for (usize i = 0; i < lines.Size(); ++i) {
							const char *l = lines[i].CStr();
							if (!l[0] || !l[1] || !l[2])
								continue;
							const char x = l[0], y = l[1];
							char code = 0;
							if (x == '?' && y == '?')
								code = 'U';
							else if (x == 'U' || y == 'U' || (x == 'A' && y == 'A') || (x == 'D' && y == 'D'))
								code = 'C';
							else if (x == 'R')
								code = 'R';
							else if (x == 'A')
								code = 'A';
							else if (x == 'D' || y == 'D')
								code = 'D';
							else if (x == 'M' || y == 'M')
								code = 'M';
							if (!code)
								continue;
							const char *p = l + 3;
							if (code == 'R') { // "R  old -> new" : garder new
								const char *arrow = p;
								for (const char *q = p; *q; ++q)
									if (q[0] == '-' && q[1] == '>' && q[2] == ' ')
										arrow = q + 3;
								p = arrow;
							}
							char clean[1024];
							int32 n = 0;
							for (const char *q = p; *q && n < 1023; ++q)
								if (*q != '"')
									clean[n++] = (*q == '\\') ? '/' : *q;
							while (n > 0 && (clean[n - 1] == '\r' || clean[n - 1] == '\n' || clean[n - 1] == ' '))
								--n;
							clean[n] = 0;
							mGitPath.PushBack(NkString(clean));
							mGitCode.PushBack(code);
						}
						mRowsDirty = true;
						mGitNext = mTick + 300; // ~5 s avant le prochain scan
					}
					if (mTick >= mGitNext && mS->root.ToString().Length() > 0) {
						NkString cmd = "git -C \"";
						cmd += mS->root.ToString();
						cmd += "\" status --porcelain";
						mGit.Start(cmd);
						mGitNext = mTick + 300; // garde-fou si le lancement échoue
					}
				}

				// Code Git d'un chemin (relatif à la racine, séparateurs '/').
				char GitCodeFor(const NkString &fullPath) const {
					const usize rootLen = mRootStr.Length();
					const char *f = fullPath.CStr();
					if (fullPath.Length() <= rootLen + 1)
						return 0;
					char rel[1024];
					int32 n = 0;
					for (const char *q = f + rootLen; *q && n < 1023; ++q) {
						if (n == 0 && (*q == '/' || *q == '\\'))
							continue; // saute le séparateur de tête
						rel[n++] = (*q == '\\') ? '/' : *q;
					}
					rel[n] = 0;
					for (usize i = 0; i < mGitPath.Size(); ++i)
						if (SameStr(mGitPath[i].CStr(), rel))
							return mGitCode[i];
					return 0;
				}

				static NkColor GitColor(char code) {
					switch (code) {
						case 'M':
							return {230, 160, 60, 255}; // orange
						case 'A':
							return {63, 185, 80, 255}; // vert
						case 'U':
							return {86, 182, 216, 255}; // cyan
						case 'D':
							return {248, 81, 73, 255}; // rouge
						case 'C':
							return {240, 196, 25, 255}; // jaune
						case 'R':
							return {15, 115, 213, 255}; // bleu
						default:
							return {200, 200, 200, 255};
					}
				}

				// ── (Re)construit le cache de lignes depuis le disque. ──
				void BuildRows() {
					mRows.Clear();
					mRowsDirty = false;
					if (mRootStr.Length() == 0)
						return;
					if (mFilterOn && mFilter[0]) { // filtre : liste PLATE des correspondances
						CollectFiltered(mS->root, 0);
						return;
					}
					Row r;
					r.path = mRootStr;
					r.name = mS->root.GetFileName();
					r.depth = 0;
					r.dir = true;
					r.open = mRootOpen;
					r.root = true;
					mRows.PushBack(r);
					if (mRootOpen)
						AppendDir(mS->root, 1);
				}

				void AppendDir(const NkPath &dir, int32 depth) {
					if (depth > 24)
						return; // garde-fou
					NkVector<NkDirectoryEntry> entries =
						NkDirectory::GetEntries(dir, "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
					for (int32 pass = 0; pass < 2; ++pass) { // dossiers d'abord, puis fichiers
						const bool wantDir = (pass == 0);
						for (usize i = 0; i < entries.Size(); ++i) {
							const NkDirectoryEntry &e = entries[i];
							if (e.IsDirectory != wantDir)
								continue;
							if (!mShowExcluded && e.IsDirectory && IsExcluded(e.Name.CStr()))
								continue;
							Row r;
							r.path = e.FullPath.ToString();
							r.name = e.Name;
							r.depth = depth;
							r.dir = e.IsDirectory;
							if (e.IsDirectory) {
								r.open = IsExpanded(r.path);
								mRows.PushBack(r);
								if (r.open)
									AppendDir(e.FullPath, depth + 1);
							} else {
								r.git = GitCodeFor(r.path);
								mRows.PushBack(r);
							}
						}
					}
				}

				void CollectFiltered(const NkPath &dir, int32 depth) {
					if (depth > 24 || mRows.Size() > 400)
						return;
					NkVector<NkDirectoryEntry> entries =
						NkDirectory::GetEntries(dir, "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
					for (usize i = 0; i < entries.Size(); ++i) {
						const NkDirectoryEntry &e = entries[i];
						if (e.IsDirectory) {
							if (mShowExcluded || !IsExcluded(e.Name.CStr()))
								CollectFiltered(e.FullPath, depth + 1);
						} else if (ContainsI(e.Name.CStr(), mFilter)) {
							Row r;
							r.path = e.FullPath.ToString();
							r.name = e.Name;
							r.depth = 0;
							r.dir = false;
							r.git = GitCodeFor(r.path);
							mRows.PushBack(r);
						}
					}
				}

				bool IsExpanded(const NkString &p) const {
					for (usize i = 0; i < mExpanded.Size(); ++i)
						if (SameStr(mExpanded[i].CStr(), p.CStr()))
							return true;
					return false;
				}

				void ToggleExpanded(const NkString &p) {
					for (usize i = 0; i < mExpanded.Size(); ++i)
						if (SameStr(mExpanded[i].CStr(), p.CStr())) {
							mExpanded.Erase(mExpanded.Begin() + i);
							return;
						}
					mExpanded.PushBack(p);
				}

				// ── En-tête : "EXPLORATEUR" + toolbar d'icônes alignée à droite. ──
				void DrawHeader(NkGuiContext &ctx) {
					const float32 h = ctx.ItemHeight();
					const NkRect bar = ctx.NextItemRect(ctx.ContentWidth(), h);
					auto &dl = ctx.DL();
					if (ctx.font && ctx.font->Valid())
						dl.AddText(ctx.font->Face(), ctx.font->TexId(),
								   {bar.x + 4.f, bar.y + (h - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent()},
								   "EXPLORATEUR", ctx.theme.textDisabled);
					const NkVec2 m = ctx.input.mousePos;
					const bool inClip = NkGuiRectContains(dl.CurrentClip(), m);
					const float32 bs = h - 4.f;
					float32 bx = bar.x + bar.w - bs - 2.f;
					// [4] filtre, [3] œil (exclus), [2] tout replier, [1] rafraîchir.
					for (int32 b = 4; b >= 1; --b, bx -= bs + 2.f) {
						const NkRect r = {bx, bar.y + 2.f, bs, bs};
						const bool hov = inClip && NkGuiRectContains(r, m);
						if (hov)
							dl.AddRectFilled(r, ctx.theme.tabHover, 2.f);
						const NkColor c = hov ? ctx.theme.text : ctx.theme.textDisabled;
						const uint32 tex = !mS->icons ? 0u
										   : b == 1  ? mS->icons->rebuild
										   : b == 3  ? (mShowExcluded ? mS->icons->oeilOuvert : mS->icons->oeilFermer)
										   : b == 4  ? mS->icons->search
												     : 0u;
						if (tex)
							dl.AddImage(tex, {r.x + 2.f, r.y + 2.f, r.w - 4.f, r.h - 4.f}, {0.f, 0.f}, {1.f, 1.f},
										{255, 255, 255, 255});
						else if (b == 2) { // tout replier : deux chevrons vers le haut (au trait)
							const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f, a = r.w * 0.22f;
							dl.AddLine({cx - a, cy - a * 0.1f}, {cx, cy - a * 1.1f}, c, 1.6f);
							dl.AddLine({cx, cy - a * 1.1f}, {cx + a, cy - a * 0.1f}, c, 1.6f);
							dl.AddLine({cx - a, cy + a * 1.1f}, {cx, cy + a * 0.1f}, c, 1.6f);
							dl.AddLine({cx, cy + a * 0.1f}, {cx + a, cy + a * 1.1f}, c, 1.6f);
						} else { // repli si l'icône n'est pas chargée : carré
							dl.AddRect({r.x + 3.f, r.y + 3.f, r.w - 6.f, r.h - 6.f}, c, 1.2f);
						}
						if (hov && ctx.input.mouseClicked[0]) {
							mFocus = true;
							if (b == 1) { // rafraîchir : disque + git
								mGitNext = mTick;
								mRowsDirty = true;
							} else if (b == 2) { // tout replier
								mExpanded.Clear();
								mRootOpen = true;
								mRowsDirty = true;
							} else if (b == 3) { // fichiers exclus
								mShowExcluded = !mShowExcluded;
								mRowsDirty = true;
							} else if (b == 4) { // filtre
								mFilterOn = !mFilterOn;
								if (!mFilterOn)
									mFilter[0] = 0;
								mRowsDirty = true;
							}
						}
					}
				}

				// ── Barre de filtre : loupe + saisie + caret + X. Échap ferme. ──
				void DrawFilterBar(NkGuiContext &ctx) {
					const float32 h = ctx.ItemHeight();
					const NkRect bar = ctx.NextItemRect(ctx.ContentWidth(), h);
					auto &dl = ctx.DL();
					dl.AddRectFilled({bar.x + 2.f, bar.y + 1.f, bar.w - 4.f, h - 2.f}, ctx.theme.bgPrimary, 2.f);
					dl.AddRect({bar.x + 2.f, bar.y + 1.f, bar.w - 4.f, h - 2.f}, ctx.theme.border, 2.f);
					if (mS->icons && mS->icons->search)
						dl.AddImage(mS->icons->search, {bar.x + 5.f, bar.y + 3.f, h - 6.f, h - 6.f}, {0.f, 0.f},
									{1.f, 1.f}, {255, 255, 255, 255});
					const char *shown = mFilter[0] ? mFilter : NkT("exp.filter");
					const NkColor col = mFilter[0] ? ctx.theme.text : ctx.theme.textDisabled;
					float32 tx = bar.x + h + 2.f;
					if (ctx.font && ctx.font->Valid()) {
						dl.AddText(ctx.font->Face(), ctx.font->TexId(),
								   {tx, bar.y + (h - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent()}, shown, col);
						if (mFocus && (mTick / 30) % 2 == 0 && mFilter[0]) // caret clignotant
							dl.AddRectFilled({tx + ctx.font->MeasureWidth(mFilter) + 1.f, bar.y + 3.f, 1.5f, h - 6.f},
											 ctx.theme.accent);
					}
					// X : ferme le filtre.
					const NkRect xr = {bar.x + bar.w - h, bar.y + 2.f, h - 4.f, h - 4.f};
					const NkVec2 m = ctx.input.mousePos;
					const bool xh = NkGuiRectContains(dl.CurrentClip(), m) && NkGuiRectContains(xr, m);
					const NkColor xc = xh ? ctx.theme.text : ctx.theme.textDisabled;
					dl.AddLine({xr.x + 4.f, xr.y + 4.f}, {xr.x + xr.w - 4.f, xr.y + xr.h - 4.f}, xc, 1.5f);
					dl.AddLine({xr.x + 4.f, xr.y + xr.h - 4.f}, {xr.x + xr.w - 4.f, xr.y + 4.f}, xc, 1.5f);
					if (xh && ctx.input.mouseClicked[0]) {
						mFilterOn = false;
						mFilter[0] = 0;
						mRowsDirty = true;
					}
					// Saisie clavier (si le panneau a le focus-clic).
					if (mFocus) {
						int32 len = 0;
						while (mFilter[len])
							++len;
						bool changed = false;
						for (int32 i = 0; i < ctx.input.charCount; ++i) {
							const uint32 cp = ctx.input.chars[i];
							if (cp >= 32 && cp < 127 && len < 62) {
								mFilter[len++] = static_cast<char>(cp);
								changed = true;
							}
						}
						if (ctx.input.KeyPressed(NkGuiKey::Backspace) && len > 0) {
							--len;
							changed = true;
						}
						mFilter[len] = 0;
						if (ctx.input.KeyPressed(NkGuiKey::Escape)) {
							mFilterOn = false;
							mFilter[0] = 0;
							changed = true;
						}
						if (changed)
							mRowsDirty = true;
					}
				}

				// ── L'arbre : une ligne par row (chevron + icône + nom + badge git). ──
				void DrawRows(NkGuiContext &ctx) {
					const float32 rowH = ctx.ItemHeight();
					const float32 fullW = ctx.ContentWidth();
					auto &dl = ctx.DL();
					const NkVec2 m = ctx.input.mousePos;
					const NkRect clip = dl.CurrentClip();
					const bool inClip = NkGuiRectContains(clip, m);
					// Focus-clic du panneau : un clic DANS la zone le prend, ailleurs le rend.
					if (ctx.input.mouseClicked[0])
						mFocus = inClip;
					int32 toToggle = -1, toOpen = -1;
					for (usize i = 0; i < mRows.Size(); ++i) {
						const Row &r = mRows[i];
						const NkRect row = ctx.NextItemRect(fullW, rowH);
						if (row.y + rowH < clip.y || row.y > clip.y + clip.h)
							continue; // hors vue : la place est réservée, pas de dessin
						const bool hov = inClip && NkGuiRectContains(row, m);
						const bool sel = SameStr(mSelPath.CStr(), r.path.CStr());
						if (sel) {
							dl.AddRectFilled(row, ctx.theme.selection);
							dl.AddRectFilled({row.x, row.y, 2.f, rowH}, ctx.theme.accent);
						} else if (hov)
							dl.AddRectFilled(row, ctx.theme.tabHover);
						float32 x = row.x + 4.f + r.depth * 12.f;
						const float32 cy = row.y + rowH * 0.5f;
						if (r.dir) { // chevron ▸/▾ au trait
							const float32 a = 3.5f;
							const NkColor cc = ctx.theme.textDisabled;
							if (r.open) {
								dl.AddLine({x, cy - a * 0.5f}, {x + a, cy + a * 0.5f}, cc, 1.5f);
								dl.AddLine({x + a, cy + a * 0.5f}, {x + 2 * a, cy - a * 0.5f}, cc, 1.5f);
							} else {
								dl.AddLine({x, cy - a}, {x + a, cy}, cc, 1.5f);
								dl.AddLine({x + a, cy}, {x, cy + a}, cc, 1.5f);
							}
						}
						x += 10.f;
						// Icône : dossier = folderOpen teinté ; fichier = registre ForFile.
						const float32 is = rowH - 6.f;
						uint32 tex = 0;
						NkColor tint = {255, 255, 255, 255};
						if (mS->icons) {
							if (r.dir) {
								tex = mS->icons->folderOpen;
								tint = r.root ? NkColor{230, 160, 60, 255} : NkColor{120, 160, 200, 255};
							} else {
								tex = mS->icons->ForFile(r.name.CStr());
								if (!tex) {
									tex = mS->icons->fileText;
									tint = {150, 150, 150, 255};
								}
							}
						}
						if (tex)
							dl.AddImage(tex, {x, cy - is * 0.5f, is, is}, {0.f, 0.f}, {1.f, 1.f}, tint);
						x += is + 4.f;
						// Nom (racine en circonflexe visuel : couleur pleine).
						if (ctx.font && ctx.font->Valid()) {
							NkColor nc = ctx.theme.text;
							if (r.git == 'D')
								nc.a = 120;
							dl.AddText(ctx.font->Face(), ctx.font->TexId(),
									   {x, row.y + (rowH - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent()},
									   r.name.CStr(), nc, row.x + fullW - x - 16.f);
						}
						// Badge Git coloré à droite.
						if (r.git && ctx.font && ctx.font->Valid()) {
							const char b[2] = {r.git, 0};
							dl.AddText(ctx.font->Face(), ctx.font->TexId(),
									   {row.x + fullW - 12.f,
										row.y + (rowH - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent()},
									   b, GitColor(r.git));
						}
						// Clic : dossier = plier/déplier ; fichier = sélection + ouverture.
						if (hov && ctx.input.mouseClicked[0]) {
							mSelPath = r.path;
							if (r.dir)
								toToggle = static_cast<int32>(i);
							else
								toOpen = static_cast<int32>(i);
						}
					}
					// Mutations APRÈS la boucle (mRows est reconstruit par BuildRows).
					if (toToggle >= 0) {
						const Row &r = mRows[toToggle];
						if (r.root)
							mRootOpen = !mRootOpen;
						else
							ToggleExpanded(r.path);
						mRowsDirty = true;
					} else if (toOpen >= 0)
						mS->OpenPath(NkPath(mRows[toOpen].path));
					// Entrée : ouvre la sélection (fichier).
					if (mFocus && ctx.input.KeyPressed(NkGuiKey::Enter) && mSelPath.Length() > 0)
						for (usize i = 0; i < mRows.Size(); ++i)
							if (!mRows[i].dir && SameStr(mRows[i].path.CStr(), mSelPath.CStr())) {
								mS->OpenPath(NkPath(mRows[i].path));
								break;
							}
				}

				NkCodeState *mS = nullptr;
				NkVector<Row> mRows;
				NkVector<NkString> mExpanded;
				NkString mRootStr, mSelPath;
				bool mRowsDirty = true, mRootOpen = true;
				bool mFilterOn = false, mShowExcluded = false, mFocus = false;
				char mFilter[64] = {};
				uint32 mTick = 0;
				// Git async
				NkProcess mGit;
				NkVector<NkString> mGitPath;
				NkVector<char> mGitCode;
				uint32 mGitNext = 0;
		};

	} // namespace nkcode
} // namespace nkentseu
