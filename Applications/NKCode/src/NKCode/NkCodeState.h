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
#include "NKCode/NkProcess.h"

#include <cstring>   // memcpy

namespace nkcode {

    using namespace nkentseu;

    inline bool StrEq(const char* a, const char* b) {
        if (!a || !b) return a == b;
        while (*a && *a == *b) { ++a; ++b; }
        return *a == *b;
    }

    // Un fichier ouvert dans l'editeur : chemin + tampon editable + drapeau modifie.
    struct OpenFile {
        NkPath         path;
        NkVector<char> buffer;          // tampon C editable (contenu + marge de saisie)
        bool           dirty = false;
        NkString Name() const { return path.GetFileName(); }
    };

    struct NkCodeState {
        NkPath             root;        // dossier courant de l'explorateur
        NkVector<OpenFile> files;       // onglets ouverts
        int32              active = -1; // onglet actif
        NkVector<NkString> output;      // sortie de la derniere commande jenga
        NkString           status;      // ligne d'etat (ex. "Build OK")

        NkCodeState() {
            // Racine = dossier de l'app NKCode (lisible) ; l'utilisateur peut remonter.
            root = NkPath::GetCurrentDirectory() / "Applications" / "NKCode";
        }

        // Ouvre `p` dans l'editeur (ou le re-selectionne si deja ouvert).
        void OpenPath(const NkPath& p) {
            const NkString ps = p.ToString();
            for (usize i = 0; i < files.Size(); ++i)
                if (StrEq(files[i].path.ToString().CStr(), ps.CStr())) { active = static_cast<int32>(i); return; }

            const NkString content = NkFile::ReadAllText(p);
            OpenFile f; f.path = p;
            const usize n = content.Size();
            f.buffer.Resize(n + 65536u);          // +64 Ko de marge d'edition
            if (n > 0) std::memcpy(f.buffer.Data(), content.CStr(), n);
            f.buffer.Data()[n] = '\0';
            files.PushBack(f);
            active = static_cast<int32>(files.Size()) - 1;
        }

        bool SaveActive() {
            if (active < 0 || active >= static_cast<int32>(files.Size())) return false;
            OpenFile& f = files[active];
            if (NkFile::WriteAllText(f.path, NkString(f.buffer.Data()))) {
                f.dirty = false; status = NkString("Enregistre : ") + f.Name().CStr();
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
    };

} // namespace nkcode
