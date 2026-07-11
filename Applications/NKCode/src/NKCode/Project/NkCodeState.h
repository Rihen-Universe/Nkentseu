#pragma once
// =============================================================================
// NkCodeState.h — Etat partage de l'IDE NKCode (projet, fichiers ouverts, sortie)
// + lancement de Jenga (build/run) avec capture de sortie.
// VSCode-like : explorateur -> ouvre des fichiers -> editeur multi-onglets ->
// commande Construire (jenga build) -> panneau Sortie.
// =============================================================================
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkPath.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKCode/Project/NkProcess.h"
#include "NKCode/Project/NkText.h"
#include "NKCode/Project/NkLogSink.h" // GlobalLogBuffer : traces [ac] de la completion (panneau OUTPUT)
#include "NKCode/Editor/NkCodeEditor.h"
#include "NKCode/Project/NkLsp.h"
#include "NKCode/Shell/NkI18n.h" // NkT() : ages relatifs traduits
#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace nkentseu {
	namespace nkcode {

		struct NkIcons; // forward (defini dans Shell/NkUi.h) — icones de la vue principale IDE

		using namespace nkentseu;

		inline bool StrEq(const char *a, const char *b) {
			if (!a || !b)
				return a == b;
			while (*a && *a == *b) {
				++a;
				++b;
			}
			return *a == *b;
		}

		// Un fichier ouvert dans l'editeur : chemin + document editable (modele par
		// lignes + curseur/selection/scroll). L'etat vit dans le doc -> jamais perdu
		// au changement d'onglet ni au re-dock.
		struct OpenFile {
				NkPath path;
				NkCodeDoc doc;				// modele par lignes + etat d'edition
				bool pinned = false;		// onglet epingle (non fermable accidentellement, affiche en tete)
				bool untitled = false;		// fichier « sans-titre » (pas encore sauvegarde -> Ctrl+S = dialogue)
				int64 diskMtime = 0;		// date de modif sur disque au dernier open/save (detection externe)
				bool deletedOnDisk = false; // le fichier a ete supprime en dehors de NKCode
				bool changedOnDisk = false; // le fichier a ete modifie en dehors de NKCode
				float32 codeZoom = 0.f;		// taille police PROPRE a cet onglet (0 = taille globale). Zoom par-fichier.

				NkString Name() const {
					return path.GetFileName();
				}
		};

		struct NkCodeState {
				NkIcons *icons = nullptr;  // icones de la vue principale IDE (pose par main.cpp)
				NkPath root;			   // dossier racine de l'explorateur (arbre)
				NkVector<OpenFile> files;  // onglets ouverts
				int32 active = -1;		   // onglet actif
				bool reqSaveAs = false;	   // demande d'ouvrir le dialogue « Enregistrer sous » (+ nouveau fichier /
										   // ré-enregistrer supprimé)
				NkVector<NkString> output; // sortie de la derniere commande jenga
				NkString status;		   // ligne d'etat (ex. "Build OK")
				int32 buildTotal = 0, buildDone = 0; // progression : projets total / faits

				bool IsBuilding() const {
					return mBuild.Running() || !mQueue.Empty();
				}

				float32 BuildProgress() const {
					return buildTotal > 0 ? (float32)buildDone / (float32)buildTotal : 0.f;
				}

				// Barre d'outils Visual Studio : config / plateforme / appareil cibles.
				int32 cfgIdx = 0;  // 0 Debug, 1 Release
				int32 platIdx = 0; // 0 Windows, 1 Linux, 2 Android, 3 Web
				int32 devIdx = 0;  // appareil/emulateur (mobile)

				NkCodeState() {
					// Racine = workspace Nkentseu (dossier courant au lancement) -> l'explorateur
					// montre tout le repo et la barre d'outils liste tous les projets du .jenga.
					root = NkPath::GetCurrentDirectory();
				}

				~NkCodeState() {
					if (navThread.Joinable())
						navThread.Join();
				} // go-to-def async : évite un thread orphelin à la fermeture

				// Ouvre `p` dans l'editeur (ou le re-selectionne si deja ouvert).
				void OpenPath(const NkPath &p) {
					const NkString ps = p.ToString();
					for (usize i = 0; i < files.Size(); ++i)
						if (StrEq(files[i].path.ToString().CStr(), ps.CStr())) {
							active = static_cast<int32>(i);
							return;
						}

					const NkString content = NkFile::ReadAllText(p);
					// GARDE-FOU anti perte de donnees : le fichier existe et est NON VIDE sur disque,
					// mais on lit un contenu VIDE (echec de lecture / verrou / chemin foireux) -> NE PAS
					// ouvrir un onglet vide (un Ctrl+S l'ecraserait). On abandonne l'ouverture.
					if (content.Empty() && NkFile::GetFileSize(p) > 0)
						return;
					OpenFile f;
					f.path = p;
					f.doc.SetText(content.CStr());
					f.doc.savedSig = f.doc.SymSig();			// etat propre de reference (undo -> point eteint)
					f.diskMtime = MTimeOf(p.ToString().CStr()); // référence pour la détection de changement externe
					files.PushBack(f);
					active = static_cast<int32>(files.Size()) - 1;
					RefreshGit(files[active]); // indicateurs Git de la gouttière
				}

				// ── Encodage (mojibake) : réparation par LOT de tous les fichiers ouverts affectés ──
				int32 CountMojibake() const {
					int32 c = 0;
					for (usize i = 0; i < files.Size(); ++i)
						if (files[i].doc.mojibake)
							++c;
					return c;
				}

				void RepairAllOpenEncodings() {
					for (usize i = 0; i < files.Size(); ++i)
						if (files[i].doc.mojibake)
							files[i].doc.RepairEncoding();
				}

				// Nouvel onglet vierge « sans-titre-N » (bouton + de la barre d'onglets).
				int32 untitledSeq = 0;

				void NewUntitled() {
					char nm[32];
					std::snprintf(nm, sizeof(nm), "sans-titre-%d.txt", ++untitledSeq);
					OpenFile f;
					f.path = NkPath(nm);
					f.doc.SetText("");
					f.untitled = true;
					files.PushBack(f);
					active = static_cast<int32>(files.Size()) - 1;
				}

				// ── Indicateurs Git de la gouttière : parse `git diff` vs HEAD ──
				static NkString RunCapture(const char *cmd) {
					NkString out;
#ifdef _WIN32
					FILE *p = _popen(cmd, "r");
#else
					FILE *p = popen(cmd, "r");
#endif
					if (!p)
						return out;
					char buf[1024];
					usize n;
					while ((n = std::fread(buf, 1, sizeof(buf) - 1, p)) > 0) {
						buf[n] = '\0';
						out += buf;
					}
#ifdef _WIN32
					_pclose(p);
#else
					pclose(p);
#endif
					return out;
				}

				static const char *SkipNum(const char *p) {
					while (*p >= '0' && *p <= '9')
						++p;
					return p;
				}

				static void ParseHunk(const char *s, int32 &oldS, int32 &oldC, int32 &newS, int32 &newC) {
					oldS = 0;
					oldC = 1;
					newS = 0;
					newC = 1; // b/d omis => 1
					const char *p = s + 2;
					while (*p == ' ')
						++p;
					if (*p == '-') {
						++p;
						oldS = NkAtoi(p);
						p = SkipNum(p);
						if (*p == ',') {
							++p;
							oldC = NkAtoi(p);
							p = SkipNum(p);
						}
					}
					while (*p == ' ')
						++p;
					if (*p == '+') {
						++p;
						newS = NkAtoi(p);
						p = SkipNum(p);
						if (*p == ',') {
							++p;
							newC = NkAtoi(p);
							p = SkipNum(p);
						}
					}
				}

				// Recalcule gitStatus/gitDeleted du fichier (vs HEAD). Appelé à l'ouverture + sauvegarde.
				void RefreshGit(OpenFile &f) {
					f.doc.gitStatus.Clear();
					f.doc.gitDeleted.Clear();
					const int32 lc = f.doc.LineCount();
					for (int32 i = 0; i < lc; ++i)
						f.doc.gitStatus.PushBack(0);
					if (f.path.ToString().Empty() || root.ToString().Empty())
						return;
					NkString cmd = NkString("git -C \"") + root.ToString().CStr() +
								   "\" --no-pager diff --no-color -U0 -- \"" + f.path.ToString().CStr() + "\"";
#ifdef _WIN32
					cmd += " 2>NUL";
#else
					cmd += " 2>/dev/null";
#endif
					const NkString out = RunCapture(cmd.CStr());
					const char *p = out.CStr();
					while (*p) {
						if (p[0] == '@' && p[1] == '@') {
							int32 oldS, oldC, newS, newC;
							ParseHunk(p, oldS, oldC, newS, newC);
							if (oldC > 0 && newC == 0) {
								int32 idx = newS - 1;
								if (idx < 0)
									idx = 0;
								f.doc.gitDeleted.PushBack(idx);
							} else {
								const uint8 kind = (oldC == 0) ? 1 : 2;
								for (int32 k = 0; k < newC; ++k) {
									const int32 idx = newS - 1 + k;
									if (idx >= 0 && idx < lc)
										f.doc.gitStatus[idx] = kind;
								}
							}
						}
						while (*p && *p != '\n')
							++p;
						if (*p == '\n')
							++p;
					}
				}

				// ── Index de symboles NIVEAU PROJET + BIBLIOTHÈQUE (types/fonctions de tout
				//    l'arbre du workspace, headers moteur inclus). Construit UNE fois en tâche
				//    de fond (ne gèle pas l'UI) ; lecture seule ensuite (gate `projReady`). ──
				NkVector<NkString> projTypes, projFuncs;
				// Tables de symboles du WORKSPACE (thread) : membres+types+SIGNATURES, heritage,
				// fonctions libres (retour+prototype), MACROS — voir NkSymTables (NkSyntax.h).
				NkSymTables wsTab;
				volatile bool projReady = false;
				bool projLoggedReady = false; // log one-shot quand l index est pret
				bool projStarted = false;
				threading::NkThread projThread;

				void StartProjectIndex() {
					if (projStarted || root.ToString().Empty())
						return;
					projStarted = true;
					projThread = threading::NkThread([this](void *) { BuildProjectIndex(); });
				}

				void BuildProjectIndex() {
					NkVector<NkString> t, f;
					NkSymTables tt;
					ScanDirSymbols(root, t, f, tt, 0);
					NkSymSortDedup(t);
					NkSymSortDedup(f);
					projTypes = t;
					projFuncs = f; // ecrit une fois puis lecture seule
					wsTab = tt;
					projReady = true;
				}

				void ScanDirSymbols(const NkPath &dir, NkVector<NkString> &t, NkVector<NkString> &f, NkSymTables &tt,
									int32 depth) {
					if (depth > 16)
						return;
					NkVector<NkDirectoryEntry> entries =
						NkDirectory::GetEntries(dir, "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
					for (usize i = 0; i < entries.Size(); ++i) {
						const NkDirectoryEntry &e = entries[i];
						const char *nm = e.Name.CStr();
						if (e.IsDirectory) {
							if (nm[0] == '.' || StrEq(nm, "Build") || StrEq(nm, "build") || StrEq(nm, "dist") ||
								StrEq(nm, "node_modules") || StrEq(nm, "Captures") || StrEq(nm, "bin") ||
								StrEq(nm, "obj") || StrEq(nm, "out"))
								continue; // dossiers lourds/inutiles
							ScanDirSymbols(e.FullPath, t, f, tt, depth + 1);
						} else {
							const NkLang lg = NkLangFromExt(e.FullPath.GetExtension().CStr());
							if (lg == NkLang::None || lg == NkLang::Markdown)
								continue;
							const NkString txt = NkFile::ReadAllText(e.FullPath);
							if (txt.Size() > 500000)
								continue; // ignore les très gros fichiers
							const bool isC = (lg == NkLang::C || lg == NkLang::NKSL);
							NkScanTextSymbols(txt.CStr(), isC, t, f);
							if (isC && tt.memOwner.Size() < 120000)
								NkScanTextMembers(txt.CStr(), tt); // membres/heritage/retours/macros/signatures
						}
					}
				}

				// ── DIAGNOSTICS (erreurs/avertissements du compilateur cible) ─────────────
				// Récupère les flags via `jenga compile-flags` (async), puis lance le compilo
				// en vérif syntaxe (`-fsyntax-only`//Zs) sur le fichier SAUVEGARDÉ et parse la
				// sortie -> squiggles. Fiable (vrai compilateur du toolchain), pas d'heuristique.
				// .jcdb = base de compilation Jenga : UNE entrée par projet (chacun ses includes/defines/std).
				struct ProjFlags {
						NkString name, dir, std;
						NkVector<NkString> includes, defines;
				};

				struct CompileDb {
						NkString compiler;
						bool msvc = false;
						NkVector<ProjFlags> projects;
						bool ready = false;
				};

				CompileDb cdb;
				NkProcess flagsProc;
				NkVector<NkString> flagsAcc;
				NkString flagsSig; // (plateforme|config) -> re-génère le .jcdb si change
				bool flagsBusy = false;
				bool flagsStale = false; // un .jenga a changé -> forcer la régénération du .jcdb
				NkProcess diagProc;
				NkVector<NkString> diagAcc;
				int32 diagTarget = -1; // index du fichier en cours de diag

				// ── Macros EFFECTIVES par projet (dump `<compilateur> -dM -E fichier`) : inclut les
				//    macros DÉRIVÉES des headers (ex _WIN32 -> NKENTSEU_PLATFORM_WINDOWS) que la simple
				//    liste de defines du .jcdb ignore. Sert au grisage préproc EXACT (comme clangd). ──
				struct MacroSet {
						NkString dir;
						NkVector<NkString> defs;
						bool ready = false;
				};

				NkVector<MacroSet> macroSets; // un par dossier de projet, résolu à la demande
				NkProcess macroProc;
				NkVector<NkString> macroAcc;
				NkString macroDir; // projet dont le dump est en cours
				bool macroBusy = false;

				static bool IsCppExt(const char *e) {
					return StrEqI(e, ".cpp") || StrEqI(e, ".cc") || StrEqI(e, ".cxx") || StrEqI(e, ".c") ||
						   StrEqI(e, ".h") || StrEqI(e, ".hpp") || StrEqI(e, ".hh") || StrEqI(e, ".hxx") ||
						   StrEqI(e, ".inl");
				}

				NkString ActiveFilePath() const {
					return (active >= 0 && active < static_cast<int32>(files.Size())) ? files[active].path.ToString()
																					  : NkString();
				}

				// Préfixe `set "PATH=<dossier du compilateur>;%PATH%" && ` : indispensable pour que
				// le compilateur lancé DIRECTEMENT (diag/dump macros) trouve ses DLL (ex msys/ucrt64
				// clang++ -> libc++). Sans ça il échoue en silence si son bin n'est pas dans le PATH.
				NkString CompilerPathPrefix() const {
					const char *c = cdb.compiler.CStr();
					int32 last = -1;
					for (int32 i = 0; c[i]; ++i)
						if (c[i] == '\\' || c[i] == '/')
							last = i;
					if (last <= 0)
						return NkString();
					NkString dir;
					for (int32 i = 0; i < last; ++i)
						dir += c[i];
					return NkString("set \"PATH=") + dir.CStr() + ";%PATH%\" && ";
				}

				NkString DiagSig() const { // le .jcdb dépend de (plateforme|config), pas du fichier
					int32 nSys = 0;
					const SysDef *sys = Systems(&nSys);
					const char *plat = sys[(sysIdx >= 0 && sysIdx < nSys) ? sysIdx : 0].name;
					return NkString(plat) + "|" + ConfigNameOf(cfgIdx >= 2 ? 0 : cfgIdx);
				}

				// Entrée du .jcdb dont le DOSSIER est l'ancêtre le plus proche du fichier.
				const ProjFlags *FlagsForFile(const NkString &path) const {
					const ProjFlags *best = nullptr;
					int32 bestLen = -1;
					for (usize i = 0; i < cdb.projects.Size(); ++i) {
						const ProjFlags &pf = cdb.projects[i];
						if (pf.dir.Empty())
							continue;
						if (NkPathIsAncestor(pf.dir.CStr(), path.CStr())) {
							const int32 n = static_cast<int32>(pf.dir.Size());
							if (n > bestLen) {
								bestLen = n;
								best = &pf;
							}
						}
					}
					return best;
				}

				// Parse le format PLAT .jcdb (lignes `clé\tvaleur`, section `project`).
				void ParseJcdb(const NkString &raw) {
					cdb = CompileDb();
					const char *p = raw.CStr();
					ProjFlags *cur = nullptr;
					char line[4096];
					while (*p) {
						int32 n = 0;
						while (*p && *p != '\n' && *p != '\r' && n < 4095)
							line[n++] = *p++;
						line[n] = 0;
						while (*p == '\n' || *p == '\r')
							++p;
						if (n == 0 || line[0] == '#')
							continue;
						int32 t = 0;
						while (t < n && line[t] != '\t')
							++t;
						if (t >= n)
							continue;
						line[t] = 0;
						const char *key = line;
						const char *val = line + t + 1;
						if (StrEq(key, "compiler"))
							cdb.compiler = val;
						else if (StrEq(key, "msvc"))
							cdb.msvc = (val[0] == '1');
						else if (StrEq(key, "project")) {
							cdb.projects.PushBack(ProjFlags{});
							cur = &cdb.projects[cdb.projects.Size() - 1];
							cur->name = val;
						} else if (cur) {
							if (StrEq(key, "dir"))
								cur->dir = val;
							else if (StrEq(key, "std"))
								cur->std = val;
							else if (StrEq(key, "inc"))
								cur->includes.PushBack(NkString(val));
							else if (StrEq(key, "def"))
								cur->defines.PushBack(NkString(val));
						}
					}
					cdb.ready = !cdb.compiler.Empty() && !cdb.projects.Empty();
				}

				// A appeler chaque frame : (re)génère le .jcdb (une fois par plateforme/config).
				void PollFlags() {
					if (!HasWorkspace())
						return;
					const NkString sig = DiagSig();
					if (!flagsBusy && (flagsStale || !StrEq(sig.CStr(), flagsSig.CStr()))) {
						int32 nSys = 0;
						const SysDef *sys = Systems(&nSys);
						const char *plat = sys[(sysIdx >= 0 && sysIdx < nSys) ? sysIdx : 0].name;
						NkString cmd = NkString("jenga compile-flags --platform ") + plat + " --config " +
									   ConfigNameOf(cfgIdx >= 2 ? 0 : cfgIdx) + JengaFileArg();
						flagsAcc.Clear();
						if (flagsProc.Start(cmd)) {
							flagsBusy = true;
							flagsSig = sig;
							flagsStale = false;
						}
						return;
					}
					if (flagsBusy) {
						flagsProc.Drain(flagsAcc);
						if (flagsProc.Done()) {
							flagsBusy = false;
							ParseJcdb(NkFile::ReadAllText(root / ".jenga" / "compileflags.jcdb"));
							macroSets.Clear(); // flags changés -> re-dumper les macros effectives par projet
						}
					}
				}

				NkString diagTempPath; // fichier temporaire compilé (BUFFER courant), supprimé après

				// Vérif syntaxe LIVE : écrit le BUFFER dans un fichier temp SIBLING (même dossier
				// -> `#include "x.h"` relatifs OK), sans exiger de sauvegarde. Le fichier temp a
				// l'extension `.nkcheck` (non compilée par les globs du build) + `-x` force le langage.
				NkString diagSent;				  // texte envoyé au compilateur (passes 1+ : ';' virtuels insérés)
				int32 diagPass = 0;				  // n° de passe (0 = buffer réel)
				int32 diagRawE = 0, diagRawW = 0; // compte BRUT cumulé (headers inclus)
				NkVector<NkString> diagShown;	  // lignes déjà tracées dans OUTPUT (dédup entre passes)

				// Point d'entrée : NOUVELLE analyse (frappe / save) -> passe 0 sur le buffer réel.
				void RunDiagnostics(int32 fileIdx) {
					diagPass = 0;
					diagRawE = 0;
					diagRawW = 0;
					diagShown.Clear();
					RunDiagPass(fileIdx);
				}

				// Une passe compilateur. Passe 0 = buffer réel ; passes 1+ = `diagSent` PATCHÉ (des ';'
				// virtuels insérés là où clang a dit « expected ';' ») -> révèle les erreurs que la
				// récupération du parseur MASQUAIT, sans jamais toucher le buffer de l'utilisateur.
				bool RunDiagPass(int32 fileIdx) {
					if (!cdb.ready || fileIdx < 0 || fileIdx >= static_cast<int32>(files.Size()))
						return false;
					if (diagProc.Running())
						return false;
					OpenFile &f = files[fileIdx];
					const NkString ext = f.path.GetExtension();
					if (!IsCppExt(ext.CStr()))
						return false;
					const ProjFlags *pf = FlagsForFile(f.path.ToString()); // flags du PROJET du fichier
					if (!pf)
						return false;
					if (diagPass == 0)
						diagSent = f.doc.GetText();
					const NkString tmp = (f.path.GetParent() / ".nkcode_diag.nkcheck").ToString();
					if (!NkFile::WriteAllText(NkPath(tmp), diagSent))
						return false;
					diagTempPath = tmp;
					const bool isC = StrEqI(ext.CStr(), ".c");
					NkString cmd = CompilerPathPrefix() + "\"" + cdb.compiler.CStr() + "\" ";
					if (cdb.msvc) {
						cmd += isC ? "/TC " : "/TP ";
						cmd += "/Zs /nologo ";
						if (!pf->std.Empty()) {
							cmd += "/std:";
							cmd += pf->std.CStr();
							cmd += " ";
						}
					} else {
						cmd += "-fsyntax-only ";
						cmd += isC ? "-x c " : "-x c++ ";
						if (!pf->std.Empty()) {
							cmd += "-std=";
							cmd += pf->std.CStr();
							cmd += " ";
						}
						// Rapporter TOUTES les erreurs en une passe (pas de plafond) -> on marque tout d'un coup.
						cmd +=
							NkFindSub(cdb.compiler.CStr(), "clang")
								? "-ferror-limit=0 -fno-caret-diagnostics "
								  "-Wno-pragma-once-outside-header " // le buffer est compile comme fichier PRINCIPAL :
																	 // un header ouvert declencherait ce faux warning
								: "-fmax-errors=0 -fno-diagnostics-show-caret ";
					}
					for (usize i = 0; i < pf->includes.Size(); ++i) {
						cmd += cdb.msvc ? "/I\"" : "-I\"";
						cmd += pf->includes[i].CStr();
						cmd += "\" ";
					}
					for (usize i = 0; i < pf->defines.Size(); ++i) {
						cmd += cdb.msvc ? "/D" : "-D";
						cmd += pf->defines[i].CStr();
						cmd += " ";
					}
					cmd += "\"";
					cmd += tmp.CStr();
					cmd += "\" 2>&1";
					diagAcc.Clear();
					diagTarget = fileIdx;
					diagProc.Start(cmd);
					return true;
				}

				// Insère ';' dans `diagSent` à (ligne, col) 0-based — passes de continuation UNIQUEMENT.
				void DiagInsertSemi(int32 line0, int32 col0) {
					const char *base = diagSent.CStr();
					usize off = 0;
					int32 l = 0;
					while (base[off] && l < line0) {
						if (base[off] == '\n')
							++l;
						++off;
					}
					usize len = 0;
					while (base[off + len] && base[off + len] != '\n' && base[off + len] != '\r')
						++len;
					const usize ins =
						off + (col0 < 0 ? 0 : (static_cast<usize>(col0) > len ? len : static_cast<usize>(col0)));
					NkString nu;
					for (usize k = 0; k < ins; ++k)
						nu += base[k];
					nu += ';';
					nu += (base + ins);
					diagSent = nu;
				}

				void PollDiagnostics() {
					if (diagTarget < 0)
						return;
					diagProc.Drain(diagAcc);
					if (!diagProc.Done())
						return;
					const int32 tgt = diagTarget;
					diagTarget = -1;
					bool chained = false;
					bool atLimit = false;
					if (tgt < static_cast<int32>(files.Size())) {
						OpenFile &f = files[tgt];
						if (diagPass == 0)
							f.doc.diags.Clear();
						const usize before = f.doc.diags.Size();
						for (usize li = 0; li < diagAcc.Size(); ++li)
							ParseDiagLine(diagAcc[li].CStr(), diagTempPath.CStr(), f.doc);
						// Dédup : une passe de continuation re-rapporte ce que la précédente montrait déjà.
						for (usize i = f.doc.diags.Size(); i > before;) {
							--i;
							bool dup = false;
							for (usize j = 0; j < before && !dup; ++j)
								dup = f.doc.diags[j].line == f.doc.diags[i].line &&
									  f.doc.diags[j].col == f.doc.diags[i].col &&
									  StrEq(f.doc.diags[j].msg.CStr(), f.doc.diags[i].msg.CStr());
							if (dup)
								f.doc.diags.Erase(f.doc.diags.Begin() + i);
						}
						// Trace OUTPUT (dédupliquée entre passes) : fichier:ligne:col + message, headers inclus.
						for (usize li = 0; li < diagAcc.Size(); ++li) {
							const char *ln = diagAcc[li].CStr();
							const char *pe = NkFindSub(ln, "error:");
							const char *pw = pe ? nullptr : NkFindSub(ln, "warning:");
							if (!pe && !pw)
								continue;
							NkString show;
							const char *tp = NkFindSub(ln, ".nkcode_diag.nkcheck");
							if (tp) { // remplace le fichier TEMP par le nom de l'onglet
								show = f.Name();
								show += (tp + 20); // strlen(".nkcode_diag.nkcheck")
							} else
								show = ln;
							bool seen = false;
							for (usize k = 0; k < diagShown.Size() && !seen; ++k)
								seen = StrEq(diagShown[k].CStr(), show.CStr());
							if (seen)
								continue;
							diagShown.PushBack(show);
							(pe ? diagRawE : diagRawW) += 1;
							GlobalLogBuffer().Push(NkString("[diag] ") + show.CStr());
						}
						// « expected ';' » dans les NOUVEAUX diags -> passe de CONTINUATION sur texte patché
						// (façon IDE : le vrai compilateur re-analyse, les erreurs masquées apparaissent).
						NkVector<int32> semiL, semiC;
						for (usize i = before; i < f.doc.diags.Size(); ++i)
							if (f.doc.diags[i].sev == 1 && NkFindSub(f.doc.diags[i].msg.CStr(), "expected ';'")) {
								semiL.PushBack(f.doc.diags[i].line);
								semiC.PushBack(f.doc.diags[i].col);
							}
						atLimit = !semiL.Empty() && diagPass >= 4;
						if (!semiL.Empty() && diagPass < 4) {
							for (usize a = 0; a < semiL.Size();
								 ++a) // tri (ligne, col) DÉCROISSANT : insertions bas -> haut
								for (usize b = a + 1; b < semiL.Size(); ++b)
									if (semiL[b] > semiL[a] || (semiL[b] == semiL[a] && semiC[b] > semiC[a])) {
										int32 t1 = semiL[a];
										semiL[a] = semiL[b];
										semiL[b] = t1;
										t1 = semiC[a];
										semiC[a] = semiC[b];
										semiC[b] = t1;
									}
							for (usize a = 0; a < semiL.Size(); ++a)
								DiagInsertSemi(semiL[a], semiC[a]);
							++diagPass;
							chained = RunDiagPass(tgt);
						}
						if (!chained) { // fin de chaîne -> résumé
							char lb[220];
							std::snprintf(lb, sizeof(lb),
										  "[diag] %s : %d marque(s) dans l'editeur ; compilateur : %d erreur(s), %d "
										  "avertissement(s) au total (headers inclus, %d passe%s)",
										  f.Name().CStr(), static_cast<int32>(f.doc.diags.Size()), diagRawE, diagRawW,
										  diagPass + 1, diagPass > 0 ? "s" : "");
							GlobalLogBuffer().Push(NkString(lb));
							if (atLimit)
								GlobalLogBuffer().Push(
									NkString("[diag] note : limite de passes atteinte - d'autres erreurs "
											 "peuvent rester masquees ; corrige celles-ci, la passe suivante "
											 "(~0,6 s) revelera la suite."));
						}
					}
					if (!chained && !diagTempPath.Empty()) {
						NkFile::Delete(NkPath(diagTempPath));
						diagTempPath = NkString();
					}
				}

				// ── Complétion CONTEXTUELLE (membres après '.', '->', '::'), en DEUX temps :
				//    1) INSTANTANÉ  : heuristique — type de l'objet inféré en remontant sa
				//       déclaration, membres lus dans la table workspace (memOwner/memName) et le
				//       buffer courant. Marche partout (MSVC/gcc compris), zéro latence.
				//    2) AFFINÉ      : le VRAI compilateur (famille clang, flags .jcdb) via
				//       `-Xclang -code-completion-at`, accéléré par un PCH DE PRÉAMBULE (le bloc
				//       d'#include compilé une fois en .pch, façon clangd) -> il remplace la liste
				//       heuristique QUAND il répond (jamais s'il échoue). ──
				NkProcess acProc;
				NkVector<NkString> acAcc;
				int32 acTarget = -1;
				NkString acTempPath;
				int32 acReqLine = -1, acReqCol = -1; // point demandé (0-based)
				bool acUsedPch = false;				 // la requête en vol utilisait le PCH
				NkString acFile;					 // fichier de la requête en vol

				// Cache PCH de préambule (un par fichier ; hash = préambule + flags).
				struct AcPch {
						NkString file; // fichier source
						NkString pch;  // chemin du .pch
						int64 hash = -1;
						bool ready = false;
				};

				NkVector<AcPch> acPchs;
				NkProcess pchProc;
				NkVector<NkString> pchAcc;
				int32 pchBuild = -1; // index acPchs en cours de build (-1 aucun)
				NkString pchHdrTemp; // header temporaire du préambule (supprimé après build)

				static int64 NkFnv(const char *s, int64 h = static_cast<int64>(1469598103934665603ULL)) {
					while (*s)
						h = (h ^ (unsigned char)*s++) * 1099511628211LL;
					return h;
				}

				// Nb de lignes de PRÉAMBULE (commentaires / vides / directives # en tête) + texte.
				// Renvoie 0 s'il n'y a aucun #include (un PCH n'apporterait rien).
				static int32 PreambleLines(const NkCodeDoc &doc, NkString &outText) {
					int32 l = 0, nInc = 0;
					bool blk = false, cont = false;
					for (; l < doc.LineCount(); ++l) {
						const NkCodeLine &L = doc.lines[l];
						const int32 n = static_cast<int32>(L.Size());
						bool pass = false;
						if (cont) {
							pass = true; // suite d'une directive terminée par '\'
							cont = n > 0 && L[n - 1] == '\\';
						} else if (blk) {
							pass = true;
							for (int32 i = 0; i + 1 < n; ++i)
								if (L[i] == '*' && L[i + 1] == '/') {
									blk = false;
									break;
								}
						} else {
							int32 s = 0;
							while (s < n && (L[s] == ' ' || L[s] == '\t'))
								++s;
							if (s >= n)
								pass = true; // ligne vide
							else if (L[s] == '/' && s + 1 < n && L[s + 1] == '/')
								pass = true;
							else if (L[s] == '/' && s + 1 < n && L[s + 1] == '*') {
								pass = true;
								blk = true;
								for (int32 i = s + 2; i + 1 < n; ++i)
									if (L[i] == '*' && L[i + 1] == '/') {
										blk = false;
										break;
									}
							} else if (L[s] == '#') {
								pass = true;
								cont = n > 0 && L[n - 1] == '\\';
								int32 k = s + 1;
								while (k < n && (L[k] == ' ' || L[k] == '\t'))
									++k;
								const char *w = "include";
								int32 t = 0;
								while (w[t] && k + t < n && L[k + t] == w[t])
									++t;
								if (!w[t])
									++nInc;
							}
						}
						if (!pass)
							break;
						for (int32 i = 0; i < n; ++i)
							outText += L[i];
						outText += '\n';
					}
					return nInc > 0 ? l : 0;
				}

				// Applique le préfixe tapé depuis (fromLine, fromCol) sur doc.acCtxAll -> popup.
				static void ApplyCtxFilter(NkCodeDoc &doc, int32 fromLine, int32 fromCol) {
					if (doc.curLine != fromLine || doc.curCol < fromCol)
						return;
					char pre[128];
					int32 pn = 0;
					if (doc.curLine < doc.LineCount()) {
						const NkCodeLine &L = doc.lines[doc.curLine];
						for (int32 k = fromCol; k < doc.curCol && pn < 127; ++k) {
							if (!NkCodeDoc::IsWChar(L[k])) {
								pn = -1;
								break;
							}
							pre[pn++] = L[k];
						}
					}
					if (pn < 0) {
						doc.acCtxAll.Clear();
						doc.acOpen = false;
						return; // le préfixe n'est plus un identifiant (ex. `)` tapé)
					}
					pre[pn] = 0;
					doc.acItems.Clear();
					for (usize ii = 0; ii < doc.acCtxAll.Size(); ++ii) {
						const char *nm = doc.acCtxAll[ii].CStr();
						int32 m2 = 0;
						while (pre[m2] && nm[m2] && (nm[m2] | 32) == (pre[m2] | 32))
							++m2;
						if (!pre[m2])
							doc.acItems.PushBack(doc.acCtxAll[ii]);
					}
					doc.acWordCol = fromCol;
					doc.acSel = 0;
					doc.acTop = 0;
					doc.acXOff = 0.f;
					doc.acOpen = !doc.acItems.Empty();
				}

				// ── Heuristique INSTANTANÉE (v2) : membres du type de l'expression avant '.', '->'
				//    ou '::'. Gère : variable déclarée (remontée), `this->` (classe englobante),
				//    `auto x = Type(...)`, chaînes `a.b().c.` (via le TYPE des membres/retours) et
				//    l'HÉRITAGE (membres des classes de base). Le compilateur AFFINE ensuite. ──

				// Classe dont le CORPS contient `line` : `class X { … }` englobant, ou corps d'une
				// méthode hors-classe `Ret X::Meth(...) { … }`. out="" si hors de tout type.
				void EnclosingTypeAt(const NkCodeDoc &doc, int32 line, char *out) {
					out[0] = 0;

					struct Sc {
							char nm[96];
							int32 depth;
					};

					Sc st[24];
					int32 sn = 0, depth = 0;
					bool blk = false;
					char pendT[96], pendO[96];
					pendT[0] = pendO[0] = 0;
					auto isW = [](char c) {
						return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
					};
					const int32 end = line < doc.LineCount() ? line : doc.LineCount() - 1;
					for (int32 l = 0; l <= end; ++l) {
						const NkCodeLine &L = doc.lines[l];
						const int32 n = static_cast<int32>(L.Size());
						char cl[1024];
						int32 cn = 0;
						for (int32 i = 0; i < n && cn < 1020;) { // nettoie (chaînes/commentaires)
							const char c = L[i];
							if (blk) {
								if (c == '*' && i + 1 < n && L[i + 1] == '/') {
									blk = false;
									i += 2;
									continue;
								}
								++i;
								continue;
							}
							if (c == '/' && i + 1 < n && L[i + 1] == '/')
								break;
							if (c == '/' && i + 1 < n && L[i + 1] == '*') {
								blk = true;
								i += 2;
								continue;
							}
							if (c == '"' || c == '\'') {
								const char q = c;
								++i;
								while (i < n) {
									if (L[i] == '\\') {
										i += 2;
										continue;
									}
									if (L[i] == q) {
										++i;
										break;
									}
									++i;
								}
								cl[cn++] = q;
								continue;
							}
							cl[cn++] = c;
							++i;
						}
						cl[cn] = 0;
						for (int32 i = 0; i < cn; ++i) {
							const char c = cl[i];
							if (c == '{') {
								++depth;
								if ((pendT[0] || pendO[0]) && sn < 24) {
									Sc &sc = st[sn++];
									const char *src = pendT[0] ? pendT : pendO;
									int32 k = 0;
									while (src[k] && k < 95) {
										sc.nm[k] = src[k];
										++k;
									}
									sc.nm[k] = 0;
									sc.depth = depth;
								}
								pendT[0] = pendO[0] = 0;
							} else if (c == '}') {
								if (sn > 0 && st[sn - 1].depth == depth)
									--sn;
								if (depth > 0)
									--depth;
							} else if (c == ';') {
								pendT[0] = pendO[0] = 0;
							} else if (c == '(') {
								// `X::meth(` hors type -> le corps qui suit appartient à X
								if (sn == 0 && !pendO[0]) {
									int32 e2 = i;
									while (e2 > 0 && (cl[e2 - 1] == ' ' || cl[e2 - 1] == '\t'))
										--e2;
									int32 s2 = e2;
									while (s2 > 0 && isW(cl[s2 - 1]))
										--s2;
									if (e2 > s2 && s2 >= 2 && cl[s2 - 1] == ':' && cl[s2 - 2] == ':') {
										int32 ce = s2 - 2, cs = ce;
										while (cs > 0 && isW(cl[cs - 1]))
											--cs;
										if (ce > cs) {
											int32 k = 0;
											for (int32 t2 = cs; t2 < ce && k < 95; ++t2)
												pendO[k++] = cl[t2];
											pendO[k] = 0;
										}
									}
								}
							} else if ((c == 's' || c == 'c' || c == 'u') && (i == 0 || !isW(cl[i - 1]))) {
								int32 adv = 0;
								if (i + 6 <= cn && c == 's' && cl[i + 1] == 't' && cl[i + 2] == 'r' &&
									cl[i + 3] == 'u' && cl[i + 4] == 'c' && cl[i + 5] == 't' && !isW(cl[i + 6]))
									adv = 6;
								else if (i + 5 <= cn && c == 'c' && cl[i + 1] == 'l' && cl[i + 2] == 'a' &&
										 cl[i + 3] == 's' && cl[i + 4] == 's' && !isW(cl[i + 5]))
									adv = 5;
								else if (i + 5 <= cn && c == 'u' && cl[i + 1] == 'n' && cl[i + 2] == 'i' &&
										 cl[i + 3] == 'o' && cl[i + 4] == 'n' && !isW(cl[i + 5]))
									adv = 5;
								if (!adv)
									continue;
								int32 s = i + adv;
								while (s < cn && (cl[s] == ' ' || cl[s] == '\t'))
									++s;
								int32 e = s;
								while (e < cn && isW(cl[e]))
									++e;
								if (e > s && !(cl[s] >= '0' && cl[s] <= '9')) {
									int32 k = 0;
									for (int32 t2 = s; t2 < e && k < 95; ++t2)
										pendT[k++] = cl[t2];
									pendT[k] = 0;
								}
								i = e - 1;
							}
						}
					}
					if (sn > 0) {
						int32 k = 0;
						while (st[sn - 1].nm[k]) {
							out[k] = st[sn - 1].nm[k];
							++k;
						}
						out[k] = 0;
					}
				}

				// Déclaration remontée de `var` -> type. Gère `auto x = Type(...)` / `= new T` /
				// init LAMBDA. Les COMMENTAIRES (`//`, `/*…*/` mono-ligne) et CHAÎNES sont ignorés
				// (une phrase de doc « via NkX » ne doit pas devenir un type). Init par APPEL ->
				// suffixe "()" : le résolveur force alors le type de RETOUR de la fonction.
				void VarDeclType(const NkCodeDoc &doc, const char *obj, char *out) {
					out[0] = 0;
					int32 on = 0;
					while (obj[on])
						++on;
					if (!on)
						return;
					const int32 from = doc.acCtxLine;
					char DL2[1024];
					for (int32 l = from; l >= 0 && l > from - 500 && !out[0]; --l) {
						const NkCodeLine &RAW = doc.lines[l];
						int32 dn = static_cast<int32>(RAW.Size());
						if (dn > 1020)
							dn = 1020;
						int32 cn2 = 0; // copie NETTOYÉE de la ligne
						for (int32 i = 0; i < dn;) {
							const char c = RAW[i];
							if (c == '/' && i + 1 < dn && RAW[i + 1] == '/')
								break;
							if (c == '/' && i + 1 < dn && RAW[i + 1] == '*') {
								i += 2;
								while (i + 1 < dn && !(RAW[i] == '*' && RAW[i + 1] == '/'))
									++i;
								i = (i + 1 < dn) ? i + 2 : dn;
								DL2[cn2++] = ' ';
								continue;
							}
							if (c == '"' || c == '\'') {
								const char q0 = c;
								++i;
								while (i < dn) {
									if (RAW[i] == '\\') {
										i += 2;
										continue;
									}
									if (RAW[i] == q0) {
										++i;
										break;
									}
									++i;
								}
								DL2[cn2++] = ' ';
								continue;
							}
							DL2[cn2++] = c;
							++i;
						}
						const int32 n2 = cn2;
						for (int32 i = 0; i + on <= n2; ++i) {
							if (i > 0 && NkCodeDoc::IsWChar(DL2[i - 1]))
								continue;
							int32 k = 0;
							while (k < on && DL2[i + k] == obj[k])
								++k;
							if (k != on || (i + on < n2 && NkCodeDoc::IsWChar(DL2[i + on])))
								continue;
							if (l == from && i > doc.acCtxCol)
								break; // la déclaration peut ÊTRE le mot survolé (i <= col)
							int32 q = i;
							while (q > 0 && (DL2[q - 1] == ' ' || DL2[q - 1] == '\t'))
								--q;
							while (q > 0 && (DL2[q - 1] == '*' || DL2[q - 1] == '&'))
								--q;
							while (q > 0 && (DL2[q - 1] == ' ' || DL2[q - 1] == '\t'))
								--q;
							if (q > 0 && DL2[q - 1] == '>') { // args template `NkVector<...>`
								int32 dep = 1;
								--q;
								while (q > 0 && dep > 0) {
									--q;
									if (DL2[q] == '>')
										++dep;
									else if (DL2[q] == '<')
										--dep;
								}
							}
							while (q > 0 && (DL2[q - 1] == ' ' || DL2[q - 1] == '\t'))
								--q;
							int32 e2 = q;
							while (q > 0 && NkCodeDoc::IsWChar(DL2[q - 1]))
								--q;
							if (e2 <= q || (DL2[q] >= '0' && DL2[q] <= '9'))
								continue;
							char cand[96];
							int32 c2 = 0;
							for (int32 t = q; t < e2 && c2 < 95; ++t)
								cand[c2++] = DL2[t];
							cand[c2] = 0;
							if (StrEq(cand, "return") || StrEq(cand, "else") || StrEq(cand, "new") ||
								StrEq(cand, "const") || StrEq(cand, "case") || StrEq(cand, "delete") ||
								StrEq(cand, "if") || StrEq(cand, "while"))
								continue;
							if (StrEq(cand, "auto")) { // `auto x = ...`
								int32 p2 = i + on;
								while (p2 < n2 && DL2[p2] == ' ')
									++p2;
								if (p2 < n2 && (DL2[p2] == '*' || DL2[p2] == '&'))
									++p2;
								while (p2 < n2 && DL2[p2] == ' ')
									++p2;
								if (p2 >= n2 || DL2[p2] != '=')
									continue;
								++p2;
								while (p2 < n2 && DL2[p2] == ' ')
									++p2;
								if (p2 + 3 <= n2 && DL2[p2] == 'n' && DL2[p2 + 1] == 'e' && DL2[p2 + 2] == 'w' &&
									(p2 + 3 >= n2 || !NkCodeDoc::IsWChar(DL2[p2 + 3]))) { // saute `new`
									p2 += 3;
									while (p2 < n2 && DL2[p2] == ' ')
										++p2;
								}
								if (p2 < n2 && DL2[p2] == '[') { // init LAMBDA `= [&](...)`
									CopyCap(out, "(lambda)", 95);
									continue;
								}
								int32 e3 = p2;
								while (e3 < n2 && NkCodeDoc::IsWChar(DL2[e3]))
									++e3;
								if (e3 > p2 && !(DL2[p2] >= '0' && DL2[p2] <= '9')) {
									int32 k2 = 0;
									for (int32 t = p2; t < e3 && k2 < 93; ++t)
										out[k2++] = DL2[t];
									if (e3 < n2 && DL2[e3] == '(') { // init par APPEL -> marqueur "()"
										out[k2++] = '(';
										out[k2++] = ')';
									}
									out[k2] = 0;
								}
								continue;
							}
							int32 k2 = 0;
							while (cand[k2] && k2 < 95) {
								out[k2] = cand[k2];
								++k2;
							}
							out[k2] = 0;
							break;
						}
					}
				}

				// Membres de `T` + de ses BASES (héritage), tables BUFFER + WORKSPACE, anti-cycles.
				void CollectMembersOf(const char *T, const NkSymTables &dt, NkVector<NkString> &out, char seen[8][96],
									  int32 &nSeen) {
					if (!T[0] || nSeen >= 8)
						return;
					for (int32 i = 0; i < nSeen; ++i)
						if (StrEq(seen[i], T))
							return; // déjà visité (anti-cycle)
					{
						int32 k = 0;
						while (T[k] && k < 95) {
							seen[nSeen][k] = T[k];
							++k;
						}
						seen[nSeen][k] = 0;
						++nSeen;
					}
					for (usize i = 0; i < dt.memOwner.Size(); ++i)
						if (StrEq(dt.memOwner[i].CStr(), T))
							out.PushBack(dt.memName[i]);
					if (projReady)
						for (usize i = 0; i < wsTab.memOwner.Size(); ++i)
							if (StrEq(wsTab.memOwner[i].CStr(), T))
								out.PushBack(wsTab.memName[i]);
					for (usize i = 0; i < dt.inhOwner.Size(); ++i)
						if (StrEq(dt.inhOwner[i].CStr(), T))
							CollectMembersOf(dt.inhBase[i].CStr(), dt, out, seen, nSeen);
					if (projReady)
						for (usize i = 0; i < wsTab.inhOwner.Size(); ++i)
							if (StrEq(wsTab.inhOwner[i].CStr(), T))
								CollectMembersOf(wsTab.inhBase[i].CStr(), dt, out, seen, nSeen);
				}

				// Type/retour d'un membre : tables du BUFFER d'abord, WORKSPACE ensuite.
				bool MemberTypeOf(const NkSymTables &dt, const char *T, const char *mem, char *o, char *sig = nullptr) {
					o[0] = 0;
					if (sig)
						sig[0] = 0;
					for (usize i = 0; i < dt.memOwner.Size(); ++i)
						if (StrEq(dt.memOwner[i].CStr(), T) && StrEq(dt.memName[i].CStr(), mem)) {
							CopyCap(o, dt.memType[i].CStr(), 95);
							if (sig)
								CopyCap(sig, dt.memSig[i].CStr(), 198);
							return true;
						}
					if (projReady)
						for (usize i = 0; i < wsTab.memOwner.Size(); ++i)
							if (StrEq(wsTab.memOwner[i].CStr(), T) && StrEq(wsTab.memName[i].CStr(), mem)) {
								CopyCap(o, wsTab.memType[i].CStr(), 95);
								if (sig)
									CopyCap(sig, wsTab.memSig[i].CStr(), 198);
								return true;
							}
					return false;
				}

				// Retour (+ prototype) d'une fonction LIBRE.
				bool GlobalFnOf(const NkSymTables &dt, const char *fn, char *rt, char *sig = nullptr) {
					rt[0] = 0;
					if (sig)
						sig[0] = 0;
					for (usize i = 0; i < dt.gfnName.Size(); ++i)
						if (StrEq(dt.gfnName[i].CStr(), fn)) {
							CopyCap(rt, dt.gfnRet[i].CStr(), 95);
							if (sig)
								CopyCap(sig, dt.gfnSig[i].CStr(), 198);
							return true;
						}
					if (projReady)
						for (usize i = 0; i < wsTab.gfnName.Size(); ++i)
							if (StrEq(wsTab.gfnName[i].CStr(), fn)) {
								CopyCap(rt, wsTab.gfnRet[i].CStr(), 95);
								if (sig)
									CopyCap(sig, wsTab.gfnSig[i].CStr(), 198);
								return true;
							}
					return false;
				}

				// MACRO `#define` : nom -> (args, corps). Buffer puis workspace.
				bool MacroOf(const NkSymTables &dt, const char *nm, NkString &args, NkString &body) {
					for (usize i = 0; i < dt.mcrName.Size(); ++i)
						if (StrEq(dt.mcrName[i].CStr(), nm)) {
							args = dt.mcrArgs[i];
							body = dt.mcrBody[i];
							return true;
						}
					if (projReady)
						for (usize i = 0; i < wsTab.mcrName.Size(); ++i)
							if (StrEq(wsTab.mcrName[i].CStr(), nm)) {
								args = wsTab.mcrArgs[i];
								body = wsTab.mcrBody[i];
								return true;
							}
					return false;
				}

				static void CopyCap(char *dst, const char *src, int32 cap) {
					int32 k = 0;
					while (src && src[k] && k < cap) {
						dst[k] = src[k];
						++k;
					}
					dst[k] = 0;
				}

				// Résout le PROPRIÉTAIRE de l'expression finissant juste avant `trigEnd` sur la
				// ligne `line` : chaîne `a.b().c` -> type final (déclaration remontée / this /
				// retour de fonction / membres). "" si non résolu. Partagé hover + complétion.
				void ResolveOwnerBefore(NkCodeDoc &doc, const NkSymTables &dt, int32 line, int32 trigEnd, bool scope2,
										char *owner) {
					owner[0] = 0;
					if (line < 0 || line >= doc.LineCount())
						return;
					const NkCodeLine &L = doc.lines[line];

					struct Seg {
							char nm[96];
							bool call;
					};

					Seg segs[8];
					int32 ns = 0;
					int32 q = trigEnd;
					while (ns < 8) {
						bool call = false;
						if (q > 0 && L[q - 1] == ')') {
							int32 dep = 1;
							--q;
							while (q > 0 && dep > 0) {
								--q;
								if (L[q] == ')')
									++dep;
								else if (L[q] == '(')
									--dep;
							}
							call = true;
						}
						int32 e = q;
						while (q > 0 && NkCodeDoc::IsWChar(L[q - 1]))
							--q;
						if (e <= q)
							return; // pas d'identifiant -> non résolu
						int32 k = 0;
						for (int32 t = q; t < e && k < 95; ++t)
							segs[ns].nm[k++] = L[t];
						segs[ns].nm[k] = 0;
						segs[ns].call = call;
						++ns;
						if (q >= 2 && L[q - 2] == '-' && L[q - 1] == '>') {
							q -= 2;
							continue;
						}
						if (q >= 1 && L[q - 1] == '.') {
							q -= 1;
							continue;
						}
						break;
					}
					if (ns == 0)
						return;
					char cur[96];
					cur[0] = 0;
					Seg &b = segs[ns - 1];
					if (scope2 && ns == 1) { // `Type::`
						CopyCap(cur, b.nm, 95);
					} else if (StrEq(b.nm, "this")) {
						EnclosingTypeAt(doc, line, cur);
					} else if (b.call) {
						char sig0[200];
						GlobalFnOf(dt, b.nm, cur, sig0);
						if (!cur[0]) {
							char encl[96];
							EnclosingTypeAt(doc, line, encl);
							if (encl[0])
								MemberTypeOf(dt, encl, b.nm, cur);
						}
					} else {
						const int32 sl = doc.acCtxLine, sc = doc.acCtxCol;
						doc.acCtxLine = line;
						doc.acCtxCol = trigEnd;
						VarDeclType(doc, b.nm, cur);
						doc.acCtxLine = sl;
						doc.acCtxCol = sc;
						{ // init par APPEL (suffixe "()") -> type de RETOUR force
							int32 L0 = 0;
							while (cur[L0])
								++L0;
							if (L0 > 2 && cur[L0 - 2] == 0x28 && cur[L0 - 1] == 0x29) {
								cur[L0 - 2] = 0;
								char alt0[96];
								if (GlobalFnOf(dt, cur, alt0) && alt0[0])
									CopyCap(cur, alt0, 95);
							}
						}
						if (!cur[0]) {
							char encl[96];
							EnclosingTypeAt(doc, line, encl);
							if (encl[0])
								MemberTypeOf(dt, encl, b.nm, cur);
						}
					}
					// base = NOM DE FONCTION (auto &e = NkEvents();) ? -> son type de RETOUR
					if (cur[0]) {
						bool isTy = false;
						for (usize i = 0; i < dt.memOwner.Size() && !isTy; ++i)
							if (StrEq(dt.memOwner[i].CStr(), cur))
								isTy = true;
						if (!isTy && projReady)
							for (usize i = 0; i < wsTab.memOwner.Size() && !isTy; ++i)
								if (StrEq(wsTab.memOwner[i].CStr(), cur))
									isTy = true;
						if (!isTy) {
							char alt0[96];
							if (GlobalFnOf(dt, cur, alt0) && alt0[0])
								CopyCap(cur, alt0, 95);
						}
					}
					for (int32 si = ns - 2; si >= 0 && cur[0]; --si) {
						char nx[96];
						if (!MemberTypeOf(dt, cur, segs[si].nm, nx))
							nx[0] = 0;
						CopyCap(cur, nx, 95);
					}
					CopyCap(owner, cur, 95);
				}

				void HeuristicMembers(NkCodeDoc &doc, NkVector<NkString> &out) {
					out.Clear();
					if (doc.acCtxLine < 0 || doc.acCtxLine >= doc.LineCount())
						return;
					const NkCodeLine &L = doc.lines[doc.acCtxLine];
					const int32 n = static_cast<int32>(L.Size());
					const int32 trigEnd = doc.acCtxCol > n ? n : doc.acCtxCol;
					bool scope = false;
					int32 pos = -1; // position juste AVANT le déclencheur
					if (trigEnd >= 1 && L[trigEnd - 1] == '.')
						pos = trigEnd - 1;
					else if (trigEnd >= 2 && L[trigEnd - 2] == '-' && L[trigEnd - 1] == '>')
						pos = trigEnd - 2;
					else if (trigEnd >= 2 && L[trigEnd - 2] == ':' && L[trigEnd - 1] == ':') {
						pos = trigEnd - 2;
						scope = true;
					}
					if (pos <= 0)
						return;
					// ── Tables LOCALES du buffer courant (types en cours d'édition, non sauvés). ──
					NkSymTables dt;
					NkScanTextMembers(doc.GetText().CStr(), dt);
					char cur[96];
					ResolveOwnerBefore(doc, dt, doc.acCtxLine, pos, scope, cur);
					if (!cur[0])
						return;
					// `cur` peut être un NOM DE FONCTION (ex. `auto &e = NkEvents();` -> l'init a
					// donné "NkEvents") : si ce n'est pas un type connu mais une fonction, son RETOUR.
					{
						bool isType = false;
						for (usize i = 0; i < dt.memOwner.Size() && !isType; ++i)
							if (StrEq(dt.memOwner[i].CStr(), cur))
								isType = true;
						if (!isType && projReady)
							for (usize i = 0; i < wsTab.memOwner.Size() && !isType; ++i)
								if (StrEq(wsTab.memOwner[i].CStr(), cur))
									isType = true;
						if (!isType) {
							char alt[96];
							if (GlobalFnOf(dt, cur, alt) && alt[0])
								CopyCap(cur, alt, 95);
						}
					}
					// ── Membres du type + de ses BASES (héritage, profondeur bornée). ──
					char seen[8][96];
					int32 nSeen = 0;
					CollectMembersOf(cur, dt, out, seen, nSeen);
					NkSymSortDedup(out);
					if (out.Size() > 400)
						out.Resize(400);
				}

				// ── Hover documentation (v3) : signature COMPLÈTE (prototype capturé au scan) et
				//    MACROS avec expansion des arguments du site d'appel. Ordre : macro > contexte
				//    membre (propriétaire par la chaîne) > variable locale > membre de la classe
				//    englobante > type > fonction libre > membre par nom (~ dernier recours). ──
				void ProcessHover() {
					if (active < 0 || active >= static_cast<int32>(files.Size()))
						return;
					NkCodeDoc &doc = files[active].doc;
					if (!doc.hovReq)
						return;
					doc.hovReq = false;
					doc.hovDone = true; // une seule resolution par mot survole
					const char *sym = doc.hovSym.CStr();
					if (!*sym || doc.hovLine < 0 || doc.hovLine >= doc.LineCount())
						return;
					// Mots-cles / primitifs / litteraux : pas de carte (bruit).
					{
						static const char *kws2[] = {
							"if",		"for",		 "while",	"switch",	"return",	"break",	"continue",
							"do",		"else",		 "case",	"goto",		"new",		"delete",	"sizeof",
							"true",		"false",	 "nullptr", "void",		"int",		"float",	"double",
							"char",		"bool",		 "auto",	"const",	"static",	"inline",	"constexpr",
							"unsigned", "signed",	 "long",	"short",	"class",	"struct",	"union",
							"enum",		"namespace", "using",	"typedef",	"template", "typename", "public",
							"private",	"protected", "virtual", "override", "final",	"operator", "this",
							nullptr};
						for (int32 k = 0; kws2[k]; ++k)
							if (StrEq(sym, kws2[k])) {
								char lb[96];
								std::snprintf(lb, sizeof(lb), "[hover] '%s' -> (mot-cle)", sym);
								GlobalLogBuffer().Push(NkString(lb));
								return;
							}
					}
					NkSymTables dt;
					NkScanTextMembers(doc.GetText().CStr(), dt);
					const NkCodeLine &HL = doc.lines[doc.hovLine];
					const int32 hn = static_cast<int32>(HL.Size());
					const int32 wordEnd = doc.hovCol + static_cast<int32>(doc.hovSym.Size());
					const bool callish = (wordEnd < hn && HL[wordEnd] == '(');
					// ── Pas de carte DANS un commentaire ou une chaîne (mots de prose -> bruit). ──
					{
						bool blk = false;
						for (int32 l = 0; l < doc.hovLine; ++l) { // état bloc /* */ au début de la ligne
							const NkCodeLine &L = doc.lines[l];
							const int32 n2 = static_cast<int32>(L.Size());
							for (int32 i = 0; i < n2;) {
								if (blk) {
									if (L[i] == 0x2A && i + 1 < n2 && L[i + 1] == 0x2F) {
										blk = false;
										i += 2;
										continue;
									}
									++i;
									continue;
								}
								if (L[i] == 0x2F && i + 1 < n2 && L[i + 1] == 0x2F)
									break;
								if (L[i] == 0x2F && i + 1 < n2 && L[i + 1] == 0x2A) {
									blk = true;
									i += 2;
									continue;
								}
								if (L[i] == 0x22 || L[i] == 0x27) {
									const char qq = L[i];
									++i;
									while (i < n2) {
										if (L[i] == 0x5C) {
											i += 2;
											continue;
										}
										if (L[i] == qq) {
											++i;
											break;
										}
										++i;
									}
									continue;
								}
								++i;
							}
						}
						bool inside = false, inStr = false;
						for (int32 i = 0; i < doc.hovCol && i < hn;) {
							if (blk) {
								if (HL[i] == 0x2A && i + 1 < hn && HL[i + 1] == 0x2F) {
									blk = false;
									i += 2;
									continue;
								}
								++i;
								continue;
							}
							if (HL[i] == 0x2F && i + 1 < hn && HL[i + 1] == 0x2F) {
								inside = true;
								break;
							}
							if (HL[i] == 0x2F && i + 1 < hn && HL[i + 1] == 0x2A) {
								blk = true;
								i += 2;
								continue;
							}
							if (HL[i] == 0x22 || HL[i] == 0x27) {
								const char qq = HL[i];
								inStr = true;
								++i;
								while (i < hn) {
									if (HL[i] == 0x5C) {
										i += 2;
										continue;
									}
									if (HL[i] == qq) {
										inStr = false;
										++i;
										break;
									}
									++i;
								}
								continue;
							}
							++i;
						}
						if (inside || blk || inStr) {
							char lb[128];
							std::snprintf(lb, sizeof(lb), "[hover] '%s' -> (commentaire/chaine)", sym);
							GlobalLogBuffer().Push(NkString(lb));
							return;
						}
					}
					char title[256];
					title[0] = 0;
					doc.hovBody.Clear();
					NkVector<NkString> docAnchors; // lignes-ancres pour retrouver la VRAIE definition (doc)
					doc.hovKind = 1;			   // defaut : fonction/membre (bleu)
					// ── (0) MACRO `#define` : corps + EXPANSION avec les arguments du site d'appel. ──
					{
						NkString margs, mbody;
						if (MacroOf(dt, sym, margs, mbody)) {
							std::snprintf(title, sizeof(title), "#define %s%s", sym, margs.CStr());
							doc.hovKind = 4;
							docAnchors.PushBack(NkString("#define ") + sym);
							if (!mbody.Empty())
								doc.hovBody.PushBack(mbody);
							if (callish && !margs.Empty() && !mbody.Empty()) {
								// paramètres de la macro
								NkVector<NkString> pn;
								{
									const char *a = margs.CStr();
									NkString curp;
									for (++a; *a; ++a) { // saute '('
										if (*a == ',' || *a == ')') {
											// rogne
											while (!curp.Empty() && (curp.CStr()[0] == ' '))
												curp = NkString(curp.CStr() + 1);
											if (!curp.Empty())
												pn.PushBack(curp);
											curp = NkString();
											if (*a == ')')
												break;
										} else
											curp += *a;
									}
								}
								// arguments réels du site d'appel (virgules de niveau 0)
								NkVector<NkString> av;
								{
									int32 i2 = wordEnd + 1, dep = 1;
									NkString curp;
									while (i2 < hn && dep > 0) {
										const char c2 = HL[i2];
										if (c2 == '(')
											++dep;
										else if (c2 == ')') {
											--dep;
											if (dep == 0)
												break;
										}
										if (c2 == ',' && dep == 1) {
											av.PushBack(curp);
											curp = NkString();
										} else
											curp += c2;
										++i2;
									}
									if (!curp.Empty() || !av.Empty())
										av.PushBack(curp);
								}
								// substitution NAÏVE (frontière de mot) param -> argument
								NkString exp = mbody;
								for (usize pi = 0; pi < pn.Size() && pi < av.Size(); ++pi) {
									const char *P = pn[pi].CStr();
									const int32 pl = static_cast<int32>(pn[pi].Size());
									NkString outS;
									const char *b2 = exp.CStr();
									const int32 bl = static_cast<int32>(exp.Size());
									for (int32 i3 = 0; i3 < bl;) {
										const bool lb = (i3 == 0) || !NkCodeDoc::IsWChar(b2[i3 - 1]);
										int32 k3 = 0;
										while (k3 < pl && i3 + k3 < bl && b2[i3 + k3] == P[k3])
											++k3;
										const bool rb = (i3 + pl >= bl) || !NkCodeDoc::IsWChar(b2[i3 + pl]);
										if (k3 == pl && lb && rb) {
											outS += av[pi];
											i3 += pl;
										} else {
											outS += b2[i3];
											++i3;
										}
									}
									exp = outS;
								}
								NkString ln2 = NkString("=> ") + exp;
								doc.hovBody.PushBack(ln2);
							}
						}
					}
					// ── (1) CONTEXTE MEMBRE : './->/::' avant le mot -> propriétaire par la chaîne. ──
					if (!title[0]) {
						int32 pos = -1;
						bool scope2 = false;
						const int32 q0 = doc.hovCol;
						if (q0 >= 1 && HL[q0 - 1] == '.')
							pos = q0 - 1;
						else if (q0 >= 2 && HL[q0 - 2] == '-' && HL[q0 - 1] == '>')
							pos = q0 - 2;
						else if (q0 >= 2 && HL[q0 - 2] == ':' && HL[q0 - 1] == ':') {
							pos = q0 - 2;
							scope2 = true;
						}
						if (pos > 0) {
							char owner[96];
							ResolveOwnerBefore(doc, dt, doc.hovLine, pos, scope2, owner);
							if (owner[0]) {
								char mty[96], sig[200];
								const bool known = MemberTypeOf(dt, owner, sym, mty, sig);
								if (known && sig[0]) {
									docAnchors.PushBack(NkString(sig));
									std::snprintf(title, sizeof(title), "%s", sig);
									NkString ctx2 = NkString("membre de ") + owner;
									doc.hovBody.PushBack(ctx2);
								} else if (known && mty[0])
									std::snprintf(title, sizeof(title), "%s %s::%s%s", mty, owner, sym,
												  callish ? "(...)" : "");
								else
									std::snprintf(title, sizeof(title), "%s::%s%s", owner, sym, callish ? "(...)" : "");
								if (known && !sig[0]) { // diagnostic : entrée appariée SANS prototype
									char lb2[128];
									std::snprintf(lb2, sizeof(lb2), "[hover] (sig vide) %s::%s", owner, sym);
									GlobalLogBuffer().Push(NkString(lb2));
								} else if (!known) {
									char lb2[128];
									std::snprintf(lb2, sizeof(lb2), "[hover] (membre inconnu) %s::%s", owner, sym);
									GlobalLogBuffer().Push(NkString(lb2));
								}
							}
						}
					}
					// ── (2) VARIABLE locale/paramètre : déclaration remontée depuis le survol. ──
					if (!title[0] && !callish) {
						const int32 sl = doc.acCtxLine, sc = doc.acCtxCol;
						doc.acCtxLine = doc.hovLine;
						doc.acCtxCol = doc.hovCol;
						char vt[96];
						VarDeclType(doc, sym, vt);
						doc.acCtxLine = sl;
						doc.acCtxCol = sc;
						{ // init par APPEL (suffixe "()") -> type de RETOUR force
							int32 L0 = 0;
							while (vt[L0])
								++L0;
							if (L0 > 2 && vt[L0 - 2] == 0x28 && vt[L0 - 1] == 0x29) {
								vt[L0 - 2] = 0;
								char alt0[96];
								if (GlobalFnOf(dt, vt, alt0) && alt0[0])
									CopyCap(vt, alt0, 95);
							}
						}
						if (vt[0]) { // init par une FONCTION (auto &e = NkEvents();) -> type de RETOUR
							bool isTy = false;
							for (usize i = 0; i < dt.memOwner.Size() && !isTy; ++i)
								if (StrEq(dt.memOwner[i].CStr(), vt))
									isTy = true;
							if (!isTy && projReady)
								for (usize i = 0; i < wsTab.memOwner.Size() && !isTy; ++i)
									if (StrEq(wsTab.memOwner[i].CStr(), vt))
										isTy = true;
							if (!isTy) {
								char alt[96];
								if (GlobalFnOf(dt, vt, alt) && alt[0])
									CopyCap(vt, alt, 95);
							}
							doc.hovKind = 3; // variable (jaune)
							std::snprintf(title, sizeof(title), "%s %s", vt, sym);
						}
					}
					// ── (3) Membre de la CLASSE ENGLOBANTE (accès implicite via this). ──
					if (!title[0]) {
						char encl[96];
						EnclosingTypeAt(doc, doc.hovLine, encl);
						if (encl[0]) {
							char mty[96], sig[200];
							if (MemberTypeOf(dt, encl, sym, mty, sig)) {
								if (sig[0]) {
									docAnchors.PushBack(NkString(sig));
									std::snprintf(title, sizeof(title), "%s", sig);
									NkString ctx2 = NkString("membre de ") + encl;
									doc.hovBody.PushBack(ctx2);
								} else if (mty[0])
									std::snprintf(title, sizeof(title), "%s %s::%s%s", mty, encl, sym,
												  callish ? "(...)" : "");
								else
									std::snprintf(title, sizeof(title), "%s::%s%s", encl, sym, callish ? "(...)" : "");
							}
						}
					}
					// ── (4) TYPE connu -> compte des membres. ──
					if (!title[0]) {
						bool isType = false;
						int32 nMem = 0;
						for (usize i = 0; i < dt.memOwner.Size(); ++i)
							if (StrEq(dt.memOwner[i].CStr(), sym)) {
								isType = true;
								++nMem;
							}
						if (projReady)
							for (usize i = 0; i < wsTab.memOwner.Size(); ++i)
								if (StrEq(wsTab.memOwner[i].CStr(), sym)) {
									isType = true;
									++nMem;
								}
						if (!isType && NkSymHas(&doc.symTypes, sym, static_cast<int32>(doc.hovSym.Size())))
							isType = true;
						if (!isType && projReady && NkSymHas(&projTypes, sym, static_cast<int32>(doc.hovSym.Size())))
							isType = true;
						if (isType)
							doc.hovKind = 2;
						if (isType) {
							// GENRE réel depuis le registre (struct/class/union/enum/namespace).
							const char *kindW = nullptr;
							for (usize i = 0; i < dt.tyName.Size() && !kindW; ++i)
								if (StrEq(dt.tyName[i].CStr(), sym))
									kindW = dt.tyKind[i].CStr();
							if (!kindW && projReady)
								for (usize i = 0; i < wsTab.tyName.Size() && !kindW; ++i)
									if (StrEq(wsTab.tyName[i].CStr(), sym))
										kindW = wsTab.tyKind[i].CStr();
							if (!kindW)
								kindW = "type";
							NkVector<NkString> mm; // compte DEDUPLIQUE (heritage inclus, buffer+workspace)
							char seen2[8][96];
							int32 nSeen2 = 0;
							CollectMembersOf(sym, dt, mm, seen2, nSeen2);
							NkSymSortDedup(mm);
							const int32 nMem2 = static_cast<int32>(mm.Size());
							if (StrEq(kindW, "namespace"))
								std::snprintf(title, sizeof(title), "namespace %s", sym);
							else if (nMem2 > 0)
								std::snprintf(title, sizeof(title), "%s %s — %d %s", kindW, sym, nMem2,
											  StrEq(kindW, "enum") ? "valeur(s)" : "membre(s)");
							else
								std::snprintf(title, sizeof(title), "%s %s", kindW, sym);
							docAnchors.PushBack(NkString(kindW) + " " + sym);
							docAnchors.PushBack(NkString("struct ") + sym);
							docAnchors.PushBack(NkString("class ") + sym);
							docAnchors.PushBack(NkString("union ") + sym);
							docAnchors.PushBack(NkString("enum ") + sym);
						}
					}
					// ── (5) FONCTION libre -> prototype complet (sinon retour reconstruit). ──
					if (!title[0]) {
						char rt[96], sig[200];
						if (GlobalFnOf(dt, sym, rt, sig)) {
							if (sig[0]) {
								std::snprintf(title, sizeof(title), "%s", sig);
								docAnchors.PushBack(NkString(sig));
							} else if (rt[0])
								std::snprintf(title, sizeof(title), "%s %s(...)", rt, sym);
						}
					}
					// ── (6) Dernier recours : membre par NOM (ambigu -> préfixe '~'). ──
					if (!title[0]) {
						const char *own = nullptr;
						const char *sg = nullptr;
						const char *mty = nullptr;
						for (usize i = 0; i < dt.memName.Size() && !own; ++i)
							if (StrEq(dt.memName[i].CStr(), sym)) {
								own = dt.memOwner[i].CStr();
								mty = dt.memType[i].CStr();
								sg = dt.memSig[i].CStr();
							}
						if (!own && projReady)
							for (usize i = 0; i < wsTab.memName.Size() && !own; ++i)
								if (StrEq(wsTab.memName[i].CStr(), sym)) {
									own = wsTab.memOwner[i].CStr();
									mty = wsTab.memType[i].CStr();
									sg = wsTab.memSig[i].CStr();
								}
						if (own) {
							if (sg && *sg) {
								std::snprintf(title, sizeof(title), "~ %s", sg);
								docAnchors.PushBack(NkString(sg));
							} else if (mty && *mty)
								std::snprintf(title, sizeof(title), "~ %s %s::%s%s", mty, own, sym,
											  callish ? "(...)" : "");
							else
								std::snprintf(title, sizeof(title), "~ %s::%s%s", own, sym, callish ? "(...)" : "");
							NkString ctx2 = NkString("membre de ") + own + " (par nom, peut-etre ambigu)";
							doc.hovBody.PushBack(ctx2);
						}
					}
					{
						char lb[192];
						std::snprintf(lb, sizeof(lb), "[hover] '%s' L%d:C%d -> %s", sym, doc.hovLine + 1, doc.hovCol,
									  title[0] ? title : "(rien)");
						GlobalLogBuffer().Push(NkString(lb));
					}
					if (!title[0]) {
						doc.hovBody.Clear();
						return; // rien d'utile -> pas de carte
					}
					doc.hovTitle = NkString(title);
					// ── Documentation : commentaires CONTIGUS au-dessus de la définition (buffer). ──
					{
						int32 defLine = -1;
						// Ligne de la VRAIE définition = celle qui contient l'ANCRE (prototype capturé,
						// `struct sym`, `#define sym`) — un site d'appel ne matche plus (fini les
						// commentaires piochés au-dessus d'un appel).
						for (usize ai = 0; ai < docAnchors.Size() && defLine < 0; ++ai) {
							const char *anc = docAnchors[ai].CStr();
							int32 al = static_cast<int32>(docAnchors[ai].Size());
							if (al > 60)
								al = 60; // le début du prototype suffit à l'identifier
							if (al < 4)
								continue;
							for (int32 l = 0; l < doc.LineCount() && defLine < 0; ++l) {
								const NkCodeLine &L = doc.lines[l];
								const int32 n = static_cast<int32>(L.Size());
								for (int32 i = 0; i + al <= n; ++i) {
									int32 k = 0;
									while (k < al && L[i + k] == anc[k])
										++k;
									if (k == al) {
										defLine = l;
										break;
									}
								}
							}
						}
						// Prototype MULTI-LIGNES (clang-format coupe à 120 col) : quand la définition
						// est dans le buffer, rejoint les lignes jusqu'à la ')' équilibrée -> le titre
						// montre la signature COMPLÈTE (pas la 1re ligne tronquée).
						if (defLine >= 0 && title[0] && title[0] != 't' && title[0] != '#') { // pas type/#define
							char joined[224];
							int32 jk = 0;
							int32 depP = 0;
							bool started = false, done = false;
							const bool tilde = (title[0] == '~');
							if (tilde && jk < 220) {
								joined[jk++] = '~';
								joined[jk++] = ' ';
							}
							for (int32 l = defLine; l <= defLine + 3 && l < doc.LineCount() && !done; ++l) {
								const NkCodeLine &L = doc.lines[l];
								const int32 n = static_cast<int32>(L.Size());
								int32 s2 = 0;
								while (s2 < n && (L[s2] == ' ' || L[s2] == '\t'))
									++s2;
								if (l > defLine && jk < 220)
									joined[jk++] = ' ';
								for (int32 i = s2; i < n && jk < 220; ++i) {
									const char c2 = L[i];
									if (c2 == '(') {
										++depP;
										started = true;
									} else if (c2 == ')') {
										--depP;
									}
									joined[jk++] = c2;
									if (started && depP == 0) {
										done = true;
										break;
									}
								}
							}
							while (jk > 0 && (joined[jk - 1] == ' ' || joined[jk - 1] == '\t' || joined[jk - 1] == '{'))
								--jk;
							joined[jk] = 0;
							if (started && done && jk > 4) {
								// garde seulement si le symbole y figure (sécurité anti-mismatch)
								bool hasSym = false;
								const int32 sl2 = static_cast<int32>(doc.hovSym.Size());
								for (int32 i = 0; i + sl2 <= jk && !hasSym; ++i) {
									int32 k = 0;
									while (k < sl2 && joined[i + k] == sym[k])
										++k;
									hasSym = (k == sl2);
								}
								if (hasSym)
									CopyCap(title, joined, 250);
							}
						}
						if (defLine > 0) { // remonte les commentaires contigus (max 10 lignes)
							int32 top = defLine;
							while (top > 0) {
								const NkCodeLine &L = doc.lines[top - 1];
								int32 s2 = 0;
								const int32 n = static_cast<int32>(L.Size());
								while (s2 < n && (L[s2] == ' ' || L[s2] == '\t'))
									++s2;
								const bool cmt =
									(s2 + 1 < n && L[s2] == '/' && (L[s2 + 1] == '/' || L[s2 + 1] == '*')) ||
									(s2 < n && L[s2] == '*');
								if (!cmt || defLine - top >= 24)
									break;
								--top;
							}
							for (int32 l = top; l < defLine && static_cast<int32>(doc.hovBody.Size()) < 24; ++l) {
								const NkCodeLine &L = doc.lines[l];
								int32 s2 = 0;
								const int32 n = static_cast<int32>(L.Size());
								while (s2 < n && (L[s2] == ' ' || L[s2] == '\t'))
									++s2;
								if (s2 + 1 < n && L[s2] == '/' && (L[s2 + 1] == '/' || L[s2 + 1] == '*'))
									s2 += 2;
								else if (s2 < n && L[s2] == '*')
									s2 += 1;
								while (s2 < n && (L[s2] == ' ' || L[s2] == '\t'))
									++s2;
								int32 e2 = n;
								if (e2 >= 2 && L[e2 - 2] == '*' && L[e2 - 1] == '/')
									e2 -= 2;
								NkString line;
								for (int32 k = s2; k < e2; ++k)
									line += L[k];
								if (!line.Empty())
									doc.hovBody.PushBack(line);
							}
						}
					}
					doc.hovShow = true;
				}

				void ProcessCompletionRequest() {
					if (active < 0 || active >= static_cast<int32>(files.Size()))
						return;
					OpenFile &f = files[active];
					NkCodeDoc &doc = f.doc;
					if (!doc.acCtxReq)
						return;
					doc.acCtxReq = false; // consommée : heuristique tout de suite, compilateur si possible
					// ── 1) INSTANTANÉ : membres heuristiques -> popup immédiat. ──
					NkVector<NkString> heur;
					HeuristicMembers(doc, heur);
					if (!heur.Empty()) {
						doc.acCtxAll = heur;
						ApplyCtxFilter(doc, doc.acCtxLine, doc.acCtxCol);
					}
					{
						char lb[160];
						std::snprintf(lb, sizeof(lb), "[ac] heuristique: %d membres (index projet: %s)",
									  static_cast<int32>(heur.Size()), projReady ? "pret" : "en construction");
						GlobalLogBuffer().Push(NkString(lb));
					}
					// ── 2) COMPILATEUR (affine) : famille clang uniquement (NDK/emsdk/OHOS = clang ;
					//       gcc/MSVC restent sur l'heuristique + mots). ──
					if (!cdb.ready) {
						GlobalLogBuffer().Push(NkString("[ac] compilo: .jcdb pas pret (compile-flags en cours)"));
						return;
					}
					if (cdb.msvc) {
						GlobalLogBuffer().Push(
							NkString("[ac] compilo: MSVC (pas de completion CLI) -> heuristique seule"));
						return;
					}
					const bool clangish =
						NkFindSub(cdb.compiler.CStr(), "clang") || NkFindSub(cdb.compiler.CStr(), "zig") ||
						NkFindSub(cdb.compiler.CStr(), "emcc") || NkFindSub(cdb.compiler.CStr(), "em++");
					if (!clangish) {
						GlobalLogBuffer().Push(
							NkString("[ac] compilo: gcc/inconnu (pas -code-completion-at) -> heuristique seule"));
						return;
					}
					if (acProc.Running())
						return; // requête précédente en vol : l'heuristique reste affichée
					const NkString ext = f.path.GetExtension();
					const ProjFlags *pf = IsCppExt(ext.CStr()) ? FlagsForFile(f.path.ToString()) : nullptr;
					if (!pf) {
						GlobalLogBuffer().Push(
							NkString("[ac] compilo: fichier HORS des projets du .jcdb -> heuristique seule"));
						return;
					}
					const bool isC = StrEqI(ext.CStr(), ".c");
					// ── PCH de préambule : valide ? sinon (re)build async (une fois). ──
					NkString preText;
					const int32 preN = PreambleLines(doc, preText);
					bool usePch = false;
					NkString pchPath;
					if (preN > 0) {
						const int64 key = NkFnv(preText.CStr(), NkFnv(flagsSig.CStr(), NkFnv(pf->dir.CStr())));
						int32 idx = -1;
						for (usize i = 0; i < acPchs.Size(); ++i)
							if (StrEq(acPchs[i].file.CStr(), f.path.ToString().CStr())) {
								idx = static_cast<int32>(i);
								break;
							}
						if (idx < 0) {
							AcPch e0;
							e0.file = f.path.ToString();
							acPchs.PushBack(e0);
							idx = static_cast<int32>(acPchs.Size()) - 1;
						}
						AcPch &e = acPchs[idx];
						const bool valid = e.ready && e.hash == key && NkFile::Exists(NkPath(e.pch));
						if (valid) {
							usePch = true;
							pchPath = e.pch;
						} else if ((e.hash != key || e.ready) && pchBuild < 0 && !pchProc.Running()) {
							char hx[24];
							std::snprintf(hx, sizeof(hx), "%08x",
										  static_cast<uint32>(NkFnv(e.file.CStr()) & 0xffffffffLL));
							const NkString hdrName = NkString(".nkcode_ac_") + hx + ".h";
							const NkString pchName = NkString(".nkcode_ac_") + hx + ".pch";
							const NkString hdr = (f.path.GetParent() / hdrName.CStr()).ToString();
							e.pch = (f.path.GetParent() / pchName.CStr()).ToString();
							if (NkFile::WriteAllText(NkPath(hdr), preText)) {
								e.hash = key;
								e.ready = false;
								NkString c2 = CompilerPathPrefix() + "\"" + cdb.compiler.CStr() + "\" ";
								c2 += isC ? "-x c-header " : "-x c++-header ";
								c2 += "-w -Xclang -skip-function-bodies ";
								if (!pf->std.Empty()) {
									c2 += "-std=";
									c2 += pf->std.CStr();
									c2 += " ";
								}
								for (usize i = 0; i < pf->includes.Size(); ++i) {
									c2 += "-I\"";
									c2 += pf->includes[i].CStr();
									c2 += "\" ";
								}
								for (usize i = 0; i < pf->defines.Size(); ++i) {
									c2 += "-D";
									c2 += pf->defines[i].CStr();
									c2 += " ";
								}
								c2 += "-o \"";
								c2 += e.pch.CStr();
								c2 += "\" \"";
								c2 += hdr.CStr();
								c2 += "\" 2>&1";
								pchHdrTemp = hdr;
								pchAcc.Clear();
								pchBuild = idx;
								pchProc.Start(c2);
							}
						}
					}
					// ── Fichier compilé : buffer complet, ou préambule BLANCHI si PCH (lignes gardées). ──
					const NkString tmp = (f.path.GetParent() / ".nkcode_ac.nkcheck").ToString();
					bool wrote = false;
					if (usePch) {
						NkString body;
						for (int32 l2 = 0; l2 < doc.LineCount(); ++l2) {
							if (l2 >= preN) {
								const NkCodeLine &L2 = doc.lines[l2];
								for (usize i2 = 0; i2 < L2.Size(); ++i2)
									body += L2[i2];
							}
							body += '\n';
						}
						wrote = NkFile::WriteAllText(NkPath(tmp), body);
					} else
						wrote = NkFile::WriteAllText(NkPath(tmp), doc.GetText());
					if (!wrote)
						return;
					acTempPath = tmp;
					acReqLine = doc.acCtxLine;
					acReqCol = doc.acCtxCol;
					acUsedPch = usePch;
					acFile = f.path.ToString();
					NkString cmd = CompilerPathPrefix() + "\"" + cdb.compiler.CStr() + "\" -fsyntax-only ";
					cmd += isC ? "-x c " : "-x c++ ";
					// Vitesse : saute les CORPS de fonctions (l'astuce de clangd) + zéro warnings.
					cmd += "-w -fno-caret-diagnostics -Xclang -skip-function-bodies ";
					if (!pf->std.Empty()) {
						cmd += "-std=";
						cmd += pf->std.CStr();
						cmd += " ";
					}
					if (usePch) {
						cmd += "-include-pch \"";
						cmd += pchPath.CStr();
						cmd += "\" ";
					}
					char at[64];
					std::snprintf(at, sizeof(at), ":%d:%d\" ", acReqLine + 1, acReqCol + 1);
					cmd += "-Xclang -code-completion-at=\"";
					cmd += tmp.CStr();
					cmd += at;
					for (usize i = 0; i < pf->includes.Size(); ++i) {
						cmd += "-I\"";
						cmd += pf->includes[i].CStr();
						cmd += "\" ";
					}
					for (usize i = 0; i < pf->defines.Size(); ++i) {
						cmd += "-D";
						cmd += pf->defines[i].CStr();
						cmd += " ";
					}
					cmd += "\"";
					cmd += tmp.CStr();
					cmd += "\" 2>&1";
					acAcc.Clear();
					acTarget = active;
					acProc.Start(cmd);
					GlobalLogBuffer().Push(NkString(acUsedPch ? "[ac] compilo lance (PCH preambule: rapide)"
															  : "[ac] compilo lance (sans PCH: complet)"));
				}

				void PollCompletion() {
					if (projReady && !projLoggedReady) { // l heuristique instantanee devient disponible
						projLoggedReady = true;
						char lb0[128];
						std::snprintf(lb0, sizeof(lb0), "[ac] index projet PRET: %d types, %d membres, %d fonctions",
									  static_cast<int32>(projTypes.Size()), static_cast<int32>(wsTab.memName.Size()),
									  static_cast<int32>(wsTab.gfnName.Size()));
						GlobalLogBuffer().Push(NkString(lb0));
					}
					// PCH de préambule en cours ? (indépendant de la requête de complétion)
					if (pchBuild >= 0 && pchBuild < static_cast<int32>(acPchs.Size())) {
						pchProc.Drain(pchAcc);
						if (pchProc.Done()) {
							AcPch &e = acPchs[pchBuild];
							e.ready = (pchProc.ExitCode() == 0) && NkFile::Exists(NkPath(e.pch));
							if (!e.ready)
								e.hash = -1; // échec -> retentera au prochain déclencheur
							if (!pchHdrTemp.Empty()) {
								NkFile::Delete(NkPath(pchHdrTemp));
								pchHdrTemp = NkString();
							}
							pchBuild = -1;
						}
					}
					if (acTarget < 0)
						return;
					acProc.Drain(acAcc);
					if (!acProc.Done())
						return;
					const int32 tgt = acTarget;
					acTarget = -1;
					if (!acTempPath.Empty()) {
						NkFile::Delete(NkPath(acTempPath));
						acTempPath = NkString();
					}
					if (tgt < 0 || tgt >= static_cast<int32>(files.Size()))
						return;
					NkCodeDoc &doc = files[tgt].doc;
					// Résultats périmés si le caret a changé de ligne ou est revenu avant le point.
					if (doc.curLine != acReqLine || doc.curCol < acReqCol)
						return;
					NkVector<NkString> got; // parse LOCAL : n'écrase l'heuristique que si non-vide
					for (usize li = 0; li < acAcc.Size(); ++li) {
						const char *ln = acAcc[li].CStr();
						const char *pfx = "COMPLETION: ";
						int32 m = 0;
						while (pfx[m] && ln[m] == pfx[m])
							++m;
						if (pfx[m])
							continue; // pas une ligne de complétion
						const char *nm = ln + m;
						int32 e = 0;
						while (nm[e] && nm[e] != ' ' && nm[e] != ':')
							++e;
						if (!e || nm[0] == '~')
							continue; // vide / destructeur
						NkString name;
						for (int32 k = 0; k < e; ++k)
							name += nm[k];
						if (StrEq(name.CStr(), "Pattern") || NkFindSub(name.CStr(), "operator"))
							continue; // snippets clang / opérateurs : bruit
						if (!got.Empty() && StrEq(got.Back().CStr(), name.CStr()))
							continue; // surcharges adjacentes (clang trie) -> dédoublonne
						got.PushBack(name);
						if (got.Size() >= 500)
							break; // garde-fou
					}
					{
						char lb[96];
						std::snprintf(lb, sizeof(lb), "[ac] compilo: %d membres%s", static_cast<int32>(got.Size()),
									  got.Empty() ? " -> heuristique conservee" : "");
						GlobalLogBuffer().Push(NkString(lb));
					}
					if (got.Empty()) {
						// Échec avec PCH (header modifié -> PCH périmé ?) : invalide pour reconstruire.
						if (acUsedPch)
							for (usize i = 0; i < acPchs.Size(); ++i)
								if (StrEq(acPchs[i].file.CStr(), acFile.CStr())) {
									acPchs[i].hash = -1;
									acPchs[i].ready = false;
									break;
								}
						return; // l'heuristique affichée reste en place
					}
					doc.acCtxAll = got;
					ApplyCtxFilter(doc, acReqLine, acReqCol);
				}

				// Defines EFFECTIFS pour griser les branches préproc du fichier : macros réellement
				// définies (builtins compilateur + defines projet + DÉRIVÉES des headers). Renvoie
				// nullptr tant que le dump n'est pas prêt (pas de grisage plutôt qu'un grisage faux).
				// Lance le dump `<compilateur> -dM -E <fichier>` à la demande (async, une fois/projet).
				const NkVector<NkString> *EffectiveDefines(const NkString &filePath) {
					const ProjFlags *pf = FlagsForFile(filePath);
					if (!pf || pf->dir.Empty())
						return nullptr;
					for (usize i = 0; i < macroSets.Size(); ++i)
						if (StrEq(macroSets[i].dir.CStr(), pf->dir.CStr()))
							return macroSets[i].ready ? &macroSets[i].defs : nullptr;
					macroSets.PushBack(MacroSet{pf->dir, {}, false}); // pending
					if (!macroBusy && cdb.ready) {
						const NkString ext = NkPath(filePath).GetExtension();
						const bool isC = StrEqI(ext.CStr(), ".c");
						NkString cmd = CompilerPathPrefix() + "\"" + cdb.compiler.CStr() + "\" ";
						if (cdb.msvc) {
							cmd += "/EP /nologo ";
						} // MSVC : pas de dump macros fiable -> best effort
						else {
							cmd += "-dM -E ";
							cmd += isC ? "-x c " : "-x c++ ";
							if (!pf->std.Empty()) {
								cmd += "-std=";
								cmd += pf->std.CStr();
								cmd += " ";
							}
						}
						for (usize i = 0; i < pf->includes.Size(); ++i) {
							cmd += cdb.msvc ? "/I\"" : "-I\"";
							cmd += pf->includes[i].CStr();
							cmd += "\" ";
						}
						for (usize i = 0; i < pf->defines.Size(); ++i) {
							cmd += cdb.msvc ? "/D" : "-D";
							cmd += pf->defines[i].CStr();
							cmd += " ";
						}
						cmd += "\"";
						cmd += filePath.CStr();
						cmd += "\" 2>&1";
						macroAcc.Clear();
						if (macroProc.Start(cmd)) {
							macroBusy = true;
							macroDir = pf->dir;
						}
					}
					return nullptr;
				}

				void PollMacros() {
					if (!macroBusy)
						return;
					macroProc.Drain(macroAcc);
					if (!macroProc.Done())
						return;
					macroBusy = false;
					MacroSet *set = nullptr;
					for (usize i = 0; i < macroSets.Size(); ++i)
						if (StrEq(macroSets[i].dir.CStr(), macroDir.CStr())) {
							set = &macroSets[i];
							break;
						}
					if (!set)
						return;
					set->defs.Clear();
					// Chaque ligne : `#define NAME[(...)] [corps]`. On garde NAME (+ =valeur si entier).
					for (usize li = 0; li < macroAcc.Size(); ++li) {
						const char *p = macroAcc[li].CStr();
						while (*p == ' ' || *p == '\t')
							++p;
						if (*p != '#')
							continue;
						++p;
						while (*p == ' ' || *p == '\t')
							++p;
						if (NkFindSub(p, "define") != p)
							continue;
						p += 6;
						while (*p == ' ' || *p == '\t')
							++p;
						const char *name = p;
						int32 nl = 0;
						while ((p[nl] >= 'A' && p[nl] <= 'Z') || (p[nl] >= 'a' && p[nl] <= 'z') ||
							   (p[nl] >= '0' && p[nl] <= '9') || p[nl] == '_')
							++nl;
						if (nl == 0)
							continue;
						NkString entry;
						for (int32 k = 0; k < nl; ++k)
							entry += name[k];
						const char *body = name + nl;
						if (*body != '(') { // object-like : corps entier simple -> NAME=valeur
							while (*body == ' ' || *body == '\t')
								++body;
							bool allDigit = (*body >= '0' && *body <= '9');
							const char *b = body;
							while (*b >= '0' && *b <= '9')
								++b;
							while (*b == 'L' || *b == 'l' || *b == 'U' || *b == 'u')
								++b;
							while (*b == ' ' || *b == '\t' || *b == '\r' || *b == '\n')
								++b;
							if (allDigit && *b == '\0') {
								entry += '=';
								for (const char *c = body; *c >= '0' && *c <= '9'; ++c)
									entry += *c;
							}
						}
						set->defs.PushBack(entry);
					}
					set->ready = true;
				}

				// ── Ctrl+clic (navigation façon VSCode) : consomme d.linkTarget du fichier actif ──
				static bool IsHdrSrc(const char *e) {
					return StrEqI(e, ".h") || StrEqI(e, ".hpp") || StrEqI(e, ".hh") || StrEqI(e, ".hxx") ||
						   StrEqI(e, ".inl") || StrEqI(e, ".cpp") || StrEqI(e, ".cc") || StrEqI(e, ".cxx") ||
						   StrEqI(e, ".c");
				}

				// Ligne (0-based) de la DÉFINITION de `sym` dans `text`, ou -1. Priorité aux définitions
				// fortes (class/struct/enum/union/namespace/typedef/using/#define) ; repli faible (`sym(`).
				static int32 DefLineOf(const char *text, const char *sym) {
					int32 sl = 0;
					while (sym[sl])
						++sl;
					if (sl == 0)
						return -1;
					auto isW = [](char c) {
						return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
					};
					int32 line = 0, weak = -1;
					const char *p = text;
					while (*p) {
						const char *ls = p;
						while (*p && *p != '\n')
							++p; // [ls,p) = ligne courante
						// Scan BORNÉ À LA LIGNE (jamais jusqu'à la fin du fichier -> évite un O(taille²)
						// qui figeait la recherche sur les gros headers).
						for (const char *q = ls; q + sl <= p; ++q) {
							bool m = true;
							for (int32 t = 0; t < sl; ++t)
								if (q[t] != sym[t]) {
									m = false;
									break;
								}
							if (!m)
								continue;
							const char before = (q > ls) ? q[-1] : ' ', after = q[sl];
							if (isW(before) || isW(after))
								continue; // frontière de mot
							const char *kw = q;
							while (kw > ls && (kw[-1] == ' ' || kw[-1] == '\t'))
								--kw; // mot-clé juste avant
							auto pre = [&](const char *w) {
								int32 wl = 0;
								while (w[wl])
									++wl;
								if (kw - ls < wl)
									return false;
								const char *s = kw - wl;
								for (int32 t = 0; t < wl; ++t)
									if (s[t] != w[t])
										return false;
								return (s == ls || s[-1] == ' ' || s[-1] == '\t');
							};
							if (pre("class") || pre("struct") || pre("enum") || pre("union") || pre("namespace") ||
								pre("typedef") || pre("using"))
								return line;
							{
								const char *h = ls;
								while (h < q && (*h == ' ' || *h == '\t'))
									++h;
								if (*h == '#') {
									const char *d = h + 1;
									while (*d == ' ' || *d == '\t')
										++d;
									if (d[0] == 'd' && d[1] == 'e' && d[2] == 'f' && d[3] == 'i' && d[4] == 'n' &&
										d[5] == 'e')
										return line;
								}
							}
							// Membre d'ÉNUMÉRATION : symbole = 1er token de la ligne, suivi de ',' / '}' / '='
							// (sans
							// ';', sinon c'est une affectation).
							{
								const char *h2 = ls;
								while (h2 < q && (*h2 == ' ' || *h2 == '\t'))
									++h2;
								if (h2 == q) {
									const char *a = q + sl;
									while (*a == ' ' || *a == '\t')
										++a;
									bool semi = false;
									for (const char *z = ls; z < p; ++z)
										if (*z == ';') {
											semi = true;
											break;
										}
									if (*a == ',' || *a == '}' || (*a == '=' && !semi))
										return line;
								}
							}
							// ── Repli FAIBLE : DÉFINITION de fonction/méthode ou DÉCLARATION de variable —
							//    JAMAIS un APPEL. Exclut `obj.f()`/`obj->f()` (membre) ; un `f(`/`Ns::f(` n'est
							//    une définition que s'il est précédé d'un TYPE de retour (sinon = appel). ──
							if (weak < 0) {
								auto isKw = [](const char *s, const char *e) {
									auto eq = [&](const char *w) {
										int32 wl = 0;
										while (w[wl])
											++wl;
										if ((int32)(e - s) != wl)
											return false;
										for (int32 t = 0; t < wl; ++t)
											if (s[t] != w[t])
												return false;
										return true;
									};
									return eq("return") || eq("else") || eq("case") || eq("delete") || eq("new") ||
										   eq("sizeof") || eq("throw") || eq("goto") || eq("if") || eq("while") ||
										   eq("for") || eq("switch") || eq("do") || eq("co_await") || eq("co_return") ||
										   eq("co_yield") || eq("const") || eq("static") || eq("inline") ||
										   eq("virtual") || eq("explicit") || eq("friend") || eq("constexpr") ||
										   eq("noexcept") || eq("template") || eq("using") || eq("typedef");
								};
								const char *pbk = q;
								while (pbk > ls && (pbk[-1] == ' ' || pbk[-1] == '\t'))
									--pbk;
								const bool member =
									(pbk > ls && pbk[-1] == '.') || (pbk - 1 > ls && pbk[-1] == '>' && pbk[-2] == '-');
								const char *a2 = q + sl;
								while (*a2 == ' ' || *a2 == '\t')
									++a2;
								if (!member && *a2 == '(') { // fonction/méthode : DÉFINITION seulement si TYPE
															 // devant (chaîne A::B:: remontée)
									const char *c = pbk;
									while (c - 1 > ls && c[-1] == ':' && c[-2] == ':') {
										c -= 2;
										while (c > ls && (c[-1] == ' ' || c[-1] == '\t'))
											--c;
										while (c > ls && isW(c[-1]))
											--c;
										while (c > ls && (c[-1] == ' ' || c[-1] == '\t'))
											--c;
									}
									const char *te = c;
									while (te > ls && (te[-1] == ' ' || te[-1] == '\t' || te[-1] == '&' ||
													   te[-1] == '*' || te[-1] == '>'))
										--te;
									if (te > ls && isW(te[-1])) {
										const char *ts = te;
										while (ts > ls && isW(ts[-1]))
											--ts;
										if (!isKw(ts, te))
											weak = line;
									}
								} else if (!member &&
										   (*a2 == ';' || *a2 == '=' || *a2 == ',' || *a2 == ')' || *a2 == '[' ||
											*a2 == '{')) { // déclaration de variable `Type nom`
									const char *b = q;
									while (b > ls && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '&' || b[-1] == '*'))
										--b;
									if (b > ls && isW(b[-1])) {
										const char *ts = b;
										while (ts > ls && isW(ts[-1]))
											--ts;
										if (!isKw(ts, b))
											weak = line;
									}
								}
							}
						}
						if (*p == '\n')
							++p;
						++line;
					}
					return weak;
				}

				void OpenAt(const NkPath &p, int32 line) {
					OpenPath(p);
					if (active >= 0 && line >= 0) {
						OpenFile &g = files[active];
						g.doc.curLine = line;
						g.doc.curCol = 0;
						g.doc.selLine = line;
						g.doc.selCol = 0; // PAS de sélection (juste le curseur)
						g.doc.ClampCursor();
						g.doc.ResetEditRun();
						g.doc.wantReveal = true; // le scroll se positionne sur la ligne
					}
				}

				void ProcessNavigation() {
					if (active < 0 || active >= static_cast<int32>(files.Size()))
						return;
					OpenFile &f = files[active];
					// ── Maj+F12 / bouton [Références] de la carte : consomme d.refsTarget ──
					if (!f.doc.refsTarget.Empty()) {
						const NkString sym = f.doc.refsTarget;
						f.doc.refsTarget = NkString(); // consomme (une seule action)
						if (!navPickerOpen && navPickChoice < 0) {
							const ProjFlags *pf = FlagsForFile(f.path.ToString());
							if (pf)
								StartRefs(sym, pf);
							else
								status = NkString("Références : projet inconnu pour ") + sym.CStr();
						}
					}
					if (f.doc.linkTarget.Empty())
						return;
					// Picker ouvert / choix en attente -> l'utilisateur interagit avec la LISTE
					// (souvent Ctrl encore enfoncé) : on ignore le lien Ctrl de l'éditeur dessous.
					if (navPickerOpen || navPickChoice >= 0) {
						f.doc.linkTarget = NkString();
						return;
					}
					const NkString tgt = f.doc.linkTarget;
					const bool inc = f.doc.linkIsInclude;
					f.doc.linkTarget = NkString(); // consomme (une seule action)
					const ProjFlags *pf = FlagsForFile(f.path.ToString());
					if (inc) { // ── #include "x.h" / <x.h> : dossier courant puis include dirs du projet ──
						NkPath c0 = f.path.GetParent() / NkPath(tgt);
						if (NkFile::Exists(c0)) {
							OpenAt(c0, -1);
							return;
						}
						if (pf)
							for (usize i = 0; i < pf->includes.Size(); ++i) {
								NkPath c = NkPath(pf->includes[i]) / NkPath(tgt);
								if (NkFile::Exists(c)) {
									OpenAt(c, -1);
									return;
								}
							}
						status = NkString("Include introuvable : ") + tgt.CStr();
						return;
					}
					// ── Symbole : 1) définition dans le FICHIER ACTIF -> saut IMMÉDIAT (variable locale,
					//    type/fonction locale) : pas de thread, pas de liste. ──
					{
						const int32 lnA = DefLineOf(f.doc.GetText().CStr(), tgt.CStr());
						if (lnA >= 0) {
							f.doc.curLine = lnA;
							f.doc.curCol = 0;
							f.doc.selLine = lnA;
							f.doc.selCol = 0;
							f.doc.ClampCursor();
							f.doc.ResetEditRun();
							f.doc.wantReveal = true;
							status = NkString();
							return;
						}
					}
					// 2) sinon go-to-definition PROJET sur un THREAD (progression + liste des occurrences).
					if (!pf) {
						status = NkString("Définition introuvable : ") + tgt.CStr();
						return;
					}
					StartGotoDef(tgt, pf);
				}

				// ── Go-to-definition ASYNCHRONE (thread) : collecte TOUTES les définitions ────────
				struct NavHit {
						NkString file;
						int32 line;
						NkString preview;
				};

				NkThread navThread;
				bool navBusy = false; // écrit par l'UI, lu simple (comme NkProcess.mRunning)
				bool navDone = false; // écrit par le thread à la fin
				NkString navSym;
				int32 navMode = 0;			// 0 = définitions (F12/Ctrl+clic), 1 = références (Maj+F12)
				NkVector<NkString> navDirs; // entrées : symbole + dossiers d'include
				NkVector<NkString> navOpenPaths, navOpenTexts; // snapshots des fichiers ouverts (buffers)
				NkVector<NavHit> navResults;				   // sortie : 1 hit par fichier (déf. trouvée)
				int32 navScanned = 0, navTotal = 0;			   // progression (thread -> UI)
				bool navPickerOpen = false;
				int32 navPickerSel = 0;	 // liste de choix si >1
				bool navCancel = false;	 // annulation COOPÉRATIVE (le thread lit ce drapeau)
				bool navPending = false; // une nouvelle recherche attend l'arrêt de l'actuelle
				NkString navPendSym;
				int32 navPendMode = 0;
				NkVector<NkString> navPendDirs, navPendOpenP, navPendOpenT;

				void StartNav(const NkString &sym, const ProjFlags *pf, int32 mode) {
					// Snapshots des entrées (dossiers d'include + buffers ouverts).
					NkVector<NkString> dirs;
					for (usize i = 0; i < pf->includes.Size(); ++i)
						dirs.PushBack(pf->includes[i]);
					NkVector<NkString> openP, openT;
					for (usize i = 0; i < files.Size(); ++i) {
						openP.PushBack(files[i].path.ToString());
						openT.PushBack(files[i].doc.GetText());
					}
					if (navBusy) { // recherche en cours -> l'ANNULER et mémoriser la nouvelle (relancée dès
								   // l'arrêt)
						navCancel = true;
						navPending = true;
						navPendSym = sym;
						navPendMode = mode;
						navPendDirs = dirs;
						navPendOpenP = openP;
						navPendOpenT = openT;
						status = NkString("Nouvelle recherche de « ") + sym.CStr() + " »…";
						return;
					}
					NavLaunch(sym, dirs, openP, openT, mode);
				}

				void StartGotoDef(const NkString &sym, const ProjFlags *pf) {
					StartNav(sym, pf, 0);
				}

				void StartRefs(const NkString &sym, const ProjFlags *pf) {
					StartNav(sym, pf, 1);
				}

				void NavLaunch(const NkString &sym, const NkVector<NkString> &dirs, const NkVector<NkString> &openP,
							   const NkVector<NkString> &openT, int32 mode) {
					if (navThread.Joinable())
						navThread.Join();
					navSym = sym;
					navMode = mode;
					navDirs = dirs;
					navOpenPaths = openP;
					navOpenTexts = openT;
					navResults.Clear();
					navScanned = 0;
					navTotal = 0;
					navPickerOpen = false;
					navPickerSel = 0;
					navCancel = false;
					navDone = false;
					navBusy = true;
					status =
						(mode == 1 ? NkString("Références de « ") : NkString("Recherche de « ")) + sym.CStr() + " »…";
					navThread = NkThread([this](void *) { NavScan(); });
				}

				static NkString LineTextOf(const char *text, int32 target) { // ligne `target` (0-based), rognée
					int32 ln = 0;
					const char *p = text;
					while (*p && ln < target) {
						if (*p == '\n')
							++ln;
						++p;
					}
					while (*p == ' ' || *p == '\t')
						++p;
					NkString o;
					for (const char *q = p; *q && *q != '\n' && *q != '\r' && (int32)o.Size() < 120; ++q)
						o += *q;
					return o;
				}

				// Lignes (0-based) contenant `sym` en FRONTIÈRE DE MOT (mode références), HORS
				// commentaires (// et /* */) et chaînes/caractères — au plus `cap` lignes par fichier.
				static void RefLinesOf(const char *text, const char *sym, NkVector<int32> &out, int32 cap) {
					int32 sl = 0;
					while (sym[sl])
						++sl;
					if (sl == 0)
						return;
					auto isW = [](char c) {
						return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
					};
					int32 line = 0;
					bool inBlock = false, lineHit = false;
					const char *p = text;
					while (*p && static_cast<int32>(out.Size()) < cap) {
						const char c = *p;
						if (c == '\n') { // fin de ligne : reset des etats LIGNE (chaines non multi-lignes)
							++line;
							lineHit = false;
							++p;
							continue;
						}
						if (inBlock) { // dans /* ... */
							if (c == '*' && p[1] == '/') {
								inBlock = false;
								++p;
							}
							++p;
							continue;
						}
						if (c == '/' && p[1] == '/') { // commentaire ligne : saute jusqu'a la fin de ligne
							while (*p && *p != '\n')
								++p;
							continue;
						}
						if (c == '/' && p[1] == '*') {
							inBlock = true;
							p += 2;
							continue;
						}
						if (c == '"' || c == '\'') { // chaine / caractere : saute (gere \\ et \")
							const char q = c;
							++p;
							while (*p && *p != '\n' && *p != q) {
								if (*p == '\\' && p[1])
									++p;
								++p;
							}
							if (*p == q)
								++p;
							continue;
						}
						if (!lineHit && *p == sym[0]) { // candidat : compare + frontieres de mot
							bool m = true;
							for (int32 t = 0; t < sl; ++t)
								if (p[t] != sym[t]) {
									m = false;
									break;
								}
							if (m) {
								const char before = (p > text) ? p[-1] : ' ';
								if (!isW(before) && !isW(p[sl])) {
									out.PushBack(line);
									lineHit = true; // une entree par ligne suffit
								}
							}
						}
						++p;
					}
				}

				// Marche récursive ANNULABLE : liste chaque dossier en TOP-ONLY (rapide) puis récurse,
				// en vérifiant `navCancel` très souvent -> l'annulation est quasi immédiate (contrairement
				// à un GetEntries récursif unique, non interruptible qui « bloquait à un niveau »).
				void NavWalk(const NkPath &dir, NkVector<NkPath> &named, NkVector<NkPath> &others, int32 &budget,
							 int32 depth) {
					if (navCancel || budget <= 0 || depth > 24)
						return; // limite de profondeur = anti-cycle
					NkVector<NkDirectoryEntry> es =
						NkDirectory::GetEntries(dir, "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
					for (usize k = 0; k < es.Size(); ++k) {
						if (navCancel || budget <= 0)
							return;
						--budget; // CHAQUE entrée (fichier OU dossier) consomme du budget -> les
								  // jonctions/cycles s'arrêtent
						if (es[k].IsDirectory)
							NavWalk(es[k].FullPath, named, others, budget, depth + 1);
						else if (IsHdrSrc(es[k].FullPath.GetExtension().CStr())) {
							if (StrEq(es[k].FullPath.GetFileNameWithoutExtension().CStr(), navSym.CStr()))
								named.PushBack(es[k].FullPath);
							else
								others.PushBack(es[k].FullPath);
						}
					}
				}

				void NavScan() { // THREAD : lit navSym/navDirs/navOpen* + FS ; écrit navResults/navScanned/navTotal
					const bool refs = (navMode == 1);
					const usize kMaxReads = 700, kMaxHits = refs ? 200 : 100;
					auto already = [&](const NkString &f) {
						for (usize i = 0; i < navResults.Size(); ++i)
							if (StrEq(navResults[i].file.CStr(), f.CStr()))
								return true;
						return false;
					};
					// Collecte MODE-AWARE : défs = 1 ligne (DefLineOf) ; réfs = occurrences en frontière
					// de mot, plafonnées PAR FICHIER pour que la liste reste lisible.
					auto collect = [&](const NkString &file, const char *txt) {
						if (!refs) {
							const int32 ln = DefLineOf(txt, navSym.CStr());
							if (ln >= 0 && !already(file))
								navResults.PushBack(NavHit{file, ln, LineTextOf(txt, ln)});
							return;
						}
						NkVector<int32> lines;
						RefLinesOf(txt, navSym.CStr(), lines, 8);
						for (usize k = 0; k < lines.Size() && navResults.Size() < kMaxHits; ++k)
							navResults.PushBack(NavHit{file, lines[k], LineTextOf(txt, lines[k])});
					};
					// 1) fichiers ouverts (buffers snapshottés — inclut les non sauvegardés)
					for (usize i = 0; i < navOpenPaths.Size() && !navCancel; ++i)
						collect(navOpenPaths[i], navOpenTexts[i].CStr());
					// 2) énumération annulable de l'arbre (homonymes prioritaires)
					NkVector<NkPath> named, others;
					int32 budget = 30000; // budget = ENTRÉES (fichiers + dossiers) visitées
					for (usize i = 0; i < navDirs.Size() && !navCancel; ++i)
						NavWalk(NkPath(navDirs[i]), named, others, budget, 0);
					NkVector<NkPath> order;
					for (usize i = 0; i < named.Size(); ++i)
						order.PushBack(named[i]);
					for (usize i = 0; i < others.Size() && order.Size() < kMaxReads; ++i)
						order.PushBack(others[i]);
					navTotal = static_cast<int32>(order.Size());
					// 3) lecture bornée + annulable
					for (usize i = 0; i < order.Size() && navResults.Size() < kMaxHits; ++i) {
						if (navCancel)
							break;
						navScanned = static_cast<int32>(i) + 1;
						const NkString f = order[i].ToString();
						if (already(f))
							continue;
						const NkString txt = NkFile::ReadAllText(order[i]);
						collect(f, txt.CStr());
					}
					if (!navCancel)
						navScanned = navTotal;
					navDone = true;
				}

				void PollNav() { // UI : rejoint le thread fini ; relance si une recherche est en attente
					if (!navBusy || !navDone)
						return;
					if (navThread.Joinable())
						navThread.Join();
					navBusy = false;
					if (navPending) {
						navPending = false;
						NavLaunch(navPendSym, navPendDirs, navPendOpenP, navPendOpenT, navPendMode);
						return;
					}
					if (navCancel) {
						navCancel = false;
						return;
					} // annulée sans nouvelle demande
					if (navResults.Empty()) {
						status =
							(navMode == 1 ? NkString("Aucune référence : ") : NkString("Définition introuvable : ")) +
							navSym.CStr();
						return;
					}
					if (navResults.Size() == 1) {
						OpenAt(NkPath(navResults[0].file), navResults[0].line);
						status = NkString();
						return;
					}
					navPickerOpen = true;
					navPickerSel = 0; // plusieurs -> liste de choix (façon VSCode)
					status = NkString();
				}

				int32 navPickChoice = -1; // choix mémorisé (traité HORS rendu, cf. ProcessNavPick)

				void NavPick(int32 i) { // appelé DEPUIS le rendu du picker -> on ne fait QUE mémoriser :
					// ouvrir ici ferait OpenPath -> files.PushBack -> réalloc pendant que le panneau
					// éditeur tient une référence `OpenFile& f` -> use-after-free (crash). On diffère.
					navPickChoice = i;
					navPickerOpen = false;
				}

				void ProcessNavPick() { // appelé dans le poll (AVANT le rendu des panneaux) -> ouverture sûre
					if (navPickChoice < 0)
						return;
					const int32 i = navPickChoice;
					navPickChoice = -1;
					if (i >= 0 && i < static_cast<int32>(navResults.Size()))
						OpenAt(NkPath(navResults[i].file), navResults[i].line);
				}

				// ── LSP clangd (étape A) : process long-vivant + compile_commands.json généré depuis
				//    le .jcdb. Diagnostics clangd TRACÉS dans OUTPUT ([lsp]) en parallèle du compile-first
				//    (comparaison côte à côte avant bascule). clangd absent -> repli silencieux. ──
				NkLspClient lsp;
				int32 lspState = 0; // 0 = pas tenté, 1 = actif, 2 = indisponible
				NkString lspActive; // fichier suivi (didOpen envoyé)
				int64 lspSig = 0;
				float32 lspTimer = 0.f;

				// clangd vit généralement à côté du compilateur (msys2/LLVM) ; sinon PATH.
				static NkString DeriveClangd(const NkString &compiler) {
					const char *s2 = compiler.CStr();
					int32 cut = -1;
					for (int32 i = 0; s2[i]; ++i)
						if (s2[i] == '\\' || s2[i] == '/')
							cut = i;
					if (cut < 0)
						return NkString("clangd");
					NkString d2;
					for (int32 i = 0; i <= cut; ++i)
						d2 += s2[i];
					d2 += "clangd.exe";
					return NkFile::Exists(NkPath(d2)) ? d2 : NkString("clangd");
				}

				// Sources d'un projet (arbre borné, dossiers générés exclus) pour compile_commands.json.
				void CcWalk(const NkPath &dir, NkVector<NkPath> &out, int32 &budget, int32 depth) {
					if (budget <= 0 || depth > 24)
						return;
					NkVector<NkDirectoryEntry> es =
						NkDirectory::GetEntries(dir, "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
					for (usize k = 0; k < es.Size(); ++k) {
						if (budget <= 0)
							return;
						--budget;
						if (es[k].IsDirectory) {
							const NkString nm = es[k].FullPath.GetFileName();
							if (StrEq(nm.CStr(), ".git") || StrEq(nm.CStr(), "Build") || StrEq(nm.CStr(), ".nkcode"))
								continue;
							CcWalk(es[k].FullPath, out, budget, depth + 1);
						} else {
							const NkString e2 = es[k].FullPath.GetExtension();
							if (StrEqI(e2.CStr(), ".cpp") || StrEqI(e2.CStr(), ".cc") || StrEqI(e2.CStr(), ".cxx") ||
								StrEqI(e2.CStr(), ".c"))
								out.PushBack(es[k].FullPath);
						}
					}
				}

				// compile_commands.json (format clang) depuis le .jcdb : UNE entrée par source de chaque
				// projet, flags identiques à ceux des diagnostics compile-first. Écrit dans .nkcode/.
				void GenCompileCommands() {
					auto esc = [](const NkString &in) {
						NkString o;
						for (const char *q = in.CStr(); *q; ++q) {
							if (*q == '\\' || *q == '"')
								o += '\\';
							o += *q;
						}
						return o;
					};
					NkString js("[\n");
					int32 budget = 60000, wrote = 0;
					for (usize pi = 0; pi < cdb.projects.Size(); ++pi) {
						const ProjFlags &pf = cdb.projects[pi];
						if (pf.dir.Empty())
							continue;
						NkString base = NkString("\\\"") + esc(cdb.compiler).CStr() + "\\\"";
						if (!pf.std.Empty()) {
							base += " -std=";
							base += pf.std.CStr();
						}
						for (usize i2 = 0; i2 < pf.includes.Size(); ++i2) {
							base += " -I\\\"";
							base += esc(pf.includes[i2]).CStr();
							base += "\\\"";
						}
						for (usize i2 = 0; i2 < pf.defines.Size(); ++i2) {
							base += " -D";
							base += pf.defines[i2].CStr();
						}
						NkVector<NkPath> srcs;
						CcWalk(NkPath(pf.dir), srcs, budget, 0);
						for (usize i2 = 0; i2 < srcs.Size(); ++i2) {
							const NkString f2 = srcs[i2].ToString();
							if (wrote)
								js += ",\n";
							js += "  {\"directory\": \"";
							js += esc(pf.dir).CStr();
							js += "\", \"command\": \"";
							js += base.CStr();
							js += " -c \\\"";
							js += esc(f2).CStr();
							js += "\\\"\", \"file\": \"";
							js += esc(f2).CStr();
							js += "\"}";
							++wrote;
						}
					}
					js += "\n]\n";
					NkFile::WriteAllText(root / ".nkcode" / "compile_commands.json", js);
					char lb[96];
					std::snprintf(lb, sizeof(lb), "[lsp] compile_commands.json : %d entree(s)", wrote);
					GlobalLogBuffer().Push(NkString(lb));
				}

				void TickLsp(float32 dt) {
					if (lspState == 0) {
						if (!cdb.ready || root.ToString().Empty())
							return;
						lspState = 2;
						if (!cdb.msvc && NkFindSub(cdb.compiler.CStr(), "clang")) {
							GenCompileCommands();
							const NkString ccDir = (root / ".nkcode").ToString();
							if (lsp.Start(DeriveClangd(cdb.compiler), root.ToString(), ccDir))
								lspState = 1;
						}
						if (lspState == 2)
							GlobalLogBuffer().Push(NkString("[lsp] clangd indisponible - repli compile-first seul"));
						return;
					}
					if (lspState != 1)
						return;
					if (!lsp.Running()) { // clangd mort en route -> repli
						lspState = 2;
						GlobalLogBuffer().Push(NkString("[lsp] clangd s'est arrete - repli compile-first seul"));
						return;
					}
					lsp.Poll();
					for (usize i = 0; i < lsp.log.Size(); ++i)
						GlobalLogBuffer().Push(lsp.log[i]);
					lsp.log.Clear();
					if (lsp.diagsFresh)
						lsp.diagsFresh = false; // étape A : trace OUTPUT uniquement (bascule des squiggles = étape B)
					if (!lsp.Ready() || !HasActive())
						return;
					OpenFile &f = files[active];
					if (!IsCppExt(f.path.GetExtension().CStr()))
						return;
					const NkString p2 = f.path.ToString();
					const int64 sig = f.doc.SymSig();
					if (!StrEq(p2.CStr(), lspActive.CStr())) { // nouvel onglet actif -> didOpen
						lspActive = p2;
						lspSig = sig;
						lspTimer = 0.f;
						lsp.DidOpen(p2, f.doc.GetText());
						return;
					}
					if (sig != lspSig) { // frappe -> didChange (débounce 0,5 s, texte FULL)
						lspTimer += dt;
						if (lspTimer >= 0.5f) {
							lspSig = sig;
							lspTimer = 0.f;
							lsp.DidChange(p2, f.doc.GetText());
						}
					} else
						lspTimer = 0.f;
				}

				// ── Recherche WORKSPACE (Ctrl+Maj+F) : plein texte multi-fichiers sur THREAD, panneau
				//    « Recherche ». Même hygiène que la navigation : annulable, relance en attente,
				//    ouverture des résultats DIFFÉRÉE au poll (mutation de `files` interdite au rendu). ──
				struct WsHit {
						NkString file;
						int32 line, col;
						NkString preview;
				};

				NkThread wsThread;
				bool wsBusy = false, wsDone = false, wsCancel = false, wsPending = false;
				NkString wsQuery, wsPendQuery;
				bool wsCase = false, wsWord = false, wsPendCase = false, wsPendWord = false;
				NkVector<NkString> wsOpenP, wsOpenT, wsPendOpenP, wsPendOpenT; // snapshots buffers ouverts
				NkVector<WsHit> wsResults;
				int32 wsScanned = 0, wsTotal = 0, wsFileCount = 0;
				bool wsFocusReq = false; // Ctrl+Maj+F -> le panneau prend le focus du champ
				NkString wsPrefill;		 // sélection de l'éditeur préremplie dans le champ
				NkString wsOpenFile;	 // clic sur un résultat : consommé par ProcessWsOpen (poll)
				int32 wsOpenLine = -1;

				void StartWsFind(const NkString &q, bool cs, bool ww) {
					if (q.Empty())
						return;
					NkVector<NkString> openP, openT;
					for (usize i = 0; i < files.Size(); ++i) {
						openP.PushBack(files[i].path.ToString());
						openT.PushBack(files[i].doc.GetText());
					}
					if (wsBusy) { // recherche en cours -> l'annuler, mémoriser la nouvelle
						wsCancel = true;
						wsPending = true;
						wsPendQuery = q;
						wsPendCase = cs;
						wsPendWord = ww;
						wsPendOpenP = openP;
						wsPendOpenT = openT;
						return;
					}
					WsLaunch(q, cs, ww, openP, openT);
				}

				void WsLaunch(const NkString &q, bool cs, bool ww, const NkVector<NkString> &openP,
							  const NkVector<NkString> &openT) {
					if (wsThread.Joinable())
						wsThread.Join();
					wsQuery = q;
					wsCase = cs;
					wsWord = ww;
					wsOpenP = openP;
					wsOpenT = openT;
					wsResults.Clear();
					wsScanned = 0;
					wsTotal = 0;
					wsFileCount = 0;
					wsCancel = false;
					wsDone = false;
					wsBusy = true;
					wsThread = NkThread([this](void *) { WsScan(); });
				}

				static char WsLow(char c) {
					return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
				}

				// Fichiers texte candidats : code + docs + scripts de build.
				static bool WsSearchable(const char *e) {
					return IsHdrSrc(e) || StrEqI(e, ".md") || StrEqI(e, ".txt") || StrEqI(e, ".py") ||
						   StrEqI(e, ".jenga") || StrEqI(e, ".nksl") || StrEqI(e, ".glsl") || StrEqI(e, ".json") ||
						   StrEqI(e, ".xml") || StrEqI(e, ".cfg") || StrEqI(e, ".ini");
				}

				// Occurrences de `q` dans `text` (casse/mot selon options) -> pousse des hits (cap par fichier).
				void WsCollect(const NkString &file, const char *text, usize maxHits) {
					const char *q = wsQuery.CStr();
					int32 ql = 0;
					while (q[ql])
						++ql;
					if (ql == 0)
						return;
					auto isW = [](char c) {
						return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
					};
					int32 line = 0;
					usize mine = 0;
					const char *p = text;
					while (*p && mine < maxHits && wsResults.Size() < 800 && !wsCancel) {
						const char *ls = p;
						while (*p && *p != '\n')
							++p;
						for (const char *w = ls; w + ql <= p; ++w) {
							bool m = true;
							for (int32 t = 0; t < ql && m; ++t)
								m = wsCase ? (w[t] == q[t]) : (WsLow(w[t]) == WsLow(q[t]));
							if (!m)
								continue;
							if (wsWord) {
								const char before = (w > ls) ? w[-1] : ' ', after = w[ql];
								if (isW(before) || isW(after))
									continue;
							}
							// aperçu : la ligne rognée (début à l'indentation près)
							const char *ps = ls;
							while (*ps == ' ' || *ps == '\t')
								++ps;
							NkString pv;
							for (const char *z = ps; z < p && pv.Size() < 160; ++z)
								pv += *z;
							wsResults.PushBack(WsHit{file, line, static_cast<int32>(w - ls), pv});
							++mine;
							if (mine >= maxHits)
								break;
							w += ql - 1;
						}
						if (*p == '\n')
							++p;
						++line;
					}
				}

				// Marche annulable de l'arbre du WORKSPACE (racine `root`), même hygiène que NavWalk.
				void WsWalk(const NkPath &dir, NkVector<NkPath> &out, int32 &budget, int32 depth) {
					if (wsCancel || budget <= 0 || depth > 24)
						return;
					NkVector<NkDirectoryEntry> es =
						NkDirectory::GetEntries(dir, "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
					for (usize k = 0; k < es.Size(); ++k) {
						if (wsCancel || budget <= 0)
							return;
						--budget;
						if (es[k].IsDirectory) {
							const NkString nm = es[k].FullPath.GetFileName();
							if (StrEq(nm.CStr(), ".git") || StrEq(nm.CStr(), "Build") || StrEq(nm.CStr(), ".nkcode"))
								continue; // dossiers générés/VCS : inutiles et énormes
							WsWalk(es[k].FullPath, out, budget, depth + 1);
						} else if (WsSearchable(es[k].FullPath.GetExtension().CStr()))
							out.PushBack(es[k].FullPath);
					}
				}

				void WsScan() { // THREAD : buffers ouverts d'abord (non sauvegardés inclus), puis l'arbre
					const usize kMaxReads = 3000, kPerFile = 50;
					for (usize i = 0; i < wsOpenP.Size() && !wsCancel; ++i)
						WsCollect(wsOpenP[i], wsOpenT[i].CStr(), kPerFile);
					NkVector<NkPath> order;
					int32 budget = 60000;
					WsWalk(root, order, budget, 0);
					wsTotal = static_cast<int32>(order.Size());
					auto opened = [&](const NkString &f2) {
						for (usize i = 0; i < wsOpenP.Size(); ++i)
							if (StrEq(wsOpenP[i].CStr(), f2.CStr()))
								return true;
						return false;
					};
					for (usize i = 0; i < order.Size() && wsResults.Size() < 800 && !wsCancel && i < kMaxReads; ++i) {
						wsScanned = static_cast<int32>(i) + 1;
						const NkString f2 = order[i].ToString();
						if (opened(f2))
							continue; // déjà couvert par le snapshot buffer (version la plus fraîche)
						const NkString txt = NkFile::ReadAllText(order[i]);
						WsCollect(f2, txt.CStr(), kPerFile);
					}
					if (!wsCancel)
						wsScanned = wsTotal;
					{ // fichiers distincts
						int32 n = 0;
						for (usize i = 0; i < wsResults.Size(); ++i) {
							bool seen = false;
							for (usize j = 0; j < i && !seen; ++j)
								seen = StrEq(wsResults[j].file.CStr(), wsResults[i].file.CStr());
							if (!seen)
								++n;
						}
						wsFileCount = n;
					}
					wsDone = true;
				}

				NkString wsRenameTo; // F2 : le scan fini enchaîne sur WsReplaceAll(wsQuery -> wsRenameTo)
				bool wsRenamePending = false;

				void PollWsFind() { // UI : rejoint le thread fini ; relance si une recherche attend
					if (!wsBusy || !wsDone)
						return;
					if (wsThread.Joinable())
						wsThread.Join();
					wsBusy = false;
					if (wsRenamePending && !wsPending) { // F2 : remplacement en chaîne (mêmes critères que le scan)
						wsRenamePending = false;
						WsReplaceAll(wsQuery, wsRenameTo);
						wsRenameTo = NkString();
						return;
					}
					if (wsPending) {
						wsPending = false;
						WsLaunch(wsPendQuery, wsPendCase, wsPendWord, wsPendOpenP, wsPendOpenT);
					}
				}

				void ProcessWsOpen() { // ouverture SÛRE d'un résultat (jamais depuis le rendu)
					if (wsOpenLine < 0 || wsOpenFile.Empty())
						return;
					const NkString f2 = wsOpenFile;
					const int32 l2 = wsOpenLine;
					wsOpenFile = NkString();
					wsOpenLine = -1;
					OpenAt(NkPath(f2), l2);
				}

				// Remplace `q` par `rep` dans `text` (mêmes critères que la recherche). Retourne le compte.
				int32 WsReplaceInText(const NkString &text, const NkString &q, const NkString &rep, NkString &out) {
					const char *tp = text.CStr();
					const char *qs = q.CStr();
					int32 ql = 0;
					while (qs[ql])
						++ql;
					auto isW = [](char c) {
						return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
					};
					int32 count = 0;
					usize i = 0;
					const usize n = text.Size();
					while (i < n) {
						bool m = (i + static_cast<usize>(ql) <= n);
						for (int32 t = 0; t < ql && m; ++t)
							m = wsCase ? (tp[i + t] == qs[t]) : (WsLow(tp[i + t]) == WsLow(qs[t]));
						if (m && wsWord) {
							const char before = (i > 0) ? tp[i - 1] : ' ';
							const char after = tp[i + ql];
							if (isW(before) || isW(after))
								m = false;
						}
						if (m && tp[i] == '\n')
							m = false; // (garde théorique)
						if (m) {
							out += rep.CStr();
							i += static_cast<usize>(ql);
							++count;
						} else {
							out += tp[i];
							++i;
						}
					}
					return count;
				}

				// « Tout remplacer » : buffers OUVERTS en place (undo par fichier) ; fichiers fermés sur DISQUE.
				int32 WsReplaceAll(const NkString &q, const NkString &rep) {
					if (wsBusy || q.Empty() || wsResults.Empty())
						return 0;
					int32 total = 0, nfiles = 0;
					for (usize i = 0; i < wsResults.Size(); ++i) {
						const NkString &f2 = wsResults[i].file;
						bool seen = false;
						for (usize j = 0; j < i && !seen; ++j)
							seen = StrEq(wsResults[j].file.CStr(), f2.CStr());
						if (seen)
							continue;
						int32 fi = -1;
						for (usize k = 0; k < files.Size(); ++k)
							if (StrEq(files[k].path.ToString().CStr(), f2.CStr())) {
								fi = static_cast<int32>(k);
								break;
							}
						if (fi >= 0) { // ouvert : remplace le BUFFER (checkpoint -> Ctrl+Z possible)
							NkCodeDoc &doc2 = files[static_cast<usize>(fi)].doc;
							NkString nu;
							const int32 c2 = WsReplaceInText(doc2.GetText(), q, rep, nu);
							if (c2 > 0) {
								doc2.Checkpoint(3);
								const float32 sx = doc2.scrollX, sy = doc2.scrollY;
								doc2.SetText(nu.CStr());
								doc2.scrollX = sx;
								doc2.scrollY = sy;
								doc2.ClampCursor();
								doc2.dirty = (doc2.SymSig() != doc2.savedSig);
								total += c2;
								++nfiles;
							}
						} else { // fermé : remplace sur DISQUE
							const NkString txt = NkFile::ReadAllText(NkPath(f2));
							if (txt.Empty())
								continue;
							NkString nu;
							const int32 c2 = WsReplaceInText(txt, q, rep, nu);
							if (c2 > 0 && NkFile::WriteAllText(NkPath(f2), nu)) {
								total += c2;
								++nfiles;
							}
						}
					}
					char lb[128];
					std::snprintf(lb, sizeof(lb), "%d remplacement(s) dans %d fichier(s)", total, nfiles);
					status = NkString(lb);
					wsResults.Clear(); // la liste est périmée après remplacement
					wsFileCount = 0;
					return total;
				}

				// ── Session : persiste les onglets + le contenu NON SAUVEGARDÉ (hot-exit VSCode /
				//    reprise après crash). `.nkcode/session.nk` (liste) + `.nkcode/bak<k>.txt` (buffers). ──
				bool mSessionLoaded = false;
				float32 mSessionTimer = 0.f;
				int64 mSessionSig = 0;

				static NkString IntToStr(int32 v) {
					char b[16];
					std::snprintf(b, sizeof(b), "%d", v);
					return NkString(b);
				}

				int64 SessionSig() {
					int64 h = static_cast<int64>(1469598103934665603ULL);
					auto mix = [&](int64 v) { h = (h ^ static_cast<uint64>(v)) * 1099511628211LL; };
					mix(active);
					mix(static_cast<int64>(files.Size()));
					for (usize i = 0; i < files.Size(); ++i) {
						OpenFile &f = files[i];
						for (const char *p = f.path.ToString().CStr(); *p; ++p)
							h = (h ^ (unsigned char)*p) * 1099511628211LL;
						mix(f.doc.curLine);
						mix(f.doc.curCol);
						mix(f.pinned ? 1 : 0);
						mix(static_cast<int32>(f.codeZoom));
						mix(f.doc.dirty ? f.doc.SymSig() : 7);
					}
					return h;
				}

				void SaveSession() {
					if (!HasWorkspace())
						return;
					NkPath dir = root / ".nkcode";
					NkDirectory::CreateRecursive(dir);
					NkString s = NkString("nksession/2\nactive ") + IntToStr(active).CStr() + "\n";
					for (usize i = 0; i < files.Size(); ++i) {
						OpenFile &f = files[i];
						const int32 dy = f.doc.dirty ? 1 : 0;
						s += "F ";
						s += IntToStr(f.doc.curLine).CStr();
						s += " ";
						s += IntToStr(f.doc.curCol).CStr();
						s += " ";
						s += IntToStr(f.pinned ? 1 : 0).CStr();
						s += " ";
						s += IntToStr(dy).CStr();
						s += " ";
						s += IntToStr(static_cast<int32>(f.codeZoom + 0.5f)).CStr();
						s += " "; // v2 : zoom par onglet (0 = global)
						s += f.path.ToString();
						s += "\n";
						if (dy)
							NkFile::WriteAllText(
								dir / (NkString("bak") + IntToStr(static_cast<int32>(i)).CStr() + ".txt").CStr(),
								f.doc.GetText());
					}
					NkFile::WriteAllText(dir / "session.nk", s);
				}

				void LoadSession() {
					NkPath sf = root / ".nkcode" / "session.nk";
					if (!NkFile::Exists(sf))
						return;
					const NkString raw = NkFile::ReadAllText(sf);

					struct Ent {
							NkString path;
							int32 cl, cc, pin, dy, zoom;
					};

					NkVector<Ent> ents;
					int32 savedActive = 0;
					int32 ver = 1;
					const char *p = raw.CStr();
					static char line[65536];
					auto nextField = [](const char *&q) {
						while (*q == ' ')
							++q;
						bool neg = false;
						if (*q == '-') {
							neg = true;
							++q;
						}
						int32 v = 0;
						while (*q >= '0' && *q <= '9') {
							v = v * 10 + (*q - '0');
							++q;
						}
						return neg ? -v : v;
					};
					while (*p) {
						int32 n = 0;
						while (*p && *p != '\n' && *p != '\r' && n < 65535)
							line[n++] = *p++;
						line[n] = 0;
						while (*p == '\n' || *p == '\r')
							++p;
						if (line[0] == 'n' && line[1] == 'k' && line[2] == 's') {
							const char *q = line;
							while (*q && *q != '/')
								++q;
							if (*q == '/') {
								++q;
								ver = nextField(q);
							}
						} // "nksession/N"
						else if (line[0] == 'a' && line[1] == 'c') {
							const char *q = line + 6;
							savedActive = nextField(q);
						} else if (line[0] == 'F' && line[1] == ' ') {
							const char *q = line + 2;
							const int32 cl = nextField(q), cc = nextField(q), pin = nextField(q), dy = nextField(q);
							const int32 zoom = (ver >= 2) ? nextField(q) : 0; // champ zoom present depuis v2
							while (*q == ' ')
								++q;
							ents.PushBack(Ent{NkString(q), cl, cc, pin, dy, zoom});
						}
					}
					if (ents.Empty())
						return;
					files.Clear();
					active = -1;
					for (usize k = 0; k < ents.Size(); ++k) {
						Ent &e = ents[k];
						// Robustesse : une entree dont le fichier n'existe PAS sur disque et qui n'a
						// pas de contenu non-sauvegarde (bak) est ignoree -> evite les onglets VIDES
						// issus d'une session empoisonnee (chemins corrompus). Auto-nettoyage au prochain
						// SaveSession.
						if (!e.dy && !NkFile::Exists(NkPath(e.path)))
							continue;
						OpenPath(NkPath(e.path));
						if (active < 0 || active >= static_cast<int32>(files.Size()))
							continue;
						OpenFile &g = files[active];
						if (e.dy) {
							NkPath bak = root / ".nkcode" /
										 (NkString("bak") + IntToStr(static_cast<int32>(k)).CStr() + ".txt").CStr();
							if (NkFile::Exists(bak)) {
								g.doc.SetText(NkFile::ReadAllText(bak).CStr());
								g.doc.dirty = true;
							}
						}
						g.doc.curLine = e.cl;
						g.doc.curCol = e.cc;
						g.doc.selLine = e.cl;
						g.doc.selCol = e.cc;
						g.doc.ClampCursor();
						g.doc.wantReveal = true;
						g.pinned = (e.pin != 0);
						g.codeZoom = static_cast<float32>(e.zoom); // zoom par onglet restaure (0 = global)
					}
					active = (savedActive >= 0 && savedActive < static_cast<int32>(files.Size()))
								 ? savedActive
								 : (files.Empty() ? -1 : 0);
				}

				// ── Surveillance des fichiers OUVERTS : suppression / modification EXTERNE (hors NKCode). ──
				float32 mFileWatchTimer = 0.f;

				void TickFileWatch(float32 dt) {
					mFileWatchTimer += dt;
					if (mFileWatchTimer < 1.5f)
						return;
					mFileWatchTimer = 0.f;
					for (usize i = 0; i < files.Size(); ++i) {
						OpenFile &f = files[i];
						if (f.untitled)
							continue;
						if (!NkFile::Exists(f.path)) { // supprimé sur le disque
							if (!f.deletedOnDisk) {
								f.deletedOnDisk = true;
								f.doc.dirty = true;
							} // buffer devient « non sauvegardé » -> ré-enregistrable
							continue;
						}
						if (f.deletedOnDisk) {
							f.deletedOnDisk = false;
							f.diskMtime = MTimeOf(f.path.ToString().CStr());
						} // recréé
						const int64 mt = MTimeOf(f.path.ToString().CStr());
						if (f.diskMtime != 0 && mt > f.diskMtime) {
							f.diskMtime = mt;
							if (!f.doc.dirty) {
								f.doc.SetText(NkFile::ReadAllText(f.path).CStr());
								f.doc.dirty = false;
								f.doc.savedSig = f.doc.SymSig();
								f.changedOnDisk = false;
							} // pas de modif locale -> recharge
							else
								f.changedOnDisk = true; // conflit (modif locale + disque) -> bannière
						}
					}
				}

				void TickSession(float32 dt) {
					if (!HasWorkspace())
						return;
					if (!mSessionLoaded) {
						mSessionLoaded = true;
						LoadSession();
						mSessionSig = SessionSig();
						return;
					}
					mSessionTimer += dt;
					if (mSessionTimer < 2.5f)
						return;
					mSessionTimer = 0.f;
					const int64 sig = SessionSig();
					if (sig != mSessionSig) {
						mSessionSig = sig;
						SaveSession();
					}
				}

				// ── Débounce : diagnostics ~0.6 s après la dernière frappe (pas besoin de save) ──
				int64 diagLastSig = 0;
				float32 diagTimer = 0.f;
				int32 diagLastFile = -1;
				bool diagArmed = false;

				void TickDiagnostics(float32 dt) {
					if (active < 0 || active >= static_cast<int32>(files.Size()) || !cdb.ready)
						return;
					OpenFile &f = files[active];
					if (!IsCppExt(f.path.GetExtension().CStr()))
						return;
					const int64 sig = f.doc.SymSig();
					if (sig != diagLastSig || active != diagLastFile) {
						diagLastSig = sig;
						diagLastFile = active;
						diagTimer = 0.f;
						diagArmed = true;
						return;
					}
					if (!diagArmed)
						return;
					diagTimer += dt;
					if (diagTimer >= 0.6f && !diagProc.Running()) {
						RunDiagnostics(active);
						diagArmed = false;
					}
				}

				// Parse une ligne d'erreur clang/gcc (`chemin:L:C: error|warning: msg`) ou MSVC
				// (`chemin(L,C): error Cxxxx: msg`). N'ajoute que les diags du fichier `self`.
				static void ParseDiagLine(const char *p, const char *self, NkCodeDoc &doc) {
					// ── clang/gcc : localise le délimiteur `:<line>:<col>: ` (gère le `:` de lecteur
					//    Windows ET le préfixe « fatal error: » des includes manquants). ──
					{
						auto isD = [](char c) { return c >= '0' && c <= '9'; };
						for (int32 i = 0; p[i]; ++i) {
							if (p[i] != ':' || !isD(p[i + 1]))
								continue;
							int32 j = i + 1, line = 0;
							while (isD(p[j])) {
								line = line * 10 + (p[j] - '0');
								++j;
							}
							if (p[j] != ':' || !isD(p[j + 1]))
								continue;
							int32 k = j + 1, col = 0;
							while (isD(p[k])) {
								col = col * 10 + (p[k] - '0');
								++k;
							}
							if (p[k] != ':' || p[k + 1] != ' ')
								continue;
							const char *sevp =
								p + k + 2; // après « : » -> "error:" / "fatal error:" / "warning:" / "note:"
							uint8 sev;
							if (NkFindSub(sevp, "error:") == sevp || NkFindSub(sevp, "fatal error:") == sevp)
								sev = 1;
							else if (NkFindSub(sevp, "warning:") == sevp)
								sev = 0;
							else
								return; // note:/remark: -> ignore
							if (!NkPathSuffixMatch(p, i, self))
								return; // pas notre fichier (ex header inclus)
							const char *msg = sevp;
							while (*msg && *msg != ':')
								++msg;
							if (*msg == ':') {
								++msg;
								while (*msg == ' ')
									++msg;
							}
							if (line > 0)
								doc.diags.PushBack({line - 1, col > 0 ? col - 1 : 0, col > 0 ? col - 1 : 0, sev,
													NkString(*msg ? msg : sevp)});
							return;
						}
					}
					// ── MSVC ──  chemin(L,C): error Cxxxx: msg   /  chemin(L): error ...
					const char *pe = NkFindSub(p, "): error ");
					uint8 s2 = 1;
					if (!pe) {
						pe = NkFindSub(p, "): warning ");
						s2 = 0;
					}
					if (pe) {
						int32 op = -1;
						for (int32 k = static_cast<int32>(pe - p); k >= 0; --k)
							if (p[k] == '(') {
								op = k;
								break;
							}
						if (op < 0)
							return;
						const int32 line = NkAtoi(p + op + 1);
						int32 col = 0;
						const char *cc = p + op + 1;
						while (*cc && *cc != ',' && *cc != ')')
							++cc;
						if (*cc == ',')
							col = NkAtoi(cc + 1);
						if (!NkPathSuffixMatch(p, op, self))
							return;
						const char *msg = pe + 2;
						while (*msg && *msg != ':')
							++msg;
						if (*msg == ':')
							++msg;
						while (*msg == ' ')
							++msg;
						if (line > 0)
							doc.diags.PushBack(
								{line - 1, col > 0 ? col - 1 : 0, col > 0 ? col - 1 : 0, s2, NkString(msg)});
					}
				}

				// (NkPathSuffixMatch -> NkText.h)

				// ── MRU des onglets (Ctrl+Tab) + pile des fermés (Ctrl+Maj+T) + réordonner (drag) ──
				NkVector<NkString> mruPaths;	// ordre d'usage, le plus récent en DERNIER
				NkVector<NkString> closedPaths; // pile des onglets fermés (rouvrables)
				NkString mruLastPath;			// dernier actif vu par TickMru

				void TouchMru(const NkString &p) {
					if (p.Empty())
						return;
					for (usize i = 0; i < mruPaths.Size(); ++i)
						if (StrEq(mruPaths[i].CStr(), p.CStr())) {
							mruPaths.Erase(mruPaths.Begin() + i);
							break;
						}
					mruPaths.PushBack(p);
					if (mruPaths.Size() > 64)
						mruPaths.Erase(mruPaths.Begin());
				}

				// Appelé chaque frame : enregistre l'onglet actif dans le MRU. Couvre TOUS les chemins
				// d'activation (clic d'onglet, picker, goto, restauration de session) sans les patcher un à un.
				void TickMru() {
					if (!HasActive())
						return;
					const NkString p = files[active].path.ToString();
					if (StrEq(p.CStr(), mruLastPath.CStr()))
						return;
					mruLastPath = p;
					TouchMru(p);
				}

				// Onglets ouverts triés du PLUS RÉCENT au plus ancien (jamais touchés -> à la fin).
				NkVector<NkString> MruOrder() const {
					NkVector<NkString> out;
					for (int32 i = static_cast<int32>(mruPaths.Size()) - 1; i >= 0; --i)
						for (usize j = 0; j < files.Size(); ++j)
							if (StrEq(files[j].path.ToString().CStr(), mruPaths[i].CStr())) {
								out.PushBack(mruPaths[i]);
								break;
							}
					for (usize j = 0; j < files.Size(); ++j) {
						const NkString p = files[j].path.ToString();
						bool seen = false;
						for (usize k = 0; k < out.Size(); ++k)
							if (StrEq(out[k].CStr(), p.CStr())) {
								seen = true;
								break;
							}
						if (!seen)
							out.PushBack(p);
					}
					return out;
				}

				void PushClosed(const NkString &p) {
					if (p.Empty())
						return;
					for (usize i = 0; i < closedPaths.Size(); ++i)
						if (StrEq(closedPaths[i].CStr(), p.CStr())) {
							closedPaths.Erase(closedPaths.Begin() + i);
							break;
						}
					closedPaths.PushBack(p);
					if (closedPaths.Size() > 10)
						closedPaths.Erase(closedPaths.Begin());
				}

				void ReopenClosed() { // Ctrl+Maj+T : rouvre le dernier onglet fermé encore présent sur disque
					while (!closedPaths.Empty()) {
						const NkString p = closedPaths.Back();
						closedPaths.PopBack();
						if (NkFile::Exists(NkPath(p))) {
							OpenPath(NkPath(p));
							return;
						}
					}
					status = NkString("Aucun onglet à rouvrir");
				}

				// Déplace l'onglet `from` en position `to` (drag de la barre d'onglets), actif préservé.
				void MoveTab(int32 from, int32 to) {
					const int32 n = static_cast<int32>(files.Size());
					if (from < 0 || to < 0 || from >= n || to >= n || from == to)
						return;
					const NkString act = HasActive() ? files[active].path.ToString() : NkString();
					while (from < to) {
						OpenFile tmp = files[from];
						files[from] = files[from + 1];
						files[from + 1] = tmp;
						++from;
					}
					while (from > to) {
						OpenFile tmp = files[from];
						files[from] = files[from - 1];
						files[from - 1] = tmp;
						--from;
					}
					if (!act.Empty())
						SyncActiveTo(act);
				}

				// Ferme l'onglet `i` et reajuste l'onglet actif.
				void CloseFile(int32 i) {
					if (i < 0 || i >= static_cast<int32>(files.Size()))
						return;
					PushClosed(files[i].path.ToString());
					files.Erase(files.Begin() + i);
					if (active >= static_cast<int32>(files.Size()))
						active = static_cast<int32>(files.Size()) - 1;
					if (active < 0 && !files.Empty())
						active = 0;
				}

				// ── Actions d'onglets (menu contextuel de la barre d'onglets) ──
				void TogglePin(int32 i) {
					if (i >= 0 && i < static_cast<int32>(files.Size()))
						files[i].pinned = !files[i].pinned;
				}

				// Ferme tous les onglets SAUF `keep` (et sauf les epingles). `keep` reste actif.
				void CloseOthers(int32 keep) {
					if (keep < 0 || keep >= static_cast<int32>(files.Size()))
						return;
					const NkString keepPath = files[keep].path.ToString();
					for (int32 i = static_cast<int32>(files.Size()) - 1; i >= 0; --i) {
						if (files[i].pinned)
							continue;
						if (StrEq(files[i].path.ToString().CStr(), keepPath.CStr()))
							continue;
						PushClosed(files[i].path.ToString());
						files.Erase(files.Begin() + i);
					}
					SyncActiveTo(keepPath);
				}

				// Ferme tous les onglets a DROITE de `i` (sauf epingles).
				void CloseToRight(int32 i) {
					if (i < 0 || i >= static_cast<int32>(files.Size()))
						return;
					const NkString keepPath = files[i].path.ToString();
					for (int32 j = static_cast<int32>(files.Size()) - 1; j > i; --j)
						if (!files[j].pinned) {
							PushClosed(files[j].path.ToString());
							files.Erase(files.Begin() + j);
						}
					SyncActiveTo(keepPath);
				}

				void SyncActiveTo(const NkString &path) {
					for (int32 i = 0; i < static_cast<int32>(files.Size()); ++i)
						if (StrEq(files[i].path.ToString().CStr(), path.CStr())) {
							active = i;
							return;
						}
					if (active >= static_cast<int32>(files.Size()))
						active = static_cast<int32>(files.Size()) - 1;
					if (active < 0 && !files.Empty())
						active = 0;
				}

				bool SaveActive() {
					if (active < 0 || active >= static_cast<int32>(files.Size()))
						return false;
					OpenFile &f = files[active];
					if (NkFile::WriteAllText(f.path, f.doc.GetText())) {
						f.doc.dirty = false;
						f.doc.savedSig = f.doc.SymSig();
						f.untitled = false;
						f.deletedOnDisk = false;
						f.changedOnDisk = false;
						f.diskMtime = MTimeOf(f.path.ToString().CStr());
						status = NkString("Enregistre : ") + f.Name().CStr();
						RefreshGit(f);			// met à jour la bande Git après écriture disque
						RunDiagnostics(active); // vérif syntaxe (squiggles) sur le fichier sauvegardé
						// Sauvegarde d'un .jenga (workspace OU projet inclus) -> recharge la
						// liste des projets (jenga info relit le workspace + ses includes).
						if (EndsWithI(f.Name().CStr(), ".jenga"))
							RequestReload();
						return true;
					}
					status = NkString("Echec enregistrement");
					return false;
				}

				bool HasActive() const {
					return active >= 0 && active < static_cast<int32>(files.Size());
				}

				bool ActiveHasPath() const {
					return HasActive() && !files[active].untitled && !files[active].path.ToString().Empty();
				}

				// Recharge l'onglet actif depuis le disque (abandonne les modifs locales).
				void ReloadActive() {
					if (!HasActive())
						return;
					OpenFile &f = files[active];
					f.doc.SetText(NkFile::ReadAllText(f.path).CStr());
					f.doc.dirty = false;
					f.doc.savedSig = f.doc.SymSig();
					f.changedOnDisk = false;
					f.deletedOnDisk = false;
					f.diskMtime = MTimeOf(f.path.ToString().CStr());
					f.doc.ClampCursor();
					RefreshGit(f);
				}

				// Nouveau fichier sans titre (onglet vide). Reste « sans titre » jusqu'a
				// un Enregistrer sous (path vide -> SaveActive renvoie false).
				void NewFile() {
					OpenFile f; // path vide
					f.doc.SetText("");
					f.untitled = true;
					files.PushBack(f);
					active = static_cast<int32>(files.Size()) - 1;
				}

				// Enregistre l'onglet actif vers `p` (Enregistrer sous).
				bool SaveActiveAs(const NkPath &p) {
					if (!HasActive())
						return false;
					files[active].path = p;
					return SaveActive();
				}

				// Enregistre tous les onglets modifies ayant un chemin. Renvoie le nombre ecrit.
				int32 SaveAll() {
					const int32 keep = active;
					int32 n = 0;
					for (int32 i = 0; i < static_cast<int32>(files.Size()); ++i) {
						if (files[i].path.ToString().Empty() || !files[i].doc.dirty)
							continue;
						active = i;
						if (SaveActive())
							++n;
					}
					active = keep;
					char sb[48];
					std::snprintf(sb, sizeof(sb), "Tout enregistre (%d fichier(s))", n);
					status = NkString(sb);
					return n;
				}

				NkProcess mBuild; // build ASYNCHRONE (ne gele pas l'UI)
				NkProcess mCfg;	  // commandes `jenga config ...` (toolchains)
				bool mCfgPending = false;
				NkString cfgStatus;

				// Lance une commande `jenga config ...` (ex. toolchain add/remove) puis,
				// a la fin, force la re-detection des toolchains (RequestReload).
				void RunConfig(const NkString &args) {
					if (mCfg.Running())
						return;
					cfgStatus = NkString("jenga ") + args.CStr();
					mCfg.Start(cfgStatus);
					mCfgPending = true;
				}

				void PollConfig() {
					if (!mCfgPending)
						return;
					NkVector<NkString> sink;
					mCfg.Drain(sink);
					if (!mCfg.Running()) {
						mCfgPending = false;
						cfgStatus = (mCfg.ExitCode() == 0) ? NkString("Toolchain : OK") : NkString("Toolchain : echec");
						RequestReload(); // re-detecte les toolchains (jenga info)
					}
				}

				// Ecrit un fichier JSON de toolchain (format `jenga config toolchain add`)
				// dans le home, puis lance l'ajout. Retourne false si nom vide.
				bool ToolchainAdd(const char *name, const NkString &json) {
					if (!name || !*name)
						return false;
					const char *home = std::getenv("USERPROFILE");
					if (!home || !*home)
						home = std::getenv("HOME");
					NkString jp =
						(home && *home) ? (NkString(home) + "/.nkcode_tc_tmp.json") : NkString("nkcode_tc_tmp.json");
					if (!NkFile::WriteAllText(NkPath(jp.CStr()), json))
						return false;
					RunConfig(NkString("config toolchain add ") + name + " \"" + jp.CStr() + "\"");
					return true;
				}

				void ToolchainRemove(const char *name) {
					if (name && *name)
						RunConfig(NkString("config toolchain remove ") + name);
				}

				// Lance `jenga <args>` en arriere-plan ; la sortie arrive via PollBuild().
				void StartJenga(const char *args) {
					output.Clear();
					NkString cmd("jenga ");
					cmd += args;
					output.PushBack(NkString("$ ") + cmd.CStr());
					if (!mBuild.Start(cmd)) {
						status = NkString("Build deja en cours...");
						return;
					}
					status = NkString("Construction...");
				}

				// A appeler CHAQUE FRAME : recupere la sortie + enchaine la file + statut.
				void PollBuild() {
					mBuild.Drain(output);
					if (!mBuild.Running()) {
						if (!mQueue.Empty()) {
							PumpQueue();
							return;
						} // commande suivante (rafale)
						if (status.Size() > 0 && StrEq(status.CStr(), "Construction..."))
							status = (mBuild.ExitCode() == 0) ? NkString("Termine (OK)") : NkString("Termine (echec)");
					}
				}

				// ── Projets du workspace (un .jenga en contient plusieurs) ───────────────
				NkVector<NkString> projects; // projets (hors tests) selectionnables
				int32 projIdx = 0;			 // projet cible courant
				NkVector<NkString> tests;	 // projets de test (Kind = TestSuite)
				int32 testIdx = -1;			 // -1 = tous les tests visibles ; >=0 = un test precis

				// Toolchains DETECTEES par Jenga (table "Available Toolchains" de `jenga info`).
				struct ToolchainRow {
						NkString name, family, os, arch, env;
				};

				NkVector<ToolchainRow> toolchains;
				// Compilateur FORCE pour la plateforme courante ("" = auto/meilleur match).
				// Envoye a `jenga build --toolchain <name>`. Reinitialise si l'entree ne
				// correspond plus a la plateforme selectionnee.
				NkString compilerName;

				// Indices (dans `toolchains`) des compilateurs disponibles pour la plateforme
				// courante (os == Systems()[sysIdx].name). Pilote le combo compilateur.
				NkVector<int32> CompilersForCurrentPlatform() const {
					NkVector<int32> out;
					int32 nSys = 0;
					const SysDef *sys = Systems(&nSys);
					const char *osname = sys[(sysIdx >= 0 && sysIdx < nSys) ? sysIdx : 0].name;
					for (int32 i = 0; i < static_cast<int32>(toolchains.Size()); ++i)
						if (StrEqI(toolchains[i].os.CStr(), osname))
							out.PushBack(i);
					return out;
				}

				// Si le compilateur force ne fait plus partie de la plateforme courante -> auto.
				void ValidateCompilerForPlatform() {
					if (compilerName.Empty())
						return;
					const NkVector<int32> cs = CompilersForCurrentPlatform();
					for (usize i = 0; i < cs.Size(); ++i)
						if (StrEqI(toolchains[cs[i]].name.CStr(), compilerName.CStr()))
							return;
					compilerName = NkString(); // n'appartient plus a cette plateforme
				}

				// Infos d'en-tete de `jenga info` (pour les cartes workspace).
				NkString infoConfigs; // "Debug, Release"
				NkString infoOSes;	  // "Windows, Linux, macOS, ..."

				const char *SelectedProject() const {
					return (projIdx >= 0 && projIdx < static_cast<int32>(projects.Size())) ? projects[projIdx].CStr()
																						   : "";
				}

				// Charge la liste des projets du WORKSPACE selectionne via `jenga info`
				// (ASYNCHRONE). Recharge automatiquement quand on change de workspace.
				void LoadProjects() {
					if (mInfoStarted && mInfoWsIdx == wsIdx)
						return;
					mInfoStarted = true;
					mInfoParsed = false;
					mInfoWsIdx = wsIdx;
					mInfoLines.Clear();
					projects.Clear();
					mInfo.Start(NkString("jenga info") + JengaFileArg().CStr());
				}

				// A appeler CHAQUE FRAME : draine `jenga info` puis parse la table des projets.
				void PollProjects() {
					if (!mInfoStarted || mInfoParsed)
						return;
					mInfo.Drain(mInfoLines);
					if (mInfo.Done()) {
						mInfoParsed = true;
						ParseProjects();
					}
				}

				// ── Aides pour l'écran de chargement (section 14) ──
				bool InfoStarted() const {
					return mInfoStarted;
				}

				bool InfoParsed() const {
					return mInfoParsed;
				}

				bool InfoHasError() const {
					for (usize i = 0; i < mInfoLines.Size(); ++i) {
						const NkString c = CleanLine(mInfoLines[i].CStr());
						if (c.Contains("Error:") || c.Contains("Traceback") || c.Contains("Exception"))
							return true;
					}
					return false;
				}

				// Renvoie la ligne d'erreur la plus parlante, préfixée du n° de ligne si trouvé.
				NkString InfoErrorLine() const {
					NkString num, err;
					for (usize i = 0; i < mInfoLines.Size(); ++i) {
						const NkString c = CleanLine(mInfoLines[i].CStr());
						if (c.Contains("Error:") || c.Contains("Exception:"))
							err = c;
						const char *p = c.CStr();
						for (const char *q = p; *q; ++q)
							if (q[0] == 'l' && q[1] == 'i' && q[2] == 'n' && q[3] == 'e' && q[4] == ' ') {
								const char *d = q + 5;
								NkString n;
								while (*d >= '0' && *d <= '9')
									n += *d++;
								if (!n.Empty())
									num = n;
							}
					}
					NkString out;
					if (!num.Empty()) {
						out += "Ligne ";
						out += num;
						out += " : ";
					}
					out += err.Empty() ? NkString("erreur inconnue (voir logs)") : err;
					return out;
				}

				// ── Exemples Jenga : enumeres dynamiquement via `jenga examples list` ──
				struct Example {
						NkString id, desc, platforms, difficulty;
				};

				NkVector<Example> examples;
				NkProcess mExamples;
				bool mExStarted = false, mExParsed = false;
				NkVector<NkString> mExLines;

				void LoadExamples() {
					if (mExStarted)
						return;
					mExStarted = true;
					mExParsed = false;
					mExLines.Clear();
					mExamples.Start(NkString("jenga examples list"));
				}

				void PollExamples() {
					if (!mExStarted || mExParsed)
						return;
					mExamples.Drain(mExLines);
					if (mExamples.Done()) {
						mExParsed = true;
						ParseExamples();
					}
				}

				// Retire les codes ANSI (ESC[...m) + trim debut/fin d'une ligne.
				static NkString CleanLine(const char *s) {
					char out[512];
					usize n = 0;
					for (const char *p = s; *p && n + 1 < sizeof(out); ++p) {
						if (*p == 0x1b) {
							while (*p && *p != 'm')
								++p;
							if (!*p)
								break;
							continue;
						}
						out[n++] = *p;
					}
					out[n] = '\0';
					char *b = out;
					while (*b == ' ' || *b == '\t')
						++b;
					usize m = 0;
					while (b[m])
						++m;
					while (m > 0 && (b[m - 1] == ' ' || b[m - 1] == '\r' || b[m - 1] == '\t'))
						--m;
					b[m] = '\0';
					return NkString(b);
				}

				void ParseExamples() {
					examples.Clear();
					for (usize i = 0; i < mExLines.Size(); ++i) {
						const NkString cl = CleanLine(mExLines[i].CStr());
						const char *L = cl.CStr();
						if (StartsWithI(L, "ID:")) {
							Example e;
							e.id = AfterColon(L);
							examples.PushBack(e);
						} else if (!examples.Empty()) {
							Example &e = examples[examples.Size() - 1];
							if (StartsWithI(L, "Description:"))
								e.desc = AfterColon(L);
							else if (StartsWithI(L, "Platforms:"))
								e.platforms = AfterColon(L);
							else if (StartsWithI(L, "Difficulty:"))
								e.difficulty = AfterColon(L);
						}
					}
				}

				// Construit le projet selectionne (config/plateforme courantes).
				void BuildSelected(const char *platformArg) {
					NkString a("build --target ");
					a += SelectedProject();
					a += " --config ";
					a += ConfigName();
					if (platformArg && platformArg[0]) {
						a += " --platform ";
						a += platformArg;
					}
					StartJenga(a.CStr());
				}

				// Lance (run) le projet selectionne ; --build force la (re)compilation avant.
				void RunSelected(const char *platformArg, const char *deviceArg) {
					NkString a("run ");
					a += SelectedProject();
					a += " --config ";
					a += ConfigName();
					if (platformArg && platformArg[0]) {
						a += " --platform ";
						a += platformArg;
					}
					if (deviceArg && deviceArg[0]) {
						a += " --device ";
						a += deviceArg;
					}
					a += " --build";
					StartJenga(a.CStr());
				}

				const char *ConfigName() const {
					return cfgIdx == 1 ? "Release" : "Debug";
				}

				// ====================================================================
				// ── Barre d'outils build complete : workspaces -> projets -> system ->
				//    config -> architecture -> [Construire/Recompiler/Nettoyer/Demarrer]
				// ====================================================================

				// Systeme cible + ses architectures (encodees dans --platform <OS>-<arch>).
				struct SysDef {
						const char *name;
						const char *archs[6];
						int32 nArch;
				};

				static const SysDef *Systems(int32 *n) {
					static const SysDef s[] = {
						{"Windows", {"x86_64", "x86", "arm64"}, 3},
						{"Linux", {"x86_64", "arm64"}, 2},
						{"macOS", {"x86_64", "arm64"}, 2},
						{"Android", {"arm64", "arm", "x86", "x86_64"}, 4},
						{"iOS", {"arm64"}, 1},
						{"Web", {"wasm32"}, 1},
						{"HarmonyOS", {"arm64"}, 1},
						{"XboxSeries", {"x86_64"}, 1},
					};
					if (n)
						*n = 8;
					return s;
				}

				int32 sysIdx = 0;  // index dans Systems()
				int32 archIdx = 0; // 0..nArch-1 = arch precise ; == nArch -> "Toutes"

				// ── Workspaces : fichiers .jenga a la racine contenant "with workspace" ──
				NkVector<NkString> wsPaths, wsNames;
				int32 wsIdx = 0;
				bool mWsScanned = false;

				void ScanWorkspaces() {
					if (mWsScanned)
						return;
					mWsScanned = true;
					wsPaths.Clear();
					wsNames.Clear();
					NkVector<NkDirectoryEntry> entries =
						NkDirectory::GetEntries(root, "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
					for (usize i = 0; i < entries.Size(); ++i) {
						const NkDirectoryEntry &e = entries[i];
						if (e.IsDirectory)
							continue;
						const NkString nm = e.Name;
						if (!EndsWithI(nm.CStr(), ".jenga"))
							continue;
						const NkString txt = NkFile::ReadAllText(e.FullPath);
						if (!Contains(txt.CStr(), "with workspace") && !Contains(txt.CStr(), "workspace("))
							continue;
						wsPaths.PushBack(e.FullPath.ToString());
						wsNames.PushBack(WorkspaceName(txt, nm));
					}
					if (wsIdx < 0 || wsIdx >= static_cast<int32>(wsPaths.Size()))
						wsIdx = 0;
				}

				bool HasWorkspace() const {
					return !wsPaths.Empty();
				}

				// Scanne un dossier ARBITRAIRE pour ses workspaces (sans toucher a la racine).
				// Sert au panneau « Charger » du launcher (apercu avant chargement).
				static void ScanWorkspacesIn(const NkPath &folder, NkVector<NkString> &outPaths,
											 NkVector<NkString> &outNames) {
					outPaths.Clear();
					outNames.Clear();
					NkVector<NkDirectoryEntry> entries =
						NkDirectory::GetEntries(folder, "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
					for (usize i = 0; i < entries.Size(); ++i) {
						const NkDirectoryEntry &e = entries[i];
						if (e.IsDirectory)
							continue;
						const NkString nm = e.Name;
						if (!EndsWithI(nm.CStr(), ".jenga"))
							continue;
						const NkString txt = NkFile::ReadAllText(e.FullPath);
						if (!Contains(txt.CStr(), "with workspace") && !Contains(txt.CStr(), "workspace("))
							continue;
						outPaths.PushBack(e.FullPath.ToString());
						outNames.PushBack(WorkspaceName(txt, nm));
					}
				}

				// Fichier de config d'interface PAR PROJET : <racine>/.nkcode/ui.cfg
				// (etat maximise + panneaux ouverts, lu/ecrit par le shell).
				NkString UiConfigPath() const {
					return (root / ".nkcode" / "ui.cfg").ToString();
				}

				// Charge `folder` comme racine de travail : re-scan des workspaces du dossier.
				// REFUSE (renvoie false, racine inchangee) si aucun workspace (.jenga contenant
				// "with workspace") n'y est trouve — qu'il ait ete cree par l'UI ou non.
				bool LoadFolder(const NkPath &folder) {
					const NkPath saved = root;
					root = folder;
					wsIdx = 0;
					mWsScanned = false;
					ScanWorkspaces();
					if (!HasWorkspace()) { // aucun workspace -> refus
						root = saved;
						mWsScanned = false;
						ScanWorkspaces();
						return false;
					}
					mLastJengaMtime = 0; // re-amorce le watch sur la nouvelle racine
					files.Clear();
					active = -1;			   // onglets repartent a zero
					RequestReload();		   // recharge la liste des projets
					AddRecent(wsPaths[wsIdx]); // memorise dans les recents
					// Restaure la SESSION de ce workspace (onglets + contenu non sauvegardé). On N'OUVRE PAS
					// le .jenga d'office : il ne réapparaît que s'il était un onglet de la session précédente.
					LoadSession();
					mSessionLoaded = true;
					mSessionTimer = 0.f;
					mSessionSig = SessionSig();
					status = NkString("Workspace charge : ") + folder.ToString().CStr();
					return true;
				}

				// ── Recents + epingles : workspaces ouverts avec l'IDE (~/.nkcode_recent.cfg) ──
				// Fichier : 1 ligne/entree, prefixe "P " = epingle, "R " (ou rien) = recent.
				NkVector<NkString> recents;					 // non epingles (ordre = recence) — chemins .jenga
				NkVector<NkString> pinned;					 // epingles (restent en tete)
				NkVector<NkString> recentNames, pinnedNames; // noms `with workspace(...)` (cache)

				// Lit un .jenga et renvoie le nom du workspace (`with workspace("NAME")`),
				// ou le nom de fichier sans extension en repli.
				static NkString WorkspaceNameOf(const char *jengaPath) {
					const NkString txt = NkFile::ReadAllText(NkPath(jengaPath));
					return WorkspaceName(txt, NkPath(jengaPath).GetFileNameWithoutExtension());
				}

				void RebuildRecentNames() {
					recentNames.Clear();
					pinnedNames.Clear();
					for (usize i = 0; i < recents.Size(); ++i) {
						const char *o = NameOverride(recents[i].CStr());
						recentNames.PushBack(o ? NkString(o) : WorkspaceNameOf(recents[i].CStr()));
					}
					for (usize i = 0; i < pinned.Size(); ++i) {
						const char *o = NameOverride(pinned[i].CStr());
						pinnedNames.PushBack(o ? NkString(o) : WorkspaceNameOf(pinned[i].CStr()));
					}
				}

				// ── Noms personnalises (menu "Renommer dans les recents") -> ~/.nkcode_recent_names.cfg ──
				NkVector<NkString> nameOvrPath, nameOvrName;

				static NkString NamesPath() {
					const char *home = std::getenv("USERPROFILE");
					if (!home || !*home)
						home = std::getenv("HOME");
					if (home && *home)
						return NkString(home) + "/.nkcode_recent_names.cfg";
					return NkString("nkcode_recent_names.cfg");
				}

				const char *NameOverride(const char *path) const {
					for (usize i = 0; i < nameOvrPath.Size(); ++i)
						if (StrEq(nameOvrPath[i].CStr(), path))
							return nameOvrName[i].CStr();
					return nullptr;
				}

				void LoadNameOverrides() {
					nameOvrPath.Clear();
					nameOvrName.Clear();
					NkString txt = NkFile::ReadAllText(NkPath(NamesPath().CStr())), line;
					auto flush = [&]() {
						if (line.Empty())
							return;
						const char *s = line.CStr();
						const char *bar = nullptr;
						for (const char *p = s; *p; ++p)
							if (*p == '|') {
								bar = p;
								break;
							}
						if (bar) {
							char pbuf[512];
							usize n = (usize)(bar - s);
							if (n >= sizeof(pbuf))
								n = sizeof(pbuf) - 1;
							for (usize k = 0; k < n; ++k)
								pbuf[k] = s[k];
							pbuf[n] = '\0';
							nameOvrPath.PushBack(NkString(pbuf));
							nameOvrName.PushBack(NkString(bar + 1));
						}
						line.Clear();
					};
					for (const char *p = txt.CStr(); *p; ++p) {
						if (*p == '\n' || *p == '\r')
							flush();
						else
							line += *p;
					}
					flush();
				}

				void SaveNameOverrides() {
					NkString out;
					for (usize i = 0; i < nameOvrPath.Size(); ++i) {
						out += nameOvrPath[i];
						out += "|";
						out += nameOvrName[i];
						out += "\n";
					}
					NkFile::WriteAllText(NkPath(NamesPath().CStr()), out);
				}

				void SetRecentName(const NkString &path, const NkString &name) {
					for (usize i = 0; i < nameOvrPath.Size(); ++i)
						if (StrEq(nameOvrPath[i].CStr(), path.CStr())) {
							if (name.Empty()) {
								nameOvrPath.Erase(nameOvrPath.Begin() + i);
								nameOvrName.Erase(nameOvrName.Begin() + i);
							} else
								nameOvrName[i] = name;
							SaveNameOverrides();
							RebuildRecentNames();
							return;
						}
					if (!name.Empty()) {
						nameOvrPath.PushBack(path);
						nameOvrName.PushBack(name);
						SaveNameOverrides();
						RebuildRecentNames();
					}
				}

				static NkString RecentsPath() {
					const char *home = std::getenv("USERPROFILE");
					if (!home || !*home)
						home = std::getenv("HOME");
					if (home && *home)
						return NkString(home) + "/.nkcode_recent.cfg";
					return NkString("nkcode_recent.cfg");
				}

				static void RemoveFrom(NkVector<NkString> &v, const char *path) {
					for (usize i = 0; i < v.Size();)
						if (StrEq(v[i].CStr(), path))
							v.Erase(v.Begin() + i);
						else
							++i;
				}

				bool IsPinned(const char *path) const {
					for (usize i = 0; i < pinned.Size(); ++i)
						if (StrEq(pinned[i].CStr(), path))
							return true;
					return false;
				}

				void LoadRecents() {
					recents.Clear();
					pinned.Clear();
					NkString txt = NkFile::ReadAllText(NkPath(RecentsPath().CStr()));
					NkString cur;
					auto flush = [&]() {
						if (cur.Empty())
							return;
						if (cur.CStr()[0] == 'P' && cur.CStr()[1] == ' ')
							pinned.PushBack(NkString(cur.CStr() + 2));
						else if (cur.CStr()[0] == 'R' && cur.CStr()[1] == ' ')
							recents.PushBack(NkString(cur.CStr() + 2));
						else
							recents.PushBack(cur); // ancien format (chemin nu)
						cur.Clear();
					};
					for (const char *p = txt.CStr(); *p; ++p) {
						if (*p == '\n' || *p == '\r')
							flush();
						else
							cur += *p;
					}
					flush();
					LoadNameOverrides();
					RebuildRecentNames();
				}

				void SaveRecents() {
					NkString out;
					for (usize i = 0; i < pinned.Size(); ++i) {
						out += "P ";
						out += pinned[i];
						out += "\n";
					}
					for (usize i = 0; i < recents.Size(); ++i) {
						out += "R ";
						out += recents[i];
						out += "\n";
					}
					NkFile::WriteAllText(NkPath(RecentsPath().CStr()), out);
				}

				void AddRecent(const NkString &wsPath) {
					if (IsPinned(wsPath.CStr()))
						return; // deja epingle -> reste en tete
					NkVector<NkString> nw;
					nw.PushBack(wsPath); // en tete (le plus recent)
					for (usize i = 0; i < recents.Size() && nw.Size() < 12; ++i)
						if (!StrEq(recents[i].CStr(), wsPath.CStr()))
							nw.PushBack(recents[i]);
					recents = nw;
					SaveRecents();
					RebuildRecentNames();
				}

				void PinRecent(const NkString &path) {
					RemoveFrom(recents, path.CStr());
					if (!IsPinned(path.CStr()))
						pinned.PushBack(path);
					SaveRecents();
					RebuildRecentNames();
				}

				void UnpinRecent(const NkString &path) {
					RemoveFrom(pinned, path.CStr());
					RemoveFrom(recents, path.CStr());
					recents.Insert(recents.Begin(), path);
					SaveRecents();
					RebuildRecentNames();
				}

				void RemoveRecent(const NkString &path) {
					RemoveFrom(recents, path.CStr());
					RemoveFrom(pinned, path.CStr());
					SaveRecents();
					RebuildRecentNames();
				}

				// ── Dates : groupes AUJOURD'HUI / CETTE SEMAINE + libelle "Modifie il y a X" ──
				static int64 NowEpoch() {
					return static_cast<int64>(std::time(nullptr));
				}

				// Date de modif d'un .jenga via NkDirectory (evite NKFileSystem.h -> collision
				// macro winbase 'GetFreeSpace'). NkDirectoryEntry.ModificationTime peut etre un
				// FILETIME brut (100ns depuis 1601) sur Windows -> on normalise en epoch Unix (s).
				static int64 MTimeOf(const char *path) {
					const NkPath p(path);
					const NkString fname = p.GetFileName();
					NkVector<NkDirectoryEntry> es =
						NkDirectory::GetEntries(p.GetParent(), fname.CStr(), NkSearchOption::NK_TOP_DIRECTORY_ONLY);
					int64 t = 0;
					for (usize i = 0; i < es.Size(); ++i)
						if (StrEq(es[i].Name.CStr(), fname.CStr())) {
							t = static_cast<int64>(es[i].ModificationTime);
							break;
						}
					if (t > 100000000000000LL)
						t = (t - 116444736000000000LL) / 10000000LL;
					return t;
				}

				// 0 = aujourd'hui (<24h), 1 = cette semaine (<7j), 2 = plus ancien.
				static int32 AgeBucket(int64 mtime, int64 now) {
					if (mtime <= 0)
						return 3;
					const int64 d = now - mtime;
					if (d < 86400)
						return 0; // aujourd'hui
					if (d < 7 * 86400)
						return 1; // cette semaine
					if (d < 30 * 86400)
						return 2; // ce mois
					return 3;	  // plus anciens
				}

				static const char *BucketLabel(int32 b) {
					return b == 0 ? "AUJOURD'HUI" : b == 1 ? "CETTE SEMAINE" : b == 2 ? "CE MOIS" : "PLUS ANCIEN";
				}

				// ── "Derniere activite reelle" : le fichier le PLUS recemment modifie du dossier
				//    workspace (hors Build/.git/Externals/...). Borne par un budget de fichiers. ──
				static bool IsSkippedDir(const char *nm) {
					static const char *skip[] = {"Build",  "build", "Externals", "External",	"node_modules", "cache",
												 "dist",   "tmps",	"tmp",		 "__pycache__", "bin",			"obj",
												 "target", ".git",	".nkcode",	 ".vs",			".idea"};
					for (const char *s : skip)
						if (StrEqI(nm, s))
							return true;
					return false;
				}

				static void ScanActivity(const NkPath &dir, int64 &maxT, int32 &budget) {
					if (budget <= 0)
						return;
					NkVector<NkDirectoryEntry> es =
						NkDirectory::GetEntries(dir, "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
					for (usize i = 0; i < es.Size() && budget > 0; ++i) {
						const NkDirectoryEntry &e = es[i];
						const char *nm = e.Name.CStr();
						if (e.IsDirectory) {
							if (nm[0] == '.' || IsSkippedDir(nm))
								continue;
							ScanActivity(e.FullPath, maxT, budget);
						} else {
							--budget;
							int64 t = static_cast<int64>(e.ModificationTime);
							if (t > 100000000000000LL)
								t = (t - 116444736000000000LL) / 10000000LL; // FILETIME->epoch (repli)
							if (t > maxT)
								maxT = t;
						}
					}
				}

				static int64 ActivityTime(const char *folder) {
					int64 maxT = 0;
					int32 budget = 2500;
					if (folder && *folder)
						ScanActivity(NkPath(folder), maxT, budget);
					return maxT;
				}

				static NkString HumanAge(int64 mtime, int64 now) {
					if (mtime <= 0)
						return NkString("");
					int64 d = now - mtime;
					if (d < 0)
						d = 0;
					char b[64];
					if (d < 60)
						std::snprintf(b, sizeof(b), "%s", NkT("age.now"));
					else if (d < 3600)
						std::snprintf(b, sizeof(b), NkT("age.min"), (int)(d / 60));
					else if (d < 86400)
						std::snprintf(b, sizeof(b), NkT("age.h"), (int)(d / 3600));
					else if (d < 7 * 86400)
						std::snprintf(b, sizeof(b), NkT("age.j"), (int)(d / 86400));
					else
						std::snprintf(b, sizeof(b), NkT("age.sem"), (int)(d / (7 * 86400)));
					return NkString(b);
				}

				// ── Metadonnees d'un workspace NON ouvert (carte du launcher) ─────────────
				// Parse leger du .jenga : configs, plateformes, langage, projets. Mis en
				// cache par chemin (le Home redessine chaque frame -> pas de relecture disque).
				struct WsMeta {
						NkString path, configs, platforms, projects, langVer, toolchains, jengaVer;
						int64 activity = 0;
						int32 projCount = 0;
				};

				NkVector<WsMeta> mWsMeta;

				static char UpC(char c) {
					return (c >= 'a' && c <= 'z') ? char(c - 'a' + 'A') : c;
				}

				static bool StrEqI(const char *a, const char *b) {
					if (!a || !b)
						return a == b;
					while (*a && *b) {
						if (UpC(*a) != UpC(*b))
							return false;
						++a;
						++b;
					}
					return *a == *b;
				}

				// Sous-chaine insensible a la casse (filtres de recherche).
				static bool ContainsI(const char *hay, const char *needle) {
					if (!needle || !*needle)
						return true;
					if (!hay)
						return false;
					for (const char *h = hay; *h; ++h) {
						const char *a = h;
						const char *b = needle;
						while (*a && *b && UpC(*a) == UpC(*b)) {
							++a;
							++b;
						}
						if (!*b)
							return true;
					}
					return false;
				}

				static const char *FindStr(const char *h, const char *n) {
					if (!h || !n || !*n)
						return nullptr;
					for (; *h; ++h) {
						const char *a = h;
						const char *b = n;
						while (*a && *b && *a == *b) {
							++a;
							++b;
						}
						if (!*b)
							return h;
					}
					return nullptr;
				}

				// Concatene les chaines entre guillemets a l'interieur de `open` jusqu'a ']'.
				static NkString JoinQuotedInCall(const char *txt, const char *open) {
					const char *s = FindStr(txt, open);
					if (!s)
						return NkString();
					s += Len(open);
					NkString out;
					for (const char *p = s; *p && *p != ']'; ++p)
						if (*p == '"') {
							++p;
							NkString tok;
							while (*p && *p != '"')
								tok += *p++;
							if (!tok.Empty()) {
								if (!out.Empty())
									out += ", ";
								out += tok;
							}
						}
					return out;
				}

				static NkString FriendlyOS(const char *t) {
					if (StrEqI(t, "WINDOWS"))
						return NkString("Windows");
					if (StrEqI(t, "LINUX"))
						return NkString("Linux");
					if (StrEqI(t, "MACOS"))
						return NkString("macOS");
					if (StrEqI(t, "ANDROID"))
						return NkString("Android");
					if (StrEqI(t, "IOS"))
						return NkString("iOS");
					if (StrEqI(t, "WEB") || StrEqI(t, "EMSCRIPTEN"))
						return NkString("Web");
					if (StrEqI(t, "HARMONYOS"))
						return NkString("HarmonyOS");
					NkString o;
					bool first = true; // repli : Titlecase
					for (const char *p = t; *p; ++p) {
						char c = *p;
						c = first ? UpC(c) : char((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
						first = false;
						o += c;
					}
					return o;
				}

				// Tokens `prefix.XXX` a l'interieur de `open` jusqu'a ']' (ex. TargetOS.WINDOWS).
				static NkString JoinEnumInCall(const char *txt, const char *open, const char *prefix) {
					const char *s = FindStr(txt, open);
					if (!s)
						return NkString();
					s += Len(open);
					const usize pl = Len(prefix);
					NkString out;
					for (const char *p = s; *p && *p != ']'; ++p) {
						bool m = true;
						for (usize k = 0; k < pl; ++k)
							if (p[k] != prefix[k]) {
								m = false;
								break;
							}
						if (!m)
							continue;
						p += pl;
						NkString tok;
						while (*p && (*p == '_' || (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
									  (*p >= '0' && *p <= '9')))
							tok += *p++;
						--p;
						if (!tok.Empty()) {
							if (!out.Empty())
								out += ", ";
							out += FriendlyOS(tok.CStr());
						}
					}
					return out;
				}

				static NkString DetectLang(const char *txt) {
					const char *d = FindStr(txt, "cppdialect(\"");
					if (d) {
						d += Len("cppdialect(\"");
						NkString t;
						while (*d && *d != '"')
							t += *d++;
						if (!t.Empty())
							return t;
					}
					if (FindStr(txt, "cppcompiler") || FindStr(txt, "cxxflags") || FindStr(txt, "C++"))
						return NkString("C++");
					if (FindStr(txt, "python") || FindStr(txt, "Python"))
						return NkString("Python");
					return NkString("C++");
				}

				// Compte/liste les projets d'un workspace : `project("Nom")` (frontière de mot,
				// couvre `with project(`) + `startproject(`) dans le .jenga racine ET dans tous
				// les fichiers atteints par `include("…")`, RÉCURSIVEMENT (BFS, anti-cycles,
				// plafond 300 fichiers) — même règle pour toutes les cartes du launcher.
				static NkString CollectProjects(const char *txt, const NkPath &baseDir, int32 *outCount = nullptr) {
					NkVector<NkString> names;
					auto isW2 = [](char c) {
						return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
					};
					auto collect = [&](const char *src, const char *pat, bool boundary) {
						const char *base = src;
						const char *p = src;
						while ((p = FindStr(p, pat))) {
							const char *hit = p;
							p += Len(pat);
							if (boundary && hit > base && isW2(hit[-1]))
								continue; // « startproject( » ne doit pas matcher « project( »
							while (*p && *p != '"')
								++p;
							if (*p == '"') {
								++p;
								NkString tok;
								while (*p && *p != '"')
									tok += *p++;
								bool dup = false;
								for (usize i = 0; i < names.Size(); ++i)
									if (StrEq(names[i].CStr(), tok.CStr())) {
										dup = true;
										break;
									}
								if (!dup && !tok.Empty())
									names.PushBack(tok);
							}
						}
					};
					// BFS sur les fichiers : (texte, dossier de base) — la racine puis chaque include.
					NkVector<NkString> qTxt, qDir, seen;
					qTxt.PushBack(NkString(txt));
					qDir.PushBack(baseDir.ToString());
					for (usize qi = 0; qi < qTxt.Size() && qTxt.Size() <= 300; ++qi) {
						const char *src = qTxt[qi].CStr();
						collect(src, "project(", true); // couvre `with project(`
						collect(src, "startproject(", true);
						const char *p = src;
						while ((p = FindStr(p, "include(\""))) {
							const char *hit = p;
							p += Len("include(\"");
							if (hit > src && isW2(hit[-1]))
								continue; // pas `xinclude(`
							NkString rel;
							while (*p && *p != '"')
								rel += *p++;
							if (rel.Empty())
								continue;
							const NkString full = (NkPath(qDir[qi].CStr()) / rel.CStr()).ToString();
							bool vis = false;
							for (usize i = 0; i < seen.Size(); ++i)
								if (StrEq(seen[i].CStr(), full.CStr())) {
									vis = true;
									break;
								}
							if (vis || qTxt.Size() > 300)
								continue;
							seen.PushBack(full);
							const NkString sub = NkFile::ReadAllText(NkPath(full.CStr()));
							if (!sub.Empty()) {
								qTxt.PushBack(sub);
								qDir.PushBack(NkPath(full.CStr()).GetParent().ToString());
							}
						}
					}
					if (outCount)
						*outCount = (int32)names.Size();
					NkString out; // noms (jusqu'a 6) ; le total "(N)" est affiche a part
					for (usize i = 0; i < names.Size() && i < 6; ++i) {
						if (!out.Empty())
							out += ", ";
						out += names[i];
					}
					return out;
				}

				// Noms entre guillemets du PREMIER argument de chaque appel `pat...("name"...)`
				// (uniques, jusqu'a 8). Ex. pat = "toolchain(".
				static NkString JoinCallArgs(const char *txt, const char *pat) {
					NkVector<NkString> names;
					const char *p = txt;
					while ((p = FindStr(p, pat))) {
						p += Len(pat);
						while (*p && *p != '"' && *p != ')')
							++p;
						if (*p == '"') {
							++p;
							NkString t;
							while (*p && *p != '"')
								t += *p++;
							bool dup = false;
							for (usize i = 0; i < names.Size(); ++i)
								if (StrEq(names[i].CStr(), t.CStr())) {
									dup = true;
									break;
								}
							if (!dup && !t.Empty())
								names.PushBack(t);
						}
					}
					NkString out;
					for (usize i = 0; i < names.Size() && i < 8; ++i) {
						if (!out.Empty())
							out += ", ";
						out += names[i];
					}
					return out;
				}

				// Renvoie (par valeur, duree de vie sure cote appelant) les metadonnees parsees.
				WsMeta WorkspaceMeta(const char *path) {
					for (usize i = 0; i < mWsMeta.Size(); ++i)
						if (StrEq(mWsMeta[i].path.CStr(), path))
							return mWsMeta[i];
					WsMeta m;
					m.path = path;
					const NkString txt = NkFile::ReadAllText(NkPath(path));
					m.configs = JoinQuotedInCall(txt.CStr(), "configurations([");
					m.platforms = JoinEnumInCall(txt.CStr(), "targetoses([", "TargetOS.");
					m.langVer = DetectLang(txt.CStr());
					m.toolchains = JoinCallArgs(txt.CStr(), "toolchain(");
					{
						const char *v = FindStr(txt.CStr(), "jengaversion(\"");
						if (v) {
							v += Len("jengaversion(\"");
							NkString t;
							while (*v && *v != '"')
								t += *v++;
							m.jengaVer = t;
						}
					}
					m.projects = CollectProjects(txt.CStr(), NkPath(path).GetParent(), &m.projCount);
					m.activity = ActivityTime(NkPath(path).GetParent().ToString().CStr()); // derniere activite reelle
					if (m.activity == 0)
						m.activity = MTimeOf(path); // repli : mtime du .jenga
					mWsMeta.PushBack(m);
					return m;
				}

				// Argument --jenga-file pour cibler le workspace selectionne.
				NkString JengaFileArg() const {
					if (wsIdx >= 0 && wsIdx < static_cast<int32>(wsPaths.Size()))
						return NkString(" --jenga-file \"") + wsPaths[wsIdx].CStr() + "\"";
					return NkString();
				}

				// Projet « Tous les projets » = entree virtuelle apres la liste.
				bool AllProjects() const {
					return projIdx >= static_cast<int32>(projects.Size());
				}

				// `s` commence-t-il par le prefixe `pre` (insensible a la casse) ?
				static bool StartsWithI(const char *s, const char *pre) {
					if (!s || !pre || !*pre)
						return false;
					for (; *pre; ++s, ++pre) {
						char a = *s, b = *pre;
						if (a >= 'A' && a <= 'Z')
							a += 32;
						if (b >= 'A' && b <= 'Z')
							b += 32;
						if (a != b)
							return false;
					}
					return true;
				}

				static usize Len(const char *s) {
					usize n = 0;
					if (s)
						while (s[n])
							++n;
					return n;
				}

				// Un test est-il visible pour la selection courante ?
				//  - « Tous les projets » -> tous les tests.
				//  - projet precis -> les tests dont le nom commence par CE projet, et pour
				//    lesquels aucun AUTRE projet n'est un prefixe PLUS LONG (ex. "NKPlatform_Tests"
				//    appartient a "NKPlatform", pas a "NK").
				bool TestVisible(int32 i) const {
					if (i < 0 || i >= static_cast<int32>(tests.Size()))
						return false;
					if (AllProjects())
						return true;
					const char *t = tests[i].CStr();
					const char *sel = SelectedProject();
					if (!StartsWithI(t, sel))
						return false;
					const usize selLen = Len(sel);
					for (usize p = 0; p < projects.Size(); ++p)
						if (Len(projects[p].CStr()) > selLen && StartsWithI(t, projects[p].CStr()))
							return false;
					return true;
				}

				const char *ConfigNameOf(int32 i) const {
					return i == 1 ? "Release" : "Debug";
				}

				// ── File d'attente de commandes jenga (compilation en rafale) ──
				NkVector<NkString> mQueue;

				void EnqueueJenga(const NkString &args) {
					mQueue.PushBack(NkString("jenga ") + args.CStr());
				}

				void PumpQueue() {
					if (mBuild.Running() || mQueue.Empty())
						return;
					NkString next = mQueue[0];
					mQueue.Erase(mQueue.Begin());
					output.PushBack(NkString("$ ") + next.CStr());
					buildTotal = 0;
					buildDone = 0; // progression de cette commande
					mBuild.Start(next);
					status = NkString("Construction...");
				}

				// verb = "build" (Construire) ou "rebuild" (Recompiler de zero).
				void DoBuildAction(const char *verb) {
					if (!HasWorkspace()) {
						status = NkString("(aucun workspace)");
						return;
					}
					output.Clear();
					mQueue.Clear();
					int32 nSys = 0;
					const SysDef *sys = Systems(&nSys);
					const SysDef &S = sys[(sysIdx >= 0 && sysIdx < nSys) ? sysIdx : 0];
					int32 cfgs[2], nc = 0;
					if (cfgIdx >= 2) {
						cfgs[nc++] = 0;
						cfgs[nc++] = 1;
					} else
						cfgs[nc++] = cfgIdx;
					int32 archs[6], na = 0;
					if (archIdx >= S.nArch) {
						for (int32 i = 0; i < S.nArch; ++i)
							archs[na++] = i;
					} else
						archs[na++] = archIdx;
					for (int32 c = 0; c < nc; ++c)
						for (int32 a = 0; a < na; ++a) {
							NkString cmd(verb);
							if (!AllProjects()) {
								cmd += " --target ";
								cmd += SelectedProject();
							}
							cmd += " --config ";
							cmd += ConfigNameOf(cfgs[c]);
							cmd += " --platform ";
							cmd += S.name;
							cmd += "-";
							cmd += S.archs[archs[a]];
							if (!compilerName.Empty()) {
								cmd += " --toolchain ";
								cmd += compilerName;
							}
							cmd += JengaFileArg();
							EnqueueJenga(cmd);
						}
					PumpQueue();
				}

				void DoClean() {
					if (!HasWorkspace()) {
						status = NkString("(aucun workspace)");
						return;
					}
					output.Clear();
					mQueue.Clear();
					NkString cmd("clean");
					if (!AllProjects()) {
						cmd += " --project ";
						cmd += SelectedProject();
					}
					cmd += JengaFileArg();
					EnqueueJenga(cmd);
					PumpQueue();
				}

				void DoRun() {
					if (!HasWorkspace()) {
						status = NkString("(aucun workspace)");
						return;
					}
					if (AllProjects()) {
						status = NkString("(choisir un projet pour Demarrer)");
						return;
					}
					output.Clear();
					mQueue.Clear();
					int32 nSys = 0;
					const SysDef *sys = Systems(&nSys);
					const SysDef &S = sys[(sysIdx >= 0 && sysIdx < nSys) ? sysIdx : 0];
					NkString cmd("run ");
					cmd += SelectedProject();
					cmd += " --config ";
					cmd += ConfigNameOf(cfgIdx >= 2 ? 0 : cfgIdx);
					cmd += " --platform ";
					cmd += S.name;
					if (!compilerName.Empty()) {
						cmd += " --toolchain ";
						cmd += compilerName;
					}
					cmd += " --build";
					cmd += JengaFileArg();
					EnqueueJenga(cmd);
					PumpQueue();
				}

				// Lance les tests : si testIdx == -1 -> TOUS les tests visibles (ceux du
				// projet selectionne, ou tous si « Tous les projets ») ; sinon un test precis.
				// Plusieurs tests -> file d'attente (rafale).
				void DoTest() {
					if (!HasWorkspace() || tests.Empty()) {
						status = NkString("(aucun test)");
						return;
					}
					output.Clear();
					mQueue.Clear();
					int32 ran = 0;
					for (int32 i = 0; i < static_cast<int32>(tests.Size()); ++i) {
						if (!TestVisible(i))
							continue;
						if (testIdx >= 0 && i != testIdx)
							continue; // un seul test demande
						NkString cmd("test ");
						cmd += tests[i].CStr();
						cmd += " --config ";
						cmd += ConfigNameOf(cfgIdx >= 2 ? 0 : cfgIdx);
						cmd += JengaFileArg();
						EnqueueJenga(cmd);
						++ran;
					}
					if (ran == 0) {
						status = NkString("(aucun test pour ce projet)");
						return;
					}
					PumpQueue();
				}

				// Force un re-scan des workspaces + rechargement de `jenga info` (relit le
				// workspace ET tous ses projets inclus). Appele par le bouton Recharger,
				// au changement de workspace, et a l'auto-detection de modifs.
				void RequestReload() {
					mWsScanned = false;	  // re-scan des .jenga racine
					mInfoStarted = false; // force le rechargement de jenga info
					mInfoWsIdx = -1;
					flagsStale = true; // un .jenga a (peut-être) changé -> régénère le .jcdb (includes/defines
									   // par projet)
				}

				// Auto-detection (sur timer) : si un .jenga de la racine a change de date de
				// modification -> recharge. Les modifs faites DANS l'editeur (workspace ou
				// projet inclus) declenchent aussi un reload via SaveActive.
				void TickWatch(float32 dt) {
					mWatchTimer += dt;
					if (mWatchTimer < 1.5f)
						return;
					mWatchTimer = 0.f;
					// Signature = max(date de modif) des .jenga racine. Si elle augmente -> reload.
					int64 mx = 0;
					NkVector<NkDirectoryEntry> entries =
						NkDirectory::GetEntries(root, "*.jenga", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
					for (usize i = 0; i < entries.Size(); ++i) {
						if (entries[i].IsDirectory)
							continue;
						const int64 t = static_cast<int64>(entries[i].ModificationTime);
						if (t > mx)
							mx = t;
					}
					if (mLastJengaMtime == 0) {
						mLastJengaMtime = mx;
						return;
					} // 1re mesure
					if (mx > mLastJengaMtime) {
						mLastJengaMtime = mx;
						RequestReload();
					}
				}

				float32 mWatchTimer = 0.f; // (struct interne -> helpers statiques accessibles a NkOpenWs)
				int64 mLastJengaMtime = 0;

				static bool EndsWithI(const char *s, const char *suf) {
					usize ls = 0, lf = 0;
					for (const char *p = s; *p; ++p)
						++ls;
					for (const char *p = suf; *p; ++p)
						++lf;
					if (lf > ls)
						return false;
					const char *a = s + (ls - lf);
					for (usize i = 0; i < lf; ++i) {
						char x = a[i], y = suf[i];
						if (x >= 'A' && x <= 'Z')
							x += 32;
						if (y >= 'A' && y <= 'Z')
							y += 32;
						if (x != y)
							return false;
					}
					return true;
				}

				// Extrait le nom depuis workspace("NAME" ; sinon nom de fichier sans .jenga.
				static NkString WorkspaceName(const NkString &txt, const NkString &fileName) {
					const char *p = txt.CStr();
					const char *k = "workspace(";
					for (; *p; ++p) {
						const char *a = p;
						const char *b = k;
						while (*a && *b && *a == *b) {
							++a;
							++b;
						}
						if (!*b) {
							p = a;
							break;
						}
					}
					if (*p) {
						while (*p == ' ' || *p == '\t')
							++p;
						if (*p == '"' || *p == '\'') {
							char q = *p++;
							char nm[96];
							usize i = 0;
							while (*p && *p != q && i + 1 < sizeof(nm))
								nm[i++] = *p++;
							nm[i] = '\0';
							if (i > 0)
								return NkString(nm);
						}
					}
					NkString s = fileName;
					const char *d = s.CStr();
					usize n = 0;
					for (const char *z = d; *z; ++z)
						++n;
					if (n > 6) {
						char b[128];
						usize i = 0;
						for (; i < n - 6 && i + 1 < sizeof(b); ++i)
							b[i] = d[i];
						b[i] = '\0';
						return NkString(b);
					}
					return s;
				}

				NkProcess mInfo; // `jenga info` (liste des projets)
				NkVector<NkString> mInfoLines;
				bool mInfoStarted = false;
				bool mInfoParsed = false;
				int32 mInfoWsIdx = -1; // workspace pour lequel les projets sont charges

				// Parse la table "Projects" de `jenga info` (colonnes Name Kind ...).
				// Les projets de TEST (Kind = TestSuite) vont dans `tests` ; les autres
				// dans `projects`. Exclut les separateurs / entrees parasites (ex. --unitest--).
				void ParseProjects() {
					projects.Clear();
					tests.Clear();
					toolchains.Clear();

					enum { NONE, PROJ, TOOL } cur = NONE;

					for (usize i = 0; i < mInfoLines.Size(); ++i) {
						const char *L = mInfoLines[i].CStr();
						if (StartsWithI(L, "Configurations:")) {
							infoConfigs = AfterColon(L);
							continue;
						}
						if (StartsWithI(L, "Target OSes:")) {
							infoOSes = AfterColon(L);
							continue;
						}
						if (Contains(L, "Name") && Contains(L, "Kind")) {
							cur = PROJ;
							continue;
						}
						if (Contains(L, "Name") && Contains(L, "Family")) {
							cur = TOOL;
							continue;
						}
						if (L[0] == '=' || L[0] == '-')
							continue; // separateurs
						if (IsBlank(L)) {
							cur = NONE;
							continue;
						} // fin de table (d'autres suivent)
						if (cur == PROJ) {
							char name[128], kind[64];
							if (!TwoTokens(L, name, sizeof(name), kind, sizeof(kind)))
								continue;
							if (name[0] == '-' || Contains(name, "unitest"))
								continue; // parasite / --unitest--
							if (Contains(kind, "Test"))
								tests.PushBack(NkString(name)); // TestSuite -> combo Tests
							else
								projects.PushBack(NkString(name));
						} else if (cur == TOOL) {
							char t[5][96];
							const int32 n = NTokens(L, t, 5, 96);
							if (n < 4)
								continue;
							ToolchainRow r;
							r.name = t[0];
							r.family = t[1];
							r.os = t[2];
							r.arch = t[3];
							r.env = (n >= 5) ? NkString(t[4]) : NkString();
							toolchains.PushBack(r);
						}
					}
					for (usize i = 0; i < projects.Size(); ++i) // defaut = NKCode si present
						if (StrEq(projects[i].CStr(), "NKCode")) {
							projIdx = static_cast<int32>(i);
							break;
						}
				}

				// Decoupe jusqu'a `maxN` jetons separes par des espaces/tabs. Renvoie le nombre lu.
				static int32 NTokens(const char *s, char t[][96], int32 maxN, int32 cap) {
					int32 n = 0;
					while (*s && n < maxN) {
						while (*s == ' ' || *s == '\t')
							++s;
						if (!*s)
							break;
						int32 i = 0;
						while (*s && *s != ' ' && *s != '\t' && i + 1 < cap)
							t[n][i++] = *s++;
						t[n][i] = '\0';
						++n;
					}
					return n;
				}

				static bool IsBlank(const char *s) {
					for (; *s; ++s)
						if (*s != ' ' && *s != '\t')
							return false;
					return true;
				}

				// Partie apres le premier ':' (trim espaces). Retire d'eventuels codes ANSI ESC[...m.
				static NkString AfterColon(const char *s) {
					const char *p = s;
					while (*p && *p != ':')
						++p;
					if (*p == ':')
						++p;
					while (*p == ' ' || *p == '\t')
						++p;
					char out[256];
					usize n = 0;
					for (; *p && n + 1 < sizeof(out); ++p) {
						if (*p == 0x1b) {
							while (*p && *p != 'm')
								++p;
							if (!*p)
								break;
							continue;
						} // saute ESC[...m
						out[n++] = *p;
					}
					while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\r' || out[n - 1] == '\t'))
						--n;
					out[n] = '\0';
					return NkString(out);
				}

				static bool Contains(const char *h, const char *n) {
					for (; *h; ++h) {
						const char *a = h;
						const char *b = n;
						while (*a && *b && *a == *b) {
							++a;
							++b;
						}
						if (!*b)
							return true;
					}
					return false;
				}

				// Extrait les 2 premiers jetons separes par des espaces.
				static bool TwoTokens(const char *s, char *t0, usize c0, char *t1, usize c1) {
					while (*s == ' ' || *s == '\t')
						++s;
					usize i = 0;
					while (*s && *s != ' ' && *s != '\t' && i + 1 < c0)
						t0[i++] = *s++;
					t0[i] = '\0';
					if (i == 0)
						return false;
					while (*s == ' ' || *s == '\t')
						++s;
					usize j = 0;
					while (*s && *s != ' ' && *s != '\t' && j + 1 < c1)
						t1[j++] = *s++;
					t1[j] = '\0';
					return j > 0;
				}
		};

	} // namespace nkcode
} // namespace nkentseu
