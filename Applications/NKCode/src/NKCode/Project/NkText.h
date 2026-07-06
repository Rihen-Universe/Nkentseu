#pragma once
// =============================================================================
// NkText.h — Petits utilitaires texte/JSON/chemin (préfixe Nk, sans allocation
// superflue) partagés par NKCode. Convention Rihen : PascalCase + préfixe Nk.
// =============================================================================
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
namespace nkcode {

    // Entier décimal en tête de `p` (saute les espaces initiaux). 0 si aucun chiffre.
    inline int32 NkAtoi(const char* p) {
        while (*p == ' ' || *p == '\t') ++p;
        int32 v = 0; while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); ++p; }
        return v;
    }

    // Première occurrence de `needle` dans `hay` (sous-chaîne). nullptr sinon.
    inline const char* NkFindSub(const char* hay, const char* needle) {
        if (!needle || !*needle) return hay;
        for (; *hay; ++hay) { const char* a = hay; const char* b = needle; while (*a && *b && *a == *b) { ++a; ++b; } if (!*b) return hay; }
        return nullptr;
    }

    // Vrai si `self` (chemin) est un SUFFIXE de p[0,len) — comparaison insensible aux
    // séparateurs (/ vs \\) et à la casse (Windows). Sert à faire correspondre le
    // chemin renvoyé par un compilateur (absolu/relatif) au fichier ouvert.
    inline bool NkPathSuffixMatch(const char* p, int32 len, const char* self) {
        int32 sl = 0; while (self[sl]) ++sl;
        int32 a = len - 1, b = sl - 1;
        while (a >= 0 && b >= 0) {
            char ca = p[a], cb = self[b];
            if (ca == '\\') ca = '/'; if (cb == '\\') cb = '/';
            if (ca >= 'A' && ca <= 'Z') ca += 32; if (cb >= 'A' && cb <= 'Z') cb += 32;
            if (ca != cb) return false; --a; --b;
        }
        return b < 0;   // `self` entièrement retrouvé en suffixe
    }

    // Vrai si `dir` est un ANCÊTRE de `file` (ou égal) — insensible / \\ + casse Windows.
    // Sert à trouver le projet d'un fichier (dossier projet préfixe du chemin fichier).
    inline bool NkPathIsAncestor(const char* dir, const char* file) {
        int32 dl = 0; while (dir[dl]) ++dl; int32 fl = 0; while (file[fl]) ++fl;
        // ignore un séparateur final sur `dir`
        while (dl > 0 && (dir[dl - 1] == '/' || dir[dl - 1] == '\\')) --dl;
        if (dl == 0 || dl > fl) return false;
        for (int32 k = 0; k < dl; ++k) {
            char a = dir[k], b = file[k];
            if (a == '\\') a = '/'; if (b == '\\') b = '/';
            if (a >= 'A' && a <= 'Z') a += 32; if (b >= 'A' && b <= 'Z') b += 32;
            if (a != b) return false;
        }
        return fl == dl || file[dl] == '/' || file[dl] == '\\';   // frontière de dossier
    }

    // JSON minimal : valeur STRING de la 1re occurrence de "key":"…". Gère \n et \\.
    inline NkString NkJsonStr(const char* j, const char* key) {
        const NkString pat = NkString("\"") + key + "\"";
        const char* k = NkFindSub(j, pat.CStr()); if (!k) return NkString();
        const char* p = k + pat.Size(); while (*p && *p != ':') ++p; if (*p) ++p; while (*p == ' ') ++p;
        if (*p != '\"') return NkString(); ++p;
        NkString o; while (*p && *p != '\"') { if (*p == '\\' && p[1]) { ++p; o += (*p == 'n' ? '\n' : *p); } else o += *p; ++p; }
        return o;
    }

    // JSON minimal : tableau de STRINGS "key": [ "a", "b" ] -> append dans `out`.
    inline void NkJsonArray(const char* j, const char* key, NkVector<NkString>& out) {
        const NkString pat = NkString("\"") + key + "\"";
        const char* k = NkFindSub(j, pat.CStr()); if (!k) return;
        const char* p = k + pat.Size(); while (*p && *p != '[') { if (*p == '}') return; ++p; } if (*p != '[') return; ++p;
        while (*p && *p != ']') {
            while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r' || *p == '\t') ++p;
            if (*p == '\"') { ++p; NkString s; while (*p && *p != '\"') { if (*p == '\\' && p[1]) { ++p; s += *p; } else s += *p; ++p; } if (*p) ++p; out.PushBack(s); }
            else if (*p != ']') ++p;
        }
    }

    // ── Préprocesseur : évaluation d'expression #if (sous-ensemble C) ─────────────
    // Valeur d'un define `name[0,nameLen)` dans `defs` (entrées "NOM" ou "NOM=val").
    // *found=false si absent ; sinon valeur numérique (défini sans valeur => 1).
    inline int64 NkDefineValue(const NkVector<NkString>& defs, const char* name, int32 nameLen, bool* found) {
        for (usize i = 0; i < defs.Size(); ++i) {
            const char* d = defs[i].CStr();
            int32 k = 0; while (k < nameLen && d[k] && d[k] != '=' && d[k] == name[k]) ++k;
            if (k == nameLen && (d[k] == '\0' || d[k] == '=')) {
                *found = true;
                if (d[k] == '=') { const char* v = d + k + 1; while (*v == ' ') ++v; return (int64)NkAtoi(v); }
                return 1;
            }
        }
        *found = false; return 0;
    }

    // Descente récursive : ||  <  &&  <  ! / primaire (defined(X), entier, (), identifiant).
    struct NkPPCur { const char* p; const NkVector<NkString>* defs; bool ok; };
    inline int64 NkPPParseOr(NkPPCur& c);
    inline bool  NkPPIsIdc(char ch) { return (ch>='A'&&ch<='Z')||(ch>='a'&&ch<='z')||(ch>='0'&&ch<='9')||ch=='_'; }
    inline int64 NkPPParsePrimary(NkPPCur& c) {
        while (*c.p == ' ' || *c.p == '\t') ++c.p;
        if (*c.p == '(') { ++c.p; int64 v = NkPPParseOr(c); while (*c.p==' '||*c.p=='\t') ++c.p; if (*c.p == ')') ++c.p; return v; }
        if (*c.p == '!') { ++c.p; return NkPPParsePrimary(c) ? 0 : 1; }
        if (NkFindSub(c.p, "defined") == c.p && !NkPPIsIdc(c.p[7])) {
            const char* q = c.p + 7; while (*q==' '||*q=='\t') ++q; const bool par = (*q=='('); if (par) ++q; while (*q==' '||*q=='\t') ++q;
            const char* s = q; int32 L = 0; while (NkPPIsIdc(q[L])) ++L; q += L; while (*q==' '||*q=='\t') ++q; if (par && *q==')') ++q;
            c.p = q; bool f = false; NkDefineValue(*c.defs, s, L, &f); return f ? 1 : 0;
        }
        if (*c.p >= '0' && *c.p <= '9') { int64 v = 0; while (*c.p>='0'&&*c.p<='9') { v = v*10 + (*c.p-'0'); ++c.p; } while (*c.p=='L'||*c.p=='l'||*c.p=='U'||*c.p=='u') ++c.p; return v; }
        if ((*c.p>='A'&&*c.p<='Z')||(*c.p>='a'&&*c.p<='z')||*c.p=='_') { const char* s=c.p; int32 L=0; while (NkPPIsIdc(c.p[L])) ++L; c.p += L; bool f=false; int64 v=NkDefineValue(*c.defs,s,L,&f); return f? v : 0; }
        c.ok = false; return 1;   // inconnu -> actif par prudence
    }
    inline int64 NkPPParseAnd(NkPPCur& c) { int64 v = NkPPParsePrimary(c); for (;;) { while (*c.p==' '||*c.p=='\t') ++c.p; if (c.p[0]=='&'&&c.p[1]=='&') { c.p+=2; int64 r=NkPPParsePrimary(c); v = (v && r)?1:0; } else break; } return v; }
    inline int64 NkPPParseOr(NkPPCur& c)  { int64 v = NkPPParseAnd(c);     for (;;) { while (*c.p==' '||*c.p=='\t') ++c.p; if (c.p[0]=='|'&&c.p[1]=='|') { c.p+=2; int64 r=NkPPParseAnd(c);     v = (v || r)?1:0; } else break; } return v; }
    // Vrai si l'expression #if est ACTIVE. Constructions non gérées (==, <, arith)
    // => renvoie true (prudence : on ne grise pas une branche qu'on ne comprend pas).
    inline bool NkPPEvalActive(const char* expr, const NkVector<NkString>& defs) {
        NkPPCur c{ expr, &defs, true }; const int64 v = NkPPParseOr(c);
        while (*c.p==' '||*c.p=='\t') ++c.p;
        if (*c.p != '\0') c.ok = false;
        return c.ok ? (v != 0) : true;
    }

} // namespace nkcode
} // namespace nkentseu
