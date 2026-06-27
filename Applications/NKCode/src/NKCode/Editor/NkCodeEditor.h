#pragma once
// =============================================================================
// NkCodeEditor.h — Widget code-editeur facon VSCode (immediate mode, sur NKGui).
//   Modele par LIGNES (source de verite pour l'edition) + etat curseur/selection/
//   scroll EMBARQUE dans le document -> survit au changement d'onglet ET au
//   re-dock (etat hors-frame). Fonctions : gouttiere + numeros de ligne, scroll
//   vertical/horizontal (molette + barres), caret, selection souris (clic-glisser)
//   et clavier (Shift+fleches), navigation (fleches/Home/End), edition (saisie,
//   Entree, Backspace, Delete), auto-scroll pour garder le caret visible.
//
//   Limites v1 : pas de presse-papiers (NKWindow n'expose pas encore de clipboard),
//   pas de PageUp/Down (touches absentes de NkGuiKey), largeur H approximee sur les
//   lignes visibles. Coloration syntaxique = phase suivante.
// =============================================================================
#include "NKGui/NKGui.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCode/Editor/NkSyntax.h"
#include "NKCode/Editor/NkTextDraw.h"

#include <cstdio>   // snprintf (numeros de ligne)

namespace nkcode {

    using namespace nkentseu;
    using namespace nkentseu::nkgui;

    // Une ligne = caracteres bruts (PAS de '\0'). Mesure/dessin via plage [begin,end).
    using NkCodeLine = NkVector<char>;

    // Document editable : modele par lignes + etat d'edition. Place dans OpenFile
    // (persistant) afin que curseur/selection/scroll ne soient jamais perdus.
    struct NkCodeDoc {
        NkVector<NkCodeLine> lines;
        int32   curLine = 0, curCol = 0;     // curseur (col = index caractere)
        int32   selLine = 0, selCol = 0;     // ancre de selection (== curseur si vide)
        float32 scrollX = 0.f, scrollY = 0.f;
        bool    dirty   = false;
        // Cache de la largeur de la PLUS LONGUE ligne (px) -> barre H stable
        // (recalcule seulement a l'edition, pas a chaque frame / scroll).
        bool    widthDirty   = true;
        float32 maxLineWCache = 0.f;

        int32 LineCount()        const { return static_cast<int32>(lines.Size()); }
        int32 LineLen(int32 l)   const { return (l >= 0 && l < LineCount()) ? static_cast<int32>(lines[l].Size()) : 0; }
        bool  HasSel()           const { return curLine != selLine || curCol != selCol; }
        void  Collapse()               { selLine = curLine; selCol = curCol; }

        void EnsureNonEmpty() { if (lines.Empty()) lines.PushBack(NkCodeLine()); }

        void ClampCursor() {
            if (curLine < 0) curLine = 0;
            if (curLine >= LineCount()) curLine = LineCount() - 1;
            if (curLine < 0) { curLine = 0; }
            if (curCol < 0) curCol = 0;
            const int32 m = LineLen(curLine);
            if (curCol > m) curCol = m;
        }

        // Selection normalisee : (aL,aC) <= (bL,bC).
        void SelRange(int32& aL, int32& aC, int32& bL, int32& bC) const {
            aL = selLine; aC = selCol; bL = curLine; bC = curCol;
            if (aL > bL || (aL == bL && aC > bC)) {
                int32 tL = aL, tC = aC; aL = bL; aC = bC; bL = tL; bC = tC;
            }
        }

        // ── Construction / serialisation ──────────────────────────────────────
        void Clear() { lines.Clear(); curLine = curCol = selLine = selCol = 0; scrollX = scrollY = 0.f; widthDirty = true; }

        void SetText(const char* s) {
            Clear();
            lines.PushBack(NkCodeLine());
            for (const char* p = s; p && *p; ++p) {
                if (*p == '\n')      lines.PushBack(NkCodeLine());
                else if (*p == '\r') {}                              // ignore CR (CRLF -> LF)
                else if (*p == '\t') { for (int k = 0; k < 4; ++k) lines[lines.Size() - 1].PushBack(' '); }
                else                 lines[lines.Size() - 1].PushBack(*p);
            }
            EnsureNonEmpty();
        }

        NkString GetText() const {
            NkVector<char> buf;
            for (usize i = 0; i < lines.Size(); ++i) {
                const NkCodeLine& ln = lines[i];
                for (usize j = 0; j < ln.Size(); ++j) buf.PushBack(ln[j]);
                if (i + 1 < lines.Size()) buf.PushBack('\n');
            }
            buf.PushBack('\0');
            return NkString(buf.Data());
        }

        // ── Edition ───────────────────────────────────────────────────────────
        void EraseSelection() {
            if (!HasSel()) return;
            int32 aL, aC, bL, bC; SelRange(aL, aC, bL, bC);
            if (aL == bL) {
                lines[aL].Erase(lines[aL].Begin() + aC, lines[aL].Begin() + bC);
            } else {
                NkCodeLine& A = lines[aL];
                A.Erase(A.Begin() + aC, A.End());                   // garde [0, aC)
                NkCodeLine& B = lines[bL];
                for (usize j = static_cast<usize>(bC); j < B.Size(); ++j) A.PushBack(B[j]);  // + queue de B
                lines.Erase(lines.Begin() + (aL + 1), lines.Begin() + (bL + 1));
            }
            curLine = aL; curCol = aC; Collapse(); dirty = true; widthDirty = true;
        }

        void InsertChar(char c) {
            EraseSelection();
            NkCodeLine& L = lines[curLine];
            L.Insert(L.Begin() + curCol, c);
            ++curCol; Collapse(); dirty = true; widthDirty = true;
        }

        void InsertNewline() {
            EraseSelection();
            NkCodeLine& C = lines[curLine];
            NkCodeLine tail;
            for (usize j = static_cast<usize>(curCol); j < C.Size(); ++j) tail.PushBack(C[j]);
            C.Erase(C.Begin() + curCol, C.End());
            lines.Insert(lines.Begin() + (curLine + 1), tail);
            ++curLine; curCol = 0; Collapse(); dirty = true; widthDirty = true;
        }

        void Backspace() {
            if (HasSel()) { EraseSelection(); return; }
            if (curCol > 0) {
                lines[curLine].Erase(lines[curLine].Begin() + (curCol - 1));
                --curCol;
            } else if (curLine > 0) {
                const int32 prev = curLine - 1, plen = LineLen(prev);
                NkCodeLine& P = lines[prev]; NkCodeLine& C = lines[curLine];
                for (usize j = 0; j < C.Size(); ++j) P.PushBack(C[j]);
                lines.Erase(lines.Begin() + curLine);
                curLine = prev; curCol = plen;
            }
            Collapse(); dirty = true; widthDirty = true;
        }

        void DeleteFwd() {
            if (HasSel()) { EraseSelection(); return; }
            if (curCol < LineLen(curLine)) {
                lines[curLine].Erase(lines[curLine].Begin() + curCol);
            } else if (curLine < LineCount() - 1) {
                NkCodeLine& C = lines[curLine]; NkCodeLine& N = lines[curLine + 1];
                for (usize j = 0; j < N.Size(); ++j) C.PushBack(N[j]);
                lines.Erase(lines.Begin() + (curLine + 1));
            }
            Collapse(); dirty = true; widthDirty = true;
        }

        // Texte selectionne (multi-ligne, lignes jointes par '\n'). Vide si pas de selection.
        NkString GetSelectedText() const {
            if (!HasSel()) return NkString();
            int32 aL, aC, bL, bC; SelRange(aL, aC, bL, bC);
            NkVector<char> buf;
            for (int32 l = aL; l <= bL; ++l) {
                const int32 c0 = (l == aL) ? aC : 0, c1 = (l == bL) ? bC : LineLen(l);
                for (int32 c = c0; c < c1; ++c) buf.PushBack(lines[l][c]);
                if (l < bL) buf.PushBack('\n');
            }
            buf.PushBack('\0');
            return NkString(buf.Data());
        }
        // Insere `s` au curseur (remplace la selection). Gere '\n' / '\t'.
        void InsertText(const char* s) {
            if (HasSel()) EraseSelection();
            for (const char* p = s; *p; ++p) {
                if (*p == '\n') InsertNewline();
                else if (*p == '\r') { /* ignore */ }
                else if (*p == '\t') { for (int32 k = 0; k < 4; ++k) InsertChar(' '); }
                else InsertChar(*p);
            }
        }
        void SelectAll() {
            selLine = 0; selCol = 0;
            curLine = LineCount() - 1; if (curLine < 0) curLine = 0;
            curCol = LineLen(curLine);
        }

        // Re-indente le document a partir de la profondeur d'accolades (style VS
        // "Format Document"). 4 espaces / niveau ; les lignes commencant par } ) ]
        // se desindentent. v1 : naif (ignore accolades dans chaines/commentaires).
        void FormatCpp() {
            int32 depth = 0;
            for (usize li = 0; li < lines.Size(); ++li) {
                NkCodeLine& ln = lines[li];
                usize s = 0; while (s < ln.Size() && (ln[s] == ' ' || ln[s] == '\t')) ++s;
                int32 lineDepth = depth;
                if (s < ln.Size() && (ln[s] == '}' || ln[s] == ')' || ln[s] == ']'))
                    lineDepth = depth > 0 ? depth - 1 : 0;
                NkCodeLine nl;
                if (s < ln.Size()) {                              // ligne non vide -> indente
                    for (int32 d = 0; d < lineDepth; ++d) for (int32 k = 0; k < 4; ++k) nl.PushBack(' ');
                    for (usize j = s; j < ln.Size(); ++j) nl.PushBack(ln[j]);
                }
                for (usize j = s; j < ln.Size(); ++j) {           // MAJ profondeur
                    const char c = ln[j];
                    if (c == '{') ++depth; else if (c == '}') { if (depth > 0) --depth; }
                }
                ln = nl;
            }
            dirty = true; widthDirty = true; ClampCursor(); Collapse();
        }
    };

    // ── Focus clavier global (un seul editeur actif a la fois) ────────────────
    inline NkGuiId& NkCodeFocusId() { static NkGuiId id = NKGUI_ID_NONE; return id; }
    inline NkCtxMenu& NkCodeCtxMenu() { static NkCtxMenu mn; return mn; }   // menu clic droit (partage)

    namespace detail {
        inline bool InRect(const NkRect& r, const NkVec2& p) {
            return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
        }
        // Largeur en px du prefixe [0, col) de la ligne `l`.
        inline float32 PrefixW(NkGuiContext& ctx, const NkCodeDoc& d, int32 l, int32 col) {
            if (col <= 0 || l < 0 || l >= d.LineCount()) return 0.f;
            const NkCodeLine& ln = d.lines[l];
            if (ln.Size() == 0) return 0.f;
            const int32 c = col > static_cast<int32>(ln.Size()) ? static_cast<int32>(ln.Size()) : col;
            return ctx.font->Face()->CalcTextSizeX(ln.Data(), ln.Data() + c);
        }
        // Colonne dont la position pixel est la plus proche de targetX.
        inline int32 ColAtX(NkGuiContext& ctx, const NkCodeDoc& d, int32 l, float32 targetX) {
            const int32 n = d.LineLen(l);
            if (n <= 0 || targetX <= 0.f) return 0;
            const NkCodeLine& ln = d.lines[l];
            float32 prev = 0.f;
            for (int32 i = 1; i <= n; ++i) {
                const float32 w = ctx.font->Face()->CalcTextSizeX(ln.Data(), ln.Data() + i);
                if (w >= targetX) return (targetX - prev < w - targetX) ? (i - 1) : i;
                prev = w;
            }
            return n;
        }
    } // namespace detail

    // Dessine + pilote un editeur de code sur `d` dans `area`. Retourne true si le
    // document a ete modifie cette frame.
    inline bool CodeEditor(NkGuiContext& ctx, const char* idStr, NkCodeDoc& d, const NkRect& area,
                           NkLang lang = NkLang::None) {
        using namespace detail;
        NkCodeFontScope _cfs(ctx);   // tout l'editeur dessine avec la police monospace (code)
        if (!ctx.font || !ctx.font->Face()) return false;
        d.EnsureNonEmpty();

        // Palette VSCode Dark+.
        const NkColor kBg       = {  13,  17,  23, 255 };   // #0D1117
        const NkColor kGutterBg = {  13,  17,  23, 255 };   // #0D1117
        const NkColor kLineNo   = { 110, 118, 129, 255 };   // muted
        const NkColor kLineNoCur= { 223, 223, 223, 255 };   // #DFDFDF
        const NkColor kText     = { 223, 223, 223, 255 };   // #DFDFDF
        const NkColor kSel      = {  31, 111, 235, 110 };   // #1F6FEB (semi)
        const NkColor kCurLine  = { 110, 118, 129,  26 };   // surlignage subtil
        const NkColor kCaret    = { 223, 223, 223, 255 };   // #DFDFDF
        const NkColor kScrollTk = {  72,  79,  87, 200 };
        const NkColor kBorder   = {  31, 111, 235, 255 };   // focus #1F6FEB

        auto& dl = ctx.DL();
        const NkGuiId id      = ctx.GetId(idStr);
        const NkGuiId dragId  = ctx.GetId((NkString(idStr) + "#drag").CStr());
        const NkGuiId vbarId  = ctx.GetId((NkString(idStr) + "#vbar").CStr());
        const NkGuiId hbarId  = ctx.GetId((NkString(idStr) + "#hbar").CStr());
        const bool    focused = (NkCodeFocusId() == id);

        const float32 lineGap = ctx.S(5.f);                     // espace entre les lignes (interligne)
        const float32 lineH = ctx.font->LineHeight() + lineGap; // hauteur d'une ligne
        const float32 asc   = ctx.font->Ascent() + lineGap * 0.5f;  // baseline centree dans la ligne
        const float32 pad   = 4.f;

        // Gouttiere : largeur calee sur le nombre de chiffres du dernier numero.
        char numbuf[16];
        std::snprintf(numbuf, sizeof(numbuf), "%d", d.LineCount());
        const float32 gutterW = ctx.font->MeasureWidth(numbuf) + pad * 3.f;
        // Cadre regle : on RESERVE en permanence les gouttieres de scroll (V a droite,
        // H en bas) -> zone texte bornee, barres toujours visibles (facon VSCode/VS).
        const float32 sbW = 14.f;
        const NkRect  textArea = { area.x + gutterW, area.y, area.w - gutterW - sbW, area.h - sbW };
        const float32 textLeft = textArea.x + pad;
        const float32 topPad   = lineH, botPad = lineH;     // ligne vierge haut + bas (non editable)
        const float32 textTop  = textArea.y + topPad;       // 1re ligne decalee d'une ligne vierge
        const float32 viewH    = textArea.h;
        const float32 viewW    = textArea.w - pad * 2.f;

        // Fond.
        dl.AddRectFilled(area, kBg);
        dl.AddRectFilled({ area.x, area.y, gutterW, area.h }, kGutterBg);

        // ── Entrees ───────────────────────────────────────────────────────────
        const NkVec2 mouse = ctx.input.mousePos;
        const bool   hover = InRect(area, mouse);
        const int32  oldL = d.curLine, oldC = d.curCol;   // pour detecter un mouvement du curseur

        // Molette (consommee pour ne pas scroller la fenetre dessous).
        if (hover) {
            if (ctx.input.wheel != 0.f) {
                if (ctx.input.shiftDown) d.scrollX -= ctx.input.wheel * 40.f;
                else                     d.scrollY -= ctx.input.wheel * lineH * 3.f;
                ctx.input.wheel = 0.f;
            }
            if (ctx.input.wheelH != 0.f) { d.scrollX -= ctx.input.wheelH * 40.f; ctx.input.wheelH = 0.f; }
        }

        // Clic dans la zone texte : focus + place le curseur (+ selection si Shift) + drag.
        // Ignore si un popup (ex. combo ouvert vers le haut) recouvre l'editeur -> sinon
        // le clic sur le popup volerait le focus a l'editeur.
        const bool overText = InRect(textArea, mouse) && ctx.popupDepth == 0;
        if (ctx.input.mouseClicked[0] && overText) {
            NkCodeFocusId() = id;
            ctx.activeId = dragId;
            int32 l = static_cast<int32>((mouse.y - textTop + d.scrollY) / lineH);
            if (l < 0) l = 0; if (l >= d.LineCount()) l = d.LineCount() - 1;
            const int32 c = ColAtX(ctx, d, l, mouse.x - textLeft + d.scrollX);
            d.curLine = l; d.curCol = c;
            if (!ctx.input.shiftDown) d.Collapse();
        }
        // Glisser : etend la selection.
        if (ctx.activeId == dragId && ctx.input.mouseDown[0]) {
            int32 l = static_cast<int32>((mouse.y - textTop + d.scrollY) / lineH);
            if (l < 0) l = 0; if (l >= d.LineCount()) l = d.LineCount() - 1;
            d.curLine = l; d.curCol = ColAtX(ctx, d, l, mouse.x - textLeft + d.scrollX);
        }
        // Clic DROIT dans la zone texte : focus + menu contextuel Copier/Couper/Coller.
        if (ctx.input.mouseClicked[2] && overText) {
            NkCodeFocusId() = id; NkCodeCtxMenu().open = true; NkCodeCtxMenu().pos = mouse;
        }
        // Clic hors zone texte mais hors editeur : perd le focus.
        if (ctx.input.mouseClicked[0] && !hover && focused) NkCodeFocusId() = NKGUI_ID_NONE;

        bool changed = false;
        if (focused) {
            const bool shift = ctx.input.shiftDown;
            // Saisie texte.
            if (!ctx.input.ctrlDown) {
                for (int32 i = 0; i < ctx.input.charCount; ++i) {
                    const uint32 cp = ctx.input.chars[i];
                    if (cp == 9) { for (int k = 0; k < 4; ++k) d.InsertChar(' '); changed = true; }
                    else if (cp >= 32 && cp < 127) { d.InsertChar(static_cast<char>(cp)); changed = true; }
                }
            }
            // Touches d'edition (avec repetition au maintien).
            auto K = [&](NkGuiKey k) { return ctx.input.KeyPressedRepeat(k); };
            if (K(NkGuiKey::Enter))     { d.InsertNewline(); changed = true; }
            if (K(NkGuiKey::Backspace)) { d.Backspace();     changed = true; }
            if (K(NkGuiKey::Delete))    { d.DeleteFwd();     changed = true; }
            if (K(NkGuiKey::Left)) {
                if (d.curCol > 0) --d.curCol;
                else if (d.curLine > 0) { --d.curLine; d.curCol = d.LineLen(d.curLine); }
                if (!shift) d.Collapse();
            }
            if (K(NkGuiKey::Right)) {
                if (d.curCol < d.LineLen(d.curLine)) ++d.curCol;
                else if (d.curLine < d.LineCount() - 1) { ++d.curLine; d.curCol = 0; }
                if (!shift) d.Collapse();
            }
            if (K(NkGuiKey::Up))   { if (d.curLine > 0) { --d.curLine; d.ClampCursor(); } if (!shift) d.Collapse(); }
            if (K(NkGuiKey::Down)) { if (d.curLine < d.LineCount() - 1) { ++d.curLine; d.ClampCursor(); } if (!shift) d.Collapse(); }
            if (K(NkGuiKey::Home)) { d.curCol = 0;                 if (!shift) d.Collapse(); }
            if (K(NkGuiKey::End))  { d.curCol = d.LineLen(d.curLine); if (!shift) d.Collapse(); }
            // Copier / couper / coller / tout-selectionner (presse-papiers).
            if (ctx.input.wantSelectAll) d.SelectAll();
            if ((ctx.input.wantCopy || ctx.input.wantCut) && d.HasSel()) {
                ctx.SetClipboard(d.GetSelectedText().CStr());
                if (ctx.input.wantCut) { d.EraseSelection(); changed = true; }
            }
            if (ctx.input.wantPaste) {
                const NkString clip = ctx.GetClipboard();
                if (!clip.Empty()) { d.InsertText(clip.CStr()); changed = true; }
            }
        }
        d.ClampCursor();

        // ── Largeur max GLOBALE (cache) -> barre H stable, independante du scroll ──
        const float32 contentH = d.LineCount() * lineH + topPad + botPad;
        if (d.widthDirty) {
            float32 mw = 0.f;
            for (usize i = 0; i < d.lines.Size(); ++i) {
                const NkCodeLine& ln = d.lines[i];
                if (ln.Size() == 0) continue;
                const float32 w = ctx.font->Face()->CalcTextSizeX(ln.Data(), ln.Data() + ln.Size());
                if (w > mw) mw = w;
            }
            d.maxLineWCache = mw; d.widthDirty = false;
        }
        const float32 maxLineW = d.maxLineWCache;

        // Auto-scroll : garde le caret dans la vue UNIQUEMENT s'il vient de bouger
        // (clic/clavier/edition) -> ne combat pas le scroll molette/barre.
        const bool ensureCaret = (d.curLine != oldL || d.curCol != oldC || changed);
        if (ensureCaret) {
            const float32 cX = PrefixW(ctx, d, d.curLine, d.curCol);
            const float32 cY = topPad + d.curLine * lineH;
            if (cY < d.scrollY)                 d.scrollY = cY;
            if (cY + lineH > d.scrollY + viewH) d.scrollY = cY + lineH - viewH;
            if (cX < d.scrollX)                 d.scrollX = cX;
            if (cX + 2.f > d.scrollX + viewW)   d.scrollX = cX + 2.f - viewW;
        }
        const float32 maxScrollY = contentH > viewH ? contentH - viewH : 0.f;
        const float32 maxScrollX = maxLineW > viewW ? maxLineW - viewW : 0.f;
        if (d.scrollY < 0.f) d.scrollY = 0.f; if (d.scrollY > maxScrollY) d.scrollY = maxScrollY;
        if (d.scrollX < 0.f) d.scrollX = 0.f; if (d.scrollX > maxScrollX) d.scrollX = maxScrollX;

        // ── Rendu des lignes visibles ─────────────────────────────────────────
        int32 aL, aC, bL, bC; d.SelRange(aL, aC, bL, bC);
        int32 firstVis = static_cast<int32>((d.scrollY - topPad) / lineH); if (firstVis < 0) firstVis = 0;
        const int32 lastVis  = firstVis + static_cast<int32>(viewH / lineH) + 2;

        // Surlignage de la ligne courante (toute la zone texte).
        if (focused) {
            const float32 y = textTop + d.curLine * lineH - d.scrollY;
            if (y + lineH > textArea.y && y < textArea.y + textArea.h)
                dl.AddRectFilled({ textArea.x, y, textArea.w, lineH }, kCurLine);
        }

        // Etat bloc-commentaire (/* .. */) AU DEBUT de la 1re ligne visible :
        // scan prefixe [0, firstVis) (C uniquement ; sinon jamais en bloc).
        const NkSynColors& syn = ctx.syntax;   // couleurs editables via Preferences > Langages
        const NkFont* face = ctx.font->Face();
        const uint32  tex  = ctx.font->TexId();
        bool inBlock = false;
        if (lang == NkLang::C || lang == NkLang::NKSL || lang == NkLang::Markdown)
            for (int32 i = 0; i < firstVis && i < d.LineCount(); ++i)
                inBlock = TokenizeLine(lang, d.lines[i].Data(), static_cast<int32>(d.lines[i].Size()),
                                       inBlock, syn, [](int32, int32, const NkColor&) {});

        dl.PushClipRect(textArea, true);
        for (int32 i = firstVis; i <= lastVis && i < d.LineCount(); ++i) {
            if (i < 0) continue;
            const float32 y        = textTop + i * lineH - d.scrollY;
            const float32 baseline = y + asc;
            const NkCodeLine& ln   = d.lines[i];
            const int32 n          = static_cast<int32>(ln.Size());

            // Selection sur cette ligne.
            if (d.HasSel() && i >= aL && i <= bL) {
                const int32 c0 = (i == aL) ? aC : 0;
                const int32 c1 = (i == bL) ? bC : n;
                float32 x0 = textLeft + PrefixW(ctx, d, i, c0) - d.scrollX;
                float32 x1 = textLeft + PrefixW(ctx, d, i, c1) - d.scrollX;
                if (i < bL) x1 += 4.f;                       // marque le saut de ligne
                dl.AddRectFilled({ x0, y, x1 - x0, lineH }, kSel);
            }
            // Texte COLORE : tokenise la ligne et dessine chaque plage (curseur x
            // incremental). Appel meme si n==0 pour propager l'etat de bloc.
            const char* data = ln.Data();
            float32 sx = textLeft - d.scrollX;
            inBlock = TokenizeLine(lang, data, n, inBlock, syn,
                [&](int32 a, int32 b, const NkColor& col) {
                    sx = NkDrawTextU(ctx, sx, baseline, y, lineH, data + a, data + b, col);   // box-drawing en primitives
                });
        }
        // Caret.
        if (focused) {
            const float32 cx = textLeft + PrefixW(ctx, d, d.curLine, d.curCol) - d.scrollX;
            const float32 cy = textTop + d.curLine * lineH - d.scrollY;
            dl.AddLine({ cx, cy + 1.f }, { cx, cy + lineH - 1.f }, kCaret, 1.5f);
        }
        dl.PopClipRect();

        // Numeros de ligne (clip a la gouttiere).
        dl.PushClipRect({ area.x, area.y, gutterW, area.h }, true);
        for (int32 i = firstVis; i <= lastVis && i < d.LineCount(); ++i) {
            if (i < 0) continue;
            const float32 baseline = textTop + i * lineH - d.scrollY + asc;
            char nb[16]; std::snprintf(nb, sizeof(nb), "%d", i + 1);
            const float32 nw = ctx.font->MeasureWidth(nb);
            dl.AddText(ctx.font->Face(), ctx.font->TexId(),
                       { area.x + gutterW - pad - nw, baseline }, nb,
                       (i == d.curLine) ? kLineNoCur : kLineNo);
        }
        dl.PopClipRect();

        // ── Barres de defilement : gouttieres TOUJOURS visibles (cadre regle) ──
        const NkColor kTrack  = {  25,  29,  35, 255 };    // gouttiere #191D23
        const NkColor kThumb  = {  72,  79,  87, 200 };    // pouce au repos
        const NkColor kThumbH = { 110, 118, 129, 235 };    // pouce survole/actif
        const NkRect  vTrack  = { area.x + area.w - sbW, area.y, sbW, viewH };
        const NkRect  hTrack  = { area.x, area.y + area.h - sbW, area.w - sbW, sbW };   // pleine largeur (- coin V)
        dl.AddRectFilled(vTrack, kTrack);
        dl.AddRectFilled(hTrack, kTrack);
        dl.AddRectFilled({ vTrack.x, hTrack.y, sbW, sbW }, kTrack);   // coin bas-droite

        // Bouton fleche (dir : 0=haut 1=bas 2=gauche 3=droite). Retourne MAINTENU.
        auto arrowBtn = [&](const NkRect& r, int32 dir) -> bool {
            const bool h = InRect(r, mouse);
            if (h) dl.AddRectFilled(r, NkColor{ 33, 39, 48, 255 });
            const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f, a = 3.2f;
            const NkColor ac = h ? kThumbH : kThumb;
            if      (dir == 0) dl.AddTriangleFilled({ cx, cy - a }, { cx - a, cy + a }, { cx + a, cy + a }, ac);
            else if (dir == 1) dl.AddTriangleFilled({ cx - a, cy - a }, { cx + a, cy - a }, { cx, cy + a }, ac);
            else if (dir == 2) dl.AddTriangleFilled({ cx - a, cy }, { cx + a, cy - a }, { cx + a, cy + a }, ac);
            else               dl.AddTriangleFilled({ cx - a, cy - a }, { cx + a, cy }, { cx - a, cy + a }, ac);
            return h && ctx.input.mouseDown[0];
        };

        // ── Barre VERTICALE : fleche haut + piste (pouce) + fleche bas ──
        {
            const NkRect upB = { vTrack.x, vTrack.y, sbW, sbW };
            const NkRect dnB = { vTrack.x, vTrack.y + viewH - sbW, sbW, sbW };
            const NkRect inner = { vTrack.x, vTrack.y + sbW, sbW, viewH - 2.f * sbW };
            if (arrowBtn(upB, 0)) d.scrollY -= lineH * 0.8f;
            if (arrowBtn(dnB, 1)) d.scrollY += lineH * 0.8f;
            if (maxScrollY > 0.f && inner.h > 8.f) {
                float32 th = inner.h * (viewH / contentH); if (th < 24.f) th = 24.f; if (th > inner.h) th = inner.h;
                const float32 ty = inner.y + (d.scrollY / maxScrollY) * (inner.h - th);
                const NkRect  thumb = { inner.x + 3.f, ty, sbW - 6.f, th };
                if (ctx.input.mouseClicked[0] && InRect(inner, mouse)) ctx.activeId = vbarId;
                const bool act = (ctx.activeId == vbarId);
                if (act && ctx.input.mouseDown[0]) {
                    const float32 t = (mouse.y - inner.y - th * 0.5f) / (inner.h - th);
                    d.scrollY = (t < 0.f ? 0.f : t > 1.f ? 1.f : t) * maxScrollY;
                }
                dl.AddRectFilled(thumb, (act || InRect(inner, mouse)) ? kThumbH : kThumb, 3.f);
            }
        }
        // ── Barre HORIZONTALE : fleche gauche + piste (pouce) + fleche droite ──
        {
            const NkRect lfB = { hTrack.x, hTrack.y, sbW, sbW };
            const NkRect rtB = { hTrack.x + hTrack.w - sbW, hTrack.y, sbW, sbW };
            const NkRect inner = { hTrack.x + sbW, hTrack.y, hTrack.w - 2.f * sbW, sbW };
            if (arrowBtn(lfB, 2)) d.scrollX -= 18.f;
            if (arrowBtn(rtB, 3)) d.scrollX += 18.f;
            if (maxScrollX > 0.f && inner.w > 8.f) {
                float32 tw = inner.w * (viewW / maxLineW); if (tw < 24.f) tw = 24.f; if (tw > inner.w) tw = inner.w;
                const float32 tx = inner.x + (d.scrollX / maxScrollX) * (inner.w - tw);
                const NkRect  thumb = { tx, hTrack.y + 3.f, tw, sbW - 6.f };
                if (ctx.input.mouseClicked[0] && InRect(inner, mouse)) ctx.activeId = hbarId;
                const bool act = (ctx.activeId == hbarId);
                if (act && ctx.input.mouseDown[0]) {
                    const float32 t = (mouse.x - inner.x - tw * 0.5f) / (inner.w - tw);
                    d.scrollX = (t < 0.f ? 0.f : t > 1.f ? 1.f : t) * maxScrollX;
                }
                dl.AddRectFilled(thumb, (act || InRect(inner, mouse)) ? kThumbH : kThumb, 3.f);
            }
        }
        // Re-borne apres defilement par les fleches (l'auto-scroll plus haut est passe).
        if (d.scrollY < 0.f) d.scrollY = 0.f; if (d.scrollY > maxScrollY) d.scrollY = maxScrollY;
        if (d.scrollX < 0.f) d.scrollX = 0.f; if (d.scrollX > maxScrollX) d.scrollX = maxScrollX;

        // Cadre de l'editeur (bordure permanente) + accent si focus.
        dl.AddRect(area, focused ? kBorder : NkColor{ 33, 39, 48, 255 }, 1.f);

        // ── Menu contextuel (clic droit) Copier / Couper / Coller ── (editeur focus)
        if (focused && NkCodeCtxMenu().open) {
            const char* items[] = { "Copier", "Couper", "Coller" };
            const bool  en[]    = { d.HasSel(), d.HasSel(), true };
            const int32 act = NkCtxMenuDraw(ctx, NkCodeCtxMenu(), items, en, 3);
            if (act == 0 && d.HasSel()) ctx.SetClipboard(d.GetSelectedText().CStr());
            else if (act == 1 && d.HasSel()) { ctx.SetClipboard(d.GetSelectedText().CStr()); d.EraseSelection(); changed = true; }
            else if (act == 2) { const NkString clip = ctx.GetClipboard(); if (!clip.Empty()) { d.InsertText(clip.CStr()); changed = true; } }
        }

        if (changed) d.dirty = true;
        return changed;
    }

} // namespace nkcode
