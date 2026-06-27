#pragma once
// =============================================================================
// NkSyntax.h — Coloration syntaxique (style Visual Studio / VSCode Dark+).
//   Tokenise UNE ligne et emet des plages colorees [begin,end) via un callback :
//   mots-cles, types, chaines, commentaires (// et /*..*/ multi-lignes), nombres,
//   preprocesseur. Langages : C/C++ et Python (sinon texte brut).
//   Sans allocation : gap-filling (les zones non colorees -> couleur texte).
// =============================================================================
#include "NKGui/NKGui.h"

namespace nkcode {

    using namespace nkentseu;
    using namespace nkentseu::nkgui;

    enum class NkLang { None, C, Python, NKSL, Markdown };

    struct NkSynColors {
        NkColor text    = { 212, 212, 212, 255 };   // #d4d4d4
        NkColor keyword = {  86, 156, 214, 255 };   // #569cd6
        NkColor type    = {  78, 201, 176, 255 };   // #4ec9b0
        NkColor string  = { 206, 145, 120, 255 };   // #ce9178
        NkColor comment = { 106, 153,  85, 255 };   // #6a9955
        NkColor number  = { 181, 206, 168, 255 };   // #b5cea8
        NkColor preproc = { 197, 134, 192, 255 };   // #c586c0
        NkColor heading = {  78, 201, 176, 255 };   // titres Markdown (#4ec9b0)
        NkColor mdcode  = { 206, 145, 120, 255 };   // code Markdown (#ce9178)
    };

    namespace synd {
        inline bool IsAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
        inline bool IsDigit(char c) { return c >= '0' && c <= '9'; }
        inline bool IsWord(char c)  { return IsAlpha(c) || IsDigit(c); }
        inline bool WordEq(const char* s, int32 n, const char* kw) {
            int32 i = 0; for (; i < n && kw[i]; ++i) if (s[i] != kw[i]) return false;
            return i == n && kw[i] == '\0';
        }
        inline bool InList(const char* s, int32 n, const char* const* list) {
            for (int32 i = 0; list[i]; ++i) if (WordEq(s, n, list[i])) return true;
            return false;
        }
        inline const char* const* CKeywords() {
            static const char* k[] = {
                "if","else","for","while","do","switch","case","default","break","continue","return",
                "goto","sizeof","typedef","struct","class","union","enum","namespace","using","template",
                "typename","public","private","protected","virtual","override","final","const","constexpr",
                "static","inline","extern","volatile","mutable","new","delete","this","nullptr","true","false",
                "operator","friend","explicit","auto","noexcept","try","catch","throw","static_cast",
                "reinterpret_cast","const_cast","dynamic_cast","decltype","include","define","pragma", nullptr };
            return k;
        }
        inline const char* const* CTypes() {
            static const char* t[] = {
                "void","bool","char","short","int","long","float","double","unsigned","signed","wchar_t",
                "size_t","usize","int8","int16","int32","int64","uint8","uint16","uint32","uint64",
                "float32","float64","NkString","NkVector",
                // Types shader (NKSL/GLSL-like) :
                "vec2","vec3","vec4","ivec2","ivec3","ivec4","uvec2","uvec3","uvec4","bvec2","bvec3","bvec4",
                "mat2","mat3","mat4","float2","float3","float4","int2","int3","int4","half","half2","half3","half4",
                "sampler2D","sampler3D","samplerCube","texture2D","Texture2D","SamplerState", nullptr };
            return t;
        }
        inline const char* const* NkslKeywords() {
            static const char* k[] = {
                "if","else","for","while","do","switch","case","default","break","continue","return",
                "struct","const","static","inline","true","false","void","in","out","inout","uniform",
                "attribute","varying","layout","buffer","flat","smooth","discard","precision","highp",
                "mediump","lowp","cbuffer","register","numthreads","groupshared","stage","shader","vertex",
                "fragment","compute","include","define","pragma","version", nullptr };
            return k;
        }
        inline const char* const* PyKeywords() {
            static const char* k[] = {
                "def","class","if","elif","else","for","while","return","import","from","as","with","try",
                "except","finally","pass","break","continue","lambda","None","True","False","and","or","not",
                "in","is","global","nonlocal","yield","await","async","raise","del","assert","with", nullptr };
            return k;
        }
    } // namespace synd

    inline NkLang NkLangFromExt(const char* ext) {
        using namespace synd;
        auto eq = [&](const char* a) { int32 n = 0; while (ext[n]) ++n; return WordEq(ext, n, a); };
        if (eq(".cpp") || eq(".cc") || eq(".cxx") || eq(".h") || eq(".hpp") || eq(".hxx") ||
            eq(".c") || eq(".inl") || eq(".ino")) return NkLang::C;
        if (eq(".py") || eq(".jenga") || eq(".pyi")) return NkLang::Python;   // .jenga = Python
        if (eq(".nksl") || eq(".glsl") || eq(".hlsl") || eq(".vert") || eq(".frag") || eq(".comp") ||
            eq(".shader")) return NkLang::NKSL;
        if (eq(".md") || eq(".markdown")) return NkLang::Markdown;
        return NkLang::None;
    }

    // Tokenise [L, L+n). `inBlock` = on est dans un /*..*/ ouvert. emit(a,b,color)
    // pour CHAQUE plage (les trous sont comblés en couleur texte). Retourne le nouvel
    // etat de bloc-commentaire. Emit signature : void(int32, int32, const NkColor&).
    template <class Emit>
    inline bool TokenizeLine(NkLang lang, const char* L, int32 n, bool inBlock,
                             const NkSynColors& C, Emit emit) {
        using namespace synd;
        if (lang == NkLang::None) { if (n > 0) emit(0, n, C.text); return false; }

        // ── Markdown (titres, blocs ``` , code inline `...`) ─────────────────
        if (lang == NkLang::Markdown) {
            int32 last = 0;
            auto flush = [&](int32 upTo) { if (upTo > last) emit(last, upTo, C.text); };
            int32 s = 0; while (s < n && (L[s] == ' ' || L[s] == '\t')) ++s;
            const bool fence = (s + 2 < n && L[s] == '`' && L[s + 1] == '`' && L[s + 2] == '`');
            if (inBlock) {                                  // dans un bloc de code ```
                if (fence) { if (n > 0) emit(0, n, C.comment); return false; }
                if (n > 0) emit(0, n, C.mdcode); return true;
            }
            if (fence) { emit(0, n, C.comment); return true; }            // ouvre un bloc
            if (s < n && L[s] == '#') { emit(0, n, C.heading); return false; }   // titre #
            for (int32 i = 0; i < n;) {                                   // code inline `...`
                if (L[i] == '`') {
                    int32 j = i + 1; while (j < n && L[j] != '`') ++j; j = (j < n) ? j + 1 : n;
                    flush(i); emit(i, j, C.mdcode); last = j; i = j; continue;
                }
                ++i;
            }
            flush(n);
            return false;
        }

        int32 last = 0;                                  // fin de la derniere plage emise
        auto flush = [&](int32 upTo) { if (upTo > last) emit(last, upTo, C.text); };
        auto span  = [&](int32 a, int32 b, const NkColor& col) { flush(a); emit(a, b, col); last = b; };

        int32 i = 0;
        if (inBlock) {                                   // fin d'un /*..*/ entame avant
            int32 k = i;
            while (k + 1 < n && !(L[k] == '*' && L[k + 1] == '/')) ++k;
            if (k + 1 < n) { span(0, k + 2, C.comment); i = k + 2; inBlock = false; }
            else           { span(0, n, C.comment); return true; }
        }
        const bool isC = (lang == NkLang::C || lang == NkLang::NKSL);   // NKSL = famille C
        for (; i < n;) {
            const char c = L[i];
            if (isC && c == '/' && i + 1 < n && L[i + 1] == '/') { span(i, n, C.comment); i = n; break; }
            if (isC && c == '/' && i + 1 < n && L[i + 1] == '*') {
                int32 k = i + 2;
                while (k + 1 < n && !(L[k] == '*' && L[k + 1] == '/')) ++k;
                if (k + 1 < n) { span(i, k + 2, C.comment); i = k + 2; }
                else           { span(i, n, C.comment); return true; }   // bloc ouvert -> lignes suivantes
                continue;
            }
            if (!isC && c == '#') { span(i, n, C.comment); i = n; break; }       // Python : commentaire
            if (isC && c == '#') {                                               // C : preprocesseur
                bool firstTok = true; for (int32 j = 0; j < i; ++j) if (L[j] != ' ' && L[j] != '\t') { firstTok = false; break; }
                if (firstTok) { span(i, n, C.preproc); i = n; break; }
            }
            if (c == '"' || c == '\'') {                                         // chaine / caractere
                int32 j = i + 1; while (j < n && L[j] != c) { if (L[j] == '\\' && j + 1 < n) ++j; ++j; }
                j = (j < n) ? j + 1 : n; span(i, j, C.string); i = j; continue;
            }
            if (IsDigit(c)) {                                                     // nombre
                int32 j = i + 1; while (j < n && (IsWord(L[j]) || L[j] == '.')) ++j;
                span(i, j, C.number); i = j; continue;
            }
            if (IsAlpha(c)) {                                                     // identifiant / mot-cle / type
                int32 j = i + 1; while (j < n && IsWord(L[j])) ++j;
                const int32 wn = j - i;
                const char* const* kws = (lang == NkLang::C) ? CKeywords()
                                       : (lang == NkLang::NKSL) ? NkslKeywords() : PyKeywords();
                if (InList(L + i, wn, kws))                 span(i, j, C.keyword);
                else if (isC && InList(L + i, wn, CTypes())) span(i, j, C.type);
                i = j; continue;
            }
            ++i;                                                                 // ponctuation/operateur -> texte (comble)
        }
        flush(n);
        return inBlock;
    }

} // namespace nkcode
