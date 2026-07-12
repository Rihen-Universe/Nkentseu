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
#include "NKCode/Shell/NkUi.h"		// NkIcons (registre extension -> texture)
#include "NKCode/Shell/NkShell.h"	// NkCodeShellRun (révéler dans l'OS)
#include "NKCode/Editor/NkTextDraw.h" // NkCtxMenu (menu contextuel modal scrollable)
#include "NKContainers/String/NkFormat.h" // NkPrintf/NkFormat (outils maison, pas snprintf)
#include "NKWindow/Core/NkDialogs.h"		  // OpenFolderDialog (ajouter une racine)

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
						LoadRoots(); // racines secondaires du workspace (.nkcode/roots.cfg)
						mRowsDirty = true;
					}
					if (mRowsDirty)
						BuildRows();
					// L'EN-TÊTE est FIXE (il ne défile pas) : l'espace est réservé dans le
					// flux, les rows défilent DESSOUS, le chrome est dessiné PAR-DESSUS.
					const NkRect vclip = ctx.DL().CurrentClip();
					const float32 headH = ctx.ItemHeight() * (mFilterOn ? 2.f : 1.f);
					ctx.NextItemRect(ctx.ContentWidth(), headH); // réserve du flux
					DrawRows(ctx, vclip.y + headH);
					mS->explorerFocus = mFocus; // publie le focus (l'éditeur coupe son clavier)
					DrawHeader(ctx, vclip);
					if (mFilterOn)
						DrawFilterBar(ctx, vclip);
					DrawCtxMenu(ctx);	 // overlay : par-dessus tout
					DrawConfirmDel(ctx); // confirmation de suppression
					DrawPeek(ctx);		 // aperçu barre Espace (overlay centré) — lit ctx.input
					// PEEK OUVERT = MODAL : l'explorateur est le 1er panneau dessiné ; on
					// neutralise l'input APRÈS le peek -> les panneaux suivants (éditeur,
					// terminal…) ne reçoivent aucun CLIC sous l'overlay. Le peek, lui, a
					// utilisé l'input RÉEL (même chemin que DrawRows).
					// ⚠ NE PAS toucher mousePos : il n'est mis à jour QUE sur un événement
					// de DÉPLACEMENT (jamais re-sondé) -> l'écraser le GÈLE pour toute
					// l'app à la frame suivante (clic sans bouger = position figée). On
					// neutralise donc SEULEMENT les clics/molette/frappes.
					if (mPeekOpen) {
						for (int32 b = 0; b < 3; ++b) {
							ctx.input.mouseClicked[b] = false;
							ctx.input.mouseDown[b] = false;
							ctx.input.mouseDoubleClicked[b] = false;
						}
						ctx.input.wheel = ctx.input.wheelH = 0.f;
						ctx.input.charCount = 0;
					}
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
						bool extraRoot = false; ///< racine SECONDAIRE (retirable du workspace)
						bool editNew = false;	///< row VIRTUELLE : champ de création inline
						char git = 0;			///< 0 = aucun, sinon M/A/U/D/C/R ('*' = dossier touché)
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

				// Extension (minuscule, sans point) d'un nom de fichier ; "" si aucune.
				static void ExtOf(const char *name, char *out, int32 cap) {
					const char *dot = nullptr;
					for (const char *p = name; *p; ++p)
						if (*p == '.')
							dot = p;
					int32 n = 0;
					if (dot)
						for (const char *p = dot + 1; *p && n < cap - 1; ++p) {
							char c = *p;
							out[n++] = (c >= 'A' && c <= 'Z') ? char(c + 32) : c;
						}
					out[n] = 0;
				}

				static bool ExtIs(const char *e, const char *k) {
					return SameStr(e, k);
				}

				// Shaders (tout type) : icône dédiée ; NKSL (langage maison) encore plus.
				static bool IsShaderExt(const char *e) {
					return ExtIs(e, "vert") || ExtIs(e, "frag") || ExtIs(e, "glsl") || ExtIs(e, "hlsl") ||
						   ExtIs(e, "comp") || ExtIs(e, "geom") || ExtIs(e, "tesc") || ExtIs(e, "tese") ||
						   ExtIs(e, "wgsl") || ExtIs(e, "metal") || ExtIs(e, "shader");
				}

				// FALLBACK sans icône : pastille « lettres du langage » + couleur dédiée.
				// true si le langage est connu (label 1-2 lettres + couleur posés).
				static bool LangBadge(const char *e, char *label, NkColor &col) {
					struct L {
							const char *ext, *lab;
							NkColor c;
					};
					static const L k[] = {
						{"cpp", "C++", {101, 155, 211, 255}}, {"cc", "C++", {101, 155, 211, 255}},
						{"cxx", "C++", {101, 155, 211, 255}}, {"h", "H", {178, 132, 219, 255}},
						{"hpp", "H", {178, 132, 219, 255}},	  {"hxx", "H", {178, 132, 219, 255}},
						{"c", "C", {86, 130, 180, 255}},	  {"py", "PY", {240, 200, 80, 255}},
						{"rs", "RS", {222, 130, 80, 255}},	  {"zig", "ZG", {247, 164, 66, 255}},
						{"js", "JS", {230, 210, 90, 255}},	  {"ts", "TS", {80, 140, 220, 255}},
						{"json", "{}", {200, 180, 80, 255}},  {"md", "MD", {110, 170, 230, 255}},
						{"txt", "T", {160, 160, 160, 255}},	  {"lua", "LU", {90, 120, 220, 255}},
						{"java", "JV", {220, 120, 70, 255}},  {"cs", "C#", {150, 110, 200, 255}},
						{"sh", "SH", {120, 200, 120, 255}},	  {"bat", "BT", {120, 200, 120, 255}},
						{"cmd", "BT", {120, 200, 120, 255}},  {"ps1", "PS", {80, 160, 220, 255}},
						{"cmake", "CM", {180, 80, 80, 255}},  {"yml", "YM", {200, 100, 150, 255}},
						{"yaml", "YM", {200, 100, 150, 255}}, {"xml", "XM", {200, 140, 80, 255}},
						{"html", "<>", {228, 110, 80, 255}},  {"css", "#", {90, 160, 220, 255}},
						{"toml", "TM", {180, 120, 90, 255}},  {"ini", "IN", {150, 150, 150, 255}},
						{"cfg", "IN", {150, 150, 150, 255}},  {"jenga", "JG", {247, 154, 40, 255}},
					};
					for (usize i = 0; i < sizeof(k) / sizeof(k[0]); ++i)
						if (ExtIs(e, k[i].ext)) {
							int32 n = 0;
							for (; k[i].lab[n] && n < 3; ++n)
								label[n] = k[i].lab[n];
							label[n] = 0;
							col = k[i].c;
							return true;
						}
					return false;
				}

				// Couleur des DOSSIERS : les dossiers « spéciaux » ont leur teinte propre.
				static NkColor DirTint(const char *name, bool root) {
					if (root)
						return {230, 160, 60, 255}; // racine : orange
					auto is = [&](const char *k) {
						const char *a = name;
						const char *b = k;
						auto low = [](char c) { return (c >= 'A' && c <= 'Z') ? char(c + 32) : c; };
						while (*a && *b && low(*a) == low(*b)) {
							++a;
							++b;
						}
						return !*a && !*b;
					};
					if (is("src") || is("source") || is("sources"))
						return {96, 165, 250, 255}; // bleu vif
					if (is("include") || is("inc") || is("headers"))
						return {178, 132, 219, 255}; // violet
					if (is("test") || is("tests") || is("unitest"))
						return {200, 200, 90, 255}; // jaune
					if (is("assets") || is("data") || is("media") || is("resources") || is("textures"))
						return {110, 190, 120, 255}; // vert
					if (is("shaders") || is("shader"))
						return {170, 120, 220, 255}; // violet clair
					if (is("docs") || is("doc") || is("documentation"))
						return {90, 190, 210, 255}; // cyan
					if (is("build") || is("bin") || is("obj") || is("out"))
						return {130, 130, 130, 255}; // gris
					if (is("exemples") || is("examples") || is("samples"))
						return {100, 200, 180, 255}; // turquoise
					return {120, 160, 200, 255}; // défaut : bleu-gris
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
					// Done() RESTE vrai après la fin : sans ce drapeau le parse tournait à
					// CHAQUE frame (badges écrasés par un parse vide + BuildRows relisant
					// tout le disque en boucle -> stress allocateur massif).
					if (mGit.Done() && !mGitParsed) { // parse le résultat accumulé (1 fois)
						mGitParsed = true;
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
						if (mGit.Start(cmd))
							mGitParsed = false;
						mGitNext = mTick + 300; // garde-fou si le lancement échoue
					}
				}

				// Un DOSSIER contient-il des modifications git ? (préfixe "rel/" d'un
				// chemin touché -> le dossier ET ses parents sont marqués, façon VSCode.)
				bool GitDirTouched(const NkString &dirFull) const {
					const usize rootLen = mRootStr.Length();
					if (dirFull.Length() <= rootLen || mGitPath.Empty())
						return false;
					char rel[1024];
					int32 n = 0;
					const char *f = dirFull.CStr();
					for (const char *q = f + rootLen; *q && n < 1022; ++q) {
						if (n == 0 && (*q == '/' || *q == '\\'))
							continue;
						rel[n++] = (*q == '\\') ? '/' : *q;
					}
					rel[n++] = '/';
					rel[n] = 0;
					for (usize i = 0; i < mGitPath.Size(); ++i) {
						const char *a = mGitPath[i].CStr();
						const char *b = rel;
						bool pref = true;
						while (*b) {
							if (*a != *b) {
								pref = false;
								break;
							}
							++a;
							++b;
						}
						if (pref)
							return true;
					}
					return false;
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
						case '*':
							return {230, 160, 60, 255}; // dossier contenant des modifs
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
					if (mRootOpen) {
						if (mEditParent.Length() > 0 && SameStr(mEditParent.CStr(), mRootStr.CStr()))
							PushEditRow(1);
						AppendDir(mS->root, 1);
					}
					// ── RACINES SECONDAIRES (multi-racines, façon VSCode) ──
					for (usize e = 0; e < mExtraRoots.Size(); ++e) {
						const NkString &er = mExtraRoots[e];
						Row rr;
						rr.path = er;
						rr.name = NkPath(er).GetFileName();
						rr.depth = 0;
						rr.dir = true;
						rr.open = IsExpanded(er);
						rr.root = true;
						rr.extraRoot = true;
						mRows.PushBack(rr);
						if (rr.open)
							AppendDir(NkPath(er), 1);
					}
				}

				// ── Multi-racines : persistance dans <ws>/.nkcode/roots.cfg. ──
				NkString RootsPath() const {
					return (NkPath(mRootStr.CStr()) / ".nkcode" / "roots.cfg").ToString();
				}

				void LoadRoots() {
					mExtraRoots.Clear();
					const NkString f = RootsPath();
					if (!NkFile::Exists(f.CStr()))
						return;
					const NkString txt = NkFile::ReadAllText(NkPath(f));
					NkString line;
					for (const char *p = txt.CStr();; ++p) {
						if (*p == '\n' || *p == '\r' || *p == '\0') {
							if (line.Length() > 2 && NkDirectory::Exists(line.CStr()))
								mExtraRoots.PushBack(line);
							line.Clear();
							if (*p == '\0')
								break;
						} else
							line += *p;
					}
				}

				void SaveRoots() {
					NkString out;
					for (usize i = 0; i < mExtraRoots.Size(); ++i) {
						out += mExtraRoots[i];
						out += "\n";
					}
					const NkString f = RootsPath();
					NkDirectory::CreateRecursive(NkPath(f).GetParent());
					NkFile::WriteAllText(NkPath(f), out);
				}

				// Ctrl+R : sélecteur natif -> ajoute une racine secondaire (dédupliquée).
				void AddRootFolder() {
					const auto res = nkentseu::NkDialogs::OpenFolderDialog(NkT("exp.addroot"));
					if (!res.confirmed || res.path.Length() < 2)
						return;
					NkString np = res.path;
					for (int32 i = 0; i < static_cast<int32>(np.Length()); ++i)
						if (np.CStr()[i] == '\\')
							const_cast<char *>(np.CStr())[i] = '/';
					if (SameStr(np.CStr(), mRootStr.CStr()))
						return; // déjà la racine principale
					for (usize i = 0; i < mExtraRoots.Size(); ++i)
						if (SameStr(mExtraRoots[i].CStr(), np.CStr()))
							return; // déjà présente
					mExtraRoots.PushBack(np);
					if (!IsExpanded(np))
						ToggleExpanded(np);
					SaveRoots();
					mRowsDirty = true;
				}

				void RemoveRootFolder(const NkString &p) {
					for (usize i = 0; i < mExtraRoots.Size(); ++i)
						if (SameStr(mExtraRoots[i].CStr(), p.CStr())) {
							mExtraRoots.Erase(mExtraRoots.Begin() + i);
							SaveRoots();
							mRowsDirty = true;
							return;
						}
				}

				// Row VIRTUELLE du champ de création inline (en tête du dossier parent).
				void PushEditRow(int32 depth) {
					Row r;
					r.depth = depth;
					r.dir = mEditCreateDir;
					r.editNew = true;
					mRows.PushBack(r);
				}

				void AppendDir(const NkPath &dir, int32 depth) {
					if (depth > 24)
						return; // garde-fou
					if (mEditParent.Length() > 0 && SameStr(mEditParent.CStr(), dir.ToString().CStr()))
						PushEditRow(depth); // champ de création en tête du dossier cible
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
								r.git = GitDirTouched(r.path) ? '*' : 0;
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

				// ── Chemins ──
				static NkString ParentOf(const NkString &p) {
					const char *s = p.CStr();
					int32 cut = -1;
					for (int32 j = 0; s[j]; ++j)
						if (s[j] == '/' || s[j] == '\\')
							cut = j;
					return cut > 0 ? p.SubStr(0, cut) : p;
				}

				// Chemin relatif à la racine (séparateurs '/').
				void RelOf(const NkString &full, char *out, int32 cap) const {
					const usize rootLen = mRootStr.Length();
					int32 n = 0;
					if (full.Length() > rootLen)
						for (const char *q = full.CStr() + rootLen; *q && n < cap - 1; ++q) {
							if (n == 0 && (*q == '/' || *q == '\\'))
								continue;
							out[n++] = (*q == '\\') ? '/' : *q;
						}
					out[n] = 0;
				}

				// Dossier CIBLE des créations/collages : la sélection si c'est un dossier,
				// sinon son parent, sinon la racine.
				NkString TargetDir() const {
					if (mSelPath.Length() == 0)
						return mRootStr;
					for (usize i = 0; i < mRows.Size(); ++i)
						if (SameStr(mRows[i].path.CStr(), mSelPath.CStr()))
							return mRows[i].dir ? mRows[i].path : ParentOf(mRows[i].path);
					return mRootStr;
				}

				bool SelIsDir() const {
					for (usize i = 0; i < mRows.Size(); ++i)
						if (SameStr(mRows[i].path.CStr(), mSelPath.CStr()))
							return mRows[i].dir;
					return false;
				}

				// ── Opérations fichiers : APIs MAISON NkFile/NkDirectory (jamais la STL
				//    ni un shell externe — le moteur a déjà tout, CORBEILLE comprise). ──

				// Supprimer = envoyer à la CORBEILLE système (récupérable).
				void Trash(const NkString &path, bool dir) {
					const bool ok = dir ? NkDirectory::MoveToTrash(path.CStr()) : NkFile::MoveToTrash(path.CStr());
					if (ok) {
						mRowsDirty = true;
						mGitNext = mTick;
					}
				}

				void CopyOrMove(const NkString &src, const NkString &dst, bool move) {
					const bool dir = NkDirectory::Exists(src.CStr());
					bool ok;
					if (move)
						ok = dir ? NkDirectory::Move(src.CStr(), dst.CStr()) : NkFile::Move(src.CStr(), dst.CStr());
					else
						ok = dir ? NkDirectory::Copy(src.CStr(), dst.CStr(), /*recursive=*/true)
								 : NkFile::Copy(src.CStr(), dst.CStr());
					if (ok) {
						mRowsDirty = true;
						mGitNext = mTick;
					}
				}

				// Demande de suppression : ouvre la CONFIRMATION (menu à la souris).
				void RequestDelete(NkGuiContext &ctx, const NkString &path, bool dir) {
					if (path.Length() == 0 || SameStr(path.CStr(), mRootStr.CStr()))
						return;
					mDelPath = path;
					mDelIsDir = dir;
					// SÉLECTION MULTIPLE : si la cible fait partie d'un groupe, la
					// confirmation (et la suppression) portent sur TOUT le groupe.
					mDelGroup = (mSelSet.Size() > 1 && IsSelected(path));
					if (mDelGroup)
						mDelLabel = NkPrintf("%s (%d)", NkT("exp.ctx.delete"), static_cast<int32>(mSelSet.Size()));
					else {
						const NkString name = NkPath(path).GetFileName();
						mDelLabel = NkPrintf("%s \xC2\xAB %s \xC2\xBB", NkT("exp.ctx.delete"), name.CStr());
					}
					mDelMenu.open = true;
					mDelMenu.pos = ctx.input.mousePos;
				}

				// Confirmation de suppression (2 items : confirmer / annuler).
				void DrawConfirmDel(NkGuiContext &ctx) {
					if (!mDelMenu.open)
						return;
					const char *items[2] = {mDelLabel.CStr(), NkT("exp.del.cancel")};
					bool en[2] = {true, true};
					const int32 act = NkCtxMenuDraw(ctx, mDelMenu, items, en, 2);
					if (act == 0) {
						if (mDelGroup) { // corbeille pour TOUTE la sélection
							const NkVector<NkString> grp = mSelSet;
							for (usize i = 0; i < grp.Size(); ++i)
								if (!SameStr(grp[i].CStr(), mRootStr.CStr()))
									Trash(grp[i], NkDirectory::Exists(grp[i].CStr()));
							mSelSet.Clear();
							mSelPath.Clear();
						} else
							Trash(mDelPath, mDelIsDir);
						mDelPath.Clear();
					} else if (act == 1 || !mDelMenu.open)
						mDelPath.Clear();
				}

				// « Nom - copie.ext » à côté de l'original.
				void DuplicateOf(const NkString &p) {
					const NkString name = NkPath(p).GetFileName();
					const char *s = name.CStr();
					int32 dot = -1;
					for (int32 j = 0; s[j]; ++j)
						if (s[j] == '.')
							dot = j;
					NkString base = (dot > 0) ? name.SubStr(0, dot) : name;
					NkString ext = (dot > 0) ? name.SubStr(dot, name.Length() - dot) : NkString("");
					NkString dst = ParentOf(p);
					dst += "/";
					dst += base;
					dst += " - copie";
					dst += ext;
					CopyOrMove(p, dst, false);
				}

				void RevealInOS(const NkString &p) {
					NkString cmd = "explorer /select,\"";
					cmd += p;
					cmd += "\"";
					NkCodeShellRun(cmd.CStr());
				}

				void AddToGitignore(const NkString &p) {
					char rel[1024];
					RelOf(p, rel, sizeof(rel));
					if (!rel[0])
						return;
					NkString gi = mRootStr;
					gi += "/.gitignore";
					NkString txt = NkFile::Exists(gi.CStr()) ? NkFile::ReadAllText(NkPath(gi)) : NkString("");
					if (txt.Length() > 0 && txt.CStr()[txt.Length() - 1] != '\n')
						txt += "\n";
					txt += rel;
					txt += "\n";
					NkFile::WriteAllText(NkPath(gi), txt);
					mGitNext = mTick;
					mRowsDirty = true;
				}

				// ── Édition INLINE : renommage (F2 / clic-lent / menu) + création. ──
				void StartRename(const NkString &p) {
					if (p.Length() == 0)
						return;
					mEditPath = p;
					mEditParent.Clear();
					const NkString name = NkPath(p).GetFileName();
					int32 n = 0;
					for (const char *q = name.CStr(); *q && n < 127; ++q)
						mEditBuf[n++] = *q;
					mEditBuf[n] = 0;
					mFocus = true;
				}

				void StartCreate(const NkString &parent, bool dir) {
					mEditParent = parent.Length() ? parent : mRootStr;
					mEditPath.Clear();
					mEditCreateDir = dir;
					mEditBuf[0] = 0;
					if (!IsExpanded(mEditParent) && !SameStr(mEditParent.CStr(), mRootStr.CStr()))
						ToggleExpanded(mEditParent);
					mRootOpen = true;
					mFocus = true;
					mRowsDirty = true; // insère la row virtuelle
				}

				void CancelEdit() {
					mEditPath.Clear();
					mEditParent.Clear();
					mEditBuf[0] = 0;
					mRowsDirty = true;
				}

				void CommitEdit() {
					if (!mEditBuf[0]) {
						CancelEdit();
						return;
					}
					if (mEditParent.Length() > 0) { // CRÉATION
						NkString full = mEditParent;
						full += "/";
						full += mEditBuf;
						if (!NkFile::Exists(full.CStr())) {
							if (mEditCreateDir)
								NkDirectory::CreateRecursive(NkPath(full));
							else {
								NkFile::WriteAllText(NkPath(full), "");
								mS->OpenPath(NkPath(full));
							}
							mSelPath = full;
						}
					} else if (mEditPath.Length() > 0) { // RENOMMAGE (API maison, pas la STL)
						NkString dst = ParentOf(mEditPath);
						dst += "/";
						dst += mEditBuf;
						if (!SameStr(dst.CStr(), mEditPath.CStr()) && !NkFile::Exists(dst.CStr())) {
							const bool dir = NkDirectory::Exists(mEditPath.CStr());
							const bool ok = dir ? NkDirectory::Move(mEditPath.CStr(), dst.CStr())
												: NkFile::Move(mEditPath.CStr(), dst.CStr());
							if (ok)
								mSelPath = dst;
						}
					}
					CancelEdit();
					mGitNext = mTick;
				}

				// ── Sélection multiple : helpers. ──
				bool IsSelected(const NkString &p) const {
					for (usize i = 0; i < mSelSet.Size(); ++i)
						if (SameStr(mSelSet[i].CStr(), p.CStr()))
							return true;
					return false;
				}

				void SelectOnly(const NkString &p, int32 row) {
					mSelSet.Clear();
					mSelSet.PushBack(p);
					mSelPath = p;
					mSelAnchor = row;
				}

				void SelectToggle(const NkString &p, int32 row) { // Ctrl+clic
					for (usize i = 0; i < mSelSet.Size(); ++i)
						if (SameStr(mSelSet[i].CStr(), p.CStr())) {
							mSelSet.Erase(mSelSet.Begin() + i);
							if (SameStr(mSelPath.CStr(), p.CStr()))
								mSelPath = mSelSet.Empty() ? NkString() : mSelSet.Back();
							return;
						}
					mSelSet.PushBack(p);
					mSelPath = p;
					mSelAnchor = row;
				}

				void SelectRange(int32 a, int32 b) { // Maj+clic (plage de rows visibles)
					if (a < 0 || b < 0)
						return;
					if (a > b) {
						const int32 t = a;
						a = b;
						b = t;
					}
					mSelSet.Clear();
					for (int32 i = a; i <= b && i < static_cast<int32>(mRows.Size()); ++i)
						if (!mRows[i].editNew)
							mSelSet.PushBack(mRows[i].path);
					if (!mSelSet.Empty())
						mSelPath = mSelSet.Back();
				}

				// ── En-tête FIXE : "EXPLORATEUR" + toolbar (fichier, dossier, actualiser,
				//    replier, exclus, filtre) alignée à droite, fond opaque. ──
				void DrawHeader(NkGuiContext &ctx, const NkRect &clip) {
					const float32 h = ctx.ItemHeight();
					const NkRect bar = {clip.x, clip.y, clip.w, h};
					auto &dl = ctx.DL();
					dl.AddRectFilled(bar, ctx.theme.panel); // opaque : les rows passent dessous
					dl.AddRectFilled({bar.x, bar.y + h - 1.f, bar.w, 1.f}, ctx.theme.border);
					if (ctx.font && ctx.font->Valid())
						dl.AddText(ctx.font->Face(), ctx.font->TexId(),
								   {bar.x + 4.f, bar.y + (h - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent()},
								   "EXPLORATEUR", ctx.theme.textDisabled);
					const NkVec2 m = ctx.input.mousePos;
					const bool inClip = NkGuiRectContains(dl.CurrentClip(), m);
					const float32 bs = h - 4.f;
					float32 bx = bar.x + bar.w - bs - 2.f;
					// Fin du titre : les boutons ne CHEVAUCHENT jamais « EXPLORATEUR » —
					// si le panneau est trop étroit, les boutons de gauche disparaissent.
					const float32 titleEnd =
						bar.x + 4.f +
						((ctx.font && ctx.font->Valid()) ? ctx.font->MeasureWidth("EXPLORATEUR") : 90.f) + 6.f;
					// [6] filtre, [5] œil (exclus), [4] replier, [3] actualiser,
					// [2] nouveau dossier, [1] nouveau fichier — de droite à gauche.
					for (int32 b = 6; b >= 1; --b, bx -= bs + 2.f) {
						if (bx < titleEnd)
							break;
						const NkRect r = {bx, bar.y + 2.f, bs, bs};
						const bool hov = inClip && NkGuiRectContains(r, m);
						if (hov)
							dl.AddRectFilled(r, ctx.theme.tabHover, 2.f);
						const NkColor c = hov ? ctx.theme.text : ctx.theme.textDisabled;
						const uint32 tex = !mS->icons ? 0u
										   : b == 1  ? (mS->icons->newFile2 ? mS->icons->newFile2 : mS->icons->filePlus)
										   : b == 2  ? mS->icons->newFolder
										   : b == 3  ? (mS->icons->rebuild)
										   : b == 4  ? mS->icons->collapseAll
										   : b == 5  ? (mShowExcluded ? mS->icons->oeilOuvert : mS->icons->oeilFermer)
										   : b == 6  ? (mS->icons->filter ? mS->icons->filter : mS->icons->search)
												     : 0u;
						if (tex)
							dl.AddImage(tex, {r.x + 2.f, r.y + 2.f, r.w - 4.f, r.h - 4.f}, {0.f, 0.f}, {1.f, 1.f},
										{255, 255, 255, 255});
						else if (b == 4) { // tout replier : deux chevrons vers le haut (au trait)
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
							if (b == 1) // nouveau fichier : champ inline dans le dossier cible
								StartCreate(TargetDir(), false);
							else if (b == 2) // nouveau dossier
								StartCreate(TargetDir(), true);
							else if (b == 3) { // actualiser : disque + git
								mGitNext = mTick;
								mRowsDirty = true;
							} else if (b == 4) { // tout replier
								mExpanded.Clear();
								mRootOpen = true;
								mRowsDirty = true;
							} else if (b == 5) { // fichiers exclus
								mShowExcluded = !mShowExcluded;
								mRowsDirty = true;
							} else if (b == 6) { // filtre
								mFilterOn = !mFilterOn;
								if (!mFilterOn)
									mFilter[0] = 0;
								mRowsDirty = true;
							}
						}
					}
				}

				// ── Barre de filtre FIXE (2e ligne de l'en-tête) : loupe + saisie + X. ──
				void DrawFilterBar(NkGuiContext &ctx, const NkRect &clip) {
					const float32 h = ctx.ItemHeight();
					const NkRect bar = {clip.x, clip.y + h, clip.w, h};
					auto &dl = ctx.DL();
					dl.AddRectFilled(bar, ctx.theme.panel); // opaque
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
						if ((mTick / 30) % 2 == 0) // caret clignotant (aussi champ vide)
							dl.AddRectFilled({tx + (mFilter[0] ? ctx.font->MeasureWidth(mFilter) : 0.f) + 1.f,
											  bar.y + 3.f, 1.5f, h - 6.f},
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
					// Saisie clavier : filtre OUVERT = saisie ACTIVE (l'exigence de focus-clic
					// rendait le champ muet ; Échap ou X referment, comme VSCode).
					// Une ÉDITION INLINE en cours a la priorité sur la saisie du filtre.
					if (mEditPath.Length() == 0 && mEditParent.Length() == 0) {
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

				// ── L'arbre : une ligne par row (chevron + icône + nom + badge git).
				//    `topY` = bas de l'en-tête fixe : pas d'interaction au-dessus. ──
				void DrawRows(NkGuiContext &ctx, float32 topY) {
					const float32 rowH = ctx.ItemHeight();
					const float32 fullW = ctx.ContentWidth();
					auto &dl = ctx.DL();
					const NkVec2 m = ctx.input.mousePos;
					const NkRect clip = dl.CurrentClip();
					// Peek ouvert : l'arbre ne réagit plus (le peek modal a la main).
					const bool inClip = NkGuiRectContains(clip, m) && m.y >= topY && !mPeekOpen;
					// Focus-clic du panneau : un clic DANS le panneau (header compris) le
					// prend, un clic ailleurs le rend — les raccourcis Ctrl+C/X/V, F2,
					// Suppr restent actifs après un clic sur la toolbar ou le menu.
					if (ctx.input.mouseClicked[0])
						mFocus = NkGuiRectContains(clip, m);
					const bool editing = mEditPath.Length() > 0 || mEditParent.Length() > 0;
					int32 toToggle = -1, toOpen = -1, toRename = -1;
					bool rowHit = false;
					if (mDragging)
						mDragOverDir.Clear(); // re-posée par la row dossier survolée ce frame
					// Fin de drag GLOBAL : une frame APRÈS le release (toutes les cibles —
					// éditeur, terminal… — ont pu consommer le release avant).
					if (mS->dragActive && !ctx.input.mouseDown[0] && !ctx.input.mouseReleased[0]) {
						mS->dragActive = false;
						mS->dragPaths.Clear();
					}
					for (usize i = 0; i < mRows.Size(); ++i) {
						const Row &r = mRows[i];
						const NkRect row = ctx.NextItemRect(fullW, rowH);
						if (row.y + rowH < clip.y || row.y > clip.y + clip.h)
							continue; // hors vue : la place est réservée, pas de dessin
						const bool hov = inClip && NkGuiRectContains(row, m);
						const bool sel = !r.editNew && IsSelected(r.path);
						const bool inEdit = r.editNew || (mEditPath.Length() > 0 && !r.editNew &&
														  SameStr(mEditPath.CStr(), r.path.CStr()));
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
						// Icône : dossier OUVERT/FERMÉ distincts ; fichier = registre ForFile,
						// sinon shaders/NKSL dédiés, sinon pastille « lettres du langage ».
						const float32 is = rowH - 9.f; // ~16 px : taille façon VSCode
						float32 iadv = is; // largeur réellement occupée (pastilles adaptatives)
						const NkRect ir = {x, cy - is * 0.5f, is, is};
						if (r.dir) {
							const NkColor tint = DirTint(r.name.CStr(), r.root);
							// 1) dossier SPÉCIAL Material (coloré, sans teinte) ; 2) dossier
							// Material générique ; 3) icône blanche maison teintée ; 4) trait.
							uint32 mtex = 0;
								if (mS->icons) {
									if (r.root) // racine du projet : icône DÉDIÉE
										mtex = r.open ? mS->icons->folderRootOpen : mS->icons->folderRoot;
									else
										mtex = mS->icons->ForDir(r.name.CStr(), r.open);
								}
							if (!mtex && mS->icons)
								mtex = r.open ? mS->icons->folderMOpen : mS->icons->folderM;
							const uint32 tex =
								mtex ? 0u : (!mS->icons ? 0u : (r.open ? mS->icons->folderOpen : mS->icons->folder));
							if (mtex)
								dl.AddImage(mtex, ir, {0.f, 0.f}, {1.f, 1.f}, {255, 255, 255, 255});
							else if (tex)
								dl.AddImage(tex, ir, {0.f, 0.f}, {1.f, 1.f}, tint);
							else if (r.open) { // repli au trait : dossier ouvert (rabat incliné)
								dl.AddRect({ir.x, ir.y + 2.f, ir.w - 2.f, ir.h - 4.f}, tint, 1.4f);
								dl.AddLine({ir.x, ir.y + ir.h - 2.f}, {ir.x + 3.f, ir.y + ir.h * 0.45f}, tint, 1.4f);
							} else { // dossier fermé : corps + languette
								dl.AddRect({ir.x, ir.y + 3.f, ir.w - 2.f, ir.h - 5.f}, tint, 1.4f);
								dl.AddLine({ir.x, ir.y + 3.f}, {ir.x + ir.w * 0.4f, ir.y + 1.f}, tint, 1.4f);
							}
						} else {
							char e[16];
							ExtOf(r.name.CStr(), e, sizeof(e));
							const uint32 tex = mS->icons ? mS->icons->ForFile(r.name.CStr()) : 0u;
							char lab[4] = {};
							NkColor bc = {150, 150, 150, 255};
							if (tex)
								dl.AddImage(tex, ir, {0.f, 0.f}, {1.f, 1.f}, {255, 255, 255, 255});
							else if (ExtIs(e, "nksl")) { // NKSL : pastille brandée (vert Nkentseu)
								dl.AddRectFilled(ir, {36, 61, 31, 255}, 3.f);
								dl.AddRect(ir, {120, 200, 120, 255}, 3.f);
								if (ctx.font && ctx.font->Valid())
									dl.AddText(ctx.font->Face(), ctx.font->TexId(),
											   {ir.x + 2.f, cy - ctx.font->LineHeight() * 0.5f + ctx.font->Ascent()},
											   "NK", {200, 216, 196, 255}, ir.w - 3.f);
							} else if (IsShaderExt(e)) { // shader : pastille violette + éclair (Eclaire.png)
								dl.AddRectFilled(ir, {70, 45, 110, 255}, 3.f);
								if (mS->icons && mS->icons->zap)
									dl.AddImage(mS->icons->zap, {ir.x + 1.f, ir.y + 1.f, ir.w - 2.f, ir.h - 2.f},
												{0.f, 0.f}, {1.f, 1.f}, {230, 200, 90, 255});
								else {
									const float32 mx = ir.x + ir.w * 0.5f;
									dl.AddLine({mx + 2.f, ir.y + 2.f}, {mx - 2.f, cy + 1.f}, {230, 200, 90, 255},
											   1.6f);
									dl.AddLine({mx - 2.f, cy + 1.f}, {mx + 1.f, cy + 1.f}, {230, 200, 90, 255}, 1.6f);
									dl.AddLine({mx + 1.f, cy + 1.f}, {mx - 2.f, ir.y + ir.h - 2.f},
											   {230, 200, 90, 255}, 1.6f);
								}
							} else if (LangBadge(e, lab, bc)) { // pastille lettres + couleur
								// Largeur ADAPTATIVE : « C++ » s'affiche en entier, la pastille
								// s'élargit plutôt que de tronquer le libellé.
								const float32 lw = (ctx.font && ctx.font->Valid()) ? ctx.font->MeasureWidth(lab) : is;
								const float32 bw = (lw + 5.f > is) ? lw + 5.f : is;
								iadv = bw;
								NkColor bg = bc;
								bg.r = static_cast<uint8>(bg.r / 4);
								bg.g = static_cast<uint8>(bg.g / 4);
								bg.b = static_cast<uint8>(bg.b / 4);
								dl.AddRectFilled({ir.x, ir.y, bw, ir.h}, bg, 3.f);
								if (ctx.font && ctx.font->Valid())
									dl.AddText(ctx.font->Face(), ctx.font->TexId(),
											   {ir.x + (bw - lw) * 0.5f,
												cy - ctx.font->LineHeight() * 0.5f + ctx.font->Ascent()},
											   lab, bc);
							} else if (mS->icons && mS->icons->fileText) { // inconnu : fichier générique grisé
								dl.AddImage(mS->icons->fileText, ir, {0.f, 0.f}, {1.f, 1.f}, {150, 150, 150, 255});
							} else { // repli au trait
								dl.AddRect({ir.x + 2.f, ir.y + 1.f, ir.w - 5.f, ir.h - 2.f}, bc, 1.3f);
								dl.AddLine({ir.x + 4.f, ir.y + 5.f}, {ir.x + ir.w - 6.f, ir.y + 5.f}, bc, 1.f);
								dl.AddLine({ir.x + 4.f, ir.y + 8.f}, {ir.x + ir.w - 6.f, ir.y + 8.f}, bc, 1.f);
							}
						}
						x += iadv + 4.f;
						// ── Champ d'ÉDITION INLINE (renommage / création) à la place du nom ──
						if (inEdit) {
							const NkRect er = {x, row.y + 1.f, row.x + fullW - x - 6.f, rowH - 2.f};
							mEditRect = er; // mémorisé : un clic HORS du champ VALIDE l'édition
							dl.AddRectFilled(er, ctx.theme.bgPrimary, 2.f);
							dl.AddRect(er, ctx.theme.accent, 2.f);
							if (ctx.font && ctx.font->Valid()) {
								dl.AddText(ctx.font->Face(), ctx.font->TexId(),
										   {er.x + 3.f, row.y + (rowH - ctx.font->LineHeight()) * 0.5f +
															ctx.font->Ascent()},
										   mEditBuf, ctx.theme.text, er.w - 6.f);
								if ((mTick / 30) % 2 == 0)
									dl.AddRectFilled({er.x + 3.f + ctx.font->MeasureWidth(mEditBuf) + 1.f,
													  er.y + 2.f, 1.5f, er.h - 4.f},
													 ctx.theme.accent);
							}
							continue; // pas de badge/clic sur la ligne en édition
						}
						// Nom (racine en circonflexe visuel : couleur pleine).
						if (ctx.font && ctx.font->Valid()) {
							// Modifié/ajouté/non-tracké : le NOM prend la couleur git
							// (dossiers contenant des modifs compris), façon VSCode.
							NkColor nc = r.git ? GitColor(r.git) : ctx.theme.text;
							if (r.git == 'D')
								nc.a = 120;
							dl.AddText(ctx.font->Face(), ctx.font->TexId(),
									   {x, row.y + (rowH - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent()},
									   r.name.CStr(), nc, row.x + fullW - x - 16.f);
						}
						// Badge Git coloré à droite (lettre ; les dossiers touchés : un point).
						if (r.git == '*') {
							dl.AddCircleFilled({row.x + fullW - 10.f, cy}, 2.5f, GitColor('*'));
						} else if (r.git && ctx.font && ctx.font->Valid()) {
							const char b[2] = {r.git, 0};
							dl.AddText(ctx.font->Face(), ctx.font->TexId(),
									   {row.x + fullW - 12.f,
										row.y + (rowH - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent()},
									   b, GitColor(r.git));
						}
						// Cible du DRAG & DROP : pendant un glissement, le dossier survolé
						// s'illumine et devient la destination.
						if (mDragging && r.dir && hov && !IsSelected(r.path)) {
							dl.AddRect(row, ctx.theme.accent, 1.5f);
							mDragOverDir = r.path;
						}
						// Drop OS : le dossier sous la POSITION du dépôt devient la cible.
						if (!mS->osDropPaths.Empty() && r.dir && !r.editNew &&
							NkGuiRectContains(row, {static_cast<float32>(mS->osDropX),
													static_cast<float32>(mS->osDropY)}))
							mOsDropDir = r.path;
						// Clic gauche : Ctrl = sélection multiple, Maj = plage ; sinon
						// dossier = plier/déplier, fichier = ouverture, RE-clic LENT sur
						// l'élément (seul) sélectionné = renommer. Le press ARME le drag.
						if (hov && ctx.input.mouseClicked[0] && !editing) {
							rowHit = true;
							mDragArmed = !r.editNew && !r.root;
							mDragging = false;
							mDragStart = m;
							if (ctx.input.ctrlDown) { // Ctrl+clic : bascule dans la sélection
								SelectToggle(r.path, static_cast<int32>(i));
								mSelTick = mTick;
							} else if (ctx.input.shiftDown && mSelAnchor >= 0) { // Maj+clic : plage
								SelectRange(mSelAnchor, static_cast<int32>(i));
								mSelTick = mTick;
							} else {
								const uint32 dt = mTick - mSelTick;
								const bool wasSolo = sel && mSelSet.Size() == 1;
								if (wasSolo && !r.root && dt > 30 && dt < 150)
									toRename = static_cast<int32>(i); // double-clic subtil
								else {
									SelectOnly(r.path, static_cast<int32>(i));
									mSelTick = mTick;
									if (r.dir)
										toToggle = static_cast<int32>(i);
									else
										toOpen = static_cast<int32>(i);
								}
							}
						}
						// Clic DROIT : garde la sélection multiple si la cible en fait
						// partie (le menu agit alors sur le groupe), sinon sélection simple.
						if (hov && ctx.input.mouseClicked[1] && !editing) {
							rowHit = true;
							if (!IsSelected(r.path))
								SelectOnly(r.path, static_cast<int32>(i));
							mSelTick = mTick;
							mCtx.open = true;
							mCtx.pos = m;
							mCtxPath = r.path;
							mCtxIsDir = r.dir;
							mCtxIsExtraRoot = r.extraRoot;
						}
					}
					// Clic droit dans le VIDE : menu sur la racine (création à la racine).
					if (inClip && ctx.input.mouseClicked[1] && !rowHit && !editing && mRootStr.Length() > 0) {
						mCtx.open = true;
						mCtx.pos = m;
						mCtxPath = mRootStr;
						mCtxIsDir = true;
						mCtxIsExtraRoot = false;
					}
					// ── Drop de fichiers depuis l'OS : COPIE dans le dossier sous la
					// position (posé par la boucle ci-dessus), sinon à la racine. ──
					if (!mS->osDropPaths.Empty() && mRootStr.Length() > 0 &&
						NkGuiRectContains(clip, {static_cast<float32>(mS->osDropX),
												 static_cast<float32>(mS->osDropY)})) {
						const NkString target = mOsDropDir.Length() > 0 ? mOsDropDir : mRootStr;
						for (usize di = 0; di < mS->osDropPaths.Size(); ++di) {
							const NkString &src = mS->osDropPaths[di];
							NkString dst = target;
							dst += "/";
							dst += NkPath(src).GetFileName();
							if (SameStr(dst.CStr(), src.CStr()))
								continue; // déjà à cet endroit
							if (NkFile::Exists(dst.CStr()))
								dst += " - copie"; // ne pas écraser
							CopyOrMove(src, dst, /*move=*/false); // source OS = COPIE
						}
						mS->osDropPaths.Clear();
						mS->osDropTtl = 0;
					}
					mOsDropDir.Clear(); // re-détecté à la prochaine frame de drop
					// ── DRAG & DROP : seuil de départ, fantôme, dépôt, annulation. ──
					if (mDragArmed && !mDragging && ctx.input.mouseDown[0]) {
						const float32 dx = m.x - mDragStart.x, dy = m.y - mDragStart.y;
						if (dx * dx + dy * dy > 25.f) {
							mDragging = true; // ~5 px : on glisse vraiment
							mS->dragActive = true; // drag GLOBAL : l'éditeur/terminal peuvent recevoir
							mS->dragPaths = mSelSet;
						}
					}
					if (mDragging) {
						if (ctx.input.KeyPressed(NkGuiKey::Escape)) { // Échap = annule
							mDragArmed = mDragging = false;
							mDragOverDir.Clear();
						} else { // fantôme « N élément(s) » près de la souris (overlay)
							const NkString gh = NkPrintf("%d", static_cast<int32>(mSelSet.Size()));
							auto &ov = ctx.dlOverlay;
							const NkRect gr = {m.x + 12.f, m.y + 8.f, 22.f, 18.f};
							ov.AddRectFilled(gr, ctx.theme.accent, 4.f);
							if (ctx.font && ctx.font->Valid())
								ov.AddText(ctx.font->Face(), ctx.font->TexId(),
										   {gr.x + 7.f, gr.y + 1.f + ctx.font->Ascent()}, gh.CStr(),
										   {255, 255, 255, 255});
						}
					}
					if (!ctx.input.mouseDown[0]) { // relâchement : dépôt éventuel
						if (mDragging && mDragOverDir.Length() > 0) {
							for (usize si = 0; si < mSelSet.Size(); ++si) {
								const NkString &src = mSelSet[si];
								// gardes : jamais dans soi-même/sa descendance, ni sur place
								bool inside = false;
								{
									const char *a = src.CStr();
									const char *b = mDragOverDir.CStr();
									bool pref = true;
									while (*a) {
										if (*b != *a) {
											pref = false;
											break;
										}
										++a;
										++b;
									}
									inside = pref && (*b == 0 || *b == '/' || *b == '\\');
								}
								if (inside || SameStr(ParentOf(src).CStr(), mDragOverDir.CStr()))
									continue;
								NkString dst = mDragOverDir;
								dst += "/";
								dst += NkPath(src).GetFileName();
								if (!NkFile::Exists(dst.CStr()))
									CopyOrMove(src, dst, /*move=*/true);
							}
							mSelSet.Clear();
							mSelPath.Clear();
						}
						mDragArmed = mDragging = false;
						mDragOverDir.Clear();
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
					else if (toRename >= 0)
						StartRename(mRows[toRename].path);
					// ── Saisie de l'ÉDITION INLINE (prioritaire sur tout le reste) ──
					if (editing) {
						int32 len = 0;
						while (mEditBuf[len])
							++len;
						for (int32 i = 0; i < ctx.input.charCount; ++i) {
							const uint32 cp = ctx.input.chars[i];
							// caractères interdits dans un nom de fichier
							if (cp >= 32 && cp < 127 && len < 126 && cp != '/' && cp != '\\' && cp != ':' &&
								cp != '*' && cp != '?' && cp != '"' && cp != '<' && cp != '>' && cp != '|')
								mEditBuf[len++] = static_cast<char>(cp);
						}
						if (ctx.input.KeyPressed(NkGuiKey::Backspace) && len > 0)
							--len;
						mEditBuf[len] = 0;
						if (ctx.input.KeyPressed(NkGuiKey::Enter))
							CommitEdit();
						else if (ctx.input.KeyPressed(NkGuiKey::Escape))
							CancelEdit();
						else if (ctx.input.mouseClicked[0] && !NkGuiRectContains(mEditRect, m))
							CommitEdit(); // clic HORS de la zone de saisie = VALIDE (façon VSCode)
						return;			  // pas de raccourcis pendant l'édition
					}
					// ── Raccourcis (focus-clic dans l'explorateur, hors filtre actif) ──
					if (mFocus && !mFilterOn && !mPeekOpen) {
						if (ctx.input.KeyPressed(NkGuiKey::Enter) && mSelPath.Length() > 0 && !SelIsDir())
							mS->OpenPath(NkPath(mSelPath));
						if (ctx.input.KeyPressed(NkGuiKey::F2) && mSelPath.Length() > 0)
							StartRename(mSelPath);
						// (Ajouter/retirer un dossier racine = menu contextuel : Ctrl+R est
						//  déjà la commande globale « jenga run ».)
						// Barre ESPACE : aperçu (peek) du fichier sélectionné.
						if (!mPeekOpen && mSelPath.Length() > 0 && !SelIsDir())
							for (int32 ci = 0; ci < ctx.input.charCount; ++ci)
								if (ctx.input.chars[ci] == 32) {
									OpenPeek(mSelPath);
									break;
								}
						if (ctx.input.KeyPressed(NkGuiKey::Delete) && !mSelSet.Empty())
							RequestDelete(ctx, mSelPath, SelIsDir()); // confirmation (groupe si multi)
						if (ctx.input.ctrlDown && ctx.input.KeyPressed(NkGuiKey::D) && !mSelSet.Empty()) {
							const NkVector<NkString> dup = mSelSet; // Ctrl+D : duplique TOUT le groupe
							for (usize si = 0; si < dup.Size(); ++si)
								if (!SameStr(dup[si].CStr(), mRootStr.CStr()))
									DuplicateOf(dup[si]);
						}
						if (ctx.input.wantCopy && !mSelSet.Empty()) { // Ctrl+C : copie interne (groupe)
							mClipPaths = mSelSet;
							mClipCut = false;
						}
						if (ctx.input.wantCut && !mSelSet.Empty()) { // Ctrl+X
							mClipPaths = mSelSet;
							mClipCut = true;
						}
						if (ctx.input.wantPaste && !mClipPaths.Empty()) { // Ctrl+V : colle le groupe
							const NkString tgt = TargetDir();
							for (usize ci = 0; ci < mClipPaths.Size(); ++ci) {
								const NkString &cp = mClipPaths[ci];
								// GARDE : jamais coller un dossier dans LUI-MÊME/sa descendance.
								bool inside = false;
								{
									const char *a = cp.CStr();
									const char *b = tgt.CStr();
									bool pref = true;
									while (*a) {
										if (*b != *a) {
											pref = false;
											break;
										}
										++a;
										++b;
									}
									inside = pref && (*b == 0 || *b == '/' || *b == '\\');
								}
								if (inside)
									continue;
								NkString dst = tgt;
								dst += "/";
								dst += NkPath(cp).GetFileName();
								if (SameStr(dst.CStr(), cp.CStr()))
									DuplicateOf(cp); // même dossier : « Nom - copie »
								else {
									if (NkFile::Exists(dst.CStr()))
										dst += " - copie"; // ne pas écraser l'existant
									CopyOrMove(cp, dst, mClipCut);
								}
							}
							if (mClipCut)
								mClipPaths.Clear();
						}
					}
				}

				// ── PEEK (barre Espace, façon VSCode/maquette) : ouvre l'aperçu. ──
				void OpenPeek(const NkString &p) {
					if (p.Length() == 0 || NkDirectory::Exists(p.CStr()))
						return;
					mPeekPath = p;
					mPeekOpen = true;
					mPeekJustOpened = true; // l'Espace d'OUVERTURE ne doit pas REFERMER (même frame)
					mPeekOff = {0.f, 0.f};	// recentre à chaque ouverture
					mPeekDrag = false;
					mPeekScroll = 0.f;
					mPeekSize = NkFile::GetFileSize(p.CStr());
					mPeekLines.Clear();
					const NkString txt = NkFile::ReadAllText(NkPath(p));
					NkString cur;
					int32 cnt = 0;
					for (const char *q = txt.CStr(); *q && cnt < 40; ++q) {
						if (*q == '\n') {
							mPeekLines.PushBack(cur);
							cur.Clear();
							++cnt;
						} else if (*q != '\r' && *q != '\t')
							cur += *q;
						else if (*q == '\t')
							cur += "    ";
					}
					if (!cur.Empty() && cnt < 40)
						mPeekLines.PushBack(cur);
					// Git : dernier commit + diff vs HEAD (une commande, async).
					mPeekCommit.Clear();
					mPeekDiff.Clear();
					char rel[1024];
					RelOf(p, rel, sizeof(rel));
					if (!mPeekGit.Running() && rel[0]) {
						NkString cmd = "git -C \"";
						cmd += mRootStr;
						cmd += "\" log -1 --format=@C%an ";
						cmd += "\xE2\x80\x94 %ar \xE2\x80\x94 %s -- \"";
						cmd += rel;
						cmd += "\" & git -C \"";
						cmd += mRootStr;
						cmd += "\" diff HEAD -- \"";
						cmd += rel;
						cmd += "\"";
						if (mPeekGit.Start(cmd))
							mPeekGitPending = true;
					}
				}

				void TickPeek() {
					if (mPeekGitPending && mPeekGit.Done()) {
						mPeekGitPending = false;
						NkVector<NkString> lines;
						mPeekGit.Drain(lines);
						for (usize i = 0; i < lines.Size(); ++i) {
							const char *l = lines[i].CStr();
							if (l[0] == '@' && l[1] == 'C')
								mPeekCommit = NkString(l + 2);
							else if (mPeekDiff.Size() < 60)
								mPeekDiff.PushBack(lines[i]);
						}
					}
				}

				// Aperçu : overlay centré — en-tête (nom/badge/chemin/taille + Ouvrir),
				// code COLORÉ (TokenizeLine), dernier commit, diff (+vert/-rouge).
				void DrawPeek(NkGuiContext &ctx) {
					if (!mPeekOpen)
						return;
					TickPeek();
					auto &ov = ctx.dlOverlay;
					// Le peek lit ctx.input DIRECTEMENT (même chemin éprouvé que DrawRows) :
					// hit-tests fiables. L'input est neutralisé pour les AUTRES panneaux
					// APRÈS le peek (dans OnUI).
					const nkgui::NkGuiInput &in = ctx.input;
					const float32 W = static_cast<float32>(ctx.viewW), H = static_cast<float32>(ctx.viewH);
					// Position = base centrée + décalage utilisateur (DÉPLAÇABLE par l'en-tête).
					const NkRect box = {W * 0.18f + mPeekOff.x, H * 0.12f + mPeekOff.y, W * 0.64f, H * 0.72f};
					ov.AddRectFilled({box.x + 2.f, box.y + 4.f, box.w, box.h}, {0, 0, 0, 70}, 8.f); // ombre douce
					ov.AddRectFilled(box, ctx.theme.panel, 8.f);
					// Bordure FINE, couleur ACCENT (bleu) : cadre représentatif (3e arg =
					// épaisseur, pas arrondi). Double trait pour un liseré net.
					ov.AddRect(box, ctx.theme.accent, 1.5f);
					ov.AddRectFilled({box.x, box.y, box.w, 3.f}, ctx.theme.accent, 8.f); // bandeau haut accent
					const float32 lh = (ctx.font && ctx.font->Valid()) ? ctx.font->LineHeight() : 16.f;
					const float32 asc = (ctx.font && ctx.font->Valid()) ? ctx.font->Ascent() : 12.f;
					float32 y = box.y + 10.f;
					const NkVec2 m = in.mousePos;
					if (ctx.font && ctx.font->Valid()) {
						// En-tête : nom  ·  chemin relatif  ·  taille — bouton Ouvrir.
						char rel[1024];
						RelOf(mPeekPath, rel, sizeof(rel));
						const NkString head = NkPrintf("%s   \xC2\xB7   %s   \xC2\xB7   %d o",
													   NkPath(mPeekPath).GetFileName().CStr(), rel,
													   static_cast<int32>(mPeekSize));
						ov.AddText(ctx.font->Face(), ctx.font->TexId(), {box.x + 14.f, y + asc}, head.CStr(),
								   ctx.theme.text, box.w - 120.f);
						// Bouton FERMER (✕) tout à droite.
						const NkRect xbr = {box.x + box.w - lh - 12.f, y - 3.f, lh + 8.f, lh + 8.f};
						const bool xbh = NkGuiRectContains(xbr, m);
						if (xbh)
							ov.AddRectFilled(xbr, ctx.theme.buttonHover, 4.f);
						{
							const float32 cx = xbr.x + xbr.w * 0.5f, cy = xbr.y + xbr.h * 0.5f, s = 4.f;
							const NkColor xc = xbh ? NkColor{230, 90, 90, 255} : ctx.theme.textDisabled;
							ov.AddLine({cx - s, cy - s}, {cx + s, cy + s}, xc, 1.6f);
							ov.AddLine({cx - s, cy + s}, {cx + s, cy - s}, xc, 1.6f);
						}
						if (xbh && in.mouseClicked[0])
							mPeekOpen = false;
						// Bouton Ouvrir (à gauche du ✕).
						const char *ob = NkT("peek.open");
						const float32 obw = ctx.font->MeasureWidth(ob) + 20.f;
						const NkRect obr = {xbr.x - obw - 6.f, y - 3.f, obw, lh + 8.f};
						const bool obh = NkGuiRectContains(obr, m);
						ov.AddRectFilled(obr, obh ? ctx.theme.buttonHover : ctx.theme.button, 4.f);
						ov.AddText(ctx.font->Face(), ctx.font->TexId(), {obr.x + 10.f, y + asc}, ob, ctx.theme.text);
						if (obh && in.mouseClicked[0]) {
							mS->OpenPath(NkPath(mPeekPath));
							mPeekOpen = false;
						}
						y += lh + 10.f;
						ov.AddRectFilled({box.x + 1.f, y, box.w - 2.f, 1.f}, ctx.theme.border);
						y += 6.f;
						// Code COLORÉ (mêmes couleurs que l'éditeur).
						const NkLang lg = NkLangFromExt(NkPath(mPeekPath).GetExtension().CStr());
						int32 blk = 0;
						const float32 codeBottom = box.y + box.h * 0.55f;
						for (usize i = 0; i < mPeekLines.Size() && y + lh < codeBottom; ++i) {
							const char *L = mPeekLines[i].CStr();
							const int32 n = static_cast<int32>(mPeekLines[i].Length());
							float32 x = box.x + 16.f;
							blk = TokenizeLine(lg, L, n, blk, ctx.syntax,
											   [&](int32 a, int32 b, const NkColor &col) {
												   char seg[512];
												   int32 sl = 0;
												   for (int32 k = a; k < b && sl < 511; ++k)
													   seg[sl++] = L[k];
												   seg[sl] = 0;
												   ov.AddText(ctx.font->Face(), ctx.font->TexId(), {x, y + asc}, seg,
															  col, box.x + box.w - 14.f - x);
												   x += ctx.font->MeasureWidth(seg);
											   });
							y += lh;
						}
						y = codeBottom + 4.f;
						ov.AddRectFilled({box.x + 1.f, y, box.w - 2.f, 1.f}, ctx.theme.border);
						y += 6.f;
						// Dernier commit.
						if (mPeekCommit.Length() > 0) {
							ov.AddText(ctx.font->Face(), ctx.font->TexId(), {box.x + 14.f, y + asc},
									   mPeekCommit.CStr(), ctx.theme.textDisabled, box.w - 28.f);
							y += lh + 4.f;
						}
						// Diff vs HEAD (tronqué) : + vert, - rouge, @@ accent.
						for (usize i = 0; i < mPeekDiff.Size() && y + lh < box.y + box.h - 8.f; ++i) {
							const char *L = mPeekDiff[i].CStr();
							NkColor col = ctx.theme.textDisabled;
							if (L[0] == '+')
								col = {63, 185, 80, 255};
							else if (L[0] == '-')
								col = {248, 81, 73, 255};
							else if (L[0] == '@')
								col = ctx.theme.accent;
							ov.AddText(ctx.font->Face(), ctx.font->TexId(), {box.x + 14.f, y + asc}, L, col,
									   box.w - 28.f);
							y += lh;
						}
					}
					// ── DÉPLACEMENT par l'en-tête (bande du haut, hors boutons à droite). ──
					const NkRect headerBar = {box.x, box.y, box.w - 130.f, lh + 12.f};
					if (in.mouseClicked[0] && NkGuiRectContains(headerBar, m)) {
						mPeekDrag = true;
						mPeekDragOff = {m.x - box.x, m.y - box.y};
					}
					if (mPeekDrag && in.mouseDown[0]) {
						mPeekOff.x = (m.x - mPeekDragOff.x) - W * 0.18f;
						mPeekOff.y = (m.y - mPeekDragOff.y) - H * 0.12f;
					}
					if (!in.mouseDown[0])
						mPeekDrag = false;
					// ── Fermeture UNIQUEMENT explicite : bouton ✕ (ci-dessus), Échap, ou
					//    Espace à nouveau. PAS de fermeture au clic (évite toute fermeture
					//    intempestive : un clic dans le panneau ne le referme jamais). ──
					const bool spaceAgain = [&] {
						for (int32 i = 0; i < in.charCount; ++i)
							if (in.chars[i] == 32)
								return true;
						return false;
					}();
					if (mPeekJustOpened) // l'Espace d'ouverture ne referme pas
						mPeekJustOpened = false;
					else if (in.KeyPressed(NkGuiKey::Escape) || spaceAgain)
						mPeekOpen = false;
				}

				// ── Menu contextuel (clic droit) : actions sur la cible. ──
				void DrawCtxMenu(NkGuiContext &ctx) {
					if (!mCtx.open)
						return;
					// Fichiers/dossiers (0-9) + GIT & IA (10-15) : ces derniers sont des
					// PLACEHOLDERS — présents dans le menu, comportement défini plus tard.
					const char *items[18] = {NkT("exp.ctx.newfile"),  NkT("exp.ctx.newfolder"),
											 NkT("exp.ctx.rename"),	  NkT("exp.ctx.delete"),
											 NkT("exp.ctx.dup"),	  NkT("exp.ctx.copypath"),
											 NkT("exp.ctx.copyrel"),  NkT("exp.ctx.reveal"),
											 NkT("exp.ctx.term"),	  NkT("exp.ctx.gitignore"),
											 NkT("exp.ctx.compare"),  NkT("exp.ctx.blame"),
											 NkT("exp.ctx.discard"),  NkT("exp.ctx.aigen"),
											 NkT("exp.ctx.aitest"),	  NkT("exp.ctx.props"),
											 NkT("exp.addroot"),	  NkT("exp.removeroot")};
					bool en[18];
					for (int32 i = 0; i < 18; ++i)
						en[i] = true;
					const bool isMainRoot = SameStr(mCtxPath.CStr(), mRootStr.CStr());
					const bool isExtraRoot = mCtxIsExtraRoot;
					if (isMainRoot || isExtraRoot) // pas de rename/suppr/dup/gitignore d'une racine
						en[2] = en[3] = en[4] = en[9] = false;
					if (mCtxIsDir) // git compare/blame/discard + IA = fichiers uniquement
						en[10] = en[11] = en[12] = en[13] = en[14] = false;
					en[16] = isMainRoot || isExtraRoot; // « Ajouter un dossier » : sur une racine
					en[17] = isExtraRoot;				// « Retirer du workspace » : racine secondaire
					// Cache les 2 derniers si non pertinents (les met en fin, désactivés).
					const int32 nItems = (isMainRoot || isExtraRoot) ? 18 : 16;
					const int32 act = NkCtxMenuDraw(ctx, mCtx, items, en, nItems);
					if (act < 0)
						return;
					const NkString p = mCtxPath;
					const bool dir = mCtxIsDir;
					const NkString parent = dir ? p : ParentOf(p);
					switch (act) {
						case 0:
							StartCreate(parent, false);
							break;
						case 1:
							StartCreate(parent, true);
							break;
						case 2:
							StartRename(p);
							break;
						case 3:
							RequestDelete(ctx, p, dir); // confirmation avant la corbeille
							break;
						case 4:
							DuplicateOf(p);
							break;
						case 5:
							ctx.SetClipboard(p.CStr());
							break;
						case 6: {
							char rel[1024];
							RelOf(p, rel, sizeof(rel));
							ctx.SetClipboard(rel);
						} break;
						case 7:
							RevealInOS(p);
							break;
						case 8: // terminal intégré dans ce dossier (shell par défaut)
							mS->termOpenAt = parent;
							mS->termOpenKind = -1;
							break;
						case 9:
							AddToGitignore(p);
							break;
						case 10: // Comparer avec HEAD  (à définir)
						case 11: // Blame Git           (à définir)
						case 12: // Annuler les modifs  (à définir)
						case 13: // IA → Générer         (à définir)
						case 14: // IA → Créer des tests (à définir)
							mS->status = NkString(NkT("exp.ctx.soon")); // fonctionnalité à venir
							break;
						case 15: { // Propriétés : infos basiques dans la barre d'état
							const int64 sz = NkFile::GetFileSize(p.CStr());
							mS->status = NkPrintf("%s \xE2\x80\x94 %d o", NkPath(p).GetFileName().CStr(),
												  static_cast<int32>(sz));
						} break;
						case 16: // Ajouter un dossier au workspace (racine secondaire)
							AddRootFolder();
							break;
						case 17: // Retirer du workspace (racine secondaire)
							RemoveRootFolder(p);
							break;
						default:
							break;
					}
				}

				NkCodeState *mS = nullptr;
				NkVector<Row> mRows;
				NkVector<NkString> mExpanded;
				NkVector<NkString> mExtraRoots; ///< racines secondaires (multi-racines)
				bool mCtxIsExtraRoot = false;  ///< la cible du menu est une racine secondaire
				NkString mRootStr, mSelPath;
				// ── Sélection MULTIPLE (mSelPath = élément principal/dernier cliqué) ──
				NkVector<NkString> mSelSet;
				int32 mSelAnchor = -1; ///< row de l'ancre (Maj+clic = plage)
				// ── Drag & drop interne (déplacer la sélection sur un dossier) ──
				bool mDragArmed = false, mDragging = false;
				NkVec2 mDragStart{0.f, 0.f};
				NkString mDragOverDir;
				NkString mOsDropDir; ///< dossier sous la position d'un drop OS (frame courante)
				bool mDelGroup = false; ///< la confirmation vise toute la sélection
				bool mRowsDirty = true, mRootOpen = true;
				bool mFilterOn = false, mShowExcluded = false, mFocus = false;
				char mFilter[64] = {};
				uint32 mTick = 0;
				// Git async
				NkProcess mGit;
				NkVector<NkString> mGitPath;
				NkVector<char> mGitCode;
				uint32 mGitNext = 0;
				bool mGitParsed = true; ///< résultat du run courant déjà consommé ?
				// ── Tranche 2 : menu contextuel + édition inline + opérations fichiers ──
				NkCtxMenu mCtx;			///< menu clic droit
				NkString mCtxPath;		///< cible du menu
				bool mCtxIsDir = false;
				NkString mEditPath;		///< row en RENOMMAGE inline (vide = aucun)
				NkString mEditParent;	///< création inline : dossier parent (vide = aucune)
				bool mEditCreateDir = false;
				char mEditBuf[128] = {};
				NkVector<NkString> mClipPaths; ///< copier/couper interne (Ctrl+C/X/V, groupe)
				bool mClipCut = false;
				uint32 mSelTick = 0;	///< frame de la dernière sélection (clic-lent = renommer)
				NkCtxMenu mDelMenu;		///< confirmation de suppression
				NkString mDelPath;
				bool mDelIsDir = false;
				NkString mDelLabel;
				// ── PEEK (barre Espace) : aperçu coloré + métadonnées + diff vs HEAD ──
				bool mPeekOpen = false;
				NkString mPeekPath;
				NkVector<NkString> mPeekLines; ///< premières lignes du fichier
				int64 mPeekSize = 0;
				NkProcess mPeekGit;			  ///< log -1 + diff HEAD (async)
				NkString mPeekCommit;		  ///< « auteur — date — sujet »
				NkVector<NkString> mPeekDiff; ///< lignes du diff (tronquées)
				bool mPeekGitPending = false;
				bool mPeekJustOpened = false; ///< frame d'ouverture : l'Espace ne referme pas
				nkgui::NkGuiInput mPeekInput; ///< input RÉEL (l'overlay le lit, le reste est masqué)
				NkVec2 mPeekOff{0.f, 0.f};	  ///< décalage de position (déplacement par l'en-tête)
				NkVec2 mPeekDragOff{0.f, 0.f};
				bool mPeekDrag = false;
				float32 mPeekScroll = 0.f;
				NkRect mEditRect = {0.f, 0.f, 0.f, 0.f}; ///< zone de saisie inline (clic hors = valide)
		};

	} // namespace nkcode
} // namespace nkentseu
