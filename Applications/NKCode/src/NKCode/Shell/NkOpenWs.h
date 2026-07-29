#pragma once
// =============================================================================
// NkOpenWs.h — Vue « Ouvrir un Workspace » du launcher : navigateur de fichiers
// custom (dessine entierement en NKGui). D'apres le design Banani
// « Launcher — Ouvrir un Workspace » + la reference visuelle (screenshot).
//
// Mode PLEIN CADRE du launcher : meme barre de titre + meme sidebar nav (item
// « Ouvrir » actif) ; le panneau central Home est remplace par ce navigateur.
//
#include "NKEditorKit/NkDirBrowser.h"
#include "NKEditorKit/NkEditorScrollbar.h" // scrollbar general // NkDirBrowserState : navigation reutilisable
		// Coeur fonctionnel : listing reel d'un dossier, detection des .jenga + du
// workspace declare (`with workspace("NOM")`), navigation (fil d'Ariane / parent
// / historique / double-clic), recherche, ouverture du dossier (DoLoad).
// Extras de la spec (tooltip apercu, favoris drag, vue grille, saisie libre +
// autocompletion, raccourcis complets) : a faire APRES ce coeur.
//
// L'etat (NkOpenWsState) vit dans NkHomeState ; le panneau renvoie une action
// (0 = rien, 1 = annuler -> retour Home) pour eviter toute dependance circulaire.
// =============================================================================
#include "NKCode/Shell/NkUi.h"
#include "NKCode/Shell/NkI18n.h" // NkT() : traductions multi-langue
#include "NKCode/Project/NkCodeState.h"
#include "NKCode/Shell/Dialogs.h"
#include "NKContainers/String/NkFormat.h" // NkPrintf (formatage maison, ex-<cstdio>)
#include "NKPlatform/NkEnv.h"			  // env::GetEnvVar (variables d'environnement maison, ex-<cstdlib>)

namespace nkentseu {
	namespace nkcode {

		struct NkOwEntry {
				NkString name;
				bool isDir = false;
				bool isJenga = false;	   // fichier .jenga
				bool hasWorkspace = false; // .jenga avec `with workspace(...)`
				NkString workspace;		   // nom du workspace declare
				int64 mtime = 0;		   // epoch s
				NkString ageStr;		   // date « il y a X » CALCULEE AU SCAN (pas chaque frame)
				int64 size = 0;			   // taille en octets (fichiers)
				int32 projCount = 0;	   // nombre de projets declares (.jenga)
				int32 jengaKind = 0;	   // 0 aucun/vide, 1 workspace, 2 projet, 3 config
				bool isWsDir = false;	   // DOSSIER contenant un .jenga qui declare un workspace
				NkString wsFile;		   // chemin du .jenga workspace (fichier direct ou dans le dossier)
				NkString sizeStr, typeStr; // chaines pretes a afficher (figees au scan)
		};

		struct NkOpenWsState : public NkDirBrowserState {
				NkVector<NkOwEntry> entries;
				char search[64] = {};
				bool focusSearch = false;
				int32 viewMode = 0; // 0 liste, 1 grille (grille = TODO)
				float32 scrollMax = 0.f;
				int32 jengaCount = 0, wsCount = 0, firstWsIdx = -1;
				float32 dblTimer = 0.f;
				int32 dblIdx = -1; // detection double-clic
				int32 sortBy = 0;  // 0 = Nom, 1 = Modifie
				bool sortAsc = true;
				bool barDrag = false;
				float32 barOff = 0.f; // scrollbar verticale
				bool hbarDrag = false;
				float32 hbarOff = 0.f; // scrollbar horizontale (grille)
				float32 hscroll = 0.f, hscrollMax = 0.f;
				// — saisie libre du chemin (barre haut/bas) —
				bool pathEdit = false;
				bool pathEditTop = false;
				char pathBuf[600] = {};
				// — autocompletion du chemin (TAB + dropdown de dossiers) —
				NkVector<NkString> dirCache;
				char cacheParent[600] = {}; // listing du dossier parent (cache)
				NkVector<int32> suggIdx;	// indices de dirCache filtrant le partiel courant
				NkString suggParent;		// chemin parent (avec '/' final)
				int32 suggSel = -1;			// selection clavier dans le dropdown
				// — renommage inline (double-clic lent / menu) —
				char renameBuf[256] = {};
				// — editeur de texte partage (rename + barres de chemin) : caret + clignotement —
				int32 editCaret = 0;
				float32 editBlink = 0.f;
				bool showHidden = false; // fichiers/dossiers « . » masques par defaut
				bool showJenga = true, showDirs = true,
					 showAll = true; // filtres ; « Tous les fichiers » coche par defaut
				// — tooltip d'apercu .jenga (survol prolonge) —
				int32 hoverIdx = -1, hoverPrev = -1;
				float32 hoverX = 0.f, hoverY = 0.f, hoverTimer = 0.f;

				// — Favoris (persistes) + leur menu/renommage + drag-to-add —
				struct Fav {
						NkString path, alias;
				};

				NkVector<Fav> favs;
				bool favsLoaded = false;
				int32 favMenuIdx = -1;
				float32 favMenuX = 0.f, favMenuY = 0.f; // menu contextuel favori
				int32 favRename = -1;
				char favBuf[256] = {}; // renommage d'un favori
				bool dragging = false;
				NkString dragPath;
				float32 dragX0 = 0.f, dragY0 = 0.f;		   // glisser un dossier -> favoris
				float32 sbScroll = 0.f, sbScrollMax = 0.f; // defilement du panneau gauche
				float32 slowClk = 0.f;
				int32 slowIdx = -1; // double-clic lent -> renommer
				// — menu contextuel (clic droit) : -1 ferme, >=0 sur entree, -2 zone vide —
				float32 menuX = 0.f, menuY = 0.f;
				// — confirmation de suppression —
				



				// Dossier connu : preferer la redirection OneDrive si elle existe (sinon Explorer
				// montre plus de fichiers que le ~/Desktop local, qui est vide/perime).



				// Lit une valeur brute (key=value) dans ~/.nkcode/settings.cfg. Vide si absente.
				// Permet à New Workspace / Clone de respecter les chemins par défaut choisis dans les Paramètres,
				// sans dépendance circulaire vers NkSettingsState.
				static NkString ReadNkSetting(const char *key) {
					const NkString path = (NkPath(Home().CStr()) / ".nkcode" / "settings.cfg").ToString();
					if (!NkFile::Exists(path.CStr()))
						return NkString();
					const NkString js = NkFile::ReadAllText(NkPath(path.CStr()));
					NkString line;
					for (const char *s = js.CStr();; ++s) {
						if (*s == '\n' || *s == '\0') {
							const char *eq = nullptr;
							for (const char *p = line.CStr(); *p; ++p)
								if (*p == '=') {
									eq = p;
									break;
								}
							if (eq) {
								NkString k;
								for (const char *p = line.CStr(); p < eq; ++p)
									k += *p;
								if (StrEq(k.CStr(), key)) {
									NkString v;
									for (const char *p = eq + 1; *p; ++p)
										if (*p != '\r')
											v += *p;
									return v;
								}
							}
							line = NkString();
							if (*s == '\0')
								break;
						} else if (*s != '\r')
							line += *s;
					}
					return NkString();
				}

				// Dossier d'enregistrement par défaut des workspaces : réglage utilisateur (projDir) sinon ~/Projects.
				static NkString DefaultProjectDir() {
					const NkString v = ReadNkSetting("projDir");
					if (!v.Empty())
						return v;
					return (NkPath(Home().CStr()) / "Projects").ToString();
				}

				// Dossier de config Jenga (~/.jenga, ou override JENGA_CONFIG_DIR / JENGA_HOME — aligné sur
				// JengaConfig.py).
				static NkString JengaConfigDir() {
					const char *ov = env::GetEnvVar("JENGA_CONFIG_DIR");
					if (!ov || !*ov)
						ov = env::GetEnvVar("JENGA_HOME");
					if (ov && *ov)
						return NkString(ov);
					return (NkPath(Home().CStr()) / ".jenga").ToString();
				}

				// Dossier de cache Jenga : <config>/cache.
				static NkString JengaCacheDir() {
					return (NkPath(JengaConfigDir().CStr()) / "cache").ToString();
				}

				// Dossier de l'exécutable NKCode (renseigné par main.cpp au démarrage) — pour trouver le Jenga
				// EMBARQUÉ.
				static NkString &ExeDir() {
					static NkString d;
					return d;
				}

				// Chemin RÉEL du binaire Jenga :
				//  1) override JENGA_EXE,
				//  2) Jenga EMBARQUÉ à côté de NKCode : <exe>/tools/jenga/jenga(.exe|.cmd)  (intégration future, zéro
				//  install Python), 3) sinon "jenga" du PATH (résolu en chemin complet si possible).
				static NkString DefaultJengaPath() {
					const char *ov = env::GetEnvVar("JENGA_EXE");
					if (ov && *ov && NkFile::Exists(ov))
						return NkString(ov);
					const NkString ed = ExeDir();
					if (!ed.Empty()) {
						const char *cand[] = {
#if defined(_WIN32)
							"tools/jenga/jenga.exe",
							"tools/jenga/jenga.cmd",
							"jenga/jenga.exe",
#else
							"tools/jenga/jenga",
							"jenga/jenga",
#endif
						};
						for (const char *rel : cand) {
							const NkString p = (NkPath(ed.CStr()) / rel).ToString();
							if (NkFile::Exists(p.CStr()))
								return p;
						}
					}
					// PATH : renvoie le chemin complet si trouvé, sinon le littéral "jenga" (exécutable via PATH).
					const NkString w = NkNewWsState_TcWhich_jenga();
					return w.Empty() ? NkString("jenga") : w;
				}

				// Petit wrapper (NkNewWsState n'est pas encore visible ici) : cherche "jenga" dans le PATH.
				static NkString NkNewWsState_TcWhich_jenga() {
					const char *path = env::GetEnvVar("PATH");
					if (!path)
						return NkString();
#if defined(_WIN32)
					const char sep = ';';
					const char *exts[] = {".exe", ".cmd", ".bat", ""};
#else
					const char sep = ':';
					const char *exts[] = {""};
#endif
					NkString dir;
					auto tryDir = [&](const NkString &d) -> NkString {
						for (const char *e : exts) {
							NkString name = "jenga";
							name += e;
							NkString f = (NkPath(d.CStr()) / name.CStr()).ToString();
							if (NkFile::Exists(f.CStr()))
								return f;
						}
						return NkString();
					};
					for (const char *c = path;; ++c) {
						if (*c == sep || *c == '\0') {
							if (!dir.Empty()) {
								const NkString r = tryDir(dir);
								if (!r.Empty())
									return r;
							}
							dir = NkString();
							if (*c == '\0')
								break;
						} else
							dir += *c;
					}
					return NkString();
				}

				// Taille lisible (octets -> « 12.3 Ko », « 1.4 Mo »…).
				static NkString HumanSize(int64 b) {
					if (b < 0)
						return NkString();
					const char *unit[] = {"o", "Ko", "Mo", "Go", "To"};
					double v = (double)b;
					int32 u = 0;
					while (v >= 1024.0 && u < 4) {
						v /= 1024.0;
						++u;
					}
					if (u == 0)
						return NkPrintf("%lld o", (long long)b); // NkPrintf maison
					return NkPrintf("%.1f %s", v, unit[u]);
				}

				// Extension en MAJUSCULES (sans le point), pour la colonne Type des fichiers.
				static NkString ExtUpper(const char *name) {
					const char *dot = nullptr;
					for (const char *p = name; *p; ++p)
						if (*p == '.')
							dot = p;
					if (!dot || !dot[1])
						return NkString();
					NkString e;
					for (const char *p = dot + 1; *p; ++p)
						e += Upc(*p);
					return e;
				}

				// Applique la saisie clavier (texte + backspace + collage Ctrl+V) a un buffer.
				static void ApplyInput(const NkUi &u, char *buf, int32 cap) {
					if (u.ctx->input.KeyPressedRepeat(NkGuiKey::Backspace)) {
						int32 n = 0;
						while (buf[n])
							++n;
						if (n > 0)
							buf[n - 1] = '\0';
					}
					if (u.ctx->input.wantPaste) {
						NkString c = u.ctx->GetClipboard();
						int32 n = 0;
						while (buf[n])
							++n;
						for (const char *s = c.CStr(); *s && n + 1 < cap; ++s) {
							if ((unsigned char)*s >= 32)
								buf[n++] = *s;
						}
						buf[n] = '\0';
						u.ctx->input.wantPaste = false;
					}
					for (int32 i = 0; i < u.ctx->input.charCount; ++i) {
						const uint32 cp = u.ctx->input.chars[i];
						if (cp >= 32 && cp < 127) {
							int32 n = 0;
							while (buf[n])
								++n;
							if (n + 1 < cap) {
								buf[n] = (char)cp;
								buf[n + 1] = '\0';
							}
						}
					}
				}

				// ── Operations fichiers ──
				void BeginRename(int32 idx) {
					if (idx < 0 || idx >= (int32)entries.Size())
						return;
					renameIdx = idx;
					slowIdx = -1;
					const char *s = entries[idx].name.CStr();
					int32 n = 0;
					while (s[n] && n + 1 < (int32)sizeof(renameBuf)) {
						renameBuf[n] = s[n];
						++n;
					}
					renameBuf[n] = '\0';
					editCaret = n;
					editBlink = 0.f;
				}

				// Passe une barre de chemin (haut/bas) en saisie libre, caret en fin.
				void BeginPathEdit(bool top) {
					pathEdit = true;
					pathEditTop = top;
					int32 n = 0;
					for (; curDir[n] && n + 1 < (int32)sizeof(pathBuf); ++n)
						pathBuf[n] = curDir[n];
					pathBuf[n] = '\0';
					editCaret = n;
					editBlink = 0.f;
					suggSel = -1;
					cacheParent[0] = '\0';
				}

				// Pose un nouveau contenu dans pathBuf (caret en fin) — utilise par l'autocompletion.
				void SetPathBuf(const char *s) {
					int32 n = 0;
					for (; s[n] && n + 1 < (int32)sizeof(pathBuf); ++n)
						pathBuf[n] = s[n];
					pathBuf[n] = '\0';
					editCaret = n;
					editBlink = 0.f;
				}

				// Recalcule les suggestions de dossiers a partir de pathBuf (parent + partiel).
				void RefreshSugg() {
					int32 n = 0;
					while (pathBuf[n])
						++n;
					int32 slash = -1;
					for (int32 k = 0; k < n; ++k)
						if (pathBuf[k] == '/' || pathBuf[k] == '\\')
							slash = k;
					char parent[600];
					int32 pl = 0;
					for (int32 k = 0; k <= slash && pl < 599; ++k)
						parent[pl++] = pathBuf[k];
					parent[pl] = '\0'; // inclut le '/' final (ou vide si pas de slash)
					const char *partial = (slash >= 0) ? pathBuf + slash + 1 : pathBuf;
					suggParent = parent;
					if (!StrEq(parent, cacheParent)) { // (re)lister le parent seulement s'il change
						int32 c = 0;
						for (; parent[c] && c < 599; ++c)
							cacheParent[c] = parent[c];
						cacheParent[c] = '\0';
						dirCache.Clear();
						if (parent[0] && NkDirectory::Exists(parent)) {
							NkVector<NkDirectoryEntry> es =
								NkDirectory::GetEntries(NkPath(parent), "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
							for (usize i = 0; i < es.Size(); ++i)
								if (es[i].IsDirectory && !es[i].IsHidden && !StrEq(es[i].Name.CStr(), ".") &&
									!StrEq(es[i].Name.CStr(), ".."))
									dirCache.PushBack(es[i].Name);
						}
					}
					suggIdx.Clear();
					for (usize i = 0; i < dirCache.Size(); ++i)
						if (NkCodeState::StartsWithI(dirCache[i].CStr(), partial))
							suggIdx.PushBack((int32)i);
					if (suggSel >= (int32)suggIdx.Size())
						suggSel = -1;
				}

				// Applique un dossier choisi (clic / Entree dropdown) : parent + nom + '/'.
				void ApplySugg(int32 cacheI) {
					if (cacheI < 0 || cacheI >= (int32)dirCache.Size())
						return;
					NkString full = suggParent;
					full += dirCache[cacheI].CStr();
					full += "/";
					SetPathBuf(full.CStr());
					suggSel = -1;
					cacheParent[0] = '\0'; // force un re-listing du nouveau parent
				}

				// TAB : complete au plus long prefixe commun ; si une seule correspondance -> complet + '/'.
				void CompleteSugg() {
					if (suggIdx.Empty())
						return;
					if (suggIdx.Size() == 1) {
						ApplySugg(suggIdx[0]);
						return;
					}
					// plus long prefixe commun (insensible casse, casse du 1er conservee)
					NkString lcp = dirCache[suggIdx[0]];
					for (usize k = 1; k < suggIdx.Size(); ++k) {
						const char *b = dirCache[suggIdx[k]].CStr();
						int32 j = 0;
						while (j < (int32)lcp.Length() && b[j] && NkCodeState::UpC(lcp[j]) == NkCodeState::UpC(b[j]))
							++j;
						lcp = lcp.SubStr(0, j);
					}
					NkString full = suggParent;
					full += lcp.CStr();
					SetPathBuf(full.CStr());
				}

				void CommitRename() {
					if (renameIdx < 0 || renameIdx >= (int32)entries.Size()) {
						renameIdx = -1;
						return;
					}
					if (renameBuf[0] && !StrEq(renameBuf, entries[renameIdx].name.CStr())) {
						const NkString src = (NkPath(curDir) / entries[renameIdx].name.CStr()).ToString();
						const NkString dst = (NkPath(curDir) / renameBuf).ToString();
						const bool ok = entries[renameIdx].isDir ? NkDirectory::Move(src.CStr(), dst.CStr())
																 : NkFile::Move(src.CStr(), dst.CStr());
						if (!ok)
							status = NkT("ow.st.renamefail");
						scanned = false;
					}
					renameIdx = -1;
				}

				void NewFolder() {
					NkString name = NkT("ow.st.newfolder");
					int32 k = 2;
					while (NkDirectory::Exists((NkPath(curDir) / name.CStr()).ToString().CStr())) {
						name = NkPrintf("Nouveau dossier (%d)", k++); // NkPrintf maison
					}
					if (NkDirectory::CreateRecursive((NkPath(curDir) / name.CStr()).ToString().CStr())) {
						Rescan();
						for (usize i = 0; i < entries.Size(); ++i)
							if (StrEq(entries[i].name.CStr(), name.CStr())) {
								selected = (int32)i;
								BeginRename((int32)i);
								break;
							}
					} else
						status = NkT("ow.st.createfail");
				}

				void DeleteEntry(int32 idx) {
					confirmDel = -1;
					if (idx < 0 || idx >= (int32)entries.Size())
						return;
					const bool dir = entries[idx].isDir;
					const NkString p = (NkPath(curDir) / entries[idx].name.CStr()).ToString();
					bool ok = dir ? NkDirectory::MoveToTrash(p.CStr())
								  : NkFile::MoveToTrash(p.CStr()); // corbeille (annulable)
					if (!ok)
						ok = dir ? NkDirectory::Delete(p.CStr(), true) : NkFile::Delete(p.CStr()); // repli : definitif
					if (!ok)
						status = NkT("ow.st.deletefail");
					scanned = false;
				}

				// ── Favoris : persistance dans ~/.nkcode/favorites.txt (une ligne « chemin<TAB>alias ») ──
				static NkString FavFile() {
					return (NkPath(Home().CStr()) / ".nkcode" / "favorites.txt").ToString();
				}

				void LoadFavs() {
					favs.Clear();
					favsLoaded = true;
					const NkString f = FavFile();
					if (!NkFile::Exists(f.CStr()))
						return;
					const NkString txt = NkFile::ReadAllText(NkPath(f));
					NkString line;
					for (const char *p = txt.CStr();; ++p) {
						if (*p == '\n' || *p == '\0') {
							if (!line.Empty()) {
								Fav fv;
								const char *s = line.CStr();
								int32 tab = -1;
								for (int32 k = 0; s[k]; ++k)
									if (s[k] == '\t') {
										tab = k;
										break;
									}
								if (tab >= 0) {
									for (int32 k = 0; k < tab; ++k)
										fv.path += s[k];
									for (int32 k = tab + 1; s[k]; ++k)
										fv.alias += s[k];
								} else
									fv.path = line;
								if (!fv.path.Empty()) {
									if (fv.alias.Empty())
										fv.alias = NkPath(fv.path.CStr()).GetFileName();
									favs.PushBack(fv);
								}
							}
							line.Clear();
							if (*p == '\0')
								break;
						} else if (*p != '\r')
							line += *p;
					}
				}

				void SaveFavs() {
					NkDirectory::CreateRecursive((NkPath(Home().CStr()) / ".nkcode").ToString().CStr());
					NkString out;
					for (usize i = 0; i < favs.Size(); ++i) {
						out += favs[i].path;
						out += "\t";
						out += favs[i].alias;
						out += "\n";
					}
					NkFile::WriteAllText(NkPath(FavFile()), out);
				}

				bool IsFav(const char *path) const {
					for (usize i = 0; i < favs.Size(); ++i)
						if (StrEq(favs[i].path.CStr(), path))
							return true;
					return false;
				}

				void AddFav(const char *path) {
					if (!favsLoaded)
						LoadFavs();
					if (!path || !*path || IsFav(path))
						return;
					Fav fv;
					fv.path = path;
					fv.alias = NkPath(path).GetFileName();
					if (fv.alias.Empty())
						fv.alias = path;
					favs.PushBack(fv);
					SaveFavs();
				}

				void RemoveFav(int32 idx) {
					if (idx < 0 || idx >= (int32)favs.Size())
						return;
					favs.Erase(favs.Begin() + idx);
					SaveFavs();
					favMenuIdx = -1;
				}

				void BeginFavRename(int32 idx) {
					if (idx < 0 || idx >= (int32)favs.Size())
						return;
					favRename = idx;
					const char *s = favs[idx].alias.CStr();
					int32 n = 0;
					while (s[n] && n + 1 < (int32)sizeof(favBuf)) {
						favBuf[n] = s[n];
						++n;
					}
					favBuf[n] = '\0';
					editCaret = n;
					editBlink = 0.f;
					favMenuIdx = -1;
				}

				void CommitFavRename() {
					if (favRename >= 0 && favRename < (int32)favs.Size() && favBuf[0]) {
						favs[favRename].alias = favBuf;
						SaveFavs();
					}
					favRename = -1;
				}

				void EnsureInit(NkCodeState *st) {
					if (!favsLoaded)
						LoadFavs();
					if (curDir[0])
						return;
					NkString start = (st && st->HasWorkspace()) ? st->root.ToString() : Home();
					SetDir(start.CStr(), true);
				}


				void NavInto(const NkOwEntry &e) {
					SetDir((NkPath(curDir) / e.name.CStr()).ToString().CStr(), true);
				}






				bool HasError() const {
					return jengaCount == 0 || wsCount == 0;
				}

				// Filtres de la rangee d'options (les « . » sont deja exclus au scan via showHidden).
				bool Pass(const NkOwEntry &e) const {
					return e.isDir ? showDirs : (e.isJenga ? showJenga : showAll);
				}

				void Rescan() {
					entries.Clear();
					jengaCount = wsCount = 0;
					firstWsIdx = -1;
					selected = -1;
					NkVector<NkDirectoryEntry> es =
						NkDirectory::GetEntries(NkPath(curDir), "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
					int32 dirScanBudget = 150; // limite le sondage des sous-dossiers (perf)
					for (usize i = 0; i < es.Size(); ++i) {
						const NkDirectoryEntry &d = es[i];
						if (StrEq(d.Name.CStr(), ".") || StrEq(d.Name.CStr(), ".."))
							continue;
						if (!showHidden && (d.IsHidden || d.Name.CStr()[0] == '.'))
							continue; // caches/systeme OS masques par defaut
						NkOwEntry r;
						r.name = d.Name;
						r.isDir = d.IsDirectory;
						r.size = (int64)d.Size;
						int64 t = (int64)d.ModificationTime;
						if (t > 100000000000000LL)
							t = (t - 116444736000000000LL) / 10000000LL;
						r.mtime = t;
						// DOSSIER : sonder s'il contient directement un .jenga declarant un workspace.
						if (r.isDir && dirScanBudget > 0) {
							--dirScanBudget;
							NkVector<NkDirectoryEntry> sub =
								NkDirectory::GetEntries(d.FullPath, "*.jenga", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
							for (usize j = 0; j < sub.Size(); ++j) {
								if (!NkCodeState::EndsWithI(sub[j].Name.CStr(), ".jenga"))
									continue;
								const NkString stx = NkFile::ReadAllText(sub[j].FullPath);
								if (NkCodeState::Contains(stx.CStr(), "with workspace") ||
									NkCodeState::Contains(stx.CStr(), "workspace(")) {
									r.isWsDir = true;
									r.wsFile = sub[j].FullPath.ToString();
									r.workspace = NkCodeState::WorkspaceName(
										stx, NkPath(sub[j].Name.CStr()).GetFileNameWithoutExtension());
									break;
								}
							}
						}
						if (!r.isDir && NkCodeState::EndsWithI(d.Name.CStr(), ".jenga")) {
							r.isJenga = true;
							const NkString txt = NkFile::ReadAllText(d.FullPath);
							r.hasWorkspace = NkCodeState::Contains(txt.CStr(), "with workspace") ||
											 NkCodeState::Contains(txt.CStr(), "workspace(");
							r.workspace =
								NkCodeState::WorkspaceName(txt, NkPath(d.Name.CStr()).GetFileNameWithoutExtension());
							NkCodeState::CollectProjects(txt.CStr(), d.FullPath.GetParent(), &r.projCount);
							// Nature du fichier .jenga : workspace > projet > config > vide.
							const bool hasProj = r.projCount > 0 || NkCodeState::Contains(txt.CStr(), "project(");
							const bool hasCfg = NkCodeState::Contains(txt.CStr(), "config") ||
												NkCodeState::Contains(txt.CStr(), "configuration");
							if (r.size == 0 && txt.Empty())
								r.jengaKind = 0; // vide
							else if (r.hasWorkspace)
								r.jengaKind = 1; // workspace
							else if (hasProj)
								r.jengaKind = 2; // projet
							else if (hasCfg)
								r.jengaKind = 3; // config
							else
								r.jengaKind = 0; // .jenga sans contenu reconnu
						}
						// chaines figees au scan (Type / Taille)
						if (r.isDir)
							r.typeStr = r.isWsDir ? NkString(NkT("ow.type.wsjenga")) : NkString(NkT("ow.type.folder"));
						else if (r.isJenga) {
							switch (r.jengaKind) {
								case 1:
									r.typeStr = "Workspace Jenga";
									break;
								case 2:
									r.typeStr = "Projet Jenga";
									break;
								case 3:
									r.typeStr = "Config Jenga";
									break;
								default:
									r.typeStr = (r.size == 0) ? NkString(NkT("ow.type.jengaempty"))
															  : NkString(NkT("ow.type.jengafile"));
									break;
							}
						} else if (r.size == 0)
							r.typeStr = NkT("ow.type.emptyfile");
						else {
							const NkString e = ExtUpper(d.Name.CStr());
							r.typeStr = e.Empty() ? NkString(NkT("ow.type.file"))
												  : (NkString(NkT("ow.type.fileprefix")) + e.CStr());
						}
						if (!r.isDir)
							r.sizeStr = HumanSize(r.size);
						entries.PushBack(r);
					}
					const int64 now = NkCodeState::NowEpoch();
					for (usize i = 0; i < entries.Size(); ++i)
						entries[i].ageStr = NkCodeState::HumanAge(entries[i].mtime, now); // date FIGEE au scan
					Sort();
					for (usize i = 0; i < entries.Size(); ++i)
						if (entries[i].isJenga) {
							++jengaCount;
							if (entries[i].hasWorkspace) {
								++wsCount;
								if (firstWsIdx < 0)
									firstWsIdx = (int32)i;
							}
						}
					selected = firstWsIdx;
					scanned = true;
				}

				// Tri : dossiers TOUJOURS en premier ; au sein d'un groupe, par Nom (0) ou Modifie (1).
				void Sort() {
					auto rank = [](const NkOwEntry &e) { return e.isDir ? 0 : (e.isJenga ? 1 : 2); };
					for (usize a = 0; a < entries.Size(); ++a)
						for (usize b = a + 1; b < entries.Size(); ++b) {
							const int32 ra = rank(entries[a]), rb = rank(entries[b]);
							bool swap;
							if (rb != ra)
								swap = (rb < ra);
							else {
								int32 c = (sortBy == 1) ? (entries[a].mtime < entries[b].mtime
															   ? -1
															   : (entries[a].mtime > entries[b].mtime ? 1 : 0))
														: CmpI(entries[a].name.CStr(), entries[b].name.CStr());
								if (!sortAsc)
									c = -c;
								swap = (c > 0);
							}
							if (swap) {
								NkOwEntry t = entries[a];
								entries[a] = entries[b];
								entries[b] = t;
							}
						}
				}

				void SetSort(int32 by) {
					if (sortBy == by)
						sortAsc = !sortAsc;
					else {
						sortBy = by;
						sortAsc = true;
					}
					const int32 selIdx = selected;
					Sort();
					(void)selIdx; // (la selection suit l'index ; recalcul simple : on garde firstWsIdx)
				}
		};

		// Couleur d'un fichier .jenga selon sa nature (workspace/projet/config/vide).
		inline NkColor NkOwJengaColor(const NkOwEntry &e) {
			switch (e.jengaKind) {
				case 1:
					return NkCol::success; // workspace (vert)
				case 2:
					return NkCol::accent; // projet (orange)
				case 3:
					return NkColor{96, 165, 250, 255}; // config (bleu)
				default:
					return NkCol::mutedFg; // vide / non reconnu
			}
		}

		// Dessine une icone TEXTURE si disponible, sinon le trace vectoriel en repli.
		inline void NkOwIco(const NkUi &u, uint32 texId, const char *drawn, const NkRect &r, const NkColor &c) {
			if (texId)
				NkDrawIcon(u, texId, r, c);
			else
				u.Icon(drawn, r, c);
		}

		// ── Editeur de texte mono-ligne avec caret positionnable ──
		// Caret deplacable (fleches / Home / End), clignotant, insertion/suppression AU caret,
		// collage au caret, defilement horizontal pour garder le caret visible. Le panneau de
		// fond est dessine par l'appelant ; ici on gere saisie + texte + caret. Renvoie true si Entree.
		// ── Menu contextuel PARTAGE des champs texte (clic droit) : Couper/Copier/Coller/Tout.
		//    Ouvert par un champ (NkOwEdit / NkOverlayTextField), rendu EN DERNIER (au-dessus de tout),
		//    et il depose une `action` que le champ focus execute a la frame suivante.
		struct NkTxtMenuState {
				bool open = false;
				float32 x = 0.f, y = 0.f;
				int32 action = 0;
				bool hasSelection = false;
		}; // action : 0 rien,1 couper,2 copier,3 coller,4 tout

		inline NkTxtMenuState &NkTxtMenu() {
			static NkTxtMenuState m;
			return m;
		}

		// Champ de saisie mono-ligne : navigation flechee, SELECTION (Maj+fleches / Ctrl+A),
		// COPIER (Ctrl+C) / COUPER (Ctrl+X) / COLLER (Ctrl+V, remplace la selection), surbrillance,
		// cliquer-glisser, double-clic (tout), et CLIC DROIT -> menu contextuel.
		inline bool NkOwEdit(const NkUi &u, const NkRect &fr, char *buf, int32 cap, int32 &caret, float32 &blink,
							 float32 dt, float32 leftPad, bool mask = false) {
			static const void *s_owner = nullptr;
			static int32 s_anchor = -1; // -1 = pas de selection
			static bool s_drag = false; // selection souris en cours (cliquer-glisser)
			if (s_owner != buf) {
				s_owner = buf;
				s_anchor = -1;
				s_drag = false;
			}

			auto &in = u.ctx->input;
			blink += dt;
			int32 len = 0;
			while (buf[len])
				++len;
			if (caret > len)
				caret = len;
			if (caret < 0)
				caret = 0;
			if (s_anchor > len)
				s_anchor = len;
			const bool shift = in.shiftDown;
			// Indice du caractere sous la souris (tient compte du masque + defilement).
			auto caretAtMouse = [&]() -> int32 {
				const float32 availW0 = fr.w - leftPad - u.s(6);
				char tp[600];
				{
					int32 cc = caret < 599 ? caret : 599;
					for (int32 k = 0; k < cc; ++k)
						tp[k] = mask ? '*' : buf[k];
					tp[cc] = '\0';
				}
				const float32 pw = u.TextW(tp);
				const float32 offc = (pw > availW0) ? (pw - availW0) : 0.f;
				const float32 target = u.mp.x - (fr.x + leftPad) + offc;
				int32 best = 0;
				float32 bestd = 1e9f;
				char acc[600];
				int32 an = 0;
				for (int32 i = 0;; ++i) {
					acc[an] = '\0';
					const float32 wv = u.TextW(acc);
					const float32 d = wv > target ? wv - target : target - wv;
					if (d < bestd) {
						bestd = d;
						best = i;
					}
					if (!buf[i] || an >= 599)
						break;
					acc[an++] = mask ? '*' : buf[i];
				}
				return best;
			};
			const bool menuOpen = NkTxtMenu().open;
			bool menuPaste = false;
			// Clic DROIT -> ouvre le menu contextuel a la position souris (memorise l'etat de selection).
			if (!menuOpen && u.Hit(fr) && in.mouseClicked[1]) {
				auto &mn = NkTxtMenu();
				mn.open = true;
				mn.x = u.mp.x;
				mn.y = u.mp.y;
				mn.action = 0;
				mn.hasSelection = (s_anchor >= 0 && s_anchor != caret);
			}
			// Double-clic -> tout ; appui -> ancre ; glisser -> etend ; clic simple = curseur + deselection.
			// (souris gauche INACTIVE tant que le menu contextuel est ouvert)
			if (!menuOpen) {
				if (u.Hit(fr) && in.mouseDoubleClicked[0]) {
					s_anchor = 0;
					caret = len;
					s_drag = false;
					blink = 0.f;
				} else if (u.Hit(fr) && u.click) {
					const int32 c = caretAtMouse();
					caret = c;
					s_anchor = c;
					s_drag = true;
					blink = 0.f;
				} else if (s_drag && in.mouseDown[0]) {
					caret = caretAtMouse();
					blink = 0.f;
				} // glissement : etend
				if (!in.mouseDown[0])
					s_drag = false;
			}
			auto hasSel = [&]() -> bool { return s_anchor >= 0 && s_anchor != caret; };
			auto selLo = [&]() -> int32 { return s_anchor < caret ? s_anchor : caret; };
			auto selHi = [&]() -> int32 { return s_anchor < caret ? caret : s_anchor; };
			auto startSel = [&]() {
				if (s_anchor < 0)
					s_anchor = caret;
			};
			auto delSel = [&]() {
				const int32 lo = selLo(), n = selHi() - lo;
				for (int32 k = lo; k + n <= len; ++k)
					buf[k] = buf[k + n];
				len -= n;
				buf[len] = '\0';
				caret = lo;
				s_anchor = -1;
			};
			auto copySel = [&]() {
				char t2[600];
				int32 lo = selLo(), hi = selHi(), j = 0;
				for (int32 k = lo; k < hi && j < 599; ++k)
					t2[j++] = buf[k];
				t2[j] = '\0';
				u.ctx->SetClipboard(t2);
			};
			// Action deposee par le menu contextuel (choisie a la frame precedente).
			{
				auto &mn = NkTxtMenu();
				if (mn.action) {
					const int32 a = mn.action;
					mn.action = 0;
					blink = 0.f;
					if (a == 1) {
						if (hasSel()) {
							copySel();
							delSel();
						}
					} else if (a == 2) {
						if (hasSel())
							copySel();
					} else if (a == 4) {
						s_anchor = 0;
						caret = len;
					} else if (a == 3)
						menuPaste = true;
				}
			}

			// — navigation (Shift = etend la selection ; sans Shift = deplace et efface la selection) —
			if (in.KeyPressedRepeat(NkGuiKey::Left)) {
				if (shift) {
					startSel();
					if (caret > 0)
						--caret;
				} else {
					if (hasSel())
						caret = selLo();
					else if (caret > 0)
						--caret;
					s_anchor = -1;
				}
				blink = 0.f;
			}
			if (in.KeyPressedRepeat(NkGuiKey::Right)) {
				if (shift) {
					startSel();
					if (caret < len)
						++caret;
				} else {
					if (hasSel())
						caret = selHi();
					else if (caret < len)
						++caret;
					s_anchor = -1;
				}
				blink = 0.f;
			}
			if (in.KeyPressed(NkGuiKey::Home)) {
				if (shift)
					startSel();
				else
					s_anchor = -1;
				caret = 0;
				blink = 0.f;
			}
			if (in.KeyPressed(NkGuiKey::End)) {
				if (shift)
					startSel();
				else
					s_anchor = -1;
				caret = len;
				blink = 0.f;
			}
			// — tout selectionner / copier / couper —
			if (in.wantSelectAll) {
				s_anchor = 0;
				caret = len;
				in.wantSelectAll = false;
				blink = 0.f;
			}
			if (in.wantCopy) {
				if (hasSel())
					copySel();
				in.wantCopy = false;
			}
			if (in.wantCut) {
				if (hasSel()) {
					copySel();
					delSel();
				}
				in.wantCut = false;
				blink = 0.f;
			}
			// — suppression (efface la selection si presente) —
			if (in.KeyPressedRepeat(NkGuiKey::Backspace)) {
				if (hasSel())
					delSel();
				else if (caret > 0) {
					for (int32 k = caret - 1; k < len; ++k)
						buf[k] = buf[k + 1];
					--caret;
					--len;
				}
				blink = 0.f;
			}
			if (in.KeyPressedRepeat(NkGuiKey::Delete)) {
				if (hasSel())
					delSel();
				else if (caret < len) {
					for (int32 k = caret; k < len; ++k)
						buf[k] = buf[k + 1];
					--len;
				}
				blink = 0.f;
			}
			// — collage (remplace la selection) — Ctrl+V ou item « Coller » du menu contextuel —
			if (in.wantPaste || menuPaste) {
				if (hasSel())
					delSel();
				const NkString cb = u.ctx->GetClipboard();
				for (const char *s = cb.CStr(); *s; ++s) {
					if ((unsigned char)*s < 32 || len + 1 >= cap)
						continue;
					for (int32 k = len; k >= caret; --k)
						buf[k + 1] = buf[k];
					buf[caret] = *s;
					++caret;
					++len;
				}
				in.wantPaste = false;
				blink = 0.f;
			}
			// — insertion des caracteres tapes (remplace la selection) —
			for (int32 i = 0; i < in.charCount; ++i) {
				const uint32 cp = in.chars[i];
				if (cp < 32 || cp >= 127)
					continue;
				if (hasSel())
					delSel();
				if (len + 1 >= cap)
					break;
				for (int32 k = len; k >= caret; --k)
					buf[k + 1] = buf[k];
				buf[caret] = (char)cp;
				++caret;
				++len;
				blink = 0.f;
			}
			if (caret > len)
				caret = len;
			if (s_anchor > len)
				s_anchor = len;
			// — dessin (surbrillance selection + texte + caret), clip + defilement vers le caret —
			// En mode masque (mot de passe), on affiche/mesure sur une chaine de '*'.
			char disp[600];
			{
				int32 n = len < 599 ? len : 599;
				for (int32 k = 0; k < n; ++k)
					disp[k] = mask ? '*' : buf[k];
				disp[n] = '\0';
			}
			const float32 viewW = fr.w - leftPad - u.s(6);
			const float32 tx = fr.x + leftPad, ty = fr.y + (fr.h - u.Lh()) * 0.5f;
			char tmp[600];
			int32 c = caret < (int32)sizeof(tmp) - 1 ? caret : (int32)sizeof(tmp) - 1;
			for (int32 k = 0; k < c; ++k)
				tmp[k] = disp[k];
			tmp[c] = '\0';
			const float32 prefW = u.TextW(tmp);
			const float32 off = (prefW > viewW) ? (prefW - viewW) : 0.f; // garde le caret visible
			u.dl->PushClipRect({fr.x + leftPad, fr.y, viewW, fr.h}, true);
			if (hasSel()) {
				const int32 lo = selLo(), hi = selHi();
				char a[600];
				int32 na = lo < 599 ? lo : 599;
				for (int32 k = 0; k < na; ++k)
					a[k] = disp[k];
				a[na] = '\0';
				char b[600];
				int32 nb = hi < 599 ? hi : 599;
				for (int32 k = 0; k < nb; ++k)
					b[k] = disp[k];
				b[nb] = '\0';
				const float32 xa = u.TextW(a), xb = u.TextW(b);
				u.dl->AddRectFilled({tx - off + xa, fr.y + u.s(4), (xb - xa), fr.h - u.s(8)},
									NkColor{46, 110, 190, 140}, u.s(2));
			}
			u.Text(tx - off, ty, disp, NkCol::foreground);
			const float32 frac = blink - (float32)(int64)blink;
			if (frac < 0.5f)
				u.dl->AddRectFilled({tx - off + prefW, fr.y + u.s(5), u.s(1.5f), fr.h - u.s(10)}, NkCol::primary, 0.f);
			u.dl->PopClipRect();
			return in.KeyPressed(NkGuiKey::Enter);
		}

		// ── Variante caret AUTOMATIQUE : pour les champs simples SANS etat caret dedie.
		//    Le caret/blink sont conserves en statique, associes au buffer focus (un seul a la fois).
		//    Offre toutes les capacites de NkOwEdit (selection, copier/couper/coller, double-clic).
		inline bool NkOwEditA(const NkUi &u, const NkRect &fr, char *buf, int32 cap, float32 dt, float32 leftPad,
							  bool mask = false) {
			static const void *s_owner = nullptr;
			static int32 s_caret = 0;
			static float32 s_blink = 0.f;
			if (s_owner != buf) {
				s_owner = buf;
				int32 n = 0;
				while (buf[n])
					++n;
				s_caret = n;
				s_blink = 0.f;
			}
			return NkOwEdit(u, fr, buf, cap, s_caret, s_blink, dt, leftPad, mask);
		}

		// Rectangle du menu contextuel (clampe a la fenetre).
		inline NkRect NkTxtMenuBox(const NkUi &u) {
			auto &mn = NkTxtMenu();
			const float32 iw = u.s(172), ih = u.s(26), pad = u.s(4), mh = 4 * ih + 2 * pad;
			float32 mx = mn.x, my = mn.y;
			if (mx + iw > (float32)u.ctx->viewW)
				mx = (float32)u.ctx->viewW - iw - u.s(4);
			if (my + mh > (float32)u.ctx->viewH)
				my = (float32)u.ctx->viewH - mh - u.s(4);
			if (mx < u.s(2))
				mx = u.s(2);
			if (my < u.s(2))
				my = u.s(2);
			return {mx, my, iw, mh};
		}

		// Item actif ? 0 couper / 1 copier -> selection requise ; 2 coller -> presse-papier non vide ; 3 tout ->
		// toujours.
		inline bool NkTxtMenuEnabled(const NkUi &u, int32 i) {
			if (i == 0 || i == 1)
				return NkTxtMenu().hasSelection;
			if (i == 2)
				return !u.ctx->GetClipboard().Empty();
			return true;
		}

		// ── Input du menu : a appeler AVANT les panneaux. Consomme le clic (u.click = false) pour
		//    ne RIEN laisser passer vers les widgets derriere. Depose l'action choisie. ──
		inline void NkTextMenuInput(NkUi &u) {
			auto &mn = NkTxtMenu();
			if (!mn.open)
				return;
			if (u.ctx->input.KeyPressed(NkGuiKey::Escape)) {
				mn.open = false;
			}
			if (!u.click)
				return;
			const NkRect box = NkTxtMenuBox(u);
			const float32 ih = u.s(26), pad = u.s(4);
			for (int32 i = 0; i < 4; ++i) {
				const NkRect ir = {box.x + pad, box.y + pad + i * ih, box.w - 2 * pad, ih};
				if (u.Hit(ir) && NkTxtMenuEnabled(u, i)) {
					mn.action = i + 1;
					break;
				}
			}
			mn.open = false; // tout clic (item ou dehors) ferme le menu
			u.click = false;
			u.ctx->input.mouseClicked[0] = false; // CONSOMME : les panneaux ne verront pas ce clic
		}

		// ── Rendu du menu : a appeler EN DERNIER (au-dessus de tout). Grise les items inactifs. ──
		inline void NkDrawTextMenu(const NkUi &u) {
			auto &mn = NkTxtMenu();
			if (!mn.open)
				return;
			const char *items[] = {NkT("menu.cut"), NkT("menu.copy"), NkT("menu.paste"), NkT("menu.selectall")};
			const NkRect box = NkTxtMenuBox(u);
			const float32 ih = u.s(26), pad = u.s(4);
			u.dl->AddRectFilled({box.x + u.s(2), box.y + u.s(3), box.w, box.h}, NkColor{0, 0, 0, 90}, NkR::md * u.S);
			u.Panel(box, NkCol::surface, NkColor{48, 54, 61, 255}, NkR::md * u.S);
			for (int32 i = 0; i < 4; ++i) {
				const NkRect ir = {box.x + pad, box.y + pad + i * ih, box.w - 2 * pad, ih};
				const bool en = NkTxtMenuEnabled(u, i);
				if (en && u.Hit(ir))
					u.Rect(ir, NkCol::hover, NkR::sm * u.S);
				u.TextV(ir.x + u.s(10), ir.y, ih, items[i], en ? NkCol::foreground : NkCol::mutedFg);
				if (i == 3)
					u.Rect({box.x + pad, ir.y - 1.f, box.w - 2 * pad, 1.f}, NkCol::border); // separateur avant « Tout »
			}
		}

		// ── Champ de recherche local (icone loupe + saisie complete) — autonome ──
		// Saisie via NkOwEditA : selection, copier/couper/coller, double-clic tout selectionner.
		inline bool NkOwSearch(const NkUi &u, const NkRect &r, char *buf, int32 cap, bool focused,
							   const char *placeholder, uint32 searchTex, float32 dt) {
			u.Panel(r, NkCol::input, focused ? NkCol::primary : NkCol::border, NkR::md * u.S);
			NkOwIco(u, searchTex, "search", {r.x + u.s(9), r.y + (r.h - u.s(13)) * 0.5f, u.s(13), u.s(13)},
					NkCol::mutedFg);
			const float32 padL = u.s(28), ty = r.y + (r.h - u.Lh()) * 0.5f;
			if (focused)
				NkOwEditA(u, r, buf, cap, dt, padL);
			else if (buf[0])
				u.TextEllipsis(r.x + padL, ty, r.w - padL - u.s(10), buf, NkCol::foreground);
			else
				u.Text(r.x + padL, ty, placeholder, NkCol::mutedFg);
			return u.Hit(r) && u.click;
		}

		// ── Bouton icone carre (barre d'outils) ──
		inline bool NkOwIconBtn(const NkUi &u, const NkRect &r, uint32 texId, const char *drawn, bool enabled = true) {
			const bool hov = enabled && u.Hit(r);
			u.Rect(r, hov ? NkColor{48, 54, 61, 255} : NkCol::muted, NkR::sm * u.S);
			// desactive -> GRISE (visible), pas cache : un gris sombre distinct du fond.
			NkOwIco(u, texId, drawn, {r.x + (r.w - u.s(14)) * 0.5f, r.y + (r.h - u.s(14)) * 0.5f, u.s(14), u.s(14)},
					enabled ? NkCol::mutedFg : NkColor{74, 80, 88, 255});
			return hov && enabled && u.click;
		}

		// ── Panneau « Ouvrir un Workspace ». Renvoie 1 si « Annuler » (retour Home). ──
		// pickFolder = true -> mode « choisir un dossier » : le bouton bas devient « Choisir ce
		// dossier » (jamais grise), n'ouvre PAS de workspace, et renvoie 2 (dossier choisi = ow->curDir).
		inline int32 NkOpenWsPanel(const NkUi &u, const NkRect &r, NkOpenWsState *ow, NkCodeState *st,
								   NkCodeDialogs *dlg, float32 dt, const NkIcons &ic, bool pickFolder = false) {
			ow->EnsureInit(st);
			if (!ow->scanned)
				ow->Rescan();
			int32 result = 0;
			const float32 lh = u.Lh();
			// Un overlay (menu contextuel / modale de suppression) est-il ouvert ? Si oui,
			// le fond ne doit PLUS recevoir d'evenements (sinon le clic « traverse » le menu).
			// L'etat est lu en DEBUT de frame : le menu s'ouvre frame N, l'item est clique frame N+1.
			const bool blockBg =
				(ow->menuIdx != -1) || (ow->confirmDel >= 0) || (ow->favMenuIdx != -1) || NkTxtMenu().open;
			NkRect pathFieldR = {0, 0, 0, 0};
			bool pathFieldShown = false, pathFieldUp = false; // dropdown autocompletion
			u.Rect(r, NkCol::background);

			// ============ BARRE D'OUTILS ============
			const float32 tbH = u.s(48);
			u.Rect({r.x, r.y, r.w, tbH}, NkCol::surface);
			u.Rect({r.x, r.y + tbH - 1.f, r.w, 1.f}, NkCol::border);
			const float32 bw = u.s(26), by = r.y + (tbH - bw) * 0.5f;
			float32 bx = r.x + u.s(14);
			if (NkOwIconBtn(u, {bx, by, bw, bw}, ic.back, "arrow-left", ow->CanBack()) && !blockBg)
				ow->Back();
			bx += bw + u.s(6);
			if (NkOwIconBtn(u, {bx, by, bw, bw}, ic.forward, "arrow-right", ow->CanForward()) && !blockBg)
				ow->Forward();
			bx += bw + u.s(6);
			if (NkOwIconBtn(u, {bx, by, bw, bw}, ic.up, "arrow-up", true) && !blockBg)
				ow->Up();
			bx += bw + u.s(12);

			const float32 searchW = u.s(180), viewBoxW = u.s(60);
			const float32 crX = bx;
			const float32 crRight = r.x + r.w - u.s(14) - viewBoxW - u.s(8) - searchW - u.s(8);
			const NkRect crBar = {crX, by, (crRight > crX + u.s(80) ? crRight - crX : u.s(80)), bw};
			const bool crEditHere = ow->pathEdit && ow->pathEditTop;
			u.Panel(crBar, NkCol::input, crEditHere ? NkCol::primary : NkCol::border, NkR::md * u.S);
			if (crEditHere) {
				// Saisie libre du chemin dans le fil d'Ariane (taper / coller / fleches -> Entree).
				const bool ent = NkOwEdit(u, crBar, ow->pathBuf, (int32)sizeof(ow->pathBuf), ow->editCaret,
										  ow->editBlink, dt, u.s(10));
				ow->RefreshSugg();
				pathFieldR = crBar;
				pathFieldShown = true;
				pathFieldUp = false;
				if (u.ctx->input.KeyPressed(NkGuiKey::Tab))
					ow->CompleteSugg();
				if (!ow->suggIdx.Empty()) {
					if (u.ctx->input.KeyPressedRepeat(NkGuiKey::Down))
						ow->suggSel = (ow->suggSel + 1) % (int32)ow->suggIdx.Size();
					if (u.ctx->input.KeyPressedRepeat(NkGuiKey::Up))
						ow->suggSel = (ow->suggSel <= 0 ? (int32)ow->suggIdx.Size() - 1 : ow->suggSel - 1);
				}
				if (ent) {
					if (ow->suggSel >= 0 && ow->suggSel < (int32)ow->suggIdx.Size())
						ow->ApplySugg(ow->suggIdx[ow->suggSel]);
					else if (NkDirectory::Exists(ow->pathBuf)) {
						ow->SetDir(ow->pathBuf, true);
						ow->pathEdit = false;
					} else
						ow->status = NkT("ow.st.pathnotfound");
				}
				if (u.ctx->input.KeyPressed(NkGuiKey::Escape))
					ow->pathEdit = false;
			} else {
				// Fil d'Ariane : segments cliquables. On reconstruit le chemin segment par segment.
				bool segClicked = false;
				u.dl->PushClipRect({crBar.x + u.s(8), crBar.y, crBar.w - u.s(16), crBar.h}, true);
				float32 sx = crBar.x + u.s(10);
				const char *p = ow->curDir;
				NkString acc; // chemin accumule jusqu'au segment courant
				int32 seg = 0;
				char buf[256];
				int32 bn = 0;
				auto flush = [&](bool last) {
					if (bn == 0)
						return;
					buf[bn] = '\0';
					if (seg == 0)
						acc = buf;
					else {
						acc += "/";
						acc += buf;
					} // chemin cumule cliquable
					if (seg > 0) {
						u.Icon("chevron-right", {sx, crBar.y + (bw - u.s(10)) * 0.5f, u.s(10), u.s(10)},
							   NkCol::mutedFg);
						sx += u.s(14);
					}
					const float32 tw = u.TextW(buf);
					const NkRect segR = {sx, crBar.y, tw + u.s(4), bw};
					const bool hv = u.Hit(segR);
					u.TextV(sx, crBar.y, bw, buf, last ? NkCol::foreground : (hv ? NkCol::primary : NkCol::mutedFg));
					if (hv && u.click && !blockBg) {
						ow->SetDir(acc.CStr(), true);
						segClicked = true;
					}
					sx += tw + u.s(8);
					++seg;
					bn = 0;
				};
				for (; *p; ++p) {
					if (*p == '/' || *p == '\\') {
						flush(false);
					} else if (bn + 1 < (int32)sizeof(buf))
						buf[bn++] = *p;
				}
				flush(true);
				u.dl->PopClipRect();
				// Clic sur le fond du fil d'Ariane (hors segment) -> edition libre du chemin.
				if (u.Hit(crBar) && u.click && !segClicked && !blockBg)
					ow->BeginPathEdit(true);
			}
			// Recherche (vrai champ avec caret)
			const NkRect searchR = {crBar.x + crBar.w + u.s(8), by, searchW, bw};
			{
				const bool clk = NkOwSearch(u, searchR, ow->search, (int32)sizeof(ow->search), ow->focusSearch,
											NkT("ow.search"), ic.search, dt);
				// Tout clic (re)definit le focus : sur le champ -> actif ; ailleurs -> inactif.
				if (u.click && !blockBg)
					ow->focusSearch = clk;
			}
			// Bascule liste / grille
			const NkRect vbox = {searchR.x + searchR.w + u.s(8), by, viewBoxW, bw};
			u.Rect(vbox, NkCol::muted, NkR::md * u.S);
			const NkRect vL = {vbox.x + u.s(2), vbox.y + u.s(2), bw - u.s(2), bw - u.s(4)};
			const NkRect vG = {vbox.x + bw + u.s(2), vbox.y + u.s(2), bw - u.s(2), bw - u.s(4)};
			if (ow->viewMode == 0)
				u.Rect(vL, NkCol::surface, NkR::sm * u.S);
			if (ow->viewMode == 1)
				u.Rect(vG, NkCol::surface, NkR::sm * u.S);
			NkOwIco(u, ic.viewList, "list",
					{vL.x + (vL.w - u.s(14)) * 0.5f, vL.y + (vL.h - u.s(14)) * 0.5f, u.s(14), u.s(14)},
					ow->viewMode == 0 ? NkCol::foreground : NkCol::mutedFg);
			NkOwIco(u, ic.viewGrid, "grid",
					{vG.x + (vG.w - u.s(14)) * 0.5f, vG.y + (vG.h - u.s(14)) * 0.5f, u.s(14), u.s(14)},
					ow->viewMode == 1 ? NkCol::foreground : NkCol::mutedFg);
			if (u.Hit(vL) && u.click && !blockBg)
				ow->viewMode = 0;
			if (u.Hit(vG) && u.click && !blockBg)
				ow->viewMode = 1;

			// ============ CORPS : sous-sidebar (Emplacements/Recents) + liste ============
			const float32 botH = u.s(58);
			const float32 optH = u.s(32);	 // rangee d'options (filtres)
			const float32 bannerH = u.s(34); // bandeau d'etat (espace RESERVE, ne chevauche plus la liste)
			const float32 bodyTop = r.y + tbH, bodyH = r.h - tbH - botH;
			const float32 sbW = u.s(180);
			// — Panneau gauche : Emplacements · Favoris · Projets · Appareils —
			const NkRect sbR = {r.x, bodyTop, sbW, bodyH};
			u.Rect(sbR, NkCol::sidebar);
			u.Rect({r.x + sbW - 1.f, bodyTop, 1.f, bodyH}, NkCol::border);
			NkRect favZone = {0, 0, 0, 0}; // zone de depot des favoris (drag-to-add)
			if (u.Hit(sbR) && u.ctx->input.wheel != 0.f && !blockBg) {
				ow->sbScroll -= u.ctx->input.wheel * u.s(30);
				u.ctx->input.wheel = 0.f;
				if (ow->sbScroll < 0.f)
					ow->sbScroll = 0.f;
				if (ow->sbScroll > ow->sbScrollMax)
					ow->sbScroll = ow->sbScrollMax;
			}
			u.dl->PushClipRect(sbR, true);
			{
				float32 yy = bodyTop + u.s(12) - ow->sbScroll;
				const float32 ix0 = r.x + u.s(14), tx0 = r.x + u.s(34), rowW = sbW - u.s(12);
				// item generique cliquable ; renvoie l'action (0 rien, 1 clic gauche, 2 clic droit)
				auto navItem = [&](uint32 tex, const char *drawn, const char *label, const NkColor &lc,
								   bool sel) -> int32 {
					const NkRect ir = {r.x + u.s(6), yy, rowW, u.s(26)};
					const bool hv = u.Hit(ir);
					if (sel)
						u.Rect(ir, NkCol::primary, NkR::sm * u.S);
					else if (hv)
						u.Rect(ir, NkCol::hover, NkR::sm * u.S);
					NkOwIco(u, tex, drawn, {ix0, yy + (u.s(26) - u.s(15)) * 0.5f, u.s(15), u.s(15)},
							sel ? NkCol::primaryFg : lc);
					u.TextEllipsis(tx0, yy + (u.s(26) - lh) * 0.5f, rowW - u.s(34), label,
								   sel ? NkCol::primaryFg : NkCol::sidebarFg);
					yy += u.s(28);
					if (blockBg)
						return 0;
					if (hv && u.ctx->input.mouseClicked[1])
						return 2;
					if (hv && u.click)
						return 1;
					return 0;
				};
				auto header = [&](const char *t) {
					u.Text(r.x + u.s(14), yy, t, NkCol::mutedFg);
					yy += u.s(20);
				};

				// ── EMPLACEMENTS ──
				header(NkT("ow.places"));

				struct Place {
						uint32 tex;
						const char *drawn;
						const char *label;
						NkString path;
				};

				NkVector<Place> places;
				places.PushBack({ic.accueil, "home", NkT("nav.home"), NkOpenWsState::Home()});
				places.PushBack({ic.bureau, "monitor", NkT("ow.desktop"), NkOpenWsState::Desktop()});
				places.PushBack({ic.ouvrirDossier, "folder", NkT("ow.documents"), NkOpenWsState::Documents()});
				for (usize i = 0; i < places.Size(); ++i)
					if (navItem(places[i].tex, places[i].drawn, places[i].label, NkCol::mutedFg,
								StrEq(ow->curDir, places[i].path.CStr())) == 1)
						ow->SetDir(places[i].path.CStr(), true);

				// ── FAVORIS (persistes, glisser-deposer, clic-droit renommer/retirer) ──
				yy += u.s(4);
				u.Rect({r.x + u.s(14), yy, sbW - u.s(28), 1.f}, NkCol::border);
				yy += u.s(10);
				const float32 favTop = yy - u.s(4);
				header(NkT("ow.favorites"));
				if (ow->favs.Empty()) {
					u.TextV(r.x + u.s(14), yy, u.s(20), "(glissez un dossier ici)", NkCol::mutedFg);
					yy += u.s(22);
				}
				for (usize i = 0; i < ow->favs.Size(); ++i) {
					if (ow->favRename == (int32)i) {
						const NkRect fr = {r.x + u.s(8), yy, rowW - u.s(4), u.s(24)};
						u.Panel(fr, NkCol::input, NkCol::primary, NkR::sm * u.S);
						if (NkOwEdit(u, fr, ow->favBuf, (int32)sizeof(ow->favBuf), ow->editCaret, ow->editBlink, dt,
									 u.s(8)))
							ow->CommitFavRename();
						if (u.ctx->input.KeyPressed(NkGuiKey::Escape))
							ow->favRename = -1;
						yy += u.s(28);
						continue;
					}
					const int32 a = navItem(ic.ouvrirDossier, "folder", ow->favs[i].alias.CStr(), NkCol::accent,
											StrEq(ow->curDir, ow->favs[i].path.CStr()));
					if (a == 1)
						ow->SetDir(ow->favs[i].path.CStr(), true);
					else if (a == 2) {
						ow->favMenuIdx = (int32)i;
						ow->favMenuX = u.mp.x;
						ow->favMenuY = u.mp.y;
					}
				}
				favZone = {r.x, favTop, sbW, (yy - favTop)};

				// ── PROJETS (workspaces Jenga connus : epingles + recents) ──
				yy += u.s(4);
				u.Rect({r.x + u.s(14), yy, sbW - u.s(28), 1.f}, NkCol::border);
				yy += u.s(10);
				header(NkT("ow.projects"));
				NkVector<NkString> projPaths;
				NkVector<NkString> projNames;
				NkVector<NkString> seenDirs;
				// N'affiche QUE des workspaces reellement presents sur le disque, et SANS doublon
				// (un meme workspace epingle ET recent n'apparait qu'une fois).
				auto addProj = [&](const NkString &path, const NkString &name) {
					const NkString dir = NkPath(path.CStr()).GetParent().ToString();
					if (!NkFile::Exists(path.CStr()) && !NkDirectory::Exists(dir.CStr()))
						return; // entree perimee -> ignoree
					NkString key;
					for (const char *c = dir.CStr(); *c; ++c) {
						char ch = (*c == '\\') ? '/' : *c;
						if (ch >= 'A' && ch <= 'Z')
							ch = (char)(ch + 32);
						key += ch;
					}
					for (usize k = 0; k < seenDirs.Size(); ++k)
						if (seenDirs[k] == key)
							return; // deja liste -> pas de doublon
					seenDirs.PushBack(key);
					projPaths.PushBack(path);
					projNames.PushBack(name);
				};
				for (usize i = 0; st && i < st->pinned.Size(); ++i)
					addProj(st->pinned[i], i < st->pinnedNames.Size() ? st->pinnedNames[i] : NkString());
				for (usize i = 0; st && i < st->recents.Size(); ++i)
					addProj(st->recents[i], i < st->recentNames.Size() ? st->recentNames[i] : NkString());
				if (projPaths.Empty()) {
					u.TextV(r.x + u.s(14), yy, u.s(20), "(aucun)", NkCol::mutedFg);
					yy += u.s(22);
				}
				for (usize i = 0; i < projPaths.Size() && i < 8; ++i) {
					const NkString dir = NkPath(projPaths[i].CStr()).GetParent().ToString();
					NkString label = projNames[i];
					if (label.Empty())
						label = NkPath(dir.CStr()).GetFileName();
					if (navItem(ic.jenga, "book-open", label.CStr(), NkCol::accent, StrEq(ow->curDir, dir.CStr())) == 1)
						ow->SetDir(dir.CStr(), true);
				}

				// ── APPAREILS (disques montes) ── enumeration sans header lourd : lettres existantes.
				yy += u.s(4);
				u.Rect({r.x + u.s(14), yy, sbW - u.s(28), 1.f}, NkCol::border);
				yy += u.s(10);
				header(NkT("ow.devices"));
				int32 shown = 0;
				for (char d = 'C'; d <= 'Z'; ++d) {
					const char root[4] = {d, ':', '/', 0};
					if (!NkDirectory::Exists(root))
						continue;
					const char label[8] = {'D', 'i', 's', 'q', 'u', 'e', ' ', 0}; // base; on ajoute la lettre
					const NkString lab = NkPrintf("Disque %c:", d);				  // NkPrintf maison
					(void)label;
					if (navItem(ic.disque, "hard-drive", lab.CStr(), NkCol::mutedFg, StrEq(ow->curDir, root)) == 1)
						ow->SetDir(root, true);
					if (++shown >= 12)
						break;
				}
				if (shown == 0) { // Unix : racine
					if (navItem(ic.disque, "hard-drive", "/", NkCol::mutedFg, StrEq(ow->curDir, "/")) == 1)
						ow->SetDir("/", true);
				}
				const float32 contentBottom = yy + ow->sbScroll;
				ow->sbScrollMax = (contentBottom - bodyTop > bodyH) ? (contentBottom - bodyTop - bodyH + u.s(12)) : 0.f;
				if (ow->sbScroll > ow->sbScrollMax)
					ow->sbScroll = ow->sbScrollMax;
			}
			u.dl->PopClipRect();

			// — Liste des fichiers (hauteur reduite par le bandeau d'etat RESERVE + la rangee d'options) —
			const NkRect list = {r.x + sbW, bodyTop, r.w - sbW, bodyH - optH - bannerH};
			// en-tetes de colonnes : Nom (flex) | Type | Taille | Projets | Modifie
			const float32 colType = u.s(130), colSize = u.s(82), colProj = u.s(64), colMod = u.s(118);
			const float32 colsW = colType + colSize + colProj + colMod;
			const float32 hH = u.s(26);
			// Largeur de contenu (liste) pour le scroll horizontal : nom min + colonnes.
			const float32 minContentW = u.s(220) + colsW;
			const float32 lcw = (ow->viewMode == 0 && list.w < minContentW) ? minContentW : list.w;
			ow->hscrollMax = (lcw > list.w) ? (lcw - list.w) : 0.f;
			if (ow->hscroll > ow->hscrollMax)
				ow->hscroll = ow->hscrollMax;
			if (ow->hscroll < 0.f)
				ow->hscroll = 0.f;
			const float32 lox = list.x - ow->hscroll; // origine X du contenu (decalee par hscroll)
			// bornes droites des colonnes (ancrees sur la largeur de contenu)
			const float32 xMod = lox + lcw - colMod, xProj = xMod - colProj, xSize = xProj - colSize,
						  xType = xSize - colType;
			u.Rect({list.x, list.y, list.w, hH}, NkCol::surface);
			u.Rect({list.x, list.y + hH - 1.f, list.w, 1.f}, NkCol::border);
			u.dl->PushClipRect({list.x, list.y, list.w, hH}, true);
			// En-tete « Nom » : cliquable (tri) + icone de tri (↕)
			{
				const NkRect hh = {lox, list.y, xType - lox, hH};
				u.Text(lox + u.s(16), list.y + (hH - lh) * 0.5f, NkT("ow.col.name"),
					   ow->sortBy == 0 ? NkCol::foreground : NkCol::mutedFg);
				NkOwIco(u, ic.sort, "chevron-down",
						{lox + u.s(16) + u.TextW(NkT("ow.col.name")) + u.s(5), list.y + (hH - u.s(11)) * 0.5f, u.s(11),
						 u.s(11)},
						ow->sortBy == 0 ? NkCol::accent : NkCol::mutedFg);
				if (u.Hit(hh) && u.click && !blockBg)
					ow->SetSort(0);
			}
			u.Text(xType, list.y + (hH - lh) * 0.5f, NkT("ow.col.type"), NkCol::mutedFg);
			u.Text(xSize, list.y + (hH - lh) * 0.5f, NkT("ow.col.size"), NkCol::mutedFg);
			u.Text(xProj, list.y + (hH - lh) * 0.5f, NkT("ow.col.projects"), NkCol::mutedFg);
			// En-tete « Modifie » : cliquable (tri par date)
			{
				const NkRect hh = {xMod, list.y, colMod, hH};
				u.Text(hh.x, list.y + (hH - lh) * 0.5f, NkT("ow.col.modified"),
					   ow->sortBy == 1 ? NkCol::foreground : NkCol::mutedFg);
				if (ow->sortBy == 1)
					NkOwIco(u, ic.sort, "chevron-down",
							{hh.x + u.TextW(NkT("ow.col.modified")) + u.s(5), list.y + (hH - u.s(11)) * 0.5f, u.s(11),
							 u.s(11)},
							NkCol::accent);
				if (u.Hit(hh) && u.click && !blockBg)
					ow->SetSort(1);
			}
			u.dl->PopClipRect();

			// zone defilable des lignes
			const NkRect rowsArea = {list.x, list.y + hH, list.w, list.h - hH};
			if (u.Hit(rowsArea) && u.ctx->input.wheel != 0.f) {
				ow->scroll -= u.ctx->input.wheel * u.s(34);
				u.ctx->input.wheel = 0.f;
				if (ow->scroll < 0.f)
					ow->scroll = 0.f;
				if (ow->scroll > ow->scrollMax)
					ow->scroll = ow->scrollMax;
			}
			int32 doNav = -1;
			bool doOpen = false;
			bool clickedInRename = false;
			bool rowHit = false; // un element a-t-il ete clique cette frame (sinon clic vide -> deselection)
			// Champ de renommage inline (reutilise en liste comme en grille).
			auto renameField = [&](const NkRect &fr) {
				if (u.Hit(fr) && (u.click || u.ctx->input.mouseClicked[1]))
					clickedInRename = true;
				u.Panel(fr, NkCol::input, NkCol::primary, NkR::sm * u.S);
				if (NkOwEdit(u, fr, ow->renameBuf, (int32)sizeof(ow->renameBuf), ow->editCaret, ow->editBlink, dt,
							 u.s(6)))
					ow->CommitRename();
				if (u.ctx->input.KeyPressed(NkGuiKey::Escape))
					ow->renameIdx = -1;
			};
			// Interaction commune (selection / double-clic ouvrir / clic-droit menu / double-clic lent renommer).
			auto interact = [&](int32 i, const NkOwEntry &e, const NkRect &cell, bool wasSel) {
				if (blockBg || ow->renameIdx == i)
					return; // overlay ouvert -> le fond ne reagit pas
				const bool hv = u.Hit(cell);
				if (hv && u.ctx->input.mouseClicked[1]) {
					ow->menuIdx = i;
					ow->menuX = u.mp.x;
					ow->menuY = u.mp.y;
					ow->selected = i;
					rowHit = true;
				}
				if (hv && u.click) {
					rowHit = true;
					if (wasSel && !u.ctx->input.mouseDoubleClicked[0]) {
						ow->slowIdx = i;
						ow->slowClk = 0.f;
					} else
						ow->slowIdx = -1;
					ow->selected = i;
					if (e.isDir) {
						ow->dragPath = (NkPath(ow->curDir) / e.name.CStr()).ToString();
						ow->dragX0 = u.mp.x;
						ow->dragY0 = u.mp.y;
						ow->dragging = false;
					}
				}
				if (hv && u.ctx->input.mouseDoubleClicked[0]) {
					ow->slowIdx = -1;
					if (e.isDir)
						doNav = i;
					else if (e.isJenga && e.hasWorkspace)
						doOpen = true;
				}
			};
			ow->hoverIdx = -1; // recalcule chaque frame par la boucle (pour le tooltip)
			u.dl->PushClipRect(rowsArea, true);
			if (ow->viewMode == 0) {
				// ─────────────── VUE LISTE ───────────────
				const float32 rowH = u.s(30);
				float32 ry = rowsArea.y - ow->scroll;
				for (usize i = 0; i < ow->entries.Size(); ++i) {
					const NkOwEntry &e = ow->entries[i];
					if (!ow->Pass(e))
						continue;
					if (ow->search[0] && !NkCodeState::ContainsI(e.name.CStr(), ow->search))
						continue;
					const NkRect row = {list.x, ry, list.w, rowH};
					if (ry > rowsArea.y + rowsArea.h || ry + rowH < rowsArea.y) {
						ry += rowH;
						continue;
					}
					const bool sel = ((int32)i == ow->selected);
					const NkColor jc =
						e.isJenga ? NkOwJengaColor(e) : NkCol::success; // couleur d'accent (jenga ou dossier-workspace)
					const bool wsLike = e.isJenga || e.isWsDir;			// mis en avant
					const bool hv = u.Hit(row);
					if (sel)
						u.Rect(row, NkCol::primary, 0.f);
					else if (hv)
						u.Rect(row, NkCol::hover, 0.f);
					else if (wsLike)
						u.Rect(row, NkColor{30, 26, 18, 90}, 0.f); // fond legerement teinte
					u.Rect({list.x, ry + rowH - 1.f, list.w, 1.f}, NkCol::border);
					// icone + nom
					const float32 icx = lox + u.s(16), icy = ry + (rowH - u.s(18)) * 0.5f;
					if (e.isJenga)
						NkOwIco(u, ic.jenga, "book-open", {icx, icy, u.s(18), u.s(18)}, jc);
					else if (e.isDir)
						NkOwIco(u, ic.ouvrirDossier, "folder", {icx, icy, u.s(18), u.s(18)},
								e.isWsDir ? NkCol::success : NkCol::mutedFg);
					else {
						const uint32 ft = ic.ForFile(e.name.CStr());
						NkOwIco(u, ft ? ft : ic.fichier, "file", {icx, icy, u.s(18), u.s(18)}, NkCol::mutedFg);
					}
					const float32 nameW = xType - (lox + u.s(40)) - u.s(8);
					if (ow->renameIdx == (int32)i) {
						renameField({lox + u.s(38), ry + (rowH - u.s(22)) * 0.5f, nameW + u.s(2), u.s(22)});
					} else {
						NkString nm = e.name;
						if (e.isDir)
							nm += "/";
						const NkColor nameC = sel ? NkCol::foreground : (wsLike ? jc : NkCol::foreground);
						u.TextEllipsis(lox + u.s(40), ry + (rowH - lh) * 0.5f, nameW, nm.CStr(), nameC);
					}
					const NkColor subC = sel ? NkColor{255, 255, 255, 200} : NkCol::mutedFg;
					const float32 cy = ry + (rowH - lh) * 0.5f;
					// Type (colore selon la nature)
					u.TextEllipsis(xType, cy, colType - u.s(8), e.typeStr.CStr(),
								   sel ? NkCol::foreground : (wsLike ? jc : subC));
					// Taille (fichiers)
					if (!e.isDir && e.sizeStr.CStr()[0])
						u.TextEllipsis(xSize, cy, colSize - u.s(8), e.sizeStr.CStr(), subC);
					// Projets (nombre, .jenga)
					if (e.isJenga) {
						const NkString pc = NkPrintf("%d", e.projCount); // NkPrintf maison
						u.Text(xProj, cy, pc.CStr(), sel ? NkCol::foreground : subC);
					}
					// Modifie
					if (e.ageStr.CStr()[0])
						u.TextEllipsis(xMod, cy, colMod - u.s(10), e.ageStr.CStr(), subC);
					interact((int32)i, e, row, sel);
					// tooltip d'apercu : survol prolonge d'un .jenga OU d'un dossier-workspace
					if (hv && wsLike && !blockBg) {
						ow->hoverIdx = (int32)i;
						ow->hoverX = u.mp.x;
						ow->hoverY = ry + rowH;
					}
					ry += rowH;
				}
				const float32 contentH = (ry + ow->scroll) - rowsArea.y;
				ow->scrollMax = contentH > rowsArea.h ? contentH - rowsArea.h : 0.f;
			} else {
				// ─────────────── VUE GRILLE ───────────────
				const float32 pad = u.s(12), tileW = u.s(118), tileH = u.s(98);
				int32 cols = (int32)((rowsArea.w - pad) / (tileW + pad));
				if (cols < 1)
					cols = 1;
				const float32 gx0 = rowsArea.x + pad;
				int32 vi = 0;
				float32 lastBottom = rowsArea.y;
				for (usize i = 0; i < ow->entries.Size(); ++i) {
					const NkOwEntry &e = ow->entries[i];
					if (!ow->Pass(e))
						continue;
					if (ow->search[0] && !NkCodeState::ContainsI(e.name.CStr(), ow->search))
						continue;
					const int32 cx = vi % cols, cy = vi / cols;
					++vi;
					const float32 tx = gx0 + cx * (tileW + pad);
					const float32 ty = rowsArea.y + pad + cy * (tileH + pad) - ow->scroll;
					lastBottom = ty + tileH;
					if (ty > rowsArea.y + rowsArea.h || ty + tileH < rowsArea.y)
						continue;
					const NkRect tile = {tx, ty, tileW, tileH};
					const bool sel = ((int32)i == ow->selected);
					const NkColor jc = e.isJenga ? NkOwJengaColor(e) : NkCol::success;
					const bool wsLike = e.isJenga || e.isWsDir;
					const bool hv = u.Hit(tile);
					if (sel)
						u.Rect(tile, NkCol::primary, NkR::md * u.S);
					else if (hv)
						u.Rect(tile, NkCol::hover, NkR::md * u.S);
					else if (wsLike)
						u.Rect(tile, NkColor{30, 26, 18, 110}, NkR::md * u.S);
					const float32 isz = u.s(40), ix = tx + (tileW - isz) * 0.5f, iy = ty + u.s(14);
					if (e.isJenga)
						NkOwIco(u, ic.jenga, "book-open", {ix, iy, isz, isz}, jc);
					else if (e.isDir)
						NkOwIco(u, ic.ouvrirDossier, "folder", {ix, iy, isz, isz},
								e.isWsDir ? NkCol::success : NkCol::mutedFg);
					else {
						const uint32 ft = ic.ForFile(e.name.CStr());
						NkOwIco(u, ft ? ft : ic.fichier, "file", {ix, iy, isz, isz}, NkCol::mutedFg);
					}
					if (ow->renameIdx == (int32)i) {
						renameField({tx + u.s(6), ty + tileH - u.s(26), tileW - u.s(12), u.s(22)});
					} else {
						NkString nm = e.name;
						if (e.isDir)
							nm += "/";
						const NkColor nameC = sel ? NkCol::foreground : (e.isJenga ? jc : NkCol::foreground);
						// Nom CENTRE sous l'icone (sinon decale a gauche). Ellipsis si trop long.
						const float32 maxW = tileW - u.s(12);
						const float32 tw = u.TextW(nm.CStr());
						const float32 nx = (tw < maxW) ? tx + (tileW - tw) * 0.5f : tx + u.s(6);
						u.TextEllipsis(nx, ty + tileH - u.s(22), maxW, nm.CStr(), nameC);
					}
					interact((int32)i, e, tile, sel);
				}
				const float32 contentH = (lastBottom + ow->scroll) - rowsArea.y + pad;
				ow->scrollMax = contentH > rowsArea.h ? contentH - rowsArea.h : 0.f;
			}
			if (ow->scroll > ow->scrollMax)
				ow->scroll = ow->scrollMax;
			// Clic gauche / double-clic dans le VIDE (aucun element touche) -> deselection.
			if (!blockBg && u.Hit(rowsArea) && u.click && !rowHit) {
				ow->selected = -1;
				if (ow->renameIdx >= 0)
					ow->CommitRename();
			}
			// Clic-droit zone vide -> menu « Nouveau dossier / Actualiser ».
			if (!blockBg && u.Hit(rowsArea) && u.ctx->input.mouseClicked[1] && ow->menuIdx == -1) {
				ow->menuIdx = -2;
				ow->menuX = u.mp.x;
				ow->menuY = u.mp.y;
			}
			u.dl->PopClipRect();
			// Double-clic LENT (clic sur element deja selectionne, sans 2e clic rapide) -> renommer.
			if (ow->slowIdx >= 0) {
				ow->slowClk += dt;
				if (ow->slowClk > 0.45f)
					ow->BeginRename(ow->slowIdx);
			}
			// Scrollbar VERTICALE (draggable).
			if (ow->scrollMax > 0.5f) {
				const float32 sw = editorkit::NkScrollbarWidth();
				const NkRect track = {rowsArea.x + rowsArea.w - sw, rowsArea.y, sw, rowsArea.h};
				editorkit::NkVScrollbar(*u.ctx, *u.dl, track, ow->scroll, rowsArea.h + ow->scrollMax, rowsArea.h,
										0x0E157011u, u.s(28)); // scrollbar general (= editeur)
			}
			// Scrollbar HORIZONTALE (vue liste, quand les colonnes depassent la largeur).
			if (ow->hscrollMax > 0.5f) {
				const float32 sh = editorkit::NkScrollbarWidth();
				const float32 availW = rowsArea.w - (ow->scrollMax > 0.5f ? sh : 0.f); // place pour la barre V
				const NkRect track = {rowsArea.x, rowsArea.y + rowsArea.h - sh, availW, sh};
				editorkit::NkHScrollbar(*u.ctx, *u.dl, track, ow->hscroll, lcw, list.w, 0x0E157012u,
										u.s(40)); // scrollbar general (= editeur)
			}

			// ============ RANGEE D'OPTIONS (filtres) ============
			{
				const NkRect strip = {r.x + sbW, bodyTop + bodyH - optH, r.w - sbW, optH};
				u.Rect(strip, NkCol::surface);
				u.Rect({strip.x, strip.y, strip.w, 1.f}, NkCol::border);
				const float32 cyc = strip.y + optH * 0.5f;
				auto checkbox = [&](float32 cx, const char *label, bool &val, bool rescan) -> float32 {
					const float32 bs = u.s(15);
					const NkRect b = {cx, cyc - bs * 0.5f, bs, bs};
					const float32 lw = u.TextW(label);
					const NkRect hit = {cx - u.s(3), strip.y, bs + u.s(8) + lw + u.s(12), optH};
					const bool hv = u.Hit(hit);
					u.Panel(b, val ? NkCol::primary : NkCol::input, val ? NkCol::primary : NkCol::border,
							NkR::sm * u.S);
					if (val) {
						const float32 s = bs;
						u.dl->AddLine({b.x + s * 0.24f, b.y + s * 0.52f}, {b.x + s * 0.42f, b.y + s * 0.70f},
									  NkCol::primaryFg, u.s(1.7f));
						u.dl->AddLine({b.x + s * 0.42f, b.y + s * 0.70f}, {b.x + s * 0.78f, b.y + s * 0.30f},
									  NkCol::primaryFg, u.s(1.7f));
					}
					u.TextV(cx + bs + u.s(7), strip.y, optH, label, hv ? NkCol::foreground : NkCol::sidebarFg);
					if (hv && u.click && !blockBg) {
						val = !val;
						if (rescan)
							ow->scanned = false;
					}
					return hit.x + hit.w + u.s(6);
				};
				float32 cx = strip.x + u.s(14);
				cx = checkbox(cx, "Workspaces .jenga", ow->showJenga, false);
				cx = checkbox(cx, NkT("ow.dirs"), ow->showDirs, false);
				cx = checkbox(cx, NkT("ow.hidden"), ow->showHidden, true);
				cx = checkbox(cx, NkT("ow.allfiles"), ow->showAll, false);
			}

			// ============ BARRE DU BAS ============
			const NkRect bot = {r.x, r.y + r.h - botH, r.w, botH};
			u.Rect(bot, NkCol::surface);
			u.Rect({bot.x, bot.y, bot.w, 1.f}, NkCol::border);
			// bandeau d'etat (au-dessus des boutons, sur la barre du bas haute)
			// champ chemin (selection courante ou dossier)
			const float32 btnOpenW = u.s(150), btnCancelW = u.s(96);
			const NkRect pathBar = {bot.x + u.s(14), bot.y + (botH - u.s(30)) * 0.5f,
									bot.w - u.s(28) - btnOpenW - btnCancelW - u.s(16), u.s(30)};
			const bool pathEditHere = ow->pathEdit && !ow->pathEditTop;
			u.Panel(pathBar, NkCol::input, pathEditHere ? NkCol::primary : NkCol::border, NkR::md * u.S);
			NkOwIco(u, ic.ouvrirDossier, "folder-open",
					{pathBar.x + u.s(9), pathBar.y + (pathBar.h - u.s(14)) * 0.5f, u.s(14), u.s(14)}, NkCol::accent);
			if (pathEditHere) {
				const bool ent = NkOwEdit(u, pathBar, ow->pathBuf, (int32)sizeof(ow->pathBuf), ow->editCaret,
										  ow->editBlink, dt, u.s(28));
				ow->RefreshSugg();
				pathFieldR = pathBar;
				pathFieldShown = true;
				pathFieldUp = true;
				if (u.ctx->input.KeyPressed(NkGuiKey::Tab))
					ow->CompleteSugg();
				if (!ow->suggIdx.Empty()) {
					if (u.ctx->input.KeyPressedRepeat(NkGuiKey::Down))
						ow->suggSel = (ow->suggSel + 1) % (int32)ow->suggIdx.Size();
					if (u.ctx->input.KeyPressedRepeat(NkGuiKey::Up))
						ow->suggSel = (ow->suggSel <= 0 ? (int32)ow->suggIdx.Size() - 1 : ow->suggSel - 1);
				}
				if (ent) {
					if (ow->suggSel >= 0 && ow->suggSel < (int32)ow->suggIdx.Size())
						ow->ApplySugg(ow->suggIdx[ow->suggSel]);
					else if (NkDirectory::Exists(ow->pathBuf)) {
						ow->SetDir(ow->pathBuf, true);
						ow->pathEdit = false;
					} else
						ow->status = NkT("ow.st.pathnotfound");
				}
				if (u.ctx->input.KeyPressed(NkGuiKey::Escape))
					ow->pathEdit = false;
			} else {
				u.TextEllipsis(pathBar.x + u.s(28), pathBar.y + (pathBar.h - lh) * 0.5f, pathBar.w - u.s(36),
							   ow->curDir, NkCol::foreground);
				if (u.Hit(pathBar) && u.click && !blockBg)
					ow->BeginPathEdit(false);
			}
			// bouton Ouvrir / Choisir : plus jamais grise par l'absence de workspace
			// Jenga — un dossier SANS `.jenga` s'ouvre desormais en mode edition
			// simple (a la VSCode), cf. issues beta #3/#11. Le bandeau ci-dessous
			// reste informatif dans ce cas (pas bloquant).
			const bool err = false;
			const char *openLbl = pickFolder ? NkT("ow.choosefolder") : NkT("ow.openfolder");
			const NkRect openR = {bot.x + bot.w - u.s(14) - btnOpenW - btnCancelW - u.s(8),
								  bot.y + (botH - u.s(32)) * 0.5f, btnOpenW, u.s(32)};
			{
				const bool hv = !err && u.Hit(openR);
				u.Rect(openR, err ? NkCol::muted : (hv ? NkColHover(NkCol::primary) : NkCol::primary), NkR::md * u.S);
				const float32 tw = u.TextW(openLbl);
				NkOwIco(
					u, ic.ouvrirDossier, "folder-open",
					{openR.x + (openR.w - tw - u.s(20)) * 0.5f, openR.y + (openR.h - u.s(14)) * 0.5f, u.s(14), u.s(14)},
					err ? NkCol::mutedFg : NkCol::primaryFg);
				u.TextV(openR.x + (openR.w - tw - u.s(20)) * 0.5f + u.s(20), openR.y, openR.h, openLbl,
						err ? NkCol::mutedFg : NkCol::primaryFg);
				if (hv && u.click && !blockBg) {
					if (pickFolder)
						result = 2;
					else
						doOpen = true;
				}
			}
			// bouton Annuler
			const NkRect cancelR = {bot.x + bot.w - u.s(14) - btnCancelW, bot.y + (botH - u.s(32)) * 0.5f, btnCancelW,
									u.s(32)};
			if (u.Button(cancelR, NkT("btn.cancel"), NkCol::muted, NkCol::hover, NkCol::foreground, NkR::md * u.S) &&
				!blockBg)
				result = 1;

			// bandeau d'etat / erreur : dans son ESPACE RESERVE (au-dessus de la rangee d'options, sous la liste)
			{
				const float32 bannerY = bodyTop + bodyH - optH - bannerH + u.s(3);
				const NkRect banner = {r.x + sbW + u.s(14), bannerY, r.w - sbW - u.s(28), u.s(28)};
				if (ow->jengaCount == 0) {
					// INFORMATIF (pas bloquant) : le dossier s'ouvrira quand meme, en mode
					// edition simple (build/run/etc. indisponibles sans workspace Jenga).
					u.Panel(banner, NkCol::surface, NkCol::accent, NkR::md * u.S);
					u.Icon("alert-triangle",
						   {banner.x + u.s(10), banner.y + (banner.h - u.s(13)) * 0.5f, u.s(13), u.s(13)},
						   NkCol::accent);
					u.TextV(banner.x + u.s(30), banner.y, banner.h,
							"Aucun fichier .jenga — ouverture en mode edition simple (sans build/run).",
							NkCol::accent);
				} else if (ow->wsCount == 0) {
					u.Panel(banner, NkCol::surface, NkCol::accent, NkR::md * u.S);
					u.Icon("alert-triangle",
						   {banner.x + u.s(10), banner.y + (banner.h - u.s(13)) * 0.5f, u.s(13), u.s(13)},
						   NkCol::accent);
					u.TextV(banner.x + u.s(30), banner.y, banner.h,
							"Fichiers Jenga presents (projet / config) mais aucun ne declare de workspace.",
							NkCol::accent);
				} else {
					u.Panel(banner, NkCol::surface, NkCol::secondary, NkR::md * u.S);
					NkOwIco(u, ic.valide, "check-circle",
							{banner.x + u.s(10), banner.y + (banner.h - u.s(13)) * 0.5f, u.s(13), u.s(13)},
							NkCol::success);
					const char *wsName = (ow->firstWsIdx >= 0) ? ow->entries[ow->firstWsIdx].workspace.CStr() : "";
					const NkString msg = NkPrintf("%d fichier(s) .jenga detecte(s) — workspace : %s", ow->jengaCount,
												  wsName); // NkPrintf maison
					u.TextV(banner.x + u.s(30), banner.y, banner.h, msg.CStr(), NkCol::mutedFg);
				}
			}

			// message d'operation (erreur) -> remplace le bandeau le temps de l'afficher
			if (!ow->status.Empty()) {
				const NkRect sb = {r.x + sbW + u.s(14), bodyTop + bodyH - optH - bannerH + u.s(3), r.w - sbW - u.s(28),
								   u.s(28)};
				u.Panel(sb, NkCol::surface, NkCol::danger, NkR::md * u.S);
				u.Icon("alert-triangle", {sb.x + u.s(10), sb.y + (sb.h - u.s(13)) * 0.5f, u.s(13), u.s(13)},
					   NkCol::danger);
				u.TextV(sb.x + u.s(30), sb.y, sb.h, ow->status.CStr(), NkCol::danger);
			}

			// ── Commit du renommage si clic hors du champ ; idem favori ──
			// (la saisie de chemin est geree apres le dropdown d'autocompletion, plus bas.)
			if (!blockBg && ow->renameIdx >= 0 && u.click && !clickedInRename)
				ow->CommitRename();
			if (!blockBg && ow->favRename >= 0 && u.click && !u.Hit(sbR))
				ow->CommitFavRename();

			// ── Raccourcis clavier ──
			const bool editing = ow->renameIdx >= 0 || ow->pathEdit || ow->favRename >= 0;
			const bool ctrl = u.ctx->input.ctrlDown, alt = u.ctx->input.altDown, shift = u.ctx->input.shiftDown;
			// Copier le chemin (selection sinon dossier courant) — marche meme en saisie.
			if (ctrl && u.ctx->input.KeyPressed(NkGuiKey::C) && !blockBg) {
				const NkString p = (ow->selected >= 0 && ow->selected < (int32)ow->entries.Size())
									   ? (NkPath(ow->curDir) / ow->entries[ow->selected].name.CStr()).ToString()
									   : NkString(ow->curDir);
				u.ctx->SetClipboard(p.CStr());
			}
			if (!editing && !blockBg && !ow->focusSearch) {
				if (alt && u.ctx->input.KeyPressed(NkGuiKey::Left))
					ow->Back(); // Alt+Gauche : precedent
				if (alt && u.ctx->input.KeyPressed(NkGuiKey::Right))
					ow->Forward(); // Alt+Droite : suivant
				if (alt && u.ctx->input.KeyPressed(NkGuiKey::Up))
					ow->Up(); // Alt+Haut   : parent
				if (u.ctx->input.KeyPressed(NkGuiKey::F5))
					ow->scanned = false; // F5 : actualiser
				if (ctrl && u.ctx->input.KeyPressed(NkGuiKey::L))
					ow->BeginPathEdit(true); // Ctrl+L : saisie libre
				if (ctrl && shift && u.ctx->input.KeyPressed(NkGuiKey::N))
					ow->NewFolder(); // Ctrl+Shift+N : nouveau dossier
				if (u.ctx->input.KeyPressed(NkGuiKey::F2) && ow->selected >= 0)
					ow->BeginRename(ow->selected); // F2 : renommer
				if (ctrl && u.ctx->input.KeyPressed(NkGuiKey::H)) {
					ow->showHidden = !ow->showHidden;
					ow->scanned = false;
				}													// Ctrl+H : caches
				if (ctrl && u.ctx->input.KeyPressed(NkGuiKey::D)) { // Ctrl+D : ajouter aux favoris
					const bool selDir = ow->selected >= 0 && ow->selected < (int32)ow->entries.Size() &&
										ow->entries[ow->selected].isDir;
					ow->AddFav(selDir ? (NkPath(ow->curDir) / ow->entries[ow->selected].name.CStr()).ToString().CStr()
									  : ow->curDir);
				}
				if (ctrl && u.ctx->input.KeyPressed(NkGuiKey::Num1))
					ow->viewMode = 0; // Ctrl+1 : liste
				if (ctrl && u.ctx->input.KeyPressed(NkGuiKey::Num2))
					ow->viewMode = 1; // Ctrl+2 : grille
				// Suppr -> confirmation de suppression
				if (ow->confirmDel < 0 && ow->selected >= 0 && u.ctx->input.KeyPressed(NkGuiKey::Delete))
					ow->confirmDel = ow->selected;
			}
			if (ow->focusSearch && u.ctx->input.KeyPressed(NkGuiKey::Escape))
				ow->focusSearch = false;

			// ============ GLISSER-DEPOSER d'un dossier vers les FAVORIS ============
			if (!ow->dragPath.Empty()) {
				if (u.down) {
					const float32 dx = u.mp.x - ow->dragX0, dy = u.mp.y - ow->dragY0;
					if (!ow->dragging && (dx * dx + dy * dy) > u.s(7) * u.s(7))
						ow->dragging = true;
				} else {
					if (ow->dragging && u.Hit(favZone))
						ow->AddFav(ow->dragPath.CStr());
					ow->dragPath.Clear();
					ow->dragging = false;
				}
			}

			// ============ TOOLTIP D'APERCU .jenga (survol > 800 ms) ============
			if (ow->hoverIdx == ow->hoverPrev && ow->hoverIdx >= 0)
				ow->hoverTimer += dt;
			else
				ow->hoverTimer = 0.f;
			ow->hoverPrev = ow->hoverIdx;
			if (ow->hoverIdx >= 0 && ow->hoverIdx < (int32)ow->entries.Size() && ow->hoverTimer > 0.8f && st) {
				const NkOwEntry &e = ow->entries[ow->hoverIdx];
				const NkString full =
					(e.isWsDir && !e.wsFile.Empty()) ? e.wsFile : (NkPath(ow->curDir) / e.name.CStr()).ToString();
				const NkCodeState::WsMeta m = st->WorkspaceMeta(full.CStr());

				// construit les lignes (label, valeur)
				struct Ln {
						const char *k;
						NkString v;
				};

				NkVector<Ln> lines;
				if (!e.workspace.Empty())
					lines.PushBack({"Workspace", NkString("\"") + e.workspace.CStr() + "\""});
				if (!m.projects.Empty()) {
					lines.PushBack({"", NkPrintf("Projets (%d)", m.projCount)}); // NkPrintf maison
					lines.PushBack({"", m.projects});
				}
				if (!m.configs.Empty())
					lines.PushBack({"Configs", m.configs});
				if (!m.platforms.Empty())
					lines.PushBack({"Plateformes", m.platforms});
				if (!m.toolchains.Empty())
					lines.PushBack({"Toolchains", m.toolchains});
				if (!m.langVer.Empty())
					lines.PushBack({"Langage", m.langVer});
				if (!m.jengaVer.Empty())
					lines.PushBack({"Jenga", NkString("v") + m.jengaVer.CStr()});
				// dimensions
				const float32 padX = u.s(12), rowh = lh + u.s(5), titleH = u.s(28);
				float32 tw = u.TextW(e.name.CStr()) + u.s(40);
				for (usize k = 0; k < lines.Size(); ++k) {
					const float32 w =
						(lines[k].k[0] ? u.TextW(lines[k].k) + u.s(12) : 0.f) + u.TextW(lines[k].v.CStr()) + padX * 2;
					if (w > tw)
						tw = w;
				}
				if (tw > u.s(460))
					tw = u.s(460);
				const float32 th = titleH + lines.Size() * rowh + u.s(10);
				float32 tx = ow->hoverX + u.s(2), ty = ow->hoverY + u.s(2);
				if (tx + tw > r.x + r.w)
					tx = r.x + r.w - tw - u.s(6);
				if (ty + th > r.y + r.h)
					ty = ow->hoverY - th - u.s(20);
				const NkRect tip = {tx, ty, tw, th};
				u.dl->AddRectFilled({tip.x + u.s(3), tip.y + u.s(4), tip.w, tip.h}, NkColor{0, 0, 0, 90},
									NkR::md * u.S); // ombre
				u.Panel(tip, NkColor{26, 31, 40, 255}, NkCol::accent, NkR::md * u.S);
				NkOwIco(u, ic.jenga, "book-open", {tip.x + padX, tip.y + u.s(8), u.s(15), u.s(15)}, NkCol::accent);
				u.TextEllipsis(tip.x + padX + u.s(20), tip.y + u.s(7), tw - padX * 2 - u.s(20), e.name.CStr(),
							   NkCol::foreground);
				u.Rect({tip.x + u.s(6), tip.y + titleH - u.s(2), tw - u.s(12), 1.f}, NkCol::border);
				float32 yy = tip.y + titleH + u.s(2);
				u.dl->PushClipRect(tip, true);
				for (usize k = 0; k < lines.Size(); ++k) {
					if (lines[k].k[0]) {
						u.Text(tip.x + padX, yy, lines[k].k, NkCol::mutedFg);
						u.TextEllipsis(tip.x + padX + u.s(86), yy, tw - padX * 2 - u.s(86), lines[k].v.CStr(),
									   NkCol::foreground);
					} else {
						u.TextEllipsis(tip.x + padX, yy, tw - padX * 2, lines[k].v.CStr(), NkCol::accent);
					}
					yy += rowh;
				}
				u.dl->PopClipRect();
			}

			// ============ DROPDOWN D'AUTOCOMPLETION DU CHEMIN ============
			NkRect ddRect = {0, 0, 0, 0};
			bool suggClick = false;
			if (pathFieldShown && ow->pathEdit && !ow->suggIdx.Empty()) {
				const int32 total = (int32)ow->suggIdx.Size();
				const int32 cnt = total < 8 ? total : 8;
				const float32 ih = u.s(26), dw = pathFieldR.w;
				const float32 dh = cnt * ih + u.s(6);
				const float32 dy = pathFieldUp ? (pathFieldR.y - dh - u.s(2)) : (pathFieldR.y + pathFieldR.h + u.s(2));
				ddRect = {pathFieldR.x, dy, dw, dh};
				u.dl->AddRectFilled({ddRect.x + u.s(2), ddRect.y + u.s(3), ddRect.w, ddRect.h}, NkColor{0, 0, 0, 90},
									NkR::md * u.S);
				u.Panel(ddRect, NkCol::surface, NkCol::primary, NkR::md * u.S);
				for (int32 k = 0; k < cnt; ++k) {
					const int32 ci = ow->suggIdx[k];
					const NkRect ir = {ddRect.x + u.s(4), ddRect.y + u.s(3) + k * ih, dw - u.s(8), ih};
					const bool hv = u.Hit(ir);
					if (hv || k == ow->suggSel)
						u.Rect(ir, NkCol::hover, NkR::sm * u.S);
					NkOwIco(u, ic.ouvrirDossier, "folder",
							{ir.x + u.s(6), ir.y + (ih - u.s(14)) * 0.5f, u.s(14), u.s(14)}, NkCol::accent);
					u.TextEllipsis(ir.x + u.s(26), ir.y + (ih - lh) * 0.5f, dw - u.s(34), ow->dirCache[ci].CStr(),
								   NkCol::foreground);
					if (hv && u.click) {
						ow->ApplySugg(ci);
						suggClick = true;
					}
				}
				if (total > cnt)
					u.TextV(ddRect.x + u.s(8), ddRect.y + cnt * ih, ih, "...", NkCol::mutedFg);
			}
			// Clic hors du champ de chemin ET hors du dropdown -> fin de la saisie.
			// (pathFieldShown garantit que le champ a ete rendu CETTE frame : on n'annule donc
			//  pas sur le clic qui vient juste d'ACTIVER l'edition, ou pathFieldR n'est pas encore pose.)
			if (pathFieldShown && !blockBg && ow->pathEdit && u.click && !suggClick && !u.Hit(pathFieldR) &&
				!u.Hit(ddRect))
				ow->pathEdit = false;

			// ============ MENU CONTEXTUEL (clic droit) ============
			if (ow->menuIdx != -1) {
				ow->status.Clear();

				struct MItem {
						const char *label;
						int32 act;
				}; // act: 0 ouvrir/entrer,1 renommer,2 supprimer,3 nouveau dossier,4 actualiser

				NkVector<MItem> items;
				const bool onEntry = ow->menuIdx >= 0 && ow->menuIdx < (int32)ow->entries.Size();
				if (onEntry) {
					const NkOwEntry &e = ow->entries[ow->menuIdx];
					items.PushBack({e.isDir ? "Entrer" : "Ouvrir", 0});
					items.PushBack({"Renommer", 1});
					items.PushBack({"Supprimer", 2});
					if (e.isDir)
						items.PushBack({"Ajouter aux favoris", 5});
				}
				items.PushBack({"Nouveau dossier", 3});
				items.PushBack({"Actualiser", 4});
				const float32 mw = u.s(180), ih = u.s(28);
				float32 mx = ow->menuX, my = ow->menuY;
				if (mx + mw > r.x + r.w)
					mx = r.x + r.w - mw - u.s(4);
				if (my + ih * items.Size() > r.y + r.h)
					my = r.y + r.h - ih * items.Size() - u.s(4);
				const NkRect menu = {mx, my, mw, ih * items.Size() + u.s(8)};
				u.Panel(menu, NkCol::surface, NkCol::border, NkR::md * u.S);
				int32 chosen = -1;
				for (usize k = 0; k < items.Size(); ++k) {
					const NkRect ir = {menu.x + u.s(4), menu.y + u.s(4) + k * ih, mw - u.s(8), ih};
					const bool hv = u.Hit(ir);
					if (hv)
						u.Rect(ir, NkCol::hover, NkR::sm * u.S);
					u.TextV(ir.x + u.s(10), ir.y, ih, items[k].label,
							items[k].act == 2 ? NkCol::danger : NkCol::foreground);
					if (hv && u.click)
						chosen = items[k].act;
				}
				if (chosen >= 0) {
					const int32 idx = ow->menuIdx;
					ow->menuIdx = -1;
					if (chosen == 0 && onEntry) {
						const NkOwEntry &e = ow->entries[idx];
						if (e.isDir)
							doNav = idx;
						else if (e.isJenga && e.hasWorkspace)
							doOpen = true;
					} else if (chosen == 1)
						ow->BeginRename(idx);
					else if (chosen == 2)
						ow->confirmDel = idx;
					else if (chosen == 3)
						ow->NewFolder();
					else if (chosen == 4)
						ow->scanned = false;
					else if (chosen == 5 && onEntry)
						ow->AddFav((NkPath(ow->curDir) / ow->entries[idx].name.CStr()).ToString().CStr());
				} else if (u.click && !u.Hit(menu))
					ow->menuIdx = -1;
			}

			// ============ CONFIRMATION DE SUPPRESSION ============
			if (ow->confirmDel >= 0 && ow->confirmDel < (int32)ow->entries.Size()) {
				u.dl->AddRectFilled(r, NkColor{0, 0, 0, 140}, 0.f); // voile
				const float32 dw = u.s(380), dh = u.s(150);
				const NkRect box = {r.x + (r.w - dw) * 0.5f, r.y + (r.h - dh) * 0.5f, dw, dh};
				u.Panel(box, NkCol::surface, NkCol::border, NkR::lg * u.S);
				const NkOwEntry &e = ow->entries[ow->confirmDel];
				u.Icon("alert-triangle", {box.x + u.s(20), box.y + u.s(20), u.s(22), u.s(22)}, NkCol::accent);
				u.Text(box.x + u.s(52), box.y + u.s(22),
					   e.isDir ? "Mettre ce dossier a la corbeille ?" : "Mettre ce fichier a la corbeille ?",
					   NkCol::foreground);
				u.TextEllipsis(box.x + u.s(20), box.y + u.s(58), dw - u.s(40), e.name.CStr(), NkCol::mutedFg);
				u.Text(box.x + u.s(20), box.y + u.s(80), "Recuperable depuis la corbeille du systeme.", NkCol::mutedFg);
				const float32 bw2 = u.s(110), bh2 = u.s(32), byy = box.y + dh - bh2 - u.s(16);
				if (u.Button({box.x + dw - u.s(16) - bw2 * 2 - u.s(8), byy, bw2, bh2}, "Annuler", NkCol::muted,
							 NkCol::hover, NkCol::foreground, NkR::md * u.S))
					ow->confirmDel = -1;
				const NkRect delR = {box.x + dw - u.s(16) - bw2, byy, bw2, bh2};
				if (u.Button(delR, "Supprimer", NkCol::danger, NkColor{220, 70, 62, 255}, NkCol::primaryFg,
							 NkR::md * u.S))
					ow->DeleteEntry(ow->confirmDel);
				if (u.ctx->input.KeyPressed(NkGuiKey::Escape))
					ow->confirmDel = -1;
			}

			// ============ MENU CONTEXTUEL FAVORI (Renommer / Retirer) ============
			if (ow->favMenuIdx != -1) {
				struct FI {
						const char *label;
						int32 act;
				};

				const FI items[] = {{"Renommer", 0}, {"Retirer des favoris", 1}};
				const float32 mw = u.s(170), ih = u.s(28);
				float32 mx = ow->favMenuX, my = ow->favMenuY;
				if (mx + mw > r.x + r.w)
					mx = r.x + r.w - mw - u.s(4);
				const NkRect menu = {mx, my, mw, ih * 2 + u.s(8)};
				u.Panel(menu, NkCol::surface, NkCol::border, NkR::md * u.S);
				int32 chosen = -1;
				for (int32 k = 0; k < 2; ++k) {
					const NkRect ir = {menu.x + u.s(4), menu.y + u.s(4) + k * ih, mw - u.s(8), ih};
					const bool hv = u.Hit(ir);
					if (hv)
						u.Rect(ir, NkCol::hover, NkR::sm * u.S);
					u.TextV(ir.x + u.s(10), ir.y, ih, items[k].label,
							items[k].act == 1 ? NkCol::danger : NkCol::foreground);
					if (hv && u.click)
						chosen = items[k].act;
				}
				if (chosen >= 0) {
					const int32 idx = ow->favMenuIdx;
					ow->favMenuIdx = -1;
					if (chosen == 0)
						ow->BeginFavRename(idx);
					else
						ow->RemoveFav(idx);
				} else if (u.click && !u.Hit(menu))
					ow->favMenuIdx = -1;
			}

			// ============ GHOST de glisser-deposer (par-dessus tout) ============
			if (ow->dragging && !ow->dragPath.Empty()) {
				if (u.Hit(favZone))
					u.dl->AddRectFilled(favZone, NkColor{15, 115, 213, 46}, 0.f); // surbrillance cible
				const NkString nm = NkPath(ow->dragPath.CStr()).GetFileName();
				const float32 gw = u.TextW(nm.CStr()) + u.s(36);
				const NkRect g = {u.mp.x + u.s(12), u.mp.y + u.s(6), gw, u.s(26)};
				u.dl->AddRectFilled(g, NkColor{15, 115, 213, 230}, NkR::sm * u.S);
				NkOwIco(u, ic.ouvrirDossier, "folder", {g.x + u.s(7), g.y + (g.h - u.s(14)) * 0.5f, u.s(14), u.s(14)},
						NkCol::primaryFg);
				u.TextV(g.x + u.s(26), g.y, g.h, nm.CStr(), NkCol::primaryFg);
			}

			// ============ ACTIONS DIFFEREES ============
			if (doNav >= 0)
				ow->NavInto(ow->entries[doNav]);
			else if (doOpen && !err && !pickFolder) {
				dlg->DoLoad(NkPath(ow->curDir));
			} // charge le workspace du dossier

			return result;
		}

	} // namespace nkcode
} // namespace nkentseu
