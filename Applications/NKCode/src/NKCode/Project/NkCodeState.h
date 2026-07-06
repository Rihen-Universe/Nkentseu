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
#include "NKCode/Project/NkText.h"   // NkAtoi / NkFindSub / NkJsonStr / NkJsonArray / NkPathSuffixMatch
#include "NKCode/Editor/NkCodeEditor.h"
#include "NKCode/Shell/NkI18n.h"        // NkT() : ages relatifs traduits
#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace nkentseu {
namespace nkcode {

    struct NkIcons;   // forward (defini dans Shell/NkUi.h) — icones de la vue principale IDE

    using namespace nkentseu;

    inline bool StrEq(const char* a, const char* b) {
        if (!a || !b) return a == b;
        while (*a && *a == *b) { ++a; ++b; }
        return *a == *b;
    }

    // Un fichier ouvert dans l'editeur : chemin + document editable (modele par
    // lignes + curseur/selection/scroll). L'etat vit dans le doc -> jamais perdu
    // au changement d'onglet ni au re-dock.
    struct OpenFile {
        NkPath     path;
        NkCodeDoc  doc;                 // modele par lignes + etat d'edition
        bool       pinned = false;      // onglet epingle (non fermable accidentellement, affiche en tete)
        bool       untitled = false;    // fichier « sans-titre » (pas encore sauvegarde -> Ctrl+S = dialogue)
        int64      diskMtime = 0;       // date de modif sur disque au dernier open/save (detection externe)
        bool       deletedOnDisk = false;  // le fichier a ete supprime en dehors de NKCode
        bool       changedOnDisk = false;  // le fichier a ete modifie en dehors de NKCode
        float32    codeZoom = 0.f;          // taille police PROPRE a cet onglet (0 = taille globale). Zoom par-fichier.
        NkString Name() const { return path.GetFileName(); }
    };

    struct NkCodeState {
        NkIcons*           icons = nullptr;   // icones de la vue principale IDE (pose par main.cpp)
        NkPath             root;        // dossier racine de l'explorateur (arbre)
        NkVector<OpenFile> files;       // onglets ouverts
        int32              active = -1; // onglet actif
        bool               reqSaveAs = false;   // demande d'ouvrir le dialogue « Enregistrer sous » (+ nouveau fichier / ré-enregistrer supprimé)
        NkVector<NkString> output;      // sortie de la derniere commande jenga
        NkString           status;      // ligne d'etat (ex. "Build OK")
        int32              buildTotal = 0, buildDone = 0;   // progression : projets total / faits
        bool IsBuilding() const { return mBuild.Running() || !mQueue.Empty(); }
        float32 BuildProgress() const { return buildTotal > 0 ? (float32)buildDone / (float32)buildTotal : 0.f; }

        // Barre d'outils Visual Studio : config / plateforme / appareil cibles.
        int32 cfgIdx  = 0;              // 0 Debug, 1 Release
        int32 platIdx = 0;             // 0 Windows, 1 Linux, 2 Android, 3 Web
        int32 devIdx  = 0;             // appareil/emulateur (mobile)

        NkCodeState() {
            // Racine = workspace Nkentseu (dossier courant au lancement) -> l'explorateur
            // montre tout le repo et la barre d'outils liste tous les projets du .jenga.
            root = NkPath::GetCurrentDirectory();
        }
        ~NkCodeState() { if (navThread.Joinable()) navThread.Join(); }   // go-to-def async : évite un thread orphelin à la fermeture

        // Ouvre `p` dans l'editeur (ou le re-selectionne si deja ouvert).
        void OpenPath(const NkPath& p) {
            const NkString ps = p.ToString();
            for (usize i = 0; i < files.Size(); ++i)
                if (StrEq(files[i].path.ToString().CStr(), ps.CStr())) { active = static_cast<int32>(i); return; }

            const NkString content = NkFile::ReadAllText(p);
            OpenFile f; f.path = p;
            f.doc.SetText(content.CStr());
            f.diskMtime = MTimeOf(p.ToString().CStr());   // référence pour la détection de changement externe
            files.PushBack(f);
            active = static_cast<int32>(files.Size()) - 1;
            RefreshGit(files[active]);   // indicateurs Git de la gouttière
        }

        // ── Encodage (mojibake) : réparation par LOT de tous les fichiers ouverts affectés ──
        int32 CountMojibake() const { int32 c = 0; for (usize i = 0; i < files.Size(); ++i) if (files[i].doc.mojibake) ++c; return c; }
        void RepairAllOpenEncodings() { for (usize i = 0; i < files.Size(); ++i) if (files[i].doc.mojibake) files[i].doc.RepairEncoding(); }

        // Nouvel onglet vierge « sans-titre-N » (bouton + de la barre d'onglets).
        int32 untitledSeq = 0;
        void NewUntitled() {
            char nm[32]; std::snprintf(nm, sizeof(nm), "sans-titre-%d.txt", ++untitledSeq);
            OpenFile f; f.path = NkPath(nm); f.doc.SetText(""); f.untitled = true;
            files.PushBack(f);
            active = static_cast<int32>(files.Size()) - 1;
        }

        // ── Indicateurs Git de la gouttière : parse `git diff` vs HEAD ──
        static NkString RunCapture(const char* cmd) {
            NkString out;
        #ifdef _WIN32
            FILE* p = _popen(cmd, "r");
        #else
            FILE* p = popen(cmd, "r");
        #endif
            if (!p) return out;
            char buf[1024]; usize n;
            while ((n = std::fread(buf, 1, sizeof(buf) - 1, p)) > 0) { buf[n] = '\0'; out += buf; }
        #ifdef _WIN32
            _pclose(p);
        #else
            pclose(p);
        #endif
            return out;
        }
        static const char* SkipNum(const char* p) { while (*p >= '0' && *p <= '9') ++p; return p; }
        static void ParseHunk(const char* s, int32& oldS, int32& oldC, int32& newS, int32& newC) {
            oldS = 0; oldC = 1; newS = 0; newC = 1;   // b/d omis => 1
            const char* p = s + 2; while (*p == ' ') ++p;
            if (*p == '-') { ++p; oldS = NkAtoi(p); p = SkipNum(p); if (*p == ',') { ++p; oldC = NkAtoi(p); p = SkipNum(p); } }
            while (*p == ' ') ++p;
            if (*p == '+') { ++p; newS = NkAtoi(p); p = SkipNum(p); if (*p == ',') { ++p; newC = NkAtoi(p); p = SkipNum(p); } }
        }
        // Recalcule gitStatus/gitDeleted du fichier (vs HEAD). Appelé à l'ouverture + sauvegarde.
        void RefreshGit(OpenFile& f) {
            f.doc.gitStatus.Clear(); f.doc.gitDeleted.Clear();
            const int32 lc = f.doc.LineCount();
            for (int32 i = 0; i < lc; ++i) f.doc.gitStatus.PushBack(0);
            if (f.path.ToString().Empty() || root.ToString().Empty()) return;
            NkString cmd = NkString("git -C \"") + root.ToString().CStr()
                         + "\" --no-pager diff --no-color -U0 -- \"" + f.path.ToString().CStr() + "\"";
        #ifdef _WIN32
            cmd += " 2>NUL";
        #else
            cmd += " 2>/dev/null";
        #endif
            const NkString out = RunCapture(cmd.CStr());
            const char* p = out.CStr();
            while (*p) {
                if (p[0] == '@' && p[1] == '@') {
                    int32 oldS, oldC, newS, newC; ParseHunk(p, oldS, oldC, newS, newC);
                    if (oldC > 0 && newC == 0) { int32 idx = newS - 1; if (idx < 0) idx = 0; f.doc.gitDeleted.PushBack(idx); }
                    else { const uint8 kind = (oldC == 0) ? 1 : 2; for (int32 k = 0; k < newC; ++k) { const int32 idx = newS - 1 + k; if (idx >= 0 && idx < lc) f.doc.gitStatus[idx] = kind; } }
                }
                while (*p && *p != '\n') ++p; if (*p == '\n') ++p;
            }
        }

        // ── Index de symboles NIVEAU PROJET + BIBLIOTHÈQUE (types/fonctions de tout
        //    l'arbre du workspace, headers moteur inclus). Construit UNE fois en tâche
        //    de fond (ne gèle pas l'UI) ; lecture seule ensuite (gate `projReady`). ──
        NkVector<NkString> projTypes, projFuncs;
        volatile bool projReady = false;
        bool projStarted = false;
        threading::NkThread projThread;

        void StartProjectIndex() {
            if (projStarted || root.ToString().Empty()) return;
            projStarted = true;
            projThread = threading::NkThread([this](void*) { BuildProjectIndex(); });
        }
        void BuildProjectIndex() {
            NkVector<NkString> t, f;
            ScanDirSymbols(root, t, f, 0);
            NkSymSortDedup(t); NkSymSortDedup(f);
            projTypes = t; projFuncs = f;   // écrit une fois puis lecture seule
            projReady = true;
        }
        void ScanDirSymbols(const NkPath& dir, NkVector<NkString>& t, NkVector<NkString>& f, int32 depth) {
            if (depth > 16) return;
            NkVector<NkDirectoryEntry> entries = NkDirectory::GetEntries(dir, "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
            for (usize i = 0; i < entries.Size(); ++i) {
                const NkDirectoryEntry& e = entries[i];
                const char* nm = e.Name.CStr();
                if (e.IsDirectory) {
                    if (nm[0] == '.' || StrEq(nm, "Build") || StrEq(nm, "build") || StrEq(nm, "dist") || StrEq(nm, "node_modules")
                        || StrEq(nm, "Captures") || StrEq(nm, "bin") || StrEq(nm, "obj") || StrEq(nm, "out")) continue;   // dossiers lourds/inutiles
                    ScanDirSymbols(e.FullPath, t, f, depth + 1);
                } else {
                    const NkLang lg = NkLangFromExt(e.FullPath.GetExtension().CStr());
                    if (lg == NkLang::None || lg == NkLang::Markdown) continue;
                    const NkString txt = NkFile::ReadAllText(e.FullPath);
                    if (txt.Size() > 500000) continue;                                   // ignore les très gros fichiers
                    NkScanTextSymbols(txt.CStr(), (lg == NkLang::C || lg == NkLang::NKSL), t, f);
                }
            }
        }

        // ── DIAGNOSTICS (erreurs/avertissements du compilateur cible) ─────────────
        // Récupère les flags via `jenga compile-flags` (async), puis lance le compilo
        // en vérif syntaxe (`-fsyntax-only`//Zs) sur le fichier SAUVEGARDÉ et parse la
        // sortie -> squiggles. Fiable (vrai compilateur du toolchain), pas d'heuristique.
        // .jcdb = base de compilation Jenga : UNE entrée par projet (chacun ses includes/defines/std).
        struct ProjFlags { NkString name, dir, std; NkVector<NkString> includes, defines; };
        struct CompileDb  { NkString compiler; bool msvc = false; NkVector<ProjFlags> projects; bool ready = false; };
        CompileDb          cdb;
        NkProcess          flagsProc;
        NkVector<NkString> flagsAcc;
        NkString           flagsSig;              // (plateforme|config) -> re-génère le .jcdb si change
        bool               flagsBusy = false;
        bool               flagsStale = false;    // un .jenga a changé -> forcer la régénération du .jcdb
        NkProcess          diagProc;
        NkVector<NkString> diagAcc;
        int32              diagTarget = -1;        // index du fichier en cours de diag
        // ── Macros EFFECTIVES par projet (dump `<compilateur> -dM -E fichier`) : inclut les
        //    macros DÉRIVÉES des headers (ex _WIN32 -> NKENTSEU_PLATFORM_WINDOWS) que la simple
        //    liste de defines du .jcdb ignore. Sert au grisage préproc EXACT (comme clangd). ──
        struct MacroSet { NkString dir; NkVector<NkString> defs; bool ready = false; };
        NkVector<MacroSet> macroSets;              // un par dossier de projet, résolu à la demande
        NkProcess          macroProc;
        NkVector<NkString> macroAcc;
        NkString           macroDir;               // projet dont le dump est en cours
        bool               macroBusy = false;

        static bool IsCppExt(const char* e) {
            return StrEqI(e, ".cpp") || StrEqI(e, ".cc") || StrEqI(e, ".cxx") || StrEqI(e, ".c")
                || StrEqI(e, ".h") || StrEqI(e, ".hpp") || StrEqI(e, ".hh") || StrEqI(e, ".hxx") || StrEqI(e, ".inl");
        }
        NkString ActiveFilePath() const { return (active >= 0 && active < static_cast<int32>(files.Size())) ? files[active].path.ToString() : NkString(); }
        // Préfixe `set "PATH=<dossier du compilateur>;%PATH%" && ` : indispensable pour que
        // le compilateur lancé DIRECTEMENT (diag/dump macros) trouve ses DLL (ex msys/ucrt64
        // clang++ -> libc++). Sans ça il échoue en silence si son bin n'est pas dans le PATH.
        NkString CompilerPathPrefix() const {
            const char* c = cdb.compiler.CStr(); int32 last = -1;
            for (int32 i = 0; c[i]; ++i) if (c[i] == '\\' || c[i] == '/') last = i;
            if (last <= 0) return NkString();
            NkString dir; for (int32 i = 0; i < last; ++i) dir += c[i];
            return NkString("set \"PATH=") + dir.CStr() + ";%PATH%\" && ";
        }
        NkString DiagSig() const {   // le .jcdb dépend de (plateforme|config), pas du fichier
            int32 nSys = 0; const SysDef* sys = Systems(&nSys);
            const char* plat = sys[(sysIdx >= 0 && sysIdx < nSys) ? sysIdx : 0].name;
            return NkString(plat) + "|" + ConfigNameOf(cfgIdx >= 2 ? 0 : cfgIdx);
        }
        // Entrée du .jcdb dont le DOSSIER est l'ancêtre le plus proche du fichier.
        const ProjFlags* FlagsForFile(const NkString& path) const {
            const ProjFlags* best = nullptr; int32 bestLen = -1;
            for (usize i = 0; i < cdb.projects.Size(); ++i) {
                const ProjFlags& pf = cdb.projects[i];
                if (pf.dir.Empty()) continue;
                if (NkPathIsAncestor(pf.dir.CStr(), path.CStr())) { const int32 n = static_cast<int32>(pf.dir.Size()); if (n > bestLen) { bestLen = n; best = &pf; } }
            }
            return best;
        }
        // Parse le format PLAT .jcdb (lignes `clé\tvaleur`, section `project`).
        void ParseJcdb(const NkString& raw) {
            cdb = CompileDb();
            const char* p = raw.CStr(); ProjFlags* cur = nullptr; char line[4096];
            while (*p) {
                int32 n = 0; while (*p && *p != '\n' && *p != '\r' && n < 4095) line[n++] = *p++; line[n] = 0; while (*p == '\n' || *p == '\r') ++p;
                if (n == 0 || line[0] == '#') continue;
                int32 t = 0; while (t < n && line[t] != '\t') ++t; if (t >= n) continue;
                line[t] = 0; const char* key = line; const char* val = line + t + 1;
                if      (StrEq(key, "compiler")) cdb.compiler = val;
                else if (StrEq(key, "msvc"))     cdb.msvc = (val[0] == '1');
                else if (StrEq(key, "project"))  { cdb.projects.PushBack(ProjFlags{}); cur = &cdb.projects[cdb.projects.Size() - 1]; cur->name = val; }
                else if (cur) {
                    if      (StrEq(key, "dir")) cur->dir = val;
                    else if (StrEq(key, "std")) cur->std = val;
                    else if (StrEq(key, "inc")) cur->includes.PushBack(NkString(val));
                    else if (StrEq(key, "def")) cur->defines.PushBack(NkString(val));
                }
            }
            cdb.ready = !cdb.compiler.Empty() && !cdb.projects.Empty();
        }
        // A appeler chaque frame : (re)génère le .jcdb (une fois par plateforme/config).
        void PollFlags() {
            if (!HasWorkspace()) return;
            const NkString sig = DiagSig();
            if (!flagsBusy && (flagsStale || !StrEq(sig.CStr(), flagsSig.CStr()))) {
                int32 nSys = 0; const SysDef* sys = Systems(&nSys);
                const char* plat = sys[(sysIdx >= 0 && sysIdx < nSys) ? sysIdx : 0].name;
                NkString cmd = NkString("jenga compile-flags --platform ") + plat + " --config " + ConfigNameOf(cfgIdx >= 2 ? 0 : cfgIdx) + JengaFileArg();
                flagsAcc.Clear();
                if (flagsProc.Start(cmd)) { flagsBusy = true; flagsSig = sig; flagsStale = false; }
                return;
            }
            if (flagsBusy) {
                flagsProc.Drain(flagsAcc);
                if (flagsProc.Done()) {
                    flagsBusy = false;
                    ParseJcdb(NkFile::ReadAllText(root / ".jenga" / "compileflags.jcdb"));
                    macroSets.Clear();   // flags changés -> re-dumper les macros effectives par projet
                }
            }
        }
        NkString diagTempPath;   // fichier temporaire compilé (BUFFER courant), supprimé après
        // Vérif syntaxe LIVE : écrit le BUFFER dans un fichier temp SIBLING (même dossier
        // -> `#include "x.h"` relatifs OK), sans exiger de sauvegarde. Le fichier temp a
        // l'extension `.nkcheck` (non compilée par les globs du build) + `-x` force le langage.
        void RunDiagnostics(int32 fileIdx) {
            if (!cdb.ready || fileIdx < 0 || fileIdx >= static_cast<int32>(files.Size())) return;
            if (diagProc.Running()) return;
            OpenFile& f = files[fileIdx];
            const NkString ext = f.path.GetExtension();
            if (!IsCppExt(ext.CStr())) return;
            const ProjFlags* pf = FlagsForFile(f.path.ToString());   // flags du PROJET du fichier
            if (!pf) return;
            const NkString tmp = (f.path.GetParent() / ".nkcode_diag.nkcheck").ToString();
            if (!NkFile::WriteAllText(NkPath(tmp), f.doc.GetText())) return;
            diagTempPath = tmp;
            const bool isC = StrEqI(ext.CStr(), ".c");
            NkString cmd = CompilerPathPrefix() + "\"" + cdb.compiler.CStr() + "\" ";
            if (cdb.msvc) { cmd += isC ? "/TC " : "/TP "; cmd += "/Zs /nologo "; if (!pf->std.Empty()) { cmd += "/std:"; cmd += pf->std.CStr(); cmd += " "; } }
            else {
                cmd += "-fsyntax-only "; cmd += isC ? "-x c " : "-x c++ "; if (!pf->std.Empty()) { cmd += "-std="; cmd += pf->std.CStr(); cmd += " "; }
                // Rapporter TOUTES les erreurs en une passe (pas de plafond) -> on marque tout d'un coup.
                cmd += NkFindSub(cdb.compiler.CStr(), "clang") ? "-ferror-limit=0 -fno-caret-diagnostics " : "-fmax-errors=0 -fno-diagnostics-show-caret ";
            }
            for (usize i = 0; i < pf->includes.Size(); ++i) { cmd += cdb.msvc ? "/I\"" : "-I\""; cmd += pf->includes[i].CStr(); cmd += "\" "; }
            for (usize i = 0; i < pf->defines.Size(); ++i) { cmd += cdb.msvc ? "/D" : "-D"; cmd += pf->defines[i].CStr(); cmd += " "; }
            cmd += "\""; cmd += tmp.CStr(); cmd += "\" 2>&1";
            diagAcc.Clear(); diagTarget = fileIdx;
            diagProc.Start(cmd);
        }
        void PollDiagnostics() {
            if (diagTarget < 0) return;
            diagProc.Drain(diagAcc);
            if (!diagProc.Done()) return;
            const int32 tgt = diagTarget; diagTarget = -1;
            if (tgt < static_cast<int32>(files.Size())) {
                OpenFile& f = files[tgt];
                f.doc.diags.Clear();
                for (usize li = 0; li < diagAcc.Size(); ++li) ParseDiagLine(diagAcc[li].CStr(), diagTempPath.CStr(), f.doc);
            }
            if (!diagTempPath.Empty()) { NkFile::Delete(NkPath(diagTempPath)); diagTempPath = NkString(); }
        }
        // Defines EFFECTIFS pour griser les branches préproc du fichier : macros réellement
        // définies (builtins compilateur + defines projet + DÉRIVÉES des headers). Renvoie
        // nullptr tant que le dump n'est pas prêt (pas de grisage plutôt qu'un grisage faux).
        // Lance le dump `<compilateur> -dM -E <fichier>` à la demande (async, une fois/projet).
        const NkVector<NkString>* EffectiveDefines(const NkString& filePath) {
            const ProjFlags* pf = FlagsForFile(filePath);
            if (!pf || pf->dir.Empty()) return nullptr;
            for (usize i = 0; i < macroSets.Size(); ++i)
                if (StrEq(macroSets[i].dir.CStr(), pf->dir.CStr())) return macroSets[i].ready ? &macroSets[i].defs : nullptr;
            macroSets.PushBack(MacroSet{ pf->dir, {}, false });   // pending
            if (!macroBusy && cdb.ready) {
                const NkString ext = NkPath(filePath).GetExtension();
                const bool isC = StrEqI(ext.CStr(), ".c");
                NkString cmd = CompilerPathPrefix() + "\"" + cdb.compiler.CStr() + "\" ";
                if (cdb.msvc) { cmd += "/EP /nologo "; }   // MSVC : pas de dump macros fiable -> best effort
                else { cmd += "-dM -E "; cmd += isC ? "-x c " : "-x c++ "; if (!pf->std.Empty()) { cmd += "-std="; cmd += pf->std.CStr(); cmd += " "; } }
                for (usize i = 0; i < pf->includes.Size(); ++i) { cmd += cdb.msvc ? "/I\"" : "-I\""; cmd += pf->includes[i].CStr(); cmd += "\" "; }
                for (usize i = 0; i < pf->defines.Size(); ++i) { cmd += cdb.msvc ? "/D" : "-D"; cmd += pf->defines[i].CStr(); cmd += " "; }
                cmd += "\""; cmd += filePath.CStr(); cmd += "\" 2>&1";
                macroAcc.Clear();
                if (macroProc.Start(cmd)) { macroBusy = true; macroDir = pf->dir; }
            }
            return nullptr;
        }
        void PollMacros() {
            if (!macroBusy) return;
            macroProc.Drain(macroAcc);
            if (!macroProc.Done()) return;
            macroBusy = false;
            MacroSet* set = nullptr;
            for (usize i = 0; i < macroSets.Size(); ++i) if (StrEq(macroSets[i].dir.CStr(), macroDir.CStr())) { set = &macroSets[i]; break; }
            if (!set) return;
            set->defs.Clear();
            // Chaque ligne : `#define NAME[(...)] [corps]`. On garde NAME (+ =valeur si entier).
            for (usize li = 0; li < macroAcc.Size(); ++li) {
                const char* p = macroAcc[li].CStr();
                while (*p == ' ' || *p == '\t') ++p;
                if (*p != '#') continue; ++p; while (*p == ' ' || *p == '\t') ++p;
                if (NkFindSub(p, "define") != p) continue;
                p += 6; while (*p == ' ' || *p == '\t') ++p;
                const char* name = p; int32 nl = 0;
                while ((p[nl]>='A'&&p[nl]<='Z')||(p[nl]>='a'&&p[nl]<='z')||(p[nl]>='0'&&p[nl]<='9')||p[nl]=='_') ++nl;
                if (nl == 0) continue;
                NkString entry; for (int32 k = 0; k < nl; ++k) entry += name[k];
                const char* body = name + nl;
                if (*body != '(') {   // object-like : corps entier simple -> NAME=valeur
                    while (*body == ' ' || *body == '\t') ++body;
                    bool allDigit = (*body >= '0' && *body <= '9'); const char* b = body;
                    while (*b >= '0' && *b <= '9') ++b;
                    while (*b == 'L' || *b == 'l' || *b == 'U' || *b == 'u') ++b;
                    while (*b == ' ' || *b == '\t' || *b == '\r' || *b == '\n') ++b;
                    if (allDigit && *b == '\0') { entry += '='; for (const char* c = body; *c>='0'&&*c<='9'; ++c) entry += *c; }
                }
                set->defs.PushBack(entry);
            }
            set->ready = true;
        }
        // ── Ctrl+clic (navigation façon VSCode) : consomme d.linkTarget du fichier actif ──
        static bool IsHdrSrc(const char* e) {
            return StrEqI(e,".h")||StrEqI(e,".hpp")||StrEqI(e,".hh")||StrEqI(e,".hxx")||StrEqI(e,".inl")
                || StrEqI(e,".cpp")||StrEqI(e,".cc")||StrEqI(e,".cxx")||StrEqI(e,".c");
        }
        // Ligne (0-based) de la DÉFINITION de `sym` dans `text`, ou -1. Priorité aux définitions
        // fortes (class/struct/enum/union/namespace/typedef/using/#define) ; repli faible (`sym(`).
        static int32 DefLineOf(const char* text, const char* sym) {
            int32 sl = 0; while (sym[sl]) ++sl; if (sl == 0) return -1;
            auto isW = [](char c){ return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'; };
            int32 line = 0, weak = -1; const char* p = text;
            while (*p) {
                const char* ls = p; while (*p && *p != '\n') ++p;   // [ls,p) = ligne courante
                // Scan BORNÉ À LA LIGNE (jamais jusqu'à la fin du fichier -> évite un O(taille²)
                // qui figeait la recherche sur les gros headers).
                for (const char* q = ls; q + sl <= p; ++q) {
                    bool m = true; for (int32 t = 0; t < sl; ++t) if (q[t] != sym[t]) { m = false; break; }
                    if (!m) continue;
                    const char before = (q > ls) ? q[-1] : ' ', after = q[sl];
                    if (isW(before) || isW(after)) continue;   // frontière de mot
                    const char* kw = q; while (kw > ls && (kw[-1]==' '||kw[-1]=='\t')) --kw;   // mot-clé juste avant
                    auto pre = [&](const char* w){ int32 wl=0; while (w[wl]) ++wl; if (kw-ls < wl) return false; const char* s=kw-wl; for (int32 t=0;t<wl;++t) if (s[t]!=w[t]) return false; return (s==ls || s[-1]==' '||s[-1]=='\t'); };
                    if (pre("class")||pre("struct")||pre("enum")||pre("union")||pre("namespace")||pre("typedef")||pre("using")) return line;
                    { const char* h = ls; while (h < q && (*h==' '||*h=='\t')) ++h; if (*h=='#') { const char* d=h+1; while (*d==' '||*d=='\t')++d; if (d[0]=='d'&&d[1]=='e'&&d[2]=='f'&&d[3]=='i'&&d[4]=='n'&&d[5]=='e') return line; } }
                    // Membre d'ÉNUMÉRATION : symbole = 1er token de la ligne, suivi de ',' / '}' / '=' (sans ';', sinon c'est une affectation).
                    { const char* h2 = ls; while (h2 < q && (*h2==' '||*h2=='\t')) ++h2;
                      if (h2 == q) { const char* a = q + sl; while (*a==' '||*a=='\t') ++a;
                        bool semi = false; for (const char* z = ls; z < p; ++z) if (*z==';') { semi = true; break; }
                        if (*a==',' || *a=='}' || (*a=='=' && !semi)) return line; } }
                    // ── Repli FAIBLE : DÉFINITION de fonction/méthode ou DÉCLARATION de variable —
                    //    JAMAIS un APPEL. Exclut `obj.f()`/`obj->f()` (membre) ; un `f(`/`Ns::f(` n'est
                    //    une définition que s'il est précédé d'un TYPE de retour (sinon = appel). ──
                    if (weak < 0) {
                        auto isKw = [](const char* s, const char* e){ auto eq=[&](const char* w){ int32 wl=0; while(w[wl])++wl; if ((int32)(e-s)!=wl) return false; for(int32 t=0;t<wl;++t) if(s[t]!=w[t]) return false; return true; };
                            return eq("return")||eq("else")||eq("case")||eq("delete")||eq("new")||eq("sizeof")||eq("throw")||eq("goto")||eq("if")||eq("while")||eq("for")||eq("switch")||eq("do")||eq("co_await")||eq("co_return")||eq("co_yield")||eq("const")||eq("static")||eq("inline")||eq("virtual")||eq("explicit")||eq("friend")||eq("constexpr")||eq("noexcept")||eq("template")||eq("using")||eq("typedef"); };
                        const char* pbk = q; while (pbk > ls && (pbk[-1]==' '||pbk[-1]=='\t')) --pbk;
                        const bool member = (pbk > ls && pbk[-1]=='.') || (pbk - 1 > ls && pbk[-1]=='>' && pbk[-2]=='-');
                        const char* a2 = q + sl; while (*a2==' '||*a2=='\t') ++a2;
                        if (!member && *a2 == '(') {   // fonction/méthode : DÉFINITION seulement si TYPE devant (chaîne A::B:: remontée)
                            const char* c = pbk;
                            while (c - 1 > ls && c[-1]==':' && c[-2]==':') { c -= 2; while (c > ls && (c[-1]==' '||c[-1]=='\t')) --c; while (c > ls && isW(c[-1])) --c; while (c > ls && (c[-1]==' '||c[-1]=='\t')) --c; }
                            const char* te = c; while (te > ls && (te[-1]==' '||te[-1]=='\t'||te[-1]=='&'||te[-1]=='*'||te[-1]=='>')) --te;
                            if (te > ls && isW(te[-1])) { const char* ts = te; while (ts > ls && isW(ts[-1])) --ts; if (!isKw(ts, te)) weak = line; }
                        } else if (!member && (*a2==';'||*a2=='='||*a2==','||*a2==')'||*a2=='['||*a2=='{')) {   // déclaration de variable `Type nom`
                            const char* b = q; while (b > ls && (b[-1]==' '||b[-1]=='\t'||b[-1]=='&'||b[-1]=='*')) --b;
                            if (b > ls && isW(b[-1])) { const char* ts = b; while (ts > ls && isW(ts[-1])) --ts; if (!isKw(ts, b)) weak = line; }
                        }
                    }
                }
                if (*p == '\n') ++p; ++line;
            }
            return weak;
        }
        void OpenAt(const NkPath& p, int32 line) {
            OpenPath(p);
            if (active >= 0 && line >= 0) {
                OpenFile& g = files[active];
                g.doc.curLine = line; g.doc.curCol = 0;
                g.doc.selLine = line; g.doc.selCol = 0;   // PAS de sélection (juste le curseur)
                g.doc.ClampCursor(); g.doc.ResetEditRun();
                g.doc.wantReveal = true;                  // le scroll se positionne sur la ligne
            }
        }
        void ProcessNavigation() {
            if (active < 0 || active >= static_cast<int32>(files.Size())) return;
            OpenFile& f = files[active];
            if (f.doc.linkTarget.Empty()) return;
            // Picker ouvert / choix en attente -> l'utilisateur interagit avec la LISTE
            // (souvent Ctrl encore enfoncé) : on ignore le lien Ctrl de l'éditeur dessous.
            if (navPickerOpen || navPickChoice >= 0) { f.doc.linkTarget = NkString(); return; }
            const NkString tgt = f.doc.linkTarget; const bool inc = f.doc.linkIsInclude;
            f.doc.linkTarget = NkString();   // consomme (une seule action)
            const ProjFlags* pf = FlagsForFile(f.path.ToString());
            if (inc) {   // ── #include "x.h" / <x.h> : dossier courant puis include dirs du projet ──
                NkPath c0 = f.path.GetParent() / NkPath(tgt);
                if (NkFile::Exists(c0)) { OpenAt(c0, -1); return; }
                if (pf) for (usize i = 0; i < pf->includes.Size(); ++i) { NkPath c = NkPath(pf->includes[i]) / NkPath(tgt); if (NkFile::Exists(c)) { OpenAt(c, -1); return; } }
                status = NkString("Include introuvable : ") + tgt.CStr();
                return;
            }
            // ── Symbole : 1) définition dans le FICHIER ACTIF -> saut IMMÉDIAT (variable locale,
            //    type/fonction locale) : pas de thread, pas de liste. ──
            { const int32 lnA = DefLineOf(f.doc.GetText().CStr(), tgt.CStr());
              if (lnA >= 0) { f.doc.curLine = lnA; f.doc.curCol = 0; f.doc.selLine = lnA; f.doc.selCol = 0; f.doc.ClampCursor(); f.doc.ResetEditRun(); f.doc.wantReveal = true; status = NkString(); return; } }
            // 2) sinon go-to-definition PROJET sur un THREAD (progression + liste des occurrences).
            if (!pf) { status = NkString("Définition introuvable : ") + tgt.CStr(); return; }
            StartGotoDef(tgt, pf);
        }
        // ── Go-to-definition ASYNCHRONE (thread) : collecte TOUTES les définitions ────────
        struct NavHit { NkString file; int32 line; NkString preview; };
        NkThread          navThread;
        bool              navBusy = false;   // écrit par l'UI, lu simple (comme NkProcess.mRunning)
        bool              navDone = false;   // écrit par le thread à la fin
        NkString          navSym; NkVector<NkString> navDirs;         // entrées : symbole + dossiers d'include
        NkVector<NkString> navOpenPaths, navOpenTexts;                // snapshots des fichiers ouverts (buffers)
        NkVector<NavHit>  navResults;                                 // sortie : 1 hit par fichier (déf. trouvée)
        int32             navScanned = 0, navTotal = 0;               // progression (thread -> UI)
        bool              navPickerOpen = false; int32 navPickerSel = 0;   // liste de choix si >1
        bool              navCancel = false;                       // annulation COOPÉRATIVE (le thread lit ce drapeau)
        bool              navPending = false;                      // une nouvelle recherche attend l'arrêt de l'actuelle
        NkString          navPendSym; NkVector<NkString> navPendDirs, navPendOpenP, navPendOpenT;
        void StartGotoDef(const NkString& sym, const ProjFlags* pf) {
            // Snapshots des entrées (dossiers d'include + buffers ouverts).
            NkVector<NkString> dirs; for (usize i = 0; i < pf->includes.Size(); ++i) dirs.PushBack(pf->includes[i]);
            NkVector<NkString> openP, openT;
            for (usize i = 0; i < files.Size(); ++i) { openP.PushBack(files[i].path.ToString()); openT.PushBack(files[i].doc.GetText()); }
            if (navBusy) {   // recherche en cours -> l'ANNULER et mémoriser la nouvelle (relancée dès l'arrêt)
                navCancel = true; navPending = true;
                navPendSym = sym; navPendDirs = dirs; navPendOpenP = openP; navPendOpenT = openT;
                status = NkString("Nouvelle recherche de « ") + sym.CStr() + " »…";
                return;
            }
            NavLaunch(sym, dirs, openP, openT);
        }
        void NavLaunch(const NkString& sym, const NkVector<NkString>& dirs, const NkVector<NkString>& openP, const NkVector<NkString>& openT) {
            if (navThread.Joinable()) navThread.Join();
            navSym = sym; navDirs = dirs; navOpenPaths = openP; navOpenTexts = openT;
            navResults.Clear(); navScanned = 0; navTotal = 0; navPickerOpen = false; navPickerSel = 0;
            navCancel = false; navDone = false; navBusy = true;
            status = NkString("Recherche de « ") + sym.CStr() + " »…";
            navThread = NkThread([this](void*) { NavScan(); });
        }
        static NkString LineTextOf(const char* text, int32 target) {   // ligne `target` (0-based), rognée
            int32 ln = 0; const char* p = text;
            while (*p && ln < target) { if (*p == '\n') ++ln; ++p; }
            while (*p == ' ' || *p == '\t') ++p;
            NkString o; for (const char* q = p; *q && *q != '\n' && *q != '\r' && (int32)o.Size() < 120; ++q) o += *q;
            return o;
        }
        // Marche récursive ANNULABLE : liste chaque dossier en TOP-ONLY (rapide) puis récurse,
        // en vérifiant `navCancel` très souvent -> l'annulation est quasi immédiate (contrairement
        // à un GetEntries récursif unique, non interruptible qui « bloquait à un niveau »).
        void NavWalk(const NkPath& dir, NkVector<NkPath>& named, NkVector<NkPath>& others, int32& budget, int32 depth) {
            if (navCancel || budget <= 0 || depth > 24) return;   // limite de profondeur = anti-cycle
            NkVector<NkDirectoryEntry> es = NkDirectory::GetEntries(dir, "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
            for (usize k = 0; k < es.Size(); ++k) {
                if (navCancel || budget <= 0) return;
                --budget;   // CHAQUE entrée (fichier OU dossier) consomme du budget -> les jonctions/cycles s'arrêtent
                if (es[k].IsDirectory) NavWalk(es[k].FullPath, named, others, budget, depth + 1);
                else if (IsHdrSrc(es[k].FullPath.GetExtension().CStr())) {
                    if (StrEq(es[k].FullPath.GetFileNameWithoutExtension().CStr(), navSym.CStr())) named.PushBack(es[k].FullPath);
                    else others.PushBack(es[k].FullPath);
                }
            }
        }
        void NavScan() {   // THREAD : lit navSym/navDirs/navOpen* + FS ; écrit navResults/navScanned/navTotal
            const usize kMaxReads = 700, kMaxHits = 100;
            auto already = [&](const NkString& f){ for (usize i = 0; i < navResults.Size(); ++i) if (StrEq(navResults[i].file.CStr(), f.CStr())) return true; return false; };
            // 1) fichiers ouverts (buffers snapshottés — inclut les non sauvegardés)
            for (usize i = 0; i < navOpenPaths.Size() && !navCancel; ++i) {
                const int32 ln = DefLineOf(navOpenTexts[i].CStr(), navSym.CStr());
                if (ln >= 0 && !already(navOpenPaths[i])) navResults.PushBack(NavHit{ navOpenPaths[i], ln, LineTextOf(navOpenTexts[i].CStr(), ln) });
            }
            // 2) énumération annulable de l'arbre (homonymes prioritaires)
            NkVector<NkPath> named, others; int32 budget = 30000;   // budget = ENTRÉES (fichiers + dossiers) visitées
            for (usize i = 0; i < navDirs.Size() && !navCancel; ++i) NavWalk(NkPath(navDirs[i]), named, others, budget, 0);
            NkVector<NkPath> order; for (usize i = 0; i < named.Size(); ++i) order.PushBack(named[i]);
            for (usize i = 0; i < others.Size() && order.Size() < kMaxReads; ++i) order.PushBack(others[i]);
            navTotal = static_cast<int32>(order.Size());
            // 3) lecture bornée + annulable
            for (usize i = 0; i < order.Size() && navResults.Size() < kMaxHits; ++i) {
                if (navCancel) break;
                navScanned = static_cast<int32>(i) + 1;
                const NkString f = order[i].ToString(); if (already(f)) continue;
                const NkString txt = NkFile::ReadAllText(order[i]);
                const int32 ln = DefLineOf(txt.CStr(), navSym.CStr());
                if (ln >= 0) navResults.PushBack(NavHit{ f, ln, LineTextOf(txt.CStr(), ln) });
            }
            if (!navCancel) navScanned = navTotal;
            navDone = true;
        }
        void PollNav() {   // UI : rejoint le thread fini ; relance si une recherche est en attente
            if (!navBusy || !navDone) return;
            if (navThread.Joinable()) navThread.Join();
            navBusy = false;
            if (navPending) { navPending = false; NavLaunch(navPendSym, navPendDirs, navPendOpenP, navPendOpenT); return; }
            if (navCancel) { navCancel = false; return; }   // annulée sans nouvelle demande
            if (navResults.Empty()) { status = NkString("Définition introuvable : ") + navSym.CStr(); return; }
            if (navResults.Size() == 1) { OpenAt(NkPath(navResults[0].file), navResults[0].line); status = NkString(); return; }
            navPickerOpen = true; navPickerSel = 0;   // plusieurs -> liste de choix (façon VSCode)
            status = NkString();
        }
        int32 navPickChoice = -1;   // choix mémorisé (traité HORS rendu, cf. ProcessNavPick)
        void NavPick(int32 i) {   // appelé DEPUIS le rendu du picker -> on ne fait QUE mémoriser :
            // ouvrir ici ferait OpenPath -> files.PushBack -> réalloc pendant que le panneau
            // éditeur tient une référence `OpenFile& f` -> use-after-free (crash). On diffère.
            navPickChoice = i; navPickerOpen = false;
        }
        void ProcessNavPick() {   // appelé dans le poll (AVANT le rendu des panneaux) -> ouverture sûre
            if (navPickChoice < 0) return;
            const int32 i = navPickChoice; navPickChoice = -1;
            if (i >= 0 && i < static_cast<int32>(navResults.Size())) OpenAt(NkPath(navResults[i].file), navResults[i].line);
        }

        // ── Session : persiste les onglets + le contenu NON SAUVEGARDÉ (hot-exit VSCode /
        //    reprise après crash). `.nkcode/session.nk` (liste) + `.nkcode/bak<k>.txt` (buffers). ──
        bool mSessionLoaded = false; float32 mSessionTimer = 0.f; int64 mSessionSig = 0;
        static NkString IntToStr(int32 v) { char b[16]; std::snprintf(b, sizeof(b), "%d", v); return NkString(b); }
        int64 SessionSig() {
            int64 h = static_cast<int64>(1469598103934665603ULL);
            auto mix = [&](int64 v){ h = (h ^ static_cast<uint64>(v)) * 1099511628211LL; };
            mix(active); mix(static_cast<int64>(files.Size()));
            for (usize i = 0; i < files.Size(); ++i) { OpenFile& f = files[i]; for (const char* p = f.path.ToString().CStr(); *p; ++p) h = (h ^ (unsigned char)*p) * 1099511628211LL; mix(f.doc.curLine); mix(f.doc.curCol); mix(f.pinned ? 1 : 0); mix(static_cast<int32>(f.codeZoom)); mix(f.doc.dirty ? f.doc.SymSig() : 7); }
            return h;
        }
        void SaveSession() {
            if (!HasWorkspace()) return;
            NkPath dir = root / ".nkcode"; NkDirectory::CreateRecursive(dir);
            NkString s = NkString("nksession/2\nactive ") + IntToStr(active).CStr() + "\n";
            for (usize i = 0; i < files.Size(); ++i) {
                OpenFile& f = files[i]; const int32 dy = f.doc.dirty ? 1 : 0;
                s += "F "; s += IntToStr(f.doc.curLine).CStr(); s += " "; s += IntToStr(f.doc.curCol).CStr(); s += " ";
                s += IntToStr(f.pinned ? 1 : 0).CStr(); s += " "; s += IntToStr(dy).CStr(); s += " ";
                s += IntToStr(static_cast<int32>(f.codeZoom + 0.5f)).CStr(); s += " ";   // v2 : zoom par onglet (0 = global)
                s += f.path.ToString(); s += "\n";
                if (dy) NkFile::WriteAllText(dir / (NkString("bak") + IntToStr(static_cast<int32>(i)).CStr() + ".txt").CStr(), f.doc.GetText());
            }
            NkFile::WriteAllText(dir / "session.nk", s);
        }
        void LoadSession() {
            NkPath sf = root / ".nkcode" / "session.nk";
            if (!NkFile::Exists(sf)) return;
            const NkString raw = NkFile::ReadAllText(sf);
            struct Ent { NkString path; int32 cl, cc, pin, dy, zoom; };
            NkVector<Ent> ents; int32 savedActive = 0; int32 ver = 1;
            const char* p = raw.CStr(); static char line[65536];
            auto nextField = [](const char*& q){ while (*q == ' ') ++q; bool neg = false; if (*q == '-') { neg = true; ++q; } int32 v = 0; while (*q >= '0' && *q <= '9') { v = v * 10 + (*q - '0'); ++q; } return neg ? -v : v; };
            while (*p) {
                int32 n = 0; while (*p && *p != '\n' && *p != '\r' && n < 65535) line[n++] = *p++; line[n] = 0; while (*p == '\n' || *p == '\r') ++p;
                if (line[0] == 'n' && line[1] == 'k' && line[2] == 's') { const char* q = line; while (*q && *q != '/') ++q; if (*q == '/') { ++q; ver = nextField(q); } }   // "nksession/N"
                else if (line[0] == 'a' && line[1] == 'c') { const char* q = line + 6; savedActive = nextField(q); }
                else if (line[0] == 'F' && line[1] == ' ') {
                    const char* q = line + 2; const int32 cl = nextField(q), cc = nextField(q), pin = nextField(q), dy = nextField(q);
                    const int32 zoom = (ver >= 2) ? nextField(q) : 0;   // champ zoom present depuis v2
                    while (*q == ' ') ++q; ents.PushBack(Ent{ NkString(q), cl, cc, pin, dy, zoom });
                }
            }
            if (ents.Empty()) return;
            files.Clear(); active = -1;
            for (usize k = 0; k < ents.Size(); ++k) {
                Ent& e = ents[k];
                OpenPath(NkPath(e.path));
                if (active < 0 || active >= static_cast<int32>(files.Size())) continue;
                OpenFile& g = files[active];
                if (e.dy) { NkPath bak = root / ".nkcode" / (NkString("bak") + IntToStr(static_cast<int32>(k)).CStr() + ".txt").CStr(); if (NkFile::Exists(bak)) { g.doc.SetText(NkFile::ReadAllText(bak).CStr()); g.doc.dirty = true; } }
                g.doc.curLine = e.cl; g.doc.curCol = e.cc; g.doc.selLine = e.cl; g.doc.selCol = e.cc; g.doc.ClampCursor(); g.doc.wantReveal = true; g.pinned = (e.pin != 0);
                g.codeZoom = static_cast<float32>(e.zoom);   // zoom par onglet restaure (0 = global)
            }
            active = (savedActive >= 0 && savedActive < static_cast<int32>(files.Size())) ? savedActive : (files.Empty() ? -1 : 0);
        }
        // ── Surveillance des fichiers OUVERTS : suppression / modification EXTERNE (hors NKCode). ──
        float32 mFileWatchTimer = 0.f;
        void TickFileWatch(float32 dt) {
            mFileWatchTimer += dt; if (mFileWatchTimer < 1.5f) return; mFileWatchTimer = 0.f;
            for (usize i = 0; i < files.Size(); ++i) {
                OpenFile& f = files[i];
                if (f.untitled) continue;
                if (!NkFile::Exists(f.path)) {                 // supprimé sur le disque
                    if (!f.deletedOnDisk) { f.deletedOnDisk = true; f.doc.dirty = true; }   // buffer devient « non sauvegardé » -> ré-enregistrable
                    continue;
                }
                if (f.deletedOnDisk) { f.deletedOnDisk = false; f.diskMtime = MTimeOf(f.path.ToString().CStr()); }   // recréé
                const int64 mt = MTimeOf(f.path.ToString().CStr());
                if (f.diskMtime != 0 && mt > f.diskMtime) {
                    f.diskMtime = mt;
                    if (!f.doc.dirty) { f.doc.SetText(NkFile::ReadAllText(f.path).CStr()); f.doc.dirty = false; f.changedOnDisk = false; }   // pas de modif locale -> recharge
                    else f.changedOnDisk = true;               // conflit (modif locale + disque) -> bannière
                }
            }
        }
        void TickSession(float32 dt) {
            if (!HasWorkspace()) return;
            if (!mSessionLoaded) { mSessionLoaded = true; LoadSession(); mSessionSig = SessionSig(); return; }
            mSessionTimer += dt; if (mSessionTimer < 2.5f) return; mSessionTimer = 0.f;
            const int64 sig = SessionSig(); if (sig != mSessionSig) { mSessionSig = sig; SaveSession(); }
        }
        // ── Débounce : diagnostics ~0.6 s après la dernière frappe (pas besoin de save) ──
        int64 diagLastSig = 0; float32 diagTimer = 0.f; int32 diagLastFile = -1; bool diagArmed = false;
        void TickDiagnostics(float32 dt) {
            if (active < 0 || active >= static_cast<int32>(files.Size()) || !cdb.ready) return;
            OpenFile& f = files[active];
            if (!IsCppExt(f.path.GetExtension().CStr())) return;
            const int64 sig = f.doc.SymSig();
            if (sig != diagLastSig || active != diagLastFile) { diagLastSig = sig; diagLastFile = active; diagTimer = 0.f; diagArmed = true; return; }
            if (!diagArmed) return;
            diagTimer += dt;
            if (diagTimer >= 0.6f && !diagProc.Running()) { RunDiagnostics(active); diagArmed = false; }
        }
        // Parse une ligne d'erreur clang/gcc (`chemin:L:C: error|warning: msg`) ou MSVC
        // (`chemin(L,C): error Cxxxx: msg`). N'ajoute que les diags du fichier `self`.
        static void ParseDiagLine(const char* p, const char* self, NkCodeDoc& doc) {
            // ── clang/gcc : localise le délimiteur `:<line>:<col>: ` (gère le `:` de lecteur
            //    Windows ET le préfixe « fatal error: » des includes manquants). ──
            {
                auto isD = [](char c){ return c >= '0' && c <= '9'; };
                for (int32 i = 0; p[i]; ++i) {
                    if (p[i] != ':' || !isD(p[i + 1])) continue;
                    int32 j = i + 1, line = 0; while (isD(p[j])) { line = line * 10 + (p[j] - '0'); ++j; }
                    if (p[j] != ':' || !isD(p[j + 1])) continue;
                    int32 k = j + 1, col = 0; while (isD(p[k])) { col = col * 10 + (p[k] - '0'); ++k; }
                    if (p[k] != ':' || p[k + 1] != ' ') continue;
                    const char* sevp = p + k + 2;   // après « : » -> "error:" / "fatal error:" / "warning:" / "note:"
                    uint8 sev;
                    if (NkFindSub(sevp, "error:") == sevp || NkFindSub(sevp, "fatal error:") == sevp) sev = 1;
                    else if (NkFindSub(sevp, "warning:") == sevp) sev = 0;
                    else return;   // note:/remark: -> ignore
                    if (!NkPathSuffixMatch(p, i, self)) return;   // pas notre fichier (ex header inclus)
                    const char* msg = sevp; while (*msg && *msg != ':') ++msg; if (*msg == ':') { ++msg; while (*msg == ' ') ++msg; }
                    if (line > 0) doc.diags.PushBack({ line - 1, col > 0 ? col - 1 : 0, col > 0 ? col - 1 : 0, sev, NkString(*msg ? msg : sevp) });
                    return;
                }
            }
            // ── MSVC ──  chemin(L,C): error Cxxxx: msg   /  chemin(L): error ...
            const char* pe = NkFindSub(p, "): error "); uint8 s2 = 1;
            if (!pe) { pe = NkFindSub(p, "): warning "); s2 = 0; }
            if (pe) {
                int32 op = -1; for (int32 k = static_cast<int32>(pe - p); k >= 0; --k) if (p[k] == '(') { op = k; break; }
                if (op < 0) return;
                const int32 line = NkAtoi(p + op + 1); int32 col = 0; const char* cc = p + op + 1; while (*cc && *cc != ',' && *cc != ')') ++cc; if (*cc == ',') col = NkAtoi(cc + 1);
                if (!NkPathSuffixMatch(p, op, self)) return;
                const char* msg = pe + 2; while (*msg && *msg != ':') ++msg; if (*msg == ':') ++msg; while (*msg == ' ') ++msg;
                if (line > 0) doc.diags.PushBack({ line - 1, col > 0 ? col - 1 : 0, col > 0 ? col - 1 : 0, s2, NkString(msg) });
            }
        }
        // (NkPathSuffixMatch -> NkText.h)

        // Ferme l'onglet `i` et reajuste l'onglet actif.
        void CloseFile(int32 i) {
            if (i < 0 || i >= static_cast<int32>(files.Size())) return;
            files.Erase(files.Begin() + i);
            if (active >= static_cast<int32>(files.Size())) active = static_cast<int32>(files.Size()) - 1;
            if (active < 0 && !files.Empty()) active = 0;
        }
        // ── Actions d'onglets (menu contextuel de la barre d'onglets) ──
        void TogglePin(int32 i) { if (i >= 0 && i < static_cast<int32>(files.Size())) files[i].pinned = !files[i].pinned; }
        // Ferme tous les onglets SAUF `keep` (et sauf les epingles). `keep` reste actif.
        void CloseOthers(int32 keep) {
            if (keep < 0 || keep >= static_cast<int32>(files.Size())) return;
            const NkString keepPath = files[keep].path.ToString();
            for (int32 i = static_cast<int32>(files.Size()) - 1; i >= 0; --i) {
                if (files[i].pinned) continue;
                if (StrEq(files[i].path.ToString().CStr(), keepPath.CStr())) continue;
                files.Erase(files.Begin() + i);
            }
            SyncActiveTo(keepPath);
        }
        // Ferme tous les onglets a DROITE de `i` (sauf epingles).
        void CloseToRight(int32 i) {
            if (i < 0 || i >= static_cast<int32>(files.Size())) return;
            const NkString keepPath = files[i].path.ToString();
            for (int32 j = static_cast<int32>(files.Size()) - 1; j > i; --j)
                if (!files[j].pinned) files.Erase(files.Begin() + j);
            SyncActiveTo(keepPath);
        }
        void SyncActiveTo(const NkString& path) {
            for (int32 i = 0; i < static_cast<int32>(files.Size()); ++i)
                if (StrEq(files[i].path.ToString().CStr(), path.CStr())) { active = i; return; }
            if (active >= static_cast<int32>(files.Size())) active = static_cast<int32>(files.Size()) - 1;
            if (active < 0 && !files.Empty()) active = 0;
        }

        bool SaveActive() {
            if (active < 0 || active >= static_cast<int32>(files.Size())) return false;
            OpenFile& f = files[active];
            if (NkFile::WriteAllText(f.path, f.doc.GetText())) {
                f.doc.dirty = false; f.untitled = false; f.deletedOnDisk = false; f.changedOnDisk = false; f.diskMtime = MTimeOf(f.path.ToString().CStr());
                status = NkString("Enregistre : ") + f.Name().CStr();
                RefreshGit(f);   // met à jour la bande Git après écriture disque
                RunDiagnostics(active);   // vérif syntaxe (squiggles) sur le fichier sauvegardé
                // Sauvegarde d'un .jenga (workspace OU projet inclus) -> recharge la
                // liste des projets (jenga info relit le workspace + ses includes).
                if (EndsWithI(f.Name().CStr(), ".jenga")) RequestReload();
                return true;
            }
            status = NkString("Echec enregistrement"); return false;
        }

        bool HasActive() const { return active >= 0 && active < static_cast<int32>(files.Size()); }
        bool ActiveHasPath() const { return HasActive() && !files[active].untitled && !files[active].path.ToString().Empty(); }
        // Recharge l'onglet actif depuis le disque (abandonne les modifs locales).
        void ReloadActive() {
            if (!HasActive()) return; OpenFile& f = files[active];
            f.doc.SetText(NkFile::ReadAllText(f.path).CStr()); f.doc.dirty = false; f.changedOnDisk = false; f.deletedOnDisk = false;
            f.diskMtime = MTimeOf(f.path.ToString().CStr()); f.doc.ClampCursor(); RefreshGit(f);
        }

        // Nouveau fichier sans titre (onglet vide). Reste « sans titre » jusqu'a
        // un Enregistrer sous (path vide -> SaveActive renvoie false).
        void NewFile() {
            OpenFile f;                 // path vide
            f.doc.SetText(""); f.untitled = true;
            files.PushBack(f);
            active = static_cast<int32>(files.Size()) - 1;
        }

        // Enregistre l'onglet actif vers `p` (Enregistrer sous).
        bool SaveActiveAs(const NkPath& p) {
            if (!HasActive()) return false;
            files[active].path = p;
            return SaveActive();
        }

        // Enregistre tous les onglets modifies ayant un chemin. Renvoie le nombre ecrit.
        int32 SaveAll() {
            const int32 keep = active;
            int32 n = 0;
            for (int32 i = 0; i < static_cast<int32>(files.Size()); ++i) {
                if (files[i].path.ToString().Empty() || !files[i].doc.dirty) continue;
                active = i; if (SaveActive()) ++n;
            }
            active = keep;
            char sb[48]; std::snprintf(sb, sizeof(sb), "Tout enregistre (%d fichier(s))", n);
            status = NkString(sb);
            return n;
        }

        NkProcess mBuild;   // build ASYNCHRONE (ne gele pas l'UI)
        NkProcess mCfg;     // commandes `jenga config ...` (toolchains)
        bool      mCfgPending = false;
        NkString  cfgStatus;

        // Lance une commande `jenga config ...` (ex. toolchain add/remove) puis,
        // a la fin, force la re-detection des toolchains (RequestReload).
        void RunConfig(const NkString& args) {
            if (mCfg.Running()) return;
            cfgStatus = NkString("jenga ") + args.CStr();
            mCfg.Start(cfgStatus);
            mCfgPending = true;
        }
        void PollConfig() {
            if (!mCfgPending) return;
            NkVector<NkString> sink; mCfg.Drain(sink);
            if (!mCfg.Running()) {
                mCfgPending = false;
                cfgStatus = (mCfg.ExitCode() == 0) ? NkString("Toolchain : OK") : NkString("Toolchain : echec");
                RequestReload();   // re-detecte les toolchains (jenga info)
            }
        }
        // Ecrit un fichier JSON de toolchain (format `jenga config toolchain add`)
        // dans le home, puis lance l'ajout. Retourne false si nom vide.
        bool ToolchainAdd(const char* name, const NkString& json) {
            if (!name || !*name) return false;
            const char* home = std::getenv("USERPROFILE"); if (!home || !*home) home = std::getenv("HOME");
            NkString jp = (home && *home) ? (NkString(home) + "/.nkcode_tc_tmp.json") : NkString("nkcode_tc_tmp.json");
            if (!NkFile::WriteAllText(NkPath(jp.CStr()), json)) return false;
            RunConfig(NkString("config toolchain add ") + name + " \"" + jp.CStr() + "\"");
            return true;
        }
        void ToolchainRemove(const char* name) {
            if (name && *name) RunConfig(NkString("config toolchain remove ") + name);
        }

        // Lance `jenga <args>` en arriere-plan ; la sortie arrive via PollBuild().
        void StartJenga(const char* args) {
            output.Clear();
            NkString cmd("jenga "); cmd += args;
            output.PushBack(NkString("$ ") + cmd.CStr());
            if (!mBuild.Start(cmd)) { status = NkString("Build deja en cours..."); return; }
            status = NkString("Construction...");
        }

        // A appeler CHAQUE FRAME : recupere la sortie + enchaine la file + statut.
        void PollBuild() {
            mBuild.Drain(output);
            if (!mBuild.Running()) {
                if (!mQueue.Empty()) { PumpQueue(); return; }   // commande suivante (rafale)
                if (status.Size() > 0 && StrEq(status.CStr(), "Construction..."))
                    status = (mBuild.ExitCode() == 0) ? NkString("Termine (OK)") : NkString("Termine (echec)");
            }
        }

        // ── Projets du workspace (un .jenga en contient plusieurs) ───────────────
        NkVector<NkString> projects;     // projets (hors tests) selectionnables
        int32              projIdx = 0;  // projet cible courant
        NkVector<NkString> tests;        // projets de test (Kind = TestSuite)
        int32              testIdx = -1; // -1 = tous les tests visibles ; >=0 = un test precis

        // Toolchains DETECTEES par Jenga (table "Available Toolchains" de `jenga info`).
        struct ToolchainRow { NkString name, family, os, arch, env; };
        NkVector<ToolchainRow> toolchains;
        // Compilateur FORCE pour la plateforme courante ("" = auto/meilleur match).
        // Envoye a `jenga build --toolchain <name>`. Reinitialise si l'entree ne
        // correspond plus a la plateforme selectionnee.
        NkString compilerName;

        // Indices (dans `toolchains`) des compilateurs disponibles pour la plateforme
        // courante (os == Systems()[sysIdx].name). Pilote le combo compilateur.
        NkVector<int32> CompilersForCurrentPlatform() const {
            NkVector<int32> out;
            int32 nSys = 0; const SysDef* sys = Systems(&nSys);
            const char* osname = sys[(sysIdx >= 0 && sysIdx < nSys) ? sysIdx : 0].name;
            for (int32 i = 0; i < static_cast<int32>(toolchains.Size()); ++i)
                if (StrEqI(toolchains[i].os.CStr(), osname)) out.PushBack(i);
            return out;
        }
        // Si le compilateur force ne fait plus partie de la plateforme courante -> auto.
        void ValidateCompilerForPlatform() {
            if (compilerName.Empty()) return;
            const NkVector<int32> cs = CompilersForCurrentPlatform();
            for (usize i = 0; i < cs.Size(); ++i)
                if (StrEqI(toolchains[cs[i]].name.CStr(), compilerName.CStr())) return;
            compilerName = NkString();   // n'appartient plus a cette plateforme
        }
        // Infos d'en-tete de `jenga info` (pour les cartes workspace).
        NkString infoConfigs;   // "Debug, Release"
        NkString infoOSes;      // "Windows, Linux, macOS, ..."

        const char* SelectedProject() const {
            return (projIdx >= 0 && projIdx < static_cast<int32>(projects.Size()))
                 ? projects[projIdx].CStr() : "";
        }

        // Charge la liste des projets du WORKSPACE selectionne via `jenga info`
        // (ASYNCHRONE). Recharge automatiquement quand on change de workspace.
        void LoadProjects() {
            if (mInfoStarted && mInfoWsIdx == wsIdx) return;
            mInfoStarted = true; mInfoParsed = false; mInfoWsIdx = wsIdx;
            mInfoLines.Clear(); projects.Clear();
            mInfo.Start(NkString("jenga info") + JengaFileArg().CStr());
        }

        // A appeler CHAQUE FRAME : draine `jenga info` puis parse la table des projets.
        void PollProjects() {
            if (!mInfoStarted || mInfoParsed) return;
            mInfo.Drain(mInfoLines);
            if (mInfo.Done()) { mInfoParsed = true; ParseProjects(); }
        }

        // ── Aides pour l'écran de chargement (section 14) ──
        bool InfoStarted() const { return mInfoStarted; }
        bool InfoParsed()  const { return mInfoParsed; }
        bool InfoHasError() const {
            for (usize i = 0; i < mInfoLines.Size(); ++i) { const NkString c = CleanLine(mInfoLines[i].CStr());
                if (c.Contains("Error:") || c.Contains("Traceback") || c.Contains("Exception")) return true; }
            return false;
        }
        // Renvoie la ligne d'erreur la plus parlante, préfixée du n° de ligne si trouvé.
        NkString InfoErrorLine() const {
            NkString num, err;
            for (usize i = 0; i < mInfoLines.Size(); ++i) { const NkString c = CleanLine(mInfoLines[i].CStr());
                if (c.Contains("Error:") || c.Contains("Exception:")) err = c;
                const char* p = c.CStr();
                for (const char* q = p; *q; ++q) if (q[0]=='l'&&q[1]=='i'&&q[2]=='n'&&q[3]=='e'&&q[4]==' ') { const char* d = q + 5; NkString n; while (*d >= '0' && *d <= '9') n += *d++; if (!n.Empty()) num = n; }
            }
            NkString out; if (!num.Empty()) { out += "Ligne "; out += num; out += " : "; }
            out += err.Empty() ? NkString("erreur inconnue (voir logs)") : err;
            return out;
        }

        // ── Exemples Jenga : enumeres dynamiquement via `jenga examples list` ──
        struct Example { NkString id, desc, platforms, difficulty; };
        NkVector<Example>  examples;
        NkProcess          mExamples;
        bool               mExStarted = false, mExParsed = false;
        NkVector<NkString> mExLines;

        void LoadExamples() {
            if (mExStarted) return;
            mExStarted = true; mExParsed = false; mExLines.Clear();
            mExamples.Start(NkString("jenga examples list"));
        }
        void PollExamples() {
            if (!mExStarted || mExParsed) return;
            mExamples.Drain(mExLines);
            if (mExamples.Done()) { mExParsed = true; ParseExamples(); }
        }
        // Retire les codes ANSI (ESC[...m) + trim debut/fin d'une ligne.
        static NkString CleanLine(const char* s) {
            char out[512]; usize n = 0;
            for (const char* p = s; *p && n + 1 < sizeof(out); ++p) {
                if (*p == 0x1b) { while (*p && *p != 'm') ++p; if (!*p) break; continue; }
                out[n++] = *p;
            }
            out[n] = '\0';
            char* b = out; while (*b == ' ' || *b == '\t') ++b;
            usize m = 0; while (b[m]) ++m;
            while (m > 0 && (b[m - 1] == ' ' || b[m - 1] == '\r' || b[m - 1] == '\t')) --m;
            b[m] = '\0';
            return NkString(b);
        }
        void ParseExamples() {
            examples.Clear();
            for (usize i = 0; i < mExLines.Size(); ++i) {
                const NkString cl = CleanLine(mExLines[i].CStr());
                const char* L = cl.CStr();
                if (StartsWithI(L, "ID:")) { Example e; e.id = AfterColon(L); examples.PushBack(e); }
                else if (!examples.Empty()) {
                    Example& e = examples[examples.Size() - 1];
                    if      (StartsWithI(L, "Description:")) e.desc       = AfterColon(L);
                    else if (StartsWithI(L, "Platforms:"))   e.platforms  = AfterColon(L);
                    else if (StartsWithI(L, "Difficulty:"))  e.difficulty = AfterColon(L);
                }
            }
        }

        // Construit le projet selectionne (config/plateforme courantes).
        void BuildSelected(const char* platformArg) {
            NkString a("build --target "); a += SelectedProject();
            a += " --config "; a += ConfigName();
            if (platformArg && platformArg[0]) { a += " --platform "; a += platformArg; }
            StartJenga(a.CStr());
        }
        // Lance (run) le projet selectionne ; --build force la (re)compilation avant.
        void RunSelected(const char* platformArg, const char* deviceArg) {
            NkString a("run "); a += SelectedProject();
            a += " --config "; a += ConfigName();
            if (platformArg && platformArg[0]) { a += " --platform "; a += platformArg; }
            if (deviceArg && deviceArg[0])     { a += " --device ";   a += deviceArg; }
            a += " --build";
            StartJenga(a.CStr());
        }

        const char* ConfigName() const { return cfgIdx == 1 ? "Release" : "Debug"; }

        // ====================================================================
        // ── Barre d'outils build complete : workspaces -> projets -> system ->
        //    config -> architecture -> [Construire/Recompiler/Nettoyer/Demarrer]
        // ====================================================================

        // Systeme cible + ses architectures (encodees dans --platform <OS>-<arch>).
        struct SysDef { const char* name; const char* archs[6]; int32 nArch; };
        static const SysDef* Systems(int32* n) {
            static const SysDef s[] = {
                { "Windows",    { "x86_64", "x86", "arm64" },          3 },
                { "Linux",      { "x86_64", "arm64" },                  2 },
                { "macOS",      { "x86_64", "arm64" },                  2 },
                { "Android",    { "arm64", "arm", "x86", "x86_64" },    4 },
                { "iOS",        { "arm64" },                            1 },
                { "Web",        { "wasm32" },                           1 },
                { "HarmonyOS",  { "arm64" },                            1 },
                { "XboxSeries", { "x86_64" },                           1 },
            };
            if (n) *n = 8; return s;
        }
        int32 sysIdx  = 0;   // index dans Systems()
        int32 archIdx = 0;   // 0..nArch-1 = arch precise ; == nArch -> "Toutes"

        // ── Workspaces : fichiers .jenga a la racine contenant "with workspace" ──
        NkVector<NkString> wsPaths, wsNames;
        int32 wsIdx = 0;
        bool  mWsScanned = false;

        void ScanWorkspaces() {
            if (mWsScanned) return; mWsScanned = true;
            wsPaths.Clear(); wsNames.Clear();
            NkVector<NkDirectoryEntry> entries = NkDirectory::GetEntries(root, "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
            for (usize i = 0; i < entries.Size(); ++i) {
                const NkDirectoryEntry& e = entries[i];
                if (e.IsDirectory) continue;
                const NkString nm = e.Name;
                if (!EndsWithI(nm.CStr(), ".jenga")) continue;
                const NkString txt = NkFile::ReadAllText(e.FullPath);
                if (!Contains(txt.CStr(), "with workspace") && !Contains(txt.CStr(), "workspace(")) continue;
                wsPaths.PushBack(e.FullPath.ToString());
                wsNames.PushBack(WorkspaceName(txt, nm));
            }
            if (wsIdx < 0 || wsIdx >= static_cast<int32>(wsPaths.Size())) wsIdx = 0;
        }
        bool HasWorkspace() const { return !wsPaths.Empty(); }

        // Scanne un dossier ARBITRAIRE pour ses workspaces (sans toucher a la racine).
        // Sert au panneau « Charger » du launcher (apercu avant chargement).
        static void ScanWorkspacesIn(const NkPath& folder, NkVector<NkString>& outPaths,
                                     NkVector<NkString>& outNames) {
            outPaths.Clear(); outNames.Clear();
            NkVector<NkDirectoryEntry> entries = NkDirectory::GetEntries(folder, "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
            for (usize i = 0; i < entries.Size(); ++i) {
                const NkDirectoryEntry& e = entries[i];
                if (e.IsDirectory) continue;
                const NkString nm = e.Name;
                if (!EndsWithI(nm.CStr(), ".jenga")) continue;
                const NkString txt = NkFile::ReadAllText(e.FullPath);
                if (!Contains(txt.CStr(), "with workspace") && !Contains(txt.CStr(), "workspace(")) continue;
                outPaths.PushBack(e.FullPath.ToString());
                outNames.PushBack(WorkspaceName(txt, nm));
            }
        }

        // Fichier de config d'interface PAR PROJET : <racine>/.nkcode/ui.cfg
        // (etat maximise + panneaux ouverts, lu/ecrit par le shell).
        NkString UiConfigPath() const { return (root / ".nkcode" / "ui.cfg").ToString(); }

        // Charge `folder` comme racine de travail : re-scan des workspaces du dossier.
        // REFUSE (renvoie false, racine inchangee) si aucun workspace (.jenga contenant
        // "with workspace") n'y est trouve — qu'il ait ete cree par l'UI ou non.
        bool LoadFolder(const NkPath& folder) {
            const NkPath saved = root;
            root  = folder;
            wsIdx = 0;
            mWsScanned = false;
            ScanWorkspaces();
            if (!HasWorkspace()) {                       // aucun workspace -> refus
                root = saved; mWsScanned = false; ScanWorkspaces();
                return false;
            }
            mLastJengaMtime = 0;                         // re-amorce le watch sur la nouvelle racine
            files.Clear(); active = -1;                  // onglets repartent a zero
            RequestReload();                             // recharge la liste des projets
            AddRecent(wsPaths[wsIdx]);                   // memorise dans les recents
            // Restaure la SESSION de ce workspace (onglets + contenu non sauvegardé). On N'OUVRE PAS
            // le .jenga d'office : il ne réapparaît que s'il était un onglet de la session précédente.
            LoadSession();
            mSessionLoaded = true; mSessionTimer = 0.f; mSessionSig = SessionSig();
            status = NkString("Workspace charge : ") + folder.ToString().CStr();
            return true;
        }

        // ── Recents + epingles : workspaces ouverts avec l'IDE (~/.nkcode_recent.cfg) ──
        // Fichier : 1 ligne/entree, prefixe "P " = epingle, "R " (ou rien) = recent.
        NkVector<NkString> recents;   // non epingles (ordre = recence) — chemins .jenga
        NkVector<NkString> pinned;    // epingles (restent en tete)
        NkVector<NkString> recentNames, pinnedNames;   // noms `with workspace(...)` (cache)

        // Lit un .jenga et renvoie le nom du workspace (`with workspace("NAME")`),
        // ou le nom de fichier sans extension en repli.
        static NkString WorkspaceNameOf(const char* jengaPath) {
            const NkString txt = NkFile::ReadAllText(NkPath(jengaPath));
            return WorkspaceName(txt, NkPath(jengaPath).GetFileNameWithoutExtension());
        }
        void RebuildRecentNames() {
            recentNames.Clear(); pinnedNames.Clear();
            for (usize i = 0; i < recents.Size(); ++i) { const char* o = NameOverride(recents[i].CStr()); recentNames.PushBack(o ? NkString(o) : WorkspaceNameOf(recents[i].CStr())); }
            for (usize i = 0; i < pinned.Size();  ++i) { const char* o = NameOverride(pinned[i].CStr());  pinnedNames.PushBack(o ? NkString(o) : WorkspaceNameOf(pinned[i].CStr())); }
        }

        // ── Noms personnalises (menu "Renommer dans les recents") -> ~/.nkcode_recent_names.cfg ──
        NkVector<NkString> nameOvrPath, nameOvrName;
        static NkString NamesPath() {
            const char* home = std::getenv("USERPROFILE"); if (!home || !*home) home = std::getenv("HOME");
            if (home && *home) return NkString(home) + "/.nkcode_recent_names.cfg";
            return NkString("nkcode_recent_names.cfg");
        }
        const char* NameOverride(const char* path) const {
            for (usize i = 0; i < nameOvrPath.Size(); ++i) if (StrEq(nameOvrPath[i].CStr(), path)) return nameOvrName[i].CStr();
            return nullptr;
        }
        void LoadNameOverrides() {
            nameOvrPath.Clear(); nameOvrName.Clear();
            NkString txt = NkFile::ReadAllText(NkPath(NamesPath().CStr())), line;
            auto flush = [&]() {
                if (line.Empty()) return;
                const char* s = line.CStr(); const char* bar = nullptr;
                for (const char* p = s; *p; ++p) if (*p == '|') { bar = p; break; }
                if (bar) {
                    char pbuf[512]; usize n = (usize)(bar - s); if (n >= sizeof(pbuf)) n = sizeof(pbuf) - 1;
                    for (usize k = 0; k < n; ++k) pbuf[k] = s[k]; pbuf[n] = '\0';
                    nameOvrPath.PushBack(NkString(pbuf)); nameOvrName.PushBack(NkString(bar + 1));
                }
                line.Clear();
            };
            for (const char* p = txt.CStr(); *p; ++p) { if (*p == '\n' || *p == '\r') flush(); else line += *p; }
            flush();
        }
        void SaveNameOverrides() {
            NkString out;
            for (usize i = 0; i < nameOvrPath.Size(); ++i) { out += nameOvrPath[i]; out += "|"; out += nameOvrName[i]; out += "\n"; }
            NkFile::WriteAllText(NkPath(NamesPath().CStr()), out);
        }
        void SetRecentName(const NkString& path, const NkString& name) {
            for (usize i = 0; i < nameOvrPath.Size(); ++i) if (StrEq(nameOvrPath[i].CStr(), path.CStr())) {
                if (name.Empty()) { nameOvrPath.Erase(nameOvrPath.Begin() + i); nameOvrName.Erase(nameOvrName.Begin() + i); }
                else nameOvrName[i] = name;
                SaveNameOverrides(); RebuildRecentNames(); return;
            }
            if (!name.Empty()) { nameOvrPath.PushBack(path); nameOvrName.PushBack(name); SaveNameOverrides(); RebuildRecentNames(); }
        }
        static NkString RecentsPath() {
            const char* home = std::getenv("USERPROFILE");
            if (!home || !*home) home = std::getenv("HOME");
            if (home && *home) return NkString(home) + "/.nkcode_recent.cfg";
            return NkString("nkcode_recent.cfg");
        }
        static void RemoveFrom(NkVector<NkString>& v, const char* path) {
            for (usize i = 0; i < v.Size(); )
                if (StrEq(v[i].CStr(), path)) v.Erase(v.Begin() + i); else ++i;
        }
        bool IsPinned(const char* path) const {
            for (usize i = 0; i < pinned.Size(); ++i) if (StrEq(pinned[i].CStr(), path)) return true;
            return false;
        }
        void LoadRecents() {
            recents.Clear(); pinned.Clear();
            NkString txt = NkFile::ReadAllText(NkPath(RecentsPath().CStr()));
            NkString cur;
            auto flush = [&]() {
                if (cur.Empty()) return;
                if (cur.CStr()[0] == 'P' && cur.CStr()[1] == ' ') pinned.PushBack(NkString(cur.CStr() + 2));
                else if (cur.CStr()[0] == 'R' && cur.CStr()[1] == ' ') recents.PushBack(NkString(cur.CStr() + 2));
                else recents.PushBack(cur);   // ancien format (chemin nu)
                cur.Clear();
            };
            for (const char* p = txt.CStr(); *p; ++p) { if (*p == '\n' || *p == '\r') flush(); else cur += *p; }
            flush();
            LoadNameOverrides();
            RebuildRecentNames();
        }
        void SaveRecents() {
            NkString out;
            for (usize i = 0; i < pinned.Size(); ++i)  { out += "P "; out += pinned[i];  out += "\n"; }
            for (usize i = 0; i < recents.Size(); ++i) { out += "R "; out += recents[i]; out += "\n"; }
            NkFile::WriteAllText(NkPath(RecentsPath().CStr()), out);
        }
        void AddRecent(const NkString& wsPath) {
            if (IsPinned(wsPath.CStr())) return;          // deja epingle -> reste en tete
            NkVector<NkString> nw;
            nw.PushBack(wsPath);                          // en tete (le plus recent)
            for (usize i = 0; i < recents.Size() && nw.Size() < 12; ++i)
                if (!StrEq(recents[i].CStr(), wsPath.CStr())) nw.PushBack(recents[i]);
            recents = nw;
            SaveRecents(); RebuildRecentNames();
        }
        void PinRecent(const NkString& path)   { RemoveFrom(recents, path.CStr()); if (!IsPinned(path.CStr())) pinned.PushBack(path); SaveRecents(); RebuildRecentNames(); }
        void UnpinRecent(const NkString& path) { RemoveFrom(pinned, path.CStr()); RemoveFrom(recents, path.CStr()); recents.Insert(recents.Begin(), path); SaveRecents(); RebuildRecentNames(); }
        void RemoveRecent(const NkString& path){ RemoveFrom(recents, path.CStr()); RemoveFrom(pinned, path.CStr()); SaveRecents(); RebuildRecentNames(); }

        // ── Dates : groupes AUJOURD'HUI / CETTE SEMAINE + libelle "Modifie il y a X" ──
        static int64 NowEpoch() { return static_cast<int64>(std::time(nullptr)); }
        // Date de modif d'un .jenga via NkDirectory (evite NKFileSystem.h -> collision
        // macro winbase 'GetFreeSpace'). NkDirectoryEntry.ModificationTime peut etre un
        // FILETIME brut (100ns depuis 1601) sur Windows -> on normalise en epoch Unix (s).
        static int64 MTimeOf(const char* path) {
            const NkPath p(path);
            const NkString fname = p.GetFileName();
            NkVector<NkDirectoryEntry> es = NkDirectory::GetEntries(p.GetParent(), fname.CStr(), NkSearchOption::NK_TOP_DIRECTORY_ONLY);
            int64 t = 0;
            for (usize i = 0; i < es.Size(); ++i)
                if (StrEq(es[i].Name.CStr(), fname.CStr())) { t = static_cast<int64>(es[i].ModificationTime); break; }
            if (t > 100000000000000LL) t = (t - 116444736000000000LL) / 10000000LL;
            return t;
        }
        // 0 = aujourd'hui (<24h), 1 = cette semaine (<7j), 2 = plus ancien.
        static int32 AgeBucket(int64 mtime, int64 now) {
            if (mtime <= 0) return 3;
            const int64 d = now - mtime;
            if (d < 86400)        return 0;   // aujourd'hui
            if (d < 7 * 86400)    return 1;   // cette semaine
            if (d < 30 * 86400)   return 2;   // ce mois
            return 3;                          // plus anciens
        }
        static const char* BucketLabel(int32 b) {
            return b == 0 ? "AUJOURD'HUI" : b == 1 ? "CETTE SEMAINE" : b == 2 ? "CE MOIS" : "PLUS ANCIEN";
        }
        // ── "Derniere activite reelle" : le fichier le PLUS recemment modifie du dossier
        //    workspace (hors Build/.git/Externals/...). Borne par un budget de fichiers. ──
        static bool IsSkippedDir(const char* nm) {
            static const char* skip[] = { "Build","build","Externals","External","node_modules","cache",
                                          "dist","tmps","tmp","__pycache__","bin","obj","target",".git",".nkcode",".vs",".idea" };
            for (const char* s : skip) if (StrEqI(nm, s)) return true;
            return false;
        }
        static void ScanActivity(const NkPath& dir, int64& maxT, int32& budget) {
            if (budget <= 0) return;
            NkVector<NkDirectoryEntry> es = NkDirectory::GetEntries(dir, "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
            for (usize i = 0; i < es.Size() && budget > 0; ++i) {
                const NkDirectoryEntry& e = es[i];
                const char* nm = e.Name.CStr();
                if (e.IsDirectory) {
                    if (nm[0] == '.' || IsSkippedDir(nm)) continue;
                    ScanActivity(e.FullPath, maxT, budget);
                } else {
                    --budget;
                    int64 t = static_cast<int64>(e.ModificationTime);
                    if (t > 100000000000000LL) t = (t - 116444736000000000LL) / 10000000LL;   // FILETIME->epoch (repli)
                    if (t > maxT) maxT = t;
                }
            }
        }
        static int64 ActivityTime(const char* folder) {
            int64 maxT = 0; int32 budget = 2500;
            if (folder && *folder) ScanActivity(NkPath(folder), maxT, budget);
            return maxT;
        }

        static NkString HumanAge(int64 mtime, int64 now) {
            if (mtime <= 0) return NkString("");
            int64 d = now - mtime; if (d < 0) d = 0;
            char b[64];
            if      (d < 60)        std::snprintf(b, sizeof(b), "%s", NkT("age.now"));
            else if (d < 3600)      std::snprintf(b, sizeof(b), NkT("age.min"), (int)(d / 60));
            else if (d < 86400)     std::snprintf(b, sizeof(b), NkT("age.h"),   (int)(d / 3600));
            else if (d < 7 * 86400) std::snprintf(b, sizeof(b), NkT("age.j"),   (int)(d / 86400));
            else                    std::snprintf(b, sizeof(b), NkT("age.sem"), (int)(d / (7 * 86400)));
            return NkString(b);
        }

        // ── Metadonnees d'un workspace NON ouvert (carte du launcher) ─────────────
        // Parse leger du .jenga : configs, plateformes, langage, projets. Mis en
        // cache par chemin (le Home redessine chaque frame -> pas de relecture disque).
        struct WsMeta { NkString path, configs, platforms, projects, langVer, toolchains, jengaVer; int64 activity = 0; int32 projCount = 0; };
        NkVector<WsMeta> mWsMeta;

        static char UpC(char c) { return (c >= 'a' && c <= 'z') ? char(c - 'a' + 'A') : c; }
        static bool StrEqI(const char* a, const char* b) {
            if (!a || !b) return a == b;
            while (*a && *b) { if (UpC(*a) != UpC(*b)) return false; ++a; ++b; }
            return *a == *b;
        }
        // Sous-chaine insensible a la casse (filtres de recherche).
        static bool ContainsI(const char* hay, const char* needle) {
            if (!needle || !*needle) return true;
            if (!hay) return false;
            for (const char* h = hay; *h; ++h) {
                const char* a = h; const char* b = needle;
                while (*a && *b && UpC(*a) == UpC(*b)) { ++a; ++b; }
                if (!*b) return true;
            }
            return false;
        }
        static const char* FindStr(const char* h, const char* n) {
            if (!h || !n || !*n) return nullptr;
            for (; *h; ++h) { const char* a = h; const char* b = n; while (*a && *b && *a == *b) { ++a; ++b; } if (!*b) return h; }
            return nullptr;
        }
        // Concatene les chaines entre guillemets a l'interieur de `open` jusqu'a ']'.
        static NkString JoinQuotedInCall(const char* txt, const char* open) {
            const char* s = FindStr(txt, open); if (!s) return NkString();
            s += Len(open);
            NkString out;
            for (const char* p = s; *p && *p != ']'; ++p)
                if (*p == '"') { ++p; NkString tok; while (*p && *p != '"') tok += *p++; if (!tok.Empty()) { if (!out.Empty()) out += ", "; out += tok; } }
            return out;
        }
        static NkString FriendlyOS(const char* t) {
            if (StrEqI(t, "WINDOWS"))   return NkString("Windows");
            if (StrEqI(t, "LINUX"))     return NkString("Linux");
            if (StrEqI(t, "MACOS"))     return NkString("macOS");
            if (StrEqI(t, "ANDROID"))   return NkString("Android");
            if (StrEqI(t, "IOS"))       return NkString("iOS");
            if (StrEqI(t, "WEB") || StrEqI(t, "EMSCRIPTEN")) return NkString("Web");
            if (StrEqI(t, "HARMONYOS")) return NkString("HarmonyOS");
            NkString o; bool first = true;                       // repli : Titlecase
            for (const char* p = t; *p; ++p) { char c = *p; c = first ? UpC(c) : char((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c); first = false; o += c; }
            return o;
        }
        // Tokens `prefix.XXX` a l'interieur de `open` jusqu'a ']' (ex. TargetOS.WINDOWS).
        static NkString JoinEnumInCall(const char* txt, const char* open, const char* prefix) {
            const char* s = FindStr(txt, open); if (!s) return NkString();
            s += Len(open);
            const usize pl = Len(prefix); NkString out;
            for (const char* p = s; *p && *p != ']'; ++p) {
                bool m = true; for (usize k = 0; k < pl; ++k) if (p[k] != prefix[k]) { m = false; break; }
                if (!m) continue;
                p += pl; NkString tok;
                while (*p && (*p == '_' || (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))) tok += *p++;
                --p;
                if (!tok.Empty()) { if (!out.Empty()) out += ", "; out += FriendlyOS(tok.CStr()); }
            }
            return out;
        }
        static NkString DetectLang(const char* txt) {
            const char* d = FindStr(txt, "cppdialect(\"");
            if (d) { d += Len("cppdialect(\""); NkString t; while (*d && *d != '"') t += *d++; if (!t.Empty()) return t; }
            if (FindStr(txt, "cppcompiler") || FindStr(txt, "cxxflags") || FindStr(txt, "C++")) return NkString("C++");
            if (FindStr(txt, "python") || FindStr(txt, "Python")) return NkString("Python");
            return NkString("C++");
        }
        static NkString CollectProjects(const char* txt, int32* outCount = nullptr) {
            NkVector<NkString> names;
            auto collect = [&](const char* pat) {
                const char* p = txt;
                while ((p = FindStr(p, pat))) {
                    p += Len(pat);
                    while (*p && *p != '"') ++p;
                    if (*p == '"') { ++p; NkString tok; while (*p && *p != '"') tok += *p++;
                        bool dup = false; for (usize i = 0; i < names.Size(); ++i) if (StrEq(names[i].CStr(), tok.CStr())) { dup = true; break; }
                        if (!dup && !tok.Empty()) names.PushBack(tok); }
                }
            };
            collect("with project(");
            collect("startproject(");
            if (outCount) *outCount = (int32)names.Size();
            NkString out;                                  // noms (jusqu'a 6) ; le total "(N)" est affiche a part
            for (usize i = 0; i < names.Size() && i < 6; ++i) { if (!out.Empty()) out += ", "; out += names[i]; }
            return out;
        }
        // Noms entre guillemets du PREMIER argument de chaque appel `pat...("name"...)`
        // (uniques, jusqu'a 8). Ex. pat = "toolchain(".
        static NkString JoinCallArgs(const char* txt, const char* pat) {
            NkVector<NkString> names; const char* p = txt;
            while ((p = FindStr(p, pat))) {
                p += Len(pat); while (*p && *p != '"' && *p != ')') ++p;
                if (*p == '"') { ++p; NkString t; while (*p && *p != '"') t += *p++;
                    bool dup = false; for (usize i = 0; i < names.Size(); ++i) if (StrEq(names[i].CStr(), t.CStr())) { dup = true; break; }
                    if (!dup && !t.Empty()) names.PushBack(t); }
            }
            NkString out; for (usize i = 0; i < names.Size() && i < 8; ++i) { if (!out.Empty()) out += ", "; out += names[i]; }
            return out;
        }
        // Renvoie (par valeur, duree de vie sure cote appelant) les metadonnees parsees.
        WsMeta WorkspaceMeta(const char* path) {
            for (usize i = 0; i < mWsMeta.Size(); ++i) if (StrEq(mWsMeta[i].path.CStr(), path)) return mWsMeta[i];
            WsMeta m; m.path = path;
            const NkString txt = NkFile::ReadAllText(NkPath(path));
            m.configs   = JoinQuotedInCall(txt.CStr(), "configurations([");
            m.platforms = JoinEnumInCall(txt.CStr(), "targetoses([", "TargetOS.");
            m.langVer   = DetectLang(txt.CStr());
            m.toolchains = JoinCallArgs(txt.CStr(), "toolchain(");
            { const char* v = FindStr(txt.CStr(), "jengaversion(\""); if (v) { v += Len("jengaversion(\""); NkString t; while (*v && *v != '"') t += *v++; m.jengaVer = t; } }
            m.projects  = CollectProjects(txt.CStr(), &m.projCount);
            m.activity  = ActivityTime(NkPath(path).GetParent().ToString().CStr());   // derniere activite reelle
            if (m.activity == 0) m.activity = MTimeOf(path);                            // repli : mtime du .jenga
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
        bool AllProjects() const { return projIdx >= static_cast<int32>(projects.Size()); }

        // `s` commence-t-il par le prefixe `pre` (insensible a la casse) ?
        static bool StartsWithI(const char* s, const char* pre) {
            if (!s || !pre || !*pre) return false;
            for (; *pre; ++s, ++pre) { char a = *s, b = *pre; if (a >= 'A' && a <= 'Z') a += 32; if (b >= 'A' && b <= 'Z') b += 32; if (a != b) return false; }
            return true;
        }
        static usize Len(const char* s) { usize n = 0; if (s) while (s[n]) ++n; return n; }

        // Un test est-il visible pour la selection courante ?
        //  - « Tous les projets » -> tous les tests.
        //  - projet precis -> les tests dont le nom commence par CE projet, et pour
        //    lesquels aucun AUTRE projet n'est un prefixe PLUS LONG (ex. "NKPlatform_Tests"
        //    appartient a "NKPlatform", pas a "NK").
        bool TestVisible(int32 i) const {
            if (i < 0 || i >= static_cast<int32>(tests.Size())) return false;
            if (AllProjects()) return true;
            const char* t = tests[i].CStr();
            const char* sel = SelectedProject();
            if (!StartsWithI(t, sel)) return false;
            const usize selLen = Len(sel);
            for (usize p = 0; p < projects.Size(); ++p)
                if (Len(projects[p].CStr()) > selLen && StartsWithI(t, projects[p].CStr())) return false;
            return true;
        }
        const char* ConfigNameOf(int32 i) const { return i == 1 ? "Release" : "Debug"; }

        // ── File d'attente de commandes jenga (compilation en rafale) ──
        NkVector<NkString> mQueue;
        void EnqueueJenga(const NkString& args) { mQueue.PushBack(NkString("jenga ") + args.CStr()); }
        void PumpQueue() {
            if (mBuild.Running() || mQueue.Empty()) return;
            NkString next = mQueue[0]; mQueue.Erase(mQueue.Begin());
            output.PushBack(NkString("$ ") + next.CStr());
            buildTotal = 0; buildDone = 0;     // progression de cette commande
            mBuild.Start(next);
            status = NkString("Construction...");
        }

        // verb = "build" (Construire) ou "rebuild" (Recompiler de zero).
        void DoBuildAction(const char* verb) {
            if (!HasWorkspace()) { status = NkString("(aucun workspace)"); return; }
            output.Clear(); mQueue.Clear();
            int32 nSys = 0; const SysDef* sys = Systems(&nSys);
            const SysDef& S = sys[(sysIdx >= 0 && sysIdx < nSys) ? sysIdx : 0];
            int32 cfgs[2], nc = 0; if (cfgIdx >= 2) { cfgs[nc++] = 0; cfgs[nc++] = 1; } else cfgs[nc++] = cfgIdx;
            int32 archs[6], na = 0;
            if (archIdx >= S.nArch) { for (int32 i = 0; i < S.nArch; ++i) archs[na++] = i; }
            else archs[na++] = archIdx;
            for (int32 c = 0; c < nc; ++c) for (int32 a = 0; a < na; ++a) {
                NkString cmd(verb);
                if (!AllProjects()) { cmd += " --target "; cmd += SelectedProject(); }
                cmd += " --config "; cmd += ConfigNameOf(cfgs[c]);
                cmd += " --platform "; cmd += S.name; cmd += "-"; cmd += S.archs[archs[a]];
                if (!compilerName.Empty()) { cmd += " --toolchain "; cmd += compilerName; }
                cmd += JengaFileArg();
                EnqueueJenga(cmd);
            }
            PumpQueue();
        }
        void DoClean() {
            if (!HasWorkspace()) { status = NkString("(aucun workspace)"); return; }
            output.Clear(); mQueue.Clear();
            NkString cmd("clean");
            if (!AllProjects()) { cmd += " --project "; cmd += SelectedProject(); }
            cmd += JengaFileArg();
            EnqueueJenga(cmd); PumpQueue();
        }
        void DoRun() {
            if (!HasWorkspace()) { status = NkString("(aucun workspace)"); return; }
            if (AllProjects())  { status = NkString("(choisir un projet pour Demarrer)"); return; }
            output.Clear(); mQueue.Clear();
            int32 nSys = 0; const SysDef* sys = Systems(&nSys);
            const SysDef& S = sys[(sysIdx >= 0 && sysIdx < nSys) ? sysIdx : 0];
            NkString cmd("run "); cmd += SelectedProject();
            cmd += " --config "; cmd += ConfigNameOf(cfgIdx >= 2 ? 0 : cfgIdx);
            cmd += " --platform "; cmd += S.name;
            if (!compilerName.Empty()) { cmd += " --toolchain "; cmd += compilerName; }
            cmd += " --build";
            cmd += JengaFileArg();
            EnqueueJenga(cmd); PumpQueue();
        }
        // Lance les tests : si testIdx == -1 -> TOUS les tests visibles (ceux du
        // projet selectionne, ou tous si « Tous les projets ») ; sinon un test precis.
        // Plusieurs tests -> file d'attente (rafale).
        void DoTest() {
            if (!HasWorkspace() || tests.Empty()) { status = NkString("(aucun test)"); return; }
            output.Clear(); mQueue.Clear();
            int32 ran = 0;
            for (int32 i = 0; i < static_cast<int32>(tests.Size()); ++i) {
                if (!TestVisible(i)) continue;
                if (testIdx >= 0 && i != testIdx) continue;   // un seul test demande
                NkString cmd("test "); cmd += tests[i].CStr();
                cmd += " --config "; cmd += ConfigNameOf(cfgIdx >= 2 ? 0 : cfgIdx);
                cmd += JengaFileArg();
                EnqueueJenga(cmd); ++ran;
            }
            if (ran == 0) { status = NkString("(aucun test pour ce projet)"); return; }
            PumpQueue();
        }

        // Force un re-scan des workspaces + rechargement de `jenga info` (relit le
        // workspace ET tous ses projets inclus). Appele par le bouton Recharger,
        // au changement de workspace, et a l'auto-detection de modifs.
        void RequestReload() {
            mWsScanned = false;       // re-scan des .jenga racine
            mInfoStarted = false;     // force le rechargement de jenga info
            mInfoWsIdx = -1;
            flagsStale = true;        // un .jenga a (peut-être) changé -> régénère le .jcdb (includes/defines par projet)
        }

        // Auto-detection (sur timer) : si un .jenga de la racine a change de date de
        // modification -> recharge. Les modifs faites DANS l'editeur (workspace ou
        // projet inclus) declenchent aussi un reload via SaveActive.
        void TickWatch(float32 dt) {
            mWatchTimer += dt;
            if (mWatchTimer < 1.5f) return;
            mWatchTimer = 0.f;
            // Signature = max(date de modif) des .jenga racine. Si elle augmente -> reload.
            int64 mx = 0;
            NkVector<NkDirectoryEntry> entries = NkDirectory::GetEntries(root, "*.jenga", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
            for (usize i = 0; i < entries.Size(); ++i) {
                if (entries[i].IsDirectory) continue;
                const int64 t = static_cast<int64>(entries[i].ModificationTime);
                if (t > mx) mx = t;
            }
            if (mLastJengaMtime == 0) { mLastJengaMtime = mx; return; }   // 1re mesure
            if (mx > mLastJengaMtime) { mLastJengaMtime = mx; RequestReload(); }
        }

        float32   mWatchTimer = 0.f;   // (struct interne -> helpers statiques accessibles a NkOpenWs)
        int64 mLastJengaMtime = 0;
        static bool EndsWithI(const char* s, const char* suf) {
            usize ls = 0, lf = 0; for (const char* p = s; *p; ++p) ++ls; for (const char* p = suf; *p; ++p) ++lf;
            if (lf > ls) return false; const char* a = s + (ls - lf);
            for (usize i = 0; i < lf; ++i) { char x = a[i], y = suf[i]; if (x >= 'A' && x <= 'Z') x += 32; if (y >= 'A' && y <= 'Z') y += 32; if (x != y) return false; }
            return true;
        }
        // Extrait le nom depuis workspace("NAME" ; sinon nom de fichier sans .jenga.
        static NkString WorkspaceName(const NkString& txt, const NkString& fileName) {
            const char* p = txt.CStr(); const char* k = "workspace(";
            for (; *p; ++p) { const char* a = p; const char* b = k; while (*a && *b && *a == *b) { ++a; ++b; } if (!*b) { p = a; break; } }
            if (*p) { while (*p == ' ' || *p == '\t') ++p; if (*p == '"' || *p == '\'') { char q = *p++; char nm[96]; usize i = 0; while (*p && *p != q && i + 1 < sizeof(nm)) nm[i++] = *p++; nm[i] = '\0'; if (i > 0) return NkString(nm); } }
            NkString s = fileName; const char* d = s.CStr(); usize n = 0; for (const char* z = d; *z; ++z) ++n;
            if (n > 6) { char b[128]; usize i = 0; for (; i < n - 6 && i + 1 < sizeof(b); ++i) b[i] = d[i]; b[i] = '\0'; return NkString(b); }
            return s;
        }


        NkProcess          mInfo;            // `jenga info` (liste des projets)
        NkVector<NkString> mInfoLines;
        bool               mInfoStarted = false;
        bool               mInfoParsed  = false;
        int32              mInfoWsIdx   = -1;   // workspace pour lequel les projets sont charges

        // Parse la table "Projects" de `jenga info` (colonnes Name Kind ...).
        // Les projets de TEST (Kind = TestSuite) vont dans `tests` ; les autres
        // dans `projects`. Exclut les separateurs / entrees parasites (ex. --unitest--).
        void ParseProjects() {
            projects.Clear(); tests.Clear(); toolchains.Clear();
            enum { NONE, PROJ, TOOL } cur = NONE;
            for (usize i = 0; i < mInfoLines.Size(); ++i) {
                const char* L = mInfoLines[i].CStr();
                if (StartsWithI(L, "Configurations:")) { infoConfigs = AfterColon(L); continue; }
                if (StartsWithI(L, "Target OSes:"))    { infoOSes    = AfterColon(L); continue; }
                if (Contains(L, "Name") && Contains(L, "Kind"))   { cur = PROJ; continue; }
                if (Contains(L, "Name") && Contains(L, "Family")) { cur = TOOL; continue; }
                if (L[0] == '=' || L[0] == '-') continue;          // separateurs
                if (IsBlank(L)) { cur = NONE; continue; }          // fin de table (d'autres suivent)
                if (cur == PROJ) {
                    char name[128], kind[64];
                    if (!TwoTokens(L, name, sizeof(name), kind, sizeof(kind))) continue;
                    if (name[0] == '-' || Contains(name, "unitest")) continue;   // parasite / --unitest--
                    if (Contains(kind, "Test")) tests.PushBack(NkString(name));  // TestSuite -> combo Tests
                    else                        projects.PushBack(NkString(name));
                } else if (cur == TOOL) {
                    char t[5][96]; const int32 n = NTokens(L, t, 5, 96);
                    if (n < 4) continue;
                    ToolchainRow r;
                    r.name = t[0]; r.family = t[1]; r.os = t[2]; r.arch = t[3]; r.env = (n >= 5) ? NkString(t[4]) : NkString();
                    toolchains.PushBack(r);
                }
            }
            for (usize i = 0; i < projects.Size(); ++i)        // defaut = NKCode si present
                if (StrEq(projects[i].CStr(), "NKCode")) { projIdx = static_cast<int32>(i); break; }
        }
        // Decoupe jusqu'a `maxN` jetons separes par des espaces/tabs. Renvoie le nombre lu.
        static int32 NTokens(const char* s, char t[][96], int32 maxN, int32 cap) {
            int32 n = 0;
            while (*s && n < maxN) {
                while (*s == ' ' || *s == '\t') ++s;
                if (!*s) break;
                int32 i = 0; while (*s && *s != ' ' && *s != '\t' && i + 1 < cap) t[n][i++] = *s++;
                t[n][i] = '\0'; ++n;
            }
            return n;
        }

        static bool IsBlank(const char* s) { for (; *s; ++s) if (*s != ' ' && *s != '\t') return false; return true; }
        // Partie apres le premier ':' (trim espaces). Retire d'eventuels codes ANSI ESC[...m.
        static NkString AfterColon(const char* s) {
            const char* p = s; while (*p && *p != ':') ++p; if (*p == ':') ++p;
            while (*p == ' ' || *p == '\t') ++p;
            char out[256]; usize n = 0;
            for (; *p && n + 1 < sizeof(out); ++p) {
                if (*p == 0x1b) { while (*p && *p != 'm') ++p; if (!*p) break; continue; }   // saute ESC[...m
                out[n++] = *p;
            }
            while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\r' || out[n - 1] == '\t')) --n;
            out[n] = '\0'; return NkString(out);
        }
        static bool Contains(const char* h, const char* n) {
            for (; *h; ++h) { const char* a = h; const char* b = n; while (*a && *b && *a == *b) { ++a; ++b; } if (!*b) return true; }
            return false;
        }
        // Extrait les 2 premiers jetons separes par des espaces.
        static bool TwoTokens(const char* s, char* t0, usize c0, char* t1, usize c1) {
            while (*s == ' ' || *s == '\t') ++s;
            usize i = 0; while (*s && *s != ' ' && *s != '\t' && i + 1 < c0) t0[i++] = *s++;
            t0[i] = '\0'; if (i == 0) return false;
            while (*s == ' ' || *s == '\t') ++s;
            usize j = 0; while (*s && *s != ' ' && *s != '\t' && j + 1 < c1) t1[j++] = *s++;
            t1[j] = '\0'; return j > 0;
        }
    };

} // namespace nkcode
} // namespace nkentseu
