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
                return true;
            }
            status = NkString("Echec enregistrement"); return false;
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

        // A appeler CHAQUE FRAME : recupere la sortie du build + met a jour le statut.
        void PollBuild() {
            mBuild.Drain(output);
            if (mBuild.Done() && status.Size() > 0 && StrEq(status.CStr(), "Construction..."))
                status = (mBuild.ExitCode() == 0) ? NkString("Termine (OK)") : NkString("Termine (echec)");
        }

        // ── Projets du workspace (un .jenga en contient plusieurs) ───────────────
        NkVector<NkString> projects;     // noms des projets selectionnables
        int32              projIdx = 0;  // projet cible courant

        const char* SelectedProject() const {
            return (projIdx >= 0 && projIdx < static_cast<int32>(projects.Size()))
                 ? projects[projIdx].CStr() : "";
        }

        // Charge la liste des projets via `jenga info` (ASYNCHRONE).
        void LoadProjects() {
            if (mInfoStarted) return;
            mInfoStarted = true; mInfoParsed = false;
            mInfo.Start(NkString("jenga info"));
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

    private:
        NkProcess          mInfo;            // `jenga info` (liste des projets)
        NkVector<NkString> mInfoLines;
        bool               mInfoStarted = false;
        bool               mInfoParsed  = false;

        // Parse la table "Projects" de `jenga info` -> ne garde que les EXECUTABLES
        // (Kind contenant "App") = cibles build/run pertinentes.
        void ParseProjects() {
            projects.Clear();
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
                projects.PushBack(NkString(name));             // TOUS les projets du workspace
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
