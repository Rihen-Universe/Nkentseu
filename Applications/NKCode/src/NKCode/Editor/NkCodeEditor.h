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
#include "NKCode/Project/NkText.h"   // NkPPEvalActive / NkDefineValue (régions préproc inactives)

#include <cstdio>   // snprintf (numeros de ligne)

namespace nkentseu {
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
        int32   lineSelAnchor = -1;          // ancre de la sélection-ligne Ctrl+L (pour étendre au répété)
        float32 scrollX = 0.f, scrollY = 0.f;
        bool    dirty   = false;
        bool    wantReveal = false;          // demande de révélation du curseur (clic Outline -> défile vers la ligne)
        // ── Ctrl+clic (navigation façon VSCode) : rempli par l'éditeur, consommé par NkCodeState ──
        NkString linkTarget;                 // symbole (go-to-def) ou chemin d'include à ouvrir ; vidé après traitement
        bool     linkIsInclude = false;      // true = chemin d'#include ; false = symbole (définition)
        NkVector<int32> breakpoints;         // lignes (0-based) avec point d'arrêt (gouttière)
        bool HasBreakpoint(int32 l) const { for (usize i = 0; i < breakpoints.Size(); ++i) if (breakpoints[i] == l) return true; return false; }
        void ToggleBreakpoint(int32 l) { for (usize i = 0; i < breakpoints.Size(); ++i) if (breakpoints[i] == l) { breakpoints.Erase(breakpoints.Begin() + i); return; } breakpoints.PushBack(l); }
        // Indicateurs Git par ligne (bande gouttière) : 0 aucun, 1 ajoutée, 2 modifiée.
        // Rempli par NkCodeState::RefreshGit (à l'ouverture + à la sauvegarde). `gitDeleted`
        // = index de ligne AU-DESSUS de laquelle des lignes ont été supprimées.
        NkVector<uint8> gitStatus;
        NkVector<int32> gitDeleted;
        uint8 GitAt(int32 l) const { return (l >= 0 && l < static_cast<int32>(gitStatus.Size())) ? gitStatus[l] : 0; }
        bool  GitDelAt(int32 l) const { for (usize i = 0; i < gitDeleted.Size(); ++i) if (gitDeleted[i] == l) return true; return false; }
        // ── Undo / Redo PAR FICHIER (snapshots texte + curseur, coalescés par type) ──
        struct UndoState { NkString text; int32 cl; int32 cc; };
        NkVector<UndoState> undoU, redoU;
        int32 lastEdit = 0;                  // 0 aucun, 1 saisie, 2 suppression, 3 autre (pour coalescer)
        // ── Recherche / Remplacement (Ctrl+F / Ctrl+H) ──
        bool     findOpen = false;           // barre de recherche affichée
        bool     findReplace = false;        // mode remplacement (champ « Remplacer » en plus)
        bool     findCase = false;           // sensible à la casse
        int32    findFocus = 0;              // champ actif : 0 = Rechercher, 1 = Remplacer
        NkString findQuery, findRepl;        // termes recherché / de remplacement
        NkVector<int32> findLine, findCol;   // occurrences (arrays parallèles ligne/colonne)
        int32    findCur = -1;              // occurrence courante (surlignée)
        int64    findSig = -1;              // signature (contenu+query+casse) -> invalide le cache
        // ── Index de symboles du fichier (coloration SÉMANTIQUE fiable) ──
        NkVector<NkString> symTypes, symFuncs;   // triés -> recherche binaire (NkSymHas)
        int64 symSig = -1;                   // signature du contenu (rebuild seulement si changé)
        // ── Autocomplétion (popup façon VSCode) ──
        bool acOpen = false;                 // popup affiché
        NkVector<NkString> acItems;          // candidats filtrés (préfixe courant)
        int32 acSel = 0;                     // item sélectionné
        int32 acWordCol = 0;                 // colonne de début du mot en cours (pour le remplacer)
        // Remplace le mot [acWordCol, curCol) par l'item choisi (réutilise Backspace/InsertChar).
        void AcceptAutocomplete() {
            if (!acOpen || acItems.Empty()) return;
            const int32 sel = (acSel >= 0 && acSel < static_cast<int32>(acItems.Size())) ? acSel : 0;
            const NkString item = acItems[static_cast<usize>(sel)];
            Checkpoint(3);
            for (int32 del = curCol - acWordCol; del > 0; --del) Backspace();
            for (int32 k = 0; k < static_cast<int32>(item.Size()); ++k) InsertChar(item.CStr()[k]);
            acOpen = false;
        }
        // ── Diagnostics (erreurs/avertissements du compilateur) ──
        struct Diag { int32 line; int32 col; int32 endCol; uint8 sev; NkString msg; };   // sev: 0 warning, 1 error
        NkVector<Diag> diags;
        bool HasDiagOn(int32 l) const { for (usize i = 0; i < diags.Size(); ++i) if (diags[i].line == l) return true; return false; }
        int32 DiagSevOn(int32 l) const { int32 s = -1; for (usize i = 0; i < diags.Size(); ++i) if (diags[i].line == l) { if (diags[i].sev > (uint8)(s < 0 ? 0 : s)) s = diags[i].sev; else if (s < 0) s = diags[i].sev; } return s; }   // -1 aucun, 0 warning, 1 erreur
        // ── Régions préprocesseur inactives (#if/#ifdef non satisfait) -> atténuées ──
        NkVector<uint8> inactive;            // par ligne : 1 = branche morte (grisée façon VSCode)
        int64 ppSig = -1;                    // signature (contenu ^ defines) -> recalcul paresseux
        bool InactiveAt(int32 l) const { return l >= 0 && l < static_cast<int32>(inactive.Size()) && inactive[l] != 0; }
        // Évalue toutes les branches #if/#ifdef/#ifndef/#elif/#else/#endif du fichier avec
        // les defines du PROJET (issus du .jcdb). Une ligne de code dans une branche non
        // prise est marquée inactive ; les directives ne sont grisées que si leur contexte
        // ENGLOBANT est déjà inactif (comme VSCode). `symSig` doit être à jour (EnsureSymbols).
        void EnsurePreproc(NkLang lang, const NkVector<NkString>* defs) {
            int64 h = static_cast<int64>(static_cast<int32>(lang) + 1) * 1099511628211LL;
            if (defs) for (usize i = 0; i < defs->Size(); ++i) { const char* s = (*defs)[i].CStr(); while (*s) { h = (h ^ (unsigned char)*s) * 1099511628211LL; ++s; } }
            const int64 sig = symSig ^ h;
            if (sig == ppSig) return; ppSig = sig;
            inactive.Clear(); inactive.Resize(LineCount(), 0);
            if (lang != NkLang::C || !defs) return;   // .c/.cpp/.h -> NkLang::C ; sinon rien
            // ── 1) Macros DÉFINIES PAR CE FICHIER (include-guard `#ifndef X/#define X` inclus).
            //    Elles ne comptent PAS avant leur `#define` -> on les retire de l'ensemble effectif
            //    (issu du compilateur = état de FIN) et on les (re)définit à leur position. ──
            NkVector<NkString> fileDef;
            auto readName = [](const char* s, NkString& out) { int32 L2 = 0; while (NkPPIsIdc(s[L2])) ++L2; out = NkString(); for (int32 t = 0; t < L2; ++t) out += s[t]; return L2; };
            for (int32 i = 0; i < LineCount(); ++i) {
                const NkCodeLine& L = lines[i]; const char* d = L.Data(); const int32 n = static_cast<int32>(L.Size());
                int32 j = 0; while (j < n && (d[j]==' '||d[j]=='\t')) ++j; if (j >= n || d[j] != '#') continue; ++j; while (j < n && (d[j]==' '||d[j]=='\t')) ++j;
                if (n - j >= 6 && d[j]=='d'&&d[j+1]=='e'&&d[j+2]=='f'&&d[j+3]=='i'&&d[j+4]=='n'&&d[j+5]=='e') { int32 k = j + 6; while (k < n && (d[k]==' '||d[k]=='\t')) ++k; NkString nm; if (readName(d + k, nm) > 0 && (int32)nm.Size() <= n - k) fileDef.PushBack(nm); }
            }
            auto nameOf = [](const NkString& e){ NkString o; for (const char* p = e.CStr(); *p && *p != '='; ++p) o += *p; return o; };
            auto streq = [](const char* a, const char* b){ while (*a && *a == *b) { ++a; ++b; } return *a == *b; };
            auto inFileDef = [&](const NkString& nm){ for (usize x = 0; x < fileDef.Size(); ++x) if (streq(fileDef[x].CStr(), nm.CStr())) return true; return false; };
            // ── 2) Base = effective SANS les macros locales ──
            NkVector<NkString> adj;
            for (usize x = 0; x < defs->Size(); ++x) { if (!inFileDef(nameOf((*defs)[x]))) adj.PushBack((*defs)[x]); }
            auto adjHas = [&](const char* name, int32 len){ for (usize x = 0; x < adj.Size(); ++x) { const char* e = adj[x].CStr(); int32 t = 0; while (t < len && e[t] && e[t] != '=' && e[t] == name[t]) ++t; if (t == len && (e[t] == '\0' || e[t] == '=')) return (int32)x; } return -1; };
            auto adjAdd = [&](const char* name, int32 len){ if (adjHas(name, len) < 0) { NkString nm; for (int32 t = 0; t < len; ++t) nm += name[t]; adj.PushBack(nm); } };
            auto adjDel = [&](const char* name, int32 len){ const int32 x = adjHas(name, len); if (x >= 0) adj.Erase(adj.Begin() + x); };
            // ── 3) Scan des directives (mutations #define/#undef à leur position, si branche active) ──
            struct PF { bool outer; bool taken; bool active; };
            NkVector<PF> st; char buf[4096];
            for (int32 i = 0; i < LineCount(); ++i) {
                const bool emit = st.Empty() ? true : st.Back().active;
                const NkCodeLine& L = lines[i]; const char* d = L.Data(); const int32 n = static_cast<int32>(L.Size());
                int32 j = 0; while (j < n && (d[j]==' '||d[j]=='\t')) ++j;
                if (j >= n || d[j] != '#') { inactive[i] = emit ? 0 : 1; continue; }
                ++j; while (j < n && (d[j]==' '||d[j]=='\t')) ++j;
                const int32 w0 = j; while (j < n && d[j]>='a' && d[j]<='z') ++j; const int32 wl = j - w0;
                auto isW = [&](const char* lit) { int32 k = 0; while (lit[k]) { if (k >= wl || d[w0+k] != lit[k]) return false; ++k; } return k == wl; };
                while (j < n && (d[j]==' '||d[j]=='\t')) ++j;
                int32 a = 0; for (int32 k = j; k < n && a < 4094; ++k) { if (d[k]=='/' && k+1<n && d[k+1]=='/') break; buf[a++] = d[k]; } buf[a] = 0;
                if (isW("if") || isW("ifdef") || isW("ifndef")) {
                    bool cond;
                    if (isW("if")) cond = NkPPEvalActive(buf, adj);
                    else { const char* s = buf; int32 L2 = 0; while (NkPPIsIdc(s[L2])) ++L2; bool f = false; NkDefineValue(adj, s, L2, &f); cond = isW("ifdef") ? f : !f; }
                    inactive[i] = emit ? 0 : 1;
                    st.PushBack(PF{ emit, emit && cond, emit && cond });
                } else if (isW("elif")) {
                    if (!st.Empty()) { PF& f = st.Back(); const bool wasTaken = f.taken; const bool cond = wasTaken ? false : NkPPEvalActive(buf, adj); f.active = f.outer && !wasTaken && cond; if (f.active) f.taken = true; inactive[i] = f.outer ? 0 : 1; }
                    else inactive[i] = emit ? 0 : 1;
                } else if (isW("else")) {
                    if (!st.Empty()) { PF& f = st.Back(); f.active = f.outer && !f.taken; f.taken = true; inactive[i] = f.outer ? 0 : 1; }
                    else inactive[i] = emit ? 0 : 1;
                } else if (isW("endif")) {
                    if (!st.Empty()) { const bool outer = st.Back().outer; inactive[i] = outer ? 0 : 1; st.Erase(st.End() - 1); }
                    else inactive[i] = 0;
                } else {
                    if (emit) {   // #define/#undef ACTIFS -> mettent à jour l'ensemble à leur position
                        if (isW("define")) { const char* s = buf; int32 L2 = 0; while (NkPPIsIdc(s[L2])) ++L2; if (L2 > 0) adjAdd(s, L2); }
                        else if (isW("undef")) { const char* s = buf; int32 L2 = 0; while (NkPPIsIdc(s[L2])) ++L2; if (L2 > 0) adjDel(s, L2); }
                    }
                    inactive[i] = emit ? 0 : 1;   // #include/#define/#pragma... suit l'état courant
                }
            }
        }
        // A appeler AVANT une mutation. Coalesce les runs de saisie/suppression.
        void Checkpoint(int32 kind) {
            if (kind != 0 && kind == lastEdit && (kind == 1 || kind == 2)) return;   // même run -> pas de nouveau snapshot
            if (undoU.Size() > 400) undoU.Erase(undoU.Begin());
            undoU.PushBack({ GetText(), curLine, curCol });
            redoU.Clear();
            lastEdit = kind;
        }
        void ResetEditRun() { lastEdit = 0; lineSelAnchor = -1; acOpen = false; }   // déplacement curseur : nouveau groupe undo + reset sélection-ligne + ferme l'autocomplétion
        void Undo() {
            if (undoU.Empty()) return;
            redoU.PushBack({ GetText(), curLine, curCol });
            const UndoState s = undoU[undoU.Size() - 1]; undoU.Erase(undoU.End() - 1);
            SetText(s.text.CStr()); curLine = s.cl; curCol = s.cc; ClampCursor(); Collapse();
            lastEdit = 0; dirty = true; widthDirty = true;
        }
        void Redo() {
            if (redoU.Empty()) return;
            undoU.PushBack({ GetText(), curLine, curCol });
            const UndoState s = redoU[redoU.Size() - 1]; redoU.Erase(redoU.End() - 1);
            SetText(s.text.CStr()); curLine = s.cl; curCol = s.cc; ClampCursor(); Collapse();
            lastEdit = 0; dirty = true; widthDirty = true;
        }
        // ── Recherche / Remplacement ──────────────────────────────────────────────
        static char FLow(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c; }
        int64 FindSigOf() const {
            int64 h = SymSig();                                   // contenu du document
            for (const char* p = findQuery.CStr(); *p; ++p) h = (h ^ static_cast<unsigned char>(*p)) * 1099511628211LL;
            return (h ^ (findCase ? 1 : 2)) * 1099511628211LL;
        }
        int32 FindCount() const { return static_cast<int32>(findLine.Size()); }
        void FindRecompute() {
            findLine.Clear(); findCol.Clear();
            const int32 qn = static_cast<int32>(findQuery.Size());
            if (qn > 0) for (int32 l = 0; l < LineCount(); ++l) {
                const NkCodeLine& L = lines[l]; const int32 n = static_cast<int32>(L.Size());
                for (int32 c = 0; c + qn <= n; ++c) {
                    bool ok = true;
                    for (int32 k = 0; k < qn; ++k) { char a = L[c + k], b = findQuery.CStr()[k]; if (!findCase) { a = FLow(a); b = FLow(b); } if (a != b) { ok = false; break; } }
                    if (ok) { findLine.PushBack(l); findCol.PushBack(c); }
                }
            }
            findSig = FindSigOf();
            if (findCur >= FindCount()) findCur = FindCount() > 0 ? 0 : -1;
        }
        void FindEnsure() { if (findSig != FindSigOf()) FindRecompute(); }
        void FindGoto(int32 i) {   // place curseur + sélection sur l'occurrence (surlignage + révélation réutilisés)
            if (i < 0 || i >= FindCount()) return;
            findCur = i; const int32 qn = static_cast<int32>(findQuery.Size());
            selLine = findLine[i]; selCol = findCol[i]; curLine = findLine[i]; curCol = findCol[i] + qn;
            ClampCursor(); wantReveal = true;
        }
        void FindNext(bool fwd) {
            FindEnsure(); const int32 n = FindCount(); if (n == 0) { findCur = -1; return; }
            int32 i = findCur;
            if (i < 0) {   // pas d'occ courante -> la 1ère >= curseur (ou la dernière < curseur si recul)
                i = 0; for (int32 k = 0; k < n; ++k) if (findLine[k] > curLine || (findLine[k] == curLine && findCol[k] >= curCol)) { i = k; break; }
                if (!fwd) i = (i - 1 + n) % n;
            } else i = fwd ? (i + 1) % n : (i - 1 + n) % n;
            FindGoto(i);
        }
        void ReplaceCurrent() {
            FindEnsure(); if (findCur < 0 || findCur >= FindCount()) { FindNext(true); return; }
            Checkpoint(3);
            const int32 l = findLine[findCur], c = findCol[findCur], qn = static_cast<int32>(findQuery.Size());
            selLine = l; selCol = c; curLine = l; curCol = c + qn; EraseSelection(); InsertText(findRepl.CStr());
            FindRecompute(); FindNext(true);
        }
        void ReplaceAll() {
            FindEnsure(); if (FindCount() == 0) return;
            Checkpoint(3);
            for (int32 i = FindCount() - 1; i >= 0; --i) {   // de la FIN vers le DÉBUT : préserve les positions
                const int32 l = findLine[i], c = findCol[i], qn = static_cast<int32>(findQuery.Size());
                selLine = l; selCol = c; curLine = l; curCol = c + qn; EraseSelection(); InsertText(findRepl.CStr());
            }
            FindRecompute(); findCur = -1; Collapse();
        }
        void FindClose() { findOpen = false; findCur = -1; findLine.Clear(); findCol.Clear(); findSig = -1; }
        // ── Index de symboles (coloration sémantique) : rebuild paresseux si le contenu change ──
        static int32 SymCmp(const char* a, const char* b) { while (*a && *a == *b) { ++a; ++b; } return (int32)(unsigned char)*a - (int32)(unsigned char)*b; }
        static bool  IsWChar(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }
        int64 SymSig() const {
            int64 h = static_cast<int64>(1469598103934665603ULL);
            for (int32 l = 0; l < LineCount(); ++l) { const NkCodeLine& L = lines[l]; for (usize k = 0; k < L.Size(); ++k) h = (h ^ (unsigned char)L[k]) * 1099511628211LL; h = (h ^ 10) * 1099511628211LL; }
            return h;
        }
        void EnsureSymbols(NkLang lang) { const int64 s = SymSig(); if (s == symSig) return; symSig = s; RebuildSymbols(lang); }
        static void SortDedup(NkVector<NkString>& v) {
            for (int32 i = 1; i < (int32)v.Size(); ++i) { NkString key = v[i]; int32 j = i - 1; while (j >= 0 && SymCmp(v[j].CStr(), key.CStr()) > 0) { v[j + 1] = v[j]; --j; } v[j + 1] = key; }
            for (int32 i = (int32)v.Size() - 1; i > 0; --i) if (SymCmp(v[i].CStr(), v[i - 1].CStr()) == 0) v.Erase(v.Begin() + i);
        }
        void RebuildSymbols(NkLang lang) {
            symTypes.Clear(); symFuncs.Clear();
            if (lang == NkLang::None || lang == NkLang::Markdown) return;
            const bool isC = (lang == NkLang::C || lang == NkLang::NKSL);
            NkScanTextSymbols(GetText().CStr(), isC, symTypes, symFuncs);
            NkSymSortDedup(symTypes); NkSymSortDedup(symFuncs);
        }
        void ExtractSyms(const char* p, int32 n, bool isC, NkLang lang) {
            (void)lang;
            int32 i = 0; while (i < n && (p[i] == ' ' || p[i] == '\t')) ++i;
            if (i >= n || p[i] == '#') return;                                   // ligne vide / préproc-commentaire
            auto starts = [&](const char* w) { int32 k = 0; for (; w[k]; ++k) if (i + k >= n || p[i + k] != w[k]) return false; return true; };
            auto pushRange = [&](int32 s, int32 e, NkVector<NkString>& out) { if (e > s && (p[s] < '0' || p[s] > '9')) { char nm[128]; int32 k = 0; for (int32 t = s; t < e && k < 127; ++t) nm[k++] = p[t]; nm[k] = 0; out.PushBack(NkString(nm)); } };
            auto addFrom  = [&](int32 s, NkVector<NkString>& out) { while (s < n && (p[s] == ' ' || p[s] == '\t')) ++s; int32 e = s; while (e < n && IsWChar(p[e])) ++e; pushRange(s, e, out); };
            // ── Types ──
            if      (starts("struct "))     addFrom(i + 7,  symTypes);
            else if (starts("class "))      addFrom(i + 6,  symTypes);
            else if (starts("enum class ")) addFrom(i + 11, symTypes);
            else if (starts("enum "))       addFrom(i + 5,  symTypes);
            else if (starts("union "))      addFrom(i + 6,  symTypes);
            else if (starts("namespace "))  addFrom(i + 10, symTypes);
            else if (isC && starts("using "))   addFrom(i + 6, symTypes);       // using Alias = ...
            else if (isC && starts("typedef ")) {                               // alias = dernier identifiant avant ';'
                int32 e = n; while (e > 0 && (p[e - 1] == ' ' || p[e - 1] == '\t' || p[e - 1] == ';')) --e;
                int32 s = e; while (s > 0 && IsWChar(p[s - 1])) --s; pushRange(s, e, symTypes);
            }
            // ── Fonctions ──
            if (!isC && starts("def ")) addFrom(i + 4, symFuncs);               // Python
            if (isC) {                                                          // C : définition (ligne finissant par '{' ou ')' sans ';')
                int32 e = n; while (e > 0 && (p[e - 1] == ' ' || p[e - 1] == '\t')) --e;
                const bool endsBrace = (e > 0 && p[e - 1] == '{'), endsParen = (e > 0 && p[e - 1] == ')');
                bool hasSemi = false; for (int32 t = 0; t < n; ++t) if (p[t] == ';') { hasSemi = true; break; }
                if (endsBrace || (endsParen && !hasSemi)) {
                    int32 par = -1; for (int32 t = i; t < n; ++t) { if (p[t] == '(') { par = t; break; } if (p[t] == ';' || p[t] == '=') break; }
                    if (par > i) { int32 ee = par; while (ee > i && p[ee - 1] == ' ') --ee; int32 ss = ee; while (ss > i && IsWChar(p[ss - 1])) --ss;
                        char nm[128]; int32 k = 0; for (int32 t = ss; t < ee && k < 127; ++t) nm[k++] = p[t]; nm[k] = 0;
                        if (ee > ss && SymCmp(nm, "if") && SymCmp(nm, "for") && SymCmp(nm, "while") && SymCmp(nm, "switch") && SymCmp(nm, "catch") && SymCmp(nm, "return") && SymCmp(nm, "sizeof")) symFuncs.PushBack(NkString(nm)); }
                }
            }
        }
        bool    mojibake = false;            // contenu vraisemblablement double-encodé UTF-8 (bannière « Réparer »)
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
            mojibake = NkMojibakeDetect(s);   // détecte le double-encodage à l'ouverture
        }

        // Répare le double-encodage UTF-8 du document (bouton « Réparer l'encodage »).
        // Reconstruit le texte correct puis le recharge ; marque le doc modifié (à enregistrer).
        void RepairEncoding() {
            const NkString fixed = NkMojibakeRepair(GetText().CStr());
            const int32 sl = curLine, sc = curCol;             // préserve grossièrement le curseur
            SetText(fixed.CStr());                             // recharge (mojibake re-détecté -> false)
            curLine = sl; curCol = sc; ClampCursor(); Collapse();
            dirty = true; widthDirty = true;
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

        // ── Opérations LIGNE (raccourcis éditeur, Phase 4) ──────────────────────
        void SelLineRange(int32& a, int32& b) const {
            a = selLine < curLine ? selLine : curLine;
            b = selLine < curLine ? curLine : selLine;
            if (a < 0) a = 0; if (b >= LineCount()) b = LineCount() - 1;
        }
        // Ctrl+L : 1er appui = sélectionne la ligne courante (ancre col 0, curseur en
        // fin de ligne — pas de débordement) ; appuis suivants = ÉTEND vers le bas
        // ligne par ligne (accumulation), façon VS Code.
        void SelectCurrentLine() {
            // « Continue » = notre sélection-ligne précédente est INTACTE (ancre valide,
            // sélection pleine-ligne du bas). Sinon on (re)part de la ligne courante.
            const bool cont = (lineSelAnchor >= 0 && lineSelAnchor < LineCount()
                               && selLine == lineSelAnchor && selCol == 0
                               && curLine >= selLine && curCol == LineLen(curLine));
            int32 anchor, bottom;
            if (cont) { anchor = lineSelAnchor; bottom = (curLine < LineCount() - 1) ? curLine + 1 : curLine; }  // étend d'une ligne
            else      { anchor = bottom = curLine; lineSelAnchor = curLine; }                                    // nouvelle sélection
            if (bottom >= LineCount()) bottom = LineCount() - 1;
            selLine = anchor; selCol = 0;
            curLine = bottom; curCol = LineLen(bottom);
        }
        void DeleteLines() {
            int32 a, b; SelLineRange(a, b);
            for (int32 l = b; l >= a; --l) if (LineCount() > 1) lines.Erase(lines.Begin() + l);
            if (lines.Empty()) lines.PushBack(NkCodeLine());
            curLine = a; if (curLine >= LineCount()) curLine = LineCount() - 1;
            curCol = 0; Collapse(); dirty = true; widthDirty = true;
        }
        void MoveLines(bool up) {
            int32 a, b; SelLineRange(a, b);
            if (up)  { if (a == 0) return; NkCodeLine t = lines[a - 1]; lines.Erase(lines.Begin() + (a - 1)); lines.Insert(lines.Begin() + b, t); --a; --b; }
            else     { if (b >= LineCount() - 1) return; NkCodeLine t = lines[b + 1]; lines.Erase(lines.Begin() + (b + 1)); lines.Insert(lines.Begin() + a, t); ++a; ++b; }
            selLine = a; curLine = b; ClampCursor(); dirty = true; widthDirty = true;
        }
        void DuplicateLines(bool up) {
            int32 a, b; SelLineRange(a, b);
            NkVector<NkCodeLine> copy; for (int32 l = a; l <= b; ++l) copy.PushBack(lines[l]);
            const int32 at = b + 1;
            for (int32 i = 0; i < static_cast<int32>(copy.Size()); ++i) lines.Insert(lines.Begin() + at + i, copy[i]);
            if (!up) { selLine = at; curLine = at + (b - a); }   // curseur sur la copie (bas)
            else     { selLine = a;  curLine = b; }              // reste sur l'original (haut)
            curCol = LineLen(curLine); Collapse(); dirty = true; widthDirty = true;
        }
        void InsertLineBelow() {
            lines.Insert(lines.Begin() + curLine + 1, NkCodeLine());
            ++curLine; curCol = 0; Collapse(); dirty = true; widthDirty = true;
        }
        void InsertLineAbove() {
            lines.Insert(lines.Begin() + curLine, NkCodeLine());
            curCol = 0; Collapse(); dirty = true; widthDirty = true;
        }
        void IndentSelection(bool out) {
            int32 a, b; SelLineRange(a, b);
            for (int32 l = a; l <= b; ++l) {
                if (!out) { for (int32 k = 0; k < 4; ++k) lines[l].Insert(lines[l].Begin(), ' '); }
                else { int32 rm = 0; while (rm < 4 && rm < LineLen(l) && lines[l][rm] == ' ') ++rm; for (int32 k = 0; k < rm; ++k) lines[l].Erase(lines[l].Begin()); }
            }
            ClampCursor(); dirty = true; widthDirty = true;
        }
        // Commente/décommente les lignes sélectionnées avec `prefix` (ex "//", "#").
        void ToggleComment(const char* prefix) {
            if (!prefix || !*prefix) return;
            int32 a, b; SelLineRange(a, b);
            int32 pl = 0; while (prefix[pl]) ++pl;
            bool allComment = true;
            for (int32 l = a; l <= b; ++l) {
                int32 i = 0; while (i < LineLen(l) && lines[l][i] == ' ') ++i;
                if (i >= LineLen(l)) continue;                    // ligne vide -> ignore
                bool m = (i + pl <= LineLen(l));
                for (int32 k = 0; m && k < pl; ++k) if (lines[l][i + k] != prefix[k]) m = false;
                if (!m) { allComment = false; break; }
            }
            for (int32 l = a; l <= b; ++l) {
                int32 i = 0; while (i < LineLen(l) && lines[l][i] == ' ') ++i;
                if (i >= LineLen(l)) continue;
                if (allComment) { for (int32 k = 0; k < pl; ++k) lines[l].Erase(lines[l].Begin() + i);
                                  if (i < LineLen(l) && lines[l][i] == ' ') lines[l].Erase(lines[l].Begin() + i); }
                else { lines[l].Insert(lines[l].Begin() + i, ' '); for (int32 k = pl - 1; k >= 0; --k) lines[l].Insert(lines[l].Begin() + i, prefix[k]); }
            }
            ClampCursor(); dirty = true; widthDirty = true;
        }

        static bool MatchAt(const char* s, int32 i, const char* pat, int32 plen, int32 n) {
            if (i < 0 || i + plen > n) return false;
            for (int32 k = 0; k < plen; ++k) if (s[i + k] != pat[k]) return false; return true;
        }
        int32 LineColToOffset(int32 line, int32 col) const {
            int32 off = 0; for (int32 i = 0; i < line && i < LineCount(); ++i) off += LineLen(i) + 1; return off + col;
        }
        // Commentaire BLOC INTELLIGENT (Ctrl+Maj+/) — délimiteurs DISTINCTS (ex "/* "…" */").
        // TOGGLE : si la sélection est DÉJÀ dans un commentaire englobant `open…close`, on
        // le RETIRE (dé-commente) au lieu d'imbriquer (ce qui casserait le commentaire).
        void BlockComment(const char* open, const char* close) {
            if (!open || !*open || !close || !*close) return;
            int32 aL, aC, bL, bC;
            if (HasSel()) SelRange(aL, aC, bL, bC);
            else { aL = bL = curLine; aC = 0; bC = LineLen(curLine); }
            const NkString full = GetText();
            const char* s = full.CStr(); const int32 n = static_cast<int32>(full.Size());
            const int32 ol = static_cast<int32>(NkString(open).Size()), cl = static_cast<int32>(NkString(close).Size());
            const int32 aOff = LineColToOffset(aL, aC), bOff = LineColToOffset(bL, bC);
            // Commentaire ENGLOBANT la sélection : `open` à/avant le début (sans `close`
            // entre) + son `close` apparié à/après la fin. Couvre BOTH : sélection = le
            // commentaire exact (délimiteurs aux bords) ET sélection À L'INTÉRIEUR d'un
            // commentaire (délimiteurs dehors). Si trouvé -> on RETIRE (dé-commente).
            int32 encOpen = -1;
            for (int32 i = aOff; i >= 0; --i) { if (MatchAt(s, i, close, cl, n)) break; if (MatchAt(s, i, open, ol, n)) { encOpen = i; break; } }
            int32 encClose = -1;
            if (encOpen >= 0) for (int32 i = encOpen + ol; i + cl <= n; ++i) { if (MatchAt(s, i, open, ol, n)) break; if (MatchAt(s, i, close, cl, n)) { encClose = i; break; } }
            const bool found = (encOpen >= 0 && encClose >= 0 && encOpen <= aOff && encClose + cl >= bOff);
            const bool exact = found && encOpen >= aOff && encClose + cl <= bOff;   // les DEUX délimiteurs sont DANS la sélection
            // Sélection À L'INTÉRIEUR d'un commentaire (délimiteurs dehors) -> NE PAS
            // retirer le bloc : commentaire de LIGNE sur la sélection (comme demandé).
            if (found && !exact) { ToggleComment("//"); return; }
            NkVector<char> out;
            if (exact) {
                // dé-commente le bloc EXACT : copie tout SAUF [encOpen,+ol) et [encClose,+cl)
                for (int32 i = 0; i < n; ++i) { if ((i >= encOpen && i < encOpen + ol) || (i >= encClose && i < encClose + cl)) continue; out.PushBack(s[i]); }
            } else {
                // commente : insère open à aOff et close à bOff
                for (int32 i = 0; i < n; ++i) { if (i == aOff) for (int32 k = 0; k < ol; ++k) out.PushBack(open[k]); if (i == bOff) for (int32 k = 0; k < cl; ++k) out.PushBack(close[k]); out.PushBack(s[i]); }
                if (bOff == n) for (int32 k = 0; k < cl; ++k) out.PushBack(close[k]);   // close en toute fin
            }
            out.PushBack('\0');
            SetText(out.Data()); curLine = aL; curCol = aC; ClampCursor(); Collapse();
            dirty = true; widthDirty = true;
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

    // Préfixe de commentaire de ligne selon le langage (Ctrl+/). "" = pas de commentaire.
    inline const char* CommentPrefix(NkLang lang) {
        switch (lang) {
            case NkLang::C: case NkLang::NKSL: return "//";
            case NkLang::Python:               return "#";
            default:                            return "";   // None / Markdown
        }
    }

    // ── Autocomplétion : mots-clés par langage + filtrage par préfixe ────────────
    inline const char* const* NkKeywordsFor(NkLang lang, int32& count) {
        static const char* kCpp[] = { "alignas","alignof","auto","bool","break","case","catch","char","class","const","constexpr","continue","decltype","default","delete","do","double","else","enum","explicit","extern","false","float","for","friend","goto","if","inline","int","long","mutable","namespace","new","noexcept","nullptr","operator","private","protected","public","return","short","signed","sizeof","static","static_cast","struct","switch","template","this","throw","true","try","typedef","typename","union","unsigned","using","virtual","void","volatile","while","override","final","uint8","uint16","uint32","uint64","int8","int16","int32","int64","usize","float32","float64", nullptr };
        static const char* kPy[]  = { "and","as","assert","async","await","break","class","continue","def","del","elif","else","except","False","finally","for","from","global","if","import","in","is","lambda","None","nonlocal","not","or","pass","raise","return","True","try","while","with","yield","self","range","len","print","str","int","float","list","dict","set","tuple", nullptr };
        static const char* kNone[] = { nullptr };
        const char* const* p = (lang == NkLang::C) ? kCpp : (lang == NkLang::Python) ? kPy : kNone;
        count = 0; for (const char* const* q = p; *q; ++q) ++count;
        return p;
    }
    inline bool NkStartsWithI(const char* s, const char* pre) {
        for (; *pre; ++s, ++pre) { char a = *s, b = *pre; if (a >= 'A' && a <= 'Z') a += 32; if (b >= 'A' && b <= 'Z') b += 32; if (a != b) return false; }
        return true;
    }
    // Candidats commençant par `prefix` (symboles fichier + projet + mots-clés), dédup, cap 40.
    inline void NkBuildCompletions(const char* prefix, NkLang lang,
            const NkVector<NkString>* fileT, const NkVector<NkString>* fileF,
            const NkVector<NkString>* projT, const NkVector<NkString>* projF,
            NkVector<NkString>& out) {
        out.Clear();
        if (!prefix || !*prefix) return;
        auto add = [&](const char* s) {
            if (!NkStartsWithI(s, prefix)) return;
            if (NkCodeDoc::SymCmp(s, prefix) == 0) return;                                  // pas le mot exact
            for (usize i = 0; i < out.Size(); ++i) if (NkCodeDoc::SymCmp(out[i].CStr(), s) == 0) return;   // dédup
            if (out.Size() < 40) out.PushBack(NkString(s));
        };
        const NkVector<NkString>* lists[] = { fileT, fileF, projT, projF };
        for (int32 li = 0; li < 4; ++li) if (lists[li]) for (usize i = 0; i < lists[li]->Size(); ++i) add((*lists[li])[i].CStr());
        int32 kn = 0; const char* const* kw = NkKeywordsFor(lang, kn);
        for (int32 i = 0; i < kn; ++i) add(kw[i]);
    }

    // Dessine + pilote un editeur de code sur `d` dans `area`. Retourne true si le
    // document a ete modifie cette frame.
    inline bool CodeEditor(NkGuiContext& ctx, const char* idStr, NkCodeDoc& d, const NkRect& area,
                           NkLang lang = NkLang::None,
                           const NkVector<NkString>* projTypes = nullptr,
                           const NkVector<NkString>* projFuncs = nullptr,
                           const NkVector<NkString>* projDefines = nullptr) {
        using namespace detail;
        NkCodeFontScope _cfs(ctx);   // tout l'editeur dessine avec la police monospace (code)
        if (!ctx.font || !ctx.font->Face()) return false;
        d.EnsureNonEmpty();

        // Palette THEME-AWARE (suit le thème NKCode via ctx.theme : Dark Pro/Dark/Midnight/Light).
        const NkColor kBg       = ctx.theme.bgPrimary;
        const NkColor kGutterBg = ctx.theme.bgPrimary;
        const NkColor kLineNo   = ctx.theme.textDisabled;
        const NkColor kLineNoCur= ctx.theme.text;
        const NkColor kText     = ctx.theme.text;
        const NkColor kSel      = { ctx.theme.accent.r, ctx.theme.accent.g, ctx.theme.accent.b, 100 };   // sélection semi-transparente
        const NkColor kCurLine  = { ctx.theme.textDisabled.r, ctx.theme.textDisabled.g, ctx.theme.textDisabled.b, 24 };
        const NkColor kCaret    = ctx.theme.text;
        const NkColor kScrollTk = ctx.theme.button;
        const NkColor kBorder   = ctx.theme.accent;

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

        // Gouttiere : [numeros right-alignes | colonne breakpoints | bande Git 3px].
        char numbuf[16];
        std::snprintf(numbuf, sizeof(numbuf), "%d", d.LineCount());
        const float32 numAreaW = ctx.font->MeasureWidth(numbuf) + pad * 2.f;
        const float32 bpW      = lineH;                 // colonne breakpoints (carree)
        const float32 gitW     = 3.f;                   // bande Git au bord droit de la gouttiere
        const float32 gutterW  = numAreaW + bpW + gitW;
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
        // ── Ctrl+survol : détecte un lien navigable (chemin d'#include OU identifiant) sous
        //    la souris -> souligné + Ctrl+clic = navigation (item traité par NkCodeState). ──
        int32 linkL = -1, linkC0 = -1, linkC1 = -1; bool linkInc = false;
        if (ctx.input.ctrlDown && overText) {
            int32 l = static_cast<int32>((mouse.y - textTop + d.scrollY) / lineH);
            if (l >= 0 && l < d.LineCount()) {
                const int32 c = ColAtX(ctx, d, l, mouse.x - textLeft + d.scrollX);
                const NkCodeLine& L = d.lines[l]; const char* dd = L.Data(); const int32 n = static_cast<int32>(L.Size());
                int32 t = 0; while (t < n && (dd[t]==' '||dd[t]=='\t')) ++t;
                const bool isInc = (t < n && dd[t]=='#') && [&]{ int32 k=t+1; while (k<n&&(dd[k]==' '||dd[k]=='\t'))++k; const char* w="include"; int32 m=0; while (w[m] && k+m<n && dd[k+m]==w[m]) ++m; return w[m]=='\0'; }();
                if (isInc) {   // span = intérieur des guillemets ou chevrons
                    int32 s = t; while (s < n && dd[s] != '"' && dd[s] != '<') ++s;
                    if (s < n) { const char close = (dd[s]=='<') ? '>' : '"'; int32 e = s+1; while (e < n && dd[e] != close) ++e; if (c >= s && c <= e) { linkL=l; linkC0=s+1; linkC1=e; linkInc=true; } }
                } else if (c >= 0 && c < n && NkCodeDoc::IsWChar(dd[c])) {   // identifiant sous la souris
                    int32 s = c; while (s > 0 && NkCodeDoc::IsWChar(dd[s-1])) --s;
                    int32 e = c; while (e < n && NkCodeDoc::IsWChar(dd[e])) ++e;
                    if (e > s && !(dd[s] >= '0' && dd[s] <= '9')) { linkL=l; linkC0=s; linkC1=e; linkInc=false; }
                }
            }
        }
        const bool ctrlLink = (linkL >= 0);
        if (ctrlLink && ctx.input.mouseClicked[0] && overText) {   // Ctrl+clic : arme la navigation, ne bouge pas le caret
            const NkCodeLine& L = d.lines[linkL]; NkString tgt; for (int32 k = linkC0; k < linkC1; ++k) tgt += L[k];
            d.linkTarget = tgt; d.linkIsInclude = linkInc; NkCodeFocusId() = id;
        }
        if (!ctrlLink && ctx.input.mouseClicked[0] && overText) {
            NkCodeFocusId() = id;
            ctx.activeId = dragId;
            int32 l = static_cast<int32>((mouse.y - textTop + d.scrollY) / lineH);
            if (l < 0) l = 0; if (l >= d.LineCount()) l = d.LineCount() - 1;
            const int32 c = ColAtX(ctx, d, l, mouse.x - textLeft + d.scrollX);
            d.curLine = l; d.curCol = c;
            d.lineSelAnchor = -1;             // clic -> réinitialise la sélection-ligne Ctrl+L
            if (!ctx.input.shiftDown) d.Collapse();
        }
        // Glisser : etend la selection.
        if (ctx.activeId == dragId && ctx.input.mouseDown[0]) {
            int32 l = static_cast<int32>((mouse.y - textTop + d.scrollY) / lineH);
            if (l < 0) l = 0; if (l >= d.LineCount()) l = d.LineCount() - 1;
            d.curLine = l; d.curCol = ColAtX(ctx, d, l, mouse.x - textLeft + d.scrollX);
        }
        // Clic DROIT dans la zone texte : focus + menu contextuel Copier/Couper/Coller.
        if (ctx.input.mouseClicked[1] && overText) {   // clic DROIT (convention [1]=droit)
            NkCodeFocusId() = id; NkCodeCtxMenu().open = true; NkCodeCtxMenu().pos = mouse;
        }
        // Clic hors zone texte mais hors editeur : perd le focus.
        if (ctx.input.mouseClicked[0] && !hover && focused) NkCodeFocusId() = NKGUI_ID_NONE;

        bool changed = false;
        if (focused) {
            const bool shift = ctx.input.shiftDown;
            const bool ctrl  = ctx.input.ctrlDown, alt = ctx.input.altDown;
            // ── Barre de recherche/remplacement (Ctrl+F / Ctrl+H) ── ouverture + capture d'input.
            if (ctrl && !alt) {
                if (ctx.input.KeyPressed(NkGuiKey::F)) {
                    d.findReplace = false; d.findFocus = 0;
                    if (!d.findOpen && d.HasSel()) { int32 aL, aC, bL, bC; d.SelRange(aL, aC, bL, bC); if (aL == bL) { NkString s; const NkCodeLine& L = d.lines[aL]; for (int32 c = aC; c < bC; ++c) s += L[c]; d.findQuery = s; } }
                    d.findOpen = true; d.FindRecompute();
                }
                if (ctx.input.KeyPressed(NkGuiKey::H)) { d.findOpen = true; d.findReplace = true; d.findFocus = 0; d.FindRecompute(); }
            }
            if (d.findOpen) {   // la barre capte TOUTE la saisie tant qu'elle est ouverte
                NkString& fld = (d.findReplace && d.findFocus == 1) ? d.findRepl : d.findQuery;
                for (int32 i = 0; i < ctx.input.charCount; ++i) { const uint32 cp = ctx.input.chars[i]; if (cp >= 32 && cp < 127) fld += static_cast<char>(cp); }
                if (ctx.input.KeyPressedRepeat(NkGuiKey::Backspace)) { if (!fld.Empty()) fld.PopBack(); }
                if (ctx.input.KeyPressed(NkGuiKey::Escape)) d.FindClose();
                if (d.findReplace && ctx.input.KeyPressedRepeat(NkGuiKey::Tab)) d.findFocus ^= 1;
                if (ctx.input.KeyPressed(NkGuiKey::Enter)) { d.FindEnsure(); if (d.findReplace && d.findFocus == 1) d.ReplaceCurrent(); else d.FindNext(!shift); changed = true; }
                d.FindEnsure();
            }
            // ── Autocomplétion ouverte : capte ↑↓ (navigue), Tab/Entrée (accepte), Échap
            //    (ferme) AVANT l'édition normale. `acEat` = touche consommée cette frame. ──
            bool acEat = false, acTyped = false;
            if (!d.findOpen) {   // la saisie du DOCUMENT est gelée tant que la barre est ouverte
            if (d.acOpen && !d.acItems.Empty()) {
                const int32 acN = static_cast<int32>(d.acItems.Size());
                if (ctx.input.KeyPressedRepeat(NkGuiKey::Up))   { d.acSel = (d.acSel - 1 + acN) % acN; acEat = true; }
                if (ctx.input.KeyPressedRepeat(NkGuiKey::Down)) { d.acSel = (d.acSel + 1) % acN; acEat = true; }
                if (ctx.input.KeyPressed(NkGuiKey::Escape))     { d.acOpen = false; acEat = true; }
                if (!ctrl && !alt && (ctx.input.KeyPressed(NkGuiKey::Enter) || ctx.input.KeyPressedRepeat(NkGuiKey::Tab))) { d.AcceptAutocomplete(); acEat = true; changed = true; }
            }
            // Saisie texte.
            if (!ctx.input.ctrlDown) {
                bool typed = false;
                for (int32 i = 0; i < ctx.input.charCount; ++i) {
                    const uint32 cp = ctx.input.chars[i];
                    if (cp >= 32 && cp < 127) { if (!typed) { d.Checkpoint(1); typed = true; } d.InsertChar(static_cast<char>(cp)); changed = true; acTyped = true; }
                }
            }
            // Tab arrive par ÉVÉNEMENT TOUCHE (0x09 est filtré au niveau WM_CHAR) :
            // Tab sur sélection = indente ; Maj+Tab = désindente ; sinon 4 espaces.
            if (!acEat && !ctx.input.ctrlDown && !ctx.input.altDown && ctx.input.KeyPressedRepeat(NkGuiKey::Tab)) {
                d.Checkpoint(3);
                if (ctx.input.shiftDown)      d.IndentSelection(true);
                else if (d.HasSel())          d.IndentSelection(false);
                else                          { for (int k = 0; k < 4; ++k) d.InsertChar(' '); }
                changed = true;
            }
            // Touches d'edition (avec repetition au maintien).
            auto K = [&](NkGuiKey k) { return ctx.input.KeyPressedRepeat(k); };
            if (!acEat && !ctrl && K(NkGuiKey::Enter)) { d.Checkpoint(3); d.InsertNewline(); changed = true; }
            if (K(NkGuiKey::Backspace)) { d.Checkpoint(2); d.Backspace();     changed = true; acTyped = true; }
            if (K(NkGuiKey::Delete))    { d.Checkpoint(2); d.DeleteFwd();     changed = true; }
            if (K(NkGuiKey::Left)) {
                d.ResetEditRun();
                if (d.curCol > 0) --d.curCol;
                else if (d.curLine > 0) { --d.curLine; d.curCol = d.LineLen(d.curLine); }
                if (!shift) d.Collapse();
            }
            if (K(NkGuiKey::Right)) {
                d.ResetEditRun();
                if (d.curCol < d.LineLen(d.curLine)) ++d.curCol;
                else if (d.curLine < d.LineCount() - 1) { ++d.curLine; d.curCol = 0; }
                if (!shift) d.Collapse();
            }
            if (!acEat && !alt && K(NkGuiKey::Up))   { d.ResetEditRun(); if (d.curLine > 0) { --d.curLine; d.ClampCursor(); } if (!shift) d.Collapse(); }
            if (!acEat && !alt && K(NkGuiKey::Down)) { d.ResetEditRun(); if (d.curLine < d.LineCount() - 1) { ++d.curLine; d.ClampCursor(); } if (!shift) d.Collapse(); }
            if (K(NkGuiKey::Home)) { d.ResetEditRun(); if (ctrl) d.curLine = 0;                       d.curCol = 0;                 if (!shift) d.Collapse(); }   // Ctrl+Home = début du fichier
            if (K(NkGuiKey::End))  { d.ResetEditRun(); if (ctrl) d.curLine = d.LineCount() - 1;       d.curCol = d.LineLen(d.curLine); if (!shift) d.Collapse(); }   // Ctrl+End = fin du fichier
            // ── Raccourcis d'édition (Phase 4) ── (ctrl/alt définis en haut du bloc)
            if (alt && !ctrl) {
                if (K(NkGuiKey::Up))   { d.Checkpoint(3); if (shift) d.DuplicateLines(true);  else d.MoveLines(true);  changed = true; }   // Alt+↑ / Maj+Alt+↑
                if (K(NkGuiKey::Down)) { d.Checkpoint(3); if (shift) d.DuplicateLines(false); else d.MoveLines(false); changed = true; }   // Alt+↓ / Maj+Alt+↓
            }
            if (ctrl && !alt) {
                if (K(NkGuiKey::Z)) { if (shift) d.Redo(); else d.Undo(); changed = true; }      // Ctrl+Z undo / Ctrl+Maj+Z redo
                if (K(NkGuiKey::Y)) { d.Redo(); changed = true; }                                // Ctrl+Y redo
                if (shift && K(NkGuiKey::K)) { d.Checkpoint(3); d.DeleteLines(); changed = true; }   // Ctrl+Maj+K = supprimer ligne
                if (!shift && K(NkGuiKey::L)) { d.SelectCurrentLine(); }                         // Ctrl+L = sélectionner ligne (étend au répété)
                if (K(NkGuiKey::Enter)) { d.Checkpoint(3); if (shift) d.InsertLineAbove(); else d.InsertLineBelow(); changed = true; }  // Ctrl+Entrée / Ctrl+Maj+Entrée
                if (K(NkGuiKey::Slash)) {   // Ctrl+/ = commenter la ligne ; Ctrl+Maj+/ = commentaire BLOC
                    d.Checkpoint(3);
                    if (shift) {
                        if (lang == NkLang::C || lang == NkLang::NKSL) d.BlockComment("/* ", " */");   // toggle bloc intelligent
                        else d.ToggleComment(CommentPrefix(lang));   // Python etc. : pas de bloc natif -> lignes (toggle sûr)
                    } else d.ToggleComment(CommentPrefix(lang));
                    changed = true;
                }
                if (K(NkGuiKey::RBracket)) { d.Checkpoint(3); d.IndentSelection(false); changed = true; }   // Ctrl+] = indenter
                if (K(NkGuiKey::LBracket)) { d.Checkpoint(3); d.IndentSelection(true);  changed = true; }   // Ctrl+[ = désindenter
            }
            // Copier / couper / coller / tout-selectionner (presse-papiers).
            if (ctx.input.wantSelectAll) d.SelectAll();
            if ((ctx.input.wantCopy || ctx.input.wantCut) && d.HasSel()) {
                ctx.SetClipboard(d.GetSelectedText().CStr());
                if (ctx.input.wantCut) { d.Checkpoint(3); d.EraseSelection(); changed = true; }
            }
            if (ctx.input.wantPaste) {
                const NkString clip = ctx.GetClipboard();
                if (!clip.Empty()) { d.Checkpoint(3); d.InsertText(clip.CStr()); changed = true; }
            }
            // ── Recalcule l'autocomplétion après une frappe/effacement réel (identifiant) ──
            if (acTyped && lang != NkLang::None && d.curLine < d.LineCount()) {
                const NkCodeLine& L = d.lines[d.curLine];
                int32 s = d.curCol; while (s > 0 && NkCodeDoc::IsWChar(L[s - 1])) --s;
                const bool startsId = s < static_cast<int32>(L.Size()) && (((L[s] | 32) >= 'a' && (L[s] | 32) <= 'z') || L[s] == '_');
                const int32 wl = d.curCol - s;
                if (wl >= 1 && startsId) {
                    char pre[128]; int32 pn = 0; for (int32 k = s; k < d.curCol && pn < 127; ++k) pre[pn++] = L[k]; pre[pn] = 0;
                    d.EnsureSymbols(lang);   // symboles du fichier à jour
                    NkBuildCompletions(pre, lang, &d.symTypes, &d.symFuncs, projTypes, projFuncs, d.acItems);
                    d.acOpen = !d.acItems.Empty(); d.acSel = 0; d.acWordCol = s;
                } else d.acOpen = false;
            }
            }   // ferme if (!d.findOpen) : saisie document gelée quand la barre de recherche est ouverte
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
        const bool ensureCaret = (d.curLine != oldL || d.curCol != oldC || changed || d.wantReveal);
        if (ensureCaret) {
            const float32 cX = PrefixW(ctx, d, d.curLine, d.curCol);
            const float32 cY = topPad + d.curLine * lineH;
            if (d.wantReveal) { d.scrollY = cY - viewH * 0.4f; if (d.scrollY < 0.f) d.scrollY = 0.f; d.wantReveal = false; }   // clic Outline -> centre
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
        d.EnsureSymbols(lang);   // index sémantique (types/fonctions du fichier) — rebuild paresseux
        d.EnsurePreproc(lang, projDefines);   // régions préproc inactives (grisées) — après EnsureSymbols (symSig à jour)
        int32 inBlock = 0;   // état de bloc multi-lignes (0 aucun, 1 /*..*/ ou ```, 2 py """, 3 py ''')
        if (lang != NkLang::None)
            for (int32 i = 0; i < firstVis && i < d.LineCount(); ++i)
                inBlock = TokenizeLine(lang, d.lines[i].Data(), static_cast<int32>(d.lines[i].Size()),
                                       inBlock, syn, [](int32, int32, const NkColor&) {});

        // ── Guides d'indentation + bracket matching (surcouches subtiles) ──
        const float32 chW = ctx.font->MeasureWidth("0");
        const int32   tabSize = 4;
        const bool    edLight = ((int32)ctx.theme.bgPrimary.r + ctx.theme.bgPrimary.g + ctx.theme.bgPrimary.b) > 384;
        const int32   bgR = ctx.theme.bgPrimary.r, bgG = ctx.theme.bgPrimary.g, bgB = ctx.theme.bgPrimary.b;   // pour atténuer les branches préproc inactives
        const NkColor kGuide   = edLight ? NkColor{ 0, 0, 0, 28 } : NkColor{ 255, 255, 255, 24 };
        const NkColor kBracket = { ctx.theme.accent.r, ctx.theme.accent.g, ctx.theme.accent.b, 70 };
        // Paire d'accolades/parenthèses correspondante quand le curseur en est adjacent.
        int32 bl1 = -1, bc1 = -1, bl2 = -1, bc2 = -1;
        if (focused) {
            auto chAt = [&](int32 l, int32 c) -> char { if (l < 0 || l >= d.LineCount()) return 0; const NkCodeLine& L = d.lines[l]; return (c >= 0 && c < (int32)L.Size()) ? L[c] : 0; };
            auto isOpen = [](char c) { return c == '(' || c == '[' || c == '{'; };
            auto isClose = [](char c) { return c == ')' || c == ']' || c == '}'; };
            int32 pc = -1; char bc = 0; const char lc = chAt(d.curLine, d.curCol - 1), rc = chAt(d.curLine, d.curCol);
            if (isOpen(lc) || isClose(lc)) { pc = d.curCol - 1; bc = lc; }
            else if (isOpen(rc) || isClose(rc)) { pc = d.curCol; bc = rc; }
            if (pc >= 0) {
                bl1 = d.curLine; bc1 = pc;
                if (isOpen(bc)) { int32 l = d.curLine, c = pc + 1, depth = 1, g = 0; while (l < d.LineCount() && g++ < 300000) { const int32 sz = (int32)d.lines[l].Size(); if (c >= sz) { ++l; c = 0; continue; } const char cc = d.lines[l][c]; if (isOpen(cc)) ++depth; else if (isClose(cc)) { if (--depth == 0) { bl2 = l; bc2 = c; break; } } ++c; } }
                else { int32 l = d.curLine, c = pc - 1, depth = 1, g = 0; while (l >= 0 && g++ < 300000) { if (c < 0) { if (--l < 0) break; c = (int32)d.lines[l].Size() - 1; continue; } const char cc = (c < (int32)d.lines[l].Size()) ? d.lines[l][c] : 0; if (isClose(cc)) ++depth; else if (isOpen(cc)) { if (--depth == 0) { bl2 = l; bc2 = c; break; } } --c; } }
            }
        }

        dl.PushClipRect(textArea, true);
        for (int32 i = firstVis; i <= lastVis && i < d.LineCount(); ++i) {
            if (i < 0) continue;
            const float32 y        = textTop + i * lineH - d.scrollY;
            const float32 baseline = y + asc;
            const NkCodeLine& ln   = d.lines[i];
            const int32 n          = static_cast<int32>(ln.Size());

            // Guides d'indentation : une ligne verticale par niveau d'indentation.
            { int32 ind = 0; const char* dd = ln.Data(); for (int32 k = 0; k < n; ++k) { if (dd[k] == ' ') ++ind; else if (dd[k] == '\t') ind += tabSize; else break; }
              for (int32 col = 0; col < ind; col += tabSize) { const float32 gx = textLeft + col * chW - d.scrollX; if (gx >= textArea.x - 1.f && gx < textArea.x + textArea.w) dl.AddLine({ gx, y }, { gx, y + lineH }, kGuide, 1.f); } }
            // Surlignage bracket matching (les deux extrémités).
            if (i == bl1 && bc1 >= 0) dl.AddRectFilled({ textLeft + PrefixW(ctx, d, i, bc1) - d.scrollX, y, chW, lineH }, kBracket, 2.f);
            if (i == bl2 && bc2 >= 0) dl.AddRectFilled({ textLeft + PrefixW(ctx, d, i, bc2) - d.scrollX, y, chW, lineH }, kBracket, 2.f);

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
            const bool dim = d.InactiveAt(i);   // branche préproc morte -> texte atténué vers le fond
            inBlock = TokenizeLine(lang, data, n, inBlock, syn,
                [&](int32 a, int32 b, const NkColor& col) {
                    NkColor c = col;
                    if (dim) { c.r = static_cast<uint8>((col.r*42 + bgR*58)/100); c.g = static_cast<uint8>((col.g*42 + bgG*58)/100); c.b = static_cast<uint8>((col.b*42 + bgB*58)/100); }
                    sx = NkDrawTextU(ctx, sx, baseline, y, lineH, data + a, data + b, c);   // box-drawing en primitives
                }, &d.symTypes, &d.symFuncs, projTypes, projFuncs);   // coloration sémantique (fichier + projet)
        }
        // ── Diagnostics : soulignement ondulé (rouge=erreur / jaune=warning) + message
        //    inline en fin de ligne (style « Error Lens » de VS Code). ──
        {
            auto isWd = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; };
            for (usize di = 0; di < d.diags.Size(); ++di) {
                const NkCodeDoc::Diag& dg = d.diags[di];
                if (dg.line < firstVis || dg.line > lastVis || dg.line < 0 || dg.line >= d.LineCount()) continue;
                const NkColor col = dg.sev ? NkColor{ 240, 80, 80, 255 } : NkColor{ 224, 190, 70, 255 };
                const float32 yy = textTop + dg.line * lineH - d.scrollY;
                const NkCodeLine& ln = d.lines[dg.line];
                int32 c0 = dg.col; if (c0 < 0) c0 = 0; if (c0 > (int32)ln.Size()) c0 = (int32)ln.Size();
                int32 c1 = c0; while (c1 < (int32)ln.Size() && isWd(ln[c1])) ++c1; if (c1 <= c0) c1 = (c0 < (int32)ln.Size()) ? c0 + 1 : c0;
                float32 x0 = textLeft + PrefixW(ctx, d, dg.line, c0) - d.scrollX;
                float32 x1 = textLeft + PrefixW(ctx, d, dg.line, c1) - d.scrollX;
                if (x1 < x0 + chW) x1 = x0 + chW;
                const float32 wy = yy + lineH - 2.5f;
                for (float32 x = x0; x < x1; x += 4.f) { dl.AddLine({ x, wy }, { x + 2.f, wy - 2.f }, col, 1.1f); dl.AddLine({ x + 2.f, wy - 2.f }, { x + 4.f, wy }, col, 1.1f); }
                // message inline (aprÃ¨s la fin du texte de la ligne), attÃ©nuÃ©.
                if (ctx.font && ctx.font->Valid()) {
                    const float32 endX = textLeft + PrefixW(ctx, d, dg.line, (int32)ln.Size()) - d.scrollX + chW * 2.f;
                    dl.AddText(ctx.font->Face(), ctx.font->TexId(), { endX, yy + asc }, dg.msg.CStr(), NkColor{ col.r, col.g, col.b, 165 });
                }
            }
        }
        // Ctrl+survol : souligne le lien navigable (style VSCode) sous la souris.
        if (ctrlLink && linkL >= firstVis && linkL <= lastVis) {
            const float32 ux = textLeft + PrefixW(ctx, d, linkL, linkC0) - d.scrollX;
            const float32 ux2 = textLeft + PrefixW(ctx, d, linkL, linkC1) - d.scrollX;
            const float32 uy = textTop + linkL * lineH - d.scrollY + lineH - 2.f;
            dl.AddLine({ ux, uy }, { ux2, uy }, ctx.theme.accent, 1.2f);
        }
        // Caret.
        if (focused) {
            const float32 cx = textLeft + PrefixW(ctx, d, d.curLine, d.curCol) - d.scrollX;
            const float32 cy = textTop + d.curLine * lineH - d.scrollY;
            dl.AddLine({ cx, cy + 1.f }, { cx, cy + lineH - 1.f }, kCaret, 1.5f);
        }
        dl.PopClipRect();

        // ── Popup d'autocomplétion (façon VSCode) : liste sous le mot, pilotée au clavier
        //    (↑↓ naviguer · Tab/Entrée accepter · Échap fermer). Dessiné hors clip. ──
        if (focused && d.acOpen && !d.acItems.Empty() && ctx.font && ctx.font->Valid()) {
            const int32 n = static_cast<int32>(d.acItems.Size());
            const int32 shown = n < 8 ? n : 8;
            int32 top = 0; if (d.acSel >= shown) top = d.acSel - shown + 1; if (top > n - shown) top = n - shown; if (top < 0) top = 0;
            float32 pw = 140.f;
            for (int32 i = 0; i < n; ++i) { const float32 w = ctx.font->MeasureWidth(d.acItems[static_cast<usize>(i)].CStr()) + 26.f; if (w > pw) pw = w; }
            if (pw > 380.f) pw = 380.f;
            const float32 rowH = lineH;
            const float32 px = textLeft + PrefixW(ctx, d, d.curLine, d.acWordCol) - d.scrollX;
            float32 py = textTop + (d.curLine + 1) * lineH - d.scrollY;
            const float32 ph = shown * rowH + 4.f;
            if (py + ph > area.y + area.h) py = textTop + d.curLine * lineH - d.scrollY - ph;   // au-dessus si ça déborde
            const NkRect box = { px, py, pw, ph };
            dl.AddRectFilled(box, ctx.theme.header, 5.f); dl.AddRect(box, ctx.theme.accent, 1.f);
            const NkColor selC = { ctx.theme.accent.r, ctx.theme.accent.g, ctx.theme.accent.b, 80 };
            for (int32 vi = 0; vi < shown; ++vi) {
                const int32 i = top + vi;
                const NkRect row = { box.x + 2.f, box.y + 2.f + vi * rowH, pw - 4.f, rowH };
                if (i == d.acSel) dl.AddRectFilled(row, selC, 3.f);
                dl.AddText(ctx.font->Face(), ctx.font->TexId(), { row.x + 8.f, row.y + asc }, d.acItems[static_cast<usize>(i)].CStr(), ctx.theme.text);
            }
        }

        // ── Gouttiere : numeros + colonne breakpoints (clic = toggle, survol = creux) ──
        const float32 bpX  = area.x + numAreaW;
        const float32 bpCx = bpX + bpW * 0.5f;
        const NkColor kBpOn = { 229, 74, 68, 255 };         // point d'arret actif (#E54A44)
        const bool overBp = ctx.popupDepth == 0 && mouse.x >= bpX && mouse.x < bpX + bpW
                          && mouse.y >= textArea.y && mouse.y < textArea.y + viewH;
        const int32 bpHoverLine = overBp ? static_cast<int32>((mouse.y - textTop + d.scrollY) / lineH) : -1;
        if (overBp && ctx.input.mouseClicked[0] && bpHoverLine >= 0 && bpHoverLine < d.LineCount()) {
            d.ToggleBreakpoint(bpHoverLine); NkCodeFocusId() = id;
        }
        dl.PushClipRect({ area.x, area.y, gutterW, area.h }, true);
        for (int32 i = firstVis; i <= lastVis && i < d.LineCount(); ++i) {
            if (i < 0) continue;
            const float32 baseline = textTop + i * lineH - d.scrollY + asc;
            const float32 cy = textTop + i * lineH - d.scrollY + lineH * 0.5f;
            const float32 yTop = textTop + i * lineH - d.scrollY;
            // Bande Git au bord droit de la gouttière : vert=ajout, bleu=modif, triangle rouge=suppression.
            const uint8 gs = d.GitAt(i);
            if (gs) dl.AddRectFilled({ area.x + gutterW - gitW, yTop, gitW, lineH }, gs == 1 ? NkColor{ 63, 185, 80, 255 } : NkColor{ 84, 174, 255, 255 });
            if (d.GitDelAt(i)) dl.AddTriangleFilled({ area.x + gutterW - gitW - 4.f, yTop - 3.f }, { area.x + gutterW, yTop - 3.f }, { area.x + gutterW - gitW - 4.f, yTop + 3.f }, NkColor{ 248, 81, 73, 255 });
            // Point d'arret : plein si pose, creux (alpha) au survol de sa ligne.
            if (d.HasBreakpoint(i))      dl.AddCircleFilled({ bpCx, cy }, 4.5f, kBpOn);
            else if (i == bpHoverLine)   dl.AddCircleFilled({ bpCx, cy }, 4.5f, NkColor{ kBpOn.r, kBpOn.g, kBpOn.b, 90 });
            char nb[16]; std::snprintf(nb, sizeof(nb), "%d", i + 1);
            const float32 nw = ctx.font->MeasureWidth(nb);
            // Marqueur d'erreur/avertissement (façon VSCode) : numéro coloré + pastille à gauche.
            const int32 sev = d.DiagSevOn(i);
            const NkColor nColor = (sev == 1) ? NkColor{ 240, 80, 80, 255 } : (sev == 0) ? NkColor{ 224, 190, 70, 255 }
                                 : (i == d.curLine) ? kLineNoCur : kLineNo;
            if (sev >= 0) dl.AddCircleFilled({ area.x + 4.f, cy }, 3.f, nColor);
            dl.AddText(ctx.font->Face(), ctx.font->TexId(),
                       { area.x + numAreaW - pad - nw, baseline }, nb, nColor);
        }
        dl.PopClipRect();

        // ── Barres de defilement : gouttieres TOUJOURS visibles (theme-aware) ──
        const bool    sbLight = ((int32)ctx.theme.bgPrimary.r + ctx.theme.bgPrimary.g + ctx.theme.bgPrimary.b) > 384;
        const NkColor kTrack  = sbLight ? NkColor{ 0, 0, 0, 20 } : NkColor{ 255, 255, 255, 16 };
        const NkColor kThumb  = sbLight ? NkColor{ 168, 176, 185, 255 } : NkColor{ 80, 88, 98, 255 };
        const NkColor kThumbH = sbLight ? NkColor{ 130, 138, 148, 255 } : NkColor{ 120, 130, 142, 255 };
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

        // ── Barre de RECHERCHE / REMPLACEMENT (Ctrl+F / Ctrl+H) : flottante en haut-droite ──
        if (d.findOpen && ctx.font && ctx.font->Face()) {
            const float32 fh = ctx.font->LineHeight(), fasc = ctx.font->Ascent();
            const float32 rowH = fh + ctx.S(10.f), gap = ctx.S(4.f), pad = ctx.S(6.f);
            const float32 barW = ctx.S(360.f);
            const float32 barH = rowH * (d.findReplace ? 2.f : 1.f) + ctx.S(8.f);
            const float32 bx = area.x + area.w - barW - ctx.S(18.f), by = area.y + ctx.S(6.f);
            const NkColor panel = { 40, 46, 54, 255 }, fieldBg = { 22, 27, 34, 255 }, brd = { 60, 66, 74, 255 };
            dl.AddRectFilled({ bx, by, barW, barH }, panel, ctx.S(6.f));
            dl.AddRect({ bx, by, barW, barH }, ctx.theme.accent, 1.f);
            const NkVec2 mp = ctx.input.mousePos; const bool clk = ctx.input.mouseClicked[0];
            auto hit = [&](const NkRect& r) { return mp.x >= r.x && mp.x < r.x + r.w && mp.y >= r.y && mp.y < r.y + r.h; };
            auto btn = [&](float32 x, float32 y, float32 w, const char* lbl, bool on) -> bool {
                const NkRect r = { x, y, w, rowH - gap };
                if (on) dl.AddRectFilled(r, ctx.theme.accent, ctx.S(4.f));
                else if (hit(r)) dl.AddRectFilled(r, ctx.theme.buttonHover, ctx.S(4.f));
                const float32 tw = ctx.font->MeasureWidth(lbl);
                dl.AddText(ctx.font->Face(), ctx.font->TexId(), { r.x + (w - tw) * 0.5f, r.y + (r.h - fh) * 0.5f + fasc }, lbl, ctx.theme.text);
                return hit(r) && clk;
            };
            auto field = [&](float32 x, float32 y, float32 w, const char* txt, const char* ph, bool foc, int32 idx) {
                const NkRect r = { x, y, w, rowH - gap };
                dl.AddRectFilled(r, fieldBg, ctx.S(4.f)); dl.AddRect(r, foc ? ctx.theme.accent : brd, 1.f);
                const bool empty = (txt[0] == '\0');
                dl.AddText(ctx.font->Face(), ctx.font->TexId(), { r.x + ctx.S(6.f), r.y + (r.h - fh) * 0.5f + fasc }, empty ? ph : txt, empty ? ctx.theme.textDisabled : ctx.theme.text);
                if (foc && !empty) { const float32 cxx = r.x + ctx.S(6.f) + ctx.font->MeasureWidth(txt); dl.AddRectFilled({ cxx, r.y + ctx.S(3.f), 1.f, r.h - ctx.S(6.f) }, ctx.theme.text); }
                if (hit(r) && clk) d.findFocus = idx;
            };
            const float32 btnW = ctx.S(28.f), countW = ctx.S(52.f);
            float32 rx = bx + pad, ry = by + ctx.S(4.f);
            const float32 queryW = barW - pad * 2.f - countW - btnW * 4.f - gap * 4.f;
            field(rx, ry, queryW, d.findQuery.CStr(), "Rechercher", d.findFocus == 0, 0);
            float32 cx2 = rx + queryW + gap;
            char cnt[32];
            if (d.FindCount() == 0) { cnt[0] = d.findQuery.Empty() ? '\0' : '0'; cnt[1] = '\0'; }
            else std::snprintf(cnt, sizeof(cnt), "%d/%d", d.findCur < 0 ? 0 : d.findCur + 1, d.FindCount());
            dl.AddText(ctx.font->Face(), ctx.font->TexId(), { cx2, ry + (rowH - gap - fh) * 0.5f + fasc }, cnt, ctx.theme.textDisabled);
            cx2 += countW;
            if (btn(cx2, ry, btnW, "Aa", d.findCase)) { d.findCase = !d.findCase; d.FindRecompute(); }  cx2 += btnW + gap;
            if (btn(cx2, ry, btnW, "<", false)) { d.FindEnsure(); d.FindNext(false); changed = true; }  cx2 += btnW + gap;
            if (btn(cx2, ry, btnW, ">", false)) { d.FindEnsure(); d.FindNext(true);  changed = true; }  cx2 += btnW + gap;
            if (btn(cx2, ry, btnW, "x", false)) { d.FindClose(); }
            if (d.findReplace) {
                ry += rowH;
                const float32 rBtnW = ctx.S(54.f);
                const float32 replW = barW - pad * 2.f - rBtnW * 2.f - gap * 2.f;
                field(rx, ry, replW, d.findRepl.CStr(), "Remplacer", d.findFocus == 1, 1);
                float32 rcx = rx + replW + gap;
                if (btn(rcx, ry, rBtnW, "Rempl.", false)) { d.ReplaceCurrent(); changed = true; }  rcx += rBtnW + gap;
                if (btn(rcx, ry, rBtnW, "Tout", false))   { d.ReplaceAll();     changed = true; }
            }
        }

        if (changed) d.dirty = true;
        return changed;
    }

} // namespace nkcode
} // namespace nkentseu
