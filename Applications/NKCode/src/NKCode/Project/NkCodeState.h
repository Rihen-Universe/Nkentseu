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
#include <cstdlib>
#include <ctime>

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
            OpenPath(NkPath(wsPaths[wsIdx].CStr()));     // ouvre le .jenga du workspace
            RequestReload();                             // recharge la liste des projets
            AddRecent(wsPaths[wsIdx]);                   // memorise dans les recents
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
            char b[48];
            if      (d < 60)        std::snprintf(b, sizeof(b), "a l'instant");
            else if (d < 3600)      std::snprintf(b, sizeof(b), "il y a %d min", (int)(d / 60));
            else if (d < 86400)     std::snprintf(b, sizeof(b), "il y a %d h",   (int)(d / 3600));
            else if (d < 7 * 86400) std::snprintf(b, sizeof(b), "il y a %d j",   (int)(d / 86400));
            else                    std::snprintf(b, sizeof(b), "il y a %d sem", (int)(d / (7 * 86400)));
            return NkString(b);
        }

        // ── Metadonnees d'un workspace NON ouvert (carte du launcher) ─────────────
        // Parse leger du .jenga : configs, plateformes, langage, projets. Mis en
        // cache par chemin (le Home redessine chaque frame -> pas de relecture disque).
        struct WsMeta { NkString path, configs, platforms, projects, langVer; int64 activity = 0; int32 projCount = 0; };
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
        // Renvoie (par valeur, duree de vie sure cote appelant) les metadonnees parsees.
        WsMeta WorkspaceMeta(const char* path) {
            for (usize i = 0; i < mWsMeta.Size(); ++i) if (StrEq(mWsMeta[i].path.CStr(), path)) return mWsMeta[i];
            WsMeta m; m.path = path;
            const NkString txt = NkFile::ReadAllText(NkPath(path));
            m.configs   = JoinQuotedInCall(txt.CStr(), "configurations([");
            m.platforms = JoinEnumInCall(txt.CStr(), "targetoses([", "TargetOS.");
            m.langVer   = DetectLang(txt.CStr());
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
