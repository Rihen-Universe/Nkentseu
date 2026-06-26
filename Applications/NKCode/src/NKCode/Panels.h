#pragma once
// =============================================================================
// Panels.h — Panneaux de l'IDE NKCode (sur NKEditorKit / NKGui).
//   Explorateur (arbre de fichiers reel) · Editeur (onglets + saisie multi-ligne)
//   · Sortie (resultat de jenga build).
// =============================================================================
#include "NKEditorKit/NkEditorKit.h"
#include "NKCode/NkCodeState.h"

namespace nkcode {

    using namespace nkentseu;
    using namespace nkentseu::editorkit;
    using namespace nkentseu::nkgui;

    // ── Explorateur : navigue le systeme de fichiers ; clic dossier = entrer,
    //    clic fichier = ouvrir dans l'editeur. ──
    class ExplorerPanel : public NkEditorPanel {
    public:
        explicit ExplorerPanel(NkCodeState* s)
            : NkEditorPanel("Explorateur", NkEditorDockSide::NK_LEFT), mS(s) {}
        void OnUI(NkEditorFrameContext& ec) override {
            auto& ctx = ec.Ui();
            ec.Text(mS->root.ToString().CStr());
            ec.Separator();
            if (Selectable(ctx, "..  (dossier parent)", false)) mS->root = mS->root.GetParent();

            NkVector<NkDirectoryEntry> entries =
                NkDirectory::GetEntries(mS->root, "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
            for (usize i = 0; i < entries.Size(); ++i) {
                const NkDirectoryEntry& e = entries[i];
                char lbl[300];
                std::snprintf(lbl, sizeof(lbl), "%s %s", e.IsDirectory ? "[+]" : "   ", e.Name.CStr());
                if (Selectable(ctx, lbl, false)) {
                    if (e.IsDirectory) mS->root = e.FullPath;
                    else               mS->OpenPath(e.FullPath);
                }
            }
        }
    private:
        NkCodeState* mS;
    };

    // ── Editeur : onglets des fichiers ouverts + saisie multi-ligne du fichier actif. ──
    class EditorPanel : public NkEditorPanel {
    public:
        explicit EditorPanel(NkCodeState* s)
            : NkEditorPanel("Editeur", NkEditorDockSide::NK_CENTER), mS(s) {}
        void OnUI(NkEditorFrameContext& ec) override {
            auto& ctx = ec.Ui();
            if (mS->files.Empty()) { ec.Text("Ouvrez un fichier depuis l'Explorateur."); return; }

            // Barre d'onglets (un par fichier ouvert).
            NkVector<NkString>     names;
            NkVector<const char*>  labels;
            for (usize i = 0; i < mS->files.Size(); ++i) names.PushBack(mS->files[i].Name());
            for (usize i = 0; i < names.Size(); ++i)     labels.PushBack(names[i].CStr());
            const int32 sel = TabBar(ctx, "##codetabs", labels.Data(), static_cast<int32>(labels.Size()));
            if (sel >= 0) mS->active = sel;
            if (mS->active < 0 || mS->active >= static_cast<int32>(mS->files.Size())) mS->active = 0;

            OpenFile& f = mS->files[mS->active];
            const NkRect r = { ctx.layout.cursor.x, ctx.layout.cursor.y, ctx.ContentWidth(), ctx.AvailHeight() };
            if (InputTextMultiline(ctx, "##code", f.buffer.Data(), static_cast<int32>(f.buffer.Size()), r))
                f.dirty = true;
        }
    private:
        NkCodeState* mS;
    };

    // ── Sortie : resultat de la derniere commande jenga. ──
    class OutputPanel : public NkEditorPanel {
    public:
        explicit OutputPanel(NkCodeState* s)
            : NkEditorPanel("Sortie", NkEditorDockSide::NK_BOTTOM), mS(s) {}
        void OnUI(NkEditorFrameContext& ec) override {
            if (!mS->status.Empty()) { ec.Text(mS->status.CStr()); ec.Separator(); }
            if (mS->output.Empty()) { ec.Text("(Ctrl+B : construire le projet via jenga)"); return; }
            for (usize i = 0; i < mS->output.Size(); ++i) ec.Text(mS->output[i].CStr());
        }
    private:
        NkCodeState* mS;
    };

} // namespace nkcode
