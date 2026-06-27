#pragma once
// =============================================================================
// Panels.h — Panneaux de l'IDE NKCode (sur NKEditorKit / NKGui).
//   Explorateur (arbre de fichiers reel) · Editeur (onglets + saisie multi-ligne)
//   · Sortie (resultat de jenga build).
// =============================================================================
#include "NKEditorKit/NkEditorKit.h"
#include "NKCode/Project/NkCodeState.h"
#include "NKCode/Project/NkLogSink.h"
#include "NKCode/Project/NkPty.h"
#include "NKCode/Project/NkTerm.h"
#include "NKCode/Editor/NkTextDraw.h"

namespace nkcode {

    using namespace nkentseu;
    using namespace nkentseu::editorkit;
    using namespace nkentseu::nkgui;

    // Texte COLORE sur une ligne : reserve un rect de la LARGEUR DU TEXTE (pas
    // pleine largeur, sinon un SameLine() suivant pousse l'item hors champ) et
    // dessine `s` en `col`. Avance le curseur (nouvelle ligne par defaut).
    inline void TermText(NkGuiContext& ctx, const char* s, const NkColor& col) {
        const float32 h = ctx.ItemHeight();
        const float32 w = (ctx.font && ctx.font->Valid() && s) ? ctx.font->MeasureWidth(s) + 4.f : 40.f;
        const NkRect  r = ctx.NextItemRect(w, h);
        if (ctx.font && ctx.font->Valid() && s && *s)
            ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
                             { r.x, r.y + (h - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent() }, s, col);
    }

    // ── Explorateur : ARBRE repliable facon VSCode. Les dossiers s'ouvrent/ferment
    //    en place (chevron + indentation) ; clic fichier = ouvrir dans l'editeur. ──
    class ExplorerPanel : public NkEditorPanel {
    public:
        explicit ExplorerPanel(NkCodeState* s)
            : NkEditorPanel("Explorateur", NkEditorDockSide::NK_LEFT), mS(s) {}
        void OnUI(NkEditorFrameContext& ec) override {
            auto& ctx = ec.Ui();
            // En-tete "EXPLORATEUR" + le PROJET comme NOEUD RACINE repliable (tout
            // l'arbre est imbrique dessous). SCOPE projet : pas de ".." -> jamais hors projet.
            ec.Text("EXPLORATEUR");
            ec.Separator();
            char name[260]; ToUpperInto(mS->root.GetFileName().CStr(), name, sizeof(name));
            char rootLbl[280]; std::snprintf(rootLbl, sizeof(rootLbl), "%s %s", mRootOpen ? "v" : ">", name);
            if (Selectable(ctx, rootLbl, false)) mRootOpen = !mRootOpen;
            if (mRootOpen) DrawTree(ctx, mS->root, 1);
        }
    private:
        // Dessine recursivement l'arbre du dossier `dir` a la profondeur `depth`.
        void DrawTree(NkGuiContext& ctx, const NkPath& dir, int32 depth) {
            if (depth > 24) return;                    // garde-fou
            NkVector<NkDirectoryEntry> entries =
                NkDirectory::GetEntries(dir, "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
            for (int pass = 0; pass < 2; ++pass) {     // dossiers d'abord, puis fichiers
                const bool wantDir = (pass == 0);
                for (usize i = 0; i < entries.Size(); ++i) {
                    const NkDirectoryEntry& e = entries[i];
                    if (e.IsDirectory != wantDir) continue;
                    char lbl[340]; int32 n = 0;
                    for (int32 d = 0; d < depth; ++d) { lbl[n++] = ' '; lbl[n++] = ' '; }  // indentation
                    if (e.IsDirectory) {
                        const bool exp = IsExpanded(e.FullPath);
                        lbl[n++] = exp ? 'v' : '>'; lbl[n++] = ' ';       // chevron
                        std::snprintf(lbl + n, sizeof(lbl) - n, "%s", e.Name.CStr());
                        if (Selectable(ctx, lbl, false)) ToggleExpanded(e.FullPath);
                        if (IsExpanded(e.FullPath)) DrawTree(ctx, e.FullPath, depth + 1);
                    } else {
                        lbl[n++] = ' '; lbl[n++] = ' ';                   // aligne sous le chevron
                        std::snprintf(lbl + n, sizeof(lbl) - n, "%s", e.Name.CStr());
                        if (Selectable(ctx, lbl, false)) mS->OpenPath(e.FullPath);
                    }
                }
            }
        }
        bool IsExpanded(const NkPath& p) const {
            const NkString s = p.ToString();
            for (usize i = 0; i < mExpanded.Size(); ++i)
                if (StrEq(mExpanded[i].CStr(), s.CStr())) return true;
            return false;
        }
        void ToggleExpanded(const NkPath& p) {
            const NkString s = p.ToString();
            for (usize i = 0; i < mExpanded.Size(); ++i)
                if (StrEq(mExpanded[i].CStr(), s.CStr())) { mExpanded.Erase(mExpanded.Begin() + i); return; }
            mExpanded.PushBack(s);
        }
        static void ToUpperInto(const char* s, char* dst, usize cap) {
            usize i = 0;
            if (s) for (; s[i] && i + 1 < cap; ++i) {
                char c = s[i];
                dst[i] = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : c;
            }
            dst[i] = '\0';
        }
        NkCodeState*       mS;
        NkVector<NkString> mExpanded;   // dossiers deplies (chemins) — etat persistant
        bool               mRootOpen = true;   // noeud racine (projet) deplie par defaut
    };

    // ── Editeur : onglets des fichiers ouverts + saisie multi-ligne du fichier actif. ──
    class EditorPanel : public NkEditorPanel {
    public:
        EditorPanel(NkCodeState* s, NkEditorShell* shell)
            : NkEditorPanel("Editeur", NkEditorDockSide::NK_CENTER), mS(s), mShell(shell) {}
        void OnUI(NkEditorFrameContext& ec) override {
            auto& ctx = ec.Ui();
            if (mS->files.Empty()) {
                if (mShell) mShell->SetFooter("NKCode", "Jenga");
                ec.Text("Ouvrez un fichier depuis l'Explorateur.");
                return;
            }

            // Bandeau d'onglets de fichiers CUSTOM (pilote par mS->active) : onglet
            // actif surligne, point "modifie", bouton X. Remplace le TabBar NKGui
            // (qui gardait son propre index et empechait la revelation au clic).
            DrawFileTabs(ctx);
            if (mS->active < 0 || mS->active >= static_cast<int32>(mS->files.Size())) mS->active = 0;
            if (mS->files.Empty()) return;

            OpenFile& f = mS->files[mS->active];
            // AvailHeight()/ContentWidth() = taille du CONTENU (scrollable, ~1e9), PAS
            // la taille visible -> on borne par le rect de CLIP (zone visible du dock)
            // sinon viewH gigantesque (pas de scrollbar, barre H hors ecran).
            const NkRect clip = ctx.DL().CurrentClip();
            NkRect r = { ctx.layout.cursor.x, ctx.layout.cursor.y, ctx.ContentWidth(), ctx.AvailHeight() };
            if (r.x + r.w > clip.x + clip.w) r.w = clip.x + clip.w - r.x;
            if (r.y + r.h > clip.y + clip.h) r.h = clip.y + clip.h - r.y;
            CodeEditor(ctx, "##code", f.doc, r, NkLangFromExt(f.path.GetExtension().CStr()));

            // Footer VSCode : nom du fichier (gauche) + Ln/Col + langage (droite).
            if (mShell) {
                char rbuf[128];
                std::snprintf(rbuf, sizeof(rbuf), "Ln %d, Col %d    %s",
                              f.doc.curLine + 1, f.doc.curCol + 1, LangOf(f.path));
                NkString left = f.Name();
                if (f.doc.dirty) left = NkString("* ") + left.CStr();
                mShell->SetFooter(left.CStr(), rbuf);

                // Infos centrees dans la barre de titre : "fichier - NKCode".
                char center[200];
                std::snprintf(center, sizeof(center), "%s%s - NKCode",
                              f.doc.dirty ? "* " : "", f.Name().CStr());
                mShell->SetTitleInfo(center);
            }
        }
    private:
        // Bandeau d'onglets de fichiers (facon VSCode) : dessine chaque fichier
        // ouvert, gere clic (activer) + X (fermer), puis avance le curseur de layout
        // sous le bandeau. Pilote par mS->active (source de verite).
        void DrawFileTabs(NkGuiContext& ctx) {
            const float32 h  = ctx.ItemHeight();
            const float32 x0 = ctx.layout.cursor.x, y0 = ctx.layout.cursor.y;
            const float32 fullW = ctx.ContentWidth();
            auto& dl = ctx.DL();
            dl.AddRectFilled({ x0, y0, fullW, h }, ctx.theme.tabBar);
            const NkVec2 m = ctx.input.mousePos;
            float32 x = x0; int32 toClose = -1;
            for (usize i = 0; i < mS->files.Size(); ++i) {
                OpenFile& f = mS->files[i];
                const NkString nm = f.Name();
                const float32 nameW = (ctx.font && ctx.font->Valid()) ? ctx.font->MeasureWidth(nm.CStr()) : 40.f;
                const float32 dotW = 16.f;
                const float32 tabW = nameW + 14.f + dotW + 6.f;
                const NkRect  tab  = { x, y0, tabW, h };
                const bool active = (static_cast<int32>(i) == mS->active);
                const bool hov = m.x >= tab.x && m.x < tab.x + tab.w && m.y >= tab.y && m.y < tab.y + tab.h;
                dl.AddRectFilled(tab, active ? ctx.theme.tabActive : (hov ? ctx.theme.tabHover : ctx.theme.tab));
                if (active) dl.AddRectFilled({ tab.x, y0 + h - 2.f, tab.w, 2.f }, ctx.theme.accent);
                if (ctx.font && ctx.font->Valid())
                    dl.AddText(ctx.font->Face(), ctx.font->TexId(),
                               { tab.x + 8.f, y0 + (h - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent() },
                               nm.CStr(), active ? ctx.theme.text : ctx.theme.textDisabled, nameW);
                // Zone droite : point "modifie" (si dirty et non survole) sinon X.
                const NkRect cl = { tab.x + tabW - dotW - 5.f, y0 + (h - dotW) * 0.5f, dotW, dotW };
                const bool clHov = m.x >= cl.x && m.x < cl.x + cl.w && m.y >= cl.y && m.y < cl.y + cl.h;
                if (f.doc.dirty && !clHov) {
                    dl.AddCircleFilled({ cl.x + cl.w * 0.5f, cl.y + cl.h * 0.5f }, 4.f, ctx.theme.text);
                } else {
                    if (clHov) dl.AddRectFilled(cl, ctx.theme.buttonHover);
                    const float32 cx = cl.x + cl.w * 0.5f, cy = cl.y + cl.h * 0.5f, a = 3.5f;
                    dl.AddLine({ cx - a, cy - a }, { cx + a, cy + a }, ctx.theme.text, 1.2f);
                    dl.AddLine({ cx - a, cy + a }, { cx + a, cy - a }, ctx.theme.text, 1.2f);
                }
                if (ctx.input.mouseClicked[0] && hov) { if (clHov) toClose = static_cast<int32>(i); else mS->active = static_cast<int32>(i); }
                dl.AddRectFilled({ tab.x + tabW - 1.f, y0, 1.f, h }, ctx.theme.border);
                x += tabW;
            }
            // Avance le curseur de layout SOUS le bandeau (l'editeur suit dessous).
            ctx.layout.cursor.x   = x0;
            ctx.layout.cursor.y   = y0 + h;
            ctx.layout.lineStartX = x0;
            ctx.layout.curLineH   = 0.f;
            if (toClose >= 0) mS->CloseFile(toClose);
        }
        // Langage devine a partir de l'extension (affiche dans le footer).
        static const char* LangOf(const NkPath& p) {
            const NkString e = p.GetExtension();
            const char* x = e.CStr();
            if (StrEq(x, ".cpp") || StrEq(x, ".cc") || StrEq(x, ".cxx") || StrEq(x, ".h") || StrEq(x, ".hpp")) return "C++";
            if (StrEq(x, ".c"))    return "C";
            if (StrEq(x, ".py"))   return "Python";
            if (StrEq(x, ".jenga"))return "Jenga";
            if (StrEq(x, ".md"))   return "Markdown";
            if (StrEq(x, ".json")) return "JSON";
            if (StrEq(x, ".txt"))  return "Texte";
            return "Texte";
        }
        NkCodeState*   mS;
        NkEditorShell* mShell;
    };

    // ── OUTPUT : VRAI affichage NKLogger (logs du moteur) + sortie du build jenga.
    //    Draine le tampon de logs partage, colore par niveau, suit le bas. ──
    class OutputPanel : public NkEditorPanel {
    public:
        explicit OutputPanel(NkCodeState* s)
            : NkEditorPanel("OUTPUT", NkEditorDockSide::NK_BOTTOM), mS(s) {}
        void OnUI(NkEditorFrameContext& ec) override {
            auto& ctx = ec.Ui();
            mS->PollBuild();                      // tient le statut de build a jour
            GlobalLogBuffer().Drain(mLogs);       // recupere les nouveaux logs NKLogger
            for (usize i = 0; i < mS->output.Size(); ++i) mLogs.PushBack(mS->output[i]);  // + sortie build
            mS->output.Clear();
            while (mLogs.Size() > 5000) mLogs.Erase(mLogs.Begin());   // borne

            ctx.DL().AddRectFilled(ctx.DL().CurrentClip(), NkColor{ 13, 17, 23, 255 });   // fond #0D1117
            if (mLogs.Empty()) { ec.Text("(logs NKLogger — le moteur ecrit ici)"); return; }

            // Affiche la QUEUE (dernieres lignes qui tiennent dans la zone visible).
            const NkRect  clip  = ctx.DL().CurrentClip();
            const float32 lineH = (ctx.font && ctx.font->Valid()) ? ctx.font->LineHeight() : 16.f;
            const int32   fit   = (lineH > 1.f) ? static_cast<int32>(clip.h / lineH) : 20;
            int32 start = static_cast<int32>(mLogs.Size()) - fit; if (start < 0) start = 0;
            for (int32 i = start; i < static_cast<int32>(mLogs.Size()); ++i)
                TermText(ctx, mLogs[i].CStr(), LogColor(mLogs[i].CStr()));
        }
    private:
        // Couleur par niveau (prefixe "[LEVEL]").
        static NkColor LogColor(const char* s) {
            if (s[0] == '[') {
                const char* l = s + 1;
                if (l[0] == 'E' || l[0] == 'C' || l[0] == 'F') return { 248,  81,  73, 255 };  // Error/Crit/Fatal
                if (l[0] == 'W')                                return { 210, 153,  34, 255 };  // Warn
                if (l[0] == 'D' || l[0] == 'T')                 return { 110, 118, 129, 255 };  // Debug/Trace
            }
            return { 204, 204, 204, 255 };   // Info/defaut
        }
        NkCodeState*       mS;
        NkVector<NkString> mLogs;
    };

    // ── Terminal MULTI-SHELL facon VSCode : plusieurs terminaux internes
    //    (PowerShell / WSL / cmd / Jenga), onglets + bouton "+" + selecteur de
    //    shell. Chaque commande est routee vers le shell de l'onglet. Fond noir,
    //    invite coloree, execution ASYNC -> sortie en flux. ──
    class TerminalPanel : public NkEditorPanel {
    public:
        enum Shell { SH_PWSH = 0, SH_WSL, SH_BASH, SH_CMD, SH_JENGA, SH_COUNT };

        // Entree du selecteur de shell : un type (kind) + un libelle + une distro
        // WSL optionnelle. La liste est construite dynamiquement (distros WSL2 reelles).
        struct ShellDef { int32 kind; NkString label; NkString distro; };

        TerminalPanel() : NkEditorPanel("TERMINAL", NkEditorDockSide::NK_BOTTOM) {
            mTerm[0].alive = true;   // un terminal PowerShell par defaut
            mTerm[0].label = "powershell";
        }

        void OnUI(NkEditorFrameContext& ec) override {
            auto& ctx = ec.Ui();
            auto& dl  = ctx.DL();
            if (!mTerm[mActive].alive) mActive = FirstAlive();
            Term& t = mTerm[mActive];

            const NkRect clip = dl.CurrentClip();
            dl.AddRectFilled(clip, NkColor{ 13, 17, 23, 255 });    // fond terminal #0D1117

            // Disposition VSCode : terminal a GAUCHE, LISTE des terminaux a DROITE.
            const float32 listW = ctx.S(190.f);
            const NkRect  mainR = { clip.x, clip.y, clip.w - listW, clip.h };
            const NkRect  listR = { clip.x + clip.w - listW, clip.y, listW, clip.h };
            DrawTermList(ctx, listR);

            // A partir d'ici : police MONOSPACE (grille du terminal).
            NkCodeFontScope _cfs(ctx);

            // Lance le shell (ConPTY) au premier affichage de cet onglet.
            StartTerm(t);
            // Recupere la sortie brute et la passe a l'emulateur VT.
            mDrain.Clear(); t.pty.Drain(mDrain);
            if (mDrain.Size() > 0) t.screen.Feed(mDrain.Data(), mDrain.Size());

            const NkVec2 m = ctx.input.mousePos;
            const bool inMain = m.x >= mainR.x && m.x < mainR.x + mainR.w && m.y >= mainR.y && m.y < mainR.y + mainR.h;
            const bool inClip = m.x >= clip.x && m.x < clip.x + clip.w && m.y >= clip.y && m.y < clip.y + clip.h;

            // Focus clavier : clic gauche dans la zone -> focus ; clic hors panneau -> defocus.
            if (ctx.input.mouseClicked[0] && ctx.popupDepth == 0) {
                if (inMain) { mFocused = true; NkCodeFocusId() = NKGUI_ID_NONE; }
                else if (!inClip) mFocused = false;
            }
            // Clic droit dans la zone -> menu contextuel Copier/Coller.
            if (ctx.input.mouseClicked[2] && inMain && ctx.popupDepth == 0) { mMenu.open = true; mMenu.pos = m; mFocused = true; }

            const float32 lineH = (ctx.font && ctx.font->Valid()) ? ctx.font->LineHeight() : 16.f;
            const float32 pad   = 6.f;
            if (mainR.h > lineH) DrawGrid(ctx, t, mainR, lineH, pad);

            // ── Clavier : frappes routees vers le pty (pas de boite de saisie) ──
            if (mFocused && !mMenu.open && ctx.popupDepth == 0) RouteKeyboard(ctx, t);

            // ── Menu contextuel (overlay) ──
            const char* items[] = { "Copier", "Coller", "Tout selectionner" };
            const bool  en[]    = { t.HasSel(), true, true };
            const int32 act = NkCtxMenuDraw(ctx, mMenu, items, en, 3);
            if (act == 0) CopySelection(ctx, t);
            else if (act == 1) PasteClipboard(ctx, t);
            else if (act == 2) SelectAll(t);
        }

        // Actions sur la BARRE D'ONGLETS (a droite) quand TERMINAL est l'onglet actif :
        // bouton "+" + combobox de shell. Apparait sur la meme ligne que OUTPUT/TERMINAL.
        void OnTabBarActions(NkGuiContext& ctx, const NkRect& bar) noexcept override {
            auto& dl = ctx.DL();
            const float32 h = bar.h;
            const NkVec2 m = ctx.input.mousePos;
            auto inR = [&](const NkRect& r) { return m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h; };
            // Bouton "+" (a l'extreme droite).
            const NkRect addR = { bar.x + bar.w - h - 2.f, bar.y + 1.f, h, h - 2.f };
            { const bool hov = inR(addR); if (hov) dl.AddRectFilled(addR, ctx.theme.buttonHover);
              const float32 cx = addR.x + h * 0.5f, cy = addR.y + (h - 2.f) * 0.5f, a = 5.f;
              dl.AddRectFilled({ cx - a, cy - 1.f, 2.f * a, 2.f }, ctx.theme.text);
              dl.AddRectFilled({ cx - 1.f, cy - a, 2.f, 2.f * a }, ctx.theme.text);
              if (hov && ctx.input.mouseClicked[0] && ctx.popupDepth == 0) AddTerm(mNewShell); }
            // Combobox de shell (a gauche du "+").
            const float32 cw = ctx.S(118.f);
            const float32 savedW = ctx.layout.region.w;
            ctx.layout.cursor     = { addR.x - cw - 4.f, bar.y + 1.f };
            ctx.layout.lineStartX = ctx.layout.cursor.x; ctx.layout.curLineH = 0.f;
            ctx.layout.region.w   = (ctx.layout.cursor.x - ctx.layout.region.x) + cw;
            EnsureBaseShells();
            if (mNewShell < 0 || mNewShell >= static_cast<int32>(mShells.Size())) mNewShell = 0;
            ctx.PushId("shellhdr");
            if (BeginCombo(ctx, "", mShells[mNewShell].label.CStr(), static_cast<int32>(mShells.Size()))) {
                DetectWslDistros();   // ajoute les distros WSL2 reelles (une seule fois)
                for (int32 i = 0; i < static_cast<int32>(mShells.Size()); ++i)
                    if (Selectable(ctx, mShells[i].label.CStr(), i == ctx.comboNav) || (i == ctx.comboNav && ctx.comboEnter)) { mNewShell = i; ctx.ClosePopup(); }
                EndCombo(ctx);
            }
            ctx.PopId();
            ctx.layout.region.w = savedW;
        }

    private:
        struct Term {
            NkPty    pty;                      // shell interactif (ConPTY)
            NkTerm   screen;                   // emulateur VT (grille de cellules)
            int32    shell = SH_PWSH;
            NkString distro;                   // distro WSL ciblee (si shell == SH_WSL)
            NkString label = "powershell";     // libelle affiche (onglet/liste)
            bool     alive = false;
            bool     started = false;          // pty deja lance ?
            float32  scrollX = 0.f, scrollY = 0.f;
            bool     follow = true;            // colle au bas (desactive au scroll manuel)
            // Selection en cellules : ancre (A) + curseur (B), en (ligne ABSOLUE, colonne).
            int32    sAL = 0, sAC = 0, sBL = 0, sBC = 0;
            bool     dragging = false;
            bool HasSel() const { return sAL != sBL || sAC != sBC; }
        };

        // ── Lance le shell interactif (ConPTY) pour ce terminal, une seule fois. ──
        void StartTerm(Term& t) {
            if (t.started) return;
            t.started = true;
            t.pty.Start(PtyCommand(t.shell, t.distro), t.screen.Cols(), t.screen.Rows());
        }

        // Programme reel a lancer pour chaque type de shell.
        static NkString PtyCommand(int32 s, const NkString& distro) {
            switch (s) {
                case SH_PWSH:  return NkString("powershell.exe -NoLogo");
                case SH_WSL:   return distro.Empty() ? NkString("wsl.exe")
                                                     : (NkString("wsl.exe -d ") + distro);
                case SH_BASH:  return NkString("bash.exe");
                case SH_JENGA: return NkString("powershell.exe -NoLogo");   // jenga s'utilise dans powershell
                default:       return NkString("cmd.exe");
            }
        }

        // ── Grille du terminal : rend les cellules visibles + curseur + selection +
        //    scrollbars V/H avec fleches + auto-suivi du bas (vrai terminal). ──
        void DrawGrid(NkGuiContext& ctx, Term& t, const NkRect& out, float32 lineH, float32 pad) {
            auto& dl = ctx.DL();
            const NkColor kTrk = { 25, 29, 35, 255 }, kThb = { 72, 79, 87, 200 }, kThbH = { 110, 118, 129, 235 };
            const float32 sbW = 14.f;
            const NkFont* face = (ctx.font && ctx.font->Valid()) ? ctx.font->Face() : nullptr;
            const float32 cellW = face ? face->CalcTextSizeX("M") : 8.f;
            const float32 cw    = cellW > 1.f ? cellW : 8.f;
            const float32 viewW = out.w - sbW - pad * 2.f;
            const float32 viewH = out.h - sbW;
            const float32 left  = out.x + pad;
            const NkVec2  m = ctx.input.mousePos;
            auto in = [&](const NkRect& r) { return m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h; };

            // Recale la taille de la grille (+ le pty) sur la zone visible.
            int16 cols = static_cast<int16>(viewW / cw); if (cols < 1) cols = 1; if (cols > 500) cols = 500;
            int16 rows = static_cast<int16>(viewH / lineH); if (rows < 1) rows = 1; if (rows > 300) rows = 300;
            if (t.started && (cols != t.screen.Cols() || rows != t.screen.Rows())) { t.screen.Resize(cols, rows); t.pty.Resize(cols, rows); }

            const float32 topPad = lineH;
            const int32   total = static_cast<int32>(t.screen.TotalLines());
            const float32 contentH = total * lineH + topPad;
            // « Coller au bas » = afficher l'ECRAN (les rows dernieres lignes) epingle.
            // On ne defile QUE dans le scrollback : borne basse = followY. Pas de marge
            // basse over-scrollable -> evite le va-et-vient (clignotement) au scroll bas.
            float32 followY = static_cast<float32>(total - t.screen.Rows()) * lineH;
            if (followY < 0.f) followY = 0.f;
            const float32 maxSY = followY;   // on ne descend pas en dessous de l'ecran
            const float32 maxSX = 0.f;       // contenu cale sur cols -> pas de defilement H

            if (in(out)) {
                if (ctx.input.wheel != 0.f) { t.scrollY -= ctx.input.wheel * lineH * 3.f; ctx.input.wheel = 0.f; t.follow = false; }
            }
            if (t.follow) t.scrollY = followY;
            if (t.scrollY < 0.f) t.scrollY = 0.f; if (t.scrollY > maxSY) t.scrollY = maxSY;
            t.scrollX = 0.f;

            // ── Selection souris (cellules) ──
            const NkRect selArea = { out.x, out.y, out.w - sbW, viewH };
            auto rowAtY = [&](float32 y) -> int32 { int32 L = static_cast<int32>((y - out.y - topPad + t.scrollY) / lineH); if (L < 0) L = 0; if (L >= total) L = total - 1; return L; };
            auto colAtX = [&](float32 x) -> int32 { int32 c = static_cast<int32>((x - left) / cw + 0.5f); if (c < 0) c = 0; return c; };
            if (ctx.input.mouseClicked[0] && in(selArea) && ctx.popupDepth == 0 && !mMenu.open) {
                const int32 L = rowAtY(m.y); t.sAL = t.sBL = L; t.sAC = t.sBC = colAtX(m.x); t.dragging = true;
            }
            if (t.dragging && ctx.input.mouseDown[0]) { t.sBL = rowAtY(m.y); t.sBC = colAtX(m.x); }
            if (!ctx.input.mouseDown[0]) t.dragging = false;
            // Selection normalisee (aL,aC) <= (bL,bC).
            int32 nAL = t.sAL, nAC = t.sAC, nBL = t.sBL, nBC = t.sBC;
            if (nAL > nBL || (nAL == nBL && nAC > nBC)) { int32 tl = nAL, tc = nAC; nAL = nBL; nAC = nBC; nBL = tl; nBC = tc; }
            // Ctrl+C : copie si selection, sinon laisse RouteKeyboard envoyer SIGINT.
            if (ctx.input.wantCopy && t.HasSel()) CopySelection(ctx, t);

            // ── Rendu des cellules ──
            const NkRect txtClip = { out.x, out.y, out.w - sbW, viewH };
            dl.PushClipRect(txtClip, true);
            int32 first = static_cast<int32>((t.scrollY - topPad) / lineH); if (first < 0) first = 0;
            const int32 last = first + static_cast<int32>(viewH / lineH) + 2;
            const float32 asc = ctx.font ? ctx.font->Ascent() : 12.f;
            for (int32 i = first; i <= last && i < total; ++i) {
                if (i < 0) continue;
                const float32 ytop = out.y + topPad + i * lineH - t.scrollY;
                const NkTerm::Line& ln = t.screen.LineAt(static_cast<usize>(i));
                // Surlignage de selection (en colonnes de cellules).
                if (t.HasSel() && i >= nAL && i <= nBL) {
                    const int32 c0 = (i == nAL) ? nAC : 0;
                    const int32 c1 = (i == nBL) ? nBC : cols;
                    if (c1 > c0) dl.AddRectFilled({ left + c0 * cw, ytop, (c1 - c0) * cw, lineH }, NkColor{ 31, 111, 235, 90 });
                }
                const int32 ncell = static_cast<int32>(ln.Size());
                for (int32 c = 0; c < ncell; ++c) {
                    const NkTermCell& cell = ln[c];
                    const float32 x = left + c * cw;
                    if (x >= out.x + out.w - sbW) break;
                    if (cell.bg.a != 0) dl.AddRectFilled({ x, ytop, cw + 0.5f, lineH }, cell.bg);
                    if (cell.cp != 0x20 && cell.cp != 0 && face) {
                        char u8[5]; const int32 n = NkEncodeU8(cell.cp, u8);
                        NkDrawTextU(ctx, x, ytop + asc, ytop, lineH, u8, u8 + n, cell.fg);
                    }
                }
            }
            // Curseur (bloc) si focus.
            if (mFocused && t.screen.CursorVisible()) {
                const int32 cl = static_cast<int32>(t.screen.CursorLine());
                const int32 cc = t.screen.CursorCol();
                const float32 cx = left + cc * cw;
                const float32 cy = out.y + topPad + cl * lineH - t.scrollY;
                dl.AddRectFilled({ cx, cy, cw, lineH }, NkColor{ 223, 223, 223, 150 });
            }
            dl.PopClipRect();

            // ── Scrollbars V + H avec fleches ──
            auto arrow = [&](const NkRect& r, int32 dir) -> bool {
                const bool h = in(r); if (h) dl.AddRectFilled(r, NkColor{ 33, 39, 48, 255 });
                const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f, a = 3.2f; const NkColor c = h ? kThbH : kThb;
                if (dir == 0) dl.AddTriangleFilled({ cx, cy - a }, { cx - a, cy + a }, { cx + a, cy + a }, c);
                else if (dir == 1) dl.AddTriangleFilled({ cx - a, cy - a }, { cx + a, cy - a }, { cx, cy + a }, c);
                else if (dir == 2) dl.AddTriangleFilled({ cx - a, cy }, { cx + a, cy - a }, { cx + a, cy + a }, c);
                else dl.AddTriangleFilled({ cx - a, cy - a }, { cx + a, cy }, { cx - a, cy + a }, c);
                return h && ctx.input.mouseDown[0];
            };
            const NkRect vT = { out.x + out.w - sbW, out.y, sbW, viewH };
            const NkRect hT = { out.x, out.y + viewH, out.w - sbW, sbW };
            dl.AddRectFilled(vT, kTrk); dl.AddRectFilled(hT, kTrk);
            dl.AddRectFilled({ vT.x, hT.y, sbW, sbW }, kTrk);
            { const NkRect up = { vT.x, vT.y, sbW, sbW }, dn = { vT.x, vT.y + viewH - sbW, sbW, sbW };
              const NkRect iv = { vT.x, vT.y + sbW, sbW, viewH - 2.f * sbW };
              if (arrow(up, 0)) { t.scrollY -= lineH * 0.8f; t.follow = false; } if (arrow(dn, 1)) t.scrollY += lineH * 0.8f;
              if (maxSY > 0.f && iv.h > 8.f) {
                  float32 th = iv.h * (viewH / contentH); if (th < 24.f) th = 24.f; if (th > iv.h) th = iv.h;
                  const float32 ty = iv.y + (t.scrollY / maxSY) * (iv.h - th);
                  if (ctx.input.mouseClicked[0] && in(iv)) ctx.activeId = ctx.GetId("##tvbar");
                  const bool actv = (ctx.activeId == ctx.GetId("##tvbar"));
                  if (actv && ctx.input.mouseDown[0]) { const float32 u = (m.y - iv.y - th * 0.5f) / (iv.h - th); t.scrollY = (u < 0 ? 0 : u > 1 ? 1 : u) * maxSY; t.follow = false; }
                  dl.AddRectFilled({ iv.x + 3.f, ty, sbW - 6.f, th }, (actv || in(iv)) ? kThbH : kThb, 3.f);
              } }
            { const NkRect lf = { hT.x, hT.y, sbW, sbW }, rt = { hT.x + hT.w - sbW, hT.y, sbW, sbW };
              const NkRect ih = { hT.x + sbW, hT.y, hT.w - 2.f * sbW, sbW };
              arrow(lf, 2); arrow(rt, 3);
              dl.AddRectFilled({ ih.x + 3.f, hT.y + 3.f, ih.w - 6.f, sbW - 6.f }, kThb, 3.f);   // H inactif (contenu cale)
            }
            if (t.scrollY < 0.f) t.scrollY = 0.f; if (t.scrollY > maxSY) t.scrollY = maxSY;
            if (t.scrollY >= followY - 1.f) t.follow = true;   // revenu au bas -> re-suit le flux
        }

        // ── Clavier : route les frappes vers l'entree du pty (UTF-8 + sequences VT). ──
        void RouteKeyboard(NkGuiContext& ctx, Term& t) {
            NkVector<char> seq;
            auto put = [&](const char* s) { for (; *s; ++s) seq.PushBack(*s); };
            // Caracteres tapes (hors touches d'edition + hors Ctrl-C/A/V/X geres en flags).
            for (int32 i = 0; i < ctx.input.charCount; ++i) {
                const uint32 cp = ctx.input.chars[i];
                if (cp == 9) { seq.PushBack('\t'); continue; }
                if (cp == 10 || cp == 13 || cp == 8 || cp == 127) continue;    // touches dediees
                if (cp < 32) { if (cp == 3 || cp == 1 || cp == 22 || cp == 24) continue; seq.PushBack(static_cast<char>(cp)); continue; }  // Ctrl+lettre
                char u8[5]; const int32 n = NkEncodeU8(cp, u8); for (int32 k = 0; k < n; ++k) seq.PushBack(u8[k]);
            }
            // Touches d'edition -> sequences.
            auto K = [&](NkGuiKey k) { return ctx.input.KeyPressedRepeat(k); };
            if (K(NkGuiKey::Enter))     put("\r");
            if (K(NkGuiKey::Backspace)) put("\x7f");
            if (K(NkGuiKey::Delete))    put("\x1b[3~");
            if (K(NkGuiKey::Up))        put("\x1b[A");
            if (K(NkGuiKey::Down))      put("\x1b[B");
            if (K(NkGuiKey::Right))     put("\x1b[C");
            if (K(NkGuiKey::Left))      put("\x1b[D");
            if (K(NkGuiKey::Home))      put("\x1b[H");
            if (K(NkGuiKey::End))       put("\x1b[F");
            if (ctx.input.KeyPressed(NkGuiKey::Escape)) put("\x1b");
            // Raccourcis : coller / copier (->SIGINT si pas de selection) / tout selectionner.
            if (ctx.input.wantPaste)     PasteClipboard(ctx, t);
            if (ctx.input.wantSelectAll) SelectAll(t);
            if (ctx.input.wantCopy && !t.HasSel()) put("\x03");   // Ctrl+C = interruption
            if (seq.Size() > 0) { t.scrollY = 1.0e9f; t.follow = true; t.pty.Write(seq.Data(), seq.Size()); }
        }

        // Texte de la selection (cellules -> UTF-8), espaces de fin retires par ligne.
        void CopySelection(NkGuiContext& ctx, Term& t) {
            int32 aL = t.sAL, aC = t.sAC, bL = t.sBL, bC = t.sBC;
            if (aL > bL || (aL == bL && aC > bC)) { int32 tl = aL, tc = aC; aL = bL; aC = bC; bL = tl; bC = tc; }
            const int32 total = static_cast<int32>(t.screen.TotalLines());
            NkVector<char> buf;
            for (int32 L = aL; L <= bL && L < total; ++L) {
                if (L < 0) continue;
                const NkTerm::Line& ln = t.screen.LineAt(static_cast<usize>(L));
                const int32 ncell = static_cast<int32>(ln.Size());
                const int32 c0 = (L == aL) ? aC : 0;
                int32 c1 = (L == bL) ? bC : ncell; if (c1 > ncell) c1 = ncell;
                int32 end = c1; while (end > c0 && (ln[end - 1].cp == 0x20 || ln[end - 1].cp == 0)) --end;   // trim fin
                for (int32 c = (c0 < 0 ? 0 : c0); c < end; ++c) {
                    char u8[5]; const int32 n = NkEncodeU8(ln[c].cp ? ln[c].cp : 0x20, u8);
                    for (int32 k = 0; k < n; ++k) buf.PushBack(u8[k]);
                }
                if (L < bL) buf.PushBack('\n');
            }
            buf.PushBack('\0');
            if (buf.Size() > 1) ctx.SetClipboard(buf.Data());
        }

        void PasteClipboard(NkGuiContext& ctx, Term& t) {
            const NkString clip = ctx.GetClipboard();
            if (!clip.Empty()) t.pty.Write(clip.CStr(), clip.Size());
        }
        void SelectAll(Term& t) {
            t.sAL = 0; t.sAC = 0;
            t.sBL = static_cast<int32>(t.screen.TotalLines()) - 1; t.sBC = t.screen.Cols();
        }

        // Construit la liste de base (toujours dispo, sans cout) : PowerShell, cmd,
        // jenga, bash. Les distros WSL sont ajoutees a la demande (DetectWslDistros).
        void EnsureBaseShells() {
            if (mShellsBuilt) return;
            mShellsBuilt = true;
            mShells.PushBack(ShellDef{ SH_PWSH, "powershell", "" });
            mShells.PushBack(ShellDef{ SH_CMD,  "cmd",        "" });
            mShells.PushBack(ShellDef{ SH_BASH, "bash",       "" });
        }

        // Detecte les distributions WSL2 INSTALLEES (`wsl --list --quiet`) et ajoute
        // une entree par distro. WSL_UTF8=1 force une sortie UTF-8 (sinon UTF-16LE) ;
        // on filtre quand meme les octets 0x00 / BOM par robustesse. Appel UNE fois,
        // a la 1re ouverture du combo (evite de geler le demarrage).
        void DetectWslDistros() {
            if (mWslDetected) return;
            mWslDetected = true;
#if defined(_WIN32)
            FILE* pipe = _popen("set \"WSL_UTF8=1\" && wsl --list --quiet 2>nul", "r");
            if (!pipe) return;
            char buf[256]; usize j = 0; int ch; int32 found = 0;
            auto flush = [&]() {
                while (j > 0 && (buf[j - 1] == ' ' || buf[j - 1] == '\t')) --j;   // trim fin
                buf[j] = '\0';
                if (j > 0) { mShells.PushBack(ShellDef{ SH_WSL, NkString("WSL: ") + buf, NkString(buf) }); ++found; }
                j = 0;
            };
            while ((ch = std::fgetc(pipe)) != EOF) {
                if (ch == 0x00 || ch == '\r' || ch == 0xFF || ch == 0xFE) continue;   // nuls UTF-16 + BOM
                if (ch == '\n') { flush(); continue; }
                if (j + 1 < sizeof(buf)) buf[j++] = static_cast<char>(ch);
            }
            flush();
            _pclose(pipe);
            if (found == 0) mShells.PushBack(ShellDef{ SH_WSL, "wsl", "" });   // repli : wsl generique
#else
            mShells.PushBack(ShellDef{ SH_WSL, "wsl", "" });
#endif
        }
        int32 FirstAlive() const { for (int32 i = 0; i < 8; ++i) if (mTerm[i].alive) return i; return 0; }
        int32 AliveCount() const { int32 n = 0; for (int32 i = 0; i < 8; ++i) if (mTerm[i].alive) ++n; return n; }
        // `idx` = index dans mShells (selecteur). Copie kind + distro + libelle.
        void AddTerm(int32 idx) {
            EnsureBaseShells();
            if (idx < 0 || idx >= static_cast<int32>(mShells.Size())) idx = 0;
            const ShellDef& sd = mShells[idx];
            for (int32 i = 0; i < 8; ++i) if (!mTerm[i].alive) {
                mTerm[i].pty.Stop();                   // recycle un eventuel ancien pty du slot
                mTerm[i].screen.Clear();
                mTerm[i].alive = true; mTerm[i].started = false;
                mTerm[i].shell = sd.kind; mTerm[i].distro = sd.distro; mTerm[i].label = sd.label;
                mTerm[i].scrollY = 0.f; mTerm[i].follow = true;
                mTerm[i].sAL = mTerm[i].sAC = mTerm[i].sBL = mTerm[i].sBC = 0; mTerm[i].dragging = false;
                mActive = i; return;
            }
        }
        void CloseTerm(int32 i) {
            if (AliveCount() <= 1) return;
            mTerm[i].pty.Stop();
            mTerm[i].alive = false; mTerm[i].started = false;
            if (mActive == i) mActive = FirstAlive();
        }

        static NkColor ShellColor(int32 s) {
            switch (s) { case SH_PWSH: return { 31, 111, 235, 255 }; case SH_WSL: return { 233, 84, 32, 255 };
                         case SH_BASH: return { 77, 160, 79, 255 };  case SH_JENGA: return { 200, 150, 40, 255 };
                         default: return { 150, 158, 168, 255 }; }
        }

        // Liste VERTICALE des terminaux a DROITE (facon VSCode) : actions (+ / combo
        // de shell) en haut, puis un item par terminal (icone couleur + nom), actif
        // surligne, X au survol.
        void DrawTermList(NkGuiContext& ctx, const NkRect& R) {
            if (R.w < 8.f) return;
            auto& dl = ctx.DL();
            dl.AddRectFilled(R, NkColor{ 1, 4, 9, 255 });                       // fond liste #010409
            dl.AddRectFilled({ R.x, R.y, 1.f, R.h }, NkColor{ 33, 39, 48, 255 }); // bord gauche
            const NkVec2 m = ctx.input.mousePos;
            auto inR = [&](const NkRect& r) { return m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h; };
            const float32 h = ctx.ItemHeight();
            const float32 by = (h - (ctx.font ? ctx.font->LineHeight() : 14.f)) * 0.5f + (ctx.font ? ctx.font->Ascent() : 11.f);

            // (Le "+" et le combobox de shell sont sur la BARRE D'ONGLETS — OnTabBarActions.)
            // Items : un par terminal vivant.
            float32 y = R.y + 6.f; int32 toClose = -1;
            for (int32 i = 0; i < 8; ++i) {
                if (!mTerm[i].alive) continue;
                const NkRect row = { R.x + 1.f, y, R.w - 1.f, h };
                const bool active = (i == mActive), hov = inR(row);
                if (active)    dl.AddRectFilled(row, NkColor{ 31, 111, 235, 55 });   // selection
                else if (hov)  dl.AddRectFilled(row, ctx.theme.buttonHover);
                if (active)    dl.AddRectFilled({ R.x + 1.f, y, 2.f, h }, ctx.theme.accent);
                dl.AddRectFilled({ R.x + 10.f, y + (h - 9.f) * 0.5f, 9.f, 9.f }, ShellColor(mTerm[i].shell)); // icone
                if (ctx.font && ctx.font->Valid())
                    dl.AddText(ctx.font->Face(), ctx.font->TexId(), { R.x + 26.f, y + by },
                               mTerm[i].label.CStr(), active ? ctx.theme.text : ctx.theme.textDisabled);
                bool closeClicked = false;
                if (hov && AliveCount() > 1) {
                    const NkRect cl = { row.x + row.w - 20.f, y + (h - 14.f) * 0.5f, 14.f, 14.f };
                    const bool ch = inR(cl); if (ch) dl.AddRectFilled(cl, NkColor{ 33, 39, 48, 255 });
                    const float32 cx = cl.x + 7.f, cy = cl.y + 7.f, a = 3.f;
                    dl.AddLine({ cx - a, cy - a }, { cx + a, cy + a }, ctx.theme.text, 1.2f);
                    dl.AddLine({ cx - a, cy + a }, { cx + a, cy - a }, ctx.theme.text, 1.2f);
                    if (ch && ctx.input.mouseClicked[0] && ctx.popupDepth == 0) { toClose = i; closeClicked = true; }
                }
                if (hov && !closeClicked && ctx.input.mouseClicked[0] && ctx.popupDepth == 0) mActive = i;
                y += h;
            }
            if (toClose >= 0) CloseTerm(toClose);
        }

        Term  mTerm[8];
        int32 mActive   = 0;
        int32 mNewShell = 0;          // index dans mShells (0 = powershell)
        NkVector<ShellDef> mShells;   // selecteur de shells (base + distros WSL)
        bool  mShellsBuilt  = false;
        bool  mWslDetected  = false;
        bool  mFocused      = false;  // le terminal capte-t-il le clavier ?
        NkCtxMenu      mMenu;         // menu contextuel (clic droit) Copier/Coller
        NkVector<char> mDrain;        // tampon de drain pty (reutilise)
    };

} // namespace nkcode
