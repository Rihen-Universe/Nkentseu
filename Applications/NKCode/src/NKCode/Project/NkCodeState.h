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
#include "NKCode/Editor/NkCodeEditor.h"
#include <cstdio>

namespace nkentseu {
namespace nkcode {

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
        NkString Name() const { return path.GetFileName(); }
    };

    struct NkCodeState {
        NkPath             root;        // dossier racine de l'explorateur (arbre)
        NkVector<OpenFile> files;       // onglets ouverts
        int32              active = -1; // onglet actif
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

        // Ouvre `p` dans l'editeur (ou le re-selectionne si deja ouvert).
        void OpenPath(const NkPath& p) {
            const NkString ps = p.ToString();
            for (usize i = 0; i < files.Size(); ++i)
                if (StrEq(files[i].path.ToString().CStr(), ps.CStr())) { active = static_cast<int32>(i); return; }

            const NkString content = NkFile::ReadAllText(p);
            OpenFile f; f.path = p;
            f.doc.SetText(content.CStr());
            files.PushBack(f);
            active = static_cast<int32>(files.Size()) - 1;
        }

        // Ferme l'onglet `i` et reajuste l'onglet actif.
        void CloseFile(int32 i) {
            if (i < 0 || i >= static_cast<int32>(files.Size())) return;
            files.Erase(files.Begin() + i);
            if (active >= static_cast<int32>(files.Size())) active = static_cast<int32>(files.Size()) - 1;
            if (active < 0 && !files.Empty()) active = 0;
        }

        bool SaveActive() {
            if (active < 0 || active >= static_cast<int32>(files.Size())) return false;
            OpenFile& f = files[active];
            if (NkFile::WriteAllText(f.path, f.doc.GetText())) {
                f.doc.dirty = false; status = NkString("Enregistre : ") + f.Name().CStr();
                // Sauvegarde d'un .jenga (workspace OU projet inclus) -> recharge la
                // liste des projets (jenga info relit le workspace + ses includes).
                if (EndsWithI(f.Name().CStr(), ".jenga")) RequestReload();
                return true;
            }
            status = NkString("Echec enregistrement"); return false;
        }

        bool HasActive() const { return active >= 0 && active < static_cast<int32>(files.Size()); }
        bool ActiveHasPath() const { return HasActive() && !files[active].path.ToString().Empty(); }

        // Nouveau fichier sans titre (onglet vide). Reste « sans titre » jusqu'a
        // un Enregistrer sous (path vide -> SaveActive renvoie false).
        void NewFile() {
            OpenFile f;                 // path vide
            f.doc.SetText("");
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

    private:
        float32   mWatchTimer = 0.f;
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
            projects.Clear(); tests.Clear();
            bool inTable = false;
            for (usize i = 0; i < mInfoLines.Size(); ++i) {
                const char* L = mInfoLines[i].CStr();
                if (!inTable) {
                    if (Contains(L, "Name") && Contains(L, "Kind")) inTable = true;
                    continue;
                }
                if (L[0] == '=' || L[0] == '-') continue;     // separateurs
                if (IsBlank(L)) break;                          // fin de table
                char name[128], kind[64];
                if (!TwoTokens(L, name, sizeof(name), kind, sizeof(kind))) continue;
                if (name[0] == '-' || Contains(name, "unitest")) continue;   // parasite / --unitest--
                if (Contains(kind, "Test")) tests.PushBack(NkString(name));  // TestSuite -> combo Tests
                else                        projects.PushBack(NkString(name));
            }
            if (projects.Empty()) return;
            for (usize i = 0; i < projects.Size(); ++i)        // defaut = NKCode si present
                if (StrEq(projects[i].CStr(), "NKCode")) { projIdx = static_cast<int32>(i); break; }
        }

        static bool IsBlank(const char* s) { for (; *s; ++s) if (*s != ' ' && *s != '\t') return false; return true; }
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
